#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/statfs.h>

#include "storage.h"

#if 0
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <malloc.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>

#include <sys/mount.h>

#include <pthread.h>
#include <assert.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/statfs.h>

#include <fcntl.h>
#include <linux/hdreg.h>
#include "storage.h"
#include "defs.h"
#include "types.h"

#define eMMC 0
#define TSK_DEF_STACK_SIZE		16384


#define BOOTCODE_SIZE		448
#define BOOTCODE_FAT32_SIZE	420

#define SECTOR_SIZE		512
#define FORMAT_SECTORS		64
#define BUF_SIZE		(FORMAT_SECTORS * SECTOR_SIZE)

#define BUFSIZE		(32 * 1024)

#define DEV_DIR	"/dev"

#define VOLUME_NAME		"DETU"
#define SYSTEM_ID			"LINUX" /*³§ÉÌ±ê¼ÇºÍOS  */
#define KILO			1024

#define MSDOS_EXT_SIGN		0x29
#define MSDOS_FAT12_SIGN	"FAT12   "
#define MSDOS_FAT16_SIGN	"FAT16   "
#define MSDOS_FAT32_SIGN	"FAT32   "


/* attribute bits  ¸ùÄ¿Â¼ÊôĞÔ*/
#define ATTR_RO      1
#define ATTR_HIDDEN  2
#define ATTR_SYS     4
#define ATTR_VOLUME  8
#define ATTR_DIR     16
#define ATTR_ARCH    32
#define ATTR_NONE    0
#define ATTR_UNUSED  (ATTR_VOLUME | ATTR_ARCH | ATTR_SYS | ATTR_HIDDEN)


#define BOOT_SIGN		0xAA55	/* ¹Ì¶¨ĞÅÏ¢ */
#define BOOTCODE_SIZE		448
#define BOOTCODE_FAT32_SIZE	420

/* fsinfo ¹Ì¶¨µÄĞòÁĞºÅ */
#define FAT_FSINFO_SIG1		0x41615252
#define FAT_FSINFO_SIG2		0x61417272


typedef struct tagFAT_FORMAT
{
	Uint8_t	fat32_type;	/* fat32 ±ê¼Ç,==1 ±íÊ¾fat32 */
	Uint8_t	sec_per_clus;/* Ã¿´ØÉÈÇøÊı */
	Uint32_t	sec_per_ftable;	/*  Ò»¸öfat±íÕ¼µÄÉÈÇøÊı*/
	Uint16_t	fat_reserved;	/* ±£ÁôÉÈÇøÊı */
} __attribute__ ((packed))FAT_FORMAT;

typedef struct sd_card
{
	Uint32_t			mmc_type;
	char		device_name[32];
	char		partition_name[32];//
	Uint32_t			capability;
	Uint32_t			remain;
	char		directory[128];
}SD_CARD_INFO_S;



/* FAT32ÎÄ¼şÏµÍ³¾í±êĞÅÏ¢ */
typedef struct msdos_volume_info_t
{
	Uint8_t	drive_number;	/* ÎïÀíÇı¶¯Æ÷ºÅ 1 (Ó²ÅÌÎª0x80) */
	Uint8_t	RESERVED;		/* ±£Áô×Ö¶Î 1 (Ò»°ãÎª0)*/
	Uint8_t	ext_boot_sign;	/*  À©Õ¹Òıµ¼±êÇ© 1 Ò»°ã0X28»ò0X29*/
	Uint8_t	volume_id[4];	/*  ·ÖÇøĞòºÅ 4 (¸ñÊ½µÄÊ±ºòËæ¼´²úÉúµÄĞòºÅ)*/
	Uint8_t	volume_label[11];/* ¾í±ê 11 (ÓÃÀ´±£´æ¾í±êºÅ£¬ÏÖÔÚ±»µ±³ÉÎÄ¼ş±£´æÔÚ¸ùÄ¿Â¼Çø)*/
	Uint8_t	fs_type[8];		/* ÏµÍ³ID FAT32ÖĞÒ»°ãÎª"FAT32"*/
} __attribute__ ((packed))msdos_volume_info;


