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
static int drvAudioStreamControlInternalBackend(PDRVAUDIO pThis, PPDMAUDIOSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd);
static int drvAudioStreamControlInternal(PDRVAUDIO pThis, PPDMAUDIOSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd);
static int drvAudioStreamDestroyInternalBackend(PDRVAUDIO pThis, PPDMAUDIOSTREAM pHstStream);
static int drvAudioStreamDestroyInternal(PDRVAUDIO pThis, PPDMAUDIOSTREAM pStream);
static int drvAudioStreamInitInternal(PDRVAUDIO pThis, PPDMAUDIOSTREAM pStream, PPDMAUDIOSTREAMCFG pCfgHost, PPDMAUDIOSTREAMCFG pCfgGuest);
static int drvAudioStreamIterateInternal(PDRVAUDIO pThis, PPDMAUDIOSTREAM pStream);
static int drvAudioStreamReInitInternal(PDRVAUDIO pThis, PPDMAUDIOSTREAM pStream);

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

/**
 * Returns the host stream part of an audio stream pair, or NULL
 * if no host stream has been assigned / is not available.
 *
 * @returns IPRT status code.
 * @param   pStream             Audio stream to retrieve host stream part for.
 */
inline PPDMAUDIOSTREAM drvAudioGetHostStream(PPDMAUDIOSTREAM pStream)
{
    AssertPtrReturn(pStream, NULL);

    if (!pStream)
        return NULL;

    PPDMAUDIOSTREAM pHstStream = pStream->enmCtx == PDMAUDIOSTREAMCTX_HOST
                               ? pStream
                               : pStream->pPair;
    if (pHstStream)
    {
        Assert(pHstStream->enmCtx == PDMAUDIOSTREAMCTX_HOST);
    }
    else
        LogFlowFunc(("%s: Warning: Does not have a host stream (anymore)\n", pStream->szName));

    return pHstStream;
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

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamControl}
 */
static DECLCALLBACK(int) drvAudioStreamControl(PPDMIAUDIOCONNECTOR pInterface,
                                               PPDMAUDIOSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);

    if (!pStream)
        return VINF_SUCCESS;

    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_FAILURE(rc))
        return rc;

    LogFlowFunc(("[%s] enmStreamCmd=%RU32\n", pStream->szName, enmStreamCmd));

    rc = drvAudioStreamControlInternal(pThis, pStream, enmStreamCmd);

    int rc2 = RTCritSectLeave(&pThis->CritSect);
    if (RT_SUCCESS(rc))
        rc = rc2;

    return rc;
}

/**
 * Controls an audio stream.
 *
 * @returns IPRT status code.
 * @param   pThis               Pointer to driver instance.
 * @param   pStream             Stream to control.
 * @param   enmStreamCmd        Control command.
 */
static int drvAudioStreamControlInternal(PDRVAUDIO pThis, PPDMAUDIOSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    AssertPtrReturn(pThis,   VERR_INVALID_POINTER);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);

    LogFunc(("[%s] enmStreamCmd=%RU32\n", pStream->szName, enmStreamCmd));

    PPDMAUDIOSTREAM pHstStream = drvAudioGetHostStream(pStream);
    PPDMAUDIOSTREAM pGstStream = pHstStream ? pHstStream->pPair : pStream;
    AssertPtr(pGstStream);

    LogFlowFunc(("Status host=0x%x, guest=0x%x\n", pHstStream->fStatus, pGstStream->fStatus));

    int rc = VINF_SUCCESS;

    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
        {
            if (!(pGstStream->fStatus & PDMAUDIOSTRMSTS_FLAG_ENABLED))
            {
                if (pHstStream)
                {
                    /* Is a pending disable outstanding? Then disable first. */
                    if (pHstStream->fStatus & PDMAUDIOSTRMSTS_FLAG_PENDING_DISABLE)
                        rc = drvAudioStreamControlInternalBackend(pThis, pHstStream, PDMAUDIOSTREAMCMD_DISABLE);

                    if (RT_SUCCESS(rc))
                        rc = drvAudioStreamControlInternalBackend(pThis, pHstStream, PDMAUDIOSTREAMCMD_ENABLE);
                }

                pGstStream->fStatus |= PDMAUDIOSTRMSTS_FLAG_ENABLED;
            }
            break;
        }

        case PDMAUDIOSTREAMCMD_DISABLE:
        case PDMAUDIOSTREAMCMD_PAUSE:
        {
            if (pGstStream->fStatus & PDMAUDIOSTRMSTS_FLAG_ENABLED)
            {
                /* Is the guest side stream still active?
                 * Mark the host stream as pending disable and bail out. */
                if (pHstStream)
                {
                    LogFunc(("[%s] Pending disable/pause\n", pHstStream->szName));
                    pHstStream->fStatus |= PDMAUDIOSTRMSTS_FLAG_PENDING_DISABLE;
                }

                if (enmStreamCmd == PDMAUDIOSTREAMCMD_DISABLE)
                {
                    pGstStream->fStatus &= ~PDMAUDIOSTRMSTS_FLAG_ENABLED;
                }
                else if (enmStreamCmd == PDMAUDIOSTREAMCMD_PAUSE)
                    pGstStream->fStatus |= PDMAUDIOSTRMSTS_FLAG_PAUSED;
                else
                    AssertFailedBreakStmt(rc = VERR_NOT_IMPLEMENTED);
            }

            if (   pHstStream
                && !(pHstStream->fStatus & PDMAUDIOSTRMSTS_FLAG_PENDING_DISABLE))
            {
                rc = drvAudioStreamControlInternalBackend(pThis, pHstStream, enmStreamCmd);
                if (RT_SUCCESS(rc))
                    pHstStream->fStatus &= ~PDMAUDIOSTRMSTS_FLAG_PENDING_DISABLE;
            }
            break;
        }

        case PDMAUDIOSTREAMCMD_RESUME:
        {
            if (pGstStream->fStatus & PDMAUDIOSTRMSTS_FLAG_PAUSED)
            {
                if (pHstStream)
                    rc = drvAudioStreamControlInternalBackend(pThis, pHstStream, PDMAUDIOSTREAMCMD_RESUME);

                pGstStream->fStatus &= ~PDMAUDIOSTRMSTS_FLAG_PAUSED;
            }
            break;
        }

        default:
            AssertMsgFailed(("Command %RU32 not implemented\n", enmStreamCmd));
            rc = VERR_NOT_IMPLEMENTED;
            break;
    }

    if (RT_FAILURE(rc))
        LogFunc(("[%s] Failed with %Rrc\n", pStream->szName, rc));

    return rc;
}

/**
 * Controls a stream's backend.
 * If the stream has no backend available, VERR_NOT_FOUND is returned.
 *
 * @returns IPRT status code.
 * @param   pThis               Pointer to driver instance.
 * @param   pStream             Stream to control.
 * @param   enmStreamCmd        Control command.
 */
