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


#ifndef FFmpegDecoder_H
#define FFmpegDecoder_H

#include <string>

struct AVPacket;
struct AVFormatContext;
struct AVStream;
#include "FFmpegVideoParam.h"
#include "FFmpegAudioParam.h"

using namespace std;

///
/// @brief  The video decoder class for wrapping FFmpeg decoding API.
///
/// @todo   There may be several audio/video streams in the input file. However, only one stream is used here.
///
class FFMPEG_EXPORT FFmpegDecoder
{
public:
    //////////////////////////////////////////////////////////////////////////
    //
    //  Initialization and finalization
    //
    //////////////////////////////////////////////////////////////////////////

    ///
    /// @brief  Constructor for initializing an object of FFmpegDecoder.
    ///
    FFmpegDecoder();

    ///
    /// @brief  Destructor
    ///
    virtual ~FFmpegDecoder();


public:
    //////////////////////////////////////////////////////////////////////////
    //
    //  Public properties
    //
    //////////////////////////////////////////////////////////////////////////

    ///
    /// @brief  Get the video parameters which are available after open() is called
    ///
    const FFmpegVideoParam &getVideoParam() const;

    ///
    /// @brief  Get the audio parameters which are available after open() is called
    ///
    const FFmpegAudioParam &getAudioParam() const;

    ///
    /// @brief  Get the decoded video frame data.
    ///
    /// If the return value of decodeFrame() is 0 (i.e. video stream),
    /// use this method to get decoded video frame.
    ///
    /// Use getVideoFrameSize() to get the returned size of the decoded video frame data (unit: in bytes/uint8_t).
    ///
    /// @return A uint8_t pointer representing the decoded frame.
    ///
    const uint8_t *getVideoFrame() const;

    ///
    /// @brief  Get the size of the decoded video frame (unit: in bytes).
    ///
    /// If the return value of decodeFrame() is 0 (i.e. video stream),
    /// use this method to get the size of the decoded video frame.
    ///
    /// @return An int representing the bytes of the decoded video frame (unit: in bytes/uint8_t).
    ///
    int getVideoFrameSize() const;

    ///
    /// @brief  Get the decoded audio frame data containing all channels.
    ///
    /// If the return value of decodeFrame() is 1 (i.e. audio stream),
    /// use this method to get decoded audio frame.
    ///
    /// Use getAudioFrameSize() to get the returned size of the decoded audio frame data (unit: in bytes/uint8_t).
    ///
    /// @return A uint8_t pointer representing the decoded frame.
    ///
    const uint8_t *getAudioFrame() const;

    ///
    /// @brief  Get the size of the decoded audio frame containing all channels (unit: in bytes/uint8_t).
    ///
    /// If the return value of decodeFrame() is 1 (i.e. audio stream),
    /// use this method to get the size of the decoded audio frame.
    ///
    /// @return An int representing the bytes of the decoded audio frame containing all channels (unit: in bytes/uint8_t).
    ///
    int getAudioFrameSize() const;

    ///
    /// @brief  Get the presentation time stamp of current packet being decoded
    ///
    /// @return A double representing the presentation time stamp (in seconds).
    ///
    double getPresentTimeStamp() const;

    ///
    /// @brief  Get the decoding time stamp of current packet being decoded
    ///
    /// @return A double representing the decoding time stamp (in seconds).
    ///
    double getDecodeTimeStamp() const;

public:
    //////////////////////////////////////////////////////////////////////////
    //
    //  Public Methods
    //
    //////////////////////////////////////////////////////////////////////////

    ///
    /// @brief  Open the input file, codecs, and allocate the necessary structures and memories.
    ///
    /// @param  [in] fileName   The name of the input media file (including the extension).
    ///
    void open(const char *fileName);
	/*
	打开解码器
	@param vPar 视频参数 
	@param aPar 音频参数
	@return bool 成功则返回true 
	*/
	bool open(const FFmpegVideoParam& vPar, const FFmpegAudioParam& aPar);
    ///
    /// @brief  Close the input file, codecs, and release the memories.
    ///
    /// Must be called after decoding process is finished.
    ///
    void close();

    ///
    /// @brief  Read a frame from the input file and decode it.
    ///
    /// The frame can be a video frame or an audio frame.
    /// After decodeFrame() is called, you can call getVideoFrame()/getAudioFrame()
    /// or getVideoFrameSize()/getAudioFrameSize() to get what you want according to
    ///  the returned value of this function.
    ///
    /// @return An int representing the stream identity of the decoded frame.
    /// @retval 0  Video frame
    /// @retval 1  Audio frame
    /// @retval -1 Error occurred or end of file
    ///
    int decodeFrame();

	int decodeAudioFrame(uint8_t* data, int size);
	int decodeVideoFrame(uint8_t* data, int size);
	///
	/// @brief  Read a packet from the input file
	///
	/// @return Negative int on error, or zero on success
	/// @retval 0  Video frame
	/// @retval 1  Audio frame
	/// @retval -1 Error occurred or end of file
	int readFrame();

	/// get current packet data buffer
	const uint8_t *getRawFrame() const;
	/// get current packet data buffer size
	int getRawFrameSize() const;
	/// 返回当前读取的包(必须包含FFMPEG头文件后才能解析)
	AVPacket* getCurPacket();
private:

    //////////////////////////////////////////////////////////////////////////
    //
    //  Private Definitions
    //
    //////////////////////////////////////////////////////////////////////////

    bool decodeVideo;           ///< Whether video decoding is needed
    bool decodeAudio;           ///< Whether audio decoding is needed
    bool opened;                ///< Whether the FFmpegDecoder is opened yet
    FFmpegVideoParam videoParam;        ///< The video parameters of the video to be decoded
    FFmpegAudioParam audioParam;        ///< The audio parameters of the audio to be decoded
    AVFormatContext *inputContext;      ///< The input format context
    AVStream *videoStream;              ///< The video output stream
    AVStream *audioStream;              ///< The audio output stream
	// 采用指针防止头文件污染
    AVPacket *currentPacket;		///< the packet read from the input file currently
	// yuv
    uint8_t *videoFrameBuffer;  ///< The buffer storing one output video frame data
    int      videoFrameSize;    ///< The size of the output video frame
    int      videoBufferSize;   ///< The total size of the video output buffer
	// pcm
    uint8_t *audioFrameBuffer;  ///< The buffer storing one output audio frame data
    int      audioFrameSize;    ///< The size of the output audio frame
    int      audioBufferSize;   ///< The total size of the audio output buffer
	// 由于音频packet可以生成好多个Frame，因此要保存最后一个packet encode data(currentPacket的部分)
    uint8_t *audioPacketData;   ///< The remaining audio packet data to be decoded
    int      audioPacketSize;   ///< The size of the remaining audio packet data to be decoded

    double currentPacketPts;	///< The presentation time stamp of the current packet(s)
    double currentPacketDts;	///< The decompression time stamp of the current packet(s)

	AVCodecContext *audioCodecContext;
	AVCodecContext *videoCodecContext;
private:
    //////////////////////////////////////////////////////////////////////////
    //
    //  Private Methods
    //
    //////////////////////////////////////////////////////////////////////////

    ///
    /// @brief  Decode a video frame from the current packet, and store it in the video frame buffer
    ///
    void decodeVideoFrame(bool bFreePkt=true);

    ///
    /// @brief  Decode an audio frame from the current packet, and store it in the audio frame buffer
    ///
    void decodeAudioFrame(bool bFreePkt=true);
};

#endif//FFmpegDecoder_H
