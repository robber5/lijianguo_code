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
#ifndef MP4_ENCODER_H /* MP4_ENCODER_H */
#define MP4_ENCODER_H /* MP4_ENCODER_H */

/*****************************************
Function:		mp4_write_open
Description:	write mp4 header
Input:
				mp4FilePath:	output mp4 file
				tmpMp2Path:		temp mp2 file path
				width:	frame width
				height: frame height
				videoCodecSelect: 1 for mjpeg,2 for h264
				audioCodecSelect: 1 for 8bit,8k pcm, 2 for 16bit,16k pcm, 3 for aac
Output:			none
Return:			NULL when error happens,otherwise handler of mp4 encoder
Others:			none
*****************************************/
void *mp4_write_open(const wchar_t *mp4FilePath, const wchar_t *tmpMp2Path, int width, int height, int videoCodecSelect, int audioCodecSelect);

/*****************************************
Function:		mp4_write_video
Description:	write one frame(one jpeg or one nalu(including I frame, p frame, b frame, sps, pps)) to mp4
Input:			
				videoBuf:	buffer of frame
				bufSize:	size of frame
				timeStamp:	time stamp of this frame(unit: ms)(µÝÔö)
				handler:  handler of mp4 encoder
Output:			none
Return:			-1 when error happens,otherwise 0
Others:			none
*****************************************/
int mp4_seach_video(unsigned char *videoBuf, int bufSize, int timeStamp, void *handler);

int mp4_write_video(unsigned char *videoBuf, int bufSize, int timeStamp, void *handler);

int mp4_write_splite_sps_pps_i(unsigned char *videoBuf, int bufSize, int timeStamp, void *handler);

/*****************************************
Function:		mp4_write_audio
Description:	encode pcm to mp2, or handle aac frame
Input:			
				pcmBuf:		buffer of pcm or aac
				bufSize:	size of pcmBuf, bufSize % 576 == 0 for 8bit,8khz pcm
                                  bufSize % 2304 == 0  for 16bit,16khz pcm
        		handler:  handler of mp4 encoder
Output:			none
Return:			-1 when error happens,otherwise 0
Others:			none
*****************************************/
int mp4_write_audio(unsigned char *pcmBuf, int bufSize, int audioTimeStamp, void *handler);

/*****************************************
Function:		mp4_write_close
Description:	write mp4 file end
Input:	    	handler:  handler of mp4 encoder
Output:			none
Return:			-1 when error happens,otherwise 0
Others:			none
*****************************************/
int mp4_write_close(void *handler);

/*****************************************
Function:		mp4_check_data
Description:	mp4 file check data
Input:			none
Output:			none
Return:			1 when have data,otherwise 0
Others:			none
*****************************************/
int mp4_check_data(void *handler);

#endif /* MP4_ENCODER_H */
