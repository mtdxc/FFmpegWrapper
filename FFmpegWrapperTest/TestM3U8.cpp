#include "stdafx.h"
extern "C"{
#include "libavformat/avformat.h"
//#include "../FFmpegWrapper/inttypes.h"
};
#include "../FFmpegWrapper/FFmpegEncoder.h"
#include "../FFmpegWrapper/FFmpegDecoder.h"
#include "TestM3U8.h"

CTestM3U8::CTestM3U8(void)
{
	m_ptsMux = NULL;
	m_dwFistTick = 0;
	m_nMaxSeg = -1;
	m_dwLastSeg = 0;
	m_hlsVer = 3;
	// 10s
	m_nSegTime = 10;
	m_nSegIdx = 0;
	m_bLive = false;
}


CTestM3U8::~CTestM3U8(void)
{
	Close();
}

void CTestM3U8::OnAudio( BYTE* pcm, int length )
{
	if(m_ptsMux){
		AutoLock al(m_lock);
		if(!m_dwFistTick)
			m_dwFistTick = GetTickCount();
		if(!m_ptsMux->hasVideo()){//没有视频就实时检查音频
			int nDelTick = GetTickCount()-m_dwFistTick;
			if(nDelTick-m_dwLastSeg>m_nSegTime*1000){
				AddSegMent(nDelTick);
			}
		}
		m_ptsMux->writeAudioFrame(pcm, length);
	}
}

void CTestM3U8::OnVideo( BYTE* pRgb, int length )
{
	if(m_ptsMux)
	{
		AutoLock al(m_lock);
		if(!m_dwFistTick) // 必须进行时间戳校准，否则FLV长度不对呀
			m_dwFistTick = GetTickCount();
		int nDelTick = GetTickCount()-m_dwFistTick;
		if(nDelTick-m_dwLastSeg>m_nSegTime*1000){
			AddSegMent(nDelTick);
			// 强制生成关键帧，如果读取文件得判断关键帧,才可生成M3U8文件[这边不用加也行，因为编码器已经重新开关，会产生一个关键帧的]
			// m_ptsMux->ReqKeyFrame();
		}
		printf("%d.", nFrame++);

		int nPkg = m_ptsMux->writeVideoFrame(pRgb, m_ptsMux->ms2pts(false,nDelTick));
		if(nPkg && m_ptsMux->getVideoEncodeType())
		{
			//* 解关键帧的测试
			FFmpegDecoder dec;
			FFmpegVideoParam vPar = m_ptsMux->getVideoParam();
			vPar.pixelFormat = AV_PIX_FMT_RGB24;
			if(dec.open(vPar,m_ptsMux->getAudioParam())){
				int ret=dec.decodeVideoFrame((uint8_t*)m_ptsMux->getVideoEncodedBuffer(), nPkg);
				if(ret>0)
					printf("decode keyFrame %d -> %d\n", nPkg, ret);
			}
			//*/
		}
	}
}

void CTestM3U8::AudioCallBack( BYTE* pcm, int length, LPVOID arg )
{
	CTestM3U8* pThis = (CTestM3U8*) arg;
	pThis->OnAudio(pcm, length);
}

void CTestM3U8::VideoCallBack( BYTE* pRgb, int width, int height, LPVOID arg )
{
	CTestM3U8* pThis = (CTestM3U8*) arg;
	pThis->OnVideo(pRgb, width*height*3);
}

bool CTestM3U8::Start(FFmpegVideoParam& videoParam, FFmpegAudioParam& audioParam, 
	LPCTSTR path, LPCTSTR preUrl, bool bSource)
{
	AutoLock al(m_lock);
	m_nSegIdx = 0;
	TCHAR szTmp[MAX_PATH]={0};
	strcpy(szTmp, path);
	LPSTR pSplit = strrchr(szTmp,'\\');
	if(pSplit)
	{
		*pSplit = 0;
		m_szFileDir = szTmp;
		strrchr(pSplit+1,'.')[0]=0;
		m_szFileName = pSplit+1;
		sprintf(pSplit, "\\%s-0.ts", m_szFileName.c_str());
	}
	if(preUrl&&preUrl[0])
		m_szUrlPrefix = preUrl;
	if(videoParam.codecId) videoParam.codecId = AV_CODEC_ID_H264;
	if(audioParam.codecId) audioParam.codecId = AV_CODEC_ID_AAC;
	if(bSource){
		videoParam.pixelFormat = AV_PIX_FMT_RGB24;
		audioParam.sampleFormat = AV_SAMPLE_FMT_S16;
	}
	m_ptsMux = new FFmpegEncoder(videoParam,audioParam);
	if(m_ptsMux){
		//m_dwFistTick = GetTickCount();
		m_ptsMux->open(szTmp);
		if(bSource)
		{
			nFrame = 0;
			// 设置每次采样数
			if(audioParam.codecId){
				audio.m_nChannel = audioParam.channels;
				audio.m_WaveInSampleRate = audioParam.sampleRate;
				audio.m_NbMaxSamples = m_ptsMux->getAudioFrameSize() / 2;
				audio.SetCallBack(&AudioCallBack,this);
				audio.OpenMic();
			}
			if(videoParam.codecId)
			{
				m_ptsMux->SetFlip(1,1);
				video.SetDataCallBack(&VideoCallBack,this);
				video.Setup(videoParam.width, videoParam.height, videoParam.frameRate, 1);
				// 测试BMP抓图功能
				//video.TakeSnap("D:\\abc.bmp");
				video.Start();
			}
		}
	}
	return true;
}

