/****a* stdp_sp.c/stdp_sp
*
* SUMMARY
*  This file contains the functions related to the spike-pair
*  (or nearest-neighbour)STDP learning algorithm.
*  Synaptic weight update is computed after the time window closes for the
*  received spikes: once the spikes are received by the destination core,
*  the timestamp of the event is stored and, in the case earlier spikes are
*  out of the STDP time window, they may be processed for weight update.
*  The record of the post-synaptic spikes is stored using the following
*  data structure: one field represents the time of the last outgoing event
*  and it is always represented using 32 bits.
*  A second field represents the fine record using a bitmask to represent
*  when spike has been emitted. The structure of this field may use 32 bits
*  or more and is (using t to represented the coarse time stamp):
*
*          t-11  t-10  t-9   t-8   t-7   t-6   t-5   t-4   t-3   t-2   t-1    t
*        +-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+
*  (...) |  x  |  x  |  x  |  x  |  x  |  x  |  x  |  x  |  x  |  x  |  x  |  x  |
*        +-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+-----+
*   MSB                                                                      LSB
*
*  The history of the pre-synaptic spike involves only the last two events
*  which are held together with the synaptic information in SDRAM,
*  and fetched every time the pre-synaptic neuron emits a spike.
*  The synaptic weight block has 3 header words (each 32 bits) to represent
*  these information:
*    * Global pre-synaptic neuron ID (32 bits);
*    * Oldest time stamp for pre-synaptic neuron (32 bits);
*    * More recent timestamp for pre-synaptic neuron (32 bits);
*  The post-synaptic spike record is held in DTCM (one record for each neuron
*  simulated in the core) and the length of the fine time stamp record is
*  variable and set through a parameter passed by pacman.
*  When a spike is transmitted, the update of the correspondent spike
*  record involves two operations:
*    * update the fine timestamp: shift all the bitmask to the left by the amount
*      of milliseconds passed since last update, and write a "1" in the LSB;
*    * update the coarse time stamp with the current simulation time.
*  The update of the pre-synaptic record involves a three-step process:
*    * store the Oldest time stamp in a temporary variable, used for subsequent
*      computation;
*    * move the more recent time stamp to the Oldest time stamp field;
*    * store the current time stamp in the More reeecent time stamp field.
*  The spike-pair STDP is triggered every time a spike is received. However
*  the spike used as reference for this algorithm is the one which is placed
*  in the "Oldest" field, and for which the nearest neighbour is computed using
*  as limit boundaries the current simulation time (for the potentiation)
*  and the time stamp in the temporary field (for the depression). Only one
*  spike for each of the two sides is used for the synaptic weight update.
*
* AUTHOR
*  Sergio Davies - daviess@cs.man.ac.uk
*
* DETAILS
*  Created on       : 26 May 2011
*  Version          : $Revision: 2061 $
*  Last modified on : $Date: 2012-11-13 13:51:25 +0000 (Tue, 13 Nov 2012) $
*  Last modified by : $Author: daviess $
*  $Id: stdp_sp.c 2061 2012-11-13 13:51:25Z daviess $
*  $HeadURL: https://solem.cs.man.ac.uk/svn/app_frame/testing/src/stdp_sp.c $
*
* COPYRIGHT
*  Copyright (c) The University of Manchester, 2011. All rights reserved.
*  SpiNNaker Project
*  Advanced Processor Technologies Group
*  School of Computer Science
*
*******/

#ifdef STDP_SP

#include "spin1_api.h"
#include "spinn_io.h"

#include <string.h>
#include "stdp_generic.h"
#include "stdp_sp.h"
#include "config.h"
#include "dma.h"
#include "model_general.h"

//#define DEBUG_STDP_ALLOC
//#define DEBUG_PRINT_STDP_POST_UPDATE
//#define DEBUG_PRINT_STDP_S_P

unsigned int *shift_record = NULL;
unsigned int *shl_result = NULL;
int *PostTimeStamp = NULL;
extern app_data_t app_data;
stdp_table_t *stdp_table = NULL;

#ifdef TIMER_MEASURE
unsigned long long iteration_count=0, iterations_num=0;
#endif

