/****a* simple.c/simple_summary
*
* SUMMARY
*  sample SpiNNaker API application
*
* AUTHOR
*  Luis Plana - luis.plana@manchester.ac.uk
*
* DETAILS
*  Created on       : 03 May 2011
*  Version          : $Revision: 1196 $
*  Last modified on : $Date: 2011-06-27 14:32:29 +0100 (Mon, 27 Jun 2011) $
*  Last modified by : $Author: plana $
*  $Id: simple.c 1196 2011-06-27 13:32:29Z plana $
*  $HeadURL: file:///home/amulinks/spinnaker/svn/spinn_api/trunk/examples/simple.c $
*
* COPYRIGHT
*  Copyright (c) The University of Manchester, 2011. All rights reserved.
*  SpiNNaker Project
*  Advanced Processor Technologies Group
*  School of Computer Science
*
*******/

// SpiNNaker API
#include "spin1_api.h"
#include "spinn_io.h"

// ------------------------------------------------------------------------
// simulation constants
// ------------------------------------------------------------------------
#define TIMER_TICK_PERIOD  1000000
#define TOTAL_TICKS        10
#define ITER_TIMES         18
#define PRINT_DLY          200
#define BUFFER_SIZE        100

#define NUMBER_OF_CHIPS    4
#define CORE_MAP_00        0x00002
#define CORE_MAP_01        0x00002
#define CORE_MAP_10        0x00002
#define CORE_MAP_11        0x00002

// ------------------------------------------------------------------------
// function prototypes
// ------------------------------------------------------------------------
/*
void app_init(void);
void app_done(void);
void flip_led(uint a, uint b);
void count_packets(uint key, uint payload);
void check_memcopy(uint tid, uint a);
void do_transfer (uint val, uint none);
*/

// ------------------------------------------------------------------------
// variables
// ------------------------------------------------------------------------
uint coreID;
uint chipID;
uint test_DMA;

uint packets = 0;
uint pfailed = 0;

uint transfers = 0;
uint tfailed = 0;
uint tfaddr = 0;
uint tfvald;
uint tfvals;

uint ufailed = 0;

uint * dtcm_buffer;
uint * sysram_buffer;
uint * sdram_buffer;

uint core_map[NUMBER_OF_CHIPS];

/****f* simple.c/get_ncores
*
* SUMMARY
*  This function returns the number of cores in the simulation.
*
* SYNOPSIS
*  get_ncores(uint chips, uint * core_map)
*
* INPUTS
*   uint chips
*   uint * core_map
*
* SOURCE
*/
uint get_ncores(uint chips, uint * core_map)
{
  uint i, j;
  uint nc = 0;
  uint cores;

  // count the number of cores
  for (i = 0; i < chips; i++)
  {
    cores = core_map[i];
    /* exclude monitor -- core 0 */
    for (j = 1; j < sv->num_cpus; j++)
    {
      if (cores & (1 << j)) nc++;
    }
  }

  return(nc);
}
/*
*******/


/****f* simple.c/app_init
*
* SUMMARY
*  This function is called at application start-up.
*  It's used to say hello, setup multicast routing table entries,
*  and initialise application variables.
*
* SYNOPSIS
*  void app_init ()
*
* SOURCE
*/
void app_init ()
{
  uint i;

  /* ------------------------------------------------------------------- */
  /* initialise routing entries                                          */
  /* ------------------------------------------------------------------- */
  /* set a MC routing table entry to send my packets back to me */

  spin1_set_mc_table_entry(coreID,                    // entry
                     coreID,                    // key
                     0xffffffff,                // mask
                     (uint) (1 << (coreID+6))   // route
                    );

  /* ------------------------------------------------------------------- */

  /* ------------------------------------------------------------------- */
  /* initialize the application processor resources                      */
  /* ------------------------------------------------------------------- */
  /* say hello */
  io_printf (IO_STD, "[core %d] -----------------------\n", coreID);
  spin1_delay_us (PRINT_DLY);
  io_printf (IO_STD, "[core %d] starting simulation\n", coreID);
  spin1_delay_us (PRINT_DLY);

  /* use a buffer in the middle of system RAM - avoid conflict with RTK */
  sysram_buffer = (uint *) (SPINN_SYSRAM_BASE + SPINN_SYSRAM_SIZE/2
                            + (coreID * BUFFER_SIZE * sizeof(uint)));
  /* use a buffer somewhere in SDRAM */
  sdram_buffer  = (uint *) (SPINN_SDRAM_BASE + SPINN_SDRAM_SIZE/8
                            + (coreID * BUFFER_SIZE * sizeof(uint)));

  /* allocate buffer in DTCM */
  if ((dtcm_buffer = (uint *) spin1_malloc(BUFFER_SIZE*sizeof(uint))) == 0)
  {
    test_DMA = FALSE;
    io_printf (IO_STD, "[core %d] error - cannot allocate dtcm buffer\n", coreID);
    spin1_delay_us (PRINT_DLY);
  }
  else
  {
    test_DMA = TRUE;
    /* initialize sections of DTCM, system RAM and SDRAM */
    for (i=0; i<BUFFER_SIZE; i++)
    {
      dtcm_buffer[i]   = i;
      sysram_buffer[i] = 0xa5a5a5a5;
      sdram_buffer[i]  = 0x5a5a5a5a;
    }
    io_printf (IO_STD, "[core %d] dtcm buffer @ 0x%8z\n", coreID,
               (uint) dtcm_buffer);
    spin1_delay_us (PRINT_DLY);
  }
  /* ------------------------------------------------------------------- */
}
/*
*******/


/****f* simple.c/app_done
*
* SUMMARY
*  This function is called at application exit.
*  It's used to report some statistics and say goodbye.
*
* SYNOPSIS
*  void app_done ()
*
* SOURCE
*/
void app_done ()
{
  /* skew io_printfs to avoid congesting tubotron */
  spin1_delay_us (200 * ((chipID << 5) + coreID));

  /* report simulation time */
  io_printf (IO_STD, "[core %d] simulation lasted %d ticks\n", coreID,
             spin1_get_simulation_time());
  spin1_delay_us (PRINT_DLY);

  /* report number of packets */
  io_printf (IO_STD, "[core %d] received %d packets\n", coreID, packets);
  spin1_delay_us (PRINT_DLY);

  /* report number of failed packets */
  io_printf (IO_STD, "[core %d] failed %d packets\n", coreID, pfailed);
  spin1_delay_us (PRINT_DLY);

  /* report number of failed user events*/
  io_printf (IO_STD, "[core %d] failed %d USER events\n", coreID, ufailed);
  spin1_delay_us (PRINT_DLY);

  /* report number of DMA transfers */
  io_printf (IO_STD, "[core %d] completed %d DMA transfers\n", coreID, transfers);
  spin1_delay_us (PRINT_DLY);

  /* report number of failed transfers */
  io_printf (IO_STD, "[core %d] failed %d DMA transfers\n", coreID, tfailed);
  spin1_delay_us (PRINT_DLY);
  if (tfailed)
  {
    io_printf (IO_STD, "\t%d : %d @ %d\n", tfvald, tfvals, tfaddr);
    spin1_delay_us (PRINT_DLY);
    io_printf (IO_STD, "\t%d : %d @ %d\n", dtcm_buffer[tfaddr],
               sdram_buffer[tfaddr], tfaddr);
    spin1_delay_us (PRINT_DLY);
  }

  /* say goodbye */
  io_printf (IO_STD, "[core %d] stopping simulation\n", coreID);
  spin1_delay_us (PRINT_DLY);
  io_printf (IO_STD, "[core %d] -----------------------\n", coreID);
}
/*
*******/


/****f* simple.c/send_packets
*
* SUMMARY
*  This function is used by a core to send packets to itself.
*  It's used to test the triggering of USER_EVENT
*
* SYNOPSIS
*  void send_packets (uint null_a, uint null_b)
*
* INPUTS
*   uint null_a: padding - all callbacks must have two uint arguments!
*   uint null_b: padding - all callbacks must have two uint arguments!
*
* SOURCE
*/
void send_packets (uint ticks, uint none)
{
  uint i;

  /* send packets */
  for (i=0; i<ITER_TIMES; i++)
  {
    if (!spin1_send_mc_packet(coreID, ticks, 1)) pfailed++;
  }
}
/*
*******/


/****f* simple.c/flip_led
*
* SUMMARY
*  This function is used as a callback for timer tick events.
*  It inverts the state of LED 1 and sends packets.
*
* SYNOPSIS
*  void flip_led (uint ticks, uint null)
*
* INPUTS
*   uint ticks: simulation time (in ticks) - provided by the RTS
*   uint null: padding - all callbacks must have two uint arguments!
*
* SOURCE
*/
void flip_led (uint ticks, uint null)
{
  /* flip led 1 */
  /* Only 1 core should flip leds! */
  if ((get_ncores(NUMBER_OF_CHIPS, core_map) == 1) || (coreID == 1))
  {
    spin1_led_control(LED_INV (1));
  }

  /* trigger user event to send packets */
  if (spin1_trigger_user_event(ticks, NULL) == FAILURE)
  {
    ufailed++;
  }

  /* stop if desired number of ticks reached */
  if (ticks >= TOTAL_TICKS) spin1_stop();
}
/*
*******/


