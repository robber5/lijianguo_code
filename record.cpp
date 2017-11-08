#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>

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
#define ERROR (-1)
#define OK 0
#define TRUE 1
#define FALSE 0
#define UNCOMPLETE 0
#define COMPLETE 1

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
	CODEC_H264 = 96,
	CODEC_H265 = 265,

} CODEC_TYPE_E;

typedef enum
{
	NALU_TYPE_DEFAULT = 0,
	NALU_TYPE_IDR = 5,
	NALU_TYPE_SEI = 6,
	NALU_TYPE_SPS = 7,
	NALU_TYPE_PPS = 8,
} NALU_TYPE_E;

typedef struct record_thread_params
{
	unsigned int delay_time; //延时录像时间
	unsigned int divide_time; //分时录像时间
	pthread_t           pid;
	volatile THD_STAT_E thd_stat;
	sem_t               wake_sem;//录像开启关闭信号量
	pthread_mutex_t     mutex;
	STREAM_TYPE_E       stream_t; //音视频

} RECORD_THREAD_PARAMS_S, *p_record_thread_params_s;


static int record_enable_flag = TRUE;
static int sd_card_mount = FALSE;
static p_record_thread_params_s p_gs_record_thd_param;

#define RECORD_DIVIDE_TIME (20*60*1000*1000) //second
#define PID_NULL			((pid_t)(-1))
#define PACKET_SIZE_MAX (1024*1024)

static Uint8_t g_packet[PACKET_SIZE_MAX];
static int g_packet_size;

static NALU_TYPE_E get_the_nalu_type(Uint8_t *packet, CODEC_TYPE_E encode_type)
{
	if (CODEC_H264 == encode_type)
	{
		return (NALU_TYPE_E)(packet[0] & 0x1F);
	}
	else
	{
	}
	return NALU_TYPE_DEFAULT;
}

static int combine_the_first_frame(Uint8_t *packet, int size, CODEC_TYPE_E encode_type)
{
	NALU_TYPE_E nalu_type;
	int ret = UNCOMPLETE;
	nalu_type = get_the_nalu_type(packet + 4, encode_type);
	switch (nalu_type)
	{
		case NALU_TYPE_IDR:
			ret = COMPLETE;
			printf("idr size:%d\n", size);
			break;
		case NALU_TYPE_SPS:
			printf("SPS size:%d\n", size);
			memset(g_packet, 0, PACKET_SIZE_MAX);
			g_packet_size = 0;
			break;
		case NALU_TYPE_PPS:
			printf("PPS size:%d\n", size);
			break;
		case NALU_TYPE_SEI:
			printf("SEI size:%d\n", size);
			break;
		default:
			memset(g_packet, 0, PACKET_SIZE_MAX);
			g_packet_size = 0;
			ret = COMPLETE;
			printf("other size:%d, nalu_type:%d\n", size, nalu_type);
			break;
	}
	if (PACKET_SIZE_MAX >= (g_packet_size + size))
	{
		memcpy(g_packet + g_packet_size, packet, size);
		g_packet_size += size;
	}
	else
	{
		return COMPLETE;
	}
	return ret;
}


static void record_get_mp4_filename(wchar_t *filename)
{
	time_t time_seconds = time(NULL);  
	struct tm local_time;  
	localtime_r(&time_seconds, &local_time);  

	swprintf(filename, FILE_NAME_LEN_MAX, L"%s%d%02d%02d%02d%02d%02d.mp4", MOUNT_DIR, local_time.tm_year + 1900, local_time.tm_mon + 1,  
		local_time.tm_mday, local_time.tm_hour, local_time.tm_min, local_time.tm_sec);

	return;
}


