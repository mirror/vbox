/* $Id$ */
/** @file
 * Host audio driver - OSS (Open Sound System).
 */

/*
 * Copyright (C) 2014-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
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
#include <VBox/vmm/pdmaudioinline.h>

#include "VBoxDD.h"


/*********************************************************************************************************************************
*   Defines                                                                                                                      *
*********************************************************************************************************************************/
#if ((SOUND_VERSION > 360) && (defined(OSS_SYSINFO)))
/* OSS > 3.6 has a new syscall available for querying a bit more detailed information
 * about OSS' audio capabilities. This is handy for e.g. Solaris. */
# define VBOX_WITH_AUDIO_OSS_SYSINFO 1
#endif

/** Makes DRVHOSTOSSAUDIO out of PDMIHOSTAUDIO. */
#define PDMIHOSTAUDIO_2_DRVHOSTOSSAUDIO(pInterface) \
    ( (PDRVHOSTOSSAUDIO)((uintptr_t)pInterface - RT_UOFFSETOF(DRVHOSTOSSAUDIO, IHostAudio)) )


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
    PDMAUDIOPCMPROPS  Props;
    uint16_t          cFragments;
    uint32_t          cbFragmentSize;
} OSSAUDIOSTREAMCFG, *POSSAUDIOSTREAMCFG;

typedef struct OSSAUDIOSTREAM
{
    /** The stream's acquired configuration. */
    PPDMAUDIOSTREAMCFG pCfg;
    /** Buffer alignment. */
    uint8_t            uAlign;
    union
    {
        struct
        {

        } In;
        struct
        {
#ifndef RT_OS_L4
            /** Whether we use a memory mapped file instead of our
             *  own allocated PCM buffer below. */
            /** @todo The memory mapped code seems to be utterly broken.
             *        Needs investigation! */
            bool       fMMIO;
#endif
        } Out;
    };
    int                hFile;
    int                cFragments;
    int                cbFragmentSize;
    /** Own PCM buffer. */
    void              *pvBuf;
    /** Size (in bytes) of own PCM buffer. */
    size_t             cbBuf;
    int                old_optr;
} OSSAUDIOSTREAM, *POSSAUDIOSTREAM;

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


static int ossOSSToAudioProps(PPDMAUDIOPCMPROPS pProps, int fmt, int cChannels, int uHz)
{
    switch (fmt)
    {
        case AFMT_S8:
            PDMAudioPropsInit(pProps, 1 /*8-bit*/, true /*signed*/, cChannels, uHz);
            break;

        case AFMT_U8:
            PDMAudioPropsInit(pProps, 1 /*8-bit*/, false /*signed*/, cChannels, uHz);
            break;

        case AFMT_S16_LE:
            PDMAudioPropsInitEx(pProps, 2 /*16-bit*/, true /*signed*/, cChannels, uHz, true /*fLittleEndian*/, false /*fRaw*/);
            break;

        case AFMT_U16_LE:
            PDMAudioPropsInitEx(pProps, 2 /*16-bit*/, false /*signed*/, cChannels, uHz, true /*fLittleEndian*/, false /*fRaw*/);
            break;

        case AFMT_S16_BE:
            PDMAudioPropsInitEx(pProps, 2 /*16-bit*/, true /*signed*/, cChannels, uHz, false /*fLittleEndian*/, false /*fRaw*/);
            break;

        case AFMT_U16_BE:
            PDMAudioPropsInitEx(pProps, 2 /*16-bit*/, false /*signed*/, cChannels, uHz, false /*fLittleEndian*/, false /*fRaw*/);
            break;

        default:
            AssertMsgFailedReturn(("Format %d not supported\n", fmt), VERR_NOT_SUPPORTED);
    }

    return VINF_SUCCESS;
}


