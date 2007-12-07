/** @file
 *
 * VBox storage devices:
 * ATA/ATAPI controller device (disk and cdrom).
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_IDE
#include <VBox/pdmdev.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#ifdef IN_RING3
# include <iprt/uuid.h>
# include <iprt/semaphore.h>
# include <iprt/thread.h>
# include <iprt/time.h>
# include <iprt/alloc.h>
#endif /* IN_RING3 */
#include <iprt/critsect.h>
#include <iprt/asm.h>
#include <VBox/stam.h>
#include <VBox/mm.h>
#include <VBox/pgm.h>

#include <VBox/scsi.h>

#include "Builtins.h"
#include "PIIX3ATABmDma.h"
#include "ide.h"

/**
 * Maximum number of sectors to transfer in a READ/WRITE MULTIPLE request.
 * Set to 1 to disable multi-sector read support. According to the ATA
 * specification this must be a power of 2 and it must fit in an 8 bit
 * value. Thus the only valid values are 1, 2, 4, 8, 16, 32, 64 and 128.
 */
#define ATA_MAX_MULT_SECTORS 128

/**
 * Fastest PIO mode supported by the drive.
 */
#define ATA_PIO_MODE_MAX 4
/**
 * Fastest MDMA mode supported by the drive.
 */
#define ATA_MDMA_MODE_MAX 2
/**
 * Fastest UDMA mode supported by the drive.
 */
#define ATA_UDMA_MODE_MAX 6


/**
 * The SSM saved state version.
 */
#define ATA_SAVED_STATE_VERSION 16

/** The maximum number of release log entries per device. */
#define MAX_LOG_REL_ERRORS  1024

typedef struct ATADevState {
    /** Flag indicating whether the current command uses LBA48 mode. */
    bool fLBA48;
    /** Flag indicating whether this drive implements the ATAPI command set. */
    bool fATAPI;
    /** Set if this interface has asserted the IRQ. */
    bool fIrqPending;
    /** Currently configured number of sectors in a multi-sector transfer. */
    uint8_t cMultSectors;
    /** PCHS disk geometry. */
    uint32_t cCHSCylinders, cCHSHeads, cCHSSectors;
    /** Total number of sectors on this disk. */
    uint64_t cTotalSectors;
    /** Number of sectors to transfer per IRQ. */
    uint32_t cSectorsPerIRQ;

    /** ATA/ATAPI register 1: feature (write-only). */
    uint8_t uATARegFeature;
    /** ATA/ATAPI register 1: feature, high order byte. */
    uint8_t uATARegFeatureHOB;
    /** ATA/ATAPI register 1: error (read-only). */
    uint8_t uATARegError;
    /** ATA/ATAPI register 2: sector count (read/write). */
    uint8_t uATARegNSector;
    /** ATA/ATAPI register 2: sector count, high order byte. */
    uint8_t uATARegNSectorHOB;
    /** ATA/ATAPI register 3: sector (read/write). */
    uint8_t uATARegSector;
    /** ATA/ATAPI register 3: sector, high order byte. */
    uint8_t uATARegSectorHOB;
    /** ATA/ATAPI register 4: cylinder low (read/write). */
    uint8_t uATARegLCyl;
    /** ATA/ATAPI register 4: cylinder low, high order byte. */
    uint8_t uATARegLCylHOB;
    /** ATA/ATAPI register 5: cylinder high (read/write). */
    uint8_t uATARegHCyl;
    /** ATA/ATAPI register 5: cylinder high, high order byte. */
    uint8_t uATARegHCylHOB;
    /** ATA/ATAPI register 6: select drive/head (read/write). */
    uint8_t uATARegSelect;
    /** ATA/ATAPI register 7: status (read-only). */
    uint8_t uATARegStatus;
    /** ATA/ATAPI register 7: command (write-only). */
    uint8_t uATARegCommand;
    /** ATA/ATAPI drive control register (write-only). */
    uint8_t uATARegDevCtl;

    /** Currently active transfer mode (MDMA/UDMA) and speed. */
    uint8_t uATATransferMode;
    /** Current transfer direction. */
    uint8_t uTxDir;
    /** Index of callback for begin transfer. */
    uint8_t iBeginTransfer;
    /** Index of callback for source/sink of data. */
    uint8_t iSourceSink;
    /** Flag indicating whether the current command transfers data in DMA mode. */
    bool fDMA;
    /** Set to indicate that ATAPI transfer semantics must be used. */
    bool fATAPITransfer;

    /** Total ATA/ATAPI transfer size, shared PIO/DMA. */
    uint32_t cbTotalTransfer;
    /** Elementary ATA/ATAPI transfer size, shared PIO/DMA. */
    uint32_t cbElementaryTransfer;
    /** Current read/write buffer position, shared PIO/DMA. */
    uint32_t iIOBufferCur;
    /** First element beyond end of valid buffer content, shared PIO/DMA. */
    uint32_t iIOBufferEnd;

    /** ATA/ATAPI current PIO read/write transfer position. Not shared with DMA for safety reasons. */
    uint32_t iIOBufferPIODataStart;
    /** ATA/ATAPI current PIO read/write transfer end. Not shared with DMA for safety reasons. */
    uint32_t iIOBufferPIODataEnd;

    /** ATAPI current LBA position. */
    uint32_t iATAPILBA;
    /** ATAPI current sector size. */
    uint32_t cbATAPISector;
    /** ATAPI current command. */
    uint8_t aATAPICmd[ATAPI_PACKET_SIZE];
    /** ATAPI sense key. */
    uint8_t uATAPISenseKey;
    /** ATAPI additional sense code. */
    uint8_t uATAPIASC;
    /** HACK: Countdown till we report a newly unmounted drive as mounted. */
    uint8_t cNotifiedMediaChange;

    /** The status LED state for this drive. */
    PDMLED Led;

    /** Size of I/O buffer. */
    uint32_t cbIOBuffer;
    /** Pointer to the I/O buffer. */
    R3R0PTRTYPE(uint8_t *) pbIOBufferHC;
    /** Pointer to the I/O buffer. */
    GCPTRTYPE(uint8_t *) pbIOBufferGC;
#if HC_ARCH_BITS == 64 && GC_ARCH_BITS != 64
    RTGCPTR Aligmnent0; /**< Align the statistics at an 8-byte boundrary. */
#endif

    /*
     * No data that is part of the saved state after this point!!!!!
     */

    /* Release statistics: number of ATA DMA commands. */
    STAMCOUNTER StatATADMA;
    /* Release statistics: number of ATA PIO commands. */
    STAMCOUNTER StatATAPIO;
    /* Release statistics: number of ATAPI PIO commands. */
    STAMCOUNTER StatATAPIDMA;
    /* Release statistics: number of ATAPI PIO commands. */
    STAMCOUNTER StatATAPIPIO;

    /** Statistics: number of read operations and the time spent reading. */
    STAMPROFILEADV  StatReads;
    /** Statistics: number of bytes read. */
    STAMCOUNTER     StatBytesRead;
    /** Statistics: number of write operations and the time spent writing. */
    STAMPROFILEADV  StatWrites;
    /** Statistics: number of bytes written. */
    STAMCOUNTER     StatBytesWritten;
    /** Statistics: number of flush operations and the time spend flushing. */
    STAMPROFILE     StatFlushes;

    /** Enable passing through commands directly to the ATAPI drive. */
    bool            fATAPIPassthrough;
    /** Number of errors we've reported to the release log.
     * This is to prevent flooding caused by something going horribly wrong.
     * this value against MAX_LOG_REL_ERRORS in places likely to cause floods
     * like the ones we currently seeing on the linux smoke tests (2006-11-10). */
    uint32_t        cErrors;
    /** Timestamp of last started command. 0 if no command pending. */
    uint64_t        u64CmdTS;

    /** Pointer to the attached driver's base interface. */
    R3PTRTYPE(PPDMIBASE)            pDrvBase;
    /** Pointer to the attached driver's block interface. */
    R3PTRTYPE(PPDMIBLOCK)           pDrvBlock;
    /** Pointer to the attached driver's block bios interface. */
    R3PTRTYPE(PPDMIBLOCKBIOS)       pDrvBlockBios;
    /** Pointer to the attached driver's mount interface.
     * This is NULL if the driver isn't a removable unit. */
    R3PTRTYPE(PPDMIMOUNT)           pDrvMount;
    /** The base interface. */
    PDMIBASE                        IBase;
    /** The block port interface. */
    PDMIBLOCKPORT                   IPort;
    /** The mount notify interface. */
    PDMIMOUNTNOTIFY                 IMountNotify;
    /** The LUN #. */
    RTUINT                          iLUN;
#if HC_ARCH_BITS == 64
    RTUINT                          Alignment2; /**< Align pDevInsHC correctly. */
#endif
    /** Pointer to device instance. */
    R3R0PTRTYPE(PPDMDEVINS)             pDevInsHC;
    /** Pointer to controller instance. */
    R3R0PTRTYPE(struct ATACONTROLLER *) pControllerHC;
    /** Pointer to device instance. */
    GCPTRTYPE(PPDMDEVINS)               pDevInsGC;
    /** Pointer to controller instance. */
    GCPTRTYPE(struct ATACONTROLLER *)   pControllerGC;
} ATADevState;


typedef struct ATATransferRequest
{
    uint8_t iIf;
    uint8_t iBeginTransfer;
    uint8_t iSourceSink;
    uint32_t cbTotalTransfer;
    uint8_t uTxDir;
} ATATransferRequest;


typedef struct ATAAbortRequest
{
    uint8_t iIf;
    bool fResetDrive;
} ATAAbortRequest;


typedef enum
{
    /** Begin a new transfer. */
    ATA_AIO_NEW = 0,
    /** Continue a DMA transfer. */
    ATA_AIO_DMA,
    /** Continue a PIO transfer. */
    ATA_AIO_PIO,
    /** Reset the drives on current controller, stop all transfer activity. */
    ATA_AIO_RESET_ASSERTED,
    /** Reset the drives on current controller, resume operation. */
    ATA_AIO_RESET_CLEARED,
    /** Abort the current transfer of a particular drive. */
    ATA_AIO_ABORT
} ATAAIO;


typedef struct ATARequest
{
    ATAAIO ReqType;
    union
    {
        ATATransferRequest t;
        ATAAbortRequest a;
    } u;
} ATARequest;


typedef struct ATACONTROLLER
{
    /** The base of the first I/O Port range. */
    RTIOPORT    IOPortBase1;
    /** The base of the second I/O Port range. (0 if none) */
    RTIOPORT    IOPortBase2;
    /** The assigned IRQ. */
    RTUINT      irq;
    /** Access critical section */
    PDMCRITSECT lock;

    /** Selected drive. */
    uint8_t     iSelectedIf;
    /** The interface on which to handle async I/O. */
    uint8_t     iAIOIf;
    /** The state of the async I/O thread. */
    uint8_t     uAsyncIOState;
    /** Flag indicating whether the next transfer is part of the current command. */
    bool        fChainedTransfer;
    /** Set when the reset processing is currently active on this controller. */
    bool        fReset;
    /** Flag whether the current transfer needs to be redone. */
    bool        fRedo;
    /** Flag whether the redo suspend has been finished. */
    bool        fRedoIdle;
    /** Flag whether the DMA operation to be redone is the final transfer. */
    bool        fRedoDMALastDesc;
    /** The BusMaster DMA state. */
    BMDMAState  BmDma;
    /** Pointer to first DMA descriptor. */
    RTGCPHYS    pFirstDMADesc;
    /** Pointer to last DMA descriptor. */
    RTGCPHYS    pLastDMADesc;
    /** Pointer to current DMA buffer (for redo operations). */
    RTGCPHYS    pRedoDMABuffer;
    /** Size of current DMA buffer (for redo operations). */
    uint32_t    cbRedoDMABuffer;

    /** The ATA/ATAPI interfaces of this controller. */
    ATADevState aIfs[2];

    /** Pointer to device instance. */
    R3R0PTRTYPE(PPDMDEVINS)         pDevInsHC;
    /** Pointer to device instance. */
    GCPTRTYPE(PPDMDEVINS)           pDevInsGC;

    /** Set when the destroying the device instance and the thread must exit. */
    uint32_t volatile   fShutdown;
    /** The async I/O thread handle. NIL_RTTHREAD if no thread. */
    RTTHREAD            AsyncIOThread;
    /** The event semaphore the thread is waiting on for requests. */
    RTSEMEVENT          AsyncIOSem;
    /** The request queue for the AIO thread. One element is always unused. */
    ATARequest          aAsyncIORequests[4];
    /** The position at which to insert a new request for the AIO thread. */
    uint8_t             AsyncIOReqHead;
    /** The position at which to get a new request for the AIO thread. */
    uint8_t             AsyncIOReqTail;
    uint8_t             Alignment3[2]; /**< Explicit padding of the 2 byte gap. */
    /** Magic delay before triggering interrupts in DMA mode. */
    uint32_t            DelayIRQMillies;
    /** The mutex protecting the request queue. */
    RTSEMMUTEX          AsyncIORequestMutex;
    /** The event semaphore the thread is waiting on during suspended I/O. */
    RTSEMEVENT          SuspendIOSem;
#if HC_ARCH_BITS == 32
    uint32_t            Alignment0;
#endif

    /* Statistics */
    STAMCOUNTER StatAsyncOps;
    uint64_t    StatAsyncMinWait;
    uint64_t    StatAsyncMaxWait;
    STAMCOUNTER StatAsyncTimeUS;
    STAMPROFILEADV StatAsyncTime;
    STAMPROFILE StatLockWait;
} ATACONTROLLER, *PATACONTROLLER;

typedef struct PCIATAState {
    PCIDEVICE           dev;
    /** The controllers. */
    ATACONTROLLER       aCts[2];
    /** Pointer to device instance. */
    PPDMDEVINSR3        pDevIns;
    /** Status Port - Base interface. */
    PDMIBASE            IBase;
    /** Status Port - Leds interface. */
    PDMILEDPORTS        ILeds;
    /** Partner of ILeds. */
    R3PTRTYPE(PPDMILEDCONNECTORS)   pLedsConnector;
    /** Flag whether GC is enabled. */
    bool                fGCEnabled;
    /** Flag whether R0 is enabled. */
    bool                fR0Enabled;
    /** Flag indicating whether PIIX4 or PIIX3 is being emulated. */
    bool                fPIIX4;
    bool                Alignment0[HC_ARCH_BITS == 64 ? 5 : 1]; /**< Align the struct size. */
} PCIATAState;

#define ATADEVSTATE_2_CONTROLLER(pIf)          ( (pIf)->CTXSUFF(pController) )
#define ATADEVSTATE_2_DEVINS(pIf)              ( (pIf)->CTXSUFF(pDevIns) )
#define CONTROLLER_2_DEVINS(pController)    ( (pController)->CTXSUFF(pDevIns) )
#define PDMIBASE_2_PCIATASTATE(pInterface)      ( (PCIATAState *)((uintptr_t)(pInterface) - RT_OFFSETOF(PCIATAState, IBase)) )
#define PDMILEDPORTS_2_PCIATASTATE(pInterface)  ( (PCIATAState *)((uintptr_t)(pInterface) - RT_OFFSETOF(PCIATAState, ILeds)) )
#define PDMIBASE_2_ATASTATE(pInterface)         ( (ATADevState *)((uintptr_t)(pInterface) - RT_OFFSETOF(ATADevState, IBase)) )
#define PDMIBLOCKPORT_2_ATASTATE(pInterface)    ( (ATADevState *)((uintptr_t)(pInterface) - RT_OFFSETOF(ATADevState, IPort)) )
#define PDMIMOUNT_2_ATASTATE(pInterface)        ( (ATADevState *)((uintptr_t)(pInterface) - RT_OFFSETOF(ATADevState, IMount)) )
#define PDMIMOUNTNOTIFY_2_ATASTATE(pInterface)  ( (ATADevState *)((uintptr_t)(pInterface) - RT_OFFSETOF(ATADevState, IMountNotify)) )
#define PCIDEV_2_PCIATASTATE(pPciDev)           ( (PCIATAState *)(pPciDev) )

#define ATACONTROLLER_IDX(pController) ( (pController) - PDMINS2DATA(CONTROLLER_2_DEVINS(pController), PCIATAState *)->aCts )


#ifndef VBOX_DEVICE_STRUCT_TESTCASE
/*******************************************************************************
 *  Internal Functions                                                         *
 ******************************************************************************/
__BEGIN_DECLS
PDMBOTHCBDECL(int) ataIOPortWrite1(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb);
PDMBOTHCBDECL(int) ataIOPortRead1(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *u32, unsigned cb);
PDMBOTHCBDECL(int) ataIOPortWriteStr1(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, RTGCPTR *pGCPtrSrc, unsigned *pcTransfer, unsigned cb);
PDMBOTHCBDECL(int) ataIOPortReadStr1(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, RTGCPTR *pGCPtrDst, unsigned *pcTransfer, unsigned cb);
PDMBOTHCBDECL(int) ataIOPortWrite2(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb);
PDMBOTHCBDECL(int) ataIOPortRead2(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *u32, unsigned cb);
PDMBOTHCBDECL(int) ataBMDMAIOPortWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb);
PDMBOTHCBDECL(int) ataBMDMAIOPortRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb);
__END_DECLS



DECLINLINE(void) ataSetStatusValue(ATADevState *s, uint8_t stat)
{
    PATACONTROLLER pCtl = ATADEVSTATE_2_CONTROLLER(s);

    /* Freeze status register contents while processing RESET. */
    if (!pCtl->fReset)
    {
        s->uATARegStatus = stat;
        Log2(("%s: LUN#%d status %#04x\n", __FUNCTION__, s->iLUN, s->uATARegStatus));
    }
}


DECLINLINE(void) ataSetStatus(ATADevState *s, uint8_t stat)
{
    PATACONTROLLER pCtl = ATADEVSTATE_2_CONTROLLER(s);

    /* Freeze status register contents while processing RESET. */
    if (!pCtl->fReset)
    {
        s->uATARegStatus |= stat;
        Log2(("%s: LUN#%d status %#04x\n", __FUNCTION__, s->iLUN, s->uATARegStatus));
    }
}


DECLINLINE(void) ataUnsetStatus(ATADevState *s, uint8_t stat)
{
    PATACONTROLLER pCtl = ATADEVSTATE_2_CONTROLLER(s);

    /* Freeze status register contents while processing RESET. */
    if (!pCtl->fReset)
    {
        s->uATARegStatus &= ~stat;
        Log2(("%s: LUN#%d status %#04x\n", __FUNCTION__, s->iLUN, s->uATARegStatus));
    }
}

#ifdef IN_RING3

typedef void (*PBeginTransferFunc)(ATADevState *);
typedef bool (*PSourceSinkFunc)(ATADevState *);

static void ataReadWriteSectorsBT(ATADevState *);
static void ataPacketBT(ATADevState *);
static void atapiCmdBT(ATADevState *);
static void atapiPassthroughCmdBT(ATADevState *);

static bool ataIdentifySS(ATADevState *);
static bool ataFlushSS(ATADevState *);
static bool ataReadSectorsSS(ATADevState *);
static bool ataWriteSectorsSS(ATADevState *);
static bool ataExecuteDeviceDiagnosticSS(ATADevState *);
static bool ataPacketSS(ATADevState *);
static bool atapiGetConfigurationSS(ATADevState *);
static bool atapiIdentifySS(ATADevState *);
static bool atapiInquirySS(ATADevState *);
static bool atapiMechanismStatusSS(ATADevState *);
static bool atapiModeSenseErrorRecoverySS(ATADevState *);
static bool atapiModeSenseCDStatusSS(ATADevState *);
static bool atapiReadSS(ATADevState *);
static bool atapiReadCapacitySS(ATADevState *);
static bool atapiReadDiscInformationSS(ATADevState *);
static bool atapiReadTOCNormalSS(ATADevState *);
static bool atapiReadTOCMultiSS(ATADevState *);
static bool atapiReadTOCRawSS(ATADevState *);
static bool atapiReadTrackInformationSS(ATADevState *);
static bool atapiRequestSenseSS(ATADevState *);
static bool atapiPassthroughSS(ATADevState *);

/**
 * Begin of transfer function indexes for g_apfnBeginTransFuncs.
 */
typedef enum ATAFNBT
{
    ATAFN_BT_NULL = 0,
    ATAFN_BT_READ_WRITE_SECTORS,
    ATAFN_BT_PACKET,
    ATAFN_BT_ATAPI_CMD,
    ATAFN_BT_ATAPI_PASSTHROUGH_CMD,
    ATAFN_BT_MAX
} ATAFNBT;

/**
 * Array of end transfer functions, the index is ATAFNET.
 * Make sure ATAFNET and this array match!
 */
static const PBeginTransferFunc g_apfnBeginTransFuncs[ATAFN_BT_MAX] =
{
    NULL,
    ataReadWriteSectorsBT,
    ataPacketBT,
    atapiCmdBT,
    atapiPassthroughCmdBT,
};

/**
 * Source/sink function indexes for g_apfnSourceSinkFuncs.
 */
typedef enum ATAFNSS
{
    ATAFN_SS_NULL = 0,
    ATAFN_SS_IDENTIFY,
    ATAFN_SS_FLUSH,
    ATAFN_SS_READ_SECTORS,
    ATAFN_SS_WRITE_SECTORS,
    ATAFN_SS_EXECUTE_DEVICE_DIAGNOSTIC,
    ATAFN_SS_PACKET,
    ATAFN_SS_ATAPI_GET_CONFIGURATION,
    ATAFN_SS_ATAPI_IDENTIFY,
    ATAFN_SS_ATAPI_INQUIRY,
    ATAFN_SS_ATAPI_MECHANISM_STATUS,
    ATAFN_SS_ATAPI_MODE_SENSE_ERROR_RECOVERY,
    ATAFN_SS_ATAPI_MODE_SENSE_CD_STATUS,
    ATAFN_SS_ATAPI_READ,
    ATAFN_SS_ATAPI_READ_CAPACITY,
    ATAFN_SS_ATAPI_READ_DISC_INFORMATION,
    ATAFN_SS_ATAPI_READ_TOC_NORMAL,
    ATAFN_SS_ATAPI_READ_TOC_MULTI,
    ATAFN_SS_ATAPI_READ_TOC_RAW,
    ATAFN_SS_ATAPI_READ_TRACK_INFORMATION,
    ATAFN_SS_ATAPI_REQUEST_SENSE,
    ATAFN_SS_ATAPI_PASSTHROUGH,
    ATAFN_SS_MAX
} ATAFNSS;

/**
 * Array of source/sink functions, the index is ATAFNSS.
 * Make sure ATAFNSS and this array match!
 */
static const PSourceSinkFunc g_apfnSourceSinkFuncs[ATAFN_SS_MAX] =
{
    NULL,
    ataIdentifySS,
    ataFlushSS,
    ataReadSectorsSS,
    ataWriteSectorsSS,
    ataExecuteDeviceDiagnosticSS,
    ataPacketSS,
    atapiGetConfigurationSS,
    atapiIdentifySS,
    atapiInquirySS,
    atapiMechanismStatusSS,
    atapiModeSenseErrorRecoverySS,
    atapiModeSenseCDStatusSS,
    atapiReadSS,
    atapiReadCapacitySS,
    atapiReadDiscInformationSS,
    atapiReadTOCNormalSS,
    atapiReadTOCMultiSS,
    atapiReadTOCRawSS,
    atapiReadTrackInformationSS,
    atapiRequestSenseSS,
    atapiPassthroughSS
};


static const ATARequest ataDMARequest = { ATA_AIO_DMA, };
static const ATARequest ataPIORequest = { ATA_AIO_PIO, };
static const ATARequest ataResetARequest = { ATA_AIO_RESET_ASSERTED, };
static const ATARequest ataResetCRequest = { ATA_AIO_RESET_CLEARED, };


static void ataAsyncIOClearRequests(PATACONTROLLER pCtl)
{
    int rc;

    rc = RTSemMutexRequest(pCtl->AsyncIORequestMutex, RT_INDEFINITE_WAIT);
    AssertRC(rc);
    pCtl->AsyncIOReqHead = 0;
    pCtl->AsyncIOReqTail = 0;
    rc = RTSemMutexRelease(pCtl->AsyncIORequestMutex);
    AssertRC(rc);
}


static void ataAsyncIOPutRequest(PATACONTROLLER pCtl, const ATARequest *pReq)
{
    int rc;

    rc = RTSemMutexRequest(pCtl->AsyncIORequestMutex, RT_INDEFINITE_WAIT);
    AssertRC(rc);
    Assert((pCtl->AsyncIOReqHead + 1) % RT_ELEMENTS(pCtl->aAsyncIORequests) != pCtl->AsyncIOReqTail);
    memcpy(&pCtl->aAsyncIORequests[pCtl->AsyncIOReqHead], pReq, sizeof(*pReq));
    pCtl->AsyncIOReqHead++;
    pCtl->AsyncIOReqHead %= RT_ELEMENTS(pCtl->aAsyncIORequests);
    rc = RTSemMutexRelease(pCtl->AsyncIORequestMutex);
    AssertRC(rc);
    LogBird(("ata: %x: signalling\n", pCtl->IOPortBase1));
    rc = PDMR3CritSectScheduleExitEvent(&pCtl->lock, pCtl->AsyncIOSem);
    if (VBOX_FAILURE(rc))
    {
        LogBird(("ata: %x: schedule failed, rc=%Vrc\n", pCtl->IOPortBase1, rc));
        rc = RTSemEventSignal(pCtl->AsyncIOSem);
        AssertRC(rc);
    }
}


static const ATARequest *ataAsyncIOGetCurrentRequest(PATACONTROLLER pCtl)
{
    int rc;
    const ATARequest *pReq;

    rc = RTSemMutexRequest(pCtl->AsyncIORequestMutex, RT_INDEFINITE_WAIT);
    AssertRC(rc);
    if (pCtl->AsyncIOReqHead != pCtl->AsyncIOReqTail)
        pReq = &pCtl->aAsyncIORequests[pCtl->AsyncIOReqTail];
    else
        pReq = NULL;
    rc = RTSemMutexRelease(pCtl->AsyncIORequestMutex);
    AssertRC(rc);
    return pReq;
}


/**
 * Remove the request with the given type, as it's finished. The request
 * is not removed blindly, as this could mean a RESET request that is not
 * yet processed (but has cleared the request queue) is lost.
 *
 * @param pCtl      Controller for which to remove the request.
 * @param ReqType   Type of the request to remove.
 */
static void ataAsyncIORemoveCurrentRequest(PATACONTROLLER pCtl, ATAAIO ReqType)
{
    int rc;

    rc = RTSemMutexRequest(pCtl->AsyncIORequestMutex, RT_INDEFINITE_WAIT);
    AssertRC(rc);
    if (pCtl->AsyncIOReqHead != pCtl->AsyncIOReqTail && pCtl->aAsyncIORequests[pCtl->AsyncIOReqTail].ReqType == ReqType)
    {
        pCtl->AsyncIOReqTail++;
        pCtl->AsyncIOReqTail %= RT_ELEMENTS(pCtl->aAsyncIORequests);
    }
    rc = RTSemMutexRelease(pCtl->AsyncIORequestMutex);
    AssertRC(rc);
}


/**
 * Dump the request queue for a particular controller. First dump the queue
 * contents, then the already processed entries, as long as they haven't been
 * overwritten.
 *
 * @param pCtl      Controller for which to dump the queue.
 */
static void ataAsyncIODumpRequests(PATACONTROLLER pCtl)
{
    int rc;
    uint8_t curr;

    rc = RTSemMutexRequest(pCtl->AsyncIORequestMutex, RT_INDEFINITE_WAIT);
    AssertRC(rc);
    LogRel(("PIIX3 ATA: Ctl#%d: request queue dump (topmost is current):\n", ATACONTROLLER_IDX(pCtl)));
    curr = pCtl->AsyncIOReqTail;
    do
    {
        if (curr == pCtl->AsyncIOReqHead)
            LogRel(("PIIX3 ATA: Ctl#%d: processed requests (topmost is oldest):\n", ATACONTROLLER_IDX(pCtl)));
        switch (pCtl->aAsyncIORequests[curr].ReqType)
        {
            case ATA_AIO_NEW:
                LogRel(("new transfer request, iIf=%d iBeginTransfer=%d iSourceSink=%d cbTotalTransfer=%d uTxDir=%d\n", pCtl->aAsyncIORequests[curr].u.t.iIf, pCtl->aAsyncIORequests[curr].u.t.iBeginTransfer, pCtl->aAsyncIORequests[curr].u.t.iSourceSink, pCtl->aAsyncIORequests[curr].u.t.cbTotalTransfer, pCtl->aAsyncIORequests[curr].u.t.uTxDir));
                break;
            case ATA_AIO_DMA:
                LogRel(("dma transfer finished\n"));
                break;
            case ATA_AIO_PIO:
                LogRel(("pio transfer finished\n"));
                break;
            case ATA_AIO_RESET_ASSERTED:
                LogRel(("reset asserted request\n"));
                break;
            case ATA_AIO_RESET_CLEARED:
                LogRel(("reset cleared request\n"));
                break;
            case ATA_AIO_ABORT:
                LogRel(("abort request, iIf=%d fResetDrive=%d\n", pCtl->aAsyncIORequests[curr].u.a.iIf,  pCtl->aAsyncIORequests[curr].u.a.fResetDrive));
                break;
            default:
                LogRel(("unknown request %d\n", pCtl->aAsyncIORequests[curr].ReqType));
        }
        curr = (curr + 1) % RT_ELEMENTS(pCtl->aAsyncIORequests);
    } while (curr != pCtl->AsyncIOReqTail);
    rc = RTSemMutexRelease(pCtl->AsyncIORequestMutex);
    AssertRC(rc);
}


/**
 * Checks whether the request queue for a particular controller is empty
 * or whether a particular controller is idle.
 *
 * @param pCtl      Controller for which to check the queue.
 * @param fStrict   If set then the controller is checked to be idle.
 */
static bool ataAsyncIOIsIdle(PATACONTROLLER pCtl, bool fStrict)
{
    int rc;
    bool fIdle;

    rc = RTSemMutexRequest(pCtl->AsyncIORequestMutex, RT_INDEFINITE_WAIT);
    AssertRC(rc);
    fIdle = pCtl->fRedoIdle;
    if (!fIdle)
        fIdle = (pCtl->AsyncIOReqHead == pCtl->AsyncIOReqTail);
    if (fStrict)
        fIdle &= (pCtl->uAsyncIOState == ATA_AIO_NEW);
    rc = RTSemMutexRelease(pCtl->AsyncIORequestMutex);
    AssertRC(rc);
    return fIdle;
}


/**
 * Send a transfer request to the async I/O thread.
 *
 * @param   s                   Pointer to the ATA device state data.
 * @param   cbTotalTransfer     Data transfer size.
 * @param   uTxDir              Data transfer direction.
 * @param   iBeginTransfer      Index of BeginTransfer callback.
 * @param   iSourceSink         Index of SourceSink callback.
 * @param   fChainedTransfer    Whether this is a transfer that is part of the previous command/transfer.
 */
static void ataStartTransfer(ATADevState *s, uint32_t cbTotalTransfer, uint8_t uTxDir, ATAFNBT iBeginTransfer, ATAFNSS iSourceSink, bool fChainedTransfer)
{
    PATACONTROLLER pCtl = ATADEVSTATE_2_CONTROLLER(s);
    ATARequest Req;

    Assert(PDMCritSectIsOwner(&pCtl->lock));

    /* Do not issue new requests while the RESET line is asserted. */
    if (pCtl->fReset)
    {
        Log2(("%s: Ctl#%d: suppressed new request as RESET is active\n", __FUNCTION__, ATACONTROLLER_IDX(pCtl)));
        return;
    }

    /* If the controller is already doing something else right now, ignore
     * the command that is being submitted. Some broken guests issue commands
     * twice (e.g. the Linux kernel that comes with Acronis True Image 8). */
    if (!fChainedTransfer && !ataAsyncIOIsIdle(pCtl, true))
    {
        Log(("%s: Ctl#%d: ignored command %#04x, controller state %d\n", __FUNCTION__, ATACONTROLLER_IDX(pCtl), s->uATARegCommand, pCtl->uAsyncIOState));
        LogRel(("PIIX3 IDE: guest issued command %#04x while controller busy\n", s->uATARegCommand));
        return;
    }

    Req.ReqType = ATA_AIO_NEW;
    if (fChainedTransfer)
        Req.u.t.iIf = pCtl->iAIOIf;
    else
        Req.u.t.iIf = pCtl->iSelectedIf;
    Req.u.t.cbTotalTransfer = cbTotalTransfer;
    Req.u.t.uTxDir = uTxDir;
    Req.u.t.iBeginTransfer = iBeginTransfer;
    Req.u.t.iSourceSink = iSourceSink;
    ataSetStatusValue(s, ATA_STAT_BUSY);
    pCtl->fChainedTransfer = fChainedTransfer;

    /*
     * Kick the worker thread into action.
     */
    Log2(("%s: Ctl#%d: message to async I/O thread, new request\n", __FUNCTION__, ATACONTROLLER_IDX(pCtl)));
    ataAsyncIOPutRequest(pCtl, &Req);
}


/**
 * Send an abort command request to the async I/O thread.
 *
 * @param   s           Pointer to the ATA device state data.
 * @param   fResetDrive Whether to reset the drive or just abort a command.
 */
static void ataAbortCurrentCommand(ATADevState *s, bool fResetDrive)
{
    PATACONTROLLER pCtl = ATADEVSTATE_2_CONTROLLER(s);
    ATARequest Req;

    Assert(PDMCritSectIsOwner(&pCtl->lock));

    /* Do not issue new requests while the RESET line is asserted. */
    if (pCtl->fReset)
    {
        Log2(("%s: Ctl#%d: suppressed aborting command as RESET is active\n", __FUNCTION__, ATACONTROLLER_IDX(pCtl)));
        return;
    }

    Req.ReqType = ATA_AIO_ABORT;
    Req.u.a.iIf = pCtl->iSelectedIf;
    Req.u.a.fResetDrive = fResetDrive;
    ataSetStatus(s, ATA_STAT_BUSY);
    Log2(("%s: Ctl#%d: message to async I/O thread, abort command on LUN#%d\n", __FUNCTION__, ATACONTROLLER_IDX(pCtl), s->iLUN));
    ataAsyncIOPutRequest(pCtl, &Req);
}


static void ataSetIRQ(ATADevState *s)
{
    PATACONTROLLER pCtl = ATADEVSTATE_2_CONTROLLER(s);
    PPDMDEVINS pDevIns = ATADEVSTATE_2_DEVINS(s);

    if (!(s->uATARegDevCtl & ATA_DEVCTL_DISABLE_IRQ))
    {
        Log2(("%s: LUN#%d asserting IRQ\n", __FUNCTION__, s->iLUN));
        /* The BMDMA unit unconditionally sets BM_STATUS_INT if the interrupt
         * line is asserted. It monitors the line for a rising edge. */
        if (!s->fIrqPending)
            pCtl->BmDma.u8Status |= BM_STATUS_INT;
        /* Only actually set the IRQ line if updating the currently selected drive. */
        if (s == &pCtl->aIfs[pCtl->iSelectedIf])
        {
            /** @todo experiment with adaptive IRQ delivery: for reads it is
             * better to wait for IRQ delivery, as it reduces latency. */
            if (pCtl->irq == 16)
                PDMDevHlpPCISetIrqNoWait(pDevIns, 0, 1);
            else
                PDMDevHlpISASetIrqNoWait(pDevIns, pCtl->irq, 1);
        }
    }
    s->fIrqPending = true;
}

#endif /* IN_RING3 */