/****f* simple.c/do_transfer
*
* SUMMARY
*  This function is used as a task example
*  It triggers a dma transfers.
*
* SYNOPSIS
*  void do_transfer (uint val, uint none)
*
* INPUTS
*   uint val: argument
*   uint none: padding argument - task must have 2
*
* SOURCE
*/
void do_transfer (uint val, uint none)
{
  if (val == 3)
  {
    spin1_dma_transfer(DMA_WRITE, sysram_buffer, dtcm_buffer, DMA_WRITE,
                       BUFFER_SIZE*sizeof(uint));
  }
  else
  {
    if (val == 5)
    {
      spin1_dma_transfer(DMA_WRITE, sdram_buffer, dtcm_buffer, DMA_WRITE,
                         BUFFER_SIZE*sizeof(uint));
    }
  }
}
/*
*******/


/****f* simple.c/count_packets
*
* SUMMARY
*  This function is used as a callback for packet received events.
*  It counts the number of received packets and triggers dma transfers.
*
* SYNOPSIS
*  void count_packets (uint key, uint payload)
*
* INPUTS
*   uint key: packet routing key - provided by the RTS
*   uint payload: packet payload - provided by the RTS
*
* SOURCE
*/
void count_packets (uint key, uint payload)
{
  /* count packets */
  packets++;

  /* if testing DMA trigger transfer requests at the right time */
  if (test_DMA)
  {
    /* schedule task (with priority 1) to trigger DMA */
    spin1_schedule_callback(do_transfer, payload, 0, 1);
  }
}
/*
*******/


/****f* simple.c/check_memcopy
*
* SUMMARY
*  This function is used as a callback for dma done events.
*  It counts the number of transfers and checks that
*  data was copied correctly.
*
* SYNOPSIS
*  void check_memcopy(uint tid, uint ttag)
*
* INPUTS
*   uint tid: transfer ID - provided by the RTS
*   uint ttag: transfer tag - provided by application
*
* SOURCE
*/
void check_memcopy(uint tid, uint ttag)
{
  uint i;

  /* count transfers */
  transfers++;


  /* check if DMA transfer completed correctly */
  if (tid <= ITER_TIMES)
  {
    for (i=0; i<BUFFER_SIZE; i++)
    {
      if (dtcm_buffer[i] != sysram_buffer[i])
      {
        tfailed++;
        tfaddr = i;
        break;
      }
    }
  }

  if (tid > ITER_TIMES)
  {
    for (i=0; i<BUFFER_SIZE; i++)
    {
      tfvald = dtcm_buffer[i];
      tfvals = sdram_buffer[i];
      if (tfvald != tfvals)
      {
        tfailed++;
        tfaddr = i;
        break;
      }
    }
  }
}
/*
*******/


/****f* simple.c/c_main
*
* SUMMARY
*  This function is called at application start-up.
*  It is used to register event callbacks and begin the simulation.
*
* SYNOPSIS
*  int c_main()
*
* SOURCE
*/
void c_main()
{
  io_printf (IO_STD, ">> simple\n");

  /* get this core's IDs */
  coreID = spin1_get_core_id();
  chipID = spin1_get_chip_id();

  /* TODO: Works on 4-chip board only! */
  /* set the core map for the simulation */
  if (NUMBER_OF_CHIPS > 0) core_map[0] = CORE_MAP_00;
  if (NUMBER_OF_CHIPS > 1) core_map[1] = CORE_MAP_01;
  if (NUMBER_OF_CHIPS > 2) core_map[2] = CORE_MAP_10;
  if (NUMBER_OF_CHIPS > 3) core_map[3] = CORE_MAP_11;
  spin1_set_core_map(NUMBER_OF_CHIPS, core_map);

  /* set timer tick value (in microseconds) */
  spin1_set_timer_tick(TIMER_TICK_PERIOD);

  /* register callbacks */
  spin1_callback_on(MC_PACKET_RECEIVED, count_packets, -1);
  spin1_callback_on(DMA_TRANSFER_DONE, check_memcopy, 0);
  spin1_callback_on(USER_EVENT, send_packets, 2);
  spin1_callback_on(TIMER_TICK, flip_led, 3);

  /* initialize application */
  app_init();

  /* go */
  spin1_start();

  /* report results */
  app_done();
}
/*
*******/
