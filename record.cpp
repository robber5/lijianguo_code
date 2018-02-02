/*********************************************************************************
  *Copyright(C),Zhejiang Detu Internet CO.Ltd
  *FileName:  record.c
  *Author:  Li Jianguo
  *Version:  1.0
  *Date:  2017.11.15
  *Description:  the record module

**********************************************************************************/

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <semaphore.h>

#include "video_encoder.h"

#include "mp4_encoder.h"
#include "record.h"
#include "storage.h"

#include "config_manager.h"
#include "media.h"

using namespace std;
using namespace detu_config_manager;
using namespace detu_media;
using namespace detu_record;

#define TRUE 1
#define FALSE 0
#define UNCOMPLETE 0
#define COMPLETE 1

#define RECORD_M "record"
#define DELAY_TIME "delay_time"
#define REC_STATUS "status"
#define VIDEO_ENTYPE "video_entype"
#define AUDIO_ENTYPE "audio_entype"
#define REC_MODE "record_mode"
#define VALUE "value"

#define RECORD_DIVIDE_TIME (3*60*1000*1000) //second
#define PID_NULL			((pthread_t)(-1))

#define PACKET_SIZE_MAX (15*1024*1024)

#define PACKET_SIZE_MARK (9*1024*1024)

#define CHN_COUNT 5

#define GOP_COUNT_MAX 15

#define FRAME_COUNT_MAX 35

#define FLUSH_CMD "echo 1 > /proc/sys/vm/drop_caches"

#define FILE_SIZE_MAX (4l*1000*1000*1000)


typedef enum
{
	NALU_TYPE_DEFAULT = 0,
	NALU_TYPE_H264_IDR = 5,
	NALU_TYPE_H264_SEI = 6,
	NALU_TYPE_H264_SPS = 7,
	NALU_TYPE_H264_PPS = 8,
	NALU_TYPE_H265_IDR_W_RADL = 19,
	NALU_TYPE_H265_IDR_N_LP   = 20,
	NALU_TYPE_H265_VPS        = 32,
	NALU_TYPE_H265_SPS        = 33,
	NALU_TYPE_H265_PPS        = 34,
	NALU_TYPE_H265_SEI_PREFIX = 39,
	NALU_TYPE_H265_SEI_SUFFIX = 40,
} NALU_TYPE_E;

typedef enum
{
	THD_STAT_START,
	THD_STAT_STOP,
	THD_STAT_QUIT,
	THD_STAT_MAX,
} THD_STAT_E;

typedef enum
{
	RECORD_MODE_SINGLE,
	RECORD_MODE_MULTI,
	RECORD_MODE_MAX,

} RECORD_MODE_E;


typedef enum
{
	CODEC_H264,
	CODEC_H265,
	CODEC_MAX,

} CODEC_TYPE_E;

typedef struct record_config
{
	unsigned int delay_time; //延时录像时间
	THD_STAT_E thd_stat;
	CODEC_TYPE_E entype;
	RECORD_MODE_E record_mode;

} RECORD_USER_CONFIG_S;

typedef struct record_cond_s
{
	pthread_cond_t cond;
	pthread_mutex_t mutex;
	int				wake;
	int				wake_success;
}RECORD_COND_S;

typedef struct record_thread_params
{
	unsigned int delay_time; //延时录像时间
	unsigned int divide_time; //分时录像时间
	pthread_t           gpid;
	pthread_t           fpid;
	pthread_t           wpid;
	pthread_t           lpid;
	volatile THD_STAT_E thd_stat;
	RECORD_COND_S		record_cond;//条件变量，录像控制
	pthread_mutex_t     mutex;//命令锁，保证同时只执行一条命令
	MediaType       stream_t; //音视频
	CODEC_TYPE_E		entype;//编码类型
	RECORD_MODE_E		record_mode;//录像模式

} RECORD_THREAD_PARAMS_S, *p_record_thread_params_s;

typedef struct framelist_s
{
	Uint8_t frame[PACKET_SIZE_MAX];
	int chn;
	int last; //是否是最后一帧数据
	int first; //是否是第一帧数据

	Uint32_t pts[FRAME_COUNT_MAX];
	Uint32_t packetSize[FRAME_COUNT_MAX];
	Uint32_t gopsize;
	Uint32_t frame_count;
	CODEC_TYPE_E		entype;//编码类型
	LIST_HEAD_S list;

}FRAMELIST_S;

typedef struct listinfo_s
{
	//pthread_cond_t cond;
	pthread_mutex_t mutex;
	pthread_spinlock_t spin_lock;
	sem_t sem;
	Uint32_t gop_count;
	FRAMELIST_S *listhead;
} LISTINFO_S;


static char s_rec_status[THD_STAT_MAX][16] = {"start", "stop", "quit"};
static char s_rec_mode[RECORD_MODE_MAX][16] = {"avs", "separate"};
static char s_video_entype[CODEC_MAX][16] = {"H264", "H265"};

static char s_video_dir[CHN_COUNT][16] = {AVS_VIDEO_DIR, CH0_VIDEO_DIR, CH1_VIDEO_DIR, CH2_VIDEO_DIR, CH3_VIDEO_DIR};
static FRAMELIST_S *s_framelist_node[CHN_COUNT];
static int s_record_enable_flag = TRUE;

static p_record_thread_params_s s_p_record_thd_param;

static LISTINFO_S *s_framelist_info;
static FRAMELIST_S *s_framelist_head;

static S_Result record_thread_start(RECORD_USER_CONFIG_S usercfg);
static S_Result record_thread_stop(void);
//static S_Result record_thread_restart(RECORD_USER_CONFIG_S usercfg);

/*
static NALU_TYPE_E get_the_nalu_type(Uint8_t *packet, CODEC_TYPE_E encode_type)
{
	if (CODEC_H264 == encode_type)
	{
		return (NALU_TYPE_E)(packet[0] & 0x1F);
	}
	else
	{
		return (NALU_TYPE_E)((packet[0] & 0x7E)>>1);
	}
	return NALU_TYPE_DEFAULT;
}
*/
inline static NALU_TYPE_E get_the_nalu_type(Uint8_t *packet, CODEC_TYPE_E encode_type)
{

	return (NALU_TYPE_E)((CODEC_H264 == encode_type) ? (packet[0] & 0x1F) : ((packet[0] & 0x7E)>>1));

}

