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
#include <iprt/thread.h>
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
    PPDMDRVINS          pDrvIns;
    /** Pointer to host audio interface. */
    PDMIHOSTAUDIO       IHostAudio;
    /** Error count for not flooding the release log.
     *  UINT32_MAX for unlimited logging. */
    uint32_t            cLogErrors;
} DRVHOSTOSSAUDIO;
/** Pointer to the instance data for an OSS host audio driver. */
typedef DRVHOSTOSSAUDIO *PDRVHOSTOSSAUDIO;

/**
 * OSS audio stream configuration.
 */
typedef struct OSSAUDIOSTREAMCFG
{
    PDMAUDIOPCMPROPS    Props;
    uint16_t            cFragments;
    /** The log2 of cbFragment. */
    uint16_t            cbFragmentLog2;
    uint32_t            cbFragment;
} OSSAUDIOSTREAMCFG;
/** Pointer to an OSS audio stream configuration. */
typedef OSSAUDIOSTREAMCFG *POSSAUDIOSTREAMCFG;

/**
 * OSS audio stream.
 */
typedef struct OSSAUDIOSTREAM
{
    /** Common part. */
    PDMAUDIOBACKENDSTREAM   Core;
    /** The file descriptor. */
    int                     hFile;
    /** Buffer alignment. */
    uint8_t                 uAlign;
    /** Set if we're draining the stream (output only). */
    bool                    fDraining;
    /** Internal stream byte offset. */
    uint64_t                offInternal;
    /** The stream's acquired configuration. */
    PDMAUDIOSTREAMCFG       Cfg;
    /** The acquired OSS configuration. */
    OSSAUDIOSTREAMCFG       OssCfg;
    /** Handle to the thread draining output streams. */
    RTTHREAD                hThreadDrain;
} OSSAUDIOSTREAM;
/** Pointer to an OSS audio stream. */
typedef OSSAUDIOSTREAM *POSSAUDIOSTREAM;


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** The path to the output OSS device. */
static char g_szPathOutputDev[] = "/dev/dsp";
/** The path to the input OSS device. */
static char g_szPathInputDev[]  = "/dev/dsp";



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
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetConfig}
 */
