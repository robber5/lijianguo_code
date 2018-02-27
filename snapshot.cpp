#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "video_encoder.h"
#include "types.h"
#include "storage.h"
#include "snapshot.h"
#include "record_search.h"
#include "config_manager.h"
#include "media.h"
#include "video_processor.h"
#include "video_input.h"
#include "Detu_AlgSrMain.h"
#include "jpeg_encoder.h"

using namespace detu_config_manager;
using namespace detu_media;

#define TRUE 1
#define FALSE 0
#define PACKET_SIZE_MAX (10*1024*1024)
#define PACKET_YUV_SIZE_MAX (15*1024*1024)

typedef enum
{
	THD_STAT_START,
	THD_STAT_STOP,
	THD_STAT_QUIT,
	THD_STAT_MAX,
} THD_STAT_E;

typedef enum
{
	SAVE_2DFILE_OFF,
	SAVE_2DFILE_ON,
	SAVE_2DFILE_MAX,
} SAVE_2DFILE_E;

typedef enum
{
	SAVE_SOURCEFILE_OFF,
	SAVE_SOURCEFILE_ON,
	SAVE_SOURCEFILE_MAX,
} SAVE_SOURCEFILE_E;


typedef enum
{
	SNAPSHOT_MODE_SINGLE,
	SNAPSHOT_MODE_SERIES,
	SNAPSHOT_MODE_MAX,
} SNAPSHOT_MODE_E;

typedef enum
{
	EXPOSURE_MODE_AUTO,
	EXPOSURE_MODE_MANUAL,
	EXPOSURE_MODE_MAX,

} EXPOSURE_MODE_E;

typedef enum
{
	EXPOSURE_TIME_AUTO, //自动模式
	EXPOSURE_TIME_250US = 250, // 1/4000s
	EXPOSURE_TIME_500US = 500, // 1/2000s
	EXPOSURE_TIME_1000US = 1000, // 1/1000s
	EXPOSURE_TIME_2000US = 2000, // 1/500s
	EXPOSURE_TIME_4000US = 4000, // 1/250s
	EXPOSURE_TIME_16666US = 16666, // 1/60s
	EXPOSURE_TIME_40000US = 40000, // 1/25s
}EXPOSURE_TIME_E;

typedef struct snapshot_cond_s
{
	pthread_cond_t cond;
	pthread_mutex_t mutex;
	int				wake;
	int				finish;
}SNAPSHOT_COND_S;

typedef struct snapshot_res_s
{
	Uint32_t height;
	Uint32_t width;
}SNAPSHOT_RES_S;

typedef struct snapshot_config
{
	unsigned int delay_time; //延时录像时间
	THD_STAT_E thd_stat;
	SNAPSHOT_MODE_E snapshot_mode;
	SAVE_SOURCEFILE_E save_sourcefile;
	SAVE_2DFILE_E save_2dfile;
	SNAPSHOT_RES_S resolution;
	Uint32_t enlarge_factor;
	Uint32_t media_wkmode;
	Uint32_t capture_effect;

} SNAPSHOT_USER_CONFIG_S;

typedef struct snapshot_params
{
	SNAPSHOT_MODE_E snapshot_mode; //拍照模式；单拍，连拍
	Uint32_t media_wkmode;
	unsigned int delay_time; //延时拍照
	SAVE_SOURCEFILE_E save_sourcefile;
	SAVE_2DFILE_E save_2dfile;
	SNAPSHOT_RES_S resolution;
	Uint32_t enlarge_factor;
	Uint32_t capture_effect;
	pthread_t           pid;
	volatile THD_STAT_E thd_stat;
	sem_t               wake_sem;//拍照开关信号量
	SNAPSHOT_COND_S		snapshot_cond;
	pthread_mutex_t     mutex;

} SNAPSHOT_PARAMS_S, *p_snapshot_thread_params_s;

/*
typedef struct snapshot_pic_packet_s
{
	Uint32_t packet_cur_index;
	Uint32_t packet_size;
	Uint32_t *packet_index_size;
	Int32_t *packet_chn_index;
	Uint8_t *packet;
} SNAPSHOT_PIC_PACKET_S;
*/
typedef struct snapshot_pic_chn_s
{
	Int32_t chn;
	Int32_t fd;
	Uint32_t packet_size;
	Uint32_t chn_status;//1:stop,0:start
	char filename[FILE_NAME_LEN_MAX];
	Uint8_t *packet;
} SNAPSHOT_PIC_CHN_S;

typedef struct snapshot_pic_s
{
	Int32_t chn_num;
	SNAPSHOT_PIC_CHN_S *pic_chn;
}SNAPSHOT_PIC_S;


static int snapshot_enable_flag = TRUE;
static p_snapshot_thread_params_s p_gs_snapshot_thd_param;
static SNAPSHOT_PIC_S *p_s_snapshot_pic_st;
static SNAPSHOT_PIC_CHN_S *p_s_snapshot_pic_chn_st;

static VideoEncodeMgr& s_videoEncoder = *VideoEncodeMgr::instance();
static VideoProcMgr& s_videoprocessor = *VideoProcMgr::instance();
static VideoInputMgr& s_videoinput = *VideoInputMgr::instance();

#define VALUE "value"
#define SNAPSHOT_M "snapshot"
#define SNAPSHOT_STATUS "status"
#define DELAY_TIME "delay_time"
#define PIC_CHN "chn"
#define SNAPSHOT_MODE "snapshot_mode"
#define RESOLUTION "resolution"
#define SAVE_SOURCEFILE "save_sourcefile"
#define SAVE_2DFILE "save_2dfile"
#define ENLARGE_FACTOR "enlarge_factor"
#define ORIGINAL_RES "original_res"

#define CHN_COUNT 6
#define CHN_NUM_MAX 7


