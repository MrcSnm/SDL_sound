/*
 * This source code is public domain.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>,
 *          Adam Goode       <adam@evdebs.org> (endian and char fixes for PPC)
*/

#include <math.h> //for GCCFIX
#include "libmodplug.h"

extern BOOL MMCMP_Unpack(LPCBYTE *ppMemFile, LPDWORD pdwMemLength);

// External decompressors
extern void AMSUnpack(const char *psrc, UINT inputlen, char *pdest, UINT dmax, char packcharacter);
extern WORD MDLReadBits(DWORD &bitbuf, UINT &bitnum, LPBYTE &ibuf, CHAR n);
extern int DMFUnpack(LPBYTE psample, LPBYTE ibuf, LPBYTE ibufmax, UINT maxlen);
extern DWORD ITReadBits(DWORD &bitbuf, UINT &bitnum, LPBYTE &ibuf, CHAR n);
extern void ITUnpack8Bit(signed char *pSample, DWORD dwLen, LPBYTE lpMemFile, DWORD dwMemLength, BOOL b215);
extern void ITUnpack16Bit(signed char *pSample, DWORD dwLen, LPBYTE lpMemFile, DWORD dwMemLength, BOOL b215);


#define MAX_PACK_TABLES		3


// Compression table
static const signed char UnpackTable[MAX_PACK_TABLES][16] =
//--------------------------------------------
{
	// CPU-generated dynamic table
	{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	// u-Law table
	{0, 1, 2, 4, 8, 16, 32, 64,
	-1, -2, -4, -8, -16, -32, -48, -64},
	// Linear table
	{0, 1, 2, 3, 5, 7, 12, 19,
	-1, -2, -3, -5, -7, -12, -19, -31}
};


//////////////////////////////////////////////////////////
// CSoundFile

CSoundFile::CSoundFile()
//----------------------
{
	m_nType = MOD_TYPE_NONE;
	m_dwSongFlags = 0;
	m_nChannels = 0;
	m_nMixChannels = 0;
	m_nSamples = 0;
	m_nInstruments = 0;
	m_nPatternNames = 0;
	m_lpszPatternNames = NULL;
	m_lpszSongComments = NULL;
	m_nFreqFactor = m_nTempoFactor = 128;
	m_nMasterVolume = 128;
	m_nMinPeriod = 0x20;
	m_nMaxPeriod = 0x7FFF;
	m_nRepeatCount = 0;
	SDL_memset(Chn, 0, sizeof(Chn));
	SDL_memset(ChnMix, 0, sizeof(ChnMix));
	SDL_memset(Ins, 0, sizeof(Ins));
	SDL_memset(ChnSettings, 0, sizeof(ChnSettings));
	SDL_memset(Headers, 0, sizeof(Headers));
	SDL_memset(Order, 0xFF, sizeof(Order));
	SDL_memset(Patterns, 0, sizeof(Patterns));
	SDL_memset(m_szNames, 0, sizeof(m_szNames));
	SDL_memset(m_MixPlugins, 0, sizeof(m_MixPlugins));
}


CSoundFile::~CSoundFile()
//-----------------------
{
	Destroy();
}


BOOL CSoundFile::Create(LPCBYTE lpStream, DWORD dwMemLength)
//----------------------------------------------------------
{
	int i;

	m_nType = MOD_TYPE_NONE;
	m_dwSongFlags = 0;
	m_nChannels = 0;
	m_nMixChannels = 0;
	m_nSamples = 0;
	m_nInstruments = 0;
	m_nFreqFactor = m_nTempoFactor = 128;
	m_nMasterVolume = 128;
	m_nDefaultGlobalVolume = 256;
	m_nGlobalVolume = 256;
	m_nOldGlbVolSlide = 0;
	m_nDefaultSpeed = 6;
	m_nDefaultTempo = 125;
	m_nPatternDelay = 0;
	m_nFrameDelay = 0;
	m_nNextRow = 0;
	m_nRow = 0;
	m_nPattern = 0;
	m_nCurrentPattern = 0;
	m_nNextPattern = 0;
	m_nRestartPos = 0;
	m_nMinPeriod = 16;
	m_nMaxPeriod = 32767;
	m_nSongPreAmp = 0x30;
	m_nPatternNames = 0;
	m_nMaxOrderPosition = 0;
	m_lpszPatternNames = NULL;
	m_lpszSongComments = NULL;
	SDL_memset(Ins, 0, sizeof(Ins));
	SDL_memset(ChnMix, 0, sizeof(ChnMix));
	SDL_memset(Chn, 0, sizeof(Chn));
	SDL_memset(Headers, 0, sizeof(Headers));
	SDL_memset(Order, 0xFF, sizeof(Order));
	SDL_memset(Patterns, 0, sizeof(Patterns));
	SDL_memset(m_szNames, 0, sizeof(m_szNames));
	SDL_memset(m_MixPlugins, 0, sizeof(m_MixPlugins));
	ResetMidiCfg();
	for (UINT npt=0; npt<MAX_PATTERNS; npt++) PatternSize[npt] = 64;
	for (UINT nch=0; nch<MAX_BASECHANNELS; nch++)
	{
		ChnSettings[nch].nPan = 128;
		ChnSettings[nch].nVolume = 64;
		ChnSettings[nch].dwFlags = 0;
		ChnSettings[nch].szName[0] = 0;
	}
	if (lpStream)
	{
		BOOL bMMCmp = MMCMP_Unpack(&lpStream, &dwMemLength);
		if ((!ReadXM(lpStream, dwMemLength))
		 && (!ReadS3M(lpStream, dwMemLength))
		 && (!ReadIT(lpStream, dwMemLength))
#ifndef MODPLUG_BASIC_SUPPORT
/* Sequencer File Format Support */
		 && (!ReadABC(lpStream, dwMemLength))
		 && (!ReadMID(lpStream, dwMemLength))
		 && (!ReadPAT(lpStream, dwMemLength))
		 && (!ReadSTM(lpStream, dwMemLength))
		 && (!ReadMed(lpStream, dwMemLength))
		 && (!ReadMTM(lpStream, dwMemLength))
		 && (!ReadMDL(lpStream, dwMemLength))
		 && (!ReadDBM(lpStream, dwMemLength))
		 && (!Read669(lpStream, dwMemLength))
		 && (!ReadFAR(lpStream, dwMemLength))
		 && (!ReadAMS(lpStream, dwMemLength))
		 && (!ReadOKT(lpStream, dwMemLength))
		 && (!ReadPTM(lpStream, dwMemLength))
		 && (!ReadUlt(lpStream, dwMemLength))
		 && (!ReadDMF(lpStream, dwMemLength))
		 && (!ReadDSM(lpStream, dwMemLength))
		 && (!ReadUMX(lpStream, dwMemLength))
		 && (!ReadAMF(lpStream, dwMemLength))
		 && (!ReadPSM(lpStream, dwMemLength))
		 && (!ReadMT2(lpStream, dwMemLength))
#endif // MODPLUG_BASIC_SUPPORT
		 && (!ReadMod(lpStream, dwMemLength))) m_nType = MOD_TYPE_NONE;
		if (bMMCmp)
		{
			GlobalFreePtr(lpStream);
			lpStream = NULL;
		}
	}
	// Adjust song names
	for (i=0; i<MAX_SAMPLES; i++)
	{
		LPSTR p = m_szNames[i];
		int j = 31;
		p[j] = 0;
		while ((j>=0) && (p[j]<=' ')) p[j--] = 0;
		while (j>=0)
		{
			if (((BYTE)p[j]) < ' ') p[j] = ' ';
			j--;
		}
	}
	// Adjust channels
	for (i=0; i<MAX_BASECHANNELS; i++)
	{
		if (ChnSettings[i].nVolume > 64) ChnSettings[i].nVolume = 64;
		if (ChnSettings[i].nPan > 256) ChnSettings[i].nPan = 128;
		Chn[i].nPan = ChnSettings[i].nPan;
		Chn[i].nGlobalVol = ChnSettings[i].nVolume;
		Chn[i].dwFlags = ChnSettings[i].dwFlags;
		Chn[i].nVolume = 256;
		Chn[i].nCutOff = 0x7F;
	}
	// Checking instruments
	MODINSTRUMENT *pins = Ins;

	for (i=0; i<MAX_INSTRUMENTS; i++, pins++)
	{
		if (pins->pSample)
		{
			if (pins->nLoopEnd > pins->nLength) pins->nLoopEnd = pins->nLength;
			if (pins->nLoopStart + 3 >= pins->nLoopEnd)
			{
				pins->nLoopStart = 0;
				pins->nLoopEnd = 0;
			}
			if (pins->nSustainEnd > pins->nLength) pins->nSustainEnd = pins->nLength;
			if (pins->nSustainStart + 3 >= pins->nSustainEnd)
			{
				pins->nSustainStart = 0;
				pins->nSustainEnd = 0;
			}
		} else
		{
			pins->nLength = 0;
			pins->nLoopStart = 0;
			pins->nLoopEnd = 0;
			pins->nSustainStart = 0;
			pins->nSustainEnd = 0;
		}
		if (!pins->nLoopEnd) pins->uFlags &= ~CHN_LOOP;
		if (!pins->nSustainEnd) pins->uFlags &= ~CHN_SUSTAINLOOP;
		if (pins->nGlobalVol > 64) pins->nGlobalVol = 64;
	}
	// Check invalid instruments
	while ((m_nInstruments > 0) && (!Headers[m_nInstruments])) 
		m_nInstruments--;
	// Set default values
	if (m_nSongPreAmp < 0x20) m_nSongPreAmp = 0x20;
	if (m_nDefaultTempo < 32) m_nDefaultTempo = 125;
	if (!m_nDefaultSpeed) m_nDefaultSpeed = 6;
	m_nMusicSpeed = m_nDefaultSpeed;
	m_nMusicTempo = m_nDefaultTempo;
	m_nGlobalVolume = m_nDefaultGlobalVolume;
	m_nNextPattern = 0;
	m_nCurrentPattern = 0;
	m_nPattern = 0;
	m_nBufferCount = 0;
	m_nTickCount = m_nMusicSpeed;
	m_nNextRow = 0;
	m_nRow = 0;
	if ((m_nRestartPos >= MAX_ORDERS) || (Order[m_nRestartPos] >= MAX_PATTERNS)) m_nRestartPos = 0;
	if (m_nType)
	{
		UINT maxpreamp = 0x10+(m_nChannels*8);
		if (maxpreamp > 100) maxpreamp = 100;
		if (m_nSongPreAmp > maxpreamp) m_nSongPreAmp = maxpreamp;
		return TRUE;
	}
	return FALSE;
}


BOOL CSoundFile::Destroy()

//------------------------
{
	int i;
	for (i=0; i<MAX_PATTERNS; i++) if (Patterns[i])
	{
		FreePattern(Patterns[i]);
		Patterns[i] = NULL;
	}
	m_nPatternNames = 0;
	if (m_lpszPatternNames)
	{
		delete [] m_lpszPatternNames;
		m_lpszPatternNames = NULL;
	}
	if (m_lpszSongComments)
	{
		delete [] m_lpszSongComments;
		m_lpszSongComments = NULL;
	}
	for (i=1; i<MAX_SAMPLES; i++)
	{
		MODINSTRUMENT *pins = &Ins[i];
		if (pins->pSample)
		{
			FreeSample(pins->pSample);
			pins->pSample = NULL;
		}
	}
	for (i=0; i<MAX_INSTRUMENTS; i++)
	{
		if (Headers[i])
		{
			delete Headers[i];
			Headers[i] = NULL;
		}
	}
	for (i=0; i<MAX_MIXPLUGINS; i++)
	{
		if ((m_MixPlugins[i].nPluginDataSize) && (m_MixPlugins[i].pPluginData))
		{
			m_MixPlugins[i].nPluginDataSize = 0;
			delete [] (signed char*)m_MixPlugins[i].pPluginData;
			m_MixPlugins[i].pPluginData = NULL;
		}
		m_MixPlugins[i].pMixState = NULL;
		if (m_MixPlugins[i].pMixPlugin)
		{
			m_MixPlugins[i].pMixPlugin->Release();
			m_MixPlugins[i].pMixPlugin = NULL;
		}
	}
	m_nType = MOD_TYPE_NONE;
	m_nChannels = m_nSamples = m_nInstruments = 0;
	return TRUE;
}


//////////////////////////////////////////////////////////////////////////
// Memory Allocation

MODCOMMAND *CSoundFile::AllocatePattern(UINT rows, UINT nchns)
//------------------------------------------------------------
{
	MODCOMMAND *p = new MODCOMMAND[rows*nchns];
	if (p) SDL_memset(p, 0, rows*nchns*sizeof(MODCOMMAND));
	return p;
}


void CSoundFile::FreePattern(LPVOID pat)
//--------------------------------------
{
	if (pat) delete [] (signed char*)pat;
}


signed char* CSoundFile::AllocateSample(UINT nbytes)
//-------------------------------------------
{
	signed char * p = (signed char *)GlobalAllocPtr(GHND, (nbytes+39) & ~7);
	if (p) p += 16;
	return p;
}


void CSoundFile::FreeSample(LPVOID p)
//-----------------------------------
{
	if (p)
	{
		GlobalFreePtr(((LPSTR)p)-16);
	}
}


//////////////////////////////////////////////////////////////////////////
// Misc functions

void CSoundFile::ResetMidiCfg()
//-----------------------------
{
	SDL_memset(&m_MidiCfg, 0, sizeof(m_MidiCfg));
	SDL_strlcpy(&m_MidiCfg.szMidiGlb[MIDIOUT_START*32], "FF", 32);
	SDL_strlcpy(&m_MidiCfg.szMidiGlb[MIDIOUT_STOP*32], "FC", 32);
	SDL_strlcpy(&m_MidiCfg.szMidiGlb[MIDIOUT_NOTEON*32], "9c n v", 32);
	SDL_strlcpy(&m_MidiCfg.szMidiGlb[MIDIOUT_NOTEOFF*32], "9c n 0", 32);
	SDL_strlcpy(&m_MidiCfg.szMidiGlb[MIDIOUT_PROGRAM*32], "Cc p", 32);
	SDL_strlcpy(&m_MidiCfg.szMidiSFXExt[0], "F0F000z", 32);
	for (int iz=0; iz<16; iz++) SDL_snprintf(&m_MidiCfg.szMidiZXXExt[iz*32], 32, "F0F001%02X", iz*8);
}




BOOL CSoundFile::SetWaveConfig(UINT nRate,UINT nBits,UINT nChannels,BOOL bMMX)
//----------------------------------------------------------------------------
{
	BOOL bReset = FALSE;
	DWORD d = gdwSoundSetup & ~SNDMIX_ENABLEMMX;
	if (bMMX) d |= SNDMIX_ENABLEMMX;
	if ((gdwMixingFreq != nRate) || (gnBitsPerSample != nBits) || (gnChannels != nChannels) || (d != gdwSoundSetup)) bReset = TRUE;
	gnChannels = nChannels;
	gdwSoundSetup = d;
	gdwMixingFreq = nRate;
	gnBitsPerSample = nBits;
	InitPlayer(bReset);
	return TRUE;
}

BOOL CSoundFile::SetMixConfig(UINT nStereoSeparation, UINT nMaxMixChannels)
//-------------------------------------------------------------------------
{
	if (nMaxMixChannels < 2) return FALSE;

	m_nMaxMixChannels = nMaxMixChannels;
	m_nStereoSeparation = nStereoSeparation;
	return TRUE;
}


BOOL CSoundFile::SetResamplingMode(UINT nMode)
//--------------------------------------------
{
	DWORD d = gdwSoundSetup & ~(SNDMIX_NORESAMPLING|SNDMIX_HQRESAMPLER|SNDMIX_ULTRAHQSRCMODE);
	switch(nMode)
	{
	case SRCMODE_NEAREST:	d |= SNDMIX_NORESAMPLING; break;
	case SRCMODE_LINEAR:	break;
	case SRCMODE_SPLINE:	d |= SNDMIX_HQRESAMPLER; break;
	case SRCMODE_POLYPHASE:	d |= (SNDMIX_HQRESAMPLER|SNDMIX_ULTRAHQSRCMODE); break;
	default:
		return FALSE;
	}
	gdwSoundSetup = d;
	return TRUE;
}

UINT CSoundFile::GetMaxPosition() const
//-------------------------------------
{
	UINT max = 0;
	UINT i = 0;

	while ((i < MAX_ORDERS) && (Order[i] != 0xFF))
	{
		if (Order[i] < MAX_PATTERNS) max += PatternSize[Order[i]];
		i++;
	}
	return max;
}

void CSoundFile::SetCurrentPos(UINT nPos)
//---------------------------------------
{
	UINT i, nPattern;

	for (i=0; i<MAX_CHANNELS; i++)
	{
		Chn[i].nNote = Chn[i].nNewNote = Chn[i].nNewIns = 0;
		Chn[i].pInstrument = NULL;
		Chn[i].pHeader = NULL;
		Chn[i].nPortamentoDest = 0;
		Chn[i].nCommand = 0;
		Chn[i].nPatternLoopCount = 0;
		Chn[i].nPatternLoop = 0;
		Chn[i].nFadeOutVol = 0;
		Chn[i].dwFlags |= CHN_KEYOFF|CHN_NOTEFADE;
		Chn[i].nTremorCount = 0;
	}
	if (!nPos)
	{
		for (i=0; i<MAX_CHANNELS; i++)
		{
			Chn[i].nPeriod = 0;
			Chn[i].nPos = Chn[i].nLength = 0;
			Chn[i].nLoopStart = 0;
			Chn[i].nLoopEnd = 0;
			Chn[i].nROfs = Chn[i].nLOfs = 0;
			Chn[i].pSample = NULL;
			Chn[i].pInstrument = NULL;
			Chn[i].pHeader = NULL;
			Chn[i].nCutOff = 0x7F;
			Chn[i].nResonance = 0;
			Chn[i].nLeftVol = Chn[i].nRightVol = 0;
			Chn[i].nNewLeftVol = Chn[i].nNewRightVol = 0;
			Chn[i].nLeftRamp = Chn[i].nRightRamp = 0;
			Chn[i].nVolume = 256;
			if (i < MAX_BASECHANNELS)
			{
				Chn[i].dwFlags = ChnSettings[i].dwFlags;
				Chn[i].nPan = ChnSettings[i].nPan;
				Chn[i].nGlobalVol = ChnSettings[i].nVolume;
			} else
			{
				Chn[i].dwFlags = 0;
				Chn[i].nPan = 128;
				Chn[i].nGlobalVol = 64;
			}
		}
		m_nGlobalVolume = m_nDefaultGlobalVolume;
		m_nMusicSpeed = m_nDefaultSpeed;
		m_nMusicTempo = m_nDefaultTempo;
	}
	m_dwSongFlags &= ~(SONG_PATTERNLOOP|SONG_CPUVERYHIGH|SONG_FADINGSONG|SONG_ENDREACHED|SONG_GLOBALFADE);
	for (nPattern = 0; nPattern < MAX_ORDERS; nPattern++)
	{
		UINT ord = Order[nPattern];
		if (ord == 0xFE) continue;
		if (ord == 0xFF) break;
		if (ord < MAX_PATTERNS)
		{
			if (nPos < (UINT)PatternSize[ord]) break;
			nPos -= PatternSize[ord];
		}
	}
	// Buggy position ?
	if ((nPattern >= MAX_ORDERS)
	 || (Order[nPattern] >= MAX_PATTERNS)
	 || (nPos >= PatternSize[Order[nPattern]]))
	{
		nPos = 0;
		nPattern = 0;
	}
	UINT nRow = nPos;
	if ((nRow) && (Order[nPattern] < MAX_PATTERNS))
	{
		MODCOMMAND *p = Patterns[Order[nPattern]];
		if ((p) && (nRow < PatternSize[Order[nPattern]]))
		{
			BOOL bOk = FALSE;
			while ((!bOk) && (nRow > 0))
			{
				UINT n = nRow * m_nChannels;
				for (UINT k=0; k<m_nChannels; k++, n++)
				{
					if (p[n].note)
					{
						bOk = TRUE;
						break;
					}
				}
				if (!bOk) nRow--;
			}
		}
	}
	m_nNextPattern = nPattern;
	m_nNextRow = nRow;
	m_nTickCount = m_nMusicSpeed;
	m_nBufferCount = 0;
	m_nPatternDelay = 0;
	m_nFrameDelay = 0;
}


// Flags:
//	0 = signed 8-bit PCM data (default)
//	1 = unsigned 8-bit PCM data
//	2 = 8-bit ADPCM data with linear table
//	3 = 4-bit ADPCM data
//	4 = 16-bit ADPCM data with linear table
//	5 = signed 16-bit PCM data
//	6 = unsigned 16-bit PCM data


UINT CSoundFile::ReadSample(MODINSTRUMENT *pIns, UINT nFlags, LPCSTR lpMemFile, DWORD dwMemLength)
//------------------------------------------------------------------------------
{
	UINT len = 0, mem = pIns->nLength+6;

	// Disable >2Gb samples,(preventing buffer overflow in AllocateSample)
	if ((!pIns) || ((int)pIns->nLength < 4) || (!lpMemFile)) return 0;
	if (pIns->nLength > MAX_SAMPLE_LENGTH) pIns->nLength = MAX_SAMPLE_LENGTH;
	pIns->uFlags &= ~(CHN_16BIT|CHN_STEREO);
	if (nFlags & RSF_16BIT)
	{
		mem *= 2;
		pIns->uFlags |= CHN_16BIT;
	}
	if (nFlags & RSF_STEREO)
	{
		mem *= 2;
		pIns->uFlags |= CHN_STEREO;
	}
	if ((pIns->pSample = AllocateSample(mem)) == NULL)
	{
		pIns->nLength = 0;
		return 0;
	}
	switch(nFlags)
	{
	// 1: 8-bit unsigned PCM data
	case RS_PCM8U:
		{
			len = pIns->nLength;
			if (len > dwMemLength) len = pIns->nLength = dwMemLength;
			signed char *pSample = pIns->pSample;
			for (UINT j=0; j<len; j++) pSample[j] = (signed char)(lpMemFile[j] - 0x80);
		}
		break;

	// 2: 8-bit ADPCM data with linear table
	case RS_PCM8D:
		{
			len = pIns->nLength;
			if (len > dwMemLength) break;
			signed char *pSample = pIns->pSample;
			const signed char *p = (const signed char *)lpMemFile;
			int delta = 0;

			for (UINT j=0; j<len; j++)
			{
				delta += p[j];
				*pSample++ = (signed char)delta;
			}
		}
		break;

	// 3: 4-bit ADPCM data
	case RS_ADPCM4:
		{
			len = (pIns->nLength + 1) / 2;
			if (len > dwMemLength - 16) break;
			SDL_memcpy(CompressionTable, lpMemFile, 16);
			lpMemFile += 16;
			signed char *pSample = pIns->pSample;
			signed char delta = 0;
			for (UINT j=0; j<len; j++)
			{
				BYTE b0 = (BYTE)lpMemFile[j];
				BYTE b1 = (BYTE)(lpMemFile[j] >> 4);
				delta = (signed char)GetDeltaValue((int)delta, b0);
				pSample[0] = delta;
				delta = (signed char)GetDeltaValue((int)delta, b1);
				pSample[1] = delta;
				pSample += 2;
			}
			len += 16;
		}
		break;

	// 4: 16-bit ADPCM data with linear table
	case RS_PCM16D:
		{
			len = pIns->nLength * 2;
			if (len > dwMemLength) break;
			int16_t *pSample = (int16_t *)pIns->pSample;
			int16_t *p = (int16_t *)lpMemFile;
			int delta16 = 0;
			for (UINT j=0; j<len; j+=2)
			{
				delta16 += bswapLE16(*p++);
				*pSample++ = (int16_t )delta16;
			}
		}
		break;

	// 5: 16-bit signed PCM data
	case RS_PCM16S:
	        {
		len = pIns->nLength * 2;
		if (len <= dwMemLength) SDL_memcpy(pIns->pSample, lpMemFile, len);
			int16_t *pSample = (int16_t *)pIns->pSample;
			for (UINT j=0; j<len; j+=2)
			{
				int16_t rawSample = *pSample;
			        *pSample++ = bswapLE16(rawSample);
			}
		}
		break;

	// 16-bit signed mono PCM motorola byte order
	case RS_PCM16M:
		len = pIns->nLength * 2;
		if (len > dwMemLength) len = dwMemLength & ~1;
		if (len > 1)
		{
			signed char *pSample = (signed char *)pIns->pSample;
			signed char *pSrc = (signed char *)lpMemFile;
			for (UINT j=0; j<len; j+=2)
			{
			  	// pSample[j] = pSrc[j+1];
				// pSample[j+1] = pSrc[j];
			        *((uint16_t *)(pSample+j)) = bswapBE16(*((uint16_t *)(pSrc+j)));
			}
		}
		break;

	// 6: 16-bit unsigned PCM data
	case RS_PCM16U:
		{
			len = pIns->nLength * 2;
			if (len > dwMemLength) break;
			int16_t *pSample = (int16_t *)pIns->pSample;
			int16_t *pSrc = (int16_t *)lpMemFile;
			for (UINT j=0; j<len; j+=2) *pSample++ = bswapLE16(*(pSrc++)) - 0x8000;
		}
		break;

	// 16-bit signed stereo big endian
	case RS_STPCM16M:
		len = pIns->nLength * 2;
		if (len*2 <= dwMemLength)
		{
			signed char *pSample = (signed char *)pIns->pSample;
			signed char *pSrc = (signed char *)lpMemFile;
			for (UINT j=0; j<len; j+=2)
			{
			        // pSample[j*2] = pSrc[j+1];
				// pSample[j*2+1] = pSrc[j];
				// pSample[j*2+2] = pSrc[j+1+len];
				// pSample[j*2+3] = pSrc[j+len];
			        *((uint16_t *)(pSample+j*2)) = bswapBE16(*((uint16_t *)(pSrc+j)));
				*((uint16_t *)(pSample+j*2+2)) = bswapBE16(*((uint16_t *)(pSrc+j+len)));
			}
			len *= 2;
		}
		break;

	// 8-bit stereo samples
	case RS_STPCM8S:
	case RS_STPCM8U:
	case RS_STPCM8D:
		{
			int iadd_l = 0, iadd_r = 0;
			if (nFlags == RS_STPCM8U) { iadd_l = iadd_r = -128; }
			len = pIns->nLength;
			signed char *psrc = (signed char *)lpMemFile;
			signed char *pSample = (signed char *)pIns->pSample;
			if (len*2 > dwMemLength) break;
			for (UINT j=0; j<len; j++)
			{
				pSample[j*2] = (signed char)(psrc[0] + iadd_l);
				pSample[j*2+1] = (signed char)(psrc[len] + iadd_r);
				psrc++;
				if (nFlags == RS_STPCM8D)
				{
					iadd_l = pSample[j*2];
					iadd_r = pSample[j*2+1];
				}
			}
			len *= 2;
		}
		break;

	// 16-bit stereo samples
	case RS_STPCM16S:
	case RS_STPCM16U:
	case RS_STPCM16D:
		{
			int iadd_l = 0, iadd_r = 0;
			if (nFlags == RS_STPCM16U) { iadd_l = iadd_r = -0x8000; }
			len = pIns->nLength;
			int16_t *psrc = (int16_t *)lpMemFile;
			int16_t *pSample = (int16_t *)pIns->pSample;
			if (len*4 > dwMemLength) break;
			for (UINT j=0; j<len; j++)
			{
				pSample[j*2] = (int16_t) (bswapLE16(psrc[0]) + iadd_l);
				pSample[j*2+1] = (int16_t) (bswapLE16(psrc[len]) + iadd_r);
				psrc++;
				if (nFlags == RS_STPCM16D)
				{
					iadd_l = pSample[j*2];
					iadd_r = pSample[j*2+1];
				}
			}
			len *= 4;
		}
		break;

	// IT 2.14 compressed samples
	case RS_IT2148:
	case RS_IT21416:
	case RS_IT2158:
	case RS_IT21516:
		len = dwMemLength;
		if (len < 4) break;
		if ((nFlags == RS_IT2148) || (nFlags == RS_IT2158))
			ITUnpack8Bit(pIns->pSample, pIns->nLength, (LPBYTE)lpMemFile, dwMemLength, (nFlags == RS_IT2158));
		else
			ITUnpack16Bit(pIns->pSample, pIns->nLength, (LPBYTE)lpMemFile, dwMemLength, (nFlags == RS_IT21516));
		break;

#ifndef MODPLUG_BASIC_SUPPORT
	// 8-bit interleaved stereo samples
	case RS_STIPCM8S:
	case RS_STIPCM8U:
		{
			int iadd = 0;
			if (nFlags == RS_STIPCM8U) { iadd = -0x80; }
			len = pIns->nLength;
			if (len*2 > dwMemLength) len = dwMemLength >> 1;
			LPBYTE psrc = (LPBYTE)lpMemFile;
			LPBYTE pSample = (LPBYTE)pIns->pSample;
			for (UINT j=0; j<len; j++)
			{
				pSample[j*2] = (signed char)(psrc[0] + iadd);
				pSample[j*2+1] = (signed char)(psrc[1] + iadd);
				psrc+=2;
			}
			len *= 2;
		}
		break;

	// 16-bit interleaved stereo samples
	case RS_STIPCM16S:
	case RS_STIPCM16U:
		{
			int iadd = 0;
			if (nFlags == RS_STIPCM16U) iadd = -32768;
			len = pIns->nLength;
			if (len*4 > dwMemLength) len = dwMemLength >> 2;
			int16_t *psrc = (int16_t *)lpMemFile;
			int16_t *pSample = (int16_t *)pIns->pSample;
			for (UINT j=0; j<len; j++)
			{
				pSample[j*2] = (int16_t)(bswapLE16(psrc[0]) + iadd);
				pSample[j*2+1] = (int16_t)(bswapLE16(psrc[1]) + iadd);
				psrc += 2;
			}
			len *= 4;
		}
		break;

	// AMS compressed samples
	case RS_AMS8:
	case RS_AMS16:
		len = 9;
		if (dwMemLength > 9)
		{
			const char *psrc = lpMemFile;
			char packcharacter = lpMemFile[8], *pdest = (char *)pIns->pSample;
			len += bswapLE32(*((LPDWORD)(lpMemFile+4)));
			if (len > dwMemLength) len = dwMemLength;
			UINT dmax = pIns->nLength;
			if (pIns->uFlags & CHN_16BIT) dmax <<= 1;
			AMSUnpack(psrc+9, len-9, pdest, dmax, packcharacter);
		}
		break;

	// PTM 8bit delta to 16-bit sample
	case RS_PTM8DTO16:
		{
			len = pIns->nLength * 2;
			if (len > dwMemLength) break;
			int8_t *pSample = (int8_t *)pIns->pSample;
			int8_t delta8 = 0;
			for (UINT j=0; j<len; j++)
			{
				delta8 += lpMemFile[j];
				*pSample++ = delta8;
			}
			uint16_t *pSampleW = (uint16_t *)pIns->pSample;
			for (UINT j=0; j<len; j+=2)   // swaparoni!
			{
				uint16_t rawSample = *pSampleW;
			        *pSampleW++ = bswapLE16(rawSample);
			}
		}
		break;

	// Huffman MDL compressed samples
	case RS_MDL8:
	case RS_MDL16:
		len = dwMemLength;
		if (len >= 4)
		{
			LPBYTE pSample = (LPBYTE)pIns->pSample;
			LPBYTE ibuf = (LPBYTE)lpMemFile;
			DWORD bitbuf = bswapLE32(*((DWORD *)ibuf));
			UINT bitnum = 32;
			BYTE dlt = 0, lowbyte = 0;
			ibuf += 4;
			for (UINT j=0; j<pIns->nLength; j++)
			{
				BYTE hibyte;
				BYTE sign;
				if (nFlags == RS_MDL16) lowbyte = (BYTE)MDLReadBits(bitbuf, bitnum, ibuf, 8);
				sign = (BYTE)MDLReadBits(bitbuf, bitnum, ibuf, 1);
				if (MDLReadBits(bitbuf, bitnum, ibuf, 1))
				{
					hibyte = (BYTE)MDLReadBits(bitbuf, bitnum, ibuf, 3);
				} else
				{
					hibyte = 8;
					while (!MDLReadBits(bitbuf, bitnum, ibuf, 1)) hibyte += 0x10;
					hibyte += MDLReadBits(bitbuf, bitnum, ibuf, 4);
				}
				if (sign) hibyte = ~hibyte;
				dlt += hibyte;
				if (nFlags != RS_MDL16)
					pSample[j] = dlt;
				else
				{
					pSample[j<<1] = lowbyte;
					pSample[(j<<1)+1] = dlt;
				}
			}
		}
		break;

	case RS_DMF8:
	case RS_DMF16:
		len = dwMemLength;
		if (len >= 4)
		{
			UINT maxlen = pIns->nLength;
			if (pIns->uFlags & CHN_16BIT) maxlen <<= 1;
			LPBYTE ibuf = (LPBYTE)lpMemFile, ibufmax = (LPBYTE)(lpMemFile+dwMemLength);
			len = DMFUnpack((LPBYTE)pIns->pSample, ibuf, ibufmax, maxlen);
		}
		break;

#ifdef MODPLUG_TRACKER
	// PCM 24-bit signed -> load sample, and normalize it to 16-bit
	case RS_PCM24S:
	case RS_PCM32S:
		len = pIns->nLength * 3;
		if (nFlags == RS_PCM32S) len += pIns->nLength;
		if (len > dwMemLength) break;
		if (len > 4*8)
		{
			UINT slsize = (nFlags == RS_PCM32S) ? 4 : 3;
			LPBYTE pSrc = (LPBYTE)lpMemFile;
			LONG max = 255;
			if (nFlags == RS_PCM32S) pSrc++;
			for (UINT j=0; j<len; j+=slsize)
			{
				LONG l = ((((pSrc[j+2] << 8) + pSrc[j+1]) << 8) + pSrc[j]) << 8;
				l /= 256;
				if (l > max) max = l;
				if (-l > max) max = -l;
			}
			max = (max / 128) + 1;
			int16_t *pDest = (int16_t *)pIns->pSample;
			for (UINT k=0; k<len; k+=slsize)
			{
				LONG l = ((((pSrc[k+2] << 8) + pSrc[k+1]) << 8) + pSrc[k]) << 8;
				*pDest++ = (uint16_t)(l / max);
			}
		}
		break;

	// Stereo PCM 24-bit signed -> load sample, and normalize it to 16-bit
	case RS_STIPCM24S:
	case RS_STIPCM32S:
		len = pIns->nLength * 6;
		if (nFlags == RS_STIPCM32S) len += pIns->nLength * 2;
		if (len > dwMemLength) break;
		if (len > 8*8)
		{
			UINT slsize = (nFlags == RS_STIPCM32S) ? 4 : 3;
			LPBYTE pSrc = (LPBYTE)lpMemFile;
			LONG max = 255;
			if (nFlags == RS_STIPCM32S) pSrc++;
			for (UINT j=0; j<len; j+=slsize)
			{
				LONG l = ((((pSrc[j+2] << 8) + pSrc[j+1]) << 8) + pSrc[j]) << 8;
				l /= 256;
				if (l > max) max = l;
				if (-l > max) max = -l;
			}
			max = (max / 128) + 1;
			int16_t *pDest = (int16_t *)pIns->pSample;
			for (UINT k=0; k<len; k+=slsize)
			{
				LONG lr = ((((pSrc[k+2] << 8) + pSrc[k+1]) << 8) + pSrc[k]) << 8;
				k += slsize;
				LONG ll = ((((pSrc[k+2] << 8) + pSrc[k+1]) << 8) + pSrc[k]) << 8;
				pDest[0] = (int16_t)ll;
				pDest[1] = (int16_t)lr;
				pDest += 2;
			}
		}
		break;

	// 16-bit signed big endian interleaved stereo
	case RS_STIPCM16M:
		{
			len = pIns->nLength;
			if (len*4 > dwMemLength) len = dwMemLength >> 2;
			LPCBYTE psrc = (LPCBYTE)lpMemFile;
			int16_t *pSample = (int16_t *)pIns->pSample;
			for (UINT j=0; j<len; j++)
			{
				pSample[j*2] = (int16_t)(((UINT)psrc[0] << 8) | (psrc[1]));
				pSample[j*2+1] = (int16_t)(((UINT)psrc[2] << 8) | (psrc[3]));
				psrc += 4;
			}
			len *= 4;
		}
		break;

#endif // MODPLUG_TRACKER
#endif // !MODPLUG_BASIC_SUPPORT

	// Default: 8-bit signed PCM data
	default:
		len = pIns->nLength;
		if (len > dwMemLength) len = pIns->nLength = dwMemLength;
		SDL_memcpy(pIns->pSample, lpMemFile, len);
	}
	if (len > dwMemLength)
	{
		if (pIns->pSample)
		{
			pIns->nLength = 0;
			FreeSample(pIns->pSample);
			pIns->pSample = NULL;
		}
		return 0;
	}
	AdjustSampleLoop(pIns);
	return len;
}


void CSoundFile::AdjustSampleLoop(MODINSTRUMENT *pIns)
//----------------------------------------------------
{
	if (!pIns->pSample) return;
	if (pIns->nLength > MAX_SAMPLE_LENGTH) pIns->nLength = MAX_SAMPLE_LENGTH;
	if (pIns->nLoopEnd > pIns->nLength) pIns->nLoopEnd = pIns->nLength;
	if (pIns->nLoopStart > pIns->nLength+2) pIns->nLoopStart = pIns->nLength+2;
	if (pIns->nLoopStart+2 >= pIns->nLoopEnd)
	{
		pIns->nLoopStart = pIns->nLoopEnd = 0;
		pIns->uFlags &= ~CHN_LOOP;
	}
	UINT len = pIns->nLength;
	if (pIns->uFlags & CHN_16BIT)
	{
		int16_t *pSample = (int16_t *)pIns->pSample;
		// Adjust end of sample
		if (pIns->uFlags & CHN_STEREO)
		{
			pSample[len*2+6] = pSample[len*2+4] = pSample[len*2+2] = pSample[len*2] = 0;
			pSample[len*2+7] = pSample[len*2+5] = pSample[len*2+3] = pSample[len*2+1] = 0;
		} else
		{
			pSample[len+4] = pSample[len+3] = pSample[len+2] = pSample[len+1] = pSample[len] = 0;
		}
		if ((pIns->uFlags & (CHN_LOOP|CHN_PINGPONGLOOP|CHN_STEREO)) == CHN_LOOP)
		{
			// Fix bad loops
			if ((pIns->nLoopEnd+3 >= pIns->nLength) || (m_nType & MOD_TYPE_S3M))
			{
				pSample[pIns->nLoopEnd] = pSample[pIns->nLoopStart];
				pSample[pIns->nLoopEnd+1] = pSample[pIns->nLoopStart+1];
				pSample[pIns->nLoopEnd+2] = pSample[pIns->nLoopStart+2];
				pSample[pIns->nLoopEnd+3] = pSample[pIns->nLoopStart+3];
				pSample[pIns->nLoopEnd+4] = pSample[pIns->nLoopStart+4];
			}
		}
	} else
	{
		signed char *pSample = pIns->pSample;
		// Crappy samples (except chiptunes) ?
		if ((pIns->nLength > 0x100) && (m_nType & (MOD_TYPE_MOD|MOD_TYPE_S3M))
		 && (!(pIns->uFlags & CHN_STEREO)))
		{
			int smpend = pSample[pIns->nLength-1], smpfix = 0, kscan;
			for (kscan=pIns->nLength-1; kscan>0; kscan--)
			{
				smpfix = pSample[kscan-1];
				if (smpfix != smpend) break;
			}
			int delta = smpfix - smpend;
			if (((!(pIns->uFlags & CHN_LOOP)) || (kscan > (int)pIns->nLoopEnd))
			 && ((delta < -8) || (delta > 8)))
			{
				while (kscan<(int)pIns->nLength)
				{
					if (!(kscan & 7))
					{
						if (smpfix > 0) smpfix--;
						if (smpfix < 0) smpfix++;
					}
					pSample[kscan] = (signed char)smpfix;
					kscan++;
				}
			}
		}

		// Adjust end of sample
		if (pIns->uFlags & CHN_STEREO)
		{
			pSample[len*2+6] = pSample[len*2+4] = pSample[len*2+2] = pSample[len*2] = 0;
			pSample[len*2+7] = pSample[len*2+5] = pSample[len*2+3] = pSample[len*2+1] = 0;

		} else
		{
			pSample[len+4] = pSample[len+3] = pSample[len+2] = pSample[len+1] = pSample[len] = 0;
		}
		if ((pIns->uFlags & (CHN_LOOP|CHN_PINGPONGLOOP|CHN_STEREO)) == CHN_LOOP)
		{
			if ((pIns->nLoopEnd+3 >= pIns->nLength) || (m_nType & (MOD_TYPE_MOD|MOD_TYPE_S3M)))
			{
				pSample[pIns->nLoopEnd] = pSample[pIns->nLoopStart];
				pSample[pIns->nLoopEnd+1] = pSample[pIns->nLoopStart+1];
				pSample[pIns->nLoopEnd+2] = pSample[pIns->nLoopStart+2];
				pSample[pIns->nLoopEnd+3] = pSample[pIns->nLoopStart+3];
				pSample[pIns->nLoopEnd+4] = pSample[pIns->nLoopStart+4];
			}
		}
	}
}


/////////////////////////////////////////////////////////////
// Transpose <-> Frequency conversions

// returns 8363*2^((transp*128+ftune)/(12*128))
DWORD CSoundFile::TransposeToFrequency(int transp, int ftune)
//-----------------------------------------------------------
{
	return (DWORD)(8363*SDL_pow(2, (double)(transp*128+ftune)/(1536)));
}


// returns 12*128*log2(freq/8363)
int CSoundFile::FrequencyToTranspose(DWORD freq)
//----------------------------------------------
{
	return int(1536*(SDL_log(freq/8363.0)/SDL_log(2.0)));
}


void CSoundFile::FrequencyToTranspose(MODINSTRUMENT *psmp)
//--------------------------------------------------------
{
	int f2t = FrequencyToTranspose(psmp->nC4Speed);
	int transp = f2t >> 7;
	int ftune = f2t & 0x7F;
	if (ftune > 80)
	{
		transp++;
		ftune -= 128;
	}
	if (transp > 127) transp = 127;
	if (transp < -127) transp = -127;
	psmp->RelativeTone = transp;
	psmp->nFineTune = ftune;
}

BOOL CSoundFile::SetPatternName(UINT nPat, LPCSTR lpszName)
//---------------------------------------------------------
{
        char szName[MAX_PATTERNNAME];
        szName[0] = 0;
	// check input arguments
	if (nPat >= MAX_PATTERNS) return FALSE;
	if (lpszName == NULL) return(FALSE);

	if (lpszName) SDL_strlcpy(szName, lpszName, MAX_PATTERNNAME);
	szName[MAX_PATTERNNAME-1] = 0;
	if (!m_lpszPatternNames) m_nPatternNames = 0;
	if (nPat >= m_nPatternNames)
	{
		if (!lpszName[0]) return TRUE;
		UINT len = (nPat+1)*MAX_PATTERNNAME;
		char *p = new char[len];
		if (!p) return FALSE;
		SDL_memset(p, 0, len);
		if (m_lpszPatternNames)
		{
			SDL_memcpy(p, m_lpszPatternNames, m_nPatternNames * MAX_PATTERNNAME);
			delete [] m_lpszPatternNames;
			m_lpszPatternNames = NULL;
		}
		m_lpszPatternNames = p;
		m_nPatternNames = nPat + 1;
	}
	SDL_memcpy(m_lpszPatternNames + nPat * MAX_PATTERNNAME, szName, MAX_PATTERNNAME);
	return TRUE;
}


UINT CSoundFile::DetectUnusedSamples(BOOL *pbIns)
//-----------------------------------------------
{
	UINT nExt = 0;

	if (!pbIns) return 0;
	if (m_nInstruments)
	{
		SDL_memset(pbIns, 0, MAX_SAMPLES * sizeof(BOOL));
		for (UINT ipat=0; ipat<MAX_PATTERNS; ipat++)
		{
			MODCOMMAND *p = Patterns[ipat];
			if (p)
			{
				UINT jmax = PatternSize[ipat] * m_nChannels;
				for (UINT j=0; j<jmax; j++, p++)
				{
					if ((p->note) && (p->note <= NOTE_MAX))
					{
						if ((p->instr) && (p->instr < MAX_INSTRUMENTS))
						{
							INSTRUMENTHEADER *penv = Headers[p->instr];
							if (penv)
							{
								UINT n = penv->Keyboard[p->note-1];
								if (n < MAX_SAMPLES) pbIns[n] = TRUE;
							}
						} else
						{
							for (UINT k=1; k<=m_nInstruments; k++)
							{
								INSTRUMENTHEADER *penv = Headers[k];
								if (penv)
								{
									UINT n = penv->Keyboard[p->note-1];
									if (n < MAX_SAMPLES) pbIns[n] = TRUE;
								}
							}
						}
					}
				}
			}
		}
		for (UINT ichk=1; ichk<=m_nSamples; ichk++)
		{
			if ((!pbIns[ichk]) && (Ins[ichk].pSample)) nExt++;
		}
	}
	return nExt;
}


BOOL CSoundFile::DestroySample(UINT nSample)
//------------------------------------------
{
	if ((!nSample) || (nSample >= MAX_SAMPLES)) return FALSE;
	if (!Ins[nSample].pSample) return TRUE;
	MODINSTRUMENT *pins = &Ins[nSample];
	signed char *pSample = pins->pSample;
	pins->pSample = NULL;
	pins->nLength = 0;
	pins->uFlags &= ~(CHN_16BIT);
	for (UINT i=0; i<MAX_CHANNELS; i++)
	{
		if (Chn[i].pSample == pSample)
		{
			Chn[i].nPos = Chn[i].nLength = 0;
			Chn[i].pSample = Chn[i].pCurrentSample = NULL;
		}
	}
	FreeSample(pSample);
	return TRUE;
}

