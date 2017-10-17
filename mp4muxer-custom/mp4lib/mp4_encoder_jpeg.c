/**********************************
File name:		mp4_encoder.c
Version:		1.0
Description:	mp4 encoder
Author:			Shen Jiabin
Create Date:	2014-02-12

History:
-----------------------------------
01,12Feb14,Shen Jiabin create file.
-----------------------------------
**********************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "mp4_encoder_jpeg.h"
#include "mp4_file.h"
#include "mp4_input_mp2.h"
#include "mp4_pcm_to_mp2.h"
#include "libmp2enc.h"

//#include "debug.h"

#define MP4_LANGUAGE	(0x55C4)
#define MP4_BUF_SIZE	(65536)

/* mjpeg list struct node */
typedef struct MJPEG_LIST
{
	int mjpegSize;						/* jpeg size */
	int timeStamp;						/* timeStamp(ms) */
	struct MJPEG_LIST *next;			/* next node */
} MJPEG_LIST;

/* mp2 frame struct node */
typedef struct MP2_FRAME
{
	int frameSize;						/* mp2 frame size */
	struct MP2_FRAME *next;				/* next node */
} MP2_FRAME;

/* mp4 key info struct */
typedef struct MP4_CONTEXT 
{
	FILE *fpMp4;						/* mp4 file pointer */
	FILE *fpMp2;						/* mp2 file pointer */
	unsigned int time;					/* encode time flag */
	long mdatPos;						/* mdat position */
	unsigned int mdatSize;				/* mdat size */

	MJPEG_LIST *videoTrack;				/* mjpeg list head */							
	MJPEG_LIST *videoTail;				/* mjpeg list tail */									
	unsigned int videoTrackSize;		/* jpeg num */
	unsigned int width;					/* video width */
	unsigned int height;				/* video height */

	long audioPos;						/* audio data position */
	MP2_FRAME *audioTrack;				/* mp2 list head */
	MP2_FRAME *audioTail;				/* mp2 list tail */
	unsigned int audioTrackSize;		/* mp2 frame num */

	unsigned int videoSampleDuration;	/* video sample time duration */				
	unsigned int audioSampleDuration;	/* audio sample time duration */
	unsigned int movTimeScale;			/* mov time scale */
	unsigned int movToMediaVideo;		/* mov scale to video scale ratio  */
	unsigned int movToMediaAudio;		/* mov scale to audio scale ratio */
	unsigned int videoTimeScale;		/* video time scale */
	unsigned int audioTimeScale;		/* audio time scale */
	int audioCodec;                 /* audio codec type */

	HMP2ENC hEnc;						/* handle of mp2 encoder */

	int returnError;					/* return error value */
}MP4_CONTEXT;

//static MP4_CONTEXT l_mp4Mov;

static void mjpeg_list_insert_end(MJPEG_LIST *newNode, MP4_CONTEXT *mov)
{
	if(NULL == newNode || NULL == mov)
	{
		return;
	}

	newNode->next = NULL;
	(mov->videoTrackSize)++;
	if(NULL == mov->videoTrack)
	{									
		mov->videoTrack = newNode;
	}
	else
	{
		mov->videoTail->next = newNode;
	}
	mov->videoTail = newNode;

	return;
}

static void mjpeg_list_free(MP4_CONTEXT *mov)
{
	MJPEG_LIST *tmpNode = NULL;

	if(NULL == mov)
	{
		return;
	}
	else
	{
		while(mov->videoTrack != NULL)
		{
			tmpNode = mov->videoTrack;
			mov->videoTrack = mov->videoTrack->next;
			free(tmpNode);
		}
		mov->videoTrack = mov->videoTail = NULL;
		return;
	}
}

static void audio_list_insert_end(MP2_FRAME *newNode, MP4_CONTEXT *mov)
{
	if(NULL == newNode || NULL == mov)
	{
		return;
	}

	newNode->next = NULL;
	(mov->audioTrackSize)++;
	if(NULL == mov->audioTrack)
	{										
		mov->audioTrack = newNode;
	}
	else
	{
		mov->audioTail->next = newNode;
	}
	mov->audioTail = newNode;

	return;
}

static void audio_list_free(MP4_CONTEXT *mov)
{
	MP2_FRAME *tmpNode = NULL;

	if(NULL == mov)
	{
		return;
	}
	else
	{
		while(mov->audioTrack != NULL)
		{
			tmpNode = mov->audioTrack;
			mov->audioTrack = mov->audioTrack->next;
			free(tmpNode);
		}
		mov->audioTrack = mov->audioTail = NULL;
		return;
	}
}


static int update_size(FILE *fp, long pos, MP4_CONTEXT *mov)
{
	long curPos = 0;

	if(NULL == fp || NULL == mov)
	{
		return -1;
	}

	curPos = mp4_ftell(fp);
	if(-1 == curPos)
	{
		return -1;
	}
	else
	{
		mov->returnError += mp4_fseek(fp, pos, SEEK_SET);
		mov->returnError += mp4_put_be32(fp, (unsigned int)(curPos - pos));	/* rewrite size */
		mov->returnError += mp4_fseek(fp, curPos, SEEK_SET);
		return 0;
	}
}

static int mp4_write_tkhd_tag(FILE *fp, MP4_CONTEXT *mov)
{
	if(NULL == fp || NULL == mov)
	{
		return -1;
	}
	else
	{
		mov->returnError += mp4_put_be32(fp, 92); /* size */
		mov->returnError += mp4_put_tag(fp, "tkhd");
		mov->returnError += mp4_put_byte(fp, 0);
		mov->returnError += mp4_put_be24(fp, 0xf);	/* flags (track enabled) */

		mov->returnError += mp4_put_be32(fp, mov->time);	/* creation time */
		mov->returnError += mp4_put_be32(fp, mov->time);	/* modification time */

		mov->returnError += mp4_put_be32(fp, 1);	/* track-id */
		mov->returnError += mp4_put_be32(fp, 0);	/* reserved */

		if(0 == mov->movToMediaVideo)
		{
			return -1;
		}
		mov->returnError += mp4_put_be32(fp, mov->videoTail->timeStamp - mov->videoTrack->timeStamp + 50);

		mov->returnError += mp4_put_be32(fp, 0); /* reserved */
		mov->returnError += mp4_put_be32(fp, 0); /* reserved */
		mov->returnError += mp4_put_be32(fp, 0x0); /* reserved (Layer & Alternate group) */

		mov->returnError += mp4_put_be16(fp, 0); 
		mov->returnError += mp4_put_be16(fp, 0); /* reserved */

		/* Matrix structure */
		mov->returnError += mp4_put_be32(fp, 0x00010000); /* reserved */
		mov->returnError += mp4_put_be32(fp, 0x0); /* reserved */
		mov->returnError += mp4_put_be32(fp, 0x0); /* reserved */
		mov->returnError += mp4_put_be32(fp, 0x0); /* reserved */
		mov->returnError += mp4_put_be32(fp, 0x00010000); /* reserved */
		mov->returnError += mp4_put_be32(fp, 0x0); /* reserved */
		mov->returnError += mp4_put_be32(fp, 0x0); /* reserved */
		mov->returnError += mp4_put_be32(fp, 0x0); /* reserved */
		mov->returnError += mp4_put_be32(fp, 0x40000000); /* reserved */

		mov->returnError += mp4_put_be32(fp, mov->width);
		mov->returnError += mp4_put_be32(fp, mov->height);

		return 0;
	}
}

