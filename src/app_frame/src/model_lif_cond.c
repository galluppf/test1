#include "spin1_api.h"
#include "spinn_io.h"
#include "spinn_sdp.h" // Required by comms.h

#include "comms.h"
#include "config.h"
#include "dma.h"
#include "model_general.h"
#include "model_lif_cond.h"
#include "recording.h"

#ifdef STDP
#include "stdp.h"
#endif

#ifdef STDP_SP
#include "stdp_sp.h"
#endif

// Neuron data structures
uint num_populations;
population_t *population;
psp_buffer_t *psp_buffer;


void buffer_post_synaptic_potentials(void *dma_copy, uint row_size)
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
            case 0: psp_buffer[decoded_word.index].exci[arrival % PSP_BUFFER_SIZE] += (decoded_word.weight << (16 - decoded_word.weight_scale)); break;
            case 1: psp_buffer[decoded_word.index].inhi[arrival % PSP_BUFFER_SIZE] += (decoded_word.weight << (16 - decoded_word.weight_scale)); break;
            default: break;
        }
    }
}


// set_bias_value sets all the neurons in population population_id on this core to value
void set_bias_value(uint population_id, int value)
{
    neuron_t *neuron = (neuron_t *) population[population_id].neuron;

    for(uint j = 0; j < population[population_id].num_neurons; j++)
    {
        neuron[j].bias_current = value;
    }
}


/*  Set_bias_values reads tuples (neuron_id, value) in the SDP packet and changes the value for the bias current of each neuron_id to value
    The source SDP packet has the following structure: 
        - population_id (relative to the core) sdp_msg -> arg1
        - length of the payload in 4 bytes words: sdp_msg->arg3
        - series of tuples (ushort neuron_id, short value): sdp_msg->data. value needs to be scaled TO 8.8 already, in the routine it will be scaled down by 8 positions so to be in 16.16 notation
    It is similar to set_bias_value but with this function every neuron can be injected a different current
*/
void set_bias_values(uint population_id, uint length, uint * payload)
{
    neuron_t *neuron = (neuron_t *) population[population_id].neuron;

    for(uint j = 0; j < length; j++)
    {
        uint neuron_id = payload[j] & 0xFFFF;
        int bias_current_value = (payload[j] & 0xFFFF0000) >> 8;
        neuron[neuron_id].bias_current = bias_current_value;
//        io_printf(IO_STD, "time: %d set bias current for neuron %d in population %d to %d\n", spin1_get_simulation_time(), neuron_id, population_id, bias_current_value);
    }
}



void set_population_flag(int operation, int flag_position, int population_id)
{
    if (operation == 0) population[population_id].flags = population[population_id].flags & (~(1<<flag_position));   // Clear
    if (operation == 1) population[population_id].flags = population[population_id].flags | (1<<flag_position);   // Set
}


void timer_callback(uint ticks, uint null)
{
    long long current = 0;
    long long rest = 0;
    long long dv = 0;
    long long input = 0;
    long long integration = 0;

    uint neuron_count = 0;

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
            long long exci_current = psp_buffer[j].exci[ticks % PSP_BUFFER_SIZE];
            long long inhi_current = psp_buffer[j].inhi[ticks % PSP_BUFFER_SIZE];

            // Clear PSP buffers for this timestep
            psp_buffer[j].exci[ticks % PSP_BUFFER_SIZE] = 0;
            psp_buffer[j].inhi[ticks % PSP_BUFFER_SIZE] = 0;

            // If this neuron is not in a refractory period...
	        if(neuron[j].refrac_clock == 0)
	        {
                // ... compute the input current for a conductance based model...
                current =     (exci_current*(neuron[j].v_rev_exc - neuron[j].v) >> LOG_P2)      // g_exc(v_rev_exc-v)
                            + (inhi_current*(neuron[j].v_rev_inh - neuron[j].v) >> LOG_P2)      // g_inh(v_rev_inh-v)
                            + neuron[j].bias_current;
                            
                input = current * neuron[j].v_resistance;
                input = input >> LOG_P2;

                // ... and its effect on membrane potential...
                rest = neuron[j].v_rest - neuron[j].v;
                dv = (rest + input) * neuron[j].v_decay;
                dv = dv >> LOG_P2;
                integration = neuron[j].v + dv;

                // ... control for overflow...
                if(integration < INT_MIN)       integration = INT_MIN;
                else if(integration > INT_MAX)  integration = INT_MAX;

                // ... reduce the result back down to an integer...
                neuron[j].v = (int) integration;

                // ... and emit a spike if necessary
                if(neuron[j].v >= neuron[j].v_thresh)
                {
                    uint key = spin1_get_chip_id() << 16 |
                               app_data.virtual_core_id << 11 |
                               population[i].id |
                               j;

                    spin1_send_mc_packet(key, NULL, NO_PAYLOAD);

                    neuron[j].v = neuron[j].v_reset;
                    neuron[j].refrac_clock = neuron[j].refrac_tau;

#if defined STDP || defined STDP_SP
                    StdpPostUpdate(neuron_count, ticks);
#endif
                }
            }
            // Else decrement the refractory clock
            else
            {
	            neuron[j].refrac_clock--;
            }

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

            // Recording:
                    
            // Optionally record voltage and current trace
            if(population[i].flags & RECORD_STATE_BIT)
            {
                record_v[population[i].num_neurons * (ticks - 1) + j] = (short) (neuron[j].v >> LOG_P1);
/*                record_v[population[i].num_neurons * (ticks - 1) + j] = (short) (exci_current) >> LOG_P1;*/
            }
            
            if(population[i].flags & RECORD_GSYN_BIT)
            {
                record_i[population[i].num_neurons * (ticks - 1) + j] = (short) (current >> LOG_P1);
            }                

        } // Cycle neurons in population i 
            
        if((population[i].flags & OUTPUT_RATE_BIT) && (ticks & OUTPUT_STATISTIC_TIMER)==0)            // Sending aggregate spike count
        {

            // Instruction 64 is the rate
            // Instruction 65 is the bias
//            io_printf(IO_STD, "%d ms, spikes: %d normalized: %d\n", ticks, number_of_spikes, number_of_spikes*1000/(population[i].num_neurons*64));
            // instruction code 0 is the rate
            send_special_mc_packet(i, 0, spike_count[i]);       // i is the population id
            // instruction code 1 is the bias current
            send_special_mc_packet(i, 1, neuron[0].bias_current);
            spike_count[i] = 0;
        }
                       
    }   // Cycle populations
                
}


// Functions specific for every neural model, implementing prototypes in model_general.h

void handle_sdp_msg(sdp_msg_t * sdp_msg)
{
    switch(sdp_msg->cmd_rc)
    {
        case 0x102:
            set_population_flag(sdp_msg->arg1, sdp_msg->arg2, sdp_msg->arg3);
            break;
        case 0x103:
            set_bias_value(sdp_msg->arg1, sdp_msg->arg3);
            break;
        case 0x104:
            set_bias_values(sdp_msg->arg1, sdp_msg->arg3, (uint *) sdp_msg->data);
            break;            
        default:
            io_printf(IO_STD, "SDP ERROR: Instruction code %x not handled\n", sdp_msg->cmd_rc);
            break;
    }
}


void configure_recording_space()
{
    record_v = (short *) 0x72136400 + 0x200000 * (app_data.virtual_core_id - 1);
    record_i = (short *) 0x7309a400 + 0x200000 * (app_data.virtual_core_id - 1);

    // cleaning
    for (uint i = 0; i < 0x100000; i++)    record_v[i] = 0;
    for (uint i = 0; i < 0x100000; i++)    record_i[i] = 0;

    
    record_spikes = (uint *) 0x72040000;
    for(uint i = 0; i < 1000; i++) record_spikes[i] = 0; //TODO improve 

    // spike count

    int *spike_count_dest = NULL;
    spike_count_dest = (int *) spin1_malloc(num_populations);
    spike_count =  (int *) spike_count_dest;
    for(uint i = 0; i < num_populations; i++) spike_count[i] = 0; //TODO improve    
    
}

void decode_synaptic_word (unsigned int word, synaptic_word_t *decoded_word)
{
    decoded_word -> stdp_on = (word >> 27) & 0x01;
    decoded_word -> synapse_type = word & 0x7;
    decoded_word -> weight = word & 0xfff8;
    decoded_word -> index = (word >> 16) & 0x7ff;
    decoded_word -> delay = 1 + ((word >> 28) & 0xf);
    decoded_word -> weight_scale = 16;
}

unsigned int encode_synaptic_word (synaptic_word_t *decoded_word)
{
    uint encoded_word = 0;

#ifdef DEBUG
    io_printf(IO_STD, "stdp: %d, synapse_type: %d, weight: %04x, index: %d, delay: %d\n", (decoded_word -> stdp_on), (decoded_word -> synapse_type), (decoded_word -> weight), (decoded_word -> index), decoded_word -> delay);
#endif

    encoded_word = (((decoded_word -> stdp_on) & 0x01) << 27);
    encoded_word |= ((decoded_word -> synapse_type) & 0x07);
    encoded_word |= ((decoded_word -> weight) & 0xfff8);
    encoded_word |= (((decoded_word -> index) & 0x7ff) << 16);
    encoded_word |= (((decoded_word -> delay - 1) & 0x0f) << 28);

#ifdef DEBUG
    io_printf(IO_STD, "encoded word: %08x\n", encoded_word);
#endif

    return encoded_word;
}
