/****** main.c/summary
 *
 * SUMMARY
 *  This program handles application monitoring packets, collates them
 *  into categories, buffers then and sends on to the host as SDP messages.
 *
 * AUTHOR
 *  Thomas Sharp - thomas.sharp@cs.man.ac.uk
 *
 * COPYRIGHT
 *  SpiNNaker Project, The University of Manchester
 *  Copyright (C) SpiNNaker Project, 2010. All rights reserved.
 *
 *******/



#include "spin1_api.h"
#include "spinn_io.h"
#include "spinn_sdp.h"



#define DEBUG_CODES                 (16)
#define NUM_CHANNELS                (DEBUG_CODES + 1) // + 1 for spikes
#define SDP_HEADER_SIZE             (24) // bytes
#define SPIKE_CHANNEL               (NUM_CHANNELS - 1)



typedef struct
{
    sdp_msg_t *message;     // Pointer to currently active sdp_message
    sdp_msg_t message_0;
    sdp_msg_t message_1;
    uint active;            // ID of currently active sdp_message
    uint received;          // Number of multicast packets received
    uint transmitted;       // Number of SDP messages sent
    uint lost;              // Number of multicast packets dropped
} channel_t;

typedef struct
{
    uint key;
    uint payload;
} mc_packet_t;

typedef struct
{
    uint run_time;
    uint synaptic_row_length;
    uint max_axonal_delay;
    uint num_populations;
    uint total_neurons;
    uint neuron_data_size;
    uint virtual_core_id;
    uint reserved_2;
} app_data_t;



void c_main(void);
void dump_mc_packets(uint, uint);
void mc_packet_callback(uint, uint);
void timer_callback(uint, uint);
void send_sdp_packet(int tag, int cmd_rc, int arg1, int arg2, int arg3);

app_data_t app_data;

channel_t channel[NUM_CHANNELS];



/****f* main.c/c_main
 *
 * SUMMARY
 *  Prepares the SDP messages which are used to buffer and retransmit multicast
 *  packets from each of the monitoring channels. Sets up the timer and
 *  simulation synchronisation mechanisms. Registers the appropriate timer and
 *  multicast packet callbacks.
 *
 * SYNOPSIS
 *  int c_main()
 *
 * SOURCE
 */
void c_main()
{
    io_printf(IO_STD, "Hello, World!\nBooting app_dump...\n");

    // Set the core map and get application data
    spin1_set_core_map(64, (uint *)(0x74220000)); // FIXME make the number of chips dynamic
    app_data = *((app_data_t *) (0x74000000 + 0x10000 * (spin1_get_core_id() - 1)));

    // Set timer tick (in microseconds)
    spin1_set_timer_tick(1000*1);

    // Register callbacks (packet RX = FIQ, timer = non-queueable)
    spin1_callback_on(MC_PACKET_RECEIVED, mc_packet_callback, -1);
    spin1_callback_on(TIMER_TICK, timer_callback, 0);

    // Initialise each of the channels
    for(uint i = 0; i < NUM_CHANNELS; i++)
    {
        // Configure first SDP message
        channel[i].message_0.tag = 1; // Arbitrary tag
        channel[i].message_0.flags = 0x07; // No reply required

        channel[i].message_0.dest_addr = 0; // Chip 0,0
        channel[i].message_0.dest_port = PORT_ETH; // Dump through Ethernet

        channel[i].message_0.srce_addr = spin1_get_chip_id();
        channel[i].message_0.srce_port = 3 << 5; // Monitoring port
        channel[i].message_0.srce_port |= spin1_get_core_id();

        channel[i].message_0.cmd_rc = 64 + i; // Monitoring channel
        channel[i].message_0.length = 0;

        // Configure second SDP message
        channel[i].message_1.tag = 1; // Arbitrary tag
        channel[i].message_1.flags = 0x07; // No reply required

        channel[i].message_1.dest_addr = 0; // Chip 0,0
        channel[i].message_1.dest_port = PORT_ETH; // Dump through Ethernet

        channel[i].message_1.srce_addr = spin1_get_chip_id();
        channel[i].message_1.srce_port = 3 << 5; // Monitoring port
        channel[i].message_1.srce_port |= spin1_get_core_id();

        channel[i].message_1.cmd_rc = 64 + i; // Monitoring channel
        channel[i].message_1.length = 0;

        // Point to the first SDP message
        channel[i].message = &channel[i].message_0;
        channel[i].active = 0;

        // Initialise log variables
        channel[i].received = 0;
        channel[i].transmitted = 0;
        channel[i].lost = 0;
    }

    // Go!
    io_printf(IO_STD, "app_dump booted!\nStarting app_dump...\n");
    spin1_start();
    io_printf(IO_STD, "app_dump complete.\n");//TODO detailed print %d spikes dumped, %d overflows reported\n", spikes, overflow);

    // Tell the spike receiver than the simulation has finished
    spin1_delay_us(10000);
    channel[SPIKE_CHANNEL].message->cmd_rc = 64 + NUM_CHANNELS;
    channel[SPIKE_CHANNEL].message->length = SDP_HEADER_SIZE;
    
    spin1_send_sdp_msg(channel[SPIKE_CHANNEL].message, 10); // 1ms timeout
    spin1_delay_us(10000);
    spin1_send_sdp_msg(channel[SPIKE_CHANNEL].message, 10); // 1ms timeout    
    spin1_delay_us(10000);
    
    // Tell the PyNN runsystem that the simulation is over
    send_sdp_packet(2, 80, 0, 0, 0);
    spin1_delay_us(10000);
    send_sdp_packet(2, 80, 0, 0, 0);    
}
/*
 *******/

