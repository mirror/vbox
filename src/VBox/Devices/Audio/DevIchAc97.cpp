/* $Id$ */
/** @file
 * DevIchAc97 - VBox ICH AC97 Audio Controller.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DEV_AC97
#include <VBox/log.h>
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pdmaudioifs.h>

#include <iprt/assert.h>
#ifdef IN_RING3
# ifdef DEBUG
#  include <iprt/file.h>
# endif
# include <iprt/mem.h>
# include <iprt/semaphore.h>
# include <iprt/string.h>
# include <iprt/uuid.h>
#endif

#include "VBoxDD.h"

#include "AudioMixBuffer.h"
#include "AudioMixer.h"
#include "DrvAudio.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/

/** Current saved state version. */
#define AC97_SAVED_STATE_VERSION 1

/** Default timer frequency (in Hz). */
#define AC97_TIMER_HZ_DEFAULT   100

/** Maximum number of streams we support. */
#define AC97_MAX_STREAMS        3

/** Maximum FIFO size (in bytes). */
#define AC97_FIFO_MAX           256

#define AC97_SR_FIFOE           RT_BIT(4)           /**< rwc, FIFO error. */
#define AC97_SR_BCIS            RT_BIT(3)           /**< rwc, Buffer completion interrupt status. */
#define AC97_SR_LVBCI           RT_BIT(2)           /**< rwc, Last valid buffer completion interrupt. */
#define AC97_SR_CELV            RT_BIT(1)           /**< ro,  Current equals last valid. */
#define AC97_SR_DCH             RT_BIT(0)           /**< ro,  Controller halted. */
#define AC97_SR_VALID_MASK      (RT_BIT(5) - 1)
#define AC97_SR_WCLEAR_MASK     (AC97_SR_FIFOE | AC97_SR_BCIS | AC97_SR_LVBCI)
#define AC97_SR_RO_MASK         (AC97_SR_DCH | AC97_SR_CELV)
#define AC97_SR_INT_MASK        (AC97_SR_FIFOE | AC97_SR_BCIS | AC97_SR_LVBCI)

#define AC97_CR_IOCE            RT_BIT(4)           /**< rw,   Interrupt On Completion Enable. */
#define AC97_CR_FEIE            RT_BIT(3)           /**< rw    FIFO Error Interrupt Enable. */
#define AC97_CR_LVBIE           RT_BIT(2)           /**< rw    Last Valid Buffer Interrupt Enable. */
#define AC97_CR_RR              RT_BIT(1)           /**< rw    Reset Registers. */
#define AC97_CR_RPBM            RT_BIT(0)           /**< rw    Run/Pause Bus Master. */
#define AC97_CR_VALID_MASK      (RT_BIT(5) - 1)
#define AC97_CR_DONT_CLEAR_MASK (AC97_CR_IOCE | AC97_CR_FEIE | AC97_CR_LVBIE)

#define AC97_GC_WR              4                   /**< rw    Warm reset. */
#define AC97_GC_CR              2                   /**< rw    Cold reset. */
#define AC97_GC_VALID_MASK      (RT_BIT(6) - 1)

#define AC97_GS_MD3             RT_BIT(17)          /**< rw */
#define AC97_GS_AD3             RT_BIT(16)          /**< rw */
#define AC97_GS_RCS             RT_BIT(15)          /**< rwc */
#define AC97_GS_B3S12           RT_BIT(14)          /**< ro */
#define AC97_GS_B2S12           RT_BIT(13)          /**< ro */
#define AC97_GS_B1S12           RT_BIT(12)          /**< ro */
#define AC97_GS_S1R1            RT_BIT(11)          /**< rwc */
#define AC97_GS_S0R1            RT_BIT(10)          /**< rwc */
#define AC97_GS_S1CR            RT_BIT(9)           /**< ro */
#define AC97_GS_S0CR            RT_BIT(8)           /**< ro */
#define AC97_GS_MINT            RT_BIT(7)           /**< ro */
#define AC97_GS_POINT           RT_BIT(6)           /**< ro */
#define AC97_GS_PIINT           RT_BIT(5)           /**< ro */
#define AC97_GS_RSRVD           (RT_BIT(4) | RT_BIT(3))
#define AC97_GS_MOINT           RT_BIT(2)           /**< ro */
#define AC97_GS_MIINT           RT_BIT(1)           /**< ro */
#define AC97_GS_GSCI            RT_BIT(0)           /**< rwc */
#define AC97_GS_RO_MASK         (  AC97_GS_B3S12 \
                                 | AC97_GS_B2S12 \
                                 | AC97_GS_B1S12 \
                                 | AC97_GS_S1CR \
                                 | AC97_GS_S0CR \
                                 | AC97_GS_MINT \
                                 | AC97_GS_POINT \
                                 | AC97_GS_PIINT \
                                 | AC97_GS_RSRVD \
                                 | AC97_GS_MOINT \
                                 | AC97_GS_MIINT)
#define AC97_GS_VALID_MASK      (RT_BIT(18) - 1)
#define AC97_GS_WCLEAR_MASK     (AC97_GS_RCS | AC97_GS_S1R1 | AC97_GS_S0R1 | AC97_GS_GSCI)

/** @name Buffer Descriptor (BD).
 * @{ */
#define AC97_BD_IOC             RT_BIT(31)          /**< Interrupt on Completion. */
#define AC97_BD_BUP             RT_BIT(30)          /**< Buffer Underrun Policy. */

#define AC97_BD_LEN_MASK        0xFFFF              /**< Mask for the BDL buffer length. */

#define AC97_MAX_BDLE           32                  /**< Maximum number of BDLEs. */
/** @} */

/** @name Extended Audio ID Register (EAID).
 * @{ */
#define AC97_EAID_VRA           RT_BIT(0)           /**< Variable Rate Audio. */
#define AC97_EAID_VRM           RT_BIT(3)           /**< Variable Rate Mic Audio. */
#define AC97_EAID_REV0          RT_BIT(10)          /**< AC'97 revision compliance. */
#define AC97_EAID_REV1          RT_BIT(11)          /**< AC'97 revision compliance. */
/** @} */

/** @name Extended Audio Control and Status Register (EACS).
 * @{ */
#define AC97_EACS_VRA           RT_BIT(0)           /**< Variable Rate Audio (4.2.1.1). */
#define AC97_EACS_VRM           RT_BIT(3)           /**< Variable Rate Mic Audio (4.2.1.1). */
/** @} */

/** @name Baseline Audio Register Set (BARS).
 * @{ */
#define AC97_BARS_VOL_MASK      0x1f                /**< Volume mask for the Baseline Audio Register Set (5.7.2). */
#define AC97_BARS_GAIN_MASK     0x0f                /**< Gain mask for the Baseline Audio Register Set. */
#define AC97_BARS_VOL_MUTE_SHIFT 15                 /**< Mute bit shift for the Baseline Audio Register Set (5.7.2). */
/** @} */

/** AC'97 uses 1.5dB steps, we use 0.375dB steps: 1 AC'97 step equals 4 PDM steps. */
#define AC97_DB_FACTOR          4

/** @name Recording inputs?
 * @{ */
#define AC97_REC_MIC            UINT8_C(0)
#define AC97_REC_CD             UINT8_C(1)
#define AC97_REC_VIDEO          UINT8_C(2)
#define AC97_REC_AUX            UINT8_C(3)
#define AC97_REC_LINE_IN        UINT8_C(4)
#define AC97_REC_STEREO_MIX     UINT8_C(5)
#define AC97_REC_MONO_MIX       UINT8_C(6)
#define AC97_REC_PHONE          UINT8_C(7)
#define AC97_REC_MASK           UINT8_C(7)
/** @} */

/** @name Mixer registers / NAM BAR registers?
 * @{ */
#define AC97_Reset                      0x00
#define AC97_Master_Volume_Mute         0x02
#define AC97_Headphone_Volume_Mute      0x04        /**< Also known as AUX, see table 16, section 5.7. */
#define AC97_Master_Volume_Mono_Mute    0x06
#define AC97_Master_Tone_RL             0x08
#define AC97_PC_BEEP_Volume_Mute        0x0a
#define AC97_Phone_Volume_Mute          0x0c
#define AC97_Mic_Volume_Mute            0x0e
#define AC97_Line_In_Volume_Mute        0x10
#define AC97_CD_Volume_Mute             0x12
#define AC97_Video_Volume_Mute          0x14
#define AC97_Aux_Volume_Mute            0x16
#define AC97_PCM_Out_Volume_Mute        0x18
#define AC97_Record_Select              0x1a
#define AC97_Record_Gain_Mute           0x1c
#define AC97_Record_Gain_Mic_Mute       0x1e
#define AC97_General_Purpose            0x20
#define AC97_3D_Control                 0x22
#define AC97_AC_97_RESERVED             0x24
#define AC97_Powerdown_Ctrl_Stat        0x26
#define AC97_Extended_Audio_ID          0x28
#define AC97_Extended_Audio_Ctrl_Stat   0x2a
#define AC97_PCM_Front_DAC_Rate         0x2c
#define AC97_PCM_Surround_DAC_Rate      0x2e
#define AC97_PCM_LFE_DAC_Rate           0x30
#define AC97_PCM_LR_ADC_Rate            0x32
#define AC97_MIC_ADC_Rate               0x34
#define AC97_6Ch_Vol_C_LFE_Mute         0x36
#define AC97_6Ch_Vol_L_R_Surround_Mute  0x38
#define AC97_Vendor_Reserved            0x58
#define AC97_AD_Misc                    0x76
#define AC97_Vendor_ID1                 0x7c
#define AC97_Vendor_ID2                 0x7e
/** @} */

/** @name Analog Devices miscellaneous regiter bits used in AD1980.
 * @{  */
#define AC97_AD_MISC_LOSEL              RT_BIT(5)   /**< Surround (rear) goes to line out outputs. */
#define AC97_AD_MISC_HPSEL              RT_BIT(10)  /**< PCM (front) goes to headphone outputs. */
/** @} */


/** @name BUP flag values.
 * @{ */
#define BUP_SET                         RT_BIT_32(0)
#define BUP_LAST                        RT_BIT_32(1)
/** @}   */

/** @name AC'97 source indices.
 * @note The order of these indices is fixed (also applies for saved states) for
 *       the moment.  So make sure you know what you're done when altering this!
 * @{
 */
#define AC97SOUNDSOURCE_PI_INDEX        0           /**< PCM in */
#define AC97SOUNDSOURCE_PO_INDEX        1           /**< PCM out */
#define AC97SOUNDSOURCE_MC_INDEX        2           /**< Mic in */
#define AC97SOUNDSOURCE_MAX             3           /**< Max sound sources. */
/** @} */

/** Port number (offset into NABM BAR) to stream index. */
#define AC97_PORT2IDX(a_idx)            ( ((a_idx) >> 4) & 3 )
/** Port number (offset into NABM BAR) to stream index, but no masking. */
#define AC97_PORT2IDX_UNMASKED(a_idx)   ( ((a_idx) >> 4) )

/** @name Stream offsets
 * @{ */
#define AC97_NABM_OFF_BDBAR             0x0         /**< Buffer Descriptor Base Address */
#define AC97_NABM_OFF_CIV               0x4         /**< Current Index Value */
#define AC97_NABM_OFF_LVI               0x5         /**< Last Valid Index */
#define AC97_NABM_OFF_SR                0x6         /**< Status Register */
#define AC97_NABM_OFF_PICB              0x8         /**< Position in Current Buffer */
#define AC97_NABM_OFF_PIV               0xa         /**< Prefetched Index Value */
#define AC97_NABM_OFF_CR                0xb         /**< Control Register */
#define AC97_NABM_OFF_MASK              0xf         /**< Mask for getting the the per-stream register. */
/** @} */


/** @name PCM in NABM BAR registers (0x00..0x0f).
 * @{ */
#define PI_BDBAR (AC97SOUNDSOURCE_PI_INDEX * 0x10 + 0x0) /**< PCM in: Buffer Descriptor Base Address */
#define PI_CIV   (AC97SOUNDSOURCE_PI_INDEX * 0x10 + 0x4) /**< PCM in: Current Index Value */
#define PI_LVI   (AC97SOUNDSOURCE_PI_INDEX * 0x10 + 0x5) /**< PCM in: Last Valid Index */
#define PI_SR    (AC97SOUNDSOURCE_PI_INDEX * 0x10 + 0x6) /**< PCM in: Status Register */
#define PI_PICB  (AC97SOUNDSOURCE_PI_INDEX * 0x10 + 0x8) /**< PCM in: Position in Current Buffer */
#define PI_PIV   (AC97SOUNDSOURCE_PI_INDEX * 0x10 + 0xa) /**< PCM in: Prefetched Index Value */
#define PI_CR    (AC97SOUNDSOURCE_PI_INDEX * 0x10 + 0xb) /**< PCM in: Control Register */
/** @} */

/** @name PCM out NABM BAR registers (0x10..0x1f).
 * @{ */
#define PO_BDBAR (AC97SOUNDSOURCE_PO_INDEX * 0x10 + 0x0) /**< PCM out: Buffer Descriptor Base Address */
#define PO_CIV   (AC97SOUNDSOURCE_PO_INDEX * 0x10 + 0x4) /**< PCM out: Current Index Value */
#define PO_LVI   (AC97SOUNDSOURCE_PO_INDEX * 0x10 + 0x5) /**< PCM out: Last Valid Index */
#define PO_SR    (AC97SOUNDSOURCE_PO_INDEX * 0x10 + 0x6) /**< PCM out: Status Register */
#define PO_PICB  (AC97SOUNDSOURCE_PO_INDEX * 0x10 + 0x8) /**< PCM out: Position in Current Buffer */
#define PO_PIV   (AC97SOUNDSOURCE_PO_INDEX * 0x10 + 0xa) /**< PCM out: Prefetched Index Value */
#define PO_CR    (AC97SOUNDSOURCE_PO_INDEX * 0x10 + 0xb) /**< PCM out: Control Register */
/** @} */

/** @name Mic in NABM BAR registers (0x20..0x2f).
 * @{ */
#define MC_BDBAR (AC97SOUNDSOURCE_MC_INDEX * 0x10 + 0x0) /**< PCM in: Buffer Descriptor Base Address */
#define MC_CIV   (AC97SOUNDSOURCE_MC_INDEX * 0x10 + 0x4) /**< PCM in: Current Index Value */
#define MC_LVI   (AC97SOUNDSOURCE_MC_INDEX * 0x10 + 0x5) /**< PCM in: Last Valid Index */
#define MC_SR    (AC97SOUNDSOURCE_MC_INDEX * 0x10 + 0x6) /**< PCM in: Status Register */
#define MC_PICB  (AC97SOUNDSOURCE_MC_INDEX * 0x10 + 0x8) /**< PCM in: Position in Current Buffer */
#define MC_PIV   (AC97SOUNDSOURCE_MC_INDEX * 0x10 + 0xa) /**< PCM in: Prefetched Index Value */
#define MC_CR    (AC97SOUNDSOURCE_MC_INDEX * 0x10 + 0xb) /**< PCM in: Control Register */
/** @} */

/** @name Misc NABM BAR registers.
 * @{  */
/** NABMBAR: Global Control Register.
 * @note This is kind of in the MIC IN area.  */
#define AC97_GLOB_CNT                   0x2c
/** NABMBAR: Global Status. */
#define AC97_GLOB_STA                   0x30
/** Codec Access Semaphore Register. */
#define AC97_CAS                        0x34
/** @} */


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/** The ICH AC'97 (Intel) controller (shared). */
typedef struct AC97STATE *PAC97STATE;
/** The ICH AC'97 (Intel) controller (ring-3). */
typedef struct AC97STATER3 *PAC97STATER3;

/**
 * Buffer Descriptor List Entry (BDLE).
 */
typedef struct AC97BDLE
{
    /** Location of data buffer (bits 31:1). */
    uint32_t                addr;
    /** Flags (bits 31 + 30) and length (bits 15:0) of data buffer (in audio samples). */
    uint32_t                ctl_len;
} AC97BDLE;
AssertCompileSize(AC97BDLE, 8);
/** Pointer to BDLE. */
typedef AC97BDLE *PAC97BDLE;

/**
 * Bus master register set for an audio stream.
 */
typedef struct AC97BMREGS
{
    uint32_t                bdbar;      /**< rw 0, Buffer Descriptor List: BAR (Base Address Register). */
    uint8_t                 civ;        /**< ro 0, Current index value. */
    uint8_t                 lvi;        /**< rw 0, Last valid index. */
    uint16_t                sr;         /**< rw 1, Status register. */
    uint16_t                picb;       /**< ro 0, Position in current buffer (in samples). */
    uint8_t                 piv;        /**< ro 0, Prefetched index value. */
    uint8_t                 cr;         /**< rw 0, Control register. */
    int32_t                 bd_valid;   /**< Whether current BDLE is initialized or not. */
    AC97BDLE                bd;         /**< Current Buffer Descriptor List Entry (BDLE). */
} AC97BMREGS;
AssertCompileSizeAlignment(AC97BMREGS, 8);
/** Pointer to the BM registers of an audio stream. */
typedef AC97BMREGS *PAC97BMREGS;

#ifdef VBOX_WITH_AUDIO_AC97_ASYNC_IO
/**
 * Asynchronous I/O state for an AC'97 stream.
 */
typedef struct AC97STREAMSTATEAIO
{
    /** Thread handle for the actual I/O thread. */
    RTTHREAD                Thread;
    /** Event for letting the thread know there is some data to process. */
    RTSEMEVENT              Event;
    /** Critical section for synchronizing access. */
    RTCRITSECT              CritSect;
    /** Started indicator. */
    volatile bool           fStarted;
    /** Shutdown indicator. */
    volatile bool           fShutdown;
    /** Whether the thread should do any data processing or not. */
    volatile bool           fEnabled;
    bool                    afPadding[5];
} AC97STREAMSTATEAIO;
/** Pointer to the async I/O state for an AC'97 stream. */
typedef AC97STREAMSTATEAIO *PAC97STREAMSTATEAIO;
#endif


/**
 * The internal state of an AC'97 stream.
 */
typedef struct AC97STREAMSTATE
{
    /** Criticial section for this stream. */
    RTCRITSECT              CritSect;
    /** Circular buffer (FIFO) for holding DMA'ed data. */
    R3PTRTYPE(PRTCIRCBUF)   pCircBuf;
#if HC_ARCH_BITS == 32
    uint32_t                Padding;
#endif
    /** The stream's current configuration. */
    PDMAUDIOSTREAMCFG       Cfg; //+108
#ifdef VBOX_WITH_AUDIO_AC97_ASYNC_IO
    /** Asynchronous I/O state members. */
    AC97STREAMSTATEAIO      AIO;
#endif
    /** Timestamp of the last DMA data transfer. */
    uint64_t                tsTransferLast;
    /** Timestamp of the next DMA data transfer.
     *  Next for determining the next scheduling window.
     *  Can be 0 if no next transfer is scheduled. */
    uint64_t                tsTransferNext;
    /** Transfer chunk size (in bytes) of a transfer period. */
    uint32_t                cbTransferChunk;
    /** The stream's timer Hz rate.
     *  This value can can be different from the device's default Hz rate,
     *  depending on the rate the stream expects (e.g. for 5.1 speaker setups).
     *  Set in R3StreamInit(). */
    uint16_t                uTimerHz;
    uint8_t                 Padding3[2];
    /** (Virtual) clock ticks per transfer. */
    uint64_t                cTransferTicks;
   /** Timestamp (in ns) of last stream update. */
    uint64_t                tsLastUpdateNs;
} AC97STREAMSTATE;
AssertCompileSizeAlignment(AC97STREAMSTATE, 8);
/** Pointer to internal state of an AC'97 stream. */
typedef AC97STREAMSTATE *PAC97STREAMSTATE;

/**
 * Runtime configurable debug stuff for an AC'97 stream.
 */
typedef struct AC97STREAMDEBUGRT
{
    /** Whether debugging is enabled or not. */
    bool                        fEnabled;
    uint8_t                     Padding[7];
    /** File for dumping stream reads / writes.
     *  For input streams, this dumps data being written to the device FIFO,
     *  whereas for output streams this dumps data being read from the device FIFO. */
    R3PTRTYPE(PPDMAUDIOFILE)    pFileStream;
    /** File for dumping DMA reads / writes.
     *  For input streams, this dumps data being written to the device DMA,
     *  whereas for output streams this dumps data being read from the device DMA. */
    R3PTRTYPE(PPDMAUDIOFILE)    pFileDMA;
} AC97STREAMDEBUGRT;

/**
 * Debug stuff for an AC'97 stream.
 */
typedef struct AC97STREAMDEBUG
{
    /** Runtime debug stuff. */
    AC97STREAMDEBUGRT       Runtime;
} AC97STREAMDEBUG;

/**
 * The shared AC'97 stream state.
 */
typedef struct AC97STREAM
{
    /** Stream number (SDn). */
    uint8_t                 u8SD;
    uint8_t                 abPadding0[7];
    /** Bus master registers of this stream. */
    AC97BMREGS              Regs;
    /** The timer for pumping data thru the attached LUN drivers. */
    TMTIMERHANDLE           hTimer;
} AC97STREAM;
AssertCompileSizeAlignment(AC97STREAM, 8);
/** Pointer to a shared AC'97 stream state. */
typedef AC97STREAM *PAC97STREAM;


/**
 * The ring-3 AC'97 stream state.
 */
typedef struct AC97STREAMR3
{
    /** Stream number (SDn). */
    uint8_t                 u8SD;
    uint8_t                 abPadding0[7];
    /** Internal state of this stream. */
    AC97STREAMSTATE         State;
    /** Debug stuff. */
    AC97STREAMDEBUG         Dbg;
} AC97STREAMR3;
AssertCompileSizeAlignment(AC97STREAMR3, 8);
/** Pointer to an AC'97 stream state for ring-3. */
typedef AC97STREAMR3 *PAC97STREAMR3;


#ifdef VBOX_WITH_AUDIO_AC97_ASYNC_IO
/**
 * Asynchronous I/O thread context (arguments).
 */
typedef struct AC97STREAMTHREADCTX
{
    /** The AC'97 device state (shared). */
    PAC97STATE              pThis;
    /** The AC'97 device state (ring-3). */
    PAC97STATER3            pThisCC;
    /** The AC'97 stream state (shared). */
    PAC97STREAM             pStream;
    /** The AC'97 stream state (ring-3). */
    PAC97STREAMR3           pStreamCC;
} AC97STREAMTHREADCTX;
/** Pointer to the context for an async I/O thread. */
typedef AC97STREAMTHREADCTX *PAC97STREAMTHREADCTX;
#endif

/**
 * A driver stream (host backend).
 *
 * Each driver has its own instances of audio mixer streams, which then
 * can go into the same (or even different) audio mixer sinks.
 */
typedef struct AC97DRIVERSTREAM
{
    /** Associated mixer stream handle. */
    R3PTRTYPE(PAUDMIXSTREAM)    pMixStrm;
} AC97DRIVERSTREAM;
/** Pointer to a driver stream. */
typedef AC97DRIVERSTREAM *PAC97DRIVERSTREAM;

/**
 * A host backend driver (LUN).
 */
typedef struct AC97DRIVER
{
    /** Node for storing this driver in our device driver list of AC97STATE. */
    RTLISTNODER3                    Node;
    /** Driver flags. */
    PDMAUDIODRVFLAGS                fFlags;
    /** LUN # to which this driver has been assigned. */
    uint8_t                         uLUN;
    /** Whether this driver is in an attached state or not. */
    bool                            fAttached;
    uint8_t                         abPadding[2];
    /** Pointer to the description string passed to PDMDevHlpDriverAttach(). */
    R3PTRTYPE(char *)               pszDesc;
    /** Pointer to attached driver base interface. */
    R3PTRTYPE(PPDMIBASE)            pDrvBase;
    /** Audio connector interface to the underlying host backend. */
    R3PTRTYPE(PPDMIAUDIOCONNECTOR)  pConnector;
    /** Driver stream for line input. */
    AC97DRIVERSTREAM                LineIn;
    /** Driver stream for mic input. */
    AC97DRIVERSTREAM                MicIn;
    /** Driver stream for output. */
    AC97DRIVERSTREAM                Out;
} AC97DRIVER;
/** Pointer to a host backend driver (LUN). */
typedef AC97DRIVER *PAC97DRIVER;

/**
 * Debug settings.
 */
typedef struct AC97STATEDEBUG
{
    /** Whether debugging is enabled or not. */
    bool                    fEnabled;
    bool                    afAlignment[7];
    /** Path where to dump the debug output to.
     *  Defaults to VBOX_AUDIO_DEBUG_DUMP_PCM_DATA_PATH. */
    R3PTRTYPE(char *)       pszOutPath;
} AC97STATEDEBUG;


/* Codec models. */
typedef enum AC97CODEC
{
    AC97CODEC_INVALID = 0,      /**< Customary illegal zero value. */
    AC97CODEC_STAC9700,         /**< SigmaTel STAC9700 */
    AC97CODEC_AD1980,           /**< Analog Devices AD1980 */
    AC97CODEC_AD1981B,          /**< Analog Devices AD1981B */
    AC97CODEC_32BIT_HACK = 0x7fffffff
} AC97CODEC;


/**
 * The shared AC'97 device state.
 */
