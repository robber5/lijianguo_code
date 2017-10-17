/**********************************
File name:		mp4_input_aac.c
Version:		1.0
Description:	handle aac data
Author:			Shen Jiabin
Create Date:	2014-02-12

History:
-----------------------------------
01,12Feb14,Shen Jiabin create file.
-----------------------------------
**********************************/
#include <stdio.h>
#include <stdlib.h>
#include "mp4_input_aac.h"

/*****************************************
Function:		mp4_handle_aac_head
Description:	handle aac head data
Input:			fpAac:aac file pointer
Output:			none
Return:			-1 when error happens,otherwise 0
Others:			none
*****************************************/
int mp4_handle_aac_head(FILE *fpAac)
{
	int tmp = 0;
	unsigned char char1 = '\0';
	int flag = 0;
	int ok = 0;

	if(NULL == fpAac)
	{
		printf("mp4:input aac:fpAac is null\n");
		return -1;
	}
	else
	{
		while(!feof(fpAac))
		{
			tmp = fgetc(fpAac);
			if(EOF == tmp)
			{
				printf("mp4:input aac:handle aac header error(EOF or read error)\n");
				return -1;
			}
			else
			{
				char1 = (unsigned char)tmp;
				if(0xFF == char1)
				{
					flag++;
				}
				else if(0xF1 == char1)
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
Function:		mp4_read_aac_frame
Description:	read aac frame
Input:			fpAac:aac file pointer;buf:buffer;bufSize:size of buffer;dataSize:actual data size
Output:			none
Return:			-1 when error happens,otherwise 0
Others:			none
*****************************************/
int mp4_read_aac_frame(FILE *fpAac, unsigned char *buf, unsigned int bufSize, unsigned int *dataSize)
{
	// Begin by reading the 7-byte fixed_variable headers:
	unsigned char headers[7];
	int i;

	if (NULL == fpAac || NULL == buf || NULL == dataSize || bufSize < 2)
	{
		printf("mp4:aac input:pointer is null or bufSize < 2\n");
		return -1;
	}

	if (fread(headers, 1, sizeof(headers), fpAac) < sizeof(headers))
	{
		printf("Read aac header fail\n");
		return -1;
	}

	// check AAC header
	if (headers[0] != 0xFF || (headers[1] & 0xF0) != 0xF0)
	{
		printf("AAC header check fail\n");
		return -1;
	}

	*dataSize = ((headers[3] & 0x03) << 11) | (headers[4] << 3) | ((headers[5] & 0xE0) >> 5);

	// check data size
	if (*dataSize > bufSize)
	{
		printf("Buf size is too small\n");
		return -1;
	}

	// copy header to buf
	for (i = 0; i<7; i++)
	{
		buf[i] = headers[i];
	}
	if (fread(buf + 7, 1, (*dataSize - 7), fpAac) < (*dataSize - 7))
	{
		printf("Read AAC frame fail\n");
		return -1;
	}

	return 0;
}

