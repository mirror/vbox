/* $Id$ */
/** @file
 * HDAStream.cpp - Stream functions for HD Audio.
 */

/*
 * Copyright (C) 2017-2020 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DEV_HDA
#include <VBox/log.h>

#include <iprt/mem.h>
#include <iprt/semaphore.h>

#include <VBox/AssertGuest.h>
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pdmaudioifs.h>

#include "DrvAudio.h"

#include "DevHDA.h"
#include "HDAStream.h"

#ifdef VBOX_WITH_DTRACE
# include "dtrace/VBoxDD.h"
#endif

#ifdef IN_RING3 /* whole file */


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static void hdaR3StreamSetPositionAbs(PHDASTREAM pStreamShared, PPDMDEVINS pDevIns, PHDASTATE pThis, uint32_t uLPIB);

static int  hdaR3StreamAsyncIODestroy(PHDASTREAMR3 pStreamR3);
static int  hdaR3StreamAsyncIONotify(PHDASTREAMR3 pStreamR3);



/**
 * Creates an HDA stream.
 *
 * @returns IPRT status code.
 * @param   pStreamShared       The HDA stream to construct - shared bits.
 * @param   pStreamR3           The HDA stream to construct - ring-3 bits.
 * @param   pThis               The shared HDA device instance.
 * @param   pThisCC             The ring-3 HDA device instance.
 * @param   uSD                 Stream descriptor number to assign.
 */
int hdaR3StreamConstruct(PHDASTREAM pStreamShared, PHDASTREAMR3 pStreamR3, PHDASTATE pThis, PHDASTATER3 pThisCC, uint8_t uSD)
{
    int rc;

    pStreamR3->u8SD             = uSD;
    pStreamShared->u8SD         = uSD;
    pStreamR3->pMixSink         = NULL;
    pStreamR3->pHDAStateShared  = pThis;
    pStreamR3->pHDAStateR3      = pThisCC;
    Assert(pStreamShared->hTimer != NIL_TMTIMERHANDLE); /* hdaR3Construct initalized this one already. */

    pStreamShared->State.fInReset = false;
    pStreamShared->State.fRunning = false;
#ifdef HDA_USE_DMA_ACCESS_HANDLER
    RTListInit(&pStreamR3->State.lstDMAHandlers);
#endif

#ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
    AssertPtr(pStreamR3->pHDAStateR3);
    AssertPtr(pStreamR3->pHDAStateR3->pDevIns);
    rc = PDMDevHlpCritSectInit(pStreamR3->pHDAStateR3->pDevIns, &pStreamShared->CritSect,
                               RT_SRC_POS, "hda_sd#%RU8", pStreamShared->u8SD);
    AssertRCReturn(rc, rc);
#endif

    rc = hdaR3StreamPeriodCreate(&pStreamShared->State.Period);
    AssertRCReturn(rc, rc);

#ifdef DEBUG
    rc = RTCritSectInit(&pStreamR3->Dbg.CritSect);
    AssertRCReturn(rc, rc);
#endif

    const bool fIsInput = hdaGetDirFromSD(uSD) == PDMAUDIODIR_IN;

    if (fIsInput)
    {
        pStreamShared->State.Cfg.u.enmSrc = PDMAUDIORECSRC_UNKNOWN;
        pStreamShared->State.Cfg.enmDir   = PDMAUDIODIR_IN;
    }
    else
    {
        pStreamShared->State.Cfg.u.enmDst = PDMAUDIOPLAYBACKDST_UNKNOWN;
        pStreamShared->State.Cfg.enmDir   = PDMAUDIODIR_OUT;
    }

    pStreamR3->Dbg.Runtime.fEnabled = pThisCC->Dbg.fEnabled;

    if (pStreamR3->Dbg.Runtime.fEnabled)
    {
        char szFile[64];
        char szPath[RTPATH_MAX];

        /* pFileStream */
        if (fIsInput)
            RTStrPrintf(szFile, sizeof(szFile), "hdaStreamWriteSD%RU8", uSD);
        else
            RTStrPrintf(szFile, sizeof(szFile), "hdaStreamReadSD%RU8", uSD);

        int rc2 = DrvAudioHlpFileNameGet(szPath, sizeof(szPath), pThisCC->Dbg.pszOutPath, szFile,
                                         0 /* uInst */, PDMAUDIOFILETYPE_WAV, PDMAUDIOFILENAME_FLAGS_NONE);
        AssertRC(rc2);

        rc2 = DrvAudioHlpFileCreate(PDMAUDIOFILETYPE_WAV, szPath, PDMAUDIOFILE_FLAGS_NONE, &pStreamR3->Dbg.Runtime.pFileStream);
        AssertRC(rc2);

        /* pFileDMARaw */
        if (fIsInput)
            RTStrPrintf(szFile, sizeof(szFile), "hdaDMARawWriteSD%RU8", uSD);
        else
            RTStrPrintf(szFile, sizeof(szFile), "hdaDMARawReadSD%RU8", uSD);

        rc2 = DrvAudioHlpFileNameGet(szPath, sizeof(szPath), pThisCC->Dbg.pszOutPath, szFile,
                                     0 /* uInst */, PDMAUDIOFILETYPE_WAV, PDMAUDIOFILENAME_FLAGS_NONE);
        AssertRC(rc2);

        rc2 = DrvAudioHlpFileCreate(PDMAUDIOFILETYPE_WAV, szPath, PDMAUDIOFILE_FLAGS_NONE, &pStreamR3->Dbg.Runtime.pFileDMARaw);
        AssertRC(rc2);

        /* pFileDMAMapped */
        if (fIsInput)
            RTStrPrintf(szFile, sizeof(szFile), "hdaDMAWriteMappedSD%RU8", uSD);
        else
            RTStrPrintf(szFile, sizeof(szFile), "hdaDMAReadMappedSD%RU8", uSD);

        rc2 = DrvAudioHlpFileNameGet(szPath, sizeof(szPath), pThisCC->Dbg.pszOutPath, szFile,
                                     0 /* uInst */, PDMAUDIOFILETYPE_WAV, PDMAUDIOFILENAME_FLAGS_NONE);
        AssertRC(rc2);

        rc2 = DrvAudioHlpFileCreate(PDMAUDIOFILETYPE_WAV, szPath, PDMAUDIOFILE_FLAGS_NONE, &pStreamR3->Dbg.Runtime.pFileDMAMapped);
        AssertRC(rc2);

        /* Delete stale debugging files from a former run. */
        DrvAudioHlpFileDelete(pStreamR3->Dbg.Runtime.pFileStream);
        DrvAudioHlpFileDelete(pStreamR3->Dbg.Runtime.pFileDMARaw);
        DrvAudioHlpFileDelete(pStreamR3->Dbg.Runtime.pFileDMAMapped);
    }

    return rc;
}

/**
 * Destroys an HDA stream.
 *
 * @param   pStreamShared       The HDA stream to destroy - shared bits.
 * @param   pStreamR3           The HDA stream to destroy - ring-3 bits.
 */
void hdaR3StreamDestroy(PHDASTREAM pStreamShared, PHDASTREAMR3 pStreamR3)
{
    LogFlowFunc(("[SD%RU8] Destroying ...\n", pStreamShared->u8SD));

    hdaR3StreamMapDestroy(&pStreamR3->State.Mapping);

    int rc2;

#ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
    rc2 = hdaR3StreamAsyncIODestroy(pStreamR3);
    AssertRC(rc2);
#endif

#ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
    if (PDMCritSectIsInitialized(&pStreamShared->CritSect))
    {
        rc2 = PDMR3CritSectDelete(&pStreamShared->CritSect);
        AssertRC(rc2);
    }
#endif

    if (pStreamR3->State.pCircBuf)
    {
        RTCircBufDestroy(pStreamR3->State.pCircBuf);
        pStreamR3->State.pCircBuf = NULL;
    }

    hdaR3StreamPeriodDestroy(&pStreamShared->State.Period);

#ifdef DEBUG
    if (RTCritSectIsInitialized(&pStreamR3->Dbg.CritSect))
    {
        rc2 = RTCritSectDelete(&pStreamR3->Dbg.CritSect);
        AssertRC(rc2);
    }
#endif

    if (pStreamR3->Dbg.Runtime.fEnabled)
    {
        DrvAudioHlpFileDestroy(pStreamR3->Dbg.Runtime.pFileStream);
        pStreamR3->Dbg.Runtime.pFileStream = NULL;

        DrvAudioHlpFileDestroy(pStreamR3->Dbg.Runtime.pFileDMARaw);
        pStreamR3->Dbg.Runtime.pFileDMARaw = NULL;

        DrvAudioHlpFileDestroy(pStreamR3->Dbg.Runtime.pFileDMAMapped);
        pStreamR3->Dbg.Runtime.pFileDMAMapped = NULL;
    }

    LogFlowFuncLeave();
}

/**
 * Sets up ((re-)iniitalizes) an HDA stream.
 *
 * @returns IPRT status code. VINF_NO_CHANGE if the stream does not need
 *          be set-up again because the stream's (hardware) parameters did
 *          not change.
 * @param   pDevIns         The device instance.
 * @param   pThis           The shared HDA device state (for HW register
 *                          parameters).
 * @param   pStreamShared   HDA stream to set up, shared portion.
 * @param   pStreamR3       HDA stream to set up, ring-3 portion.
 * @param   uSD             Stream descriptor number to assign it.
 */
