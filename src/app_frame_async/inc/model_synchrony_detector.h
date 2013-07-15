#ifndef __MODEL_DENDRITIC_SYNCHRONY_H__
#define __MODEL_DENDRITIC_SYNCHRONY_H__



#define LOG_P1                  (8)
#define LOG_P2                  (16)
#define P1                      (1 << LOG_P1)
#define P2                      (1 << LOG_P2)



typedef struct
{
    // Membrane potential constants and variables
    int last1;
    int last2;
    int refractory_period;
    int delay_window;
    int last_spike;
} neuron_t;


// spike count
#define OUTPUT_STATISTIC_TIMER 0x3F     // Time interval to collect spike counts
int * spike_count;              // used by recorders


#endif
