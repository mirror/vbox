/* $Id$ */
/** @file
 * DevHDA.h - VBox Intel HD Audio Controller.
 */

/*
 * Copyright (C) 2016-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_INCLUDED_SRC_Audio_DevHDA_h
#define VBOX_INCLUDED_SRC_Audio_DevHDA_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/path.h>

#include <VBox/vmm/pdmdev.h>

#include "AudioMixer.h"

#include "HDACodec.h"
#include "HDAStream.h"
#include "HDAStreamMap.h"
#include "HDAStreamPeriod.h"



/**
 * HDA mixer sink definition (ring-3).
 *
 * Its purpose is to know which audio mixer sink is bound to which SDn
 * (SDI/SDO) device stream.
 *
 * This is needed in order to handle interleaved streams (that is, multiple
 * channels in one stream) or non-interleaved streams (each channel has a
 * dedicated stream).
 *
 * This is only known to the actual device emulation level.
 */
typedef struct HDAMIXERSINK
{
    R3PTRTYPE(PHDASTREAM)   pStreamShared;
    R3PTRTYPE(PHDASTREAMR3) pStreamR3;
    /** Pointer to the actual audio mixer sink. */
    R3PTRTYPE(PAUDMIXSINK)  pMixSink;
} HDAMIXERSINK;
/** Pointer to an HDA mixer sink definition (ring-3). */
typedef HDAMIXERSINK *PHDAMIXERSINK;

/**
 * Mapping a stream tag to an HDA stream (ring-3).
 */
typedef struct HDATAG
{
    /** Own stream tag. */
    uint8_t                 uTag;
    uint8_t                 Padding[7];
    /** Pointer to associated stream. */
    R3PTRTYPE(PHDASTREAMR3) pStreamR3;
} HDATAG;
/** Pointer to a HDA stream tag mapping. */
typedef HDATAG *PHDATAG;

/**
 * Shared ICH Intel HD audio controller state.
 */
typedef struct HDASTATE
{
    /** Critical section protecting the HDA state. */
    PDMCRITSECT             CritSect;
    /** The HDA's register set. */
    uint32_t                au32Regs[HDA_NUM_REGS];
    /** Internal stream states. */
    HDASTREAM               aStreams[HDA_MAX_STREAMS];
    /** CORB buffer base address. */
    uint64_t                u64CORBBase;
    /** RIRB buffer base address. */
    uint64_t                u64RIRBBase;
    /** DMA base address.
     *  Made out of DPLBASE + DPUBASE (3.3.32 + 3.3.33). */
    uint64_t                u64DPBase;
    /** Size in bytes of CORB buffer (#au32CorbBuf). */
    uint32_t                cbCorbBuf;
    /** Size in bytes of RIRB buffer (#au64RirbBuf). */
    uint32_t                cbRirbBuf;
    /** Response Interrupt Count (RINTCNT). */
    uint16_t                u16RespIntCnt;
    /** Position adjustment (in audio frames).
     *
     *  This is not an official feature of the HDA specs, but used by
     *  certain OS drivers (e.g. snd_hda_intel) to work around certain
     *  quirks by "real" HDA hardware implementations.
     *
     *  The position adjustment specifies how many audio frames
     *  a stream is ahead from its actual reading/writing position when
     *  starting a stream.
     */
    uint16_t                cPosAdjustFrames;
    /** Whether the position adjustment is enabled or not. */
    bool                    fPosAdjustEnabled;
    /** DMA position buffer enable bit. */
    bool                    fDMAPosition;
    /** Current IRQ level. */
    uint8_t                 u8IRQL;
#ifdef VBOX_STRICT
    /** Wall clock (WALCLK) stale count.
     *  This indicates the number of set wall clock values which did not actually
     *  move the counter forward (stale). */
    uint8_t                 u8WalClkStaleCnt;
#else
    uint8_t                 bPadding1;
#endif
    /** The device timer Hz rate. Defaults to HDA_TIMER_HZ_DEFAULT. */
    uint16_t                uTimerHz;
    /** Padding for alignment. */
    uint16_t                au16Padding3[3];
    /** Last updated wall clock (WALCLK) counter. */
    uint64_t                u64WalClk;
    /** The CORB buffer. */
    uint32_t                au32CorbBuf[HDA_CORB_SIZE];
    /** Pointer to RIRB buffer. */
    uint64_t                au64RirbBuf[HDA_RIRB_SIZE];

    /** PCI Region \#0: 16KB of MMIO stuff. */
    IOMMMIOHANDLE           hMmio;

#ifdef VBOX_WITH_STATISTICS
    STAMPROFILE             StatIn;
    STAMPROFILE             StatOut;
    STAMCOUNTER             StatBytesRead;
    STAMCOUNTER             StatBytesWritten;

    /** @name Register statistics.
     * The array members run parallel to g_aHdaRegMap.
     * @{ */
    STAMCOUNTER             aStatRegReads[HDA_NUM_REGS];
    STAMCOUNTER             aStatRegReadsToR3[HDA_NUM_REGS];
    STAMCOUNTER             aStatRegWrites[HDA_NUM_REGS];
    STAMCOUNTER             aStatRegWritesToR3[HDA_NUM_REGS];
    STAMCOUNTER             StatRegMultiReadsRZ;
    STAMCOUNTER             StatRegMultiReadsR3;
    STAMCOUNTER             StatRegMultiWritesRZ;
    STAMCOUNTER             StatRegMultiWritesR3;
    STAMCOUNTER             StatRegSubWriteRZ;
    STAMCOUNTER             StatRegSubWriteR3;
    STAMCOUNTER             StatRegUnknownReads;
    STAMCOUNTER             StatRegUnknownWrites;
    STAMCOUNTER             StatRegWritesBlockedByReset;
    STAMCOUNTER             StatRegWritesBlockedByRun;
    /** @} */
#endif

#ifdef DEBUG
    /** Debug stuff.
     * @todo Make STAM values out some of this? */
    struct
    {
# if 0 /* unused */
        /** Timestamp (in ns) of the last timer callback (hdaTimer).
         * Used to calculate the time actually elapsed between two timer callbacks. */
        uint64_t                tsTimerLastCalledNs;
# endif
        /** IRQ debugging information. */
        struct
        {
            /** Timestamp (in ns) of last processed (asserted / deasserted) IRQ. */
            uint64_t            tsProcessedLastNs;
            /** Timestamp (in ns) of last asserted IRQ. */
            uint64_t            tsAssertedNs;
# if 0 /* unused */
            /** How many IRQs have been asserted already. */
            uint64_t            cAsserted;
            /** Accumulated elapsed time (in ns) of all IRQ being asserted. */
            uint64_t            tsAssertedTotalNs;
            /** Timestamp (in ns) of last deasserted IRQ. */
            uint64_t            tsDeassertedNs;
            /** How many IRQs have been deasserted already. */
            uint64_t            cDeasserted;
            /** Accumulated elapsed time (in ns) of all IRQ being deasserted. */
            uint64_t            tsDeassertedTotalNs;
# endif
        } IRQ;
    } Dbg;
#endif
    /** This is for checking that the build was correctly configured in all contexts.
     *  This is set to HDASTATE_ALIGNMENT_CHECK_MAGIC. */
    uint64_t                uAlignmentCheckMagic;
} HDASTATE;
/** Pointer to a shared HDA device state.  */
typedef HDASTATE *PHDASTATE;

