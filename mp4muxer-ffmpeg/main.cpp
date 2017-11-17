#include <cstdio>
#include <cstring>
#include "mp4_encoder.h"

const int SIZE = 4000000;
char data[SIZE] = { 0 };

int main(int argc, char **argv)
{
	//3840x2160_10bit.h265
	//3840x2160_8bit.h264
	std::string infile = "3840x2160_10bit.h265";
	std::string filename = "hisi265_2.mp4";

	FILE *sizeFile = nullptr;
	FILE *videoFile = nullptr;

	detu_record::VideoParamter video_param = { AV_CODEC_ID_H265, 3840, 2160 };
	detu_record::AudioParamter audio_param = { AV_CODEC_ID_AAC };

	detu_record::Mp4Encoder mp4_encoder;
	if (!mp4_encoder.Init(filename, video_param, audio_param))
	{
		DBG_INFO("Mp4 init failed\n");
		return -1;
	}

	sizeFile = fopen("265size.txt", "r");
	if (sizeFile == nullptr)
	{
		DBG_INFO("Can not open 265size.txt\n");
		return -1;
	}

	videoFile = fopen(infile.c_str(), "rb");
	if (videoFile == nullptr)
	{
		DBG_INFO("Can not open %s\n", infile);
		return -1;
	}

	int size = 0;
	int pts = 0;
	while (1) 
	{

		// get a video frame（从编码器获取一帧编码后的H264数据）
		int res = fscanf(sizeFile, "%d\n", &size);
		//DBG_INFO("size = %d res = %d\n", size, res);
		if (res != 1)
			break;	
		size_t readBytes = fread(data, sizeof(char), size, videoFile);
		//DBG_INFO("readBytes = %d\n", readBytes);

		if (mp4_encoder.WriteOneFrame(detu_record::MEDIA_TYPE_VIDEO, data, readBytes, pts) != 0)
		{
			DBG_INFO("Write video frame failed\n");
		}

		pts += 40;
	}

	mp4_encoder.Close();

	fclose(sizeFile);
	fclose(videoFile);
	return 0;
}