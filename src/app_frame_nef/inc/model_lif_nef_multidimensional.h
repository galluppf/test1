# ifndef __MODEL_LIF_NEF_MULTIDIMENSIONAL_H__
# define __MODEL_LIF_NEF_MULTIDIMENSIONAL_H__
//# define DIMENSIONS     2


# define LOG_P1                  (8)
# define LOG_P2                  (16)

//# define nef_v_rest ((int)(-4915200))    // -75 << 16
//# define nef_v_reset ((int)(-4915200))   // -75 << 16
//# define nef_v_thresh ((int)(-2293760))   // -35 << 16
//# define nef_decay ((short)(3277))       // 1.0/20.0 << 16
//# define nef_tau_refrac ((short)(2))     // 2 :)

# define nef_v_rest ((int)(0))    // 0 << 16
# define nef_v_reset ((int)(0))   // -75 << 16
# define nef_v_thresh ((int)(65536))   // -35 << 16
# define nef_decay ((short)(3277))       // 1.0/20.0 << 16
# define nef_tau_refrac ((short)(2))     // 2 :)



typedef struct
{
    int v;                      // <<16
    int encoder[DIMENSIONS];    // <<16  (will contain the scaling factor as well)
    int bias_current;           // <<16
    int value_current;          // <<16   
    int refrac_clock;           // counter
} neuron_t;

//extern short[DIMENSIONS] x_value;      // input value for NEF input neurons

# endif