static int Is_the_frame_completed(Uint8_t *packet, CODEC_TYPE_E encode_type)
{
	NALU_TYPE_E nalu_type;
	int ret = UNCOMPLETE;
	nalu_type = get_the_nalu_type(packet + 4, encode_type);
	if (CODEC_H264 == encode_type)
	{
		switch (nalu_type)
		{
			case NALU_TYPE_H264_SPS:
				break;
			case NALU_TYPE_H264_PPS:
				break;
			case NALU_TYPE_H264_SEI:
				break;

			default:
				ret = COMPLETE;
				break;
		}
	}
	else
	{
		switch (nalu_type)
		{
			case NALU_TYPE_H265_VPS:
				break;
			case NALU_TYPE_H265_SPS:
				break;
			case NALU_TYPE_H265_PPS:
				break;
			case NALU_TYPE_H265_SEI_PREFIX:
				break;
			case NALU_TYPE_H265_SEI_SUFFIX:
				break;

			default:
				ret = COMPLETE;
				break;
		}


	}

	return ret;
}


static int Is_the_Bframe(Uint8_t *packet, CODEC_TYPE_E encode_type)
{
	int ret = FALSE;
	NALU_TYPE_E nalu_type;

	nalu_type = get_the_nalu_type(packet + 4, encode_type);
	if (CODEC_H264 == encode_type)
	{
		switch (nalu_type)
		{
			case NALU_TYPE_H264_IDR:
				break;
			case NALU_TYPE_H264_SPS:
				break;
			case NALU_TYPE_H264_PPS:
				break;
			case NALU_TYPE_H264_SEI:
				break;

			default:
				ret = TRUE;
				break;
		}
	}
	else
	{
		switch (nalu_type)
		{
			case NALU_TYPE_H265_IDR_W_RADL:
				break;
			case NALU_TYPE_H265_IDR_N_LP:
				break;
			case NALU_TYPE_H265_VPS:
				break;
			case NALU_TYPE_H265_SPS:
				break;
			case NALU_TYPE_H265_PPS:
				break;
			case NALU_TYPE_H265_SEI_PREFIX:
				break;
			case NALU_TYPE_H265_SEI_SUFFIX:
				break;

			default:
				ret = TRUE;
				break;
		}


	}
	return ret;
}


static int Is_gopsize_reach_mark(Uint32_t packetSize)
{

	if (PACKET_SIZE_MARK <= packetSize)
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}

}

static S_Result record_get_mp4_filename(char *filename, int index)
{
	time_t time_seconds = time(NULL);
	struct tm local_time;
	localtime_r(&time_seconds, &local_time);

	snprintf(filename, FILE_NAME_LEN_MAX, "%s%s/%d%02d%02d%02d%02d%02d-%s.mp4", MOUNT_DIR, s_video_dir[index], local_time.tm_year + 1900, local_time.tm_mon + 1,
		local_time.tm_mday, local_time.tm_hour, local_time.tm_min, local_time.tm_sec, s_video_dir[index]);

	return S_OK;
}


static S_Result record_set_chn(int *chn, CODEC_TYPE_E encode_type)
{
	if (CODEC_H264 == encode_type)
	{
		chn[0] = CHN12_AVS_H264;
		chn[1] = CHN4_PIPE0_H264;
		chn[2] = CHN5_PIPE1_H264;
		chn[3] = CHN6_PIPE2_H264;
		chn[4] = CHN7_PIPE3_H264;
	}
	else
	{
		chn[0] = CHN13_AVS_H265;
		chn[1] = CHN0_PIPE0_H265;
		chn[2] = CHN1_PIPE1_H265;
		chn[3] = CHN2_PIPE2_H265;
		chn[4] = CHN3_PIPE3_H265;
	}

	return S_OK;
}


static int get_max_fd(int *fd, int num)
{
	int i, fd_max = -1;
	for (i = 0; i < num; i++)
	{
		if (fd[i] >= fd_max)
		{
			fd_max = fd[i];
		}
	}

	return fd_max;
}

static FRAMELIST_S *record_alloc_framelist_node(void)
{
	FRAMELIST_S *framelist_node = NULL;
	framelist_node = (FRAMELIST_S *)malloc(sizeof(FRAMELIST_S));
	if (NULL == framelist_node)
	{
		return NULL;
	}
	memset(framelist_node, 0, sizeof(FRAMELIST_S));
	framelist_node->gopsize = 0;

	return framelist_node;
}

static S_Result record_add_gop(FRAMELIST_S *goplist_node)
{
	if ((NULL != s_framelist_info) && (NULL != s_framelist_head) && (NULL != goplist_node))
	{
		pthread_spin_lock(&s_framelist_info->spin_lock);
		if ((GOP_COUNT_MAX > s_framelist_info->gop_count) || (TRUE == goplist_node->last))
		{
			list_add_tail(&goplist_node->list, &s_framelist_head->list);
			s_framelist_info->gop_count++;
			if (1 == s_framelist_info->gop_count)
			{
				sem_post(&s_framelist_info->sem);
			}
		}
		else
		{
			printf("chn[%d]drop frame,too many frame in list\n", goplist_node->chn);
			free(goplist_node);
		}
		pthread_spin_unlock(&s_framelist_info->spin_lock);
	}
	else
	{
		return S_ERROR;
	}

	return S_OK;
}

