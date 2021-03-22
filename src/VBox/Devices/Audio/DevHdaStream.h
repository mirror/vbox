/* $Id$ */
/** @file
 * HDAStream.h - Streams for HD Audio.
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

#ifndef VBOX_INCLUDED_SRC_Audio_DevHdaStream_h
#define VBOX_INCLUDED_SRC_Audio_DevHdaStream_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "DevHdaCommon.h"
#include "DevHdaStreamMap.h"


#ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
/**
 * HDA stream's state for asynchronous I/O.
 */
typedef struct HDASTREAMSTATEAIO
{
    /** Thread handle for the actual I/O thread. */
    RTTHREAD                hThread;
    /** Event for letting the thread know there is some data to process. */
    RTSEMEVENT              hEvent;
    /** Critical section for synchronizing access. */
    RTCRITSECT              CritSect;
    /** Started indicator. */
    volatile bool           fStarted;
    /** Shutdown indicator. */
    volatile bool           fShutdown;
    /** Whether the thread should do any data processing or not. */
    volatile bool           fEnabled;
    bool                    afPadding[1+4];
} HDASTREAMSTATEAIO;
/** Pointer to a HDA stream's asynchronous I/O state. */
typedef HDASTREAMSTATEAIO *PHDASTREAMSTATEAIO;
#endif

/**
 * Structure containing HDA stream debug stuff, configurable at runtime.
 */
typedef struct HDASTREAMDEBUGRT
{
    /** Whether debugging is enabled or not. */
    bool                     fEnabled;
    uint8_t                  Padding[7];
    /** File for dumping stream reads / writes.
     *  For input streams, this dumps data being written to the device FIFO,
     *  whereas for output streams this dumps data being read from the device FIFO. */
    R3PTRTYPE(PPDMAUDIOFILE) pFileStream;
    /** File for dumping raw DMA reads / writes.
     *  For input streams, this dumps data being written to the device DMA,
     *  whereas for output streams this dumps data being read from the device DMA. */
    R3PTRTYPE(PPDMAUDIOFILE) pFileDMARaw;
    /** File for dumping mapped (that is, extracted) DMA reads / writes. */
    R3PTRTYPE(PPDMAUDIOFILE) pFileDMAMapped;
} HDASTREAMDEBUGRT;

/**
 * Structure containing HDA stream debug information.
 */
typedef struct HDASTREAMDEBUG
{
    /** Runtime debug info. */
    HDASTREAMDEBUGRT        Runtime;
#ifdef DEBUG
    /** Critical section to serialize access if needed. */
    RTCRITSECT              CritSect;
    uint32_t                Padding0[2];
    /** Number of total read accesses. */
    uint64_t                cReadsTotal;
    /** Number of total DMA bytes read. */
    uint64_t                cbReadTotal;
    /** Timestamp (in ns) of last read access. */
    uint64_t                tsLastReadNs;
    /** Number of total write accesses. */
    uint64_t                cWritesTotal;
    /** Number of total DMA bytes written. */
    uint64_t                cbWrittenTotal;
    /** Number of total write accesses since last iteration (Hz). */
    uint64_t                cWritesHz;
    /** Number of total DMA bytes written since last iteration (Hz). */
    uint64_t                cbWrittenHz;
    /** Timestamp (in ns) of beginning a new write slot. */
    uint64_t                tsWriteSlotBegin;
    /** Number of current silence samples in a (consecutive) row. */
    uint64_t                csSilence;
    /** Number of silent samples in a row to consider an audio block as audio gap (silence). */
    uint64_t                cSilenceThreshold;
    /** How many bytes to skip in an audio stream before detecting silence.
     *  (useful for intros and silence at the beginning of a song). */
    uint64_t                cbSilenceReadMin;
#else
    uint64_t                au64Alignment[2];
#endif
} HDASTREAMDEBUG;
typedef HDASTREAMDEBUG *PHDASTREAMDEBUG;

/**
 * Internal state of a HDA stream.
 */