int hdaR3StreamSetUp(PPDMDEVINS pDevIns, PHDASTATE pThis, PHDASTREAM pStreamShared, PHDASTREAMR3 pStreamR3, uint8_t uSD)
{
    /* This must be valid all times. */
    AssertReturn(uSD < HDA_MAX_STREAMS, VERR_INVALID_PARAMETER);

    /* These member can only change on data corruption, despite what the code does further down (bird).  */
    AssertReturn(pStreamShared->u8SD == uSD, VERR_WRONG_ORDER);
    AssertReturn(pStreamR3->u8SD     == uSD, VERR_WRONG_ORDER);

    const uint64_t u64BDLBase = RT_MAKE_U64(HDA_STREAM_REG(pThis, BDPL, uSD),
                                            HDA_STREAM_REG(pThis, BDPU, uSD));
    const uint16_t u16LVI     = HDA_STREAM_REG(pThis, LVI, uSD);
    const uint32_t u32CBL     = HDA_STREAM_REG(pThis, CBL, uSD);
    const uint8_t  u8FIFOS    = HDA_STREAM_REG(pThis, FIFOS, uSD) + 1;
          uint8_t  u8FIFOW    = hdaSDFIFOWToBytes(HDA_STREAM_REG(pThis, FIFOW, uSD));
    const uint16_t u16FMT     = HDA_STREAM_REG(pThis, FMT, uSD);

    /* Is the bare minimum set of registers configured for the stream?
     * If not, bail out early, as there's nothing to do here for us (yet). */
    if (   !u64BDLBase
        || !u16LVI
        || !u32CBL
        || !u8FIFOS
        || !u8FIFOW
        || !u16FMT)
    {
        LogFunc(("[SD%RU8] Registers not set up yet, skipping (re-)initialization\n", uSD));
        return VINF_SUCCESS;
    }

    PDMAUDIOPCMPROPS Props;
    int rc = hdaR3SDFMTToPCMProps(u16FMT, &Props);
    if (RT_FAILURE(rc))
    {
        LogRelMax(32, ("HDA: Warning: Format 0x%x for stream #%RU8 not supported\n", HDA_STREAM_REG(pThis, FMT, uSD), uSD));
        return rc;
    }

    /* Reset (any former) stream map. */
    hdaR3StreamMapReset(&pStreamR3->State.Mapping);

    /*
     * Initialize the stream mapping in any case, regardless if
     * we support surround audio or not. This is needed to handle
     * the supported channels within a single audio stream, e.g. mono/stereo.
     *
     * In other words, the stream mapping *always* knows the real
     * number of channels in a single audio stream.
     */
    rc = hdaR3StreamMapInit(&pStreamR3->State.Mapping, &Props);
    AssertRCReturn(rc, rc);

    ASSERT_GUEST_LOGREL_MSG_RETURN(   pStreamR3->State.Mapping.cbFrameSize > 0
                                   && u32CBL % pStreamR3->State.Mapping.cbFrameSize == 0,
                                   ("CBL for stream #%RU8 does not align to frame size (u32CBL=%u cbFrameSize=%u)\n",
                                    uSD, u32CBL, pStreamR3->State.Mapping.cbFrameSize),
                                   VERR_INVALID_PARAMETER);

#ifndef VBOX_WITH_AUDIO_HDA_51_SURROUND
    if (Props.cChannels > 2)
    {
        /*
         * When not running with surround support enabled, override the audio channel count
         * with stereo (2) channels so that we at least can properly work with those.
         *
         * Note: This also involves dealing with surround setups the guest might has set up for us.
         */
        LogRelMax(32, ("HDA: Warning: More than stereo (2) channels are not supported (%RU8 requested), falling back to stereo channels for stream #%RU8\n",
                       Props.cChannels, uSD));
        Props.cChannels = 2;
        Props.cShift    = PDMAUDIOPCMPROPS_MAKE_SHIFT_PARMS(Props.cbSample, Props.cChannels);
    }
#endif

    /* Make sure the guest behaves regarding the stream's FIFO. */
    ASSERT_GUEST_LOGREL_MSG_STMT(u8FIFOW <= u8FIFOS,
                                 ("Guest tried setting a bigger FIFOW (%RU8) than FIFOS (%RU8), limiting\n", u8FIFOW, u8FIFOS),
                                 u8FIFOW = u8FIFOS /* ASSUMES that u8FIFOS has been validated. */);

    pStreamShared->u8SD       = uSD;

    /* Update all register copies so that we later know that something has changed. */
    pStreamShared->u64BDLBase = u64BDLBase;
    pStreamShared->u16LVI     = u16LVI;
    pStreamShared->u32CBL     = u32CBL;
    pStreamShared->u8FIFOS    = u8FIFOS;
    pStreamShared->u8FIFOW    = u8FIFOW;
    pStreamShared->u16FMT     = u16FMT;

    PPDMAUDIOSTREAMCFG pCfg = &pStreamShared->State.Cfg;
    pCfg->Props = Props;

    /* Set the stream's direction. */
    pCfg->enmDir = hdaGetDirFromSD(uSD);

    /* The the stream's name, based on the direction. */
    switch (pCfg->enmDir)
    {
        case PDMAUDIODIR_IN:
# ifdef VBOX_WITH_AUDIO_HDA_MIC_IN
#  error "Implement me!"
# else
            pCfg->u.enmSrc  = PDMAUDIORECSRC_LINE;
            pCfg->enmLayout = PDMAUDIOSTREAMLAYOUT_NON_INTERLEAVED;
            RTStrCopy(pCfg->szName, sizeof(pCfg->szName), "Line In");
# endif
            break;

        case PDMAUDIODIR_OUT:
            /* Destination(s) will be set in hdaAddStreamOut(),
             * based on the channels / stream layout. */
            break;

        default:
            AssertFailedReturn(VERR_NOT_SUPPORTED);
            break;
    }

    LogRel2(("HDA: Stream #%RU8 DMA @ 0x%x (%RU32 bytes = %RU64ms total)\n",
             uSD, pStreamShared->u64BDLBase, pStreamShared->u32CBL,
             DrvAudioHlpBytesToMilli(pStreamShared->u32CBL, &pStreamR3->State.Mapping.PCMProps)));

    /* Figure out how many transfer fragments we're going to use for this stream. */
    uint32_t cTransferFragments = pStreamShared->u16LVI + 1;
    if (cTransferFragments <= 1)
        LogRel(("HDA: Warning: Stream #%RU8 transfer fragments (%RU16) invalid -- buggy guest audio driver!\n",
                uSD, pStreamShared->u16LVI));

    /*
     * Handle the stream's position adjustment.
     */
    uint32_t cfPosAdjust = 0;

    LogFunc(("[SD%RU8] fPosAdjustEnabled=%RTbool, cPosAdjustFrames=%RU16\n",
             uSD, pThis->fPosAdjustEnabled, pThis->cPosAdjustFrames));

    if (pThis->fPosAdjustEnabled) /* Is the position adjustment enabled at all? */
    {
        HDABDLE BDLE;
        RT_ZERO(BDLE);

        int rc2 = hdaR3BDLEFetch(pDevIns, &BDLE, pStreamShared->u64BDLBase, 0 /* Entry */);
        AssertRC(rc2);

        /* Note: Do *not* check if this BDLE aligns to the stream's frame size.
         *       It can happen that this isn't the case on some guests, e.g.
         *       on Windows with a 5.1 speaker setup.
         *
         *       The only thing which counts is that the stream's CBL value
         *       properly aligns to the stream's frame size.
         */

        /* If no custom set position adjustment is set, apply some
         * simple heuristics to detect the appropriate position adjustment. */
        if (   !pThis->cPosAdjustFrames
        /* Position adjustmenet buffer *must* have the IOC bit set! */
            && hdaR3BDLENeedsInterrupt(&BDLE))
        {
            /** @todo Implement / use a (dynamic) table once this gets more complicated. */
#ifdef VBOX_WITH_INTEL_HDA
            /* Intel ICH / PCH: 1 frame. */
            if (BDLE.Desc.u32BufSize == (uint32_t)(1 * pStreamR3->State.Mapping.cbFrameSize))
            {
                cfPosAdjust = 1;
            }
            /* Intel Baytrail / Braswell: 32 frames. */
            else if (BDLE.Desc.u32BufSize == (uint32_t)(32 * pStreamR3->State.Mapping.cbFrameSize))
            {
                cfPosAdjust = 32;
            }
#endif
        }
        else /* Go with the set default. */
            cfPosAdjust = pThis->cPosAdjustFrames;

        if (cfPosAdjust)
        {
            /* Also adjust the number of fragments, as the position adjustment buffer
             * does not count as an own fragment as such.
             *
             * This e.g. can happen on (newer) Ubuntu guests which use
             * 4 (IOC) + 4408 (IOC) + 4408 (IOC) + 4408 (IOC) + 4404 (= 17632) bytes,
             * where the first buffer (4) is used as position adjustment.
             *
             * Only skip a fragment if the whole buffer fragment is used for
             * position adjustment.
             */
            if ((cfPosAdjust * pStreamR3->State.Mapping.cbFrameSize) == BDLE.Desc.u32BufSize)
                cTransferFragments--;

            /* Initialize position adjustment counter. */
            pStreamShared->State.cfPosAdjustDefault = cfPosAdjust;
            pStreamShared->State.cfPosAdjustLeft    = cfPosAdjust;
            LogRel2(("HDA: Position adjustment for stream #%RU8 active (%RU32 frames)\n", uSD, cfPosAdjust));
        }
    }

    Log3Func(("[SD%RU8] cfPosAdjust=%RU32, cFragments=%RU32\n", uSD, cfPosAdjust, cTransferFragments));

    /*
     * Set up data transfer stuff.
     */

    /* Assign the global device rate to the stream I/O timer as default. */
    pStreamShared->State.uTimerIoHz = pThis->uTimerHz;

    /*
     * Determine the transfer Hz the guest OS expects data transfer at.
     *
     * Guests also expect a very extact DMA timing for reading / writing audio data, so we run on a constant
     * (virtual) rate which we expose to the guest.
     *
     * Data rate examples:
     *   * Windows 10 @ 44,1kHz / 16-bit
     *      2 channels (stereo):
     *          * Default mode: 448 audio frames -> ~10.15ms = 1792 byte every ~10ms.
     *          * Fast mode:    128 audio frames -> ~ 2.90ms =  512 byte every ~3ms.
     *      6 channels (5.1 surround):
     *          * Default mode: Same values as above!
     */

    /* Audio data per second the stream needs. */
    const uint32_t cbDataPerSec = DrvAudioHlpMilliToBytes(RT_MS_1SEC, &pStreamR3->State.Mapping.PCMProps);

    /* This is used to indicate whether we're done or should the uTimerIoHz as fallback. */
    rc = VINF_SUCCESS;

    /* The transfer Hz depend on the heuristics above, that is,
       how often the guest expects to see a new data transfer. */
    ASSERT_GUEST_LOGREL_MSG_STMT(pStreamShared->State.uTimerIoHz,
                                 ("I/O timer Hz rate for stream #%RU8 is invalid\n", uSD),
                                 pStreamShared->State.uTimerIoHz = HDA_TIMER_HZ_DEFAULT);
    unsigned uTransferHz = pStreamShared->State.uTimerIoHz;

    LogRel2(("HDA: Stream #%RU8 needs %RU32 bytes/s audio data\n", uSD, cbDataPerSec));

    if (pThis->fTransferHeuristicsEnabled) /* Are data transfer heuristics enabled? */
    {
        /* Don't take frames (as bytes) into account which are part of the position adjustment. */
        uint32_t cbTransferHeuristicsPosAdjust = pStreamShared->State.cfPosAdjustDefault * pStreamR3->State.Mapping.cbFrameSize;
        uint32_t cbTransferHeuristics          = 0;
        uint32_t cbTransferHeuristicsCur       = 0;
        uint32_t cBufferIrqs                   = 0;
        for (uint32_t i = 0; i < cTransferFragments; i++)
        {
            /** @todo wrong read type!   */
            HDABDLEDESC bd = { 0, 0, 0 };
            PDMDevHlpPhysRead(pDevIns, u64BDLBase + i * sizeof(HDABDLEDESC), &bd, sizeof(bd));

            LogRel2(("HDA: Stream #%RU8 BDLE%03u: %#RX64 LB %#x %s (%#x)\n", uSD, i,
                     bd.u64BufAddr, bd.u32BufSize, bd.fFlags & HDA_BDLE_F_IOC ? " IOC=1" : "", bd.fFlags));

            /* Position adjustment (still) needed / active? */
            if (cbTransferHeuristicsPosAdjust)
            {
                const uint32_t cbTransferHeuristicsPosAdjustMin = RT_MIN(cbTransferHeuristicsPosAdjust, bd.u32BufSize);

                bd.u32BufSize                 -= cbTransferHeuristicsPosAdjustMin;
                cbTransferHeuristicsPosAdjust -= cbTransferHeuristicsPosAdjustMin;
            }

            /* Anything left to process for the current BDLE after doing the position adjustment? */
            if (bd.u32BufSize == 0)
                continue;

            /* Is an interrupt expected for the current BDLE? */
            if (bd.fFlags & HDA_BDLE_F_IOC)
            {
                cbTransferHeuristicsCur += bd.u32BufSize;
                if (   cbTransferHeuristicsCur == cbTransferHeuristics
                    || !cbTransferHeuristics)
                    cbTransferHeuristics = cbTransferHeuristicsCur;
                else
                {
                    /** @todo r=bird: you need to find the smallest common denominator here, not
                     *        just the minimum.  Ignoring this for now as windows has two equal
                     *        sized buffers both with IOC set. */
                    LogRelMax(32, ("HDA: Uneven IRQ buffer config; i=%u cbCur=%#x cbMin=%#x.\n", i, cbTransferHeuristicsCur, cbTransferHeuristics));
                    cbTransferHeuristics = RT_MIN(cbTransferHeuristicsCur, cbTransferHeuristics);
                }
                cbTransferHeuristicsCur = 0;
                cBufferIrqs++;
            }
            else /* No interrupt expected -> add it to the former BDLE size. */
                cbTransferHeuristicsCur += bd.u32BufSize;
        }

        /*
         * If the guest doesn't use buffer IRQs or only has one, just split the total
         * buffer length in half and use that as timer heuristics.  That gives the
         * guest half a buffer to fill while we're processing the other half.
         */
        if (cBufferIrqs <= 1)
            cbTransferHeuristics = pStreamShared->u32CBL / 2;

        /* Paranoia (fall back on I/O timer Hz if this happens). */
        if (cbTransferHeuristics >= 8)
        {
            ASSERT_GUEST_LOGREL_MSG(DrvAudioHlpBytesIsAligned(cbTransferHeuristics, &pStreamR3->State.Mapping.PCMProps),
                                    ("We arrived at a misaligned transfer size for stream #%RU8: %#x (%u)\n",
                                     uSD, cbTransferHeuristics, cbTransferHeuristics));

            uint64_t const cTimerTicksPerSec = PDMDevHlpTimerGetFreq(pDevIns, pStreamShared->hTimer);
            uint64_t const cbTransferPerSec  = RT_MAX(pStreamR3->State.Mapping.PCMProps.uHz * pStreamR3->State.Mapping.cbFrameSize,
                                                      4096 /* zero div prevention: min is 6kHz, picked 4k in case I'm mistaken */);

            /* Make sure the period is 250ms (random value) or less, in case the guest
               want to play the whole "Der Ring des Nibelungen" cycle in one go.  Just
               halve the buffer till we get there. */
            while (cbTransferHeuristics > 1024 && cbTransferHeuristics > cbTransferPerSec / 4)
                cbTransferHeuristics = DrvAudioHlpBytesAlign(cbTransferHeuristics / 2, &pStreamR3->State.Mapping.PCMProps);

            /* Set the transfer size per timer callout. (No chunking, so same.) */
            pStreamShared->State.cbTransferSize  = cbTransferHeuristics;
            pStreamShared->State.cbTransferChunk = cbTransferHeuristics;
            ASSERT_GUEST_LOGREL_MSG(DrvAudioHlpBytesIsAligned(cbTransferHeuristics, &pStreamR3->State.Mapping.PCMProps),
                                    ("We arrived at a misaligned transfer size for stream #%RU8: %#x (%u)\n",
                                     uSD, cbTransferHeuristics, cbTransferHeuristics));

            /* Convert to timer ticks. */
            pStreamShared->State.cTicksPerByte = (cTimerTicksPerSec + cbTransferPerSec / 2) / cbTransferPerSec;
            AssertStmt(pStreamShared->State.cTicksPerByte, pStreamShared->State.cTicksPerByte = 4096);

            pStreamShared->State.cTransferTicks = (cTimerTicksPerSec * cbTransferHeuristics + cbTransferPerSec / 2)
                                                / cbTransferPerSec;

            /* Estimate timer HZ for the circular buffer setup. */
            uTransferHz = cbTransferPerSec * 1000 / cbTransferHeuristics;
            LogRel2(("HDA: Stream #%RU8 needs a data transfer at least every %RU64 ticks / %RU32 bytes / approx %u.%03u Hz\n",
                     uSD, pStreamShared->State.cTransferTicks, cbTransferHeuristics, uTransferHz / 1000, uTransferHz % 1000));
            uTransferHz /= 1000;

            /* Indicate that we're done with period calculation. */
            rc = VINF_ALREADY_INITIALIZED;
        }
    }

    if (uTransferHz > 400) /* Anything above 400 Hz looks fishy -- tell the user. */
        LogRelMax(32, ("HDA: Warning: Calculated transfer Hz rate for stream #%RU8 looks incorrect (%u), please re-run with audio debug mode and report a bug\n",
                       uSD, uTransferHz));

    /* Set I/O scheduling hint for the backends. */
    pCfg->Device.cMsSchedulingHint = RT_MS_1SEC / pStreamShared->State.uTimerIoHz;
    LogRel2(("HDA: Stream #%RU8 set scheduling hint for the backends to %RU32ms\n", uSD, pCfg->Device.cMsSchedulingHint));

    if (rc != VINF_ALREADY_INITIALIZED && RT_SUCCESS(rc))
    {
        /*
         * Transfer heuristics disabled or failed.
         */
        Assert(uTransferHz == pStreamShared->State.uTimerIoHz);
        LogRel2(("HDA: Stream #%RU8 transfer timer and I/O timer rate is %u Hz.\n", uSD, uTransferHz));

        /* Make sure that the chosen transfer Hz rate dividable by the stream's overall data rate. */
        ASSERT_GUEST_LOGREL_MSG_STMT(cbDataPerSec % uTransferHz == 0,
                                     ("Transfer data rate (%RU32 bytes/s) for stream #%RU8 does not fit to stream timing (%u Hz)\n",
                                      cbDataPerSec, uSD, uTransferHz),
                                     uTransferHz = HDA_TIMER_HZ_DEFAULT);

        pStreamShared->State.cbTransferSize = (pStreamR3->State.Mapping.PCMProps.uHz * pStreamR3->State.Mapping.cbFrameSize)
                                            / uTransferHz;
        ASSERT_GUEST_LOGREL_MSG_STMT(pStreamShared->State.cbTransferSize,
                                     ("Transfer size for stream #%RU8 is invalid\n", uSD), rc = VERR_INVALID_PARAMETER);
        if (RT_SUCCESS(rc))
        {
            /*
             * Calculate the bytes we need to transfer to / from the stream's DMA per iteration.
             * This is bound to the device's Hz rate and thus to the (virtual) timing the device expects.
             *
             * As we don't do chunked transfers the moment, the chunk size equals the overall transfer size.
             */
            pStreamShared->State.cbTransferChunk = pStreamShared->State.cbTransferSize;
            ASSERT_GUEST_LOGREL_MSG_STMT(pStreamShared->State.cbTransferChunk,
                                         ("Transfer chunk for stream #%RU8 is invalid\n", uSD),
                                         rc = VERR_INVALID_PARAMETER);
            if (RT_SUCCESS(rc))
            {
                /* Make sure that the transfer chunk does not exceed the overall transfer size. */
                AssertStmt(pStreamShared->State.cbTransferChunk <= pStreamShared->State.cbTransferSize,
                           pStreamShared->State.cbTransferChunk = pStreamShared->State.cbTransferSize);

                const uint64_t uTimerFreq  = PDMDevHlpTimerGetFreq(pDevIns, pStreamShared->hTimer);

                const double cTicksPerHz   = uTimerFreq  / uTransferHz;

                double       cTicksPerByte = cTicksPerHz / (double)pStreamShared->State.cbTransferChunk;
                if (uTransferHz < 100)
                    cTicksPerByte /= 100 / uTransferHz;
                else
                    cTicksPerByte *= uTransferHz / 100;
                Assert(cTicksPerByte);

#define HDA_ROUND_NEAREST(a_X) ((a_X) >= 0 ? (uint32_t)((a_X) + 0.5) : (uint32_t)((a_X) - 0.5))

                /* Calculate the timer ticks per byte for this stream. */
                pStreamShared->State.cTicksPerByte = HDA_ROUND_NEAREST(cTicksPerByte);
                Assert(pStreamShared->State.cTicksPerByte);

                const double cTransferTicks = pStreamShared->State.cbTransferChunk * cTicksPerByte;

                /* Calculate timer ticks per transfer. */
                pStreamShared->State.cTransferTicks = HDA_ROUND_NEAREST(cTransferTicks);
                Assert(pStreamShared->State.cTransferTicks);
#undef HDA_ROUND_NEAREST

                LogRel2(("HDA: Stream #%RU8 is using %uHz I/O timer (%RU64 virtual ticks / Hz), stream Hz=%RU32, cTicksPerByte=%RU64, cTransferTicks=%RU64 -> cbTransferChunk=%RU32 (%RU64ms), cbTransferSize=%RU32 (%RU64ms)\n",
                         uSD, pStreamShared->State.uTimerIoHz, (uint64_t)cTicksPerHz, pStreamR3->State.Mapping.PCMProps.uHz,
                         pStreamShared->State.cTicksPerByte, pStreamShared->State.cTransferTicks,
                         pStreamShared->State.cbTransferChunk, DrvAudioHlpBytesToMilli(pStreamShared->State.cbTransferChunk, &pStreamR3->State.Mapping.PCMProps),
                         pStreamShared->State.cbTransferSize,  DrvAudioHlpBytesToMilli(pStreamShared->State.cbTransferSize,  &pStreamR3->State.Mapping.PCMProps)));
            }
        }
    }

    if (RT_SUCCESS(rc))
    {
        /* Make sure to also update the stream's DMA counter (based on its current LPIB value). */
        hdaR3StreamSetPositionAbs(pStreamShared, pDevIns, pThis, HDA_STREAM_REG(pThis, LPIB, uSD));

#ifdef LOG_ENABLED
        hdaR3BDLEDumpAll(pDevIns, pThis, pStreamShared->u64BDLBase, pStreamShared->u16LVI + 1);
#endif

        /*
         * Set up internal ring buffer.
         */

        /* (Re-)Allocate the stream's internal DMA buffer,
         * based on the timing *and* PCM properties we just got above. */
        if (pStreamR3->State.pCircBuf)
        {
            RTCircBufDestroy(pStreamR3->State.pCircBuf);
            pStreamR3->State.pCircBuf = NULL;
        }
        pStreamR3->State.offWrite = 0;
        pStreamR3->State.offRead  = 0;

        /*
         * The default internal ring buffer size must be:
         *
         *      - Large enough for at least three periodic DMA transfers.
         *
         *        It is critically important that we don't experience underruns
         *        in the DMA OUT code, because it will cause the buffer processing
         *        to get skewed and possibly overlap with what the guest is updating.
         *        At the time of writing (2021-03-05) there is no code for getting
         *        back into sync there.
         *
         *      - Large enough for at least three I/O scheduling hints.
         *
         *        We want to lag behind a DMA period or two, but there must be
         *        sufficent space for the AIO thread to get schedule and shuffle
         *        data thru the mixer and onto the host audio hardware.
         *
         *      - Both above with plenty to spare.
         *
         * So, just take the longest of the two periods and multipling it by 6.
         * We aren't not talking about very large base buffers heres, so size isn't
         * an issue.
         *
         * Note: Use pCfg->Props as PCM properties here, as we only want to store the
         *       samples we actually need, in other words, skipping the interleaved
         *       channels we don't support / need to save space.
         */
        uint32_t cbCircBuf = DrvAudioHlpMilliToBytes(RT_MS_1SEC * 6 / RT_MIN(uTransferHz, pStreamShared->State.uTimerIoHz),
                                                     &pCfg->Props);
        LogRel2(("HDA: Stream #%RU8 default ring buffer size is %RU32 bytes / %RU64 ms\n",
                 uSD, cbCircBuf, DrvAudioHlpBytesToMilli(cbCircBuf, &pCfg->Props)));

        uint32_t msCircBufCfg = hdaGetDirFromSD(uSD) == PDMAUDIODIR_IN ? pThis->cbCircBufInMs : pThis->cbCircBufOutMs;
        if (msCircBufCfg) /* Anything set via CFGM? */
        {
            cbCircBuf = DrvAudioHlpMilliToBytes(msCircBufCfg, &pCfg->Props);
            LogRel2(("HDA: Stream #%RU8 is using a custom ring buffer size of %RU32 bytes / %RU64 ms\n",
                     uSD, cbCircBuf, DrvAudioHlpBytesToMilli(cbCircBuf, &pCfg->Props)));
        }

        /* Serious paranoia: */
        ASSERT_GUEST_LOGREL_MSG_STMT(cbCircBuf % (pCfg->Props.cbSample * pCfg->Props.cChannels) == 0,
                                     ("Ring buffer size (%RU32) for stream #%RU8 not aligned to the (host) frame size (%RU8)\n",
                                      cbCircBuf, uSD, pCfg->Props.cbSample * pCfg->Props.cChannels),
                                     rc = VERR_INVALID_PARAMETER);
        ASSERT_GUEST_LOGREL_MSG_STMT(cbCircBuf, ("Ring buffer size for stream #%RU8 is invalid\n", uSD),
                                     rc = VERR_INVALID_PARAMETER);
        if (RT_SUCCESS(rc))
        {
            rc = RTCircBufCreate(&pStreamR3->State.pCircBuf, cbCircBuf);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Forward the timer frequency hint to TM as well for better accuracy on
                 * systems w/o preemption timers (also good for 'info timers').
                 */
                PDMDevHlpTimerSetFrequencyHint(pDevIns, pStreamShared->hTimer, uTransferHz);
            }
        }
    }

    if (RT_FAILURE(rc))
        LogRelMax(32, ("HDA: Initializing stream #%RU8 failed with %Rrc\n", uSD, rc));

