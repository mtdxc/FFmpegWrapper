#pragma once
#include <memory>
#include <functional>
#include "FFmpegVideoParam.h"
extern "C" {
#include "libavfilter/avfilter.h"
}

class FFmpegFrame {
public:
	using Ptr = std::shared_ptr<FFmpegFrame>;

	FFmpegFrame(std::shared_ptr<AVFrame> frame = nullptr);
	~FFmpegFrame();

	AVFrame *get() const;
	void fillPicture(AVPixelFormat target_format, int target_width, int target_height);

private:
	char *_data = nullptr;
	std::shared_ptr<AVFrame> _frame;
};

class FFMPEG_EXPORT FFmpegFilter
{
	std::shared_ptr<AVFilterGraph> _graph;
	std::shared_ptr<AVFilterContext> _src_ctx, _sink_ctx;
public:
	FFmpegFilter();
	~FFmpegFilter();
	/** 要使用subtitles和drawtext滤镜到ffmpeg中，则编译ffmpeg库时需要开启如下选项：
	*  1、字幕编解码器 --enable-encoder=ass --enable-decoder=ass --enable-encoder=srt --enable-decoder=srt --enable-encoder=webvtt --enable-decoder=webvtt；
	*  2、字幕解封装器 --enable-muxer=ass --enable-demuxer=ass --enable-muxer=srt --enable-demuxer=srt --enable-muxer=webvtt --enable-demuxer=webvtt
	*  3、滤镜选项  --enable-filter=drawtext --enable-libfreetype --enable-libass --enable-filter=subtitles
	*
	*  备注：以上字幕编解码器以及字幕解封装器可以只使用一个即可，代表只能使用一个字幕格式。具体参考编译脚本
	*/
	bool init(const char *desc, AVCodecContext* ctx);
	static void setupFontConfig(const char *cfg);
	void inputFrame(const FFmpegFrame::Ptr &frame);

	using cbOutput = std::function<void(const FFmpegFrame::Ptr &)>;
	void setOnOutput(cbOutput cb) { _cb = cb; }
	int sendCmd(const char *target, const char *cmd, const char *args);
	int queueCmd(double ts, const char *target, const char *cmd, const char *args);

private:
	bool init(const char* input, const char* desc);
	void onOutput(const FFmpegFrame::Ptr &frame) {
		if (_cb)
			_cb(frame);
	}
	cbOutput _cb;
};