static int ossStreamClose(int *phFile)
{
    if (!phFile || !*phFile || *phFile == -1)
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


static int ossStreamOpen(const char *pszDev, int fOpen, POSSAUDIOSTREAMCFG pOSSReq, POSSAUDIOSTREAMCFG pOSSAcq, int *phFile)
{
    int rc = VINF_SUCCESS;

    int fdFile = -1;
    do
    {
        fdFile = open(pszDev, fOpen);
        if (fdFile == -1)
        {
            LogRel(("OSS: Failed to open %s: %s (%d)\n", pszDev, strerror(errno), errno));
            break;
        }

        int iFormat;
        switch (PDMAudioPropsSampleSize(&pOSSReq->Props))
        {
            case 1:
                iFormat = pOSSReq->Props.fSigned ? AFMT_S8 : AFMT_U8;
                break;

            case 2:
                if (PDMAudioPropsIsLittleEndian(&pOSSReq->Props))
                    iFormat = pOSSReq->Props.fSigned ? AFMT_S16_LE : AFMT_U16_LE;
                else
                    iFormat = pOSSReq->Props.fSigned ? AFMT_S16_BE : AFMT_U16_BE;
                break;

            default:
                rc = VERR_AUDIO_STREAM_COULD_NOT_CREATE;
                break;
        }

        if (RT_FAILURE(rc))
            break;

        if (ioctl(fdFile, SNDCTL_DSP_SAMPLESIZE, &iFormat))
        {
            LogRel(("OSS: Failed to set audio format to %ld: %s (%d)\n", iFormat, strerror(errno), errno));
            break;
        }

        int cChannels = PDMAudioPropsChannels(&pOSSReq->Props);
        if (ioctl(fdFile, SNDCTL_DSP_CHANNELS, &cChannels))
        {
            LogRel(("OSS: Failed to set number of audio channels (%RU8): %s (%d)\n",
                    PDMAudioPropsChannels(&pOSSReq->Props), strerror(errno), errno));
            break;
        }

        int freq = pOSSReq->Props.uHz;
        if (ioctl(fdFile, SNDCTL_DSP_SPEED, &freq))
        {
            LogRel(("OSS: Failed to set audio frequency (%dHZ): %s (%d)\n", pOSSReq->Props.uHz, strerror(errno), errno));
            break;
        }

        /* Obsolete on Solaris (using O_NONBLOCK is sufficient). */
#if !(defined(VBOX) && defined(RT_OS_SOLARIS))
        if (ioctl(fdFile, SNDCTL_DSP_NONBLOCK))
        {
            LogRel(("OSS: Failed to set non-blocking mode: %s (%d)\n", strerror(errno), errno));
            break;
        }
#endif

        /* Check access mode (input or output). */
        bool fIn = ((fOpen & O_ACCMODE) == O_RDONLY);

        LogRel2(("OSS: Requested %RU16 %s fragments, %RU32 bytes each\n",
                 pOSSReq->cFragments, fIn ? "input" : "output", pOSSReq->cbFragmentSize));

        int mmmmssss = (pOSSReq->cFragments << 16) | lsbindex(pOSSReq->cbFragmentSize);
        if (ioctl(fdFile, SNDCTL_DSP_SETFRAGMENT, &mmmmssss))
        {
            LogRel(("OSS: Failed to set %RU16 fragments to %RU32 bytes each: %s (%d)\n",
                    pOSSReq->cFragments, pOSSReq->cbFragmentSize, strerror(errno), errno));
            break;
        }

        audio_buf_info abinfo;
        if (ioctl(fdFile, fIn ? SNDCTL_DSP_GETISPACE : SNDCTL_DSP_GETOSPACE, &abinfo))
        {
            LogRel(("OSS: Failed to retrieve %c"
                    "s buffer length: %s (%d)\n", fIn ? "input" : "output", strerror(errno), errno));
            break;
        }

        rc = ossOSSToAudioProps(&pOSSAcq->Props, iFormat, cChannels, freq);
        if (RT_SUCCESS(rc))
        {
            pOSSAcq->cFragments      = abinfo.fragstotal;
            pOSSAcq->cbFragmentSize  = abinfo.fragsize;

            LogRel2(("OSS: Got %RU16 %s fragments, %RU32 bytes each\n",
                     pOSSAcq->cFragments, fIn ? "input" : "output", pOSSAcq->cbFragmentSize));

            *phFile = fdFile;
        }
    }
    while (0);

    if (RT_FAILURE(rc))
        ossStreamClose(&fdFile);

    LogFlowFuncLeaveRC(rc);
    return rc;
}


static int ossControlStreamIn(/*PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd*/ void)
{
    /** @todo Nothing to do here right now!? */

    return VINF_SUCCESS;
}


static int ossControlStreamOut(PPDMAUDIOBACKENDSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    POSSAUDIOSTREAM pStreamOSS = (POSSAUDIOSTREAM)pStream;

    int rc = VINF_SUCCESS;

    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
        case PDMAUDIOSTREAMCMD_RESUME:
        {
            PDMAudioPropsClearBuffer(&pStreamOSS->pCfg->Props, pStreamOSS->pvBuf, pStreamOSS->cbBuf,
                                     PDMAUDIOPCMPROPS_B2F(&pStreamOSS->pCfg->Props, pStreamOSS->cbBuf));

            int mask = PCM_ENABLE_OUTPUT;
            if (ioctl(pStreamOSS->hFile, SNDCTL_DSP_SETTRIGGER, &mask) < 0)
            {
                LogRel(("OSS: Failed to enable output stream: %s\n", strerror(errno)));
                rc = RTErrConvertFromErrno(errno);
            }

            break;
        }

        case PDMAUDIOSTREAMCMD_DISABLE:
        case PDMAUDIOSTREAMCMD_PAUSE:
        {
            int mask = 0;
            if (ioctl(pStreamOSS->hFile, SNDCTL_DSP_SETTRIGGER, &mask) < 0)
            {
                LogRel(("OSS: Failed to disable output stream: %s\n", strerror(errno)));
                rc = RTErrConvertFromErrno(errno);
            }

            break;
        }

        default:
            rc = VERR_NOT_SUPPORTED;
            break;
    }

    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnInit}
 */
