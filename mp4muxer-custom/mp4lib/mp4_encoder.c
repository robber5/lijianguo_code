#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mp4_encoder_h264.h"
#include "mp4_encoder_jpeg.h"

typedef struct MP4_ENC_HANDLER
{
	int l_mp4CodecSelect;
	void *handler;
} MP4_ENC_HANDLER;

/*****************************************
Function:		mp4_write_open
Description:	write mp4 header
Input:
mp4FilePath:	output mp4 file
tmpMp2Path:		temp mp2 file path
width:	frame width
height: frame height
videoCodecSelect: 1 for mjpeg,2 for h264
Output:			none
Return:			-1 when error happens,otherwise 0
Others:			none
*****************************************/
void *mp4_write_open(const wchar_t *mp4FilePath, const wchar_t *tmpMp2Path, int width, int height, int videoCodecSelect, int audioCodecSelect)
{
	MP4_ENC_HANDLER *h = NULL;

	if(videoCodecSelect != 1 && videoCodecSelect != 2 && videoCodecSelect != 4)/*??H264 / H265 ??DD?D??*/
	{
		return NULL;
	}

	h = (MP4_ENC_HANDLER *)malloc(sizeof(struct MP4_ENC_HANDLER));
	if(NULL == h)
	{
		return NULL;
	}

	if(1 == videoCodecSelect)
	{
		h->l_mp4CodecSelect = 1;
		h->handler = mp4_write_head_jpeg(mp4FilePath, tmpMp2Path, width, height, audioCodecSelect);
		if(NULL == h->handler)
		{
			free(h);
			return NULL;
		}
	}
	else if (2 == videoCodecSelect )
	{
		h->l_mp4CodecSelect = 2;
		h->handler = mp4_write_head_h264(mp4FilePath, tmpMp2Path, width, height, audioCodecSelect);
		if(NULL == h->handler)
		{
			free(h);
			return NULL;
		}
	}
	else if (4 == videoCodecSelect )
	{
		h->l_mp4CodecSelect = 4;/*H265 ?3?¦Ì3¨¦H265???¨´?¦Ì*/
		h->handler = mp4_write_head_h265(mp4FilePath, tmpMp2Path, width, height, audioCodecSelect);
		if(NULL == h->handler)
		{
			free(h);
			return NULL;
		}
	}

	return (void *)h;
}

/*****************************************
Function:		mp4_write_video
Description:	write one frame(one jpeg or one nalu(including sps,pps)) to mp4
Input:			
videoBuf:	buffer of frame
bufSize:	size of frame
timeStamp:	time stamp of this frame(unit: ms)(ignored for sps and pps)
Output:			none
Return:			-1 when error happens,otherwise 0
Others:			none
*****************************************/
int mp4_write_video(unsigned char *videoBuf, int bufSize, int timeStamp, void *handler)
{
	MP4_ENC_HANDLER *h = NULL;
	h = (MP4_ENC_HANDLER *)handler;
	if(NULL == handler)
	{
		return -1;
	}

	if(1 == h->l_mp4CodecSelect)
	{
		return mp4_write_one_jpeg(videoBuf, bufSize, timeStamp, h->handler);
	}
	else if (2 == h->l_mp4CodecSelect)
	{
		return mp4_write_one_h264(videoBuf, bufSize, timeStamp, h->handler);
	}
	else if (4 == h->l_mp4CodecSelect)
	{
		return mp4_write_one_h265(videoBuf, bufSize, timeStamp, h->handler);
	}
	
}

/*****************************************
Function:		mp4_seach_video
Description:	find one frame(one jpeg or one nalu(including sps,pps)) to mp4
Input:
videoBuf:	buffer of frame
bufSize:	size of frame
timeStamp:	time stamp of this frame(unit: ms)(ignored for sps and pps)
Output:			none
Return:			-1 when error happens,otherwise 0
Others:			none
*****************************************/
int mp4_seach_video(unsigned char *videoBuf, int bufSize, int timeStamp, void *handler)
{
	MP4_ENC_HANDLER *h = NULL;
	h = (MP4_ENC_HANDLER *)handler;
	if (NULL == handler)
	{
		return -1;
	}

	if (1 == h->l_mp4CodecSelect)
	{
		return mp4_write_one_jpeg(videoBuf, bufSize, timeStamp, h->handler);
	}
	else
	{
		return mp4_write_seach_sps_pps(videoBuf, bufSize, timeStamp, h->handler);
	}
}

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
int mp4_write_audio(unsigned char *pcmBuf, int bufSize, int audioTimeStamp, void *handler)
{
	MP4_ENC_HANDLER *h = NULL;

	h = (MP4_ENC_HANDLER *)handler;
	if(NULL == handler)
	{
		return -1;
	}

	if(1 == h->l_mp4CodecSelect)
	{
		return mp4_write_pcm_jpeg(pcmBuf, bufSize, h->handler);
	}
	else
	{
		return mp4_write_pcm_h264(pcmBuf, bufSize, audioTimeStamp, h->handler);
	}
}

/*****************************************
Function:		mp4_write_end
Description:	write mp4 file end
Input:			none
Output:			none
Return:			-1 when error happens,otherwise 0
Others:			none
*****************************************/
int mp4_write_close(void *handler)
{
	MP4_ENC_HANDLER *h = NULL;
	int res = 0;

	h = (MP4_ENC_HANDLER *)handler;
	if(NULL == handler)
	{
		return -1;
	}

	if(1 == h->l_mp4CodecSelect)
	{
		res = mp4_write_end_jpeg(h->handler);
		free(h);
		return res;
	}
	else
	{
		res = mp4_write_end_h264(h->handler);
		free(h);
		return res;
	}
}

/*****************************************
Function:		mp4_check_data
Description:	mp4 file check data
Input:			none
Output:			none
Return:			1 when have data,otherwise 0
Others:			none
*****************************************/
int mp4_check_data(void *handler)
{
	MP4_ENC_HANDLER *h = NULL;
	int res = 0;

	h = (MP4_ENC_HANDLER *)handler;
	if (NULL == handler)
	{
		return -1;
	}

	res = mp4_check_h264_data(h->handler);
//	free(h);

	return res; 
}

int mp4_write_splite_sps_pps_i(unsigned char *videoBuf, int bufSize, int timeStamp, void *handler)
{
	int current_pos;
	int pre_pos;
	int ret;
	int time;

	if (NULL == videoBuf || bufSize <= 0 || timeStamp < 0 || NULL == handler)
	{
		return -1;
	}

	current_pos = 3;
	pre_pos = 0;
	ret = 0;
	time = timeStamp;

	while (current_pos < (bufSize - 3))
	{
		if ((videoBuf[current_pos] == 0x00 && videoBuf[current_pos+1] == 0x00 && videoBuf[current_pos+2] == 0x01)
			||(videoBuf[current_pos] == 0x00 && videoBuf[current_pos+1] == 0x00 && videoBuf[current_pos+2] == 0x00 && videoBuf[current_pos+3] == 0x01))
		{
//			printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!in the if\n");
			ret = mp4_write_video((videoBuf + pre_pos), (current_pos - pre_pos), time, handler);

			if (ret != 0)
			{
				return -1;
			}
			pre_pos = current_pos;
			current_pos+=3;
		}
		current_pos++;
	} 

	/*write other frame*/
//	printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!in the pf %d %d\n", bufSize, pre_pos);
//	pre_pos = 0;
	ret = mp4_write_video((videoBuf + pre_pos), (bufSize - pre_pos), time++, handler);

	return ret;

}