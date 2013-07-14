/****a* spin1_api.c/spin1_api
*
* SUMMARY
*  SpiNNaker API functions
*
* AUTHOR
*  Thomas Sharp - thomas.sharp@cs.man.ac.uk
*  Luis Plana   - luis.plana@manchester.ac.uk
*
* DETAILS
*  Created on       : 08 June 2011
*  Version          : $Revision: 2011 $
*  Last modified on : $Date: 2012-10-24 14:50:54 +0100 (Wed, 24 Oct 2012) $
*  Last modified by : $Author: plana $
*  $Id: spin1_api.c 2011 2012-10-24 13:50:54Z plana $
*  $HeadURL: https://solem.cs.man.ac.uk/svn/spin1_api/testing/src/spin1_api.c $
*
* COPYRIGHT
*  Copyright (c) The University of Manchester, 2011. All rights reserved.
*  SpiNNaker Project
*  Advanced Processor Technologies Group
*  School of Computer Science
*
*******/

#include "spinnaker.h"
#include "spin1_api.h"
#include "spin1_api_params.h"
#include "spinn_io.h"


// ------------------------------------------------------------------------
// global variables
// ------------------------------------------------------------------------
// --------------------------------
/* virtual and physical core IDs */
// --------------------------------
extern uint virt_cpu;
extern uint phys_cpu;

// ---------------------
/* simulation control */
// ---------------------
static uchar  rootAp;                 // root appl. core has special functions
static uchar  rootChip;               // root chip appl. core has special functions
static ushort rootAddr;               // root appl. core has special functions

static uint   my_chip = 0;            // chip address in core_map coordinates

static volatile uint run = 0;         // controls simulation start/stop
volatile uint ticks = 0;              // number of elapsed timer periods
static volatile uint timer_tick = 0;  // timer tick period
static volatile uint nchips = 1;      // chips in the simulation
static volatile uint ncores = 1;      // application cores in the simulation
static volatile uint my_ncores = 0;   // this chip's appl. cores in the sim.
volatile uint exit_val = NO_ERROR;    // simulation return value
// number of cores that have reached the synchronisation barrier
volatile uint barrier_rdy1_cnt;
volatile uint barrier_rdy2_cnt;
// synchronisation barrier go signals
volatile uint barrier_go = FALSE;
volatile uint barrier_rdygo = FALSE;
// default fiq handler -- restored after simulation
isr_t def_fiq_isr;
int fiq_event = -1;
// ---------------------

// ---------------
/* dma transfer */
// ---------------
static uint dma_id = 1;
dma_queue_t dma_queue;
// ---------------

// -----------------
/* communications */
// -----------------
tx_packet_queue_t tx_packet_queue;
// -----------------

// -----------
/* hardware */
// -----------
/* router MC tables */
static volatile uint * const rtr_mcrte = (uint *) RTR_MCRAM_BASE;
static volatile uint * const rtr_mckey = (uint *) RTR_MCKEY_BASE;
static volatile uint * const rtr_mcmsk = (uint *) RTR_MCMASK_BASE;

/* router P2P tables */
static volatile uint * const rtr_p2p   = (uint *) RTR_P2P_BASE;

/* Number and position of LED bits for various PCB variants */
const ushort S2_LEDS[] = {3, 0x0001, 0x0002, 0x0040}; // S2 Bits 0, 1, 6
const ushort S3_LEDS[] = {2, 0x0001, 0x0020, 0x0000}; // S3 Bits 0, 5
// -----------

// --------------------
/* memory allocation */
// --------------------
// Heap starting address - provided by the linker
#ifdef __GNUC__
extern uint HEAP_BASE;
static void *current_addr = (void *) &HEAP_BASE;
#else
extern uint const Image$$ARM_LIB_STACKHEAP$$ZI$$Base;
static void *current_addr = (void *) &Image$$ARM_LIB_STACKHEAP$$ZI$$Base;
#endif
// --------------------

// -----------------------
/* scheduler/dispatcher */
// -----------------------
static task_queue_t task_queue[NUM_PRIORITIES-1];  // priority <= 0 is non-queueable
cback_t callback[NUM_EVENTS];
volatile uchar user_pending = FALSE;
uint user_arg0;
uint user_arg1;
// -----------------------

// ----------------
/* debug, warning and diagnostics support */
// ----------------
diagnostics_t diagnostics;

#if (API_DEBUG == TRUE) || (API_DIAGNOSTICS == TRUE)
  volatile uint thrown; // keep track of thrown away packets
#endif

#if (API_WARN == TRUE) || (API_DIAGNOSTICS == TRUE)
  static uint warnings;  // report warnings
  static uint fullq;     // keep track of times task queue full
  static uint pfull;     // keep track of times transmit queue full
  static uint dfull;     // keep track of times DMA queue full
  #if USE_WRITE_BUFFER == TRUE
    volatile uint wberrors; // keep track of write buffer errors
  #endif
#endif
// ------------------
// ------------------------------------------------------------------------



// ------------------------------------------------------------------------
// functions
// ------------------------------------------------------------------------
// -----------------------------
/* interrupt service routines */
// -----------------------------
#ifdef __GNUC__
extern void __attribute__ ((interrupt ("IRQ"))) barrier_packet_handler (void);
extern void __attribute__ ((interrupt ("IRQ"))) cc_rx_error_isr (void);
extern void __attribute__ ((interrupt ("IRQ"))) cc_rx_ready_isr (void);
extern void __attribute__ ((interrupt ("IRQ"))) cc_tx_empty_isr (void);
extern void __attribute__ ((interrupt ("IRQ"))) dma_done_isr (void);
extern void __attribute__ ((interrupt ("IRQ"))) dma_error_isr (void);
extern void __attribute__ ((interrupt ("IRQ"))) timer1_isr (void);
extern void __attribute__ ((interrupt ("IRQ"))) sys_controller_isr (void);
extern void __attribute__ ((interrupt ("IRQ"))) soft_int_isr (void);
extern void __attribute__ ((interrupt ("IRQ"))) cc_rx_ready_fiqsr (void);
extern void __attribute__ ((interrupt ("IRQ"))) dma_done_fiqsr (void);
extern void __attribute__ ((interrupt ("IRQ"))) timer1_fiqsr (void);
extern void __attribute__ ((interrupt ("IRQ"))) soft_int_fiqsr (void);
#else
extern __irq void barrier_packet_handler (void);
extern __irq void cc_rx_error_isr (void);
extern __irq void cc_rx_ready_isr (void);
extern __irq void cc_tx_empty_isr (void);
extern __irq void dma_done_isr (void);
extern __irq void dma_error_isr (void);
extern __irq void timer1_isr (void);
extern __irq void sys_controller_isr (void);
extern __irq void soft_int_isr (void);
extern __irq void cc_rx_ready_fiqsr (void);
extern __irq void dma_done_fiqsr (void);
extern __irq void timer1_fiqsr (void);
extern __irq void soft_int_fiqsr (void);
#endif
// ----------------

// ----------------------------
/* intercore synchronisation */
// ----------------------------
static void barrier_setup (uint chips, uint * core_map);
static void barrier_wait (void);

__inline static uint lock_try (uint lock)
{
  return ((sc[SC_TAS0 + lock] & BIT_31) == 0);
}


__inline static void lock_free (uint lock)
{
  (void) sc[SC_TAC0 + lock];
}
// ---------------------

// ----------------
/* data transfer */
// ----------------
// ----------------

// -----------------
/* communications */
// -----------------
void send_mc_packet_fulfil(uint key, uint data, uint load);
// -----------------

// -------------
/* interrupts */
// -------------
uint irq_enable(void);
void wait_for_irq(void);
// -------------

// -----------
/* hardware */
// -----------
static uint p2p_get (uint entry);
static void configure_communications_controller(void);
static void configure_dma_controller(void);
static void configure_timer1(uint time);
static void configure_vic(void);
// -----------

// -----------------------
/* scheduler/dispatcher */
// -----------------------
static void dispatch(void);
static void deschedule(uint event_id);
void schedule_sysmode(uchar event_id, uint arg0, uint arg1);
// -----------------------

// ---------------------
/* rts initialization */
// ---------------------
void rts_init (void);
// ---------------------


// ------------------------------------------------------------------------
// simulation control and event management functions
// ------------------------------------------------------------------------
/****f* spin1_api.c/spin1_callback_on
*
* SUMMARY
*  This function sets the given callback to be scheduled on occurrence of the
*  specified event. The priority argument dictates the order in which
*  callbacks are executed by the scheduler.
*
* SYNOPSIS
*  void spin1_callback_on(uchar event_id, callback_t cback, int priority)
*
* INPUTS
*  uint event_id: event for which callback should be enabled
*  callback_t cback: callback function
*  int priority:   0 = non-queueable callback (associated to irq)
*                > 0 = queueable callback
*                < 0 = preeminent callback (associated to fiq)
*
* SOURCE
*/
void spin1_callback_on(uint event_id, callback_t cback, int priority)
{
  callback[event_id].cback = cback;
  if (priority < 0)
  {
    if ((fiq_event == -1) | (fiq_event == event_id))
    {
      callback[event_id].priority = priority;
      fiq_event = event_id;
    }
    else
    {
      // only one callback can have priority -1 -- demote to priority 0
      callback[event_id].priority = 0;
      #if API_WARN == TRUE
        io_printf (IO_STD, "\t\t[api_warn] warning: too many FIQ events\n");
      #endif
    }
  }
  else
  {
    callback[event_id].priority = priority;
  }
}
/*
*******/