static DECLCALLBACK(int) drvHostOssAudioHA_Init(PPDMIHOSTAUDIO pInterface)
{
    RT_NOREF(pInterface);

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCapture}
 */
static DECLCALLBACK(int) drvHostOssAudioHA_StreamCapture(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                         void *pvBuf, uint32_t uBufSize, uint32_t *puRead)
{
    RT_NOREF(pInterface);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);

    POSSAUDIOSTREAM pStreamOSS = (POSSAUDIOSTREAM)pStream;

    int rc = VINF_SUCCESS;

    size_t cbToRead = RT_MIN(pStreamOSS->cbBuf, uBufSize);

    LogFlowFunc(("cbToRead=%zi\n", cbToRead));

    uint32_t cbReadTotal = 0;
    uint32_t cbTemp;
    ssize_t  cbRead;
    size_t   offWrite = 0;

    while (cbToRead)
    {
        cbTemp = RT_MIN(cbToRead, pStreamOSS->cbBuf);
        AssertBreakStmt(cbTemp, rc = VERR_NO_DATA);
        cbRead = read(pStreamOSS->hFile, (uint8_t *)pStreamOSS->pvBuf, cbTemp);

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
            memcpy((uint8_t *)pvBuf + offWrite, pStreamOSS->pvBuf, cbRead);

            Assert((ssize_t)cbToRead >= cbRead);
            cbToRead    -= cbRead;
            offWrite    += cbRead;
            cbReadTotal += cbRead;
        }
        else /* No more data, try next round. */
            break;
    }

    if (rc == VERR_NO_DATA)
        rc = VINF_SUCCESS;

    if (RT_SUCCESS(rc))
    {
        if (puRead)
            *puRead = cbReadTotal;
    }

    return rc;
}


static int ossDestroyStreamIn(PPDMAUDIOBACKENDSTREAM pStream)
{
    POSSAUDIOSTREAM pStreamOSS = (POSSAUDIOSTREAM)pStream;

    LogFlowFuncEnter();

    if (pStreamOSS->pvBuf)
    {
        Assert(pStreamOSS->cbBuf);

        RTMemFree(pStreamOSS->pvBuf);
        pStreamOSS->pvBuf = NULL;
    }

    pStreamOSS->cbBuf = 0;

    ossStreamClose(&pStreamOSS->hFile);

    return VINF_SUCCESS;
}


static int ossDestroyStreamOut(PPDMAUDIOBACKENDSTREAM pStream)
{
    POSSAUDIOSTREAM pStreamOSS = (POSSAUDIOSTREAM)pStream;

#ifndef RT_OS_L4
    if (pStreamOSS->Out.fMMIO)
    {
        if (pStreamOSS->pvBuf)
        {
            Assert(pStreamOSS->cbBuf);

            int rc2 = munmap(pStreamOSS->pvBuf, pStreamOSS->cbBuf);
            if (rc2 == 0)
            {
                pStreamOSS->pvBuf      = NULL;
                pStreamOSS->cbBuf      = 0;

                pStreamOSS->Out.fMMIO  = false;
            }
            else
                LogRel(("OSS: Failed to memory unmap playback buffer on close: %s\n", strerror(errno)));
        }
    }
    else
#endif
    {
        if (pStreamOSS->pvBuf)
        {
            Assert(pStreamOSS->cbBuf);

            RTMemFree(pStreamOSS->pvBuf);
            pStreamOSS->pvBuf = NULL;
        }

        pStreamOSS->cbBuf = 0;
    }

    ossStreamClose(&pStreamOSS->hFile);

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetConfig}
 */
