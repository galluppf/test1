#ifndef __MODEL_SPIKE_SOURCE_POISSON_H__
#define __MODEL_SPIKE_SOURCE_POISSON_H__

#define LOG_P1                  (8)
#define LOG_P2                  (16)

#pragma pack(1)

typedef struct
{
    unsigned long long rate_conv;   //8
    unsigned int start;             //4
    unsigned int end;               //4
    int time_to_next_spike;         //4
} neuron_t;

typedef union
{
    unsigned long long value;
    struct
    {
        unsigned int lo;
        unsigned int hi;
    } words;
}uint_64_t;

#endif
