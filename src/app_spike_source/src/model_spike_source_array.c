/*#include "spinn_api.h"*/
#include "spin1_api.h"
#include "spinn_io.h"
#include "spinn_sdp.h" // Required by comms.h

#include "model_general.h"
#include "comms.h"
#include "config.h"
#include "dma.h"

uint num_populations = 0;
population_t *population = NULL;

extern uint sdp_packets_received;

unsigned int *SDRAMaddress;
unsigned int offsetForAMilliSec;
unsigned short int *PN_ID; //array of populations and neuron IDs (16 bits each)

void process_spikes(void *dma_copy)
{
    int i, j;

    unsigned int *TCM_spikes = (unsigned int*) dma_copy;

    //io_printf(IO_STD, "tick: %d, spikes: %08x\n", spin1_get_simulation_time(),*TCM_spikes);

    for (i = 0; i < offsetForAMilliSec; i++)
    {
        for (j = 0; j < 32; j++)
        {
            if (((*(TCM_spikes + i)) >> j) & 0x01)
            {
                //if there is a "1" then send a mc packet
                uint key = spin1_get_chip_id() << 16 |
                            app_data.virtual_core_id << 11 |
                            PN_ID[((i << 5) + j)]; //this is the population and neuron ID

                spin1_send_mc_packet(key, NULL, NO_PAYLOAD);
            }
        }
    }
}

void handle_sdp_msg(sdp_msg_t * sdp_msg) {}

void timer_callback(uint ticks, uint null)
{

    spin1_dma_transfer(0xFFFFFFFD,
                       (void*) SDRAMaddress,
                       dma_pipeline.cache,
                       DMA_READ,
                       (offsetForAMilliSec << 2));

    SDRAMaddress += offsetForAMilliSec;

    if(ticks == 1)
    {
        io_printf (IO_STD, "Spike source model\n");
    }

    if(ticks >= app_data.run_time)
    {
        io_printf(IO_STD, "Simulation complete - received %d sdp packets.\n", sdp_packets_received);
        spin1_stop();
    }
}

void configure_recording_space()
{
    int i, j, counter;
    unsigned int InputSpikesInSDRAM;

	InputSpikesInSDRAM = (0x72136400 + 0x200000 * (app_data.virtual_core_id - 1));
	SDRAMaddress =(unsigned int *)InputSpikesInSDRAM;

    io_printf(IO_STD, "Loading spikes from: %x\n", (0x72136400 + 0x200000 * (app_data.virtual_core_id - 1))); //Used for debug prints
//    io_printf(IO_STD, "DEBUG CHECK: %x\n", InputSpikesInSDRAM); //Used for debug prints
//    io_printf(IO_STD, "DEBUG CHECK2: %x\n", SDRAMaddress); //Used for debug prints
 

    offsetForAMilliSec = ((app_data.total_neurons-1) >> 5)+1; // Optimised by sergio
//    io_printf(IO_STD, "Total Number of Neurons %d, # of fetches from the SDRAM for a ms %d\n",app_data.total_neurons,(offsetForAMilliSec));	

    num_populations = app_data.num_populations;
    PN_ID = spin1_malloc(app_data.total_neurons);
    counter = 0;

    for (i = 0; i < num_populations; i++)
    {
        for (j = 0; j < population[i].num_neurons; j++)
        {
            PN_ID[counter++] = population[i].id | j;
        }
    }
}

