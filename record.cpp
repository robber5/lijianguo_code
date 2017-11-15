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
#include "Mp4Encoder.h"
#include "record.h"
#include "storage.h"


using namespace detu_media;
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

typedef enum
{
	STREAM_TYPE_NONE,
	STREAM_TYPE_AUDIO,
	STREAM_TYPE_VIDEO,
	STREAM_TYPE_MAX,
} STREAM_TYPE_E;


typedef enum
{
	THD_STAT_IDLE,
	THD_STAT_START,
	THD_STAT_STOP,
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
	pthread_t           pid;
	volatile THD_STAT_E thd_stat;
	RECORD_COND_S		record_cond;//条件变量，录像控制
	pthread_mutex_t     mutex;//命令锁，保证同时只执行一条命令
	STREAM_TYPE_E       stream_t; //音视频
	CODEC_TYPE_E		entype;//编码类型
	RECORD_MODE_E		record_mode;//录像模式

} RECORD_THREAD_PARAMS_S, *p_record_thread_params_s;


static int record_enable_flag = TRUE;
static int sd_card_mount = FALSE;
static p_record_thread_params_s p_gs_record_thd_param;

#define RECORD_DIVIDE_TIME (20*60*1000*1000) //second
#define PID_NULL			((pid_t)(-1))
#define PACKET_SIZE_MAX (3*1024*1024)


#define CHN_COUNT 5

static char s_video_dir[CHN_COUNT][16] = {AVS_VIDEO_DIR, CH0_VIDEO_DIR, CH1_VIDEO_DIR, CH2_VIDEO_DIR, CH3_VIDEO_DIR};

static Uint8_t g_packet[CHN_COUNT][PACKET_SIZE_MAX];
static int g_packet_size[CHN_COUNT];

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

static int combine_the_first_frame(Uint8_t *packet, int size, CODEC_TYPE_E encode_type, int index)
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
				printf("idr size:%d\n", size);
				break;
			case NALU_TYPE_H264_SPS:
				printf("SPS size:%d\n", size);
				memset(g_packet[index], 0, PACKET_SIZE_MAX);
				g_packet_size[index] = 0;
				break;
			case NALU_TYPE_H264_PPS:
				printf("PPS size:%d\n", size);
				break;
			case NALU_TYPE_H264_SEI:
				printf("SEI size:%d\n", size);
				break;
			default:
				memset(g_packet[index], 0, PACKET_SIZE_MAX);
				g_packet_size[index] = 0;
				ret = COMPLETE;
				printf("other size:%d, nalu_type:%d\n", size, nalu_type);
				break;
		}
	}
	else
	{
		switch (nalu_type)
		{
			case NALU_TYPE_H265_IDR_W_RADL:
				ret = COMPLETE;
				printf("idr size:%d\n", size);
				break;
			case NALU_TYPE_H265_IDR_N_LP:
				ret = COMPLETE;
				printf("idr size:%d\n", size);
				break;

			case NALU_TYPE_H265_VPS:
				printf("VPS size:%d\n", size);
				memset(g_packet[index], 0, PACKET_SIZE_MAX);
				g_packet_size[index] = 0;
				break;
			case NALU_TYPE_H265_SPS:
				printf("SPS size:%d\n", size);
				break;
			case NALU_TYPE_H265_PPS:
				printf("PPS size:%d\n", size);
				break;
			case NALU_TYPE_H265_SEI_PREFIX:
				printf("SEI size:%d\n", size);
				break;
			case NALU_TYPE_H265_SEI_SUFFIX:
				printf("SEI size:%d\n", size);
				break;
			default:
				memset(g_packet[index], 0, PACKET_SIZE_MAX);
				g_packet_size[index] = 0;
				ret = COMPLETE;
				printf("other size:%d, nalu_type:%d\n", size, nalu_type);
				break;
		}


	}
	if (PACKET_SIZE_MAX >= (g_packet_size[index] + size))
	{
		memcpy(g_packet[index] + g_packet_size[index], packet, size);
		g_packet_size[index] += size;
	}
	else
	{
		return COMPLETE;
	}
	return ret;
}


static void record_get_mp4_filename(wchar_t *filename, int index)
{
	time_t time_seconds = time(NULL);  
	struct tm local_time;  
	localtime_r(&time_seconds, &local_time);  

	swprintf(filename, FILE_NAME_LEN_MAX, L"%s%s/%d%02d%02d%02d%02d%02d.mp4", MOUNT_DIR, s_video_dir[index], local_time.tm_year + 1900, local_time.tm_mon + 1,  
		local_time.tm_mday, local_time.tm_hour, local_time.tm_min, local_time.tm_sec);

	return;
}