static void *record_get_frame_thread(void *p)
{
	int fd_max = -1;
	int i = 0, j = 0, chn_num = 0;
	int chn[CHN_COUNT] = {0};
	uint32_t seq[CHN_COUNT] = {0u};
	int frame_count = 0;
	Uint8_t *packet = NULL;
	Uint32_t packetSize = PACKET_SIZE_MAX;
	Uint64_t pts = 0;
	int result = 0;
	Uint64_t start_time[CHN_COUNT] = {0};
	Uint64_t file_size[CHN_COUNT] = {0};
	fd_set inputs, testfds;
	struct timeval timeout;
	int fd[CHN_COUNT];
	VideoEncodeMgr& videoEncoder = *VideoEncodeMgr::instance();
	int fd_found[CHN_COUNT];
	int index;
	memset(fd_found, -1, CHN_COUNT * sizeof(int));

	FRAMELIST_S *framelist_node = NULL;
#if 1
	while (s_record_enable_flag)
	{

		pthread_mutex_lock(&s_p_record_thd_param->record_cond.mutex);
		while ((THD_STAT_START != s_p_record_thd_param->thd_stat) && (THD_STAT_QUIT != s_p_record_thd_param->thd_stat))
		{
			pthread_cond_wait(&s_p_record_thd_param->record_cond.cond, &s_p_record_thd_param->record_cond.mutex);
		}
		pthread_mutex_unlock(&s_p_record_thd_param->record_cond.mutex);

		if (THD_STAT_QUIT == s_p_record_thd_param->thd_stat)
		{
			break;//record destroy
		}

		record_set_chn(chn, s_p_record_thd_param->entype);

		if (RECORD_MODE_SINGLE == s_p_record_thd_param->record_mode)
		{
			videoEncoder.getFd(chn[0], fd[0]);
			for (i = 1; i < CHN_COUNT; i++)
			{
				fd[i] = -1;
			}
		}
		else
		{
			fd[0] = -1;
			for (i = 0; i < CHN_COUNT; i++)
			{
				videoEncoder.getFd(chn[i], fd[i]);
			}
		}
		/*
			1.to do 是否接入sd卡
			2.to do sd是否已进行过格式化
			3.失败 goto start_record，提醒用户进行相关操作并重试。
		*/


		if (S_ERROR == storage_sdcard_check())
		{
			pthread_mutex_lock(&s_p_record_thd_param->record_cond.mutex);
			s_p_record_thd_param->record_cond.wake_success = FALSE;
			pthread_cond_broadcast(&s_p_record_thd_param->record_cond.cond);//通知录像启动失败
			s_p_record_thd_param->thd_stat = THD_STAT_STOP;
			pthread_mutex_unlock(&s_p_record_thd_param->record_cond.mutex);
			continue;
		}

		sleep(s_p_record_thd_param->delay_time);
		FD_ZERO(&inputs);//用select函数之前先把集合清零
		if (RECORD_MODE_SINGLE == s_p_record_thd_param->record_mode)
		{
			FD_SET(fd[0],&inputs);//把要监测的句柄——fd,加入到集合里
			fd_max = fd[0];
			videoEncoder.startRecvStream(chn[0]);
			chn_num = 1;
		}
		else
		{
			fd_max = get_max_fd(&fd[0], CHN_COUNT);
			for (i = 0; i < CHN_COUNT; i++)
			{
				videoEncoder.startRecvStream(chn[i]);
				FD_SET(fd[i],&inputs);
			}
			chn_num = CHN_COUNT;
		}
		pthread_mutex_lock(&s_p_record_thd_param->record_cond.mutex);
		if (FALSE == s_p_record_thd_param->record_cond.wake)
		{
			s_p_record_thd_param->record_cond.wake = TRUE;
			pthread_cond_broadcast(&s_p_record_thd_param->record_cond.cond);//通知录像已处于正常运行
		}
		pthread_mutex_unlock(&s_p_record_thd_param->record_cond.mutex);
		while (THD_STAT_START == s_p_record_thd_param->thd_stat)
		{
			timeout.tv_sec = 2;
			timeout.tv_usec = 500000;
			testfds = inputs;
			result = select(fd_max+1, &testfds, NULL, NULL, &timeout);
			switch (result)
			{
				case 0:
					printf("timeout\n");
					break;
				case -1:
					perror("select");
					goto err;
				default:
	/*  1.分时录像，第一个I帧到最后一个B帧时间差要小于20分钟
		2.延时录像，录像开始时间延后delay_time
		3.录像文件大小，单个不超过2G
	*/
					for (i = 0, j = 0; i < chn_num; i++)
					{
						if (FD_ISSET(fd[i],&testfds))
						{
							fd_found[j] = i;
							j++;
						}
					}
					if (0 == j)
					{
						printf("not found available fd");
						break;
					}
					for (i = 0; i < j; i++)
					{
						index = fd_found[i];
						if (NULL == s_framelist_node[index])
						{
							s_framelist_node[index] = record_alloc_framelist_node();
							if (NULL == s_framelist_node[index])
							{
								perror("record_alloc_framelist_node:");
								printf("record_alloc_framelist_node failed,try another once\n");
								s_framelist_node[index] = record_alloc_framelist_node();
								if (NULL == s_framelist_node[index])
								{
									s_p_record_thd_param->thd_stat = THD_STAT_STOP;
									goto err;
								}
							}
							s_framelist_node[index]->chn = index;
							s_framelist_node[index]->entype = s_p_record_thd_param->entype;
						}
						framelist_node = s_framelist_node[index];
						packet = framelist_node->frame + framelist_node->gopsize;
						packetSize = PACKET_SIZE_MAX - framelist_node->gopsize;
						if (-1 == videoEncoder.getStream(chn[index], seq[index], packet, packetSize, pts))
						{
							record_add_gop(framelist_node);
							s_framelist_node[index] = NULL;
							break;
						}

						if (0 == start_time[index])
						{
							if (TRUE == Is_the_Bframe(packet, framelist_node->entype))
							{
								break;
							}
						}
						//printf("chn:%d, packet size:%d\n", fd_found[i], packetSize);
						framelist_node->gopsize += packetSize;

						if (UNCOMPLETE == Is_the_frame_completed(packet, framelist_node->entype))//组合第一帧数据
						{
							framelist_node->packetSize[frame_count] += packetSize;
							break;
						}
						frame_count = framelist_node->frame_count++;
						if (0 == start_time[index])
						{
							start_time[index] = pts;
							file_size[index] = 0;
							framelist_node->first = TRUE;//文件的第一组gop，新建MP4文件
							framelist_node->pts[frame_count] = 0;
						}
						else
						{
							framelist_node->pts[frame_count] = (pts - start_time[index])/1000;
						}
						framelist_node->packetSize[frame_count] += packetSize;
						if (Is_gopsize_reach_mark(framelist_node->gopsize) || ((FRAME_COUNT_MAX - 1) <= framelist_node->frame_count))
						{
							file_size[index] += framelist_node->gopsize;
							if (s_p_record_thd_param->divide_time <= (pts -start_time[index]))//分时功能
							{
								framelist_node->last = TRUE;//最后一组gop，关闭MP4文件
								start_time[index] = 0;
								file_size[index] = 0;
								printf("time reach mark!\n");
							}
							else if (FILE_SIZE_MAX <= file_size[index])//文件大小不能超过2G
							{
								framelist_node->last = TRUE;//最后一组gop，关闭MP4文件
								start_time[index] = 0;
								file_size[index] = 0;
								printf("file size reach mark!\n");
							}
							record_add_gop(framelist_node);
							s_framelist_node[index] = NULL;//处理完数据指针置为NULL
						}

					}
					break;
				}
		}
err:
		if (THD_STAT_START != s_p_record_thd_param->thd_stat)//录像结束关闭MP4文件
		{
			if (RECORD_MODE_SINGLE == s_p_record_thd_param->record_mode)
			{
				if (NULL != s_framelist_node[0])
				{
					s_framelist_node[0]->last = TRUE;
					s_framelist_node[0]->first = FALSE;
					record_add_gop(s_framelist_node[0]);
					s_framelist_node[0] = NULL;
				}
				else
				{
					s_framelist_node[0] = record_alloc_framelist_node();
					s_framelist_node[0]->entype = s_p_record_thd_param->entype;
					s_framelist_node[0]->chn = 0;
					s_framelist_node[0]->last = TRUE;
					s_framelist_node[0]->first = FALSE;
					record_add_gop(s_framelist_node[0]);
					s_framelist_node[0] = NULL;
				}
				start_time[0] = 0;
				file_size[0] = 0;
				videoEncoder.stopRecvStream(chn[0]);
				printf("record stop!\n");
			}
			else
			{
				for (i = 0; i < CHN_COUNT; i++)
				{
					if (NULL != s_framelist_node[i])
					{
						s_framelist_node[i]->last = TRUE;
						s_framelist_node[i]->first = FALSE;
						record_add_gop(s_framelist_node[i]);
						s_framelist_node[i] = NULL;
					}
					else
					{
						s_framelist_node[i] = record_alloc_framelist_node();
						s_framelist_node[i]->entype = s_p_record_thd_param->entype;
						s_framelist_node[i]->chn = i;
						s_framelist_node[i]->last = TRUE;
						s_framelist_node[i]->first = FALSE;
						record_add_gop(s_framelist_node[i]);
						s_framelist_node[i] = NULL;

					}
					videoEncoder.stopRecvStream(chn[i]);
					start_time[i] = 0;
					file_size[i] = 0;
				}
				printf("record stop!\n");
			}
			pthread_mutex_lock(&s_p_record_thd_param->record_cond.mutex);
			if (TRUE == s_p_record_thd_param->record_cond.wake)
			{
				s_p_record_thd_param->record_cond.wake = FALSE;
				pthread_cond_broadcast(&s_p_record_thd_param->record_cond.cond);//通知录像已关闭
			}
			pthread_mutex_unlock(&s_p_record_thd_param->record_cond.mutex);
		}

	}
#endif

	return (void *)S_OK;

}

