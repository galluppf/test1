/*#include "spinn_api.h"*/
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

void mc_packet_callback(uint key, uint payload)
{
//    io_printf(IO_STD, "R: %x\n", key);
    buffer_mc_packet(key, payload);
}


void sdp_packet_callback(uint msg, uint null)
{
    sdp_msg_t *sdp_msg = (sdp_msg_t *) msg;
    sdp_packets_received++;
//    io_printf (IO_STD, "SDP PACKET: flags: %x tag: %x dest_port: %x srce_port: %x dest_addr: %x srce_addr: %x cmd_rc: %x arg1: %x\n", sdp_msg -> flags, sdp_msg -> tag, sdp_msg -> dest_port, sdp_msg -> srce_port, sdp_msg -> dest_addr, sdp_msg -> srce_addr, sdp_msg -> cmd_rc, sdp_msg -> arg1);
    switch(sdp_msg->cmd_rc)
    {
        default:
            handle_sdp_msg(sdp_msg);
            break;
    }
    spin1_msg_free(sdp_msg);
}

