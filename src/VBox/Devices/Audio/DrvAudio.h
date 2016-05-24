/* $Id$ */
/** @file
 * Intermediate audio driver header.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
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
    /** List of input/output audio streams. */
    RTLISTANCHOR            lstStreams;
    /** Max. number of free input streams.
     *  UINT32_MAX for unlimited streams. */
    uint32_t                cStreamsFreeIn;
    /** Max. number of free output streams.
     *  UINT32_MAX for unlimited streams. */
    uint32_t                cStreamsFreeOut;
    /** Audio configuration settings retrieved from the backend. */
    PDMAUDIOBACKENDCFG      BackendCfg;
#ifdef VBOX_WITH_AUDIO_CALLBACKS
    /** @todo Use a map with primary key set to the callback type? */
    RTLISTANCHOR            lstCBIn;
    RTLISTANCHOR            lstCBOut;
#endif
} DRVAUDIO, *PDRVAUDIO;

/** Makes a PDRVAUDIO out of a PPDMIAUDIOCONNECTOR. */
#define PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface) \
    ( (PDRVAUDIO)((uintptr_t)pInterface - RT_OFFSETOF(DRVAUDIO, IAudioConnector)) )


bool DrvAudioAudFmtIsSigned(PDMAUDIOFMT enmFmt);
uint8_t DrvAudioAudFmtToBits(PDMAUDIOFMT enmFmt);
const char *DrvAudioAudFmtToStr(PDMAUDIOFMT enmFmt);
void DrvAudioClearBuf(PPDMPCMPROPS pPCMInfo, void *pvBuf, size_t cbBuf, uint32_t cSamples);
bool DrvAudioPCMPropsAreEqual(PPDMPCMPROPS pPCMProps1, PPDMPCMPROPS pPCMProps2);
bool DrvAudioPCMPropsAreEqual(PPDMPCMPROPS pPCMProps, PPDMAUDIOSTREAMCFG pCfg);
int DrvAudioPCMPropsToStreamCfg(PPDMPCMPROPS pPCMProps, PPDMAUDIOSTREAMCFG pCfg);
const char *DrvAudRecSrcToStr(PDMAUDIORECSOURCE enmRecSource);
void DrvAudioStreamCfgPrint(PPDMAUDIOSTREAMCFG pCfg);
bool DrvAudioStreamCfgIsValid(PPDMAUDIOSTREAMCFG pCfg);
int DrvAudioStreamCfgToProps(PPDMAUDIOSTREAMCFG pCfg, PPDMPCMPROPS pProps);
PDMAUDIOFMT DrvAudioStrToAudFmt(const char *pszFmt);

#endif /* DRV_AUDIO_H */

