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

#include "video_encoder.h"


#include <wchar.h>
#include <stdlib.h>
#include "mp4_encoder.h"
#include "record.h"
#include "storage.h"


using namespace detu_media;
using namespace detu_record;

#define CHN0 0
#define CHN1 1
#define CHN2 2
#define CHN3 3
#define CHN4 4
#define ERROR (-1)
#define OK 0
#define TRUE 1
#define FALSE 0
#define UNCOMPLETE 0
#define COMPLETE 1

#define CH0_VIDEO_DIR "chn0"
#define CH1_VIDEO_DIR "chn1"
#define CH2_VIDEO_DIR "chn2"
#define CH3_VIDEO_DIR "chn3"
#define AVS_VIDEO_DIR "avs"

#define RECORD_DIVIDE_TIME (20*60*1000*1000) //second
#define PID_NULL			((pid_t)(-1))

#define PACKET_SIZE_MAX (10*1024*1024)

#define CHN_COUNT 5

#define GOP_COUNT_MAX 30

#define FRAME_COUNT_MAX 35


typedef enum
{
	THD_STAT_IDLE,
	THD_STAT_START,
	THD_STAT_STOP,
	THD_STAT_QUIT,
	THD_STAT_MAX,
} THD_STAT_E;


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
	pthread_t           wpid;
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
	pthread_cond_t cond;
	pthread_mutex_t mutex;
	Uint32_t gop_count;
	FRAMELIST_S *listhead;
} LISTINFO_S;

static char s_video_dir[CHN_COUNT][16] = {AVS_VIDEO_DIR, CH0_VIDEO_DIR, CH1_VIDEO_DIR, CH2_VIDEO_DIR, CH3_VIDEO_DIR};
static FRAMELIST_S *s_framelist_node[CHN_COUNT];
static int s_record_enable_flag = TRUE;
static int s_sd_card_mount = FALSE;
static p_record_thread_params_s s_p_record_thd_param;

static LISTINFO_S *s_framelist_info;
static FRAMELIST_S *s_framelist_head;

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

static int Is_the_frame_completed(Uint8_t *packet, CODEC_TYPE_E encode_type)
{
	NALU_TYPE_E nalu_type;
	int ret = UNCOMPLETE;
	nalu_type = get_the_nalu_type(packet + 4, encode_type);
	if (CODEC_H264 == encode_type)
	{
		switch (nalu_type)
		{
			case NALU_TYPE_H264_IDR:
				ret = COMPLETE;
				//printf("idr size:%d\n", size);
				break;
			case NALU_TYPE_H264_SPS:
				//printf("SPS size:%d\n", size);
				break;
			case NALU_TYPE_H264_PPS:
				//printf("PPS size:%d\n", size);
				break;
			case NALU_TYPE_H264_SEI:
				//printf("SEI size:%d\n", size);
				break;
			default:
				ret = COMPLETE;
				//printf("other size:%d, nalu_type:%d\n", size, nalu_type);
				break;
		}
	}
	else
	{
		switch (nalu_type)
		{
			case NALU_TYPE_H265_IDR_W_RADL:
				ret = COMPLETE;
				//printf("idr size:%d\n", size);
				break;
			case NALU_TYPE_H265_IDR_N_LP:
				ret = COMPLETE;
				//printf("idr size:%d\n", size);
				break;

			case NALU_TYPE_H265_VPS:
				//printf("VPS size:%d\n", size);
				//memset(s_packet[index], 0, PACKET_SIZE_MAX);
				break;
			case NALU_TYPE_H265_SPS:
				//printf("SPS size:%d\n", size);
				break;
			case NALU_TYPE_H265_PPS:
				//printf("PPS size:%d\n", size);
				break;
			case NALU_TYPE_H265_SEI_PREFIX:
				//printf("SEI size:%d\n", size);
				break;
			case NALU_TYPE_H265_SEI_SUFFIX:
				//printf("SEI size:%d\n", size);
				break;
			default:
				//memset(s_packet[index], 0, PACKET_SIZE_MAX);
				ret = COMPLETE;
				//printf("other size:%d, nalu_type:%d\n", size, nalu_type);
				break;
		}


	}

	return ret;
}

