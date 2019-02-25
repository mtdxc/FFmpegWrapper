///
/// @file
///
/// @brief  Head file for decoder of FFmpeg
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
///   Changed:  Bug fix, change the video output format to AVPicture
/// - Version:  0.2.1
///   Author:   John (john.zywu@gmail.com)
///   Date:     2008/06/26
///   Changed:  Changed some of the interfaces
///

#include "stdafx.h"
#include "FFmpegDecoder.h"

FFmpegDecoder::FFmpegDecoder()
{
	// initialize the private fields
	inputContext = NULL;
	videoStream = NULL;
	audioStream = NULL;
	videoFrameBuffer = NULL;
	videoFrameSize = 0;
	videoBufferSize = 0;
	audioFrameBuffer = NULL;
	audioFrameSize = 0;
	audioBufferSize = 0;
	audioPacketData = NULL;
	audioPacketSize = 0;
	currentPacketPts = 0;
	currentPacketDts = 0;
	decodeVideo = false;
	decodeAudio = false;
	opened = false;
	currentPacket = (AVPacket*)av_mallocz(sizeof(AVPacket));
	audioCodecContext = NULL;
	videoCodecContext = NULL;
	// register all codecs and demux
	av_register_all();
}

FFmpegDecoder::~FFmpegDecoder()
{
	close();
	if (currentPacket)
	{
		av_packet_unref(currentPacket);
		av_freep(currentPacket);
		currentPacket = NULL;
	}
}

//////////////////////////////////////////////////////////////////////////
//
//  Public properties
//
//////////////////////////////////////////////////////////////////////////

const FFmpegVideoParam &FFmpegDecoder::getVideoParam() const
{
	return videoParam;
}

const FFmpegAudioParam &FFmpegDecoder::getAudioParam() const
{
	return audioParam;
}

const uint8_t *FFmpegDecoder::getVideoFrame() const
{
	return videoFrameBuffer;
}

int FFmpegDecoder::getVideoFrameSize() const
{
	return videoFrameSize;
}

const uint8_t *FFmpegDecoder::getAudioFrame() const
{
	return audioFrameBuffer;
}

int FFmpegDecoder::getAudioFrameSize() const
{
	return audioFrameSize;
}

const uint8_t * FFmpegDecoder::getRawFrame() const
{
	return currentPacket ? currentPacket->data : NULL;
}

int FFmpegDecoder::getRawFrameSize() const
{
	return currentPacket ? currentPacket->size : 0;
}

double FFmpegDecoder::getPresentTimeStamp() const
{
	return currentPacketPts;
}

double FFmpegDecoder::getDecodeTimeStamp() const
{
	return currentPacketDts;
}


//////////////////////////////////////////////////////////////////////////
//
//  Public Methods
//
//////////////////////////////////////////////////////////////////////////

void FFmpegDecoder::open(const char *fileName)
{
	if (opened)
	{
		throw runtime_error("The decoder is already opened. Call close before opening a new file for decoding.");
	}
	// open a media file as input.
	// The codecs are not opened. Only the file header (if present) is read
#ifdef FFMPEG51
	if (av_open_input_file(&inputContext, fileName, NULL, 0, NULL))
#else
	if (avformat_open_input(&inputContext, fileName, NULL, NULL))
#endif // FFMPEG51
	{
		throw runtime_error(string("Could not open the media file as input: ") + fileName);
	}

	// Read packets of a media file to get stream information.
#ifdef FFMPEG51
	if (av_find_stream_info(inputContext) < 0)
#else
	if (avformat_find_stream_info(inputContext, NULL) < 0)
#endif
	{
		throw runtime_error("Could not get stream information from the media file.");
	}


	// find the video/audio stream
	for (size_t i = 0; i < inputContext->nb_streams; i++)
	{
		// TODO
		// there might be several audio or video streams,
		// however, only one audio/video stream is used here
		if (!videoStream && inputContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			videoStream = inputContext->streams[i];
			decodeVideo = true;
			continue;
		}

		if (!audioStream && inputContext->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
		{
			audioStream = inputContext->streams[i];
			decodeAudio = true;
			continue;
		}
	}

	// video related initialization if necessary
	if (decodeVideo)
	{
		// initialize the video codec
		videoCodecContext = videoStream->codec;
		AVCodec *videoCodec = avcodec_find_decoder(videoCodecContext->codec_id);
		if (!videoCodec)
		{
			throw runtime_error("Video codec not found.");
		}

		// get the video parameters
		videoParam.Read(videoCodecContext);
		videoParam.frameRate = videoStream->r_frame_rate.num / videoStream->r_frame_rate.den;

		// open the video codec
		if (avcodec_open(videoCodecContext, videoCodec))
		{
			throw runtime_error("Could not open the video codec.");
		}
	}

	// audio related initialization if necessary
	if (decodeAudio)
	{
		// initialize the audio codec
		audioCodecContext = audioStream->codec;
		AVCodec *audioCodec = avcodec_find_decoder(audioCodecContext->codec_id);
		if (!audioCodec)
		{
			throw runtime_error("Audio codec not found.");
		}

		// get the audio parameters
		audioParam.Read(audioCodecContext);

		// open the audio codec
		if (avcodec_open(audioCodecContext, audioCodec))
		{
			throw runtime_error("Could not open the audio codec.");
		}

		// allocate output buffer
		audioBufferSize = AVCODEC_MAX_AUDIO_FRAME_SIZE;
		audioFrameBuffer = (uint8_t *)av_malloc(audioBufferSize);
		audioFrameSize = 0;

		audioPacketData = NULL;
		audioPacketSize = 0; // no data in the packet now, for initialization
	}

	opened = true;
}

bool FFmpegDecoder::open(const FFmpegVideoParam& vPar, const FFmpegAudioParam& aPar)
{
	videoParam = vPar;
	audioParam = aPar;
	if (vPar.codecId)
	{
		AVCodec* codec = avcodec_find_decoder((AVCodecID)vPar.codecId);
		if (!codec)
		{
			throw runtime_error("unable to find video codec");
		}

		videoCodecContext = avcodec_alloc_context3(codec);
		if (vPar.width) videoCodecContext->width = vPar.width;
		if (vPar.height) videoCodecContext->height = vPar.height;
		// 这应该是输出格式
		if (codec->pix_fmts)
			videoCodecContext->pix_fmt = SelectBestFormat(codec, (AVPixelFormat)vPar.pixelFormat);
		if (avcodec_open(videoCodecContext, codec) < 0){
			throw runtime_error("video codec open error");
		}
		decodeVideo = true;
	}
	if (aPar.codecId)
	{
		AVCodec* codec = avcodec_find_decoder((AVCodecID)aPar.codecId);
		if (!codec)
		{
			throw runtime_error("unable to find audio codec");
		}
		audioCodecContext = avcodec_alloc_context3(codec);
		if (aPar.sampleRate) audioCodecContext->sample_rate = aPar.sampleRate;
		if (aPar.channels) audioCodecContext->channels = aPar.channels;
		if (aPar.sampleFormat && codec->sample_fmts)
			audioCodecContext->sample_fmt = SelectBestFormat(codec, aPar.sampleFormat);
		if (avcodec_open(audioCodecContext, codec) < 0){
			throw runtime_error("audio codec open error");
		}
		decodeAudio = true;
		// allocate output buffer
		audioBufferSize = AVCODEC_MAX_AUDIO_FRAME_SIZE;
		audioFrameBuffer = (uint8_t *)av_malloc(audioBufferSize);
		audioFrameSize = 0;
	}
	opened = true;
	return opened;
}


void FFmpegDecoder::close()
{
	if (!opened)
	{
		return;
	}

	currentPacketPts = 0;
	currentPacketDts = 0;

	if (decodeVideo)
	{
		if (videoCodecContext){
			avcodec_close(videoCodecContext);
			// av_freep(videoCodecContext);
			videoCodecContext = NULL;
		}
		if (videoFrameBuffer)
			av_freep(&videoFrameBuffer);
		videoFrameSize = 0;
		videoBufferSize = 0;
	}
	if (decodeAudio)
	{
		if (audioCodecContext){
			avcodec_close(audioCodecContext);
			// av_freep(audioCodecContext);
			audioCodecContext = NULL;
		}
		av_freep(&audioFrameBuffer);
		audioFrameSize = 0;
		audioBufferSize = 0;
		audioPacketData = NULL;
		audioPacketSize = 0;
	}
	// close the input file
	if (inputContext){
#ifdef FFMPEG51
		av_close_input_file(inputContext);
#else
		avformat_close_input(&inputContext);
#endif
		inputContext = NULL;
	}

	audioStream = NULL;
	videoStream = NULL;

	av_init_packet(currentPacket);

	audioParam = FFmpegAudioParam();
	videoParam = FFmpegVideoParam();

	opened = false;
	decodeAudio = false;
	decodeVideo = false;
}

int FFmpegDecoder::decodeFrame()
{
	if (!opened)
	{
		throw runtime_error("Please call open() before decoding and reading frame.");
	}

	// one audio packet may contains several audio frame, so we need to
	// make sure all the audio frames in current packet were decoded.
	if (audioPacketSize <= 0)
	{
		// all data in the audio packet have been decoded, read new frame now
		if (readFrame() < 0)
		{
			return -1;
		}
	}

	if (videoStream && currentPacket->stream_index == videoStream->index)
	{
		// decode video frame
		decodeVideoFrame();
		return 0;
	}
	else if (audioStream && currentPacket->stream_index == audioStream->index)
	{
		// decode audio frame
		decodeAudioFrame();
		return 1;
	}
	else
	{
		// error
		return -1;
	}
}


//////////////////////////////////////////////////////////////////////////
//
//  Private Methods
//
//////////////////////////////////////////////////////////////////////////

int FFmpegDecoder::readFrame()
{
	av_init_packet(currentPacket);

	// read frame from input
	if (av_read_frame(inputContext, currentPacket) < 0)
	{
		return -1;
	}

	// For video, the returned packet contain exactly one frame.
	// For audio, it contains an integer number of frames if each frame has
	// a known fixed size (e.g. PCM or ADPCM) data.
	// It contains only one frame if the audio frames have a variable size (e.g. MPEG audio).

	if (audioStream && currentPacket->stream_index == audioStream->index)
	{
		// set the audio packet data to be decoded
		// for dealing with one packet contains several audio frames
		audioPacketData = currentPacket->data;
		audioPacketSize = currentPacket->size;
		currentPacketPts = (double)currentPacket->pts * audioStream->time_base.num / audioStream->time_base.den;
		currentPacketDts = (double)currentPacket->dts * audioStream->time_base.num / audioStream->time_base.den;
		return 1;
	}
	if (videoStream && currentPacket->stream_index == videoStream->index)
	{
		currentPacketPts = (double)currentPacket->pts * videoStream->time_base.num / videoStream->time_base.den;
		currentPacketDts = (double)currentPacket->dts * videoStream->time_base.num / videoStream->time_base.den;
		return 0;
	}

	return 0;
}

void FFmpegDecoder::decodeVideoFrame(bool bFreePkt)
{
	int decodedSize, gotPicture = 0;
	AVFrame videoFrame = { 0 };

	// set default value
	//avcodec_get_frame_defaults(&videoFrame);

	// decode the video frame
#ifdef FFMPEG51
	decodedSize = avcodec_decode_video(videoCodecContext, &videoFrame, &gotPicture,
		currentPacket->data, currentPacket->size);
#else
	decodedSize = avcodec_decode_video2(videoCodecContext, &videoFrame, &gotPicture,
		currentPacket);
#endif

	videoFrameSize = 0;
	if (gotPicture)
	{
		if (!videoFrameBuffer ||
			(videoParam.width != videoCodecContext->width || videoParam.height != videoCodecContext->height))
		{
			videoParam.width = videoCodecContext->width;
			videoParam.height = videoCodecContext->height;
			// allocate the video frame to be encoded
			// todo use (AVPixelFormat)videoFrame.format
			videoBufferSize = avpicture_get_size(videoCodecContext->pix_fmt, videoParam.width, videoParam.height);
			videoFrameSize = 0;
			videoFrameBuffer = (uint8_t *)av_malloc(videoBufferSize);
			if (!videoFrameBuffer)
			{
				throw runtime_error("Could not allocate video frame.");
			}
		}
		// read the data to the buffer
		avpicture_layout((AVPicture*)&videoFrame,
			(AVPixelFormat)videoParam.pixelFormat, videoParam.width, videoParam.height,
			videoFrameBuffer, videoBufferSize);
		videoFrameSize = videoBufferSize;
	}

	// video frame is decoded, free the packet now
	if (bFreePkt)
		av_free_packet(currentPacket);

	if (decodedSize < 0)
	{
		throw runtime_error("Fail to decode a video frame.");
	}
}

int FFmpegDecoder::decodeVideoFrame(uint8_t* data, int size)
{
	if (!videoCodecContext) return 0;
	currentPacket->data = data;
	currentPacket->size = size;
	decodeVideoFrame(false);
	return videoFrameSize;
}

int FFmpegDecoder::decodeAudioFrame(uint8_t* data, int size)
{
	if (!audioCodecContext) return 0;
	audioPacketData = data;
	audioPacketSize = size;
	decodeAudioFrame(false);
	return audioFrameSize;
}

void FFmpegDecoder::decodeAudioFrame(bool bFreePkt)
{
	int decodedSize, outputFrameSize = audioBufferSize;

	// decode one audio frame
#ifdef FFMPEG51
	decodedSize = avcodec_decode_audio2(audioCodecContext,
		(int16_t *)audioFrameBuffer, &outputFrameSize,
		audioPacketData, audioPacketSize);
#else
	AVPacket pack = {0};
	pack.data = audioPacketData;
	pack.size = audioPacketSize;
	/*
	decodedSize = avcodec_decode_audio3(audioCodecContext, (int16_t *)audioFrameBuffer, &outputFrameSize, &pack);
	*/
	AVFrame frame = {0};
	int got_frame = 0;
	decodedSize = avcodec_decode_audio4(audioCodecContext, &frame, &got_frame, &pack);
	if (got_frame){
		outputFrameSize = av_samples_get_buffer_size(NULL, frame.channels, frame.nb_samples, (AVSampleFormat)frame.format, 0);
		// outputFrameSize = frame.nb_samples * frame.channels * sizeof(short);
		memcpy(audioFrameBuffer, frame.data[0], outputFrameSize);
	}
#endif

	audioFrameSize = outputFrameSize;
	if (audioPacketSize - decodedSize <= 0)
	{
		// all the audio frames in the packet have been decoded
		// free the packet now
		if (bFreePkt)
			av_free_packet(currentPacket);
	}

	if (decodedSize< 0)
	{
		throw runtime_error("Fail to decode a audio frame.");
	}
	else
	{
		audioPacketData += decodedSize;
		audioPacketSize -= decodedSize;
	}
}

AVPacket* FFmpegDecoder::getCurPacket()
{
	return currentPacket;
}


int FFmpegDecoder::seek(double sec) {
	return av_seek_frame(inputContext, -1, sec * AV_TIME_BASE, AVSEEK_FLAG_ANY);
}

#include "FFmpegEncoder.h"
bool FFmpegDecoder::split(const char * inFile, const char * outFile, double start, double end)
{
	try {
		FFmpegDecoder dec;
		dec.open(inFile);
		FFmpegEncoder enc(dec.getVideoParam(), dec.getAudioParam());
		enc.open(outFile);
		dec.seek(start);
		AVPacket* packet = NULL;
		bool wait_key = false;
		while (true)
		{
			int r = dec.readFrame();
			if (r < 0) break;
			// reach to split end
			if (dec.getPresentTimeStamp() > end)
				break;

			if (dec.getPresentTimeStamp() < start) 
			{// seek to before
				if (r == 0) {//decode and cache last video frame
					dec.decodeVideoFrame();
					wait_key = true;
				}
				continue;
			}
			else {
				AVPacket* pack = dec.getCurPacket();
				if (r == 0 && wait_key) {
					if (pack->flags & AV_PKT_FLAG_KEY) {
						wait_key = false;
						enc.writeVideoFrame(NULL, pack->dts);
						// save keyframe data
						enc.writeVideoData(pack->data, pack->size, true, dec.getPresentTimeStamp() * 1000);
					}
					else {
						// cache last 
						dec.decodeVideoFrame();
						enc.writeVideoFrame(dec.getVideoFrame(), pack->dts);
					}
				}
				else {
					if (r == 0) {
						enc.writeAudioData(pack->data, pack->size, dec.getPresentTimeStamp() * 1000);
					}
					else {
						enc.writeVideoData(pack->data, pack->size, pack->flags & AV_PKT_FLAG_KEY, dec.getPresentTimeStamp() * 1000);
					}
					// enc.writePacket(pack);
				}
			}
		}
	}
	catch (...) {
		return false;
	}
}

#include "libavutil\error.h"
bool FFmpegDecoder::split2(const char * in_filename, const char * out_filename, double start, double end)
{
	AVFormatContext *ifmt_ctx = NULL, *ofmt_ctx = NULL;
	AVPacket pkt;
	av_register_all();
	int ret;
	// open input
	if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
		fprintf(stderr, "Could not open input file '%s'", in_filename);
		goto end;
	}

	if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
		fprintf(stderr, "Failed to retrieve input stream information");
		goto end;
	}
	av_dump_format(ifmt_ctx, 0, in_filename, 0);
	// prepare output
	avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename);
	if (!ofmt_ctx) {
		fprintf(stderr, "Could not create output context\n");
		ret = AVERROR_UNKNOWN;
		goto end;
	}
	// copy stream
	for (size_t i = 0; i < ifmt_ctx->nb_streams; i++)
	{
		AVStream *in_stream = ifmt_ctx->streams[i];
		AVCodecContext *in_codec = in_stream->codec;
		AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_codec->codec);
		avcodec_copy_context(out_stream->codec, in_codec);
		out_stream->time_base = in_stream->time_base;
		ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
		out_stream->codecpar->codec_tag = 0;
		if (in_codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			// open video decoder and encoder
			ret = avcodec_open(in_codec, avcodec_find_decoder(in_codec->codec_id));
			if (ret < 0) { fprintf(stderr, "unable to find decoder %d\n", in_codec->codec_id); }
			in_codec->refcounted_frames = 1;
			ret = avcodec_open(out_stream->codec, avcodec_find_encoder(in_codec->codec_id));
			if (ret < 0) { fprintf(stderr, "unable to find encoder %d\n", in_codec->codec_id); }
		}
	}
	av_dump_format(ofmt_ctx, 0, out_filename, 1);
	
	// open output file
	if (!(ofmt_ctx->flags & AVFMT_NOFILE)) {
		ret = avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE);
		if (ret < 0) {
			fprintf(stderr, "Could not open output file '%s'\n", out_filename);
			goto end;
		}
	}
	ret = avformat_write_header(ofmt_ctx, NULL);
	if (ret < 0) {
		fprintf(stderr, "Error occurred when opening output file\n");
		goto end;
	}

	// seek with start
	ret = av_seek_frame(ifmt_ctx, -1, start * AV_TIME_BASE, AVSEEK_FLAG_BACKWARD);
	if (ret < 0) {
		fprintf(stderr, "Error occurred when seek to %f\n", start);
		goto end;
	}

	int got_picture = 0;
	AVFrame* last_pic = av_frame_alloc();
	bool wait_key = false;
	while (1) {
		ret = av_read_frame(ifmt_ctx, &pkt);
		if (ret < 0)
			break;

		AVStream* in_stream = ifmt_ctx->streams[pkt.stream_index];
		AVStream* out_stream = ofmt_ctx->streams[pkt.stream_index];
		double pts = pkt.pts * av_q2d(in_stream->time_base);
		if (pts > end)
			break;

		/* copy packet */
		//pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
		//pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX);
		//pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
		// offset timestamp
		pkt.pts -= start / av_q2d(in_stream->time_base);
		pkt.dts -= start / av_q2d(in_stream->time_base);
		pkt.pos = -1;

		// is video
		bool video = in_stream->codec->codec_type == AVMEDIA_TYPE_VIDEO;
		// is keyframe
		bool key = pkt.flags & AV_PKT_FLAG_KEY;
		if (pkt.pts < 0) {
			if (video) {
				avcodec_decode_video2(in_stream->codec, last_pic, &got_picture, &pkt);
				wait_key = true;
			}
			av_packet_unref(&pkt);
			continue;
		}
		else {
			if (video && wait_key) {
				if (key)
					wait_key = false;
				else {
					avcodec_decode_video2(in_stream->codec, last_pic, &got_picture, &pkt);
					if (got_picture && last_pic) {
						AVPacket out_pkt = {0};
						av_init_packet(&out_pkt);
						// vp8 crash?
						avcodec_encode_video2(out_stream->codec, &out_pkt, last_pic, &got_picture);
						if (got_picture) {
							av_frame_unref(last_pic);
							out_pkt.pts = pkt.pts;
							out_pkt.dts = pkt.dts;
							out_pkt.stream_index = pkt.stream_index;
							ret = av_interleaved_write_frame(ofmt_ctx, &out_pkt);
							if (ret < 0) {
								fprintf(stderr, "Error muxing packet %lld\n", pkt.pts);
								break;
							}
						}
					}
					av_packet_unref(&pkt);
					continue;
				}
			}
		}
		ret = av_interleaved_write_frame(ofmt_ctx, &pkt);
		if (ret < 0) {
			fprintf(stderr, "Error muxing packet %lld\n", pkt.pts);
			break;
		}
		av_packet_unref(&pkt);
	}

	av_write_trailer(ofmt_ctx);
	av_frame_free(&last_pic);
end:
	// @todo
	// avcodec_close();

	avformat_close_input(&ifmt_ctx);

	/* close output */
	if (ofmt_ctx && !(ofmt_ctx->flags & AVFMT_NOFILE))
		avio_closep(&ofmt_ctx->pb);
	avformat_free_context(ofmt_ctx);

	if (ret < 0 && ret != AVERROR_EOF) {
		//fprintf(stderr, "Error occurred: %s\n", av_err2str(ret));
		return 1;
	}
	return 0;
}