#ifndef __TRACE_H__
#define __TRACE_H__



#define RECORD_SPIKE_BIT            (1 << 0)
#define RECORD_STATE_BIT            (1 << 1)
#define OUTPUT_SPIKE_BIT            (1 << 4)
#define OUTPUT_STATE_BIT            (1 << 5)
#define OUTPUT_RATE_BIT             (1 << 6)

#define VALUE_ROBOT_OUTPUT          (1 << 7)



extern short *record_v;
extern short *record_i;
extern uint *record_spikes;



#endif
