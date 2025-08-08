/***************************************************************************/
/* Copyright (C) 2006 Electronic Theatre Controls, Inc.                    */
/* All rights reserved.                                                    */
/*                                                                         */
/* This software is provided by Electronic Theatre Controls, Inc. ("ETC")  */
/* at no charge to the user to facilitate the use and implementation of    */
/* ANSI BSR E1.31-2006 BSR E1.31, Entertainment Technology - DMX512-A      */
/* Streaming Protocol (DSP).                                               */
/*                                                                         */
/* Redistribution and use in source and binary forms, with or without      */
/* modification, are permitted provided that the following conditions are  */
/* met:                                                                    */
/*                                                                         */
/*      Redistributions of source code, whether in binary form or          */
/*      otherwise must retain the above copyright notice, a list of        */
/*      modifications made, if any, a description of modifications made,   */
/*      this list of conditions and the following disclaimer.              */
/*                                                                         */
/* THIS SOFTWARE IS PROVIDED "AS IS" BY THE COPYRIGHT HOLDER AND WITHOUT   */
/* WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING, WITHOUT     */
/* LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,            */
/* MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.  THE     */
/* ENTIRE RISK AS TO THE QUALITY, OPERATION, AND PERFORMANCE OF THE        */
/* SOFTWARE IS WITH THE USER.                                              */
/*                                                                         */
/* UNDER NO CIRCUMSTANCES AND UNDER NO LEGAL THEORY, WHETHER TORT          */
/* (INCLUDING NEGLIGENCE), CONTRACT OR OTHERWISE, SHALL ETC BE LIABLE TO   */
/* ANY PERSON OR ENTITY FOR ANY INDIRECT, SPECIAL, INCIDENTAL, OR          */
/* CONSEQUENTIAL DAMAGES OF ANY KIND INCLUDING WITHOUT LIMITATION, DAMAGES */
/* FOR LOSS OF GOODWILL, WORK STOPPAGE, COMPUTER FAILURE OR MALFUNCTION,   */
/* OR ANY AND ALL OTHER COMMERCIAL DAMAGES OR LOSSES, EVEN IF SUCH         */
/* PARTY SHALL HAVE BEEN INFORMED OF THE POSSIBILITY OF SUCH DAMAGES.      */
/* THIS LIMITATION OF LIABILITY SHALL NOT APPLY (I) TO DEATH OR PERSONAL   */
/* INJURY RESULTING FROM SUCH PARTY'S NEGLIGENCE TO THE EXTENT APPLICABLE  */
/* LAW PROHIBITS SUCH LIMITATION, (II) FOR CONSUMERS WITH RESIDENCE IN A   */
/* MEMBER COUNTRY OF THE EUROPEAN UNION (A) TO DEATH OR PERSONAL INJURY    */
/* RESULTING FROM ETC'S NEGLIGENCE, (B) TO THE VIOLATION OF ESSENTIAL      */
/* CONTRACTUAL DUTIES BY ETC, AND (C) TO DAMAGES RESULTING FROM            */
/* ETC'S GROSS NEGLIGENT OR WILLFUL BEHAVIOR, SOME JURISDICTIONS DO NOT    */
/* ALLOW THE EXCLUSION OR LIMITATION OF INCIDENTAL OR CONSEQUENTIAL        */
/* DAMAGES, SO THIS EXCLUSION AND LIMITATION MAY NOT APPLY TO YOU.         */
/*                                                                         */
/* Neither the name of ETC, its affiliates, nor the names of its           */
/* contributors may be used to endorse or promote products derived from    */
/* this software without express prior written permission of the President */
/* of ETC.                                                                 */
/*                                                                         */
/* The user of this software agrees to indemnify and hold ETC, its         */
/* affiliates, and contributors harmless from and against any and all      */
/* claims, damages, and suits that are in any way connected to or arising  */ 
/* out of, either in whole or in part, this software or the user's use of  */
/* this software.                                                          */
/*                                                                         */
/* Except for the limited license granted herein, all right, title and     */
/* interest to and in the Software remains forever with ETC.               */
/***************************************************************************/

/*streamcommon.h  The common functions both a client or server may need,
 * mostly dealing with packing or parsing the streaming ACN header*/

#ifndef _STREAMCOMMON_H_
#define _STREAMCOMMON_H_

#include "deftypes.h"
#include "CID.h"
#include "ipaddr.h"

/* 
 * a description of the address space being used
 */

#define PREAMBLE_SIZE_ADDR 0
#define POSTAMBLE_SIZE_ADDR 2
#define ACN_IDENTIFIER_ADDR 4
#define ROOT_FLAGS_AND_LENGTH_ADDR 16
#define ROOT_VECTOR_ADDR 18
#define CID_ADDR 22
#define FRAMING_FLAGS_AND_LENGTH_ADDR 38
#define FRAMING_VECTOR_ADDR 40
#define SOURCE_NAME_ADDR 44
#define PRIORITY_ADDR 108
#define RESERVED_ADDR 109
#define SEQ_NUM_ADDR 111
#define OPTIONS_ADDR 112
#define UNIVERSE_ADDR 113
#define DMP_FLAGS_AND_LENGTH_ADDR 115
#define DMP_VECTOR_ADDR 117
#define DMP_ADDRESS_AND_DATA_ADDR 118
#define FIRST_PROPERTY_ADDRESS_ADDR 119
#define ADDRESS_INC_ADDR 121
#define PROP_COUNT_ADDR 123
#define START_CODE_ADDR 125
#define PROP_VALUES_ADDR (START_CODE_ADDR + 1)

