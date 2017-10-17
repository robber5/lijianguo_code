#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>

#include "record_list.h"

#define VIDEO_DIR "/home/lijianguo/test"

typedef struct filelist_s
{
	char filename[32];
	LIST_HEAD_S list;

}FILELIST_S;

typedef struct fileinfo_s
{
	unsigned int file_num;
	FILELIST_S *listhead;
} FILEINFO_S;

int video_search_init();
void show_filelist();
int get_video_filelist();
void search_video_by_name(char *filename);
void delete_file_by_name(char **filename, int count);
int video_search_exit();

static DIR *g_video_dir;
static FILEINFO_S *g_fileinfo;


/*
	显示录像文件列表：
	1.时间顺序
	2.全部显示
	3.显示某天
*/
int get_video_filelist()
{
	struct dirent *file;
	FILELIST_S *filelist_node;
	if (NULL == g_video_dir)
	{
		if ((g_video_dir = opendir(VIDEO_DIR)) == NULL)
		{
			perror("opendir video_dir");
			printf("get_video_filelist failed\n");
			return -1;
		}
	}
	rewinddir(g_video_dir);
	int i = 0;
	while ((file = readdir(g_video_dir)) != NULL)
	{
		if ('.' != file->d_name[0]) //排除'.','..'和其他隐藏文件
		{
			//printf("file%d：%s",i, file->d_name);
			//i++;
			filelist_node = (FILELIST_S *)malloc(sizeof(FILELIST_S));
			strcpy(filelist_node->filename, file->d_name);
			list_add_tail(&filelist_node->list, &g_fileinfo->listhead->list);
			g_fileinfo->file_num++;
		}
	}

	if (closedir(g_video_dir))
	{
		perror("opendir video_dir");
	}
}

/*
	按照文件名查找录像文件

*/
void search_video_by_name(char *filename)
{
	LIST_HEAD_S *pos;
	FILELIST_S *filelist_node = NULL;

	list_for_each(pos, &g_fileinfo->listhead->list)   
	{   
	    filelist_node = list_entry(pos, FILELIST_S, list);
	    if (0 == strcmp(filelist_node->filename, filename))
	    {
			printf("found the file %s\n", filename);
			return;
	    }
	}
	printf("not found\n");
}

/*
	按照录像时间查找录像文件
*/
void search_video_by_time()
{

}

/* 文件删除 */
void delete_file_by_name(char **filename, int count)
{	
	FILELIST_S *filelist_node = NULL;
	LIST_HEAD_S *pos, *next;
	int i = 0, remain = count;
	char cmd[32] = {'\0'};
	list_for_each_safe(pos, next, &g_fileinfo->listhead->list)  
	{  
		filelist_node = list_entry(pos, FILELIST_S, list); //获取双链表结构体的地址
		for (i = 0; i < count; i++)
		{
			if (0 == strcmp(filelist_node->filename, filename[i]))
			{
				sprintf(cmd, "rm %s/%s", VIDEO_DIR, filename[i]);
				system(cmd);
				list_del_init(pos);
				g_fileinfo->file_num--;
				remain--;
				free(filelist_node);
			}
		}
		if (0 >= remain)
		{
			break;
		}
	}
}

void show_filelist()
{
	LIST_HEAD_S *pos;
	FILELIST_S *filelist_node = NULL;

	list_for_each(pos, &g_fileinfo->listhead->list)   
	{   
	    filelist_node = list_entry(pos, FILELIST_S, list);

		printf("file %s\n", filelist_node->filename);
	}
}

int video_search_init()
{
	g_fileinfo = (FILEINFO_S *)malloc(sizeof(FILEINFO_S));
	if (NULL == g_fileinfo)
	{
		perror("g_fileinfo malloc failed:");
		printf("filelist_init failed\n");
	}
	g_fileinfo->file_num = 0;
	g_fileinfo->listhead = (FILELIST_S *)malloc(sizeof(FILELIST_S));
	if (NULL == g_fileinfo->listhead)
	{
		perror("g_fileinfo->head malloc failed:");
		printf("filelist_init failed\n");
	}
	INIT_LIST_HEAD(&g_fileinfo->listhead->list);
}

int video_search_exit()
{
	FILELIST_S *filelist_node = NULL;
	LIST_HEAD_S *pos, *next;
	list_for_each_safe(pos, next, &g_fileinfo->listhead->list)  
	{  
		filelist_node = list_entry(pos, FILELIST_S, list); //获取双链表结构体的地址
		list_del_init(pos);
		g_fileinfo->file_num--;
		free(filelist_node);
	}
	return 0;

}