typedef struct AC97STATE
{
    /** Critical section protecting the AC'97 state. */
    PDMCRITSECT             CritSect;
    /** Global Control (Bus Master Control Register). */
    uint32_t                glob_cnt;
    /** Global Status (Bus Master Control Register). */
    uint32_t                glob_sta;
    /** Codec Access Semaphore Register (Bus Master Control Register). */
    uint32_t                cas;
    uint32_t                last_samp;
    uint8_t                 mixer_data[256];
    /** Array of AC'97 streams (parallel to AC97STATER3::aStreams). */
    AC97STREAM              aStreams[AC97_MAX_STREAMS];
    /** The device timer Hz rate. Defaults to AC97_TIMER_HZ_DEFAULT_DEFAULT. */
    uint16_t                uTimerHz;
    uint16_t                au16Padding1[3];
    uint8_t                 silence[128];
    uint32_t                bup_flag;
    /** Codec model. */
    AC97CODEC               enmCodecModel;

    /** PCI region \#0: NAM I/O ports. */
    IOMIOPORTHANDLE         hIoPortsNam;
    /** PCI region \#0: NANM I/O ports. */
    IOMIOPORTHANDLE         hIoPortsNabm;

    STAMCOUNTER             StatUnimplementedNabmReads;
    STAMCOUNTER             StatUnimplementedNabmWrites;
#ifdef VBOX_WITH_STATISTICS
    STAMPROFILE             StatTimer;
    STAMPROFILE             StatIn;
    STAMPROFILE             StatOut;
    STAMCOUNTER             StatBytesRead;
    STAMCOUNTER             StatBytesWritten;
#endif
} AC97STATE;
AssertCompileMemberAlignment(AC97STATE, aStreams, 8);
AssertCompileMemberAlignment(AC97STATE, StatUnimplementedNabmReads, 8);
#ifdef VBOX_WITH_STATISTICS
AssertCompileMemberAlignment(AC97STATE, StatTimer,        8);
AssertCompileMemberAlignment(AC97STATE, StatBytesRead,    8);
AssertCompileMemberAlignment(AC97STATE, StatBytesWritten, 8);
#endif


/**
 * The ring-3 AC'97 device state.
 */
typedef struct AC97STATER3
{
    /** Array of AC'97 streams (parallel to AC97STATE:aStreams). */
    AC97STREAMR3            aStreams[AC97_MAX_STREAMS];
    /** R3 pointer to the device instance. */
    PPDMDEVINSR3            pDevIns;
    /** List of associated LUN drivers (AC97DRIVER). */
    RTLISTANCHORR3          lstDrv;
    /** The device's software mixer. */
    R3PTRTYPE(PAUDIOMIXER)  pMixer;
    /** Audio sink for PCM output. */
    R3PTRTYPE(PAUDMIXSINK)  pSinkOut;
    /** Audio sink for line input. */
    R3PTRTYPE(PAUDMIXSINK)  pSinkLineIn;
    /** Audio sink for microphone input. */
    R3PTRTYPE(PAUDMIXSINK)  pSinkMicIn;
    /** The base interface for LUN\#0. */
    PDMIBASE                IBase;
    /** Debug settings. */
    AC97STATEDEBUG          Dbg;
} AC97STATER3;
AssertCompileMemberAlignment(AC97STATER3, aStreams, 8);
/** Pointer to the ring-3 AC'97 device state. */
typedef AC97STATER3 *PAC97STATER3;


/**
 * Acquires the AC'97 lock.
 */
#define DEVAC97_LOCK(a_pDevIns, a_pThis) \
    do { \
        int rcLock = PDMDevHlpCritSectEnter((a_pDevIns), &(a_pThis)->CritSect, VERR_IGNORED); \
        AssertRC(rcLock); \
    } while (0)

/**
 * Acquires the AC'97 lock or returns.
 */
# define DEVAC97_LOCK_RETURN(a_pDevIns, a_pThis, a_rcBusy) \
    do { \
        int rcLock = PDMDevHlpCritSectEnter((a_pDevIns), &(a_pThis)->CritSect, a_rcBusy); \
        if (rcLock == VINF_SUCCESS) \
            break; \
        AssertRC(rcLock); \
        return rcLock; \
    } while (0)

/** Retrieves an attribute from a specific audio stream in RC. */
#define DEVAC97_CTX_SUFF_SD(a_Var, a_SD)      CTX_SUFF(a_Var)[a_SD]

/**
 * Releases the AC'97 lock.
 */
#define DEVAC97_UNLOCK(a_pDevIns, a_pThis) \
    do { PDMDevHlpCritSectLeave((a_pDevIns), &(a_pThis)->CritSect); } while (0)

/**
 * Acquires the TM lock and AC'97 lock, returns on failure.
 */
#define DEVAC97_LOCK_BOTH_RETURN(a_pDevIns, a_pThis, a_pStream, a_rcBusy) \
    do { \
        VBOXSTRICTRC rcLock = PDMDevHlpTimerLockClock2((a_pDevIns), (a_pStream)->hTimer, &(a_pThis)->CritSect, (a_rcBusy)); \
        if (RT_LIKELY(rcLock == VINF_SUCCESS)) \
        { /* likely */ } \
        else \
        { \
            AssertRC(VBOXSTRICTRC_VAL(rcLock)); \
            return rcLock; \
        } \
    } while (0)

/**
 * Releases the AC'97 lock and TM lock.
 */
#define DEVAC97_UNLOCK_BOTH(a_pDevIns, a_pThis, a_pStream) \
    PDMDevHlpTimerUnlockClock2((a_pDevIns), (a_pStream)->hTimer, &(a_pThis)->CritSect)

#ifndef VBOX_DEVICE_STRUCT_TESTCASE


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
#ifdef IN_RING3
static int                ichac97R3StreamOpen(PAC97STATE pThis, PAC97STATER3 pThisCC, PAC97STREAM pStream, PAC97STREAMR3 pStreamCC, bool fForce);
static int                ichac97R3StreamClose(PAC97STREAM pStream);
static void               ichac97R3StreamLock(PAC97STREAMR3 pStreamCC);
static void               ichac97R3StreamUnlock(PAC97STREAMR3 pStreamCC);
static uint32_t           ichac97R3StreamGetUsed(PAC97STREAMR3 pStreamCC);
static uint32_t           ichac97R3StreamGetFree(PAC97STREAMR3 pStreamCC);
static int                ichac97R3StreamTransfer(PPDMDEVINS pDevIns, PAC97STATE pThis, PAC97STREAM pStream,
                                                  PAC97STREAMR3 pStreamCC, uint32_t cbToProcessMax);
static void               ichac97R3StreamUpdate(PPDMDEVINS pDevIns, PAC97STATE pThis, PAC97STATER3 pThisCC, PAC97STREAM pStream,
                                                PAC97STREAMR3 pStreamCC, bool fInTimer);

static DECLCALLBACK(void) ichac97R3Reset(PPDMDEVINS pDevIns);

static DECLCALLBACK(void) ichac97R3Timer(PPDMDEVINS pDevIns, PTMTIMER pTimer, void *pvUser);

static void               ichac97R3MixerRemoveDrvStreams(PAC97STATER3 pThisCC, PAUDMIXSINK pMixSink, PDMAUDIODIR enmDir,
                                                         PDMAUDIODSTSRCUNION dstSrc);

# ifdef VBOX_WITH_AUDIO_AC97_ASYNC_IO
static int                ichac97R3StreamAsyncIOCreate(PAC97STATE pThis, PAC97STATER3 pThisCC, PAC97STREAM pStream, PAC97STREAMR3 pStreamCC);
static int                ichac97R3StreamAsyncIODestroy(PAC97STATE pThis, PAC97STREAMR3 pStreamCC);
static void               ichac97R3StreamAsyncIOLock(PAC97STREAMR3 pStreamCC);
static void               ichac97R3StreamAsyncIOUnlock(PAC97STREAMR3 pStreamCC);
/*static void               ichac97R3StreamAsyncIOEnable(PAC97STREAM pStream, bool fEnable); Unused */
# endif

DECLINLINE(PDMAUDIODIR)   ichac97GetDirFromSD(uint8_t uSD);
DECLINLINE(void)          ichac97R3TimerSet(PPDMDEVINS pDevIns, PAC97STREAM pStream, uint64_t cTicksToDeadline);
#endif /* IN_RING3 */


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
#ifdef IN_RING3
/** NABM I/O port descriptions. */
static const IOMIOPORTDESC g_aNabmPorts[] =
{
    { "PCM IN - BDBAR",     "PCM IN - BDBAR",   NULL, NULL },
    { "",                   NULL,               NULL, NULL },
    { "",                   NULL,               NULL, NULL },
    { "",                   NULL,               NULL, NULL },
    { "PCM IN - CIV",       "PCM IN - CIV",     NULL, NULL },
    { "PCM IN - LVI",       "PCM IN - LIV",     NULL, NULL },
    { "PCM IN - SR",        "PCM IN - SR",      NULL, NULL },
    { "",                   NULL,               NULL, NULL },
    { "PCM IN - PICB",      "PCM IN - PICB",    NULL, NULL },
    { "",                   NULL,               NULL, NULL },
    { "PCM IN - PIV",       "PCM IN - PIV",     NULL, NULL },
    { "PCM IN - CR",        "PCM IN - CR",      NULL, NULL },
    { "",                   NULL,               NULL, NULL },
    { "",                   NULL,               NULL, NULL },
    { "",                   NULL,               NULL, NULL },
    { "",                   NULL,               NULL, NULL },

    { "PCM OUT - BDBAR",    "PCM OUT - BDBAR",  NULL, NULL },
    { "",                   NULL,               NULL, NULL },
    { "",                   NULL,               NULL, NULL },
    { "",                   NULL,               NULL, NULL },
    { "PCM OUT - CIV",      "PCM OUT - CIV",    NULL, NULL },
    { "PCM OUT - LVI",      "PCM OUT - LIV",    NULL, NULL },
    { "PCM OUT - SR",       "PCM OUT - SR",     NULL, NULL },
    { "",                   NULL,               NULL, NULL },
    { "PCM OUT - PICB",     "PCM OUT - PICB",   NULL, NULL },
    { "",                   NULL,               NULL, NULL },
    { "PCM OUT - PIV",      "PCM OUT - PIV",    NULL, NULL },
    { "PCM OUT - CR",       "PCM IN - CR",      NULL, NULL },
    { "",                   NULL,               NULL, NULL },
    { "",                   NULL,               NULL, NULL },
    { "",                   NULL,               NULL, NULL },
    { "",                   NULL,               NULL, NULL },

    { "MIC IN - BDBAR",     "MIC IN - BDBAR",   NULL, NULL },
    { "",                   NULL,               NULL, NULL },
    { "",                   NULL,               NULL, NULL },
    { "",                   NULL,               NULL, NULL },
    { "MIC IN - CIV",       "MIC IN - CIV",     NULL, NULL },
    { "MIC IN - LVI",       "MIC IN - LIV",     NULL, NULL },
    { "MIC IN - SR",        "MIC IN - SR",      NULL, NULL },
    { "",                   NULL,               NULL, NULL },
    { "MIC IN - PICB",      "MIC IN - PICB",    NULL, NULL },
    { "",                   NULL,               NULL, NULL },
    { "MIC IN - PIV",       "MIC IN - PIV",     NULL, NULL },
    { "MIC IN - CR",        "MIC IN - CR",      NULL, NULL },
    { "GLOB CNT",           "GLOB CNT",         NULL, NULL },
    { "",                   NULL,               NULL, NULL },
    { "",                   NULL,               NULL, NULL },
    { "",                   NULL,               NULL, NULL },

    { "GLOB STA",           "GLOB STA",         NULL, NULL },
    { "",                   NULL,               NULL, NULL },
    { "",                   NULL,               NULL, NULL },
    { "",                   NULL,               NULL, NULL },
    { "CAS",                "CAS",              NULL, NULL },
    { NULL,                 NULL,               NULL, NULL },
};

#define AC97SOUNDSOURCE_PI_INDEX        0           /**< PCM in */
#define AC97SOUNDSOURCE_PO_INDEX        1           /**< PCM out */
#define AC97SOUNDSOURCE_MC_INDEX        2           /**< Mic in */
#define AC97SOUNDSOURCE_MAX             3           /**< Max sound sources. */
/** @} */

/** Port number (offset into NABM BAR) to stream index. */
#define AC97_PORT2IDX(a_idx)            ( ((a_idx) >> 4) & 3 )
/** Port number (offset into NABM BAR) to stream index, but no masking. */
#define AC97_PORT2IDX_UNMASKED(a_idx)   ( ((a_idx) >> 4) )

/** @name Stream offsets
 * @{ */
#define AC97_NABM_OFF_BDBAR             0x0         /**< Buffer Descriptor Base Address */
#define AC97_NABM_OFF_CIV               0x4         /**< Current Index Value */
#define AC97_NABM_OFF_LVI               0x5         /**< Last Valid Index */
#define AC97_NABM_OFF_SR                0x6         /**< Status Register */
#define AC97_NABM_OFF_PICB              0x8         /**< Position in Current Buffer */
#define AC97_NABM_OFF_PIV               0xa         /**< Prefetched Index Value */
#define AC97_NABM_OFF_CR                0xb         /**< Control Register */
#define AC97_NABM_OFF_MASK              0xf         /**< Mask for getting the the per-stream register. */

#endif



static void ichac97WarmReset(PAC97STATE pThis)
{
    NOREF(pThis);
}

static void ichac97ColdReset(PAC97STATE pThis)
{
    NOREF(pThis);
}


#ifdef IN_RING3

/**
 * Retrieves the audio mixer sink of a corresponding AC'97 stream index.
 *
 * @returns Pointer to audio mixer sink if found, or NULL if not found / invalid.
 * @param   pThisCC             The ring-3 AC'97 state.
 * @param   uIndex              Stream index to get audio mixer sink for.
 */
DECLINLINE(PAUDMIXSINK) ichac97R3IndexToSink(PAC97STATER3 pThisCC, uint8_t uIndex)
{
    switch (uIndex)
    {
        case AC97SOUNDSOURCE_PI_INDEX: return pThisCC->pSinkLineIn;
        case AC97SOUNDSOURCE_PO_INDEX: return pThisCC->pSinkOut;
        case AC97SOUNDSOURCE_MC_INDEX: return pThisCC->pSinkMicIn;
        default:
            AssertMsgFailedReturn(("Wrong index %RU8\n", uIndex), NULL);
    }
}

/**
 * Fetches the current BDLE (Buffer Descriptor List Entry) of an AC'97 audio stream.
 *
 * @returns IPRT status code.
 * @param   pDevIns             The device instance.
 * @param   pStream             AC'97 stream to fetch BDLE for.
 *
 * @remark  Uses CIV as BDLE index.
 */
static void ichac97R3StreamFetchBDLE(PPDMDEVINS pDevIns, PAC97STREAM pStream)
{
    PAC97BMREGS pRegs = &pStream->Regs;

    AC97BDLE BDLE;
    PDMDevHlpPhysRead(pDevIns, pRegs->bdbar + pRegs->civ * sizeof(AC97BDLE), &BDLE, sizeof(AC97BDLE));
    pRegs->bd_valid   = 1;
# ifndef RT_LITTLE_ENDIAN
#  error "Please adapt the code (audio buffers are little endian)!"
# else
    pRegs->bd.addr    = RT_H2LE_U32(BDLE.addr & ~3);
    pRegs->bd.ctl_len = RT_H2LE_U32(BDLE.ctl_len);
# endif
    pRegs->picb       = pRegs->bd.ctl_len & AC97_BD_LEN_MASK;
    LogFlowFunc(("bd %2d addr=%#x ctl=%#06x len=%#x(%d bytes), bup=%RTbool, ioc=%RTbool\n",
                  pRegs->civ, pRegs->bd.addr, pRegs->bd.ctl_len >> 16,
                  pRegs->bd.ctl_len & AC97_BD_LEN_MASK,
                 (pRegs->bd.ctl_len & AC97_BD_LEN_MASK) << 1,  /** @todo r=andy Assumes 16bit samples. */
                  RT_BOOL(pRegs->bd.ctl_len & AC97_BD_BUP),
                  RT_BOOL(pRegs->bd.ctl_len & AC97_BD_IOC)));
}

#endif /* IN_RING3 */

/**
 * Updates the status register (SR) of an AC'97 audio stream.
 *
 * @param   pDevIns             The device instance.
 * @param   pThis               The shared AC'97 state.
 * @param   pStream             AC'97 stream to update SR for.
 * @param   new_sr              New value for status register (SR).
 */
static void ichac97StreamUpdateSR(PPDMDEVINS pDevIns, PAC97STATE pThis, PAC97STREAM pStream, uint32_t new_sr)
{
    PAC97BMREGS pRegs   = &pStream->Regs;

    bool fSignal = false;
    int  iIRQL = 0;

    uint32_t new_mask = new_sr & AC97_SR_INT_MASK;
    uint32_t old_mask = pRegs->sr  & AC97_SR_INT_MASK;

    if (new_mask ^ old_mask)
    {
        /** @todo Is IRQ deasserted when only one of status bits is cleared? */
        if (!new_mask)
        {
            fSignal = true;
            iIRQL   = 0;
        }
        else if ((new_mask & AC97_SR_LVBCI) && (pRegs->cr & AC97_CR_LVBIE))
        {
            fSignal = true;
            iIRQL   = 1;
        }
        else if ((new_mask & AC97_SR_BCIS) && (pRegs->cr & AC97_CR_IOCE))
        {
            fSignal = true;
            iIRQL   = 1;
        }
    }

    pRegs->sr = new_sr;

    LogFlowFunc(("IOC%d, LVB%d, sr=%#x, fSignal=%RTbool, IRQL=%d\n",
                 pRegs->sr & AC97_SR_BCIS, pRegs->sr & AC97_SR_LVBCI, pRegs->sr, fSignal, iIRQL));

    if (fSignal)
    {
        static uint32_t const s_aMasks[] = { AC97_GS_PIINT, AC97_GS_POINT, AC97_GS_MINT };
        Assert(pStream->u8SD < AC97_MAX_STREAMS);
        if (iIRQL)
            pThis->glob_sta |=  s_aMasks[pStream->u8SD];
        else
            pThis->glob_sta &= ~s_aMasks[pStream->u8SD];

        LogFlowFunc(("Setting IRQ level=%d\n", iIRQL));
        PDMDevHlpPCISetIrq(pDevIns, 0, iIRQL);
    }
}

/**
 * Writes a new value to a stream's status register (SR).
 *
 * @param   pDevIns             The device instance.
 * @param   pThis               The shared AC'97 device state.
 * @param   pStream             Stream to update SR for.
 * @param   u32Val              New value to set the stream's SR to.
 */
static void ichac97StreamWriteSR(PPDMDEVINS pDevIns, PAC97STATE pThis, PAC97STREAM pStream, uint32_t u32Val)
{
    PAC97BMREGS pRegs = &pStream->Regs;

    Log3Func(("[SD%RU8] SR <- %#x (sr %#x)\n", pStream->u8SD, u32Val, pRegs->sr));

    pRegs->sr |= u32Val & ~(AC97_SR_RO_MASK | AC97_SR_WCLEAR_MASK);
    ichac97StreamUpdateSR(pDevIns, pThis, pStream, pRegs->sr & ~(u32Val & AC97_SR_WCLEAR_MASK));
}

#ifdef IN_RING3

/**
 * Returns whether an AC'97 stream is enabled or not.
 *
 * @returns IPRT status code.
 * @param   pThisCC             The ring-3 AC'97 device state.
 * @param   pStream             Stream to return status for.
 */
static bool ichac97R3StreamIsEnabled(PAC97STATER3 pThisCC, PAC97STREAM pStream)
{
    PAUDMIXSINK pSink = ichac97R3IndexToSink(pThisCC, pStream->u8SD);
    bool fIsEnabled = RT_BOOL(AudioMixerSinkGetStatus(pSink) & AUDMIXSINK_STS_RUNNING);

    LogFunc(("[SD%RU8] fIsEnabled=%RTbool\n", pStream->u8SD, fIsEnabled));
    return fIsEnabled;
}

/**
 * Enables or disables an AC'97 audio stream.
 *
 * @returns IPRT status code.
 * @param   pThis               The shared AC'97 state.
 * @param   pThisCC             The ring-3 AC'97 state.
 * @param   pStream             The AC'97 stream to enable or disable (shared
 *                              state).
 * @param   pStreamCC           The ring-3 stream state (matching to @a pStream).
 * @param   fEnable             Whether to enable or disable the stream.
 *
 */
static int ichac97R3StreamEnable(PAC97STATE pThis, PAC97STATER3 pThisCC,
                                 PAC97STREAM pStream, PAC97STREAMR3 pStreamCC, bool fEnable)
{
    ichac97R3StreamLock(pStreamCC);

    int rc = VINF_SUCCESS;

# ifdef VBOX_WITH_AUDIO_AC97_ASYNC_IO
    if (fEnable)
        rc = ichac97R3StreamAsyncIOCreate(pThis, pThisCC, pStream, pStreamCC);
    if (RT_SUCCESS(rc))
        ichac97R3StreamAsyncIOLock(pStreamCC);
# endif

    if (fEnable)
    {
        if (pStreamCC->State.pCircBuf)
            RTCircBufReset(pStreamCC->State.pCircBuf);

        rc = ichac97R3StreamOpen(pThis, pThisCC, pStream, pStreamCC, false /* fForce */);

        if (RT_LIKELY(!pStreamCC->Dbg.Runtime.fEnabled))
        { /* likely */ }
        else
        {
            if (!DrvAudioHlpFileIsOpen(pStreamCC->Dbg.Runtime.pFileStream))
            {
                int rc2 = DrvAudioHlpFileOpen(pStreamCC->Dbg.Runtime.pFileStream, PDMAUDIOFILE_DEFAULT_OPEN_FLAGS,
                                              &pStreamCC->State.Cfg.Props);
                AssertRC(rc2);
            }

            if (!DrvAudioHlpFileIsOpen(pStreamCC->Dbg.Runtime.pFileDMA))
            {
                int rc2 = DrvAudioHlpFileOpen(pStreamCC->Dbg.Runtime.pFileDMA, PDMAUDIOFILE_DEFAULT_OPEN_FLAGS,
                                              &pStreamCC->State.Cfg.Props);
                AssertRC(rc2);
            }
        }
    }
    else
        rc = ichac97R3StreamClose(pStream);

    if (RT_SUCCESS(rc))
    {
        /* First, enable or disable the stream and the stream's sink, if any. */
        rc = AudioMixerSinkCtl(ichac97R3IndexToSink(pThisCC, pStream->u8SD),
                               fEnable ? AUDMIXSINKCMD_ENABLE : AUDMIXSINKCMD_DISABLE);
    }

# ifdef VBOX_WITH_AUDIO_AC97_ASYNC_IO
    ichac97R3StreamAsyncIOUnlock(pStreamCC);
# endif

    /* Make sure to leave the lock before (eventually) starting the timer. */
    ichac97R3StreamUnlock(pStreamCC);

    LogFunc(("[SD%RU8] fEnable=%RTbool, rc=%Rrc\n", pStream->u8SD, fEnable, rc));
    return rc;
}

/**
 * Resets an AC'97 stream.
 *
 * @param   pThis               The shared AC'97 state.
 * @param   pStream             The AC'97 stream to reset (shared).
 * @param   pStreamCC           The AC'97 stream to reset (ring-3).
 */
static void ichac97R3StreamReset(PAC97STATE pThis, PAC97STREAM pStream, PAC97STREAMR3 pStreamCC)
{
    ichac97R3StreamLock(pStreamCC);

    LogFunc(("[SD%RU8]\n", pStream->u8SD));

    if (pStreamCC->State.pCircBuf)
        RTCircBufReset(pStreamCC->State.pCircBuf);

    PAC97BMREGS pRegs = &pStream->Regs;

    pRegs->bdbar    = 0;
    pRegs->civ      = 0;
    pRegs->lvi      = 0;

    pRegs->picb     = 0;
    pRegs->piv      = 0;
    pRegs->cr       = pRegs->cr & AC97_CR_DONT_CLEAR_MASK;
    pRegs->bd_valid = 0;

    RT_ZERO(pThis->silence);

    ichac97R3StreamUnlock(pStreamCC);
}

/**
 * Creates an AC'97 audio stream.
 *
 * @returns IPRT status code.
 * @param   pThisCC             The ring-3 AC'97 state.
 * @param   pStream             The AC'97 stream to create (shared).
 * @param   pStreamCC           The AC'97 stream to create (ring-3).
 * @param   u8SD                Stream descriptor number to assign.
 */