static DECLCALLBACK(int) drvHostOssAudioHA_GetConfig(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDCFG pBackendCfg)
{
    RT_NOREF(pInterface);

    /*
     * Fill in the config structure.
     */
    RTStrCopy(pBackendCfg->szName, sizeof(pBackendCfg->szName), "OSS");
    pBackendCfg->cbStream       = sizeof(OSSAUDIOSTREAM);
    pBackendCfg->fFlags         = 0;
    pBackendCfg->cMaxStreamsIn  = 0;
    pBackendCfg->cMaxStreamsOut = 0;

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
             pOSSReq->cFragments, fInput ? "input" : "output", pOSSReq->cbFragment));

    int mmmmssss = (pOSSReq->cFragments << 16) | pOSSReq->cbFragmentLog2;
    AssertLogRelMsgReturn(ioctl(hFile, SNDCTL_DSP_SETFRAGMENT, &mmmmssss) >= 0,
                          ("OSS: Failed to set %RU16 fragments to %RU32 bytes each: %s (%d)\n",
                           pOSSReq->cFragments, pOSSReq->cbFragment, strerror(errno), errno),
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
        pOSSAcq->cFragments     = BufInfo.fragstotal;
        pOSSAcq->cbFragment     = BufInfo.fragsize;
        pOSSAcq->cbFragmentLog2 = ASMBitFirstSetU32(BufInfo.fragsize) - 1;
        Assert(RT_BIT_32(pOSSAcq->cbFragmentLog2) == pOSSAcq->cbFragment);

        LogRel2(("OSS: Got %RU16 %s fragments, %RU32 bytes each\n",
                 pOSSAcq->cFragments, fInput ? "input" : "output", pOSSAcq->cbFragment));
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

    pStreamOSS->hThreadDrain = NIL_RTTHREAD;

    /*
     * Open the device
     */
    int rc;
    if (pCfgReq->enmDir == PDMAUDIODIR_IN)
        pStreamOSS->hFile = open(g_szPathInputDev, O_RDONLY | O_NONBLOCK);
    else
        pStreamOSS->hFile = open(g_szPathOutputDev, O_WRONLY);
    if (pStreamOSS->hFile >= 0)
    {
        /*
         * Configure it.
         */
        OSSAUDIOSTREAMCFG ReqOssCfg;
        RT_ZERO(ReqOssCfg);

        memcpy(&ReqOssCfg.Props, &pCfgReq->Props, sizeof(PDMAUDIOPCMPROPS));
        ReqOssCfg.cbFragmentLog2 = 12;
        ReqOssCfg.cbFragment     = RT_BIT_32(ReqOssCfg.cbFragmentLog2);
        uint32_t const cbBuffer  = PDMAudioPropsFramesToBytes(&pCfgReq->Props, pCfgReq->Backend.cFramesBufferSize);
        ReqOssCfg.cFragments     = cbBuffer >> ReqOssCfg.cbFragmentLog2;
        AssertLogRelStmt(cbBuffer < ((uint32_t)0x7ffe << ReqOssCfg.cbFragmentLog2), ReqOssCfg.cFragments = 0x7ffe);

        rc = ossStreamConfigure(pStreamOSS->hFile, pCfgReq->enmDir == PDMAUDIODIR_IN, &ReqOssCfg, &pStreamOSS->OssCfg);
        if (RT_SUCCESS(rc))
        {
            pStreamOSS->uAlign = 0; /** @todo r=bird: Where did the correct assignment of this go? */

            /*
             * Complete the stream structure and fill in the pCfgAcq bits.
             */
            if ((pStreamOSS->OssCfg.cFragments * pStreamOSS->OssCfg.cbFragment) & pStreamOSS->uAlign)
                LogRel(("OSS: Warning: Misaligned playback buffer: Size = %zu, Alignment = %u\n",
                        pStreamOSS->OssCfg.cFragments * pStreamOSS->OssCfg.cbFragment, pStreamOSS->uAlign + 1));

            memcpy(&pCfgAcq->Props, &pStreamOSS->OssCfg.Props, sizeof(PDMAUDIOPCMPROPS));
            pCfgAcq->Backend.cFramesPeriod     = PDMAudioPropsBytesToFrames(&pCfgAcq->Props, pStreamOSS->OssCfg.cbFragment);
            pCfgAcq->Backend.cFramesBufferSize = pCfgAcq->Backend.cFramesPeriod * pStreamOSS->OssCfg.cFragments;
            if (pCfgReq->enmDir == PDMAUDIODIR_OUT)
                pCfgAcq->Backend.cFramesPreBuffering = (uint64_t)pCfgReq->Backend.cFramesPreBuffering
                                                     * pCfgAcq->Backend.cFramesBufferSize
                                                     / RT_MAX(pCfgReq->Backend.cFramesBufferSize, 1);
            else
                pCfgAcq->Backend.cFramesPreBuffering = 0; /** @todo is this sane? */

            /*
             * Copy the stream config and we're done!
             */
            PDMAudioStrmCfgCopy(&pStreamOSS->Cfg, pCfgAcq);
            return VINF_SUCCESS;
        }
        ossStreamClose(&pStreamOSS->hFile);
    }
    else
    {
        rc = RTErrConvertFromErrno(errno);
        LogRel(("OSS: Failed to open '%s': %s (%d) / %Rrc\n",
                pCfgReq->enmDir == PDMAUDIODIR_IN ? g_szPathInputDev : g_szPathOutputDev, strerror(errno), errno, rc));
    }
    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamDestroy}
 */