static char s_snapshot_status[THD_STAT_MAX][16] = {"start", "stop", "quit"};
static char s_snapshot_mode[SNAPSHOT_MODE_MAX][16] = {"single", "series"};
static char s_snapshot_chn[CHN_NUM_MAX][16] = {CH0_VIDEO_DIR, CH1_VIDEO_DIR, CH2_VIDEO_DIR, CH3_VIDEO_DIR, AVS_1080P, AVS_VIDEO_DIR, "avsSr"};

static char s_picture_dir[CHN_NUM_MAX][16] = {CH0_VIDEO_DIR, CH1_VIDEO_DIR, CH2_VIDEO_DIR, CH3_VIDEO_DIR, AVS_VIDEO_DIR, AVS_VIDEO_DIR, AVS_VIDEO_DIR};
static char s_save_sourcefile[SAVE_SOURCEFILE_MAX][16] = {"off", "on"};
static char s_save_2dfile[SAVE_2DFILE_MAX][16] = {"off", "on"};

#define PID_NULL			((pthread_t)(-1))

#define PIC_COUNT_MIN 35

#define ORIGINAL_CHN_NUM 4
#define AVS_CHN_COUNT 2

static S_Result snapshot_thread_start(SNAPSHOT_USER_CONFIG_S usercfg);

static S_Result snapshot_thread_stop();

static S_Result snapshot_get_jpeg_filename(char *filename, int index)
{
	time_t time_seconds = time(NULL);
	struct tm local_time;
	localtime_r(&time_seconds, &local_time);

	if (0 == p_gs_snapshot_thd_param->capture_effect)
	{
		if (SAVE_SOURCEFILE_OFF == p_gs_snapshot_thd_param->save_sourcefile)
		{
			index += ORIGINAL_CHN_NUM;
		}
		snprintf(filename, FILE_NAME_LEN_MAX, "%s%s/%d%02d%02d%02d%02d%02d-%s.jpg", MOUNT_DIR, s_picture_dir[index], local_time.tm_year + 1900, local_time.tm_mon + 1,
					local_time.tm_mday, local_time.tm_hour, local_time.tm_min, local_time.tm_sec, s_snapshot_chn[index]);
	}
	else
	{
		index += ORIGINAL_CHN_NUM;
		snprintf(filename, FILE_NAME_LEN_MAX, "%s%s/%d%02d%02d%02d%02d%02d-%s.jpg", MOUNT_DIR, s_picture_dir[index], local_time.tm_year + 1900, local_time.tm_mon + 1,
							local_time.tm_mday, local_time.tm_hour, local_time.tm_min, local_time.tm_sec, s_snapshot_chn[index]);
	}

	return S_OK;
}

static S_Result snapshot_set_chn()
{
	SNAPSHOT_PIC_CHN_S *pic_chn = p_s_snapshot_pic_chn_st;

	if (0 == p_gs_snapshot_thd_param->capture_effect)
	{
		if (SAVE_SOURCEFILE_OFF == p_gs_snapshot_thd_param->save_sourcefile)
		{

			if (1 == p_gs_snapshot_thd_param->media_wkmode)
			{
				pic_chn[0].chn = CHN5_AVS_1080P;
				pic_chn[1].chn = CHN4_AVS_4K3K;
			}
			else
			{
				pic_chn[0].chn = CHN17_AVS_JEPG_1080P;
				pic_chn[1].chn = CHN14_AVS_JEPG;
			}
			if (1 < p_gs_snapshot_thd_param->enlarge_factor)
			{
				pic_chn[2].chn = pic_chn[1].chn;
			}

		}
		else
		{
			if (1 == p_gs_snapshot_thd_param->media_wkmode)
			{
				pic_chn[0].chn = CHN0_PIPE0_4K3K;
				pic_chn[1].chn = CHN1_PIPE1_4K3K;
				pic_chn[2].chn = CHN2_PIPE2_4K3K;
				pic_chn[3].chn = CHN3_PIPE3_4K3K;
				pic_chn[4].chn = CHN5_AVS_1080P;
				pic_chn[5].chn = CHN4_AVS_4K3K;
			}
			else
			{
				pic_chn[0].chn = CHN8_PIPE0_JPEG;
				pic_chn[1].chn = CHN9_PIPE1_JPEG;
				pic_chn[2].chn = CHN10_PIPE2_JPEG;
				pic_chn[3].chn = CHN11_PIPE3_JPEG;
				pic_chn[4].chn = CHN17_AVS_JEPG_1080P;
				pic_chn[5].chn = CHN14_AVS_JEPG;
			}
			if (1 < p_gs_snapshot_thd_param->enlarge_factor)
			{
				pic_chn[6].chn = pic_chn[5].chn;
			}
		}
	}
	else
	{
		// 3D todo;
	}

	return S_OK;
}

static S_Result snapshot_set_chn_num()
{

	if (0 == p_gs_snapshot_thd_param->capture_effect)
	{
		if (SAVE_SOURCEFILE_OFF == p_gs_snapshot_thd_param->save_sourcefile)
		{
			p_s_snapshot_pic_st->chn_num = AVS_CHN_COUNT;
		}
		else
		{
			p_s_snapshot_pic_st->chn_num = CHN_COUNT;
		}
		if (1 < p_gs_snapshot_thd_param->enlarge_factor)
		{
			p_s_snapshot_pic_st->chn_num++;
		}

	}
	else
	{
		// 3D todo;
	}

	return S_OK;
}


static int get_max_fd()
{
	int i, fd_max = -1, num;
	SNAPSHOT_PIC_CHN_S *pic_chn;
	num = p_s_snapshot_pic_st->chn_num;
	pic_chn = p_s_snapshot_pic_st->pic_chn;
	for (i = 0; i < num; i++)
	{
		if (pic_chn[i].fd >= fd_max)
		{
			fd_max = pic_chn[i].fd;
		}
	}

	return fd_max;
}