/* FAT32ÎÄ¼şÏµÍ³Òıµ¼ÉÈÇø */
typedef struct msdos_boot_sector_t
{
	Uint8_t	boot_jump[3];	/* Ìø×ªÖ¸Áî 3 */
	Int8_t   system_id[8];	/* ³§ÉÌ±êÖ¾OS°æ±¾ºÅ 8 * partition manager volumes */

	Uint8_t	sector_size[2];	/*  Ã¿ÉÈÇø×Ö½ÚÊı 2*/
	Uint8_t	cluster_size;		/*  Ã¿´ØÉÈÇøÊı 1 */
	Uint16_t	reserved;		/*  ±£ÁôÉÈÇøÊı 2 (Ò»°ãÎª32) */
	Uint8_t	fats;			/*  FAT Êı (Ò»°ãÎª2)*/
	Uint8_t	dir_entries[2];	/* ¸ùÄ¿Â¼Êı 2(FAT12ºÍFAT16ÖĞÊ¹ÓÃ¸Ã×Ö¶Î£¬FAT32ÖĞ±ØĞëÎª0)*/
	Uint8_t	sectors[2];		/* Ğ¡ÉÈÇøÊı 2 (FAT12ºÍFAT16ÖĞÊ¹ÓÃ¸Ã×Ö¶Î£¬FAT32ÖĞ±ØĞëÎª0)*/
	Uint8_t	media;			/* Ã½ÌåÃèÊö·û 1 Ö÷ÒªÓÃÓÚFAT16 0XF8±íÊ¾Ó²ÅÌ 0XF0±íÊ¾¸ßÃÜ¶È3.5´çÈíÅÌ*/
	Uint16_t	fat_length;		/* Ã¿FATÉÈÇøÊı 2 (FAT12ºÍFAT16ÖĞÊ¹ÓÃ¸Ã×Ö¶Î£¬FAT32ÖĞ±ØĞëÎª0*/
	Uint16_t	secs_track;		/*  Ã¿µÀÉÈÇøÊı 2 */
	Uint16_t	heads;			/*  ´ÅÍ·Êı 2*/
	Uint32_t	hidden;			/* Òş²ØÉÈÇøÊı 4 ¸Ã·ÖÇøÒıµ¼ÉÈÇøÖ®Ç°µÄÉÈÇøÊı*/
	Uint32_t	total_sect;		/*   ×ÜÉÈÇøÊı 4*/
	struct
	{
			Uint32_t	fat32_length;	/*  Ã¿FATÉÈÇøÊı 4 (Ö»±»FAT32Ê¹ÓÃ)*/
			Uint16_t	flags;			/*  À©Õ¹±êÖ¾ 2 (Ö»±»FAT32Ê¹ÓÃ) */
			Uint8_t	version[2];		/* ÎÄ¼şÏµÍ³°æ±¾ 2 (Ö»±»FAT32Ê¹ÓÃ)*/
			Uint32_t	root_cluster;		/*  ¸ùÄ¿Â¼µÚÒ»´ØµÄ´ØºÅ4 (Ö»±»FAT32Ê¹ÓÃ,Ò»°ãÎª2)*/
			Uint16_t	info_sector;		/* ÎÄ¼şÏµÍ³ĞÅÏ¢ÉÈÇøºÅ2*/
			Uint16_t	backup_boot;	/* ±¸·İÒıµ¼ÉÈÇøÉÈÇøºÅ 2 (Ò»°ãÎª6)*/
			Uint16_t	reserved2[6];	/* Unused ±£Áô×Ö¶Î 12*/
			msdos_volume_info vi;
			Uint8_t	boot_code[BOOTCODE_FAT32_SIZE];
	} __attribute__ ((packed)) fat32;

	Uint16_t	boot_sign;

} __attribute__ ((packed))msdos_boot_sector;



typedef struct fat_boot_fsinfo_t
{
	Uint32_t	signature1;		/* 0x41615252L  ¹Ì¶¨*/
	Uint32_t	reserved1[120];	/* 480 Bytes ±£Áô*/
	Uint32_t	signature2;		/* 0x61417272 ¹Ì¶¨ */
	Uint32_t	free_clusters;	/*Ê£Óà´Ø×ÜÊı  -1 ÊÇÎ´Öª*/
	Uint32_t	next_cluster;	/* ×îĞÂ·ÖÅä´ØºÅ */
	Uint8_t	reserved2[14];
	Uint16_t	boot_sign;
} __attribute__ ((packed))fat_boot_fsinfo;


/* fat32 Ä¿Â¼Ïî32×Ö½Ú±íÊ¾¶¨Òå*/
typedef   struct msdos_dir_entry_t
{
	Uint8_t	name[8], ext[3];	/* name and extension */
	Uint8_t	attr;			/* attribute bits */
	Uint8_t	lcase;			/* Case for base and extension */
	Uint8_t	ctime_ms;		/* Creation time, milliseconds */
	Uint16_t	ctime;			/* Creation time */
	Uint16_t	cdate;			/* Creation date */
	Uint16_t	adate;			/* Last access date */
	Uint16_t	starthi;			/* high 16 bits of first cl. (FAT32) */
	Uint16_t	time, date, start;	/* time, date and first cluster */
	Uint32_t	size;			/* file size (in bytes) */
} __attribute__ ((packed))msdos_dir_entry;


#define MMC_DEV "mmcblk"
static SD_CARD_INFO_S s_sd_card;
static int	s_devfd = -1;
static FAT_FORMAT  s_format = {0};
static Uint8_t s_fatbuf[BUF_SIZE];


static int storage_sdcard_dev_init(char *devname)
{
	if ((s_devfd = open(devname, O_RDWR)) < 0)
	{
		printf("%s \n", devname);
		perror("open");
		return S_ERROR;
	}

	lseek(s_devfd, 0, SEEK_SET);

	return S_OK;
}

static int storage_sdcard_dev_release(void)
{
	int ret = 0;

	if (s_devfd >= 0)
	{
		ret = close(s_devfd);
		s_devfd = -1;
	}

	return ret;
}

