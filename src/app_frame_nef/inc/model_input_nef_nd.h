#ifndef __MODEL_LIF_NEF_H__
#define __MODEL_LIF_NEF_H__

#define LOG_P1                  (8)
#define LOG_P2                  (16)

#define nef_v_rest ((short)(-19200))    // -75 << 8
#define nef_v_reset ((short)(-19200))   // -75 << 8
#define nef_v_thresh ((short)(-8690))   // -35 << 8
#define nef_decay ((short)(3277))       // 1.0/20.0 << 16
#define nef_tau_refrac ((short)(2))     // 2 :)


typedef struct
{
    short v;                // <<8
    short encoder;          // <<8  (will contain the scaling factor as well)
    int bias_current;       // <<8
    int value_current;      // <<8    
    int refrac_clock;       // counter
} neuron_t;


#endif

