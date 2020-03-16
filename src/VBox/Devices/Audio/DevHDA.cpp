/* $Id$ */
/** @file
 * DevHDA.cpp - VBox Intel HD Audio Controller.
 *
 * Implemented against the specifications found in "High Definition Audio
 * Specification", Revision 1.0a June 17, 2010, and  "Intel I/O Controller
 * HUB 6 (ICH6) Family, Datasheet", document number 301473-002.
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
#define LOG_GROUP LOG_GROUP_DEV_HDA
#include <VBox/log.h>

#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pdmaudioifs.h>
#include <VBox/version.h>
#include <VBox/AssertGuest.h>

#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/asm-math.h>
#include <iprt/file.h>
#include <iprt/list.h>
# include <iprt/string.h>
#ifdef IN_RING3
# include <iprt/mem.h>
# include <iprt/semaphore.h>
# include <iprt/uuid.h>
#endif

#include "VBoxDD.h"

#include "AudioMixBuffer.h"
#include "AudioMixer.h"

#include "DevHDA.h"
#include "DevHDACommon.h"

#include "HDACodec.h"
#include "HDAStream.h"
#include "HDAStreamMap.h"
#include "HDAStreamPeriod.h"

#include "DrvAudio.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
//#define HDA_AS_PCI_EXPRESS

/* Installs a DMA access handler (via PGM callback) to monitor
 * HDA's DMA operations, that is, writing / reading audio stream data.
 *
 * !!! Note: Certain guests are *that* timing sensitive that when enabling  !!!
 * !!!       such a handler will mess up audio completely (e.g. Windows 7). !!! */
//#define HDA_USE_DMA_ACCESS_HANDLER
#ifdef HDA_USE_DMA_ACCESS_HANDLER
# include <VBox/vmm/pgm.h>
#endif

/* Uses the DMA access handler to read the written DMA audio (output) data.
 * Only valid if HDA_USE_DMA_ACCESS_HANDLER is set.
 *
 * Also see the note / warning for HDA_USE_DMA_ACCESS_HANDLER. */
//# define HDA_USE_DMA_ACCESS_HANDLER_WRITING

/* Useful to debug the device' timing. */
//#define HDA_DEBUG_TIMING

/* To debug silence coming from the guest in form of audio gaps.
 * Very crude implementation for now. */
//#define HDA_DEBUG_SILENCE

#if defined(VBOX_WITH_HP_HDA)
/* HP Pavilion dv4t-1300 */
# define HDA_PCI_VENDOR_ID 0x103c
# define HDA_PCI_DEVICE_ID 0x30f7
#elif defined(VBOX_WITH_INTEL_HDA)
/* Intel HDA controller */
# define HDA_PCI_VENDOR_ID 0x8086
# define HDA_PCI_DEVICE_ID 0x2668
#elif defined(VBOX_WITH_NVIDIA_HDA)
/* nVidia HDA controller */
# define HDA_PCI_VENDOR_ID 0x10de
# define HDA_PCI_DEVICE_ID 0x0ac0
#else
# error "Please specify your HDA device vendor/device IDs"
#endif

/**
 * Acquires the HDA lock.
 */
#define DEVHDA_LOCK(a_pDevIns, a_pThis) \
    do { \
        int rcLock = PDMDevHlpCritSectEnter((a_pDevIns), &(a_pThis)->CritSect, VERR_IGNORED); \
        AssertRC(rcLock); \
    } while (0)

/**
 * Acquires the HDA lock or returns.
 */
#define DEVHDA_LOCK_RETURN(a_pDevIns, a_pThis, a_rcBusy) \
    do { \
        int rcLock = PDMDevHlpCritSectEnter((a_pDevIns), &(a_pThis)->CritSect, a_rcBusy); \
        if (rcLock == VINF_SUCCESS) \
        { /* likely */ } \
        else \
        { \
            AssertRC(rcLock); \
            return rcLock; \
        } \
    } while (0)

/**
 * Acquires the HDA lock or returns.
 */
# define DEVHDA_LOCK_RETURN_VOID(a_pDevIns, a_pThis) \
    do { \
        int rcLock = PDMDevHlpCritSectEnter((a_pDevIns), &(a_pThis)->CritSect, VERR_IGNORED); \
        if (rcLock == VINF_SUCCESS) \
        { /* likely */ } \
        else \
        { \
            AssertRC(rcLock); \
            return; \
        } \
    } while (0)

/**
 * Releases the HDA lock.
 */
#define DEVHDA_UNLOCK(a_pDevIns, a_pThis) \
    do { PDMDevHlpCritSectLeave((a_pDevIns), &(a_pThis)->CritSect); } while (0)

/**
 * Acquires the TM lock and HDA lock, returns on failure.
 */
#define DEVHDA_LOCK_BOTH_RETURN(a_pDevIns, a_pThis, a_pStream, a_rcBusy) \
    do { \
        VBOXSTRICTRC rcLock = PDMDevHlpTimerLockClock2(pDevIns, (a_pStream)->hTimer, &(a_pThis)->CritSect,  (a_rcBusy)); \
        if (RT_LIKELY(rcLock == VINF_SUCCESS)) \
        {  /* likely */ } \
        else \
            return VBOXSTRICTRC_TODO(rcLock); \
    } while (0)


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/**
 * Structure defining a (host backend) driver stream.
 * Each driver has its own instances of audio mixer streams, which then
 * can go into the same (or even different) audio mixer sinks.
 */
typedef struct HDADRIVERSTREAM
{
    /** Associated mixer handle. */
    R3PTRTYPE(PAUDMIXSTREAM)           pMixStrm;
} HDADRIVERSTREAM, *PHDADRIVERSTREAM;

#ifdef HDA_USE_DMA_ACCESS_HANDLER
/**
 * Struct for keeping an HDA DMA access handler context.
 */
typedef struct HDADMAACCESSHANDLER
{
    /** Node for storing this handler in our list in HDASTREAMSTATE. */
    RTLISTNODER3            Node;
    /** Pointer to stream to which this access handler is assigned to. */
    R3PTRTYPE(PHDASTREAM)   pStream;
    /** Access handler type handle. */
    PGMPHYSHANDLERTYPE      hAccessHandlerType;
    /** First address this handler uses. */
    RTGCPHYS                GCPhysFirst;
    /** Last address this handler uses. */
    RTGCPHYS                GCPhysLast;
    /** Actual BDLE address to handle. */
    RTGCPHYS                BDLEAddr;
    /** Actual BDLE buffer size to handle. */
    RTGCPHYS                BDLESize;
    /** Whether the access handler has been registered or not. */
    bool                    fRegistered;
    uint8_t                 Padding[3];
} HDADMAACCESSHANDLER, *PHDADMAACCESSHANDLER;
#endif

/**
 * Struct for maintaining a host backend driver.
 * This driver must be associated to one, and only one,
 * HDA codec. The HDA controller does the actual multiplexing
 * of HDA codec data to various host backend drivers then.
 *
 * This HDA device uses a timer in order to synchronize all
 * read/write accesses across all attached LUNs / backends.
 */
typedef struct HDADRIVER
{
    /** Node for storing this driver in our device driver list of HDASTATE. */
    RTLISTNODER3                       Node;
    /** Pointer to shared HDA device state. */
    R3PTRTYPE(PHDASTATE)               pHDAStateShared;
    /** Pointer to the ring-3 HDA device state. */
    R3PTRTYPE(PHDASTATER3)             pHDAStateR3;
    /** Driver flags. */
    PDMAUDIODRVFLAGS                   fFlags;
    uint8_t                            u32Padding0[2];
    /** LUN to which this driver has been assigned. */
    uint8_t                            uLUN;
    /** Whether this driver is in an attached state or not. */
    bool                               fAttached;
    /** Pointer to attached driver base interface. */
    R3PTRTYPE(PPDMIBASE)               pDrvBase;
    /** Audio connector interface to the underlying host backend. */
    R3PTRTYPE(PPDMIAUDIOCONNECTOR)     pConnector;
    /** Mixer stream for line input. */
    HDADRIVERSTREAM                    LineIn;
#ifdef VBOX_WITH_AUDIO_HDA_MIC_IN
    /** Mixer stream for mic input. */
    HDADRIVERSTREAM                    MicIn;
#endif
    /** Mixer stream for front output. */
    HDADRIVERSTREAM                    Front;
#ifdef VBOX_WITH_AUDIO_HDA_51_SURROUND
    /** Mixer stream for center/LFE output. */
    HDADRIVERSTREAM                    CenterLFE;
    /** Mixer stream for rear output. */
    HDADRIVERSTREAM                    Rear;
#endif
} HDADRIVER;


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
#ifndef VBOX_DEVICE_STRUCT_TESTCASE
#ifdef IN_RING3
static void hdaR3GCTLReset(PPDMDEVINS pDevIns, PHDASTATE pThis, PHDASTATER3 pThisCC);
#endif

/** @name Register read/write stubs.
 * @{
 */
static FNHDAREGREAD  hdaRegReadUnimpl;
static FNHDAREGWRITE hdaRegWriteUnimpl;
/** @} */

/** @name Global register set read/write functions.
 * @{
 */
static FNHDAREGWRITE hdaRegWriteGCTL;
static FNHDAREGREAD  hdaRegReadLPIB;
static FNHDAREGREAD  hdaRegReadWALCLK;
static FNHDAREGWRITE hdaRegWriteCORBWP;
static FNHDAREGWRITE hdaRegWriteCORBRP;
static FNHDAREGWRITE hdaRegWriteCORBCTL;
static FNHDAREGWRITE hdaRegWriteCORBSIZE;
static FNHDAREGWRITE hdaRegWriteCORBSTS;
static FNHDAREGWRITE hdaRegWriteRINTCNT;
static FNHDAREGWRITE hdaRegWriteRIRBWP;
static FNHDAREGWRITE hdaRegWriteRIRBSTS;
static FNHDAREGWRITE hdaRegWriteSTATESTS;
static FNHDAREGWRITE hdaRegWriteIRS;
static FNHDAREGREAD  hdaRegReadIRS;
static FNHDAREGWRITE hdaRegWriteBase;
/** @} */

/** @name {IOB}SDn write functions.
 * @{
 */
static FNHDAREGWRITE hdaRegWriteSDCBL;
static FNHDAREGWRITE hdaRegWriteSDCTL;
static FNHDAREGWRITE hdaRegWriteSDSTS;
static FNHDAREGWRITE hdaRegWriteSDLVI;
static FNHDAREGWRITE hdaRegWriteSDFIFOW;
static FNHDAREGWRITE hdaRegWriteSDFIFOS;
static FNHDAREGWRITE hdaRegWriteSDFMT;
static FNHDAREGWRITE hdaRegWriteSDBDPL;
static FNHDAREGWRITE hdaRegWriteSDBDPU;
/** @} */

/** @name Generic register read/write functions.
 * @{
 */
static FNHDAREGREAD  hdaRegReadU32;
static FNHDAREGWRITE hdaRegWriteU32;
static FNHDAREGREAD  hdaRegReadU24;
#ifdef IN_RING3
static FNHDAREGWRITE hdaRegWriteU24;
#endif
static FNHDAREGREAD  hdaRegReadU16;
static FNHDAREGWRITE hdaRegWriteU16;
static FNHDAREGREAD  hdaRegReadU8;
static FNHDAREGWRITE hdaRegWriteU8;
/** @} */

/** @name HDA device functions.
 * @{
 */
#ifdef IN_RING3
static int                        hdaR3AddStream(PHDASTATER3 pThisCC, PPDMAUDIOSTREAMCFG pCfg);
static int                        hdaR3RemoveStream(PHDASTATER3 pThisCC, PPDMAUDIOSTREAMCFG pCfg);
# ifdef HDA_USE_DMA_ACCESS_HANDLER
static DECLCALLBACK(VBOXSTRICTRC) hdaR3DmaAccessHandler(PVM pVM, PVMCPU pVCpu, RTGCPHYS GCPhys, void *pvPhys,
                                                        void *pvBuf, size_t cbBuf,
                                                        PGMACCESSTYPE enmAccessType, PGMACCESSORIGIN enmOrigin, void *pvUser);
# endif
#endif /* IN_RING3 */
/** @} */

/** @name HDA mixer functions.
 * @{
 */
#ifdef IN_RING3
static int hdaR3MixerAddDrvStream(PAUDMIXSINK pMixSink, PPDMAUDIOSTREAMCFG pCfg, PHDADRIVER pDrv);
#endif
/** @} */

#ifdef IN_RING3
static FNSSMFIELDGETPUT hdaR3GetPutTrans_HDABDLEDESC_fFlags_6;
static FNSSMFIELDGETPUT hdaR3GetPutTrans_HDABDLE_Desc_fFlags_1thru4;
#endif


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/

/** No register description (RD) flags defined. */
#define HDA_RD_F_NONE           0
/** Writes to SD are allowed while RUN bit is set. */
#define HDA_RD_F_SD_WRITE_RUN   RT_BIT(0)

