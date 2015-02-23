/* $Id$ */
/** @file
 * Intermediate audio driver header.
 *
 * @remarks Intermediate audio driver having audio device as one of the sink and
 *          host backend as other.
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

#ifdef LOG_GROUP
# undef LOG_GROUP
#endif
#define LOG_GROUP LOG_GROUP_DEV_AUDIO
#include <VBox/log.h>

#include "VBoxDD.h"
#include "vl_vbox.h"

#include <ctype.h>
#include <stdlib.h>

#include "DrvAudio.h"
#include "AudioMixBuffer.h"

static int drvAudioDestroyGstIn(PDRVAUDIO pThis, PPDMAUDIOGSTSTRMIN pGstStrmIn);

static int drvAudioAllocHstIn(PDRVAUDIO pThis, PPDMAUDIOSTREAMCFG pCfg, PDMAUDIORECSOURCE enmRecSource, PPDMAUDIOHSTSTRMIN *ppHstStrmIn);
static int drvAudioDestroyHstIn(PDRVAUDIO pThis, PPDMAUDIOHSTSTRMIN pHstStrmIn);

volume_t nominal_volume =
{
    0,
    INT_MAX,
    INT_MAX
};

volume_t sum_out_volume =
{
    0,
    INT_MAX,
    INT_MAX
};

volume_t pcm_in_volume =
{
    0,
    INT_MAX,
    INT_MAX
};

int drvAudioAddHstOut(PDRVAUDIO pThis, const char *pszName, PPDMAUDIOSTREAMCFG pCfg, PPDMAUDIOHSTSTRMOUT *ppHstStrmOut)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg, VERR_INVALID_POINTER);

    PPDMAUDIOHSTSTRMOUT pHstStrmOut;

    int rc;
    if (   conf.fixed_out.enabled /** @todo Get rid of these settings! */
        && conf.fixed_out.greedy)
    {
        rc = drvAudioAllocHstOut(pThis, pszName, pCfg, &pHstStrmOut);
    }
    else
        rc = VERR_NOT_FOUND;

    if (RT_FAILURE(rc))
    {
        pHstStrmOut = drvAudioFindSpecificOut(pThis, NULL, pCfg);
        if (!pHstStrmOut)
        {
            rc = drvAudioAllocHstOut(pThis, pszName, pCfg, &pHstStrmOut);
            if (RT_FAILURE(rc))
                pHstStrmOut = drvAudioFindAnyHstOut(pThis, NULL /* pHstStrmOut */);
        }

        rc = pHstStrmOut ? VINF_SUCCESS : rc;
    }

    if (RT_SUCCESS(rc))
        *ppHstStrmOut = pHstStrmOut;

    return rc;
}

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

    PDMAUDIOFMT fmt = drvAudioHlpStringToFormat(pszValue);
    if (fmt == AUD_FMT_INVALID)
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

static int drvAudioProcessOptions(PCFGMNODE pCfgHandle, const char *pszPrefix, struct audio_option *opt)
{
    AssertPtrReturn(pCfgHandle, VERR_INVALID_POINTER);
    AssertPtrReturn(pszPrefix, VERR_INVALID_POINTER);
    AssertPtrReturn(opt, VERR_INVALID_POINTER);

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
       if(!strncmp(pszPrefix, "AUDIO", 5)) /** @todo Use a #define */
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

    for (; opt->name; opt++)
    {
        LogFlowFunc(("Option value pointer for `%s' is not set\n",
                     opt->name));
        if (!opt->valp) {
            LogFlowFunc(("Option value pointer for `%s' is not set\n",
                   opt->name));
            continue;
        }

        bool fUseDefault;

        switch (opt->tag)
        {
            case AUD_OPT_BOOL:
            case AUD_OPT_INT:
            {
                int *intp = (int *)opt->valp;
                *intp = drvAudioGetConfInt(pCfgHandle, opt->name, *intp, &fUseDefault);

                break;
            }

            case AUD_OPT_FMT:
            {
                PDMAUDIOFMT *fmtp = (PDMAUDIOFMT *)opt->valp;
                *fmtp = drvAudioGetConfFormat(pCfgHandle, opt->name, *fmtp, &fUseDefault);

                break;
            }

            case AUD_OPT_STR:
            {
                const char **strp = (const char **)opt->valp;
                *strp = drvAudioGetConfStr(pCfgHandle, opt->name, *strp, &fUseDefault);

                break;
            }

            default:
                LogFlowFunc(("Bad value tag for option `%s' - %d\n", opt->name, opt->tag));
                fUseDefault = false;
                break;
        }

        if (!opt->overridenp)
            opt->overridenp = &opt->overriden;

        *opt->overridenp = !fUseDefault;
    }

    return VINF_SUCCESS;
}

static bool drvAudioStreamCfgIsValid(PPDMAUDIOSTREAMCFG pCfg)
{
    bool fValid = (   pCfg->cChannels == 1
                   || pCfg->cChannels == 2); /* Either stereo (2) or mono (1), per stream. */

    fValid |= (   pCfg->enmEndianness == PDMAUDIOENDIANESS_LITTLE
               || pCfg->enmEndianness == PDMAUDIOENDIANESS_BIG);

    if (fValid)
    {
        switch (pCfg->enmFormat)
        {
            case AUD_FMT_S8:
            case AUD_FMT_U8:
            case AUD_FMT_S16:
            case AUD_FMT_U16:
            case AUD_FMT_S32:
            case AUD_FMT_U32:
                break;
            default:
                fValid = false;
                break;
        }
    }

    /** @todo Check for defined frequencies supported. */
    fValid |= pCfg->uHz > 0;

#ifdef DEBUG
    drvAudioStreamCfgPrint(pCfg);
#endif

    LogFlowFunc(("pCfg=%p, fValid=%RTbool\n", pCfg, fValid));
    return fValid;
}

void audio_pcm_info_clear_buf(PPDMPCMPROPS pPCMInfo, void *pvBuf, int len)
{
    if (!len)
        return;

    if (pPCMInfo->fSigned)
    {
        memset (pvBuf, 0, len << pPCMInfo->cShift);
    }
    else
    {
        switch (pPCMInfo->cBits)
        {

        case 8:
            memset (pvBuf, 0x80, len << pPCMInfo->cShift);
            break;

        case 16:
            {
                int i;
                uint16_t *p = (uint16_t *)pvBuf;
                int shift = pPCMInfo->cChannels - 1;
                short s = INT16_MAX;

                if (pPCMInfo->fSwapEndian)
                    s = bswap16(s);

                for (i = 0; i < len << shift; i++)
                    p[i] = s;
            }
            break;

        case 32:
            {
                int i;
                uint32_t *p = (uint32_t *)pvBuf;
                int shift = pPCMInfo->cChannels - 1;
                int32_t s = INT32_MAX;

                if (pPCMInfo->fSwapEndian)
                    s = bswap32(s);

                for (i = 0; i < len << shift; i++)
                    p[i] = s;
            }
            break;

        default:
            LogFlowFunc(("audio_pcm_info_clear_buf: invalid bits %d\n", pPCMInfo->cBits));
            break;
        }
    }
}

