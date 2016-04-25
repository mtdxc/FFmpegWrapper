///
/// @file
///
/// @brief  Implementation file for encoder of FFmpeg
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
///   Changed:  Bug fix, change the video input format to AVPicture, 
///             add pixel format convertor
/// - Version:  0.2.1
///   Author:   John (john.zywu@gmail.com)
///   Date:     2008/06/26
///   Changed:  Fixed the memory leak bug, changed some of the interfaces
///

#include "stdafx.h"
#include "FFmpegEncoder.h"
#include "Resample.h"
#include "AutoLock.h"
using namespace std;

// define the max audio packet size as 128 KB
#define MAX_AUDIO_PACKET_SIZE (128 * 1024)

FFmpegEncoder::FFmpegEncoder(const FFmpegVideoParam &videoParam, const FFmpegAudioParam &audioParam)
	: videoParam(videoParam), audioParam(audioParam)
{
	// initialize the private fields
	outputContext = NULL;
	videoStream = NULL;
	audioStream = NULL;
	videoFrame = NULL;
	videoCodecContext = NULL;
	audioCodecContext = NULL;
	bFlipUV = bFlipV = FALSE;
#ifdef FFMPEG51
	audioBuffer = NULL;
#endif
	audioBufferSize = 0;
	m_pMutex = new Mutex;
	opened = false;
	hasOutput = false;
	encodeVideo = !videoParam.empty();
	encodeAudio = !audioParam.empty();
	img_convert_ctx = NULL;
	m_pResample = NULL;
	reqKeyFrame = false;
	audioPkt = new AVPacket();
	videoPkt = new AVPacket();
	// initialize libavcodec, and register all codecs and formats
	av_register_all();
}

FFmpegEncoder::~FFmpegEncoder()
{
	close();
	delete m_pMutex;
	delete audioPkt;
	delete videoPkt;
}

//////////////////////////////////////////////////////////////////////////
//
//  Public properties
//
//////////////////////////////////////////////////////////////////////////

const uint8_t *FFmpegEncoder::getVideoEncodedBuffer() const
{
	return videoPkt ? videoPkt->data : NULL;
}

bool FFmpegEncoder::getVideoEncodeType()
{
	if (videoPkt){
		return videoPkt->flags;
	}
	/*
	if(videoCodecContext && videoCodecContext->coded_frame){
	return isKeyFrame(videoCodecContext->coded_frame);
	}*/
	return 0;
}

double FFmpegEncoder::getVideoTimeStamp() const
{
	if (!opened || !encodeVideo || !hasOutput)
	{
		return 0;
	}
	return (double)videoStream->pts.val * videoStream->time_base.num / videoStream->time_base.den;
}

const FFmpegVideoParam &FFmpegEncoder::getVideoParam() const
{
	return videoParam;
}

int FFmpegEncoder::getVideoFrameSize() const
{
	if (!opened || !encodeVideo)
	{
		return 0;
	}
	return avpicture_get_size((AVPixelFormat)videoParam.pixelFormat, videoParam.width, videoParam.height);
}

const uint8_t *FFmpegEncoder::getAudioEncodedBuffer() const
{
	return audioPkt ? audioPkt->data : NULL;
}

double FFmpegEncoder::getAudioTimeStamp() const
{
	if (!opened || !encodeAudio || !hasOutput)
	{
		return 0;
	}
	return (double)audioStream->pts.val * audioStream->time_base.num / audioStream->time_base.den;
}

const FFmpegAudioParam &FFmpegEncoder::getAudioParam() const
{
	return audioParam;
}

int FFmpegEncoder::getAudioFrameSize() const
{
	if (!opened || !encodeAudio)
	{
		return 0;
	}
	return audioBufferSize;

	int frameSize = 0;
	AVCodecContext* c = audioCodecContext;
	if (c && c->frame_size > 1)
	{
		/*
		frameSize  = audioCodecContext->frame_size;
		frameSize *= audioCodecContext->channels;    // multiply the channels
		frameSize *= sizeof(short); // convert to bytes
		*/
		frameSize = av_samples_get_buffer_size(NULL, c->channels, c->frame_size, c->sample_fmt, 0);
	}
	else
	{
		frameSize = audioBufferSize;  // including all channels, return bytes directly
	}
	return frameSize;
}


