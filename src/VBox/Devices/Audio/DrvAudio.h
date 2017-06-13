/* $Id$ */
/** @file
 * Intermediate audio driver header.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 * --------------------------------------------------------------------
 *
 * This code is based on: audio.h
 *
 * QEMU Audio subsystem header
 *
 * Copyright (c) 2003-2005 Vassili Karpov (malc)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef DRV_AUDIO_H
#define DRV_AUDIO_H

#include <limits.h>

#include <iprt/circbuf.h>
#include <iprt/critsect.h>
#include <iprt/file.h>
#include <iprt/path.h>

#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pdm.h>
#include <VBox/vmm/pdmaudioifs.h>

#ifdef DEBUG_andy
# define VBOX_AUDIO_DEBUG_DUMP_PCM_DATA
#endif

#ifdef VBOX_AUDIO_DEBUG_DUMP_PCM_DATA
# ifdef RT_OS_WINDOWS
#  define VBOX_AUDIO_DEBUG_DUMP_PCM_DATA_PATH "c:\\temp\\"
# else
#  define VBOX_AUDIO_DEBUG_DUMP_PCM_DATA_PATH "/tmp/"
# endif
#endif /* VBOX_AUDIO_DEBUG_DUMP_PCM_DATA */

typedef enum
{
    AUD_OPT_INT,
    AUD_OPT_FMT,
    AUD_OPT_STR,
    AUD_OPT_BOOL
} audio_option_tag_e;

typedef struct audio_option
{
    const char *name;
    audio_option_tag_e tag;
    void *valp;
    const char *descr;
    int *overridenp;
    int overriden;
} audio_option;

#ifdef VBOX_WITH_STATISTICS
/**
 * Structure for keeping stream statistics for the
 * statistic manager (STAM).
 */
typedef struct DRVAUDIOSTATS
{
    STAMCOUNTER TotalStreamsActive;
    STAMCOUNTER TotalStreamsCreated;
    STAMCOUNTER TotalSamplesRead;
    STAMCOUNTER TotalSamplesWritten;
    STAMCOUNTER TotalSamplesMixedIn;
    STAMCOUNTER TotalSamplesMixedOut;
    STAMCOUNTER TotalSamplesLostIn;
    STAMCOUNTER TotalSamplesLostOut;
    STAMCOUNTER TotalSamplesOut;
    STAMCOUNTER TotalSamplesIn;
    STAMCOUNTER TotalBytesRead;
    STAMCOUNTER TotalBytesWritten;
    /** How much delay (in ms) for input processing. */
    STAMPROFILEADV DelayIn;
    /** How much delay (in ms) for output processing. */
    STAMPROFILEADV DelayOut;
} DRVAUDIOSTATS, *PDRVAUDIOSTATS;
#endif

/**
 * Audio driver instance data.
 *
 * @implements PDMIAUDIOCONNECTOR
 */
typedef struct DRVAUDIO
{
    /** Input/output processing thread. */
    RTTHREAD                hThread;
    /** Critical section for serializing access. */
    RTCRITSECT              CritSect;
    /** Shutdown indicator. */
    bool                    fTerminate;
    /** Our audio connector interface. */
    PDMIAUDIOCONNECTOR      IAudioConnector;
    /** Pointer to the driver instance. */
    PPDMDRVINS              pDrvIns;
    /** Pointer to audio driver below us. */
    PPDMIHOSTAUDIO          pHostDrvAudio;
    /** List of host input/output audio streams. */
    RTLISTANCHOR            lstHstStreams;
    /** List of guest input/output audio streams. */
    RTLISTANCHOR            lstGstStreams;
    /** Max. number of free input streams.
     *  UINT32_MAX for unlimited streams. */
    uint32_t                cStreamsFreeIn;
    /** Max. number of free output streams.
     *  UINT32_MAX for unlimited streams. */
    uint32_t                cStreamsFreeOut;
#ifdef VBOX_WITH_AUDIO_ENUM
    /** Flag indicating to perform an (re-)enumeration of the host audio devices. */
    bool                    fEnumerateDevices;
#endif
    /** Audio configuration settings retrieved from the backend. */
    PDMAUDIOBACKENDCFG      BackendCfg;
#ifdef VBOX_WITH_AUDIO_DEVICE_CALLBACKS
    /** @todo Use a map with primary key set to the callback type? */
    RTLISTANCHOR            lstCBIn;
    RTLISTANCHOR            lstCBOut;
#endif
#ifdef VBOX_WITH_STATISTICS
    /** Statistics for the statistics manager (STAM). */
    DRVAUDIOSTATS           Stats;
#endif
} DRVAUDIO, *PDRVAUDIO;

/** Makes a PDRVAUDIO out of a PPDMIAUDIOCONNECTOR. */
#define PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface) \
    ( (PDRVAUDIO)((uintptr_t)pInterface - RT_OFFSETOF(DRVAUDIO, IAudioConnector)) )