static int Is_the_Iframe(Uint8_t *packet, CODEC_TYPE_E encode_type)
{
	int ret = FALSE;
	NALU_TYPE_E nalu_type;

	nalu_type = get_the_nalu_type(packet + 4, encode_type);
	if ((CODEC_H264 == encode_type) && (NALU_TYPE_H264_IDR == nalu_type))
	{
		ret = TRUE;
	}
	else if ((CODEC_H265 == encode_type) && ((NALU_TYPE_H265_IDR_W_RADL == nalu_type) || (NALU_TYPE_H265_IDR_N_LP == nalu_type)))
	{
		ret = TRUE;
	}

	return ret;
}


static void record_get_mp4_filename(char *filename, int index)
{
	time_t time_seconds = time(NULL);  
	struct tm local_time;  
	localtime_r(&time_seconds, &local_time);  

	snprintf(filename, FILE_NAME_LEN_MAX, "%s%s/%d%02d%02d%02d%02d%02d.mp4", MOUNT_DIR, s_video_dir[index], local_time.tm_year + 1900, local_time.tm_mon + 1,  
		local_time.tm_mday, local_time.tm_hour, local_time.tm_min, local_time.tm_sec);

	return;
}


static void record_set_chn(int *chn, CODEC_TYPE_E encode_type)
{
	if (CODEC_H264 == encode_type)
	{
		chn[0] = 12;
		chn[1] = 4;
		chn[2] = 5;
		chn[3] = 6;
		chn[4] = 7;
	}
	else
	{
		chn[0] = 0;
		chn[1] = 0;
		chn[2] = 1;
		chn[3] = 2;
		chn[4] = 3;
	}
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
	//framelist_node->packetSize = 0;

	return framelist_node;
}

static int32_t record_add_gop(FRAMELIST_S *goplist_node)
{
	if ((NULL != s_framelist_info) && (NULL != s_framelist_head) && (NULL != goplist_node))
	{
		pthread_mutex_lock(&s_framelist_info->mutex);
		if (GOP_COUNT_MAX > s_framelist_info->gop_count)
		{
			list_add_tail(&goplist_node->list, &s_framelist_head->list);
			s_framelist_info->gop_count++;
			if (1 == s_framelist_info->gop_count)
			{
				pthread_cond_signal(&s_framelist_info->cond);
			}
		}
		else
		{
			printf("chn[%d]#####################drop frame,too many frame in list#############################\n", goplist_node->chn);
			free(goplist_node);
		}
		pthread_mutex_unlock(&s_framelist_info->mutex);
	}
	else
	{
		return ERROR;
	}

	return OK;
}

static void *record_get_frame_thread(void *p)
{
	int ret = 0;
	int fd_max = -1;
	int i = 0, j = 0, chn_num = 0;
	int chn[CHN_COUNT] = {0};
	int frame_count = 0;
	Uint8_t *packet = NULL;
	Uint32_t packetSize = PACKET_SIZE_MAX;
	Uint64_t pts = 0;
	Uint64_t last_pts = 0;
	int result = 0;
	Uint64_t start_time[CHN_COUNT] = {0};
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
start_record:
		pthread_mutex_lock(&s_p_record_thd_param->record_cond.mutex);
		while (THD_STAT_START != s_p_record_thd_param->thd_stat)
		{
			pthread_cond_wait(&s_p_record_thd_param->record_cond.cond, &s_p_record_thd_param->record_cond.mutex);
		}
		pthread_mutex_unlock(&s_p_record_thd_param->record_cond.mutex);

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
			for (i = 1; i < CHN_COUNT; i++)
			{
				videoEncoder.getFd(chn[i], fd[i]);
			}
		}
		/*
			1.to do 是否接入sd卡
			2.to do sd是否已进行过格式化
			3.失败 goto start_record，提醒用户进行相关操作并重试。
		*/

		if (FALSE == s_sd_card_mount)
		{
			if (ERROR == storage_mount_sdcard(MOUNT_DIR, DEV_NAME))
			{
				printf("record start failed\n");
				pthread_mutex_lock(&s_p_record_thd_param->record_cond.mutex);
				s_p_record_thd_param->record_cond.wake_success = FALSE;
				pthread_cond_signal(&s_p_record_thd_param->record_cond.cond);//通知录像启动失败
				s_p_record_thd_param->thd_stat = THD_STAT_STOP;
				pthread_mutex_lock(&s_p_record_thd_param->record_cond.mutex);
				continue;
			}
			s_sd_card_mount = TRUE;
		}

		sleep(s_p_record_thd_param->delay_time);
		timeout.tv_sec = 2; 	 
		timeout.tv_usec = 500000;
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
			fd_max = get_max_fd(&fd[1], CHN_COUNT - 1);
			for (i = 1; i < CHN_COUNT; i++)
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
			pthread_cond_signal(&s_p_record_thd_param->record_cond.cond);//通知录像已处于正常运行
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
									goto err;
								}
							}
							s_framelist_node[index]->chn = index;
							s_framelist_node[index]->entype = s_p_record_thd_param->entype;
						}
						framelist_node = s_framelist_node[index];
						packet = framelist_node->frame + framelist_node->gopsize;
						packetSize = PACKET_SIZE_MAX - framelist_node->gopsize;
						if (-1 == videoEncoder.getStream(chn[index], packet, packetSize, pts))
						{
							record_add_gop(framelist_node);
							s_framelist_node[index] = NULL;
							break;
						}
						//printf("chn:%d, packet size:%d\n", fd_found[i], packetSize);
						framelist_node->gopsize += packetSize;
						if (UNCOMPLETE == Is_the_frame_completed(packet, framelist_node->entype))//组合第一帧数据
						{
							break;
						}

						if (Is_the_Iframe(packet, framelist_node->entype) || ((FRAME_COUNT_MAX - 1) <= framelist_node->frame_count))
						{
							if (0 == start_time[index])
							{
								start_time[index] = pts;
								framelist_node->first = TRUE;//文件的第一组gop，新建MP4文件
							}
							if (s_p_record_thd_param->divide_time <= (pts -start_time[index]))//分时功能
							{
								framelist_node->last = TRUE;//最后一组gop，关闭MP4文件
								start_time[index] = 0;
							}
							record_add_gop(framelist_node);
							s_framelist_node[index] = NULL;//处理完数据指针置为NULL
						}

						frame_count = framelist_node->frame_count++;
						framelist_node->pts[frame_count] = pts/1000;
						framelist_node->packetSize[frame_count] = packetSize;
					}
					break;
				}
