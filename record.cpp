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


#include <stdlib.h>


using namespace detu_media;
#define CHN0 0
#define ERROR -1
#define OK 0
#define TRUE 1
#define FALSE 0

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
static p_record_thread_params_s p_gs_record_thd_param;

#define RECORD_DIVIDE_TIME (20 * 60) //second
#define PID_NULL			((pid_t)(-1))


void *record_thread(void *p)
{
/*
	int ret = 0;
	GB_THREAD_PARAMS_S *parameter;
	parameter = (GB_THREAD_PARAMS_S *)p;
	sem_wait(wake_sem);
start_record:
	if (record_start_flag)
	{
		//todo
		while(record_start_flag)
		{
			//todo
		}
		sem_post(&sem);
	}
	else
	{
		pthread_cond_wait();
		goto start_record;
	}
	return ret;
*/
	int ret = 0;
//	p_record_thread_params_s parameter;
//	parameter = (p_record_thread_params_s *)p;

	VideoEncodeMgr& videoEncoder = *VideoEncodeMgr::instance();
	Uint8_t *packet = NULL;
	Uint32_t packetSize = 500*1024;
	Uint32_t tmp = 0;
	int i = 0;
	Uint64_t pts = 0;
	packet = (Uint8_t *)malloc(500*1024);
	memset(packet, 0, 500*1024);
	
	FILE *fp = NULL; /* 需要注意 */
	FILE *fp_size = NULL;
	FILE *fp_size_div = NULL;


	int result = 0, nread = 0;
	Uint64_t start_time = 0;
	fd_set inputs, testfds;
	struct timeval timeout;
	int fd;
	videoEncoder.getFd(CHN0, fd);
	printf("fd:%d\n", fd);
#if 1	
	while (record_enable_flag)
	{
		sem_wait(&p_gs_record_thd_param->wake_sem);
		sleep(p_gs_record_thd_param->delay_time);
		videoEncoder.startRecvStream(CHN0);
		printf("start recv\n");

		while (THD_STAT_START == p_gs_record_thd_param->thd_stat)
		{
			testfds = inputs;   
			timeout.tv_sec = 2;	 
			timeout.tv_usec = 500000;   
			FD_ZERO(&inputs);//用select函数之前先把集合清零	  
			FD_SET(fd,&inputs);//把要监测的句柄——fd,加入到集合里。 
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
						//printf("%s, %d\n", __func__, __LINE__);
					
						videoEncoder.getStream(CHN0, packet, packetSize, pts);
						//combine_slice_to_frame(package);//组合多个package成完整的一帧
#ifdef MP4
						if (0 == start_time)
						{
							start_time = pts;
							//todo write MP4 header
						}
						else if (parameter->divide_time <= pts -start_time)//分时功能
						{
							//todo close MP4
							start_time = pts;
							//todo write MP4 header;
						}
						else
						{
							//todo write video frame to MP4 file;
						}
					}
					break;
#else
						if (0 == start_time)
						{
							fp = fopen("h265_test.h265", "wb");
							fp_size = fopen("265size.txt", "w");
							fp_size_div = fopen("265size_div.txt", "w");
							
							start_time = pts;
							//todo write MP4 header
						}
						if (packetSize > 50)
						{
							if ( 3 == i)
							{
								i = 0;
								fprintf(fp_size, "%d\n", (packetSize + tmp));
								tmp = 0;
							}
							else
							{
								fprintf(fp_size, "%d\n", packetSize);
							}
							fprintf(fp_size_div, "%d\n", packetSize);
						}
						else if(((++i)%4))
						{
							tmp += packetSize;
							printf("size :%d\n", tmp);
							fprintf(fp_size_div, "%d\n", packetSize);
						}
						fwrite(packet,packetSize,1,fp);
						if ((pts - start_time) > (10*1000*1000))
						{
							videoEncoder.stopRecvStream(CHN0);
							fclose(fp);
							fclose(fp_size);
							fclose(fp_size_div);
							p_gs_record_thd_param->thd_stat = THD_STAT_STOP;
							break;
						}
						packetSize = 500*1024;
						memset(packet, 0, packetSize);
					}
					break;
#endif
			}
		}

		videoEncoder.stopRecvStream(CHN0);
		break;
		if (THD_STAT_STOP == p_gs_record_thd_param->thd_stat)//录像结束关闭MP4文件
		{
			//todo close MP4
		}
	}
#endif
	return 0;
}

int record_thread_create()
{
/*
    int ret = 0;
	char p1, p2;
    pthread_t get_buf, write_disk;
	sem_init(&sem,0,0);//第二个如果不为0代表进程间共享sem，第三个代表sem值的初始值
    ret = pthread_create(&get_buf, NULL, get_buf_thread, &p1);
	ret |= pthread_create(&write_disk, NULL, write_disk_thread, &p2);
    if (ret != 0)
        printf("create thread failed: %s\n", strerror(err));

	return ret;
*/


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

int record_thread_stop()
{
	pthread_mutex_lock(&p_gs_record_thd_param->mutex);
	p_gs_record_thd_param->thd_stat = THD_STAT_STOP;
	pthread_mutex_unlock(&p_gs_record_thd_param->mutex);

}

int record_thread_start()
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

	return 0;
}

int record_thread_destroy()
{
	record_enable_flag = FALSE;
	if (p_gs_record_thd_param->pid != PID_NULL)
	{
		pthread_join(p_gs_record_thd_param->pid, 0);
	}

	return OK;
}

int record_module_init()
{
	record_param_init();
	record_thread_start();

	return 0;
}

int record_module_exit()
{
	record_thread_stop();
	record_param_exit();

	return 0;
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
	return 0;
}

/*
延时录像设置
*/
int record_set_divide_time()
{
	//暂无需求
}
