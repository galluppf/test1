/****a* spinn_sdp.h/spinn_sdp_header
*
* SUMMARY
*  SpiNNaker Datagram Protocol (SDP) main header file
*
* AUTHOR
*  Steve Temple - temples@cs.man.ac.uk
*
* DETAILS
*  Created on       : 03 May 2011
*  Version          : $Revision: 1645 $
*  Last modified on : $Date: 2012-01-13 15:09:32 +0000 (Fri, 13 Jan 2012) $
*  Last modified by : $Author: plana $
*  $Id: spinn_sdp.h 1645 2012-01-13 15:09:32Z plana $
*  $HeadURL: https://solem.cs.man.ac.uk/svn/spin1_api/testing/src/spinn_sdp.h $
*
* COPYRIGHT
*  Copyright (c) The University of Manchester, 2011. All rights reserved.
*  SpiNNaker Project
*  Advanced Processor Technologies Group
*  School of Computer Science
*
*******/

#ifndef __SPINN_SDP_H__
#define __SPINN_SDP_H__


#define PORT_ETH 		255
#define SDP_BUF_SIZE 		(BUF_SIZE)


// ------------------------------------------------------------------------
// SDP type definitions
// ------------------------------------------------------------------------
// Note that the length field is the number of bytes following
// the checksum. It will be a minimum of 8 as the SDP header
// should always be present.

typedef struct sdp_msg		// SDP message (=292 bytes)
{
  struct sdp_msg *next;		// Next in free list
  ushort length;		// length
  ushort checksum;		// checksum (if used)

  // sdp_hdr_t

  uchar flags;			// SDP flag byte
  uchar tag;			// SDP IPtag
  uchar dest_port;		// SDP destination port
  uchar srce_port;		// SDP source port
  ushort dest_addr;		// SDP destination address
  ushort srce_addr;		// SDP source address

  // cmd_hdr_t (optional)

  ushort cmd_rc;		// Command/Return Code
  ushort seq;			// Sequence number
  uint arg1;			// Arg 1
  uint arg2;			// Arg 2
  uint arg3;			// Arg 3

  // user data (optional)

  uchar data[SDP_BUF_SIZE];	// User data (256 bytes)

  uint _PAD;			// Private padding
} sdp_msg_t;


#endif /* __SPINN_SDP_H__ */