static void *record_flush_cache_thread(void *p)//每写四次gop，flush一次
{
	while (s_record_enable_flag)
	{
		pthread_mutex_lock(&s_p_record_thd_param->record_cond.mutex);
		while ((THD_STAT_START != s_p_record_thd_param->thd_stat) && (THD_STAT_QUIT != s_p_record_thd_param->thd_stat))
		{
			pthread_cond_wait(&s_p_record_thd_param->record_cond.cond, &s_p_record_thd_param->record_cond.mutex);
		}
		pthread_mutex_unlock(&s_p_record_thd_param->record_cond.mutex);

		if (THD_STAT_QUIT == s_p_record_thd_param->thd_stat)
		{
			break;//record destroy
		}
		while (THD_STAT_START == s_p_record_thd_param->thd_stat)
		{
			sleep(60);
			sync();
			system(FLUSH_CMD);
		}

	}
	return (void *)S_OK;
}

static void *record_write_mp4_thread(void *p)
{
	int chn = -1;
	int state = 0;
	unsigned int i = 0;
	int offset = 0;

	Mp4Encoder *mp4_encoder = new Mp4Encoder[CHN_COUNT];
	char filename[FILE_NAME_LEN_MAX];
	memset(filename, 0, FILE_NAME_LEN_MAX);
	FRAMELIST_S *framelist_node = NULL;

	VideoParamter video_param = {AV_CODEC_ID_H264, 3840, 2160};
	AudioParamter audio_param = {AV_CODEC_ID_NONE, 0, 0, 0};

	while (s_record_enable_flag)
	{
		if (0 == s_framelist_info->gop_count)
		{
			do
			{
				//pthread_cond_wait(&s_framelist_info->cond, &s_framelist_info->mutex);
				sem_wait(&s_framelist_info->sem);
				if(((FALSE == s_record_enable_flag)
					|| (THD_STAT_QUIT == s_p_record_thd_param->thd_stat)) && (0 == s_framelist_info->gop_count))
				{
					goto exit;//退出线程
				}

			}while(0 == s_framelist_info->gop_count);
		}

		framelist_node = list_entry(s_framelist_head->list.next, FRAMELIST_S, list);
		chn = framelist_node->chn;
		if (TRUE == framelist_node->first)
		{
			record_get_mp4_filename(filename, chn);
			video_param.codec_id_ = (CODEC_H264 == framelist_node->entype) ? AV_CODEC_ID_H264 :  AV_CODEC_ID_H265;
			if (!mp4_encoder[chn].Init(filename, video_param, audio_param))
			{
				DBG_INFO("Mp4 init failed\n");
				goto exit;
			}
		}
		if (0 != framelist_node->gopsize)
		{
			for (i = 0, offset = 0; i < framelist_node->frame_count; i++)
			{
				if (0 != (mp4_encoder[chn].WriteVideoFrame((char*)framelist_node->frame + offset, framelist_node->packetSize[i], framelist_node->pts[i])))
				{
					DBG_INFO("Write video frame failed\n");
				}
				offset += framelist_node->packetSize[i];
			}
		}
		if (TRUE == framelist_node->last)
		{
			mp4_encoder[chn].GetFileState(state);
			if (1 == state)
			{
				mp4_encoder[chn].Close();
				mp4_encoder[chn].Release();
				printf("chn[%d]close file\n", chn);
			}
		}
		pthread_spin_lock(&s_framelist_info->spin_lock);
		list_del_init(&framelist_node->list);
		s_framelist_info->gop_count--;
		free(framelist_node);
		//record_flush_cache();
		pthread_spin_unlock(&s_framelist_info->spin_lock);

	}

exit:

	for (chn = 0; chn < CHN_COUNT; chn++)
	{
		mp4_encoder[chn].GetFileState(state);
		if (1 == state)//open 状态需关闭
		{
			mp4_encoder[chn].Close();
			mp4_encoder[chn].Release();
		}
	}
	delete []mp4_encoder;
	return (void *)S_OK;
}

static S_Result record_framelist_init(void)
{
	s_framelist_info = (LISTINFO_S *)malloc(sizeof(LISTINFO_S));
	if (NULL == s_framelist_info)
	{
		perror("s_fileinfo malloc failed:");
		printf("filelist_init failed\n");
		return S_ERROR;
	}
	s_framelist_info->gop_count = 0;
	s_framelist_info->mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_spin_init(&s_framelist_info->spin_lock, PTHREAD_PROCESS_PRIVATE);
	//s_framelist_info->cond = PTHREAD_COND_INITIALIZER;
	sem_init(&(s_framelist_info->sem),0,0);
	s_framelist_info->listhead = (FRAMELIST_S *)malloc(sizeof(FRAMELIST_S));
	if (NULL == s_framelist_info->listhead)
	{
		perror("s_fileinfo->head malloc failed:");
		printf("filelist_init failed\n");
		return S_ERROR;
	}
	INIT_LIST_HEAD(&s_framelist_info->listhead->list);
	s_framelist_head = s_framelist_info->listhead;

	return S_OK;
}

