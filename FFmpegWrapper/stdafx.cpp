// stdafx.cpp : ֻ������׼�����ļ���Դ�ļ�
// FFWrapper.pch ����ΪԤ����ͷ
// stdafx.obj ������Ԥ����������Ϣ

#include "stdafx.h"

void SetPacketKey(AVPacket* pack, bool key)
{
	if (key)
		pack->flags |= AV_PKT_FLAG_KEY;
	else
		pack->flags &= ~AV_PKT_FLAG_KEY;
}

bool isPacketKey(AVPacket* pack)
{
	return pack->flags & AV_PKT_FLAG_KEY;
}

// TODO: �� STDAFX.H ��
// �����κ�����ĸ���ͷ�ļ����������ڴ��ļ�������

void FlipYUV(AVFrame* pFrame, AVCodecContext * pContext, bool bFlipV, bool bFlipUV)
{
	if (bFlipV)
	{
		// ���з�ת
		pFrame->data[0] += pFrame->linesize[0] * (pContext->height - 1);
		pFrame->linesize[0] *= -1;
		pFrame->data[1] += pFrame->linesize[1] * (pContext->height / 2 - 1);
		pFrame->linesize[1] *= -1;
		pFrame->data[2] += pFrame->linesize[2] * (pContext->height / 2 - 1);
		pFrame->linesize[2] *= -1;
	}
	if (bFlipUV){
		std::swap(pFrame->data[1], pFrame->data[2]);
	}
}

AVSampleFormat SelectBestFormat(AVCodec* audioCodec, AVSampleFormat dstFmt)
{
	if (audioCodec->sample_fmts)
	{
		// try to find the PixelFormat required by the input param,
		// use the default PixelFormat directly if required format not found
		const enum AVSampleFormat *p = audioCodec->sample_fmts;
		for (; *p != AV_SAMPLE_FMT_NONE; p++)
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
		const enum AVPixelFormat *p = videoCodec->pix_fmts;
		for (; *p != AV_PIX_FMT_NONE; p++)
		{
			if (*p == dstFmt)
				break;
		}
		if (*p == AV_PIX_FMT_NONE)
			return videoCodec->pix_fmts[0];
		else
			return *p;
	}

}
