/* $Id$ */
/** @file
 * NULL audio driver -- also acts as a fallback if no
 * other backend is available.
 */

/*
 * Copyright (C) 2006-2015 Oracle Corporation
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
 * This code is based on: noaudio.c QEMU based code.
 *
 * QEMU Timer based audio emulation
 *
 * Copyright (c) 2004-2005 Vassili Karpov (malc)
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
#define LOG_GROUP LOG_GROUP_DRV_HOST_AUDIO
#include <VBox/log.h>
#include "DrvAudio.h"
#include "AudioMixBuffer.h"

#include "VBoxDD.h"

#include <iprt/alloc.h>
#include <iprt/uuid.h> /* For PDMIBASE_2_PDMDRV. */
#include <VBox/vmm/pdmaudioifs.h>

typedef struct NULLAUDIOSTREAMOUT
{
    /** Note: Always must come first! */
    PDMAUDIOHSTSTRMOUT streamOut;
    uint64_t           u64TicksLast;
    uint64_t           csPlayBuffer;
    uint8_t           *pu8PlayBuffer;
} NULLAUDIOSTREAMOUT, *PNULLAUDIOSTREAMOUT;

typedef struct NULLAUDIOSTREAMIN
{
    /** Note: Always must come first! */
    PDMAUDIOHSTSTRMIN  streamIn;
} NULLAUDIOSTREAMIN, *PNULLAUDIOSTREAMIN;

/**
 * NULL audio driver instance data.
 * @implements PDMIAUDIOCONNECTOR
 */
typedef struct DRVHOSTNULLAUDIO
{
    /** Pointer to the driver instance structure. */
    PPDMDRVINS    pDrvIns;
    /** Pointer to host audio interface. */
    PDMIHOSTAUDIO IHostAudio;
} DRVHOSTNULLAUDIO, *PDRVHOSTNULLAUDIO;

/*******************************************PDM_AUDIO_DRIVER******************************/