static int ichac97R3StreamCreate(PAC97STATER3 pThisCC, PAC97STREAM pStream, PAC97STREAMR3 pStreamCC, uint8_t u8SD)
{
    LogFunc(("[SD%RU8] pStream=%p\n", u8SD, pStream));

    AssertReturn(u8SD < AC97_MAX_STREAMS, VERR_INVALID_PARAMETER);
    pStream->u8SD       = u8SD;
    pStreamCC->u8SD     = u8SD;

    int rc = RTCritSectInit(&pStreamCC->State.CritSect);
    AssertRCReturn(rc, rc);

    pStreamCC->Dbg.Runtime.fEnabled = pThisCC->Dbg.fEnabled;

    if (RT_LIKELY(!pStreamCC->Dbg.Runtime.fEnabled))
    { /* likely */ }
    else
    {
        char szFile[64];

        if (ichac97GetDirFromSD(pStream->u8SD) == PDMAUDIODIR_IN)
            RTStrPrintf(szFile, sizeof(szFile), "ac97StreamWriteSD%RU8", pStream->u8SD);
        else
            RTStrPrintf(szFile, sizeof(szFile), "ac97StreamReadSD%RU8", pStream->u8SD);

        char szPath[RTPATH_MAX];
        int rc2 = DrvAudioHlpFileNameGet(szPath, sizeof(szPath), pThisCC->Dbg.pszOutPath, szFile,
                                         0 /* uInst */, PDMAUDIOFILETYPE_WAV, PDMAUDIOFILENAME_FLAGS_NONE);
        AssertRC(rc2);
        rc2 = DrvAudioHlpFileCreate(PDMAUDIOFILETYPE_WAV, szPath, PDMAUDIOFILE_FLAGS_NONE, &pStreamCC->Dbg.Runtime.pFileStream);
        AssertRC(rc2);

        if (ichac97GetDirFromSD(pStream->u8SD) == PDMAUDIODIR_IN)
            RTStrPrintf(szFile, sizeof(szFile), "ac97DMAWriteSD%RU8", pStream->u8SD);
        else
            RTStrPrintf(szFile, sizeof(szFile), "ac97DMAReadSD%RU8", pStream->u8SD);

        rc2 = DrvAudioHlpFileNameGet(szPath, sizeof(szPath), pThisCC->Dbg.pszOutPath, szFile,
                                     0 /* uInst */, PDMAUDIOFILETYPE_WAV, PDMAUDIOFILENAME_FLAGS_NONE);
        AssertRC(rc2);

        rc2 = DrvAudioHlpFileCreate(PDMAUDIOFILETYPE_WAV, szPath, PDMAUDIOFILE_FLAGS_NONE, &pStreamCC->Dbg.Runtime.pFileDMA);
        AssertRC(rc2);

        /* Delete stale debugging files from a former run. */
        DrvAudioHlpFileDelete(pStreamCC->Dbg.Runtime.pFileStream);
        DrvAudioHlpFileDelete(pStreamCC->Dbg.Runtime.pFileDMA);
    }

    return rc;
}

/**
 * Destroys an AC'97 audio stream.
 *
 * @returns IPRT status code.
 * @param   pThis               The shared AC'97 state.
 * @param   pStream             The AC'97 stream to destroy (shared).
 * @param   pStreamCC           The AC'97 stream to destroy (ring-3).
 */
static void ichac97R3StreamDestroy(PAC97STATE pThis, PAC97STREAM pStream, PAC97STREAMR3 pStreamCC)
{
    LogFlowFunc(("[SD%RU8]\n", pStream->u8SD));

    ichac97R3StreamClose(pStream);

    int rc2 = RTCritSectDelete(&pStreamCC->State.CritSect);
    AssertRC(rc2);

# ifdef VBOX_WITH_AUDIO_AC97_ASYNC_IO
    rc2 = ichac97R3StreamAsyncIODestroy(pThis, pStreamCC);
    AssertRC(rc2);
# else
    RT_NOREF(pThis);
# endif

    if (RT_LIKELY(!pStreamCC->Dbg.Runtime.fEnabled))
    { /* likely */ }
    else
    {
        DrvAudioHlpFileDestroy(pStreamCC->Dbg.Runtime.pFileStream);
        pStreamCC->Dbg.Runtime.pFileStream = NULL;

        DrvAudioHlpFileDestroy(pStreamCC->Dbg.Runtime.pFileDMA);
        pStreamCC->Dbg.Runtime.pFileDMA = NULL;
    }

    if (pStreamCC->State.pCircBuf)
    {
        RTCircBufDestroy(pStreamCC->State.pCircBuf);
        pStreamCC->State.pCircBuf = NULL;
    }

    LogFlowFuncLeave();
}

/**
 * Destroys all AC'97 audio streams of the device.
 *
 * @param   pThis               The shared AC'97 state.
 * @param   pThisCC             The ring-3 AC'97 state.
 */
static void ichac97R3StreamsDestroy(PAC97STATE pThis, PAC97STATER3 pThisCC)
{
    LogFlowFuncEnter();

    /*
     * Destroy all AC'97 streams.
     */
    for (unsigned i = 0; i < AC97_MAX_STREAMS; i++)
        ichac97R3StreamDestroy(pThis, &pThis->aStreams[i], &pThisCC->aStreams[i]);

    /*
     * Destroy all sinks.
     */

    PDMAUDIODSTSRCUNION dstSrc;
    if (pThisCC->pSinkLineIn)
    {
        dstSrc.enmSrc = PDMAUDIORECSRC_LINE;
        ichac97R3MixerRemoveDrvStreams(pThisCC, pThisCC->pSinkLineIn, PDMAUDIODIR_IN, dstSrc);

        AudioMixerSinkDestroy(pThisCC->pSinkLineIn);
        pThisCC->pSinkLineIn = NULL;
    }

    if (pThisCC->pSinkMicIn)
    {
        dstSrc.enmSrc = PDMAUDIORECSRC_MIC;
        ichac97R3MixerRemoveDrvStreams(pThisCC, pThisCC->pSinkMicIn, PDMAUDIODIR_IN, dstSrc);

        AudioMixerSinkDestroy(pThisCC->pSinkMicIn);
        pThisCC->pSinkMicIn = NULL;
    }

    if (pThisCC->pSinkOut)
    {
        dstSrc.enmDst = PDMAUDIOPLAYBACKDST_FRONT;
        ichac97R3MixerRemoveDrvStreams(pThisCC, pThisCC->pSinkOut, PDMAUDIODIR_OUT, dstSrc);

        AudioMixerSinkDestroy(pThisCC->pSinkOut);
        pThisCC->pSinkOut = NULL;
    }
}

/**
 * Writes audio data from a mixer sink into an AC'97 stream's DMA buffer.
 *
 * @returns IPRT status code.
 * @param   pDstStreamCC        The AC'97 stream to write to (ring-3).
 * @param   pSrcMixSink         Mixer sink to get audio data to write from.
 * @param   cbToWrite           Number of bytes to write.
 * @param   pcbWritten          Number of bytes written. Optional.
 */
static int ichac97R3StreamWrite(PAC97STREAMR3 pDstStreamCC, PAUDMIXSINK pSrcMixSink, uint32_t cbToWrite, uint32_t *pcbWritten)
{
    AssertPtrReturn(pSrcMixSink, VERR_INVALID_POINTER);
    AssertReturn(cbToWrite > 0,  VERR_INVALID_PARAMETER);
    /* pcbWritten is optional. */

    PRTCIRCBUF pCircBuf = pDstStreamCC->State.pCircBuf;
    AssertPtr(pCircBuf);

    uint32_t cbRead = 0;

    void    *pvDst;
    size_t   cbDst;
    RTCircBufAcquireWriteBlock(pCircBuf, cbToWrite, &pvDst, &cbDst);

    if (cbDst)
    {
        int rc2 = AudioMixerSinkRead(pSrcMixSink, AUDMIXOP_COPY, pvDst, (uint32_t)cbDst, &cbRead);
        AssertRC(rc2);

        if (RT_LIKELY(!pDstStreamCC->Dbg.Runtime.fEnabled))
        { /* likely */ }
        else
            DrvAudioHlpFileWrite(pDstStreamCC->Dbg.Runtime.pFileStream, pvDst, cbRead, 0 /* fFlags */);
    }

    RTCircBufReleaseWriteBlock(pCircBuf, cbRead);

    if (pcbWritten)
        *pcbWritten = cbRead;

    return VINF_SUCCESS;
}

/**
 * Reads audio data from an AC'97 stream's DMA buffer and writes into a specified mixer sink.
 *
 * @returns IPRT status code.
 * @param   pSrcStreamCC        AC'97 stream to read audio data from (ring-3).
 * @param   pDstMixSink         Mixer sink to write audio data to.
 * @param   cbToRead            Number of bytes to read.
 * @param   pcbRead             Number of bytes read. Optional.
 */