static void ataUnsetIRQ(ATADevState *s)
{
    PATACONTROLLER pCtl = ATADEVSTATE_2_CONTROLLER(s);
    PPDMDEVINS pDevIns = ATADEVSTATE_2_DEVINS(s);

    if (!(s->uATARegDevCtl & ATA_DEVCTL_DISABLE_IRQ))
    {
        Log2(("%s: LUN#%d deasserting IRQ\n", __FUNCTION__, s->iLUN));
        /* Only actually unset the IRQ line if updating the currently selected drive. */
        if (s == &pCtl->aIfs[pCtl->iSelectedIf])
        {
            if (pCtl->irq == 16)
                PDMDevHlpPCISetIrqNoWait(pDevIns, 0, 0);
            else
                PDMDevHlpISASetIrqNoWait(pDevIns, pCtl->irq, 0);
        }
    }
    s->fIrqPending = false;
}

#ifdef IN_RING3

static void ataPIOTransferStart(ATADevState *s, uint32_t start, uint32_t size)
{
    Log2(("%s: LUN#%d start %d size %d\n", __FUNCTION__, s->iLUN, start, size));
    s->iIOBufferPIODataStart = start;
    s->iIOBufferPIODataEnd = start + size;
    ataSetStatus(s, ATA_STAT_DRQ);
}


static void ataPIOTransferStop(ATADevState *s)
{
    Log2(("%s: LUN#%d\n", __FUNCTION__, s->iLUN));
    if (s->fATAPITransfer)
    {
        s->uATARegNSector = (s->uATARegNSector & ~7) | ATAPI_INT_REASON_IO | ATAPI_INT_REASON_CD;
        Log2(("%s: interrupt reason %#04x\n", __FUNCTION__, s->uATARegNSector));
        ataSetIRQ(s);
        s->fATAPITransfer = false;
    }
    s->cbTotalTransfer = 0;
    s->cbElementaryTransfer = 0;
    s->iIOBufferPIODataStart = 0;
    s->iIOBufferPIODataEnd = 0;
    s->iBeginTransfer = ATAFN_BT_NULL;
    s->iSourceSink = ATAFN_SS_NULL;
}


static void ataPIOTransferLimitATAPI(ATADevState *s)
{
    uint32_t cbLimit, cbTransfer;

    cbLimit = s->uATARegLCyl | (s->uATARegHCyl << 8);
    /* Use maximum transfer size if the guest requested 0. Avoids a hang. */
    if (cbLimit == 0)
        cbLimit = 0xfffe;
    Log2(("%s: byte count limit=%d\n", __FUNCTION__, cbLimit));
    if (cbLimit == 0xffff)
        cbLimit--;
    cbTransfer = RT_MIN(s->cbTotalTransfer, s->iIOBufferEnd - s->iIOBufferCur);
    if (cbTransfer > cbLimit)
    {
        /* Byte count limit for clipping must be even in this case */
        if (cbLimit & 1)
            cbLimit--;
        cbTransfer = cbLimit;
    }
    s->uATARegLCyl = cbTransfer;
    s->uATARegHCyl = cbTransfer >> 8;
    s->cbElementaryTransfer = cbTransfer;
}


static uint32_t ataGetNSectors(ATADevState *s)
{
    /* 0 means either 256 (LBA28) or 65536 (LBA48) sectors. */
    if (s->fLBA48)
    {
        if (!s->uATARegNSector && !s->uATARegNSectorHOB)
            return 65536;
        else
            return s->uATARegNSectorHOB << 8 | s->uATARegNSector;
    }
    else
    {
        if (!s->uATARegNSector)
            return 256;
        else
            return s->uATARegNSector;
    }
}


static void ataPadString(uint8_t *pbDst, const char *pbSrc, uint32_t cbSize)
{
    for (uint32_t i = 0; i < cbSize; i++)
    {
        if (*pbSrc)
            pbDst[i ^ 1] = *pbSrc++;
        else
            pbDst[i ^ 1] = ' ';
    }
}


static void ataSCSIPadStr(uint8_t *pbDst, const char *pbSrc, uint32_t cbSize)
{
    for (uint32_t i = 0; i < cbSize; i++)
    {
        if (*pbSrc)
            pbDst[i] = *pbSrc++;
        else
            pbDst[i] = ' ';
    }
}


DECLINLINE(void) ataH2BE_U16(uint8_t *pbBuf, uint16_t val)
{
    pbBuf[0] = val >> 8;
    pbBuf[1] = val;
}


DECLINLINE(void) ataH2BE_U24(uint8_t *pbBuf, uint32_t val)
{
    pbBuf[0] = val >> 16;
    pbBuf[1] = val >> 8;
    pbBuf[2] = val;
}


DECLINLINE(void) ataH2BE_U32(uint8_t *pbBuf, uint32_t val)
{
    pbBuf[0] = val >> 24;
    pbBuf[1] = val >> 16;
    pbBuf[2] = val >> 8;
    pbBuf[3] = val;
}


DECLINLINE(uint16_t) ataBE2H_U16(const uint8_t *pbBuf)
{
    return (pbBuf[0] << 8) | pbBuf[1];
}


DECLINLINE(uint32_t) ataBE2H_U24(const uint8_t *pbBuf)
{
    return (pbBuf[0] << 16) | (pbBuf[1] << 8) | pbBuf[2];
}


DECLINLINE(uint32_t) ataBE2H_U32(const uint8_t *pbBuf)
{
    return (pbBuf[0] << 24) | (pbBuf[1] << 16) | (pbBuf[2] << 8) | pbBuf[3];
}


DECLINLINE(void) ataLBA2MSF(uint8_t *pbBuf, uint32_t iATAPILBA)
{
    iATAPILBA += 150;
    pbBuf[0] = (iATAPILBA / 75) / 60;
    pbBuf[1] = (iATAPILBA / 75) % 60;
    pbBuf[2] = iATAPILBA % 75;
}


DECLINLINE(uint32_t) ataMSF2LBA(const uint8_t *pbBuf)
{
    return (pbBuf[0] * 60 + pbBuf[1]) * 75 + pbBuf[2];
}


static void ataCmdOK(ATADevState *s, uint8_t status)
{
    s->uATARegError = 0; /* Not needed by ATA spec, but cannot hurt. */
    ataSetStatusValue(s, ATA_STAT_READY | status);
}


static void ataCmdError(ATADevState *s, uint8_t uErrorCode)
{
    Log(("%s: code=%#x\n", __FUNCTION__, uErrorCode));
    s->uATARegError = uErrorCode;
    ataSetStatusValue(s, ATA_STAT_READY | ATA_STAT_ERR);
    s->cbTotalTransfer = 0;
    s->cbElementaryTransfer = 0;
    s->iIOBufferCur = 0;
    s->iIOBufferEnd = 0;
    s->uTxDir = PDMBLOCKTXDIR_NONE;
    s->iBeginTransfer = ATAFN_BT_NULL;
    s->iSourceSink = ATAFN_SS_NULL;
}


static bool ataIdentifySS(ATADevState *s)
{
    uint16_t *p;
    char aSerial[20];
    int rc;
    RTUUID Uuid;

    Assert(s->uTxDir == PDMBLOCKTXDIR_FROM_DEVICE);
    Assert(s->cbElementaryTransfer == 512);
    rc = s->pDrvBlock ? s->pDrvBlock->pfnGetUuid(s->pDrvBlock, &Uuid) : RTUuidClear(&Uuid);
    if (VBOX_FAILURE(rc) || RTUuidIsNull(&Uuid))
    {
        PATACONTROLLER pCtl = ATADEVSTATE_2_CONTROLLER(s);
        /* Generate a predictable serial for drives which don't have a UUID. */
        RTStrPrintf(aSerial, sizeof(aSerial), "VB%x-%04x%04x",
                    s->iLUN + ATADEVSTATE_2_DEVINS(s)->iInstance * 32,
                    pCtl->IOPortBase1, pCtl->IOPortBase2);
    }
    else
        RTStrPrintf(aSerial, sizeof(aSerial), "VB%08x-%08x", Uuid.au32[0], Uuid.au32[3]);

    p = (uint16_t *)s->CTXSUFF(pbIOBuffer);
    memset(p, 0, 512);
    p[0] = RT_H2LE_U16(0x0040);
    p[1] = RT_H2LE_U16(RT_MIN(s->cCHSCylinders, 16383));
    p[3] = RT_H2LE_U16(s->cCHSHeads);
    /* Block size; obsolete, but required for the BIOS. */
    p[5] = RT_H2LE_U16(512);
    p[6] = RT_H2LE_U16(s->cCHSSectors);
    ataPadString((uint8_t *)(p + 10), aSerial, 20); /* serial number */
    p[20] = RT_H2LE_U16(3); /* XXX: retired, cache type */
    p[21] = RT_H2LE_U16(512); /* XXX: retired, cache size in sectors */
    p[22] = RT_H2LE_U16(0); /* ECC bytes per sector */
    ataPadString((uint8_t *)(p + 23), "1.0", 8); /* firmware version */
    ataPadString((uint8_t *)(p + 27), "VBOX HARDDISK", 40); /* model */
#if ATA_MAX_MULT_SECTORS > 1
    p[47] = RT_H2LE_U16(0x8000 | ATA_MAX_MULT_SECTORS);
#endif
    p[48] = RT_H2LE_U16(1); /* dword I/O, used by the BIOS */
    p[49] = RT_H2LE_U16(1 << 11 | 1 << 9 | 1 << 8); /* DMA and LBA supported */
    p[50] = RT_H2LE_U16(1 << 14); /* No drive specific standby timer minimum */
    p[51] = RT_H2LE_U16(240); /* PIO transfer cycle */
    p[52] = RT_H2LE_U16(240); /* DMA transfer cycle */
    p[53] = RT_H2LE_U16(1 | 1 << 1 | 1 << 2); /* words 54-58,64-70,88 valid */
    p[54] = RT_H2LE_U16(RT_MIN(s->cCHSCylinders, 16383));
    p[55] = RT_H2LE_U16(s->cCHSHeads);
    p[56] = RT_H2LE_U16(s->cCHSSectors);
    p[57] = RT_H2LE_U16(RT_MIN(s->cCHSCylinders, 16383) * s->cCHSHeads * s->cCHSSectors);
    p[58] = RT_H2LE_U16(RT_MIN(s->cCHSCylinders, 16383) * s->cCHSHeads * s->cCHSSectors >> 16);
    if (s->cMultSectors)
        p[59] = RT_H2LE_U16(0x100 | s->cMultSectors);
    if (s->cTotalSectors <= (1 << 28) - 1)
    {
        p[60] = RT_H2LE_U16(s->cTotalSectors);
        p[61] = RT_H2LE_U16(s->cTotalSectors >> 16);
    }
    else
    {
        /* Report maximum number of sectors possible with LBA28 */
        p[60] = RT_H2LE_U16(((1 << 28) - 1) & 0xffff);
        p[61] = RT_H2LE_U16(((1 << 28) - 1) >> 16);
    }
    p[63] = RT_H2LE_U16(ATA_TRANSFER_ID(ATA_MODE_MDMA, ATA_MDMA_MODE_MAX, s->uATATransferMode)); /* MDMA modes supported / mode enabled */
    p[64] = RT_H2LE_U16(ATA_PIO_MODE_MAX > 2 ? (1 << (ATA_PIO_MODE_MAX - 2)) - 1 : 0); /* PIO modes beyond PIO2 supported */
    p[65] = RT_H2LE_U16(120); /* minimum DMA multiword tx cycle time */
    p[66] = RT_H2LE_U16(120); /* recommended DMA multiword tx cycle time */
    p[67] = RT_H2LE_U16(120); /* minimum PIO cycle time without flow control */
    p[68] = RT_H2LE_U16(120); /* minimum PIO cycle time with IORDY flow control */
    p[80] = RT_H2LE_U16(0x7e); /* support everything up to ATA/ATAPI-6 */
    p[81] = RT_H2LE_U16(0x22); /* conforms to ATA/ATAPI-6 */
    p[82] = RT_H2LE_U16(1 << 3 | 1 << 5 | 1 << 6); /* supports power management,  write cache and look-ahead */
    if (s->cTotalSectors <= (1 << 28) - 1)
        p[83] = RT_H2LE_U16(1 << 14 | 1 << 12); /* supports FLUSH CACHE */
    else
        p[83] = RT_H2LE_U16(1 << 14 | 1 << 10 | 1 << 12 | 1 << 13); /* supports LBA48, FLUSH CACHE and FLUSH CACHE EXT */
    p[84] = RT_H2LE_U16(1 << 14);
    p[85] = RT_H2LE_U16(1 << 3 | 1 << 5 | 1 << 6); /* enabled power management,  write cache and look-ahead */
    if (s->cTotalSectors <= (1 << 28) - 1)
        p[86] = RT_H2LE_U16(1 << 12); /* enabled FLUSH CACHE */
    else
        p[86] = RT_H2LE_U16(1 << 10 | 1 << 12 | 1 << 13); /* enabled LBA48, FLUSH CACHE and FLUSH CACHE EXT */
    p[87] = RT_H2LE_U16(1 << 14);
    p[88] = RT_H2LE_U16(ATA_TRANSFER_ID(ATA_MODE_UDMA, ATA_UDMA_MODE_MAX, s->uATATransferMode)); /* UDMA modes supported / mode enabled */
    p[93] = RT_H2LE_U16((1 | 1 << 1) << ((s->iLUN & 1) == 0 ? 0 : 8) | 1 << 13 | 1 << 14);
    if (s->cTotalSectors > (1 << 28) - 1)
    {
        p[100] = RT_H2LE_U16(s->cTotalSectors);
        p[101] = RT_H2LE_U16(s->cTotalSectors >> 16);
        p[102] = RT_H2LE_U16(s->cTotalSectors >> 32);
        p[103] = RT_H2LE_U16(s->cTotalSectors >> 48);
    }
    s->iSourceSink = ATAFN_SS_NULL;
    ataCmdOK(s, ATA_STAT_SEEK);
    return false;
}


static bool ataFlushSS(ATADevState *s)
{
    PATACONTROLLER pCtl = ATADEVSTATE_2_CONTROLLER(s);
    int rc;

    Assert(s->uTxDir == PDMBLOCKTXDIR_NONE);
    Assert(!s->cbElementaryTransfer);

    PDMCritSectLeave(&pCtl->lock);

    STAM_PROFILE_START(&s->StatFlushes, f);
    rc = s->pDrvBlock->pfnFlush(s->pDrvBlock);
    AssertRC(rc);
    STAM_PROFILE_STOP(&s->StatFlushes, f);

    STAM_PROFILE_START(&pCtl->StatLockWait, a);
    PDMCritSectEnter(&pCtl->lock, VINF_SUCCESS);
    STAM_PROFILE_STOP(&pCtl->StatLockWait, a);
    ataCmdOK(s, 0);
    return false;
}


static bool atapiIdentifySS(ATADevState *s)
{
    uint16_t *p;
    char aSerial[20];
    RTUUID Uuid;
    int rc;

    Assert(s->uTxDir == PDMBLOCKTXDIR_FROM_DEVICE);
    Assert(s->cbElementaryTransfer == 512);
    rc = s->pDrvBlock ? s->pDrvBlock->pfnGetUuid(s->pDrvBlock, &Uuid) : RTUuidClear(&Uuid);
    if (VBOX_FAILURE(rc) || RTUuidIsNull(&Uuid))
    {
        PATACONTROLLER pCtl = ATADEVSTATE_2_CONTROLLER(s);
        /* Generate a predictable serial for drives which don't have a UUID. */
        RTStrPrintf(aSerial, sizeof(aSerial), "VB%x-%04x%04x",
                    s->iLUN + ATADEVSTATE_2_DEVINS(s)->iInstance * 32,
                    pCtl->IOPortBase1, pCtl->IOPortBase2);
    }
    else
        RTStrPrintf(aSerial, sizeof(aSerial), "VB%08x-%08x", Uuid.au32[0], Uuid.au32[3]);

    p = (uint16_t *)s->CTXSUFF(pbIOBuffer);
    memset(p, 0, 512);
    /* Removable CDROM, 50us response, 12 byte packets */
    p[0] = RT_H2LE_U16(2 << 14 | 5 << 8 | 1 << 7 | 2 << 5 | 0 << 0);
    ataPadString((uint8_t *)(p + 10), aSerial, 20); /* serial number */
    p[20] = RT_H2LE_U16(3); /* XXX: retired, cache type */
    p[21] = RT_H2LE_U16(512); /* XXX: retired, cache size in sectors */
    ataPadString((uint8_t *)(p + 23), "1.0", 8); /* firmware version */
    ataPadString((uint8_t *)(p + 27), "VBOX CD-ROM", 40); /* model */
    p[49] = RT_H2LE_U16(1 << 11 | 1 << 9 | 1 << 8); /* DMA and LBA supported */
    p[50] = RT_H2LE_U16(1 << 14);  /* No drive specific standby timer minimum */
    p[51] = RT_H2LE_U16(240); /* PIO transfer cycle */
    p[52] = RT_H2LE_U16(240); /* DMA transfer cycle */
    p[53] = RT_H2LE_U16(1 << 1 | 1 << 2); /* words 64-70,88 are valid */
    p[63] = RT_H2LE_U16(ATA_TRANSFER_ID(ATA_MODE_MDMA, ATA_MDMA_MODE_MAX, s->uATATransferMode)); /* MDMA modes supported / mode enabled */
    p[64] = RT_H2LE_U16(ATA_PIO_MODE_MAX > 2 ? (1 << (ATA_PIO_MODE_MAX - 2)) - 1 : 0); /* PIO modes beyond PIO2 supported */
    p[65] = RT_H2LE_U16(120); /* minimum DMA multiword tx cycle time */
    p[66] = RT_H2LE_U16(120); /* recommended DMA multiword tx cycle time */
    p[67] = RT_H2LE_U16(120); /* minimum PIO cycle time without flow control */
    p[68] = RT_H2LE_U16(120); /* minimum PIO cycle time with IORDY flow control */
    p[73] = RT_H2LE_U16(0x003e); /* ATAPI CDROM major */
    p[74] = RT_H2LE_U16(9); /* ATAPI CDROM minor */
    p[75] = RT_H2LE_U16(1); /* queue depth 1 */
    p[80] = RT_H2LE_U16(0x7e); /* support everything up to ATA/ATAPI-6 */
    p[81] = RT_H2LE_U16(0x22); /* conforms to ATA/ATAPI-6 */
    p[82] = RT_H2LE_U16(1 << 4 | 1 << 9); /* supports packet command set and DEVICE RESET */
    p[83] = RT_H2LE_U16(1 << 14);
    p[84] = RT_H2LE_U16(1 << 14);
    p[85] = RT_H2LE_U16(1 << 4 | 1 << 9); /* enabled packet command set and DEVICE RESET */
    p[86] = RT_H2LE_U16(0);
    p[87] = RT_H2LE_U16(1 << 14);
    p[88] = RT_H2LE_U16(ATA_TRANSFER_ID(ATA_MODE_UDMA, ATA_UDMA_MODE_MAX, s->uATATransferMode)); /* UDMA modes supported / mode enabled */
    p[93] = RT_H2LE_U16((1 | 1 << 1) << ((s->iLUN & 1) == 0 ? 0 : 8) | 1 << 13 | 1 << 14);
    s->iSourceSink = ATAFN_SS_NULL;
    ataCmdOK(s, ATA_STAT_SEEK);
    return false;
}


static void ataSetSignature(ATADevState *s)
{
    s->uATARegSelect &= 0xf0; /* clear head */
    /* put signature */
    s->uATARegNSector = 1;
    s->uATARegSector = 1;
    if (s->fATAPI)
    {
        s->uATARegLCyl = 0x14;
        s->uATARegHCyl = 0xeb;
    }
    else if (s->pDrvBlock)
    {
        s->uATARegLCyl = 0;
        s->uATARegHCyl = 0;
    }
    else
    {
        s->uATARegLCyl = 0xff;
        s->uATARegHCyl = 0xff;
    }
}


static uint64_t ataGetSector(ATADevState *s)
{
    uint64_t iLBA;
    if (s->uATARegSelect & 0x40)
    {
        /* any LBA variant */
        if (s->fLBA48)
        {
            /* LBA48 */
            iLBA = ((uint64_t)s->uATARegHCylHOB << 40) |
                ((uint64_t)s->uATARegLCylHOB << 32) |
                ((uint64_t)s->uATARegSectorHOB << 24) |
                ((uint64_t)s->uATARegHCyl << 16) |
                ((uint64_t)s->uATARegLCyl << 8) |
                s->uATARegSector;
        }
        else
        {
            /* LBA */
            iLBA = ((s->uATARegSelect & 0x0f) << 24) | (s->uATARegHCyl << 16) |
                (s->uATARegLCyl << 8) | s->uATARegSector;
        }
    }
    else
    {
        /* CHS */
        iLBA = ((s->uATARegHCyl << 8) | s->uATARegLCyl) * s->cCHSHeads * s->cCHSSectors +
            (s->uATARegSelect & 0x0f) * s->cCHSSectors +
            (s->uATARegSector - 1);
    }
    return iLBA;
}

static void ataSetSector(ATADevState *s, uint64_t iLBA)
{
    uint32_t cyl, r;
    if (s->uATARegSelect & 0x40)
    {
        /* any LBA variant */
        if (s->fLBA48)
        {
            /* LBA48 */
            s->uATARegHCylHOB = iLBA >> 40;
            s->uATARegLCylHOB = iLBA >> 32;
            s->uATARegSectorHOB = iLBA >> 24;
            s->uATARegHCyl = iLBA >> 16;
            s->uATARegLCyl = iLBA >> 8;
            s->uATARegSector = iLBA;
        }
        else
        {
            /* LBA */
            s->uATARegSelect = (s->uATARegSelect & 0xf0) | (iLBA >> 24);
            s->uATARegHCyl = (iLBA >> 16);
            s->uATARegLCyl = (iLBA >> 8);
            s->uATARegSector = (iLBA);
        }
    }
    else
    {
        /* CHS */
        cyl = iLBA / (s->cCHSHeads * s->cCHSSectors);
        r = iLBA % (s->cCHSHeads * s->cCHSSectors);
        s->uATARegHCyl = cyl >> 8;
        s->uATARegLCyl = cyl;
        s->uATARegSelect = (s->uATARegSelect & 0xf0) | ((r / s->cCHSSectors) & 0x0f);
        s->uATARegSector = (r % s->cCHSSectors) + 1;
    }
}


static int ataReadSectors(ATADevState *s, uint64_t u64Sector, void *pvBuf, uint32_t cSectors)
{
    PATACONTROLLER pCtl = ATADEVSTATE_2_CONTROLLER(s);
    int rc;

    PDMCritSectLeave(&pCtl->lock);

    STAM_PROFILE_ADV_START(&s->StatReads, r);
    s->Led.Asserted.s.fReading = s->Led.Actual.s.fReading = 1;
    rc = s->pDrvBlock->pfnRead(s->pDrvBlock, u64Sector * 512, pvBuf, cSectors * 512);
    s->Led.Actual.s.fReading = 0;
    STAM_PROFILE_ADV_STOP(&s->StatReads, r);

    STAM_COUNTER_ADD(&s->StatBytesRead, cSectors * 512);

    STAM_PROFILE_START(&pCtl->StatLockWait, a);
    PDMCritSectEnter(&pCtl->lock, VINF_SUCCESS);
    STAM_PROFILE_STOP(&pCtl->StatLockWait, a);
    return rc;
}


static int ataWriteSectors(ATADevState *s, uint64_t u64Sector, const void *pvBuf, uint32_t cSectors)
{
    PATACONTROLLER pCtl = ATADEVSTATE_2_CONTROLLER(s);
    int rc;

    PDMCritSectLeave(&pCtl->lock);

    STAM_PROFILE_ADV_START(&s->StatWrites, w);
    s->Led.Asserted.s.fWriting = s->Led.Actual.s.fWriting = 1;
    rc = s->pDrvBlock->pfnWrite(s->pDrvBlock, u64Sector * 512, pvBuf, cSectors * 512);
    s->Led.Actual.s.fWriting = 0;
    STAM_PROFILE_ADV_STOP(&s->StatWrites, w);

    STAM_COUNTER_ADD(&s->StatBytesWritten, cSectors * 512);

    STAM_PROFILE_START(&pCtl->StatLockWait, a);
    PDMCritSectEnter(&pCtl->lock, VINF_SUCCESS);
    STAM_PROFILE_STOP(&pCtl->StatLockWait, a);
    return rc;
}


static void ataReadWriteSectorsBT(ATADevState *s)
{
    uint32_t cSectors;

    cSectors = s->cbTotalTransfer / 512;
    if (cSectors > s->cSectorsPerIRQ)
        s->cbElementaryTransfer = s->cSectorsPerIRQ * 512;
    else
        s->cbElementaryTransfer = cSectors * 512;
    if (s->uTxDir == PDMBLOCKTXDIR_TO_DEVICE)
        ataCmdOK(s, 0);
}


static void ataWarningDiskFull(PPDMDEVINS pDevIns)
{
    int rc;
    LogRel(("PIIX3 ATA: Host disk full\n"));
    rc = VMSetRuntimeError(PDMDevHlpGetVM(pDevIns),
                           false, "DevATA_DISKFULL",
                           N_("Host system reported disk full. VM execution is suspended. You can resume after freeing some space"));
    AssertRC(rc);
}


static void ataWarningFileTooBig(PPDMDEVINS pDevIns)
{
    int rc;
    LogRel(("PIIX3 ATA: File too big\n"));
    rc = VMSetRuntimeError(PDMDevHlpGetVM(pDevIns),
                           false, "DevATA_FILETOOBIG",
                           N_("Host system reported that the file size limit of the host file system has been exceeded. VM execution is suspended. You need to move your virtual hard disk to a filesystem which allows bigger files"));
    AssertRC(rc);
}


static void ataWarningISCSI(PPDMDEVINS pDevIns)
{
    int rc;
    LogRel(("PIIX3 ATA: iSCSI target unavailable\n"));
    rc = VMSetRuntimeError(PDMDevHlpGetVM(pDevIns),
                           false, "DevATA_ISCSIDOWN",
                           N_("The iSCSI target has stopped responding. VM execution is suspended. You can resume when it is available again"));
    AssertRC(rc);
}


static bool ataReadSectorsSS(ATADevState *s)
{
    int rc;
    uint32_t cSectors;
    uint64_t iLBA;

    cSectors = s->cbElementaryTransfer / 512;
    Assert(cSectors);
    iLBA = ataGetSector(s);
    Log(("%s: %d sectors at LBA %d\n", __FUNCTION__, cSectors, iLBA));
    rc = ataReadSectors(s, iLBA, s->CTXSUFF(pbIOBuffer), cSectors);
    if (VBOX_SUCCESS(rc))
    {
        ataSetSector(s, iLBA + cSectors);
        if (s->cbElementaryTransfer == s->cbTotalTransfer)
            s->iSourceSink = ATAFN_SS_NULL;
        ataCmdOK(s, ATA_STAT_SEEK);
    }
    else
    {
        if (rc == VERR_DISK_FULL)
        {
            ataWarningDiskFull(ATADEVSTATE_2_DEVINS(s));
            return true;
        }
        if (rc == VERR_FILE_TOO_BIG)
        {
            ataWarningFileTooBig(ATADEVSTATE_2_DEVINS(s));
            return true;
        }
        if (rc == VERR_BROKEN_PIPE || rc == VERR_NET_CONNECTION_REFUSED)
        {
            /* iSCSI connection abort (first error) or failure to reestablish
             * connection (second error). Pause VM. On resume we'll retry. */
            ataWarningISCSI(ATADEVSTATE_2_DEVINS(s));
            return true;
        }
        if (s->cErrors++ < MAX_LOG_REL_ERRORS)
            LogRel(("PIIX3 ATA: LUN#%d: disk read error (rc=%Vrc iSector=%#RX64 cSectors=%#RX32)\n",
                    s->iLUN, rc, iLBA, cSectors));
        ataCmdError(s, ID_ERR);
    }
    /** @todo implement redo for iSCSI */
    return false;
}


static bool ataWriteSectorsSS(ATADevState *s)
{
    int rc;
    uint32_t cSectors;
    uint64_t iLBA;

    cSectors = s->cbElementaryTransfer / 512;
    Assert(cSectors);
    iLBA = ataGetSector(s);
    Log(("%s: %d sectors at LBA %d\n", __FUNCTION__, cSectors, iLBA));
    rc = ataWriteSectors(s, iLBA, s->CTXSUFF(pbIOBuffer), cSectors);
    if (VBOX_SUCCESS(rc))
    {
        ataSetSector(s, iLBA + cSectors);
        if (!s->cbTotalTransfer)
            s->iSourceSink = ATAFN_SS_NULL;
        ataCmdOK(s, ATA_STAT_SEEK);
    }
    else
    {
        if (rc == VERR_DISK_FULL)
        {
            ataWarningDiskFull(ATADEVSTATE_2_DEVINS(s));
            return true;
        }
        if (rc == VERR_FILE_TOO_BIG)
        {
            ataWarningFileTooBig(ATADEVSTATE_2_DEVINS(s));
            return true;
        }
        if (rc == VERR_BROKEN_PIPE || rc == VERR_NET_CONNECTION_REFUSED)
        {
            /* iSCSI connection abort (first error) or failure to reestablish
             * connection (second error). Pause VM. On resume we'll retry. */
            ataWarningISCSI(ATADEVSTATE_2_DEVINS(s));
            return true;
        }
        if (s->cErrors++ < MAX_LOG_REL_ERRORS)
            LogRel(("PIIX3 ATA: LUN#%d: disk write error (rc=%Vrc iSector=%#RX64 cSectors=%#RX32)\n",
                    s->iLUN, rc, iLBA, cSectors));
        ataCmdError(s, ID_ERR);
    }
    /** @todo implement redo for iSCSI */
    return false;
}


static void atapiCmdOK(ATADevState *s)
{
    s->uATARegError = 0;
    ataSetStatusValue(s, ATA_STAT_READY);
    s->uATARegNSector = (s->uATARegNSector & ~7)
        | ((s->uTxDir != PDMBLOCKTXDIR_TO_DEVICE) ? ATAPI_INT_REASON_IO : 0)
        | (!s->cbTotalTransfer ? ATAPI_INT_REASON_CD : 0);
    Log2(("%s: interrupt reason %#04x\n", __FUNCTION__, s->uATARegNSector));
    s->uATAPISenseKey = SCSI_SENSE_NONE;
    s->uATAPIASC = SCSI_ASC_NONE;
}


static void atapiCmdError(ATADevState *s, uint8_t uATAPISenseKey, uint8_t uATAPIASC)
{
    Log(("%s: sense=%#x asc=%#x\n", __FUNCTION__, uATAPISenseKey, uATAPIASC));
    s->uATARegError = uATAPISenseKey << 4;
    ataSetStatusValue(s, ATA_STAT_READY | ATA_STAT_ERR);
    s->uATARegNSector = (s->uATARegNSector & ~7) | ATAPI_INT_REASON_IO | ATAPI_INT_REASON_CD;
    Log2(("%s: interrupt reason %#04x\n", __FUNCTION__, s->uATARegNSector));
    s->uATAPISenseKey = uATAPISenseKey;
    s->uATAPIASC = uATAPIASC;
    s->cbTotalTransfer = 0;
    s->cbElementaryTransfer = 0;
    s->iIOBufferCur = 0;
    s->iIOBufferEnd = 0;
    s->uTxDir = PDMBLOCKTXDIR_NONE;
    s->iBeginTransfer = ATAFN_BT_NULL;
    s->iSourceSink = ATAFN_SS_NULL;
}


static void atapiCmdBT(ATADevState *s)
{
    s->fATAPITransfer = true;
    s->cbElementaryTransfer = s->cbTotalTransfer;
    if (s->uTxDir == PDMBLOCKTXDIR_TO_DEVICE)
        atapiCmdOK(s);
}


static void atapiPassthroughCmdBT(ATADevState *s)
{
    /* @todo implement an algorithm for correctly determining the read and
     * write sector size without sending additional commands to the drive.
     * This should be doable by saving processing the configuration requests
     * and replies. */
#if 0
    if (s->uTxDir == PDMBLOCKTXDIR_TO_DEVICE)
    {
        uint8_t cmd = s->aATAPICmd[0];
        if (cmd == SCSI_WRITE_10 || cmd == SCSI_WRITE_12 || cmd == SCSI_WRITE_AND_VERIFY_10)
        {
            uint8_t aModeSenseCmd[10];
            uint8_t aModeSenseResult[16];
            uint8_t uDummySense;
            uint32_t cbTransfer;
            int rc;

            cbTransfer = sizeof(aModeSenseResult);
            aModeSenseCmd[0] = SCSI_MODE_SENSE_10;
            aModeSenseCmd[1] = 0x08; /* disable block descriptor = 1 */
            aModeSenseCmd[2] = (SCSI_PAGECONTROL_CURRENT << 6) | SCSI_MODEPAGE_WRITE_PARAMETER;
            aModeSenseCmd[3] = 0; /* subpage code */
            aModeSenseCmd[4] = 0; /* reserved */
            aModeSenseCmd[5] = 0; /* reserved */
            aModeSenseCmd[6] = 0; /* reserved */
            aModeSenseCmd[7] = cbTransfer >> 8;
            aModeSenseCmd[8] = cbTransfer & 0xff;
            aModeSenseCmd[9] = 0; /* control */
            rc = s->pDrvBlock->pfnSendCmd(s->pDrvBlock, aModeSenseCmd, PDMBLOCKTXDIR_FROM_DEVICE, aModeSenseResult, &cbTransfer, &uDummySense, 500);
            if (VBOX_FAILURE(rc))
            {
                atapiCmdError(s, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_NONE);
                return;
            }
            /* Select sector size based on the current data block type. */
            switch (aModeSenseResult[12] & 0x0f)
            {
                case 0:
                    s->cbATAPISector = 2352;
                    break;
                case 1:
                    s->cbATAPISector = 2368;
                    break;
                case 2:
                case 3:
                    s->cbATAPISector = 2448;
                    break;
                case 8:
                case 10:
                    s->cbATAPISector = 2048;
                    break;
                case 9:
                    s->cbATAPISector = 2336;
                    break;
                case 11:
                    s->cbATAPISector = 2056;
                    break;
                case 12:
                    s->cbATAPISector = 2324;
                    break;
                case 13:
                    s->cbATAPISector = 2332;
                    break;
                default:
                    s->cbATAPISector = 0;
            }
            Log2(("%s: sector size %d\n", __FUNCTION__, s->cbATAPISector));
            s->cbTotalTransfer *= s->cbATAPISector;
            if (s->cbTotalTransfer == 0)
                s->uTxDir = PDMBLOCKTXDIR_NONE;
        }
    }
#endif
    atapiCmdBT(s);
}