static DECLCALLBACK(int) drvHostOssAudioHA_GetConfig(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDCFG pBackendCfg)
{
    RT_NOREF(pInterface);

    RTStrPrintf2(pBackendCfg->szName, sizeof(pBackendCfg->szName), "OSS");

    pBackendCfg->cbStreamIn  = sizeof(OSSAUDIOSTREAM);
    pBackendCfg->cbStreamOut = sizeof(OSSAUDIOSTREAM);

    int hFile = open("/dev/dsp", O_WRONLY | O_NONBLOCK, 0);
    if (hFile == -1)
    {
        /* Try opening the mixing device instead. */
        hFile = open("/dev/mixer", O_RDONLY | O_NONBLOCK, 0);
    }

    int ossVer = -1;

#ifdef VBOX_WITH_AUDIO_OSS_SYSINFO
    oss_sysinfo ossInfo;
    RT_ZERO(ossInfo);
#endif

    if (hFile != -1)
    {
        int err = ioctl(hFile, OSS_GETVERSION, &ossVer);
        if (err == 0)
        {
            LogRel2(("OSS: Using version: %d\n", ossVer));
#ifdef VBOX_WITH_AUDIO_OSS_SYSINFO
            err = ioctl(hFile, OSS_SYSINFO, &ossInfo);
            if (err == 0)
            {
                LogRel2(("OSS: Number of DSPs: %d\n", ossInfo.numaudios));
                LogRel2(("OSS: Number of mixers: %d\n", ossInfo.nummixers));

                int cDev = ossInfo.nummixers;
                if (!cDev)
                    cDev = ossInfo.numaudios;

                pBackendCfg->cMaxStreamsIn   = UINT32_MAX;
                pBackendCfg->cMaxStreamsOut  = UINT32_MAX;
            }
            else
            {
#endif
                /* Since we cannot query anything, assume that we have at least
                 * one input and one output if we found "/dev/dsp" or "/dev/mixer". */

                pBackendCfg->cMaxStreamsIn   = UINT32_MAX;
                pBackendCfg->cMaxStreamsOut  = UINT32_MAX;
#ifdef VBOX_WITH_AUDIO_OSS_SYSINFO
            }
#endif
        }
        else
            LogRel(("OSS: Unable to determine installed version: %s (%d)\n", strerror(err), err));
    }
    else
        LogRel(("OSS: No devices found, audio is not available\n"));

    if (hFile != -1)
        close(hFile);

    return VINF_SUCCESS;
}


static int ossCreateStreamIn(POSSAUDIOSTREAM pStreamOSS, PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    int rc;

    int hFile = -1;

    do
    {
        OSSAUDIOSTREAMCFG ossReq;
        memcpy(&ossReq.Props, &pCfgReq->Props, sizeof(PDMAUDIOPCMPROPS));

        ossReq.cFragments     = s_OSSConf.nfrags;
        ossReq.cbFragmentSize = s_OSSConf.fragsize;

        OSSAUDIOSTREAMCFG ossAcq;
        RT_ZERO(ossAcq);

        rc = ossStreamOpen(s_OSSConf.devpath_in, O_RDONLY | O_NONBLOCK, &ossReq, &ossAcq, &hFile);
        if (RT_SUCCESS(rc))
        {
            memcpy(&pCfgAcq->Props, &ossAcq.Props, sizeof(PDMAUDIOPCMPROPS));

            if (ossAcq.cFragments * ossAcq.cbFragmentSize & pStreamOSS->uAlign)
            {
                LogRel(("OSS: Warning: Misaligned capturing buffer: Size = %zu, Alignment = %u\n",
                        ossAcq.cFragments * ossAcq.cbFragmentSize, pStreamOSS->uAlign + 1));
            }

            if (RT_SUCCESS(rc))
            {
                size_t cbBuf = PDMAUDIOSTREAMCFG_F2B(pCfgAcq, ossAcq.cFragments * ossAcq.cbFragmentSize);
                void  *pvBuf = RTMemAlloc(cbBuf);
                if (!pvBuf)
                {
                    LogRel(("OSS: Failed allocating capturing buffer with (%zu bytes)\n", cbBuf));
                    rc = VERR_NO_MEMORY;
                }

                pStreamOSS->hFile = hFile;
                pStreamOSS->pvBuf = pvBuf;
                pStreamOSS->cbBuf = cbBuf;

                pCfgAcq->Backend.cFramesPeriod     = PDMAUDIOSTREAMCFG_B2F(pCfgAcq, ossAcq.cbFragmentSize);
                pCfgAcq->Backend.cFramesBufferSize = pCfgAcq->Backend.cFramesPeriod * 2; /* Use "double buffering". */
                /** @todo Pre-buffering required? */
            }
        }

    } while (0);

    if (RT_FAILURE(rc))
        ossStreamClose(&hFile);

    LogFlowFuncLeaveRC(rc);
    return rc;
}