//for support of the early draft:
#define DRAFT_PRIORITY_ADDR 76
#define DRAFT_SEQ_NUM_ADDR 77
#define DRAFT_UNIVERSE_ADDR 78
#define DRAFT_DMP_FLAGS_AND_LENGTH_ADDR 80
#define DRAFT_DMP_VECTOR_ADDR 82
#define DRAFT_DMP_ADDRESS_AND_DATA_ADDR 83
#define DRAFT_FIRST_PROPERTY_ADDRESS_ADDR 84
#define DRAFT_ADDRESS_INC_ADDR 86
#define DRAFT_PROP_COUNT_ADDR 88
#define DRAFT_PROP_VALUES_ADDR 90

/*
 * common sizes
 */
//You'd think this would be 125, wouldn't you?  
//But it's not.  It's not because the start code is squeaked in
//right before the actual DMX512-A data.
#define STREAM_HEADER_SIZE 126
#define SOURCE_NAME_SIZE 64
#define RLP_PREAMBLE_SIZE 16
#define RLP_POSTAMBLE_SIZE 0
#define ACN_IDENTIFIER_SIZE 12

//for support of the early draft
#define DRAFT_STREAM_HEADER_SIZE 90
#define DRAFT_SOURCE_NAME_SIZE 32

/*
 * data definitions
 */
#define ACN_IDENTIFIER "ASC-E1.17\0\0\0"	
#define ROOT_VECTOR 4
#define FRAMING_VECTOR 2
#define DMP_VECTOR 2
#define ADDRESS_AND_DATA_FORMAT 0xa1
#define ADDRESS_INC 1
#define DMP_FIRST_PROPERTY_ADDRESS_FORCE 0
#define RESERVED_VALUE 0

//for support of the early draft
#define DRAFT_ROOT_VECTOR 3

/***/

//The well-known streaming ACN port (currently the ACN port)
#define STREAM_IP_PORT 5568

/*The start codes we'll commonly use*/
#ifndef STARTCODE_PRIORITY
//The payload is up to 512 1-byte dmx values
#define STARTCODE_DMX 0
///The payload is the per-channel priority (0-200), 
//where 0 means "ignore my values on this channel"
#define STARTCODE_PRIORITY 0xDD     

#endif

/*** Functions ***/

/*
 * Given a buffer, initialize the header, based on the data slot count, 
 * cid, etc. The buffer must be at least STREAM_HEADER_SIZE bytes long
 */
void InitStreamHeader(uint1* pbuf, const CID &source_cid, 
		      const char* source_name, uint1 priority, uint2 reserved,
		      uint1 options, uint1 start_code, uint2 universe, 
		      uint2 slot_count);
/*
 * Given a buffer, initialize the header, based on the data slot count, 
 * cid, etc. The buffer must be at least STREAM_HEADER_SIZE bytes long
 * This function is included to support legacy code from before 
 * ratification of the standard.
 */
void InitStreamHeaderForDraft(uint1* pbuf, const CID &source_cid, 
			      const char* source_name, uint1 priority, 
			      uint1 start_code, uint2 universe, 
			      uint2 slot_count);

/* Given an initialized buffer, change the sequence number to... */
void SetStreamHeaderSequence(uint1* pbuf, uint1 seq);
void SetStreamHeaderSequenceForDraft(uint1* pbuf, uint1 seq);

/*
 * Given a buffer, validate that the stream header is correct.  If this returns
 * true, the header is validated, and the necessary values are filled in.  
 * source_space must be of size SOURCE_NAME_SPACE.
 * pdata is the offset into the buffer where the data is stored
 */
bool ValidateStreamHeader(uint1* pbuf, uint buflen, CID &source_cid, 
			  char* source_space, uint1 &priority, 
			  uint1 &start_code, uint2 &reserved, uint1 &sequence,
			  uint1 &options, uint2 &universe,
			  uint2 &slot_count, uint1* &pdata);

/*
 * helper function that does the actual validation of a header
 * that carries the post-ratification root vector
 */
bool VerifyStreamHeader(uint1* pbuf, uint buflen, CID &source_cid, 
			char* source_space, uint1 &priority, 
			uint1 &start_code, uint2 &reserved, uint1 &sequence, 
			uint1 &options, uint2 &universe,
 			uint2 &slot_count, uint1* &pdata);
/*
 * helper function that does the actual validation of a header
 * that carries the early draft's root vector
 * This function is included to support legacy code from before 
 * ratification of the standard.
 */
bool VerifyStreamHeaderForDraft(uint1* pbuf, uint buflen, CID &source_cid, 
				char* source_space, uint1 &priority, 
				uint1 &start_code, uint1 &sequence, 
				uint2 &universe, uint2 &slot_count, 
				uint1* &pdata);

/* 
 * toggles the preview_data bit of the options field to either 1 or 0
 */
void SetPreviewData(uint1* pbuf, bool preview);

/* 
 * toggles the stream_terminated  bit of the options field to either 1 or 0
 */
void SetStreamTerminated(uint1* pbuf, bool terminated);

/* 
 * returns the stream_terminated  bit of the options field
 */
bool GetStreamTerminated(uint1* pbuf);

/*Fills in the multicast address and port (not netiface) to use for listening to or sending on a universe*/
void GetUniverseAddress(uint2 universe, CIPAddr& addr);


#endif //_STREAMCOMMON_H_