/****f* spin1_api.c/spin1_callback_off
*
* SUMMARY
*  This function disables the callback for the specified event.
*
* SYNOPSIS
*  void spin1_callback_off(uint event_id)
*
* INPUTS
*  uint event_id: event for which callback should be disabled
*
* SOURCE
*/
void spin1_callback_off(uint event_id)
{
  callback[event_id].cback = NULL;
  if (callback[event_id].priority < 0)
  {
    fiq_event = -1;
  }

}
/*
*******/



/****f* spin1_api.c/spin1_get_simulation_time
*
* SUMMARY
*  This function returns the number of timer periods which have elapsed since
*  the beginning of the simulation.
*
* SYNOPSIS
*  uint spin1_get_simulation_time()
*
* OUTPUTS
*  Timer ticks since beginning of simulation.
*
* SOURCE
*/
uint spin1_get_simulation_time()
{
  return ticks;
}
/*
*******/

/****f* spin1_api.c/spin1_get_us_since_last_tick(void)
*
* SUMMARY
*  This function returns the number of microseconds which have elapsed since
*  the last tick registered by the simulation time.
*
* SYNOPSIS
*  uint spin1_get_us_since_last_tick()
*
* OUTPUTS
*  microseconds since last simulation tick.
*
* SOURCE
*/
uint spin1_get_us_since_last_tick() {
  return (tc[T1_LOAD] - tc[T1_COUNT]) / sv->cpu_clk;
}


/****f* spin1_api.c/spin1_kill
*
* SUMMARY
*  This function dirtily terminates a simulation. It is to be called if an
*  error condition arises.
*
* SYNOPSIS
*  void spin1_kill(uint error)
*
* INPUTS
*  uint error: error exit code
*
* SOURCE
*/
void spin1_kill(uint error)
{
  // disable api-enabled interrupts to allow simulation to stop,
  vic[VIC_DISABLE] = (1 << CC_RDY_INT)   |
                     (1 << TIMER1_INT)   |
                     (1 << SOFTWARE_INT) |
                     (1 << DMA_ERR_INT)  |
                     (1 << DMA_DONE_INT);

  // Switch on red LED,
  //TODO: not all boards have red LEDs
  spin1_led_control (LED_ON (1));

  // report back the cause of the problem,
  exit_val = error;

  // and stop the simulation
  run = 0;
}
/*
*******/



/****f* spin1_api.c/spin1_set_timer_tick
*
* SUMMARY
*  This function sets the period of the timer tick
*
* SYNOPSIS
*  void set_timer_tick(uint time)
*
* INPUTS
*  uint time: timer tick period (in microseconds)
*
* SOURCE
*/
void spin1_set_timer_tick(uint time)
{
  timer_tick = time;
}
/*
*******/



/****f* spin1_api.c/spin1_set_core_map
*
* SUMMARY
*  This function sets the number of cores in the simulation
*
* SYNOPSIS
*  void spin1_set_core_map(uint chips, uint * core_map)
*
* INPUTS
*  uint chips: number of chips in the simulation
*  uint core_map: bit map of cores involved in the simulation
*
* SOURCE
*/
void spin1_set_core_map(uint chips, uint * core_map)
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
  ncores = nc;
  #if API_DEBUG == TRUE
    if ((NON_ROOT) || rootAp)
    {
      io_printf (IO_STD, "\t\t[api_debug] Number of cores: %d\n", ncores);
      spin1_delay_us (API_PRINT_DLY);
    }
  #endif

  // if needed, setup mc routing table to implement barrier synchonisation
  // only the root application core does the setup
  if ((ncores > 1) && (rootAp))
  {
    barrier_setup(chips, core_map);
  }
}
/*
*******/



/****f* spin1_api.c/clean_up
*
* SUMMARY
*  This function is called after simulation stops to configure
*  hardware for idle operation. It cleans up interrupt lines.
*
* SYNOPSIS
*  void clean_up ()
*
* SOURCE
*/
void clean_up ()
{
  uint i;

  uint cpsr = spin1_int_disable ();

  // select all interrupts as irq
  vic[VIC_SELECT] = vic[VIC_SELECT] &
                    ~((1 << TIMER1_INT)   |
                      (1 << CC_RDY_INT)   |
                      (1 << SOFTWARE_INT) |
                      (1 << DMA_ERR_INT)  |
                      (1 << DMA_DONE_INT)
                     );

  // restore default fiq_isr handler
  exception_table[7] = def_fiq_isr;

  // install default packet received interrupt vector in VIC
  // to get rid of any remaining packets in CC & CNoC buffers!
  vic_vectors[RX_READY_PRIORITY] = cc_rx_ready_isr;

  // de-register callbacks
  for (i = 0; i < NUM_EVENTS; i++)
  {
    callback[i].cback = NULL;
  }

  // timer1
  tc[T1_INT_CLR] = 1;   // clear possible interrupt

  // dma controller
  dma[DMA_GCTL] = 0;    // disable all IRQ sources
  dma[DMA_CTRL] = 0x3f; // Abort pending and active transfers
  dma[DMA_CTRL] = 0x0d; // clear possible transfer done and restart

  spin1_mode_restore (cpsr);
}
/*
*******/



/****f* spin1_api.c/report_debug
*
* SUMMARY
*  This function reports warnings if requested
*  at compile time
*
* SYNOPSIS
*  void report_debug ()
*
* SOURCE
*/
void report_debug ()
{
  #if API_DEBUG == TRUE
    // if more than 1 core only the root appl. core reports router data
    if ((ncores == 1) || (rootAp))
    {
      // report router counters
//      io_printf (IO_STD, "\t\t[api_debug] RTR total  packets: %d\n",
//                 rtr[RTR_DGC0]);
//      spin1_delay_us (API_PRINT_DLY);
      io_printf (IO_STD, "\t\t[api_debug] RTR mc     packets: %d\n",
                 rtr[RTR_DGC1]);
      spin1_delay_us (API_PRINT_DLY);
//      io_printf (IO_STD, "\t\t[api_debug] RTR p2p    packets: %d\n",
//                 rtr[RTR_DGC2]);
//      spin1_delay_us (API_PRINT_DLY);
//      io_printf (IO_STD, "\t\t[api_debug] RTR nn     packets: %d\n",
//                 rtr[RTR_DGC3]);
//      spin1_delay_us (API_PRINT_DLY);
//      io_printf (IO_STD, "\t\t[api_debug] RTR dumped packets: %d\n",
//                 rtr[RTR_DGC4]);
//      spin1_delay_us (API_PRINT_DLY);
      io_printf (IO_STD, "\t\t[api_debug] RTR dpd mc packets: %d\n",
                 rtr[RTR_DGC5]);
      spin1_delay_us (API_PRINT_DLY);
    }
    if ((NON_ROOT) || rootAp)
    {
      io_printf (IO_STD, "\t\t[api_debug] ISR thrown packets: %d\n", thrown);
      spin1_delay_us (API_PRINT_DLY);

      // report dmac counters
      io_printf (IO_STD, "\t\t[api_debug] DMA bursts:  %d\n",
                 dma[DMA_STAT0]);
      spin1_delay_us (API_PRINT_DLY);
    }
  #endif
}
/*
*******/



/****f* spin1_api.c/report_warns
*
* SUMMARY
*  This function reports warnings if requested
*  at compile time
*
* SYNOPSIS
*  void report_warns ()
*
* SOURCE
*/
void report_warns ()
{
  #if API_WARN == TRUE
    // report warnings
    if (warnings & TASK_QUEUE_FULL)
    {
      io_printf (IO_STD, "\t\t[api_warn] warning: task queue full (%u)\n",
                 fullq);
      spin1_delay_us (API_PRINT_DLY);
    }
    if (warnings & DMA_QUEUE_FULL)
    {
      io_printf (IO_STD, "\t\t[api_warn] warning: DMA queue full (%u)\n",
                 dfull);
      spin1_delay_us (API_PRINT_DLY);
    }
    if (warnings & PACKET_QUEUE_FULL)
    {
      io_printf (IO_STD, "\t\t[api_warn] warning: packet queue full (%u)\n",
                 pfull);
      spin1_delay_us (API_PRINT_DLY);
    }
    # if USE_WRITE_BUFFER == TRUE
      if (warnings & WRITE_BUFFER_ERROR)
      {
        io_printf (IO_STD,
                   "\t\t[api_warn] warning: write buffer errors (%u)\n",
                   wberrors);
        spin1_delay_us (API_PRINT_DLY);
      }
    #endif
  #endif
}
/*
*******/