int drvAudioDestroyHstOut(PDRVAUDIO pThis, PPDMAUDIOHSTSTRMOUT pHstStrmOut)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(pHstStrmOut, VERR_INVALID_POINTER);

    LogFlowFunc(("%s\n", pHstStrmOut->MixBuf.pszName));

    int rc;
    if (RTListIsEmpty(&pHstStrmOut->lstGstStrmOut))
    {
        rc = pThis->pHostDrvAudio->pfnFiniOut(pThis->pHostDrvAudio, pHstStrmOut);
        if (RT_SUCCESS(rc))
        {
            drvAudioHstOutFreeRes(pHstStrmOut);

            /* Remove from driver instance list. */
            RTListNodeRemove(&pHstStrmOut->Node);

            RTMemFree(pHstStrmOut);
            pThis->cFreeOutputStreams++;
            return VINF_SUCCESS;
        }
    }
    else
        rc = VERR_ACCESS_DENIED;

    LogFlowFunc(("[%s] Still is being used, rc=%Rrc\n", pHstStrmOut->MixBuf.pszName, rc));
    return rc;
}

int drvAudioDestroyGstOut(PDRVAUDIO pThis, PPDMAUDIOGSTSTRMOUT pGstStrmOut)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    if (pGstStrmOut)
    {
        drvAudioGstOutFreeRes(pGstStrmOut);

        if (pGstStrmOut->pHstStrmOut)
        {
            /* Unregister from parent first. */
            RTListNodeRemove(&pGstStrmOut->Node);

            /* Try destroying the associated host output stream. This could
             * be skipped if there are other guest output streams with this
             * host stream. */
            drvAudioDestroyHstOut(pThis, pGstStrmOut->pHstStrmOut);
        }

        RTMemFree(pGstStrmOut);
    }

    return VINF_SUCCESS;
}

PPDMAUDIOHSTSTRMIN drvAudioFindNextHstIn(PDRVAUDIO pThis, PPDMAUDIOHSTSTRMIN pHstStrmIn)
{
    if (pHstStrmIn)
    {
        if (RTListNodeIsLast(&pThis->lstHstStrmIn, &pHstStrmIn->Node))
            return NULL;

        return RTListNodeGetNext(&pHstStrmIn->Node, PDMAUDIOHSTSTRMIN, Node);
    }

    return RTListGetFirst(&pThis->lstHstStrmIn, PDMAUDIOHSTSTRMIN, Node);
}

PPDMAUDIOHSTSTRMIN drvAudioFindNextEnabledHstIn(PDRVAUDIO pThis, PPDMAUDIOHSTSTRMIN pHstStrmIn)
{
    while ((pHstStrmIn = drvAudioFindNextHstIn(pThis, pHstStrmIn)))
        if (pHstStrmIn->fEnabled)
            return pHstStrmIn;

    return NULL;
}

PPDMAUDIOHSTSTRMIN drvAudioFindNextEqHstIn(PDRVAUDIO pThis, PPDMAUDIOHSTSTRMIN pHstStrmIn,
                                           PPDMAUDIOSTREAMCFG pCfg)
{
    while ((pHstStrmIn = drvAudioFindNextHstIn(pThis, pHstStrmIn)))
        if (drvAudioPCMPropsAreEqual(&pHstStrmIn->Props, pCfg))
            return pHstStrmIn;

    return NULL;
}

static int drvAudioHstInAdd(PDRVAUDIO pThis, PPDMAUDIOSTREAMCFG pCfg, PDMAUDIORECSOURCE enmRecSource,
                            PPDMAUDIOHSTSTRMIN *ppHstStrmIn)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg, VERR_INVALID_POINTER);
    AssertPtrReturn(ppHstStrmIn, VERR_INVALID_POINTER);

    PPDMAUDIOHSTSTRMIN pHstStrmIn;
    int rc = drvAudioAllocHstIn(pThis, pCfg, enmRecSource, &pHstStrmIn);
    if (RT_SUCCESS(rc))
        *ppHstStrmIn = pHstStrmIn;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int drvAudioGstOutInit(PPDMAUDIOGSTSTRMOUT pGstStrmOut, PPDMAUDIOHSTSTRMOUT pHostStrmOut,
                       const char *pszName, PPDMAUDIOSTREAMCFG pCfg)
{
    int rc = drvAudioStreamCfgToProps(pCfg, &pGstStrmOut->Props);
    if (RT_SUCCESS(rc))
    {
        rc = audioMixBufInit(&pGstStrmOut->MixBuf, pszName, &pGstStrmOut->Props,
                             audioMixBufSize(&pHostStrmOut->MixBuf));
        if (RT_SUCCESS(rc))
            rc = audioMixBufLinkTo(&pGstStrmOut->MixBuf, &pHostStrmOut->MixBuf);

        if (RT_SUCCESS(rc))
        {
            pGstStrmOut->State.fActive = false;
            pGstStrmOut->State.fEmpty  = true;

            pGstStrmOut->State.pszName = RTStrDup(pszName);
            if (!pGstStrmOut->State.pszName)
                return VERR_NO_MEMORY;

            pGstStrmOut->pHstStrmOut = pHostStrmOut;
        }
    }

    LogFlowFunc(("pszName=%s, rc=%Rrc\n", pszName, rc));
    return rc;
}

