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

#ifdef IN_RING3 /* whole file */


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static void hdaR3StreamSetPosition(PHDASTREAM pStreamShared, PPDMDEVINS pDevIns, PHDASTATE pThis, uint32_t u32LPIB);

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

# ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
    rc = RTCritSectInit(&pStreamR3->CritSect);
    AssertRCReturn(rc, rc);
# endif

    rc = hdaR3StreamPeriodCreate(&pStreamShared->State.Period);
    AssertRCReturn(rc, rc);

    pStreamShared->State.tsLastUpdateNs = 0;

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

# ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
    if (RTCritSectIsInitialized(&pStreamR3->CritSect))
    {
        rc2 = RTCritSectDelete(&pStreamR3->CritSect);
        AssertRC(rc2);
    }
# endif

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
    /* These member can only change on data corruption, despite what the code does further down (bird).  */
    Assert(pStreamShared->u8SD == uSD);
    Assert(pStreamR3->u8SD     == uSD);

    const uint64_t u64BDLBase = RT_MAKE_U64(HDA_STREAM_REG(pThis, BDPL, uSD),
                                            HDA_STREAM_REG(pThis, BDPU, uSD));
    const uint16_t u16LVI     = HDA_STREAM_REG(pThis, LVI, uSD);
    const uint32_t u32CBL     = HDA_STREAM_REG(pThis, CBL, uSD);
    const uint16_t u16FIFOS   = HDA_STREAM_REG(pThis, FIFOS, uSD) + 1;
    const uint16_t u16FMT     = HDA_STREAM_REG(pThis, FMT, uSD);

    /* Is the bare minimum set of registers configured for the stream?
     * If not, bail out early, as there's nothing to do here for us (yet). */
    if (   !u64BDLBase
        || !u16LVI
        || !u32CBL
        || !u16FIFOS
        || !u16FMT)
    {
        LogFunc(("[SD%RU8] Registers not set up yet, skipping (re-)initialization\n", uSD));
        return VINF_SUCCESS;
    }

    PDMAUDIOPCMPROPS Props;
    int rc = hdaR3SDFMTToPCMProps(u16FMT, &Props);
    if (RT_FAILURE(rc))
    {
        LogRel(("HDA: Warning: Format 0x%x for stream #%RU8 not supported\n", HDA_STREAM_REG(pThis, FMT, uSD), uSD));
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

    /*
     * Set the stream's timer Hz rate, based on the stream channel count.
     * Currently this is just a rough guess and we might want to optimize this further.
     *
     * In any case, more channels per SDI/SDO means that we have to drive data more frequently.
     */
    if (pThis->uTimerHz == HDA_TIMER_HZ_DEFAULT) /* Make sure that we don't have any custom Hz rate set we want to enforce */
    {
        if (Props.cChannels >= 5)
            pStreamShared->State.uTimerHz = 300;
        else if (Props.cChannels == 4)
            pStreamShared->State.uTimerHz = 150;
        else
            pStreamShared->State.uTimerHz = 100;
    }
    else
        pStreamShared->State.uTimerHz = pThis->uTimerHz;

#ifndef VBOX_WITH_AUDIO_HDA_51_SURROUND
    if (Props.cChannels > 2)
    {
        /*
         * When not running with surround support enabled, override the audio channel count
         * with stereo (2) channels so that we at least can properly work with those.
         *
         * Note: This also involves dealing with surround setups the guest might has set up for us.
         */
        LogRel2(("HDA: More than stereo (2) channels are not supported (%RU8 requested), "
                 "falling back to stereo channels for stream #%RU8\n", Props.cChannels, uSD));
        Props.cChannels = 2;
        Props.cShift    = PDMAUDIOPCMPROPS_MAKE_SHIFT_PARMS(Props.cbSample, Props.cChannels);
    }
#endif

    /* Did some of the vital / critical parameters change?
     * If not, we can skip a lot of the (re-)initialization and just (re-)use the existing stuff.
     * Also, tell the caller so that further actions can be taken. */
    if (   uSD        == pStreamShared->u8SD   /* paranoia OFC */
        && u64BDLBase == pStreamShared->u64BDLBase
        && u16LVI     == pStreamShared->u16LVI
        && u32CBL     == pStreamShared->u32CBL
        && u16FIFOS   == pStreamShared->u16FIFOS
        && u16FMT     == pStreamShared->u16FMT)
    {
        LogFunc(("[SD%RU8] No format change, skipping (re-)initialization\n", uSD));
        return VINF_NO_CHANGE;
    }

    pStreamShared->u8SD       = uSD;

    /* Update all register copies so that we later know that something has changed. */
    pStreamShared->u64BDLBase = u64BDLBase;
    pStreamShared->u16LVI     = u16LVI;
    pStreamShared->u32CBL     = u32CBL;
    pStreamShared->u16FIFOS   = u16FIFOS;
    pStreamShared->u16FMT     = u16FMT;

    PPDMAUDIOSTREAMCFG pCfg = &pStreamShared->State.Cfg;
    pCfg->Props = Props;

    /* (Re-)Allocate the stream's internal DMA buffer, based on the PCM  properties we just got above. */
    if (pStreamR3->State.pCircBuf)
    {
        RTCircBufDestroy(pStreamR3->State.pCircBuf);
        pStreamR3->State.pCircBuf = NULL;
    }

    /* By default we allocate an internal buffer of 100ms. */
    rc = RTCircBufCreate(&pStreamR3->State.pCircBuf,
                         DrvAudioHlpMilliToBytes(100 /* ms */, &pCfg->Props)); /** @todo Make this configurable. */
    AssertRCReturn(rc, rc);

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
            rc = VERR_NOT_SUPPORTED;
            break;
    }

    /* Set scheduling hint (if available). */
    if (pStreamShared->State.uTimerHz)
        pCfg->Device.cMsSchedulingHint = 1000 /* ms */ / pStreamShared->State.uTimerHz;

    LogFunc(("[SD%RU8] DMA @ 0x%x (%RU32 bytes), LVI=%RU16, FIFOS=%RU16\n",
             uSD, pStreamShared->u64BDLBase, pStreamShared->u32CBL, pStreamShared->u16LVI, pStreamShared->u16FIFOS));

    if (RT_SUCCESS(rc))
    {
        /* Make sure that the chosen Hz rate dividable by the stream's rate. */
        if (pStreamShared->State.Cfg.Props.uHz % pStreamShared->State.uTimerHz != 0)
            LogRel(("HDA: Stream timer Hz rate (%RU32) does not fit to stream #%RU8 timing (%RU32)\n",
                    pStreamShared->State.uTimerHz, uSD, pStreamShared->State.Cfg.Props.uHz));

        /* Figure out how many transfer fragments we're going to use for this stream. */
        /** @todo Use a more dynamic fragment size? */
        uint8_t cFragments = pStreamShared->u16LVI + 1;
        if (cFragments <= 1)
            cFragments = 2; /* At least two fragments (BDLEs) must be present. */

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
                if (   (cfPosAdjust * pStreamR3->State.Mapping.cbFrameSize) == BDLE.Desc.u32BufSize
                    && cFragments)
                {
                    cFragments--;
                }

                /* Initialize position adjustment counter. */
                pStreamShared->State.cfPosAdjustDefault = cfPosAdjust;
                pStreamShared->State.cfPosAdjustLeft    = pStreamShared->State.cfPosAdjustDefault;

                LogRel2(("HDA: Position adjustment for stream #%RU8 active (%RU32 frames)\n",
                         uSD, pStreamShared->State.cfPosAdjustDefault));
            }
        }

        LogFunc(("[SD%RU8] cfPosAdjust=%RU32, cFragments=%RU8\n", uSD, cfPosAdjust, cFragments));

        /*
         * Set up data transfer stuff.
         */

        /* Calculate the fragment size the guest OS expects interrupt delivery at. */
        pStreamShared->State.cbTransferSize = pStreamShared->u32CBL / cFragments;
        Assert(pStreamShared->State.cbTransferSize);
        Assert(pStreamShared->State.cbTransferSize % pStreamR3->State.Mapping.cbFrameSize == 0);
        ASSERT_GUEST_LOGREL_MSG_STMT(pStreamShared->State.cbTransferSize,
                                     ("Transfer size for stream #%RU8 is invalid\n", uSD), rc = VERR_INVALID_PARAMETER);
        if (RT_SUCCESS(rc))
        {
            /* Calculate the bytes we need to transfer to / from the stream's DMA per iteration.
             * This is bound to the device's Hz rate and thus to the (virtual) timing the device expects. */
            pStreamShared->State.cbTransferChunk = (pStreamShared->State.Cfg.Props.uHz / pStreamShared->State.uTimerHz) * pStreamR3->State.Mapping.cbFrameSize;
            Assert(pStreamShared->State.cbTransferChunk);
            Assert(pStreamShared->State.cbTransferChunk % pStreamR3->State.Mapping.cbFrameSize == 0);
            ASSERT_GUEST_LOGREL_MSG_STMT(pStreamShared->State.cbTransferChunk,
                                         ("Transfer chunk for stream #%RU8 is invalid\n", uSD),
                                         rc = VERR_INVALID_PARAMETER);
            if (RT_SUCCESS(rc))
            {
                /* Make sure that the transfer chunk does not exceed the overall transfer size. */
                if (pStreamShared->State.cbTransferChunk > pStreamShared->State.cbTransferSize)
                    pStreamShared->State.cbTransferChunk = pStreamShared->State.cbTransferSize;

                const uint64_t cTicksPerHz = PDMDevHlpTimerGetFreq(pDevIns, pStreamShared->hTimer) / pStreamShared->State.uTimerHz;

                /* Calculate the timer ticks per byte for this stream. */
                pStreamShared->State.cTicksPerByte = cTicksPerHz / pStreamShared->State.cbTransferChunk;
                Assert(pStreamShared->State.cTicksPerByte);

                /* Calculate timer ticks per transfer. */
                pStreamShared->State.cTransferTicks = pStreamShared->State.cbTransferChunk * pStreamShared->State.cTicksPerByte;
                Assert(pStreamShared->State.cTransferTicks);

                LogFunc(("[SD%RU8] Timer %uHz (%RU64 ticks per Hz), cTicksPerByte=%RU64, cbTransferChunk=%RU32, " \
                         "cTransferTicks=%RU64, cbTransferSize=%RU32\n",
                         uSD, pStreamShared->State.uTimerHz, cTicksPerHz, pStreamShared->State.cTicksPerByte,
                         pStreamShared->State.cbTransferChunk, pStreamShared->State.cTransferTicks, pStreamShared->State.cbTransferSize));

                /* Make sure to also update the stream's DMA counter (based on its current LPIB value). */
                hdaR3StreamSetPosition(pStreamShared, pDevIns, pThis, HDA_STREAM_REG(pThis, LPIB, uSD));

#ifdef LOG_ENABLED
                hdaR3BDLEDumpAll(pDevIns, pThis, pStreamShared->u64BDLBase, pStreamShared->u16LVI + 1);
#endif
            }
        }
    }

    if (RT_FAILURE(rc))
        LogRel(("HDA: Initializing stream #%RU8 failed with %Rrc\n", uSD, rc));

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
    HDA_STREAM_REG(pThis, STS,   uSD) = HDA_SDSTS_FIFORDY;
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
    pStreamShared->State.cbTransferProcessed        = 0;
    pStreamShared->State.cTransferPendingInterrupts = 0;
    pStreamShared->State.tsTransferLast = 0;
    pStreamShared->State.tsTransferNext = 0;

    /* Initialize other timestamps. */
    pStreamShared->State.tsLastUpdateNs = 0;

    RT_ZERO(pStreamShared->State.BDLE);
    pStreamShared->State.uCurBDLE = 0;

    if (pStreamR3->State.pCircBuf)
        RTCircBufReset(pStreamR3->State.pCircBuf);

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

