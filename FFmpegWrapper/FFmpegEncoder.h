///
/// @file
///
/// @brief  Head file for encoder of FFmpeg
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

#ifndef FFmpegEncoder_H
#define FFmpegEncoder_H

#include "FFmpegVideoParam.h"
#include "FFmpegAudioParam.h"

struct SwsContext;
struct AVStream;
struct AVFormatContext;
struct AVPicture;
struct AVPacket;
class Mutex;
class CResample;
#ifndef AV_NOPTS_VALUE 
#define AV_NOPTS_VALUE          ((int64_t)(0x8000000000000000))
#endif
///
/// @brief  The video/audio encoder class for wrapping FFmpeg encoding API.
///
class FFMPEG_EXPORT FFmpegEncoder
{
public:
    //////////////////////////////////////////////////////////////////////////
    //
    //  Initialization and finalization
    //
    //////////////////////////////////////////////////////////////////////////

    ///
    /// @brief  Constructor for initializing an object of FFmpegEncoder.
    ///
    /// If the video/audio stream is not needed, pass an empty FFmpegVideoParam/FFmpegAudioParam directly.
    ///	For example, the following codes create a "flv" encoder with audio only:
    /// @verbatim
    ///		FFmpegVideoParam videoParam;
    ///		FFmpegAudioParam audioParam(64000, 44100, 2);
    ///		FFmpegEncoder testVideo(videoParam, audioParam);
    ///     testVideo.open("test.flv");
    ///     //... codes go here
    ///     testVideo.close();
    /// @endverbatim
    ///
    /// If there is no output, name of video/audio codec must be specified in the FFmpegVideoParam/FFmpegAudioParam.
    ///	For example:
    /// @verbatim
    ///		FFmpegVideoParam videoParam(352, 288, PIX_FMT_YUV420P, 400000, 25, "flv");
    ///		FFmpegAudioParam audioParam(64000, 44100, 2, "libmp3lame");
    ///		FFmpegEncoder testVideo(videoParam, audioParam);
    ///     testVideo.open();
    ///     //... codes go here
    ///     testVideo.close();
    /// @endverbatim
    ///
    /// @param  [in] videoParam   The video parameters to initialize the FFmpegEncoder.
    /// @param  [in] audioParam   The audio parameters to initialize the FFmpegEncoder.
    ///
	FFmpegEncoder(const FFmpegVideoParam &videoParam, const FFmpegAudioParam &audioParam);

    ///
    /// @brief  Destructor
    ///
	virtual ~FFmpegEncoder();


public:
    //////////////////////////////////////////////////////////////////////////
    //
    //  Public properties
    //
    //////////////////////////////////////////////////////////////////////////

    ///
    /// @brief  Get the video buffer which contains the encoded frame data.
    ///
    /// @return An uint8_t pointer to the encoded video frame data.
    /// @retval NULL Encoder is not opened or there is no video to be encoded.
    ///
	const uint8_t *getVideoEncodedBuffer() const;
	/// 判断上一帧视频类型
	bool getVideoEncodeType();
    ///
    /// @brief  Get the presentation time stamp of the video stream being encoded.
    ///
    /// This is usually used for synchronizing the video and audio streams.
    ///
    /// @return A non-negative double representing the time stamp (in seconds).
    /// @retval 0 Encoder is not opened or there is no video to be encoded or output.
    ///
	double getVideoTimeStamp() const;

    ///
    /// @brief  Get the video parameters
    ///
	const FFmpegVideoParam &getVideoParam() const;

    ///
    /// @brief  Get the size of the raw video frame (unit: in bytes/uint8_t).
    ///
    /// This is usually used for allocating memory for the raw video frame data.
    ///
    /// @return A non-negative int representing the number of bytes (unit: in bytes/uint8_t).
    /// @retval 0 Encoder is not opened or there is no video to be encoded.
    ///
    int getVideoFrameSize() const;

    ///
    /// @brief  Get the audio buffer which contains the encoded frame data.
    ///
    /// @return An uint8_t pointer to the encoded audio frame data.
    /// @retval NULL Encoder is not opened or there is no audio to be encoded.
    ///
	const uint8_t *getAudioEncodedBuffer() const;

