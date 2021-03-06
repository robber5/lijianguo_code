#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <asm/types.h>
//该头文件需要放在netlink.h前面防止编译出现__kernel_sa_family未定义
#include <sys/socket.h>
#include <linux/netlink.h>
#include <unistd.h>

#include "config_manager.h"
#include "defs.h"
#include "storage.h"

#define BLK_NAME "mmcblk1p1"
#define REMOVE "remove@"
#define ADD "add@"

static pthread_t s_hotplug_pid = -1;
static pthread_t s_capacity_pid = -1;

using namespace detu_config_manager;


void *storage_sdcard_capacity_monitor(void *arg)
{
	Json::Value reccfg, snapcfg, response;
	ConfigManager& config = *ConfigManager::instance();
	unsigned int mbFreedisk, mbTotalsize;
	float percent;
	while(1)
	{
		if (S_ERROR == storage_sdcard_mount_check())
		{
			mbFreedisk = 0;
			mbTotalsize = 0;
			percent = 0;
		}
		else
		{
			storage_sdcard_capacity_info(&mbFreedisk, &mbTotalsize, &percent);
		}
		if (FREE_SPACE_MARK > percent)
		{

			config.getTempConfig("record.status.value", reccfg, response);
			if (!(reccfg.asString()).compare("start"))
			{
				reccfg = "stop";
				config.setTempConfig("record.status.value", reccfg, response);
			}
			config.getTempConfig("snapshot.status.value", snapcfg, response);
			if (!(reccfg.asString()).compare("start"))
			{
				snapcfg = "stop";
				config.setTempConfig("snapshot.status.value", snapcfg, response);
			}
			printf("sd_card free space is smaller than %d%%\n", FREE_SPACE_MARK);
		}
		usleep(10*1000*1000);
	}

}

void *storage_sdcard_hotplug_monitor(void *arg)
{
	Json::Value reccfg, snapcfg, response;
	ConfigManager& config = *ConfigManager::instance();

	int sdcard_status = -1;//0:add,1:remove,-1:initial value
    int sockfd;
    struct sockaddr_nl sa;
    int len;
    char buf[4096];
    struct iovec iov;
    struct msghdr msg;
//	int i;

    memset(&sa,0,sizeof(sa));
    sa.nl_family=AF_NETLINK;
    sa.nl_groups=NETLINK_KOBJECT_UEVENT;
    sa.nl_pid = 0;//getpid(); both is ok
    memset(&msg,0,sizeof(msg));
    iov.iov_base=(void *)buf;
    iov.iov_len=sizeof(buf);
    msg.msg_name=(void *)&sa;
    msg.msg_namelen=sizeof(sa);
    msg.msg_iov=&iov;
    msg.msg_iovlen=1;

    sockfd=socket(AF_NETLINK,SOCK_RAW,NETLINK_KOBJECT_UEVENT);
    if(sockfd==-1)
    {
        printf("socket creating failed:%s\n",strerror(errno));
    }
    if(bind(sockfd,(struct sockaddr *)&sa,sizeof(sa))==-1)
    {
        printf("bind error:%s\n",strerror(errno));
    }
	while(1)
	{
	    len=recvmsg(sockfd,&msg,0);
	    if(len<0)
	        printf("receive error\n");
	    else if(len<32||len>(int)sizeof(buf))
	        printf("invalid message");
/*
	    for(i=0;i<len;i++)
	    {
	        if(*(buf+i)=='\0')
	        {
	            buf[i]=',';
	        }
	    }
*/
		if (NULL != strstr(buf, BLK_NAME))
		{
			if ((NULL != strstr(buf, ADD)) && (0 != sdcard_status))
			{
				sdcard_status = 0;
				printf("insert the sd_card\n");
			}
			else if ((NULL != strstr(buf, REMOVE)) && ( 1 != sdcard_status))
			{
				sdcard_status = 1;
				config.getTempConfig("record.status.value", reccfg, response);
				if (!(reccfg.asString()).compare("start"))
				{
					reccfg = "stop";
					config.setTempConfig("record.status.value", reccfg, response);
				}
				config.getTempConfig("snapshot.status.value", snapcfg, response);
				if (!(reccfg.asString()).compare("start"))
				{
					snapcfg = "stop";
					config.setTempConfig("snapshot.status.value", snapcfg, response);
				}
				//printf("remove the sd_card\n");
			}
		}

	    //printf("received %d bytes\n%s\n",len,buf);
		memset(buf, 0, 4096 * sizeof(char));
    }
}

S_Result storage_create_monitor_thread(void)
{
	if (0 != pthread_create(&s_hotplug_pid, NULL, storage_sdcard_hotplug_monitor, NULL))
	{
		perror("pthread_create failed");
		printf("pthread_create get frame thread failed\n");
		return S_ERROR;
	}
	pthread_setname_np(s_hotplug_pid, "s_hotplug\0");

	if (0 != pthread_create(&s_capacity_pid, NULL, storage_sdcard_capacity_monitor, NULL))
	{
		perror("pthread_create failed");
		printf("pthread_create get frame thread failed\n");
		return S_ERROR;
	}
	pthread_setname_np(s_capacity_pid, "s_capacity\0");


	return S_OK;
}

S_Result storage_destroy_monitor_thread(void)
{
	if ((unsigned int)-1 != s_hotplug_pid)
	{
		pthread_cancel(s_hotplug_pid);
		pthread_join(s_hotplug_pid, 0);
	}
	if ((unsigned int)-1 != s_capacity_pid)
	{
		pthread_cancel(s_capacity_pid);
		pthread_join(s_capacity_pid, 0);
	}

	return S_OK;
}


