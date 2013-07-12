/*#include "spinn_api.h"*/
#include "spin1_api.h"
#include "spinn_io.h"
#include "spinn_sdp.h" // Required by comms.h

#include "comms.h"
#include "config.h"
#include "dma.h"
#include "model_general.h"
#include "model_lif_nef_multidimensional.h"
#include "recording.h"

// Neuron data structures
uint num_populations;
population_t *population;
psp_buffer_t *psp_buffer;

short x_value[DIMENSIONS];

void handle_sdp_msg(sdp_msg_t * sdp_msg)
{
    int *ptr = (int*) sdp_msg-> data;
    switch(sdp_msg->cmd_rc)
    {    
        case 0x101: // SDP input spikes packet            
            
            for (int d = 0; d < DIMENSIONS; d++)    x_value[d] = (short) ptr[d];
//            io_printf(IO_STD, "%d - value %d, %d\n", spin1_get_simulation_time(), x_value[0], x_value[1]);        
            break;
        default:
            io_printf(IO_STD, "received unrecognized SDP command %x\n");
            break;

    }

}


void buffer_post_synaptic_potentials(void *dma_copy, uint row_size)
{
    uint time = spin1_get_simulation_time();
    
    synaptic_row_t *synaptic_row = (synaptic_row_t *) dma_copy;
    
    for(uint i = 0; i < row_size; i++)
    {
        uint type = synaptic_row->synapses[i] & 0x7;
        uint weight = synaptic_row->synapses[i] & 0xfff8;
        uint index = synaptic_row->synapses[i] >> 16 & 0x7ff;
        //SD commenting out the next instruction because stdp is not used here
        //uint stdp = synaptic_row->synapses[0] >> 27 & 0x1;
        uint arrival = 1 + time + (synaptic_row->synapses[i] >> 28 & 0xf);
        
/*        io_printf(IO_STD, "Type %d, weight %d, index %d, stdp %d, arrival %d\n", type, weight, index, stdp, arrival);*/
        
        switch(type)
        {
            case 0: psp_buffer[index].exci[arrival % PSP_BUFFER_SIZE] += weight; break;
            case 1: psp_buffer[index].inhi[arrival % PSP_BUFFER_SIZE] += weight; break;
            default: break;
        }
    }
}


void timer_callback(uint ticks, uint null)
{
/*    io_printf(IO_STD, "Time: %d - X value = %d\n", ticks, x_value);*/
/*    if(ticks % 128 == 0)  io_printf(IO_STD, "Time: %d\n", ticks);*/


/*    if(ticks == 1 && app_data.virtual_core_id == 1 && spin1_get_chip_id() == 0)*/
/*    {*/
/*        io_printf(IO_STD, "Sending sync spike.\n");*/
/*        send_sync_sdp();*/
/*    }*/

    if(ticks == 1)  io_printf(IO_STD, "lif_nef d: %d.\n", DIMENSIONS);

    if(ticks >= app_data.run_time)
    {
        io_printf(IO_STD, "Simulation complete.\n");
        spin1_stop();
    }

/*    if(ticks == 1)  io_printf(IO_STD, "x_value: %d\n", x_value);*/
    if(ticks == 1)  for (int d = 0; d < DIMENSIONS; d++)    x_value[d] = 0;

/*    if(ticks % 2000 == 0)*/
/*    {*/
/*        x_value[0] += 64;*/
/*        x_value[1] -= 64;        */
/*        io_printf(IO_STD, "x_value: %d, %d\n", x_value[0], x_value[1]);*/
/*    }*/

    


// Input integrator, 2 pulses
    
/*    if(ticks == 200)  */
/*    {*/
/*        x_value = 192;*/
/*        io_printf(IO_STD, "%d - x_value: %d\n", ticks, x_value);*/
/*    }*/
/*    else if(ticks == 400)  */
/*    {*/
/*        x_value = 0;*/
/*        io_printf(IO_STD, "%d - x_value: %d\n", ticks, x_value);*/
/*    }*/
/*    else if(ticks == 1500)  */
/*    {*/
/*        x_value = -192;*/
/*        io_printf(IO_STD, "%d - x_value: %d\n", ticks, x_value);*/
/*    }*/
/*    else if(ticks == 1700)  */
/*    {*/
/*        x_value = 0;*/
/*        io_printf(IO_STD, "%d - x_value: %d\n", ticks, x_value);*/
/*    }*/
    
    
        

// oscillator input, kick
    
/*    else if(ticks == 2000 && spin1_get_core_id() == 3)  */
/*    {*/
/*        x_value = 256;*/
/*        io_printf(IO_STD, "%d - x_value: %d\n", ticks, x_value);*/
/*    }*/

/*    else if(ticks == 2100 && spin1_get_core_id() == 3)  */
/*    {*/
/*        x_value = 0;*/
/*        io_printf(IO_STD, "%d - x_value: %d\n", ticks, x_value);*/
/*    }*/

// oscillator input, halving frequency

/*    else if(ticks == 10 && spin1_get_core_id() == 1)  */
/*    {*/
/*        x_value = 256;*/
/*        io_printf(IO_STD, "%d - x_value: %d\n", ticks, x_value);*/
/*    }*/


/*    else if(ticks == 6000 && spin1_get_core_id() == 1)  */
/*    {*/
/*        x_value = 128;*/
/*        io_printf(IO_STD, "%d - x_value: %d\n", ticks, x_value);*/
/*    }*/



    

    for(uint i = 0; i < num_populations; i++)       // i = population
    {
        neuron_t *neuron = (neuron_t *) population[i].neuron;
/*        psp_buffer_t *psp_buffer = population[i].psp_buffer;*/
        
        for(uint j = 0; j < population[i].num_neurons; j++)
        {
/*            if(ticks == 1)  io_printf(IO_STD, "neuron %d - params: %d %d %d %d %d\n", j, neuron[j].v, neuron[j].encoder, neuron[j].bias_current, neuron[j].value_current, neuron[j].refrac_clock);*/

/*            if(ticks == 1)  io_printf(IO_STD, "neuron %d - encoders: %d %d bias: %d %d %d\n", j, neuron[j].encoder[0], neuron[j].encoder[1], neuron[j].bias_current, neuron[j].value_current, neuron[j].refrac_clock);*/

            int current = 0;
            if (neuron[j].refrac_clock==0) // if we're not in refractory period
            {
                
                // Calculates current
/*                current = neuron[j].bias_current + (int)(((long long)(neuron[j].encoder*(long long)x_value << 8)) >> 16);*/
                current = neuron[j].bias_current;
                for (int d = 0; d < DIMENSIONS; d++)    current += (int) ( (long long)(((long long)(neuron[j].encoder[d]) * (long long)(x_value[d])) >> 8));
                
                // Calculates membrane potential            
                neuron[j].v += (int)(((long long)(nef_v_reset - neuron[j].v + current) * ((long long)nef_decay))>>16);
/*                if(j ==3)  io_printf(IO_STD, "v[%d]=%d\n",j,neuron[j].v);                                                */
/*                if(j ==3)   io_printf(IO_STD, "i[%d]=%d\n",j,current);                                                */
            }
            else
                neuron[j].refrac_clock--;            

            // Transmit spikes and reset state
            if(neuron[j].v >= nef_v_thresh)
            {
                uint key = spin1_get_chip_id() << 16 | app_data.virtual_core_id << 11 | population[i].id | j;


/*                io_printf(IO_STD, "Time: %d spike! %08x\n", ticks, key);                            */

                spin1_send_mc_packet(key, NULL, NO_PAYLOAD); 


                neuron[j].v = nef_v_reset + neuron[j].v - nef_v_thresh;
/*                neuron[j].v = nef_v_reset;*/
                neuron[j].refrac_clock = nef_tau_refrac;                
                
                if(population[i].flags & OUTPUT_SPIKE_BIT) output_spike(key);
            }

            // Optionally record voltage and current trace FROM A SINGLE POPULATION ONLY
            if(population[i].flags & RECORD_STATE_BIT)
            {
                record_v[population[i].num_neurons * ticks + j] = (short) neuron[j].v >> 16;
/*                record_i[population[i].num_neurons * ticks + j] = (short) current;*/
            }
            
        }
    }
    
//    flush_output_spikes();
}