/****f* spin1_api.c/spin1_start
*
* SUMMARY
*  This function begins a simulation by enabling the timer (if called for) and
*  beginning the dispatcher loop.
*
* SYNOPSIS
*  void spin1_start()
*
* SOURCE
*/
uint spin1_start()
{
  // initialise hardware
  configure_communications_controller();
  configure_dma_controller();
  configure_timer1(timer_tick);
  configure_vic();

  #if (API_WARN == TRUE) || (API_DIAGNOSTICS == TRUE)
    warnings = NO_ERROR;
    dfull = 0;
    fullq = 0;
    pfull = 0;
    #if USE_WRITE_BUFFER == TRUE
      wberrors = 0;
    #endif
  #endif

  // synchronise with other application cores
  if (ncores > 1)
  {
    barrier_wait();
  }
  else
  {
    if(callback[MC_PACKET_RECEIVED].priority < 0)
    {
      // select packet received interrupt as fiq
      vic[VIC_SELECT] = vic[VIC_SELECT] | (1 << CC_RDY_INT);
    }
    else
    {
      // install handler vector in VIC
      vic_vectors[RX_READY_PRIORITY] = cc_rx_ready_isr;
    }
  }

  // initialise counter and ticks for simulation
  if (timer_tick)
  {
    // 32-bit, periodic counter, interrupts enabled
    tc[T1_CONTROL] = 0xe2;
  }
  ticks = 0;
  run = 1;

  // simulate!
  dispatch ();

  // simulation finished - clean up before returning to c_main
  clean_up ();

  // re-enable interrupts for sark
  // only CPU_INT enabled in the VIC
  irq_enable ();

  // provide diagnostics data to application
  #if (API_DIAGNOSTICS == TRUE)
    diagnostics.exit_code            = exit_val;
    diagnostics.warnings             = warnings;
    diagnostics.total_mc_packets     = rtr[RTR_DGC1];
    diagnostics.dumped_mc_packets    = rtr[RTR_DGC5];
    diagnostics.discarded_mc_packets = thrown;
    diagnostics.dma_transfers        = dma_id - 1;
    diagnostics.dma_bursts           = dma[DMA_STAT0];
    diagnostics.dma_queue_full       = dfull;
    diagnostics.task_queue_full      = fullq;
    diagnostics.tx_packet_queue_full = pfull;
    #if USE_WRITE_BUFFER == TRUE
      diagnostics.writeBack_errors     = wberrors;
    #endif
  #endif

  // report problems if requested!
  #if (API_DEBUG == TRUE) || (API_WARN == TRUE)
    // avoid sending output at the same time as other chips!
    spin1_delay_us(10000 * my_chip);

    #if API_DEBUG == TRUE
      // report debug information
      report_debug();
    #endif

    #if API_WARN == TRUE
      // report warnings
      report_warns ();
    #endif
  #endif

  return exit_val;
}
/*
*******/



/****f* spin1_api.c/spin1_stop
*
* SUMMARY
*  This function terminates a simulation by setting the dispatcher control
*  variable. It is possible that the dispatcher will enter wait for interrupt
*  state immediately after a call to this function, in which case the dispatch
*  loop will not terminate until occurrance of the next event.
*
* SYNOPSIS
*  void spin1_stop()
*
* SOURCE
*/
void spin1_stop()
{
  // disable api-enabled interrupts to allow simulation to stop,
  vic[VIC_DISABLE] = (1 << CC_RDY_INT)   |
                     (1 << TIMER1_INT)   |
                     (1 << SOFTWARE_INT) |
                     (1 << DMA_ERR_INT)  |
                     (1 << DMA_DONE_INT);


  // report back clean exit,
  exit_val = NO_ERROR;

  // and stop the simulation
  run = 0;
}
/*
*******/



/****f* spin1_api.c/spin1_delay_us
*
* SUMMARY
*  This function implements a delay measured in microseconds
*  The function busy waits to implement the delay.
*
* SYNOPSIS
*  void spin1_delay_us(uint n)
*
* INPUTS
*  uint n: requested delay (in microseconds)
*
* SOURCE
*/
void spin1_delay_us (uint n)
{
  n = (n * sv->cpu_clk) / 4;

  while (n--)
    continue;
}
/*
*******/



/****f* spin1_api.c/barrier_setup
*
* SUMMARY
*
* SYNOPSIS
*  void barrier_setup(void)
*
* SOURCE
*/
void barrier_setup (uint chips, uint * core_map)
{
  uint i;
  uint my_cores = 0;

  // TODO: needs extending -- works for square core maps only!
  uint bsize = 0;
  uint bside = 1;
  for (i = 0; i < 32; i++)
  {
    if ((chips & (0x80000000 >> i)) != 0)
    {
      bsize = (31 - i) >> 1;
      bside = 1 << bsize;
      break;
    }
  }

  #if API_DEBUG == TRUE
    io_printf (IO_STD, "\t\t[api_debug] bside: %d, bsize: %d\n",
               bside, bsize);
    spin1_delay_us (API_PRINT_DLY);
  #endif

  // TODO: needs extending -- works for square core maps only!
  uint my_x = sv->p2p_addr >> 8;
  uint my_y = sv->p2p_addr & 0xff;
  my_chip = (my_x << bsize) | my_y;

  // check if root chip
  rootChip = (sv->p2p_addr == CHIP_ADDR(0, 0));
  rootAddr = CHIP_ADDR(0, 0);

  // setup routing entries for synchronization barrier
  uint loc_route = 0;
  uint off_route = 0;
  uint rdygo_route;
  uint rdy2_route;

  // setup the local (on-chip) routes
  my_cores = core_map[my_chip] & 0xfffffffe;  // exclude monitor
  // need to do virtual to physical core ID conversion
  for (uint v = 1; v < sv->num_cpus; v++)  // go through virt_cpus
  {
    if (my_cores & (1 << v))
    {
      loc_route |= CORE_ROUTE(sv->v2p_map[v]);
      my_ncores++;
    }
  }
  #if API_DEBUG == TRUE
    io_printf (IO_STD, "\t\t[api_debug] chip: %d, core map: 0x%x\n",
               my_chip, my_cores);
    spin1_delay_us (API_PRINT_DLY);
  #endif

  // TODO: needs fixing -- fault-tolerant tree
  // setup off-chip routes -- check for borders!
  // north
  if ((my_x == 0) && (my_y < (bside - 1)) &&
      ((core_map[my_chip + 1] & 0xfffffffe) != 0))
  {
    off_route |= (1 << NORTH); // add link to north chip
  }

  // east
  if ((my_x < (bside - 1)) && (my_y == 0) &&
      ((core_map[my_chip + bside] & 0xfffffffe) != 0))
  {
    off_route |= (1 << EAST); // add link to east chip
  }

  // north-east
  if ((my_y < (bside - 1)) && (my_x < (bside - 1)) &&
      ((core_map[my_chip + bside + 1] & 0xfffffffe) != 0))
  {
    off_route |= (1 << NORTH_EAST); // add link to north-east chip
  }

  // TODO: needs fixing -- non-fault-tolerant tree
  // TODO: doesn't use wrap around
  // spanning tree from root chip N, E & NE
  if (rootChip)
  {
    // compute number of active chips
    for (i = 0; i < chips; i++)
    {
      if ((i != my_chip) && ((core_map[i] & 0xfffffffe) != 0))
      {
        nchips++;
      }
    }

    // setup the RDYGO entry -- only off-chip routes!
    rdygo_route = off_route;

    // setup the RDY2 route -- packets come to me
    rdy2_route = CORE_ROUTE(phys_cpu);
  }
  else
  {
    // setup the RDYGO entry -- packets to me and forward
    rdygo_route = CORE_ROUTE(phys_cpu) | off_route;

    // setup the RDY2 route -- packets go to root chip
    // use p2p routing table to find the way
    rdy2_route = P2P_ROUTE(rootAddr);
  }

  // setup the RDYGO entry
  rtr_mckey[1] = RDYGO_KEY;
  rtr_mcmsk[1] = BARRIER_MASK;
  rtr_mcrte[1] = rdygo_route;

  // setup the RDY2 entry
  rtr_mckey[3] = RDY2_KEY;
  rtr_mcmsk[3] = BARRIER_MASK;
  rtr_mcrte[3] = rdy2_route;

  // setup the GO entry
  rtr_mckey[0] = GO_KEY;
  rtr_mcmsk[0] = BARRIER_MASK;
  rtr_mcrte[0] = loc_route | off_route;

  #if API_DEBUG == TRUE
    io_printf (IO_STD, "\t\t[api_debug] RDYGO: k 0x%8z m 0x%8z r 0x%8z\n",
               RDYGO_KEY, BARRIER_MASK, rdygo_route);
    spin1_delay_us (API_PRINT_DLY);

    io_printf (IO_STD, "\t\t[api_debug] RDY2: k 0x%8z m 0x%8z r 0x%8z\n",
               RDY2_KEY, BARRIER_MASK, rdy2_route);
    spin1_delay_us (API_PRINT_DLY);

    io_printf (IO_STD, "\t\t[api_debug] GO: k 0x%8z m 0x%8z r 0x%8z\n",
               GO_KEY, BARRIER_MASK, loc_route | off_route);
  #endif
}
/*
*******/