#ifdef VBOX_WITH_DTRACE
    VBOXDD_HDA_STREAM_SETUP((uint32_t)uSD, rc, pStreamShared->State.Cfg.Props.uHz,
                            pStreamShared->State.cTransferTicks, pStreamShared->State.cbTransferSize);
#endif
    return rc;
}

/**
 * Resets an HDA stream.
 *
 * @param   pThis               The shared HDA device state.
 * @param   pThisCC             The ring-3 HDA device state.
 * @param   pStreamShared       HDA stream to reset (shared).
 * @param   pStreamR3           HDA stream to reset (ring-3).
 * @param   uSD                 Stream descriptor (SD) number to use for this stream.
 */
void hdaR3StreamReset(PHDASTATE pThis, PHDASTATER3 pThisCC, PHDASTREAM pStreamShared, PHDASTREAMR3 pStreamR3, uint8_t uSD)
{
    AssertPtr(pThis);
    AssertPtr(pStreamShared);
    AssertPtr(pStreamR3);
    Assert(uSD < HDA_MAX_STREAMS);
    AssertMsg(!pStreamShared->State.fRunning, ("[SD%RU8] Cannot reset stream while in running state\n", uSD));

    LogFunc(("[SD%RU8] Reset\n", uSD));

    /*
     * Set reset state.
     */
    Assert(ASMAtomicReadBool(&pStreamShared->State.fInReset) == false); /* No nested calls. */
    ASMAtomicXchgBool(&pStreamShared->State.fInReset, true);

    /*
     * Second, initialize the registers.
     */
    /* See 6.2.33: Clear on reset. */
    HDA_STREAM_REG(pThis, STS,   uSD) = 0;
    /* According to the ICH6 datasheet, 0x40000 is the default value for stream descriptor register 23:20
     * bits are reserved for stream number 18.2.33, resets SDnCTL except SRST bit. */
    HDA_STREAM_REG(pThis, CTL,   uSD) = 0x40000 | (HDA_STREAM_REG(pThis, CTL, uSD) & HDA_SDCTL_SRST);
    /* ICH6 defines default values (120 bytes for input and 192 bytes for output descriptors) of FIFO size. 18.2.39. */
    HDA_STREAM_REG(pThis, FIFOS, uSD) = hdaGetDirFromSD(uSD) == PDMAUDIODIR_IN ? HDA_SDIFIFO_120B : HDA_SDOFIFO_192B;
    /* See 18.2.38: Always defaults to 0x4 (32 bytes). */
    HDA_STREAM_REG(pThis, FIFOW, uSD) = HDA_SDFIFOW_32B;
    HDA_STREAM_REG(pThis, LPIB,  uSD) = 0;
    HDA_STREAM_REG(pThis, CBL,   uSD) = 0;
    HDA_STREAM_REG(pThis, LVI,   uSD) = 0;
    HDA_STREAM_REG(pThis, FMT,   uSD) = 0;
    HDA_STREAM_REG(pThis, BDPU,  uSD) = 0;
    HDA_STREAM_REG(pThis, BDPL,  uSD) = 0;

#ifdef HDA_USE_DMA_ACCESS_HANDLER
    hdaR3StreamUnregisterDMAHandlers(pThis, pStream);
#endif

    /* Assign the default mixer sink to the stream. */
    pStreamR3->pMixSink = hdaR3GetDefaultSink(pThisCC, uSD);

    /* Reset position adjustment counter. */
    pStreamShared->State.cfPosAdjustLeft = pStreamShared->State.cfPosAdjustDefault;

    /* Reset transfer stuff. */
    pStreamShared->State.cTransferPendingInterrupts = 0;
    pStreamShared->State.tsTransferLast = 0;
    pStreamShared->State.tsTransferNext = 0;

    /* Initialize timestamps. */
    pStreamShared->State.tsLastTransferNs = 0;
    pStreamShared->State.tsLastReadNs     = 0;

    RT_ZERO(pStreamShared->State.BDLE);
    pStreamShared->State.uCurBDLE = 0;

    if (pStreamR3->State.pCircBuf)
        RTCircBufReset(pStreamR3->State.pCircBuf);
    pStreamR3->State.offWrite = 0;
    pStreamR3->State.offRead  = 0;

    /* Reset the stream's period. */
    hdaR3StreamPeriodReset(&pStreamShared->State.Period);

#ifdef DEBUG
    pStreamR3->Dbg.cReadsTotal      = 0;
    pStreamR3->Dbg.cbReadTotal      = 0;
    pStreamR3->Dbg.tsLastReadNs     = 0;
    pStreamR3->Dbg.cWritesTotal     = 0;
    pStreamR3->Dbg.cbWrittenTotal   = 0;
    pStreamR3->Dbg.cWritesHz        = 0;
    pStreamR3->Dbg.cbWrittenHz      = 0;
    pStreamR3->Dbg.tsWriteSlotBegin = 0;
#endif

    /* Report that we're done resetting this stream. */
    HDA_STREAM_REG(pThis, CTL,   uSD) = 0;

#ifdef VBOX_WITH_DTRACE
    VBOXDD_HDA_STREAM_RESET((uint32_t)uSD);
#endif
    LogFunc(("[SD%RU8] Reset\n", uSD));

    /* Exit reset mode. */
    ASMAtomicXchgBool(&pStreamShared->State.fInReset, false);
}