static void snapshot_sr_consult(Uint8_t *packet_in, char *filename)
{
	Uint32_t width_in, height_in;
	Uint32_t width_out, height_out;
	Uint32_t size;
	Uint8_t *packet_out;
//	FILE *fp = NULL;

	height_in = p_gs_snapshot_thd_param->resolution.height;
	width_in = p_gs_snapshot_thd_param->resolution.width;
	height_out = p_gs_snapshot_thd_param->resolution.height * p_gs_snapshot_thd_param->enlarge_factor;
	width_out = p_gs_snapshot_thd_param->resolution.width * p_gs_snapshot_thd_param->enlarge_factor;
	//size = height_in * width_in * 3/2;
	size = height_out * width_out * 3/2;
	packet_out = (Uint8_t *)malloc(size * sizeof(Uint8_t));

	Detu_AlgSrRun(packet_in, width_in, height_in, packet_out, width_out, height_out);


	yuv_encode_to_jpeg(filename, packet_out, width_out, height_out);

	free(packet_out);

}

static void snapshot_get_stream_fd()
{
	int chn_num = 0, i = 0;
	SNAPSHOT_PIC_CHN_S *pic_chn = p_s_snapshot_pic_chn_st;
	if (0 == p_gs_snapshot_thd_param->capture_effect)
	{
		chn_num = p_s_snapshot_pic_st->chn_num;

		if (1 == p_gs_snapshot_thd_param->enlarge_factor)
		{

			for (i = 0; i < chn_num; i++)
			{
				s_videoEncoder.getFd(pic_chn[i].chn, pic_chn[i].fd);
			}
		}
		else
		{
			chn_num--;
			for (i = 0; i < chn_num; i++)
			{
				s_videoEncoder.getFd(pic_chn[i].chn, pic_chn[i].fd);
			}
			pic_chn[i].fd = pic_chn[i-1].fd;
		}
	}
	else
	{
		//3D todo;
	}

}

static void snapshot_stop_stream(SNAPSHOT_PIC_CHN_S *pic_chn)
{
	int i = 0;
	Int32_t chn_num;
	if (0 == p_gs_snapshot_thd_param->capture_effect)
	{

		chn_num = p_s_snapshot_pic_st->chn_num - 1;

		for (i = 0; i < chn_num; i++)
		{
			s_videoEncoder.stopRecvStream(pic_chn[i].chn);
		}
		if (1 == p_gs_snapshot_thd_param->enlarge_factor)
		{
			s_videoEncoder.stopRecvStream(pic_chn[i].chn);
		}

	}
	else
	{
		//3D todo;
	}

}

static void snapshot_start_stream()
{
	int i = 0;
	Int32_t chn_num;
	SNAPSHOT_PIC_CHN_S *pic_chn = p_s_snapshot_pic_chn_st;

	if (0 == p_gs_snapshot_thd_param->capture_effect)
	{
		chn_num = p_s_snapshot_pic_st->chn_num - 1;

		for (i = 0; i < chn_num; i++)
		{
			s_videoEncoder.startRecvStream(pic_chn[i].chn);
		}
		if (1 == p_gs_snapshot_thd_param->enlarge_factor)
		{
			s_videoEncoder.startRecvStream(pic_chn[i].chn);
		}

	}
	else
	{
		//3D todo;
	}


}


static S_Result snapshot_get_stream(SNAPSHOT_PIC_CHN_S &pic_chn, int32_t chn_index, Uint32_t flag)
{
	Uint8_t *packet = NULL;
	Uint64_t pts;
	Uint32_t seq;
	Uint32_t packetSize;

	if ((0 == pic_chn.chn_status) && (TRUE == flag))
	{
		packet = pic_chn.packet;
		packetSize = PACKET_SIZE_MAX;
	}
	else
	{
		packet = NULL;
		packetSize = 0;
	}
	if ((chn_index == (p_s_snapshot_pic_st->chn_num - 1)) && (1 < p_gs_snapshot_thd_param->enlarge_factor))
	{
		if (NULL != packet)
		{
			Yuv yuv;
			yuv.data = packet;
			yuv.size = PACKET_YUV_SIZE_MAX;
			if (S_ERROR == s_videoprocessor.GetChnFrame(0, 0, yuv))
			{
				return S_ERROR;
			}
			packetSize = yuv.size;
		}
	}
	else
	{
		if (S_ERROR == s_videoEncoder.getStream(pic_chn.chn, seq, packet, packetSize, pts))
		{
			return S_ERROR;
		}

	}
	if ((0 == pic_chn.chn_status) && (TRUE == flag))
	{
			pic_chn.packet_size = packetSize;
	}

	return S_OK;

}


static void snapshot_save_pic_to_sdcard()
{
	FILE *fp = NULL;
	int i = 0;
	SNAPSHOT_PIC_CHN_S *pic_chn = p_s_snapshot_pic_chn_st;
	for (i = 0; i < p_s_snapshot_pic_st->chn_num; i++)
	{

		if ((i == (p_s_snapshot_pic_st->chn_num - 1)) && (1 < p_gs_snapshot_thd_param->enlarge_factor))
		{
			snapshot_sr_consult(pic_chn[i].packet, pic_chn[i].filename);
		}
		else
		{
			if (NULL == (fp = fopen(pic_chn[i].filename, "wb")))
			{
				printf("can not open jpeg file:%s\n", pic_chn[i].filename);
			}
			fwrite(pic_chn[i].packet, sizeof(Uint8_t), pic_chn[i].packet_size, fp);
			fflush(fp);
			fclose(fp);
			printf("create file:%s, packet_size:%d, chn_index:%d\n", pic_chn[i].filename, pic_chn[i].packet_size, i);
		}
	}


}

