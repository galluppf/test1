#ifndef __MODEL_NEF_OUT_MULTIDIMENSIONAL_H__
#define __MODEL_NEF_OUT_MULTIDIMENSIONAL_H__

#define LOG_P1                  (8)
#define LOG_P2                  (16)

//# define DIMENSIONS             2

typedef struct
{
    int decoder[DIMENSIONS];          // <<16  (will contain the scaling factor as well)
} neuron_t;

void send_out_2d_value(int arg1, int arg2, int arg3, int out_value_0, int out_value_1);

#endif                          // __MODEL_NEF_OUT_MULTIDIMENSIONAL_H__

