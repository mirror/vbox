/* $Id$ */
/** @file
 * OSS (Open Sound System) host audio backend.
 */

/*
 * Copyright (C) 2014-2016 Oracle Corporation
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
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/soundcard.h>
#include <unistd.h>

#include <iprt/alloc.h>
#include <iprt/uuid.h> /* For PDMIBASE_2_PDMDRV. */

#define LOG_GROUP LOG_GROUP_DRV_HOST_AUDIO
#include <VBox/log.h>
#include <VBox/vmm/pdmaudioifs.h>

#include "DrvAudio.h"
#include "AudioMixBuffer.h"

#include "VBoxDD.h"


/*********************************************************************************************************************************
*   Defines                                                                                                                      *
*********************************************************************************************************************************/

#if ((SOUND_VERSION > 360) && (defined(OSS_SYSINFO)))
/* OSS > 3.6 has a new syscall available for querying a bit more detailed information
 * about OSS' audio capabilities. This is handy for e.g. Solaris. */
# define VBOX_WITH_OSS_SYSINFO 1
#endif

/** Makes DRVHOSTOSSAUDIO out of PDMIHOSTAUDIO. */
#define PDMIHOSTAUDIO_2_DRVHOSTOSSAUDIO(pInterface) \
    ( (PDRVHOSTOSSAUDIO)((uintptr_t)pInterface - RT_OFFSETOF(DRVHOSTOSSAUDIO, IHostAudio)) )


/*********************************************************************************************************************************
*   Structures                                                                                                                   *
*********************************************************************************************************************************/

/**
 * OSS host audio driver instance data.
 * @implements PDMIAUDIOCONNECTOR
 */
typedef struct DRVHOSTOSSAUDIO
{
    /** Pointer to the driver instance structure. */
    PPDMDRVINS         pDrvIns;
    /** Pointer to host audio interface. */
    PDMIHOSTAUDIO      IHostAudio;
    /** Error count for not flooding the release log.
     *  UINT32_MAX for unlimited logging. */
    uint32_t           cLogErrors;
} DRVHOSTOSSAUDIO, *PDRVHOSTOSSAUDIO;

typedef struct OSSAUDIOSTREAMCFG
{
    PDMAUDIOFMT       enmFormat;
    PDMAUDIOENDIANNESS enmENDIANNESS;
    uint16_t          uFreq;
    uint8_t           cChannels;
    uint16_t          cFragments;
    uint32_t          cbFragmentSize;
} OSSAUDIOSTREAMCFG, *POSSAUDIOSTREAMCFG;

typedef struct OSSAUDIOSTREAMIN
{
    /** Note: Always must come first! */
    PDMAUDIOSTREAM     pStreamIn;
    int                hFile;
    int                cFragments;
    int                cbFragmentSize;
    /** Own PCM buffer. */
    void              *pvBuf;
    /** Size (in bytes) of own PCM buffer. */
    size_t             cbBuf;
    int                old_optr;
} OSSAUDIOSTREAMIN, *POSSAUDIOSTREAMIN;

typedef struct OSSAUDIOSTREAMOUT
{
    /** Note: Always must come first! */
    PDMAUDIOSTREAM      pStreamOut;
    int                 hFile;
    int                 cFragments;
    int                 cbFragmentSize;
#ifndef RT_OS_L4
    /** Whether we use a memory mapped file instead of our
     *  own allocated PCM buffer below. */
    bool                fMemMapped;
#endif
    /** Own PCM buffer in case memory mapping is unavailable. */
    void               *pvBuf;
    /** Size (in bytes) of own PCM buffer. */
    size_t              cbBuf;
    int                 old_optr;
} OSSAUDIOSTREAMOUT, *POSSAUDIOSTREAMOUT;

typedef struct OSSAUDIOCFG
{
#ifndef RT_OS_L4
    bool try_mmap;
#endif
    int nfrags;
    int fragsize;
    const char *devpath_out;
    const char *devpath_in;
    int debug;
} OSSAUDIOCFG, *POSSAUDIOCFG;

static OSSAUDIOCFG s_OSSConf =
{
#ifndef RT_OS_L4
    false,
#endif
    4,
    4096,
    "/dev/dsp",
    "/dev/dsp",
    0
};


/* http://www.df.lth.se/~john_e/gems/gem002d.html */
static uint32_t popcount(uint32_t u)
{
    u = ((u&0x55555555) + ((u>>1)&0x55555555));
    u = ((u&0x33333333) + ((u>>2)&0x33333333));
    u = ((u&0x0f0f0f0f) + ((u>>4)&0x0f0f0f0f));
    u = ((u&0x00ff00ff) + ((u>>8)&0x00ff00ff));
    u = ( u&0x0000ffff) + (u>>16);
    return u;
}