static int ossCreateStreamOut(POSSAUDIOSTREAM pStreamOSS, PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    int rc;
    int hFile = -1;

    do
    {
        OSSAUDIOSTREAMCFG reqStream;
        RT_ZERO(reqStream);

        OSSAUDIOSTREAMCFG obtStream;
        RT_ZERO(obtStream);

        memcpy(&reqStream.Props, &pCfgReq->Props, sizeof(PDMAUDIOPCMPROPS));

        reqStream.cFragments     = s_OSSConf.nfrags;
        reqStream.cbFragmentSize = s_OSSConf.fragsize;

        rc = ossStreamOpen(s_OSSConf.devpath_out, O_WRONLY, &reqStream, &obtStream, &hFile);
        if (RT_SUCCESS(rc))
        {
            memcpy(&pCfgAcq->Props, &obtStream.Props, sizeof(PDMAUDIOPCMPROPS));

            if (obtStream.cFragments * obtStream.cbFragmentSize & pStreamOSS->uAlign)
            {
                LogRel(("OSS: Warning: Misaligned playback buffer: Size = %zu, Alignment = %u\n",
                        obtStream.cFragments * obtStream.cbFragmentSize, pStreamOSS->uAlign + 1));
            }
        }

        if (RT_SUCCESS(rc)) /** @todo r=bird: great code structure ... */
        {
            pStreamOSS->Out.fMMIO = false;

            size_t cbBuf = PDMAUDIOSTREAMCFG_F2B(pCfgAcq, obtStream.cFragments * obtStream.cbFragmentSize);
            Assert(cbBuf);

#ifndef RT_OS_L4
            if (s_OSSConf.try_mmap)
            {
                pStreamOSS->pvBuf = mmap(0, cbBuf, PROT_READ | PROT_WRITE, MAP_SHARED, hFile, 0);
                if (pStreamOSS->pvBuf == MAP_FAILED)
                {
                    LogRel(("OSS: Failed to memory map %zu bytes of playback buffer: %s\n", cbBuf, strerror(errno)));
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
                        {
                            pStreamOSS->Out.fMMIO = true;
                            LogRel(("OSS: Using MMIO\n"));
                        }
                    }

                    if (RT_FAILURE(rc))
                    {
                        int rc2 = munmap(pStreamOSS->pvBuf, cbBuf);
                        if (rc2)
                            LogRel(("OSS: Failed to memory unmap playback buffer: %s\n", strerror(errno)));
                        break;
                    }
                }
            }
#endif /* !RT_OS_L4 */

            /* Memory mapping failed above? Try allocating an own buffer. */
#ifndef RT_OS_L4
            if (!pStreamOSS->Out.fMMIO)
            {
#endif
                void *pvBuf = RTMemAlloc(cbBuf);
                if (!pvBuf)
                {
                    LogRel(("OSS: Failed allocating playback buffer with %zu bytes\n", cbBuf));
                    rc = VERR_NO_MEMORY;
                    break;
                }

                pStreamOSS->hFile = hFile;
                pStreamOSS->pvBuf = pvBuf;
                pStreamOSS->cbBuf = cbBuf;
#ifndef RT_OS_L4
            }
#endif
            pCfgAcq->Backend.cFramesPeriod     = PDMAUDIOSTREAMCFG_B2F(pCfgAcq, obtStream.cbFragmentSize);
            pCfgAcq->Backend.cFramesBufferSize = pCfgAcq->Backend.cFramesPeriod * 2; /* Use "double buffering" */
        }

    } while (0);

    if (RT_FAILURE(rc))
        ossStreamClose(&hFile);

    LogFlowFuncLeaveRC(rc);
    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamPlay}
 */