err:

				if (THD_STAT_STOP == s_p_record_thd_param->thd_stat)//录像结束关闭MP4文件
				{
					if (RECORD_MODE_SINGLE == s_p_record_thd_param->record_mode)
					{
						if (NULL != s_framelist_node[0])
						{
							videoEncoder.stopRecvStream(chn[0]);
							s_framelist_node[0]->last = TRUE;
							s_framelist_node[0]->first = FALSE;
							record_add_gop(s_framelist_node[0]);
							s_framelist_node[0] = NULL;
							start_time[0] = 0;
						}
					}
					else
					{
						for (i = 1; i < CHN_COUNT; i++)
						{
							if (NULL != s_framelist_node[i])
							{
								videoEncoder.stopRecvStream(chn[i]);
								s_framelist_node[i]->last = TRUE;
								s_framelist_node[i]->first = FALSE;
								record_add_gop(s_framelist_node[i]);
								s_framelist_node[i] = NULL;
								start_time[i] = 0;
							}
						}
					}
					pthread_mutex_lock(&s_p_record_thd_param->record_cond.mutex);
					if (TRUE == s_p_record_thd_param->record_cond.wake)
					{
						s_p_record_thd_param->record_cond.wake = FALSE;
						pthread_cond_signal(&s_p_record_thd_param->record_cond.cond);//通知录像已关闭
					}
					pthread_mutex_unlock(&s_p_record_thd_param->record_cond.mutex);

					break;
				}

		}

	}
#endif

	return (void *)OK;

}

