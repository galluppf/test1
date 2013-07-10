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
*  Version          : $Revision: 2046 $
*  Last modified on : $Date: 2012-11-07 16:18:47 +0000 (Wed, 07 Nov 2012) $
*  Last modified by : $Author: daviess $
*  $Id: stdp.h 2046 2012-11-07 16:18:47Z daviess $
*  $HeadURL: https://solem.cs.man.ac.uk/svn/app_frame/testing/inc/stdp.h $
*
* COPYRIGHT
*  Copyright (c) The University of Manchester, 2011. All rights reserved.
*  SpiNNaker Project
*  Advanced Processor Technologies Group
*  School of Computer Science
*
*******/
#ifdef STDP

#ifndef __STDP_H__
#define __STDP_H__

#include "stdp_generic.h"
#include "dma.h"

//#define DEBUG_STDP_ALLOC
//#define DEBUG_PRINT_STDP_POST_UPDATE
//#define DEBUG_PRINT_STDP
//#define DEBUG_PRINT_STDP_UPDATE

extern int *PostTimeStamp;
extern stdp_table_t *stdp_table;

void load_stdp_config_data (void);
int alloc_stdp_post_TS_mem (void);
void StdpPostUpdate (unsigned int neuronID, unsigned int currentSimTime);
void Stdp (synaptic_row_t *start, unsigned short int row_size, unsigned int currentSimTime);

#endif //#ifndef __STDP_H__

#endif //#ifdef STDP
