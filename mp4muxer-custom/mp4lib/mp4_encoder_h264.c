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
//#include <tchar.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include "mp4_encoder_h264.h"
#include "mp4_file.h"
#include "mp4_input_mp2.h"
#include "mp4_input_aac.h"
#include "mp4_pcm_to_mp2.h"
#include "libmp2enc.h"

//#include "debug.h"

#define MP4_LANGUAGE	(0x55C4)
#define MP4_BUF_SIZE	(65536)

#define MOV_TIME_SCALE 1000
#define VIDEO_TIME_SCALE 1000
#define AUDIO_TIME_SCALE 44100
#define AUDIO_SAMPLE_DURATION 1024

#define STANDBY_SECONDS 5

#define IS_IDR(s) ((s)->nal_unit_type == NAL_IDR_W_RADL || (s)->nal_unit_type == NAL_IDR_N_LP)
#define IS_BLA(s) ((s)->nal_unit_type == NAL_BLA_W_RADL || (s)->nal_unit_type == NAL_BLA_W_LP || \
                   (s)->nal_unit_type == NAL_BLA_N_LP)
#define IS_IRAP(s) ((s)->nal_unit_type >= 16 && (s)->nal_unit_type <= 23)

#define MAX_SPATIAL_SEGMENTATION 4096 // max. value of u(12) field
#define MAX_VPS_COUNT 16
#define MAX_SPS_COUNT 32
#define MAX_PPS_COUNT 256

enum NALUnitType {
    NAL_TRAIL_N    = 0,
    NAL_TRAIL_R    = 1,
    NAL_TSA_N      = 2,
    NAL_TSA_R      = 3,
    NAL_STSA_N     = 4,
    NAL_STSA_R     = 5,
    NAL_RADL_N     = 6,
    NAL_RADL_R     = 7,
    NAL_RASL_N     = 8,
    NAL_RASL_R     = 9,
    NAL_BLA_W_LP   = 16,
    NAL_BLA_W_RADL = 17,
    NAL_BLA_N_LP   = 18,
    NAL_IDR_W_RADL = 19,
    NAL_IDR_N_LP   = 20,
    NAL_CRA_NUT    = 21,
    NAL_VPS        = 32,
    NAL_SPS        = 33,
    NAL_PPS        = 34,
    NAL_AUD        = 35,
    NAL_EOS_NUT    = 36,
    NAL_EOB_NUT    = 37,
    NAL_FD_NUT     = 38,
    NAL_SEI_PREFIX = 39,
    NAL_SEI_SUFFIX = 40,
};

typedef enum STREAMTYPE
{
	MJPEG = 0,
	MJPEG_MIXED,
	H264,
	H264_MIXED,
	H265,
	H265_MIXED,
	DEFAULT_VIDEO_STREAM_TYPE
} STREAMTYPE;

/* nal list struct node */
typedef struct NAL_LIST
{
	int nalType;						  /* B\P:0 IDR:1 SPS:2 PPS:3 SEI:4 UnKnowed:-1 */
	int nalSize;						  /* nal size */
	int timeStamp;					  /* timeStamp(ms) */
	struct NAL_LIST *next;	  /* next node */
} NAL_LIST;

/* mp2 frame struct node */
typedef struct MP2_FRAME
{
	int frameSize;						/* mp2 frame size */
	int timeStamp;						/* timeStamp(ms) */
	struct MP2_FRAME *next;				/* next node */
} MP2_FRAME;

typedef struct
{
	unsigned int sampleCount;
	unsigned int sampleDuration; // in the scale of media's timescale
} MP4STTSENTRY;

typedef struct HVCCNALUnitArray {
    uint8_t  array_completeness;
    uint8_t  NAL_unit_type;
    uint16_t numNalus;
   // uint16_t *nalUnitLength;
   uint16_t nalUnitLength;
   // uint8_t  **nalUnit;
    uint8_t  *nalUnit;	
} HVCCNALUnitArray;


typedef struct HEVCDecoderConfigurationRecord {
    uint8_t  configurationVersion;
    uint8_t  general_profile_space;
    uint8_t  general_tier_flag;
    uint8_t  general_profile_idc;
    uint32_t general_profile_compatibility_flags;
    uint64_t general_constraint_indicator_flags;
    uint8_t  general_level_idc;
    uint16_t min_spatial_segmentation_idc;
    uint8_t  parallelismType;
    uint8_t  chromaFormat;
    uint8_t  bitDepthLumaMinus8;
    uint8_t  bitDepthChromaMinus8;
    uint16_t avgFrameRate;
    uint8_t  constantFrameRate;
    uint8_t  numTemporalLayers;
    uint8_t  temporalIdNested;
    uint8_t  lengthSizeMinusOne;
    uint8_t  numOfArrays;
    HVCCNALUnitArray array[3];
} HEVCDecoderConfigurationRecord;