static uint32_t hdaR3StreamGetPosition(PHDASTATE pThis, PHDASTREAM pStreamShared)
{
    return HDA_STREAM_REG(pThis, LPIB, pStreamShared->u8SD);
}

/*
 * Updates an HDA stream's current read or write buffer position (depending on the stream type) by
 * updating its associated LPIB register and DMA position buffer (if enabled).
 *
 * @param   pStreamShared       HDA stream to update read / write position for (shared).
 * @param   pDevIns             The device instance.
 * @param   pThis               The shared HDA device state.
 * @param   u32LPIB             Absolute position (in bytes) to set current read / write position to.
 */
static void hdaR3StreamSetPosition(PHDASTREAM pStreamShared, PPDMDEVINS pDevIns, PHDASTATE pThis, uint32_t u32LPIB)
{
    AssertPtrReturnVoid(pStreamShared);

    Log3Func(("[SD%RU8] LPIB=%RU32 (DMA Position Buffer Enabled: %RTbool)\n",  pStreamShared->u8SD, u32LPIB, pThis->fDMAPosition));

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

    Log3Func(("cbWrittenTotal=%RU32\n", cbWrittenTotal));

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
            Log2Func(("[SD%RU8] %RU32/%zu bytes read\n", pStreamR3->u8SD, cbWritten, cbSrc));
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
 * @param   fInTimer            Set if we're in the timer callout.
 */
static int hdaR3StreamTransfer(PPDMDEVINS pDevIns, PHDASTATE pThis, PHDASTATER3 pThisCC, PHDASTREAM pStreamShared,
                               PHDASTREAMR3 pStreamR3, uint32_t cbToProcessMax, bool fInTimer)
{
    uint8_t const uSD = pStreamShared->u8SD;
    hdaR3StreamLock(pStreamR3);

    PHDASTREAMPERIOD pPeriod = &pStreamShared->State.Period;
    hdaR3StreamPeriodLock(pPeriod);

    bool fProceed = true;

    /* Stream not running? */
    if (!pStreamShared->State.fRunning)
    {
        Log3Func(("[SD%RU8] Not running\n", uSD));
        fProceed = false;
    }
    else if (HDA_STREAM_REG(pThis, STS, uSD) & HDA_SDSTS_BCIS)
    {
        Log3Func(("[SD%RU8] BCIS bit set\n", uSD));
        fProceed = false;
    }

    if (!fProceed)
    {
        hdaR3StreamPeriodUnlock(pPeriod);
        hdaR3StreamUnlock(pStreamR3);
        return VINF_SUCCESS;
    }

    const uint64_t tsNow = PDMDevHlpTimerGet(pDevIns, pStreamShared->hTimer);

    if (!pStreamShared->State.tsTransferLast)
        pStreamShared->State.tsTransferLast = tsNow;

#ifdef DEBUG
    const int64_t iTimerDelta = tsNow - pStreamShared->State.tsTransferLast;
    Log3Func(("[SD%RU8] Time now=%RU64, last=%RU64 -> %RI64 ticks delta\n",
              uSD, tsNow, pStreamShared->State.tsTransferLast, iTimerDelta));
#endif

    pStreamShared->State.tsTransferLast = tsNow;

    /* Sanity checks. */
    Assert(uSD < HDA_MAX_STREAMS);
    Assert(pStreamShared->u64BDLBase);
    Assert(pStreamShared->u32CBL);
    Assert(pStreamShared->u16FIFOS);

    /* State sanity checks. */
    Assert(ASMAtomicReadBool(&pStreamShared->State.fInReset) == false);

    int rc = VINF_SUCCESS;

    /* Fetch first / next BDL entry. */
    PHDABDLE pBDLE = &pStreamShared->State.BDLE;
    if (hdaR3BDLEIsComplete(pBDLE))
    {
        rc = hdaR3BDLEFetch(pDevIns, pBDLE, pStreamShared->u64BDLBase, pStreamShared->State.uCurBDLE);
        AssertRC(rc);
    }

    uint32_t cbToProcess = RT_MIN(pStreamShared->State.cbTransferSize - pStreamShared->State.cbTransferProcessed,
                                  pStreamShared->State.cbTransferChunk);

    Log3Func(("[SD%RU8] cbToProcess=%RU32, cbToProcessMax=%RU32\n", uSD, cbToProcess, cbToProcessMax));

    if (cbToProcess > cbToProcessMax)
    {
        LogFunc(("[SD%RU8] Limiting transfer (cbToProcess=%RU32, cbToProcessMax=%RU32)\n", uSD, cbToProcess, cbToProcessMax));

        /* Never process more than a stream currently can handle. */
        cbToProcess = cbToProcessMax;
    }

    uint32_t cbProcessed = 0;
    uint32_t cbLeft      = cbToProcess;

    uint8_t abChunk[HDA_FIFO_MAX + 1];
    while (cbLeft)
    {
        /* Limit the chunk to the stream's FIFO size and what's left to process. */
        uint32_t cbChunk = RT_MIN(cbLeft, pStreamShared->u16FIFOS);

        /* Limit the chunk to the remaining data of the current BDLE. */
        cbChunk = RT_MIN(cbChunk, pBDLE->Desc.u32BufSize - pBDLE->State.u32BufOff);

        /* If there are position adjustment frames left to be processed,
         * make sure that we process them first as a whole. */
        if (pStreamShared->State.cfPosAdjustLeft)
            cbChunk = RT_MIN(cbChunk, uint32_t(pStreamShared->State.cfPosAdjustLeft * pStreamR3->State.Mapping.cbFrameSize));

        Log3Func(("[SD%RU8] cbChunk=%RU32, cPosAdjustFramesLeft=%RU16\n",
                  uSD, cbChunk, pStreamShared->State.cfPosAdjustLeft));

        if (!cbChunk)
            break;

        uint32_t   cbDMA    = 0;
        PRTCIRCBUF pCircBuf = pStreamR3->State.pCircBuf;

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

                memcpy(abChunk + cbDMAWritten, pvBuf, cbBuf);

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
                memset((uint8_t *)abChunk + cbDMAWritten, 0, cbDMAToWrite);
                cbDMAWritten = cbChunk;
            }

            rc = hdaR3DMAWrite(pDevIns, pThis, pStreamShared, pStreamR3, abChunk, cbDMAWritten, &cbDMA /* pcbWritten */);
            if (RT_FAILURE(rc))
                LogRel(("HDA: Writing to stream #%RU8 DMA failed with %Rrc\n", uSD, rc));

            STAM_PROFILE_STOP(&pThis->StatIn, a);
        }
        else if (hdaGetDirFromSD(uSD) == PDMAUDIODIR_OUT) /* Output (SDO). */
        {
            STAM_PROFILE_START(&pThis->StatOut, a);

            rc = hdaR3DMARead(pDevIns, pThis, pStreamShared, pStreamR3, abChunk, cbChunk, &cbDMA /* pcbRead */);
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
                            memcpy(pvBuf, abChunk + cbDMARead, cbBuf);
                            cbDMARead += (uint32_t)cbBuf;
                            cbDMALeft -= (uint32_t)cbBuf;
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

                        uint8_t *pbSrcBuf = abChunk;
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

                                Log3Func(("Mapping #%u: Frame #%02u:    -> cbSrcOff=%zu\n", m, i, cbSrcOff));
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

            /*
             * Update the stream's current position.
             * Do this as accurate and close to the actual data transfer as possible.
             * All guetsts rely on this, depending on the mechanism they use (LPIB register or DMA counters).
             */
            uint32_t cbStreamPos = hdaR3StreamGetPosition(pThis, pStreamShared);
            if (cbStreamPos == pStreamShared->u32CBL)
                cbStreamPos = 0;

            hdaR3StreamSetPosition(pStreamShared, pDevIns, pThis, cbStreamPos + cbDMA);
        }

        if (hdaR3BDLEIsComplete(pBDLE))
        {
            Log3Func(("[SD%RU8] Complete: %R[bdle]\n", uSD, pBDLE));

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
                    pStreamShared->State.cTransferPendingInterrupts++;

                    AssertMsg(pStreamShared->State.cTransferPendingInterrupts <= 32,
                              ("Too many pending interrupts (%RU8) for stream #%RU8\n",
                               pStreamShared->State.cTransferPendingInterrupts, uSD));
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

    Log3Func(("[SD%RU8] cbToProcess=%RU32, cbProcessed=%RU32, cbLeft=%RU32, %R[bdle], rc=%Rrc\n",
              uSD, cbToProcess, cbProcessed, cbLeft, pBDLE, rc));

    /* Sanity. */
    Assert(cbProcessed == cbToProcess);
    Assert(cbLeft      == 0);

    /* Only do the data accounting if we don't have to do any position
     * adjustment anymore. */
    if (pStreamShared->State.cfPosAdjustLeft == 0)
    {
        hdaR3StreamPeriodInc(pPeriod, RT_MIN(cbProcessed / pStreamR3->State.Mapping.cbFrameSize,
                                             hdaR3StreamPeriodGetRemainingFrames(pPeriod)));

        pStreamShared->State.cbTransferProcessed += cbProcessed;
    }

    /* Make sure that we never report more stuff processed than initially announced. */
    if (pStreamShared->State.cbTransferProcessed > pStreamShared->State.cbTransferSize)
        pStreamShared->State.cbTransferProcessed = pStreamShared->State.cbTransferSize;

    uint32_t cbTransferLeft     = pStreamShared->State.cbTransferSize - pStreamShared->State.cbTransferProcessed;
    bool     fTransferComplete  = !cbTransferLeft;
    uint64_t tsTransferNext     = 0;

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
                                                 hdaWalClkGetCurrent(pThis)
                                               + hdaR3StreamPeriodFramesToWalClk(pPeriod,
                                                                                   pStreamShared->State.cbTransferProcessed
                                                                                 / pStreamR3->State.Mapping.cbFrameSize),
                                               false /* fForce */);
        RT_NOREF(fWalClkSet);
    }

    /* Does the period have any interrupts outstanding? */
    if (pStreamShared->State.cTransferPendingInterrupts)
    {
        Log3Func(("[SD%RU8] Scheduling interrupt\n", uSD));

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
    }
    else /* Transfer still in-flight -- schedule the next timing slot. */
    {
        uint32_t cbTransferNext = cbTransferLeft;

        /* No data left to transfer anymore or do we have more data left
         * than we can transfer per timing slot? Clamp. */
        if (   !cbTransferNext
            || cbTransferNext > pStreamShared->State.cbTransferChunk)
        {
            cbTransferNext = pStreamShared->State.cbTransferChunk;
        }

        tsTransferNext = tsNow + (cbTransferNext * pStreamShared->State.cTicksPerByte);

        /*
         * If the current transfer is complete, reset our counter.
         *
         * This can happen for examlpe if the guest OS (like macOS) sets up
         * big BDLEs without IOC bits set (but for the last one) and the
         * transfer is complete before we reach such a BDL entry.
         */
        if (fTransferComplete)
            pStreamShared->State.cbTransferProcessed = 0;
    }

    /* If we need to do another transfer, (re-)arm the device timer.  */
    if (tsTransferNext) /* Can be 0 if no next transfer is needed. */
    {
        Log3Func(("[SD%RU8] Scheduling timer\n", uSD));

        LogFunc(("Timer set SD%RU8\n", uSD));
        Assert(!fInTimer || tsNow == PDMDevHlpTimerGet(pDevIns, pStreamShared->hTimer));
        hdaR3TimerSet(pDevIns, pStreamShared, tsTransferNext,
                      true /* fForce - skip tsTransferNext check */, fInTimer ? tsNow : 0);

        pStreamShared->State.tsTransferNext = tsTransferNext;
    }

    pStreamShared->State.tsTransferLast = tsNow;

    Log3Func(("[SD%RU8] cbTransferLeft=%RU32 -- %RU32/%RU32\n",
              uSD, cbTransferLeft, pStreamShared->State.cbTransferProcessed, pStreamShared->State.cbTransferSize));
    Log3Func(("[SD%RU8] fTransferComplete=%RTbool, cTransferPendingInterrupts=%RU8\n",
              uSD, fTransferComplete, pStreamShared->State.cTransferPendingInterrupts));
    Log3Func(("[SD%RU8] tsNow=%RU64, tsTransferNext=%RU64 (in %RU64 ticks)\n",
              uSD, tsNow, tsTransferNext, tsTransferNext - tsNow));

    hdaR3StreamPeriodUnlock(pPeriod);
    hdaR3StreamUnlock(pStreamR3);

    return VINF_SUCCESS;
}

