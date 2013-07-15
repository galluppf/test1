#ifndef __MODEL_GENERAL_H__
#define __MODEL_GENERAL_H__



#define INT_MAX                 ( 2147483647) //TODO define programmatically
#define INT_MIN                 (-2147483648)


typedef struct
{
    uint id;            // Population ID
    uint flags;         // Flags for debug etc.
    uint num_neurons;   // Number of neurons in population
    uint neuron_size;   // Size of each neuron data structure
    uint null;          // Reserved
    void *neuron;       // Pointer to neuron structures
    void *null2;        // Compatibility with other models
} population_t;


typedef struct
{
    int weight;
    unsigned short index;
    unsigned char delay;
    unsigned char stdp_on;
    unsigned char synapse_type;
    unsigned char weight_scale;
} synaptic_word_t;


extern uint num_populations;
extern population_t *population;

void synaptic_event(void *, uint);
void handle_sdp_msg(sdp_msg_t *);
void set_population_data_pointer(void *);
void set_population_flag(int operation, int flag_position, int population_id);
void timer_callback(uint, uint);

// These functions need to be implemented by every neural model
void handle_sdp_msg(sdp_msg_t *);
void configure_recording_space(void);
void configure_model(void);

void decode_synaptic_word (unsigned int word, synaptic_word_t *decoded_word);
unsigned int encode_synaptic_word (synaptic_word_t *decoded_word);


#endif
