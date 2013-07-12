#include "spin1_api.h"
#include "spinn_io.h"
#include "spinn_sdp.h" // Required by comms.h

#include "comms.h"
#include "config.h"
#include "dma.h"
#include "model_general.h"


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
            spin1_trigger_user_event(0, 0);
        }
    }
    else
    {
        overflows++;
    }
/*    {*/
/*        io_printf(IO_STD, "PB!\n");*/
/*        feed_dma_pipeline();*/
/*//        spin1_stop();*/
/*    }*/
}


void dma_callback(uint null0, uint tag)
{
   

        synaptic_row_t *synaptic_row = dma_pipeline.cache[dma_pipeline.flip ^ 1];
        uint row_size = dma_pipeline.row_size[dma_pipeline.flip ^ 1]; //in bytes, including synaptic row header

        buffer_post_synaptic_potentials(synaptic_row, ((row_size>>2)-3));

        uint cpsr = spin1_int_disable();
        uint feed = mc_packet_buffer.start != mc_packet_buffer.end ? 1 : 0;
        spin1_mode_restore(cpsr);

        if(feed) feed_dma_pipeline(NULL, NULL);
        else     dma_pipeline.busy = FALSE;

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

            //the next row computer the address of the synaptic row start.
            //It computes a displacement from the base address as received neuron index - first neuron index in the population
            //and multiplies it by the synaptic row size (adding the synaptic row header length).
            //synaptic_row_address is the base address of the synaptic row extracted from the lookup table
            //(key & ~synapse_lookup[i].core_mask) if the neuron ID received by the routing key
            //(synapse_lookup[i].core_id & 0x7FF) start neuron ID for the specific block
            //(synapse_lookup[i].row_size * sizeof(unsigned int) + sizeof(synaptic_row_t)) size of the synaptic weight row in bytes
            //The two terms, combined, give the displacement from the base address for the synaptic row: [base_address+(a*b)]

            dma_pipeline.synapse_lookup_address[dma_pipeline.flip] = (synapse_lookup_t *) (synaptic_row_address + ((key & ~synapse_lookup[i].core_mask)) * (synapse_lookup[i].row_size * sizeof(unsigned int) + sizeof(synaptic_row_t)));
            dma_pipeline.row_size[dma_pipeline.flip] = (synapse_lookup[i].row_size + 3) * 4;

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