/*
int storage_sdcard_read_part_table(char *devname)
{
	if (storage_sdcard_dev_init(devname) < 0)
	{
		return S_ERROR;
	}

	ioctl(s_devfd, BLKRRPART, NULL);
	sync();

	storage_sdcard_dev_release();

	return S_OK;
}

*/
static int storage_fat_get_capacity(void)
{
	int total_sect = 0;

	if (ioctl(s_devfd, BLKGETSIZE, &total_sect) < 0)
	{
		perror("BLKGETSIZE");
		return S_ERROR;
	}
	return total_sect;
}


/*
	è·å–sdå¡çš„ä¿¡æ¯ï¼Œè®¾å¤‡åï¼Œè·¯å¾„ï¼Œå®¹é‡
*/

int storage_dev_info_init()
{

	char line[64], tmp[16];
	char *sdcardname;
	FILE *fp;
	int partition_num = 0;
	SD_CARD_INFO_S *sdcard = &s_sd_card;

	if ((fp = fopen("/proc/partitions", "r")) == NULL)
	{
		fprintf(stderr, "open /proc/partitions fail\n");
		return -1;
	}

	strncpy(tmp, MMC_DEV, sizeof(tmp));
	while (fgets(line, sizeof(line), fp))
	{
		sdcardname = strstr(line, tmp);
		if (NULL == sdcardname)
		{
			continue;
		}
		if (NULL != strstr(sdcardname, "p")) //æ˜¯å¦æ˜¯åˆ†åŒºèŠ‚ç‚¹
		{
			if (NULL != strstr(sdcardname, "rpmb"))
			{
				sdcard->mmc_type = eMMC;
			}

			if (NULL == strstr(sdcardname, "p1"))
			{
				continue;
			}
			int major, minor, blocks;

			sscanf(line, "%d %d %d", &major, &minor, &blocks);

			sdcard->capability = blocks;
			sdcard->remain = blocks;
			sprintf(sdcard->partition_name, "%s%s", DEV_DIR, sdcardname);
			strcpy(sdcard->directory, MOUNT_DIR);

		}
		else
		{
			strcpy(sdcard->device_name, sdcardname);
		}
	}

	fclose(fp);

	return 0;

}


static int cal_free_clus(int fat_bits, int fat_rsv_sec, int sec_per_ftab)
{
	int ret, i, j;
	int left, once;
	int free_clus = 0;

	ret = lseek(s_devfd, fat_rsv_sec*SECTOR_SIZE, SEEK_SET);
	if (ret < 0)
	{
		perror("lseek");
		return S_ERROR;
	}

	for (left = sec_per_ftab; left > 0; left -= FORMAT_SECTORS)
	{
		once = (left > FORMAT_SECTORS) ? FORMAT_SECTORS : left;

		ret = read(s_devfd, s_fatbuf, once*SECTOR_SIZE);
		if (ret < 0)
		{
			perror("read");
			return S_ERROR;
		}

		for (i = 0; i < ret; i += fat_bits)
		{
			for (j = 0; j < fat_bits; j++)
			{
				if (s_fatbuf[i+j] != 0)
				{
					break;
				}
			}
			if (j == fat_bits)
			{
				free_clus++;
			}
		}
	}

	return free_clus;
}

/******************************************************************************
 * º¯ÊıÃû³Æ:	fat_get_free
 * º¯ÊıÃèÊö:	¼ÆËãfat¸ñÊ½µÄÊ£Óà¿Õ¼ä,KBµ¥Î».
 * Êä    Èë: 	ÎŞ
 * Êä    ³ö: 	ÎŞ
 * ·µ »Ø Öµ: 	¿ÕÓàÈİÁ¿KBµ¥Î»
 ******************************************************************************/
static int fat_get_free(void)
{
	msdos_boot_sector *pbs;
	int free_clus = 0;
	int free_size = 0;
	int sec_per_clu = 0;
	int fat_bits, fat_rsv_sec, sec_per_ftab;

	memset(s_fatbuf, 0, BUF_SIZE);

	if (read(s_devfd, s_fatbuf, BUF_SIZE) < 0)
	{
		perror("read");
		return S_ERROR;
	}

	pbs = (msdos_boot_sector *)s_fatbuf;
	fat_rsv_sec = pbs->reserved;
	sec_per_clu = pbs->cluster_size;

	if (pbs->fat_length > 0)	/* Èç¹ûÊÇfat16 ¸ñÊ½*/
	{
		fat_bits = 2;
		sec_per_ftab = pbs->fat_length;

		free_clus = cal_free_clus(fat_bits, fat_rsv_sec, sec_per_ftab);
	}
	else
	{
		fat_boot_fsinfo *pfinfo;

		pfinfo = (fat_boot_fsinfo *)&(s_fatbuf[SECTOR_SIZE]);
		if (pfinfo->free_clusters>= 0)/*Èç¹ûÄÜÈ¡³ö¿ÕÓà´Ø,ÔòÖ±½Ó¶Á³ö²»ÓÃ¼ÆËã*/
		{
			free_clus = pfinfo->free_clusters;
		}
		else
		{
			fat_bits = 4;
			sec_per_ftab = pbs->fat32.fat32_length;

			free_clus = cal_free_clus(fat_bits, fat_rsv_sec, sec_per_ftab);
		}
	}

	free_size = (free_clus * sec_per_clu)/2;
	return free_size;

}

