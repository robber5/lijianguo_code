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
	鑾峰彇sd鍗＄殑淇℃伅锛岃澶囧悕锛岃矾寰勶紝瀹归噺
*/

int storage_dev_info_init();

int storage_get_free_space();

int storage_mount_sdcard(char *mountpoint, char *devname);

int storage_umount_sdcard(char *mountpoint);




/******************************************************************************
 * 函数名称:	bak_disk_format
 * 函数描述:	usb设备格式化	.步骤:
 				1> 格式化引导分区
 				2> 格式化fsinfo 信息
 				3> 格式化fat表
 				4> 格式化根目录
 * 输    入: 	设备名称指针,容量值
 * 输    出: 	无
 * 返 回 值: 	总容量KB	
 ******************************************************************************/
int bak_disk_format(char *devname, int capacity);



#ifdef __cplusplus
}
#endif  /* #endif __cplusplus */


#endif  /* _CAL_H_ */