/**
 * Enables or disables an HDA audio stream.
 *
 * @returns IPRT status code.
 * @param   pStreamShared       HDA stream to enable or disable - shared bits.
 * @param   pStreamR3           HDA stream to enable or disable - ring-3 bits.
 * @param   fEnable             Whether to enable or disble the stream.
 */
int hdaR3StreamEnable(PHDASTREAM pStreamShared, PHDASTREAMR3 pStreamR3, bool fEnable)
{
    AssertPtr(pStreamR3);
    AssertPtr(pStreamShared);

    LogFunc(("[SD%RU8] fEnable=%RTbool, pMixSink=%p\n", pStreamShared->u8SD, fEnable, pStreamR3->pMixSink));

    int rc = VINF_SUCCESS;

    AUDMIXSINKCMD enmCmd = fEnable
                         ? AUDMIXSINKCMD_ENABLE : AUDMIXSINKCMD_DISABLE;

    /* First, enable or disable the stream and the stream's sink, if any. */
    if (   pStreamR3->pMixSink
        && pStreamR3->pMixSink->pMixSink)
        rc = AudioMixerSinkCtl(pStreamR3->pMixSink->pMixSink, enmCmd);

    if (   RT_SUCCESS(rc)
        && fEnable
        && pStreamR3->Dbg.Runtime.fEnabled)
    {
        Assert(DrvAudioHlpPCMPropsAreValid(&pStreamShared->State.Cfg.Props));

        if (fEnable)
        {
            if (!DrvAudioHlpFileIsOpen(pStreamR3->Dbg.Runtime.pFileStream))
            {
                int rc2 = DrvAudioHlpFileOpen(pStreamR3->Dbg.Runtime.pFileStream, PDMAUDIOFILE_DEFAULT_OPEN_FLAGS,
                                              &pStreamShared->State.Cfg.Props);
                AssertRC(rc2);
            }

            if (!DrvAudioHlpFileIsOpen(pStreamR3->Dbg.Runtime.pFileDMARaw))
            {
                int rc2 = DrvAudioHlpFileOpen(pStreamR3->Dbg.Runtime.pFileDMARaw, PDMAUDIOFILE_DEFAULT_OPEN_FLAGS,
                                              &pStreamShared->State.Cfg.Props);
                AssertRC(rc2);
            }

            if (!DrvAudioHlpFileIsOpen(pStreamR3->Dbg.Runtime.pFileDMAMapped))
            {
                int rc2 = DrvAudioHlpFileOpen(pStreamR3->Dbg.Runtime.pFileDMAMapped, PDMAUDIOFILE_DEFAULT_OPEN_FLAGS,
                                              &pStreamShared->State.Cfg.Props);
                AssertRC(rc2);
            }
        }
    }

    if (RT_SUCCESS(rc))
    {
        pStreamShared->State.fRunning = fEnable;
    }

    LogFunc(("[SD%RU8] rc=%Rrc\n", pStreamShared->u8SD, rc));
    return rc;
}

#if 0 /* Not used atm. */
static uint32_t hdaR3StreamGetPosition(PHDASTATE pThis, PHDASTREAM pStreamShared)
{
    return HDA_STREAM_REG(pThis, LPIB, pStreamShared->u8SD);
}
#endif

/**
 * Updates an HDA stream's current read or write buffer position (depending on the stream type) by
 * setting its associated LPIB register and DMA position buffer (if enabled) to an absolute value.
 *
 * @param   pStreamShared       HDA stream to update read / write position for (shared).
 * @param   pDevIns             The device instance.
 * @param   pThis               The shared HDA device state.
 * @param   uLPIB               Absolute position (in bytes) to set current read / write position to.
 */
static void hdaR3StreamSetPositionAbs(PHDASTREAM pStreamShared, PPDMDEVINS pDevIns, PHDASTATE pThis, uint32_t uLPIB)
{
    AssertPtrReturnVoid(pStreamShared);
    AssertReturnVoid   (uLPIB <= pStreamShared->u32CBL); /* Make sure that we don't go out-of-bounds. */

    Log3Func(("[SD%RU8] LPIB=%RU32 (DMA Position Buffer Enabled: %RTbool)\n",  pStreamShared->u8SD, uLPIB, pThis->fDMAPosition));

    /* Update LPIB in any case. */
    HDA_STREAM_REG(pThis, LPIB, pStreamShared->u8SD) = uLPIB;

    /* Do we need to tell the current DMA position? */
    if (pThis->fDMAPosition)
    {
        int rc2 = PDMDevHlpPCIPhysWrite(pDevIns,
                                        pThis->u64DPBase + (pStreamShared->u8SD * 2 * sizeof(uint32_t)),
                                        (void *)&uLPIB, sizeof(uint32_t));
        AssertRC(rc2);
    }
}

/**
 * Updates an HDA stream's current read or write buffer position (depending on the stream type) by
 * adding a value to its associated LPIB register and DMA position buffer (if enabled).
 *
 * @note    Handles automatic CBL wrap-around.
 *
 * @param   pStreamShared       HDA stream to update read / write position for (shared).
 * @param   pDevIns             The device instance.
 * @param   pThis               The shared HDA device state.
 * @param   uToAdd              Position (in bytes) to add to the current read / write position.
 */
void hdaR3StreamSetPositionAdd(PHDASTREAM pStreamShared, PPDMDEVINS pDevIns, PHDASTATE pThis, uint32_t uToAdd)
{
    if (!uToAdd) /* No need to update anything if 0. */
        return;

    hdaR3StreamSetPositionAbs(pStreamShared, pDevIns, pThis,
                              (HDA_STREAM_REG(pThis, LPIB, pStreamShared->u8SD) + uToAdd) % pStreamShared->u32CBL);
}

/**
 * Retrieves the available size of (buffered) audio data (in bytes) of a given HDA stream.
 *
 * @returns Available data (in bytes).
 * @param   pStreamR3           HDA stream to retrieve size for (ring-3).
 */
static uint32_t hdaR3StreamGetUsed(PHDASTREAMR3 pStreamR3)
{
    AssertPtrReturn(pStreamR3, 0);

    if (pStreamR3->State.pCircBuf)
        return (uint32_t)RTCircBufUsed(pStreamR3->State.pCircBuf);
    return 0;
}

/**
 * Retrieves the free size of audio data (in bytes) of a given HDA stream.
 *
 * @returns Free data (in bytes).
 * @param   pStreamR3           HDA stream to retrieve size for (ring-3).
 */
static uint32_t hdaR3StreamGetFree(PHDASTREAMR3 pStreamR3)
{
    AssertPtrReturn(pStreamR3, 0);

    if (pStreamR3->State.pCircBuf)
        return (uint32_t)RTCircBufFree(pStreamR3->State.pCircBuf);
    return 0;
}

/**
 * Returns whether a next transfer for a given stream is scheduled or not.
 *
 * This takes pending stream interrupts into account as well as the next scheduled
 * transfer timestamp.
 *
 * @returns True if a next transfer is scheduled, false if not.
 * @param   pStreamShared       HDA stream to retrieve schedule status for (shared).
 * @param   tsNow               The current time.
 */
bool hdaR3StreamTransferIsScheduled(PHDASTREAM pStreamShared, uint64_t tsNow)
{
    if (pStreamShared)
    {
        if (pStreamShared->State.fRunning)
        {
            if (pStreamShared->State.cTransferPendingInterrupts)
            {
                Log3Func(("[SD%RU8] Scheduled (%RU8 IRQs pending)\n", pStreamShared->u8SD, pStreamShared->State.cTransferPendingInterrupts));
                return true;
            }

            if (pStreamShared->State.tsTransferNext > tsNow)
            {
                Log3Func(("[SD%RU8] Scheduled in %RU64\n", pStreamShared->u8SD, pStreamShared->State.tsTransferNext - tsNow));
                return true;
            }
        }
    }
    return false;
}

/**
 * Returns the (virtual) clock timestamp of the next transfer, if any.
 * Will return 0 if no new transfer is scheduled.
 *
 * @returns The (virtual) clock timestamp of the next transfer.
 * @param   pStreamShared       HDA stream to retrieve timestamp for (shared).
 */
uint64_t hdaR3StreamTransferGetNext(PHDASTREAM pStreamShared)
{
    return pStreamShared->State.tsTransferNext;
}

/**
 * Writes audio data from a mixer sink into an HDA stream's DMA buffer.
 *
 * @returns IPRT status code.
 * @param   pStreamR3           HDA stream to write to (ring-3).
 * @param   pvBuf               Data buffer to write.
 *                              If NULL, silence will be written.
 * @param   cbBuf               Number of bytes of data buffer to write.
 * @param   pcbWritten          Number of bytes written. Optional.
 */
static int hdaR3StreamWrite(PHDASTREAMR3 pStreamR3, const void *pvBuf, uint32_t cbBuf, uint32_t *pcbWritten)
{
    Assert(cbBuf);

    PRTCIRCBUF pCircBuf = pStreamR3->State.pCircBuf;
    AssertPtr(pCircBuf);

    uint32_t cbWrittenTotal = 0;
    uint32_t cbLeft         = RT_MIN(cbBuf, (uint32_t)RTCircBufFree(pCircBuf));

    while (cbLeft)
    {
        void *pvDst;
        size_t cbDst;
        RTCircBufAcquireWriteBlock(pCircBuf, cbLeft, &pvDst, &cbDst);

        if (cbDst)
        {
            if (pvBuf)
                memcpy(pvDst, (uint8_t *)pvBuf + cbWrittenTotal, cbDst);
            else /* Send silence. */
            {
                /** @todo Use a sample spec for "silence" based on the PCM parameters.
                 *        For now we ASSUME that silence equals NULLing the data. */
                RT_BZERO(pvDst, cbDst);
            }
#ifdef VBOX_WITH_DTRACE
            VBOXDD_HDA_STREAM_AIO_IN((uint32_t)pStreamR3->u8SD, (uint32_t)cbDst, pStreamR3->State.offWrite);
#endif
            pStreamR3->State.offWrite += cbDst;

            if (RT_LIKELY(!pStreamR3->Dbg.Runtime.fEnabled))
            { /* likely */ }
            else
                DrvAudioHlpFileWrite(pStreamR3->Dbg.Runtime.pFileStream, pvDst, cbDst, 0 /* fFlags */);
        }

        RTCircBufReleaseWriteBlock(pCircBuf, cbDst);

        Assert(cbLeft  >= (uint32_t)cbDst);
        cbLeft         -= (uint32_t)cbDst;
        cbWrittenTotal += (uint32_t)cbDst;
    }

    Log3Func(("cbWrittenTotal=%#RX32 @ %#RX64\n", cbWrittenTotal, pStreamR3->State.offWrite - cbWrittenTotal));

    if (pcbWritten)
        *pcbWritten = cbWrittenTotal;

    return VINF_SUCCESS;
}


/**
 * Reads audio data from an HDA stream's DMA buffer and writes into a specified mixer sink.
 *
 * @returns IPRT status code.
 * @param   pStreamR3           HDA stream to read audio data from (ring-3).
 * @param   cbToRead            Number of bytes to read.
 * @param   pcbRead             Number of bytes read. Optional.
 */
