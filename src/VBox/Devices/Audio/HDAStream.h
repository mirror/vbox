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

#ifndef VBOX_INCLUDED_SRC_Audio_HDAStream_h
#define VBOX_INCLUDED_SRC_Audio_HDAStream_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "DevHDACommon.h"
#include "HDAStreamMap.h"
#include "HDAStreamPeriod.h"


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
#endif
    /** Runtime debug info. */
    HDASTREAMDEBUGRT        Runtime;
} HDASTREAMDEBUG;
typedef HDASTREAMDEBUG *PHDASTREAMDEBUG;

/**
 * Internal state of a HDA stream.
 */
typedef struct HDASTREAMSTATE
{
    /** Current BDLE to use. Wraps around to 0 if
     *  maximum (cBDLE) is reached. */
    uint16_t                uCurBDLE;
    /** Flag indicating whether this stream currently is
     *  in reset mode and therefore not acccessible by the guest. */
    volatile bool           fInReset;
    /** Flag indicating if the stream is in running state or not. */
    volatile bool           fRunning;
    /** Unused, padding. */
    uint8_t                 abPadding0[4];
    /** Current BDLE (Buffer Descriptor List Entry). */
    HDABDLE                 BDLE;
    /** Timestamp (absolute, in timer ticks) of the last DMA data transfer. */
    uint64_t                tsTransferLast;
    /** Timestamp (absolute, in timer ticks) of the next DMA data transfer.
     *  Next for determining the next scheduling window.
     *  Can be 0 if no next transfer is scheduled. */
    uint64_t                tsTransferNext;
    /** Total transfer size (in bytes) of a transfer period. */
    uint32_t                cbTransferSize;
    /** Transfer chunk size (in bytes) of a transfer period. */
    uint32_t                cbTransferChunk;
    /** How many interrupts are pending due to
     *  BDLE interrupt-on-completion (IOC) bits set. */
    uint8_t                 cTransferPendingInterrupts;
    uint8_t                 abPadding2[7];
    /** The stream's I/O timer Hz rate. */
    uint16_t                uTimerIoHz;
    /** Number of audio data frames for the position adjustment.
     *  0 if no position adjustment is needed. */
    uint16_t                cfPosAdjustDefault;
    /** How many audio data frames are left to be processed
     *  for the position adjustment handling.
     *
     *  0 if position adjustment handling is done or inactive. */
    uint16_t                cfPosAdjustLeft;
    uint16_t                u16Padding3;
    /** (Virtual) clock ticks per byte. */
    uint64_t                cTicksPerByte;
    /** (Virtual) clock ticks per transfer. */
    uint64_t                cTransferTicks;
    /** The stream's period. Need for timing. */
    HDASTREAMPERIOD         Period;
    /** The stream's current configuration.
     *  Should match SDFMT. */
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
} HDASTREAMSTATE;
AssertCompileSizeAlignment(HDASTREAMSTATE, 8);

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
    /** Stream descriptor number (SDn). */
    uint8_t                     u8SD;
    /** Current channel index.
     *  For a stereo stream, this is u8Channel + 1. */
    uint8_t                     u8Channel;
    uint8_t                     abPadding0[6];
#ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
    /** The stream's shared r0/r3 critical section to serialize access between the async I/O
     *  thread and (basically) the guest. */
    PDMCRITSECT                 CritSect;
#endif
    /** DMA base address (SDnBDPU - SDnBDPL).
     *  Will be updated in hdaR3StreamInit(). */
    uint64_t                    u64BDLBase;
    /** Cyclic Buffer Length (SDnCBL).
     *  Represents the size of the ring buffer.
     *  Will be updated in hdaR3StreamInit(). */
    uint32_t                    u32CBL;
    /** Format (SDnFMT).
     *  Will be updated in hdaR3StreamInit(). */
    uint16_t                    u16FMT;
    /** FIFO Size (checked + translated in bytes, FIFOS).
     *  Maximum number of bytes that may have been DMA'd into
     *  memory but not yet transmitted on the link.
     *
     *  Will be updated in hdaR3StreamInit(). */
    uint8_t                     u8FIFOS;
    /** FIFO Watermark (checked + translated in bytes, FIFOW). */
    uint8_t                     u8FIFOW;
    uint8_t                     abPadding1[2];
    /** FIFO scratch buffer, to avoid intermediate (re-)allocations. */
    uint8_t                     abFIFO[HDA_FIFO_MAX + 1];
    /** Last Valid Index (SDnLVI).
     *  Will be updated in hdaR3StreamInit(). */
    uint16_t                    u16LVI;
    /** The timer for pumping data thru the attached LUN drivers. */
    TMTIMERHANDLE               hTimer;
    /** Internal state of this stream. */
    HDASTREAMSTATE              State;
} HDASTREAM;
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
        /** Under/overflow statistics counter.   */
        STAMCOUNTER             StatDmaFlowErrors;
    } State;
    /** Debug bits. */
    HDASTREAMDEBUG              Dbg;
} HDASTREAMR3;
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
bool                hdaR3StreamTransferIsScheduled(PHDASTREAM pStreamShared, uint64_t tsNow);
uint64_t            hdaR3StreamTransferGetNext(PHDASTREAM pStreamShared);
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
# endif /* VBOX_WITH_AUDIO_HDA_ASYNC_IO */
/** @} */

#endif /* IN_RING3 */
#endif /* !VBOX_INCLUDED_SRC_Audio_HDAStream_h */

