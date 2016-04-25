// Sound.cpp: implementation of the CSoundIn class.
// Download by http://www.codefans.net
/////////////////////////////////////////////////////////////////////////////////////////
/*
   
	    This program is Copyright  Developped by Yannick Sustrac
                   yannstrc@mail.dotcom.fr
		        http://www.mygale.org/~yannstrc
 

This program is free software; you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program;
if not, write to the Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/
//////////////////////////////////////////////////////////////////////////////////////////    

#include "stdafx.h"
#include "SoundIn.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

#pragma comment(lib, "winmm")

extern void LogOut(LPCTSTR fmt, ...);
#define real double

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
//    Glogal Thread procedure for the CSoundIn class
//    It cannot be included inside the Class
//   
// The LPARAM is the Class pointer (this) it can be the base class CSoundIn ptr or a derived new class
// The value of this parametre can change according the Topmost class of the process 
/////////////////////////////////////////////////////////////////////////////////////////////////////////////

DWORD CSoundIn::WaveInThreadProc(CMyThread * pThread)
{
	CSoundIn* pSound =(CSoundIn*) pThread->GetArg();    
	while (pThread->WaitEvent(pSound->m_WaveInEvent))
	{
		pSound->AddBuffer();      // Toggle as changed state here !Toggle point to the just received buffer
		pSound->ComputeSamples(pSound->InputBuffer);
	}
	return 0;
}


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CSoundIn::CSoundIn()
{
  m_NbMaxSamples = 2048;
  m_WaveInSampleRate = 11025;
  memset(InputBuffer,0,sizeof InputBuffer);
  m_nChannel = 1;
}

CSoundIn::~CSoundIn()
{
	CloseMic();
}

///////////////////////////////////////////////////////////////////
MMRESULT CSoundIn::OpenMic()
{
    MMRESULT result;

    result=waveInGetNumDevs(); 
	if (result == 0)
	{
        LogOut(_T("No Sound Device"));
		return result;
	}

   // test for Mic available   
   result=waveInGetDevCaps(0, &m_WaveInDevCaps, sizeof(WAVEINCAPS));
   
   if ( result!= MMSYSERR_NOERROR)
   {
       LogOut(_T("Cannot determine sound card capabilities !"));
   }

	// The Sound Devive is OK now we can create an Event  and start the Thread
	m_WaveInEvent = CreateEvent(NULL,FALSE,FALSE,_T("WaveInThreadEvent"));
	m_WaveInThread.Start((ThreadFunc)&WaveInThreadProc,this);   

	// init format 
	WaveInitFormat(m_nChannel,m_WaveInSampleRate /* khz */,16 /* bits */); 

	
	// Open Input 
	result = waveInOpen( &m_WaveIn,0, &m_WaveFormat,(DWORD)m_WaveInEvent ,NULL ,CALLBACK_EVENT); 
	if ( result!= MMSYSERR_NOERROR)
	{
        LogOut(_T("Cannot Open Sound Input Device!"));
	    return result;
	}
  // prepare header
   /*
    typedef struct {     LPSTR  lpData;     
					 DWORD  dwBufferLength; 
					DWORD  dwBytesRecorded;  
					DWORD  dwUser; 
					DWORD  dwFlags; 
					DWORD  dwLoops;
					struct wavehdr_tag * lpNext;  
					DWORD  reserved; 
	} WAVEHDR; 
	*/
	m_SizeRecord = m_NbMaxSamples;
    m_WaveHeader.lpData = (CHAR *)&InputBuffer[0];
    m_WaveHeader.dwBufferLength = m_SizeRecord*2;
	m_WaveHeader.dwFlags = 0;

    result = waveInPrepareHeader( m_WaveIn, &m_WaveHeader, sizeof(WAVEHDR) ); 
   if ( (result!= MMSYSERR_NOERROR) || ( m_WaveHeader.dwFlags != WHDR_PREPARED) )
   {
        LogOut(_T("Cannot Prepare Header !"));
	    return result;
   }

   result = waveInAddBuffer( m_WaveIn, &m_WaveHeader, sizeof(WAVEHDR) );
   if  (result!= MMSYSERR_NOERROR) 
   {
        LogOut(_T("Cannot Add Buffer !"));
	    return result;
   }

   // all is correct now we can start the process
   result = waveInStart( m_WaveIn );
   if  (result!= MMSYSERR_NOERROR) 
   {
        LogOut(_T("Cannot Start Wave In !"));
	    return result;
   }
   return result;
}


void SaveWav(BYTE* pSaveBuffer, DWORD dwDataLength, WAVEFORMATEX& m_WaveFormat)
{
	FILE* pFile = fopen("D:\\audio.wav","wb");
	if(pFile){
		DWORD m_WaveHeaderSize = 38;
		DWORD m_WaveFormatSize = 18;
		fwrite("RIFF",4,1,pFile);
		unsigned int Sec=(dwDataLength + m_WaveHeaderSize);
		fwrite(&Sec,sizeof(Sec),1,pFile);
		fwrite("WAVE",4,1,pFile);
		fwrite("fmt ",4,1,pFile);
		fwrite(&m_WaveFormatSize,sizeof(m_WaveFormatSize),1,pFile);
		fwrite(&m_WaveFormat,sizeof(m_WaveFormat),1,pFile);
		fwrite("data",4,1,pFile);
		fwrite(&dwDataLength,sizeof(dwDataLength),1,pFile);
		fwrite(pSaveBuffer,dwDataLength,1,pFile);
		fclose(pFile);
	}
}