static int mp4_write_tkhd_tag_2(FILE *fp, MP4_CONTEXT *mov)
{
	if(NULL == fp || NULL == mov)
	{
		return -1;
	}
	else
	{
		mov->returnError += mp4_put_be32(fp, 92); /* size */
		mov->returnError += mp4_put_tag(fp, "tkhd");
		mov->returnError += mp4_put_byte(fp, 0);
		mov->returnError += mp4_put_be24(fp, 0xf); /* flags (track enabled) */

		mov->returnError += mp4_put_be32(fp, mov->time);/* creation time */
		mov->returnError += mp4_put_be32(fp, mov->time);/* modification time */

		mov->returnError += mp4_put_be32(fp, 2);/* track-id */
		mov->returnError += mp4_put_be32(fp, 0); /* reserved */

		if(0 == mov->movToMediaAudio)
		{
			return -1;
		}
		mov->returnError += mp4_put_be32(fp, mov->audioTrackSize*mov->audioSampleDuration/mov->movToMediaAudio);

		mov->returnError += mp4_put_be32(fp, 0); /* reserved */
		mov->returnError += mp4_put_be32(fp, 0); /* reserved */
		mov->returnError += mp4_put_be32(fp, 0x0); /* reserved (Layer & Alternate group) */

		mov->returnError += mp4_put_be16(fp, 0x0100);
		mov->returnError += mp4_put_be16(fp, 0); /* reserved */

		/* Matrix structure */
		mov->returnError += mp4_put_be32(fp, 0x00010000); /* reserved */
		mov->returnError += mp4_put_be32(fp, 0x0); /* reserved */
		mov->returnError += mp4_put_be32(fp, 0x0); /* reserved */
		mov->returnError += mp4_put_be32(fp, 0x0); /* reserved */
		mov->returnError += mp4_put_be32(fp, 0x00010000); /* reserved */
		mov->returnError += mp4_put_be32(fp, 0x0); /* reserved */
		mov->returnError += mp4_put_be32(fp, 0x0); /* reserved */
		mov->returnError += mp4_put_be32(fp, 0x0); /* reserved */
		mov->returnError += mp4_put_be32(fp, 0x40000000); /* reserved */

		mov->returnError += mp4_put_be32(fp, 0);
		mov->returnError += mp4_put_be32(fp, 0);

		return 0;
	}
}


static int mp4_write_mdhd_tag(FILE *fp,MP4_CONTEXT *mov)
{
	if(NULL == fp || NULL == mov)
	{
		return -1;
	}
	else
	{
		mov->returnError += mp4_put_be32(fp, 32); /* size */
		mov->returnError += mp4_put_tag(fp, "mdhd");

		mov->returnError += mp4_put_byte(fp, 0);
		mov->returnError += mp4_put_be24(fp, 0); /* flags */

		mov->returnError += mp4_put_be32(fp, mov->time); /* creation time */
		mov->returnError += mp4_put_be32(fp, mov->time); /* modification time */

		mov->returnError += mp4_put_be32(fp, mov->videoTimeScale); /* time scale (sample rate for audio) */
		mov->returnError += mp4_put_be32(fp, mov->videoTail->timeStamp - mov->videoTrack->timeStamp + 50); /* duration */

		mov->returnError += mp4_put_be16(fp, MP4_LANGUAGE); /* language */
		mov->returnError += mp4_put_be16(fp, 0); /* reserved (quality) */

		return 0;
	}
}

static int mp4_write_mdhd_tag_2(FILE *fp,MP4_CONTEXT *mov)
{
	if(NULL == fp || NULL == mov)
	{
		return -1;
	}
	else
	{
		mov->returnError += mp4_put_be32(fp, 32); /* size */
		mov->returnError += mp4_put_tag(fp, "mdhd");
		mov->returnError += mp4_put_byte(fp, 0);
		mov->returnError += mp4_put_be24(fp, 0); /* flags */

		mov->returnError += mp4_put_be32(fp, mov->time); /* creation time */
		mov->returnError += mp4_put_be32(fp, mov->time); /* modification time */

		mov->returnError += mp4_put_be32(fp, mov->audioTimeScale); /* time scale (sample rate for audio) */
		mov->returnError += mp4_put_be32(fp, mov->audioSampleDuration * mov->audioTrackSize); /* duration */

		mov->returnError += mp4_put_be16(fp, MP4_LANGUAGE); /* language */
		mov->returnError += mp4_put_be16(fp, 0); /* reserved (quality) */

		return 0;
	}
}

static int mp4_write_hdlr_tag(FILE *fp, MP4_CONTEXT *mov)
{
	char *hdlr = NULL;
	char *descr = NULL;
	char *hdlrType = NULL;
	long pos = 0;

	if(NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if(-1 == pos)
	{
		return -1;
	}

	hdlr = "mhlr";
	hdlrType = "vide"; /* video */
	descr = "MP4 for IPC";

	mov->returnError += mp4_put_be32(fp, 0); /* size */
	mov->returnError += mp4_put_tag(fp, "hdlr");
	mov->returnError += mp4_put_be32(fp, 0); /* Version & flags */
	mov->returnError += mp4_put_buffer(fp, (unsigned char *)hdlr, 4); /* handler */
	mov->returnError += mp4_put_tag(fp, hdlrType); /* handler type */
	mov->returnError += mp4_put_be32(fp ,0); /* reserved */
	mov->returnError += mp4_put_be32(fp ,0); /* reserved */
	mov->returnError += mp4_put_be32(fp ,0); /* reserved */
	mov->returnError += mp4_put_buffer(fp, (unsigned char *)descr, strlen(descr));	/* handler description */
	mov->returnError += mp4_put_byte(fp, 0);						/* ����ַ�����\0 */

	return update_size(fp, pos, mov);
}

static int mp4_write_hdlr_tag_2(FILE *fp, MP4_CONTEXT *mov)
{
	char *hdlr = NULL;
	char *descr = NULL;
	char *hdlrType = NULL;
	long pos = 0;

	if(NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if(-1 == pos)
	{
		return -1;
	}

	hdlr = "mhlr";
	hdlrType = "soun"; /* sound */
	descr = "MP4 for IPC";

	mov->returnError += mp4_put_be32(fp, 0); /* size */
	mov->returnError += mp4_put_tag(fp, "hdlr");
	mov->returnError += mp4_put_be32(fp, 0); /* Version & flags */
	mov->returnError += mp4_put_buffer(fp, (unsigned char *)hdlr, 4); /* handler */
	mov->returnError += mp4_put_tag(fp, hdlrType); /* handler type */
	mov->returnError += mp4_put_be32(fp ,0); /* reserved */
	mov->returnError += mp4_put_be32(fp ,0); /* reserved */
	mov->returnError += mp4_put_be32(fp ,0); /* reserved */
	mov->returnError += mp4_put_buffer(fp, (unsigned char *)descr, strlen(descr));	/* handler description */
	mov->returnError += mp4_put_byte(fp, 0);						/* ����ַ�����\0 */

	return update_size(fp, pos, mov);
}



static int mp4_write_vmhd_tag(FILE *fp, MP4_CONTEXT *mov)
{
	if(NULL == fp || NULL == mov)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 0x14); /* size */
	mov->returnError += mp4_put_tag(fp, "vmhd");
	mov->returnError += mp4_put_be32(fp, 0x01); /* version & flags */
	mov->returnError += mp4_put_be32(fp, 0); /* reserved (graphics mode = copy) */
	mov->returnError += mp4_put_be32(fp, 0);

	return 0;
}

static int mp4_write_smhd_tag(FILE *fp, MP4_CONTEXT *mov)
{
	if(NULL == fp || NULL == mov)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 0x10); /* size */
	mov->returnError += mp4_put_tag(fp, "smhd");
	mov->returnError += mp4_put_be32(fp, 0x00); /* version & flags */
	mov->returnError += mp4_put_be32(fp, 0); /* reserved */

	return 0;
}


