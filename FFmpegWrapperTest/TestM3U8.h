#pragma once
#include "ScreenCapture.h"
#include "Soundin.h"
#include "../FFmpegWrapper/AutoLock.h"
#include <vector>
/// 切片项
struct SegItem
{
	/// URL路径
	TCHAR url[MAX_PATH];
	/// 持续时间
	float duration;
	/// 相对于上一个切换参数是否变化
	bool bChange;
};

/// M3U8文件切割类
class CTestM3U8
{
public:
	CTestM3U8(void);
	~CTestM3U8(void);

	/// 音频采集设备
	CSoundIn audio;
	/// 屏幕捕捉设备
	CScreenCapture video;
	/// 当前TS文件
	FFmpegEncoder* m_ptsMux;

	/// 整个M3U8文件第一帧Tick
	DWORD m_dwFistTick;
	/// 上一切片时间
	DWORD m_dwLastSeg;
	/// hls版本号
	int m_hlsVer;
	/// 切片持续时间
	int m_nSegTime;
	/// 最大切片数[用于生成长度受限的M3U8文件]
	int m_nMaxSeg;
	/// 直播模，FALSE则带结尾标记
	bool m_bLive;
	/// URL前缀
	std::string m_szUrlPrefix;
	/// 当前片索引，从0增长
	int m_nSegIdx;
	/// 文件名，不包含路径
	std::string m_szFileName;
	/// 本地存储目录
	std::string m_szFileDir;
	/// 切片列表
	std::vector<SegItem> m_vecSegs;
	/// 逻辑锁
	Mutex m_lock;
	/// 添加切片，并更新索引
	bool AddSegMent(DWORD tick);
	/// 更新M3U8索引文件
	bool updateIndex();
	int nFrame;
public:
	static void AudioCallBack(BYTE* pcm, int length, LPVOID arg);
	static void VideoCallBack(BYTE* pRgb, int width, int height, LPVOID arg);
	/*
	设置切片属性
	@param nSegTime 每个切片大小
	@param nMaxSeg 切片个数
	@param bLive 是否实时模式
	@return bool 返回true
	*/
	bool SetSegment(int nSegTime, int nMaxSeg, bool bLive);
	/*
	开启
	@param vParam 视频参数
	@param aParam 音频参数
	@param path 本地存放M3U8路径
	@param preUrl 网络发布URL,可为空
	@param bSource 是否开启采集
	@return bool 成功则返回true
	*/
	bool Start(FFmpegVideoParam& vParam, FFmpegAudioParam& aParam, 
		LPCTSTR path, LPCTSTR preUrl=NULL, bool bSource=true);
	/// 关闭采集和M3U8文件
	bool Close();

	/*
	编码并写入音频
	@param pcm PCM数据
	@param length 长度
	*/
	void OnAudio( BYTE* pcm, int length );
	/*
	编码并写入视频
	@param pRgb RGB数据 
	@param length 长度
	*/
	void OnVideo( BYTE* pRgb, int length );
};