typedef struct HDASTREAMSTATE
{
    /** Flag indicating whether this stream currently is
     *  in reset mode and therefore not acccessible by the guest. */
    volatile bool           fInReset;
    /** Flag indicating if the stream is in running state or not. */
    volatile bool           fRunning;
    /** The stream's I/O timer Hz rate. */
    uint16_t                uTimerIoHz;
    /** How many interrupts are pending due to
     *  BDLE interrupt-on-completion (IOC) bits set. */
    uint8_t                 cTransferPendingInterrupts;
    /** Unused, padding. */
    uint8_t                 abPadding1[2];
    /** Input streams only: Set when we switch from feeding the guest silence and
     *  commits to proving actual audio input bytes. */
    bool                    fInputPreBuffered;
    /** Input streams only: The number of bytes we need to prebuffer. */
    uint32_t                cbInputPreBuffer;
    uint32_t                u32Padding2;
    /** Timestamp (absolute, in timer ticks) of the last DMA data transfer.
     * @note This is used for wall clock (WALCLK) calculations.  */
    uint64_t volatile       tsTransferLast;
    /** Timestamp (absolute, in timer ticks) of the next DMA data transfer.
     *  Next for determining the next scheduling window.
     *  Can be 0 if no next transfer is scheduled. */
    uint64_t                tsTransferNext;
    /** Total transfer size (in bytes) of a transfer period.
     * @note This is in host side frames, in case we're doing any mapping. */
    uint32_t                cbTransferSize;
    /** The size of an average transfer. */
    uint32_t                cbAvgTransfer;
    /** The stream's current host side configuration.
     * This should match the SDnFMT in all respects but maybe the channel count as
     * we may need to expand mono or into/from into stereo.  The unmodified SDnFMT
     * properties can be found in HDASTREAMR3::Mapping::PCMProps. */
    PDMAUDIOSTREAMCFG       Cfg;
    /** Timestamp (real time, in ns) of last DMA transfer. */
    uint64_t                tsLastTransferNs;
    /** Timestamp (real time, in ns) of last stream read (to backends).
     *  When running in async I/O mode, this differs from \a tsLastTransferNs,
     *  because reading / processing will be done in a separate stream. */
    uint64_t                tsLastReadNs;

    /** This is set to the timer clock time when the msInitialDelay period is over.
     * Once reached, this is set to zero to avoid unnecessary time queries. */
    uint64_t                tsAioDelayEnd;
    /** The start time for the playback (on the timer clock). */
    uint64_t                tsStart;

    /** @name DMA engine
     * @{ */
    /** The offset into the current BDLE. */
    uint32_t                offCurBdle;
    /** LVI + 1 */
    uint16_t                cBdles;
    /** The index of the current BDLE.
     * This is the entry which period is currently "running" on the DMA timer.  */
    uint8_t                 idxCurBdle;
    /** The number of prologue scheduling steps.
     * This is used when the tail BDLEs doesn't have IOC set.  */
    uint8_t                 cSchedulePrologue;
    /** Number of scheduling steps. */
    uint16_t                cSchedule;
    /** Current scheduling step. */
    uint16_t                idxSchedule;
    /** Current loop number within the current scheduling step.  */
    uint32_t                idxScheduleLoop;

    /** Buffer descriptors and additional timer scheduling state.
     * (Same as HDABDLEDESC, with more sensible naming.)  */
    struct
    {
        /** The buffer address. */
        uint64_t            GCPhys;
        /** The buffer size (guest bytes). */
        uint32_t            cb;
        /** The flags (only bit 0 is defined).    */
        uint32_t            fFlags;
    }                       aBdl[256];
    /** Scheduling steps. */
    struct
    {
        /** Number of timer ticks per period.
         * ASSUMES that we don't need a full second and that the timer resolution
         * isn't much higher than nanoseconds. */
        uint32_t            cPeriodTicks;
        /** The period length in host bytes. */
        uint32_t            cbPeriod;
        /** Number of times to repeat the period. */
        uint32_t            cLoops;
        /** The BDL index of the first entry.   */
        uint8_t             idxFirst;
        /** The number of BDL entries. */
        uint8_t             cEntries;
        uint8_t             abPadding[2];
    }                       aSchedule[512+8];
    /** @} */
} HDASTREAMSTATE;
AssertCompileSizeAlignment(HDASTREAMSTATE, 8);
AssertCompileMemberAlignment(HDASTREAMSTATE, aBdl, 8);
AssertCompileMemberAlignment(HDASTREAMSTATE, aBdl, 16);
AssertCompileMemberAlignment(HDASTREAMSTATE, aSchedule, 16);

