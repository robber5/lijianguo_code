#include "Mp4Encoder.h"
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <wchar.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "./mp4lib/mp4_encoder.h"

#ifdef __cplusplus
}
#endif

Mp4Encoder::Mp4Encoder()
	: m_iAudioBufferSize(0)
	, m_iVStreamType(H264)
	, m_iAStreamType(AAC)
	, m_iHeight(2160)
	, m_iWidth(3840)
	, m_wChannels(0x01)
	, m_wBitPerSample(0x10)
	, m_dwSampleRate(0x00003E80)
	, m_iFirstVideoTimeStamp(-1)
	, m_llTotalSize(0)
{
	m_Handler = nullptr;
	m_pMP2DecHandler = nullptr;
	m_iCarryBitCount = 0;
	m_iLastTimeStemp = 0;
	m_iRcdTime = 0;
	m_iRcdBegainWrite = 0;
	m_iALastTimeStamp = -1;

	memset(m_AudioFrameBuffer, 0, sizeof(m_AudioFrameBuffer));
	memset(m_FileName, 0, sizeof(wchar_t) * MP4_FILE_NAME_PATH_LENGTH);
	memset(m_VideoFrameBuffer, 0, sizeof(m_VideoFrameBuffer));
}


Mp4Encoder::~Mp4Encoder()
{
	closeMp4Encoder();
}

int32_t Mp4Encoder::writeAudioFrame(int8_t *audioFrame, int32_t frameSize, int32_t timeStamp)
{
	if (audioFrame == nullptr || frameSize < 0 || m_Handler == nullptr || frameSize > MP2_AUDIO_TEMP_BUFFER_SIZE)
	{
		return -1;
	}

	if (m_iRcdBegainWrite == 0)
	{
		return -1;
	}

	int32_t ret = 0;
	uint32_t mDataLength = 0;
	int32_t iAudioTimeStamp = timeStamp;
	
	if (m_iAStreamType == MP2)
	{
		if (m_pMP2DecHandler == nullptr)
		{
			return 0;
		}
		/*decoder the MP2*/
		ret = MP2_decode_frame(m_pMP2DecHandler
			, m_AudioFrameBuffer
			, (unsigned int *)&mDataLength
			, (unsigned char *)audioFrame
			, frameSize);
		if (FALSE == ret)
		{
			//MP2_decode_close(m_pMP2DecHandler);
			m_pMP2DecHandler = nullptr;
			mp4_write_close(m_Handler);
			m_Handler = nullptr;
			return ret;
		}
		//MY_DEBUG_PRINT(TEXT("Audio Frame decode Success\n"));
	}
	else
	{
		memcpy(m_AudioFrameBuffer, audioFrame, frameSize);
		mDataLength = frameSize;
	}
	if (mDataLength <= 7)
	{
		return 0;
	}

	/*write the audio frame to the mp4 encoder*/
	ret = mp4_write_audio((unsigned char *)m_AudioFrameBuffer, mDataLength, iAudioTimeStamp, m_Handler);
	if (ret != 0)
	{
		//mp4_write_close(m_Handler);
		m_Handler = nullptr;
	}
	m_llTotalSize += mDataLength;

	return ret;
}
int32_t Mp4Encoder::writeVideoFrame(int8_t *videoFrame, int32_t frameSize, int32_t timeStamp)
{
	if (videoFrame == nullptr || frameSize < 0 || m_Handler == nullptr || frameSize > MP4_VIDEO_FRAME_SIZE)
	{
		return -1;
	}

	int32_t ret = 0;
	int32_t mMicroSeconds = 0;
	int32_t nalHeaderSize = 0;


	memset(m_VideoFrameBuffer, 0, sizeof(m_VideoFrameBuffer));
	memcpy(m_VideoFrameBuffer, videoFrame, frameSize);

	mMicroSeconds = timeStamp;

	if (m_iRcdBegainWrite == 0)
	{
		if (0x00 == m_VideoFrameBuffer[0] && 0x00 == m_VideoFrameBuffer[1])
		{
			if (0x00 == m_VideoFrameBuffer[2] && 0x01 == m_VideoFrameBuffer[3])
			{
				nalHeaderSize = 4;
			}
			else if (0x01 == m_VideoFrameBuffer[2])
			{
				nalHeaderSize = 3;
			}
		}

		//���ڼ�⿪ʼ¼��ĵ�һ֡�Ƿ���i֡���粻���򷵻أ��������ʼ¼��
		if (nalHeaderSize > 0 && m_iVStreamType == H264 &&
			((m_VideoFrameBuffer[nalHeaderSize] & 0x1F) == 5 || (m_VideoFrameBuffer[nalHeaderSize] & 0x1F) == 7 || (m_VideoFrameBuffer[nalHeaderSize] & 0x1F) == 8))
		{
			m_iRcdBegainWrite = 1;

		}
		else if (nalHeaderSize > 0 && m_iVStreamType == H265 && (((m_VideoFrameBuffer[nalHeaderSize] & 0x7E) >> 1) == 0x20 || ((m_VideoFrameBuffer[nalHeaderSize] & 0x7E) >> 1) == 33 || \
			((m_VideoFrameBuffer[nalHeaderSize] & 0x7E) >> 1) == 34 || ((m_VideoFrameBuffer[nalHeaderSize] & 0x7E) >> 1) == 19 || ((m_VideoFrameBuffer[nalHeaderSize] & 0x7E) >> 1) == 20))/*H265 ��I֡��������?*/
		{
			m_iRcdBegainWrite = 1;
		}
		else
		{
			return -1;
		}
	}

	if (m_iFirstVideoTimeStamp < 0)
	{
		/*write the first video frame*/
		m_iFirstVideoTimeStamp = mMicroSeconds;
		m_iLastTimeStemp = mMicroSeconds;
	}

	/*���PTS�Ƿ�������
	�����������1��PTS���䵽�ܴ� 2��PTS���䵽��С*/
	if ((mMicroSeconds - m_iLastTimeStemp) > 3000
		|| ((m_iLastTimeStemp > mMicroSeconds) && (mMicroSeconds + (0x200000000) / 90 - m_iLastTimeStemp)>3000))
	{
		m_iLastTimeStemp = mMicroSeconds;
		return -1;
	}

	/*��¼��λ��Ϣ��Ŀǰʱ�����ts����pts�л�ȡ����pts��33λ��չ��36λ*/
	if (m_iLastTimeStemp > mMicroSeconds)
	{
		m_iCarryBitCount++;
	}

	m_iLastTimeStemp = mMicroSeconds;

	switch (m_iCarryBitCount)
	{
	case 1:
		mMicroSeconds += (0x200000000) / 90;
		break;
	case 2:
		mMicroSeconds += (0x400000000) / 90;
		break;
	case 3:
		mMicroSeconds += (0x400000000 + 0x200000000) / 90;
		break;
	case 4:
		mMicroSeconds += (0x800000000) / 90;
		break;
	case 5:
		mMicroSeconds += (0x800000000 + 0x200000000) / 90;
		break;
	case 6:
		mMicroSeconds += (0x800000000 + 0x400000000) / 90;
		break;
	case 7:
		mMicroSeconds += (0x800000000 + 0x400000000 + 0x200000000) / 90;
		break;
	default:
		m_iCarryBitCount = 0;
		break;
	}

	m_iRcdTime = mMicroSeconds - m_iFirstVideoTimeStamp;

	if (m_iRcdTime < 0)
	{
		return -1;
	}



	if (m_iVStreamType == H264 || m_iVStreamType == H265)
	{
		ret = mp4_write_splite_sps_pps_i((unsigned char *)m_VideoFrameBuffer, frameSize, mMicroSeconds, m_Handler);
		if (ret != 0)
		{
			printf("write video failed...\n");
			//mp4_write_close(m_Handler);
			//m_Handler = NULL;
		}
		printf("write video success...\n");
		m_llTotalSize += frameSize;
	}
	else
	{
	}

	return ret;
}

