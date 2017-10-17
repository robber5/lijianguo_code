#include <iostream>
#include "Mp4Encoder.h"
#include <cstring>
#include <cstdlib>
#include <cstdio>

const int SIZE = 4000000;
uint8_t data[SIZE] = { 0 };

int main(int argc, char **argv)
{
	//const char *infile = "3840x2160_8bit.h264";
	const char *infile = "h265_test.h265";
	const char *filename = "hisi265.mp4";

	FILE *sizeFile = NULL;
	FILE *videoFile = NULL;

	sizeFile = fopen("265size.txt", "r");
	if (sizeFile == NULL)
	{
		printf("Can not open 265size.txt\n");
		return -1;
	}

	videoFile = fopen(infile, "rb");
	if (videoFile == NULL)
	{
		printf("Can not open %s\n", infile);
		return -1;
	}

	int size = 0;


	Mp4Encoder mp4Encoder;
	mp4Encoder.setFileName(L"bbbbb.mp4");
	mp4Encoder.openMp4Encoder();
	
	int pts = 0;
	
	while (1)
	{
		// get a video frame���ӱ�������ȡһ֡������H264���ݣ�
		//...
		int res = fscanf(sizeFile, "%d\n", &size);
		//fprintf(stdout, "size = %d res = %d\n", size, res);
		if (res != 1)
			break;

		size_t readBytes = fread(data, sizeof(uint8_t), size, videoFile);
		//fprintf(stdout, "readBytes = %d\n", readBytes);

		mp4Encoder.writeVideoFrame((int8_t *)data, size, pts);

		pts += 40;

		memset(data, 0, SIZE);
	}


	mp4Encoder.closeMp4Encoder();
	return 0;
}