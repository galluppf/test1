#include "spin1_api.h"
#include "spinn_io.h"
#include "spinn_sdp.h" // Required by comms.h

#include "comms.h"
#include "config.h"
#include "dma.h"
#include "model_general.h"

#ifdef STDP
#include "stdp.h"
#endif

#ifdef STDP_SP
#include "stdp_sp.h"
#endif

#ifdef STDP_TTS
#include "stdp_tts.h"
#endif

//#define DEBUG

dma_pipeline_t dma_pipeline;
synapse_lookup_t *synapse_lookup;

int overflows = 0;


void buffer_mc_packet(uint key, uint payload)
{
    if((mc_packet_buffer.end + 1) % MC_PACKET_BUFFER_SIZE != mc_packet_buffer.start)
    {
        mc_packet_buffer.buffer[mc_packet_buffer.end] = key;
        mc_packet_buffer.end = (mc_packet_buffer.end + 1) % MC_PACKET_BUFFER_SIZE;

        if(!dma_pipeline.busy)
        {
            dma_pipeline.busy = TRUE;
            spin1_trigger_user_event(0, 0); //TODO should this be spin1_schedule_callback?
        }
    }
    else
    {
        overflows++;
    }
}

void dma_callback(uint null0, uint tag)
{

#ifdef DEBUG
    io_printf (IO_STD, "DMA callback with tag %08x\n", tag);
#endif

    if (tag == NULL) //fetch of synaptic row from SDRAM
    {
        synaptic_row_t *synaptic_row = dma_pipeline.cache[dma_pipeline.flip ^ 1];
        uint row_size = dma_pipeline.row_size[dma_pipeline.flip ^ 1]; //in bytes, including synaptic row header

#if defined STDP || defined STDP_SP || defined STDP_TTS
        synapse_lookup_t *synapse_lookup_address = dma_pipeline.synapse_lookup_address[dma_pipeline.flip ^ 1];

#ifdef DEBUG
        io_printf (IO_STD, "stdp on address: %08x, row size: %d, number of synaptic words: %d \n", synaptic_row, row_size, ((row_size>>2)-3));

        io_printf (IO_STD, "from address: %08x, source neuron: %08x, pre_coarse: %08x, pre_fine: %08x\n", (uint) synaptic_row, synaptic_row->neuron_id, synaptic_row->coarse_stdp_time_constant, synaptic_row->fine_stdp_time_constant);
#endif

        Stdp(synaptic_row, ((row_size>>2)-3), spin1_get_simulation_time());

#ifdef DEBUG
        io_printf (IO_STD, "from address: %08x, source neuron: %08x, pre_coarse: %08x, pre_fine: %08x\n", (uint) synaptic_row, synaptic_row->neuron_id, synaptic_row->coarse_stdp_time_constant, synaptic_row->fine_stdp_time_constant);

        io_printf (IO_STD, "Copying back the synaptic weight row to the SDRAM.\n");
#endif

        Stdp_Writeback(synaptic_row, synapse_lookup_address, row_size);
#endif

        buffer_post_synaptic_potentials(synaptic_row, ((row_size>>2)-3));

        uint cpsr = spin1_int_disable();
        uint feed = mc_packet_buffer.start != mc_packet_buffer.end ? 1 : 0;
        spin1_mode_restore(cpsr);

        if(feed) feed_dma_pipeline(NULL, NULL);
        else     dma_pipeline.busy = FALSE;

        return;
    }
    else if (tag == 0xFFFFFFFF) //writeback from STDP
    {

#ifdef DEBUG
        io_printf (IO_STD, "DTCM: %08x, source neuron: %08x, pre_coarse: %08x, pre_fine: %08x, synaptic word: %08x\n", (uint) dma_pipeline.cache[dma_pipeline.flip ^ 1], dma_pipeline.cache[dma_pipeline.flip ^ 1]->neuron_id, dma_pipeline.cache[dma_pipeline.flip ^ 1]->coarse_stdp_time_constant, dma_pipeline.cache[dma_pipeline.flip ^ 1]->fine_stdp_time_constant, dma_pipeline.cache[dma_pipeline.flip ^ 1]->synapses[0]);
        io_printf (IO_STD, "SDRAM: source neuron: %08x, pre_coarse: %08x, pre_fine: %08x, synaptic word: %08x\n", *(int*) 0x70000000, *(int*) 0x70000004, *(int*) 0x70000008, *(int*) 0x7000000C);
#endif

        return;

    }
    else if (tag == 0xFFFFFFFE) //second fetch for the Refetch Event Model
        return;
}