//////////////////////////////////////////////////////////////////////////
//
//  Public Methods
//
//////////////////////////////////////////////////////////////////////////

int FFmpegEncoder::encodeVideoFrame(const uint8_t *frameData)
{
	if (!opened)
	{
		throw runtime_error("Please call open() before encoding.");
	}

	if (!encodeVideo)
	{
		throw runtime_error("Can not encode video frame because video codec is not initialized.");
	}

	if (hasOutput)
	{
		throw runtime_error("Please use writeVideoFrame() if there is output.");
	}

	// encode the image frame
	AVPicture picture;
	avpicture_fill(&picture, (uint8_t *)frameData, (AVPixelFormat)videoParam.pixelFormat, videoParam.width, videoParam.height);
	return encodeVideoData(&picture);
}

int FFmpegEncoder::writeVideoFrame(const uint8_t *frameData, int64_t tsp)
{
	if (!opened)
	{
		throw runtime_error("Please call open() before encoding.");
	}

	if (!encodeVideo)
	{
		throw runtime_error("Can not encode video frame because video codec is not initialized.");
	}

	if (!hasOutput)
	{
		throw runtime_error("Please use encodeVideoFrame() if there is no output.");
	}

	// encode the image
	AVPicture picture;
	avpicture_fill(&picture, (uint8_t *)frameData, (AVPixelFormat)videoParam.pixelFormat, videoParam.width, videoParam.height);
	int encodedSize = encodeVideoData(&picture, tsp);

	// output the encoded image data
	if (encodedSize > 0)
	{
		writePack(true);
	}
	return encodedSize;
}

int FFmpegEncoder::encodeAudioFrame(const uint8_t *frameData, int dataSize)
{
	if (!opened)
	{
		throw runtime_error("Please call open() before encoding.");
	}

	if (!encodeAudio)
	{
		throw runtime_error("Can not encode video frame because audio codec is not initialized.");
	}

	if (hasOutput)
	{
		throw runtime_error("Please use writeAudioFrame() if there is output.");
	}

	if (audioCodecContext->frame_size <= 1 && dataSize < 1)
	{
		throw runtime_error("Parameter dataSize must be set for some specific (e.g. PCM) codecs.");
	}

	return encodeAudioData((uint8_t*)frameData, dataSize);
}

int FFmpegEncoder::writeAudioFrame(const uint8_t *frameData, int dataSize)
{
	if (!opened)
	{
		throw runtime_error("Please call open() before encoding.");
	}

	if (!encodeAudio)
	{
		throw runtime_error("Can not encode video frame because audio codec is not initialized.");
	}

	if (!hasOutput)
	{
		throw runtime_error("Please use encodeVideoFrame() if there is no output.");
	}

	if (audioCodecContext->frame_size <= 1 && dataSize < 1)
	{
		throw runtime_error("Parameter dataSize must be set for some specific (e.g. PCM) codecs.");
	}

	// encode the audio data
	int encodedSize = encodeAudioData((uint8_t*)frameData, dataSize);

	// output the encoded audio data
	if (encodedSize > 0)
	{
		writePack(false);
		//writeAudioData(audioBuffer, encodedSize);
	}
	return encodedSize;
}

