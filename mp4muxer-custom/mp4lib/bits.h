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
* File Name: bits.h							
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
#ifndef _BITS_H_
#define _BITS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "typedef.h"

typedef struct tagPutBitContext 
{
    UINT32 bit_buf;
    int bit_left;
    UINT8 *buf;
	UINT8 *buf_ptr;
	UINT8 *buf_end;
} PutBitContext;

void init_put_bits(PutBitContext *s, UINT8 *buffer, UINT32 buffer_size);
void flush_put_bits(PutBitContext *s);
void put_bits(PutBitContext *s, int n, UINT32 value);

#ifdef __cplusplus
}
#endif

#endif //_BITS_H_

