#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>

#include "record_search.h"


static DIR *s_video_dir;
static FILEINFO_S *s_fileinfo;

int filelist_clear_up();

/*
	显示录像文件列表：
	1.时间顺序
	2.全部显示
	3.显示某天
*/

int refresh_video_filelist()
{
	struct dirent *file;

	filelist_clear_up();
	if (NULL == s_video_dir)
	{
		if ((s_video_dir = opendir(VIDEO_DIR)) == NULL)
		{
			perror("opendir video_dir");
			printf("get_video_filelist failed\n");
			return -1;
		}
	}
	rewinddir(s_video_dir);
	while ((file = readdir(s_video_dir)) != NULL)
	{
		if ('.' != file->d_name[0]) //排除'.','..'和其他隐藏文件
		{
			//printf("file%d：%s",i, file->d_name);
			//i++;
			inset_video_by_name(file->d_name);
			s_fileinfo->file_num++;
		}
	}

	if (closedir(s_video_dir))
	{
		perror("opendir video_dir");
		return -1;
	}
	return 0;
}

int get_filename_prefix_number(char *filename)
{
	char *p = NULL, *fileprename = NULL, filename_bk[FILE_NAME_LEN_MAX] = {'\0'};
	strncpy(filename_bk, filename, FILE_NAME_LEN_MAX);
	fileprename = strtok_r(filename_bk, ".", &p);
	return (atoi(fileprename));
}


/*
	通过文件名的方式添加录像文件到链表 ,按照日期从小到大排序

*/

int inset_video_by_name(char *filename)
{
	int ret = 0;
	LIST_HEAD_S *pos = NULL, *nextpos = NULL;
	FILELIST_S *filelist_node = NULL, *filelist_node_tmp = NULL, *filelist_node_next = NULL;
	int filename_num = 0, filename_num_tmp = 0, filename_num_next = 0;

	if (0 == s_fileinfo->file_num)
	{
		ret = 1;
		pos = &s_fileinfo->listhead->list;
	}
	else
	{
		filename_num = get_filename_prefix_number(filename);
		list_for_each_safe(pos, nextpos, &s_fileinfo->listhead->list)
		{
			filelist_node_tmp = list_entry(pos, FILELIST_S, list);
			filename_num_tmp = get_filename_prefix_number(filelist_node_tmp->filename);

			if (filename_num <= filename_num_tmp)
			{
				ret = 1;
				break;
			}
			else
			{
				if ((&s_fileinfo->listhead->list) != nextpos)
				{
					filelist_node_next = list_entry(nextpos, FILELIST_S, list);
					filename_num_next = get_filename_prefix_number(filelist_node_next->filename);
					if ( filename_num <= filename_num_next)
					{
						ret = 1;
						break;
					}
				}
				else
				{
					ret = 1;
					break;
				}
			}
		}
	}

	if (1 == ret)
	{
		filelist_node = (FILELIST_S *)malloc(sizeof(FILELIST_S));
		memset(filelist_node, 0, sizeof(FILELIST_S));
		strncpy(filelist_node->filename, filename, FILE_NAME_LEN_MAX);
		if (filename_num > filename_num_tmp)
		{
			list_add_forward(&filelist_node->list, pos);
		}
		else
		{
			list_add_tail(&filelist_node->list, pos);
		}

	}
	else
	{
		printf("illegal filename:%s!\n", filename);
	}

	return ret;

}

/*
	按照文件名查找录像文件

*/
void search_video_by_name(char *filename)
{
	LIST_HEAD_S *pos;
	FILELIST_S *filelist_node = NULL;

	list_for_each(pos, &s_fileinfo->listhead->list)
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
	char file[64] = {'\0'};
	list_for_each_safe(pos, next, &s_fileinfo->listhead->list)
	{
		filelist_node = list_entry(pos, FILELIST_S, list); //获取双链表结构体的地址
		for (i = 0; i < count; i++)
		{
			if (0 == strcmp(filelist_node->filename, filename[i]))
			{
				sprintf(file, "rm %s/%s", VIDEO_DIR, filename[i]);
				remove(file);
				//system(cmd);
				list_del_init(pos);
				s_fileinfo->file_num--;
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

	list_for_each(pos, &s_fileinfo->listhead->list)
	{
	    filelist_node = list_entry(pos, FILELIST_S, list);

		printf("file %s\n", filelist_node->filename);
	}

}

int video_search_init()
{
	s_fileinfo = (FILEINFO_S *)malloc(sizeof(FILEINFO_S));
	if (NULL == s_fileinfo)
	{
		perror("s_fileinfo malloc failed:");
		printf("filelist_init failed\n");
	}
	s_fileinfo->file_num = 0;
	s_fileinfo->listhead = (FILELIST_S *)malloc(sizeof(FILELIST_S));
	if (NULL == s_fileinfo->listhead)
	{
		perror("s_fileinfo->head malloc failed:");
		printf("filelist_init failed\n");
	}
	INIT_LIST_HEAD(&s_fileinfo->listhead->list);

	return 0;
}

int video_search_exit()
{
	filelist_clear_up();
	return 0;

}

int filelist_clear_up()
{
	FILELIST_S *filelist_node = NULL;
	LIST_HEAD_S *pos, *next;
	list_for_each_safe(pos, next, &s_fileinfo->listhead->list)
	{
		filelist_node = list_entry(pos, FILELIST_S, list); //获取双链表结构体的地址
		list_del_init(pos);
		s_fileinfo->file_num--;
		free(filelist_node);
	}
	return 0;
}


