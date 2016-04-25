// stdafx.cpp : 只包括标准包含文件的源文件
// FFWrapper.pch 将作为预编译头
// stdafx.obj 将包含预编译类型信息

#include "stdafx.h"

// TODO: 在 STDAFX.H 中
// 引用任何所需的附加头文件，而不是在此文件中引用

void FlipYUV( AVFrame* pFrame, AVCodecContext * pContext, bool bFlipV, bool bFlipUV)
{
	if(bFlipV)
	{
		// 进行反转
		pFrame->data[0] += pFrame->linesize[0] * (pContext->height - 1);
		pFrame->linesize[0] *= -1;
		pFrame->data[1] += pFrame->linesize[1] * (pContext->height / 2 - 1);
		pFrame->linesize[1] *= -1;
		pFrame->data[2] += pFrame->linesize[2] * (pContext->height / 2 - 1);
		pFrame->linesize[2] *= -1;
	}
	if(bFlipUV){
		std::swap(pFrame->data[1], pFrame->data[2]);
	}
}

AVSampleFormat SelectBestFormat(AVCodec* audioCodec, AVSampleFormat dstFmt)
{
	if (audioCodec->sample_fmts)
	{
		// try to find the PixelFormat required by the input param,
		// use the default PixelFormat directly if required format not found
		const enum AVSampleFormat *p= audioCodec->sample_fmts;
		for ( ; *p != AV_SAMPLE_FMT_NONE; p ++)
		{
			if (*p == dstFmt)
				break;
		}
		if (*p == AV_SAMPLE_FMT_NONE)
			return audioCodec->sample_fmts[0];
		else
			return *p;
	}
	return AV_SAMPLE_FMT_NONE;
}

AVPixelFormat SelectBestFormat(AVCodec* videoCodec, AVPixelFormat dstFmt)
{
	// set the PixelFormat of the target encoded video
	if (videoCodec->pix_fmts)
	{
		// try to find the PixelFormat required by the input param,
		// use the default PixelFormat directly if required format not found
		const enum PixelFormat *p= videoCodec->pix_fmts;
		for ( ; *p != PIX_FMT_NONE; p ++)
		{
			if (*p == dstFmt)
				break;
		}
		if (*p == PIX_FMT_NONE)
			return videoCodec->pix_fmts[0];
		else
			return *p;
	}

}