/**
 * An HDA stream (SDI / SDO) - shared.
 *
 * @note This HDA stream has nothing to do with a regular audio stream handled
 *       by the audio connector or the audio mixer. This HDA stream is a serial
 *       data in/out stream (SDI/SDO) defined in hardware and can contain
 *       multiple audio streams in one single SDI/SDO (interleaving streams).
 *
 * How a specific SDI/SDO is mapped to our internal audio streams relies on the
 * stream channel mappings.
 *
 * Contains only register values which do *not* change until a stream reset
 * occurs.
 */
typedef struct HDASTREAM
{
    /** Internal state of this stream. */
    HDASTREAMSTATE              State;

    /** Stream descriptor number (SDn). */
    uint8_t                     u8SD;
    /** Current channel index.
     *  For a stereo stream, this is u8Channel + 1. */
    uint8_t                     u8Channel;
    /** FIFO Watermark (checked + translated in bytes, FIFOW).
     * This will be update from hdaRegWriteSDFIFOW() and also copied
     * hdaR3StreamInit() for some reason. */
    uint8_t                     u8FIFOW;

    /** @name Register values at stream setup.
     * These will all be copied in hdaR3StreamInit().
     * @{ */
    /** FIFO Size (checked + translated in bytes, FIFOS).
     * This is supposedly the max number of bytes we'll be DMA'ing in one chunk
     * and correspondingly the LPIB & wall clock update jumps.  However, we're
     * not at all being honest with the guest about this. */
    uint8_t                     u8FIFOS;
    /** Cyclic Buffer Length (SDnCBL) - Represents the size of the ring buffer. */
    uint32_t                    u32CBL;
    /** Last Valid Index (SDnLVI). */
    uint16_t                    u16LVI;
    /** Format (SDnFMT). */
    uint16_t                    u16FMT;
    uint8_t                     abPadding[4];
    /** DMA base address (SDnBDPU - SDnBDPL). */
    uint64_t                    u64BDLBase;
    /** @} */

    /** The timer for pumping data thru the attached LUN drivers. */
    TMTIMERHANDLE               hTimer;

#ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
    /** Pad the structure size to a 64 byte alignment. */
    uint64_t                    au64Padding1[4];
    /** Critical section for serialize access to the stream state between the async
     * I/O thread and (basically) the guest. */
    PDMCRITSECT                 CritSect;
#endif
} HDASTREAM;
AssertCompileMemberAlignment(HDASTREAM, State.aBdl, 16);
AssertCompileMemberAlignment(HDASTREAM, State.aSchedule, 16);
AssertCompileSizeAlignment(HDASTREAM, 64);
/** Pointer to an HDA stream (SDI / SDO).  */
typedef HDASTREAM *PHDASTREAM;


/**
 * An HDA stream (SDI / SDO) - ring-3 bits.
 */
typedef struct HDASTREAMR3
{
    /** Stream descriptor number (SDn). */
    uint8_t                     u8SD;
    uint8_t                     abPadding[7];
    /** The shared state for the parent HDA device. */
    R3PTRTYPE(PHDASTATE)        pHDAStateShared;
    /** The ring-3 state for the parent HDA device. */
    R3PTRTYPE(PHDASTATER3)      pHDAStateR3;
    /** Pointer to HDA sink this stream is attached to. */
    R3PTRTYPE(PHDAMIXERSINK)    pMixSink;
    /** Internal state of this stream. */
    struct
    {
        /** This stream's data mapping. */
        HDASTREAMMAP            Mapping;
        /** Circular buffer (FIFO) for holding DMA'ed data. */
        R3PTRTYPE(PRTCIRCBUF)   pCircBuf;
        /** Current circular buffer read offset (for tracing & logging). */
        uint64_t                offRead;
        /** Current circular buffer write offset (for tracing & logging). */
        uint64_t                offWrite;
#ifdef HDA_USE_DMA_ACCESS_HANDLER
        /** List of DMA handlers. */
        RTLISTANCHORR3          lstDMAHandlers;
#endif
#ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
        /** Asynchronous I/O state members. */
        HDASTREAMSTATEAIO       AIO;
#endif
        /** Counter for all under/overflows problems. */
        STAMCOUNTER             StatDmaFlowProblems;
        /** Counter for unresovled under/overflows problems. */
        STAMCOUNTER             StatDmaFlowErrors;
        /** Number of bytes involved in unresolved flow errors. */
        STAMCOUNTER             StatDmaFlowErrorBytes;
    } State;
    /** Debug bits. */
    HDASTREAMDEBUG              Dbg;
    uint64_t                    au64Alignment[2];
} HDASTREAMR3;
AssertCompileSizeAlignment(HDASTREAMR3, 64);
/** Pointer to an HDA stream (SDI / SDO).  */
typedef HDASTREAMR3 *PHDASTREAMR3;