/** Value for HDASTATE:uAlignmentCheckMagic. */
#define HDASTATE_ALIGNMENT_CHECK_MAGIC  UINT64_C(0x1298afb75893e059)


/**
 * Ring-3 ICH Intel HD audio controller state.
 */
typedef struct HDASTATER3
{
    /** Internal stream states. */
    HDASTREAMR3             aStreams[HDA_MAX_STREAMS];
    /** Mapping table between stream tags and stream states. */
    HDATAG                  aTags[HDA_MAX_TAGS];
    /** Number of active (running) SDn streams. */
    uint8_t                 cStreamsActive;
    uint8_t                 abPadding0[7];
    /** R3 Pointer to the device instance. */
    PPDMDEVINSR3            pDevIns;
    /** The base interface for LUN\#0. */
    PDMIBASE                IBase;
    /** Pointer to HDA codec to use. */
    R3PTRTYPE(PHDACODEC)    pCodec;
    /** List of associated LUN drivers (HDADRIVER). */
    RTLISTANCHORR3          lstDrv;
    /** The device' software mixer. */
    R3PTRTYPE(PAUDIOMIXER)  pMixer;
    /** HDA sink for (front) output. */
    HDAMIXERSINK            SinkFront;
#ifdef VBOX_WITH_AUDIO_HDA_51_SURROUND
    /** HDA sink for center / LFE output. */
    HDAMIXERSINK            SinkCenterLFE;
    /** HDA sink for rear output. */
    HDAMIXERSINK            SinkRear;
#endif
    /** HDA mixer sink for line input. */
    HDAMIXERSINK            SinkLineIn;
#ifdef VBOX_WITH_AUDIO_HDA_MIC_IN
    /** Audio mixer sink for microphone input. */
    HDAMIXERSINK            SinkMicIn;
#endif
    /** Debug stuff. */
    struct
    {
        /** Whether debugging is enabled or not. */
        bool                    fEnabled;
        /** Path where to dump the debug output to.
         *  Defaults to VBOX_AUDIO_DEBUG_DUMP_PCM_DATA_PATH. */
        R3PTRTYPE(char *)       pszOutPath;
    } Dbg;
} HDASTATER3;
/** Pointer to a ring-3 HDA device state.  */
typedef HDASTATER3 *PHDASTATER3;


#endif /* !VBOX_INCLUDED_SRC_Audio_DevHDA_h */

