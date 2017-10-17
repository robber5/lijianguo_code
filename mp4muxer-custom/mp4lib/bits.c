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
* File Name: bits.c						
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

#include "bits.h"

void init_put_bits(PutBitContext *s, UINT8 *buffer, UINT32 buffer_size)
{
    s->buf = buffer;
    s->buf_end = s->buf + buffer_size;
    s->buf_ptr = s->buf;
    s->bit_left=32;
    s->bit_buf=0;
}

/* pad the end of the output stream with zeros */
void flush_put_bits(PutBitContext *s)
{
    s->bit_buf<<= s->bit_left;
    while (s->bit_left < 32) 
	{
        *s->buf_ptr++=s->bit_buf >> 24;
        s->bit_buf<<=8;
        s->bit_left+=8;
    }
    s->bit_left=32;
    s->bit_buf=0;
}

void put_bits(PutBitContext *s, int n, UINT32 value)
{
    UINT32 bit_buf;
    int bit_left;
	
    //assert(n == 32 || value < (1U << n));
    
    bit_buf = s->bit_buf;
    bit_left = s->bit_left;
	
    if (n < bit_left)
	{
        bit_buf = (bit_buf<<n) | value;
        bit_left-=n;
    } 
	else 
	{
		bit_buf<<=bit_left;
        bit_buf |= value >> (n - bit_left);
			*(UINT32 *)s->buf_ptr = SWAP32(bit_buf);
        s->buf_ptr+=4;
		bit_left+=32 - n;
        bit_buf = value;
    }
	
    s->bit_buf = bit_buf;
    s->bit_left = bit_left;
}