int32_t Mp4Encoder::openMp4Encoder()
{
	if (m_FileName == nullptr)
	{
		return 0;
	}

	wchar_t TempPath[MP4_FILE_NAME_PATH_LENGTH];

	//GetTempPathW(MP4_FILE_NAME_PATH_LENGTH, TempPath);

	//GetTempFileNameW(TempPath, TEXT("TP_TMP"), 0, m_Mp2TmpFile);

	swprintf(m_Mp2TmpFile, sizeof(L"tmp.mp2"), L"%s", L"tmp.mp2");

	if (m_iVStreamType == H264 || m_iVStreamType == H265)
	{
		m_Handler = (int8_t *)mp4_write_open(m_FileName,
			m_Mp2TmpFile,
			m_iWidth,
			m_iHeight,
			(m_iVStreamType == H264 ? 2 : (m_iVStreamType == H265 ? 4 : 1)),
			(m_iAStreamType == MP2 ? 2 : 3));

		if (m_Handler == nullptr)
		{
			return 0;
		}
	}
	else
	{
	}

	if (m_iAStreamType == MP2)
	{
		/*init the mp2 decoder*/
		m_pMP2DecHandler = MP2_decode_init();
		if (m_pMP2DecHandler == nullptr)
		{
			return 0;
		}
	}
	else
	{
		/*do nothing */
	}

	return 1;
}
int32_t Mp4Encoder::closeMp4Encoder()
{
	int32_t ifH264Data = 1;

	if (m_iVStreamType == H264 || m_iVStreamType == H265)
	{
		/*close the file and clean the buffer*/
		if (m_Handler)
		{
			ifH264Data = mp4_check_data((void *)m_Handler);
			mp4_write_close((void *)m_Handler);
			m_Handler = nullptr;
		}
		if (m_pMP2DecHandler)
		{
			MP2_decode_close(m_pMP2DecHandler);
			m_pMP2DecHandler = nullptr;
		}

		memset(m_FileName, 0, sizeof(wchar_t) * MP4_FILE_NAME_PATH_LENGTH);
		m_iFirstVideoTimeStamp = -1;
		m_iCarryBitCount = 0;
		m_iLastTimeStemp = 0;
		m_iRcdBegainWrite = 0;
		m_llTotalSize = 0;
		m_iRcdTime = 0;
	}
	else
	{
	}
	return 0;
}
void Mp4Encoder::setFileName(wchar_t *fileName)
{
	if (NULL == fileName)
	{
		return;
	}
	wcsncpy(m_FileName, fileName, wcslen(fileName));

	return;
}
