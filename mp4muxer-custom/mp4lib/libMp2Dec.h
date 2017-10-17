/**********************************
File name:		libmp2dec.h
Version:		1.0
Description:	mp2 decoder
Author:			Shen Jiabin
Create Date:	2014-10-10

History:
-----------------------------------
01,10Oct14,Shen Jiabin create file.
-----------------------------------
**********************************/
#ifndef __MP2_DEC_H__
#define __MP2_DEC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "mp2DecTypedef.h"

	
#define  HMP2DEC void*

/*****************************************
Function:		MP2_decode_init
Description:	init mp2 decoder
Input:			none
Output:			none
Return:			NULL when error happens,otherwise handler of decoder
Others:			none
*****************************************/
HMP2DEC MP2_decode_init();

/*****************************************
Function:		MP2_decode_frame
Description:	decode one mpeg audio layer II(mp2) frame into lpcm data
Input:			hDec: handler of mp2 decoder
            pPcmData: lpcm data buf
            pnPcmDatalen: lpcm data buf size
            pMp2Data: mp2 frame data to decode
            nMp2DataLen: mp2 frame data size(<= 1792 bytes)
Output:			none
Return:			false when error happens, otherwise true
Others:			none
*****************************************/
BOOL	MP2_decode_frame(HMP2DEC hDec, void *pPcmData, unsigned int *pnPcmDatalen, unsigned char *pMp2Data, unsigned int nMp2DataLen);

/*****************************************
Function:		MP2_decode_close
Description:	close mp2 decoder
Input:			hDec: handler of mp2 decoder
Output:			none
Return:			none
Others:			none
*****************************************/
void	MP2_decode_close(HMP2DEC hDec);
				 
#ifdef __cplusplus
}
#endif
	
#endif

