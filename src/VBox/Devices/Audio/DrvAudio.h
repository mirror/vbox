/* $Id$ */
/** @file
 * Intermediate audio driver header.
 */

/*
 * Copyright (C) 2006-2014 Oracle Corporation
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

#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pdm.h>
#include <VBox/vmm/pdmaudioifs.h>

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

/**
 * Audio driver instance data.
 *
 * @implements PDMIAUDIOCONNECTOR
 */
typedef struct DRVAUDIO
{
    /** Input/output processing thread. */
    RTTHREAD                hThread;
    /** Event for input/ouput processing. */
    RTSEMEVENT              hEvent;
    /** Shutdown indicator. */
    bool                    fTerminate;
    /** The audio interface presented to the device/driver above us. */
    PDMIAUDIOCONNECTOR      IAudioConnector;
    /** Pointer to the driver instance. */
    PPDMDRVINS              pDrvIns;
    /** Pointer to audio driver below us. */
    PPDMIHOSTAUDIO          pHostDrvAudio;
    RTLISTANCHOR            lstHstStrmIn;
    RTLISTANCHOR            lstHstStrmOut;
    /** Max. number of free input streams. */
    uint8_t                 cFreeInputStreams;
    /** Max. number of free output streams. */
    uint8_t                 cFreeOutputStreams;
    /** Audio configuration settings retrieved
     *  from the backend. */
    PDMAUDIOBACKENDCFG      BackendCfg;

} DRVAUDIO, *PDRVAUDIO;

/** Makes a PDRVBLOCK out of a PPDMIBLOCK. */
#define PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface) \
    ( (PDRVAUDIO)((uintptr_t)pInterface - RT_OFFSETOF(DRVAUDIO, IAudioConnector)) )

//const char *drvAudioHlpFormatToString(PDMAUDIOFMT fmt);
const char *drvAudioRecSourceToString(PDMAUDIORECSOURCE enmRecSource);
PDMAUDIOFMT drvAudioHlpStringToFormat(const char *pszFormat);

bool drvAudioPCMPropsAreEqual(PPDMPCMPROPS info, PPDMAUDIOSTREAMCFG pCfg);
int drvAudioStreamCfgToProps(PPDMAUDIOSTREAMCFG pCfg, PPDMPCMPROPS pProps);
void drvAudioStreamCfgPrint(PPDMAUDIOSTREAMCFG pCfg);

/* AUDIO IN function declarations. */
void drvAudioHlpPcmSwFreeResourcesIn(PPDMAUDIOGSTSTRMIN pGstStrmIn);
void drvAudioGstInFreeRes(PPDMAUDIOGSTSTRMIN pGstStrmIn);
void drvAudioGstInRemove(PPDMAUDIOGSTSTRMIN pGstStrmIn);
uint32_t drvAudioHstInFindMinCaptured(PPDMAUDIOHSTSTRMIN pHstStrmIn);
void drvAudioHstInFreeRes(PPDMAUDIOHSTSTRMIN pHstStrmIn);
uint32_t drvAudioHstInGetFree(PPDMAUDIOHSTSTRMIN pHstStrmIn);
uint32_t drvAudioHstInGetLive(PPDMAUDIOHSTSTRMIN pHstStrmIn);
void drvAudioGstInRemove(PPDMAUDIOGSTSTRMIN pGstStrmIn);
int  drvAudioGstInInit(PPDMAUDIOGSTSTRMIN pGstStrmIn, PPDMAUDIOHSTSTRMIN pHstStrmIn, const char *pszName, PPDMAUDIOSTREAMCFG pCfg);

PPDMAUDIOHSTSTRMIN drvAudioFindNextHstIn(PDRVAUDIO pDrvAudio, PPDMAUDIOHSTSTRMIN pHstStrmIn);
PPDMAUDIOHSTSTRMIN drvAudioFindNextEnabledHstIn(PDRVAUDIO pDrvAudio, PPDMAUDIOHSTSTRMIN pHstStrmIn);
PPDMAUDIOHSTSTRMIN drvAudioFindNextEqHstIn(PDRVAUDIO pDrvAudio, PPDMAUDIOHSTSTRMIN pHstStrmIn, PPDMAUDIOSTREAMCFG pCfg);