    ///
    /// @brief  Get the presentation time stamp of the audio stream being encoded.
    ///
    /// This is usually used for synchronizing the video and audio streams.
    ///
    /// @return A non-negative double representing the time stamp (in seconds).
    /// @retval 0 Encoder is not opened or there is no audio to be encoded or output.
    ///
	double getAudioTimeStamp() const;

    ///
    /// @brief  Get the audio parameters
    ///
	const FFmpegAudioParam &getAudioParam() const;

    ///
    /// @brief  Get the size of the raw audio frame including all channels (unit: in bytes/uint8_t).
    ///
    /// This is usually used for allocating memory for the raw audio frame data.
    ///
    /// @return A non-negative int representing the number of bytes including all audio channels (unit: in bytes/uint8_t).
    /// @retval 0 Encoder is not opened or this is no audio to be encoded.
    ///
	int getAudioFrameSize() const;


public:
    //////////////////////////////////////////////////////////////////////////
    //
    //  Public Methods
    //
    //////////////////////////////////////////////////////////////////////////

	///
    /// @brief  Open the codec, output file and allocate the necessary internal structures.
    ///
    /// Must be called before encoding process.
    ///
    /// @param  [in/optional] fileName  The name of the file to which encoded results are written.
    ///
	void open(const char *fileName = NULL, bool openCodec=true);

    ///
    /// @brief  Close the codec, output file and release the memories
    ///
    /// Must be called after encoding process is finished.
    ///
    void close();

    ///
    /// @brief  Encode a specific video frame (just encode, won't write encoded data to output file).
    ///
    /// The encoded frame data can be retrieved by calling getVideoEncodedBuffer().
    /// Use this method only when this is no output, otherwise use writeVideoFrame() instead.
    ///
    /// @param  [in] frameData The image data of the frame to be encoded.
    ///
    /// @return A non-negative int representing the size of the encoded buffer.
    ///
    int encodeVideoFrame(const uint8_t *frameData);

    ///
    /// @brief  Encode a specific video frame and write encoded data to the output file.
    ///
    /// Use this method only when this is output, otherwise use encodeVideoFrame() instead.
    ///
    /// @param  [in] frameData The image data of the frame to be encoded.
    ///
    /// @return A non-negative int representing the size of the encoded buffer.
    ///
    int writeVideoFrame(const uint8_t *frameData, int64_t tsp = AV_NOPTS_VALUE);

    ///
    /// @brief  Encode a specific audio frame (just encode, won't write encoded data to output file).
    ///
    /// The encoded frame data can be retrieved by calling getAudioEncodedBuffer().
    /// Use this method only when this is no output, otherwise use writeAudioFrame() instead.
    ///
    /// @param  [in]          frameData The audio data of the frame to be encoded.
    /// @param  [in/optional] dataSize  The size of the audio frame data, required for PCM related codecs.
    ///
    /// @return A non-negative int representing the size of the encoded buffer.
    ///
    int encodeAudioFrame(const uint8_t *frameData, int dataSize = 0);

