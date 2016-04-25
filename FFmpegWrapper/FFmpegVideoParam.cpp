///
/// @file
///
/// @brief  Implementation file for the video parameters for encoder/decoder
///
/// @version    0.2.1
/// @date       2008/06/26
///
/// <b>History:</b>
/// - Version:  0.1.0
///   Author:   farthinker (farthinker@gmail.com)
///   Date:     2008/05/14
///   Changed:  Created
/// - Version:  0.2.0
///   Author:   farthinker (farthinker@gmail.com)
///   Date:     2008/06/06
///   Changed:  Bug fix
/// - Version:  0.2.1
///   Author:   John (john.zywu@gmail.com)
///   Date:     2008/06/26
///   Changed:  Changed some of the interfaces
///

#include "stdafx.h"
#include "FFmpegVideoParam.h"

FFmpegVideoParam::FFmpegVideoParam(int width, int height, int pixelFormat, int bitRate, int frameRate, int videoCodecId) :
width(width), height(height), pixelFormat(pixelFormat), bitRate(bitRate), frameRate(frameRate), codecId(videoCodecId)
{
	// valid the arguments
	if (width < 1 || height < 1 || pixelFormat == AV_PIX_FMT_NONE || bitRate < 1 || frameRate < 1)
	{
		throw std::invalid_argument("The arguments for the constructor of FFmpegVideoParam is invalid.");
	}
}

FFmpegVideoParam::FFmpegVideoParam() :
width(0), height(0), pixelFormat(AV_PIX_FMT_NONE), bitRate(0), frameRate(0), codecId(0)
{
}

FFmpegVideoParam::~FFmpegVideoParam()
{
}

bool FFmpegVideoParam::empty() const
{
	return width < 1 && height < 1 && pixelFormat == AV_PIX_FMT_NONE  &&
		bitRate < 1 && frameRate < 1 || codecId == 0;
}

bool FFmpegVideoParam::Read(AVCodecContext* ctx)
{
	width = ctx->width;
	height = ctx->height;
	pixelFormat = ctx->pix_fmt;
	bitRate = ctx->bit_rate;
	codecId = ctx->codec_id;
	return !empty();
}

bool FFmpegVideoParam::isDiff(AVCodecContext* ctx)
{
	return (height != ctx->height || width != ctx->width || pixelFormat != ctx->pix_fmt);
}
