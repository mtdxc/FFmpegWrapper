///
/// @file
///
/// @brief  Head file for the video parameters for encoder/decoder
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
#ifndef FFmpegVideoParam_H
#define FFmpegVideoParam_H

#ifdef FFWRAPPER_EXPORTS
#define FFMPEG_EXPORT _declspec(dllexport)
#else
#define FFMPEG_EXPORT _declspec(dllimport)
#endif

#ifndef AVUTIL_PIXFMT_H
enum AVPixelFormat {
	AV_PIX_FMT_NONE = -1,
	AV_PIX_FMT_YUV420P,   ///< planar YUV 4:2:0, 12bpp, (1 Cr & Cb sample per 2x2 Y samples)
	AV_PIX_FMT_YUYV422,   ///< packed YUV 4:2:2, 16bpp, Y0 Cb Y1 Cr
	AV_PIX_FMT_RGB24,     ///< packed RGB 8:8:8, 24bpp, RGBRGB...
	AV_PIX_FMT_BGR24,     ///< packed RGB 8:8:8, 24bpp, BGRBGR...
	AV_PIX_FMT_YUV422P,   ///< planar YUV 4:2:2, 16bpp, (1 Cr & Cb sample per 2x1 Y samples)
	AV_PIX_FMT_YUV444P,   ///< planar YUV 4:4:4, 24bpp, (1 Cr & Cb sample per 1x1 Y samples)
	AV_PIX_FMT_YUV410P,   ///< planar YUV 4:1:0,  9bpp, (1 Cr & Cb sample per 4x4 Y samples)
	AV_PIX_FMT_YUV411P,   ///< planar YUV 4:1:1, 12bpp, (1 Cr & Cb sample per 4x1 Y samples)
	AV_PIX_FMT_GRAY8,     ///<        Y        ,  8bpp
	AV_PIX_FMT_MONOWHITE, ///<        Y        ,  1bpp, 0 is white, 1 is black, in each byte pixels are ordered from the msb to the lsb
	AV_PIX_FMT_MONOBLACK, ///<        Y        ,  1bpp, 0 is black, 1 is white, in each byte pixels are ordered from the msb to the lsb
	AV_PIX_FMT_PAL8,      ///< 8 bit with PIX_FMT_RGB32 palette
	AV_PIX_FMT_YUVJ420P,  ///< planar YUV 4:2:0, 12bpp, full scale (JPEG), deprecated in favor of PIX_FMT_YUV420P and setting color_range
	AV_PIX_FMT_YUVJ422P,  ///< planar YUV 4:2:2, 16bpp, full scale (JPEG), deprecated in favor of PIX_FMT_YUV422P and setting color_range
	AV_PIX_FMT_YUVJ444P,  ///< planar YUV 4:4:4, 24bpp, full scale (JPEG), deprecated in favor of PIX_FMT_YUV444P and setting color_range
	AV_PIX_FMT_XVMC_MPEG2_MC,///< XVideo Motion Acceleration via common packet passing
	AV_PIX_FMT_XVMC_MPEG2_IDCT,
	AV_PIX_FMT_UYVY422,   ///< packed YUV 4:2:2, 16bpp, Cb Y0 Cr Y1
	AV_PIX_FMT_UYYVYY411, ///< packed YUV 4:1:1, 12bpp, Cb Y0 Y1 Cr Y2 Y3
	AV_PIX_FMT_BGR8,      ///< packed RGB 3:3:2,  8bpp, (msb)2B 3G 3R(lsb)
	AV_PIX_FMT_BGR4,      ///< packed RGB 1:2:1 bitstream,  4bpp, (msb)1B 2G 1R(lsb), a byte contains two pixels, the first pixel in the byte is the one composed by the 4 msb bits
	AV_PIX_FMT_BGR4_BYTE, ///< packed RGB 1:2:1,  8bpp, (msb)1B 2G 1R(lsb)
	AV_PIX_FMT_RGB8,      ///< packed RGB 3:3:2,  8bpp, (msb)2R 3G 3B(lsb)
	AV_PIX_FMT_RGB4,      ///< packed RGB 1:2:1 bitstream,  4bpp, (msb)1R 2G 1B(lsb), a byte contains two pixels, the first pixel in the byte is the one composed by the 4 msb bits
	AV_PIX_FMT_RGB4_BYTE, ///< packed RGB 1:2:1,  8bpp, (msb)1R 2G 1B(lsb)
	AV_PIX_FMT_NV12,      ///< planar YUV 4:2:0, 12bpp, 1 plane for Y and 1 plane for the UV components, which are interleaved (first byte U and the following byte V)
	AV_PIX_FMT_NV21,      ///< as above, but U and V bytes are swapped

	AV_PIX_FMT_ARGB,      ///< packed ARGB 8:8:8:8, 32bpp, ARGBARGB...
	AV_PIX_FMT_RGBA,      ///< packed RGBA 8:8:8:8, 32bpp, RGBARGBA...
	AV_PIX_FMT_ABGR,      ///< packed ABGR 8:8:8:8, 32bpp, ABGRABGR...
	AV_PIX_FMT_BGRA,      ///< packed BGRA 8:8:8:8, 32bpp, BGRABGRA...
};
#endif

struct AVCodecContext;
///
/// @brief  The video parameter class for FFmpegEncoder initializing
///
class FFMPEG_EXPORT FFmpegVideoParam
{
public:
    ///
    /// @brief  Constructor for initializing an object of FFmpegVideoParam
    ///
    ///	@param  [in] width          The width of the video frame, must be greater than 0
    ///	@param  [in] height         The height of the video frame, must be greater than 0
    ///	@param  [in] pixelFormat    PixelFormat enum representing the pixel format of the source video frame
    ///	@param  [in] bitRate        The target bit rate of the target video stream, must be greater than 0
    ///	@param  [in] frameRate      The frame rate of the target video, must be greater than 0
    ///	@param  [in] videoCodecName The name of the video codec which is going to be used in encoding/decoding
    ///
    FFmpegVideoParam(int width, int height, int pixelFormat, int bitRate, int frameRate, int videoCodecId = 0);

    ///
    /// @brief  Constructor for initializing an empty FFmpegVideoParam object
    ///
    FFmpegVideoParam();

    ///
    /// @brief  Destructor
    ///
    virtual ~FFmpegVideoParam();

    ///
    ///	@brief  Judge whether a FFmpegVideoParam object is empty
    ///	
    bool empty() const;

	bool Read(AVCodecContext* ctx);
	bool isDiff(AVCodecContext* ctx);
public:
    int width;                  ///< The width of the video
    int height;                 ///< The height of the video
    int pixelFormat;    ///< The pixel format of the video
    int bitRate;                ///< The bit rate of the video
    int frameRate;              ///< The frame rate of the video
    int codecId; ///< The name of the video codec
};

#endif//FFmpegVideoParam_H