    ///
    /// @brief  Encode a specific audio frame and write encoded data to the output file.
    ///
    /// Use this method only when this is output, otherwise use encodeAudioFrame() instead.
    ///
    /// @param  [in]          frameData The audio data of the frame to be encoded.
    /// @param  [in/optional] dataSize  The size of the audio frame data, required for PCM related codecs.
    ///
    /// @return A non-negative int representing the size of the encoded buffer.
    ///
    int writeAudioFrame(const uint8_t *frameData, int dataSize = 0);
	/// 将ms转成pts
	inline int64_t ms2pts( AVStream* pStream, int ms);
	inline int64_t ms2pts( bool bAudio, int ms );
	///
	/// @brief  Write the encoded audio frame packet data to the output
	///
	/// @param  [in] packetData  The packet data of the encoded audio frame
	/// @param  [in] packetSize  The size of the encoded audio frame data
	///
	void writeAudioData(uint8_t *packetData, int packetSize, uint64_t tsp = 0xFFFFFFFF);
	///
	/// @brief  Write the encoded video frame packet data to the output
	///
	/// @param  [in] packetData  The packet data of the encoded video frame
	/// @param  [in] packetSize  The size of the encoded video frame data
	///
	void writeVideoData(uint8_t *packetData, int packetSize, bool bKey, uint64_t tsp = 0xFFFFFFFF);
	/// 请求压一个视频关键帧
	void ReqKeyFrame(){reqKeyFrame=true;}
	/*
	设置翻转标记，对整个编码过程生效
	@param vFlip 上下翻转
	@param uvFlip UV反转
	*/
	void SetFlip(bool vFlip,bool uvFlip){bFlipV=vFlip;bFlipUV=uvFlip;}
	bool bFlipV,bFlipUV;
	bool hasAudio(){return encodeAudio;}
	bool hasVideo(){return encodeVideo;}
	bool isOpen(){return opened;}
private:
    //////////////////////////////////////////////////////////////////////////
    //
    //  Private Definitions
    //
    //////////////////////////////////////////////////////////////////////////
	volatile bool reqKeyFrame;
	bool encodeVideo;   ///< Whether video encoding is needed
	bool encodeAudio;   ///< Whether audio encoding is needed
	bool hasOutput;     ///< Whether there is a output file for encoding
	bool opened;        ///< Whether the FFmpegEncoder is opened yet
	FFmpegVideoParam videoParam;        ///< The video parameters of the video to be encoded
	FFmpegAudioParam audioParam;        ///< The audio parameters of the audio to be encoded
	AVFormatContext *outputContext;     ///< The output format context
	AVStream *videoStream;              ///< The video output stream
	AVStream *audioStream;              ///< The audio output stream
	AVCodecContext *videoCodecContext;
	AVCodecContext *audioCodecContext;
    AVPicture *videoFrame;              ///< The temporary video frame for pixel format conversion
	AVPacket* videoPkt;			///< The video output buffer
	AVPacket* audioPkt;			///< The audio output buffer
#ifdef FFMPEG51
	int     videoBufferSize;    ///< The size of video output buffer
	uint8_t *audioBuffer;       ///< The audio output buffer
#endif
	int     audioBufferSize;    ///< The size of audio output buffer
	
	void RlsResample();
	// 音频重采样数据
	CResample* m_pResample;
	Mutex* m_pMutex;
	// 视频转换对象
	SwsContext *img_convert_ctx;
private:
    //////////////////////////////////////////////////////////////////////////
    //
    //  Private Methods
    //
    //////////////////////////////////////////////////////////////////////////

    ///
    /// @brief  Encode a video frame to the internal encoded data buffer
    ///
    /// @param  [in] picture    The video frame data to be encoded
    ///
    /// @return A non-negative int represents the size of the encoded data
    ///
	int encodeVideoData(AVPicture *picture, int64_t tsp=AV_NOPTS_VALUE);

	void writePack(int type);
    ///
    /// @brief  Convert the pixel format of the input image
    ///
    /// @param  [in]  srcParam    The parameters of the source image picture
    /// @param  [in]  dstContext  The codec context information of the output (destination) image picture
    /// @param  [in]  srcPic      The source image picture to be converted
    /// @param  [out] dstPic      Return the output image picture which has been converted
    ///
    /// @return 0 when conversion is successful, otherwise a negative int
    ///
    int convertPixFmt(AVPicture *srcPic, AVPicture *dstPic, const FFmpegVideoParam *srcParam, const AVCodecContext *dstContext);

    ///
    /// @brief  Encode an audio frame to the internal encoded data buffer
    ///
    /// @param  [in]          frameData  The audio frame data to be encoded
    /// @param  [in/optional] dataSize   The size of the audio frame to be encoded
    ///
    /// @return A non-negative int represents the size of the encoded data
    ///
	int encodeAudioData(uint8_t *frameData, int dataSize);

	int64_t getStreamPts(AVStream* pStm);
};

#endif//FFmpegEncoder_H
