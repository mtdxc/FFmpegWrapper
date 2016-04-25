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
    inputContext     = NULL;
    videoStream      = NULL;
    audioStream      = NULL;
    videoFrameBuffer = NULL;
    videoFrameSize   = 0;
    videoBufferSize  = 0;
    audioFrameBuffer = NULL;
    audioFrameSize   = 0;
    audioBufferSize  = 0;
    audioPacketData  = NULL;
    audioPacketSize  = 0;
    currentPacketPts = 0;
    currentPacketDts = 0;
    decodeVideo      = false;
    decodeAudio      = false;
    opened           = false;
	currentPacket	 = new AVPacket();
	audioCodecContext = NULL;
	videoCodecContext = NULL;
    // register all codecs and demux
    av_register_all();
}

FFmpegDecoder::~FFmpegDecoder()
{
    close();
	if(currentPacket)
	{
		delete currentPacket;
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
	return currentPacket?currentPacket->data:NULL;
}

int FFmpegDecoder::getRawFrameSize() const
{
	return currentPacket?currentPacket->size:0;
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
    if (av_open_input_file(&inputContext, fileName, NULL, 0, NULL))
    {
        throw runtime_error(string("Could not open the media file as input: ") + fileName);
    }

    // Read packets of a media file to get stream information.
    if (av_find_stream_info(inputContext) < 0)
    {
        throw runtime_error("Could not get stream information from the media file.");
    }

    // find the video/audio stream
    for (size_t i = 0; i < inputContext->nb_streams; i++)
    {
        // TODO
        // there might be several audio or video streams,
        // however, only one audio/video stream is used here
        if (!videoStream && inputContext->streams[i]->codec->codec_type == CODEC_TYPE_VIDEO)
        {
            videoStream = inputContext->streams[i];
            decodeVideo = true;
            continue;
        }

        if (!audioStream && inputContext->streams[i]->codec->codec_type == CODEC_TYPE_AUDIO)
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
        videoParam.frameRate   = videoStream->r_frame_rate.num / videoStream->r_frame_rate.den;

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
        audioBufferSize  = AVCODEC_MAX_AUDIO_FRAME_SIZE;
        audioFrameSize   = 0;
        audioFrameBuffer = (uint8_t *)av_malloc(audioBufferSize);
        audioPacketData  = NULL;
        audioPacketSize  = 0; // no data in the packet now, for initialization
    }

    opened = true;
}

bool FFmpegDecoder::open( const FFmpegVideoParam& vPar, const FFmpegAudioParam& aPar )
{
	videoParam = vPar;
	audioParam = aPar;
	if(vPar.codecId)
	{
		AVCodec* codec = avcodec_find_decoder((AVCodecID)vPar.codecId); 
		if(!codec)
		{
			throw runtime_error("unable to find video codec");
		}

		videoCodecContext = avcodec_alloc_context3(codec);
		if(vPar.width) videoCodecContext->width = vPar.width;
		if(vPar.height) videoCodecContext->height = vPar.height;
		// 这应该是输出格式
		if(codec->pix_fmts)
			videoCodecContext->pix_fmt = SelectBestFormat(codec,(AVPixelFormat)vPar.pixelFormat);
		if(avcodec_open(videoCodecContext, codec)<0)
			throw runtime_error("video codec open error");
		decodeVideo = true;
	}
	if(aPar.codecId)
	{
		AVCodec* codec = avcodec_find_decoder((AVCodecID)aPar.codecId); 
		if(!codec)
		{
			throw runtime_error("unable to find audio codec");
		}
		audioCodecContext = avcodec_alloc_context3(codec);
		if(aPar.sampleRate) audioCodecContext->sample_rate = aPar.sampleRate;
		if(aPar.channels) audioCodecContext->channels = aPar.channels;
		if(aPar.sampleFormat&&codec->sample_fmts) 
			audioCodecContext->sample_fmt = SelectBestFormat(codec, aPar.sampleFormat);
		if(avcodec_open(audioCodecContext, codec)<0)
			throw runtime_error("audio codec open error");
		decodeAudio = true;
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
		if(videoCodecContext){
			avcodec_close(videoCodecContext);
			// av_freep(videoCodecContext);
			videoCodecContext = NULL;
		}
		if(videoFrameBuffer)
			av_freep(&videoFrameBuffer);
        videoFrameSize = 0;
        videoBufferSize = 0;
    }
    if (decodeAudio)
    {
		if(audioCodecContext){
			avcodec_close(audioCodecContext);
			// av_freep(audioCodecContext);
			audioCodecContext = NULL;
		}
        av_freep(&audioFrameBuffer);
        audioFrameSize  = 0;
        audioBufferSize = 0;
        audioPacketData = NULL;
        audioPacketSize = 0;
    }
    // close the input file
	if(inputContext){
		av_close_input_file(inputContext);
		inputContext = NULL;
	}

    audioStream  = NULL;
    videoStream  = NULL;

	av_init_packet(currentPacket);

    audioParam  = FFmpegAudioParam();
    videoParam  = FFmpegVideoParam();

    opened      = false;
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
	if(videoStream && currentPacket->stream_index == videoStream->index)
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
    AVFrame videoFrame;

    // set default value
    avcodec_get_frame_defaults(&videoFrame);

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
		if(!videoFrameBuffer || 
			(videoParam.width != videoCodecContext->width || videoParam.height != videoCodecContext->height))
		{
			videoParam.width = videoCodecContext->width;
			videoParam.height= videoCodecContext->height;
			// allocate the video frame to be encoded
			videoBufferSize  = avpicture_get_size(videoCodecContext->pix_fmt, videoParam.width, videoParam.height);
			videoFrameSize   = 0;
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
	if(bFreePkt)
		av_free_packet(currentPacket);

    if (decodedSize < 0)
    {
        throw runtime_error("Fail to decode a video frame.");
    }
}

int FFmpegDecoder::decodeVideoFrame( uint8_t* data, int size )
{
	if(!videoCodecContext) return 0;
	currentPacket->data = data;
	currentPacket->size = size;
	decodeVideoFrame(false);
	return videoFrameSize;
}

int FFmpegDecoder::decodeAudioFrame( uint8_t* data, int size )
{
	if(!audioCodecContext) return 0;
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
	AVPacket pack;
	pack.data = audioPacketData;
	pack.size = audioPacketSize;
	decodedSize = avcodec_decode_audio3(audioCodecContext, 
		(int16_t *)audioFrameBuffer, &outputFrameSize,
		&pack);
	/*
	AVFrame frame; int got_frame =0;
	decodedSize = avcodec_decode_audio4(audioCodecContext, &frame, &got_frame, &pack);
	if(got_frame){
		av_samples_get_buffer_size()
	}*/
#endif

    audioFrameSize = outputFrameSize;

    if (audioPacketSize - decodedSize <= 0)
    {
        // all the audio frames in the packet have been decoded
        // free the packet now
		if(bFreePkt)
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

