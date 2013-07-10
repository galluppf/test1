// ----------------------------------------------------------------------------
// $Id: main.c 2046 2012-11-07 16:18:47Z daviess $
// Copyright(C) The University of Manchester
// APT Group, School of Computer Science
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// Abstract             : sample application main file
// Project              : SpiNNaker API
// $HeadURL: https://solem.cs.man.ac.uk/svn/app_spike_source/testing/src/main.c $
// Author               : $Author: daviess $
// Creation date        : 03 May 2011
// Last modified date   : $Date: 2012-11-07 16:18:47 +0000 (Wed, 07 Nov 2012) $
// Version              : $Revision: 2046 $
// ------------------------------------------------------------------------

/****** main.c/summary
 *
 * SUMMARY
 *  This file contains the main function of the application framework, which
 *  the application programmer uses to configure and run applications.
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
#include "spinn_sdp.h" // Required by comms.h

#include "comms.h"
#include "config.h"
#include "dma.h"
#include "model_general.h"

#ifdef STDP
#include "stdp.h"
#endif



/****f* main.c/c_main
 *
 * SUMMARY
 *  This function is called at application start-up. The application programmer
 *  must implement the function body to configure hardware, register callbacks,
 *  and finally begin the simulation.
 *
 * SYNOPSIS
 *  int c_main()
 *
 * SOURCE
 */
void c_main()
{
    io_printf(IO_STD, "Hello, World!\nBooting application...\n");

    // Set the number of chips in the simulation
    spin1_set_core_map(64, (uint *)(0x74220000)); // FIXME make the number of chips dynamic

    // Set timer tick (in microseconds)
    spin1_set_timer_tick(1000*1);

    // Configure simulation
    load_application_data();
    load_mc_routing_tables();
    configure_app_frame();

#ifdef STDP
    load_stdp_config_data();
    alloc_stdp_post_TS_mem();
#endif

    // Register callbacks
    spin1_callback_on(DMA_TRANSFER_DONE, dma_callback, 0);
    spin1_callback_on(TIMER_TICK, timer_callback, 2);
    spin1_callback_on(SDP_PACKET_RX, sdp_packet_callback, 1);

    // Go!
    io_printf(IO_STD, "Application booted!\nStarting simulation...\n");
    spin1_start();
}
/*
 *******/
