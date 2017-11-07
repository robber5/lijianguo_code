#ifndef RECORD_SEARCH_MODULE_
#define RECORD_SEARCH_MODULE_

#include "record_list.h"

#define FILE_NAME_LEN_MAX 128

#define VIDEO_DIR "/home/lijianguo/test"

typedef struct filelist_s
{
	char filename[FILE_NAME_LEN_MAX];
	LIST_HEAD_S list;

}FILELIST_S;

typedef struct fileinfo_s
{
	unsigned int file_num;
	FILELIST_S *listhead;
} FILEINFO_S;

int video_search_init();
void show_filelist();
int refresh_video_filelist();
void search_video_by_name(char *filename);
void delete_file_by_name(char **filename, int count);
int video_search_exit();
int inset_video_by_name(char *filename);

#endif