static int ichac97R3StreamRead(PAC97STREAMR3 pSrcStreamCC, PAUDMIXSINK pDstMixSink, uint32_t cbToRead, uint32_t *pcbRead)
{
    AssertPtrReturn(pDstMixSink, VERR_INVALID_POINTER);
    AssertReturn(cbToRead > 0, VERR_INVALID_PARAMETER);
    /* pcbRead is optional. */

    PRTCIRCBUF pCircBuf = pSrcStreamCC->State.pCircBuf;
    AssertPtr(pCircBuf);

    void *pvSrc;
    size_t cbSrc;

    int rc = VINF_SUCCESS;

    uint32_t cbReadTotal = 0;
    uint32_t cbLeft      = RT_MIN(cbToRead, (uint32_t)RTCircBufUsed(pCircBuf));

    while (cbLeft)
    {
        uint32_t cbWritten = 0;

        RTCircBufAcquireReadBlock(pCircBuf, cbLeft, &pvSrc, &cbSrc);

        if (cbSrc)
        {
            if (RT_LIKELY(!pSrcStreamCC->Dbg.Runtime.fEnabled))
            { /* likely */ }
            else
                DrvAudioHlpFileWrite(pSrcStreamCC->Dbg.Runtime.pFileStream, pvSrc, cbSrc, 0 /* fFlags */);

            rc = AudioMixerSinkWrite(pDstMixSink, AUDMIXOP_COPY, pvSrc, (uint32_t)cbSrc, &cbWritten);
            AssertRC(rc);

            Assert(cbSrc >= cbWritten);
            Log3Func(("[SD%RU8] %RU32/%zu bytes read\n", pSrcStreamCC->u8SD, cbWritten, cbSrc));
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

# ifdef VBOX_WITH_AUDIO_AC97_ASYNC_IO

/**
 * Asynchronous I/O thread for an AC'97 stream.
 * This will do the heavy lifting work for us as soon as it's getting notified by another thread.
 *
 * @returns IPRT status code.
 * @param   hThreadSelf         Thread handle.
 * @param   pvUser              User argument. Must be of type PAC97STREAMTHREADCTX.
 */
static DECLCALLBACK(int) ichac97R3StreamAsyncIOThread(RTTHREAD hThreadSelf, void *pvUser)
{
    PAC97STREAMTHREADCTX pCtx = (PAC97STREAMTHREADCTX)pvUser;
    AssertPtr(pCtx);

    PAC97STATE pThis = pCtx->pThis;
    AssertPtr(pThis);

    PAC97STATER3 pThisCC = pCtx->pThisCC;
    AssertPtr(pThisCC);

    PAC97STREAM pStream = pCtx->pStream;
    AssertPtr(pStream);

    PAC97STREAMR3 pStreamCC = pCtx->pStreamCC;
    AssertPtr(pStreamCC);

    PAC97STREAMSTATEAIO pAIO = &pStreamCC->State.AIO;

    ASMAtomicXchgBool(&pAIO->fStarted, true);

    RTThreadUserSignal(hThreadSelf);

    LogFunc(("[SD%RU8] Started\n", pStream->u8SD));

    for (;;)
    {
        Log2Func(("[SD%RU8] Waiting ...\n", pStream->u8SD));

        int rc2 = RTSemEventWait(pAIO->Event, RT_INDEFINITE_WAIT);
        if (RT_FAILURE(rc2))
            break;

        if (ASMAtomicReadBool(&pAIO->fShutdown))
            break;

        rc2 = RTCritSectEnter(&pAIO->CritSect);
        if (RT_SUCCESS(rc2))
        {
            if (!pAIO->fEnabled)
            {
                RTCritSectLeave(&pAIO->CritSect);
                continue;
            }

            ichac97R3StreamUpdate(pThisCC->pDevIns, pThis, pThisCC, pStream, pStreamCC, false /* fInTimer */);

            int rc3 = RTCritSectLeave(&pAIO->CritSect);
            AssertRC(rc3);
        }

        AssertRC(rc2);
    }

    LogFunc(("[SD%RU8] Ended\n", pStream->u8SD));

    ASMAtomicXchgBool(&pAIO->fStarted, false);

    RTMemFree(pCtx);
    pCtx = NULL;

    return VINF_SUCCESS;
}

/**
 * Creates the async I/O thread for a specific AC'97 audio stream.
 *
 * @returns IPRT status code.
 * @param   pThis               The shared AC'97 state (shared).
 * @param   pThisCC             The shared AC'97 state (ring-3).
 * @param   pStream             AC'97 audio stream to create the async I/O thread for (shared).
 * @param   pStreamCC           AC'97 audio stream to create the async I/O thread for (ring-3).
 */
static int ichac97R3StreamAsyncIOCreate(PAC97STATE pThis, PAC97STATER3 pThisCC, PAC97STREAM pStream, PAC97STREAMR3 pStreamCC)
{
    PAC97STREAMSTATEAIO pAIO = &pStreamCC->State.AIO;

    int rc;

    if (!ASMAtomicReadBool(&pAIO->fStarted))
    {
        pAIO->fShutdown = false;
        pAIO->fEnabled  = true; /* Enabled by default. */

        rc = RTSemEventCreate(&pAIO->Event);
        if (RT_SUCCESS(rc))
        {
            rc = RTCritSectInit(&pAIO->CritSect);
            if (RT_SUCCESS(rc))
            {
/** @todo r=bird:
 *        Why aren't this code using the PDM threads (PDMDevHlpThreadCreate)?
 *        They would help you with managing stuff like VM suspending, resuming
 *        and powering off.
 *
 *        Finally, just create the threads at construction time. */
                PAC97STREAMTHREADCTX pCtx = (PAC97STREAMTHREADCTX)RTMemAllocZ(sizeof(AC97STREAMTHREADCTX));
                if (pCtx)
                {
                    pCtx->pStream   = pStream;
                    pCtx->pStreamCC = pStreamCC;
                    pCtx->pThis     = pThis;
                    pCtx->pThisCC   = pThisCC;

                    rc = RTThreadCreateF(&pAIO->Thread, ichac97R3StreamAsyncIOThread, pCtx,
                                         0, RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE, "ac97AIO%RU8", pStreamCC->u8SD);
                    if (RT_SUCCESS(rc))
                        rc = RTThreadUserWait(pAIO->Thread, 30 * 1000 /* 30s timeout */);
                }
                else
                    rc = VERR_NO_MEMORY;
            }
        }
    }
    else
        rc = VINF_SUCCESS;

    LogFunc(("[SD%RU8] Returning %Rrc\n", pStreamCC->u8SD, rc));
    return rc;
}

/**
 * Lets the stream's async I/O thread know that there is some data to process.
 *
 * @returns IPRT status code.
 * @param   pStreamCC             The AC'97 stream to notify async I/O thread
 *                                for (ring-3).
 */
static int ichac97R3StreamAsyncIONotify(PAC97STREAMR3 pStreamCC)
{
    LogFunc(("[SD%RU8]\n", pStreamCC->u8SD));
    return RTSemEventSignal(pStreamCC->State.AIO.Event);
}

/**
 * Destroys the async I/O thread of a specific AC'97 audio stream.
 *
 * @returns IPRT status code.
 * @param   pThis               The shared AC'97 state.
 * @param   pStreamCC           AC'97 audio stream to destroy the async I/O thread for.
 */
static int ichac97R3StreamAsyncIODestroy(PAC97STATE pThis, PAC97STREAMR3 pStreamR3)
{
    RT_NOREF(pThis);

    PAC97STREAMSTATEAIO pAIO = &pStreamR3->State.AIO;

    if (!ASMAtomicReadBool(&pAIO->fStarted))
        return VINF_SUCCESS;

    ASMAtomicWriteBool(&pAIO->fShutdown, true);

    int rc = ichac97R3StreamAsyncIONotify(pStreamR3);
    AssertRC(rc);

    int rcThread;
    rc = RTThreadWait(pAIO->Thread, 30 * 1000 /* 30s timeout */, &rcThread);
    LogFunc(("Async I/O thread ended with %Rrc (%Rrc)\n", rc, rcThread));

    if (RT_SUCCESS(rc))
    {
        rc = RTCritSectDelete(&pAIO->CritSect);
        AssertRC(rc);

        rc = RTSemEventDestroy(pAIO->Event);
        AssertRC(rc);

        pAIO->fStarted  = false;
        pAIO->fShutdown = false;
        pAIO->fEnabled  = false;
    }

    LogFunc(("[SD%RU8] Returning %Rrc\n", pStreamR3->u8SD, rc));
    return rc;
}

/**
 * Locks the async I/O thread of a specific AC'97 audio stream.
 *
 * @param   pStreamCC           AC'97 stream to lock async I/O thread for.
 */
static void ichac97R3StreamAsyncIOLock(PAC97STREAMR3 pStreamCC)
{
    PAC97STREAMSTATEAIO pAIO = &pStreamCC->State.AIO;

    if (!ASMAtomicReadBool(&pAIO->fStarted))
        return;

    int rc2 = RTCritSectEnter(&pAIO->CritSect);
    AssertRC(rc2);
}

/**
 * Unlocks the async I/O thread of a specific AC'97 audio stream.
 *
 * @param   pStreamCC           AC'97 stream to unlock async I/O thread for.
 */
static void ichac97R3StreamAsyncIOUnlock(PAC97STREAMR3 pStreamCC)
{
    PAC97STREAMSTATEAIO pAIO = &pStreamCC->State.AIO;

    if (!ASMAtomicReadBool(&pAIO->fStarted))
        return;

    int rc2 = RTCritSectLeave(&pAIO->CritSect);
    AssertRC(rc2);
}

#if 0 /* Unused */
/**
 * Enables (resumes) or disables (pauses) the async I/O thread.
 *
 * @param   pStream             AC'97 stream to enable/disable async I/O thread for.
 * @param   fEnable             Whether to enable or disable the I/O thread.
 *
 * @remarks Does not do locking.
 */
static void ichac97R3StreamAsyncIOEnable(PAC97STREAM pStream, bool fEnable)
{
    PAC97STREAMSTATEAIO pAIO = &pStreamCC->State.AIO;
    ASMAtomicXchgBool(&pAIO->fEnabled, fEnable);
}
#endif
# endif /* VBOX_WITH_AUDIO_AC97_ASYNC_IO */

# ifdef LOG_ENABLED
static void ichac97R3BDLEDumpAll(PPDMDEVINS pDevIns, uint64_t u64BDLBase, uint16_t cBDLE)
{
    LogFlowFunc(("BDLEs @ 0x%x (%RU16):\n", u64BDLBase, cBDLE));
    if (!u64BDLBase)
        return;

    uint32_t cbBDLE = 0;
    for (uint16_t i = 0; i < cBDLE; i++)
    {
        AC97BDLE BDLE;
        PDMDevHlpPhysRead(pDevIns, u64BDLBase + i * sizeof(AC97BDLE), &BDLE, sizeof(AC97BDLE));

# ifndef RT_LITTLE_ENDIAN
#  error "Please adapt the code (audio buffers are little endian)!"
# else
        BDLE.addr    = RT_H2LE_U32(BDLE.addr & ~3);
        BDLE.ctl_len = RT_H2LE_U32(BDLE.ctl_len);
#endif
        LogFunc(("\t#%03d BDLE(adr:0x%llx, size:%RU32 [%RU32 bytes], bup:%RTbool, ioc:%RTbool)\n",
                  i, BDLE.addr,
                  BDLE.ctl_len & AC97_BD_LEN_MASK,
                 (BDLE.ctl_len & AC97_BD_LEN_MASK) << 1, /** @todo r=andy Assumes 16bit samples. */
                  RT_BOOL(BDLE.ctl_len & AC97_BD_BUP),
                  RT_BOOL(BDLE.ctl_len & AC97_BD_IOC)));

        cbBDLE += (BDLE.ctl_len & AC97_BD_LEN_MASK) << 1; /** @todo r=andy Ditto. */
    }

    LogFlowFunc(("Total: %RU32 bytes\n", cbBDLE));
}
# endif /* LOG_ENABLED */

/**
 * Updates an AC'97 stream by doing its required data transfers.
 * The host sink(s) set the overall pace.
 *
 * This routine is called by both, the synchronous and the asynchronous
 * (VBOX_WITH_AUDIO_AC97_ASYNC_IO), implementations.
 *
 * When running synchronously, the device DMA transfers *and* the mixer sink
 * processing is within the device timer.
 *
 * When running asynchronously, only the device DMA transfers are done in the
 * device timer, whereas the mixer sink processing then is done in the stream's
 * own async I/O thread. This thread also will call this function
 * (with fInTimer set to @c false).
 *
 * @param   pDevIns             The device instance.
 * @param   pThis               The shared AC'97 state.
 * @param   pThisCC             The ring-3 AC'97 state.
 * @param   pStream             The AC'97 stream to update (shared).
 * @param   pStreamCC           The AC'97 stream to update (ring-3).
 * @param   fInTimer            Whether to this function was called from the timer
 *                              context or an asynchronous I/O stream thread (if supported).
 */
static void ichac97R3StreamUpdate(PPDMDEVINS pDevIns, PAC97STATE pThis, PAC97STATER3 pThisCC,
                                  PAC97STREAM pStream, PAC97STREAMR3 pStreamCC, bool fInTimer)
{
    RT_NOREF(fInTimer);

    PAUDMIXSINK pSink = ichac97R3IndexToSink(pThisCC, pStream->u8SD);
    AssertPtr(pSink);

    if (!AudioMixerSinkIsActive(pSink)) /* No sink available? Bail out. */
        return;

    int rc2;

    if (pStreamCC->State.Cfg.enmDir == PDMAUDIODIR_OUT) /* Output (SDO). */
    {
# ifdef VBOX_WITH_AUDIO_AC97_ASYNC_IO
        if (fInTimer)
# endif
        {
            const uint32_t cbStreamFree = ichac97R3StreamGetFree(pStreamCC);
            if (cbStreamFree)
            {
                Log3Func(("[SD%RU8] PICB=%zu (%RU64ms), cbFree=%zu (%RU64ms), cbTransferChunk=%zu (%RU64ms)\n",
                          pStream->u8SD,
                          (pStream->Regs.picb << 1), DrvAudioHlpBytesToMilli((pStream->Regs.picb << 1), &pStreamCC->State.Cfg.Props),
                          cbStreamFree, DrvAudioHlpBytesToMilli(cbStreamFree, &pStreamCC->State.Cfg.Props),
                          pStreamCC->State.cbTransferChunk, DrvAudioHlpBytesToMilli(pStreamCC->State.cbTransferChunk, &pStreamCC->State.Cfg.Props)));

                /* Do the DMA transfer. */
                rc2 = ichac97R3StreamTransfer(pDevIns, pThis, pStream, pStreamCC,
                                              RT_MIN(pStreamCC->State.cbTransferChunk, cbStreamFree));
                AssertRC(rc2);

                pStreamCC->State.tsLastUpdateNs = RTTimeNanoTS();
            }
        }

        Log3Func(("[SD%RU8] fInTimer=%RTbool\n", pStream->u8SD, fInTimer));

# ifdef VBOX_WITH_AUDIO_AC97_ASYNC_IO
        rc2 = ichac97R3StreamAsyncIONotify(pStreamCC);
        AssertRC(rc2);
# endif

# ifdef VBOX_WITH_AUDIO_AC97_ASYNC_IO
        if (!fInTimer) /* In async I/O thread */
        {
# endif
            const uint32_t cbSinkWritable     = AudioMixerSinkGetWritable(pSink);
            const uint32_t cbStreamReadable   = ichac97R3StreamGetUsed(pStreamCC);
            const uint32_t cbToReadFromStream = RT_MIN(cbStreamReadable, cbSinkWritable);

            Log3Func(("[SD%RU8] cbSinkWritable=%RU32, cbStreamReadable=%RU32\n", pStream->u8SD, cbSinkWritable, cbStreamReadable));

            if (cbToReadFromStream)
            {
                /* Read (guest output) data and write it to the stream's sink. */
                rc2 = ichac97R3StreamRead(pStreamCC, pSink, cbToReadFromStream, NULL /* pcbRead */);
                AssertRC(rc2);
            }
# ifdef VBOX_WITH_AUDIO_AC97_ASYNC_IO
        }
#endif
        /* When running synchronously, update the associated sink here.
         * Otherwise this will be done in the async I/O thread. */
        rc2 = AudioMixerSinkUpdate(pSink);
        AssertRC(rc2);
    }
    else /* Input (SDI). */
    {
# ifdef VBOX_WITH_AUDIO_AC97_ASYNC_IO
        if (!fInTimer)
        {
# endif
            rc2 = AudioMixerSinkUpdate(pSink);
            AssertRC(rc2);

            /* Is the sink ready to be read (host input data) from? If so, by how much? */
            uint32_t cbSinkReadable = AudioMixerSinkGetReadable(pSink);

            /* How much (guest input) data is available for writing at the moment for the AC'97 stream? */
            uint32_t cbStreamFree = ichac97R3StreamGetFree(pStreamCC);

            Log3Func(("[SD%RU8] cbSinkReadable=%RU32, cbStreamFree=%RU32\n", pStream->u8SD, cbSinkReadable, cbStreamFree));

            /* Do not read more than the sink can provide at the moment.
             * The host sets the overall pace. */
            if (cbSinkReadable > cbStreamFree)
                cbSinkReadable = cbStreamFree;

            if (cbSinkReadable)
            {
                /* Write (guest input) data to the stream which was read from stream's sink before. */
                rc2 = ichac97R3StreamWrite(pStreamCC, pSink, cbSinkReadable, NULL /* pcbWritten */);
                AssertRC(rc2);
            }
# ifdef VBOX_WITH_AUDIO_AC97_ASYNC_IO
        }
        else /* fInTimer */
        {
# endif

# ifdef VBOX_WITH_AUDIO_AC97_ASYNC_IO
            const uint64_t tsNowNs = RTTimeNanoTS();
            if (tsNowNs - pStreamCC->State.tsLastUpdateNs >= pStreamCC->State.Cfg.Device.cMsSchedulingHint * RT_NS_1MS)
            {
                rc2 = ichac97R3StreamAsyncIONotify(pStreamCC);
                AssertRC(rc2);

                pStreamCC->State.tsLastUpdateNs = tsNowNs;
            }
# endif

            const uint32_t cbStreamUsed = ichac97R3StreamGetUsed(pStreamCC);
            if (cbStreamUsed)
            {
                /* When running synchronously, do the DMA data transfers here.
                 * Otherwise this will be done in the stream's async I/O thread. */
                rc2 = ichac97R3StreamTransfer(pDevIns, pThis, pStream, pStreamCC, cbStreamUsed);
                AssertRC(rc2);
            }
# ifdef VBOX_WITH_AUDIO_AC97_ASYNC_IO
        }
# endif
    }
}

#endif /* IN_RING3 */

/**
 * Sets a AC'97 mixer control to a specific value.
 *
 * @returns IPRT status code.
 * @param   pThis               The shared AC'97 state.
 * @param   uMixerIdx           Mixer control to set value for.
 * @param   uVal                Value to set.
 */
static void ichac97MixerSet(PAC97STATE pThis, uint8_t uMixerIdx, uint16_t uVal)
{
    AssertMsgReturnVoid(uMixerIdx + 2U <= sizeof(pThis->mixer_data),
                         ("Index %RU8 out of bounds (%zu)\n", uMixerIdx, sizeof(pThis->mixer_data)));

    LogRel2(("AC97: Setting mixer index #%RU8 to %RU16 (%RU8 %RU8)\n",
             uMixerIdx, uVal, RT_HI_U8(uVal), RT_LO_U8(uVal)));

    pThis->mixer_data[uMixerIdx + 0] = RT_LO_U8(uVal);
    pThis->mixer_data[uMixerIdx + 1] = RT_HI_U8(uVal);
}

/**
 * Gets a value from a specific AC'97 mixer control.
 *
 * @returns Retrieved mixer control value.
 * @param   pThis               The shared AC'97 state.
 * @param   uMixerIdx           Mixer control to get value for.
 */
static uint16_t ichac97MixerGet(PAC97STATE pThis, uint32_t uMixerIdx)
{
    AssertMsgReturn(uMixerIdx + 2U <= sizeof(pThis->mixer_data),
                    ("Index %RU8 out of bounds (%zu)\n", uMixerIdx, sizeof(pThis->mixer_data)),
                    UINT16_MAX);
    return RT_MAKE_U16(pThis->mixer_data[uMixerIdx + 0], pThis->mixer_data[uMixerIdx + 1]);
}

#ifdef IN_RING3

/**
 * Retrieves a specific driver stream of a AC'97 driver.
 *
 * @returns Pointer to driver stream if found, or NULL if not found.
 * @param   pDrv                Driver to retrieve driver stream for.
 * @param   enmDir              Stream direction to retrieve.
 * @param   dstSrc              Stream destination / source to retrieve.
 */
static PAC97DRIVERSTREAM ichac97R3MixerGetDrvStream(PAC97DRIVER pDrv, PDMAUDIODIR enmDir, PDMAUDIODSTSRCUNION dstSrc)
{
    PAC97DRIVERSTREAM pDrvStream = NULL;

    if (enmDir == PDMAUDIODIR_IN)
    {
        LogFunc(("enmRecSource=%d\n", dstSrc.enmSrc));

        switch (dstSrc.enmSrc)
        {
            case PDMAUDIORECSRC_LINE:
                pDrvStream = &pDrv->LineIn;
                break;
            case PDMAUDIORECSRC_MIC:
                pDrvStream = &pDrv->MicIn;
                break;
            default:
                AssertFailed();
                break;
        }
    }
    else if (enmDir == PDMAUDIODIR_OUT)
    {
        LogFunc(("enmPlaybackDest=%d\n", dstSrc.enmDst));

        switch (dstSrc.enmDst)
        {
            case PDMAUDIOPLAYBACKDST_FRONT:
                pDrvStream = &pDrv->Out;
                break;
            default:
                AssertFailed();
                break;
        }
    }
    else
        AssertFailed();

    return pDrvStream;
}

/**
 * Adds a driver stream to a specific mixer sink.
 *
 * @returns IPRT status code.
 * @param   pMixSink            Mixer sink to add driver stream to.
 * @param   pCfg                Stream configuration to use.
 * @param   pDrv                Driver stream to add.
 */
static int ichac97R3MixerAddDrvStream(PAUDMIXSINK pMixSink, PPDMAUDIOSTREAMCFG pCfg, PAC97DRIVER pDrv)
{
    AssertPtrReturn(pMixSink, VERR_INVALID_POINTER);

    PPDMAUDIOSTREAMCFG pStreamCfg = DrvAudioHlpStreamCfgDup(pCfg);
    if (!pStreamCfg)
        return VERR_NO_MEMORY;

    if (!RTStrPrintf(pStreamCfg->szName, sizeof(pStreamCfg->szName), "%s", pCfg->szName))
    {
        DrvAudioHlpStreamCfgFree(pStreamCfg);
        return VERR_BUFFER_OVERFLOW;
    }

    LogFunc(("[LUN#%RU8] %s\n", pDrv->uLUN, pStreamCfg->szName));

    int rc;

    PAC97DRIVERSTREAM pDrvStream = ichac97R3MixerGetDrvStream(pDrv, pStreamCfg->enmDir, pStreamCfg->u);
    if (pDrvStream)
    {
        AssertMsg(pDrvStream->pMixStrm == NULL, ("[LUN#%RU8] Driver stream already present when it must not\n", pDrv->uLUN));

        PAUDMIXSTREAM pMixStrm;
        rc = AudioMixerSinkCreateStream(pMixSink, pDrv->pConnector, pStreamCfg, 0 /* fFlags */, &pMixStrm);
        LogFlowFunc(("LUN#%RU8: Created stream \"%s\" for sink, rc=%Rrc\n", pDrv->uLUN, pStreamCfg->szName, rc));
        if (RT_SUCCESS(rc))
        {
            rc = AudioMixerSinkAddStream(pMixSink, pMixStrm);
            LogFlowFunc(("LUN#%RU8: Added stream \"%s\" to sink, rc=%Rrc\n", pDrv->uLUN, pStreamCfg->szName, rc));
            if (RT_SUCCESS(rc))
            {
                /* If this is an input stream, always set the latest (added) stream
                 * as the recording source. */
                /** @todo Make the recording source dynamic (CFGM?). */
                if (pStreamCfg->enmDir == PDMAUDIODIR_IN)
                {
                    PDMAUDIOBACKENDCFG Cfg;
                    rc = pDrv->pConnector->pfnGetConfig(pDrv->pConnector, &Cfg);
                    if (RT_SUCCESS(rc))
                    {
                        if (Cfg.cMaxStreamsIn) /* At least one input source available? */
                        {
                            rc = AudioMixerSinkSetRecordingSource(pMixSink, pMixStrm);
                            LogFlowFunc(("LUN#%RU8: Recording source for '%s' -> '%s', rc=%Rrc\n",
                                         pDrv->uLUN, pStreamCfg->szName, Cfg.szName, rc));

                            if (RT_SUCCESS(rc))
                                LogRel2(("AC97: Set recording source for '%s' to '%s'\n", pStreamCfg->szName, Cfg.szName));
                        }
                        else
                            LogRel(("AC97: Backend '%s' currently is not offering any recording source for '%s'\n",
                                    Cfg.szName, pStreamCfg->szName));
                    }
                    else if (RT_FAILURE(rc))
                        LogFunc(("LUN#%RU8: Unable to retrieve backend configuratio for '%s', rc=%Rrc\n",
                                 pDrv->uLUN, pStreamCfg->szName, rc));
                }
                /** @todo r=bird: see below. */
                if (RT_FAILURE(rc))
                    AudioMixerSinkRemoveStream(pMixSink, pMixStrm);
            }
            /** @todo r=bird: I've added this destroy stuff here, because if it looks as if
             * you just drop the stream if the AudioMixerSinkAddStream fails for some
             * reason.  This is definitely true if AudioMixerSinkSetRecordingSource fails
             * above, because it leads to duplicate statistics when starting XP with ICH97
             * and VRDP enabled.  Looks like the VRDP line-in fails with
             * VERR_AUDIO_STREAM_NOT_READY when configured for 8000HZ, then it asserts in
             * STAM when 48000Hz is configured right afterwards. */
            if (RT_FAILURE(rc))
                AudioMixerStreamDestroy(pMixStrm);
        }

        if (RT_SUCCESS(rc))
            pDrvStream->pMixStrm = pMixStrm;
    }
    else
        rc = VERR_INVALID_PARAMETER;

    DrvAudioHlpStreamCfgFree(pStreamCfg);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Adds all current driver streams to a specific mixer sink.
 *
 * @returns IPRT status code.
 * @param   pThisCC             The ring-3 AC'97 state.
 * @param   pMixSink            Mixer sink to add stream to.
 * @param   pCfg                Stream configuration to use.
 */
static int ichac97R3MixerAddDrvStreams(PAC97STATER3 pThisCC, PAUDMIXSINK pMixSink, PPDMAUDIOSTREAMCFG pCfg)
{
    AssertPtrReturn(pMixSink, VERR_INVALID_POINTER);

    if (!DrvAudioHlpStreamCfgIsValid(pCfg))
        return VERR_INVALID_PARAMETER;

    int rc = AudioMixerSinkSetFormat(pMixSink, &pCfg->Props);
    if (RT_FAILURE(rc))
        return rc;

    PAC97DRIVER pDrv;
    RTListForEach(&pThisCC->lstDrv, pDrv, AC97DRIVER, Node)
    {
        int rc2 = ichac97R3MixerAddDrvStream(pMixSink, pCfg, pDrv);
        if (RT_FAILURE(rc2))
            LogFunc(("Attaching stream failed with %Rrc\n", rc2));

        /* Do not pass failure to rc here, as there might be drivers which aren't
         * configured / ready yet. */
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Adds a specific AC'97 driver to the driver chain.
 *
 * @return IPRT status code.
 * @param  pThisCC              The ring-3 AC'97 device state.
 * @param  pDrv                 The AC'97 driver to add.
 */
static int ichac97R3MixerAddDrv(PAC97STATER3 pThisCC, PAC97DRIVER pDrv)
{
    int rc = VINF_SUCCESS;

    if (DrvAudioHlpStreamCfgIsValid(&pThisCC->aStreams[AC97SOUNDSOURCE_PI_INDEX].State.Cfg))
        rc = ichac97R3MixerAddDrvStream(pThisCC->pSinkLineIn, &pThisCC->aStreams[AC97SOUNDSOURCE_PI_INDEX].State.Cfg, pDrv);

    if (DrvAudioHlpStreamCfgIsValid(&pThisCC->aStreams[AC97SOUNDSOURCE_PO_INDEX].State.Cfg))
    {
        int rc2 = ichac97R3MixerAddDrvStream(pThisCC->pSinkOut, &pThisCC->aStreams[AC97SOUNDSOURCE_PO_INDEX].State.Cfg, pDrv);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    if (DrvAudioHlpStreamCfgIsValid(&pThisCC->aStreams[AC97SOUNDSOURCE_MC_INDEX].State.Cfg))
    {
        int rc2 = ichac97R3MixerAddDrvStream(pThisCC->pSinkMicIn, &pThisCC->aStreams[AC97SOUNDSOURCE_MC_INDEX].State.Cfg, pDrv);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    return rc;
}

/**
 * Removes a specific AC'97 driver from the driver chain and destroys its
 * associated streams.
 *
 * @param pThisCC               The ring-3 AC'97 device state.
 * @param pDrv                  AC'97 driver to remove.
 */
static void ichac97R3MixerRemoveDrv(PAC97STATER3 pThisCC, PAC97DRIVER pDrv)
{
    if (pDrv->MicIn.pMixStrm)
    {
        if (AudioMixerSinkGetRecordingSource(pThisCC->pSinkMicIn) == pDrv->MicIn.pMixStrm)
            AudioMixerSinkSetRecordingSource(pThisCC->pSinkMicIn, NULL);

        AudioMixerSinkRemoveStream(pThisCC->pSinkMicIn,  pDrv->MicIn.pMixStrm);
        AudioMixerStreamDestroy(pDrv->MicIn.pMixStrm);
        pDrv->MicIn.pMixStrm = NULL;
    }

    if (pDrv->LineIn.pMixStrm)
    {
        if (AudioMixerSinkGetRecordingSource(pThisCC->pSinkLineIn) == pDrv->LineIn.pMixStrm)
            AudioMixerSinkSetRecordingSource(pThisCC->pSinkLineIn, NULL);

        AudioMixerSinkRemoveStream(pThisCC->pSinkLineIn, pDrv->LineIn.pMixStrm);
        AudioMixerStreamDestroy(pDrv->LineIn.pMixStrm);
        pDrv->LineIn.pMixStrm = NULL;
    }

    if (pDrv->Out.pMixStrm)
    {
        AudioMixerSinkRemoveStream(pThisCC->pSinkOut,    pDrv->Out.pMixStrm);
        AudioMixerStreamDestroy(pDrv->Out.pMixStrm);
        pDrv->Out.pMixStrm = NULL;
    }

    RTListNodeRemove(&pDrv->Node);
}

/**
 * Removes a driver stream from a specific mixer sink.
 *
 * @param   pMixSink            Mixer sink to remove audio streams from.
 * @param   enmDir              Stream direction to remove.
 * @param   dstSrc              Stream destination / source to remove.
 * @param   pDrv                Driver stream to remove.
 */
static void ichac97R3MixerRemoveDrvStream(PAUDMIXSINK pMixSink, PDMAUDIODIR enmDir, PDMAUDIODSTSRCUNION dstSrc, PAC97DRIVER pDrv)
{
    PAC97DRIVERSTREAM pDrvStream = ichac97R3MixerGetDrvStream(pDrv, enmDir, dstSrc);
    if (pDrvStream)
    {
        if (pDrvStream->pMixStrm)
        {
            AudioMixerSinkRemoveStream(pMixSink, pDrvStream->pMixStrm);

            AudioMixerStreamDestroy(pDrvStream->pMixStrm);
            pDrvStream->pMixStrm = NULL;
        }
    }
}

/**
 * Removes all driver streams from a specific mixer sink.
 *
 * @param   pThisCC             The ring-3 AC'97 state.
 * @param   pMixSink            Mixer sink to remove audio streams from.
 * @param   enmDir              Stream direction to remove.
 * @param   dstSrc              Stream destination / source to remove.
 */
static void ichac97R3MixerRemoveDrvStreams(PAC97STATER3 pThisCC, PAUDMIXSINK pMixSink,
                                           PDMAUDIODIR enmDir, PDMAUDIODSTSRCUNION dstSrc)
{
    AssertPtrReturnVoid(pMixSink);

    PAC97DRIVER pDrv;
    RTListForEach(&pThisCC->lstDrv, pDrv, AC97DRIVER, Node)
    {
        ichac97R3MixerRemoveDrvStream(pMixSink, enmDir, dstSrc, pDrv);
    }
}

/**
 * Calculates and returns the ticks for a specified amount of bytes.
 *
 * @returns Calculated ticks
 * @param   pDevIns             The device instance.
 * @param   pStream             AC'97 stream to calculate ticks for (shared).
 * @param   pStreamCC           AC'97 stream to calculate ticks for (ring-3).
 * @param   cbBytes             Bytes to calculate ticks for.
 */
static uint64_t ichac97R3StreamTransferCalcNext(PPDMDEVINS pDevIns, PAC97STREAM pStream, PAC97STREAMR3 pStreamCC, uint32_t cbBytes)
{
    if (!cbBytes)
        return 0;

    const uint64_t usBytes        = DrvAudioHlpBytesToMicro(cbBytes, &pStreamCC->State.Cfg.Props);
    const uint64_t cTransferTicks = PDMDevHlpTimerFromMicro(pDevIns, pStream->hTimer, usBytes);

    Log3Func(("[SD%RU8] Timer %uHz, cbBytes=%RU32 -> usBytes=%RU64, cTransferTicks=%RU64\n",
              pStream->u8SD, pStreamCC->State.uTimerHz, cbBytes, usBytes, cTransferTicks));

    return cTransferTicks;
}

/**
 * Updates the next transfer based on a specific amount of bytes.
 *
 * @param   pDevIns             The device instance.
 * @param   pStream             The AC'97 stream to update (shared).
 * @param   pStreamCC           The AC'97 stream to update (ring-3).
 * @param   cbBytes             Bytes to update next transfer for.
 */
static void ichac97R3StreamTransferUpdate(PPDMDEVINS pDevIns, PAC97STREAM pStream, PAC97STREAMR3 pStreamCC, uint32_t cbBytes)
{
    if (!cbBytes)
        return;

    /* Calculate the bytes we need to transfer to / from the stream's DMA per iteration.
     * This is bound to the device's Hz rate and thus to the (virtual) timing the device expects. */
    pStreamCC->State.cbTransferChunk = cbBytes;

    /* Update the transfer ticks. */
    pStreamCC->State.cTransferTicks = ichac97R3StreamTransferCalcNext(pDevIns, pStream, pStreamCC,
                                                                      pStreamCC->State.cbTransferChunk);
    Assert(pStreamCC->State.cTransferTicks); /* Paranoia. */
}

/**
 * Opens an AC'97 stream with its current mixer settings.
 *
 * This will open an AC'97 stream with 2 (stereo) channels, 16-bit samples and
 * the last set sample rate in the AC'97 mixer for this stream.
 *
 * @returns IPRT status code.
 * @param   pThis               The shared AC'97 device state (shared).
 * @param   pThisCC             The shared AC'97 device state (ring-3).
 * @param   pStream             The AC'97 stream to open (shared).
 * @param   pStreamCC           The AC'97 stream to open (ring-3).
 * @param   fForce              Whether to force re-opening the stream or not.
 *                              Otherwise re-opening only will happen if the PCM properties have changed.
 */
static int ichac97R3StreamOpen(PAC97STATE pThis, PAC97STATER3 pThisCC, PAC97STREAM pStream, PAC97STREAMR3 pStreamCC, bool fForce)
{
    PDMAUDIOSTREAMCFG Cfg;
    RT_ZERO(Cfg);
    Cfg.Props.cChannels = 2;
    Cfg.Props.cbSample  = 2 /* 16-bit */;
    Cfg.Props.fSigned   = true;
    Cfg.Props.cShift    = PDMAUDIOPCMPROPS_MAKE_SHIFT_PARMS(Cfg.Props.cbSample, Cfg.Props.cChannels);

    int rc = VINF_SUCCESS;
    PAUDMIXSINK pMixSink;
    switch (pStream->u8SD)
    {
        case AC97SOUNDSOURCE_PI_INDEX:
        {
            Cfg.Props.uHz   = ichac97MixerGet(pThis, AC97_PCM_LR_ADC_Rate);
            Cfg.enmDir      = PDMAUDIODIR_IN;
            Cfg.u.enmSrc    = PDMAUDIORECSRC_LINE;
            Cfg.enmLayout   = PDMAUDIOSTREAMLAYOUT_NON_INTERLEAVED;
            RTStrCopy(Cfg.szName, sizeof(Cfg.szName), "Line-In");

            pMixSink        = pThisCC->pSinkLineIn;
            break;
        }

        case AC97SOUNDSOURCE_MC_INDEX:
        {
            Cfg.Props.uHz   = ichac97MixerGet(pThis, AC97_MIC_ADC_Rate);
            Cfg.enmDir      = PDMAUDIODIR_IN;
            Cfg.u.enmSrc    = PDMAUDIORECSRC_MIC;
            Cfg.enmLayout   = PDMAUDIOSTREAMLAYOUT_NON_INTERLEAVED;
            RTStrCopy(Cfg.szName, sizeof(Cfg.szName), "Mic-In");

            pMixSink        = pThisCC->pSinkMicIn;
            break;
        }

        case AC97SOUNDSOURCE_PO_INDEX:
        {
            Cfg.Props.uHz   = ichac97MixerGet(pThis, AC97_PCM_Front_DAC_Rate);
            Cfg.enmDir      = PDMAUDIODIR_OUT;
            Cfg.u.enmDst    = PDMAUDIOPLAYBACKDST_FRONT;
            Cfg.enmLayout   = PDMAUDIOSTREAMLAYOUT_NON_INTERLEAVED;
            RTStrCopy(Cfg.szName, sizeof(Cfg.szName), "Output");

            pMixSink        = pThisCC->pSinkOut;
            break;
        }

        default:
            rc = VERR_NOT_SUPPORTED;
            pMixSink = NULL;
            break;
    }

    if (RT_SUCCESS(rc))
    {
        /* Only (re-)create the stream (and driver chain) if we really have to.
         * Otherwise avoid this and just reuse it, as this costs performance. */
        if (   !DrvAudioHlpPCMPropsAreEqual(&Cfg.Props, &pStreamCC->State.Cfg.Props)
            || fForce)
        {
            LogRel2(("AC97: (Re-)Opening stream '%s' (%RU32Hz, %RU8 channels, %s%RU8)\n",
                     Cfg.szName, Cfg.Props.uHz, Cfg.Props.cChannels, Cfg.Props.fSigned ? "S" : "U", Cfg.Props.cbSample * 8));

            LogFlowFunc(("[SD%RU8] uHz=%RU32\n", pStream->u8SD, Cfg.Props.uHz));

            if (Cfg.Props.uHz)
            {
                Assert(Cfg.enmDir != PDMAUDIODIR_UNKNOWN);

                /*
                 * Set the stream's timer Hz rate, based on the PCM properties Hz rate.
                 */
                if (pThis->uTimerHz == AC97_TIMER_HZ_DEFAULT) /* Make sure that we don't have any custom Hz rate set we want to enforce */
                {
                    if (Cfg.Props.uHz > 44100) /* E.g. 48000 Hz. */
                        pStreamCC->State.uTimerHz = 200;
                    else /* Just take the global Hz rate otherwise. */
                        pStreamCC->State.uTimerHz = pThis->uTimerHz;
                }
                else
                    pStreamCC->State.uTimerHz = pThis->uTimerHz;

                /* Set scheduling hint (if available). */
                if (pStreamCC->State.uTimerHz)
                    Cfg.Device.cMsSchedulingHint = 1000 /* ms */ / pStreamCC->State.uTimerHz;

                if (pStreamCC->State.pCircBuf)
                {
                    RTCircBufDestroy(pStreamCC->State.pCircBuf);
                    pStreamCC->State.pCircBuf = NULL;
                }

                rc = RTCircBufCreate(&pStreamCC->State.pCircBuf, DrvAudioHlpMilliToBytes(100 /* ms */, &Cfg.Props)); /** @todo Make this configurable. */
                if (RT_SUCCESS(rc))
                {
                    ichac97R3MixerRemoveDrvStreams(pThisCC, pMixSink, Cfg.enmDir, Cfg.u);

                    rc = ichac97R3MixerAddDrvStreams(pThisCC, pMixSink, &Cfg);
                    if (RT_SUCCESS(rc))
                        rc = DrvAudioHlpStreamCfgCopy(&pStreamCC->State.Cfg, &Cfg);
                }
            }
        }
        else
            LogFlowFunc(("[SD%RU8] Skipping (re-)creation\n", pStream->u8SD));
    }

    LogFlowFunc(("[SD%RU8] rc=%Rrc\n", pStream->u8SD, rc));
    return rc;
}

/**
 * Closes an AC'97 stream.
 *
 * @returns IPRT status code.
 * @param   pStream             The AC'97 stream to close (shared).
 */
static int ichac97R3StreamClose(PAC97STREAM pStream)
{
    RT_NOREF(pStream);
    LogFlowFunc(("[SD%RU8]\n", pStream->u8SD));
    return VINF_SUCCESS;
}

/**
 * Re-opens (that is, closes and opens again) an AC'97 stream on the backend
 * side with the current AC'97 mixer settings for this stream.
 *
 * @returns IPRT status code.
 * @param   pThis               The shared AC'97 device state.
 * @param   pThisCC             The ring-3 AC'97 device state.
 * @param   pStream             The AC'97 stream to re-open (shared).
 * @param   pStreamCC           The AC'97 stream to re-open (ring-3).
 * @param   fForce              Whether to force re-opening the stream or not.
 *                              Otherwise re-opening only will happen if the PCM properties have changed.
 */
static int ichac97R3StreamReOpen(PAC97STATE pThis, PAC97STATER3 pThisCC,
                                 PAC97STREAM pStream, PAC97STREAMR3 pStreamCC, bool fForce)
{
    LogFlowFunc(("[SD%RU8]\n", pStream->u8SD));
    Assert(pStream->u8SD == pStreamCC->u8SD);
    Assert(pStream   - &pThis->aStreams[0]   == pStream->u8SD);
    Assert(pStreamCC - &pThisCC->aStreams[0] == pStream->u8SD);

    int rc = ichac97R3StreamClose(pStream);
    if (RT_SUCCESS(rc))
        rc = ichac97R3StreamOpen(pThis, pThisCC, pStream, pStreamCC, fForce);

    return rc;
}

/**
 * Locks an AC'97 stream for serialized access.
 *
 * @returns IPRT status code.
 * @param   pStreamCC           The AC'97 stream to lock (ring-3).
 */
static void ichac97R3StreamLock(PAC97STREAMR3 pStreamCC)
{
    int rc2 = RTCritSectEnter(&pStreamCC->State.CritSect);
    AssertRC(rc2);
}

/**
 * Unlocks a formerly locked AC'97 stream.
 *
 * @returns IPRT status code.
 * @param   pStreamCC           The AC'97 stream to unlock (ring-3).
 */
static void ichac97R3StreamUnlock(PAC97STREAMR3 pStreamCC)
{
    int rc2 = RTCritSectLeave(&pStreamCC->State.CritSect);
    AssertRC(rc2);
}

/**
 * Retrieves the available size of (buffered) audio data (in bytes) of a given AC'97 stream.
 *
 * @returns Available data (in bytes).
 * @param   pStreamCC           The AC'97 stream to retrieve size for (ring-3).
 */
static uint32_t ichac97R3StreamGetUsed(PAC97STREAMR3 pStreamCC)
{
    if (!pStreamCC->State.pCircBuf)
        return 0;

    return (uint32_t)RTCircBufUsed(pStreamCC->State.pCircBuf);
}

/**
 * Retrieves the free size of audio data (in bytes) of a given AC'97 stream.
 *
 * @returns Free data (in bytes).
 * @param   pStreamCC           AC'97 stream to retrieve size for (ring-3).
 */
static uint32_t ichac97R3StreamGetFree(PAC97STREAMR3 pStreamCC)
{
    if (!pStreamCC->State.pCircBuf)
        return 0;

    return (uint32_t)RTCircBufFree(pStreamCC->State.pCircBuf);
}

/**
 * Sets the volume of a specific AC'97 mixer control.
 *
 * This currently only supports attenuation -- gain support is currently not implemented.
 *
 * @returns IPRT status code.
 * @param   pThis               The shared AC'97 state.
 * @param   pThisCC             The ring-3 AC'97 state.
 * @param   index               AC'97 mixer index to set volume for.
 * @param   enmMixerCtl         Corresponding audio mixer sink.
 * @param   uVal                Volume value to set.
 */
static int ichac97R3MixerSetVolume(PAC97STATE pThis, PAC97STATER3 pThisCC, int index, PDMAUDIOMIXERCTL enmMixerCtl, uint32_t uVal)
{
    /*
     * From AC'97 SoundMax Codec AD1981A/AD1981B:
     * "Because AC '97 defines 6-bit volume registers, to maintain compatibility whenever the
     *  D5 or D13 bits are set to 1, their respective lower five volume bits are automatically
     *  set to 1 by the Codec logic. On readback, all lower 5 bits will read ones whenever
     *  these bits are set to 1."
     *
     * Linux ALSA depends on this behavior to detect that only 5 bits are used for volume
     * control and the optional 6th bit is not used. Note that this logic only applies to the
     * master volume controls.
     */
    if (index == AC97_Master_Volume_Mute || index == AC97_Headphone_Volume_Mute || index == AC97_Master_Volume_Mono_Mute)
    {
        if (uVal & RT_BIT(5))  /* D5 bit set? */
            uVal |= RT_BIT(4) | RT_BIT(3) | RT_BIT(2) | RT_BIT(1) | RT_BIT(0);
        if (uVal & RT_BIT(13)) /* D13 bit set? */
            uVal |= RT_BIT(12) | RT_BIT(11) | RT_BIT(10) | RT_BIT(9) | RT_BIT(8);
    }

    const bool  fCtlMuted    = (uVal >> AC97_BARS_VOL_MUTE_SHIFT) & 1;
    uint8_t     uCtlAttLeft  = (uVal >> 8) & AC97_BARS_VOL_MASK;
    uint8_t     uCtlAttRight = uVal & AC97_BARS_VOL_MASK;

    /* For the master and headphone volume, 0 corresponds to 0dB attenuation. For the other
     * volume controls, 0 means 12dB gain and 8 means unity gain.
     */
    if (index != AC97_Master_Volume_Mute && index != AC97_Headphone_Volume_Mute)
    {
# ifndef VBOX_WITH_AC97_GAIN_SUPPORT
        /* NB: Currently there is no gain support, only attenuation. */
        uCtlAttLeft  = uCtlAttLeft  < 8 ? 0 : uCtlAttLeft  - 8;
        uCtlAttRight = uCtlAttRight < 8 ? 0 : uCtlAttRight - 8;
# endif
    }
    Assert(uCtlAttLeft  <= 255 / AC97_DB_FACTOR);
    Assert(uCtlAttRight <= 255 / AC97_DB_FACTOR);

    LogFunc(("index=0x%x, uVal=%RU32, enmMixerCtl=%RU32\n", index, uVal, enmMixerCtl));
    LogFunc(("uCtlAttLeft=%RU8, uCtlAttRight=%RU8 ", uCtlAttLeft, uCtlAttRight));

    /*
     * For AC'97 volume controls, each additional step means -1.5dB attenuation with
     * zero being maximum. In contrast, we're internally using 255 (PDMAUDIO_VOLUME_MAX)
     * steps, each -0.375dB, where 0 corresponds to -96dB and 255 corresponds to 0dB.
     */
    uint8_t lVol = PDMAUDIO_VOLUME_MAX - uCtlAttLeft  * AC97_DB_FACTOR;
    uint8_t rVol = PDMAUDIO_VOLUME_MAX - uCtlAttRight * AC97_DB_FACTOR;

    Log(("-> fMuted=%RTbool, lVol=%RU8, rVol=%RU8\n", fCtlMuted, lVol, rVol));

    int rc = VINF_SUCCESS;

    if (pThisCC->pMixer) /* Device can be in reset state, so no mixer available. */
    {
        PDMAUDIOVOLUME Vol   = { fCtlMuted, lVol, rVol };
        PAUDMIXSINK    pSink = NULL;

        switch (enmMixerCtl)
        {
            case PDMAUDIOMIXERCTL_VOLUME_MASTER:
                rc = AudioMixerSetMasterVolume(pThisCC->pMixer, &Vol);
                break;

            case PDMAUDIOMIXERCTL_FRONT:
                pSink = pThisCC->pSinkOut;
                break;

            case PDMAUDIOMIXERCTL_MIC_IN:
            case PDMAUDIOMIXERCTL_LINE_IN:
                /* These are recognized but do nothing. */
                break;

            default:
                AssertFailed();
                rc = VERR_NOT_SUPPORTED;
                break;
        }

        if (pSink)
            rc = AudioMixerSinkSetVolume(pSink, &Vol);
    }

    ichac97MixerSet(pThis, index, uVal);

    if (RT_FAILURE(rc))
        LogFlowFunc(("Failed with %Rrc\n", rc));

    return rc;
}

/**
 * Sets the gain of a specific AC'97 recording control.
 *
 * NB: gain support is currently not implemented in PDM audio.
 *
 * @returns IPRT status code.
 * @param   pThis               The shared AC'97 state.
 * @param   pThisCC             The ring-3 AC'97 state.
 * @param   index               AC'97 mixer index to set volume for.
 * @param   enmMixerCtl         Corresponding audio mixer sink.
 * @param   uVal                Volume value to set.
 */
static int ichac97R3MixerSetGain(PAC97STATE pThis, PAC97STATER3 pThisCC, int index, PDMAUDIOMIXERCTL enmMixerCtl, uint32_t uVal)
{
    /*
     * For AC'97 recording controls, each additional step means +1.5dB gain with
     * zero being 0dB gain and 15 being +22.5dB gain.
     */
    const bool  fCtlMuted     = (uVal >> AC97_BARS_VOL_MUTE_SHIFT) & 1;
    uint8_t     uCtlGainLeft  = (uVal >> 8) & AC97_BARS_GAIN_MASK;
    uint8_t     uCtlGainRight = uVal & AC97_BARS_GAIN_MASK;

    Assert(uCtlGainLeft  <= 255 / AC97_DB_FACTOR);
    Assert(uCtlGainRight <= 255 / AC97_DB_FACTOR);

    LogFunc(("index=0x%x, uVal=%RU32, enmMixerCtl=%RU32\n", index, uVal, enmMixerCtl));
    LogFunc(("uCtlGainLeft=%RU8, uCtlGainRight=%RU8 ", uCtlGainLeft, uCtlGainRight));

    uint8_t lVol = PDMAUDIO_VOLUME_MAX + uCtlGainLeft  * AC97_DB_FACTOR;
    uint8_t rVol = PDMAUDIO_VOLUME_MAX + uCtlGainRight * AC97_DB_FACTOR;

    /* We do not currently support gain. Since AC'97 does not support attenuation
     * for the recording input, the best we can do is set the maximum volume.
     */
# ifndef VBOX_WITH_AC97_GAIN_SUPPORT
    /* NB: Currently there is no gain support, only attenuation. Since AC'97 does not
     * support attenuation for the recording inputs, the best we can do is set the
     * maximum volume.
     */
    lVol = rVol = PDMAUDIO_VOLUME_MAX;
# endif

    Log(("-> fMuted=%RTbool, lVol=%RU8, rVol=%RU8\n", fCtlMuted, lVol, rVol));

    int rc = VINF_SUCCESS;

    if (pThisCC->pMixer) /* Device can be in reset state, so no mixer available. */
    {
        PDMAUDIOVOLUME Vol   = { fCtlMuted, lVol, rVol };
        PAUDMIXSINK    pSink = NULL;

        switch (enmMixerCtl)
        {
            case PDMAUDIOMIXERCTL_MIC_IN:
                pSink = pThisCC->pSinkMicIn;
                break;

            case PDMAUDIOMIXERCTL_LINE_IN:
                pSink = pThisCC->pSinkLineIn;
                break;

            default:
                AssertFailed();
                rc = VERR_NOT_SUPPORTED;
                break;
        }

        if (pSink) {
            rc = AudioMixerSinkSetVolume(pSink, &Vol);
            /* There is only one AC'97 recording gain control. If line in
             * is changed, also update the microphone. If the optional dedicated
             * microphone is changed, only change that.
             * NB: The codecs we support do not have the dedicated microphone control.
             */
            if ((pSink == pThisCC->pSinkLineIn) && pThisCC->pSinkMicIn)
                rc = AudioMixerSinkSetVolume(pSink, &Vol);
        }
    }

    ichac97MixerSet(pThis, index, uVal);

    if (RT_FAILURE(rc))
        LogFlowFunc(("Failed with %Rrc\n", rc));

    return rc;
}

/**
 * Converts an AC'97 recording source index to a PDM audio recording source.
 *
 * @returns PDM audio recording source.
 * @param   uIdx                AC'97 index to convert.
 */
static PDMAUDIORECSRC ichac97R3IdxToRecSource(uint8_t uIdx)
{
    switch (uIdx)
    {
        case AC97_REC_MIC:     return PDMAUDIORECSRC_MIC;
        case AC97_REC_CD:      return PDMAUDIORECSRC_CD;
        case AC97_REC_VIDEO:   return PDMAUDIORECSRC_VIDEO;
        case AC97_REC_AUX:     return PDMAUDIORECSRC_AUX;
        case AC97_REC_LINE_IN: return PDMAUDIORECSRC_LINE;
        case AC97_REC_PHONE:   return PDMAUDIORECSRC_PHONE;
        default:
            break;
    }

    LogFlowFunc(("Unknown record source %d, using MIC\n", uIdx));
    return PDMAUDIORECSRC_MIC;
}

/**
 * Converts a PDM audio recording source to an AC'97 recording source index.
 *
 * @returns AC'97 recording source index.
 * @param   enmRecSrc           PDM audio recording source to convert.
 */
static uint8_t ichac97R3RecSourceToIdx(PDMAUDIORECSRC enmRecSrc)
{
    switch (enmRecSrc)
    {
        case PDMAUDIORECSRC_MIC:     return AC97_REC_MIC;
        case PDMAUDIORECSRC_CD:      return AC97_REC_CD;
        case PDMAUDIORECSRC_VIDEO:   return AC97_REC_VIDEO;
        case PDMAUDIORECSRC_AUX:     return AC97_REC_AUX;
        case PDMAUDIORECSRC_LINE:    return AC97_REC_LINE_IN;
        case PDMAUDIORECSRC_PHONE:   return AC97_REC_PHONE;
        default:
            break;
    }

    LogFlowFunc(("Unknown audio recording source %d using MIC\n", enmRecSrc));
    return AC97_REC_MIC;
}

/**
 * Returns the audio direction of a specified stream descriptor.
 *
 * @return  Audio direction.
 */
DECLINLINE(PDMAUDIODIR) ichac97GetDirFromSD(uint8_t uSD)
{
    switch (uSD)
    {
        case AC97SOUNDSOURCE_PI_INDEX: return PDMAUDIODIR_IN;
        case AC97SOUNDSOURCE_PO_INDEX: return PDMAUDIODIR_OUT;
        case AC97SOUNDSOURCE_MC_INDEX: return PDMAUDIODIR_IN;
    }

    AssertFailed();
    return PDMAUDIODIR_UNKNOWN;
}

#endif /* IN_RING3 */

#ifdef IN_RING3

/**
 * Performs an AC'97 mixer record select to switch to a different recording
 * source.
 *
 * @param   pThis               The shared AC'97 state.
 * @param   val                 AC'97 recording source index to set.
 */
static void ichac97R3MixerRecordSelect(PAC97STATE pThis, uint32_t val)
{
    uint8_t rs = val & AC97_REC_MASK;
    uint8_t ls = (val >> 8) & AC97_REC_MASK;

    const PDMAUDIORECSRC ars = ichac97R3IdxToRecSource(rs);
    const PDMAUDIORECSRC als = ichac97R3IdxToRecSource(ls);

    rs = ichac97R3RecSourceToIdx(ars);
    ls = ichac97R3RecSourceToIdx(als);

    LogRel(("AC97: Record select to left=%s, right=%s\n", DrvAudioHlpRecSrcToStr(ars), DrvAudioHlpRecSrcToStr(als)));

    ichac97MixerSet(pThis, AC97_Record_Select, rs | (ls << 8));
}

/**
 * Resets the AC'97 mixer.
 *
 * @returns IPRT status code.
 * @param   pThis               The shared AC'97 state.
 * @param   pThisCC             The ring-3 AC'97 state.
 */
static int ichac97R3MixerReset(PAC97STATE pThis, PAC97STATER3 pThisCC)
{
    LogFlowFuncEnter();

    RT_ZERO(pThis->mixer_data);

    /* Note: Make sure to reset all registers first before bailing out on error. */

    ichac97MixerSet(pThis, AC97_Reset                   , 0x0000); /* 6940 */
    ichac97MixerSet(pThis, AC97_Master_Volume_Mono_Mute , 0x8000);
    ichac97MixerSet(pThis, AC97_PC_BEEP_Volume_Mute     , 0x0000);

    ichac97MixerSet(pThis, AC97_Phone_Volume_Mute       , 0x8008);
    ichac97MixerSet(pThis, AC97_Mic_Volume_Mute         , 0x8008);
    ichac97MixerSet(pThis, AC97_CD_Volume_Mute          , 0x8808);
    ichac97MixerSet(pThis, AC97_Aux_Volume_Mute         , 0x8808);
    ichac97MixerSet(pThis, AC97_Record_Gain_Mic_Mute    , 0x8000);
    ichac97MixerSet(pThis, AC97_General_Purpose         , 0x0000);
    ichac97MixerSet(pThis, AC97_3D_Control              , 0x0000);
    ichac97MixerSet(pThis, AC97_Powerdown_Ctrl_Stat     , 0x000f);

    /* Configure Extended Audio ID (EAID) + Control & Status (EACS) registers. */
    const uint16_t fEAID = AC97_EAID_REV1 | AC97_EACS_VRA | AC97_EACS_VRM; /* Our hardware is AC'97 rev2.3 compliant. */
    const uint16_t fEACS = AC97_EACS_VRA | AC97_EACS_VRM;                  /* Variable Rate PCM Audio (VRA) + Mic-In (VRM) capable. */

    LogRel(("AC97: Mixer reset (EAID=0x%x, EACS=0x%x)\n", fEAID, fEACS));

    ichac97MixerSet(pThis, AC97_Extended_Audio_ID,        fEAID);
    ichac97MixerSet(pThis, AC97_Extended_Audio_Ctrl_Stat, fEACS);
    ichac97MixerSet(pThis, AC97_PCM_Front_DAC_Rate      , 0xbb80 /* 48000 Hz by default */);
    ichac97MixerSet(pThis, AC97_PCM_Surround_DAC_Rate   , 0xbb80 /* 48000 Hz by default */);
    ichac97MixerSet(pThis, AC97_PCM_LFE_DAC_Rate        , 0xbb80 /* 48000 Hz by default */);
    ichac97MixerSet(pThis, AC97_PCM_LR_ADC_Rate         , 0xbb80 /* 48000 Hz by default */);
    ichac97MixerSet(pThis, AC97_MIC_ADC_Rate            , 0xbb80 /* 48000 Hz by default */);

    if (pThis->enmCodecModel == AC97CODEC_AD1980)
    {
        /* Analog Devices 1980 (AD1980) */
        ichac97MixerSet(pThis, AC97_Reset                   , 0x0010); /* Headphones. */
        ichac97MixerSet(pThis, AC97_Vendor_ID1              , 0x4144);
        ichac97MixerSet(pThis, AC97_Vendor_ID2              , 0x5370);
        ichac97MixerSet(pThis, AC97_Headphone_Volume_Mute   , 0x8000);
    }
    else if (pThis->enmCodecModel == AC97CODEC_AD1981B)
    {
        /* Analog Devices 1981B (AD1981B) */
        ichac97MixerSet(pThis, AC97_Vendor_ID1              , 0x4144);
        ichac97MixerSet(pThis, AC97_Vendor_ID2              , 0x5374);
    }
    else
    {
        /* Sigmatel 9700 (STAC9700) */
        ichac97MixerSet(pThis, AC97_Vendor_ID1              , 0x8384);
        ichac97MixerSet(pThis, AC97_Vendor_ID2              , 0x7600); /* 7608 */
    }
    ichac97R3MixerRecordSelect(pThis, 0);

    /* The default value is 8000h, which corresponds to 0 dB attenuation with mute on. */
    ichac97R3MixerSetVolume(pThis, pThisCC, AC97_Master_Volume_Mute,  PDMAUDIOMIXERCTL_VOLUME_MASTER, 0x8000);

    /* The default value for stereo registers is 8808h, which corresponds to 0 dB gain with mute on.*/
    ichac97R3MixerSetVolume(pThis, pThisCC, AC97_PCM_Out_Volume_Mute, PDMAUDIOMIXERCTL_FRONT,         0x8808);
    ichac97R3MixerSetVolume(pThis, pThisCC, AC97_Line_In_Volume_Mute, PDMAUDIOMIXERCTL_LINE_IN,       0x8808);
    ichac97R3MixerSetVolume(pThis, pThisCC, AC97_Mic_Volume_Mute,     PDMAUDIOMIXERCTL_MIC_IN,        0x8008);

    /* The default for record controls is 0 dB gain with mute on. */
    ichac97R3MixerSetGain(pThis, pThisCC, AC97_Record_Gain_Mute,      PDMAUDIOMIXERCTL_LINE_IN,       0x8000);
    ichac97R3MixerSetGain(pThis, pThisCC, AC97_Record_Gain_Mic_Mute,  PDMAUDIOMIXERCTL_MIC_IN,        0x8000);

    return VINF_SUCCESS;
}

# if 0 /* Unused */
static void ichac97R3WriteBUP(PAC97STATE pThis, uint32_t cbElapsed)
{
    LogFlowFunc(("cbElapsed=%RU32\n", cbElapsed));

    if (!(pThis->bup_flag & BUP_SET))
    {
        if (pThis->bup_flag & BUP_LAST)
        {
            unsigned int i;
            uint32_t *p = (uint32_t*)pThis->silence;
            for (i = 0; i < sizeof(pThis->silence) / 4; i++) /** @todo r=andy Assumes 16-bit samples, stereo. */
                *p++ = pThis->last_samp;
        }
        else
            RT_ZERO(pThis->silence);

        pThis->bup_flag |= BUP_SET;
    }

    while (cbElapsed)
    {
        uint32_t cbToWrite = RT_MIN(cbElapsed, (uint32_t)sizeof(pThis->silence));
        uint32_t cbWrittenToStream;

        int rc2 = AudioMixerSinkWrite(pThisCC->pSinkOut, AUDMIXOP_COPY,
                                      pThis->silence, cbToWrite, &cbWrittenToStream);
        if (RT_SUCCESS(rc2))
        {
            if (cbWrittenToStream < cbToWrite) /* Lagging behind? */
                LogFlowFunc(("Warning: Only written %RU32 / %RU32 bytes, expect lags\n", cbWrittenToStream, cbToWrite));
        }

        /* Always report all data as being written;
         * backends who were not able to catch up have to deal with it themselves. */
        Assert(cbElapsed >= cbToWrite);
        cbElapsed -= cbToWrite;
    }
}
# endif /* Unused */

/**
 * @callback_method_impl{FNTMTIMERDEV,
 * Timer callback which handles the audio data transfers on a periodic basis.}
 */
static DECLCALLBACK(void) ichac97R3Timer(PPDMDEVINS pDevIns, PTMTIMER pTimer, void *pvUser)
{
    PAC97STATE      pThis     = PDMDEVINS_2_DATA(pDevIns, PAC97STATE);
    STAM_PROFILE_START(&pThis->StatTimer, a);
    PAC97STATER3    pThisCC   = PDMDEVINS_2_DATA_CC(pDevIns, PAC97STATER3);
    PAC97STREAM     pStream   = (PAC97STREAM)pvUser;
    PAC97STREAMR3   pStreamCC = &RT_SAFE_SUBSCRIPT8(pThisCC->aStreams, pStream->u8SD);
    RT_NOREF(pTimer);

    Assert(pStream - &pThis->aStreams[0] == pStream->u8SD);
    Assert(PDMDevHlpCritSectIsOwner(pDevIns, &pThis->CritSect));
    Assert(PDMDevHlpTimerIsLockOwner(pDevIns, pStream->hTimer));

    ichac97R3StreamUpdate(pDevIns, pThis, pThisCC, pStream, pStreamCC, true /* fInTimer */);

    PAUDMIXSINK pSink = ichac97R3IndexToSink(pThisCC, pStream->u8SD);
    if (pSink && AudioMixerSinkIsActive(pSink))
    {
        ichac97R3StreamTransferUpdate(pDevIns, pStream, pStreamCC, pStream->Regs.picb << 1); /** @todo r=andy Assumes 16-bit samples. */
        ichac97R3TimerSet(pDevIns, pStream, pStreamCC->State.cTransferTicks);
    }

    STAM_PROFILE_STOP(&pThis->StatTimer, a);
}


/**
 * Sets the virtual device timer to a new expiration time.
 *
 * @param   pDevIns             The device instance.
 * @param   pStream             AC'97 stream to set timer for.
 * @param   cTicksToDeadline    The number of ticks to the new deadline.
 *
 * @remarks This used to be more complicated a long time ago...
 */
DECLINLINE(void) ichac97R3TimerSet(PPDMDEVINS pDevIns, PAC97STREAM pStream, uint64_t cTicksToDeadline)
{
    int rc = PDMDevHlpTimerSetRelative(pDevIns, pStream->hTimer, cTicksToDeadline, NULL /*pu64Now*/);
    AssertRC(rc);
}


/**
 * Transfers data of an AC'97 stream according to its usage (input / output).
 *
 * For an SDO (output) stream this means reading DMA data from the device to
 * the AC'97 stream's internal FIFO buffer.
 *
 * For an SDI (input) stream this is reading audio data from the AC'97 stream's
 * internal FIFO buffer and writing it as DMA data to the device.
 *
 * @returns IPRT status code.
 * @param   pDevIns             The device instance.
 * @param   pThis               The shared AC'97 state.
 * @param   pStream             The AC'97 stream to update (shared).
 * @param   pStreamCC           The AC'97 stream to update (ring-3).
 * @param   cbToProcessMax      Maximum of data (in bytes) to process.
 */
static int ichac97R3StreamTransfer(PPDMDEVINS pDevIns, PAC97STATE pThis, PAC97STREAM pStream,
                                   PAC97STREAMR3 pStreamCC, uint32_t cbToProcessMax)
{
    if (!cbToProcessMax)
        return VINF_SUCCESS;

#ifdef VBOX_STRICT
    const unsigned cbFrame = DrvAudioHlpPCMPropsBytesPerFrame(&pStreamCC->State.Cfg.Props);
#endif

    /* Make sure to only process an integer number of audio frames. */
    Assert(cbToProcessMax % cbFrame == 0);

    ichac97R3StreamLock(pStreamCC);

    PAC97BMREGS pRegs = &pStream->Regs;

    if (pRegs->sr & AC97_SR_DCH) /* Controller halted? */
    {
        if (pRegs->cr & AC97_CR_RPBM) /* Bus master operation starts. */
        {
            switch (pStream->u8SD)
            {
                case AC97SOUNDSOURCE_PO_INDEX:
                    /*ichac97R3WriteBUP(pThis, cbToProcess);*/
                    break;

                default:
                    break;
            }
        }

        ichac97R3StreamUnlock(pStreamCC);
        return VINF_SUCCESS;
    }

    /* BCIS flag still set? Skip iteration. */
    if (pRegs->sr & AC97_SR_BCIS)
    {
        Log3Func(("[SD%RU8] BCIS set\n", pStream->u8SD));

        ichac97R3StreamUnlock(pStreamCC);
        return VINF_SUCCESS;
    }

    uint32_t cbLeft           = RT_MIN((uint32_t)(pRegs->picb << 1), cbToProcessMax); /** @todo r=andy Assumes 16bit samples. */
    uint32_t cbProcessedTotal = 0;

    PRTCIRCBUF pCircBuf = pStreamCC->State.pCircBuf;
    AssertPtr(pCircBuf);

    int rc = VINF_SUCCESS;

    Log3Func(("[SD%RU8] cbToProcessMax=%RU32, cbLeft=%RU32\n", pStream->u8SD, cbToProcessMax, cbLeft));

    while (cbLeft)
    {
        if (!pRegs->picb) /* Got a new buffer descriptor, that is, the position is 0? */
        {
            Log3Func(("Fresh buffer descriptor %RU8 is empty, addr=%#x, len=%#x, skipping\n",
                      pRegs->civ, pRegs->bd.addr, pRegs->bd.ctl_len));
            if (pRegs->civ == pRegs->lvi)
            {
                pRegs->sr |= AC97_SR_DCH; /** @todo r=andy Also set CELV? */
                pThis->bup_flag = 0;

                rc = VINF_EOF;
                break;
            }

            pRegs->sr &= ~AC97_SR_CELV;
            pRegs->civ = pRegs->piv;
            pRegs->piv = (pRegs->piv + 1) % AC97_MAX_BDLE;

            ichac97R3StreamFetchBDLE(pDevIns, pStream);
            continue;
        }

        uint32_t cbChunk = cbLeft;

        switch (pStream->u8SD)
        {
            case AC97SOUNDSOURCE_PO_INDEX: /* Output */
            {
                void *pvDst;
                size_t cbDst;

                RTCircBufAcquireWriteBlock(pCircBuf, cbChunk, &pvDst, &cbDst);

                if (cbDst)
                {
                    int rc2 = PDMDevHlpPhysRead(pDevIns, pRegs->bd.addr, (uint8_t *)pvDst, cbDst);
                    AssertRC(rc2);

                    if (RT_LIKELY(!pStreamCC->Dbg.Runtime.fEnabled))
                    { /* likely */ }
                    else
                        DrvAudioHlpFileWrite(pStreamCC->Dbg.Runtime.pFileDMA, pvDst, cbDst, 0 /* fFlags */);
                }

                RTCircBufReleaseWriteBlock(pCircBuf, cbDst);

                cbChunk = (uint32_t)cbDst; /* Update the current chunk size to what really has been written. */
                break;
            }

            case AC97SOUNDSOURCE_PI_INDEX: /* Input */
            case AC97SOUNDSOURCE_MC_INDEX: /* Input */
            {
                void *pvSrc;
                size_t cbSrc;

                RTCircBufAcquireReadBlock(pCircBuf, cbChunk, &pvSrc, &cbSrc);

                if (cbSrc)
                {
/** @todo r=bird: Just curious, DevHDA uses PDMDevHlpPCIPhysWrite here.  So,
 *        is AC97 not subject to PCI busmaster enable/disable? */
                    int rc2 = PDMDevHlpPhysWrite(pDevIns, pRegs->bd.addr, (uint8_t *)pvSrc, cbSrc);
                    AssertRC(rc2);

                    if (RT_LIKELY(!pStreamCC->Dbg.Runtime.fEnabled))
                    { /* likely */ }
                    else
                        DrvAudioHlpFileWrite(pStreamCC->Dbg.Runtime.pFileDMA, pvSrc, cbSrc, 0 /* fFlags */);
                }

                RTCircBufReleaseReadBlock(pCircBuf, cbSrc);

                cbChunk = (uint32_t)cbSrc; /* Update the current chunk size to what really has been read. */
                break;
            }

            default:
                AssertMsgFailed(("Stream #%RU8 not supported\n", pStream->u8SD));
                rc = VERR_NOT_SUPPORTED;
                break;
        }

        if (RT_FAILURE(rc))
            break;

        if (cbChunk)
        {
            cbProcessedTotal     += cbChunk;
            Assert(cbProcessedTotal <= cbToProcessMax);
            Assert(cbLeft >= cbChunk);
            cbLeft      -= cbChunk;
            Assert((cbChunk & 1) == 0); /* Else the following shift won't work */

            pRegs->picb    -= (cbChunk >> 1); /** @todo r=andy Assumes 16bit samples. */
            pRegs->bd.addr += cbChunk;
        }

        LogFlowFunc(("[SD%RU8] cbChunk=%RU32, cbLeft=%RU32, cbTotal=%RU32, rc=%Rrc\n",
                     pStream->u8SD, cbChunk, cbLeft, cbProcessedTotal, rc));

        if (!pRegs->picb)
        {
            uint32_t new_sr = pRegs->sr & ~AC97_SR_CELV;

            if (pRegs->bd.ctl_len & AC97_BD_IOC)
            {
                new_sr |= AC97_SR_BCIS;
            }

            if (pRegs->civ == pRegs->lvi)
            {
                /* Did we run out of data? */
                LogFunc(("Underrun CIV (%RU8) == LVI (%RU8)\n", pRegs->civ, pRegs->lvi));

                new_sr |= AC97_SR_LVBCI | AC97_SR_DCH | AC97_SR_CELV;
                pThis->bup_flag = (pRegs->bd.ctl_len & AC97_BD_BUP) ? BUP_LAST : 0;

                rc = VINF_EOF;
            }
            else
            {
                pRegs->civ = pRegs->piv;
                pRegs->piv = (pRegs->piv + 1) % AC97_MAX_BDLE;
                ichac97R3StreamFetchBDLE(pDevIns, pStream);
            }

            ichac97StreamUpdateSR(pDevIns, pThis, pStream, new_sr);
        }

        if (/* All data processed? */
               rc == VINF_EOF
            /* ... or an error occurred? */
            || RT_FAILURE(rc))
        {
            break;
        }
    }

    ichac97R3StreamUnlock(pStreamCC);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

#endif /* IN_RING3 */


/**
 * @callback_method_impl{FNIOMIOPORTNEWIN}
 */
static DECLCALLBACK(VBOXSTRICTRC)
ichac97IoPortNabmRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT offPort, uint32_t *pu32, unsigned cb)
{
    PAC97STATE pThis = PDMDEVINS_2_DATA(pDevIns, PAC97STATE);
    RT_NOREF(pvUser);

    DEVAC97_LOCK_RETURN(pDevIns, pThis, VINF_IOM_R3_IOPORT_READ);

    /* Get the index of the NABMBAR port. */
    if (   AC97_PORT2IDX_UNMASKED(offPort) < AC97_MAX_STREAMS
        && offPort != AC97_GLOB_CNT)
    {
        PAC97STREAM pStream = &pThis->aStreams[AC97_PORT2IDX(offPort)];
        PAC97BMREGS pRegs   = &pStream->Regs;

        switch (cb)
        {
            case 1:
                switch (offPort & AC97_NABM_OFF_MASK)
                {
                    case AC97_NABM_OFF_CIV:
                        /* Current Index Value Register */
                        *pu32 = pRegs->civ;
                        Log3Func(("CIV[%d] -> %#x\n", AC97_PORT2IDX(offPort), *pu32));
                        break;
                    case AC97_NABM_OFF_LVI:
                        /* Last Valid Index Register */
                        *pu32 = pRegs->lvi;
                        Log3Func(("LVI[%d] -> %#x\n", AC97_PORT2IDX(offPort), *pu32));
                        break;
                    case AC97_NABM_OFF_PIV:
                        /* Prefetched Index Value Register */
                        *pu32 = pRegs->piv;
                        Log3Func(("PIV[%d] -> %#x\n", AC97_PORT2IDX(offPort), *pu32));
                        break;
                    case AC97_NABM_OFF_CR:
                        /* Control Register */
                        *pu32 = pRegs->cr;
                        Log3Func(("CR[%d] -> %#x\n", AC97_PORT2IDX(offPort), *pu32));
                        break;
                    case AC97_NABM_OFF_SR:
                        /* Status Register (lower part) */
                        *pu32 = RT_LO_U8(pRegs->sr);
                        Log3Func(("SRb[%d] -> %#x\n", AC97_PORT2IDX(offPort), *pu32));
                        break;
                    default:
                        *pu32 = UINT32_MAX;
                        LogFunc(("U nabm readb %#x -> %#x\n", offPort, UINT32_MAX));
                        STAM_REL_COUNTER_INC(&pThis->StatUnimplementedNabmReads);
                        break;
                }
                break;

            case 2:
                switch (offPort & AC97_NABM_OFF_MASK)
                {
                    case AC97_NABM_OFF_SR:
                        /* Status Register */
                        *pu32 = pRegs->sr;
                        Log3Func(("SR[%d] -> %#x\n", AC97_PORT2IDX(offPort), *pu32));
                        break;
                    case AC97_NABM_OFF_PICB:
                        /* Position in Current Buffer */
                        *pu32 = pRegs->picb;
                        Log3Func(("PICB[%d] -> %#x\n", AC97_PORT2IDX(offPort), *pu32));
                        break;
                    default:
                        *pu32 = UINT32_MAX;
                        LogFunc(("U nabm readw %#x -> %#x\n", offPort, UINT32_MAX));
                        STAM_REL_COUNTER_INC(&pThis->StatUnimplementedNabmReads);
                        break;
                }
                break;

            case 4:
                switch (offPort & AC97_NABM_OFF_MASK)
                {
                    case AC97_NABM_OFF_BDBAR:
                        /* Buffer Descriptor Base Address Register */
                        *pu32 = pRegs->bdbar;
                        Log3Func(("BMADDR[%d] -> %#x\n", AC97_PORT2IDX(offPort), *pu32));
                        break;
                    case AC97_NABM_OFF_CIV:
                        /* 32-bit access: Current Index Value Register +
                         *                Last Valid Index Register +
                         *                Status Register */
                        *pu32 = pRegs->civ | (pRegs->lvi << 8) | (pRegs->sr << 16); /** @todo r=andy Use RT_MAKE_U32_FROM_U8. */
                        Log3Func(("CIV LVI SR[%d] -> %#x, %#x, %#x\n",
                                  AC97_PORT2IDX(offPort), pRegs->civ, pRegs->lvi, pRegs->sr));
                        break;
                    case AC97_NABM_OFF_PICB:
                        /* 32-bit access: Position in Current Buffer Register +
                         *                Prefetched Index Value Register +
                         *                Control Register */
                        *pu32 = pRegs->picb | (pRegs->piv << 16) | (pRegs->cr << 24); /** @todo r=andy Use RT_MAKE_U32_FROM_U8. */
                        Log3Func(("PICB PIV CR[%d] -> %#x %#x %#x %#x\n",
                                  AC97_PORT2IDX(offPort), *pu32, pRegs->picb, pRegs->piv, pRegs->cr));
                        break;

                    default:
                        *pu32 = UINT32_MAX;
                        LogFunc(("U nabm readl %#x -> %#x\n", offPort, UINT32_MAX));
                        STAM_REL_COUNTER_INC(&pThis->StatUnimplementedNabmReads);
                        break;
                }
                break;

            default:
                DEVAC97_UNLOCK(pDevIns, pThis);
                AssertFailed();
                return VERR_IOM_IOPORT_UNUSED;
        }
    }
    else
    {
        switch (cb)
        {
            case 1:
                switch (offPort)
                {
                    case AC97_CAS:
                        /* Codec Access Semaphore Register */
                        Log3Func(("CAS %d\n", pThis->cas));
                        *pu32 = pThis->cas;
                        pThis->cas = 1;
                        break;
                    default:
                        *pu32 = UINT32_MAX;
                        LogFunc(("U nabm readb %#x -> %#x\n", offPort, UINT32_MAX));
                        STAM_REL_COUNTER_INC(&pThis->StatUnimplementedNabmReads);
                        break;
                }
                break;

            case 2:
                *pu32 = UINT32_MAX;
                LogFunc(("U nabm readw %#x -> %#x\n", offPort, UINT32_MAX));
                STAM_REL_COUNTER_INC(&pThis->StatUnimplementedNabmReads);
                break;

            case 4:
                switch (offPort)
                {
                    case AC97_GLOB_CNT:
                        /* Global Control */
                        *pu32 = pThis->glob_cnt;
                        Log3Func(("glob_cnt -> %#x\n", *pu32));
                        break;
                    case AC97_GLOB_STA:
                        /* Global Status */
                        *pu32 = pThis->glob_sta | AC97_GS_S0CR;
                        Log3Func(("glob_sta -> %#x\n", *pu32));
                        break;
                    default:
                        *pu32 = UINT32_MAX;
                        LogFunc(("U nabm readl %#x -> %#x\n", offPort, UINT32_MAX));
                        STAM_REL_COUNTER_INC(&pThis->StatUnimplementedNabmReads);
                        break;
                }
                break;

            default:
                DEVAC97_UNLOCK(pDevIns, pThis);
                AssertFailed();
                return VERR_IOM_IOPORT_UNUSED;
        }
    }

    DEVAC97_UNLOCK(pDevIns, pThis);
    return VINF_SUCCESS;
}

/**
 * @callback_method_impl{FNIOMIOPORTNEWOUT}
 */
static DECLCALLBACK(VBOXSTRICTRC)
ichac97IoPortNabmWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT offPort, uint32_t u32, unsigned cb)
{
    PAC97STATE      pThis   = PDMDEVINS_2_DATA(pDevIns, PAC97STATE);
#ifdef IN_RING3
    PAC97STATER3    pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PAC97STATER3);
#endif
    RT_NOREF(pvUser);

    VBOXSTRICTRC rc = VINF_SUCCESS;
    if (   AC97_PORT2IDX_UNMASKED(offPort) < AC97_MAX_STREAMS
        && offPort != AC97_GLOB_CNT)
    {
#ifdef IN_RING3
        PAC97STREAMR3   pStreamCC = &pThisCC->aStreams[AC97_PORT2IDX(offPort)];
#endif
        PAC97STREAM     pStream   = &pThis->aStreams[AC97_PORT2IDX(offPort)];
        PAC97BMREGS     pRegs     = &pStream->Regs;

        DEVAC97_LOCK_BOTH_RETURN(pDevIns, pThis, pStream, VINF_IOM_R3_IOPORT_WRITE);
        switch (cb)
        {
            case 1:
                switch (offPort & AC97_NABM_OFF_MASK)
                {
                    /*
                     * Last Valid Index.
                     */
                    case AC97_NABM_OFF_LVI:
                        if (   (pRegs->cr & AC97_CR_RPBM)
                            && (pRegs->sr & AC97_SR_DCH))
                        {
#ifdef IN_RING3
                            pRegs->sr &= ~(AC97_SR_DCH | AC97_SR_CELV);
                            pRegs->civ = pRegs->piv;
                            pRegs->piv = (pRegs->piv + 1) % AC97_MAX_BDLE;
#else
                            rc = VINF_IOM_R3_IOPORT_WRITE;
#endif
                        }
                        pRegs->lvi = u32 % AC97_MAX_BDLE;
                        Log3Func(("[SD%RU8] LVI <- %#x\n", pStream->u8SD, u32));
                        break;

                    /*
                     * Control Registers.
                     */
                    case AC97_NABM_OFF_CR:
#ifdef IN_RING3
                        Log3Func(("[SD%RU8] CR <- %#x (cr %#x)\n", pStream->u8SD, u32, pRegs->cr));
                        if (u32 & AC97_CR_RR) /* Busmaster reset. */
                        {
                            Log3Func(("[SD%RU8] Reset\n", pStream->u8SD));

                            /* Make sure that Run/Pause Bus Master bit (RPBM) is cleared (0). */
                            Assert((pRegs->cr & AC97_CR_RPBM) == 0);

                            ichac97R3StreamEnable(pThis, pThisCC, pStream, pStreamCC, false /* fEnable */);
                            ichac97R3StreamReset(pThis, pStream, pStreamCC);

                            ichac97StreamUpdateSR(pDevIns, pThis, pStream, AC97_SR_DCH); /** @todo Do we need to do that? */
                        }
                        else
                        {
                            pRegs->cr = u32 & AC97_CR_VALID_MASK;

                            if (!(pRegs->cr & AC97_CR_RPBM))
                            {
                                Log3Func(("[SD%RU8] Disable\n", pStream->u8SD));

                                ichac97R3StreamEnable(pThis, pThisCC, pStream, pStreamCC, false /* fEnable */);

                                pRegs->sr |= AC97_SR_DCH;
                            }
                            else
                            {
                                Log3Func(("[SD%RU8] Enable\n", pStream->u8SD));

                                pRegs->civ = pRegs->piv;
                                pRegs->piv = (pRegs->piv + 1) % AC97_MAX_BDLE;

                                pRegs->sr &= ~AC97_SR_DCH;

                                /* Fetch the initial BDLE descriptor. */
                                ichac97R3StreamFetchBDLE(pDevIns, pStream);
# ifdef LOG_ENABLED
                                ichac97R3BDLEDumpAll(pDevIns, pStream->Regs.bdbar, pStream->Regs.lvi + 1);
# endif
                                ichac97R3StreamEnable(pThis, pThisCC, pStream, pStreamCC, true /* fEnable */);

                                /* Arm the timer for this stream. */
                                /** @todo r=bird: This function returns bool, not VBox status! */
                                ichac97R3TimerSet(pDevIns, pStream, pStreamCC->State.cTransferTicks);
                            }
                        }
#else /* !IN_RING3 */
                        rc = VINF_IOM_R3_IOPORT_WRITE;
#endif
                        break;

                    /*
                     * Status Registers.
                     */
                    case AC97_NABM_OFF_SR:
                        ichac97StreamWriteSR(pDevIns, pThis, pStream, u32);
                        break;

                    default:
                        LogRel2(("AC97: Warning: Unimplemented NABMWrite offPort=%#x <- %#x LB 1\n", offPort, u32));
                        STAM_REL_COUNTER_INC(&pThis->StatUnimplementedNabmWrites);
                        break;
                }
                break;

            case 2:
                switch (offPort & AC97_NABM_OFF_MASK)
                {
                    case AC97_NABM_OFF_SR:
                        ichac97StreamWriteSR(pDevIns, pThis, pStream, u32);
                        break;
                    default:
                        LogRel2(("AC97: Warning: Unimplemented NABMWrite offPort=%#x <- %#x LB 2\n", offPort, u32));
                        STAM_REL_COUNTER_INC(&pThis->StatUnimplementedNabmWrites);
                        break;
                }
                break;

            case 4:
                switch (offPort & AC97_NABM_OFF_MASK)
                {
                    case AC97_NABM_OFF_BDBAR:
                        /* Buffer Descriptor list Base Address Register */
                        pRegs->bdbar = u32 & ~3;
                        Log3Func(("[SD%RU8] BDBAR <- %#x (bdbar %#x)\n", AC97_PORT2IDX(offPort), u32, pRegs->bdbar));
                        break;
                    default:
                        LogRel2(("AC97: Warning: Unimplemented NABMWrite offPort=%#x <- %#x LB 4\n", offPort, u32));
                        STAM_REL_COUNTER_INC(&pThis->StatUnimplementedNabmWrites);
                        break;
                }
                break;

            default:
                AssertMsgFailed(("offPort=%#x <- %#x LB %u\n", offPort, u32, cb));
                break;
        }
        DEVAC97_UNLOCK_BOTH(pDevIns, pThis, pStream);
    }
    else
    {
        switch (cb)
        {
            case 1:
                LogRel2(("AC97: Warning: Unimplemented NABMWrite offPort=%#x <- %#x LB 1\n", offPort, u32));
                STAM_REL_COUNTER_INC(&pThis->StatUnimplementedNabmWrites);
                break;

            case 2:
                LogRel2(("AC97: Warning: Unimplemented NABMWrite offPort=%#x <- %#x LB 2\n", offPort, u32));
                STAM_REL_COUNTER_INC(&pThis->StatUnimplementedNabmWrites);
                break;

            case 4:
                switch (offPort)
                {
                    case AC97_GLOB_CNT:
                        /* Global Control */
                        DEVAC97_LOCK_RETURN(pDevIns, pThis, VINF_IOM_R3_IOPORT_WRITE);
                        if (u32 & AC97_GC_WR)
                            ichac97WarmReset(pThis);
                        if (u32 & AC97_GC_CR)
                            ichac97ColdReset(pThis);
                        if (!(u32 & (AC97_GC_WR | AC97_GC_CR)))
                            pThis->glob_cnt = u32 & AC97_GC_VALID_MASK;
                        Log3Func(("glob_cnt <- %#x (glob_cnt %#x)\n", u32, pThis->glob_cnt));
                        DEVAC97_UNLOCK(pDevIns, pThis);
                        break;
                    case AC97_GLOB_STA:
                        /* Global Status */
                        DEVAC97_LOCK_RETURN(pDevIns, pThis, VINF_IOM_R3_IOPORT_WRITE);
                        pThis->glob_sta &= ~(u32 & AC97_GS_WCLEAR_MASK);
                        pThis->glob_sta |= (u32 & ~(AC97_GS_WCLEAR_MASK | AC97_GS_RO_MASK)) & AC97_GS_VALID_MASK;
                        Log3Func(("glob_sta <- %#x (glob_sta %#x)\n", u32, pThis->glob_sta));
                        DEVAC97_UNLOCK(pDevIns, pThis);
                        break;
                    default:
                        LogRel2(("AC97: Warning: Unimplemented NABMWrite offPort=%#x <- %#x LB 4\n", offPort, u32));
                        STAM_REL_COUNTER_INC(&pThis->StatUnimplementedNabmWrites);
                        break;
                }
                break;

            default:
                AssertMsgFailed(("offPort=%#x <- %#x LB %u\n", offPort, u32, cb));
                break;
        }
    }

    return rc;
}

/**
 * @callback_method_impl{FNIOMIOPORTNEWIN}
 */
static DECLCALLBACK(VBOXSTRICTRC)
ichac97IoPortNamRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT offPort, uint32_t *pu32, unsigned cb)
{
    PAC97STATE pThis = PDMDEVINS_2_DATA(pDevIns, PAC97STATE);
    RT_NOREF(pvUser);
    Assert(offPort < 256);

    DEVAC97_LOCK_RETURN(pDevIns, pThis, VINF_IOM_R3_IOPORT_READ);

    VBOXSTRICTRC rc = VINF_SUCCESS;
    switch (cb)
    {
        case 1:
        {
            LogRel2(("AC97: Warning: Unimplemented read (1 byte) offPort=%#x\n", offPort));
            pThis->cas = 0;
            *pu32 = UINT32_MAX;
            break;
        }

        case 2:
        {
            pThis->cas = 0;
            *pu32 = ichac97MixerGet(pThis, offPort);
            break;
        }

        case 4:
        {
            LogRel2(("AC97: Warning: Unimplemented read (4 bytes) offPort=%#x\n", offPort));
            pThis->cas = 0;
            *pu32 = UINT32_MAX;
            break;
        }

        default:
        {
            AssertFailed();
            rc = VERR_IOM_IOPORT_UNUSED;
        }
    }

    DEVAC97_UNLOCK(pDevIns, pThis);
    return rc;
}

/**
 * @callback_method_impl{FNIOMIOPORTNEWOUT}
 */
static DECLCALLBACK(VBOXSTRICTRC)
ichac97IoPortNamWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT offPort, uint32_t u32, unsigned cb)
{
    PAC97STATE   pThis   = PDMDEVINS_2_DATA(pDevIns, PAC97STATE);
#ifdef IN_RING3
    PAC97STATER3 pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PAC97STATER3);
#endif
    RT_NOREF(pvUser);

    DEVAC97_LOCK_RETURN(pDevIns, pThis, VINF_IOM_R3_IOPORT_WRITE);

    VBOXSTRICTRC rc = VINF_SUCCESS;
    switch (cb)
    {
        case 1:
        {
            LogRel2(("AC97: Warning: Unimplemented NAMWrite (1 byte) offPort=%#x <- %#x\n", offPort, u32));
            pThis->cas = 0;
            break;
        }

        case 2:
        {
            pThis->cas = 0;
            switch (offPort)
            {
                case AC97_Reset:
#ifdef IN_RING3
                    ichac97R3Reset(pDevIns);
#else
                    rc = VINF_IOM_R3_IOPORT_WRITE;
#endif
                    break;
                case AC97_Powerdown_Ctrl_Stat:
                    u32 &= ~0xf;
                    u32 |= ichac97MixerGet(pThis, offPort) & 0xf;
                    ichac97MixerSet(pThis, offPort, u32);
                    break;
                case AC97_Master_Volume_Mute:
                    if (pThis->enmCodecModel == AC97CODEC_AD1980)
                    {
                        if (ichac97MixerGet(pThis, AC97_AD_Misc) & AC97_AD_MISC_LOSEL)
                            break; /* Register controls surround (rear), do nothing. */
                    }
#ifdef IN_RING3
                    ichac97R3MixerSetVolume(pThis, pThisCC, offPort, PDMAUDIOMIXERCTL_VOLUME_MASTER, u32);
#else
                    rc = VINF_IOM_R3_IOPORT_WRITE;
#endif
                    break;
                case AC97_Headphone_Volume_Mute:
                    if (pThis->enmCodecModel == AC97CODEC_AD1980)
                    {
                        if (ichac97MixerGet(pThis, AC97_AD_Misc) & AC97_AD_MISC_HPSEL)
                        {
                            /* Register controls PCM (front) outputs. */
#ifdef IN_RING3
                            ichac97R3MixerSetVolume(pThis, pThisCC, offPort, PDMAUDIOMIXERCTL_VOLUME_MASTER, u32);
#else
                            rc = VINF_IOM_R3_IOPORT_WRITE;
#endif
                        }
                    }
                    break;
                case AC97_PCM_Out_Volume_Mute:
#ifdef IN_RING3
                    ichac97R3MixerSetVolume(pThis, pThisCC, offPort, PDMAUDIOMIXERCTL_FRONT, u32);
#else
                    rc = VINF_IOM_R3_IOPORT_WRITE;
#endif
                    break;
                case AC97_Line_In_Volume_Mute:
#ifdef IN_RING3
                    ichac97R3MixerSetVolume(pThis, pThisCC, offPort, PDMAUDIOMIXERCTL_LINE_IN, u32);
#else
                    rc = VINF_IOM_R3_IOPORT_WRITE;
#endif
                    break;
                case AC97_Record_Select:
#ifdef IN_RING3
                    ichac97R3MixerRecordSelect(pThis, u32);
#else
                    rc = VINF_IOM_R3_IOPORT_WRITE;
#endif
                    break;
                case AC97_Record_Gain_Mute:
#ifdef IN_RING3
                    /* Newer Ubuntu guests rely on that when controlling gain and muting
                     * the recording (capturing) levels. */
                    ichac97R3MixerSetGain(pThis, pThisCC, offPort, PDMAUDIOMIXERCTL_LINE_IN, u32);
#else
                    rc = VINF_IOM_R3_IOPORT_WRITE;
#endif
                    break;
                case AC97_Record_Gain_Mic_Mute:
#ifdef IN_RING3
                    /* Ditto; see note above. */
                    ichac97R3MixerSetGain(pThis, pThisCC, offPort, PDMAUDIOMIXERCTL_MIC_IN,  u32);
#else
                    rc = VINF_IOM_R3_IOPORT_WRITE;
#endif
                    break;
                case AC97_Vendor_ID1:
                case AC97_Vendor_ID2:
                    LogFunc(("Attempt to write vendor ID to %#x\n", u32));
                    break;
                case AC97_Extended_Audio_ID:
                    LogFunc(("Attempt to write extended audio ID to %#x\n", u32));
                    break;
                case AC97_Extended_Audio_Ctrl_Stat:
#ifdef IN_RING3
                    /*
                     * Handle VRA bits.
                     */
                    if (!(u32 & AC97_EACS_VRA)) /* Check if VRA bit is not set. */
                    {
                        ichac97MixerSet(pThis, AC97_PCM_Front_DAC_Rate, 0xbb80); /* Set default (48000 Hz). */
                        ichac97R3StreamReOpen(pThis, pThisCC, &pThis->aStreams[AC97SOUNDSOURCE_PO_INDEX],
                                              &pThisCC->aStreams[AC97SOUNDSOURCE_PO_INDEX], true /* fForce */);

                        ichac97MixerSet(pThis, AC97_PCM_LR_ADC_Rate, 0xbb80); /* Set default (48000 Hz). */
                        ichac97R3StreamReOpen(pThis, pThisCC, &pThis->aStreams[AC97SOUNDSOURCE_PI_INDEX],
                                              &pThisCC->aStreams[AC97SOUNDSOURCE_PI_INDEX], true /* fForce */);
                    }
                    else
                        LogRel2(("AC97: Variable rate audio (VRA) is not supported\n"));

                    /*
                     * Handle VRM bits.
                     */
                    if (!(u32 & AC97_EACS_VRM)) /* Check if VRM bit is not set. */
                    {
                        ichac97MixerSet(pThis, AC97_MIC_ADC_Rate, 0xbb80); /* Set default (48000 Hz). */
                        ichac97R3StreamReOpen(pThis, pThisCC, &pThis->aStreams[AC97SOUNDSOURCE_MC_INDEX],
                                              &pThisCC->aStreams[AC97SOUNDSOURCE_MC_INDEX], true /* fForce */);
                    }
                    else
                        LogRel2(("AC97: Variable rate microphone audio (VRM) is not supported\n"));

                    LogRel2(("AC97: Setting extended audio control to %#x\n", u32));
                    ichac97MixerSet(pThis, AC97_Extended_Audio_Ctrl_Stat, u32);
#else /* !IN_RING3 */
                    rc = VINF_IOM_R3_IOPORT_WRITE;
#endif
                    break;
                case AC97_PCM_Front_DAC_Rate: /* Output slots 3, 4, 6. */
#ifdef IN_RING3
                    if (ichac97MixerGet(pThis, AC97_Extended_Audio_Ctrl_Stat) & AC97_EACS_VRA)
                    {
                        LogRel2(("AC97: Setting front DAC rate to 0x%x\n", u32));
                        ichac97MixerSet(pThis, offPort, u32);
                        ichac97R3StreamReOpen(pThis, pThisCC, &pThis->aStreams[AC97SOUNDSOURCE_PO_INDEX],
                                              &pThisCC->aStreams[AC97SOUNDSOURCE_PO_INDEX], true /* fForce */);
                    }
                    else
                        LogRel2(("AC97: Setting front DAC rate (0x%x) when VRA is not set is forbidden, ignoring\n", u32));
#else
                    rc = VINF_IOM_R3_IOPORT_WRITE;
#endif
                    break;
                case AC97_MIC_ADC_Rate: /* Input slot 6. */
#ifdef IN_RING3
                    if (ichac97MixerGet(pThis, AC97_Extended_Audio_Ctrl_Stat) & AC97_EACS_VRM)
                    {
                        LogRel2(("AC97: Setting microphone ADC rate to 0x%x\n", u32));
                        ichac97MixerSet(pThis, offPort, u32);
                        ichac97R3StreamReOpen(pThis, pThisCC, &pThis->aStreams[AC97SOUNDSOURCE_MC_INDEX],
                                              &pThisCC->aStreams[AC97SOUNDSOURCE_MC_INDEX], true /* fForce */);
                    }
                    else
                        LogRel2(("AC97: Setting microphone ADC rate (0x%x) when VRM is not set is forbidden, ignoring\n", u32));
#else
                    rc = VINF_IOM_R3_IOPORT_WRITE;
#endif
                    break;
                case AC97_PCM_LR_ADC_Rate: /* Input slots 3, 4. */
#ifdef IN_RING3
                    if (ichac97MixerGet(pThis, AC97_Extended_Audio_Ctrl_Stat) & AC97_EACS_VRA)
                    {
                        LogRel2(("AC97: Setting line-in ADC rate to 0x%x\n", u32));
                        ichac97MixerSet(pThis, offPort, u32);
                        ichac97R3StreamReOpen(pThis, pThisCC, &pThis->aStreams[AC97SOUNDSOURCE_PI_INDEX],
                                              &pThisCC->aStreams[AC97SOUNDSOURCE_PI_INDEX], true /* fForce */);
                    }
                    else
                        LogRel2(("AC97: Setting line-in ADC rate (0x%x) when VRA is not set is forbidden, ignoring\n", u32));
#else
                    rc = VINF_IOM_R3_IOPORT_WRITE;
#endif
                    break;
                default:
                    LogRel2(("AC97: Warning: Unimplemented NAMWrite (2 bytes) offPort=%#x <- %#x\n", offPort, u32));
                    ichac97MixerSet(pThis, offPort, u32);
                    break;
            }
            break;
        }

        case 4:
        {
            LogRel2(("AC97: Warning: Unimplemented 4 byte NAMWrite: offPort=%#x <- %#x\n", offPort, u32));
            pThis->cas = 0;
            break;
        }

        default:
            AssertMsgFailed(("Unhandled NAMWrite offPort=%#x, cb=%u u32=%#x\n", offPort, cb, u32));
            break;
    }

    DEVAC97_UNLOCK(pDevIns, pThis);
    return rc;
}

#ifdef IN_RING3

/**
 * Saves (serializes) an AC'97 stream using SSM.
 *
 * @param   pDevIns             Device instance.
 * @param   pSSM                Saved state manager (SSM) handle to use.
 * @param   pStream             AC'97 stream to save.
 */
static void ichac97R3SaveStream(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, PAC97STREAM pStream)
{
    PAC97BMREGS   pRegs = &pStream->Regs;
    PCPDMDEVHLPR3 pHlp  = pDevIns->pHlpR3;

    pHlp->pfnSSMPutU32(pSSM, pRegs->bdbar);
    pHlp->pfnSSMPutU8( pSSM, pRegs->civ);
    pHlp->pfnSSMPutU8( pSSM, pRegs->lvi);
    pHlp->pfnSSMPutU16(pSSM, pRegs->sr);
    pHlp->pfnSSMPutU16(pSSM, pRegs->picb);
    pHlp->pfnSSMPutU8( pSSM, pRegs->piv);
    pHlp->pfnSSMPutU8( pSSM, pRegs->cr);
    pHlp->pfnSSMPutS32(pSSM, pRegs->bd_valid);
    pHlp->pfnSSMPutU32(pSSM, pRegs->bd.addr);
    pHlp->pfnSSMPutU32(pSSM, pRegs->bd.ctl_len);
}

/**
 * @callback_method_impl{FNSSMDEVSAVEEXEC}
 */
static DECLCALLBACK(int) ichac97R3SaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PAC97STATE      pThis   = PDMDEVINS_2_DATA(pDevIns, PAC97STATE);
    PAC97STATER3    pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PAC97STATER3);
    PCPDMDEVHLPR3   pHlp    = pDevIns->pHlpR3;
    LogFlowFuncEnter();

    pHlp->pfnSSMPutU32(pSSM, pThis->glob_cnt);
    pHlp->pfnSSMPutU32(pSSM, pThis->glob_sta);
    pHlp->pfnSSMPutU32(pSSM, pThis->cas);

    /*
     * The order that the streams are saved here is fixed, so don't change.
     */
    /** @todo r=andy For the next saved state version, add unique stream identifiers and a stream count. */
    for (unsigned i = 0; i < AC97_MAX_STREAMS; i++)
        ichac97R3SaveStream(pDevIns, pSSM, &pThis->aStreams[i]);

    pHlp->pfnSSMPutMem(pSSM, pThis->mixer_data, sizeof(pThis->mixer_data));

    /* The stream order is against fixed and set in stone. */
    uint8_t afActiveStrms[AC97SOUNDSOURCE_MAX];
    afActiveStrms[AC97SOUNDSOURCE_PI_INDEX] = ichac97R3StreamIsEnabled(pThisCC, &pThis->aStreams[AC97SOUNDSOURCE_PI_INDEX]);
    afActiveStrms[AC97SOUNDSOURCE_PO_INDEX] = ichac97R3StreamIsEnabled(pThisCC, &pThis->aStreams[AC97SOUNDSOURCE_PO_INDEX]);
    afActiveStrms[AC97SOUNDSOURCE_MC_INDEX] = ichac97R3StreamIsEnabled(pThisCC, &pThis->aStreams[AC97SOUNDSOURCE_MC_INDEX]);
    AssertCompile(RT_ELEMENTS(afActiveStrms) == 3);
    pHlp->pfnSSMPutMem(pSSM, afActiveStrms, sizeof(afActiveStrms));

    LogFlowFuncLeaveRC(VINF_SUCCESS);
    return VINF_SUCCESS;
}

/**
 * Loads an AC'97 stream from SSM.
 *
 * @returns IPRT status code.
 * @param   pDevIns             The device instance.
 * @param   pSSM                Saved state manager (SSM) handle to use.
 * @param   pStream             AC'97 stream to load.
 */
static int ichac97R3LoadStream(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, PAC97STREAM pStream)
{
    PAC97BMREGS   pRegs = &pStream->Regs;
    PCPDMDEVHLPR3 pHlp  = pDevIns->pHlpR3;

    pHlp->pfnSSMGetU32(pSSM, &pRegs->bdbar);
    pHlp->pfnSSMGetU8( pSSM, &pRegs->civ);
    pHlp->pfnSSMGetU8( pSSM, &pRegs->lvi);
    pHlp->pfnSSMGetU16(pSSM, &pRegs->sr);
    pHlp->pfnSSMGetU16(pSSM, &pRegs->picb);
    pHlp->pfnSSMGetU8( pSSM, &pRegs->piv);
    pHlp->pfnSSMGetU8( pSSM, &pRegs->cr);
    pHlp->pfnSSMGetS32(pSSM, &pRegs->bd_valid);
    pHlp->pfnSSMGetU32(pSSM, &pRegs->bd.addr);
    return pHlp->pfnSSMGetU32(pSSM, &pRegs->bd.ctl_len);
}

/**
 * @callback_method_impl{FNSSMDEVLOADEXEC}
 */
static DECLCALLBACK(int) ichac97R3LoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    PAC97STATE    pThis   = PDMDEVINS_2_DATA(pDevIns, PAC97STATE);
    PAC97STATER3  pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PAC97STATER3);
    PCPDMDEVHLPR3 pHlp    = pDevIns->pHlpR3;

    LogRel2(("ichac97LoadExec: uVersion=%RU32, uPass=0x%x\n", uVersion, uPass));

    AssertMsgReturn (uVersion == AC97_SAVED_STATE_VERSION, ("%RU32\n", uVersion), VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION);
    Assert(uPass == SSM_PASS_FINAL); NOREF(uPass);

    pHlp->pfnSSMGetU32(pSSM, &pThis->glob_cnt);
    pHlp->pfnSSMGetU32(pSSM, &pThis->glob_sta);
    pHlp->pfnSSMGetU32(pSSM, &pThis->cas);

    /*
     * The order the streams are loaded here is critical (defined by
     * AC97SOUNDSOURCE_XX_INDEX), so don't touch!
     */
    for (unsigned i = 0; i < AC97_MAX_STREAMS; i++)
    {
        int rc2 = ichac97R3LoadStream(pDevIns, pSSM, &pThis->aStreams[i]);
        AssertRCReturn(rc2, rc2);
    }

    pHlp->pfnSSMGetMem(pSSM, pThis->mixer_data, sizeof(pThis->mixer_data));

    ichac97R3MixerRecordSelect(pThis, ichac97MixerGet(pThis, AC97_Record_Select));
    ichac97R3MixerSetVolume(pThis, pThisCC, AC97_Master_Volume_Mute,    PDMAUDIOMIXERCTL_VOLUME_MASTER,
                            ichac97MixerGet(pThis, AC97_Master_Volume_Mute));
    ichac97R3MixerSetVolume(pThis, pThisCC, AC97_PCM_Out_Volume_Mute,   PDMAUDIOMIXERCTL_FRONT,
                            ichac97MixerGet(pThis, AC97_PCM_Out_Volume_Mute));
    ichac97R3MixerSetVolume(pThis, pThisCC, AC97_Line_In_Volume_Mute,   PDMAUDIOMIXERCTL_LINE_IN,
                            ichac97MixerGet(pThis, AC97_Line_In_Volume_Mute));
    ichac97R3MixerSetVolume(pThis, pThisCC, AC97_Mic_Volume_Mute,       PDMAUDIOMIXERCTL_MIC_IN,
                            ichac97MixerGet(pThis, AC97_Mic_Volume_Mute));
    ichac97R3MixerSetGain(pThis, pThisCC, AC97_Record_Gain_Mic_Mute,    PDMAUDIOMIXERCTL_MIC_IN,
                          ichac97MixerGet(pThis, AC97_Record_Gain_Mic_Mute));
    ichac97R3MixerSetGain(pThis, pThisCC, AC97_Record_Gain_Mute,        PDMAUDIOMIXERCTL_LINE_IN,
                          ichac97MixerGet(pThis, AC97_Record_Gain_Mute));
    if (pThis->enmCodecModel == AC97CODEC_AD1980)
        if (ichac97MixerGet(pThis, AC97_AD_Misc) & AC97_AD_MISC_HPSEL)
            ichac97R3MixerSetVolume(pThis, pThisCC, AC97_Headphone_Volume_Mute, PDMAUDIOMIXERCTL_VOLUME_MASTER,
                                    ichac97MixerGet(pThis, AC97_Headphone_Volume_Mute));

    /*
     * Again the stream order is set is stone.
     */
    uint8_t afActiveStrms[AC97SOUNDSOURCE_MAX];
    int rc2 = pHlp->pfnSSMGetMem(pSSM, afActiveStrms, sizeof(afActiveStrms));
    AssertRCReturn(rc2, rc2);

    for (unsigned i = 0; i < AC97_MAX_STREAMS; i++)
    {
        const bool          fEnable   = RT_BOOL(afActiveStrms[i]);
        const PAC97STREAM   pStream   = &pThis->aStreams[i];
        const PAC97STREAMR3 pStreamCC = &pThisCC->aStreams[i];

        rc2 = ichac97R3StreamEnable(pThis, pThisCC, pStream, pStreamCC, fEnable);
        AssertRC(rc2);
        if (   fEnable
            && RT_SUCCESS(rc2))
        {
            /* Re-arm the timer for this stream. */
            /** @todo r=aeichner This causes a VM hang upon saved state resume when NetBSD is used as a guest
             * Stopping the timer if cTransferTicks is 0 is a workaround but needs further investigation,
             * see @bugref{9759} for more information. */
            if (pStreamCC->State.cTransferTicks)
                ichac97R3TimerSet(pDevIns, pStream, pStreamCC->State.cTransferTicks);
            else
                PDMDevHlpTimerStop(pDevIns, pStream->hTimer);
        }

        /* Keep going. */
    }

    pThis->bup_flag  = 0;
    pThis->last_samp = 0;

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) ichac97R3QueryInterface(struct PDMIBASE *pInterface, const char *pszIID)
{
    PAC97STATER3 pThisCC = RT_FROM_MEMBER(pInterface, AC97STATER3, IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pThisCC->IBase);
    return NULL;
}