bool DrvAudioHlpAudFmtIsSigned(PDMAUDIOFMT enmFmt);
uint8_t DrvAudioHlpAudFmtToBits(PDMAUDIOFMT enmFmt);
const char *DrvAudioHlpAudFmtToStr(PDMAUDIOFMT enmFmt);
void DrvAudioHlpClearBuf(const PPDMAUDIOPCMPROPS pPCMInfo, void *pvBuf, size_t cbBuf, uint32_t cSamples);
uint32_t DrvAudioHlpCalcBitrate(uint8_t cBits, uint32_t uHz, uint8_t cChannels);
uint32_t DrvAudioHlpCalcBitrate(const PPDMAUDIOPCMPROPS pProps);
bool DrvAudioHlpPCMPropsAreEqual(const PPDMAUDIOPCMPROPS pPCMProps1, const PPDMAUDIOPCMPROPS pPCMProps2);
bool DrvAudioHlpPCMPropsAreEqual(const PPDMAUDIOPCMPROPS pPCMProps, const PPDMAUDIOSTREAMCFG pCfg);
bool DrvAudioHlpPCMPropsAreValid(const PPDMAUDIOPCMPROPS pProps);
void DrvAudioHlpPCMPropsPrint(const PPDMAUDIOPCMPROPS pProps);
int DrvAudioHlpPCMPropsToStreamCfg(const PPDMAUDIOPCMPROPS pPCMProps, PPDMAUDIOSTREAMCFG pCfg);
const char *DrvAudioHlpRecSrcToStr(const PDMAUDIORECSOURCE enmRecSource);
void DrvAudioHlpStreamCfgPrint(const PPDMAUDIOSTREAMCFG pCfg);
bool DrvAudioHlpStreamCfgIsValid(const PPDMAUDIOSTREAMCFG pCfg);
int DrvAudioHlpStreamCfgCopy(PPDMAUDIOSTREAMCFG pDstCfg, const PPDMAUDIOSTREAMCFG pSrcCfg);
PPDMAUDIOSTREAMCFG DrvAudioHlpStreamCfgDup(const PPDMAUDIOSTREAMCFG pCfg);
void DrvAudioHlpStreamCfgFree(PPDMAUDIOSTREAMCFG pCfg);
const char *DrvAudioHlpStreamCmdToStr(PDMAUDIOSTREAMCMD enmCmd);
PDMAUDIOFMT DrvAudioHlpStrToAudFmt(const char *pszFmt);

int DrvAudioHlpSanitizeFileName(char *pszPath, size_t cbPath);
int DrvAudioHlpGetFileName(char *pszFile, size_t cchFile, const char *pszPath, const char *pszName, PDMAUDIOFILETYPE enmType);

PPDMAUDIODEVICE DrvAudioHlpDeviceAlloc(size_t cbData);
void DrvAudioHlpDeviceFree(PPDMAUDIODEVICE pDev);
PPDMAUDIODEVICE DrvAudioHlpDeviceDup(const PPDMAUDIODEVICE pDev, bool fCopyUserData);

int DrvAudioHlpDeviceEnumInit(PPDMAUDIODEVICEENUM pDevEnm);
void DrvAudioHlpDeviceEnumFree(PPDMAUDIODEVICEENUM pDevEnm);
int DrvAudioHlpDeviceEnumAdd(PPDMAUDIODEVICEENUM pDevEnm, PPDMAUDIODEVICE pDev);
int DrvAudioHlpDeviceEnumCopyEx(PPDMAUDIODEVICEENUM pDstDevEnm, const PPDMAUDIODEVICEENUM pSrcDevEnm, PDMAUDIODIR enmUsage);
int DrvAudioHlpDeviceEnumCopy(PPDMAUDIODEVICEENUM pDstDevEnm, const PPDMAUDIODEVICEENUM pSrcDevEnm);
PPDMAUDIODEVICEENUM DrvAudioHlpDeviceEnumDup(const PPDMAUDIODEVICEENUM pDevEnm);
int DrvAudioHlpDeviceEnumCopy(PPDMAUDIODEVICEENUM pDstDevEnm, const PPDMAUDIODEVICEENUM pSrcDevEnm);
int DrvAudioHlpDeviceEnumCopyEx(PPDMAUDIODEVICEENUM pDstDevEnm, const PPDMAUDIODEVICEENUM pSrcDevEnm, PDMAUDIODIR enmUsage, bool fCopyUserData);
PPDMAUDIODEVICE DrvAudioHlpDeviceEnumGetDefaultDevice(const PPDMAUDIODEVICEENUM pDevEnm, PDMAUDIODIR enmDir);
void DrvAudioHlpDeviceEnumPrint(const char *pszDesc, const PPDMAUDIODEVICEENUM pDevEnm);

const char *DrvAudioHlpAudDirToStr(PDMAUDIODIR enmDir);
const char *DrvAudioHlpAudMixerCtlToStr(PDMAUDIOMIXERCTL enmMixerCtl);
char *DrvAudioHlpAudDevFlagsToStrA(PDMAUDIODEVFLAG fFlags);

int DrvAudioHlpWAVFileOpen(PPDMAUDIOFILE pFile, const char *pszFile, uint32_t fOpen, const PPDMAUDIOPCMPROPS pProps, PDMAUDIOFILEFLAGS fFlags);
int DrvAudioHlpWAVFileClose(PPDMAUDIOFILE pFile);
size_t DrvAudioHlpWAVFileGetDataSize(PPDMAUDIOFILE pFile);
int DrvAudioHlpWAVFileWrite(PPDMAUDIOFILE pFile, const void *pvBuf, size_t cbBuf, uint32_t fFlags);

#define AUDIO_MAKE_FOURCC(c0, c1, c2, c3) RT_H2LE_U32_C(RT_MAKE_U32_FROM_U8(c0, c1, c2, c3))

#endif /* DRV_AUDIO_H */