/** Emits a single audio stream register set (e.g. OSD0) at a specified offset. */
#define HDA_REG_MAP_STRM(offset, name) \
    /* offset        size     read mask   write mask  flags                     read callback   write callback     index + abbrev                 description */ \
    /* -------       -------  ----------  ----------  ------------------------- --------------  -----------------  -----------------------------  ----------- */ \
    /* Offset 0x80 (SD0) */ \
    { offset,        0x00003, 0x00FF001F, 0x00F0001F, HDA_RD_F_SD_WRITE_RUN, hdaRegReadU24 , hdaRegWriteSDCTL  , HDA_REG_IDX_STRM(name, CTL)  , #name " Stream Descriptor Control" }, \
    /* Offset 0x83 (SD0) */ \
    { offset + 0x3,  0x00001, 0x0000003C, 0x0000001C, HDA_RD_F_SD_WRITE_RUN, hdaRegReadU8  , hdaRegWriteSDSTS  , HDA_REG_IDX_STRM(name, STS)  , #name " Status" }, \
    /* Offset 0x84 (SD0) */ \
    { offset + 0x4,  0x00004, 0xFFFFFFFF, 0x00000000, HDA_RD_F_NONE,         hdaRegReadLPIB, hdaRegWriteU32    , HDA_REG_IDX_STRM(name, LPIB) , #name " Link Position In Buffer" }, \
    /* Offset 0x88 (SD0) */ \
    { offset + 0x8,  0x00004, 0xFFFFFFFF, 0xFFFFFFFF, HDA_RD_F_NONE,         hdaRegReadU32 , hdaRegWriteSDCBL  , HDA_REG_IDX_STRM(name, CBL)  , #name " Cyclic Buffer Length" }, \
    /* Offset 0x8C (SD0) -- upper 8 bits are reserved */ \
    { offset + 0xC,  0x00002, 0x0000FFFF, 0x000000FF, HDA_RD_F_NONE,         hdaRegReadU16 , hdaRegWriteSDLVI  , HDA_REG_IDX_STRM(name, LVI)  , #name " Last Valid Index" }, \
    /* Reserved: FIFO Watermark. ** @todo Document this! */ \
    { offset + 0xE,  0x00002, 0x00000007, 0x00000007, HDA_RD_F_NONE,         hdaRegReadU16 , hdaRegWriteSDFIFOW, HDA_REG_IDX_STRM(name, FIFOW), #name " FIFO Watermark" }, \
    /* Offset 0x90 (SD0) */ \
    { offset + 0x10, 0x00002, 0x000000FF, 0x000000FF, HDA_RD_F_NONE,         hdaRegReadU16 , hdaRegWriteSDFIFOS, HDA_REG_IDX_STRM(name, FIFOS), #name " FIFO Size" }, \
    /* Offset 0x92 (SD0) */ \
    { offset + 0x12, 0x00002, 0x00007F7F, 0x00007F7F, HDA_RD_F_NONE,         hdaRegReadU16 , hdaRegWriteSDFMT  , HDA_REG_IDX_STRM(name, FMT)  , #name " Stream Format" }, \
    /* Reserved: 0x94 - 0x98. */ \
    /* Offset 0x98 (SD0) */ \
    { offset + 0x18, 0x00004, 0xFFFFFF80, 0xFFFFFF80, HDA_RD_F_NONE,         hdaRegReadU32 , hdaRegWriteSDBDPL , HDA_REG_IDX_STRM(name, BDPL) , #name " Buffer Descriptor List Pointer-Lower Base Address" }, \
    /* Offset 0x9C (SD0) */ \
    { offset + 0x1C, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, HDA_RD_F_NONE,         hdaRegReadU32 , hdaRegWriteSDBDPU , HDA_REG_IDX_STRM(name, BDPU) , #name " Buffer Descriptor List Pointer-Upper Base Address" }

/** Defines a single audio stream register set (e.g. OSD0). */
#define HDA_REG_MAP_DEF_STREAM(index, name) \
    HDA_REG_MAP_STRM(HDA_REG_DESC_SD0_BASE + (index * 32 /* 0x20 */), name)

/** See 302349 p 6.2. */
const HDAREGDESC g_aHdaRegMap[HDA_NUM_REGS] =
{
    /* offset  size     read mask   write mask  flags             read callback     write callback       index + abbrev              */
    /*-------  -------  ----------  ----------  ----------------- ----------------  -------------------     ------------------------ */
    { 0x00000, 0x00002, 0x0000FFFB, 0x00000000, HDA_RD_F_NONE, hdaRegReadU16   , hdaRegWriteUnimpl  , HDA_REG_IDX(GCAP)         }, /* Global Capabilities */
    { 0x00002, 0x00001, 0x000000FF, 0x00000000, HDA_RD_F_NONE, hdaRegReadU8    , hdaRegWriteUnimpl  , HDA_REG_IDX(VMIN)         }, /* Minor Version */
    { 0x00003, 0x00001, 0x000000FF, 0x00000000, HDA_RD_F_NONE, hdaRegReadU8    , hdaRegWriteUnimpl  , HDA_REG_IDX(VMAJ)         }, /* Major Version */
    { 0x00004, 0x00002, 0x0000FFFF, 0x00000000, HDA_RD_F_NONE, hdaRegReadU16   , hdaRegWriteU16     , HDA_REG_IDX(OUTPAY)       }, /* Output Payload Capabilities */
    { 0x00006, 0x00002, 0x0000FFFF, 0x00000000, HDA_RD_F_NONE, hdaRegReadU16   , hdaRegWriteUnimpl  , HDA_REG_IDX(INPAY)        }, /* Input Payload Capabilities */
    { 0x00008, 0x00004, 0x00000103, 0x00000103, HDA_RD_F_NONE, hdaRegReadU32   , hdaRegWriteGCTL    , HDA_REG_IDX(GCTL)         }, /* Global Control */
    { 0x0000c, 0x00002, 0x00007FFF, 0x00007FFF, HDA_RD_F_NONE, hdaRegReadU16   , hdaRegWriteU16     , HDA_REG_IDX(WAKEEN)       }, /* Wake Enable */
    { 0x0000e, 0x00002, 0x00000007, 0x00000007, HDA_RD_F_NONE, hdaRegReadU8    , hdaRegWriteSTATESTS, HDA_REG_IDX(STATESTS)     }, /* State Change Status */
    { 0x00010, 0x00002, 0xFFFFFFFF, 0x00000000, HDA_RD_F_NONE, hdaRegReadUnimpl, hdaRegWriteUnimpl  , HDA_REG_IDX(GSTS)         }, /* Global Status */
    { 0x00018, 0x00002, 0x0000FFFF, 0x00000000, HDA_RD_F_NONE, hdaRegReadU16   , hdaRegWriteU16     , HDA_REG_IDX(OUTSTRMPAY)   }, /* Output Stream Payload Capability */
    { 0x0001A, 0x00002, 0x0000FFFF, 0x00000000, HDA_RD_F_NONE, hdaRegReadU16   , hdaRegWriteUnimpl  , HDA_REG_IDX(INSTRMPAY)    }, /* Input Stream Payload Capability */
    { 0x00020, 0x00004, 0xC00000FF, 0xC00000FF, HDA_RD_F_NONE, hdaRegReadU32   , hdaRegWriteU32     , HDA_REG_IDX(INTCTL)       }, /* Interrupt Control */
    { 0x00024, 0x00004, 0xC00000FF, 0x00000000, HDA_RD_F_NONE, hdaRegReadU32   , hdaRegWriteUnimpl  , HDA_REG_IDX(INTSTS)       }, /* Interrupt Status */
    { 0x00030, 0x00004, 0xFFFFFFFF, 0x00000000, HDA_RD_F_NONE, hdaRegReadWALCLK, hdaRegWriteUnimpl  , HDA_REG_IDX_NOMEM(WALCLK) }, /* Wall Clock Counter */
    { 0x00034, 0x00004, 0x000000FF, 0x000000FF, HDA_RD_F_NONE, hdaRegReadU32   , hdaRegWriteU32     , HDA_REG_IDX(SSYNC)        }, /* Stream Synchronization */
    { 0x00040, 0x00004, 0xFFFFFF80, 0xFFFFFF80, HDA_RD_F_NONE, hdaRegReadU32   , hdaRegWriteBase    , HDA_REG_IDX(CORBLBASE)    }, /* CORB Lower Base Address */
    { 0x00044, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, HDA_RD_F_NONE, hdaRegReadU32   , hdaRegWriteBase    , HDA_REG_IDX(CORBUBASE)    }, /* CORB Upper Base Address */
    { 0x00048, 0x00002, 0x000000FF, 0x000000FF, HDA_RD_F_NONE, hdaRegReadU16   , hdaRegWriteCORBWP  , HDA_REG_IDX(CORBWP)       }, /* CORB Write Pointer */
    { 0x0004A, 0x00002, 0x000080FF, 0x00008000, HDA_RD_F_NONE, hdaRegReadU16   , hdaRegWriteCORBRP  , HDA_REG_IDX(CORBRP)       }, /* CORB Read Pointer */
    { 0x0004C, 0x00001, 0x00000003, 0x00000003, HDA_RD_F_NONE, hdaRegReadU8    , hdaRegWriteCORBCTL , HDA_REG_IDX(CORBCTL)      }, /* CORB Control */
    { 0x0004D, 0x00001, 0x00000001, 0x00000001, HDA_RD_F_NONE, hdaRegReadU8    , hdaRegWriteCORBSTS , HDA_REG_IDX(CORBSTS)      }, /* CORB Status */
    { 0x0004E, 0x00001, 0x000000F3, 0x00000003, HDA_RD_F_NONE, hdaRegReadU8    , hdaRegWriteCORBSIZE, HDA_REG_IDX(CORBSIZE)     }, /* CORB Size */
    { 0x00050, 0x00004, 0xFFFFFF80, 0xFFFFFF80, HDA_RD_F_NONE, hdaRegReadU32   , hdaRegWriteBase    , HDA_REG_IDX(RIRBLBASE)    }, /* RIRB Lower Base Address */
    { 0x00054, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, HDA_RD_F_NONE, hdaRegReadU32   , hdaRegWriteBase    , HDA_REG_IDX(RIRBUBASE)    }, /* RIRB Upper Base Address */
    { 0x00058, 0x00002, 0x000000FF, 0x00008000, HDA_RD_F_NONE, hdaRegReadU8    , hdaRegWriteRIRBWP  , HDA_REG_IDX(RIRBWP)       }, /* RIRB Write Pointer */
    { 0x0005A, 0x00002, 0x000000FF, 0x000000FF, HDA_RD_F_NONE, hdaRegReadU16   , hdaRegWriteRINTCNT , HDA_REG_IDX(RINTCNT)      }, /* Response Interrupt Count */
    { 0x0005C, 0x00001, 0x00000007, 0x00000007, HDA_RD_F_NONE, hdaRegReadU8    , hdaRegWriteU8      , HDA_REG_IDX(RIRBCTL)      }, /* RIRB Control */
    { 0x0005D, 0x00001, 0x00000005, 0x00000005, HDA_RD_F_NONE, hdaRegReadU8    , hdaRegWriteRIRBSTS , HDA_REG_IDX(RIRBSTS)      }, /* RIRB Status */
    { 0x0005E, 0x00001, 0x000000F3, 0x00000000, HDA_RD_F_NONE, hdaRegReadU8    , hdaRegWriteUnimpl  , HDA_REG_IDX(RIRBSIZE)     }, /* RIRB Size */
    { 0x00060, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, HDA_RD_F_NONE, hdaRegReadU32   , hdaRegWriteU32     , HDA_REG_IDX(IC)           }, /* Immediate Command */
    { 0x00064, 0x00004, 0x00000000, 0xFFFFFFFF, HDA_RD_F_NONE, hdaRegReadU32   , hdaRegWriteUnimpl  , HDA_REG_IDX(IR)           }, /* Immediate Response */
    { 0x00068, 0x00002, 0x00000002, 0x00000002, HDA_RD_F_NONE, hdaRegReadIRS   , hdaRegWriteIRS     , HDA_REG_IDX(IRS)          }, /* Immediate Command Status */
    { 0x00070, 0x00004, 0xFFFFFFFF, 0xFFFFFF81, HDA_RD_F_NONE, hdaRegReadU32   , hdaRegWriteBase    , HDA_REG_IDX(DPLBASE)      }, /* DMA Position Lower Base */
    { 0x00074, 0x00004, 0xFFFFFFFF, 0xFFFFFFFF, HDA_RD_F_NONE, hdaRegReadU32   , hdaRegWriteBase    , HDA_REG_IDX(DPUBASE)      }, /* DMA Position Upper Base */
    /* 4 Serial Data In (SDI). */
    HDA_REG_MAP_DEF_STREAM(0, SD0),
    HDA_REG_MAP_DEF_STREAM(1, SD1),
    HDA_REG_MAP_DEF_STREAM(2, SD2),
    HDA_REG_MAP_DEF_STREAM(3, SD3),
    /* 4 Serial Data Out (SDO). */
    HDA_REG_MAP_DEF_STREAM(4, SD4),
    HDA_REG_MAP_DEF_STREAM(5, SD5),
    HDA_REG_MAP_DEF_STREAM(6, SD6),
    HDA_REG_MAP_DEF_STREAM(7, SD7)
};

const HDAREGALIAS g_aHdaRegAliases[] =
{
    { 0x2084, HDA_REG_SD0LPIB },
    { 0x20a4, HDA_REG_SD1LPIB },
    { 0x20c4, HDA_REG_SD2LPIB },
    { 0x20e4, HDA_REG_SD3LPIB },
    { 0x2104, HDA_REG_SD4LPIB },
    { 0x2124, HDA_REG_SD5LPIB },
    { 0x2144, HDA_REG_SD6LPIB },
    { 0x2164, HDA_REG_SD7LPIB }
};

#ifdef IN_RING3

/** HDABDLEDESC field descriptors for the v7 saved state. */
static SSMFIELD const g_aSSMBDLEDescFields7[] =
{
    SSMFIELD_ENTRY(HDABDLEDESC, u64BufAddr),
    SSMFIELD_ENTRY(HDABDLEDESC, u32BufSize),
    SSMFIELD_ENTRY(HDABDLEDESC, fFlags),
    SSMFIELD_ENTRY_TERM()
};

/** HDABDLEDESC field descriptors for the v6 saved states. */
static SSMFIELD const g_aSSMBDLEDescFields6[] =
{
    SSMFIELD_ENTRY(HDABDLEDESC, u64BufAddr),
    SSMFIELD_ENTRY(HDABDLEDESC, u32BufSize),
    SSMFIELD_ENTRY_CALLBACK(HDABDLEDESC, fFlags, hdaR3GetPutTrans_HDABDLEDESC_fFlags_6),
    SSMFIELD_ENTRY_TERM()
};

/** HDABDLESTATE field descriptors for the v6+ saved state. */
static SSMFIELD const g_aSSMBDLEStateFields6[] =
{
    SSMFIELD_ENTRY(HDABDLESTATE, u32BDLIndex),
    SSMFIELD_ENTRY(HDABDLESTATE, cbBelowFIFOW),
    SSMFIELD_ENTRY_OLD(FIFO,     HDA_FIFO_MAX), /* Deprecated; now is handled in the stream's circular buffer. */
    SSMFIELD_ENTRY(HDABDLESTATE, u32BufOff),
    SSMFIELD_ENTRY_TERM()
};

/** HDABDLESTATE field descriptors for the v7 saved state. */
static SSMFIELD const g_aSSMBDLEStateFields7[] =
{
    SSMFIELD_ENTRY(HDABDLESTATE, u32BDLIndex),
    SSMFIELD_ENTRY(HDABDLESTATE, cbBelowFIFOW),
    SSMFIELD_ENTRY(HDABDLESTATE, u32BufOff),
    SSMFIELD_ENTRY_TERM()
};

/** HDASTREAMSTATE field descriptors for the v6 saved state. */
static SSMFIELD const g_aSSMStreamStateFields6[] =
{
    SSMFIELD_ENTRY_OLD(cBDLE,      sizeof(uint16_t)), /* Deprecated. */
    SSMFIELD_ENTRY(HDASTREAMSTATE, uCurBDLE),
    SSMFIELD_ENTRY_OLD(fStop,      1),                /* Deprecated; see SSMR3PutBool(). */
    SSMFIELD_ENTRY_OLD(fRunning,   1),                /* Deprecated; using the HDA_SDCTL_RUN bit is sufficient. */
    SSMFIELD_ENTRY(HDASTREAMSTATE, fInReset),
    SSMFIELD_ENTRY_TERM()
};

/** HDASTREAMSTATE field descriptors for the v7 saved state. */
static SSMFIELD const g_aSSMStreamStateFields7[] =
{
    SSMFIELD_ENTRY(HDASTREAMSTATE, uCurBDLE),
    SSMFIELD_ENTRY(HDASTREAMSTATE, fInReset),
    SSMFIELD_ENTRY(HDASTREAMSTATE, tsTransferNext),
    SSMFIELD_ENTRY_TERM()
};

/** HDASTREAMPERIOD field descriptors for the v7 saved state. */
static SSMFIELD const g_aSSMStreamPeriodFields7[] =
{
    SSMFIELD_ENTRY(HDASTREAMPERIOD, u64StartWalClk),
    SSMFIELD_ENTRY(HDASTREAMPERIOD, u64ElapsedWalClk),
    SSMFIELD_ENTRY(HDASTREAMPERIOD, cFramesTransferred),
    SSMFIELD_ENTRY(HDASTREAMPERIOD, cIntPending),
    SSMFIELD_ENTRY_TERM()
};

/** HDABDLE field descriptors for the v1 thru v4 saved states. */
static SSMFIELD const g_aSSMStreamBdleFields1234[] =
{
    SSMFIELD_ENTRY(HDABDLE,             Desc.u64BufAddr),       /* u64BdleCviAddr */
    SSMFIELD_ENTRY_OLD(u32BdleMaxCvi,   sizeof(uint32_t)),      /* u32BdleMaxCvi */
    SSMFIELD_ENTRY(HDABDLE,             State.u32BDLIndex),     /* u32BdleCvi */
    SSMFIELD_ENTRY(HDABDLE,             Desc.u32BufSize),       /* u32BdleCviLen */
    SSMFIELD_ENTRY(HDABDLE,             State.u32BufOff),       /* u32BdleCviPos */
    SSMFIELD_ENTRY_CALLBACK(HDABDLE,    Desc.fFlags, hdaR3GetPutTrans_HDABDLE_Desc_fFlags_1thru4), /* fBdleCviIoc */
    SSMFIELD_ENTRY(HDABDLE,             State.cbBelowFIFOW),    /* cbUnderFifoW */
    SSMFIELD_ENTRY_OLD(au8FIFO,         256),                   /* au8FIFO */
    SSMFIELD_ENTRY_TERM()
};

#endif /* IN_RING3 */

/**
 * 32-bit size indexed masks, i.e. g_afMasks[2 bytes] = 0xffff.
 */
static uint32_t const g_afMasks[5] =
{
    UINT32_C(0), UINT32_C(0x000000ff), UINT32_C(0x0000ffff), UINT32_C(0x00ffffff), UINT32_C(0xffffffff)
};




/**
 * Retrieves the number of bytes of a FIFOW register.
 *
 * @return Number of bytes of a given FIFOW register.
 */
DECLINLINE(uint8_t) hdaSDFIFOWToBytes(uint32_t u32RegFIFOW)
{
    uint32_t cb;
    switch (u32RegFIFOW)
    {
        case HDA_SDFIFOW_8B:  cb = 8;  break;
        case HDA_SDFIFOW_16B: cb = 16; break;
        case HDA_SDFIFOW_32B: cb = 32; break;
        default:              cb = 0;  break;
    }

    Assert(RT_IS_POWER_OF_TWO(cb));
    return cb;
}

#ifdef IN_RING3
/**
 * Reschedules pending interrupts for all audio streams which have complete
 * audio periods but did not have the chance to issue their (pending) interrupts yet.
 *
 * @param   pDevIns             The device instance.
 * @param   pThis               The shared HDA device state.
 * @param   pThisCC             The ring-3 HDA device state.
 */
static void hdaR3ReschedulePendingInterrupts(PPDMDEVINS pDevIns, PHDASTATE pThis, PHDASTATER3 pThisCC)
{
    bool fInterrupt = false;

    for (uint8_t i = 0; i < HDA_MAX_STREAMS; ++i)
    {
        PHDASTREAM pStream = &pThis->aStreams[i];
        if (   hdaR3StreamPeriodIsComplete(    &pStream->State.Period)
            && hdaR3StreamPeriodNeedsInterrupt(&pStream->State.Period)
            && hdaR3WalClkSet(pThis, pThisCC, hdaR3StreamPeriodGetAbsElapsedWalClk(&pStream->State.Period), false /* fForce */))
        {
            fInterrupt = true;
            break;
        }
    }

    LogFunc(("fInterrupt=%RTbool\n", fInterrupt));

    HDA_PROCESS_INTERRUPT(pDevIns, pThis);
}
#endif /* IN_RING3 */

/**
 * Looks up a register at the exact offset given by @a offReg.
 *
 * @returns Register index on success, -1 if not found.
 * @param   offReg              The register offset.
 */
static int hdaRegLookup(uint32_t offReg)
{
    /*
     * Aliases.
     */
    if (offReg >= g_aHdaRegAliases[0].offReg)
    {
        for (unsigned i = 0; i < RT_ELEMENTS(g_aHdaRegAliases); i++)
            if (offReg == g_aHdaRegAliases[i].offReg)
                return g_aHdaRegAliases[i].idxAlias;
        Assert(g_aHdaRegMap[RT_ELEMENTS(g_aHdaRegMap) - 1].offset < offReg);
        return -1;
    }

    /*
     * Binary search the
     */
    int idxEnd  = RT_ELEMENTS(g_aHdaRegMap);
    int idxLow  = 0;
    for (;;)
    {
        int idxMiddle = idxLow + (idxEnd - idxLow) / 2;
        if (offReg < g_aHdaRegMap[idxMiddle].offset)
        {
            if (idxLow == idxMiddle)
                break;
            idxEnd = idxMiddle;
        }
        else if (offReg > g_aHdaRegMap[idxMiddle].offset)
        {
            idxLow = idxMiddle + 1;
            if (idxLow >= idxEnd)
                break;
        }
        else
            return idxMiddle;
    }

#ifdef RT_STRICT
    for (unsigned i = 0; i < RT_ELEMENTS(g_aHdaRegMap); i++)
        Assert(g_aHdaRegMap[i].offset != offReg);
#endif
    return -1;
}

#ifdef IN_RING3

/**
 * Looks up a register covering the offset given by @a offReg.
 *
 * @returns Register index on success, -1 if not found.
 * @param   offReg              The register offset.
 */
static int hdaR3RegLookupWithin(uint32_t offReg)
{
    /*
     * Aliases.
     */
    if (offReg >= g_aHdaRegAliases[0].offReg)
    {
        for (unsigned i = 0; i < RT_ELEMENTS(g_aHdaRegAliases); i++)
        {
            uint32_t off = offReg - g_aHdaRegAliases[i].offReg;
            if (off < 4 && off < g_aHdaRegMap[g_aHdaRegAliases[i].idxAlias].size)
                return g_aHdaRegAliases[i].idxAlias;
        }
        Assert(g_aHdaRegMap[RT_ELEMENTS(g_aHdaRegMap) - 1].offset < offReg);
        return -1;
    }

    /*
     * Binary search the register map.
     */
    int idxEnd  = RT_ELEMENTS(g_aHdaRegMap);
    int idxLow  = 0;
    for (;;)
    {
        int idxMiddle = idxLow + (idxEnd - idxLow) / 2;
        if (offReg < g_aHdaRegMap[idxMiddle].offset)
        {
            if (idxLow == idxMiddle)
                break;
            idxEnd = idxMiddle;
        }
        else if (offReg >= g_aHdaRegMap[idxMiddle].offset + g_aHdaRegMap[idxMiddle].size)
        {
            idxLow = idxMiddle + 1;
            if (idxLow >= idxEnd)
                break;
        }
        else
            return idxMiddle;
    }

# ifdef RT_STRICT
    for (unsigned i = 0; i < RT_ELEMENTS(g_aHdaRegMap); i++)
        Assert(offReg - g_aHdaRegMap[i].offset >= g_aHdaRegMap[i].size);
# endif
    return -1;
}


/**
 * Synchronizes the CORB / RIRB buffers between internal <-> device state.
 *
 * @returns IPRT status code.
 *
 * @param   pDevIns             The device instance.
 * @param   pThis               The shared HDA device state.
 * @param   fLocal              Specify true to synchronize HDA state's CORB buffer with the device state,
 *                              or false to synchronize the device state's RIRB buffer with the HDA state.
 *
 * @todo r=andy Break this up into two functions?
 */
static int hdaR3CmdSync(PPDMDEVINS pDevIns, PHDASTATE pThis, bool fLocal)
{
    int rc = VINF_SUCCESS;
    if (fLocal)
    {
        if (pThis->u64CORBBase)
        {
            Assert(pThis->cbCorbBuf);

/** @todo r=bird: An explanation is required why PDMDevHlpPhysRead is used with
 *        the CORB and PDMDevHlpPCIPhysWrite with RIRB below.  There are
 *        similar unexplained inconsistencies in DevHDACommon.cpp. */
            rc = PDMDevHlpPhysRead(pDevIns, pThis->u64CORBBase, pThis->au32CorbBuf,
                                   RT_MIN(pThis->cbCorbBuf, sizeof(pThis->au32CorbBuf)));
            Log(("hdaR3CmdSync/CORB: read %RGp LB %#x (%Rrc)\n", pThis->u64CORBBase, pThis->cbCorbBuf, rc));
            AssertRCReturn(rc, rc);
        }
    }
    else
    {
        if (pThis->u64RIRBBase)
        {
            Assert(pThis->cbRirbBuf);

            rc = PDMDevHlpPCIPhysWrite(pDevIns, pThis->u64RIRBBase, pThis->au64RirbBuf,
                                       RT_MIN(pThis->cbRirbBuf, sizeof(pThis->au64RirbBuf)));
            Log(("hdaR3CmdSync/RIRB: phys read %RGp LB %#x (%Rrc)\n", pThis->u64RIRBBase, pThis->cbRirbBuf, rc));
            AssertRCReturn(rc, rc);
        }
    }

# ifdef DEBUG_CMD_BUFFER
        LogFunc(("fLocal=%RTbool\n", fLocal));

        uint8_t i = 0;
        do
        {
            LogFunc(("CORB%02x: ", i));
            uint8_t j = 0;
            do
            {
                const char *pszPrefix;
                if ((i + j) == HDA_REG(pThis, CORBRP))
                    pszPrefix = "[R]";
                else if ((i + j) == HDA_REG(pThis, CORBWP))
                    pszPrefix = "[W]";
                else
                    pszPrefix = "   "; /* three spaces */
                Log((" %s%08x", pszPrefix, pThis->pu32CorbBuf[i + j]));
                j++;
            } while (j < 8);
            Log(("\n"));
            i += 8;
        } while (i != 0);

        do
        {
            LogFunc(("RIRB%02x: ", i));
            uint8_t j = 0;
            do
            {
                const char *prefix;
                if ((i + j) == HDA_REG(pThis, RIRBWP))
                    prefix = "[W]";
                else
                    prefix = "   ";
                Log((" %s%016lx", prefix, pThis->pu64RirbBuf[i + j]));
            } while (++j < 8);
            Log(("\n"));
            i += 8;
        } while (i != 0);
# endif
    return rc;
}

/**
 * Processes the next CORB buffer command in the queue.
 *
 * This will invoke the HDA codec verb dispatcher.
 *
 * @returns VBox status code suitable for MMIO write return.
 * @param   pDevIns             The device instance.
 * @param   pThis               The shared HDA device state.
 * @param   pThisCC             The ring-3 HDA device state.
 */
static int hdaR3CORBCmdProcess(PPDMDEVINS pDevIns, PHDASTATE pThis, PHDASTATER3 pThisCC)
{
    Log3Func(("CORB(RP:%x, WP:%x) RIRBWP:%x\n", HDA_REG(pThis, CORBRP), HDA_REG(pThis, CORBWP), HDA_REG(pThis, RIRBWP)));

    if (!(HDA_REG(pThis, CORBCTL) & HDA_CORBCTL_DMA))
    {
        LogFunc(("CORB DMA not active, skipping\n"));
        return VINF_SUCCESS;
    }

    Assert(pThis->cbCorbBuf);

    int rc = hdaR3CmdSync(pDevIns, pThis, true /* Sync from guest */);
    AssertRCReturn(rc, rc);

    /*
     * Prepare local copies of relevant registers.
     */
    uint16_t cIntCnt = HDA_REG(pThis, RINTCNT) & 0xff;
    if (!cIntCnt) /* 0 means 256 interrupts. */
        cIntCnt = HDA_MAX_RINTCNT;

    uint32_t const cCorbEntries = RT_MIN(RT_MAX(pThis->cbCorbBuf, 1), sizeof(pThis->au32CorbBuf)) / HDA_CORB_ELEMENT_SIZE;
    uint8_t const  corbWp       = HDA_REG(pThis, CORBWP) % cCorbEntries;
    uint8_t        corbRp       = HDA_REG(pThis, CORBRP);
    uint8_t        rirbWp       = HDA_REG(pThis, RIRBWP);

    /*
     * The loop.
     */
    Log3Func(("START CORB(RP:%x, WP:%x) RIRBWP:%x, RINTCNT:%RU8/%RU8\n", corbRp, corbWp, rirbWp, pThis->u16RespIntCnt, cIntCnt));
    while (corbRp != corbWp)
    {
        /* Fetch the command from the CORB. */
        corbRp = (corbRp + 1) /* Advance +1 as the first command(s) are at CORBWP + 1. */ % cCorbEntries;
        uint32_t const uCmd = pThis->au32CorbBuf[corbRp];

        /*
         * Execute the command.
         */
        uint64_t uResp = 0;
        rc = pThisCC->pCodec->pfnLookup(pThisCC->pCodec, HDA_CODEC_CMD(uCmd, 0 /* Codec index */), &uResp);
        if (RT_FAILURE(rc))
            LogFunc(("Codec lookup failed with rc=%Rrc\n", rc));
        Log3Func(("Codec verb %08x -> response %016RX64\n", uCmd, uResp));

        if (   (uResp & CODEC_RESPONSE_UNSOLICITED)
            && !(HDA_REG(pThis, GCTL) & HDA_GCTL_UNSOL))
        {
            LogFunc(("Unexpected unsolicited response.\n"));
            HDA_REG(pThis, CORBRP) = corbRp;
            /** @todo r=andy No RIRB syncing to guest required in that case? */
            /** @todo r=bird: Why isn't RIRBWP updated here.  The response might come
             *        after already processing several commands, can't it?  (When you think
             *        about it, it is bascially the same question as Andy is asking.) */
            return VINF_SUCCESS;
        }

        /*
         * Store the response in the RIRB.
         */
        AssertCompile(HDA_RIRB_SIZE == RT_ELEMENTS(pThis->au64RirbBuf));
        rirbWp = (rirbWp + 1) % HDA_RIRB_SIZE;
        pThis->au64RirbBuf[rirbWp] = uResp;

        /*
         * Send interrupt if needed.
         */
        bool fSendInterrupt = false;
        pThis->u16RespIntCnt++;
        if (pThis->u16RespIntCnt >= cIntCnt) /* Response interrupt count reached? */
        {
            pThis->u16RespIntCnt = 0; /* Reset internal interrupt response counter. */

            Log3Func(("Response interrupt count reached (%RU16)\n", pThis->u16RespIntCnt));
            fSendInterrupt = true;
        }
        else if (corbRp == corbWp) /* Did we reach the end of the current command buffer? */
        {
            Log3Func(("Command buffer empty\n"));
            fSendInterrupt = true;
        }
        if (fSendInterrupt)
        {
            if (HDA_REG(pThis, RIRBCTL) & HDA_RIRBCTL_RINTCTL) /* Response Interrupt Control (RINTCTL) enabled? */
            {
                HDA_REG(pThis, RIRBSTS) |= HDA_RIRBSTS_RINTFL;
                HDA_PROCESS_INTERRUPT(pDevIns, pThis);
            }
        }
    }

    /*
     * Put register locals back.
     */
    Log3Func(("END CORB(RP:%x, WP:%x) RIRBWP:%x, RINTCNT:%RU8/%RU8\n", corbRp, corbWp, rirbWp, pThis->u16RespIntCnt, cIntCnt));
    HDA_REG(pThis, CORBRP) = corbRp;
    HDA_REG(pThis, RIRBWP) = rirbWp;

    /*
     * Write out the response.
     */
    rc = hdaR3CmdSync(pDevIns, pThis, false /* Sync to guest */);
    AssertRC(rc);

    return rc;
}

#endif /* IN_RING3 */

/* Register access handlers. */

static VBOXSTRICTRC hdaRegReadUnimpl(PPDMDEVINS pDevIns, PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value)
{
    RT_NOREF(pDevIns, pThis, iReg);
    *pu32Value = 0;
    return VINF_SUCCESS;
}

static VBOXSTRICTRC hdaRegWriteUnimpl(PPDMDEVINS pDevIns, PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    RT_NOREF(pDevIns, pThis, iReg, u32Value);
    return VINF_SUCCESS;
}

/* U8 */
static VBOXSTRICTRC hdaRegReadU8(PPDMDEVINS pDevIns, PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value)
{
    Assert(((pThis->au32Regs[g_aHdaRegMap[iReg].mem_idx] & g_aHdaRegMap[iReg].readable) & 0xffffff00) == 0);
    return hdaRegReadU32(pDevIns, pThis, iReg, pu32Value);
}

static VBOXSTRICTRC hdaRegWriteU8(PPDMDEVINS pDevIns, PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    Assert((u32Value & 0xffffff00) == 0);
    return hdaRegWriteU32(pDevIns, pThis, iReg, u32Value);
}

/* U16 */
static VBOXSTRICTRC hdaRegReadU16(PPDMDEVINS pDevIns, PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value)
{
    Assert(((pThis->au32Regs[g_aHdaRegMap[iReg].mem_idx] & g_aHdaRegMap[iReg].readable) & 0xffff0000) == 0);
    return hdaRegReadU32(pDevIns, pThis, iReg, pu32Value);
}

static VBOXSTRICTRC hdaRegWriteU16(PPDMDEVINS pDevIns, PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    Assert((u32Value & 0xffff0000) == 0);
    return hdaRegWriteU32(pDevIns, pThis, iReg, u32Value);
}

/* U24 */
static VBOXSTRICTRC hdaRegReadU24(PPDMDEVINS pDevIns, PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value)
{
    Assert(((pThis->au32Regs[g_aHdaRegMap[iReg].mem_idx] & g_aHdaRegMap[iReg].readable) & 0xff000000) == 0);
    return hdaRegReadU32(pDevIns, pThis, iReg, pu32Value);
}

#ifdef IN_RING3
static VBOXSTRICTRC hdaRegWriteU24(PPDMDEVINS pDevIns, PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    Assert((u32Value & 0xff000000) == 0);
    return hdaRegWriteU32(pDevIns, pThis, iReg, u32Value);
}
#endif

/* U32 */
static VBOXSTRICTRC hdaRegReadU32(PPDMDEVINS pDevIns, PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value)
{
    RT_NOREF(pDevIns);

    uint32_t const iRegMem = g_aHdaRegMap[iReg].mem_idx;
    *pu32Value = pThis->au32Regs[iRegMem] & g_aHdaRegMap[iReg].readable;
    return VINF_SUCCESS;
}

static VBOXSTRICTRC hdaRegWriteU32(PPDMDEVINS pDevIns, PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    RT_NOREF(pDevIns);

    uint32_t const iRegMem = g_aHdaRegMap[iReg].mem_idx;
    pThis->au32Regs[iRegMem]  = (u32Value & g_aHdaRegMap[iReg].writable)
                              | (pThis->au32Regs[iRegMem] & ~g_aHdaRegMap[iReg].writable);
    return VINF_SUCCESS;
}

static VBOXSTRICTRC hdaRegWriteGCTL(PPDMDEVINS pDevIns, PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    RT_NOREF(pDevIns, iReg);

    if (u32Value & HDA_GCTL_CRST)
    {
        /* Set the CRST bit to indicate that we're leaving reset mode. */
        HDA_REG(pThis, GCTL) |= HDA_GCTL_CRST;
        LogFunc(("Guest leaving HDA reset\n"));
    }
    else
    {
#ifdef IN_RING3
        /* Enter reset state. */
        LogFunc(("Guest entering HDA reset with DMA(RIRB:%s, CORB:%s)\n",
                 HDA_REG(pThis, CORBCTL) & HDA_CORBCTL_DMA ? "on" : "off",
                 HDA_REG(pThis, RIRBCTL) & HDA_RIRBCTL_RDMAEN ? "on" : "off"));

        /* Clear the CRST bit to indicate that we're in reset state. */
        HDA_REG(pThis, GCTL) &= ~HDA_GCTL_CRST;

        hdaR3GCTLReset(pDevIns, pThis, PDMDEVINS_2_DATA_CC(pDevIns,  PHDASTATER3));
#else
        return VINF_IOM_R3_MMIO_WRITE;
#endif
    }

    if (u32Value & HDA_GCTL_FCNTRL)
    {
        /* Flush: GSTS:1 set, see 6.2.6. */
        HDA_REG(pThis, GSTS) |= HDA_GSTS_FSTS;  /* Set the flush status. */
        /* DPLBASE and DPUBASE should be initialized with initial value (see 6.2.6). */
    }

    return VINF_SUCCESS;
}

static VBOXSTRICTRC hdaRegWriteSTATESTS(PPDMDEVINS pDevIns, PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    RT_NOREF(pDevIns);

    uint32_t v  = HDA_REG_IND(pThis, iReg);
    uint32_t nv = u32Value & HDA_STATESTS_SCSF_MASK;

    HDA_REG(pThis, STATESTS) &= ~(v & nv); /* Write of 1 clears corresponding bit. */

    return VINF_SUCCESS;
}

static VBOXSTRICTRC hdaRegReadLPIB(PPDMDEVINS pDevIns, PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value)
{
    RT_NOREF(pDevIns);

    const uint8_t  uSD     = HDA_SD_NUM_FROM_REG(pThis, LPIB, iReg);
    const uint32_t u32LPIB = HDA_STREAM_REG(pThis, LPIB, uSD);
    *pu32Value = u32LPIB;
    LogFlowFunc(("[SD%RU8] LPIB=%RU32, CBL=%RU32\n", uSD, u32LPIB, HDA_STREAM_REG(pThis, CBL, uSD)));

    return VINF_SUCCESS;
}

#ifdef IN_RING3
/**
 * Returns the current maximum value the wall clock counter can be set to.
 *
 * This maximum value depends on all currently handled HDA streams and their own current timing.
 *
 * @return  Current maximum value the wall clock counter can be set to.
 * @param   pThis               The shared HDA device state.
 * @param   pThisCC             The ring-3 HDA device state.
 *
 * @remark  Does not actually set the wall clock counter.
 *
 * @todo r=bird: This function is in the wrong file.
 */
static uint64_t hdaR3WalClkGetMax(PHDASTATE pThis, PHDASTATER3 pThisCC)
{
    const uint64_t u64WalClkCur       = ASMAtomicReadU64(&pThis->u64WalClk);
    const uint64_t u64FrontAbsWalClk  = pThisCC->SinkFront.pStreamShared
                                      ? hdaR3StreamPeriodGetAbsElapsedWalClk(&pThisCC->SinkFront.pStreamShared->State.Period)  : 0;
# ifdef VBOX_WITH_AUDIO_HDA_51_SURROUND
#  error "Implement me!"
# endif
    const uint64_t u64LineInAbsWalClk = pThisCC->SinkLineIn.pStreamShared
                                      ? hdaR3StreamPeriodGetAbsElapsedWalClk(&pThisCC->SinkLineIn.pStreamShared->State.Period) : 0;
# ifdef VBOX_WITH_HDA_MIC_IN
    const uint64_t u64MicInAbsWalClk  = pThisCC->SinkMicIn.pStreamShared
                                      ? hdaR3StreamPeriodGetAbsElapsedWalClk(&pThisCC->SinkMicIn.pStreamShared->State.Period)  : 0;
# endif

    uint64_t u64WalClkNew = RT_MAX(u64WalClkCur, u64FrontAbsWalClk);
# ifdef VBOX_WITH_AUDIO_HDA_51_SURROUND
#  error "Implement me!"
# endif
    u64WalClkNew          = RT_MAX(u64WalClkNew, u64LineInAbsWalClk);
# ifdef VBOX_WITH_HDA_MIC_IN
    u64WalClkNew          = RT_MAX(u64WalClkNew, u64MicInAbsWalClk);
# endif

    Log3Func(("%RU64 -> Front=%RU64, LineIn=%RU64 -> %RU64\n",
              u64WalClkCur, u64FrontAbsWalClk, u64LineInAbsWalClk, u64WalClkNew));

    return u64WalClkNew;
}
#endif /* IN_RING3 */

static VBOXSTRICTRC hdaRegReadWALCLK(PPDMDEVINS pDevIns, PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value)
{
#ifdef IN_RING3 /** @todo r=bird: No reason (except logging) for this to be ring-3 only! */
    RT_NOREF(pDevIns, iReg);

    const uint64_t u64WalClkCur = ASMAtomicReadU64(&pThis->u64WalClk);
    *pu32Value = RT_LO_U32(u64WalClkCur);

    Log3Func(("%RU32 (max @ %RU64)\n", *pu32Value, hdaR3WalClkGetMax(pThis, PDMDEVINS_2_DATA_CC(pDevIns, PHDASTATER3))));
    return VINF_SUCCESS;
#else
    RT_NOREF(pDevIns, pThis, iReg, pu32Value);
    return VINF_IOM_R3_MMIO_READ;
#endif
}

static VBOXSTRICTRC hdaRegWriteCORBRP(PPDMDEVINS pDevIns, PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    RT_NOREF(pDevIns, iReg);
    if (u32Value & HDA_CORBRP_RST)
    {
        /* Do a CORB reset. */
        if (pThis->cbCorbBuf)
        {
            RT_ZERO(pThis->au32CorbBuf);
            return VINF_IOM_R3_MMIO_WRITE;
        }

        LogRel2(("HDA: CORB reset\n"));

        HDA_REG(pThis, CORBRP) = HDA_CORBRP_RST;    /* Clears the pointer. */
    }
    else
        HDA_REG(pThis, CORBRP) &= ~HDA_CORBRP_RST;  /* Only CORBRP_RST bit is writable. */

    return VINF_SUCCESS;
}

static VBOXSTRICTRC hdaRegWriteCORBCTL(PPDMDEVINS pDevIns, PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
#ifdef IN_RING3
    VBOXSTRICTRC rc = hdaRegWriteU8(pDevIns, pThis, iReg, u32Value);
    AssertRCSuccess(VBOXSTRICTRC_VAL(rc));

    if (HDA_REG(pThis, CORBCTL) & HDA_CORBCTL_DMA) /* Start DMA engine. */
        rc = hdaR3CORBCmdProcess(pDevIns, pThis, PDMDEVINS_2_DATA_CC(pDevIns, PHDASTATER3));
    else
        LogFunc(("CORB DMA not running, skipping\n"));

    return rc;
#else
    RT_NOREF(pDevIns, pThis, iReg, u32Value);
    return VINF_IOM_R3_MMIO_WRITE;
#endif
}

static VBOXSTRICTRC hdaRegWriteCORBSIZE(PPDMDEVINS pDevIns, PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    RT_NOREF(pDevIns, iReg);

    if (!(HDA_REG(pThis, CORBCTL) & HDA_CORBCTL_DMA)) /* Ignore request if CORB DMA engine is (still) running. */
    {
        u32Value = (u32Value & HDA_CORBSIZE_SZ);

        uint16_t cEntries;
        switch (u32Value)
        {
            case 0: /* 8 byte; 2 entries. */
                cEntries = 2;
                break;
            case 1: /* 64 byte; 16 entries. */
                cEntries = 16;
                break;
            case 2: /* 1 KB; 256 entries. */
                cEntries = HDA_CORB_SIZE; /* default. */
                break;
            default:
                LogRel(("HDA: Guest tried to set an invalid CORB size (0x%x), keeping default\n", u32Value));
                u32Value = 2;
                cEntries = HDA_CORB_SIZE; /* Use default size. */
                break;
        }

        uint32_t cbCorbBuf = cEntries * HDA_CORB_ELEMENT_SIZE;
        Assert(cbCorbBuf <= sizeof(pThis->au32CorbBuf)); /* paranoia */

        if (cbCorbBuf != pThis->cbCorbBuf)
        {
            RT_ZERO(pThis->au32CorbBuf); /* Clear CORB when setting a new size. */
            pThis->cbCorbBuf = cbCorbBuf;
        }

        LogFunc(("CORB buffer size is now %RU32 bytes (%u entries)\n", pThis->cbCorbBuf, pThis->cbCorbBuf / HDA_CORB_ELEMENT_SIZE));

        HDA_REG(pThis, CORBSIZE) = u32Value;
    }
    else
        LogFunc(("CORB DMA is (still) running, skipping\n"));
    return VINF_SUCCESS;
}

static VBOXSTRICTRC hdaRegWriteCORBSTS(PPDMDEVINS pDevIns, PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    RT_NOREF(pDevIns, iReg);

    uint32_t v = HDA_REG(pThis, CORBSTS);
    HDA_REG(pThis, CORBSTS) &= ~(v & u32Value);

    return VINF_SUCCESS;
}

static VBOXSTRICTRC hdaRegWriteCORBWP(PPDMDEVINS pDevIns, PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
#ifdef IN_RING3
    VBOXSTRICTRC rc = hdaRegWriteU16(pDevIns, pThis, iReg, u32Value);
    AssertRCSuccess(VBOXSTRICTRC_VAL(rc));

    return hdaR3CORBCmdProcess(pDevIns, pThis, PDMDEVINS_2_DATA_CC(pDevIns, PHDASTATER3));
#else
    RT_NOREF(pDevIns, pThis, iReg, u32Value);
    return VINF_IOM_R3_MMIO_WRITE;
#endif
}

static VBOXSTRICTRC hdaRegWriteSDCBL(PPDMDEVINS pDevIns, PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    return hdaRegWriteU32(pDevIns, pThis, iReg, u32Value);
}

static VBOXSTRICTRC hdaRegWriteSDCTL(PPDMDEVINS pDevIns, PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
#ifdef IN_RING3
    /* Get the stream descriptor number. */
    const uint8_t uSD = HDA_SD_NUM_FROM_REG(pThis, CTL, iReg);
    AssertReturn(uSD < RT_ELEMENTS(pThis->aStreams), VERR_INTERNAL_ERROR_3); /* paranoia^2: Bad g_aHdaRegMap. */

    /*
     * Extract the stream tag the guest wants to use for this specific
     * stream descriptor (SDn). This only can happen if the stream is in a non-running
     * state, so we're doing the lookup and assignment here.
     *
     * So depending on the guest OS, SD3 can use stream tag 4, for example.
     */
    PHDASTATER3 pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PHDASTATER3);
    uint8_t     uTag    = (u32Value >> HDA_SDCTL_NUM_SHIFT) & HDA_SDCTL_NUM_MASK;
    ASSERT_GUEST_MSG_RETURN(uTag < RT_ELEMENTS(pThisCC->aTags),
                            ("SD%RU8: Invalid stream tag %RU8 (u32Value=%#x)!\n", uSD, uTag, u32Value),
                            VINF_SUCCESS /* Always return success to the MMIO handler. */);

    PHDASTREAM   const pStreamShared = &pThis->aStreams[uSD];
    PHDASTREAMR3 const pStreamR3     = &pThisCC->aStreams[uSD];

    const bool fRun      = RT_BOOL(u32Value & HDA_SDCTL_RUN);
    const bool fReset    = RT_BOOL(u32Value & HDA_SDCTL_SRST);

    /* If the run bit is set, we take the virtual-sync clock lock as well so we
       can safely update timers via hdaR3TimerSet if necessary.   We need to be
       very careful with the fInReset and fInRun indicators here, as they may
       change during the relocking if we need to acquire the clock lock. */
    const bool fNeedVirtualSyncClockLock = (u32Value & (HDA_SDCTL_RUN | HDA_SDCTL_SRST)) == HDA_SDCTL_RUN
                                        && (HDA_REG_IND(pThis, iReg) & HDA_SDCTL_RUN) == 0;
    if (fNeedVirtualSyncClockLock)
    {
        DEVHDA_UNLOCK(pDevIns, pThis);
        DEVHDA_LOCK_BOTH_RETURN(pDevIns, pThis, pStreamShared, VINF_IOM_R3_MMIO_WRITE);
    }

    const bool fInRun    = RT_BOOL(HDA_REG_IND(pThis, iReg) & HDA_SDCTL_RUN);
    const bool fInReset  = RT_BOOL(HDA_REG_IND(pThis, iReg) & HDA_SDCTL_SRST);

    /*LogFunc(("[SD%RU8] fRun=%RTbool, fInRun=%RTbool, fReset=%RTbool, fInReset=%RTbool, %R[sdctl]\n",
               uSD, fRun, fInRun, fReset, fInReset, u32Value));*/
    if (fInReset)
    {
        Assert(!fReset);
        Assert(!fInRun && !fRun);

        /* Exit reset state. */
        ASMAtomicXchgBool(&pStreamShared->State.fInReset, false);

        /* Report that we're done resetting this stream by clearing SRST. */
        HDA_STREAM_REG(pThis, CTL, uSD) &= ~HDA_SDCTL_SRST;

        LogFunc(("[SD%RU8] Reset exit\n", uSD));
    }
    else if (fReset)
    {
        /* ICH6 datasheet 18.2.33 says that RUN bit should be cleared before initiation of reset. */
        Assert(!fInRun && !fRun);

        LogFunc(("[SD%RU8] Reset enter\n", uSD));

        hdaR3StreamLock(pStreamR3);

# ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
        hdaR3StreamAsyncIOLock(pStreamR3);
# endif
        /* Make sure to remove the run bit before doing the actual stream reset. */
        HDA_STREAM_REG(pThis, CTL, uSD) &= ~HDA_SDCTL_RUN;

        hdaR3StreamReset(pThis, pThisCC, pStreamShared, pStreamR3, uSD);

# ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
        hdaR3StreamAsyncIOUnlock(pStreamR3);
# endif
        hdaR3StreamUnlock(pStreamR3);
    }
    else
    {
        /*
         * We enter here to change DMA states only.
         */
        if (fInRun != fRun)
        {
            Assert(!fReset && !fInReset);
            LogFunc(("[SD%RU8] State changed (fRun=%RTbool)\n", uSD, fRun));

            hdaR3StreamLock(pStreamR3);

            int rc2 = VINF_SUCCESS;

# ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
            if (fRun)
                rc2 = hdaR3StreamAsyncIOCreate(pStreamR3);

            hdaR3StreamAsyncIOLock(pStreamR3);
# endif
            if (fRun)
            {
                if (hdaGetDirFromSD(uSD) == PDMAUDIODIR_OUT)
                {
                    const uint8_t uStripeCtl = ((u32Value >> HDA_SDCTL_STRIPE_SHIFT) & HDA_SDCTL_STRIPE_MASK) + 1;
                    LogFunc(("[SD%RU8] Using %RU8 SDOs (stripe control)\n", uSD, uStripeCtl));
                    if (uStripeCtl > 1)
                        LogRel2(("HDA: Warning: Striping output over more than one SDO for stream #%RU8 currently is not implemented " \
                                 "(%RU8 SDOs requested)\n", uSD, uStripeCtl));
                }

                /* Assign new values. */
                LogFunc(("[SD%RU8] Using stream tag=%RU8\n", uSD, uTag));
                PHDATAG pTag = &pThisCC->aTags[uTag];
                pTag->uTag      = uTag;
                pTag->pStreamR3 = &pThisCC->aStreams[uSD];

# ifdef LOG_ENABLED
                PDMAUDIOPCMPROPS Props;
                rc2 = hdaR3SDFMTToPCMProps(HDA_STREAM_REG(pThis, FMT, uSD), &Props);
                AssertRC(rc2);
                LogFunc(("[SD%RU8] %RU32Hz, %RU8bit, %RU8 channel(s)\n",
                         uSD, Props.uHz, Props.cbSample * 8 /* Bit */, Props.cChannels));
# endif
                /* (Re-)initialize the stream with current values. */
                rc2 = hdaR3StreamSetUp(pDevIns, pThis, pStreamShared, pStreamR3, uSD);
                if (   RT_SUCCESS(rc2)
                    /* Any vital stream change occurred so that we need to (re-)add the stream to our setup?
                     * Otherwise just skip this, as this costs a lot of performance. */
                    && rc2 != VINF_NO_CHANGE)
                {
                    /* Remove the old stream from the device setup. */
                    rc2 = hdaR3RemoveStream(pThisCC, &pStreamShared->State.Cfg);
                    AssertRC(rc2);

                    /* Add the stream to the device setup. */
                    rc2 = hdaR3AddStream(pThisCC, &pStreamShared->State.Cfg);
                    AssertRC(rc2);
                }
            }

            if (RT_SUCCESS(rc2))
            {
                /* Enable/disable the stream. */
                rc2 = hdaR3StreamEnable(pStreamShared, pStreamR3, fRun /* fEnable */);
                AssertRC(rc2);

                if (fRun)
                {
                    /* Keep track of running streams. */
                    pThisCC->cStreamsActive++;

                    /* (Re-)init the stream's period. */
                    hdaR3StreamPeriodInit(&pStreamShared->State.Period, uSD, pStreamShared->u16LVI,
                                          pStreamShared->u32CBL, &pStreamShared->State.Cfg);

                    /* Begin a new period for this stream. */
                    rc2 = hdaR3StreamPeriodBegin(&pStreamShared->State.Period,
                                                 hdaWalClkGetCurrent(pThis) /* Use current wall clock time */);
                    AssertRC(rc2);

                    uint64_t const tsNow = PDMDevHlpTimerGet(pDevIns, pStreamShared->hTimer);
                    rc2 = hdaR3TimerSet(pDevIns, pStreamShared, tsNow + pStreamShared->State.cTransferTicks, false /* fForce */, tsNow);
                    AssertRC(rc2);
                }
                else
                {
                    /* Keep track of running streams. */
                    Assert(pThisCC->cStreamsActive);
                    if (pThisCC->cStreamsActive)
                        pThisCC->cStreamsActive--;

                    /* Make sure to (re-)schedule outstanding (delayed) interrupts. */
                    hdaR3ReschedulePendingInterrupts(pDevIns, pThis, pThisCC);

                    /* Reset the period. */
                    hdaR3StreamPeriodReset(&pStreamShared->State.Period);
                }
            }

# ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
            hdaR3StreamAsyncIOUnlock(pStreamR3);
# endif
            /* Make sure to leave the lock before (eventually) starting the timer. */
            hdaR3StreamUnlock(pStreamR3);
        }
    }

    if (fNeedVirtualSyncClockLock)
        PDMDevHlpTimerUnlockClock(pDevIns, pStreamShared->hTimer); /* Caller will unlock pThis->CritSect. */

    return hdaRegWriteU24(pDevIns, pThis, iReg, u32Value);
#else  /* !IN_RING3 */
    RT_NOREF(pDevIns, pThis, iReg, u32Value);
    return VINF_IOM_R3_MMIO_WRITE;
#endif /* !IN_RING3 */
}

static VBOXSTRICTRC hdaRegWriteSDSTS(PPDMDEVINS pDevIns, PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
#ifdef IN_RING3
    const uint8_t uSD = HDA_SD_NUM_FROM_REG(pThis, STS, iReg);
    AssertReturn(uSD < RT_ELEMENTS(pThis->aStreams), VERR_INTERNAL_ERROR_3); /* paranoia^2: Bad g_aHdaRegMap. */
    PHDASTATER3 const  pThisCC       = PDMDEVINS_2_DATA_CC(pDevIns, PHDASTATER3);
    PHDASTREAMR3 const pStreamR3     = &pThisCC->aStreams[uSD];
    PHDASTREAM const   pStreamShared = &pThis->aStreams[uSD];

    /* We only need to take the virtual-sync lock if we want to call
       PDMDevHlpTimerGet or hdaR3TimerSet later.  Only precondition for that
       is that we've got a non-zero ticks-per-transfer value. */
    uint64_t const cTransferTicks = pStreamShared->State.cTransferTicks;
    if (cTransferTicks)
    {
        DEVHDA_UNLOCK(pDevIns, pThis);
        DEVHDA_LOCK_BOTH_RETURN(pDevIns, pThis, pStreamShared, VINF_IOM_R3_MMIO_WRITE);
    }

    hdaR3StreamLock(pStreamR3);

    uint32_t v = HDA_REG_IND(pThis, iReg);

    /* Clear (zero) FIFOE, DESE and BCIS bits when writing 1 to it (6.2.33). */
    HDA_REG_IND(pThis, iReg) &= ~(u32Value & v);

/** @todo r=bird: Check if we couldn't this stuff involving hdaR3StreamPeriodLock
 *  and IRQs after PDMDevHlpTimerUnlockClock. */

    /*
     * ...
     */
    /* Some guests tend to write SDnSTS even if the stream is not running.
     * So make sure to check if the RUN bit is set first. */
    const bool fRunning = pStreamShared->State.fRunning;

    Log3Func(("[SD%RU8] fRunning=%RTbool %R[sdsts]\n", uSD, fRunning, v));

    PHDASTREAMPERIOD pPeriod = &pStreamShared->State.Period;
    hdaR3StreamPeriodLock(pPeriod);
    if (hdaR3StreamPeriodNeedsInterrupt(pPeriod))
        hdaR3StreamPeriodReleaseInterrupt(pPeriod);
    if (hdaR3StreamPeriodIsComplete(pPeriod))
    {
        /* Make sure to try to update the WALCLK register if a period is complete.
         * Use the maximum WALCLK value all (active) streams agree to. */
        const uint64_t uWalClkMax = hdaR3WalClkGetMax(pThis, PDMDEVINS_2_DATA_CC(pDevIns, PHDASTATER3));
        if (uWalClkMax > hdaWalClkGetCurrent(pThis))
            hdaR3WalClkSet(pThis, pThisCC, uWalClkMax, false /* fForce */);

        hdaR3StreamPeriodEnd(pPeriod);

        if (fRunning)
            hdaR3StreamPeriodBegin(pPeriod, hdaWalClkGetCurrent(pThis) /* Use current wall clock time */);
    }
    hdaR3StreamPeriodUnlock(pPeriod); /* Unlock before processing interrupt. */

    HDA_PROCESS_INTERRUPT(pDevIns, pThis);

    /*
     * ...
     */
    uint64_t cTicksToNext = pStreamShared->State.cTransferTicks;
    Assert(cTicksToNext == cTicksToNext);
    if (cTicksToNext) /* Only do any calculations if the stream currently is set up for transfers. */
    {
        const uint64_t tsNow = PDMDevHlpTimerGet(pDevIns, pStreamShared->hTimer);
        Assert(tsNow >= pStreamShared->State.tsTransferLast);

        const uint64_t cTicksElapsed     = tsNow - pStreamShared->State.tsTransferLast;
# ifdef LOG_ENABLED
        const uint64_t cTicksTransferred = pStreamShared->State.cbTransferProcessed * pStreamShared->State.cTicksPerByte;
# endif

        Log3Func(("[SD%RU8] cTicksElapsed=%RU64, cTicksTransferred=%RU64, cTicksToNext=%RU64\n",
                  uSD, cTicksElapsed, cTicksTransferred, cTicksToNext));

        Log3Func(("[SD%RU8] cbTransferProcessed=%RU32, cbTransferChunk=%RU32, cbTransferSize=%RU32\n", uSD,
                  pStreamShared->State.cbTransferProcessed, pStreamShared->State.cbTransferChunk, pStreamShared->State.cbTransferSize));

        if (cTicksElapsed <= cTicksToNext)
            cTicksToNext = cTicksToNext - cTicksElapsed;
        else /* Catch up. */
        {
            Log3Func(("[SD%RU8] Warning: Lagging behind (%RU64 ticks elapsed, maximum allowed is %RU64)\n",
                     uSD, cTicksElapsed, cTicksToNext));

            LogRelMax2(64, ("HDA: Stream #%RU8 interrupt lagging behind (expected %uus, got %uus), trying to catch up ...\n", uSD,
                            (PDMDevHlpTimerGetFreq(pDevIns, pStreamShared->hTimer) / pThis->uTimerHz) / 1000,
                            (tsNow - pStreamShared->State.tsTransferLast) / 1000));

            cTicksToNext = 0;
        }

        Log3Func(("[SD%RU8] -> cTicksToNext=%RU64\n", uSD, cTicksToNext));

        /* Reset processed data counter. */
        pStreamShared->State.cbTransferProcessed = 0;
        pStreamShared->State.tsTransferNext      = tsNow + cTicksToNext;

        /* Only re-arm the timer if there were pending transfer interrupts left
         *  -- it could happen that we land in here if a guest writes to SDnSTS
         * unconditionally. */
        if (pStreamShared->State.cTransferPendingInterrupts)
        {
            pStreamShared->State.cTransferPendingInterrupts--;

            /* Re-arm the timer. */
            LogFunc(("Timer set SD%RU8\n", uSD));
            hdaR3TimerSet(pDevIns, pStreamShared, tsNow + cTicksToNext,
                          true /* fForce - we just set tsTransferNext*/, 0 /*tsNow*/);
        }
    }

    if (cTransferTicks)
        PDMDevHlpTimerUnlockClock(pDevIns, pStreamShared->hTimer); /* Caller will unlock pThis->CritSect. */
    hdaR3StreamUnlock(pStreamR3);
    return VINF_SUCCESS;
#else  /* !IN_RING3 */
    RT_NOREF(pDevIns, pThis, iReg, u32Value);
    return VINF_IOM_R3_MMIO_WRITE;
#endif /* !IN_RING3 */
}

static VBOXSTRICTRC hdaRegWriteSDLVI(PPDMDEVINS pDevIns, PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    const size_t idxStream = HDA_SD_NUM_FROM_REG(pThis, LVI, iReg);
    AssertReturn(idxStream < RT_ELEMENTS(pThis->aStreams), VERR_INTERNAL_ERROR_3); /* paranoia^2: Bad g_aHdaRegMap. */

#ifdef HDA_USE_DMA_ACCESS_HANDLER
    if (hdaGetDirFromSD(uSD) == PDMAUDIODIR_OUT)
    {
        /* Try registering the DMA handlers.
         * As we can't be sure in which order LVI + BDL base are set, try registering in both routines. */
        PHDASTREAM pStream = hdaGetStreamFromSD(pThis, idxStream);
        if (   pStream
            && hdaR3StreamRegisterDMAHandlers(pThis, pStream))
            LogFunc(("[SD%RU8] DMA logging enabled\n", pStream->u8SD));
    }
#endif

    ASSERT_GUEST_LOGREL_MSG(u32Value <= UINT8_MAX, /* Should be covered by the register write mask, but just to make sure. */
                            ("LVI for stream #%zu must not be bigger than %RU8\n", idxStream, UINT8_MAX - 1));
    return hdaRegWriteU16(pDevIns, pThis, iReg, u32Value);
}

static VBOXSTRICTRC hdaRegWriteSDFIFOW(PPDMDEVINS pDevIns, PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    size_t const idxStream = HDA_SD_NUM_FROM_REG(pThis, FIFOW, iReg);
    AssertReturn(idxStream < RT_ELEMENTS(pThis->aStreams), VERR_INTERNAL_ERROR_3); /* paranoia^2: Bad g_aHdaRegMap. */

    if (RT_LIKELY(hdaGetDirFromSD((uint8_t)idxStream) == PDMAUDIODIR_IN)) /* FIFOW for input streams only. */
    { /* likely */ }
    else
    {
#ifndef IN_RING0
        LogRel(("HDA: Warning: Guest tried to write read-only FIFOW to output stream #%RU8, ignoring\n", idxStream));
        return VINF_SUCCESS;
#else
        return VINF_IOM_R3_MMIO_WRITE; /* (Go to ring-3 for release logging.) */
#endif
    }

    uint32_t u32FIFOW = 0;
    switch (u32Value)
    {
        case HDA_SDFIFOW_8B:
        case HDA_SDFIFOW_16B:
        case HDA_SDFIFOW_32B:
            u32FIFOW = u32Value;
            break;
        default:
            ASSERT_GUEST_LOGREL_MSG_FAILED(("Guest tried writing unsupported FIFOW (0x%zx) to stream #%RU8, defaulting to 32 bytes\n",
                                            u32Value, idxStream));
            u32FIFOW = HDA_SDFIFOW_32B;
            break;
    }

    if (u32FIFOW) /** @todo r=bird: Logic error. it will never be zero, so why this check? */
    {
        pThis->aStreams[idxStream].u16FIFOW = hdaSDFIFOWToBytes(u32FIFOW);
        LogFunc(("[SD%zu] Updating FIFOW to %u bytes\n", idxStream, pThis->aStreams[idxStream].u16FIFOW));
        return hdaRegWriteU16(pDevIns, pThis, iReg, u32FIFOW);
    }
    return VINF_SUCCESS;
}

/**
 * @note This method could be called for changing value on Output Streams only (ICH6 datasheet 18.2.39).
 */
static VBOXSTRICTRC hdaRegWriteSDFIFOS(PPDMDEVINS pDevIns, PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    uint8_t uSD = HDA_SD_NUM_FROM_REG(pThis, FIFOS, iReg);

    ASSERT_GUEST_LOGREL_MSG_RETURN(hdaGetDirFromSD(uSD) == PDMAUDIODIR_OUT, /* FIFOS for output streams only. */
                                   ("Guest tried writing read-only FIFOS to input stream #%RU8, ignoring\n", uSD),
                                   VINF_SUCCESS);

    uint32_t u32FIFOS;
    switch (u32Value)
    {
        case HDA_SDOFIFO_16B:
        case HDA_SDOFIFO_32B:
        case HDA_SDOFIFO_64B:
        case HDA_SDOFIFO_128B:
        case HDA_SDOFIFO_192B:
        case HDA_SDOFIFO_256B:
            u32FIFOS = u32Value;
            break;

        default:
            ASSERT_GUEST_LOGREL_MSG_FAILED(("Guest tried writing unsupported FIFOS (0x%x) to stream #%RU8, defaulting to 192 bytes\n",
                                            u32Value, uSD));
            u32FIFOS = HDA_SDOFIFO_192B;
            break;
    }

    return hdaRegWriteU16(pDevIns, pThis, iReg, u32FIFOS);
}

#ifdef IN_RING3

/**
 * Adds an audio output stream to the device setup using the given configuration.
 *
 * @returns IPRT status code.
 * @param   pThisCC             The ring-3 HDA device state.
 * @param   pCfg                Stream configuration to use for adding a stream.
 */
static int hdaR3AddStreamOut(PHDASTATER3 pThisCC, PPDMAUDIOSTREAMCFG pCfg)
{
    AssertPtrReturn(pCfg,  VERR_INVALID_POINTER);

    AssertReturn(pCfg->enmDir == PDMAUDIODIR_OUT, VERR_INVALID_PARAMETER);

    LogFlowFunc(("Stream=%s\n", pCfg->szName));

    int rc = VINF_SUCCESS;

    bool fUseFront = true; /* Always use front out by default. */
# ifdef VBOX_WITH_AUDIO_HDA_51_SURROUND
    bool fUseRear;
    bool fUseCenter;
    bool fUseLFE;

    fUseRear = fUseCenter = fUseLFE = false;

    /*
     * Use commonly used setups for speaker configurations.
     */

    /** @todo Make the following configurable through mixer API and/or CFGM? */
    switch (pCfg->Props.cChannels)
    {
        case 3:  /* 2.1: Front (Stereo) + LFE. */
        {
            fUseLFE   = true;
            break;
        }

        case 4:  /* Quadrophonic: Front (Stereo) + Rear (Stereo). */
        {
            fUseRear  = true;
            break;
        }

        case 5:  /* 4.1: Front (Stereo) + Rear (Stereo) + LFE. */
        {
            fUseRear  = true;
            fUseLFE   = true;
            break;
        }

        case 6:  /* 5.1: Front (Stereo) + Rear (Stereo) + Center/LFE. */
        {
            fUseRear   = true;
            fUseCenter = true;
            fUseLFE    = true;
            break;
        }

        default: /* Unknown; fall back to 2 front channels (stereo). */
        {
            rc = VERR_NOT_SUPPORTED;
            break;
        }
    }
# endif /* !VBOX_WITH_AUDIO_HDA_51_SURROUND */

    if (rc == VERR_NOT_SUPPORTED)
    {
        LogRel2(("HDA: Warning: Unsupported channel count (%RU8), falling back to stereo channels (2)\n", pCfg->Props.cChannels));

        /* Fall back to 2 channels (see below in fUseFront block). */
        rc = VINF_SUCCESS;
    }

    do
    {
        if (RT_FAILURE(rc))
            break;

        if (fUseFront)
        {
            RTStrPrintf(pCfg->szName, RT_ELEMENTS(pCfg->szName), "Front");

            pCfg->u.enmDst          = PDMAUDIOPLAYBACKDST_FRONT;
            pCfg->enmLayout         = PDMAUDIOSTREAMLAYOUT_NON_INTERLEAVED;

            pCfg->Props.cShift      = PDMAUDIOPCMPROPS_MAKE_SHIFT_PARMS(pCfg->Props.cbSample, pCfg->Props.cChannels);

            rc = hdaCodecAddStream(pThisCC->pCodec, PDMAUDIOMIXERCTL_FRONT, pCfg);
        }

# ifdef VBOX_WITH_AUDIO_HDA_51_SURROUND
        if (   RT_SUCCESS(rc)
            && (fUseCenter || fUseLFE))
        {
            RTStrPrintf(pCfg->szName, RT_ELEMENTS(pCfg->szName), "Center/LFE");

            pCfg->u.enmDst          = PDMAUDIOPLAYBACKDST_CENTER_LFE;
            pCfg->enmLayout         = PDMAUDIOSTREAMLAYOUT_NON_INTERLEAVED;

            pCfg->Props.cChannels   = (fUseCenter && fUseLFE) ? 2 : 1;
            pCfg->Props.cShift      = PDMAUDIOPCMPROPS_MAKE_SHIFT_PARMS(pCfg->Props.cbSample, pCfg->Props.cChannels);

            rc = hdaCodecAddStream(pThisCC->pCodec, PDMAUDIOMIXERCTL_CENTER_LFE, pCfg);
        }

        if (   RT_SUCCESS(rc)
            && fUseRear)
        {
            RTStrPrintf(pCfg->szName, RT_ELEMENTS(pCfg->szName), "Rear");

            pCfg->u.enmDst          = PDMAUDIOPLAYBACKDST_REAR;
            pCfg->enmLayout         = PDMAUDIOSTREAMLAYOUT_NON_INTERLEAVED;

            pCfg->Props.cChannels   = 2;
            pCfg->Props.cShift      = PDMAUDIOPCMPROPS_MAKE_SHIFT_PARMS(pCfg->Props.cbSample, pCfg->Props.cChannels);

            rc = hdaCodecAddStream(pThisCC->pCodec, PDMAUDIOMIXERCTL_REAR, pCfg);
        }
# endif /* VBOX_WITH_AUDIO_HDA_51_SURROUND */

    } while (0);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Adds an audio input stream to the device setup using the given configuration.
 *
 * @returns IPRT status code.
 * @param   pThisCC             The ring-3 HDA device state.
 * @param   pCfg                Stream configuration to use for adding a stream.
 */
static int hdaR3AddStreamIn(PHDASTATER3 pThisCC, PPDMAUDIOSTREAMCFG pCfg)
{
    AssertPtrReturn(pCfg,  VERR_INVALID_POINTER);

    AssertReturn(pCfg->enmDir == PDMAUDIODIR_IN, VERR_INVALID_PARAMETER);

    LogFlowFunc(("Stream=%s, Source=%ld\n", pCfg->szName, pCfg->u.enmSrc));

    int rc;
    switch (pCfg->u.enmSrc)
    {
        case PDMAUDIORECSRC_LINE:
            rc = hdaCodecAddStream(pThisCC->pCodec, PDMAUDIOMIXERCTL_LINE_IN, pCfg);
            break;
# ifdef VBOX_WITH_AUDIO_HDA_MIC_IN
        case PDMAUDIORECSRC_MIC:
            rc = hdaCodecAddStream(pThisCC->pCodec, PDMAUDIOMIXERCTL_MIC_IN, pCfg);
            break;
# endif
        default:
            rc = VERR_NOT_SUPPORTED;
            break;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Adds an audio stream to the device setup using the given configuration.
 *
 * @returns IPRT status code.
 * @param   pThisCC             The ring-3 HDA device state.
 * @param   pCfg                Stream configuration to use for adding a stream.
 */
static int hdaR3AddStream(PHDASTATER3 pThisCC, PPDMAUDIOSTREAMCFG pCfg)
{
    AssertPtrReturn(pCfg,  VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    int rc;
    switch (pCfg->enmDir)
    {
        case PDMAUDIODIR_OUT:
            rc = hdaR3AddStreamOut(pThisCC, pCfg);
            break;

        case PDMAUDIODIR_IN:
            rc = hdaR3AddStreamIn(pThisCC, pCfg);
            break;

        default:
            rc = VERR_NOT_SUPPORTED;
            AssertFailed();
            break;
    }

    LogFlowFunc(("Returning %Rrc\n", rc));

    return rc;
}

/**
 * Removes an audio stream from the device setup using the given configuration.
 *
 * @returns IPRT status code.
 * @param   pThisCC             The ring-3 HDA device state.
 * @param   pCfg                Stream configuration to use for removing a stream.
 */
static int hdaR3RemoveStream(PHDASTATER3 pThisCC, PPDMAUDIOSTREAMCFG pCfg)
{
    AssertPtrReturn(pCfg,  VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;

    PDMAUDIOMIXERCTL enmMixerCtl = PDMAUDIOMIXERCTL_UNKNOWN;
    switch (pCfg->enmDir)
    {
        case PDMAUDIODIR_IN:
        {
            LogFlowFunc(("Stream=%s, Source=%ld\n", pCfg->szName, pCfg->u.enmSrc));

            switch (pCfg->u.enmSrc)
            {
                case PDMAUDIORECSRC_UNKNOWN: break;
                case PDMAUDIORECSRC_LINE:    enmMixerCtl = PDMAUDIOMIXERCTL_LINE_IN; break;
# ifdef VBOX_WITH_AUDIO_HDA_MIC_IN
                case PDMAUDIORECSRC_MIC:     enmMixerCtl = PDMAUDIOMIXERCTL_MIC_IN;  break;
# endif
                default:
                    rc = VERR_NOT_SUPPORTED;
                    break;
            }

            break;
        }

        case PDMAUDIODIR_OUT:
        {
            LogFlowFunc(("Stream=%s, Source=%ld\n", pCfg->szName, pCfg->u.enmDst));

            switch (pCfg->u.enmDst)
            {
                case PDMAUDIOPLAYBACKDST_UNKNOWN:    break;
                case PDMAUDIOPLAYBACKDST_FRONT:      enmMixerCtl = PDMAUDIOMIXERCTL_FRONT;      break;
# ifdef VBOX_WITH_AUDIO_HDA_51_SURROUND
                case PDMAUDIOPLAYBACKDST_CENTER_LFE: enmMixerCtl = PDMAUDIOMIXERCTL_CENTER_LFE; break;
                case PDMAUDIOPLAYBACKDST_REAR:       enmMixerCtl = PDMAUDIOMIXERCTL_REAR;       break;
# endif
                default:
                    rc = VERR_NOT_SUPPORTED;
                    break;
            }
            break;
        }

        default:
            rc = VERR_NOT_SUPPORTED;
            break;
    }

    if (   RT_SUCCESS(rc)
        && enmMixerCtl != PDMAUDIOMIXERCTL_UNKNOWN)
    {
        rc = hdaCodecRemoveStream(pThisCC->pCodec, enmMixerCtl);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

#endif /* IN_RING3 */

static VBOXSTRICTRC hdaRegWriteSDFMT(PPDMDEVINS pDevIns, PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    /*
     * Write the wanted stream format into the register in any case.
     *
     * This is important for e.g. MacOS guests, as those try to initialize streams which are not reported
     * by the device emulation (wants 4 channels, only have 2 channels at the moment).
     *
     * When ignoring those (invalid) formats, this leads to MacOS thinking that the device is malfunctioning
     * and therefore disabling the device completely.
     */
    return hdaRegWriteU16(pDevIns, pThis, iReg, u32Value);
}

/**
 * Worker for writes to the BDPL and BDPU registers.
 */
DECLINLINE(VBOXSTRICTRC) hdaRegWriteSDBDPX(PPDMDEVINS pDevIns, PHDASTATE pThis, uint32_t iReg, uint32_t u32Value, uint8_t uSD)
{
#ifndef HDA_USE_DMA_ACCESS_HANDLER
    RT_NOREF(uSD);
    return hdaRegWriteU32(pDevIns, pThis, iReg, u32Value);
#else
# ifdef IN_RING3
    if (hdaGetDirFromSD(uSD) == PDMAUDIODIR_OUT)
    {
        /* Try registering the DMA handlers.
         * As we can't be sure in which order LVI + BDL base are set, try registering in both routines. */
        PHDASTREAM pStream = hdaGetStreamFromSD(pThis, uSD);
        if (   pStream
            && hdaR3StreamRegisterDMAHandlers(pThis, pStream))
            LogFunc(("[SD%RU8] DMA logging enabled\n", pStream->u8SD));
    }
    return hdaRegWriteU32(pDevIns, pThis, iReg, u32Value);
# else
    RT_NOREF(pDevIns, pThis, iReg, u32Value, uSD);
    return VINF_IOM_R3_MMIO_WRITE;
# endif
#endif
}

static VBOXSTRICTRC hdaRegWriteSDBDPL(PPDMDEVINS pDevIns, PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    return hdaRegWriteSDBDPX(pDevIns, pThis, iReg, u32Value, HDA_SD_NUM_FROM_REG(pThis, BDPL, iReg));
}

static VBOXSTRICTRC hdaRegWriteSDBDPU(PPDMDEVINS pDevIns, PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    return hdaRegWriteSDBDPX(pDevIns, pThis, iReg, u32Value, HDA_SD_NUM_FROM_REG(pThis, BDPU, iReg));
}

static VBOXSTRICTRC hdaRegReadIRS(PPDMDEVINS pDevIns, PHDASTATE pThis, uint32_t iReg, uint32_t *pu32Value)
{
    /* regarding 3.4.3 we should mark IRS as busy in case CORB is active */
    if (   HDA_REG(pThis, CORBWP) != HDA_REG(pThis, CORBRP)
        || (HDA_REG(pThis, CORBCTL) & HDA_CORBCTL_DMA))
        HDA_REG(pThis, IRS) = HDA_IRS_ICB;  /* busy */

    return hdaRegReadU32(pDevIns, pThis, iReg, pu32Value);
}

static VBOXSTRICTRC hdaRegWriteIRS(PPDMDEVINS pDevIns, PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    RT_NOREF(pDevIns, iReg);

    /*
     * If the guest set the ICB bit of IRS register, HDA should process the verb in IC register,
     * write the response to IR register, and set the IRV (valid in case of success) bit of IRS register.
     */
    if (   (u32Value & HDA_IRS_ICB)
        && !(HDA_REG(pThis, IRS) & HDA_IRS_ICB))
    {
#ifdef IN_RING3
        uint32_t uCmd = HDA_REG(pThis, IC);

        if (HDA_REG(pThis, CORBWP) != HDA_REG(pThis, CORBRP))
        {
            /*
             * 3.4.3: Defines behavior of immediate Command status register.
             */
            LogRel(("HDA: Guest attempted process immediate verb (%x) with active CORB\n", uCmd));
            return VINF_SUCCESS;
        }

        HDA_REG(pThis, IRS) = HDA_IRS_ICB;  /* busy */

        PHDASTATER3 pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PHDASTATER3);
        uint64_t    uResp   = 0;
        int rc2 = pThisCC->pCodec->pfnLookup(pThisCC->pCodec, HDA_CODEC_CMD(uCmd, 0 /* LUN */), &uResp);
        if (RT_FAILURE(rc2))
            LogFunc(("Codec lookup failed with rc2=%Rrc\n", rc2));

        HDA_REG(pThis, IR)   = (uint32_t)uResp; /** @todo r=andy Do we need a 64-bit response? */
        HDA_REG(pThis, IRS)  = HDA_IRS_IRV;     /* result is ready  */
        /** @todo r=michaln We just set the IRS value, why are we clearing unset bits? */
        HDA_REG(pThis, IRS) &= ~HDA_IRS_ICB;    /* busy is clear */

        return VINF_SUCCESS;
#else  /* !IN_RING3 */
        return VINF_IOM_R3_MMIO_WRITE;
#endif /* !IN_RING3 */
    }

    /*
     * Once the guest read the response, it should clear the IRV bit of the IRS register.
     */
    HDA_REG(pThis, IRS) &= ~(u32Value & HDA_IRS_IRV);
    return VINF_SUCCESS;
}

static VBOXSTRICTRC hdaRegWriteRIRBWP(PPDMDEVINS pDevIns, PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    RT_NOREF(pDevIns, iReg);

    if (HDA_REG(pThis, CORBCTL) & HDA_CORBCTL_DMA) /* Ignore request if CORB DMA engine is (still) running. */
        LogFunc(("CORB DMA (still) running, skipping\n"));
    else
    {
        if (u32Value & HDA_RIRBWP_RST)
        {
            /* Do a RIRB reset. */
            if (pThis->cbRirbBuf)
                RT_ZERO(pThis->au64RirbBuf);

            LogRel2(("HDA: RIRB reset\n"));

            HDA_REG(pThis, RIRBWP) = 0;
        }
        /* The remaining bits are O, see 6.2.22. */
    }
    return VINF_SUCCESS;
}

static VBOXSTRICTRC hdaRegWriteRINTCNT(PPDMDEVINS pDevIns, PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    RT_NOREF(pDevIns);
    if (HDA_REG(pThis, CORBCTL) & HDA_CORBCTL_DMA) /* Ignore request if CORB DMA engine is (still) running. */
    {
        LogFunc(("CORB DMA is (still) running, skipping\n"));
        return VINF_SUCCESS;
    }

    VBOXSTRICTRC rc = hdaRegWriteU16(pDevIns, pThis, iReg, u32Value);
    AssertRC(VBOXSTRICTRC_VAL(rc));

    /** @todo r=bird: Shouldn't we make sure the HDASTATE::u16RespIntCnt is below
     *        the new RINTCNT value?  Or alterantively, make the DMA look take
     *        this into account instead...  I'll do the later for now. */

    LogFunc(("Response interrupt count is now %RU8\n", HDA_REG(pThis, RINTCNT) & 0xFF));
    return rc;
}

static VBOXSTRICTRC hdaRegWriteBase(PPDMDEVINS pDevIns, PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    RT_NOREF(pDevIns);

    VBOXSTRICTRC rc = hdaRegWriteU32(pDevIns, pThis, iReg, u32Value);
    AssertRCSuccess(VBOXSTRICTRC_VAL(rc));

    uint32_t const iRegMem = g_aHdaRegMap[iReg].mem_idx;
    switch (iReg)
    {
        case HDA_REG_CORBLBASE:
            pThis->u64CORBBase &= UINT64_C(0xFFFFFFFF00000000);
            pThis->u64CORBBase |= pThis->au32Regs[iRegMem];
            break;
        case HDA_REG_CORBUBASE:
            pThis->u64CORBBase &= UINT64_C(0x00000000FFFFFFFF);
            pThis->u64CORBBase |= (uint64_t)pThis->au32Regs[iRegMem] << 32;
            break;
        case HDA_REG_RIRBLBASE:
            pThis->u64RIRBBase &= UINT64_C(0xFFFFFFFF00000000);
            pThis->u64RIRBBase |= pThis->au32Regs[iRegMem];
            break;
        case HDA_REG_RIRBUBASE:
            pThis->u64RIRBBase &= UINT64_C(0x00000000FFFFFFFF);
            pThis->u64RIRBBase |= (uint64_t)pThis->au32Regs[iRegMem] << 32;
            break;
        case HDA_REG_DPLBASE:
            pThis->u64DPBase = pThis->au32Regs[iRegMem] & DPBASE_ADDR_MASK;
            Assert(pThis->u64DPBase % 128 == 0); /* Must be 128-byte aligned. */

            /* Also make sure to handle the DMA position enable bit. */
            pThis->fDMAPosition = pThis->au32Regs[iRegMem] & RT_BIT_32(0);
            LogRel(("HDA: %s DMA position buffer\n", pThis->fDMAPosition ? "Enabled" : "Disabled"));
            break;
        case HDA_REG_DPUBASE:
            pThis->u64DPBase = RT_MAKE_U64(RT_LO_U32(pThis->u64DPBase) & DPBASE_ADDR_MASK, pThis->au32Regs[iRegMem]);
            break;
        default:
            AssertMsgFailed(("Invalid index\n"));
            break;
    }

    LogFunc(("CORB base:%llx RIRB base: %llx DP base: %llx\n",
             pThis->u64CORBBase, pThis->u64RIRBBase, pThis->u64DPBase));
    return rc;
}

static VBOXSTRICTRC hdaRegWriteRIRBSTS(PPDMDEVINS pDevIns, PHDASTATE pThis, uint32_t iReg, uint32_t u32Value)
{
    RT_NOREF(pDevIns, iReg);

    uint8_t v = HDA_REG(pThis, RIRBSTS);
    HDA_REG(pThis, RIRBSTS) &= ~(v & u32Value);

    HDA_PROCESS_INTERRUPT(pDevIns, pThis);
    return VINF_SUCCESS;
}

#ifdef IN_RING3

/**
 * Retrieves a corresponding sink for a given mixer control.
 *
 * @return  Pointer to the sink, NULL if no sink is found.
 * @param   pThisCC             The ring-3 HDA device state.
 * @param   enmMixerCtl         Mixer control to get the corresponding sink for.
 */
static PHDAMIXERSINK hdaR3MixerControlToSink(PHDASTATER3 pThisCC, PDMAUDIOMIXERCTL enmMixerCtl)
{
    PHDAMIXERSINK pSink;

    switch (enmMixerCtl)
    {
        case PDMAUDIOMIXERCTL_VOLUME_MASTER:
            /* Fall through is intentional. */
        case PDMAUDIOMIXERCTL_FRONT:
            pSink = &pThisCC->SinkFront;
            break;
# ifdef VBOX_WITH_AUDIO_HDA_51_SURROUND
        case PDMAUDIOMIXERCTL_CENTER_LFE:
            pSink = &pThisCC->SinkCenterLFE;
            break;
        case PDMAUDIOMIXERCTL_REAR:
            pSink = &pThisCC->SinkRear;
            break;
# endif
        case PDMAUDIOMIXERCTL_LINE_IN:
            pSink = &pThisCC->SinkLineIn;
            break;
# ifdef VBOX_WITH_AUDIO_HDA_MIC_IN
        case PDMAUDIOMIXERCTL_MIC_IN:
            pSink = &pThisCC->SinkMicIn;
            break;
# endif
        default:
            AssertMsgFailed(("Unhandled mixer control\n"));
            pSink = NULL;
            break;
    }

    return pSink;
}

/**
 * Adds a specific HDA driver to the driver chain.
 *
 * @return IPRT status code.
 * @param   pThisCC             The ring-3 HDA device state.
 * @param  pDrv                 HDA driver to add.
 */
static int hdaR3MixerAddDrv(PHDASTATER3 pThisCC, PHDADRIVER pDrv)
{
    int rc = VINF_SUCCESS;

    PHDASTREAM pStream = hdaR3GetSharedStreamFromSink(&pThisCC->SinkLineIn);
    if (   pStream
        && DrvAudioHlpStreamCfgIsValid(&pStream->State.Cfg))
    {
        int rc2 = hdaR3MixerAddDrvStream(pThisCC->SinkLineIn.pMixSink, &pStream->State.Cfg, pDrv);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

# ifdef VBOX_WITH_AUDIO_HDA_MIC_IN
    pStream = hdaR3GetSharedStreamFromSink(&pThisCC->SinkMicIn);
    if (   pStream
        && DrvAudioHlpStreamCfgIsValid(&pStream->State.Cfg))
    {
        int rc2 = hdaR3MixerAddDrvStream(pThisCC->SinkMicIn.pMixSink, &pStream->State.Cfg, pDrv);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }
# endif

    pStream = hdaR3GetSharedStreamFromSink(&pThisCC->SinkFront);
    if (   pStream
        && DrvAudioHlpStreamCfgIsValid(&pStream->State.Cfg))
    {
        int rc2 = hdaR3MixerAddDrvStream(pThisCC->SinkFront.pMixSink, &pStream->State.Cfg, pDrv);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

# ifdef VBOX_WITH_AUDIO_HDA_51_SURROUND
    pStream = hdaR3GetSharedStreamFromSink(&pThisCC->SinkCenterLFE);
    if (   pStream
        && DrvAudioHlpStreamCfgIsValid(&pStream->State.Cfg))
    {
        int rc2 = hdaR3MixerAddDrvStream(pThisCC->SinkCenterLFE.pMixSink, &pStream->State.Cfg, pDrv);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    pStream = hdaR3GetSharedStreamFromSink(&pThisCC->SinkRear);
    if (   pStream
        && DrvAudioHlpStreamCfgIsValid(&pStream->State.Cfg))
    {
        int rc2 = hdaR3MixerAddDrvStream(pThisCC->SinkRear.pMixSink, &pStream->State.Cfg, pDrv);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }
# endif

    return rc;
}

/**
 * Removes a specific HDA driver from the driver chain and destroys its
 * associated streams.
 *
 * @param   pThisCC             The ring-3 HDA device state.
 * @param   pDrv                HDA driver to remove.
 */
static void hdaR3MixerRemoveDrv(PHDASTATER3 pThisCC, PHDADRIVER pDrv)
{
    AssertPtrReturnVoid(pDrv);

    if (pDrv->LineIn.pMixStrm)
    {
        if (AudioMixerSinkGetRecordingSource(pThisCC->SinkLineIn.pMixSink) == pDrv->LineIn.pMixStrm)
            AudioMixerSinkSetRecordingSource(pThisCC->SinkLineIn.pMixSink, NULL);

        AudioMixerSinkRemoveStream(pThisCC->SinkLineIn.pMixSink, pDrv->LineIn.pMixStrm);
        AudioMixerStreamDestroy(pDrv->LineIn.pMixStrm);
        pDrv->LineIn.pMixStrm = NULL;
    }

# ifdef VBOX_WITH_AUDIO_HDA_MIC_IN
    if (pDrv->MicIn.pMixStrm)
    {
        if (AudioMixerSinkGetRecordingSource(pThisCC->SinkMicIn.pMixSink) == pDrv->MicIn.pMixStrm)
            AudioMixerSinkSetRecordingSource(&pThisCC->SinkMicIn.pMixSink, NULL);

        AudioMixerSinkRemoveStream(pThisCC->SinkMicIn.pMixSink, pDrv->MicIn.pMixStrm);
        AudioMixerStreamDestroy(pDrv->MicIn.pMixStrm);
        pDrv->MicIn.pMixStrm = NULL;
    }
# endif

    if (pDrv->Front.pMixStrm)
    {
        AudioMixerSinkRemoveStream(pThisCC->SinkFront.pMixSink, pDrv->Front.pMixStrm);
        AudioMixerStreamDestroy(pDrv->Front.pMixStrm);
        pDrv->Front.pMixStrm = NULL;
    }

# ifdef VBOX_WITH_AUDIO_HDA_51_SURROUND
    if (pDrv->CenterLFE.pMixStrm)
    {
        AudioMixerSinkRemoveStream(pThisCC->SinkCenterLFE.pMixSink, pDrv->CenterLFE.pMixStrm);
        AudioMixerStreamDestroy(pDrv->CenterLFE.pMixStrm);
        pDrv->CenterLFE.pMixStrm = NULL;
    }

    if (pDrv->Rear.pMixStrm)
    {
        AudioMixerSinkRemoveStream(pThisCC->SinkRear.pMixSink, pDrv->Rear.pMixStrm);
        AudioMixerStreamDestroy(pDrv->Rear.pMixStrm);
        pDrv->Rear.pMixStrm = NULL;
    }
# endif

    RTListNodeRemove(&pDrv->Node);
}

/**
 * Adds a driver stream to a specific mixer sink.
 *
 * @returns IPRT status code (ignored by caller).
 * @param   pMixSink            Audio mixer sink to add audio streams to.
 * @param   pCfg                Audio stream configuration to use for the audio streams to add.
 * @param   pDrv                Driver stream to add.
 */
static int hdaR3MixerAddDrvStream(PAUDMIXSINK pMixSink, PPDMAUDIOSTREAMCFG pCfg, PHDADRIVER pDrv)
{
    AssertPtrReturn(pMixSink, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg,     VERR_INVALID_POINTER);

    LogFunc(("szSink=%s, szStream=%s, cChannels=%RU8\n", pMixSink->pszName, pCfg->szName, pCfg->Props.cChannels));

    PPDMAUDIOSTREAMCFG pStreamCfg = DrvAudioHlpStreamCfgDup(pCfg);
    if (!pStreamCfg)
        return VERR_NO_MEMORY;

    LogFunc(("[LUN#%RU8] %s\n", pDrv->uLUN, pStreamCfg->szName));

    int rc = VINF_SUCCESS;

    PHDADRIVERSTREAM pDrvStream = NULL;

    if (pStreamCfg->enmDir == PDMAUDIODIR_IN)
    {
        LogFunc(("enmRecSource=%d\n", pStreamCfg->u.enmSrc));

        switch (pStreamCfg->u.enmSrc)
        {
            case PDMAUDIORECSRC_LINE:
                pDrvStream = &pDrv->LineIn;
                break;
# ifdef VBOX_WITH_AUDIO_HDA_MIC_IN
            case PDMAUDIORECSRC_MIC:
                pDrvStream = &pDrv->MicIn;
                break;
# endif
            default:
                rc = VERR_NOT_SUPPORTED;
                break;
        }
    }
    else if (pStreamCfg->enmDir == PDMAUDIODIR_OUT)
    {
        LogFunc(("enmPlaybackDest=%d\n", pStreamCfg->u.enmDst));

        switch (pStreamCfg->u.enmDst)
        {
            case PDMAUDIOPLAYBACKDST_FRONT:
                pDrvStream = &pDrv->Front;
                break;
# ifdef VBOX_WITH_AUDIO_HDA_51_SURROUND
            case PDMAUDIOPLAYBACKDST_CENTER_LFE:
                pDrvStream = &pDrv->CenterLFE;
                break;
            case PDMAUDIOPLAYBACKDST_REAR:
                pDrvStream = &pDrv->Rear;
                break;
# endif
            default:
                rc = VERR_NOT_SUPPORTED;
                break;
        }
    }
    else
        rc = VERR_NOT_SUPPORTED;

    if (RT_SUCCESS(rc))
    {
        AssertPtr(pDrvStream);
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
                                LogRel(("HDA: Set recording source for '%s' to '%s'\n",
                                        pStreamCfg->szName, Cfg.szName));
                        }
                        else
                            LogRel(("HDA: Backend '%s' currently is not offering any recording source for '%s'\n",
                                    Cfg.szName, pStreamCfg->szName));
                    }
                    else if (RT_FAILURE(rc))
                        LogFunc(("LUN#%RU8: Unable to retrieve backend configuration for '%s', rc=%Rrc\n",
                                 pDrv->uLUN, pStreamCfg->szName, rc));
                }
            }
        }

        if (RT_SUCCESS(rc))
            pDrvStream->pMixStrm = pMixStrm;
    }

    DrvAudioHlpStreamCfgFree(pStreamCfg);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Adds all current driver streams to a specific mixer sink.
 *
 * @returns IPRT status code.
 * @param   pThisCC             The ring-3 HDA device state.
 * @param   pMixSink            Audio mixer sink to add stream to.
 * @param   pCfg                Audio stream configuration to use for the audio streams to add.
 */
static int hdaR3MixerAddDrvStreams(PHDASTATER3 pThisCC, PAUDMIXSINK pMixSink, PPDMAUDIOSTREAMCFG pCfg)
{
    AssertPtrReturn(pMixSink, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg,     VERR_INVALID_POINTER);

    LogFunc(("Sink=%s, Stream=%s\n", pMixSink->pszName, pCfg->szName));

    if (!DrvAudioHlpStreamCfgIsValid(pCfg))
        return VERR_INVALID_PARAMETER;

    int rc = AudioMixerSinkSetFormat(pMixSink, &pCfg->Props);
    if (RT_FAILURE(rc))
        return rc;

    PHDADRIVER pDrv;
    RTListForEach(&pThisCC->lstDrv, pDrv, HDADRIVER, Node)
    {
        int rc2 = hdaR3MixerAddDrvStream(pMixSink, pCfg, pDrv);
        if (RT_FAILURE(rc2))
            LogFunc(("Attaching stream failed with %Rrc\n", rc2));

        /* Do not pass failure to rc here, as there might be drivers which aren't
         * configured / ready yet. */
    }

    return rc;
}

/**
 * @interface_method_impl{HDACODEC,pfnCbMixerAddStream}
 */
static DECLCALLBACK(int) hdaR3MixerAddStream(PPDMDEVINS pDevIns, PDMAUDIOMIXERCTL enmMixerCtl, PPDMAUDIOSTREAMCFG pCfg)
{
    PHDASTATER3 pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PHDASTATER3);
    AssertPtrReturn(pCfg,  VERR_INVALID_POINTER);
    int rc;

    PHDAMIXERSINK pSink = hdaR3MixerControlToSink(pThisCC, enmMixerCtl);
    if (pSink)
    {
        rc = hdaR3MixerAddDrvStreams(pThisCC, pSink->pMixSink, pCfg);

        AssertPtr(pSink->pMixSink);
        LogFlowFunc(("Sink=%s, Mixer control=%s\n", pSink->pMixSink->pszName, DrvAudioHlpAudMixerCtlToStr(enmMixerCtl)));
    }
    else
        rc = VERR_NOT_FOUND;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * @interface_method_impl{HDACODEC,pfnCbMixerRemoveStream}
 */
static DECLCALLBACK(int) hdaR3MixerRemoveStream(PPDMDEVINS pDevIns, PDMAUDIOMIXERCTL enmMixerCtl)
{
    PHDASTATER3 pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PHDASTATER3);
    int         rc;

    PHDAMIXERSINK pSink = hdaR3MixerControlToSink(pThisCC, enmMixerCtl);
    if (pSink)
    {
        PHDADRIVER pDrv;
        RTListForEach(&pThisCC->lstDrv, pDrv, HDADRIVER, Node)
        {
            PAUDMIXSTREAM pMixStream = NULL;
            switch (enmMixerCtl)
            {
                /*
                 * Input.
                 */
                case PDMAUDIOMIXERCTL_LINE_IN:
                    pMixStream = pDrv->LineIn.pMixStrm;
                    pDrv->LineIn.pMixStrm = NULL;
                    break;
# ifdef VBOX_WITH_AUDIO_HDA_MIC_IN
                case PDMAUDIOMIXERCTL_MIC_IN:
                    pMixStream = pDrv->MicIn.pMixStrm;
                    pDrv->MicIn.pMixStrm = NULL;
                    break;
# endif
                /*
                 * Output.
                 */
                case PDMAUDIOMIXERCTL_FRONT:
                    pMixStream = pDrv->Front.pMixStrm;
                    pDrv->Front.pMixStrm = NULL;
                    break;
# ifdef VBOX_WITH_AUDIO_HDA_51_SURROUND
                case PDMAUDIOMIXERCTL_CENTER_LFE:
                    pMixStream = pDrv->CenterLFE.pMixStrm;
                    pDrv->CenterLFE.pMixStrm = NULL;
                    break;
                case PDMAUDIOMIXERCTL_REAR:
                    pMixStream = pDrv->Rear.pMixStrm;
                    pDrv->Rear.pMixStrm = NULL;
                    break;
# endif
                default:
                    AssertMsgFailed(("Mixer control %d not implemented\n", enmMixerCtl));
                    break;
            }

            if (pMixStream)
            {
                AudioMixerSinkRemoveStream(pSink->pMixSink, pMixStream);
                AudioMixerStreamDestroy(pMixStream);

                pMixStream = NULL;
            }
        }

        AudioMixerSinkRemoveAllStreams(pSink->pMixSink);
        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_NOT_FOUND;

    LogFunc(("Mixer control=%s, rc=%Rrc\n", DrvAudioHlpAudMixerCtlToStr(enmMixerCtl), rc));
    return rc;
}

/**
 * @interface_method_impl{HDACODEC,pfnCbMixerControl}
 *
 * @note Is also called directly by the DevHDA code.
 */
static DECLCALLBACK(int) hdaR3MixerControl(PPDMDEVINS pDevIns, PDMAUDIOMIXERCTL enmMixerCtl, uint8_t uSD, uint8_t uChannel)
{
    PHDASTATE   pThis   = PDMDEVINS_2_DATA(pDevIns, PHDASTATE);
    PHDASTATER3 pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PHDASTATER3);
    LogFunc(("enmMixerCtl=%s, uSD=%RU8, uChannel=%RU8\n", DrvAudioHlpAudMixerCtlToStr(enmMixerCtl), uSD, uChannel));

    if (uSD == 0) /* Stream number 0 is reserved. */
    {
        Log2Func(("Invalid SDn (%RU8) number for mixer control '%s', ignoring\n", uSD, DrvAudioHlpAudMixerCtlToStr(enmMixerCtl)));
        return VINF_SUCCESS;
    }
    /* uChannel is optional. */

    /* SDn0 starts as 1. */
    Assert(uSD);
    uSD--;

# ifndef VBOX_WITH_AUDIO_HDA_MIC_IN
    /* Only SDI0 (Line-In) is supported. */
    if (   hdaGetDirFromSD(uSD) == PDMAUDIODIR_IN
        && uSD >= 1)
    {
        LogRel2(("HDA: Dedicated Mic-In support not imlpemented / built-in (stream #%RU8), using Line-In (stream #0) instead\n", uSD));
        uSD = 0;
    }
# endif

    int rc = VINF_SUCCESS;

    PHDAMIXERSINK pSink = hdaR3MixerControlToSink(pThisCC, enmMixerCtl);
    if (pSink)
    {
        AssertPtr(pSink->pMixSink);

        /* If this an output stream, determine the correct SD#. */
        if (   uSD < HDA_MAX_SDI
            && AudioMixerSinkGetDir(pSink->pMixSink) == AUDMIXSINKDIR_OUTPUT)
            uSD += HDA_MAX_SDI;

        /* Make 100% sure we got a good stream number before continuing. */
        AssertLogRelReturn(uSD < RT_ELEMENTS(pThisCC->aStreams), VERR_NOT_IMPLEMENTED);

        /* Detach the existing stream from the sink. */
        if (   pSink->pStreamShared
            && pSink->pStreamR3
            && (   pSink->pStreamShared->u8SD      != uSD
                || pSink->pStreamShared->u8Channel != uChannel)
           )
        {
            LogFunc(("Sink '%s' was assigned to stream #%RU8 (channel %RU8) before\n",
                     pSink->pMixSink->pszName, pSink->pStreamShared->u8SD, pSink->pStreamShared->u8Channel));

            hdaR3StreamLock(pSink->pStreamR3);

            /* Only disable the stream if the stream descriptor # has changed. */
            if (pSink->pStreamShared->u8SD != uSD)
                hdaR3StreamEnable(pSink->pStreamShared, pSink->pStreamR3, false /*fEnable*/);

            pSink->pStreamR3->pMixSink = NULL;

            hdaR3StreamUnlock(pSink->pStreamR3);

            pSink->pStreamShared = NULL;
            pSink->pStreamR3     = NULL;
        }

        /* Attach the new stream to the sink.
         * Enabling the stream will be done by the gust via a separate SDnCTL call then. */
        if (pSink->pStreamShared == NULL)
        {
            LogRel2(("HDA: Setting sink '%s' to stream #%RU8 (channel %RU8), mixer control=%s\n",
                     pSink->pMixSink->pszName, uSD, uChannel, DrvAudioHlpAudMixerCtlToStr(enmMixerCtl)));

            PHDASTREAMR3 pStreamR3     = &pThisCC->aStreams[uSD];
            PHDASTREAM   pStreamShared = &pThis->aStreams[uSD];
            hdaR3StreamLock(pStreamR3);

            pSink->pStreamR3     = pStreamR3;
            pSink->pStreamShared = pStreamShared;

            pStreamShared->u8Channel = uChannel;
            pStreamR3->pMixSink      = pSink;

            hdaR3StreamUnlock(pStreamR3);
            rc = VINF_SUCCESS;
        }
    }
    else
        rc = VERR_NOT_FOUND;

    if (RT_FAILURE(rc))
        LogRel(("HDA: Converter control for stream #%RU8 (channel %RU8) / mixer control '%s' failed with %Rrc, skipping\n",
                uSD, uChannel, DrvAudioHlpAudMixerCtlToStr(enmMixerCtl), rc));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * @interface_method_impl{HDACODEC,pfnCbMixerSetVolume}
 */
static DECLCALLBACK(int) hdaR3MixerSetVolume(PPDMDEVINS pDevIns, PDMAUDIOMIXERCTL enmMixerCtl, PPDMAUDIOVOLUME pVol)
{
    PHDASTATER3   pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PHDASTATER3);
    int           rc;

    PHDAMIXERSINK pSink = hdaR3MixerControlToSink(pThisCC, enmMixerCtl);
    if (   pSink
        && pSink->pMixSink)
    {
        LogRel2(("HDA: Setting volume for mixer sink '%s' to %RU8/%RU8 (%s)\n",
                 pSink->pMixSink->pszName, pVol->uLeft, pVol->uRight, pVol->fMuted ? "Muted" : "Unmuted"));

        /* Set the volume.
         * We assume that the codec already converted it to the correct range. */
        rc = AudioMixerSinkSetVolume(pSink->pMixSink, pVol);
    }
    else
        rc = VERR_NOT_FOUND;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * @callback_method_impl{FNTMTIMERDEV, Main routine for the stream's timer.}
 */
static DECLCALLBACK(void) hdaR3Timer(PPDMDEVINS pDevIns, PTMTIMER pTimer, void *pvUser)
{
    PHDASTATE       pThis         = PDMDEVINS_2_DATA(pDevIns, PHDASTATE);
    PHDASTATER3     pThisCC       = PDMDEVINS_2_DATA_CC(pDevIns, PHDASTATER3);
    uintptr_t       idxStream     = (uintptr_t)pvUser;
    AssertReturnVoid(idxStream < RT_ELEMENTS(pThis->aStreams));
    PHDASTREAM      pStreamShared = &pThis->aStreams[idxStream];
    PHDASTREAMR3    pStreamR3     = &pThisCC->aStreams[idxStream];
    TMTIMERHANDLE   hTimer        = pStreamShared->hTimer;
    RT_NOREF(pTimer);

    Assert(PDMDevHlpCritSectIsOwner(pDevIns, &pThis->CritSect));
    Assert(PDMDevHlpTimerIsLockOwner(pDevIns, hTimer));

    hdaR3StreamUpdate(pDevIns, pThis, pThisCC, pStreamShared, pStreamR3, true /* fInTimer */);

    /* Flag indicating whether to kick the timer again for a new data processing round. */
    bool fSinkActive = false;
    if (pStreamR3->pMixSink)
        fSinkActive = AudioMixerSinkIsActive(pStreamR3->pMixSink->pMixSink);

    if (fSinkActive)
    {
        uint64_t const tsNow = PDMDevHlpTimerGet(pDevIns, hTimer); /* (For virtual sync this remains the same for the whole callout IIRC) */
        const bool fTimerScheduled = hdaR3StreamTransferIsScheduled(pStreamShared, tsNow);
        Log3Func(("fSinksActive=%RTbool, fTimerScheduled=%RTbool\n", fSinkActive, fTimerScheduled));
        if (!fTimerScheduled)
            hdaR3TimerSet(pDevIns, pStreamShared, tsNow + PDMDevHlpTimerGetFreq(pDevIns, hTimer) / pThis->uTimerHz,
                          true /*fForce*/, tsNow /*fixed*/ );
    }
    else
        Log3Func(("fSinksActive=%RTbool\n", fSinkActive));
}

# ifdef HDA_USE_DMA_ACCESS_HANDLER
/**
 * HC access handler for the FIFO.
 *
 * @returns VINF_SUCCESS if the handler have carried out the operation.
 * @returns VINF_PGM_HANDLER_DO_DEFAULT if the caller should carry out the access operation.
 * @param   pVM             VM Handle.
 * @param   pVCpu           The cross context CPU structure for the calling EMT.
 * @param   GCPhys          The physical address the guest is writing to.
 * @param   pvPhys          The HC mapping of that address.
 * @param   pvBuf           What the guest is reading/writing.
 * @param   cbBuf           How much it's reading/writing.
 * @param   enmAccessType   The access type.
 * @param   enmOrigin       Who is making the access.
 * @param   pvUser          User argument.
 */
static DECLCALLBACK(VBOXSTRICTRC) hdaR3DmaAccessHandler(PVM pVM, PVMCPU pVCpu, RTGCPHYS GCPhys, void *pvPhys,
                                                        void *pvBuf, size_t cbBuf,
                                                        PGMACCESSTYPE enmAccessType, PGMACCESSORIGIN enmOrigin, void *pvUser)
{
    RT_NOREF(pVM, pVCpu, pvPhys, pvBuf, enmOrigin);

    PHDADMAACCESSHANDLER pHandler = (PHDADMAACCESSHANDLER)pvUser;
    AssertPtr(pHandler);

    PHDASTREAM pStream = pHandler->pStream;
    AssertPtr(pStream);

    Assert(GCPhys >= pHandler->GCPhysFirst);
    Assert(GCPhys <= pHandler->GCPhysLast);
    Assert(enmAccessType == PGMACCESSTYPE_WRITE);

    /* Not within BDLE range? Bail out. */
    if (   (GCPhys         < pHandler->BDLEAddr)
        || (GCPhys + cbBuf > pHandler->BDLEAddr + pHandler->BDLESize))
    {
        return VINF_PGM_HANDLER_DO_DEFAULT;
    }

    switch (enmAccessType)
    {
        case PGMACCESSTYPE_WRITE:
        {
#  ifdef DEBUG
            PHDASTREAMDEBUG pStreamDbg = &pStream->Dbg;

            const uint64_t tsNowNs     = RTTimeNanoTS();
            const uint32_t tsElapsedMs = (tsNowNs - pStreamDbg->tsWriteSlotBegin) / 1000 / 1000;

            uint64_t cWritesHz   = ASMAtomicReadU64(&pStreamDbg->cWritesHz);
            uint64_t cbWrittenHz = ASMAtomicReadU64(&pStreamDbg->cbWrittenHz);

            if (tsElapsedMs >= (1000 / HDA_TIMER_HZ_DEFAULT))
            {
                LogFunc(("[SD%RU8] %RU32ms elapsed, cbWritten=%RU64, cWritten=%RU64 -- %RU32 bytes on average per time slot (%zums)\n",
                         pStream->u8SD, tsElapsedMs, cbWrittenHz, cWritesHz,
                         ASMDivU64ByU32RetU32(cbWrittenHz, cWritesHz ? cWritesHz : 1), 1000 / HDA_TIMER_HZ_DEFAULT));

                pStreamDbg->tsWriteSlotBegin = tsNowNs;

                cWritesHz   = 0;
                cbWrittenHz = 0;
            }

            cWritesHz   += 1;
            cbWrittenHz += cbBuf;

            ASMAtomicIncU64(&pStreamDbg->cWritesTotal);
            ASMAtomicAddU64(&pStreamDbg->cbWrittenTotal, cbBuf);

            ASMAtomicWriteU64(&pStreamDbg->cWritesHz,   cWritesHz);
            ASMAtomicWriteU64(&pStreamDbg->cbWrittenHz, cbWrittenHz);

            LogFunc(("[SD%RU8] Writing %3zu @ 0x%x (off %zu)\n",
                     pStream->u8SD, cbBuf, GCPhys, GCPhys - pHandler->BDLEAddr));

            LogFunc(("[SD%RU8] cWrites=%RU64, cbWritten=%RU64 -> %RU32 bytes on average\n",
                     pStream->u8SD, pStreamDbg->cWritesTotal, pStreamDbg->cbWrittenTotal,
                     ASMDivU64ByU32RetU32(pStreamDbg->cbWrittenTotal, pStreamDbg->cWritesTotal)));
#  endif

            if (pThis->fDebugEnabled)
            {
                RTFILE fh;
                RTFileOpen(&fh, VBOX_AUDIO_DEBUG_DUMP_PCM_DATA_PATH "hdaDMAAccessWrite.pcm",
                           RTFILE_O_OPEN_CREATE | RTFILE_O_APPEND | RTFILE_O_WRITE | RTFILE_O_DENY_NONE);
                RTFileWrite(fh, pvBuf, cbBuf, NULL);
                RTFileClose(fh);
            }

#  ifdef HDA_USE_DMA_ACCESS_HANDLER_WRITING
            PRTCIRCBUF pCircBuf = pStream->State.pCircBuf;
            AssertPtr(pCircBuf);

            uint8_t *pbBuf = (uint8_t *)pvBuf;
            while (cbBuf)
            {
                /* Make sure we only copy as much as the stream's FIFO can hold (SDFIFOS, 18.2.39). */
                void *pvChunk;
                size_t cbChunk;
                RTCircBufAcquireWriteBlock(pCircBuf, cbBuf, &pvChunk, &cbChunk);

                if (cbChunk)
                {
                    memcpy(pvChunk, pbBuf, cbChunk);

                    pbBuf  += cbChunk;
                    Assert(cbBuf >= cbChunk);
                    cbBuf  -= cbChunk;
                }
                else
                {
                    //AssertMsg(RTCircBufFree(pCircBuf), ("No more space but still %zu bytes to write\n", cbBuf));
                    break;
                }

                LogFunc(("[SD%RU8] cbChunk=%zu\n", pStream->u8SD, cbChunk));

                RTCircBufReleaseWriteBlock(pCircBuf, cbChunk);
            }
#  endif /* HDA_USE_DMA_ACCESS_HANDLER_WRITING */
            break;
        }

        default:
            AssertMsgFailed(("Access type not implemented\n"));
            break;
    }

    return VINF_PGM_HANDLER_DO_DEFAULT;
}
# endif /* HDA_USE_DMA_ACCESS_HANDLER */

/**
 * Soft reset of the device triggered via GCTL.
 *
 * @param   pDevIns             The device instance.
 * @param   pThis               The shared HDA device state.
 * @param   pThisCC             The ring-3 HDA device state.
 */
static void hdaR3GCTLReset(PPDMDEVINS pDevIns, PHDASTATE pThis, PHDASTATER3 pThisCC)
{
    LogFlowFuncEnter();

    pThisCC->cStreamsActive = 0;

    HDA_REG(pThis, GCAP)     = HDA_MAKE_GCAP(HDA_MAX_SDO, HDA_MAX_SDI, 0, 0, 1); /* see 6.2.1 */
    HDA_REG(pThis, VMIN)     = 0x00;                                             /* see 6.2.2 */
    HDA_REG(pThis, VMAJ)     = 0x01;                                             /* see 6.2.3 */
    HDA_REG(pThis, OUTPAY)   = 0x003C;                                           /* see 6.2.4 */
    HDA_REG(pThis, INPAY)    = 0x001D;                                           /* see 6.2.5 */
    HDA_REG(pThis, CORBSIZE) = 0x42; /* Up to 256 CORB entries                      see 6.2.1 */
    HDA_REG(pThis, RIRBSIZE) = 0x42; /* Up to 256 RIRB entries                      see 6.2.1 */
    HDA_REG(pThis, CORBRP)   = 0x0;
    HDA_REG(pThis, CORBWP)   = 0x0;
    HDA_REG(pThis, RIRBWP)   = 0x0;
    /* Some guests (like Haiku) don't set RINTCNT explicitly but expect an interrupt after each
     * RIRB response -- so initialize RINTCNT to 1 by default. */
    HDA_REG(pThis, RINTCNT)  = 0x1;

    /*
     * Stop any audio currently playing and/or recording.
     */
    pThisCC->SinkFront.pStreamShared = NULL;
    pThisCC->SinkFront.pStreamR3     = NULL;
    if (pThisCC->SinkFront.pMixSink)
        AudioMixerSinkReset(pThisCC->SinkFront.pMixSink);
# ifdef VBOX_WITH_AUDIO_HDA_MIC_IN
    pThisCC->SinkMicIn.pStreamShared = NULL;
    pThisCC->SinkMicIn.pStreamR3     = NULL;
    if (pThisCC->SinkMicIn.pMixSink)
        AudioMixerSinkReset(pThisCC->SinkMicIn.pMixSink);
# endif
    pThisCC->SinkLineIn.pStreamShared = NULL;
    pThisCC->SinkLineIn.pStreamR3     = NULL;
    if (pThisCC->SinkLineIn.pMixSink)
        AudioMixerSinkReset(pThisCC->SinkLineIn.pMixSink);
# ifdef VBOX_WITH_AUDIO_HDA_51_SURROUND
    pThisCC->SinkCenterLFE = NULL;
    if (pThisCC->SinkCenterLFE.pMixSink)
        AudioMixerSinkReset(pThisCC->SinkCenterLFE.pMixSink);
    pThisCC->SinkRear.pStreamShared = NULL;
    pThisCC->SinkRear.pStreamR3     = NULL;
    if (pThisCC->SinkRear.pMixSink)
        AudioMixerSinkReset(pThisCC->SinkRear.pMixSink);
# endif

    /*
     * Reset the codec.
     */
    if (   pThisCC->pCodec
        && pThisCC->pCodec->pfnReset)
    {
        pThisCC->pCodec->pfnReset(pThisCC->pCodec);
    }

    /*
     * Set some sensible defaults for which HDA sinks
     * are connected to which stream number.
     *
     * We use SD0 for input and SD4 for output by default.
     * These stream numbers can be changed by the guest dynamically lateron.
     */
    ASMCompilerBarrier(); /* paranoia */
# ifdef VBOX_WITH_AUDIO_HDA_MIC_IN
    hdaR3MixerControl(pDevIns, PDMAUDIOMIXERCTL_MIC_IN    , 1 /* SD0 */, 0 /* Channel */);
# endif
    hdaR3MixerControl(pDevIns, PDMAUDIOMIXERCTL_LINE_IN   , 1 /* SD0 */, 0 /* Channel */);

    hdaR3MixerControl(pDevIns, PDMAUDIOMIXERCTL_FRONT     , 5 /* SD4 */, 0 /* Channel */);
# ifdef VBOX_WITH_AUDIO_HDA_51_SURROUND
    hdaR3MixerControl(pDevIns, PDMAUDIOMIXERCTL_CENTER_LFE, 5 /* SD4 */, 0 /* Channel */);
    hdaR3MixerControl(pDevIns, PDMAUDIOMIXERCTL_REAR      , 5 /* SD4 */, 0 /* Channel */);
# endif
    ASMCompilerBarrier(); /* paranoia */

    /* Reset CORB. */
    pThis->cbCorbBuf = HDA_CORB_SIZE * HDA_CORB_ELEMENT_SIZE;
    RT_ZERO(pThis->au32CorbBuf);

    /* Reset RIRB. */
    pThis->cbRirbBuf = HDA_RIRB_SIZE * HDA_RIRB_ELEMENT_SIZE;
    RT_ZERO(pThis->au64RirbBuf);

    /* Clear our internal response interrupt counter. */
    pThis->u16RespIntCnt = 0;

    for (size_t idxStream = 0; idxStream < RT_ELEMENTS(pThis->aStreams); idxStream++)
    {
        int rc2 = hdaR3StreamEnable(&pThis->aStreams[idxStream], &pThisCC->aStreams[idxStream], false /* fEnable */);
        if (RT_SUCCESS(rc2))
        {
            /* Remove the RUN bit from SDnCTL in case the stream was in a running state before. */
            HDA_STREAM_REG(pThis, CTL, idxStream) &= ~HDA_SDCTL_RUN;
            hdaR3StreamReset(pThis, pThisCC, &pThis->aStreams[idxStream], &pThisCC->aStreams[idxStream], (uint8_t)idxStream);
        }
    }

    /* Clear stream tags <-> objects mapping table. */
    RT_ZERO(pThisCC->aTags);

    /* Emulation of codec "wake up" (HDA spec 5.5.1 and 6.5). */
    HDA_REG(pThis, STATESTS) = 0x1;

    LogFlowFuncLeave();
    LogRel(("HDA: Reset\n"));
}

#else   /* !IN_RING3 */

/**
 * Checks if a dword read starting with @a idxRegDsc is safe.
 *
 * We can guarentee it only standard reader callbacks are used.
 * @returns true if it will always succeed, false if it may return back to
 *          ring-3 or we're just not sure.
 * @param   idxRegDsc       The first register descriptor in the DWORD being read.
 */
DECLINLINE(bool) hdaIsMultiReadSafeInRZ(unsigned idxRegDsc)
{
    int32_t cbLeft = 4; /* signed on purpose */
    do
    {
        if (   g_aHdaRegMap[idxRegDsc].pfnRead == hdaRegReadU24
            || g_aHdaRegMap[idxRegDsc].pfnRead == hdaRegReadU16
            || g_aHdaRegMap[idxRegDsc].pfnRead == hdaRegReadU8
            || g_aHdaRegMap[idxRegDsc].pfnRead == hdaRegReadUnimpl)
        { /* okay */ }
        else
        {
            Log4(("hdaIsMultiReadSafeInRZ: idxRegDsc=%u %s\n", idxRegDsc, g_aHdaRegMap[idxRegDsc].abbrev));
            return false;
        }

        idxRegDsc++;
        if (idxRegDsc < RT_ELEMENTS(g_aHdaRegMap))
            cbLeft -= g_aHdaRegMap[idxRegDsc].offset - g_aHdaRegMap[idxRegDsc - 1].offset;
        else
            break;
    } while (cbLeft > 0);
    return true;
}


#endif /* !IN_RING3 */


/* MMIO callbacks */

/**
 * @callback_method_impl{FNIOMMMIONEWREAD, Looks up and calls the appropriate handler.}
 *
 * @note During implementation, we discovered so-called "forgotten" or "hole"
 *       registers whose description is not listed in the RPM, datasheet, or
 *       spec.
 */
static DECLCALLBACK(VBOXSTRICTRC) hdaMmioRead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void *pv, unsigned cb)
{
    PHDASTATE       pThis  = PDMDEVINS_2_DATA(pDevIns, PHDASTATE);
    VBOXSTRICTRC    rc;
    RT_NOREF_PV(pvUser);
    Assert(pThis->uAlignmentCheckMagic == HDASTATE_ALIGNMENT_CHECK_MAGIC);

    /*
     * Look up and log.
     */
    int             idxRegDsc = hdaRegLookup(off);    /* Register descriptor index. */
#ifdef LOG_ENABLED
    unsigned const  cbLog     = cb;
    uint32_t        offRegLog = (uint32_t)off;
#endif

    Log3Func(("off=%#x cb=%#x\n", offRegLog, cb));
    Assert(cb == 4); Assert((off & 3) == 0);

    rc = PDMDevHlpCritSectEnter(pDevIns, &pThis->CritSect, VINF_IOM_R3_MMIO_READ);
    if (rc == VINF_SUCCESS)
    {
        if (!(HDA_REG(pThis, GCTL) & HDA_GCTL_CRST) && idxRegDsc != HDA_REG_GCTL)
            LogFunc(("Access to registers except GCTL is blocked while resetting\n"));

        if (idxRegDsc >= 0)
        {
            /* ASSUMES gapless DWORD at end of map. */
            if (g_aHdaRegMap[idxRegDsc].size == 4)
            {
                /*
                 * Straight forward DWORD access.
                 */
                rc = g_aHdaRegMap[idxRegDsc].pfnRead(pDevIns, pThis, idxRegDsc, (uint32_t *)pv);
                Log3Func(("\tRead %s => %x (%Rrc)\n", g_aHdaRegMap[idxRegDsc].abbrev, *(uint32_t *)pv, VBOXSTRICTRC_VAL(rc)));
                STAM_COUNTER_INC(&pThis->aStatRegReads[idxRegDsc]);
            }
#ifndef IN_RING3
            else if (!hdaIsMultiReadSafeInRZ(idxRegDsc))

            {
                STAM_COUNTER_INC(&pThis->aStatRegReadsToR3[idxRegDsc]);
                rc = VINF_IOM_R3_MMIO_READ;
            }
#endif
            else
            {
                /*
                 * Multi register read (unless there are trailing gaps).
                 * ASSUMES that only DWORD reads have sideeffects.
                 */
                STAM_COUNTER_INC(&pThis->CTX_SUFF_Z(StatRegMultiReads));
                Log4(("hdaMmioRead: multi read: %#x LB %#x %s\n", off, cb, g_aHdaRegMap[idxRegDsc].abbrev));
                uint32_t u32Value = 0;
                unsigned cbLeft   = 4;
                do
                {
                    uint32_t const  cbReg        = g_aHdaRegMap[idxRegDsc].size;
                    uint32_t        u32Tmp       = 0;

                    rc = g_aHdaRegMap[idxRegDsc].pfnRead(pDevIns, pThis, idxRegDsc, &u32Tmp);
                    Log4Func(("\tRead %s[%db] => %x (%Rrc)*\n", g_aHdaRegMap[idxRegDsc].abbrev, cbReg, u32Tmp, VBOXSTRICTRC_VAL(rc)));
                    STAM_COUNTER_INC(&pThis->aStatRegReads[idxRegDsc]);
#ifdef IN_RING3
                    if (rc != VINF_SUCCESS)
                        break;
#else
                    AssertMsgBreak(rc == VINF_SUCCESS, ("rc=%Rrc - impossible, we sanitized the readers!\n", VBOXSTRICTRC_VAL(rc)));
#endif
                    u32Value |= (u32Tmp & g_afMasks[cbReg]) << ((4 - cbLeft) * 8);

                    cbLeft -= cbReg;
                    off    += cbReg;
                    idxRegDsc++;
                } while (cbLeft > 0 && g_aHdaRegMap[idxRegDsc].offset == off);

                if (rc == VINF_SUCCESS)
                    *(uint32_t *)pv = u32Value;
                else
                    Assert(!IOM_SUCCESS(rc));
            }
        }
        else
        {
            LogRel(("HDA: Invalid read access @0x%x (bytes=%u)\n", (uint32_t)off, cb));
            Log3Func(("\tHole at %x is accessed for read\n", offRegLog));
            STAM_COUNTER_INC(&pThis->StatRegUnknownReads);
            rc = VINF_IOM_MMIO_UNUSED_FF;
        }

        DEVHDA_UNLOCK(pDevIns, pThis);

        /*
         * Log the outcome.
         */
#ifdef LOG_ENABLED
        if (cbLog == 4)
            Log3Func(("\tReturning @%#05x -> %#010x %Rrc\n", offRegLog, *(uint32_t *)pv, VBOXSTRICTRC_VAL(rc)));
        else if (cbLog == 2)
            Log3Func(("\tReturning @%#05x -> %#06x %Rrc\n", offRegLog, *(uint16_t *)pv, VBOXSTRICTRC_VAL(rc)));
        else if (cbLog == 1)
            Log3Func(("\tReturning @%#05x -> %#04x %Rrc\n", offRegLog, *(uint8_t *)pv, VBOXSTRICTRC_VAL(rc)));
#endif
    }
    else
    {
        if (idxRegDsc >= 0)
            STAM_COUNTER_INC(&pThis->aStatRegReadsToR3[idxRegDsc]);
    }
    return rc;
}


DECLINLINE(VBOXSTRICTRC) hdaWriteReg(PPDMDEVINS pDevIns, PHDASTATE pThis, int idxRegDsc, uint32_t u32Value, char const *pszLog)
{
    DEVHDA_LOCK_RETURN(pDevIns, pThis, VINF_IOM_R3_MMIO_WRITE);

    if (!(HDA_REG(pThis, GCTL) & HDA_GCTL_CRST) && idxRegDsc != HDA_REG_GCTL)
    {
        Log(("hdaWriteReg: Warning: Access to %s is blocked while controller is in reset mode\n", g_aHdaRegMap[idxRegDsc].abbrev));
        LogRel2(("HDA: Warning: Access to register %s is blocked while controller is in reset mode\n",
                 g_aHdaRegMap[idxRegDsc].abbrev));
        STAM_COUNTER_INC(&pThis->StatRegWritesBlockedByReset);

        DEVHDA_UNLOCK(pDevIns, pThis);
        return VINF_SUCCESS;
    }

    /*
     * Handle RD (register description) flags.
     */

    /* For SDI / SDO: Check if writes to those registers are allowed while SDCTL's RUN bit is set. */
    if (idxRegDsc >= HDA_NUM_GENERAL_REGS)
    {
        const uint32_t uSDCTL = HDA_STREAM_REG(pThis, CTL, HDA_SD_NUM_FROM_REG(pThis, CTL, idxRegDsc));

        /*
         * Some OSes (like Win 10 AU) violate the spec by writing stuff to registers which are not supposed to be be touched
         * while SDCTL's RUN bit is set. So just ignore those values.
         */

        /* Is the RUN bit currently set? */
        if (   RT_BOOL(uSDCTL & HDA_SDCTL_RUN)
            /* Are writes to the register denied if RUN bit is set? */
            && !(g_aHdaRegMap[idxRegDsc].fFlags & HDA_RD_F_SD_WRITE_RUN))
        {
            Log(("hdaWriteReg: Warning: Access to %s is blocked! %R[sdctl]\n", g_aHdaRegMap[idxRegDsc].abbrev, uSDCTL));
            LogRel2(("HDA: Warning: Access to register %s is blocked while the stream's RUN bit is set\n",
                     g_aHdaRegMap[idxRegDsc].abbrev));
            STAM_COUNTER_INC(&pThis->StatRegWritesBlockedByRun);

            DEVHDA_UNLOCK(pDevIns, pThis);
            return VINF_SUCCESS;
        }
    }

#ifdef LOG_ENABLED
    uint32_t const idxRegMem   = g_aHdaRegMap[idxRegDsc].mem_idx;
    uint32_t const u32OldValue = pThis->au32Regs[idxRegMem];
#endif
    VBOXSTRICTRC rc = g_aHdaRegMap[idxRegDsc].pfnWrite(pDevIns, pThis, idxRegDsc, u32Value);
    Log3Func(("Written value %#x to %s[%d byte]; %x => %x%s, rc=%d\n", u32Value, g_aHdaRegMap[idxRegDsc].abbrev,
              g_aHdaRegMap[idxRegDsc].size, u32OldValue, pThis->au32Regs[idxRegMem], pszLog, VBOXSTRICTRC_VAL(rc)));
#ifndef IN_RING3
    if (rc == VINF_IOM_R3_MMIO_WRITE)
        STAM_COUNTER_INC(&pThis->aStatRegWritesToR3[idxRegDsc]);
    else
#endif
        STAM_COUNTER_INC(&pThis->aStatRegWrites[idxRegDsc]);

    DEVHDA_UNLOCK(pDevIns, pThis);
    RT_NOREF(pszLog);
    return rc;
}


/**
 * @callback_method_impl{FNIOMMMIONEWWRITE,
 *      Looks up and calls the appropriate handler.}
 */
static DECLCALLBACK(VBOXSTRICTRC) hdaMmioWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void const *pv, unsigned cb)
{
    PHDASTATE pThis  = PDMDEVINS_2_DATA(pDevIns, PHDASTATE);
    RT_NOREF_PV(pvUser);
    Assert(pThis->uAlignmentCheckMagic == HDASTATE_ALIGNMENT_CHECK_MAGIC);

    /*
     * Look up and log the access.
     */
    int         idxRegDsc = hdaRegLookup(off);
#if defined(IN_RING3) || defined(LOG_ENABLED)
    uint32_t    idxRegMem = idxRegDsc != -1 ? g_aHdaRegMap[idxRegDsc].mem_idx : UINT32_MAX;
#endif
    uint64_t    u64Value;
    if (cb == 4)        u64Value = *(uint32_t const *)pv;
    else if (cb == 2)   u64Value = *(uint16_t const *)pv;
    else if (cb == 1)   u64Value = *(uint8_t const *)pv;
    else if (cb == 8)   u64Value = *(uint64_t const *)pv;
    else
        ASSERT_GUEST_MSG_FAILED_RETURN(("cb=%u %.*Rhxs\n", cb, cb, pv),
                                       PDMDevHlpDBGFStop(pDevIns, RT_SRC_POS, "odd write size: off=%RGp cb=%u\n", off, cb));

    /*
     * The behavior of accesses that aren't aligned on natural boundraries is
     * undefined. Just reject them outright.
     */
    ASSERT_GUEST_MSG_RETURN((off & (cb - 1)) == 0, ("off=%RGp cb=%u %.*Rhxs\n", off, cb, cb, pv),
                            PDMDevHlpDBGFStop(pDevIns, RT_SRC_POS, "misaligned write access: off=%RGp cb=%u\n", off, cb));

#ifdef LOG_ENABLED
    uint32_t const u32LogOldValue = idxRegDsc >= 0 ? pThis->au32Regs[idxRegMem] : UINT32_MAX;
#endif

    /*
     * Try for a direct hit first.
     */
    VBOXSTRICTRC rc;
    if (idxRegDsc >= 0 && g_aHdaRegMap[idxRegDsc].size == cb)
    {
        Log3Func(("@%#05x u%u=%#0*RX64 %s\n", (uint32_t)off, cb * 8, 2 + cb * 2, u64Value, g_aHdaRegMap[idxRegDsc].abbrev));
        rc = hdaWriteReg(pDevIns, pThis, idxRegDsc, u64Value, "");
        Log3Func(("\t%#x -> %#x\n", u32LogOldValue, idxRegMem != UINT32_MAX ? pThis->au32Regs[idxRegMem] : UINT32_MAX));
    }
    /*
     * Sub-register access.  Supply missing bits as needed.
     */
    else if (   idxRegDsc >= 0
             && cb < g_aHdaRegMap[idxRegDsc].size)
    {
        u64Value |=   pThis->au32Regs[g_aHdaRegMap[idxRegDsc].mem_idx]
                    & g_afMasks[g_aHdaRegMap[idxRegDsc].size]
                    & ~g_afMasks[cb];
        Log4Func(("@%#05x u%u=%#0*RX64 cb=%#x cbReg=%x %s\n"
                  "\tSupplying missing bits (%#x): %#llx -> %#llx ...\n",
                  (uint32_t)off, cb * 8, 2 + cb * 2, u64Value, cb, g_aHdaRegMap[idxRegDsc].size, g_aHdaRegMap[idxRegDsc].abbrev,
                  g_afMasks[g_aHdaRegMap[idxRegDsc].size] & ~g_afMasks[cb], u64Value & g_afMasks[cb], u64Value));
        rc = hdaWriteReg(pDevIns, pThis, idxRegDsc, u64Value, "");
        Log4Func(("\t%#x -> %#x\n", u32LogOldValue, idxRegMem != UINT32_MAX ? pThis->au32Regs[idxRegMem] : UINT32_MAX));
        STAM_COUNTER_INC(&pThis->CTX_SUFF_Z(StatRegSubWrite));
    }
    /*
     * Partial or multiple register access, loop thru the requested memory.
     */
    else
    {
#ifdef IN_RING3
        if (idxRegDsc == -1)
            Log4Func(("@%#05x u32=%#010x cb=%d\n", (uint32_t)off, *(uint32_t const *)pv, cb));
        else if (g_aHdaRegMap[idxRegDsc].size == cb)
            Log4Func(("@%#05x u%u=%#0*RX64 %s\n", (uint32_t)off, cb * 8, 2 + cb * 2, u64Value, g_aHdaRegMap[idxRegDsc].abbrev));
        else
            Log4Func(("@%#05x u%u=%#0*RX64 %s - mismatch cbReg=%u\n", (uint32_t)off, cb * 8, 2 + cb * 2, u64Value,
                      g_aHdaRegMap[idxRegDsc].abbrev, g_aHdaRegMap[idxRegDsc].size));

        /*
         * If it's an access beyond the start of the register, shift the input
         * value and fill in missing bits. Natural alignment rules means we
         * will only see 1 or 2 byte accesses of this kind, so no risk of
         * shifting out input values.
         */
        if (idxRegDsc < 0)
        {
            idxRegDsc = hdaR3RegLookupWithin(off);
            if (idxRegDsc != -1)
            {
                uint32_t const cbBefore = (uint32_t)off - g_aHdaRegMap[idxRegDsc].offset;
                Assert(cbBefore > 0 && cbBefore < 4);
                off      -= cbBefore;
                idxRegMem = g_aHdaRegMap[idxRegDsc].mem_idx;
                u64Value <<= cbBefore * 8;
                u64Value  |= pThis->au32Regs[idxRegMem] & g_afMasks[cbBefore];
                Log4Func(("\tWithin register, supplied %u leading bits: %#llx -> %#llx ...\n",
                          cbBefore * 8, ~g_afMasks[cbBefore] & u64Value, u64Value));
                STAM_COUNTER_INC(&pThis->CTX_SUFF_Z(StatRegMultiWrites));
            }
            else
                STAM_COUNTER_INC(&pThis->StatRegUnknownWrites);
        }
        else
        {
            Log4(("hdaMmioWrite: multi write: %s\n", g_aHdaRegMap[idxRegDsc].abbrev));
            STAM_COUNTER_INC(&pThis->CTX_SUFF_Z(StatRegMultiWrites));
        }

        /* Loop thru the write area, it may cover multiple registers. */
        rc = VINF_SUCCESS;
        for (;;)
        {
            uint32_t cbReg;
            if (idxRegDsc >= 0)
            {
                idxRegMem = g_aHdaRegMap[idxRegDsc].mem_idx;
                cbReg     = g_aHdaRegMap[idxRegDsc].size;
                if (cb < cbReg)
                {
                    u64Value |= pThis->au32Regs[idxRegMem] & g_afMasks[cbReg] & ~g_afMasks[cb];
                    Log4Func(("\tSupplying missing bits (%#x): %#llx -> %#llx ...\n",
                              g_afMasks[cbReg] & ~g_afMasks[cb], u64Value & g_afMasks[cb], u64Value));
                }
# ifdef LOG_ENABLED
                uint32_t uLogOldVal = pThis->au32Regs[idxRegMem];
# endif
                rc = hdaWriteReg(pDevIns, pThis, idxRegDsc, u64Value & g_afMasks[cbReg], "*");
                Log4Func(("\t%#x -> %#x\n", uLogOldVal, pThis->au32Regs[idxRegMem]));
            }
            else
            {
                LogRel(("HDA: Invalid write access @0x%x\n", (uint32_t)off));
                cbReg = 1;
            }
            if (rc != VINF_SUCCESS)
                break;
            if (cbReg >= cb)
                break;

            /* Advance. */
            off += cbReg;
            cb  -= cbReg;
            u64Value >>= cbReg * 8;
            if (idxRegDsc == -1)
                idxRegDsc = hdaRegLookup(off);
            else
            {
                idxRegDsc++;
                if (   (unsigned)idxRegDsc >= RT_ELEMENTS(g_aHdaRegMap)
                    || g_aHdaRegMap[idxRegDsc].offset != off)
                    idxRegDsc = -1;
            }
        }

#else  /* !IN_RING3 */
        /* Take the simple way out. */
        rc = VINF_IOM_R3_MMIO_WRITE;
#endif /* !IN_RING3 */
    }

    return rc;
}

#ifdef IN_RING3


/*********************************************************************************************************************************
*   Saved state                                                                                                                  *
*********************************************************************************************************************************/

/**
 * @callback_method_impl{FNSSMFIELDGETPUT,
 * Version 6 saves the IOC flag in HDABDLEDESC::fFlags as a bool}
 */
static DECLCALLBACK(int)
hdaR3GetPutTrans_HDABDLEDESC_fFlags_6(PSSMHANDLE pSSM, const struct SSMFIELD *pField, void *pvStruct,
                                      uint32_t fFlags, bool fGetOrPut, void *pvUser)
{
    PPDMDEVINS pDevIns = (PPDMDEVINS)pvUser;
    RT_NOREF(pSSM, pField, pvStruct, fFlags);
    AssertReturn(fGetOrPut, VERR_INTERNAL_ERROR_4);
    bool fIoc;
    int rc = pDevIns->pHlpR3->pfnSSMGetBool(pSSM, &fIoc);
    if (RT_SUCCESS(rc))
    {
        PHDABDLEDESC pDesc = (PHDABDLEDESC)pvStruct;
        pDesc->fFlags = fIoc ? HDA_BDLE_F_IOC : 0;
    }
    return rc;
}


/**
 * @callback_method_impl{FNSSMFIELDGETPUT,
 * Versions 1 thru 4 save the IOC flag in HDASTREAMSTATE::DescfFlags as a bool}
 */
static DECLCALLBACK(int)
hdaR3GetPutTrans_HDABDLE_Desc_fFlags_1thru4(PSSMHANDLE pSSM, const struct SSMFIELD *pField, void *pvStruct,
                                            uint32_t fFlags, bool fGetOrPut, void *pvUser)
{
    PPDMDEVINS pDevIns = (PPDMDEVINS)pvUser;
    RT_NOREF(pSSM, pField, pvStruct, fFlags);
    AssertReturn(fGetOrPut, VERR_INTERNAL_ERROR_4);
    bool fIoc;
    int rc = pDevIns->pHlpR3->pfnSSMGetBool(pSSM, &fIoc);
    if (RT_SUCCESS(rc))
    {
        PHDABDLE pState = (PHDABDLE)pvStruct;
        pState->Desc.fFlags = fIoc ? HDA_BDLE_F_IOC : 0;
    }
    return rc;
}


static int hdaR3SaveStream(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, PHDASTREAM pStreamShared, PHDASTREAMR3 pStreamR3)
{
    PCPDMDEVHLPR3 pHlp = pDevIns->pHlpR3;
# ifdef LOG_ENABLED
    PHDASTATE pThis = PDMDEVINS_2_DATA(pDevIns, PHDASTATE);
# endif

    Log2Func(("[SD%RU8]\n", pStreamShared->u8SD));

    /* Save stream ID. */
    Assert(pStreamShared->u8SD < HDA_MAX_STREAMS);
    int rc = pHlp->pfnSSMPutU8(pSSM, pStreamShared->u8SD);
    AssertRCReturn(rc, rc);

    rc = pHlp->pfnSSMPutStructEx(pSSM, &pStreamShared->State, sizeof(pStreamShared->State),
                                 0 /*fFlags*/, g_aSSMStreamStateFields7, NULL);
    AssertRCReturn(rc, rc);

    rc = pHlp->pfnSSMPutStructEx(pSSM, &pStreamShared->State.BDLE.Desc, sizeof(pStreamShared->State.BDLE.Desc),
                                 0 /*fFlags*/, g_aSSMBDLEDescFields7, NULL);
    AssertRCReturn(rc, rc);

    rc = pHlp->pfnSSMPutStructEx(pSSM, &pStreamShared->State.BDLE.State, sizeof(pStreamShared->State.BDLE.State),
                                 0 /*fFlags*/,  g_aSSMBDLEStateFields7, NULL);
    AssertRCReturn(rc, rc);

    rc = pHlp->pfnSSMPutStructEx(pSSM, &pStreamShared->State.Period, sizeof(pStreamShared->State.Period),
                                 0 /* fFlags */, g_aSSMStreamPeriodFields7, NULL);
    AssertRCReturn(rc, rc);

    uint32_t cbCircBufSize = 0;
    uint32_t cbCircBufUsed = 0;

    if (pStreamR3->State.pCircBuf)
    {
        cbCircBufSize = (uint32_t)RTCircBufSize(pStreamR3->State.pCircBuf);
        cbCircBufUsed = (uint32_t)RTCircBufUsed(pStreamR3->State.pCircBuf);
    }

    rc = pHlp->pfnSSMPutU32(pSSM, cbCircBufSize);
    AssertRCReturn(rc, rc);

    rc = pHlp->pfnSSMPutU32(pSSM, cbCircBufUsed);
    AssertRCReturn(rc, rc);

    if (cbCircBufUsed)
    {
        /*
         * We now need to get the circular buffer's data without actually modifying
         * the internal read / used offsets -- otherwise we would end up with broken audio
         * data after saving the state.
         *
         * So get the current read offset and serialize the buffer data manually based on that.
         */
        size_t const offBuf = RTCircBufOffsetRead(pStreamR3->State.pCircBuf);
        void  *pvBuf;
        size_t cbBuf;
        RTCircBufAcquireReadBlock(pStreamR3->State.pCircBuf, cbCircBufUsed, &pvBuf, &cbBuf);
        Assert(cbBuf);
        if (cbBuf)
        {
            rc = pHlp->pfnSSMPutMem(pSSM, pvBuf, cbBuf);
            AssertRC(rc);
            if (   RT_SUCCESS(rc)
                && cbBuf < cbCircBufUsed)
            {
                rc = pHlp->pfnSSMPutMem(pSSM, (uint8_t *)pvBuf - offBuf, cbCircBufUsed - cbBuf);
            }
        }
        RTCircBufReleaseReadBlock(pStreamR3->State.pCircBuf, 0 /* Don't advance read pointer -- see comment above */);
    }

    Log2Func(("[SD%RU8] LPIB=%RU32, CBL=%RU32, LVI=%RU32\n", pStreamR3->u8SD, HDA_STREAM_REG(pThis, LPIB, pStreamShared->u8SD),
              HDA_STREAM_REG(pThis, CBL, pStreamShared->u8SD), HDA_STREAM_REG(pThis, LVI, pStreamShared->u8SD)));

#ifdef LOG_ENABLED
    hdaR3BDLEDumpAll(pDevIns, pThis, pStreamShared->u64BDLBase, pStreamShared->u16LVI + 1);
#endif

    return rc;
}

/**
 * @callback_method_impl{FNSSMDEVSAVEEXEC}
 */
static DECLCALLBACK(int) hdaR3SaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PHDASTATE     pThis   = PDMDEVINS_2_DATA(pDevIns, PHDASTATE);
    PHDASTATER3   pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PHDASTATER3);
    PCPDMDEVHLPR3 pHlp    = pDevIns->pHlpR3;

    /* Save Codec nodes states. */
    hdaCodecSaveState(pDevIns, pThisCC->pCodec, pSSM);

    /* Save MMIO registers. */
    pHlp->pfnSSMPutU32(pSSM, RT_ELEMENTS(pThis->au32Regs));
    pHlp->pfnSSMPutMem(pSSM, pThis->au32Regs, sizeof(pThis->au32Regs));

    /* Save controller-specifc internals. */
    pHlp->pfnSSMPutU64(pSSM, pThis->u64WalClk);
    pHlp->pfnSSMPutU8(pSSM, pThis->u8IRQL);

    /* Save number of streams. */
    pHlp->pfnSSMPutU32(pSSM, HDA_MAX_STREAMS);

    /* Save stream states. */
    for (uint8_t i = 0; i < HDA_MAX_STREAMS; i++)
    {
        int rc = hdaR3SaveStream(pDevIns, pSSM, &pThis->aStreams[i], &pThisCC->aStreams[i]);
        AssertRCReturn(rc, rc);
    }

    return VINF_SUCCESS;
}

/**
 * Does required post processing when loading a saved state.
 *
 * @param   pDevIns             The device instance.
 * @param   pThis               Pointer to the shared HDA state.
 * @param   pThisCC             Pointer to the ring-3 HDA state.
 */
static int hdaR3LoadExecPost(PPDMDEVINS pDevIns, PHDASTATE pThis, PHDASTATER3 pThisCC)
{
    int rc = VINF_SUCCESS; /** @todo r=bird: Really, what's the point of this? */

    /*
     * Enable all previously active streams.
     */
    for (size_t i = 0; i < HDA_MAX_STREAMS; i++)
    {
        PHDASTREAM pStreamShared = &pThis->aStreams[i];

        bool fActive = RT_BOOL(HDA_STREAM_REG(pThis, CTL, i) & HDA_SDCTL_RUN);
        if (fActive)
        {
            PHDASTREAMR3 pStreamR3 = &pThisCC->aStreams[i];

            int rc2;
#ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
            /* Make sure to also create the async I/O thread before actually enabling the stream. */
            rc2 = hdaR3StreamAsyncIOCreate(pStreamR3);
            AssertRC(rc2);

            /* ... and enabling it. */
            hdaR3StreamAsyncIOEnable(pStreamR3, true /* fEnable */);
#endif
            /* Resume the stream's period. */
            hdaR3StreamPeriodResume(&pStreamShared->State.Period);

            /* (Re-)enable the stream. */
            rc2 = hdaR3StreamEnable(pStreamShared, pStreamR3, true /* fEnable */);
            AssertRC(rc2);

            /* Add the stream to the device setup. */
            rc2 = hdaR3AddStream(pThisCC, &pStreamShared->State.Cfg);
            AssertRC(rc2);

#ifdef HDA_USE_DMA_ACCESS_HANDLER
            /* (Re-)install the DMA handler. */
            hdaR3StreamRegisterDMAHandlers(pThis, pStreamShared);
#endif
            if (hdaR3StreamTransferIsScheduled(pStreamShared, PDMDevHlpTimerGet(pDevIns, pStreamShared->hTimer)))
                hdaR3TimerSet(pDevIns, pStreamShared, hdaR3StreamTransferGetNext(pStreamShared), true /*fForce*/, 0 /*tsNow*/);

            /* Also keep track of the currently active streams. */
            pThisCC->cStreamsActive++;
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}


/**
 * Handles loading of all saved state versions older than the current one.
 *
 * @param   pDevIns     The device instance.
 * @param   pThis       Pointer to the shared HDA state.
 * @param   pThisCC     Pointer to the ring-3 HDA state.
 * @param   pSSM        Pointer to SSM handle.
 * @param   uVersion    Saved state version to load.
 */
static int hdaR3LoadExecLegacy(PPDMDEVINS pDevIns, PHDASTATE pThis, PHDASTATER3 pThisCC, PSSMHANDLE pSSM, uint32_t uVersion)
{
    PCPDMDEVHLPR3 pHlp = pDevIns->pHlpR3;
    int           rc;

    /*
     * Load MMIO registers.
     */
    uint32_t cRegs;
    switch (uVersion)
    {
        case HDA_SAVED_STATE_VERSION_1:
            /* Starting with r71199, we would save 112 instead of 113
               registers due to some code cleanups.  This only affected trunk
               builds in the 4.1 development period. */
            cRegs = 113;
            if (pHlp->pfnSSMHandleRevision(pSSM) >= 71199)
            {
                uint32_t uVer = pHlp->pfnSSMHandleVersion(pSSM);
                if (   VBOX_FULL_VERSION_GET_MAJOR(uVer) == 4
                    && VBOX_FULL_VERSION_GET_MINOR(uVer) == 0
                    && VBOX_FULL_VERSION_GET_BUILD(uVer) >= 51)
                    cRegs = 112;
            }
            break;

        case HDA_SAVED_STATE_VERSION_2:
        case HDA_SAVED_STATE_VERSION_3:
            cRegs = 112;
            AssertCompile(RT_ELEMENTS(pThis->au32Regs) >= 112);
            break;

        /* Since version 4 we store the register count to stay flexible. */
        case HDA_SAVED_STATE_VERSION_4:
        case HDA_SAVED_STATE_VERSION_5:
        case HDA_SAVED_STATE_VERSION_6:
            rc = pHlp->pfnSSMGetU32(pSSM, &cRegs);
            AssertRCReturn(rc, rc);
            if (cRegs != RT_ELEMENTS(pThis->au32Regs))
                LogRel(("HDA: SSM version cRegs is %RU32, expected %RU32\n", cRegs, RT_ELEMENTS(pThis->au32Regs)));
            break;

        default:
            AssertLogRelMsgFailedReturn(("HDA: Internal Error! Didn't expect saved state version %RU32 ending up in hdaR3LoadExecLegacy!\n",
                                         uVersion), VERR_INTERNAL_ERROR_5);
    }

    if (cRegs >= RT_ELEMENTS(pThis->au32Regs))
    {
        pHlp->pfnSSMGetMem(pSSM, pThis->au32Regs, sizeof(pThis->au32Regs));
        pHlp->pfnSSMSkip(pSSM, sizeof(uint32_t) * (cRegs - RT_ELEMENTS(pThis->au32Regs)));
    }
    else
        pHlp->pfnSSMGetMem(pSSM, pThis->au32Regs, sizeof(uint32_t) * cRegs);

    /* Make sure to update the base addresses first before initializing any streams down below. */
    pThis->u64CORBBase  = RT_MAKE_U64(HDA_REG(pThis, CORBLBASE), HDA_REG(pThis, CORBUBASE));
    pThis->u64RIRBBase  = RT_MAKE_U64(HDA_REG(pThis, RIRBLBASE), HDA_REG(pThis, RIRBUBASE));
    pThis->u64DPBase    = RT_MAKE_U64(HDA_REG(pThis, DPLBASE) & DPBASE_ADDR_MASK, HDA_REG(pThis, DPUBASE));

    /* Also make sure to update the DMA position bit if this was enabled when saving the state. */
    pThis->fDMAPosition = RT_BOOL(HDA_REG(pThis, DPLBASE) & RT_BIT_32(0));

    /*
     * Load BDLEs (Buffer Descriptor List Entries) and DMA counters.
     *
     * Note: Saved states < v5 store LVI (u32BdleMaxCvi) for
     *       *every* BDLE state, whereas it only needs to be stored
     *       *once* for every stream. Most of the BDLE state we can
     *       get out of the registers anyway, so just ignore those values.
     *
     *       Also, only the current BDLE was saved, regardless whether
     *       there were more than one (and there are at least two entries,
     *       according to the spec).
     */
    switch (uVersion)
    {
        case HDA_SAVED_STATE_VERSION_1:
        case HDA_SAVED_STATE_VERSION_2:
        case HDA_SAVED_STATE_VERSION_3:
        case HDA_SAVED_STATE_VERSION_4:
        {
            /* Only load the internal states.
             * The rest will be initialized from the saved registers later. */

            /* Note 1: Only the *current* BDLE for a stream was saved! */
            /* Note 2: The stream's saving order is/was fixed, so don't touch! */

            /* Output */
            PHDASTREAM pStreamShared = &pThis->aStreams[4];
            rc = hdaR3StreamSetUp(pDevIns, pThis, pStreamShared, &pThisCC->aStreams[4], 4 /* Stream descriptor, hardcoded */);
            AssertRCReturn(rc, rc);
            rc = pHlp->pfnSSMGetStructEx(pSSM, &pStreamShared->State.BDLE, sizeof(pStreamShared->State.BDLE),
                                         0 /* fFlags */, g_aSSMStreamBdleFields1234, pDevIns);
            AssertRCReturn(rc, rc);
            pStreamShared->State.uCurBDLE = pStreamShared->State.BDLE.State.u32BDLIndex;

            /* Microphone-In */
            pStreamShared = &pThis->aStreams[2];
            rc = hdaR3StreamSetUp(pDevIns, pThis, pStreamShared, &pThisCC->aStreams[2], 2 /* Stream descriptor, hardcoded */);
            AssertRCReturn(rc, rc);
            rc = pHlp->pfnSSMGetStructEx(pSSM, &pStreamShared->State.BDLE, sizeof(pStreamShared->State.BDLE),
                                         0 /* fFlags */, g_aSSMStreamBdleFields1234, pDevIns);
            AssertRCReturn(rc, rc);
            pStreamShared->State.uCurBDLE = pStreamShared->State.BDLE.State.u32BDLIndex;

            /* Line-In */
            pStreamShared = &pThis->aStreams[0];
            rc = hdaR3StreamSetUp(pDevIns, pThis, pStreamShared, &pThisCC->aStreams[0], 0 /* Stream descriptor, hardcoded */);
            AssertRCReturn(rc, rc);
            rc = pHlp->pfnSSMGetStructEx(pSSM, &pStreamShared->State.BDLE, sizeof(pStreamShared->State.BDLE),
                                         0 /* fFlags */, g_aSSMStreamBdleFields1234, pDevIns);
            AssertRCReturn(rc, rc);
            pStreamShared->State.uCurBDLE = pStreamShared->State.BDLE.State.u32BDLIndex;
            break;
        }

        /*
         * v5 & v6 - Since v5 we support flexible stream and BDLE counts.
         */
        default:
        {
            /* Stream count. */
            uint32_t cStreams;
            rc = pHlp->pfnSSMGetU32(pSSM, &cStreams);
            AssertRCReturn(rc, rc);
            if (cStreams > HDA_MAX_STREAMS)
                return pHlp->pfnSSMSetLoadError(pSSM, VERR_SSM_DATA_UNIT_FORMAT_CHANGED, RT_SRC_POS,
                                                N_("State contains %u streams while %u is the maximum supported"),
                                                cStreams, HDA_MAX_STREAMS);

            /* Load stream states. */
            for (uint32_t i = 0; i < cStreams; i++)
            {
                uint8_t idStream;
                rc = pHlp->pfnSSMGetU8(pSSM, &idStream);
                AssertRCReturn(rc, rc);

                HDASTREAM    StreamDummyShared;
                HDASTREAMR3  StreamDummyR3;
                PHDASTREAM   pStreamShared = idStream < RT_ELEMENTS(pThis->aStreams) ? &pThis->aStreams[idStream] : &StreamDummyShared;
                PHDASTREAMR3 pStreamR3     = idStream < RT_ELEMENTS(pThisCC->aStreams) ? &pThisCC->aStreams[idStream] : &StreamDummyR3;
                AssertLogRelMsgStmt(idStream < RT_ELEMENTS(pThisCC->aStreams),
                                    ("HDA stream ID=%RU8 not supported, skipping loadingit ...\n", idStream),
                                    RT_ZERO(StreamDummyShared); RT_ZERO(StreamDummyR3));

                rc = hdaR3StreamSetUp(pDevIns, pThis, pStreamShared, pStreamR3, idStream);
                if (RT_FAILURE(rc))
                {
                    LogRel(("HDA: Stream #%RU32: Setting up of stream %RU8 failed, rc=%Rrc\n", i, idStream, rc));
                    break;
                }

                /*
                 * Load BDLEs (Buffer Descriptor List Entries) and DMA counters.
                 */
                if (uVersion == HDA_SAVED_STATE_VERSION_5)
                {
                    struct V5HDASTREAMSTATE /* HDASTREAMSTATE + HDABDLE */
                    {
                        uint16_t cBLDEs;
                        uint16_t uCurBDLE;
                        uint32_t u32BDLEIndex;
                        uint32_t cbBelowFIFOW;
                        uint32_t u32BufOff;
                    } Tmp;
                    static SSMFIELD const g_aV5State1Fields[] =
                    {
                        SSMFIELD_ENTRY(V5HDASTREAMSTATE, cBLDEs),
                        SSMFIELD_ENTRY(V5HDASTREAMSTATE, uCurBDLE),
                        SSMFIELD_ENTRY_TERM()
                    };
                    rc = pHlp->pfnSSMGetStructEx(pSSM, &Tmp, sizeof(Tmp), 0 /* fFlags */, g_aV5State1Fields, NULL);
                    AssertRCReturn(rc, rc);
                    pStreamShared->State.uCurBDLE = Tmp.uCurBDLE;

                    for (uint16_t a = 0; a < Tmp.cBLDEs; a++)
                    {
                        static SSMFIELD const g_aV5State2Fields[] =
                        {
                            SSMFIELD_ENTRY(V5HDASTREAMSTATE, u32BDLEIndex),
                            SSMFIELD_ENTRY_OLD(au8FIFO, 256),
                            SSMFIELD_ENTRY(V5HDASTREAMSTATE, cbBelowFIFOW),
                            SSMFIELD_ENTRY_TERM()
                        };
                        rc = pHlp->pfnSSMGetStructEx(pSSM, &Tmp, sizeof(Tmp), 0 /* fFlags */, g_aV5State2Fields, NULL);
                        AssertRCReturn(rc, rc);
                        if (Tmp.u32BDLEIndex == pStreamShared->State.uCurBDLE)
                        {
                            pStreamShared->State.BDLE.State.cbBelowFIFOW = Tmp.cbBelowFIFOW;
                            pStreamShared->State.BDLE.State.u32BufOff    = Tmp.u32BufOff;
                        }
                    }
                }
                else
                {
                    rc = pHlp->pfnSSMGetStructEx(pSSM, &pStreamShared->State, sizeof(HDASTREAMSTATE),
                                                 0 /* fFlags */, g_aSSMStreamStateFields6, NULL);
                    AssertRCReturn(rc, rc);

                    rc = pHlp->pfnSSMGetStructEx(pSSM, &pStreamShared->State.BDLE.Desc, sizeof(pStreamShared->State.BDLE.Desc),
                                                 0 /* fFlags */, g_aSSMBDLEDescFields6, pDevIns);
                    AssertRCReturn(rc, rc);

                    rc = pHlp->pfnSSMGetStructEx(pSSM, &pStreamShared->State.BDLE.State, sizeof(HDABDLESTATE),
                                                 0 /* fFlags */, g_aSSMBDLEStateFields6, NULL);
                    AssertRCReturn(rc, rc);

                    Log2Func(("[SD%RU8] LPIB=%RU32, CBL=%RU32, LVI=%RU32\n", idStream, HDA_STREAM_REG(pThis, LPIB, idStream),
                              HDA_STREAM_REG(pThis, CBL, idStream), HDA_STREAM_REG(pThis, LVI, idStream)));
#ifdef LOG_ENABLED
                    hdaR3BDLEDumpAll(pDevIns, pThis, pStreamShared->u64BDLBase, pStreamShared->u16LVI + 1);
#endif
                }

            } /* for cStreams */
            break;
        } /* default */
    }

    return rc;
}

/**
 * @callback_method_impl{FNSSMDEVLOADEXEC}
 */
static DECLCALLBACK(int) hdaR3LoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    PHDASTATE     pThis   = PDMDEVINS_2_DATA(pDevIns, PHDASTATE);
    PHDASTATER3   pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PHDASTATER3);
    PCPDMDEVHLPR3 pHlp    = pDevIns->pHlpR3;

    Assert(uPass == SSM_PASS_FINAL); NOREF(uPass);

    LogRel2(("hdaR3LoadExec: uVersion=%RU32, uPass=0x%x\n", uVersion, uPass));

    /*
     * Load Codec nodes states.
     */
    int rc = hdaCodecLoadState(pDevIns, pThisCC->pCodec, pSSM, uVersion);
    if (RT_FAILURE(rc))
    {
        LogRel(("HDA: Failed loading codec state (version %RU32, pass 0x%x), rc=%Rrc\n", uVersion, uPass, rc));
        return rc;
    }

    if (uVersion <= HDA_SAVED_STATE_VERSION_6) /* Handle older saved states? */
    {
        rc = hdaR3LoadExecLegacy(pDevIns, pThis, pThisCC, pSSM, uVersion);
        if (RT_SUCCESS(rc))
            rc = hdaR3LoadExecPost(pDevIns, pThis, pThisCC);
        return rc;
    }

    /*
     * Load MMIO registers.
     */
    uint32_t cRegs;
    rc = pHlp->pfnSSMGetU32(pSSM, &cRegs); AssertRCReturn(rc, rc);
    AssertRCReturn(rc, rc);
    if (cRegs != RT_ELEMENTS(pThis->au32Regs))
        LogRel(("HDA: SSM version cRegs is %RU32, expected %RU32\n", cRegs, RT_ELEMENTS(pThis->au32Regs)));

    if (cRegs >= RT_ELEMENTS(pThis->au32Regs))
    {
        pHlp->pfnSSMGetMem(pSSM, pThis->au32Regs, sizeof(pThis->au32Regs));
        rc = pHlp->pfnSSMSkip(pSSM, sizeof(uint32_t) * (cRegs - RT_ELEMENTS(pThis->au32Regs)));
        AssertRCReturn(rc, rc);
    }
    else
    {
        rc = pHlp->pfnSSMGetMem(pSSM, pThis->au32Regs, sizeof(uint32_t) * cRegs);
        AssertRCReturn(rc, rc);
    }

    /* Make sure to update the base addresses first before initializing any streams down below. */
    pThis->u64CORBBase  = RT_MAKE_U64(HDA_REG(pThis, CORBLBASE), HDA_REG(pThis, CORBUBASE));
    pThis->u64RIRBBase  = RT_MAKE_U64(HDA_REG(pThis, RIRBLBASE), HDA_REG(pThis, RIRBUBASE));
    pThis->u64DPBase    = RT_MAKE_U64(HDA_REG(pThis, DPLBASE) & DPBASE_ADDR_MASK, HDA_REG(pThis, DPUBASE));

    /* Also make sure to update the DMA position bit if this was enabled when saving the state. */
    pThis->fDMAPosition = RT_BOOL(HDA_REG(pThis, DPLBASE) & RT_BIT_32(0));

    /*
     * Load controller-specifc internals.
     * Don't annoy other team mates (forgot this for state v7).
     */
    if (   pHlp->pfnSSMHandleRevision(pSSM) >= 116273
        || pHlp->pfnSSMHandleVersion(pSSM)  >= VBOX_FULL_VERSION_MAKE(5, 2, 0))
    {
        pHlp->pfnSSMGetU64(pSSM, &pThis->u64WalClk);
        rc = pHlp->pfnSSMGetU8(pSSM, &pThis->u8IRQL);
        AssertRCReturn(rc, rc);
    }

    /*
     * Load streams.
     */
    uint32_t cStreams;
    rc = pHlp->pfnSSMGetU32(pSSM, &cStreams);
    AssertRCReturn(rc, rc);
    if (cStreams > HDA_MAX_STREAMS)
        return pHlp->pfnSSMSetLoadError(pSSM, VERR_SSM_DATA_UNIT_FORMAT_CHANGED, RT_SRC_POS,
                                        N_("State contains %u streams while %u is the maximum supported"),
                                        cStreams, HDA_MAX_STREAMS);
    Log2Func(("cStreams=%RU32\n", cStreams));

    /* Load stream states. */
    for (uint32_t i = 0; i < cStreams; i++)
    {
        uint8_t idStream;
        rc = pHlp->pfnSSMGetU8(pSSM, &idStream);
        AssertRCReturn(rc, rc);

        /* Paranoia. */
        AssertLogRelMsgReturn(idStream < HDA_MAX_STREAMS,
                              ("HDA: Saved state contains bogus stream ID %RU8 for stream #%RU8", idStream, i),
                              VERR_SSM_INVALID_STATE);

        HDASTREAM    StreamDummyShared;
        HDASTREAMR3  StreamDummyR3;
        PHDASTREAM   pStreamShared = idStream < RT_ELEMENTS(pThis->aStreams) ? &pThis->aStreams[idStream] : &StreamDummyShared;
        PHDASTREAMR3 pStreamR3     = idStream < RT_ELEMENTS(pThisCC->aStreams) ? &pThisCC->aStreams[idStream] : &StreamDummyR3;
        AssertLogRelMsgStmt(idStream < RT_ELEMENTS(pThisCC->aStreams),
                            ("HDA stream ID=%RU8 not supported, skipping loadingit ...\n", idStream),
                            RT_ZERO(StreamDummyShared); RT_ZERO(StreamDummyR3));

        rc = hdaR3StreamSetUp(pDevIns, pThis, pStreamShared, pStreamR3, idStream);
        if (RT_FAILURE(rc))
        {
            LogRel(("HDA: Stream #%RU8: Setting up failed, rc=%Rrc\n", idStream, rc));
            /* Continue. */
        }

        rc = pHlp->pfnSSMGetStructEx(pSSM, &pStreamShared->State, sizeof(HDASTREAMSTATE),
                                     0 /* fFlags */, g_aSSMStreamStateFields7, NULL);
        AssertRCReturn(rc, rc);

        /*
         * Load BDLEs (Buffer Descriptor List Entries) and DMA counters.
         */
        rc = pHlp->pfnSSMGetStructEx(pSSM, &pStreamShared->State.BDLE.Desc, sizeof(HDABDLEDESC),
                                     0 /* fFlags */, g_aSSMBDLEDescFields7, NULL);
        AssertRCReturn(rc, rc);

        rc = pHlp->pfnSSMGetStructEx(pSSM, &pStreamShared->State.BDLE.State, sizeof(HDABDLESTATE),
                                     0 /* fFlags */, g_aSSMBDLEStateFields7, NULL);
        AssertRCReturn(rc, rc);

        Log2Func(("[SD%RU8] %R[bdle]\n", pStreamShared->u8SD, &pStreamShared->State.BDLE));

        /*
         * Load period state.
         */
        hdaR3StreamPeriodInit(&pStreamShared->State.Period, pStreamShared->u8SD, pStreamShared->u16LVI,
                              pStreamShared->u32CBL, &pStreamShared->State.Cfg);

        rc = pHlp->pfnSSMGetStructEx(pSSM, &pStreamShared->State.Period, sizeof(pStreamShared->State.Period),
                                     0 /* fFlags */, g_aSSMStreamPeriodFields7, NULL);
        AssertRCReturn(rc, rc);

        /*
         * Load internal (FIFO) buffer.
         */
        uint32_t cbCircBufSize = 0;
        pHlp->pfnSSMGetU32(pSSM, &cbCircBufSize); /* cbCircBuf */
        uint32_t cbCircBufUsed = 0;
        rc = pHlp->pfnSSMGetU32(pSSM, &cbCircBufUsed); /* cbCircBuf */
        AssertRCReturn(rc, rc);

        if (cbCircBufSize) /* If 0, skip the buffer. */
        {
            /* Paranoia. */
            AssertLogRelMsgReturn(cbCircBufSize <= _1M,
                                  ("HDA: Saved state contains bogus DMA buffer size (%RU32) for stream #%RU8",
                                   cbCircBufSize, idStream),
                                  VERR_SSM_DATA_UNIT_FORMAT_CHANGED);
            AssertLogRelMsgReturn(cbCircBufUsed <= cbCircBufSize,
                                  ("HDA: Saved state contains invalid DMA buffer usage (%RU32/%RU32) for stream #%RU8",
                                   cbCircBufUsed, cbCircBufSize, idStream),
                                  VERR_SSM_DATA_UNIT_FORMAT_CHANGED);

            /* Do we need to cre-create the circular buffer do fit the data size? */
            if (   pStreamR3->State.pCircBuf
                && cbCircBufSize != (uint32_t)RTCircBufSize(pStreamR3->State.pCircBuf))
            {
                RTCircBufDestroy(pStreamR3->State.pCircBuf);
                pStreamR3->State.pCircBuf = NULL;
            }

            rc = RTCircBufCreate(&pStreamR3->State.pCircBuf, cbCircBufSize);
            AssertRCReturn(rc, rc);

            if (cbCircBufUsed)
            {
                void  *pvBuf;
                size_t cbBuf;
                RTCircBufAcquireWriteBlock(pStreamR3->State.pCircBuf, cbCircBufUsed, &pvBuf, &cbBuf);

                AssertLogRelMsgReturn(cbBuf == cbCircBufUsed, ("cbBuf=%zu cbCircBufUsed=%zu\n", cbBuf, cbCircBufUsed),
                                      VERR_INTERNAL_ERROR_3);
                rc = pHlp->pfnSSMGetMem(pSSM, pvBuf, cbBuf);
                AssertRCReturn(rc, rc);

                RTCircBufReleaseWriteBlock(pStreamR3->State.pCircBuf, cbBuf);

                Assert(cbBuf == cbCircBufUsed);
            }
        }

        Log2Func(("[SD%RU8] LPIB=%RU32, CBL=%RU32, LVI=%RU32\n", idStream, HDA_STREAM_REG(pThis, LPIB, idStream),
                  HDA_STREAM_REG(pThis, CBL, idStream), HDA_STREAM_REG(pThis, LVI, idStream)));
#ifdef LOG_ENABLED
        hdaR3BDLEDumpAll(pDevIns, pThis, pStreamShared->u64BDLBase, pStreamShared->u16LVI + 1);
#endif
        /** @todo (Re-)initialize active periods? */

    } /* for cStreams */

    rc = hdaR3LoadExecPost(pDevIns, pThis, pThisCC);
    AssertRC(rc);

    LogFlowFuncLeaveRC(rc);
    return rc;
}


/*********************************************************************************************************************************
*   IPRT format type handlers                                                                                                    *
*********************************************************************************************************************************/

/**
 * @callback_method_impl{FNRTSTRFORMATTYPE}
 */
static DECLCALLBACK(size_t) hdaR3StrFmtBDLE(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput,
                                            const char *pszType, void const *pvValue,
                                            int cchWidth, int cchPrecision, unsigned fFlags,
                                            void *pvUser)
{
    RT_NOREF(pszType, cchWidth,  cchPrecision, fFlags, pvUser);
    PHDABDLE pBDLE = (PHDABDLE)pvValue;
    return RTStrFormat(pfnOutput,  pvArgOutput, NULL, 0,
                       "BDLE(idx:%RU32, off:%RU32, fifow:%RU32, IOC:%RTbool, DMA[%RU32 bytes @ 0x%x])",
                       pBDLE->State.u32BDLIndex, pBDLE->State.u32BufOff, pBDLE->State.cbBelowFIFOW,
                       pBDLE->Desc.fFlags & HDA_BDLE_F_IOC, pBDLE->Desc.u32BufSize, pBDLE->Desc.u64BufAddr);
}

/**
 * @callback_method_impl{FNRTSTRFORMATTYPE}
 */
static DECLCALLBACK(size_t) hdaR3StrFmtSDCTL(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput,
                                             const char *pszType, void const *pvValue,
                                             int cchWidth, int cchPrecision, unsigned fFlags,
                                             void *pvUser)
{
    RT_NOREF(pszType, cchWidth,  cchPrecision, fFlags, pvUser);
    uint32_t uSDCTL = (uint32_t)(uintptr_t)pvValue;
    return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0,
                       "SDCTL(raw:%#x, DIR:%s, TP:%RTbool, STRIPE:%x, DEIE:%RTbool, FEIE:%RTbool, IOCE:%RTbool, RUN:%RTbool, RESET:%RTbool)",
                       uSDCTL,
                       uSDCTL & HDA_SDCTL_DIR ? "OUT" : "IN",
                       RT_BOOL(uSDCTL & HDA_SDCTL_TP),
                       (uSDCTL & HDA_SDCTL_STRIPE_MASK) >> HDA_SDCTL_STRIPE_SHIFT,
                       RT_BOOL(uSDCTL & HDA_SDCTL_DEIE),
                       RT_BOOL(uSDCTL & HDA_SDCTL_FEIE),
                       RT_BOOL(uSDCTL & HDA_SDCTL_IOCE),
                       RT_BOOL(uSDCTL & HDA_SDCTL_RUN),
                       RT_BOOL(uSDCTL & HDA_SDCTL_SRST));
}

/**
 * @callback_method_impl{FNRTSTRFORMATTYPE}
 */
static DECLCALLBACK(size_t) hdaR3StrFmtSDFIFOS(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput,
                                               const char *pszType, void const *pvValue,
                                               int cchWidth, int cchPrecision, unsigned fFlags,
                                               void *pvUser)
{
    RT_NOREF(pszType, cchWidth,  cchPrecision, fFlags, pvUser);
    uint32_t uSDFIFOS = (uint32_t)(uintptr_t)pvValue;
    return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "SDFIFOS(raw:%#x, sdfifos:%RU8 B)", uSDFIFOS, uSDFIFOS ? uSDFIFOS + 1 : 0);
}

/**
 * @callback_method_impl{FNRTSTRFORMATTYPE}
 */
static DECLCALLBACK(size_t) hdaR3StrFmtSDFIFOW(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput,
                                               const char *pszType, void const *pvValue,
                                               int cchWidth, int cchPrecision, unsigned fFlags,
                                               void *pvUser)
{
    RT_NOREF(pszType, cchWidth,  cchPrecision, fFlags, pvUser);
    uint32_t uSDFIFOW = (uint32_t)(uintptr_t)pvValue;
    return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0, "SDFIFOW(raw: %#0x, sdfifow:%d B)", uSDFIFOW, hdaSDFIFOWToBytes(uSDFIFOW));
}

/**
 * @callback_method_impl{FNRTSTRFORMATTYPE}
 */
static DECLCALLBACK(size_t) hdaR3StrFmtSDSTS(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput,
                                             const char *pszType, void const *pvValue,
                                             int cchWidth, int cchPrecision, unsigned fFlags,
                                             void *pvUser)
{
    RT_NOREF(pszType, cchWidth,  cchPrecision, fFlags, pvUser);
    uint32_t uSdSts = (uint32_t)(uintptr_t)pvValue;
    return RTStrFormat(pfnOutput, pvArgOutput, NULL, 0,
                       "SDSTS(raw:%#0x, fifordy:%RTbool, dese:%RTbool, fifoe:%RTbool, bcis:%RTbool)",
                       uSdSts,
                       RT_BOOL(uSdSts & HDA_SDSTS_FIFORDY),
                       RT_BOOL(uSdSts & HDA_SDSTS_DESE),
                       RT_BOOL(uSdSts & HDA_SDSTS_FIFOE),
                       RT_BOOL(uSdSts & HDA_SDSTS_BCIS));
}


/*********************************************************************************************************************************
*   Debug Info Item Handlers                                                                                                     *
*********************************************************************************************************************************/

static int hdaR3DbgLookupRegByName(const char *pszArgs)
{
    int iReg = 0;
    for (; iReg < HDA_NUM_REGS; ++iReg)
        if (!RTStrICmp(g_aHdaRegMap[iReg].abbrev, pszArgs))
            return iReg;
    return -1;
}


static void hdaR3DbgPrintRegister(PHDASTATE pThis, PCDBGFINFOHLP pHlp, int iHdaIndex)
{
    Assert(   pThis
           && iHdaIndex >= 0
           && iHdaIndex < HDA_NUM_REGS);
    pHlp->pfnPrintf(pHlp, "%s: 0x%x\n", g_aHdaRegMap[iHdaIndex].abbrev, pThis->au32Regs[g_aHdaRegMap[iHdaIndex].mem_idx]);
}

/**
 * @callback_method_impl{FNDBGFHANDLERDEV}
 */
static DECLCALLBACK(void) hdaR3DbgInfo(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PHDASTATE pThis = PDMDEVINS_2_DATA(pDevIns, PHDASTATE);
    int iHdaRegisterIndex = hdaR3DbgLookupRegByName(pszArgs);
    if (iHdaRegisterIndex != -1)
        hdaR3DbgPrintRegister(pThis, pHlp, iHdaRegisterIndex);
    else
    {
        for(iHdaRegisterIndex = 0; (unsigned int)iHdaRegisterIndex < HDA_NUM_REGS; ++iHdaRegisterIndex)
            hdaR3DbgPrintRegister(pThis, pHlp, iHdaRegisterIndex);
    }
}

static void hdaR3DbgPrintStream(PHDASTATE pThis, PCDBGFINFOHLP pHlp, int iIdx)
{
    Assert(   pThis
           && iIdx >= 0
           && iIdx < HDA_MAX_STREAMS);

    const PHDASTREAM pStream = &pThis->aStreams[iIdx];

    pHlp->pfnPrintf(pHlp, "Stream #%d:\n", iIdx);
    pHlp->pfnPrintf(pHlp, "\tSD%dCTL  : %R[sdctl]\n",   iIdx, HDA_STREAM_REG(pThis, CTL,   iIdx));
    pHlp->pfnPrintf(pHlp, "\tSD%dCTS  : %R[sdsts]\n",   iIdx, HDA_STREAM_REG(pThis, STS,   iIdx));
    pHlp->pfnPrintf(pHlp, "\tSD%dFIFOS: %R[sdfifos]\n", iIdx, HDA_STREAM_REG(pThis, FIFOS, iIdx));
    pHlp->pfnPrintf(pHlp, "\tSD%dFIFOW: %R[sdfifow]\n", iIdx, HDA_STREAM_REG(pThis, FIFOW, iIdx));
    pHlp->pfnPrintf(pHlp, "\tBDLE     : %R[bdle]\n",    &pStream->State.BDLE);
}

static void hdaR3DbgPrintBDLE(PPDMDEVINS pDevIns, PHDASTATE pThis, PCDBGFINFOHLP pHlp, int iIdx)
{
    Assert(   pThis
           && iIdx >= 0
           && iIdx < HDA_MAX_STREAMS);

    const PHDASTREAM pStream = &pThis->aStreams[iIdx];
    const PHDABDLE   pBDLE   = &pStream->State.BDLE;

    pHlp->pfnPrintf(pHlp, "Stream #%d BDLE:\n",      iIdx);

    uint64_t u64BaseDMA = RT_MAKE_U64(HDA_STREAM_REG(pThis, BDPL, iIdx),
                                      HDA_STREAM_REG(pThis, BDPU, iIdx));
    uint16_t u16LVI     = HDA_STREAM_REG(pThis, LVI, iIdx);
    uint32_t u32CBL     = HDA_STREAM_REG(pThis, CBL, iIdx);

    if (!u64BaseDMA)
        return;

    pHlp->pfnPrintf(pHlp, "\tCurrent: %R[bdle]\n\n", pBDLE);

    pHlp->pfnPrintf(pHlp, "\tMemory:\n");

    uint32_t cbBDLE = 0;
    for (uint16_t i = 0; i < u16LVI + 1; i++)
    {
        HDABDLEDESC bd;
        PDMDevHlpPhysRead(pDevIns, u64BaseDMA + i * sizeof(HDABDLEDESC), &bd, sizeof(bd));

        pHlp->pfnPrintf(pHlp, "\t\t%s #%03d BDLE(adr:0x%llx, size:%RU32, ioc:%RTbool)\n",
                        pBDLE->State.u32BDLIndex == i ? "*" : " ", i, bd.u64BufAddr, bd.u32BufSize, bd.fFlags & HDA_BDLE_F_IOC);

        cbBDLE += bd.u32BufSize;
    }

    pHlp->pfnPrintf(pHlp, "Total: %RU32 bytes\n", cbBDLE);

    if (cbBDLE != u32CBL)
        pHlp->pfnPrintf(pHlp, "Warning: %RU32 bytes does not match CBL (%RU32)!\n", cbBDLE, u32CBL);

    pHlp->pfnPrintf(pHlp, "DMA counters (base @ 0x%llx):\n", u64BaseDMA);
    if (!u64BaseDMA) /* No DMA base given? Bail out. */
    {
        pHlp->pfnPrintf(pHlp, "\tNo counters found\n");
        return;
    }

    for (int i = 0; i < u16LVI + 1; i++)
    {
        uint32_t uDMACnt;
        PDMDevHlpPhysRead(pDevIns, (pThis->u64DPBase & DPBASE_ADDR_MASK) + (i * 2 * sizeof(uint32_t)),
                          &uDMACnt, sizeof(uDMACnt));

        pHlp->pfnPrintf(pHlp, "\t#%03d DMA @ 0x%x\n", i , uDMACnt);
    }
}

static int hdaR3DbgLookupStrmIdx(PHDASTATE pThis, const char *pszArgs)
{
    RT_NOREF(pThis, pszArgs);
    /** @todo Add args parsing. */
    return -1;
}

/**
 * @callback_method_impl{FNDBGFHANDLERDEV, hdastream}
 */
static DECLCALLBACK(void) hdaR3DbgInfoStream(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PHDASTATE   pThis         = PDMDEVINS_2_DATA(pDevIns, PHDASTATE);
    int         iHdaStreamdex = hdaR3DbgLookupStrmIdx(pThis, pszArgs);
    if (iHdaStreamdex != -1)
        hdaR3DbgPrintStream(pThis, pHlp, iHdaStreamdex);
    else
        for(iHdaStreamdex = 0; iHdaStreamdex < HDA_MAX_STREAMS; ++iHdaStreamdex)
            hdaR3DbgPrintStream(pThis, pHlp, iHdaStreamdex);
}

/**
 * @callback_method_impl{FNDBGFHANDLERDEV, hdabdle}
 */
static DECLCALLBACK(void) hdaR3DbgInfoBDLE(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PHDASTATE   pThis         = PDMDEVINS_2_DATA(pDevIns, PHDASTATE);
    int         iHdaStreamdex = hdaR3DbgLookupStrmIdx(pThis, pszArgs);
    if (iHdaStreamdex != -1)
        hdaR3DbgPrintBDLE(pDevIns, pThis, pHlp, iHdaStreamdex);
    else
        for (iHdaStreamdex = 0; iHdaStreamdex < HDA_MAX_STREAMS; ++iHdaStreamdex)
            hdaR3DbgPrintBDLE(pDevIns, pThis, pHlp, iHdaStreamdex);
}

/**
 * @callback_method_impl{FNDBGFHANDLERDEV, hdcnodes}
 */
static DECLCALLBACK(void) hdaR3DbgInfoCodecNodes(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PHDASTATER3 pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PHDASTATER3);

    if (pThisCC->pCodec->pfnDbgListNodes)
        pThisCC->pCodec->pfnDbgListNodes(pThisCC->pCodec, pHlp, pszArgs);
    else
        pHlp->pfnPrintf(pHlp, "Codec implementation doesn't provide corresponding callback\n");
}

/**
 * @callback_method_impl{FNDBGFHANDLERDEV, hdcselector}
 */
static DECLCALLBACK(void) hdaR3DbgInfoCodecSelector(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PHDASTATER3 pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PHDASTATER3);

    if (pThisCC->pCodec->pfnDbgSelector)
        pThisCC->pCodec->pfnDbgSelector(pThisCC->pCodec, pHlp, pszArgs);
    else
        pHlp->pfnPrintf(pHlp, "Codec implementation doesn't provide corresponding callback\n");
}

/**
 * @callback_method_impl{FNDBGFHANDLERDEV, hdamixer}
 */
static DECLCALLBACK(void) hdaR3DbgInfoMixer(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PHDASTATER3 pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PHDASTATER3);

    if (pThisCC->pMixer)
        AudioMixerDebug(pThisCC->pMixer, pHlp, pszArgs);
    else
        pHlp->pfnPrintf(pHlp, "Mixer not available\n");
}


/*********************************************************************************************************************************
*   PDMIBASE                                                                                                                     *
*********************************************************************************************************************************/

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) hdaR3QueryInterface(struct PDMIBASE *pInterface, const char *pszIID)
{
    PHDASTATER3 pThisCC = RT_FROM_MEMBER(pInterface, HDASTATER3, IBase);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pThisCC->IBase);
    return NULL;
}


/*********************************************************************************************************************************
*   PDMDEVREGR3                                                                                                                  *
*********************************************************************************************************************************/

/**
 * Attach command, internal version.
 *
 * This is called to let the device attach to a driver for a specified LUN
 * during runtime. This is not called during VM construction, the device
 * constructor has to attach to all the available drivers.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pThis       The shared HDA device state.
 * @param   pThisCC     The ring-3 HDA device state.
 * @param   uLUN        The logical unit which is being detached.
 * @param   fFlags      Flags, combination of the PDMDEVATT_FLAGS_* \#defines.
 * @param   ppDrv       Attached driver instance on success. Optional.
 */
static int hdaR3AttachInternal(PPDMDEVINS pDevIns, PHDASTATE pThis, PHDASTATER3 pThisCC, unsigned uLUN, uint32_t fFlags, PHDADRIVER *ppDrv)
{
    RT_NOREF(fFlags);

    /*
     * Attach driver.
     */
    char *pszDesc;
    if (RTStrAPrintf(&pszDesc, "Audio driver port (HDA) for LUN#%u", uLUN) <= 0)
        AssertLogRelFailedReturn(VERR_NO_MEMORY);

    PPDMIBASE pDrvBase;
    int rc = PDMDevHlpDriverAttach(pDevIns, uLUN, &pThisCC->IBase, &pDrvBase, pszDesc);
    if (RT_SUCCESS(rc))
    {
        PHDADRIVER pDrv = (PHDADRIVER)RTMemAllocZ(sizeof(HDADRIVER));
        if (pDrv)
        {
            pDrv->pDrvBase          = pDrvBase;
            pDrv->pHDAStateShared   = pThis;
            pDrv->pHDAStateR3       = pThisCC;
            pDrv->uLUN              = uLUN;
            pDrv->pConnector        = PDMIBASE_QUERY_INTERFACE(pDrvBase, PDMIAUDIOCONNECTOR);
            AssertMsg(pDrv->pConnector != NULL, ("Configuration error: LUN#%u has no host audio interface, rc=%Rrc\n", uLUN, rc));

            /*
             * For now we always set the driver at LUN 0 as our primary
             * host backend. This might change in the future.
             */
            if (pDrv->uLUN == 0)
                pDrv->fFlags |= PDMAUDIODRVFLAGS_PRIMARY;

            LogFunc(("LUN#%u: pCon=%p, drvFlags=0x%x\n", uLUN, pDrv->pConnector, pDrv->fFlags));

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
        LogFunc(("No attached driver for LUN #%u\n", uLUN));

    if (RT_FAILURE(rc))
    {
        /* Only free this string on failure;
         * must remain valid for the live of the driver instance. */
        RTStrFree(pszDesc);
    }

    LogFunc(("uLUN=%u, fFlags=0x%x, rc=%Rrc\n", uLUN, fFlags, rc));
    return rc;
}

/**
 * Detach command, internal version.
 *
 * This is called to let the device detach from a driver for a specified LUN
 * during runtime.
 *
 * @returns VBox status code.
 * @param   pThisCC     The ring-3 HDA device state.
 * @param   pDrv        Driver to detach from device.
 * @param   fFlags      Flags, combination of the PDMDEVATT_FLAGS_* \#defines.
 */
static int hdaR3DetachInternal(PHDASTATER3 pThisCC, PHDADRIVER pDrv, uint32_t fFlags)
{
    RT_NOREF(fFlags);

    /* First, remove the driver from our list and destory it's associated streams.
     * This also will un-set the driver as a recording source (if associated). */
    hdaR3MixerRemoveDrv(pThisCC, pDrv);

    /* Next, search backwards for a capable (attached) driver which now will be the
     * new recording source. */
    PHDADRIVER pDrvCur;
    RTListForEachReverse(&pThisCC->lstDrv, pDrvCur, HDADRIVER, Node)
    {
        if (!pDrvCur->pConnector)
            continue;

        PDMAUDIOBACKENDCFG Cfg;
        int rc2 = pDrvCur->pConnector->pfnGetConfig(pDrvCur->pConnector, &Cfg);
        if (RT_FAILURE(rc2))
            continue;

        PHDADRIVERSTREAM pDrvStrm;
# ifdef VBOX_WITH_AUDIO_HDA_MIC_IN
        pDrvStrm = &pDrvCur->MicIn;
        if (   pDrvStrm
            && pDrvStrm->pMixStrm)
        {
            rc2 = AudioMixerSinkSetRecordingSource(pThisCC->SinkMicIn.pMixSink, pDrvStrm->pMixStrm);
            if (RT_SUCCESS(rc2))
                LogRel2(("HDA: Set new recording source for 'Mic In' to '%s'\n", Cfg.szName));
        }
# endif
        pDrvStrm = &pDrvCur->LineIn;
        if (   pDrvStrm
            && pDrvStrm->pMixStrm)
        {
            rc2 = AudioMixerSinkSetRecordingSource(pThisCC->SinkLineIn.pMixSink, pDrvStrm->pMixStrm);
            if (RT_SUCCESS(rc2))
                LogRel2(("HDA: Set new recording source for 'Line In' to '%s'\n", Cfg.szName));
        }
    }

    LogFunc(("uLUN=%u, fFlags=0x%x\n", pDrv->uLUN, fFlags));
    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMDEVREG,pfnAttach}
 */
static DECLCALLBACK(int) hdaR3Attach(PPDMDEVINS pDevIns, unsigned uLUN, uint32_t fFlags)
{
    PHDASTATE   pThis   = PDMDEVINS_2_DATA(pDevIns, PHDASTATE);
    PHDASTATER3 pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PHDASTATER3);

    DEVHDA_LOCK_RETURN(pDevIns, pThis, VERR_IGNORED);

    LogFunc(("uLUN=%u, fFlags=0x%x\n", uLUN, fFlags));

    PHDADRIVER pDrv;
    int rc2 = hdaR3AttachInternal(pDevIns, pThis, pThisCC, uLUN, fFlags, &pDrv);
    if (RT_SUCCESS(rc2))
        rc2 = hdaR3MixerAddDrv(pThisCC, pDrv);

    if (RT_FAILURE(rc2))
        LogFunc(("Failed with %Rrc\n", rc2));

    DEVHDA_UNLOCK(pDevIns, pThis);

    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMDEVREG,pfnDetach}
 */
static DECLCALLBACK(void) hdaR3Detach(PPDMDEVINS pDevIns, unsigned uLUN, uint32_t fFlags)
{
    PHDASTATE   pThis   = PDMDEVINS_2_DATA(pDevIns, PHDASTATE);
    PHDASTATER3 pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PHDASTATER3);

    DEVHDA_LOCK(pDevIns, pThis);

    LogFunc(("uLUN=%u, fFlags=0x%x\n", uLUN, fFlags));

    PHDADRIVER pDrv, pDrvNext;
    RTListForEachSafe(&pThisCC->lstDrv, pDrv, pDrvNext, HDADRIVER, Node)
    {
        if (pDrv->uLUN == uLUN)
        {
            int rc2 = hdaR3DetachInternal(pThisCC, pDrv, fFlags);
            if (RT_SUCCESS(rc2))
            {
                RTMemFree(pDrv);
                pDrv = NULL;
            }

            break;
        }
    }

    DEVHDA_UNLOCK(pDevIns, pThis);
}

/**
 * Powers off the device.
 *
 * @param   pDevIns             Device instance to power off.
 */
static DECLCALLBACK(void) hdaR3PowerOff(PPDMDEVINS pDevIns)
{
    PHDASTATE   pThis   = PDMDEVINS_2_DATA(pDevIns, PHDASTATE);
    PHDASTATER3 pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PHDASTATER3);

    DEVHDA_LOCK_RETURN_VOID(pDevIns, pThis);

    LogRel2(("HDA: Powering off ...\n"));

    /* Ditto goes for the codec, which in turn uses the mixer. */
    hdaCodecPowerOff(pThisCC->pCodec);

    /*
     * Note: Destroy the mixer while powering off and *not* in hdaR3Destruct,
     *       giving the mixer the chance to release any references held to
     *       PDM audio streams it maintains.
     */
    if (pThisCC->pMixer)
    {
        AudioMixerDestroy(pThisCC->pMixer);
        pThisCC->pMixer = NULL;
    }

    DEVHDA_UNLOCK(pDevIns, pThis);
}

/**
 * Replaces a driver with a the NullAudio drivers.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pThis       The shared HDA device state.
 * @param   pThisCC     The ring-3 HDA device state.
 * @param   pThis       Device instance.
 * @param   iLun        The logical unit which is being replaced.
 */
static int hdaR3ReconfigLunWithNullAudio(PPDMDEVINS pDevIns, PHDASTATE pThis, PHDASTATER3 pThisCC, unsigned iLun)
{
    int rc = PDMDevHlpDriverReconfigure2(pDevIns, iLun, "AUDIO", "NullAudio");
    if (RT_SUCCESS(rc))
        rc = hdaR3AttachInternal(pDevIns, pThis, pThisCC, iLun, 0 /* fFlags */, NULL /* ppDrv */);
    LogFunc(("pThis=%p, iLun=%u, rc=%Rrc\n", pThis, iLun, rc));
    return rc;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnReset}
 */
static DECLCALLBACK(void) hdaR3Reset(PPDMDEVINS pDevIns)
{
    PHDASTATE   pThis   = PDMDEVINS_2_DATA(pDevIns, PHDASTATE);
    PHDASTATER3 pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PHDASTATER3);

    LogFlowFuncEnter();

    DEVHDA_LOCK_RETURN_VOID(pDevIns, pThis);

     /*
     * 18.2.6,7 defines that values of this registers might be cleared on power on/reset
     * hdaR3Reset shouldn't affects these registers.
     */
    HDA_REG(pThis, WAKEEN)  = 0x0;

    hdaR3GCTLReset(pDevIns, pThis, pThisCC);

    /* Indicate that HDA is not in reset. The firmware is supposed to (un)reset HDA,
     * but we can take a shortcut.
     */
    HDA_REG(pThis, GCTL)    = HDA_GCTL_CRST;

    DEVHDA_UNLOCK(pDevIns, pThis);
}


/**
 * @interface_method_impl{PDMDEVREG,pfnDestruct}
 */
static DECLCALLBACK(int) hdaR3Destruct(PPDMDEVINS pDevIns)
{
    PDMDEV_CHECK_VERSIONS_RETURN_QUIET(pDevIns); /* this shall come first */
    PHDASTATE   pThis   = PDMDEVINS_2_DATA(pDevIns, PHDASTATE);
    PHDASTATER3 pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PHDASTATER3);
    DEVHDA_LOCK(pDevIns, pThis); /** @todo r=bird: this will fail on early constructor failure. */

    PHDADRIVER pDrv;
    while (!RTListIsEmpty(&pThisCC->lstDrv))
    {
        pDrv = RTListGetFirst(&pThisCC->lstDrv, HDADRIVER, Node);

        RTListNodeRemove(&pDrv->Node);
        RTMemFree(pDrv);
    }

    if (pThisCC->pCodec)
    {
        hdaCodecDestruct(pThisCC->pCodec);

        RTMemFree(pThisCC->pCodec);
        pThisCC->pCodec = NULL;
    }

    for (uint8_t i = 0; i < HDA_MAX_STREAMS; i++)
        hdaR3StreamDestroy(&pThis->aStreams[i], &pThisCC->aStreams[i]);

    DEVHDA_UNLOCK(pDevIns, pThis);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int) hdaR3Construct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns); /* this shall come first */
    PHDASTATE       pThis   = PDMDEVINS_2_DATA(pDevIns, PHDASTATE);
    PHDASTATER3     pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PHDASTATER3);
    PCPDMDEVHLPR3   pHlp    = pDevIns->pHlpR3;
    Assert(iInstance == 0); RT_NOREF(iInstance);

    /*
     * Initialize the state sufficently to make the destructor work.
     */
    pThis->uAlignmentCheckMagic = HDASTATE_ALIGNMENT_CHECK_MAGIC;
    RTListInit(&pThisCC->lstDrv);
    pThis->cbCorbBuf            = HDA_CORB_SIZE * HDA_CORB_ELEMENT_SIZE;
    pThis->cbRirbBuf            = HDA_RIRB_SIZE * HDA_RIRB_ELEMENT_SIZE;

    /** @todo r=bird: There are probably other things which should be
     *        initialized here before we start failing. */

    /*
     * Validate and read configuration.
     */
    PDMDEV_VALIDATE_CONFIG_RETURN(pDevIns, "TimerHz|PosAdjustEnabled|PosAdjustFrames|DebugEnabled|DebugPathOut", "");

    int rc = pHlp->pfnCFGMQueryU16Def(pCfg, "TimerHz", &pThis->uTimerHz, HDA_TIMER_HZ_DEFAULT /* Default value, if not set. */);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("HDA configuration error: failed to read Hertz (Hz) rate as unsigned integer"));

    if (pThis->uTimerHz != HDA_TIMER_HZ_DEFAULT)
        LogRel(("HDA: Using custom device timer rate (%RU16Hz)\n", pThis->uTimerHz));

    rc = pHlp->pfnCFGMQueryBoolDef(pCfg, "PosAdjustEnabled", &pThis->fPosAdjustEnabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("HDA configuration error: failed to read position adjustment enabled as boolean"));

    if (!pThis->fPosAdjustEnabled)
        LogRel(("HDA: Position adjustment is disabled\n"));

    rc = pHlp->pfnCFGMQueryU16Def(pCfg, "PosAdjustFrames", &pThis->cPosAdjustFrames, HDA_POS_ADJUST_DEFAULT);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("HDA configuration error: failed to read position adjustment frames as unsigned integer"));

    if (pThis->cPosAdjustFrames)
        LogRel(("HDA: Using custom position adjustment (%RU16 audio frames)\n", pThis->cPosAdjustFrames));

    rc = pHlp->pfnCFGMQueryBoolDef(pCfg, "DebugEnabled", &pThisCC->Dbg.fEnabled, false);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("HDA configuration error: failed to read debugging enabled flag as boolean"));

    rc = pHlp->pfnCFGMQueryStringAllocDef(pCfg, "DebugPathOut", &pThisCC->Dbg.pszOutPath, VBOX_AUDIO_DEBUG_DUMP_PCM_DATA_PATH);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("HDA configuration error: failed to read debugging output path flag as string"));

    if (pThisCC->Dbg.fEnabled)
        LogRel2(("HDA: Debug output will be saved to '%s'\n", pThisCC->Dbg.pszOutPath));

    /*
     * Use our own critical section for the device instead of the default
     * one provided by PDM. This allows fine-grained locking in combination
     * with TM when timer-specific stuff is being called in e.g. the MMIO handlers.
     */
    rc = PDMDevHlpCritSectInit(pDevIns, &pThis->CritSect, RT_SRC_POS, "HDA");
    AssertRCReturn(rc, rc);

    rc = PDMDevHlpSetDeviceCritSect(pDevIns, PDMDevHlpCritSectGetNop(pDevIns));
    AssertRCReturn(rc, rc);

    /*
     * Initialize data (most of it anyway).
     */
    pThisCC->pDevIns                  = pDevIns;
    /* IBase */
    pThisCC->IBase.pfnQueryInterface  = hdaR3QueryInterface;

    /* PCI Device */
    PPDMPCIDEV pPciDev = pDevIns->apPciDevs[0];
    PDMPCIDEV_ASSERT_VALID(pDevIns, pPciDev);

    PDMPciDevSetVendorId(      pPciDev, HDA_PCI_VENDOR_ID); /* nVidia */
    PDMPciDevSetDeviceId(      pPciDev, HDA_PCI_DEVICE_ID); /* HDA */

    PDMPciDevSetCommand(       pPciDev, 0x0000); /* 04 rw,ro - pcicmd. */
    PDMPciDevSetStatus(        pPciDev, VBOX_PCI_STATUS_CAP_LIST); /* 06 rwc?,ro? - pcists. */
    PDMPciDevSetRevisionId(    pPciDev, 0x01);   /* 08 ro - rid. */
    PDMPciDevSetClassProg(     pPciDev, 0x00);   /* 09 ro - pi. */
    PDMPciDevSetClassSub(      pPciDev, 0x03);   /* 0a ro - scc; 03 == HDA. */
    PDMPciDevSetClassBase(     pPciDev, 0x04);   /* 0b ro - bcc; 04 == multimedia. */
    PDMPciDevSetHeaderType(    pPciDev, 0x00);   /* 0e ro - headtyp. */
    PDMPciDevSetBaseAddress(   pPciDev, 0,       /* 10 rw - MMIO */
                               false /* fIoSpace */, false /* fPrefetchable */, true /* f64Bit */, 0x00000000);
    PDMPciDevSetInterruptLine( pPciDev, 0x00);   /* 3c rw. */
    PDMPciDevSetInterruptPin(  pPciDev, 0x01);   /* 3d ro - INTA#. */