/**
 * Powers off the device.
 *
 * @param   pDevIns             Device instance to power off.
 */
static DECLCALLBACK(void) ichac97R3PowerOff(PPDMDEVINS pDevIns)
{
    PAC97STATE   pThis   = PDMDEVINS_2_DATA(pDevIns, PAC97STATE);
    PAC97STATER3 pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PAC97STATER3);

    LogRel2(("AC97: Powering off ...\n"));

    /* Note: Involves mixer stream / sink destruction, so also do this here
     *       instead of in ichac97R3Destruct(). */
    ichac97R3StreamsDestroy(pThis, pThisCC);

    /*
     * Note: Destroy the mixer while powering off and *not* in ichac97R3Destruct,
     *       giving the mixer the chance to release any references held to
     *       PDM audio streams it maintains.
     */
    if (pThisCC->pMixer)
    {
        AudioMixerDestroy(pThisCC->pMixer);
        pThisCC->pMixer = NULL;
    }
}


/**
 * @interface_method_impl{PDMDEVREG,pfnReset}
 *
 * @remarks The original sources didn't install a reset handler, but it seems to
 *          make sense to me so we'll do it.
 */
static DECLCALLBACK(void) ichac97R3Reset(PPDMDEVINS pDevIns)
{
    PAC97STATE   pThis   = PDMDEVINS_2_DATA(pDevIns, PAC97STATE);
    PAC97STATER3 pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PAC97STATER3);

    LogRel(("AC97: Reset\n"));

    /*
     * Reset the mixer too. The Windows XP driver seems to rely on
     * this. At least it wants to read the vendor id before it resets
     * the codec manually.
     */
    ichac97R3MixerReset(pThis, pThisCC);

    /*
     * Reset all streams.
     */
    for (unsigned i = 0; i < AC97_MAX_STREAMS; i++)
    {
        ichac97R3StreamEnable(pThis, pThisCC, &pThis->aStreams[i], &pThisCC->aStreams[i], false /* fEnable */);
        ichac97R3StreamReset(pThis, &pThis->aStreams[i], &pThisCC->aStreams[i]);
    }

    /*
     * Reset mixer sinks.
     *
     * Do the reset here instead of in ichac97R3StreamReset();
     * the mixer sink(s) might still have data to be processed when an audio stream gets reset.
     */
    AudioMixerSinkReset(pThisCC->pSinkLineIn);
    AudioMixerSinkReset(pThisCC->pSinkMicIn);
    AudioMixerSinkReset(pThisCC->pSinkOut);
}