void CSoundIn::AddBuffer()
{
    MMRESULT result;
    result = waveInUnprepareHeader( m_WaveIn, &m_WaveHeader, sizeof(WAVEHDR) ); 
    if  (result!= MMSYSERR_NOERROR) 
    {
        LogOut(_T("Cannot UnPrepareHeader !"));
        return;
    };
	if(m_cbAudio)
	{// 引起数据回调
		int nLen = m_NbMaxSamples*2;
		(*m_cbAudio)((BYTE*)InputBuffer, nLen, m_cbArgs);
		/* 已测试有PCM数据
		static BYTE pcm[200*1024];
		static int nPcm = 0;
		int remain = sizeof pcm - nPcm;
		if(remain>=0)
		{
			if(remain<nLen)
			{
				SaveWav(pcm, nPcm,m_WaveFormat);
			}
			else
			{
				memcpy(pcm+nPcm, InputBuffer, nLen);
			}
			nPcm+=nLen;
		}
		*/
	}
 	m_SizeRecord = m_NbMaxSamples;
    m_WaveHeader.lpData = (CHAR *)&InputBuffer[0];
    m_WaveHeader.dwBufferLength = m_SizeRecord *2;
	m_WaveHeader.dwFlags = 0;
    result = waveInPrepareHeader( m_WaveIn, &m_WaveHeader, sizeof(WAVEHDR) ); 
    if( (result!= MMSYSERR_NOERROR) || ( m_WaveHeader.dwFlags != WHDR_PREPARED) )
        LogOut(_T("Cannot Prepare Header !"));

    result = waveInAddBuffer( m_WaveIn, &m_WaveHeader, sizeof(WAVEHDR) );
    if(result!= MMSYSERR_NOERROR) 
        LogOut(_T("Cannot Add Buffer !"));

   result = waveInStart( m_WaveIn );
   if(result!= MMSYSERR_NOERROR) 
        LogOut(_T("Cannot Start Wave In !"));
}

/*
WAVE_FORMAT_1M08  11.025 kHz, mono, 8-bit 
WAVE_FORMAT_1M16  11.025 kHz, mono, 16-bit 
WAVE_FORMAT_1S08  11.025 kHz, stereo, 8-bit 
WAVE_FORMAT_1S16  11.025 kHz, stereo, 16-bit 
WAVE_FORMAT_2M08  22.05 kHz, mono, 8-bit 
WAVE_FORMAT_2M16  22.05 kHz, mono, 16-bit 
WAVE_FORMAT_2S08  22.05 kHz, stereo, 8-bit 
WAVE_FORMAT_2S16  22.05 kHz, stereo, 16-bit 
WAVE_FORMAT_4M08  44.1 kHz, mono, 8-bit 
WAVE_FORMAT_4M16  44.1 kHz, mono, 16-bit 
WAVE_FORMAT_4S08  44.1 kHz, stereo, 8-bit 
WAVE_FORMAT_4S16  44.1 kHz, stereo, 16-bit 
*/ 

void CSoundIn:: WaveInitFormat( WORD    nCh, // number of channels (mono, stereo)
								DWORD   nSampleRate, // sample rate
								WORD    BitsPerSample)
{
	m_WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
	m_WaveFormat.nChannels = nCh;
	m_WaveFormat.nSamplesPerSec = nSampleRate;
	m_WaveFormat.nAvgBytesPerSec = nSampleRate * nCh * BitsPerSample/8;
	m_WaveFormat.nBlockAlign = m_WaveFormat.nChannels * BitsPerSample/8;
	m_WaveFormat.wBitsPerSample = BitsPerSample;
	m_WaveFormat.cbSize = 0;
}   


///////////////////////////////////////////////////////////////////////////
// the comutation for the input samples need to be calibrated according
// to the sound board  add an Offset and a Mult coef.

void CSoundIn::ComputeSamples(SHORT *pt)
{
	int i;
	for ( i = 0 ; i<m_NbMaxSamples; i++) // scaling the input samples du to the sound card
	{
//		InputBuffer[i] += m_CalOffset;
//		InputBuffer[i] *=m_CalGain;
	}
}  


void CSoundIn::CloseMic()
{
	m_WaveInThread.Close(1000);
	if (m_WaveIn){
		waveInStop(m_WaveIn);
		waveInClose(m_WaveIn);
		m_WaveIn = NULL;
	}
}

void CSoundIn::RazBuffers()
{
	for (int i=0;i<MAX_SAMPLES;i++)
	{
	    InputBuffer[i] = 0;
	    InputBuffer[i] = 0;
	}
}

void CSoundIn::StopMic()
{
	waveInStop(m_WaveIn);
	waveInReset(m_WaveIn);
}

void CSoundIn::StartMic()
{
	waveInStart(m_WaveIn);
}

void CSoundIn::SetCallBack( AudioCallBack cb, LPVOID arg )
{
	m_cbAudio = cb;
	m_cbArgs = arg;
}