/*
	è·å–sdå¡å‰©ä½™å®¹é‡
*/

int storage_get_free_space()
{
	SD_CARD_INFO_S *sd_card;


		sd_card = &s_sd_card;
		if (storage_sdcard_dev_init(sd_card->partition_name) < 0)
		{
			return 0;
		}

		sd_card->remain = fat_get_free();

		storage_sdcard_dev_release();

	return sd_card->remain;

}

static void fat_set_type(int total_sect)
{
	int tcap = total_sect / 2;
	int fat_bits;

	if (tcap < (2 * KILO * KILO))
	{
		s_format.fat32_type = 0;
		fat_bits = 2;

		if (tcap < (32 * KILO))
		{
			s_format.sec_per_clus = 1;
		}
		else if (tcap < (64 * KILO))
		{
			s_format.sec_per_clus = 2;
		}
		else if (tcap < (128 * KILO))
		{
			s_format.sec_per_clus = 4;
		}
		else if (tcap < (256 * KILO))
		{
			s_format.sec_per_clus = 8;
		}
		else if (tcap < (512 * KILO))
		{
			s_format.sec_per_clus = 16;
		}
		else if (tcap < (1024 * KILO))
		{
			s_format.sec_per_clus = 32;
		}
		else
		{
			s_format.sec_per_clus = 64;
		}
	}
	else
	{
		s_format.fat32_type = 1;
		fat_bits = 4;

		if (tcap < (8 * KILO  * KILO))
		{
			s_format.sec_per_clus = 8;
		}
		else if (tcap < (16 * KILO * KILO))
		{
			s_format.sec_per_clus = 16;
		}
		else if (tcap < (32 * KILO * KILO))
		{
			s_format.sec_per_clus = 32;
		}
		else
		{
			s_format.sec_per_clus = 64;
		}
	}

	 /* fab±íËùĞèÉÈÇøÊı=   (×ÜÉÈÇøÊı/Ã¿´ØÉÈÇøÊı)* ±íÊ¾Ã¿´ØËùĞè×Ö½ÚÊı/512 */
	s_format.sec_per_ftable = (total_sect * fat_bits) / (s_format.sec_per_clus * SECTOR_SIZE);
	if ((total_sect * fat_bits) % (s_format.sec_per_clus * SECTOR_SIZE) != 0)
	{
		s_format.sec_per_ftable++;
	}
}

/******************************************************************************
 * º¯ÊıÃû³Æ:	fatboot_format
 * º¯ÊıÃèÊö:	¸ñÊ½»¯Òıµ¼ÉÈÇø ,°´ÕÕÒıµ¼ÉÈÇøµÄ¶¨Òå½øĞĞ¸³Öµ
 * Êä    Èë: 	×ÜÉÈÇøÊı
 * Êä    ³ö:
 * ·µ »Ø Öµ: 	×´Ì¬
 ******************************************************************************/