/**
 * Attach command, internal version.
 *
 * This is called to let the device attach to a driver for a specified LUN
 * during runtime. This is not called during VM construction, the device
 * constructor has to attach to all the available drivers.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pThisCC     The ring-3 AC'97 device state.
 * @param   iLun        The logical unit which is being attached.
 * @param   fFlags      Flags, combination of the PDMDEVATT_FLAGS_* \#defines.
 * @param   ppDrv       Attached driver instance on success. Optional.
 */
static int ichac97R3AttachInternal(PPDMDEVINS pDevIns, PAC97STATER3 pThisCC, unsigned iLun, uint32_t fFlags, PAC97DRIVER *ppDrv)
{
    RT_NOREF(fFlags);

    /*
     * Attach driver.
     */
    char *pszDesc;
    if (RTStrAPrintf(&pszDesc, "Audio driver port (AC'97) for LUN #%u", iLun) <= 0)
        AssertLogRelFailedReturn(VERR_NO_MEMORY);

    PPDMIBASE pDrvBase;
    int rc = PDMDevHlpDriverAttach(pDevIns, iLun, &pThisCC->IBase, &pDrvBase, pszDesc);
    if (RT_SUCCESS(rc))
    {
        PAC97DRIVER pDrv = (PAC97DRIVER)RTMemAllocZ(sizeof(AC97DRIVER));
        if (pDrv)
        {
            pDrv->pDrvBase   = pDrvBase;
            pDrv->pConnector = PDMIBASE_QUERY_INTERFACE(pDrvBase, PDMIAUDIOCONNECTOR);
            AssertMsg(pDrv->pConnector != NULL, ("Configuration error: LUN #%u has no host audio interface, rc=%Rrc\n", iLun, rc));
            pDrv->uLUN       = iLun;
            pDrv->pszDesc    = pszDesc;

            /*
             * For now we always set the driver at LUN 0 as our primary
             * host backend. This might change in the future.
             */
            if (iLun == 0)
                pDrv->fFlags |= PDMAUDIODRVFLAGS_PRIMARY;

            LogFunc(("LUN#%u: pCon=%p, drvFlags=0x%x\n", iLun, pDrv->pConnector, pDrv->fFlags));

            /* Attach to driver list if not attached yet. */
            if (!pDrv->fAttached)
            {
                RTListAppend(&pThisCC->lstDrv, &pDrv->Node);
                pDrv->fAttached = true;
            }

            if (ppDrv)
                *ppDrv = pDrv;
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
        LogFunc(("No attached driver for LUN #%u\n", iLun));

    if (RT_FAILURE(rc))
    {
        /* Only free this string on failure;
         * must remain valid for the live of the driver instance. */
        RTStrFree(pszDesc);
    }

    LogFunc(("iLun=%u, fFlags=0x%x, rc=%Rrc\n", iLun, fFlags, rc));
    return rc;
}

/**
 * Detach command, internal version.
 *
 * This is called to let the device detach from a driver for a specified LUN
 * during runtime.
 *
 * @returns VBox status code.
 * @param   pThisCC     The ring-3 AC'97 device state.
 * @param   pDrv        Driver to detach from device.
 * @param   fFlags      Flags, combination of the PDMDEVATT_FLAGS_* \#defines.
 */
static int ichac97R3DetachInternal(PAC97STATER3 pThisCC, PAC97DRIVER pDrv, uint32_t fFlags)
{
    RT_NOREF(fFlags);

    /* First, remove the driver from our list and destory it's associated streams.
     * This also will un-set the driver as a recording source (if associated). */
    ichac97R3MixerRemoveDrv(pThisCC, pDrv);

    /* Next, search backwards for a capable (attached) driver which now will be the
     * new recording source. */
    PDMAUDIODSTSRCUNION dstSrc;
    PAC97DRIVER pDrvCur;
    RTListForEachReverse(&pThisCC->lstDrv, pDrvCur, AC97DRIVER, Node)
    {
        if (!pDrvCur->pConnector)
            continue;

        PDMAUDIOBACKENDCFG Cfg;
        int rc2 = pDrvCur->pConnector->pfnGetConfig(pDrvCur->pConnector, &Cfg);
        if (RT_FAILURE(rc2))
            continue;

        dstSrc.enmSrc = PDMAUDIORECSRC_MIC;
        PAC97DRIVERSTREAM pDrvStrm = ichac97R3MixerGetDrvStream(pDrvCur, PDMAUDIODIR_IN, dstSrc);
        if (   pDrvStrm
            && pDrvStrm->pMixStrm)
        {
            rc2 = AudioMixerSinkSetRecordingSource(pThisCC->pSinkMicIn, pDrvStrm->pMixStrm);
            if (RT_SUCCESS(rc2))
                LogRel2(("AC97: Set new recording source for 'Mic In' to '%s'\n", Cfg.szName));
        }

        dstSrc.enmSrc = PDMAUDIORECSRC_LINE;
        pDrvStrm = ichac97R3MixerGetDrvStream(pDrvCur, PDMAUDIODIR_IN, dstSrc);
        if (   pDrvStrm
            && pDrvStrm->pMixStrm)
        {
            rc2 = AudioMixerSinkSetRecordingSource(pThisCC->pSinkLineIn, pDrvStrm->pMixStrm);
            if (RT_SUCCESS(rc2))
                LogRel2(("AC97: Set new recording source for 'Line In' to '%s'\n", Cfg.szName));
        }
    }

    LogFunc(("uLUN=%u, fFlags=0x%x\n", pDrv->uLUN, fFlags));
    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMDEVREGR3,pfnAttach}
 */
static DECLCALLBACK(int) ichac97R3Attach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    PAC97STATE   pThis   = PDMDEVINS_2_DATA(pDevIns, PAC97STATE);
    PAC97STATER3 pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PAC97STATER3);

    LogFunc(("iLUN=%u, fFlags=0x%x\n", iLUN, fFlags));

    DEVAC97_LOCK(pDevIns, pThis);

    PAC97DRIVER pDrv;
    int rc2 = ichac97R3AttachInternal(pDevIns, pThisCC, iLUN, fFlags, &pDrv);
    if (RT_SUCCESS(rc2))
        rc2 = ichac97R3MixerAddDrv(pThisCC, pDrv);

    if (RT_FAILURE(rc2))
        LogFunc(("Failed with %Rrc\n", rc2));

    DEVAC97_UNLOCK(pDevIns, pThis);

    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMDEVREG,pfnDetach}
 */
