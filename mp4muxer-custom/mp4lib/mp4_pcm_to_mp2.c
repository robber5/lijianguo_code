/**********************************
File name:		mp4_pcm_to_mp2.c
Version:		1.0
Description:	encode pcm into mp2 and write to file
Author:			Shen Jiabin
Create Date:	2014-02-12

History:
-----------------------------------
01,12Feb14,Shen Jiabin create file.
-----------------------------------
**********************************/
#ifdef __cplusplus
extern "C"{
#endif
#include <stdio.h>
#include <time.h>
//#include <sys/time.h>
//#include <unistd.h>
#include "libmp2enc.h"
#include "mp4_pcm_to_mp2.h"

#ifdef __cplusplus
}
#endif


#define MP4_PCM_BUFFER_SIZE		(2304)
#define MP4_SAMPLES_PER_FRAME	(1152)
#define MP4_SLEEP_TIME			(20000)

#ifdef DEBUG /* below is used for debugging */
static void time_begin(struct timeval *begin)
{
	gettimeofday(begin, NULL);
}

static void time_end(struct timeval *begin)
{
	struct timeval end;
	int t = 0;
	
	gettimeofday(&end, NULL);
	t = end.tv_usec - begin->tv_usec;
	if (end.tv_sec > begin->tv_sec)
	{
		t += 1000000 * (end.tv_sec - begin->tv_sec);
	}
	printf("mp4: time cost for mp2 encoding: %dus\n", t);
}
#endif /* end of debug code */


/*****************************************
Function:		mp4_pcm_to_mp2
Description:	encode pcm into mp2 and write to file
Input:			fpMp2:mp2 file pointer; pcmBuf:pcm data buffer;size of pcmBuf; hEnc:handle of mp2 encoder
Output:			none
Return:			-1 when error happens,otherwise 0
Others:			none
*****************************************/
int mp4_pcm_to_mp2(FILE *fpMp2, unsigned char *pcmBuf, int bufSize, void* hEnc)
{
	SINT16 pcmData[MP4_PCM_BUFFER_SIZE];
	UINT8 mpaData[MPA_MAX_CODED_FRAME_SIZE];
	unsigned int mpaDataSize = 0;
	int i = 0;
	int j = 0;
	signed short sample = 0;
	unsigned char char1 = '\0';
	int counter = 0;
#ifdef DEBUG /* below is used for debugging */
	struct timeval time1; 
#endif /* end of debug code */

	if(NULL == fpMp2 || NULL == pcmBuf || bufSize <= 0 || bufSize % 576 != 0)
	{
		printf("mp4:mp2 eocoder:input param error!\n");
		return -1;
	}
	else
	{
#ifdef DEBUG /* below is used for debugging */
		time_begin(&time1);
#endif /* end of debug code */

		

		while(counter < bufSize)
		{
			j = 0;
			for(i = 0; i < MP4_SAMPLES_PER_FRAME; i += 2)
			{
				char1 = pcmBuf[counter + j];
				sample = ((signed short)(char1+ 0x80)) << 8;
				pcmData[i] = sample;
				pcmData[i + 1] = sample;
				j++;
			}
			
			MP2_encode_frame(hEnc, &mpaDataSize, mpaData, MP4_PCM_BUFFER_SIZE, pcmData);
			if(mpaDataSize <= 0)
			{
				printf("mp4:mp2 encode error!\n");
				return -1;
			}
			if(fwrite(mpaData, 1, mpaDataSize, fpMp2) != mpaDataSize)
			{
				printf("mp4:mp2 encode:write file error!\n");
				return -1;
			}
			//printf("encode one frame:%d\n", mpa_data_size);

			counter += 576;
			//usleep(MP4_SLEEP_TIME);
		}

		
#ifdef DEBUG /* below is used for debugging */
		time_end(&time1);
#endif /* end of debug code */
	}
	
	return 0;
}


/*****************************************
Function:		mp4_pcm_to_mp2_2
Description:	encode pcm into mp2 and write to file
Input:			fpMp2:mp2 file pointer; pcmBuf:pcm data buffer;size of pcmBuf; hEnc:handle of mp2 encoder
Output:			none
Return:			-1 when error happens,otherwise 0
Others:			none
*****************************************/
int mp4_pcm_to_mp2_2(FILE *fpMp2, unsigned char *pcmBuf, int bufSize, void* hEnc)
{
	SINT16 pcmData[MP4_PCM_BUFFER_SIZE];
	UINT8 mpaData[MPA_MAX_CODED_FRAME_SIZE];
	unsigned int mpaDataSize = 0;
	int i = 0;
	signed short *pSample = NULL;
	int counter = 0;
#ifdef DEBUG /* below is used for debugging */
	struct timeval time1; 
#endif /* end of debug code */

	if(NULL == fpMp2 || NULL == pcmBuf || bufSize <= 0 || bufSize % 2304 != 0 || NULL == hEnc)
	{
		printf("mp4:mp2 eocoder:input param error!\n");
		return -1;
	}
	else
	{
#ifdef DEBUG /* below is used for debugging */
		time_begin(&time1);
#endif /* end of debug code */

		
    pSample = (signed short *)pcmBuf;
		while(counter < bufSize)
		{
			for(i = 0; i < MP4_SAMPLES_PER_FRAME; i ++)
			{
				pcmData[i] = *pSample;
				pSample++;
			}
			
			MP2_encode_frame(hEnc, &mpaDataSize, mpaData, MP4_PCM_BUFFER_SIZE, pcmData);
			if(mpaDataSize <= 0)
			{
				printf("mp4:mp2 encode error!\n");
				return -1;
			}
			if(fwrite(mpaData, 1, mpaDataSize, fpMp2) != mpaDataSize)
			{
				printf("mp4:mp2 encode:write file error!\n");
				return -1;
			}
			//printf("encode one frame:%d\n", mpa_data_size);

			counter += 2304;
			//usleep(MP4_SLEEP_TIME);
		}

		
#ifdef DEBUG /* below is used for debugging */
		time_end(&time1);
#endif /* end of debug code */
	}
	
	return 0;
}

