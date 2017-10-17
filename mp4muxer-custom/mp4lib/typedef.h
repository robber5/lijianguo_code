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
* File Name: typedef.h							
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
#ifndef _TYPEDEF_H_
#define _TYPEDEF_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef signed char		SINT8;
typedef signed short	SINT16;
typedef signed int		SINT32;
typedef unsigned char	UINT8;
typedef unsigned short	UINT16;
typedef unsigned int	UINT32;

typedef signed long long   SINT64;
typedef unsigned long long UINT64;

#define BOOL int
#define TRUE 1
#define FALSE 0

#define SWAP32(val) (UINT32)((((UINT32)(val)) & 0x000000FF)<<24|	\
					(((UINT32)(val)) & 0x0000FF00)<<8 |	\
					(((UINT32)(val)) & 0x00FF0000)>>8 |	\
					(((UINT32)(val)) & 0xFF000000)>>24)	

#ifdef __cplusplus
}
#endif

#endif

