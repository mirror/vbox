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
    int                hFile;
    int                cFragments;
    int                cbFragmentSize;
    int                old_optr;
} OSSAUDIOSTREAM, *POSSAUDIOSTREAM;

typedef struct OSSAUDIOCFG
{
    int nfrags;
    int fragsize;
    const char *devpath_out;
    const char *devpath_in;
    int debug;
} OSSAUDIOCFG, *POSSAUDIOCFG;

static OSSAUDIOCFG s_OSSConf =
{
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
    return popcount((u & -u) - 1);
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
        rc = RTErrConvertFromErrno(errno);
        LogRel(("OSS: Closing stream failed: %s / %Rrc\n", strerror(errno), rc));
    }
    else
    {
        *phFile = -1;
        rc = VINF_SUCCESS;
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
 * @interface_method_impl{PDMIHOSTAUDIO,pfnShutdown}
 */
static DECLCALLBACK(void) drvHostOssAudioHA_Shutdown(PPDMIHOSTAUDIO pInterface)
{
    RT_NOREF(pInterface);
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetConfig}
 */
static DECLCALLBACK(int) drvHostOssAudioHA_GetConfig(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDCFG pBackendCfg)
{
    RT_NOREF(pInterface);

    RTStrCopy(pBackendCfg->szName, sizeof(pBackendCfg->szName), "OSS");

    pBackendCfg->cbStreamIn  = sizeof(OSSAUDIOSTREAM);
    pBackendCfg->cbStreamOut = sizeof(OSSAUDIOSTREAM);

    int hFile = open("/dev/dsp", O_WRONLY | O_NONBLOCK, 0);
    if (hFile == -1)
    {
        /* Try opening the mixing device instead. */
        hFile = open("/dev/mixer", O_RDONLY | O_NONBLOCK, 0);
    }
    if (hFile != -1)
    {
        int ossVer = -1;
        int err = ioctl(hFile, OSS_GETVERSION, &ossVer);
        if (err == 0)
        {
            LogRel2(("OSS: Using version: %d\n", ossVer));
#ifdef VBOX_WITH_AUDIO_OSS_SYSINFO
            oss_sysinfo ossInfo;
            RT_ZERO(ossInfo);
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
#endif
            {
                /* Since we cannot query anything, assume that we have at least
                 * one input and one output if we found "/dev/dsp" or "/dev/mixer". */

                pBackendCfg->cMaxStreamsIn   = UINT32_MAX;
                pBackendCfg->cMaxStreamsOut  = UINT32_MAX;
            }
        }
        else
            LogRel(("OSS: Unable to determine installed version: %s (%d)\n", strerror(err), err));
        close(hFile);
    }
    else
        LogRel(("OSS: No devices found, audio is not available\n"));

    return VINF_SUCCESS;
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


static int ossStreamConfigure(int hFile, bool fInput, POSSAUDIOSTREAMCFG pOSSReq, POSSAUDIOSTREAMCFG pOSSAcq)
{
    /*
     * Format.
     */
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
            LogRel2(("OSS: Unsupported sample size: %u\n", PDMAudioPropsSampleSize(&pOSSReq->Props)));
            return VERR_AUDIO_STREAM_COULD_NOT_CREATE;
    }
    AssertLogRelMsgReturn(ioctl(hFile, SNDCTL_DSP_SAMPLESIZE, &iFormat) >= 0,
                          ("OSS: Failed to set audio format to %d: %s (%d)\n", iFormat, strerror(errno), errno),
                          RTErrConvertFromErrno(errno));

    /*
     * Channel count.
     */
    int cChannels = PDMAudioPropsChannels(&pOSSReq->Props);
    AssertLogRelMsgReturn(ioctl(hFile, SNDCTL_DSP_CHANNELS, &cChannels) >= 0,
                          ("OSS: Failed to set number of audio channels (%RU8): %s (%d)\n",
                           PDMAudioPropsChannels(&pOSSReq->Props), strerror(errno), errno),
                          RTErrConvertFromErrno(errno));

    /*
     * Frequency.
     */
    int iFrequenc = pOSSReq->Props.uHz;
    AssertLogRelMsgReturn(ioctl(hFile, SNDCTL_DSP_SPEED, &iFrequenc) >= 0,
                          ("OSS: Failed to set audio frequency to %d Hz: %s (%d)\n", pOSSReq->Props.uHz, strerror(errno), errno),
                          RTErrConvertFromErrno(errno));


    /*
     * Set obsolete non-blocking call for input streams.
     */
    if (fInput)
    {
#if !(defined(VBOX) && defined(RT_OS_SOLARIS)) /* Obsolete on Solaris (using O_NONBLOCK is sufficient). */
        AssertLogRelMsgReturn(ioctl(hFile, SNDCTL_DSP_NONBLOCK, NULL) >= 0,
                              ("OSS: Failed to set non-blocking mode: %s (%d)\n", strerror(errno), errno),
                              RTErrConvertFromErrno(errno));
#endif
    }

    /*
     * Set fragment size and count.
     */
    LogRel2(("OSS: Requested %RU16 %s fragments, %RU32 bytes each\n",
             pOSSReq->cFragments, fInput ? "input" : "output", pOSSReq->cbFragmentSize));

    int mmmmssss = (pOSSReq->cFragments << 16) | lsbindex(pOSSReq->cbFragmentSize);
    AssertLogRelMsgReturn(ioctl(hFile, SNDCTL_DSP_SETFRAGMENT, &mmmmssss) >= 0,
                          ("OSS: Failed to set %RU16 fragments to %RU32 bytes each: %s (%d)\n",
                           pOSSReq->cFragments, pOSSReq->cbFragmentSize, strerror(errno), errno),
                          RTErrConvertFromErrno(errno));

    /*
     * Get parameters and popuplate pOSSAcq.
     */
    audio_buf_info BufInfo = { 0, 0, 0, 0 };
    AssertLogRelMsgReturn(ioctl(hFile, fInput ? SNDCTL_DSP_GETISPACE : SNDCTL_DSP_GETOSPACE, &BufInfo) >= 0,
                          ("OSS: Failed to retrieve %s buffer length: %s (%d)\n",
                           fInput ? "input" : "output", strerror(errno), errno),
                          RTErrConvertFromErrno(errno));

    int rc = ossOSSToAudioProps(&pOSSAcq->Props, iFormat, cChannels, iFrequenc);
    if (RT_SUCCESS(rc))
    {
        pOSSAcq->cFragments      = BufInfo.fragstotal;
        pOSSAcq->cbFragmentSize  = BufInfo.fragsize;

        LogRel2(("OSS: Got %RU16 %s fragments, %RU32 bytes each\n",
                 pOSSAcq->cFragments, fInput ? "input" : "output", pOSSAcq->cbFragmentSize));
    }

    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCreate}
 */
static DECLCALLBACK(int) drvHostOssAudioHA_StreamCreate(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                        PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    AssertPtr(pInterface); RT_NOREF(pInterface);
    POSSAUDIOSTREAM pStreamOSS = (POSSAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamOSS, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgReq, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgAcq, VERR_INVALID_POINTER);

    /*
     * Open the device
     */
    int rc;
    if (pCfgReq->enmDir == PDMAUDIODIR_IN)
        pStreamOSS->hFile = open(s_OSSConf.devpath_in, O_RDONLY | O_NONBLOCK);
    else
        pStreamOSS->hFile = open(s_OSSConf.devpath_out, O_WRONLY);
    if (pStreamOSS->hFile >= 0)
    {
        /*
         * Configure it.
         */
        OSSAUDIOSTREAMCFG ReqOssCfg;
        RT_ZERO(ReqOssCfg);

        memcpy(&ReqOssCfg.Props, &pCfgReq->Props, sizeof(PDMAUDIOPCMPROPS));
        ReqOssCfg.cFragments     = s_OSSConf.nfrags;
        ReqOssCfg.cbFragmentSize = s_OSSConf.fragsize;

        OSSAUDIOSTREAMCFG AcqOssCfg;
        RT_ZERO(AcqOssCfg);
        rc = ossStreamConfigure(pStreamOSS->hFile, pCfgReq->enmDir == PDMAUDIODIR_IN, &ReqOssCfg, &AcqOssCfg);
        if (RT_SUCCESS(rc))
        {
            /*
             * Complete the stream structure and fill in the pCfgAcq bits.
             */
            if ((AcqOssCfg.cFragments * AcqOssCfg.cbFragmentSize) & pStreamOSS->uAlign)
                LogRel(("OSS: Warning: Misaligned playback buffer: Size = %zu, Alignment = %u\n",
                        AcqOssCfg.cFragments * AcqOssCfg.cbFragmentSize, pStreamOSS->uAlign + 1));

            memcpy(&pCfgAcq->Props, &AcqOssCfg.Props, sizeof(PDMAUDIOPCMPROPS));
            pCfgAcq->Backend.cFramesPeriod     = PDMAUDIOSTREAMCFG_B2F(pCfgAcq, AcqOssCfg.cbFragmentSize);
            pCfgAcq->Backend.cFramesBufferSize = pCfgAcq->Backend.cFramesPeriod * 2; /* Use "double buffering" */
            /** @todo Pre-buffering required? */

            /*
             * Duplicate the stream config and we're done!
             */
            pStreamOSS->pCfg = PDMAudioStrmCfgDup(pCfgAcq);
            if (pStreamOSS->pCfg)
                return VINF_SUCCESS;

            rc = VERR_NO_MEMORY;
        }
        ossStreamClose(&pStreamOSS->hFile);
    }
    else
    {
        rc = RTErrConvertFromErrno(errno);
        LogRel(("OSS: Failed to open '%s': %s (%d) / %Rrc\n",
                pCfgReq->enmDir == PDMAUDIODIR_IN ? s_OSSConf.devpath_in : s_OSSConf.devpath_out, strerror(errno), errno, rc));
    }
    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamDestroy}
 */
static DECLCALLBACK(int) drvHostOssAudioHA_StreamDestroy(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface);
    POSSAUDIOSTREAM pStreamOSS = (POSSAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamOSS, VERR_INVALID_POINTER);

    ossStreamClose(&pStreamOSS->hFile);
    PDMAudioStrmCfgFree(pStreamOSS->pCfg);
    pStreamOSS->pCfg = NULL;

    return VINF_SUCCESS;
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
    audio_buf_info BufInfo = { 0, 0, 0, 0 };
    int rc2 = ioctl(pStreamOSS->hFile, SNDCTL_DSP_GETOSPACE, &BufInfo);
    AssertMsgReturn(rc2 >= 0, ("SNDCTL_DSP_GETOSPACE failed: %s (%d)\n", strerror(errno), errno), 0);

#if 0 /** @todo we could return BufInfo.bytes here iff StreamPlay didn't use the fragmented approach */
    /** @todo r=bird: WTF do we make a fuss over BufInfo.bytes for when we don't
     *        even use it?!? */
    AssertLogRelMsgReturn(BufInfo.bytes >= 0, ("OSS: Warning: Invalid available size: %d\n", BufInfo.bytes), VERR_INTERNAL_ERROR_3);
    if ((unsigned)BufInfo.bytes > cbBuf)
    {
        LogRel2(("OSS: Warning: Too big output size (%d > %RU32), limiting to %RU32\n", BufInfo.bytes, cbBuf, cbBuf));
        BufInfo.bytes = cbBuf;
        /* Keep going. */
    }
#endif

    return (uint32_t)(BufInfo.fragments * BufInfo.fragsize);
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
    audio_buf_info BufInfo;
    int rc2 = ioctl(pStreamOSS->hFile, SNDCTL_DSP_GETOSPACE, &BufInfo);
    AssertLogRelMsgReturn(rc2 >= 0, ("OSS: Failed to retrieve current playback buffer: %s (%d)\n", strerror(errno), errno),
                          RTErrConvertFromErrno(errno));

#if 0   /** @todo r=bird: WTF do we make a fuss over BufInfo.bytes for when we don't even use it?!? */
    AssertLogRelMsgReturn(BufInfo.bytes >= 0, ("OSS: Warning: Invalid available size: %d\n", BufInfo.bytes), VERR_INTERNAL_ERROR_3);
    if ((unsigned)BufInfo.bytes > cbBuf)
    {
        LogRel2(("OSS: Warning: Too big output size (%d > %RU32), limiting to %RU32\n", BufInfo.bytes, cbBuf, cbBuf));
        BufInfo.bytes = cbBuf;
        /* Keep going. */
    }
#endif
    cbToWrite = (unsigned)(BufInfo.fragments * BufInfo.fragsize);
    cbToWrite = RT_MIN(cbToWrite, cbBuf);

    /*
     * Write.
     */
    uint8_t const *pbBuf    = (uint8_t const *)pvBuf;
    uint32_t       cbChunk  = cbToWrite;
    uint32_t       offChunk = 0;
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

    *pcbWritten = cbToWrite;
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCapture}
 */
