// ----------------------------------------------------------------------------
// $Id: main.c 1452 2011-10-26 14:00:17Z sharpt $
// Copyright(C) The University of Manchester
// APT Group, School of Computer Science
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// Abstract             : sample application main file
// Project              : SpiNNaker API
// $HeadURL: https://solem.cs.man.ac.uk/svn/app_frame/trunk/src/main.c $
// Author               : $Author: sharpt $
// Creation date        : 03 May 2011
// Last modified date   : $Date: 2011-10-26 15:00:17 +0100 (Wed, 26 Oct 2011) $
// Version              : $Revision: 1452 $
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

    // Register callbacks
    spin1_callback_on(MC_PACKET_RECEIVED, mc_packet_callback, -1);
    spin1_callback_on(DMA_TRANSFER_DONE, dma_callback, 0);
    spin1_callback_on(USER_EVENT, feed_dma_pipeline, 0);
    spin1_callback_on(TIMER_TICK, timer_callback, 2);
    spin1_callback_on(SDP_PACKET_RX, sdp_packet_callback, 1);
            
    spin1_start();
    
    // Cleaning up within each application (diagnostics etc.)
    app_done();            

}

void app_done(void)
{
/*    short size_diagnostic = 6;*/
/*    uint * diagnostics;*/
/*    diagnostics = (uint *) (0x7335A400 + (spin1_get_core_id()-1)*size_diagnostic*4);*/

/*    io_printf(IO_STD, "Simulation complete. Diagnostics in 0x%x\n", diagnostics);*/

/*    diagnostics[0] = api_diagnostics.warnings;*/
/*    diagnostics[1] = api_diagnostics.total_mc_packets;*/
/*    diagnostics[2] = api_diagnostics.dumped_mc_packets;*/
/*    diagnostics[3] = api_diagnostics.dma_bursts;*/
/*    diagnostics[4] = 0;*/
/*    diagnostics[5] = 0;            */
}
/*
 *******/