static void snapshot_pics_alloc(SNAPSHOT_PIC_S *pics)
{
	int i = 0;
	if (NULL == pics->pic_chn)
	{
		pics->pic_chn = (SNAPSHOT_PIC_CHN_S *)malloc(pics->chn_num * sizeof(SNAPSHOT_PIC_CHN_S));
		memset(pics->pic_chn, 0, pics->chn_num * sizeof(SNAPSHOT_PIC_CHN_S));
		if (NULL != pics->pic_chn)
		{
			p_s_snapshot_pic_chn_st = pics->pic_chn;
			for (i = 0; i < (pics->chn_num - 1); i++)
			{
				pics->pic_chn[i].packet = (Uint8_t *)malloc(PACKET_SIZE_MAX * sizeof(pics->chn_num));
			}
			if (1 < p_gs_snapshot_thd_param->enlarge_factor)
			{
				pics->pic_chn[i].packet = (Uint8_t *)malloc(PACKET_YUV_SIZE_MAX * sizeof(pics->chn_num));
			}
			else
			{
				pics->pic_chn[i].packet = (Uint8_t *)malloc(PACKET_SIZE_MAX * sizeof(pics->chn_num));

			}
		}
	}
}

static void snapshot_pics_free(SNAPSHOT_PIC_S *pics)
{
	int i = 0;
	if (NULL != pics->pic_chn)
	{
		for (i = 0; i < pics->chn_num; i++)
		{
			free(pics->pic_chn[i].packet);
		}
		free(pics->pic_chn);
		pics->pic_chn = NULL;
		p_s_snapshot_pic_chn_st = NULL;
	}
	memset(pics, 0, sizeof(SNAPSHOT_PIC_S));
}

static void *snapshot_thread(void *p)
{
	Uint32_t pic_count = 0;
	fd_set inputs, testfds;
	struct timeval timeout;
	int result = 0;
	int fd_found[CHN_COUNT];
	Uint32_t index = 0;
	Uint32_t remain_chn_num = 0;
	int i = 0, j = 0;
	int fd_max = -1;
	//int chn_status[CHN_COUNT] = {0};//0:stop,1:start
	SNAPSHOT_PIC_CHN_S *pic_chn;

	while (snapshot_enable_flag)
	{
//		sem_wait(&p_gs_snapshot_thd_param->wake_sem);
		pthread_mutex_lock(&p_gs_snapshot_thd_param->snapshot_cond.mutex);
		while ((THD_STAT_START != p_gs_snapshot_thd_param->thd_stat) && (THD_STAT_QUIT != p_gs_snapshot_thd_param->thd_stat))
		{
			pthread_cond_wait(&p_gs_snapshot_thd_param->snapshot_cond.cond, &p_gs_snapshot_thd_param->snapshot_cond.mutex);
		}
		pthread_mutex_unlock(&p_gs_snapshot_thd_param->snapshot_cond.mutex);


		if (THD_STAT_QUIT == p_gs_snapshot_thd_param->thd_stat)
		{
			break;//record destroy
		}

		if (S_ERROR == storage_sdcard_check())
		{
			pthread_mutex_lock(&p_gs_snapshot_thd_param->snapshot_cond.mutex);
			p_gs_snapshot_thd_param->snapshot_cond.wake = FALSE;
			p_gs_snapshot_thd_param->snapshot_cond.finish = TRUE;//拍照命令执行完成，但失败
			p_gs_snapshot_thd_param->thd_stat = THD_STAT_STOP;
			pthread_cond_broadcast(&p_gs_snapshot_thd_param->snapshot_cond.cond);//通知拍照模线程唤醒失败
			pthread_mutex_unlock(&p_gs_snapshot_thd_param->snapshot_cond.mutex);
			continue;
		}

		sleep(p_gs_snapshot_thd_param->delay_time);
		snapshot_set_chn_num();
		snapshot_pics_alloc(p_s_snapshot_pic_st);
		snapshot_set_chn();
		snapshot_get_stream_fd();
		snapshot_start_stream();
		fd_max = get_max_fd();
		pic_chn = p_s_snapshot_pic_st->pic_chn;
		for (i = 0; i < (p_s_snapshot_pic_st->chn_num - 1); i++)
		{
			FD_SET(pic_chn[i].fd, &inputs);
		}
		if (1 == p_gs_snapshot_thd_param->enlarge_factor)
		{
			FD_SET(pic_chn[i].fd, &inputs);
		}
		remain_chn_num = p_s_snapshot_pic_st->chn_num;

		while (THD_STAT_START == p_gs_snapshot_thd_param->thd_stat)
		{
			timeout.tv_sec = 2;
			timeout.tv_usec = 500000;
			testfds = inputs;
			result = select(fd_max + 1, &testfds, NULL, NULL, &timeout);
			switch (result)
			{
				case 0:
					printf("timeout\n");
					break;
				case -1:
					perror("select");
					break;
				default:
					for (i = 0, j = 0; i < p_s_snapshot_pic_st->chn_num; i++)
					{
						if (FD_ISSET(pic_chn[i].fd, &testfds))
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
						if (PIC_COUNT_MIN > pic_count)
						{
							if (0 == index)
							{
								pic_count++;
							}
							snapshot_get_stream(pic_chn[index], index, FALSE);
							continue;
						}
						else if (1 == pic_chn[index].chn_status)
						{
							snapshot_get_stream(pic_chn[index], index, FALSE);
							continue;
						}
						if(S_ERROR == snapshot_get_stream(pic_chn[index], index, TRUE))
						{
							printf("get pic failed\n");
							continue;
						}

						snapshot_get_jpeg_filename(pic_chn[index].filename, index);
						if ((SNAPSHOT_MODE_SINGLE == p_gs_snapshot_thd_param->snapshot_mode) && (0 < remain_chn_num))
						{
							printf("remain chn:%d\n", remain_chn_num);
							pic_chn[index].chn_status = 1;
							--remain_chn_num;
							if (0 == remain_chn_num)
							{
								snapshot_stop_stream(pic_chn);
								FD_ZERO(&inputs);
								snapshot_save_pic_to_sdcard();
								pthread_mutex_lock(&p_gs_snapshot_thd_param->snapshot_cond.mutex);
								if (FALSE == p_gs_snapshot_thd_param->snapshot_cond.wake)
								{
									p_gs_snapshot_thd_param->snapshot_cond.wake = TRUE;
									pthread_cond_broadcast(&p_gs_snapshot_thd_param->snapshot_cond.cond);//通知拍照命令执行完成
								}
								pthread_mutex_unlock(&p_gs_snapshot_thd_param->snapshot_cond.mutex);
								break;
							}
						}
						else if (SNAPSHOT_MODE_SERIES == p_gs_snapshot_thd_param->snapshot_mode)
						{
							//todo
							pthread_mutex_lock(&p_gs_snapshot_thd_param->snapshot_cond.mutex);
							if (FALSE == p_gs_snapshot_thd_param->snapshot_cond.wake)
							{
								p_gs_snapshot_thd_param->snapshot_cond.wake = TRUE;
								pthread_cond_broadcast(&p_gs_snapshot_thd_param->snapshot_cond.cond);//通知拍照命令执行完成
							}
							pthread_mutex_unlock(&p_gs_snapshot_thd_param->snapshot_cond.mutex);

						}
					}
					break;
			}

		}
		if (THD_STAT_START != p_gs_snapshot_thd_param->thd_stat)//退出拍照模式
		{
			pic_count = 0;
			if (SNAPSHOT_MODE_SERIES == p_gs_snapshot_thd_param->snapshot_mode)
			{
				snapshot_stop_stream(pic_chn);
			}
			snapshot_pics_free(p_s_snapshot_pic_st);
			pthread_mutex_lock(&p_gs_snapshot_thd_param->snapshot_cond.mutex);
			if (TRUE == p_gs_snapshot_thd_param->snapshot_cond.wake)
			{
				p_gs_snapshot_thd_param->snapshot_cond.wake = FALSE;
				pthread_cond_broadcast(&p_gs_snapshot_thd_param->snapshot_cond.cond);//通知拍照模块已处于stop状态
			}
			pthread_mutex_unlock(&p_gs_snapshot_thd_param->snapshot_cond.mutex);
		}

	}

	snapshot_pics_free(p_s_snapshot_pic_st);

	return (void *)S_OK;
}