#if defined(HDA_AS_PCI_EXPRESS)
    PDMPciDevSetCapabilityList(pPciDev, 0x80);
#elif defined(VBOX_WITH_MSI_DEVICES)
    PDMPciDevSetCapabilityList(pPciDev, 0x60);
#else
    PDMPciDevSetCapabilityList(pPciDev, 0x50);   /* ICH6 datasheet 18.1.16 */
#endif

    /// @todo r=michaln: If there are really no PDMPciDevSetXx for these, the
    /// meaning of these values needs to be properly documented!
    /* HDCTL off 0x40 bit 0 selects signaling mode (1-HDA, 0 - Ac97) 18.1.19 */
    PDMPciDevSetByte(          pPciDev, 0x40, 0x01);

    /* Power Management */
    PDMPciDevSetByte(          pPciDev, 0x50 + 0, VBOX_PCI_CAP_ID_PM);
    PDMPciDevSetByte(          pPciDev, 0x50 + 1, 0x0); /* next */
    PDMPciDevSetWord(          pPciDev, 0x50 + 2, VBOX_PCI_PM_CAP_DSI | 0x02 /* version, PM1.1 */ );

#ifdef HDA_AS_PCI_EXPRESS
    /* PCI Express */
    PDMPciDevSetByte(          pPciDev, 0x80 + 0, VBOX_PCI_CAP_ID_EXP); /* PCI_Express */
    PDMPciDevSetByte(          pPciDev, 0x80 + 1, 0x60); /* next */
    /* Device flags */
    PDMPciDevSetWord(          pPciDev, 0x80 + 2,
                                 1 /* version */
                               | (VBOX_PCI_EXP_TYPE_ROOT_INT_EP << 4) /* Root Complex Integrated Endpoint */
                               | (100 << 9) /* MSI */ );
    /* Device capabilities */
    PDMPciDevSetDWord(         pPciDev, 0x80 + 4, VBOX_PCI_EXP_DEVCAP_FLRESET);
    /* Device control */
    PDMPciDevSetWord(          pPciDev, 0x80 + 8, 0);
    /* Device status */
    PDMPciDevSetWord(          pPciDev, 0x80 + 10, 0);
    /* Link caps */
    PDMPciDevSetDWord(         pPciDev, 0x80 + 12, 0);
    /* Link control */
    PDMPciDevSetWord(          pPciDev, 0x80 + 16, 0);
    /* Link status */
    PDMPciDevSetWord(          pPciDev, 0x80 + 18, 0);
    /* Slot capabilities */
    PDMPciDevSetDWord(         pPciDev, 0x80 + 20, 0);
    /* Slot control */
    PDMPciDevSetWord(          pPciDev, 0x80 + 24, 0);
    /* Slot status */
    PDMPciDevSetWord(          pPciDev, 0x80 + 26, 0);
    /* Root control */
    PDMPciDevSetWord(          pPciDev, 0x80 + 28, 0);
    /* Root capabilities */
    PDMPciDevSetWord(          pPciDev, 0x80 + 30, 0);
    /* Root status */
    PDMPciDevSetDWord(         pPciDev, 0x80 + 32, 0);
    /* Device capabilities 2 */
    PDMPciDevSetDWord(         pPciDev, 0x80 + 36, 0);
    /* Device control 2 */
    PDMPciDevSetQWord(         pPciDev, 0x80 + 40, 0);
    /* Link control 2 */
    PDMPciDevSetQWord(         pPciDev, 0x80 + 48, 0);
    /* Slot control 2 */
    PDMPciDevSetWord(          pPciDev, 0x80 + 56, 0);