void FFmpegEncoder::open(const char *fileName, bool openCodec)
{
	AutoLock al(m_pMutex);
	if (opened)
	{
		throw runtime_error("The encoder is already opened. Call close before opening a new encoder.");
	}

	hasOutput = (fileName != NULL) && (fileName[0] != 0);
	if (!hasOutput && !videoParam.codecId && !audioParam.codecId)
	{
		throw invalid_argument("The encoder must have output file or video/audio codec set.");
	}

	// initialize the output format
	AVOutputFormat *outputFormat = NULL;
	if (hasOutput)
	{
		// find suitable output format according to the postfix of the filename
		outputFormat = av_guess_format(NULL, fileName, NULL);
		if (!outputFormat)
		{
			throw runtime_error("Couldn't find corresponding format for the output file.");
		}
	}

	// allocate the output media context
#ifdef FFMPEG51
	outputContext = av_alloc_format_context();
#else
	outputContext = avformat_alloc_context();
#endif
	if (!outputContext)
	{
		throw runtime_error("Couldn't allocate the output media context.");
	}

	if (hasOutput)
	{
		outputContext->oformat = outputFormat;
		strcpy_s(outputContext->filename, sizeof outputContext->filename, fileName);
	}

	// video related initialization if necessary
	if (encodeVideo)
	{
		// validate the video codec
		if ((!outputFormat || outputFormat->video_codec == AV_CODEC_ID_NONE) && videoParam.codecId == AV_CODEC_ID_NONE)
		{
			throw runtime_error("The video codec wasn't specified.");
		}

		// find the video encoder
		AVCodec *videoCodec = NULL;
		if (videoParam.codecId)
		{
			// use the codec name preferentially if it is specified in the input param
			// videoCodec = avcodec_find_encoder_by_name(videoParam.videoCodecName.c_str());
			videoCodec = avcodec_find_encoder((AVCodecID)videoParam.codecId);
		}
		else if (outputFormat)
		{
			// otherwise, use the codec guessed from the filename
			videoCodec = avcodec_find_encoder(outputFormat->video_codec);
		}
		if (!videoCodec)
		{
			throw runtime_error("Video codec not found.");
		}

		// add the video stream with stream id 0
#ifdef FFMPEG51
		videoStream = av_new_stream(outputContext, 0);
		videoStream->codec->codec_id = videoCodec->id;
		videoStream->codec->codec_type = CODEC_TYPE_VIDEO;
#else
		videoStream = avformat_new_stream(outputContext, videoCodec);
#endif
		if (!videoStream)
		{
			throw runtime_error("Couldn't allocate video stream.");
		}
		videoStream->id = outputContext->nb_streams - 1;

		// set the parameters for video codec context
		videoCodecContext = videoStream->codec;
		videoCodecContext->bit_rate = videoParam.bitRate;
		videoCodecContext->width = videoParam.width;
		videoCodecContext->height = videoParam.height;
		videoCodecContext->time_base.den = videoParam.frameRate;
		videoCodecContext->time_base.num = 1;
		// 不允许B帧被引用
		av_opt_set(videoCodecContext->priv_data, "b-pyramid", "none", 0);
		videoCodecContext->max_b_frames = 2;
		/* // x264额外编码参数
		// videoCodecContext->rc_strategy;
		if(videoCodec->id == AV_CODEC_ID_H264){
		av_opt_set(videoCodecContext->priv_data, "preset", "veryfast", 0);
		// av_opt_set(videoCodecContext->priv_data, "preset", "superfast", 0);
		// 实时编码关键看这句，上面那条无所谓
		av_opt_set(videoCodecContext->priv_data, "tune", "zerolatency", 0);
		}
		videoCodecContext->qcompress = 0.0;//0.0 => CBR, 1.0 => CQP. Recommended default: -qcomp 0.60
		// 最大关键帧距
		videoCodecContext->gop_size = 128;
		// 禁用B帧
		videoCodecContext->max_b_frames = 0;
		// 多线程编码
		// videoCodecContext->thread_count = 1;
		// 采用HEX,还有个是DIA
		videoCodecContext->me_method = ME_HEX;
		// ABR?
		videoCodecContext->rc_min_rate = videoParam.bitRate/10;//1000;
		videoCodecContext->rc_max_rate = videoParam.bitRate;
		// videoCodecContext->bit_rate_tolerance = videoParam.bitRate;
		/// END
		*/

		// set the PixelFormat of the target encoded video
		if (videoCodec->pix_fmts)
			videoCodecContext->pix_fmt = SelectBestFormat(videoCodec, (AVPixelFormat)videoParam.pixelFormat);
		/* Some formats want stream headers to be separate. */
		if (outputFormat && outputFormat->flags & AVFMT_GLOBALHEADER)
			videoCodecContext->flags |= CODEC_FLAG_GLOBAL_HEADER;

		// allocate the output buffer
		// the maximum possible buffer size could be the raw bmp format with R/G/B/A
		// videoBufferSize = 4 * videoParam.width * videoParam.height;
		av_init_packet(videoPkt);

		if (openCodec)
		{
			// open the video codec
			if (avcodec_open(videoCodecContext, videoCodec) < 0)
			{
				throw runtime_error("Could not open the video codec.");
			}

			// allocate the temporal video frame buffer for pixel format conversion if needed
			if (videoParam.pixelFormat != videoCodecContext->pix_fmt)
			{
				videoFrame = (AVPicture *)av_malloc(sizeof(AVPicture));
				if (videoFrame == NULL
					|| avpicture_alloc(videoFrame, videoCodecContext->pix_fmt, videoCodecContext->width, videoCodecContext->height) < 0)
				{
					throw runtime_error("Couldn't allocate the video frame for pixel format conversion.");
				}
			}
		}
	}

	// audio related initialization if necessary
	if (encodeAudio)
	{
		// validate the audio codec
		if ((!outputFormat || outputFormat->audio_codec == AV_CODEC_ID_NONE) && audioParam.codecId == AV_CODEC_ID_NONE)
		{
			throw runtime_error("The audio codec wasn't specified.");
		}

		// find the audio encoder
		AVCodec *audioCodec = NULL;
		if (audioParam.codecId)
		{
			// use the codec name preferentially if it is specified in the input param
			// audioCodec = avcodec_find_encoder_by_name(audioParam.audioCodecName.c_str());
			audioCodec = avcodec_find_encoder((AVCodecID)audioParam.codecId);
		}
		else if (outputFormat)
		{
			// otherwise, use the codec guessed from the filename
			audioCodec = avcodec_find_encoder(outputFormat->audio_codec);
		}
		if (!audioCodec)
		{
			throw runtime_error("Audio codec not found.");
		}

		// add the audio stream with stream id 1
#ifdef FFMPEG51
		audioStream = av_new_stream(outputContext, 1);
		audioStream->codec->codec_id = audioCodec->id;
		audioStream->codec->codec_type = CODEC_TYPE_AUDIO;
#else
		audioStream = avformat_new_stream(outputContext, audioCodec);
#endif
		if (!audioStream)
		{
			throw runtime_error("Couldn't allocate audio stream.");
		}
		audioStream->id = outputContext->nb_streams - 1;

		// set the parameters for audio codec context
		audioCodecContext = audioStream->codec;
		audioCodecContext->bit_rate = audioParam.bitRate;
		audioCodecContext->sample_rate = audioParam.sampleRate;
		audioCodecContext->channels = audioParam.channels;
		// 采用格式自动匹配
		// audioCodecContext->sample_fmt	  = audioCodec->sample_fmts?audioCodec->sample_fmts[0]:SAMPLE_FMT_FLTP;
		if (audioCodec->sample_fmts)
			audioCodecContext->sample_fmt = SelectBestFormat(audioCodec, audioParam.sampleFormat);
		/* Some formats want stream headers to be separate. */
		if (outputFormat && outputFormat->flags & AVFMT_GLOBALHEADER)
			audioCodecContext->flags |= CODEC_FLAG_GLOBAL_HEADER;
		// -stick -2
		audioCodecContext->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

		av_init_packet(audioPkt);
		if (openCodec)
		{
			// open the audio codec
			if (avcodec_open(audioCodecContext, audioCodec) < 0)
			{
				throw runtime_error("Could not open the audio codec.");
			}

			// TODO: how to determine the buffer size?
			// allocate the output buffer
#ifdef FFMPEG51
			audioBufferSize = 4 * MAX_AUDIO_PACKET_SIZE;
			audioBuffer = (uint8_t*)(av_malloc(audioBufferSize));
#endif
			// 16位采样(要乘以2)
			audioBufferSize = audioCodec->capabilities & CODEC_CAP_VARIABLE_FRAME_SIZE ?
				10000 : audioCodecContext->frame_size;
			if (audioParam.isDiff(audioCodecContext))
			{
				m_pResample = new CResample();
				m_pResample->SetSrcArgs(audioParam);
				m_pResample->SetDstArgs(audioCodecContext);
				if (!m_pResample->Init(audioBufferSize))
				{
					throw runtime_error("unable to init audio resample.");
				}
			}
			audioBufferSize = av_samples_get_buffer_size(NULL, audioCodecContext->channels, audioBufferSize,
				audioCodecContext->sample_fmt, 0);
		}
	}
#ifdef FFMPEG51
	// set the output parameters (must be done even if no parameters).
	if (hasOutput)
	{
		if (av_set_parameters(outputContext, NULL) < 0)
		{
			throw runtime_error("Invalid output format parameters.");
		}
	}
#endif

	av_dump_format(outputContext, 0, fileName, 1);

	// open the output file, if needed
	if (hasOutput)
	{
#ifdef FFMPEG51
		if (url_fopen(&outputContext->pb, fileName, URL_WRONLY) < 0)
#else
		if (avio_open(&outputContext->pb, fileName, AVIO_FLAG_WRITE) < 0)
#endif
		{
			throw runtime_error(string("Could not open the file: ") + fileName);
		}

		// write the stream header, if any
#ifdef FFMPEG51
		if (av_write_header(outputContext))
#else
		if (avformat_write_header(outputContext, NULL))
#endif
		{
			throw runtime_error("Could not write the video header.");
		}
	}
	opened = true;
}

