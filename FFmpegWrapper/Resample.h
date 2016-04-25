#ifndef FFMPEG_RESAMPLE
#define FFMPEG_RESAMPLE

struct SwrContext;
struct AVCodecContext;
struct AVFrame;

#include "FFmpegAudioParam.h"

/**
音频重采样
能改变通道数，采样率或是采样格式
*/
class FFMPEG_EXPORT CResample
{
public:
	CResample();
	virtual ~CResample();

	// 设置音频源参数
	bool SetSrcArgs(FFmpegAudioParam& param);
	bool SetSrcArgs(AVCodecContext* ctx);
	// 设置音频目的参数
	bool SetDstArgs(FFmpegAudioParam& param);
	bool SetDstArgs(AVCodecContext* ctx);

	// 初始化重采样器
	bool Init(int src_nb_samples);
	// 关闭重采样器
	bool Close();
	/* 重采样
	
	@param unsigned char * input 输入缓冲区
	@param int & len 缓冲区字节长度
	@return unsigned char* 输出缓冲区，视情况可能是内部缓冲，或原始缓冲区
	*/
	unsigned char* Process(unsigned char* input, int& len);
	/*
	重采样，并填充帧
	@param AVFrame * frame 填充的目的帧 
	@param unsigned char * input 输入缓冲区
	@param int len 缓冲区字节长度
	@return bool 成功则返回true
	*/
	bool FillFrame(AVFrame* frame, unsigned char* input, int len);
protected:
	FFmpegAudioParam srcParam;
	FFmpegAudioParam dstParam;

	// 音频重采样数据
	unsigned char **src_samples_data;
	int src_samples_linesize;
	// 目的缓冲区[由于消息滞留目的缓冲区在编码过程中可能变化]
	unsigned char **dst_samples_data;
	int dst_samples_linesize;
	int max_dst_nb_samples;
	// 转换句柄
	SwrContext *swr_ctx;
};
#endif
