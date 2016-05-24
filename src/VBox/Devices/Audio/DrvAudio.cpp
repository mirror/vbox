/* $Id$ */
/** @file
 * Intermediate audio driver header.
 *
 * @remarks Intermediate audio driver for connecting the audio device emulation
 *          with the host backend.
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
 * This code is based on: audio.c from QEMU AUDIO subsystem.
 *
 * QEMU Audio subsystem
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
#define LOG_GROUP LOG_GROUP_DRV_AUDIO
#include <VBox/log.h>
#include <VBox/vmm/pdm.h>
#include <VBox/err.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/pdmaudioifs.h>

#include <iprt/alloc.h>
#include <iprt/asm-math.h>
#include <iprt/assert.h>
#include <iprt/circbuf.h>
#include <iprt/string.h>
#include <iprt/uuid.h>

#include "VBoxDD.h"

#include <ctype.h>
#include <stdlib.h>

#include "DrvAudio.h"
#include "AudioMixBuffer.h"

static DECLCALLBACK(int) drvAudioStreamDestroy(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream);
static int drvAudioStreamControlInternal(PDRVAUDIO pThis, PPDMAUDIOSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd);
static int drvAudioStreamDestroyInternalBackend(PDRVAUDIO pThis, PPDMAUDIOSTREAM pHstStream);
static int drvAudioStreamDestroyInternal(PDRVAUDIO pThis, PPDMAUDIOSTREAM pStream);

#ifndef VBOX_AUDIO_TESTCASE
static PDMAUDIOFMT drvAudioGetConfFormat(PCFGMNODE pCfgHandle, const char *pszKey,
                                         PDMAUDIOFMT enmDefault, bool *pfDefault)
{
    if (   pCfgHandle == NULL
        || pszKey == NULL)
    {
        *pfDefault = true;
        return enmDefault;
    }

    char *pszValue = NULL;
    int rc = CFGMR3QueryStringAlloc(pCfgHandle, pszKey, &pszValue);
    if (RT_FAILURE(rc))
    {
        *pfDefault = true;
        return enmDefault;
    }

    PDMAUDIOFMT fmt = DrvAudioHlpStrToAudFmt(pszValue);
    if (fmt == PDMAUDIOFMT_INVALID)
    {
         *pfDefault = true;
        return enmDefault;
    }

    *pfDefault = false;
    return fmt;
}

static int drvAudioGetConfInt(PCFGMNODE pCfgHandle, const char *pszKey,
                              int iDefault, bool *pfDefault)
{

    if (   pCfgHandle == NULL
        || pszKey == NULL)
    {
        *pfDefault = true;
        return iDefault;
    }

    uint64_t u64Data = 0;
    int rc = CFGMR3QueryInteger(pCfgHandle, pszKey, &u64Data);
    if (RT_FAILURE(rc))
    {
        *pfDefault = true;
        return iDefault;

    }

    *pfDefault = false;
    return u64Data;
}

static const char *drvAudioGetConfStr(PCFGMNODE pCfgHandle, const char *pszKey,
                                      const char *pszDefault, bool *pfDefault)
{
    if (   pCfgHandle == NULL
        || pszKey == NULL)
    {
        *pfDefault = true;
        return pszDefault;
    }

    char *pszValue = NULL;
    int rc = CFGMR3QueryStringAlloc(pCfgHandle, pszKey, &pszValue);
    if (RT_FAILURE(rc))
    {
        *pfDefault = true;
        return pszDefault;
    }

    *pfDefault = false;
    return pszValue;
}

inline PPDMAUDIOSTREAM drvAudioGetHostStream(PPDMAUDIOSTREAM pStream)
{
    AssertPtrReturn(pStream, NULL);

    PPDMAUDIOSTREAM pStreamHst = pStream->enmCtx == PDMAUDIOSTREAMCTX_HOST
                               ? pStream
                               : pStream->pPair;
    AssertPtr(pStreamHst);
    return pStreamHst;
}

static int drvAudioProcessOptions(PCFGMNODE pCfgHandle, const char *pszPrefix, audio_option *paOpts, size_t cOpts)
{
    AssertPtrReturn(pCfgHandle, VERR_INVALID_POINTER);
    AssertPtrReturn(pszPrefix,  VERR_INVALID_POINTER);
    /* oaOpts and cOpts are optional. */

    PCFGMNODE pCfgChildHandle = NULL;
    PCFGMNODE pCfgChildChildHandle = NULL;

   /* If pCfgHandle is NULL, let NULL be passed to get int and get string functions..
    * The getter function will return default values.
    */
    if (pCfgHandle != NULL)
    {
       /* If its audio general setting, need to traverse to one child node.
        * /Devices/ichac97/0/LUN#0/Config/Audio
        */
       if(!strncmp(pszPrefix, "AUDIO", 5)) /** @todo Use a \#define */
       {
            pCfgChildHandle = CFGMR3GetFirstChild(pCfgHandle);
            if(pCfgChildHandle)
                pCfgHandle = pCfgChildHandle;
        }
        else
        {
            /* If its driver specific configuration , then need to traverse two level deep child
             * child nodes. for eg. in case of DirectSoundConfiguration item
             * /Devices/ichac97/0/LUN#0/Config/Audio/DirectSoundConfig
             */
            pCfgChildHandle = CFGMR3GetFirstChild(pCfgHandle);
            if (pCfgChildHandle)
            {
                pCfgChildChildHandle = CFGMR3GetFirstChild(pCfgChildHandle);
                if (pCfgChildChildHandle)
                    pCfgHandle = pCfgChildChildHandle;
            }
        }
    }

    for (size_t i = 0; i < cOpts; i++)
    {
        audio_option *pOpt = &paOpts[i];
        if (!pOpt->valp)
        {
            LogFlowFunc(("Option value pointer for `%s' is not set\n", pOpt->name));
            continue;
        }

        bool fUseDefault;

        switch (pOpt->tag)
        {
            case AUD_OPT_BOOL:
            case AUD_OPT_INT:
            {
                int *intp = (int *)pOpt->valp;
                *intp = drvAudioGetConfInt(pCfgHandle, pOpt->name, *intp, &fUseDefault);

                break;
            }

            case AUD_OPT_FMT:
            {
                PDMAUDIOFMT *fmtp = (PDMAUDIOFMT *)pOpt->valp;
                *fmtp = drvAudioGetConfFormat(pCfgHandle, pOpt->name, *fmtp, &fUseDefault);

                break;
            }

            case AUD_OPT_STR:
            {
                const char **strp = (const char **)pOpt->valp;
                *strp = drvAudioGetConfStr(pCfgHandle, pOpt->name, *strp, &fUseDefault);

                break;
            }

            default:
                LogFlowFunc(("Bad value tag for option `%s' - %d\n", pOpt->name, pOpt->tag));
                fUseDefault = false;
                break;
        }

        if (!pOpt->overridenp)
            pOpt->overridenp = &pOpt->overriden;

        *pOpt->overridenp = !fUseDefault;
    }

    return VINF_SUCCESS;
}
#endif /* !VBOX_AUDIO_TESTCASE */

static DECLCALLBACK(int) drvAudioStreamControl(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);

    return drvAudioStreamControlInternal(pThis, pStream, enmStreamCmd);
}

