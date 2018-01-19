#ifndef _MP4_ENCODER_H_
#define _MP4_ENCODER_H_

#include <string>
#include <cstdarg>

#ifdef __cplusplus
extern "C" {
#endif
#include <libavformat/avformat.h>
#ifdef __cplusplus
};
#endif

#define DBG_ON 0

#if DBG_ON
#define DBG_INFO(format, ...) detu_record::DebugOut(__FILE__, __LINE__, format, ##__VA_ARGS__)
#else
#define DBG_INFO(format, ...)
#endif

namespace detu_record
{

void DebugOut(const char *file, int line, const char *format, ...);

struct VideoParamter
{
	enum AVCodecID codec_id_;
	int width_;
	int height_;
};

struct AudioParamter
{
	enum AVCodecID codec_id_;
	int sample_rate_;
	int bit_width_;
	int channels_;
};

enum MediaType
{
	MEDIA_TYPE_VIDEO,
	MEDIA_TYPE_AUDIO,
};

class Mp4Encoder
{
public:
	Mp4Encoder();
	virtual ~Mp4Encoder();

	bool Init(std::string file_name, const VideoParamter &video_param, const AudioParamter &audio_param);
	int WriteOneFrame(int media_type, char *frame, int length, int pts);
	void GetFileState(int &state);
	void Close();
	void Release();
	int WriteVideoFrame(char *frame, int length, int pts);
	int WriteAudioFrame(char *frame, int length, int pts);

private:
	bool InitEncoder();
	bool InitOutput();
	bool InitVideo();
	bool InitAudio();
	bool InitHeader();

private:
	std::string file_name_;
	VideoParamter video_param_;
	AudioParamter audio_param_;

	bool video_enable_;
	bool audio_enable_;

	AVStream *video_out_stream_;
	AVStream *audio_out_stream_;
	AVFormatContext *ofmt_ctx_;
	AVOutputFormat *ofmt_;
	int state;
	AVPacket pkt;
};
}



#endif