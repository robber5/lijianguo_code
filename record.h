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

#define RECORD

typedef enum
{
	RECORD_MODE_SINGLE,
	RECORD_MODE_MULTI,

} RECORD_MODE_E;


typedef enum
{
	CODEC_H264 = 96,
	CODEC_H265 = 265,

} CODEC_TYPE_E;


void record_thread_start();
void record_thread_stop();
int record_set_delay_time(int time);
void record_thread_restart();
int record_set_record_mode(RECORD_MODE_E mode);
int record_set_encode_type(CODEC_TYPE_E entype);

int record_module_init();
int record_module_exit();

#endif