int drvAudioAllocHstOut(PDRVAUDIO pThis, const char *pszName, PPDMAUDIOSTREAMCFG pCfg, PPDMAUDIOHSTSTRMOUT *ppHstStrmOut)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg, VERR_INVALID_POINTER);

    if (!pThis->cFreeOutputStreams)
    {
        LogFlowFunc(("Maximum number of host output streams reached\n"));
        return VERR_NO_MORE_HANDLES;
    }

    /* Validate backend configuration. */
    if (!pThis->BackendCfg.cbStreamOut)
    {
        LogFlowFunc(("Backend output configuration not valid, bailing out\n"));
        return VERR_INVALID_PARAMETER;
    }

    PPDMAUDIOHSTSTRMOUT pHstStrmOut =
        (PPDMAUDIOHSTSTRMOUT)RTMemAllocZ(pThis->BackendCfg.cbStreamOut);
    if (!pHstStrmOut)
    {
        LogFlowFunc(("Error allocating host output stream with %zu bytes\n",
                     pThis->BackendCfg.cbStreamOut));
        return VERR_NO_MEMORY;
    }

    int rc;
    bool fInitialized = false;

    do
    {
        RTListInit(&pHstStrmOut->lstGstStrmOut);

        uint32_t cSamples;
        rc = pThis->pHostDrvAudio->pfnInitOut(pThis->pHostDrvAudio, pHstStrmOut, pCfg,
                                              &cSamples);
        if (RT_FAILURE(rc))
        {
            LogFlowFunc(("Initializing host backend failed with rc=%Rrc\n", rc));
            break;
        }

        fInitialized = true;

        rc = audioMixBufInit(&pHstStrmOut->MixBuf, pszName, &pHstStrmOut->Props, cSamples);
        if (RT_SUCCESS(rc))
        {
            RTListPrepend(&pThis->lstHstStrmOut, &pHstStrmOut->Node);
            pThis->cFreeOutputStreams--;
        }

    } while (0);

    if (RT_FAILURE(rc))
    {
        if (fInitialized)
        {
            int rc2 = pThis->pHostDrvAudio->pfnFiniOut(pThis->pHostDrvAudio,
                                                       pHstStrmOut);
            AssertRC(rc2);
        }

        drvAudioHstOutFreeRes(pHstStrmOut);
        RTMemFree(pHstStrmOut);
    }
    else
        *ppHstStrmOut = pHstStrmOut;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int drvAudioCreateStreamPairOut(PDRVAUDIO pThis, const char *pszName,
                                PPDMAUDIOSTREAMCFG pCfg, PPDMAUDIOGSTSTRMOUT *ppGstStrmOut)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg, VERR_INVALID_POINTER);

    PPDMAUDIOSTREAMCFG pThisCfg;
    if (conf.fixed_out.enabled)
        pThisCfg = &conf.fixed_out.settings;
    else
        pThisCfg = pCfg;

    AssertPtrReturn(pThisCfg, VERR_INVALID_POINTER);

    LogFlowFunc(("Using fixed audio output settings: %RTbool\n",
                 RT_BOOL(conf.fixed_out.enabled)));

    PPDMAUDIOGSTSTRMOUT pGstStrmOut =
        (PPDMAUDIOGSTSTRMOUT)RTMemAllocZ(sizeof(PDMAUDIOGSTSTRMOUT));
    if (!pGstStrmOut)
    {
        LogFlowFunc(("Failed to allocate memory for guest output stream \"%s\"\n", pszName));
        return VERR_NO_MEMORY;
    }

    PPDMAUDIOHSTSTRMOUT pHstStrmOut;
    int rc = drvAudioAddHstOut(pThis, pszName, pThisCfg, &pHstStrmOut);
    if (RT_FAILURE(rc))
    {
        LogFlowFunc(("Error adding host output stream \"%s\", rc=%Rrc\n", pszName, rc));

        RTMemFree(pGstStrmOut);
        return rc;
    }

    rc = drvAudioGstOutInit(pGstStrmOut, pHstStrmOut, pszName, pThisCfg);
    if (RT_SUCCESS(rc))
    {
        RTListPrepend(&pHstStrmOut->lstGstStrmOut, &pGstStrmOut->Node);

        if (ppGstStrmOut)
            *ppGstStrmOut = pGstStrmOut;
    }

    if (RT_FAILURE(rc))
        drvAudioDestroyGstOut(pThis, pGstStrmOut);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int drvAudioCreateStreamPairIn(PDRVAUDIO pThis, const char *pszName, PDMAUDIORECSOURCE enmRecSource,
                                      PPDMAUDIOSTREAMCFG pCfg, PPDMAUDIOGSTSTRMIN *ppGstStrmIn)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);

    PPDMAUDIOSTREAMCFG pThisCfg;
    if (conf.fixed_in.enabled)
    {
        pThisCfg = &conf.fixed_in.settings;
        LogFlowFunc(("Using fixed audio settings\n"));
    }
    else
        pThisCfg = pCfg;
    AssertPtrReturn(pThisCfg, VERR_INVALID_POINTER);

    PPDMAUDIOGSTSTRMIN pGstStrmIn =
        (PPDMAUDIOGSTSTRMIN)RTMemAllocZ(sizeof(PDMAUDIOGSTSTRMIN));
    if (!pGstStrmIn)
        return VERR_NO_MEMORY;

    PPDMAUDIOHSTSTRMIN pHstStrmIn;
    int rc = drvAudioHstInAdd(pThis, pThisCfg, enmRecSource, &pHstStrmIn);
    if (RT_FAILURE(rc))
    {
        LogFunc(("Failed to add host audio input stream \"%s\", rc=%Rrc\n",
                 pszName, rc));

        RTMemFree(pGstStrmIn);
        return rc;
    }

    rc = drvAudioGstInInit(pGstStrmIn, pHstStrmIn, pszName, pThisCfg);
    if (RT_SUCCESS(rc))
    {
        pHstStrmIn->pGstStrmIn = pGstStrmIn;

        if (ppGstStrmIn)
            *ppGstStrmIn = pGstStrmIn;
    }
    else
        drvAudioDestroyGstIn(pThis, pGstStrmIn);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Initializes a guest input stream.
 *
 * @return  IPRT status code.
 * @param   pGstStrmIn          Pointer to guest stream to initialize.
 * @param   pHstStrmIn          Pointer to host input stream to associate this guest
 *                              stream with.
 * @param   pszName             Pointer to stream name to use for this stream.
 * @param   pCfg                Pointer to stream configuration to use.
 */
int drvAudioGstInInit(PPDMAUDIOGSTSTRMIN pGstStrmIn, PPDMAUDIOHSTSTRMIN pHstStrmIn,
                      const char *pszName, PPDMAUDIOSTREAMCFG pCfg)
{
    AssertPtrReturn(pGstStrmIn, VERR_INVALID_POINTER);
    AssertPtrReturn(pHstStrmIn, VERR_INVALID_POINTER);
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg, VERR_INVALID_POINTER);

    int rc = drvAudioStreamCfgToProps(pCfg, &pGstStrmIn->Props);
    if (RT_SUCCESS(rc))
    {
        rc = audioMixBufInit(&pGstStrmIn->MixBuf, pszName, &pHstStrmIn->Props,
                             audioMixBufSize(&pHstStrmIn->MixBuf));
        if (RT_SUCCESS(rc))
            rc = audioMixBufLinkTo(&pHstStrmIn->MixBuf, &pGstStrmIn->MixBuf);

        if (RT_SUCCESS(rc))
        {
    #ifdef DEBUG
            drvAudioStreamCfgPrint(pCfg);
    #endif
            pGstStrmIn->State.fActive = false;
            pGstStrmIn->State.fEmpty  = true;

            pGstStrmIn->State.pszName = RTStrDup(pszName);
            if (!pGstStrmIn->State.pszName)
                return VERR_NO_MEMORY;

            pGstStrmIn->pHstStrmIn = pHstStrmIn;
        }
    }

    LogFlowFunc(("pszName=%s, rc=%Rrc\n", pszName, rc));
    return rc;
}

