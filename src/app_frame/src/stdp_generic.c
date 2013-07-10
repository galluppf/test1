/****a* stdp_generic.c/stdp_generic
*
* SUMMARY
*  Common routines required by the learning rules
*
* AUTHOR
*  Sergio Davies - daviess@cs.man.ac.uk
*
* DETAILS
*  Created on       : 30 May 2011
*  Version          : $Revision: 2170 $
*  Last modified on : $Date: 2013-04-10 15:07:45 +0100 (Wed, 10 Apr 2013) $
*  Last modified by : $Author: daviess $
*  $Id: stdp_generic.c 2170 2013-04-10 14:07:45Z daviess $
*  $HeadURL: https://solem.cs.man.ac.uk/svn/app_frame/tags/app_frame_testing_20130412_pre_capocaccia2013/src/stdp_generic.c $
*
* COPYRIGHT
*  Copyright (c) The University of Manchester, 2011. All rights reserved.
*  SpiNNaker Project
*  Advanced Processor Technologies Group
*  School of Computer Science
*
*******/

#include "spin1_api.h"
#include "spinn_io.h"
#include "stdp_generic.h"
#include "config.h"
#include "dma.h"

//#define DEBUG_PRINT_STDP_UPDATE
//#define DEBUG_PRINT_STDP_WRITEBACK 

extern stdp_table_t *stdp_table;

/****f* stdp_generic.c/shl
*
* SUMMARY
*  The function shifts left an array of words (32 bits) made up of -size- elements
*  by a number of bits identified by -n-. the output is placed in the -dest- array
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

void shl (unsigned int* data, unsigned int* dest, unsigned int size, int n)
{
    int i;
    unsigned int a, b;
    unsigned int temp1, temp2;

    if (n < 0)
        shr(data, dest, size, -n);
    else
    {
        for (i=0; i < size; i++)
            dest[i] = 0;

//        if (n < size * sizeof(unsigned int) * 8)
        if (n < (size << 5))
        {
            a = n>>5; // n >= 0 always!!!
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
    }
}

/*
*******/


/****f* stdp_generic.c/shr
*
* SUMMARY
*  The function shifts right an array of words (32 bits) made up of -size- elements
*  by a number of bits identified by -n-. the output is placed in the -dest- array
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

void shr (unsigned int* data, unsigned int* dest, unsigned int size, int n)
{
    int i;
    unsigned int a, b;
    unsigned int temp1, temp2;

    if (n < 0)
        shl(data, dest, size, -n);
    else
    {
        for (i=0; i < size; i++)
            dest[i] = 0;

//        if (n < size * sizeof(unsigned int) * 8)
        if (n < (size << 5))
        {
            a = n>>5; // n >= 0 always!!!
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
    }
}

/*
*******/

/****f* stdp_generic.c/STDP_update
*
* SUMMARY
*  This function computes the new synaptic weight on the pasis of the pre- and
*  post-synaptic spike times given the STDP weight modification table. The new
*  weight will be included in the range [max_wgt, min_wgt]
*
* SYNOPSIS
*  int STDP_update (int pre_syn_time, int post_syn_time, int old_wgt, unsigned char weight_scale)
*
* INPUTS
*  pre_syn_time: time stamp of the pre-synaptic event
*  post_syn_time: time stamp of the post-synaptic event
*  old_wgt: weight of the synapse from which the pre-synaptic spike is arriving
*  weight_scale: identifies the number of bits used to represent the
*                decimal part of the synaptic weight
*
* OUTPUTS
*  new synapse weight in the 2 LSB of the output value (no sign extension)
*
* SOURCE
*/

