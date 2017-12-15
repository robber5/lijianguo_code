#ifndef SNAPSHOT_MODULE
#define SNAPSHOT_MODULE

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
	SNAPSHOT_MODE_IDLE,
	SNAPSHOT_MODE_SINGLE,
	SNAPSHOT_MODE_SERIES,
	SNAPSHOT_MODE_MAX,
} SNAPSHOT_MODE_E;


typedef struct snapshot_config
{
	unsigned int delay_time; //延时录像时间
	THD_STAT_E thd_stat;
	unsigned int chn;
	SNAPSHOT_MODE_E snapshot_mode;

} SNAPSHOT_USER_CONFIG_S;


int snapshot_module_exit();
int snapshot_module_init();

void snapshot_thread_start(SNAPSHOT_USER_CONFIG_S usercfg);

void snapshot_thread_stop();

void snapshot_for_once();

int snapshot_set_chn(int chn);



#endif