static int drvAudioAllocHstIn(PDRVAUDIO pThis, PPDMAUDIOSTREAMCFG pCfg,
                              PDMAUDIORECSOURCE enmRecSource, PPDMAUDIOHSTSTRMIN *ppHstStrmIn)
{
    if (!pThis->cFreeInputStreams)
    {
        LogFlowFunc(("No more input streams free to use, bailing out\n"));
        return VERR_NO_MORE_HANDLES;
    }

    /* Validate backend configuration. */
    if (!pThis->BackendCfg.cbStreamIn)
    {
        LogFlowFunc(("Backend input configuration not valid, bailing out\n"));
        return VERR_INVALID_PARAMETER;
    }

    PPDMAUDIOHSTSTRMIN pHstStrmIn =
        (PPDMAUDIOHSTSTRMIN)RTMemAllocZ(pThis->BackendCfg.cbStreamIn);
    if (!pHstStrmIn)
    {
        LogFlowFunc(("Error allocating host innput stream with %RU32 bytes\n",
                     pThis->BackendCfg.cbStreamOut));
        return VERR_NO_MEMORY;
    }

    int rc;
    bool fInitialized = false;

    do
    {
        uint32_t cSamples;
        rc = pThis->pHostDrvAudio->pfnInitIn(pThis->pHostDrvAudio, pHstStrmIn,
                                             pCfg, enmRecSource, &cSamples);
        if (RT_FAILURE(rc))
        {
            LogFlowFunc(("Initializing host backend failed with rc=%Rrc\n", rc));
            break;
        }

        fInitialized = true;

        rc = audioMixBufInit(&pHstStrmIn->MixBuf, "HostIn", &pHstStrmIn->Props,
                             cSamples);
        if (RT_SUCCESS(rc))
        {
            RTListPrepend(&pThis->lstHstStrmIn, &pHstStrmIn->Node);
            pThis->cFreeInputStreams--;
        }

    } while (0);

    if (RT_FAILURE(rc))
    {
        if (fInitialized)
        {
            int rc2 = pThis->pHostDrvAudio->pfnFiniIn(pThis->pHostDrvAudio,
                                                      pHstStrmIn);
            AssertRC(rc2);
        }

        drvAudioHstInFreeRes(pHstStrmIn);
        RTMemFree(pHstStrmIn);
    }
    else
        *ppHstStrmIn = pHstStrmIn;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

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
int drvAudioWrite(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOGSTSTRMOUT pGstStrmOut,
                  const void *pvBuf, size_t cbBuf, uint32_t *pcbWritten)
{
    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    AssertPtrReturn(pGstStrmOut, VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    AssertReturn(cbBuf, VERR_INVALID_PARAMETER);
    /* pcbWritten is optional. */

    if (!pThis->pHostDrvAudio->pfnIsEnabled(pThis->pHostDrvAudio, PDMAUDIODIR_OUT))
        return VERR_NOT_AVAILABLE;

    PPDMAUDIOHSTSTRMOUT pHstStrmOut = pGstStrmOut->pHstStrmOut;
    AssertPtrReturn(pHstStrmOut, VERR_INVALID_POINTER);

    AssertMsg(pGstStrmOut->pHstStrmOut->fEnabled,
              ("Writing to disabled host output stream \"%s\" not possible\n",
              pHstStrmOut->MixBuf.pszName));

    /*
     * First, write data from the device emulation into our
     * guest mixing buffer.
     */
    uint32_t cWritten;
    int rc = audioMixBufWriteAt(&pGstStrmOut->MixBuf, 0 /* Offset in samples */, pvBuf, cbBuf, &cWritten);

    /*
     * Second, mix the guest mixing buffer with the host mixing
     * buffer so that the host backend can play the data lateron.
     */
    uint32_t cMixed;
    if (   RT_SUCCESS(rc)
        && cWritten)
    {
        rc = audioMixBufMixToParent(&pGstStrmOut->MixBuf, cWritten, &cMixed);
    }
    else
        cMixed = 0;

    if (RT_SUCCESS(rc))
    {
        /* Return the number of samples which actually have been mixed
         * down to the parent, regardless how much samples were written
         * into the children buffer. */
        if (pcbWritten)
            *pcbWritten = AUDIOMIXBUF_S2B(&pGstStrmOut->MixBuf, cMixed);
    }

    LogFlowFunc(("%s -> %s: Written pvBuf=%p, cbBuf=%zu, cWritten=%RU32 (%RU32 bytes), cMixed=%RU32, rc=%Rrc\n",
                 pGstStrmOut->MixBuf.pszName, pHstStrmOut->MixBuf.pszName, pvBuf, cbBuf, cWritten,
                 AUDIOMIXBUF_S2B(&pGstStrmOut->MixBuf, cWritten), cMixed, rc));
    return rc;
}

PPDMAUDIOHSTSTRMOUT drvAudioFindAnyHstOut(PDRVAUDIO pThis, PPDMAUDIOHSTSTRMOUT pHstStrmOut)
{
    if (pHstStrmOut)
    {
        if (RTListNodeIsLast(&pThis->lstHstStrmOut, &pHstStrmOut->Node))
            return NULL;

        return RTListNodeGetNext(&pHstStrmOut->Node, PDMAUDIOHSTSTRMOUT, Node);
    }

    return RTListGetFirst(&pThis->lstHstStrmOut, PDMAUDIOHSTSTRMOUT, Node);
}

PPDMAUDIOHSTSTRMOUT drvAudioHstFindAnyEnabledOut(PDRVAUDIO pThis, PPDMAUDIOHSTSTRMOUT pHostStrmOut)
{
    while ((pHostStrmOut = drvAudioFindAnyHstOut(pThis, pHostStrmOut)))
    {
        if (pHostStrmOut->fEnabled)
            return pHostStrmOut;
    }

    return NULL;
}

PPDMAUDIOHSTSTRMOUT drvAudioFindSpecificOut(PDRVAUDIO pThis, PPDMAUDIOHSTSTRMOUT pHstStrmOut,
                                            PPDMAUDIOSTREAMCFG pCfg)
{
    while ((pHstStrmOut = drvAudioFindAnyHstOut(pThis, pHstStrmOut)))
    {
        if (drvAudioPCMPropsAreEqual(&pHstStrmOut->Props, pCfg))
            return pHstStrmOut;
    }

    return NULL;
}

int drvAudioDestroyHstIn(PDRVAUDIO pThis, PPDMAUDIOHSTSTRMIN pHstStrmIn)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    AssertPtrReturn(pHstStrmIn, VERR_INVALID_POINTER);

    LogFlowFunc(("%s\n", pHstStrmIn->MixBuf.pszName));

    int rc;
    if (!pHstStrmIn->pGstStrmIn) /* No parent anymore? */
    {
        rc = pThis->pHostDrvAudio->pfnFiniIn(pThis->pHostDrvAudio, pHstStrmIn);
        if (RT_SUCCESS(rc))
        {
            drvAudioHstInFreeRes(pHstStrmIn);

            /* Remove from driver instance list. */
            RTListNodeRemove(&pHstStrmIn->Node);

            RTMemFree(pHstStrmIn);
            pThis->cFreeInputStreams++;
        }
    }
    else
        rc = VERR_ACCESS_DENIED;

    LogFlowFunc(("[%s] Still is being used, rc=%Rrc\n", pHstStrmIn->MixBuf.pszName, rc));
    return rc;
}

static int drvAudioDestroyGstIn(PDRVAUDIO pThis, PPDMAUDIOGSTSTRMIN pGstStrmIn)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    LogFlowFunc(("%s\n", pGstStrmIn->MixBuf.pszName));

    if (pGstStrmIn)
    {
        drvAudioGstInFreeRes(pGstStrmIn);

        if (pGstStrmIn->pHstStrmIn)
        {
            /* Unlink child. */
            pGstStrmIn->pHstStrmIn->pGstStrmIn = NULL;

            /* Try destroying the associated host input stream. This could
             * be skipped if there are other guest input streams with this
             * host stream. */
            drvAudioDestroyHstIn(pThis, pGstStrmIn->pHstStrmIn);
        }

        RTMemFree(pGstStrmIn);
    }

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvAudioQueryStatus(PPDMIAUDIOCONNECTOR pInterface,
                                             uint32_t *pcbAvailIn, uint32_t *pcbFreeOut,
                                             uint32_t *pcSamplesLive)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    /* pcbAvailIn is optional. */
    /* pcbFreeOut is optional. */
    /* pcSamplesLive is optional. */

    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);

    if (!pThis->pHostDrvAudio->pfnIsEnabled(pThis->pHostDrvAudio, PDMAUDIODIR_OUT))
        return VERR_NOT_AVAILABLE;

    int rc = VINF_SUCCESS;
    uint32_t cSamplesLive = 0;

    /*
     * Playback.
     */
    uint32_t cbFreeOut = UINT32_MAX;

    PPDMAUDIOHSTSTRMOUT pHstStrmOut = NULL;
    while ((pHstStrmOut = drvAudioHstFindAnyEnabledOut(pThis, pHstStrmOut)))
    {
        uint32_t cStreamsLive;
        cSamplesLive = drvAudioHstOutSamplesLive(pHstStrmOut, &cStreamsLive);
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
        if (!cSamplesLive)
        {
            uint32_t cbFree2 = UINT32_MAX;
            RTListForEach(&pHstStrmOut->lstGstStrmOut, pGstStrmOut, PDMAUDIOGSTSTRMOUT, Node)
            {
                if (pGstStrmOut->State.fActive)
                {
                    /* Tell the sound device emulation how many samples are free
                     * so that it can start writing PCM data to us. */
                    cbFree2 = RT_MIN(cbFree2, AUDIOMIXBUF_S2B_RATIO(&pGstStrmOut->MixBuf,
                                                                    audioMixBufFree(&pGstStrmOut->MixBuf)));

                    LogFlowFunc(("\t[%s] cbFree=%RU32\n", pGstStrmOut->MixBuf.pszName, cbFree2));
                }
            }

            cbFreeOut = RT_MIN(cbFreeOut, cbFree2);
        }
    }

    /*
     * Recording.
     */
    uint32_t cbAvailIn = 0;

    PPDMAUDIOHSTSTRMIN pHstStrmIn = NULL;
    while ((pHstStrmIn = drvAudioFindNextEnabledHstIn(pThis, pHstStrmIn)))
    {
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
                                                          audioMixBufMixed(&pHstStrmIn->MixBuf)));

            LogFlowFunc(("\t[%s] cbFree=%RU32\n", pHstStrmIn->MixBuf.pszName, cbAvailIn));
        }
    }

    if (RT_SUCCESS(rc))
    {
        if (cbFreeOut == UINT32_MAX)
            cbFreeOut = 0;

        if (pcbAvailIn)
            *pcbAvailIn = cbAvailIn;

        if (pcbFreeOut)
            *pcbFreeOut = cbFreeOut;

        if (pcSamplesLive)
            *pcSamplesLive = cSamplesLive;
    }

    return rc;
}

