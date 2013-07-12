#include "spin1_api.h"
#include "spinn_io.h"
#include "spinn_sdp.h"
#include "model_general.h"

#include "config.h"
#include "comms.h"
#include "dma.h"
uint sdp_packets_received = 0;


mc_packet_buffer_t mc_packet_buffer;
sdp_msg_t *spike_output;
extern app_data_t app_data;

int dma_received = 0;


void flush_output_spikes()
{
    if(spike_output->arg1)
    {
        spike_output->arg2 = spin1_get_simulation_time();
   
        spike_output->length = 24 + spike_output->arg1 * 4;
        spin1_send_sdp_msg(spike_output, 100); // Timeout on transmission at 100ms
        
        spike_output->arg1 = 0;
    }
}

void send_out_value(int arg1, int arg2, int arg3, int out_value)
{

    sdp_msg_t sdp_value_packet;
    int * ptr = (int *)&(sdp_value_packet.data[0]);
    //io_printf (IO_STD, "sending packet\n");
    sdp_value_packet.tag = 1;
    sdp_value_packet.dest_port = PORT_ETH;
    sdp_value_packet.dest_addr = 0;
    sdp_value_packet.flags = 0x07;
    sdp_value_packet.srce_port = 4 << 5 | app_data.virtual_core_id;
    sdp_value_packet.srce_addr = 0;
    *ptr = out_value;
    sdp_value_packet.length = 28;
    sdp_value_packet.cmd_rc = 0x102;
    sdp_value_packet.arg1 = arg1;
    sdp_value_packet.arg2 = arg2;
    sdp_value_packet.arg3 = arg3;

    spin1_send_sdp_msg (&sdp_value_packet, 5);
/*    io_printf(IO_STD, "sending value %d\n", out_value);*/
    sdp_value_packet.arg1 = 0;
}


void mc_packet_callback(uint key, uint payload)
{
/*    io_printf(IO_STD, "packet received: %d", key);*/
    buffer_mc_packet(key, payload);
}



void sdp_packet_callback(uint msg, uint null)
{
    sdp_msg_t *sdp_msg = (sdp_msg_t *) msg;
	
/*    sdp_packets_received++;*/
/*    io_printf (IO_STD, "%x %x %x %x %x %x %x %x\n", sdp_msg -> flags, sdp_msg -> tag, sdp_msg -> dest_port, sdp_msg -> srce_port, sdp_msg -> dest_addr, sdp_msg -> srce_addr, sdp_msg -> cmd_rc, sdp_msg -> arg1);    */
    switch(sdp_msg->cmd_rc)
    {
        case 0x100: // SDP input spikes packet            
/*            io_printf(IO_STD, "received command %x, value %d at %d\n", sdp_msg->cmd_rc, x_value, spin1_get_simulation_time());        */
            break;    
        default:
            handle_sdp_msg(sdp_msg);
            break;
    }
    spin1_msg_free(sdp_msg);
}


void output_spike(uint key)
{
    uint *data = (uint *) spike_output->data;
    data[spike_output->arg1++] = key;

    if(spike_output->arg1 >= SDP_BUF_SIZE / sizeof(int)) flush_output_spikes();
}


void input_spikes(uint length, uint *key)
{
/*    uint cpsr = irq_disable();*/
    for(uint i = 0; i < length; i++)
    {
        spin1_send_mc_packet(key[i], NULL, FALSE);
    }
/*    irq_restore(cpsr);*/
}


void send_sync_sdp(void)
{

    spike_output -> tag = 1;
    spike_output -> dest_port = PORT_ETH;
    spike_output -> dest_addr = 0;
    spike_output -> flags = 0x07;
    spike_output -> srce_port = 3 << 5 | app_data.virtual_core_id;
    spike_output -> srce_addr = 0;
    spike_output -> data[0] = 0;
    spike_output -> data[1] = 0;
    spike_output -> length = 26;

    spin1_send_sdp_msg (spike_output, 100);

    spike_output->flags = 7;
    spike_output->tag = 1;
    spike_output->dest_port = 255;
    spike_output->srce_port = 1;
    spike_output->dest_addr = 0;
    spike_output->srce_addr = spin1_get_chip_id();
    spike_output->cmd_rc = 0x100;
    spike_output->arg1 = 0;
}