/* AUDIO OUT function declarations. */
int  drvAudioGstOutAlloc(PPDMAUDIOGSTSTRMOUT pGstStrmOut);
void drvAudioGstOutFreeRes(PPDMAUDIOGSTSTRMOUT pGstStrmOut);
void drvAudioHstOutFreeRes(PPDMAUDIOHSTSTRMOUT pHstStrmOut);
int  drvAudioDestroyGstOut(PDRVAUDIO pDrvAudio, PPDMAUDIOGSTSTRMOUT pGstStrmOut);
void drvAudioDestroyHstOut(PDRVAUDIO pDrvAudio, PDMAUDIOHSTSTRMOUT pHstStrmOut);
int  drvAudioGstOutInit(PPDMAUDIOGSTSTRMOUT pGstStrmOut, PPDMAUDIOHSTSTRMOUT pHstStrmOut, const char *pszName, PPDMAUDIOSTREAMCFG pCfg);

PPDMAUDIOHSTSTRMOUT drvAudioFindAnyHstOut(PDRVAUDIO pDrvAudio, PPDMAUDIOHSTSTRMOUT pHstStrmOut);
PPDMAUDIOHSTSTRMOUT drvAudioHstFindAnyEnabledOut(PDRVAUDIO pDrvAudio, PPDMAUDIOHSTSTRMOUT pHstStrmOut);
PPDMAUDIOHSTSTRMOUT drvAudioFindSpecificOut(PDRVAUDIO pDrvAudio, PPDMAUDIOHSTSTRMOUT pHstStrmOut, PPDMAUDIOSTREAMCFG pCfg);
int drvAudioAllocHstOut(PDRVAUDIO pDrvAudio, const char *pszName, PPDMAUDIOSTREAMCFG pCfg, PPDMAUDIOHSTSTRMOUT *ppHstStrmOut);
int drvAudioHlpPcmHwAddOut(PDRVAUDIO pDrvAudio, PPDMAUDIOSTREAMCFG pCfg, PPDMAUDIOHSTSTRMOUT *ppHstStrmOut);
int drvAudioHlpPcmCreateVoicePairOut(PDRVAUDIO pDrvAudio, const char *pszName, PPDMAUDIOSTREAMCFG pCfg, PPDMAUDIOGSTSTRMOUT *ppGstStrmOut);

/* Common functions between DrvAudio and backends (host audio drivers). */
int  drvAudioAttachCapture(PDRVAUDIO pDrvAudio, PPDMAUDIOHSTSTRMOUT pHstStrmOut);
void drvAudioDetachCapture(PPDMAUDIOHSTSTRMOUT pHstStrmOut);
uint32_t drvAudioHstOutSamplesLive(PPDMAUDIOHSTSTRMOUT pHstStrmOut, uint32_t *pcStreamsLive);

void audio_pcm_info_clear_buf(PPDMPCMPROPS pPCMInfo, void *pvBuf, int len);

/* Externals. */
extern volume_t nominal_volume;
extern volume_t pcm_in_volume;

typedef struct fixed_settings
{
    int enabled;
    int cStreams;
    int greedy;
    PDMAUDIOSTREAMCFG settings;
} fixed_settings;

static struct {
    struct fixed_settings fixed_out;
    struct fixed_settings fixed_in;
    union {
        int hz;
        int64_t ticks;
    } period;
    int plive;
} conf = {

    /* Fixed output settings. */
    {                           /* DAC fixed settings */
        1,                      /* enabled */
        1,                      /* cStreams */
        1,                      /* greedy */
        {
            44100,              /* freq */
            2,                  /* nchannels */
            AUD_FMT_S16,        /* fmt */
            PDMAUDIOHOSTENDIANESS
        }
    },

    /* Fixed input settings. */
    {                           /* ADC fixed settings */
        1,                      /* enabled */
        1,                      /* cStreams */
        1,                      /* greedy */
        {
            44100,              /* freq */
            2,                  /* nchannels */
            AUD_FMT_S16,        /* fmt */
            PDMAUDIOHOSTENDIANESS
        }
    },

    { 200 },                    /* frequency (in Hz) */
    0,                          /* plive */ /** @todo Disable pending live? */
};
#endif /* DRV_AUDIO_H */