static int hdaR3StreamRead(PHDASTREAMR3 pStreamR3, uint32_t cbToRead, uint32_t *pcbRead)
{
    Assert(cbToRead);

    PHDAMIXERSINK pSink = pStreamR3->pMixSink;
    AssertMsgReturnStmt(pSink, ("[SD%RU8] Can't read from a stream with no sink attached\n", pStreamR3->u8SD),
                        if (pcbRead) *pcbRead = 0,
                        VINF_SUCCESS);

    PRTCIRCBUF pCircBuf = pStreamR3->State.pCircBuf;
    AssertPtr(pCircBuf);

    int rc = VINF_SUCCESS;

    uint32_t cbReadTotal = 0;
    uint32_t cbLeft      = RT_MIN(cbToRead, (uint32_t)RTCircBufUsed(pCircBuf));

    while (cbLeft)
    {
        void *pvSrc;
        size_t cbSrc;

        uint32_t cbWritten = 0;

        RTCircBufAcquireReadBlock(pCircBuf, cbLeft, &pvSrc, &cbSrc);

        if (cbSrc)
        {
            if (pStreamR3->Dbg.Runtime.fEnabled)
                DrvAudioHlpFileWrite(pStreamR3->Dbg.Runtime.pFileStream, pvSrc, cbSrc, 0 /* fFlags */);

            rc = AudioMixerSinkWrite(pSink->pMixSink, AUDMIXOP_COPY, pvSrc, (uint32_t)cbSrc, &cbWritten);
            AssertRC(rc);
            Assert(cbSrc >= cbWritten);

            Log2Func(("[SD%RU8] %#RX32/%#zx bytes read @ %#RX64\n", pStreamR3->u8SD, cbWritten, cbSrc, pStreamR3->State.offRead));
#ifdef VBOX_WITH_DTRACE
            VBOXDD_HDA_STREAM_AIO_OUT(pStreamR3->u8SD, (uint32_t)cbSrc, pStreamR3->State.offRead);
#endif
            pStreamR3->State.offRead += cbSrc;
        }

        RTCircBufReleaseReadBlock(pCircBuf, cbWritten);

        if (   !cbWritten /* Nothing written? */
            || RT_FAILURE(rc))
            break;

        Assert(cbLeft  >= cbWritten);
        cbLeft         -= cbWritten;

        cbReadTotal    += cbWritten;
    }

    if (pcbRead)
        *pcbRead = cbReadTotal;

    return rc;
}

/**
 * Transfers data of an HDA stream according to its usage (input / output).
 *
 * For an SDO (output) stream this means reading DMA data from the device to
 * the HDA stream's internal FIFO buffer.
 *
 * For an SDI (input) stream this is reading audio data from the HDA stream's
 * internal FIFO buffer and writing it as DMA data to the device.
 *
 * @returns IPRT status code.
 * @param   pDevIns             The device instance.
 * @param   pThis               The shared HDA device state.
 * @param   pThisCC             The ring-3 HDA device state.
 * @param   pStreamShared       HDA stream to update (shared).
 * @param   pStreamR3           HDA stream to update (ring-3).
 * @param   cbToProcessMax      How much data (in bytes) to process as maximum.
 */
