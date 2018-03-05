#include "mp4_encoder.h"
#include <stdexcept>

namespace detu_record
{
void DebugOut(const char *file, int line, const char *format, ...)
{
	va_list ap;
	std::string str(file);
#ifdef _WIN32
	size_t pos = str.find_last_of("\\");
#else
	size_t pos = str.find_last_of("/");
#endif
	std::string sub_str = str.substr(pos + 1);
	printf("[%s][%d]", sub_str.c_str(), line);
	va_start(ap, format);
	vfprintf(stdout, format, ap);
	va_end(ap);
}

/*
H.264 in some container format (FLV, MP4, MKV etc.) need
"h264_mp4toannexb" bitstream filter (BSF)
*Add SPS,PPS in front of IDR frame
*Add start code ("0,0,0,1") in front of NALU
H.264 in some container (MPEG2TS) don't need this BSF.
*/
//'1': Use H.264 Bitstream Filter
#define USE_H264BSF 0
#if USE_H264BSF
AVBitStreamFilterContext *h264bsfc = NULL;
#endif

/*
AAC in some container format (FLV, MP4, MKV etc.) need
"aac_adtstoasc" bitstream filter (BSF)
*/
//'1': Use AAC Bitstream Filter
#define USE_AACBSF 0
#if USE_AACBSF
AVBitStreamFilterContext *aacbsfc = NULL;
#endif

Mp4Encoder::Mp4Encoder() : video_enable_(true), audio_enable_(true), video_out_stream_(nullptr),
						   audio_out_stream_(nullptr), ofmt_ctx_(nullptr), ofmt_(nullptr), state(0)
{
	memset(&video_param_, 0, sizeof(video_param_));
	memset(&audio_param_, 0, sizeof(audio_param_));
	av_register_all();
	av_log_set_level(AV_LOG_DEBUG);
}

Mp4Encoder::~Mp4Encoder()
{
	//Release();
}

bool Mp4Encoder::Init(std::string file_name, const VideoParamter &video_param,
					  const AudioParamter &audio_param)
{
	file_name_ = file_name;
	video_param_ = video_param;
	audio_param_ = audio_param;
	if (!InitEncoder())
	{
		state = 0;
		return false;
	}
	state = 1;
	return true;
}

void Mp4Encoder::Close()
{
	//some packet maybe not be written
	//...
	av_write_trailer(ofmt_ctx_);

#if USE_H264BSF
	av_bitstream_filter_close(h264bsfc);
#endif
#if USE_AACBSF
	av_bitstream_filter_close(aacbsfc);
#endif

	state = 0;
}

bool Mp4Encoder::InitEncoder()
{
	if (file_name_.empty() || !InitOutput())
	{
		DBG_INFO("Init output failed\n");
		return false;
	}

	if (video_param_.codec_id_ == AV_CODEC_ID_NONE || !InitVideo())
	{
		video_enable_ = false;
		DBG_INFO("Init video failed\n");
	}
	if (audio_param_.codec_id_ == AV_CODEC_ID_NONE || !InitAudio())
	{
		audio_enable_ = false;
		DBG_INFO("Init audio failed\n");
	}

	if (!video_enable_ && !audio_enable_)
	{
		DBG_INFO("Init av failed\n");
		return false;
	}

	if (!InitHeader())
	{
		DBG_INFO("Init header failed\n");
		return false;
	}

	av_dump_format(ofmt_ctx_, 0, file_name_.c_str(), 1);

#if USE_H264BSF
	h264bsfc = av_bitstream_filter_init("h264_mp4toannexb");
#endif
#if USE_AACBSF
	aacbsfc = av_bitstream_filter_init("aac_adtstoasc");
#endif

	return true;
}

bool Mp4Encoder::InitOutput()
{
	/* allocate the output media context */
	avformat_alloc_output_context2(&ofmt_ctx_, nullptr, nullptr, file_name_.c_str());
	if (!ofmt_ctx_)
	{
		DBG_INFO("Could not deduce output format from file extension: using mp4.\n");
		avformat_alloc_output_context2(&ofmt_ctx_, nullptr, "mp4", file_name_.c_str());
	}
	if (!ofmt_ctx_)
	{
		return false;
	}

	ofmt_ = ofmt_ctx_->oformat;

	/* print output stream information*/
	av_dump_format(ofmt_ctx_, 0, file_name_.c_str(), 1);

	if (!(ofmt_->flags & AVFMT_NOFILE))
	{
		int ret = avio_open(&ofmt_ctx_->pb, file_name_.c_str(), AVIO_FLAG_WRITE);
		if (ret < 0)
		{
			DBG_INFO("Could not open output file '%s'\n", file_name_.c_str());
			return false;
		}
		DBG_INFO("Open output file success!\n");
		return true;
	}
	return false;
}

bool Mp4Encoder::InitVideo()
{
	AVCodec *video_codec = avcodec_find_decoder(video_param_.codec_id_);
	video_out_stream_ = avformat_new_stream(ofmt_ctx_, video_codec);
	if (!video_out_stream_)
	{
		DBG_INFO("Failed to add video stream\n");
		return false;
	}
	video_out_stream_->id = 0;
	video_out_stream_->codec->codec_tag = 0;

	AVCodecContext *avctx = video_out_stream_->codec;
	avctx->codec_type = AVMEDIA_TYPE_VIDEO;
	/*此处,需指定编码后的H264/5数据的分辨率、帧率及码率*/
	avctx->codec_id = video_param_.codec_id_;
	//avctx->bit_rate = 5000000;
	avctx->width = video_param_.width_;
	avctx->height = video_param_.height_;
	avctx->time_base.num = 1;
	avctx->time_base.den = 15;
	if (ofmt_ctx_->oformat->flags & AVFMT_GLOBALHEADER)
		avctx->flags |= CODEC_FLAG_GLOBAL_HEADER;

#if 0
	if (avcodec_open2(avctx, video_codec, NULL) < 0)
	{
		DBG_INFO("avcodec_open2 error\n");
		return false;
	}
#endif
	return true;
}

bool Mp4Encoder::InitAudio()
{
	AVCodec *audio_codec = avcodec_find_decoder(audio_param_.codec_id_);
	audio_out_stream_ = avformat_new_stream(ofmt_ctx_, audio_codec);
	if (!audio_out_stream_)
	{
		DBG_INFO("Failed to add audio stream\n");
		return false;
	}

	audio_out_stream_->id = 1;
	audio_out_stream_->codec->codec_tag = 0;

	AVCodecContext *avctx = audio_out_stream_->codec;
	avctx->codec_type = AVMEDIA_TYPE_AUDIO;
	avctx->codec_id = audio_param_.codec_id_;
	//avctx->sample_fmt = AV_SAMPLE_FMT_S16;
	avctx->sample_rate = audio_param_.sample_rate_;
	avctx->bits_per_coded_sample = audio_param_.bit_width_;
	avctx->channels = audio_param_.channels_;
	avctx->bit_rate = avctx->sample_rate * avctx->channels * avctx->bits_per_coded_sample / 8;
	//avctx->frame_size = 300;
	avctx->channel_layout = AV_CH_LAYOUT_STEREO;
	if (ofmt_ctx_->oformat->flags & AVFMT_GLOBALHEADER)
		avctx->flags |= CODEC_FLAG_GLOBAL_HEADER;

#if 0
	if (avcodec_open2(avctx, audio_codec, NULL) < 0)
	{
		DBG_INFO("avcodec_open2 error\n");
		return false;
	}
#endif
	return true;
}

bool Mp4Encoder::InitHeader()
{
	//写文件头（Write file header）
	int ret = avformat_write_header(ofmt_ctx_, nullptr);
	if (ret < 0)
	{
		DBG_INFO("Write avi file header failed\n");
		return false;
	}
	return true;
}

int Mp4Encoder::WriteOneFrame(int media_type, char *frame, int length, int pts)
{
	if (nullptr == ofmt_ctx_)
	{
		printf("AVFormatContext is null\n");
		return -1;
	}
	if (frame == nullptr || length < 1 || pts < 0)
	{
		DBG_INFO("Invalid input paramter\n");
		return -1;
	}

	if (media_type == MEDIA_TYPE_AUDIO)
	{
		return WriteAudioFrame(frame, length, pts);
	}
	else if (media_type == MEDIA_TYPE_VIDEO)
	{
		return WriteVideoFrame(frame, length, pts);
	}
	else
	{
		DBG_INFO("Other media type\n");
		return -2;
	}
}
void Mp4Encoder::Release()
{
	if (video_enable_)
		avcodec_close(video_out_stream_->codec);
	if (audio_enable_)
		avcodec_close(audio_out_stream_->codec);

	if (ofmt_ctx_ && !(ofmt_ctx_->oformat->flags & AVFMT_NOFILE))
		avio_close(ofmt_ctx_->pb);

	avformat_free_context(ofmt_ctx_);
	ofmt_ctx_ = nullptr;
}

int Mp4Encoder::WriteVideoFrame(char *frame, int length, int pts)
{
	if (nullptr == ofmt_ctx_)
	{
		printf("AVFormatContext is null\n");
		return -1;
	}
	if (!video_enable_)
	{
		DBG_INFO("Record video disabled\n");
		return -1;
	}
	if (frame == nullptr || length < 1 || pts < 0)
	{
		DBG_INFO("Invalid input paramter\n");
		return -2;
	}

	AVPacket pkt = {0};
	av_init_packet(&pkt);

	int nalHeaderSize = -1;
	if (0x00 == frame[0] && 0x00 == frame[1])
	{
		if (0x00 == frame[2] && 0x01 == frame[3])
		{
			nalHeaderSize = 4;
		}
		else if (0x01 == frame[2])
		{
			nalHeaderSize = 3;
		}
	}

	if (video_param_.codec_id_ == AV_CODEC_ID_H265)
	{
		if (nalHeaderSize > 0 && (((frame[nalHeaderSize] & 0x7E) >> 1) == 0x20 || ((frame[nalHeaderSize] & 0x7E) >> 1) == 33 ||
								  ((frame[nalHeaderSize] & 0x7E) >> 1) == 34 || ((frame[nalHeaderSize] & 0x7E) >> 1) == 19 || ((frame[nalHeaderSize] & 0x7E) >> 1) == 20))
		{ // 判断该H265帧是否为I帧
			DBG_INFO("IIIIIIIIIIIIIIIIIIIIIIII\n");
			pkt.flags |= AV_PKT_FLAG_KEY;
		}
		else
		{ /* p frame*/
			pkt.flags = 0;
		}
	}
	else if (video_param_.codec_id_ == AV_CODEC_ID_H264)
	{
		if (nalHeaderSize > 0 && ((frame[nalHeaderSize] & 0x1F) == 5 ||
								  (frame[nalHeaderSize] & 0x1F) == 7 || (frame[nalHeaderSize] & 0x1F) == 8))
		{ // 判断该H264帧是否为I帧
			DBG_INFO("IIIIIIIIIIIIIIIIIIIIIIII\n");
			pkt.flags |= AV_PKT_FLAG_KEY;
		}
		else
		{ /* p frame*/
			pkt.flags = 0;
		}
	}
	else
	{
		av_packet_unref(&pkt);
		DBG_INFO("Other video codec id\n");
		return -3;
	}

#if 1
	AVRational bq = {1, 1000};
	AVRational cq = video_out_stream_->time_base;
	pkt.dts = pkt.pts = pts;
	pkt.pts = av_rescale_q(pkt.pts, bq, cq);
	pkt.dts = av_rescale_q(pkt.dts, bq, cq);
	//pkt.pts = av_rescale_q(pkt.pts, video_out_stream_->codec->time_base, video_out_stream_->time_base);
	//pkt.dts = av_rescale_q(pkt.dts, video_out_stream_->codec->time_base, video_out_stream_->time_base);
	//pkt.duration = av_rescale_q(pkt.duration, video_out_stream_->codec->time_base, video_out_stream_->time_base);

	//DBG_INFO("Video---out_stream_->codec->time_base: %d   %d\n", video_out_stream_->codec->time_base.den, video_out_stream_->codec->time_base.num);
	//DBG_INFO("Video---out_stream_->time_base: %d   %d\n", video_out_stream_->time_base.den, video_out_stream_->time_base.num);
	DBG_INFO("Video---pkt.pts: %d\n", pkt.pts);
	DBG_INFO("Video---pkt.dts: %d\n", pkt.dts);
	//DBG_INFO("Video---pkt.duration: %d\n", pkt.duration);
#endif

#if 0
		if (pkt.pts == AV_NOPTS_VALUE) {
			static int frame_index = 0;
			//Write PTS
			AVRational time_base1 = video_out_stream_->codec->time_base;
			//Duration between 2 frames (us)
			int64_t calc_duration = (double)AV_TIME_BASE / av_q2d(video_out_stream_->codec->time_base);
			//Parameters
			pkt.pts = (double)(frame_index*calc_duration) / (double)(av_q2d(time_base1)*AV_TIME_BASE);
			pkt.dts = pkt.pts;
			pkt.duration = (double)calc_duration / (double)(av_q2d(time_base1)*AV_TIME_BASE);
			frame_index++;
		}

		//Convert PTS/DTS
		pkt.pts = av_rescale_q_rnd(pkt.pts, video_out_stream_->codec->time_base, video_out_stream_->codec->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.dts = av_rescale_q_rnd(pkt.dts, video_out_stream_->codec->time_base, video_out_stream_->codec->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.duration = av_rescale_q(pkt.duration, video_out_stream_->codec->time_base, video_out_stream_->codec->time_base);
#endif
	pkt.pos = -1;
	pkt.stream_index = 0;

	pkt.size = length; /*帧大小*/

	pkt.data = (uint8_t *)frame; /*帧数据*/
	if (!pkt.data)
	{
		DBG_INFO("No data\n");
	}

		//Bitstream Filter
#if USE_H264BSF
	av_bitstream_filter_filter(h264bsfc, video_out_stream_->codec, NULL, &pkt.data, &pkt.size, pkt.data, pkt.size, 0);
#endif

	char buf[1024];
	//写入（Write）
	int ret = av_interleaved_write_frame(ofmt_ctx_, &pkt);
	if (ret < 0)
	{
		av_strerror(ret, buf, 1024);
	}
	avio_flush(ofmt_ctx_->pb);
	av_packet_unref(&pkt);

	return 0;
}

int Mp4Encoder::WriteAudioFrame(char *frame, int length, int pts)
{
	if (nullptr == ofmt_ctx_)
	{
		printf("AVFormatContext is null\n");
		return -1;
	}
	if (!audio_enable_)
	{
		DBG_INFO("Record audio disabled\n");
		return -1;
	}
	if (frame == nullptr || length < 1 || pts < 0)
	{
		DBG_INFO("Invalid input paramter\n");
		return -2;
	}

	AVPacket pkt = {0};
	av_init_packet(&pkt);
	pkt.flags |= AV_PKT_FLAG_KEY;

#if 1
	AVRational bq = {1, 1000};
	AVRational cq = audio_out_stream_->time_base;
	pkt.dts = pkt.pts = pts;
	pkt.pts = av_rescale_q(pkt.pts, bq, cq);
	pkt.dts = av_rescale_q(pkt.dts, bq, cq);
	//pkt.pts = av_rescale_q(pkt.pts, audio_out_stream_->codec->time_base, audio_out_stream_->time_base);
	//pkt.dts = av_rescale_q(pkt.dts, audio_out_stream_->codec->time_base, audio_out_stream_->time_base);
	//pkt.duration = av_rescale_q(pkt.duration, audio_out_stream_->codec->time_base, audio_out_stream_->time_base);

	//DBG_INFO("Audio---out_stream_->codec->time_base: %d   %d\n", audio_out_stream_->codec->time_base.den, audio_out_stream_->codec->time_base.num);
	//DBG_INFO("Audio---out_stream_->time_base: %d   %d\n", audio_out_stream_->time_base.den, audio_out_stream_->time_base.num);
	DBG_INFO("Audio---pkt.pts: %d\n", pkt.pts);
	DBG_INFO("Audio---pkt.dts: %d\n", pkt.dts);
	//DBG_INFO("Audio---pkt.duration: %d\n", pkt.duration);
#endif

#if 0
		if (pkt.pts == AV_NOPTS_VALUE) {
			static int frame_index = 0;
			//Write PTS
			AVRational time_base1 = audio_out_stream_->codec->time_base;
			//Duration between 2 frames (us)
			int64_t calc_duration = (double)AV_TIME_BASE / av_q2d(audio_out_stream_->codec->time_base);
			//Parameters
			pkt.pts = (double)(frame_index*calc_duration) / (double)(av_q2d(time_base1)*AV_TIME_BASE);
			pkt.dts = pkt.pts;
			pkt.duration = (double)calc_duration / (double)(av_q2d(time_base1)*AV_TIME_BASE);
			frame_index++;
		}

		//Convert PTS/DTS
		pkt.pts = av_rescale_q_rnd(pkt.pts, audio_out_stream_->codec->time_base, audio_out_stream_->codec->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.dts = av_rescale_q_rnd(pkt.dts, audio_out_stream_->codec->time_base, audio_out_stream_->codec->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		pkt.duration = av_rescale_q(pkt.duration, audio_out_stream_->codec->time_base, audio_out_stream_->codec->time_base);
#endif

	pkt.pos = -1;
	pkt.stream_index = 1;

	pkt.size = length; /*帧大小*/

	pkt.data = (uint8_t *)frame; /*帧数据*/
	if (!pkt.data)
	{
		DBG_INFO("No data\n");
	}

#if USE_AACBSF
	av_bitstream_filter_filter(aacbsfc, audio_out_stream_->codec, NULL, &pkt.data, &pkt.size, pkt.data, pkt.size, 0);
#endif

	char buf[1024];
	//写入（Write）
	int ret = av_interleaved_write_frame(ofmt_ctx_, &pkt);
	if (ret < 0)
	{
		av_strerror(ret, buf, 1024);
	}
	avio_flush(ofmt_ctx_->pb);

	av_packet_unref(&pkt);

	return 0;
}

void Mp4Encoder::GetFileState(int &state)
{
	state = this->state;
}
}