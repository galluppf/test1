#ifndef __COMMS_H__
#define __COMMS_H__



#define MC_PACKET_BUFFER_SIZE 256



typedef struct
{
    uint start;
    uint end;
    uint *buffer;
} mc_packet_buffer_t;



extern mc_packet_buffer_t mc_packet_buffer;
extern sdp_msg_t *spike_output;



void flush_output_spikes(void);
void mc_packet_callback(uint, uint);
void sdp_packet_callback(uint, uint);
void output_spike(uint);
void input_spikes(uint, uint *);
void send_sync_sdp(void);
void send_out_value(int arg1, int arg2, int arg3, int out_value);



#endif