bool CTestM3U8::Close()
{
	audio.CloseMic();
	video.Stop();
	AutoLock al(m_lock);
	if(m_ptsMux)
	{
		delete m_ptsMux;
		m_ptsMux = NULL;
		if(m_dwFistTick)
			AddSegMent(GetTickCount()-m_dwFistTick);
	}
	return true;
}

bool CTestM3U8::AddSegMent( DWORD tick )
{
	TCHAR szTemp[MAX_PATH];
	if(m_nMaxSeg>0)
	while(m_vecSegs.size()>=m_nMaxSeg)
	{
		// 必须要删除文件
		SegItem& si = m_vecSegs[0];
		LPSTR pFile = strrchr(si.url,'/');
		if(pFile)
			sprintf_s(szTemp, "%s\\%s", m_szFileDir.c_str(), pFile+1);
		else
			sprintf_s(szTemp, "%s\\%s", m_szFileDir.c_str(), si.url);			
		DeleteFile(szTemp);
		printf("DelteFile %s\n", szTemp);
		m_vecSegs.erase(m_vecSegs.begin());
	}
	SegItem si;
	if(m_szUrlPrefix.length())
		sprintf_s(si.url, "%s/%s-%d.ts", m_szUrlPrefix.c_str(), m_szFileName.c_str(), m_nSegIdx++);
	else
		sprintf_s(si.url, "%s-%d.ts", m_szFileName.c_str(), m_nSegIdx++);
	si.duration = (tick-m_dwLastSeg)/1000.0;
	m_vecSegs.push_back(si);
	updateIndex();
	if(m_ptsMux&&m_ptsMux->isOpen())
	{
		m_ptsMux->close();
		// 新的TS文件名称
		sprintf_s(si.url, "%s\\%s-%d.ts", m_szFileDir.c_str(), m_szFileName.c_str(), m_nSegIdx);
		m_ptsMux->open(si.url);
		nFrame = 0;
	}
	m_dwLastSeg = tick;
	//m_dwLastSeg = 0;
	//m_dwFistTick = GetTickCount();
	return true;
}

bool CTestM3U8::updateIndex()
{
	TCHAR tmpPath[MAX_PATH],realPath[MAX_PATH];
	sprintf_s(tmpPath, "%s\\%s.tmp", m_szFileDir.c_str(), m_szFileName.c_str());
	sprintf_s(realPath, "%s\\%s.m3u8", m_szFileDir.c_str(), m_szFileName.c_str());

	FILE* index_fp = fopen(tmpPath, "w");
	if (!index_fp) {
		printf("Could not open temporary m3u8 index file (%s), no index file will be created\n", tmpPath);
		return false;
	}
	fprintf(index_fp, "#EXTM3U\n#EXT-X-TARGETDURATION:%lu\n#EXT-X-VERSION:%d\n", m_nSegTime, m_hlsVer);
	if (m_nMaxSeg>0) 
		fprintf(index_fp, "#EXT-X-MEDIA-SEQUENCE:%u\n", max(m_nSegIdx - m_nMaxSeg,0));
	for (int i = 0; i < m_vecSegs.size(); i++) 
	{
		if(m_hlsVer>=3)
		{
			fprintf(index_fp, "#EXTINF:%5.2f,\n%s\n", m_vecSegs[i].duration, m_vecSegs[i].url);
		}
		else
		{
			int dur = m_vecSegs[i].duration;
			fprintf(index_fp, "#EXTINF:%lu,\n%s\n", dur, m_vecSegs[i].url);
		}
	}

	if (!m_bLive) {
		fprintf(index_fp, "#EXT-X-ENDLIST\n");
	}

	fclose(index_fp);
	DeleteFile(realPath);
	// 这好像不能删除旧文件
	return MoveFile(tmpPath, realPath);
	//return rename(tmpPath, realPath);
}

bool CTestM3U8::SetSegment( int nSegTime, int nMaxSeg, bool bLive )
{
	m_nSegTime = nSegTime;
	m_nMaxSeg = nMaxSeg;
	m_bLive = bLive;
	return true;
}

/*
struct options_t {
	const char *input_file;
	// 每个分片最大长度
	long segment_duration;
	// URL前缀
	const char *url_prefix;
	// 文件名前缀
	const char *output_prefix;
	// 实际的M3U8文件
	const char *m3u8_file;
	// 临时M3U8文件
	char *tmp_m3u8_file;
	// 最大分块数
	long num_segments;
};
int write_index_file(const struct options_t options, 
	const unsigned int first_segment, const unsigned int last_segment, 
	const int end) 
{
	unsigned int i;

	FILE* index_fp = fopen(options.tmp_m3u8_file, "w");
	if (!index_fp) {
		fprintf(stderr, "Could not open temporary m3u8 index file (%s), no index file will be created\n", 
			options.tmp_m3u8_file);
		return -1;
	}

	if (options.num_segments) {
		fprintf(index_fp, "#EXTM3U\n#EXT-X-TARGETDURATION:%lu\n#EXT-X-MEDIA-SEQUENCE:%u\n", 
			options.segment_duration, first_segment);
	}
	else {
		fprintf(index_fp, "#EXTM3U\n#EXT-X-TARGETDURATION:%lu\n", 
			options.segment_duration);
	}

	for (i = first_segment; i <= last_segment; i++) {
		fprintf(index_fp, "#EXTINF:%lu,\n%s%s-%u.ts\n", 
			options.segment_duration, options.url_prefix, options.output_prefix, i);
	}

	if (end) {
		fprintf(index_fp, "#EXT-X-ENDLIST\n");
	}

	fclose(index_fp);
	return rename(options.tmp_m3u8_file, options.m3u8_file);
}
*/