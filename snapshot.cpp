#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>

#include <sys/types.h>   
#include <sys/time.h>     
#include <fcntl.h>   
#include <sys/ioctl.h>   
#include <unistd.h> 
#include "video_encoder.h"

using namespace detu_media;
#define CHN0 0

typedef enum
{
	THD_STAT_IDLE,
	THD_STAT_START,
	THD_STAT_STOP,
	THD_STAT_MAX,
} THD_STAT_E;

typedef enum
{
	EXPOSURE_MODE_AUTO,
	EXPOSURE_MODE_MANUAL,
	EXPOSURE_MODE_MAX,

} EXPOSURE_MODE_E;

typedef enum
{
	SNAPSHOT_MODE_IDLE,
	SNAPSHOT_MODE_SINGLE,
	SNAPSHOT_MODE_SERIES,
	SNAPSHOT_MODE_MAX,
} SNAPSHOT_MODE_E;

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

typedef struct snapshot_params
{
	unsigned int exposure_mode; //曝光模式
	unsigned int exposure_time; //曝光时长
	unsigned int snapshot_mode; //拍照模式；单拍，连拍
	unsigned int delay_time; //延时拍照
	pthread_t           pid;
	volatile THD_STAT_E thd_stat;
	sem_t               wake_sem;//拍照开关信号量
	pthread_mutex_t     mutex;

} SNAPSHOT_PARAMS_S, *p_snapshot_thread_params_s;

SNAPSHOT_PARAMS_S gs_snapshot_param;


static int snapshot_enable_flag = TRUE;
static p_snapshot_thread_params_s p_gs_snapshot_thd_param;

#define PID_NULL			((pid_t)(-1))


int snapshot_thread(void *p)
{

	int ret = 0;
	p_record_thread_params_s parameter;
	parameter = (p_record_thread_params_s *)p;

	VideoEncodeMgr& videoEncoder = *VideoEncodeMgr::instance();

	while (snapshot_enable_flag)
	{
		sem_wait(&p_gs_record_thd_param->wake_sem);
		sleep(p_gs_record_thd_param->delay_time);
		videoEncoder.startRecvStream(CHN0);
		while (THD_STAT_START == p_gs_record_thd_param->thd_stat)
		{

			videoEncoder.getStream(CHN0, Uint8_t* packet, Uint32_t& packetSize, Uint64_t& pts, Int32_t milliSec = -1);
			if (SNAPSHOT_MODE_SINGLE == p_gs_snapshot_thd_param->snapshot_mode)
			{
				//todo 写入sd卡
				break;
			}
			else if (SNAPSHOT_MODE_SINGLE == p_gs_snapshot_thd_param->snapshot_mode)
			{
				//todo
				
			}
			else if (SNAPSHOT_MODE_IDLE == p_gs_snapshot_thd_param->snapshot_mode)
			{
				break;
			}
		}
		if (THD_STAT_STOP == p_gs_record_thd_param->thd_stat)//退出拍照模式
		{
			//todo 退出处理
		}
		videoEncoder.stopRecvStream(Int32_t channel);
	}
	return 0;
}

int snapshot_thread_create()
{
	if (0 != pthread_create(&p_gs_snapshot_thd_param->pid, NULL, snapshot_thread, p_gs_snapshot_thd_param))
	{
		perror("pthread_create failed:");
		printf("pthread_create get buffer thread failed\n");
		return ERROR;
	}
	pthread_setname_np(p_gs_snapshot_thd_param->pid, "snapshot\0");
	gs_wd_thd_param.thd_stat = THD_STAT_RUNNING;

	return OK;

}


//连拍
int snapshot_for_series_stop()
{
	pthread_mutex_lock(&p_gs_snapshot_thd_param->mutex);
	p_gs_snapshot_thd_param->thd_stat = THD_STAT_STOP;
	pthread_mutex_unlock(&p_gs_snapshot_thd_param->mutex);

}

int snapshot_for_series_start()
{
	sem_post(&p_gs_record_thd_param->wake_sem);
}

int snapshot_for_once()
{
	sem_post(&p_gs_record_thd_param->wake_sem);
}


int snapshot_param_init()
{
	p_gs_snapshot_thd_param = (p_record_thread_params_s)malloc(sizeof(RECORD_THREAD_PARAMS_S));
	if (NULL == p_gs_snapshot_thd_param)
	{
		perror("gs_record_thd_param malloc failed:");
		printf("record_param_init failed\n");
		return -ERROR；
	}
	p_gs_snapshot_thd_param->delay_time = 0;
	p_gs_snapshot_thd_param->pid = PID_NULL;
	p_gs_snapshot_thd_param->thd_stat = THD_STAT_IDLE;
	p_gs_snapshot_thd_param->exposure_mode = EXPOSURE_MODE_AUTO; //默认自动曝光
	p_gs_snapshot_thd_param->exposure_time = EXPOSURE_TIME_AUTO;
	p_gs_snapshot_thd_param->snapshot_mode = SNAPSHOT_MODE_IDLE;
	pthread_mutex_init(&p_gs_snapshot_thd_param->mutex, NULL);
	sem_init(&(p_gs_snapshot_thd_param->wake_sem),0,0);//第二个如果不为0代表进程间共享sem，第三个代表sem值的初始值

	return OK;
}


int record_param_exit()
{
	sem_destroy(&p_gs_snapshot_thd_param->wake_sem);
	pthread_mutex_destory(&p_gs_snapshot_thd_param->mutex);
	if (NULL != p_gs_snapshot_thd_param)
	{
		free(p_gs_snapshot_thd_param);
	}

	return 0;
}

int snapshot_thread_destory()
{
	record_enable_flag = FALSE;
	if (p_gs_snapshot_thd_param->pid != PID_NULL)
	{
		pthread_join(p_gs_snapshot_thd_param->pid, 0);
	}

	return OK;
}

int snapshot_module_init()
{
	record_param_init();
	record_thread_start();

	return 0;
}

int snapshot_module_exit()
{
	record_thread_stop();
	record_param_exit();

	return 0;
}

/*
延时拍照设置
*/
int snapshot_set_delay_time(int time)
{
	//todo
	pthread_mutex_lock(&p_gs_snapshot_thd_param->mutex);
	p_gs_snapshot_thd_param->delay_time = time;
	pthread_mutex_unlock(&p_gs_snapshot_thd_param->mutex);
	return 0;
}

void snapshot_set_exposure_mode(EXPOSURE_MODE_E mode, EXPOSURE_TIME_E time);//设置曝光时间
{
	//todo

}

