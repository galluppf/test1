#include "spin1_api.h"
#include "spinn_io.h"
#include "spinn_sdp.h" // Required by comms.h

#include "comms.h"
#include "config.h"
#include "dma.h"
#include "model_general.h"
#include "model_izhikevich.h"
#include "recording.h"

#ifdef STDP
#include "stdp.h"
#endif

#ifdef STDP_SP
#include "stdp_sp.h"
#endif

#ifdef STDP_TTS
#include "stdp_tts.h"
#endif


// Neuron data structures
uint num_populations;
population_t *population;
psp_buffer_t *psp_buffer;

void buffer_post_synaptic_potentials(void *dma_copy, uint row_size) //TODO row size should be taken from the row itself, and not passed via the DMA tag
{
#ifdef DEBUG
    io_printf(IO_STD, "Inside buffer_post_synaptic_potentials\n");
#endif

    uint time = spin1_get_simulation_time();
    synaptic_row_t *synaptic_row = (synaptic_row_t *) dma_copy;

    for(uint i = 0; i < row_size; i++)
    {
        synaptic_word_t decoded_word;

        decode_synaptic_word (synaptic_row->synapses[i], &decoded_word);

        uint arrival = time + decoded_word.delay;

#ifdef DEBUG
        io_printf(IO_STD, "Type %d, weight %d, index %d, arrival %d, synapses: %08x, synaptic word: %08x\n", decoded_word.synapse_type, decoded_word.weight, decoded_word.index, arrival, (int) &synaptic_row->synapses[i], synaptic_row->synapses[i]);
#endif

        switch(decoded_word.synapse_type)
        {
            case 0: psp_buffer[decoded_word.index].exci[arrival % PSP_BUFFER_SIZE] += decoded_word.weight; break;
            case 1: psp_buffer[decoded_word.index].inhi[arrival % PSP_BUFFER_SIZE] += decoded_word.weight; break;
            default: break;
        }
    }
}