static void *record_write_mp4_thread(void *p)
{
	int ret = OK;
	int chn = -1;
	int state = 0;
	int i = 0;
	int offset = 0;

	Mp4Encoder *mp4_encoder = new Mp4Encoder[CHN_COUNT];
	char filename[FILE_NAME_LEN_MAX];
	memset(filename, 0, FILE_NAME_LEN_MAX);
	FRAMELIST_S *framelist_node = NULL;

	VideoParamter video_param = {AV_CODEC_ID_H264, 3840, 2160};
	AudioParamter audio_param = {AV_CODEC_ID_AAC};

	while (s_record_enable_flag)
	{
		if (0 == s_framelist_info->gop_count)
		{
			pthread_mutex_lock(&s_framelist_info->mutex);
			if (0 == s_framelist_info->gop_count)
			{
				do
				{	
					pthread_cond_wait(&s_framelist_info->cond, &s_framelist_info->mutex);
					if((FALSE == s_record_enable_flag)
						|| ((THD_STAT_STOP == s_p_record_thd_param->thd_stat) && (0 == s_framelist_info->gop_count)))
					{
						pthread_mutex_unlock(&s_framelist_info->mutex);
						goto exit;//退出线程
					}
						
				}while(0 == s_framelist_info->gop_count);
			}
			pthread_mutex_unlock(&s_framelist_info->mutex);
		}

		framelist_node = list_entry(s_framelist_head->list.next, FRAMELIST_S, list);
		chn = framelist_node->chn;
		if (TRUE == framelist_node->first)
		{
			record_get_mp4_filename(filename, chn);
			if (!mp4_encoder[chn].Init(filename, video_param, audio_param))
			{
				DBG_INFO("Mp4 init failed\n");
				return (void *)-1;
			}
		}
		for (i = 0, offset = 0; i < framelist_node->frame_count; i++)
		{
			if (0 != (mp4_encoder[chn].WriteOneFrame(MEDIA_TYPE_VIDEO,(char*)framelist_node->frame + offset, framelist_node->packetSize[i], framelist_node->pts[i])))
			{
				DBG_INFO("Write video frame failed\n");
			}
			offset += framelist_node->packetSize[i];
		}
		if (TRUE == framelist_node->last)
		{
			mp4_encoder[chn].Close();
		}
		pthread_mutex_lock(&s_framelist_info->mutex);
		list_del_init(&framelist_node->list);
		s_framelist_info->gop_count--;
		free(framelist_node);
		pthread_mutex_unlock(&s_framelist_info->mutex);

	}

exit:
	for (chn = 0; chn < CHN_COUNT; chn++)
	{
		mp4_encoder[chn].GetFileState(state);
		if (1 == state)//open 状态需关闭
		{
			mp4_encoder[chn].Close();
		}
	}
	delete []mp4_encoder;
	return (void *)OK;
}

static int record_framelist_init(void)
{
	s_framelist_info = (LISTINFO_S *)malloc(sizeof(LISTINFO_S));
	if (NULL == s_framelist_info)
	{
		perror("s_fileinfo malloc failed:");
		printf("filelist_init failed\n");
		return ERROR;
	}
	s_framelist_info->gop_count = 0;
	s_framelist_info->mutex = PTHREAD_MUTEX_INITIALIZER;
	s_framelist_info->cond = PTHREAD_COND_INITIALIZER;
	s_framelist_info->listhead = (FRAMELIST_S *)malloc(sizeof(FRAMELIST_S));
	if (NULL == s_framelist_info->listhead)
	{
		perror("s_fileinfo->head malloc failed:");
		printf("filelist_init failed\n");
		return ERROR;
	}
	INIT_LIST_HEAD(&s_framelist_info->listhead->list);
	s_framelist_head = s_framelist_info->listhead;

	return OK;
}


static int record_thread_create(void)
{
	pthread_attr_t attr1,attr2;
	struct sched_param param;

	if (OK != record_framelist_init())
	{
		printf("pthread_create get frame thread failed\n");
		return ERROR;
	}

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


	if (0 != pthread_create(&s_p_record_thd_param->gpid, &attr1, record_get_frame_thread, NULL))
	{
		perror("pthread_create failed");
		printf("pthread_create get frame thread failed\n");
		return ERROR;
	}
	pthread_setname_np(s_p_record_thd_param->gpid, "recg\0");
	if (0 != pthread_create(&s_p_record_thd_param->wpid, &attr2, record_write_mp4_thread, NULL))
	{
		perror("pthread_create failed");
		printf("pthread_create get frame thread failed\n");
		return ERROR;
	}
	pthread_setname_np(s_p_record_thd_param->wpid, "recw\0");
	s_p_record_thd_param->thd_stat = THD_STAT_IDLE;

	return OK;

}

void record_thread_stop(void)
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
}