/** @name Stream functions (shared).
 * @{
 */
void                hdaStreamLock(PHDASTREAM pStreamShared);
void                hdaStreamUnlock(PHDASTREAM pStreamShared);
/** @} */

#ifdef IN_RING3

/** @name Stream functions (ring-3).
 * @{
 */
int                 hdaR3StreamConstruct(PHDASTREAM pStreamShared, PHDASTREAMR3 pStreamR3, PHDASTATE pThis,
                                         PHDASTATER3 pThisCC, uint8_t uSD);
void                hdaR3StreamDestroy(PHDASTREAM pStreamShared, PHDASTREAMR3 pStreamR3);
int                 hdaR3StreamSetUp(PPDMDEVINS pDevIns, PHDASTATE pThis, PHDASTREAM pStreamShared,
                                     PHDASTREAMR3 pStreamR3, uint8_t uSD);
void                hdaR3StreamReset(PHDASTATE pThis, PHDASTATER3 pThisCC,
                                     PHDASTREAM pStreamShared, PHDASTREAMR3 pStreamR3, uint8_t uSD);
int                 hdaR3StreamEnable(PHDASTREAM pStreamShared, PHDASTREAMR3 pStreamR3, bool fEnable);
void                hdaR3StreamMarkStarted(PPDMDEVINS pDevIns, PHDASTATE pThis, PHDASTREAM pStreamShared, uint64_t tsNow);
void                hdaR3StreamMarkStopped(PHDASTREAM pStreamShared);

void                hdaR3StreamSetPositionAdd(PHDASTREAM pStreamShared, PPDMDEVINS pDevIns, PHDASTATE pThis, uint32_t uToAdd);
uint64_t            hdaR3StreamTimerMain(PPDMDEVINS pDevIns, PHDASTATE pThis, PHDASTATER3 pThisCC,
                                         PHDASTREAM pStreamShared, PHDASTREAMR3 pStreamR3);
void                hdaR3StreamUpdate(PPDMDEVINS pDevIns, PHDASTATE pThis, PHDASTATER3 pThisCC,
                                      PHDASTREAM pStreamShared, PHDASTREAMR3 pStreamR3, bool fInTimer);
PHDASTREAM          hdaR3StreamR3ToShared(PHDASTREAMR3 pStreamCC);
# ifdef HDA_USE_DMA_ACCESS_HANDLER
bool                hdaR3StreamRegisterDMAHandlers(PHDASTREAM pStream);
void                hdaR3StreamUnregisterDMAHandlers(PHDASTREAM pStream);
# endif
/** @} */

/** @name Async I/O stream functions (ring-3).
 * @{
 */
# ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
int                 hdaR3StreamAsyncIOCreate(PHDASTREAMR3 pStreamR3);
void                hdaR3StreamAsyncIOLock(PHDASTREAMR3 pStreamR3);
void                hdaR3StreamAsyncIOUnlock(PHDASTREAMR3 pStreamR3);
void                hdaR3StreamAsyncIOEnable(PHDASTREAMR3 pStreamR3, bool fEnable);
int                 hdaR3StreamAsyncIONotify(PHDASTREAMR3 pStreamR3);
# endif /* VBOX_WITH_AUDIO_HDA_ASYNC_IO */
/** @} */

#endif /* IN_RING3 */
#endif /* !VBOX_INCLUDED_SRC_Audio_DevHdaStream_h */

