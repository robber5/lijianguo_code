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

using namespace detu_config_manager;
using namespace detu_media;

#define TRUE 1
#define FALSE 0

typedef enum
{
	THD_STAT_START,
	THD_STAT_STOP,
	THD_STAT_QUIT,
	THD_STAT_MAX,
} THD_STAT_E;

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

typedef struct snapshot_config
{
	unsigned int delay_time; //延时录像时间
	THD_STAT_E thd_stat;
	unsigned int chn;
	SNAPSHOT_MODE_E snapshot_mode;

} SNAPSHOT_USER_CONFIG_S;

typedef struct snapshot_params
{
//	EXPOSURE_MODE_E exposure_mode; //曝光模式
//	EXPOSURE_TIME_E exposure_time; //曝光时长
	SNAPSHOT_MODE_E snapshot_mode; //拍照模式；单拍，连拍
	unsigned int delay_time; //延时拍照
	unsigned int chn;//抓图通道
	pthread_t           pid;
	volatile THD_STAT_E thd_stat;
	sem_t               wake_sem;//拍照开关信号量
	SNAPSHOT_COND_S		snapshot_cond;
	pthread_mutex_t     mutex;

} SNAPSHOT_PARAMS_S, *p_snapshot_thread_params_s;


static int snapshot_enable_flag = TRUE;
static p_snapshot_thread_params_s p_gs_snapshot_thd_param;

#define VALUE "value"
#define SNAPSHOT_M "snapshot"
#define SNAPSHOT_STATUS "status"
#define DELAY_TIME "delay_time"
#define PIC_CHN "chn"
#define SNAPSHOT_MODE "snapshot_mode"

#define CHN_COUNT 6
#define CHN_NUM_MAX 7

#define PACKET_SIZE_MAX (10*1024*1024)

static char s_snapshot_status[THD_STAT_MAX][16] = {"start", "stop", "quit"};
static char s_snapshot_mode[SNAPSHOT_MODE_MAX][16] = {"single", "series"};
static char s_snapshot_chn[CHN_NUM_MAX][16] = {CH0_VIDEO_DIR, CH1_VIDEO_DIR, CH2_VIDEO_DIR, CH3_VIDEO_DIR, AVS_VIDEO_DIR, AVS_1080P, ALLCHN};

static char s_picture_dir[CHN_COUNT][16] = {CH0_VIDEO_DIR, CH1_VIDEO_DIR, CH2_VIDEO_DIR, CH3_VIDEO_DIR, AVS_VIDEO_DIR, AVS_VIDEO_DIR};


#define PID_NULL			((pthread_t)(-1))

#define PIC_COUNT_MIN 35

static S_Result snapshot_thread_start(SNAPSHOT_USER_CONFIG_S usercfg);

static S_Result snapshot_thread_stop();