int gpio_test_in(unsigned int gpio_chip_num, unsigned int gpio_offset_num)
{
        FILE *fp;
        char file_name[50];
        char buf[10];
        unsigned int gpio_num;

        gpio_num = gpio_chip_num * 8 + gpio_offset_num;
        sprintf(file_name, "/sys/class/gpio/export");
        fp = fopen(file_name, "w");
        if (fp == NULL) {
                printf("Cannot open %s.\n", file_name);
                return -1;
        }
        fprintf(fp, "%d", gpio_num);
        fclose(fp);

        sprintf(file_name, "/sys/class/gpio/gpio%d/direction", gpio_num);
        fp = fopen(file_name, "rb+");
        if (fp == NULL) {
                printf("Cannot open %s.\n", file_name);
                return -1;
        }
        fprintf(fp, "in");
        fclose(fp);

        sprintf(file_name, "/sys/class/gpio/gpio%d/value", gpio_num);
        fp = fopen(file_name, "rb+");
        if (fp == NULL) {
                printf("Cannot open %s.\n", file_name);
                return -1;
        }

        memset(buf, 0, 10);
        fread(buf, sizeof(char), sizeof(buf) - 1, fp);
        //printf("%s: gpio%d_%d = %d\n", __func__,
        //gpio_chip_num, gpio_offset_num, buf[0]-48);
        fclose(fp);

        sprintf(file_name, "/sys/class/gpio/unexport");
        fp = fopen(file_name, "w");
        if (fp == NULL) {
                printf("Cannot open %s.\n", file_name);
                return -1;
        }
        fprintf(fp, "%d", gpio_num);
        fclose(fp);
        return (int)(buf[0]-48);
}

int gpio_test_out(unsigned int gpio_chip_num, unsigned int gpio_offset_num, int gpio_out_val)
{
        FILE *fp;
        char file_name[50];
        char buf[10];
        unsigned int gpio_num;
        gpio_num = gpio_chip_num * 8 + gpio_offset_num;
        sprintf(file_name, "/sys/class/gpio/export");
        fp = fopen(file_name, "w");
        if (fp == NULL) {
        printf("Cannot open %s.\n", file_name);
        return -1;
        }
        fprintf(fp, "%d", gpio_num);
        fclose(fp);

        sprintf(file_name, "/sys/class/gpio/gpio%d/direction", gpio_num);
        fp = fopen(file_name, "rb+");
        if (fp == NULL) {
        printf("Cannot open %s.\n", file_name);
        return -1;
        }
        fprintf(fp, "out");
        fclose(fp);

        sprintf(file_name, "/sys/class/gpio/gpio%d/value", gpio_num);
        fp = fopen(file_name, "rb+");
        if (fp == NULL) {
        printf("Cannot open %s.\n", file_name);
        return -1;
        }
        if (gpio_out_val)
                strcpy(buf,"1");
        else
                strcpy(buf,"0");
        fwrite(buf, sizeof(char), sizeof(buf) - 1, fp);
        //printf("%s: gpio%d_%d = %s\n", __func__,
        //gpio_chip_num, gpio_offset_num, buf);
        fclose(fp);

        sprintf(file_name, "/sys/class/gpio/unexport");
        fp = fopen(file_name, "w");
        if (fp == NULL) {
        printf("Cannot open %s.\n", file_name);
        return -1;
        }
        fprintf(fp, "%d", gpio_num);
        fclose(fp);
        return 0;
}

static void *record_listen_cmd_host_thread(void *p)
{
	int status_cur = 0, status_bef = 0;
	ConfigManager& config = *ConfigManager::instance();

	Json::Value recCfg, response;

	//status_cur = gpio_test_in(4,2);
	gpio_test_out(7,5,0);
	status_bef = status_cur;
	while (s_record_enable_flag)
	{
		//status_cur = gpio_test_in(4,2);
		if (status_cur != status_bef)
		{
			config.getTempConfig("record.status.value", recCfg, response);
			if (!(recCfg.asString()).compare("stop"))
			{
				gpio_test_out(7,5,1);
				recCfg = "start";
				config.setTempConfig("record.status.value", recCfg, response);
			}
			else
			{
				gpio_test_out(7,5,0);
				recCfg = "stop";
				config.setTempConfig("record.status.value", recCfg, response);
			}
		}
		status_bef = status_cur;
	}

	return (void *)S_OK;
}

static void *record_listen_cmd_slave_thread(void *p)
{
	int status_cur = 0, status_bef = 0, count = 0;
	ConfigManager& config = *ConfigManager::instance();

	Json::Value recCfg, response;

	//status_cur = gpio_test_in(7,5);
	//status_bef = status_cur;
	while (s_record_enable_flag)
	{
		if (status_cur != status_bef)
		{
				count = !count;
				if(count)
				{
					config.getTempConfig("record.status.value", recCfg, response);
					recCfg = "start";
					config.setTempConfig("record.status.value", recCfg, response);
				}
				else
				{
					config.getTempConfig("record.status.value", recCfg, response);
					recCfg = "stop";
					config.setTempConfig("record.status.value", recCfg, response);
				}
		}
		status_bef = status_cur;
		status_cur = gpio_test_in(7,5);
		usleep(10*1000);
	}
	return (void *)S_OK;

}

static S_Result record_thread_create(void)
{
//	pthread_attr_t attr1,attr2;
//	struct sched_param param;
	ConfigManager& config = *ConfigManager::instance();

	Json::Value chipCfg, response;

	config.getConfig("chip.type.value", chipCfg, response);

	if (S_OK != record_framelist_init())
	{
		printf("pthread_create get frame thread failed\n");
		return S_ERROR;
	}
/*
	pthread_attr_init(&attr1);
	pthread_attr_init(&attr2);

	param.sched_priority = 99;
	pthread_attr_setschedpolicy(&attr2,SCHED_RR);
	pthread_attr_setschedparam(&attr2,&param);
	pthread_attr_setinheritsched(&attr2,PTHREAD_EXPLICIT_SCHED);//要使优先级其作用必须要有这句话

	param.sched_priority = 1;
	pthread_attr_setschedpolicy(&attr1,SCHED_RR);
	pthread_attr_setschedparam(&attr1,&param);
	pthread_attr_setinheritsched(&attr1,PTHREAD_EXPLICIT_SCHED);

*/
	if (0 != pthread_create(&s_p_record_thd_param->gpid, NULL, record_get_frame_thread, NULL))
	{
		perror("pthread_create failed");
		printf("pthread_create get frame thread failed\n");
		return S_ERROR;
	}
	pthread_setname_np(s_p_record_thd_param->gpid, "recg\0");
	if (0 != pthread_create(&s_p_record_thd_param->wpid, NULL, record_write_mp4_thread, NULL))
	{
		perror("pthread_create failed");
		printf("pthread_create write mp4 thread failed\n");
		return S_ERROR;
	}
	pthread_setname_np(s_p_record_thd_param->wpid, "recw\0");
	if (0 != pthread_create(&s_p_record_thd_param->fpid, NULL, record_flush_cache_thread, NULL))
	{
		perror("pthread_create failed");
		printf("pthread_create flush cache thread failed\n");
		return S_ERROR;
	}
	pthread_setname_np(s_p_record_thd_param->fpid, "recf\0");
	if (0 == chipCfg.asString().compare("host"))
	{
		if (0 != pthread_create(&s_p_record_thd_param->lpid, NULL, record_listen_cmd_host_thread, NULL))
		{
			perror("pthread_create failed");
			printf("pthread_create flush cache thread failed\n");
			return S_ERROR;
		}
	}
	else
	{
		if (0 != pthread_create(&s_p_record_thd_param->lpid, NULL, record_listen_cmd_slave_thread, NULL))
		{
			perror("pthread_create failed");
			printf("pthread_create flush cache thread failed\n");
			return S_ERROR;
		}

	}
	pthread_setname_np(s_p_record_thd_param->lpid, "recl\0");
	return S_OK;

}

