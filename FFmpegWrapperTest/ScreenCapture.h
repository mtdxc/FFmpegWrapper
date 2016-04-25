#pragma once
#include "mythread.h"

/*
数据回调函数
@param pRgb RGB数据
@param width 宽度
@param height 高度
@param arg 参数
*/
typedef void (*DataCallBack)(BYTE* pRgb, int width, int height, LPVOID arg);

/*
屏幕捕捉类
使用方式：
---
CScreenCapture sCap;
sCap.SetDataCallBack
sCap.Setup
sCap.Start
---
DataCallBack(rgb,widht, height);
---
sCap.Stop
*/
class CScreenCapture
{
public:
	CScreenCapture(void);
	~CScreenCapture(void);

	/*
	设置采集参数
	@param width 宽度
	@param height 高度
	@param nFps 帧率
	@param bltMode 拉伸模式 1 拉伸，0 BitBlt
	@return BOOL 成功则返回true
	*/
	bool Setup(int width, int height, int nFps, int bltMode);
	/*
	设置数据回调
	@param pCb 回调函数
	@param arg 参数
	*/
	void SetDataCallBack(DataCallBack pCb, LPVOID arg);
	/// 返回采集延迟
	int GetDelay(){return m_nDelay;}
	/// 开始捕捉
	bool Start();
	/// 停止捕捉
	bool Stop();

	/*
	抓图，并保存成BMP
	CScreenCapture video;
	video.Setup(width, height, nFps, 1);
	video.TakeSnap("D:\\abc.bmp");
	@param bmpPath bmp路径
	@return bool 成功返回true
	*/
	bool TakeSnap(LPCTSTR bmpPath);
	/// 获取目的宽度
	int GetWidth(){return m_bmpInfo.bmiHeader.biWidth;}
	/// 获取目的高度
	int GetHeight(){return m_bmpInfo.bmiHeader.biHeight;}
	/// 捕捉线程
	static DWORD CaptureThread(CMyThread* pThread);
protected:
	void RlsRGB();
	void OnCapture();
	BITMAPINFO m_bmpInfo;
	/// 内存Bitmap
	HBITMAP m_bmp;
	/// 旧的BMP对象
	HGDIOBJ m_hOldObj;
	/// 内存DC
	HDC m_dcMem;
	/// 屏幕Dc
	HDC m_wdc;
	/// 每帧延迟 1000/fps
	int m_nDelay;
	/// 拉伸模式
	int m_nBltMode;
	/// 采集线程
	CMyThread m_capThread;
	/// RGB缓冲区
	BYTE* m_pRGB;
	/// RGB缓冲区大小
	int m_nRGB;

	/// 数据回调函数
	DataCallBack m_dataCb;
	/// 回调参数
	LPVOID m_argCb;
};

