/****** main.c/main
*
* SUMMARY
*
* AUTHOR
*
* DETAILS
* 
* COPYRIGHT
*  SpiNNaker Project, The University of Manchester
*  Copyright (C) SpiNNaker Project, 2013. All rights reserved.
*
*******/

#include "spin1_api.h"
#include "spinn_io.h"
#include "spinn_sdp.h" // Required by comms.h

#include "comms.h"
#include "config.h"
#include "dma.h"
#include "model_general.h"

#define PLASTICITY_UPDATE_TIME 1024

// TODO: These needs to be dynamically set by PACMAN
#define NUMBER_OF_POST_NEURONS 128
#define STARTING_LOOKUP_ADDRESS 0x00000000

// post_spike_count is a uint * [NUMBER_OF_POST_NEURONS][BUFFER_SIZE]
// the 2 buffers are switched in alternative phases to avoid collisions with
// ongoing simulation
uint * post_spike_count;

// thresholds is a vector of adaptive thresholds, one for each post neuron
uint * thresholds;


/****f* main.c/c_main
*
* SUMMARY
*
* SYNOPSIS
*
* INPUTS
*  NONE
*
* OUTPUTS
*  NONE
* 
* SOURCE
*/

void c_main()
{
    io_printf(IO_STD, "Hello, World!\nBooting application...\n");

    // Set the number of chips in the simulation
    spin1_set_core_map(64, (uint *)(0x74220000)); // FIXME make the number of chips dynamic

    // Set timer tick (in microseconds)
    spin1_set_timer_tick(1000*PLASTICITY_UPDATE_TIME);

    // Configure simulation
    load_mc_routing_tables();
    configure_app_frame();

    // Register callbacks
    spin1_callback_on(MC_PACKET_RECEIVED, mc_packet_callback, -1);
    spin1_callback_on(DMA_TRANSFER_DONE, dma_callback, 0);
    spin1_callback_on(USER_EVENT, feed_dma_pipeline, 0);
    spin1_callback_on(TIMER_TICK, timer_callback, 2);
    spin1_callback_on(SDP_PACKET_RX, sdp_packet_callback, 1);

    // Go!
    io_printf(IO_STD, "Application booted!\nStarting simulation...\n");
    spin1_start();
}

/****f* config.c/configure_app_frame
*
* SUMMARY
*
* SYNOPSIS
*  void configure_app_frame()
*
* INPUTS
*  NONE
*
* OUTPUTS
*  NONE
* 
* SOURCE
*/


void configure_app_frame()
{

    // Load synapse lookup tables for your twin neural core
    /*
    size = *((uint *) (0x74200000 + 0x1000 * (twin_neural_core_id - 1))) * sizeof(synapse_lookup_t); //TODO I want size in bytes!
    source = (char *) 0x74200004 + 0x1000 *  (twin_neural_core_id - 1);
    dest = (char *) spin1_malloc(size);
    for(uint i = 0; i < size; i++) dest[i] = source[i];
    synapse_lookup = (synapse_lookup_t *)  dest;
    */

    
/*  // calculating the size of the row length
    for (uint i = 0; synapse_lookup[i].core_id != 0xffffffff; i++)
    {
        if (synapse_lookup[i].row_size > app_data.synaptic_row_length)
        {
            app_data.synaptic_row_length = synapse_lookup[i].row_size;
        }
    }
*/    

    // TODO: configure DMA pipeline    

/*
    dma_pipeline.busy = FALSE;
    dma_pipeline.flip = 0;
    dma_pipeline.row_size_max = sizeof(synaptic_row_t) + sizeof(uint) * app_data.total_neurons;
    dma_pipeline.cache[0] = (synaptic_row_t *) spin1_malloc(dma_pipeline.row_size_max);
    dma_pipeline.cache[1] = (synaptic_row_t *) spin1_malloc(dma_pipeline.row_size_max);
    dma_pipeline.synapse_lookup_address[0] = dma_pipeline.synapse_lookup_address[1] = NULL;
    dma_pipeline.row_size[0] = dma_pipeline.row_size[1] = 0;
*/

    // Configure comms buffers // FIXME do I need this?
    mc_packet_buffer.start = mc_packet_buffer.end = 0;
    mc_packet_buffer.buffer = spin1_malloc(MC_PACKET_BUFFER_SIZE * sizeof(int));

    // Instantiates the thresholds vector
    thresholds = spin1_malloc(sizeof(uint) * NUMBER_OF_POST_NEURONS);
    for (int i = 0; i < NUMBER_OF_POST_NEURONS; i++) thresholds[i] = 0;
    
    // Instantiates the post_spike_count buffer 
    post_spike_count = spin1_malloc(sizeof(uint) * NUMBER_OF_POST_NEURONS * BUFFER_SIZE);
    for (int i = 0; i < NUMBER_OF_POST_NEURONS; i++) post_spike_count[i] = 0;

}