static DECLCALLBACK(int) drvAudioPlayOut(PPDMIAUDIOCONNECTOR pInterface, uint32_t *pcSamplesPlayed)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    /* pcSamplesPlayed is optional. */

    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);

    int rc = VINF_SUCCESS;
    uint32_t cSamplesPlayedMax = 0;

    /*
     * Process all enabled host output streams.
     */
    PPDMAUDIOHSTSTRMOUT pHstStrmOut = NULL;
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
        int rc2 = pThis->pHostDrvAudio->pfnPlayOut(pThis->pHostDrvAudio, pHstStrmOut,
                                                   &cSamplesPlayed);
        if (RT_SUCCESS(rc2))
            cSamplesPlayedMax = RT_MAX(cSamplesPlayed, cSamplesPlayedMax);

        LogFlowFunc(("\t[%s] cSamplesPlayed=%RU32, rc=%Rrc\n", pHstStrmOut->MixBuf.pszName, cSamplesPlayed, rc2));

        bool fNeedsCleanup = false;

        PPDMAUDIOGSTSTRMOUT pGstStrmOut;
        RTListForEach(&pHstStrmOut->lstGstStrmOut, pGstStrmOut, PDMAUDIOGSTSTRMOUT, Node)
        {
            if (   !pGstStrmOut->State.fActive
                && pGstStrmOut->State.fEmpty)
                continue;

            if (audioMixBufIsEmpty(&pGstStrmOut->MixBuf))
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

    return rc;
}

static int drvAudioHostInit(PCFGMNODE pCfgHandle, PDRVAUDIO pThis)
{
    /* pCfgHandle is optional. */
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    NOREF(pCfgHandle);

    LogFlowFuncEnter();

    int rc = pThis->pHostDrvAudio->pfnInit(pThis->pHostDrvAudio);
    if (RT_FAILURE(rc))
    {
        LogFlowFunc(("Initialization of lower driver failed with rc=%Rrc\n", rc));
        return rc;
    }

    uint32_t cMaxHstStrmsOut = pThis->BackendCfg.cMaxHstStrmsOut;
    uint32_t cbHstStrmsOut = pThis->BackendCfg.cbStreamOut;

    if (cbHstStrmsOut)
    {
        pThis->cFreeOutputStreams = 1; /** @todo Make this configurable. */
        if (pThis->cFreeOutputStreams > cMaxHstStrmsOut)
        {
            LogRel(("Audio: Warning: %RU32 output streams were requested, host driver only supports %RU32\n",
                    pThis->cFreeOutputStreams, cMaxHstStrmsOut));
            pThis->cFreeOutputStreams = cMaxHstStrmsOut;
        }
    }
    else
        pThis->cFreeOutputStreams = 0;

    uint32_t cMaxHstStrmsIn = pThis->BackendCfg.cMaxHstStrmsIn;
    uint32_t cbHstStrmIn = pThis->BackendCfg.cbStreamIn;

    if (cbHstStrmIn)
    {
        /*
         * Note:
         *  - Our AC'97 emulation has two inputs, line (P.IN) and microphone (P.MIC).
         ** @todo Document HDA.
         */
        pThis->cFreeInputStreams = 2; /** @todo Make this configurable. */
        if (pThis->cFreeInputStreams > cMaxHstStrmsIn)
        {
            LogRel(("Audio: Warning: %RU32 input streams were requested, host driver only supports %RU32\n",
                    pThis->cFreeInputStreams, cMaxHstStrmsIn));
            pThis->cFreeInputStreams = cMaxHstStrmsIn;
        }
    }
    else
        pThis->cFreeInputStreams = 0;

    LogFlowFunc(("cMaxHstStrmsOut=%RU32 (cb=%RU32), cMaxHstStrmsIn=%RU32 (cb=%RU32)\n",
                 cMaxHstStrmsOut, cbHstStrmsOut, cMaxHstStrmsIn, cbHstStrmIn));

    LogFlowFunc(("cFreeInputStreams=%RU8, cFreeOutputStreams=%RU8\n",
                 pThis->cFreeInputStreams, pThis->cFreeOutputStreams));

    LogFlowFuncLeave();
    return VINF_SUCCESS;
}

static void drvAudioStateHandler(PPDMDRVINS pDrvIns, PDMAUDIOSTREAMCMD enmCmd)
{
    PDRVAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVAUDIO);

    LogFlowFunc(("enmCmd=%ld\n", enmCmd));

    if (!pThis->pHostDrvAudio)
        return;

    PPDMAUDIOHSTSTRMOUT pHstStrmOut = NULL;
    while ((pHstStrmOut = drvAudioHstFindAnyEnabledOut(pThis, pHstStrmOut)))
        pThis->pHostDrvAudio->pfnControlOut(pThis->pHostDrvAudio, pHstStrmOut, enmCmd);

    PPDMAUDIOHSTSTRMIN pHstStrmIn = NULL;
    while ((pHstStrmIn = drvAudioFindNextEnabledHstIn(pThis, pHstStrmIn)))
        pThis->pHostDrvAudio->pfnControlIn(pThis->pHostDrvAudio, pHstStrmIn, enmCmd);
}

