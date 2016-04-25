///
/// @file
///
/// @brief  Head file for the audio parameters for encoder/decoder
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
///   Changed:  Bug fix
/// - Version:  0.2.1
///   Author:   John (john.zywu@gmail.com)
///   Date:     2008/06/26
///   Changed:  Changed some of the interfaces
///

#ifndef FFmpegAudioParam_H
#define FFmpegAudioParam_H

#ifndef AVUTIL_SAMPLEFMT_H
// 把ffmpeg对应的头文件拷贝出来，避免再次包含ffmpeg的库
enum AVSampleFormat {
	AV_SAMPLE_FMT_NONE = -1,
	AV_SAMPLE_FMT_U8,          ///< unsigned 8 bits
	AV_SAMPLE_FMT_S16,         ///< signed 16 bits
	AV_SAMPLE_FMT_S32,         ///< signed 32 bits
	AV_SAMPLE_FMT_FLT,         ///< float
	AV_SAMPLE_FMT_DBL,         ///< double

	AV_SAMPLE_FMT_U8P,         ///< unsigned 8 bits, planar
	AV_SAMPLE_FMT_S16P,        ///< signed 16 bits, planar
	AV_SAMPLE_FMT_S32P,        ///< signed 32 bits, planar
	AV_SAMPLE_FMT_FLTP,        ///< float, planar
	AV_SAMPLE_FMT_DBLP,        ///< double, planar

	AV_SAMPLE_FMT_NB           ///< Number of sample formats. DO NOT USE if linking dynamically
};
#endif

#ifdef FFWRAPPER_EXPORTS
#define FFMPEG_EXPORT _declspec(dllexport)
#else
#define FFMPEG_EXPORT _declspec(dllimport)
#endif

struct AVCodecContext;
///
/// @brief  The audio parameter class for FFmpegEncoder initializing
///
class FFMPEG_EXPORT FFmpegAudioParam
{
public:
    ///
    /// @brief  Constructor for initializing an object of FFmpegAudioParam
    ///
    /// @param  [in] sampleRate     The sample rate of the audio, must be greater than 0
    /// @param  [in] channels       The number of channels in the audio, must be greater than 0
    /// @param  [in] bitRate        The target bit rate of the target audio stream, must be greater than 0
    /// @param  [in] audioCodecId	The id of the audio codec which is going to be used in encoding/decoding
    ///
    FFmpegAudioParam(int sampleRate, int channels, int bitRate, int audioCodecId = 0, AVSampleFormat sampleFormat = AV_SAMPLE_FMT_S16);

    ///
    /// @brief  Constructor for initializing an empty FFmpegAudioParam object
    ///
    FFmpegAudioParam();

    ///
    /// @brief  Destructor
    ///
    virtual ~FFmpegAudioParam();

	bool Read(AVCodecContext* ctx);
	bool isDiff(AVCodecContext* ctx);
	bool isDiff(const FFmpegAudioParam& other);
    ///
    ///	@brief  Judge whether a FFmpegAudioParam object is empty
    ///	
    bool empty() const;

public:
    int sampleRate;             ///< The sample rate of the audio
    int channels;               ///< The number of audio channels
    int bitRate;                ///< The bit rate of the audio
    int codecId;			///< The name of the audio codec
	AVSampleFormat sampleFormat;
};

#endif//FFmpegAudioParam_H