static S_Result snapshot_trans_config(const Json::Value& config,SNAPSHOT_USER_CONFIG_S& usercfg)
{

	int i = 0;

	if (config.isMember(SNAPSHOT_STATUS) && config[SNAPSHOT_STATUS].isMember(VALUE))
	{
		for (i = 0; i < THD_STAT_MAX; i++)
		{
			if (!(config[SNAPSHOT_STATUS][VALUE].asString()).compare(s_snapshot_status[i]))
			{
				usercfg.thd_stat = (THD_STAT_E)i;
				break;
			}
		}
	}
	if (config.isMember(SNAPSHOT_MODE))
	{
		for (i = 0; i < SNAPSHOT_MODE_MAX; i++)
		{
			if (!(config[SNAPSHOT_MODE][VALUE].asString()).compare(s_snapshot_mode[i]))
			{
				usercfg.snapshot_mode = (SNAPSHOT_MODE_E)i;
				break;
			}
		}
	}
#if 1
	if (config.isMember(SAVE_2DFILE))
	{
		for (i = 0; i < SAVE_2DFILE_MAX; i++)
		{
			if (!(config[SAVE_2DFILE][VALUE].asString()).compare(s_save_2dfile[i]))
			{
				usercfg.save_2dfile = (SAVE_2DFILE_E)i;
				break;
			}
		}
	}

	if (config.isMember(SAVE_SOURCEFILE))
	{
		for (i = 0; i < SAVE_SOURCEFILE_MAX; i++)
		{
			if (!(config[SAVE_SOURCEFILE][VALUE].asString()).compare(s_save_sourcefile[i]))
			{
				usercfg.save_sourcefile = (SAVE_SOURCEFILE_E)i;
				break;
			}
		}
	}
#else
	if (config.isMember(SAVE_2DFILE))
	{
		usercfg.save_2dfile = (SAVE_2DFILE_E)config[SAVE_2DFILE][VALUE].asUInt();

	}
	if (config.isMember(SAVE_SOURCEFILE))
	{
		usercfg.save_sourcefile = (SAVE_SOURCEFILE_E)config[SAVE_SOURCEFILE][VALUE].asUInt();
	}
#endif
	if (config.isMember(DELAY_TIME))
	{
		usercfg.delay_time = config[DELAY_TIME][VALUE].asUInt();
	}

	if (config.isMember(RESOLUTION))
	{
		usercfg.resolution.width = config[RESOLUTION][VALUE][ORIGINAL_RES][0].asUInt();
		usercfg.resolution.height = config[RESOLUTION][VALUE][ORIGINAL_RES][1].asUInt();
		usercfg.enlarge_factor = config[RESOLUTION][VALUE][ENLARGE_FACTOR].asUInt();
	}
	if (config.isMember("mode"))
	{
		usercfg.media_wkmode = config["mode"][VALUE].asUInt();
	}
	if (config.isMember("capture_effect"))
	{
		usercfg.capture_effect = config["capture_effect"][VALUE].asUInt();
	}

	return S_OK;

}