void record_set_chn(int *chn, CODEC_TYPE_E encode_type)
{
	if (CODEC_H264 == encode_type)
	{
		chn[0] = 4;
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


int get_max_fd(int *fd, int num)
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


void *record_thread(void *p)
{
	int ret = 0;
	int fd_max = -1;
	int i = 0;
	Mp4Encoder *mp4Encoder = new Mp4Encoder[CHN_COUNT];
	int chn[CHN_COUNT] = {0};
	Uint8_t *packet = NULL;
	Uint32_t packetSize = PACKET_SIZE_MAX;
	Uint64_t pts = 0;
	int result = 0;
	Uint64_t start_time[CHN_COUNT] = {0};
	fd_set inputs, testfds;
	struct timeval timeout;
	int fd[CHN_COUNT];
	wchar_t filename[FILE_NAME_LEN_MAX];
	wmemset(filename, 0, FILE_NAME_LEN_MAX);
	VideoEncodeMgr& videoEncoder = *VideoEncodeMgr::instance();

	packet = (Uint8_t *)malloc(PACKET_SIZE_MAX);
	memset(packet, 0, PACKET_SIZE_MAX);
#if 1
	while (record_enable_flag)
	{
start_record:
		pthread_mutex_lock(&p_gs_record_thd_param->record_cond.mutex);
		while (THD_STAT_START != p_gs_record_thd_param->thd_stat)
		{
			pthread_cond_wait(&p_gs_record_thd_param->record_cond.cond, &p_gs_record_thd_param->record_cond.mutex);
		}
		pthread_mutex_unlock(&p_gs_record_thd_param->record_cond.mutex);

		record_set_chn(chn, p_gs_record_thd_param->entype);

		if (RECORD_MODE_SINGLE == p_gs_record_thd_param->record_mode)
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

		if (FALSE == sd_card_mount)
		{
			if (ERROR == storage_mount_sdcard(MOUNT_DIR, DEV_NAME))
			{
				printf("record start failed\n");
				pthread_mutex_lock(&p_gs_record_thd_param->record_cond.mutex);
				p_gs_record_thd_param->record_cond.wake_success = FALSE;
				pthread_cond_signal(&p_gs_record_thd_param->record_cond.cond);//通知录像启动失败
				p_gs_record_thd_param->thd_stat = THD_STAT_STOP;
				pthread_mutex_lock(&p_gs_record_thd_param->record_cond.mutex);
				continue;
			}
			sd_card_mount = TRUE;
		}

		sleep(p_gs_record_thd_param->delay_time);
		if (RECORD_MODE_MULTI == p_gs_record_thd_param->record_mode)
		{
			for (i = 1; i < CHN_COUNT; i++)
			{
				videoEncoder.startRecvStream(chn[i]);
				mp4Encoder[i].setVStreamType(p_gs_record_thd_param->entype);
			}
		}
		else
		{
			videoEncoder.startRecvStream(chn[0]);
			mp4Encoder[0].setVStreamType(p_gs_record_thd_param->entype);
		}

		while (THD_STAT_START == p_gs_record_thd_param->thd_stat)
		{
			packetSize = PACKET_SIZE_MAX;
			memset(packet, 0, packetSize);
			//FD_ZERO(&testfds);
			testfds = inputs;
			timeout.tv_sec = 2;      
			timeout.tv_usec = 500000; 
			FD_ZERO(&inputs);//用select函数之前先把集合清零
			if (RECORD_MODE_SINGLE == p_gs_record_thd_param->record_mode)
			{
				FD_SET(fd[0],&inputs);//把要监测的句柄——fd,加入到集合里
				fd_max = fd[0];
			}
			else
			{
				fd_max = get_max_fd(&fd[1], 4);
				FD_SET(fd[1],&inputs);
				FD_SET(fd[2],&inputs);
				FD_SET(fd[3],&inputs);
				FD_SET(fd[4],&inputs);
			}
			pthread_mutex_lock(&p_gs_record_thd_param->record_cond.mutex);
			if (FALSE == p_gs_record_thd_param->record_cond.wake)
			{
				p_gs_record_thd_param->record_cond.wake = TRUE;
				pthread_cond_signal(&p_gs_record_thd_param->record_cond.cond);//通知录像已处于正常运行
			}
			pthread_mutex_unlock(&p_gs_record_thd_param->record_cond.mutex);
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
					for (i = 0; i < CHN_COUNT; i++)
					{
						if (FD_ISSET(fd[i],&testfds))
						{
							break;
						}
					}
					if (CHN_COUNT <= i)
					{
						printf("not found available fd");
						break;
					}
				
					videoEncoder.getStream(chn[i], packet, packetSize, pts);
					if (UNCOMPLETE == combine_the_first_frame(packet, packetSize, p_gs_record_thd_param->entype, i))//组合第一帧数据
					{
						break;
					}
					if (0 == start_time[i])
					{
						start_time[i] = pts;
						record_get_mp4_filename(filename, i);
						mp4Encoder[i].setFileName(filename);
						mp4Encoder[i].openMp4Encoder();
						if (ERROR == mp4Encoder[i].writeVideoFrame(g_packet[i], g_packet_size[i], (Uint32_t)(pts/1000)))
						{
							printf("write video failed!\n");
						}
					}
					else if (p_gs_record_thd_param->divide_time <= (pts -start_time[i]))//分时功能
					{
						videoEncoder.stopRecvStream(chn[i]);
						mp4Encoder[i].closeMp4Encoder();
						printf("create new file\n");
						videoEncoder.startRecvStream(chn[i]);
						start_time[i] = pts;
						record_get_mp4_filename(filename, i);
						mp4Encoder[i].setFileName(filename);
						mp4Encoder[i].openMp4Encoder();
						if (ERROR == mp4Encoder[i].writeVideoFrame(g_packet[i], g_packet_size[i], (Uint32_t)(pts/1000)))
						{
							printf("write video failed!\n");
						}
					}
					else
					{
						if (ERROR == mp4Encoder[i].writeVideoFrame(g_packet[i], g_packet_size[i], (Uint32_t)(pts/1000)))
						{
							printf("write video failed!\n");
						}

					}
					memset(g_packet[i], 0, PACKET_SIZE_MAX);
					break;
				}
				if (THD_STAT_STOP == p_gs_record_thd_param->thd_stat)//录像结束关闭MP4文件
				{
					if (RECORD_MODE_SINGLE == p_gs_record_thd_param->record_mode)
					{
						videoEncoder.stopRecvStream(chn[0]);
						mp4Encoder[0].closeMp4Encoder();
						start_time[0] = 0;
					}
					else
					{
						for (i = 1; i < CHN_COUNT; i++)
						{
							videoEncoder.stopRecvStream(chn[i]);
							mp4Encoder[i].closeMp4Encoder();
							start_time[i] = 0;
						}
					}
					pthread_mutex_lock(&p_gs_record_thd_param->record_cond.mutex);
					if (TRUE == p_gs_record_thd_param->record_cond.wake)
					{
						p_gs_record_thd_param->record_cond.wake = FALSE;
						pthread_cond_signal(&p_gs_record_thd_param->record_cond.cond);//通知录像已关闭
					}
					pthread_mutex_unlock(&p_gs_record_thd_param->record_cond.mutex);

					break;
				}

		}
err:
		if (THD_STAT_STOP == p_gs_record_thd_param->thd_stat)
		{
			pthread_mutex_lock(&p_gs_record_thd_param->record_cond.mutex);
			if (TRUE == p_gs_record_thd_param->record_cond.wake)
			{
				p_gs_record_thd_param->record_cond.wake = FALSE;
				pthread_cond_signal(&p_gs_record_thd_param->record_cond.cond);//通知录像已关闭
			}
			pthread_mutex_unlock(&p_gs_record_thd_param->record_cond.mutex);
		}

	}
#endif
	delete []mp4Encoder;
	return OK;

}

int record_thread_create()
{

	if (0 != pthread_create(&p_gs_record_thd_param->pid, NULL, record_thread, NULL))
	{
		perror("pthread_create failed:");
		printf("pthread_create get buffer thread failed\n");
		return ERROR;
	}
	pthread_setname_np(p_gs_record_thd_param->pid, "recg\0");
	p_gs_record_thd_param->thd_stat = THD_STAT_IDLE;

	return OK;

}

void record_thread_stop()
{
	pthread_mutex_lock(&p_gs_record_thd_param->mutex);

	pthread_mutex_lock(&p_gs_record_thd_param->record_cond.mutex);
	if (THD_STAT_START == p_gs_record_thd_param->thd_stat)
	{
		p_gs_record_thd_param->thd_stat = THD_STAT_STOP;
		do
		{
			pthread_cond_wait(&p_gs_record_thd_param->record_cond.cond, &p_gs_record_thd_param->record_cond.mutex);//确保在录像停止之前不接受其他命令
		}while(TRUE == p_gs_record_thd_param->record_cond.wake); //等待wake置为false时退出
	}
	pthread_mutex_unlock(&p_gs_record_thd_param->record_cond.mutex);

	pthread_mutex_unlock(&p_gs_record_thd_param->mutex);
}

void record_thread_start()
{
	pthread_mutex_lock(&p_gs_record_thd_param->mutex);

	pthread_mutex_lock(&p_gs_record_thd_param->record_cond.mutex);
	if (THD_STAT_START != p_gs_record_thd_param->thd_stat)
	{
		p_gs_record_thd_param->thd_stat = THD_STAT_START;
		pthread_cond_signal(&p_gs_record_thd_param->record_cond.cond);
		do
		{
			pthread_cond_wait(&p_gs_record_thd_param->record_cond.cond, &p_gs_record_thd_param->record_cond.mutex);//确保在录像正常运行之前不接受其他命令
			if (FALSE == p_gs_record_thd_param->record_cond.wake_success)
			{
				p_gs_record_thd_param->record_cond.wake_success = TRUE;//置为初始值
				break;//启动失败退出
			}
		}while(FALSE == p_gs_record_thd_param->record_cond.wake);
	}
	pthread_mutex_unlock(&p_gs_record_thd_param->record_cond.mutex);

	pthread_mutex_unlock(&p_gs_record_thd_param->mutex);
}

void record_cond_init(RECORD_COND_S *record_cond)
{
	record_cond->mutex = PTHREAD_MUTEX_INITIALIZER; //初始化互斥锁
	record_cond->cond = PTHREAD_COND_INITIALIZER; //初始化条件变量
	record_cond->wake = FALSE;
	record_cond->wake_success = TRUE; //默认唤醒操作可以执行成功,发生失败时置为FALSE
}

void record_cond_deinit(RECORD_COND_S *record_cond)
{
	pthread_mutex_destroy(&record_cond->mutex);
	pthread_cond_destroy(&record_cond->cond);
}

int record_param_init()
{
	p_gs_record_thd_param = (p_record_thread_params_s)malloc(sizeof(RECORD_THREAD_PARAMS_S));
	if (NULL == p_gs_record_thd_param)
	{
		perror("gs_record_thd_param malloc failed:");
		printf("record_param_init failed\n");
		return ERROR;
	}
	p_gs_record_thd_param->delay_time = 0;
	p_gs_record_thd_param->divide_time = RECORD_DIVIDE_TIME;
	p_gs_record_thd_param->pid = PID_NULL;
	p_gs_record_thd_param->thd_stat = THD_STAT_IDLE;
	p_gs_record_thd_param->stream_t = STREAM_TYPE_NONE;
	p_gs_record_thd_param->entype = CODEC_H264;
	p_gs_record_thd_param->record_mode = RECORD_MODE_MULTI;
	pthread_mutex_init(&p_gs_record_thd_param->mutex, NULL);
	record_cond_init(&p_gs_record_thd_param->record_cond);

	//sem_init(&(p_gs_record_thd_param->wake_sem),0,0);//第二个如果不为0代表进程间共享sem，第三个代表sem值的初始值
	return OK;
}


int record_param_exit()
{
	record_cond_deinit(&p_gs_record_thd_param->record_cond);
	pthread_mutex_destroy(&p_gs_record_thd_param->mutex);
	if (NULL != p_gs_record_thd_param)
	{
		free(p_gs_record_thd_param);
	}

	return OK;
}

int record_thread_destroy()
{
	record_enable_flag = FALSE;
	record_thread_start();
	record_thread_stop();
	if (TRUE == sd_card_mount)
	{
		if (OK == storage_umount_sdcard(MOUNT_DIR))
		{
			sd_card_mount = FALSE;
		}
	}
	if (p_gs_record_thd_param->pid != PID_NULL)
	{
		pthread_join(p_gs_record_thd_param->pid, 0);
		p_gs_record_thd_param->pid = PID_NULL;
	}

	return OK;
}

int record_module_init()
{
	record_param_init();
	record_thread_create();

	return OK;
}

int record_module_exit()
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
	pthread_mutex_lock(&p_gs_record_thd_param->mutex);
	p_gs_record_thd_param->delay_time = time;
	pthread_mutex_unlock(&p_gs_record_thd_param->mutex);
	return OK;
}