void feed_dma_pipeline(uint null0, uint null1)
{
    while(!mc_packet_buffer_empty())
    {
        uint key = mc_packet_buffer.buffer[mc_packet_buffer.start];
        mc_packet_buffer.start = (mc_packet_buffer.start + 1) % MC_PACKET_BUFFER_SIZE;

        lookup_synapses(key);

        void *source = (void *) dma_pipeline.synapse_lookup_address[dma_pipeline.flip];

        if(source != NULL)
        {
            void *destination = (void *) dma_pipeline.cache[dma_pipeline.flip];

#ifdef DEBUG
		    io_printf(IO_STD, "Sterting DMA request from address: %08x, to address: %08x, size: %d bytes\n", (int) source, (int) destination, dma_pipeline.row_size[dma_pipeline.flip]);
#endif

		    spin1_dma_transfer(NULL,
		                       source,
		                       destination,
		                       DMA_READ,
		                       dma_pipeline.row_size[dma_pipeline.flip]);

            dma_pipeline.flip ^= 1;

            return;
        }
    }

    dma_pipeline.busy = FALSE;
}


void lookup_synapses(uint key)
{
    //TODO This could be neater...
    for(uint i = 0; synapse_lookup[i].core_id != 0xffffffff; i++)
    {
        if((synapse_lookup[i].core_mask & key) == synapse_lookup[i].core_id)
        {
            uint synaptic_row_address = (uint) synapse_lookup[i].synaptic_block;
//            io_printf(IO_STD, "synaptic_row_address: %08x\n", synaptic_row_address);

            //the next row computer the address of the synaptic row start.
            //It computes a displacement from the base address as received neuron index - first neuron index in the population
            //and multiplies it by the synaptic row size (adding the synaptic row header length).
            //synaptic_row_address is the base address of the synaptic row extracted from the lookup table
            //(key & ~synapse_lookup[i].core_mask) if the neuron ID received by the routing key
            //(synapse_lookup[i].row_size * sizeof(unsigned int) + sizeof(synaptic_row_t)) size of the synaptic weight row in bytes
            //The two terms, combined, give the displacement from the base address for the synaptic row: [base_address+(a*b)]

            dma_pipeline.synapse_lookup_address[dma_pipeline.flip] = (synapse_lookup_t *) (synaptic_row_address + ((key & ~synapse_lookup[i].core_mask)) * (synapse_lookup[i].row_size * sizeof(unsigned int) + sizeof(synaptic_row_t)));
            dma_pipeline.row_size[dma_pipeline.flip] = (synapse_lookup[i].row_size + 3) * 4;

#ifdef DEBUG
            io_printf(IO_STD, "lookup - key: %08x, address: %08x, row size: %d \n", key, dma_pipeline.synapse_lookup_address[dma_pipeline.flip], dma_pipeline.row_size[dma_pipeline.flip]);
#endif

            return;
        }
    }

    io_printf(IO_STD, "Failed synapse lookup. MC packet %08x dropped\n", key);

    dma_pipeline.synapse_lookup_address[dma_pipeline.flip] = NULL;
    dma_pipeline.row_size[dma_pipeline.flip] = 0;

    return;
}


uint mc_packet_buffer_empty()
{
    uint cpsr = spin1_int_disable();
    uint empty = mc_packet_buffer.start == mc_packet_buffer.end ? TRUE : FALSE;
    spin1_mode_restore(cpsr);

    return empty;
}