static int fboot_format(int total_sect)
{
	msdos_boot_sector *pbs;
	Uint8_t	dummy_boot_jump[3] = {0xEB, 0x58, 0x90};/*fat32°×Æ¤ÊéÏÔÊ¾-- EB 58 90 */

	fat_set_type(total_sect);

	memset(s_fatbuf, 0, SECTOR_SIZE);
	pbs = (msdos_boot_sector *)s_fatbuf;

	memcpy(pbs->boot_jump, dummy_boot_jump, 3);
	strncpy(pbs->system_id, SYSTEM_ID, 8);

	pbs->sector_size[0] = (SECTOR_SIZE & 0xff);
	pbs->sector_size[1] = ((SECTOR_SIZE >> 8) & 0xff);

	pbs->cluster_size = s_format.sec_per_clus;
	if (s_format.fat32_type)
	{
		pbs->reserved = 32;
	}
	else
	{
		pbs->reserved = 1;
	}

	s_format.fat_reserved = pbs->reserved;

	pbs->fats = 2;

	if (s_format.fat32_type)
	{
		pbs->dir_entries[0] = 0;
		pbs->dir_entries[1] = 0;
		pbs->sectors[0] = 0;//xjw
		pbs->sectors[1] = 0;
	}
	else
	{
		pbs->dir_entries[0] = 0;	/* FAT12ºÍFAT16 Ä¬ÈÏ=512*/
		pbs->dir_entries[1] = 2;

		pbs->sectors[0] = (total_sect&0xFF);/* fat16 ×ÜÉÈÇøÊı*/
		pbs->sectors[1] =  ((total_sect >> 8)&0xFF);

	}

	pbs->media = 0xf8;/* Ã½ÌåÃèÊö·û,¶ÔÓÚ±¸·İ×ÜÊÇ¿ÉÒÆ¶¯µÄ,ËùÒÔ= 0xF0(µ«ÓĞĞ©µØ·½ËµÊÇ3.5´çÈíÅÌ), 0xF8ÊÇ±ê×¼Öµ,"¹Ì¶¨"½éÖÊ*/

	if (s_format.fat32_type)
	{
		pbs->fat_length = 0;
	}
	else
	{
		pbs->fat_length = (Uint16_t)s_format.sec_per_ftable;/* fat12/fat16 Ò»¸öfat±íÕ¼µÄÉÈÇøÊı-xjw */
	}

	/*
	×ÜÉÈÇøÊı= ÖùÃæÊı* ´ÅÍ·Êı* Ã¿´ÅµÀµÄÉÈÇøÊı
	struct hd_geometry
	{
		unsigned char heads; //´ÅÍ·Êı
		unsigned char sectors;//Ã¿´ÅµÀÉÈÇøÊı
		unsigned short cylinders;//ÖùÃæÊı
		unsigned long start;	//ÆğÊ¼Êı¾İÉÈÇø
	}
	HDIO_GETGEO »ñÈ¡¿éÉè±¸µÄÎïÀí²ÎÊı
	*/
	struct hd_geometry geometry;

	if (ioctl(s_devfd, HDIO_GETGEO, &geometry) < 0)
	{
		perror("HDIO_GETGEO");
		return S_ERROR;
	}

	pbs->secs_track = geometry.sectors;
	pbs->heads = geometry.heads;

	pbs->hidden = 0;/* Òş²Ø·ÖÇøÊı,Ã»ÓĞ·ÖÇøÊ±=0 */
	pbs->total_sect = total_sect;/*  ×ÜÉÈÇøÊı */

	msdos_volume_info *pvi;
	if (s_format.fat32_type)
	{
		pbs->fat32.fat32_length = s_format.sec_per_ftable;
		pbs->fat32.flags = 0;
		pbs->fat32.version[0] = 0;
		pbs->fat32.version[1] = 0;
		pbs->fat32.root_cluster = 2;/* ¸ùÄ¿Â¼µÚÒ»´ØµÄ´ØºÅ 2 */
		pbs->fat32.info_sector = 1;/* fs_info½á¹¹ËùÕ¼µÄÉÈÇøºÅ */
		pbs->fat32.backup_boot = 6;/* ±¸·İµÄÒıµ¼ÉÈÇøºÅ,Í¨³£Îª6 */

		pvi = (msdos_volume_info *)&(pbs->fat32.vi);
		memcpy(pvi->fs_type, MSDOS_FAT32_SIGN, 8);
	}
	else
	{
		//pvi = (struct msdos_volume_info *)&(pbs->oldfat.vi);---xjw
		//memcpy(pvi->fs_type, MSDOS_FAT16_SIGN, 8);
	}

	pvi->ext_boot_sign = MSDOS_EXT_SIGN;

	time_t create_time;
	long volume_id;

	time(&create_time);
	volume_id = (long)create_time;
	pvi->volume_id[0] = (volume_id & 0xff);/*Ëæ»úĞòÁĞºÅÓÃÊ±¼ä¸³Öµ */
	pvi->volume_id[1] = ((volume_id >>  8) & 0xff);
	pvi->volume_id[2] = ((volume_id >> 16) & 0xff);
	pvi->volume_id[3] = ((volume_id >> 24) & 0xff);

	memcpy(pvi->volume_label, VOLUME_NAME, 11);/* 11 ¸ö×Ö½ÚĞÅÏ¢ ±êÇ©*/
	pbs->boot_sign = BOOT_SIGN;


	lseek(s_devfd, 0, SEEK_SET);
	if (write(s_devfd, s_fatbuf, SECTOR_SIZE) < 0)
	{
		perror("write");
		return S_ERROR;
	}

	if (s_format.fat32_type)/*fat 32»¹±ØĞëÔÚ±¸·İÒıµ¼ÉÈÇøÔÙĞ´Ò»±é*/
	{
		lseek(s_devfd, (pbs->fat32.backup_boot) * SECTOR_SIZE, SEEK_SET);
		if (write(s_devfd, s_fatbuf, SECTOR_SIZE) < 0)
		{
			perror("write");
			return S_ERROR;
		}
	}

	return S_OK;
}

/******************************************************************************
 * º¯ÊıÃû³Æ:	fsinfo_format
 * º¯ÊıÃèÊö:	¸ñÊ½»¯ÎÄ¼şÏµÍ³ĞÅÏ¢
 * Êä    Èë: 	ÎŞ
 * Êä    ³ö: 	¸³Öµfat_boot_fsinfo
 * ·µ »Ø Öµ: 	×´Ì¬
 ******************************************************************************/
