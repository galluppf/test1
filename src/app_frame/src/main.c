/****** main.c/main
*
* SUMMARY
*  This file contains the main function of the application framework, which
*  the application programmer uses to configure and run applications.
*
* AUTHOR
*  Thomas Sharp - thomas.sharp@cs.man.ac.uk
*
* DETAILS
*  Created on       : 03 May 2011
*  Version          : $Revision: 2048 $
*  Last modified on : $Date: 2012-11-08 13:02:14 +0000 (Thu, 08 Nov 2012) $
*  Last modified by : $Author: daviess $
*  $Id: main.c 2048 2012-11-08 13:02:14Z daviess $
*  $HeadURL: https://solem.cs.man.ac.uk/svn/app_frame/testing/src/main.c $
* 
* COPYRIGHT
*  SpiNNaker Project, The University of Manchester
*  Copyright (C) SpiNNaker Project, 2010. All rights reserved.
*
*******/

#include "spin1_api.h"
#include "spinn_io.h"
#include "spinn_sdp.h" // Required by comms.h

#include "comms.h"
#include "config.h"
#include "dma.h"
#include "model_general.h"

#ifdef STDP
#include "stdp.h"
#endif

#ifdef STDP_SP
#include "stdp_sp.h"
#endif

#ifdef STDP_TTS
#include "stdp_tts.h"
#endif

/****f* main.c/c_main
*
* SUMMARY
*  This function is called at application start-up. The application programmer
*  must implement the function body to configure hardware, register callbacks,
*  and finally begin the simulation.
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

void c_main()
{
    io_printf(IO_STD, "Hello, World!\nBooting application...\n");

    // Set the number of chips in the simulation
//    spin1_set_core_map(4, (uint *)(0x74220000)); // FIXME make the number of chips dynamic
    spin1_set_core_map(64, (uint *)(0x74220000)); // FIXME make the number of chips dynamic


    // Set timer tick (in microseconds)
    spin1_set_timer_tick(1000*1);

    // Configure simulation
    load_application_data();
    load_mc_routing_tables();

#if defined STDP || defined STDP_SP || defined STDP_TTS
    load_stdp_config_data();
    alloc_stdp_post_TS_mem(); //TODO: check for return value
#endif

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

/*
*******/
