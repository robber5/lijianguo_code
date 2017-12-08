#ifndef SNAPSHOT_MODULE
#define SNAPSHOT_MODULE

int snapshot_module_exit();
int snapshot_module_init();

void snapshot_start();

void snapshot_stop();

int snapshot_for_once();

int snapshot_set_chn(int chn);



#endif