static DECLCALLBACK(int) drvHostOssAudioHA_StreamDestroy(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                         bool fImmediate)
{
    RT_NOREF(pInterface, fImmediate);
    POSSAUDIOSTREAM pStreamOSS = (POSSAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamOSS, VERR_INVALID_POINTER);

    ossStreamClose(&pStreamOSS->hFile);

    if (pStreamOSS->hThreadDrain != NIL_RTTHREAD)
    {
        int rc = RTThreadWait(pStreamOSS->hThreadDrain, 1, NULL);
        AssertRC(rc);
        pStreamOSS->hThreadDrain = NIL_RTTHREAD;
    }

    return VINF_SUCCESS;
}


/**
 * @ interface_method_impl{PDMIHOSTAUDIO,pfnStreamEnable}
 */
static DECLCALLBACK(int) drvHostOssAudioHA_StreamEnable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface);
    POSSAUDIOSTREAM pStreamOSS = (POSSAUDIOSTREAM)pStream;

    /** @todo this might be a little optimisitic...   */
    pStreamOSS->fDraining = false;

    int rc;
    if (pStreamOSS->Cfg.enmDir == PDMAUDIODIR_IN)
        rc = VINF_SUCCESS; /** @todo apparently nothing to do here? */
    else
    {
        int fMask = PCM_ENABLE_OUTPUT;
        if (ioctl(pStreamOSS->hFile, SNDCTL_DSP_SETTRIGGER, &fMask) >= 0)
            rc = VINF_SUCCESS;
        else
        {
            LogRel(("OSS: Failed to enable output stream: %s (%d)\n", strerror(errno), errno));
            rc = RTErrConvertFromErrno(errno);
        }
    }

    LogFlowFunc(("returns %Rrc for '%s'\n", rc, pStreamOSS->Cfg.szName));
    return rc;
}


/**
 * @ interface_method_impl{PDMIHOSTAUDIO,pfnStreamDisable}
 */
static DECLCALLBACK(int) drvHostOssAudioHA_StreamDisable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface);
    POSSAUDIOSTREAM pStreamOSS = (POSSAUDIOSTREAM)pStream;

    int rc;
    if (pStreamOSS->Cfg.enmDir == PDMAUDIODIR_IN)
        rc = VINF_SUCCESS; /** @todo apparently nothing to do here? */
    else
    {
        /*
         * If we're still draining, try kick the thread before we try disable the stream.
         */
        if (pStreamOSS->fDraining)
        {
            LogFlowFunc(("Trying to cancel draining...\n"));
            if (pStreamOSS->hThreadDrain != NIL_RTTHREAD)
            {
                RTThreadPoke(pStreamOSS->hThreadDrain);
                rc = RTThreadWait(pStreamOSS->hThreadDrain, 1 /*ms*/, NULL);
                if (RT_SUCCESS(rc) || rc == VERR_INVALID_HANDLE)
                    pStreamOSS->fDraining = false;
                else
                    LogFunc(("Failed to cancel draining (%Rrc)\n", rc));
            }
            else
            {
                LogFlowFunc(("Thread handle is NIL, so we can't be draining\n"));
                pStreamOSS->fDraining = false;
            }
        }

        /** @todo Official documentation says this isn't the right way to stop playback.
         *        It may work in some implementations but fail in all others...  Suggest
         *        using SNDCTL_DSP_RESET / SNDCTL_DSP_HALT. */
        int fMask = 0;
        if (ioctl(pStreamOSS->hFile, SNDCTL_DSP_SETTRIGGER, &fMask) >= 0)
            rc = VINF_SUCCESS;
        else
        {
            LogRel(("OSS: Failed to enable output stream: %s (%d)\n", strerror(errno), errno));
            rc = RTErrConvertFromErrno(errno);
        }
    }
    LogFlowFunc(("returns %Rrc for '%s'\n", rc, pStreamOSS->Cfg.szName));
    return rc;
}


/**
 * @ interface_method_impl{PDMIHOSTAUDIO,pfnStreamPause}
 */