static DECLCALLBACK(int) drvHostNullAudioGetConf(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDCFG pCfg)
{
    NOREF(pInterface);
    AssertPtrReturn(pCfg, VERR_INVALID_POINTER);

    pCfg->cbStreamOut = sizeof(NULLAUDIOSTREAMOUT);
    pCfg->cbStreamIn  = sizeof(NULLAUDIOSTREAMIN);

    pCfg->cMaxHstStrmsOut = 1; /* Output */
    pCfg->cMaxHstStrmsIn  = 2; /* Line input + microphone input. */

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvHostNullAudioInit(PPDMIHOSTAUDIO pInterface)
{
    NOREF(pInterface);

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvHostNullAudioInitIn(PPDMIHOSTAUDIO pInterface,
                                                PPDMAUDIOHSTSTRMIN pHstStrmIn, PPDMAUDIOSTREAMCFG pCfg,
                                                PDMAUDIORECSOURCE enmRecSource,
                                                uint32_t *pcSamples)
{
    NOREF(pInterface);
    NOREF(enmRecSource);

    /* Just adopt the wanted stream configuration. */
    int rc = DrvAudioStreamCfgToProps(pCfg, &pHstStrmIn->Props);
    if (RT_SUCCESS(rc))
    {
        if (pcSamples)
            *pcSamples = _1K;
    }

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvHostNullAudioInitOut(PPDMIHOSTAUDIO pInterface,
                                                 PPDMAUDIOHSTSTRMOUT pHstStrmOut, PPDMAUDIOSTREAMCFG pCfg,
                                                 uint32_t *pcSamples)
{
    NOREF(pInterface);

    /* Just adopt the wanted stream configuration. */
    int rc = DrvAudioStreamCfgToProps(pCfg, &pHstStrmOut->Props);
    if (RT_SUCCESS(rc))
    {
        PNULLAUDIOSTREAMOUT pNullStrmOut = (PNULLAUDIOSTREAMOUT)pHstStrmOut;
        pNullStrmOut->u64TicksLast  = 0;
        pNullStrmOut->csPlayBuffer  = _1K;
        pNullStrmOut->pu8PlayBuffer = (uint8_t *)RTMemAlloc(_1K << pHstStrmOut->Props.cShift);
        if (pNullStrmOut->pu8PlayBuffer)
        {
            if (pcSamples)
                *pcSamples = pNullStrmOut->csPlayBuffer;
        }
        else
        {
            rc = VERR_NO_MEMORY;
        }
    }

    return rc;
}

static DECLCALLBACK(bool) drvHostNullAudioIsEnabled(PPDMIHOSTAUDIO pInterface, PDMAUDIODIR enmDir)
{
    NOREF(pInterface);
    NOREF(enmDir);
    return true; /* Always all enabled. */
}

static DECLCALLBACK(int) drvHostNullAudioPlayOut(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMOUT pHstStrmOut,
                                                 uint32_t *pcSamplesPlayed)
{
    PDRVHOSTNULLAUDIO pDrv = RT_FROM_MEMBER(pInterface, DRVHOSTNULLAUDIO, IHostAudio);
    PNULLAUDIOSTREAMOUT pNullStrmOut = (PNULLAUDIOSTREAMOUT)pHstStrmOut;

    /* Consume as many samples as would be played at the current frequency since last call. */
    uint32_t csLive          = AudioMixBufAvail(&pHstStrmOut->MixBuf);
    uint64_t u64TicksNow     = PDMDrvHlpTMGetVirtualTime(pDrv->pDrvIns);
    uint64_t u64TicksElapsed = u64TicksNow  - pNullStrmOut->u64TicksLast;
    uint64_t u64TicksFreq    = PDMDrvHlpTMGetVirtualFreq(pDrv->pDrvIns);

    /* Remember when samples were consumed. */
    pNullStrmOut->u64TicksLast = u64TicksNow;

    /*
     * Minimize the rounding error by adding 0.5: samples = int((u64TicksElapsed * samplesFreq) / u64TicksFreq + 0.5).
     * If rounding is not taken into account then the playback rate will be consistently lower that expected.
     */
    uint64_t cSamplesPlayed = (2 * u64TicksElapsed * pHstStrmOut->Props.uHz + u64TicksFreq) / u64TicksFreq / 2;

    /* Don't play more than available. */
    if (cSamplesPlayed > csLive)
        cSamplesPlayed = csLive;

    cSamplesPlayed = RT_MIN(cSamplesPlayed, pNullStrmOut->csPlayBuffer);

    uint32_t csRead = 0;
    AudioMixBufReadCirc(&pHstStrmOut->MixBuf, pNullStrmOut->pu8PlayBuffer,
                        AUDIOMIXBUF_S2B(&pHstStrmOut->MixBuf, cSamplesPlayed), &csRead);
    AudioMixBufFinish(&pHstStrmOut->MixBuf, csRead);

    if (pcSamplesPlayed)
        *pcSamplesPlayed = csRead;

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvHostNullAudioCaptureIn(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMIN pHstStrmIn,
                                                   uint32_t *pcSamplesCaptured)
{
    /* Never capture anything. */
    if (pcSamplesCaptured)
        *pcSamplesCaptured = 0;

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvHostNullAudioControlIn(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMIN pHstStrmIn,
                                                   PDMAUDIOSTREAMCMD enmStreamCmd)
{
    NOREF(pInterface);
    NOREF(pHstStrmIn);
    NOREF(enmStreamCmd);

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvHostNullAudioControlOut(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMOUT pHstStrmOut,
                                                    PDMAUDIOSTREAMCMD enmStreamCmd)
{
    NOREF(pInterface);
    NOREF(pHstStrmOut);
    NOREF(enmStreamCmd);

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvHostNullAudioFiniIn(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMIN pHstStrmIn)
{
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvHostNullAudioFiniOut(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMOUT pHstStrmOut)
{
    PNULLAUDIOSTREAMOUT pNullStrmOut = (PNULLAUDIOSTREAMOUT)pHstStrmOut;
    if (   pNullStrmOut
        && pNullStrmOut->pu8PlayBuffer)
    {
        RTMemFree(pNullStrmOut->pu8PlayBuffer);
    }
    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvHostNullAudioQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS        pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVHOSTNULLAUDIO pThis   = PDMINS_2_DATA(pDrvIns, PDRVHOSTNULLAUDIO);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIHOSTAUDIO, &pThis->IHostAudio);
    return NULL;
}

static DECLCALLBACK(void) drvHostNullAudioShutdown(PPDMIHOSTAUDIO pInterface)
{
    NOREF(pInterface);
}

/**
 * Constructs a Null audio driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvHostNullAudioConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    AssertPtrReturn(pDrvIns, VERR_INVALID_POINTER);
    /* pCfg is optional. */

    PDRVHOSTNULLAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTNULLAUDIO);
    LogRel(("Audio: Initializing NULL driver\n"));

    /*
     * Init the static parts.
     */
    pThis->pDrvIns                   = pDrvIns;
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface = drvHostNullAudioQueryInterface;
    /* IHostAudio */
    PDMAUDIO_IHOSTAUDIO_CALLBACKS(drvHostNullAudio);

    return VINF_SUCCESS;
}

/**
 * Char driver registration record.
 */
const PDMDRVREG g_DrvHostNullAudio =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "NullAudio",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "NULL audio host driver",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_AUDIO,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(DRVHOSTNULLAUDIO),
    /* pfnConstruct */
    drvHostNullAudioConstruct,
    /* pfnDestruct */
    NULL,
    /* pfnRelocate */
    NULL,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    NULL,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32EndVersion */
    PDM_DRVREG_VERSION
};

