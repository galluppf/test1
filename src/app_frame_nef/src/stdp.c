/****a* stdp.c/stdp
*
* SUMMARY
*  This file contains the functions related to the standard STDP learning algorithm
*
* AUTHOR
*  Sergio Davies - daviess@cs.man.ac.uk
*
* DETAILS
*  Created on       : 26 May 2011
*  Version          : $Revision: 1225 $
*  Last modified on : $Date: 2011-06-30 19:26:19 -0600 (Thu, 30 Jun 2011) $
*  Last modified by : $Author: sergio $
*  $Id: stdp.c 1225 2011-07-01 01:26:19Z sergio $
*  $HeadURL: file:///media/drybar/app_frame/branch/pacman_modifications/src/stdp.c $
*
* COPYRIGHT
*  Copyright (c) The University of Manchester, 2011. All rights reserved.
*  SpiNNaker Project
*  Advanced Processor Technologies Group
*  School of Computer Science
*
*******/

#include <string.h>
#include "stdp.h"
#include "spinn_api.h"
#include "spinn_io.h"
#include "config.h"
#include "dma.h"

int *PostTimeStamp = NULL;
extern app_data_t app_data;

void load_stdp_config_data(void)
{
}

/****f* stdp.c/allocStdpPostTSMem
*
* SUMMARY
*  The function allocates the memory for the STDP algoritm and sets this memory area to 0x00.
*
* SYNOPSIS
*  int* allocStdpPostTSMem (unsigned int numNeurons, unsigned int size)
*
* INPUTS
*  numNeurons: Number of neurons simulated in the core
*  size: size of the post-synaptic record for each neuron (in words)
*
* OUTPUTS
*  pointer to the allocated structure for the STDP (or -1 if an error occourred)
*
* SOURCE
*/

void alloc_stdp_post_TS_mem (unsigned int size)
{

#ifdef DEBUG_STDP_ALLOC
     io_printf ("Post-synaptic record size: %d\n", size);
#endif

     if (size < 2)
     {
          io_printf ("Post-synaptic record size must be greater than 1\n");
          while (1);
     }

     if ((PostTimeStamp = (int *)malloc(sizeof(int) * app_data.total_neurons * size))==NULL)          //time stamp for post-synaptic neurons
     {
          io_printf("memory allocation error\n");
          while (1);
     }

#ifdef DEBUG_STDP_ALLOC
     io_printf("PostTimeStamp = 0x%x, size = %d\n", PostTimeStamp, sizeof(int) * numNeurons * size);
#endif

     memset(PostTimeStamp, 0x00, sizeof(int) * numNeurons * size);
}
/*
*******/


/****f* stdp.c/StdpPostUpdate
*
* SUMMARY
*  The function updates the record of the post-synaptic spikes
*
* SYNOPSIS
*  int StdpPostUpdate (int* postTimeStampRecord, unsigned int size, unsigned int resolution, unsigned int neuronID, unsigned int currentSimTime)
*
* INPUTS
*  postTimeStampRecord: memory structure allocated from the routine allocStdpPostTSMem
*  size: size of the post-synaptic record for each neuron (in words)
*  resolution: time resolution for the STDP (expressed in bit: 0 = 1 msec resolution, 1 = 2 msec resolution, 2 = 4 msec resolution ...)
*  neuronID: ID of the neuron which emitted an action potential (spike)
*  currentSimTime: time of the simulation when the neuron emitted the spike
*
* OUTPUTS
*  -1: error in the execution
*   0: execution successful
*
* SOURCE
*/
int StdpPostUpdate (int* postTimeStampRecord, unsigned int size, unsigned int resolution, unsigned int neuronID, unsigned int currentSimTime)
{
     static unsigned int *shl_result = NULL;

     unsigned int *post_coarse, *post_fine;
     unsigned int sh_left;
     unsigned int resolution_bitmask = (0x01 << resolution) - 1;

#ifdef DEBUG_PRINT_STDP_POST_UPDATE
     int i;

     io_printf ("Inside c_STDP_post_Update. Time: 0x%8.8X, neuron: 0x%8.8X\n", currentSimTime, neuronID);
#endif

     if (shl_result == NULL)
     {
          if     ((shl_result = (unsigned int *)malloc(sizeof(unsigned int) * (size - 1)))==NULL)          //storage required for temp shifting
          {
               io_printf("memory allocation error\n");
               return -1;
          }

#ifdef DEBUG_PRINT_STDP_POST_UPDATE
          io_printf("shl_result = 0x%x, size = %d\n", shl_result, sizeof(unsigned int) * (size - 1));     
#endif

          memset(shl_result, 0x00, sizeof(unsigned int) * (size - 1));
     }

     post_coarse = (unsigned int*) postTimeStampRecord + (neuronID * size);
     post_fine = post_coarse + 1;

#ifdef DEBUG_PRINT_STDP_POST_UPDATE
     io_printf ("post_coarse: 0x%8.8X content: 0x%8.8X\n", post_coarse, *post_coarse);
     //io_printf ("post_fine: 0x%8.8X, content: 0x%8.8X%8.8X%8.8X%8.8X\n", post_fine, *post_fine, *(post_fine+1), *(post_fine+2), *(post_fine+3));
     io_printf ("post_fine: 0x%8.8X, content: 0x", post_fine);
     for (i=0; i< size; i++)
          io_printf ("%8.8X", post_fine[i]);
     io_printf("\n");
#endif

     sh_left = (currentSimTime - (*post_coarse)) >> resolution;

#ifdef DEBUG_PRINT_STDP_POST_UPDATE
     io_printf ("sh_left: 0x%8.8X\n", sh_left);
#endif
     *post_coarse = currentSimTime & ~resolution_bitmask;

#ifdef DEBUG_PRINT_STDP_POST_UPDATE
     io_printf ("post_coarse: 0x%8.8X\n", *post_coarse);
     //io_printf ("post_fine: 0x%8.8X, content: 0x%8.8X%8.8X%8.8X%8.8X\n", post_fine, *post_fine, *((post_fine)+1), *((post_fine)+2), *((post_fine)+3));
     io_printf ("post_fine: 0x%8.8X, content: 0x", post_fine);
     for (i=0; i< size; i++)
          io_printf ("%8.8X", post_fine[i]);
     io_printf("\n");
#endif

     shl(post_fine, shl_result, size-1, sh_left);
     *(shl_result + (size-2)) |= 0x01;

#ifdef DEBUG_PRINT_STDP_POST_UPDATE
     //io_printf ("shl_result: 0x%8.8X, content: 0x%8.8X%8.8X%8.8X%8.8X\n", &shl_result, shl_result, *((&shl_result)+1), *((&shl_result)+2), *((&shl_result)+3));
     io_printf ("shl_result: 0x%8.8X, content: 0x", &shl_result);
     for (i=0; i< size; i++)
          io_printf ("%8.8X", *((&shl_result)+i));
     io_printf("\n");
#endif

     memcpy(post_fine, &shl_result, (size-1)<<2);

#ifdef DEBUG_PRINT_STDP_POST_UPDATE
     //io_printf ("post_fine: 0x%8.8X, content: 0x%8.8X%8.8X%8.8X%8.8X\n", post_fine, *post_fine, *((post_fine)+1), *((post_fine)+2), *((post_fine)+3));
     io_printf ("post_fine: 0x%8.8X, content: 0x", post_fine);
     for (i=0; i< size; i++)
          io_printf ("%8.8X", post_fine[i]);
     io_printf("\n");
     io_printf ("End of c_STDP_post_Update\n");
#endif

     return 0;
}
/*
*******/