static DECLCALLBACK(int) drvHostOssAudioHA_StreamPause(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    return drvHostOssAudioHA_StreamDisable(pInterface, pStream);
}


/**
 * @ interface_method_impl{PDMIHOSTAUDIO,pfnStreamResume}
 */
static DECLCALLBACK(int) drvHostOssAudioHA_StreamResume(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    return drvHostOssAudioHA_StreamEnable(pInterface, pStream);
}


/**
 * @callback_method_impl{FNRTTHREAD,
 * Thread for calling SNDCTL_DSP_SYNC (blocking) on an output stream.}
 */
static DECLCALLBACK(int) drvHostOssAudioDrainThread(RTTHREAD ThreadSelf, void *pvUser)
{
    RT_NOREF(ThreadSelf);
    POSSAUDIOSTREAM pStreamOSS = (POSSAUDIOSTREAM)pvUser;
    int rc;

    /* Make it blocking (for Linux). */
    int fOrgFlags = fcntl(pStreamOSS->hFile, F_GETFL, 0);
    LogFunc(("F_GETFL -> %#x\n", fOrgFlags));
    Assert(fOrgFlags != -1);
    if (fOrgFlags != -1)
    {
        rc = fcntl(pStreamOSS->hFile, F_SETFL, fOrgFlags & ~O_NONBLOCK);
        AssertStmt(rc != -1, fOrgFlags = -1);
    }

    /* Drain it. */
    LogFunc(("Calling SNDCTL_DSP_SYNC now...\n"));
    rc = ioctl(pStreamOSS->hFile, SNDCTL_DSP_SYNC, NULL);
    LogFunc(("SNDCTL_DSP_SYNC returned %d / errno=%d\n", rc, errno)); RT_NOREF(rc);

    /* Re-enable non-blocking mode and disable it. */
    if (fOrgFlags != -1)
    {
        rc = fcntl(pStreamOSS->hFile, F_SETFL, fOrgFlags);
        Assert(rc != -1);

        int fMask = 0;
        rc = ioctl(pStreamOSS->hFile, SNDCTL_DSP_SETTRIGGER, &fMask);
        Assert(rc >= 0);

        pStreamOSS->fDraining = false;
        LogFunc(("Restored non-block mode and cleared the trigger mask\n"));
    }

    return VINF_SUCCESS;
}


/**
 * @ interface_method_impl{PDMIHOSTAUDIO,pfnStreamDisable}
 */
static DECLCALLBACK(int) drvHostOssAudioHA_StreamDrain(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    PDRVHOSTOSSAUDIO pThis      = RT_FROM_MEMBER(pInterface, DRVHOSTOSSAUDIO, IHostAudio);
    POSSAUDIOSTREAM  pStreamOSS = (POSSAUDIOSTREAM)pStream;
    AssertReturn(pStreamOSS->Cfg.enmDir == PDMAUDIODIR_OUT, VERR_WRONG_ORDER);

    pStreamOSS->fDraining = true;

    /*
     * Because the SNDCTL_DSP_SYNC call is blocking on real OSS,
     * we kick off a thread to deal with it as we're probably on EMT
     * and cannot block for extended periods.
     */
    if (pStreamOSS->hThreadDrain != NIL_RTTHREAD)
    {
        int rc = RTThreadWait(pStreamOSS->hThreadDrain, 0, NULL);
        if (RT_SUCCESS(rc))
        {
            pStreamOSS->hThreadDrain = NIL_RTTHREAD;
            LogFunc(("Cleaned up stale thread handle.\n"));
        }
        else
        {
            LogFunc(("Drain thread already running (%Rrc).\n", rc));
            AssertMsg(rc == VERR_TIMEOUT, ("%Rrc\n", rc));
            return rc == VERR_TIMEOUT ? VINF_SUCCESS : rc;
        }
    }

    int rc = RTThreadCreateF(&pStreamOSS->hThreadDrain, drvHostOssAudioDrainThread, pStreamOSS, 0,
                             RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, "ossdrai%u", pThis->pDrvIns->iInstance);
    LogFunc(("Started drain thread: %Rrc\n", rc));
    AssertRCReturn(rc, rc);

    return VINF_SUCCESS;
}