#endif

    /*
     * Register the PCI device.
     */
    rc = PDMDevHlpPCIRegister(pDevIns, pPciDev);
    AssertRCReturn(rc, rc);

    /** @todo r=bird: The IOMMMIO_FLAGS_READ_DWORD flag isn't entirely optimal,
     * as several frequently used registers aren't dword sized.  6.0 and earlier
     * will go to ring-3 to handle accesses to any such register, where-as 6.1 and
     * later will do trivial register reads in ring-0.   Real optimal code would use
     * IOMMMIO_FLAGS_READ_PASSTHRU and do the necessary extra work to deal with
     * anything the guest may throw at us. */
    rc = PDMDevHlpPCIIORegionCreateMmio(pDevIns, 0, 0x4000, PCI_ADDRESS_SPACE_MEM, hdaMmioWrite, hdaMmioRead, NULL /*pvUser*/,
                                        IOMMMIO_FLAGS_READ_DWORD | IOMMMIO_FLAGS_WRITE_PASSTHRU, "HDA", &pThis->hMmio);
    AssertRCReturn(rc, rc);

#ifdef VBOX_WITH_MSI_DEVICES
    PDMMSIREG MsiReg;
    RT_ZERO(MsiReg);
    MsiReg.cMsiVectors    = 1;
    MsiReg.iMsiCapOffset  = 0x60;
    MsiReg.iMsiNextOffset = 0x50;
    rc = PDMDevHlpPCIRegisterMsi(pDevIns, &MsiReg);
    if (RT_FAILURE(rc))
    {
        /* That's OK, we can work without MSI */
        PDMPciDevSetCapabilityList(pPciDev, 0x50);
    }
