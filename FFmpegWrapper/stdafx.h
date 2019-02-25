// stdafx.h : ��׼ϵͳ�����ļ��İ����ļ���
// ���Ǿ���ʹ�õ��������ĵ�
// �ض�����Ŀ�İ����ļ�
//

#pragma once

#define WIN32_LEAN_AND_MEAN             //  �� Windows ͷ�ļ����ų�����ʹ�õ���Ϣ
// Windows ͷ�ļ�:
#include <windows.h>


//#define FFMPEG51
#ifdef FFMPEG51
extern "C"
{
#include "avformat.h"
#include "avcodec.h"
#include "avutil.h"
#include "swscale.h"
}
#define av_dump_format dump_format
#define AVMEDIA_TYPE_VIDEO CODEC_TYPE_VIDEO
#define AVMEDIA_TYPE_AUDIO CODEC_TYPE_AUDIO 
#else
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libswscale/swscale.h"
#include "libavutil/samplefmt.h"
#include "libswresample/swresample.h"
#include "libavutil/opt.h"
}

#define avcodec_open(ctx,codec) avcodec_open2(ctx,codec,NULL)

/* in bytes */
#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio
#endif

void SetPacketKey(AVPacket* pack, bool key);
bool isPacketKey(AVPacket* pack);
/// ��YUV֡�������º�UV��ת
void FlipYUV(AVFrame* pFrame, AVCodecContext* pContext, bool bFlipV, bool bFlipUV);
AVSampleFormat SelectBestFormat(AVCodec* audioCodec, AVSampleFormat dstFmt);
AVPixelFormat SelectBestFormat(AVCodec* videoCodec, AVPixelFormat dstFmt);
#include <stdexcept>
#include <string>
// TODO: �ڴ˴����ó�����Ҫ������ͷ�ļ�
