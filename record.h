#ifndef RECORD_MODULE_
#define RECORD_MODULE_

#include "record_search.h"

//#define RECORD

void record_thread_start();
void record_thread_stop();
int record_set_delay_time(int time);


int record_module_init();
int record_module_exit();

#endif

