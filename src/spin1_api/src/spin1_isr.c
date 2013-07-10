#include "spin1_api.h"
#include "spin1_api_params.h"
#include "spinn_io.h"

extern void schedule (uchar event_id, uint arg0, uint arg1);

extern void send_mc_packet_fulfil (uint key, uint data, uint load);

extern uint def_fiq_isr;
extern volatile uchar user_pending;
extern uint user_arg0;
extern uint user_arg1;

extern volatile uint barrier_rdy1_cnt;
extern volatile uint barrier_rdy2_cnt;
extern volatile uint barrier_go;
extern volatile uint barrier_rdygo;
extern volatile uint ticks;
extern volatile uint thrown;

extern dma_queue_t dma_queue;
extern tx_packet_queue_t tx_packet_queue;
extern volatile uint wberrors;

// ----------------------------------
/* pseudo-random number generation */
// ----------------------------------
static uint prng_seed = 1;
static uint prng_seed_ext = 0;
// --------------------

#ifdef __GNUC__
void __attribute__ ((interrupt ("IRQ"))) cc_rx_ready_isr (void);
#else
__irq void cc_rx_ready_isr(void);
#endif

#ifdef __GNUC__
void __attribute__ ((interrupt ("IRQ"))) cc_rx_ready_fiqsr (void);
#else
__irq void cc_rx_ready_fiqsr(void);
#endif


// ------------------------------------------------------------------------
// interrupt support functions
// ------------------------------------------------------------------------
/****f* spin1_isr.c/barrier_packet_handler
*
* SUMMARY
*
* SYNOPSIS
*  void barrier_packet_handler (void)
*   checks ready and go MC packets for barrier
*
* INPUTS
*  uint n: number of cores that synchronise
*
* SOURCE
*/
#ifdef __GNUC__
void __attribute__ ((interrupt ("IRQ"))) barrier_packet_handler (void)
#else
__irq void barrier_packet_handler (void)
#endif
{
  /* check packet key */
  switch (cc[CC_RXKEY])  // also clears interrupt
  {
    case RDYGO_KEY:
      barrier_rdygo = TRUE;
      break;
    case GO_KEY:
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
      barrier_go = TRUE;
      break;
    case RDY1_KEY:
      barrier_rdy1_cnt++;
      break;
    case RDY2_KEY:
      barrier_rdy2_cnt++;
      break;
    default:
      //TODO: unexpected - problem!
      break;
  }

  /* ack VIC */
  vic[VIC_VADDR] = 1;
}
/*
*******/


/****f* spin1_isr.c/cc_rx_error_isr
*
* SUMMARY
*  This interrupt service routine is called in response to receipt of a packet
*  from the router with either parity or framing errors. The routine simply
*  clears the error and disposes of the packet. The monitor processor may
*  observe packet errors by reading from the router diagnostic registers.
*
* SYNOPSIS
*  __irq void cc_rx_error_isr()
*
* SOURCE
*/
#ifdef __GNUC__
void __attribute__ ((interrupt ("IRQ"))) cc_rx_error_isr (void)
#else
__irq void cc_rx_error_isr()
#endif
{
  /* consume erroneous packet */
  uint sink = cc[CC_RXKEY];  // also clears interrupt

  /* clear error flags (sticky) in CC and ack VIC */
  cc[CC_RSR] = 0;
  vic[VIC_VADDR] = 1;
}
/*
*******/


/****f* spin1_isr.c/cc_rx_ready_isr
*
* SUMMARY
*  This interrupt service routine is called in response to receipt of a packet
*  from the router. Chips are configured such that fascicle processors receive
*  only multicast neural event packets. In response to receipt of a MC packet
*  a callback is scheduled to process the corresponding routing key and data.
*
*  Checking for parity and framing errors is not performed. The VIC is
*  configured so that the interrupts raised by erroneous packets prompt
*  execution of cc_rx_error_isr which clears them.
*
* SYNOPSIS
*  __irq void cc_rx_ready_isr()
*
* SOURCE
*/
#ifdef __GNUC__
void __attribute__ ((interrupt ("IRQ"))) cc_rx_ready_isr (void)
#else
__irq void cc_rx_ready_isr(void)
#endif
{
  /* get received packet */
  uint rx_data = cc[CC_RXDATA];
  uint rx_key = cc[CC_RXKEY];  // also clears interrupt

  /* if application callback registered schedule it */
  if(callback[MC_PACKET_RECEIVED].cback != NULL)
  {
    schedule(MC_PACKET_RECEIVED, rx_key, rx_data);
  }
  else
  {
    #if API_DEBUG == TRUE
      thrown++;
    #endif
  }

  // TODO: maybe clear error flags (sticky) in CC

  /* ack VIC */
  vic[VIC_VADDR] = 1;
}
/*
*******/


/****f* spin1_isr.c/cc_tx_empty_isr
*
* SUMMARY
*  This interrupt service function is called when the comms controller
*  transmit buffer is empty. The function dequeues packets queued for
*  transmission by spin1_send_mc_packet function and writes them to the comms
*  controller hardware, until either the packet queue is empty or the comms
*  controller is full.
*
* SYNOPSIS
*  __irq void cc_tx_empty_isr()
*
* SOURCE
*/
#ifdef __GNUC__
void __attribute__ ((interrupt ("IRQ"))) cc_tx_empty_isr (void)
#else
__irq void cc_tx_empty_isr()
#endif
{
//TODO: should use TX_not_full interrupt in SpiNNaker -- more efficient!

  /* Clear the sticky TX full bit */
  cc[CC_TCR] = TX_TCR_MCDEFAULT;

  /* drain queue: send packets while queue not empty and CC not full */
  while(tx_packet_queue.start != tx_packet_queue.end && ~cc[CC_TCR] & 0x40000000)
  {
    /* dequeue packet */
    uint key = tx_packet_queue.queue[tx_packet_queue.start].key;
    uint data = tx_packet_queue.queue[tx_packet_queue.start].data;
    uint load = tx_packet_queue.queue[tx_packet_queue.start].load;

    tx_packet_queue.start = (tx_packet_queue.start + 1) % TX_PACKET_QUEUE_SIZE;

    send_mc_packet_fulfil(key, data, load);
  }

  if(tx_packet_queue.start == tx_packet_queue.end)
  {
    /* if queue empty turn off tx_empty interrupt */
    vic[VIC_DISABLE] = 0x1 << CC_TMT_INT;
  }

  /* ack VIC */
  vic[VIC_VADDR] = 1;
}
/*
*******/


/****f* spin1_isr.c/dma_error_isr
*
* SUMMARY
*  This interrupt service function is called when a DMA transfer error arises.
*  Currently, such an event causes termination of the simulation.
*
* SYNOPSIS
*  __irq void dma_error_isr()
*
* SOURCE
*/
#ifdef __GNUC__
void __attribute__ ((interrupt ("IRQ"))) dma_error_isr (void)
#else
__irq void dma_error_isr()
#endif
{
  //TODO: update to other dma error sources when supported
  // deal with write buffer errors
  #if API_WARN == TRUE
    #if USE_WRITE_BUFFER == TRUE
      // increase error count
      wberrors++;
    #endif
  #endif

  /* clear write buffer error interrupt in DMAC and ack VIC */
  dma[DMA_CTRL]  = 0x20;
  vic[VIC_VADDR] = 1;
}
/*
*******/


/****f* spin1_isr.c/dma_done_isr
*
* SUMMARY
*  This interrupt service routine is called upon completion of a DMA transfer.
*  A user callback is scheduled (with two parameters, the ID of the completed
*  transfer and the user-provided transfer tag) and the next DMA transfer
*  request is dequeued and fulfilled. The completion and subsequent scheduling
*  of transfers must be atomic (as they are in this uninterruptable ISR)
*  otherwise transfer requests may not be completed in the order they were
*  made.
*
* SYNOPSIS
*  __irq void dma_done_isr()
*
* SOURCE
*/
#ifdef __GNUC__
void __attribute__ ((interrupt ("IRQ"))) dma_done_isr (void)
#else
__irq void dma_done_isr()
#endif
{
  /* clear transfer done interrupt in DMAC */
  dma[DMA_CTRL]  = 0x8;

  /* prepare data for callback before triggering a new DMA transfer */
  uint completed_id  = dma_queue.queue[dma_queue.start].id;
  uint completed_tag = dma_queue.queue[dma_queue.start].tag;

  //TODO: can schedule up to 2 transfers if DMA free
  /* update queue pointer and trigger new transfer if queue not empty */
  dma_queue.start = (dma_queue.start + 1) % DMA_QUEUE_SIZE;
  if(dma_queue.start != dma_queue.end)
  {
    uint *system_address = dma_queue.queue[dma_queue.start].system_address;
    uint *tcm_address = dma_queue.queue[dma_queue.start].tcm_address;
    uint  description = dma_queue.queue[dma_queue.start].description;

    dma[DMA_ADRS] = (uint) system_address;
    dma[DMA_ADRT] = (uint) tcm_address;
    dma[DMA_DESC] = description;
  }

  /* if application callback registered schedule it */
  if(callback[DMA_TRANSFER_DONE].cback != NULL)
  {
    schedule(DMA_TRANSFER_DONE, completed_id, completed_tag);
  }

  /* ack VIC */
  vic[VIC_VADDR] = 1;
}
/*
*******/


/****f* spin1_isr.c/timer1_isr
*
* SUMMARY
*  This interrupt service routine is called upon countdown of the processor's
*  primary timer to zero. In response, a callback is scheduled.
* 
* SYNOPSIS
*  __irq void timer1_isr()
*
* SOURCE
*/
#ifdef __GNUC__
void  __attribute__ ((interrupt ("IRQ"))) timer1_isr (void)
#else
__irq void timer1_isr()
#endif
{
  /* clear timer interrupt */
  tc[T1_INT_CLR] = 1;

  /* increment simulation "time" */
  ticks++;

  /* if application callback registered schedule it */
  if(callback[TIMER_TICK].cback != NULL)
  {
    schedule(TIMER_TICK, ticks, NULL);
  }

  /* ack VIC */
  vic[VIC_VADDR] = 1;
}
/*
*******/


/****f* spin1_isr.c/soft_int_isr
*
* SUMMARY
*  This interrupt service routine is called upon receipt of software
*  controller interrupt, triggered by a "USER EVENT".
*
* SYNOPSIS
*  __irq void soft_int_isr()
*
* SOURCE
*/
#ifdef __GNUC__
void __attribute__ ((interrupt ("IRQ"))) soft_int_isr (void)
#else
__irq void soft_int_isr()
#endif
{
  /* clear software interrupt in the VIC */
  vic[VIC_SOFT_CLR] = (1 << SOFTWARE_INT);

  /* clear flag to indicate event has been serviced */
  user_pending = FALSE;

  /* if application callback registered schedule it */
  if(callback[USER_EVENT].cback != NULL)
  {
    schedule(USER_EVENT, user_arg0, user_arg1);
  }

  /* ack VIC */
  vic[VIC_VADDR] = 1;
}
/*
*******/


/****f* spin1_isr.c/cc_rx_ready_fiqsr
*
* SUMMARY
*  This interrupt service routine is called in response to receipt of a packet
*  from the router. Chips are configured such that fascicle processors receive
*  only multicast neural event packets. In response to receipt of a MC packet
*  a callback is scheduled to process the corresponding routing key and data.
*
*  Checking for parity and framing errors is not performed. The VIC is
*  configured so that the interrupts raised by erroneous packets prompt
*  execution of cc_rx_error_isr which clears them.
*
* SYNOPSIS
*  __irq void cc_rx_ready_fiqsr()
*
* SOURCE
*/
#ifdef __GNUC__
void __attribute__ ((interrupt ("IRQ"))) cc_rx_ready_fiqsr (void)
#else
__irq void cc_rx_ready_fiqsr()
#endif
{
  /* get received packet */
  uint rx_data = cc[CC_RXDATA];
  uint rx_key = cc[CC_RXKEY];  // also clears interrupt

  /* execute preeminent callback */
  callback[MC_PACKET_RECEIVED].cback(rx_key, rx_data);

  // TODO: maybe clear error flags (sticky) in CC
}
/*
*******/


/****f* spin1_isr.c/dma_done_fiqsr
*
* SUMMARY
*  This interrupt service routine is called upon completion of a DMA transfer.
*  A user callback is scheduled (with two parameters, the ID of the completed
*  transfer and `1' indicating transfer success) and the next DMA transfer
*  request is dequeued and fulfilled. The completion and subsequent scheduling
*  of transfers must be atomic (as they are in this uninterruptable ISR)
*  otherwise transfer requests may not be completed in the order they were
*  made.
*
* SYNOPSIS
*  __irq void dma_done_fiqsr()
*
* SOURCE
*/
#ifdef __GNUC__
void __attribute__ ((interrupt ("IRQ"))) dma_done_fiqsr (void)
#else
__irq void dma_done_fiqsr()
#endif
{
  /* clear transfer done interrupt in DMAC */
  dma[DMA_CTRL]  = 0x8;

  /* prepare data for callback before triggering a new DMA transfer */
  uint completed_id  = dma_queue.queue[dma_queue.start].id;
  uint completed_tag = dma_queue.queue[dma_queue.start].tag;

  //TODO: can schedule up to 2 transfers if DMA free
  /* update queue pointer and trigger new transfer if queue not empty */
  dma_queue.start = (dma_queue.start + 1) % DMA_QUEUE_SIZE;
  if(dma_queue.start != dma_queue.end)
  {
    uint *system_address = dma_queue.queue[dma_queue.start].system_address;
    uint *tcm_address = dma_queue.queue[dma_queue.start].tcm_address;
    uint  description = dma_queue.queue[dma_queue.start].description;

    dma[DMA_ADRS] = (uint) system_address;
    dma[DMA_ADRT] = (uint) tcm_address;
    dma[DMA_DESC] = description;
  }

  /* execute preeminent callback */
  callback[DMA_TRANSFER_DONE].cback(completed_id, completed_tag);
}
/*
*******/


