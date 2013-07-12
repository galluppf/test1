#include "spin1_api.h"
#include "spinn_io.h"
#include "spinn_sdp.h" // Required by comms.h

#include "comms.h"
#include "config.h"
#include "dma.h"
#include "model_general.h"
//#include "nef.h"
#include "model_nef_out.h"
#include "recording.h"



// Neuron data structures
uint num_populations;
population_t *population;
psp_buffer_t *psp_buffer;

void handle_sdp_msg(sdp_msg_t * sdp_msg)
{
// void function
}


void buffer_post_synaptic_potentials(void *dma_copy, uint row_size)
{    
    uint time = spin1_get_simulation_time();
    
    synaptic_row_t *synaptic_row = (synaptic_row_t *) dma_copy;
    

    for(uint i = 0; i < app_data.synaptic_row_length; i++)
    {
/*        uint type = synaptic_row->synapses[0] & 0x7;*/
/*        uint index = synaptic_row->synapses[0] >> 16 & 0x7ff;*/
/*        uint arrival = 1 + time + (synaptic_row->synapses[0] >> 28 & 0xf);*/
/*        io_printf(IO_STD, "Type %d, index %d\n", type, index);*/


/*        uint weight = synaptic_row->synapses[i] & 0xfffff;*/
        uint type = (synaptic_row->synapses[i] >> 20) & 0x1;
        uint index = (synaptic_row->synapses[i] >> 21) & 0x7ff;
        uint arrival = 1 + time;

        
        switch(type)
        {
            case 0: psp_buffer[index].exci[arrival % PSP_BUFFER_SIZE] = 1; break; // TODO OMG that's a hack! it will write 1 every time it received a packet
/*            case 0: psp_buffer[index].exci[arrival % PSP_BUFFER_SIZE] += weight; break;*/
            case 1: psp_buffer[index].inhi[arrival % PSP_BUFFER_SIZE] = 1; break;
            default: break;
        }
    }
}


void timer_callback(uint ticks, uint null)
{
/*    io_printf(IO_STD, "Time: %d\n", ticks);*/

    int out_value = 0;
/*    if(ticks % 128 == 0)  io_printf(IO_STD, "Time: %d\n", ticks);*/

/*    if(ticks == 1 && app_data.virtual_core_id == 1 && spin1_get_chip_id() == 0)*/
/*    {*/
/*        io_printf(IO_STD, "Sending sync spike.\n");*/
/*        send_sync_sdp();*/
/*    }*/

    
    if(ticks >= app_data.run_time)
    {
        io_printf(IO_STD, "Simulation complete");
        spin1_stop();
    }
    

    for(uint i = 0; i < num_populations; i++)
    {

        neuron_t *neuron = (neuron_t *) population[i].neuron;
        psp_buffer_t *psp_buffer = population[i].psp_buffer;
        
        out_value = 0;
        
        for(uint j = 0; j < population[i].num_neurons; j++)
        {
        
/*            if (ticks == 1)  io_printf(IO_STD, "decoder: %d\n", neuron[j].decoder);        */
            if(ticks <= 16)    psp_buffer[j].exci[ticks % PSP_BUFFER_SIZE] = 0;        // FIXME Superhack: why are buffer not clean on startup? Got -1s
                                    
            // Get excitatory and inhibitory currents from synaptic inputs                    
            short exci_current = psp_buffer[j].exci[ticks % PSP_BUFFER_SIZE];
            short spike_received=exci_current;

/*            if (spike_received>0)   io_printf(IO_STD, "n %d\n", j);*/

/*            out_value += (int)(spike_received)*neuron[j].decoder;            */
            out_value += spike_received*neuron[j].decoder;
            
            // Clear PSP buffers for this timestep
            psp_buffer[j].exci[ticks % PSP_BUFFER_SIZE] = 0;
            psp_buffer[j].inhi[ticks % PSP_BUFFER_SIZE] = 0;
        }

    }
/*    io_printf(IO_STD, "T:%d;x=%d\n", ticks, out_value >> 8);*/
    send_out_value(1, 0, 0, out_value >> 8);    
    
}