void record_thread_start(void)
{
	pthread_mutex_lock(&s_p_record_thd_param->mutex);

	pthread_mutex_lock(&s_p_record_thd_param->record_cond.mutex);
	if (THD_STAT_START != s_p_record_thd_param->thd_stat)
	{
		s_p_record_thd_param->thd_stat = THD_STAT_START;
		pthread_cond_signal(&s_p_record_thd_param->record_cond.cond);
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
	pthread_mutex_unlock(&s_p_record_thd_param->record_cond.mutex);

	pthread_mutex_unlock(&s_p_record_thd_param->mutex);
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

static int record_param_init(void)
{
	s_p_record_thd_param = (p_record_thread_params_s)malloc(sizeof(RECORD_THREAD_PARAMS_S));
	if (NULL == s_p_record_thd_param)
	{
		perror("gs_record_thd_param malloc failed:");
		printf("record_param_init failed\n");
		return ERROR;
	}
	s_p_record_thd_param->delay_time = 0;
	s_p_record_thd_param->divide_time = RECORD_DIVIDE_TIME;
	s_p_record_thd_param->gpid = PID_NULL;
	s_p_record_thd_param->wpid = PID_NULL;
	s_p_record_thd_param->thd_stat = THD_STAT_IDLE;
	s_p_record_thd_param->stream_t = MEDIA_TYPE_VIDEO;
	s_p_record_thd_param->entype = CODEC_H264;
	s_p_record_thd_param->record_mode = RECORD_MODE_MULTI;
	pthread_mutex_init(&s_p_record_thd_param->mutex, NULL);
	record_cond_init(&s_p_record_thd_param->record_cond);

	//sem_init(&(s_p_record_thd_param->wake_sem),0,0);//第二个如果不为0代表进程间共享sem，第三个代表sem值的初始值
	return OK;
}


static int record_param_exit(void)
{
	record_cond_deinit(&s_p_record_thd_param->record_cond);
	pthread_mutex_destroy(&s_p_record_thd_param->mutex);
	if (NULL != s_p_record_thd_param)
	{
		free(s_p_record_thd_param);
	}

	return OK;
}

static int record_framelist_destroy(void)
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
		pthread_cond_destroy(&s_framelist_info->cond);
		free(s_framelist_info);
		s_framelist_info = NULL;
	}
	
	return 0;
}


int record_thread_destroy(void)
{
	s_record_enable_flag = FALSE;
	record_thread_start();
	s_p_record_thd_param->thd_stat = THD_STAT_STOP;
	if (s_p_record_thd_param->gpid != PID_NULL)
	{
		pthread_join(s_p_record_thd_param->gpid, 0);
		s_p_record_thd_param->gpid = PID_NULL;
	}
	if (s_p_record_thd_param->wpid != PID_NULL)
	{
		pthread_join(s_p_record_thd_param->wpid, 0);
		s_p_record_thd_param->wpid = PID_NULL;
	}

	if (TRUE == s_sd_card_mount)
	{
		if (OK == storage_umount_sdcard(MOUNT_DIR))
		{
			s_sd_card_mount = FALSE;
		}
	}

	record_framelist_destroy();

	return OK;
}

int record_module_init(void)
{
	record_param_init();
	record_thread_create();

	return OK;
}

int record_module_exit(void)
{
	record_thread_destroy();
	record_param_exit();
	return OK;
}

/*
延时录像设置
*/
int record_set_delay_time(int time)
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
int record_set_divide_time(void)
{
	//暂无需求
}


/*
录像模式设置
*/
int record_set_record_mode(RECORD_MODE_E mode)
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
		}
		s_p_record_thd_param->record_mode = mode;
	
		if (THD_STAT_START != s_p_record_thd_param->thd_stat)
		{
			
			s_p_record_thd_param->thd_stat = THD_STAT_START;
			pthread_cond_signal(&s_p_record_thd_param->record_cond.cond);
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
录像重启
*/
void record_thread_restart(void)
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
	if (THD_STAT_START != s_p_record_thd_param->thd_stat)
	{
		s_p_record_thd_param->thd_stat = THD_STAT_START;
		pthread_cond_signal(&s_p_record_thd_param->record_cond.cond);
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
	pthread_mutex_unlock(&s_p_record_thd_param->record_cond.mutex);

	pthread_mutex_unlock(&s_p_record_thd_param->mutex);
}


/*
编码类型设置
*/
int record_set_encode_type(CODEC_TYPE_E entype)
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
		}
		s_p_record_thd_param->entype = entype;
	
		if (THD_STAT_START != s_p_record_thd_param->thd_stat)
		{
			
			s_p_record_thd_param->thd_stat = THD_STAT_START;
			pthread_cond_signal(&s_p_record_thd_param->record_cond.cond);
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
		pthread_mutex_unlock(&s_p_record_thd_param->record_cond.mutex);
	}
	else
	{
		printf("already the entype :%d\n", entype);
	}
	
	pthread_mutex_unlock(&s_p_record_thd_param->mutex);

	return OK;
}