#endif

    rc = PDMDevHlpSSMRegister(pDevIns, HDA_SAVED_STATE_VERSION, sizeof(*pThis), hdaR3SaveExec, hdaR3LoadExec);
    AssertRCReturn(rc, rc);

#ifdef VBOX_WITH_AUDIO_HDA_ASYNC_IO
    LogRel(("HDA: Asynchronous I/O enabled\n"));
#endif

    /*
     * Attach drivers.  We ASSUME they are configured consecutively without any
     * gaps, so we stop when we hit the first LUN w/o a driver configured.
     */
    for (unsigned iLun = 0; ; iLun++)
    {
        AssertBreak(iLun < UINT8_MAX);
        LogFunc(("Trying to attach driver for LUN#%u ...\n", iLun));
        rc = hdaR3AttachInternal(pDevIns, pThis, pThisCC, iLun, 0 /* fFlags */, NULL /* ppDrv */);
        if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
        {
            LogFunc(("cLUNs=%u\n", iLun));
            break;
        }
        if (rc == VERR_AUDIO_BACKEND_INIT_FAILED)
        {
            hdaR3ReconfigLunWithNullAudio(pDevIns, pThis, pThisCC, iLun); /* Pretend attaching to the NULL audio backend will never fail. */
            PDMDevHlpVMSetRuntimeError(pDevIns, 0 /*fFlags*/, "HostAudioNotResponding",
                                       N_("Host audio backend initialization has failed. Selecting the NULL audio backend with the consequence that no sound is audible"));
        }
        else
            AssertLogRelMsgReturn(RT_SUCCESS(rc),  ("LUN#%u: rc=%Rrc\n", iLun, rc), rc);
    }

    /*
     * Create the mixer.
     */
    rc = AudioMixerCreate("HDA Mixer", 0 /* uFlags */, &pThisCC->pMixer);
    AssertRCReturn(rc, rc);

    /*
     * Add mixer output sinks.
     */
