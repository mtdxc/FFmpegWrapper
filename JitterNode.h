#ifndef __RTP_JITTER_NODE_H___ 
#define __RTP_JITTER_NODE_H___ 

#include "rtp.h"


class Entry : public RTP_DataFrame
{
  public:
    Entry * next;
    Entry * prev;
    PTimeInterval tick;
	operator =(RTP_DataFrame& frame)
	{
		*(RTP_DataFrame*)this=frame;
	}
};

class RTP_JitterBufferAnalyser;
/****************************************
自己修改的抖动缓存类,主要用于服务器的音频缓存,提供的方法有
内部实现主要是条排序的链表,并根据读取和写入的速度平缓音频包的抖动
自适应的抖动缓存设置
线程安全的WriteData\ReadData接口
目前只直接一次ReadData读取数据
建议:服务器缓存和转发音频应该以最低延迟为依据,不宜再加入抖动缓冲来增加延迟
因此,建议修改简单的存储结构,并可支持1次写入多次读出,以节省内存使用
****************************************/
class JitterNode
{
public:

	JitterNode(unsigned minJitterDelay,unsigned maxJitterDelay);
	virtual ~JitterNode();

	void SetDelay(unsigned minJitterDelay, unsigned maxJitterDelay);

	virtual BOOL WriteData(
		RTP_DataFrame& frame
	);

	virtual BOOL ReadData(
	  DWORD timestamp,        /// Timestamp to read from buffer.
	  RTP_DataFrame & frame   /// Frame read from the RTP session
	);
	
	void UseImmediateReduction(BOOL state) { doJitterReductionImmediately = state; }

	/**Get current delay for jitter buffer.//当前Jitter Buffer的延迟
	   */
	DWORD GetJitterTime() const { return currentJitterTime; }

	/**Get total number received packets too late to go into jitter buffer.
	  */
	DWORD GetPacketsTooLate() const { return packetsTooLate; }

	/**Get total number received packets that overran the jitter buffer.
	  */
	DWORD GetBufferOverruns() const { return bufferOverruns; }

	/**Get maximum consecutive marker bits before buffer starts to ignore them.
	  */
	DWORD GetMaxConsecutiveMarkerBits() const { return maxConsecutiveMarkerBits; }

	/**Set maximum consecutive marker bits before buffer starts to ignore them.
	  */
	void SetMaxConsecutiveMarkerBits(DWORD max) { maxConsecutiveMarkerBits = max; }
protected:
	PINDEX        bufferSize;
	DWORD         minJitterTime;
	DWORD         maxJitterTime;
	DWORD         maxConsecutiveMarkerBits;

	unsigned currentDepth;		//oldestFrame链表的长度
	DWORD    currentJitterTime; //current delay for jitter buffer
	DWORD    packetsTooLate;
	unsigned bufferOverruns;
	unsigned consecutiveBufferOverruns;
	DWORD    consecutiveMarkerBits;
	PTimeInterval    consecutiveEarlyPacketStartTime;
	DWORD    lastWriteTimestamp;
	PTimeInterval lastWriteTick;
	DWORD    jitterCalc;
	DWORD    targetJitterTime;
	unsigned jitterCalcPacketCount;
	BOOL     doJitterReductionImmediately;
	BOOL     doneFreeTrash;

	Entry * oldestFrame;
	Entry * newestFrame;
	Entry * freeFrames;
	Entry * currentWriteFrame;

	PMutex bufferMutex;
	BOOL   preBuffering;
	BOOL   doneFirstWrite;

	RTP_JitterBufferAnalyser * analyser;
};

#endif