void FFmpegEncoder::close()
{
	AutoLock al(m_pMutex);
	if (!opened)
	{
		return;
	}

	if (hasOutput)
	{
		// write the trailer, and close the output file
		av_write_trailer(outputContext);
#ifdef FFMPEG51
		url_fclose(&outputContext->pb);
#else
		avio_close(outputContext->pb);
#endif
	}

	if (encodeVideo)
	{
		// close the video stream and codec
		if (videoCodecContext){
			avcodec_close(videoCodecContext);
			av_freep(&videoCodecContext);
		}
		av_freep(&videoStream);
		av_free_packet(videoPkt);
		if (videoFrame != NULL)
		{
			avpicture_free(videoFrame);
			av_freep(&videoFrame);
		}
		if (img_convert_ctx){
			sws_freeContext(img_convert_ctx);
			img_convert_ctx = NULL;
		}
	}

	if (encodeAudio)
	{
		// close the audio stream and codec
		if (audioCodecContext){
			avcodec_close(audioCodecContext);
			av_freep(&audioCodecContext);
		}
		av_freep(&audioStream);
#ifdef FFMPEG51
		av_freep(&audioBuffer);
#endif
		av_free_packet(audioPkt);
		audioBufferSize = 0;
	}
	RlsResample();

	av_freep(&outputContext);

	opened = false;
	hasOutput = false;
}