static DECLCALLBACK(int) drvHostOssAudioHA_StreamPlay(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                      const void *pvBuf, uint32_t cbBuf, uint32_t *pcbWritten)
{
    POSSAUDIOSTREAM pStreamOSS = (POSSAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamOSS, VERR_INVALID_POINTER);
    RT_NOREF(pInterface);

    /*
     * Figure out now much to write.
     */
    uint32_t cbToWrite;
#ifndef RT_OS_L4
    count_info CountInfo = {0,0,0};
    if (pStreamOSS->Out.fMMIO)
    {
        /* Get current playback pointer. */
        int rc2 = ioctl(pStreamOSS->hFile, SNDCTL_DSP_GETOPTR, &CountInfo);
        AssertLogRelMsgReturn(rc2 >= 0, ("OSS: Failed to retrieve current playback pointer: %s (%d)\n", strerror(errno), errno),
                              RTErrConvertFromErrno(errno));

        int cbData;
        if (CountInfo.ptr >= pStreamOSS->old_optr)
            cbData = CountInfo.ptr - pStreamOSS->old_optr;
        else
            cbData = pStreamOSS->cbBuf + CountInfo.ptr - pStreamOSS->old_optr;
        Assert(cbData >= 0);
        cbToWrite = (unsigned)cbData;
    }
    else
#endif
    {
        audio_buf_info abinfo;
        int rc2 = ioctl(pStreamOSS->hFile, SNDCTL_DSP_GETOSPACE, &abinfo);
        AssertLogRelMsgReturn(rc2 >= 0, ("OSS: Failed to retrieve current playback buffer: %s (%d)\n", strerror(errno), errno),
                              RTErrConvertFromErrno(errno));

#if 0   /** @todo r=bird: WTF do we make a fuss over abinfo.bytes for when we don't even use it?!? */
        AssertLogRelMsgReturn(abinfo.bytes >= 0, ("OSS: Warning: Invalid available size: %d\n", abinfo.bytes), VERR_INTERNAL_ERROR_3);
        if ((unsigned)abinfo.bytes > cbBuf)
        {
            LogRel2(("OSS: Warning: Too big output size (%d > %RU32), limiting to %RU32\n", abinfo.bytes, cbBuf, cbBuf));
            abinfo.bytes = cbBuf;
            /* Keep going. */
        }
#endif
        cbToWrite = (unsigned)(abinfo.fragments * abinfo.fragsize);
    }
    cbToWrite = RT_MIN(cbToWrite, cbBuf);
    cbToWrite = RT_MIN(cbToWrite, pStreamOSS->cbBuf);

    /*
     * This is probably for the mmap functionality and not needed in the no-mmap case.
     */
    /** @todo skip for non-mmap?   */
    uint8_t *pbBuf = (uint8_t *)memcpy(pStreamOSS->pvBuf, pvBuf, cbToWrite);

    /*
     * Write.
     */
    uint32_t cbChunk  = cbToWrite;
    uint32_t offChunk = 0;
    while (cbChunk > 0)
    {
        ssize_t cbWritten = write(pStreamOSS->hFile, &pbBuf[offChunk], RT_MIN(cbChunk, (unsigned)s_OSSConf.fragsize));
        if (cbWritten >= 0)
        {
            AssertLogRelMsg(!(cbWritten & pStreamOSS->uAlign),
                            ("OSS: Misaligned write (written %#zx, alignment %#x)\n", cbWritten, pStreamOSS->uAlign));

            Assert((uint32_t)cbWritten <= cbChunk);
            offChunk += (uint32_t)cbWritten;
            cbChunk  -= (uint32_t)cbWritten;
        }
        else
        {
            LogRel(("OSS: Failed writing output data: %s (%d)\n", strerror(errno), errno));
            return RTErrConvertFromErrno(errno);
        }
    }

#ifndef RT_OS_L4
    /* Update read pointer. */
    if (pStreamOSS->Out.fMMIO)
        pStreamOSS->old_optr = CountInfo.ptr;
#endif

    *pcbWritten = cbToWrite;
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnShutdown}
 */
