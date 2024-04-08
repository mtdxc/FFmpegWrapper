#include "stdafx.h"
#include "FFmpegFilter.h"
extern "C"{
#include <libavutil\imgutils.h>
#include <libavfilter\buffersink.h>
#include <libavfilter\buffersrc.h>
}
#include <iostream>
#define WarnL std::cerr
#define InfoL std::cerr
static std::string ffmpeg_err(int errnum) {
	char errbuf[AV_ERROR_MAX_STRING_SIZE];
	av_strerror(errnum, errbuf, AV_ERROR_MAX_STRING_SIZE);
	return errbuf;
}

FFmpegFrame::FFmpegFrame(std::shared_ptr<AVFrame> frame) {
	if (frame) {
		_frame = std::move(frame);
	}
	else {
		_frame.reset(av_frame_alloc(), [](AVFrame *ptr) {
			av_frame_free(&ptr);
		});
	}
}

FFmpegFrame::~FFmpegFrame() {
	if (_data) {
		delete[] _data;
		_data = nullptr;
	}
}

AVFrame *FFmpegFrame::get() const {
	return _frame.get();
}

void FFmpegFrame::fillPicture(AVPixelFormat target_format, int target_width, int target_height) {
	//assert(_data == nullptr);
	_data = new char[av_image_get_buffer_size(target_format, target_width, target_height, 32)];
	av_image_fill_arrays(_frame->data, _frame->linesize, (uint8_t *)_data, target_format, target_width, target_height, 32);
}

///////////////////////////////////////////////////////////////////
/** Ҫʹ��subtitles��drawtext�˾���ffmpeg�У������ffmpeg��ʱ��Ҫ��������ѡ�
*  1����Ļ������� --enable-encoder=ass --enable-decoder=ass --enable-encoder=srt --enable-decoder=srt --enable-encoder=webvtt --enable-decoder=webvtt��
*  2����Ļ���װ�� --enable-muxer=ass --enable-demuxer=ass --enable-muxer=srt --enable-demuxer=srt --enable-muxer=webvtt --enable-demuxer=webvtt
*  3���˾�ѡ��  --enable-filter=drawtext --enable-libfreetype --enable-libass --enable-filter=subtitles
*
*  ��ע��������Ļ��������Լ���Ļ���װ������ֻʹ��һ�����ɣ�����ֻ��ʹ��һ����Ļ��ʽ������ο�����ű�
*/
void FFmpegFilter::setupFontConfig(const char *cfg) {
	/** �������⣺��ʹ��libass�����ϳ���Ļʱ�޷�������Ļ
	*  ����ԭ��libassʹ��fontconfig����ƥ�����壬��������û��ָ������ƥ���õ������ļ�
	*  �������������FONTCONFIG_FILE��ֵ
	*
	*  fontconfig����ԭ��fontconfigͨ����������FONTCONFIG_FILE���ҵ�ָ����fonts.conf�ļ�(���ļ���ָ���������ļ�(ttf,ttc��)��Ŀ¼���Լ�����fallback�Ĺ���)������ѡ��ָ���������ļ�
	*  font fallback:���ĳ���ַ���ָ����������в����ڣ���ô����Ҫ�ҵ��ܹ���ʾ���ַ��ı�������⣬fontconfig����ר�������µġ�
	*
	*  ��ע��
	*  1��mac�� ϵͳ������·��Ϊ��/System/Library/Fonts
	*  2��iOS�� ϵͳ������·��Ϊ��iosϵͳ���岻�������
	*  3����׿�� ϵͳ������·Ϊ��/system/fonts
	*  4��Ubuntu�� ϵͳ������·��Ϊ��/usr/share/fonts
	*  ��ͬϵͳ֧�ֵ��������ܲ�һ��������fontconfig������fallback���ƣ�������Զ����Լ�������⣬���ܲ�ͬϵͳ������Ϊѡ�������ⲻһ�����ºϳ���ĻҲ��һ����
	*  ���Խ���취����ͳһ���ڸ���ƽ̨������⣬Ȼ���Զ���fontconfig������������·��
	*/
	// �˾�������
#ifdef WIN32
	_putenv_s("FONTCONFIG_FILE", cfg);
#else
	setenv("FONTCONFIG_FILE", cfg, 0);
#endif // WIN32
}

bool FFmpegFilter::init(const char *filter, AVCodecContext* ctx) {
	// �����˾�,���ô�С��ʽ��ʱ�������
	char desc[400];
	auto time_base = ctx->time_base;
	if (!time_base.num || !time_base.den)
		time_base = { 1, 1000 };
	sprintf(desc, "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d",
		ctx->width, ctx->height, ctx->pix_fmt,
		time_base.num, time_base.den);
	return init(desc, filter);
	//av_buffersink_set_frame_size(dst_ctx, ctx->frame_size);
}

bool FFmpegFilter::init(const char *desc, const char* filter_des) {
	AVFilterGraph *graph = avfilter_graph_alloc();
	_graph.reset(graph, [](AVFilterGraph *p) { avfilter_graph_free(&p); });
	int ret = 0;
	AVFilterContext *src_ctx = nullptr;
	ret = avfilter_graph_create_filter(&src_ctx, avfilter_get_by_name("buffer"), "in", desc, NULL, _graph.get());
	if (ret < 0) {
		WarnL << "init srcfilter " << desc << " failed:" << ffmpeg_err(ret);
		return false;
	}
	_src_ctx.reset(src_ctx, [](AVFilterContext *c) { avfilter_free(c); });

	// ����˾�
	AVFilterContext *dst_ctx = nullptr;
	ret = avfilter_graph_create_filter(&dst_ctx, avfilter_get_by_name("buffersink"), "out", NULL, NULL, _graph.get());
	if (ret < 0) {
		WarnL << "init buffersink failed " << ffmpeg_err(ret);
		return false;
	}
	_sink_ctx.reset(dst_ctx, [](AVFilterContext *c) { avfilter_free(c); });

	AVFilterInOut* inputs = avfilter_inout_alloc();
	inputs->name = av_strdup("out");
	inputs->filter_ctx = dst_ctx;
	inputs->next = NULL;
	inputs->pad_idx = 0;

	AVFilterInOut* ouputs = avfilter_inout_alloc();
	ouputs->name = av_strdup("in");
	ouputs->filter_ctx = src_ctx;
	ouputs->next = NULL;
	ouputs->pad_idx = 0;

	ret = avfilter_graph_parse_ptr(graph, filter_des, &inputs, &ouputs, NULL);
	if (ret < 0) {
		InfoL << "avfilter_graph_parse_ptr " << filter_des << " failed " << ffmpeg_err(ret);
	}
	else {
		// ��ʼ���˾�
		ret = avfilter_graph_config(graph, NULL);
		if (ret < 0) {
			InfoL << "avfilter_graph_config failed " << ret;
		}
	}
	avfilter_inout_free(&inputs);
	avfilter_inout_free(&ouputs);
	return ret >= 0;
}

void FFmpegFilter::inputFrame(const FFmpegFrame::Ptr &frame) {
	//��д֡��ʱ�����
	int ret = av_buffersrc_add_frame_flags(_src_ctx.get(), frame->get(), AV_BUFFERSRC_FLAG_KEEP_REF);
	if (ret < 0) {
		InfoL << "av_buffersrc_add_frame_flags failed " << ffmpeg_err(ret);
		return;
	}

	while (true) {
		FFmpegFrame::Ptr dst = std::make_shared<FFmpegFrame>();
		ret = av_buffersink_get_frame(_sink_ctx.get(), dst->get());
		if (ret == AVERROR_EOF) {
			// ˵��������
			InfoL << "avfilter endeof";
			break;
		}
		else if (ret < 0) {
			if (ret != -11) {
				InfoL << "av_buffersink_get_frame failed " << ffmpeg_err(ret);
			}
			break;
		}
		onOutput(dst);
	}
}

int FFmpegFilter::sendCmd(const char *target, const char *cmd, const char *args) {
	int ret = -1;
	if (_graph)
		ret = avfilter_graph_send_command(_graph.get(), target, cmd, args, NULL, 0, 0);
	return ret;
}

int FFmpegFilter::queueCmd(double ts, const char *target, const char *cmd, const char *args) {
	int ret = -1;
	if (_graph)
		ret = avfilter_graph_queue_command(_graph.get(), target, cmd, args, 0, ts);
	return ret;
}