static int drvAudioStreamControlInternal(PDRVAUDIO pThis, PPDMAUDIOSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    AssertPtrReturn(pThis,   VERR_INVALID_POINTER);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_FAILURE(rc))
        return rc;

    PPDMAUDIOSTREAM pHstStream = drvAudioGetHostStream(pStream);

    LogFlowFunc(("%s: enmStreamCmd=%ld\n", pHstStream->szName, enmStreamCmd));

    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
        {
            if (!(pHstStream->fStatus & PDMAUDIOSTRMSTS_FLAG_ENABLED))
            {
                rc = pThis->pHostDrvAudio->pfnStreamControl(pThis->pHostDrvAudio, pHstStream, PDMAUDIOSTREAMCMD_ENABLE);
                if (RT_SUCCESS(rc))
                {
                    Assert(!(pHstStream->fStatus & PDMAUDIOSTRMSTS_FLAG_PENDING_DISABLE));
                    pHstStream->fStatus |= PDMAUDIOSTRMSTS_FLAG_ENABLED;
                }
            }
            else
                rc = VINF_SUCCESS;

            break;
        }

        case PDMAUDIOSTREAMCMD_DISABLE:
        {
            if (pHstStream->fStatus & PDMAUDIOSTRMSTS_FLAG_ENABLED)
            {
                rc = pThis->pHostDrvAudio->pfnStreamControl(pThis->pHostDrvAudio, pHstStream, PDMAUDIOSTREAMCMD_DISABLE);
                if (RT_SUCCESS(rc))
                {
                    pHstStream->fStatus &= ~PDMAUDIOSTRMSTS_FLAG_ENABLED;
                    AudioMixBufClear(&pHstStream->MixBuf);
                }
            }
            else
                rc = VINF_SUCCESS;

            break;
        }

        case PDMAUDIOSTREAMCMD_PAUSE:
        {
            if (!(pHstStream->fStatus & PDMAUDIOSTRMSTS_FLAG_PAUSED))
            {
                rc = pThis->pHostDrvAudio->pfnStreamControl(pThis->pHostDrvAudio, pHstStream, PDMAUDIOSTREAMCMD_PAUSE);
                if (RT_SUCCESS(rc))
                    pHstStream->fStatus |= PDMAUDIOSTRMSTS_FLAG_PAUSED;
            }
            else
                rc = VINF_SUCCESS;

            break;
        }

        case PDMAUDIOSTREAMCMD_RESUME:
        {
            if (pHstStream->fStatus & PDMAUDIOSTRMSTS_FLAG_PAUSED)
            {
                rc = pThis->pHostDrvAudio->pfnStreamControl(pThis->pHostDrvAudio, pHstStream, PDMAUDIOSTREAMCMD_RESUME);
                if (RT_SUCCESS(rc))
                    pHstStream->fStatus &= ~PDMAUDIOSTRMSTS_FLAG_PAUSED;
            }
            else
                rc = VINF_SUCCESS;

            break;
        }

        default:
            AssertMsgFailed(("Command %ld not implemented\n", enmStreamCmd));
            rc = VERR_NOT_IMPLEMENTED;
            break;
    }

    if (RT_FAILURE(rc))
        LogFunc(("%s: Failed with %Rrc\n", pHstStream->szName, rc));

    int rc2 = RTCritSectLeave(&pThis->CritSect);
    if (RT_SUCCESS(rc))
        rc = rc2;

    return rc;
}

#if 1
/**
 * Writes VM audio output data from the guest stream into the host stream.
 * The attached host driver backend then will play out the audio in a
 * later step then.
 *
 * @return  IPRT status code.
 * @return  int
 * @param   pThis
 * @param   pGstStrmOut
 * @param   pvBuf
 * @param   cbBuf
 * @param   pcbWritten
 */
static DECLCALLBACK(int) drvAudioStreamWrite(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream,
                                             const void *pvBuf, uint32_t cbBuf, uint32_t *pcbWritten)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuf,      VERR_INVALID_POINTER);
    /* pcbWritten is optional. */

    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);

    if (!cbBuf)
    {
        if (pcbWritten)
            *pcbWritten = 0;
        return VINF_SUCCESS;
    }

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_FAILURE(rc))
        return rc;

    if (!pThis->pHostDrvAudio->pfnGetStatus(pThis->pHostDrvAudio, PDMAUDIODIR_OUT))
    {
        rc = RTCritSectLeave(&pThis->CritSect);
        AssertRC(rc);

        return VERR_NOT_AVAILABLE;
    }

    PPDMAUDIOSTREAM pHstStream = drvAudioGetHostStream(pStream);
    PPDMAUDIOSTREAM pGstStream = pHstStream->pPair;

    AssertMsg(pHstStream->fStatus & PDMAUDIOSTRMSTS_FLAG_ENABLED,
              ("Writing to disabled host output stream \"%s\" not possible\n", pHstStream->szName));

    if (!AudioMixBufFreeBytes(&pHstStream->MixBuf))
    {
        if (pcbWritten)
            *pcbWritten = 0;

        return RTCritSectLeave(&pThis->CritSect);
    }

    /*
     * First, write data from the device emulation into our
     * guest mixing buffer.
     */
    uint32_t cWritten;
    rc = AudioMixBufWriteCirc(&pGstStream->MixBuf, pvBuf, cbBuf, &cWritten);
    if (rc == VINF_BUFFER_OVERFLOW)
        LogRelMax(32, ("Audio: Lost audio samples from guest, expect stuttering audio output\n"));

    /* Host stream currently has no samples to play? */
    if (AudioMixBufAvail(&pHstStream->MixBuf) == 0)
    {
        /* Mix just written guest stream samples to the host immediately. */
        uint32_t cMixed;
        rc = AudioMixBufMixToParent(&pGstStream->MixBuf, cWritten, &cMixed);
        LogFlowFunc(("cMixed=%RU32\n", cMixed));
    }

#ifdef DEBUG_andy
    AssertRC(rc);
#endif

#if 0
    /*
     * Second, mix the guest mixing buffer with the host mixing
     * buffer so that the host backend can play the data lateron.
     */
    uint32_t cMixed;
    if (   RT_SUCCESS(rc)
        && cWritten)
    {
        rc = AudioMixBufMixToParent(&pGstStream->MixBuf, cWritten, &cMixed);
    }
    else
        cMixed = 0;

    if (RT_SUCCESS(rc))
    {
        /*
         * Return the number of samples which actually have been mixed
         * down to the parent, regardless how much samples were written
         * into the children buffer.
         */
        if (pcbWritten)
            *pcbWritten = AUDIOMIXBUF_S2B(&pGstStream->MixBuf, cMixed);
    }
#else
    if (RT_SUCCESS(rc))
    {
        if (pcbWritten)
            *pcbWritten = AUDIOMIXBUF_S2B(&pGstStream->MixBuf, cWritten);
    }
#endif

    LogFlowFunc(("%s -> %s: cbBuf=%RU32, cWritten=%RU32 (%RU32 bytes), rc=%Rrc\n",
                 pGstStream->szName, pHstStream->szName, cbBuf,
                 cWritten, AUDIOMIXBUF_S2B(&pHstStream->MixBuf, cWritten), rc));

    int rc2 = RTCritSectLeave(&pThis->CritSect);
    if (RT_SUCCESS(rc))
        rc = rc2;

    return rc;
}
#else
static DECLCALLBACK(int) drvAudioWrite(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOGSTSTRMOUT pGstStrmOut,
                                       const void *pvBuf, uint32_t cbBuf, uint32_t *pcbWritten)
{
    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    AssertPtrReturn(pGstStrmOut, VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuf,       VERR_INVALID_POINTER);
    AssertReturn(cbBuf,          VERR_INVALID_PARAMETER);
    /* pcbWritten is optional. */

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_FAILURE(rc))
        return rc;

    if (!pThis->pHostDrvAudio->pfnIsEnabled(pThis->pHostDrvAudio, PDMAUDIODIR_OUT))
    {
        rc = RTCritSectLeave(&pThis->CritSect);
        AssertRC(rc);

        return VERR_NOT_AVAILABLE;
    }

    PPDMAUDIOHSTSTRMOUT pHstStrmOut = pGstStrmOut->pHstStrmOut;
    AssertPtrReturn(pHstStrmOut, VERR_INVALID_POINTER);

    AssertMsg(pGstStrmOut->pHstStrmOut->fStatus & PDMAUDIOSTRMSTS_FLAG_ENABLED,
              ("Writing to disabled host output stream \"%s\" not possible\n",
              pHstStrmOut->MixBuf.pszName));

    if (!AudioMixBufFreeBytes(&pGstStrmOut->MixBuf))
    {
        if (pcbWritten)
            *pcbWritten = 0;

        return RTCritSectLeave(&pThis->CritSect);
    }

    /*
     * First, write data from the device emulation into our
     * guest mixing buffer.
     */
    uint32_t cWritten;
    //rc = AudioMixBufWriteAt(&pGstStrmOut->MixBuf, 0 /* Offset in samples */, pvBuf, cbBuf, &cWritten);
    rc = AudioMixBufWriteCirc(&pGstStrmOut->MixBuf, pvBuf, cbBuf, &cWritten);
    if (rc == VINF_BUFFER_OVERFLOW)
        LogRel(("Audio: Lost audio samples from guest, expect stuttering audio output\n"));

    /*
     * Second, mix the guest mixing buffer with the host mixing
     * buffer so that the host backend can play the data lateron.
     */
    uint32_t cMixed;
    if (   RT_SUCCESS(rc)
        && cWritten)
    {
        rc = AudioMixBufMixToParent(&pGstStrmOut->MixBuf, cWritten, &cMixed);
    }
    else
        cMixed = 0;

    if (RT_SUCCESS(rc))
    {
        /*
         * Return the number of samples which actually have been mixed
         * down to the parent, regardless how much samples were written
         * into the children buffer.
         */
        if (pcbWritten)
            *pcbWritten = AUDIOMIXBUF_S2B(&pGstStrmOut->MixBuf, cMixed);
    }

    LogFlowFunc(("%s -> %s: cbBuf=%RU32, cWritten=%RU32 (%RU32 bytes), cMixed=%RU32, rc=%Rrc\n",
                 pGstStrmOut->MixBuf.pszName, pHstStrmOut->MixBuf.pszName, cbBuf, cWritten,
                 AUDIOMIXBUF_S2B(&pGstStrmOut->MixBuf, cWritten), cMixed, rc));

    int rc2 = RTCritSectLeave(&pThis->CritSect);
    if (RT_SUCCESS(rc))
        rc = rc2;

    return rc;
}
#endif

