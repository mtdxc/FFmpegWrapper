#include "JitterNode.h"

///////////////////////////////////////
//RTP_JitterBuffer::JitterNode
///////////////////////////////////////
JitterNode::JitterNode(unsigned minJitterDelay,
                             unsigned maxJitterDelay)
{

  // Jitter buffer is a queue of frames waiting for playback, a list of
  // free frames, and a couple of place holders for the frame that is
  // currently beeing read from the RTP transport or written to the codec.
  // 在这里Jitter 的Buffer 是一个双向的队列结构，用于缓存读到的数据。
  
  oldestFrame = newestFrame = currentWriteFrame = NULL;
  
  // Calculate the maximum amount of timestamp units for the jitter buffer
  minJitterTime = minJitterDelay;
  maxJitterTime = maxJitterDelay;
  currentJitterTime = minJitterDelay;
  targetJitterTime = currentJitterTime;

  // Calculate number of frames to allocate, we make the assumption that the
  // smallest packet we can possibly get is 5ms long (assuming audio 8kHz unit).
  bufferSize = maxJitterTime/5+1;	//maxJitterTime用来确定bufferSize

  // Nothing in the buffer so far
  currentDepth = 0;
  packetsTooLate = 0;
  bufferOverruns = 0;
  consecutiveBufferOverruns = 0;
  maxConsecutiveMarkerBits = 10;
  consecutiveMarkerBits = 0;
  consecutiveEarlyPacketStartTime = 0;
  doJitterReductionImmediately = FALSE;
  doneFreeTrash = FALSE;

  lastWriteTimestamp = 0;
  lastWriteTick = 0;
  jitterCalc = 0;
  jitterCalcPacketCount = 0;

  preBuffering = TRUE;
  doneFirstWrite = FALSE;

  // Allocate the frames and put them all into the free list
  freeFrames = new Entry;
  freeFrames->next = freeFrames->prev = NULL;

  for (PINDEX i = 0; i < bufferSize; i++) {
    Entry * frame = new Entry;
    frame->prev = NULL;
    frame->next = freeFrames;
    freeFrames->prev = frame;
    freeFrames = frame;
  }

  PTRACE(2, "RTP\tJitter buffer created:"
            " size=" << bufferSize <<
            " delay=" << minJitterTime << '-' << maxJitterTime << '/' << currentJitterTime <<
            " obj=" << this);

#if PTRACING && !defined(NO_ANALYSER)
  analyser = new RTP_JitterBufferAnalyser;
#else
  analyser = NULL;
#endif
}
	
JitterNode::~JitterNode()
{
  bufferMutex.Wait();

  // Free up all the memory allocated
  //两条链表和一个单元
  while (oldestFrame != NULL) {
    Entry * frame = oldestFrame;
    oldestFrame = oldestFrame->next;
    delete frame;
  }

  while (freeFrames != NULL) {
    Entry * frame = freeFrames;
    freeFrames = freeFrames->next;
    delete frame;
  }
  //单元
  delete currentWriteFrame;

  bufferMutex.Signal();

#if PTRACING && !defined(NO_ANALYSER)
  PTRACE(5, "Jitter buffer analysis: size=" << bufferSize
         << " time=" << currentJitterTime << '\n' << *analyser);
  delete analyser;
#endif
}
	  
