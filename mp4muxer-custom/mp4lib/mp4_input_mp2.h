/**********************************
File name:		mp4_input_mp2.h
Version:		1.0
Description:	handle mp2 data
Author:			Shen Jiabin
Create Date:	2014-02-12

History:
-----------------------------------
01,12Feb14,Shen Jiabin create file.
-----------------------------------
**********************************/
#ifndef MP4_INPUT_MP2_H /* MP4_INPUT_MP2_H */
#define MP4_INPUT_MP2_H /* MP4_INPUT_MP2_H */

/*****************************************
Function:		mp4_handle_mp2_head
Description:	handle mp2 head data
Input:			fpMp2:mp2 file pointer
Output:			none
Return:			-1 when error happens,otherwise 0
Others:			none
*****************************************/
int mp4_handle_mp2_head(FILE *fpMp2);

/*****************************************
Function:		mp4_read_mp2_frame
Description:	read mp2 frame
Input:			fpMp2:mp2 file pointer;buf:buffer;bufSize:size of buffer;dataSize:actual data size
Output:			none
Return:			-1 when error happens,otherwise 0
Others:			none
*****************************************/
int mp4_read_mp2_frame(FILE *fpMp2, unsigned char *buf, unsigned int bufSize, unsigned int *dataSize);
	
#endif /* MP4_INPUT_MP2_H */