static int fsinfo_format(void)
{
	fat_boot_fsinfo *pfinfo;

	if (0 == s_format.fat32_type)
	{
		return S_OK;
	}

	memset(s_fatbuf, 0, BUF_SIZE);
	pfinfo = (fat_boot_fsinfo *)s_fatbuf;

	pfinfo->signature1 = FAT_FSINFO_SIG1;
	pfinfo->signature2 = FAT_FSINFO_SIG2;
	pfinfo->free_clusters = (s_format.sec_per_ftable * SECTOR_SIZE - 16) / 4; /* ×îĞÂÊ£Óà´ØÊıËã·¨=((fat±íÕ¼ÉÈÇøÊı*512)-4¸ö´ØËùÕ¼×Ö½ÚÊı)/(1¸ö´ØËùĞè×Ö½ÚÊı) */
	pfinfo->next_cluster = 2;/* ×îĞÂ·ÖÅäµÄ´ØĞòºÅ*/
	pfinfo->boot_sign = BOOT_SIGN;

	lseek(s_devfd, 1 * SECTOR_SIZE, SEEK_SET);
	if (write(s_devfd, s_fatbuf, SECTOR_SIZE) < 0)
	{
		perror("write");
		return S_ERROR;
	}

	return S_OK;
}

/******************************************************************************
 * º¯ÊıÃû³Æ:	ftable_format
 * º¯ÊıÃèÊö:	¸ñÊ½»¯fat±í.	s_format.sec_per_ftable ÊÇfat±íÕ¼µÄ×ÜÉÈÇøÊı
 					 FAT32ÖĞ×Ö½ÚµÄÅÅ²¼ÊÇ²ÉÓÃĞ¡¶ËÄ£Ê½µÄ
 * Êä    Èë: 	ÎŞ
 * Êä    ³ö: 	¸ñÊ½»¯ºóµÄfat±í
 * ·µ »Ø Öµ: 	×´Ì¬
 ******************************************************************************/
static int ftable_format(void)
{
	int once, left;
	int buf_flag, ret;
	int sect_off;

	memset(s_fatbuf, 0, BUF_SIZE);

	if (s_format.fat32_type)
	{
		s_fatbuf[0]	= 0xf8;
		s_fatbuf[1]	= 0xff;
		s_fatbuf[2]	= 0xff;
		s_fatbuf[3]	= 0x0f;/* 0 ºÅ±íÏî×ÜÊÇÕâ¸öÖµ,0x0FFFFFF8;FAT±íÆğÊ¼¹Ì¶¨±êÊ¶*/

		s_fatbuf[4]	= 0xff;
		s_fatbuf[5]	= 0xff;
		s_fatbuf[6]	= 0xff;
		s_fatbuf[7]	= 0x0f;/* 1 ºÅ±íÏîÒ²¹Ì¶¨,0xFFFFFFFF»ò0x0FFFFFFFÄ¬ÈÏÖµ*/

		s_fatbuf[8]	= 0xff;
		s_fatbuf[9]	= 0xff;
		s_fatbuf[10]	= 0xff;
		s_fatbuf[11]	= 0x0f;
	}
	else
	{
		s_fatbuf[0]	= 0xf8;
		s_fatbuf[1]	= 0xff;
		s_fatbuf[2]	= 0xff;
		s_fatbuf[3]	= 0xff;
	}

	fprintf(stderr, "Formating ...\n");
	buf_flag = 0;

	for (left = s_format.sec_per_ftable; left > 0; left -= FORMAT_SECTORS)
	{
		once = (left > FORMAT_SECTORS) ? FORMAT_SECTORS : left;
		sect_off = s_format.fat_reserved + s_format.sec_per_ftable - left;
		ret = lseek(s_devfd, sect_off*SECTOR_SIZE, SEEK_SET);
		if (ret < 0)
		{
			perror("lseek");
			return S_ERROR;
		}

		ret = write(s_devfd, s_fatbuf, once*SECTOR_SIZE);
		if (ret < 0)
		{
			perror("write");
			return S_ERROR;
		}

		/* ÓÃÓÚ±¸·İµÄfat±íÒ²½øĞĞ¸ñÊ½»¯ */
		ret = lseek(s_devfd, (sect_off+s_format.sec_per_ftable)*SECTOR_SIZE, SEEK_SET);
		if (ret < 0)
		{
			perror("lseek");
			return S_ERROR;
		}
		ret = write(s_devfd, s_fatbuf, once*SECTOR_SIZE);
		if (ret < 0)
		{
			perror("write");
			return S_ERROR;
		}

		if (0 == buf_flag)/* µÚÒ»´ÎĞ´Èëºó,ºóĞøfat±íµÄĞ´Èë¶¼ÊÇ0*/
		{
			memset(s_fatbuf, 0, BUF_SIZE);
			buf_flag = 1;
		}

		sect_off += once;
	}

	return S_OK;
}

/******************************************************************************
 * º¯ÊıÃû³Æ:	froot_format
 * º¯ÊıÃèÊö:	¸ùÄ¿Â¼ ÎÄ¼şÄ¿Â¼ÏîÇåÁã.Ö»¸ñÊ½»¯ÁË¾í±êĞÅÏ¢
 * Êä    Èë: 	ÎŞ
 * Êä    ³ö: 	¸ñÊ½»¯ºóµÄfat±í
 * ·µ »Ø Öµ: 	×´Ì¬
 ******************************************************************************/