/*
分时录像设置
*/
int record_set_divide_time()
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
	pthread_mutex_lock(&p_gs_record_thd_param->mutex);

	if (mode != p_gs_record_thd_param->record_mode)
	{
		pthread_mutex_lock(&p_gs_record_thd_param->record_cond.mutex);
		if (THD_STAT_START == p_gs_record_thd_param->thd_stat)
		{
			p_gs_record_thd_param->thd_stat = THD_STAT_STOP;
			do
			{
				pthread_cond_wait(&p_gs_record_thd_param->record_cond.cond, &p_gs_record_thd_param->record_cond.mutex);//确保在录像停止之前不接受其他命令
			}while(TRUE == p_gs_record_thd_param->record_cond.wake); //等待wake置为false时退出
		}
		p_gs_record_thd_param->record_mode = mode;
	
		if (THD_STAT_START != p_gs_record_thd_param->thd_stat)
		{
			
			p_gs_record_thd_param->thd_stat = THD_STAT_START;
			pthread_cond_signal(&p_gs_record_thd_param->record_cond.cond);
			do
			{
				pthread_cond_wait(&p_gs_record_thd_param->record_cond.cond, &p_gs_record_thd_param->record_cond.mutex);//确保在录像正常运行之前不接受其他命令
				if (FALSE == p_gs_record_thd_param->record_cond.wake_success)
				{
					p_gs_record_thd_param->record_cond.wake_success = TRUE;//置为初始值
					break;//启动失败退出
				}
			}while(FALSE == p_gs_record_thd_param->record_cond.wake);
		}
		pthread_mutex_unlock(&p_gs_record_thd_param->record_cond.mutex);
	}
	else
	{
		printf("already the mode :%d\n", mode);
	}
	
	pthread_mutex_unlock(&p_gs_record_thd_param->mutex);

	return OK;
}