static S_Result record_get_jpeg_filename(char *filename, int index)
{
	time_t time_seconds = time(NULL);
	struct tm local_time;
	localtime_r(&time_seconds, &local_time);

	snprintf(filename, FILE_NAME_LEN_MAX, "%s%s/%d%02d%02d%02d%02d%02d-%s.jpg", MOUNT_DIR, s_picture_dir[index], local_time.tm_year + 1900, local_time.tm_mon + 1,
		local_time.tm_mday, local_time.tm_hour, local_time.tm_min, local_time.tm_sec, s_snapshot_chn[index]);

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

static void *snapshot_thread(void *p)
{
	Uint32_t pic_count = 0;
	fd_set inputs, testfds;
	struct timeval timeout;
	int result = 0;
	Uint8_t *packet = NULL;
	Uint32_t packetSize = PACKET_SIZE_MAX;
	Uint64_t pts;
	int fd[CHN_COUNT];
	int fd_found[CHN_COUNT];
	Uint32_t chn[CHN_COUNT];
	Uint32_t chnnum = 0;
	Uint32_t curchn = 0;
	Uint32_t seq = 0u;
	char filename[FILE_NAME_LEN_MAX];
	FILE *fp = NULL;
	Uint32_t remain_chn_num = 0, i = 0, j = 0;
	int fd_max = -1;
	int chn_status[CHN_COUNT] = {0};//0:stop,1:start

	packet = (Uint8_t *)malloc(PACKET_SIZE_MAX*sizeof(Uint8_t));
	memset(packet, 0, PACKET_SIZE_MAX);
	VideoEncodeMgr& videoEncoder = *VideoEncodeMgr::instance();

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
		chnnum = p_gs_snapshot_thd_param->chn;
		if (CHN_COUNT > chnnum)
		{
			chn[0] = chnnum;
			videoEncoder.startRecvStream(chn[0]);
			videoEncoder.getFd(chn[0], fd[0]);
			FD_ZERO(&inputs);
			FD_SET(fd[0],&inputs);
			remain_chn_num = 1;
			fd_max = fd[0];
			chn_status[0] = 1;
		}
		else
		{
			FD_ZERO(&inputs);
			for (i = 0; i < CHN_COUNT; i++)
			{
				chn[i] = i;
				videoEncoder.startRecvStream(chn[i]);
				videoEncoder.getFd(chn[i], fd[i]);
				FD_SET(fd[i],&inputs);
				chn_status[i] = 1;
			}
			remain_chn_num = CHN_COUNT;
			fd_max = get_max_fd(fd, CHN_COUNT);
			printf("start allchn\n");
		}

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
					for (i = 0, j = 0; i < CHN_COUNT; i++)
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
						curchn = fd_found[i];
						packetSize = PACKET_SIZE_MAX;
						videoEncoder.getStream(curchn, seq, packet, packetSize, pts);

						if (PIC_COUNT_MIN > pic_count)
						{
							if (0 == curchn)
							{
								pic_count++;
							}
							continue;
						}
						else if (0 == chn_status[curchn])
						{
							continue;
						}
						record_get_jpeg_filename(filename, curchn);
						if (SNAPSHOT_MODE_SINGLE == p_gs_snapshot_thd_param->snapshot_mode)
						{
							//todo 写入sd卡
							if (NULL == (fp = fopen(filename, "wb")))
							{
								printf("can not open jpeg file:%s\n", filename);
								break;
							}
							fwrite(packet, sizeof(char), packetSize, fp);
							printf("remain chn:%d\n", remain_chn_num);
							printf("create file:%s\n", filename);
							fflush(fp);
							fclose(fp);
							FD_CLR(fd[curchn],&inputs);
							chn_status[curchn] = 0;
							if (0 == (--remain_chn_num))
							{
								FD_ZERO(&inputs);
								pthread_mutex_lock(&p_gs_snapshot_thd_param->snapshot_cond.mutex);
								if (FALSE == p_gs_snapshot_thd_param->snapshot_cond.wake)
								{
									p_gs_snapshot_thd_param->snapshot_cond.wake = TRUE;
									pthread_cond_broadcast(&p_gs_snapshot_thd_param->snapshot_cond.cond);//通知拍照命令执行完成
								}
								pthread_mutex_unlock(&p_gs_snapshot_thd_param->snapshot_cond.mutex);
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
			if (CHN_COUNT > chnnum)
			{
				videoEncoder.stopRecvStream(chn[0]);
			}
			else
			{
				for (i = 0; i < CHN_COUNT; i++)
				{
					videoEncoder.stopRecvStream(chn[i]);
				}

			}

			pthread_mutex_lock(&p_gs_snapshot_thd_param->snapshot_cond.mutex);
			if (TRUE == p_gs_snapshot_thd_param->snapshot_cond.wake)
			{
				p_gs_snapshot_thd_param->snapshot_cond.wake = FALSE;
				pthread_cond_broadcast(&p_gs_snapshot_thd_param->snapshot_cond.cond);//通知拍照模块已处于stop状态
			}
			pthread_mutex_unlock(&p_gs_snapshot_thd_param->snapshot_cond.mutex);
		}

	}

	free(packet);

	return (void *)S_OK;
}

static S_Result snapshot_trans_config(const Json::Value& config,SNAPSHOT_USER_CONFIG_S& usercfg)
{
	int i = 0;

	for (i = 0; i < THD_STAT_MAX; i++)
	{
		if (!(config[SNAPSHOT_STATUS][VALUE].asString()).compare(s_snapshot_status[i]))
		{
			usercfg.thd_stat = (THD_STAT_E)i;
			break;
		}
	}
	for (i = 0; i < CHN_NUM_MAX; i++)
	{
		if (!(config[PIC_CHN][VALUE].asString()).compare(s_snapshot_chn[i]))
		{
			usercfg.chn = i;
			break;
		}
	}
	for (i = 0; i < SNAPSHOT_MODE_MAX; i++)
	{
		if (!(config[SNAPSHOT_MODE][VALUE].asString()).compare(s_snapshot_mode[i]))
		{
			usercfg.snapshot_mode = (SNAPSHOT_MODE_E)i;
			break;
		}
	}

	usercfg.delay_time = config[DELAY_TIME][VALUE].asUInt();

	return S_OK;

}


static S_Result snapshot_check_config(const Json::Value& config)
{
	int delay_time = 0;
	int i = 0;
	S_Result S_ret = S_OK;
	delay_time = config[DELAY_TIME][VALUE].asInt();
	do
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
		for (i = 0; i < CHN_NUM_MAX; i++)
		{
			if (!(config[PIC_CHN][VALUE].asString()).compare(s_snapshot_chn[i]))
			{
				break;
			}
		}
		if (CHN_NUM_MAX == i)
		{
			S_ret = S_ERROR;
			printf("chn error\n");
			break;
		}
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

	snapshot_trans_config(newConfig, newcfg);
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
					config.getConfig("snapshot.status.value", snapcfg, response);
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
	enable_pic_mode();
	pthread_mutex_lock(&p_gs_snapshot_thd_param->snapshot_cond.mutex);
	if (THD_STAT_START != p_gs_snapshot_thd_param->thd_stat)
	{
		p_gs_snapshot_thd_param->thd_stat = THD_STAT_START;
		p_gs_snapshot_thd_param->snapshot_mode = usercfg.snapshot_mode;
		p_gs_snapshot_thd_param->delay_time = usercfg.delay_time;
		p_gs_snapshot_thd_param->chn = usercfg.chn;
		pthread_cond_broadcast(&p_gs_snapshot_thd_param->snapshot_cond.cond);
		do
		{
			pthread_cond_wait(&p_gs_snapshot_thd_param->snapshot_cond.cond, &p_gs_snapshot_thd_param->snapshot_cond.mutex);
			if ((TRUE == p_gs_snapshot_thd_param->snapshot_cond.finish) && (FALSE == p_gs_snapshot_thd_param->snapshot_cond.wake))
			{
				p_gs_snapshot_thd_param->snapshot_cond.finish = FALSE; //置为初始值
				disable_pic_mode();
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
	disable_pic_mode();
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

	config.getConfig("snapshot.status.value", snapCfg, response);
	snapCfg = "stop";
	config.setTempConfig("snapshot.status.value", snapCfg, response);

	config.getConfig(SNAPSHOT_M, snapCfg, response);

	snapshot_trans_config(snapCfg, usercfg);

	p_gs_snapshot_thd_param = (p_snapshot_thread_params_s)malloc(sizeof(SNAPSHOT_PARAMS_S));
	if (NULL == p_gs_snapshot_thd_param)
	{
		perror("gs_record_thd_param malloc failed:");
		printf("record_param_init failed\n");
		return S_ERROR;
	}
	p_gs_snapshot_thd_param->delay_time = usercfg.delay_time;
	p_gs_snapshot_thd_param->pid = PID_NULL;
	p_gs_snapshot_thd_param->thd_stat = usercfg.thd_stat;
//	p_gs_snapshot_thd_param->exposure_mode = EXPOSURE_MODE_AUTO; //默认自动曝光
//	p_gs_snapshot_thd_param->exposure_time = EXPOSURE_TIME_AUTO;
	p_gs_snapshot_thd_param->snapshot_mode = usercfg.snapshot_mode;
	p_gs_snapshot_thd_param->chn = usercfg.chn;
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