static uint32_t lsbindex(uint32_t u)
{
    return popcount ((u&-u)-1);
}

static int ossAudioFmtToOSS(PDMAUDIOFMT fmt)
{
    switch (fmt)
    {
        case PDMAUDIOFMT_S8:
            return AFMT_S8;

        case PDMAUDIOFMT_U8:
            return AFMT_U8;

        case PDMAUDIOFMT_S16:
            return AFMT_S16_LE;

        case PDMAUDIOFMT_U16:
            return AFMT_U16_LE;

        default:
            break;
    }

    AssertMsgFailed(("Format %ld not supported\n", fmt));
    return AFMT_U8;
}

static int ossOSSToAudioFmt(int fmt, PDMAUDIOFMT *pFmt, PDMAUDIOENDIANNESS *pENDIANNESS)
{
    switch (fmt)
    {
        case AFMT_S8:
            *pFmt = PDMAUDIOFMT_S8;
            if (pENDIANNESS)
                *pENDIANNESS = PDMAUDIOENDIANNESS_LITTLE;
            break;

        case AFMT_U8:
            *pFmt = PDMAUDIOFMT_U8;
            if (pENDIANNESS)
                *pENDIANNESS = PDMAUDIOENDIANNESS_LITTLE;
            break;

        case AFMT_S16_LE:
            *pFmt = PDMAUDIOFMT_S16;
            if (pENDIANNESS)
                *pENDIANNESS = PDMAUDIOENDIANNESS_LITTLE;
            break;

        case AFMT_U16_LE:
            *pFmt = PDMAUDIOFMT_U16;
            if (pENDIANNESS)
                *pENDIANNESS = PDMAUDIOENDIANNESS_LITTLE;
            break;

        case AFMT_S16_BE:
            *pFmt = PDMAUDIOFMT_S16;
            if (pENDIANNESS)
                *pENDIANNESS = PDMAUDIOENDIANNESS_BIG;
            break;

        case AFMT_U16_BE:
            *pFmt = PDMAUDIOFMT_U16;
            if (pENDIANNESS)
                *pENDIANNESS = PDMAUDIOENDIANNESS_BIG;
            break;

        default:
            AssertMsgFailed(("Format %ld not supported\n", fmt));
            return VERR_NOT_SUPPORTED;
    }

    return VINF_SUCCESS;
}

static int ossStreamClose(int *phFile)
{
    if (!phFile || !*phFile)
        return VINF_SUCCESS;

    int rc;
    if (close(*phFile))
    {
        LogRel(("OSS: Closing stream failed: %s\n", strerror(errno)));
        rc = VERR_GENERAL_FAILURE; /** @todo */
    }
    else
    {
        *phFile = -1;
        rc = VINF_SUCCESS;
    }

    return rc;
}

