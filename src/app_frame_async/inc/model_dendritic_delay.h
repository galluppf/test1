#ifndef __MODEL_DELAY_US_H__
#define __MODEL_DELAY_US_H__



#define LOG_P1                  (8)
#define LOG_P2                  (16)
#define P1                      (1 << LOG_P1)
#define P2                      (1 << LOG_P2)

#define SPIKE_QUEUE_SIZE        100

typedef struct
{
    // Membrane potential constants and variables
    uint delay;
} neuron_t;

typedef struct
{
  uint ts;
  uint key;
} spike_t;

typedef struct
{
  uint ring_start;
  uint ring_end;
  uint ring_empty;
  spike_t ring[SPIKE_QUEUE_SIZE];
} spike_ring_t;

// spike count
#define OUTPUT_STATISTIC_TIMER 0x3F     // Time interval to collect spike counts
int * spike_count;              // used by recorders


#endif
