/**********************************
File name:		mp4_input_mp2.c
Version:		1.0
Description:	handle mp2 data
Author:			Shen Jiabin
Create Date:	2014-02-12

History:
-----------------------------------
01,12Feb14,Shen Jiabin create file.
-----------------------------------
**********************************/
#include <stdio.h>
#include <stdlib.h>
#include "mp4_input_mp2.h"

/*****************************************
Function:		mp4_handle_mp2_head
Description:	handle mp2 head data
Input:			fpMp2:mp2 file pointer
Output:			none
Return:			-1 when error happens,otherwise 0
Others:			none
*****************************************/
int mp4_handle_mp2_head(FILE *fpMp2)
{
	int tmp = 0;
	unsigned char char1 = '\0';
	int flag = 0;
	int ok = 0;

	if(NULL == fpMp2)
	{
		printf("mp4:input mp2:fpMp2 is null\n");
		return -1;
	}
	else
	{
		while(!feof(fpMp2))
		{
			tmp = fgetc(fpMp2);
			if(EOF == tmp)
			{
				printf("mp4:input mp2:handle mp2 header error(EOF or read error)\n");
				return -1;
			}
			else
			{
				char1 = (unsigned char)tmp;
				if(0xFF == char1)
				{
					flag++;
				}
				else if(0xF5 == char1)
				{
					if(1 == flag)
					{
						ok = 1;
						break;
					}
					else
					{
						flag = 0;
					}
				}
				else
				{
					flag = 0;
				}
			}
		}

		if(1 == ok)
		{
			return 0;
		}
		else
		{
			return -1;
		}
    
	}
}


/*****************************************
Function:		mp4_read_mp2_frame
Description:	read mp2 frame
Input:			fpMp2:mp2 file pointer;buf:buffer;bufSize:size of buffer;dataSize:actual data size
Output:			none
Return:			-1 when error happens,otherwise 0
Others:			none
*****************************************/
int mp4_read_mp2_frame(FILE *fpMp2, unsigned char *buf, unsigned int bufSize, unsigned int *dataSize)
{
	int tmp = 0;
	unsigned int counter = 2;
	int flag = 0;
	unsigned char char1 = '\0';

	if(NULL == fpMp2 || NULL == buf || NULL == dataSize || bufSize < 2)
	{
		printf("mp4:mp2 input:pointer is null or bufSize < 2\n");
		return -1;
	}
  
	buf[0] = 0xFF;
	buf[1] = 0xF5;
  
	while(!feof(fpMp2))
	{
		tmp = fgetc(fpMp2);
		if(EOF == tmp)
		{
			if(feof(fpMp2))
			{
				break;
			}
			else
			{
				printf("mp4:mp2 input:read file error\n");
				return -1;
			}
		}
		else
		{
			char1 = (unsigned char)tmp;
			if(counter >= bufSize)
			{
				printf("mp4:mp2 input:buf size is too small\n");
				return -1;
			}
			buf[counter] = char1;
			counter++;
			
			if(0xFF == char1)
			{
				flag++;
			}
			else if(0xF5 == char1)
			{
				if(1 == flag)
				{
					counter = counter - 2;
					break;
				}
				else
				{
					flag = 0;
				}
			}
			else
			{
				flag = 0;
			}
		}
	}

	if(counter > 2)
	{
		*dataSize = counter;
		return 0;
	}
	else
	{
		return -1;
	}
}