static DECLCALLBACK(uint32_t) drvAudioStreamAddRef(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream)
{
   AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
   AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

   NOREF(pInterface);

   return ++pStream->cRefs;
}

static DECLCALLBACK(uint32_t) drvAudioStreamRelease(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream)
{
   AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
   AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

   NOREF(pInterface);

   Assert(pStream->cRefs);
   if (pStream->cRefs)
       pStream->cRefs--;

   return pStream->cRefs;
}

#if 1
static DECLCALLBACK(int) drvAudioStreamGetData(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream, uint32_t *pcData)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    /* pcData is optional. */

    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_FAILURE(rc))
        return rc;

    uint32_t cData = 0;

    PPDMAUDIOSTREAM pHstStream = drvAudioGetHostStream(pStream);
    PPDMAUDIOSTREAM pGstStream = pHstStream->pPair;

    do
    {
        if (!(pHstStream->fStatus & PDMAUDIOSTRMSTS_FLAG_ENABLED))
            break;

        if (pHstStream->enmDir == PDMAUDIODIR_IN)
        {
            /* Call the host backend to capture the audio input data. */
            rc = pThis->pHostDrvAudio->pfnStreamCapture(pThis->pHostDrvAudio, pHstStream, &cData);
            if (RT_FAILURE(rc))
                break;

            //cbData = AUDIOMIXBUF_S2B(&pStream->MixBuf, AudioMixBufMixed(&pStream->MixBuf)));

        }
        else /* Out */
        {
            uint32_t cSamplesMixed = 0;
            uint32_t cSamplesToMix = 0;

            uint32_t cSamplesLive = AudioMixBufAvail(&pHstStream->MixBuf);
            if (!cSamplesLive)
            {

            }

            cSamplesToMix = AudioMixBufAvail(&pGstStream->MixBuf);
            if (cSamplesToMix)
                rc = AudioMixBufMixToParent(&pGstStream->MixBuf, cSamplesToMix, &cSamplesMixed);

            /* Has this stream marked as disabled but there still were guest streams relying
             * on it? CheckpHstStreamif this stream now can be closed and do so, if possible. */
            if (   (pHstStream->fStatus & PDMAUDIOSTRMSTS_FLAG_PENDING_DISABLE)
                && !cSamplesToMix)
            {
                /* Stop playing the current (pending) stream. */
                int rc2 = drvAudioStreamControlInternal(pThis, pHstStream, PDMAUDIOSTREAMCMD_DISABLE);
                if (RT_SUCCESS(rc2))
                {
                    pHstStream->fStatus &= ~PDMAUDIOSTRMSTS_FLAG_PENDING_DISABLE;

                    LogFunc(("%s: Disabling stream\n", pHstStream->szName));
                }
                else
                    LogFunc(("%s: Backend vetoed against closing output stream, rc=%Rrc\n", pHstStream->szName, rc2));

                break;
            }

            LogFlowFunc(("%s: cSamplesLive=%RU32, cSamplesToMix=%RU32, cSamplesMixed=%RU32\n",
                         pHstStream->szName, cSamplesLive, cSamplesToMix, cSamplesMixed));

            /* Return live samples to play. */
            cData = cSamplesLive;
        }

    } while (0);

#ifdef DEBUG_andy
    LogFlowFunc(("%s: cData=%RU32\n", pHstStream->szName, cData));
#endif

    if (pcData)
        *pcData = cData;

    int rc2 = RTCritSectLeave(&pThis->CritSect);
    if (RT_SUCCESS(rc))
        rc = rc2;

    if (RT_FAILURE(rc))
        LogFlowFuncLeaveRC(rc);

    return rc;
}
#else
static DECLCALLBACK(int) drvAudioGetDataIn(PPDMIAUDIOCONNECTOR pInterface, uint32_t *pcbAvailIn)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    /* pcbAvailIn is optional. */

    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_FAILURE(rc))
        return rc;

    uint32_t cbAvailIn = 0;

    PPDMAUDIOHSTSTRMIN pHstStrmIn = NULL;
    while ((pHstStrmIn = drvAudioFindAnyHstIn(pThis, pHstStrmIn)))
    {
        /* Disabled? Skip it! */
        if (!(pHstStrmIn->fStatus & PDMAUDIOSTRMSTS_FLAG_ENABLED))
            continue;

        /* Call the host backend to capture the audio input data. */
        uint32_t cSamplesCaptured;
        int rc2 = pThis->pHostDrvAudio->pfnCaptureIn(pThis->pHostDrvAudio, pHstStrmIn,
                                                     &cSamplesCaptured);
        if (RT_FAILURE(rc2))
            continue;

        PPDMAUDIOGSTSTRMIN pGstStrmIn = pHstStrmIn->pGstStrmIn;
        AssertPtrBreak(pGstStrmIn);

        if (pGstStrmIn->State.fActive)
        {
            cbAvailIn = RT_MAX(cbAvailIn, AUDIOMIXBUF_S2B(&pHstStrmIn->MixBuf,
                                                          AudioMixBufMixed(&pHstStrmIn->MixBuf)));
#ifdef DEBUG_andy
            LogFlowFunc(("\t[%s] cbAvailIn=%RU32\n", pHstStrmIn->MixBuf.pszName, cbAvailIn));
#endif
        }
    }

    if (RT_SUCCESS(rc))
    {
        if (pcbAvailIn)
            *pcbAvailIn = cbAvailIn;
    }

    int rc2 = RTCritSectLeave(&pThis->CritSect);
    if (RT_SUCCESS(rc))
        rc = rc2;

    if (RT_FAILURE(rc))
        LogFlowFuncLeaveRC(rc);

    return rc;
}

static DECLCALLBACK(int) drvAudioGetDataOut(PPDMIAUDIOCONNECTOR pInterface, uint32_t *pcbFreeOut, uint32_t *pcSamplesLive)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    /* pcbFreeOut is optional. */
    /* pcSamplesLive is optional. */

    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_FAILURE(rc))
        return rc;

    static uint64_t s_tsLast = 0;


    uint64_t s_tsDelta = RTTimeNanoTS() - s_tsLast;
    LogFlowFunc(("delta=%RU64 (%RU64ms) -> ", s_tsDelta, s_tsDelta / 1000 / 1000));

    #if 0
    PPDMAUDIOHSTSTRMOUT pHstStrmOut = drvAudioHstFindAnyEnabledOut(pThis, NULL);
    uint64_t ns = s_tsDelta / pHstStrmOut->Props.uHz;



    uint32_t cSamplesMin  = (cTicksElapsed / pSink->PCMProps.uHz) * pSink->PCMProps.cChannels;
    #endif

    s_tsLast = RTTimeNanoTS();

    /*
     * Playback.
     */
    uint32_t cSamplesLive = 0;
    uint32_t cbFreeOut    = UINT32_MAX;

    PPDMAUDIOHSTSTRMOUT pHstStrmOut = NULL;
    while ((pHstStrmOut = drvAudioHstFindAnyEnabledOut(pThis, pHstStrmOut)))
    {
        cSamplesLive = AudioMixBufAvail(&pHstStrmOut->MixBuf);

        /* Has this stream marked as disabled but there still were guest streams relying
         * on it? Check if this stream now can be closed and do so, if possible. */
        if (   (pHstStrmOut->fStatus & PDMAUDIOSTRMSTS_FLAG_PENDING_DISABLE)
            && !cSamplesLive)
        {
            /* Stop playing the current (pending) stream. */
            int rc2 = drvAudioControlHstOut(pThis, pHstStrmOut, PDMAUDIOSTREAMCMD_DISABLE);
            if (RT_SUCCESS(rc2))
            {
                pHstStrmOut->fStatus &= ~PDMAUDIOSTRMSTS_FLAG_PENDING_DISABLE;

                LogFunc(("[%s] Disabling stream\n", pHstStrmOut->MixBuf.pszName));
            }
            else
                LogFunc(("[%s] Backend vetoed against closing output stream, rc=%Rrc\n", pHstStrmOut->MixBuf.pszName, rc2));

            continue;
        }

        LogFlowFunc(("[%s] cSamplesLive=%RU32\n", pHstStrmOut->MixBuf.pszName, cSamplesLive));

        /*
         * No live samples to play at the moment?
         *
         * Tell the device emulation for each connected guest stream how many
         * bytes are free so that the device emulation can continue writing data to
         * these streams.
         */
        PPDMAUDIOGSTSTRMOUT pGstStrmOut;
        uint32_t cbFree2 = UINT32_MAX;
        RTListForEach(&pHstStrmOut->lstGstStrmOut, pGstStrmOut, PDMAUDIOGSTSTRMOUT, Node)
        {
            if (pGstStrmOut->State.fActive)
            {
                /* Tell the sound device emulation how many samples are free
                 * so that it can start writing PCM data to us. */
                cbFree2 = RT_MIN(cbFree2, AUDIOMIXBUF_S2B_RATIO(&pGstStrmOut->MixBuf,
                                                                AudioMixBufFree(&pGstStrmOut->MixBuf)));
#ifdef DEBUG_andy
                LogFlowFunc(("\t[%s] cbFreeOut=%RU32\n", pGstStrmOut->MixBuf.pszName, cbFree2));
#endif
            }
        }

        cbFreeOut = RT_MIN(cbFreeOut, cbFree2);
    }

    if (RT_SUCCESS(rc))
    {
        if (cbFreeOut == UINT32_MAX)
            cbFreeOut = 0;

        if (pcbFreeOut)
            *pcbFreeOut = cbFreeOut;

        if (pcSamplesLive)
            *pcSamplesLive = cSamplesLive;
    }

    int rc2 = RTCritSectLeave(&pThis->CritSect);
    if (RT_SUCCESS(rc))
        rc = rc2;

    if (RT_FAILURE(rc))
        LogFlowFuncLeaveRC(rc);

    return rc;
}
#endif

#if 1
static DECLCALLBACK(int) drvAudioStreamPlay(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream, uint32_t *pcSamplesPlayed)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    /* pcSamplesPlayed is optional. */

    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_FAILURE(rc))
        return rc;

    /* Backend output (temporarily) disabled / unavailable? */
    if (pThis->pHostDrvAudio->pfnGetStatus(pThis->pHostDrvAudio, PDMAUDIODIR_OUT) != PDMAUDIOBACKENDSTS_RUNNING)
    {
        rc = pThis->pHostDrvAudio->pfnGetConfig(pThis->pHostDrvAudio, &pThis->BackendCfg);
        AssertRC(rc);

        if (   !pThis->BackendCfg.cSinks
            || !pThis->BackendCfg.cMaxStreamsOut)
        {
            int rc2 = RTCritSectLeave(&pThis->CritSect);
            AssertRC(rc2);

            return VERR_NOT_AVAILABLE;
        }
    }

    PPDMAUDIOSTREAM pHstStream = drvAudioGetHostStream(pStream);

    uint32_t cSamplesPlayed = 0;
    rc = pThis->pHostDrvAudio->pfnStreamPlay(pThis->pHostDrvAudio, pHstStream, &cSamplesPlayed);
    if (RT_FAILURE(rc))
    {
        int rc3 = pThis->pHostDrvAudio->pfnStreamControl(pThis->pHostDrvAudio, pHstStream, PDMAUDIOSTREAMCMD_DISABLE);
        AssertRC(rc3);
    }

    LogFlowFunc(("[%s] cSamplesPlayed=%RU32, rc=%Rrc\n", pStream->szName, cSamplesPlayed, rc));

    PPDMAUDIOSTREAM pGstStream = pHstStream->pPair;
    if (pGstStream)
    {
        bool fIsEmpty = AudioMixBufIsEmpty(&pGstStream->MixBuf);


    }

#if 0
        if (pStream->pPair)
        {
            bool fIsEmpty = AudioMixBufIsEmpty(&pStream->pPair->MixBuf);

            if (   !pPair->State.fActive
                && fIsEmpty)
                continue;

            if ()
            {
                pGstStrmOut->State.fEmpty = true;
                fNeedsCleanup |= !pGstStrmOut->State.fActive;
            }
        }

        if (fNeedsCleanup)
        {
            RTListForEach(&pHstStrmOut->lstGstStrmOut, pGstStrmOut, PDMAUDIOGSTSTRMOUT, Node)
            {
                if (!pGstStrmOut->State.fActive)
                    drvAudioDestroyGstOut(pThis, pGstStrmOut);
            }
        }
#endif

    if (RT_SUCCESS(rc))
    {
        if (pcSamplesPlayed)
            *pcSamplesPlayed = cSamplesPlayed;
    }

    int rc2 = RTCritSectLeave(&pThis->CritSect);
    if (RT_SUCCESS(rc))
        rc = rc2;

    if (RT_FAILURE(rc))
        LogFlowFuncLeaveRC(rc);

    return rc;
}
#else
static DECLCALLBACK(int) drvAudioPlayOut(PPDMIAUDIOCONNECTOR pInterface, uint32_t *pcSamplesPlayed)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    /* pcSamplesPlayed is optional. */

    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_FAILURE(rc))
        return rc;

    /* Backend output (temporarily) disabled / unavailable? */
    if (!pThis->pHostDrvAudio->pfnIsEnabled(pThis->pHostDrvAudio, PDMAUDIODIR_OUT))
    {
        rc = pThis->pHostDrvAudio->pfnGetConf(pThis->pHostDrvAudio, &pThis->BackendCfg);
        AssertRC(rc);

        if (   !pThis->BackendCfg.cSinks
            || !pThis->BackendCfg.cMaxStreamsOut)
        {
            int rc2 = RTCritSectLeave(&pThis->CritSect);
            AssertRC(rc2);

            return VERR_NOT_AVAILABLE;
        }
    }

    /*
     * Process all enabled host output streams.
     */
    uint32_t            cSamplesPlayedMax = 0;
    PPDMAUDIOHSTSTRMOUT pHstStrmOut       = NULL;
    while ((pHstStrmOut = drvAudioHstFindAnyEnabledOut(pThis, pHstStrmOut)))
    {
#if 0
        uint32_t cStreamsLive;
        uint32_t cSamplesLive = drvAudioHstOutSamplesLive(pHstStrmOut, &cStreamsLive);
        if (!cStreamsLive)
            cSamplesLive = 0;

        /* Has this stream marked as disabled but there still were guest streams relying
         * on it? Check if this stream now can be closed and do so, if possible. */
        if (   pHstStrmOut->fPendingDisable
            && !cStreamsLive)
        {
            /* Stop playing the current (pending) stream. */
            int rc2 = pThis->pHostDrvAudio->pfnControlOut(pThis->pHostDrvAudio, pHstStrmOut,
                                                          PDMAUDIOSTREAMCMD_DISABLE);
            if (RT_SUCCESS(rc2))
            {
                pHstStrmOut->fEnabled        = false;
                pHstStrmOut->fPendingDisable = false;

                LogFunc(("\t%p: Disabling stream\n", pHstStrmOut));
            }
            else
                LogFunc(("\t%p: Backend vetoed against closing output stream, rc=%Rrc\n",
                         pHstStrmOut, rc2));

            continue;
        }
#endif
        uint32_t cSamplesPlayed = 0;
        int rc2 = pThis->pHostDrvAudio->pfnPlayOut(pThis->pHostDrvAudio, pHstStrmOut, &cSamplesPlayed);
        if (RT_FAILURE(rc2))
        {
            int rc3 = pThis->pHostDrvAudio->pfnControlOut(pThis->pHostDrvAudio, pHstStrmOut, PDMAUDIOSTREAMCMD_DISABLE);
            AssertRC(rc3);
        }
        else
            cSamplesPlayedMax = RT_MAX(cSamplesPlayed, cSamplesPlayedMax);

        LogFlowFunc(("\t[%s] cSamplesPlayed=%RU32, cSamplesPlayedMax=%RU32, rc=%Rrc\n",
                     pHstStrmOut->MixBuf.pszName, cSamplesPlayed, cSamplesPlayedMax, rc2));

        bool fNeedsCleanup = false;

        PPDMAUDIOGSTSTRMOUT pGstStrmOut;
        RTListForEach(&pHstStrmOut->lstGstStrmOut, pGstStrmOut, PDMAUDIOGSTSTRMOUT, Node)
        {
            if (   !pGstStrmOut->State.fActive
                && pGstStrmOut->State.fEmpty)
                continue;

            if (AudioMixBufIsEmpty(&pGstStrmOut->MixBuf))
            {
                pGstStrmOut->State.fEmpty = true;
                fNeedsCleanup |= !pGstStrmOut->State.fActive;
            }
        }

        if (fNeedsCleanup)
        {
            RTListForEach(&pHstStrmOut->lstGstStrmOut, pGstStrmOut, PDMAUDIOGSTSTRMOUT, Node)
            {
                if (!pGstStrmOut->State.fActive)
                    drvAudioDestroyGstOut(pThis, pGstStrmOut);
            }
        }
    }

    if (RT_SUCCESS(rc))
    {
        if (pcSamplesPlayed)
            *pcSamplesPlayed = cSamplesPlayedMax;
    }

    int rc2 = RTCritSectLeave(&pThis->CritSect);
    if (RT_SUCCESS(rc))
        rc = rc2;

    if (RT_FAILURE(rc))
        LogFlowFuncLeaveRC(rc);

    return rc;
}
#endif