static DECLCALLBACK(void) ichac97R3Detach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    PAC97STATE   pThis   = PDMDEVINS_2_DATA(pDevIns, PAC97STATE);
    PAC97STATER3 pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PAC97STATER3);

    LogFunc(("iLUN=%u, fFlags=0x%x\n", iLUN, fFlags));

    DEVAC97_LOCK(pDevIns, pThis);

    PAC97DRIVER pDrv, pDrvNext;
    RTListForEachSafe(&pThisCC->lstDrv, pDrv, pDrvNext, AC97DRIVER, Node)
    {
        if (pDrv->uLUN == iLUN)
        {
            int rc2 = ichac97R3DetachInternal(pThisCC, pDrv, fFlags);
            if (RT_SUCCESS(rc2))
            {
                RTStrFree(pDrv->pszDesc);
                RTMemFree(pDrv);
                pDrv = NULL;
            }

            break;
        }
    }

    DEVAC97_UNLOCK(pDevIns, pThis);
}

/**
 * Replaces a driver with a the NullAudio drivers.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pThisCC     The ring-3 AC'97 device state.
 * @param   iLun        The logical unit which is being replaced.
 */
static int ichac97R3ReconfigLunWithNullAudio(PPDMDEVINS pDevIns, PAC97STATER3 pThisCC, unsigned iLun)
{
    int rc = PDMDevHlpDriverReconfigure2(pDevIns, iLun, "AUDIO", "NullAudio");
    if (RT_SUCCESS(rc))
        rc = ichac97R3AttachInternal(pDevIns,  pThisCC, iLun, 0 /* fFlags */, NULL /* ppDrv */);
    LogFunc(("pThisCC=%p, iLun=%u, rc=%Rrc\n", pThisCC, iLun, rc));
    return rc;
}