/****f* spin1_api.c/barrier_wait
*
* SUMMARY
*
* SYNOPSIS
*  void barrier_wait(uint n)
*
* INPUTS
*  uint n: number of cores that synchronise
*
* SOURCE
*/
void barrier_wait (void)
{
  uint start;
  uint resend;
  volatile uint *ms_cnt = &sv->clock_ms;

  if (rootAp)
  {
    // let other cores go!
    sv->lock = RDYGO_LCK;

    if (rootChip)
    {
      // root node, root application core
      // send rdygo packet
      spin1_send_mc_packet(RDYGO_KEY, 0, NO_PAYLOAD);

      // wait until all ready packets arrive and give the go signal
      // timeout if taking too long!
      start = *ms_cnt;          // initial value
      resend = start;           // may need to resend rdygo
      while (((*ms_cnt - start) < (BARRIER_RDY2_WAIT))
             && ((barrier_rdy1_cnt < my_ncores)
                 || (barrier_rdy2_cnt < nchips)
                )
            )
      {
        if ((*ms_cnt - resend) > BARRIER_RESEND_WAIT)
        {
          // send a new rdygo packet -- just in case the first was missed!
          spin1_send_mc_packet(RDYGO_KEY, 0, NO_PAYLOAD);
          resend = *ms_cnt;
        }
      }
      #if API_WARN == TRUE
        if ((barrier_rdy1_cnt != my_ncores)
            || (barrier_rdy2_cnt != nchips)
           )
        {
          warnings |= SYNCHRO_ERROR;
          io_printf (IO_STD, "\t\t[api_warn] warning: failed to synchronise (%d/%d) (%d/%d).\n",
                             barrier_rdy1_cnt, my_ncores, barrier_rdy2_cnt, nchips);
          spin1_delay_us (API_PRINT_DLY);
        }
      #endif

      // send go packet
      spin1_send_mc_packet(GO_KEY, 0, NO_PAYLOAD);
    }
    else
    {
      // non-root node, root application core
      // wait until the rdygo packet and all local ready packets arrive
      // timeout if taking too long!
      start = *ms_cnt;          // initial value
      while (((*ms_cnt - start) < (BARRIER_RDYGO_WAIT))
             && ((barrier_rdy1_cnt < my_ncores)
                 || (!barrier_rdygo)
                )
            )
      {
        continue;
      }
      #if API_WARN == TRUE
        if (barrier_rdy1_cnt != my_ncores)
        {
          warnings |= SYNCHRO_ERROR;
          io_printf (IO_STD, "\t\t[api_warn] warning: failed to synchronise (%d/%d).\n",
                             barrier_rdy1_cnt, my_ncores);
          spin1_delay_us (API_PRINT_DLY);
        }
        else if (!barrier_rdygo)
        {
          warnings |= SYNCHRO_ERROR;
          io_printf (IO_STD, "\t\t[api_warn] warning: synchronisation failed (rdygo).\n");
          spin1_delay_us (API_PRINT_DLY);
        }
      #endif

      // send rdy2 packet
      spin1_send_mc_packet(RDY2_KEY, 0, NO_PAYLOAD);
      #if API_DEBUG == TRUE
        io_printf (IO_STD, "\t\t[api_debug] Sending rdy2...\n");
      #endif
    }
  }
  else
  {
    // all others
    // wait for lock -- from local root application core
    // timeout if taking too long!
    start = *ms_cnt;          // initial value
    while (((*ms_cnt - start) < BARRIER_LOCK_WAIT) && (sv->lock < RDYGO_LCK))
    {
      continue;
    }

    #if API_WARN == TRUE
      if (sv->lock < RDYGO_LCK)
      {
        warnings |= SYNCHRO_ERROR;
        io_printf (IO_STD, "\t\t[api_warn] warning: synchronisation failed (lock).\n");
        spin1_delay_us (API_PRINT_DLY);
      }
    #endif

    // send local ready packet
    #if API_DEBUG == TRUE
      if (NON_ROOT)
      {
        io_printf (IO_STD, "\t\t[api_debug] Sending ready...\n");
      }
    #endif
    spin1_send_mc_packet(RDY1_KEY, 0, NO_PAYLOAD);
  }

  // wait until go packet arrives
  // timeout if taking too long!
  start = *ms_cnt;          // initial value
  while (((*ms_cnt - start) < BARRIER_GO_WAIT) && (barrier_go == FALSE))
  {
    continue;
  }
  #if API_WARN == TRUE
    if (barrier_go == FALSE)
    {
      warnings |= SYNCHRO_ERROR;
      io_printf (IO_STD, "\t\t[api_warn] warning: synchronisation timeout (go).\n");
      spin1_delay_us (API_PRINT_DLY);
    }
  #endif
}
/*
*******/



// ------------------------------------------------------------------------
// data transfer functions
// ------------------------------------------------------------------------
/****f* spin1_api.c/spin1_dma_transfer
*
* SUMMARY
*  This function enqueues a DMA transfer request. Requests are consumed by
*  dma_done_isr, which schedules a user callback with the ID of the completed
*  transfer and fulfils the next transfer request. If the DMA controller
*  hardware buffer is not full (which also implies that the request queue is
*  empty, given the consumer operation) then a transfer request is fulfilled
*  immediately.
*
* SYNOPSIS
*  uint spin1_dma_transfer(uint tag, void *system_address, void *tcm_address,
*                          uint direction, uint length)
*
* INPUTS
*  uint *system_address: system NOC address of the transfer
*  uint *tcm_address: processor TCM address of the transfer
*  uint direction: 0 = transfer to TCM, 1 = transfer to system
*  uint length: length of transfer in bytes
*
* OUTPUTS
*   uint: 0 if the request queue is full, DMA transfer ID otherwise
*
* SOURCE
*/
uint spin1_dma_transfer(uint tag, void *system_address, void *tcm_address,
                  uint direction, uint length)
{
  uint id = 0;
  uint desc;

  uint cpsr = spin1_int_disable();

  uint new_end = (dma_queue.end + 1) % DMA_QUEUE_SIZE;

  if(new_end != dma_queue.start)
  {
    id = dma_id++;

    desc =   DMA_WIDTH << 24 | DMA_BURST_SIZE << 21
           | direction << 19 | length;

    dma_queue.queue[dma_queue.end].id = id;
    dma_queue.queue[dma_queue.end].tag = tag;
    dma_queue.queue[dma_queue.end].system_address = system_address;
    dma_queue.queue[dma_queue.end].tcm_address = tcm_address;
    dma_queue.queue[dma_queue.end].description = desc;

    /* if dmac is available and dma_queue empty trigger transfer now */
    if(!(dma[DMA_STAT] & 4) && (dma_queue.start == dma_queue.end))
    {
      dma[DMA_ADRS] = (uint) system_address;
      dma[DMA_ADRT] = (uint) tcm_address;
      dma[DMA_DESC] = desc;
    }

    dma_queue.end = new_end;
  }
  else
  {
    #if (API_WARN == TRUE) || (API_DIAGNOSTICS == TRUE)
      warnings |= DMA_QUEUE_FULL;
      dfull++;
    #endif
  }

  spin1_mode_restore(cpsr);

  return id;
}
/*
*******/



/****f* spin1_api.c/spin1_memcpy
*
* SUMMARY
*  This function copies a block of memory
*
* SYNOPSIS
*  void spin1_memcpy(void *dst, void const *src, uint len)
*
* INPUTS
*  void *dst: destination of the transfer
*  void const *src: source of the transfer
*  uint len: number of bytes to transfer
*
* SOURCE
*/
void spin1_memcpy(void *dst, void const *src, uint len)
{
    char *pDst = (char *) dst;
    char const *pSrc = (char const *) src;

    while (len--)
    {
        *pDst++ = *pSrc++;
    }
}
/*
*******/
// ------------------------------------------------------------------------



// ------------------------------------------------------------------------
// communications functions
// ------------------------------------------------------------------------
/****f* spin1_api.c/spin1_flush_rx_packet_queue
*
* SUMMARY
*  This function effectively discards all received packets which are yet
*  to be processed by calling deschedule(MC_PACKET_RECEIVED).
*
* SYNOPSIS
*  void spin1_flush_rx_packet_queue()
*
* SOURCE
*/
void spin1_flush_rx_packet_queue()
{
  deschedule(MC_PACKET_RECEIVED);
}
/*
*******/



/****f* spin1_api.c/spin1_flush_tx_packet_queue
*
* SUMMARY
*  This function flushes the outbound packet queue by adjusting the
*  queue pointers to make it appear empty to the consumer.
*
* SYNOPSIS
*  void spin1_flush_tx_packet_queue()
*
* SOURCE
*/
void spin1_flush_tx_packet_queue()
{
  uint cpsr = spin1_irq_disable();

  tx_packet_queue.start = tx_packet_queue.end;

  spin1_mode_restore(cpsr);
}
/*
*******/