#ifdef VBOX_WITH_AUDIO_CALLBACKS
static PPDMAUDIOCALLBACK drvAudioCallbackDuplicate(PPDMAUDIOCALLBACK pCB)
{
    PPDMAUDIOCALLBACK pCBCopy = (PPDMAUDIOCALLBACK)RTMemDup((void *)pCB, sizeof(PDMAUDIOCALLBACK));
    if (!pCBCopy)
        return NULL;

    if (pCB->pvCtx)
    {
        pCBCopy->pvCtx = RTMemDup(pCB->pvCtx, pCB->cbCtx);
        if (!pCBCopy->pvCtx)
        {
            RTMemFree(pCBCopy);
            return NULL;
        }

        pCBCopy->cbCtx = pCB->cbCtx;
    }

    return pCBCopy;
}

static void drvAudioCallbackDestroy(PPDMAUDIOCALLBACK pCB)
{
    if (!pCB)
        return;

    RTListNodeRemove(&pCB->Node);
    if (pCB->pvCtx)
    {
        Assert(pCB->cbCtx);
        RTMemFree(pCB->pvCtx);
    }
    RTMemFree(pCB);
}

static DECLCALLBACK(int) drvAudioRegisterCallbacks(PPDMIAUDIOCONNECTOR pInterface,
                                                   PPDMAUDIOCALLBACK paCallbacks, size_t cCallbacks)
{
    AssertPtrReturn(pInterface,  VERR_INVALID_POINTER);
    AssertPtrReturn(paCallbacks, VERR_INVALID_POINTER);
    AssertReturn(cCallbacks,     VERR_INVALID_PARAMETER);

    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_FAILURE(rc))
        return rc;

    for (size_t i = 0; i < cCallbacks; i++)
    {
        PPDMAUDIOCALLBACK pCB = drvAudioCallbackDuplicate(&paCallbacks[i]);
        if (!pCB)
        {
            rc = VERR_NO_MEMORY;
            break;
        }

        switch (pCB->enmType)
        {
            case PDMAUDIOCALLBACKTYPE_INPUT:
                RTListAppend(&pThis->lstCBIn, &pCB->Node);
                break;

            case PDMAUDIOCALLBACKTYPE_OUTPUT:
                RTListAppend(&pThis->lstCBOut, &pCB->Node);
                break;

            default:
                AssertMsgFailed(("Not supported\n"));
                break;
        }
    }

    /** @todo Undo allocations on error. */

    int rc2 = RTCritSectLeave(&pThis->CritSect);
    if (RT_SUCCESS(rc))
        rc = rc2;

    return rc;
}

static DECLCALLBACK(int) drvAudioCallback(PPDMIAUDIOCONNECTOR pInterface, PDMAUDIOCALLBACKTYPE enmType,
                                          void *pvUser, size_t cbUser)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pvUser,     VERR_INVALID_POINTER);
    AssertReturn(cbUser,        VERR_INVALID_PARAMETER);

    PDRVAUDIO     pThis       = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);
    PRTLISTANCHOR pListAnchor = NULL;

    switch (enmType)
    {
        case PDMAUDIOCALLBACKTYPE_INPUT:
            pListAnchor = &pThis->lstCBIn;
            break;

        case PDMAUDIOCALLBACKTYPE_OUTPUT:
            pListAnchor = &pThis->lstCBOut;
            break;

        default:
            AssertMsgFailed(("Not supported\n"));
            break;
    }

    if (pListAnchor)
    {
        PPDMAUDIOCALLBACK pCB;
        RTListForEach(pListAnchor, pCB, PDMAUDIOCALLBACK, Node)
        {
            Assert(pCB->enmType == enmType);
            pCB->pfnCallback(enmType, pCB->pvCtx, pCB->cbCtx, pvUser, cbUser);
        }
    }

    return VINF_SUCCESS;
}
#endif

static int drvAudioHostInit(PCFGMNODE pCfgHandle, PDRVAUDIO pThis)
{
    /* pCfgHandle is optional. */
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    NOREF(pCfgHandle);

    LogFlowFuncEnter();

    AssertPtr(pThis->pHostDrvAudio);
    int rc = pThis->pHostDrvAudio->pfnInit(pThis->pHostDrvAudio);
    if (RT_FAILURE(rc))
    {
        LogFlowFunc(("Initialization of lower driver failed with rc=%Rrc\n", rc));
        return rc;
    }

    /* Get the configuration data from backend. */
    rc = pThis->pHostDrvAudio->pfnGetConfig(pThis->pHostDrvAudio, &pThis->BackendCfg);
    if (RT_FAILURE(rc))
    {
        LogFlowFunc(("Getting backend configuration failed with rc=%Rrc\n", rc));
        return rc;
    }

    pThis->cStreamsFreeIn  = 0;
    pThis->cStreamsFreeOut = 0;

    if (pThis->BackendCfg.cSinks)
    {
        Assert(pThis->BackendCfg.cbStreamOut);

        pThis->cStreamsFreeOut = pThis->BackendCfg.cMaxStreamsOut;
    }

    if (pThis->BackendCfg.cSources)
    {
        Assert(pThis->BackendCfg.cbStreamIn);

        pThis->cStreamsFreeIn = pThis->BackendCfg.cMaxStreamsIn;
    }

    LogFlowFunc(("cStreamsFreeIn=%RU8, cStreamsFreeOut=%RU8\n", pThis->cStreamsFreeIn, pThis->cStreamsFreeOut));

    LogRel2(("Audio: Host audio backend supports %RU32 input streams and %RU32 output streams at once\n",
             /* Clamp for logging. Unlimited streams are defined by UINT32_MAX. */
             RT_MIN(64, pThis->cStreamsFreeIn), RT_MIN(64, pThis->cStreamsFreeOut)));

    LogFlowFuncLeave();
    return VINF_SUCCESS;
}

static void drvAudioStateHandler(PPDMDRVINS pDrvIns, PDMAUDIOSTREAMCMD enmCmd)
{
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);
    PDRVAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVAUDIO);

    LogFlowFunc(("enmCmd=%ld\n", enmCmd));

    if (!pThis->pHostDrvAudio)
        return;

    PPDMAUDIOSTREAM pStream;
    RTListForEach(&pThis->lstStreams, pStream, PDMAUDIOSTREAM, Node)
        drvAudioStreamControlInternal(pThis, pStream, enmCmd);
}

static DECLCALLBACK(int) drvAudioInit(PCFGMNODE pCfgHandle, PPDMDRVINS pDrvIns)
{
    AssertPtrReturn(pCfgHandle, VERR_INVALID_POINTER);
    AssertPtrReturn(pDrvIns, VERR_INVALID_POINTER);

    PDRVAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVAUDIO);
    LogFlowFunc(("pThis=%p, pDrvIns=%p\n", pThis, pDrvIns));

    RTListInit(&pThis->lstStreams);
#ifdef VBOX_WITH_AUDIO_CALLBACKS
    RTListInit(&pThis->lstCBIn);
    RTListInit(&pThis->lstCBOut);
#endif

    int rc = RTCritSectInit(&pThis->CritSect);

    /** @todo Add audio driver options. */

    /*
     * If everything went well, initialize the lower driver.
     */
    if (RT_SUCCESS(rc))
        rc = drvAudioHostInit(pCfgHandle, pThis);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