/**
 * @interface_method_impl{PDMDEVREG,pfnDestruct}
 */
static DECLCALLBACK(int) ichac97R3Destruct(PPDMDEVINS pDevIns)
{
    PDMDEV_CHECK_VERSIONS_RETURN_QUIET(pDevIns); /* this shall come first */
    PAC97STATER3 pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PAC97STATER3);

    LogFlowFuncEnter();

    PAC97DRIVER pDrv, pDrvNext;
    RTListForEachSafe(&pThisCC->lstDrv, pDrv, pDrvNext, AC97DRIVER, Node)
    {
        RTListNodeRemove(&pDrv->Node);
        RTMemFree(pDrv->pszDesc);
        RTMemFree(pDrv);
    }

    /* Sanity. */
    Assert(RTListIsEmpty(&pThisCC->lstDrv));

    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int) ichac97R3Construct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns); /* this shall come first */
    PAC97STATE      pThis   = PDMDEVINS_2_DATA(pDevIns, PAC97STATE);
    PAC97STATER3    pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PAC97STATER3);
    PCPDMDEVHLPR3   pHlp    = pDevIns->pHlpR3;
    Assert(iInstance == 0); RT_NOREF(iInstance);

    /*
     * Initialize data so we can run the destructor without scewing up.
     */
    pThisCC->pDevIns                  = pDevIns;
    pThisCC->IBase.pfnQueryInterface  = ichac97R3QueryInterface;
    RTListInit(&pThisCC->lstDrv);

    /*
     * Validate and read configuration.
     */
    PDMDEV_VALIDATE_CONFIG_RETURN(pDevIns, "Codec|TimerHz|DebugEnabled|DebugPathOut", "");

    char szCodec[20];
    int rc = pHlp->pfnCFGMQueryStringDef(pCfg, "Codec", &szCodec[0], sizeof(szCodec), "STAC9700");
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES,
                                N_("AC'97 configuration error: Querying \"Codec\" as string failed"));

    rc = pHlp->pfnCFGMQueryU16Def(pCfg, "TimerHz", &pThis->uTimerHz, AC97_TIMER_HZ_DEFAULT /* Default value, if not set. */);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("AC'97 configuration error: failed to read Hertz (Hz) rate as unsigned integer"));

    if (pThis->uTimerHz != AC97_TIMER_HZ_DEFAULT)
        LogRel(("AC97: Using custom device timer rate (%RU16Hz)\n", pThis->uTimerHz));

    rc = pHlp->pfnCFGMQueryBoolDef(pCfg, "DebugEnabled", &pThisCC->Dbg.fEnabled, false);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("AC97 configuration error: failed to read debugging enabled flag as boolean"));

    rc = pHlp->pfnCFGMQueryStringAllocDef(pCfg, "DebugPathOut", &pThisCC->Dbg.pszOutPath, VBOX_AUDIO_DEBUG_DUMP_PCM_DATA_PATH);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("AC97 configuration error: failed to read debugging output path flag as string"));

    if (pThisCC->Dbg.fEnabled)
        LogRel2(("AC97: Debug output will be saved to '%s'\n", pThisCC->Dbg.pszOutPath));

    /*
     * The AD1980 codec (with corresponding PCI subsystem vendor ID) is whitelisted
     * in the Linux kernel; Linux makes no attempt to measure the data rate and assumes
     * 48 kHz rate, which is exactly what we need. Same goes for AD1981B.
     */
    if (!strcmp(szCodec, "STAC9700"))
        pThis->enmCodecModel = AC97CODEC_STAC9700;
    else if (!strcmp(szCodec, "AD1980"))
        pThis->enmCodecModel = AC97CODEC_AD1980;
    else if (!strcmp(szCodec, "AD1981B"))
        pThis->enmCodecModel = AC97CODEC_AD1981B;
    else
        return PDMDevHlpVMSetError(pDevIns, VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES, RT_SRC_POS,
                                   N_("AC'97 configuration error: The \"Codec\" value \"%s\" is unsupported"), szCodec);

    LogRel(("AC97: Using codec '%s'\n", szCodec));

    /*
     * Use an own critical section for the device instead of the default
     * one provided by PDM. This allows fine-grained locking in combination
     * with TM when timer-specific stuff is being called in e.g. the MMIO handlers.
     */
    rc = PDMDevHlpCritSectInit(pDevIns, &pThis->CritSect, RT_SRC_POS, "AC'97");
    AssertRCReturn(rc, rc);

    rc = PDMDevHlpSetDeviceCritSect(pDevIns, PDMDevHlpCritSectGetNop(pDevIns));
    AssertRCReturn(rc, rc);

    /*
     * Initialize data (most of it anyway).
     */
    /* PCI Device */
    PPDMPCIDEV pPciDev = pDevIns->apPciDevs[0];
    PCIDevSetVendorId(pPciDev,              0x8086); /* 00 ro - intel. */               Assert(pPciDev->abConfig[0x00] == 0x86); Assert(pPciDev->abConfig[0x01] == 0x80);
    PCIDevSetDeviceId(pPciDev,              0x2415); /* 02 ro - 82801 / 82801aa(?). */  Assert(pPciDev->abConfig[0x02] == 0x15); Assert(pPciDev->abConfig[0x03] == 0x24);
    PCIDevSetCommand(pPciDev,               0x0000); /* 04 rw,ro - pcicmd. */           Assert(pPciDev->abConfig[0x04] == 0x00); Assert(pPciDev->abConfig[0x05] == 0x00);
    PCIDevSetStatus(pPciDev,                VBOX_PCI_STATUS_DEVSEL_MEDIUM |  VBOX_PCI_STATUS_FAST_BACK); /* 06 rwc?,ro? - pcists. */  Assert(pPciDev->abConfig[0x06] == 0x80); Assert(pPciDev->abConfig[0x07] == 0x02);
    PCIDevSetRevisionId(pPciDev,            0x01);   /* 08 ro - rid. */                 Assert(pPciDev->abConfig[0x08] == 0x01);
    PCIDevSetClassProg(pPciDev,             0x00);   /* 09 ro - pi. */                  Assert(pPciDev->abConfig[0x09] == 0x00);
    PCIDevSetClassSub(pPciDev,              0x01);   /* 0a ro - scc; 01 == Audio. */    Assert(pPciDev->abConfig[0x0a] == 0x01);
    PCIDevSetClassBase(pPciDev,             0x04);   /* 0b ro - bcc; 04 == multimedia.*/Assert(pPciDev->abConfig[0x0b] == 0x04);
    PCIDevSetHeaderType(pPciDev,            0x00);   /* 0e ro - headtyp. */             Assert(pPciDev->abConfig[0x0e] == 0x00);
    PCIDevSetBaseAddress(pPciDev,           0,       /* 10 rw - nambar - native audio mixer base. */
                           true /* fIoSpace */, false /* fPrefetchable */, false /* f64Bit */, 0x00000000); Assert(pPciDev->abConfig[0x10] == 0x01); Assert(pPciDev->abConfig[0x11] == 0x00); Assert(pPciDev->abConfig[0x12] == 0x00); Assert(pPciDev->abConfig[0x13] == 0x00);
    PCIDevSetBaseAddress(pPciDev,           1,       /* 14 rw - nabmbar - native audio bus mastering. */
                         true /* fIoSpace */, false /* fPrefetchable */, false /* f64Bit */, 0x00000000); Assert(pPciDev->abConfig[0x14] == 0x01); Assert(pPciDev->abConfig[0x15] == 0x00); Assert(pPciDev->abConfig[0x16] == 0x00); Assert(pPciDev->abConfig[0x17] == 0x00);
    PCIDevSetInterruptLine(pPciDev,         0x00);   /* 3c rw. */                       Assert(pPciDev->abConfig[0x3c] == 0x00);
    PCIDevSetInterruptPin(pPciDev,          0x01);   /* 3d ro - INTA#. */               Assert(pPciDev->abConfig[0x3d] == 0x01);

    if (pThis->enmCodecModel == AC97CODEC_AD1980)
    {
        PCIDevSetSubSystemVendorId(pPciDev, 0x1028); /* 2c ro - Dell.) */
        PCIDevSetSubSystemId(pPciDev,       0x0177); /* 2e ro. */
    }
    else if (pThis->enmCodecModel == AC97CODEC_AD1981B)
    {
        PCIDevSetSubSystemVendorId(pPciDev, 0x1028); /* 2c ro - Dell.) */
        PCIDevSetSubSystemId(pPciDev,       0x01ad); /* 2e ro. */
    }
    else
    {
        PCIDevSetSubSystemVendorId(pPciDev, 0x8086); /* 2c ro - Intel.) */
        PCIDevSetSubSystemId(pPciDev,       0x0000); /* 2e ro. */
    }

    /*
     * Register the PCI device and associated I/O regions.
     */
    rc = PDMDevHlpPCIRegister(pDevIns, pPciDev);
    if (RT_FAILURE(rc))
        return rc;

    rc = PDMDevHlpPCIIORegionCreateIo(pDevIns, 0 /*iPciRegion*/, 256 /*cPorts*/,
                                      ichac97IoPortNamWrite, ichac97IoPortNamRead, NULL /*pvUser*/,
                                      "ICHAC97 NAM", NULL /*paExtDescs*/, &pThis->hIoPortsNam);
    AssertRCReturn(rc, rc);

    rc = PDMDevHlpPCIIORegionCreateIo(pDevIns, 1 /*iPciRegion*/, 64 /*cPorts*/,
                                      ichac97IoPortNabmWrite, ichac97IoPortNabmRead, NULL /*pvUser*/,
                                      "ICHAC97 NABM", g_aNabmPorts, &pThis->hIoPortsNabm);
    AssertRCReturn(rc, rc);

    /*
     * Saved state.
     */
    rc = PDMDevHlpSSMRegister(pDevIns, AC97_SAVED_STATE_VERSION, sizeof(*pThis), ichac97R3SaveExec, ichac97R3LoadExec);
    if (RT_FAILURE(rc))
        return rc;

# ifdef VBOX_WITH_AUDIO_AC97_ASYNC_IO
    LogRel(("AC97: Asynchronous I/O enabled\n"));
# endif

    /*
     * Attach drivers.  We ASSUME they are configured consecutively without any
     * gaps, so we stop when we hit the first LUN w/o a driver configured.
     */
    for (unsigned iLun = 0; ; iLun++)
    {
        AssertBreak(iLun < UINT8_MAX);
        LogFunc(("Trying to attach driver for LUN#%u ...\n", iLun));
        rc = ichac97R3AttachInternal(pDevIns, pThisCC, iLun, 0 /* fFlags */, NULL /* ppDrv */);
        if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
        {
            LogFunc(("cLUNs=%u\n", iLun));
            break;
        }
        if (rc == VERR_AUDIO_BACKEND_INIT_FAILED)
        {
            ichac97R3ReconfigLunWithNullAudio(pDevIns, pThisCC, iLun); /* Pretend attaching to the NULL audio backend will never fail. */
            PDMDevHlpVMSetRuntimeError(pDevIns, 0 /*fFlags*/, "HostAudioNotResponding",
                                       N_("Host audio backend initialization has failed. "
                                          "Selecting the NULL audio backend with the consequence that no sound is audible"));
        }
        else
            AssertLogRelMsgReturn(RT_SUCCESS(rc),  ("LUN#%u: rc=%Rrc\n", iLun, rc), rc);
    }

    rc = AudioMixerCreate("AC'97 Mixer", 0 /* uFlags */, &pThisCC->pMixer);
    AssertRCReturn(rc, rc);
    rc = AudioMixerCreateSink(pThisCC->pMixer, "[Recording] Line In", AUDMIXSINKDIR_INPUT, &pThisCC->pSinkLineIn);
    AssertRCReturn(rc, rc);
    rc = AudioMixerCreateSink(pThisCC->pMixer, "[Recording] Microphone In", AUDMIXSINKDIR_INPUT, &pThisCC->pSinkMicIn);
    AssertRCReturn(rc, rc);
    rc = AudioMixerCreateSink(pThisCC->pMixer, "[Playback] PCM Output", AUDMIXSINKDIR_OUTPUT, &pThisCC->pSinkOut);
    AssertRCReturn(rc, rc);

    /*
     * Create all hardware streams.
     */
    AssertCompile(RT_ELEMENTS(pThis->aStreams) == AC97_MAX_STREAMS);
    for (unsigned i = 0; i < AC97_MAX_STREAMS; i++)
    {
        rc = ichac97R3StreamCreate(pThisCC, &pThis->aStreams[i], &pThisCC->aStreams[i], i /* SD# */);
        AssertRCReturn(rc, rc);
    }

    /*
     * Create the emulation timers (one per stream).
     *
     * We must the critical section for the timers as the device has a
     * noop section associated with it.
     *
     * Note:  Use TMCLOCK_VIRTUAL_SYNC here, as the guest's AC'97 driver
     *        relies on exact (virtual) DMA timing and uses DMA Position Buffers
     *        instead of the LPIB registers.
     */
    static const char * const s_apszNames[] = { "AC97 PI", "AC97 PO", "AC97 MC" };
    AssertCompile(RT_ELEMENTS(s_apszNames) == AC97_MAX_STREAMS);
    for (unsigned i = 0; i < AC97_MAX_STREAMS; i++)
    {
        rc = PDMDevHlpTimerCreate(pDevIns, TMCLOCK_VIRTUAL_SYNC, ichac97R3Timer, &pThis->aStreams[i],
                                  TMTIMER_FLAGS_NO_CRIT_SECT, s_apszNames[i], &pThis->aStreams[i].hTimer);
        AssertRCReturn(rc, rc);

        rc = PDMDevHlpTimerSetCritSect(pDevIns, pThis->aStreams[i].hTimer, &pThis->CritSect);
        AssertRCReturn(rc, rc);
    }


# ifdef VBOX_WITH_AUDIO_AC97_ONETIME_INIT
    PAC97DRIVER pDrv;
    RTListForEach(&pThisCC->lstDrv, pDrv, AC97DRIVER, Node)
    {
        /*
         * Only primary drivers are critical for the VM to run. Everything else
         * might not worth showing an own error message box in the GUI.
         */
        if (!(pDrv->fFlags & PDMAUDIODRVFLAGS_PRIMARY))
            continue;

        PPDMIAUDIOCONNECTOR pCon = pDrv->pConnector;
        AssertPtr(pCon);

        bool fValidLineIn = AudioMixerStreamIsValid(pDrv->LineIn.pMixStrm);
        bool fValidMicIn  = AudioMixerStreamIsValid(pDrv->MicIn.pMixStrm);
        bool fValidOut    = AudioMixerStreamIsValid(pDrv->Out.pMixStrm);

        if (    !fValidLineIn
             && !fValidMicIn
             && !fValidOut)
        {
            LogRel(("AC97: Falling back to NULL backend (no sound audible)\n"));
            ichac97R3Reset(pDevIns);
            ichac97R3ReconfigLunWithNullAudio(pdEvIns, pThsiCC, iLun);
            PDMDevHlpVMSetRuntimeError(pDevIns, 0 /*fFlags*/, "HostAudioNotResponding",
                                       N_("No audio devices could be opened. "
                                          "Selecting the NULL audio backend with the consequence that no sound is audible"));
        }
        else
        {
            bool fWarn = false;

            PDMAUDIOBACKENDCFG backendCfg;
            int rc2 = pCon->pfnGetConfig(pCon, &backendCfg);
            if (RT_SUCCESS(rc2))
            {
                if (backendCfg.cMaxStreamsIn)
                {
                    /* If the audio backend supports two or more input streams at once,
                     * warn if one of our two inputs (microphone-in and line-in) failed to initialize. */
                    if (backendCfg.cMaxStreamsIn >= 2)
                        fWarn = !fValidLineIn || !fValidMicIn;
                    /* If the audio backend only supports one input stream at once (e.g. pure ALSA, and
                     * *not* ALSA via PulseAudio plugin!), only warn if both of our inputs failed to initialize.
                     * One of the two simply is not in use then. */
                    else if (backendCfg.cMaxStreamsIn == 1)
                        fWarn = !fValidLineIn && !fValidMicIn;
                    /* Don't warn if our backend is not able of supporting any input streams at all. */
                }

                if (   !fWarn
                    && backendCfg.cMaxStreamsOut)
                {
                    fWarn = !fValidOut;
                }
            }
            else
            {
                LogRel(("AC97: Unable to retrieve audio backend configuration for LUN #%RU8, rc=%Rrc\n", pDrv->uLUN, rc2));
                fWarn = true;
            }

            if (fWarn)
            {
                char   szMissingStreams[255] = "";
                size_t len = 0;
                if (!fValidLineIn)
                {
                    LogRel(("AC97: WARNING: Unable to open PCM line input for LUN #%RU8!\n", pDrv->uLUN));
                    len = RTStrPrintf(szMissingStreams, sizeof(szMissingStreams), "PCM Input");
                }
                if (!fValidMicIn)
                {
                    LogRel(("AC97: WARNING: Unable to open PCM microphone input for LUN #%RU8!\n", pDrv->uLUN));
                    len += RTStrPrintf(szMissingStreams + len,
                                       sizeof(szMissingStreams) - len, len ? ", PCM Microphone" : "PCM Microphone");
                }
                if (!fValidOut)
                {
                    LogRel(("AC97: WARNING: Unable to open PCM output for LUN #%RU8!\n", pDrv->uLUN));
                    len += RTStrPrintf(szMissingStreams + len,
                                       sizeof(szMissingStreams) - len, len ? ", PCM Output" : "PCM Output");
                }

                PDMDevHlpVMSetRuntimeError(pDevIns, 0 /*fFlags*/, "HostAudioNotResponding",
                                           N_("Some AC'97 audio streams (%s) could not be opened. Guest applications generating audio "
                                              "output or depending on audio input may hang. Make sure your host audio device "
                                              "is working properly. Check the logfile for error messages of the audio "
                                              "subsystem"), szMissingStreams);
            }
        }
    }
# endif /* VBOX_WITH_AUDIO_AC97_ONETIME_INIT */

    ichac97R3Reset(pDevIns);

    /*
     * Register statistics.
     */
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatUnimplementedNabmReads,  STAMTYPE_COUNTER, "UnimplementedNabmReads", STAMUNIT_OCCURENCES, "Unimplemented NABM register reads.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatUnimplementedNabmWrites, STAMTYPE_COUNTER, "UnimplementedNabmWrites", STAMUNIT_OCCURENCES, "Unimplemented NABM register writes.");
# ifdef VBOX_WITH_STATISTICS
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatTimer,        STAMTYPE_PROFILE, "Timer",        STAMUNIT_TICKS_PER_CALL, "Profiling ichac97Timer.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatIn,           STAMTYPE_PROFILE, "Input",        STAMUNIT_TICKS_PER_CALL, "Profiling input.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatOut,          STAMTYPE_PROFILE, "Output",       STAMUNIT_TICKS_PER_CALL, "Profiling output.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatBytesRead,    STAMTYPE_COUNTER, "BytesRead"   , STAMUNIT_BYTES,          "Bytes read from AC97 emulation.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatBytesWritten, STAMTYPE_COUNTER, "BytesWritten", STAMUNIT_BYTES,          "Bytes written to AC97 emulation.");
# endif

    LogFlowFuncLeaveRC(VINF_SUCCESS);
    return VINF_SUCCESS;
}

#else /* !IN_RING3 */

/**
 * @callback_method_impl{PDMDEVREGR0,pfnConstruct}
 */
static DECLCALLBACK(int) ichac97RZConstruct(PPDMDEVINS pDevIns)
{
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);
    PAC97STATE pThis = PDMDEVINS_2_DATA(pDevIns, PAC97STATE);

    int rc = PDMDevHlpSetDeviceCritSect(pDevIns, PDMDevHlpCritSectGetNop(pDevIns));
    AssertRCReturn(rc, rc);

    rc = PDMDevHlpIoPortSetUpContext(pDevIns, pThis->hIoPortsNam, ichac97IoPortNamWrite, ichac97IoPortNamRead, NULL /*pvUser*/);
    AssertRCReturn(rc, rc);
    rc = PDMDevHlpIoPortSetUpContext(pDevIns, pThis->hIoPortsNabm, ichac97IoPortNabmWrite, ichac97IoPortNabmRead, NULL /*pvUser*/);
    AssertRCReturn(rc, rc);

    return VINF_SUCCESS;
}

#endif /* !IN_RING3 */

/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceICHAC97 =
{
    /* .u32Version = */             PDM_DEVREG_VERSION,
    /* .uReserved0 = */             0,
    /* .szName = */                 "ichac97",
    /* .fFlags = */                 PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_RZ | PDM_DEVREG_FLAGS_NEW_STYLE,
    /* .fClass = */                 PDM_DEVREG_CLASS_AUDIO,
    /* .cMaxInstances = */          1,
    /* .uSharedVersion = */         42,
    /* .cbInstanceShared = */       sizeof(AC97STATE),
    /* .cbInstanceCC = */           CTX_EXPR(sizeof(AC97STATER3), 0, 0),
    /* .cbInstanceRC = */           0,
    /* .cMaxPciDevices = */         1,
    /* .cMaxMsixVectors = */        0,
    /* .pszDescription = */         "ICH AC'97 Audio Controller",
#if defined(IN_RING3)
    /* .pszRCMod = */               "VBoxDDRC.rc",
    /* .pszR0Mod = */               "VBoxDDR0.r0",
    /* .pfnConstruct = */           ichac97R3Construct,
    /* .pfnDestruct = */            ichac97R3Destruct,
    /* .pfnRelocate = */            NULL,
    /* .pfnMemSetup = */            NULL,
    /* .pfnPowerOn = */             NULL,
    /* .pfnReset = */               ichac97R3Reset,
    /* .pfnSuspend = */             NULL,
    /* .pfnResume = */              NULL,
    /* .pfnAttach = */              ichac97R3Attach,
    /* .pfnDetach = */              ichac97R3Detach,
    /* .pfnQueryInterface = */      NULL,
    /* .pfnInitComplete = */        NULL,
    /* .pfnPowerOff = */            ichac97R3PowerOff,
    /* .pfnSoftReset = */           NULL,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#elif defined(IN_RING0)
    /* .pfnEarlyConstruct = */      NULL,
    /* .pfnConstruct = */           ichac97RZConstruct,
    /* .pfnDestruct = */            NULL,
    /* .pfnFinalDestruct = */       NULL,
    /* .pfnRequest = */             NULL,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#elif defined(IN_RC)
    /* .pfnConstruct = */           ichac97RZConstruct,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#else
# error "Not in IN_RING3, IN_RING0 or IN_RC!"
#endif
    /* .u32VersionEnd = */          PDM_DEVREG_VERSION
};

#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */

