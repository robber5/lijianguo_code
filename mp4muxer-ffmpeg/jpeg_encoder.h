#ifndef JPEG_ENCODER_H
#define JPEG_ENCODER_H

#ifdef __cplusplus
extern "C"
{
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include "libavutil/imgutils.h"
#include "libswscale/swscale.h"


int yuv_encode_to_jpeg(char* out_file, uint8_t* picture_buf, int in_w,int in_h);

#ifdef __cplusplus
};
#endif


#endif


