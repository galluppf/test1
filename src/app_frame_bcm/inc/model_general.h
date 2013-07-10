#ifndef __MODEL_GENERAL_H__
#define __MODEL_GENERAL_H__



#define INT_MAX                 ( 2147483647) //TODO define programmatically
#define INT_MIN                 (-2147483648)

typedef struct
{
    int weight;
    unsigned short index;
    unsigned char delay;
    unsigned char stdp_on;
    unsigned char synapse_type;
    unsigned char weight_scale;
} synaptic_word_t;


void timer_callback(uint, uint);

// TODO: do we need these?
// void decode_synaptic_word (unsigned int word, synaptic_word_t *decoded_word);
// unsigned int encode_synaptic_word (synaptic_word_t *decoded_word);


#endif