/****f* spin1_api.c/spin1_send_mc_packet
*
* SUMMARY
*  This function enqueues a request to send a multicast packet. If
*  the software buffer is full then a failure code is returned. If the comms
*  controller hardware buffer and the software buffer are empty then the
*  the packet is sent immediately, otherwise it is placed in a queue to be
*  consumed later by cc_tx_empty interrupt service routine.
*
* SYNOPSIS
*  uint spin1_send_mc_packet(uint key, uint data, uint load)
*
* INPUTS
*  uint key: packet routining key
*  uint data: packet payload
*  uint load: 0 = no payload (ignore data param), 1 = send payload
*
* OUTPUTS
*  1 if packet is enqueued or sent successfully, 0 otherwise
*
* SOURCE
*/
uint spin1_send_mc_packet(uint key, uint data, uint load)
{
  // TODO: This need to be re-written for SpiNNaker using the
  // TX_nof_full flag instead -- much more efficient!

  uint rc = SUCCESS;

  uint cpsr = spin1_irq_disable();

  /* clear sticky TX full bit and check TX state */
  cc[CC_TCR] = TX_TCR_MCDEFAULT;

  if (cc[CC_TCR] & TX_FULL_MASK)
  {
    if((tx_packet_queue.end + 1) % TX_PACKET_QUEUE_SIZE == tx_packet_queue.start)
    {
      /* if queue full cannot do anything -- report failure */
      rc = FAILURE;
      #if (API_WARN == TRUE) || (API_DIAGNOSTICS == TRUE)
        warnings |= PACKET_QUEUE_FULL;
        pfull++;
      #endif
    }
    else
    {
      /* if not full queue packet */
      tx_packet_queue.queue[tx_packet_queue.end].key = key;
      tx_packet_queue.queue[tx_packet_queue.end].data = data;
      tx_packet_queue.queue[tx_packet_queue.end].load = load;

      tx_packet_queue.end = (tx_packet_queue.end + 1) % TX_PACKET_QUEUE_SIZE;

      /* turn on tx_empty interrupt (in case it was off) */
      vic[VIC_ENABLE] = (1 << CC_TMT_INT);

    }
  }
  else
  {
    if((tx_packet_queue.end + 1) % TX_PACKET_QUEUE_SIZE == tx_packet_queue.start)
    {
      /* if queue full, dequeue and send packet at the */
      /* head of the queue to make room for new packet */
      uint hkey  = tx_packet_queue.queue[tx_packet_queue.start].key;
      uint hdata = tx_packet_queue.queue[tx_packet_queue.start].data;
      uint hload = tx_packet_queue.queue[tx_packet_queue.start].load;

      tx_packet_queue.start = (tx_packet_queue.start + 1) % TX_PACKET_QUEUE_SIZE;

      send_mc_packet_fulfil(hkey, hdata, hload);
    }

    if(tx_packet_queue.start == tx_packet_queue.end)
    {
      /* if queue empty send packet */
      send_mc_packet_fulfil(key, data, load);
      /* turn off tx_empty interrupt (in case it was on) */
      vic[VIC_DISABLE] = 0x1 << CC_TMT_INT;
    }
    else
    {
      /* if not empty queue packet */
      tx_packet_queue.queue[tx_packet_queue.end].key = key;
      tx_packet_queue.queue[tx_packet_queue.end].data = data;
      tx_packet_queue.queue[tx_packet_queue.end].load = load;

      tx_packet_queue.end = (tx_packet_queue.end + 1) % TX_PACKET_QUEUE_SIZE;
    }

  }

  spin1_mode_restore(cpsr);

  return rc;
}
/*
*******/



/****f* spin1_api.c/send_mc_packet_fulfil
*
* SUMMARY
*  This function writes a request to send a multicast packet to the comms
*  controller hardware.
*
*  Interrupts are necessarily disabled to prevent contention for the hardware.
*
* SYNOPSIS
*  void send_mc_packet_fulfil(uint key, uint data, uint load)
*
* INPUTS
*  uint key: packet routining key
*  uint data: packet payload
*  uint load: 0 = no payload (ignore data param), 1 = send payload
*
* SOURCE
*/
void send_mc_packet_fulfil(uint key, uint data, uint load)
{
  // TODO: may need to uncomment if non-mc packets are sent!
  //cc[CC_TCR]    = 0x0;

  // payload?
  if (load) cc[CC_TXDATA] = data;

  cc[CC_TXKEY]  = key;
}
/*
*******/
// ------------------------------------------------------------------------


/****f* spin1_api.c/spin1_irq_disable
*
* SUMMARY
*  This function sets the I bit in the CPSR in order to disable IRQ
*  interrupts to the processor.
*
* SYNOPSIS
*  uint spin1_irq_disable()
*
* OUTPUTS
*  state of the CPSR before the interrupt disable
*
* SOURCE
*/
#ifdef THUMB
extern uint spin1_irq_disable (void);
#else
#ifdef __GNUC__
__inline uint spin1_irq_disable (void)
{
  uint old, new;

  asm volatile (
    "mrs	%[old], cpsr \n\
     orr	%[new], %[old], #0x80 \n\
     msr	cpsr_c, %[new] \n"
     : [old] "=r" (old), [new] "=r" (new)
     : 
     : );

  return old;
}
#else
__forceinline uint spin1_irq_disable (void)
{
  uint old, new;

  __asm { mrs old, cpsr }
  __asm { orr new, old, 0x80 }
  __asm { msr cpsr_c, new }

  return old;
}
#endif
#endif
/*
*******/



/****f* spin1_api.c/spin1_mode_restore
*
* SUMMARY
*  This function sets the CPSR to the value given in parameter sr, in order to
*  restore the CPSR following a call to spin1_irq_disable.
* 
* SYNOPSIS
*  void spin1_mode_restore(uint sr)
* 
* INPUTS
*  uint sr: value with which to set the CPSR
*
* SOURCE
*/
#ifdef THUMB
extern void spin1_mode_restore (uint cpsr);
#else
#ifdef __GNUC__
__inline void spin1_mode_restore (uint cpsr)
{
  asm volatile (
    "msr	cpsr_c, %[cpsr]"
    :
    : [cpsr] "r" (cpsr)
    :);
}
#else
__forceinline void spin1_mode_restore (uint sr)
{
  __asm { msr cpsr_c, sr }
}
#endif
#endif
/*
*******/


/****f* spin1_api.c/irq_enable
*
* SUMMARY
*  This function clears the I bit in the CPSR in order to enable IRQ
*  interrupts to the processor.
*
* SYNOPSIS
*  uint irq_enable()
*
* OUTPUTS
*  state of the CPSR before the interrupt enable
*
* SOURCE
*/
#ifdef THUMB
extern uint irq_enable (void);
#else
#ifdef __GNUC__
__inline uint irq_enable (void)
{
  uint old, new;

  asm volatile (
    "mrs	%[old], cpsr \n\
     bic	%[new], %[old], #0x80 \n\
     msr	cpsr_c, %[new] \n"
     : [old] "=r" (old), [new] "=r" (new)
     : 
     : );

  return old;
}
#else
__forceinline uint irq_enable (void)
{
  uint old, new;

  __asm { mrs old, cpsr }
  __asm { bic new, old, 0x80 }
  __asm { msr cpsr_c, new }

  return old;
}
#endif
#endif
/*
*******/



/****f* spin1_api.c/wait_for_irq
*
* SUMMARY
*  This function puts the processor to into wait-for-interrupt state which
*  is exited upon an interrupt request.
* 
* SYNOPSIS
*  void wait_for_irq()
*
* SOURCE
*/
#ifdef THUMB
extern void wait_for_irq (void);
#else
#ifdef __GNUC__
__inline void wait_for_irq()
{
  asm volatile ("mcr p15, 0, r0, c7, c0, 4");
}
#else
__asm void wait_for_irq ()
{
  code32
  mcr	p15, 0, r0, c7, c0, 4
  bx    lr
}
#endif
#endif
/*
*******/


/****f* spin1_api.c/spin1_fiq_disable
*
* SUMMARY
*  This function sets the F bit in the CPSR in order to disable
*  FIQ interrupts in the processor.
*
* SYNOPSIS
*  uint spin1_fiq_disable()
*
* OUTPUTS
*  state of the CPSR before the interrupts disable
*
* SOURCE
*/
#ifdef THUMB
extern uint spin1_fiq_disable (void);
#else
#ifdef __GNUC__
__inline uint spin1_fiq_disable (void)
{
  uint old, new;

  asm volatile (
    "mrs	%[old], cpsr \n\
     orr	%[new], %[old], #0x40 \n\
     msr	cpsr_c, %[new] \n"
     : [old] "=r" (old), [new] "=r" (new)
     : 
     : );

  return old;
}
#else
__forceinline uint spin1_fiq_disable (void)
{
  uint old, new;

  __asm { mrs old, cpsr }
  __asm { orr new, old, 0x40 }
  __asm { msr cpsr_c, new }

  return old;
}
#endif
#endif
/*
*******/



/****f* spin1_api.c/spin1_int_disable
*
* SUMMARY
*  This function sets the F and I bits in the CPSR in order to disable
*  FIQ and IRQ interrupts in the processor.
*
* SYNOPSIS
*  uint spin1_int_disable()
*
* OUTPUTS
*  state of the CPSR before the interrupts disable
*
* SOURCE
*/
#ifdef THUMB
extern uint spin1_int_disable (void);
#else
#ifdef __GNUC__
__inline uint spin1_int_disable (void)
{
  uint old, new;

  asm volatile (
    "mrs	%[old], cpsr \n\
     orr	%[new], %[old], #0xc0 \n\
     msr	cpsr_c, %[new] \n"
     : [old] "=r" (old), [new] "=r" (new)
     : 
     : );

  return old;
}
#else
__forceinline uint spin1_int_disable (void)
{
  uint old, new;

  __asm { mrs old, cpsr }
  __asm { orr new, old, 0xc0 }
  __asm { msr cpsr_c, new }

  return old;
}
#endif
#endif
/*
*******/
// ------------------------------------------------------------------------