/*
*******/


/****f* main.c/timer_callback
*
* SUMMARY
* This function is called every PLASTICITY_UPDATE_TIME ms, and starts cycling 
* the weight blocks through the dma pipeline: 
* - initiate reading of every weight_row from the synaptic block by calling 
*   DMA pipeline
* SYNOPSIS
* void timer_callback(uint ticks, uint null)
*
* INPUTS
*  NONE
*
* OUTPUTS
*  NONE
* 
* SOURCE
*/


void timer_callback(uint ticks, uint null)
{
    io_printf(IO_STD, "Using buffer %d", ticks & PLASTICITY_UPDATE_TIME);
    // start_dma_pipeline_process
    // feed_dma_pipeline(starting address)
}

/*
*******/

// TODO: set up a process of DMA pipelining, where every row in the weight block
// is read. The end (beginning?) of each dma_callback kicks the next dma
// TODO: think of a way to optimize dma reads and writes scheduling?
void feed_dma_pipeline(synaptic_row_address)
{
    // this is started by the timer tick and gets called on next weight row every
    // time a read dma_callback is done
}


/****f* main.c/dma_callback
*
* SUMMARY
* This function is called upon the receipt of a DMA (TODO: need to disambuguate
* between read and writes? using tags?)
* - sets the pre spike-count list to 0 and immediately writes itback to SDRAM 
*   using write_buffer to avoid collisions with the neural core
*
* - initiates the read of the next row if there's any left (TODO how? need a pipeline)
* TODO: avoid rows with 0s
*
*   updates weights following the BCM rule by comparing the pre-spike count list 
*   (pre neuron firing rate r_pre, at the beginning of the row in SDRAM) and the
*   post-spike count list 
*
*   BCM rule: dw/dt = r_post(r_post - theta)r_pre - eps*w
*   (watch out for the dt -> model it as a gain factor)              
*
* - writes back the synaptic row (just the synapses)
* 
*
* SYNOPSIS
*  void c_main()
*
* INPUTS
*  NONE
*
* OUTPUTS
*  NONE
* 
* SOURCE
*/

void dma_callback(uint null0, uint tag)
{

    if (tag != NULL) // it's a read, the tag is the address of the row we are getting
    {
        synaptic_row_t *synaptic_row = dma_pipeline.cache[dma_pipeline.flip ^ 1];
        uint row_size = dma_pipeline.row_size[dma_pipeline.flip ^ 1]; //in bytes, including synaptic row header
    }
    else    // it's a write, do nothing
    {
            // maybe start the next read here
    }

}

/*
*******/


/****f* main.c/mc_packet_callback
*
* SUMMARY
* PACMAN sets a route/projection between the neural and the plasticity core
* Every post spike is the routed to the plasticity core, which updates the spike_count
* used to calculate the rate
* 
* SYNOPSIS
*  void c_main()
*
* INPUTS
*  NONE
*
* OUTPUTS
*  NONE
* 
* SOURCE
*/


void mc_packet_callback(uint key, uint payload)
{
    // increments the corresponding post_spike_count buffer
    short phase = (spin1_get_simulation_time() & PLASTICITY_UPDATE_TIME) == PLASTICITY_UPDATE_TIME;
    post_spike_count[(key & 0x7FF) + (NUMBER_OF_POST_NEURONS*phase)]++;
}

/*
*******/

/****f* main.c/timer2_callback
This will modify theta
*******/
