///
/// @file
///
/// @brief  Implementation file for the audio parameters for encoder/decoder
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
#include "FFmpegAudioParam.h"

FFmpegAudioParam::FFmpegAudioParam(int sampleRate, int channels, int bitRate, int audioCodecId, AVSampleFormat sft) : 
    sampleRate(sampleRate), channels(channels), bitRate(bitRate), codecId(audioCodecId), sampleFormat(sft)
{
    // valid the arguments
    if (bitRate < 1 || sampleRate < 1 || channels < 1)
    {
        throw std::invalid_argument("The arguments for the constructor of FFmpegAudioParam is invalid.");
    }
}

FFmpegAudioParam::FFmpegAudioParam() :
    sampleRate(0), channels(0), bitRate(0), codecId(0), sampleFormat(AV_SAMPLE_FMT_S16)
{
}

FFmpegAudioParam::~FFmpegAudioParam()
{
}

bool FFmpegAudioParam::empty() const
{
    return bitRate < 1 && sampleRate < 1 && channels < 1 || codecId == 0;
}

bool FFmpegAudioParam::Read( AVCodecContext* ctx )
{
	if(ctx)
	{
		channels = ctx->channels;
		sampleFormat = ctx->sample_fmt;
		sampleRate = ctx->sample_rate;
		bitRate = ctx->bit_rate;
		codecId    = ctx->codec_id;
		return !empty();
	}
	return false;
}

bool FFmpegAudioParam::isDiff( AVCodecContext* codec )
{
	return ( channels!=codec->channels
		|| sampleFormat != codec->sample_fmt 
		|| sampleRate != codec->sample_rate);
}

bool FFmpegAudioParam::isDiff( const FFmpegAudioParam& other )
{
	return ( channels!=other.channels || sampleFormat != other.sampleFormat 
		|| sampleRate != other.sampleRate);

}