/****f* stdp.c/Stdp
*
* SUMMARY
*  The function evaluates updates the synaptic weights on the basis of the STDP learning rule.
*  At the end, the software writes the updated synaptic weight row to the SDRAM at the address
*  indicated by the first word of the buffer pointed by the start parameter.
*
* SYNOPSIS
*  int Stdp (unsigned int *start, unsigned int *end, unsigned int synapticBlockHeaderSize,
*             unsigned int *postTimeStampRecord, unsigned int size, unsigned int resolution,
*             unsigned int currentSimTime, unsigned char *STDP_Table, unsigned int max_wgt,
*             unsigned int min_wgt)
*
* INPUTS
*  start: pointer to the start address of the synaptic weight row
*  end: pointer to the first word free AFTER the synaptic weight row
*  synapticBlockHeaderSize: size (in words) of the synaptic row header: Number of words to
*                           "discard" before finding the actual first synaptic word
*  postTimeStampRecord: memory structure allocated from the routine allocStdpPostTSMem
*  size: size of the post-synaptic record for each neuron (in words)
*  resolution: time resolution for the STDP (expressed in bit: 0 = 1 msec resolution,
*              1 = 2 msec resolution, 2 = 4 msec resolution ...)
*  currentSimTime: time of the simulation when the neuron emitted the spike
*  STDP_Table: pointer to the weight modification table for the STDP algorithm. The address must
*              point at the central element of the table (the one indicating the modification
*              when Delta T = 0).
*  max_wgt: maximum weight allowed for the synapse
*  min_wgt: minimum weight allowed for the synapse
*
* OUTPUTS
*  -1: error in the execution
*   0: execution successful
*
* SOURCE
*/
int Stdp (unsigned int *start, unsigned int *end, unsigned int synapticBlockHeaderSize, unsigned int *postTimeStampRecord, unsigned int size, unsigned int resolution, unsigned int currentSimTime, unsigned char *STDP_Table, int max_wgt, int min_wgt)
{
     static unsigned int *shift_record = NULL;

     unsigned int *pre_coarse = start + 2;
     unsigned int *pre_fine = start + 3;

     unsigned int *ptr;

     int i, j, k;

     unsigned int resolution_bitmask, delay_bitmask;
     unsigned int shifted_out, coarse_sh_out;
     int sh_left;

     if (shift_record == NULL)
     {
          if     ((shift_record = (unsigned int *)malloc(sizeof(unsigned int) * 2 * (size - 1)))==NULL)          //storage required for temp shifting
          {
               io_printf("memory allocation error\n");
               return -1;
          }

#ifdef DEBUG_PRINT_STDP
          io_printf("shift_record = 0x%x, size = %d\n", shift_record, sizeof(int) * 2 * (size - 1));     
#endif

          memset(shift_record, 0x00, sizeof(int) * 2 * (size - 1));
     }

#ifdef DEBUG_PRINT_STDP
     io_printf ("Inside STDP. Src neuron ID: 0x%8.8X\n", *(start+1));
     io_printf ("Pre coarse: 0x%8.8X, Pre fine: 0x%8.8X\n", *pre_coarse, *pre_fine);
#endif

     resolution_bitmask = (0x01 << resolution) - 1;
     delay_bitmask = 0x0F & (~resolution_bitmask);

     sh_left = (currentSimTime - (*pre_coarse)) >> resolution;

#ifdef DEBUG_PRINT_STDP
          io_printf ("Shift left: %d\n", sh_left);
#endif

     if (sh_left < 32)
     {
          shifted_out = (*pre_fine) >> (32 - sh_left);
          coarse_sh_out = (currentSimTime & ~(resolution_bitmask)) - ((0x01 << resolution) * 32);
     }
     else
     {
          shifted_out = (*pre_fine);
          coarse_sh_out = (*pre_coarse);
     }

     (*pre_fine) = ((*pre_fine) << sh_left) | 0x01;
     (*pre_coarse) = currentSimTime & ~resolution_bitmask;

#ifdef DEBUG_PRINT_STDP
     io_printf ("New pre coarse: 0x%8.8X, New pre fine: 0x%8.8X\n", *pre_coarse, *pre_fine);
#endif

     for (ptr = start + synapticBlockHeaderSize + 1; ptr < end; ptr++)
     {
          int sh_align;

          unsigned char n = size-1; //number of words for the fine post record

          unsigned int *shift_record_left = shift_record;
          unsigned int *shift_record_right = shift_record + n;
          unsigned int *post_coarse, *post_fine;

          unsigned short int neuron, delay;
          int weight;

#ifdef DEBUG_PRINT_STDP
          io_printf ("Start synaptic word: 0x%8.8X\n", *ptr);
#endif

          delay = (*ptr >> 28) & 0x0F;
          neuron = (*ptr >> 16) & 0xFFF;
          weight =  *ptr & 0xFFFF;

#ifdef DEBUG_PRINT_STDP
          io_printf ("Time: %d, Destination neuron: 0x%8.8X, Delay: 0x%8.8X, Weight: 0x%8.8X\n", currentSimTime, neuron, delay, weight);
#endif

          post_coarse = ((unsigned int*) postTimeStampRecord) + (size * neuron);
          post_fine = post_coarse + 1;

#ifdef DEBUG_PRINT_STDP
          io_printf ("Post coarse: 0x%8.8X\n", *post_coarse);
          io_printf ("Post_fine content: 0x");
          for (i = 0; i < n; i++)
               io_printf ("%8.8X", post_fine[i]);
          io_printf ("\n");
#endif

          sh_align = ((int)coarse_sh_out - (int)(*post_coarse)) >> resolution; //arithmetic shift right

#ifdef DEBUG_PRINT_STDP
          io_printf ("Shift align: %d\n", sh_align);
#endif

          if (sh_align >= 0)
          {
               shl(post_fine, shift_record_left, n, sh_align);
               for (i=0; i<n; i++) shift_record_right[i] = 0;
          }
          else
          {
               shl(post_fine, shift_record_right, n, (n * 32) + sh_align);
               shr(post_fine, shift_record_left, n, -sh_align);
          }

#ifdef DEBUG_PRINT_STDP
          io_printf ("Shifted out: 0x%8.8X, Coarse shifted out: 0x%8.8X, Shift record Address: 0x%8.8X, size of shift record: %d\n", shifted_out, coarse_sh_out, shift_record, n*2);
          io_printf ("Shift record: 0x");
          for (i = 0; i < n*2; i++)
               io_printf ("%8.8X", shift_record[i]);
          io_printf ("\n");
#endif

          if (shifted_out != 0)
          {
               for (i = 0; shifted_out >> i; i++) //i index of pre-synaptic spikes
               {

#ifdef DEBUG_PRINT_STDP
                    io_printf ("Shifted pre-word: 0x%8.8X\n", (shifted_out >> i));
#endif
                    if ((shifted_out >> i & 1) != 0)
                    {
                         //trigger STDP for a pre-spike arriving at time i*resolution
                         //find post spike in the stdp window range
                         for (j = 2 * n - 1; j >= 0; j--) //j index of post-synaptic spikes
                         {

#ifdef DEBUG_PRINT_STDP
                              io_printf ("Post word considered: %d, Post word: 0x%8.8X\n", j, shift_record[j]);
#endif

                              if (shift_record[j] != 0)
                              {
                                   for (k = 0; shift_record[j] >> k; k++)
                                   {
                                        if ((shift_record[j] >> k & 1) != 0)
                                        {
#ifdef DEBUG_PRINT_STDP
                                             io_printf ("Pre index: %d, Post word index: %d, Post bit index: %d, Pre-time: %d, Pre-time con delay: %d, Post-time: %d, Old weight: %4.4X, ", i, j, k, coarse_sh_out - (i * (0x01 << resolution)), coarse_sh_out - (i * (0x01 << resolution)) + ((delay + 1) & delay_bitmask), coarse_sh_out - (((2 * n - 1 - j) * 32 + k) - (n*32)) * (0x01 << resolution), weight);
#endif

                                             weight = STDP_update (coarse_sh_out - (i * (0x01 << resolution)) + ((delay + 1) & delay_bitmask), coarse_sh_out - (((2 * n - 1 - j) * 32 + k) - (n*32)) * (0x01 << resolution), weight, resolution, STDP_Table, max_wgt, min_wgt); // include the delay and consider the two halves of shift_record // + 1 because first position in the bin [0] = 1 ms

#ifdef DEBUG_PRINT_STDP
                                             io_printf ("New weight: 0x%4.4X\n", weight);
#endif
                                        }
                                   }
                              }
                         }
                    }
               }
          }
          //reconstruct the synaptic word and store it back
          *ptr = (delay & 0x0F) << 28 | (neuron & 0xFFF) << 16 | (weight & 0xFFFF);
#ifdef DEBUG_PRINT_STDP
          io_printf ("End synaptic word: 0x%8.8X\n", *ptr);
#endif
     }

     return 0;
}
/*
*******/


/****f* stdp.c/STDP_update
*
* SUMMARY
*  This function computes the new synaptic weight on the pasis of the pre- and
*  post-synaptic spike times given the STDP weight modification table. The new
*  weight will be included in the range [max_wgt, min_wgt]
*
* SYNOPSIS
*  int STDP_update (int pre_syn_time, int post_syn_time, int old_wgt,
*                   unsigned int resolution, unsigned char *STDP_Table,
*                   unsigned int max_wgt, unsigned int min_wgt)
*
* INPUTS
*  pre_syn_time: time stamp of the pre-synaptic event
*  post_syn_time: time stamp of the post-synaptic event
*  old_wgt: weight of the synapse from which the pre-synaptic spike is arriving
*  time resolution for the STDP (expressed in bit: 0 = 1 msec resolution,
*              1 = 2 msec resolution, 2 = 4 msec resolution ...)
*  STDP_Table: pointer to the weight modification table for the STDP algorithm.
*              The address must point at the central element of the table
*              (the one indicating the modification when Delta T = 0).
*  max_wgt: maximum weight allowed for the synapse
*  min_wgt: minimum weight allowed for the synapse
*
* OUTPUTS
*  new synapse weight in the 2 LSB of the output value (no sign extension)
*
* SOURCE
*/
int STDP_update (int pre_syn_time, int post_syn_time, int old_wgt, unsigned int resolution, unsigned char *STDP_Table, int max_wgt, int min_wgt)
{
     int new_wgt;
     int t_diff = post_syn_time - pre_syn_time;

#ifdef DEBUG_PRINT_STDP_UPDATE
     io_printf ("Old Weight: 0x%8.8X, Time difference: %d\n", old_wgt, t_diff);
#endif

     if (t_diff > -32 && t_diff < 32)
     {
          short int d_wgt;

#ifdef DEBUG_PRINT_STDP_UPDATE
     io_printf ("STDP_Table address: 0x%8.8X, index: 0x%8.8X\n", (unsigned int)(&STDP_Table), (t_diff >> resolution));
#endif

          d_wgt = *(STDP_Table + (t_diff >> resolution));
          new_wgt = (t_diff < 0) ? (old_wgt - d_wgt) : (old_wgt + d_wgt);

#ifdef DEBUG_PRINT_STDP_UPDATE
          io_printf ("Weight modification: %d, new weight: %d\n", d_wgt, new_wgt);
#endif

          if (new_wgt > max_wgt << 8) new_wgt = max_wgt << 8;
          if (new_wgt < min_wgt << 8) new_wgt = min_wgt << 8;
     }
     else
          new_wgt = old_wgt;

#ifdef DEBUG_PRINT_STDP_UPDATE
          io_printf ("Final weight: %d\n", new_wgt);
#endif

     return new_wgt;
}
/*
*******/


