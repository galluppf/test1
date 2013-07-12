/*#include "spinn_api.h"*/
#include "spin1_api.h"
#include "spinn_io.h"
#include "spinn_sdp.h" // Required by comms.h

#include "comms.h"
#include "config.h"
#include "dma.h"
#include "model_general.h"
#include "model_lif.h"
#include "recording.h"



// Neuron data structures
uint num_populations;
population_t *population;
psp_buffer_t *psp_buffer;

extern int overflows;

void buffer_post_synaptic_potentials(void *dma_copy, uint row_size)
{
    uint time = spin1_get_simulation_time();
    
    synaptic_row_t *synaptic_row = (synaptic_row_t *) dma_copy;
    
    //SD commenting out this part and replacing it with the same section from the Izhikevich neuron model
    /*
    uint i = 0;
    while(synaptic_row->synapses[i])
    {
        uint weight = synaptic_row->synapses[i] & 0xffff;
        uint neuron_id = synaptic_row->synapses[i] >> 16 & 0x3ff;
        uint type = synaptic_row->synapses[i] >> 26 & 0x1;
        uint arrival = 1 + time + synaptic_row->synapses[i] >> 28 & 0xf;
        
        switch(type)
        {
            case 0: psp_buffer[neuron_id].inhi[arrival % PSP_BUFFER_SIZE] += weight; break;
            case 1: psp_buffer[neuron_id].exci[arrival % PSP_BUFFER_SIZE] += weight; break;
            default: break;
        }

        i++;
    }
    */

    for(uint i = 0; i < row_size; i++)
    {
        uint type = synaptic_row->synapses[i] & 0x7;
        uint weight = synaptic_row->synapses[i] & 0xfff8;
        uint index = synaptic_row->synapses[i] >> 16 & 0x7ff;
        //SD commenting out the next instruction because stdp is not used here
        //uint stdp = synaptic_row->synapses[0] >> 27 & 0x1;
        uint arrival = 1 + time + (synaptic_row->synapses[i] >> 28 & 0xf);
        
/*        io_printf(IO_STD, "Type %d, weight %d, index %d, arrival %d, synapses: %x\n", type, weight, index, arrival, (int) &synaptic_row->synapses[i]);*/
        
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
/*    io_printf(IO_STD, "Time: %d\n", ticks);*/
    if(ticks % 128 == 0)  io_printf(IO_STD, "Time: %d\n", ticks);


    if(ticks == 1 && app_data.virtual_core_id == 1 && spin1_get_chip_id() == 0)
    {
        io_printf(IO_STD, "Sending sync spike.\n");
        send_sync_sdp();
    }

    
    if(ticks >= app_data.run_time)
    {
        spin1_us_delay(spin1_get_core_id()*100);
        io_printf(IO_STD, "Simulation complete.\n");
        io_printf(IO_STD, "\t\t[ info ] overflows: %d.\n", overflows);
        spin1_stop();
    }

    for(uint i = 0; i < num_populations; i++)
    {
        neuron_t *neuron = (neuron_t *) population[i].neuron;
        psp_buffer_t *psp_buffer = population[i].psp_buffer;
        
        for(uint j = 0; j < population[i].num_neurons; j++)
        {
            // Get excitatory and inhibitory currents from synaptic inputs

        uint exci_current = psp_buffer[j].exci[ticks % PSP_BUFFER_SIZE];
        uint inhi_current = psp_buffer[j].inhi[ticks % PSP_BUFFER_SIZE];
        
        short current=0;
        int current_temp = 0;

	    if (neuron[j].refrac_clock==0) // if we're not in refractory period
	    {	            
                current_temp = exci_current - inhi_current + neuron[j].bias_current;
/*                if (j == 0)   io_printf(IO_STD, "current %d, exci_current =%d, inhi_current=%d.\n", current_temp, exci_current, exci_current);*/
                if (current_temp < -32768)            current_temp = -32768;        // saturation control
                else if (current_temp > 32768)            current_temp = 32768;        // saturation control                
                current = (short) (current_temp);
                
                int temp = 0;
                
                temp =     neuron[j].v + (((neuron[j].v_rest - neuron[j].v) * neuron[j].decay)>>16)  + current ;
                
                if (temp < -32768)          temp = -32768;        // saturation control
                else if (temp > 32768)      temp = 32768;        // saturation control        
                
                neuron[j].v = (short)(temp);
                
        }
        else
	        neuron[j].refrac_clock--;


        // Transmit spikes and reset state
        if(neuron[j].v >= neuron[j].v_thresh)
        {
            uint key = spin1_get_chip_id() << 16 | app_data.virtual_core_id << 11 | population[i].id | j;
            
            spin1_send_mc_packet(key, NULL, NO_PAYLOAD);
            
/*            neuron[j].v = neuron[j].v_reset;*/
            neuron[j].v = neuron[j].v_reset + neuron[j].v - neuron[j].v_thresh;

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
                record_v[population[i].num_neurons * ticks + j] = (short) neuron[j].v;
                record_i[population[i].num_neurons * ticks + j] = (short) current;
            }
                        
            // Clear PSP buffers for this timestep
            psp_buffer[j].exci[ticks % PSP_BUFFER_SIZE] = 0;
            psp_buffer[j].inhi[ticks % PSP_BUFFER_SIZE] = 0;
            
            
            // Compute PSP dynamics (NB exci_decay is scaled up by 2^16 for precision)
            uint exci_decay = (exci_current * neuron[j].exci_decay);
            uint inhi_decay = (inhi_current * neuron[j].inhi_decay);
            // Subtract the decay quantity from the current and shift back down
            uint next_exci_current = ((exci_current << LOG_P2) - exci_decay) >> LOG_P2;
            uint next_inhi_current = ((inhi_current << LOG_P2) - inhi_decay) >> LOG_P2;        
            // Write decayed values to next timestep's input bins //!MUTEX!
            psp_buffer[j].exci[(ticks + 1) % PSP_BUFFER_SIZE] += next_exci_current;
            psp_buffer[j].inhi[(ticks + 1) % PSP_BUFFER_SIZE] += next_inhi_current;
        }
    }
    
    flush_output_spikes();
}

