#ifndef __MODEL_LIF_32_H__
#define __MODEL_LIF_32_H__

#define LOG_P1                  (8)
#define LOG_P2                  (16)

#pragma pack (1)

typedef struct
{
    int v;            // <<16
    int v_rest;       // <<16
    ushort decay;       // <<16  (1/tau)
    ushort resistance;
    int v_reset;      // <<16
    int v_thresh;     // <<16
    // Synaptic current input terms
    // Synaptic current input terms (scaled by P2)
    ushort exci_decay;    // <<16 Excitatory decay constant
    ushort inhi_decay;    // <<16 Inhibitory decay constant
    // Bias_current, constant until switched by host (scaled by P1)
    int bias_current;     // << 16
    ushort tau_refrac;
    ushort refrac_clock;
} neuron_t;


#endif