static bool atapiReadSS(ATADevState *s)
{
    PATACONTROLLER pCtl = ATADEVSTATE_2_CONTROLLER(s);
    int rc = VINF_SUCCESS;
    uint32_t cbTransfer, cSectors;

    Assert(s->uTxDir == PDMBLOCKTXDIR_FROM_DEVICE);
    cbTransfer = RT_MIN(s->cbTotalTransfer, s->cbIOBuffer);
    cSectors = cbTransfer / s->cbATAPISector;
    Assert(cSectors * s->cbATAPISector <= cbTransfer);
    Log(("%s: %d sectors at LBA %d\n", __FUNCTION__, cSectors, s->iATAPILBA));

    PDMCritSectLeave(&pCtl->lock);

    STAM_PROFILE_ADV_START(&s->StatReads, r);
    s->Led.Asserted.s.fReading = s->Led.Actual.s.fReading = 1;
    switch (s->cbATAPISector)
    {
        case 2048:
            rc = s->pDrvBlock->pfnRead(s->pDrvBlock, (uint64_t)s->iATAPILBA * s->cbATAPISector, s->CTXSUFF(pbIOBuffer), s->cbATAPISector * cSectors);
            break;
        case 2352:
            {
                uint8_t *pbBuf = s->CTXSUFF(pbIOBuffer);

                for (uint32_t i = s->iATAPILBA; i < s->iATAPILBA + cSectors; i++)
                {
                    /* sync bytes */
                    *pbBuf++ = 0x00;
                    memset(pbBuf, 0xff, 11);
                    pbBuf += 11;
                    /* MSF */
                    ataLBA2MSF(pbBuf, i);
                    pbBuf += 3;
                    *pbBuf++ = 0x01; /* mode 1 data */
                    /* data */
                    rc = s->pDrvBlock->pfnRead(s->pDrvBlock, (uint64_t)i * 2048, pbBuf, 2048);
                    if (VBOX_FAILURE(rc))
                        break;
                    pbBuf += 2048;
                    /* ECC */
                    memset(pbBuf, 0, 288);
                    pbBuf += 288;
                }
            }
            break;
        default:
            break;
    }
    STAM_PROFILE_ADV_STOP(&s->StatReads, r);

    STAM_PROFILE_START(&pCtl->StatLockWait, a);
    PDMCritSectEnter(&pCtl->lock, VINF_SUCCESS);
    STAM_PROFILE_STOP(&pCtl->StatLockWait, a);

    if (VBOX_SUCCESS(rc))
    {
        s->Led.Actual.s.fReading = 0;
        STAM_COUNTER_ADD(&s->StatBytesRead, s->cbATAPISector * cSectors);

        /* The initial buffer end value has been set up based on the total
         * transfer size. But the I/O buffer size limits what can actually be
         * done in one transfer, so set the actual value of the buffer end. */
        s->cbElementaryTransfer = cbTransfer;
        if (cbTransfer >= s->cbTotalTransfer)
            s->iSourceSink = ATAFN_SS_NULL;
        atapiCmdOK(s);
        s->iATAPILBA += cSectors;
    }
    else
    {
        if (s->cErrors++ < MAX_LOG_REL_ERRORS)
            LogRel(("PIIX3 ATA: LUN#%d: CD-ROM read error, %d sectors at LBA %d\n", s->iLUN, cSectors, s->iATAPILBA));
        atapiCmdError(s, SCSI_SENSE_MEDIUM_ERROR, SCSI_ASC_READ_ERROR);
    }
    return false;
}


static bool atapiPassthroughSS(ATADevState *s)
{
    PATACONTROLLER pCtl = ATADEVSTATE_2_CONTROLLER(s);
    int rc = VINF_SUCCESS;
    uint8_t uATAPISenseKey;
    size_t cbTransfer;
    PSTAMPROFILEADV pProf = NULL;

    cbTransfer = s->cbElementaryTransfer;

    if (s->uTxDir == PDMBLOCKTXDIR_TO_DEVICE)
        Log3(("ATAPI PT data write (%d): %.*Vhxs\n", cbTransfer, cbTransfer, s->CTXSUFF(pbIOBuffer)));

    /* Simple heuristics: if there is at least one sector of data
     * to transfer, it's worth updating the LEDs. */
    if (cbTransfer >= 2048)
    {
        if (s->uTxDir != PDMBLOCKTXDIR_TO_DEVICE)
        {
            s->Led.Asserted.s.fReading = s->Led.Actual.s.fReading = 1;
            pProf = &s->StatReads;
        }
        else
        {
            s->Led.Asserted.s.fWriting = s->Led.Actual.s.fWriting = 1;
            pProf = &s->StatWrites;
        }
    }

    PDMCritSectLeave(&pCtl->lock);

    if (pProf) { STAM_PROFILE_ADV_START(pProf, b); }
    if (cbTransfer > 100 * _1K)
    {
        /* Linux accepts commands with up to 100KB of data, but expects
         * us to handle commands with up to 128KB of data. The usual
         * imbalance of powers. */
        uint8_t aATAPICmd[ATAPI_PACKET_SIZE];
        uint32_t iATAPILBA, cSectors, cReqSectors;
        size_t cbCurrTX;
        uint8_t *pbBuf = s->CTXSUFF(pbIOBuffer);

        switch (s->aATAPICmd[0])
        {
            case SCSI_READ_10:
            case SCSI_WRITE_10:
            case SCSI_WRITE_AND_VERIFY_10:
                iATAPILBA = ataBE2H_U32(s->aATAPICmd + 2);
                cSectors = ataBE2H_U16(s->aATAPICmd + 7);
                break;
            case SCSI_READ_12:
            case SCSI_WRITE_12:
                iATAPILBA = ataBE2H_U32(s->aATAPICmd + 2);
                cSectors = ataBE2H_U32(s->aATAPICmd + 6);
                break;
            case SCSI_READ_CD:
                iATAPILBA = ataBE2H_U32(s->aATAPICmd + 2);
                cSectors = ataBE2H_U24(s->aATAPICmd + 6) / s->cbATAPISector;
                break;
            case SCSI_READ_CD_MSF:
                iATAPILBA = ataMSF2LBA(s->aATAPICmd + 3);
                cSectors = ataMSF2LBA(s->aATAPICmd + 6) - iATAPILBA;
                break;
            default:
                AssertMsgFailed(("Don't know how to split command %#04x\n", s->aATAPICmd[0]));
                if (s->cErrors++ < MAX_LOG_REL_ERRORS)
                    LogRel(("PIIX3 ATA: LUN#%d: CD-ROM passthrough split error\n", s->iLUN));
                atapiCmdError(s, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_ILLEGAL_OPCODE);
                {
                STAM_PROFILE_START(&pCtl->StatLockWait, a);
                PDMCritSectEnter(&pCtl->lock, VINF_SUCCESS);
                STAM_PROFILE_STOP(&pCtl->StatLockWait, a);
                }
                return false;
        }
        memcpy(aATAPICmd, s->aATAPICmd, ATAPI_PACKET_SIZE);
        cReqSectors = 0;
        for (uint32_t i = cSectors; i > 0; i -= cReqSectors)
        {
            if (i * s->cbATAPISector > 100 * _1K)
                cReqSectors = (100 * _1K) / s->cbATAPISector;
            else
                cReqSectors = i;
            cbCurrTX = s->cbATAPISector * cReqSectors;
            switch (s->aATAPICmd[0])
            {
                case SCSI_READ_10:
                case SCSI_WRITE_10:
                case SCSI_WRITE_AND_VERIFY_10:
                    ataH2BE_U32(aATAPICmd + 2, iATAPILBA);
                    ataH2BE_U16(aATAPICmd + 7, cReqSectors);
                    break;
                case SCSI_READ_12:
                case SCSI_WRITE_12:
                    ataH2BE_U32(aATAPICmd + 2, iATAPILBA);
                    ataH2BE_U32(aATAPICmd + 6, cReqSectors);
                    break;
                case SCSI_READ_CD:
                    ataH2BE_U32(s->aATAPICmd + 2, iATAPILBA);
                    ataH2BE_U24(s->aATAPICmd + 6, cbCurrTX);
                    break;
                case SCSI_READ_CD_MSF:
                    ataLBA2MSF(aATAPICmd + 3, iATAPILBA);
                    ataLBA2MSF(aATAPICmd + 6, iATAPILBA + cReqSectors);
                    break;
            }
            rc = s->pDrvBlock->pfnSendCmd(s->pDrvBlock, aATAPICmd, (PDMBLOCKTXDIR)s->uTxDir, pbBuf, &cbCurrTX, &uATAPISenseKey, 30000 /**< @todo timeout */);
            if (rc != VINF_SUCCESS)
                break;
            iATAPILBA += cReqSectors;
            pbBuf += s->cbATAPISector * cReqSectors;
        }
    }
    else
        rc = s->pDrvBlock->pfnSendCmd(s->pDrvBlock, s->aATAPICmd, (PDMBLOCKTXDIR)s->uTxDir, s->CTXSUFF(pbIOBuffer), &cbTransfer, &uATAPISenseKey, 30000 /**< @todo timeout */);
    if (pProf) { STAM_PROFILE_ADV_STOP(pProf, b); }

    STAM_PROFILE_START(&pCtl->StatLockWait, a);
    PDMCritSectEnter(&pCtl->lock, VINF_SUCCESS);
    STAM_PROFILE_STOP(&pCtl->StatLockWait, a);

    /* Update the LEDs and the read/write statistics. */
    if (cbTransfer >= 2048)
    {
        if (s->uTxDir != PDMBLOCKTXDIR_TO_DEVICE)
        {
            s->Led.Actual.s.fReading = 0;
            STAM_COUNTER_ADD(&s->StatBytesRead, cbTransfer);
        }
        else
        {
            s->Led.Actual.s.fWriting = 0;
            STAM_COUNTER_ADD(&s->StatBytesWritten, cbTransfer);
        }
    }

    if (VBOX_SUCCESS(rc))
    {
        if (s->uTxDir == PDMBLOCKTXDIR_FROM_DEVICE)
        {
            Assert(cbTransfer <= s->cbTotalTransfer);
            /* Reply with the same amount of data as the real drive. */
            s->cbTotalTransfer = cbTransfer;
            /* The initial buffer end value has been set up based on the total
             * transfer size. But the I/O buffer size limits what can actually be
             * done in one transfer, so set the actual value of the buffer end. */
            s->cbElementaryTransfer = cbTransfer;
            if (s->aATAPICmd[0] == SCSI_INQUIRY)
            {
                /* Make sure that the real drive cannot be identified.
                 * Motivation: changing the VM configuration should be as
                 * invisible as possible to the guest. */
                Log3(("ATAPI PT inquiry data before (%d): %.*Vhxs\n", cbTransfer, cbTransfer, s->CTXSUFF(pbIOBuffer)));
                ataSCSIPadStr(s->CTXSUFF(pbIOBuffer) + 8, "VBOX", 8);
                ataSCSIPadStr(s->CTXSUFF(pbIOBuffer) + 16, "CD-ROM", 16);
                ataSCSIPadStr(s->CTXSUFF(pbIOBuffer) + 32, "1.0", 4);
            }
            if (cbTransfer)
                Log3(("ATAPI PT data read (%d): %.*Vhxs\n", cbTransfer, cbTransfer, s->CTXSUFF(pbIOBuffer)));
        }
        s->iSourceSink = ATAFN_SS_NULL;
        atapiCmdOK(s);
    }
    else
    {
        if (s->cErrors++ < MAX_LOG_REL_ERRORS)
        {
            uint8_t u8Cmd = s->aATAPICmd[0];
            do
            {
                /* don't log superflous errors */
                if (    rc == VERR_DEV_IO_ERROR
                    && (   u8Cmd == SCSI_TEST_UNIT_READY
                        || u8Cmd == SCSI_READ_CAPACITY
                        || u8Cmd == SCSI_READ_TOC_PMA_ATIP))
                    break;
                LogRel(("PIIX3 ATA: LUN#%d: CD-ROM passthrough command (%#04x) error %d %Vrc\n", s->iLUN, u8Cmd, uATAPISenseKey, rc));
            } while (0);
        }
        atapiCmdError(s, uATAPISenseKey, 0);
        /* This is a drive-reported error. atapiCmdError() sets both the error
         * error code in the ATA error register and in s->uATAPISenseKey. The
         * former is correct, the latter is not. Otherwise the drive would not
         * get the next REQUEST SENSE command which is necessary to clear the
         * error status of the drive. Easy fix: clear s->uATAPISenseKey. */
        s->uATAPISenseKey = SCSI_SENSE_NONE;
        s->uATAPIASC = SCSI_ASC_NONE;
    }
    return false;
}


static bool atapiReadSectors(ATADevState *s, uint32_t iATAPILBA, uint32_t cSectors, uint32_t cbSector)
{
    Assert(cSectors > 0);
    s->iATAPILBA = iATAPILBA;
    s->cbATAPISector = cbSector;
    ataStartTransfer(s, cSectors * cbSector, PDMBLOCKTXDIR_FROM_DEVICE, ATAFN_BT_ATAPI_CMD, ATAFN_SS_ATAPI_READ, true);
    return false;
}


static bool atapiReadCapacitySS(ATADevState *s)
{
    uint8_t *pbBuf = s->CTXSUFF(pbIOBuffer);

    Assert(s->uTxDir == PDMBLOCKTXDIR_FROM_DEVICE);
    Assert(s->cbElementaryTransfer <= 8);
    ataH2BE_U32(pbBuf, s->cTotalSectors - 1);
    ataH2BE_U32(pbBuf + 4, 2048);
    s->iSourceSink = ATAFN_SS_NULL;
    atapiCmdOK(s);
    return false;
}


static bool atapiReadDiscInformationSS(ATADevState *s)
{
    uint8_t *pbBuf = s->CTXSUFF(pbIOBuffer);

    Assert(s->uTxDir == PDMBLOCKTXDIR_FROM_DEVICE);
    Assert(s->cbElementaryTransfer <= 34);
    memset(pbBuf, '\0', 34);
    ataH2BE_U16(pbBuf, 32);
    pbBuf[2] = (0 << 4) | (3 << 2) | (2 << 0); /* not erasable, complete session, complete disc */
    pbBuf[3] = 1; /* number of first track */
    pbBuf[4] = 1; /* number of sessions (LSB) */
    pbBuf[5] = 1; /* first track number in last session (LSB) */
    pbBuf[6] = 1; /* last track number in last session (LSB) */
    pbBuf[7] = (0 << 7) | (0 << 6) | (1 << 5) | (0 << 2) | (0 << 0); /* disc id not valid, disc bar code not valid, unrestricted use, not dirty, not RW medium */
    pbBuf[8] = 0; /* disc type = CD-ROM */
    pbBuf[9] = 0; /* number of sessions (MSB) */
    pbBuf[10] = 0; /* number of sessions (MSB) */
    pbBuf[11] = 0; /* number of sessions (MSB) */
    ataH2BE_U32(pbBuf + 16, 0x00ffffff); /* last session lead-in start time is not available */
    ataH2BE_U32(pbBuf + 20, 0x00ffffff); /* last possible start time for lead-out is not available */
    s->iSourceSink = ATAFN_SS_NULL;
    atapiCmdOK(s);
    return false;
}


static bool atapiReadTrackInformationSS(ATADevState *s)
{
    uint8_t *pbBuf = s->CTXSUFF(pbIOBuffer);

    Assert(s->uTxDir == PDMBLOCKTXDIR_FROM_DEVICE);
    Assert(s->cbElementaryTransfer <= 36);
    /* Accept address/number type of 1 only, and only track 1 exists. */
    if ((s->aATAPICmd[1] & 0x03) != 1 || ataBE2H_U32(&s->aATAPICmd[2]) != 1)
    {
        atapiCmdError(s, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_INV_FIELD_IN_CMD_PACKET);
        return false;
    }
    memset(pbBuf, '\0', 36);
    ataH2BE_U16(pbBuf, 34);
    pbBuf[2] = 1; /* track number (LSB) */
    pbBuf[3] = 1; /* session number (LSB) */
    pbBuf[5] = (0 << 5) | (0 << 4) | (4 << 0); /* not damaged, primary copy, data track */
    pbBuf[6] = (0 << 7) | (0 << 6) | (0 << 5) | (0 << 6) | (1 << 0); /* not reserved track, not blank, not packet writing, not fixed packet, data mode 1 */
    pbBuf[7] = (0 << 1) | (0 << 0); /* last recorded address not valid, next recordable address not valid */
    ataH2BE_U32(pbBuf + 8, 0); /* track start address is 0 */
    ataH2BE_U32(pbBuf + 24, s->cTotalSectors); /* track size */
    pbBuf[32] = 0; /* track number (MSB) */
    pbBuf[33] = 0; /* session number (MSB) */
    s->iSourceSink = ATAFN_SS_NULL;
    atapiCmdOK(s);
    return false;
}


static bool atapiGetConfigurationSS(ATADevState *s)
{
    uint8_t *pbBuf = s->CTXSUFF(pbIOBuffer);

    Assert(s->uTxDir == PDMBLOCKTXDIR_FROM_DEVICE);
    Assert(s->cbElementaryTransfer <= 32);
    /* Accept valid request types only, and only starting feature 0. */
    if ((s->aATAPICmd[1] & 0x03) == 3 || ataBE2H_U16(&s->aATAPICmd[2]) != 0)
    {
        atapiCmdError(s, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_INV_FIELD_IN_CMD_PACKET);
        return false;
    }
    memset(pbBuf, '\0', 32);
    ataH2BE_U32(pbBuf, 16);
    /** @todo implement switching between CD-ROM and DVD-ROM profile (the only
     * way to differentiate them right now is based on the image size). Also
     * implement signalling "no current profile" if no medium is loaded. */
    ataH2BE_U16(pbBuf + 6, 0x08); /* current profile: read-only CD */

    ataH2BE_U16(pbBuf + 8, 0); /* feature 0: list of profiles supported */
    pbBuf[10] = (0 << 2) | (1 << 1) | (1 || 0); /* version 0, persistent, current */
    pbBuf[11] = 8; /* additional bytes for profiles */
    /* The MMC-3 spec says that DVD-ROM read capability should be reported
     * before CD-ROM read capability. */
    ataH2BE_U16(pbBuf + 12, 0x10); /* profile: read-only DVD */
    pbBuf[14] = (0 << 0); /* NOT current profile */
    ataH2BE_U16(pbBuf + 16, 0x08); /* profile: read only CD */
    pbBuf[18] = (1 << 0); /* current profile */
    /* Other profiles we might want to add in the future: 0x40 (BD-ROM) and 0x50 (HDDVD-ROM) */
    s->iSourceSink = ATAFN_SS_NULL;
    atapiCmdOK(s);
    return false;
}


static bool atapiInquirySS(ATADevState *s)
{
    uint8_t *pbBuf = s->CTXSUFF(pbIOBuffer);

    Assert(s->uTxDir == PDMBLOCKTXDIR_FROM_DEVICE);
    Assert(s->cbElementaryTransfer <= 36);
    pbBuf[0] = 0x05; /* CD-ROM */
    pbBuf[1] = 0x80; /* removable */
#if 1/*ndef VBOX*/  /** @todo implement MESN + AENC. (async notification on removal and stuff.) */
    pbBuf[2] = 0x00; /* ISO */
    pbBuf[3] = 0x21; /* ATAPI-2 (XXX: put ATAPI-4 ?) */
#else
    pbBuf[2] = 0x00; /* ISO */
    pbBuf[3] = 0x91; /* format 1, MESN=1, AENC=9 ??? */
#endif
    pbBuf[4] = 31; /* additional length */
    pbBuf[5] = 0; /* reserved */
    pbBuf[6] = 0; /* reserved */
    pbBuf[7] = 0; /* reserved */
    ataSCSIPadStr(pbBuf + 8, "VBOX", 8);
    ataSCSIPadStr(pbBuf + 16, "CD-ROM", 16);
    ataSCSIPadStr(pbBuf + 32, "1.0", 4);
    s->iSourceSink = ATAFN_SS_NULL;
    atapiCmdOK(s);
    return false;
}


static bool atapiModeSenseErrorRecoverySS(ATADevState *s)
{
    uint8_t *pbBuf = s->CTXSUFF(pbIOBuffer);

    Assert(s->uTxDir == PDMBLOCKTXDIR_FROM_DEVICE);
    Assert(s->cbElementaryTransfer <= 16);
    ataH2BE_U16(&pbBuf[0], 16 + 6);
    pbBuf[2] = 0x70;
    pbBuf[3] = 0;
    pbBuf[4] = 0;
    pbBuf[5] = 0;
    pbBuf[6] = 0;
    pbBuf[7] = 0;

    pbBuf[8] = 0x01;
    pbBuf[9] = 0x06;
    pbBuf[10] = 0x00;
    pbBuf[11] = 0x05;
    pbBuf[12] = 0x00;
    pbBuf[13] = 0x00;
    pbBuf[14] = 0x00;
    pbBuf[15] = 0x00;
    s->iSourceSink = ATAFN_SS_NULL;
    atapiCmdOK(s);
    return false;
}


static bool atapiModeSenseCDStatusSS(ATADevState *s)
{
    uint8_t *pbBuf = s->CTXSUFF(pbIOBuffer);

    Assert(s->uTxDir == PDMBLOCKTXDIR_FROM_DEVICE);
    Assert(s->cbElementaryTransfer <= 40);
    ataH2BE_U16(&pbBuf[0], 38);
    pbBuf[2] = 0x70;
    pbBuf[3] = 0;
    pbBuf[4] = 0;
    pbBuf[5] = 0;
    pbBuf[6] = 0;
    pbBuf[7] = 0;

    pbBuf[8] = 0x2a;
    pbBuf[9] = 30; /* page length */
    pbBuf[10] = 0x08; /* DVD-ROM read support */
    pbBuf[11] = 0x00; /* no write support */
    /* The following claims we support audio play. This is obviously false,
     * but the Linux generic CDROM support makes many features depend on this
     * capability. If it's not set, this causes many things to be disabled. */
    pbBuf[12] = 0x71; /* multisession support, mode 2 form 1/2 support, audio play */
    pbBuf[13] = 0x00; /* no subchannel reads supported */
    pbBuf[14] = (1 << 0) | (1 << 3) | (1 << 5); /* lock supported, eject supported, tray type loading mechanism */
    if (s->pDrvMount->pfnIsLocked(s->pDrvMount))
        pbBuf[14] |= 1 << 1; /* report lock state */
    pbBuf[15] = 0; /* no subchannel reads supported, no separate audio volume control, no changer etc. */
    ataH2BE_U16(&pbBuf[16], 5632); /* (obsolete) claim 32x speed support */
    ataH2BE_U16(&pbBuf[18], 2); /* number of audio volume levels */
    ataH2BE_U16(&pbBuf[20], s->cbIOBuffer / _1K); /* buffer size supported in Kbyte */
    ataH2BE_U16(&pbBuf[22], 5632); /* (obsolete) current read speed 32x */
    pbBuf[24] = 0; /* reserved */
    pbBuf[25] = 0; /* reserved for digital audio (see idx 15) */
    ataH2BE_U16(&pbBuf[26], 0); /* (obsolete) maximum write speed */
    ataH2BE_U16(&pbBuf[28], 0); /* (obsolete) current write speed */
    ataH2BE_U16(&pbBuf[30], 0); /* copy management revision supported 0=no CSS */
    pbBuf[32] = 0; /* reserved */
    pbBuf[33] = 0; /* reserved */
    pbBuf[34] = 0; /* reserved */
    pbBuf[35] = 1; /* rotation control CAV */
    ataH2BE_U16(&pbBuf[36], 0); /* current write speed */
    ataH2BE_U16(&pbBuf[38], 0); /* number of write speed performance descriptors */
    s->iSourceSink = ATAFN_SS_NULL;
    atapiCmdOK(s);
    return false;
}


static bool atapiRequestSenseSS(ATADevState *s)
{
    uint8_t *pbBuf = s->CTXSUFF(pbIOBuffer);

    Assert(s->uTxDir == PDMBLOCKTXDIR_FROM_DEVICE);
    Assert(s->cbElementaryTransfer <= 18);
    memset(pbBuf, 0, 18);
    pbBuf[0] = 0x70 | (1 << 7);
    pbBuf[2] = s->uATAPISenseKey;
    pbBuf[7] = 10;
    pbBuf[12] = s->uATAPIASC;
    s->iSourceSink = ATAFN_SS_NULL;
    atapiCmdOK(s);
    return false;
}


static bool atapiMechanismStatusSS(ATADevState *s)
{
    uint8_t *pbBuf = s->CTXSUFF(pbIOBuffer);

    Assert(s->uTxDir == PDMBLOCKTXDIR_FROM_DEVICE);
    Assert(s->cbElementaryTransfer <= 8);
    ataH2BE_U16(pbBuf, 0);
    /* no current LBA */
    pbBuf[2] = 0;
    pbBuf[3] = 0;
    pbBuf[4] = 0;
    pbBuf[5] = 1;
    ataH2BE_U16(pbBuf + 6, 0);
    s->iSourceSink = ATAFN_SS_NULL;
    atapiCmdOK(s);
    return false;
}


static bool atapiReadTOCNormalSS(ATADevState *s)
{
    uint8_t *pbBuf = s->CTXSUFF(pbIOBuffer), *q, iStartTrack;
    bool fMSF;
    uint32_t cbSize;

    Assert(s->uTxDir == PDMBLOCKTXDIR_FROM_DEVICE);
    fMSF = (s->aATAPICmd[1] >> 1) & 1;
    iStartTrack = s->aATAPICmd[6];
    if (iStartTrack > 1 && iStartTrack != 0xaa)
    {
        atapiCmdError(s, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_INV_FIELD_IN_CMD_PACKET);
        return false;
    }
    q = pbBuf + 2;
    *q++ = 1; /* first session */
    *q++ = 1; /* last session */
    if (iStartTrack <= 1)
    {
        *q++ = 0; /* reserved */
        *q++ = 0x14; /* ADR, control */
        *q++ = 1;    /* track number */
        *q++ = 0; /* reserved */
        if (fMSF)
        {
            *q++ = 0; /* reserved */
            ataLBA2MSF(q, 0);
            q += 3;
        }
        else
        {
            /* sector 0 */
            ataH2BE_U32(q, 0);
            q += 4;
        }
    }
    /* lead out track */
    *q++ = 0; /* reserved */
    *q++ = 0x14; /* ADR, control */
    *q++ = 0xaa; /* track number */
    *q++ = 0; /* reserved */
    if (fMSF)
    {
        *q++ = 0; /* reserved */
        ataLBA2MSF(q, s->cTotalSectors);
        q += 3;
    }
    else
    {
        ataH2BE_U32(q, s->cTotalSectors);
        q += 4;
    }
    cbSize = q - pbBuf;
    ataH2BE_U16(pbBuf, cbSize - 2);
    if (cbSize < s->cbTotalTransfer)
        s->cbTotalTransfer = cbSize;
    s->iSourceSink = ATAFN_SS_NULL;
    atapiCmdOK(s);
    return false;
}


static bool atapiReadTOCMultiSS(ATADevState *s)
{
    uint8_t *pbBuf = s->CTXSUFF(pbIOBuffer);
    bool fMSF;

    Assert(s->uTxDir == PDMBLOCKTXDIR_FROM_DEVICE);
    Assert(s->cbElementaryTransfer <= 12);
    fMSF = (s->aATAPICmd[1] >> 1) & 1;
    /* multi session: only a single session defined */
/** @todo double-check this stuff against what a real drive says for a CD-ROM (not a CD-R) with only a single data session. Maybe solve the problem with "cdrdao read-toc" not being able to figure out whether numbers are in BCD or hex. */
    memset(pbBuf, 0, 12);
    pbBuf[1] = 0x0a;
    pbBuf[2] = 0x01;
    pbBuf[3] = 0x01;
    pbBuf[5] = 0x14; /* ADR, control */
    pbBuf[6] = 1; /* first track in last complete session */
    if (fMSF)
    {
        pbBuf[8] = 0; /* reserved */
        ataLBA2MSF(&pbBuf[9], 0);
    }
    else
    {
        /* sector 0 */
        ataH2BE_U32(pbBuf + 8, 0);
    }
    s->iSourceSink = ATAFN_SS_NULL;
    atapiCmdOK(s);
    return false;
}


static bool atapiReadTOCRawSS(ATADevState *s)
{
    uint8_t *pbBuf = s->CTXSUFF(pbIOBuffer), *q, iStartTrack;
    bool fMSF;
    uint32_t cbSize;

    Assert(s->uTxDir == PDMBLOCKTXDIR_FROM_DEVICE);
    fMSF = (s->aATAPICmd[1] >> 1) & 1;
    iStartTrack = s->aATAPICmd[6];

    q = pbBuf + 2;
    *q++ = 1; /* first session */
    *q++ = 1; /* last session */

    *q++ = 1; /* session number */
    *q++ = 0x14; /* data track */
    *q++ = 0; /* track number */
    *q++ = 0xa0; /* first track in program area */
    *q++ = 0; /* min */
    *q++ = 0; /* sec */
    *q++ = 0; /* frame */
    *q++ = 0;
    *q++ = 1; /* first track */
    *q++ = 0x00; /* disk type CD-DA or CD data */
    *q++ = 0;

    *q++ = 1; /* session number */
    *q++ = 0x14; /* data track */
    *q++ = 0; /* track number */
    *q++ = 0xa1; /* last track in program area */
    *q++ = 0; /* min */
    *q++ = 0; /* sec */
    *q++ = 0; /* frame */
    *q++ = 0;
    *q++ = 1; /* last track */
    *q++ = 0;
    *q++ = 0;

    *q++ = 1; /* session number */
    *q++ = 0x14; /* data track */
    *q++ = 0; /* track number */
    *q++ = 0xa2; /* lead-out */
    *q++ = 0; /* min */
    *q++ = 0; /* sec */
    *q++ = 0; /* frame */
    if (fMSF)
    {
        *q++ = 0; /* reserved */
        ataLBA2MSF(q, s->cTotalSectors);
        q += 3;
    }
    else
    {
        ataH2BE_U32(q, s->cTotalSectors);
        q += 4;
    }

    *q++ = 1; /* session number */
    *q++ = 0x14; /* ADR, control */
    *q++ = 0;    /* track number */
    *q++ = 1;    /* point */
    *q++ = 0; /* min */
    *q++ = 0; /* sec */
    *q++ = 0; /* frame */
    if (fMSF)
    {
        *q++ = 0; /* reserved */
        ataLBA2MSF(q, 0);
        q += 3;
    }
    else
    {
        /* sector 0 */
        ataH2BE_U32(q, 0);
        q += 4;
    }

    cbSize = q - pbBuf;
    ataH2BE_U16(pbBuf, cbSize - 2);
    if (cbSize < s->cbTotalTransfer)
        s->cbTotalTransfer = cbSize;
    s->iSourceSink = ATAFN_SS_NULL;
    atapiCmdOK(s);
    return false;
}