static void drvAudioExit(PPDMDRVINS pDrvIns)
{
    LogFlowFuncEnter();

    PDRVAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVAUDIO);

    /* Tear down all host output streams. */
    PPDMAUDIOHSTSTRMOUT pHstStrmOut = NULL;
    while ((pHstStrmOut = drvAudioFindAnyHstOut(pThis, pHstStrmOut)))
    {
        pThis->pHostDrvAudio->pfnControlOut(pThis->pHostDrvAudio, pHstStrmOut, PDMAUDIOSTREAMCMD_DISABLE);
        pThis->pHostDrvAudio->pfnFiniOut(pThis->pHostDrvAudio, pHstStrmOut);
    }

    /* Tear down all host input streams. */
    PPDMAUDIOHSTSTRMIN pHstStrmIn = NULL;
    while ((pHstStrmIn = drvAudioFindNextHstIn(pThis, pHstStrmIn)))
    {
        pThis->pHostDrvAudio->pfnControlIn(pThis->pHostDrvAudio, pHstStrmIn, PDMAUDIOSTREAMCMD_DISABLE);
        pThis->pHostDrvAudio->pfnFiniIn(pThis->pHostDrvAudio, pHstStrmIn);
    }
}

static struct audio_option audio_options[] =
{
    /* DAC */
    {"DACFixedSettings", AUD_OPT_BOOL, &conf.fixed_out.enabled,
     "Use fixed settings for host DAC", NULL, 0},

    {"DACFixedFreq", AUD_OPT_INT, &conf.fixed_out.settings.uHz,
     "Frequency for fixed host DAC", NULL, 0},

    {"DACFixedFmt", AUD_OPT_FMT, &conf.fixed_out.settings.enmFormat,
     "Format for fixed host DAC", NULL, 0},

    {"DACFixedChannels", AUD_OPT_INT, &conf.fixed_out.settings.cChannels,
     "Number of channels for fixed DAC (1 - mono, 2 - stereo)", NULL, 0},

    {"DACVoices", AUD_OPT_INT, &conf.fixed_out.cStreams, /** @todo Rename! */
     "Number of streams for DAC", NULL, 0},

    /* ADC */
    {"ADCFixedSettings", AUD_OPT_BOOL, &conf.fixed_in.enabled,
     "Use fixed settings for host ADC", NULL, 0},

    {"ADCFixedFreq", AUD_OPT_INT, &conf.fixed_in.settings.uHz,
     "Frequency for fixed host ADC", NULL, 0},

    {"ADCFixedFmt", AUD_OPT_FMT, &conf.fixed_in.settings.enmFormat,
     "Format for fixed host ADC", NULL, 0},

    {"ADCFixedChannels", AUD_OPT_INT, &conf.fixed_in.settings.cChannels,
     "Number of channels for fixed ADC (1 - mono, 2 - stereo)", NULL, 0},

    {"ADCVoices", AUD_OPT_INT, &conf.fixed_in.cStreams, /** @todo Rename! */
     "Number of streams for ADC", NULL, 0},

    /* Misc */
    {"TimerFreq", AUD_OPT_INT, &conf.period.hz,
     "Timer frequency in Hz (0 - use lowest possible)", NULL, 0},

    {"PLIVE", AUD_OPT_BOOL, &conf.plive,
     "(undocumented)", NULL, 0}, /** @todo What is this? */

    NULL
};

static DECLCALLBACK(int) drvAudioInit(PCFGMNODE pCfgHandle, PPDMDRVINS pDrvIns)
{
    AssertPtrReturn(pCfgHandle, VERR_INVALID_POINTER);
    AssertPtrReturn(pDrvIns, VERR_INVALID_POINTER);

    PDRVAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVAUDIO);
    LogFlowFunc(("pDrvAudio=%p, pDrvIns=%p\n", pThis, pDrvIns));

    RTListInit(&pThis->lstHstStrmIn);
    RTListInit(&pThis->lstHstStrmOut);

    int rc = VINF_SUCCESS;

    /* Get the configuration data from the selected backend (if available). */
    AssertPtr(pThis->pHostDrvAudio);
    if (RT_LIKELY(pThis->pHostDrvAudio->pfnGetConf))
        rc = pThis->pHostDrvAudio->pfnGetConf(pThis->pHostDrvAudio, &pThis->BackendCfg);

    if (RT_SUCCESS(rc))
    {
        rc = drvAudioProcessOptions(pCfgHandle, "AUDIO", audio_options);
        /** @todo Check for invalid options? */

        pThis->cFreeOutputStreams = conf.fixed_out.cStreams;
        pThis->cFreeInputStreams  = conf.fixed_in.cStreams;

        if (!pThis->cFreeOutputStreams)
            pThis->cFreeOutputStreams = 1;

        if (!pThis->cFreeInputStreams)
            pThis->cFreeInputStreams = 1;
    }

    /*
     * If everything went well, initialize the lower driver.
     */
    if (RT_SUCCESS(rc))
        rc = drvAudioHostInit(pCfgHandle, pThis);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(int) drvAudioInitNull(PPDMIAUDIOCONNECTOR pInterface)
{
    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);
    NOREF(pThis);

    LogRel(("Audio: Using NULL driver; no sound will be audible\n"));

    /* Nothing to do here yet. */
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvAudioRead(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOGSTSTRMIN pGstStrmIn,
                                      void *pvBuf, size_t cbBuf, uint32_t *pcbRead)
{
    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    AssertPtrReturn(pGstStrmIn, VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    AssertReturn(cbBuf, VERR_INVALID_PARAMETER);
    /* pcbWritten is optional. */

    if (!pThis->pHostDrvAudio->pfnIsEnabled(pThis->pHostDrvAudio, PDMAUDIODIR_IN))
        return VERR_NOT_AVAILABLE;

    PPDMAUDIOHSTSTRMIN pHstStrmIn = pGstStrmIn->pHstStrmIn;
    AssertPtrReturn(pHstStrmIn, VERR_INVALID_POINTER);

    AssertMsg(pGstStrmIn->pHstStrmIn->fEnabled,
              ("Reading from disabled host input stream \"%s\" not possible\n", pGstStrmIn->MixBuf.pszName));

    /*
     * Read from the parent buffer (that is, the guest buffer) which
     * should have the audio data in the format the guest needs.
     */
    uint32_t cRead;
    int rc = audioMixBufReadCirc(&pGstStrmIn->MixBuf,
                                 pvBuf, cbBuf, &cRead);
    if (RT_SUCCESS(rc))
    {
        audioMixBufFinish(&pGstStrmIn->MixBuf, cRead);

        if (pcbRead)
            *pcbRead = AUDIOMIXBUF_S2B(&pGstStrmIn->MixBuf, cRead);
    }

    LogFlowFunc(("cRead=%RU32 (%RU32 bytes), rc=%Rrc\n",
                 cRead, AUDIOMIXBUF_S2B(&pGstStrmIn->MixBuf, cRead), rc));
    return rc;
}

static DECLCALLBACK(int) drvAudioIsSetOutVolume(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOGSTSTRMOUT pGstStrmOut,
                                                bool fMute, uint8_t uVolLeft, uint8_t uVolRight)
{
    LogFlowFunc(("[%s] fMute=%RTbool, uVolLeft=%RU8, uVolRight=%RU8\n",
                 pGstStrmOut->MixBuf.pszName, fMute, uVolLeft, uVolRight));

    if (pGstStrmOut)
    {
        pGstStrmOut->State.fMuted       = fMute;
        pGstStrmOut->State.uVolumeLeft  = (uint32_t)uVolLeft * 0x808080; /* maximum is INT_MAX = 0x7fffffff */
        pGstStrmOut->State.uVolumeRight = (uint32_t)uVolRight * 0x808080; /* maximum is INT_MAX = 0x7fffffff */
    }

    return VINF_SUCCESS;
}

/** @todo Handle more channels than stereo ... */
static DECLCALLBACK(int) drvAudioSetVolume(PPDMIAUDIOCONNECTOR pInterface,
                                           bool fMute, uint8_t uVolLeft, uint8_t uVolRight)
{
    LogFlowFunc(("fMute=%RTbool, uVolLeft=%RU8, uVolRight=%RU8\n",
                 fMute, uVolLeft, uVolRight));

    volume_t vol;

    uint32_t u32VolumeLeft  = uVolLeft;
    uint32_t u32VolumeRight = uVolRight;

    /* 0x00..0xff => 0x01..0x100 */
    if (u32VolumeLeft)
        u32VolumeLeft++;
    if (u32VolumeRight)
        u32VolumeRight++;
    vol.mute  = fMute ? 1 : 0;
    vol.l     = u32VolumeLeft  * 0x800000; /* maximum is 0x80000000 */
    vol.r     = u32VolumeRight * 0x800000; /* maximum is 0x80000000 */

    sum_out_volume.mute = 0;
    sum_out_volume.l    = ASMMultU64ByU32DivByU32(INT_MAX, INT_MAX, 0x80000000U);
    sum_out_volume.r    = ASMMultU64ByU32DivByU32(INT_MAX, INT_MAX, 0x80000000U);

    LogFlowFunc(("sum_out l=%RU32, r=%RU32\n", sum_out_volume.l, sum_out_volume.r));

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvAudioEnableOut(PPDMIAUDIOCONNECTOR pInterface,
                                           PPDMAUDIOGSTSTRMOUT pGstVoiceOut, bool fEnable)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    /* pGstVoiceOut is optional. */

    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);

    if (pGstVoiceOut)
    {
        PPDMAUDIOHSTSTRMOUT pHstStrmOut = pGstVoiceOut->pHstStrmOut;
        AssertPtr(pHstStrmOut);

        if (pGstVoiceOut->State.fActive != fEnable)
        {
            if (fEnable)
            {
                pHstStrmOut->fPendingDisable = false;
                if (!pHstStrmOut->fEnabled)
                {
                    pHstStrmOut->fEnabled = true;
                    pThis->pHostDrvAudio->pfnControlOut(pThis->pHostDrvAudio, pHstStrmOut,
                                                        PDMAUDIOSTREAMCMD_ENABLE);
                    /** @todo Check rc. */
                }
            }
            else
            {
                if (pHstStrmOut->fEnabled)
                {
                    uint32_t cGstStrmsActive = 0;

                    PPDMAUDIOGSTSTRMOUT pIter;
                    RTListForEach(&pHstStrmOut->lstGstStrmOut, pIter, PDMAUDIOGSTSTRMOUT, Node)
                    {
                        if (pIter->State.fActive)
                            cGstStrmsActive++;
                    }

                    pHstStrmOut->fPendingDisable = cGstStrmsActive == 1;
                }
            }

            pGstVoiceOut->State.fActive = fEnable;
        }
    }

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvAudioEnableIn(PPDMIAUDIOCONNECTOR pInterface,
                                          PPDMAUDIOGSTSTRMIN pGstStrmIn, bool fEnable)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    /* pGstStrmIn is optional. */

    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);

    if (pGstStrmIn)
    {
        PPDMAUDIOHSTSTRMIN pHstStrmIn = pGstStrmIn->pHstStrmIn;
        AssertPtr(pHstStrmIn);

        LogFlowFunc(("%s -> %s, fEnable=%RTbool\n",
                     pHstStrmIn->MixBuf.pszName, pGstStrmIn->MixBuf.pszName, fEnable));

        if (pGstStrmIn->State.fActive != fEnable)
        {
            if (fEnable)
            {
                if (!pHstStrmIn->fEnabled)
                {
                    int rc2 = pThis->pHostDrvAudio->pfnControlIn(pThis->pHostDrvAudio, pHstStrmIn,
                                                                 PDMAUDIOSTREAMCMD_ENABLE);
                    if (RT_LIKELY(RT_SUCCESS(rc2)))
                        pHstStrmIn->fEnabled = true;
                    else
                        LogFlowFunc(("Error opening host input stream in backend, rc=%Rrc\n", rc2));
                }
            }
            else
            {
                if (pHstStrmIn->fEnabled)
                {
                    int rc2 = pThis->pHostDrvAudio->pfnControlIn(pThis->pHostDrvAudio, pHstStrmIn,
                                                                 PDMAUDIOSTREAMCMD_DISABLE);
                    if (RT_LIKELY(RT_SUCCESS(rc2))) /* Did the backend veto? */
                        pHstStrmIn->fEnabled = false;
                    else
                        LogFlowFunc(("Backend vetoed closing input stream, rc=%Rrc\n", rc2));
                }
            }

            pGstStrmIn->State.fActive = fEnable;
        }
    }

    return VINF_SUCCESS;
}

static DECLCALLBACK(bool) drvAudioIsInputOK(PPDMIAUDIOCONNECTOR pInterface,
                                            PPDMAUDIOGSTSTRMIN  pGstStrmIn)
{
    return (pGstStrmIn != NULL);
}

static DECLCALLBACK(bool) drvAudioIsOutputOK(PPDMIAUDIOCONNECTOR pInterface,
                                             PPDMAUDIOGSTSTRMOUT pGstStrmOut)
{
    return (pGstStrmOut != NULL);
}

static DECLCALLBACK(int) drvAudioOpenIn(PPDMIAUDIOCONNECTOR pInterface, const char *pszName,
                                        PDMAUDIORECSOURCE enmRecSource, PPDMAUDIOSTREAMCFG pCfg,
                                        PPDMAUDIOGSTSTRMIN *ppGstStrmIn)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(ppGstStrmIn, VERR_INVALID_POINTER);
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg, VERR_INVALID_POINTER);
    AssertPtrReturn(ppGstStrmIn, VERR_INVALID_POINTER);

    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);

    LogFlowFunc(("pszName=%s, pCfg=%p\n", pszName, pCfg));

    if (!drvAudioStreamCfgIsValid(pCfg))
    {
        LogFunc(("Input stream configuration is not valid, bailing out\n"));
        return VERR_INVALID_PARAMETER;
    }

    PPDMAUDIOGSTSTRMIN pGstStrmIn = *ppGstStrmIn;
    if (   pGstStrmIn
        && drvAudioPCMPropsAreEqual(&pGstStrmIn->Props, pCfg))
    {
        LogFunc(("[%s] Exists and matches required configuration, skipping creation\n",
                 pGstStrmIn->MixBuf.pszName));
        return VWRN_ALREADY_EXISTS;
    }

    if (   !conf.fixed_in.enabled
        && pGstStrmIn)
    {
        drvAudioDestroyGstIn(pThis, pGstStrmIn);
        pGstStrmIn = NULL;
    }

    int rc;

    if (pGstStrmIn)
    {
        PPDMAUDIOHSTSTRMIN pHstStrmIn = pGstStrmIn->pHstStrmIn;
        AssertPtr(pHstStrmIn);

        drvAudioGstInFreeRes(pGstStrmIn);
        rc = drvAudioGstInInit(pGstStrmIn, pHstStrmIn, pszName, pCfg);
    }
    else
        rc = drvAudioCreateStreamPairIn(pThis, pszName, enmRecSource, pCfg, &pGstStrmIn);

    if (pGstStrmIn)
    {
        pGstStrmIn->State.uVolumeLeft = nominal_volume.l;
        pGstStrmIn->State.uVolumeRight = nominal_volume.r;
        pGstStrmIn->State.fMuted = RT_BOOL(nominal_volume.mute);

        *ppGstStrmIn = pGstStrmIn;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

DECLCALLBACK(int) drvAudioOpenOut(PPDMIAUDIOCONNECTOR pInterface, const char *pszName,
                                  PPDMAUDIOSTREAMCFG pCfg, PPDMAUDIOGSTSTRMOUT *ppGstStrmOut)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg, VERR_INVALID_POINTER);
    AssertPtrReturn(ppGstStrmOut, VERR_INVALID_POINTER);

    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);

    LogFlowFunc(("pszName=%s, pCfg=%p\n", pszName, pCfg));

    if (!drvAudioStreamCfgIsValid(pCfg))
    {
        LogFunc(("Output stream configuration is not valid, bailing out\n"));
        return VERR_INVALID_PARAMETER;
    }

    PPDMAUDIOGSTSTRMOUT pGstStrmOut = *ppGstStrmOut;
    if (   pGstStrmOut
        && drvAudioPCMPropsAreEqual(&pGstStrmOut->Props, pCfg))
    {
        LogFunc(("[%s] Exists and matches required configuration, skipping creation\n",
                 pGstStrmOut->MixBuf.pszName));
        return VWRN_ALREADY_EXISTS;
    }