/* mp4 key info struct */
typedef struct MP4_CONTEXT
{
	FILE *fpMp4;						/* mp4 file pointer */
	FILE *fpMp2;						/* mp2 file pointer */
	unsigned int time;					/* encode time flag */
	long mdatPos;						/* mdat position */
	unsigned int mdatSize;				/* mdat size */

	unsigned int tmpNalSize;			/* temp nal size*/

	int iFirstVideoTimeStamp;			/* first video timestamp*/
	int iFirstAudioTimeStamp;			/* first audio timestamp*/
	
	unsigned int ish264or5;				/* 用于标记h264/h265  tag */	
	unsigned char *vps;					/* sps data */
	unsigned int vpsLen;				/* sps length */
	unsigned char *sps;					/* sps data */
	unsigned int spsLen;				/* sps length */
	unsigned char *pps;					/* pps data */
	unsigned int ppsLen;				/* pps length */
	NAL_LIST *videoTrack;				/* nal list head */
	NAL_LIST *videoTail;				/* nal list tail */
	MP4STTSENTRY *videoSttsEntry;		/* video stts entry */
	unsigned int numVideoSttsEntry;		/* the num of video stts entry */
	unsigned int videoTrackSize;		/* jpeg num */
	unsigned int width;					/* video width */
	unsigned int height;				/* video height */

	unsigned int lastTimeStampForStandbyMoov;	/* last timestamp for save temp moov */
	unsigned int videoSizeInStandbySeconds;		/* the statistics of video size in standby seconds*/
	unsigned int maxVideoSizeInStandbySeconds;	/* max video size in standby seconds*/

	long audioPos;						/* audio data position */
	MP2_FRAME *audioTrack;				/* mp2 list head */
	MP2_FRAME *audioTail;				/* mp2 list tail */
	MP4STTSENTRY *audioSttsEntry;		/* audio stts entry */
	unsigned int numAudioSttsEntry;		/* num of audio stts entry */
	unsigned int audioTrackSize;		/* mp2 frame num */
	unsigned int audioMaxBitRate;		/* audio max bitrate */
	unsigned int audioAvgBitRate;		/* audio average bitrate */
	unsigned int audioSamplingFreqIndex;/* audio sampling frequency index */
	unsigned int audioChannelConfig;	/* audio channel configuration */
	int audioAacHeaderParsed;			/* audio AAC header parsed flag */

	unsigned int videoSampleDuration;	/* video sample time duration */
	unsigned int audioSampleDuration;	/* audio sample time duration */
	unsigned int videoDuration;			/* video time duration in videoTimeScale*/
	unsigned int videoDurationInMovTimeScale;	/* video time duration in movTimeScale*/
	unsigned int audioDuration;			/* audio time duration in audioTimeScale*/
	unsigned int audioDurationInMovTimeScale;	/* audio time duration in movTimeScale*/
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

unsigned int AUDIO_SAMPLE_RATE_TABLE[] = {
			96000, 88200, 64000,
			48000, 44100, 32000,
			24000, 22050, 16000,
			12000, 11025, 8000, 7350 };

static void nal_list_insert_end(NAL_LIST *newNode, MP4_CONTEXT *mov)
{
	if (NULL == newNode || NULL == mov)
	{
		return;
	}

	newNode->next = NULL;
	(mov->videoTrackSize)++;
	if (NULL == mov->videoTrack)
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

static void nal_list_free(MP4_CONTEXT *mov)
{
	NAL_LIST *tmpNode = NULL;

	if (NULL == mov)
	{
		return;
	}
	else
	{
		while (mov->videoTrack != NULL)
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
	if (NULL == newNode || NULL == mov)
	{
		return;
	}

	newNode->next = NULL;
	(mov->audioTrackSize)++;
	if (NULL == mov->audioTrack)
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

	if (NULL == mov)
	{
		return;
	}
	else
	{
		while (mov->audioTrack != NULL)
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

	if (NULL == fp || NULL == mov)
	{
		return -1;
	}

	curPos = mp4_ftell(fp);
	if (-1 == curPos)
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

static void hvcc_init(HEVCDecoderConfigurationRecord *hvcc)
{
    memset(hvcc, 0, sizeof(HEVCDecoderConfigurationRecord));
    hvcc->configurationVersion = 1;
    hvcc->lengthSizeMinusOne   = 3; // 4 bytes

    /*
     * The following fields have all their valid bits set by default,
     * the ProfileTierLevel parsing code will unset them when needed.
     */
    hvcc->general_profile_compatibility_flags = 0x60000000;//0xffffffff;
    hvcc->general_constraint_indicator_flags  = 0x000000000000;//0xffffffffffff;

    /*
     * Initialize this field with an invalid value which can be used to detect
     * whether we didn't see any VUI (in which case it should be reset to zero).
     */
    hvcc->min_spatial_segmentation_idc = 0x0000;//MAX_SPATIAL_SEGMENTATION + 1;

    /* from vlc stream ,set the value*/
    hvcc->general_profile_space = 0;
    hvcc->general_tier_flag = 0;
    hvcc->general_profile_idc = 0x01;
    hvcc->general_level_idc = 0x5d;
    hvcc->parallelismType = 0x0;
    hvcc->chromaFormat = 0x1;
    hvcc->bitDepthLumaMinus8 = 0x0;
    hvcc->bitDepthChromaMinus8 = 0x0;
    hvcc->avgFrameRate =0x0;
    hvcc->constantFrameRate = 0x0;
    hvcc->numTemporalLayers =0x1;
    hvcc->temporalIdNested = 0x1;
    hvcc->numOfArrays = 0x3;/*hvcc 的nalu type矩阵的数量*/

    hvcc->array[0].array_completeness = 0x0;/*vlc == 0x0*/
    hvcc->array[0].NAL_unit_type =NAL_VPS;/*vlc == 0x20 ==32*/
    hvcc->array[0].numNalus =0x0;
    hvcc->array[0].nalUnitLength = 0;//{0x0019,0x0019,0x0018};/*vlc == 0x0019=25 , 0x0019=25 ,0x0018=24 */
    hvcc->array[0].nalUnit = NULL;
	
    hvcc->array[1].array_completeness = 0x0;/*vlc == 0x0*/
    hvcc->array[1].NAL_unit_type =NAL_SPS;
    hvcc->array[1].numNalus =0x0;
    hvcc->array[1].nalUnitLength = 0;
    hvcc->array[1].nalUnit = NULL;

    hvcc->array[2].array_completeness = 0x0;/*vlc == 0x0*/
    hvcc->array[2].NAL_unit_type =NAL_PPS;
    hvcc->array[2].numNalus =0x0;
    hvcc->array[2].nalUnitLength = 0;
    hvcc->array[2].nalUnit = NULL;
	

}

static int hvcc_write(FILE *fp, HEVCDecoderConfigurationRecord *hvcc)
{
    uint8_t i;
    uint16_t j, vps_count = 0, sps_count = 0, pps_count = 0;

    /*
     * We only support writing HEVCDecoderConfigurationRecord version 1.
     */
    hvcc->configurationVersion = 1;

    /*
     * If min_spatial_segmentation_idc is invalid, reset to 0 (unspecified).
     */
    if (hvcc->min_spatial_segmentation_idc > MAX_SPATIAL_SEGMENTATION)
        hvcc->min_spatial_segmentation_idc = 0;

    /*
     * parallelismType indicates the type of parallelism that is used to meet
     * the restrictions imposed by min_spatial_segmentation_idc when the value
     * of min_spatial_segmentation_idc is greater than 0.
     */
    if (!hvcc->min_spatial_segmentation_idc)
        hvcc->parallelismType = 0;

    /*
     * It's unclear how to properly compute these fields, so
     * let's always set them to values meaning 'unspecified'.
     */
    hvcc->avgFrameRate      = 0;
    hvcc->constantFrameRate = 0;

  /*
     * We need at least one of each: VPS, SPS and PPS.
     */
    for (i = 0; i < hvcc->numOfArrays; i++)
        switch (hvcc->array[i].NAL_unit_type) {
        case NAL_VPS:
            vps_count += hvcc->array[i].numNalus;
            break;
        case NAL_SPS:
            sps_count += hvcc->array[i].numNalus;
            break;
        case NAL_PPS:
            pps_count += hvcc->array[i].numNalus;
            break;
        default:
            break;
        }
    if (!vps_count || vps_count > MAX_VPS_COUNT ||
        !sps_count || sps_count > MAX_SPS_COUNT ||
        !pps_count || pps_count > MAX_PPS_COUNT)
        return -1;

    /* unsigned int(8) configurationVersion = 1; */
    mp4_put_byte(fp, hvcc->configurationVersion);

    /*
     * unsigned int(2) general_profile_space;
     * unsigned int(1) general_tier_flag;
     * unsigned int(5) general_profile_idc;
     */
    mp4_put_byte(fp, hvcc->general_profile_space << 6 |
                hvcc->general_tier_flag     << 5 |
                hvcc->general_profile_idc);

    /* unsigned int(32) general_profile_compatibility_flags; */
    mp4_put_be32(fp, hvcc->general_profile_compatibility_flags);

    /* unsigned int(48) general_constraint_indicator_flags; */
    mp4_put_be32(fp, hvcc->general_constraint_indicator_flags >> 16);
    mp4_put_be16(fp, hvcc->general_constraint_indicator_flags);

    /* unsigned int(8) general_level_idc; */
    mp4_put_byte(fp, hvcc->general_level_idc);

    /*
     * bit(4) reserved = 鈥?111鈥檅;
     * unsigned int(12) min_spatial_segmentation_idc;
     */
    mp4_put_be16(fp, hvcc->min_spatial_segmentation_idc | 0xf000);

    /*
     * bit(6) reserved = 鈥?11111鈥檅;
     * unsigned int(2) parallelismType;
     */
    mp4_put_byte(fp, hvcc->parallelismType | 0xfc);

    /*
     * bit(6) reserved = 鈥?11111鈥檅;
     * unsigned int(2) chromaFormat;
     */
    mp4_put_byte(fp, hvcc->chromaFormat | 0xfc);

    /*
     * bit(5) reserved = 鈥?1111鈥檅;
     * unsigned int(3) bitDepthLumaMinus8;
     */
    mp4_put_byte(fp, hvcc->bitDepthLumaMinus8 | 0xf8);

    /*
     * bit(5) reserved = 鈥?1111鈥檅;
     * unsigned int(3) bitDepthChromaMinus8;
     */
    mp4_put_byte(fp, hvcc->bitDepthChromaMinus8 | 0xf8);

    /* bit(16) avgFrameRate; */
    mp4_put_be16(fp, hvcc->avgFrameRate);

    /*
     * bit(2) constantFrameRate;
     * bit(3) numTemporalLayers;
     * bit(1) temporalIdNested;
     * unsigned int(2) lengthSizeMinusOne;
     */
    mp4_put_byte(fp, hvcc->constantFrameRate << 6 |
                hvcc->numTemporalLayers << 3 |
                hvcc->temporalIdNested  << 2 |
                hvcc->lengthSizeMinusOne);

    /* unsigned int(8) numOfArrays; */
    mp4_put_byte(fp, hvcc->numOfArrays);

    for (i = 0; i < hvcc->numOfArrays; i++) {
        /*
         * bit(1) array_completeness;
         * unsigned int(1) reserved = 0;
         * unsigned int(6) NAL_unit_type;
         */
        mp4_put_byte(fp, hvcc->array[i].array_completeness << 7 |
                    hvcc->array[i].NAL_unit_type & 0x3f);

        /* unsigned int(16) numNalus; */
        mp4_put_be16(fp, hvcc->array[i].numNalus);

        for (j = 0; j < hvcc->array[i].numNalus; j++) {
            /* unsigned int(16) nalUnitLength; */
            mp4_put_be16(fp, hvcc->array[i].nalUnitLength);//array[i].nalUnitLength[j]);

            mp4_put_buffer(fp, hvcc->array[i].nalUnit,
                       hvcc->array[i].nalUnitLength);
			
        }
    }

    return 0;
}


static int hvcc_array_add_nal_unit(uint8_t *nal_buf, uint32_t nal_size,
                                   uint8_t nal_type, int ps_array_completeness,
                                   HEVCDecoderConfigurationRecord *hvcc)
{
    int ret;
    uint8_t index;
    uint16_t numNalus;
    HVCCNALUnitArray *array;

    for (index = 0; index < hvcc->numOfArrays; index++)
        if (hvcc->array[index].NAL_unit_type == nal_type)
            break;

    array    = &hvcc->array[index];
    numNalus = array->numNalus;


	/* 填充array[0]/[1]/[2]*/
   	//array->nalUnit[numNalus] = nal_buf;
   	
	hvcc->array[index].nalUnit = (uint8_t *)malloc(nal_size);

	memcpy(hvcc->array[index].nalUnit, nal_buf, nal_size);	
    array->nalUnitLength = nal_size;
    array->NAL_unit_type           = nal_type;
    array->numNalus++;

    if (nal_type == NAL_VPS || nal_type == NAL_SPS || nal_type == NAL_PPS)
        array->array_completeness = ps_array_completeness;

    return 0;
}

/*****************************************
Function:		mp4_write_hvcC
Description:	写 h265 的hvcC box ,此标记用于指明解码器
				移植于ffmpeg函数,多数参数以固定值的方式赋值。
				因此兼容性需要改进				
Input:			fp MP4目标文件  
				mov 句柄
Output:			打了tag的fp
Return:			-1 when error happens,otherwise 0
Others:			none
*****************************************/
static int mp4_write_hvcC(FILE *fp, MP4_CONTEXT *mov)
{
   	int ret = 0;
    	HEVCDecoderConfigurationRecord hvcc;

	if (NULL == fp || NULL == mov)
	{
		return -1;
	}

    	hvcc_init(&hvcc);

    ret = hvcc_array_add_nal_unit(mov->vps, mov->vpsLen, NAL_VPS ,0 , &hvcc);
	if ( ret < 0)
	{
		printf(" hvcc_array_add_nal_unit error!!!\n");
		return -1;
	}
    	ret = hvcc_array_add_nal_unit(mov->sps, mov->spsLen, NAL_SPS ,0 , &hvcc);
	if ( ret < 0)
	{
		printf(" hvcc_array_add_nal_unit error!!!\n");
		return -1;
	}
    	ret = hvcc_array_add_nal_unit(mov->pps, mov->ppsLen, NAL_PPS ,0 , &hvcc);
	if ( ret < 0)
	{
		printf(" hvcc_array_add_nal_unit error!!!\n");
		return -1;
	}


    ret = hvcc_write(fp, &hvcc);
	if ( ret < 0)
	{
		printf(" hvcc_write error!!!\n");
		return -1;
	}

	return 0;
}



static int mp4_write_avcc(FILE *fp, MP4_CONTEXT *mov)
{
	if (NULL == fp || NULL == mov)
	{
		return -1;
	}
	else
	{
		if (mov->sps != NULL && mov->pps != NULL)
		{
			mov->returnError += mp4_put_buffer(fp, (unsigned char *)"\x01\x4D\x40\x1F\xFF\xE1", 6);
			mov->returnError += mp4_put_be16(fp, mov->spsLen);
			mov->returnError += mp4_put_buffer(fp, mov->sps, mov->spsLen);
			
			mov->returnError += mp4_put_byte(fp, 0x01);
			mov->returnError += mp4_put_be16(fp, mov->ppsLen);
			mov->returnError += mp4_put_buffer(fp, mov->pps, mov->ppsLen);
			return 0;
		}
		else
		{
			return -1;
		}
	}
}

static int mp4_write_tkhd_tag(FILE *fp, MP4_CONTEXT *mov)
{
	if (NULL == fp || NULL == mov)
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

		if (0 == mov->movToMediaVideo)
		{
			return -1;
		}
		mov->returnError += mp4_put_be32(fp, mov->videoDurationInMovTimeScale);

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

		mov->returnError += mp4_put_be16(fp, mov->width);
		mov->returnError += mp4_put_be16(fp, 0);
		mov->returnError += mp4_put_be16(fp, mov->height);
		mov->returnError += mp4_put_be16(fp, 0);
		//mov->returnError += mp4_put_be32(fp, mov->width);
		//mov->returnError += mp4_put_be32(fp, mov->height);

		return 0;
	}
}

static int mp4_write_tkhd_tag_2(FILE *fp, MP4_CONTEXT *mov)
{
	if (NULL == fp || NULL == mov)
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

		if (0 == mov->movToMediaAudio)
		{
			return -1;
		}
		mov->returnError += mp4_put_be32(fp, mov->audioDurationInMovTimeScale); 

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


static int mp4_write_mdhd_tag(FILE *fp, MP4_CONTEXT *mov)
{
	if (NULL == fp || NULL == mov)
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
		mov->returnError += mp4_put_be32(fp, mov->videoDuration); /* duration in videoTimeScale*/

		mov->returnError += mp4_put_be16(fp, MP4_LANGUAGE); /* language */
		mov->returnError += mp4_put_be16(fp, 0); /* reserved (quality) */

		return 0;
	}
}

static int mp4_write_mdhd_tag_2(FILE *fp, MP4_CONTEXT *mov)
{
	if (NULL == fp || NULL == mov)
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

		mov->returnError += mp4_put_be32(fp, mov->audioTimeScale); /* time scale (SampleRate for audio) */
		mov->returnError += mp4_put_be32(fp, mov->audioDuration); /* duration in audio timescale*/

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

	if (NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if (-1 == pos)
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
	mov->returnError += mp4_put_be32(fp, 0); /* reserved */
	mov->returnError += mp4_put_be32(fp, 0); /* reserved */
	mov->returnError += mp4_put_be32(fp, 0); /* reserved */
	mov->returnError += mp4_put_buffer(fp, (unsigned char *)descr, strlen(descr));	/* handler description */
	mov->returnError += mp4_put_byte(fp, 0);						/* 输出字符结束符\0 */

	return update_size(fp, pos, mov);
}

static int mp4_write_hdlr_tag_2(FILE *fp, MP4_CONTEXT *mov)
{
	char *hdlr = NULL;
	char *descr = NULL;
	char *hdlrType = NULL;
	long pos = 0;

	if (NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if (-1 == pos)
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
	mov->returnError += mp4_put_be32(fp, 0); /* reserved */
	mov->returnError += mp4_put_be32(fp, 0); /* reserved */
	mov->returnError += mp4_put_be32(fp, 0); /* reserved */
	mov->returnError += mp4_put_buffer(fp, (unsigned char *)descr, strlen(descr));	/* handler description */
	mov->returnError += mp4_put_byte(fp, 0);						/* 输出字符结束符\0 */

	return update_size(fp, pos, mov);
}



static int mp4_write_vmhd_tag(FILE *fp, MP4_CONTEXT *mov)
{
	if (NULL == fp || NULL == mov)
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
	if (NULL == fp || NULL == mov)
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
	if (NULL == fp || NULL == mov)
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
	if (NULL == fp || NULL == mov)
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

	if (NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if (-1 == pos)
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

	if (NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if (-1 == pos)
	{
		return -1;
	}

	mov->returnError += mp4_put_be32(fp, 0); /* size */
	mov->returnError += mp4_put_tag(fp, "dinf");
	mov->returnError += mp4_write_dref_tag_2(fp, mov);

	return update_size(fp, pos, mov);
}


static int mp4_write_stts_tag(FILE *fp, MP4_CONTEXT *mov)
{
	unsigned int entries = 0;
	unsigned int atomSize = 0;
	unsigned int i = 0;

	if (NULL == fp || NULL == mov)
	{
		return -1;
	}

	entries = mov->numVideoSttsEntry;
	atomSize = 16 + (entries * 8);

	mov->returnError += mp4_put_be32(fp, atomSize);
	mov->returnError += mp4_put_tag(fp, "stts");
	mov->returnError += mp4_put_be32(fp, 0); /* version & flags */
	mov->returnError += mp4_put_be32(fp, entries); /* entry count */
	
	for (i = 0; i < entries; i++)
	{
		mov->returnError += mp4_put_be32(fp, mov->videoSttsEntry[i].sampleCount);
		mov->returnError += mp4_put_be32(fp, mov->videoSttsEntry[i].sampleDuration);
	}
	return 0;
}

/* Time to sample atom */
static int mp4_write_stts_tag_2(FILE *fp, MP4_CONTEXT *mov)
{
	unsigned int entries = 0;
	unsigned int atomSize = 0;
	unsigned int i = 0;

	if (NULL == fp || NULL == mov)
	{
		return -1;
	}

	entries = mov->numAudioSttsEntry;
	atomSize = 16 + (entries * 8);
	mov->returnError += mp4_put_be32(fp, atomSize);
	mov->returnError += mp4_put_tag(fp, "stts");
	mov->returnError += mp4_put_be32(fp, 0); /* version & flags */
	mov->returnError += mp4_put_be32(fp, entries); /* entry count */
	for (i = 0; i < entries; i++)
	{
		mov->returnError += mp4_put_be32(fp, mov->audioSttsEntry[i].sampleCount);
		mov->returnError += mp4_put_be32(fp, mov->audioSttsEntry[i].sampleDuration);
	}

	return 0;
}

static int mp4_write_stss_tag(FILE *fp, MP4_CONTEXT *mov)
{
	long pos = 0;
	long curPos = 0;
	long entryPos = 0;
	int i = 0;
	int index = 0;
	NAL_LIST *head = NULL;

	if (NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if (-1 == pos)
	{
		return -1;
	}

	head = mov->videoTrack;
	mov->returnError += mp4_put_be32(fp, 0); /* size */
	mov->returnError += mp4_put_tag(fp, "stss");
	mov->returnError += mp4_put_be32(fp, 0); /* version & flags */

	entryPos = mp4_ftell(fp);
	if (-1 == entryPos)
	{
		return -1;
	}

	mov->returnError += mp4_put_be32(fp, 1); /* entry count */
	i = 0;
	while (head != NULL)
	{
		if (1 == head->nalType) /* I frame of h264 */
		{
			mov->returnError += mp4_put_be32(fp, i + 1);
			index++;
		}
		i++;
		head = head->next;
	}

	curPos = mp4_ftell(fp);
	if (-1 == curPos)
	{
		return -1;
	}
	mov->returnError += mp4_fseek(fp, entryPos, SEEK_SET);
	mov->returnError += mp4_put_be32(fp, index); /* rewrite size */
	mov->returnError += mp4_fseek(fp, curPos, SEEK_SET);

	return update_size(fp, pos, mov);
}

static int mp4_write_stsc_tag(FILE *fp, MP4_CONTEXT *mov)
{
	int index = 0;
	long entryPos = 0;
	long curPos = 0;
	long pos = 0;

	if (NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if (-1 == pos)
	{
		return -1;
	}

	mov->returnError += mp4_put_be32(fp, 0); /* size */
	mov->returnError += mp4_put_tag(fp, "stsc");
	mov->returnError += mp4_put_be32(fp, 0); /* version & flags */

	entryPos = mp4_ftell(fp);
	if (-1 == entryPos)
	{
		return -1;
	}

	mov->returnError += mp4_put_be32(fp, mov->videoTrackSize);
	mov->returnError += mp4_put_be32(fp, 1); /* first chunk */
	mov->returnError += mp4_put_be32(fp, mov->videoTrackSize); /* samples per chunk */
	mov->returnError += mp4_put_be32(fp, 0x1); /* sample description index */

	index = 1;
	curPos = mp4_ftell(fp);
	if (-1 == curPos)
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

	if (NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if (-1 == pos)
	{
		return -1;
	}

	mov->returnError += mp4_put_be32(fp, 0); /* size */
	mov->returnError += mp4_put_tag(fp, "stsc");
	mov->returnError += mp4_put_be32(fp, 0); /* version & flags */

	entryPos = mp4_ftell(fp);
	if (-1 == entryPos)
	{
		return -1;
	}

	mov->returnError += mp4_put_be32(fp, mov->audioTrackSize);
	mov->returnError += mp4_put_be32(fp, 1); /* first chunk */
	mov->returnError += mp4_put_be32(fp, mov->audioTrackSize);/* samples per chunk */
	mov->returnError += mp4_put_be32(fp, 0x1); /* sample description index */

	index = 1;
	curPos = mp4_ftell(fp);
	if (-1 == curPos)
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
	NAL_LIST *track = NULL;
	long pos = 0;

	if (NULL == fp || NULL == mov)
	{
		return -1;
	}

	track = mov->videoTrack;
	pos = mp4_ftell(fp);
	if (-1 == pos)
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
		mov->returnError += mp4_put_be32(fp, track->nalSize);
		track = track->next;
	}

	return update_size(fp, pos, mov);
}

static int mp4_write_stsz_tag_2(FILE *fp, MP4_CONTEXT *mov)
{
	int i = 0;
	MP2_FRAME *track = NULL;
	long pos = 0;

	if (NULL == fp || NULL == mov)
	{
		return -1;
	}

	track = mov->audioTrack;
	pos = mp4_ftell(fp);
	if (-1 == pos)
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

	if (NULL == fp || NULL == mov)
	{
		return -1;
	}

	offsetBegin = mov->mdatPos + 8;

	pos = mp4_ftell(fp);
	if (-1 == pos)
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

	if (NULL == fp || NULL == mov)
	{
		return -1;
	}

	offsetBegin = mov->audioPos;

	pos = mp4_ftell(fp);
	if (-1 == pos)
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

static int mp4_write_hvcc_tag(FILE *fp, MP4_CONTEXT *mov)
{
	long pos = 0;

	if (NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if (-1 == pos)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 0);
	mov->returnError += mp4_put_tag(fp, "hvcC");
	mov->returnError += mp4_write_hvcC(fp, mov);

	return update_size(fp, pos, mov);
}


static int mp4_write_avcc_tag(FILE *fp, MP4_CONTEXT *mov)
{
	long pos = 0;

	if (NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if (-1 == pos)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 0);
	mov->returnError += mp4_put_tag(fp, "avcC");
	mov->returnError += mp4_write_avcc(fp, mov);

	return update_size(fp, pos, mov);
}

static int mp4_write_esds_tag_2(FILE *fp, MP4_CONTEXT *mov)
{
	long pos = 0;

	if (NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if (-1 == pos)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 0);
	mov->returnError += mp4_put_tag(fp, "esds");
	mov->returnError += mp4_put_be32(fp, 0);				/* version and flag */

	if (3 == mov->audioCodec) /* aac, ugly... */
	{
		mov->returnError += mp4_put_byte(fp, 0x03); // ES_DescTag
		mov->returnError += mp4_put_be32(fp, 0x80808022);// length field, length is 0x22
		mov->returnError += mp4_put_be16(fp, 0);// ES_ID
		// 1bit: streamDependenceFlag 1bit: URL_Flag
		// 1bit: OCRStreamFlag 5bits: streamPriority
		mov->returnError += mp4_put_byte(fp, 0);
		mov->returnError += mp4_put_byte(fp, 0x04);// DecoderConfigDescriptor TAG
		mov->returnError += mp4_put_be32(fp, 0x80808014);// length field, length is 0x14
		mov->returnError += mp4_put_byte(fp, 0x40);// objectTypeIndication see 14496-1 Table 8, 0x40 means Audio ISO/IEC 14496-3
		mov->returnError += mp4_put_byte(fp, 0x15);// 6bits: streamType(14496-1 Table9), 5 is Audio Stream; 1bit for upStream; 1bit reserved(=1)
		mov->returnError += mp4_put_be24(fp, 0x000200);// bufferSizeDB(decoding buffer)
		mov->returnError += mp4_put_be32(fp, mov->audioMaxBitRate);// maxBitrate
		mov->returnError += mp4_put_be32(fp, mov->audioAvgBitRate);// avgBitrate
		mov->returnError += mp4_put_byte(fp, 0x05);// DecSepcificInfoTag see 14496-3
		mov->returnError += mp4_put_be32(fp, 0x80808002);// length field, length is 0x02
		// 5bits: audioObjectType(=2 GASpecificConfig)
		// 4bits: samplingFrequencyIndex 4bits: channelConfiguration
		// 2bits: cpConfig(=0); 1bit: directMapping(=0)
		mov->returnError += mp4_put_be16(fp, (2 << 11) | (mov->audioSamplingFreqIndex << 7) | (mov->audioChannelConfig << 3));
		mov->returnError += mp4_put_byte(fp, 0x06);// SLConfigDescrTag
		mov->returnError += mp4_put_be32(fp, 0x80808001);// length field, length is 0x01
		mov->returnError += mp4_put_byte(fp, 0x02); // 0x02 reserved for use in MP4 files
	}
	else  /* mp2, ugly... */
	{
		mov->returnError += mp4_put_be16(fp, 0x0315);
		mov->returnError += mp4_put_be16(fp, 0);
		mov->returnError += mp4_put_be16(fp, 0x0004);
		mov->returnError += mp4_put_be16(fp, 0x0D6B);
		mov->returnError += mp4_put_byte(fp, 0x15);
		mov->returnError += mp4_put_be24(fp, 0x0001A2);
		mov->returnError += mp4_put_be32(fp, 0x00020A70);
		mov->returnError += mp4_put_be32(fp, 0x0001F3F8);
		mov->returnError += mp4_put_be24(fp, 0x060102);
	}

	return update_size(fp, pos, mov);
}


static int mp4_write_video_tag(FILE *fp, MP4_CONTEXT *mov)
{
	unsigned char compressorName[32];
	long pos = 0;

	if (NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if (-1 == pos)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 0); /* size */
	if (mov->ish264or5 == H265)
	{
		mov->returnError += mp4_put_tag(fp, "hvc1");
	}
	else if (mov->ish264or5 == H264)
	{
		mov->returnError += mp4_put_tag(fp, "avc1");
	}
	else
	{
		printf(" Type error , not suport!\n");
	}
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

	memset(compressorName, 0, 32);
	mov->returnError += mp4_put_byte(fp, strlen((char *)compressorName));
	mov->returnError += mp4_put_buffer(fp, compressorName, 31);
	mov->returnError += mp4_put_be16(fp, 0x18); /* Reserved */
	mov->returnError += mp4_put_be16(fp, 0xffff); /* Reserved */

	if (mov->ish264or5 == H265)
	{
		mov->returnError += mp4_write_hvcc_tag(fp, mov);
	}
	else if (mov->ish264or5 == H264)
	{
		mov->returnError += mp4_write_avcc_tag(fp, mov);
	}
	else
	{
		printf(" Type error , not suport!\n");
	}
	
	return update_size(fp, pos, mov);
}

static int mp4_write_audio_tag(FILE *fp, MP4_CONTEXT *mov)
{
	long pos = 0;

	if (NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if (-1 == pos)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 0); /* size */
	mov->returnError += mp4_put_tag(fp, "mp4a");
	mov->returnError += mp4_put_be32(fp, 0); /* Reserved */
	mov->returnError += mp4_put_be16(fp, 0); /* Reserved */
	mov->returnError += mp4_put_be16(fp, 1); /* Data-reference index */
	mov->returnError += mp4_put_be32(fp, 0); /* Reserved */
	mov->returnError += mp4_put_be32(fp, 0); /* Reserved */
	mov->returnError += mp4_put_be16(fp, mov->audioChannelConfig);
	mov->returnError += mp4_put_be16(fp, 16);/* Sample BitWidth*/
	mov->returnError += mp4_put_be16(fp, 0); /* Pre defined*/
	mov->returnError += mp4_put_be16(fp, 0); /* reserved */
	//mov->returnError += mp4_put_be32(fp, ((unsigned int)(mov->audioTimeScale)) << 16);
	mov->returnError += mp4_put_be32(fp, AUDIO_SAMPLE_RATE_TABLE[mov->audioSamplingFreqIndex] << 16); /* Sample Rate*/
	mov->returnError += mp4_write_esds_tag_2(fp, mov);

	return update_size(fp, pos, mov);
}


static int mp4_write_stsd_tag(FILE *fp, MP4_CONTEXT *mov)
{
	long pos = 0;

	if (NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if (-1 == pos)
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

	if (NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if (-1 == pos)
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

	if (NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if (-1 == pos)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 0); /* size */
	mov->returnError += mp4_put_tag(fp, "stbl");
	mov->returnError += mp4_write_stsd_tag(fp, mov);
	mov->returnError += mp4_write_stts_tag(fp, mov);
	mov->returnError += mp4_write_stss_tag(fp, mov);
	mov->returnError += mp4_write_stsc_tag(fp, mov);
	mov->returnError += mp4_write_stsz_tag(fp, mov);
	mov->returnError += mp4_write_stco_tag(fp, mov);

	return update_size(fp, pos, mov);
}

static int mp4_write_stbl_tag_2(FILE *fp, MP4_CONTEXT *mov)
{
	long pos = 0;

	if (NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if (-1 == pos)
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

	if (NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if (-1 == pos)
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

	if (NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if (-1 == pos)
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

	if (NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if (-1 == pos)
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

	if (NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if (-1 == pos)
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
	if (NULL == fp || NULL == mov)
	{
		return -1;
	}
	else
	{
		if ((mov->iFirstVideoTimeStamp <= mov->iFirstAudioTimeStamp) || (NULL == mov->audioTrack)) /* play video first, or no audio */
		{
			mov->returnError += mp4_put_be32(fp, 0x24); /* size */
			mov->returnError += mp4_put_tag(fp, "edts");
			mov->returnError += mp4_put_be32(fp, 0x1c); /* size */
			mov->returnError += mp4_put_tag(fp, "elst");
			mov->returnError += mp4_put_be32(fp, 0x0); /* version & flag */
			mov->returnError += mp4_put_be32(fp, 0x1);/* entry count*/
			mov->returnError += mp4_put_be32(fp, mov->videoDurationInMovTimeScale);
			mov->returnError += mp4_put_be32(fp, 0);
			mov->returnError += mp4_put_be32(fp, 0x00010000);
		}
		else /* play audio first */
		{
			mov->returnError += mp4_put_be32(fp, 0x30); /* size */
			mov->returnError += mp4_put_tag(fp, "edts");
			mov->returnError += mp4_put_be32(fp, 0x28); /* size */
			mov->returnError += mp4_put_tag(fp, "elst");
			mov->returnError += mp4_put_be32(fp, 0x0); /* version & flag */
			mov->returnError += mp4_put_be32(fp, 0x2);/* entry count */
			mov->returnError += mp4_put_be32(fp, mov->iFirstVideoTimeStamp - mov->iFirstAudioTimeStamp);/* play audio first, play video later at TimeGap */
			mov->returnError += mp4_put_be32(fp, 0xffffffff); /* -1 */
			mov->returnError += mp4_put_be32(fp, 0x00010000); /* mediaRate */
			mov->returnError += mp4_put_be32(fp, mov->videoDurationInMovTimeScale); /* segementDuration */
			mov->returnError += mp4_put_be32(fp, 0);/* mediaTime */
			mov->returnError += mp4_put_be32(fp, 0x00010000); /* mediaRate */
		}

		return 0;
	}
}

static int mp4_write_edts_tag_2(FILE *fp, MP4_CONTEXT *mov)
{
	if (NULL == fp || NULL == mov)
	{
		return -1;
	}
	else
	{
		if (mov->iFirstAudioTimeStamp <= mov->iFirstVideoTimeStamp ) /* play audio first */
		{
			mov->returnError += mp4_put_be32(fp, 0x24); /* size */
			mov->returnError += mp4_put_tag(fp, "edts");
			mov->returnError += mp4_put_be32(fp, 0x1c); /* size */
			mov->returnError += mp4_put_tag(fp, "elst");
			mov->returnError += mp4_put_be32(fp, 0x0); /* version & flag */
			mov->returnError += mp4_put_be32(fp, 0x1);/* entry count */
			mov->returnError += mp4_put_be32(fp, mov->audioDurationInMovTimeScale);
			mov->returnError += mp4_put_be32(fp, 0);
			mov->returnError += mp4_put_be32(fp, 0x00010000);
		}
		else /* play video first */
		{
			mov->returnError += mp4_put_be32(fp, 0x30); /* size */
			mov->returnError += mp4_put_tag(fp, "edts");
			mov->returnError += mp4_put_be32(fp, 0x28); /* size */
			mov->returnError += mp4_put_tag(fp, "elst");
			mov->returnError += mp4_put_be32(fp, 0x0); /* version & flag */
			mov->returnError += mp4_put_be32(fp, 0x2);/* entry count*/
			mov->returnError += mp4_put_be32(fp, mov->iFirstAudioTimeStamp - mov->iFirstVideoTimeStamp);/* play video first, play audio later at TimeGap */
			mov->returnError += mp4_put_be32(fp, 0xffffffff); /* -1 */
			mov->returnError += mp4_put_be32(fp, 0x00010000); /* mediaRate */
			mov->returnError += mp4_put_be32(fp, mov->audioDurationInMovTimeScale); /* segementDuration */
			mov->returnError += mp4_put_be32(fp, 0);/* mediaTime */
			mov->returnError += mp4_put_be32(fp, 0x00010000); /* mediaRate */
		}

		return 0;
	}
}

static int mp4_write_trak_tag(FILE *fp, MP4_CONTEXT *mov)
{
	long pos = 0;

	if (NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if (-1 == pos)
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

	if (NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if (-1 == pos)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 0); /* size */
	mov->returnError += mp4_put_tag(fp, "trak");
	mov->returnError += mp4_write_tkhd_tag_2(fp, mov);
	mov->returnError += mp4_write_edts_tag_2(fp, mov);
	mov->returnError += mp4_write_mdia_tag_2(fp, mov);

	return update_size(fp, pos, mov);
}


static int mp4_write_mvhd_tag(FILE *fp, MP4_CONTEXT *mov)
{
	int maxTrackID = 2;
	unsigned int maxTrackDuration = 0;

	if (NULL == fp || NULL == mov)
	{
		return -1;
	}

	if (0 == mov->movToMediaVideo || 0 == mov->movToMediaAudio)
	{
		return -1;
	}
	mov->videoDurationInMovTimeScale = (unsigned int)((unsigned long long)mov->videoDuration * mov->movTimeScale / mov->videoTimeScale);
	mov->audioDurationInMovTimeScale = (unsigned int)((unsigned long long)mov->audioDuration * mov->movTimeScale / mov->audioTimeScale);
	if (mov->videoDurationInMovTimeScale > mov->audioDurationInMovTimeScale)
	{
		maxTrackDuration = mov->videoDurationInMovTimeScale;
	}
	else
	{
		maxTrackDuration = mov->audioDurationInMovTimeScale;
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

static int mp4_write_udta_data(FILE *fp, MP4_CONTEXT *mov)
{
	long pos = 0;

	if (NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if (-1 == pos)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 0); /* size placeholder*/
	mov->returnError += mp4_put_tag(fp, "data");
	mov->returnError += mp4_put_be32(fp, 1);
	mov->returnError += mp4_put_be32(fp, 0);
	mov->returnError += mp4_put_tag(fp, "MP4 for IPC");

	return update_size(fp, pos, mov);
}

static int mp4_write_udta_too(FILE *fp, MP4_CONTEXT *mov)
{
	long pos = 0;

	if (NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if (-1 == pos)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 0); /* size placeholder*/
	mov->returnError += mp4_put_be32(fp, 0xA9746F6F); /* ?too */
	mov->returnError += mp4_write_udta_data(fp, mov);

	return update_size(fp, pos, mov);
}

static int mp4_write_udta_ilst(FILE *fp, MP4_CONTEXT *mov)
{
	long pos = 0;

	if (NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if (-1 == pos)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 0); /* size placeholder*/
	mov->returnError += mp4_put_tag(fp, "ilst");
	mov->returnError += mp4_write_udta_too(fp, mov);

	return update_size(fp, pos, mov);
}

static int mp4_write_udta_hdlr(FILE *fp, MP4_CONTEXT *mov)
{
	long pos = 0;

	if (NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if (-1 == pos)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 0); /* size placeholder*/
	mov->returnError += mp4_put_tag(fp, "hdlr");
	mov->returnError += mp4_put_be32(fp, 0);
	mov->returnError += mp4_put_be32(fp, 0);
	mov->returnError += mp4_put_tag(fp, "mdir");
	mov->returnError += mp4_put_be32(fp, 0);
	mov->returnError += mp4_put_be32(fp, 0);
	mov->returnError += mp4_put_be32(fp, 0);
	mov->returnError += mp4_put_byte(fp, 0);

	return update_size(fp, pos, mov);
}

static int mp4_write_udta_meta(FILE *fp, MP4_CONTEXT *mov)
{
	long pos = 0;

	if (NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if (-1 == pos)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 0); /* size placeholder*/
	mov->returnError += mp4_put_tag(fp, "meta");
	mov->returnError += mp4_put_be32(fp, 0);
	mov->returnError += mp4_write_udta_hdlr(fp, mov);
	mov->returnError += mp4_write_udta_ilst(fp, mov);

	return update_size(fp, pos, mov);
}

static int mp4_write_udta_tag(FILE *fp, MP4_CONTEXT *mov)
{
	long pos = 0;

	if (NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if (-1 == pos)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 0); /* size placeholder*/
	mov->returnError += mp4_put_tag(fp, "udta");
	mov->returnError += mp4_write_udta_meta(fp, mov);

	return update_size(fp, pos, mov);
}

static int mp4_write_standby_moov_tag(FILE *fp, MP4_CONTEXT *mov)
{
	long pos = 0;

	if (NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if (-1 == pos)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 0); /* size placeholder*/
	mov->returnError += mp4_put_tag(fp, "moov");
	mov->returnError += mp4_write_mvhd_tag(fp, mov);
	mov->returnError += mp4_write_trak_tag(fp, mov);
	mov->returnError += mp4_write_udta_tag(fp, mov);

	return update_size(fp, pos, mov);
}

static int mp4_write_moov_tag(FILE *fp, MP4_CONTEXT *mov)
{
	long pos = 0;

	if (NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if (-1 == pos)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 0); /* size placeholder*/
	mov->returnError += mp4_put_tag(fp, "moov");
	mov->returnError += mp4_write_mvhd_tag(fp, mov);
	mov->returnError += mp4_write_trak_tag(fp, mov);
	if (NULL != mov->audioTrack)
	{
		mov->returnError += mp4_write_trak_tag_2(fp, mov);
	}
	mov->returnError += mp4_write_udta_tag(fp, mov);

	return update_size(fp, pos, mov);
}

static int mp4_gen_video_stts_entry(MP4_CONTEXT *mov)
{
	unsigned int i = 0;
	unsigned int duration = 0;
	MP4STTSENTRY *pSttsEntry;
	NAL_LIST *tmpNode = NULL;

	mov->videoDuration = 0;
	mov->numVideoSttsEntry = 0;
	mov->videoSttsEntry = (MP4STTSENTRY *)malloc(mov->videoTrackSize * sizeof(MP4STTSENTRY));
	memset(mov->videoSttsEntry, 0, sizeof(mov->videoTrackSize * sizeof(MP4STTSENTRY)));

	if (NULL == mov || NULL == mov->videoSttsEntry || NULL == mov->videoTrack)
	{
		return -1;
	}
	
	pSttsEntry = mov->videoSttsEntry;
	tmpNode = mov->videoTrack;

	

	for (i = 0; i < mov->videoTrackSize - 1; i++)
	{
		duration = (tmpNode->next->timeStamp - mov->videoTrack->timeStamp) * mov->movToMediaVideo - mov->videoDuration;
		if (mov->numVideoSttsEntry > 0 && pSttsEntry[mov->numVideoSttsEntry - 1].sampleDuration == duration)
		{
			pSttsEntry[mov->numVideoSttsEntry - 1].sampleCount++;
		}
		else
		{
			pSttsEntry[mov->numVideoSttsEntry].sampleCount = 1;
			pSttsEntry[mov->numVideoSttsEntry].sampleDuration = duration;
			mov->numVideoSttsEntry++;
		}
		mov->videoDuration += duration;
		tmpNode = tmpNode->next;
	}
	// set the duration of last sample equal to the last but one sample's
	pSttsEntry[mov->numVideoSttsEntry - 1].sampleCount++;
	mov->videoDuration += pSttsEntry[mov->numVideoSttsEntry - 1].sampleDuration;

	return 0;
}


static int mp4_gen_audio_stts_entry(MP4_CONTEXT *mov)
{
	unsigned int i = 0;
	unsigned int duration = 0;
	MP4STTSENTRY *pSttsEntry;
	MP2_FRAME *tmpNode = NULL;

	unsigned int uiSampleSize = 0;
	unsigned int uiMaxSampleBitrate = 0;
	unsigned int uiSampleDuration = 0;
	unsigned int uiSampleBitrate = 0;
	unsigned long ulTotalSampleSize = 0;

	mov->audioDuration = 0;
	mov->numAudioSttsEntry = 0;
	mov->audioSttsEntry = (MP4STTSENTRY *)malloc(mov->audioTrackSize * sizeof(MP4STTSENTRY));
	memset(mov->audioSttsEntry, 0, sizeof(mov->audioTrackSize * sizeof(MP4STTSENTRY)));

	if (NULL == mov || NULL == mov->audioSttsEntry || NULL == mov->audioTrack)
	{
		return -1;
	}

	pSttsEntry = mov->audioSttsEntry;
	tmpNode = mov->audioTrack;
	for (i = 0; i < mov->audioTrackSize - 1; i++)
	{
		// audio sample duration in audio timescale 
		duration = (tmpNode->next->timeStamp - mov->audioTrack->timeStamp) * mov->movToMediaAudio - mov->audioDuration;

		if (mov->numAudioSttsEntry > 0 && pSttsEntry[mov->numAudioSttsEntry - 1].sampleDuration == duration)
		{
			pSttsEntry[mov->numAudioSttsEntry - 1].sampleCount++;
		}
		else
		{
			pSttsEntry[mov->numAudioSttsEntry].sampleCount = 1;
			pSttsEntry[mov->numAudioSttsEntry].sampleDuration = duration;
			mov->numAudioSttsEntry++;
		}
		mov->audioDuration += duration;
		uiSampleDuration = duration;
		uiSampleSize = tmpNode->frameSize;
		ulTotalSampleSize += uiSampleSize;
		if (duration != 0)
			uiSampleBitrate = 8 * uiSampleSize * mov->audioTimeScale / uiSampleDuration;
		if (uiSampleBitrate > uiMaxSampleBitrate)
		{
			uiMaxSampleBitrate = uiSampleBitrate;
		}
		tmpNode = tmpNode->next;
	}
	// set the duration of last sample equal to the last but one sample's
	pSttsEntry[mov->numAudioSttsEntry - 1].sampleCount++;
	mov->audioDuration += pSttsEntry[mov->numAudioSttsEntry - 1].sampleDuration;
	// calculate the last sample bitrate
	uiSampleDuration = pSttsEntry[mov->numAudioSttsEntry - 1].sampleDuration;
	uiSampleSize = tmpNode->frameSize;
	ulTotalSampleSize += uiSampleSize;
	uiSampleBitrate = 8 * uiSampleSize * mov->audioTimeScale / uiSampleDuration;
	if (uiSampleBitrate > mov->audioMaxBitRate)
	{
		mov->audioMaxBitRate = uiSampleBitrate;
	}
	mov->audioAvgBitRate =  (unsigned int)(8* (unsigned long long)(ulTotalSampleSize) * mov->audioTimeScale / mov->audioDuration);

	return 0;
}


static int mp4_write_standby_trailer(FILE *fp, MP4_CONTEXT *mov, unsigned int maxVideoSizeInStandbySeconds)
{
	long standbyMoovPos = 0;
	long curPos = 0;
	long mdatHoldSpaceSize = 0;
	if (NULL == fp || NULL == mov)
	{
		return -1;
	}
	curPos = mp4_ftell(fp);
	if (-1 == curPos)
	{
		return -1;
	}

	mdatHoldSpaceSize = maxVideoSizeInStandbySeconds * 1.5;
	standbyMoovPos = curPos + mdatHoldSpaceSize;

	if (-1 == mp4_gen_video_stts_entry(mov))
	{
		return -1;
	}

	mov->returnError += mp4_fseek(fp, standbyMoovPos, SEEK_SET);
	mov->returnError += mp4_write_standby_moov_tag(fp, mov);
	mov->returnError += mp4_fseek(fp, mov->mdatPos, SEEK_SET);
	mov->returnError += mp4_put_be32(fp, (mov->mdatSize + mdatHoldSpaceSize + 8));
	mov->returnError += mp4_fseek(fp, curPos, SEEK_SET);

	if (mov->videoSttsEntry != NULL)
	{
		free(mov->videoSttsEntry);
	}

	return 0;
}

static int mp4_write_trailer(FILE *fp, MP4_CONTEXT *mov)
{
	long moovPos = 0;
	long freePos = 0;

	if (NULL == fp || NULL == mov)
	{
		return -1;
	}

	moovPos = mp4_ftell(fp);
	if (-1 == moovPos)
	{
		return -1;
	}

	if (-1 == mp4_gen_video_stts_entry(mov))
	{
		return -1;
	}

	if (NULL != mov->audioTrack)
	{
		if (-1 == mp4_gen_audio_stts_entry(mov))
		{
			return -1;
		}
	}

	mov->returnError += mp4_fseek(fp, mov->mdatPos, SEEK_SET);
	mov->returnError += mp4_put_be32(fp, (mov->mdatSize) + 8);
	mov->returnError += mp4_fseek(fp, moovPos, SEEK_SET);
	mov->returnError += mp4_write_moov_tag(fp, mov);

	if (mov->videoSttsEntry != NULL)
	{
		free(mov->videoSttsEntry);
	}

	if (mov->audioSttsEntry != NULL)
	{
		free(mov->audioSttsEntry);
	}

	freePos = mp4_ftell(fp);
	if (-1 == freePos)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 0); // size placeholder
	mov->returnError += mp4_put_tag(fp, "free");
	mov->returnError += mp4_fseek(fp, 0, SEEK_END);

	return update_size(fp, freePos, mov);
}

static int mp4_write_ftyp_tag(FILE *fp, MP4_CONTEXT * mov)
{
	long pos = 0;

	if (NULL == fp || NULL == mov)
	{
		return -1;
	}

	pos = mp4_ftell(fp);
	if (-1 == pos)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 0); /* size */
	mov->returnError += mp4_put_tag(fp, "ftyp");
	mov->returnError += mp4_put_tag(fp, "isom");
	mov->returnError += mp4_put_be32(fp, 0x00000200);
	mov->returnError += mp4_put_tag(fp, "isom");
	mov->returnError += mp4_put_tag(fp, "iso2");
	if (mov->ish264or5 == H265)
	mov->returnError += mp4_put_tag(fp, "hvc1");
	else if (mov->ish264or5 == H264)
	mov->returnError += mp4_put_tag(fp, "avc1");
	
	mov->returnError += mp4_put_tag(fp, "mp41");/* ??mp42 */

	return update_size(fp, pos, mov);
}


static int mp4_write_mdat_tag(FILE *fp, MP4_CONTEXT *mov)
{
	if (NULL == fp || NULL == mov)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 8);
	mov->returnError += mp4_put_tag(fp, "free");
	mov->mdatPos = mp4_ftell(fp);
	if (-1 == mov->mdatPos)
	{
		return -1;
	}
	mov->returnError += mp4_put_be32(fp, 0); /* size placeholder*/
	mov->returnError += mp4_put_tag(fp, "mdat");

	return 0;
}

static int mp4_write_header(FILE *fp, MP4_CONTEXT *mov)
{
	if (NULL == fp || NULL == mov)
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
void *mp4_write_head_h264(const wchar_t *mp4FilePath, const wchar_t *tmpMp2Path, int width, int height, int audioCodec)
{
	FILE *fpMp4 = NULL;
	FILE *fpMp2 = NULL;
	MP4_CONTEXT *l_mp4Mov = NULL;
	struct tm timeValue;
	time_t t1;
	time_t t2;

	l_mp4Mov = (MP4_CONTEXT *)malloc(sizeof(struct MP4_CONTEXT));
	if (NULL == l_mp4Mov)
	{
		return NULL;
	}
	/* init mov struct */
	l_mp4Mov->fpMp4 = NULL;
	l_mp4Mov->fpMp2 = NULL;
	l_mp4Mov->mdatPos = 0;
	l_mp4Mov->mdatSize = 0;
	l_mp4Mov->tmpNalSize = 0;
	l_mp4Mov->iFirstVideoTimeStamp = -1;
	l_mp4Mov->iFirstAudioTimeStamp = -1;
	l_mp4Mov->ish264or5 = H264;
	l_mp4Mov->vps = NULL;	
	l_mp4Mov->sps = NULL;
	l_mp4Mov->pps = NULL;
	l_mp4Mov->vpsLen = 0;
	l_mp4Mov->spsLen = 0;
	l_mp4Mov->ppsLen = 0;
	l_mp4Mov->videoTrackSize = 0;
	l_mp4Mov->videoTrack = NULL;
	l_mp4Mov->videoTail = NULL;
	l_mp4Mov->videoSttsEntry = NULL;
	l_mp4Mov->audioSttsEntry = NULL;
	l_mp4Mov->numVideoSttsEntry = 0;
	l_mp4Mov->numAudioSttsEntry = 0;
	l_mp4Mov->audioMaxBitRate = 0;
	l_mp4Mov->audioAvgBitRate = 0;
	l_mp4Mov->audioSamplingFreqIndex = 0;
	l_mp4Mov->audioChannelConfig = 0;
	l_mp4Mov->audioAacHeaderParsed = 0;
	l_mp4Mov->audioPos = 0;
	l_mp4Mov->audioTrack = NULL;
	l_mp4Mov->audioTail = NULL;
	l_mp4Mov->audioTrackSize = 0;
	l_mp4Mov->time = 0xC6BFDE31;
	l_mp4Mov->movTimeScale = MOV_TIME_SCALE;
	l_mp4Mov->movToMediaVideo = VIDEO_TIME_SCALE / MOV_TIME_SCALE;
	l_mp4Mov->movToMediaAudio = AUDIO_TIME_SCALE / MOV_TIME_SCALE;
	l_mp4Mov->videoTimeScale = VIDEO_TIME_SCALE;
	l_mp4Mov->audioTimeScale = AUDIO_TIME_SCALE;
	l_mp4Mov->videoSampleDuration = 50;
	l_mp4Mov->audioSampleDuration = AUDIO_SAMPLE_DURATION;
	l_mp4Mov->videoDuration = 0;
	l_mp4Mov->videoDurationInMovTimeScale = 0;
	l_mp4Mov->audioDuration = 0;
	l_mp4Mov->audioDurationInMovTimeScale = 0;
	l_mp4Mov->audioDuration = 0; 
	l_mp4Mov->returnError = 0;
	l_mp4Mov->width = 640;
	l_mp4Mov->height = 480;
	l_mp4Mov->hEnc = NULL;
	l_mp4Mov->audioCodec = 0;
	
	l_mp4Mov->lastTimeStampForStandbyMoov = 0;
	l_mp4Mov->videoSizeInStandbySeconds = 0;
	l_mp4Mov->maxVideoSizeInStandbySeconds = 0;
	
	/* calculate time from Jan.1 1904(seconds) */
	timeValue.tm_year = 1970 - 1900;
	timeValue.tm_mon = 1 - 1;
	timeValue.tm_mday = 1;
	timeValue.tm_hour = 0;
	timeValue.tm_min = 0;
	timeValue.tm_sec = 0;
	t1 = mktime(&timeValue);

	timeValue.tm_year = 1904 - 1900;
	timeValue.tm_mon = 1 - 1;
	timeValue.tm_mday = 1;
	timeValue.tm_hour = 0;
	timeValue.tm_min = 0;
	timeValue.tm_sec = 0;
	t2 = mktime(&timeValue);

	t2 = t1 - t2;
	l_mp4Mov->time = time(NULL);
//	l_mp4Mov->time = l_mp4Mov->time + t2;
	l_mp4Mov->time = l_mp4Mov->time + 2082844800; // 2082844800 : 1904 到 1970年 经过的秒数

	/* input param check */
	if (NULL == mp4FilePath || NULL == tmpMp2Path)
	{
		free(l_mp4Mov);
		return NULL;
	}

	if (1 == audioCodec)
	{
		l_mp4Mov->audioCodec = 1;
	}
	else if (2 == audioCodec)
	{
		l_mp4Mov->audioCodec = 2;
	}
	else if (3 == audioCodec) /* aac */
	{
		l_mp4Mov->audioSampleDuration = 1024;
		l_mp4Mov->audioCodec = 3;
	}
	else
	{
		free(l_mp4Mov);
		return NULL;
	}

	/* width and height check */
	if (width <= 0 || height <= 0)
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
	if (NULL == fpMp4)
	{
		free(l_mp4Mov);
		return NULL;
	}
	l_mp4Mov->fpMp4 = fpMp4;
	fpMp2 = mp4_fopen(tmpMp2Path, L"wb+");
	if (NULL == fpMp2)
	{
		fclose(fpMp4);
		free(l_mp4Mov);
		return NULL;
	}
	l_mp4Mov->fpMp2 = fpMp2;

	l_mp4Mov->hEnc = MP2_encode_init(16000, 64000, 1);
	if (NULL == l_mp4Mov->hEnc)
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
Function:		mp4_write_head_h265
Description:	write mp4 header
Input:
				mp4FilePath:	output mp4 file
				tmpMp2Path:		temp mp2 file path
Output:			none
Return:			-1 when error happens,otherwise 0
Others:			none
*****************************************/
void *mp4_write_head_h265(const wchar_t *mp4FilePath, const wchar_t *tmpMp2Path, int width, int height, int audioCodec)
{
	FILE *fpMp4 = NULL;
	FILE *fpMp2 = NULL;
	MP4_CONTEXT *l_mp4Mov = NULL;
	struct tm timeValue;
	time_t t1;
	time_t t2;

	l_mp4Mov = (MP4_CONTEXT *)malloc(sizeof(struct MP4_CONTEXT));
	if (NULL == l_mp4Mov)
	{
		return NULL;
	}
	/* init mov struct */
	l_mp4Mov->fpMp4 = NULL;
	l_mp4Mov->fpMp2 = NULL;
	l_mp4Mov->mdatPos = 0;
	l_mp4Mov->mdatSize = 0;
	l_mp4Mov->tmpNalSize = 0;
	l_mp4Mov->iFirstVideoTimeStamp = -1;
	l_mp4Mov->iFirstAudioTimeStamp = -1;
	l_mp4Mov->ish264or5 = H265;
	l_mp4Mov->vps = NULL;
	l_mp4Mov->sps = NULL;
	l_mp4Mov->pps = NULL;
	l_mp4Mov->vpsLen = 0;
	l_mp4Mov->spsLen = 0;
	l_mp4Mov->ppsLen = 0;
	l_mp4Mov->videoTrackSize = 0;
	l_mp4Mov->videoTrack = NULL;
	l_mp4Mov->videoTail = NULL;
	l_mp4Mov->videoSttsEntry = NULL;
	l_mp4Mov->audioSttsEntry = NULL;
	l_mp4Mov->numVideoSttsEntry = 0;
	l_mp4Mov->numAudioSttsEntry = 0;
	l_mp4Mov->audioMaxBitRate = 0;
	l_mp4Mov->audioAvgBitRate = 0;
	l_mp4Mov->audioSamplingFreqIndex = 0;
	l_mp4Mov->audioChannelConfig = 0;
	l_mp4Mov->audioAacHeaderParsed = 0;
	l_mp4Mov->audioPos = 0;
	l_mp4Mov->audioTrack = NULL;
	l_mp4Mov->audioTail = NULL;
	l_mp4Mov->audioTrackSize = 0;
	l_mp4Mov->time = 0xC6BFDE31;
	l_mp4Mov->movTimeScale = MOV_TIME_SCALE;
	l_mp4Mov->movToMediaVideo = VIDEO_TIME_SCALE / MOV_TIME_SCALE;
	l_mp4Mov->movToMediaAudio = AUDIO_TIME_SCALE / MOV_TIME_SCALE;
	l_mp4Mov->videoTimeScale = VIDEO_TIME_SCALE;
	l_mp4Mov->audioTimeScale = AUDIO_TIME_SCALE;
	l_mp4Mov->videoSampleDuration = 50;
	l_mp4Mov->audioSampleDuration = AUDIO_SAMPLE_DURATION;
	l_mp4Mov->videoDuration = 0;
	l_mp4Mov->videoDurationInMovTimeScale = 0;
	l_mp4Mov->audioDuration = 0;
	l_mp4Mov->audioDurationInMovTimeScale = 0;
	l_mp4Mov->audioDuration = 0;
	l_mp4Mov->returnError = 0;
	l_mp4Mov->width = 640;
	l_mp4Mov->height = 480;
	l_mp4Mov->hEnc = NULL;
	l_mp4Mov->audioCodec = 0;

	l_mp4Mov->lastTimeStampForStandbyMoov = 0;
	l_mp4Mov->videoSizeInStandbySeconds = 0;
	l_mp4Mov->maxVideoSizeInStandbySeconds = 0;
	
	/* calculate time from Jan.1 1904(seconds) */
	timeValue.tm_year = 1970 - 1900;
	timeValue.tm_mon = 1 - 1;
	timeValue.tm_mday = 1;
	timeValue.tm_hour = 0;
	timeValue.tm_min = 0;
	timeValue.tm_sec = 0;
	t1 = mktime(&timeValue);

	timeValue.tm_year = 1904 - 1900;
	timeValue.tm_mon = 1 - 1;
	timeValue.tm_mday = 1;
	timeValue.tm_hour = 0;
	timeValue.tm_min = 0;
	timeValue.tm_sec = 0;
	t2 = mktime(&timeValue);

	t2 = t1 - t2;
	l_mp4Mov->time = time(NULL);
	l_mp4Mov->time = l_mp4Mov->time + t2;

	/* input param check */
	if (NULL == mp4FilePath || NULL == tmpMp2Path)
	{
		free(l_mp4Mov);
		return NULL;
	}

	if (1 == audioCodec)
	{
		l_mp4Mov->audioCodec = 1;
	}
	else if (2 == audioCodec)
	{
		l_mp4Mov->audioCodec = 2;
	}
	else if (3 == audioCodec) /* aac */
	{
		l_mp4Mov->audioSampleDuration = 1024;
		l_mp4Mov->audioCodec = 3;
	}
	else
	{
		free(l_mp4Mov);
		return NULL;
	}

	/* width and height check */
	if (width <= 0 || height <= 0)
	{
		free(l_mp4Mov);
		return NULL;
	}
	else
	{
		l_mp4Mov->width = width;
		l_mp4Mov->height = height;
		printf(" @@@l_mp4Mov =width=%d, height=%d \n",width,height);
	}

	/* open mp4 and tmp mp2 file to write */
	fpMp4 = mp4_fopen(mp4FilePath, L"wb");
	if (NULL == fpMp4)
	{
		free(l_mp4Mov);
		return NULL;
	}
	l_mp4Mov->fpMp4 = fpMp4;
	fpMp2 = mp4_fopen(tmpMp2Path, L"wb+");
	if (NULL == fpMp2)
	{
		fclose(fpMp4);
		free(l_mp4Mov);
		return NULL;
	}
	l_mp4Mov->fpMp2 = fpMp2;

	l_mp4Mov->hEnc = MP2_encode_init(16000, 64000, 1);
	if (NULL == l_mp4Mov->hEnc)
	{
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
Function:		mp4_write_one_h264
Description:	write one h264 to mp4
Input:			
				jpegBuf:	buffer 
				bufSize:	size 
				timeStamp:	time stamp (unit: ms)
Output:			none
Return:			-1 when error happens,otherwise 0
Others:			none
*****************************************/
int mp4_write_one_h264(unsigned char *h264Buf, int bufSize, int timeStamp, void *handler)
{
	FILE *fpMp4 = NULL;
	NAL_LIST *videoNode = NULL;
	int nalHeaderSize = 0;
	int nalType = -1;
	MP4_CONTEXT *l_mp4Mov = NULL;

	l_mp4Mov = (MP4_CONTEXT *)handler;
	if (NULL == l_mp4Mov || NULL == h264Buf || NULL == l_mp4Mov->fpMp4 || bufSize < 4 || timeStamp < 0)
	{
		return -1;
	}
	fpMp4 = l_mp4Mov->fpMp4;

	if (0x00 == h264Buf[0] && 0x00 == h264Buf[1])
	{
		if (0x00 == h264Buf[2] && 0x01 == h264Buf[3] && bufSize > 4)
		{
			nalHeaderSize = 4;
		}
		else if (0x01 == h264Buf[2])
		{
			nalHeaderSize = 3;
		}
		else
		{
			return -1;
		}
	}
	else
	{
		return -1;
	}

	switch (h264Buf[nalHeaderSize] & 0x1F)
	{
	case 7:							//SPS信息
		nalType = 2;
		if (0 == l_mp4Mov->spsLen)	//记录一次SPS
		{
			l_mp4Mov->spsLen = bufSize - nalHeaderSize;
			l_mp4Mov->sps = (unsigned char *)malloc(l_mp4Mov->spsLen);
			if (NULL == l_mp4Mov->sps)
			{
				l_mp4Mov->spsLen = 0;
				return -1;
			}
			else
			{
				memcpy(l_mp4Mov->sps, h264Buf + nalHeaderSize, l_mp4Mov->spsLen);
			}
		} 
		break;
	case 8:							//PPS信息
		nalType = 3;
		if (0 == l_mp4Mov->ppsLen)	//记录一次PPS
		{
			l_mp4Mov->ppsLen = bufSize - nalHeaderSize;
			l_mp4Mov->pps = (unsigned char *)malloc(l_mp4Mov->ppsLen);
			if (NULL == l_mp4Mov->pps)
			{
				l_mp4Mov->ppsLen = 0;
				return -1;
			}
			else
			{
				memcpy(l_mp4Mov->pps, h264Buf + nalHeaderSize, l_mp4Mov->ppsLen);
			}
		} 
		break;
	case 5:							//IDR帧信息
		nalType = 1;
		break;
	case 6:							//SEI信息
		nalType = 4;
		break;
	case 1:							//B,P帧默认为0
		nalType = 0;
		break;
	default:						//未识别帧
		nalType = -1;
		break;
	}

	//if (1 == nalType || 0 == nalType || 2 == nalType || 3 == nalType) 
	if (1 == nalType || 0 == nalType)    //只插入I,P,B帧
	{/* write data to mp4 */
		videoNode = (NAL_LIST *)malloc(sizeof(struct NAL_LIST));
		if (NULL == videoNode)
		{
			return -1;
		}

		if (4 == nalHeaderSize)
		{
			videoNode->nalSize = bufSize;
		}
		else
		{
			videoNode->nalSize = bufSize + 1;
		}
		videoNode->nalType = nalType;
		videoNode->timeStamp = timeStamp;
		videoNode->next = NULL;
		nal_list_insert_end(videoNode, l_mp4Mov);
		if (l_mp4Mov->iFirstVideoTimeStamp < 0)
		{
			l_mp4Mov->iFirstVideoTimeStamp = timeStamp;
			//printf("l_mp4Mov->iFirstVideoTimeStamp = %d\n", l_mp4Mov->iFirstVideoTimeStamp);
		}
		l_mp4Mov->returnError += mp4_put_be32(l_mp4Mov->fpMp4, videoNode->nalSize - 4);
		l_mp4Mov->returnError += mp4_put_buffer(l_mp4Mov->fpMp4, &(h264Buf[nalHeaderSize]), videoNode->nalSize - 4);		//把数据输入mp4文件
		l_mp4Mov->mdatSize += videoNode->nalSize;
		l_mp4Mov->videoSizeInStandbySeconds += videoNode->nalSize;
	}

	if (l_mp4Mov->lastTimeStampForStandbyMoov == 0)
	{
		l_mp4Mov->lastTimeStampForStandbyMoov = timeStamp;
	}
	if (timeStamp > (l_mp4Mov->lastTimeStampForStandbyMoov + 1000 * STANDBY_SECONDS))
	{
		if (l_mp4Mov->videoSizeInStandbySeconds > l_mp4Mov->maxVideoSizeInStandbySeconds)
		{
			l_mp4Mov->maxVideoSizeInStandbySeconds = l_mp4Mov->videoSizeInStandbySeconds;
		}
		l_mp4Mov->lastTimeStampForStandbyMoov = timeStamp;
		l_mp4Mov->returnError += mp4_write_standby_trailer(l_mp4Mov->fpMp4, l_mp4Mov, l_mp4Mov->maxVideoSizeInStandbySeconds);
		l_mp4Mov->videoSizeInStandbySeconds = 0;
	}

	return 0;
}

/*****************************************
Function:		mp4_write_one_h265
Description:	write one h265 to mp4
Input:			
				jpegBuf:	buffer 
				bufSize:	size 
				timeStamp:	time stamp (unit: ms)
Output:			none
Return:			-1 when error happens,otherwise 0
Others:			none
*****************************************/
int mp4_write_one_h265(unsigned char *h265Buf, int bufSize, int timeStamp, void *handler)
{
	FILE *fpMp4 = NULL;
	NAL_LIST *videoNode = NULL;
	int nalHeaderSize = 0;
	int nalType = -1;
	MP4_CONTEXT *l_mp4Mov = NULL;
	int extLen = 0;

	l_mp4Mov = (MP4_CONTEXT *)handler;
	if (NULL == l_mp4Mov || NULL == h265Buf || NULL == l_mp4Mov->fpMp4 || bufSize < 4 || timeStamp < 0)
	{
		return -1;
	}
	fpMp4 = l_mp4Mov->fpMp4;

	if (0x00 == h265Buf[0] && 0x00 == h265Buf[1])
	{
		if (0x00 == h265Buf[2] && 0x01 == h265Buf[3] && bufSize > 4)
		{
			nalHeaderSize = 4;
		}
		else if (0x01 == h265Buf[2])
		{
			nalHeaderSize = 3;
		}
		else
		{
			return -1;
		}
	}
	else
	{
		return -1;
	}

	switch ((h265Buf[nalHeaderSize] & 0x7E)>>1)
	{
	case NAL_VPS:							//VPS信息
		nalType = 4;
		if (0 == l_mp4Mov->vpsLen)	//记录一次SPS
		{
			l_mp4Mov->vpsLen = bufSize - nalHeaderSize;
			l_mp4Mov->vps = (unsigned char *)malloc(l_mp4Mov->vpsLen);
			if (NULL == l_mp4Mov->vps)
			{
				l_mp4Mov->vpsLen = 0;
				return -1;
			}
			else
			{
				memcpy(l_mp4Mov->vps, h265Buf + nalHeaderSize, l_mp4Mov->vpsLen);
			}
		} 
		else 
		{
			//长度增加时，重新分配内存
			if (l_mp4Mov->vpsLen < bufSize - nalHeaderSize)
			{
				free(l_mp4Mov->vps);
				l_mp4Mov->vps = NULL;

				l_mp4Mov->vps = (unsigned char *)malloc(bufSize - nalHeaderSize);
				if (NULL == l_mp4Mov->vps)
				{
					l_mp4Mov->vpsLen = 0;
					return -1;
				}
			}

			l_mp4Mov->vpsLen = bufSize - nalHeaderSize;
			if (NULL == l_mp4Mov->vps)
			{
				l_mp4Mov->vpsLen = 0;
				return -1;
			}
			else
			{
				memcpy(l_mp4Mov->vps, h265Buf + nalHeaderSize, l_mp4Mov->vpsLen);
			}
		}

		break;
	
	case NAL_SPS:							//SPS信息
		nalType = 2;
		if (0 == l_mp4Mov->spsLen)	//记录一次SPS
		{
			l_mp4Mov->spsLen = bufSize - nalHeaderSize;
			l_mp4Mov->sps = (unsigned char *)malloc(l_mp4Mov->spsLen);
			if (NULL == l_mp4Mov->sps)
			{
				l_mp4Mov->spsLen = 0;
				return -1;
			}
			else
			{
				memcpy(l_mp4Mov->sps, h265Buf + nalHeaderSize, l_mp4Mov->spsLen);
			}
		} 
		else
		{
			//长度增加时，重新分配内存
			if (l_mp4Mov->spsLen < bufSize - nalHeaderSize)
			{
				free(l_mp4Mov->sps);
				l_mp4Mov->sps = NULL;

				l_mp4Mov->sps = (unsigned char *)malloc(bufSize - nalHeaderSize);
				if (NULL == l_mp4Mov->sps)
				{
					l_mp4Mov->spsLen = 0;
					return -1;
				}
			}

			l_mp4Mov->spsLen = bufSize - nalHeaderSize;
			if (NULL == l_mp4Mov->sps)
			{
				l_mp4Mov->spsLen = 0;
				return -1;
			}
			else
			{
				memcpy(l_mp4Mov->sps, h265Buf + nalHeaderSize, l_mp4Mov->spsLen);
			}
		}

		break;
	case NAL_PPS:							//PPS信息
		nalType = 3;
		if (0 == l_mp4Mov->ppsLen)	//记录一次PPS
		{
			l_mp4Mov->ppsLen = bufSize - nalHeaderSize;
			l_mp4Mov->pps = (unsigned char *)malloc(l_mp4Mov->ppsLen);
			if (NULL == l_mp4Mov->pps)
			{
				l_mp4Mov->ppsLen = 0;
				return -1;
			}
			else
			{
				memcpy(l_mp4Mov->pps, h265Buf + nalHeaderSize, l_mp4Mov->ppsLen);
			}
		} 
		else
		{
			//长度增加时，重新分配内存
			if (l_mp4Mov->ppsLen < bufSize - nalHeaderSize)
			{
				free(l_mp4Mov->pps);
				l_mp4Mov->pps = NULL;

				l_mp4Mov->pps = (unsigned char *)malloc(bufSize - nalHeaderSize);
				if (NULL == l_mp4Mov->pps)
				{
					l_mp4Mov->ppsLen = 0;
					return -1;
				}
			}

			l_mp4Mov->ppsLen = bufSize - nalHeaderSize;
			if (NULL == l_mp4Mov->pps)
			{
				l_mp4Mov->ppsLen = 0;
				return -1;
			}
			else
			{
				memcpy(l_mp4Mov->pps, h265Buf + nalHeaderSize, l_mp4Mov->ppsLen);
			}
		}

		break;
	case NAL_IDR_W_RADL:							//IDR帧信息
	case NAL_IDR_N_LP:							
		nalType = 1;
		break;
	case NAL_SEI_PREFIX:							//SEI信息
	case NAL_SEI_SUFFIX:	
		nalType = 5;
		break;
	case NAL_TRAIL_N:
	case NAL_TRAIL_R:
	case NAL_BLA_W_RADL:							
	case NAL_BLA_W_LP:						
	case NAL_BLA_N_LP:	
		nalType = 0;
		break;
	default:						//未识别帧
		nalType = -1;
		break;
	}

	//if (0 == nalType || 1 == nalType || 2 == nalType ||3 == nalType ||4 == nalType||5 == nalType)
	if (0 == nalType || 1 == nalType)
	{/* write data to mp4 */
		videoNode = (NAL_LIST *)malloc(sizeof(struct NAL_LIST));
		if (NULL == videoNode)
		{
			return -1;
		}

		if (4 == nalHeaderSize)
		{
			videoNode->nalSize = bufSize;
		}
		else
		{
			videoNode->nalSize = bufSize + 1;
		}
		videoNode->nalType = nalType;
		videoNode->timeStamp = timeStamp;
		videoNode->next = NULL;

		if (1 == nalType)
		{
			l_mp4Mov->returnError += mp4_put_be32(l_mp4Mov->fpMp4, l_mp4Mov->vpsLen);
			l_mp4Mov->returnError += mp4_put_buffer(l_mp4Mov->fpMp4, l_mp4Mov->vps, l_mp4Mov->vpsLen);
			l_mp4Mov->returnError += mp4_put_be32(l_mp4Mov->fpMp4, l_mp4Mov->spsLen);
			l_mp4Mov->returnError += mp4_put_buffer(l_mp4Mov->fpMp4, l_mp4Mov->sps, l_mp4Mov->spsLen);
			l_mp4Mov->returnError += mp4_put_be32(l_mp4Mov->fpMp4, l_mp4Mov->ppsLen);
			l_mp4Mov->returnError += mp4_put_buffer(l_mp4Mov->fpMp4, l_mp4Mov->pps, l_mp4Mov->ppsLen);

			extLen = l_mp4Mov->vpsLen + l_mp4Mov->spsLen + l_mp4Mov->ppsLen + 12;

			videoNode->nalSize += extLen;
			//l_mp4Mov->mdatSize += extLen;
			//l_mp4Mov->videoSizeInStandbySeconds += extLen;
		}

		nal_list_insert_end(videoNode, l_mp4Mov);
		if (l_mp4Mov->iFirstVideoTimeStamp < 0)
		{
			l_mp4Mov->iFirstVideoTimeStamp = timeStamp;
			//printf("------l_mp4Mov->iFirstVideoTimeStamp = %d-----\n", l_mp4Mov->iFirstVideoTimeStamp);
		}

		l_mp4Mov->returnError += mp4_put_be32(l_mp4Mov->fpMp4, videoNode->nalSize - extLen - 4);
		l_mp4Mov->returnError += mp4_put_buffer(l_mp4Mov->fpMp4, &(h265Buf[nalHeaderSize]), videoNode->nalSize - extLen - 4);		//把数据输入mp4文件
		
		l_mp4Mov->mdatSize += videoNode->nalSize;
		l_mp4Mov->videoSizeInStandbySeconds += videoNode->nalSize;
	}

	if (l_mp4Mov->lastTimeStampForStandbyMoov == 0)
	{
		l_mp4Mov->lastTimeStampForStandbyMoov = timeStamp;
	}
	if (timeStamp > (l_mp4Mov->lastTimeStampForStandbyMoov + 1000 * STANDBY_SECONDS))
	{
		if (l_mp4Mov->videoSizeInStandbySeconds > l_mp4Mov->maxVideoSizeInStandbySeconds)
		{
			l_mp4Mov->maxVideoSizeInStandbySeconds = l_mp4Mov->videoSizeInStandbySeconds;
		}
		l_mp4Mov->lastTimeStampForStandbyMoov = timeStamp;
		l_mp4Mov->returnError += mp4_write_standby_trailer(l_mp4Mov->fpMp4, l_mp4Mov, l_mp4Mov->maxVideoSizeInStandbySeconds);
		l_mp4Mov->videoSizeInStandbySeconds = 0;
	}

	return 0;
}

/*****************************************
Function:		mp4_write_seach_sps_pps
Description:	seach_sps_pps
*****************************************/
int mp4_write_seach_sps_pps(unsigned char *h264Buf, int bufSize, int timeStamp, void *handler)
{
	FILE *fpMp4 = NULL;
	NAL_LIST *videoNode = NULL;
	int nalHeaderSize = 0;
	int nalType = -1;
	MP4_CONTEXT *l_mp4Mov = NULL;

	l_mp4Mov = (MP4_CONTEXT *)handler;
	if (NULL == l_mp4Mov || NULL == h264Buf || NULL == l_mp4Mov->fpMp4 || bufSize < 4 || timeStamp < 0)
	{
		return -1;
	}
	fpMp4 = l_mp4Mov->fpMp4;

	if (0x00 == h264Buf[0] && 0x00 == h264Buf[1])
	{
		if (0x00 == h264Buf[2] && 0x01 == h264Buf[3] && bufSize > 4)
		{
			nalHeaderSize = 4;
		}
		else if (0x01 == h264Buf[2])
		{
			nalHeaderSize = 3;
		}
		else
		{
			return -1;
		}
	}
	else
	{
		return -1;
	}

	switch (h264Buf[nalHeaderSize] & 0x1F)
	{
	case 7:							//SPS信息
		nalType = 2;
		if (0 == l_mp4Mov->spsLen)	//记录一次SPS
		{
			l_mp4Mov->spsLen = bufSize - nalHeaderSize;
			l_mp4Mov->sps = (unsigned char *)malloc(l_mp4Mov->spsLen);
			if (NULL == l_mp4Mov->sps)
			{
				l_mp4Mov->spsLen = 0;
				return -1;
			}
			else
			{
				memcpy(l_mp4Mov->sps, h264Buf + nalHeaderSize, l_mp4Mov->spsLen);
			}
		}
		break;
	case 8:							//PPS信息
		nalType = 3;
		if (0 == l_mp4Mov->ppsLen)	//记录一次PPS
		{
			l_mp4Mov->ppsLen = bufSize - nalHeaderSize;
			l_mp4Mov->pps = (unsigned char *)malloc(l_mp4Mov->ppsLen);
			if (NULL == l_mp4Mov->pps)
			{
				l_mp4Mov->ppsLen = 0;
				return -1;
			}
			else
			{
				memcpy(l_mp4Mov->pps, h264Buf + nalHeaderSize, l_mp4Mov->ppsLen);
			}
		}
		break;
	case 5:							//IDR帧信息
		nalType = 1;
		break;
	case 6:							//SEI信息
		nalType = 4;
		break;
	case 1:							//B,P帧默认为0
		nalType = 0;
		break;
	default:						//未识别帧
		nalType = -1;
		break;
	}

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
int mp4_write_pcm_h264(unsigned char *pcmBuf, int bufSize, int audioTimeStamp, void *handler)
{
	FILE *fpMp2 = NULL;
	MP2_FRAME *audioNode = NULL;
	MP4_CONTEXT *l_mp4Mov = NULL;

	l_mp4Mov = (MP4_CONTEXT *)handler;
	fpMp2 = l_mp4Mov->fpMp2;
	
	if (l_mp4Mov->iFirstAudioTimeStamp < 0)
	{
		l_mp4Mov->iFirstAudioTimeStamp = audioTimeStamp;
		//printf("l_mp4Mov->iFirstAudioTimeStamp = %d\n", l_mp4Mov->iFirstAudioTimeStamp);
	}


	if (1 == l_mp4Mov->audioCodec)
	{
		if (NULL == pcmBuf || NULL == fpMp2 || bufSize <= 0 || bufSize % 576 != 0)
		{
			return -1;
		}

		if (-1 == mp4_pcm_to_mp2(fpMp2, pcmBuf, bufSize, l_mp4Mov->hEnc))
		{
			return -1;
		}
		else
		{
			return 0;
		}
	}
	else if (2 == l_mp4Mov->audioCodec)
	{
		if (NULL == pcmBuf || NULL == fpMp2 || bufSize <= 0 || bufSize % 2304 != 0)
		{
			return -1;
		}

		if (-1 == mp4_pcm_to_mp2_2(fpMp2, pcmBuf, bufSize, l_mp4Mov->hEnc))
		{
			return -1;
		}
		else
		{
			return 0;
		}
	}
	else /* aac */
	{
		if (NULL == pcmBuf || NULL == fpMp2 || bufSize <= 0 || bufSize > 1024 || pcmBuf[0] != 0xFF || (pcmBuf[1] & 0xF0) != 0xF0) /* in this case, pcmBuf store one aac frame */
		{
			return -1;
		}
		printf("mp4_write_pcm_h264: AAC process\n");
		audioNode = (MP2_FRAME *)malloc(sizeof(struct MP2_FRAME));
		audioNode->frameSize = bufSize - 7; /* skip 7 bytes aac frame header(if CRC existed in aac frame header, skip 9 bytes) */
		audioNode->timeStamp = audioTimeStamp;
		audioNode->next = NULL;
		audio_list_insert_end(audioNode, l_mp4Mov);

		if (fwrite(pcmBuf, 1, bufSize, fpMp2) != bufSize)
		{
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
int mp4_write_end_h264(void *handler)
{
	unsigned char buf[MP4_BUF_SIZE];
	MP2_FRAME *audioNode = NULL;
	unsigned int size = 0;
	MP4_CONTEXT *l_mp4Mov = NULL;

	l_mp4Mov = (MP4_CONTEXT *)handler;

	if (NULL == l_mp4Mov || NULL == l_mp4Mov->fpMp2 || NULL == l_mp4Mov->fpMp4 || NULL == l_mp4Mov->videoTrack)
	{
		if (l_mp4Mov->fpMp4)
		{
			fclose(l_mp4Mov->fpMp4);
		}
		if (l_mp4Mov->fpMp2)
		{
			fclose(l_mp4Mov->fpMp2);
		}
		if (l_mp4Mov->vps != NULL)
		{
			free(l_mp4Mov->vps);
		}
		if (l_mp4Mov->sps != NULL)
		{
			free(l_mp4Mov->sps);
		}
		if (l_mp4Mov->pps != NULL)
		{
			free(l_mp4Mov->pps);
		}
		nal_list_free(l_mp4Mov);
		MP2_encode_close(l_mp4Mov->hEnc);
		free(l_mp4Mov);
		//LOGI("input check");
		return -1;
	}

	rewind(l_mp4Mov->fpMp2);

	l_mp4Mov->audioPos = mp4_ftell(l_mp4Mov->fpMp4);
	if (-1 == l_mp4Mov->audioPos)
	{
		if (l_mp4Mov->fpMp4)
		{
			fclose(l_mp4Mov->fpMp4);
		}
		if (l_mp4Mov->fpMp2)
		{
			fclose(l_mp4Mov->fpMp2);
		}
		if (l_mp4Mov->vps != NULL)
		{
			free(l_mp4Mov->vps);
		}	
		if (l_mp4Mov->sps != NULL)
		{
			free(l_mp4Mov->sps);
		}
		if (l_mp4Mov->pps != NULL)
		{
			free(l_mp4Mov->pps);
		}
		nal_list_free(l_mp4Mov);
		MP2_encode_close(l_mp4Mov->hEnc);
		free(l_mp4Mov);
		//LOGI("ftell failed");
		return -1;
	}

	if (3 == l_mp4Mov->audioCodec) /* aac */
	{
		//printf("mp4_write_end_h264: AAC\n");
		while ((mp4_read_aac_frame(l_mp4Mov->fpMp2, buf, MP4_BUF_SIZE, &size)) != -1)
		{
			if (size > 7)
			{
				l_mp4Mov->returnError += mp4_put_buffer(l_mp4Mov->fpMp4, buf + 7, size - 7);
				l_mp4Mov->mdatSize += size - 7;
			}
			if (l_mp4Mov->audioAacHeaderParsed == 0)
			{
				l_mp4Mov->audioSamplingFreqIndex = (buf[2] & 0x3C) >> 2; // 4bits
				l_mp4Mov->audioTimeScale = AUDIO_SAMPLE_RATE_TABLE[l_mp4Mov->audioSamplingFreqIndex];
				l_mp4Mov->movToMediaAudio = l_mp4Mov->audioTimeScale / l_mp4Mov->movTimeScale;
				l_mp4Mov->audioChannelConfig = (buf[2] & 0x1) + ((buf[3] & 0xC0) >> 6);    // 3bits
				l_mp4Mov->audioAacHeaderParsed = 1;
			}
		}
	}
	else /* mp2 */
	{
		if (-1 != mp4_handle_mp2_head(l_mp4Mov->fpMp2))
		{
			while ((mp4_read_mp2_frame(l_mp4Mov->fpMp2, buf, MP4_BUF_SIZE, &size)) != -1)
			{
				audioNode = (MP2_FRAME *)malloc(sizeof(struct MP2_FRAME));
				if (NULL == audioNode)
				{
					if (l_mp4Mov->fpMp4)
					{
						fclose(l_mp4Mov->fpMp4);
					}
					if (l_mp4Mov->fpMp2)
					{
						fclose(l_mp4Mov->fpMp2);
					}
					if (l_mp4Mov->vps != NULL)
					{
						free(l_mp4Mov->vps);
					}	
					if (l_mp4Mov->sps != NULL)
					{
						free(l_mp4Mov->sps);
					}
					if (l_mp4Mov->pps != NULL)
					{
						free(l_mp4Mov->pps);
					}
					nal_list_free(l_mp4Mov);
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
	}

	if (EOF == fclose(l_mp4Mov->fpMp2))
	{
		if (l_mp4Mov->fpMp4)
		{
			fclose(l_mp4Mov->fpMp4);
		}
		if (l_mp4Mov->vps != NULL)
		{
			free(l_mp4Mov->vps);
		}	
		if (l_mp4Mov->sps != NULL)
		{
			free(l_mp4Mov->sps);
		}
		if (l_mp4Mov->pps != NULL)
		{
			free(l_mp4Mov->pps);
		}
		nal_list_free(l_mp4Mov);
		audio_list_free(l_mp4Mov);
		MP2_encode_close(l_mp4Mov->hEnc);
		free(l_mp4Mov);
		//LOGI("close mp2 failed");
		return -1;
	}

	l_mp4Mov->returnError += mp4_write_trailer(l_mp4Mov->fpMp4, l_mp4Mov);
	if (l_mp4Mov->returnError != 0)
	{
		if (l_mp4Mov->fpMp4)
		{
			fclose(l_mp4Mov->fpMp4);
		}
		if (l_mp4Mov->vps != NULL)
		{
			free(l_mp4Mov->vps);
		}
		if (l_mp4Mov->sps != NULL)
		{
			free(l_mp4Mov->sps);
		}
		if (l_mp4Mov->pps != NULL)
		{
			free(l_mp4Mov->pps);
		}
		nal_list_free(l_mp4Mov);
		audio_list_free(l_mp4Mov);
		MP2_encode_close(l_mp4Mov->hEnc);
		free(l_mp4Mov);
		//LOGI("io write error");
		return -1;
	}


	if (EOF == fclose(l_mp4Mov->fpMp4))
	{
		if (l_mp4Mov->vps != NULL)
		{
			free(l_mp4Mov->vps);
		}	
		if (l_mp4Mov->sps != NULL)
		{
			free(l_mp4Mov->sps);
		}
		if (l_mp4Mov->pps != NULL)
		{
			free(l_mp4Mov->pps);
		}
		nal_list_free(l_mp4Mov);
		audio_list_free(l_mp4Mov);
		MP2_encode_close(l_mp4Mov->hEnc);
		free(l_mp4Mov);
		//LOGI("mp4 close error");
		return -1;
	}
	if (l_mp4Mov->vps != NULL)
	{
		free(l_mp4Mov->vps);
	}
	if (l_mp4Mov->sps != NULL)
	{
		free(l_mp4Mov->sps);
	}
	if (l_mp4Mov->pps != NULL)
	{
		free(l_mp4Mov->pps);
	}
	nal_list_free(l_mp4Mov);
	audio_list_free(l_mp4Mov);
	MP2_encode_close(l_mp4Mov->hEnc);
	free(l_mp4Mov);

	return 0;
}


/*****************************************
Function:		mp4_check_h264_data
Description:	check the mp4 file if have the h264 data
Input:			none
Output:			none
Return:			1 when no data,otherwise 0
Others:			none
*****************************************/
int mp4_check_h264_data(void *handler)
{
	unsigned int spsLen;
	unsigned int ppsLen;

	spsLen = ((MP4_CONTEXT *)handler)->spsLen;
	ppsLen = ((MP4_CONTEXT *)handler)->ppsLen;

	if (spsLen == 0 || ppsLen == 0)
	{
		return 0;
	}
	else
	{
		return 1;
	}
}