static void atapiParseCmdVirtualATAPI(ATADevState *s)
{
    const uint8_t *pbPacket;
    uint8_t *pbBuf;
    uint32_t cbMax;

    pbPacket = s->aATAPICmd;
    pbBuf = s->CTXSUFF(pbIOBuffer);
    switch (pbPacket[0])
    {
        case SCSI_TEST_UNIT_READY:
            if (s->cNotifiedMediaChange > 0)
            {
                if (s->cNotifiedMediaChange-- > 2)
                    atapiCmdError(s, SCSI_SENSE_NOT_READY, SCSI_ASC_MEDIUM_NOT_PRESENT);
                else
                    atapiCmdError(s, SCSI_SENSE_UNIT_ATTENTION, SCSI_ASC_MEDIUM_MAY_HAVE_CHANGED); /* media changed */
            }
            else if (s->pDrvMount->pfnIsMounted(s->pDrvMount))
                atapiCmdOK(s);
            else
                atapiCmdError(s, SCSI_SENSE_NOT_READY, SCSI_ASC_MEDIUM_NOT_PRESENT);
            break;
        case SCSI_MODE_SENSE_10:
            {
                uint8_t uPageControl, uPageCode;
                cbMax = ataBE2H_U16(pbPacket + 7);
                uPageControl = pbPacket[2] >> 6;
                uPageCode = pbPacket[2] & 0x3f;
                switch (uPageControl)
                {
                    case SCSI_PAGECONTROL_CURRENT:
                        switch (uPageCode)
                        {
                            case SCSI_MODEPAGE_ERROR_RECOVERY:
                                ataStartTransfer(s, RT_MIN(cbMax, 16), PDMBLOCKTXDIR_FROM_DEVICE, ATAFN_BT_ATAPI_CMD, ATAFN_SS_ATAPI_MODE_SENSE_ERROR_RECOVERY, true);
                                break;
                            case SCSI_MODEPAGE_CD_STATUS:
                                ataStartTransfer(s, RT_MIN(cbMax, 40), PDMBLOCKTXDIR_FROM_DEVICE, ATAFN_BT_ATAPI_CMD, ATAFN_SS_ATAPI_MODE_SENSE_CD_STATUS, true);
                                break;
                            default:
                                goto error_cmd;
                        }
                        break;
                    case SCSI_PAGECONTROL_CHANGEABLE:
                        goto error_cmd;
                    case SCSI_PAGECONTROL_DEFAULT:
                        goto error_cmd;
                    default:
                    case SCSI_PAGECONTROL_SAVED:
                        atapiCmdError(s, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_SAVING_PARAMETERS_NOT_SUPPORTED);
                        break;
                }
            }
            break;
        case SCSI_REQUEST_SENSE:
            cbMax = pbPacket[4];
            ataStartTransfer(s, RT_MIN(cbMax, 18), PDMBLOCKTXDIR_FROM_DEVICE, ATAFN_BT_ATAPI_CMD, ATAFN_SS_ATAPI_REQUEST_SENSE, true);
            break;
        case SCSI_PREVENT_ALLOW_MEDIUM_REMOVAL:
            if (s->pDrvMount->pfnIsMounted(s->pDrvMount))
            {
                if (pbPacket[4] & 1)
                    s->pDrvMount->pfnLock(s->pDrvMount);
                else
                    s->pDrvMount->pfnUnlock(s->pDrvMount);
                atapiCmdOK(s);
            }
            else
                atapiCmdError(s, SCSI_SENSE_NOT_READY, SCSI_ASC_MEDIUM_NOT_PRESENT);
            break;
        case SCSI_READ_10:
        case SCSI_READ_12:
            {
                uint32_t cSectors, iATAPILBA;

                if (s->cNotifiedMediaChange > 0)
                {
                    s->cNotifiedMediaChange-- ;
                    atapiCmdError(s, SCSI_SENSE_UNIT_ATTENTION, SCSI_ASC_MEDIUM_MAY_HAVE_CHANGED); /* media changed */
                    break;
                }
                else if (!s->pDrvMount->pfnIsMounted(s->pDrvMount))
                {
                    atapiCmdError(s, SCSI_SENSE_NOT_READY, SCSI_ASC_MEDIUM_NOT_PRESENT);
                    break;
                }
                if (pbPacket[0] == SCSI_READ_10)
                    cSectors = ataBE2H_U16(pbPacket + 7);
                else
                    cSectors = ataBE2H_U32(pbPacket + 6);
                iATAPILBA = ataBE2H_U32(pbPacket + 2);
                if (cSectors == 0)
                {
                    atapiCmdOK(s);
                    break;
                }
                if ((uint64_t)iATAPILBA + cSectors > s->cTotalSectors)
                {
                    /* Rate limited logging, one log line per second. For
                     * guests that insist on reading from places outside the
                     * valid area this often generates too many release log
                     * entries otherwise. */
                    static uint64_t uLastLogTS = 0;
                    if (RTTimeMilliTS() >= uLastLogTS + 1000)
                    {
                        LogRel(("PIIX3 ATA: LUN#%d: CD-ROM block number %Ld invalid (READ)\n", s->iLUN, (uint64_t)iATAPILBA + cSectors));
                        uLastLogTS = RTTimeMilliTS();
                    }
                    atapiCmdError(s, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_LOGICAL_BLOCK_OOR);
                    break;
                }
                atapiReadSectors(s, iATAPILBA, cSectors, 2048);
            }
            break;
        case SCSI_READ_CD:
            {
                uint32_t cSectors, iATAPILBA;

                if (s->cNotifiedMediaChange > 0)
                {
                    s->cNotifiedMediaChange-- ;
                    atapiCmdError(s, SCSI_SENSE_UNIT_ATTENTION, SCSI_ASC_MEDIUM_MAY_HAVE_CHANGED); /* media changed */
                    break;
                }
                else if (!s->pDrvMount->pfnIsMounted(s->pDrvMount))
                {
                    atapiCmdError(s, SCSI_SENSE_NOT_READY, SCSI_ASC_MEDIUM_NOT_PRESENT);
                    break;
                }
                cSectors = (pbPacket[6] << 16) | (pbPacket[7] << 8) | pbPacket[8];
                iATAPILBA = ataBE2H_U32(pbPacket + 2);
                if (cSectors == 0)
                {
                    atapiCmdOK(s);
                    break;
                }
                if ((uint64_t)iATAPILBA + cSectors > s->cTotalSectors)
                {
                    /* Rate limited logging, one log line per second. For
                     * guests that insist on reading from places outside the
                     * valid area this often generates too many release log
                     * entries otherwise. */
                    static uint64_t uLastLogTS = 0;
                    if (RTTimeMilliTS() >= uLastLogTS + 1000)
                    {
                        LogRel(("PIIX3 ATA: LUN#%d: CD-ROM block number %Ld invalid (READ CD)\n", s->iLUN, (uint64_t)iATAPILBA + cSectors));
                        uLastLogTS = RTTimeMilliTS();
                    }
                    atapiCmdError(s, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_LOGICAL_BLOCK_OOR);
                    break;
                }
                switch (pbPacket[9] & 0xf8)
                {
                    case 0x00:
                        /* nothing */
                        atapiCmdOK(s);
                        break;
                    case 0x10:
                        /* normal read */
                        atapiReadSectors(s, iATAPILBA, cSectors, 2048);
                        break;
                    case 0xf8:
                        /* read all data */
                        atapiReadSectors(s, iATAPILBA, cSectors, 2352);
                        break;
                    default:
                        LogRel(("PIIX3 ATA: LUN#%d: CD-ROM sector format not supported\n", s->iLUN));
                        atapiCmdError(s, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_INV_FIELD_IN_CMD_PACKET);
                        break;
                }
            }
            break;
        case SCSI_SEEK_10:
            {
                uint32_t iATAPILBA;
                if (s->cNotifiedMediaChange > 0)
                {
                    s->cNotifiedMediaChange-- ;
                    atapiCmdError(s, SCSI_SENSE_UNIT_ATTENTION, SCSI_ASC_MEDIUM_MAY_HAVE_CHANGED); /* media changed */
                    break;
                }
                else if (!s->pDrvMount->pfnIsMounted(s->pDrvMount))
                {
                    atapiCmdError(s, SCSI_SENSE_NOT_READY, SCSI_ASC_MEDIUM_NOT_PRESENT);
                    break;
                }
                iATAPILBA = ataBE2H_U32(pbPacket + 2);
                if (iATAPILBA > s->cTotalSectors)
                {
                    /* Rate limited logging, one log line per second. For
                     * guests that insist on seeking to places outside the
                     * valid area this often generates too many release log
                     * entries otherwise. */
                    static uint64_t uLastLogTS = 0;
                    if (RTTimeMilliTS() >= uLastLogTS + 1000)
                    {
                        LogRel(("PIIX3 ATA: LUN#%d: CD-ROM block number %Ld invalid (SEEK)\n", s->iLUN, (uint64_t)iATAPILBA));
                        uLastLogTS = RTTimeMilliTS();
                    }
                    atapiCmdError(s, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_LOGICAL_BLOCK_OOR);
                    break;
                }
                atapiCmdOK(s);
                ataSetStatus(s, ATA_STAT_SEEK); /* Linux expects this. */
            }
            break;
        case SCSI_START_STOP_UNIT:
            {
                int rc = VINF_SUCCESS;
                switch (pbPacket[4] & 3)
                {
                    case 0: /* 00 - Stop motor */
                    case 1: /* 01 - Start motor */
                        break;
                    case 2: /* 10 - Eject media */
                        /* This must be done from EMT. */
                        {
                        PATACONTROLLER pCtl = ATADEVSTATE_2_CONTROLLER(s);
                        PPDMDEVINS pDevIns = ATADEVSTATE_2_DEVINS(s);
                        PVMREQ pReq;

                        PDMCritSectLeave(&pCtl->lock);
                        rc = VMR3ReqCall(PDMDevHlpGetVM(pDevIns), &pReq, RT_INDEFINITE_WAIT,
                                         (PFNRT)s->pDrvMount->pfnUnmount, 2, s->pDrvMount, false);
                        AssertReleaseRC(rc);
                        VMR3ReqFree(pReq);
                        {
                            STAM_PROFILE_START(&pCtl->StatLockWait, a);
                            PDMCritSectEnter(&pCtl->lock, VINF_SUCCESS);
                            STAM_PROFILE_STOP(&pCtl->StatLockWait, a);
                        }
                        }
                        break;
                    case 3: /* 11 - Load media */
                        /** @todo rc = s->pDrvMount->pfnLoadMedia(s->pDrvMount) */
                        break;
                }
                if (VBOX_SUCCESS(rc))
                    atapiCmdOK(s);
                else
                    atapiCmdError(s, SCSI_SENSE_NOT_READY, SCSI_ASC_MEDIA_LOAD_OR_EJECT_FAILED);
            }
            break;
        case SCSI_MECHANISM_STATUS:
            {
                cbMax = ataBE2H_U16(pbPacket + 8);
                ataStartTransfer(s, RT_MIN(cbMax, 8), PDMBLOCKTXDIR_FROM_DEVICE, ATAFN_BT_ATAPI_CMD, ATAFN_SS_ATAPI_MECHANISM_STATUS, true);
            }
            break;
        case SCSI_READ_TOC_PMA_ATIP:
            {
                uint8_t format;

                if (s->cNotifiedMediaChange > 0)
                {
                    s->cNotifiedMediaChange-- ;
                    atapiCmdError(s, SCSI_SENSE_UNIT_ATTENTION, SCSI_ASC_MEDIUM_MAY_HAVE_CHANGED); /* media changed */
                    break;
                }
                else if (!s->pDrvMount->pfnIsMounted(s->pDrvMount))
                {
                    atapiCmdError(s, SCSI_SENSE_NOT_READY, SCSI_ASC_MEDIUM_NOT_PRESENT);
                    break;
                }
                cbMax = ataBE2H_U16(pbPacket + 7);
                /* SCSI MMC-3 spec says format is at offset 2 (lower 4 bits),
                 * but Linux kernel uses offset 9 (topmost 2 bits). Hope that
                 * the other field is clear... */
                format = (pbPacket[2] & 0xf) | (pbPacket[9] >> 6);
                switch (format)
                {
                    case 0:
                        ataStartTransfer(s, cbMax, PDMBLOCKTXDIR_FROM_DEVICE, ATAFN_BT_ATAPI_CMD, ATAFN_SS_ATAPI_READ_TOC_NORMAL, true);
                        break;
                    case 1:
                        ataStartTransfer(s, RT_MIN(cbMax, 12), PDMBLOCKTXDIR_FROM_DEVICE, ATAFN_BT_ATAPI_CMD, ATAFN_SS_ATAPI_READ_TOC_MULTI, true);
                        break;
                    case 2:
                        ataStartTransfer(s, cbMax, PDMBLOCKTXDIR_FROM_DEVICE, ATAFN_BT_ATAPI_CMD, ATAFN_SS_ATAPI_READ_TOC_RAW, true);
                        break;
                    default:
                    error_cmd:
                        atapiCmdError(s, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_INV_FIELD_IN_CMD_PACKET);
                        break;
                }
            }
            break;
        case SCSI_READ_CAPACITY:
            if (s->cNotifiedMediaChange > 0)
            {
                s->cNotifiedMediaChange-- ;
                atapiCmdError(s, SCSI_SENSE_UNIT_ATTENTION, SCSI_ASC_MEDIUM_MAY_HAVE_CHANGED); /* media changed */
                break;
            }
            else if (!s->pDrvMount->pfnIsMounted(s->pDrvMount))
            {
                atapiCmdError(s, SCSI_SENSE_NOT_READY, SCSI_ASC_MEDIUM_NOT_PRESENT);
                break;
            }
            ataStartTransfer(s, 8, PDMBLOCKTXDIR_FROM_DEVICE, ATAFN_BT_ATAPI_CMD, ATAFN_SS_ATAPI_READ_CAPACITY, true);
            break;
        case SCSI_READ_DISC_INFORMATION:
            if (s->cNotifiedMediaChange > 0)
            {
                s->cNotifiedMediaChange-- ;
                atapiCmdError(s, SCSI_SENSE_UNIT_ATTENTION, SCSI_ASC_MEDIUM_MAY_HAVE_CHANGED); /* media changed */
                break;
            }
            else if (!s->pDrvMount->pfnIsMounted(s->pDrvMount))
            {
                atapiCmdError(s, SCSI_SENSE_NOT_READY, SCSI_ASC_MEDIUM_NOT_PRESENT);
                break;
            }
            cbMax = ataBE2H_U16(pbPacket + 7);
            ataStartTransfer(s, RT_MIN(cbMax, 34), PDMBLOCKTXDIR_FROM_DEVICE, ATAFN_BT_ATAPI_CMD, ATAFN_SS_ATAPI_READ_DISC_INFORMATION, true);
            break;
        case SCSI_READ_TRACK_INFORMATION:
            if (s->cNotifiedMediaChange > 0)
            {
                s->cNotifiedMediaChange-- ;
                atapiCmdError(s, SCSI_SENSE_UNIT_ATTENTION, SCSI_ASC_MEDIUM_MAY_HAVE_CHANGED); /* media changed */
                break;
            }
            else if (!s->pDrvMount->pfnIsMounted(s->pDrvMount))
            {
                atapiCmdError(s, SCSI_SENSE_NOT_READY, SCSI_ASC_MEDIUM_NOT_PRESENT);
                break;
            }
            cbMax = ataBE2H_U16(pbPacket + 7);
            ataStartTransfer(s, RT_MIN(cbMax, 36), PDMBLOCKTXDIR_FROM_DEVICE, ATAFN_BT_ATAPI_CMD, ATAFN_SS_ATAPI_READ_TRACK_INFORMATION, true);
            break;
        case SCSI_GET_CONFIGURATION:
            /* No media change stuff here, it can confuse Linux guests. */
            cbMax = ataBE2H_U16(pbPacket + 7);
            ataStartTransfer(s, RT_MIN(cbMax, 32), PDMBLOCKTXDIR_FROM_DEVICE, ATAFN_BT_ATAPI_CMD, ATAFN_SS_ATAPI_GET_CONFIGURATION, true);
            break;
        case SCSI_INQUIRY:
            cbMax = pbPacket[4];
            ataStartTransfer(s, RT_MIN(cbMax, 36), PDMBLOCKTXDIR_FROM_DEVICE, ATAFN_BT_ATAPI_CMD, ATAFN_SS_ATAPI_INQUIRY, true);
            break;
        default:
            atapiCmdError(s, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_ILLEGAL_OPCODE);
            break;
    }
}


/*
 * Parse ATAPI commands, passing them directly to the CD/DVD drive.
 */
static void atapiParseCmdPassthrough(ATADevState *s)
{
    const uint8_t *pbPacket;
    uint8_t *pbBuf;
    uint32_t cSectors, iATAPILBA;
    uint32_t cbTransfer = 0;
    PDMBLOCKTXDIR uTxDir = PDMBLOCKTXDIR_NONE;

    pbPacket = s->aATAPICmd;
    pbBuf = s->CTXSUFF(pbIOBuffer);
    switch (pbPacket[0])
    {
        case SCSI_BLANK:
            goto sendcmd;
        case SCSI_CLOSE_TRACK_SESSION:
            goto sendcmd;
        case SCSI_ERASE_10:
            iATAPILBA = ataBE2H_U32(pbPacket + 2);
            cbTransfer = ataBE2H_U16(pbPacket + 7);
            Log2(("ATAPI PT: lba %d\n", iATAPILBA));
            uTxDir = PDMBLOCKTXDIR_TO_DEVICE;
            goto sendcmd;
        case SCSI_FORMAT_UNIT:
            cbTransfer = s->uATARegLCyl | (s->uATARegHCyl << 8); /* use ATAPI transfer length */
            uTxDir = PDMBLOCKTXDIR_TO_DEVICE;
            goto sendcmd;
        case SCSI_GET_CONFIGURATION:
            cbTransfer = ataBE2H_U16(pbPacket + 7);
            uTxDir = PDMBLOCKTXDIR_FROM_DEVICE;
            goto sendcmd;
        case SCSI_GET_EVENT_STATUS_NOTIFICATION:
            cbTransfer = ataBE2H_U16(pbPacket + 7);
            uTxDir = PDMBLOCKTXDIR_FROM_DEVICE;
            goto sendcmd;
        case SCSI_GET_PERFORMANCE:
            cbTransfer = s->uATARegLCyl | (s->uATARegHCyl << 8); /* use ATAPI transfer length */
            uTxDir = PDMBLOCKTXDIR_FROM_DEVICE;
            goto sendcmd;
        case SCSI_INQUIRY:
            cbTransfer = pbPacket[4];
            uTxDir = PDMBLOCKTXDIR_FROM_DEVICE;
            goto sendcmd;
        case SCSI_LOAD_UNLOAD_MEDIUM:
            goto sendcmd;
        case SCSI_MECHANISM_STATUS:
            cbTransfer = ataBE2H_U16(pbPacket + 8);
            uTxDir = PDMBLOCKTXDIR_FROM_DEVICE;
            goto sendcmd;
        case SCSI_MODE_SELECT_10:
            cbTransfer = ataBE2H_U16(pbPacket + 7);
            uTxDir = PDMBLOCKTXDIR_TO_DEVICE;
            goto sendcmd;
        case SCSI_MODE_SENSE_10:
            cbTransfer = ataBE2H_U16(pbPacket + 7);
            uTxDir = PDMBLOCKTXDIR_FROM_DEVICE;
            goto sendcmd;
        case SCSI_PAUSE_RESUME:
            goto sendcmd;
        case SCSI_PLAY_AUDIO_10:
            goto sendcmd;
        case SCSI_PLAY_AUDIO_12:
            goto sendcmd;
        case SCSI_PLAY_AUDIO_MSF:
            goto sendcmd;
        case SCSI_PREVENT_ALLOW_MEDIUM_REMOVAL:
            /** @todo do not forget to unlock when a VM is shut down */
            goto sendcmd;
        case SCSI_READ_10:
            iATAPILBA = ataBE2H_U32(pbPacket + 2);
            cSectors = ataBE2H_U16(pbPacket + 7);
            Log2(("ATAPI PT: lba %d sectors %d\n", iATAPILBA, cSectors));
            s->cbATAPISector = 2048; /**< @todo this size is not always correct */
            cbTransfer = cSectors * s->cbATAPISector;
            uTxDir = PDMBLOCKTXDIR_FROM_DEVICE;
            goto sendcmd;
        case SCSI_READ_12:
            iATAPILBA = ataBE2H_U32(pbPacket + 2);
            cSectors = ataBE2H_U32(pbPacket + 6);
            Log2(("ATAPI PT: lba %d sectors %d\n", iATAPILBA, cSectors));
            s->cbATAPISector = 2048; /**< @todo this size is not always correct */
            cbTransfer = cSectors * s->cbATAPISector;
            uTxDir = PDMBLOCKTXDIR_FROM_DEVICE;
            goto sendcmd;
        case SCSI_READ_BUFFER:
            cbTransfer = ataBE2H_U24(pbPacket + 6);
            uTxDir = PDMBLOCKTXDIR_FROM_DEVICE;
            goto sendcmd;
        case SCSI_READ_BUFFER_CAPACITY:
            cbTransfer = ataBE2H_U16(pbPacket + 7);
            uTxDir = PDMBLOCKTXDIR_FROM_DEVICE;
            goto sendcmd;
        case SCSI_READ_CAPACITY:
            cbTransfer = 8;
            uTxDir = PDMBLOCKTXDIR_FROM_DEVICE;
            goto sendcmd;
        case SCSI_READ_CD:
            s->cbATAPISector = 2048; /**< @todo this size is not always correct */
            cbTransfer = ataBE2H_U24(pbPacket + 6) / s->cbATAPISector * s->cbATAPISector;
            uTxDir = PDMBLOCKTXDIR_FROM_DEVICE;
            goto sendcmd;
        case SCSI_READ_CD_MSF:
            cSectors = ataMSF2LBA(pbPacket + 6) - ataMSF2LBA(pbPacket + 3);
            if (cSectors > 32)
                cSectors = 32; /* Limit transfer size to 64~74K. Safety first. In any case this can only harm software doing CDDA extraction. */
            s->cbATAPISector = 2048; /**< @todo this size is not always correct */
            cbTransfer = cSectors * s->cbATAPISector;
            uTxDir = PDMBLOCKTXDIR_FROM_DEVICE;
            goto sendcmd;
        case SCSI_READ_DISC_INFORMATION:
            cbTransfer = ataBE2H_U16(pbPacket + 7);
            uTxDir = PDMBLOCKTXDIR_FROM_DEVICE;
            goto sendcmd;
        case SCSI_READ_DVD_STRUCTURE:
            cbTransfer = ataBE2H_U16(pbPacket + 8);
            uTxDir = PDMBLOCKTXDIR_FROM_DEVICE;
            goto sendcmd;
        case SCSI_READ_FORMAT_CAPACITIES:
            cbTransfer = ataBE2H_U16(pbPacket + 7);
            uTxDir = PDMBLOCKTXDIR_FROM_DEVICE;
            goto sendcmd;
        case SCSI_READ_SUBCHANNEL:
            cbTransfer = ataBE2H_U16(pbPacket + 7);
            uTxDir = PDMBLOCKTXDIR_FROM_DEVICE;
            goto sendcmd;
        case SCSI_READ_TOC_PMA_ATIP:
            cbTransfer = ataBE2H_U16(pbPacket + 7);
            uTxDir = PDMBLOCKTXDIR_FROM_DEVICE;
            goto sendcmd;
        case SCSI_READ_TRACK_INFORMATION:
            cbTransfer = ataBE2H_U16(pbPacket + 7);
            uTxDir = PDMBLOCKTXDIR_FROM_DEVICE;
            goto sendcmd;
        case SCSI_REPAIR_TRACK:
            goto sendcmd;
        case SCSI_REPORT_KEY:
            cbTransfer = ataBE2H_U16(pbPacket + 8);
            uTxDir = PDMBLOCKTXDIR_FROM_DEVICE;
            goto sendcmd;
        case SCSI_REQUEST_SENSE:
            cbTransfer = pbPacket[4];
            if (s->uATAPISenseKey != SCSI_SENSE_NONE)
            {
                ataStartTransfer(s, RT_MIN(cbTransfer, 18), PDMBLOCKTXDIR_FROM_DEVICE, ATAFN_BT_ATAPI_CMD, ATAFN_SS_ATAPI_REQUEST_SENSE, true);
                break;
            }
            uTxDir = PDMBLOCKTXDIR_FROM_DEVICE;
            goto sendcmd;
        case SCSI_RESERVE_TRACK:
            goto sendcmd;
        case SCSI_SCAN:
            goto sendcmd;
        case SCSI_SEEK_10:
            goto sendcmd;
        case SCSI_SEND_CUE_SHEET:
            cbTransfer = ataBE2H_U24(pbPacket + 6);
            uTxDir = PDMBLOCKTXDIR_TO_DEVICE;
            goto sendcmd;
        case SCSI_SEND_DVD_STRUCTURE:
            cbTransfer = ataBE2H_U16(pbPacket + 8);
            uTxDir = PDMBLOCKTXDIR_TO_DEVICE;
            goto sendcmd;
        case SCSI_SEND_EVENT:
            cbTransfer = ataBE2H_U16(pbPacket + 8);
            uTxDir = PDMBLOCKTXDIR_TO_DEVICE;
            goto sendcmd;
        case SCSI_SEND_KEY:
            cbTransfer = ataBE2H_U16(pbPacket + 8);
            uTxDir = PDMBLOCKTXDIR_TO_DEVICE;
            goto sendcmd;
        case SCSI_SEND_OPC_INFORMATION:
            cbTransfer = ataBE2H_U16(pbPacket + 7);
            uTxDir = PDMBLOCKTXDIR_TO_DEVICE;
            goto sendcmd;
        case SCSI_SET_CD_SPEED:
            goto sendcmd;
        case SCSI_SET_READ_AHEAD:
            goto sendcmd;
        case SCSI_SET_STREAMING:
            cbTransfer = ataBE2H_U16(pbPacket + 9);
            uTxDir = PDMBLOCKTXDIR_TO_DEVICE;
            goto sendcmd;
        case SCSI_START_STOP_UNIT:
            goto sendcmd;
        case SCSI_STOP_PLAY_SCAN:
            goto sendcmd;
        case SCSI_SYNCHRONIZE_CACHE:
            goto sendcmd;
        case SCSI_TEST_UNIT_READY:
            goto sendcmd;
        case SCSI_VERIFY_10:
            goto sendcmd;
        case SCSI_WRITE_10:
            iATAPILBA = ataBE2H_U32(pbPacket + 2);
            cSectors = ataBE2H_U16(pbPacket + 7);
            Log2(("ATAPI PT: lba %d sectors %d\n", iATAPILBA, cSectors));
#if 0
            /* The sector size is determined by the async I/O thread. */
            s->cbATAPISector = 0;
            /* Preliminary, will be corrected once the sector size is known. */
            cbTransfer = cSectors;
#else
            s->cbATAPISector = 2048; /**< @todo this size is not always correct */
            cbTransfer = cSectors * s->cbATAPISector;
#endif
            uTxDir = PDMBLOCKTXDIR_TO_DEVICE;
            goto sendcmd;
        case SCSI_WRITE_12:
            iATAPILBA = ataBE2H_U32(pbPacket + 2);
            cSectors = ataBE2H_U32(pbPacket + 6);
            Log2(("ATAPI PT: lba %d sectors %d\n", iATAPILBA, cSectors));
#if 0
            /* The sector size is determined by the async I/O thread. */
            s->cbATAPISector = 0;
            /* Preliminary, will be corrected once the sector size is known. */
            cbTransfer = cSectors;
#else
            s->cbATAPISector = 2048; /**< @todo this size is not always correct */
            cbTransfer = cSectors * s->cbATAPISector;
#endif
            uTxDir = PDMBLOCKTXDIR_TO_DEVICE;
            goto sendcmd;
        case SCSI_WRITE_AND_VERIFY_10:
            iATAPILBA = ataBE2H_U32(pbPacket + 2);
            cSectors = ataBE2H_U16(pbPacket + 7);
            Log2(("ATAPI PT: lba %d sectors %d\n", iATAPILBA, cSectors));
            /* The sector size is determined by the async I/O thread. */
            s->cbATAPISector = 0;
            /* Preliminary, will be corrected once the sector size is known. */
            cbTransfer = cSectors;
            uTxDir = PDMBLOCKTXDIR_TO_DEVICE;
            goto sendcmd;
        case SCSI_WRITE_BUFFER:
            switch (pbPacket[1] & 0x1f)
            {
                case 0x04: /* download microcode */
                case 0x05: /* download microcode and save */
                case 0x06: /* download microcode with offsets */
                case 0x07: /* download microcode with offsets and save */
                case 0x0e: /* download microcode with offsets and defer activation */
                case 0x0f: /* activate deferred microcode */
                    LogRel(("PIIX3 ATA: LUN#%d: CD-ROM passthrough command attempted to update firmware, blocked\n", s->iLUN));
                    atapiCmdError(s, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_INV_FIELD_IN_CMD_PACKET);
                    break;
                default:
                    cbTransfer = ataBE2H_U16(pbPacket + 6);
                    uTxDir = PDMBLOCKTXDIR_TO_DEVICE;
                    goto sendcmd;
            }
            break;
        case SCSI_REPORT_LUNS: /* Not part of MMC-3, but used by Windows. */
            cbTransfer = ataBE2H_U32(pbPacket + 6);
            uTxDir = PDMBLOCKTXDIR_FROM_DEVICE;
            goto sendcmd;
        case SCSI_REZERO_UNIT:
            /* Obsolete command used by cdrecord. What else would one expect?
             * This command is not sent to the drive, it is handled internally,
             * as the Linux kernel doesn't like it (message "scsi: unknown
             * opcode 0x01" in syslog) and replies with a sense code of 0,
             * which sends cdrecord to an endless loop. */
            atapiCmdError(s, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_ILLEGAL_OPCODE);
            break;
        default:
            LogRel(("PIIX3 ATA: LUN#%d: passthrough unimplemented for command %#x\n", s->iLUN, pbPacket[0]));
            atapiCmdError(s, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_ILLEGAL_OPCODE);
            break;
        sendcmd:
            /* Send a command to the drive, passing data in/out as required. */
            Log2(("ATAPI PT: max size %d\n", cbTransfer));
            Assert(cbTransfer <= s->cbIOBuffer);
            if (cbTransfer == 0)
                uTxDir = PDMBLOCKTXDIR_NONE;
            ataStartTransfer(s, cbTransfer, uTxDir, ATAFN_BT_ATAPI_PASSTHROUGH_CMD, ATAFN_SS_ATAPI_PASSTHROUGH, true);
    }
}


static void atapiParseCmd(ATADevState *s)
{
    const uint8_t *pbPacket;

    pbPacket = s->aATAPICmd;
#ifdef DEBUG
    Log(("%s: LUN#%d DMA=%d CMD=%#04x \"%s\"\n", __FUNCTION__, s->iLUN, s->fDMA, pbPacket[0], g_apszSCSICmdNames[pbPacket[0]]));
#else /* !DEBUG */
    Log(("%s: LUN#%d DMA=%d CMD=%#04x\n", __FUNCTION__, s->iLUN, s->fDMA, pbPacket[0]));
#endif /* !DEBUG */
    Log2(("%s: limit=%#x packet: %.*Vhxs\n", __FUNCTION__, s->uATARegLCyl | (s->uATARegHCyl << 8), ATAPI_PACKET_SIZE, pbPacket));

    if (s->fATAPIPassthrough)
        atapiParseCmdPassthrough(s);
    else
        atapiParseCmdVirtualATAPI(s);
}


static bool ataPacketSS(ATADevState *s)
{
    s->fDMA = !!(s->uATARegFeature & 1);
    memcpy(s->aATAPICmd, s->CTXSUFF(pbIOBuffer), ATAPI_PACKET_SIZE);
    s->uTxDir = PDMBLOCKTXDIR_NONE;
    s->cbTotalTransfer = 0;
    s->cbElementaryTransfer = 0;
    atapiParseCmd(s);
    return false;
}


/**
 * Called when a media is mounted.
 *
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 */
static DECLCALLBACK(void) ataMountNotify(PPDMIMOUNTNOTIFY pInterface)
{
    ATADevState *pIf = PDMIMOUNTNOTIFY_2_ATASTATE(pInterface);
    Log(("%s: changing LUN#%d\n", __FUNCTION__, pIf->iLUN));

    /* Ignore the call if we're called while being attached. */
    if (!pIf->pDrvBlock)
        return;

    if (pIf->fATAPI)
        pIf->cTotalSectors = pIf->pDrvBlock->pfnGetSize(pIf->pDrvBlock) / 2048;
    else
        pIf->cTotalSectors = pIf->pDrvBlock->pfnGetSize(pIf->pDrvBlock) / 512;

    /* Report media changed in TEST UNIT and other (probably incorrect) places. */
    if (pIf->cNotifiedMediaChange < 2)
        pIf->cNotifiedMediaChange = 2;
}

/**
 * Called when a media is unmounted
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 */
static DECLCALLBACK(void) ataUnmountNotify(PPDMIMOUNTNOTIFY pInterface)
{
    ATADevState *pIf = PDMIMOUNTNOTIFY_2_ATASTATE(pInterface);
    Log(("%s:\n", __FUNCTION__));
    pIf->cTotalSectors = 0;

    /*
     * Whatever I do, XP will not use the GET MEDIA STATUS nor the EVENT stuff.
     * However, it will respond to TEST UNIT with a 0x6 0x28 (media changed) sense code.
     * So, we'll give it 4 TEST UNIT command to catch up, two which the media is not
     * present and 2 in which it is changed.
     */
    pIf->cNotifiedMediaChange = 4;
}

static void ataPacketBT(ATADevState *s)
{
    s->cbElementaryTransfer = s->cbTotalTransfer;
    s->uATARegNSector = (s->uATARegNSector & ~7) | ATAPI_INT_REASON_CD;
    Log2(("%s: interrupt reason %#04x\n", __FUNCTION__, s->uATARegNSector));
    ataSetStatusValue(s, ATA_STAT_READY);
}


static void ataResetDevice(ATADevState *s)
{
    s->cMultSectors = ATA_MAX_MULT_SECTORS;
    s->cNotifiedMediaChange = 0;
    ataUnsetIRQ(s);

    s->uATARegSelect = 0x20;
    ataSetStatusValue(s, ATA_STAT_READY);
    ataSetSignature(s);
    s->cbTotalTransfer = 0;
    s->cbElementaryTransfer = 0;
    s->iIOBufferPIODataStart = 0;
    s->iIOBufferPIODataEnd = 0;
    s->iBeginTransfer = ATAFN_BT_NULL;
    s->iSourceSink = ATAFN_SS_NULL;
    s->fATAPITransfer = false;
    s->uATATransferMode = ATA_MODE_UDMA | 2; /* PIIX3 supports only up to UDMA2 */

    s->uATARegFeature = 0;
}


static bool ataExecuteDeviceDiagnosticSS(ATADevState *s)
{
    ataSetSignature(s);
    ataSetStatusValue(s, 0); /* NOTE: READY is _not_ set */
    s->uATARegError = 0x01;
    return false;
}


static void ataParseCmd(ATADevState *s, uint8_t cmd)
{
#ifdef DEBUG
    Log(("%s: LUN#%d CMD=%#04x \"%s\"\n", __FUNCTION__, s->iLUN, cmd, g_apszATACmdNames[cmd]));
#else /* !DEBUG */
    Log(("%s: LUN#%d CMD=%#04x\n", __FUNCTION__, s->iLUN, cmd));
#endif /* !DEBUG */
    s->fLBA48 = false;
    s->fDMA = false;
    if (cmd == ATA_IDLE_IMMEDIATE)
    {
        /* Detect Linux timeout recovery, first tries IDLE IMMEDIATE (which
         * would overwrite the failing command unfortunately), then RESET. */
        int32_t uCmdWait = -1;
        uint64_t uNow = RTTimeNanoTS();
        if (s->u64CmdTS)
            uCmdWait = (uNow - s->u64CmdTS) / 1000;
        LogRel(("PIIX3 ATA: LUN#%d: IDLE IMMEDIATE, CmdIf=%#04x (%d usec ago)\n",
                s->iLUN, s->uATARegCommand, uCmdWait));
    }
    s->uATARegCommand = cmd;
    switch (cmd)
    {
        case ATA_IDENTIFY_DEVICE:
            if (s->pDrvBlock && !s->fATAPI)
                ataStartTransfer(s, 512, PDMBLOCKTXDIR_FROM_DEVICE, ATAFN_BT_NULL, ATAFN_SS_IDENTIFY, false);
            else
            {
                if (s->fATAPI)
                    ataSetSignature(s);
                ataCmdError(s, ABRT_ERR);
                ataSetIRQ(s); /* Shortcut, do not use AIO thread. */
            }
            break;
        case ATA_INITIALIZE_DEVICE_PARAMETERS:
        case ATA_RECALIBRATE:
            ataCmdOK(s, ATA_STAT_SEEK);
            ataSetIRQ(s); /* Shortcut, do not use AIO thread. */
            break;
        case ATA_SET_MULTIPLE_MODE:
            if (    s->uATARegNSector != 0
                &&  (   s->uATARegNSector > ATA_MAX_MULT_SECTORS
                     || (s->uATARegNSector & (s->uATARegNSector - 1)) != 0))
            {
                ataCmdError(s, ABRT_ERR);
            }
            else
            {
                Log2(("%s: set multi sector count to %d\n", __FUNCTION__, s->uATARegNSector));
                s->cMultSectors = s->uATARegNSector;
                ataCmdOK(s, 0);
            }
            ataSetIRQ(s); /* Shortcut, do not use AIO thread. */
            break;
        case ATA_READ_VERIFY_SECTORS_EXT:
            s->fLBA48 = true;
        case ATA_READ_VERIFY_SECTORS:
        case ATA_READ_VERIFY_SECTORS_WITHOUT_RETRIES:
            /* do sector number check ? */
            ataCmdOK(s, 0);
            ataSetIRQ(s); /* Shortcut, do not use AIO thread. */
            break;
        case ATA_READ_SECTORS_EXT:
            s->fLBA48 = true;
        case ATA_READ_SECTORS:
        case ATA_READ_SECTORS_WITHOUT_RETRIES:
            if (!s->pDrvBlock)
                goto abort_cmd;
            s->cSectorsPerIRQ = 1;
            ataStartTransfer(s, ataGetNSectors(s) * 512, PDMBLOCKTXDIR_FROM_DEVICE, ATAFN_BT_READ_WRITE_SECTORS, ATAFN_SS_READ_SECTORS, false);
            break;
        case ATA_WRITE_SECTORS_EXT:
            s->fLBA48 = true;
        case ATA_WRITE_SECTORS:
        case ATA_WRITE_SECTORS_WITHOUT_RETRIES:
            s->cSectorsPerIRQ = 1;
            ataStartTransfer(s, ataGetNSectors(s) * 512, PDMBLOCKTXDIR_TO_DEVICE, ATAFN_BT_READ_WRITE_SECTORS, ATAFN_SS_WRITE_SECTORS, false);
            break;
        case ATA_READ_MULTIPLE_EXT:
            s->fLBA48 = true;
        case ATA_READ_MULTIPLE:
            if (!s->cMultSectors)
                goto abort_cmd;
            s->cSectorsPerIRQ = s->cMultSectors;
            ataStartTransfer(s, ataGetNSectors(s) * 512, PDMBLOCKTXDIR_FROM_DEVICE, ATAFN_BT_READ_WRITE_SECTORS, ATAFN_SS_READ_SECTORS, false);
            break;
        case ATA_WRITE_MULTIPLE_EXT:
            s->fLBA48 = true;
        case ATA_WRITE_MULTIPLE:
            if (!s->cMultSectors)
                goto abort_cmd;
            s->cSectorsPerIRQ = s->cMultSectors;
            ataStartTransfer(s, ataGetNSectors(s) * 512, PDMBLOCKTXDIR_TO_DEVICE, ATAFN_BT_READ_WRITE_SECTORS, ATAFN_SS_WRITE_SECTORS, false);
            break;
        case ATA_READ_DMA_EXT:
            s->fLBA48 = true;
        case ATA_READ_DMA:
        case ATA_READ_DMA_WITHOUT_RETRIES:
            if (!s->pDrvBlock)
                goto abort_cmd;
            s->cSectorsPerIRQ = ATA_MAX_MULT_SECTORS;
            s->fDMA = true;
            ataStartTransfer(s, ataGetNSectors(s) * 512, PDMBLOCKTXDIR_FROM_DEVICE, ATAFN_BT_READ_WRITE_SECTORS, ATAFN_SS_READ_SECTORS, false);
            break;
        case ATA_WRITE_DMA_EXT:
            s->fLBA48 = true;
        case ATA_WRITE_DMA:
        case ATA_WRITE_DMA_WITHOUT_RETRIES:
            if (!s->pDrvBlock)
                goto abort_cmd;
            s->cSectorsPerIRQ = ATA_MAX_MULT_SECTORS;
            s->fDMA = true;
            ataStartTransfer(s, ataGetNSectors(s) * 512, PDMBLOCKTXDIR_TO_DEVICE, ATAFN_BT_READ_WRITE_SECTORS, ATAFN_SS_WRITE_SECTORS, false);
            break;
        case ATA_READ_NATIVE_MAX_ADDRESS_EXT:
            s->fLBA48 = true;
            ataSetSector(s, s->cTotalSectors - 1);
            ataCmdOK(s, 0);
            ataSetIRQ(s); /* Shortcut, do not use AIO thread. */
            break;
        case ATA_READ_NATIVE_MAX_ADDRESS:
            ataSetSector(s, RT_MIN(s->cTotalSectors, 1 << 28) - 1);
            ataCmdOK(s, 0);
            ataSetIRQ(s); /* Shortcut, do not use AIO thread. */
            break;
        case ATA_CHECK_POWER_MODE:
            s->uATARegNSector = 0xff; /* drive active or idle */
            ataCmdOK(s, 0);
            ataSetIRQ(s); /* Shortcut, do not use AIO thread. */
            break;
        case ATA_SET_FEATURES:
            Log2(("%s: feature=%#x\n", __FUNCTION__, s->uATARegFeature));
            if (!s->pDrvBlock)
                goto abort_cmd;
            switch (s->uATARegFeature)
            {
                case 0x02: /* write cache enable */
                    Log2(("%s: write cache enable\n", __FUNCTION__));
                    ataCmdOK(s, ATA_STAT_SEEK);
                    ataSetIRQ(s); /* Shortcut, do not use AIO thread. */
                    break;
                case 0xaa: /* read look-ahead enable */
                    Log2(("%s: read look-ahead enable\n", __FUNCTION__));
                    ataCmdOK(s, ATA_STAT_SEEK);
                    ataSetIRQ(s); /* Shortcut, do not use AIO thread. */
                    break;
                case 0x55: /* read look-ahead disable */
                    Log2(("%s: read look-ahead disable\n", __FUNCTION__));
                    ataCmdOK(s, ATA_STAT_SEEK);
                    ataSetIRQ(s); /* Shortcut, do not use AIO thread. */
                    break;
                case 0xcc: /* reverting to power-on defaults enable */
                    Log2(("%s: revert to power-on defaults enable\n", __FUNCTION__));
                    ataCmdOK(s, ATA_STAT_SEEK);
                    ataSetIRQ(s); /* Shortcut, do not use AIO thread. */
                    break;
                case 0x66: /* reverting to power-on defaults disable */
                    Log2(("%s: revert to power-on defaults disable\n", __FUNCTION__));
                    ataCmdOK(s, ATA_STAT_SEEK);
                    ataSetIRQ(s); /* Shortcut, do not use AIO thread. */
                    break;
                case 0x82: /* write cache disable */
                    Log2(("%s: write cache disable\n", __FUNCTION__));
                    /* As per the ATA/ATAPI-6 specs, a write cache disable
                     * command MUST flush the write buffers to disc. */
                    ataStartTransfer(s, 0, PDMBLOCKTXDIR_NONE, ATAFN_BT_NULL, ATAFN_SS_FLUSH, false);
                    break;
                case 0x03: { /* set transfer mode */
                    Log2(("%s: transfer mode %#04x\n", __FUNCTION__, s->uATARegNSector));
                    switch (s->uATARegNSector & 0xf8)
                    {
                        case 0x00: /* PIO default */
                        case 0x08: /* PIO mode */
                            break;
                        case ATA_MODE_MDMA: /* MDMA mode */
                            s->uATATransferMode = (s->uATARegNSector & 0xf8) | RT_MIN(s->uATARegNSector & 0x07, ATA_MDMA_MODE_MAX);
                            break;
                        case ATA_MODE_UDMA: /* UDMA mode */
                            s->uATATransferMode = (s->uATARegNSector & 0xf8) | RT_MIN(s->uATARegNSector & 0x07, ATA_UDMA_MODE_MAX);
                            break;
                        default:
                            goto abort_cmd;
                    }
                    ataCmdOK(s, ATA_STAT_SEEK);
                    ataSetIRQ(s); /* Shortcut, do not use AIO thread. */
                    break;
                }
                default:
                    goto abort_cmd;
            }
            /*
             * OS/2 workarond:
             * The OS/2 IDE driver from MCP2 appears to rely on the feature register being
             * reset here. According to the specification, this is a driver bug as the register
             * contents are undefined after the call. This means we can just as well reset it.
             */
            s->uATARegFeature = 0;
            break;
        case ATA_FLUSH_CACHE_EXT:
        case ATA_FLUSH_CACHE:
            if (!s->pDrvBlock || s->fATAPI)
                goto abort_cmd;
            ataStartTransfer(s, 0, PDMBLOCKTXDIR_NONE, ATAFN_BT_NULL, ATAFN_SS_FLUSH, false);
            break;
        case ATA_STANDBY_IMMEDIATE:
            ataCmdOK(s, 0);
            ataSetIRQ(s); /* Shortcut, do not use AIO thread. */
            break;
        case ATA_IDLE_IMMEDIATE:
            LogRel(("PIIX3 ATA: LUN#%d: aborting current command\n", s->iLUN));
            ataAbortCurrentCommand(s, false);
            break;
            /* ATAPI commands */
        case ATA_IDENTIFY_PACKET_DEVICE:
            if (s->fATAPI)
                ataStartTransfer(s, 512, PDMBLOCKTXDIR_FROM_DEVICE, ATAFN_BT_NULL, ATAFN_SS_ATAPI_IDENTIFY, false);
            else
            {
                ataCmdError(s, ABRT_ERR);
                ataSetIRQ(s); /* Shortcut, do not use AIO thread. */
            }
            break;
        case ATA_EXECUTE_DEVICE_DIAGNOSTIC:
            ataStartTransfer(s, 0, PDMBLOCKTXDIR_NONE, ATAFN_BT_NULL, ATAFN_SS_EXECUTE_DEVICE_DIAGNOSTIC, false);
            break;
        case ATA_DEVICE_RESET:
            if (!s->fATAPI)
                goto abort_cmd;
            LogRel(("PIIX3 ATA: LUN#%d: performing device RESET\n", s->iLUN));
            ataAbortCurrentCommand(s, true);
            break;
        case ATA_PACKET:
            if (!s->fATAPI)
                goto abort_cmd;
            /* overlapping commands not supported */
            if (s->uATARegFeature & 0x02)
                goto abort_cmd;
            ataStartTransfer(s, ATAPI_PACKET_SIZE, PDMBLOCKTXDIR_TO_DEVICE, ATAFN_BT_PACKET, ATAFN_SS_PACKET, false);
            break;
        default:
        abort_cmd:
            ataCmdError(s, ABRT_ERR);
            ataSetIRQ(s); /* Shortcut, do not use AIO thread. */
            break;
    }
}


