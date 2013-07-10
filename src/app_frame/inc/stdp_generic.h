/****a* stdp_generic.h/stdp_header
*
* SUMMARY
*  Header file for all the structures required for learning rules
*
* AUTHOR
*  Sergio Davies - daviess@cs.man.ac.uk
*
* DETAILS
*  Created on       : 30 May 2011
*  Version          : $Revision: 2066 $
*  Last modified on : $Date: 2012-11-13 17:25:32 +0000 (Tue, 13 Nov 2012) $
*  Last modified by : $Author: daviess $
*  $Id: stdp_generic.h 2066 2012-11-13 17:25:32Z daviess $
*  $HeadURL: https://solem.cs.man.ac.uk/svn/app_frame/testing/inc/stdp_generic.h $
*
* COPYRIGHT
*  Copyright (c) The University of Manchester, 2011. All rights reserved.
*  SpiNNaker Project
*  Advanced Processor Technologies Group
*  School of Computer Science
*
*******/

#ifndef __STDP_GENERIC_H__
#define __STDP_GENERIC_H__

#define STDP_START_ADDR 0x74300000
#define STDP_BLOCK_SIZE 0x200

#include "dma.h"

/****s* stdp_generic.h/stdp_table_t
*
* SUMMARY
*  This structure holds all the data relevant for the learning rules.
*  Different structures are allocated for STDP, spike-pair STDP and STDP TTS
*
* FIELDS
*  min_weight: Minimum synaptic weight allowed
*  max_weight: Maximum synaptic weight allowed
*  ltp_time_window: long term potentiation time window (in msec)
*  ltd_time_window: long term depotentiation time window (in msec)
*  resolution: resolution (in msec) of the time stamp record (may be only power of 2: 1, 2, 4, 8...)
*  log_resolution: number of bits not to consider according to resolution:
*                  the value of this variable must be setting according to the
*                  value of the variable resolution:
*                  resolution value -> log_resolution value
*                  1                -> 0
*                  2                -> 1
*                  4                -> 2
*                  8                -> 3
*                  (...)
*  ps_record_words: number of words for the post stynaptic time record
*  d_weight: synaptic weight variation value. This value is represented in 0.8 fixed
*            point arithmetic - the bits represent only decimal values this array is
*            used for the STDP update rule and the case in which pre and post synaptic
*            spike occur simultaneously corresponds the the element d_weight[128].
*            The elements before this represent the amount of depression (tpre > tpost).
*            The elements d_weight[129] to d_weight [256] represent th amount of
*            potentiation (tpost > tpre).
*  L_parameter (only for STDP TTS rule): is the L parameter as described in the paper
*            "A forecast-based STDP rule suitable for neuromorphic implementation" by 
*            Sergio Davies, Alexander Rast, Francesco Galluppi and Steve Furber
*
* SOURCE
*/

#if defined STDP || defined STDP_SP
typedef struct
{
    short int min_weight;
    short int max_weight;
    unsigned char ltp_time_window;
    unsigned char ltd_time_window;
    unsigned char resolution;
    unsigned char log_resolution;
    unsigned char ps_record_words;
    unsigned char d_weight[257];
} stdp_table_t;
#endif

#if defined STDP_TTS
typedef struct
{
    short int min_weight;
    short int max_weight;
    unsigned char ltp_time_window;
    unsigned char ltd_time_window;
    unsigned char resolution;
    unsigned char log_resolution;
    unsigned char ps_record_words;
    unsigned char d_weight[257];
    short int L_parameter;
} stdp_table_t;
#endif

/*
*******/
void shl (unsigned int* data, unsigned int* dest, unsigned int size, int n);
void shr (unsigned int* data, unsigned int* dest, unsigned int size, int n);
int STDP_update (int pre_syn_time, int post_syn_time, int old_wgt, unsigned char weight_scale);
void Stdp_Writeback (void * source, void * destination, int row_size);
void* spin1_memset(void *s, int c, int n);

#ifdef __GNUC__
void raise();
#endif

#ifdef OUTPUT_WEIGHT
void send_weight (unsigned int currentSimTime, synaptic_row_t *row_address, unsigned int row_size);
#endif

#endif