#if 1
static DECLCALLBACK(int) drvAudioStreamRead(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream,
                                            void *pvBuf, uint32_t cbBuf, uint32_t *pcbRead)
{
    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    if (!pStream)
        return VERR_NOT_AVAILABLE;

    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    AssertReturn(cbBuf,    VERR_INVALID_PARAMETER);
    /* pcbWritten is optional. */

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_FAILURE(rc))
        return rc;

    if (pThis->pHostDrvAudio->pfnGetStatus(pThis->pHostDrvAudio, PDMAUDIODIR_IN) != PDMAUDIOBACKENDSTS_RUNNING)
    {
        if (pcbRead)
            *pcbRead = 0;

        return RTCritSectLeave(&pThis->CritSect);
    }

    PPDMAUDIOSTREAM pHstStream = drvAudioGetHostStream(pStream);

    AssertMsg(pHstStream->fStatus & PDMAUDIOSTRMSTS_FLAG_ENABLED,
              ("Reading from disabled host input stream '%s' not possible\n", pHstStream->szName));

    /*
     * Read from the parent buffer (that is, the guest buffer) which
     * should have the audio data in the format the guest needs.
     */
    uint32_t cRead;
    rc = AudioMixBufReadCirc(&pStream->MixBuf, pvBuf, cbBuf, &cRead);
    if (RT_SUCCESS(rc))
    {
        AudioMixBufFinish(&pStream->MixBuf, cRead);

        if (pcbRead)
            *pcbRead = AUDIOMIXBUF_S2B(&pStream->MixBuf, cRead);
    }

    LogFlowFunc(("cRead=%RU32 (%RU32 bytes), rc=%Rrc\n",
                 cRead, AUDIOMIXBUF_S2B(&pStream->MixBuf, cRead), rc));

    int rc2 = RTCritSectLeave(&pThis->CritSect);
    if (RT_SUCCESS(rc))
        rc = rc2;

    return rc;
}
#else
static DECLCALLBACK(int) drvAudioRead(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOGSTSTRMIN pGstStrmIn,
                                      void *pvBuf, uint32_t cbBuf, uint32_t *pcbRead)
{
    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    if (!pGstStrmIn)
        return VERR_NOT_AVAILABLE;

    AssertPtrReturn(pvBuf,      VERR_INVALID_POINTER);
    AssertReturn(cbBuf,         VERR_INVALID_PARAMETER);
    /* pcbWritten is optional. */

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_FAILURE(rc))
        return rc;

    if (!pThis->pHostDrvAudio->pfnIsEnabled(pThis->pHostDrvAudio, PDMAUDIODIR_IN))
    {
        if (pcbRead)
            *pcbRead = 0;

        return RTCritSectLeave(&pThis->CritSect);
    }

    PPDMAUDIOHSTSTRMIN pHstStrmIn = pGstStrmIn->pHstStrmIn;
    AssertPtrReturn(pHstStrmIn, VERR_INVALID_POINTER);

    AssertMsg(pGstStrmIn->pHstStrmIn->fStatus & PDMAUDIOSTRMSTS_FLAG_ENABLED,
              ("Reading from disabled host input stream \"%s\" not possible\n", pGstStrmIn->MixBuf.pszName));

    /*
     * Read from the parent buffer (that is, the guest buffer) which
     * should have the audio data in the format the guest needs.
     */
    uint32_t cRead;
    rc = AudioMixBufReadCirc(&pGstStrmIn->MixBuf, pvBuf, cbBuf, &cRead);
    if (RT_SUCCESS(rc))
    {
        AudioMixBufFinish(&pGstStrmIn->MixBuf, cRead);

        if (pcbRead)
            *pcbRead = AUDIOMIXBUF_S2B(&pGstStrmIn->MixBuf, cRead);
    }

    LogFlowFunc(("cRead=%RU32 (%RU32 bytes), rc=%Rrc\n",
                 cRead, AUDIOMIXBUF_S2B(&pGstStrmIn->MixBuf, cRead), rc));

    int rc2 = RTCritSectLeave(&pThis->CritSect);
    if (RT_SUCCESS(rc))
        rc = rc2;

    return rc;
}
#endif

#if 0
static DECLCALLBACK(int) drvAudioEnableOut(PPDMIAUDIOCONNECTOR pInterface,
                                           PPDMAUDIOGSTSTRMOUT pGstStrmOut, bool fEnable)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);

    if (!pGstStrmOut)
        return VERR_NOT_AVAILABLE;

    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);

    int rc = VINF_SUCCESS;

    PPDMAUDIOHSTSTRMOUT pHstStrmOut = pGstStrmOut->pHstStrmOut;
    AssertPtr(pHstStrmOut);

    if (fEnable)
    {
        /* Is a pending disable outstanding? Then disable first. */
        if (pHstStrmOut->fStatus & PDMAUDIOSTRMSTS_FLAG_PENDING_DISABLE)
        {
            rc = drvAudioControlHstOut(pThis, pHstStrmOut, PDMAUDIOSTREAMCMD_DISABLE);
            if (RT_SUCCESS(rc))
                pHstStrmOut->fStatus &= ~PDMAUDIOSTRMSTS_FLAG_PENDING_DISABLE;
        }

        if (RT_SUCCESS(rc))
            rc = drvAudioControlHstOut(pThis, pHstStrmOut, PDMAUDIOSTREAMCMD_ENABLE);

        if (RT_SUCCESS(rc))
            pGstStrmOut->State.fActive = fEnable;
    }
    else /* Disable */
    {
        if (pHstStrmOut->fStatus & PDMAUDIOSTRMSTS_FLAG_ENABLED)
        {
            size_t cGstStrmsActive = 0;

            /*
             * Check if there are any active guest streams assigned
             * to this host stream which still are being marked as active.
             *
             * In that case we have to defer closing the host stream and
             * wait until all guest streams have been finished.
             */
            PPDMAUDIOGSTSTRMOUT pIter;
            RTListForEach(&pHstStrmOut->lstGstStrmOut, pIter, PDMAUDIOGSTSTRMOUT, Node)
            {
                if (pIter->State.fActive)
                {
                    cGstStrmsActive++;
                    break; /* At least one assigned & active guest stream is enough. */
                }
            }

            /* Do we need to defer closing the host stream? */
            if (cGstStrmsActive)
            {
                LogFlowFunc(("Closing stream deferred: %zu guest stream(s) active\n", cGstStrmsActive));
                pHstStrmOut->fStatus |= PDMAUDIOSTRMSTS_FLAG_PENDING_DISABLE;

                rc = VERR_AUDIO_STREAM_PENDING_DISABLE;
            }
            else
            {
                rc = drvAudioControlHstOut(pThis, pHstStrmOut, PDMAUDIOSTREAMCMD_DISABLE);
                if (RT_SUCCESS(rc))
                    pGstStrmOut->State.fActive = fEnable;
            }
        }
    }

    LogFlowFunc(("%s: fEnable=%RTbool, fStatus=0x%x, rc=%Rrc\n",
                 pGstStrmOut->MixBuf.pszName, fEnable, pHstStrmOut->fStatus, rc));

    return rc;
}
#endif

static DECLCALLBACK(int) drvAudioStreamCreate(PPDMIAUDIOCONNECTOR pInterface,
                                              PPDMAUDIOSTREAMCFG pCfgHost, PPDMAUDIOSTREAMCFG pCfgGuest,
                                              PPDMAUDIOSTREAM *ppStream)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgHost,   VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgGuest,  VERR_INVALID_POINTER);
    AssertPtrReturn(ppStream,   VERR_INVALID_POINTER);

    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_FAILURE(rc))
        return rc;

    LogFlowFunc(("Host=%s, Guest=%s\n", pCfgHost->szName, pCfgGuest->szName));
#ifdef DEBUG
    DrvAudioHlpStreamCfgPrint(pCfgHost);
    DrvAudioHlpStreamCfgPrint(pCfgGuest);
#endif

    /*
     * The guest stream always will get the audio stream configuration told
     * by the device emulation (which in turn was/could be set by the guest OS).
     */
    PPDMAUDIOSTREAM pGstStrm = NULL;

    /** @todo Docs! */
    PPDMAUDIOSTREAM pHstStrm = NULL;

