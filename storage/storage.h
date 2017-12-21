#ifndef _STORAGE_MODULE_H_
#define _STORAGE_MODULE_H_

#ifdef __cplusplus
#if __cplusplus
extern "C"
{
#endif
#endif

#include "defs.h"

#define _LARGEFILE_SOURCE		1
#define _LARGEFILE64_SOURCE		1
#define _FILE_OFFSET_BITS		64

#define DEV_NAME "/dev/mmcblk1p1"

#define MOUNT_DIR "/mnt/sd_card/"


#define CH0_VIDEO_DIR "chn0"
#define CH1_VIDEO_DIR "chn1"
#define CH2_VIDEO_DIR "chn2"
#define CH3_VIDEO_DIR "chn3"
#define AVS_VIDEO_DIR "avs"

#define BAK_DEBUG(format, args...)	printf("%s, %dL: "format, __FUNCTION__, __LINE__, ##args)








/*
	获取sd卡的信息，设备名，路径，容量
*/


int storage_sdcard_mount(void);

int storage_sdcard_umount(void);

S_Result storage_sdcard_check(void);


#ifdef __cplusplus
}
#endif  /* #endif __cplusplus */


#endif  /* _CAL_H_ */