/****f* stdp_sp.c/load_stdp_config_data
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

/****f* stdp_sp.c/alloc_stdp_post_TS_mem
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

/****f* stdp_tts.c/StdpPostUpdate
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
    unsigned int resolution = stdp_table->log_resolution;
    unsigned int *post_coarse, *post_fine;
    unsigned int sh_left;
    unsigned int resolution_bitmask = (0x01 << resolution) - 1;
    unsigned int GlobalTime = currentSimTime - 1;
    unsigned int size_ps_record = stdp_table->ps_record_words;

#ifdef DEBUG_PRINT_STDP_POST_UPDATE
    int i;

    io_printf (IO_STD, "Inside c_STDP_post_Update. Time: 0x%08x, neuron: 0x%08x\n", GlobalTime, neuronID);
#endif

    post_coarse = (unsigned int*) PostTimeStamp + (neuronID * size_ps_record);

#ifdef DEBUG_PRINT_STDP_POST_UPDATE
    io_printf (IO_STD, "post_coarse: 0x%08x content: 0x%08x\n", post_coarse, *post_coarse);
#endif

    post_fine = post_coarse + 1;

#ifdef DEBUG_PRINT_STDP_POST_UPDATE
    io_printf (IO_STD, "post_fine: 0x%08x, content: 0x", post_fine);
    for (i = 0; i < size_ps_record-1; i++)
        io_printf (IO_STD, "%08x", post_fine[i]);
    io_printf (IO_STD, "\n");
#endif

    sh_left = (GlobalTime - (*post_coarse)) >> resolution;

#ifdef DEBUG_PRINT_STDP_POST_UPDATE
    io_printf (IO_STD, "sh_left: 0x%08x\n", sh_left);
#endif
    *post_coarse = GlobalTime & ~resolution_bitmask;

#ifdef DEBUG_PRINT_STDP_POST_UPDATE
    io_printf (IO_STD, "post_coarse: 0x%08x\n", *post_coarse);
    io_printf (IO_STD, "post_fine: 0x%08x, content: 0x", post_fine);
    for (i = 0; i < size_ps_record-1; i++)
        io_printf (IO_STD, "%08x", post_fine[i]);
    io_printf (IO_STD, "\n");
#endif

    shl(post_fine, shl_result, size_ps_record-1, sh_left);
    *(shl_result + (size_ps_record-2)) |= 0x01;

#ifdef DEBUG_PRINT_STDP_POST_UPDATE
    io_printf (IO_STD, "shl_result: 0x%08x, content: 0x", shl_result);
    for (i = 0; i < size_ps_record-1; i++)
        io_printf (IO_STD, "%08x", *(shl_result+i));
    io_printf (IO_STD, "\n");
#endif

    spin1_memcpy(post_fine, shl_result, (size_ps_record-1)<<2);

#ifdef DEBUG_PRINT_STDP_POST_UPDATE
    io_printf (IO_STD, "post_fine: 0x%08x, content: 0x", post_fine);
    for (i = 0; i < size_ps_record-1; i++)
        io_printf (IO_STD, "%08x", post_fine[i]);
    io_printf (IO_STD, "\n");
#endif

#ifdef DEBUG_PRINT_STDP_POST_UPDATE
    io_printf (IO_STD, "End of c_STDP_post_Update\n");
#endif
}

/*
*******/

//void c_STDP_s_p(unsigned int* start, unsigned int* end, unsigned int syn_blk_header) // the resolution is in bits!!! eg. 2 ms resolution -> 1 bit, 4 msec resolution -> 2 bits

/****f* stdp_tts.c/Stdp
*
* SUMMARY
*  The function computes the synaptic weight update (where required)
*  following the spike pair (or nearest-neighbour) STDP rule. FInally
*  the weights updated are transferred back to SDRAM. See the description
*  of the stdp_sp algorithm for details.
*
* SYNOPSIS
*  void Stdp (synaptic_row_t *start, unsigned short int row_size, unsigned int currentSimTime)
*
* INPUTS
*  start: address of the synaptic weight block in DTCM
*  row_size: number of synapses to process
*  currentSimTime: simulation time
*
* OUTPUTS
*  NONE
*
* SOURCE
*/