static S_Result snapshot_check_config(const Json::Value& config)
{
	int delay_time = 0;
	int i = 0;
	S_Result S_ret = S_OK;

	if (config.isMember(DELAY_TIME))
	{
		delay_time = config[DELAY_TIME][VALUE].asInt();
	}
	//printf("config:%s\n", config.toStyledString().c_str());
	do
	{
		if (config.isMember(SNAPSHOT_STATUS) && config[SNAPSHOT_STATUS].isMember(VALUE))
		{
			for (i = 0; i < THD_STAT_MAX; i++)
			{
				if (!(config[SNAPSHOT_STATUS][VALUE].asString()).compare(s_snapshot_status[i]))
				{
					break;
				}
			}
			if (THD_STAT_MAX == i)
			{
				S_ret = S_ERROR;
				printf("status error\n");
				break;
			}
		}
		if (config.isMember(SNAPSHOT_MODE))
		{
			for (i = 0; i < SNAPSHOT_MODE_MAX; i++)
			{
				if (!(config[SNAPSHOT_MODE][VALUE].asString()).compare(s_snapshot_mode[i]))
				{
					break;
				}
			}
			if (SNAPSHOT_MODE_MAX == i)
			{
				S_ret = S_ERROR;
				printf("mode error\n");
				break;
			}
		}
#if 1
		if (config.isMember(SAVE_SOURCEFILE))
		{
			for (i = 0; i < SAVE_SOURCEFILE_MAX; i++)
			{
				if (!(config[SAVE_SOURCEFILE][VALUE].asString()).compare(s_save_sourcefile[i]))
				{
					break;
				}
			}
			if (SAVE_SOURCEFILE_MAX == i)
			{
				S_ret = S_ERROR;
				printf("save_sourcefile error\n");
				break;
			}
		}
		if (config.isMember(SAVE_2DFILE))
		{
			for (i = 0; i < SAVE_2DFILE_MAX; i++)
			{
				if (!(config[SAVE_2DFILE][VALUE].asString()).compare(s_save_2dfile[i]))
				{
					break;
				}
			}
			if (SAVE_2DFILE_MAX == i)
			{
				S_ret = S_ERROR;
				printf("save_2dfile error\n");
				break;
			}
		}
#endif
		if (config.isMember(RESOLUTION))
		{
			// todo
		}
		if (0 > delay_time)
		{
			S_ret = S_ERROR;
			printf("delay error\n");
		}
	}while (0);

	return S_ret;
}

//连拍
/*
static S_Result snapshot_for_series_stop()
{
	pthread_mutex_lock(&p_gs_snapshot_thd_param->mutex);
	p_gs_snapshot_thd_param->snapshot_mode = SNAPSHOT_MODE_IDLE;
	pthread_mutex_unlock(&p_gs_snapshot_thd_param->mutex);

	return S_OK;
}

static S_Result snapshot_for_series_start()
{
	pthread_mutex_lock(&p_gs_snapshot_thd_param->mutex);
	p_gs_snapshot_thd_param->snapshot_mode = SNAPSHOT_MODE_SERIES;
	pthread_mutex_unlock(&p_gs_snapshot_thd_param->mutex);

	return S_OK;
}
*/

static S_Result snapshot_thread_cb(const void* clientData, const std::string& name, const Json::Value& oldConfig, const Json::Value& newConfig, Json::Value& response)
{
	S_Result S_ret = S_OK;
	Json::Value snapcfg;
	SNAPSHOT_USER_CONFIG_S oldcfg,newcfg;
	ConfigManager& config = *ConfigManager::instance();

	memset(&newcfg, 0, sizeof(newcfg));
	memset(&oldcfg, 0, sizeof(oldcfg));

	config.getConfig(SNAPSHOT_M, snapcfg, response);

	snapshot_trans_config(newConfig, newcfg);
	snapshot_trans_config(snapcfg, newcfg);
	config.getConfig("media", snapcfg, response);
	snapshot_trans_config(snapcfg, newcfg);
	config.getTempConfig("media", snapcfg, response);
	snapshot_trans_config(snapcfg, newcfg);
	snapshot_trans_config(oldConfig, oldcfg);

	do
	{
		if (newcfg.thd_stat != oldcfg.thd_stat)
		{
			if (THD_STAT_STOP == newcfg.thd_stat)
			{
				snapshot_thread_stop();
				break;
			}
			else
			{
				if (S_ERROR == snapshot_thread_start(newcfg))
				{
					config.getTempConfig("snapshot.status.value", snapcfg, response);
					snapcfg = "stop";
					config.setTempConfig("snapshot.status.value", snapcfg, response);
					S_ret = S_ERROR;
				}
				break;
			}
		}
	} while (0);

	return S_ret;
}

static S_Result snapshot_thread_vd(const void* clientData, const std::string& name, const Json::Value& oldConfig, const Json::Value& newConfig, Json::Value& response)
{
	S_Result S_ret = S_OK;
	S_ret = snapshot_check_config(newConfig);
	if (S_OK != S_ret)
	{
		printf("config is not valid to set!\n");
	}

	return S_ret;
}

static S_Result snapshot_register_validator()
{
	ConfigManager& config = *ConfigManager::instance();

	config.registerValidator(SNAPSHOT_M, &snapshot_thread_vd, (void *)1);

	return S_OK;

}

static S_Result snapshot_register_callback()
{
	ConfigManager& config = *ConfigManager::instance();

	config.registerCallback(SNAPSHOT_M, &snapshot_thread_cb, (void *)1);

	return S_OK;
}

static S_Result snapshot_unregister_validator()
{
	ConfigManager& config = *ConfigManager::instance();

	config.unregisterCallback(SNAPSHOT_M, &snapshot_thread_cb, (void *)1);

	return S_OK;
}

