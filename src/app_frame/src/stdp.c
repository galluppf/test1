/****a* stdp.c/stdp
*
* SUMMARY
*  This file contains the functions related to the standard STDP learning
*  algorithm. The idea behind the implementation of the learning algorithm
*  is described in the paper "Implementing Spike-Timing-Dependent Plasticity
*  on SpiNNaker Neuromorphic Hardware" by Xin Jin, Alexander Rast, Francesco
*  Galluppi, Sergio Davies and Steve Furber and its name is "Deferred Event Model".
*  Synaptic weight update is computed after the time window closes for the
*  received spikes: once the spikes are received by the destination core,
*  the timestamp of the event is stored and, in the case earlier spikes are
*  out of the STDP time window, they may be processed for weight update.
*  The record of the spikes (both pre-synaptic and post-synaptic) is stored
*  using the same data structure: one field represents the time of the last
*  event (either incoming or outgoing) and it is always represented using 32 bits.
*  A second field represents the fine record using a bitmask to represent
*  when spike has been received/transmitted. The structure of this field may
*  use 32 bits or more and is (given t the time represented by the coarse
*  time stamp):
*
*          t-11  t-10  t-9   t-8   t-7   t-6   t-5   t-4   t-3   t-2   t-1    t
*        +-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+
*  (...) |  x  |  x  |  x  |  x  |  x  |  x  |  x  |  x  |  x  |  x  |  x  |  x  |
*        +-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+
*   MSB                                                                      LSB
*
*  The pre-synaptic spike record is held together with the synaptic
*  information in SDRAM, and fetched every time the pre-synaptic neuron emits
*  a spike.
*  The synaptic weight block has 3 header words (each 32 bits) to represent
*  these information:
*    * Global pre-synaptic neuron ID (32 bits);
*    * Coarse time stamp for pre-synaptic neuron (32 bits);
*    * Fine timestamp for pre-synaptic neuron (32 bits);
*  The post-synaptic spike record is held in DTCM (one record for each neuron
*  simulated in the core) and the length of the fine time stamp record is
*  variable and set through a parameter passed by pacman.
*  When a spike is received or transmitted, the update of the correspondent spike
*  record involves two operations:
*    * update the fine timestamp: shift all the bitmask to the left by the amount
*      of milliseconds passed since last update, and write a "1" in the LSB;
*    * update the coarse time stamp with the current simulation time.
*  When a pre-synaptic spike record is shifted left and one or more spikes are
*  pushed beyond the boundary of the record, the synaptic weight update algorithm
*  is triggered with respect to the history of all the outgoing spikes that have
*  been stored (in the meanwhile) in the post-synaptic record.
*
* AUTHOR
*  Sergio Davies - daviess@cs.man.ac.uk
*
* DETAILS
*  Created on       : 26 May 2011
*  Version          : $Revision: 2049 $
*  Last modified on : $Date: 2012-11-08 13:09:33 +0000 (Thu, 08 Nov 2012) $
*  Last modified by : $Author: daviess $
*  $Id: stdp.c 2049 2012-11-08 13:09:33Z daviess $
*  $HeadURL: https://solem.cs.man.ac.uk/svn/app_frame/testing/src/stdp.c $
*
* COPYRIGHT
*  Copyright (c) The University of Manchester, 2011. All rights reserved.
*  SpiNNaker Project
*  Advanced Processor Technologies Group
*  School of Computer Science
*
*******/

#ifdef STDP

#include "spin1_api.h"
#include "spinn_io.h"

#include "stdp_generic.h"
#include "stdp.h"
#include "config.h"
#include "dma.h"
#include "model_general.h"

//#define DEBUG_STDP_ALLOC
//#define DEBUG_PRINT_STDP_POST_UPDATE
//#define DEBUG_PRINT_STDP

unsigned int *shift_record = NULL;
unsigned int *shl_result = NULL;
int *PostTimeStamp = NULL;
extern app_data_t app_data;
stdp_table_t *stdp_table = NULL;