static S_Result record_thread_stop(void)
{
	pthread_mutex_lock(&s_p_record_thd_param->mutex);

	pthread_mutex_lock(&s_p_record_thd_param->record_cond.mutex);
	if (THD_STAT_START == s_p_record_thd_param->thd_stat)
	{
		s_p_record_thd_param->thd_stat = THD_STAT_STOP;
		do
		{
			pthread_cond_wait(&s_p_record_thd_param->record_cond.cond, &s_p_record_thd_param->record_cond.mutex);//确保在录像停止之前不接受其他命令
		}while(TRUE == s_p_record_thd_param->record_cond.wake); //等待wake置为false时退出
	}
	pthread_mutex_unlock(&s_p_record_thd_param->record_cond.mutex);

	pthread_mutex_unlock(&s_p_record_thd_param->mutex);

	return S_OK;
}

static S_Result record_thread_start(RECORD_USER_CONFIG_S usercfg)
{
	S_Result S_ret = S_OK;

	pthread_mutex_lock(&s_p_record_thd_param->mutex);

	pthread_mutex_lock(&s_p_record_thd_param->record_cond.mutex);

	if (THD_STAT_START != s_p_record_thd_param->thd_stat)
	{
		s_p_record_thd_param->thd_stat = THD_STAT_START;

		s_p_record_thd_param->delay_time = usercfg.delay_time;
		s_p_record_thd_param->entype = usercfg.entype;
		s_p_record_thd_param->record_mode = usercfg.record_mode;
		pthread_cond_broadcast(&s_p_record_thd_param->record_cond.cond);
		do
		{
			pthread_cond_wait(&s_p_record_thd_param->record_cond.cond, &s_p_record_thd_param->record_cond.mutex);//确保在录像正常运行之前不接受其他命令
			if (FALSE == s_p_record_thd_param->record_cond.wake_success)
			{
				s_p_record_thd_param->record_cond.wake_success = TRUE;//置为初始值
				S_ret = S_ERROR;
				break;//启动失败退出
			}
		}while(FALSE == s_p_record_thd_param->record_cond.wake);
	}
	pthread_mutex_unlock(&s_p_record_thd_param->record_cond.mutex);

	pthread_mutex_unlock(&s_p_record_thd_param->mutex);

	return S_ret;
}

static void record_cond_init(RECORD_COND_S *record_cond)
{
	record_cond->mutex = PTHREAD_MUTEX_INITIALIZER; //初始化互斥锁
	record_cond->cond = PTHREAD_COND_INITIALIZER; //初始化条件变量
	record_cond->wake = FALSE;
	record_cond->wake_success = TRUE; //默认唤醒操作可以执行成功,发生失败时置为FALSE
}

static void record_cond_deinit(RECORD_COND_S *record_cond)
{
	pthread_mutex_destroy(&record_cond->mutex);
	pthread_cond_destroy(&record_cond->cond);
}

static S_Result record_trans_config(const Json::Value& config,RECORD_USER_CONFIG_S& usercfg)
{
	int i = 0;
	if (config.isMember(REC_STATUS) && config[REC_STATUS].isMember(VALUE))
	{
		for (i = 0; i < THD_STAT_MAX; i++)
		{
			if (!(config[REC_STATUS][VALUE].asString()).compare(s_rec_status[i]))
			{
				usercfg.thd_stat = (THD_STAT_E)i;
				break;
			}
		}
	}
	if (config.isMember(VIDEO_ENTYPE))
	{
		for (i = 0; i < CODEC_MAX; i++)
		{
			if (!(config[VIDEO_ENTYPE][VALUE].asString()).compare(s_video_entype[i]))
			{
				usercfg.entype = (CODEC_TYPE_E)i;
				break;
			}
		}
	}
	if (config.isMember(REC_MODE))
	{
		for (i = 0; i < RECORD_MODE_MAX; i++)
		{
			if (!(config[REC_MODE][VALUE].asString()).compare(s_rec_mode[i]))
			{
				usercfg.record_mode = (RECORD_MODE_E)i;
				break;
			}
		}
	}
	if (config.isMember(REC_MODE))
	{
		usercfg.delay_time = config[DELAY_TIME][VALUE].asUInt();
	}

	return S_OK;

}

static S_Result record_check_config(const Json::Value& config)
{
	int delay_time = 0;
	int i = 0;
	S_Result S_ret = S_OK;
	if (config.isMember(DELAY_TIME))
	{
		delay_time = config[DELAY_TIME][VALUE].asInt();
	}
	do
	{
		if (config.isMember(REC_STATUS) && config[REC_STATUS].isMember(VALUE))
		{
			for (i = 0; i < THD_STAT_MAX; i++)
			{
				if (!(config[REC_STATUS][VALUE].asString()).compare(s_rec_status[i]))
				{
					break;
				}
			}
			if (THD_STAT_MAX == i)
			{
				S_ret = S_ERROR;
				break;
			}
		}
		if (config.isMember(VIDEO_ENTYPE))
		{
			for (i = 0; i < CODEC_MAX; i++)
			{
				if (!(config[VIDEO_ENTYPE][VALUE].asString()).compare(s_video_entype[i]))
				{
					break;
				}
			}
			if (CODEC_MAX == i)
			{
				S_ret = S_ERROR;
				break;
			}
		}
		if (config.isMember(REC_MODE))
		{
			for (i = 0; i < RECORD_MODE_MAX; i++)
			{
				if (!(config[REC_MODE][VALUE].asString()).compare(s_rec_mode[i]))
				{
					break;
				}
			}
			if (RECORD_MODE_MAX == i)
			{
				S_ret = S_ERROR;
				break;
			}
		}
		if (0 > delay_time)
		{
			S_ret = S_ERROR;
		}
	}while (0);

	return S_ret;

}