static int hdaR3StreamTransfer(PPDMDEVINS pDevIns, PHDASTATE pThis, PHDASTATER3 pThisCC, PHDASTREAM pStreamShared,
                               PHDASTREAMR3 pStreamR3, uint32_t cbToProcessMax)
{
    uint8_t const uSD = pStreamShared->u8SD;
    LogFlowFunc(("ENTER - #%u cbToProcessMax=%#x\n", uSD, cbToProcessMax));

    hdaStreamLock(pStreamShared);

    PHDASTREAMPERIOD pPeriod = &pStreamShared->State.Period;

    bool fProceed = true;

    /* Stream not running (anymore)? */
    if (!pStreamShared->State.fRunning)
    {
        Log3Func(("[SD%RU8] Not running, skipping transfer\n", uSD));
        fProceed = false;
    }

    else if (HDA_STREAM_REG(pThis, STS, uSD) & HDA_SDSTS_BCIS)
    {
        Log3Func(("[SD%RU8] BCIS bit set, skipping transfer\n", uSD));
#ifdef HDA_STRICT
        /* Timing emulation bug or guest is misbehaving -- let me know. */
        AssertMsgFailed(("BCIS bit for stream #%RU8 still set when it shouldn't\n", uSD));
#endif
        fProceed = false;
    }

    if (!fProceed)
    {
        hdaStreamUnlock(pStreamShared);
        return VINF_SUCCESS;
    }

    /* Update real-time timestamp. */
    const uint64_t tsNowNs = RTTimeNanoTS();
#ifdef LOG_ENABLED
    const uint64_t tsDeltaMs = (tsNowNs - pStreamShared->State.tsLastTransferNs) / RT_NS_1MS;
    Log3Func(("[SD%RU8] tsDeltaNs=%RU64ms\n", uSD, tsDeltaMs));
#endif
    pStreamShared->State.tsLastTransferNs = tsNowNs;

    const uint64_t tsNow = PDMDevHlpTimerGet(pDevIns, pStreamShared->hTimer);

    if (!pStreamShared->State.tsTransferLast)
        pStreamShared->State.tsTransferLast = tsNow;

    pStreamShared->State.tsTransferLast = tsNow;

    /* Register sanity checks. */
    Assert(uSD < HDA_MAX_STREAMS);
    Assert(pStreamShared->u64BDLBase);
    Assert(pStreamShared->u32CBL);
    Assert(pStreamShared->u8FIFOS);

    /* State sanity checks. */
    Assert(ASMAtomicReadBool(&pStreamShared->State.fInReset) == false);
    Assert(ASMAtomicReadBool(&pStreamShared->State.fRunning));

    /* Transfer sanity checks. */
    Assert(pStreamShared->State.cbTransferSize);
    Assert(pStreamShared->State.cbTransferChunk <= pStreamShared->State.cbTransferSize);

    int rc = VINF_SUCCESS;

    /* Fetch first / next BDL entry. */
    PHDABDLE pBDLE = &pStreamShared->State.BDLE;
    if (hdaR3BDLEIsComplete(pBDLE))
    {
        rc = hdaR3BDLEFetch(pDevIns, pBDLE, pStreamShared->u64BDLBase, pStreamShared->State.uCurBDLE);
        AssertRC(rc);
    }

    uint32_t cbToProcess = RT_MIN(pStreamShared->State.cbTransferSize, pStreamShared->State.cbTransferChunk);

    Assert(cbToProcess);                                                     /* Nothing to process when there should be data. Accounting bug? */

    /* More data to process than maximum allowed? */
#ifdef HDA_STRICT
    AssertStmt(cbToProcess <= cbToProcessMax, cbToProcess = cbToProcessMax);
#else
    if (cbToProcess > cbToProcessMax)
        cbToProcess = cbToProcessMax;
#endif

    uint32_t cbProcessed = 0;
    uint32_t cbLeft      = cbToProcess;

    /* Whether an interrupt has been sent (asserted) for this transfer period already or not.
     *
     * Note: Windows 10 relies on this, e.g. sending more than one interrupt per transfer period
     *       confuses the Windows' audio driver and will screw up the audio data. So only send
     *       one interrupt per transfer period.
     */
    bool fInterruptSent = false;

    /* Set the FIFORDY bit on the stream while doing the transfer. */
    HDA_STREAM_REG(pThis, STS, uSD) |= HDA_SDSTS_FIFORDY;

    while (cbLeft)
    {
        /* Limit the chunk to the stream's FIFO size and what's left to process. */
        uint32_t cbChunk = RT_MIN(cbLeft, pStreamShared->u8FIFOS);

        /* Limit the chunk to the remaining data of the current BDLE. */
        cbChunk = RT_MIN(cbChunk, pBDLE->Desc.u32BufSize - pBDLE->State.u32BufOff);

        /* If there are position adjustment frames left to be processed,
         * make sure that we process them first as a whole. */
        if (pStreamShared->State.cfPosAdjustLeft)
            cbChunk = RT_MIN(cbChunk, uint32_t(pStreamShared->State.cfPosAdjustLeft * pStreamR3->State.Mapping.cbFrameSize));

        if (!cbChunk)
            break;

        uint32_t   cbDMA    = 0;
        PRTCIRCBUF pCircBuf = pStreamR3->State.pCircBuf;
        uint8_t   *pabFIFO  = pStreamShared->abFIFO;

        if (hdaGetDirFromSD(uSD) == PDMAUDIODIR_IN) /* Input (SDI). */
        {
            STAM_PROFILE_START(&pThis->StatIn, a);

            uint32_t cbDMAWritten = 0;
            uint32_t cbDMAToWrite = cbChunk;

            /** @todo Do we need interleaving streams support here as well?
             *        Never saw anything else besides mono/stereo mics (yet). */
            while (cbDMAToWrite)
            {
                void *pvBuf; size_t cbBuf;
                RTCircBufAcquireReadBlock(pCircBuf, cbDMAToWrite, &pvBuf, &cbBuf);

                if (   !cbBuf
                    && !RTCircBufUsed(pCircBuf))
                    break;

                memcpy(pabFIFO + cbDMAWritten, pvBuf, cbBuf);
#ifdef VBOX_WITH_DTRACE
                VBOXDD_HDA_STREAM_DMA_IN((uint32_t)uSD, (uint32_t)cbBuf, pStreamR3->State.offRead);
#endif
                pStreamR3->State.offRead += cbBuf;

                RTCircBufReleaseReadBlock(pCircBuf, cbBuf);

                Assert(cbDMAToWrite >= cbBuf);
                cbDMAToWrite -= (uint32_t)cbBuf;
                cbDMAWritten += (uint32_t)cbBuf;
                Assert(cbDMAWritten <= cbChunk);
            }

            if (cbDMAToWrite)
            {
                LogRel2(("HDA: FIFO underflow for stream #%RU8 (%RU32 bytes outstanding)\n", uSD, cbDMAToWrite));

                Assert(cbChunk == cbDMAWritten + cbDMAToWrite);
                memset((uint8_t *)pabFIFO + cbDMAWritten, 0, cbDMAToWrite);
                cbDMAWritten = cbChunk;
            }

            rc = hdaR3DMAWrite(pDevIns, pThis, pStreamShared, pStreamR3, pabFIFO, cbDMAWritten, &cbDMA /* pcbWritten */);
            if (RT_FAILURE(rc))
                LogRel(("HDA: Writing to stream #%RU8 DMA failed with %Rrc\n", uSD, rc));

            STAM_PROFILE_STOP(&pThis->StatIn, a);
        }
        else if (hdaGetDirFromSD(uSD) == PDMAUDIODIR_OUT) /* Output (SDO). */
        {
            STAM_PROFILE_START(&pThis->StatOut, a);

            rc = hdaR3DMARead(pDevIns, pThis, pStreamShared, pStreamR3, pabFIFO, cbChunk, &cbDMA /* pcbRead */);
            if (RT_SUCCESS(rc))
            {
                const uint32_t cbFree = (uint32_t)RTCircBufFree(pCircBuf);

                /*
                 * Most guests don't use different stream frame sizes than
                 * the default one, so save a bit of CPU time and don't go into
                 * the frame extraction code below.
                 *
                 * Only macOS guests need the frame extraction branch below at the moment AFAIK.
                 */
                if (pStreamR3->State.Mapping.cbFrameSize == HDA_FRAME_SIZE_DEFAULT)
                {
                    uint32_t cbDMARead = 0;
                    uint32_t cbDMALeft = RT_MIN(cbDMA, cbFree);

                    while (cbDMALeft)
                    {
                        void *pvBuf; size_t cbBuf;
                        RTCircBufAcquireWriteBlock(pCircBuf, cbDMALeft, &pvBuf, &cbBuf);

                        if (cbBuf)
                        {
                            memcpy(pvBuf, pabFIFO + cbDMARead, cbBuf);
                            cbDMARead += (uint32_t)cbBuf;
                            cbDMALeft -= (uint32_t)cbBuf;
#ifdef VBOX_WITH_DTRACE
                            VBOXDD_HDA_STREAM_DMA_OUT((uint32_t)uSD, (uint32_t)cbBuf, pStreamR3->State.offWrite);
#endif
                            pStreamR3->State.offWrite += cbBuf;
                        }

                        RTCircBufReleaseWriteBlock(pCircBuf, cbBuf);
                    }
                }
                else
                {
                    /*
                     * The following code extracts the required audio stream (channel) data
                     * of non-interleaved *and* interleaved audio streams.
                     *
                     * We by default only support 2 channels with 16-bit samples (HDA_FRAME_SIZE),
                     * but an HDA audio stream can have interleaved audio data of multiple audio
                     * channels in such a single stream ("AA,AA,AA vs. AA,BB,AA,BB").
                     *
                     * So take this into account by just handling the first channel in such a stream ("A")
                     * and just discard the other channel's data.
                     *
                     * I know, the following code is horribly slow, but seems to work for now.
                     */
                    /** @todo Optimize channel data extraction! Use some SSE(3) / intrinsics? */
                    for (unsigned m = 0; m < pStreamR3->State.Mapping.cMappings; m++)
                    {
                        const uint32_t cbFrame  = pStreamR3->State.Mapping.cbFrameSize;

                        Assert(cbFree >= cbDMA);

                        PPDMAUDIOSTREAMMAP pMap = &pStreamR3->State.Mapping.paMappings[m];
                        AssertPtr(pMap);

                        Log3Func(("Mapping #%u: Start (cbDMA=%RU32, cbFrame=%RU32, offNext=%RU32)\n",
                                  m, cbDMA, cbFrame, pMap->offNext));


                        /* Skip the current DMA chunk if the chunk is smaller than what the current stream mapping needs to read
                         * the next associated frame (pointed to at pMap->cbOff).
                         *
                         * This can happen if the guest did not come up with enough data within a certain time period, especially
                         * when using multi-channel speaker (> 2 channels [stereo]) setups. */
                        if (pMap->offNext > cbChunk)
                        {
                            Log2Func(("Mapping #%u: Skipped (cbChunk=%RU32, cbMapOff=%RU32)\n", m, cbChunk, pMap->offNext));
                            continue;
                        }

                        uint8_t *pbSrcBuf = pabFIFO;
                        size_t cbSrcOff   = pMap->offNext;

                        for (unsigned i = 0; i < cbDMA / cbFrame; i++)
                        {
                            void *pvDstBuf; size_t cbDstBuf;
                            RTCircBufAcquireWriteBlock(pCircBuf, pMap->cbStep, &pvDstBuf, &cbDstBuf);

                            Assert(cbDstBuf >= pMap->cbStep);

                            if (cbDstBuf)
                            {
                                Log3Func(("Mapping #%u: Frame #%02u:    cbStep=%u, offFirst=%u, offNext=%u, cbDstBuf=%u, cbSrcOff=%u\n",
                                          m, i, pMap->cbStep, pMap->offFirst, pMap->offNext, cbDstBuf, cbSrcOff));

                                memcpy(pvDstBuf, pbSrcBuf + cbSrcOff, cbDstBuf);

#if 0 /* Too slow, even for release builds, so disabled it. */
                                if (pStreamR3->Dbg.Runtime.fEnabled)
                                    DrvAudioHlpFileWrite(pStreamR3->Dbg.Runtime.pFileDMAMapped, pvDstBuf, cbDstBuf,
                                                         0 /* fFlags */);
#endif
                                Assert(cbSrcOff <= cbDMA);
                                if (cbSrcOff + cbFrame + pMap->offFirst<= cbDMA)
                                    cbSrcOff += cbFrame + pMap->offFirst;

#ifdef VBOX_WITH_DTRACE
                                VBOXDD_HDA_STREAM_DMA_OUT((uint32_t)uSD, (uint32_t)cbDstBuf, pStreamR3->State.offWrite);
#endif
                                Log3Func(("Mapping #%u: Frame #%02u:    -> cbSrcOff=%zu\n", m, i, cbSrcOff));
                                pStreamR3->State.offWrite += cbDstBuf;
                            }

                            RTCircBufReleaseWriteBlock(pCircBuf, cbDstBuf);
                        }

                        Log3Func(("Mapping #%u: End cbSize=%u, cbDMA=%RU32, cbSrcOff=%zu\n",
                                  m, pMap->cbStep, cbDMA, cbSrcOff));

                        Assert(cbSrcOff <= cbDMA);

                        const uint32_t cbSrcLeft = cbDMA - (uint32_t)cbSrcOff;
                        if (cbSrcLeft)
                        {
                            Log3Func(("Mapping #%u: cbSrcLeft=%RU32\n", m, cbSrcLeft));

                            if (cbSrcLeft >= pMap->cbStep)
                            {
                                void *pvDstBuf; size_t cbDstBuf;
                                RTCircBufAcquireWriteBlock(pCircBuf, pMap->cbStep, &pvDstBuf, &cbDstBuf);

                                Assert(cbDstBuf >= pMap->cbStep);

                                if (cbDstBuf)
                                {
                                    memcpy(pvDstBuf, pbSrcBuf + cbSrcOff, cbDstBuf);
#ifdef VBOX_WITH_DTRACE
                                    VBOXDD_HDA_STREAM_DMA_OUT((uint32_t)uSD, (uint32_t)cbDstBuf, pStreamR3->State.offWrite);
#endif
                                    pStreamR3->State.offWrite += cbDstBuf;
                                }

                                RTCircBufReleaseWriteBlock(pCircBuf, cbDstBuf);
                            }

                            Assert(pMap->cbFrame >= cbSrcLeft);
                            pMap->offNext = pMap->cbFrame - cbSrcLeft;
                        }
                        else
                            pMap->offNext = 0;

                        Log3Func(("Mapping #%u finish (cbSrcOff=%zu, offNext=%zu)\n", m, cbSrcOff, pMap->offNext));
                    }
                }
            }
            else
                LogRel(("HDA: Reading from stream #%RU8 DMA failed with %Rrc\n", uSD, rc));

            STAM_PROFILE_STOP(&pThis->StatOut, a);
        }

        else /** @todo Handle duplex streams? */
            AssertFailed();

        if (cbDMA)
        {
            /* We always increment the position of DMA buffer counter because we're always reading
             * into an intermediate DMA buffer. */
            pBDLE->State.u32BufOff += (uint32_t)cbDMA;
            Assert(pBDLE->State.u32BufOff <= pBDLE->Desc.u32BufSize);

            /* Are we done doing the position adjustment?
             * Only then do the transfer accounting .*/
            if (pStreamShared->State.cfPosAdjustLeft == 0)
            {
                Assert(cbLeft >= cbDMA);
                cbLeft        -= cbDMA;

                cbProcessed   += cbDMA;
            }

            Log3Func(("[SD%RU8] cbDMA=%RU32 -> %R[bdle]\n", uSD, cbDMA, pBDLE));
        }

        if (hdaR3BDLEIsComplete(pBDLE))
        {
            Log3Func(("[SD%RU8] Completed %R[bdle]\n", uSD, pBDLE));

            /* Make sure to also update the wall clock when a BDLE is complete.
             * Needed for Windows 10 guests. */
            hdaR3WalClkSet(pThis, pThisCC,
                             hdaWalClkGetCurrent(pThis)
                           + hdaR3StreamPeriodFramesToWalClk(pPeriod,
                                                               pBDLE->Desc.u32BufSize
                                                             / pStreamR3->State.Mapping.cbFrameSize),
                           false /* fForce */);

            /*
             * Update the stream's current position.
             * Do this as accurate and close to the actual data transfer as possible.
             * All guetsts rely on this, depending on the mechanism they use (LPIB register or DMA counters).
             *
             * Note for Windows 10: The OS' driver is *very* picky about *when* the (DMA) positions get updated!
             *                      Not doing this at the right time will result in ugly sound crackles!
             */
            hdaR3StreamSetPositionAdd(pStreamShared, pDevIns, pThis, pBDLE->Desc.u32BufSize);

                /* Does the current BDLE require an interrupt to be sent? */
            if (   hdaR3BDLENeedsInterrupt(pBDLE)
                /* Are we done doing the position adjustment?
                 * It can happen that a BDLE which is handled while doing the
                 * position adjustment requires an interrupt on completion (IOC) being set.
                 *
                 * In such a case we need to skip such an interrupt and just move on. */
                && pStreamShared->State.cfPosAdjustLeft == 0)
            {
                /* If the IOCE ("Interrupt On Completion Enable") bit of the SDCTL register is set
                 * we need to generate an interrupt.
                 */
                if (HDA_STREAM_REG(pThis, CTL, uSD) & HDA_SDCTL_IOCE)
                {
                    /* Assert the interrupt before actually fetching the next BDLE below. */
                    if (!fInterruptSent)
                    {
                        pStreamShared->State.cTransferPendingInterrupts = 1;

                        AssertMsg(pStreamShared->State.cTransferPendingInterrupts <= 32,
                                  ("Too many pending interrupts (%RU8) for stream #%RU8\n",
                                   pStreamShared->State.cTransferPendingInterrupts, uSD));

                        Log3Func(("[SD%RU8] Scheduling interrupt (now %RU8 total)\n", uSD, pStreamShared->State.cTransferPendingInterrupts));

                        /*
                         * Set the stream's BCIS bit.
                         *
                         * Note: This only must be done if the whole period is complete, and not if only
                         * one specific BDL entry is complete (if it has the IOC bit set).
                         *
                         * This will otherwise confuses the guest when it 1) deasserts the interrupt,
                         * 2) reads SDSTS (with BCIS set) and then 3) too early reads a (wrong) WALCLK value.
                         *
                         * snd_hda_intel on Linux will tell.
                         */
                        HDA_STREAM_REG(pThis, STS, uSD) |= HDA_SDSTS_BCIS;

                        /* Trigger an interrupt first and let hdaRegWriteSDSTS() deal with
                         * ending / beginning a period. */
                        HDA_PROCESS_INTERRUPT(pDevIns, pThis);

                        fInterruptSent = true;
                    }
                }
            }

            if (pStreamShared->State.uCurBDLE == pStreamShared->u16LVI)
            {
                pStreamShared->State.uCurBDLE = 0;
            }
            else
                pStreamShared->State.uCurBDLE++;

            /* Fetch the next BDLE entry. */
            hdaR3BDLEFetch(pDevIns, pBDLE, pStreamShared->u64BDLBase, pStreamShared->State.uCurBDLE);
        }

        /* Do the position adjustment accounting. */
        pStreamShared->State.cfPosAdjustLeft -=
            RT_MIN(pStreamShared->State.cfPosAdjustLeft, cbDMA / pStreamR3->State.Mapping.cbFrameSize);

        if (RT_FAILURE(rc))
            break;
    }

    /* Remove the FIFORDY bit again. */
    HDA_STREAM_REG(pThis, STS, uSD) &= ~HDA_SDSTS_FIFORDY;

    /* Sanity. */
    Assert(cbProcessed == cbToProcess);
    Assert(cbLeft      == 0);

    /* Only do the data accounting if we don't have to do any position
     * adjustment anymore. */
    if (pStreamShared->State.cfPosAdjustLeft == 0)
    {
        hdaR3StreamPeriodInc(pPeriod, RT_MIN(cbProcessed / pStreamR3->State.Mapping.cbFrameSize,
                                             hdaR3StreamPeriodGetRemainingFrames(pPeriod)));
    }

    const bool fTransferComplete  = cbLeft == 0;
    if (fTransferComplete)
    {
        /*
         * Try updating the wall clock.
         *
         * Note 1) Only certain guests (like Linux' snd_hda_intel) rely on the WALCLK register
         *         in order to determine the correct timing of the sound device. Other guests
         *         like Windows 7 + 10 (or even more exotic ones like Haiku) will completely
         *         ignore this.
         *
         * Note 2) When updating the WALCLK register too often / early (or even in a non-monotonic
         *         fashion) this *will* upset guest device drivers and will completely fuck up the
         *         sound output. Running VLC on the guest will tell!
         */
        const bool fWalClkSet = hdaR3WalClkSet(pThis, pThisCC,
                                               RT_MIN(  hdaWalClkGetCurrent(pThis)
                                                      + hdaR3StreamPeriodFramesToWalClk(pPeriod,
                                                                                          cbProcessed
                                                                                        / pStreamR3->State.Mapping.cbFrameSize),
                                                      hdaR3WalClkGetMax(pThis, pThisCC)),
                                               false /* fForce */);
        RT_NOREF(fWalClkSet);
    }

    /* Set the next transfer timing slot.
     * This must happen at a constant rate. */
    pStreamShared->State.tsTransferNext = tsNow + pStreamShared->State.cTransferTicks;

    /* Always update this timestamp, no matter what pStreamShared->State.tsTransferNext is. */
    pStreamShared->State.tsTransferLast = tsNow;

    Log3Func(("[SD%RU8] %R[bdle] -- %#RX32/%#RX32 @ %#RX64\n", uSD, pBDLE, cbProcessed, pStreamShared->State.cbTransferSize,
              (hdaGetDirFromSD(uSD) == PDMAUDIODIR_OUT ? pStreamR3->State.offWrite : pStreamR3->State.offRead) - cbProcessed));
    Log3Func(("[SD%RU8] fTransferComplete=%RTbool, cTransferPendingInterrupts=%RU8\n",
              uSD, fTransferComplete, pStreamShared->State.cTransferPendingInterrupts));
    Log3Func(("[SD%RU8] tsNow=%RU64, tsTransferNext=%RU64 (in %RU64 ticks)\n",
              uSD, tsNow, pStreamShared->State.tsTransferNext,
              pStreamShared->State.tsTransferNext ? pStreamShared->State.tsTransferNext - tsNow : 0));

    LogFlowFuncLeave();

    hdaStreamUnlock(pStreamShared);

    return VINF_SUCCESS;
}

/**
 * The stream's main function when called by the timer.
 *
 * Note: This function also will be called without timer invocation
 *       when starting (enabling) the stream to minimize startup latency.
 *
 * @param   pDevIns         The device instance.
 * @param   pThis           The shared HDA device state.
 * @param   pThisCC         The ring-3 HDA device state.
 * @param   pStreamShared   HDA stream to update (shared bits).
 * @param   pStreamR3       HDA stream to update (ring-3 bits).
 */
void hdaR3StreamTimerMain(PPDMDEVINS pDevIns, PHDASTATE pThis, PHDASTATER3 pThisCC,
                          PHDASTREAM pStreamShared, PHDASTREAMR3 pStreamR3)
{
    Assert(PDMDevHlpCritSectIsOwner(pDevIns, &pThis->CritSect));
    Assert(PDMDevHlpTimerIsLockOwner(pDevIns, pStreamShared->hTimer));

    hdaR3StreamUpdate(pDevIns, pThis, pThisCC, pStreamShared, pStreamR3, true /* fInTimer */);

    /* Flag indicating whether to kick the timer again for a new data processing round. */
    bool fSinkActive = false;
    if (pStreamR3->pMixSink)
        fSinkActive = AudioMixerSinkIsActive(pStreamR3->pMixSink->pMixSink);

#ifdef LOG_ENABLED
    const uint8_t uSD = pStreamShared->u8SD;
#endif

    if (fSinkActive)
    {
        const uint64_t tsNow           = PDMDevHlpTimerGet(pDevIns, pStreamShared->hTimer); /* (For virtual sync this remains the same for the whole callout IIRC) */
        const bool     fTimerScheduled = hdaR3StreamTransferIsScheduled(pStreamShared, tsNow);

        uint64_t tsTransferNext  = 0;
        if (fTimerScheduled)
        {
            Assert(pStreamShared->State.tsTransferNext); /* Make sure that a new transfer timestamp is set. */
            tsTransferNext = pStreamShared->State.tsTransferNext;
        }
        else /* Schedule at the precalculated rate. */
            tsTransferNext = tsNow + pStreamShared->State.cTransferTicks;

        Log3Func(("[SD%RU8] fSinksActive=%RTbool, fTimerScheduled=%RTbool, tsTransferNext=%RU64 (in %RU64)\n",
                  uSD, fSinkActive, fTimerScheduled, tsTransferNext, tsTransferNext - tsNow));

        hdaR3TimerSet(pDevIns, pStreamShared, tsTransferNext,
                      true /*fForce*/, tsNow);
    }
    else
        Log3Func(("[SD%RU8] fSinksActive=%RTbool\n", uSD, fSinkActive));
}

