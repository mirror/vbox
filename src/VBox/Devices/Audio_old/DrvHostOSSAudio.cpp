/* $Id$ */
/** @file
 * OSS (Open Sound System) host audio backend.
 */

/*
 * Copyright (C) 2014-2015 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DRV_HOST_AUDIO
#include <VBox/log.h>
#include "DrvAudio.h"
#include "AudioMixBuffer.h"

#include "VBoxDD.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/soundcard.h>
#include <unistd.h>

#include <iprt/alloc.h>
#include <iprt/uuid.h> /* For PDMIBASE_2_PDMDRV. */
#include <VBox/vmm/pdmaudioifs.h>

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
    PDMAUDIOHSTSTRMIN  pStreamIn;
    int                hFile;
    int                cFragments;
    int                cbFragmentSize;
    /** Own PCM buffer. */
    void              *pvPCMBuf;
    /** Size (in bytes) of own PCM buffer. */
    size_t             cbPCMBuf;
    int                old_optr;
} OSSAUDIOSTREAMIN, *POSSAUDIOSTREAMIN;

typedef struct OSSAUDIOSTREAMOUT
{
    /** Note: Always must come first! */
    PDMAUDIOHSTSTRMOUT  pStreamOut;
    int                 hFile;
    int                 cFragments;
    int                 cbFragmentSize;
#ifndef RT_OS_L4
    /** Whether we use a memory mapped file instead of our
     *  own allocated PCM buffer below. */
    bool                fMemMapped;
#endif
    /** Own PCM buffer in case memory mapping is unavailable. */
    void               *pvPCMBuf;
    /** Size (in bytes) of own PCM buffer. */
    size_t              cbPCMBuf;
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

static int drvHostOSSAudioFmtToOSS(PDMAUDIOFMT fmt)
{
    switch (fmt)
    {
        case AUD_FMT_S8:
            return AFMT_S8;

        case AUD_FMT_U8:
            return AFMT_U8;

        case AUD_FMT_S16:
            return AFMT_S16_LE;

        case AUD_FMT_U16:
            return AFMT_U16_LE;

        default:
            break;
    }

    AssertMsgFailed(("Format %ld not supported\n", fmt));
    return AFMT_U8;
}

static int drvHostOSSAudioOSSToFmt(int fmt,
                                   PDMAUDIOFMT *pFmt, PDMAUDIOENDIANNESS *pENDIANNESS)
{
    switch (fmt)
    {
        case AFMT_S8:
            *pFmt = AUD_FMT_S8;
            if (pENDIANNESS)
                *pENDIANNESS = PDMAUDIOENDIANNESS_LITTLE;
            break;

        case AFMT_U8:
            *pFmt = AUD_FMT_U8;
            if (pENDIANNESS)
                *pENDIANNESS = PDMAUDIOENDIANNESS_LITTLE;
            break;

        case AFMT_S16_LE:
            *pFmt = AUD_FMT_S16;
            if (pENDIANNESS)
                *pENDIANNESS = PDMAUDIOENDIANNESS_LITTLE;
            break;

        case AFMT_U16_LE:
            *pFmt = AUD_FMT_U16;
            if (pENDIANNESS)
                *pENDIANNESS = PDMAUDIOENDIANNESS_LITTLE;
            break;

        case AFMT_S16_BE:
            *pFmt = AUD_FMT_S16;
            if (pENDIANNESS)
                *pENDIANNESS = PDMAUDIOENDIANNESS_BIG;
            break;

        case AFMT_U16_BE:
            *pFmt = AUD_FMT_U16;
            if (pENDIANNESS)
                *pENDIANNESS = PDMAUDIOENDIANNESS_BIG;
            break;

        default:
            AssertMsgFailed(("Format %ld not supported\n", fmt));
            return VERR_NOT_SUPPORTED;
    }

    return VINF_SUCCESS;
}

static int drvHostOSSAudioClose(int *phFile)
{
    if (!phFile || !*phFile)
        return VINF_SUCCESS;

    int rc;
    if (close(*phFile))
    {
        LogRel(("OSS: Closing descriptor failed: %s\n",
                strerror(errno)));
        rc = VERR_GENERAL_FAILURE; /** @todo */
    }
    else
    {
        *phFile = -1;
        rc = VINF_SUCCESS;
    }

    return rc;
}

static int drvHostOSSAudioOpen(bool fIn,
                               POSSAUDIOSTREAMCFG pReq, POSSAUDIOSTREAMCFG pObt,
                               int *phFile)
{
    AssertPtrReturn(pReq, VERR_INVALID_POINTER);
    AssertPtrReturn(pObt, VERR_INVALID_POINTER);
    AssertPtrReturn(phFile, VERR_INVALID_POINTER);

    int rc;
    int hFile;

    do
    {
        const char *pszDev = fIn ? s_OSSConf.devpath_in : s_OSSConf.devpath_out;
        if (!pszDev)
        {
            LogRel(("OSS: Invalid or no %s device name set\n",
                    fIn ? "input" : "output"));
            rc = VERR_INVALID_PARAMETER;
            break;
        }

        hFile = open(pszDev, (fIn ? O_RDONLY : O_WRONLY) | O_NONBLOCK);
        if (hFile == -1)
        {
            LogRel(("OSS: Failed to open %s: %s(%d)\n", pszDev, strerror(errno), errno));
            rc = RTErrConvertFromErrno(errno);
            break;
        }

        int iFormat = drvHostOSSAudioFmtToOSS(pReq->enmFormat);
        if (ioctl(hFile, SNDCTL_DSP_SAMPLESIZE, &iFormat))
        {
            LogRel(("OSS: Failed to set audio format to %ld errno=%s(%d)\n", iFormat, strerror(errno), errno));
            rc = RTErrConvertFromErrno(errno);
            break;
        }

        int cChannels = pReq->cChannels;
        if (ioctl(hFile, SNDCTL_DSP_CHANNELS, &cChannels))
        {
            LogRel(("OSS: Failed to set number of audio channels (%d): %s(%d)\n", pReq->cChannels, strerror(errno), errno));
            rc = RTErrConvertFromErrno(errno);
            break;
        }

        int freq = pReq->uFreq;
        if (ioctl(hFile, SNDCTL_DSP_SPEED, &freq))
        {
            LogRel(("OSS: Failed to set audio frequency (%dHZ): %s(%d)\n", pReq->uFreq, strerror(errno), errno));
            rc = RTErrConvertFromErrno(errno);
            break;
        }

        /* Obsolete on Solaris (using O_NONBLOCK is sufficient). */
#if !(defined(VBOX) && defined(RT_OS_SOLARIS))
        if (ioctl(hFile, SNDCTL_DSP_NONBLOCK))
        {
            LogRel(("OSS: Failed to set non-blocking mode: %s(%d)\n", strerror(errno), errno));
            rc = RTErrConvertFromErrno(errno);
            break;
        }
#endif
        int mmmmssss = (pReq->cFragments << 16) | lsbindex(pReq->cbFragmentSize);
        if (ioctl(hFile, SNDCTL_DSP_SETFRAGMENT, &mmmmssss))
        {
            LogRel(("OSS: Failed to set %RU16 fragments to %RU32 bytes each: %s(%d)\n",
                    pReq->cFragments, pReq->cbFragmentSize, strerror(errno), errno));
            rc = RTErrConvertFromErrno(errno);
            break;
        }

        audio_buf_info abinfo;
        if (ioctl(hFile, fIn ? SNDCTL_DSP_GETISPACE : SNDCTL_DSP_GETOSPACE, &abinfo))
        {
            LogRel(("OSS: Failed to retrieve buffer length: %s(%d)\n", strerror(errno), errno));
            rc = RTErrConvertFromErrno(errno);
            break;
        }

        rc = drvHostOSSAudioOSSToFmt(iFormat, &pObt->enmFormat, &pObt->enmENDIANNESS);
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
        drvHostOSSAudioClose(&hFile);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(int) drvHostOSSAudioControlIn(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMIN pHstStrmIn,
                                                   PDMAUDIOSTREAMCMD enmStreamCmd)
{
    NOREF(pInterface);
    NOREF(pHstStrmIn);
    NOREF(enmStreamCmd);

    /** @todo Nothing to do here right now!? */

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvHostOSSAudioControlOut(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMOUT pHstStrmOut,
                                                   PDMAUDIOSTREAMCMD enmStreamCmd)
{
    NOREF(pInterface);
    AssertPtrReturn(pHstStrmOut, VERR_INVALID_POINTER);

    POSSAUDIOSTREAMOUT pThisStrmOut = (POSSAUDIOSTREAMOUT)pHstStrmOut;

#ifdef RT_OS_L4
    return VINF_SUCCESS;
#else
    if (!pThisStrmOut->fMemMapped)
        return VINF_SUCCESS;
#endif

    int rc = VINF_SUCCESS;
    int mask;
    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
        case PDMAUDIOSTREAMCMD_RESUME:
        {
            DrvAudioClearBuf(&pHstStrmOut->Props,
                             pThisStrmOut->pvPCMBuf, pThisStrmOut->cbPCMBuf, AudioMixBufSize(&pHstStrmOut->MixBuf));

            mask = PCM_ENABLE_OUTPUT;
            if (ioctl(pThisStrmOut->hFile, SNDCTL_DSP_SETTRIGGER, &mask) < 0)
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
            if (ioctl(pThisStrmOut->hFile, SNDCTL_DSP_SETTRIGGER, &mask) < 0)
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

static DECLCALLBACK(int) drvHostOSSAudioCaptureIn(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMIN pHstStrmIn,
                                                  uint32_t *pcSamplesCaptured)
{
    NOREF(pInterface);
    AssertPtrReturn(pHstStrmIn, VERR_INVALID_POINTER);

    POSSAUDIOSTREAMIN pThisStrmIn = (POSSAUDIOSTREAMIN)pHstStrmIn;

    int rc = VINF_SUCCESS;
    size_t cbToRead = RT_MIN(pThisStrmIn->cbPCMBuf,
                             AudioMixBufFreeBytes(&pHstStrmIn->MixBuf));

    LogFlowFunc(("cbToRead=%zu\n", cbToRead));

    uint32_t cWrittenTotal = 0;
    uint32_t cbTemp;
    ssize_t  cbRead;
    size_t   offWrite = 0;

    while (cbToRead)
    {
        cbTemp = RT_MIN(cbToRead, pThisStrmIn->cbPCMBuf);
        AssertBreakStmt(cbTemp, rc = VERR_NO_DATA);
        cbRead = read(pThisStrmIn->hFile, (uint8_t *)pThisStrmIn->pvPCMBuf + offWrite, cbTemp);

        LogFlowFunc(("cbRead=%zi, cbTemp=%RU32, cbToRead=%zu\n",
                     cbRead, cbTemp, cbToRead));

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
                    LogFlowFunc(("Failed to read %zu input frames, rc=%Rrc\n",
                                 cbTemp, rc));
                    rc = VERR_GENERAL_FAILURE; /** @todo */
                    break;
            }

            if (RT_FAILURE(rc))
                break;
        }
        else if (cbRead)
        {
            uint32_t cWritten;
            rc = AudioMixBufWriteCirc(&pHstStrmIn->MixBuf,
                                      pThisStrmIn->pvPCMBuf, cbRead,
                                      &cWritten);
            if (RT_FAILURE(rc))
                break;

            uint32_t cbWritten = AUDIOMIXBUF_S2B(&pHstStrmIn->MixBuf, cWritten);

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
            rc = AudioMixBufMixToParent(&pHstStrmIn->MixBuf, cWrittenTotal,
                                        &cProcessed);

        if (pcSamplesCaptured)
            *pcSamplesCaptured = cWrittenTotal;

        LogFlowFunc(("cWrittenTotal=%RU32 (%RU32 processed), rc=%Rrc\n",
                     cWrittenTotal, cProcessed, rc));
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(int) drvHostOSSAudioFiniIn(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMIN pHstStrmIn)
{
    NOREF(pInterface);
    AssertPtrReturn(pHstStrmIn, VERR_INVALID_POINTER);

    POSSAUDIOSTREAMIN pThisStrmIn = (POSSAUDIOSTREAMIN)pHstStrmIn;

    LogFlowFuncEnter();

    if (pThisStrmIn->pvPCMBuf)
    {
        Assert(pThisStrmIn->cbPCMBuf);

        RTMemFree(pThisStrmIn->pvPCMBuf);
        pThisStrmIn->pvPCMBuf = NULL;
    }

    pThisStrmIn->cbPCMBuf = 0;

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvHostOSSAudioFiniOut(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMOUT pHstStrmOut)
{
    NOREF(pInterface);
    AssertPtrReturn(pHstStrmOut, VERR_INVALID_POINTER);

    POSSAUDIOSTREAMOUT pThisStrmOut = (POSSAUDIOSTREAMOUT)pHstStrmOut;

    LogFlowFuncEnter();

#ifndef RT_OS_L4
    if (!pThisStrmOut->fMemMapped)
    {
        if (pThisStrmOut->pvPCMBuf)
        {
            Assert(pThisStrmOut->cbPCMBuf);

            RTMemFree(pThisStrmOut->pvPCMBuf);
            pThisStrmOut->pvPCMBuf = NULL;
        }

        pThisStrmOut->cbPCMBuf = 0;
    }
#endif

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvHostOSSAudioGetConf(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDCFG pCfg)
{
    NOREF(pInterface);

    pCfg->cbStreamOut = sizeof(OSSAUDIOSTREAMOUT);
    pCfg->cbStreamIn = sizeof(OSSAUDIOSTREAMIN);
    pCfg->cMaxHstStrmsOut = INT_MAX;
    pCfg->cMaxHstStrmsIn = INT_MAX;

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) drvHostOSSAudioInitIn(PPDMIHOSTAUDIO pInterface,
                                               PPDMAUDIOHSTSTRMIN pHstStrmIn, PPDMAUDIOSTREAMCFG pCfg,
                                               PDMAUDIORECSOURCE enmRecSource,
                                               uint32_t *pcSamples)
{
    NOREF(pInterface);
    AssertPtrReturn(pHstStrmIn, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg, VERR_INVALID_POINTER);

    POSSAUDIOSTREAMIN pThisStrmIn = (POSSAUDIOSTREAMIN)pHstStrmIn;

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

        rc = drvHostOSSAudioOpen(true /* fIn */,
                                 &reqStream, &obtStream, &hFile);
        if (RT_SUCCESS(rc))
        {
            if (obtStream.cFragments * obtStream.cbFragmentSize & pHstStrmIn->Props.uAlign)
                LogRel(("OSS: Warning: Misaligned DAC output buffer: Size = %zu, Alignment = %u\n",
                        obtStream.cFragments * obtStream.cbFragmentSize,
                        pHstStrmIn->Props.uAlign + 1));

            pThisStrmIn->hFile = hFile;

            PDMAUDIOSTREAMCFG streamCfg;
            streamCfg.enmFormat     = obtStream.enmFormat;
            streamCfg.uHz           = obtStream.uFreq;
            streamCfg.cChannels     = pCfg->cChannels;
            streamCfg.enmEndianness = obtStream.enmENDIANNESS;

            rc = DrvAudioStreamCfgToProps(&streamCfg, &pHstStrmIn->Props);
            if (RT_SUCCESS(rc))
            {
                cSamples = (obtStream.cFragments * obtStream.cbFragmentSize)
                           >> pHstStrmIn->Props.cShift;
                if (!cSamples)
                    rc = VERR_INVALID_PARAMETER;
            }
        }

        if (RT_SUCCESS(rc))
        {
            size_t cbSample = (1 << pHstStrmIn->Props.cShift);
            size_t cbBuf    = cSamples * cbSample;
            pThisStrmIn->pvPCMBuf = RTMemAlloc(cbBuf);
            if (!pThisStrmIn->pvPCMBuf)
            {
                LogRel(("OSS: Failed allocating ADC buffer with %RU32 samples (%zu bytes per sample)\n", cSamples, cbSample));
                rc = VERR_NO_MEMORY;
            }

            pThisStrmIn->cbPCMBuf = cbBuf;

            if (pcSamples)
                *pcSamples = cSamples;
        }

    } while (0);

    if (RT_FAILURE(rc))
        drvHostOSSAudioClose(&hFile);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(int) drvHostOSSAudioInitOut(PPDMIHOSTAUDIO pInterface,
                                                PPDMAUDIOHSTSTRMOUT pHstStrmOut, PPDMAUDIOSTREAMCFG pCfg,
                                                uint32_t *pcSamples)
{
    NOREF(pInterface);
    AssertPtrReturn(pHstStrmOut, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg, VERR_INVALID_POINTER);

    POSSAUDIOSTREAMOUT pThisStrmOut = (POSSAUDIOSTREAMOUT)pHstStrmOut;

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

        rc = drvHostOSSAudioOpen(false /* fIn */,
                                 &reqStream, &obtStream, &hFile);
        if (RT_SUCCESS(rc))
        {
            if (obtStream.cFragments * obtStream.cbFragmentSize & pHstStrmOut->Props.uAlign)
                LogRel(("OSS: Warning: Misaligned DAC output buffer: Size = %zu, Alignment = %u\n",
                        obtStream.cFragments * obtStream.cbFragmentSize,
                        pHstStrmOut->Props.uAlign + 1));

            pThisStrmOut->hFile = hFile;

            PDMAUDIOSTREAMCFG streamCfg;
            streamCfg.enmFormat     = obtStream.enmFormat;
            streamCfg.uHz           = obtStream.uFreq;
            streamCfg.cChannels     = pCfg->cChannels;
            streamCfg.enmEndianness = obtStream.enmENDIANNESS;

            rc = DrvAudioStreamCfgToProps(&streamCfg, &pHstStrmOut->Props);
            if (RT_SUCCESS(rc))
                cSamples = (obtStream.cFragments * obtStream.cbFragmentSize)
                           >> pHstStrmOut->Props.cShift;
        }

        if (RT_SUCCESS(rc))
        {
#ifndef RT_OS_L4
            pThisStrmOut->fMemMapped = false;
            if (s_OSSConf.try_mmap)
            {
                pThisStrmOut->pvPCMBuf = mmap(0, cSamples << pHstStrmOut->Props.cShift,
                                              PROT_READ | PROT_WRITE, MAP_SHARED, hFile, 0);
                if (pThisStrmOut->pvPCMBuf == MAP_FAILED)
                {
                    LogRel(("OSS: Failed to memory map %zu bytes of DAC output file: %s\n",
                            cSamples << pHstStrmOut->Props.cShift, strerror(errno)));
                    rc = RTErrConvertFromErrno(errno);
                    break;
                }
                else
                {
                    int mask = 0;
                    if (ioctl(hFile, SNDCTL_DSP_SETTRIGGER, &mask) < 0)
                    {
                        LogRel(("OSS: Failed to retrieve initial trigger mask: %s\n",
                                strerror(errno)));
                        rc = RTErrConvertFromErrno(errno);
                        /* Note: No break here, need to unmap file first! */
                    }
                    else
                    {
                        mask = PCM_ENABLE_OUTPUT;
                        if (ioctl (hFile, SNDCTL_DSP_SETTRIGGER, &mask) < 0)
                        {
                            LogRel(("OSS: Failed to retrieve PCM_ENABLE_OUTPUT mask: %s\n",
                                    strerror(errno)));
                            rc = RTErrConvertFromErrno(errno);
                            /* Note: No break here, need to unmap file first! */
                        }
                        else
                            pThisStrmOut->fMemMapped = true;
                    }

                    if (!pThisStrmOut->fMemMapped)
                    {
                        int rc2 = munmap(pThisStrmOut->pvPCMBuf,
                                         cSamples << pHstStrmOut->Props.cShift);
                        if (rc2)
                            LogRel(("OSS: Failed to unmap DAC output file: %s\n", strerror(errno)));
                        break;
                    }
                }
            }
#endif /* !RT_OS_L4 */

            /* Memory mapping failed above? Try allocating an own buffer. */
#ifndef RT_OS_L4
            if (!pThisStrmOut->fMemMapped)
            {
#endif
                size_t cbSample = (1 << pHstStrmOut->Props.cShift);
                size_t cbPCMBuf = cSamples * cbSample;

                LogFlowFunc(("cSamples=%RU32\n", cSamples));

                pThisStrmOut->pvPCMBuf = RTMemAlloc(cbPCMBuf);
                if (!pThisStrmOut->pvPCMBuf)
                {
                    LogRel(("OSS: Failed allocating DAC buffer with %RU32 samples (%zu bytes per sample)\n", cSamples, cbSample));
                    rc = VERR_NO_MEMORY;
                    break;
                }

                pThisStrmOut->cbPCMBuf = cbPCMBuf;
#ifndef RT_OS_L4
            }
#endif
            if (pcSamples)
                *pcSamples = cSamples;
        }

    } while (0);

    if (RT_FAILURE(rc))
        drvHostOSSAudioClose(&hFile);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(bool) drvHostOSSAudioIsEnabled(PPDMIHOSTAUDIO pInterface, PDMAUDIODIR enmDir)
{
    NOREF(pInterface);
    NOREF(enmDir);
    return true; /* Always all enabled. */
}

static DECLCALLBACK(int) drvHostOSSAudioPlayOut(PPDMIHOSTAUDIO pInterface, PPDMAUDIOHSTSTRMOUT pHstStrmOut,
                                                uint32_t *pcSamplesPlayed)
{
    NOREF(pInterface);
    AssertPtrReturn(pHstStrmOut, VERR_INVALID_POINTER);

    POSSAUDIOSTREAMOUT pThisStrmOut = (POSSAUDIOSTREAMOUT)pHstStrmOut;

    int rc = VINF_SUCCESS;
    uint32_t cbReadTotal = 0;
    count_info cntinfo;

    do
    {
        size_t cbBuf = AudioMixBufSizeBytes(&pHstStrmOut->MixBuf);

        uint32_t cLive = AudioMixBufAvail(&pHstStrmOut->MixBuf);
        uint32_t cToRead;

#ifndef RT_OS_L4
        if (pThisStrmOut->fMemMapped)
        {
            /* Get current playback pointer. */
            int rc2 = ioctl(pThisStrmOut->hFile, SNDCTL_DSP_GETOPTR, &cntinfo);
            if (!rc2)
            {
                LogRel(("OSS: Failed to retrieve current playback pointer: %s\n",
                        strerror(errno)));
                rc = RTErrConvertFromErrno(errno);
                break;
            }

            /* Nothing to play? */
            if (cntinfo.ptr == pThisStrmOut->old_optr)
                break;

            int cbData;
            if (cntinfo.ptr > pThisStrmOut->old_optr)
                cbData = cntinfo.ptr - pThisStrmOut->old_optr;
            else
                cbData = cbBuf + cntinfo.ptr - pThisStrmOut->old_optr;
            Assert(cbData);

            cToRead = RT_MIN((uint32_t)AUDIOMIXBUF_B2S(&pHstStrmOut->MixBuf, cbData),
                             cLive);
        }
        else
        {
#endif
            audio_buf_info abinfo;
            int rc2 = ioctl(pThisStrmOut->hFile, SNDCTL_DSP_GETOSPACE, &abinfo);
            if (rc2 < 0)
            {
                LogRel(("OSS: Failed to retrieve current playback buffer: %s\n",
                        strerror(errno)));
                rc = RTErrConvertFromErrno(errno);
                break;
            }

            if ((size_t)abinfo.bytes > cbBuf)
            {
                LogFlowFunc(("Warning: Invalid available size, size=%d, bufsize=%d\n",
                             abinfo.bytes, cbBuf));
                abinfo.bytes = cbBuf;
                /* Keep going. */
            }

            if (abinfo.bytes < 0)
            {
                LogFlowFunc(("Warning: Invalid available size, size=%d, bufsize=%d\n",
                             abinfo.bytes, cbBuf));
                rc = VERR_INVALID_PARAMETER;
                break;
            }

            cToRead = RT_MIN((uint32_t)AUDIOMIXBUF_B2S(&pHstStrmOut->MixBuf, abinfo.bytes),
                             cLive);
            if (!cToRead)
                break;
#ifndef RT_OS_L4
        }
#endif
        size_t cbToRead = AUDIOMIXBUF_S2B(&pHstStrmOut->MixBuf, cToRead);
        LogFlowFunc(("cbToRead=%zu\n", cbToRead));

        uint32_t cRead, cbRead;
        while (cbToRead)
        {
            rc = AudioMixBufReadCirc(&pHstStrmOut->MixBuf,
                                     pThisStrmOut->pvPCMBuf, cbToRead, &cRead);
            if (RT_FAILURE(rc))
                break;

            cbRead = AUDIOMIXBUF_S2B(&pHstStrmOut->MixBuf, cRead);
            ssize_t cbWritten = write(pThisStrmOut->hFile, pThisStrmOut->pvPCMBuf,
                                      cbRead);
            if (cbWritten == -1)
            {
                LogRel(("OSS: Failed writing output data: %s\n", strerror(errno)));
                rc = RTErrConvertFromErrno(errno);
                break;
            }

            Assert(cbToRead >= cbRead);
            cbToRead -= cbRead;
            cbReadTotal += cbRead;
        }

#ifndef RT_OS_L4
        /* Update read pointer. */
        if (pThisStrmOut->fMemMapped)
            pThisStrmOut->old_optr = cntinfo.ptr;
#endif

    } while(0);

    if (RT_SUCCESS(rc))
    {
        uint32_t cReadTotal = AUDIOMIXBUF_B2S(&pHstStrmOut->MixBuf, cbReadTotal);
        if (cReadTotal)
            AudioMixBufFinish(&pHstStrmOut->MixBuf, cReadTotal);

        if (pcSamplesPlayed)
            *pcSamplesPlayed = cReadTotal;

        LogFlowFunc(("cReadTotal=%RU32 (%RU32 bytes), rc=%Rrc\n",
                     cReadTotal, cbReadTotal, rc));
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(void) drvHostOSSAudioShutdown(PPDMIHOSTAUDIO pInterface)
{
    NOREF(pInterface);
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
