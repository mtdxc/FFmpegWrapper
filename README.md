
下面介绍的API已过时，请下载最新版本的源代码，并参考其注释。新版本主要由John编写，在旧版本的基础上做了很多改进。
# 什么是FFmpeg？
FFmpeg是一套完整的录制、转换、流化音视频的解决方案，也是一个在LGPL协议下的开源项目。它包含了业界领先的音视频编解码库。FFmpeg是在Linux操作系统下开发的，但它也能在其他操作系统下编译，包括Windows。
整个项目由以下几个部分组成：
 - ffmpeg：一个用来转换视频文件格式的命令行工具，它也支持从电视卡中实时的抓取和编码视频。
 - ffserver：一个基于HTTP协议（基于RTSP的版本正在开发中）用于实时广播的多媒体服务器，它也支持实时广播的时间平移。
 - ffplay：一个用SDL和FFmpeg库开发的简单的媒体播放器。
 - libavcodec：一个包含了所有FFmpeg音视频编解码器的库。为了保证最优的性能和高可复用性,大多数编解码器都是从头开发的。
 - libavformat：一个包含了所有的普通音视频格式的解析器和产生器的库。
FFmpegWrapper仅使用了libavcodec和libavformat这两部分。
# 什么是FFmpegWrapper？
FFmpegWrapper：
	- 是一个在Windows下用VS2005编写的C++ Win32动态库。
	- 用面向对象的方法封装了FFmpeg库中常用的API，使之易于使用，不需要开发人员了解很多音视频编解码的知识。
	- 其中99%的代码符合C++标准，很容易移植到其他平台。
	- 由farthinker开发和维护。
 
# 为什么要使用FFmpegWrapper？
对于一个对视频编解码不太了解的开发者来说，用FFmpeg的库来编写应用绝对是一件痛苦的事情。首先需要编译从SVN下载的源代码（FFmpeg官方只提供源代码……）。如果是在Windows下编译，麻烦就开始了（当然你也可以直接使用别人编译好的SDK，跳过这一步）。当你好不容易编译好一个可以使用的动态库之后，你会发现很难找到合适的文档来学习如何使用FFmpeg的库，你只能一边参考示例代码一边摸索使用方法。然后你会发现问题一个接一个的出现，你又不知从何处下手来解决。总之，使用FFmpeg的学习成本是很高的。
FFmpegWrapper的目的就在于让FFmpeg的调用过程简单化、面向对象化，降低使用FFmpeg的学习成本，让对视频编解码不太了解的开发人员也能轻松的使用FFmpeg。但是，简化使用的同时也在一定程度上简化了功能，FFmpegWrapper很难继承FFmpeg库的所有功能。所以FFmpegWrapper适合一些编解码需求相对简单的应用，而不适合那些需求复杂灵活、扩展性很强的应用。
# 如何使用FFmpegWrapper来编解码音视频？
## 准备工作
首先下载FFmpegWrapper的库文件（若是在非Windows平台下使用，则需要下载源代码自己编译），然后将FFmpegWrapper部署到项目中。部署的过程中需要注意的是，最好不要改变ffmpeg文件夹相对于FFmpegWrapper.h的路径，若必须要改变，组需要修改FFmpegWrapper.h中#include “ffmpeg/include/avformat.h”的路径。调用动态库的具体方法这里就不赘述了。
## 使用FFmpegWrapper编码音视频
### 指定音视频参数
首先需要指定一些音视频的参数。FFmpegWrapper中用FFmpegVideoParam和FFmpegAudioParam这两个类来表示音视频的参数。下面的例子指定了一个flv视频的参数：
```
	//指定视频参数，从左到右分别是：宽、高、像素格式、比特率、帧率
	FFmpegVideoParam videoParam(352, 288, PIX_FMT_YUV420P, 400000, 25);
	//指定视频参数，从左到右分别是：比特率、采样率、声道数
	FFmpegAudioParam audioParam(64000, 44100, 2);
```  
若视频中没有视频流（音频流），则可以初始化一个空的FFmpegVideoParam（FFmpegAudioParam）：
```
	FFmpegVideoParam videoParam(352, 288, PIX_FMT_YUV420P, 400000, 25);
	//没有音频流，初始化一个空的音频参数对象
	FFmpegAudioParam audioParam();
```  
### 初始化FFmpegEncoder对象
用音视频参数初始化FFmpegEncoder对象：
```
	FFmpegVideoParam videoParam(352, 288, PIX_FMT_YUV420P, 400000, 25);
	FFmpegAudioParam audioParam(64000, 44100, 2);
	//参数从左到右分别是：FFmpegVideoParam、FFmpegAudioParam 、编码输出文件名
	FFmpegEncoder testEncoder(videoParam, audioParam, "test.flv");
```  
其中第三个参数包含了输出文件的路径、名字和后缀，并且是可选的参数，也就是说可以没有输出文件。但是在没有输出文件的时候需要音视频参数中指定codec的名称，因为FFmpegEncoder不能从文件后缀判断出使用什么codec：
```
  FFmpegVideoParam videoParam(352, 288, PIX_FMT_YUV420P, 400000, 25,"flv");
	FFmpegAudioParam audioParam(64000, 44100, 2, "libmp3lame");
	//参数从左到右分别是：FFmpegVideoParam、FFmpegAudioParam 、编码输出文件名
	FFmpegEncoder testEncoder(videoParam, audioParam);
```  
### 逐帧编码音视频
开始编码之前还需要先调用FFmpegEncoder对象的open方法，打开相应的codec和输出文件：
```
	FFmpegVideoParam videoParam(352, 288, PIX_FMT_YUV420P, 400000, 25);
	FFmpegAudioParam audioParam(64000, 44100, 2);
	FFmpegEncoder testEncoder(videoParam, audioParam,"test.flv");
	testEncoder.open();
```
然后就可以调用FFmpegEncoder对象的writeVideoFrame（writeAudioFrame）方法来逐帧的编码并输出视（音）频了：
```
	//其中videoFrameData是uint8_t *（unsigned char *）类型的参数
	testEncoder.writeVideoFrame(videoFrameData);
	//其中audioFrameData是short *类型的参数
	testEncoder.writeAudioFrame(audioFrameData);
```
如果编码之后不需要输出，即没有输出文件，则使用encodeVideoFrame和getVideoBuffer（encodeAudioFrame和getAudioBuffer）方法来编码和获得编码后的数据：
```
	//其中videoFrameData是uint8_t *（unsigned char *）类型的参数
	testEncoder.encodeVideoFrame(videoFrameData);
	uint8_t *encodedVideo = testEncoder.getVideoBuffer();
	//其中audioFrameData是short *类型的参数
	testEncoder.encodeAudioFrame(audioFrameData);
	uint8_t *encodedAudio = testEncoder.getAudioBuffer();
```  
编码的过程中，还可以获得音视频的时间戳（pts）来处理音视频同步（暂不适用于没有输出文件的情况），下面是一个例子：
```
	short *audioData;
	uint8_t *videoData;
	double videoPts, audioPts;
	videoPts = testEncoder.getVideoPts();
	audioPts = testEncoder.getAudioPts();
	while (audioPts< 5) {
	    if (audioPts< = videoPts) {
	         audioData = getTestAudioData();
	         testEncoder.writeAudioFrame(audioData);
	     } else {
	         videoData = getVideoFrame();
	         testEncoder.writeVideoFrame(videoData);
	     }
	     audioPts = testEncoder.getAudioPts();
	     videoPts = testEncoder.getVideoPts();
	}
```  
完成编码后还需要调用FFmpegEncoder对象的close方法，关闭codec和输出文件并释放资源：
```
	testEncoder.close();
```
## 使用FFmpegWrapper解码音视频
### 初始化FFmpegDecoder对象
首先初始化一个FFmpegDecoder对象，并传入输入文件的名称（包括路径、名字和后缀）：
```
	FFmpegDecoder testDecoder("test.flv");
```
### 逐帧解码音视频
开始解码之前还需要先调用FFmpegDecoder对象的open方法，打开相应的codec和输入文件：
```
	FFmpegDecoder testDecoder("test.flv");
	testDecoder.open();
```
然后就可以调用FFmpegDecoder对象的decodeFrame方法来逐帧的解码音视频文件了：
```
	//decodeFrame的返回值表示当前解码的帧的状态：
	//   0 - 视频帧
	//   1 - 音频帧
	// -1 - 文件末尾或解码出错
	int signal = testDecoder.decodeFrame()
```
解码之后可以通过FFmpegDecoder对象的getVideoFrame（getAudioFrame）方法来获得解码后的视（音）频数据，
下面是一个完整的例子：
```
	int signal;
	uint8_t *decodedVideo;
	short *decodedAudio;
	FFmpegDecoder testDecoder("test.flv");
	ffencoder.open();
	while (true) {
	     signal = testDecoder.decodeFrame();
	    if (signal == 0) {
	         decodedVideo = ffdecoder.getVideoFrame();
	        //处理解码之后的视频数据
	     } elseif (signal == 1) {
	         decodedAudio = ffdecoder.getAudioFrame();
	        //处理解码之后的音频数据
	     } else if (signal == -1) {
	        break;
	     }
	}
```
完成编码后还需要调用FFmpegDecoder对象的close方法，关闭codec和输入文件并释放资源：
```
	testDecoder.close();
```
注意
	- FFmpegWrapper暂时没有完整的转码功能，如有需要请使用FFmpeg提供的格式转换工具ffmpeg.exe。
	- 上面的介绍只涉及到一部分FFmpegWrapper的公共API，详细的API介绍和其他细节见FFmpegWrapper API参考（upcoming）。
	- farthinker只是一个web开发者，对音视频的了解实在有限，所以FFmpegWrapper肯定存在一些潜在的问题，欢迎大家积极批评指正。

# FFmpegWrapper的未来
就像上面提到的那样，我只是一个web开发者，不是音视频方面的专业人员。因为项目中有一些简单的编码需求，我才编写了FFmpegWrapper。我希望FFmpegWrapper能帮助像我这样的音视频的菜鸟，能让刚接触FFmpeg的朋友少走弯路。当然，我的能力和精力实在有限，今后如果没有更多的编解码需求，我可能很难抽出大把时间继续完善FFmepgWrapper。但我真心希望FFmpegWrapper能继续走下去，所以有心和我一起继续编写FFmpegWrapper朋友请和我联系。

来自 <http://blog.sina.com.cn/s/blog_75992b660101mlfr.html> 
