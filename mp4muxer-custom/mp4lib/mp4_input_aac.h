/**********************************
File name:		mp4_input_aac.h
Version:		1.0
Description:	handle aac data
Author:			Shen Jiabin
Create Date:	2014-02-12

History:
-----------------------------------
01,12Feb14,Shen Jiabin create file.
-----------------------------------
**********************************/
#ifndef MP4_INPUT_AAC_H /* MP4_INPUT_AAC_H */
#define MP4_INPUT_AAC_H /* MP4_INPUT_AAC_H */

/*****************************************
Function:		mp4_handle_aac_head
Description:	handle aac head data
Input:			fpAac:aac file pointer
Output:			none
Return:			-1 when error happens,otherwise 0
Others:			none
*****************************************/
int mp4_handle_aac_head(FILE *fpAac);

/*****************************************
Function:		mp4_read_aac_frame
Description:	read aac frame
Input:			fpAac:aac file pointer;buf:buffer;bufSize:size of buffer;dataSize:actual data size
Output:			none
Return:			-1 when error happens,otherwise 0
Others:			none
*****************************************/
int mp4_read_aac_frame(FILE *fpAac, unsigned char *buf, unsigned int bufSize, unsigned int *dataSize);
	
#endif /* MP4_INPUT_AAC_H */
