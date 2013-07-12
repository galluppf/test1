/****a* stdp.h/stdp_header
*
* SUMMARY
*  implementation of standard STDP learning rule header file
*
* AUTHOR
*  Sergio Davies - daviess@cs.man.ac.uk
*
* DETAILS
*  Created on       : 30 May 2011
*  Version          : $Revision: 1225 $
*  Last modified on : $Date: 2011-06-30 19:26:19 -0600 (Thu, 30 Jun 2011) $
*  Last modified by : $Author: sergio $
*  $Id: stdp.h 1225 2011-07-01 01:26:19Z sergio $
*  $HeadURL: file:///media/drybar/app_frame/branch/pacman_modifications/inc/stdp.h $
*
* COPYRIGHT
*  Copyright (c) The University of Manchester, 2011. All rights reserved.
*  SpiNNaker Project
*  Advanced Processor Technologies Group
*  School of Computer Science
*
*******/

#ifndef __STDP_H__
#define __STDP_H__

//#define DEBUG_STDP_ALLOC
//#define DEBUG_PRINT_STDP_POST_UPDATE
//#define DEBUG_PRINT_STDP
//#define DEBUG_PRINT_STDP_UPDATE


int* allocStdpPostTSMem (unsigned int numNeurons, unsigned int size);
int StdpPostUpdate (int* postTimeStampRecord, unsigned int size, unsigned int resolution, unsigned int neuronID, unsigned int currentSimTime);
int Stdp (unsigned int *start, unsigned int *end, unsigned int synapticBlockHeaderSize, unsigned int *postTimeStampRecord, unsigned int size, unsigned int resolution, unsigned int currentSimTime, unsigned char *STDP_Table, int max_wgt, int min_wgt);
int STDP_update (int pre_syn_time, int post_syn_time, int old_wgt, unsigned int resolution, unsigned char *STDP_Table, int max_wgt, int min_wgt);
void shl (unsigned int* data, unsigned int* dest, unsigned int size, unsigned int n);
void shr (unsigned int* data, unsigned int* dest, unsigned int size, unsigned int n);
void Stdp_Writeback(void * source, void * destination);

#endif
