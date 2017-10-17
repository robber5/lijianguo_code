/*
* This source code is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2 of the License, or (at your option) any later version.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*       
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*
* File Name: mp2.h							
*
* Reference:
*
* Author: Li Feng (fli_linux@yahoo.com.cn)                                                
*
* Description:
*
* 	
* 
* History:
* 11/16/2005  
*  
*
*CodeReview Log:
* 
*/
#ifndef __MP2_H__
#define __MP2_H__

#ifdef __cplusplus
extern "C" {
#endif
#include "bits.h"
#include "libmp2enc.h"

#define SBLIMIT 32 /* number of subbands */

#define MPA_STEREO  0
#define MPA_JSTEREO 1
#define MPA_DUAL    2
#define MPA_MONO    3

#define FRAC_BITS 15
#define WFRAC_BITS  14
#define MUL(a,b) (((SINT64)(a) * (SINT64)(b)) >> FRAC_BITS)
#define FIX(a)   ((int)((a) * (1 << FRAC_BITS)))

#define SAMPLES_BUF_SIZE 4096

typedef struct tagMpegAudioContext 
{
    PutBitContext pb;
    int nb_channels;
    int freq, bit_rate;
    int lsf;			/* 1 if mpeg2 low bitrate selected */
    int bitrate_index;	/* bit rate */
    int freq_index;
    int frame_size;		/* frame size, in bits, without padding */
    SINT64 nb_samples; /* total number of samples encoded */
    int frame_frac;
	int frame_frac_incr;
	int do_padding;
    short samples_buf[MPA_MAX_CHANNELS][SAMPLES_BUF_SIZE]; /* buffer for filter */
    int samples_offset[MPA_MAX_CHANNELS];       /* offset in samples_buf */
    int sb_samples[MPA_MAX_CHANNELS][3][12][SBLIMIT];
    unsigned char scale_factors[MPA_MAX_CHANNELS][SBLIMIT][3]; /* scale factors */
    /* code to group 3 scale factors */
    unsigned char scale_code[MPA_MAX_CHANNELS][SBLIMIT];       
    int sblimit; /* number of used subbands */
    const unsigned char *alloc_table;
} MpegAudioContext;

#ifdef __cpluscplus
}
#endif

#endif