/**
 * Waits for a particular async I/O thread to complete whatever it
 * is doing at the moment.
 *
 * @returns true on success.
 * @returns false when the thread is still processing.
 * @param   pData       Pointer to the controller data.
 * @param   cMillies    How long to wait (total).
 */
static bool ataWaitForAsyncIOIsIdle(PATACONTROLLER pCtl, unsigned cMillies)
{
    uint64_t        u64Start;

    /*
     * Wait for any pending async operation to finish
     */
    u64Start = RTTimeMilliTS();
    for (;;)
    {
        if (ataAsyncIOIsIdle(pCtl, false))
            return true;
        if (RTTimeMilliTS() - u64Start >= cMillies)
            break;

        /* Sleep for a bit. */
        RTThreadSleep(100);
    }

    return false;
}

#endif /* IN_RING3 */

static int ataIOPortWriteU8(PATACONTROLLER pCtl, uint32_t addr, uint32_t val)
{
    Log2(("%s: write addr=%#x val=%#04x\n", __FUNCTION__, addr, val));
    addr &= 7;
    switch (addr)
    {
        case 0:
            break;
        case 1: /* feature register */
            /* NOTE: data is written to the two drives */
            pCtl->aIfs[0].uATARegDevCtl &= ~ATA_DEVCTL_HOB;
            pCtl->aIfs[1].uATARegDevCtl &= ~ATA_DEVCTL_HOB;
            pCtl->aIfs[0].uATARegFeatureHOB = pCtl->aIfs[0].uATARegFeature;
            pCtl->aIfs[1].uATARegFeatureHOB = pCtl->aIfs[1].uATARegFeature;
            pCtl->aIfs[0].uATARegFeature = val;
            pCtl->aIfs[1].uATARegFeature = val;
            break;
        case 2: /* sector count */
            pCtl->aIfs[0].uATARegDevCtl &= ~ATA_DEVCTL_HOB;
            pCtl->aIfs[1].uATARegDevCtl &= ~ATA_DEVCTL_HOB;
            pCtl->aIfs[0].uATARegNSectorHOB = pCtl->aIfs[0].uATARegNSector;
            pCtl->aIfs[1].uATARegNSectorHOB = pCtl->aIfs[1].uATARegNSector;
            pCtl->aIfs[0].uATARegNSector = val;
            pCtl->aIfs[1].uATARegNSector = val;
            break;
        case 3: /* sector number */
            pCtl->aIfs[0].uATARegDevCtl &= ~ATA_DEVCTL_HOB;
            pCtl->aIfs[1].uATARegDevCtl &= ~ATA_DEVCTL_HOB;
            pCtl->aIfs[0].uATARegSectorHOB = pCtl->aIfs[0].uATARegSector;
            pCtl->aIfs[1].uATARegSectorHOB = pCtl->aIfs[1].uATARegSector;
            pCtl->aIfs[0].uATARegSector = val;
            pCtl->aIfs[1].uATARegSector = val;
            break;
        case 4: /* cylinder low */
            pCtl->aIfs[0].uATARegDevCtl &= ~ATA_DEVCTL_HOB;
            pCtl->aIfs[1].uATARegDevCtl &= ~ATA_DEVCTL_HOB;
            pCtl->aIfs[0].uATARegLCylHOB = pCtl->aIfs[0].uATARegLCyl;
            pCtl->aIfs[1].uATARegLCylHOB = pCtl->aIfs[1].uATARegLCyl;
            pCtl->aIfs[0].uATARegLCyl = val;
            pCtl->aIfs[1].uATARegLCyl = val;
            break;
        case 5: /* cylinder high */
            pCtl->aIfs[0].uATARegDevCtl &= ~ATA_DEVCTL_HOB;
            pCtl->aIfs[1].uATARegDevCtl &= ~ATA_DEVCTL_HOB;
            pCtl->aIfs[0].uATARegHCylHOB = pCtl->aIfs[0].uATARegHCyl;
            pCtl->aIfs[1].uATARegHCylHOB = pCtl->aIfs[1].uATARegHCyl;
            pCtl->aIfs[0].uATARegHCyl = val;
            pCtl->aIfs[1].uATARegHCyl = val;
            break;
        case 6: /* drive/head */
            pCtl->aIfs[0].uATARegSelect = (val & ~0x10) | 0xa0;
            pCtl->aIfs[1].uATARegSelect = (val | 0x10) | 0xa0;
            if (((val >> 4) & 1) != pCtl->iSelectedIf)
            {
                PPDMDEVINS pDevIns = CONTROLLER_2_DEVINS(pCtl);

                /* select another drive */
                pCtl->iSelectedIf = (val >> 4) & 1;
                /* The IRQ line is multiplexed between the two drives, so
                 * update the state when switching to another drive. Only need
                 * to update interrupt line if it is enabled and there is a
                 * state change. */
                if (    !(pCtl->aIfs[pCtl->iSelectedIf].uATARegDevCtl & ATA_DEVCTL_DISABLE_IRQ)
                    &&  (   pCtl->aIfs[pCtl->iSelectedIf].fIrqPending
                         !=  pCtl->aIfs[pCtl->iSelectedIf ^ 1].fIrqPending))
                {
                    if (pCtl->aIfs[pCtl->iSelectedIf].fIrqPending)
                    {
                        Log2(("%s: LUN#%d asserting IRQ (drive select change)\n", __FUNCTION__, pCtl->aIfs[pCtl->iSelectedIf].iLUN));
                        /* The BMDMA unit unconditionally sets BM_STATUS_INT if
                         * the interrupt line is asserted. It monitors the line
                         * for a rising edge. */
                        pCtl->BmDma.u8Status |= BM_STATUS_INT;
                        if (pCtl->irq == 16)
                            PDMDevHlpPCISetIrqNoWait(pDevIns, 0, 1);
                        else
                            PDMDevHlpISASetIrqNoWait(pDevIns, pCtl->irq, 1);
                    }
                    else
                    {
                        Log2(("%s: LUN#%d deasserting IRQ (drive select change)\n", __FUNCTION__, pCtl->aIfs[pCtl->iSelectedIf].iLUN));
                        if (pCtl->irq == 16)
                            PDMDevHlpPCISetIrqNoWait(pDevIns, 0, 0);
                        else
                            PDMDevHlpISASetIrqNoWait(pDevIns, pCtl->irq, 0);
                    }
                }
            }
            break;
        default:
        case 7: /* command */
            /* ignore commands to non existant slave */
            if (pCtl->iSelectedIf && !pCtl->aIfs[pCtl->iSelectedIf].pDrvBlock)
                break;
#ifndef IN_RING3
            /* Don't do anything complicated in GC */
            return VINF_IOM_HC_IOPORT_WRITE;
#else /* IN_RING3 */
            ataParseCmd(&pCtl->aIfs[pCtl->iSelectedIf], val);
#endif /* !IN_RING3 */
    }
    return VINF_SUCCESS;
}


static int ataIOPortReadU8(PATACONTROLLER pCtl, uint32_t addr, uint32_t *pu32)
{
    ATADevState *s = &pCtl->aIfs[pCtl->iSelectedIf];
    uint32_t val;
    bool fHOB;

    fHOB = !!(s->uATARegDevCtl & (1 << 7));
    switch (addr & 7)
    {
        case 0: /* data register */
            val = 0xff;
            break;
        case 1: /* error register */
            /* The ATA specification is very terse when it comes to specifying
             * the precise effects of reading back the error/feature register.
             * The error register (read-only) shares the register number with
             * the feature register (write-only), so it seems that it's not
             * necessary to support the usual HOB readback here. */
            if (!s->pDrvBlock)
                val = 0;
            else
                val = s->uATARegError;
            break;
        case 2: /* sector count */
            if (!s->pDrvBlock)
                val = 0;
            else if (fHOB)
                val = s->uATARegNSectorHOB;
            else
                val = s->uATARegNSector;
            break;
        case 3: /* sector number */
            if (!s->pDrvBlock)
                val = 0;
            else if (fHOB)
                val = s->uATARegSectorHOB;
            else
                val = s->uATARegSector;
            break;
        case 4: /* cylinder low */
            if (!s->pDrvBlock)
                val = 0;
            else if (fHOB)
                val = s->uATARegLCylHOB;
            else
                val = s->uATARegLCyl;
            break;
        case 5: /* cylinder high */
            if (!s->pDrvBlock)
                val = 0;
            else if (fHOB)
                val = s->uATARegHCylHOB;
            else
                val = s->uATARegHCyl;
            break;
        case 6: /* drive/head */
            /* This register must always work as long as there is at least
             * one drive attached to the controller. It is common between
             * both drives anyway (completely identical content). */
            if (!pCtl->aIfs[0].pDrvBlock && !pCtl->aIfs[1].pDrvBlock)
                val = 0;
            else
                val = s->uATARegSelect;
            break;
        default:
        case 7: /* primary status */
        {
            /* Counter for number of busy status seen in GC in a row. */
            static unsigned cBusy = 0;

            if (!s->pDrvBlock)
                val = 0;
            else
                val = s->uATARegStatus;

            /* Give the async I/O thread an opportunity to make progress,
             * don't let it starve by guests polling frequently. EMT has a
             * lower priority than the async I/O thread, but sometimes the
             * host OS doesn't care. With some guests we are only allowed to
             * be busy for about 5 milliseconds in some situations. Note that
             * this is no guarantee for any other VBox thread getting
             * scheduled, so this just lowers the CPU load a bit when drives
             * are busy. It cannot help with timing problems. */
            if (val & ATA_STAT_BUSY)
            {
#ifdef IN_RING3
                cBusy = 0;
                PDMCritSectLeave(&pCtl->lock);

                RTThreadYield();

                {
                    STAM_PROFILE_START(&pCtl->StatLockWait, a);
                    PDMCritSectEnter(&pCtl->lock, VINF_SUCCESS);
                    STAM_PROFILE_STOP(&pCtl->StatLockWait, a);
                }

                val = s->uATARegStatus;
#else /* !IN_RING3 */
                /* Cannot yield CPU in guest context. And switching to host
                 * context for each and every busy status is too costly,
                 * especially on SMP systems where we don't gain much by
                 * yielding the CPU to someone else. */
                if (++cBusy >= 20)
                {
                    cBusy = 0;
                    return VINF_IOM_HC_IOPORT_READ;
                }
#endif /* !IN_RING3 */
            }
            else
                cBusy = 0;
            ataUnsetIRQ(s);
            break;
        }
    }
    Log2(("%s: addr=%#x val=%#04x\n", __FUNCTION__, addr, val));
    *pu32 = val;
    return VINF_SUCCESS;
}


static uint32_t ataStatusRead(PATACONTROLLER pCtl, uint32_t addr)
{
    ATADevState *s = &pCtl->aIfs[pCtl->iSelectedIf];
    uint32_t val;

    if ((!pCtl->aIfs[0].pDrvBlock && !pCtl->aIfs[1].pDrvBlock) ||
            (pCtl->iSelectedIf == 1 && !s->pDrvBlock))
        val = 0;
    else
        val = s->uATARegStatus;
    Log2(("%s: addr=%#x val=%#04x\n", __FUNCTION__, addr, val));
    return val;
}

static int ataControlWrite(PATACONTROLLER pCtl, uint32_t addr, uint32_t val)
{
#ifndef IN_RING3
    if ((val ^ pCtl->aIfs[0].uATARegDevCtl) & ATA_DEVCTL_RESET)
        return VINF_IOM_HC_IOPORT_WRITE; /* The RESET stuff is too complicated for GC. */
#endif /* !IN_RING3 */

    Log2(("%s: addr=%#x val=%#04x\n", __FUNCTION__, addr, val));
    /* RESET is common for both drives attached to a controller. */
    if (!(pCtl->aIfs[0].uATARegDevCtl & ATA_DEVCTL_RESET) &&
            (val & ATA_DEVCTL_RESET))
    {
#ifdef IN_RING3
        /* Software RESET low to high */
        int32_t uCmdWait0 = -1, uCmdWait1 = -1;
        uint64_t uNow = RTTimeNanoTS();
        if (pCtl->aIfs[0].u64CmdTS)
            uCmdWait0 = (uNow - pCtl->aIfs[0].u64CmdTS) / 1000;
        if (pCtl->aIfs[1].u64CmdTS)
            uCmdWait1 = (uNow - pCtl->aIfs[1].u64CmdTS) / 1000;
        LogRel(("PIIX3 ATA: Ctl#%d: RESET, DevSel=%d AIOIf=%d CmdIf0=%#04x (%d usec ago) CmdIf1=%#04x (%d usec ago)\n",
                    ATACONTROLLER_IDX(pCtl), pCtl->iSelectedIf, pCtl->iAIOIf,
                    pCtl->aIfs[0].uATARegCommand, uCmdWait0,
                    pCtl->aIfs[1].uATARegCommand, uCmdWait1));
        pCtl->fReset = true;
        /* Everything must be done after the reset flag is set, otherwise
         * there are unavoidable races with the currently executing request
         * (which might just finish in the mean time). */
        pCtl->fChainedTransfer = false;
        for (uint32_t i = 0; i < RT_ELEMENTS(pCtl->aIfs); i++)
        {
            ataResetDevice(&pCtl->aIfs[i]);
            /* The following cannot be done using ataSetStatusValue() since the
             * reset flag is already set, which suppresses all status changes. */
            pCtl->aIfs[i].uATARegStatus = ATA_STAT_BUSY | ATA_STAT_SEEK;
            Log2(("%s: LUN#%d status %#04x\n", __FUNCTION__, pCtl->aIfs[i].iLUN, pCtl->aIfs[i].uATARegStatus));
            pCtl->aIfs[i].uATARegError = 0x01;
        }
        ataAsyncIOClearRequests(pCtl);
        Log2(("%s: Ctl#%d: message to async I/O thread, resetA\n", __FUNCTION__, ATACONTROLLER_IDX(pCtl)));
        if (val & ATA_DEVCTL_HOB)
        {
            val &= ~ATA_DEVCTL_HOB;
            Log2(("%s: ignored setting HOB\n", __FUNCTION__));
        }
        ataAsyncIOPutRequest(pCtl, &ataResetARequest);
#else /* !IN_RING3 */
        AssertMsgFailed(("RESET handling is too complicated for GC\n"));
#endif /* IN_RING3 */
    }
    else if ((pCtl->aIfs[0].uATARegDevCtl & ATA_DEVCTL_RESET) &&
               !(val & ATA_DEVCTL_RESET))
    {
#ifdef IN_RING3
        /* Software RESET high to low */
        Log(("%s: deasserting RESET\n", __FUNCTION__));
        Log2(("%s: Ctl#%d: message to async I/O thread, resetC\n", __FUNCTION__, ATACONTROLLER_IDX(pCtl)));
        if (val & ATA_DEVCTL_HOB)
        {
            val &= ~ATA_DEVCTL_HOB;
            Log2(("%s: ignored setting HOB\n", __FUNCTION__));
        }
        ataAsyncIOPutRequest(pCtl, &ataResetCRequest);
#else /* !IN_RING3 */
        AssertMsgFailed(("RESET handling is too complicated for GC\n"));
#endif /* IN_RING3 */
    }

    /* Change of interrupt disable flag. Update interrupt line if interrupt
     * is pending on the current interface. */
    if ((val ^ pCtl->aIfs[0].uATARegDevCtl) & ATA_DEVCTL_DISABLE_IRQ
        &&  pCtl->aIfs[pCtl->iSelectedIf].fIrqPending)
    {
        if (!(val & ATA_DEVCTL_DISABLE_IRQ))
        {
            Log2(("%s: LUN#%d asserting IRQ (interrupt disable change)\n", __FUNCTION__, pCtl->aIfs[pCtl->iSelectedIf].iLUN));
            /* The BMDMA unit unconditionally sets BM_STATUS_INT if the
             * interrupt line is asserted. It monitors the line for a rising
             * edge. */
            pCtl->BmDma.u8Status |= BM_STATUS_INT;
            if (pCtl->irq == 16)
                PDMDevHlpPCISetIrqNoWait(CONTROLLER_2_DEVINS(pCtl), 0, 1);
            else
                PDMDevHlpISASetIrqNoWait(CONTROLLER_2_DEVINS(pCtl), pCtl->irq, 1);
        }
        else
        {
            Log2(("%s: LUN#%d deasserting IRQ (interrupt disable change)\n", __FUNCTION__, pCtl->aIfs[pCtl->iSelectedIf].iLUN));
            if (pCtl->irq == 16)
                PDMDevHlpPCISetIrqNoWait(CONTROLLER_2_DEVINS(pCtl), 0, 0);
            else
                PDMDevHlpISASetIrqNoWait(CONTROLLER_2_DEVINS(pCtl), pCtl->irq, 0);
        }
    }

    if (val & ATA_DEVCTL_HOB)
        Log2(("%s: set HOB\n", __FUNCTION__));

    pCtl->aIfs[0].uATARegDevCtl = val;
    pCtl->aIfs[1].uATARegDevCtl = val;

    return VINF_SUCCESS;
}

#ifdef IN_RING3

static void ataPIOTransfer(PATACONTROLLER pCtl)
{
    ATADevState *s;

    s = &pCtl->aIfs[pCtl->iAIOIf];
    Log3(("%s: if=%p\n", __FUNCTION__, s));

    if (s->cbTotalTransfer && s->iIOBufferCur > s->iIOBufferEnd)
    {
        LogRel(("PIIX3 ATA: LUN#%d: %s data in the middle of a PIO transfer - VERY SLOW\n", s->iLUN, s->uTxDir == PDMBLOCKTXDIR_FROM_DEVICE ? "loading" : "storing"));
        /* Any guest OS that triggers this case has a pathetic ATA driver.
         * In a real system it would block the CPU via IORDY, here we do it
         * very similarly by not continuing with the current instruction
         * until the transfer to/from the storage medium is completed. */
        if (s->iSourceSink != ATAFN_SS_NULL)
        {
            bool fRedo;
            uint8_t status = s->uATARegStatus;
            ataSetStatusValue(s, ATA_STAT_BUSY);
            Log2(("%s: calling source/sink function\n", __FUNCTION__));
            fRedo = g_apfnSourceSinkFuncs[s->iSourceSink](s);
            pCtl->fRedo = fRedo;
            if (RT_UNLIKELY(fRedo))
                return;
            ataSetStatusValue(s, status);
            s->iIOBufferCur = 0;
            s->iIOBufferEnd = s->cbElementaryTransfer;
        }
    }
    if (s->cbTotalTransfer)
    {
        if (s->fATAPITransfer)
            ataPIOTransferLimitATAPI(s);

        if (s->uTxDir == PDMBLOCKTXDIR_TO_DEVICE && s->cbElementaryTransfer > s->cbTotalTransfer)
            s->cbElementaryTransfer = s->cbTotalTransfer;

        Log2(("%s: %s tx_size=%d elem_tx_size=%d index=%d end=%d\n",
             __FUNCTION__, s->uTxDir == PDMBLOCKTXDIR_FROM_DEVICE ? "T2I" : "I2T",
             s->cbTotalTransfer, s->cbElementaryTransfer,
             s->iIOBufferCur, s->iIOBufferEnd));
        ataPIOTransferStart(s, s->iIOBufferCur, s->cbElementaryTransfer);
        s->cbTotalTransfer -= s->cbElementaryTransfer;
        s->iIOBufferCur += s->cbElementaryTransfer;

        if (s->uTxDir == PDMBLOCKTXDIR_FROM_DEVICE && s->cbElementaryTransfer > s->cbTotalTransfer)
            s->cbElementaryTransfer = s->cbTotalTransfer;
    }
    else
        ataPIOTransferStop(s);
}


DECLINLINE(void) ataPIOTransferFinish(PATACONTROLLER pCtl, ATADevState *s)
{
    /* Do not interfere with RESET processing if the PIO transfer finishes
     * while the RESET line is asserted. */
    if (pCtl->fReset)
    {
        Log2(("%s: Ctl#%d: suppressed continuing PIO transfer as RESET is active\n", __FUNCTION__, ATACONTROLLER_IDX(pCtl)));
        return;
    }

    if (   s->uTxDir == PDMBLOCKTXDIR_TO_DEVICE
        || (   s->iSourceSink != ATAFN_SS_NULL
            && s->iIOBufferCur >= s->iIOBufferEnd))
    {
        /* Need to continue the transfer in the async I/O thread. This is
         * the case for write operations or generally for not yet finished
         * transfers (some data might need to be read). */
        ataUnsetStatus(s, ATA_STAT_READY | ATA_STAT_DRQ);
        ataSetStatus(s, ATA_STAT_BUSY);

        Log2(("%s: Ctl#%d: message to async I/O thread, continuing PIO transfer\n", __FUNCTION__, ATACONTROLLER_IDX(pCtl)));
        ataAsyncIOPutRequest(pCtl, &ataPIORequest);
    }
    else
    {
        /* Either everything finished (though some data might still be pending)
         * or some data is pending before the next read is due. */

        /* Continue a previously started transfer. */
        ataUnsetStatus(s, ATA_STAT_DRQ);
        ataSetStatus(s, ATA_STAT_READY);

        if (s->cbTotalTransfer)
        {
            /* There is more to transfer, happens usually for large ATAPI
             * reads - the protocol limits the chunk size to 65534 bytes. */
            ataPIOTransfer(pCtl);
            ataSetIRQ(s);
        }
        else
        {
            Log2(("%s: Ctl#%d: skipping message to async I/O thread, ending PIO transfer\n", __FUNCTION__, ATACONTROLLER_IDX(pCtl)));
            /* Finish PIO transfer. */
            ataPIOTransfer(pCtl);
            Assert(!pCtl->fRedo);
        }
    }
}

#endif /* IN_RING3 */

static int ataDataWrite(PATACONTROLLER pCtl, uint32_t addr, uint32_t cbSize, const uint8_t *pbBuf)
{
    ATADevState *s = &pCtl->aIfs[pCtl->iSelectedIf];
    uint8_t *p;

    if (s->iIOBufferPIODataStart < s->iIOBufferPIODataEnd)
    {
        Assert(s->uTxDir == PDMBLOCKTXDIR_TO_DEVICE);
        p = s->CTXSUFF(pbIOBuffer) + s->iIOBufferPIODataStart;
#ifndef IN_RING3
        /* All but the last transfer unit is simple enough for GC, but
         * sending a request to the async IO thread is too complicated. */
        if (s->iIOBufferPIODataStart + cbSize < s->iIOBufferPIODataEnd)
        {
            memcpy(p, pbBuf, cbSize);
            s->iIOBufferPIODataStart += cbSize;
        }
        else
            return VINF_IOM_HC_IOPORT_WRITE;
#else /* IN_RING3 */
        memcpy(p, pbBuf, cbSize);
        s->iIOBufferPIODataStart += cbSize;
        if (s->iIOBufferPIODataStart >= s->iIOBufferPIODataEnd)
            ataPIOTransferFinish(pCtl, s);
#endif /* !IN_RING3 */
    }
    else
        Log2(("%s: DUMMY data\n", __FUNCTION__));
    Log3(("%s: addr=%#x val=%.*Vhxs\n", __FUNCTION__, addr, cbSize, pbBuf));
    return VINF_SUCCESS;
}

static int ataDataRead(PATACONTROLLER pCtl, uint32_t addr, uint32_t cbSize, uint8_t *pbBuf)
{
    ATADevState *s = &pCtl->aIfs[pCtl->iSelectedIf];
    uint8_t *p;

    if (s->iIOBufferPIODataStart < s->iIOBufferPIODataEnd)
    {
        Assert(s->uTxDir == PDMBLOCKTXDIR_FROM_DEVICE);
        p = s->CTXSUFF(pbIOBuffer) + s->iIOBufferPIODataStart;
#ifndef IN_RING3
        /* All but the last transfer unit is simple enough for GC, but
         * sending a request to the async IO thread is too complicated. */
        if (s->iIOBufferPIODataStart + cbSize < s->iIOBufferPIODataEnd)
        {
            memcpy(pbBuf, p, cbSize);
            s->iIOBufferPIODataStart += cbSize;
        }
        else
            return VINF_IOM_HC_IOPORT_READ;
#else /* IN_RING3 */
        memcpy(pbBuf, p, cbSize);
        s->iIOBufferPIODataStart += cbSize;
        if (s->iIOBufferPIODataStart >= s->iIOBufferPIODataEnd)
            ataPIOTransferFinish(pCtl, s);
#endif /* !IN_RING3 */
    }
    else
    {
        Log2(("%s: DUMMY data\n", __FUNCTION__));
        memset(pbBuf, '\xff', cbSize);
    }
    Log3(("%s: addr=%#x val=%.*Vhxs\n", __FUNCTION__, addr, cbSize, pbBuf));
    return VINF_SUCCESS;
}

#ifdef IN_RING3

/* Attempt to guess the LCHS disk geometry from the MS-DOS master boot
 * record (partition table). */
static int ataGuessDiskLCHS(ATADevState *s, uint32_t *pcCylinders, uint32_t *pcHeads, uint32_t *pcSectors)
{
    uint8_t aMBR[512], *p;
    int rc;
    uint32_t iEndHead, iEndSector, cCHSCylinders, cCHSHeads, cCHSSectors;

    if (s->fATAPI || !s->pDrvBlock)
        return VERR_INVALID_PARAMETER;
    rc = s->pDrvBlock->pfnRead(s->pDrvBlock, 0, aMBR, 1 * 512);
    if (VBOX_FAILURE(rc))
        return rc;
    /* Test MBR magic number. */
    if (aMBR[510] != 0x55 || aMBR[511] != 0xaa)
        return VERR_INVALID_PARAMETER;
    for (uint32_t i = 0; i < 4; i++)
    {
        /* Figure out the start of a partition table entry. */
        p = &aMBR[0x1be + i * 16];
        iEndHead = p[5];
        iEndSector = p[6] & 63;
        if ((p[12] | p[13] | p[14] | p[15]) && iEndSector & iEndHead)
        {
            /* Assumption: partition terminates on a cylinder boundary. */
            cCHSHeads = iEndHead + 1;
            cCHSSectors = iEndSector;
            cCHSCylinders = s->cTotalSectors / (cCHSHeads * cCHSSectors);
            if (cCHSCylinders >= 1)
            {
                *pcHeads = cCHSHeads;
                *pcSectors = cCHSSectors;
                *pcCylinders = cCHSCylinders;
                Log(("%s: LCHS=%d %d %d\n", __FUNCTION__, cCHSCylinders, cCHSHeads, cCHSSectors));
                return VINF_SUCCESS;
            }
        }
    }
    return VERR_INVALID_PARAMETER;
}


static void ataDMATransferStop(ATADevState *s)
{
    s->cbTotalTransfer = 0;
    s->cbElementaryTransfer = 0;
    s->iBeginTransfer = ATAFN_BT_NULL;
    s->iSourceSink = ATAFN_SS_NULL;
}


/**
 * Perform the entire DMA transfer in one go (unless a source/sink operation
 * has to be redone or a RESET comes in between). Unlike the PIO counterpart
 * this function cannot handle empty transfers.
 *
 * @param pCtl      Controller for which to perform the transfer.
 */
