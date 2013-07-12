#include "spin1_api.h"
#include "spinn_io.h"
#include "spinn_sdp.h" // Required by comms.h

#include "comms.h"
#include "config.h"
#include "dma.h"
#include "model_general.h"
#include "model_lif_32.h"
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
/*    uint time = spin1_get_simulation_time();    */
    uint arrival = 1 + spin1_get_simulation_time();
    
    synaptic_row_t *synaptic_row = (synaptic_row_t *) dma_copy;
    
    for(uint i = 0; i < row_size; i++)
    {
/*        uint type = synaptic_row->synapses[i] & 0x7;    */
/*        uint weight = (synaptic_row->synapses[i] & 0xfff8) << 3;   // 3.10        (3.13)*/
/*        uint index = synaptic_row->synapses[i] >> 16 & 0x7ff;*/
/*        uint arrival = 1 + time + (synaptic_row->synapses[i] >> 28 & 0xf);                */
    
/*        uint type = synaptic_row->synapses[i] & 0x1;*/
/*        uint weight = (synaptic_row->synapses[i] & 0xfffe) << 3;       // 3.12     (3.13)*/
/*        uint index = synaptic_row->synapses[i] >> 16 & 0x7ff;*/
/*        uint arrival = 1 + time;*/


/*        uint type = synaptic_row->synapses[i] & 0x1;*/
/*        uint weight = (synaptic_row->synapses[i] & 0xfffe);       // 0.15     (0.16)*/
/*        uint index = synaptic_row->synapses[i] >> 16 & 0x7ff;*/
/*        uint arrival = 1 + time;*/


/*| destination neuron index 11 bit | synapse type 1 bit |weight 20 bit */
/*        uint weight = synaptic_row->synapses[i] & 0xfffff;*/
/*        uint type = (synaptic_row->synapses[i] >> 20) & 0x1;*/
/*        uint index = (synaptic_row->synapses[i] >> 21) & 0x7ff;*/
/*        uint arrival = 1 + time;*/


/*| synapse type 1 bit |weight 31 bit (destination neuron index implicit) 
        uint weight = synaptic_row->synapses[i] & 0x7FFFFFFF;
        uint type = (synaptic_row->synapses[i] >> 31) & 0x1; */

/*| destination neuron index 7 bit | synapse type 1 bit |weight 24 bit */
        uint weight = synaptic_row->synapses[i] & 0xFFFFFF;
        uint type = (synaptic_row->synapses[i] >> 24) & 0x1;
        uint index = (synaptic_row->synapses[i] >> 25) & 0x7f;

/*        io_printf(IO_STD, "T: %d t %d, w %d, i %d\n", spin1_get_simulation_time(), type, weight, index);*/
/*        spin1_delay_us(10);*/


        switch(type)
        {            
            case 0: 
                psp_buffer[index].exci[arrival % PSP_BUFFER_SIZE] += weight;                 
                break;
            case 1: 
                psp_buffer[index].inhi[arrival % PSP_BUFFER_SIZE] += weight; 
                break;
            default: break;
        }
    }
}


void timer_callback(uint ticks, uint null)
{
    
    if(ticks >= app_data.run_time)
    {
        io_printf(IO_STD, "Done.\n");
        spin1_stop();
    }

    for(uint i = 0; i < num_populations; i++)
    {
        neuron_t *neuron = (neuron_t *) population[i].neuron;
        psp_buffer_t *psp_buffer = population[i].psp_buffer;
     
            
        for(uint j = 0; j < population[i].num_neurons; j++)
        {
            // Get excitatory and inhibitory currents from synaptic inputs
            
/*            if(ticks == 1)  io_printf(IO_STD, "neuron %d - params: %d %d %d %d %d\n", j, neuron[j].v, neuron[j].bias_current, neuron[j].refrac_clock, neuron[j].v_thresh, neuron[j].v_rest);*/
            

        uint exci_current = psp_buffer[j].exci[ticks % PSP_BUFFER_SIZE];
        uint inhi_current = psp_buffer[j].inhi[ticks % PSP_BUFFER_SIZE];
        
        int current=0;

	    if (neuron[j].refrac_clock==0) // if we're not in refractory period
	    {	            
                current = ((int)exci_current) - ((int)inhi_current) + neuron[j].bias_current;
                neuron[j].v += (int)(((long long)(neuron[j].v_rest - neuron[j].v + current) * ((long long)neuron[j].decay))>>16);
        }
        else
	        neuron[j].refrac_clock--;


        // Transmit spikes and reset state
        if(neuron[j].v >= neuron[j].v_thresh)
        {

            uint key = spin1_get_chip_id() << 16 | app_data.virtual_core_id << 11 | population[i].id | j;
#ifdef DEBUG_SPIKES
            io_printf(IO_STD, "Time: %d spike! neuron %x\n", ticks, key);            
#endif            
            spin1_send_mc_packet(key, NULL, NO_PAYLOAD);
            
//            neuron[j].v = neuron[j].v_reset;                                      // normal reset
            neuron[j].v = neuron[j].v_reset + neuron[j].v - neuron[j].v_thresh;     // compensating for v overshoot

            neuron[j].refrac_clock = neuron[j].tau_refrac;
            
            // Optionally end spike to host
            if(population[i].flags & OUTPUT_SPIKE_BIT) output_spike(key);
        }
                        

            // v, current, reset are all <<8, decay is <<16
/*            neuron[j].v += current - (((neuron[j].v-neuron[j].v_rest) * neuron[j].decay)>>16);*/
            
            // Optionally record voltage and current trace FROM A SINGLE POPULATION ONLY
            if(population[i].flags & RECORD_STATE_BIT)
            {
/*                io_printf(IO_STD, "v: %d\n", neuron[i].v / 256);*/
/*                io_printf(IO_STD, "v: %d\n", neuron[i].v / 256);*/
                record_v[population[i].num_neurons * ticks + j] = (short) (neuron[j].v >> 8) & 0xFFFF;
                record_i[population[i].num_neurons * ticks + j] = (short) (current >> 8) & 0xFFFF;;
            }
                        
            // Clear PSP buffers for this timestep
            psp_buffer[j].exci[ticks % PSP_BUFFER_SIZE] = 0;
            psp_buffer[j].inhi[ticks % PSP_BUFFER_SIZE] = 0;
            
            
            // Compute PSP dynamics (NB exci_decay is scaled up by 2^16 for precision)
            long long exci_decay = ((long long)exci_current * (long long)neuron[j].exci_decay) >> 16;
            long long inhi_decay = ((long long)inhi_current * (long long)neuron[j].inhi_decay) >> 16; 
            // Subtract the decay quantity from the current and shift back down
            int next_exci_current = (exci_current - (int)(exci_decay));
            int next_inhi_current = (inhi_current - (int)(inhi_decay));
            
            // Write decayed values to next timestep's input bins 
            uint cpsr = spin1_irq_disable();            //!MUTEX!
            psp_buffer[j].exci[(ticks + 1) % PSP_BUFFER_SIZE] += next_exci_current;
            psp_buffer[j].inhi[(ticks + 1) % PSP_BUFFER_SIZE] += next_inhi_current;
            spin1_mode_restore(cpsr);            
        }
    }
    
/*    flush_output_spikes();*/
}