static int froot_format(void)
{
	msdos_dir_entry *pde;
	int once, left;
	int root_sectors, root_off;
	int buf_flag, ret;

	if (s_format.fat32_type)
	{
		root_sectors = s_format.sec_per_clus;
	}
	else
	{
		root_sectors = 32;
	}

	memset(s_fatbuf, 0, BUF_SIZE);
	pde = (msdos_dir_entry *)s_fatbuf;


	memcpy(pde->name, VOLUME_NAME, 11);
	pde->attr = ATTR_VOLUME;

	/* ¸ùÄ¿Â¼ÆğÊ¼ÉÈÇø = ±£ÁôÉÈÇøÊı + Ò»¸öFATµÄÉÈÇøÊı ¡Á FAT±í¸öÊı + (ÆğÊ¼´ØºÅ-2) x Ã¿´ØµÄÉÈÇøÊı*/
	root_off = s_format.fat_reserved + s_format.sec_per_ftable * 2;
	ret = lseek(s_devfd, root_off * SECTOR_SIZE, SEEK_SET);
	if (ret < 0)
	{
		perror("lseek");
		return S_ERROR;
	}

	buf_flag = 0;

	for (left = root_sectors; left > 0; left -= FORMAT_SECTORS)
	{
		once = (left > FORMAT_SECTORS) ? FORMAT_SECTORS : left;

		ret = write(s_devfd, s_fatbuf, once*SECTOR_SIZE);
		if (ret < 0)
		{
			perror("write");
			return S_ERROR;
		}
		if (0 == buf_flag)
		{
			memset(s_fatbuf, 0, BUF_SIZE);
			buf_flag = 1;
		}
	}

	return S_OK;
}

int storage_format_sdcard(char *devname)
{
	int total_sect = 0;
	int ret = 0;
	int i = 0;
	SD_CARD_INFO_S *sd_card;

	sd_card = &s_sd_card;
	if (0 == strcmp(sd_card->device_name, devname))
	{
		if (storage_sdcard_dev_init(sd_card->partition_name) < 0)
		{
			return -1;
		}

		if (sd_card->capability > 0)
		{
			total_sect = sd_card->capability * 2;
		}
		else
		{
			total_sect = storage_fat_get_capacity();
			if (total_sect < 0)
			{
				goto out;
			}
		}

		printf("Disk capacity: sector%d * 512Bytes\n", total_sect);

		ret = fboot_format(total_sect);
		if (ret < 0)
		{
			goto out;
		}

		ret = fsinfo_format();
		if (ret < 0)
		{
			goto out;
		}

		ret = ftable_format();
		if (ret < 0)
		{
			goto out;
		}

		ret = froot_format();
		if (ret < 0)
		{
			goto out;
		}

		ret = total_sect / 2;

	out:
		storage_sdcard_dev_release();
	}

	return ret;


}
#endif

#define DIR_COUNT 5

static const char rec_dir[DIR_COUNT][8] = {AVS_VIDEO_DIR, CH0_VIDEO_DIR, CH1_VIDEO_DIR, CH2_VIDEO_DIR, CH3_VIDEO_DIR};


/*

å—è®¾å¤‡æ–‡ä»¶æ£€æµ‹ï¼š

*/

static S_Result storage_sdcard_dev_block_check(void)
{
	S_Result S_ret = S_ERROR;
	char line[64], tmp[16];
	char *blkname = NULL;
	FILE *fp;

	if (NULL == (fp = fopen("/proc/partitions", "r")))
	{
		fprintf(stderr, "open /proc/partitions fail\n");
		return S_ret;
	}

	strncpy(tmp, "mmcblk1p1", sizeof(tmp));
	while (fgets(line, sizeof(line), fp))
	{
		blkname = strstr(line, tmp);
		if (NULL != blkname)
		{
			S_ret = S_OK;
			break;
		}

	}

	fclose(fp);

	return S_ret;
}

/*

è®¾å¤‡æŒ‚è½½æ£€æµ‹

*/

static S_Result storage_sdcard_mount_check(void)
{
	S_Result S_ret = S_ERROR;
	FILE *fp;
	char line[128], tmp[16];
	char *blkname = NULL;

	if (NULL == (fp = popen("df -h", "r")))
	{
		perror("popen:");
		return S_ret;
	}

	strncpy(tmp, DEV_NAME, sizeof(tmp));

	while (fgets(line, 128, fp) != NULL)
	{
	    blkname = strstr(line, tmp);
		if (NULL != blkname)
		{
			S_ret = S_OK;
			break;
		}
	}

	pclose(fp);

	return S_ret;

}

/*

æ–‡ä»¶ç›®å½•æ£€æµ‹

*/