static void ataDMATransfer(PATACONTROLLER pCtl)
{
    PPDMDEVINS pDevIns = CONTROLLER_2_DEVINS(pCtl);
    ATADevState *s = &pCtl->aIfs[pCtl->iAIOIf];
    bool fRedo;
    RTGCPHYS pDesc;
    uint32_t cbTotalTransfer, cbElementaryTransfer;
    uint32_t iIOBufferCur, iIOBufferEnd;
    uint32_t dmalen;
    PDMBLOCKTXDIR uTxDir;
    bool fLastDesc = false;

    Assert(sizeof(BMDMADesc) == 8);

    fRedo = pCtl->fRedo;
    if (RT_LIKELY(!fRedo))
        Assert(s->cbTotalTransfer);
    uTxDir = (PDMBLOCKTXDIR)s->uTxDir;
    cbTotalTransfer = s->cbTotalTransfer;
    cbElementaryTransfer = s->cbElementaryTransfer;
    iIOBufferCur = s->iIOBufferCur;
    iIOBufferEnd = s->iIOBufferEnd;

    /* The DMA loop is designed to hold the lock only when absolutely
     * necessary. This avoids long freezes should the guest access the
     * ATA registers etc. for some reason. */
    PDMCritSectLeave(&pCtl->lock);

    Log2(("%s: %s tx_size=%d elem_tx_size=%d index=%d end=%d\n",
         __FUNCTION__, uTxDir == PDMBLOCKTXDIR_FROM_DEVICE ? "T2I" : "I2T",
         cbTotalTransfer, cbElementaryTransfer,
         iIOBufferCur, iIOBufferEnd));
    for (pDesc = pCtl->pFirstDMADesc; pDesc <= pCtl->pLastDMADesc; pDesc += sizeof(BMDMADesc))
    {
        BMDMADesc DMADesc;
        RTGCPHYS pBuffer;
        uint32_t cbBuffer;

        if (RT_UNLIKELY(fRedo))
        {
            pBuffer = pCtl->pRedoDMABuffer;
            cbBuffer = pCtl->cbRedoDMABuffer;
            fLastDesc = pCtl->fRedoDMALastDesc;
        }
        else
        {
            PDMDevHlpPhysRead(pDevIns, pDesc, &DMADesc, sizeof(BMDMADesc));
            pBuffer = RT_LE2H_U32(DMADesc.pBuffer);
            cbBuffer = RT_LE2H_U32(DMADesc.cbBuffer);
            fLastDesc = !!(cbBuffer & 0x80000000);
            cbBuffer &= 0xfffe;
            if (cbBuffer == 0)
                cbBuffer = 0x10000;
            if (cbBuffer > cbTotalTransfer)
                cbBuffer = cbTotalTransfer;
        }

        while (RT_UNLIKELY(fRedo) || (cbBuffer && cbTotalTransfer))
        {
            if (RT_LIKELY(!fRedo))
            {
                dmalen = RT_MIN(cbBuffer, iIOBufferEnd - iIOBufferCur);
                Log2(("%s: DMA desc %#010x: addr=%#010x size=%#010x\n", __FUNCTION__,
                       (int)pDesc, pBuffer, cbBuffer));
                if (uTxDir == PDMBLOCKTXDIR_FROM_DEVICE)
                    PDMDevHlpPhysWrite(pDevIns, pBuffer, s->CTXSUFF(pbIOBuffer) + iIOBufferCur, dmalen);
                else
                    PDMDevHlpPhysRead(pDevIns, pBuffer, s->CTXSUFF(pbIOBuffer) + iIOBufferCur, dmalen);
                iIOBufferCur += dmalen;
                cbTotalTransfer -= dmalen;
                cbBuffer -= dmalen;
                pBuffer += dmalen;
            }
            if (    iIOBufferCur == iIOBufferEnd
                &&  (uTxDir == PDMBLOCKTXDIR_TO_DEVICE || cbTotalTransfer))
            {
                if (uTxDir == PDMBLOCKTXDIR_FROM_DEVICE && cbElementaryTransfer > cbTotalTransfer)
                    cbElementaryTransfer = cbTotalTransfer;

                {
                STAM_PROFILE_START(&pCtl->StatLockWait, a);
                PDMCritSectEnter(&pCtl->lock, VINF_SUCCESS);
                STAM_PROFILE_STOP(&pCtl->StatLockWait, a);
                }

                /* The RESET handler could have cleared the DMA transfer
                 * state (since we didn't hold the lock until just now
                 * the guest can continue in parallel). If so, the state
                 * is already set up so the loop is exited immediately. */
                if (s->iSourceSink != ATAFN_SS_NULL)
                {
                    s->iIOBufferCur = iIOBufferCur;
                    s->iIOBufferEnd = iIOBufferEnd;
                    s->cbElementaryTransfer = cbElementaryTransfer;
                    s->cbTotalTransfer = cbTotalTransfer;
                    Log2(("%s: calling source/sink function\n", __FUNCTION__));
                    fRedo = g_apfnSourceSinkFuncs[s->iSourceSink](s);
                    if (RT_UNLIKELY(fRedo))
                    {
                        pCtl->pFirstDMADesc = pDesc;
                        pCtl->pRedoDMABuffer = pBuffer;
                        pCtl->cbRedoDMABuffer = cbBuffer;
                        pCtl->fRedoDMALastDesc = fLastDesc;
                    }
                    else
                    {
                        cbTotalTransfer = s->cbTotalTransfer;
                        cbElementaryTransfer = s->cbElementaryTransfer;

                        if (uTxDir == PDMBLOCKTXDIR_TO_DEVICE && cbElementaryTransfer > cbTotalTransfer)
                            cbElementaryTransfer = cbTotalTransfer;
                        iIOBufferCur = 0;
                        iIOBufferEnd = cbElementaryTransfer;
                    }
                    pCtl->fRedo = fRedo;
                }
                else
                {
                    /* This forces the loop to exit immediately. */
                    pDesc = pCtl->pLastDMADesc + 1;
                }

                PDMCritSectLeave(&pCtl->lock);
                if (RT_UNLIKELY(fRedo))
                    break;
            }
        }

        if (RT_UNLIKELY(fRedo))
            break;

        /* end of transfer */
        if (!cbTotalTransfer || fLastDesc)
            break;

        {
        STAM_PROFILE_START(&pCtl->StatLockWait, a);
        PDMCritSectEnter(&pCtl->lock, VINF_SUCCESS);
        STAM_PROFILE_STOP(&pCtl->StatLockWait, a);
        }

        if (!(pCtl->BmDma.u8Cmd & BM_CMD_START) || pCtl->fReset)
        {
            LogRel(("PIIX3 ATA: Ctl#%d: ABORT DMA%s\n", ATACONTROLLER_IDX(pCtl), pCtl->fReset ? " due to RESET" : ""));
            if (!pCtl->fReset)
                ataDMATransferStop(s);
            /* This forces the loop to exit immediately. */
            pDesc = pCtl->pLastDMADesc + 1;
        }

        PDMCritSectLeave(&pCtl->lock);
    }

    {
    STAM_PROFILE_START(&pCtl->StatLockWait, a);
    PDMCritSectEnter(&pCtl->lock, VINF_SUCCESS);
    STAM_PROFILE_STOP(&pCtl->StatLockWait, a);
    }

    if (RT_UNLIKELY(fRedo))
        return;

    if (fLastDesc)
        pCtl->BmDma.u8Status &= ~BM_STATUS_DMAING;
    s->cbTotalTransfer = cbTotalTransfer;
    s->cbElementaryTransfer = cbElementaryTransfer;
    s->iIOBufferCur = iIOBufferCur;
    s->iIOBufferEnd = iIOBufferEnd;
}


/**
 * Suspend I/O operations on a controller. Also suspends EMT, because it's
 * waiting for I/O to make progress. The next attempt to perform an I/O
 * operation will be made when EMT is resumed up again (as the resume
 * callback below restarts I/O).
 *
 * @param pCtl      Controller for which to suspend I/O.
 */
static void ataSuspendRedo(PATACONTROLLER pCtl)
{
    PPDMDEVINS  pDevIns = CONTROLLER_2_DEVINS(pCtl);
    PVMREQ      pReq;
    int         rc;

    pCtl->fRedoIdle = true;
    rc = VMR3ReqCall(PDMDevHlpGetVM(pDevIns), &pReq, RT_INDEFINITE_WAIT,
                     (PFNRT)pDevIns->pDevHlp->pfnVMSuspend, 1, pDevIns);
    AssertReleaseRC(rc);
    VMR3ReqFree(pReq);
}

/** Asynch I/O thread for an interface. Once upon a time this was readable
 * code with several loops and a different semaphore for each purpose. But
 * then came the "how can one save the state in the middle of a PIO transfer"
 * question. The solution was to use an ASM, which is what's there now. */
static DECLCALLBACK(int) ataAsyncIOLoop(RTTHREAD ThreadSelf, void *pvUser)
{
    const ATARequest *pReq;
    uint64_t        u64TS = 0; /* shut up gcc */
    uint64_t        uWait;
    int             rc   = VINF_SUCCESS;
    PATACONTROLLER  pCtl = (PATACONTROLLER)pvUser;
    ATADevState     *s;

    pReq = NULL;
    pCtl->fChainedTransfer = false;
    while (!pCtl->fShutdown)
    {
        /* Keep this thread from doing anything as long as EMT is suspended. */
        while (pCtl->fRedoIdle)
        {
            rc = RTSemEventWait(pCtl->SuspendIOSem, RT_INDEFINITE_WAIT);
            if (VBOX_FAILURE(rc) || pCtl->fShutdown)
                break;

            pCtl->fRedoIdle = false;
        }

        /* Wait for work.  */
        if (pReq == NULL)
        {
            LogBird(("ata: %x: going to sleep...\n", pCtl->IOPortBase1));
            rc = RTSemEventWait(pCtl->AsyncIOSem, RT_INDEFINITE_WAIT);
            LogBird(("ata: %x: waking up\n", pCtl->IOPortBase1));
            if (VBOX_FAILURE(rc) || pCtl->fShutdown)
                break;

            pReq = ataAsyncIOGetCurrentRequest(pCtl);
        }

        if (pReq == NULL)
            continue;

        ATAAIO ReqType = pReq->ReqType;

        Log2(("%s: Ctl#%d: state=%d, req=%d\n", __FUNCTION__, ATACONTROLLER_IDX(pCtl), pCtl->uAsyncIOState, ReqType));
        if (pCtl->uAsyncIOState != ReqType)
        {
            /* The new state is not the state that was expected by the normal
             * state changes. This is either a RESET/ABORT or there's something
             * really strange going on. */
            if (    (pCtl->uAsyncIOState == ATA_AIO_PIO || pCtl->uAsyncIOState == ATA_AIO_DMA)
                &&  (ReqType == ATA_AIO_PIO || ReqType == ATA_AIO_DMA))
            {
                /* Incorrect sequence of PIO/DMA states. Dump request queue. */
                ataAsyncIODumpRequests(pCtl);
            }
            AssertReleaseMsg(ReqType == ATA_AIO_RESET_ASSERTED || ReqType == ATA_AIO_RESET_CLEARED || ReqType == ATA_AIO_ABORT || pCtl->uAsyncIOState == ReqType, ("I/O state inconsistent: state=%d request=%d\n", pCtl->uAsyncIOState, ReqType));
        }

        /* Do our work.  */
        {
        STAM_PROFILE_START(&pCtl->StatLockWait, a);
        LogBird(("ata: %x: entering critsect\n", pCtl->IOPortBase1));
        PDMCritSectEnter(&pCtl->lock, VINF_SUCCESS);
        LogBird(("ata: %x: entered\n", pCtl->IOPortBase1));
        STAM_PROFILE_STOP(&pCtl->StatLockWait, a);
        }

        if (pCtl->uAsyncIOState == ATA_AIO_NEW && !pCtl->fChainedTransfer)
        {
            u64TS = RTTimeNanoTS();
#if defined(DEBUG) || defined(VBOX_WITH_STATISTICS)
            STAM_PROFILE_ADV_START(&pCtl->StatAsyncTime, a);
#endif /* DEBUG || VBOX_WITH_STATISTICS */
        }

        switch (ReqType)
        {
            case ATA_AIO_NEW:

                pCtl->iAIOIf = pReq->u.t.iIf;
                s = &pCtl->aIfs[pCtl->iAIOIf];
                s->cbTotalTransfer = pReq->u.t.cbTotalTransfer;
                s->uTxDir = pReq->u.t.uTxDir;
                s->iBeginTransfer = pReq->u.t.iBeginTransfer;
                s->iSourceSink = pReq->u.t.iSourceSink;
                s->iIOBufferEnd = 0;
                s->u64CmdTS = u64TS;

                if (s->fATAPI)
                {
                    if (pCtl->fChainedTransfer)
                    {
                        /* Only count the actual transfers, not the PIO
                         * transfer of the ATAPI command bytes. */
                        if (s->fDMA)
                            STAM_REL_COUNTER_INC(&s->StatATAPIDMA);
                        else
                            STAM_REL_COUNTER_INC(&s->StatATAPIPIO);
                    }
                }
                else
                {
                    if (s->fDMA)
                        STAM_REL_COUNTER_INC(&s->StatATADMA);
                    else
                        STAM_REL_COUNTER_INC(&s->StatATAPIO);
                }

                pCtl->fChainedTransfer = false;

                if (s->iBeginTransfer != ATAFN_BT_NULL)
                {
                    Log2(("%s: Ctl#%d: calling begin transfer function\n", __FUNCTION__, ATACONTROLLER_IDX(pCtl)));
                    g_apfnBeginTransFuncs[s->iBeginTransfer](s);
                    s->iBeginTransfer = ATAFN_BT_NULL;
                    if (s->uTxDir != PDMBLOCKTXDIR_FROM_DEVICE)
                        s->iIOBufferEnd = s->cbElementaryTransfer;
                }
                else
                {
                    s->cbElementaryTransfer = s->cbTotalTransfer;
                    s->iIOBufferEnd = s->cbTotalTransfer;
                }
                s->iIOBufferCur = 0;

                if (s->uTxDir != PDMBLOCKTXDIR_TO_DEVICE)
                {
                    if (s->iSourceSink != ATAFN_SS_NULL)
                    {
                        bool fRedo;
                        Log2(("%s: Ctl#%d: calling source/sink function\n", __FUNCTION__, ATACONTROLLER_IDX(pCtl)));
                        fRedo = g_apfnSourceSinkFuncs[s->iSourceSink](s);
                        pCtl->fRedo = fRedo;
                        if (RT_UNLIKELY(fRedo))
                        {
                            /* Operation failed at the initial transfer, restart
                             * everything from scratch by resending the current
                             * request. Occurs very rarely, not worth optimizing. */
                            LogRel(("%s: Ctl#%d: redo entire operation\n", __FUNCTION__, ATACONTROLLER_IDX(pCtl)));
                            ataAsyncIOPutRequest(pCtl, pReq);
                            ataSuspendRedo(pCtl);
                            break;
                        }
                    }
                    else
                        ataCmdOK(s, 0);
                    s->iIOBufferEnd = s->cbElementaryTransfer;

                }

                /* Do not go into the transfer phase if RESET is asserted.
                 * The CritSect is released while waiting for the host OS
                 * to finish the I/O, thus RESET is possible here. Most
                 * important: do not change uAsyncIOState. */
                if (pCtl->fReset)
                    break;

                if (s->fDMA)
                {
                    if (s->cbTotalTransfer)
                    {
                        ataSetStatus(s, ATA_STAT_DRQ);

                        pCtl->uAsyncIOState = ATA_AIO_DMA;
                        /* If BMDMA is already started, do the transfer now. */
                        if (pCtl->BmDma.u8Cmd & BM_CMD_START)
                        {
                            Log2(("%s: Ctl#%d: message to async I/O thread, continuing DMA transfer immediately\n", __FUNCTION__, ATACONTROLLER_IDX(pCtl)));
                            ataAsyncIOPutRequest(pCtl, &ataDMARequest);
                        }
                    }
                    else
                    {
                        Assert(s->uTxDir == PDMBLOCKTXDIR_NONE); /* Any transfer which has an initial transfer size of 0 must be marked as such. */
                        /* Finish DMA transfer. */
                        ataDMATransferStop(s);
                        ataSetIRQ(s);
                        pCtl->uAsyncIOState = ATA_AIO_NEW;
                    }
                }
                else
                {
                    if (s->cbTotalTransfer)
                    {
                        ataPIOTransfer(pCtl);
                        Assert(!pCtl->fRedo);
                        if (s->fATAPITransfer || s->uTxDir != PDMBLOCKTXDIR_TO_DEVICE)
                            ataSetIRQ(s);

                        if (s->uTxDir == PDMBLOCKTXDIR_TO_DEVICE || s->iSourceSink != ATAFN_SS_NULL)
                        {
                            /* Write operations and not yet finished transfers
                             * must be completed in the async I/O thread. */
                            pCtl->uAsyncIOState = ATA_AIO_PIO;
                        }
                        else
                        {
                            /* Finished read operation can be handled inline
                             * in the end of PIO transfer handling code. Linux
                             * depends on this, as it waits only briefly for
                             * devices to become ready after incoming data
                             * transfer. Cannot find anything in the ATA spec
                             * that backs this assumption, but as all kernels
                             * are affected (though most of the time it does
                             * not cause any harm) this must work. */
                            pCtl->uAsyncIOState = ATA_AIO_NEW;
                        }
                    }
                    else
                    {
                        Assert(s->uTxDir == PDMBLOCKTXDIR_NONE); /* Any transfer which has an initial transfer size of 0 must be marked as such. */
                        /* Finish PIO transfer. */
                        ataPIOTransfer(pCtl);
                        Assert(!pCtl->fRedo);
                        if (!s->fATAPITransfer)
                            ataSetIRQ(s);
                        pCtl->uAsyncIOState = ATA_AIO_NEW;
                    }
                }
                break;

            case ATA_AIO_DMA:
            {
                BMDMAState *bm = &pCtl->BmDma;
                s = &pCtl->aIfs[pCtl->iAIOIf]; /* Do not remove or there's an instant crash after loading the saved state */
                ATAFNSS iOriginalSourceSink = (ATAFNSS)s->iSourceSink; /* Used by the hack below, but gets reset by then. */

                if (s->uTxDir == PDMBLOCKTXDIR_FROM_DEVICE)
                    AssertRelease(bm->u8Cmd & BM_CMD_WRITE);
                else
                    AssertRelease(!(bm->u8Cmd & BM_CMD_WRITE));

                if (RT_LIKELY(!pCtl->fRedo))
                {
                    /* The specs say that the descriptor table must not cross a
                     * 4K boundary. */
                    pCtl->pFirstDMADesc = bm->pvAddr;
                    pCtl->pLastDMADesc = RT_ALIGN_32(bm->pvAddr + 1, _4K) - sizeof(BMDMADesc);
                }
                ataDMATransfer(pCtl);

                if (RT_UNLIKELY(pCtl->fRedo))
                {
                    LogRel(("PIIX3 ATA: Ctl#%d: redo DMA operation\n", ATACONTROLLER_IDX(pCtl)));
                    ataAsyncIOPutRequest(pCtl, &ataDMARequest);
                    ataSuspendRedo(pCtl);
                    break;
                }

                /* The infamous delay IRQ hack. */
                if (   iOriginalSourceSink == ATAFN_SS_WRITE_SECTORS
                    && s->cbTotalTransfer == 0
                    && pCtl->DelayIRQMillies)
                {
                    /* Delay IRQ for writing. Required to get the Win2K
                     * installation work reliably (otherwise it crashes,
                     * usually during component install). So far no better
                     * solution has been found. */
                    Log(("%s: delay IRQ hack\n", __FUNCTION__));
                    PDMCritSectLeave(&pCtl->lock);
                    RTThreadSleep(pCtl->DelayIRQMillies);
                    PDMCritSectEnter(&pCtl->lock, VINF_SUCCESS);
                }

                ataUnsetStatus(s, ATA_STAT_DRQ);
                Assert(!pCtl->fChainedTransfer);
                Assert(s->iSourceSink == ATAFN_SS_NULL);
                if (s->fATAPITransfer)
                {
                    s->uATARegNSector = (s->uATARegNSector & ~7) | ATAPI_INT_REASON_IO | ATAPI_INT_REASON_CD;
                    Log2(("%s: Ctl#%d: interrupt reason %#04x\n", __FUNCTION__, ATACONTROLLER_IDX(pCtl), s->uATARegNSector));
                    s->fATAPITransfer = false;
                }
                ataSetIRQ(s);
                pCtl->uAsyncIOState = ATA_AIO_NEW;
                break;
            }

            case ATA_AIO_PIO:
                s = &pCtl->aIfs[pCtl->iAIOIf]; /* Do not remove or there's an instant crash after loading the saved state */

                if (s->iSourceSink != ATAFN_SS_NULL)
                {
                    bool fRedo;
                    Log2(("%s: Ctl#%d: calling source/sink function\n", __FUNCTION__, ATACONTROLLER_IDX(pCtl)));
                    fRedo = g_apfnSourceSinkFuncs[s->iSourceSink](s);
                    pCtl->fRedo = fRedo;
                    if (RT_UNLIKELY(fRedo))
                    {
                        LogRel(("PIIX3 ATA: Ctl#%d: redo PIO operation\n", ATACONTROLLER_IDX(pCtl)));
                        ataAsyncIOPutRequest(pCtl, &ataPIORequest);
                        ataSuspendRedo(pCtl);
                        break;
                    }
                    s->iIOBufferCur = 0;
                    s->iIOBufferEnd = s->cbElementaryTransfer;
                }
                else
                {
                    /* Continue a previously started transfer. */
                    ataUnsetStatus(s, ATA_STAT_BUSY);
                    ataSetStatus(s, ATA_STAT_READY);
                }

                /* It is possible that the drives on this controller get RESET
                 * during the above call to the source/sink function. If that's
                 * the case, don't restart the transfer and don't finish it the
                 * usual way. RESET handling took care of all that already.
                 * Most important: do not change uAsyncIOState. */
                if (pCtl->fReset)
                    break;

                if (s->cbTotalTransfer)
                {
                    ataPIOTransfer(pCtl);
                    ataSetIRQ(s);

                    if (s->uTxDir == PDMBLOCKTXDIR_TO_DEVICE || s->iSourceSink != ATAFN_SS_NULL)
                    {
                        /* Write operations and not yet finished transfers
                         * must be completed in the async I/O thread. */
                        pCtl->uAsyncIOState = ATA_AIO_PIO;
                    }
                    else
                    {
                        /* Finished read operation can be handled inline
                         * in the end of PIO transfer handling code. Linux
                         * depends on this, as it waits only briefly for
                         * devices to become ready after incoming data
                         * transfer. Cannot find anything in the ATA spec
                         * that backs this assumption, but as all kernels
                         * are affected (though most of the time it does
                         * not cause any harm) this must work. */
                        pCtl->uAsyncIOState = ATA_AIO_NEW;
                    }
                }
                else
                {
                    /* Finish PIO transfer. */
                    ataPIOTransfer(pCtl);
                    if (    !pCtl->fChainedTransfer
                        &&  !s->fATAPITransfer
                        &&  s->uTxDir != PDMBLOCKTXDIR_FROM_DEVICE)
                    {
                            ataSetIRQ(s);
                    }
                    pCtl->uAsyncIOState = ATA_AIO_NEW;
                }
                break;

            case ATA_AIO_RESET_ASSERTED:
                pCtl->uAsyncIOState = ATA_AIO_RESET_CLEARED;
                ataPIOTransferStop(&pCtl->aIfs[0]);
                ataPIOTransferStop(&pCtl->aIfs[1]);
                /* Do not change the DMA registers, they are not affected by the
                 * ATA controller reset logic. It should be sufficient to issue a
                 * new command, which is now possible as the state is cleared. */
                break;

            case ATA_AIO_RESET_CLEARED:
                pCtl->uAsyncIOState = ATA_AIO_NEW;
                pCtl->fReset = false;
                LogRel(("PIIX3 ATA: Ctl#%d: finished processing RESET\n",
                        ATACONTROLLER_IDX(pCtl)));
                for (uint32_t i = 0; i < RT_ELEMENTS(pCtl->aIfs); i++)
                {
                    if (pCtl->aIfs[i].fATAPI)
                        ataSetStatusValue(&pCtl->aIfs[i], 0); /* NOTE: READY is _not_ set */
                    else
                        ataSetStatusValue(&pCtl->aIfs[i], ATA_STAT_READY | ATA_STAT_SEEK);
                    ataSetSignature(&pCtl->aIfs[i]);
                }
                break;

            case ATA_AIO_ABORT:
                /* Abort the current command only if it operates on the same interface. */
                if (pCtl->iAIOIf == pReq->u.a.iIf)
                {
                    s = &pCtl->aIfs[pCtl->iAIOIf];

                    pCtl->uAsyncIOState = ATA_AIO_NEW;
                    /* Do not change the DMA registers, they are not affected by the
                     * ATA controller reset logic. It should be sufficient to issue a
                     * new command, which is now possible as the state is cleared. */
                    if (pReq->u.a.fResetDrive)
                    {
                        ataResetDevice(s);
                        ataExecuteDeviceDiagnosticSS(s);
                    }
                    else
                    {
                        ataPIOTransferStop(s);
                        ataUnsetStatus(s, ATA_STAT_BUSY | ATA_STAT_DRQ | ATA_STAT_SEEK | ATA_STAT_ERR);
                        ataSetStatus(s, ATA_STAT_READY);
                        ataSetIRQ(s);
                    }
                }
                break;

            default:
                AssertMsgFailed(("Undefined async I/O state %d\n", pCtl->uAsyncIOState));
        }

        ataAsyncIORemoveCurrentRequest(pCtl, ReqType);
        pReq = ataAsyncIOGetCurrentRequest(pCtl);

        if (pCtl->uAsyncIOState == ATA_AIO_NEW && !pCtl->fChainedTransfer)
        {
#if defined(DEBUG) || defined(VBOX_WITH_STATISTICS)
            STAM_PROFILE_ADV_STOP(&pCtl->StatAsyncTime, a);
#endif /* DEBUG || VBOX_WITH_STATISTICS */

            u64TS = RTTimeNanoTS() - u64TS;
            uWait = u64TS / 1000;
            Log(("%s: Ctl#%d: LUN#%d finished I/O transaction in %d microseconds\n", __FUNCTION__, ATACONTROLLER_IDX(pCtl), pCtl->aIfs[pCtl->iAIOIf].iLUN, (uint32_t)(uWait)));
            /* Mark command as finished. */
            pCtl->aIfs[pCtl->iAIOIf].u64CmdTS = 0;

            /*
             * Release logging of command execution times depends on the
             * command type. ATAPI commands often take longer (due to CD/DVD
             * spin up time etc.) so the threshold is different.
             */
            if (pCtl->aIfs[pCtl->iAIOIf].uATARegCommand != ATA_PACKET)
            {
                if (uWait > 8 * 1000 * 1000)
                {
                    /*
                     * Command took longer than 8 seconds. This is close
                     * enough or over the guest's command timeout, so place
                     * an entry in the release log to allow tracking such
                     * timing errors (which are often caused by the host).
                     */
                    LogRel(("PIIX3 ATA: execution time for ATA command %#04x was %d seconds\n", pCtl->aIfs[pCtl->iAIOIf].uATARegCommand, uWait / (1000 * 1000)));
                }
            }
            else
            {
                if (uWait > 20 * 1000 * 1000)
                {
                    /*
                     * Command took longer than 20 seconds. This is close
                     * enough or over the guest's command timeout, so place
                     * an entry in the release log to allow tracking such
                     * timing errors (which are often caused by the host).
                     */
                    LogRel(("PIIX3 ATA: execution time for ATAPI command %#04x was %d seconds\n", pCtl->aIfs[pCtl->iAIOIf].aATAPICmd[0], uWait / (1000 * 1000)));
                }
            }

#if defined(DEBUG) || defined(VBOX_WITH_STATISTICS)
            if (uWait < pCtl->StatAsyncMinWait || !pCtl->StatAsyncMinWait)
                pCtl->StatAsyncMinWait = uWait;
            if (uWait > pCtl->StatAsyncMaxWait)
                pCtl->StatAsyncMaxWait = uWait;

            STAM_COUNTER_ADD(&pCtl->StatAsyncTimeUS, uWait);
            STAM_COUNTER_INC(&pCtl->StatAsyncOps);
#endif /* DEBUG || VBOX_WITH_STATISTICS */
        }

        LogBird(("ata: %x: leaving critsect\n", pCtl->IOPortBase1));
        PDMCritSectLeave(&pCtl->lock);
    }

    /* Cleanup the state.  */
    if (pCtl->AsyncIOSem)
    {
        RTSemEventDestroy(pCtl->AsyncIOSem);
        pCtl->AsyncIOSem = NIL_RTSEMEVENT;
    }
    if (pCtl->SuspendIOSem)
    {
        RTSemEventDestroy(pCtl->SuspendIOSem);
        pCtl->SuspendIOSem = NIL_RTSEMEVENT;
    }
    /* Do not destroy request mutex yet, still needed for proper shutdown. */
    pCtl->fShutdown = false;
    /* This must be last, as it also signals thread exit to EMT. */
    pCtl->AsyncIOThread = NIL_RTTHREAD;

    Log2(("%s: Ctl#%d: return %Vrc\n", __FUNCTION__, ATACONTROLLER_IDX(pCtl), rc));
    return rc;
}

#endif /* IN_RING3 */

static uint32_t ataBMDMACmdReadB(PATACONTROLLER pCtl, uint32_t addr)
{
    uint32_t val = pCtl->BmDma.u8Cmd;
    Log2(("%s: addr=%#06x val=%#04x\n", __FUNCTION__, addr, val));
    return val;
}


static void ataBMDMACmdWriteB(PATACONTROLLER pCtl, uint32_t addr, uint32_t val)
{
    Log2(("%s: addr=%#06x val=%#04x\n", __FUNCTION__, addr, val));
    if (!(val & BM_CMD_START))
    {
        pCtl->BmDma.u8Status &= ~BM_STATUS_DMAING;
        pCtl->BmDma.u8Cmd = val & (BM_CMD_START | BM_CMD_WRITE);
    }
    else
    {
#ifdef IN_RING3
        /* Check whether the guest OS wants to change DMA direction in
         * mid-flight. Not allowed, according to the PIIX3 specs. */
        Assert(!(pCtl->BmDma.u8Status & BM_STATUS_DMAING) || !((val ^ pCtl->BmDma.u8Cmd) & 0x04));
        pCtl->BmDma.u8Status |= BM_STATUS_DMAING;
        pCtl->BmDma.u8Cmd = val & (BM_CMD_START | BM_CMD_WRITE);

        /* Do not continue DMA transfers while the RESET line is asserted. */
        if (pCtl->fReset)
        {
            Log2(("%s: Ctl#%d: suppressed continuing DMA transfer as RESET is active\n", __FUNCTION__, ATACONTROLLER_IDX(pCtl)));
            return;
        }

        /* Do not start DMA transfers if there's a PIO transfer going on. */
        if (!pCtl->aIfs[pCtl->iSelectedIf].fDMA)
            return;

        if (pCtl->aIfs[pCtl->iAIOIf].uATARegStatus & ATA_STAT_DRQ)
        {
            Log2(("%s: Ctl#%d: message to async I/O thread, continuing DMA transfer\n", __FUNCTION__, ATACONTROLLER_IDX(pCtl)));
            ataAsyncIOPutRequest(pCtl, &ataDMARequest);
        }
#else /* !IN_RING3 */
        AssertMsgFailed(("DMA START handling is too complicated for GC\n"));
#endif /* IN_RING3 */
    }
}

static uint32_t ataBMDMAStatusReadB(PATACONTROLLER pCtl, uint32_t addr)
{
    uint32_t val = pCtl->BmDma.u8Status;
    Log2(("%s: addr=%#06x val=%#04x\n", __FUNCTION__, addr, val));
    return val;
}

static void ataBMDMAStatusWriteB(PATACONTROLLER pCtl, uint32_t addr, uint32_t val)
{
    Log2(("%s: addr=%#06x val=%#04x\n", __FUNCTION__, addr, val));
    pCtl->BmDma.u8Status =    (val & (BM_STATUS_D0DMA | BM_STATUS_D1DMA))
                           |  (pCtl->BmDma.u8Status & BM_STATUS_DMAING)
                           |  (pCtl->BmDma.u8Status & ~val & (BM_STATUS_ERROR | BM_STATUS_INT));
}

static uint32_t ataBMDMAAddrReadL(PATACONTROLLER pCtl, uint32_t addr)
{
    uint32_t val = (uint32_t)pCtl->BmDma.pvAddr;
    Log2(("%s: addr=%#06x val=%#010x\n", __FUNCTION__, addr, val));
    return val;
}

static void ataBMDMAAddrWriteL(PATACONTROLLER pCtl, uint32_t addr, uint32_t val)
{
    Log2(("%s: addr=%#06x val=%#010x\n", __FUNCTION__, addr, val));
    pCtl->BmDma.pvAddr = val & ~3;
}

static void ataBMDMAAddrWriteLowWord(PATACONTROLLER pCtl, uint32_t addr, uint32_t val)
{
    Log2(("%s: addr=%#06x val=%#010x\n", __FUNCTION__, addr, val));
    pCtl->BmDma.pvAddr = (pCtl->BmDma.pvAddr & 0xFFFF0000) | RT_LOWORD(val & ~3);

}

static void ataBMDMAAddrWriteHighWord(PATACONTROLLER pCtl, uint32_t addr, uint32_t val)
{
    Log2(("%s: addr=%#06x val=%#010x\n", __FUNCTION__, addr, val));
    pCtl->BmDma.pvAddr = (RT_LOWORD(val) << 16) | RT_LOWORD(pCtl->BmDma.pvAddr);
}

#define VAL(port, size)   ( ((port) & 7) | ((size) << 3) )

/**
 * Port I/O Handler for bus master DMA IN operations.
 * @see FNIOMIOPORTIN for details.
 */
PDMBOTHCBDECL(int) ataBMDMAIOPortRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    uint32_t       i = (uint32_t)(uintptr_t)pvUser;
    PCIATAState   *pData = PDMINS2DATA(pDevIns, PCIATAState *);
    PATACONTROLLER pCtl = &pData->aCts[i];
    int rc;

    rc = PDMCritSectEnter(&pCtl->lock, VINF_IOM_HC_IOPORT_READ);
    if (rc != VINF_SUCCESS)
        return rc;
    switch (VAL(Port, cb))
    {
        case VAL(0, 1): *pu32 = ataBMDMACmdReadB(pCtl, Port); break;
        case VAL(0, 2): *pu32 = ataBMDMACmdReadB(pCtl, Port); break;
        case VAL(2, 1): *pu32 = ataBMDMAStatusReadB(pCtl, Port); break;
        case VAL(2, 2): *pu32 = ataBMDMAStatusReadB(pCtl, Port); break;
        case VAL(4, 4): *pu32 = ataBMDMAAddrReadL(pCtl, Port); break;
        default:
            AssertMsgFailed(("%s: Unsupported read from port %x size=%d\n", __FUNCTION__, Port, cb));
            PDMCritSectLeave(&pCtl->lock);
            return VERR_IOM_IOPORT_UNUSED;
    }
    PDMCritSectLeave(&pCtl->lock);
    return rc;
}

/**
 * Port I/O Handler for bus master DMA OUT operations.
 * @see FNIOMIOPORTOUT for details.
 */
PDMBOTHCBDECL(int) ataBMDMAIOPortWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    uint32_t       i = (uint32_t)(uintptr_t)pvUser;
    PCIATAState   *pData = PDMINS2DATA(pDevIns, PCIATAState *);
    PATACONTROLLER pCtl = &pData->aCts[i];
    int rc;

    rc = PDMCritSectEnter(&pCtl->lock, VINF_IOM_HC_IOPORT_WRITE);
    if (rc != VINF_SUCCESS)
        return rc;
    switch (VAL(Port, cb))
    {
        case VAL(0, 1):
#ifndef IN_RING3
            if (u32 & BM_CMD_START)
            {
                rc = VINF_IOM_HC_IOPORT_WRITE;
                break;
            }
#endif /* !IN_RING3 */
            ataBMDMACmdWriteB(pCtl, Port, u32);
            break;
        case VAL(2, 1): ataBMDMAStatusWriteB(pCtl, Port, u32); break;
        case VAL(4, 4): ataBMDMAAddrWriteL(pCtl, Port, u32); break;
        case VAL(4, 2): ataBMDMAAddrWriteLowWord(pCtl, Port, u32); break;
        case VAL(6, 2): ataBMDMAAddrWriteHighWord(pCtl, Port, u32); break;
        default:        AssertMsgFailed(("%s: Unsupported write to port %x size=%d val=%x\n", __FUNCTION__, Port, cb, u32)); break;
    }
    PDMCritSectLeave(&pCtl->lock);
    return rc;
}

#undef VAL

#ifdef IN_RING3

/**
 * Callback function for mapping an PCI I/O region.
 *
 * @return VBox status code.
 * @param   pPciDev         Pointer to PCI device. Use pPciDev->pDevIns to get the device instance.
 * @param   iRegion         The region number.
 * @param   GCPhysAddress   Physical address of the region. If iType is PCI_ADDRESS_SPACE_IO, this is an
 *                          I/O port, else it's a physical address.
 *                          This address is *NOT* relative to pci_mem_base like earlier!
 * @param   enmType         One of the PCI_ADDRESS_SPACE_* values.
 */
static DECLCALLBACK(int) ataBMDMAIORangeMap(PPCIDEVICE pPciDev, /*unsigned*/ int iRegion, RTGCPHYS GCPhysAddress, uint32_t cb, PCIADDRESSSPACE enmType)
{
    PCIATAState *pData = PCIDEV_2_PCIATASTATE(pPciDev);
    int         rc = VINF_SUCCESS;
    Assert(enmType == PCI_ADDRESS_SPACE_IO);
    Assert(iRegion == 4);
    AssertMsg(RT_ALIGN(GCPhysAddress, 8) == GCPhysAddress, ("Expected 8 byte alignment. GCPhysAddress=%#x\n", GCPhysAddress));

    /* Register the port range. */
    for (uint32_t i = 0; i < RT_ELEMENTS(pData->aCts); i++)
    {
        int rc2 = PDMDevHlpIOPortRegister(pPciDev->pDevIns, (RTIOPORT)GCPhysAddress + i * 8, 8,
                                          (RTHCPTR)i, ataBMDMAIOPortWrite, ataBMDMAIOPortRead, NULL, NULL, "ATA Bus Master DMA");
        AssertRC(rc2);
        if (rc2 < rc)
            rc = rc2;

        if (pData->fGCEnabled)
        {
            rc2 = PDMDevHlpIOPortRegisterGC(pPciDev->pDevIns, (RTIOPORT)GCPhysAddress + i * 8, 8,
                                            (RTGCPTR)i, "ataBMDMAIOPortWrite", "ataBMDMAIOPortRead", NULL, NULL, "ATA Bus Master DMA");
            AssertRC(rc2);
            if (rc2 < rc)
                rc = rc2;
        }
        if (pData->fR0Enabled)
        {
            rc2 = PDMDevHlpIOPortRegisterR0(pPciDev->pDevIns, (RTIOPORT)GCPhysAddress + i * 8, 8,
                                            (RTR0PTR)i, "ataBMDMAIOPortWrite", "ataBMDMAIOPortRead", NULL, NULL, "ATA Bus Master DMA");
            AssertRC(rc2);
            if (rc2 < rc)
                rc = rc2;
        }
    }
    return rc;
}