/****f* stdp.c/load_stdp_config_data
*
* SUMMARY
*  The function allocates memory for the STDP
*  data structure, as defined in stdp_table_t struct,
*  and copies the data from the SDRAM location where
*  it was loaded to the allocated memory are in DTCM.
*  For a complete memory map refer to config.c file
*
* SYNOPSIS
*  void load_stdp_config_data (void)
*
* INPUTS
*  NONE
*
* OUTPUTS
*  NONE
*
* SOURCE
*/

void load_stdp_config_data (void)
{
    unsigned int src;

    stdp_table = (stdp_table_t *) spin1_malloc (sizeof(stdp_table_t));
    if (stdp_table == NULL)
    {
         io_printf(IO_STD, "memory allocation error\n");
         while (1); //TODO: stop simulation
    }
    src = STDP_START_ADDR + STDP_BLOCK_SIZE * (spin1_get_core_id() - 1);
    spin1_memcpy ((void*) stdp_table, (void*)src, sizeof(stdp_table_t));
}

/*
*******/

/****f* stdp.c/alloc_stdp_post_TS_mem
*
* SUMMARY
*  The function allocates the memory for the STDP algoritm and sets
*  this memory area to 0x00. The memory allocated is used to store:
*    * Post-synaptic timestamp (PostTimeStamp)
*    * Two temporary variables used in the algorithm (shift_record and shl_result)
*
* SYNOPSIS
*  int alloc_stdp_post_TS_mem (void)
*
* INPUTS
*  NONE
*
* OUTPUTS
*   0: function completed succesfully
*  -1: memory allocation error
*
* SOURCE
*/

int alloc_stdp_post_TS_mem (void)
{
    int size = stdp_table -> ps_record_words;
    int numNeurons = app_data.total_neurons;

#ifdef DEBUG_STDP_ALLOC
    io_printf (IO_STD, "Post-synaptic record size: %d\n", size);
    io_printf (IO_STD, "Number of neurons: %d\n", app_data.total_neurons);
#endif

    if (size < 2)
    {
        io_printf (IO_STD, "Post-synaptic record size must be greater than 1\n");
        return -1;
    }

    if ((PostTimeStamp = (int *)spin1_malloc(sizeof(int) * app_data.total_neurons * size))==NULL)          //time stamp for post-synaptic neurons
    {
        io_printf(IO_STD, "memory allocation error\n");
        return -1;
    }

#ifdef DEBUG_STDP_ALLOC
    io_printf(IO_STD, "PostTimeStamp = 0x%08x, size = %d\n", PostTimeStamp, sizeof(int) * numNeurons * size);
#endif

    spin1_memset(PostTimeStamp, 0x00, sizeof(int) * numNeurons * size);

    if ((shift_record = (unsigned int *)spin1_malloc(sizeof(unsigned int) * 2 * (size - 1)))==NULL)          //storage required for temp shifting
    {
        io_printf(IO_STD, "memory allocation error\n");
        return -1;
    }

#ifdef DEBUG_STDP_ALLOC
    io_printf(IO_STD, "shift_record = 0x%08x, size = %d\n", shift_record, sizeof(int) * 2 * (size - 1));     
#endif

    spin1_memset(shift_record, 0x00, sizeof(int) * 2 * (size - 1));

    if ((shl_result = (unsigned int *)spin1_malloc(sizeof(unsigned int) * (size - 1)))==NULL)          //storage required for temp shifting
    {
        io_printf(IO_STD, "memory allocation error\n");
        return -1;
    }

    spin1_memset(shl_result, 0x00, sizeof(unsigned int) * (size - 1));

#ifdef DEBUG_STDP_ALLOC
    io_printf(IO_STD, "shl_result = 0x%08x, size = %d\n", shl_result, sizeof(unsigned int) * (size - 1));     
#endif

    return 0;
}

/*
*******/


/****f* stdp.c/StdpPostUpdate
*
* SUMMARY
*  The function updates the record of the last spike emitted
*  by the post-synaptic neuron. The function stores the information
*  in the PostTimeStamp array. See description of the stdp_sp algorithm
*  for details.
*
* SYNOPSIS
*  void StdpPostUpdate (unsigned int neuronID, unsigned int currentSimTime)
*
* INPUTS
*  neuronID: ID of the neuron emitting a spike. In the case of multiple
*            populations inside a single core, this is a sequentionl number
*            that starts from 0 (firt neuron of the first population) and
*            increments for all the neurons in all the populations in the core
*  currentSimTime: current simulation time
*
* OUTPUTS
*  NONE
*
* SOURCE
*/