// ------------------------------------------------------------------------
// hardware support functions
// ------------------------------------------------------------------------
/****f* spin1_api.c/configure_communications_controller
*
* SUMMARY
*  This function configures the communications controller by clearing out
*  any pending packets from the RX buffer and clearing sticky error bits.
*
* SYNOPSIS
*  void configure_communications_controller()
*
* SOURCE
*/
void configure_communications_controller()
{
  // initialize transmitter control to send MC packets
  cc[CC_TCR] = 0x00000000;

  //TODO: clear receiver status
//  cc[CC_RSR] = 0x00000000;
}
/*
*******/



/****f* spin1_api.c/configure_dma_controller
*
* SUMMARY
*  This function configures the DMA controller by aborting any previously-
*  queued or currently-executing transfers and clearing any corresponding
*  interrupts then enabling all interrupts sources.
*
* SYNOPSIS
*  void configure_dma_controller()
*
* SOURCE
*/
void configure_dma_controller()
{
  dma[DMA_CTRL] = 0x3f; // Abort pending and active transfers
  dma[DMA_CTRL] = 0x0d; // clear possible transfer done and restart

  // TODO: needs updating when error support is completed
  // enable interrupt sources
  #if USE_WRITE_BUFFER == TRUE
    dma[DMA_GCTL] = 0x100c01; // enable dma done & write buffer ints.
  #else
    dma[DMA_GCTL] = 0x000c00; // enable dma done interrupt
  #endif

  #if API_DEBUG == TRUE
    /* initialise dmac counters */
    /* dmac counts transfer bursts */
    dma[DMA_SCTL] = 3; // clear and enable counters
  #endif
}
/*
*******/



/****f* spin1_api.c/configure_timer1
*
* SUMMARY
*  This function configures timer 1 to raise an interrupt with a pediod
*  specified by `time'. Firstly, timer 1 is disabled and any pending
*  interrupts are cleared. Then timer 1 load and background load
*  registers are loaded with the core clock frequency (set by the monitor and
*  recorded in system RAM MHz) multiplied by `time' and finally timer 1 is
*  loaded with the configuration below.
*
*    [0]   One-shot/wrapping     Wrapping
*    [1]   Timer size            32 bit
*    [3:2] Input clock divide    1
*    [5]   IRQ enable            Enabled
*    [6]   Mode                  Periodic
*    [7]   Timer enable          Disabled
*
* SYNOPSIS
*  void configure_timer1(uint time)
*
* INPUTS
*  uint time: timer period in microseconds, 0 = timer disabled
*
* SOURCE
*/
void configure_timer1(uint time)
{
  // do not enable yet!
  tc[T1_CONTROL] = 0;
  tc[T1_INT_CLR] = 1;
  tc[T1_LOAD] = sv->cpu_clk * time;
  tc[T1_BG_LOAD] = sv->cpu_clk * time;
}
/*
*******/



/****f* spin1_api.c/configure_vic
*
* SUMMARY
*  This function configures the Vectored Interrupt Controller. Firstly, all
*  interrupts are disabled and then the addresses of the interrupt service
*  routines are placed in the VIC vector address registers and the
*  corresponding control registers are configured. Priority is set in such a
*  manner that the first interrupt source configured by the function is the
*  first priority, the second interrupt configured is the second priority and
*  so on. This mechanism allows future programmers to reorder the IRQ
*  priorities by simply switching around the order in which they are set (a
*  simple copy+paste operation). Finally, the interrupt sources are enabled.
*
* SYNOPSIS
*  void configure_vic()
*
* SOURCE
*/
void configure_vic()
{
  uint fiq_select = 0;
  uint int_select = ((1 << TIMER1_INT)   |
                     (1 << SOFTWARE_INT) |
                     (1 << DMA_ERR_INT)  |
                     (1 << DMA_DONE_INT)
                    );

  // disable the relevant interrupts while configuring the VIC
  vic[VIC_DISABLE] = int_select;

  // configure fiq -- if requested by user
  switch (fiq_event)
  {
    case MC_PACKET_RECEIVED:
      // packet received is used by the barrier synchronization
      // cannot configure completely now!
      exception_table[7] = cc_rx_ready_fiqsr;
      break;
    case DMA_TRANSFER_DONE:
      exception_table[7] = dma_done_fiqsr;
      fiq_select = (1 << DMA_DONE_INT);
      break;
    case TIMER_TICK:
      exception_table[7] = timer1_fiqsr;
      fiq_select = (1 << TIMER1_INT);
      break;
    case USER_EVENT:
      exception_table[7] = soft_int_fiqsr;
      fiq_select = (1 << SOFTWARE_INT);
      break;
  }

  /* configure the DMA done interrupt */
  vic_vectors[DMA_DONE_PRIORITY]  = dma_done_isr;
  vic_controls[DMA_DONE_PRIORITY] = 0x20 | DMA_DONE_INT;

  /* configure the timer1 interrupt */
  vic_vectors[TIMER1_PRIORITY]  = timer1_isr;
  vic_controls[TIMER1_PRIORITY] = 0x20 | TIMER1_INT;

  /* configure the TX empty interrupt but don't enable it yet! */
  vic_vectors[CC_TMT_PRIORITY] = cc_tx_empty_isr;
  vic_controls[CC_TMT_PRIORITY] = 0x20 | CC_TMT_INT;

  /* configure the software interrupt */
  vic_vectors[SOFT_INT_PRIORITY]  = soft_int_isr;
  vic_controls[SOFT_INT_PRIORITY] = 0x20 | SOFTWARE_INT;

  #if USE_WRITE_BUFFER == TRUE
    /* configure the DMA error interrupt */
    vic_vectors[DMA_ERR_PRIORITY]  = dma_error_isr;
    vic_controls[DMA_ERR_PRIORITY] = 0x20 | DMA_ERR_INT;
  #endif

  /* select either as irq (0) or fiq (1) and enable interrupts */
  vic[VIC_SELECT] = (vic[VIC_SELECT] & ~int_select) | fiq_select;
  #if USE_WRITE_BUFFER == TRUE
    vic[VIC_ENABLE] = int_select;
  #else
    // don't enable the dma error interrupt
    vic[VIC_ENABLE] = int_select & ~(1 << DMA_ERR_INT);
  #endif
}
/*
*******/


/****f* spin1_api.c/spin1_get_id
*
* SUMMARY
*  This function returns a global ID for the processor.
*
* SYNOPSIS
*  uint spin1_get_id()
*
* OUTPUTS
*  Chip ID in bits [20:5], core ID in bits [4:0].
*
* SOURCE
*/
uint spin1_get_id(void)
{
  return (uint) ((sv->p2p_addr << 5) | virt_cpu);
}
/*
*******/



/****f* spin1_api.c/spin1_get_core_id
*
* SUMMARY
*  This function returns the core ID
*
* SYNOPSIS
*  uint spin1_get_core_id()
*
* OUTPUTS
*  core ID in the bottom 5 bits.
*
* SOURCE
*/
uint spin1_get_core_id(void)
{
	return virt_cpu;
}
/*
*******/



/****f* spin1_api.c/spin1_get_chip_id
*
* SUMMARY
*  This function returns the chip ID
*
* SYNOPSIS
*  uint spin1_get_chip_id()
*
* OUTPUTS
*  chip ID in the bottom 16 bits.
*
* SOURCE
*/
uint spin1_get_chip_id(void)
{
  return (uint) sv->p2p_addr;
}
/*
*******/



/****f* spin1_api.c/spin1_set_mc_table_entry
*
* SUMMARY
*  This function sets up an entry in the multicast routing table
*
* SYNOPSIS
*  void spin1_set_mc_table_entry(uint entry, uint key, uint mask,
*                                uint route)
*
* INPUTS
*  uint entry: table entry
*  uint key: entry routing key field
*  uint mask: entry mask field
*  uint route: entry route field
*
* SOURCE
*/
uint spin1_set_mc_table_entry(uint entry, uint key, uint mask, uint route)
{
  if (entry < APP_MC_ENTRIES)
  {
    // need to do virtual to physical core ID conversion
    uint proute = route & (1 << NUM_LINKS) - 1;  // Keep link bits

    for (uint v = 0; v < sv->num_cpus; v++)  // go through virt_cpus
    {
      uint bit = route & (1 << (NUM_LINKS + v));
      if (bit)
      {
        proute |= CORE_ROUTE(sv->v2p_map[v]);
      }
    }

    // top priority entries reserved for the system
    entry += SYS_MC_ENTRIES;

    rtr_mckey[entry] = key;
    rtr_mcmsk[entry] = mask;
    rtr_mcrte[entry] = proute;

    #if API_DEBUG == TRUE
      if ((NON_ROOT) || rootAp)
      {
        io_printf (IO_STD,
                   "\t\t[api_debug] MC entry %d: k 0x%8z m 0x%8z r 0x%8z\n",
                   entry, key, mask, proute);
        spin1_delay_us (API_PRINT_DLY);
      }
    #endif

    return SUCCESS;
  }
  else
  {
    return FAILURE;
  }
}
/*
*******/