static S_Result record_param_init(void)
{
	ConfigManager& config = *ConfigManager::instance();

	Json::Value recCfg, response;

	RECORD_USER_CONFIG_S usercfg;

	config.setTempConfig("record.status.value", "stop", response);
	config.getTempConfig(RECORD_M, recCfg, response);
	printf("init status:%s\n", recCfg.toStyledString().c_str());
	record_trans_config(recCfg, usercfg);

	config.getConfig(RECORD_M, recCfg, response);

	record_trans_config(recCfg, usercfg);
	s_p_record_thd_param = (p_record_thread_params_s)malloc(sizeof(RECORD_THREAD_PARAMS_S));
	if (NULL == s_p_record_thd_param)
	{
		perror("gs_record_thd_param malloc failed:");
		printf("record_param_init failed\n");
		return S_ERROR;
	}
	s_p_record_thd_param->delay_time = usercfg.delay_time;
	s_p_record_thd_param->divide_time = RECORD_DIVIDE_TIME;
	s_p_record_thd_param->gpid = PID_NULL;
	s_p_record_thd_param->fpid = PID_NULL;
	s_p_record_thd_param->wpid = PID_NULL;
	s_p_record_thd_param->lpid = PID_NULL;
	s_p_record_thd_param->thd_stat = usercfg.thd_stat;
	s_p_record_thd_param->stream_t = MEDIA_TYPE_VIDEO;
	s_p_record_thd_param->entype = usercfg.entype;
	s_p_record_thd_param->record_mode = usercfg.record_mode;
	pthread_mutex_init(&s_p_record_thd_param->mutex, NULL);
	record_cond_init(&s_p_record_thd_param->record_cond);

	//sem_init(&(s_p_record_thd_param->wake_sem),0,0);//第二个如果不为0代表进程间共享sem，第三个代表sem值的初始值
	return S_OK;
}


static S_Result record_param_exit(void)
{

	record_cond_deinit(&s_p_record_thd_param->record_cond);
	pthread_mutex_destroy(&s_p_record_thd_param->mutex);
	if (NULL != s_p_record_thd_param)
	{
		free(s_p_record_thd_param);
	}

	return S_OK;
}

static S_Result record_framelist_destroy(void)
{
	FRAMELIST_S *framelist_node = NULL;
	LIST_HEAD_S *pos, *next;
	if ((s_framelist_info != NULL) && (s_framelist_head != NULL))
	{
		list_for_each_safe(pos, next, &s_framelist_head->list)
		{
			framelist_node = list_entry(pos, FRAMELIST_S, list); //获取双链表结构体的地址
			list_del_init(pos);
			s_framelist_info->gop_count--;
			free(framelist_node);
		}
		free(s_framelist_head);
		s_framelist_head = NULL;
	}
	if (s_framelist_info != NULL)
	{
		pthread_mutex_destroy(&s_framelist_info->mutex);
		pthread_spin_destroy(&s_framelist_info->spin_lock);
		//pthread_cond_destroy(&s_framelist_info->cond);
		sem_destroy(&s_framelist_info->sem);
		free(s_framelist_info);
		s_framelist_info = NULL;
	}

	return S_OK;
}


static S_Result record_thread_destroy(void)
{
	s_record_enable_flag = FALSE;
	pthread_mutex_lock(&s_p_record_thd_param->record_cond.mutex);
	s_p_record_thd_param->thd_stat = THD_STAT_QUIT;

	pthread_cond_broadcast(&s_p_record_thd_param->record_cond.cond);//唤醒stop状态的线程

	pthread_mutex_unlock(&s_p_record_thd_param->record_cond.mutex);
	if (s_p_record_thd_param->gpid != PID_NULL)
	{
		pthread_join(s_p_record_thd_param->gpid, 0);
		s_p_record_thd_param->gpid = PID_NULL;
	}
	sem_post(&s_framelist_info->sem);//退出写线程
	if (s_p_record_thd_param->wpid != PID_NULL)
	{
		pthread_join(s_p_record_thd_param->wpid, 0);
		s_p_record_thd_param->wpid = PID_NULL;
	}
	if (s_p_record_thd_param->fpid != PID_NULL)
	{
		pthread_join(s_p_record_thd_param->fpid, 0);
		s_p_record_thd_param->fpid = PID_NULL;
	}

	if (s_p_record_thd_param->lpid != PID_NULL)
	{
		pthread_join(s_p_record_thd_param->lpid, 0);
		s_p_record_thd_param->lpid = PID_NULL;
	}

	record_framelist_destroy();

	return S_OK;
}

static S_Result record_thread_cb(const void* clientData, const std::string& name, const Json::Value& oldConfig, const Json::Value& newConfig, Json::Value& response)
{
	S_Result S_ret = S_OK;
	Json::Value reccfg;
	ConfigManager& config = *ConfigManager::instance();
	RECORD_USER_CONFIG_S oldcfg,newcfg;

	memset(&newcfg, 0, sizeof(newcfg));
	memset(&oldcfg, 0, sizeof(oldcfg));

	config.getConfig(RECORD_M, reccfg, response);
	record_trans_config(reccfg, newcfg);

	record_trans_config(newConfig, newcfg);
	record_trans_config(oldConfig, oldcfg);

	do
	{
		printf("set status:%s, old status:%s!\n", newConfig.toStyledString().c_str(), oldConfig.toStyledString().c_str());
		if (newcfg.thd_stat != oldcfg.thd_stat)
		{
			if (THD_STAT_STOP == newcfg.thd_stat)
			{
				gpio_test_out(7,5,0);
				record_thread_stop();
				break;
			}
			else
			{
				gpio_test_out(7,5,1);
				if (S_ERROR == record_thread_start(newcfg))
				{
					gpio_test_out(7,5,0);
					config.getTempConfig("record.status.value", reccfg, response);
					reccfg = "stop";
					config.setTempConfig("record.status.value", reccfg, response);
					S_ret = S_ERROR;
				}
				break;
			}
		}
/*		if ((newcfg.entype != oldcfg.entype) || (newcfg.record_mode != oldcfg.record_mode))
		{
			if (S_ERROR == record_thread_restart(newcfg))
			{
				config.getTempConfig("record.status.value", reccfg, response);
				reccfg = "stop";
				config.setTempConfig("record.status.value", reccfg, response);
				S_ret = S_ERROR;
			}
		}
*/
	} while (0);

	return S_ret;
}

static S_Result record_thread_vd(const void* clientData, const std::string& name, const Json::Value& oldConfig, const Json::Value& newConfig, Json::Value& response)
{
	S_Result S_ret = S_OK;
	S_ret = record_check_config(newConfig);
	if (S_OK != S_ret)
	{
		printf("config is not valid to set!\n");
	}

	return S_ret;
}

static S_Result record_register_validator()
{
	ConfigManager& config = *ConfigManager::instance();

	config.registerValidator(RECORD_M, &record_thread_vd, (void *)1);

	return S_OK;

}

static S_Result record_register_callback()
{
	ConfigManager& config = *ConfigManager::instance();

	config.registerCallback(RECORD_M, &record_thread_cb, (void *)1);

	return S_OK;
}

static S_Result record_unregister_validator()
{
	ConfigManager& config = *ConfigManager::instance();

	config.unregisterCallback(RECORD_M, &record_thread_vd, (void *)1);

	return S_OK;
}

static S_Result record_unregister_callback()
{
	ConfigManager& config = *ConfigManager::instance();

	config.unregisterCallback(RECORD_M, &record_thread_cb, (void *)1);

	return S_OK;
}