/**
 * Updates a HDA stream by doing its required data transfers.
 *
 * The host sink(s) set the overall pace.
 *
 * This routine is called by both, the synchronous and the asynchronous
 * (VBOX_WITH_AUDIO_HDA_ASYNC_IO), implementations.
 *
 * When running synchronously, the device DMA transfers *and* the mixer sink
 * processing is within the device timer.
 *
 * When running asynchronously, only the device DMA transfers are done in the
 * device timer, whereas the mixer sink processing then is done in the stream's
 * own async I/O thread. This thread also will call this function
 * (with fInTimer set to @c false).
 *
 * @param   pDevIns         The device instance.
 * @param   pThis           The shared HDA device state.
 * @param   pThisCC         The ring-3 HDA device state.
 * @param   pStreamShared   HDA stream to update (shared bits).
 * @param   pStreamR3       HDA stream to update (ring-3 bits).
 * @param   fInTimer        Whether to this function was called from the timer
 *                          context or an asynchronous I/O stream thread (if supported).
 */
void hdaR3StreamUpdate(PPDMDEVINS pDevIns, PHDASTATE pThis, PHDASTATER3 pThisCC,
                       PHDASTREAM pStreamShared, PHDASTREAMR3 pStreamR3, bool fInTimer)
{
    if (!pStreamShared)
        return;

    PAUDMIXSINK pSink = NULL;
    if (pStreamR3->pMixSink)
        pSink = pStreamR3->pMixSink->pMixSink;

    if (!AudioMixerSinkIsActive(pSink)) /* No sink available? Bail out. */
        return;

    const uint64_t tsNowNs = RTTimeNanoTS();

    int rc2;

    if (hdaGetDirFromSD(pStreamShared->u8SD) == PDMAUDIODIR_OUT) /* Output (SDO). */
    {
        bool fDoRead = fInTimer; /* Whether to read from the HDA stream or not. */

        /*
         * Do DMA work.
         */
# ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
        if (fInTimer)
# endif
        {
            uint32_t cbStreamFree = hdaR3StreamGetFree(pStreamR3);
            if (cbStreamFree)
            { /* likely */ }
            else
            {
                LogRel2(("HDA: Warning: Hit stream #%RU8 overflow, dropping audio data\n", pStreamShared->u8SD));
# ifdef HDA_STRICT
                AssertMsgFailed(("Hit stream #%RU8 overflow -- timing bug?\n", pStreamShared->u8SD));
# endif
                /* When hitting an overflow, drop all remaining data to make space for current data.
                 * This is needed in order to keep the device emulation running at a constant rate,
                 * at the cost of losing valid (but too much) data. */
                RTCircBufReset(pStreamR3->State.pCircBuf);
                pStreamR3->State.offWrite = 0;
                pStreamR3->State.offRead  = 0;
                cbStreamFree = hdaR3StreamGetFree(pStreamR3);
            }

            /* Do the DMA transfer. */
            rc2 = hdaR3StreamTransfer(pDevIns, pThis, pThisCC, pStreamShared, pStreamR3, cbStreamFree);
            AssertRC(rc2);

            /* Never read yet? Set initial timestamp. */
            if (pStreamShared->State.tsLastReadNs == 0)
                pStreamShared->State.tsLastReadNs = tsNowNs;

            /*
             * Push data to down thru the mixer to and to the host drivers?
             *
             * This is generally done at the rate given by cMsSchedulingHint,
             * however we must also check available DMA buffer space.  There
             * should be at least two periodic transfer units worth of space
             * available now.
             */
            Assert(tsNowNs >= pStreamShared->State.tsLastReadNs);
            /** @todo convert cMsSchedulingHint to nano seconds and save a div.  */
            const uint64_t msDelta = (tsNowNs - pStreamShared->State.tsLastReadNs) / RT_NS_1MS;
            cbStreamFree = hdaR3StreamGetFree(pStreamR3);
            if (   cbStreamFree < pStreamShared->State.cbTransferSize * 2
                || msDelta >= pStreamShared->State.Cfg.Device.cMsSchedulingHint)
                fDoRead = true;

            Log3Func(("msDelta=%RU64 (vs %u) cbStreamFree=%#x (vs %#x) => fDoRead=%RTbool\n", msDelta,
                      pStreamShared->State.Cfg.Device.cMsSchedulingHint, cbStreamFree,
                      pStreamShared->State.cbTransferSize * 2, fDoRead));

            if (fDoRead)
            {
# ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
                /* Notify the async I/O worker thread that there's work to do. */
                Log5Func(("Notifying AIO thread\n"));
                rc2 = hdaR3StreamAsyncIONotify(pStreamR3);
                AssertRC(rc2);
# endif
                /* Update last read timestamp so that we know when to run next. */
                pStreamShared->State.tsLastReadNs = tsNowNs;
            }
        }

# ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
        if (!fInTimer) /* In async I/O thread */
# else
        if (fDoRead)
# endif
        {
            uint32_t const cbSinkWritable     = AudioMixerSinkGetWritable(pSink);
            uint32_t const cbStreamReadable   = hdaR3StreamGetUsed(pStreamR3);
            uint32_t       cbToReadFromStream = RT_MIN(cbStreamReadable, cbSinkWritable);
            /* Make sure that we always align the number of bytes when reading to the stream's PCM properties. */
            cbToReadFromStream = DrvAudioHlpBytesAlign(cbToReadFromStream, &pStreamR3->State.Mapping.PCMProps);

            Assert(tsNowNs >= pStreamShared->State.tsLastReadNs);
            Log3Func(("[SD%RU8] msDeltaLastRead=%RI64\n",
                      pStreamShared->u8SD, (tsNowNs - pStreamShared->State.tsLastReadNs) / RT_NS_1MS));
            Log3Func(("[SD%RU8] cbSinkWritable=%RU32, cbStreamReadable=%RU32 -> cbToReadFromStream=%RU32\n",
                      pStreamShared->u8SD, cbSinkWritable, cbStreamReadable, cbToReadFromStream));

            if (cbToReadFromStream)
            {
                /* Read (guest output) data and write it to the stream's sink. */
                rc2 = hdaR3StreamRead(pStreamR3, cbToReadFromStream, NULL /* pcbRead */);
                AssertRC(rc2);
            }

            /* When running synchronously, update the associated sink here.
             * Otherwise this will be done in the async I/O thread. */
            rc2 = AudioMixerSinkUpdate(pSink);
            AssertRC(rc2);
        }
    }
    else /* Input (SDI). */
    {
# ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
        if (!fInTimer)
# endif
        {
            rc2 = AudioMixerSinkUpdate(pSink);
            AssertRC(rc2);

            /* Is the sink ready to be read (host input data) from? If so, by how much? */
            uint32_t cbSinkReadable = AudioMixerSinkGetReadable(pSink);

            /* How much (guest input) data is available for writing at the moment for the HDA stream? */
            const uint32_t cbStreamFree = hdaR3StreamGetFree(pStreamR3);

            Log3Func(("[SD%RU8] cbSinkReadable=%RU32, cbStreamFree=%RU32\n", pStreamShared->u8SD, cbSinkReadable, cbStreamFree));

            /* Do not read more than the HDA stream can hold at the moment.
             * The host sets the overall pace. */
            if (cbSinkReadable > cbStreamFree)
                cbSinkReadable = cbStreamFree;

            if (cbSinkReadable)
            {
                void    *pvFIFO = &pStreamShared->abFIFO[0];
                uint32_t cbFIFO = (uint32_t)sizeof(pStreamShared->abFIFO);

                while (cbSinkReadable)
                {
                    uint32_t cbRead;
                    rc2 = AudioMixerSinkRead(pSink, AUDMIXOP_COPY,
                                             pvFIFO, RT_MIN(cbSinkReadable, cbFIFO), &cbRead);
                    AssertRCBreak(rc2);

                    if (!cbRead)
                    {
                        AssertMsgFailed(("Nothing read from sink, even if %RU32 bytes were (still) announced\n", cbSinkReadable));
                        break;
                    }

                    /* Write (guest input) data to the stream which was read from stream's sink before. */
                    uint32_t cbWritten;
                    rc2 = hdaR3StreamWrite(pStreamR3, pvFIFO, cbRead, &cbWritten);
                    AssertRCBreak(rc2);
                    AssertBreak(cbWritten > 0); /* Should never happen, as we know how much we can write. */

                    Assert(cbSinkReadable >= cbRead);
                    cbSinkReadable -= cbRead;
                }
            }
        }
# ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
        else /* fInTimer */
# endif
        {
# ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
            if (tsNowNs - pStreamShared->State.tsLastReadNs >= pStreamShared->State.Cfg.Device.cMsSchedulingHint * RT_NS_1MS)
            {
                Log5Func(("Notifying AIO thread\n"));
                rc2 = hdaR3StreamAsyncIONotify(pStreamR3);
                AssertRC(rc2);

                pStreamShared->State.tsLastReadNs = tsNowNs;
            }
# endif
            const uint32_t cbStreamUsed = hdaR3StreamGetUsed(pStreamR3);
            if (cbStreamUsed)
            {
                rc2 = hdaR3StreamTransfer(pDevIns, pThis, pThisCC, pStreamShared, pStreamR3, cbStreamUsed);
                AssertRC(rc2);
            }
        }
    }
}

#endif /* IN_RING3 */

/**
 * Locks an HDA stream for serialized access.
 *
 * @returns IPRT status code.
 * @param   pStreamShared       HDA stream to lock (shared bits).
 */
void hdaStreamLock(PHDASTREAM pStreamShared)
{
    AssertPtrReturnVoid(pStreamShared);
# ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
    int rc2 = PDMCritSectEnter(&pStreamShared->CritSect, VINF_SUCCESS);
    AssertRC(rc2);
#endif
}

/**
 * Unlocks a formerly locked HDA stream.
 *
 * @returns IPRT status code.
 * @param   pStreamShared       HDA stream to unlock (shared bits).
 */
void hdaStreamUnlock(PHDASTREAM pStreamShared)
{
    AssertPtrReturnVoid(pStreamShared);
# ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
    int rc2 = PDMCritSectLeave(&pStreamShared->CritSect);
    AssertRC(rc2);
# endif
}

#ifdef IN_RING3

#if 0 /* unused - no prototype even */
/**
 * Updates an HDA stream's current read or write buffer position (depending on the stream type) by
 * updating its associated LPIB register and DMA position buffer (if enabled).
 *
 * @returns Set LPIB value.
 * @param   pDevIns             The device instance.
 * @param   pStream             HDA stream to update read / write position for.
 * @param   u32LPIB             New LPIB (position) value to set.
 */
uint32_t hdaR3StreamUpdateLPIB(PPDMDEVINS pDevIns, PHDASTATE pThis, PHDASTREAM pStreamShared, uint32_t u32LPIB)
{
    AssertMsg(u32LPIB <= pStreamShared->u32CBL,
              ("[SD%RU8] New LPIB (%RU32) exceeds CBL (%RU32)\n", pStreamShared->u8SD, u32LPIB, pStreamShared->u32CBL));

    u32LPIB = RT_MIN(u32LPIB, pStreamShared->u32CBL);

    LogFlowFunc(("[SD%RU8] LPIB=%RU32 (DMA Position Buffer Enabled: %RTbool)\n",
                 pStreamShared->u8SD, u32LPIB, pThis->fDMAPosition));

    /* Update LPIB in any case. */
    HDA_STREAM_REG(pThis, LPIB, pStreamShared->u8SD) = u32LPIB;

    /* Do we need to tell the current DMA position? */
    if (pThis->fDMAPosition)
    {
        int rc2 = PDMDevHlpPCIPhysWrite(pDevIns,
                                        pThis->u64DPBase + (pStreamShared->u8SD * 2 * sizeof(uint32_t)),
                                        (void *)&u32LPIB, sizeof(uint32_t));
        AssertRC(rc2);
    }

    return u32LPIB;
}
#endif

# ifdef HDA_USE_DMA_ACCESS_HANDLER
/**
 * Registers access handlers for a stream's BDLE DMA accesses.
 *
 * @returns true if registration was successful, false if not.
 * @param   pStream             HDA stream to register BDLE access handlers for.
 */