void Stdp (synaptic_row_t *start, unsigned short int row_size, unsigned int currentSimTime)
{
    unsigned char resolution = stdp_table->log_resolution;
    unsigned char n = stdp_table -> ps_record_words - 1; //number of words for the fine post record
    unsigned int *shift_record_left = shift_record;
    unsigned int *shift_record_right = shift_record + n;
    unsigned int *pre_coarse1 = &(start -> coarse_stdp_time_constant);
    unsigned int *pre_coarse2 = &(start -> fine_stdp_time_constant);
    unsigned int temp_coarse;

    unsigned int CurrentTick = currentSimTime - 1;

    int i, j, k;

    unsigned int resolution_bitmask, delay_bitmask;



#ifdef DEBUG_PRINT_STDP_S_P
    io_printf (IO_STD, "Inside STDP. Src neuron ID: 0x%08x\n", start -> neuron_id);
    io_printf (IO_STD, "Pre coarse 1: 0x%08x, Pre coarse 2: 0x%08x\n", *pre_coarse1, *pre_coarse2);
#endif


    resolution_bitmask = (0x01 << resolution) - 1;
    delay_bitmask = 0x0F & (~resolution_bitmask);

    temp_coarse = *pre_coarse2;
    *pre_coarse2 = *pre_coarse1;
    *pre_coarse1 = CurrentTick & ~resolution_bitmask; 

#ifdef DEBUG_PRINT_STDP_S_P
    io_printf (IO_STD, "New pre coarse 1: 0x%08x, New pre coarse 2: 0x%08x, temp coarse: 0x%08x\n", *pre_coarse1, *pre_coarse2, temp_coarse);
#endif

    for (k = 0; k < row_size; k++)
    {
        int sh_align;

        unsigned int *post_coarse, *post_fine;

        synaptic_word_t decoded_word;

        decode_synaptic_word (start -> synapses[k], &decoded_word);

#ifdef DEBUG_PRINT_STDP_S_P
        io_printf (IO_STD, "Start synaptic word: 0x%08x\n", start -> synapses[k]);
#endif

        if (decoded_word.stdp_on)
        {

#ifdef DEBUG_PRINT_STDP_S_P
            io_printf (IO_STD, "Time: %d, Destination neuron: 0x%08x, Delay: 0x%08x, Weight: 0x%08x, Connector type: %08x\n", currentSimTime, decoded_word.index, decoded_word.delay, decoded_word.weight, decoded_word.synapse_type);
#endif

            post_coarse = ((unsigned int*) PostTimeStamp) + ((n + 1) * decoded_word.index);
            post_fine = post_coarse + 1;

#ifdef DEBUG_PRINT_STDP_S_P
            io_printf (IO_STD, "Post coarse: 0x%08x\nPost_fine content: 0x", *post_coarse);
            for (i = 0; i < n; i++)
                io_printf (IO_STD, "%08x", post_fine[i]);
            io_printf (IO_STD, "\n");
#endif

            sh_align = (int)(((*pre_coarse2) + ((decoded_word.delay + 1) & delay_bitmask)) - (unsigned int)(*post_coarse)) >> resolution; //arithmetic shift right

#ifdef DEBUG_PRINT_STDP_S_P
            io_printf (IO_STD, "Shift align: %d\n", sh_align);
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

#ifdef DEBUG_PRINT_STDP_S_P
            io_printf (IO_STD, "Pre Coarse 2: 0x%08x, Shift record Address: 0x%08x, size of shift record: %d\n", *pre_coarse2, shift_record, n*2);
            io_printf (IO_STD, "Shift record: 0x");
            for (i = 0; i < n*2; i++)
                io_printf (IO_STD, "%08x", shift_record[i]);
            io_printf (IO_STD, "\n");
#endif

            //compute LTP
            for (i = 0; i < n; i++)
            {
                int found = 0;

#ifdef DEBUG_PRINT_STDP_S_P
                io_printf (IO_STD, "Post word (right) considered: %d, Post word: 0x%08x\n", i, shift_record_right[i]);
#endif

                if (shift_record_right[i] != 0)
                {
                    for (j = 31; j >= 0; j--)
                    {

#ifdef DEBUG_PRINT_STDP_S_P
                        io_printf (IO_STD, "shift_record_right[%d]: 0x%08x\n", i, (shift_record_right[i] >> j));
#endif

                        if (shift_record_right[i] >> j & 0x01 == 1)
                        {
                            unsigned int post_time = (*pre_coarse2) + (decoded_word.delay & delay_bitmask) + ((32 - j) * (0x01 << resolution)) + (32 * i * (0x01 << resolution));

#ifdef DEBUG_PRINT_STDP_S_P
                            io_printf (IO_STD, "First spike considered: %d, Post-synaptic spike time: %d\n", j, post_time);
#endif

                        //here: compute stdp with this spike!
                            if ( post_time <= (*pre_coarse1))
                            {

#ifdef DEBUG_PRINT_STDP_S_P
                                io_printf (IO_STD, "Shifted record right word index: %d, Post bit index: %d, Pre-time: %d, Pre-time with delay: %d, Post-time: %d, Old weight: %04x, ", i, j, *pre_coarse2, *pre_coarse2 + (decoded_word.delay & delay_bitmask), post_time, decoded_word.weight);
#endif

                                decoded_word.weight = STDP_update ((*pre_coarse2) + (decoded_word.delay & delay_bitmask), post_time, decoded_word.weight, decoded_word.weight_scale);

#ifdef DEBUG_PRINT_STDP_S_P
                                io_printf (IO_STD, "New weight: 0x%04x\n", decoded_word.weight);
#endif

                            }

                            found = 1;
                            break;
                        }
                    }
                    if (found == 1) break;
                }
            }

            start -> synapses[k] = encode_synaptic_word (&decoded_word);

            //compute LTD
            for (i = n-1; i >= 0; i--)
            {
                int found = 0;

#ifdef DEBUG_PRINT_STDP_S_P
                io_printf (IO_STD, "Post word (left) considered: %d, Post word: 0x%08x\n", i, shift_record_left[i]);
#endif

                if (shift_record_left[i] != 0)
                {

                    for (j = 0; j < 32; j++)
                    {

#ifdef DEBUG_PRINT_STDP_S_P
                        io_printf (IO_STD, "Bit %d of shift_record_left[%d]: 0x%08x\n", j,  i, (shift_record_left[i] >> j) & 0x01);
#endif

                        if (shift_record_left[i] >> j & 0x01 == 1)
                        {
                            unsigned int post_time = (*pre_coarse2) + ((decoded_word.delay + 1) & delay_bitmask) - (j * (0x01 << resolution)) - (32 * (n - 1 - i) * (0x01 << resolution));

#ifdef DEBUG_PRINT_STDP_S_P
                            io_printf (IO_STD, "First spike considered: %d, Post-synaptic spike time: %d\n", j, post_time);
#endif


                            //here: compute stdp with this spike!
                            if (post_time >= temp_coarse)
                            {

#ifdef DEBUG_PRINT_STDP_S_P
                                io_printf (IO_STD, "Shifted record left word index: %d, Post bit index: %d, Pre-time: %d, Pre-time with delay: %d, Post-time: %d, Old weight: %04x, ", i, j, *pre_coarse2, *pre_coarse2 + (decoded_word.delay & delay_bitmask), post_time, decoded_word.weight);
#endif

                                decoded_word.weight = STDP_update ((*pre_coarse2) + (decoded_word.delay & delay_bitmask), post_time, decoded_word.weight, decoded_word.weight_scale);

#ifdef DEBUG_PRINT_STDP_S_P
                                io_printf (IO_STD, "New weight: 0x%04x\n", decoded_word.weight);
#endif

                            }

                            found = 1;
                            break;
                        }
                    }
                    if (found == 1) break;
                }
            }

            //reconstruct the synaptic word and store it back
            start -> synapses[k] = encode_synaptic_word (&decoded_word);
#ifdef DEBUG_PRINT_STDP_S_P
            io_printf (IO_STD, "End synaptic word: 0x%08x\n", start -> synapses[k]);
#endif
        }
    }

#ifdef DEBUG_PRINT_STDP_S_P
    io_printf (IO_STD, "STDP finished\n");
#endif
}

/*
*******/

#endif //#ifdef STDP_SP

