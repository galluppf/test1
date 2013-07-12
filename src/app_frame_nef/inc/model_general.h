#ifndef __MODEL_GENERAL_H__
#define __MODEL_GENERAL_H__

#define PSP_BUFFER_SIZE         (2)

//NB this will have to be moved into model_x.h as synapse models become more detailed
typedef struct
{
    uint exci[PSP_BUFFER_SIZE];
    uint inhi[PSP_BUFFER_SIZE];
} psp_buffer_t;

typedef struct
{
    uint id;            // Population ID
    uint flags;         // Flags for debug etc.
    uint num_neurons;   // Number of neurons in population
    uint neuron_size;   // Size of each neuron data structure
    uint null;          // Reserved
    void *neuron;       // Pointer to neuron structures
    psp_buffer_t *psp_buffer;   // Pointer to input buffers for this population
} population_t;



extern uint num_populations;
extern population_t *population;
extern psp_buffer_t *psp_buffer; // Contiguous input buffers for all neurons



void buffer_post_synaptic_potentials(void *, uint);
void set_population_data_pointer(void *);
void timer_callback(uint, uint);
void handle_sdp_msg(sdp_msg_t *);

#endif