#define RC_BREAK(x) { rc = x; break; }

    do
    {
        if (   !DrvAudioHlpStreamCfgIsValid(pCfgHost)
            || !DrvAudioHlpStreamCfgIsValid(pCfgGuest))
        {
            RC_BREAK(VERR_INVALID_PARAMETER);
        }

        /* Make sure that both configurations actually intend the same thing. */
        if (pCfgHost->enmDir != pCfgGuest->enmDir)
        {
            AssertMsgFailed(("Stream configuration directions do not match\n"));
            RC_BREAK(VERR_INVALID_PARAMETER);
        }

        /* Note: cbHstStrm will contain sizeof(PDMAUDIOSTREAM) + additional data
         *       which the host backend will need. */
        size_t cbHstStrm;
        if (pCfgHost->enmDir == PDMAUDIODIR_IN)
        {
            if (!pThis->cStreamsFreeIn)
            {
                LogFlowFunc(("No more input streams free to use, bailing out\n"));
                RC_BREAK(VERR_AUDIO_NO_FREE_INPUT_STREAMS);
            }

            /* Validate backend configuration. */
            if (!pThis->BackendCfg.cbStreamIn)
            {
                LogFlowFunc(("Backend input configuration not valid, bailing out\n"));
                RC_BREAK(VERR_INVALID_PARAMETER);
            }

            cbHstStrm = pThis->BackendCfg.cbStreamIn;
        }
        else /* Out */
        {
            if (!pThis->cStreamsFreeOut)
            {
                LogFlowFunc(("Maximum number of host output streams reached\n"));
                RC_BREAK(VERR_NO_MORE_HANDLES); /** @todo Fudge! */
            }

            /* Validate backend configuration. */
            if (!pThis->BackendCfg.cbStreamOut)
            {
                LogFlowFunc(("Backend output configuration invalid, bailing out\n"));
                RC_BREAK(VERR_INVALID_PARAMETER);
            }

            cbHstStrm = pThis->BackendCfg.cbStreamOut;
        }

        pHstStrm = (PPDMAUDIOSTREAM)RTMemAllocZ(cbHstStrm);
        AssertPtrBreakStmt(pHstStrm, rc = VERR_NO_MEMORY);

        RTListAppend(&pThis->lstStreams, &pHstStrm->Node);

        pGstStrm = (PPDMAUDIOSTREAM)RTMemAllocZ(sizeof(PDMAUDIOSTREAM));
        AssertPtrBreakStmt(pGstStrm, rc = VERR_NO_MEMORY);

        RTListAppend(&pThis->lstStreams, &pGstStrm->Node);

        /*
         * Create host stream.
         */

        RTStrPrintf(pHstStrm->szName, RT_ELEMENTS(pHstStrm->szName), "%s (Host)",
                    strlen(pCfgHost->szName) ? pCfgHost->szName : "<Untitled>");

        /* Note: Direction is always from child -> parent. */
        uint32_t cSamples = 0;
        rc = pThis->pHostDrvAudio->pfnStreamCreate(pThis->pHostDrvAudio, pHstStrm, pCfgHost, &cSamples);
        if (RT_FAILURE(rc))
        {
            LogFlowFunc(("Initializing host backend failed with rc=%Rrc\n", rc));
            break;
        }

        pHstStrm->enmCtx   = PDMAUDIOSTREAMCTX_HOST;
        pHstStrm->enmDir   = pCfgHost->enmDir;
        pHstStrm->fStatus |= PDMAUDIOSTRMSTS_FLAG_INITIALIZED;
        pHstStrm->pPair    = pGstStrm;

        rc = DrvAudioHlpStreamCfgToProps(pCfgHost, &pHstStrm->Props);
        AssertRCBreak(rc);

        rc = AudioMixBufInit(&pHstStrm->MixBuf, pHstStrm->szName, &pHstStrm->Props, cSamples);
        AssertRCBreak(rc);

        /*
         * Create guest stream.
         */

        RTStrPrintf(pGstStrm->szName, RT_ELEMENTS(pGstStrm->szName), "%s (Guest)",
                    strlen(pCfgGuest->szName) ? pCfgGuest->szName : "<Untitled>");

        rc = DrvAudioHlpStreamCfgToProps(pCfgGuest, &pGstStrm->Props);
        AssertRCBreak(rc);

        rc = AudioMixBufInit(&pGstStrm->MixBuf, pGstStrm->szName, &pGstStrm->Props, AudioMixBufSize(&pHstStrm->MixBuf));
        if (RT_SUCCESS(rc))
        {
            if (pCfgGuest->enmDir == PDMAUDIODIR_IN)
            {
                rc = AudioMixBufLinkTo(&pHstStrm->MixBuf, &pGstStrm->MixBuf);
            }
            else
            {
                rc = AudioMixBufLinkTo(&pGstStrm->MixBuf, &pHstStrm->MixBuf);
            }
        }

        pGstStrm->enmCtx   = PDMAUDIOSTREAMCTX_GUEST;
        pGstStrm->enmDir   = pCfgGuest->enmDir;
        pGstStrm->fStatus |= PDMAUDIOSTRMSTS_FLAG_INITIALIZED;
        pGstStrm->pPair    = pHstStrm;

        AssertRCBreak(rc);

    } while (0);

#undef RC_BREAK

    if (RT_FAILURE(rc))
    {
        if (   pHstStrm
            && pHstStrm->enmCtx == PDMAUDIOSTREAMCTX_HOST)
        {
            if (pHstStrm->fStatus & PDMAUDIOSTRMSTS_FLAG_INITIALIZED)
            {
                rc = drvAudioStreamControlInternal(pThis, pHstStrm, PDMAUDIOSTREAMCMD_DISABLE);
                if (RT_SUCCESS(rc))
                {
                    rc = pThis->pHostDrvAudio->pfnStreamDestroy(pThis->pHostDrvAudio, pHstStrm);
                    if (RT_SUCCESS(rc))
                        pHstStrm->fStatus &= ~PDMAUDIOSTRMSTS_FLAG_INITIALIZED;
                }
            }

            AssertMsgRC(rc, ("Host stream '%s' failed to uninit in backend: %Rrc\n",  pHstStrm->szName, rc));
        }

        drvAudioStreamDestroyInternal(pThis, pGstStrm);
        pGstStrm = NULL;

        drvAudioStreamDestroyInternal(pThis, pHstStrm);
        pHstStrm = NULL;
    }
    else
    {
        /* Set initial reference counts. */
        pGstStrm->cRefs = 1;
        pHstStrm->cRefs = 1;

        if (pCfgHost->enmDir == PDMAUDIODIR_IN)
        {
            Assert(pThis->cStreamsFreeIn);
            pThis->cStreamsFreeIn--;
        }
        else /* Out */
        {
            Assert(pThis->cStreamsFreeOut);
            pThis->cStreamsFreeOut--;
        }

        *ppStream = pGstStrm;
    }

    int rc2 = RTCritSectLeave(&pThis->CritSect);
    if (RT_SUCCESS(rc))
        rc = rc2;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

#if 1
static DECLCALLBACK(int) drvAudioGetConfig(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOBACKENDCFG pCfg)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg,       VERR_INVALID_POINTER);

    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_FAILURE(rc))
        return rc;

    rc = pThis->pHostDrvAudio->pfnGetConfig(pThis->pHostDrvAudio, pCfg);

    int rc2 = RTCritSectLeave(&pThis->CritSect);
    if (RT_SUCCESS(rc))
        rc = rc2;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(PDMAUDIOSTRMSTS) drvAudioStreamGetStatus(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream)
{
    AssertPtrReturn(pInterface, false);
    /* pStream is optional. */

    if (!pStream)
        return PDMAUDIOSTRMSTS_FLAG_NONE;

    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);

    int rc2 = RTCritSectEnter(&pThis->CritSect);
    AssertRC(rc2);

    PPDMAUDIOSTREAM pHstStream = drvAudioGetHostStream(pStream);
    PDMAUDIOSTRMSTS strmSts    = pHstStream->fStatus;

    LogFlowFunc(("%s: strmSts=0x%x\n", pHstStream->szName, strmSts));
    rc2 = RTCritSectLeave(&pThis->CritSect);
    AssertRC(rc2);

    return strmSts;
}
#endif

static DECLCALLBACK(int) drvAudioStreamDestroy(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream)
{
    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);

    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertRC(rc);

    PDMAUDIODIR enmDir = pStream->enmDir;

    LogFlowFunc(("%s: cRefs=%RU32\n", pStream->szName, pStream->cRefs));
    if (pStream->cRefs > 1)
        rc = VERR_WRONG_ORDER;

    PPDMAUDIOSTREAM pHstStream = drvAudioGetHostStream(pStream);
    if (   RT_SUCCESS(rc)
        && pHstStream->cRefs == 1)
    {
        rc = drvAudioStreamDestroyInternalBackend(pThis, pHstStream);
        if (RT_SUCCESS(rc))
        {
            if (pStream->pPair)
            {
                rc = drvAudioStreamDestroyInternal(pThis, pStream->pPair);
                pStream->pPair = NULL;
            }

            if (RT_SUCCESS(rc))
                rc = drvAudioStreamDestroyInternal(pThis, pStream);

            if (RT_SUCCESS(rc))
            {
                if (enmDir == PDMAUDIODIR_IN)
                {
                    pThis->cStreamsFreeIn++;
                }
                else /* Out */
                {
                    pThis->cStreamsFreeOut++;
                }
            }
        }
    }
    else
        rc = VERR_WRONG_ORDER;

    int rc2 = RTCritSectLeave(&pThis->CritSect);
    if (RT_SUCCESS(rc))
        rc = rc2;

    if (RT_FAILURE(rc))
        LogFlowFunc(("Failed with %Rrc\n", rc));

    return rc;
}

