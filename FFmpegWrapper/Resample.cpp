#include "stdafx.h"
#include "Resample.h"
using namespace std;

CResample::CResample()
{
	dst_samples_data = NULL;
	src_samples_data = NULL;
	swr_ctx = NULL;
	dst_samples_linesize = src_samples_linesize = 0;
	max_dst_nb_samples = 0;
}

CResample::~CResample()
{
	Close();
}

bool CResample::Close()
{
	if(dst_samples_data)
		av_freep(dst_samples_data);
	if(src_samples_data)
		av_freep(src_samples_data);
	if(swr_ctx)
	{
		swr_free(&swr_ctx);
		swr_ctx = NULL;
	}
	max_dst_nb_samples = 0;
	dst_samples_linesize = src_samples_linesize = 0;
	return true;
}

bool CResample::SetSrcArgs( FFmpegAudioParam& param )
{
	srcParam = param;
	return !srcParam.empty();
}

bool CResample::SetSrcArgs( AVCodecContext* ctx )
{
	return srcParam.Read(ctx);
}

bool CResample::SetDstArgs( FFmpegAudioParam& param )
{
	dstParam = param;
	return !dstParam.empty();

}

bool CResample::SetDstArgs( AVCodecContext* ctx )
{
	return dstParam.Read(ctx);
}

bool CResample::Init(int src_nb_samples)
{
	if(swr_ctx)
		return true;
	int ret ;
	swr_ctx = swr_alloc();
	if (!swr_ctx) {
		throw runtime_error("Could not allocate resampler context");
	}

	// set options 
	//av_opt_set_int(swr_ctx, "in_channel_layout",    src_ch_layout, 0);
	//av_opt_set_int(swr_ctx, "out_channel_layout",   dst_ch_layout, 0);
	av_opt_set_int       (swr_ctx, "in_channel_count",   srcParam.channels,		0);
	av_opt_set_int       (swr_ctx, "in_sample_rate",     srcParam.sampleRate,	0);
	av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt",      srcParam.sampleFormat,	0);
	av_opt_set_int       (swr_ctx, "out_channel_count",  dstParam.channels,       0);
	av_opt_set_int       (swr_ctx, "out_sample_rate",    dstParam.sampleRate,    0);
	av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt",     dstParam.sampleFormat,     0);

	// initialize the resampling context
	if (swr_init(swr_ctx) < 0) {
		throw runtime_error("Failed to initialize the resampling context");
	}
	// int src_nb_samples = c->codec->capabilities & CODEC_CAP_VARIABLE_FRAME_SIZE ? 10000 : c->frame_size;
	ret = av_samples_alloc_array_and_samples(&src_samples_data, &src_samples_linesize, 
		srcParam.channels, src_nb_samples, srcParam.sampleFormat, 0);
	if (ret < 0)
		throw runtime_error("Could not allocate source samples");

	/* compute the number of converted samples: buffering is avoided
		* ensuring that the output buffer will contain at least all the
		* converted input samples */
	max_dst_nb_samples = src_nb_samples;
	ret = av_samples_alloc_array_and_samples(&dst_samples_data, &dst_samples_linesize, 
		dstParam.channels, max_dst_nb_samples, dstParam.sampleFormat, 0);
	if (ret < 0) {
		throw runtime_error("Could not allocate destination samples");
	}
	/*dst_samples_size = av_samples_get_buffer_size(NULL, c->channels, max_dst_nb_samples, c->sample_fmt, 0);*/
	return true;
}

unsigned char* CResample::Process( unsigned char* input, int& len )
{
	int src_nb_samples = len/srcParam.channels/av_get_bytes_per_sample(srcParam.sampleFormat);
	if( !srcParam.isDiff(dstParam) )
	{
		return input;
		/*frame->nb_samples = src_nb_samples;
		avcodec_fill_audio_frame(frame, srcParam.channels, srcParam.sampleFormat, input, len, 0);
		return 1;*/
	}
	int ret;
	// compute destination number of samples
	int dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx, srcParam.sampleRate) + src_nb_samples,
		dstParam.sampleRate, srcParam.sampleRate, AV_ROUND_UP);
	if (dst_nb_samples > max_dst_nb_samples) {
		if(dst_samples_data)
			av_free(dst_samples_data[0]);
		ret = av_samples_alloc(dst_samples_data, &dst_samples_linesize, dstParam.channels,
			dst_nb_samples, dstParam.sampleFormat, 1);
		if (ret < 0)
			throw runtime_error("Could not realloc destination samples");
		max_dst_nb_samples = dst_nb_samples;
	}

	uint8_t* p = src_samples_data[0];
	src_samples_data[0] = input;
	// convert to destination format
	ret = swr_convert(swr_ctx,
		dst_samples_data, dst_nb_samples,
		(const uint8_t **)src_samples_data, src_nb_samples);
	src_samples_data[0] = p;

	len = av_samples_get_buffer_size(NULL, dstParam.channels, dst_nb_samples, dstParam.sampleFormat, 0);
	/*
	frame->nb_samples = dst_nb_samples;
	avcodec_fill_audio_frame(frame, dstParam.channels, dstParam.sampleFormat, dst_samples_data[0], dstSize, 0);
	*/
	return dst_samples_data[0];
}

bool CResample::FillFrame( AVFrame* frame, unsigned char* input, int len )
{
	int src_nb_samples = len/srcParam.channels/av_get_bytes_per_sample(srcParam.sampleFormat);
	if( !srcParam.isDiff(dstParam) )
	{
		frame->nb_samples = src_nb_samples;
		avcodec_fill_audio_frame(frame, srcParam.channels, srcParam.sampleFormat, input, len, 0);
		return true;
	}
	int ret;
	// compute destination number of samples
	int dst_nb_samples = av_rescale_rnd(swr_get_delay(swr_ctx, srcParam.sampleRate) + src_nb_samples,
		dstParam.sampleRate, srcParam.sampleRate, AV_ROUND_UP);
	if (dst_nb_samples > max_dst_nb_samples) {
		if(dst_samples_data)
			av_free(dst_samples_data[0]);
		ret = av_samples_alloc(dst_samples_data, &dst_samples_linesize, dstParam.channels,
			dst_nb_samples, dstParam.sampleFormat, 1);
		if (ret < 0)
			throw runtime_error("Could not realloc destination samples");
		max_dst_nb_samples = dst_nb_samples;
	}

	uint8_t* p = src_samples_data[0];
	src_samples_data[0] = input;
	// convert to destination format
	ret = swr_convert(swr_ctx,
		dst_samples_data, dst_nb_samples,
		(const uint8_t **)src_samples_data, src_nb_samples);
	src_samples_data[0] = p;

	len = av_samples_get_buffer_size(NULL, dstParam.channels, dst_nb_samples, dstParam.sampleFormat, 0);
	frame->nb_samples = dst_nb_samples;
	avcodec_fill_audio_frame(frame, dstParam.channels, dstParam.sampleFormat, dst_samples_data[0], len, 0);
	return true;
}