void timer_callback(uint ticks, uint null)
{
    uint neuron_count = 0;

#ifdef DEBUG
    io_printf(IO_STD, "Timer Callback. Tick %d\n", ticks);
#endif

    if(ticks >= app_data.run_time)
    {
        io_printf(IO_STD, "Simulation complete.\n");
        spin1_stop();
    }

    for(uint i = 0; i < num_populations; i++)
    {
        neuron_t *neuron = (neuron_t *) population[i].neuron;
        psp_buffer_t *psp_buffer = population[i].psp_buffer;

        for(uint j = 0; j < population[i].num_neurons; j++, neuron_count++)
        {
            // Get excitatory and inhibitory currents from synaptic inputs
            int exci_current = psp_buffer[j].exci[ticks % PSP_BUFFER_SIZE];
            int inhi_current = psp_buffer[j].inhi[ticks % PSP_BUFFER_SIZE];
            int current = exci_current - inhi_current + neuron[j].bias_current;

            // Clear PSP buffers for this timestep
            psp_buffer[j].exci[ticks % PSP_BUFFER_SIZE] = 0;
            psp_buffer[j].inhi[ticks % PSP_BUFFER_SIZE] = 0;

#ifdef DEBUG
            io_printf(IO_STD, "neuron %d, msec %d, initial membrane value: %d, current injected: %d, ", j, ticks, neuron[j].v, current);
#endif


            // Compute numerical approximation of Izhikevich equations
            neuron[j].v += (neuron[j].v *
                           (IZK_CONST_1 * neuron[j].v / P2 + IZK_CONST_2) / P1
                           +IZK_CONST_3
                           +current
                           -neuron[j].u)
                           / 2;
            neuron[j].v += (neuron[j].v *
                           (IZK_CONST_1 * neuron[j].v / P2 + IZK_CONST_2) / P1
                           +IZK_CONST_3
                           +current
                           -neuron[j].u)
                           / 2;


//implementation of Izhikevich neuron compatible with Scott's code with the addition of the double-step integration

//variable v reeeepresented in 8.8 in a 32 bit int type (only the lower 16 bits used)
//TODO: scale up to use the whole 32 bit vaiable
/*
            int v_temp;

            v_temp = ((int) (((long long) IZK_CONST_1 * (long long) neuron[j].v) >> 16) + IZK_CONST_2) << 8;
            v_temp = (int) (((long long) v_temp * (long long) neuron[j].v) >> 16) + IZK_CONST_3;
            neuron[j].v = (int) ((long long) neuron[j].v + ((v_temp + current - neuron[j].u) >> 1));

            v_temp = ((int) (((long long) IZK_CONST_1 * (long long) neuron[j].v) >> 16) + IZK_CONST_2) << 8;
            v_temp = (int) (((long long) v_temp * (long long) neuron[j].v) >> 16) + IZK_CONST_3;
            neuron[j].v = (int) ((long long) neuron[j].v + ((v_temp + current - neuron[j].u) >> 1));
*/
            // Set v = max(v, threshold)
            if(neuron[j].v >= IZK_THRESHOLD * P1) neuron[j].v = IZK_THRESHOLD * P1;

/*
            // Compute u
            neuron[j].u = neuron[j].na * neuron[j].u / P2
                         +neuron[j].u
                         +neuron[j].ab * neuron[j].v / P2;
*/

            int u_temp;
            u_temp = ((neuron[j].na * neuron[j].u) >> 16) + neuron[j].u;
            neuron[j].u = (int) (((long long) neuron[j].ab * (long long) neuron[j].v) >> 16) + u_temp;

            // Optionally record v and current FROM A SINGLE POPULATION ONLY
            if(population[i].flags & RECORD_STATE_BIT)
            {
                record_v[population[i].num_neurons * ticks + j] = (short) neuron[j].v;                
            }
            
            if(population[i].flags & RECORD_GSYN_BIT)
            {
                record_i[population[i].num_neurons * ticks + j] = (short) current;
            }

            // Transmit spikes and reset state
            if(neuron[j].v >= IZK_THRESHOLD * P1)
            {
#ifdef DEBUG
                io_printf(IO_STD, "Spike! Population %d Neuron %d\n", population[i].id, j);
#endif

                uint key = spin1_get_chip_id() << 16 | app_data.virtual_core_id << 11 | population[i].id | j;
                spin1_send_mc_packet(key, NULL, NO_PAYLOAD);

#ifdef SPIKE_IO
                // Optionally send spike to host FROM A SINGLE POPULATION ONLY
                if(population[i].flags & OUTPUT_SPIKE_BIT) output_spike(key);
#endif

                // Optionally record spikes FROM A SINGLE POPULATION ONLY
                if(population[i].flags & RECORD_SPIKE_BIT)
                {
                    uint size = population[i].num_neurons >> 5;
                    size += (population[i].num_neurons & 0x1F) ? 1 : 0;

                    record_spikes[(size * ticks) + (j >> 5)] |= 1 << (j & 0x1F);
                }

                neuron[j].v = neuron[j].c;
                neuron[j].u += neuron[j].d;

#if defined STDP || defined STDP_SP || defined STDP_TTS
                StdpPostUpdate(neuron_count, ticks);
#endif
            }

            // exponential decay for current injected in a neuron
            // Compute synapse dynamics (NB exci_decay is scaled up by 2^16 for precision)
            long long exci_decay = exci_current * neuron[j].exci_decay;
            long long inhi_decay = inhi_current * neuron[j].inhi_decay;
            // Subtract the decay quantity from the current and shift back down
            long long next_exci_current = exci_current - (exci_decay >> LOG_P2);
            long long next_inhi_current = inhi_current - (inhi_decay >> LOG_P2);
            // Write decayed values to next timestep's input bins
            uint cpsr = spin1_irq_disable();
            psp_buffer[j].exci[(ticks + 1) % PSP_BUFFER_SIZE] += (int) next_exci_current;
            psp_buffer[j].inhi[(ticks + 1) % PSP_BUFFER_SIZE] += (int) next_inhi_current;
            spin1_mode_restore(cpsr);
            
/*	    
            // Compute PSP dynamics (NB exci_decay is scaled up by 2^16 for precision)
            long long exci_decay = (long long)exci_current * (long long)neuron[j].exci_decay;
            long long inhi_decay = (long long)inhi_current * (long long)neuron[j].inhi_decay;
            // Subtract the decay quantity from the current and shift back down
            long long next_exci_current = (uint) ((((long long)exci_current << LOG_P2) - exci_decay) >> LOG_P2);
            long long next_inhi_current = (uint) ((((long long)inhi_current << LOG_P2) - inhi_decay) >> LOG_P2);
            // Write decayed values to next timestep's input bins

//            io_printf(IO_STD, "before: bin_val %d, next_exci_current: %d\n", psp_buffer[j].exci[(ticks + 1) % PSP_BUFFER_SIZE], (int)next_exci_current);
            psp_buffer[j].exci[(ticks + 1) % PSP_BUFFER_SIZE] = (uint)((long long)psp_buffer[j].exci[(ticks + 1) % PSP_BUFFER_SIZE] + next_exci_current);
//            io_printf(IO_STD, "after: bin_val: %d\n", psp_buffer[j].exci[(ticks + 1) % PSP_BUFFER_SIZE]);
            psp_buffer[j].inhi[(ticks + 1) % PSP_BUFFER_SIZE] = (uint)((long long)psp_buffer[j].inhi[(ticks + 1) % PSP_BUFFER_SIZE] + next_inhi_current);
*/

        }
    }

#ifdef SPIKE_IO
    flush_output_spikes();
#endif
}

// Specific functions for each neural model

void configure_recording_space()
{
    record_v = (short *) 0x72136400 + 0x200000 * (app_data.virtual_core_id - 1);
    record_i = (short *) 0x7309a400 + 0x200000 * (app_data.virtual_core_id - 1);
    record_spikes = (uint *) 0x72040000;
    
    // cleaning
    for (uint i = 0; i < 0x100000; i++)    record_v[i] = 0;
    for (uint i = 0; i < 0x100000; i++)    record_i[i] = 0;

    
    for(uint i = 0; i < 1000; i++) record_spikes[i] = 0; //TODO improve 
}

void handle_sdp_msg(sdp_msg_t *sdp_msg)
{
    return;
}

void decode_synaptic_word (unsigned int word, synaptic_word_t *decoded_word)
{
    decoded_word -> stdp_on = (word >> 27) & 0x01;
    decoded_word -> synapse_type = (word >> 13) & 0x7;
    decoded_word -> weight = word & 0x1fff;
    decoded_word -> index = (word >> 16) & 0x7ff;
    decoded_word -> delay = 1 + ((word >> 28) & 0xf);
    decoded_word -> weight_scale = 8;
}

unsigned int encode_synaptic_word (synaptic_word_t *decoded_word)
{
    uint encoded_word = 0;

#ifdef DEBUG
    io_printf(IO_STD, "stdp: %d, synapse_type: %d, weight: %04x, index: %d, delay: %d\n", (decoded_word -> stdp_on), (decoded_word -> synapse_type), (decoded_word -> weight), (decoded_word -> index), decoded_word -> delay);
#endif

    encoded_word = (((decoded_word -> stdp_on) & 0x01) << 27);
    encoded_word |= (((decoded_word -> synapse_type) & 0x07) << 13);
    encoded_word |= ((decoded_word -> weight) & 0x1fff);
    encoded_word |= (((decoded_word -> index) & 0x7ff) << 16);
    encoded_word |= (((decoded_word -> delay - 1) & 0x0f) << 28);

#ifdef DEBUG
    io_printf(IO_STD, "encoded word: %08x\n", encoded_word);
#endif

    return encoded_word;
}