void StdpPostUpdate (unsigned int neuronID, unsigned int currentSimTime)
{
    unsigned int size = stdp_table->ps_record_words;
    unsigned int resolution = stdp_table->log_resolution;

    unsigned int *post_coarse, *post_fine;
    unsigned int sh_left;
    unsigned int resolution_bitmask = (0x01 << resolution) - 1;

#ifdef DEBUG_PRINT_STDP_POST_UPDATE
    int i;
#endif

#ifdef DEBUG_PRINT_STDP_POST_UPDATE
    io_printf (IO_STD, "shl_result: 0x%08x, content: 0x", shl_result);
    for (i=0; i< size-1; i++)
        io_printf (IO_STD, "%08x", shl_result[i]);
    io_printf(IO_STD, "\n");
#endif

#ifdef DEBUG_PRINT_STDP_POST_UPDATE
    io_printf (IO_STD, "Inside c_STDP_post_Update. Time: 0x%08x, neuron: 0x%08x\n", currentSimTime, neuronID);
#endif

    post_coarse = (unsigned int*) PostTimeStamp + (neuronID * size);
    post_fine = post_coarse + 1;

#ifdef DEBUG_PRINT_STDP_POST_UPDATE
    io_printf (IO_STD, "post_coarse: 0x%08x content: 0x%08x\n", post_coarse, *post_coarse);
    //io_printf (IO_STD, "post_fine: 0x%08x, content: 0x%08x%08x%08x%08x\n", post_fine, *post_fine, *(post_fine+1), *(post_fine+2), *(post_fine+3));
    io_printf (IO_STD, "post_fine: 0x%08x, content: 0x", post_fine);
    for (i=0; i< size-1; i++)
        io_printf (IO_STD, "%08x", post_fine[i]);
    io_printf(IO_STD, "\n");
#endif

    sh_left = (currentSimTime - (*post_coarse)) >> resolution;

#ifdef DEBUG_PRINT_STDP_POST_UPDATE
    io_printf (IO_STD, "sh_left: 0x%08x\n", sh_left);
#endif
    *post_coarse = currentSimTime & ~resolution_bitmask;

#ifdef DEBUG_PRINT_STDP_POST_UPDATE
    io_printf (IO_STD, "post_coarse: 0x%08x\n", *post_coarse);
    io_printf (IO_STD, "post_fine: 0x%08x, content: 0x", post_fine);
    for (i=0; i< size-1; i++)
        io_printf (IO_STD, "%08x", post_fine[i]);
    io_printf(IO_STD, "\n");
#endif

    shl(post_fine, shl_result, size-1, sh_left);
    shl_result[size-2] |= 0x01;

#ifdef DEBUG_PRINT_STDP_POST_UPDATE
    io_printf (IO_STD, "shl_result: 0x%08x, content: 0x", shl_result);
    for (i=0; i< size-1; i++)
        io_printf (IO_STD, "%08x", shl_result[i]);
    io_printf(IO_STD, "\n");
#endif

    spin1_memcpy(post_fine, shl_result, (size-1)<<2);

#ifdef DEBUG_PRINT_STDP_POST_UPDATE
    //io_printf (IO_STD, "post_fine: 0x%08x, content: 0x%08x%08x%08x%08x\n", post_fine, *post_fine, *((post_fine)+1), *((post_fine)+2), *((post_fine)+3));
    io_printf (IO_STD, "post_fine: 0x%08x, content: 0x", post_fine);
    for (i=0; i< size-1; i++)
        io_printf (IO_STD, "%08x", post_fine[i]);
    io_printf(IO_STD, "\n");
    io_printf (IO_STD, "End of c_STDP_post_Update\n");
#endif
}

/*
*******/


