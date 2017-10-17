#include <stdio.h>
#include "record_search.h"
#include <stdlib.h>

int main()
{
	char cmd[32] = {'\0'};
	int count ,i;
	printf("how many file do you want to create \n");
	scanf("%d", &count);
	for (i = 0; i < count; i++)
	{
		sprintf(cmd, "touch /home/lijianguo/test/%d.mp4", i);
		system(cmd);
	}
	char* filelist[3] = {(char *)"1.mp4", (char *)"3.mp4", (char *)"5.mp4"};
	video_search_init();
	get_video_filelist();
	show_filelist();
	search_video_by_name((char *)"3.mp4");
	delete_file_by_name(filelist, 3);
	show_filelist();
	video_search_exit();

	return 0;
}