static int mp4_write_dref_tag(FILE *fp, MP4_CONTEXT *mov)
{
	if(NULL == fp || NULL == mov)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 28); /* size */
	mov->returnError += mp4_put_tag(fp, "dref");
	mov->returnError += mp4_put_be32(fp, 0); /* version & flags */
	mov->returnError += mp4_put_be32(fp, 1); /* entry count */
	mov->returnError += mp4_put_be32(fp, 0xc); /* size */
	mov->returnError += mp4_put_tag(fp, "url ");
	mov->returnError += mp4_put_be32(fp, 1); /* version & flags */

	return 0;
}

static int mp4_write_dref_tag_2(FILE *fp, MP4_CONTEXT *mov)
{
	if(NULL == fp || NULL == mov)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 28); /* size */
	mov->returnError += mp4_put_tag(fp, "dref");
	mov->returnError += mp4_put_be32(fp, 0); /* version & flags */
	mov->returnError += mp4_put_be32(fp, 1); /* entry count */
	mov->returnError += mp4_put_be32(fp, 0xc); /* size */
	mov->returnError += mp4_put_tag(fp, "url ");
	mov->returnError += mp4_put_be32(fp, 1); /* version & flags */

	return 0;
}


static int mp4_write_dinf_tag(FILE *fp, MP4_CONTEXT *mov)
{
	long pos = 0;

	if(NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if(-1 == pos)
	{
		return -1;
	}

	mov->returnError += mp4_put_be32(fp, 0); /* size */
	mov->returnError += mp4_put_tag(fp, "dinf");
	mov->returnError += mp4_write_dref_tag(fp, mov);

	return update_size(fp, pos, mov);
}

static int mp4_write_dinf_tag_2(FILE *fp, MP4_CONTEXT *mov)
{
	long pos = 0;

	if(NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if(-1 == pos)
	{
		return -1;
	}

	mov->returnError += mp4_put_be32(fp, 0); /* size */
	mov->returnError += mp4_put_tag(fp, "dinf");
	mov->returnError += mp4_write_dref_tag_2(fp, mov);

	return update_size(fp, pos, mov);
}


static int mp4_write_stts_tag(FILE *fp,MP4_CONTEXT *mov)
{
	unsigned int i = 0;
	long pos = 0;
	MJPEG_LIST *tmpNode = NULL;

	if(NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if(-1 == pos)
	{
		return -1;
	}

	mov->returnError += mp4_put_be32(fp, 0); 
	mov->returnError += mp4_put_tag(fp, "stts");
	mov->returnError += mp4_put_be32(fp, 0); /* version & flags */
	mov->returnError += mp4_put_be32(fp, mov->videoTrackSize); /* entry count */
	tmpNode = mov->videoTrack;
	for (i = 0; i < mov->videoTrackSize - 1; i++)
	{
		mov->returnError += mp4_put_be32(fp, 1);
		mov->returnError += mp4_put_be32(fp, tmpNode->next->timeStamp - tmpNode->timeStamp);
		tmpNode = tmpNode->next;
	}
	mov->returnError += mp4_put_be32(fp, 1);
	mov->returnError += mp4_put_be32(fp, 50);

	return update_size(fp, pos, mov); 
}

/* Time to sample atom */
static int mp4_write_stts_tag_2(FILE *fp,MP4_CONTEXT *mov)
{
	unsigned int entries = 0;
	unsigned int atomSize = 0;
	unsigned int i = 0;

	if(NULL == fp || NULL == mov)
	{
		return -1;
	}

	entries = 1;
	atomSize = 16 + (entries * 8);
	mov->returnError += mp4_put_be32(fp, atomSize); 
	mov->returnError += mp4_put_tag(fp, "stts");
	mov->returnError += mp4_put_be32(fp, 0); /* version & flags */
	mov->returnError += mp4_put_be32(fp, entries); /* entry count */
	for (i = 0; i < entries; i++)
	{
		mov->returnError += mp4_put_be32(fp, mov->audioTrackSize);
		mov->returnError += mp4_put_be32(fp, mov->audioSampleDuration);
	}

	return 0; 
}

static int mp4_write_stsc_tag(FILE *fp, MP4_CONTEXT *mov)
{
	int index = 0;
	long entryPos = 0;
	long curPos = 0;
	long pos = 0;

	if(NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if(-1 == pos)
	{
		return -1;
	}

	mov->returnError += mp4_put_be32(fp, 0); /* size */
	mov->returnError += mp4_put_tag(fp, "stsc");
	mov->returnError += mp4_put_be32(fp, 0); /* version & flags */

	entryPos = mp4_ftell(fp);
	if(-1 == entryPos)
	{
		return -1;
	}

	mov->returnError += mp4_put_be32(fp, mov->videoTrackSize);
	mov->returnError += mp4_put_be32(fp, 1); /* first chunk */
	mov->returnError += mp4_put_be32(fp, mov->videoTrackSize); /* samples per chunk */
	mov->returnError += mp4_put_be32(fp, 0x1); /* sample description index */

	index = 1;
	curPos = mp4_ftell(fp);
	if(-1 == curPos)
	{
		return -1;
	}

	mov->returnError += mp4_fseek(fp, entryPos, SEEK_SET);
	mov->returnError += mp4_put_be32(fp, index); /* rewrite size */
	mov->returnError += mp4_fseek(fp, curPos, SEEK_SET);

	return update_size(fp, pos, mov);
}

static int mp4_write_stsc_tag_2(FILE *fp, MP4_CONTEXT *mov)
{
	int index = 0;
	long entryPos = 0;
	long curPos = 0;
	long pos = 0;

	if(NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if(-1 == pos)
	{
		return -1;
	}

	mov->returnError += mp4_put_be32(fp, 0); /* size */
	mov->returnError += mp4_put_tag(fp, "stsc");
	mov->returnError += mp4_put_be32(fp, 0); /* version & flags */

	entryPos = mp4_ftell(fp);
	if(-1 == entryPos)
	{
		return -1;
	}

	mov->returnError += mp4_put_be32(fp, mov->audioTrackSize);
	mov->returnError += mp4_put_be32(fp, 1); /* first chunk */
	mov->returnError += mp4_put_be32(fp, mov->audioTrackSize);/* samples per chunk */
	mov->returnError += mp4_put_be32(fp, 0x1); /* sample description index */

	index = 1;
	curPos = mp4_ftell(fp);
	if(-1 == curPos)
	{
		return -1;
	}

	mov->returnError += mp4_fseek(fp, entryPos, SEEK_SET);
	mov->returnError += mp4_put_be32(fp, index); /* rewrite size */
	mov->returnError += mp4_fseek(fp, curPos, SEEK_SET);

	return update_size(fp, pos, mov);
}


static int mp4_write_stsz_tag(FILE *fp, MP4_CONTEXT *mov)
{
	int i = 0;
	MJPEG_LIST *track = NULL;
	long pos = 0;

	if(NULL == fp || NULL == mov)
	{
		return -1;
	}

	track = mov->videoTrack;
	pos = mp4_ftell(fp);
	if(-1 == pos)
	{
		return -1;
	}

	mov->returnError += mp4_put_be32(fp, 0); /* size */
	mov->returnError += mp4_put_tag(fp, "stsz");
	mov->returnError += mp4_put_be32(fp, 0); /* version & flags */
	mov->returnError += mp4_put_be32(fp, 0); 
	mov->returnError += mp4_put_be32(fp, mov->videoTrackSize); /* sample count */
	for (i = 0; i < mov->videoTrackSize; i++)
	{
		mov->returnError += mp4_put_be32(fp, track->mjpegSize);
		track = track->next;
	}

	return update_size(fp, pos, mov);
}

static int mp4_write_stsz_tag_2(FILE *fp, MP4_CONTEXT *mov)
{
	int i = 0;
	MP2_FRAME *track = NULL;
	long pos = 0;

	if(NULL == fp || NULL == mov)
	{
		return -1;
	}

	track = mov->audioTrack;
	pos = mp4_ftell(fp);
	if(-1 == pos)
	{
		return -1;
	}

	mov->returnError += mp4_put_be32(fp, 0); /* size */
	mov->returnError += mp4_put_tag(fp, "stsz");
	mov->returnError += mp4_put_be32(fp, 0); /* version & flags */
	mov->returnError += mp4_put_be32(fp, 0); 
	mov->returnError += mp4_put_be32(fp, mov->audioTrackSize); /* sample count */
	for (i = 0; i < mov->audioTrackSize; i++)
	{
		mov->returnError += mp4_put_be32(fp, track->frameSize);
		track = track->next;
	}

	return update_size(fp, pos, mov);
}


static int mp4_write_stco_tag(FILE *fp, MP4_CONTEXT *mov)
{
	unsigned int offsetBegin = 0;
	long pos = 0;

	if(NULL == fp || NULL == mov)
	{
		return -1;
	}

	offsetBegin = mov->mdatPos + 8;

	pos = mp4_ftell(fp);
	if(-1 == pos)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 0); /* size */
	mov->returnError += mp4_put_tag(fp, "stco");
	mov->returnError += mp4_put_be32(fp, 0); /* version & flags */
	mov->returnError += mp4_put_be32(fp, 1); /* entry count */
	mov->returnError += mp4_put_be32(fp, offsetBegin);

	return update_size(fp, pos, mov);
}

static int mp4_write_stco_tag_2(FILE *fp, MP4_CONTEXT *mov)
{
	unsigned int offsetBegin = 0;
	long pos = 0;

	if(NULL == fp || NULL == mov)
	{
		return -1;
	}

	offsetBegin = mov->audioPos;	

	pos = mp4_ftell(fp);
	if(-1 == pos)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 0); /* size */
	mov->returnError += mp4_put_tag(fp, "stco");
	mov->returnError += mp4_put_be32(fp, 0); /* version & flags */
	mov->returnError += mp4_put_be32(fp, 1); /* entry count */
	mov->returnError += mp4_put_be32(fp, offsetBegin);

	return update_size(fp, pos, mov);
}


static int mp4_write_esds_tag(FILE *fp, MP4_CONTEXT *mov)
{
	long pos = 0;

	if(NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if(-1 == pos)
	{
		return -1;
	}

	mov->returnError += mp4_put_be32(fp, 0);
	mov->returnError += mp4_put_tag(fp, "esds");
	mov->returnError += mp4_put_be32(fp,0);/* version and flag */
	mov->returnError += mp4_put_be16(fp,0x0380);
	mov->returnError += mp4_put_be16(fp,0x8080);
	mov->returnError += mp4_put_be16(fp,0x1B00);
	mov->returnError += mp4_put_be16(fp,0x0100);
	mov->returnError += mp4_put_byte(fp,0x04);
	mov->returnError += mp4_put_be24(fp,0x808080);
	mov->returnError += mp4_put_be32(fp,0x0D6C1100);
	mov->returnError += mp4_put_be24(fp,0x000000);
	mov->returnError += mp4_put_be32(fp,0x0D79D400);
	mov->returnError += mp4_put_be32(fp,0x0D79D406);
	mov->returnError += mp4_put_be24(fp,0x808080);
	mov->returnError += mp4_put_be16(fp,0x0102);

	return update_size(fp, pos, mov);
}

static int mp4_write_esds_tag_2(FILE *fp, MP4_CONTEXT *mov)
{
	long pos = 0;

	if(NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if(-1 == pos)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 0);
	mov->returnError += mp4_put_tag(fp, "esds");
	mov->returnError += mp4_put_be32(fp,0);				/* version and flag */
	mov->returnError += mp4_put_be16(fp,0x0315);
	mov->returnError += mp4_put_be16(fp,0);
	mov->returnError += mp4_put_be16(fp,0x0004);
	mov->returnError += mp4_put_be16(fp,0x0D6B);
	mov->returnError += mp4_put_byte(fp,0x15);
	mov->returnError += mp4_put_be24(fp,0x0001A2);
	mov->returnError += mp4_put_be32(fp,0x00020A70);
	mov->returnError += mp4_put_be32(fp,0x0001F3F8);
	mov->returnError += mp4_put_be24(fp,0x060102);

	return update_size(fp, pos, mov);
}


static int mp4_write_video_tag(FILE *fp, MP4_CONTEXT *mov)
{
	unsigned char compressorName[32];
	long pos = 0;

	if(NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if(-1 == pos)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 0); /* size */
	mov->returnError += mp4_put_tag(fp,"mp4v");
	mov->returnError += mp4_put_be32(fp, 0); /* Reserved */
	mov->returnError += mp4_put_be16(fp, 0); /* Reserved */ 
	mov->returnError += mp4_put_be16(fp, 1); /* Data-reference index */
	mov->returnError += mp4_put_be16(fp, 0); /* Codec stream version */
	mov->returnError += mp4_put_be16(fp, 0); /* Codec stream revision (=0) */
	mov->returnError += mp4_put_be32(fp, 0); /* Reserved */
	mov->returnError += mp4_put_be32(fp, 0); /* Reserved */
	mov->returnError += mp4_put_be32(fp, 0); /* Reserved */

	mov->returnError += mp4_put_be16(fp, mov->width); /* Video width */
	mov->returnError += mp4_put_be16(fp, mov->height); /* Video height */
	mov->returnError += mp4_put_be32(fp, 0x00480000); /* Horizontal resolution 72dpi */
	mov->returnError += mp4_put_be32(fp, 0x00480000); /* Vertical resolution 72dpi */
	mov->returnError += mp4_put_be32(fp, 0); /* Data size (= 0) */
	mov->returnError += mp4_put_be16(fp, 1); /* Frame count (= 1) */

	memset(compressorName,0,32);
	mov->returnError += mp4_put_byte(fp, strlen((char *)compressorName));
	mov->returnError += mp4_put_buffer(fp, compressorName, 31);
	mov->returnError += mp4_put_be16(fp, 0x18); /* Reserved */
	mov->returnError += mp4_put_be16(fp, 0xffff); /* Reserved */

	mov->returnError += mp4_write_esds_tag(fp, mov);

	return update_size(fp, pos, mov);
}

static int mp4_write_audio_tag(FILE *fp, MP4_CONTEXT *mov)
{
	long pos = 0;

	if(NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if(-1 == pos)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 0); /* size */
	mov->returnError += mp4_put_tag(fp,"mp4a");
	mov->returnError += mp4_put_be32(fp, 0); /* Reserved */
	mov->returnError += mp4_put_be16(fp, 0); /* Reserved */ 
	mov->returnError += mp4_put_be16(fp, 1); /* Data-reference index */
	mov->returnError += mp4_put_be32(fp, 0); /* Reserved */
	mov->returnError += mp4_put_be32(fp, 0); /* Reserved */
	mov->returnError += mp4_put_be16(fp,1);
	mov->returnError += mp4_put_be16(fp,16);
	mov->returnError += mp4_put_be32(fp,0);
	mov->returnError += mp4_put_be32(fp,((unsigned int)(mov->audioTimeScale))<<16);

	mov->returnError += mp4_write_esds_tag_2(fp, mov);

	return update_size(fp, pos, mov);
}


static int mp4_write_stsd_tag(FILE *fp, MP4_CONTEXT *mov)
{
	long pos = 0;

	if(NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if(-1 == pos)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 0); /* size */
	mov->returnError += mp4_put_tag(fp, "stsd");
	mov->returnError += mp4_put_be32(fp, 0); /* version & flags */
	mov->returnError += mp4_put_be32(fp, 1); /* entry count */

	mov->returnError += mp4_write_video_tag(fp, mov);

	return update_size(fp, pos, mov);
}

static int mp4_write_stsd_tag_2(FILE *fp, MP4_CONTEXT *mov)
{
	long pos = 0;

	if(NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if(-1 == pos)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 0); /* size */
	mov->returnError += mp4_put_tag(fp, "stsd");
	mov->returnError += mp4_put_be32(fp, 0); /* version & flags */
	mov->returnError += mp4_put_be32(fp, 1); /* entry count */

	mov->returnError += mp4_write_audio_tag(fp, mov);

	return update_size(fp, pos, mov);
}

static int mp4_write_stbl_tag(FILE *fp, MP4_CONTEXT *mov)
{
	long pos = 0;

	if(NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if(-1 == pos)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 0); /* size */
	mov->returnError += mp4_put_tag(fp, "stbl");
	mov->returnError += mp4_write_stsd_tag(fp, mov);
	mov->returnError += mp4_write_stts_tag(fp, mov);
	mov->returnError += mp4_write_stsc_tag(fp, mov);
	mov->returnError += mp4_write_stsz_tag(fp, mov);
	mov->returnError += mp4_write_stco_tag(fp, mov);

	return update_size(fp, pos, mov);
}

static int mp4_write_stbl_tag_2(FILE *fp, MP4_CONTEXT *mov)
{
	long pos = 0;

	if(NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if(-1 == pos)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 0); /* size */
	mov->returnError += mp4_put_tag(fp, "stbl");
	mov->returnError += mp4_write_stsd_tag_2(fp, mov);
	mov->returnError += mp4_write_stts_tag_2(fp, mov);
	mov->returnError += mp4_write_stsc_tag_2(fp, mov);
	mov->returnError += mp4_write_stsz_tag_2(fp, mov);
	mov->returnError += mp4_write_stco_tag_2(fp, mov);

	return update_size(fp, pos, mov);
}


static int mp4_write_minf_tag(FILE *fp, MP4_CONTEXT *mov)
{
	long pos = 0;

	if(NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if(-1 == pos)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 0); /* size */
	mov->returnError += mp4_put_tag(fp, "minf");

	mov->returnError += mp4_write_vmhd_tag(fp, mov);
	mov->returnError += mp4_write_dinf_tag(fp, mov);
	mov->returnError += mp4_write_stbl_tag(fp, mov);

	return update_size(fp, pos, mov);
}

static int mp4_write_minf_tag_2(FILE *fp, MP4_CONTEXT *mov)
{
	long pos = 0;

	if(NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if(-1 == pos)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 0); /* size */
	mov->returnError += mp4_put_tag(fp, "minf");

	mov->returnError += mp4_write_smhd_tag(fp, mov);
	mov->returnError += mp4_write_dinf_tag_2(fp, mov);
	mov->returnError += mp4_write_stbl_tag_2(fp, mov);

	return update_size(fp, pos, mov);
}


static int mp4_write_mdia_tag(FILE *fp, MP4_CONTEXT *mov)
{
	long pos = 0;

	if(NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if(-1 == pos)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 0); /* size */
	mov->returnError += mp4_put_tag(fp, "mdia");
	mov->returnError += mp4_write_mdhd_tag(fp, mov);
	mov->returnError += mp4_write_hdlr_tag(fp, mov);
	mov->returnError += mp4_write_minf_tag(fp, mov);

	return update_size(fp, pos, mov);
}

static int mp4_write_mdia_tag_2(FILE *fp, MP4_CONTEXT *mov)
{
	long pos = 0;

	if(NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if(-1 == pos)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 0); /* size */
	mov->returnError += mp4_put_tag(fp, "mdia");
	mov->returnError += mp4_write_mdhd_tag_2(fp, mov);
	mov->returnError += mp4_write_hdlr_tag_2(fp, mov);
	mov->returnError += mp4_write_minf_tag_2(fp, mov);

	return update_size(fp, pos, mov);
}

static int mp4_write_edts_tag(FILE *fp, MP4_CONTEXT *mov)
{
	if(NULL == fp || NULL == mov)
	{
		return -1;
	}
	else
	{
		mov->returnError += mp4_put_be32(fp, 0x24); /* size */
		mov->returnError += mp4_put_tag(fp, "edts");
		mov->returnError += mp4_put_be32(fp, 0x1c); /* size */
		mov->returnError += mp4_put_tag(fp, "elst");
		mov->returnError += mp4_put_be32(fp, 0x0);
		mov->returnError += mp4_put_be32(fp, 0x1);
		mov->returnError += mp4_put_be32(fp, mov->videoTail->timeStamp - mov->videoTrack->timeStamp + 50);
		mov->returnError += mp4_put_be32(fp,0);
		mov->returnError += mp4_put_be32(fp, 0x00010000);

		return 0;
	}
}


static int mp4_write_trak_tag(FILE *fp, MP4_CONTEXT *mov)
{
	long pos = 0;

	if(NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if(-1 == pos)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 0); /* size */
	mov->returnError += mp4_put_tag(fp, "trak");
	mov->returnError += mp4_write_tkhd_tag(fp, mov);
	mov->returnError += mp4_write_edts_tag(fp, mov); 
	mov->returnError += mp4_write_mdia_tag(fp, mov);

	return update_size(fp, pos, mov);
}

static int mp4_write_trak_tag_2(FILE *fp, MP4_CONTEXT *mov)
{
	long pos = 0;

	if(NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if(-1 == pos)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 0); /* size */
	mov->returnError += mp4_put_tag(fp, "trak");
	mov->returnError += mp4_write_tkhd_tag_2(fp, mov);
	mov->returnError += mp4_write_mdia_tag_2(fp, mov);

	return update_size(fp, pos, mov);
}


static int mp4_write_mvhd_tag(FILE *fp, MP4_CONTEXT *mov)
{
	int maxTrackID = 2;
	unsigned int maxTrackDuration = 0;
	unsigned int videoDuration = 0;
	unsigned int audioDuration = 0;

	if(NULL == fp || NULL == mov)
	{
		return -1;
	}

	if(0 == mov->movToMediaVideo || 0 == mov->movToMediaAudio)
	{
		return -1;
	}
	videoDuration = mov->videoTail->timeStamp - mov->videoTrack->timeStamp + 50;
	audioDuration = mov->audioTrackSize*mov->audioSampleDuration/mov->movToMediaAudio;
	if(videoDuration > audioDuration)
	{
		maxTrackDuration = videoDuration;
	}
	else
	{
		maxTrackDuration = audioDuration;
	}

	mov->returnError += mp4_put_be32(fp, 108); /* size */
	mov->returnError += mp4_put_tag(fp, "mvhd");
	mov->returnError += mp4_put_byte(fp, 0);
	mov->returnError += mp4_put_be24(fp, 0); /* flags */
	mov->returnError += mp4_put_be32(fp, mov->time); /* creation time */
	mov->returnError += mp4_put_be32(fp, mov->time); /* modification time */
	mov->returnError += mp4_put_be32(fp, mov->movTimeScale);
	mov->returnError += mp4_put_be32(fp, maxTrackDuration); /* duration of longest track */

	mov->returnError += mp4_put_be32(fp, 0x00010000); /* reserved (preferred rate) 1.0 = normal */
	mov->returnError += mp4_put_be16(fp, 0x0100); /* reserved (preferred volume) 1.0 = normal */
	mov->returnError += mp4_put_be16(fp, 0); /* reserved */
	mov->returnError += mp4_put_be32(fp, 0); /* reserved */
	mov->returnError += mp4_put_be32(fp, 0); /* reserved */

	/* Matrix structure */
	mov->returnError += mp4_put_be32(fp, 0x00010000); /* reserved */
	mov->returnError += mp4_put_be32(fp, 0x0); /* reserved */
	mov->returnError += mp4_put_be32(fp, 0x0); /* reserved */
	mov->returnError += mp4_put_be32(fp, 0x0); /* reserved */
	mov->returnError += mp4_put_be32(fp, 0x00010000); /* reserved */
	mov->returnError += mp4_put_be32(fp, 0x0); /* reserved */
	mov->returnError += mp4_put_be32(fp, 0x0); /* reserved */
	mov->returnError += mp4_put_be32(fp, 0x0); /* reserved */
	mov->returnError += mp4_put_be32(fp, 0x40000000); /* reserved */

	mov->returnError += mp4_put_be32(fp, 0); /* reserved (preview time) */
	mov->returnError += mp4_put_be32(fp, 0); /* reserved (preview duration) */
	mov->returnError += mp4_put_be32(fp, 0); /* reserved (poster time) */
	mov->returnError += mp4_put_be32(fp, 0); /* reserved (selection time) */
	mov->returnError += mp4_put_be32(fp, 0); /* reserved (selection duration) */
	mov->returnError += mp4_put_be32(fp, 0); /* reserved (current time) */
	mov->returnError += mp4_put_be32(fp, maxTrackID + 1); /* Next track id */

	return 0;
}


static int mp4_write_moov_tag(FILE *fp, MP4_CONTEXT *mov)
{
	long pos = 0;

	if(NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if(-1 == pos)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 0); /* size placeholder*/
	mov->returnError += mp4_put_tag(fp, "moov");
	mov->returnError += mp4_write_mvhd_tag(fp, mov);
	mov->returnError += mp4_write_trak_tag(fp, mov);
	mov->returnError += mp4_write_trak_tag_2(fp, mov);

	return update_size(fp, pos, mov);
}

static int mp4_write_trailer(FILE *fp,MP4_CONTEXT *mov)
{
	long moovPos = 0;

	if(NULL == fp || NULL == mov)
	{
		return -1;
	}

	moovPos = mp4_ftell(fp);
	if(-1 == moovPos)
	{
		return -1;
	}
	mov->returnError += mp4_fseek(fp, mov->mdatPos, SEEK_SET);
	mov->returnError += mp4_put_be32(fp, (mov->mdatSize) + 8);
	mov->returnError += mp4_fseek(fp, moovPos, SEEK_SET);
	mov->returnError += mp4_write_moov_tag(fp, mov);

	return 0;
}

static int mp4_write_ftyp_tag(FILE *fp, MP4_CONTEXT * mov)
{
	long pos = 0;

	if(NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if(-1 == pos)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 0); /* size */
	mov->returnError += mp4_put_tag(fp, "ftyp");
	mov->returnError += mp4_put_tag(fp,"isom");
	mov->returnError += mp4_put_be32(fp, 0x00000200);
	mov->returnError += mp4_put_tag(fp,"isom");
	mov->returnError += mp4_put_tag(fp, "iso2");
	mov->returnError += mp4_put_tag(fp, "avc1");
	mov->returnError += mp4_put_tag(fp, "mp41");/* ??mp42 */

	return update_size(fp, pos, mov);
}


static int mp4_write_mdat_tag(FILE *fp, MP4_CONTEXT *mov)
{
	if(NULL == fp || NULL == mov)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 8); 
	mov->returnError += mp4_put_tag(fp, "free");
	mov->mdatPos = mp4_ftell(fp);
	if(-1 == mov->mdatPos)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 0); /* size placeholder*/
	mov->returnError += mp4_put_tag(fp, "mdat");

	return 0;
}

static int mp4_write_header(FILE *fp, MP4_CONTEXT *mov)
{
	if(NULL == fp || NULL == mov)
	{
		return -1;
	}
	mov->returnError += mp4_write_ftyp_tag(fp, mov);
	mov->returnError += mp4_write_mdat_tag(fp, mov);

	return 0;
}

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
void *mp4_write_head_jpeg(const wchar_t *mp4FilePath, const wchar_t *tmpMp2Path, int width, int height, int audioCodec)
{
	FILE *fpMp4 = NULL;
	FILE *fpMp2 = NULL;
	MP4_CONTEXT *l_mp4Mov = NULL;

	l_mp4Mov = (MP4_CONTEXT *)malloc(sizeof(struct MP4_CONTEXT));
	if(NULL == l_mp4Mov)
	{
		return NULL;
	}
	/* init mov struct */
	l_mp4Mov->fpMp4 = NULL;
	l_mp4Mov->fpMp2 = NULL;
	l_mp4Mov->mdatPos = 0;
	l_mp4Mov->mdatSize = 0;
	l_mp4Mov->videoTrackSize = 0;
	l_mp4Mov->videoTrack = NULL;
	l_mp4Mov->videoTail = NULL;
	l_mp4Mov->audioPos = 0;
	l_mp4Mov->audioTrack = NULL;
	l_mp4Mov->audioTail = NULL;
	l_mp4Mov->audioTrackSize = 0;
	l_mp4Mov->time = 0xC6BFDE31;
	l_mp4Mov->movTimeScale = 1000;
	l_mp4Mov->movToMediaVideo = 1;
	l_mp4Mov->movToMediaAudio = 16;
	l_mp4Mov->videoTimeScale = 1000;
	l_mp4Mov->audioTimeScale = 16000;
	l_mp4Mov->videoSampleDuration = 50;
	l_mp4Mov->audioSampleDuration = 1152;
	l_mp4Mov->returnError = 0;
	l_mp4Mov->width = 640;
	l_mp4Mov->height = 480;
	l_mp4Mov->hEnc = NULL;
	l_mp4Mov->audioCodec = 0;

	/* input param check */
	if(NULL == mp4FilePath || NULL == tmpMp2Path)
	{
		free(l_mp4Mov);
		return NULL;
	}

	if(1 == audioCodec)
	{
		l_mp4Mov->audioCodec = 1;
	}
	else if(2 == audioCodec)
	{
		l_mp4Mov->audioCodec = 2;
	}
	else
	{
		free(l_mp4Mov);
		return NULL;
	}

	/* width and height check */
	if(width <= 0 || height <= 0)
	{
		free(l_mp4Mov);
		return NULL;
	}
	else
	{
		l_mp4Mov->width = width;
		l_mp4Mov->height = height;
	}

	/* open mp4 and tmp mp2 file to write */
	fpMp4 = mp4_fopen(mp4FilePath, L"wb");
	if(NULL == fpMp4)
	{
		free(l_mp4Mov);
		return NULL;
	}
	l_mp4Mov->fpMp4 = fpMp4;
	fpMp2 = mp4_fopen(tmpMp2Path, L"wb+");
	if(NULL == fpMp2)
	{
		fclose(fpMp4);
		free(l_mp4Mov);
		return NULL;
	}
	l_mp4Mov->fpMp2 = fpMp2;

	l_mp4Mov->hEnc = MP2_encode_init(16000, 64000, 1);
	if(NULL == l_mp4Mov->hEnc)
	{
		printf("mp4:mp2 encoder init error!\n");
		fclose(fpMp4);
		fclose(fpMp2);
		free(l_mp4Mov);
		return NULL;
	}

	/* write mp4 header */
	l_mp4Mov->returnError += mp4_write_header(l_mp4Mov->fpMp4, l_mp4Mov);

	return (void *)l_mp4Mov;
}


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
int mp4_write_one_jpeg(unsigned char *jpegBuf, int bufSize, int timeStamp, void *handler)
{
	FILE *fpMp4 = NULL;
	int start = -1;
	int end = -1;
	int i = 0;
	int flag = 0;
	MJPEG_LIST *videoNode = NULL;
	MP4_CONTEXT *l_mp4Mov = NULL;

	l_mp4Mov = (MP4_CONTEXT *)handler;
	if(NULL == l_mp4Mov || NULL == jpegBuf || NULL == l_mp4Mov->fpMp4 || bufSize <= 0 || timeStamp < 0)
	{
		if(l_mp4Mov->fpMp4)
		{
			fclose(l_mp4Mov->fpMp4);
		}
		if(l_mp4Mov->fpMp2)
		{
			fclose(l_mp4Mov->fpMp2);
		}
		mjpeg_list_free(l_mp4Mov);
		MP2_encode_close(l_mp4Mov->hEnc);
		free(l_mp4Mov);
		//LOGI("timeStamp < %d",timeStamp);
		return -1;
	}
	fpMp4 = l_mp4Mov->fpMp4;

	/* check jpeg data header */
	for(i = 0; i < bufSize; i++)
	{
		if(0xFF == jpegBuf[i])
		{
			flag++;
		}
		else if(0xD8 == jpegBuf[i])
		{
			if(1 == flag)
			{
				start = i - 1;
				break;
			}
			else
			{
				flag = 0;
			}
		}
		else
		{
			flag = 0;
		}
	}
	if(start < 0)
	{
		if(l_mp4Mov->fpMp4)
		{
			fclose(l_mp4Mov->fpMp4);
		}
		if(l_mp4Mov->fpMp2)
		{
			fclose(l_mp4Mov->fpMp2);
		}
		mjpeg_list_free(l_mp4Mov);
		MP2_encode_close(l_mp4Mov->hEnc);
		free(l_mp4Mov);
		//LOGI("not jpeg");
		return -1;
	}

	flag = 0;

	/* check jpeg data end */
	for(i = start + 2; i < bufSize; i++)
	{
		if(0xFF == jpegBuf[i])
		{
			flag++;
		}
		else if(0xD9 == jpegBuf[i])
		{
			if(1 == flag)
			{
				end = i;
				break;
			}
			else
			{
				flag = 0;
			}
		}
		else
		{
			flag = 0;
		}
	}
	if(end < 0)
	{
		/*
		if(l_mp4Mov->fpMp4)
		{
		fclose(l_mp4Mov->fpMp4);
		}
		if(l_mp4Mov->fpMp2)
		{
		fclose(l_mp4Mov->fpMp2);
		}
		mjpeg_list_free(l_mp4Mov);
		MP2_encode_close(l_mp4Mov->hEnc);
		free(l_mp4Mov);
		//LOGI("not jpeg 2");
		return -1;
		*/
		return 0;
	}

	/* write data to mp4 */
	videoNode = (MJPEG_LIST *)malloc(sizeof(struct MJPEG_LIST));
	if(NULL == videoNode)
	{
		if(l_mp4Mov->fpMp4)
		{
			fclose(l_mp4Mov->fpMp4);
		}
		if(l_mp4Mov->fpMp2)
		{
			fclose(l_mp4Mov->fpMp2);
		}
		mjpeg_list_free(l_mp4Mov);
		MP2_encode_close(l_mp4Mov->hEnc);
		free(l_mp4Mov);
		//LOGI("malloc failed");
		return -1;
	}
	videoNode->mjpegSize = end - start + 1;
	videoNode->timeStamp = timeStamp;
	videoNode->next = NULL;		
	mjpeg_list_insert_end(videoNode, l_mp4Mov);
	l_mp4Mov->returnError += mp4_put_buffer(l_mp4Mov->fpMp4, &(jpegBuf[start]), videoNode->mjpegSize);		//���������mp4�ļ�
	l_mp4Mov->mdatSize += videoNode->mjpegSize;

	return 0;
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
int mp4_write_pcm_jpeg(unsigned char *pcmBuf, int bufSize, void *handler)
{
	FILE *fpMp2 = NULL;
	MP4_CONTEXT *l_mp4Mov = NULL;

	l_mp4Mov = (MP4_CONTEXT *)handler;
	fpMp2 = l_mp4Mov->fpMp2;
	if(1 == l_mp4Mov->audioCodec)
	{
		if(NULL == pcmBuf || NULL == fpMp2 || bufSize <= 0 || bufSize % 576 != 0)
		{
			if(l_mp4Mov->fpMp4)
			{
				fclose(l_mp4Mov->fpMp4);
			}
			if(l_mp4Mov->fpMp2)
			{
				fclose(l_mp4Mov->fpMp2);
			}
			mjpeg_list_free(l_mp4Mov);
			MP2_encode_close(l_mp4Mov->hEnc);
			free(l_mp4Mov);
			//LOGI("input check failed");
			return -1;
		}

		if(-1 == mp4_pcm_to_mp2(fpMp2, pcmBuf, bufSize, l_mp4Mov->hEnc))
		{
			if(l_mp4Mov->fpMp4)
			{
				fclose(l_mp4Mov->fpMp4);
			}
			if(l_mp4Mov->fpMp2)
			{
				fclose(l_mp4Mov->fpMp2);
			}
			mjpeg_list_free(l_mp4Mov);
			MP2_encode_close(l_mp4Mov->hEnc);
			free(l_mp4Mov);
			//LOGI("mp2 encoder failed");
			return -1;
		}
		else
		{
			return 0;
		}
	}
	else
	{
		if(NULL == pcmBuf || NULL == fpMp2 || bufSize <= 0 || bufSize % 2304 != 0)
		{
			if(l_mp4Mov->fpMp4)
			{
				fclose(l_mp4Mov->fpMp4);
			}
			if(l_mp4Mov->fpMp2)
			{
				fclose(l_mp4Mov->fpMp2);
			}
			mjpeg_list_free(l_mp4Mov);
			MP2_encode_close(l_mp4Mov->hEnc);
			free(l_mp4Mov);
			//LOGI("input check failed");
			return -1;
		}

		if(-1 == mp4_pcm_to_mp2_2(fpMp2, pcmBuf, bufSize, l_mp4Mov->hEnc))
		{
			if(l_mp4Mov->fpMp4)
			{
				fclose(l_mp4Mov->fpMp4);
			}
			if(l_mp4Mov->fpMp2)
			{
				fclose(l_mp4Mov->fpMp2);
			}
			mjpeg_list_free(l_mp4Mov);
			MP2_encode_close(l_mp4Mov->hEnc);
			//LOGI("mp2 encoder failed");
			return -1;
		}
		else
		{
			return 0;
		}
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
int mp4_write_end_jpeg(void *handler)
{
	unsigned char buf[MP4_BUF_SIZE];
	MP2_FRAME *audioNode = NULL;
	unsigned int size = 0;
	MP4_CONTEXT *l_mp4Mov = NULL;

	l_mp4Mov = (MP4_CONTEXT *)handler;
	if(NULL == l_mp4Mov->fpMp2 || NULL == l_mp4Mov->fpMp4 || NULL == l_mp4Mov->videoTrack)
	{
		if(l_mp4Mov->fpMp4)
		{
			fclose(l_mp4Mov->fpMp4);
		}
		if(l_mp4Mov->fpMp2)
		{
			fclose(l_mp4Mov->fpMp2);
		}
		mjpeg_list_free(l_mp4Mov);
		MP2_encode_close(l_mp4Mov->hEnc);
		free(l_mp4Mov);
		//LOGI("input check");
		return -1;
	}

	rewind(l_mp4Mov->fpMp2);

	l_mp4Mov->audioPos = mp4_ftell(l_mp4Mov->fpMp4);
	if(-1 == l_mp4Mov->audioPos)
	{
		if(l_mp4Mov->fpMp4)
		{
			fclose(l_mp4Mov->fpMp4);
		}
		if(l_mp4Mov->fpMp2)
		{
			fclose(l_mp4Mov->fpMp2);
		}
		mjpeg_list_free(l_mp4Mov);
		MP2_encode_close(l_mp4Mov->hEnc);
		free(l_mp4Mov);
		//LOGI("ftell failed");
		return -1;
	}

	if(-1 != mp4_handle_mp2_head(l_mp4Mov->fpMp2))
	{
		while((mp4_read_mp2_frame(l_mp4Mov->fpMp2, buf, MP4_BUF_SIZE, &size)) != -1)
		{
			audioNode = (MP2_FRAME *)malloc(sizeof(struct MP2_FRAME));
			if(NULL == audioNode)
			{
				if(l_mp4Mov->fpMp4)
				{
					fclose(l_mp4Mov->fpMp4);
				}
				if(l_mp4Mov->fpMp2)
				{
					fclose(l_mp4Mov->fpMp2);
				}
				mjpeg_list_free(l_mp4Mov);
				audio_list_free(l_mp4Mov);
				MP2_encode_close(l_mp4Mov->hEnc);
				free(l_mp4Mov);
				//LOGI("malloc failed");
				return -1;
			}
			audioNode->frameSize = size;
			audioNode->next = NULL;
			audio_list_insert_end(audioNode, l_mp4Mov);
			l_mp4Mov->returnError += mp4_put_buffer(l_mp4Mov->fpMp4, buf, size);
			l_mp4Mov->mdatSize += size;
		}
	}



	if(EOF == fclose(l_mp4Mov->fpMp2))
	{
		if(l_mp4Mov->fpMp4)
		{
			fclose(l_mp4Mov->fpMp4);
		}
		mjpeg_list_free(l_mp4Mov);
		audio_list_free(l_mp4Mov);
		MP2_encode_close(l_mp4Mov->hEnc);
		free(l_mp4Mov);
		//LOGI("close mp2 failed");
		return -1;
	}

	l_mp4Mov->returnError += mp4_write_trailer(l_mp4Mov->fpMp4, l_mp4Mov);
	if(l_mp4Mov->returnError != 0)
	{
		if(l_mp4Mov->fpMp4)
		{
			fclose(l_mp4Mov->fpMp4);
		}
		mjpeg_list_free(l_mp4Mov);
		audio_list_free(l_mp4Mov);
		MP2_encode_close(l_mp4Mov->hEnc);
		free(l_mp4Mov);
		//LOGI("io write error");
		return -1;
	}

	if(EOF == fclose(l_mp4Mov->fpMp4))
	{
		mjpeg_list_free(l_mp4Mov);
		audio_list_free(l_mp4Mov);
		MP2_encode_close(l_mp4Mov->hEnc);
		free(l_mp4Mov);
		//LOGI("mp4 close error");
		return -1;
	}

	mjpeg_list_free(l_mp4Mov);
	audio_list_free(l_mp4Mov);
	MP2_encode_close(l_mp4Mov->hEnc);
	free(l_mp4Mov);

	return 0;
}

