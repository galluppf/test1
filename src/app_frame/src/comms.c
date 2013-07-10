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
/*extern population_t *population;*/

void flush_output_spikes()
{
    if(spike_output->arg1)
    {
        spike_output->srce_addr = spin1_get_chip_id();
        spike_output->arg2 = spin1_get_simulation_time();

        spike_output->length = 24 + spike_output->arg1 * 4;
        spin1_send_sdp_msg(spike_output, 1); // Timeout on transmission at 1ms
/*        io_printf(IO_STD, "sending spikes out\n");*/
        spike_output->arg1 = 0;
        spike_output->cmd_rc = 0x100;
    }
}


void mc_packet_callback(uint key, uint payload)
{
//    io_printf(IO_STD, "R: %x\n", key);
    buffer_mc_packet(key, payload);
}

void output_spike(uint key)
{
    uint *data = (uint *) spike_output->data;
    data[spike_output->arg1++] = key;

    if(spike_output->arg1 >= SDP_BUF_SIZE / sizeof(int)) flush_output_spikes();
}


void sdp_packet_callback(uint msg, uint null)
{
    sdp_msg_t *sdp_msg = (sdp_msg_t *) msg;
    sdp_packets_received++;
//    io_printf (IO_STD, "SDP PACKET: flags: %x tag: %x dest_port: %x srce_port: %x dest_addr: %x srce_addr: %x cmd_rc: %x arg1: %x\n", sdp_msg -> flags, sdp_msg -> tag, sdp_msg -> dest_port, sdp_msg -> srce_port, sdp_msg -> dest_addr, sdp_msg -> srce_addr, sdp_msg -> cmd_rc, sdp_msg -> arg1);
    switch(sdp_msg->cmd_rc)
    {
        case 0x100: // SDP input spikes packet
/*            io_printf(IO_STD, "%d Spike Received\n", spin1_get_simulation_time());    */
            input_spikes(sdp_msg->arg1, (uint *) sdp_msg->data);
            break;
        default:
            handle_sdp_msg(sdp_msg);
            break;
    }
    spin1_msg_free(sdp_msg);
}


void input_spikes(uint length, uint *key)
{
    for(uint i = 0; i < length; i++)
    {
/*        io_printf(IO_STD, "Received key %d\n", key[i]);*/
        spin1_send_mc_packet(key[i], NULL, FALSE);
    }
}



void send_sync_sdp(void)
{
    sdp_msg_t sync_pkt;

    sync_pkt.tag = 1;
    sync_pkt.dest_port = PORT_ETH;
    sync_pkt.dest_addr = 0;
    sync_pkt.flags = 0x07;
    sync_pkt.srce_port = 3 << 5 | app_data.virtual_core_id;
    sync_pkt.srce_addr = spin1_get_chip_id();
    sync_pkt.cmd_rc = 0;
    sync_pkt.arg1 = 0;
    sync_pkt.arg2 = spin1_get_simulation_time();
    sync_pkt.arg3 = 0;
    sync_pkt.data[0] = 0;
    sync_pkt.data[1] = 0;
    sync_pkt.length = 26;
    io_printf(IO_STD, "sending sync spike\n");
    spin1_send_sdp_msg (&sync_pkt, 1);

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
    sdp_value_packet.srce_addr = spin1_get_chip_id();;
    *ptr = out_value;
    sdp_value_packet.length = 28;
    sdp_value_packet.cmd_rc = 257;
    sdp_value_packet.arg1 = arg1;
    sdp_value_packet.arg2 = arg2;
    sdp_value_packet.arg3 = arg3;

    spin1_send_sdp_msg (&sdp_value_packet, 1);
/*    io_printf(IO_STD, "sending vakue %d\n", out_value);*/
    sdp_value_packet.arg1 = 0;
}


/* sending an mc packet to the application monitoring processor */
void send_special_mc_packet(unsigned short population_id, unsigned short instruction_code, int value)
{
    
    uint key =  spin1_get_chip_id() << 16 | 
                1 << 15 |                           //   spike/no spike                
                spin1_get_core_id()-1 << 11 | 
                1 << 10 |                           //   system/application
                population_id << 4 | 
                instruction_code;            
                
/*    io_printf(IO_STD, "key: %8x x: %u y: %u s: %u core_id : %u a: %u popid : %u ic : %u\n", */
/*                key,*/
/*                (key >> 24) & 0xFF,*/
/*                (key >> 16) & 0xFF,                */
/*                (key >> 15) & 0x1,*/
/*                (key >> 11) & 0xF, */
/*                (key >> 10) & 0x1,                            //   system/application*/
/*                (key >> 4) & 0x3F, */
/*                (key & 0xF));                                   */
                
    spin1_send_mc_packet(key, value, 1);
}