//////////////////////////////////////////////////////////////////////////
//
//  Private Methods
//
//////////////////////////////////////////////////////////////////////////

int FFmpegEncoder::encodeVideoData(AVPicture *picture, int64_t tsp)
{

	AVFrame frame = { 0 };
	// set default value
	// avcodec_get_frame_defaults(&frame);

	// convert the pixel format if needed
	if (videoParam.pixelFormat != videoCodecContext->pix_fmt)
	{
		// 返回的是转换的高度
		if (convertPixFmt(picture, videoFrame, &videoParam, videoCodecContext) <= 0)
		{
			throw runtime_error("Fail to convert the image pixel format.");
		}

		// fill the frame
		(AVPicture&)frame = *videoFrame;
	}
	else
	{
		// fill the frame
		(AVPicture&)frame = *picture;
	}
	FlipYUV(&frame, videoCodecContext, bFlipV, bFlipUV);
	frame.pts = tsp;
	if (reqKeyFrame)
	{// 只有改成264零延迟压缩方式才没问题，否则会产生N多个关键帧，造成视频文太大
		frame.pict_type = AV_PICTURE_TYPE_I;
		frame.key_frame = 1;
	}

	av_free_packet(videoPkt);
	av_init_packet(videoPkt);
	int got_packet;
	int ret = avcodec_encode_video2(videoCodecContext, videoPkt, &frame, &got_packet);
	// encode the frame
	// int encodedSize = avcodec_encode_video(videoCodecContext, videoBuffer, videoBufferSize, frame);	
	if (!ret && got_packet && videoPkt->size) {
		AVFrame* pCoded = videoCodecContext->coded_frame;
		printf("%d：%d\n", pCoded->pict_type, videoPkt->size);
		// 不写B帧
		static int bCount = 1;
		if (pCoded->pict_type == AV_PICTURE_TYPE_B)
		{// B帧这样丢了也有问题！是不是要把之后所以B帧都给丢了？
			if (!(bCount++ % 2)){
				printf("drop bFrame %d\n", bCount);
				return 0;
			}
		}
		/* 不能丢失P帧否则马赛克N多
		static int pCount = 0;
		if (pCoded->pict_type == AV_PICTURE_TYPE_P){
		if (pCount++ % 3)return 0;
		}*/
		if (isPacketKey(videoPkt))
			reqKeyFrame = 0;
		else if (reqKeyFrame){
			printf("reqKeyFrame NoKeyFrame\n");
		}
		return videoPkt->size;
	}
	else{
		return 0;
	}
	/*
	if (encodedSize < 0)
	{
	throw runtime_error("Error while encoding the video frame.");
	}
	else
	{
	return encodedSize;
	}*/
}