S_Result record_module_init(void)
{
	record_param_init();
	record_register_validator();
	record_register_callback();
	record_thread_create();

	return S_OK;
}

S_Result record_module_exit(void)
{
	record_thread_destroy();
	record_unregister_validator();
	record_unregister_callback();
	record_param_exit();
	return S_OK;
}
#if 0
/*
录像重启
*/
static S_Result record_thread_restart(RECORD_USER_CONFIG_S usercfg)
{
	S_Result S_ret = S_OK;

	pthread_mutex_lock(&s_p_record_thd_param->mutex);

	pthread_mutex_lock(&s_p_record_thd_param->record_cond.mutex);
	if (THD_STAT_START == s_p_record_thd_param->thd_stat)
	{
		s_p_record_thd_param->thd_stat = THD_STAT_STOP;
		do
		{
			pthread_cond_wait(&s_p_record_thd_param->record_cond.cond, &s_p_record_thd_param->record_cond.mutex);//确保在录像停止之前不接受其他命令
		}while(TRUE == s_p_record_thd_param->record_cond.wake); //等待wake置为false时退出

		s_p_record_thd_param->delay_time = usercfg.delay_time;
		s_p_record_thd_param->entype = usercfg.entype;
		s_p_record_thd_param->record_mode = usercfg.record_mode;

		if (THD_STAT_START != s_p_record_thd_param->thd_stat)
		{
			s_p_record_thd_param->thd_stat = THD_STAT_START;
			pthread_cond_broadcast(&s_p_record_thd_param->record_cond.cond);
			do
			{
				pthread_cond_wait(&s_p_record_thd_param->record_cond.cond, &s_p_record_thd_param->record_cond.mutex);//确保在录像正常运行之前不接受其他命令
				if (FALSE == s_p_record_thd_param->record_cond.wake_success)
				{
					s_p_record_thd_param->record_cond.wake_success = TRUE;//置为初始值
					S_ret = S_ERROR;
					break;//启动失败退出
				}
			}while(FALSE == s_p_record_thd_param->record_cond.wake);
		}
	}
	pthread_mutex_unlock(&s_p_record_thd_param->record_cond.mutex);

	pthread_mutex_unlock(&s_p_record_thd_param->mutex);

	return S_ret;
}
#endif
#if 0

/*
延时录像设置
*/
static int record_set_delay_time(int time)
{
	//todo
	pthread_mutex_lock(&s_p_record_thd_param->mutex);
	s_p_record_thd_param->delay_time = time;
	pthread_mutex_unlock(&s_p_record_thd_param->mutex);
	return OK;
}

/*
分时录像设置
*/
static int record_set_divide_time(void)
{
	//暂无需求
	return OK;
}


/*
录像模式设置
*/
static int record_set_record_mode(RECORD_MODE_E mode)
{
	if ((RECORD_MODE_SINGLE != mode) && (RECORD_MODE_MULTI != mode))
	{
		return ERROR;
	}
	pthread_mutex_lock(&s_p_record_thd_param->mutex);

	if (mode != s_p_record_thd_param->record_mode)
	{
		pthread_mutex_lock(&s_p_record_thd_param->record_cond.mutex);
		if (THD_STAT_START == s_p_record_thd_param->thd_stat)
		{
			s_p_record_thd_param->thd_stat = THD_STAT_STOP;
			do
			{
				pthread_cond_wait(&s_p_record_thd_param->record_cond.cond, &s_p_record_thd_param->record_cond.mutex);//确保在录像停止之前不接受其他命令
			}while(TRUE == s_p_record_thd_param->record_cond.wake); //等待wake置为false时退出

			s_p_record_thd_param->record_mode = mode;

			if (THD_STAT_START != s_p_record_thd_param->thd_stat)
			{

				s_p_record_thd_param->thd_stat = THD_STAT_START;
				pthread_cond_broadcast(&s_p_record_thd_param->record_cond.cond);
				do
				{
					pthread_cond_wait(&s_p_record_thd_param->record_cond.cond, &s_p_record_thd_param->record_cond.mutex);//确保在录像正常运行之前不接受其他命令
					if (FALSE == s_p_record_thd_param->record_cond.wake_success)
					{
						s_p_record_thd_param->record_cond.wake_success = TRUE;//置为初始值
						break;//启动失败退出
					}
				}while(FALSE == s_p_record_thd_param->record_cond.wake);
			}
		}
		else
		{
			s_p_record_thd_param->record_mode = mode;//保持录像线程状态start-start,stop-stop
		}
		pthread_mutex_unlock(&s_p_record_thd_param->record_cond.mutex);
	}
	else
	{
		printf("already the mode :%d\n", mode);
	}

	pthread_mutex_unlock(&s_p_record_thd_param->mutex);

	return OK;
}


/*
编码类型设置
*/
static int record_set_encode_type(CODEC_TYPE_E entype)
{
	if ((CODEC_H264 != entype) && (CODEC_H265 != entype))
	{
		return ERROR;
	}
	pthread_mutex_lock(&s_p_record_thd_param->mutex);

	if (entype != s_p_record_thd_param->entype)
	{
		pthread_mutex_lock(&s_p_record_thd_param->record_cond.mutex);
		if (THD_STAT_START == s_p_record_thd_param->thd_stat)
		{
			s_p_record_thd_param->thd_stat = THD_STAT_STOP;
			do
			{
				pthread_cond_wait(&s_p_record_thd_param->record_cond.cond, &s_p_record_thd_param->record_cond.mutex);//确保在录像停止之前不接受其他命令
			}while(TRUE == s_p_record_thd_param->record_cond.wake); //等待wake置为false时退出

			s_p_record_thd_param->entype = entype;

			if (THD_STAT_START != s_p_record_thd_param->thd_stat)
			{

				s_p_record_thd_param->thd_stat = THD_STAT_START;
				pthread_cond_broadcast(&s_p_record_thd_param->record_cond.cond);
				do
				{
					pthread_cond_wait(&s_p_record_thd_param->record_cond.cond, &s_p_record_thd_param->record_cond.mutex);//确保在录像正常运行之前不接受其他命令
					if (FALSE == s_p_record_thd_param->record_cond.wake_success)
					{
						s_p_record_thd_param->record_cond.wake_success = TRUE;//置为初始值
						break;//启动失败退出
					}
				}while(FALSE == s_p_record_thd_param->record_cond.wake);
			}
		}
		else
		{
			s_p_record_thd_param->entype = entype;//保持录像线程状态start-start,stop-stop
		}
		pthread_mutex_unlock(&s_p_record_thd_param->record_cond.mutex);
	}
	else
	{
		printf("already the entype :%d\n", entype);
	}

	pthread_mutex_unlock(&s_p_record_thd_param->mutex);

	return OK;
}
#endif