#ifdef VBOX_WITH_AUDIO_HDA_51_SURROUND
    rc = AudioMixerCreateSink(pThisCC->pMixer, "[Playback] Front", AUDMIXSINKDIR_OUTPUT, &pThisCC->SinkFront.pMixSink);
    AssertRCReturn(rc, rc);
    rc = AudioMixerCreateSink(pThisCC->pMixer, "[Playback] Center / Subwoofer", AUDMIXSINKDIR_OUTPUT, &pThisCC->SinkCenterLFE.pMixSink);
    AssertRCReturn(rc, rc);
    rc = AudioMixerCreateSink(pThisCC->pMixer, "[Playback] Rear", AUDMIXSINKDIR_OUTPUT, &pThisCC->SinkRear.pMixSink);
    AssertRCReturn(rc, rc);
#else
    rc = AudioMixerCreateSink(pThisCC->pMixer, "[Playback] PCM Output", AUDMIXSINKDIR_OUTPUT, &pThisCC->SinkFront.pMixSink);
    AssertRCReturn(rc, rc);
#endif

    /*
     * Add mixer input sinks.
     */
    rc = AudioMixerCreateSink(pThisCC->pMixer, "[Recording] Line In", AUDMIXSINKDIR_INPUT, &pThisCC->SinkLineIn.pMixSink);
    AssertRCReturn(rc, rc);
