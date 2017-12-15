/*********************************************************************************
  *Copyright(C),Zhejiang Detu Internet CO.Ltd
  *FileName:  record.h
  *Author:  Li Jianguo
  *Version:  1.0
  *Date:  2017.11.15
  *Description:  the record module
**********************************************************************************/
#ifndef RECORD_MODULE_
#define RECORD_MODULE_

#include "record_search.h"

//#define RECORD

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

void record_thread_start(RECORD_USER_CONFIG_S usercfg);
void record_thread_stop(void);
int record_set_delay_time(int time);

int record_set_record_mode(RECORD_MODE_E mode);
int record_set_encode_type(CODEC_TYPE_E entype);
void record_thread_restart(RECORD_USER_CONFIG_S usercfg);
int record_module_init(void);
int record_module_exit(void);

#endif