static int ossStreamOpen(const char *pszDev, int fOpen, POSSAUDIOSTREAMCFG pReq, POSSAUDIOSTREAMCFG pObt, int *phFile)
{
    AssertPtrReturn(pszDev, VERR_INVALID_POINTER);
    AssertPtrReturn(pReq,   VERR_INVALID_POINTER);
    AssertPtrReturn(pObt,   VERR_INVALID_POINTER);
    AssertPtrReturn(phFile, VERR_INVALID_POINTER);

    int rc;

    int hFile = -1;
    do
    {
        hFile = open(pszDev, fOpen);
        if (hFile == -1)
        {
            LogRel(("OSS: Failed to open %s: %s (%d)\n", pszDev, strerror(errno), errno));
            rc = RTErrConvertFromErrno(errno);
            break;
        }

        int iFormat = ossAudioFmtToOSS(pReq->enmFormat);
        if (ioctl(hFile, SNDCTL_DSP_SAMPLESIZE, &iFormat))
        {
            LogRel(("OSS: Failed to set audio format to %ld: %s (%d)\n", iFormat, strerror(errno), errno));
            rc = RTErrConvertFromErrno(errno);
            break;
        }

        int cChannels = pReq->cChannels;
        if (ioctl(hFile, SNDCTL_DSP_CHANNELS, &cChannels))
        {
            LogRel(("OSS: Failed to set number of audio channels (%d): %s (%d)\n", pReq->cChannels, strerror(errno), errno));
            rc = RTErrConvertFromErrno(errno);
            break;
        }

        int freq = pReq->uFreq;
        if (ioctl(hFile, SNDCTL_DSP_SPEED, &freq))
        {
            LogRel(("OSS: Failed to set audio frequency (%dHZ): %s (%d)\n", pReq->uFreq, strerror(errno), errno));
            rc = RTErrConvertFromErrno(errno);
            break;
        }

        /* Obsolete on Solaris (using O_NONBLOCK is sufficient). */
#if !(defined(VBOX) && defined(RT_OS_SOLARIS))
        if (ioctl(hFile, SNDCTL_DSP_NONBLOCK))
        {
            LogRel(("OSS: Failed to set non-blocking mode: %s (%d)\n", strerror(errno), errno));
            rc = RTErrConvertFromErrno(errno);
            break;
        }
#endif
        int mmmmssss = (pReq->cFragments << 16) | lsbindex(pReq->cbFragmentSize);
        if (ioctl(hFile, SNDCTL_DSP_SETFRAGMENT, &mmmmssss))
        {
            LogRel(("OSS: Failed to set %RU16 fragments to %RU32 bytes each: %s (%d)\n",
                    pReq->cFragments, pReq->cbFragmentSize, strerror(errno), errno));
            rc = RTErrConvertFromErrno(errno);
            break;
        }

        audio_buf_info abinfo;
        if (ioctl(hFile, (fOpen & O_RDONLY) ? SNDCTL_DSP_GETISPACE : SNDCTL_DSP_GETOSPACE, &abinfo))
        {
            LogRel(("OSS: Failed to retrieve buffer length: %s (%d)\n", strerror(errno), errno));
            rc = RTErrConvertFromErrno(errno);
            break;
        }

        rc = ossOSSToAudioFmt(iFormat, &pObt->enmFormat, &pObt->enmENDIANNESS);
        if (RT_SUCCESS(rc))
        {
            pObt->cChannels      = cChannels;
            pObt->uFreq          = freq;
            pObt->cFragments     = abinfo.fragstotal;
            pObt->cbFragmentSize = abinfo.fragsize;

            *phFile = hFile;
        }
    }
    while (0);

    if (RT_FAILURE(rc))
        ossStreamClose(&hFile);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int ossControlStreamIn(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream,
                              PDMAUDIOSTREAMCMD enmStreamCmd)
{
    NOREF(pInterface);
    NOREF(pStream);
    NOREF(enmStreamCmd);

    /** @todo Nothing to do here right now!? */

    return VINF_SUCCESS;
}

static int ossControlStreamOut(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream,
                               PDMAUDIOSTREAMCMD enmStreamCmd)
{
    NOREF(pInterface);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);

    POSSAUDIOSTREAMOUT pThisStream = (POSSAUDIOSTREAMOUT)pStream;

#ifdef RT_OS_L4
    return VINF_SUCCESS;
#else
    if (!pThisStream->fMemMapped)
        return VINF_SUCCESS;
#endif

    int rc = VINF_SUCCESS;
    int mask;
    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
        case PDMAUDIOSTREAMCMD_RESUME:
        {
            DrvAudioHlpClearBuf(&pStream->Props,
                             pThisStream->pvBuf, pThisStream->cbBuf, AudioMixBufSize(&pStream->MixBuf));

            mask = PCM_ENABLE_OUTPUT;
            if (ioctl(pThisStream->hFile, SNDCTL_DSP_SETTRIGGER, &mask) < 0)
            {
                LogRel(("OSS: Failed to enable output stream: %s\n", strerror(errno)));
                rc = RTErrConvertFromErrno(errno);
            }

            break;
        }

        case PDMAUDIOSTREAMCMD_DISABLE:
        case PDMAUDIOSTREAMCMD_PAUSE:
        {
            mask = 0;
            if (ioctl(pThisStream->hFile, SNDCTL_DSP_SETTRIGGER, &mask) < 0)
            {
                LogRel(("OSS: Failed to disable output stream: %s\n", strerror(errno)));
                rc = RTErrConvertFromErrno(errno);
            }

            break;
        }

        default:
            AssertMsgFailed(("Invalid command %ld\n", enmStreamCmd));
            rc = VERR_INVALID_PARAMETER;
            break;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(int) drvHostOSSAudioInit(PPDMIHOSTAUDIO pInterface)
{
    NOREF(pInterface);

    LogFlowFuncEnter();

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvHostOSSAudioStreamCapture(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream,
                                                      uint32_t *pcSamplesCaptured)
{
    NOREF(pInterface);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);

    POSSAUDIOSTREAMIN pStrm = (POSSAUDIOSTREAMIN)pStream;

    int rc = VINF_SUCCESS;
    size_t cbToRead = RT_MIN(pStrm->cbBuf,
                             AudioMixBufFreeBytes(&pStream->MixBuf));

    LogFlowFunc(("cbToRead=%zu\n", cbToRead));

    uint32_t cWrittenTotal = 0;
    uint32_t cbTemp;
    ssize_t  cbRead;
    size_t   offWrite = 0;

    while (cbToRead)
    {
        cbTemp = RT_MIN(cbToRead, pStrm->cbBuf);
        AssertBreakStmt(cbTemp, rc = VERR_NO_DATA);
        cbRead = read(pStrm->hFile, (uint8_t *)pStrm->pvBuf + offWrite, cbTemp);

        LogFlowFunc(("cbRead=%zi, cbTemp=%RU32, cbToRead=%zu\n", cbRead, cbTemp, cbToRead));

        if (cbRead < 0)
        {
            switch (errno)
            {
                case 0:
                {
                    LogFunc(("Failed to read %z frames\n", cbRead));
                    rc = VERR_ACCESS_DENIED;
                    break;
                }

                case EINTR:
                case EAGAIN:
                    rc = VERR_NO_DATA;
                    break;

                default:
                    LogFlowFunc(("Failed to read %zu input frames, rc=%Rrc\n", cbTemp, rc));
                    rc = VERR_GENERAL_FAILURE; /** @todo Fix this. */
                    break;
            }

            if (RT_FAILURE(rc))
                break;
        }
        else if (cbRead)
        {
            uint32_t cWritten;
            rc = AudioMixBufWriteCirc(&pStream->MixBuf, pStrm->pvBuf, cbRead, &cWritten);
            if (RT_FAILURE(rc))
                break;

            uint32_t cbWritten = AUDIOMIXBUF_S2B(&pStream->MixBuf, cWritten);

            Assert(cbToRead >= cbWritten);
            cbToRead      -= cbWritten;
            offWrite      += cbWritten;
            cWrittenTotal += cWritten;
        }
        else /* No more data, try next round. */
            break;
    }

    if (rc == VERR_NO_DATA)
        rc = VINF_SUCCESS;

    if (RT_SUCCESS(rc))
    {
        uint32_t cProcessed = 0;
        if (cWrittenTotal)
            rc = AudioMixBufMixToParent(&pStream->MixBuf, cWrittenTotal, &cProcessed);

        if (pcSamplesCaptured)
            *pcSamplesCaptured = cWrittenTotal;

        LogFlowFunc(("cWrittenTotal=%RU32 (%RU32 processed), rc=%Rrc\n",
                     cWrittenTotal, cProcessed, rc));
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int ossDestroyStreamIn(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream)
{
    NOREF(pInterface);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);

    POSSAUDIOSTREAMIN pStrm = (POSSAUDIOSTREAMIN)pStream;

    LogFlowFuncEnter();

    if (pStrm->pvBuf)
    {
        Assert(pStrm->cbBuf);

        RTMemFree(pStrm->pvBuf);
        pStrm->pvBuf = NULL;
    }

    pStrm->cbBuf = 0;

    ossStreamClose(&pStrm->hFile);

    return VINF_SUCCESS;
}

static int ossDestroyStreamOut(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream)
{
    NOREF(pInterface);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);

    POSSAUDIOSTREAMOUT pStrm = (POSSAUDIOSTREAMOUT)pStream;

    LogFlowFuncEnter();

#ifndef RT_OS_L4
    if (pStrm->fMemMapped)
    {
        if (pStrm->pvBuf)
        {
            Assert(pStrm->cbBuf);

            int rc2 = munmap(pStrm->pvBuf, pStrm->cbBuf);
            if (rc2 == 0)
            {
                pStrm->pvBuf      = NULL;
                pStrm->cbBuf      = 0;

                pStrm->fMemMapped = false;
            }
            else
                LogRel(("OSS: Failed to memory unmap playback buffer on close: %s\n", strerror(errno)));
        }
    }
    else
    {
#endif
        if (pStrm->pvBuf)
        {
            Assert(pStrm->cbBuf);

            RTMemFree(pStrm->pvBuf);
            pStrm->pvBuf = NULL;
        }

        pStrm->cbBuf = 0;
#ifndef RT_OS_L4
    }
#endif

    ossStreamClose(&pStrm->hFile);

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvHostOSSAudioGetConfig(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDCFG pCfg)
{
    NOREF(pInterface);

    pCfg->cbStreamIn  = sizeof(OSSAUDIOSTREAMIN);
    pCfg->cbStreamOut = sizeof(OSSAUDIOSTREAMOUT);

    pCfg->cSources    = 0;
    pCfg->cSinks      = 0;

    int hFile = open("/dev/dsp", O_WRONLY | O_NONBLOCK, 0);
    if (hFile == -1)
    {
        /* Try opening the mixing device instead. */
        hFile = open("/dev/mixer", O_RDONLY | O_NONBLOCK, 0);
    }

    int ossVer = -1;

#ifdef VBOX_WITH_OSS_SYSINFO
    oss_sysinfo ossInfo;
    RT_ZERO(ossInfo);
#endif

    if (hFile != -1)
    {
        int err = ioctl(hFile, OSS_GETVERSION, &ossVer);
        if (err == 0)
        {
            LogRel2(("OSS: Using version: %d\n", ossVer));
#ifdef VBOX_WITH_OSS_SYSINFO
            err = ioctl(hFile, OSS_SYSINFO, &ossInfo);
            if (err == 0)
            {
                LogRel2(("OSS: Number of DSPs: %d\n", ossInfo.numaudios));
                LogRel2(("OSS: Number of mixers: %d\n", ossInfo.nummixers));

                int cDev = ossInfo.nummixers;
                if (!cDev)
                    cDev = ossInfo.numaudios;

                pCfg->cSources        = cDev;
                pCfg->cSinks          = cDev;

                pCfg->cMaxStreamsIn   = UINT32_MAX;
                pCfg->cMaxStreamsOut  = UINT32_MAX;
            }
            else
            {
#endif
                /* Since we cannot query anything, assume that we have at least
                 * one input and one output if we found "/dev/dsp" or "/dev/mixer". */
                pCfg->cSources        = 1;
                pCfg->cSinks          = 1;

                pCfg->cMaxStreamsIn   = UINT32_MAX;
                pCfg->cMaxStreamsOut  = UINT32_MAX;
#ifdef VBOX_WITH_OSS_SYSINFO
            }
#endif
        }
        else
            LogRel(("OSS: Unable to determine installed version: %s (%d)\n", strerror(err), err));
    }
    else
        LogRel(("OSS: No devices found, audio is not available\n"));

    return VINF_SUCCESS;
}

static int ossCreateStreamIn(PPDMIHOSTAUDIO pInterface,
                             PPDMAUDIOSTREAM pStream, PPDMAUDIOSTREAMCFG pCfg, uint32_t *pcSamples)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg,       VERR_INVALID_POINTER);

    PDRVHOSTOSSAUDIO  pThis = PDMIHOSTAUDIO_2_DRVHOSTOSSAUDIO(pInterface);
    POSSAUDIOSTREAMIN pStrm = (POSSAUDIOSTREAMIN)pStream;

    int rc;
    int hFile = -1;

    do
    {
        uint32_t cSamples;

        OSSAUDIOSTREAMCFG reqStream, obtStream;
        reqStream.enmFormat      = pCfg->enmFormat;
        reqStream.uFreq          = pCfg->uHz;
        reqStream.cChannels      = pCfg->cChannels;
        reqStream.cFragments     = s_OSSConf.nfrags;
        reqStream.cbFragmentSize = s_OSSConf.fragsize;

        rc = ossStreamOpen(s_OSSConf.devpath_in, O_RDONLY | O_NONBLOCK, &reqStream, &obtStream, &hFile);
        if (RT_SUCCESS(rc))
        {
            if (obtStream.cFragments * obtStream.cbFragmentSize & pStream->Props.uAlign)
                LogRel(("OSS: Warning: Misaligned capturing buffer: Size = %zu, Alignment = %u\n",
                        obtStream.cFragments * obtStream.cbFragmentSize,
                        pStream->Props.uAlign + 1));

            PDMAUDIOSTREAMCFG streamCfg;
            streamCfg.enmFormat     = obtStream.enmFormat;
            streamCfg.uHz           = obtStream.uFreq;
            streamCfg.cChannels     = pCfg->cChannels;
            streamCfg.enmEndianness = obtStream.enmENDIANNESS;

            rc = DrvAudioHlpStreamCfgToProps(&streamCfg, &pStream->Props);
            if (RT_SUCCESS(rc))
            {
                cSamples = (obtStream.cFragments * obtStream.cbFragmentSize)
                           >> pStream->Props.cShift;
                if (!cSamples)
                    rc = VERR_INVALID_PARAMETER;
            }
        }

        if (RT_SUCCESS(rc))
        {
            size_t cbSample = (1 << pStream->Props.cShift);

            size_t cbBuf = cSamples * cbSample;
            void  *pvBuf = RTMemAlloc(cbBuf);
            if (!pvBuf)
            {
                LogRel(("OSS: Failed allocating capturing buffer with %RU32 samples (%zu bytes per sample)\n",
                        cSamples, cbSample));
                rc = VERR_NO_MEMORY;
                break;
            }

            pStrm->hFile = hFile;
            pStrm->pvBuf = pvBuf;
            pStrm->cbBuf = cbBuf;

            if (pcSamples)
                *pcSamples = cSamples;
        }

    } while (0);

    if (RT_FAILURE(rc))
        ossStreamClose(&hFile);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int ossCreateStreamOut(PPDMIHOSTAUDIO pInterface,
                              PPDMAUDIOSTREAM pStream, PPDMAUDIOSTREAMCFG pCfg,
                              uint32_t *pcSamples)
{
    AssertPtrReturn(pInterface,  VERR_INVALID_POINTER);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg,        VERR_INVALID_POINTER);

    PDRVHOSTOSSAUDIO   pThis = PDMIHOSTAUDIO_2_DRVHOSTOSSAUDIO(pInterface);
    POSSAUDIOSTREAMOUT pStrm = (POSSAUDIOSTREAMOUT)pStream;

    int rc;
    int hFile = -1;

    do
    {
        uint32_t cSamples;

        OSSAUDIOSTREAMCFG reqStream, obtStream;
        reqStream.enmFormat      = pCfg->enmFormat;
        reqStream.uFreq          = pCfg->uHz;
        reqStream.cChannels      = pCfg->cChannels;
        reqStream.cFragments     = s_OSSConf.nfrags;
        reqStream.cbFragmentSize = s_OSSConf.fragsize;

        rc = ossStreamOpen(s_OSSConf.devpath_out, O_WRONLY | O_NONBLOCK, &reqStream, &obtStream, &hFile);
        if (RT_SUCCESS(rc))
        {
            if (obtStream.cFragments * obtStream.cbFragmentSize & pStream->Props.uAlign)
                LogRel(("OSS: Warning: Misaligned playback buffer: Size = %zu, Alignment = %u\n",
                        obtStream.cFragments * obtStream.cbFragmentSize,
                        pStream->Props.uAlign + 1));

            PDMAUDIOSTREAMCFG streamCfg;
            streamCfg.enmFormat     = obtStream.enmFormat;
            streamCfg.uHz           = obtStream.uFreq;
            streamCfg.cChannels     = pCfg->cChannels;
            streamCfg.enmEndianness = obtStream.enmENDIANNESS;

            rc = DrvAudioHlpStreamCfgToProps(&streamCfg, &pStream->Props);
            if (RT_SUCCESS(rc))
                cSamples = (obtStream.cFragments * obtStream.cbFragmentSize)
                           >> pStream->Props.cShift;
        }

        if (RT_SUCCESS(rc))
        {
            pStrm->fMemMapped = false;

            size_t cbSamples =  cSamples << pStream->Props.cShift;
            Assert(cbSamples);

#ifndef RT_OS_L4
            if (s_OSSConf.try_mmap)
            {
                pStrm->pvBuf = mmap(0, cbSamples, PROT_READ | PROT_WRITE, MAP_SHARED, hFile, 0);
                if (pStrm->pvBuf == MAP_FAILED)
                {
                    LogRel(("OSS: Failed to memory map %zu bytes of playback buffer: %s\n", cbSamples, strerror(errno)));
                    rc = RTErrConvertFromErrno(errno);
                    break;
                }
                else
                {
                    int mask = 0;
                    if (ioctl(hFile, SNDCTL_DSP_SETTRIGGER, &mask) < 0)
                    {
                        LogRel(("OSS: Failed to retrieve initial trigger mask for playback buffer: %s\n", strerror(errno)));
                        rc = RTErrConvertFromErrno(errno);
                        /* Note: No break here, need to unmap file first! */
                    }
                    else
                    {
                        mask = PCM_ENABLE_OUTPUT;
                        if (ioctl (hFile, SNDCTL_DSP_SETTRIGGER, &mask) < 0)
                        {
                            LogRel(("OSS: Failed to retrieve PCM_ENABLE_OUTPUT mask: %s\n", strerror(errno)));
                            rc = RTErrConvertFromErrno(errno);
                            /* Note: No break here, need to unmap file first! */
                        }
                        else
                            pStrm->fMemMapped = true;
                    }

                    if (RT_FAILURE(rc))
                    {
                        int rc2 = munmap(pStrm->pvBuf, cbSamples);
                        if (rc2)
                            LogRel(("OSS: Failed to memory unmap playback buffer: %s\n", strerror(errno)));
                        break;
                    }
                }
            }
#endif /* !RT_OS_L4 */

            /* Memory mapping failed above? Try allocating an own buffer. */
#ifndef RT_OS_L4
            if (!pStrm->fMemMapped)
            {
#endif
                void *pvBuf = RTMemAlloc(cbSamples);
                if (!pvBuf)
                {
                    LogRel(("OSS: Failed allocating playback buffer with %RU32 samples (%zu bytes)\n", cSamples, cbSamples));
                    rc = VERR_NO_MEMORY;
                    break;
                }

                pStrm->hFile = hFile;
                pStrm->pvBuf = pvBuf;
                pStrm->cbBuf = cbSamples;
#ifndef RT_OS_L4
            }
#endif
            if (pcSamples)
                *pcSamples = cSamples;
        }

    } while (0);

    if (RT_FAILURE(rc))
        ossStreamClose(&hFile);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(bool) drvHostOSSAudioIsEnabled(PPDMIHOSTAUDIO pInterface, PDMAUDIODIR enmDir)
{
    NOREF(pInterface);
    NOREF(enmDir);
    return true; /* Always all enabled. */
}

static DECLCALLBACK(int) drvHostOSSAudioStreamPlay(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream,
                                                   uint32_t *pcSamplesPlayed)
{
    NOREF(pInterface);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);

    POSSAUDIOSTREAMOUT pStrm = (POSSAUDIOSTREAMOUT)pStream;

    int rc = VINF_SUCCESS;
    uint32_t cbReadTotal = 0;
    count_info cntinfo;

    do
    {
        size_t cbBuf = AudioMixBufSizeBytes(&pStream->MixBuf);

        uint32_t cLive = AudioMixBufLive(&pStream->MixBuf);
        uint32_t cToRead;

#ifndef RT_OS_L4
        if (pStrm->fMemMapped)
        {
            /* Get current playback pointer. */
            int rc2 = ioctl(pStrm->hFile, SNDCTL_DSP_GETOPTR, &cntinfo);
            if (!rc2)
            {
                LogRel(("OSS: Failed to retrieve current playback pointer: %s\n",
                        strerror(errno)));
                rc = RTErrConvertFromErrno(errno);
                break;
            }

            /* Nothing to play? */
            if (cntinfo.ptr == pStrm->old_optr)
                break;

            int cbData;
            if (cntinfo.ptr > pStrm->old_optr)
                cbData = cntinfo.ptr - pStrm->old_optr;
            else
                cbData = cbBuf + cntinfo.ptr - pStrm->old_optr;
            Assert(cbData);

            cToRead = RT_MIN((uint32_t)AUDIOMIXBUF_B2S(&pStream->MixBuf, cbData),
                             cLive);
        }
        else
        {
#endif
            audio_buf_info abinfo;
            int rc2 = ioctl(pStrm->hFile, SNDCTL_DSP_GETOSPACE, &abinfo);
            if (rc2 < 0)
            {
                LogRel(("OSS: Failed to retrieve current playback buffer: %s\n", strerror(errno)));
                rc = RTErrConvertFromErrno(errno);
                break;
            }

            if ((size_t)abinfo.bytes > cbBuf)
            {
                LogFlowFunc(("Warning: Invalid available size, size=%d, bufsize=%zu\n", abinfo.bytes, cbBuf));
                abinfo.bytes = cbBuf;
                /* Keep going. */
            }

            if (abinfo.bytes < 0)
            {
                LogFlowFunc(("Warning: Invalid available size, size=%d, bufsize=%zu\n", abinfo.bytes, cbBuf));
                rc = VERR_INVALID_PARAMETER;
                break;
            }

            cToRead = RT_MIN((uint32_t)AUDIOMIXBUF_B2S(&pStream->MixBuf, abinfo.bytes), cLive);
            if (!cToRead)
                break;
#ifndef RT_OS_L4
        }
#endif
        size_t cbToRead = AUDIOMIXBUF_S2B(&pStream->MixBuf, cToRead);
        LogFlowFunc(("cbToRead=%zu\n", cbToRead));

        uint32_t cRead, cbRead;
        while (cbToRead)
        {
            rc = AudioMixBufReadCirc(&pStream->MixBuf, pStrm->pvBuf, cbToRead, &cRead);
            if (RT_FAILURE(rc))
                break;

            cbRead = AUDIOMIXBUF_S2B(&pStream->MixBuf, cRead);
            ssize_t cbWritten = write(pStrm->hFile, pStrm->pvBuf, cbRead);
            if (cbWritten == -1)
            {
                LogRel(("OSS: Failed writing output data: %s\n", strerror(errno)));
                rc = RTErrConvertFromErrno(errno);
                break;
            }

            Assert(cbToRead >= cbRead);
            cbToRead    -= cbRead;
            cbReadTotal += cbRead;
        }

#ifndef RT_OS_L4
        /* Update read pointer. */
        if (pStrm->fMemMapped)
            pStrm->old_optr = cntinfo.ptr;
#endif

    } while(0);

    if (RT_SUCCESS(rc))
    {
        uint32_t cReadTotal = AUDIOMIXBUF_B2S(&pStream->MixBuf, cbReadTotal);
        if (cReadTotal)
            AudioMixBufFinish(&pStream->MixBuf, cReadTotal);

        if (pcSamplesPlayed)
            *pcSamplesPlayed = cReadTotal;

        LogFlowFunc(("cReadTotal=%RU32 (%RU32 bytes), rc=%Rrc\n", cReadTotal, cbReadTotal, rc));
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(void) drvHostOSSAudioShutdown(PPDMIHOSTAUDIO pInterface)
{
    NOREF(pInterface);
}

static DECLCALLBACK(PDMAUDIOBACKENDSTS) drvHostOSSAudioGetStatus(PPDMIHOSTAUDIO pInterface, PDMAUDIODIR enmDir)
{
    AssertPtrReturn(pInterface, PDMAUDIOBACKENDSTS_UNKNOWN);

    return PDMAUDIOBACKENDSTS_RUNNING;
}

static DECLCALLBACK(int) drvHostOSSAudioStreamCreate(PPDMIHOSTAUDIO pInterface,
                                                     PPDMAUDIOSTREAM pStream, PPDMAUDIOSTREAMCFG pCfg, uint32_t *pcSamples)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg,       VERR_INVALID_POINTER);

    int rc;
    if (pCfg->enmDir == PDMAUDIODIR_IN)
        rc = ossCreateStreamIn(pInterface,  pStream, pCfg, pcSamples);
    else
        rc = ossCreateStreamOut(pInterface, pStream, pCfg, pcSamples);

    LogFlowFunc(("%s: rc=%Rrc\n", pStream->szName, rc));
    return rc;
}