/****f* spin1_api.c/p2p_get
*
* SUMMARY
*  This function reads an entry from the point-to-point routing table
*
* SYNOPSIS
*  uint p2p_get (uint entry)
*
* INPUTS
*  uint entry: table entry
*
* OUTPUTS
*  point-to-point table data
*
* SOURCE
*/
uint p2p_get (uint entry)
{
  uint word = entry >> P2P_LOG_EPW;

  if (word >= P2P_TABLE_SIZE)
    return 6;

  uint offset = P2P_BPE * (entry & P2P_EMASK);
  uint data = rtr_p2p[word];

  return (data >> offset) & P2P_BMASK;
}
/*
*******/



/****f* spin1_api.c/spin1_led_control
*
* SUMMARY
*  This function controls LEDs according to an input pattern.
*  Macros for turning LED number N on, off or inverted are
*  defined in spinnaker.h.
*
*  To turn LEDs 0 and 1 on, then invert LED2 and finally
*  turn LED 0 off:
*
*   spin1_led_control (LED_ON (0) + LED_ON (1));
*   spin1_led_control (LED_INV (2));
*   spin1_led_control (LED_OFF (0));
*
* SYNOPSIS
*  void spin1_set_leds(uint p);
*
* INPUTS
*  uint p: led control word
*
* SOURCE
*/
void spin1_led_control (uint p)
{
  const ushort *leds = (sv->hw_ver == HW_VER_S2) ? S2_LEDS : S3_LEDS;
  uint num_leds = leds[0];

  for (uint i = 1; i <= num_leds; i++)
  {
    uint led = leds[i];
    uint v = p & 1;
    uint c = p & 2;

    p = p >> 2;

    if (c == 0)
    {
      if (v != 0)
        v = sc[GPIO_PORT] & led;
      else
        continue;
    }

    if (v)
      sc[GPIO_CLR] = led;
    else
      sc[GPIO_SET] = led;
  }
}
/*
*******/
// ------------------------------------------------------------------------



// ------------------------------------------------------------------------
// memory allocation functions
// ------------------------------------------------------------------------
/****f* spin1_api.c/spin1_malloc
*
* SUMMARY
*  This function returns a pointer to a block of memory of size "bytes".
*
* SYNOPSIS
*  void * spin1_malloc(uint bytes)
*
* INPUTS
*  uint bytes: size, in bytes, of the requested memory block
*
* OUTPUTS
*  pointer to the requested memory block or 0 if unavailable
*
* SOURCE
*/
void* spin1_malloc(uint bytes)
{
  void *mem_addr;    // memory address to be returned

  // check if memory is available - leave 4KBytes for stacks
  if (((uint) current_addr + bytes) <= (uint) (DTCM_TOP - RTS_STACKS))
  {
    // requested memory fits - return pointer
    mem_addr = current_addr;
    // update pointer - word align it!
    current_addr = (void *) (((uint) current_addr + bytes + 3) & 0xfffffffc);
  }
  else
  {
    // no room available - return null
    mem_addr = 0;
  }

 return mem_addr;
 }
/*
*******/



// ------------------------------------------------------------------------
// scheduler/dispatcher functions
// ------------------------------------------------------------------------
/****f* spin1_api.c/dispatch
*
* SUMMARY
*  This function executes callbacks which are scheduled in response to events.
*  Callbacks are completed firstly in order of priority and secondly in the
*  order in which they were enqueued.
*
*  The dispatcher is the sole consumer of the scheduler queues and so can
*  safely run with interrupts enabled. Note that deschedule(uint event_id)
*  modifies the scheduler queues which naturally influences the callbacks
*  that are dispatched by this function but not in such a way as to allow the
*  dispatcher to move the processor into an invalid state such as calling a
*  NULL function.
*
*  Upon emptying the scheduling queues the dispatcher goes into wait for
*  interrupt mode.
*
*  Potential hazard: It is possible that an event will occur -and result in
*  a callback being scheduled- AFTER the last check on the scheduler queues
*  and BEFORE the wait for interrupt call. In this case, the scheduled
*  callback would not be handled until the next event occurs and causes the
*  wait for interrupt call to return.
*
*  This hazard is avoided by calling wait for interrupt with interrupts
*  disabled! Any interrupt will still wake up the core and then
*  interrupts are enabled, allowing the core to respond to it.
*
* SYNOPSIS
*  void dispatch()
*
* SOURCE
*/
void dispatch()
{
  uint i;
  uint cpsr;
  task_queue_t *tq;
  volatile callback_t cback;

  // initialise task queues
  for (i=0; i<(NUM_PRIORITIES-1); i++)
  {
    //!! ST    task_queue[i].start = 0;
    //!! ST    task_queue[i].end   = 0;
  }

  // dispatch callbacks from queues until spin1_stop () or
  // spin1_kill () are called (run = 0)
  while(run)
  {
    i = 0;

    // disable interrupts to avoid concurrent
    // scheduler/dispatcher accesses to queues
    cpsr = spin1_int_disable ();

    while (run && i < (NUM_PRIORITIES-1))
    {
      tq = &task_queue[i];

      i++;  // prepare for next priority queue

      if(tq->start != tq->end)
      {
        cback = tq->queue[tq->start].cback;
        uint arg0 = tq->queue[tq->start].arg0;
        uint arg1 = tq->queue[tq->start].arg1;

        tq->start = (tq->start + 1) % TASK_QUEUE_SIZE;

        if(cback != NULL)
        {
          // run callback with interrupts enabled
          spin1_mode_restore (cpsr);
          cback (arg0, arg1);
          cpsr = spin1_int_disable ();

          // re-start examining queues at highest priority
          i = 0;
        }
      }
    }

    if (run)
    {
      // go to sleep with interrupts disabled to avoid hazard!
      // an interrupt will still wake up the dispatcher
      wait_for_irq ();
      spin1_mode_restore (cpsr);
    }
  }
}
/*
*******/



/****f* spin1_api.c/deschedule
*
* SUMMARY
*  This function deschedules all callbacks corresponding to the given event
*  ID. One use for this function is to effectively discard all received
*  packets which are yet to be processed by calling
*  deschedule(MC_PACKET_RECEIVED). Note that this function cannot guarantee that
*  all callbacks pertaining to the given event ID will be descheduled: once a
*  callback has been prepared for execution by the dispatcher it is immune to
*  descheduling and will be executed upon return to the dispatcher.
*
* SYNOPSIS
*  void deschedule(uint event_id)
*
* INPUTS
*  uint event_id: event ID of the callbacks to be descheduled
*
* SOURCE
*/
void deschedule(uint event_id)
{
  uint cpsr = spin1_irq_disable();

  task_queue_t *tq = &task_queue[callback[event_id].priority-1];

  for(uint i = 0; i < TASK_QUEUE_SIZE; i++)
  {
    if(tq->queue[i].cback == callback[event_id].cback) tq->queue[i].cback = NULL;
  }

  spin1_mode_restore(cpsr);
}
/*
*******/



/****f* spin1_api.c/schedule_sysmode
*
* SUMMARY
*  This function places a callback into the scheduling queue corresponding
*  to its priority, which is set at configuration time by on_callback(...).
*
*  Non-queueable callbacks (those of priority 0) are executed immediately
*  and atomically.
*
*  Interrupts are not explicitly disabled during this routine: it is assumed
*  that the function is only called by interrupt service routines responding
*  to events, and these ISRs execute with interrupts disabled.
*
* SYNOPSIS
*  void schedule_sysmode(uchar event_id, uint arg0, uint arg1)
*
* INPUTS
*  uchar event_id: ID of the event triggering a callback
*  uint arg0: argument to be passed to the callback
*  uint arg1: argument to be passed to the callback
*
* SOURCE
*/
void schedule_sysmode(uchar event_id, uint arg0, uint arg1)
{
  if(callback[event_id].priority <= 0)
  {
    callback[event_id].cback(arg0, arg1);
  }
  else
  {
    task_queue_t *tq = &task_queue[callback[event_id].priority-1];

    if((tq->end + 1) % TASK_QUEUE_SIZE != tq->start)
    {
      tq->queue[tq->end].cback = callback[event_id].cback;
      tq->queue[tq->end].arg0 = arg0;
      tq->queue[tq->end].arg1 = arg1;

      tq->end = (tq->end + 1) % TASK_QUEUE_SIZE;
    }
    else
    {
      // queue is full
      #if (API_WARN == TRUE) || (API_DIAGNOSTICS == TRUE)
        warnings |= TASK_QUEUE_FULL;
        fullq++;
      #endif
    }
  }
}
/*
*******/



/****f* spin1_api.c/schedule
*
* SUMMARY
*  This function is used to switch to SYS mode so that the scheduler
*  executes in that mode. It the switches back to IRQ mode to return.
*
* SYNOPSIS
*  void schedule(uchar event_id, uint arg0, uint arg1)
*
* INPUTS
*  uchar event_id: ID of the event triggering a callback
*  uint arg0: argument to be passed to the callback
*  uint arg1: argument to be passed to the callback
*
* SOURCE
*/