/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamControl}
 */
static DECLCALLBACK(int) drvHostOssAudioHA_StreamControl(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                         PDMAUDIOSTREAMCMD enmStreamCmd)
{
    /** @todo r=bird: I'd like to get rid of this pfnStreamControl method,
     *        replacing it with individual StreamXxxx methods.  That would save us
     *        potentally huge switches and more easily see which drivers implement
     *        which operations (grep for pfnStreamXxxx). */
    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
            return drvHostOssAudioHA_StreamEnable(pInterface, pStream);
        case PDMAUDIOSTREAMCMD_DISABLE:
            return drvHostOssAudioHA_StreamDisable(pInterface, pStream);
        case PDMAUDIOSTREAMCMD_PAUSE:
            return drvHostOssAudioHA_StreamPause(pInterface, pStream);
        case PDMAUDIOSTREAMCMD_RESUME:
            return drvHostOssAudioHA_StreamResume(pInterface, pStream);
        case PDMAUDIOSTREAMCMD_DRAIN:
            return drvHostOssAudioHA_StreamDrain(pInterface, pStream);
            /** @todo the drain call for OSS is SNDCTL_DSP_SYNC, however in the non-ALSA
             *        implementation of OSS it is probably blocking.  Also, it comes with
             *        caveats about clicks and silence...  */
        case PDMAUDIOSTREAMCMD_END:
        case PDMAUDIOSTREAMCMD_32BIT_HACK:
        case PDMAUDIOSTREAMCMD_INVALID:
            /* no default*/
            break;
    }
    return VERR_NOT_SUPPORTED;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetReadable}
 */
static DECLCALLBACK(uint32_t) drvHostOssAudioHA_StreamGetReadable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);
    Log4Func(("returns UINT32_MAX\n"));
    return UINT32_MAX;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetWritable}
 */
static DECLCALLBACK(uint32_t) drvHostOssAudioHA_StreamGetWritable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface);
    POSSAUDIOSTREAM pStreamOSS = (POSSAUDIOSTREAM)pStream;
    AssertPtr(pStreamOSS);

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

    uint32_t cbRet = (uint32_t)(BufInfo.fragments * BufInfo.fragsize);
    Log4Func(("returns %#x (%u)\n", cbRet, cbRet));
    return cbRet;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetState}
 */
static DECLCALLBACK(PDMHOSTAUDIOSTREAMSTATE) drvHostOssAudioHA_StreamGetState(PPDMIHOSTAUDIO pInterface,
                                                                              PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface);
    POSSAUDIOSTREAM pStreamOSS = (POSSAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamOSS, PDMHOSTAUDIOSTREAMSTATE_INVALID);
    if (!pStreamOSS->fDraining)
        return PDMHOSTAUDIOSTREAMSTATE_OKAY;
    return PDMHOSTAUDIOSTREAMSTATE_DRAINING;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamPlay}
 */
static DECLCALLBACK(int) drvHostOssAudioHA_StreamPlay(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                      const void *pvBuf, uint32_t cbBuf, uint32_t *pcbWritten)
{
    RT_NOREF(pInterface);
    POSSAUDIOSTREAM pStreamOSS = (POSSAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamOSS, VERR_INVALID_POINTER);

    /*
     * Figure out now much to write.
     */
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
    uint32_t cbToWrite = (uint32_t)(BufInfo.fragments * BufInfo.fragsize);
    cbToWrite = RT_MIN(cbToWrite, cbBuf);
    Log3Func(("@%#RX64 cbBuf=%#x BufInfo: fragments=%#x fragstotal=%#x fragsize=%#x bytes=%#x %s cbToWrite=%#x\n",
              pStreamOSS->offInternal, cbBuf, BufInfo.fragments, BufInfo.fragstotal, BufInfo.fragsize, BufInfo.bytes,
              pStreamOSS->Cfg.szName, cbToWrite));

    /*
     * Write.
     */
    uint8_t const *pbBuf    = (uint8_t const *)pvBuf;
    uint32_t       cbChunk  = cbToWrite;
    uint32_t       offChunk = 0;
    while (cbChunk > 0)
    {
        ssize_t cbWritten = write(pStreamOSS->hFile, &pbBuf[offChunk], RT_MIN(cbChunk, pStreamOSS->OssCfg.cbFragment));
        if (cbWritten > 0)
        {
            AssertLogRelMsg(!(cbWritten & pStreamOSS->uAlign),
                            ("OSS: Misaligned write (written %#zx, alignment %#x)\n", cbWritten, pStreamOSS->uAlign));

            Assert((uint32_t)cbWritten <= cbChunk);
            offChunk += (uint32_t)cbWritten;
            cbChunk  -= (uint32_t)cbWritten;
            pStreamOSS->offInternal += cbWritten;
        }
        else if (cbWritten == 0)
        {
            LogFunc(("@%#RX64 write(%#x) returned zeroed (previously wrote %#x bytes)!\n",
                     pStreamOSS->offInternal, RT_MIN(cbChunk, pStreamOSS->OssCfg.cbFragment), cbWritten));
            break;
        }
        else
        {
            LogRel(("OSS: Failed writing output data: %s (%d)\n", strerror(errno), errno));
            return RTErrConvertFromErrno(errno);
        }
    }

    *pcbWritten = offChunk;
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCapture}
 */
static DECLCALLBACK(int) drvHostOssAudioHA_StreamCapture(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                         void *pvBuf, uint32_t cbBuf, uint32_t *pcbRead)
{
    RT_NOREF(pInterface);
    POSSAUDIOSTREAM pStreamOSS = (POSSAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamOSS, VERR_INVALID_POINTER);
    Log3Func(("@%#RX64 cbBuf=%#x %s\n", pStreamOSS->offInternal, cbBuf, pStreamOSS->Cfg.szName));

    size_t          cbToRead = cbBuf;
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
            pStreamOSS->offInternal += cbRead;
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

    *pcbRead = offWrite;
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
    pThis->IHostAudio.pfnGetConfig                  = drvHostOssAudioHA_GetConfig;
    pThis->IHostAudio.pfnGetDevices                 = NULL;
    pThis->IHostAudio.pfnGetStatus                  = drvHostOssAudioHA_GetStatus;
    pThis->IHostAudio.pfnDoOnWorkerThread           = NULL;
    pThis->IHostAudio.pfnStreamConfigHint           = NULL;
    pThis->IHostAudio.pfnStreamCreate               = drvHostOssAudioHA_StreamCreate;
    pThis->IHostAudio.pfnStreamInitAsync            = NULL;
    pThis->IHostAudio.pfnStreamDestroy              = drvHostOssAudioHA_StreamDestroy;
    pThis->IHostAudio.pfnStreamNotifyDeviceChanged  = NULL;
    pThis->IHostAudio.pfnStreamControl              = drvHostOssAudioHA_StreamControl;
    pThis->IHostAudio.pfnStreamGetReadable          = drvHostOssAudioHA_StreamGetReadable;
    pThis->IHostAudio.pfnStreamGetWritable          = drvHostOssAudioHA_StreamGetWritable;
    pThis->IHostAudio.pfnStreamGetPending           = NULL;
    pThis->IHostAudio.pfnStreamGetState             = drvHostOssAudioHA_StreamGetState;
    pThis->IHostAudio.pfnStreamPlay                 = drvHostOssAudioHA_StreamPlay;
    pThis->IHostAudio.pfnStreamCapture              = drvHostOssAudioHA_StreamCapture;

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

