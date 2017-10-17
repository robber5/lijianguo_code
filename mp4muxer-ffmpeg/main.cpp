#include <cstdio>
#include <cstring>

#define __STDC_CONSTANT_MACROS

//#undef _WIN32

#ifdef _WIN32
extern "C"
{
	#include <libavcodec\avcodec.h>
	#include <libavformat\avformat.h>
	#include <libswscale\swscale.h>	
};
#else
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#ifdef __cplusplus
};
#endif
#endif

const int SIZE = 4000000;
uint8_t data[SIZE] = { 0 };

int main(int argc, char **argv)
{
	//3840x2160_10bit.h265
	//3840x2160_8bit.h264
	const char *infile = "h265_test.h265";
	const char *filename = "hisi265.mp4";
	int ret = -1;
	AVStream *out_stream = NULL;
	AVPacket pkt = { 0 };
	AVFormatContext *ofmt_ctx = NULL;
	AVOutputFormat *ofmt = NULL;

	FILE *sizeFile = NULL;
	FILE *videoFile = NULL;

	char buf[1024];
	int size = 0;

	AVCodecContext *avctx = NULL;

	//uint8_t *data = NULL;

	av_register_all();
	av_log_set_level(AV_LOG_DEBUG);

	/* allocate the output media context */
	avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, filename);
	if (!ofmt_ctx)
	{
		printf("Could not deduce output format from file extension: using mp4.\n");
		avformat_alloc_output_context2(&ofmt_ctx, NULL, "mp4", filename);
	}
	if (!ofmt_ctx)
	{
		goto exit;
	}

	ofmt = ofmt_ctx->oformat;

	out_stream = avformat_new_stream(ofmt_ctx, NULL);
	if (!out_stream)
	{
		printf("Failed allocating output stream\n");
		ret = AVERROR_UNKNOWN;
		goto exit;
	}

	avctx = out_stream->codec;
	avctx->codec_type = AVMEDIA_TYPE_VIDEO;
	/*�˴�,��ָ��������H264/5���ݵķֱ��ʡ�֡�ʼ�����*/
	avctx->codec_id = AV_CODEC_ID_H265;
	avctx->bit_rate = 2000000;
	avctx->width = 3840;
	avctx->height = 2160;
	avctx->time_base.num = 1;
	avctx->time_base.den = 30;

	/* print output stream information*/
	av_dump_format(ofmt_ctx, 0, filename, 1);

	if (!(ofmt->flags & AVFMT_NOFILE))
	{
		ret = avio_open(&ofmt_ctx->pb, filename, AVIO_FLAG_WRITE);
		if (ret < 0) {
			printf("Could not open output file '%s'\n", filename);
			goto exit;
		}
		printf("Open output file success!\n");
	}

	//д�ļ�ͷ��Write file header��  
	ret = avformat_write_header(ofmt_ctx, NULL);
	if (ret < 0) {
		printf("write avi file header failed\n");
		goto exit;
	}
	printf("Write avi header success!\n");

	sizeFile = fopen("265size.txt", "r");
	if (sizeFile == NULL)
	{
		printf("Can not open 265size.txt\n");
		goto exit;
	}

	videoFile = fopen(infile, "rb");
	if (videoFile == NULL)
	{
		printf("Can not open %s\n", infile);
		goto exit;
	}

	//data = new uint8_t[SIZE]{ 0 };

	//if (data == NULL)
	//{
	//	printf("new data failed...\n");
	//	goto exit;
	//}

	while (1) 
	{

		// get a video frame���ӱ�������ȡһ֡������H264���ݣ�
		//...
		int res = fscanf(sizeFile, "%d\n", &size);
		if (size < 50)
		{
			for(int i=0; i < 3; i++)
			{
					int tmp = 0;
					fscanf(sizeFile, "%d\n", &tmp);
					size +=tmp;
			}
		}
		fprintf(stdout, "size = %d res = %d\n", size, res);
		if (res != 1)
			break;	
	
		size_t readBytes = fread(data, sizeof(uint8_t), size, videoFile);
		fprintf(stdout, "readBytes = %d\n", readBytes);

		av_init_packet(&pkt);

		int nalHeaderSize = -1;
		if (0x00 == data[0] && 0x00 == data[1])
		{
			if (0x00 == data[2] && 0x01 == data[3])
			{
				nalHeaderSize = 4;
			}
			else if (0x01 == data[2])
			{
				nalHeaderSize = 3;
			}
		}

			// determine whether the I frame
		if (nalHeaderSize > 0 && (((data[nalHeaderSize] & 0x7E) >> 1) == 0x20 || ((data[nalHeaderSize] & 0x7E) >> 1) == 33 || \
			((data[nalHeaderSize] & 0x7E) >> 1) == 34 || ((data[nalHeaderSize] & 0x7E) >> 1) == 19 || ((data[nalHeaderSize] & 0x7E) >> 1) == 20)) 
		{ // �жϸ�H265֡�Ƿ�ΪI֡
			printf("IIIIIIIIIIIIIIIIIIIIIIII\n");
			pkt.flags |= AV_PKT_FLAG_KEY;
		}
		else { /* p frame*/
			pkt.flags = 0;
		}

		//if (nalHeaderSize > 0 && ((data[nalHeaderSize] & 0x1F) == 5 || 
		//	(data[nalHeaderSize] & 0x1F) == 7 || (data[nalHeaderSize] & 0x1F) == 8)) 
		//{ // �жϸ�H264֡�Ƿ�ΪI֡
		//	printf("IIIIIIIIIIIIIIIIIIIIIIII\n");
		//	pkt.flags |= AV_PKT_FLAG_KEY;
		//}
		//else { /* p frame*/
		//	pkt.flags = 0;
		//}

		pkt.dts = pkt.pts = AV_NOPTS_VALUE;
		pkt.size = size; /*֡��С*/

		pkt.data = data; /*֡����*/
		if (!pkt.data) {
			printf("no data\n");
		}

		//д�루Write��   
		ret = av_interleaved_write_frame(ofmt_ctx, &pkt);
		if (ret < 0) {
			av_strerror(ret, buf, 1024);
		}
		av_packet_unref(&pkt);
		memset(data, 0, SIZE);
	}

	av_write_trailer(ofmt_ctx);
exit:
	//if (data)
		//delete[] data;
	fclose(sizeFile);
	fclose(videoFile);
	/* close output */
	if (ofmt_ctx && !(ofmt_ctx->oformat->flags & AVFMT_NOFILE))
		avio_close(ofmt_ctx->pb);

	avformat_free_context(ofmt_ctx);

	return 0;
}