#if 0
    /* Any live samples that need to be updated after
     * we set the new parameters? */
    PPDMAUDIOGSTSTRMOUT pOldGstStrmOut = NULL;
    uint32_t cLiveSamples = 0;

    if (   conf.plive
        && pGstStrmOut
        && (   !pGstStrmOut->State.fActive
            && !pGstStrmOut->State.fEmpty))
    {
        cLiveSamples = pGstStrmOut->cTotalSamplesWritten;
        if (cLiveSamples)
        {
            pOldGstStrmOut = pGstStrmOut;
            pGstStrmOut = NULL;
        }
    }
#endif

    if (   pGstStrmOut
        && !conf.fixed_out.enabled)
    {
        drvAudioDestroyGstOut(pThis, pGstStrmOut);
        pGstStrmOut = NULL;
    }

    int rc;
    if (pGstStrmOut)
    {
        PPDMAUDIOHSTSTRMOUT pHstStrmOut = pGstStrmOut->pHstStrmOut;
        AssertPtr(pHstStrmOut);

        drvAudioGstOutFreeRes(pGstStrmOut);

        rc = drvAudioGstOutInit(pGstStrmOut, pHstStrmOut, pszName, pCfg);
    }
    else
    {
        rc = drvAudioCreateStreamPairOut(pThis, pszName, pCfg, &pGstStrmOut);
        if (RT_FAILURE(rc))
            LogFunc(("Failed to create output stream \"%s\", rc=%Rrc\n", pszName, rc));
    }

    if (RT_SUCCESS(rc))
    {
        AssertPtr(pGstStrmOut);
        pGstStrmOut->State.uVolumeLeft  = nominal_volume.l;
        pGstStrmOut->State.uVolumeRight = nominal_volume.r;
        pGstStrmOut->State.fMuted       = RT_BOOL(nominal_volume.mute);

        *ppGstStrmOut = pGstStrmOut;
#if 0
        /* Update remaining live samples with new rate. */
        if (cLiveSamples)
        {
            AssertPtr(pOldGstStrmOut);

            uint32_t cSamplesMixed =
                (cLiveSamples << pOldGstStrmOut->Props.cShift)
                * pOldGstStrmOut->Props.cbPerSec
                / (*ppGstStrmOut)->Props.cbPerSec;

            pGstStrmOut->cTotalSamplesWritten += cSamplesMixed;
        }
#endif
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(bool) drvAudioIsActiveIn(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOGSTSTRMIN pGstStrmIn)
{
    return pGstStrmIn ? pGstStrmIn->State.fActive : false;
}

static DECLCALLBACK(bool) drvAudioIsActiveOut(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOGSTSTRMOUT pGstStrmOut)
{
    return pGstStrmOut ? pGstStrmOut->State.fActive : false;
}

static DECLCALLBACK(void) drvAudioCloseIn(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOGSTSTRMIN pGstStrmIn)
{
    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);
    if (pGstStrmIn)
        drvAudioDestroyGstIn(pThis, pGstStrmIn);
}

DECLCALLBACK(void) drvAudioCloseOut(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOGSTSTRMOUT pGstStrmOut)
{
    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);
    if (pGstStrmOut)
        drvAudioDestroyGstOut(pThis, pGstStrmOut);
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
    drvAudioStateHandler(pDrvIns, PDMAUDIOSTREAMCMD_DISABLE);
}

/**
 * Destructs an audio driver instance.
 *
 * Most VM resources are freed by the VM. This callback is provided so that any non-VM
 * resources can be freed correctly.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) drvAudioDestruct(PPDMDRVINS pDrvIns)
{
    LogFlowFuncEnter();
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);

    drvAudioExit(pDrvIns);
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
    pThis->pDrvIns                                   = pDrvIns;
    /* IBase. */
    pDrvIns->IBase.pfnQueryInterface                 = drvAudioQueryInterface;
    /* IAudio. */
    pThis->IAudioConnector.pfnQueryStatus            = drvAudioQueryStatus;
    pThis->IAudioConnector.pfnRead                   = drvAudioRead;
    pThis->IAudioConnector.pfnWrite                  = drvAudioWrite;
    pThis->IAudioConnector.pfnIsInputOK              = drvAudioIsInputOK;
    pThis->IAudioConnector.pfnIsOutputOK             = drvAudioIsOutputOK;
    pThis->IAudioConnector.pfnInitNull               = drvAudioInitNull;
    pThis->IAudioConnector.pfnSetVolumeOut           = drvAudioIsSetOutVolume;
    pThis->IAudioConnector.pfnSetVolume              = drvAudioSetVolume;
    pThis->IAudioConnector.pfnEnableOut              = drvAudioEnableOut;
    pThis->IAudioConnector.pfnEnableIn               = drvAudioEnableIn;
    pThis->IAudioConnector.pfnCloseIn                = drvAudioCloseIn;
    pThis->IAudioConnector.pfnCloseOut               = drvAudioCloseOut;
    pThis->IAudioConnector.pfnOpenIn                 = drvAudioOpenIn;
    pThis->IAudioConnector.pfnOpenOut                = drvAudioOpenOut;
    pThis->IAudioConnector.pfnPlayOut                = drvAudioPlayOut;
    pThis->IAudioConnector.pfnIsActiveIn             = drvAudioIsActiveIn;
    pThis->IAudioConnector.pfnIsActiveOut            = drvAudioIsActiveOut;

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

#ifdef DEBUG_andy
    CFGMR3Dump(pCfgHandle);
#endif

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
 * Suspend notification.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) drvAudioSuspend(PPDMDRVINS pDrvIns)
{
    drvAudioStateHandler(pDrvIns, PDMAUDIOSTREAMCMD_DISABLE);
}

/**
 * Resume notification.
 *
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) drvAudioResume(PPDMDRVINS pDrvIns)
{
    drvAudioStateHandler(pDrvIns, PDMAUDIOSTREAMCMD_ENABLE);
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
    /* fClass. */
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