/**
 * Reset notification.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 */
static DECLCALLBACK(void)  ataReset(PPDMDEVINS pDevIns)
{
    PCIATAState *pData = PDMINS2DATA(pDevIns, PCIATAState *);

    for (uint32_t i = 0; i < RT_ELEMENTS(pData->aCts); i++)
    {
        pData->aCts[i].iSelectedIf = 0;
        pData->aCts[i].iAIOIf = 0;
        pData->aCts[i].BmDma.u8Cmd = 0;
        /* Report that both drives present on the bus are in DMA mode. This
         * pretends that there is a BIOS that has set it up. Normal reset
         * default is 0x00. */
        pData->aCts[i].BmDma.u8Status =   (pData->aCts[i].aIfs[0].pDrvBase != NULL ? BM_STATUS_D0DMA : 0)
                                        | (pData->aCts[i].aIfs[1].pDrvBase != NULL ? BM_STATUS_D1DMA : 0);
        pData->aCts[i].BmDma.pvAddr = 0;

        pData->aCts[i].fReset = true;
        pData->aCts[i].fRedo = false;
        pData->aCts[i].fRedoIdle = false;
        ataAsyncIOClearRequests(&pData->aCts[i]);
        Log2(("%s: Ctl#%d: message to async I/O thread, reset controller\n", __FUNCTION__, i));
        ataAsyncIOPutRequest(&pData->aCts[i], &ataResetARequest);
        ataAsyncIOPutRequest(&pData->aCts[i], &ataResetCRequest);
        if (!ataWaitForAsyncIOIsIdle(&pData->aCts[i], 30000))
            AssertReleaseMsgFailed(("Async I/O thread busy after reset\n"));

        for (uint32_t j = 0; j < RT_ELEMENTS(pData->aCts[i].aIfs); j++)
            ataResetDevice(&pData->aCts[i].aIfs[j]);
    }
}


/* -=-=-=-=-=- PCIATAState::IBase  -=-=-=-=-=- */

/**
 * Queries an interface to the driver.
 *
 * @returns Pointer to interface.
 * @returns NULL if the interface was not supported by the device.
 * @param   pInterface          Pointer to ATADevState::IBase.
 * @param   enmInterface        The requested interface identification.
 */
static DECLCALLBACK(void *) ataStatus_QueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    PCIATAState *pData = PDMIBASE_2_PCIATASTATE(pInterface);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pData->IBase;
        case PDMINTERFACE_LED_PORTS:
            return &pData->ILeds;
        default:
            return NULL;
    }
}


/* -=-=-=-=-=- PCIATAState::ILeds  -=-=-=-=-=- */

/**
 * Gets the pointer to the status LED of a unit.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   iLUN            The unit which status LED we desire.
 * @param   ppLed           Where to store the LED pointer.
 */
static DECLCALLBACK(int) ataStatus_QueryStatusLed(PPDMILEDPORTS pInterface, unsigned iLUN, PPDMLED *ppLed)
{
    PCIATAState *pData = PDMILEDPORTS_2_PCIATASTATE(pInterface);
    if (iLUN >= 0 && iLUN <= 4)
    {
        switch (iLUN)
        {
            case 0: *ppLed = &pData->aCts[0].aIfs[0].Led; break;
            case 1: *ppLed = &pData->aCts[0].aIfs[1].Led; break;
            case 2: *ppLed = &pData->aCts[1].aIfs[0].Led; break;
            case 3: *ppLed = &pData->aCts[1].aIfs[1].Led; break;
        }
        Assert((*ppLed)->u32Magic == PDMLED_MAGIC);
        return VINF_SUCCESS;
    }
    return VERR_PDM_LUN_NOT_FOUND;
}


/* -=-=-=-=-=- ATADevState::IBase   -=-=-=-=-=- */

/**
 * Queries an interface to the driver.
 *
 * @returns Pointer to interface.
 * @returns NULL if the interface was not supported by the device.
 * @param   pInterface          Pointer to ATADevState::IBase.
 * @param   enmInterface        The requested interface identification.
 */
static DECLCALLBACK(void *)  ataQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    ATADevState *pIf = PDMIBASE_2_ATASTATE(pInterface);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pIf->IBase;
        case PDMINTERFACE_BLOCK_PORT:
            return &pIf->IPort;
        case PDMINTERFACE_MOUNT_NOTIFY:
            return &pIf->IMountNotify;
        default:
            return NULL;
    }
}

#endif /* IN_RING3 */


/* -=-=-=-=-=- Wrappers  -=-=-=-=-=- */

/**
 * Port I/O Handler for primary port range OUT operations.
 * @see FNIOMIOPORTOUT for details.
 */
PDMBOTHCBDECL(int) ataIOPortWrite1(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    uint32_t       i = (uint32_t)(uintptr_t)pvUser;
    PCIATAState   *pData = PDMINS2DATA(pDevIns, PCIATAState *);
    PATACONTROLLER pCtl = &pData->aCts[i];
    int            rc = VINF_SUCCESS;

    Assert(i < 2);

    rc = PDMCritSectEnter(&pCtl->lock, VINF_IOM_HC_IOPORT_WRITE);
    if (rc != VINF_SUCCESS)
        return rc;
    if (cb == 1)
        rc = ataIOPortWriteU8(pCtl, Port, u32);
    else if (Port == pCtl->IOPortBase1)
    {
        Assert(cb == 2 || cb == 4);
        rc = ataDataWrite(pCtl, Port, cb, (const uint8_t *)&u32);
    }
    else
        AssertMsgFailed(("ataIOPortWrite1: unsupported write to port %x val=%x size=%d\n", Port, u32, cb));
    LogBird(("ata: leaving critsect\n"));
    PDMCritSectLeave(&pCtl->lock);
    LogBird(("ata: left critsect\n"));
    return rc;
}


/**
 * Port I/O Handler for primary port range IN operations.
 * @see FNIOMIOPORTIN for details.
 */
PDMBOTHCBDECL(int) ataIOPortRead1(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    uint32_t       i = (uint32_t)(uintptr_t)pvUser;
    PCIATAState   *pData = PDMINS2DATA(pDevIns, PCIATAState *);
    PATACONTROLLER pCtl = &pData->aCts[i];
    int            rc = VINF_SUCCESS;

    Assert(i < 2);

    rc = PDMCritSectEnter(&pCtl->lock, VINF_IOM_HC_IOPORT_READ);
    if (rc != VINF_SUCCESS)
        return rc;
    if (cb == 1)
    {
        rc = ataIOPortReadU8(pCtl, Port, pu32);
    }
    else if (Port == pCtl->IOPortBase1)
    {
        Assert(cb == 2 || cb == 4);
        rc = ataDataRead(pCtl, Port, cb, (uint8_t *)pu32);
        if (cb == 2)
            *pu32 &= 0xffff;
    }
    else
    {
        AssertMsgFailed(("ataIOPortRead1: unsupported read from port %x size=%d\n", Port, cb));
        rc = VERR_IOM_IOPORT_UNUSED;
    }
    PDMCritSectLeave(&pCtl->lock);
    return rc;
}

#ifndef IN_RING0
/**
 * Port I/O Handler for primary port range IN string operations.
 * @see FNIOMIOPORTINSTRING for details.
 */
PDMBOTHCBDECL(int) ataIOPortReadStr1(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, RTGCPTR *pGCPtrDst, unsigned *pcTransfer, unsigned cb)
{
    uint32_t       i = (uint32_t)(uintptr_t)pvUser;
    PCIATAState   *pData = PDMINS2DATA(pDevIns, PCIATAState *);
    PATACONTROLLER pCtl = &pData->aCts[i];
    int            rc = VINF_SUCCESS;

    Assert(i < 2);

    rc = PDMCritSectEnter(&pCtl->lock, VINF_IOM_HC_IOPORT_READ);
    if (rc != VINF_SUCCESS)
        return rc;
    if (Port == pCtl->IOPortBase1)
    {
        uint32_t cTransAvailable, cTransfer = *pcTransfer, cbTransfer;
        RTGCPTR GCDst = *pGCPtrDst;
        ATADevState *s = &pCtl->aIfs[pCtl->iSelectedIf];
        Assert(cb == 2 || cb == 4);

        cTransAvailable = (s->iIOBufferPIODataEnd - s->iIOBufferPIODataStart) / cb;
#ifndef IN_RING3
        /* The last transfer unit cannot be handled in GC, as it involves thread communication. */
        cTransAvailable--;
#endif /* !IN_RING3 */
        /* Do not handle the dummy transfer stuff here, leave it to the single-word transfers.
         * They are not performance-critical and generally shouldn't occur at all. */
        if (cTransAvailable > cTransfer)
            cTransAvailable = cTransfer;
        cbTransfer = cTransAvailable * cb;

#ifdef IN_GC
        for (uint32_t i = 0; i < cbTransfer; i += cb)
            MMGCRamWriteNoTrapHandler((char *)GCDst + i, s->CTXSUFF(pbIOBuffer) + s->iIOBufferPIODataStart + i, cb);
#else /* !IN_GC */
        rc = PGMPhysWriteGCPtrDirty(PDMDevHlpGetVM(pDevIns), GCDst, s->CTXSUFF(pbIOBuffer) + s->iIOBufferPIODataStart, cbTransfer);
        Assert(rc == VINF_SUCCESS);
#endif /* IN_GC */

        if (cbTransfer)
            Log3(("%s: addr=%#x val=%.*Vhxs\n", __FUNCTION__, Port, cbTransfer, s->CTXSUFF(pbIOBuffer) + s->iIOBufferPIODataStart));
        s->iIOBufferPIODataStart += cbTransfer;
        *pGCPtrDst = (RTGCPTR)((RTGCUINTPTR)GCDst + cbTransfer);
        *pcTransfer = cTransfer - cTransAvailable;
#ifdef IN_RING3
        if (s->iIOBufferPIODataStart >= s->iIOBufferPIODataEnd)
            ataPIOTransferFinish(pCtl, s);
#endif /* IN_RING3 */
    }
    PDMCritSectLeave(&pCtl->lock);
    return rc;
}


/**
 * Port I/O Handler for primary port range OUT string operations.
 * @see FNIOMIOPORTOUTSTRING for details.
 */
PDMBOTHCBDECL(int) ataIOPortWriteStr1(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, RTGCPTR *pGCPtrSrc, unsigned *pcTransfer, unsigned cb)
{
    uint32_t       i = (uint32_t)(uintptr_t)pvUser;
    PCIATAState   *pData = PDMINS2DATA(pDevIns, PCIATAState *);
    PATACONTROLLER pCtl = &pData->aCts[i];
    int            rc;

    Assert(i < 2);

    rc = PDMCritSectEnter(&pCtl->lock, VINF_IOM_HC_IOPORT_WRITE);
    if (rc != VINF_SUCCESS)
        return rc;
    if (Port == pCtl->IOPortBase1)
    {
        uint32_t cTransAvailable, cTransfer = *pcTransfer, cbTransfer;
        RTGCPTR GCSrc = *pGCPtrSrc;
        ATADevState *s = &pCtl->aIfs[pCtl->iSelectedIf];
        Assert(cb == 2 || cb == 4);

        cTransAvailable = (s->iIOBufferPIODataEnd - s->iIOBufferPIODataStart) / cb;
#ifndef IN_RING3
        /* The last transfer unit cannot be handled in GC, as it involves thread communication. */
        cTransAvailable--;
#endif /* !IN_RING3 */
        /* Do not handle the dummy transfer stuff here, leave it to the single-word transfers.
         * They are not performance-critical and generally shouldn't occur at all. */
        if (cTransAvailable > cTransfer)
            cTransAvailable = cTransfer;
        cbTransfer = cTransAvailable * cb;

#ifdef IN_GC
        for (uint32_t i = 0; i < cbTransfer; i += cb)
            MMGCRamReadNoTrapHandler(s->CTXSUFF(pbIOBuffer) + s->iIOBufferPIODataStart + i, (char *)GCSrc + i, cb);
#else /* !IN_GC */
        rc = PGMPhysReadGCPtr(PDMDevHlpGetVM(pDevIns), s->CTXSUFF(pbIOBuffer) + s->iIOBufferPIODataStart, GCSrc, cbTransfer);
        Assert(rc == VINF_SUCCESS);
#endif /* IN_GC */

        if (cbTransfer)
            Log3(("%s: addr=%#x val=%.*Vhxs\n", __FUNCTION__, Port, cbTransfer, s->CTXSUFF(pbIOBuffer) + s->iIOBufferPIODataStart));
        s->iIOBufferPIODataStart += cbTransfer;
        *pGCPtrSrc = (RTGCPTR)((RTGCUINTPTR)GCSrc + cbTransfer);
        *pcTransfer = cTransfer - cTransAvailable;
#ifdef IN_RING3
        if (s->iIOBufferPIODataStart >= s->iIOBufferPIODataEnd)
            ataPIOTransferFinish(pCtl, s);
#endif /* IN_RING3 */
    }
    PDMCritSectLeave(&pCtl->lock);
    return rc;
}
#endif /* !IN_RING0 */

/**
 * Port I/O Handler for secondary port range OUT operations.
 * @see FNIOMIOPORTOUT for details.
 */
PDMBOTHCBDECL(int) ataIOPortWrite2(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    uint32_t       i = (uint32_t)(uintptr_t)pvUser;
    PCIATAState   *pData = PDMINS2DATA(pDevIns, PCIATAState *);
    PATACONTROLLER pCtl = &pData->aCts[i];
    int rc;

    Assert(i < 2);

    if (cb != 1)
        return VINF_SUCCESS;
    rc = PDMCritSectEnter(&pCtl->lock, VINF_IOM_HC_IOPORT_WRITE);
    if (rc != VINF_SUCCESS)
        return rc;
    rc = ataControlWrite(pCtl, Port, u32);
    PDMCritSectLeave(&pCtl->lock);
    return rc;
}


/**
 * Port I/O Handler for secondary port range IN operations.
 * @see FNIOMIOPORTIN for details.
 */
PDMBOTHCBDECL(int) ataIOPortRead2(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    uint32_t       i = (uint32_t)(uintptr_t)pvUser;
    PCIATAState   *pData = PDMINS2DATA(pDevIns, PCIATAState *);
    PATACONTROLLER pCtl = &pData->aCts[i];
    int            rc;

    Assert(i < 2);

    if (cb != 1)
        return VERR_IOM_IOPORT_UNUSED;

    rc = PDMCritSectEnter(&pCtl->lock, VINF_IOM_HC_IOPORT_READ);
    if (rc != VINF_SUCCESS)
        return rc;
    *pu32 = ataStatusRead(pCtl, Port);
    PDMCritSectLeave(&pCtl->lock);
    return VINF_SUCCESS;
}

#ifdef IN_RING3

/**
 * Waits for all async I/O threads to complete whatever they
 * are doing at the moment.
 *
 * @returns true on success.
 * @returns false when one or more threads is still processing.
 * @param   pData       Pointer to the instance data.
 * @param   cMillies    How long to wait (total).
 */
static bool ataWaitForAllAsyncIOIsIdle(PPDMDEVINS pDevIns, unsigned cMillies)
{
    PCIATAState    *pData = PDMINS2DATA(pDevIns, PCIATAState *);
    bool            fVMLocked;
    uint64_t        u64Start;
    PATACONTROLLER  pCtl;
    bool            fAllIdle = false;

    /* The only way to deal cleanly with the VM lock is to check whether
     * it is owned now (it always is owned by EMT, which is the current
     * thread). Since this function is called several times during VM
     * shutdown, and the VM lock is only held for the first call (which
     * can be either from ataPowerOff or ataSuspend), there is no other
     * reasonable solution. */
    fVMLocked = VMMR3LockIsOwner(PDMDevHlpGetVM(pDevIns));

    if (fVMLocked)
        pDevIns->pDevHlp->pfnUnlockVM(pDevIns);
    /*
     * Wait for any pending async operation to finish
     */
    u64Start = RTTimeMilliTS();
    for (;;)
    {
        /* Check all async I/O threads. */
        fAllIdle = true;
        for (uint32_t i = 0; i < RT_ELEMENTS(pData->aCts); i++)
        {
            pCtl = &pData->aCts[i];
            fAllIdle &= ataAsyncIOIsIdle(pCtl, false);
            if (!fAllIdle)
                break;
        }
        if (    fAllIdle
            ||  RTTimeMilliTS() - u64Start >= cMillies)
            break;

        /* Sleep for a bit. */
        RTThreadSleep(100);
    }

    if (fVMLocked)
        pDevIns->pDevHlp->pfnLockVM(pDevIns);

    if (!fAllIdle)
        LogRel(("PIIX3 ATA: Ctl#%d is still executing, DevSel=%d AIOIf=%d CmdIf0=%#04x CmdIf1=%#04x\n",
                ATACONTROLLER_IDX(pCtl), pCtl->iSelectedIf, pCtl->iAIOIf,
                pCtl->aIfs[0].uATARegCommand, pCtl->aIfs[1].uATARegCommand));

    return fAllIdle;
}


DECLINLINE(void) ataRelocBuffer(PPDMDEVINS pDevIns, ATADevState *s)
{
    if (s->pbIOBufferHC)
        s->pbIOBufferGC = MMHyperHC2GC(PDMDevHlpGetVM(pDevIns), s->pbIOBufferHC);
}


/**
 * @copydoc FNPDMDEVRELOCATE
 */
static DECLCALLBACK(void) ataRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    PCIATAState *pData = PDMINS2DATA(pDevIns, PCIATAState *);

    for (uint32_t i = 0; i < RT_ELEMENTS(pData->aCts); i++)
    {
        pData->aCts[i].pDevInsGC += offDelta;
        pData->aCts[i].aIfs[0].pDevInsGC += offDelta;
        pData->aCts[i].aIfs[0].pControllerGC += offDelta;
        ataRelocBuffer(pDevIns, &pData->aCts[i].aIfs[0]);
        pData->aCts[i].aIfs[1].pDevInsGC += offDelta;
        pData->aCts[i].aIfs[1].pControllerGC += offDelta;
        ataRelocBuffer(pDevIns, &pData->aCts[i].aIfs[1]);
    }
}


/**
 * Destroy a driver instance.
 *
 * Most VM resources are freed by the VM. This callback is provided so that any non-VM
 * resources can be freed correctly.
 *
 * @param   pDevIns     The device instance data.
 */
static DECLCALLBACK(int) ataDestruct(PPDMDEVINS pDevIns)
{
    PCIATAState    *pData = PDMINS2DATA(pDevIns, PCIATAState *);
    int             rc;

    Log(("%s:\n", __FUNCTION__));

    /*
     * Terminate all async helper threads
     */
    for (uint32_t i = 0; i < RT_ELEMENTS(pData->aCts); i++)
    {
        if (pData->aCts[i].AsyncIOThread != NIL_RTTHREAD)
        {
            ASMAtomicXchgU32(&pData->aCts[i].fShutdown, true);
            rc = RTSemEventSignal(pData->aCts[i].AsyncIOSem);
            AssertRC(rc);
        }
    }

    /*
     * Wait for them to complete whatever they are doing and then
     * for them to terminate.
     */
    if (ataWaitForAllAsyncIOIsIdle(pDevIns, 20000))
    {
        uint64_t    u64Start = RTTimeMilliTS();
        bool        fAllDone;
        for (;;)
        {
            /* check */
            fAllDone = true;
            for (uint32_t i = 0; i < RT_ELEMENTS(pData->aCts) && fAllDone; i++)
                fAllDone &= (pData->aCts[i].AsyncIOThread == NIL_RTTHREAD);

            if (    fAllDone
                ||  RTTimeMilliTS() - u64Start >= 500)
                break;

            /* Sleep for a bit. */
            RTThreadSleep(100);
        }
        AssertMsg(fAllDone, ("Some of the async I/O threads are still running!\n"));
    }
    else
        AssertMsgFailed(("Async I/O is still busy!\n"));

    /*
     * Now the request mutexes are no longer needed. Free resources.
     */
    for (uint32_t i = 0; i < RT_ELEMENTS(pData->aCts); i++)
    {
        if (pData->aCts[i].AsyncIORequestMutex)
        {
            RTSemMutexDestroy(pData->aCts[i].AsyncIORequestMutex);
            pData->aCts[i].AsyncIORequestMutex = NIL_RTSEMEVENT;
        }
    }
    return VINF_SUCCESS;
}


/**
 * Detach notification.
 *
 * The DVD drive has been unplugged.
 *
 * @param   pDevIns     The device instance.
 * @param   iLUN        The logical unit which is being detached.
 */
static DECLCALLBACK(void) ataDetach(PPDMDEVINS pDevIns, unsigned iLUN)
{
    PCIATAState    *pThis = PDMINS2DATA(pDevIns, PCIATAState *);
    PATACONTROLLER  pCtl;
    ATADevState    *pIf;
    unsigned        iController;
    unsigned        iInterface;

    /*
     * Locate the controller and stuff.
     */
    iController = iLUN / RT_ELEMENTS(pThis->aCts[0].aIfs);
    AssertReleaseMsg(iController < RT_ELEMENTS(pThis->aCts), ("iController=%d iLUN=%d\n", iController, iLUN));
    pCtl = &pThis->aCts[iController];

    iInterface  = iLUN % RT_ELEMENTS(pThis->aCts[0].aIfs);
    pIf = &pCtl->aIfs[iInterface];

    /*
     * Zero some important members.
     */
    pIf->pDrvBase = NULL;
    pIf->pDrvBlock = NULL;
    pIf->pDrvBlockBios = NULL;
    pIf->pDrvMount = NULL;
}


/**
 * Configure a LUN.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pIf         The ATA unit state.
 */
static int ataConfigLun(PPDMDEVINS pDevIns, ATADevState *pIf)
{
    int             rc;
    PDMBLOCKTYPE    enmType;

    /*
     * Query Block, Bios and Mount interfaces.
     */
    pIf->pDrvBlock = (PDMIBLOCK *)pIf->pDrvBase->pfnQueryInterface(pIf->pDrvBase, PDMINTERFACE_BLOCK);
    if (!pIf->pDrvBlock)
    {
        AssertMsgFailed(("Configuration error: LUN#%d hasn't a block interface!\n", pIf->iLUN));
        return VERR_PDM_MISSING_INTERFACE;
    }

    /** @todo implement the BIOS invisible code path. */
    pIf->pDrvBlockBios = (PDMIBLOCKBIOS *)pIf->pDrvBase->pfnQueryInterface(pIf->pDrvBase, PDMINTERFACE_BLOCK_BIOS);
    if (!pIf->pDrvBlockBios)
    {
        AssertMsgFailed(("Configuration error: LUN#%d hasn't a block BIOS interface!\n", pIf->iLUN));
        return VERR_PDM_MISSING_INTERFACE;
    }
    pIf->pDrvMount = (PDMIMOUNT *)pIf->pDrvBase->pfnQueryInterface(pIf->pDrvBase, PDMINTERFACE_MOUNT);

    /*
     * Validate type.
     */
    enmType = pIf->pDrvBlock->pfnGetType(pIf->pDrvBlock);
    if (    enmType != PDMBLOCKTYPE_CDROM
        &&  enmType != PDMBLOCKTYPE_DVD
        &&  enmType != PDMBLOCKTYPE_HARD_DISK)
    {
        AssertMsgFailed(("Configuration error: LUN#%d isn't a disk or cd/dvd-rom. enmType=%d\n", pIf->iLUN, enmType));
        return VERR_PDM_UNSUPPORTED_BLOCK_TYPE;
    }
    if (    (   enmType == PDMBLOCKTYPE_DVD
             || enmType == PDMBLOCKTYPE_CDROM)
        &&  !pIf->pDrvMount)
    {
        AssertMsgFailed(("Internal error: cdrom without a mountable interface, WTF???!\n"));
        return VERR_INTERNAL_ERROR;
    }
    pIf->fATAPI = enmType == PDMBLOCKTYPE_DVD || enmType == PDMBLOCKTYPE_CDROM;
    pIf->fATAPIPassthrough = pIf->fATAPI ? (pIf->pDrvBlock->pfnSendCmd != NULL) : false;

    /*
     * Allocate I/O buffer.
     */
    PVM pVM = PDMDevHlpGetVM(pDevIns);
    if (pIf->cbIOBuffer)
    {
        /* Buffer is (probably) already allocated. Validate the fields,
         * because memory corruption can also overwrite pIf->cbIOBuffer. */
        if (pIf->fATAPI)
            AssertRelease(pIf->cbIOBuffer == _128K);
        else
            AssertRelease(pIf->cbIOBuffer == ATA_MAX_MULT_SECTORS * 512);
        Assert(pIf->pbIOBufferHC);
        Assert(pIf->pbIOBufferGC == MMHyperHC2GC(pVM, pIf->pbIOBufferHC));
    }
    else
    {
        if (pIf->fATAPI)
            pIf->cbIOBuffer = _128K;
        else
            pIf->cbIOBuffer = ATA_MAX_MULT_SECTORS * 512;
        Assert(!pIf->pbIOBufferHC);
        rc = MMHyperAlloc(pVM, pIf->cbIOBuffer, 1, MM_TAG_PDM_DEVICE_USER, (void **)&pIf->pbIOBufferHC);
        if (VBOX_FAILURE(rc))
            return VERR_NO_MEMORY;
        pIf->pbIOBufferGC = MMHyperHC2GC(pVM, pIf->pbIOBufferHC);
    }

    /*
     * Init geometry.
     */
    if (pIf->fATAPI)
    {
        pIf->cTotalSectors = pIf->pDrvBlock->pfnGetSize(pIf->pDrvBlock) / 2048;
        rc = pIf->pDrvBlockBios->pfnGetGeometry(pIf->pDrvBlockBios, &pIf->cCHSCylinders, &pIf->cCHSHeads, &pIf->cCHSSectors);
        pIf->cCHSCylinders = 0; /* dummy */
        pIf->cCHSHeads     = 0; /* dummy */
        pIf->cCHSSectors   = 0; /* dummy */
        if (rc != VERR_PDM_MEDIA_NOT_MOUNTED)
        {
            pIf->pDrvBlockBios->pfnSetTranslation(pIf->pDrvBlockBios, PDMBIOSTRANSLATION_NONE);
            pIf->pDrvBlockBios->pfnSetGeometry(pIf->pDrvBlockBios, pIf->cCHSCylinders, pIf->cCHSHeads, pIf->cCHSSectors);
        }
        LogRel(("PIIX3 ATA: LUN#%d: CD/DVD, total number of sectors %Ld, passthrough %s\n", pIf->iLUN, pIf->cTotalSectors, (pIf->fATAPIPassthrough ? "enabled" : "disabled")));
    }
    else
    {
        pIf->cTotalSectors = pIf->pDrvBlock->pfnGetSize(pIf->pDrvBlock) / 512;
        rc = pIf->pDrvBlockBios->pfnGetGeometry(pIf->pDrvBlockBios, &pIf->cCHSCylinders, &pIf->cCHSHeads, &pIf->cCHSSectors);
        if (rc == VERR_PDM_MEDIA_NOT_MOUNTED)
        {
            pIf->cCHSCylinders = 0;
            pIf->cCHSHeads     = 16; /*??*/
            pIf->cCHSSectors   = 63; /*??*/
        }
        else if (VBOX_FAILURE(rc))
        {
            PDMBIOSTRANSLATION enmTranslation;
            rc = pIf->pDrvBlockBios->pfnGetTranslation(pIf->pDrvBlockBios, &enmTranslation);
            if (rc == VERR_PDM_TRANSLATION_NOT_SET)
            {
                enmTranslation = PDMBIOSTRANSLATION_AUTO;
                pIf->cCHSCylinders = 0;
                rc = VINF_SUCCESS;
            }
            AssertRC(rc);

            if (    enmTranslation == PDMBIOSTRANSLATION_AUTO
                &&  (   pIf->cCHSCylinders == 0
                     || pIf->cCHSHeads == 0
                     || pIf->cCHSSectors == 0
                    )
               )
            {
                /* Image contains no geometry information, detect geometry. */
                rc = ataGuessDiskLCHS(pIf, &pIf->cCHSCylinders, &pIf->cCHSHeads, &pIf->cCHSSectors);
                if (VBOX_SUCCESS(rc))
                {
                    /* Caution: the above function returns LCHS, but the
                     * disk must report proper PCHS values for disks bigger
                     * than approximately 512MB. */
                    if (pIf->cCHSSectors == 63 && (pIf->cCHSHeads != 16 || pIf->cCHSCylinders >= 1024))
                    {
                        pIf->cCHSCylinders = pIf->cTotalSectors / 63 / 16;
                        pIf->cCHSHeads = 16;
                        pIf->cCHSSectors = 63;
                        /* Set the disk CHS translation mode. */
                         pIf->pDrvBlockBios->pfnSetTranslation(pIf->pDrvBlockBios, PDMBIOSTRANSLATION_LBA);
                    }
                    /* Set the disk geometry information. */
                    rc = pIf->pDrvBlockBios->pfnSetGeometry(pIf->pDrvBlockBios, pIf->cCHSCylinders, pIf->cCHSHeads, pIf->cCHSSectors);
                }
                else
                {
                    /* Flag geometry as invalid, will be replaced below by the
                     * default geometry. */
                    pIf->cCHSCylinders = 0;
                }
            }
            if (!pIf->cCHSCylinders)
            {
                /* If there is no geometry, use standard physical disk geometry.
                 * This uses LCHS to LBA translation in the BIOS (which selects
                 * the logical sector count 63 and the logical head count to be
                 * the smallest of 16,32,64,128,255 which makes the logical
                 * cylinder count smaller than 1024 - if that's not possible, it
                 * uses 255 heads, so up to about 8 GByte maximum with the
                 * standard int13 interface, which supports 1024 cylinders). */
                uint64_t cCHSCylinders = pIf->cTotalSectors / (16 * 63);
                pIf->cCHSCylinders = (uint32_t)RT_MAX(cCHSCylinders, 1);
                pIf->cCHSHeads = 16;
                pIf->cCHSSectors = 63;
                /* Set the disk geometry information. */
                rc = pIf->pDrvBlockBios->pfnSetGeometry(pIf->pDrvBlockBios, pIf->cCHSCylinders, pIf->cCHSHeads, pIf->cCHSSectors);
            }
        }
        else
        {
            PDMBIOSTRANSLATION enmTranslation;
            rc = pIf->pDrvBlockBios->pfnGetTranslation(pIf->pDrvBlockBios, &enmTranslation);
            if ((   rc == VERR_PDM_TRANSLATION_NOT_SET
                 || enmTranslation == PDMBIOSTRANSLATION_LBA)
                &&  pIf->cCHSSectors == 63
                && (pIf->cCHSHeads != 16 || pIf->cCHSCylinders >= 1024))
            {
                /* Use the official LBA physical CHS geometry. */
                uint64_t cCHSCylinders = pIf->cTotalSectors / (16 * 63);
                pIf->cCHSCylinders = RT_MAX((uint32_t)RT_MIN(cCHSCylinders, 16383), 1);
                pIf->cCHSHeads = 16;
                pIf->cCHSSectors = 63;
                /* DO NOT write back the disk geometry information. This
                 * intentionally sets the ATA geometry only. */
            }
        }
        LogRel(("PIIX3 ATA: LUN#%d: disk, CHS=%d/%d/%d, total number of sectors %Ld\n", pIf->iLUN, pIf->cCHSCylinders, pIf->cCHSHeads, pIf->cCHSSectors, pIf->cTotalSectors));
    }
    return VINF_SUCCESS;
}


/**
 * Attach command.
 *
 * This is called when we change block driver for the DVD drive.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   iLUN        The logical unit which is being detached.
 */
static DECLCALLBACK(int)  ataAttach(PPDMDEVINS pDevIns, unsigned iLUN)
{
    PCIATAState    *pThis = PDMINS2DATA(pDevIns, PCIATAState *);
    PATACONTROLLER  pCtl;
    ATADevState    *pIf;
    int             rc;
    unsigned        iController;
    unsigned        iInterface;

    /*
     * Locate the controller and stuff.
     */
    iController = iLUN / RT_ELEMENTS(pThis->aCts[0].aIfs);
    AssertReleaseMsg(iController < RT_ELEMENTS(pThis->aCts), ("iController=%d iLUN=%d\n", iController, iLUN));
    pCtl = &pThis->aCts[iController];

    iInterface  = iLUN % RT_ELEMENTS(pThis->aCts[0].aIfs);
    pIf = &pCtl->aIfs[iInterface];

    /* the usual paranoia */
    AssertRelease(!pIf->pDrvBase);
    AssertRelease(!pIf->pDrvBlock);
    Assert(ATADEVSTATE_2_CONTROLLER(pIf) == pCtl);
    Assert(pIf->iLUN == iLUN);

    /*
     * Try attach the block device and get the interfaces,
     * required as well as optional.
     */
    rc = PDMDevHlpDriverAttach(pDevIns, pIf->iLUN, &pIf->IBase, &pIf->pDrvBase, NULL);
    if (VBOX_SUCCESS(rc))
        rc = ataConfigLun(pDevIns, pIf);
    else
        AssertMsgFailed(("Failed to attach LUN#%d. rc=%Vrc\n", pIf->iLUN, rc));

    if (VBOX_FAILURE(rc))
    {
        pIf->pDrvBase = NULL;
        pIf->pDrvBlock = NULL;
    }
    return rc;
}


/**
 * Suspend notification.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 */
static DECLCALLBACK(void) ataSuspend(PPDMDEVINS pDevIns)
{
    Log(("%s:\n", __FUNCTION__));
    if (!ataWaitForAllAsyncIOIsIdle(pDevIns, 20000))
        AssertMsgFailed(("Async I/O didn't stop in 20 seconds!\n"));
    return;
}


/**
 * Resume notification.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 */
static DECLCALLBACK(void) ataResume(PPDMDEVINS pDevIns)
{
    PCIATAState    *pData = PDMINS2DATA(pDevIns, PCIATAState *);
    int             rc;

    Log(("%s:\n", __FUNCTION__));
    for (uint32_t i = 0; i < RT_ELEMENTS(pData->aCts); i++)
    {
        if (pData->aCts[i].fRedo && pData->aCts[i].fRedoIdle)
        {
            rc = RTSemEventSignal(pData->aCts[i].SuspendIOSem);
            AssertRC(rc);
        }
    }
    return;
}


/**
 * Power Off notification.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 */
static DECLCALLBACK(void) ataPowerOff(PPDMDEVINS pDevIns)
{
    Log(("%s:\n", __FUNCTION__));
    if (!ataWaitForAllAsyncIOIsIdle(pDevIns, 20000))
        AssertMsgFailed(("Async I/O didn't stop in 20 seconds!\n"));
    return;
}


/**
 * Prepare state save and load operation.
 *
 * @returns VBox status code.
 * @param   pDevIns         Device instance of the device which registered the data unit.
 * @param   pSSM            SSM operation handle.
 */
static DECLCALLBACK(int) ataSaveLoadPrep(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PCIATAState    *pData = PDMINS2DATA(pDevIns, PCIATAState *);

    /* sanity - the suspend notification will wait on the async stuff. */
    for (uint32_t i = 0; i < RT_ELEMENTS(pData->aCts); i++)
    {
        Assert(ataAsyncIOIsIdle(&pData->aCts[i], false));
        if (!ataAsyncIOIsIdle(&pData->aCts[i], false))
            return VERR_SSM_IDE_ASYNC_TIMEOUT;
    }
    return VINF_SUCCESS;
}


/**
 * Saves a state of the ATA device.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSMHandle  The handle to save the state to.
 */