void FFmpegEncoder::writePack(int type)
{
	AVPacket* pkt = NULL;
	if (type)
	{
		videoPkt->stream_index = videoStream->index;
		pkt = videoPkt;
	}
	else
	{
		audioPkt->stream_index = audioStream->index;
		pkt = audioPkt;
	}
	AutoLock al(m_pMutex);
	// write the compressed frame in the media file
	int success = av_write_frame(outputContext, pkt);
	if (success < 0)
	{
		throw runtime_error("Error while writing video frame.");
	}
}

void FFmpegEncoder::writeVideoData(uint8_t *packetData, int packetSize, bool bKey, uint64_t tsp)
{
	AVPacket packet;
	av_init_packet(&packet);
	if (tsp == 0xFFFFFFFF)
	{
		packet.pts = getStreamPts(videoStream);
		if (videoCodecContext->coded_frame)
			SetPacketKey(&packet, videoCodecContext->coded_frame->key_frame);
		else
			SetPacketKey(&packet, bKey);
	}
	else
	{
		packet.pts = ms2pts(videoStream, tsp);
		SetPacketKey(&packet, bKey);
	}
	packet.stream_index = videoStream->index;
	packet.data = packetData;
	packet.size = packetSize;

	AutoLock al(m_pMutex);
	if (!outputContext) return;
	// write the compressed frame in the media file
	int success = av_write_frame(outputContext, &packet);

	av_free_packet(&packet);

	if (success < 0)
	{
		throw runtime_error("Error while writing video frame.");
	}
}