/****f* spin1_isr.c/timer1_fiqsr
*
* SUMMARY
*  This interrupt service routine is called upon countdown of the processor's
*  primary timer to zero. In response, a callback is scheduled.
*
* SYNOPSIS
*  __irq void timer1_fiqsr()
*
* SOURCE
*/
#ifdef __GNUC__
void __attribute__ ((interrupt ("IRQ"))) timer1_fiqsr (void)
#else
__irq void timer1_fiqsr()
#endif
{
  /* clear timer interrupt */
  tc[T1_INT_CLR] = 1;

  /* increment simulation "time" */
  ticks++;

  /* execute preeminent callback */
  callback[TIMER_TICK].cback(ticks, NULL);
}
/*
*******/


/****f* spin1_isr.c/soft_int_fiqsr
*
* SUMMARY
*  This interrupt service routine is called upon receipt of software
*  controller interrupt, triggered by a "USER EVENT".
*
* SYNOPSIS
*  __irq void soft_int_fiqsr()
*
* SOURCE
*/
#ifdef __GNUC__
void __attribute__ ((interrupt ("IRQ"))) soft_int_fiqsr (void)
#else
__irq void soft_int_fiqsr()
#endif
{
  /* clear software interrupt in the VIC */
  vic[VIC_SOFT_CLR] = (1 << SOFTWARE_INT);

  /* execute preeminent callback */
  callback[USER_EVENT].cback(user_arg0, user_arg1);

  /* clear flag to indicate event has been serviced */
  user_pending = FALSE;
}
/*
*******/
// ------------------------------------------------------------------------


// ------------------------------------------------------------------------
// pseudo-random number generation functions
// ------------------------------------------------------------------------
/****f* spin1_isr.c/spin1_srand
*
* SUMMARY
*  This function is used to initialize the seed for the
*  pseudo-random number generator.
*
* SYNOPSIS
*  void spin1_srand (uint seed)
*
* SOURCE
*/
void spin1_srand (uint seed)
{
  prng_seed = seed;
  prng_seed_ext = 0;
}
/*
*******/


/****f* spin1_isr.c/spin1_rand
*
* SUMMARY
*  This function generates a pseudo-random 32-bit integer.
*  Taken from "Programming Techniques"
*  ARM document ARM DUI 0021A
*
* SYNOPSIS
*  uint spin1_rand (void)
*
* OUTPUTS
*  32-bit pseudo-random integer
*
* SOURCE
*/
/*
*******/
#ifdef __GNUC__
uint spin1_rand (void)
{
  uint temp;

  asm volatile (
    "tst  %[prng_seed_ext], %[prng_seed_ext], lsr #1 \n\
     movs %[temp], %[prng_seed], rrx \n\
     adc  %[prng_seed_ext], %[prng_seed_ext], %[prng_seed_ext] \n\
     eor  %[temp], %[temp], %[prng_seed], lsl #12 \n\
     eor  %[prng_seed], %[temp], %[temp], lsr #20 \n"
     : [temp]          "=r" (temp),
       [prng_seed]     "+r" (prng_seed),
       [prng_seed_ext] "+r" (prng_seed_ext)
     :
     : );

  return prng_seed;
}
#else
uint  spin1_rand  (void)
{
  uint temp;

  __asm { tst  prng_seed_ext, prng_seed_ext, lsr 1 }
  __asm { movs temp, prng_seed, rrx }
  __asm { adc  prng_seed_ext, prng_seed_ext, prng_seed_ext }
  __asm { eor  temp, temp, prng_seed, lsl 12 }
  __asm { eor  prng_seed, temp, temp, lsr 20 }

  return prng_seed;
}
#endif
/*
*******/




// ------------------------------------------------------------------------