static S_Result snapshot_unregister_callback()
{
	ConfigManager& config = *ConfigManager::instance();

	config.unregisterCallback(SNAPSHOT_M, &snapshot_thread_vd, (void *)1);

	return S_OK;
}


static S_Result snapshot_thread_create()
{
	if (0 != pthread_create(&p_gs_snapshot_thd_param->pid, NULL, snapshot_thread, NULL))
	{
		perror("pthread_create failed:");
		printf("pthread_create get buffer thread failed\n");
		return S_ERROR;
	}
	pthread_setname_np(p_gs_snapshot_thd_param->pid, "snapshot\0");
	//p_gs_snapshot_thd_param->thd_stat = THD_STAT_START;

	return S_OK;

}
/*
static S_Result snapshot_for_once()
{
	pthread_mutex_lock(&p_gs_snapshot_thd_param->mutex);
	if (THD_STAT_START == p_gs_snapshot_thd_param->thd_stat)
	{
		while(PIC_COUNT_MIN > pic_count)
		{
			usleep(100*1000);
		}

		pthread_mutex_lock(&p_gs_snapshot_thd_param->snapshot_cond.mutex);
		if (SNAPSHOT_MODE_IDLE == p_gs_snapshot_thd_param->snapshot_mode)
		{
			p_gs_snapshot_thd_param->snapshot_mode = SNAPSHOT_MODE_SINGLE;
			p_gs_snapshot_thd_param->snapshot_cond.idle = FALSE;
			pthread_cond_broadcast(&p_gs_snapshot_thd_param->snapshot_cond.cond);
			do
			{
				pthread_cond_wait(&p_gs_snapshot_thd_param->snapshot_cond.cond, &p_gs_snapshot_thd_param->snapshot_cond.mutex);
			}while(FALSE == p_gs_snapshot_thd_param->snapshot_cond.idle);//等待拍照命令执行完成
		}
		pthread_mutex_unlock(&p_gs_snapshot_thd_param->snapshot_cond.mutex);
	}

	pthread_mutex_unlock(&p_gs_snapshot_thd_param->mutex);

	return S_OK;
}
*/
static S_Result snapshot_thread_start(SNAPSHOT_USER_CONFIG_S usercfg)
{
	S_Result S_ret = S_OK;

	pthread_mutex_lock(&p_gs_snapshot_thd_param->mutex);
	//enable_pic_mode();
	pthread_mutex_lock(&p_gs_snapshot_thd_param->snapshot_cond.mutex);
	if (THD_STAT_START != p_gs_snapshot_thd_param->thd_stat)
	{
		p_gs_snapshot_thd_param->thd_stat = THD_STAT_START;
		p_gs_snapshot_thd_param->snapshot_mode = usercfg.snapshot_mode;
		p_gs_snapshot_thd_param->delay_time = usercfg.delay_time;
		//p_gs_snapshot_thd_param->chn = usercfg.chn;
		p_gs_snapshot_thd_param->capture_effect = usercfg.capture_effect;
		p_gs_snapshot_thd_param->enlarge_factor = usercfg.enlarge_factor;
		p_gs_snapshot_thd_param->save_2dfile = usercfg.save_2dfile;
		p_gs_snapshot_thd_param->save_sourcefile = usercfg.save_sourcefile;
		p_gs_snapshot_thd_param->resolution.width = usercfg.resolution.width;
		p_gs_snapshot_thd_param->resolution.height = usercfg.resolution.height;
		p_gs_snapshot_thd_param->media_wkmode = usercfg.media_wkmode;

		pthread_cond_broadcast(&p_gs_snapshot_thd_param->snapshot_cond.cond);
		do
		{
			pthread_cond_wait(&p_gs_snapshot_thd_param->snapshot_cond.cond, &p_gs_snapshot_thd_param->snapshot_cond.mutex);
			if ((TRUE == p_gs_snapshot_thd_param->snapshot_cond.finish) && (FALSE == p_gs_snapshot_thd_param->snapshot_cond.wake))
			{
				p_gs_snapshot_thd_param->snapshot_cond.finish = FALSE; //置为初始值
				//disable_pic_mode();
				S_ret = S_ERROR;
				break;
			}
		}while(FALSE == p_gs_snapshot_thd_param->snapshot_cond.wake);
	}
	pthread_mutex_unlock(&p_gs_snapshot_thd_param->snapshot_cond.mutex);
	pthread_mutex_unlock(&p_gs_snapshot_thd_param->mutex);

	return S_ret;
}

static S_Result snapshot_thread_stop()
{
	pthread_mutex_lock(&p_gs_snapshot_thd_param->mutex);

	pthread_mutex_lock(&p_gs_snapshot_thd_param->snapshot_cond.mutex);
	if (THD_STAT_START == p_gs_snapshot_thd_param->thd_stat)
	{
		p_gs_snapshot_thd_param->thd_stat = THD_STAT_STOP;
		do
		{
			pthread_cond_wait(&p_gs_snapshot_thd_param->snapshot_cond.cond, &p_gs_snapshot_thd_param->snapshot_cond.mutex);
		}while (TRUE == p_gs_snapshot_thd_param->snapshot_cond.wake);
	}
	pthread_mutex_unlock(&p_gs_snapshot_thd_param->snapshot_cond.mutex);
	//disable_pic_mode();
	pthread_mutex_unlock(&p_gs_snapshot_thd_param->mutex);

	return S_OK;
}

static void snapshot_cond_init(SNAPSHOT_COND_S *snapshot_cond)
{
	snapshot_cond->mutex = PTHREAD_MUTEX_INITIALIZER; //初始化互斥锁
	snapshot_cond->cond = PTHREAD_COND_INITIALIZER; //初始化条件变量
	snapshot_cond->wake = FALSE;
	snapshot_cond->finish = FALSE;
}