void send_sdp_packet(int tag, int cmd_rc, int arg1, int arg2, int arg3)
{

    sdp_msg_t general_sdp_packet;
    general_sdp_packet.tag = tag;
    general_sdp_packet.dest_port = PORT_ETH;
    general_sdp_packet.dest_addr = 0;
    general_sdp_packet.flags = 0x07;
    general_sdp_packet.srce_port = 4 << 5 | app_data.virtual_core_id;
    general_sdp_packet.srce_addr = spin1_get_chip_id();
    general_sdp_packet.length = 28;
    general_sdp_packet.cmd_rc = cmd_rc;
    general_sdp_packet.arg1 = arg1;
    general_sdp_packet.arg2 = arg2;
    general_sdp_packet.arg3 = arg3;

    spin1_send_sdp_msg (&general_sdp_packet, 10);
//    io_printf(IO_STD, "\nsending sdp packet\n");
    general_sdp_packet.arg1 = 0;
}


/****f* main.c/mc_packet_callback
 *
 * SUMMARY
 *
 *
 * SYNOPSIS
 *
 *
 * SOURCE
 */
void mc_packet_callback(uint key, uint payload)
{
/*    io_printf(IO_STD, "%d\n", (key & 0x0000F800) >> 11);*/
    // If this is a monitoring packet...

//    if(key & 0x00008000)      // FIXME : pacman cores need to go from 0-15, not from 1-16
    if( ((key & 0x0000F800) >> 11) > 16)
    {
        io_printf(IO_STD, "NOOOOO! %x, %x\n", key, (key & 0x0000F800) >> 11);
        // ... identify the channel to which it belongs...
        uint c_id = key & 0xf;
        sdp_msg_t *message = channel[c_id].message;
        uint *data = (uint *) message->data;

        // ... copy the data...
        data[message->length] = key;
        data[message->length] = payload;
        message->length += sizeof(uint) * 2;
        channel[c_id].received++;

        // ... and schedule the transmission if necessary
        if(message->length >= SDP_BUF_SIZE)
        {
            message->arg1 = message->length;
            message->arg2 = spin1_get_simulation_time();

            message->length += SDP_HEADER_SIZE;

            spin1_send_sdp_msg(message, 1); // 1ms timeout

            message->length = 0;

            channel[c_id].transmitted++;
        }
    }
    // Else it's a spike packet...
    else
    {
        sdp_msg_t *message = channel[SPIKE_CHANNEL].message;
        uint *data = (uint *) message->data;

        if(message->length < SDP_BUF_SIZE / sizeof(int))
        {
            data[message->length++] = key;
            channel[SPIKE_CHANNEL].received++;
        }
        else
        {
            channel[SPIKE_CHANNEL].lost++;
        }
    }
}
/*
 *******/


/****f* main.c/timer_callback
 *
 * SUMMARY
 *
 *
 * SYNOPSIS
 *
 *
 * SOURCE
 */
void timer_callback(uint ticks, uint null)
{
    uint cpsr = spin1_int_disable();

    sdp_msg_t *message = channel[SPIKE_CHANNEL].message;

    if(message->length)
    {
        message->arg2 = message -> length;
        message->length = message->length * sizeof(uint) + SDP_HEADER_SIZE;
        message->arg1 = ticks;

        spin1_send_sdp_msg(message, 1); // 1ms timeout

        message->length = 0;

        channel[SPIKE_CHANNEL].message = channel[SPIKE_CHANNEL].active ?
                                            &channel[SPIKE_CHANNEL].message_0 :
                                            &channel[SPIKE_CHANNEL].message_1;
        channel[SPIKE_CHANNEL].active ^= 1;
    }

    spin1_mode_restore(cpsr);

    if(ticks >= app_data.run_time)
    {
        io_printf(IO_STD, "Stopping!\n");
        spin1_stop();
    }
}
/*
 *******/