int STDP_update (int pre_syn_time, int post_syn_time, int old_wgt, unsigned char weight_scale)
{
    unsigned int resolution = stdp_table->log_resolution;
    short int max_wgt = stdp_table->max_weight;
    short int min_wgt = stdp_table->min_weight;

    int new_wgt;
    int t_diff = post_syn_time - pre_syn_time;

#ifdef DEBUG_PRINT_STDP_UPDATE
    io_printf (IO_STD, "Old Weight: 0x%08x, Time difference: %d\n", old_wgt, t_diff);
#endif

    if (t_diff >= -(stdp_table->ltd_time_window) && t_diff <= (stdp_table->ltp_time_window))
    {
        int d_wgt;

#ifdef DEBUG_PRINT_STDP_UPDATE
        io_printf (IO_STD, "STDP_Table address: 0x%08x, index: 0x%08x\n", (unsigned int)(&(stdp_table -> d_weight[128])), (t_diff >> resolution));
#endif

        d_wgt = ((int) (*((&(stdp_table -> d_weight[128])) + (t_diff >> resolution)))) << (weight_scale - 8);
        new_wgt = (t_diff < 0) ? (old_wgt - d_wgt) : (old_wgt + d_wgt);

#ifdef DEBUG_PRINT_STDP_UPDATE
        io_printf (IO_STD, "Weight modification: %d, new weight: %d\n", d_wgt, new_wgt);
#endif

        if (new_wgt > max_wgt) new_wgt = max_wgt;
        if (new_wgt < min_wgt) new_wgt = min_wgt;
    }
    else
        new_wgt = old_wgt;

#ifdef DEBUG_PRINT_STDP_UPDATE
        io_printf (IO_STD, "Final weight: %d\n", new_wgt);
#endif

    return new_wgt;
}

/*
*******/

/****f* stdp_generic.c/Stdp_Writeback
*
* SUMMARY
*  This function writes the synaptic weight row in sdram after the
*  learning algorithm modified the synaptic weights.
*   When the operation is completed, a DMA don interrupt is triggered using tag 0xFFFFFFFF
*
* SYNOPSIS
*  void Stdp_Writeback(void* dtcm_addr, void* sdram_addr, int row_size)
*
* INPUTS
*  dtcm_addr: address of the buffer of the synaptic weights in DTCM
*  sdram_addr: address of the synaptic block in SDRAM
*  row_size: size (in bytes - including the header words) of the synaptic block
*
* OUTPUTS
*  new synapse weight in the 2 LSB of the output value (no sign extension)
*
* SOURCE
*/

void Stdp_Writeback(void* dtcm_addr, void* sdram_addr, int row_size)
{
#ifdef DEBUG_PRINT_STDP_WRITEBACK
    io_printf (IO_STD, "Copying back the synaptic weight row to the SDRAM.\n");
    io_printf (IO_STD, "Sterting DMA request from dtcm address: %08x, to sdram address: %08x, size: %d bytes\n", (int) dtcm_addr, (int) sdram_addr, row_size);
#endif

    spin1_dma_transfer (0xFFFFFFFF,
                  sdram_addr,
                  dtcm_addr,
                  DMA_WRITE,
                  row_size);

#ifdef DEBUG_PRINT_STDP_WRITEBACK
    io_printf (IO_STD, "STDP finished\n");
#endif
}

/*
*******/

/****f* stdp_generic.c/spin1_memset
*
* SUMMARY
*  This function sets the first -num- bytes of the memory
*  area pointed by -start- to the value -value-.
*
* SYNOPSIS
*  void * spin1_memset ( void * start, int value, int num )
*
* INPUTS
*  start: pointer to the block of memory to fill
*  value: value to be set
*  num: number of bytes to be set to the value
*
* OUTPUTS
*  start is returned
*
* SOURCE
*/

void * spin1_memset ( void * start, int value, int num )
{
    unsigned char *ptr = start;

    while (num-- != 0)
        *ptr++ = (unsigned char) value;

    return start;
}

/*
*******/

//this function is used to outout synaptic weights to the ethernet when they change.
//Not currently in use. It is here only for debug purposes
#ifdef OUTPUT_WEIGHT

void send_weight (unsigned int currentSimTime, synaptic_row_t *row_address, unsigned int row_size)
{
    sdp_msg_t wgt_mod;

    wgt_mod.flags = 7;
    wgt_mod.tag = 1;
    wgt_mod.dest_port = 255;
    wgt_mod.srce_port = 1 << 5 | app_data.virtual_core_id;
    wgt_mod.dest_addr = 0;
    wgt_mod.srce_addr = spin1_get_chip_id();
    wgt_mod.cmd_rc = 0x101;
    wgt_mod.arg1 = row_size;
    wgt_mod.arg2 = currentSimTime;
    wgt_mod.arg3 = 0;
    wgt_mod.length = 26+row_size;

    spin1_memcpy (&(wgt_mod.data), row_address, row_size);

    spin1_send_sdp_msg (&wgt_mod, 100);
}

#endif

#ifdef __GNUC__
void raise()
{
    io_printf (IO_STD, "Division by zero in stdp_tts.c load_stdp_config_data()\n");
    io_printf (IO_STD, "Error: division by 0 - Terminating\n");
    while (1);
}
#endif