// !! ST - moved to another file (sark_init.s)

/*
*******/



/****f* spin1_api.c/spin1_schedule_callback
*
* SUMMARY
*  This function places a cback into the scheduling queue corresponding
*  to its priority
*
* SYNOPSIS
*  uint spin1_schedule_callback(callback_t cback, uint arg0, uint arg1,
                                uint priority)
*
* INPUTS
*  callback_t cback: callback to be scheduled
*  uint arg0: argument to be passed to the callback
*  uint arg1: argument to be passed to the callback
*  uint priority: cback priority
*
* SOURCE
*/
uint spin1_schedule_callback(callback_t cback, uint arg0, uint arg1, uint priority)
{
  uchar result = SUCCESS;

  /* disable interrupts for atomic access to task queues */
  uint cpsr = spin1_irq_disable();

  task_queue_t *tq = &task_queue[priority-1];

  if((tq->end + 1) % TASK_QUEUE_SIZE != tq->start)
  {
    tq->queue[tq->end].cback = cback;
    tq->queue[tq->end].arg0 = arg0;
    tq->queue[tq->end].arg1 = arg1;

    tq->end = (tq->end + 1) % TASK_QUEUE_SIZE;
  }
  else
  {
    // queue is full
    result = FAILURE;
    #if (API_WARN == TRUE) || (API_DIAGNOSTICS == TRUE)
      warnings |= TASK_QUEUE_FULL;
      fullq++;
    #endif
  }

  /* restore interrupt status */
  spin1_mode_restore(cpsr);

  return result;
}
/*
*******/
// ------------------------------------------------------------------------


/****f* spin1_api.c/spin1_trigger_user_event
*
* SUMMARY
*  This function triggers a USER EVENT, i.e., a software interrupt.
*  The function returns FAILURE if a previous trigger attempt
*  is still pending.
*
* SYNOPSIS
*  __irq void spin1_trigger_user_event(uint arg0, uint arg1)
*
* INPUTS
*  uint arg0: argument to be passed to the callback
*  uint arg1: argument to be passed to the callback
*
* OUTPUTS
*   uint: 0 = FAILURE, 1 = SUCCESS
*
* SOURCE
*/
uint spin1_trigger_user_event(uint arg0, uint arg1)
{
  if (!user_pending)
  {
    /* remember callback arguments */
    user_arg0 = arg0;
    user_arg1 = arg1;
    user_pending = TRUE;

    /* trigger software interrupt in the VIC */
    vic[VIC_SOFT_SET] = (1 << SOFTWARE_INT);

    return (SUCCESS);
  }
  else
  {
    return (FAILURE);
  }
}
/*
*******/



// ------------------------------------------------------------------------
// rts initialization function
// called before the application program starts!
// ------------------------------------------------------------------------
/****f* spin1_api.c/rts_init
*
* SUMMARY
*  This function is a stub for the run-time system.
*  initializes peripherals in the way the RTS
*  is expected to do
*
* SYNOPSIS
*  void rts_init (void)
*
* SOURCE
*/
void rts_init (void)
{
  uint i;

  // try to become root application core for this chip
  rootAp = lock_try (LOCK_API_ROOT);

  if (rootAp)
  {
    #if API_DEBUG == TRUE
      io_printf (IO_STD, "\t\t[api_debug] rootAp: %d mon: %d\n",
                 phys_cpu, sv->v2p_map[0]);
      spin1_delay_us (API_PRINT_DLY);
    #endif

    // initialize all MC routing table entries to invalid
    for (i = 0; i < MC_TABLE_SIZE; i++)
    {
      rtr_mckey[i] = 0xffffffff;
      rtr_mcmsk[i] = 0x00000000;
    }

    // --- setup routing table ---
    // setup the RDY1 entry
    // local ready packets come to me
    rtr_mckey[2] = RDY1_KEY;
    rtr_mcmsk[2] = BARRIER_MASK;
    rtr_mcrte[2] = CORE_ROUTE(phys_cpu);

    // router initialized -- let other cores go!
    sv->lock = RTR_INIT_LCK;

    // initialize ready counts (I'm already ready!)
    barrier_rdy1_cnt = 1;
    barrier_rdy2_cnt = 1;

    #if (API_DEBUG == TRUE) || (API_DIAGNOSTICS == TRUE)
      io_printf (IO_STD, "\t\t[api_debug] RDY1: k 0x%8z m 0x%8z r 0x%8z\n",
                 RDY1_KEY, BARRIER_MASK, CORE_ROUTE(phys_cpu));

      spin1_delay_us (API_PRINT_DLY);

      // initialize router counters
      // router counts total, mc, p2p, nn and dumped packets
      rtr[RTR_DGEN] = 0x00000000;    // disable counters
      rtr[RTR_DGC0] = 0;             // clear counter 0
      rtr[RTR_DGF0] = 0x01fffcff;    // count all packets
      rtr[RTR_DGC1] = 0;             // clear counter 1
      rtr[RTR_DGF1] = 0x01fffcf1;    // count mc packets
      rtr[RTR_DGC2] = 0;             // clear counter 2
      rtr[RTR_DGF2] = 0x01fffcf2;    // count p2p packets
      rtr[RTR_DGC3] = 0;             // clear counter 3
      rtr[RTR_DGF3] = 0x01fffcf4;    // count nn packets
      rtr[RTR_DGC4] = 0;             // clear counter 4
      rtr[RTR_DGF4] = 0x0001fcff;    // count dumped packets
      rtr[RTR_DGC5] = 0;             // clear counter 5
      rtr[RTR_DGF5] = 0x0001fcf1;    // count dumped mc packets
      rtr[RTR_DGEN] = 0x0000003f;    // enable the 6 counters

      // initialize ISR discarded packet count
      thrown = 0;
    #endif
  }
  else
  {
    uint start;
    volatile uint *ms_cnt = &sv->clock_ms;

    /* wait for router init to finish */
    // timeout if taking too long!
    start = *ms_cnt;          // initial value
    while (((*ms_cnt - start) < ROUTER_INIT_WAIT) && (sv->lock < RTR_INIT_LCK))
    {
      continue;
    }

    #if API_WARN == TRUE
      if (sv->lock < RTR_INIT_LCK)
      {
        warnings |= SYNCHRO_ERROR;
        io_printf (IO_STD, "\t\t[api_warn] warning: waiting for router init failed.\n");
        spin1_delay_us (API_PRINT_DLY);
      }
    #endif
  }

  // initialize diagnostics data
  // --------------------------------
  diagnostics.exit_code            = NO_ERROR;
  diagnostics.warnings             = NO_ERROR;
  diagnostics.total_mc_packets     = 0;
  diagnostics.dumped_mc_packets    = 0;
  diagnostics.discarded_mc_packets = 0;
  diagnostics.dma_transfers        = 0;
  diagnostics.dma_bursts           = 0;
  diagnostics.dma_queue_full       = 0;
  diagnostics.task_queue_full      = 0;
  diagnostics.tx_packet_queue_full = 0;
  diagnostics.writeBack_errors     = 0;

  // setup the barrier packet handler
  // --------------------------------
  // disable the interrupt while configuring the VIC
  vic[VIC_DISABLE] = (1 << CC_RDY_INT);
  // point the packet received interrupt to the barrier handler
  vic_vectors[RX_READY_PRIORITY]  = barrier_packet_handler;
  // enable vector in VIC
  vic_controls[RX_READY_PRIORITY] = (0x20 | CC_RDY_INT);
  // select as irq
  vic[VIC_SELECT] = vic[VIC_SELECT] & ~(1 << CC_RDY_INT);
  // and re-enable
  vic[VIC_ENABLE] = (1 << CC_RDY_INT);

  // remember default fiq handler
  def_fiq_isr = exception_table[7];
}
/*
*******/
// ------------------------------------------------------------------------



// ------------------------------------------------------------------------
// rts cleanup function
// called after the application program finishes!
// ------------------------------------------------------------------------
/****f* spin1_api.c/rts_cleanup
*
* SUMMARY
*  This function is a stub for the run-time system.
*  makes sure that application returns cleanly to the RTS.
*
* SYNOPSIS
*  void rts_cleanup (void)
*
* SOURCE
*/
void rts_cleanup (void)
{
  uint i;

  // clear the sync lock!
  sv->lock = CLEAR_LCK;

  if (rootAp)
  {
    // initialize all MC routing table entries to invalid
    for (i = 0; i < MC_TABLE_SIZE; i++)
    {
      rtr_mckey[i] = 0xffffffff;
      rtr_mcmsk[i] = 0x00000000;
    }

    // clear the rootAp lock
    lock_free (LOCK_API_ROOT);
  }

  // re-enable interrupts to consume any received packets
  // in case other cores modify routing entries
  vic[VIC_ENABLE] = (1 << CC_RDY_INT);
}
/*
*******/
// ------------------------------------------------------------------------

#ifdef __GNUC__
void raise()
{
    io_printf (IO_STD, "Error: division by 0 - Terminating\n");
    while (1);
}
#endif
