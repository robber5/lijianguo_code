/**********************************
File name:		mp4_pcm_to_mp2.h
Version:		1.0
Description:	encode pcm into mp2 and write to file
Author:			Shen Jiabin
Create Date:	2014-02-12

History:
-----------------------------------
01,12Feb14,Shen Jiabin create file.
-----------------------------------
**********************************/
#ifndef MP4_PCM_TO_MP2_H
#define MP4_PCM_TO_MP2_H

/*****************************************
Function:		mp4_pcm_to_mp2
Description:	encode pcm into mp2 and write to file
Input:			fpMp2:mp2 file pointer; pcmBuf:pcm data buffer;size of pcmBuf; hEnc:handle of mp2 encoder
Output:			none
Return:			-1 when error happens,otherwise 0
Others:			none
*****************************************/
int mp4_pcm_to_mp2(FILE *fpMp2, unsigned char *pcmBuf, int bufSize, void* hEnc);

/*****************************************
Function:		mp4_pcm_to_mp2_2
Description:	encode pcm into mp2 and write to file
Input:			fpMp2:mp2 file pointer; pcmBuf:pcm data buffer;size of pcmBuf; hEnc:handle of mp2 encoder
Output:			none
Return:			-1 when error happens,otherwise 0
Others:			none
*****************************************/
int mp4_pcm_to_mp2_2(FILE *fpMp2, unsigned char *pcmBuf, int bufSize, void* hEnc);

#endif	/* end of MP4_PCM_TO_MP2_H */