static DECLCALLBACK(int) drvHostOssAudioHA_StreamCapture(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                         void *pvBuf, uint32_t uBufSize, uint32_t *puRead)
{
    POSSAUDIOSTREAM pStreamOSS = (POSSAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamOSS, VERR_INVALID_POINTER);
    RT_NOREF(pInterface);


    size_t cbToRead = uBufSize;
    LogFlowFunc(("cbToRead=%zi\n", cbToRead));

    uint8_t * const pbDst    = (uint8_t *)pvBuf;
    size_t          offWrite = 0;
    while (cbToRead > 0)
    {
        ssize_t cbRead = read(pStreamOSS->hFile, &pbDst[offWrite], cbToRead);
        if (cbRead)
        {
            LogFlowFunc(("cbRead=%zi, offWrite=%zu cbToRead=%zu\n", cbRead, offWrite, cbToRead));
            Assert((ssize_t)cbToRead >= cbRead);
            cbToRead    -= cbRead;
            offWrite    += cbRead;
        }
        else
        {
            LogFunc(("cbRead=%zi, offWrite=%zu cbToRead=%zu errno=%d\n", cbRead, offWrite, cbToRead, errno));

            /* Don't complain about errors if we've retrieved some audio data already.  */
            if (cbRead < 0 && offWrite == 0 && errno != EINTR && errno != EAGAIN)
            {
                AssertStmt(errno != 0, errno = EACCES);
                int rc = RTErrConvertFromErrno(errno);
                LogFunc(("Failed to read %zu input frames, errno=%d rc=%Rrc\n", cbToRead, errno, rc));
                return rc;
            }
            break;
        }
    }

    if (puRead)
        *puRead = offWrite;
    return VINF_SUCCESS;
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

