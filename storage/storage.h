#ifndef _STORAGE_MODULE_H_
#define _STORAGE_MODULE_H_

#ifdef __cplusplus
#if __cplusplus
extern "C"
{
#endif
#endif

#include "types.h"
#define _LARGEFILE_SOURCE		1
#define _LARGEFILE64_SOURCE		1
#define _FILE_OFFSET_BITS		64

#define DEV_NAME "/dev/mmcblk1p1"

#define MOUNT_DIR "/mnt/sd_card/"


#define BAK_DEBUG(format, args...)	printf("%s, %dL: "format, __FUNCTION__, __LINE__, ##args)








/*
	获取sd卡的信息，设备名，路径，容量
*/

int storage_dev_info_init();

int storage_get_free_space();

int storage_mount_sdcard(char *mountpoint, char *devname);

int storage_umount_sdcard(char *mountpoint);




/******************************************************************************
 * ��������:	bak_disk_format
 * ��������:	usb�豸��ʽ��	.����:
 				1> ��ʽ����������
 				2> ��ʽ��fsinfo ��Ϣ
 				3> ��ʽ��fat��
 				4> ��ʽ����Ŀ¼
 * ��    ��: 	�豸����ָ��,����ֵ
 * ��    ��: 	��
 * �� �� ֵ: 	������KB	
 ******************************************************************************/
int bak_disk_format(char *devname, int capacity);



#ifdef __cplusplus
}
#endif  /* #endif __cplusplus */


#endif  /* _CAL_H_ */


