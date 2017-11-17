#include "mp4_encoder.h"
#include <stdexcept>

namespace  detu_record
{
	void DebugOut(const char* file, int line, const char *format, ...)
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

	Mp4Encoder::Mp4Encoder() : out_stream_(nullptr), ofmt_ctx_(nullptr) , ofmt_(nullptr), state(0)
	{}

	Mp4Encoder::~Mp4Encoder()
	{
		Release();
	}

	bool Mp4Encoder::Init(std::string file_name, const VideoParamter &video_param,
		const AudioParamter &audio_param)
	{
		file_name_ = file_name;
		video_param_ = video_param;
		audio_param_ = audio_param;
		state = 1;
		return InitEncoder();
	}

	void Mp4Encoder::Close()
	{
		//some packet maybe not be written
		//...
		av_write_trailer(ofmt_ctx_);
		state = 0;
	}

	bool Mp4Encoder::InitEncoder()
	{
		av_register_all();
		av_log_set_level(AV_LOG_DEBUG);

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

		out_stream_ = avformat_new_stream(ofmt_ctx_, nullptr);
		if (!out_stream_)
		{
			DBG_INFO("Failed allocating output stream\n");
			return false;
		}

		AVCodecContext *avctx = out_stream_->codec;
		avctx->codec_type = AVMEDIA_TYPE_VIDEO;
		/*此处,需指定编码后的H264/5数据的分辨率、帧率及码率*/
		avctx->codec_id = video_param_.codec_id_;
		avctx->bit_rate = 2000000;
		avctx->width = video_param_.width_;
		avctx->height = video_param_.height_;
		avctx->time_base.num = 1;
		avctx->time_base.den = 1000;
		avctx->flags |= CODEC_FLAG_GLOBAL_HEADER;

		/* print output stream information*/
		av_dump_format(ofmt_ctx_, 0, file_name_.c_str(), 1);

		int ret = -1;
		if (!(ofmt_->flags & AVFMT_NOFILE))
		{
			ret = avio_open(&ofmt_ctx_->pb, file_name_.c_str(), AVIO_FLAG_WRITE);
			if (ret < 0) {
				DBG_INFO("Could not open output file '%s'\n", file_name_.c_str());
				return false;
			}
			DBG_INFO("Open output file success!\n");
		}

		//写文件头（Write file header）  
		ret = avformat_write_header(ofmt_ctx_, nullptr);
		if (ret < 0) {
			DBG_INFO("write avi file header failed\n");
			return false;
		}

		return true;
	}

	int Mp4Encoder::WriteOneFrame(int media_type, char *frame, int length, int pts)
	{
		if (frame == nullptr || length < 1 || pts < 0)
		{
			DBG_INFO("invalid input paramter\n");
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
			DBG_INFO("other media type\n");
			return -2;
		}
	}

	void Mp4Encoder::GetFileState(int &state)
	{
		state = this->state;
	}
	void Mp4Encoder::Release()
	{
		//for (int i = 0; i < ofmt_ctx_->nb_streams; i++)
		//for (int i = 0; i < 1; i++)
		//{
			//avcodec_close(ofmt_ctx_->streams[i]->codec);
			//av_freep(&ofmt_ctx_->streams[i]->codec);
			//av_freep(&ofmt_ctx_->streams[i]);
		//}
		
		if (ofmt_ctx_ && !(ofmt_ctx_->oformat->flags & AVFMT_NOFILE))
			avio_close(ofmt_ctx_->pb);

		avformat_free_context(ofmt_ctx_);
	}

	int Mp4Encoder::WriteVideoFrame(char *frame, int length, int pts)
	{
		if (frame == nullptr || length < 1 || pts < 0)
		{
			DBG_INFO("invalid input paramter\n");
			return -1;
		}

		AVPacket pkt = { 0 };
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
			if (nalHeaderSize > 0 && (((frame[nalHeaderSize] & 0x7E) >> 1) == 0x20 || ((frame[nalHeaderSize] & 0x7E) >> 1) == 33 || \
				((frame[nalHeaderSize] & 0x7E) >> 1) == 34 || ((frame[nalHeaderSize] & 0x7E) >> 1) == 19 || ((frame[nalHeaderSize] & 0x7E) >> 1) == 20))
			{ // 判断该H265帧是否为I帧
				DBG_INFO("IIIIIIIIIIIIIIIIIIIIIIII\n");
				pkt.flags |= AV_PKT_FLAG_KEY;
			}
			else { /* p frame*/
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
			else { /* p frame*/
				pkt.flags = 0;
			}
		}
		else
		{
			DBG_INFO("other video codec id\n");
			return -2;
		}
		//pkt.dts = pkt.pts = AV_NOPTS_VALUE;
		pkt.dts = pkt.pts = pts;
		pkt.pts = av_rescale_q(pkt.pts, out_stream_->codec->time_base, out_stream_->time_base);
		pkt.dts = av_rescale_q(pkt.dts, out_stream_->codec->time_base, out_stream_->time_base);  
		pkt.duration = av_rescale_q(pkt.duration, out_stream_->codec->time_base, out_stream_->time_base);

		DBG_INFO(" out_stream_->codec->time_base: %d   %d\n", out_stream_->codec->time_base.den, out_stream_->codec->time_base.num);
		DBG_INFO(" out_stream_->time_base: %d   %d\n", out_stream_->time_base.den, out_stream_->time_base.num);
		DBG_INFO(" pkt.pts: %d\n", pkt.pts);
		DBG_INFO(" pkt.dts: %d\n", pkt.dts);
		DBG_INFO(" pkt.duration: %d\n", pkt.duration);
		
		pkt.size = length; /*帧大小*/

		pkt.data = (uint8_t *)frame; /*帧数据*/
		if (!pkt.data) {
			DBG_INFO("no data\n");
		}

		char buf[1024];
		//写入（Write）   
		int ret = av_interleaved_write_frame(ofmt_ctx_, &pkt);
		if (ret < 0) {
			av_strerror(ret, buf, 1024);
		}
		av_packet_unref(&pkt);

		return 0;
	}

	int Mp4Encoder::WriteAudioFrame(char *frame, int length, int pts)
	{
		//to be done
		return 0;
	}
}