static S_Result storage_sdcard_dir_check(void)
{
	S_Result S_ret = S_ERROR;
	DIR *sdcard_dir = NULL;
	struct dirent *file;
	char recdir[24];

	int flag = 0;
	int i = 0;
	do
	{
		if ((sdcard_dir = opendir(MOUNT_DIR)) == NULL)
		{
			perror("opendir sdcard_dir");
			printf("get_video_filelist failed\n");
			break;
		}
		rewinddir(sdcard_dir);

		while ((file = readdir(sdcard_dir)) != NULL)
		{
			if ('.' != file->d_name[0]) //æ’é™¤'.','..'å’Œå…¶ä»–éšè—æ–‡ä»¶
			{
				for (i = 0; i < DIR_COUNT; i++)
				{
					if (!strcmp(rec_dir[i], file->d_name))
					{
						printf("find dir:%s\n", file->d_name);
						flag += 1 << i;
					}
				}
			}
		}

		for (i = 0; i < DIR_COUNT; i++)
		{
			if (!(flag & (1 << i)))
			{
				printf("make dir :%s", rec_dir[i]);
				snprintf(recdir, 24, "%s%s", MOUNT_DIR, rec_dir[i]);
				if(-1 == mkdir(recdir, 0755))
				{
					perror("mkdir:");
					printf("make dir :%s", recdir);
					break;
				}
			}
		}
		if (DIR_COUNT > i)
		{
			break;//å‡ºé”™é€€å‡º
		}

		if (closedir(sdcard_dir))
		{
			perror("closedir sdcard_dir");
			break;
		}
		S_ret = S_OK;
	}while (0);

	return S_ret;
}

/*

sdå¡æ£€æµ‹

*/
S_Result storage_sdcard_check(void)
{
	S_Result S_ret = S_ERROR;

	do
	{
		S_ret = storage_sdcard_dev_block_check();
		if (S_ERROR == S_ret)
		{
			printf("no sd_card!\n");
			break;
		}
		S_ret = storage_sdcard_mount_check();
		if (S_ERROR == S_ret)
		{
			printf("sd_card has not been mounted\n");
			S_ret = storage_sdcard_mount();
			if (S_ERROR == S_ret)
			{
				printf("sd_card need to be formated\n");
				break;
			}
		}
		S_ret = storage_sdcard_dir_check();
		if (S_ERROR == S_ret)
		{
			printf("sd_card mkdir error\n");
			break;
		}

		S_ret = S_OK;
	}while (0);

	return S_ret;


}

S_Result storage_sdcard_format(void)
{
	S_Result S_ret = S_ERROR;

	FILE *fp;

	char cmd[64]= {'\0'};

	snprintf(cmd, 64, "%s %s", "mkfs.vfat", DEV_NAME);

	if (NULL != (fp = popen(cmd, "r")))
	{
		S_ret = S_OK;
		pclose(fp);
	}
	else
	{
		perror("popen:");
	}

	return S_ret;
}

S_Result storage_sdcard_capacity_info(unsigned int *mbFreedisk, unsigned int *mbTotalsize, float *percent)
{
	S_Result S_ret = S_ERROR;
	unsigned long long freeDisk = 0;
	unsigned long long totalDisk = 0;
	struct statfs diskInfo;

	if (S_ERROR == storage_sdcard_mount_check())
	{
		*mbFreedisk = 0;
		*mbTotalsize = 0;
		*percent = 0;
	}
	else
	{
		statfs(MOUNT_DIR, &diskInfo);
		freeDisk = (unsigned long long)(diskInfo.f_bfree) * (unsigned long long)(diskInfo.f_bsize);
		*mbFreedisk = freeDisk >> 20;

		totalDisk = (unsigned long long)(diskInfo.f_blocks) * (unsigned long long)(diskInfo.f_bsize);
		*mbTotalsize = totalDisk >> 20;

		*percent = (float)(*mbFreedisk) * 100 / (*mbTotalsize);
	}

	printf ("sdcard: total=%dMB, free=%dMB, percent:%f\n", *mbTotalsize, *mbFreedisk, *percent);


	return S_ret;

}


S_Result storage_sdcard_mount(void)
{
	S_Result S_ret = S_ERROR;
	char *fstype =(char *)"vfat";
	unsigned long  mountflg = 0;
	mountflg = MS_MGC_VAL;

	do
	{
		umount(MOUNT_DIR);

		if (-1 == mount(DEV_NAME, MOUNT_DIR, fstype, mountflg, 0))
		{
			perror("mount");
			fprintf(stderr, "try once.\n");
			sleep(2);
			if (-1 == mount(DEV_NAME, MOUNT_DIR, fstype, mountflg, 0))
			{
				break;
			}
		}
		S_ret = S_OK;
	} while (0);

	return S_ret;
}


S_Result storage_sdcard_umount(void)
{
	S_Result S_ret = S_ERROR;

	do
	{
		sync();
		sleep(2);
		umount(MOUNT_DIR);
		if (-1 == umount(MOUNT_DIR))
		{
			perror("umount");
			fprintf(stderr, "try once.\n");
			sleep(2);
			if (-1 == umount(MOUNT_DIR))
			{
				break;
			}
		}
		S_ret = S_OK;
	}while (0);

	return S_ret;
}

S_Result storage_create_monitor_thread(void);
S_Result storage_destroy_monitor_thread(void);

S_Result storage_module_init(void)
{

	storage_create_monitor_thread();

	return S_OK;

}


S_Result storage_module_exit(void)
{

	storage_destroy_monitor_thread();

	return S_OK;
}