/**
 * Updates a HDA stream by doing its required data transfers.
 * The host sink(s) set the overall pace.
 *
 * This routine is called by both, the synchronous and the asynchronous, implementations.
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

    int rc2;

    if (hdaGetDirFromSD(pStreamShared->u8SD) == PDMAUDIODIR_OUT) /* Output (SDO). */
    {
        bool fDoRead = false; /* Whether to read from the HDA stream or not. */

# ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
        if (fInTimer)
# endif
        {
            const uint32_t cbStreamFree = hdaR3StreamGetFree(pStreamR3);
            if (cbStreamFree)
            {
                /* Do the DMA transfer. */
                rc2 = hdaR3StreamTransfer(pDevIns, pThis, pThisCC, pStreamShared, pStreamR3, cbStreamFree, fInTimer);
                AssertRC(rc2);
            }

            /* Only read from the HDA stream at the given scheduling rate. */
            const uint64_t tsNowNs = RTTimeNanoTS();
            if (tsNowNs - pStreamShared->State.tsLastUpdateNs >= pStreamShared->State.Cfg.Device.cMsSchedulingHint * RT_NS_1MS)
            {
                fDoRead = true;
                pStreamShared->State.tsLastUpdateNs = tsNowNs;
            }
        }

        Log3Func(("[SD%RU8] fInTimer=%RTbool, fDoRead=%RTbool\n", pStreamShared->u8SD, fInTimer, fDoRead));

# ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
        if (fDoRead)
        {
            rc2 = hdaR3StreamAsyncIONotify(pStreamR3);
            AssertRC(rc2);
        }
# endif

# ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
        if (!fInTimer) /* In async I/O thread */
        {
# else
        if (fDoRead)
        {
# endif
            const uint32_t cbSinkWritable     = AudioMixerSinkGetWritable(pSink);
            const uint32_t cbStreamReadable   = hdaR3StreamGetUsed(pStreamR3);
            const uint32_t cbToReadFromStream = RT_MIN(cbStreamReadable, cbSinkWritable);

            Log3Func(("[SD%RU8] cbSinkWritable=%RU32, cbStreamReadable=%RU32\n", pStreamShared->u8SD, cbSinkWritable, cbStreamReadable));

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
        {
# endif
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
                uint8_t abFIFO[HDA_FIFO_MAX + 1];
                while (cbSinkReadable)
                {
                    uint32_t cbRead;
                    rc2 = AudioMixerSinkRead(pSink, AUDMIXOP_COPY,
                                             abFIFO, RT_MIN(cbSinkReadable, (uint32_t)sizeof(abFIFO)), &cbRead);
                    AssertRCBreak(rc2);

                    if (!cbRead)
                    {
                        AssertMsgFailed(("Nothing read from sink, even if %RU32 bytes were (still) announced\n", cbSinkReadable));
                        break;
                    }

                    /* Write (guest input) data to the stream which was read from stream's sink before. */
                    uint32_t cbWritten;
                    rc2 = hdaR3StreamWrite(pStreamR3, abFIFO, cbRead, &cbWritten);
                    AssertRCBreak(rc2);
                    AssertBreak(cbWritten > 0); /* Should never happen, as we know how much we can write. */

                    Assert(cbSinkReadable >= cbRead);
                    cbSinkReadable -= cbRead;
                }
            }
# ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
        }
        else /* fInTimer */
        {
# endif

# ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
            const uint64_t tsNowNs = RTTimeNanoTS();
            if (tsNowNs - pStreamShared->State.tsLastUpdateNs >= pStreamShared->State.Cfg.Device.cMsSchedulingHint * RT_NS_1MS)
            {
                rc2 = hdaR3StreamAsyncIONotify(pStreamR3);
                AssertRC(rc2);

                pStreamShared->State.tsLastUpdateNs = tsNowNs;
            }
# endif
            const uint32_t cbStreamUsed = hdaR3StreamGetUsed(pStreamR3);
            if (cbStreamUsed)
            {
                rc2 = hdaR3StreamTransfer(pDevIns, pThis, pThisCC, pStreamShared, pStreamR3, cbStreamUsed, fInTimer);
                AssertRC(rc2);
            }
# ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
        }
# endif
    }
}

