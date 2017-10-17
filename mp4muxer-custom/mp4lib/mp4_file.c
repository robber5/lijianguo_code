/**********************************
File name:		mp4_file.c
Version:		1.0
Description:	basic file operation
Author:			Shen Jiabin
Create Date:	2014-02-12

History:
-----------------------------------
01,12Feb14,Shen Jiabin create file.
-----------------------------------
**********************************/
#include <stdio.h>
#include "mp4_file.h"
#include <stdlib.h>

/*****************************************
Function:		mp4_fopen
Description:	fopen
Input:			filePath:file path;mode:open mode
Output:			none
Return:			FILE *
Others:			none
*****************************************/
FILE *mp4_fopen(const wchar_t *filePath, const wchar_t *mode)
{
	size_t len = wcslen(filePath) + 1;
	char *CStr1;
	CStr1 = (char*)malloc(len * sizeof(char));
	wcstombs(CStr1, filePath, len);

	len = wcslen(mode) + 1;
	char *CStr2;
	CStr2 = (char*)malloc(len * sizeof(char));
	wcstombs(CStr2, mode, len);

	return fopen(CStr1, CStr2);
	//return _wfopen(filePath, mode);
}

/*****************************************
Function:		mp4_ftell
Description:	ftell
Input:			fp:file pointer
Output:			none
Return:			file offset
Others:			none
*****************************************/
long mp4_ftell(FILE *fp)
{
	return ftell(fp);
}

/*****************************************
Function:		mp4_fseek
Description:	fseek
Input:			fp:file pointer;pos:file offset;mode:seek mode
Output:			none
Return:			-1 when error;otherwise 0
Others:			none
*****************************************/
int mp4_fseek(FILE *fp,long pos,int mode)
{
	return fseek(fp,pos,mode);
}

/*****************************************
Function:		mp4_fclose
Description:	fclose
Input:			fp:file pointer
Output:			none
Return:			EOF when error;otherwise 0
Others:			none
*****************************************/
int mp4_fclose(FILE *fp)
{
	return fclose(fp);
}

/*****************************************
Function:		mp4_put_byte
Description:	put byte to file stream
Input:			fp:file pointer;val:value to write
Output:			none
Return:			-1 when error;otherwise 0
Others:			none
*****************************************/
int mp4_put_byte(FILE *fp, unsigned int val)
{
	unsigned char char1 = '\0';

	if(NULL == fp)
	{
		return -1;
	}

	char1 = (unsigned char)val;
	if(EOF == (fputc(char1, fp)))
	{
		return -1;
	}
	else
	{
		return 0;
	}
}

/*****************************************
Function:		mp4_put_be16
Description:	put 16bit(big endian) to file stream
Input:			fp:file pointer;val:value to write
Output:			none
Return:			-1 when error;otherwise 0
Others:			none
*****************************************/
int mp4_put_be16(FILE *fp, unsigned int val)
{
	if(NULL == fp)
	{
		return -1;
	}

	if(-1 == (mp4_put_byte(fp, val >> 8)))
	{
		return -1;
	}
	else
	{
		if(-1 == (mp4_put_byte(fp, val)))
		{
			return -1;
		}
		else
		{
			return 0;
		}
	}
}

/*****************************************
Function:		mp4_put_be24
Description:	put 24bit(big endian) to file stream
Input:			fp:file pointer;val:value to write
Output:			none
Return:			-1 when error;otherwise 0
Others:			none
*****************************************/
int mp4_put_be24(FILE *fp, unsigned int val)
{
	if(NULL == fp)
	{
		return -1;
	}

	if(-1 == (mp4_put_be16(fp, val >> 8)))
	{
		return -1;
	}
	else
	{
		if(-1 == (mp4_put_byte(fp, val)))
		{
			return -1;
		}
		else
		{
			return 0;
		}
	}
}

/*****************************************
Function:		mp4_put_be32
Description:	put 32bit(big endian) to file stream
Input:			fp:file pointer;val:value to write
Output:			none
Return:			-1 when error;otherwise 0
Others:			none
*****************************************/
int mp4_put_be32(FILE *fp,unsigned int val)
{
	if(NULL == fp)
	{
		return -1;
	}

	if(-1 == (mp4_put_be16(fp, val >> 16)))
	{
		return -1;
	}
	else
	{
		if(-1 == (mp4_put_be16(fp, val)))
		{
			return -1;
		}
		else
		{
			return 0;
		}
	}
}

/*****************************************
Function:		mp4_put_tag
Description:	put tag to file stream
Input:			fp:file pointer;tag:tag to write
Output:			none
Return:			-1 when error;otherwise 0
Others:			none
*****************************************/
int mp4_put_tag(FILE *fp,const char *tag)
{
	if(NULL == tag || NULL == fp)
	{
		return -1;
	}

	while(*tag) 
	{
		if(-1 == (mp4_put_byte(fp, *(tag++))))
		{
			return -1;
		}
	}
	return 0;
}

/*****************************************
Function:		mp4_put_buffer
Description:	put buffer to file stream
Input:			fp:file pointer;buf:buf to write;size:buf size
Output:			none
Return:			-1 when error;otherwise 0
Others:			none
*****************************************/
int mp4_put_buffer(FILE *fp, const unsigned char *buf, int size)
{
	if(NULL == buf || NULL == fp)
	{
		return -1;
	}

	if(fwrite(buf,1,size,fp) != size)
	{
		return -1;
	}
	else
	{
		return 0;
	}
}

/*****************************************
Function:		mp4_put_h264_buffer
Description:	put h264 buffer to file stream
Input:			fp:file pointer;buf:buf to write;size:buf size
Output:			none
Return:			-1 when error;otherwise 0
Others:			none
*****************************************/
int mp4_put_h264_buffer(FILE *fp, const unsigned char *buf, int size)
{
	if(size <= 4 || NULL == buf || NULL == fp)
	{
		return -1;
	}

	if(-1 == mp4_put_be32(fp, size - 4))
	{
		return -1;
	}
	else
	{
		if(fwrite(buf + 4,1,size - 4,fp) != size - 4)
		{
			return -1;
		}
		else
		{
			return 0;
		}
	}
}