#ifdef VBOX_WITH_AUDIO_HDA_MIC_IN
    rc = AudioMixerCreateSink(pThisCC->pMixer, "[Recording] Microphone In", AUDMIXSINKDIR_INPUT, &pThisCC->SinkMicIn.pMixSink);
    AssertRCReturn(rc, rc);
#endif

    /* There is no master volume control. Set the master to max. */
    PDMAUDIOVOLUME vol = { false, 255, 255 };
    rc = AudioMixerSetMasterVolume(pThisCC->pMixer, &vol);
    AssertRCReturn(rc, rc);

    /* Allocate codec. */
    PHDACODEC pCodec = (PHDACODEC)RTMemAllocZ(sizeof(HDACODEC));
    pThisCC->pCodec = pCodec;
    AssertReturn(pCodec, VERR_NO_MEMORY);

    /* Set codec callbacks to this controller. */
    pCodec->pDevIns                = pDevIns;
    pCodec->pfnCbMixerAddStream    = hdaR3MixerAddStream;
    pCodec->pfnCbMixerRemoveStream = hdaR3MixerRemoveStream;
    pCodec->pfnCbMixerControl      = hdaR3MixerControl;
    pCodec->pfnCbMixerSetVolume    = hdaR3MixerSetVolume;

    /* Construct the codec. */
    rc = hdaCodecConstruct(pDevIns, pCodec, 0 /* Codec index */, pCfg);
    AssertRCReturn(rc, rc);

    /* ICH6 datasheet defines 0 values for SVID and SID (18.1.14-15), which together with values returned for
       verb F20 should provide device/codec recognition. */
    Assert(pCodec->u16VendorId);
    Assert(pCodec->u16DeviceId);
    PDMPciDevSetSubSystemVendorId(pPciDev, pCodec->u16VendorId); /* 2c ro - intel.) */
    PDMPciDevSetSubSystemId(      pPciDev, pCodec->u16DeviceId); /* 2e ro. */

    /*
     * Create the per stream timers and the asso.
     *
     * We must the critical section for the timers as the device has a
     * noop section associated with it.
     *
     * Note:  Use TMCLOCK_VIRTUAL_SYNC here, as the guest's HDA driver relies
     *        on exact (virtual) DMA timing and uses DMA Position Buffers
     *        instead of the LPIB registers.
     */
    static const char * const s_apszNames[] =
    {
        "HDA SD0", "HDA SD1", "HDA SD2", "HDA SD3",
        "HDA SD4", "HDA SD5", "HDA SD6", "HDA SD7",
    };
    AssertCompile(RT_ELEMENTS(s_apszNames) == HDA_MAX_STREAMS);
    for (size_t i = 0; i < HDA_MAX_STREAMS; i++)
    {
        rc = PDMDevHlpTimerCreate(pDevIns, TMCLOCK_VIRTUAL_SYNC, hdaR3Timer, (void *)(uintptr_t)i,
                                  TMTIMER_FLAGS_NO_CRIT_SECT, s_apszNames[i], &pThis->aStreams[i].hTimer);
        AssertRCReturn(rc, rc);

        rc = PDMDevHlpTimerSetCritSect(pDevIns, pThis->aStreams[i].hTimer, &pThis->CritSect);
        AssertRCReturn(rc, rc);
    }

    /*
     * Create all hardware streams.
     */
    for (uint8_t i = 0; i < HDA_MAX_STREAMS; ++i)
    {
        rc = hdaR3StreamConstruct(&pThis->aStreams[i], &pThisCC->aStreams[i], pThis, pThisCC, i /* u8SD */);
        AssertRCReturn(rc, rc);
    }

