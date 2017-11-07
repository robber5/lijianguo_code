#pragma once
#include "./mp4lib/libMp2Dec.h"

#if defined( _WIN32 ) && !defined( __MINGW32__ )
typedef char      int8_t;
typedef short     int16_t;
typedef int       int32_t;
typedef long long int64_t;

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;
#else
#include <stdint.h>
#endif

enum VideoStreamType
{
	H264 = 0,
	H265
};

enum AudioStreamType
{
	MP2 = 0,
	AAC
};

#define G_MAX_FRAME_SIZE                4000000
#define MP2_AUDIO_FRAME_SIZE			2014
#define MP2_AUDIO_TEMP_BUFFER_SIZE		2800
#define MP4_FILE_NAME_PATH_LENGTH		256
#define MP4_VIDEO_FRAME_SIZE			(G_MAX_FRAME_SIZE + 1024)
#define ACC_BUFFER_SIZE                 4000

class Mp4Encoder
{
public:
	Mp4Encoder();
	virtual ~Mp4Encoder();

public:
	int32_t writeAudioFrame(uint8_t *audioFrame, int32_t frameSize, int32_t timeStamp);
	int32_t writeVideoFrame(uint8_t *videoFrame, int32_t frameSize, int32_t timeStamp);

	int32_t openMp4Encoder();
	int32_t closeMp4Encoder();
	void setFileName(wchar_t *fileName);

private:
	int8_t		m_AudioFrameBuffer[MP2_AUDIO_TEMP_BUFFER_SIZE];
	int32_t		m_iAudioBufferSize;
	wchar_t		m_FileName[MP4_FILE_NAME_PATH_LENGTH];
	wchar_t		m_Mp2TmpFile[MP4_FILE_NAME_PATH_LENGTH];
	int8_t		*m_Handler;
	HMP2DEC		m_pMP2DecHandler;
	int8_t      m_VideoFrameBuffer[MP4_VIDEO_FRAME_SIZE];

	/*video params*/
	int32_t		m_iWidth;
	int32_t		m_iHeight;
	int32_t		m_iVStreamType;
	int32_t		m_iFirstVideoTimeStamp;
	int32_t		m_iLastTimeStemp;
	int32_t		m_iCarryBitCount;
	int32_t		m_iRcdTime;
	int32_t     m_iRcdBegainWrite;
	int64_t     m_llTotalSize;

	/*audio params*/
	uint16_t    m_wChannels;
	uint32_t	m_dwSampleRate;
	uint16_t	m_wBitPerSample;
	int32_t		m_iAStreamType;
	int32_t     m_iALastTimeStamp;
};