static S_Result snapshot_param_init()
{
	ConfigManager& config = *ConfigManager::instance();

	Json::Value snapCfg, response;

	SNAPSHOT_USER_CONFIG_S usercfg;

	memset(&usercfg, 0, sizeof(SNAPSHOT_USER_CONFIG_S));

	config.setTempConfig("snapshot.status.value", "stop", response);
	config.getTempConfig(SNAPSHOT_M, snapCfg, response);
	snapshot_trans_config(snapCfg, usercfg);

	config.getConfig(SNAPSHOT_M, snapCfg, response);

	snapshot_trans_config(snapCfg, usercfg);

	config.getConfig("media", snapCfg, response);

	snapshot_trans_config(snapCfg, usercfg);

	config.getTempConfig("media", snapCfg, response);

	snapshot_trans_config(snapCfg, usercfg);

	p_s_snapshot_pic_st = (SNAPSHOT_PIC_S *)malloc(sizeof(SNAPSHOT_PIC_S));
	if (NULL == p_s_snapshot_pic_st)
	{
		perror("p_s_snapshot_pic_st malloc failed:");
		printf("record_param_init failed\n");
		return S_ERROR;
	}
	memset(p_s_snapshot_pic_st, 0, sizeof(SNAPSHOT_PIC_S));

	p_gs_snapshot_thd_param = (p_snapshot_thread_params_s)malloc(sizeof(SNAPSHOT_PARAMS_S));
	if (NULL == p_gs_snapshot_thd_param)
	{
		perror("gs_record_thd_param malloc failed:");
		printf("record_param_init failed\n");
		return S_ERROR;
	}

	p_gs_snapshot_thd_param->media_wkmode = usercfg.media_wkmode;
	p_gs_snapshot_thd_param->capture_effect = usercfg.capture_effect;
	p_gs_snapshot_thd_param->save_2dfile = usercfg.save_2dfile;
	p_gs_snapshot_thd_param->save_sourcefile = usercfg.save_sourcefile;
	p_gs_snapshot_thd_param->enlarge_factor = usercfg.enlarge_factor;
	p_gs_snapshot_thd_param->resolution.width = usercfg.resolution.width;
	p_gs_snapshot_thd_param->resolution.height = usercfg.resolution.height;
	p_gs_snapshot_thd_param->delay_time = usercfg.delay_time;
	p_gs_snapshot_thd_param->pid = PID_NULL;
	p_gs_snapshot_thd_param->thd_stat = usercfg.thd_stat;
	p_gs_snapshot_thd_param->snapshot_mode = usercfg.snapshot_mode;
	//p_gs_snapshot_thd_param->chn = usercfg.chn;
	pthread_mutex_init(&p_gs_snapshot_thd_param->mutex, NULL);
	snapshot_cond_init(&p_gs_snapshot_thd_param->snapshot_cond);
	//sem_init(&(p_gs_snapshot_thd_param->wake_sem),0,0);//第二个如果不为0代表进程间共享sem，第三个代表sem值的初始值

	return S_OK;
}

static void snapshot_cond_deinit(SNAPSHOT_COND_S *snapshot_cond)
{
	pthread_mutex_destroy(&snapshot_cond->mutex);
	pthread_cond_destroy(&snapshot_cond->cond);
}

static S_Result snapshot_param_exit()
{
	//sem_destroy(&p_gs_snapshot_thd_param->wake_sem);
	snapshot_cond_deinit(&p_gs_snapshot_thd_param->snapshot_cond);
	pthread_mutex_destroy(&p_gs_snapshot_thd_param->mutex);
	if (NULL != p_gs_snapshot_thd_param)
	{
		free(p_gs_snapshot_thd_param);
	}
	if (NULL != p_s_snapshot_pic_st)
	{
		free(p_s_snapshot_pic_st);
	}

	return S_OK;
}

static S_Result snapshot_thread_destory()
{
	snapshot_enable_flag = FALSE;
	pthread_mutex_lock(&p_gs_snapshot_thd_param->snapshot_cond.mutex);
	p_gs_snapshot_thd_param->thd_stat = THD_STAT_QUIT;
	pthread_cond_broadcast(&p_gs_snapshot_thd_param->snapshot_cond.cond);
	pthread_mutex_unlock(&p_gs_snapshot_thd_param->snapshot_cond.mutex);

	if (p_gs_snapshot_thd_param->pid != PID_NULL)
	{
		pthread_join(p_gs_snapshot_thd_param->pid, 0);
	}

	return S_OK;
}

S_Result snapshot_module_init()
{
	Detu_AlgSrCreate();
	Detu_AlgSrInit();
	snapshot_param_init();
	snapshot_register_callback();
	snapshot_register_validator();
	snapshot_thread_create();

	return S_OK;
}

S_Result snapshot_module_exit()
{
	snapshot_thread_destory();
	snapshot_unregister_validator();
	snapshot_unregister_callback();
	snapshot_param_exit();
	Detu_AlgSrDelete();

	return S_OK;
}

#if 0
/*
延时拍照设置
*/
static int snapshot_set_delay_time(int time)
{
	//todo
	pthread_mutex_lock(&p_gs_snapshot_thd_param->mutex);
	p_gs_snapshot_thd_param->delay_time = time;
	pthread_mutex_unlock(&p_gs_snapshot_thd_param->mutex);
	return 0;
}

static int snapshot_set_chn(int chn)
{
	pthread_mutex_lock(&p_gs_snapshot_thd_param->mutex);
	p_gs_snapshot_thd_param->chn = chn;
	pthread_mutex_unlock(&p_gs_snapshot_thd_param->mutex);

	return 0;
}

static void snapshot_set_exposure_mode(EXPOSURE_MODE_E mode, EXPOSURE_TIME_E time)//设置曝光时间
{
	//todo

}
#endif


