/****** main.c/summary
 *
 * AUTHOR
 *  Francesco Galluppi - francesco.galluppi@cs.man.ac.uk
 *
 * COPYRIGHT
 *  SpiNNaker Project, The University of Manchester
 *  Copyright (C) SpiNNaker Project, 2010. All rights reserved.
 *
 *******/



#include "spin1_api.h"
#include "spinn_io.h"
#include "spinn_sdp.h"

uint dest_key = 0;

typedef struct
{
    uint key;
    uint mask;
    uint route;
} mc_table_entry_t;



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
void mc_packet_callback(uint, uint);
void timer_callback(uint, uint);


app_data_t app_data;



/****f* main.c/c_main
 *
 * SUMMARY
 *
 * SYNOPSIS
 *  int c_main()
 *
 * SOURCE
 */
void c_main()
{
    io_printf(IO_STD, "Hello, World!\nBooting app_proxy...\n");

    // Set the core map and get application data
    spin1_set_core_map(64, (uint *)(0x74220000)); // FIXME make the number of chips dynamic
    app_data = *((app_data_t *) (0x74000000 + 0x10000 * (spin1_get_core_id() - 1)));

    // Set timer tick (in microseconds)
    spin1_set_timer_tick(1000*1);

    // Register callbacks (packet RX = FIQ, timer = non-queueable)
    spin1_callback_on(MC_PACKET_RECEIVED, mc_packet_callback, -1);
    spin1_callback_on(TIMER_TICK, timer_callback, 0);
    
    dest_key = (spin1_get_chip_id() & 0xFFFF) << 16;    // computing the dest_key and using it as a constant in the callback
    
    uint size = *((uint *) 0x74210000);
    mc_table_entry_t *mc = (mc_table_entry_t *) 0x74210004;

    for(uint i = 0; i < size; i++)
    {
        spin1_set_mc_table_entry(i, mc[i].key, mc[i].mask, mc[i].route);
    }
    

    // Go!
    io_printf(IO_STD, "app_proxy booted!\nStarting app_proxy...\n");
    spin1_start();
    io_printf(IO_STD, "app_proxy complete.\n");//TODO detailed print %d spikes proxyed, %d overflows reported\n", spikes, overflow);

    // Tell the spike receiver than the simulation has finished
    spin1_delay_us(10000);
}
/*
 *******/


/****f* main.c/mc_packet_callback
 *
 * SUMMARY
 * The packet callback will swap the top half portion of the MC key with the one for the current chip and re-issue the packet
 *
 * SYNOPSIS
 *
 * 
 * SOURCE
 */ 
void mc_packet_callback(uint key, uint payload)
{
/*    io_printf(IO_STD, "R: 0x%x\n", key);*/
    key = (dest_key) | (key & 0xFFFF) + 0x800;          //  Retinal input from 254 254 0 0-128x128x2 
    if (payload != 0)   spin1_send_mc_packet(key, payload, 1);  //if it's a packet with payload send with the payload
    else                spin1_send_mc_packet(key, 0, 0);
    
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

    if(ticks >= app_data.run_time)
    {
        io_printf(IO_STD, "Stopping!\n");
        spin1_stop();
    }
}
/*
 *******/