/**
 * Locks an HDA stream for serialized access.
 *
 * @returns IPRT status code.
 * @param   pStreamR3           HDA stream to lock (ring-3 bits).
 */
void hdaR3StreamLock(PHDASTREAMR3 pStreamR3)
{
    AssertPtrReturnVoid(pStreamR3);
# ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
    int rc2 = RTCritSectEnter(&pStreamR3->CritSect);
    AssertRC(rc2);
# else
    Assert(PDMDevHlpCritSectIsOwner(pStream->pHDAState->pDevInsR3, pStream->pHDAState->CritSect));
# endif
}

/**
 * Unlocks a formerly locked HDA stream.
 *
 * @returns IPRT status code.
 * @param   pStreamR3           HDA stream to unlock (ring-3 bits).
 */
void hdaR3StreamUnlock(PHDASTREAMR3 pStreamR3)
{
    AssertPtrReturnVoid(pStreamR3);
# ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
    int rc2 = RTCritSectLeave(&pStreamR3->CritSect);
    AssertRC(rc2);
# endif
}

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
    ASMAtomicXchgBool(&pAIO->fStarted, true);
    RTThreadUserSignal(hThreadSelf);

    LogFunc(("[SD%RU8] Started\n", pStreamShared->u8SD));

    for (;;)
    {
        int rc2 = RTSemEventWait(pAIO->hEvent, RT_INDEFINITE_WAIT);
        if (RT_FAILURE(rc2))
            break;

        if (ASMAtomicReadBool(&pAIO->fShutdown))
            break;

        rc2 = RTCritSectEnter(&pAIO->CritSect);
        AssertRC(rc2);
        if (RT_SUCCESS(rc2))
        {
            if (!pAIO->fEnabled)
            {
                RTCritSectLeave(&pAIO->CritSect);
                continue;
            }

            hdaR3StreamUpdate(pDevIns, pThis, pThisCC, pStreamShared, pStreamR3, false /* fInTimer */);

            int rc3 = RTCritSectLeave(&pAIO->CritSect);
            AssertRC(rc3);
        }
    }

    LogFunc(("[SD%RU8] Ended\n", pStreamShared->u8SD));
    ASMAtomicXchgBool(&pAIO->fStarted, false);

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