int FFmpegEncoder::convertPixFmt(AVPicture *srcPic, AVPicture *dstPic,
	const FFmpegVideoParam *srcParam, const AVCodecContext *dstContext)
{
	if (img_convert_ctx == NULL)
	{
		// 格式和大小改变
		img_convert_ctx = sws_getContext(
			srcParam->width, srcParam->height, (AVPixelFormat)srcParam->pixelFormat,
			dstContext->width, dstContext->height, dstContext->pix_fmt,
			SWS_BICUBIC, NULL, NULL, NULL);
	}

	if (img_convert_ctx == NULL)
	{
		throw runtime_error("Cannot initialize the image conversion context.");
	}

	return sws_scale(img_convert_ctx, srcPic->data, srcPic->linesize, 0, srcParam->height,
		dstPic->data, dstPic->linesize);
}

int FFmpegEncoder::encodeAudioData(uint8_t *frameData, int dataSize)
{
	// the output size of the buffer which stores the encoded data
	int audioSize = audioBufferSize;

	if (audioCodecContext->frame_size <= 1 && dataSize > 0)
	{
		// For PCM related codecs, the output size of the encoded data is
		// calculated from the size of the input audio frame.
		audioSize = dataSize;

		// The following codes are used for calculating "short" size from original "sample" size.
		// The codes are not needed any more because now the input size is already in "short" unit.
	}
	// av_init_packet(&audioPkt);
	bool bEnc = true;
	AVFrame frame = { 0 };
	//avcodec_get_frame_defaults(&frame);

	if (!m_pResample){
		// 不这么设的话，好像avcodec_fill_audio_frame不工作
		frame.nb_samples = dataSize / audioParam.channels / av_get_bytes_per_sample(audioParam.sampleFormat);
		avcodec_fill_audio_frame(&frame, audioParam.channels, audioParam.sampleFormat, frameData, dataSize, 0);
	}
	else
		bEnc = m_pResample->FillFrame(&frame, frameData, dataSize);
	if (bEnc)
	{
		int got_frame = 0;
		av_free_packet(audioPkt);
		av_init_packet(audioPkt);
		avcodec_encode_audio2(audioCodecContext, audioPkt, &frame, &got_frame);
		return (got_frame);
	}
	return 0;
#ifdef FFMPEG51
	// encode the frame
	int encodedSize = avcodec_encode_audio(audioCodecContext, audioBuffer, audioSize, (short*)frameData);
	if (encodedSize < 0)
	{
		throw runtime_error("Error while encoding the audio frame.");
	}
	else
	{
		return encodedSize;
	}
#endif
}

void FFmpegEncoder::writeAudioData(uint8_t *packetData, int packetSize, uint64_t tsp)
{
	AVPacket packet;
	av_init_packet(&packet);
	if (tsp == 0xFFFFFFFF)
		packet.pts = getStreamPts(audioStream);
	else
		packet.pts = ms2pts(audioStream, tsp);
	// SetPacketKey(&packet, true);
	packet.flags |= AV_PKT_FLAG_KEY;
	packet.stream_index = audioStream->index;
	packet.data = packetData;
	packet.size = packetSize;

	AutoLock al(m_pMutex);
	if (!outputContext) return;
	// write the compressed frame in the media file
	int success = av_write_frame(outputContext, &packet);

	av_free_packet(&packet);

	if (success < 0)
	{
		throw runtime_error("Error while writing audio frame.");
	}
}

int64_t FFmpegEncoder::getStreamPts(AVStream* pStm)
{
	if (pStm->codec && pStm->codec->coded_frame
		&& pStm->codec->coded_frame->pts != AV_NOPTS_VALUE)
	{
		return av_rescale_q(pStm->codec->coded_frame->pts, pStm->codec->time_base, pStm->time_base);
	}
	return 0;
}

int64_t FFmpegEncoder::ms2pts(bool bAudio, int ms)
{
	return ms2pts(bAudio ? audioStream : videoStream, ms);
}

int64_t FFmpegEncoder::ms2pts(AVStream* pStream, int ms)
{
	return pStream ? (int64_t)ms*pStream->time_base.den / pStream->time_base.num / 1000 : 0;
}

void FFmpegEncoder::RlsResample()
{
	if (m_pResample)
	{
		delete m_pResample;
		m_pResample = NULL;
	}
}