static int drvAudioStreamControlInternalBackend(PDRVAUDIO pThis, PPDMAUDIOSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    AssertPtrReturn(pThis,   VERR_INVALID_POINTER);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);

    PPDMAUDIOSTREAM pHstStream = drvAudioGetHostStream(pStream);
    if (!pHstStream) /* Stream does not have a host backend? Bail out. */
        return VERR_NOT_FOUND;

    LogFlowFunc(("[%s] enmStreamCmd=%RU32, fStatus=0x%x\n", pHstStream->szName, enmStreamCmd, pHstStream->fStatus));

    AssertPtr(pThis->pHostDrvAudio);

    int rc = VINF_SUCCESS;

    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
        {
            if (!(pHstStream->fStatus & PDMAUDIOSTRMSTS_FLAG_ENABLED))
            {
                LogRel2(("Audio: Enabling stream '%s'\n", pHstStream->szName));
                rc = pThis->pHostDrvAudio->pfnStreamControl(pThis->pHostDrvAudio, pHstStream, PDMAUDIOSTREAMCMD_ENABLE);
                if (RT_SUCCESS(rc))
                {
                    pHstStream->fStatus |= PDMAUDIOSTRMSTS_FLAG_ENABLED;
                }
                else
                    LogRel2(("Audio: Disabling stream '%s' failed with %Rrc\n", pHstStream->szName, rc));
            }
            break;
        }

        case PDMAUDIOSTREAMCMD_DISABLE:
        {
            if (pHstStream->fStatus & PDMAUDIOSTRMSTS_FLAG_ENABLED)
            {
                LogRel2(("Audio: Disabling stream '%s'\n", pHstStream->szName));
                rc = pThis->pHostDrvAudio->pfnStreamControl(pThis->pHostDrvAudio, pHstStream, PDMAUDIOSTREAMCMD_DISABLE);
                if (RT_SUCCESS(rc))
                {
                    pHstStream->fStatus &= ~PDMAUDIOSTRMSTS_FLAG_ENABLED;
                    pHstStream->fStatus &= ~PDMAUDIOSTRMSTS_FLAG_PENDING_DISABLE;
                    AudioMixBufReset(&pHstStream->MixBuf);
                }
                else
                    LogRel2(("Audio: Disabling stream '%s' failed with %Rrc\n", pHstStream->szName, rc));
            }
            break;
        }

        case PDMAUDIOSTREAMCMD_PAUSE:
        {
            /* Only pause if the stream is enabled. */
            if (!(pHstStream->fStatus & PDMAUDIOSTRMSTS_FLAG_ENABLED))
                break;

            if (!(pHstStream->fStatus & PDMAUDIOSTRMSTS_FLAG_PAUSED))
            {
                LogRel2(("Audio: Pausing stream '%s'\n", pHstStream->szName));
                rc = pThis->pHostDrvAudio->pfnStreamControl(pThis->pHostDrvAudio, pHstStream, PDMAUDIOSTREAMCMD_PAUSE);
                if (RT_SUCCESS(rc))
                {
                    pHstStream->fStatus |= PDMAUDIOSTRMSTS_FLAG_PAUSED;
                }
                else
                    LogRel2(("Audio: Pausing stream '%s' failed with %Rrc\n", pHstStream->szName, rc));
            }
            break;
        }

        case PDMAUDIOSTREAMCMD_RESUME:
        {
            /* Only need to resume if the stream is enabled. */
            if (!(pHstStream->fStatus & PDMAUDIOSTRMSTS_FLAG_ENABLED))
                break;

            if (pHstStream->fStatus & PDMAUDIOSTRMSTS_FLAG_PAUSED)
            {
                LogRel2(("Audio: Resuming stream '%s'\n", pHstStream->szName));
                rc = pThis->pHostDrvAudio->pfnStreamControl(pThis->pHostDrvAudio, pHstStream, PDMAUDIOSTREAMCMD_RESUME);
                if (RT_SUCCESS(rc))
                {
                    pHstStream->fStatus &= ~PDMAUDIOSTRMSTS_FLAG_PAUSED;
                }
                else
                    LogRel2(("Audio: Resuming stream '%s' failed with %Rrc\n", pHstStream->szName, rc));
            }
            break;
        }

        default:
        {
            AssertMsgFailed(("Command %RU32 not implemented\n", enmStreamCmd));
            rc = VERR_NOT_IMPLEMENTED;
            break;
        }
    }

    if (RT_FAILURE(rc))
        LogFunc(("[%s] Failed with %Rrc\n", pStream->szName, rc));

    return rc;
}

/**
 * Initializes an audio stream with a given host and guest stream configuration.
 *
 * @returns IPRT status code.
 * @param   pThis               Pointer to driver instance.
 * @param   pStream             Stream to initialize.
 * @param   pCfgHost            Stream configuration to use for the host side (backend).
 * @param   pCfgGuest           Stream configuration to use for the guest side.
 */
static int drvAudioStreamInitInternal(PDRVAUDIO pThis,
                                      PPDMAUDIOSTREAM pStream, PPDMAUDIOSTREAMCFG pCfgHost, PPDMAUDIOSTREAMCFG pCfgGuest)
{
    AssertPtrReturn(pThis,   VERR_INVALID_POINTER);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);

    PPDMAUDIOSTREAM pHstStream = drvAudioGetHostStream(pStream);
    PPDMAUDIOSTREAM pGstStream = pHstStream ? pHstStream->pPair : pStream;
    AssertPtr(pGstStream);

    LogFlowFunc(("[%s]\n", pStream->szName));

    /*
     * Init host stream.
     */

    uint32_t cSamples = 0;
    int rc = pThis->pHostDrvAudio->pfnStreamCreate(pThis->pHostDrvAudio, pHstStream, pCfgHost, &cSamples);
    if (RT_SUCCESS(rc))
    {
        /* Only set the host's stream to initialized if we were able create the stream
         * in the host backend. This is necessary for trying to re-initialize the stream
         * at some later point in time. */
        pHstStream->fStatus |= PDMAUDIOSTRMSTS_FLAG_INITIALIZED;
    }
    else
        LogFlowFunc(("[%s] Initializing stream in host backend failed with rc=%Rrc\n", pStream->szName, rc));

    int rc2 = DrvAudioHlpStreamCfgToProps(pCfgHost, &pHstStream->Props);
    AssertRC(rc2);

    /* Destroy any former mixing buffer. */
    AudioMixBufDestroy(&pHstStream->MixBuf);

    if (cSamples)
    {
        cSamples = cSamples * 4;

        LogFlowFunc(("[%s] cSamples=%RU32\n", pHstStream->szName, cSamples));

        rc2 = AudioMixBufInit(&pHstStream->MixBuf, pHstStream->szName, &pHstStream->Props, cSamples);
        AssertRC(rc2);
    }

    /* Make a copy of the host stream configuration. */
    memcpy(&pHstStream->Cfg, pCfgHost, sizeof(PDMAUDIOSTREAMCFG));

    /*
     * Init guest stream.
     */

    rc2 = DrvAudioHlpStreamCfgToProps(pCfgGuest, &pGstStream->Props);
    AssertRC(rc2);

    /* Destroy any former mixing buffer. */
    AudioMixBufDestroy(&pGstStream->MixBuf);

    if (cSamples)
    {
        cSamples = cSamples * 2;

        LogFlowFunc(("[%s] cSamples=%RU32\n", pGstStream->szName, cSamples));

        rc2 = AudioMixBufInit(&pGstStream->MixBuf, pGstStream->szName, &pGstStream->Props, cSamples);
        AssertRC(rc2);
    }

#ifdef VBOX_WITH_STATISTICS
    char szStatName[255];
#endif

    if (cSamples)
    {
        if (pCfgGuest->enmDir == PDMAUDIODIR_IN)
        {
            /* Host (Parent) -> Guest (Child). */
            rc2 = AudioMixBufLinkTo(&pHstStream->MixBuf, &pGstStream->MixBuf);
            AssertRC(rc2);

#ifdef VBOX_WITH_STATISTICS
            RTStrPrintf(szStatName, sizeof(szStatName), "Guest/%s/BytesElapsed", pGstStream->szName);
            PDMDrvHlpSTAMRegCounterEx(pThis->pDrvIns, &pGstStream->In.StatBytesElapsed,
                                      szStatName, STAMUNIT_BYTES, "Elapsed bytes read.");

            RTStrPrintf(szStatName, sizeof(szStatName), "Guest/%s/BytesRead", pGstStream->szName);
            PDMDrvHlpSTAMRegCounterEx(pThis->pDrvIns, &pGstStream->In.StatBytesTotalRead,
                                      szStatName, STAMUNIT_BYTES, "Total bytes read.");

            RTStrPrintf(szStatName, sizeof(szStatName), "Host/%s/SamplesCaptured", pHstStream->szName);
            PDMDrvHlpSTAMRegCounterEx(pThis->pDrvIns, &pHstStream->In.StatSamplesCaptured,
                                      szStatName, STAMUNIT_COUNT, "Total samples captured.");
#endif
        }
        else
        {
            /* Guest (Parent) -> Host (Child). */
            rc2 = AudioMixBufLinkTo(&pGstStream->MixBuf, &pHstStream->MixBuf);
            AssertRC(rc2);

#ifdef VBOX_WITH_STATISTICS
            RTStrPrintf(szStatName, sizeof(szStatName), "Guest/%s/BytesElapsed", pGstStream->szName);
            PDMDrvHlpSTAMRegCounterEx(pThis->pDrvIns, &pGstStream->Out.StatBytesElapsed,
                                      szStatName, STAMUNIT_BYTES, "Elapsed bytes written.");

            RTStrPrintf(szStatName, sizeof(szStatName), "Guest/%s/BytesRead", pGstStream->szName);
            PDMDrvHlpSTAMRegCounterEx(pThis->pDrvIns, &pGstStream->Out.StatBytesTotalWritten,
                                      szStatName, STAMUNIT_BYTES, "Total bytes written.");

            RTStrPrintf(szStatName, sizeof(szStatName), "Host/%s/SamplesPlayed", pHstStream->szName);
            PDMDrvHlpSTAMRegCounterEx(pThis->pDrvIns, &pHstStream->Out.StatSamplesPlayed,
                                      szStatName, STAMUNIT_COUNT, "Total samples played.");
#endif
        }
    }

    /* Make a copy of the host stream configuration. */
    memcpy(&pGstStream->Cfg, pCfgGuest, sizeof(PDMAUDIOSTREAMCFG));

    LogFlowFunc(("[%s] Returning %Rrc\n", pStream->szName, rc));
    return rc;
}

/**
 * Re-initializes an audio stream with its existing host and guest stream configuration.
 *
 * @returns IPRT status code.
 * @param   pThis               Pointer to driver instance.
 * @param   pStream             Stream to re-initialize.
 */
static int drvAudioStreamReInitInternal(PDRVAUDIO pThis, PPDMAUDIOSTREAM pStream)
{
    AssertPtrReturn(pThis,   VERR_INVALID_POINTER);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);

    LogFlowFunc(("[%s]\n", pStream->szName));

    PPDMAUDIOSTREAM pHstStream = drvAudioGetHostStream(pStream);
    PPDMAUDIOSTREAM pGstStream = pHstStream ? pHstStream->pPair : pStream;
    AssertPtr(pGstStream);

    int rc;

    if (/* Stream initialized? */
            (pHstStream->fStatus & PDMAUDIOSTRMSTS_FLAG_INITIALIZED)
        /* Not in pending re-init before? */
        && !(pHstStream->fStatus & PDMAUDIOSTRMSTS_FLAG_PENDING_REINIT))
    {
        /* Disable first. */
        rc = pThis->pHostDrvAudio->pfnStreamControl(pThis->pHostDrvAudio, pHstStream, PDMAUDIOSTREAMCMD_DISABLE);
        if (RT_FAILURE(rc))
        {
            LogFunc(("[%s] Error disabling stream, rc=%Rrc\n", pStream->szName, rc));
            return rc;
        }

        /* Give the backend the chance to clean up the old context. */
        rc = pThis->pHostDrvAudio->pfnStreamDestroy(pThis->pHostDrvAudio, pHstStream);
        if (RT_FAILURE(rc))
        {
            LogFunc(("[%s] Error destroying stream in backend, rc=%Rrc\n", pStream->szName, rc));
            return rc;
        }

        /* Set the pending re-init bit. */
        pHstStream->fStatus |= PDMAUDIOSTRMSTS_FLAG_PENDING_REINIT;
    }

    LogFlowFunc(("[%s] Host status is 0x%x\n", pStream->szName, pHstStream->fStatus));

    /* Try to re-initialize the stream. */
    rc = drvAudioStreamInitInternal(pThis, pStream, &pHstStream->Cfg, &pGstStream->Cfg);
    if (RT_SUCCESS(rc))
    {
        /* Try to restore the previous stream status, if possible. */
        PDMAUDIOSTREAMCMD enmCmdRestore = PDMAUDIOSTREAMCMD_UNKNOWN;

        if (pHstStream->fStatus & PDMAUDIOSTRMSTS_FLAG_ENABLED) /* Stream was running before? */
        {
            LogFunc(("[%s] Re-enabling host stream ...\n", pStream->szName));
            enmCmdRestore = PDMAUDIOSTREAMCMD_ENABLE;
        }

        if (enmCmdRestore != PDMAUDIOSTREAMCMD_UNKNOWN)
            rc = pThis->pHostDrvAudio->pfnStreamControl(pThis->pHostDrvAudio, pHstStream, PDMAUDIOSTREAMCMD_ENABLE);

        /* Re-initialization successful, remove bit again. */
        pHstStream->fStatus &= ~PDMAUDIOSTRMSTS_FLAG_PENDING_REINIT;
    }

    LogFunc(("[%s] Reinitialization returned %Rrc\n", pStream->szName, rc));
    return rc;
}

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamWrite}
 */
static DECLCALLBACK(int) drvAudioStreamWrite(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream,
                                             const void *pvBuf, uint32_t cbBuf, uint32_t *pcbWritten)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuf,      VERR_INVALID_POINTER);
    /* pcbWritten is optional. */

    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);

    if (   !pStream
        || !cbBuf)
    {
        if (pcbWritten)
            *pcbWritten = 0;
        return VINF_SUCCESS;
    }

    AssertMsg(pStream->enmDir == PDMAUDIODIR_OUT,
              ("Stream '%s' is not an output stream and therefore cannot be written to (direction is 0x%x)\n",
               pStream->szName, pStream->enmDir));

    uint32_t cbWritten = 0;

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_FAILURE(rc))
        return rc;

    do
    {
        if (   pThis->pHostDrvAudio
            && pThis->pHostDrvAudio->pfnGetStatus
            && pThis->pHostDrvAudio->pfnGetStatus(pThis->pHostDrvAudio, PDMAUDIODIR_OUT) != PDMAUDIOBACKENDSTS_RUNNING)
        {
            rc = VERR_NOT_AVAILABLE;
            break;
        }

        PPDMAUDIOSTREAM pHstStream = drvAudioGetHostStream(pStream);
        if (!pHstStream)
        {
            rc = VERR_NOT_AVAILABLE;
            break;
        }

        PPDMAUDIOSTREAM pGstStream = pHstStream->pPair;

        AssertMsg(pGstStream->fStatus & PDMAUDIOSTRMSTS_FLAG_ENABLED,
                  ("Writing to disabled guest output stream \"%s\" not possible\n", pGstStream->szName));

        pGstStream->Out.tsLastWriteMS = RTTimeMilliTS();

        if (!AudioMixBufFreeBytes(&pGstStream->MixBuf))
        {
            LogRel2(("Audio: Guest stream '%s' full, expect stuttering audio output\n", pGstStream->szName));
            break;
        }

        uint32_t cWritten = 0;
        rc = AudioMixBufWriteCirc(&pGstStream->MixBuf, pvBuf, cbBuf, &cWritten);
        if (rc == VINF_BUFFER_OVERFLOW)
        {
            LogRel2(("Audio: Lost audio samples from guest stream '%s', expect stuttering audio output\n", pGstStream->szName));
            rc = VINF_SUCCESS;
            break;
        }

#ifdef VBOX_WITH_STATISTICS
        STAM_COUNTER_ADD(&pThis->Stats.TotalBytesWritten, AUDIOMIXBUF_S2B(&pGstStream->MixBuf, cWritten));
        STAM_COUNTER_ADD(&pGstStream->Out.StatBytesTotalWritten, AUDIOMIXBUF_S2B(&pGstStream->MixBuf, cWritten));
#endif
        cbWritten = AUDIOMIXBUF_S2B(&pGstStream->MixBuf, cWritten);

        Log3Func(("[%s] cUsed=%RU32, cLive=%RU32\n",
                  pGstStream->szName, AudioMixBufUsed(&pGstStream->MixBuf), AudioMixBufLive(&pGstStream->MixBuf)));

    } while (0);

    int rc2 = RTCritSectLeave(&pThis->CritSect);
    if (RT_SUCCESS(rc))
        rc = rc2;

    if (RT_SUCCESS(rc))
    {
        if (pcbWritten)
            *pcbWritten = cbWritten;
    }

    return rc;
}

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamAddRef}
 */
static DECLCALLBACK(uint32_t) drvAudioStreamAddRef(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream)
{
   AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
   AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

   NOREF(pInterface);

   return ++pStream->cRefs;
}

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamRelease}
 */
static DECLCALLBACK(uint32_t) drvAudioStreamRelease(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream)
{
   AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
   AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

   NOREF(pInterface);

   if (pStream->cRefs > 1) /* 1 reference always is kept by this audio driver. */
       pStream->cRefs--;

   return pStream->cRefs;
}

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamIterate}
 */
static DECLCALLBACK(int) drvAudioStreamIterate(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    /* pcData is optional. */

    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_FAILURE(rc))
        return rc;

    rc = drvAudioStreamIterateInternal(pThis, pStream);

    int rc2 = RTCritSectLeave(&pThis->CritSect);
    if (RT_SUCCESS(rc))
        rc = rc2;

    if (RT_FAILURE(rc))
        LogFlowFuncLeaveRC(rc);

    return rc;
}

/**
 * Does one iteration of an audio stream.
 * This function gives the backend the chance of iterating / altering data and
 * does the actual mixing between the guest <-> host mixing buffers.
 *
 * @returns IPRT status code.
 * @param   pThis               Pointer to driver instance.
 * @param   pStream             Stream to iterate.
 *
 * @remark
 */
static int drvAudioStreamIterateInternal(PDRVAUDIO pThis, PPDMAUDIOSTREAM pStream)
{
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    if (!pStream)
        return VINF_SUCCESS;

    PPDMAUDIOSTREAM pHstStream = drvAudioGetHostStream(pStream);
    AssertPtr(pHstStream);
    PPDMAUDIOSTREAM pGstStream = pHstStream->pPair;
    AssertPtr(pGstStream);

    int rc;

    /* Whether to try closing a pending to close stream. */
    bool fTryClosePending = false;

    do
    {
        uint32_t cSamplesMixed = 0;

        rc = pThis->pHostDrvAudio->pfnStreamIterate(pThis->pHostDrvAudio, pHstStream);
        if (RT_FAILURE(rc))
            break;

        if (pHstStream->enmDir == PDMAUDIODIR_IN)
        {
            /* Has the host captured any samples which were not mixed to the guest side yet? */
            uint32_t cSamplesCaptured = AudioMixBufUsed(&pHstStream->MixBuf);

            Log3Func(("[%s] %RU32 samples captured\n", pHstStream->szName, cSamplesCaptured));

            if (cSamplesCaptured)
            {
                /* When capturing samples, the guest is the parent while the host is the child.
                 * So try mixing not yet mixed host-side samples to the guest-side buffer. */
                rc = AudioMixBufMixToParent(&pHstStream->MixBuf, cSamplesCaptured, &cSamplesMixed);
                if (   RT_SUCCESS(rc)
                    && cSamplesMixed)
                {
                    Log3Func(("[%s] %RU32 captured samples mixed\n", pHstStream->szName, cSamplesMixed));
                }
            }
            else
            {
                fTryClosePending = true;
            }
        }
        else if (pHstStream->enmDir == PDMAUDIODIR_OUT)
        {
            /* When playing samples, the host is the parent while the guest is the child.
             * So try mixing not yet mixed guest-side samples to the host-side buffer. */
            rc = AudioMixBufMixToParent(&pGstStream->MixBuf, AudioMixBufUsed(&pGstStream->MixBuf), &cSamplesMixed);
            if (   RT_SUCCESS(rc)
                && cSamplesMixed)
            {
                Log3Func(("[%s] %RU32 samples mixed, guest has %RU32 samples left (%RU32 live)\n",
                          pHstStream->szName, cSamplesMixed,
                          AudioMixBufUsed(&pGstStream->MixBuf), AudioMixBufLive(&pGstStream->MixBuf)));
            }

            uint32_t cSamplesLeft = AudioMixBufUsed(&pGstStream->MixBuf);
            if (!cSamplesLeft) /* No samples (anymore)? */
            {
                fTryClosePending = true;
            }
        }
        else
            AssertFailedStmt(rc = VERR_NOT_IMPLEMENTED);

        if (fTryClosePending)
        {
            /* Has the host stream marked as disabled but there still were guest streams relying
             * on it? Check if the stream now can be closed and do so, if possible. */
            if (pHstStream->fStatus & PDMAUDIOSTRMSTS_FLAG_PENDING_DISABLE)
            {
                LogFunc(("[%s] Closing pending stream\n", pHstStream->szName));
                rc = drvAudioStreamControlInternalBackend(pThis, pHstStream, PDMAUDIOSTREAMCMD_DISABLE);
                if (RT_SUCCESS(rc))
                {
                    pHstStream->fStatus &= ~PDMAUDIOSTRMSTS_FLAG_PENDING_DISABLE;
                }
                else
                    LogFunc(("%s: Backend vetoed against closing output stream, rc=%Rrc\n", pHstStream->szName, rc));
            }
        }

    } while (0);

    /* Update timestamps. */
    pHstStream->tsLastIterateMS = RTTimeMilliTS();
    pGstStream->tsLastIterateMS = RTTimeMilliTS();

    if (RT_FAILURE(rc))
        LogFunc(("Failed with %Rrc\n", rc));

    return rc;
}

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamPlay}
 */
static DECLCALLBACK(int) drvAudioStreamPlay(PPDMIAUDIOCONNECTOR pInterface,
                                            PPDMAUDIOSTREAM pStream, uint32_t *pcSamplesPlayed)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    /* pcSamplesPlayed is optional. */

    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_FAILURE(rc))
        return rc;

    AssertMsg(pStream->enmDir == PDMAUDIODIR_OUT,
              ("Stream '%s' is not an output stream and therefore cannot be played back (direction is 0x%x)\n",
               pStream->szName, pStream->enmDir));

    uint32_t cSamplesPlayed = 0;

    do
    {
        if (!pThis->pHostDrvAudio)
        {
            rc = VERR_NOT_AVAILABLE;
            break;
        }

        /* Backend output (temporarily) disabled / unavailable? */
        if (   pThis->pHostDrvAudio->pfnGetStatus
            && pThis->pHostDrvAudio->pfnGetStatus(pThis->pHostDrvAudio, PDMAUDIODIR_OUT) != PDMAUDIOBACKENDSTS_RUNNING)
        {
            /* Pull the new config from the backend and check again. */
            rc = pThis->pHostDrvAudio->pfnGetConfig(pThis->pHostDrvAudio, &pThis->BackendCfg);
            AssertRC(rc);

            if (   !pThis->BackendCfg.cSinks
                || !pThis->BackendCfg.cMaxStreamsOut)
            {
                rc = VERR_NOT_AVAILABLE;
                break;
            }
        }

        PPDMAUDIOSTREAM pHstStream = drvAudioGetHostStream(pStream);
        AssertPtr(pHstStream);
        PPDMAUDIOSTREAM pGstStream = pHstStream->pPair;
        AssertPtr(pGstStream);

        AssertPtr(pThis->pHostDrvAudio->pfnStreamGetStatus);
        PDMAUDIOSTRMSTS strmSts = pThis->pHostDrvAudio->pfnStreamGetStatus(pThis->pHostDrvAudio, pHstStream);
        if (!(strmSts & PDMAUDIOSTRMSTS_FLAG_INITIALIZED))
        {
            LogFunc(("[%s] Backend not initialized (anymore), re-initializing ...\n", pHstStream->szName));
            rc = drvAudioStreamReInitInternal(pThis, pStream);
            if (RT_FAILURE(rc))
            {
                LogFunc(("[%s] Failed to re-initialize backend, rc=%Rrc\n", pHstStream->szName, rc));
                break;
            }
        }

        uint32_t cSamplesLive = AudioMixBufLive(&pGstStream->MixBuf);
        if (cSamplesLive)
        {
            if (   (strmSts & PDMAUDIOSTRMSTS_FLAG_INITIALIZED)
                && (strmSts & PDMAUDIOSTRMSTS_FLAG_DATA_WRITABLE))
            {
                AssertPtr(pThis->pHostDrvAudio->pfnStreamPlay);
                rc = pThis->pHostDrvAudio->pfnStreamPlay(pThis->pHostDrvAudio, pHstStream, &cSamplesPlayed);
                if (RT_FAILURE(rc))
                {
                    int rc2 = drvAudioStreamControlInternalBackend(pThis, pHstStream, PDMAUDIOSTREAMCMD_DISABLE);
                    AssertRC(rc2);
                }
                else
                {
#ifdef VBOX_WITH_STATISTICS
                    STAM_COUNTER_ADD(&pThis->Stats.TotalSamplesPlayed, cSamplesPlayed);
                    STAM_COUNTER_ADD(&pHstStream->Out.StatSamplesPlayed, cSamplesPlayed);
#endif
                    cSamplesLive = AudioMixBufLive(&pGstStream->MixBuf);
                }
            }
        }

        if (!cSamplesLive)
        {
            /* Has the host stream marked as disabled but there still were guest streams relying
             * on it? Check if the stream now can be closed and do so, if possible. */
            if (pHstStream->fStatus & PDMAUDIOSTRMSTS_FLAG_PENDING_DISABLE)
            {
                LogFunc(("[%s] Closing pending stream\n", pHstStream->szName));
                rc = drvAudioStreamControlInternalBackend(pThis, pHstStream, PDMAUDIOSTREAMCMD_DISABLE);
                if (RT_SUCCESS(rc))
                {
                    pHstStream->fStatus &= ~PDMAUDIOSTRMSTS_FLAG_PENDING_DISABLE;
                }
                else
                    LogFunc(("[%s] Backend vetoed against closing output stream, rc=%Rrc\n", pHstStream->szName, rc));
            }
        }

    } while (0);

    int rc2 = RTCritSectLeave(&pThis->CritSect);
    if (RT_SUCCESS(rc))
        rc = rc2;

    if (RT_SUCCESS(rc))
    {
        if (pcSamplesPlayed)
            *pcSamplesPlayed = cSamplesPlayed;
    }

    if (RT_FAILURE(rc))
        LogFlowFunc(("[%s] Failed with %Rrc\n", pStream->szName, rc));

    return rc;
}

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamCapture}
 */
static DECLCALLBACK(int) drvAudioStreamCapture(PPDMIAUDIOCONNECTOR pInterface,
                                               PPDMAUDIOSTREAM pStream, uint32_t *pcSamplesCaptured)
{
    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_FAILURE(rc))
        return rc;

    AssertMsg(pStream->enmDir == PDMAUDIODIR_IN,
              ("Stream '%s' is not an input stream and therefore cannot be captured (direction is 0x%x)\n",
               pStream->szName, pStream->enmDir));

    Log3Func(("[%s]\n", pStream->szName));

    uint32_t cSamplesCaptured = 0;

    do
    {
        /* Backend input (temporarily) disabled / unavailable? */
        if (pThis->pHostDrvAudio->pfnGetStatus(pThis->pHostDrvAudio, PDMAUDIODIR_IN) != PDMAUDIOBACKENDSTS_RUNNING)
        {
            rc = pThis->pHostDrvAudio->pfnGetConfig(pThis->pHostDrvAudio, &pThis->BackendCfg);
            AssertRC(rc);

            if (   !pThis->BackendCfg.cSources
                || !pThis->BackendCfg.cMaxStreamsIn)
            {
                rc = VERR_NOT_AVAILABLE;
                break;
            }
        }

        PPDMAUDIOSTREAM pHstStream = drvAudioGetHostStream(pStream);
        AssertPtr(pHstStream);
        PPDMAUDIOSTREAM pGstStream = pHstStream->pPair;
        AssertPtr(pGstStream);

        PDMAUDIOSTRMSTS strmSts = pThis->pHostDrvAudio->pfnStreamGetStatus(pThis->pHostDrvAudio, pHstStream);
        if (!(strmSts & PDMAUDIOSTRMSTS_FLAG_INITIALIZED))
        {
            LogFunc(("[%s] Backend not initialized (anymore), re-initializing ...\n", pHstStream->szName));
            rc = drvAudioStreamReInitInternal(pThis, pStream);
            break;
        }

        uint32_t cSamplesLive = AudioMixBufLive(&pGstStream->MixBuf);
        if (!cSamplesLive)
        {
            if (   (strmSts & PDMAUDIOSTRMSTS_FLAG_INITIALIZED)
                && (strmSts & PDMAUDIOSTRMSTS_FLAG_DATA_READABLE))
            {
                rc = pThis->pHostDrvAudio->pfnStreamCapture(pThis->pHostDrvAudio, pHstStream, &cSamplesCaptured);
                if (RT_FAILURE(rc))
                {
                    int rc2 = drvAudioStreamControlInternalBackend(pThis, pHstStream, PDMAUDIOSTREAMCMD_DISABLE);
                    AssertRC(rc2);
                }
                else
                {
#ifdef VBOX_WITH_STATISTICS
                    STAM_COUNTER_ADD(&pHstStream->In.StatSamplesCaptured, cSamplesCaptured);
#endif
                }
            }

            Log3Func(("[%s] strmSts=0x%x, cSamplesCaptured=%RU32, rc=%Rrc\n", pHstStream->szName, strmSts, cSamplesCaptured, rc));
        }

    } while (0);

    if (RT_SUCCESS(rc))
    {
        if (pcSamplesCaptured)
            *pcSamplesCaptured = cSamplesCaptured;
    }

    int rc2 = RTCritSectLeave(&pThis->CritSect);
    if (RT_SUCCESS(rc))
        rc = rc2;

    if (RT_FAILURE(rc))
        LogFlowFuncLeaveRC(rc);

    return rc;
}

#ifdef VBOX_WITH_AUDIO_CALLBACKS
/**
 * Duplicates an audio callback.
 *
 * @returns Pointer to duplicated callback, or NULL on failure.
 * @param   pCB                 Callback to duplicate.
 */
static PPDMAUDIOCALLBACK drvAudioCallbackDuplicate(PPDMAUDIOCALLBACK pCB)
{
    AssertPtrReturn(pCB, VERR_INVALID_POINTER);

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

/**
 * Destroys a given callback.
 *
 * @param   pCB                 Callback to destroy.
 */
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

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnRegisterCallbacks}
 */
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

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnCallback}
 */
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

/**
 * Initializes the host backend and queries its initial configuration.
 * If the host backend fails, VERR_AUDIO_BACKEND_INIT_FAILED will be returned.
 *
 * Note: As this routine is called when attaching to the device LUN in the
 *       device emulation, we either check for success or VERR_AUDIO_BACKEND_INIT_FAILED.
 *       Everything else is considered as fatal and must be handled separately in
 *       the device emulation!
 *
 * @return  IPRT status code.
 * @param   pThis               Driver instance to be called.
 * @param   pCfgHandle          CFGM configuration handle to use for this driver.
 */
static int drvAudioHostInit(PDRVAUDIO pThis, PCFGMNODE pCfgHandle)
{
    /* pCfgHandle is optional. */
    NOREF(pCfgHandle);
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    AssertPtr(pThis->pHostDrvAudio);
    int rc = pThis->pHostDrvAudio->pfnInit(pThis->pHostDrvAudio);
    if (RT_FAILURE(rc))
    {
        LogRel(("Audio: Initialization of host backend failed with %Rrc\n", rc));
        return VERR_AUDIO_BACKEND_INIT_FAILED;
    }

    /* Get the configuration data from backend. */
    rc = pThis->pHostDrvAudio->pfnGetConfig(pThis->pHostDrvAudio, &pThis->BackendCfg);
    if (RT_FAILURE(rc))
    {
        LogRel(("Audio: Getting host backend configuration failed with %Rrc\n", rc));
        return VERR_AUDIO_BACKEND_INIT_FAILED;
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

/**
 * Handles state changes for all audio streams.
 *
 * @param   pDrvIns             Pointer to driver instance.
 * @param   enmCmd              Stream command to set for all streams.
 */
static void drvAudioStateHandler(PPDMDRVINS pDrvIns, PDMAUDIOSTREAMCMD enmCmd)
{
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);
    PDRVAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVAUDIO);

    LogFlowFunc(("enmCmd=%RU32\n", enmCmd));

    if (!pThis->pHostDrvAudio)
        return;

    PPDMAUDIOSTREAM pHstStream;
    RTListForEach(&pThis->lstHstStreams, pHstStream, PDMAUDIOSTREAM, Node)
        drvAudioStreamControlInternalBackend(pThis, pHstStream, enmCmd);
}

/**
 * Intializes an audio driver instance.
 *
 * @returns IPRT status code.
 * @param   pDrvIns             Pointer to driver instance.
 * @param   pCfgHandle          CFGM handle to use for configuration.
 */
static int drvAudioInit(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle)
{
    AssertPtrReturn(pCfgHandle, VERR_INVALID_POINTER);
    AssertPtrReturn(pDrvIns,    VERR_INVALID_POINTER);

    PDRVAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVAUDIO);
    LogFlowFunc(("pThis=%p, pDrvIns=%p\n", pThis, pDrvIns));

    int rc = RTCritSectInit(&pThis->CritSect);

    /** @todo Add audio driver options. */

    /*
     * If everything went well, initialize the lower driver.
     */
    if (RT_SUCCESS(rc))
        rc = drvAudioHostInit(pThis, pCfgHandle);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamRead}
 */
static DECLCALLBACK(int) drvAudioStreamRead(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream,
                                            void *pvBuf, uint32_t cbBuf, uint32_t *pcbRead)
{
    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    if (!pStream)
    {
        if (pcbRead)
            *pcbRead = 0;
        return VINF_SUCCESS;
    }

    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    AssertReturn(cbBuf,    VERR_INVALID_PARAMETER);
    /* pcbWritten is optional. */

    AssertMsg(pStream->enmDir == PDMAUDIODIR_IN,
              ("Stream '%s' is not an input stream and therefore cannot be read from (direction is 0x%x)\n",
               pStream->szName, pStream->enmDir));

    uint32_t cbRead = 0;

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_FAILURE(rc))
        return rc;

    do
    {
        if (   pThis->pHostDrvAudio
            && pThis->pHostDrvAudio->pfnGetStatus
            && pThis->pHostDrvAudio->pfnGetStatus(pThis->pHostDrvAudio, PDMAUDIODIR_IN) != PDMAUDIOBACKENDSTS_RUNNING)
        {
            rc = VERR_NOT_AVAILABLE;
            break;
        }

        PPDMAUDIOSTREAM pHstStream = drvAudioGetHostStream(pStream);
        if (pHstStream)
        {
            rc = VERR_NOT_AVAILABLE;
            break;
        }

        PPDMAUDIOSTREAM pGstStream = pHstStream->pPair;

        AssertMsg(pHstStream->fStatus & PDMAUDIOSTRMSTS_FLAG_ENABLED,
                  ("Reading from disabled host input stream '%s' not possible\n", pHstStream->szName));

        pGstStream->In.tsLastReadMS = RTTimeMilliTS();

        Log3Func(("%s\n", pStream->szName));

        /*
         * Read from the parent buffer (that is, the guest buffer) which
         * should have the audio data in the format the guest needs.
         */
        uint32_t cRead;
        rc = AudioMixBufReadCirc(&pGstStream->MixBuf, pvBuf, cbBuf, &cRead);
        if (RT_SUCCESS(rc))
        {
            if (cRead)
            {
#ifdef VBOX_WITH_STATISTICS
                STAM_COUNTER_ADD(&pThis->Stats.TotalBytesRead, AUDIOMIXBUF_S2B(&pGstStream->MixBuf, cRead));
                STAM_COUNTER_ADD(&pGstStream->In.StatBytesTotalRead, AUDIOMIXBUF_S2B(&pGstStream->MixBuf, cRead));
#endif
                AudioMixBufFinish(&pGstStream->MixBuf, cRead);

                cbRead = AUDIOMIXBUF_S2B(&pGstStream->MixBuf, cRead);
            }
        }

        Log3Func(("cRead=%RU32 (%RU32 bytes), rc=%Rrc\n", cRead, AUDIOMIXBUF_S2B(&pGstStream->MixBuf, cRead), rc));

    } while (0);

    int rc2 = RTCritSectLeave(&pThis->CritSect);
    if (RT_SUCCESS(rc))
        rc = rc2;

    if (RT_SUCCESS(rc))
    {
        if (pcbRead)
            *pcbRead = cbRead;
    }

    return rc;
}

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamCreate}
 */
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
                LogFunc(("Warning: No more input streams free to use\n"));

            /* Validate backend configuration. */
            if (!pThis->BackendCfg.cbStreamIn)
            {
                LogFunc(("Backend input configuration not valid, bailing out\n"));
                RC_BREAK(VERR_INVALID_PARAMETER);
            }

            cbHstStrm = pThis->BackendCfg.cbStreamIn;
        }
        else /* Out */
        {
            if (!pThis->cStreamsFreeOut)
            {
                LogFlowFunc(("Maximum number of host output streams reached\n"));
                RC_BREAK(VERR_AUDIO_NO_FREE_OUTPUT_STREAMS);
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

        pHstStrm->enmCtx = PDMAUDIOSTREAMCTX_HOST;
        pHstStrm->enmDir = pCfgHost->enmDir;

        pGstStrm = (PPDMAUDIOSTREAM)RTMemAllocZ(sizeof(PDMAUDIOSTREAM));
        AssertPtrBreakStmt(pGstStrm, rc = VERR_NO_MEMORY);

        pGstStrm->enmCtx = PDMAUDIOSTREAMCTX_GUEST;
        pGstStrm->enmDir = pCfgGuest->enmDir;

        /*
         * Init host stream.
         */
        RTStrPrintf(pHstStrm->szName, RT_ELEMENTS(pHstStrm->szName), "%s (Host)",
                    strlen(pCfgHost->szName) ? pCfgHost->szName : "<Untitled>");

        pHstStrm->pPair = pGstStrm;

        /*
         * Init guest stream.
         */
        RTStrPrintf(pGstStrm->szName, RT_ELEMENTS(pGstStrm->szName), "%s (Guest)",
                    strlen(pCfgGuest->szName) ? pCfgGuest->szName : "<Untitled>");

        pGstStrm->fStatus = pHstStrm->fStatus; /* Reflect the host stream's status. */
        pGstStrm->pPair   = pHstStrm;

        /*
         * Try to init the rest.
         */
        rc = drvAudioStreamInitInternal(pThis, pHstStrm, pCfgHost, pCfgGuest);
        if (RT_FAILURE(rc))
        {
            LogFlowFunc(("Stream not available (yet)\n"));
            rc = VINF_SUCCESS;
        }

    } while (0);

#undef RC_BREAK

    if (RT_FAILURE(rc))
    {
        if (pGstStrm)
        {
            drvAudioStreamDestroyInternal(pThis, pGstStrm);
            pGstStrm = NULL;
        }

        if (pHstStrm)
        {
            drvAudioStreamDestroyInternal(pThis, pHstStrm);
            pHstStrm = NULL;
        }
    }
    else
    {
        /* Set initial reference counts. */
        RTListAppend(&pThis->lstGstStreams, &pGstStrm->Node);
        pGstStrm->cRefs = 1;

        RTListAppend(&pThis->lstHstStreams, &pHstStrm->Node);
        pHstStrm->cRefs = 1;

        if (pCfgHost->enmDir == PDMAUDIODIR_IN)
        {
            if (pThis->cStreamsFreeIn)
                pThis->cStreamsFreeIn--;
        }
        else /* Out */
        {
            if (pThis->cStreamsFreeOut)
                pThis->cStreamsFreeOut--;
        }

#ifdef VBOX_WITH_STATISTICS
        STAM_COUNTER_ADD(&pThis->Stats.TotalStreamsCreated, 1);
#endif
        /* Always return the guest-side part to the device emulation. */
        *ppStream = pGstStrm;
    }

    int rc2 = RTCritSectLeave(&pThis->CritSect);
    if (RT_SUCCESS(rc))
        rc = rc2;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnGetConfig}
 */
static DECLCALLBACK(int) drvAudioGetConfig(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOBACKENDCFG pCfg)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg,       VERR_INVALID_POINTER);

    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_FAILURE(rc))
        return rc;

    if (pThis->pHostDrvAudio)
    {
        if (pThis->pHostDrvAudio->pfnGetConfig)
            rc = pThis->pHostDrvAudio->pfnGetConfig(pThis->pHostDrvAudio, pCfg);
        else
            rc = VERR_NOT_SUPPORTED;
    }
    else
        AssertFailed();

    int rc2 = RTCritSectLeave(&pThis->CritSect);
    if (RT_SUCCESS(rc))
        rc = rc2;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnGetStatus}
 */
static DECLCALLBACK(PDMAUDIOBACKENDSTS) drvAudioGetStatus(PPDMIAUDIOCONNECTOR pInterface, PDMAUDIODIR enmDir)
{
    AssertPtrReturn(pInterface, PDMAUDIOBACKENDSTS_UNKNOWN);

    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);

    PDMAUDIOBACKENDSTS backendSts = PDMAUDIOBACKENDSTS_UNKNOWN;

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        if (   pThis->pHostDrvAudio
            && pThis->pHostDrvAudio->pfnGetStatus)
        {
             backendSts = pThis->pHostDrvAudio->pfnGetStatus(pThis->pHostDrvAudio, enmDir);
        }

        int rc2 = RTCritSectLeave(&pThis->CritSect);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    LogFlowFuncLeaveRC(rc);
    return backendSts;
}

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamGetReadable}
 */
static DECLCALLBACK(uint32_t) drvAudioStreamGetReadable(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream)
{
    AssertPtrReturn(pInterface, 0);
    AssertPtrReturn(pStream,    0);

    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);

    int rc2 = RTCritSectEnter(&pThis->CritSect);
    AssertRC(rc2);

    AssertMsg(pStream->enmDir == PDMAUDIODIR_IN, ("Can't read from a non-input stream\n"));

    PPDMAUDIOSTREAM pHstStream = drvAudioGetHostStream(pStream);
    if (!pHstStream) /* No host stream available? Bail out early. */
    {
        rc2 = RTCritSectLeave(&pThis->CritSect);
        AssertRC(rc2);

        return 0;
    }

    uint32_t cReadable = 0;

    PPDMAUDIOSTREAM pGstStream = pHstStream->pPair;
    if (pGstStream)
        cReadable = AudioMixBufLive(&pGstStream->MixBuf);

    Log3Func(("[%s] cbReadable=%RU32 (%zu bytes)\n", pHstStream->szName, cReadable,
              AUDIOMIXBUF_S2B(&pGstStream->MixBuf, cReadable)));

    rc2 = RTCritSectLeave(&pThis->CritSect);
    AssertRC(rc2);

    /* Return bytes instead of audio samples. */
    return AUDIOMIXBUF_S2B(&pGstStream->MixBuf, cReadable);
}

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamGetWritable}
 */
static DECLCALLBACK(uint32_t) drvAudioStreamGetWritable(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream)
{
    AssertPtrReturn(pInterface, 0);
    AssertPtrReturn(pStream,    0);

    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);

    int rc2 = RTCritSectEnter(&pThis->CritSect);
    AssertRC(rc2);

    AssertMsg(pStream->enmDir == PDMAUDIODIR_OUT, ("Can't write to a non-output stream\n"));

    PPDMAUDIOSTREAM pHstStream = drvAudioGetHostStream(pStream);
    if (!pHstStream) /* No host stream available? Bail out early. */
    {
        rc2 = RTCritSectLeave(&pThis->CritSect);
        AssertRC(rc2);

        AssertMsgFailed(("Guest stream '%s' does not have a host stream attached\n", pStream->szName));
        return 0;
    }

    PPDMAUDIOSTREAM pGstStream = pHstStream->pPair;
    AssertPtr(pGstStream);

    uint32_t cWritable = AudioMixBufFree(&pGstStream->MixBuf);

    Log3Func(("[%s] cWritable=%RU32 (%zu bytes)\n", pHstStream->szName, cWritable,
              AUDIOMIXBUF_S2B(&pGstStream->MixBuf, cWritable)));

    rc2 = RTCritSectLeave(&pThis->CritSect);
    AssertRC(rc2);

    /* Return bytes instead of audio samples. */
    return AUDIOMIXBUF_S2B(&pGstStream->MixBuf, cWritable);
}

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamGetStatus}
 */
static DECLCALLBACK(PDMAUDIOSTRMSTS) drvAudioStreamGetStatus(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream)
{
    AssertPtrReturn(pInterface, false);

    if (!pStream)
        return PDMAUDIOSTRMSTS_FLAG_NONE;

    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);

    int rc2 = RTCritSectEnter(&pThis->CritSect);
    AssertRC(rc2);

    PDMAUDIOSTRMSTS strmSts = PDMAUDIOSTRMSTS_FLAG_NONE;

    PPDMAUDIOSTREAM pHstStream = drvAudioGetHostStream(pStream);
    if (pHstStream)
    {
        strmSts = pHstStream->fStatus;
        Log3Func(("%s: strmSts=0x%x\n", pHstStream->szName, strmSts));
    }

    rc2 = RTCritSectLeave(&pThis->CritSect);
    AssertRC(rc2);

    return strmSts;
}

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamSetVolume}
 */
static DECLCALLBACK(int) drvAudioStreamSetVolume(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream, PPDMAUDIOVOLUME pVol)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    AssertPtrReturn(pVol,       VERR_INVALID_POINTER);

    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);

    LogFlowFunc(("%s: volL=%RU32, volR=%RU32, fMute=%RTbool\n", pStream->szName, pVol->uLeft, pVol->uRight, pVol->fMuted));

    PPDMAUDIOSTREAM pHstStream = drvAudioGetHostStream(pStream);
    PPDMAUDIOSTREAM pGstStream = pHstStream ? pHstStream->pPair : pStream;

    AudioMixBufSetVolume(&pHstStream->MixBuf, pVol);
    AudioMixBufSetVolume(&pGstStream->MixBuf, pVol);
    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMIAUDIOCONNECTOR,pfnStreamDestroy}
 */
static DECLCALLBACK(int) drvAudioStreamDestroy(PPDMIAUDIOCONNECTOR pInterface, PPDMAUDIOSTREAM pStream)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    PDRVAUDIO pThis = PDMIAUDIOCONNECTOR_2_DRVAUDIO(pInterface);

    int rc = RTCritSectEnter(&pThis->CritSect);
    AssertRC(rc);

    PDMAUDIODIR enmDir = pStream->enmDir;

    LogFlowFunc(("%s: cRefs=%RU32\n", pStream->szName, pStream->cRefs));
    if (pStream->cRefs > 1)
        rc = VERR_WRONG_ORDER;

    if (RT_SUCCESS(rc))
    {
        PPDMAUDIOSTREAM pHstStream = drvAudioGetHostStream(pStream);
        PPDMAUDIOSTREAM pGstStream = pHstStream ? pHstStream->pPair : pStream;

        LogFlowFunc(("\tHost : %s\n", pHstStream ? pHstStream->szName : "<None>"));
        LogFlowFunc(("\tGuest: %s\n", pGstStream ? pGstStream->szName : "<None>"));

        /* Should prevent double frees. */
        Assert(pHstStream != pGstStream);

        if (pHstStream)
        {
            pHstStream->pPair = NULL;
            RTListNodeRemove(&pHstStream->Node);
        }

        if (pGstStream)
        {
            pGstStream->pPair = NULL;
            RTListNodeRemove(&pGstStream->Node);
        }

        if (pHstStream)
        {
            rc = drvAudioStreamDestroyInternal(pThis, pHstStream);
            AssertRC(rc);

            pHstStream = NULL;
        }

        if (pGstStream)
        {
            rc = drvAudioStreamDestroyInternal(pThis, pGstStream);
            AssertRC(rc);

            pGstStream = NULL;
        }
    }

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

    int rc2 = RTCritSectLeave(&pThis->CritSect);
    if (RT_SUCCESS(rc))
        rc = rc2;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Calls the backend to give it the chance to destroy its part of the audio stream.
 *
 * @returns IPRT status code.
 * @param   pThis               Pointer to driver instance.
 * @param   pHstStream          Host audio stream to call the backend destruction for.
 */
static int drvAudioStreamDestroyInternalBackend(PDRVAUDIO pThis, PPDMAUDIOSTREAM pHstStream)
{
    AssertPtrReturn(pThis,      VERR_INVALID_POINTER);
    AssertPtrReturn(pHstStream, VERR_INVALID_POINTER);

    AssertMsg(pHstStream->enmCtx == PDMAUDIOSTREAMCTX_HOST,
              ("Stream '%s' is not a host stream and therefore has no backend\n", pHstStream->szName));

    int rc = VINF_SUCCESS;

    LogFlowFunc(("%s: fStatus=0x%x\n", pHstStream->szName, pHstStream->fStatus));

    if (pHstStream->fStatus & PDMAUDIOSTRMSTS_FLAG_INITIALIZED)
    {
        /* Check if the pointer to  the host audio driver is still valid.
         * It can be NULL if we were called in drvAudioDestruct, for example. */
        if (pThis->pHostDrvAudio)
            rc = pThis->pHostDrvAudio->pfnStreamDestroy(pThis->pHostDrvAudio, pHstStream);
        if (RT_SUCCESS(rc))
            pHstStream->fStatus &= ~PDMAUDIOSTRMSTS_FLAG_INITIALIZED;
    }

    LogFlowFunc(("%s: Returning %Rrc\n", pHstStream->szName, rc));
    return rc;
}

/**
 * Destroys an audio stream.
 *
 * @returns IPRT status code.
 * @param   pThis               Pointer to driver instance.
 * @param   pStream             Pointer to audio stream to destroy.
 *                              That pointer will be invalid after successful destruction.
 */
static int drvAudioStreamDestroyInternal(PDRVAUDIO pThis, PPDMAUDIOSTREAM pStream)
{
    AssertPtrReturn(pThis,   VERR_INVALID_POINTER);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);

    LogFlowFunc(("%s: cRefs=%RU32\n", pStream->szName, pStream->cRefs));

    if (pStream->cRefs > 1)
        return VERR_WRONG_ORDER;

    int rc = VINF_SUCCESS;

    if (pStream->enmCtx == PDMAUDIOSTREAMCTX_GUEST)
    {
        if (pStream->fStatus & PDMAUDIOSTRMSTS_FLAG_INITIALIZED)
        {
            rc = drvAudioStreamControlInternal(pThis, pStream, PDMAUDIOSTREAMCMD_DISABLE);
            if (RT_SUCCESS(rc))
                pStream->fStatus &= ~PDMAUDIOSTRMSTS_FLAG_INITIALIZED;
        }

        if (pStream->enmDir == PDMAUDIODIR_IN)
        {
#ifdef VBOX_WITH_STATISTICS
            PDMDrvHlpSTAMDeregister(pThis->pDrvIns, &pStream->In.StatBytesElapsed);
            PDMDrvHlpSTAMDeregister(pThis->pDrvIns, &pStream->In.StatBytesTotalRead);
#endif
        }
        else if (pStream->enmDir == PDMAUDIODIR_OUT)
        {
#ifdef VBOX_WITH_STATISTICS
            PDMDrvHlpSTAMDeregister(pThis->pDrvIns, &pStream->Out.StatBytesElapsed);
            PDMDrvHlpSTAMDeregister(pThis->pDrvIns, &pStream->Out.StatBytesTotalWritten);
#endif
        }
    }
    else if (pStream->enmCtx == PDMAUDIOSTREAMCTX_HOST)
    {
        rc = drvAudioStreamDestroyInternalBackend(pThis, pStream);

        if (pStream->enmDir == PDMAUDIODIR_IN)
        {
#ifdef VBOX_WITH_STATISTICS
            PDMDrvHlpSTAMDeregister(pThis->pDrvIns, &pStream->In.StatSamplesCaptured);
#endif
        }
        else if (pStream->enmDir == PDMAUDIODIR_OUT)
        {
#ifdef VBOX_WITH_STATISTICS
            PDMDrvHlpSTAMDeregister(pThis->pDrvIns, &pStream->Out.StatSamplesPlayed);
#endif
        }
    }
    else
        AssertFailedReturn(VERR_NOT_IMPLEMENTED);

    if (RT_SUCCESS(rc))
    {
        /* Destroy mixing buffer. */
        AudioMixBufDestroy(&pStream->MixBuf);

        if (pStream)
        {
            RTMemFree(pStream);
            pStream = NULL;
        }
    }

    LogFlowFunc(("Returning %Rrc\n", rc));
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

    /* Just destroy the host stream on the backend side.
     * The rest will either be destructed by the device emulation or
     * in drvAudioDestruct(). */
    PPDMAUDIOSTREAM pStream;
    RTListForEach(&pThis->lstHstStreams, pStream, PDMAUDIOSTREAM, Node)
        drvAudioStreamDestroyInternalBackend(pThis, pStream);

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

    RTListInit(&pThis->lstHstStreams);
    RTListInit(&pThis->lstGstStreams);
#ifdef VBOX_WITH_AUDIO_CALLBACKS
    RTListInit(&pThis->lstCBIn);
    RTListInit(&pThis->lstCBOut);
#endif

    /*
     * Init the static parts.
     */
    pThis->pDrvIns                              = pDrvIns;
    /* IBase. */
    pDrvIns->IBase.pfnQueryInterface            = drvAudioQueryInterface;
    /* IAudioConnector. */
    pThis->IAudioConnector.pfnGetConfig         = drvAudioGetConfig;
    pThis->IAudioConnector.pfnGetStatus         = drvAudioGetStatus;
    pThis->IAudioConnector.pfnStreamCreate      = drvAudioStreamCreate;
    pThis->IAudioConnector.pfnStreamDestroy     = drvAudioStreamDestroy;
    pThis->IAudioConnector.pfnStreamAddRef      = drvAudioStreamAddRef;
    pThis->IAudioConnector.pfnStreamRelease     = drvAudioStreamRelease;
    pThis->IAudioConnector.pfnStreamControl     = drvAudioStreamControl;
    pThis->IAudioConnector.pfnStreamRead        = drvAudioStreamRead;
    pThis->IAudioConnector.pfnStreamWrite       = drvAudioStreamWrite;
    pThis->IAudioConnector.pfnStreamIterate     = drvAudioStreamIterate;
    pThis->IAudioConnector.pfnStreamGetReadable = drvAudioStreamGetReadable;
    pThis->IAudioConnector.pfnStreamGetWritable = drvAudioStreamGetWritable;
    pThis->IAudioConnector.pfnStreamGetStatus   = drvAudioStreamGetStatus;
    pThis->IAudioConnector.pfnStreamSetVolume   = drvAudioStreamSetVolume;
    pThis->IAudioConnector.pfnStreamPlay        = drvAudioStreamPlay;
    pThis->IAudioConnector.pfnStreamCapture     = drvAudioStreamCapture;
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

    rc = drvAudioInit(pDrvIns, pCfgHandle);
    if (RT_SUCCESS(rc))
    {
        pThis->fTerminate = false;
        pThis->pDrvIns    = pDrvIns;

#ifdef VBOX_WITH_STATISTICS
        PDMDrvHlpSTAMRegCounterEx(pDrvIns, &pThis->Stats.TotalStreamsActive,   "TotalStreamsActive",
                                  STAMUNIT_COUNT, "Active input streams.");
        PDMDrvHlpSTAMRegCounterEx(pDrvIns, &pThis->Stats.TotalStreamsCreated,  "TotalStreamsCreated",
                                  STAMUNIT_COUNT, "Total created input streams.");
        PDMDrvHlpSTAMRegCounterEx(pDrvIns, &pThis->Stats.TotalSamplesPlayed,   "TotalSamplesPlayed",
                                  STAMUNIT_COUNT, "Total samples played.");
        PDMDrvHlpSTAMRegCounterEx(pDrvIns, &pThis->Stats.TotalSamplesCaptured, "TotalSamplesCaptured",
                                  STAMUNIT_COUNT, "Total samples captured.");
        PDMDrvHlpSTAMRegCounterEx(pDrvIns, &pThis->Stats.TotalBytesRead,       "TotalBytesRead",
                                  STAMUNIT_BYTES, "Total bytes read.");
        PDMDrvHlpSTAMRegCounterEx(pDrvIns, &pThis->Stats.TotalBytesWritten,    "TotalBytesWritten",
                                  STAMUNIT_BYTES, "Total bytes written.");
#endif
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

    /* Thus, NULL the pointer to the host audio driver first,
     * so that routines like drvAudioStreamDestroyInternal() don't call the driver(s) below us anymore. */
    pThis->pHostDrvAudio = NULL;

    PPDMAUDIOSTREAM pStream, pStreamNext;
    RTListForEachSafe(&pThis->lstHstStreams, pStream, pStreamNext, PDMAUDIOSTREAM, Node)
        drvAudioStreamDestroyInternal(pThis, pStream);

    RTListForEachSafe(&pThis->lstGstStreams, pStream, pStreamNext, PDMAUDIOSTREAM, Node)
        drvAudioStreamDestroyInternal(pThis, pStream);

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
    UINT32_MAX,
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

