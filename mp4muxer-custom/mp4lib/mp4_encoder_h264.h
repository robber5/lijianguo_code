/**********************************
File name:		mp4_encoder.h
Version:		1.0
Description:	mp4 encoder
Author:			Shen Jiabin
Create Date:	2014-02-12

History:
-----------------------------------
01,12Feb14,Shen Jiabin create file.
-----------------------------------
**********************************/
#ifndef MP4_ENCODER_H264_H /* MP4_ENCODER_H264_H */
#define MP4_ENCODER_H264_H /* MP4_ENCODER_H264_H */

/*****************************************
Function:		mp4_write_head
Description:	write mp4 header
Input:
mp4FilePath:	output mp4 file
tmpMp2Path:		temp mp2 file path
frameSizeID:	1:640*480, 2:320*240
Output:			none
Return:			-1 when error happens,otherwise 0
Others:			none
*****************************************/
void *mp4_write_head_h264(const wchar_t *mp4FilePath, const wchar_t *tmpMp2Path, int width, int height, int audioCodec);

void *mp4_write_head_h265(const wchar_t *mp4FilePath, const wchar_t *tmpMp2Path, int width, int height, int audioCodec);

/*****************************************
Function:		mp4_write_one_jpeg
Description:	write one jpeg to mp4
Input:			
jpegBuf:	buffer of jpeg
bufSize:	size of jpegBuf
timeStamp:	time stamp of this jpeg(unit: ms)
Output:			none
Return:			-1 when error happens,otherwise 0
Others:			none
*****************************************/
int mp4_write_one_h264(unsigned char *h264Buf, int bufSize, int timeStamp, void *handler);

int mp4_write_one_h265(unsigned char *h265Buf, int bufSize, int timeStamp, void *handler);

int mp4_write_seach_sps_pps(unsigned char *h264Buf, int bufSize, int timeStamp, void *handler);
/*****************************************
Function:		mp4_write_pcm
Description:	encode pcm to mp2
Input:			
pcmBuf:		buffer of pcm
bufSize:	size of pcmBuf, bufSize % 2304 == 0
Output:			none
Return:			-1 when error happens,otherwise 0
Others:			none
*****************************************/
int mp4_write_pcm_h264(unsigned char *pcmBuf, int bufSize, int audioTimeStamp, void *handler);

/*****************************************
Function:		mp4_write_end
Description:	write mp4 file end
Input:			none
Output:			none
Return:			-1 when error happens,otherwise 0
Others:			none
*****************************************/
int mp4_write_end_h264(void *handler);

/*****************************************
Function:		mp4_check_h264_data
Description:	check the mp4 file if have the h264 data
Input:			none
Output:			none
Return:			1 when no data,otherwise 0
Others:			none
*****************************************/
int mp4_check_h264_data(void *handler);

#endif /* MP4_ENCODER_H264_H */