static DECLCALLBACK(int) drvHostOSSAudioStreamDestroy(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    int rc;
    if (pStream->enmDir == PDMAUDIODIR_IN)
        rc = ossDestroyStreamIn(pInterface,  pStream);
    else
        rc = ossDestroyStreamOut(pInterface, pStream);

    return rc;
}

static DECLCALLBACK(int) drvHostOSSAudioStreamControl(PPDMIHOSTAUDIO pInterface,
                                                      PPDMAUDIOSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    Assert(pStream->enmCtx == PDMAUDIOSTREAMCTX_HOST);

    int rc;
    if (pStream->enmDir == PDMAUDIODIR_IN)
        rc = ossControlStreamIn(pInterface,  pStream, enmStreamCmd);
    else
        rc = ossControlStreamOut(pInterface, pStream, enmStreamCmd);

    return rc;
}

static DECLCALLBACK(int) drvHostOSSAudioStreamIterate(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    /* Nothing to do here for OSS. */
    return VINF_SUCCESS;
}

static DECLCALLBACK(PDMAUDIOSTRMSTS) drvHostOSSAudioStreamGetStatus(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream)
{
    NOREF(pInterface);
    NOREF(pStream);

    PDMAUDIOSTRMSTS strmSts =   PDMAUDIOSTRMSTS_FLAG_INITIALIZED
                              | PDMAUDIOSTRMSTS_FLAG_ENABLED;

    strmSts |=   pStream->enmDir == PDMAUDIODIR_IN
               ? PDMAUDIOSTRMSTS_FLAG_DATA_READABLE
               : PDMAUDIOSTRMSTS_FLAG_DATA_WRITABLE;

    return strmSts;
}
/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvHostOSSAudioQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVHOSTOSSAUDIO  pThis   = PDMINS_2_DATA(pDrvIns, PDRVHOSTOSSAUDIO);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIHOSTAUDIO, &pThis->IHostAudio);

    return NULL;
}

/**
 * Constructs an OSS audio driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvHostOSSAudioConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    PDRVHOSTOSSAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTOSSAUDIO);
    LogRel(("Audio: Initializing OSS driver\n"));

    /*
     * Init the static parts.
     */
    pThis->pDrvIns                   = pDrvIns;
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface = drvHostOSSAudioQueryInterface;
    /* IHostAudio */
    PDMAUDIO_IHOSTAUDIO_CALLBACKS(drvHostOSSAudio);

    return VINF_SUCCESS;
}

/**
 * Char driver registration record.
 */
const PDMDRVREG g_DrvHostOSSAudio =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "OSSAudio",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "OSS audio host driver",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_AUDIO,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(DRVHOSTOSSAUDIO),
    /* pfnConstruct */
    drvHostOSSAudioConstruct,
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