static int drvAudioStreamDestroyInternalBackend(PDRVAUDIO pThis, PPDMAUDIOSTREAM pHstStream)
{
    AssertPtrReturn(pThis,      VERR_INVALID_POINTER);
    AssertPtrReturn(pHstStream, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;

    LogFlowFunc(("%s: fStatus=0x%x\n", pHstStream->szName, pHstStream->fStatus));

    AssertMsg(pHstStream->enmCtx == PDMAUDIOSTREAMCTX_HOST,
              ("Stream '%s' is not a host stream and therefore has no backend\n", pHstStream->szName));

    if (pHstStream->fStatus & PDMAUDIOSTRMSTS_FLAG_INITIALIZED)
    {
        rc = drvAudioStreamControlInternal(pThis, pHstStream, PDMAUDIOSTREAMCMD_DISABLE);
        if (RT_SUCCESS(rc))
        {
            rc = pThis->pHostDrvAudio->pfnStreamDestroy(pThis->pHostDrvAudio, pHstStream);
            if (RT_SUCCESS(rc))
                pHstStream->fStatus &= ~PDMAUDIOSTRMSTS_FLAG_INITIALIZED;
        }

        AssertMsgRC(rc, ("Host stream '%s' failed to uninit in backend: %Rrc\n",  pHstStream->szName, rc));
    }
    else
        rc = VINF_SUCCESS;

    LogFlowFunc(("%s: Returning %Rrc\n", pHstStream->szName, rc));
    return rc;
}

static int drvAudioStreamDestroyInternal(PDRVAUDIO pThis, PPDMAUDIOSTREAM pStream)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    /* pStream is optional. */

    if (!pStream)
        return VINF_SUCCESS;

    LogFlowFunc(("%s: cRefs=%RU32\n", pStream->szName, pStream->cRefs));

    int rc = VINF_SUCCESS;

    if (pStream->cRefs <= 1)
    {
        /* Unlink from pair. */
        if (pStream->pPair)
        {
            pStream->pPair->pPair = NULL;
            pStream->pPair = NULL;
        }

        /* Destroy mixing buffer. */
        AudioMixBufDestroy(&pStream->MixBuf);

        /* Remove from list. */
        RTListNodeRemove(&pStream->Node);

        RTMemFree(pStream);
        pStream = NULL;
    }
    else /* More than our own reference left? Bail out. */
        rc = VERR_WRONG_ORDER;

    if (RT_FAILURE(rc))
        LogFunc(("Failed with %Rrc\n", rc));

    return rc;
}

/********************************************************************/

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvAudioQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    LogFlowFunc(("pInterface=%p, pszIID=%s\n", pInterface, pszIID));

    PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVAUDIO  pThis   = PDMINS_2_DATA(pDrvIns, PDRVAUDIO);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIAUDIOCONNECTOR, &pThis->IAudioConnector);

    return NULL;
}

/**
 * Power Off notification.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) drvAudioPowerOff(PPDMDRVINS pDrvIns)
{
    PDRVAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVAUDIO);

    LogFlowFuncEnter();

    /*
     * Second, destroy all audio streams.
     */
    PPDMAUDIOSTREAM pStream;
    RTListForEach(&pThis->lstStreams, pStream, PDMAUDIOSTREAM, Node)
    {
        if (pStream->enmCtx == PDMAUDIOSTREAMCTX_HOST)
            drvAudioStreamDestroyInternalBackend(pThis, pStream);
    }

    /*
     * Last call for the driver below us.
     * Let it know that we reached end of life.
     */
    if (pThis->pHostDrvAudio->pfnShutdown)
        pThis->pHostDrvAudio->pfnShutdown(pThis->pHostDrvAudio);

    pThis->pHostDrvAudio = NULL;

    LogFlowFuncLeave();
}

/**
 * Constructs an audio driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvAudioConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle, uint32_t fFlags)
{
    LogFlowFunc(("pDrvIns=%#p, pCfgHandle=%#p, fFlags=%x\n", pDrvIns, pCfgHandle, fFlags));

    PDRVAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVAUDIO);
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);

    /*
     * Init the static parts.
     */
    pThis->pDrvIns                              = pDrvIns;
    /* IBase. */
    pDrvIns->IBase.pfnQueryInterface            = drvAudioQueryInterface;
    /* IAudioConnector. */
    pThis->IAudioConnector.pfnGetConfig         = drvAudioGetConfig;
    pThis->IAudioConnector.pfnStreamCreate      = drvAudioStreamCreate;
    pThis->IAudioConnector.pfnStreamDestroy     = drvAudioStreamDestroy;
    pThis->IAudioConnector.pfnStreamAddRef      = drvAudioStreamAddRef;
    pThis->IAudioConnector.pfnStreamRelease     = drvAudioStreamRelease;
    pThis->IAudioConnector.pfnStreamControl     = drvAudioStreamControl;
    pThis->IAudioConnector.pfnStreamRead        = drvAudioStreamRead;
    pThis->IAudioConnector.pfnStreamWrite       = drvAudioStreamWrite;
    pThis->IAudioConnector.pfnStreamGetData     = drvAudioStreamGetData;
    pThis->IAudioConnector.pfnStreamGetStatus   = drvAudioStreamGetStatus;
    pThis->IAudioConnector.pfnStreamPlay        = drvAudioStreamPlay;
#ifdef VBOX_WITH_AUDIO_CALLBACKS
    pThis->IAudioConnector.pfnRegisterCallbacks = drvAudioRegisterCallbacks;
    pThis->IAudioConnector.pfnCallback          = drvAudioCallback;
#endif

    /*
     * Attach driver below and query its connector interface.
     */
    PPDMIBASE pDownBase;
    int rc = PDMDrvHlpAttach(pDrvIns, fFlags, &pDownBase);
    if (RT_FAILURE(rc))
    {
        LogRel(("Audio: Failed to attach to driver %p below (flags=0x%x), rc=%Rrc\n",
                pDrvIns, fFlags, rc));
        return rc;
    }

    pThis->pHostDrvAudio = PDMIBASE_QUERY_INTERFACE(pDownBase, PDMIHOSTAUDIO);
    if (!pThis->pHostDrvAudio)
    {
        LogRel(("Audio: Failed to query interface for underlying host driver\n"));
        return PDMDRV_SET_ERROR(pDrvIns, VERR_PDM_MISSING_INTERFACE_BELOW,
                                N_("Host audio backend missing or invalid"));
    }

    rc = drvAudioInit(pCfgHandle, pDrvIns);
    if (RT_SUCCESS(rc))
    {
        pThis->fTerminate = false;
        pThis->pDrvIns    = pDrvIns;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Destructs an audio driver instance.
 *
 * @copydoc FNPDMDRVDESTRUCT
 */
static DECLCALLBACK(void) drvAudioDestruct(PPDMDRVINS pDrvIns)
{
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);
    PDRVAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVAUDIO);

    LogFlowFuncEnter();

    int rc2 = RTCritSectEnter(&pThis->CritSect);
    AssertRC(rc2);

    /*
     * Note: No calls here to the driver below us anymore,
     *       as PDM already has destroyed it.
     *       If you need to call something from the host driver,
     *       do this in drvAudioPowerOff() instead.
     */
    PPDMAUDIOSTREAM pStream, pStreamNext;
    RTListForEachSafe(&pThis->lstStreams, pStream, pStreamNext, PDMAUDIOSTREAM, Node)
        drvAudioStreamDestroyInternal(pThis, pStream);

    /* Sanity. */
    Assert(RTListIsEmpty(&pThis->lstStreams));

#ifdef VBOX_WITH_AUDIO_CALLBACKS
    /*
     * Destroy callbacks, if any.
     */
    PPDMAUDIOCALLBACK pCB, pCBNext;
    RTListForEachSafe(&pThis->lstCBIn, pCB, pCBNext, PDMAUDIOCALLBACK, Node)
        drvAudioCallbackDestroy(pCB);

    RTListForEachSafe(&pThis->lstCBOut, pCB, pCBNext, PDMAUDIOCALLBACK, Node)
        drvAudioCallbackDestroy(pCB);
#endif

    rc2 = RTCritSectLeave(&pThis->CritSect);
    AssertRC(rc2);

    rc2 = RTCritSectDelete(&pThis->CritSect);
    AssertRC(rc2);

    LogFlowFuncLeave();
}

/**
 * Suspend notification.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) drvAudioSuspend(PPDMDRVINS pDrvIns)
{
    drvAudioStateHandler(pDrvIns, PDMAUDIOSTREAMCMD_PAUSE);
}

/**
 * Resume notification.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) drvAudioResume(PPDMDRVINS pDrvIns)
{
    drvAudioStateHandler(pDrvIns, PDMAUDIOSTREAMCMD_RESUME);
}

/**
 * Audio driver registration record.
 */
const PDMDRVREG g_DrvAUDIO =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "AUDIO",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Audio connector driver",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass */
    PDM_DRVREG_CLASS_AUDIO,
    /* cMaxInstances */
    2,
    /* cbInstance */
    sizeof(DRVAUDIO),
    /* pfnConstruct */
    drvAudioConstruct,
    /* pfnDestruct */
    drvAudioDestruct,
    /* pfnRelocate */
    NULL,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    NULL,
    /* pfnSuspend */
    drvAudioSuspend,
    /* pfnResume */
    drvAudioResume,
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnPowerOff */
    drvAudioPowerOff,
    /* pfnSoftReset */
    NULL,
    /* u32EndVersion */
    PDM_DRVREG_VERSION
};