BOOL JitterNode::ReadData(DWORD timestamp,RTP_DataFrame & frame)
{

  // 默认反馈一个空帧(如静音)
  frame.SetPayloadSize(0);

  PWaitAndSignal mutex(bufferMutex);

  //释放写入编解码器的帧(即:把它放回free list),
  //然后清除标志(parking spot)
  if (currentWriteFrame != NULL) 
  {
    // Move frame from current to free list
    currentWriteFrame->next = freeFrames;
    if (freeFrames != NULL)
      freeFrames->prev = currentWriteFrame;
    freeFrames = currentWriteFrame;

    currentWriteFrame = NULL;
  }

  /*
    取得下一个写入解码器的下一帧,从队列最旧的位置拿一个出来,如果是时候这样做的话
	and parks it in the special member,可以不加锁Mutex,因为其他的写入线程也有自己
	的应用(使用Buffer,加锁访问)
   */
  if (oldestFrame == NULL) 
  {
    /*No data to play! We ran the buffer down to empty, restart buffer by
      setting flag that will fill it again before returning any data.
     */
	//没有数据读
    preBuffering = TRUE;
    currentJitterTime = targetJitterTime;
    
	
#if PTRACING && !defined(NO_ANALYSER)
    analyser->Out(0, currentDepth, "Empty");
#endif
    return TRUE;
  }
 
  DWORD oldestTimestamp = oldestFrame->GetTimestamp();
  DWORD newestTimestamp = newestFrame->GetTimestamp();

  /*
  如果有机会(due to silence in the buffer)减少Jitter Buffer大小 
  Take it 
  */

  if (targetJitterTime < currentJitterTime &&
      (newestTimestamp - oldestTimestamp) < currentJitterTime) 
  {
	//减少currentJitterTime,使得它成为这两个的最大值
	//<<<<<<
    currentJitterTime = max( targetJitterTime > (newestTimestamp - oldestTimestamp));
	//<<<<<<
    PTRACE(3, "RTP\tJitter buffer size decreased to " << currentJitterTime);
  }

  /* 如果oldestFrame比需要的还旧,则使用它;否则确认写线程有落后(CPU跑得不够快)
     
	 看看是否已经可以读这个数据包:
	 1. 如果最旧的frame比我们要求的年龄(timestamp)还要老,则使用它
	 2. 如果还不到时候,确认写线程没有落后
	 3. 如果Jitter中的最旧和最新帧间的时间间隔,比最大缓存的还大,无论如何返回最
	 旧的帧,使Write线程赶上
  */

  if (preBuffering) //preBuffering是由读来进行赋值
  {
    // Reset jitter baseline基准线
	// (should be handled by GetMarker() condition, but just in case...)
    lastWriteTimestamp = 0;
    lastWriteTick = 0;

    // Check for requesting something that already exceeds the maximum time,
    // or have filled the jitter buffer, not filling if this is so

    // 如果最旧的帧还没在BUFF中呆够长时间,则不返回任何东西
    if ((PTimer::Tick() - oldestFrame->tick) < currentJitterTime / 2) //如果最旧的帧还没有被延迟足够时间
	{
#if PTRACING && !defined(NO_ANALYSER)
      analyser->Out(oldestTimestamp, currentDepth, "PreBuf");
#endif
      return TRUE;
    }

    preBuffering = FALSE;
  }


  //Handle short silence bursts(爆裂, 炸破) in the middle of the buffer
  // - if we think we're getting marker bit information, use that
  BOOL shortSilence = FALSE;
  if (consecutiveMarkerBits < maxConsecutiveMarkerBits) 
  {  
	  //就是说它是静音后的第一个元素,必须延迟一定的时间，好戏才开场
     if( oldestFrame->GetMarker() && (PTimer::Tick() - oldestFrame->tick) < currentJitterTime/2 )
        shortSilence = TRUE;
  }
  else if (timestamp<oldestTimestamp && timestamp>(newestTimestamp-currentJitterTime))
    shortSilence = TRUE; //该数据包已被读走
  /*
  ===============================================
  ^	    ^										^
  |tsp  |oldestFrame							|newestFrame
  */
  if (shortSilence) 
  {
    // It is not yet time for something in the buffer
#if PTRACING && !defined(NO_ANALYSER)
    analyser->Out(oldestTimestamp, currentDepth, "Wait");
#endif
    lastWriteTimestamp = 0;
    lastWriteTick = 0;
    return TRUE;
  }

  // Detatch oldest packet from the list, put into parking space
  currentDepth--;
#if PTRACING && !defined(NO_ANALYSER)
  analyser->Out(oldestTimestamp,currentDepth,timestamp>=oldestTimestamp?"":"Late");
#endif
  currentWriteFrame = oldestFrame;
  oldestFrame = currentWriteFrame->next;
  currentWriteFrame->next = NULL;
 
  // Calculate the jitter contribution(贡献) of this frame

  // 对静音后的第一个包不进行时延抖动计算
  if (currentWriteFrame->GetMarker())
  { 
    lastWriteTimestamp = 0;
    lastWriteTick = 0;
  }

  if (lastWriteTimestamp != 0 && lastWriteTick !=0) 
  { 
	//非GetMarker的处理
    int thisJitter = 0;

    if (currentWriteFrame->GetTimestamp() < lastWriteTimestamp) 
	  {
      //Not too sure how to handle this situation...
      thisJitter = 0;
    }
    else if (currentWriteFrame->tick < lastWriteTick) 
	  {
      //Not too sure how to handle this situation either!
      thisJitter = 0;
    }
    else 
	  { //thisJitter应该是(网络传输的)时延抖动
      thisJitter = (currentWriteFrame->tick - lastWriteTick) +
                   lastWriteTimestamp - currentWriteFrame->GetTimestamp();
    }

    if (thisJitter < 0)//绝对值 
		thisJitter *=(-1);
    thisJitter *=2; //currentJitterTime needs to be at least TWICE the maximum jitter

    if (thisJitter > (int) currentJitterTime * LOWER_JITTER_MAX_PCNT / 100) 
	{ 
	//时延抖动比较大(停止说话后,不过停止后说话应该是GetMark啊,要不就是丢包)
	//<<<<<<
      targetJitterTime = currentJitterTime;//进行重新赋值抖动大小
      PTRACE(3, "RTP\tJitter buffer target realigned to current jitter buffer");
      consecutiveEarlyPacketStartTime = PTimer::Tick();//启动一个时间计数
      jitterCalcPacketCount = 0;			//计数值为0
      jitterCalc = 0;
	//<<<<<<
    }
    else 
	{ //如果时延抖动比当前小
      if (thisJitter > (int) jitterCalc)
        jitterCalc = thisJitter; //取大的 jitterCalc=MAX(jitterCalc,thisJitter)

      jitterCalcPacketCount++;   //抖动计数的个数

      //如果这个比我们尝试设置的targetJitterTime还大 ,就调整它
	  //由于前面条件,这不可能使得targetJitterTime大于currentJitterTime
      if (thisJitter > (int) targetJitterTime * LOWER_JITTER_MAX_PCNT / 100) 
	  { //targetJitterTime变大(为下一次做好准备)
	  //<<<<<<
        targetJitterTime = thisJitter * 100 / LOWER_JITTER_MAX_PCNT;
        PTRACE(3, "RTP\tJitter buffer target size increased to " << targetJitterTime);
	  //<<<<<<
      }

    }
  }

  lastWriteTimestamp = currentWriteFrame->GetTimestamp();
  lastWriteTick = currentWriteFrame->tick;


  if (oldestFrame == NULL)
    newestFrame = NULL;
  else 
  {
    oldestFrame->prev = NULL;

    // If exceeded current jitter buffer time delay: 
    if ((newestTimestamp - currentWriteFrame->GetTimestamp()) > currentJitterTime) 
	{
      PTRACE(4, "RTP\tJitter buffer length exceeded");
      consecutiveEarlyPacketStartTime = PTimer::Tick();
      jitterCalcPacketCount = 0;
      jitterCalc = 0;
      lastWriteTimestamp = 0;
      lastWriteTick = 0;
      
      // If we haven't yet written a frame, we get one free overrun
      if (!doneFirstWrite) //如果我们还没有写入任何一帧
	  { 
        PTRACE(4, "RTP\tJitter buffer length exceed was prior to first write. Not increasing buffer size");
        while ((newestTimestamp - currentWriteFrame->GetTimestamp()) > currentJitterTime) 
		{//wastedFrame浪费帧(应该除去的帧) 
          Entry * wastedFrame = currentWriteFrame;
          currentWriteFrame = oldestFrame;
          oldestFrame = oldestFrame->next;
          currentDepth--;

          currentWriteFrame->next = NULL; 
		  //current WriteFrame should never be able to be NULL
          
          wastedFrame->next = freeFrames;
          if (freeFrames != NULL)
            freeFrames->prev = wastedFrame;
          freeFrames = wastedFrame;

          if (oldestFrame == NULL) 
		  {
            newestFrame = NULL;
            break;
          }

          oldestFrame->prev = NULL;
        }
        
        doneFirstWrite = TRUE;
        frame = *currentWriteFrame;
        return TRUE;
      }


      // See if exceeded maximum jitter buffer time delay, waste them if so
	  // 超过(就是缓存过多)
      while ((newestFrame->GetTimestamp() - currentWriteFrame->GetTimestamp()) > maxJitterTime) 
	  {
        PTRACE(4, "RTP\tJitter buffer oldest packet ("
               << oldestFrame->GetTimestamp() << " < "
               << (newestTimestamp - maxJitterTime)
               << ") too late, throwing away");
		//<<<<<<
          currentJitterTime = maxJitterTime;
		//<<<<<<
          //Throw away the oldest frame and move everything up
          Entry * wastedFrame = currentWriteFrame;	//本来就持有一个
          currentWriteFrame = oldestFrame;			//再移动到下一个
          oldestFrame = oldestFrame->next;			//oldestFrame再减少一个
          currentDepth--;

          currentWriteFrame->next = NULL; 
		  //currentWriteFrame should never be able to be NULL
          
          wastedFrame->next = freeFrames;
          if (freeFrames != NULL)
            freeFrames->prev = wastedFrame;
          freeFrames = wastedFrame;

          if (oldestFrame == NULL) 
		  {
            newestFrame = NULL;
            break;
          }

      }

	// Now change the jitter time to cope with the new size
    // unless already set to maxJitterTime
	//<<<<<<
      if (newestTimestamp - currentWriteFrame->GetTimestamp() > currentJitterTime) 
          currentJitterTime = newestTimestamp - currentWriteFrame->GetTimestamp();

      targetJitterTime = currentJitterTime;
	//<<<<<<
      PTRACE(3, "RTP\tJitter buffer size increased to " << currentJitterTime);
    }
  }

  //<<<<<<
  if ((PTimer::Tick() - consecutiveEarlyPacketStartTime) > DECREASE_JITTER_PERIOD &&
       jitterCalcPacketCount >= DECREASE_JITTER_MIN_PACKETS)
  {//定时更新targetJitterTime
    jitterCalc = jitterCalc * 100 / LOWER_JITTER_MAX_PCNT;
    if (jitterCalc < targetJitterTime / 2) 
		jitterCalc = targetJitterTime / 2;
    if (jitterCalc < minJitterTime) 
		jitterCalc = minJitterTime;
    targetJitterTime = jitterCalc;//<<<<<<
    PTRACE(3, "RTP\tJitter buffer target size decreased to " << targetJitterTime);
    jitterCalc = 0;
    jitterCalcPacketCount = 0;
    consecutiveEarlyPacketStartTime = PTimer::Tick();
  }
  //<<<<<<

  /* If using immediate jitter reduction (rather than waiting for silence opportunities)
  then trash oldest frames as necessary to reduce the size of the jitter buffer */
  if (targetJitterTime < currentJitterTime &&
      doJitterReductionImmediately && newestFrame != NULL) 
  {
    while ((newestFrame->GetTimestamp() - currentWriteFrame->GetTimestamp()) > targetJitterTime)
	{
      // Throw away the newest entries
      Entry * wastedFrame = newestFrame;
      newestFrame = newestFrame->prev;
      if (newestFrame != NULL)
          newestFrame->next = NULL;
      wastedFrame->prev = NULL;

      // Put thrown away frame on free list
      wastedFrame->next = freeFrames;
      if (freeFrames != NULL)
        freeFrames->prev = wastedFrame;
      freeFrames = wastedFrame;

      // Reset jitter calculation baseline
      lastWriteTimestamp = 0;
      lastWriteTick = 0;

      currentDepth--;
      if (newestFrame == NULL) 
      {
          oldestFrame = NULL;
          break;
      }
    }

    currentJitterTime = targetJitterTime;//<<<<<<
    PTRACE(3, "RTP\tJitter buffer size decreased to " << currentJitterTime );

  }

  doneFirstWrite = TRUE;
  frame = *currentWriteFrame;
  return true;
}