static DECLCALLBACK(void) drvHostOssAudioHA_Shutdown(PPDMIHOSTAUDIO pInterface)
{
    RT_NOREF(pInterface);
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetStatus}
 */
static DECLCALLBACK(PDMAUDIOBACKENDSTS) drvHostOssAudioHA_GetStatus(PPDMIHOSTAUDIO pInterface, PDMAUDIODIR enmDir)
{
    AssertPtrReturn(pInterface, PDMAUDIOBACKENDSTS_UNKNOWN);
    RT_NOREF(enmDir);

    return PDMAUDIOBACKENDSTS_RUNNING;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCreate}
 */
static DECLCALLBACK(int) drvHostOssAudioHA_StreamCreate(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                        PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgReq,    VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgAcq,    VERR_INVALID_POINTER);

    POSSAUDIOSTREAM pStreamOSS = (POSSAUDIOSTREAM)pStream;

    int rc;
    if (pCfgReq->enmDir == PDMAUDIODIR_IN)
        rc = ossCreateStreamIn (pStreamOSS, pCfgReq, pCfgAcq);
    else
        rc = ossCreateStreamOut(pStreamOSS, pCfgReq, pCfgAcq);

    if (RT_SUCCESS(rc))
    {
        pStreamOSS->pCfg = PDMAudioStrmCfgDup(pCfgAcq);
        if (!pStreamOSS->pCfg)
            rc = VERR_NO_MEMORY;
    }

    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamDestroy}
 */
static DECLCALLBACK(int) drvHostOssAudioHA_StreamDestroy(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    POSSAUDIOSTREAM pStreamOSS = (POSSAUDIOSTREAM)pStream;

    if (!pStreamOSS->pCfg) /* Not (yet) configured? Skip. */
        return VINF_SUCCESS;

    int rc;
    if (pStreamOSS->pCfg->enmDir == PDMAUDIODIR_IN)
        rc = ossDestroyStreamIn(pStream);
    else
        rc = ossDestroyStreamOut(pStream);

    if (RT_SUCCESS(rc))
    {
        PDMAudioStrmCfgFree(pStreamOSS->pCfg);
        pStreamOSS->pCfg = NULL;
    }

    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamControl}
 */
static DECLCALLBACK(int) drvHostOssAudioHA_StreamControl(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                         PDMAUDIOSTREAMCMD enmStreamCmd)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    POSSAUDIOSTREAM pStreamOSS = (POSSAUDIOSTREAM)pStream;

    if (!pStreamOSS->pCfg) /* Not (yet) configured? Skip. */
        return VINF_SUCCESS;

    int rc;
    if (pStreamOSS->pCfg->enmDir == PDMAUDIODIR_IN)
        rc = ossControlStreamIn(/*pInterface,  pStream, enmStreamCmd*/);
    else
        rc = ossControlStreamOut(pStreamOSS, enmStreamCmd);

    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamIterate}
 */
static DECLCALLBACK(int) drvHostOssAudioHA_StreamIterate(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    /* Nothing to do here for OSS. */
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetReadable}
 */
static DECLCALLBACK(uint32_t) drvHostOssAudioHA_StreamGetReadable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);
    return UINT32_MAX;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetWritable}
 */
static DECLCALLBACK(uint32_t) drvHostOssAudioHA_StreamGetWritable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    POSSAUDIOSTREAM pStreamOSS = (POSSAUDIOSTREAM)pStream;
    AssertPtr(pStreamOSS);
    RT_NOREF(pInterface);

    /*
     * Note! This logic was found in StreamPlay and corrected a little.
     *
     * The logic here must match what StreamPlay does.
     */
    uint32_t cbWritable;
#ifndef RT_OS_L4
    count_info CountInfo = {0,0,0};
    if (pStreamOSS->Out.fMMIO)
    {
        /* Get current playback pointer. */
        int rc2 = ioctl(pStreamOSS->hFile, SNDCTL_DSP_GETOPTR, &CountInfo);
        AssertMsgReturn(rc2 >= 0, ("SNDCTL_DSP_GETOPTR failed: %s (%d)\n", strerror(errno), errno), 0);

        int cbData;
        if (CountInfo.ptr >= pStreamOSS->old_optr)
            cbData = CountInfo.ptr - pStreamOSS->old_optr;
        else
            cbData = pStreamOSS->cbBuf + CountInfo.ptr - pStreamOSS->old_optr;
        Assert(cbData >= 0);
        cbWritable = (unsigned)cbData;
    }
    else
