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
    void *pointer;   // Pointer to input buffers for this population - not used int spike sources
} population_t;



extern uint num_populations;
extern population_t *population;


void handle_sdp_msg(sdp_msg_t *);
void set_population_data_pointer(void *);
void set_population_flag(int operation, int flag_position, int population_id);
void timer_callback(uint, uint);

// These functions need to be implemented by every neural model
void handle_sdp_msg(sdp_msg_t *);
void configure_recording_space(void);



#endif
