#ifndef __MODEL_IZHIKEVICH_H__
#define __MODEL_IZHIKEVICH_H__



#define LOG_P1                  (8)
#define LOG_P2                  (16)
#define P1                      (1 << LOG_P1) // 256
#define P2                      (1 << LOG_P2) // 65536
#define IZK_CONST_1             (4*P2/100)
#define IZK_CONST_2             (5*P1)
#define IZK_CONST_3             (140*P1)
#define IZK_THRESHOLD           (30)



typedef struct
{
    // State variables (scaled by P1)
    int v;      // Membrane potential
    int u;      // Recovery variable
    // Izhikevich equation constants (scaled by P2)
    short ab;   // ab
    short na;   // -a
    short c;    // c
    short d;    // d
    // Synaptic current input terms (scaled by P2)
    ushort exci_decay;    // Excitatory decay constant
    ushort inhi_decay;    // Inhibitory decay constant
    // Bias_current, constant until switched by host (scaled by P1)
    int bias_current;
} neuron_t;



#endif