#endif
    {
        audio_buf_info abinfo = { 0, 0, 0, 0 };
        int rc2 = ioctl(pStreamOSS->hFile, SNDCTL_DSP_GETOSPACE, &abinfo);
        AssertMsgReturn(rc2 >= 0, ("SNDCTL_DSP_GETOSPACE failed: %s (%d)\n", strerror(errno), errno), 0);

#if 0 /** @todo we could return abinfo.bytes here iff StreamPlay didn't use the fragmented approach */
        /** @todo r=bird: WTF do we make a fuss over abinfo.bytes for when we don't even use it?!? */
        AssertLogRelMsgReturn(abinfo.bytes >= 0, ("OSS: Warning: Invalid available size: %d\n", abinfo.bytes), VERR_INTERNAL_ERROR_3);
        if ((unsigned)abinfo.bytes > cbBuf)
        {
            LogRel2(("OSS: Warning: Too big output size (%d > %RU32), limiting to %RU32\n", abinfo.bytes, cbBuf, cbBuf));
            abinfo.bytes = cbBuf;
            /* Keep going. */
        }
#endif

        cbWritable = (uint32_t)(abinfo.fragments * abinfo.fragsize);
    }
    cbWritable = RT_MIN(cbWritable, pStreamOSS->cbBuf);
    return cbWritable;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetStatus}
 */
static DECLCALLBACK(PDMAUDIOSTREAMSTS) drvHostOssAudioHA_StreamGetStatus(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);
    return PDMAUDIOSTREAMSTS_FLAGS_INITIALIZED | PDMAUDIOSTREAMSTS_FLAGS_ENABLED;
}

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvHostOSSAudioQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS       pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVHOSTOSSAUDIO pThis   = PDMINS_2_DATA(pDrvIns, PDRVHOSTOSSAUDIO);

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
    RT_NOREF(pCfg, fFlags);
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);
    PDRVHOSTOSSAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTOSSAUDIO);
    LogRel(("Audio: Initializing OSS driver\n"));

    /*
     * Init the static parts.
     */
    pThis->pDrvIns                   = pDrvIns;
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface = drvHostOSSAudioQueryInterface;
    /* IHostAudio */
    pThis->IHostAudio.pfnInit               = drvHostOssAudioHA_Init;
    pThis->IHostAudio.pfnShutdown           = drvHostOssAudioHA_Shutdown;
    pThis->IHostAudio.pfnGetConfig          = drvHostOssAudioHA_GetConfig;
    pThis->IHostAudio.pfnGetStatus          = drvHostOssAudioHA_GetStatus;
    pThis->IHostAudio.pfnStreamCreate       = drvHostOssAudioHA_StreamCreate;
    pThis->IHostAudio.pfnStreamDestroy      = drvHostOssAudioHA_StreamDestroy;
    pThis->IHostAudio.pfnStreamControl      = drvHostOssAudioHA_StreamControl;
    pThis->IHostAudio.pfnStreamGetReadable  = drvHostOssAudioHA_StreamGetReadable;
    pThis->IHostAudio.pfnStreamGetWritable  = drvHostOssAudioHA_StreamGetWritable;
    pThis->IHostAudio.pfnStreamGetStatus    = drvHostOssAudioHA_StreamGetStatus;
    pThis->IHostAudio.pfnStreamIterate      = drvHostOssAudioHA_StreamIterate;
    pThis->IHostAudio.pfnStreamPlay         = drvHostOssAudioHA_StreamPlay;
    pThis->IHostAudio.pfnStreamCapture      = drvHostOssAudioHA_StreamCapture;
    pThis->IHostAudio.pfnSetCallback        = NULL;
    pThis->IHostAudio.pfnGetDevices         = NULL;
    pThis->IHostAudio.pfnStreamGetPending   = NULL;
    pThis->IHostAudio.pfnStreamPlayBegin    = NULL;
    pThis->IHostAudio.pfnStreamPlayEnd      = NULL;
    pThis->IHostAudio.pfnStreamCaptureBegin = NULL;
    pThis->IHostAudio.pfnStreamCaptureEnd   = NULL;

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

