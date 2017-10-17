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
* File Name: libmp2enc.h							
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

#ifndef __MP2_ENC_H__
#define __MP2_ENC_H__

#ifdef __cplusplus
extern "C" {
#endif
#include "typedef.h"

#define MPA_FRAME_SIZE 1152 
#define MPA_MAX_CODED_FRAME_SIZE 1792
#define MPA_MAX_CHANNELS 2

#define HMP2ENC void*

HMP2ENC MP2_encode_init(UINT32 nSampleRate, UINT32 nBitRate, UINT32 nChannels);
BOOL	MP2_encode_frame(HMP2ENC hEnc, UINT32* pnFrameLen, UINT8 *pMp2Frame, UINT32 nRawFrameLen, SINT16 *pRawDate);
void	MP2_encode_close(HMP2ENC hEnc);

#ifdef __cplusplus
}
#endif

#endif

