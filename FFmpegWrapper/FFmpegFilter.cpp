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
/** 要使用subtitles和drawtext滤镜到ffmpeg中，则编译ffmpeg库时需要开启如下选项：
*  1、字幕编解码器 --enable-encoder=ass --enable-decoder=ass --enable-encoder=srt --enable-decoder=srt --enable-encoder=webvtt --enable-decoder=webvtt；
*  2、字幕解封装器 --enable-muxer=ass --enable-demuxer=ass --enable-muxer=srt --enable-demuxer=srt --enable-muxer=webvtt --enable-demuxer=webvtt
*  3、滤镜选项  --enable-filter=drawtext --enable-libfreetype --enable-libass --enable-filter=subtitles
*
*  备注：以上字幕编解码器以及字幕解封装器可以只使用一个即可，代表只能使用一个字幕格式。具体参考编译脚本
*/
void FFmpegFilter::setupFontConfig(const char *cfg) {
	/** 遇到问题：当使用libass库来合成字幕时无法生成字幕
	*  分析原因：libass使用fontconfig库来匹配字体，而程序中没有指定字体匹配用的描述文件
	*  解决方案：设置FONTCONFIG_FILE的值
	*
	*  fontconfig工作原理：fontconfig通过环境变量FONTCONFIG_FILE来找到指定的fonts.conf文件(该文件的指定了字体文件(ttf,ttc等)的目录，以及字体fallback的规则)，最终选择指定的字体文件
	*  font fallback:如果某个字符在指定的字体库中不存在，那么就需要找到能够显示此字符的备用字体库，fontconfig就是专门做此事的。
	*
	*  备注：
	*  1、mac下 系统字体库的路径为：/System/Library/Fonts
	*  2、iOS下 系统字体库的路径为：ios系统字体不允许访问
	*  3、安卓下 系统字体库的路为：/system/fonts
	*  4、Ubuntu下 系统字体库的路径为：/usr/share/fonts
	*  不同系统支持的字体库可能不一样，由于fontconfig的字体fallback机制，如果不自定义自己的字体库，可能不同系统最终因为选择的字体库不一样导致合成字幕也不一样。
	*  所以解决办法就是统一用于各个平台的字体库，然后自定义fontconfig的字体库的搜索路径
	*/
	// 滤镜描述符
#ifdef WIN32
	_putenv_s("FONTCONFIG_FILE", cfg);
#else
	setenv("FONTCONFIG_FILE", cfg, 0);
#endif // WIN32
}

bool FFmpegFilter::init(const char *filter, AVCodecContext* ctx) {
	// 输入滤镜,设置大小格式和时间戳增量
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

	// 输出滤镜
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
		// 初始化滤镜
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
	//读写帧的时候调用
	int ret = av_buffersrc_add_frame_flags(_src_ctx.get(), frame->get(), AV_BUFFERSRC_FLAG_KEEP_REF);
	if (ret < 0) {
		InfoL << "av_buffersrc_add_frame_flags failed " << ffmpeg_err(ret);
		return;
	}

	while (true) {
		FFmpegFrame::Ptr dst = std::make_shared<FFmpegFrame>();
		ret = av_buffersink_get_frame(_sink_ctx.get(), dst->get());
		if (ret == AVERROR_EOF) {
			// 说明结束了
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