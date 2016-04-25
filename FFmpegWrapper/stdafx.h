// stdafx.h : 标准系统包含文件的包含文件，
// 或是经常使用但不常更改的
// 特定于项目的包含文件
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             //  从 Windows 头文件中排除极少使用的信息
// Windows 头文件:
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
#define av_open_input_file(A1,A2,A3,A4,A5) avformat_open_input(A1,A2,A3,NULL)
#define av_find_stream_info(A1) avformat_find_stream_info(A1,NULL)
#define guess_format av_guess_format
#define av_write_header(ctx) avformat_write_header(ctx, NULL)
#define avcodec_open(ctx,codec) avcodec_open2(ctx,codec,NULL)
#define av_alloc_format_context avformat_alloc_context
#define dump_format av_dump_format
#define av_close_input_file(ctx) avformat_close_input(&ctx)
#define url_fclose(ctx) avio_close(*ctx)
#define url_fopen avio_open
#define URL_WRONLY AVIO_FLAG_WRITE
#define CODEC_TYPE_VIDEO AVMEDIA_TYPE_VIDEO
#define CODEC_TYPE_AUDIO AVMEDIA_TYPE_AUDIO
#define PKT_FLAG_KEY AV_PKT_FLAG_KEY
#define FF_I_TYPE AV_PICTURE_TYPE_I
#define SAMPLE_FMT_FLTP AV_SAMPLE_FMT_FLTP
/* in bytes */
#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000 // 1 second of 48khz 32bit audio
#endif
/// 对YUV帧进行上下和UV反转
void FlipYUV( AVFrame* pFrame, AVCodecContext* pContext, bool bFlipV, bool bFlipUV);
AVSampleFormat SelectBestFormat(AVCodec* audioCodec, AVSampleFormat dstFmt);
AVPixelFormat SelectBestFormat(AVCodec* videoCodec, AVPixelFormat dstFmt);
#include <stdexcept>
#include <string>
// TODO: 在此处引用程序需要的其他头文件
