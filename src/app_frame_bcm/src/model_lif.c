#include "spin1_api.h"
#include "spinn_io.h"
#include "spinn_sdp.h" // Required by comms.h

#include "comms.h"
#include "config.h"
#include "dma.h"
#include "model_general.h"
#include "model_lif.h"
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
            if (spin1_get_chip_id() == 0x0000 && spin1_get_core_id() == 0x0002 && decoded_word.index == 0)
        io_printf(IO_STD, "Type %d, weight %d, index %d, arrival %d, slot %d, exci slot address: %08x, inhi slot address: %08x, synapses: %08x, synaptic word: %08x\n", decoded_word.synapse_type, decoded_word.weight, decoded_word.index, arrival, arrival % PSP_BUFFER_SIZE, (int)&psp_buffer[decoded_word.index].exci[arrival % PSP_BUFFER_SIZE], (int)&psp_buffer[decoded_word.index].inhi[arrival % PSP_BUFFER_SIZE], (int) &synaptic_row->synapses[i], synaptic_row->synapses[i]);
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
    decoded_word -> synapse_type = (word >>13) & 0x7;
    decoded_word -> weight = word & 0x1FFF;
    decoded_word -> index = (word >> 16) & 0x7ff;
    decoded_word -> delay = 1 + ((word >> 28) & 0xf);
    decoded_word -> weight_scale = 8;
}

unsigned int encode_synaptic_word (synaptic_word_t *decoded_word)
{
    uint encoded_word = 0;

    encoded_word = (((decoded_word -> stdp_on) & 0x01) << 27);
    encoded_word |= ((decoded_word -> synapse_type & 0x07) << 13);
    encoded_word |= ((decoded_word -> weight) & 0x1FFF);
    encoded_word |= (((decoded_word -> index) & 0x7ff) << 16);
    encoded_word |= (((decoded_word -> delay - 1) & 0x0f) << 28);

    return encoded_word;
}