/****f* stdp.c/shl
*
* SUMMARY
*  The function shifts left an array of words (32 bits) made up of size elements
*  by a number of bits identified by n. the output is placed in the dest array
*  which must be allocated by the caller function.
*
* SYNOPSIS
*  void shl (unsigned int* data, unsigned int* dest, unsigned int size, unsigned int n)
*
* INPUTS
*  data: array of words to be shifted
*  dest: array of words result of the shift operation (allocated by the caller)
*  size: number of elements in the array
*  n: number of bits by which data needs to be shifted
*
* OUTPUTS
*  dest: array of words containing the result of the shifting operation
*
* SOURCE
*/

void shl (unsigned int* data, unsigned int* dest, unsigned int size, unsigned int n)
{
     unsigned int i;
     unsigned int a, b;
     unsigned int temp1, temp2;

     for (i=0; i < size; i++)
          dest[i] = 0;

     a = n>>5;
     b = n & 0x1F; //n%32;

     for (i=(size-1)-a; i>=0; i--)
     {
          temp1 = data[i+a] << b;
          temp2 = (b!=0)?data[i+a] >> (32-b):0;

          dest[i] |= temp1;
          if (i-1>=0)
               dest[i-1] |= temp2;
     }
}
/*
*******/


/****f* stdp.c/shr
*
* SUMMARY
*  The function shifts right an array of words (32 bits) made up of size elements
*  by a number of bits identified by n. the output is placed in the dest array
*  which must be allocated by the caller function.
*
* SYNOPSIS
*  void shr (unsigned int* data, unsigned int* dest, unsigned int size, unsigned int n)
*
* INPUTS
*  data: array of words to be shifted
*  dest: array of words result of the shift operation (allocated by the caller)
*  size: number of elements in the array
*  n: number of bits by which data needs to be shifted
*
* OUTPUTS
*  dest: array of words containing the result of the shifting operation
*
* SOURCE
*/
void shr (unsigned int* data, unsigned int* dest, unsigned int size, unsigned int n)
{
     unsigned int i;
     unsigned int a, b;
     unsigned int temp1, temp2;

     for (i=0; i< size; i++)
          dest[i] = 0;

     a = n>>5;
     b = n & 0x1F; //n%32;

     for (i=0; i<size-a; i++)
     {
          temp1 = data[i] >> b;
          temp2 = (b!=0)?data[i] << (32-b):0;

          if (i+a < size)
               dest[i+a] |= temp1;
          if (i+a+1<size)
               dest[i+a+1] |= temp2;
     }
}
/*
*******/


void Stdp_Writeback(void * source, void * destination)
{
#ifdef DEBUG_PRINT_STDP
     io_printf ("Copying back the synaptic weight row to the SDRAM.\n");
#endif

     dma_transfer (0xFFFFFFFF,
                    source,
                    destination,
                    DMA_WRITE,
                    sizeof(synapse_row_t) + sizeof(uint) * synapse_row_length);

#ifdef DEBUG_PRINT_STDP
     io_printf ("STDP finished\n");
#endif
}