void *record_thread(void *p)
{
	int ret = 0;
	Mp4Encoder mp4Encoder;
	Uint8_t *packet = NULL;
	Uint32_t packetSize = PACKET_SIZE_MAX;
	Uint64_t pts = 0;
	int result = 0;
	Uint64_t start_time = 0;
	fd_set inputs, testfds;
	struct timeval timeout;
	int fd;
	wchar_t filename[FILE_NAME_LEN_MAX];
	wmemset(filename, 0, FILE_NAME_LEN_MAX);
	VideoEncodeMgr& videoEncoder = *VideoEncodeMgr::instance();


	packet = (Uint8_t *)malloc(PACKET_SIZE_MAX);
	memset(packet, 0, PACKET_SIZE_MAX);

	videoEncoder.getFd(CHN0, fd);

	while (record_enable_flag)
	{
start_record:
		sem_wait(&p_gs_record_thd_param->wake_sem);
		/*
			1.to do 是否接入sd卡
			2.to do sd是否已进行过格式化
			3.失败 goto start_record，提醒用户进行相关操作并重试。
		*/

		if (THD_STAT_STOP == p_gs_record_thd_param->thd_stat)//录像结束关闭MP4文件
		{
			if ((TRUE == sd_card_mount) && (OK == storage_umount_sdcard(MOUNT_DIR)))
			{
				sd_card_mount = FALSE;
			}
			continue;
		}
		else if (FALSE == sd_card_mount)
		{
			if (ERROR == storage_mount_sdcard(MOUNT_DIR, DEV_NAME))
			{
				printf("record start failed\n");
				continue;
			}
			sd_card_mount = TRUE;
		}

		sleep(p_gs_record_thd_param->delay_time);
		videoEncoder.startRecvStream(CHN0);

		while (THD_STAT_START == p_gs_record_thd_param->thd_stat)
		{
			packetSize = PACKET_SIZE_MAX;
			memset(packet, 0, packetSize);
			testfds = inputs;   
			timeout.tv_sec = 2;	 
			timeout.tv_usec = 500000;   
			FD_ZERO(&inputs);//用select函数之前先把集合清零
			FD_SET(fd,&inputs);//把要监测的句柄——fd,加入到集合里
			result = select(fd+1, &testfds, NULL, NULL, &timeout);	
			switch (result)
			{   
				case 0:
					printf("timeout\n");
					break;
				case -1:
					perror("select");
					exit(1);
				default:
	/*  1.分时录像，第一个I帧到最后一个B帧时间差要小于20分钟
		2.延时录像，录像开始时间延后delay_time
		3.录像文件大小，单个不超过2G
	*/
					if(FD_ISSET(fd,&testfds))
					{
					
						videoEncoder.getStream(CHN0, packet, packetSize, pts);
						if (UNCOMPLETE == combine_the_first_frame(packet, packetSize, CODEC_H264))//组合第一帧数据
						{
							break;
						}
						if (0 == start_time)
						{
							start_time = pts;
							record_get_mp4_filename(filename);
							mp4Encoder.setFileName(filename);
							mp4Encoder.openMp4Encoder();
							if (ERROR == mp4Encoder.writeVideoFrame(g_packet, g_packet_size, (Uint32_t)(pts/1000)))
							{
								printf("write video failed!\n");
							}
						}
						else if (p_gs_record_thd_param->divide_time <= (pts -start_time))//分时功能
						{
							videoEncoder.stopRecvStream(CHN0);
							mp4Encoder.closeMp4Encoder();
							printf("create new file\n");
							//refresh_video_filelist();
							//p_gs_record_thd_param->thd_stat = THD_STAT_STOP;
							//break;
							videoEncoder.startRecvStream(CHN0);
							start_time = pts;
							record_get_mp4_filename(filename);
							mp4Encoder.setFileName(filename);
							mp4Encoder.openMp4Encoder();
							if (ERROR == mp4Encoder.writeVideoFrame(g_packet, g_packet_size, (Uint32_t)(pts/1000)))
							{
								printf("write video failed!\n");
							}
						}
						else
						{
							if (ERROR == mp4Encoder.writeVideoFrame(g_packet, g_packet_size, (Uint32_t)(pts/1000)))
							{
								printf("write video failed!\n");
							}

						}
						memset(g_packet, 0, PACKET_SIZE_MAX);
						break;
					}
				}
				if (THD_STAT_STOP == p_gs_record_thd_param->thd_stat)//录像结束关闭MP4文件
				{
					videoEncoder.stopRecvStream(CHN0);
					mp4Encoder.closeMp4Encoder();
					break;
				}

		}

	}
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
	p_gs_record_thd_param->thd_stat = THD_STAT_START;

	return OK;

}

void record_thread_stop()
{
	pthread_mutex_lock(&p_gs_record_thd_param->mutex);
	p_gs_record_thd_param->thd_stat = THD_STAT_STOP;
	pthread_mutex_unlock(&p_gs_record_thd_param->mutex);

}

void record_thread_start()
{
	sem_post(&p_gs_record_thd_param->wake_sem);
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
	pthread_mutex_init(&p_gs_record_thd_param->mutex, NULL);
	sem_init(&(p_gs_record_thd_param->wake_sem),0,0);//第二个如果不为0代表进程间共享sem，第三个代表sem值的初始值

	return OK;
}


int record_param_exit()
{
	sem_destroy(&p_gs_record_thd_param->wake_sem);
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
	record_thread_stop();
	if (TRUE == sd_card_mount)
	{
		if (OK == storage_umount_sdcard(MOUNT_DIR))
		{
			sd_card_mount = FALSE;
		}
	}
	sem_post(&p_gs_record_thd_param->wake_sem);
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
分时录像设置
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
延时录像设置
*/
int record_set_divide_time()
{
	//暂无需求
}