void JitterNode::SetDelay(unsigned int minJitterDelay, unsigned int maxJitterDelay)
{
  bufferMutex.Wait();

  minJitterTime = minJitterDelay;
  maxJitterTime = maxJitterDelay;
  currentJitterTime = minJitterDelay;
  targetJitterTime = currentJitterTime;

  PINDEX newBufferSize = maxJitterTime/5+1;
  while (bufferSize < newBufferSize) 
  {
    Entry * frame = new Entry;
    frame->prev = NULL;
    frame->next = freeFrames;
    freeFrames->prev = frame;
    freeFrames = frame;
    bufferSize++;
  }

  packetsTooLate = 0;
  bufferOverruns = 0;
  consecutiveBufferOverruns = 0;
  consecutiveMarkerBits = 0;
  consecutiveEarlyPacketStartTime = 0;

  preBuffering = TRUE;

  PTRACE(2, "RTP\tJitter buffer restarted:"
              " size=" << bufferSize <<
              " delay=" << minJitterTime << '-' << maxJitterTime << '/' << currentJitterTime);

  bufferMutex.Signal();
}

BOOL JitterNode::WriteData(RTP_DataFrame& frame)
{
	PWaitAndSignal mutex(bufferMutex);
    BOOL markerWarning = FALSE;

//<<==找currentReadFrame的过程，先到freeFrames找，然后到oldestFrame找

    // Get the next free frame available for use for reading from the RTP
    // transport. Place it into a parking spot.
    Entry * currentReadFrame;
    if (freeFrames != NULL) 
	{
      // Take the next free frame and make it the current for reading
      currentReadFrame = freeFrames;
      freeFrames = freeFrames->next;
      if (freeFrames != NULL)
        freeFrames->prev = NULL;//双向非循环链表

      PTRACE_IF(2, consecutiveBufferOverruns > 1,
                "RTP\tJitter buffer full, threw away "
                << consecutiveBufferOverruns << " oldest frames");
      consecutiveBufferOverruns = 0;
    }
    else 
	{
      // We have a full jitter buffer, need a new frame so take the oldest one
      currentReadFrame = oldestFrame;
      oldestFrame = oldestFrame->next;
      if (oldestFrame != NULL)
        oldestFrame->prev = NULL;

      currentDepth--;	//currentDepth是指oldestFrame list的大小
      bufferOverruns++; //缓存Overrun
      consecutiveBufferOverruns++;
	//<<==
      if (consecutiveBufferOverruns > MAX_BUFFER_OVERRUNS) 
	  {
        PTRACE(2, "RTP\tJitter buffer continuously(连续地)full, throwing away entire buffer.");
        freeFrames = oldestFrame;
        oldestFrame = newestFrame = NULL;
        preBuffering = TRUE;//进入预缓冲
      }
      else 
	  {
        PTRACE_IF(2, consecutiveBufferOverruns == 1,
                  "RTP\tJitter buffer full, throwing away oldest frame ("
                  << currentReadFrame->GetTimestamp() << ')');
      }
	//<<==
    }
//<<==END 找currentReadFrame
	//找到currentReadFrame
	*currentReadFrame=frame;
    currentReadFrame->next = NULL;
	currentReadFrame->tick = PTimer::Tick();
	
	//连续性标志
    if (consecutiveMarkerBits < maxConsecutiveMarkerBits) 
	{
      if (currentReadFrame->GetMarker()) 
	  {
        PTRACE(3, "RTP\tReceived start of talk burst: " << currentReadFrame->GetTimestamp());
        //preBuffering = TRUE;
        consecutiveMarkerBits++;
      }
      else
        consecutiveMarkerBits = 0;
    }
    else 
	{ //consecutiveMarkerBits>=maxConsecutiveMarkerBits
      if (currentReadFrame->GetMarker())
        currentReadFrame->SetMarker(FALSE);
      if (!markerWarning && (consecutiveMarkerBits == maxConsecutiveMarkerBits)) 
	  {
        markerWarning = TRUE;
        PTRACE(3, "RTP\tEvery packet has Marker bit, ignoring them from this client!");
      }
    }
    
    
#if PTRACING && !defined(NO_ANALYSER)
	//currentDepth当前oldest Frame的长度 =>=区分预缓冲
    analyser->In(currentReadFrame->GetTimestamp(), currentDepth, preBuffering ? "PreBuf" : "");
#endif

    // Queue the frame for playing by the thread at other end of jitter buffe

    // Have been reading a frame, put it into the queue now, at correct position
    if (newestFrame == NULL)
      oldestFrame = newestFrame = currentReadFrame; // Was empty
    else 
	{
      DWORD time = currentReadFrame->GetTimestamp();
		/*											
		============================================
		^											^
		|oldestFrame								|newestFrame
		*/
      if (time > newestFrame->GetTimestamp()) {
        // Is newer than newst, put at that end of queue
        currentReadFrame->prev = newestFrame;
        newestFrame->next = currentReadFrame;
        newestFrame = currentReadFrame;
      }
      else if (time <= oldestFrame->GetTimestamp()) {
        // Is older than the oldest, put at that end of queue
        currentReadFrame->next = oldestFrame;
        oldestFrame->prev = currentReadFrame;
        oldestFrame = currentReadFrame;
      }
      else {
        // Somewhere in between, locate its position
        Entry * frame = newestFrame->prev;
        while (time < frame->GetTimestamp())
          frame = frame->prev;

        currentReadFrame->prev = frame;
        currentReadFrame->next = frame->next;
        frame->next->prev = currentReadFrame;
        frame->next = currentReadFrame;
      }
    }

  currentDepth++;
  return true;
}