bool hdaR3StreamRegisterDMAHandlers(PHDASTREAM pStream)
{
    /* At least LVI and the BDL base must be set. */
    if (   !pStreamShared->u16LVI
        || !pStreamShared->u64BDLBase)
    {
        return false;
    }

    hdaR3StreamUnregisterDMAHandlers(pStream);

    LogFunc(("Registering ...\n"));

    int rc = VINF_SUCCESS;

    /*
     * Create BDLE ranges.
     */

    struct BDLERANGE
    {
        RTGCPHYS uAddr;
        uint32_t uSize;
    } arrRanges[16]; /** @todo Use a define. */

    size_t cRanges = 0;

    for (uint16_t i = 0; i < pStreamShared->u16LVI + 1; i++)
    {
        HDABDLE BDLE;
        rc = hdaR3BDLEFetch(pDevIns, &BDLE, pStreamShared->u64BDLBase, i /* Index */);
        if (RT_FAILURE(rc))
            break;

        bool fAddRange = true;
        BDLERANGE *pRange;

        if (cRanges)
        {
            pRange = &arrRanges[cRanges - 1];

            /* Is the current range a direct neighbor of the current BLDE? */
            if ((pRange->uAddr + pRange->uSize) == BDLE.Desc.u64BufAddr)
            {
                /* Expand the current range by the current BDLE's size. */
                pRange->uSize += BDLE.Desc.u32BufSize;

                /* Adding a new range in this case is not needed anymore. */
                fAddRange = false;

                LogFunc(("Expanding range %zu by %RU32 (%RU32 total now)\n", cRanges - 1, BDLE.Desc.u32BufSize, pRange->uSize));
            }
        }

        /* Do we need to add a new range? */
        if (   fAddRange
            && cRanges < RT_ELEMENTS(arrRanges))
        {
            pRange = &arrRanges[cRanges];

            pRange->uAddr = BDLE.Desc.u64BufAddr;
            pRange->uSize = BDLE.Desc.u32BufSize;

            LogFunc(("Adding range %zu - 0x%x (%RU32)\n", cRanges, pRange->uAddr, pRange->uSize));

            cRanges++;
        }
    }

    LogFunc(("%zu ranges total\n", cRanges));

    /*
     * Register all ranges as DMA access handlers.
     */

    for (size_t i = 0; i < cRanges; i++)
    {
        BDLERANGE *pRange = &arrRanges[i];

        PHDADMAACCESSHANDLER pHandler = (PHDADMAACCESSHANDLER)RTMemAllocZ(sizeof(HDADMAACCESSHANDLER));
        if (!pHandler)
        {
            rc = VERR_NO_MEMORY;
            break;
        }

        RTListAppend(&pStream->State.lstDMAHandlers, &pHandler->Node);

        pHandler->pStream = pStream; /* Save a back reference to the owner. */

        char szDesc[32];
        RTStrPrintf(szDesc, sizeof(szDesc), "HDA[SD%RU8 - RANGE%02zu]", pStream->u8SD, i);

        int rc2 = PGMR3HandlerPhysicalTypeRegister(PDMDevHlpGetVM(pStream->pHDAState->pDevInsR3), PGMPHYSHANDLERKIND_WRITE,
                                                   hdaDMAAccessHandler,
                                                   NULL, NULL, NULL,
                                                   NULL, NULL, NULL,
                                                   szDesc, &pHandler->hAccessHandlerType);
        AssertRCBreak(rc2);

        pHandler->BDLEAddr  = pRange->uAddr;
        pHandler->BDLESize  = pRange->uSize;

        /* Get first and last pages of the BDLE range. */
        RTGCPHYS pgFirst = pRange->uAddr & ~PAGE_OFFSET_MASK;
        RTGCPHYS pgLast  = RT_ALIGN(pgFirst + pRange->uSize, PAGE_SIZE);

        /* Calculate the region size (in pages). */
        RTGCPHYS regionSize = RT_ALIGN(pgLast - pgFirst, PAGE_SIZE);

        pHandler->GCPhysFirst = pgFirst;
        pHandler->GCPhysLast  = pHandler->GCPhysFirst + (regionSize - 1);

        LogFunc(("\tRegistering region '%s': 0x%x - 0x%x (region size: %zu)\n",
                 szDesc, pHandler->GCPhysFirst, pHandler->GCPhysLast, regionSize));
        LogFunc(("\tBDLE @ 0x%x - 0x%x (%RU32)\n",
                 pHandler->BDLEAddr, pHandler->BDLEAddr + pHandler->BDLESize, pHandler->BDLESize));

        rc2 = PGMHandlerPhysicalRegister(PDMDevHlpGetVM(pStream->pHDAState->pDevInsR3),
                                         pHandler->GCPhysFirst, pHandler->GCPhysLast,
                                         pHandler->hAccessHandlerType, pHandler, NIL_RTR0PTR, NIL_RTRCPTR,
                                         szDesc);
        AssertRCBreak(rc2);

        pHandler->fRegistered = true;
    }

    LogFunc(("Registration ended with rc=%Rrc\n", rc));

    return RT_SUCCESS(rc);
}

/**
 * Unregisters access handlers of a stream's BDLEs.
 *
 * @param   pStream             HDA stream to unregister BDLE access handlers for.
 */
void hdaR3StreamUnregisterDMAHandlers(PHDASTREAM pStream)
{
    LogFunc(("\n"));

    PHDADMAACCESSHANDLER pHandler, pHandlerNext;
    RTListForEachSafe(&pStream->State.lstDMAHandlers, pHandler, pHandlerNext, HDADMAACCESSHANDLER, Node)
    {
        if (!pHandler->fRegistered) /* Handler not registered? Skip. */
            continue;

        LogFunc(("Unregistering 0x%x - 0x%x (%zu)\n",
                 pHandler->GCPhysFirst, pHandler->GCPhysLast, pHandler->GCPhysLast - pHandler->GCPhysFirst));

        int rc2 = PGMHandlerPhysicalDeregister(PDMDevHlpGetVM(pStream->pHDAState->pDevInsR3),
                                               pHandler->GCPhysFirst);
        AssertRC(rc2);

        RTListNodeRemove(&pHandler->Node);

        RTMemFree(pHandler);
        pHandler = NULL;
    }

    Assert(RTListIsEmpty(&pStream->State.lstDMAHandlers));
}

# endif /* HDA_USE_DMA_ACCESS_HANDLER */
# ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO

/**
 * @callback_method_impl{FNRTTHREAD,
 * Asynchronous I/O thread for a HDA stream.
 *
 * This will do the heavy lifting work for us as soon as it's getting notified
 * by another thread.}
 */
static DECLCALLBACK(int) hdaR3StreamAsyncIOThread(RTTHREAD hThreadSelf, void *pvUser)
{
    PHDASTREAMR3 const       pStreamR3     = (PHDASTREAMR3)pvUser;
    PHDASTREAMSTATEAIO const pAIO          = &pStreamR3->State.AIO;
    PHDASTATE const          pThis         = pStreamR3->pHDAStateShared;
    PHDASTATER3 const        pThisCC       = pStreamR3->pHDAStateR3;
    PPDMDEVINS const         pDevIns       = pThisCC->pDevIns;
    PHDASTREAM const         pStreamShared = &pThis->aStreams[pStreamR3 - &pThisCC->aStreams[0]];
    Assert(pStreamR3 - &pThisCC->aStreams[0] == pStreamR3->u8SD);
    Assert(pStreamShared->u8SD == pStreamR3->u8SD);

    /* Signal parent thread that we've started  */
    ASMAtomicWriteBool(&pAIO->fStarted, true);
    RTThreadUserSignal(hThreadSelf);

    LogFunc(("[SD%RU8] Started\n", pStreamShared->u8SD));

    while (!ASMAtomicReadBool(&pAIO->fShutdown))
    {
        int rc2 = RTSemEventWait(pAIO->hEvent, RT_INDEFINITE_WAIT);
        if (RT_SUCCESS(rc2))
        { /* likely */ }
        else
            break;

        if (!ASMAtomicReadBool(&pAIO->fShutdown))
        { /* likely */ }
        else
            break;

        rc2 = RTCritSectEnter(&pAIO->CritSect);
        AssertRC(rc2);
        if (RT_SUCCESS(rc2))
        {
            if (pAIO->fEnabled)
                hdaR3StreamUpdate(pDevIns, pThis, pThisCC, pStreamShared, pStreamR3, false /* fInTimer */);

            int rc3 = RTCritSectLeave(&pAIO->CritSect);
            AssertRC(rc3);
        }
    }

    LogFunc(("[SD%RU8] Ended\n", pStreamShared->u8SD));
    ASMAtomicWriteBool(&pAIO->fStarted, false);

    return VINF_SUCCESS;
}

/**
 * Creates the async I/O thread for a specific HDA audio stream.
 *
 * @returns IPRT status code.
 * @param   pStreamR3           HDA audio stream to create the async I/O thread for.
 */
int hdaR3StreamAsyncIOCreate(PHDASTREAMR3 pStreamR3)
{
    PHDASTREAMSTATEAIO pAIO = &pStreamR3->State.AIO;

    int rc;

    if (!ASMAtomicReadBool(&pAIO->fStarted))
    {
        pAIO->fShutdown = false;
        pAIO->fEnabled  = true; /* Enabled by default. */

        rc = RTSemEventCreate(&pAIO->hEvent);
        if (RT_SUCCESS(rc))
        {
            rc = RTCritSectInit(&pAIO->CritSect);
            if (RT_SUCCESS(rc))
            {
                rc = RTThreadCreateF(&pAIO->hThread, hdaR3StreamAsyncIOThread, pStreamR3, 0 /*cbStack*/,
                                     RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, "hdaAIO%RU8", pStreamR3->u8SD);
                if (RT_SUCCESS(rc))
                    rc = RTThreadUserWait(pAIO->hThread, 10 * 1000 /* 10s timeout */);
            }
        }
    }
    else
        rc = VINF_SUCCESS;

    LogFunc(("[SD%RU8] Returning %Rrc\n", pStreamR3->u8SD, rc));
    return rc;
}

/**
 * Destroys the async I/O thread of a specific HDA audio stream.
 *
 * @returns IPRT status code.
 * @param   pStreamR3           HDA audio stream to destroy the async I/O thread for.
 */
static int hdaR3StreamAsyncIODestroy(PHDASTREAMR3 pStreamR3)
{
    PHDASTREAMSTATEAIO pAIO = &pStreamR3->State.AIO;

    if (!ASMAtomicReadBool(&pAIO->fStarted))
        return VINF_SUCCESS;

    ASMAtomicWriteBool(&pAIO->fShutdown, true);

    int rc = hdaR3StreamAsyncIONotify(pStreamR3);
    AssertRC(rc);

    int rcThread;
    rc = RTThreadWait(pAIO->hThread, 30 * 1000 /* 30s timeout */, &rcThread);
    LogFunc(("Async I/O thread ended with %Rrc (%Rrc)\n", rc, rcThread));

    if (RT_SUCCESS(rc))
    {
        pAIO->hThread = NIL_RTTHREAD;

        rc = RTCritSectDelete(&pAIO->CritSect);
        AssertRC(rc);

        rc = RTSemEventDestroy(pAIO->hEvent);
        AssertRC(rc);
        pAIO->hEvent = NIL_RTSEMEVENT;

        pAIO->fStarted  = false;
        pAIO->fShutdown = false;
        pAIO->fEnabled  = false;
    }

    LogFunc(("[SD%RU8] Returning %Rrc\n", pStreamR3->u8SD, rc));
    return rc;
}

/**
 * Lets the stream's async I/O thread know that there is some data to process.
 *
 * @returns IPRT status code.
 * @param   pStreamR3           HDA stream to notify async I/O thread for.
 */
static int hdaR3StreamAsyncIONotify(PHDASTREAMR3 pStreamR3)
{
    return RTSemEventSignal(pStreamR3->State.AIO.hEvent);
}

/**
 * Locks the async I/O thread of a specific HDA audio stream.
 *
 * @param   pStreamR3           HDA stream to lock async I/O thread for.
 */
void hdaR3StreamAsyncIOLock(PHDASTREAMR3 pStreamR3)
{
    PHDASTREAMSTATEAIO pAIO = &pStreamR3->State.AIO;

    if (!ASMAtomicReadBool(&pAIO->fStarted))
        return;

    int rc2 = RTCritSectEnter(&pAIO->CritSect);
    AssertRC(rc2);
}

/**
 * Unlocks the async I/O thread of a specific HDA audio stream.
 *
 * @param   pStreamR3           HDA stream to unlock async I/O thread for.
 */
void hdaR3StreamAsyncIOUnlock(PHDASTREAMR3 pStreamR3)
{
    PHDASTREAMSTATEAIO pAIO = &pStreamR3->State.AIO;

    if (!ASMAtomicReadBool(&pAIO->fStarted))
        return;

    int rc2 = RTCritSectLeave(&pAIO->CritSect);
    AssertRC(rc2);
}

/**
 * Enables (resumes) or disables (pauses) the async I/O thread.
 *
 * @param   pStreamR3           HDA stream to enable/disable async I/O thread for.
 * @param   fEnable             Whether to enable or disable the I/O thread.
 *
 * @remarks Does not do locking.
 */
void hdaR3StreamAsyncIOEnable(PHDASTREAMR3 pStreamR3, bool fEnable)
{
    PHDASTREAMSTATEAIO pAIO = &pStreamR3->State.AIO;
    ASMAtomicXchgBool(&pAIO->fEnabled, fEnable);
}

# endif /* VBOX_WITH_AUDIO_HDA_ASYNC_IO */
#endif /* IN_RING3 */