#ifdef VBOX_WITH_AUDIO_HDA_ONETIME_INIT
    /*
     * Initialize the driver chain.
     */
    PHDADRIVER pDrv;
    RTListForEach(&pThisCC->lstDrv, pDrv, HDADRIVER, Node)
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
# ifdef VBOX_WITH_AUDIO_HDA_MIC_IN
        bool fValidMicIn  = AudioMixerStreamIsValid(pDrv->MicIn.pMixStrm);
# endif
        bool fValidOut    = AudioMixerStreamIsValid(pDrv->Front.pMixStrm);
# ifdef VBOX_WITH_AUDIO_HDA_51_SURROUND
        /** @todo Anything to do here? */
# endif

        if (    !fValidLineIn
# ifdef VBOX_WITH_AUDIO_HDA_MIC_IN
             && !fValidMicIn
# endif
             && !fValidOut)
        {
            LogRel(("HDA: Falling back to NULL backend (no sound audible)\n"));
            hdaR3Reset(pDevIns);
            hdaR3ReconfigLunWithNullAudio(pThis, pDrv->uLUN);
            PDMDevHlpVMSetRuntimeError(pDevIns, 0 /*fFlags*/, "HostAudioNotResponding",
                                       N_("No audio devices could be opened. "
                                          "Selecting the NULL audio backend with the consequence that no sound is audible"));
        }
        else
        {
            bool fWarn = false;

            PDMAUDIOBACKENDCFG BackendCfg;
            int rc2 = pCon->pfnGetConfig(pCon, &BackendCfg);
            if (RT_SUCCESS(rc2))
            {
                if (BackendCfg.cMaxStreamsIn)
                {
# ifdef VBOX_WITH_AUDIO_HDA_MIC_IN
                    /* If the audio backend supports two or more input streams at once,
                     * warn if one of our two inputs (microphone-in and line-in) failed to initialize. */
                    if (BackendCfg.cMaxStreamsIn >= 2)
                        fWarn = !fValidLineIn || !fValidMicIn;
                    /* If the audio backend only supports one input stream at once (e.g. pure ALSA, and
                     * *not* ALSA via PulseAudio plugin!), only warn if both of our inputs failed to initialize.
                     * One of the two simply is not in use then. */
                    else if (BackendCfg.cMaxStreamsIn == 1)
                        fWarn = !fValidLineIn && !fValidMicIn;
                    /* Don't warn if our backend is not able of supporting any input streams at all. */
# else  /* !VBOX_WITH_AUDIO_HDA_MIC_IN */
                    /* We only have line-in as input source. */
                    fWarn = !fValidLineIn;
# endif /* !VBOX_WITH_AUDIO_HDA_MIC_IN */
                }

                if (   !fWarn
                    && BackendCfg.cMaxStreamsOut)
                    fWarn = !fValidOut;
            }
            else
            {
                LogRel(("HDA: Unable to retrieve audio backend configuration for LUN #%RU8, rc=%Rrc\n", pDrv->uLUN, rc2));
                fWarn = true;
            }

            if (fWarn)
            {
                char   szMissingStreams[255];
                size_t len = 0;
                if (!fValidLineIn)
                {
                    LogRel(("HDA: WARNING: Unable to open PCM line input for LUN #%RU8!\n", pDrv->uLUN));
                    len = RTStrPrintf(szMissingStreams, sizeof(szMissingStreams), "PCM Input");
                }
# ifdef VBOX_WITH_AUDIO_HDA_MIC_IN
                if (!fValidMicIn)
                {
                    LogRel(("HDA: WARNING: Unable to open PCM microphone input for LUN #%RU8!\n", pDrv->uLUN));
                    len += RTStrPrintf(szMissingStreams + len,
                                       sizeof(szMissingStreams) - len, len ? ", PCM Microphone" : "PCM Microphone");
                }
# endif
                if (!fValidOut)
                {
                    LogRel(("HDA: WARNING: Unable to open PCM output for LUN #%RU8!\n", pDrv->uLUN));
                    len += RTStrPrintf(szMissingStreams + len,
                                       sizeof(szMissingStreams) - len, len ? ", PCM Output" : "PCM Output");
                }

                PDMDevHlpVMSetRuntimeError(pDevIns, 0 /*fFlags*/, "HostAudioNotResponding",
                                           N_("Some HDA audio streams (%s) could not be opened. "
                                              "Guest applications generating audio output or depending on audio input may hang. "
                                              "Make sure your host audio device is working properly. "
                                              "Check the logfile for error messages of the audio subsystem"), szMissingStreams);
            }
        }
    }
#endif /* VBOX_WITH_AUDIO_HDA_ONETIME_INIT */

    hdaR3Reset(pDevIns);

    /*
     * Info items and string formatter types.  The latter is non-optional as
     * the info handles use (at least some of) the custom types and we cannot
     * accept screwing formatting.
     */
    PDMDevHlpDBGFInfoRegister(pDevIns, "hda",         "HDA info. (hda [register case-insensitive])",     hdaR3DbgInfo);
    PDMDevHlpDBGFInfoRegister(pDevIns, "hdabdle",     "HDA stream BDLE info. (hdabdle [stream number])", hdaR3DbgInfoBDLE);
    PDMDevHlpDBGFInfoRegister(pDevIns, "hdastream",   "HDA stream info. (hdastream [stream number])",    hdaR3DbgInfoStream);
    PDMDevHlpDBGFInfoRegister(pDevIns, "hdcnodes",    "HDA codec nodes.",                                hdaR3DbgInfoCodecNodes);
    PDMDevHlpDBGFInfoRegister(pDevIns, "hdcselector", "HDA codec's selector states [node number].",      hdaR3DbgInfoCodecSelector);
    PDMDevHlpDBGFInfoRegister(pDevIns, "hdamixer",    "HDA mixer state.",                                hdaR3DbgInfoMixer);

    rc = RTStrFormatTypeRegister("bdle",    hdaR3StrFmtBDLE,    NULL);
    AssertMsgReturn(RT_SUCCESS(rc) || rc == VERR_ALREADY_EXISTS, ("%Rrc\n", rc), rc);
    rc = RTStrFormatTypeRegister("sdctl",   hdaR3StrFmtSDCTL,   NULL);
    AssertMsgReturn(RT_SUCCESS(rc) || rc == VERR_ALREADY_EXISTS, ("%Rrc\n", rc), rc);
    rc = RTStrFormatTypeRegister("sdsts",   hdaR3StrFmtSDSTS,   NULL);
    AssertMsgReturn(RT_SUCCESS(rc) || rc == VERR_ALREADY_EXISTS, ("%Rrc\n", rc), rc);
    rc = RTStrFormatTypeRegister("sdfifos", hdaR3StrFmtSDFIFOS, NULL);
    AssertMsgReturn(RT_SUCCESS(rc) || rc == VERR_ALREADY_EXISTS, ("%Rrc\n", rc), rc);
    rc = RTStrFormatTypeRegister("sdfifow", hdaR3StrFmtSDFIFOW, NULL);
    AssertMsgReturn(RT_SUCCESS(rc) || rc == VERR_ALREADY_EXISTS, ("%Rrc\n", rc), rc);

    /*
     * Asserting sanity.
     */
    for (unsigned i = 0; i < RT_ELEMENTS(g_aHdaRegMap); i++)
    {
        struct HDAREGDESC const *pReg     = &g_aHdaRegMap[i];
        struct HDAREGDESC const *pNextReg = i + 1 < RT_ELEMENTS(g_aHdaRegMap) ?  &g_aHdaRegMap[i + 1] : NULL;

        /* binary search order. */
        AssertReleaseMsg(!pNextReg || pReg->offset + pReg->size <= pNextReg->offset,
                         ("[%#x] = {%#x LB %#x}  vs. [%#x] = {%#x LB %#x}\n",
                          i, pReg->offset, pReg->size, i + 1, pNextReg->offset, pNextReg->size));

        /* alignment. */
        AssertReleaseMsg(   pReg->size == 1
                         || (pReg->size == 2 && (pReg->offset & 1) == 0)
                         || (pReg->size == 3 && (pReg->offset & 3) == 0)
                         || (pReg->size == 4 && (pReg->offset & 3) == 0),
                         ("[%#x] = {%#x LB %#x}\n", i, pReg->offset, pReg->size));

        /* registers are packed into dwords - with 3 exceptions with gaps at the end of the dword. */
        AssertRelease(((pReg->offset + pReg->size) & 3) == 0 || pNextReg);
        if (pReg->offset & 3)
        {
            struct HDAREGDESC const *pPrevReg = i > 0 ?  &g_aHdaRegMap[i - 1] : NULL;
            AssertReleaseMsg(pPrevReg, ("[%#x] = {%#x LB %#x}\n", i, pReg->offset, pReg->size));
            if (pPrevReg)
                AssertReleaseMsg(pPrevReg->offset + pPrevReg->size == pReg->offset,
                                 ("[%#x] = {%#x LB %#x}  vs. [%#x] = {%#x LB %#x}\n",
                                  i - 1, pPrevReg->offset, pPrevReg->size, i + 1, pReg->offset, pReg->size));
        }
#if 0
        if ((pReg->offset + pReg->size) & 3)
        {
            AssertReleaseMsg(pNextReg, ("[%#x] = {%#x LB %#x}\n", i, pReg->offset, pReg->size));
            if (pNextReg)
                AssertReleaseMsg(pReg->offset + pReg->size == pNextReg->offset,
                                 ("[%#x] = {%#x LB %#x}  vs. [%#x] = {%#x LB %#x}\n",
                                  i, pReg->offset, pReg->size, i + 1,  pNextReg->offset, pNextReg->size));
        }
#endif
        /* The final entry is a full DWORD, no gaps! Allows shortcuts. */
        AssertReleaseMsg(pNextReg || ((pReg->offset + pReg->size) & 3) == 0,
                         ("[%#x] = {%#x LB %#x}\n", i, pReg->offset, pReg->size));
    }

# ifdef VBOX_WITH_STATISTICS
    /*
     * Register statistics.
     */
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatIn,               STAMTYPE_PROFILE, "Input",             STAMUNIT_TICKS_PER_CALL, "Profiling input.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatOut,              STAMTYPE_PROFILE, "Output",            STAMUNIT_TICKS_PER_CALL, "Profiling output.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatBytesRead,        STAMTYPE_COUNTER, "BytesRead"   ,      STAMUNIT_BYTES,          "Bytes read from HDA emulation.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatBytesWritten,     STAMTYPE_COUNTER, "BytesWritten",      STAMUNIT_BYTES,          "Bytes written to HDA emulation.");

    AssertCompile(RT_ELEMENTS(g_aHdaRegMap)              == HDA_NUM_REGS);
    AssertCompile(RT_ELEMENTS(pThis->aStatRegReads)      == HDA_NUM_REGS);
    AssertCompile(RT_ELEMENTS(pThis->aStatRegReadsToR3)  == HDA_NUM_REGS);
    AssertCompile(RT_ELEMENTS(pThis->aStatRegWrites)     == HDA_NUM_REGS);
    AssertCompile(RT_ELEMENTS(pThis->aStatRegWritesToR3) == HDA_NUM_REGS);
    for (size_t i = 0; i < RT_ELEMENTS(g_aHdaRegMap); i++)
    {
        PDMDevHlpSTAMRegisterF(pDevIns, &pThis->aStatRegReads[i], STAMTYPE_COUNTER,  STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,
                               g_aHdaRegMap[i].desc, "Regs/%03x-%s-Reads",       g_aHdaRegMap[i].offset, g_aHdaRegMap[i].abbrev);
        PDMDevHlpSTAMRegisterF(pDevIns, &pThis->aStatRegReadsToR3[i], STAMTYPE_COUNTER,  STAMVISIBILITY_USED, STAMUNIT_OCCURENCES,
                               g_aHdaRegMap[i].desc, "Regs/%03x-%s-Reads-ToR3",  g_aHdaRegMap[i].offset, g_aHdaRegMap[i].abbrev);
        PDMDevHlpSTAMRegisterF(pDevIns, &pThis->aStatRegWrites[i], STAMTYPE_COUNTER,  STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,
                               g_aHdaRegMap[i].desc, "Regs/%03x-%s-Writes",      g_aHdaRegMap[i].offset, g_aHdaRegMap[i].abbrev);
        PDMDevHlpSTAMRegisterF(pDevIns, &pThis->aStatRegWritesToR3[i], STAMTYPE_COUNTER,  STAMVISIBILITY_USED, STAMUNIT_OCCURENCES,
                               g_aHdaRegMap[i].desc, "Regs/%03x-%s-Writes-ToR3", g_aHdaRegMap[i].offset, g_aHdaRegMap[i].abbrev);
    }
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatRegMultiReadsR3,   STAMTYPE_COUNTER, "RegMultiReadsR3",    STAMUNIT_OCCURENCES, "Register read not targeting just one register, handled in ring-3");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatRegMultiReadsRZ,   STAMTYPE_COUNTER, "RegMultiReadsRZ",    STAMUNIT_OCCURENCES, "Register read not targeting just one register, handled in ring-0");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatRegMultiWritesR3,  STAMTYPE_COUNTER, "RegMultiWritesR3",   STAMUNIT_OCCURENCES, "Register writes not targeting just one register, handled in ring-3");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatRegMultiWritesRZ,  STAMTYPE_COUNTER, "RegMultiWritesRZ",   STAMUNIT_OCCURENCES, "Register writes not targeting just one register, handled in ring-0");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatRegSubWriteR3,     STAMTYPE_COUNTER, "RegSubWritesR3",     STAMUNIT_OCCURENCES, "Trucated register writes, handled in ring-3");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatRegSubWriteRZ,     STAMTYPE_COUNTER, "RegSubWritesRZ",     STAMUNIT_OCCURENCES, "Trucated register writes, handled in ring-0");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatRegUnknownReads,   STAMTYPE_COUNTER, "RegUnknownReads",    STAMUNIT_OCCURENCES, "Reads of unknown registers.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatRegUnknownWrites,  STAMTYPE_COUNTER, "RegUnknownWrites",   STAMUNIT_OCCURENCES, "Writes to unknown registers.");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatRegWritesBlockedByReset, STAMTYPE_COUNTER, "RegWritesBlockedByReset", STAMUNIT_OCCURENCES, "Writes blocked by pending reset (GCTL/CRST)");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatRegWritesBlockedByRun,   STAMTYPE_COUNTER, "RegWritesBlockedByRun",   STAMUNIT_OCCURENCES, "Writes blocked by byte RUN bit.");
# endif

    return VINF_SUCCESS;
}

#else  /* !IN_RING3 */

/**
 * @callback_method_impl{PDMDEVREGR0,pfnConstruct}
 */
static DECLCALLBACK(int) hdaRZConstruct(PPDMDEVINS pDevIns)
{
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns); /* this shall come first */
    PHDASTATE pThis = PDMDEVINS_2_DATA(pDevIns, PHDASTATE);

    int rc = PDMDevHlpSetDeviceCritSect(pDevIns, PDMDevHlpCritSectGetNop(pDevIns));
    AssertRCReturn(rc, rc);

    rc = PDMDevHlpMmioSetUpContext(pDevIns, pThis->hMmio, hdaMmioWrite, hdaMmioRead, NULL /*pvUser*/);
    AssertRCReturn(rc, rc);

    return VINF_SUCCESS;
}

#endif /* !IN_RING3 */

/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceHDA =
{
    /* .u32Version = */             PDM_DEVREG_VERSION,
    /* .uReserved0 = */             0,
    /* .szName = */                 "hda",
    /* .fFlags = */                 PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_RZ | PDM_DEVREG_FLAGS_NEW_STYLE,
    /* .fClass = */                 PDM_DEVREG_CLASS_AUDIO,
    /* .cMaxInstances = */          1,
    /* .uSharedVersion = */         42,
    /* .cbInstanceShared = */       sizeof(HDASTATE),
    /* .cbInstanceCC = */           CTX_EXPR(sizeof(HDASTATER3), 0, 0),
    /* .cbInstanceRC = */           0,
    /* .cMaxPciDevices = */         1,
    /* .cMaxMsixVectors = */        0,
    /* .pszDescription = */         "Intel HD Audio Controller",
#if defined(IN_RING3)
    /* .pszRCMod = */               "VBoxDDRC.rc",
    /* .pszR0Mod = */               "VBoxDDR0.r0",
    /* .pfnConstruct = */           hdaR3Construct,
    /* .pfnDestruct = */            hdaR3Destruct,
    /* .pfnRelocate = */            NULL,
    /* .pfnMemSetup = */            NULL,
    /* .pfnPowerOn = */             NULL,
    /* .pfnReset = */               hdaR3Reset,
    /* .pfnSuspend = */             NULL,
    /* .pfnResume = */              NULL,
    /* .pfnAttach = */              hdaR3Attach,
    /* .pfnDetach = */              hdaR3Detach,
    /* .pfnQueryInterface = */      NULL,
    /* .pfnInitComplete = */        NULL,
    /* .pfnPowerOff = */            hdaR3PowerOff,
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
    /* .pfnConstruct = */           hdaRZConstruct,
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
    /* .pfnConstruct = */           hdaRZConstruct,
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

