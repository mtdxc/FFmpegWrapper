#ifndef FFMPEG_RESAMPLE
#define FFMPEG_RESAMPLE

struct SwrContext;
struct AVCodecContext;
struct AVFrame;

#include "FFmpegAudioParam.h"

/**
��Ƶ�ز���
�ܸı�ͨ�����������ʻ��ǲ�����ʽ
*/
class FFMPEG_EXPORT CResample
{
public:
	CResample();
	virtual ~CResample();

	// ������ƵԴ����
	bool SetSrcArgs(FFmpegAudioParam& param);
	bool SetSrcArgs(AVCodecContext* ctx);
	// ������ƵĿ�Ĳ���
	bool SetDstArgs(FFmpegAudioParam& param);
	bool SetDstArgs(AVCodecContext* ctx);

	// ��ʼ���ز�����
	bool Init(int src_nb_samples);
	// �ر��ز�����
	bool Close();
	/* �ز���

	@param unsigned char * input ���뻺����
	@param int & len �������ֽڳ���
	@return unsigned char* �����������������������ڲ����壬��ԭʼ������
	*/
	unsigned char* Process(unsigned char* input, int& len);
	/*
	�ز����������֡
	@param AVFrame * frame ����Ŀ��֡
	@param unsigned char * input ���뻺����
	@param int len �������ֽڳ���
	@return bool �ɹ��򷵻�true
	*/
	bool FillFrame(AVFrame* frame, unsigned char* input, int len);
protected:
	FFmpegAudioParam srcParam;
	FFmpegAudioParam dstParam;

	// ��Ƶ�ز�������
	unsigned char **src_samples_data;
	int src_samples_linesize;
	// Ŀ�Ļ�����[������Ϣ����Ŀ�Ļ������ڱ�������п��ܱ仯]
	unsigned char **dst_samples_data;
	int dst_samples_linesize;
	int max_dst_nb_samples;
	// ת�����
	SwrContext *swr_ctx;
};
#endif