/****f* stdp.c/Stdp
*
* SUMMARY
*  The function evaluates updates the synaptic weights on the basis of the STDP learning rule.
*  At the end, the software writes the updated synaptic weight row to the SDRAM at the address
*  from which it was fetched.
*
* SYNOPSIS
*  void Stdp (synaptic_row_t *start, unsigned short int row_size, unsigned int currentSimTime)
*
* INPUTS
*  start: pointer to the start address of the synaptic weight row
*  row_size: size of the post-synaptic record for each neuron (in words)
*  currentSimTime: time of the simulation when the spike is received at the destination core
*
* OUTPUTS
*  NULL
*
* SOURCE
*/

void Stdp (synaptic_row_t *start, unsigned short int row_size, unsigned int currentSimTime)
{
    unsigned int size = stdp_table->ps_record_words;
    unsigned int resolution = stdp_table->log_resolution;

    unsigned int *pre_coarse = &(start -> coarse_stdp_time_constant);
    unsigned int *pre_fine = &(start -> fine_stdp_time_constant);

    unsigned char n = size-1; //number of words for the fine post record
    unsigned int *shift_record_left = shift_record;
    unsigned int *shift_record_right = shift_record + n;

    unsigned short int counter;

    int i, j, k;

    unsigned int resolution_bitmask, delay_bitmask;
    unsigned int shifted_out, coarse_sh_out;
    int sh_left;

#ifdef DEBUG_PRINT_STDP
    io_printf (IO_STD, "Inside STDP. Src neuron ID: 0x%08x\n", start -> neuron_id);
    io_printf (IO_STD, "Pre coarse: 0x%08x, Pre fine: 0x%08x\n", *pre_coarse, *pre_fine);
#endif

    resolution_bitmask = (0x01 << resolution) - 1;
    delay_bitmask = 0x0F & (~resolution_bitmask);

    sh_left = (currentSimTime - (*pre_coarse)) >> resolution;

#ifdef DEBUG_PRINT_STDP
    io_printf (IO_STD, "Shift left: %d\n", sh_left);
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
    io_printf (IO_STD, "New pre coarse: 0x%08x, New pre fine: 0x%08x\n", *pre_coarse, *pre_fine);
#endif

//    for (ptr = start + synapticBlockHeaderSize + 1; ptr < end; ptr++)
    for (counter = 0; counter < row_size; counter++)
    {
        int sh_align;

        unsigned int *post_coarse, *post_fine;

        synaptic_word_t decoded_word;

        decode_synaptic_word (start -> synapses[counter], &decoded_word);

#ifdef DEBUG_PRINT_STDP
        io_printf (IO_STD, "Synaptic word analysed: 0x%08x\n", start -> synapses[counter]);
#endif

        if (decoded_word.stdp_on)
        {

#ifdef DEBUG_PRINT_STDP
            io_printf (IO_STD, "Time: %d, Destination neuron: 0x%08x, Delay: 0x%08x, Weight: 0x%08x, Connector type: %08x\n", currentSimTime, decoded_word.index, decoded_word.delay, decoded_word.weight, decoded_word.synapse_type);
#endif

            post_coarse = ((unsigned int*) PostTimeStamp) + (size * decoded_word.index);
            post_fine = post_coarse + 1;

#ifdef DEBUG_PRINT_STDP
            io_printf (IO_STD, "Post coarse: 0x%08x\n", *post_coarse);
            io_printf (IO_STD, "Post_fine content: 0x");
            for (i = 0; i < n; i++)
                io_printf (IO_STD, "%08x", post_fine[i]);
            io_printf (IO_STD, "\n");
#endif

            sh_align = ((int)coarse_sh_out - (int)(*post_coarse)) >> resolution; //arithmetic shift right

#ifdef DEBUG_PRINT_STDP
            io_printf (IO_STD, "Shift align: %d\n", sh_align);
#endif

            if (sh_align >= 0)
            {
                shl(post_fine, shift_record_left, n, sh_align);

#ifdef DEBUG_PRINT_STDP
                io_printf (IO_STD, "Shift record left: 0x");
                for (i = 0; i < n; i++)
                    io_printf (IO_STD, "%08x", shift_record_left[i]);
                io_printf (IO_STD, "\n");
#endif

                for (i=0; i<n; i++) shift_record_right[i] = 0;

#ifdef DEBUG_PRINT_STDP
                io_printf (IO_STD, "Shift record right: 0x");
                for (i = 0; i < n; i++)
                    io_printf (IO_STD, "%08x", shift_record_right[i]);
                io_printf (IO_STD, "\n");
#endif

            }
            else
            {
                shl(post_fine, shift_record_right, n, (n * 32) + sh_align);

#ifdef DEBUG_PRINT_STDP
                io_printf (IO_STD, "Shift record right: 0x");
                for (i = 0; i < n; i++)
                    io_printf (IO_STD, "%08x", shift_record_right[i]);
                io_printf (IO_STD, "\n");
#endif

                shr(post_fine, shift_record_left, n, -sh_align);

#ifdef DEBUG_PRINT_STDP
                io_printf (IO_STD, "Shift record left: 0x");
                for (i = 0; i < n; i++)
                    io_printf (IO_STD, "%08x", shift_record_left[i]);
                io_printf (IO_STD, "\n");
#endif
            }

#ifdef DEBUG_PRINT_STDP
            io_printf (IO_STD, "Shifted out: 0x%08x, Coarse shifted out: 0x%08x, Shift record Address: 0x%08x, Size of shift record: %d\n", shifted_out, coarse_sh_out, shift_record, n*2);
            io_printf (IO_STD, "Shift record: 0x");
            for (i = 0; i < n*2; i++)
                io_printf (IO_STD, "%08x", shift_record[i]);
            io_printf (IO_STD, "\n");
#endif

            if (shifted_out != 0)
            {
                for (i = 0; shifted_out >> i; i++) //i index of pre-synaptic spikes
                {

#ifdef DEBUG_PRINT_STDP
                    io_printf (IO_STD, "Shifted pre-word: 0x%08x\n", (shifted_out >> i));
#endif
                    if ((shifted_out >> i & 1) != 0)
                    {
                        //trigger STDP for a pre-spike arriving at time i*resolution
                        //find post spike in the stdp window range
                        for (j = 2 * n - 1; j >= 0; j--) //j index of post-synaptic spikes
                        {
#ifdef DEBUG_PRINT_STDP
                            io_printf (IO_STD, "Post word considered: %d, Post word: 0x%08x\n", j, shift_record[j]);
#endif

                            if (shift_record[j] != 0)
                            {
                                for (k = 0; shift_record[j] >> k; k++)
                                {
                                    if ((shift_record[j] >> k & 1) != 0)
                                    {
#ifdef DEBUG_PRINT_STDP
                                        io_printf (IO_STD, "Pre index: %d, Post word index: %d, Post bit index: %d, Pre-time: %d, Pre-time con delay: %d, Post-time: %d, Old weight: %04x, ", i, j, k, coarse_sh_out - (i * (0x01 << resolution)), coarse_sh_out - (i * (0x01 << resolution)) + (decoded_word.delay & delay_bitmask), coarse_sh_out - (((2 * n - 1 - j) * 32 + k) - (n*32)) * (0x01 << resolution), decoded_word.weight);
#endif

                                        decoded_word.weight = STDP_update (coarse_sh_out - (i * (0x01 << resolution)) + (decoded_word.delay & delay_bitmask), coarse_sh_out - (((2 * n - 1 - j) * 32 + k) - (n*32)) * (0x01 << resolution), decoded_word.weight, decoded_word.weight_scale); // include the delay and consider the two halves of shift_record // + 1 because first position in the bin [0] = 1 ms

#ifdef DEBUG_PRINT_STDP
                                        io_printf (IO_STD, "New weight: 0x%04x\n", decoded_word.weight);
#endif
                                    }
                                }
                            }
                        }
                    }
                }
            }
            //reconstruct the synaptic word and store it back
            start -> synapses[counter] = encode_synaptic_word (&decoded_word);

#ifdef DEBUG_PRINT_STDP
            io_printf (IO_STD, "End synaptic word: 0x%08x\n", start -> synapses[counter]);
#endif
        }

    }

#ifdef DEBUG_PRINT_STDP
    io_printf (IO_STD, "STDP finished\n");
#endif
}
/*
*******/

#endif //#ifdef STDP

