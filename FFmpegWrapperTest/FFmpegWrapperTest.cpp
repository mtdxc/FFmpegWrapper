
#include <math.h>
extern "C"{
#include "libavformat/avformat.h"
//#include "../FFmpegWrapper/inttypes.h"
};
#include "../FFmpegWrapper/FFmpegEncoder.h"
#include "../FFmpegWrapper/FFmpegDecoder.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

float t, tincr, tincr2;

static void fillTestVieoFrame(AVFrame *frame, int frameIndex, int width, int height)
{
    int x, y, i;

    i = frameIndex;

    /* Y */
    for(y=0;y<height;y++) {
        for(x=0;x<width;x++) {
            frame->data[0][y * frame->linesize[0] + x] = x + y + i * 3;
        }
    }

    /* Cb and Cr */
    for(y=0;y<height/2;y++) {
        for(x=0;x<width/2;x++) {
            frame->data[1][y * frame->linesize[1] + x] = 128 + y + i * 2;
            frame->data[2][y * frame->linesize[2] + x] = 64 + x + i * 5;
        }
    }
}

static void getTestAudioData(int16_t *samples, int frameSize, int channels)
{
    int j, i, v;
    int16_t *q;

    q = samples;
    for(j=0;j<frameSize;j++) {
        v = (int)(sin(t) * 10000);
        for(i = 0; i < channels; i++)
            *q++ = v;
        t += tincr;
        tincr += tincr2;
    }
}

void testEncoder()
{
	int videoFramesize, frameIndex, audioFrameSize;
	uint8_t *videoData;
	AVFrame *tempFrame;
	short *audioData;
	double videoPts, audioPts;

	/* init signal generator */
	t = 0;
	tincr = 2 * M_PI * 110.0 / 44100;
	/* increment frequency by 110 Hz per second */
	tincr2 = 2 * M_PI * 110.0 / 44100 / 44100;

	FFmpegVideoParam videoParam(352, 288, AV_PIX_FMT_YUV420P, 400000, 25, AV_CODEC_ID_H264);//"libx264");
	FFmpegAudioParam audioParam(44100, 2, 64000, AV_CODEC_ID_AAC);//"libvo_aacenc");
	FFmpegEncoder testVideo(videoParam, audioParam);
	testVideo.open("test.flv");

	/* get test video data */
  tempFrame = av_frame_alloc();
	videoFramesize = testVideo.getVideoFrameSize();
	videoData = static_cast<uint8_t *>(av_malloc(videoFramesize));
	avpicture_fill((AVPicture *)tempFrame, videoData, AV_PIX_FMT_YUV420P, 352, 288);
	frameIndex = 0;

	/* get test audio data */
	audioFrameSize = testVideo.getAudioFrameSize();
	audioData = static_cast<short *>(av_malloc(audioFrameSize*audioParam.channels*2));

	videoPts = testVideo.getVideoTimeStamp();
	audioPts = testVideo.getAudioTimeStamp();
	int videoTsp = 0;
	/* output 5 seconds test video file */
	while (audioPts < 50) {
		if (audioPts <= videoPts) {
			getTestAudioData(audioData, audioFrameSize, audioParam.channels);
			testVideo.writeAudioFrame((uint8_t *)audioData, audioFrameSize);
			audioPts = testVideo.getAudioTimeStamp();
		} else {
			fillTestVieoFrame(tempFrame, frameIndex, 352, 288);
			testVideo.writeVideoFrame(tempFrame->data[0], videoTsp);
			frameIndex++;
			videoTsp += 1000/videoParam.frameRate;
			videoPts = testVideo.getVideoTimeStamp();
		}
	}

	testVideo.close();
	printf("write %d vedio frames\n", frameIndex);
	av_free(tempFrame->data[0]);
	av_free(tempFrame);
	av_free(audioData);
}

void testDecoder()
{
	int signal;

	FFmpegDecoder ffdecoder;
	ffdecoder.open("test.flv");

	FFmpegAudioParam audioParam = ffdecoder.getAudioParam();
	//audioParam.sampleRate = 44100;
	FFmpegVideoParam videoParam = ffdecoder.getVideoParam();
	FFmpegEncoder ffencoder(videoParam, audioParam);
	ffencoder.open("testDecoder.flv");

	while (true) {
		signal = ffdecoder.decodeFrame();

		if (signal == 0) {
			if(!ffdecoder.getVideoFrame())
				continue;
			ffencoder.writeVideoFrame(ffdecoder.getVideoFrame(), 
				ffencoder.ms2pts(false, ffdecoder.getDecodeTimeStamp()*1000) );
		} else if (signal == 1) {
			ffencoder.writeAudioFrame(ffdecoder.getAudioFrame(), ffdecoder.getAudioFrameSize());
		} else if (signal == -1) {
			break;
		}
	}

	ffencoder.close();
	ffdecoder.close();
}

#define TS_PATH "D:\\test\\1.ts"
void testMux()
{
	int signal;

	FFmpegDecoder ffdecoder;
	ffdecoder.open(TS_PATH);

	FFmpegAudioParam audioParam = ffdecoder.getAudioParam();
	//audioParam.sampleRate = 44100;
	FFmpegVideoParam videoParam = ffdecoder.getVideoParam();
	FFmpegEncoder ffencoder(videoParam, audioParam);
	ffencoder.open("testDecoder.flv",false);
	bool bOutFlv = true;
	while (true) {
		signal = ffdecoder.readFrame();
		AVPacket* pkg = ffdecoder.getCurPacket();
		if (signal == 0) {
			ffencoder.writeVideoData(pkg->data, pkg->size, 
				pkg->flags&AV_PKT_FLAG_KEY, ffdecoder.getPresentTimeStamp()*1000);
		} else if (signal == 1) {
			if(pkg->data[0]==0xFF&&bOutFlv) // FLV不能有ADTS头部，但TS要求有
				ffencoder.writeAudioData(pkg->data+7, pkg->size-7, ffdecoder.getPresentTimeStamp()*1000);
			else
				ffencoder.writeAudioData(pkg->data, pkg->size, ffdecoder.getPresentTimeStamp()*1000);
		} else if (signal == -1) {
			break;
		}
	}

	ffencoder.close();
	ffdecoder.close();
}

#include <Windows.h>
#include "TestM3U8.h"
int main(int argc, wchar_t* argv[])
{
	char nCmd = 0;
	if(argc>1)
		nCmd = argv[1][0];
	//else printf("enter cmd: "); nCmd = getchar();
	FFmpegDecoder::split2("d:\\264.flv", "d:\\264.1.flv", 10, 30);
	//FFmpegDecoder::split2("d:\\6.webm", "d:\\6.1.webm", 10, 30);

	CTestM3U8 tmu;
	//tmu.SetSegment(10,3,false);
	tmu.SetSegment(30, -1, false);
	switch(nCmd){
	case 'E':
	case 'e':
		testEncoder();
		break;
	case 'D':
	case 'd':
		testDecoder();
		break;
	case 'M':
	case 'm':
		testMux();
		break;
	default:
		{
			FFmpegVideoParam videoParam(800, 600, AV_PIX_FMT_RGB24, 1024*200, 15, AV_CODEC_ID_H264);
			FFmpegAudioParam audioParam(11025, 1, 64000, AV_CODEC_ID_AAC);
			tmu.Start(videoParam, audioParam, "D:\\test\\test.m3u8");//,"http://ming/hls");
		}
	}
	//
	getchar();
	return 0;
}