/*
录像重启
*/
void record_thread_restart()
{
	pthread_mutex_lock(&p_gs_record_thd_param->mutex);

	pthread_mutex_lock(&p_gs_record_thd_param->record_cond.mutex);
	if (THD_STAT_START == p_gs_record_thd_param->thd_stat)
	{
		p_gs_record_thd_param->thd_stat = THD_STAT_STOP;
		do
		{
			pthread_cond_wait(&p_gs_record_thd_param->record_cond.cond, &p_gs_record_thd_param->record_cond.mutex);//确保在录像停止之前不接受其他命令
		}while(TRUE == p_gs_record_thd_param->record_cond.wake); //等待wake置为false时退出
	}
	if (THD_STAT_START != p_gs_record_thd_param->thd_stat)
	{
		p_gs_record_thd_param->thd_stat = THD_STAT_START;
		pthread_cond_signal(&p_gs_record_thd_param->record_cond.cond);
		do
		{
			pthread_cond_wait(&p_gs_record_thd_param->record_cond.cond, &p_gs_record_thd_param->record_cond.mutex);//确保在录像正常运行之前不接受其他命令
			if (FALSE == p_gs_record_thd_param->record_cond.wake_success)
			{
				p_gs_record_thd_param->record_cond.wake_success = TRUE;//置为初始值
				break;//启动失败退出
			}
		}while(FALSE == p_gs_record_thd_param->record_cond.wake);
	}
	pthread_mutex_unlock(&p_gs_record_thd_param->record_cond.mutex);

	pthread_mutex_unlock(&p_gs_record_thd_param->mutex);
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
	pthread_mutex_lock(&p_gs_record_thd_param->mutex);

	if (entype != p_gs_record_thd_param->entype)
	{
		pthread_mutex_lock(&p_gs_record_thd_param->record_cond.mutex);
		if (THD_STAT_START == p_gs_record_thd_param->thd_stat)
		{
			p_gs_record_thd_param->thd_stat = THD_STAT_STOP;
			do
			{
				pthread_cond_wait(&p_gs_record_thd_param->record_cond.cond, &p_gs_record_thd_param->record_cond.mutex);//确保在录像停止之前不接受其他命令
			}while(TRUE == p_gs_record_thd_param->record_cond.wake); //等待wake置为false时退出
		}
		p_gs_record_thd_param->entype = entype;
	
		if (THD_STAT_START != p_gs_record_thd_param->thd_stat)
		{
			
			p_gs_record_thd_param->thd_stat = THD_STAT_START;
			pthread_cond_signal(&p_gs_record_thd_param->record_cond.cond);
			do
			{
				pthread_cond_wait(&p_gs_record_thd_param->record_cond.cond, &p_gs_record_thd_param->record_cond.mutex);//确保在录像正常运行之前不接受其他命令
				if (FALSE == p_gs_record_thd_param->record_cond.wake_success)
				{
					p_gs_record_thd_param->record_cond.wake_success = TRUE;//置为初始值
					break;//启动失败退出
				}
			}while(FALSE == p_gs_record_thd_param->record_cond.wake);
		}
		pthread_mutex_unlock(&p_gs_record_thd_param->record_cond.mutex);
	}
	else
	{
		printf("already the entype :%d\n", entype);
	}
	
	pthread_mutex_unlock(&p_gs_record_thd_param->mutex);

	return OK;
}