static DECLCALLBACK(int) ataSaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSMHandle)
{
    PCIATAState    *pData = PDMINS2DATA(pDevIns, PCIATAState *);

    for (uint32_t i = 0; i < RT_ELEMENTS(pData->aCts); i++)
    {
        SSMR3PutU8(pSSMHandle, pData->aCts[i].iSelectedIf);
        SSMR3PutU8(pSSMHandle, pData->aCts[i].iAIOIf);
        SSMR3PutU8(pSSMHandle, pData->aCts[i].uAsyncIOState);
        SSMR3PutBool(pSSMHandle, pData->aCts[i].fChainedTransfer);
        SSMR3PutBool(pSSMHandle, pData->aCts[i].fReset);
        SSMR3PutBool(pSSMHandle, pData->aCts[i].fRedo);
        SSMR3PutBool(pSSMHandle, pData->aCts[i].fRedoIdle);
        SSMR3PutBool(pSSMHandle, pData->aCts[i].fRedoDMALastDesc);
        SSMR3PutMem(pSSMHandle, &pData->aCts[i].BmDma, sizeof(pData->aCts[i].BmDma));
        SSMR3PutGCPhys(pSSMHandle, pData->aCts[i].pFirstDMADesc);
        SSMR3PutGCPhys(pSSMHandle, pData->aCts[i].pLastDMADesc);
        SSMR3PutGCPhys(pSSMHandle, pData->aCts[i].pRedoDMABuffer);
        SSMR3PutU32(pSSMHandle, pData->aCts[i].cbRedoDMABuffer);

        for (uint32_t j = 0; j < RT_ELEMENTS(pData->aCts[i].aIfs); j++)
        {
            SSMR3PutBool(pSSMHandle, pData->aCts[i].aIfs[j].fLBA48);
            SSMR3PutBool(pSSMHandle, pData->aCts[i].aIfs[j].fATAPI);
            SSMR3PutBool(pSSMHandle, pData->aCts[i].aIfs[j].fIrqPending);
            SSMR3PutU8(pSSMHandle, pData->aCts[i].aIfs[j].cMultSectors);
            SSMR3PutU32(pSSMHandle, pData->aCts[i].aIfs[j].cCHSCylinders);
            SSMR3PutU32(pSSMHandle, pData->aCts[i].aIfs[j].cCHSHeads);
            SSMR3PutU32(pSSMHandle, pData->aCts[i].aIfs[j].cCHSSectors);
            SSMR3PutU32(pSSMHandle, pData->aCts[i].aIfs[j].cSectorsPerIRQ);
            SSMR3PutU64(pSSMHandle, pData->aCts[i].aIfs[j].cTotalSectors);
            SSMR3PutU8(pSSMHandle, pData->aCts[i].aIfs[j].uATARegFeature);
            SSMR3PutU8(pSSMHandle, pData->aCts[i].aIfs[j].uATARegFeatureHOB);
            SSMR3PutU8(pSSMHandle, pData->aCts[i].aIfs[j].uATARegError);
            SSMR3PutU8(pSSMHandle, pData->aCts[i].aIfs[j].uATARegNSector);
            SSMR3PutU8(pSSMHandle, pData->aCts[i].aIfs[j].uATARegNSectorHOB);
            SSMR3PutU8(pSSMHandle, pData->aCts[i].aIfs[j].uATARegSector);
            SSMR3PutU8(pSSMHandle, pData->aCts[i].aIfs[j].uATARegSectorHOB);
            SSMR3PutU8(pSSMHandle, pData->aCts[i].aIfs[j].uATARegLCyl);
            SSMR3PutU8(pSSMHandle, pData->aCts[i].aIfs[j].uATARegLCylHOB);
            SSMR3PutU8(pSSMHandle, pData->aCts[i].aIfs[j].uATARegHCyl);
            SSMR3PutU8(pSSMHandle, pData->aCts[i].aIfs[j].uATARegHCylHOB);
            SSMR3PutU8(pSSMHandle, pData->aCts[i].aIfs[j].uATARegSelect);
            SSMR3PutU8(pSSMHandle, pData->aCts[i].aIfs[j].uATARegStatus);
            SSMR3PutU8(pSSMHandle, pData->aCts[i].aIfs[j].uATARegCommand);
            SSMR3PutU8(pSSMHandle, pData->aCts[i].aIfs[j].uATARegDevCtl);
            SSMR3PutU8(pSSMHandle, pData->aCts[i].aIfs[j].uATATransferMode);
            SSMR3PutU8(pSSMHandle, pData->aCts[i].aIfs[j].uTxDir);
            SSMR3PutU8(pSSMHandle, pData->aCts[i].aIfs[j].iBeginTransfer);
            SSMR3PutU8(pSSMHandle, pData->aCts[i].aIfs[j].iSourceSink);
            SSMR3PutBool(pSSMHandle, pData->aCts[i].aIfs[j].fDMA);
            SSMR3PutBool(pSSMHandle, pData->aCts[i].aIfs[j].fATAPITransfer);
            SSMR3PutU32(pSSMHandle, pData->aCts[i].aIfs[j].cbTotalTransfer);
            SSMR3PutU32(pSSMHandle, pData->aCts[i].aIfs[j].cbElementaryTransfer);
            SSMR3PutU32(pSSMHandle, pData->aCts[i].aIfs[j].iIOBufferCur);
            SSMR3PutU32(pSSMHandle, pData->aCts[i].aIfs[j].iIOBufferEnd);
            SSMR3PutU32(pSSMHandle, pData->aCts[i].aIfs[j].iIOBufferPIODataStart);
            SSMR3PutU32(pSSMHandle, pData->aCts[i].aIfs[j].iIOBufferPIODataEnd);
            SSMR3PutU32(pSSMHandle, pData->aCts[i].aIfs[j].iATAPILBA);
            SSMR3PutU32(pSSMHandle, pData->aCts[i].aIfs[j].cbATAPISector);
            SSMR3PutMem(pSSMHandle, &pData->aCts[i].aIfs[j].aATAPICmd, sizeof(pData->aCts[i].aIfs[j].aATAPICmd));
            SSMR3PutU8(pSSMHandle, pData->aCts[i].aIfs[j].uATAPISenseKey);
            SSMR3PutU8(pSSMHandle, pData->aCts[i].aIfs[j].uATAPIASC);
            SSMR3PutU8(pSSMHandle, pData->aCts[i].aIfs[j].cNotifiedMediaChange);
            SSMR3PutMem(pSSMHandle, &pData->aCts[i].aIfs[j].Led, sizeof(pData->aCts[i].aIfs[j].Led));
            SSMR3PutU32(pSSMHandle, pData->aCts[i].aIfs[j].cbIOBuffer);
            if (pData->aCts[i].aIfs[j].cbIOBuffer)
                SSMR3PutMem(pSSMHandle, pData->aCts[i].aIfs[j].CTXSUFF(pbIOBuffer), pData->aCts[i].aIfs[j].cbIOBuffer);
            else
                Assert(pData->aCts[i].aIfs[j].CTXSUFF(pbIOBuffer) == NULL);
        }
    }
    SSMR3PutBool(pSSMHandle, pData->fPIIX4);

    return SSMR3PutU32(pSSMHandle, ~0); /* sanity/terminator */
}


/**
 * Loads a saved ATA device state.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSMHandle  The handle to the saved state.
 * @param   u32Version  The data unit version number.
 */
static DECLCALLBACK(int) ataLoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSMHandle, uint32_t u32Version)
{
    PCIATAState    *pData = PDMINS2DATA(pDevIns, PCIATAState *);
    int             rc;
    uint32_t        u32;

    if (u32Version != ATA_SAVED_STATE_VERSION)
    {
        AssertMsgFailed(("u32Version=%d\n", u32Version));
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }

    /*
     * Restore valid parts of the PCIATAState structure
     */
    for (uint32_t i = 0; i < RT_ELEMENTS(pData->aCts); i++)
    {
        /* integrity check */
        if (!ataAsyncIOIsIdle(&pData->aCts[i], false))
        {
            AssertMsgFailed(("Async I/O for controller %d is active\n", i));
            rc = VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
            return rc;
        }

        SSMR3GetU8(pSSMHandle, &pData->aCts[i].iSelectedIf);
        SSMR3GetU8(pSSMHandle, &pData->aCts[i].iAIOIf);
        SSMR3GetU8(pSSMHandle, &pData->aCts[i].uAsyncIOState);
        SSMR3GetBool(pSSMHandle, &pData->aCts[i].fChainedTransfer);
        SSMR3GetBool(pSSMHandle, (bool *)&pData->aCts[i].fReset);
        SSMR3GetBool(pSSMHandle, (bool *)&pData->aCts[i].fRedo);
        SSMR3GetBool(pSSMHandle, (bool *)&pData->aCts[i].fRedoIdle);
        SSMR3GetBool(pSSMHandle, (bool *)&pData->aCts[i].fRedoDMALastDesc);
        SSMR3GetMem(pSSMHandle, &pData->aCts[i].BmDma, sizeof(pData->aCts[i].BmDma));
        SSMR3GetGCPhys(pSSMHandle, &pData->aCts[i].pFirstDMADesc);
        SSMR3GetGCPhys(pSSMHandle, &pData->aCts[i].pLastDMADesc);
        SSMR3GetGCPhys(pSSMHandle, &pData->aCts[i].pRedoDMABuffer);
        SSMR3GetU32(pSSMHandle, &pData->aCts[i].cbRedoDMABuffer);

        for (uint32_t j = 0; j < RT_ELEMENTS(pData->aCts[i].aIfs); j++)
        {
            SSMR3GetBool(pSSMHandle, &pData->aCts[i].aIfs[j].fLBA48);
            SSMR3GetBool(pSSMHandle, &pData->aCts[i].aIfs[j].fATAPI);
            SSMR3GetBool(pSSMHandle, &pData->aCts[i].aIfs[j].fIrqPending);
            SSMR3GetU8(pSSMHandle, &pData->aCts[i].aIfs[j].cMultSectors);
            SSMR3GetU32(pSSMHandle, &pData->aCts[i].aIfs[j].cCHSCylinders);
            SSMR3GetU32(pSSMHandle, &pData->aCts[i].aIfs[j].cCHSHeads);
            SSMR3GetU32(pSSMHandle, &pData->aCts[i].aIfs[j].cCHSSectors);
            SSMR3GetU32(pSSMHandle, &pData->aCts[i].aIfs[j].cSectorsPerIRQ);
            SSMR3GetU64(pSSMHandle, &pData->aCts[i].aIfs[j].cTotalSectors);
            SSMR3GetU8(pSSMHandle, &pData->aCts[i].aIfs[j].uATARegFeature);
            SSMR3GetU8(pSSMHandle, &pData->aCts[i].aIfs[j].uATARegFeatureHOB);
            SSMR3GetU8(pSSMHandle, &pData->aCts[i].aIfs[j].uATARegError);
            SSMR3GetU8(pSSMHandle, &pData->aCts[i].aIfs[j].uATARegNSector);
            SSMR3GetU8(pSSMHandle, &pData->aCts[i].aIfs[j].uATARegNSectorHOB);
            SSMR3GetU8(pSSMHandle, &pData->aCts[i].aIfs[j].uATARegSector);
            SSMR3GetU8(pSSMHandle, &pData->aCts[i].aIfs[j].uATARegSectorHOB);
            SSMR3GetU8(pSSMHandle, &pData->aCts[i].aIfs[j].uATARegLCyl);
            SSMR3GetU8(pSSMHandle, &pData->aCts[i].aIfs[j].uATARegLCylHOB);
            SSMR3GetU8(pSSMHandle, &pData->aCts[i].aIfs[j].uATARegHCyl);
            SSMR3GetU8(pSSMHandle, &pData->aCts[i].aIfs[j].uATARegHCylHOB);
            SSMR3GetU8(pSSMHandle, &pData->aCts[i].aIfs[j].uATARegSelect);
            SSMR3GetU8(pSSMHandle, &pData->aCts[i].aIfs[j].uATARegStatus);
            SSMR3GetU8(pSSMHandle, &pData->aCts[i].aIfs[j].uATARegCommand);
            SSMR3GetU8(pSSMHandle, &pData->aCts[i].aIfs[j].uATARegDevCtl);
            SSMR3GetU8(pSSMHandle, &pData->aCts[i].aIfs[j].uATATransferMode);
            SSMR3GetU8(pSSMHandle, &pData->aCts[i].aIfs[j].uTxDir);
            SSMR3GetU8(pSSMHandle, &pData->aCts[i].aIfs[j].iBeginTransfer);
            SSMR3GetU8(pSSMHandle, &pData->aCts[i].aIfs[j].iSourceSink);
            SSMR3GetBool(pSSMHandle, &pData->aCts[i].aIfs[j].fDMA);
            SSMR3GetBool(pSSMHandle, &pData->aCts[i].aIfs[j].fATAPITransfer);
            SSMR3GetU32(pSSMHandle, &pData->aCts[i].aIfs[j].cbTotalTransfer);
            SSMR3GetU32(pSSMHandle, &pData->aCts[i].aIfs[j].cbElementaryTransfer);
            SSMR3GetU32(pSSMHandle, &pData->aCts[i].aIfs[j].iIOBufferCur);
            SSMR3GetU32(pSSMHandle, &pData->aCts[i].aIfs[j].iIOBufferEnd);
            SSMR3GetU32(pSSMHandle, &pData->aCts[i].aIfs[j].iIOBufferPIODataStart);
            SSMR3GetU32(pSSMHandle, &pData->aCts[i].aIfs[j].iIOBufferPIODataEnd);
            SSMR3GetU32(pSSMHandle, &pData->aCts[i].aIfs[j].iATAPILBA);
            SSMR3GetU32(pSSMHandle, &pData->aCts[i].aIfs[j].cbATAPISector);
            SSMR3GetMem(pSSMHandle, &pData->aCts[i].aIfs[j].aATAPICmd, sizeof(pData->aCts[i].aIfs[j].aATAPICmd));
            SSMR3GetU8(pSSMHandle, &pData->aCts[i].aIfs[j].uATAPISenseKey);
            SSMR3GetU8(pSSMHandle, &pData->aCts[i].aIfs[j].uATAPIASC);
            SSMR3GetU8(pSSMHandle, &pData->aCts[i].aIfs[j].cNotifiedMediaChange);
            SSMR3GetMem(pSSMHandle, &pData->aCts[i].aIfs[j].Led, sizeof(pData->aCts[i].aIfs[j].Led));
            SSMR3GetU32(pSSMHandle, &pData->aCts[i].aIfs[j].cbIOBuffer);
            if (pData->aCts[i].aIfs[j].cbIOBuffer)
            {
                if (pData->aCts[i].aIfs[j].CTXSUFF(pbIOBuffer))
                    SSMR3GetMem(pSSMHandle, pData->aCts[i].aIfs[j].CTXSUFF(pbIOBuffer), pData->aCts[i].aIfs[j].cbIOBuffer);
                else
                {
                    LogRel(("ATA: No buffer for %d/%d\n", i, j));
                    if (SSMR3HandleGetAfter(pSSMHandle) != SSMAFTER_DEBUG_IT)
                        return VERR_SSM_LOAD_CONFIG_MISMATCH;

                    /* skip the buffer if we're loading for the debugger / animator. */
                    uint8_t u8Ignored;
                    size_t cbLeft = pData->aCts[i].aIfs[j].cbIOBuffer;
                    while (cbLeft-- > 0)
                        SSMR3GetU8(pSSMHandle, &u8Ignored);
                }
            }
            else
                Assert(pData->aCts[i].aIfs[j].CTXSUFF(pbIOBuffer) == NULL);
        }
    }
    SSMR3GetBool(pSSMHandle, &pData->fPIIX4);

    rc = SSMR3GetU32(pSSMHandle, &u32);
    if (VBOX_FAILURE(rc))
        return rc;
    if (u32 != ~0U)
    {
        AssertMsgFailed(("u32=%#x expected ~0\n", u32));
        rc = VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
        return rc;
    }

    return VINF_SUCCESS;
}


/**
 * Construct a device instance for a VM.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 *                      If the registration structure is needed, pDevIns->pDevReg points to it.
 * @param   iInstance   Instance number. Use this to figure out which registers and such to use.
 *                      The device number is also found in pDevIns->iInstance, but since it's
 *                      likely to be freqently used PDM passes it as parameter.
 * @param   pCfgHandle  Configuration node handle for the device. Use this to obtain the configuration
 *                      of the device instance. It's also found in pDevIns->pCfgHandle, but like
 *                      iInstance it's expected to be used a bit in this function.
 */
static DECLCALLBACK(int)   ataConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfgHandle)
{
    PCIATAState    *pData = PDMINS2DATA(pDevIns, PCIATAState *);
    PPDMIBASE       pBase;
    int             rc;
    bool            fGCEnabled;
    bool            fR0Enabled;
    uint32_t        DelayIRQMillies;

    Assert(iInstance == 0);

    /*
     * Validate and read configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "GCEnabled\0IRQDelay\0R0Enabled\0PIIX4\0"))
        return PDMDEV_SET_ERROR(pDevIns, VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES,
                                N_("PIIX3 configuration error: unknown option specified"));

    rc = CFGMR3QueryBool(pCfgHandle, "GCEnabled", &fGCEnabled);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        fGCEnabled = true;
    else if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("PIIX3 configuration error: failed to read GCEnabled as boolean"));
    Log(("%s: fGCEnabled=%d\n", __FUNCTION__, fGCEnabled));

    rc = CFGMR3QueryBool(pCfgHandle, "R0Enabled", &fR0Enabled);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        fR0Enabled = true;
    else if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("PIIX3 configuration error: failed to read R0Enabled as boolean"));
    Log(("%s: fR0Enabled=%d\n", __FUNCTION__, fR0Enabled));

    rc = CFGMR3QueryU32(pCfgHandle, "IRQDelay", &DelayIRQMillies);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        DelayIRQMillies = 0;
    else if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("PIIX3 configuration error: failed to read IRQDelay as integer"));
    Log(("%s: DelayIRQMillies=%d\n", __FUNCTION__, DelayIRQMillies));
    Assert(DelayIRQMillies < 50);

    rc = CFGMR3QueryBool(pCfgHandle, "PIIX4", &pData->fPIIX4);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pData->fPIIX4 = false;
    else if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("PIIX3 configuration error: failed to read PIIX4 as boolean"));
    Log(("%s: fPIIX4=%d\n", __FUNCTION__, pData->fPIIX4));

    /*
     * Initialize data (most of it anyway).
     */
    /* Status LUN. */
    pData->IBase.pfnQueryInterface = ataStatus_QueryInterface;
    pData->ILeds.pfnQueryStatusLed = ataStatus_QueryStatusLed;

    /* pci */
    pData->dev.config[0x00] = 0x86; /* Vendor: Intel */
    pData->dev.config[0x01] = 0x80;
    if (pData->fPIIX4)
    {
        pData->dev.config[0x02] = 0x11; /* Device: PIIX4 IDE */
        pData->dev.config[0x03] = 0x71;
        pData->dev.config[0x08] = 0x01; /* Revision: PIIX4E */
        pData->dev.config[0x48] = 0x00; /* UDMACTL */
        pData->dev.config[0x4A] = 0x00; /* UDMATIM */
        pData->dev.config[0x4B] = 0x00;
    }
    else
    {
        pData->dev.config[0x02] = 0x10; /* Device: PIIX3 IDE */
        pData->dev.config[0x03] = 0x70;
    }
    pData->dev.config[0x04] = PCI_COMMAND_IOACCESS | PCI_COMMAND_MEMACCESS | PCI_COMMAND_BUSMASTER;
    pData->dev.config[0x09] = 0x8a; /* programming interface = PCI_IDE bus master is supported */
    pData->dev.config[0x0a] = 0x01; /* class_sub = PCI_IDE */
    pData->dev.config[0x0b] = 0x01; /* class_base = PCI_mass_storage */
    pData->dev.config[0x0e] = 0x00; /* header_type */

    pData->pDevIns          = pDevIns;
    pData->fGCEnabled       = fGCEnabled;
    pData->fR0Enabled       = fR0Enabled;
    for (uint32_t i = 0; i < RT_ELEMENTS(pData->aCts); i++)
    {
        pData->aCts[i].pDevInsHC = pDevIns;
        pData->aCts[i].pDevInsGC = PDMDEVINS_2_GCPTR(pDevIns);
        pData->aCts[i].DelayIRQMillies = (uint32_t)DelayIRQMillies;
        for (uint32_t j = 0; j < RT_ELEMENTS(pData->aCts[i].aIfs); j++)
        {
            pData->aCts[i].aIfs[j].iLUN = i * RT_ELEMENTS(pData->aCts) + j;
            pData->aCts[i].aIfs[j].pDevInsHC = pDevIns;
            pData->aCts[i].aIfs[j].pDevInsGC = PDMDEVINS_2_GCPTR(pDevIns);
            pData->aCts[i].aIfs[j].pControllerHC = &pData->aCts[i];
            pData->aCts[i].aIfs[j].pControllerGC = MMHyperHC2GC(PDMDevHlpGetVM(pDevIns), &pData->aCts[i]);
            pData->aCts[i].aIfs[j].IBase.pfnQueryInterface = ataQueryInterface;
            pData->aCts[i].aIfs[j].IMountNotify.pfnMountNotify = ataMountNotify;
            pData->aCts[i].aIfs[j].IMountNotify.pfnUnmountNotify = ataUnmountNotify;
            pData->aCts[i].aIfs[j].Led.u32Magic = PDMLED_MAGIC;
        }
    }

    Assert(RT_ELEMENTS(pData->aCts) == 2);
    pData->aCts[0].irq          = 14;
    pData->aCts[0].IOPortBase1  = 0x1f0;
    pData->aCts[0].IOPortBase2  = 0x3f6;
    pData->aCts[1].irq          = 15;
    pData->aCts[1].IOPortBase1  = 0x170;
    pData->aCts[1].IOPortBase2  = 0x376;

    /*
     * Register the PCI device.
     * N.B. There's a hack in the PIIX3 PCI bridge device to assign this
     *      device the slot next to itself.
     */
    rc = PDMDevHlpPCIRegister(pDevIns, &pData->dev);
    if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("PIIX3 cannot register PCI device"));
    AssertMsg(pData->dev.devfn == 9 || iInstance != 0, ("pData->dev.devfn=%d\n", pData->dev.devfn));
    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 4, 0x10, PCI_ADDRESS_SPACE_IO, ataBMDMAIORangeMap);
    if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("PIIX3 cannot register PCI I/O region for BMDMA"));

    /*
     * Register the I/O ports.
     * The ports are all hardcoded and enforced by the PIIX3 host bridge controller.
     */
    for (uint32_t i = 0; i < RT_ELEMENTS(pData->aCts); i++)
    {
        rc = PDMDevHlpIOPortRegister(pDevIns, pData->aCts[i].IOPortBase1, 8, (RTHCPTR)i,
                                     ataIOPortWrite1, ataIOPortRead1, ataIOPortWriteStr1, ataIOPortReadStr1, "ATA I/O Base 1");
        if (VBOX_FAILURE(rc))
            return PDMDEV_SET_ERROR(pDevIns, rc, N_("PIIX3 cannot register I/O handlers"));

        if (fGCEnabled)
        {
            rc = PDMDevHlpIOPortRegisterGC(pDevIns, pData->aCts[i].IOPortBase1, 8, (RTGCPTR)i,
                                           "ataIOPortWrite1", "ataIOPortRead1", "ataIOPortWriteStr1", "ataIOPortReadStr1", "ATA I/O Base 1");
            if (VBOX_FAILURE(rc))
                return PDMDEV_SET_ERROR(pDevIns, rc, N_("PIIX3 cannot register I/O handlers (GC)"));
        }

        if (fR0Enabled)
        {
#if 1
            rc = PDMDevHlpIOPortRegisterR0(pDevIns, pData->aCts[i].IOPortBase1, 8, (RTR0PTR)i,
                                           "ataIOPortWrite1", "ataIOPortRead1", NULL, NULL, "ATA I/O Base 1");
#else
            rc = PDMDevHlpIOPortRegisterR0(pDevIns, pData->aCts[i].IOPortBase1, 8, (RTR0PTR)i,
                                           "ataIOPortWrite1", "ataIOPortRead1", "ataIOPortWriteStr1", "ataIOPortReadStr1", "ATA I/O Base 1");
#endif
            if (VBOX_FAILURE(rc))
                return PDMDEV_SET_ERROR(pDevIns, rc, "PIIX3 cannot register I/O handlers (R0).");
        }

        rc = PDMDevHlpIOPortRegister(pDevIns, pData->aCts[i].IOPortBase2, 1, (RTHCPTR)i,
                                     ataIOPortWrite2, ataIOPortRead2, NULL, NULL, "ATA I/O Base 2");
        if (VBOX_FAILURE(rc))
            return PDMDEV_SET_ERROR(pDevIns, rc, N_("PIIX3 cannot register base2 I/O handlers"));

        if (fGCEnabled)
        {
            rc = PDMDevHlpIOPortRegisterGC(pDevIns, pData->aCts[i].IOPortBase2, 1, (RTGCPTR)i,
                                           "ataIOPortWrite2", "ataIOPortRead2", NULL, NULL, "ATA I/O Base 2");
            if (VBOX_FAILURE(rc))
                return PDMDEV_SET_ERROR(pDevIns, rc, N_("PIIX3 cannot register base2 I/O handlers (GC)"));
        }
        if (fR0Enabled)
        {
            rc = PDMDevHlpIOPortRegisterR0(pDevIns, pData->aCts[i].IOPortBase2, 1, (RTR0PTR)i,
                                           "ataIOPortWrite2", "ataIOPortRead2", NULL, NULL, "ATA I/O Base 2");
            if (VBOX_FAILURE(rc))
                return PDMDEV_SET_ERROR(pDevIns, rc, N_("PIIX3 cannot register base2 I/O handlers (R0)"));
        }

        for (uint32_t j = 0; j < RT_ELEMENTS(pData->aCts[i].aIfs); j++)
        {
            ATADevState *pIf = &pData->aCts[i].aIfs[j];
            PDMDevHlpSTAMRegisterF(pDevIns, &pIf->StatATADMA,       STAMTYPE_COUNTER,    STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,       "Number of ATA DMA transfers.", "/Devices/ATA%d/Unit%d/DMA", i, j);
            PDMDevHlpSTAMRegisterF(pDevIns, &pIf->StatATAPIO,       STAMTYPE_COUNTER,    STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,       "Number of ATA PIO transfers.", "/Devices/ATA%d/Unit%d/PIO", i, j);
            PDMDevHlpSTAMRegisterF(pDevIns, &pIf->StatATAPIDMA,     STAMTYPE_COUNTER,    STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,       "Number of ATAPI DMA transfers.", "/Devices/ATA%d/Unit%d/AtapiDMA", i, j);
            PDMDevHlpSTAMRegisterF(pDevIns, &pIf->StatATAPIPIO,     STAMTYPE_COUNTER,    STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,       "Number of ATAPI PIO transfers.", "/Devices/ATA%d/Unit%d/AtapiPIO", i, j);
#ifdef VBOX_WITH_STATISTICS /** @todo release too. */
            PDMDevHlpSTAMRegisterF(pDevIns, &pIf->StatReads,        STAMTYPE_PROFILE_ADV, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL,  "Profiling of the read operations.", "/Devices/ATA%d/Unit%d/Reads", i, j);
            PDMDevHlpSTAMRegisterF(pDevIns, &pIf->StatBytesRead,    STAMTYPE_COUNTER,     STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,           "Amount of data read.",              "/Devices/ATA%d/Unit%d/ReadBytes", i, j);
            PDMDevHlpSTAMRegisterF(pDevIns, &pIf->StatWrites,       STAMTYPE_PROFILE_ADV, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL,  "Profiling of the write operations.","/Devices/ATA%d/Unit%d/Writes", i, j);
            PDMDevHlpSTAMRegisterF(pDevIns, &pIf->StatBytesWritten, STAMTYPE_COUNTER,     STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,           "Amount of data written.",           "/Devices/ATA%d/Unit%d/WrittenBytes", i, j);
            PDMDevHlpSTAMRegisterF(pDevIns, &pIf->StatFlushes,      STAMTYPE_PROFILE,     STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL,  "Profiling of the flush operations.","/Devices/ATA%d/Unit%d/Flushes", i, j);
#endif
        }
#ifdef VBOX_WITH_STATISTICS /** @todo release too. */
        PDMDevHlpSTAMRegisterF(pDevIns, &pData->aCts[i].StatAsyncOps,       STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,   "The number of async operations.",  "/Devices/ATA%d/Async/Operations", i);
        /** @todo STAMUNIT_MICROSECS */
        PDMDevHlpSTAMRegisterF(pDevIns, &pData->aCts[i].StatAsyncMinWait, STAMTYPE_U64_RESET, STAMVISIBILITY_ALWAYS, STAMUNIT_NONE,         "Minimum wait in microseconds.",    "/Devices/ATA%d/Async/MinWait", i);
        PDMDevHlpSTAMRegisterF(pDevIns, &pData->aCts[i].StatAsyncMaxWait, STAMTYPE_U64_RESET, STAMVISIBILITY_ALWAYS, STAMUNIT_NONE,         "Maximum wait in microseconds.",    "/Devices/ATA%d/Async/MaxWait", i);
        PDMDevHlpSTAMRegisterF(pDevIns, &pData->aCts[i].StatAsyncTimeUS,    STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_NONE,         "Total time spent in microseconds.","/Devices/ATA%d/Async/TotalTimeUS", i);
        PDMDevHlpSTAMRegisterF(pDevIns, &pData->aCts[i].StatAsyncTime,  STAMTYPE_PROFILE_ADV, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL, "Profiling of async operations.", "/Devices/ATA%d/Async/Time", i);
        PDMDevHlpSTAMRegisterF(pDevIns, &pData->aCts[i].StatLockWait,       STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL, "Profiling of locks.",            "/Devices/ATA%d/Async/LockWait", i);
#endif /* VBOX_WITH_STATISTICS */

        /* Initialize per-controller critical section */
        char szName[24];
        RTStrPrintf(szName, sizeof(szName), "ATA%d", i);
        rc = PDMDevHlpCritSectInit(pDevIns, &pData->aCts[i].lock, szName);
        if (VBOX_FAILURE(rc))
            return PDMDEV_SET_ERROR(pDevIns, rc, N_("PIIX3 cannot initialize critical section"));
    }

    /*
     * Attach status driver (optional).
     */
    rc = PDMDevHlpDriverAttach(pDevIns, PDM_STATUS_LUN, &pData->IBase, &pBase, "Status Port");
    if (VBOX_SUCCESS(rc))
        pData->pLedsConnector = (PDMILEDCONNECTORS *)pBase->pfnQueryInterface(pBase, PDMINTERFACE_LED_CONNECTORS);
    else if (rc != VERR_PDM_NO_ATTACHED_DRIVER)
    {
        AssertMsgFailed(("Failed to attach to status driver. rc=%Vrc\n", rc));
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("PIIX3 cannot attach to status driver"));
    }

    /*
     * Attach the units.
     */
    uint32_t cbTotalBuffer = 0;
    for (uint32_t i = 0; i < RT_ELEMENTS(pData->aCts); i++)
    {
        PATACONTROLLER pCtl = &pData->aCts[i];

        /*
         * Start the worker thread.
         */
        pCtl->uAsyncIOState = ATA_AIO_NEW;
        rc = RTSemEventCreate(&pCtl->AsyncIOSem);
        AssertRC(rc);
        rc = RTSemEventCreate(&pCtl->SuspendIOSem);
        AssertRC(rc);
        rc = RTSemMutexCreate(&pCtl->AsyncIORequestMutex);
        AssertRC(rc);
        ataAsyncIOClearRequests(pCtl);
        rc = RTThreadCreate(&pCtl->AsyncIOThread, ataAsyncIOLoop, (void *)pCtl, 128*1024, RTTHREADTYPE_IO, 0, "ATA");
        AssertRC(rc);
        Assert(pCtl->AsyncIOThread != NIL_RTTHREAD && pCtl->AsyncIOSem != NIL_RTSEMEVENT && pCtl->SuspendIOSem != NIL_RTSEMEVENT && pCtl->AsyncIORequestMutex != NIL_RTSEMMUTEX);
        Log(("%s: controller %d AIO thread id %#x; sem %p susp_sem %p mutex %p\n", __FUNCTION__, i, pCtl->AsyncIOThread, pCtl->AsyncIOSem, pCtl->SuspendIOSem, pCtl->AsyncIORequestMutex));

        for (uint32_t j = 0; j < RT_ELEMENTS(pCtl->aIfs); j++)
        {
            static const char *s_apszDescs[RT_ELEMENTS(pData->aCts)][RT_ELEMENTS(pCtl->aIfs)] =
            {
                { "Primary Master", "Primary Slave" },
                { "Secondary Master", "Secondary Slave" }
            };

            /*
             * Try attach the block device and get the interfaces,
             * required as well as optional.
             */
            ATADevState *pIf = &pCtl->aIfs[j];

            rc = PDMDevHlpDriverAttach(pDevIns, pIf->iLUN, &pIf->IBase, &pIf->pDrvBase, s_apszDescs[i][j]);
            if (VBOX_SUCCESS(rc))
                rc = ataConfigLun(pDevIns, pIf);
            else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
            {
                pIf->pDrvBase = NULL;
                pIf->pDrvBlock = NULL;
                pIf->cbIOBuffer = 0;
                pIf->pbIOBufferHC = NULL;
                pIf->pbIOBufferGC = NIL_RTGCPHYS;
                LogRel(("PIIX3 ATA: LUN#%d: no unit\n", pIf->iLUN));
            }
            else
            {
                AssertMsgFailed(("Failed to attach LUN#%d. rc=%Vrc\n", pIf->iLUN, rc));
                switch (rc)
                {
                    case VERR_ACCESS_DENIED:
                        /* Error already catched by DrvHostBase */
                        return rc;
                    default:
                        return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                                   N_("PIIX3 cannot attach drive to the %s"),
                                                   s_apszDescs[i][j]);
                }
            }
            cbTotalBuffer += pIf->cbIOBuffer;
        }
    }

    rc = PDMDevHlpSSMRegister(pDevIns, pDevIns->pDevReg->szDeviceName, iInstance,
                              ATA_SAVED_STATE_VERSION, sizeof(*pData) + cbTotalBuffer,
                              ataSaveLoadPrep, ataSaveExec, NULL,
                              ataSaveLoadPrep, ataLoadExec, NULL);
    if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("PIIX3 cannot register save state handlers"));

    /*
     * Initialize the device state.
     */
    ataReset(pDevIns);

    return VINF_SUCCESS;
}


/**
 * The device registration structure.
 */
const PDMDEVREG g_DevicePIIX3IDE =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szDeviceName */
    "piix3ide",
    /* szGCMod */
    "VBoxDDGC.gc",
    /* szR0Mod */
    "VBoxDDR0.r0",
    /* pszDescription */
    "Intel PIIX3 ATA controller.\n"
    "  LUN #0 is primary master.\n"
    "  LUN #1 is primary slave.\n"
    "  LUN #2 is secondary master.\n"
    "  LUN #3 is secondary slave.\n"
    "  LUN #999 is the LED/Status connector.",
    /* fFlags */
    PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT | PDM_DEVREG_FLAGS_GUEST_BITS_DEFAULT | PDM_DEVREG_FLAGS_GC | PDM_DEVREG_FLAGS_R0,
    /* fClass */
    PDM_DEVREG_CLASS_STORAGE,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(PCIATAState),
    /* pfnConstruct */
    ataConstruct,
    /* pfnDestruct */
    ataDestruct,
    /* pfnRelocate */
    ataRelocate,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    ataReset,
    /* pfnSuspend */
    ataSuspend,
    /* pfnResume */
    ataResume,
    /* pfnAttach */
    ataAttach,
    /* pfnDetach */
    ataDetach,
    /* pfnQueryInterface. */
    NULL,
    /* pfnInitComplete */
    NULL,
    /* pfnPowerOff */
    ataPowerOff
};
#endif /* IN_RING3 */
#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */

