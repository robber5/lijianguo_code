/**********************************
File name:		mp4_file.h
Version:		1.0
Description:	basic file operation
Author:			Shen Jiabin
Create Date:	2014-02-12

History:
-----------------------------------
01,12Feb14,Shen Jiabin create file.
-----------------------------------
**********************************/
#ifndef MP4_FILE_H /* MP4_FILE_H */
#define MP4_FILE_H /* MP4_FILE_H */

#include <wchar.h>

/*****************************************
Function:		mp4_fopen
Description:	fopen
Input:			filePath:file path;mode:open mode
Output:			none
Return:			FILE *
Others:			none
*****************************************/
FILE *mp4_fopen(const wchar_t *filePath, const wchar_t *mode);

/*****************************************
Function:		mp4_ftell
Description:	ftell
Input:			fp:file pointer
Output:			none
Return:			file offset
Others:			none
*****************************************/
long mp4_ftell(FILE *fp);

/*****************************************
Function:		mp4_fseek
Description:	fseek
Input:			fp:file pointer;pos:file offset;mode:seek mode
Output:			none
Return:			-1 when error;otherwise 0
Others:			none
*****************************************/
int mp4_fseek(FILE *fp,long pos,int mode);

/*****************************************
Function:		mp4_fclose
Description:	fclose
Input:			fp:file pointer
Output:			none
Return:			EOF when error;otherwise 0
Others:			none
*****************************************/
int mp4_fclose(FILE *fp);

/*****************************************
Function:		mp4_put_byte
Description:	put byte to file stream
Input:			fp:file pointer;val:value to write
Output:			none
Return:			-1 when error;otherwise 0
Others:			none
*****************************************/
int mp4_put_byte(FILE *fp, unsigned int val);

/*****************************************
Function:		mp4_put_be16
Description:	put 16bit(big endian) to file stream
Input:			fp:file pointer;val:value to write
Output:			none
Return:			-1 when error;otherwise 0
Others:			none
*****************************************/
int mp4_put_be16(FILE *fp, unsigned int val);

/*****************************************
Function:		mp4_put_be24
Description:	put 24bit(big endian) to file stream
Input:			fp:file pointer;val:value to write
Output:			none
Return:			-1 when error;otherwise 0
Others:			none
*****************************************/
int mp4_put_be24(FILE *fp, unsigned int val);

/*****************************************
Function:		mp4_put_be32
Description:	put 32bit(big endian) to file stream
Input:			fp:file pointer;val:value to write
Output:			none
Return:			-1 when error;otherwise 0
Others:			none
*****************************************/
int mp4_put_be32(FILE *fp,unsigned int val);

/*****************************************
Function:		mp4_put_tag
Description:	put tag to file stream
Input:			fp:file pointer;tag:tag to write
Output:			none
Return:			-1 when error;otherwise 0
Others:			none
*****************************************/
int mp4_put_tag(FILE *fp,const char *tag);

/*****************************************
Function:		mp4_put_buffer
Description:	put buffer to file stream
Input:			fp:file pointer;buf:buf to write;size:buf size
Output:			none
Return:			-1 when error;otherwise 0
Others:			none
*****************************************/
int mp4_put_buffer(FILE *fp, const unsigned char *buf, int size);

/*****************************************
Function:		mp4_put_h264_buffer
Description:	put h264 buffer to file stream
Input:			fp:file pointer;buf:buf to write;size:buf size
Output:			none
Return:			-1 when error;otherwise 0
Others:			none
*****************************************/
int mp4_put_h264_buffer(FILE *fp, const unsigned char *buf, int size);

#endif /* MP4_FILE_H */
