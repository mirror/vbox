/* $Id$ */
/** @file
 * VBox storage devices: AHCI controller device (disk and cdrom).
 *                       Implements the AHCI standard 1.1
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/** @page pg_dev_ahci   AHCI - Advanced Host Controller Interface Emulation.
 *
 * This component implements an AHCI serial ATA controller.  The device is split
 * into two parts.  The first part implements the register interface for the
 * guest and the second one does the data transfer.
 *
 * The guest can access the controller in two ways.  The first one is the native
 * way implementing the registers described in the AHCI specification and is
 * the preferred one.  The second implements the I/O ports used for booting from
 * the hard disk and for guests which don't have an AHCI SATA driver.
 *
 * The data is transfered in an asychronous way using one thread per implemented
 * port or using the new async completion interface which is still under
 * development. [not quite up to date]
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
//#define DEBUG
#define LOG_GROUP LOG_GROUP_DEV_AHCI
#include <VBox/pdmdev.h>
#include <VBox/pdmqueue.h>
#include <VBox/pdmthread.h>
#include <VBox/pdmcritsect.h>
#include <VBox/scsi.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#ifdef IN_RING3
# include <iprt/param.h>
# include <iprt/thread.h>
# include <iprt/semaphore.h>
# include <iprt/alloc.h>
# include <iprt/uuid.h>
# include <iprt/time.h>
#endif

#include "ide.h"
#include "ATAController.h"
#include "../Builtins.h"

#define AHCI_MAX_NR_PORTS_IMPL 30
#define AHCI_NR_COMMAND_SLOTS 32
#define AHCI_NR_OF_ALLOWED_BIGGER_LISTS 100

/** The current saved state version. */
#define AHCI_SAVED_STATE_VERSION                3
/** The saved state version use in VirtualBox 3.0 and earlier.
 * This was before the config was added and ahciIOTasks was dropped. */
#define AHCI_SAVED_STATE_VERSION_VBOX_30        2

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
 * Length of the configurable VPD data (without termination)
 */
#define AHCI_SERIAL_NUMBER_LENGTH            20
#define AHCI_FIRMWARE_REVISION_LENGTH         8
#define AHCI_MODEL_NUMBER_LENGTH             40
#define AHCI_ATAPI_INQUIRY_VENDOR_ID_LENGTH   8
#define AHCI_ATAPI_INQUIRY_PRODUCT_ID_LENGTH 16
#define AHCI_ATAPI_INQUIRY_REVISION_LENGTH    4

/* Command Header. */
typedef struct
{
    /** Description Information. */
    uint32_t           u32DescInf;
    /** Command status. */
    uint32_t           u32PRDBC;
    /** Command Table Base Address. */
    uint32_t           u32CmdTblAddr;
    /** Command Table Base Address - upper 32-bits. */
    uint32_t           u32CmdTblAddrUp;
    /** Reserved */
    uint32_t           u32Reserved[4];
} CmdHdr;
AssertCompileSize(CmdHdr, 32);

/* Defines for the command header. */
#define AHCI_CMDHDR_PRDTL_MASK 0xffff0000
#define AHCI_CMDHDR_PRDTL_ENTRIES(x) ((x & AHCI_CMDHDR_PRDTL_MASK) >> 16)
#define AHCI_CMDHDR_C          RT_BIT(10)
#define AHCI_CMDHDR_B          RT_BIT(9)
#define AHCI_CMDHDR_R          RT_BIT(8)
#define AHCI_CMDHDR_P          RT_BIT(7)
#define AHCI_CMDHDR_W          RT_BIT(6)
#define AHCI_CMDHDR_A          RT_BIT(5)
#define AHCI_CMDHDR_CFL_MASK   0x1f

#define AHCI_CMDHDR_PRDT_OFFSET 0x80
#define AHCI_CMDHDR_ACMD_OFFSET 0x40

/* Defines for the command FIS. */
/* Defines that are used in the first double word. */
#define AHCI_CMDFIS_TYPE                  0 /* The first byte. */
# define AHCI_CMDFIS_TYPE_H2D             0x27 /* Register - Host to Device FIS. */
# define AHCI_CMDFIS_TYPE_H2D_SIZE        20   /* Five double words. */
# define AHCI_CMDFIS_TYPE_D2H             0x34 /* Register - Device to Host FIS. */
# define AHCI_CMDFIS_TYPE_D2H_SIZE        20   /* Five double words. */
# define AHCI_CMDFIS_TYPE_SETDEVBITS      0xa1 /* Set Device Bits - Device to Host FIS. */
# define AHCI_CMDFIS_TYPE_SETDEVBITS_SIZE 8    /* Two double words. */
# define AHCI_CMDFIS_TYPE_DMAACTD2H       0x39 /* DMA Activate - Device to Host FIS. */
# define AHCI_CMDFIS_TYPE_DMAACTD2H_SIZE  4    /* One double word. */
# define AHCI_CMDFIS_TYPE_DMASETUP        0x41 /* DMA Setup - Bidirectional FIS. */
# define AHCI_CMDFIS_TYPE_DMASETUP_SIZE   28   /* Seven double words. */
# define AHCI_CMDFIS_TYPE_PIOSETUP        0x5f /* PIO Setup - Device to Host FIS. */
# define AHCI_CMDFIS_TYPE_PIOSETUP_SIZE   20   /* Five double words. */
# define AHCI_CMDFIS_TYPE_DATA            0x46 /* Data - Bidirectional FIS. */

#define AHCI_CMDFIS_BITS                  1 /* Interrupt and Update bit. */
#define AHCI_CMDFIS_C                     RT_BIT(7) /* Host to device. */
#define AHCI_CMDFIS_I                     RT_BIT(6) /* Device to Host. */

#define AHCI_CMDFIS_CMD                   2
#define AHCI_CMDFIS_FET                   3

#define AHCI_CMDFIS_SECTN                 4
#define AHCI_CMDFIS_CYLL                  5
#define AHCI_CMDFIS_CYLH                  6
#define AHCI_CMDFIS_HEAD                  7

#define AHCI_CMDFIS_SECTNEXP              8
#define AHCI_CMDFIS_CYLLEXP               9
#define AHCI_CMDFIS_CYLHEXP               10
#define AHCI_CMDFIS_FETEXP                11

#define AHCI_CMDFIS_SECTC                 12
#define AHCI_CMDFIS_SECTCEXP              13
#define AHCI_CMDFIS_CTL                   15
# define AHCI_CMDFIS_CTL_SRST             RT_BIT(2) /* Reset device. */
# define AHCI_CMDFIS_CTL_NIEN             RT_BIT(1) /* Assert or clear interrupt. */

/* For D2H FIS */
#define AHCI_CMDFIS_STS                   2
#define AHCI_CMDFIS_ERR                   3

/**
 * Scatter gather list entry data.
 */
typedef struct AHCIPORTTASKSTATESGENTRY
{
    /** Flag whether the buffer in the list is from the guest or an
     *  allocated temporary buffer because the segments in the guest
     *  are not sector aligned.
     */
    bool     fGuestMemory;
    /** Flag dependent data. */
    union
    {
        /** Data to handle direct mappings of guest buffers. */
        struct
        {
            /** The page lock. */
            PGMPAGEMAPLOCK  PageLock;
        } direct;
        /** Data to handle temporary buffers. */
        struct
        {
            /** The first segment in the guest which is not sector aligned. */
            RTGCPHYS        GCPhysAddrBaseFirstUnaligned;
            /** Number of unaligned buffers in the guest. */
            uint32_t        cUnaligned;
            /** Pointer to the start of the buffer. */
            void           *pvBuf;
        } temp;
    } u;
} AHCIPORTTASKSTATESGENTRY, *PAHCIPORTTASKSTATESGENTRY;

/** Pointer to a pointer of a scatter gather list entry. */
typedef PAHCIPORTTASKSTATESGENTRY *PPAHCIPORTTASKSTATESGENTRY;
/** Pointer to a task state. */
typedef struct AHCIPORTTASKSTATE *PAHCIPORTTASKSTATE;

/**
 * Data processing callback
 *
 * @returns VBox status.
 * @param   pAhciPortTaskState    The task state.
 */
typedef DECLCALLBACK(int)   FNAHCIPOSTPROCESS(PAHCIPORTTASKSTATE pAhciPortTaskState);
/** Pointer to a FNAHCIPOSTPROCESS() function. */
typedef FNAHCIPOSTPROCESS *PFNAHCIPOSTPROCESS;

/**
 * A task state.
 */
typedef struct AHCIPORTTASKSTATE
{
    /** Tag of the task. */
    uint32_t                   uTag;
    /** Command is queued. */
    bool                       fQueued;
    /** The command header for this task. */
    CmdHdr                     cmdHdr;
    /** The command Fis for this task. */
    uint8_t                    cmdFis[AHCI_CMDFIS_TYPE_H2D_SIZE];
    /** The ATAPI comnmand data. */
    uint8_t                    aATAPICmd[ATAPI_PACKET_SIZE];
    /** Physical address of the command header. - GC */
    RTGCPHYS                   GCPhysCmdHdrAddr;
    /** Data direction. */
    uint8_t                    uTxDir;
    /** Start offset. */
    uint64_t                   uOffset;
    /** Number of bytes to transfer. */
    uint32_t                   cbTransfer;
    /** ATA error register */
    uint8_t                    uATARegError;
    /** ATA status register */
    uint8_t                    uATARegStatus;
    /** How many entries would fit into the sg list. */
    uint32_t                   cSGListSize;
    /** Number of used SG list entries. */
    uint32_t                   cSGListUsed;
    /** Pointer to the first entry of the scatter gather list. */
    PPDMDATASEG                pSGListHead;
    /** Number of scatter gather list entries. */
    uint32_t                   cSGEntries;
    /** Total number of bytes the guest reserved for this request.
     * Sum of all SG entries. */
    uint32_t                   cbSGBuffers;
    /** Pointer to the first mapping information entry. */
    PAHCIPORTTASKSTATESGENTRY  paSGEntries;
    /** Size of the temporary buffer for unaligned guest segments. */
    uint32_t                   cbBufferUnaligned;
    /** Pointer to the temporary buffer. */
    void                      *pvBufferUnaligned;
    /** Number of times in a row the scatter gather list was too big. */
    uint32_t                   cSGListTooBig;
    /** Post processing callback.
     * If this is set we will use a buffer for the data
     * and the callback copies the data to the destination. */
    PFNAHCIPOSTPROCESS         pfnPostProcess;
} AHCIPORTTASKSTATE;

/**
 * Notifier queue item.
 */
typedef struct DEVPORTNOTIFIERQUEUEITEM
{
    /** The core part owned by the queue manager. */
    PDMQUEUEITEMCORE    Core;
    /** On which port the async io thread should be put into action. */
    uint8_t             iPort;
    /** Which task to process. */
    uint8_t             iTask;
    /** Flag whether the task is queued. */
    uint8_t             fQueued;
} DEVPORTNOTIFIERQUEUEITEM, *PDEVPORTNOTIFIERQUEUEITEM;

typedef struct AHCIPort
{
    /** Pointer to the device instance - HC ptr */
    PPDMDEVINSR3                    pDevInsR3;
    /** Pointer to the device instance - R0 ptr */
    PPDMDEVINSR0                    pDevInsR0;
    /** Pointer to the device instance - RC ptr. */
    PPDMDEVINSRC                    pDevInsRC;

#if HC_ARCH_BITS == 64
    uint32_t                        Alignment0;
#endif

    /** Pointer to the parent AHCI structure - R3 ptr. */
    R3PTRTYPE(struct AHCI *)        pAhciR3;
    /** Pointer to the parent AHCI structure - R0 ptr. */
    R0PTRTYPE(struct AHCI *)        pAhciR0;
    /** Pointer to the parent AHCI structure - RC ptr. */
    RCPTRTYPE(struct AHCI *)        pAhciRC;
    /** Command List Base Address. */
    uint32_t                        regCLB;
    /** Command List Base Address upper bits. */
    uint32_t                        regCLBU;
    /** FIS Base Address. */
    uint32_t                        regFB;
    /** FIS Base Address upper bits. */
    uint32_t                        regFBU;
    /** Interrupt Status. */
    volatile uint32_t               regIS;
    /** Interrupt Enable. */
    uint32_t                        regIE;
    /** Command. */
    uint32_t                        regCMD;
    /** Task File Data. */
    uint32_t                        regTFD;
    /** Signature */
    uint32_t                        regSIG;
    /** Serial ATA Status. */
    uint32_t                        regSSTS;
    /** Serial ATA Control. */
    uint32_t                        regSCTL;
    /** Serial ATA Error. */
    uint32_t                        regSERR;
    /** Serial ATA Active. */
    uint32_t                        regSACT;
    /** Command Issue. */
    uint32_t                        regCI;

#if HC_ARCH_BITS == 64
    uint32_t                        Alignment1;
#endif

    /** Command List Base Address */
    volatile RTGCPHYS               GCPhysAddrClb;
    /** FIS Base Address */
    volatile RTGCPHYS               GCPhysAddrFb;

    /** If we use the new async interface. */
    bool                            fAsyncInterface;

#if HC_ARCH_BITS == 64
    uint32_t                        Alignment2;
#endif

    /** Async IO Thread. */
    PPDMTHREAD                      pAsyncIOThread;
    /** Request semaphore. */
    RTSEMEVENT                      AsyncIORequestSem;

    /** Task queue. */
    volatile uint8_t                ahciIOTasks[2*AHCI_NR_COMMAND_SLOTS];
    /** Actual write position. */
    uint8_t                         uActWritePos;
    /** Actual read position. */
    uint8_t                         uActReadPos;
    /** Actual number of active tasks. */
    volatile uint32_t               uActTasksActive;

    /** Device is powered on. */
    bool                            fPoweredOn;
    /** Device has spun up. */
    bool                            fSpunUp;
    /** First D2H FIS was send. */
    bool                            fFirstD2HFisSend;
    /** Attached device is a CD/DVD drive. */
    bool                            fATAPI;

#if HC_ARCH_BITS == 64
    uint32_t                        Alignment3;
#endif

    /** Device specific settings. */
    /** Pointer to the attached driver's base interface. */
    R3PTRTYPE(PPDMIBASE)            pDrvBase;
    /** Pointer to the attached driver's block interface. */
    R3PTRTYPE(PPDMIBLOCK)           pDrvBlock;
    /** Pointer to the attached driver's async block interface. */
    R3PTRTYPE(PPDMIBLOCKASYNC)      pDrvBlockAsync;
    /** Pointer to the attached driver's block bios interface. */
    R3PTRTYPE(PPDMIBLOCKBIOS)       pDrvBlockBios;
    /** Pointer to the attached driver's mount interface. */
    R3PTRTYPE(PPDMIMOUNT)           pDrvMount;
    /** The base interface. */
    PDMIBASE                        IBase;
    /** The block port interface. */
    PDMIBLOCKPORT                   IPort;
    /** The optional block async port interface. */
    PDMIBLOCKASYNCPORT              IPortAsync;
    /** The mount notify interface. */
    PDMIMOUNTNOTIFY                 IMountNotify;
    /** Physical geometry of this image. */
    PDMMEDIAGEOMETRY                PCHSGeometry;
    /** The status LED state for this drive. */
    PDMLED                          Led;

#if HC_ARCH_BITS == 64
    uint32_t                        Alignment4;
#endif

    /** Number of total sectors. */
    uint64_t                        cTotalSectors;
    /** Currently configured number of sectors in a multi-sector transfer. */
    uint32_t                        cMultSectors;
    /** Currently active transfer mode (MDMA/UDMA) and speed. */
    uint8_t                         uATATransferMode;
    /** ATAPI sense key. */
    uint8_t                         uATAPISenseKey;
    /** ATAPI additional sens code. */
    uint8_t                         uATAPIASC;
    /** HACK: Countdown till we report a newly unmounted drive as mounted. */
    uint8_t                         cNotifiedMediaChange;

    /** The LUN. */
    RTUINT                          iLUN;
    /** Flag if we are in a device reset. */
    bool                            fResetDevice;

    /** Bitmask for finished tasks. */
    volatile uint32_t               u32TasksFinished;
    /** Bitmask for finished queued tasks. */
    volatile uint32_t               u32QueuedTasksFinished;

    /**
     * Array of cached tasks. The tag number is the index value.
     * Only used with the async interface.
     */
    R3PTRTYPE(PAHCIPORTTASKSTATE)   aCachedTasks[AHCI_NR_COMMAND_SLOTS];

    uint32_t                        u32Alignment5[4];

    /** Release statistics: number of DMA commands. */
    STAMCOUNTER                     StatDMA;
    /** Release statistics: number of bytes written. */
    STAMCOUNTER                     StatBytesWritten;
    /** Release statistics: number of bytes read. */
    STAMCOUNTER                     StatBytesRead;
    /** Release statistics: Number of I/O requests processed per second. */
    STAMCOUNTER                     StatIORequestsPerSecond;
#ifdef VBOX_WITH_STATISTICS
    /** Statistics: Time to complete one request. */
    STAMPROFILE                     StatProfileProcessTime;
    /** Statistics: Time to map requests into R3. */
    STAMPROFILE                     StatProfileMapIntoR3;
    /** Statistics: Amount of time to read/write data. */
    STAMPROFILE                     StatProfileReadWrite;
    /** Statistics: Amount of time to destroy a list. */
    STAMPROFILE                     StatProfileDestroyScatterGatherList;
#endif /* VBOX_WITH_STATISTICS */
    /** Flag whether a notification was already send to R3. */
    volatile bool                   fNotificationSend;
    /** Flag whether this port is in a reset state. */
    volatile bool                   fPortReset;
    /** Flag whether the I/O thread idles. */
    volatile bool                   fAsyncIOThreadIdle;

    /** The serial numnber to use for IDENTIFY DEVICE commands. */
    char                            szSerialNumber[AHCI_SERIAL_NUMBER_LENGTH+1]; /** < one extra byte for termination */
    /** The firmware revision to use for IDENTIFY DEVICE commands. */
    char                            szFirmwareRevision[AHCI_FIRMWARE_REVISION_LENGTH+1]; /** < one extra byte for termination */
    /** The model number to use for IDENTIFY DEVICE commands. */
    char                            szModelNumber[AHCI_MODEL_NUMBER_LENGTH+1]; /** < one extra byte for termination */
    /** The vendor identification string for SCSI INQUIRY commands. */
    char                            szInquiryVendorId[AHCI_ATAPI_INQUIRY_VENDOR_ID_LENGTH+1];
    /** The product identification string for SCSI INQUIRY commands. */
    char                            szInquiryProductId[AHCI_ATAPI_INQUIRY_PRODUCT_ID_LENGTH+1];
    /** The revision string for SCSI INQUIRY commands. */
    char                            szInquiryRevision[AHCI_ATAPI_INQUIRY_REVISION_LENGTH+1];

    uint32_t                        Alignment6;

} AHCIPort, *PAHCIPort;

/*
 * Main AHCI device state.
 */
typedef struct AHCI
{
    /** The PCI device structure. */
    PCIDEVICE                       dev;
    /** Pointer to the device instance - R3 ptr */
    PPDMDEVINSR3                    pDevInsR3;
    /** Pointer to the device instance - R0 ptr */
    PPDMDEVINSR0                    pDevInsR0;
    /** Pointer to the device instance - RC ptr. */
    PPDMDEVINSRC                    pDevInsRC;

#if HC_ARCH_BITS == 64
    uint32_t                        Alignment0;
#endif

    /** The base interface */
    PDMIBASE                        IBase;
    /** Status Port - Leds interface. */
    PDMILEDPORTS                    ILeds;
    /** Partner of ILeds. */
    R3PTRTYPE(PPDMILEDCONNECTORS)   pLedsConnector;

#if HC_ARCH_BITS == 64
    uint32_t                        Alignment1[2];
#endif

    /** Base address of the MMIO region. */
    RTGCPHYS                        MMIOBase;

    /** Global Host Control register of the HBA */

    /** HBA Capabilities - Readonly */
    uint32_t                        regHbaCap;
    /** HBA Control */
    uint32_t                        regHbaCtrl;
    /** Interrupt Status */
    uint32_t                        regHbaIs;
    /** Ports Implemented - Readonly */
    uint32_t                        regHbaPi;
    /** AHCI Version - Readonly */
    uint32_t                        regHbaVs;
    /** Command completion coalescing control */
    uint32_t                        regHbaCccCtl;
    /** Command completion coalescing ports */
    uint32_t                        regHbaCccPorts;

#if HC_ARCH_BITS == 64
    uint32_t                        Alignment3;
#endif

    /** Countdown timer for command completion coalescing - R3 ptr */
    PTMTIMERR3                      pHbaCccTimerR3;
    /** Countdown timer for command completion coalescing - R0 ptr */
    PTMTIMERR0                      pHbaCccTimerR0;
    /** Countdown timer for command completion coalescing - RC ptr */
    PTMTIMERRC                      pHbaCccTimerRC;

#if HC_ARCH_BITS == 64
    uint32_t                        Alignment4;
#endif

    /** Queue to send tasks to R3. - HC ptr */
    R3PTRTYPE(PPDMQUEUE)            pNotifierQueueR3;
    /** Queue to send tasks to R3. - HC ptr */
    R0PTRTYPE(PPDMQUEUE)            pNotifierQueueR0;
    /** Queue to send tasks to R3. - RC ptr */
    RCPTRTYPE(PPDMQUEUE)            pNotifierQueueRC;

#if HC_ARCH_BITS == 64
    uint32_t                        Alignment5;
#endif


    /** Which port number is used to mark an CCC interrupt */
    uint8_t                         uCccPortNr;

#if HC_ARCH_BITS == 64
    uint32_t                        Alignment6;
#endif

    /** Timeout value */
    uint64_t                        uCccTimeout;
    /** Number of completions used to assert an interrupt */
    uint32_t                        uCccNr;
    /** Current number of completed commands */
    uint32_t                        uCccCurrentNr;

    /** Register structure per port */
    AHCIPort                        ahciPort[AHCI_MAX_NR_PORTS_IMPL];

    /** Needed values for the emulated ide channels. */
    AHCIATACONTROLLER               aCts[2];

    /** The critical section. */
    PDMCRITSECT                     lock;

    /** Semaphore that gets set when fSignalIdle is set. */
    RTSEMEVENT                      hEvtIdle;
#if HC_ARCH_BITS == 32
    uint32_t                        Alignment7;
#endif

    /** Bitmask of ports which asserted an interrupt. */
    uint32_t                        u32PortsInterrupted;
    /** Device is in a reset state. */
    bool                            fReset;
    /** Supports 64bit addressing */
    bool                            f64BitAddr;
    /** GC enabled. */
    bool                            fGCEnabled;
    /** R0 enabled. */
    bool                            fR0Enabled;
    /** If the new async interface is used if available. */
    bool                            fUseAsyncInterfaceIfAvailable;
    /** Indicates that hEvtIdle should be signalled when a port is entering the
     * idle state. */
    bool volatile                   fSignalIdle;
    bool                            afAlignment8[1];

    /** Number of usable ports on this controller. */
    uint32_t                        cPortsImpl;

#if HC_ARCH_BITS == 64
    uint32_t                        Alignment9;
#endif

    /** Flag whether we have written the first 4bytes in an 8byte MMIO write successfully. */
    volatile bool                   f8ByteMMIO4BytesWrittenSuccessfully;
    /** At which number of I/O requests per second we consider having high I/O load. */
    uint32_t                        cHighIOThreshold;
    /** How many milliseconds to sleep. */
    uint32_t                        cMillisToSleep;

} AHCI, *PAHCI;

/* Scatter gather list entry. */
typedef struct
{
    /** Data Base Address. */
    uint32_t           u32DBA;
    /** Data Base Address - Upper 32-bits. */
    uint32_t           u32DBAUp;
    /** Reserved */
    uint32_t           u32Reserved;
    /** Description information. */
    uint32_t           u32DescInf;
} SGLEntry;
AssertCompileSize(SGLEntry, 16);

/** Defines for a scatter gather list entry. */
#define SGLENTRY_DBA_READONLY     ~(RT_BIT(0))
#define SGLENTRY_DESCINF_I        RT_BIT(31)
#define SGLENTRY_DESCINF_DBC      0x3fffff
#define SGLENTRY_DESCINF_READONLY 0x803fffff

/* Defines for the global host control registers for the HBA. */

#define AHCI_HBA_GLOBAL_SIZE 0x100

/* Defines for the HBA Capabilities - Readonly */
#define AHCI_HBA_CAP_S64A RT_BIT(31)
#define AHCI_HBA_CAP_SNCQ RT_BIT(30)
#define AHCI_HBA_CAP_SIS  RT_BIT(28)
#define AHCI_HBA_CAP_SSS  RT_BIT(27)
#define AHCI_HBA_CAP_SALP RT_BIT(26)
#define AHCI_HBA_CAP_SAL  RT_BIT(25)
#define AHCI_HBA_CAP_SCLO RT_BIT(24)
#define AHCI_HBA_CAP_ISS  (RT_BIT(23) | RT_BIT(22) | RT_BIT(21) | RT_BIT(20))
# define AHCI_HBA_CAP_ISS_SHIFT(x) (((x) << 20) & AHCI_HBA_CAP_ISS)
# define AHCI_HBA_CAP_ISS_GEN1 RT_BIT(0)
# define AHCI_HBA_CAP_ISS_GEN2 RT_BIT(1)
#define AHCI_HBA_CAP_SNZO RT_BIT(19)
#define AHCI_HBA_CAP_SAM  RT_BIT(18)
#define AHCI_HBA_CAP_SPM  RT_BIT(17)
#define AHCI_HBA_CAP_PMD  RT_BIT(15)
#define AHCI_HBA_CAP_SSC  RT_BIT(14)
#define AHCI_HBA_CAP_PSC  RT_BIT(13)
#define AHCI_HBA_CAP_NCS  (RT_BIT(12) | RT_BIT(11) | RT_BIT(10) | RT_BIT(9) | RT_BIT(8))
#define AHCI_HBA_CAP_NCS_SET(x) (((x-1) << 8) & AHCI_HBA_CAP_NCS) /* 0's based */
#define AHCI_HBA_CAP_CCCS  RT_BIT(7)
#define AHCI_HBA_CAP_NP   (RT_BIT(4) | RT_BIT(3) | RT_BIT(2) | RT_BIT(1) | RT_BIT(0))
#define AHCI_HBA_CAP_NP_SET(x) ((x-1) & AHCI_HBA_CAP_NP) /* 0's based */

/* Defines for the HBA Control register - Read/Write */
#define AHCI_HBA_CTRL_AE  RT_BIT(31)
#define AHCI_HBA_CTRL_IE  RT_BIT(1)
#define AHCI_HBA_CTRL_HR  RT_BIT(0)
#define AHCI_HBA_CTRL_RW_MASK (RT_BIT(0) | RT_BIT(1)) /* Mask for the used bits */

/* Defines for the HBA Version register - Readonly (We support AHCI 1.0) */
#define AHCI_HBA_VS_MJR   (1 << 16)
#define AHCI_HBA_VS_MNR   0x100

/* Defines for the command completion coalescing control register */
#define AHCI_HBA_CCC_CTL_TV 0xffff0000
#define AHCI_HBA_CCC_CTL_TV_SET(x) (x << 16)
#define AHCI_HBA_CCC_CTL_TV_GET(x) ((x & AHCI_HBA_CCC_CTL_TV) >> 16)

#define AHCI_HBA_CCC_CTL_CC 0xff00
#define AHCI_HBA_CCC_CTL_CC_SET(x) (x << 8)
#define AHCI_HBA_CCC_CTL_CC_GET(x) ((x & AHCI_HBA_CCC_CTL_CC) >> 8)

#define AHCI_HBA_CCC_CTL_INT 0xf8
#define AHCI_HBA_CCC_CTL_INT_SET(x) (x << 3)
#define AHCI_HBA_CCC_CTL_INT_GET(x) ((x & AHCI_HBA_CCC_CTL_INT) >> 3)

#define AHCI_HBA_CCC_CTL_EN  RT_BIT(0)

/* Defines for the port registers. */

#define AHCI_PORT_REGISTER_SIZE 0x80

#define AHCI_PORT_CLB_RESERVED 0xfffffc00 /* For masking out the reserved bits. */

#define AHCI_PORT_FB_RESERVED  0x7fffff00 /* For masking out the reserved bits. */

#define AHCI_PORT_IS_CPDS      RT_BIT(31)
#define AHCI_PORT_IS_TFES      RT_BIT(30)
#define AHCI_PORT_IS_HBFS      RT_BIT(29)
#define AHCI_PORT_IS_HBDS      RT_BIT(28)
#define AHCI_PORT_IS_IFS       RT_BIT(27)
#define AHCI_PORT_IS_INFS      RT_BIT(26)
#define AHCI_PORT_IS_OFS       RT_BIT(24)
#define AHCI_PORT_IS_IPMS      RT_BIT(23)
#define AHCI_PORT_IS_PRCS      RT_BIT(22)
#define AHCI_PORT_IS_DIS       RT_BIT(7)
#define AHCI_PORT_IS_PCS       RT_BIT(6)
#define AHCI_PORT_IS_DPS       RT_BIT(5)
#define AHCI_PORT_IS_UFS       RT_BIT(4)
#define AHCI_PORT_IS_SDBS      RT_BIT(3)
#define AHCI_PORT_IS_DSS       RT_BIT(2)
#define AHCI_PORT_IS_PSS       RT_BIT(1)
#define AHCI_PORT_IS_DHRS      RT_BIT(0)
#define AHCI_PORT_IS_READONLY  0xfd8000af /* Readonly mask including reserved bits. */

#define AHCI_PORT_IE_CPDE      RT_BIT(31)
#define AHCI_PORT_IE_TFEE      RT_BIT(30)
#define AHCI_PORT_IE_HBFE      RT_BIT(29)
#define AHCI_PORT_IE_HBDE      RT_BIT(28)
#define AHCI_PORT_IE_IFE       RT_BIT(27)
#define AHCI_PORT_IE_INFE      RT_BIT(26)
#define AHCI_PORT_IE_OFE       RT_BIT(24)
#define AHCI_PORT_IE_IPME      RT_BIT(23)
#define AHCI_PORT_IE_PRCE      RT_BIT(22)
#define AHCI_PORT_IE_DIE       RT_BIT(7)  /* Not supported for now, readonly. */
#define AHCI_PORT_IE_PCE       RT_BIT(6)
#define AHCI_PORT_IE_DPE       RT_BIT(5)
#define AHCI_PORT_IE_UFE       RT_BIT(4)
#define AHCI_PORT_IE_SDBE      RT_BIT(3)
#define AHCI_PORT_IE_DSE       RT_BIT(2)
#define AHCI_PORT_IE_PSE       RT_BIT(1)
#define AHCI_PORT_IE_DHRE      RT_BIT(0)
#define AHCI_PORT_IE_READONLY  (0xfdc000ff) /* Readonly mask including reserved bits. */

#define AHCI_PORT_CMD_ICC      (RT_BIT(28) | RT_BIT(29) | RT_BIT(30) | RT_BIT(31))
#define AHCI_PORT_CMD_ICC_SHIFT(x) ((x) << 28)
# define AHCI_PORT_CMD_ICC_IDLE    0x0
# define AHCI_PORT_CMD_ICC_ACTIVE  0x1
# define AHCI_PORT_CMD_ICC_PARTIAL 0x2
# define AHCI_PORT_CMD_ICC_SLUMBER 0x6
#define AHCI_PORT_CMD_ASP      RT_BIT(27) /* Not supported - Readonly */
#define AHCI_PORT_CMD_ALPE     RT_BIT(26) /* Not supported - Readonly */
#define AHCI_PORT_CMD_DLAE     RT_BIT(25)
#define AHCI_PORT_CMD_ATAPI    RT_BIT(24)
#define AHCI_PORT_CMD_CPD      RT_BIT(20)
#define AHCI_PORT_CMD_ISP      RT_BIT(19) /* Readonly */
#define AHCI_PORT_CMD_HPCP     RT_BIT(18)
#define AHCI_PORT_CMD_PMA      RT_BIT(17) /* Not supported - Readonly */
#define AHCI_PORT_CMD_CPS      RT_BIT(16)
#define AHCI_PORT_CMD_CR       RT_BIT(15) /* Readonly */
#define AHCI_PORT_CMD_FR       RT_BIT(14) /* Readonly */
#define AHCI_PORT_CMD_ISS      RT_BIT(13) /* Readonly */
#define AHCI_PORT_CMD_CCS      (RT_BIT(8) | RT_BIT(9) | RT_BIT(10) | RT_BIT(11) | RT_BIT(12))
#define AHCI_PORT_CMD_CCS_SHIFT(x) (x << 8) /* Readonly */
#define AHCI_PORT_CMD_FRE      RT_BIT(4)
#define AHCI_PORT_CMD_CLO      RT_BIT(3)
#define AHCI_PORT_CMD_POD      RT_BIT(2)
#define AHCI_PORT_CMD_SUD      RT_BIT(1)
#define AHCI_PORT_CMD_ST       RT_BIT(0)
#define AHCI_PORT_CMD_READONLY (0xff02001f & ~(AHCI_PORT_CMD_ASP | AHCI_PORT_CMD_ALPE | AHCI_PORT_CMD_PMA))

#define AHCI_PORT_SCTL_IPM         (RT_BIT(11) | RT_BIT(10) | RT_BIT(9) | RT_BIT(8))
#define AHCI_PORT_SCTL_IPM_GET(x)  ((x & AHCI_PORT_SCTL_IPM) >> 8)
#define AHCI_PORT_SCTL_SPD         (RT_BIT(7) | RT_BIT(6) | RT_BIT(5) | RT_BIT(4))
#define AHCI_PORT_SCTL_SPD_GET(x)  ((x & AHCI_PORT_SCTL_SPD) >> 4)
#define AHCI_PORT_SCTL_DET         (RT_BIT(3) | RT_BIT(2) | RT_BIT(1) | RT_BIT(0))
#define AHCI_PORT_SCTL_DET_GET(x)  (x & AHCI_PORT_SCTL_DET)
#define AHCI_PORT_SCTL_DET_NINIT   0
#define AHCI_PORT_SCTL_DET_INIT    1
#define AHCI_PORT_SCTL_DET_OFFLINE 4
#define AHCI_PORT_SCTL_READONLY    0xfff

#define AHCI_PORT_SSTS_IPM         (RT_BIT(11) | RT_BIT(10) | RT_BIT(9) | RT_BIT(8))
#define AHCI_PORT_SSTS_IPM_GET(x)  ((x & AHCI_PORT_SCTL_IPM) >> 8)
#define AHCI_PORT_SSTS_SPD         (RT_BIT(7) | RT_BIT(6) | RT_BIT(5) | RT_BIT(4))
#define AHCI_PORT_SSTS_SPD_GET(x)  ((x & AHCI_PORT_SCTL_SPD) >> 4)
#define AHCI_PORT_SSTS_DET         (RT_BIT(3) | RT_BIT(2) | RT_BIT(1) | RT_BIT(0))
#define AHCI_PORT_SSTS_DET_GET(x)  (x & AHCI_PORT_SCTL_DET)

#define AHCI_PORT_TFD_BSY          RT_BIT(7)
#define AHCI_PORT_TFD_DRQ          RT_BIT(3)
#define AHCI_PORT_TFD_ERR          RT_BIT(0)

#define AHCI_PORT_SERR_X           RT_BIT(26)
#define AHCI_PORT_SERR_W           RT_BIT(18)
#define AHCI_PORT_SERR_N           RT_BIT(16)

/* Signatures for attached storage devices. */
#define AHCI_PORT_SIG_DISK         0x00000101
#define AHCI_PORT_SIG_ATAPI        0xeb140101

/*
 * The AHCI spec defines an area of memory where the HBA posts received FIS's from the device.
 * regFB points to the base of this area.
 * Every FIS type has an offset where it is posted in this area.
 */
#define AHCI_RECFIS_DSFIS_OFFSET  0x00 /* DMA Setup FIS */
#define AHCI_RECFIS_PSFIS_OFFSET  0x20 /* PIO Setup FIS */
#define AHCI_RECFIS_RFIS_OFFSET   0x40 /* D2H Register FIS */
#define AHCI_RECFIS_SDBFIS_OFFSET 0x58 /* Set Device Bits FIS */
#define AHCI_RECFIS_UFIS_OFFSET   0x60 /* Unknown FIS type */

#define AHCI_TASK_IS_QUEUED(x) ((x) & 0x1)
#define AHCI_TASK_GET_TAG(x)   ((x) >> 1)
#define AHCI_TASK_SET(tag, queued) (((tag) << 1) | (queued))

/**
 * AHCI register operator.
 */
typedef struct ahci_opreg
{
    const char *pszName;
    int (*pfnRead )(PAHCI ahci, uint32_t iReg, uint32_t *pu32Value);
    int (*pfnWrite)(PAHCI ahci, uint32_t iReg, uint32_t u32Value);
} AHCIOPREG;

/**
 * AHCI port register operator.
 */
typedef struct pAhciPort_opreg
{
    const char *pszName;
    int (*pfnRead )(PAHCI ahci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t *pu32Value);
    int (*pfnWrite)(PAHCI ahci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t u32Value);
} AHCIPORTOPREG;

#ifndef VBOX_DEVICE_STRUCT_TESTCASE
RT_C_DECLS_BEGIN
PDMBOTHCBDECL(int) ahciMMIORead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb);
PDMBOTHCBDECL(int) ahciMMIOWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb);
static void ahciHBAReset(PAHCI pThis);
PDMBOTHCBDECL(int) ahciIOPortWrite1(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb);
PDMBOTHCBDECL(int) ahciIOPortRead1(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb);
PDMBOTHCBDECL(int) ahciIOPortWrite2(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb);
PDMBOTHCBDECL(int) ahciIOPortRead2(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb);
PDMBOTHCBDECL(int) ahciLegacyFakeWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb);
PDMBOTHCBDECL(int) ahciLegacyFakeRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb);
#ifdef IN_RING3
static int ahciPostFisIntoMemory(PAHCIPort pAhciPort, unsigned uFisType, uint8_t *cmdFis);
static void ahciPostFirstD2HFisIntoMemory(PAHCIPort pAhciPort);
static int ahciScatterGatherListCopyFromBuffer(PAHCIPORTTASKSTATE pAhciPortTaskState, void *pvBuf, size_t cbBuf);
static int ahciScatterGatherListCreate(PAHCIPort pAhciPort, PAHCIPORTTASKSTATE pAhciPortTaskState, bool fReadonly);
static int ahciScatterGatherListDestroy(PAHCIPort pAhciPort, PAHCIPORTTASKSTATE pAhciPortTaskState);
static void ahciCopyFromBufferIntoSGList(PPDMDEVINS pDevIns, PAHCIPORTTASKSTATESGENTRY pSGInfo);
static void ahciCopyFromSGListIntoBuffer(PPDMDEVINS pDevIns, PAHCIPORTTASKSTATESGENTRY pSGInfo);
static void ahciScatterGatherListGetTotalBufferSize(PAHCIPort pAhciPort, PAHCIPORTTASKSTATE pAhciPortTaskState);
static int ahciScatterGatherListCreateSafe(PAHCIPort pAhciPort, PAHCIPORTTASKSTATE pAhciPortTaskState,
                                           bool fReadonly, unsigned cSGEntriesProcessed);
#endif
RT_C_DECLS_END

#define PCIDEV_2_PAHCI(pPciDev)                  ( (PAHCI)(pPciDev) )
#define PDMIMOUNT_2_PAHCIPORT(pInterface)        ( (PAHCIPort)((uintptr_t)(pInterface) - RT_OFFSETOF(AHCIPort, IMount)) )
#define PDMIMOUNTNOTIFY_2_PAHCIPORT(pInterface)  ( (PAHCIPort)((uintptr_t)(pInterface) - RT_OFFSETOF(AHCIPort, IMountNotify)) )
#define PDMIBASE_2_PAHCIPORT(pInterface)         ( (PAHCIPort)((uintptr_t)(pInterface) - RT_OFFSETOF(AHCIPort, IBase)) )
#define PDMIBASE_2_PAHCI(pInterface)             ( (PAHCI)((uintptr_t)(pInterface) - RT_OFFSETOF(AHCI, IBase)) )
#define PDMILEDPORTS_2_PAHCI(pInterface)         ( (PAHCI)((uintptr_t)(pInterface) - RT_OFFSETOF(AHCI, ILeds)) )

#if 1
#define AHCI_RTGCPHYS_FROM_U32(Hi, Lo)             ( (RTGCPHYS)RT_MAKE_U64(Lo, Hi) )
#else
#define AHCI_RTGCPHYS_FROM_U32(Hi, Lo)             ( (RTGCPHYS)(Lo) )
#endif

#ifdef IN_RING3

# ifdef LOG_USE_C99
#  define ahciLog(a) \
     Log(("R3 P%u: %M", pAhciPort->iLUN, _LogRelRemoveParentheseis a))
# else
#  define ahciLog(a) \
     do { Log(("R3 P%u: ", pAhciPort->iLUN)); Log(a); } while(0)
# endif

#elif IN_RING0

# ifdef LOG_USE_C99
#  define ahciLog(a) \
     Log(("R0 P%u: %M", pAhciPort->iLUN, _LogRelRemoveParentheseis a))
# else
#  define ahciLog(a) \
     do { Log(("R0 P%u: ", pAhciPort->iLUN)); Log(a); } while(0)
# endif

#elif IN_RC

# ifdef LOG_USE_C99
#  define ahciLog(a) \
     Log(("GC P%u: %M", pAhciPort->iLUN, _LogRelRemoveParentheseis a))
# else
#  define ahciLog(a) \
     do { Log(("GC P%u: ", pAhciPort->iLUN)); Log(a); } while(0)
# endif

#endif

/**
 * Update PCI IRQ levels
 */
static void ahciHbaClearInterrupt(PAHCI pAhci)
{
    Log(("%s: Clearing interrupt\n", __FUNCTION__));
    PDMDevHlpPCISetIrqNoWait(pAhci->CTX_SUFF(pDevIns), 0, 0);
}

/**
 * Updates the IRQ level and sets port bit in the global interrupt status register of the HBA.
 */
static void ahciHbaSetInterrupt(PAHCI pAhci, uint8_t iPort)
{
    Log(("P%u: %s: Setting interrupt\n", iPort, __FUNCTION__));

    PDMCritSectEnter(&pAhci->lock, VINF_SUCCESS);

    if (pAhci->regHbaCtrl & AHCI_HBA_CTRL_IE)
    {
        if ((pAhci->regHbaCccCtl & AHCI_HBA_CCC_CTL_EN) && (pAhci->regHbaCccPorts & (1 << iPort)))
        {
            pAhci->uCccCurrentNr++;
            if (pAhci->uCccCurrentNr >= pAhci->uCccNr)
            {
                /* Reset command completion coalescing state. */
                TMTimerSetMillies(pAhci->CTX_SUFF(pHbaCccTimer), pAhci->uCccTimeout);
                pAhci->uCccCurrentNr = 0;

                pAhci->u32PortsInterrupted |= (1 << pAhci->uCccPortNr);
                if (!(pAhci->u32PortsInterrupted & ~(1 << pAhci->uCccPortNr)))
                {
                    Log(("P%u: %s: Fire interrupt\n", iPort, __FUNCTION__));
                    PDMDevHlpPCISetIrqNoWait(pAhci->CTX_SUFF(pDevIns), 0, 1);
                }
            }
        }
        else
        {
            /* If only the bit of the actual port is set assert an interrupt
             * because the interrupt status register was already read by the guest
             * and we need to send a new notification.
             * Otherwise an interrupt is still pending.
             */
            ASMAtomicOrU32((volatile uint32_t *)&pAhci->u32PortsInterrupted, (1 << iPort));
            if (!(pAhci->u32PortsInterrupted & ~(1 << iPort)))
            {
                Log(("P%u: %s: Fire interrupt\n", iPort, __FUNCTION__));
                PDMDevHlpPCISetIrqNoWait(pAhci->CTX_SUFF(pDevIns), 0, 1);
            }
        }
    }

    PDMCritSectLeave(&pAhci->lock);
}

#ifdef IN_RING3
/*
 * Assert irq when an CCC timeout occurs
 */
DECLCALLBACK(void) ahciCccTimer(PPDMDEVINS pDevIns, PTMTIMER pTimer, void *pvUser)
{
    PAHCI pAhci = (PAHCI)pvUser;

    ahciHbaSetInterrupt(pAhci, pAhci->uCccPortNr);
}
#endif

static int PortCmdIssue_w(PAHCI ahci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t u32Value)
{
    uint32_t uCIValue;

    ahciLog(("%s: write u32Value=%#010x\n", __FUNCTION__, u32Value));

    /* Update the CI register first. */
    uCIValue = ASMAtomicXchgU32(&pAhciPort->u32TasksFinished, 0);
    pAhciPort->regCI &= ~uCIValue;


    if ((pAhciPort->regCMD & AHCI_PORT_CMD_ST) && (u32Value > 0))
    {
        PDEVPORTNOTIFIERQUEUEITEM pItem;

        /* Mark the tasks set in the value as used. */
        for (uint8_t i = 0; i < AHCI_NR_COMMAND_SLOTS; i++)
        {
            /* Queue task if bit is set in written value and not already in progress. */
            if (((u32Value >> i) & 0x01) && !(pAhciPort->regCI & (1 << i)))
            {
                if (!pAhciPort->fAsyncInterface)
                {
                    /* Put the tag number of the task into the FIFO. */
                    uint8_t uTag = AHCI_TASK_SET(i, ((pAhciPort->regSACT & (1 << i)) ? 1 : 0));
                    ASMAtomicWriteU8(&pAhciPort->ahciIOTasks[pAhciPort->uActWritePos], uTag);
                    ahciLog(("%s: Before uActWritePos=%u\n", __FUNCTION__, pAhciPort->uActWritePos));
                    pAhciPort->uActWritePos++;
                    pAhciPort->uActWritePos %= RT_ELEMENTS(pAhciPort->ahciIOTasks);
                    ahciLog(("%s: After uActWritePos=%u\n", __FUNCTION__, pAhciPort->uActWritePos));

                    ASMAtomicIncU32(&pAhciPort->uActTasksActive);

                    bool fNotificationSend = ASMAtomicXchgBool(&pAhciPort->fNotificationSend, true);
                    if (!fNotificationSend)
                    {
                        /* Send new notification. */
                        pItem = (PDEVPORTNOTIFIERQUEUEITEM)PDMQueueAlloc(ahci->CTX_SUFF(pNotifierQueue));
                        AssertMsg(pItem, ("Allocating item for queue failed\n"));

                        pItem->iPort = pAhciPort->iLUN;
                        PDMQueueInsert(ahci->CTX_SUFF(pNotifierQueue), (PPDMQUEUEITEMCORE)pItem);
                    }
                }
                else
                {
                    pItem = (PDEVPORTNOTIFIERQUEUEITEM)PDMQueueAlloc(ahci->CTX_SUFF(pNotifierQueue));
                    AssertMsg(pItem, ("Allocating item for queue failed\n"));

                    pItem->iPort = pAhciPort->iLUN;
                    pItem->iTask = i;
                    pItem->fQueued = !!(pAhciPort->regSACT & (1 << i)); /* Mark if the task is queued. */
                    PDMQueueInsert(ahci->CTX_SUFF(pNotifierQueue), (PPDMQUEUEITEMCORE)pItem);
                }
            }
        }
    }

    pAhciPort->regCI |= u32Value;

    return VINF_SUCCESS;
}

static int PortCmdIssue_r(PAHCI ahci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t *pu32Value)
{
    uint32_t uCIValue = 0;

    uCIValue = ASMAtomicXchgU32(&pAhciPort->u32TasksFinished, 0);

    ahciLog(("%s: read regCI=%#010x uCIValue=%#010x\n", __FUNCTION__, pAhciPort->regCI, uCIValue));

    pAhciPort->regCI &= ~uCIValue;

    *pu32Value = pAhciPort->regCI;

    return VINF_SUCCESS;
}

static int PortSActive_w(PAHCI ahci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t u32Value)
{
    ahciLog(("%s: write u32Value=%#010x\n", __FUNCTION__, u32Value));

    pAhciPort->regSACT |= u32Value;

    return VINF_SUCCESS;
}

static int PortSActive_r(PAHCI ahci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t *pu32Value)
{
    uint32_t u32TasksFinished = ASMAtomicXchgU32(&pAhciPort->u32QueuedTasksFinished, 0);

    pAhciPort->regSACT &= ~u32TasksFinished;

    ahciLog(("%s: read regSACT=%#010x regCI=%#010x u32TasksFinished=%#010x\n",
             __FUNCTION__, pAhciPort->regSACT, pAhciPort->regCI, u32TasksFinished));

    *pu32Value = pAhciPort->regSACT;

    return VINF_SUCCESS;
}

static int PortSError_w(PAHCI ahci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t u32Value)
{
    ahciLog(("%s: write u32Value=%#010x\n", __FUNCTION__, u32Value));

    if (   (u32Value & AHCI_PORT_SERR_X)
        && (pAhciPort->regSERR & AHCI_PORT_SERR_X))
    {
        ASMAtomicAndU32(&pAhciPort->regIS, ~AHCI_PORT_IS_PCS);
        pAhciPort->regTFD |= ATA_STAT_ERR;
        pAhciPort->regTFD &= ~(ATA_STAT_DRQ | ATA_STAT_BUSY);
    }

    pAhciPort->regSERR &= ~u32Value;

    return VINF_SUCCESS;
}

static int PortSError_r(PAHCI ahci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t *pu32Value)
{
    ahciLog(("%s: read regSERR=%#010x\n", __FUNCTION__, pAhciPort->regSERR));
    *pu32Value = pAhciPort->regSERR;
    return VINF_SUCCESS;
}

static int PortSControl_w(PAHCI ahci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t u32Value)
{
    ahciLog(("%s: write u32Value=%#010x\n", __FUNCTION__, u32Value));
    ahciLog(("%s: IPM=%d SPD=%d DET=%d\n", __FUNCTION__,
             AHCI_PORT_SCTL_IPM_GET(u32Value), AHCI_PORT_SCTL_SPD_GET(u32Value), AHCI_PORT_SCTL_DET_GET(u32Value)));

    if ((u32Value & AHCI_PORT_SCTL_DET) == AHCI_PORT_SCTL_DET_INIT)
    {
        ASMAtomicXchgBool(&pAhciPort->fPortReset, true);
        pAhciPort->regSSTS = 0;
        pAhciPort->regSIG  = ~0;
        pAhciPort->regTFD  = 0x7f;
        pAhciPort->fFirstD2HFisSend = false;
    }
    else if ((u32Value & AHCI_PORT_SCTL_DET) == AHCI_PORT_SCTL_DET_NINIT && pAhciPort->pDrvBase &&
             (pAhciPort->regSCTL & AHCI_PORT_SCTL_DET) == AHCI_PORT_SCTL_DET_INIT)
    {
#ifndef IN_RING3
        return VINF_IOM_HC_MMIO_WRITE;
#else
        if (pAhciPort->pDrvBase)
        {
            /* Reset queue. */
            pAhciPort->uActWritePos = 0;
            pAhciPort->uActReadPos = 0;
            ASMAtomicXchgBool(&pAhciPort->fPortReset, false);

            /* Signature for SATA device. */
            if (pAhciPort->fATAPI)
                pAhciPort->regSIG = AHCI_PORT_SIG_ATAPI;
            else
                pAhciPort->regSIG = AHCI_PORT_SIG_DISK;

            pAhciPort->regSSTS = (0x01 << 8)  | /* Interface is active. */
                                 (0x03 << 0);   /* Device detected and communication established. */

            /*
             * Use the maximum allowed speed.
             * (Not that it changes anything really)
             */
            switch (AHCI_PORT_SCTL_SPD_GET(pAhciPort->regSCTL))
            {
                case 0x01:
                    pAhciPort->regSSTS |= (0x01 << 4); /* Generation 1 (1.5GBps) speed. */
                    break;
                case 0x02:
                case 0x00:
                default:
                    pAhciPort->regSSTS |= (0x02 << 4); /* Generation 2 (3.0GBps) speed. */
                    break;
            }

            /* We received a COMINIT from the device. Tell the guest. */
            ASMAtomicOrU32(&pAhciPort->regIS, AHCI_PORT_IS_PCS);
            pAhciPort->regSERR |= AHCI_PORT_SERR_X;
            pAhciPort->regTFD  |= ATA_STAT_BUSY;

            if ((pAhciPort->regCMD & AHCI_PORT_CMD_FRE) && (!pAhciPort->fFirstD2HFisSend))
            {
                ahciPostFirstD2HFisIntoMemory(pAhciPort);
                ASMAtomicOrU32(&pAhciPort->regIS, AHCI_PORT_IS_DHRS);

                if (pAhciPort->regIE & AHCI_PORT_IE_DHRE)
                    ahciHbaSetInterrupt(pAhciPort->CTX_SUFF(pAhci), pAhciPort->iLUN);
            }
       }
#endif
    }

    pAhciPort->regSCTL = u32Value;

    return VINF_SUCCESS;
}

static int PortSControl_r(PAHCI ahci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t *pu32Value)
{
    ahciLog(("%s: read regSCTL=%#010x\n", __FUNCTION__, pAhciPort->regSCTL));
    ahciLog(("%s: IPM=%d SPD=%d DET=%d\n", __FUNCTION__,
             AHCI_PORT_SCTL_IPM_GET(pAhciPort->regSCTL), AHCI_PORT_SCTL_SPD_GET(pAhciPort->regSCTL),
             AHCI_PORT_SCTL_DET_GET(pAhciPort->regSCTL)));

    *pu32Value = pAhciPort->regSCTL;
    return VINF_SUCCESS;
}

static int PortSStatus_r(PAHCI ahci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t *pu32Value)
{
    ahciLog(("%s: read regSSTS=%#010x\n", __FUNCTION__, pAhciPort->regSSTS));
    ahciLog(("%s: IPM=%d SPD=%d DET=%d\n", __FUNCTION__,
             AHCI_PORT_SSTS_IPM_GET(pAhciPort->regSSTS), AHCI_PORT_SSTS_SPD_GET(pAhciPort->regSSTS),
             AHCI_PORT_SSTS_DET_GET(pAhciPort->regSSTS)));

    *pu32Value = pAhciPort->regSSTS;
    return VINF_SUCCESS;
}

static int PortSignature_r(PAHCI ahci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t *pu32Value)
{
    ahciLog(("%s: read regSIG=%#010x\n", __FUNCTION__, pAhciPort->regSIG));
    *pu32Value = pAhciPort->regSIG;
    return VINF_SUCCESS;
}

static int PortTaskFileData_r(PAHCI ahci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t *pu32Value)
{
    ahciLog(("%s: read regTFD=%#010x\n", __FUNCTION__, pAhciPort->regTFD));
    ahciLog(("%s: ERR=%x BSY=%d DRQ=%d ERR=%d\n", __FUNCTION__,
             (pAhciPort->regTFD >> 8), (pAhciPort->regTFD & AHCI_PORT_TFD_BSY) >> 7,
             (pAhciPort->regTFD & AHCI_PORT_TFD_DRQ) >> 3, (pAhciPort->regTFD & AHCI_PORT_TFD_ERR)));
    *pu32Value = pAhciPort->regTFD;
    return VINF_SUCCESS;
}

/**
 * Read from the port command register.
 */
static int PortCmd_r(PAHCI ahci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t *pu32Value)
{
    ahciLog(("%s: read regCMD=%#010x\n", __FUNCTION__, pAhciPort->regCMD));
    ahciLog(("%s: ICC=%d ASP=%d ALPE=%d DLAE=%d ATAPI=%d CPD=%d ISP=%d HPCP=%d PMA=%d CPS=%d CR=%d FR=%d ISS=%d CCS=%d FRE=%d CLO=%d POD=%d SUD=%d ST=%d\n",
             __FUNCTION__, (pAhciPort->regCMD & AHCI_PORT_CMD_ICC) >> 28, (pAhciPort->regCMD & AHCI_PORT_CMD_ASP) >> 27,
             (pAhciPort->regCMD & AHCI_PORT_CMD_ALPE) >> 26, (pAhciPort->regCMD & AHCI_PORT_CMD_DLAE) >> 25,
             (pAhciPort->regCMD & AHCI_PORT_CMD_ATAPI) >> 24, (pAhciPort->regCMD & AHCI_PORT_CMD_CPD) >> 20,
             (pAhciPort->regCMD & AHCI_PORT_CMD_ISP) >> 19, (pAhciPort->regCMD & AHCI_PORT_CMD_HPCP) >> 18,
             (pAhciPort->regCMD & AHCI_PORT_CMD_PMA) >> 17, (pAhciPort->regCMD & AHCI_PORT_CMD_CPS) >> 16,
             (pAhciPort->regCMD & AHCI_PORT_CMD_CR) >> 15, (pAhciPort->regCMD & AHCI_PORT_CMD_FR) >> 14,
             (pAhciPort->regCMD & AHCI_PORT_CMD_ISS) >> 13, (pAhciPort->regCMD & AHCI_PORT_CMD_CCS) >> 8,
             (pAhciPort->regCMD & AHCI_PORT_CMD_FRE) >> 4, (pAhciPort->regCMD & AHCI_PORT_CMD_CLO) >> 3,
             (pAhciPort->regCMD & AHCI_PORT_CMD_POD) >> 2, (pAhciPort->regCMD & AHCI_PORT_CMD_SUD) >> 1,
             (pAhciPort->regCMD & AHCI_PORT_CMD_ST)));
    *pu32Value = pAhciPort->regCMD;
    return VINF_SUCCESS;
}

/**
 * Write to the port command register.
 * This is the register where all the data transfer is started
 */
static int PortCmd_w(PAHCI ahci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t u32Value)
{
    int rc = VINF_SUCCESS;
    ahciLog(("%s: write u32Value=%#010x\n", __FUNCTION__, u32Value));
    ahciLog(("%s: ICC=%d ASP=%d ALPE=%d DLAE=%d ATAPI=%d CPD=%d ISP=%d HPCP=%d PMA=%d CPS=%d CR=%d FR=%d ISS=%d CCS=%d FRE=%d CLO=%d POD=%d SUD=%d ST=%d\n",
             __FUNCTION__, (u32Value & AHCI_PORT_CMD_ICC) >> 28, (u32Value & AHCI_PORT_CMD_ASP) >> 27,
             (u32Value & AHCI_PORT_CMD_ALPE) >> 26, (u32Value & AHCI_PORT_CMD_DLAE) >> 25,
             (u32Value & AHCI_PORT_CMD_ATAPI) >> 24, (u32Value & AHCI_PORT_CMD_CPD) >> 20,
             (u32Value & AHCI_PORT_CMD_ISP) >> 19, (u32Value & AHCI_PORT_CMD_HPCP) >> 18,
             (u32Value & AHCI_PORT_CMD_PMA) >> 17, (u32Value & AHCI_PORT_CMD_CPS) >> 16,
             (u32Value & AHCI_PORT_CMD_CR) >> 15, (u32Value & AHCI_PORT_CMD_FR) >> 14,
             (u32Value & AHCI_PORT_CMD_ISS) >> 13, (u32Value & AHCI_PORT_CMD_CCS) >> 8,
             (u32Value & AHCI_PORT_CMD_FRE) >> 4, (u32Value & AHCI_PORT_CMD_CLO) >> 3,
             (u32Value & AHCI_PORT_CMD_POD) >> 2, (u32Value & AHCI_PORT_CMD_SUD) >> 1,
             (u32Value & AHCI_PORT_CMD_ST)));

    if (pAhciPort->fPoweredOn && pAhciPort->fSpunUp)
    {
        if (u32Value & AHCI_PORT_CMD_CLO)
        {
            ahciLog(("%s: Command list override requested\n", __FUNCTION__));
            u32Value &= ~(AHCI_PORT_TFD_BSY | AHCI_PORT_TFD_DRQ);
            /* Clear the CLO bit. */
            u32Value &= ~(AHCI_PORT_CMD_CLO);
        }

        if (u32Value & AHCI_PORT_CMD_ST)
        {
            ahciLog(("%s: Engine starts\n", __FUNCTION__));

            /** Set engine state to running. */
            u32Value |= AHCI_PORT_CMD_CR;
        }
        else
        {
            ahciLog(("%s: Engine stops\n", __FUNCTION__));
            /* Clear command issue register. */
            pAhciPort->regCI = 0;
            /** Clear current command slot. */
            u32Value &= ~(AHCI_PORT_CMD_CCS_SHIFT(0xff));
            u32Value &= ~AHCI_PORT_CMD_CR;
        }
    }
    else if (pAhciPort->pDrvBase)
    {
        if ((u32Value & AHCI_PORT_CMD_POD) && (pAhciPort->regCMD & AHCI_PORT_CMD_CPS) && !pAhciPort->fPoweredOn)
        {
            ahciLog(("%s: Power on the device\n", __FUNCTION__));
            pAhciPort->fPoweredOn = true;

            /*
             * Set states in the Port Signature and SStatus registers.
             */
            pAhciPort->regSIG  = 0x101; /* Signature for SATA device. */
            pAhciPort->regSSTS = (0x01 << 8) | /* Interface is active. */
                                 (0x02 << 4) | /* Generation 2 (3.0GBps) speed. */
                                 (0x03 << 0);  /* Device detected and communication established. */

            if (pAhciPort->regCMD & AHCI_PORT_CMD_FRE)
            {
#ifndef IN_RING3
                return VINF_IOM_HC_MMIO_WRITE;
#else
                ahciPostFirstD2HFisIntoMemory(pAhciPort);
                ASMAtomicOrU32(&pAhciPort->regIS, AHCI_PORT_IS_DHRS);

                if (pAhciPort->regIE & AHCI_PORT_IE_DHRE)
                    ahciHbaSetInterrupt(pAhciPort->CTX_SUFF(pAhci), pAhciPort->iLUN);
#endif
            }
        }

        if ((u32Value & AHCI_PORT_CMD_SUD) && pAhciPort->fPoweredOn && !pAhciPort->fSpunUp)
        {
            ahciLog(("%s: Spin up the device\n", __FUNCTION__));
            pAhciPort->fSpunUp = true;
        }
    }

    if (u32Value & AHCI_PORT_CMD_FRE)
    {
        ahciLog(("%s: FIS receive enabled\n", __FUNCTION__));

        u32Value |= AHCI_PORT_CMD_FR;

        /* Send the first D2H FIS only if it wasn't already send. */
        if (!pAhciPort->fFirstD2HFisSend)
        {
#ifndef IN_RING3
            return VINF_IOM_HC_MMIO_WRITE;
#else
            ahciPostFirstD2HFisIntoMemory(pAhciPort);
            pAhciPort->fFirstD2HFisSend = true;
#endif
        }
    }
    else if (!(u32Value & AHCI_PORT_CMD_FRE))
    {
        ahciLog(("%s: FIS receive disabled\n", __FUNCTION__));
        u32Value &= ~AHCI_PORT_CMD_FR;
    }

    pAhciPort->regCMD = u32Value;

    return rc;
}

/**
 * Read from the port interrupt enable register.
 */
static int PortIntrEnable_r(PAHCI ahci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t *pu32Value)
{
    ahciLog(("%s: read regIE=%#010x\n", __FUNCTION__, pAhciPort->regIE));
    ahciLog(("%s: CPDE=%d TFEE=%d HBFE=%d HBDE=%d IFE=%d INFE=%d OFE=%d IPME=%d PRCE=%d DIE=%d PCE=%d DPE=%d UFE=%d SDBE=%d DSE=%d PSE=%d DHRE=%d\n",
             __FUNCTION__, (pAhciPort->regIE & AHCI_PORT_IE_CPDE) >> 31, (pAhciPort->regIE & AHCI_PORT_IE_TFEE) >> 30,
             (pAhciPort->regIE & AHCI_PORT_IE_HBFE) >> 29, (pAhciPort->regIE & AHCI_PORT_IE_HBDE) >> 28,
             (pAhciPort->regIE & AHCI_PORT_IE_IFE) >> 27, (pAhciPort->regIE & AHCI_PORT_IE_INFE) >> 26,
             (pAhciPort->regIE & AHCI_PORT_IE_OFE) >> 24, (pAhciPort->regIE & AHCI_PORT_IE_IPME) >> 23,
             (pAhciPort->regIE & AHCI_PORT_IE_PRCE) >> 22, (pAhciPort->regIE & AHCI_PORT_IE_DIE) >> 7,
             (pAhciPort->regIE & AHCI_PORT_IE_PCE) >> 6, (pAhciPort->regIE & AHCI_PORT_IE_DPE) >> 5,
             (pAhciPort->regIE & AHCI_PORT_IE_UFE) >> 4, (pAhciPort->regIE & AHCI_PORT_IE_SDBE) >> 3,
             (pAhciPort->regIE & AHCI_PORT_IE_DSE) >> 2, (pAhciPort->regIE & AHCI_PORT_IE_PSE) >> 1,
             (pAhciPort->regIE & AHCI_PORT_IE_DHRE)));
    *pu32Value = pAhciPort->regIE;
    return VINF_SUCCESS;
}

/**
 * Write to the port interrupt enable register.
 */
static int PortIntrEnable_w(PAHCI ahci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t u32Value)
{
    ahciLog(("%s: write u32Value=%#010x\n", __FUNCTION__, u32Value));
    ahciLog(("%s: CPDE=%d TFEE=%d HBFE=%d HBDE=%d IFE=%d INFE=%d OFE=%d IPME=%d PRCE=%d DIE=%d PCE=%d DPE=%d UFE=%d SDBE=%d DSE=%d PSE=%d DHRE=%d\n",
             __FUNCTION__, (u32Value & AHCI_PORT_IE_CPDE) >> 31, (u32Value & AHCI_PORT_IE_TFEE) >> 30,
             (u32Value & AHCI_PORT_IE_HBFE) >> 29, (u32Value & AHCI_PORT_IE_HBDE) >> 28,
             (u32Value & AHCI_PORT_IE_IFE) >> 27, (u32Value & AHCI_PORT_IE_INFE) >> 26,
             (u32Value & AHCI_PORT_IE_OFE) >> 24, (u32Value & AHCI_PORT_IE_IPME) >> 23,
             (u32Value & AHCI_PORT_IE_PRCE) >> 22, (u32Value & AHCI_PORT_IE_DIE) >> 7,
             (u32Value & AHCI_PORT_IE_PCE) >> 6, (u32Value & AHCI_PORT_IE_DPE) >> 5,
             (u32Value & AHCI_PORT_IE_UFE) >> 4, (u32Value & AHCI_PORT_IE_SDBE) >> 3,
             (u32Value & AHCI_PORT_IE_DSE) >> 2, (u32Value & AHCI_PORT_IE_PSE) >> 1,
             (u32Value & AHCI_PORT_IE_DHRE)));

    pAhciPort->regIE = (u32Value & AHCI_PORT_IE_READONLY);

    /* Check if some a interrupt status bit changed*/
    uint32_t u32IntrStatus = ASMAtomicReadU32(&pAhciPort->regIS);

    if (pAhciPort->regIE & u32IntrStatus)
        ahciHbaSetInterrupt(ahci, pAhciPort->iLUN);

    return VINF_SUCCESS;
}

/**
 * Read from the port interrupt status register.
 */
static int PortIntrSts_r(PAHCI ahci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t *pu32Value)
{
    ahciLog(("%s: read regIS=%#010x\n", __FUNCTION__, pAhciPort->regIS));
    ahciLog(("%s: CPDS=%d TFES=%d HBFS=%d HBDS=%d IFS=%d INFS=%d OFS=%d IPMS=%d PRCS=%d DIS=%d PCS=%d DPS=%d UFS=%d SDBS=%d DSS=%d PSS=%d DHRS=%d\n",
             __FUNCTION__, (pAhciPort->regIS & AHCI_PORT_IS_CPDS) >> 31, (pAhciPort->regIS & AHCI_PORT_IS_TFES) >> 30,
             (pAhciPort->regIS & AHCI_PORT_IS_HBFS) >> 29, (pAhciPort->regIS & AHCI_PORT_IS_HBDS) >> 28,
             (pAhciPort->regIS & AHCI_PORT_IS_IFS) >> 27, (pAhciPort->regIS & AHCI_PORT_IS_INFS) >> 26,
             (pAhciPort->regIS & AHCI_PORT_IS_OFS) >> 24, (pAhciPort->regIS & AHCI_PORT_IS_IPMS) >> 23,
             (pAhciPort->regIS & AHCI_PORT_IS_PRCS) >> 22, (pAhciPort->regIS & AHCI_PORT_IS_DIS) >> 7,
             (pAhciPort->regIS & AHCI_PORT_IS_PCS) >> 6, (pAhciPort->regIS & AHCI_PORT_IS_DPS) >> 5,
             (pAhciPort->regIS & AHCI_PORT_IS_UFS) >> 4, (pAhciPort->regIS & AHCI_PORT_IS_SDBS) >> 3,
             (pAhciPort->regIS & AHCI_PORT_IS_DSS) >> 2, (pAhciPort->regIS & AHCI_PORT_IS_PSS) >> 1,
             (pAhciPort->regIS & AHCI_PORT_IS_DHRS)));
    *pu32Value = pAhciPort->regIS;
    return VINF_SUCCESS;
}

/**
 * Write to the port interrupt status register.
 */
static int PortIntrSts_w(PAHCI ahci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t u32Value)
{
    ahciLog(("%s: write u32Value=%#010x\n", __FUNCTION__, u32Value));
    ASMAtomicAndU32(&pAhciPort->regIS, ~(u32Value & AHCI_PORT_IS_READONLY));

    return VINF_SUCCESS;
}

/**
 * Read from the port FIS base address upper 32bit register.
 */
static int PortFisAddrUp_r(PAHCI ahci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t *pu32Value)
{
    ahciLog(("%s: read regFBU=%#010x\n", __FUNCTION__, pAhciPort->regFBU));
    *pu32Value = pAhciPort->regFBU;
    return VINF_SUCCESS;
}

/**
 * Write to the port FIS base address upper 32bit register.
 */
static int PortFisAddrUp_w(PAHCI ahci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t u32Value)
{
    ahciLog(("%s: write u32Value=%#010x\n", __FUNCTION__, u32Value));

    pAhciPort->regFBU = u32Value;
    pAhciPort->GCPhysAddrFb = AHCI_RTGCPHYS_FROM_U32(pAhciPort->regFBU, pAhciPort->regFB);

    return VINF_SUCCESS;
}

/**
 * Read from the port FIS base address register.
 */
static int PortFisAddr_r(PAHCI ahci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t *pu32Value)
{
    ahciLog(("%s: read regFB=%#010x\n", __FUNCTION__, pAhciPort->regFB));
    *pu32Value = pAhciPort->regFB;
    return VINF_SUCCESS;
}

/**
 * Write to the port FIS base address register.
 */
static int PortFisAddr_w(PAHCI ahci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t u32Value)
{
    ahciLog(("%s: write u32Value=%#010x\n", __FUNCTION__, u32Value));

    pAhciPort->regFB = (u32Value & AHCI_PORT_FB_RESERVED);
    pAhciPort->GCPhysAddrFb = AHCI_RTGCPHYS_FROM_U32(pAhciPort->regFBU, pAhciPort->regFB);

    return VINF_SUCCESS;
}

/**
 * Write to the port command list base address upper 32bit register.
 */
static int PortCmdLstAddrUp_w(PAHCI ahci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t u32Value)
{
    ahciLog(("%s: write u32Value=%#010x\n", __FUNCTION__, u32Value));

    pAhciPort->regCLBU = u32Value;
    pAhciPort->GCPhysAddrClb = AHCI_RTGCPHYS_FROM_U32(pAhciPort->regCLBU, pAhciPort->regCLB);

    return VINF_SUCCESS;
}

/**
 * Read from the port command list base address upper 32bit register.
 */
static int PortCmdLstAddrUp_r(PAHCI ahci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t *pu32Value)
{
    ahciLog(("%s: read regCLBU=%#010x\n", __FUNCTION__, pAhciPort->regCLBU));
    *pu32Value = pAhciPort->regCLBU;
    return VINF_SUCCESS;
}

/**
 * Read from the port command list base address register.
 */
static int PortCmdLstAddr_r(PAHCI ahci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t *pu32Value)
{
    ahciLog(("%s: read regCLB=%#010x\n", __FUNCTION__, pAhciPort->regCLB));
    *pu32Value = pAhciPort->regCLB;
    return VINF_SUCCESS;
}

/**
 * Write to the port command list base address register.
 */
static int PortCmdLstAddr_w(PAHCI ahci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t u32Value)
{
    ahciLog(("%s: write u32Value=%#010x\n", __FUNCTION__, u32Value));

    pAhciPort->regCLB = (u32Value & AHCI_PORT_CLB_RESERVED);
    pAhciPort->GCPhysAddrClb = AHCI_RTGCPHYS_FROM_U32(pAhciPort->regCLBU, pAhciPort->regCLB);

    return VINF_SUCCESS;
}

/**
 * Read from the global Version register.
 */
static int HbaVersion_r(PAHCI ahci, uint32_t iReg, uint32_t *pu32Value)
{
    Log(("%s: read regHbaVs=%#010x\n", __FUNCTION__, ahci->regHbaVs));
    *pu32Value = ahci->regHbaVs;
    return VINF_SUCCESS;
}

/**
 * Read from the global Ports implemented register.
 */
static int HbaPortsImplemented_r(PAHCI ahci, uint32_t iReg, uint32_t *pu32Value)
{
    Log(("%s: read regHbaPi=%#010x\n", __FUNCTION__, ahci->regHbaPi));
    *pu32Value = ahci->regHbaPi;
    return VINF_SUCCESS;
}

/**
 * Write to the global interrupt status register.
 */
static int HbaInterruptStatus_w(PAHCI ahci, uint32_t iReg, uint32_t u32Value)
{
    int rc;
    Log(("%s: write u32Value=%#010x\n", __FUNCTION__, u32Value));

    rc = PDMCritSectEnter(&ahci->lock, VINF_IOM_HC_MMIO_WRITE);
    if (rc != VINF_SUCCESS)
        return rc;

    if (u32Value > 0)
    {
        /*
         * Clear the interrupt only if no port has signalled
         * an interrupt and the guest has cleared all set interrupt
         * notification bits.
         */
        bool fClear = true;

        ahci->regHbaIs &= ~(u32Value);

        fClear = (!ahci->u32PortsInterrupted) && (!ahci->regHbaIs);
        if (fClear)
        {
            unsigned i = 0;

            /* Check if the cleared ports have a interrupt status bit set. */
            while ((u32Value > 0) && (i < AHCI_MAX_NR_PORTS_IMPL))
            {
                if (u32Value & 0x01)
                {
                    PAHCIPort pAhciPort = &ahci->ahciPort[i];

                    if (pAhciPort->regIE & pAhciPort->regIS)
                    {
                        Log(("%s: Interrupt status of port %u set -> Set interrupt again\n", __FUNCTION__, i));
                        ASMAtomicOrU32(&ahci->u32PortsInterrupted, 1 << i);
                        fClear = false;
                        break;
                    }
                }
                u32Value = u32Value >> 1;
                i++;
            }
        }

        if (fClear)
            ahciHbaClearInterrupt(ahci);
        else
        {
            Log(("%s: Not clearing interrupt: u32PortsInterrupted=%#010x\n", __FUNCTION__, ahci->u32PortsInterrupted));
            /*
             * We need to set the interrupt again because the I/O APIC does not set it again even if the
             * line is still high.
             * We need to clear it first because the PCI bus only calls the interrupt controller if the state changes.
             */
            PDMDevHlpPCISetIrqNoWait(ahci->CTX_SUFF(pDevIns), 0, 0);
            PDMDevHlpPCISetIrqNoWait(ahci->CTX_SUFF(pDevIns), 0, 1);
        }
    }

    PDMCritSectLeave(&ahci->lock);
    return VINF_SUCCESS;
}

/**
 * Read from the global interrupt status register.
 */
static int HbaInterruptStatus_r(PAHCI ahci, uint32_t iReg, uint32_t *pu32Value)
{
    uint32_t u32PortsInterrupted;
    int rc;

    rc = PDMCritSectEnter(&ahci->lock, VINF_IOM_HC_MMIO_READ);
    if (rc != VINF_SUCCESS)
        return rc;

    u32PortsInterrupted = ASMAtomicXchgU32(&ahci->u32PortsInterrupted, 0);

    PDMCritSectLeave(&ahci->lock);
    Log(("%s: read regHbaIs=%#010x u32PortsInterrupted=%#010x\n", __FUNCTION__, ahci->regHbaIs, u32PortsInterrupted));

    ahci->regHbaIs |= u32PortsInterrupted;

#ifdef LOG_ENABLED
    Log(("%s:", __FUNCTION__));
    unsigned i;
    for (i = 0; i < ahci->cPortsImpl; i++)
    {
        if ((ahci->regHbaIs >> i) & 0x01)
            Log((" P%d", i));
    }
    Log(("\n"));
#endif

    *pu32Value = ahci->regHbaIs;

    return VINF_SUCCESS;
}

/**
 * Write to the global control register.
 */
static int HbaControl_w(PAHCI ahci, uint32_t iReg, uint32_t u32Value)
{
    Log(("%s: write u32Value=%#010x\n"
         "%s: AE=%d IE=%d HR=%d\n",
         __FUNCTION__, u32Value,
         __FUNCTION__, (u32Value & AHCI_HBA_CTRL_AE) >> 31, (u32Value & AHCI_HBA_CTRL_IE) >> 1,
         (u32Value & AHCI_HBA_CTRL_HR)));

    ahci->regHbaCtrl = (u32Value & AHCI_HBA_CTRL_RW_MASK) | AHCI_HBA_CTRL_AE;
    if (ahci->regHbaCtrl & AHCI_HBA_CTRL_HR)
        ahciHBAReset(ahci);
    return VINF_SUCCESS;
}

/**
 * Read the global control register.
 */
static int HbaControl_r(PAHCI ahci, uint32_t iReg, uint32_t *pu32Value)
{
    Log(("%s: read regHbaCtrl=%#010x\n"
         "%s: AE=%d IE=%d HR=%d\n",
         __FUNCTION__, ahci->regHbaCtrl,
         __FUNCTION__, (ahci->regHbaCtrl & AHCI_HBA_CTRL_AE) >> 31, (ahci->regHbaCtrl & AHCI_HBA_CTRL_IE) >> 1,
         (ahci->regHbaCtrl & AHCI_HBA_CTRL_HR)));
    *pu32Value = ahci->regHbaCtrl;
    return VINF_SUCCESS;
}

/**
 * Read the global capabilities register.
 */
static int HbaCapabilities_r(PAHCI ahci, uint32_t iReg, uint32_t *pu32Value)
{
    Log(("%s: read regHbaCap=%#010x\n"
         "%s: S64A=%d SNCQ=%d SIS=%d SSS=%d SALP=%d SAL=%d SCLO=%d ISS=%d SNZO=%d SAM=%d SPM=%d PMD=%d SSC=%d PSC=%d NCS=%d NP=%d\n",
          __FUNCTION__, ahci->regHbaCap,
          __FUNCTION__, (ahci->regHbaCap & AHCI_HBA_CAP_S64A) >> 31, (ahci->regHbaCap & AHCI_HBA_CAP_SNCQ) >> 30,
          (ahci->regHbaCap & AHCI_HBA_CAP_SIS) >> 28, (ahci->regHbaCap & AHCI_HBA_CAP_SSS) >> 27,
          (ahci->regHbaCap & AHCI_HBA_CAP_SALP) >> 26, (ahci->regHbaCap & AHCI_HBA_CAP_SAL) >> 25,
          (ahci->regHbaCap & AHCI_HBA_CAP_SCLO) >> 24, (ahci->regHbaCap & AHCI_HBA_CAP_ISS) >> 20,
          (ahci->regHbaCap & AHCI_HBA_CAP_SNZO) >> 19, (ahci->regHbaCap & AHCI_HBA_CAP_SAM) >> 18,
          (ahci->regHbaCap & AHCI_HBA_CAP_SPM) >> 17, (ahci->regHbaCap & AHCI_HBA_CAP_PMD) >> 15,
          (ahci->regHbaCap & AHCI_HBA_CAP_SSC) >> 14, (ahci->regHbaCap & AHCI_HBA_CAP_PSC) >> 13,
          (ahci->regHbaCap & AHCI_HBA_CAP_NCS) >> 8, (ahci->regHbaCap & AHCI_HBA_CAP_NP)));
    *pu32Value = ahci->regHbaCap;
    return VINF_SUCCESS;
}

/**
 * Write to the global command completion coalescing control register.
 */
static int HbaCccCtl_w(PAHCI ahci, uint32_t iReg, uint32_t u32Value)
{
    Log(("%s: write u32Value=%#010x\n"
         "%s: TV=%d CC=%d INT=%d EN=%d\n",
         __FUNCTION__, u32Value,
         __FUNCTION__, AHCI_HBA_CCC_CTL_TV_GET(u32Value), AHCI_HBA_CCC_CTL_CC_GET(u32Value),
         AHCI_HBA_CCC_CTL_INT_GET(u32Value), (u32Value & AHCI_HBA_CCC_CTL_EN)));

    ahci->regHbaCccCtl = u32Value;
    ahci->uCccTimeout  = AHCI_HBA_CCC_CTL_TV_GET(u32Value);
    ahci->uCccPortNr   = AHCI_HBA_CCC_CTL_INT_GET(u32Value);
    ahci->uCccNr       = AHCI_HBA_CCC_CTL_CC_GET(u32Value);

    if (u32Value & AHCI_HBA_CCC_CTL_EN)
    {
        /* Arm the timer */
        TMTimerSetMillies(ahci->CTX_SUFF(pHbaCccTimer), ahci->uCccTimeout);
    }
    else
    {
        TMTimerStop(ahci->CTX_SUFF(pHbaCccTimer));
    }

    return VINF_SUCCESS;
}

/**
 * Read the global command completion coalescing control register.
 */
static int HbaCccCtl_r(PAHCI ahci, uint32_t iReg, uint32_t *pu32Value)
{
    Log(("%s: read regHbaCccCtl=%#010x\n"
         "%s: TV=%d CC=%d INT=%d EN=%d\n",
         __FUNCTION__, ahci->regHbaCccCtl,
         __FUNCTION__, AHCI_HBA_CCC_CTL_TV_GET(ahci->regHbaCccCtl), AHCI_HBA_CCC_CTL_CC_GET(ahci->regHbaCccCtl),
         AHCI_HBA_CCC_CTL_INT_GET(ahci->regHbaCccCtl), (ahci->regHbaCccCtl & AHCI_HBA_CCC_CTL_EN)));
    *pu32Value = ahci->regHbaCccCtl;
    return VINF_SUCCESS;
}

/**
 * Write to the global command completion coalescing ports register.
 */
static int HbaCccPorts_w(PAHCI ahci, uint32_t iReg, uint32_t u32Value)
{
    Log(("%s: write u32Value=%#010x\n", __FUNCTION__, u32Value));

    ahci->regHbaCccPorts = u32Value;

    return VINF_SUCCESS;
}

/**
 * Read the global command completion coalescing ports register.
 */
static int HbaCccPorts_r(PAHCI ahci, uint32_t iReg, uint32_t *pu32Value)
{
    Log(("%s: read regHbaCccPorts=%#010x\n", __FUNCTION__, ahci->regHbaCccPorts));

#ifdef LOG_ENABLED
    Log(("%s:", __FUNCTION__));
    unsigned i;
    for (i = 0; i < ahci->cPortsImpl; i++)
    {
        if ((ahci->regHbaCccPorts >> i) & 0x01)
            Log((" P%d", i));
    }
    Log(("\n"));
#endif

    *pu32Value = ahci->regHbaCccPorts;
    return VINF_SUCCESS;
}

/**
 * Invalid write to global register
 */
static int HbaInvalid_w(PAHCI ahci, uint32_t iReg, uint32_t u32Value)
{
    Log(("%s: Write denied!!! iReg=%u u32Value=%#010x\n", __FUNCTION__, iReg, u32Value));
    return VINF_SUCCESS;
}

/**
 * Invalid Port write.
 */
static int PortInvalid_w(PAHCI ahci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t u32Value)
{
    ahciLog(("%s: Write denied!!! iReg=%u u32Value=%#010x\n", __FUNCTION__, iReg, u32Value));
    return VINF_SUCCESS;
}

/**
 * Invalid Port read.
 */
static int PortInvalid_r(PAHCI ahci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t *pu32Value)
{
    ahciLog(("%s: Read denied!!! iReg=%u\n", __FUNCTION__, iReg));
    return VINF_SUCCESS;
}

/**
 * Register descriptor table for global HBA registers
 */
static const AHCIOPREG g_aOpRegs[] =
{
    {"HbaCapabilites",      HbaCapabilities_r,     HbaInvalid_w}, /* Readonly */
    {"HbaControl"    ,      HbaControl_r,          HbaControl_w},
    {"HbaInterruptStatus",  HbaInterruptStatus_r,  HbaInterruptStatus_w},
    {"HbaPortsImplemented", HbaPortsImplemented_r, HbaInvalid_w}, /* Readonly */
    {"HbaVersion",          HbaVersion_r,          HbaInvalid_w}, /* ReadOnly */
    {"HbaCccCtl",           HbaCccCtl_r,           HbaCccCtl_w},
    {"HbaCccPorts",         HbaCccPorts_r,         HbaCccPorts_w},
};

/**
 * Register descriptor table for port registers
 */
static const AHCIPORTOPREG g_aPortOpRegs[] =
{
    {"PortCmdLstAddr",   PortCmdLstAddr_r,   PortCmdLstAddr_w},
    {"PortCmdLstAddrUp", PortCmdLstAddrUp_r, PortCmdLstAddrUp_w},
    {"PortFisAddr",      PortFisAddr_r,      PortFisAddr_w},
    {"PortFisAddrUp",    PortFisAddrUp_r,    PortFisAddrUp_w},
    {"PortIntrSts",      PortIntrSts_r,      PortIntrSts_w},
    {"PortIntrEnable",   PortIntrEnable_r,   PortIntrEnable_w},
    {"PortCmd",          PortCmd_r,          PortCmd_w},
    {"PortReserved1",    PortInvalid_r,      PortInvalid_w}, /* Not used. */
    {"PortTaskFileData", PortTaskFileData_r, PortInvalid_w}, /* Readonly */
    {"PortSignature",    PortSignature_r,    PortInvalid_w}, /* Readonly */
    {"PortSStatus",      PortSStatus_r,      PortInvalid_w}, /* Readonly */
    {"PortSControl",     PortSControl_r,     PortSControl_w},
    {"PortSError",       PortSError_r,       PortSError_w},
    {"PortSActive",      PortSActive_r,      PortSActive_w},
    {"PortCmdIssue",     PortCmdIssue_r,     PortCmdIssue_w},
    {"PortReserved2",    PortInvalid_r,      PortInvalid_w}, /* Not used. */
};

/**
 * Reset initiated by system software for one port.
 *
 * @param pAhciPort     The port to reset.
 */
static void ahciPortSwReset(PAHCIPort pAhciPort)
{
    pAhciPort->regIS   = 0;
    pAhciPort->regIE   = 0;
    pAhciPort->regCMD  = AHCI_PORT_CMD_CPD  | /* Cold presence detection */
                         AHCI_PORT_CMD_SUD  | /* Device has spun up. */
                         AHCI_PORT_CMD_POD;   /* Port is powered on. */
    pAhciPort->regTFD  = (1 << 8) | ATA_STAT_SEEK | ATA_STAT_WRERR;
    pAhciPort->regSIG  = ~0;
    pAhciPort->regSSTS = 0;
    pAhciPort->regSCTL = 0;
    pAhciPort->regSERR = 0;
    pAhciPort->regSACT = 0;
    pAhciPort->regCI   = 0;

    pAhciPort->fResetDevice      = false;
    pAhciPort->fPoweredOn        = true;
    pAhciPort->fSpunUp           = true;
    pAhciPort->fNotificationSend = false;
    pAhciPort->cMultSectors = ATA_MAX_MULT_SECTORS;
    pAhciPort->uATATransferMode = ATA_MODE_UDMA | 6;

    pAhciPort->u32TasksFinished = 0;
    pAhciPort->u32QueuedTasksFinished = 0;

    pAhciPort->uActWritePos = 0;
    pAhciPort->uActReadPos = 0;
    pAhciPort->uActTasksActive = 0;

    if (pAhciPort->pDrvBase)
    {
        pAhciPort->regCMD |= AHCI_PORT_CMD_CPS; /* Indicate that there is a device on that port */

        if (pAhciPort->fPoweredOn)
        {
            /*
             * Set states in the Port Signature and SStatus registers.
             */
            pAhciPort->regSIG  = 0x101; /* Signature for SATA device. */
            pAhciPort->regSSTS = (0x01 << 8) | /* Interface is active. */
                                 (0x02 << 4) | /* Generation 2 (3.0GBps) speed. */
                                 (0x03 << 0);  /* Device detected and communication established. */
        }
    }
}

#ifdef IN_RING3
/**
 * Hardware reset used for machine power on and reset.
 *
 * @param pAhciport     The port to reset.
 */
static void ahciPortHwReset(PAHCIPort pAhciPort)
{
    /* Reset the address registers. */
    pAhciPort->regCLB  = 0;
    pAhciPort->regCLBU = 0;
    pAhciPort->regFB   = 0;
    pAhciPort->regFBU  = 0;

    /* Reset calculated addresses. */
    pAhciPort->GCPhysAddrClb = 0;
    pAhciPort->GCPhysAddrFb  = 0;
}
#endif

/**
 * Create implemented ports bitmap.
 *
 * @returns 32bit bitmask with a bit set for every implemented port.
 * @param   cPorts    Number of ports.
 */
static uint32_t ahciGetPortsImplemented(unsigned cPorts)
{
    uint32_t uPortsImplemented = 0;

    for (unsigned i = 0; i < cPorts; i++)
        uPortsImplemented |= (1 << i);

    return uPortsImplemented;
}

/**
 * Reset the entire HBA.
 *
 * @param   pThis       The HBA state.
 */
static void ahciHBAReset(PAHCI pThis)
{
    unsigned i;
    int rc = VINF_SUCCESS;

    LogFlow(("Reset the HBA controller\n"));

    /* Stop the CCC timer. */
    if (pThis->regHbaCccCtl & AHCI_HBA_CCC_CTL_EN)
    {
        rc = TMTimerStop(pThis->CTX_SUFF(pHbaCccTimer));
        if (RT_FAILURE(rc))
            AssertMsgFailed(("%s: Failed to stop timer!\n", __FUNCTION__));
    }

    /* Reset every port */
    for (i = 0; i < pThis->cPortsImpl; i++)
    {
        PAHCIPort pAhciPort = &pThis->ahciPort[i];

        pAhciPort->iLUN = i;
        ahciPortSwReset(pAhciPort);
    }

    /* Init Global registers */
    pThis->regHbaCap      = AHCI_HBA_CAP_ISS_SHIFT(AHCI_HBA_CAP_ISS_GEN2) |
                            AHCI_HBA_CAP_S64A | /* 64bit addressing supported */
                            AHCI_HBA_CAP_SAM  | /* AHCI mode only */
                            AHCI_HBA_CAP_SNCQ | /* Support native command queuing */
                            AHCI_HBA_CAP_SSS  | /* Staggered spin up */
                            AHCI_HBA_CAP_CCCS | /* Support command completion coalescing */
                            AHCI_HBA_CAP_NCS_SET(AHCI_NR_COMMAND_SLOTS) | /* Number of command slots we support */
                            AHCI_HBA_CAP_NP_SET(pThis->cPortsImpl); /* Number of supported ports */
    pThis->regHbaCtrl     = AHCI_HBA_CTRL_AE;
    pThis->regHbaIs       = 0;
    pThis->regHbaPi       = ahciGetPortsImplemented(pThis->cPortsImpl);
    pThis->regHbaVs       = AHCI_HBA_VS_MJR | AHCI_HBA_VS_MNR;
    pThis->regHbaCccCtl   = 0;
    pThis->regHbaCccPorts = 0;
    pThis->uCccTimeout    = 0;
    pThis->uCccPortNr     = 0;
    pThis->uCccNr         = 0;

    pThis->f64BitAddr = false;
    pThis->u32PortsInterrupted = 0;
    pThis->f8ByteMMIO4BytesWrittenSuccessfully = false;
    /* Clear the HBA Reset bit */
    pThis->regHbaCtrl &= ~AHCI_HBA_CTRL_HR;
}

/**
 * Memory mapped I/O Handler for read operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   GCPhysAddr  Physical address (in GC) where the read starts.
 * @param   pv          Where to store the result.
 * @param   cb          Number of bytes read.
 */
PDMBOTHCBDECL(int) ahciMMIORead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb)
{
    PAHCI pAhci = PDMINS_2_DATA(pDevIns, PAHCI);
    int   rc = VINF_SUCCESS;

    /* Break up 64 bits reads into two dword reads. */
    if (cb == 8)
    {
        rc = ahciMMIORead(pDevIns, pvUser, GCPhysAddr, pv, 4);
        if (RT_FAILURE(rc))
            return rc;

        return ahciMMIORead(pDevIns, pvUser, GCPhysAddr + 4, (uint8_t *)pv + 4, 4);
    }

    Log2(("#%d ahciMMIORead: pvUser=%p:{%.*Rhxs} cb=%d GCPhysAddr=%RGp rc=%Rrc\n",
          pDevIns->iInstance, pv, cb, pv, cb, GCPhysAddr, rc));

    /*
     * If the access offset is smaller than AHCI_HBA_GLOBAL_SIZE the guest accesses the global registers.
     * Otherwise it accesses the registers of a port.
     */
    uint32_t uOffset = (GCPhysAddr - pAhci->MMIOBase);
    uint32_t iReg;

    if (uOffset < AHCI_HBA_GLOBAL_SIZE)
    {
        iReg = uOffset >> 2;
        Log3(("%s: Trying to read from global register %u\n", __FUNCTION__, iReg));
        if (iReg < RT_ELEMENTS(g_aOpRegs))
        {
            const AHCIOPREG *pReg = &g_aOpRegs[iReg];
            rc = pReg->pfnRead(pAhci, iReg, (uint32_t *)pv);
        }
        else
        {
            Log3(("%s: Trying to read global register %u/%u!!!\n", __FUNCTION__, iReg, RT_ELEMENTS(g_aOpRegs)));
            *(uint32_t *)pv = 0;
        }
    }
    else
    {
        uint32_t iRegOffset;
        uint32_t iPort;

        /* Calculate accessed port. */
        uOffset -= AHCI_HBA_GLOBAL_SIZE;
        iPort = uOffset / AHCI_PORT_REGISTER_SIZE;
        iRegOffset  = (uOffset % AHCI_PORT_REGISTER_SIZE);
        iReg = iRegOffset >> 2;

        Log3(("%s: Trying to read from port %u and register %u\n", __FUNCTION__, iPort, iReg));

        if (RT_LIKELY(   iPort < pAhci->cPortsImpl
                      && iReg < RT_ELEMENTS(g_aPortOpRegs)))
        {
            const AHCIPORTOPREG *pPortReg = &g_aPortOpRegs[iReg];
            rc = pPortReg->pfnRead(pAhci, &pAhci->ahciPort[iPort], iReg, (uint32_t *)pv);
        }
        else
        {
            Log3(("%s: Trying to read port %u register %u/%u!!!\n", __FUNCTION__, iPort, iReg, RT_ELEMENTS(g_aPortOpRegs)));
            rc = VINF_IOM_MMIO_UNUSED_00;
        }

        /*
         * Windows Vista tries to read one byte from some registers instead of four.
         * Correct the value according to the read size.
         */
        if (RT_SUCCESS(rc) && cb != sizeof(uint32_t))
        {
            switch (cb)
            {
                case 1:
                {
                    uint8_t uNewValue;
                    uint8_t *p = (uint8_t *)pv;

                    iRegOffset &= 3;
                    Log3(("%s: iRegOffset=%u\n", __FUNCTION__, iRegOffset));
                    uNewValue = p[iRegOffset];
                    /* Clear old value */
                    *(uint32_t *)pv = 0;
                    *(uint8_t *)pv = uNewValue;
                    break;
                }
                default:
                    AssertMsgFailed(("%s: unsupported access width cb=%d uOffset=%x iPort=%x iRegOffset=%x iReg=%x!!!\n", __FUNCTION__, cb, uOffset, iPort, iRegOffset, iReg));
            }
        }
    }

    Log2(("#%d ahciMMIORead: return pvUser=%p:{%.*Rhxs} cb=%d GCPhysAddr=%RGp rc=%Rrc\n",
          pDevIns->iInstance, pv, cb, pv, cb, GCPhysAddr, rc));
    return rc;
}


/**
 * Memory mapped I/O Handler for write operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   GCPhysAddr  Physical address (in GC) where the read starts.
 * @param   pv          Where to fetch the result.
 * @param   cb          Number of bytes to write.
 */
PDMBOTHCBDECL(int) ahciMMIOWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb)
{
    PAHCI pAhci = PDMINS_2_DATA(pDevIns, PAHCI);
    int   rc = VINF_SUCCESS;

    /* Break up 64 bits writes into two dword writes. */
    if (cb == 8)
    {
        /*
         * Only write the first 4 bytes if they weren't already.
         * It is possible that the last write to the register caused a world
         * switch and we entered this function again.
         * Writing the first 4 bytes again could cause indeterminate behavior
         * which can cause errors in the guest.
         */
        if (!pAhci->f8ByteMMIO4BytesWrittenSuccessfully)
        {
            rc = ahciMMIOWrite(pDevIns, pvUser, GCPhysAddr, pv, 4);
            if (rc != VINF_SUCCESS)
                return rc;

            pAhci->f8ByteMMIO4BytesWrittenSuccessfully = true;
        }

        rc = ahciMMIOWrite(pDevIns, pvUser, GCPhysAddr + 4, (uint8_t *)pv + 4, 4);
        /*
         * Reset flag again so that the first 4 bytes are written again on the next
         * 8byte MMIO access.
         */
        if (rc == VINF_SUCCESS)
            pAhci->f8ByteMMIO4BytesWrittenSuccessfully = false;

        return rc;
    }

    Log2(("#%d ahciMMIOWrite: pvUser=%p:{%.*Rhxs} cb=%d GCPhysAddr=%RGp\n",
          pDevIns->iInstance, pv, cb, pv, cb, GCPhysAddr));

    /* Validate access. */
    if (cb != sizeof(uint32_t))
    {
        Log2(("%s: Bad write size!!! GCPhysAddr=%RGp cb=%d\n", __FUNCTION__, GCPhysAddr, cb));
        return VINF_SUCCESS;
    }
    if (GCPhysAddr & 0x3)
    {
        Log2(("%s: Unaligned write!!! GCPhysAddr=%RGp cb=%d\n", __FUNCTION__, GCPhysAddr, cb));
        return VINF_SUCCESS;
    }

    /*
     * If the access offset is smaller than 100h the guest accesses the global registers.
     * Otherwise it accesses the registers of a port.
     */
    uint32_t uOffset = (GCPhysAddr - pAhci->MMIOBase);
    uint32_t iReg;
    if (uOffset < AHCI_HBA_GLOBAL_SIZE)
    {
        Log3(("Write global HBA register\n"));
        iReg = uOffset >> 2;
        if (iReg < RT_ELEMENTS(g_aOpRegs))
        {
            const AHCIOPREG *pReg = &g_aOpRegs[iReg];
            rc = pReg->pfnWrite(pAhci, iReg, *(uint32_t *)pv);
        }
        else
        {
            Log3(("%s: Trying to write global register %u/%u!!!\n", __FUNCTION__, iReg, RT_ELEMENTS(g_aOpRegs)));
            rc = VINF_SUCCESS;
        }
    }
    else
    {
        uint32_t iPort;
        Log3(("Write Port register\n"));
        /* Calculate accessed port. */
        uOffset -= AHCI_HBA_GLOBAL_SIZE;
        iPort = uOffset / AHCI_PORT_REGISTER_SIZE;
        iReg  = (uOffset % AHCI_PORT_REGISTER_SIZE) >> 2;
        Log3(("%s: Trying to write to port %u and register %u\n", __FUNCTION__, iPort, iReg));
        if (RT_LIKELY(   iPort < pAhci->cPortsImpl
                      && iReg < RT_ELEMENTS(g_aPortOpRegs)))
        {
            const AHCIPORTOPREG *pPortReg = &g_aPortOpRegs[iReg];
            rc = pPortReg->pfnWrite(pAhci, &pAhci->ahciPort[iPort], iReg, *(uint32_t *)pv);
        }
        else
        {
            Log3(("%s: Trying to write port %u register %u/%u!!!\n", __FUNCTION__, iPort, iReg, RT_ELEMENTS(g_aPortOpRegs)));
            rc = VINF_SUCCESS;
        }
    }

    return rc;
}

PDMBOTHCBDECL(int) ahciIOPortWrite1(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    uint32_t iChannel = (uint32_t)(uintptr_t)pvUser;
    PAHCI    pAhci = PDMINS_2_DATA(pDevIns, PAHCI);
    PAHCIATACONTROLLER pCtl = &pAhci->aCts[iChannel];

    Assert(iChannel < 2);

    return ataControllerIOPortWrite1(pCtl, Port, u32, cb);
}

PDMBOTHCBDECL(int) ahciIOPortRead1(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    uint32_t iChannel = (uint32_t)(uintptr_t)pvUser;
    PAHCI    pAhci = PDMINS_2_DATA(pDevIns, PAHCI);
    PAHCIATACONTROLLER pCtl = &pAhci->aCts[iChannel];

    Assert(iChannel < 2);

    return ataControllerIOPortRead1(pCtl, Port, pu32, cb);
}

PDMBOTHCBDECL(int) ahciIOPortWrite2(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    uint32_t iChannel = (uint32_t)(uintptr_t)pvUser;
    PAHCI    pAhci = PDMINS_2_DATA(pDevIns, PAHCI);
    PAHCIATACONTROLLER pCtl = &pAhci->aCts[iChannel];

    Assert(iChannel < 2);

    return ataControllerIOPortWrite2(pCtl, Port, u32, cb);
}

PDMBOTHCBDECL(int) ahciIOPortRead2(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    uint32_t iChannel = (uint32_t)(uintptr_t)pvUser;
    PAHCI    pAhci = PDMINS_2_DATA(pDevIns, PAHCI);
    PAHCIATACONTROLLER pCtl = &pAhci->aCts[iChannel];

    Assert(iChannel < 2);

    return ataControllerIOPortRead2(pCtl, Port, pu32, cb);
}

PDMBOTHCBDECL(int) ahciLegacyFakeWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    AssertMsgFailed(("Should not happen\n"));
    return VINF_SUCCESS;
}

PDMBOTHCBDECL(int) ahciLegacyFakeRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    AssertMsgFailed(("Should not happen\n"));
    return VINF_SUCCESS;
}

#ifndef IN_RING0
/**
 * Port I/O Handler for primary port range IN string operations.
 * @see FNIOMIOPORTINSTRING for details.
 */
PDMBOTHCBDECL(int) ahciIOPortReadStr1(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, RTGCPTR *pGCPtrDst, PRTGCUINTREG pcTransfer, unsigned cb)
{
    uint32_t       iChannel = (uint32_t)(uintptr_t)pvUser;
    PAHCI          pAhci = PDMINS_2_DATA(pDevIns, PAHCI);
    PAHCIATACONTROLLER pCtl = &pAhci->aCts[iChannel];

    Assert(iChannel < 2);

    return ataControllerIOPortReadStr1(pCtl, Port, pGCPtrDst, pcTransfer, cb);
}


/**
 * Port I/O Handler for primary port range OUT string operations.
 * @see FNIOMIOPORTOUTSTRING for details.
 */
PDMBOTHCBDECL(int) ahciIOPortWriteStr1(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, RTGCPTR *pGCPtrSrc, PRTGCUINTREG pcTransfer, unsigned cb)
{
    uint32_t       iChannel = (uint32_t)(uintptr_t)pvUser;
    PAHCI          pAhci = PDMINS_2_DATA(pDevIns, PAHCI);
    PAHCIATACONTROLLER pCtl = &pAhci->aCts[iChannel];

    Assert(iChannel < 2);

    return ataControllerIOPortReadStr1(pCtl, Port, pGCPtrSrc, pcTransfer, cb);
}
#endif /* !IN_RING0 */

#ifdef IN_RING3

static DECLCALLBACK(int) ahciMMIOMap(PPCIDEVICE pPciDev, /*unsigned*/ int iRegion, RTGCPHYS GCPhysAddress, uint32_t cb, PCIADDRESSSPACE enmType)
{
    PAHCI pThis = PCIDEV_2_PAHCI(pPciDev);
    PPDMDEVINS pDevIns = pPciDev->pDevIns;
    int   rc = VINF_SUCCESS;

    Log2(("%s: registering MMIO area at GCPhysAddr=%RGp cb=%u\n", __FUNCTION__, GCPhysAddress, cb));

    Assert(enmType == PCI_ADDRESS_SPACE_MEM);
    Assert(cb >= 4352);

    /* We use the assigned size here, because we currently only support page aligned MMIO ranges. */
    rc = PDMDevHlpMMIORegister(pDevIns, GCPhysAddress, cb, NULL,
                               ahciMMIOWrite, ahciMMIORead, NULL, "AHCI");
    if (RT_FAILURE(rc))
        return rc;

    if (pThis->fR0Enabled)
    {
        rc = PDMDevHlpMMIORegisterR0(pDevIns, GCPhysAddress, cb, 0,
                                     "ahciMMIOWrite", "ahciMMIORead", NULL);
        if (RT_FAILURE(rc))
            return rc;
    }

    if (pThis->fGCEnabled)
    {
        rc = PDMDevHlpMMIORegisterGC(pDevIns, GCPhysAddress, cb, 0,
                                     "ahciMMIOWrite", "ahciMMIORead", NULL);
        if (RT_FAILURE(rc))
            return rc;
    }

    pThis->MMIOBase = GCPhysAddress;
    return rc;
}

/**
 * Map the legacy I/O port ranges to make Solaris work with the controller.
 */
static DECLCALLBACK(int) ahciLegacyFakeIORangeMap(PPCIDEVICE pPciDev, /*unsigned*/ int iRegion, RTGCPHYS GCPhysAddress, uint32_t cb, PCIADDRESSSPACE enmType)
{
    PAHCI pThis = PCIDEV_2_PAHCI(pPciDev);
    PPDMDEVINS pDevIns = pPciDev->pDevIns;
    int   rc = VINF_SUCCESS;

    Log2(("%s: registering fake I/O area at GCPhysAddr=%RGp cb=%u\n", __FUNCTION__, GCPhysAddress, cb));

    Assert(enmType == PCI_ADDRESS_SPACE_IO);

    /* We use the assigned size here, because we currently only support page aligned MMIO ranges. */
    rc = PDMDevHlpIOPortRegister(pDevIns, (RTIOPORT)GCPhysAddress, cb, NULL,
                                 ahciLegacyFakeWrite, ahciLegacyFakeRead, NULL, NULL, "AHCI Fake");
    if (RT_FAILURE(rc))
        return rc;

    if (pThis->fR0Enabled)
    {
        rc = PDMDevHlpIOPortRegisterR0(pDevIns, (RTIOPORT)GCPhysAddress, cb, 0,
                                       "ahciLegacyFakeWrite", "ahciLegacyFakeRead", NULL, NULL, "AHCI Fake");
        if (RT_FAILURE(rc))
            return rc;
    }

    if (pThis->fGCEnabled)
    {
        rc = PDMDevHlpIOPortRegisterGC(pDevIns, (RTIOPORT)GCPhysAddress, cb, 0,
                                       "ahciLegacyFakeWrite", "ahciLegacyFakeRead", NULL, NULL, "AHCI Fake");
        if (RT_FAILURE(rc))
            return rc;
    }

    return rc;
}

/* -=-=-=-=-=- PAHCI::ILeds  -=-=-=-=-=- */

/**
 * Gets the pointer to the status LED of a unit.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   iLUN            The unit which status LED we desire.
 * @param   ppLed           Where to store the LED pointer.
 */
static DECLCALLBACK(int) ahciStatus_QueryStatusLed(PPDMILEDPORTS pInterface, unsigned iLUN, PPDMLED *ppLed)
{
    PAHCI pAhci = PDMILEDPORTS_2_PAHCI(pInterface);
    if (iLUN < AHCI_MAX_NR_PORTS_IMPL)
    {
        *ppLed = &pAhci->ahciPort[iLUN].Led;
        Assert((*ppLed)->u32Magic == PDMLED_MAGIC);
        return VINF_SUCCESS;
    }
    return VERR_PDM_LUN_NOT_FOUND;
}

/**
 * Queries an interface to the driver.
 *
 * @returns Pointer to interface.
 * @returns NULL if the interface was not supported by the device.
 * @param   pInterface          Pointer to ATADevState::IBase.
 * @param   enmInterface        The requested interface identification.
 */
static DECLCALLBACK(void *) ahciStatus_QueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    PAHCI pAhci = PDMIBASE_2_PAHCI(pInterface);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pAhci->IBase;
        case PDMINTERFACE_LED_PORTS:
            return &pAhci->ILeds;
        default:
            return NULL;
    }
}

/**
 * Query interface method for the AHCI port.
 */
static DECLCALLBACK(void *) ahciPortQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    PAHCIPort pAhciPort = PDMIBASE_2_PAHCIPORT(pInterface);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pAhciPort->IBase;
        case PDMINTERFACE_BLOCK_PORT:
            return &pAhciPort->IPort;
        case PDMINTERFACE_BLOCK_ASYNC_PORT:
            return &pAhciPort->IPortAsync;
        case PDMINTERFACE_MOUNT_NOTIFY:
            return &pAhciPort->IMountNotify;
        default:
            return NULL;
    }
}

static DECLCALLBACK(void) ahciRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    uint32_t i;
    PAHCI pAhci = PDMINS_2_DATA(pDevIns, PAHCI);

    pAhci->pDevInsRC += offDelta;
    pAhci->pHbaCccTimerRC = TMTimerRCPtr(pAhci->pHbaCccTimerR3);
    pAhci->pNotifierQueueRC = PDMQueueRCPtr(pAhci->pNotifierQueueR3);

    /* Relocate every port. */
    for (i = 0; i < RT_ELEMENTS(pAhci->ahciPort); i++)
    {
        PAHCIPort pAhciPort = &pAhci->ahciPort[i];
        pAhciPort->pAhciRC += offDelta;
        pAhciPort->pDevInsRC += offDelta;
    }

    /* Relocate emulated ATA controllers. */
    for (i = 0; i < RT_ELEMENTS(pAhci->aCts); i++)
        ataControllerRelocate(&pAhci->aCts[i], offDelta);
}

#ifdef DEBUG

/**
 * Dump info about the FIS
 *
 * @returns nothing
 * @param   pAhciPort     The port the command FIS was read from.
 * @param   cmdFis        The FIS to print info from.
 */
static void ahciDumpFisInfo(PAHCIPort pAhciPort, uint8_t *cmdFis)
{
    ahciLog(("%s: *** Begin FIS info dump. ***\n", __FUNCTION__));
    /* Print FIS type. */
    switch (cmdFis[AHCI_CMDFIS_TYPE])
    {
        case AHCI_CMDFIS_TYPE_H2D:
            {
                ahciLog(("%s: Command Fis type: H2D\n", __FUNCTION__));
                ahciLog(("%s: Command Fis size: %d bytes\n", __FUNCTION__, AHCI_CMDFIS_TYPE_H2D_SIZE));
                if (cmdFis[AHCI_CMDFIS_BITS] & AHCI_CMDFIS_C)
                {
                    ahciLog(("%s: Command register update\n", __FUNCTION__));
                }
                else
                {
                    ahciLog(("%s: Control register update\n", __FUNCTION__));
                }
                ahciLog(("%s: CMD=%#04x \"%s\"\n", __FUNCTION__, cmdFis[AHCI_CMDFIS_CMD], ATACmdText(cmdFis[AHCI_CMDFIS_CMD])));
                ahciLog(("%s: FEAT=%#04x\n", __FUNCTION__, cmdFis[AHCI_CMDFIS_FET]));
                ahciLog(("%s: SECTN=%#04x\n", __FUNCTION__, cmdFis[AHCI_CMDFIS_SECTN]));
                ahciLog(("%s: CYLL=%#04x\n", __FUNCTION__, cmdFis[AHCI_CMDFIS_CYLL]));
                ahciLog(("%s: CYLH=%#04x\n", __FUNCTION__, cmdFis[AHCI_CMDFIS_CYLH]));
                ahciLog(("%s: HEAD=%#04x\n", __FUNCTION__, cmdFis[AHCI_CMDFIS_HEAD]));

                ahciLog(("%s: SECTNEXP=%#04x\n", __FUNCTION__, cmdFis[AHCI_CMDFIS_SECTNEXP]));
                ahciLog(("%s: CYLLEXP=%#04x\n", __FUNCTION__, cmdFis[AHCI_CMDFIS_CYLLEXP]));
                ahciLog(("%s: CYLHEXP=%#04x\n", __FUNCTION__, cmdFis[AHCI_CMDFIS_CYLHEXP]));
                ahciLog(("%s: FETEXP=%#04x\n", __FUNCTION__, cmdFis[AHCI_CMDFIS_FETEXP]));

                ahciLog(("%s: SECTC=%#04x\n", __FUNCTION__, cmdFis[AHCI_CMDFIS_SECTC]));
                ahciLog(("%s: SECTCEXP=%#04x\n", __FUNCTION__, cmdFis[AHCI_CMDFIS_SECTCEXP]));
                ahciLog(("%s: CTL=%#04x\n", __FUNCTION__, cmdFis[AHCI_CMDFIS_CTL]));
                if (cmdFis[AHCI_CMDFIS_CTL] & AHCI_CMDFIS_CTL_SRST)
                {
                    ahciLog(("%s: Reset bit is set\n", __FUNCTION__));
                }
            }
            break;
        case AHCI_CMDFIS_TYPE_D2H:
            {
                ahciLog(("%s: Command Fis type D2H\n", __FUNCTION__));
                ahciLog(("%s: Command Fis size: %d\n", __FUNCTION__, AHCI_CMDFIS_TYPE_D2H_SIZE));
            }
            break;
        case AHCI_CMDFIS_TYPE_SETDEVBITS:
            {
                ahciLog(("%s: Command Fis type Set Device Bits\n", __FUNCTION__));
                ahciLog(("%s: Command Fis size: %d\n", __FUNCTION__, AHCI_CMDFIS_TYPE_SETDEVBITS_SIZE));
            }
            break;
        case AHCI_CMDFIS_TYPE_DMAACTD2H:
            {
                ahciLog(("%s: Command Fis type DMA Activate H2D\n", __FUNCTION__));
                ahciLog(("%s: Command Fis size: %d\n", __FUNCTION__, AHCI_CMDFIS_TYPE_DMAACTD2H_SIZE));
            }
            break;
        case AHCI_CMDFIS_TYPE_DMASETUP:
            {
                ahciLog(("%s: Command Fis type DMA Setup\n", __FUNCTION__));
                ahciLog(("%s: Command Fis size: %d\n", __FUNCTION__, AHCI_CMDFIS_TYPE_DMASETUP_SIZE));
            }
            break;
        case AHCI_CMDFIS_TYPE_PIOSETUP:
            {
                ahciLog(("%s: Command Fis type PIO Setup\n", __FUNCTION__));
                ahciLog(("%s: Command Fis size: %d\n", __FUNCTION__, AHCI_CMDFIS_TYPE_PIOSETUP_SIZE));
            }
            break;
        case AHCI_CMDFIS_TYPE_DATA:
            {
                ahciLog(("%s: Command Fis type Data\n", __FUNCTION__));
            }
            break;
        default:
            ahciLog(("%s: ERROR Unknown command FIS type\n", __FUNCTION__));
            break;
    }
    ahciLog(("%s: *** End FIS info dump. ***\n", __FUNCTION__));
}

/**
 * Dump info about the command header
 *
 * @returns nothing
 * @param   pAhciPort   Poitner to the port the command header was read from.
 * @param   pCmdHdr     The command header to print info from.
 */
static void ahciDumpCmdHdrInfo(PAHCIPort pAhciPort, CmdHdr *pCmdHdr)
{
    ahciLog(("%s: *** Begin command header info dump. ***\n", __FUNCTION__));
    ahciLog(("%s: Number of Scatter/Gatther List entries: %u\n", __FUNCTION__, AHCI_CMDHDR_PRDTL_ENTRIES(pCmdHdr->u32DescInf)));
    if (pCmdHdr->u32DescInf & AHCI_CMDHDR_C)
        ahciLog(("%s: Clear busy upon R_OK\n", __FUNCTION__));
    if (pCmdHdr->u32DescInf & AHCI_CMDHDR_B)
        ahciLog(("%s: BIST Fis\n", __FUNCTION__));
    if (pCmdHdr->u32DescInf & AHCI_CMDHDR_R)
        ahciLog(("%s: Device Reset Fis\n", __FUNCTION__));
    if (pCmdHdr->u32DescInf & AHCI_CMDHDR_P)
        ahciLog(("%s: Command prefetchable\n", __FUNCTION__));
    if (pCmdHdr->u32DescInf & AHCI_CMDHDR_W)
        ahciLog(("%s: Device write\n", __FUNCTION__));
    else
        ahciLog(("%s: Device read\n", __FUNCTION__));
    if (pCmdHdr->u32DescInf & AHCI_CMDHDR_A)
        ahciLog(("%s: ATAPI command\n", __FUNCTION__));
    else
        ahciLog(("%s: ATA command\n", __FUNCTION__));

    ahciLog(("%s: Command FIS length %u DW\n", __FUNCTION__, (pCmdHdr->u32DescInf & AHCI_CMDHDR_CFL_MASK)));
    ahciLog(("%s: *** End command header info dump. ***\n", __FUNCTION__));
}
#endif /* DEBUG */

/**
 * Post the first D2H FIS from the device into guest memory.
 *
 * @returns nothing
 * @param   pAhciPort    Pointer to the port which "receives" the FIS.
 */
static void ahciPostFirstD2HFisIntoMemory(PAHCIPort pAhciPort)
{
    uint8_t d2hFis[AHCI_CMDFIS_TYPE_D2H_SIZE];

    pAhciPort->fFirstD2HFisSend = true;

    ahciLog(("%s: Sending First D2H FIS from FIFO\n", __FUNCTION__));
    memset(&d2hFis[0], 0, sizeof(d2hFis));
    d2hFis[AHCI_CMDFIS_TYPE] = AHCI_CMDFIS_TYPE_D2H;
    d2hFis[AHCI_CMDFIS_ERR]  = 0x01;

    d2hFis[AHCI_CMDFIS_STS]  = 0x00;

    /* Set the signature based on the device type. */
    if (pAhciPort->fATAPI)
    {
        d2hFis[AHCI_CMDFIS_CYLL] = 0x14;
        d2hFis[AHCI_CMDFIS_CYLH] = 0xeb;
    }
    else
    {
        d2hFis[AHCI_CMDFIS_CYLL]  = 0x00;
        d2hFis[AHCI_CMDFIS_CYLH]  = 0x00;
    }

    d2hFis[AHCI_CMDFIS_HEAD]  = 0x00;
    d2hFis[AHCI_CMDFIS_SECTN] = 0x01;
    d2hFis[AHCI_CMDFIS_SECTC] = 0x01;

    pAhciPort->regTFD = (1 << 8) | ATA_STAT_SEEK | ATA_STAT_WRERR;

    ahciPostFisIntoMemory(pAhciPort, AHCI_CMDFIS_TYPE_D2H, d2hFis);
}

/**
 * Post the FIS in the memory area allocated by the guest and set interrupt if neccessary.
 *
 * @returns VBox status code
 * @param   pAhciPort  The port which "receives" the FIS.
 * @param   uFisType   The type of the FIS.
 * @param   pCmdFis    Pointer to the FIS which is to be posted into memory.
 */
static int ahciPostFisIntoMemory(PAHCIPort pAhciPort, unsigned uFisType, uint8_t *pCmdFis)
{
    int         rc = VINF_SUCCESS;
    RTGCPHYS    GCPhysAddrRecFis = pAhciPort->GCPhysAddrFb;
    unsigned    cbFis = 0;

    ahciLog(("%s: pAhciPort=%p uFisType=%u pCmdFis=%p\n", __FUNCTION__, pAhciPort, uFisType, pCmdFis));

    if (pAhciPort->regCMD & AHCI_PORT_CMD_FRE)
    {
        AssertMsg(GCPhysAddrRecFis, ("%s: GCPhysAddrRecFis is 0\n", __FUNCTION__));

        /* Determine the offset and size of the FIS based on uFisType. */
        switch (uFisType)
        {
            case AHCI_CMDFIS_TYPE_D2H:
                {
                    GCPhysAddrRecFis += AHCI_RECFIS_RFIS_OFFSET;
                    cbFis = AHCI_CMDFIS_TYPE_D2H_SIZE;
                }
                break;
            case AHCI_CMDFIS_TYPE_SETDEVBITS:
                {
                    GCPhysAddrRecFis += AHCI_RECFIS_SDBFIS_OFFSET;
                    cbFis = AHCI_CMDFIS_TYPE_SETDEVBITS_SIZE;
                }
                break;
            case AHCI_CMDFIS_TYPE_DMASETUP:
                {
                    GCPhysAddrRecFis += AHCI_RECFIS_DSFIS_OFFSET;
                    cbFis = AHCI_CMDFIS_TYPE_DMASETUP_SIZE;
                }
                break;
            case AHCI_CMDFIS_TYPE_PIOSETUP:
                {
                    GCPhysAddrRecFis += AHCI_RECFIS_PSFIS_OFFSET;
                    cbFis = AHCI_CMDFIS_TYPE_PIOSETUP_SIZE;
                }
                break;
            default:
                /*
                 * We should post the unknown FIS into memory too but this never happens because
                 * we know which FIS types we generate. ;)
                 */
                AssertMsgFailed(("%s: Unknown FIS type!\n", __FUNCTION__));
        }

        /* Post the FIS into memory. */
        ahciLog(("%s: PDMDevHlpPhysWrite GCPhysAddrRecFis=%RGp cbFis=%u\n", __FUNCTION__, GCPhysAddrRecFis, cbFis));
        PDMDevHlpPhysWrite(pAhciPort->CTX_SUFF(pDevIns), GCPhysAddrRecFis, pCmdFis, cbFis);
    }

    return rc;
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

static void atapiCmdOK(PAHCIPort pAhciPort, PAHCIPORTTASKSTATE pAhciPortTaskState)
{
    pAhciPortTaskState->uATARegError = 0;
    pAhciPortTaskState->uATARegStatus = ATA_STAT_READY | ATA_STAT_SEEK;
    pAhciPortTaskState->cmdFis[AHCI_CMDFIS_SECTN] = (pAhciPortTaskState->cmdFis[AHCI_CMDFIS_SECTN] & ~7)
        | ((pAhciPortTaskState->uTxDir != PDMBLOCKTXDIR_TO_DEVICE) ? ATAPI_INT_REASON_IO : 0)
        | (!pAhciPortTaskState->cbTransfer ? ATAPI_INT_REASON_CD : 0);
    pAhciPort->uATAPISenseKey = SCSI_SENSE_NONE;
    pAhciPort->uATAPIASC = SCSI_ASC_NONE;
}


static void atapiCmdError(PAHCIPort pAhciPort, PAHCIPORTTASKSTATE pAhciPortTaskState, uint8_t uATAPISenseKey, uint8_t uATAPIASC)
{
    pAhciPortTaskState->uATARegError = uATAPISenseKey << 4;
    pAhciPortTaskState->uATARegStatus = ATA_STAT_READY | ATA_STAT_ERR;
    pAhciPortTaskState->cmdFis[AHCI_CMDFIS_SECTN] = (pAhciPortTaskState->cmdFis[AHCI_CMDFIS_SECTN] & ~7) |
                                                     ATAPI_INT_REASON_IO | ATAPI_INT_REASON_CD;
    pAhciPort->uATAPISenseKey = uATAPISenseKey;
    pAhciPort->uATAPIASC = uATAPIASC;
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

static int ahciIdentifySS(PAHCIPort pAhciPort, void *pvBuf)
{
    uint16_t *p;
    int rc = VINF_SUCCESS;

    p = (uint16_t *)pvBuf;
    memset(p, 0, 512);
    p[0] = RT_H2LE_U16(0x0040);
    p[1] = RT_H2LE_U16(RT_MIN(pAhciPort->PCHSGeometry.cCylinders, 16383));
    p[3] = RT_H2LE_U16(pAhciPort->PCHSGeometry.cHeads);
    /* Block size; obsolete, but required for the BIOS. */
    p[5] = RT_H2LE_U16(512);
    p[6] = RT_H2LE_U16(pAhciPort->PCHSGeometry.cSectors);
    ataPadString((uint8_t *)(p + 10), pAhciPort->szSerialNumber, AHCI_SERIAL_NUMBER_LENGTH); /* serial number */
    p[20] = RT_H2LE_U16(3); /* XXX: retired, cache type */
    p[21] = RT_H2LE_U16(512); /* XXX: retired, cache size in sectors */
    p[22] = RT_H2LE_U16(0); /* ECC bytes per sector */
    ataPadString((uint8_t *)(p + 23), pAhciPort->szFirmwareRevision, AHCI_FIRMWARE_REVISION_LENGTH); /* firmware version */
    ataPadString((uint8_t *)(p + 27), pAhciPort->szModelNumber, AHCI_MODEL_NUMBER_LENGTH); /* model */
#if ATA_MAX_MULT_SECTORS > 1
    p[47] = RT_H2LE_U16(0x8000 | ATA_MAX_MULT_SECTORS);
#endif
    p[48] = RT_H2LE_U16(1); /* dword I/O, used by the BIOS */
    p[49] = RT_H2LE_U16(1 << 11 | 1 << 9 | 1 << 8); /* DMA and LBA supported */
    p[50] = RT_H2LE_U16(1 << 14); /* No drive specific standby timer minimum */
    p[51] = RT_H2LE_U16(240); /* PIO transfer cycle */
    p[52] = RT_H2LE_U16(240); /* DMA transfer cycle */
    p[53] = RT_H2LE_U16(1 | 1 << 1 | 1 << 2); /* words 54-58,64-70,88 valid */
    p[54] = RT_H2LE_U16(RT_MIN(pAhciPort->PCHSGeometry.cCylinders, 16383));
    p[55] = RT_H2LE_U16(pAhciPort->PCHSGeometry.cHeads);
    p[56] = RT_H2LE_U16(pAhciPort->PCHSGeometry.cSectors);
    p[57] = RT_H2LE_U16(RT_MIN(pAhciPort->PCHSGeometry.cCylinders, 16383) * pAhciPort->PCHSGeometry.cHeads * pAhciPort->PCHSGeometry.cSectors);
    p[58] = RT_H2LE_U16(RT_MIN(pAhciPort->PCHSGeometry.cCylinders, 16383) * pAhciPort->PCHSGeometry.cHeads * pAhciPort->PCHSGeometry.cSectors >> 16);
    if (pAhciPort->cMultSectors)
        p[59] = RT_H2LE_U16(0x100 | pAhciPort->cMultSectors);
    if (pAhciPort->cTotalSectors <= (1 << 28) - 1)
    {
        p[60] = RT_H2LE_U16(pAhciPort->cTotalSectors);
        p[61] = RT_H2LE_U16(pAhciPort->cTotalSectors >> 16);
    }
    else
    {
        /* Report maximum number of sectors possible with LBA28 */
        p[60] = RT_H2LE_U16(((1 << 28) - 1) & 0xffff);
        p[61] = RT_H2LE_U16(((1 << 28) - 1) >> 16);
    }
    p[63] = RT_H2LE_U16(ATA_TRANSFER_ID(ATA_MODE_MDMA, ATA_MDMA_MODE_MAX, pAhciPort->uATATransferMode)); /* MDMA modes supported / mode enabled */
    p[64] = RT_H2LE_U16(ATA_PIO_MODE_MAX > 2 ? (1 << (ATA_PIO_MODE_MAX - 2)) - 1 : 0); /* PIO modes beyond PIO2 supported */
    p[65] = RT_H2LE_U16(120); /* minimum DMA multiword tx cycle time */
    p[66] = RT_H2LE_U16(120); /* recommended DMA multiword tx cycle time */
    p[67] = RT_H2LE_U16(120); /* minimum PIO cycle time without flow control */
    p[68] = RT_H2LE_U16(120); /* minimum PIO cycle time with IORDY flow control */
    p[80] = RT_H2LE_U16(0x7e); /* support everything up to ATA/ATAPI-6 */
    p[81] = RT_H2LE_U16(0x22); /* conforms to ATA/ATAPI-6 */
    p[82] = RT_H2LE_U16(1 << 3 | 1 << 5 | 1 << 6); /* supports power management,  write cache and look-ahead */
    p[83] = RT_H2LE_U16(1 << 14 | 1 << 10 | 1 << 12 | 1 << 13); /* supports LBA48, FLUSH CACHE and FLUSH CACHE EXT */
    p[84] = RT_H2LE_U16(1 << 14);
    p[85] = RT_H2LE_U16(1 << 3 | 1 << 5 | 1 << 6); /* enabled power management,  write cache and look-ahead */
    p[86] = RT_H2LE_U16(1 << 10 | 1 << 12 | 1 << 13); /* enabled LBA48, FLUSH CACHE and FLUSH CACHE EXT */
    p[87] = RT_H2LE_U16(1 << 14);
    p[88] = RT_H2LE_U16(ATA_TRANSFER_ID(ATA_MODE_UDMA, ATA_UDMA_MODE_MAX, pAhciPort->uATATransferMode)); /* UDMA modes supported / mode enabled */
    p[93] = RT_H2LE_U16(0x00);
    p[100] = RT_H2LE_U16(pAhciPort->cTotalSectors);
    p[101] = RT_H2LE_U16(pAhciPort->cTotalSectors >> 16);
    p[102] = RT_H2LE_U16(pAhciPort->cTotalSectors >> 32);
    p[103] = RT_H2LE_U16(pAhciPort->cTotalSectors >> 48);

    /* The following are SATA specific */
    p[75] = RT_H2LE_U16(31); /* We support 32 commands */
    p[76] = RT_H2LE_U16((1 << 8) | (1 << 2)); /* Native command queuing and Serial ATA Gen2 (3.0 Gbps) speed supported */

    return VINF_SUCCESS;
}

typedef int (*PAtapiFunc)(PAHCIPORTTASKSTATE, PAHCIPort, int *);

static int atapiGetConfigurationSS(PAHCIPORTTASKSTATE, PAHCIPort, int *);
static int atapiIdentifySS(PAHCIPORTTASKSTATE, PAHCIPort, int *);
static int atapiInquirySS(PAHCIPORTTASKSTATE, PAHCIPort, int *);
static int atapiMechanismStatusSS(PAHCIPORTTASKSTATE, PAHCIPort, int *);
static int atapiModeSenseErrorRecoverySS(PAHCIPORTTASKSTATE, PAHCIPort, int *);
static int atapiModeSenseCDStatusSS(PAHCIPORTTASKSTATE, PAHCIPort, int *);
static int atapiReadCapacitySS(PAHCIPORTTASKSTATE, PAHCIPort, int *);
static int atapiReadDiscInformationSS(PAHCIPORTTASKSTATE, PAHCIPort, int *);
static int atapiReadTOCNormalSS(PAHCIPORTTASKSTATE, PAHCIPort, int *);
static int atapiReadTOCMultiSS(PAHCIPORTTASKSTATE, PAHCIPort, int *);
static int atapiReadTOCRawSS(PAHCIPORTTASKSTATE, PAHCIPort, int *);
static int atapiReadTrackInformationSS(PAHCIPORTTASKSTATE, PAHCIPort, int *);
static int atapiRequestSenseSS(PAHCIPORTTASKSTATE, PAHCIPort, int *);
//static int atapiPassthroughSS(PAHCIPORTTASKSTATE, PAHCIPort, int *);

/**
 * Source/sink function indexes for g_apfnAtapiFuncs.
 */
typedef enum ATAPIFN
{
    ATAFN_SS_NULL = 0,
    ATAFN_SS_ATAPI_GET_CONFIGURATION,
    ATAFN_SS_ATAPI_IDENTIFY,
    ATAFN_SS_ATAPI_INQUIRY,
    ATAFN_SS_ATAPI_MECHANISM_STATUS,
    ATAFN_SS_ATAPI_MODE_SENSE_ERROR_RECOVERY,
    ATAFN_SS_ATAPI_MODE_SENSE_CD_STATUS,
    ATAFN_SS_ATAPI_READ_CAPACITY,
    ATAFN_SS_ATAPI_READ_DISC_INFORMATION,
    ATAFN_SS_ATAPI_READ_TOC_NORMAL,
    ATAFN_SS_ATAPI_READ_TOC_MULTI,
    ATAFN_SS_ATAPI_READ_TOC_RAW,
    ATAFN_SS_ATAPI_READ_TRACK_INFORMATION,
    ATAFN_SS_ATAPI_REQUEST_SENSE,
    //ATAFN_SS_ATAPI_PASSTHROUGH,
    ATAFN_SS_MAX
} ATAPIFN;

/**
 * Array of source/sink functions, the index is ATAFNSS.
 * Make sure ATAFNSS and this array match!
 */
static const PAtapiFunc g_apfnAtapiFuncs[ATAFN_SS_MAX] =
{
    NULL,
    atapiGetConfigurationSS,
    atapiIdentifySS,
    atapiInquirySS,
    atapiMechanismStatusSS,
    atapiModeSenseErrorRecoverySS,
    atapiModeSenseCDStatusSS,
    atapiReadCapacitySS,
    atapiReadDiscInformationSS,
    atapiReadTOCNormalSS,
    atapiReadTOCMultiSS,
    atapiReadTOCRawSS,
    atapiReadTrackInformationSS,
    atapiRequestSenseSS
    //atapiPassthroughSS
};

static int atapiIdentifySS(PAHCIPORTTASKSTATE pAhciPortTaskState, PAHCIPort pAhciPort, int *pcbData)
{
    uint16_t p[256];
    RTUUID Uuid;
    int rc;

    memset(p, 0, 512);
    /* Removable CDROM, 50us response, 12 byte packets */
    p[0] = RT_H2LE_U16(2 << 14 | 5 << 8 | 1 << 7 | 2 << 5 | 0 << 0);
    ataPadString((uint8_t *)(p + 10), pAhciPort->szSerialNumber, AHCI_SERIAL_NUMBER_LENGTH); /* serial number */
    p[20] = RT_H2LE_U16(3); /* XXX: retired, cache type */
    p[21] = RT_H2LE_U16(512); /* XXX: retired, cache size in sectors */
    ataPadString((uint8_t *)(p + 23), pAhciPort->szFirmwareRevision, AHCI_FIRMWARE_REVISION_LENGTH); /* firmware version */
    ataPadString((uint8_t *)(p + 27), pAhciPort->szModelNumber, AHCI_MODEL_NUMBER_LENGTH); /* model */
    p[49] = RT_H2LE_U16(1 << 11 | 1 << 9 | 1 << 8); /* DMA and LBA supported */
    p[50] = RT_H2LE_U16(1 << 14);  /* No drive specific standby timer minimum */
    p[51] = RT_H2LE_U16(240); /* PIO transfer cycle */
    p[52] = RT_H2LE_U16(240); /* DMA transfer cycle */
    p[53] = RT_H2LE_U16(1 << 1 | 1 << 2); /* words 64-70,88 are valid */
    p[63] = RT_H2LE_U16(ATA_TRANSFER_ID(ATA_MODE_MDMA, ATA_MDMA_MODE_MAX, pAhciPort->uATATransferMode)); /* MDMA modes supported / mode enabled */
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
    p[88] = RT_H2LE_U16(ATA_TRANSFER_ID(ATA_MODE_UDMA, ATA_UDMA_MODE_MAX, pAhciPort->uATATransferMode)); /* UDMA modes supported / mode enabled */
    p[93] = RT_H2LE_U16((1 | 1 << 1) << ((pAhciPort->iLUN & 1) == 0 ? 0 : 8) | 1 << 13 | 1 << 14);

    /* The following are SATA specific */
    p[75] = RT_H2LE_U16(31); /* We support 32 commands */
    p[76] = RT_H2LE_U16((1 << 8) | (1 << 2)); /* Native command queuing and Serial ATA Gen2 (3.0 Gbps) speed supported */

    /* Copy the buffer in to the scatter gather list. */
    *pcbData =  ahciScatterGatherListCopyFromBuffer(pAhciPortTaskState, (void *)&p[0], sizeof(p));

    atapiCmdOK(pAhciPort, pAhciPortTaskState);
    return VINF_SUCCESS;
}

static int atapiReadCapacitySS(PAHCIPORTTASKSTATE pAhciPortTaskState, PAHCIPort pAhciPort, int *pcbData)
{
    uint8_t aBuf[8];

    ataH2BE_U32(aBuf, pAhciPort->cTotalSectors - 1);
    ataH2BE_U32(aBuf + 4, 2048);

    /* Copy the buffer in to the scatter gather list. */
    *pcbData = ahciScatterGatherListCopyFromBuffer(pAhciPortTaskState, (void *)&aBuf[0], sizeof(aBuf));

    atapiCmdOK(pAhciPort, pAhciPortTaskState);
    return VINF_SUCCESS;
}


static int atapiReadDiscInformationSS(PAHCIPORTTASKSTATE pAhciPortTaskState, PAHCIPort pAhciPort, int *pcbData)
{
    uint8_t aBuf[34];

    memset(aBuf, '\0', 34);
    ataH2BE_U16(aBuf, 32);
    aBuf[2] = (0 << 4) | (3 << 2) | (2 << 0); /* not erasable, complete session, complete disc */
    aBuf[3] = 1; /* number of first track */
    aBuf[4] = 1; /* number of sessions (LSB) */
    aBuf[5] = 1; /* first track number in last session (LSB) */
    aBuf[6] = 1; /* last track number in last session (LSB) */
    aBuf[7] = (0 << 7) | (0 << 6) | (1 << 5) | (0 << 2) | (0 << 0); /* disc id not valid, disc bar code not valid, unrestricted use, not dirty, not RW medium */
    aBuf[8] = 0; /* disc type = CD-ROM */
    aBuf[9] = 0; /* number of sessions (MSB) */
    aBuf[10] = 0; /* number of sessions (MSB) */
    aBuf[11] = 0; /* number of sessions (MSB) */
    ataH2BE_U32(aBuf + 16, 0x00ffffff); /* last session lead-in start time is not available */
    ataH2BE_U32(aBuf + 20, 0x00ffffff); /* last possible start time for lead-out is not available */

    /* Copy the buffer in to the scatter gather list. */
    *pcbData = ahciScatterGatherListCopyFromBuffer(pAhciPortTaskState, (void *)&aBuf[0], sizeof(aBuf));

    atapiCmdOK(pAhciPort, pAhciPortTaskState);
    return VINF_SUCCESS;
}


static int atapiReadTrackInformationSS(PAHCIPORTTASKSTATE pAhciPortTaskState, PAHCIPort pAhciPort, int *pcbData)
{
    uint8_t aBuf[36];

    /* Accept address/number type of 1 only, and only track 1 exists. */
    if ((pAhciPortTaskState->aATAPICmd[1] & 0x03) != 1 || ataBE2H_U32(&pAhciPortTaskState->aATAPICmd[2]) != 1)
    {
        atapiCmdError(pAhciPort, pAhciPortTaskState, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_INV_FIELD_IN_CMD_PACKET);
        return VINF_SUCCESS;
    }
    memset(aBuf, '\0', 36);
    ataH2BE_U16(aBuf, 34);
    aBuf[2] = 1; /* track number (LSB) */
    aBuf[3] = 1; /* session number (LSB) */
    aBuf[5] = (0 << 5) | (0 << 4) | (4 << 0); /* not damaged, primary copy, data track */
    aBuf[6] = (0 << 7) | (0 << 6) | (0 << 5) | (0 << 6) | (1 << 0); /* not reserved track, not blank, not packet writing, not fixed packet, data mode 1 */
    aBuf[7] = (0 << 1) | (0 << 0); /* last recorded address not valid, next recordable address not valid */
    ataH2BE_U32(aBuf + 8, 0); /* track start address is 0 */
    ataH2BE_U32(aBuf + 24, pAhciPort->cTotalSectors); /* track size */
    aBuf[32] = 0; /* track number (MSB) */
    aBuf[33] = 0; /* session number (MSB) */

    /* Copy the buffer in to the scatter gather list. */
    *pcbData = ahciScatterGatherListCopyFromBuffer(pAhciPortTaskState, (void *)&aBuf[0], sizeof(aBuf));

    atapiCmdOK(pAhciPort, pAhciPortTaskState);
    return VINF_SUCCESS;
}


static int atapiGetConfigurationSS(PAHCIPORTTASKSTATE pAhciPortTaskState, PAHCIPort pAhciPort, int *pcbData)
{
    uint8_t aBuf[32];

    /* Accept valid request types only, and only starting feature 0. */
    if ((pAhciPortTaskState->aATAPICmd[1] & 0x03) == 3 || ataBE2H_U16(&pAhciPortTaskState->aATAPICmd[2]) != 0)
    {
        atapiCmdError(pAhciPort, pAhciPortTaskState, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_INV_FIELD_IN_CMD_PACKET);
        return VINF_SUCCESS;
    }
    memset(aBuf, '\0', 32);
    ataH2BE_U32(aBuf, 16);
    /** @todo implement switching between CD-ROM and DVD-ROM profile (the only
     * way to differentiate them right now is based on the image size). Also
     * implement signalling "no current profile" if no medium is loaded. */
    ataH2BE_U16(aBuf + 6, 0x08); /* current profile: read-only CD */

    ataH2BE_U16(aBuf + 8, 0); /* feature 0: list of profiles supported */
    aBuf[10] = (0 << 2) | (1 << 1) | (1 || 0); /* version 0, persistent, current */
    aBuf[11] = 8; /* additional bytes for profiles */
    /* The MMC-3 spec says that DVD-ROM read capability should be reported
     * before CD-ROM read capability. */
    ataH2BE_U16(aBuf + 12, 0x10); /* profile: read-only DVD */
    aBuf[14] = (0 << 0); /* NOT current profile */
    ataH2BE_U16(aBuf + 16, 0x08); /* profile: read only CD */
    aBuf[18] = (1 << 0); /* current profile */

    /* Copy the buffer in to the scatter gather list. */
    *pcbData = ahciScatterGatherListCopyFromBuffer(pAhciPortTaskState, (void *)&aBuf[0], sizeof(aBuf));

    atapiCmdOK(pAhciPort, pAhciPortTaskState);
    return VINF_SUCCESS;
}


static int atapiInquirySS(PAHCIPORTTASKSTATE pAhciPortTaskState, PAHCIPort pAhciPort, int *pcbData)
{
    uint8_t aBuf[36];

    aBuf[0] = 0x05; /* CD-ROM */
    aBuf[1] = 0x80; /* removable */
    aBuf[2] = 0x00; /* ISO */
    aBuf[3] = 0x21; /* ATAPI-2 (XXX: put ATAPI-4 ?) */
    aBuf[4] = 31; /* additional length */
    aBuf[5] = 0; /* reserved */
    aBuf[6] = 0; /* reserved */
    aBuf[7] = 0; /* reserved */
    ataSCSIPadStr(aBuf +  8, pAhciPort->szInquiryVendorId, 8);
    ataSCSIPadStr(aBuf + 16, pAhciPort->szInquiryProductId, 16);
    ataSCSIPadStr(aBuf + 32, pAhciPort->szInquiryRevision, 4);

    /* Copy the buffer in to the scatter gather list. */
    *pcbData = ahciScatterGatherListCopyFromBuffer(pAhciPortTaskState, (void *)&aBuf[0], sizeof(aBuf));

    atapiCmdOK(pAhciPort, pAhciPortTaskState);
    return VINF_SUCCESS;
}


static int atapiModeSenseErrorRecoverySS(PAHCIPORTTASKSTATE pAhciPortTaskState, PAHCIPort pAhciPort, int *pcbData)
{
    uint8_t aBuf[16];

    ataH2BE_U16(&aBuf[0], 16 + 6);
    aBuf[2] = 0x70;
    aBuf[3] = 0;
    aBuf[4] = 0;
    aBuf[5] = 0;
    aBuf[6] = 0;
    aBuf[7] = 0;

    aBuf[8] = 0x01;
    aBuf[9] = 0x06;
    aBuf[10] = 0x00;
    aBuf[11] = 0x05;
    aBuf[12] = 0x00;
    aBuf[13] = 0x00;
    aBuf[14] = 0x00;
    aBuf[15] = 0x00;

    /* Copy the buffer in to the scatter gather list. */
    *pcbData = ahciScatterGatherListCopyFromBuffer(pAhciPortTaskState, (void *)&aBuf[0], sizeof(aBuf));

    atapiCmdOK(pAhciPort, pAhciPortTaskState);
    return VINF_SUCCESS;
}


static int atapiModeSenseCDStatusSS(PAHCIPORTTASKSTATE pAhciPortTaskState, PAHCIPort pAhciPort, int *pcbData)
{
    uint8_t aBuf[40];

    ataH2BE_U16(&aBuf[0], 38);
    aBuf[2] = 0x70;
    aBuf[3] = 0;
    aBuf[4] = 0;
    aBuf[5] = 0;
    aBuf[6] = 0;
    aBuf[7] = 0;

    aBuf[8] = 0x2a;
    aBuf[9] = 30; /* page length */
    aBuf[10] = 0x08; /* DVD-ROM read support */
    aBuf[11] = 0x00; /* no write support */
    /* The following claims we support audio play. This is obviously false,
     * but the Linux generic CDROM support makes many features depend on this
     * capability. If it's not set, this causes many things to be disabled. */
    aBuf[12] = 0x71; /* multisession support, mode 2 form 1/2 support, audio play */
    aBuf[13] = 0x00; /* no subchannel reads supported */
    aBuf[14] = (1 << 0) | (1 << 3) | (1 << 5); /* lock supported, eject supported, tray type loading mechanism */
    if (pAhciPort->pDrvMount->pfnIsLocked(pAhciPort->pDrvMount))
        aBuf[14] |= 1 << 1; /* report lock state */
    aBuf[15] = 0; /* no subchannel reads supported, no separate audio volume control, no changer etc. */
    ataH2BE_U16(&aBuf[16], 5632); /* (obsolete) claim 32x speed support */
    ataH2BE_U16(&aBuf[18], 2); /* number of audio volume levels */
    ataH2BE_U16(&aBuf[20], 128); /* buffer size supported in Kbyte - We don't have a buffer because we write directly into guest memory.
                                    Just write the value DevATA is using. */
    ataH2BE_U16(&aBuf[22], 5632); /* (obsolete) current read speed 32x */
    aBuf[24] = 0; /* reserved */
    aBuf[25] = 0; /* reserved for digital audio (see idx 15) */
    ataH2BE_U16(&aBuf[26], 0); /* (obsolete) maximum write speed */
    ataH2BE_U16(&aBuf[28], 0); /* (obsolete) current write speed */
    ataH2BE_U16(&aBuf[30], 0); /* copy management revision supported 0=no CSS */
    aBuf[32] = 0; /* reserved */
    aBuf[33] = 0; /* reserved */
    aBuf[34] = 0; /* reserved */
    aBuf[35] = 1; /* rotation control CAV */
    ataH2BE_U16(&aBuf[36], 0); /* current write speed */
    ataH2BE_U16(&aBuf[38], 0); /* number of write speed performance descriptors */

    /* Copy the buffer in to the scatter gather list. */
    *pcbData = ahciScatterGatherListCopyFromBuffer(pAhciPortTaskState, (void *)&aBuf[0], sizeof(aBuf));

    atapiCmdOK(pAhciPort, pAhciPortTaskState);
    return VINF_SUCCESS;
}


static int atapiRequestSenseSS(PAHCIPORTTASKSTATE pAhciPortTaskState, PAHCIPort pAhciPort, int *pcbData)
{
    uint8_t aBuf[18];

    memset(&aBuf[0], 0, 18);
    aBuf[0] = 0x70 | (1 << 7);
    aBuf[2] = pAhciPort->uATAPISenseKey;
    aBuf[7] = 10;
    aBuf[12] = pAhciPort->uATAPIASC;

    /* Copy the buffer in to the scatter gather list. */
    *pcbData = ahciScatterGatherListCopyFromBuffer(pAhciPortTaskState, (void *)&aBuf[0], sizeof(aBuf));

    atapiCmdOK(pAhciPort, pAhciPortTaskState);
    return VINF_SUCCESS;
}


static int atapiMechanismStatusSS(PAHCIPORTTASKSTATE pAhciPortTaskState, PAHCIPort pAhciPort, int *pcbData)
{
    uint8_t aBuf[8];

    ataH2BE_U16(&aBuf[0], 0);
    /* no current LBA */
    aBuf[2] = 0;
    aBuf[3] = 0;
    aBuf[4] = 0;
    aBuf[5] = 1;
    ataH2BE_U16(aBuf + 6, 0);

    /* Copy the buffer in to the scatter gather list. */
    *pcbData = ahciScatterGatherListCopyFromBuffer(pAhciPortTaskState, (void *)&aBuf[0], sizeof(aBuf));

    atapiCmdOK(pAhciPort, pAhciPortTaskState);
    return VINF_SUCCESS;
}


static int atapiReadTOCNormalSS(PAHCIPORTTASKSTATE pAhciPortTaskState, PAHCIPort pAhciPort, int *pcbData)
{
    uint8_t aBuf[20], *q, iStartTrack;
    bool fMSF;
    uint32_t cbSize;

    fMSF = (pAhciPortTaskState->aATAPICmd[1] >> 1) & 1;
    iStartTrack = pAhciPortTaskState->aATAPICmd[6];
    if (iStartTrack > 1 && iStartTrack != 0xaa)
    {
        atapiCmdError(pAhciPort, pAhciPortTaskState, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_INV_FIELD_IN_CMD_PACKET);
        return VINF_SUCCESS;
    }
    q = aBuf + 2;
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
        ataLBA2MSF(q, pAhciPort->cTotalSectors);
        q += 3;
    }
    else
    {
        ataH2BE_U32(q, pAhciPort->cTotalSectors);
        q += 4;
    }
    cbSize = q - aBuf;
    ataH2BE_U16(aBuf, cbSize - 2);

    /* Copy the buffer in to the scatter gather list. */
    *pcbData = ahciScatterGatherListCopyFromBuffer(pAhciPortTaskState, (void *)&aBuf[0], cbSize);

    atapiCmdOK(pAhciPort, pAhciPortTaskState);
    return VINF_SUCCESS;
}


static int atapiReadTOCMultiSS(PAHCIPORTTASKSTATE pAhciPortTaskState, PAHCIPort pAhciPort, int *pcbData)
{
    uint8_t aBuf[12];
    bool fMSF;

    fMSF = (pAhciPortTaskState->aATAPICmd[1] >> 1) & 1;
    /* multi session: only a single session defined */
/** @todo double-check this stuff against what a real drive says for a CD-ROM (not a CD-R) with only a single data session. Maybe solve the problem with "cdrdao read-toc" not being able to figure out whether numbers are in BCD or hex. */
    memset(aBuf, 0, 12);
    aBuf[1] = 0x0a;
    aBuf[2] = 0x01;
    aBuf[3] = 0x01;
    aBuf[5] = 0x14; /* ADR, control */
    aBuf[6] = 1; /* first track in last complete session */
    if (fMSF)
    {
        aBuf[8] = 0; /* reserved */
        ataLBA2MSF(&aBuf[9], 0);
    }
    else
    {
        /* sector 0 */
        ataH2BE_U32(aBuf + 8, 0);
    }

    /* Copy the buffer in to the scatter gather list. */
    *pcbData = ahciScatterGatherListCopyFromBuffer(pAhciPortTaskState, (void *)&aBuf[0], sizeof(aBuf));

    atapiCmdOK(pAhciPort, pAhciPortTaskState);
    return VINF_SUCCESS;
}


static int atapiReadTOCRawSS(PAHCIPORTTASKSTATE pAhciPortTaskState, PAHCIPort pAhciPort, int *pcbData)
{
    uint8_t aBuf[50]; /* Counted a maximum of 45 bytes but better be on the safe side. */
    uint8_t *q, iStartTrack;
    bool fMSF;
    uint32_t cbSize;

    fMSF = (pAhciPortTaskState->aATAPICmd[1] >> 1) & 1;
    iStartTrack = pAhciPortTaskState->aATAPICmd[6];

    q = aBuf + 2;
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
        ataLBA2MSF(q, pAhciPort->cTotalSectors);
        q += 3;
    }
    else
    {
        ataH2BE_U32(q, pAhciPort->cTotalSectors);
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

    cbSize = q - aBuf;
    ataH2BE_U16(aBuf, cbSize - 2);

    /* Copy the buffer in to the scatter gather list. */
    *pcbData = ahciScatterGatherListCopyFromBuffer(pAhciPortTaskState, (void *)&aBuf[0], cbSize);

    atapiCmdOK(pAhciPort, pAhciPortTaskState);
    return VINF_SUCCESS;
}

static int atapiDoTransfer(PAHCIPort pAhciPort, PAHCIPORTTASKSTATE pAhciPortTaskState, ATAPIFN iSourceSink)
{
    int cbTransfered;
    int rc, rcSourceSink;

    /*
     * Create scatter gather list. We use a safe mapping here because it is
     * possible that the buffer is not a multiple of 512. The normal
     * creator would assert later here.
     */
    ahciScatterGatherListGetTotalBufferSize(pAhciPort, pAhciPortTaskState);
    rc = ahciScatterGatherListCreateSafe(pAhciPort, pAhciPortTaskState, false, 0);
    AssertRC(rc);

    rcSourceSink = g_apfnAtapiFuncs[iSourceSink](pAhciPortTaskState, pAhciPort, &cbTransfered);

    pAhciPortTaskState->cmdHdr.u32PRDBC = cbTransfered;

    rc = ahciScatterGatherListDestroy(pAhciPort, pAhciPortTaskState);
    AssertRC(rc);

    /* Write updated command header into memory of the guest. */
    PDMDevHlpPhysWrite(pAhciPort->CTX_SUFF(pDevIns), pAhciPortTaskState->GCPhysCmdHdrAddr, &pAhciPortTaskState->cmdHdr, sizeof(CmdHdr));

    return rcSourceSink;
}

static int atapiReadSectors2352PostProcess(PAHCIPORTTASKSTATE pAhciPortTaskState)
{
    uint32_t cSectors  = pAhciPortTaskState->cbTransfer / 2048;
    uint32_t iATAPILBA = pAhciPortTaskState->uOffset / 2048;
    uint8_t *pbBufDst  = (uint8_t *)pAhciPortTaskState->pvBufferUnaligned;
    uint8_t *pbBufSrc  = (uint8_t *)pAhciPortTaskState->pSGListHead[0].pvSeg;

    for (uint32_t i = iATAPILBA; i < iATAPILBA + cSectors; i++)
    {
        /* sync bytes */
        *pbBufDst++ = 0x00;
        memset(pbBufDst, 0xff, 11);
        pbBufDst += 11;
        /* MSF */
        ataLBA2MSF(pbBufDst, i);
        pbBufDst += 3;
        *pbBufDst++ = 0x01; /* mode 1 data */
        /* data */
        memcpy(pbBufDst, pbBufSrc, 2048);
        pbBufDst += 2048;
        pbBufSrc += 2048;
        /* ECC */
        memset(pbBufDst, 0, 288);
        pbBufDst += 288;
    }

    return VINF_SUCCESS;
}

static int atapiReadSectors(PAHCIPort pAhciPort, PAHCIPORTTASKSTATE pAhciPortTaskState, uint32_t iATAPILBA, uint32_t cSectors, uint32_t cbSector)
{
    Log(("%s: %d sectors at LBA %d\n", __FUNCTION__, cSectors, iATAPILBA));

    switch (cbSector)
    {
        case 2048:
            pAhciPortTaskState->uOffset = iATAPILBA * cbSector;
            pAhciPortTaskState->cbTransfer = cSectors * cbSector;
            break;
        case 2352:
        {
            pAhciPortTaskState->pfnPostProcess = atapiReadSectors2352PostProcess;
            pAhciPortTaskState->uOffset = iATAPILBA * 2048;
            pAhciPortTaskState->cbTransfer = cSectors * 2048;
            break;
        }
        default:
            AssertMsgFailed(("Unsupported sectors size\n"));
            break;
    }

    return VINF_SUCCESS;
}

static int atapiParseCmdVirtualATAPI(PAHCIPort pAhciPort, PAHCIPORTTASKSTATE pAhciPortTaskState)
{
    int rc = PDMBLOCKTXDIR_NONE;
    const uint8_t *pbPacket;
    uint32_t cbMax;

    pbPacket = pAhciPortTaskState->aATAPICmd;

    ahciLog(("%s: ATAPI CMD=%#04x \"%s\"\n", __FUNCTION__, pbPacket[0], SCSICmdText(pbPacket[0])));

    switch (pbPacket[0])
    {
        case SCSI_TEST_UNIT_READY:
            if (pAhciPort->cNotifiedMediaChange > 0)
            {
                if (pAhciPort->cNotifiedMediaChange-- > 2)
                    atapiCmdError(pAhciPort, pAhciPortTaskState, SCSI_SENSE_NOT_READY, SCSI_ASC_MEDIUM_NOT_PRESENT);
                else
                    atapiCmdError(pAhciPort, pAhciPortTaskState, SCSI_SENSE_UNIT_ATTENTION, SCSI_ASC_MEDIUM_MAY_HAVE_CHANGED); /* media changed */
            }
            else if (pAhciPort->pDrvMount->pfnIsMounted(pAhciPort->pDrvMount))
                atapiCmdOK(pAhciPort, pAhciPortTaskState);
            else
                atapiCmdError(pAhciPort, pAhciPortTaskState, SCSI_SENSE_NOT_READY, SCSI_ASC_MEDIUM_NOT_PRESENT);
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
                                atapiDoTransfer(pAhciPort, pAhciPortTaskState, ATAFN_SS_ATAPI_MODE_SENSE_ERROR_RECOVERY);
                                break;
                            case SCSI_MODEPAGE_CD_STATUS:
                                atapiDoTransfer(pAhciPort, pAhciPortTaskState, ATAFN_SS_ATAPI_MODE_SENSE_CD_STATUS);
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
                        atapiCmdError(pAhciPort, pAhciPortTaskState, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_SAVING_PARAMETERS_NOT_SUPPORTED);
                        break;
                }
            }
            break;
        case SCSI_REQUEST_SENSE:
            cbMax = pbPacket[4];
            atapiDoTransfer(pAhciPort, pAhciPortTaskState, ATAFN_SS_ATAPI_REQUEST_SENSE);
            break;
        case SCSI_PREVENT_ALLOW_MEDIUM_REMOVAL:
            if (pAhciPort->pDrvMount->pfnIsMounted(pAhciPort->pDrvMount))
            {
                if (pbPacket[4] & 1)
                    pAhciPort->pDrvMount->pfnLock(pAhciPort->pDrvMount);
                else
                    pAhciPort->pDrvMount->pfnUnlock(pAhciPort->pDrvMount);
                atapiCmdOK(pAhciPort, pAhciPortTaskState);
            }
            else
                atapiCmdError(pAhciPort, pAhciPortTaskState, SCSI_SENSE_NOT_READY, SCSI_ASC_MEDIUM_NOT_PRESENT);
            break;
        case SCSI_READ_10:
        case SCSI_READ_12:
            {
                uint32_t cSectors, iATAPILBA;

                if (pAhciPort->cNotifiedMediaChange > 0)
                {
                    pAhciPort->cNotifiedMediaChange-- ;
                    atapiCmdError(pAhciPort, pAhciPortTaskState, SCSI_SENSE_UNIT_ATTENTION, SCSI_ASC_MEDIUM_MAY_HAVE_CHANGED); /* media changed */
                    break;
                }
                else if (!pAhciPort->pDrvMount->pfnIsMounted(pAhciPort->pDrvMount))
                {
                    atapiCmdError(pAhciPort, pAhciPortTaskState, SCSI_SENSE_NOT_READY, SCSI_ASC_MEDIUM_NOT_PRESENT);
                    break;
                }
                if (pbPacket[0] == SCSI_READ_10)
                    cSectors = ataBE2H_U16(pbPacket + 7);
                else
                    cSectors = ataBE2H_U32(pbPacket + 6);
                iATAPILBA = ataBE2H_U32(pbPacket + 2);
                if (cSectors == 0)
                {
                    atapiCmdOK(pAhciPort, pAhciPortTaskState);
                    break;
                }
                if ((uint64_t)iATAPILBA + cSectors > pAhciPort->cTotalSectors)
                {
                    /* Rate limited logging, one log line per second. For
                     * guests that insist on reading from places outside the
                     * valid area this often generates too many release log
                     * entries otherwise. */
                    static uint64_t uLastLogTS = 0;
                    if (RTTimeMilliTS() >= uLastLogTS + 1000)
                    {
                        LogRel(("AHCI ATAPI: LUN#%d: CD-ROM block number %Ld invalid (READ)\n", pAhciPort->iLUN, (uint64_t)iATAPILBA + cSectors));
                        uLastLogTS = RTTimeMilliTS();
                    }
                    atapiCmdError(pAhciPort, pAhciPortTaskState, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_LOGICAL_BLOCK_OOR);
                    break;
                }
                atapiReadSectors(pAhciPort, pAhciPortTaskState, iATAPILBA, cSectors, 2048);
                rc = PDMBLOCKTXDIR_FROM_DEVICE;
            }
            break;
        case SCSI_READ_CD:
            {
                uint32_t cSectors, iATAPILBA;

                if (pAhciPort->cNotifiedMediaChange > 0)
                {
                    pAhciPort->cNotifiedMediaChange-- ;
                    atapiCmdError(pAhciPort, pAhciPortTaskState, SCSI_SENSE_UNIT_ATTENTION, SCSI_ASC_MEDIUM_MAY_HAVE_CHANGED); /* media changed */
                    break;
                }
                else if (!pAhciPort->pDrvMount->pfnIsMounted(pAhciPort->pDrvMount))
                {
                    atapiCmdError(pAhciPort, pAhciPortTaskState, SCSI_SENSE_NOT_READY, SCSI_ASC_MEDIUM_NOT_PRESENT);
                    break;
                }
                cSectors = (pbPacket[6] << 16) | (pbPacket[7] << 8) | pbPacket[8];
                iATAPILBA = ataBE2H_U32(pbPacket + 2);
                if (cSectors == 0)
                {
                    atapiCmdOK(pAhciPort, pAhciPortTaskState);
                    break;
                }
                if ((uint64_t)iATAPILBA + cSectors > pAhciPort->cTotalSectors)
                {
                    /* Rate limited logging, one log line per second. For
                     * guests that insist on reading from places outside the
                     * valid area this often generates too many release log
                     * entries otherwise. */
                    static uint64_t uLastLogTS = 0;
                    if (RTTimeMilliTS() >= uLastLogTS + 1000)
                    {
                        LogRel(("AHCI ATA: LUN#%d: CD-ROM block number %Ld invalid (READ CD)\n", pAhciPort->iLUN, (uint64_t)iATAPILBA + cSectors));
                        uLastLogTS = RTTimeMilliTS();
                    }
                    atapiCmdError(pAhciPort, pAhciPortTaskState, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_LOGICAL_BLOCK_OOR);
                    break;
                }
                switch (pbPacket[9] & 0xf8)
                {
                    case 0x00:
                        /* nothing */
                        atapiCmdOK(pAhciPort, pAhciPortTaskState);
                        break;
                    case 0x10:
                        /* normal read */
                        atapiReadSectors(pAhciPort, pAhciPortTaskState, iATAPILBA, cSectors, 2048);
                        rc = PDMBLOCKTXDIR_FROM_DEVICE;
                        break;
                    case 0xf8:
                        /* read all data */
                        atapiReadSectors(pAhciPort, pAhciPortTaskState, iATAPILBA, cSectors, 2352);
                        rc = PDMBLOCKTXDIR_FROM_DEVICE;
                        break;
                    default:
                        LogRel(("AHCI ATAPI: LUN#%d: CD-ROM sector format not supported\n", pAhciPort->iLUN));
                        atapiCmdError(pAhciPort, pAhciPortTaskState, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_INV_FIELD_IN_CMD_PACKET);
                        break;
                }
            }
            break;
        case SCSI_SEEK_10:
            {
                uint32_t iATAPILBA;
                if (pAhciPort->cNotifiedMediaChange > 0)
                {
                    pAhciPort->cNotifiedMediaChange-- ;
                    atapiCmdError(pAhciPort, pAhciPortTaskState, SCSI_SENSE_UNIT_ATTENTION, SCSI_ASC_MEDIUM_MAY_HAVE_CHANGED); /* media changed */
                    break;
                }
                else if (!pAhciPort->pDrvMount->pfnIsMounted(pAhciPort->pDrvMount))
                {
                    atapiCmdError(pAhciPort, pAhciPortTaskState, SCSI_SENSE_NOT_READY, SCSI_ASC_MEDIUM_NOT_PRESENT);
                    break;
                }
                iATAPILBA = ataBE2H_U32(pbPacket + 2);
                if (iATAPILBA > pAhciPort->cTotalSectors)
                {
                    /* Rate limited logging, one log line per second. For
                     * guests that insist on seeking to places outside the
                     * valid area this often generates too many release log
                     * entries otherwise. */
                    static uint64_t uLastLogTS = 0;
                    if (RTTimeMilliTS() >= uLastLogTS + 1000)
                    {
                        LogRel(("AHCI ATAPI: LUN#%d: CD-ROM block number %Ld invalid (SEEK)\n", pAhciPort->iLUN, (uint64_t)iATAPILBA));
                        uLastLogTS = RTTimeMilliTS();
                    }
                    atapiCmdError(pAhciPort, pAhciPortTaskState, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_LOGICAL_BLOCK_OOR);
                    break;
                }
                atapiCmdOK(pAhciPort, pAhciPortTaskState);
                pAhciPortTaskState->uATARegStatus |= ATA_STAT_SEEK; /* Linux expects this. */
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
                            PAHCI pAhci = pAhciPort->CTX_SUFF(pAhci);
                            PPDMDEVINS pDevIns = pAhci->CTX_SUFF(pDevIns);

                            rc = VMR3ReqCallWait(PDMDevHlpGetVM(pDevIns), VMCPUID_ANY,
                                                 (PFNRT)pAhciPort->pDrvMount->pfnUnmount, 2, pAhciPort->pDrvMount, false);
                            Assert(RT_SUCCESS(rc) || (rc == VERR_PDM_MEDIA_LOCKED));
                        }
                        break;
                    case 3: /* 11 - Load media */
                        /** @todo rc = s->pDrvMount->pfnLoadMedia(s->pDrvMount) */
                        break;
                }
                if (RT_SUCCESS(rc))
                    atapiCmdOK(pAhciPort, pAhciPortTaskState);
                else
                    atapiCmdError(pAhciPort, pAhciPortTaskState, SCSI_SENSE_NOT_READY, SCSI_ASC_MEDIA_LOAD_OR_EJECT_FAILED);
            }
            break;
        case SCSI_MECHANISM_STATUS:
            {
                cbMax = ataBE2H_U16(pbPacket + 8);
                atapiDoTransfer(pAhciPort, pAhciPortTaskState, ATAFN_SS_ATAPI_MECHANISM_STATUS);
            }
            break;
        case SCSI_READ_TOC_PMA_ATIP:
            {
                uint8_t format;

                if (pAhciPort->cNotifiedMediaChange > 0)
                {
                    pAhciPort->cNotifiedMediaChange-- ;
                    atapiCmdError(pAhciPort, pAhciPortTaskState, SCSI_SENSE_UNIT_ATTENTION, SCSI_ASC_MEDIUM_MAY_HAVE_CHANGED); /* media changed */
                    break;
                }
                else if (!pAhciPort->pDrvMount->pfnIsMounted(pAhciPort->pDrvMount))
                {
                    atapiCmdError(pAhciPort, pAhciPortTaskState, SCSI_SENSE_NOT_READY, SCSI_ASC_MEDIUM_NOT_PRESENT);
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
                        atapiDoTransfer(pAhciPort, pAhciPortTaskState, ATAFN_SS_ATAPI_READ_TOC_NORMAL);
                        break;
                    case 1:
                        atapiDoTransfer(pAhciPort, pAhciPortTaskState, ATAFN_SS_ATAPI_READ_TOC_MULTI);
                        break;
                    case 2:
                        atapiDoTransfer(pAhciPort, pAhciPortTaskState, ATAFN_SS_ATAPI_READ_TOC_RAW);
                        break;
                    default:
                    error_cmd:
                        atapiCmdError(pAhciPort, pAhciPortTaskState, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_INV_FIELD_IN_CMD_PACKET);
                        break;
                }
            }
            break;
        case SCSI_READ_CAPACITY:
            if (pAhciPort->cNotifiedMediaChange > 0)
            {
                pAhciPort->cNotifiedMediaChange-- ;
                atapiCmdError(pAhciPort, pAhciPortTaskState, SCSI_SENSE_UNIT_ATTENTION, SCSI_ASC_MEDIUM_MAY_HAVE_CHANGED); /* media changed */
                break;
            }
            else if (!pAhciPort->pDrvMount->pfnIsMounted(pAhciPort->pDrvMount))
            {
                atapiCmdError(pAhciPort, pAhciPortTaskState, SCSI_SENSE_NOT_READY, SCSI_ASC_MEDIUM_NOT_PRESENT);
                break;
            }
            atapiDoTransfer(pAhciPort, pAhciPortTaskState, ATAFN_SS_ATAPI_READ_CAPACITY);
            break;
        case SCSI_READ_DISC_INFORMATION:
            if (pAhciPort->cNotifiedMediaChange > 0)
            {
                pAhciPort->cNotifiedMediaChange-- ;
                atapiCmdError(pAhciPort, pAhciPortTaskState, SCSI_SENSE_UNIT_ATTENTION, SCSI_ASC_MEDIUM_MAY_HAVE_CHANGED); /* media changed */
                break;
            }
            else if (!pAhciPort->pDrvMount->pfnIsMounted(pAhciPort->pDrvMount))
            {
                atapiCmdError(pAhciPort, pAhciPortTaskState, SCSI_SENSE_NOT_READY, SCSI_ASC_MEDIUM_NOT_PRESENT);
                break;
            }
            cbMax = ataBE2H_U16(pbPacket + 7);
            atapiDoTransfer(pAhciPort, pAhciPortTaskState, ATAFN_SS_ATAPI_READ_DISC_INFORMATION);
            break;
        case SCSI_READ_TRACK_INFORMATION:
            if (pAhciPort->cNotifiedMediaChange > 0)
            {
                pAhciPort->cNotifiedMediaChange-- ;
                atapiCmdError(pAhciPort, pAhciPortTaskState, SCSI_SENSE_UNIT_ATTENTION, SCSI_ASC_MEDIUM_MAY_HAVE_CHANGED); /* media changed */
                break;
            }
            else if (!pAhciPort->pDrvMount->pfnIsMounted(pAhciPort->pDrvMount))
            {
                atapiCmdError(pAhciPort, pAhciPortTaskState, SCSI_SENSE_NOT_READY, SCSI_ASC_MEDIUM_NOT_PRESENT);
                break;
            }
            cbMax = ataBE2H_U16(pbPacket + 7);
            atapiDoTransfer(pAhciPort, pAhciPortTaskState, ATAFN_SS_ATAPI_READ_TRACK_INFORMATION);
            break;
        case SCSI_GET_CONFIGURATION:
            /* No media change stuff here, it can confuse Linux guests. */
            cbMax = ataBE2H_U16(pbPacket + 7);
            atapiDoTransfer(pAhciPort, pAhciPortTaskState, ATAFN_SS_ATAPI_GET_CONFIGURATION);
            break;
        case SCSI_INQUIRY:
            cbMax = pbPacket[4];
            atapiDoTransfer(pAhciPort, pAhciPortTaskState, ATAFN_SS_ATAPI_INQUIRY);
            break;
        default:
            atapiCmdError(pAhciPort, pAhciPortTaskState, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_ILLEGAL_OPCODE);
            break;
    }

    return rc;
}

/**
 * Reset all values after a reset of the attached storage device.
 *
 * @returns nothing
 * @param pAhciPort             The port the device is attached to.
 * @param pAhciPortTaskState    The state to get the tag number from.
 */
static void ahciFinishStorageDeviceReset(PAHCIPort pAhciPort, PAHCIPORTTASKSTATE pAhciPortTaskState)
{
    /* Send a status good D2H FIS. */
    pAhciPort->fResetDevice = false;
    if (pAhciPort->regCMD & AHCI_PORT_CMD_FRE)
        ahciPostFirstD2HFisIntoMemory(pAhciPort);

    /* As this is the first D2H FIS after the reset update the signature in the SIG register of the port. */
    pAhciPort->regSIG  = 0x101;
    ASMAtomicOrU32(&pAhciPort->u32TasksFinished, (1 << pAhciPortTaskState->uTag));

    ahciHbaSetInterrupt(pAhciPort->CTX_SUFF(pAhci), pAhciPort->iLUN);
}

/**
 * Build a D2H FIS and post into the memory area of the guest.
 *
 * @returns Nothing
 * @param   pAhciPort          The port of the SATA controller.
 * @param   pAhciPortTaskState The state of the task.
 * @param   pCmdFis            Pointer to the command FIS from the guest.
 * @param   fInterrupt         If an interrupt should be send to the guest.
 */
static void ahciSendD2HFis(PAHCIPort pAhciPort, PAHCIPORTTASKSTATE pAhciPortTaskState, uint8_t *pCmdFis, bool fInterrupt)
{
    uint8_t d2hFis[20];
    bool fAssertIntr = false;
    PAHCI pAhci = pAhciPort->CTX_SUFF(pAhci);

    ahciLog(("%s: building D2H Fis\n", __FUNCTION__));

    if (pAhciPort->regCMD & AHCI_PORT_CMD_FRE)
    {
        memset(&d2hFis[0], 0, sizeof(d2hFis));
        d2hFis[AHCI_CMDFIS_TYPE]  = AHCI_CMDFIS_TYPE_D2H;
        d2hFis[AHCI_CMDFIS_BITS]  = (fInterrupt ? AHCI_CMDFIS_I : 0);
        d2hFis[AHCI_CMDFIS_STS]   = pAhciPortTaskState->uATARegStatus;
        d2hFis[AHCI_CMDFIS_ERR]   = pAhciPortTaskState->uATARegError;
        d2hFis[AHCI_CMDFIS_SECTN] = pCmdFis[AHCI_CMDFIS_SECTN];
        d2hFis[AHCI_CMDFIS_CYLL]  = pCmdFis[AHCI_CMDFIS_CYLL];
        d2hFis[AHCI_CMDFIS_CYLH]  = pCmdFis[AHCI_CMDFIS_CYLH];
        d2hFis[AHCI_CMDFIS_HEAD]  = pCmdFis[AHCI_CMDFIS_HEAD];
        d2hFis[AHCI_CMDFIS_SECTNEXP] = pCmdFis[AHCI_CMDFIS_SECTNEXP];
        d2hFis[AHCI_CMDFIS_CYLLEXP]  = pCmdFis[AHCI_CMDFIS_CYLLEXP];
        d2hFis[AHCI_CMDFIS_CYLHEXP]  = pCmdFis[AHCI_CMDFIS_CYLHEXP];
        d2hFis[AHCI_CMDFIS_SECTC]    = pCmdFis[AHCI_CMDFIS_SECTC];
        d2hFis[AHCI_CMDFIS_SECTCEXP] = pCmdFis[AHCI_CMDFIS_SECTCEXP];

        /* Update registers. */
        pAhciPort->regTFD = (pAhciPortTaskState->uATARegError << 8) | pAhciPortTaskState->uATARegStatus;

        ahciPostFisIntoMemory(pAhciPort, AHCI_CMDFIS_TYPE_D2H, d2hFis);

        if (pAhciPortTaskState->uATARegStatus & ATA_STAT_ERR)
        {
            /* Error bit is set. */
            ASMAtomicOrU32(&pAhciPort->regIS, AHCI_PORT_IS_TFES);
            if (pAhciPort->regIE & AHCI_PORT_IE_TFEE)
                fAssertIntr = true;
        }

        if (fInterrupt)
        {
            ASMAtomicOrU32(&pAhciPort->regIS, AHCI_PORT_IS_DHRS);
            /* Check if we should assert an interrupt */
            if (pAhciPort->regIE & AHCI_PORT_IE_DHRE)
                fAssertIntr = true;
        }

        ASMAtomicOrU32(&pAhciPort->u32TasksFinished, (1 << pAhciPortTaskState->uTag));

        if (fAssertIntr)
            ahciHbaSetInterrupt(pAhci, pAhciPort->iLUN);
    }
}

/**
 * Build a SDB Fis and post it into the memory area of the guest.
 *
 * @returns Nothing
 * @param   pAhciPort           The port for which the SDB Fis is send.
 * @param   uFinishedTasks      Bitmask of finished tasks.
 * @param   pAhciPortTaskState  The state of the last task.
 * @param   fInterrupt          If an interrupt should be asserted.
 */
static void ahciSendSDBFis(PAHCIPort pAhciPort, uint32_t uFinishedTasks, PAHCIPORTTASKSTATE pAhciPortTaskState, bool fInterrupt)
{
    uint32_t sdbFis[2];
    bool fAssertIntr = false;
    PAHCI pAhci = pAhciPort->CTX_SUFF(pAhci);

    ahciLog(("%s: Building SDB FIS\n", __FUNCTION__));

    if (pAhciPort->regCMD & AHCI_PORT_CMD_FRE)
    {
        memset(&sdbFis[0], 0, sizeof(sdbFis));
        sdbFis[0] = AHCI_CMDFIS_TYPE_SETDEVBITS;
        sdbFis[0] |= (fInterrupt ? (1 << 14) : 0);
        sdbFis[0] |= pAhciPortTaskState->uATARegError << 24;
        sdbFis[0] |= (pAhciPortTaskState->uATARegStatus & 0x77) << 16; /* Some bits are marked as reserved and thus are masked out. */
        sdbFis[1] = uFinishedTasks;

        ahciPostFisIntoMemory(pAhciPort, AHCI_CMDFIS_TYPE_SETDEVBITS, (uint8_t *)sdbFis);

        if (pAhciPortTaskState->uATARegStatus & ATA_STAT_ERR)
        {
            /* Error bit is set. */
            ASMAtomicOrU32(&pAhciPort->regIS, AHCI_PORT_IS_TFES);
            if (pAhciPort->regIE & AHCI_PORT_IE_TFEE)
                fAssertIntr = true;
        }

        if (fInterrupt)
        {
            ASMAtomicOrU32(&pAhciPort->regIS, AHCI_PORT_IS_SDBS);
            /* Check if we should assert an interrupt */
            if (pAhciPort->regIE & AHCI_PORT_IE_SDBE)
                fAssertIntr = true;
        }

        /* Update registers. */
        pAhciPort->regTFD = (pAhciPortTaskState->uATARegError << 8) | pAhciPortTaskState->uATARegStatus;

        ASMAtomicOrU32(&pAhciPort->u32QueuedTasksFinished, uFinishedTasks);

        if (fAssertIntr)
            ahciHbaSetInterrupt(pAhci, pAhciPort->iLUN);
    }
}

static uint32_t ahciGetNSectors(uint8_t *pCmdFis, bool fLBA48)
{
    /* 0 means either 256 (LBA28) or 65536 (LBA48) sectors. */
    if (fLBA48)
    {
        if (!pCmdFis[AHCI_CMDFIS_SECTC] && !pCmdFis[AHCI_CMDFIS_SECTCEXP])
            return 65536;
        else
            return pCmdFis[AHCI_CMDFIS_SECTCEXP] << 8 | pCmdFis[AHCI_CMDFIS_SECTC];
    }
    else
    {
        if (!pCmdFis[AHCI_CMDFIS_SECTC])
            return 256;
        else
            return pCmdFis[AHCI_CMDFIS_SECTC];
    }
}

static uint64_t ahciGetSector(PAHCIPort pAhciPort, uint8_t *pCmdFis, bool fLBA48)
{
    uint64_t iLBA;
    if (pCmdFis[AHCI_CMDFIS_HEAD] & 0x40)
    {
        /* any LBA variant */
        if (fLBA48)
        {
            /* LBA48 */
            iLBA = ((uint64_t)pCmdFis[AHCI_CMDFIS_CYLHEXP] << 40) |
                ((uint64_t)pCmdFis[AHCI_CMDFIS_CYLLEXP] << 32) |
                ((uint64_t)pCmdFis[AHCI_CMDFIS_SECTNEXP] << 24) |
                ((uint64_t)pCmdFis[AHCI_CMDFIS_CYLH] << 16) |
                ((uint64_t)pCmdFis[AHCI_CMDFIS_CYLL] << 8) |
                pCmdFis[AHCI_CMDFIS_SECTN];
        }
        else
        {
            /* LBA */
            iLBA = ((pCmdFis[AHCI_CMDFIS_HEAD] & 0x0f) << 24) | (pCmdFis[AHCI_CMDFIS_CYLH] << 16) |
                (pCmdFis[AHCI_CMDFIS_CYLL] << 8) | pCmdFis[AHCI_CMDFIS_SECTN];
        }
    }
    else
    {
        /* CHS */
        iLBA = ((pCmdFis[AHCI_CMDFIS_CYLH] << 8) | pCmdFis[AHCI_CMDFIS_CYLL]) * pAhciPort->PCHSGeometry.cHeads * pAhciPort->PCHSGeometry.cSectors +
            (pCmdFis[AHCI_CMDFIS_HEAD] & 0x0f) * pAhciPort->PCHSGeometry.cSectors +
            (pCmdFis[AHCI_CMDFIS_SECTN] - 1);
    }
    return iLBA;
}

static uint64_t ahciGetSectorQueued(uint8_t *pCmdFis)
{
    uint64_t uLBA;

    uLBA = ((uint64_t)pCmdFis[AHCI_CMDFIS_CYLHEXP] << 40) |
           ((uint64_t)pCmdFis[AHCI_CMDFIS_CYLLEXP] << 32) |
           ((uint64_t)pCmdFis[AHCI_CMDFIS_SECTNEXP] << 24) |
           ((uint64_t)pCmdFis[AHCI_CMDFIS_CYLH] << 16) |
           ((uint64_t)pCmdFis[AHCI_CMDFIS_CYLL] << 8) |
           pCmdFis[AHCI_CMDFIS_SECTN];

    return uLBA;
}

DECLINLINE(uint32_t) ahciGetNSectorsQueued(uint8_t *pCmdFis)
{
    if (!pCmdFis[AHCI_CMDFIS_FETEXP] && !pCmdFis[AHCI_CMDFIS_FET])
        return 65536;
    else
        return pCmdFis[AHCI_CMDFIS_FETEXP] << 8 | pCmdFis[AHCI_CMDFIS_FET];
}

DECLINLINE(uint8_t) ahciGetTagQueued(uint8_t *pCmdFis)
{
    return pCmdFis[AHCI_CMDFIS_SECTC] >> 3;
}

static void ahciScatterGatherListGetTotalBufferSize(PAHCIPort pAhciPort, PAHCIPORTTASKSTATE pAhciPortTaskState)
{
    CmdHdr    *pCmdHdr = &pAhciPortTaskState->cmdHdr;
    PPDMDEVINS pDevIns = pAhciPort->CTX_SUFF(pDevIns);
    unsigned   cActualSGEntry;
    SGLEntry   aSGLEntry[32];                  /* Holds read sg entries from guest. Biggest seen number of entries a guest set up. */
    unsigned   cSGLEntriesGCRead;
    unsigned   cSGLEntriesGCLeft;              /* Available scatter gather list entries in GC */
    RTGCPHYS   GCPhysAddrPRDTLEntryStart;      /* Start address to read the entries from. */
    uint32_t   cbSGBuffers = 0;                /* Total number of bytes reserved for this request. */

    /* Retrieve the total number of bytes reserved for this request. */
    cSGLEntriesGCLeft = AHCI_CMDHDR_PRDTL_ENTRIES(pCmdHdr->u32DescInf);
    ahciLog(("%s: cSGEntriesGC=%u\n", __FUNCTION__, cSGLEntriesGCLeft));

    /* Set start address of the entries. */
    GCPhysAddrPRDTLEntryStart = AHCI_RTGCPHYS_FROM_U32(pCmdHdr->u32CmdTblAddrUp, pCmdHdr->u32CmdTblAddr) + AHCI_CMDHDR_PRDT_OFFSET;

    do
    {
        cSGLEntriesGCRead = (cSGLEntriesGCLeft < RT_ELEMENTS(aSGLEntry)) ? cSGLEntriesGCLeft : RT_ELEMENTS(aSGLEntry);
        cSGLEntriesGCLeft -= cSGLEntriesGCRead;

        /* Read the SG entries. */
        PDMDevHlpPhysRead(pDevIns, GCPhysAddrPRDTLEntryStart, &aSGLEntry[0], cSGLEntriesGCRead * sizeof(SGLEntry));

        for (cActualSGEntry = 0; cActualSGEntry < cSGLEntriesGCRead; cActualSGEntry++)
            cbSGBuffers += (aSGLEntry[cActualSGEntry].u32DescInf & SGLENTRY_DESCINF_DBC) + 1;

        /* Set address to the next entries to read. */
        GCPhysAddrPRDTLEntryStart += cSGLEntriesGCRead * sizeof(SGLEntry);

    } while (cSGLEntriesGCLeft);

    pAhciPortTaskState->cbSGBuffers = cbSGBuffers;
}

static int ahciScatterGatherListAllocate(PAHCIPORTTASKSTATE pAhciPortTaskState, uint32_t cSGList, uint32_t cbUnaligned)
{
    if (pAhciPortTaskState->cSGListSize < cSGList)
    {
        /* The entries are not allocated yet or the number is too small. */
        if (pAhciPortTaskState->cSGListSize)
        {
            RTMemFree(pAhciPortTaskState->pSGListHead);
            RTMemFree(pAhciPortTaskState->paSGEntries);
        }

        /* Allocate R3 scatter gather list. */
        pAhciPortTaskState->pSGListHead = (PPDMDATASEG)RTMemAllocZ(cSGList * sizeof(PDMDATASEG));
        if (!pAhciPortTaskState->pSGListHead)
            return VERR_NO_MEMORY;

        pAhciPortTaskState->paSGEntries = (PAHCIPORTTASKSTATESGENTRY)RTMemAllocZ(cSGList * sizeof(AHCIPORTTASKSTATESGENTRY));
        if (!pAhciPortTaskState->paSGEntries)
            return VERR_NO_MEMORY;

        /* Reset usage statistics. */
        pAhciPortTaskState->cSGListSize = cSGList;
        pAhciPortTaskState->cSGListTooBig = 0;
    }
    else if (pAhciPortTaskState->cSGListSize > cSGList)
    {
        /*
         * The list is too big. Increment counter.
         * So that the destroying function can free
         * the list if it is too big too many times
         * in a row.
         */
        pAhciPortTaskState->cSGListTooBig++;
    }
    else
    {
        /*
         * Needed entries matches current size.
         * Reset counter.
         */
        pAhciPortTaskState->cSGListTooBig = 0;
    }

    pAhciPortTaskState->cSGEntries = cSGList;

    if (pAhciPortTaskState->cbBufferUnaligned < cbUnaligned)
    {
        if (pAhciPortTaskState->pvBufferUnaligned)
            RTMemFree(pAhciPortTaskState->pvBufferUnaligned);

        Log(("%s: Allocating buffer for unaligned segments cbUnaligned=%u\n", __FUNCTION__, cbUnaligned));

        pAhciPortTaskState->pvBufferUnaligned = RTMemAllocZ(cbUnaligned);
        if (!pAhciPortTaskState->pvBufferUnaligned)
            return VERR_NO_MEMORY;

        pAhciPortTaskState->cbBufferUnaligned = cbUnaligned;
    }

    /* Make debugging easier. */
#ifdef DEBUG
    memset(pAhciPortTaskState->pSGListHead, 0, pAhciPortTaskState->cSGListSize * sizeof(PDMDATASEG));
    memset(pAhciPortTaskState->paSGEntries, 0, pAhciPortTaskState->cSGListSize * sizeof(AHCIPORTTASKSTATESGENTRY));
    if (pAhciPortTaskState->pvBufferUnaligned)
        memset(pAhciPortTaskState->pvBufferUnaligned, 0, pAhciPortTaskState->cbBufferUnaligned);
#endif

    return VINF_SUCCESS;
}

/**
 * Fallback scatter gather list creator.
 * Used if the normal one fails in PDMDevHlpPhysGCPhys2CCPtr() or
 * PDMDevHlpPhysGCPhys2CCPtrReadonly() or post processing
 * is used.
 *
 * returns VBox status code.
 * @param   pAhciPort              The ahci port.
 * @param   pAhciPortTaskState     The task state which contains the S/G list entries.
 * @param   fReadonly              If the mappings should be readonly.
 * @param   cSGEntriesProcessed    Number of entries the normal creator procecssed
 *                                 before an error occurred. Used to free
 *                                 any ressources allocated before.
 * @thread  EMT
 */
static int ahciScatterGatherListCreateSafe(PAHCIPort pAhciPort, PAHCIPORTTASKSTATE pAhciPortTaskState,
                                           bool fReadonly, unsigned cSGEntriesProcessed)
{
    CmdHdr                    *pCmdHdr = &pAhciPortTaskState->cmdHdr;
    PPDMDEVINS                 pDevIns = pAhciPort->CTX_SUFF(pDevIns);
    PAHCIPORTTASKSTATESGENTRY  pSGInfoCurr  = pAhciPortTaskState->paSGEntries;

    Assert(VALID_PTR(pAhciPortTaskState->pSGListHead) || !cSGEntriesProcessed);
    Assert(VALID_PTR(pAhciPortTaskState->paSGEntries) || !cSGEntriesProcessed);

    for (unsigned cSGEntryCurr = 0; cSGEntryCurr < cSGEntriesProcessed; cSGEntryCurr++)
    {
        if (pSGInfoCurr->fGuestMemory)
        {
            /* Release the lock. */
            PDMDevHlpPhysReleasePageMappingLock(pDevIns, &pSGInfoCurr->u.direct.PageLock);
        }

        /* Go to the next entry. */
        pSGInfoCurr++;
    }

    if (pAhciPortTaskState->pvBufferUnaligned)
    {
        RTMemFree(pAhciPortTaskState->pvBufferUnaligned);
        pAhciPortTaskState->pvBufferUnaligned = NULL;
    }
    if (pAhciPortTaskState->pSGListHead)
    {
        RTMemFree(pAhciPortTaskState->pSGListHead);
        pAhciPortTaskState->pSGListHead = NULL;
    }
    if (pAhciPortTaskState->paSGEntries)
    {
        RTMemFree(pAhciPortTaskState->paSGEntries);
        pAhciPortTaskState->paSGEntries = NULL;
    }
    pAhciPortTaskState->cSGListTooBig = 0;
    pAhciPortTaskState->cSGEntries    = 1;
    pAhciPortTaskState->cSGListUsed   = 1;
    pAhciPortTaskState->cSGListSize   = 1;
    pAhciPortTaskState->cbBufferUnaligned = pAhciPortTaskState->cbSGBuffers;

    /* Allocate new buffers and SG lists. */
    pAhciPortTaskState->pvBufferUnaligned = RTMemAlloc(pAhciPortTaskState->cbSGBuffers);
    if (!pAhciPortTaskState->pvBufferUnaligned)
        return VERR_NO_MEMORY;

    pAhciPortTaskState->pSGListHead = (PPDMDATASEG)RTMemAllocZ(1 * sizeof(PDMDATASEG));
    if (!pAhciPortTaskState->pSGListHead)
    {
        RTMemFree(pAhciPortTaskState->pvBufferUnaligned);
        return VERR_NO_MEMORY;
    }

    pAhciPortTaskState->paSGEntries = (PAHCIPORTTASKSTATESGENTRY)RTMemAllocZ(1 * sizeof(AHCIPORTTASKSTATESGENTRY));
    if (!pAhciPortTaskState->paSGEntries)
    {
        RTMemFree(pAhciPortTaskState->pvBufferUnaligned);
        RTMemFree(pAhciPortTaskState->pSGListHead);
        return VERR_NO_MEMORY;
    }

    /* Set pointers. */
    if (pAhciPortTaskState->cbTransfer)
    {
        pAhciPortTaskState->pSGListHead[0].cbSeg = pAhciPortTaskState->cbTransfer;

        /* Allocate a separate buffer if we have to do post processing . */
        if (pAhciPortTaskState->pfnPostProcess)
        {
            pAhciPortTaskState->pSGListHead[0].pvSeg = RTMemAlloc(pAhciPortTaskState->cbTransfer);
            if (!pAhciPortTaskState->pSGListHead[0].pvSeg)
            {
                RTMemFree(pAhciPortTaskState->paSGEntries);
                RTMemFree(pAhciPortTaskState->pvBufferUnaligned);
                RTMemFree(pAhciPortTaskState->pSGListHead);
                return VERR_NO_MEMORY;
            }
        }
        else
            pAhciPortTaskState->pSGListHead[0].pvSeg = pAhciPortTaskState->pvBufferUnaligned;
    }
    else
    {
        pAhciPortTaskState->pSGListHead[0].cbSeg = pAhciPortTaskState->cbBufferUnaligned;
        pAhciPortTaskState->pSGListHead[0].pvSeg = pAhciPortTaskState->pvBufferUnaligned;
    }

    pAhciPortTaskState->paSGEntries[0].fGuestMemory = false;
    pAhciPortTaskState->paSGEntries[0].u.temp.cUnaligned   = AHCI_CMDHDR_PRDTL_ENTRIES(pCmdHdr->u32DescInf);
    pAhciPortTaskState->paSGEntries[0].u.temp.GCPhysAddrBaseFirstUnaligned = AHCI_RTGCPHYS_FROM_U32(pCmdHdr->u32CmdTblAddrUp, pCmdHdr->u32CmdTblAddr) + AHCI_CMDHDR_PRDT_OFFSET;
    pAhciPortTaskState->paSGEntries[0].u.temp.pvBuf = pAhciPortTaskState->pvBufferUnaligned;

    if (pAhciPortTaskState->uTxDir == PDMBLOCKTXDIR_TO_DEVICE)
        ahciCopyFromSGListIntoBuffer(pDevIns, &pAhciPortTaskState->paSGEntries[0]);

    return VINF_SUCCESS;
}

/**
 * Create scatter gather list descriptors.
 *
 * @returns VBox status code.
 * @param   pAhciPort              The ahci port.
 * @param   pAhciPortTaskState     The task state which contains the S/G list entries.
 * @param   fReadonly              If the mappings should be readonly.
 * @thread  EMT
 */
static int ahciScatterGatherListCreate(PAHCIPort pAhciPort, PAHCIPORTTASKSTATE pAhciPortTaskState, bool fReadonly)
{
    int        rc = VINF_SUCCESS;
    CmdHdr    *pCmdHdr = &pAhciPortTaskState->cmdHdr;
    PPDMDEVINS pDevIns = pAhciPort->CTX_SUFF(pDevIns);
    unsigned   cActualSGEntry;
    unsigned   cSGEntriesR3 = 0;               /* Needed scatter gather list entries in R3. */
    unsigned   cSGEntriesProcessed = 0;        /* Number of SG entries procesed. */
    SGLEntry   aSGLEntry[32];                  /* Holds read sg entries from guest. Biggest seen number of entries a guest set up. */
    unsigned   cSGLEntriesGCRead;
    unsigned   cSGLEntriesGCLeft;              /* Available scatter gather list entries in GC */
    RTGCPHYS   GCPhysAddrPRDTLEntryStart;      /* Start address to read the entries from. */
    uint32_t   cbSegment;                      /* Size of the current segments in bytes. */
    bool       fUnaligned;                     /* Flag whether the current buffer is unaligned. */
    uint32_t   cbUnaligned;                    /* Size of the unaligned buffers. */
    uint32_t   cUnaligned;
    bool       fDoMapping = false;
    uint32_t   cbSGBuffers = 0;                /* Total number of bytes reserved for this request. */
    RTGCPHYS   GCPhysAddrPRDTLUnalignedStart = NIL_RTGCPHYS;
    PAHCIPORTTASKSTATESGENTRY  pSGInfoCurr  = NULL;
    PAHCIPORTTASKSTATESGENTRY  pSGInfoPrev  = NULL;
    PPDMDATASEG                pSGEntryCurr = NULL;
    PPDMDATASEG                pSGEntryPrev = NULL;
    RTGCPHYS                   GCPhysBufferPageAlignedPrev = NIL_RTGCPHYS;
    uint8_t                   *pu8BufferUnalignedPos = NULL;
    uint32_t                   cbUnalignedComplete = 0;

    STAM_PROFILE_START(&pAhciPort->StatProfileMapIntoR3, a);

    pAhciPortTaskState->cbSGBuffers = 0;

    /*
     * Create a safe mapping when doing post processing because the size of the
     * data to transfer and the amount of guest memory reserved can differ
     */
    if (pAhciPortTaskState->pfnPostProcess)
    {
        ahciLog(("%s: Request with post processing.\n"));

        ahciScatterGatherListGetTotalBufferSize(pAhciPort, pAhciPortTaskState);

        return ahciScatterGatherListCreateSafe(pAhciPort, pAhciPortTaskState, fReadonly, 0);
    }

    /*
     * We need to calculate the number of SG list entries in R3 first because the data buffers in the guest don't need to be
     * page aligned. Hence the number of SG list entries in the guest can differ from the ones we need
     * because PDMDevHlpPhysGCPhys2CCPtr works only on a page base.
     * In the first pass we calculate the number of segments in R3 and in the second pass we map the guest segments into R3.
     */
    for (int i = 0; i < 2; i++)
    {
        cSGLEntriesGCLeft = AHCI_CMDHDR_PRDTL_ENTRIES(pCmdHdr->u32DescInf);
        ahciLog(("%s: cSGEntriesGC=%u\n", __FUNCTION__, cSGLEntriesGCLeft));

        /* Set start address of the entries. */
        GCPhysAddrPRDTLEntryStart = AHCI_RTGCPHYS_FROM_U32(pCmdHdr->u32CmdTblAddrUp, pCmdHdr->u32CmdTblAddr) + AHCI_CMDHDR_PRDT_OFFSET;
        fUnaligned  = false;
        cbUnaligned = 0;
        cUnaligned  = 0;
        GCPhysBufferPageAlignedPrev = NIL_RTGCPHYS;

        if (fDoMapping)
        {
            ahciLog(("%s: cSGEntriesR3=%u\n", __FUNCTION__, cSGEntriesR3));
            /* The number of needed SG entries in R3 is known. Allocate needed memory. */
            rc = ahciScatterGatherListAllocate(pAhciPortTaskState, cSGEntriesR3, cbUnalignedComplete);
            AssertMsgRC(rc, ("Failed to allocate scatter gather array rc=%Rrc\n", rc));

            /* We are now able to map the pages into R3. */
            pSGInfoCurr  = pAhciPortTaskState->paSGEntries;
            pSGEntryCurr = pAhciPortTaskState->pSGListHead;
            pSGEntryPrev = pSGEntryCurr;
            pSGInfoPrev  = pSGInfoCurr;
            /* Initialize first segment to remove the need for additional if checks later in the code. */
            pSGEntryCurr->pvSeg = NULL;
            pSGEntryCurr->cbSeg = 0;
            pSGInfoCurr->fGuestMemory= false;
            pu8BufferUnalignedPos = (uint8_t *)pAhciPortTaskState->pvBufferUnaligned;
            pAhciPortTaskState->cSGListUsed = 0;
            pAhciPortTaskState->cbSGBuffers = cbSGBuffers;
        }

        do
        {
            cSGLEntriesGCRead = (cSGLEntriesGCLeft < RT_ELEMENTS(aSGLEntry)) ? cSGLEntriesGCLeft : RT_ELEMENTS(aSGLEntry);
            cSGLEntriesGCLeft -= cSGLEntriesGCRead;

            /* Read the SG entries. */
            PDMDevHlpPhysRead(pDevIns, GCPhysAddrPRDTLEntryStart, &aSGLEntry[0], cSGLEntriesGCRead * sizeof(SGLEntry));

            for (cActualSGEntry = 0; cActualSGEntry < cSGLEntriesGCRead; cActualSGEntry++)
            {
                RTGCPHYS    GCPhysAddrDataBase;
                uint32_t    cbDataToTransfer;

                ahciLog(("%s: cActualSGEntry=%u cSGEntriesR3=%u\n", __FUNCTION__, cActualSGEntry, cSGEntriesR3));

                cbDataToTransfer = (aSGLEntry[cActualSGEntry].u32DescInf & SGLENTRY_DESCINF_DBC) + 1;
                ahciLog(("%s: cbDataToTransfer=%u\n", __FUNCTION__, cbDataToTransfer));
                cbSGBuffers += cbDataToTransfer;

                /* Check if the buffer is sector aligned. */
                if (cbDataToTransfer % 512 != 0)
                {
                    if (!fUnaligned)
                    {
                        /* We are not in an unaligned buffer but this is the first unaligned one. */
                        fUnaligned  = true;
                        cbUnaligned = cbDataToTransfer;
                        GCPhysAddrPRDTLUnalignedStart = GCPhysAddrPRDTLEntryStart + cActualSGEntry * sizeof(SGLEntry);
                        cSGEntriesR3++;
                        cUnaligned = 1;
                        ahciLog(("%s: Unaligned buffer found cb=%d\n", __FUNCTION__, cbDataToTransfer));
                    }
                    else
                    {
                        /* We are already in an unaligned buffer and this one is unaligned too. */
                        cbUnaligned += cbDataToTransfer;
                        cUnaligned++;
                    }

                    cbUnalignedComplete += cbDataToTransfer;
                }
                else /* Guest segment size is sector aligned. */
                {
                    if (fUnaligned)
                    {
                        if (cbUnaligned % 512 == 0)
                        {
                            /*
                             * The last buffer started at an offset
                             * not aligned to a sector boundary but this buffer
                             * is sector aligned. Check if the current size of all
                             * unaligned segments is a multiple of a sector.
                             * If that's the case we can now map the segments again into R3.
                             */
                            fUnaligned = false;

                            if (fDoMapping)
                            {
                                /* Set up the entry. */
                                pSGInfoCurr->fGuestMemory = false;
                                pSGInfoCurr->u.temp.GCPhysAddrBaseFirstUnaligned = GCPhysAddrPRDTLUnalignedStart;
                                pSGInfoCurr->u.temp.cUnaligned = cUnaligned;
                                pSGInfoCurr->u.temp.pvBuf      = pu8BufferUnalignedPos;

                                pSGEntryCurr->pvSeg    = pu8BufferUnalignedPos;
                                pSGEntryCurr->cbSeg    = cbUnaligned;
                                pu8BufferUnalignedPos += cbUnaligned;

                                /*
                                 * If the transfer is to the device we need to copy the content of the not mapped guest
                                 * segments into the temporary buffer.
                                 */
                                if (pAhciPortTaskState->uTxDir == PDMBLOCKTXDIR_TO_DEVICE)
                                    ahciCopyFromSGListIntoBuffer(pDevIns, pSGInfoCurr);

                                /* Advance to next entry saving the pointers to the current ones. */
                                pSGEntryPrev = pSGEntryCurr;
                                pSGInfoPrev  = pSGInfoCurr;
                                pSGInfoCurr++;
                                pSGEntryCurr++;
                                pAhciPortTaskState->cSGListUsed++;
                                cSGEntriesProcessed++;
                            }
                        }
                        else
                        {
                            cbUnaligned         += cbDataToTransfer;
                            cbUnalignedComplete += cbDataToTransfer;
                            cUnaligned++;
                        }
                    }
                    else
                    {
                        /*
                         * The size of the guest segment is sector aligned but it is possible that the segment crosses
                         * a page boundary in a way splitting the segment into parts which are not sector aligned.
                         * We have to treat them like unaligned guest segments then.
                         */
                        GCPhysAddrDataBase = AHCI_RTGCPHYS_FROM_U32(aSGLEntry[cActualSGEntry].u32DBAUp, aSGLEntry[cActualSGEntry].u32DBA);

                        ahciLog(("%s: GCPhysAddrDataBase=%RGp\n", __FUNCTION__, GCPhysAddrDataBase));

                        /*
                         * Check if the physical address is page aligned.
                         */
                        if (GCPhysAddrDataBase & PAGE_OFFSET_MASK)
                        {
                            RTGCPHYS    GCPhysAddrDataNextPage = PHYS_PAGE_ADDRESS(GCPhysAddrDataBase) + PAGE_SIZE;
                            /* Difference from the buffer start to the next page boundary. */
                            uint32_t    u32GCPhysAddrDiff = GCPhysAddrDataNextPage - GCPhysAddrDataBase;

                            if (u32GCPhysAddrDiff % 512 != 0)
                            {
                                if (!fUnaligned)
                                {
                                    /* We are not in an unaligned buffer but this is the first unaligned one. */
                                    fUnaligned  = true;
                                    cbUnaligned = cbDataToTransfer;
                                    GCPhysAddrPRDTLUnalignedStart = GCPhysAddrPRDTLEntryStart + cActualSGEntry * sizeof(SGLEntry);
                                    cSGEntriesR3++;
                                    cUnaligned = 1;
                                    ahciLog(("%s: Guest segment is sector aligned but crosses a page boundary cb=%d\n", __FUNCTION__, cbDataToTransfer));
                                }
                                else
                                {
                                    /* We are already in an unaligned buffer and this one is unaligned too. */
                                    cbUnaligned += cbDataToTransfer;
                                    cUnaligned++;
                                }

                                cbUnalignedComplete += cbDataToTransfer;
                            }
                            else
                            {
                                ahciLog(("%s: Align page: GCPhysAddrDataBase=%RGp GCPhysAddrDataNextPage=%RGp\n",
                                         __FUNCTION__, GCPhysAddrDataBase, GCPhysAddrDataNextPage));

                                RTGCPHYS GCPhysBufferPageAligned = PHYS_PAGE_ADDRESS(GCPhysAddrDataBase);

                                /* Check if the mapping ends at the page boundary and set segment size accordingly. */
                                cbSegment =   (cbDataToTransfer < u32GCPhysAddrDiff)
                                            ? cbDataToTransfer
                                            : u32GCPhysAddrDiff;
                                /* Subtract size of the buffer in the actual page. */
                                cbDataToTransfer -= cbSegment;

                                if (GCPhysBufferPageAlignedPrev != GCPhysBufferPageAligned)
                                {
                                    /* We don't need to map the buffer if it is in the same page as the previous one. */
                                    if (fDoMapping)
                                    {
                                        uint8_t *pbMapping;

                                        pSGInfoCurr->fGuestMemory = true;

                                        /* Create the mapping. */
                                        if (fReadonly)
                                            rc = PDMDevHlpPhysGCPhys2CCPtrReadOnly(pDevIns, GCPhysBufferPageAligned,
                                                                                   0, (const void **)&pbMapping,
                                                                                   &pSGInfoCurr->u.direct.PageLock);
                                        else
                                            rc = PDMDevHlpPhysGCPhys2CCPtr(pDevIns, GCPhysBufferPageAligned,
                                                                           0, (void **)&pbMapping,
                                                                           &pSGInfoCurr->u.direct.PageLock);

                                        if (RT_FAILURE(rc))
                                        {
                                            /* Mapping failed. Fall back to a bounce buffer. */
                                            ahciLog(("%s: Mapping guest physical address %RGp failed with rc=%Rrc\n",
                                                     __FUNCTION__, GCPhysBufferPageAligned, rc));

                                            return ahciScatterGatherListCreateSafe(pAhciPort, pAhciPortTaskState, fReadonly,
                                                                                   cSGEntriesProcessed);
                                        }

                                        if ((pbMapping + (GCPhysAddrDataBase - GCPhysBufferPageAligned) == ((uint8_t *)pSGEntryPrev->pvSeg + pSGEntryCurr->cbSeg)))
                                        {
                                            pSGEntryPrev->cbSeg += cbSegment;
                                            ahciLog(("%s: Merged mapping pbMapping=%#p into current segment pvSeg=%#p. New size is cbSeg=%d\n",
                                                     __FUNCTION__, pbMapping, pSGEntryPrev->pvSeg, pSGEntryPrev->cbSeg));
                                        }
                                        else
                                        {
                                            pSGEntryCurr->cbSeg = cbSegment;

                                            /* Let pvBuf point to the start of the buffer in the page. */
                                            pSGEntryCurr->pvSeg =   pbMapping
                                                                  + (GCPhysAddrDataBase - GCPhysBufferPageAligned);

                                            ahciLog(("%s: pvSegBegin=%#p pvSegEnd=%#p\n", __FUNCTION__,
                                                     pSGEntryCurr->pvSeg,
                                                     (uint8_t *)pSGEntryCurr->pvSeg + pSGEntryCurr->cbSeg));

                                            pSGEntryPrev = pSGEntryCurr;
                                            pSGEntryCurr++;
                                            pAhciPortTaskState->cSGListUsed++;
                                        }

                                        pSGInfoPrev  = pSGInfoCurr;
                                        pSGInfoCurr++;
                                        cSGEntriesProcessed++;
                                    }
                                    else
                                        cSGEntriesR3++;
                                }
                                else if (fDoMapping)
                                {
                                    pSGEntryPrev->cbSeg += cbSegment;
                                    ahciLog(("%s: Buffer is already in previous mapping pvSeg=%#p. New size is cbSeg=%d\n",
                                                __FUNCTION__, pSGEntryPrev->pvSeg, pSGEntryPrev->cbSeg));
                                }

                                /* Let physical address point to the next page in the buffer. */
                                GCPhysAddrDataBase = GCPhysAddrDataNextPage;
                                GCPhysBufferPageAlignedPrev = GCPhysBufferPageAligned;
                            }
                        }

                        if (!fUnaligned)
                        {
                            /* The address is now page aligned. */
                            while (cbDataToTransfer)
                            {
                                ahciLog(("%s: GCPhysAddrDataBase=%RGp cbDataToTransfer=%u cSGEntriesR3=%u\n",
                                         __FUNCTION__, GCPhysAddrDataBase, cbDataToTransfer, cSGEntriesR3));

                                /* Check if this is the last page the buffer is in. */
                                cbSegment = (cbDataToTransfer < PAGE_SIZE) ? cbDataToTransfer : PAGE_SIZE;
                                cbDataToTransfer -= cbSegment;

                                if (fDoMapping)
                                {
                                    void *pvMapping;

                                    pSGInfoCurr->fGuestMemory = true;

                                    /* Create the mapping. */
                                    if (fReadonly)
                                        rc = PDMDevHlpPhysGCPhys2CCPtrReadOnly(pDevIns, GCPhysAddrDataBase, 0, (const void **)&pvMapping, &pSGInfoCurr->u.direct.PageLock);
                                    else
                                        rc = PDMDevHlpPhysGCPhys2CCPtr(pDevIns, GCPhysAddrDataBase, 0, &pvMapping, &pSGInfoCurr->u.direct.PageLock);

                                    if (RT_FAILURE(rc))
                                    {
                                        /* Mapping failed. Fall back to a bounce buffer. */
                                        ahciLog(("%s: Mapping guest physical address %RGp failed with rc=%Rrc\n",
                                                    __FUNCTION__, GCPhysAddrDataBase, rc));

                                        return ahciScatterGatherListCreateSafe(pAhciPort, pAhciPortTaskState, fReadonly,
                                                                               cSGEntriesProcessed);
                                    }

                                    /* Check for adjacent mappings. */
                                    if (pvMapping == ((uint8_t *)pSGEntryPrev->pvSeg + pSGEntryPrev->cbSeg)
                                        && (pSGInfoPrev->fGuestMemory == true))
                                    {
                                        /* Yes they are adjacent. Just add the size of this mapping to the previous segment. */
                                        pSGEntryPrev->cbSeg += cbSegment;
                                        ahciLog(("%s: Merged mapping pvMapping=%#p into current segment pvSeg=%#p. New size is cbSeg=%d\n",
                                                 __FUNCTION__, pvMapping, pSGEntryPrev->pvSeg, pSGEntryPrev->cbSeg));
                                    }
                                    else
                                    {
                                        /* No they are not. Use a new sg entry. */
                                        pSGEntryCurr->cbSeg       = cbSegment;
                                        pSGEntryCurr->pvSeg       = pvMapping;
                                        ahciLog(("%s: pvSegBegin=%#p pvSegEnd=%#p\n", __FUNCTION__,
                                                 pSGEntryCurr->pvSeg,
                                                 (uint8_t *)pSGEntryCurr->pvSeg + pSGEntryCurr->cbSeg));
                                        pSGEntryPrev = pSGEntryCurr;
                                        pSGEntryCurr++;
                                        pAhciPortTaskState->cSGListUsed++;
                                    }

                                    pSGInfoPrev = pSGInfoCurr;
                                    pSGInfoCurr++;
                                    cSGEntriesProcessed++;
                                }
                                else
                                    cSGEntriesR3++;

                                GCPhysBufferPageAlignedPrev = GCPhysAddrDataBase;

                                /* Go to the next page. */
                                GCPhysAddrDataBase += PAGE_SIZE;
                            }
                        } /* if (!fUnaligned) */
                    } /* if !fUnaligned */
                } /* if guest segment is sector aligned. */
            } /* for SGEntries read */

            /* Set address to the next entries to read. */
            GCPhysAddrPRDTLEntryStart += cSGLEntriesGCRead * sizeof(SGLEntry);

        } while (cSGLEntriesGCLeft);

        fDoMapping = true;

    } /* for passes */

    /* Check if the last processed segment was unaligned. We need to add it now. */
    if (fUnaligned)
    {
        /* Set up the entry. */
        AssertMsg(!(cbUnaligned % 512), ("Buffer is not sector aligned\n"));
        pSGInfoCurr->fGuestMemory = false;
        pSGInfoCurr->u.temp.GCPhysAddrBaseFirstUnaligned = GCPhysAddrPRDTLUnalignedStart;
        pSGInfoCurr->u.temp.cUnaligned = cUnaligned;
        pSGInfoCurr->u.temp.pvBuf = pu8BufferUnalignedPos;

        pSGEntryCurr->pvSeg    = pu8BufferUnalignedPos;
        pSGEntryCurr->cbSeg    = cbUnaligned;
        pAhciPortTaskState->cSGListUsed++;

        /*
         * If the transfer is to the device we need to copy the content of the not mapped guest
         * segments into the temporary buffer.
         */
        if (pAhciPortTaskState->uTxDir == PDMBLOCKTXDIR_TO_DEVICE)
            ahciCopyFromSGListIntoBuffer(pDevIns, pSGInfoCurr);
    }

    STAM_PROFILE_STOP(&pAhciPort->StatProfileMapIntoR3, a);

    return rc;
}

/**
 * Destroy a scatter gather list and free all occupied resources (mappings, etc.)
 *
 * @returns VBox status code.
 * @param   pAhciPort   The ahci port.
 * @param   pAhciPortTaskState    The task state which contains the S/G list entries.
 */
static int ahciScatterGatherListDestroy(PAHCIPort pAhciPort, PAHCIPORTTASKSTATE pAhciPortTaskState)
{
    PAHCIPORTTASKSTATESGENTRY  pSGInfoCurr  = pAhciPortTaskState->paSGEntries;
    PPDMDEVINS                 pDevIns      = pAhciPort->CTX_SUFF(pDevIns);

    STAM_PROFILE_START(&pAhciPort->StatProfileDestroyScatterGatherList, a);

    if (pAhciPortTaskState->pfnPostProcess)
    {
        int rc;
        rc = pAhciPortTaskState->pfnPostProcess(pAhciPortTaskState);
        AssertRC(rc);

        pAhciPortTaskState->pfnPostProcess = NULL;

        /* Free the buffer holding the unprocessed data. They are not needed anymore. */
        RTMemFree(pAhciPortTaskState->pSGListHead[0].pvSeg);
    }

    for (unsigned cActualSGEntry = 0; cActualSGEntry < pAhciPortTaskState->cSGEntries; cActualSGEntry++)
    {
        if (pSGInfoCurr->fGuestMemory)
        {
            /* Release the lock. */
            PDMDevHlpPhysReleasePageMappingLock(pDevIns, &pSGInfoCurr->u.direct.PageLock);
        }
        else if (pAhciPortTaskState->uTxDir == PDMBLOCKTXDIR_FROM_DEVICE)
        {
            /* Copy the data into the guest segments now. */
            ahciCopyFromBufferIntoSGList(pDevIns, pSGInfoCurr);
        }

        /* Go to the next entry. */
        pSGInfoCurr++;
    }

    /* Free allocated memory if the list was too big too many times. */
    if (pAhciPortTaskState->cSGListTooBig >= AHCI_NR_OF_ALLOWED_BIGGER_LISTS)
    {
        RTMemFree(pAhciPortTaskState->pSGListHead);
        RTMemFree(pAhciPortTaskState->paSGEntries);
        if (pAhciPortTaskState->pvBufferUnaligned)
            RTMemFree(pAhciPortTaskState->pvBufferUnaligned);
        pAhciPortTaskState->cSGListSize = 0;
        pAhciPortTaskState->cSGListTooBig = 0;
        pAhciPortTaskState->pSGListHead = NULL;
        pAhciPortTaskState->paSGEntries = NULL;
        pAhciPortTaskState->pvBufferUnaligned = NULL;
        pAhciPortTaskState->cbBufferUnaligned = 0;
    }

    STAM_PROFILE_STOP(&pAhciPort->StatProfileDestroyScatterGatherList, a);

    return VINF_SUCCESS;
}

/**
 * Copy a temporary buffer into a part of the guest scatter gather list
 * described by the given descriptor entry.
 *
 * @returns nothing.
 * @param   pDevIns    Pointer to the device instance data.
 * @param   pSGInfo    Pointer to the segment info structure which describes the guest segments
 *                     to write to which are unaligned.
 */
static void ahciCopyFromBufferIntoSGList(PPDMDEVINS pDevIns, PAHCIPORTTASKSTATESGENTRY pSGInfo)
{
    uint8_t *pu8Buf    = (uint8_t *)pSGInfo->u.temp.pvBuf;
    SGLEntry aSGLEntries[5];
    uint32_t cSGEntriesLeft = pSGInfo->u.temp.cUnaligned;
    RTGCPHYS GCPhysPRDTLStart = pSGInfo->u.temp.GCPhysAddrBaseFirstUnaligned;

    AssertMsg(!pSGInfo->fGuestMemory, ("This is not possible\n"));

    do
    {
        uint32_t cSGEntriesRead =   (cSGEntriesLeft < RT_ELEMENTS(aSGLEntries))
                                  ? cSGEntriesLeft
                                  : RT_ELEMENTS(aSGLEntries);

        PDMDevHlpPhysRead(pDevIns, GCPhysPRDTLStart, &aSGLEntries[0], cSGEntriesRead * sizeof(SGLEntry));

        for (uint32_t i = 0; i < cSGEntriesRead; i++)
        {
            RTGCPHYS GCPhysAddrDataBase = AHCI_RTGCPHYS_FROM_U32(aSGLEntries[i].u32DBAUp, aSGLEntries[i].u32DBA);
            uint32_t cbCopied = (aSGLEntries[i].u32DescInf & SGLENTRY_DESCINF_DBC) + 1;

            /* Copy into SG entry. */
            PDMDevHlpPhysWrite(pDevIns, GCPhysAddrDataBase, pu8Buf, cbCopied);

            pu8Buf    += cbCopied;
        }

        GCPhysPRDTLStart += cSGEntriesRead * sizeof(SGLEntry);
        cSGEntriesLeft   -= cSGEntriesRead;
    } while (cSGEntriesLeft);
}

/**
 * Copy a part of the guest scatter gather list into a temporary buffer.
 *
 * @returns nothing.
 * @param   pDevIns    Pointer to the device instance data.
 * @param   pSGInfo    Pointer to the segment info structure which describes the guest segments
 *                     to read from which are unaligned.
 */
static void ahciCopyFromSGListIntoBuffer(PPDMDEVINS pDevIns, PAHCIPORTTASKSTATESGENTRY pSGInfo)
{
    uint8_t *pu8Buf    = (uint8_t *)pSGInfo->u.temp.pvBuf;
    SGLEntry aSGLEntries[5];
    uint32_t cSGEntriesLeft = pSGInfo->u.temp.cUnaligned;
    RTGCPHYS GCPhysPRDTLStart = pSGInfo->u.temp.GCPhysAddrBaseFirstUnaligned;

    AssertMsg(!pSGInfo->fGuestMemory, ("This is not possible\n"));

    do
    {
        uint32_t cSGEntriesRead =   (cSGEntriesLeft < RT_ELEMENTS(aSGLEntries))
                                  ? cSGEntriesLeft
                                  : RT_ELEMENTS(aSGLEntries);

        PDMDevHlpPhysRead(pDevIns, GCPhysPRDTLStart, &aSGLEntries[0], cSGEntriesRead * sizeof(SGLEntry));

        for (uint32_t i = 0; i < cSGEntriesRead; i++)
        {
            RTGCPHYS GCPhysAddrDataBase = AHCI_RTGCPHYS_FROM_U32(aSGLEntries[i].u32DBAUp, aSGLEntries[i].u32DBA);
            uint32_t cbCopied = (aSGLEntries[i].u32DescInf & SGLENTRY_DESCINF_DBC) + 1;

            /* Copy into buffer. */
            PDMDevHlpPhysRead(pDevIns, GCPhysAddrDataBase, pu8Buf, cbCopied);

            pu8Buf    += cbCopied;
        }

        GCPhysPRDTLStart += cSGEntriesRead * sizeof(SGLEntry);
        cSGEntriesLeft   -= cSGEntriesRead;
    } while (cSGEntriesLeft);
}


/**
 * Copy the content of a buffer to a scatter gather list.
 *
 * @returns Number of bytes transfered.
 * @param   pAhciPortTaskState    The task state which contains the S/G list entries.
 * @param   pvBuf                 Pointer to the buffer which should be copied.
 * @param   cbBuf                 Size of the buffer.
 */
static int ahciScatterGatherListCopyFromBuffer(PAHCIPORTTASKSTATE pAhciPortTaskState, void *pvBuf, size_t cbBuf)
{
    unsigned cSGEntry = 0;
    int cbCopied = 0;
    PPDMDATASEG pSGEntry = &pAhciPortTaskState->pSGListHead[cSGEntry];
    uint8_t *pu8Buf = (uint8_t *)pvBuf;

    while (cSGEntry < pAhciPortTaskState->cSGEntries)
    {
        size_t cbToCopy = RT_MIN(cbBuf, pSGEntry->cbSeg);

        memcpy(pSGEntry->pvSeg, pu8Buf, cbToCopy);

        cbBuf -= cbToCopy;
        cbCopied += cbToCopy;

        /* We finished. */
        if (!cbBuf)
            break;

        /* Advance the buffer. */
        pu8Buf += cbToCopy;

        /* Go to the next entry in the list. */
        pSGEntry++;
        cSGEntry++;
    }

    LogFlow(("%s: Copied %d bytes\n", __FUNCTION__, cbCopied));
    return cbCopied;
}

/* -=-=-=-=- IBlockAsyncPort -=-=-=-=- */

/** Makes a PAHCIPort out of a PPDMIBLOCKASYNCPORT. */
#define PDMIBLOCKASYNCPORT_2_PAHCIPORT(pInterface)    ( (PAHCIPort)((uintptr_t)pInterface - RT_OFFSETOF(AHCIPort, IPortAsync)) )

/**
 * Complete a data transfer task by freeing all occupied ressources
 * and notifying the guest.
 *
 * @returns VBox status code
 *
 * @param pAhciPort             Pointer to the port where to request completed.
 * @param pAhciPortTaskState    Pointer to the task which finished.
 */
static int ahciTransferComplete(PAHCIPort pAhciPort, PAHCIPORTTASKSTATE pAhciPortTaskState)
{
    /* Free system resources occupied by the scatter gather list. */
    ahciScatterGatherListDestroy(pAhciPort, pAhciPortTaskState);

    pAhciPortTaskState->cmdHdr.u32PRDBC = pAhciPortTaskState->cbTransfer;

    pAhciPortTaskState->uATARegError = 0;
    pAhciPortTaskState->uATARegStatus = ATA_STAT_READY | ATA_STAT_SEEK;
    /* Write updated command header into memory of the guest. */
    PDMDevHlpPhysWrite(pAhciPort->CTX_SUFF(pDevIns), pAhciPortTaskState->GCPhysCmdHdrAddr,
                       &pAhciPortTaskState->cmdHdr, sizeof(CmdHdr));

    if (pAhciPortTaskState->uTxDir == PDMBLOCKTXDIR_FROM_DEVICE)
    {
        STAM_REL_COUNTER_ADD(&pAhciPort->StatBytesRead, pAhciPortTaskState->cbTransfer);
        pAhciPort->Led.Actual.s.fReading = 0;
    }
    else
    {
        STAM_REL_COUNTER_ADD(&pAhciPort->StatBytesWritten, pAhciPortTaskState->cbTransfer);
        pAhciPort->Led.Actual.s.fWriting = 0;
    }

    if (pAhciPortTaskState->fQueued)
    {
        uint32_t cOutstandingTasks;

        ahciLog(("%s: Before decrement uActTasksActive=%u\n", __FUNCTION__, pAhciPort->uActTasksActive));
        cOutstandingTasks = ASMAtomicDecU32(&pAhciPort->uActTasksActive);
        ahciLog(("%s: After decrement uActTasksActive=%u\n", __FUNCTION__, cOutstandingTasks));
        ASMAtomicOrU32(&pAhciPort->u32QueuedTasksFinished, (1 << pAhciPortTaskState->uTag));

        if (!cOutstandingTasks)
            ahciSendSDBFis(pAhciPort, pAhciPort->u32QueuedTasksFinished, pAhciPortTaskState, true);
    }
    else
        ahciSendD2HFis(pAhciPort, pAhciPortTaskState, pAhciPortTaskState->cmdFis, true);

    /* Add the task to the cache. */
    pAhciPort->aCachedTasks[pAhciPortTaskState->uTag] = pAhciPortTaskState;

    return VINF_SUCCESS;
}

/**
 * Notification callback for a completed transfer.
 *
 * @returns VBox status code.
 * @param   pInterface   Pointer to the interface.
 * @param   pvUser       User data.
 */
static DECLCALLBACK(int) ahciTransferCompleteNotify(PPDMIBLOCKASYNCPORT pInterface, void *pvUser)
{
    PAHCIPort pAhciPort = PDMIBLOCKASYNCPORT_2_PAHCIPORT(pInterface);
    PAHCIPORTTASKSTATE pAhciPortTaskState = (PAHCIPORTTASKSTATE)pvUser;

    ahciLog(("%s: pInterface=%p pvUser=%p uTag=%u\n",
             __FUNCTION__, pInterface, pvUser, pAhciPortTaskState->uTag));

    int rc = ahciTransferComplete(pAhciPort, pAhciPortTaskState);

    if (pAhciPort->uActTasksActive == 0 && pAhciPort->pAhciR3->fSignalIdle)
        RTSemEventSignal(pAhciPort->pAhciR3->hEvtIdle);
    return rc;
}

/**
 * Process an non read/write ATA command.
 *
 * @returns The direction of the data transfer
 * @param   pCmdHdr Pointer to the command header.
 */
static int ahciProcessCmd(PAHCIPort pAhciPort, PAHCIPORTTASKSTATE pAhciPortTaskState, uint8_t *pCmdFis)
{
    int rc = PDMBLOCKTXDIR_NONE;
    bool fLBA48 = false;
    CmdHdr   *pCmdHdr = &pAhciPortTaskState->cmdHdr;

    AssertMsg(pCmdFis[AHCI_CMDFIS_TYPE] == AHCI_CMDFIS_TYPE_H2D, ("FIS is not a host to device Fis!!\n"));

    pAhciPortTaskState->cbTransfer = 0;

    switch (pCmdFis[AHCI_CMDFIS_CMD])
    {
        case ATA_IDENTIFY_DEVICE:
            {
                if (pAhciPort->pDrvBlock && !pAhciPort->fATAPI)
                {
                    uint16_t u16Temp[256];

                    /* Fill the buffer. */
                    ahciIdentifySS(pAhciPort, u16Temp);

                    /* Create scatter gather list. */
                    rc = ahciScatterGatherListCreate(pAhciPort, pAhciPortTaskState, false);
                    if (RT_FAILURE(rc))
                        AssertMsgFailed(("Creating list failed rc=%Rrc\n", rc));

                    /* Copy the buffer. */
                    pCmdHdr->u32PRDBC = ahciScatterGatherListCopyFromBuffer(pAhciPortTaskState, &u16Temp[0], sizeof(u16Temp));

                    /* Destroy list. */
                    rc = ahciScatterGatherListDestroy(pAhciPort, pAhciPortTaskState);
                    if (RT_FAILURE(rc))
                        AssertMsgFailed(("Freeing list failed rc=%Rrc\n", rc));

                    pAhciPortTaskState->uATARegError = 0;
                    pAhciPortTaskState->uATARegStatus = ATA_STAT_READY | ATA_STAT_SEEK;

                    /* Write updated command header into memory of the guest. */
                    PDMDevHlpPhysWrite(pAhciPort->CTX_SUFF(pDevIns), pAhciPortTaskState->GCPhysCmdHdrAddr, pCmdHdr, sizeof(CmdHdr));
                }
                else
                {
                    pAhciPortTaskState->uATARegError = ABRT_ERR;
                    pAhciPortTaskState->uATARegStatus = ATA_STAT_READY | ATA_STAT_SEEK;
                }
                break;
            }
        case ATA_READ_NATIVE_MAX_ADDRESS_EXT:
        case ATA_READ_NATIVE_MAX_ADDRESS:
                break;
        case ATA_SET_FEATURES:
        {
            switch (pCmdFis[AHCI_CMDFIS_FET])
            {
                case 0x02: /* write cache enable */
                case 0xaa: /* read look-ahead enable */
                case 0x55: /* read look-ahead disable */
                case 0xcc: /* reverting to power-on defaults enable */
                case 0x66: /* reverting to power-on defaults disable */
                    pAhciPortTaskState->uATARegError = 0;
                    pAhciPortTaskState->uATARegStatus = ATA_STAT_READY | ATA_STAT_SEEK;
                    break;
                case 0x82: /* write cache disable */
                    rc = pAhciPort->pDrvBlock->pfnFlush(pAhciPort->pDrvBlock);
                    pAhciPortTaskState->uATARegError = 0;
                    pAhciPortTaskState->uATARegStatus = ATA_STAT_READY | ATA_STAT_SEEK;
                    break;
                case 0x03:
                { /* set transfer mode */
                    Log2(("%s: transfer mode %#04x\n", __FUNCTION__, pCmdFis[AHCI_CMDFIS_SECTC]));
                    switch (pCmdFis[AHCI_CMDFIS_SECTC] & 0xf8)
                    {
                        case 0x00: /* PIO default */
                        case 0x08: /* PIO mode */
                            break;
                        case ATA_MODE_MDMA: /* MDMA mode */
                            pAhciPort->uATATransferMode = (pCmdFis[AHCI_CMDFIS_SECTC] & 0xf8) | RT_MIN(pCmdFis[AHCI_CMDFIS_SECTC] & 0x07, ATA_MDMA_MODE_MAX);
                            break;
                        case ATA_MODE_UDMA: /* UDMA mode */
                            pAhciPort->uATATransferMode = (pCmdFis[AHCI_CMDFIS_SECTC] & 0xf8) | RT_MIN(pCmdFis[AHCI_CMDFIS_SECTC] & 0x07, ATA_UDMA_MODE_MAX);
                            break;
                    }
                    break;
                }
                default:
                    pAhciPortTaskState->uATARegError = ABRT_ERR;
                    pAhciPortTaskState->uATARegStatus = ATA_STAT_READY | ATA_STAT_ERR;
            }
            break;
        }
        case ATA_FLUSH_CACHE_EXT:
        case ATA_FLUSH_CACHE:
            rc = pAhciPort->pDrvBlock->pfnFlush(pAhciPort->pDrvBlock);
            pAhciPortTaskState->uATARegError = 0;
            pAhciPortTaskState->uATARegStatus = ATA_STAT_READY | ATA_STAT_SEEK;
            break;
        case ATA_PACKET:
            if (!pAhciPort->fATAPI)
            {
                pAhciPortTaskState->uATARegError = ABRT_ERR;
                pAhciPortTaskState->uATARegStatus = ATA_STAT_READY | ATA_STAT_ERR;
            }
            else
            {
                rc = atapiParseCmdVirtualATAPI(pAhciPort, pAhciPortTaskState);
            }
            break;
        case ATA_IDENTIFY_PACKET_DEVICE:
            if (!pAhciPort->fATAPI)
            {
                pAhciPortTaskState->uATARegError = ABRT_ERR;
                pAhciPortTaskState->uATARegStatus = ATA_STAT_READY | ATA_STAT_ERR;
            }
            else
            {
                atapiDoTransfer(pAhciPort, pAhciPortTaskState, ATAFN_SS_ATAPI_IDENTIFY);

                pAhciPortTaskState->uATARegError = 0;
                pAhciPortTaskState->uATARegStatus = ATA_STAT_READY | ATA_STAT_SEEK;
            }
            break;
        case ATA_SET_MULTIPLE_MODE:
            if (    pCmdFis[AHCI_CMDFIS_SECTC] != 0
                &&  (   pCmdFis[AHCI_CMDFIS_SECTC] > ATA_MAX_MULT_SECTORS
                     || (pCmdFis[AHCI_CMDFIS_SECTC] & (pCmdFis[AHCI_CMDFIS_SECTC] - 1)) != 0))
            {
                pAhciPortTaskState->uATARegError = ABRT_ERR;
                pAhciPortTaskState->uATARegStatus = ATA_STAT_READY | ATA_STAT_ERR;
            }
            else
            {
                Log2(("%s: set multi sector count to %d\n", __FUNCTION__, pCmdFis[AHCI_CMDFIS_SECTC]));
                pAhciPort->cMultSectors = pCmdFis[AHCI_CMDFIS_SECTC];
                pAhciPortTaskState->uATARegError = 0;
                pAhciPortTaskState->uATARegStatus = ATA_STAT_READY | ATA_STAT_SEEK;
            }
            break;
        case ATA_STANDBY_IMMEDIATE:
            break; /* Do nothing. */
        case ATA_CHECK_POWER_MODE:
            pAhciPortTaskState->cmdFis[AHCI_CMDFIS_SECTC] = 0xff; /* drive active or idle */
            /* fall through */
        case ATA_INITIALIZE_DEVICE_PARAMETERS:
        case ATA_IDLE_IMMEDIATE:
        case ATA_RECALIBRATE:
        case ATA_NOP:
        case ATA_READ_VERIFY_SECTORS_EXT:
        case ATA_READ_VERIFY_SECTORS:
        case ATA_READ_VERIFY_SECTORS_WITHOUT_RETRIES:
            pAhciPortTaskState->uATARegError = 0;
            pAhciPortTaskState->uATARegStatus = ATA_STAT_READY | ATA_STAT_SEEK;
            break;
        case ATA_READ_DMA_EXT:
            fLBA48 = true;
        case ATA_READ_DMA:
        {
            pAhciPortTaskState->cbTransfer = ahciGetNSectors(pCmdFis, fLBA48) * 512;
            pAhciPortTaskState->uOffset = ahciGetSector(pAhciPort, pCmdFis, fLBA48) * 512;
            rc = PDMBLOCKTXDIR_FROM_DEVICE;
            break;
        }
        case ATA_WRITE_DMA_EXT:
            fLBA48 = true;
        case ATA_WRITE_DMA:
        {
            pAhciPortTaskState->cbTransfer = ahciGetNSectors(pCmdFis, fLBA48) * 512;
            pAhciPortTaskState->uOffset = ahciGetSector(pAhciPort, pCmdFis, fLBA48) * 512;
            rc = PDMBLOCKTXDIR_TO_DEVICE;
            break;
        }
        case ATA_READ_FPDMA_QUEUED:
        {
            pAhciPortTaskState->cbTransfer = ahciGetNSectorsQueued(pCmdFis) * 512;
            pAhciPortTaskState->uOffset = ahciGetSectorQueued(pCmdFis) * 512;
            rc = PDMBLOCKTXDIR_FROM_DEVICE;
            break;
        }
        case ATA_WRITE_FPDMA_QUEUED:
        {
            pAhciPortTaskState->cbTransfer = ahciGetNSectorsQueued(pCmdFis) * 512;
            pAhciPortTaskState->uOffset = ahciGetSectorQueued(pCmdFis) * 512;
            rc = PDMBLOCKTXDIR_TO_DEVICE;
            break;
        }
        /* All not implemented commands go below. */
        case ATA_SECURITY_FREEZE_LOCK:
        case ATA_SMART:
        case ATA_NV_CACHE:
        case ATA_SLEEP: /* Powermanagement not supported. */
            pAhciPortTaskState->uATARegError = ABRT_ERR;
            pAhciPortTaskState->uATARegStatus = ATA_STAT_READY | ATA_STAT_ERR;
            break;
        default: /* For debugging purposes. */
            AssertMsgFailed(("Unknown command issued\n"));
            pAhciPortTaskState->uATARegError = ABRT_ERR;
            pAhciPortTaskState->uATARegStatus = ATA_STAT_READY | ATA_STAT_ERR;
    }

    return rc;
}

/**
 * Retrieve a command FIS from guest memory.
 *
 * @returns nothing
 * @param pAhciPortTaskState The state of the actual task.
 */
static void ahciPortTaskGetCommandFis(PAHCIPort pAhciPort, PAHCIPORTTASKSTATE pAhciPortTaskState)
{
    RTGCPHYS  GCPhysAddrCmdTbl;

    AssertMsg(pAhciPort->GCPhysAddrClb && pAhciPort->GCPhysAddrFb, ("%s: GCPhysAddrClb and/or GCPhysAddrFb are 0\n", __FUNCTION__));

    /*
     * First we are reading the command header pointed to by regCLB.
     * From this we get the address of the command table which we are reading too.
     * We can process the Command FIS afterwards.
     */
    pAhciPortTaskState->GCPhysCmdHdrAddr = pAhciPort->GCPhysAddrClb + pAhciPortTaskState->uTag * sizeof(CmdHdr);
    LogFlow(("%s: PDMDevHlpPhysRead GCPhysAddrCmdLst=%RGp cbCmdHdr=%u\n", __FUNCTION__,
             pAhciPortTaskState->GCPhysCmdHdrAddr, sizeof(CmdHdr)));
    PDMDevHlpPhysRead(pAhciPort->CTX_SUFF(pDevIns), pAhciPortTaskState->GCPhysCmdHdrAddr, &pAhciPortTaskState->cmdHdr, sizeof(CmdHdr));

#ifdef DEBUG
    /* Print some infos about the command header. */
    ahciDumpCmdHdrInfo(pAhciPort, &pAhciPortTaskState->cmdHdr);
#endif

    GCPhysAddrCmdTbl = AHCI_RTGCPHYS_FROM_U32(pAhciPortTaskState->cmdHdr.u32CmdTblAddrUp, pAhciPortTaskState->cmdHdr.u32CmdTblAddr);

    AssertMsg((pAhciPortTaskState->cmdHdr.u32DescInf & AHCI_CMDHDR_CFL_MASK) * sizeof(uint32_t) == AHCI_CMDFIS_TYPE_H2D_SIZE,
              ("This is not a command FIS!!\n"));

    /* Read the command Fis. */
    LogFlow(("%s: PDMDevHlpPhysRead GCPhysAddrCmdTbl=%RGp cbCmdFis=%u\n", __FUNCTION__, GCPhysAddrCmdTbl, AHCI_CMDFIS_TYPE_H2D_SIZE));
    PDMDevHlpPhysRead(pAhciPort->CTX_SUFF(pDevIns), GCPhysAddrCmdTbl, &pAhciPortTaskState->cmdFis[0], AHCI_CMDFIS_TYPE_H2D_SIZE);

    /* Set transfer direction. */
    pAhciPortTaskState->uTxDir = (pAhciPortTaskState->cmdHdr.u32DescInf & AHCI_CMDHDR_W) ? PDMBLOCKTXDIR_TO_DEVICE : PDMBLOCKTXDIR_FROM_DEVICE;

    /* If this is an ATAPI command read the atapi command. */
    if (pAhciPortTaskState->cmdHdr.u32DescInf & AHCI_CMDHDR_A)
    {
        GCPhysAddrCmdTbl += AHCI_CMDHDR_ACMD_OFFSET;
        PDMDevHlpPhysRead(pAhciPort->CTX_SUFF(pDevIns), GCPhysAddrCmdTbl, &pAhciPortTaskState->aATAPICmd[0], ATAPI_PACKET_SIZE);
    }

    /* We "received" the FIS. Clear the BSY bit in regTFD. */
    if ((pAhciPortTaskState->cmdHdr.u32DescInf & AHCI_CMDHDR_C) && (pAhciPortTaskState->fQueued))
    {
        /*
         * We need to send a FIS which clears the busy bit if this is a queued command so that the guest can queue other commands.
         * but this FIS does not assert an interrupt
         */
        ahciSendD2HFis(pAhciPort, pAhciPortTaskState, pAhciPortTaskState->cmdFis, false);
        pAhciPort->regTFD &= ~AHCI_PORT_TFD_BSY;
    }

#ifdef DEBUG
    /* Print some infos about the FIS. */
    ahciDumpFisInfo(pAhciPort, &pAhciPortTaskState->cmdFis[0]);

    /* Print the PRDT */
    RTGCPHYS GCPhysAddrPRDTLEntryStart = AHCI_RTGCPHYS_FROM_U32(pAhciPortTaskState->cmdHdr.u32CmdTblAddrUp, pAhciPortTaskState->cmdHdr.u32CmdTblAddr) + AHCI_CMDHDR_PRDT_OFFSET;

    ahciLog(("PRDT address %RGp number of entries %u\n", GCPhysAddrPRDTLEntryStart, AHCI_CMDHDR_PRDTL_ENTRIES(pAhciPortTaskState->cmdHdr.u32DescInf)));

    for (unsigned i = 0; i < AHCI_CMDHDR_PRDTL_ENTRIES(pAhciPortTaskState->cmdHdr.u32DescInf); i++)
    {
        SGLEntry SGEntry;

        ahciLog(("Entry %u at address %RGp\n", i, GCPhysAddrPRDTLEntryStart));
        PDMDevHlpPhysRead(pAhciPort->CTX_SUFF(pDevIns), GCPhysAddrPRDTLEntryStart, &SGEntry, sizeof(SGLEntry));

        RTGCPHYS GCPhysDataAddr = AHCI_RTGCPHYS_FROM_U32(SGEntry.u32DBAUp, SGEntry.u32DBA);
        ahciLog(("GCPhysAddr=%RGp Size=%u\n", GCPhysDataAddr, SGEntry.u32DescInf & SGLENTRY_DESCINF_DBC));

        GCPhysAddrPRDTLEntryStart += sizeof(SGLEntry);
    }
#endif
}

/**
 * Transmit queue consumer
 * Queue a new async task.
 *
 * @returns Success indicator.
 *          If false the item will not be removed and the flushing will stop.
 * @param   pDevIns     The device instance.
 * @param   pItem       The item to consume. Upon return this item will be freed.
 */
static DECLCALLBACK(bool) ahciNotifyQueueConsumer(PPDMDEVINS pDevIns, PPDMQUEUEITEMCORE pItem)
{
    PDEVPORTNOTIFIERQUEUEITEM pNotifierItem = (PDEVPORTNOTIFIERQUEUEITEM)pItem;
    PAHCI                     pAhci = PDMINS_2_DATA(pDevIns, PAHCI);
    PAHCIPort                 pAhciPort = &pAhci->ahciPort[pNotifierItem->iPort];
    int                       rc = VINF_SUCCESS;

    if (!pAhciPort->fAsyncInterface)
    {
        ahciLog(("%s: Got notification from GC\n", __FUNCTION__));
        /* Notify the async IO thread. */
        rc = RTSemEventSignal(pAhciPort->AsyncIORequestSem);
        AssertRC(rc);
    }
    else
    {
        int                iTxDir;
        PAHCIPORTTASKSTATE pAhciPortTaskState;

        ahciLog(("%s: Processing command at slot %d\n", __FUNCTION__, pNotifierItem->iTask));

        /* Check if there is already an allocated task struct in the cache.
         * Allocate a new task otherwise.
         */
        if (!pAhciPort->aCachedTasks[pNotifierItem->iTask])
        {
            pAhciPortTaskState = (PAHCIPORTTASKSTATE)RTMemAllocZ(sizeof(AHCIPORTTASKSTATE));
            AssertMsg(pAhciPortTaskState, ("%s: Cannot allocate task state memory!\n"));
        }
        else
        {
            pAhciPortTaskState = pAhciPort->aCachedTasks[pNotifierItem->iTask];
        }

        /** Set current command slot */
        pAhciPortTaskState->uTag = pNotifierItem->iTask;
        pAhciPort->regCMD |= (AHCI_PORT_CMD_CCS_SHIFT(pAhciPortTaskState->uTag));

        ahciPortTaskGetCommandFis(pAhciPort, pAhciPortTaskState);

        /* Mark the task as processed by the HBA if this is a queued task so that it doesn't occur in the CI register anymore. */
        if (pNotifierItem->fQueued)
        {
            pAhciPortTaskState->fQueued = true;
            ASMAtomicOrU32(&pAhciPort->u32TasksFinished, (1 << pAhciPortTaskState->uTag));
        }
        else
            pAhciPortTaskState->fQueued = false;

        if (!(pAhciPortTaskState->cmdFis[AHCI_CMDFIS_BITS] & AHCI_CMDFIS_C))
        {
            /* If the reset bit is set put the device into reset state. */
            if (pAhciPortTaskState->cmdFis[AHCI_CMDFIS_CTL] & AHCI_CMDFIS_CTL_SRST)
            {
                ahciLog(("%s: Setting device into reset state\n", __FUNCTION__));
                pAhciPort->fResetDevice = true;
                ahciSendD2HFis(pAhciPort, pAhciPortTaskState, pAhciPortTaskState->cmdFis, true);
                pAhciPort->aCachedTasks[pNotifierItem->iTask] = pAhciPortTaskState;
                return true;
            }
            else if (pAhciPort->fResetDevice) /* The bit is not set and we are in a reset state. */
            {
                ahciFinishStorageDeviceReset(pAhciPort, pAhciPortTaskState);
                pAhciPort->aCachedTasks[pNotifierItem->iTask] = pAhciPortTaskState;
                return true;
            }
            else /* We are not in a reset state update the control registers. */
            {
                AssertMsgFailed(("%s: Update the control register\n", __FUNCTION__));
            }
        }

        iTxDir = ahciProcessCmd(pAhciPort, pAhciPortTaskState, pAhciPortTaskState->cmdFis);

        if (iTxDir != PDMBLOCKTXDIR_NONE)
        {
            if (pAhciPortTaskState->fQueued)
            {
                ahciLog(("%s: Before increment uActTasksActive=%u\n", __FUNCTION__, pAhciPort->uActTasksActive));
                ASMAtomicIncU32(&pAhciPort->uActTasksActive);
                ahciLog(("%s: After increment uActTasksActive=%u\n", __FUNCTION__, pAhciPort->uActTasksActive));
            }

            STAM_REL_COUNTER_INC(&pAhciPort->StatDMA);

            rc = ahciScatterGatherListCreate(pAhciPort, pAhciPortTaskState, (iTxDir == PDMBLOCKTXDIR_FROM_DEVICE) ? false : true);
            if (RT_FAILURE(rc))
                AssertMsgFailed(("%s: Failed to process command %Rrc\n", __FUNCTION__, rc));

            if (iTxDir == PDMBLOCKTXDIR_FROM_DEVICE)
            {
                pAhciPort->Led.Asserted.s.fReading = pAhciPort->Led.Actual.s.fReading = 1;
                rc = pAhciPort->pDrvBlockAsync->pfnStartRead(pAhciPort->pDrvBlockAsync, pAhciPortTaskState->uOffset,
                                                             pAhciPortTaskState->pSGListHead, pAhciPortTaskState->cSGListUsed,
                                                             pAhciPortTaskState->cbTransfer,
                                                             pAhciPortTaskState);
            }
            else
            {
                pAhciPort->Led.Asserted.s.fWriting = pAhciPort->Led.Actual.s.fWriting = 1;
                rc = pAhciPort->pDrvBlockAsync->pfnStartWrite(pAhciPort->pDrvBlockAsync, pAhciPortTaskState->uOffset,
                                                              pAhciPortTaskState->pSGListHead, pAhciPortTaskState->cSGListUsed,
                                                              pAhciPortTaskState->cbTransfer,
                                                              pAhciPortTaskState);
            }
            if (rc == VINF_VD_ASYNC_IO_FINISHED)
                rc = ahciTransferComplete(pAhciPort, pAhciPortTaskState);

            if (RT_FAILURE(rc))
                AssertMsgFailed(("%s: Failed to enqueue command %Rrc\n", __FUNCTION__, rc));
        }
        else
        {
            /* There is nothing left to do. Notify the guest. */
            ahciSendD2HFis(pAhciPort, pAhciPortTaskState, &pAhciPortTaskState->cmdFis[0], true);
            /* Add the task to the cache. */
            pAhciPort->aCachedTasks[pAhciPortTaskState->uTag] = pAhciPortTaskState;
        }
    }

    return true;
}

/* The async IO thread for one port. */
static DECLCALLBACK(int) ahciAsyncIOLoop(PPDMDEVINS pDevIns, PPDMTHREAD pThread)
{
    PAHCIPort pAhciPort = (PAHCIPort)pThread->pvUser;
    PAHCI     pAhci     = pAhciPort->CTX_SUFF(pAhci);
    PAHCIPORTTASKSTATE pAhciPortTaskState;
    int rc = VINF_SUCCESS;
    uint64_t u64StartTime = 0;
    uint64_t u64StopTime  = 0;
    uint32_t uIORequestsProcessed = 0;
    uint32_t uIOsPerSec = 0;

    ahciLog(("%s: Port %d entering async IO loop.\n", __FUNCTION__, pAhciPort->iLUN));

    if (pThread->enmState == PDMTHREADSTATE_INITIALIZING)
        return VINF_SUCCESS;

    /* We use only one task structure. */
    pAhciPortTaskState = (PAHCIPORTTASKSTATE)RTMemAllocZ(sizeof(AHCIPORTTASKSTATE));
    if (!pAhciPortTaskState)
    {
        AssertMsgFailed(("Failed to allocate task state memory\n"));
        return VERR_NO_MEMORY;
    }

    while(pThread->enmState == PDMTHREADSTATE_RUNNING)
    {
        uint32_t uQueuedTasksFinished = 0;

        /* New run to get number of I/O requests per second?. */
        if (!u64StartTime)
            u64StartTime = RTTimeMilliTS();

        ASMAtomicXchgBool(&pAhciPort->fAsyncIOThreadIdle, true);
        if (pAhci->fSignalIdle)
            RTSemEventSignal(pAhci->hEvtIdle);

        rc = RTSemEventWait(pAhciPort->AsyncIORequestSem, 1000);
        if (rc == VERR_TIMEOUT)
        {
            /* No I/O requests inbetween. Reset statistics and wait again. */
            pAhciPort->StatIORequestsPerSecond.c = 0;
            rc = RTSemEventWait(pAhciPort->AsyncIORequestSem, RT_INDEFINITE_WAIT);
        }

        if (RT_FAILURE(rc) || (pThread->enmState != PDMTHREADSTATE_RUNNING))
            break;

        AssertMsg(pAhciPort->pDrvBase, ("I/O thread without attached device?!\n"));

        ASMAtomicXchgBool(&pAhciPort->fAsyncIOThreadIdle, false);

        /*
         * To maximize the throughput of the controller we try to minimize the
         * number of world switches during interrupts by grouping as many
         * I/O requests together as possible.
         * On the other side we want to get minimal latency if the I/O load is low.
         * Thatswhy the number of I/O requests per second is measured and if it is over
         * a threshold the thread waits for other requests from the guest.
         */
        if (uIOsPerSec >= pAhci->cHighIOThreshold)
        {
            uint8_t uActWritePosPrev = pAhciPort->uActWritePos;

            Log(("%s: Waiting for more tasks to get queued\n", __FUNCTION__));

            do
            {
                /* Sleep some time. */
                RTThreadSleep(pAhci->cMillisToSleep);
                /* Check if we got some new requests inbetween. */
                if (uActWritePosPrev != pAhciPort->uActWritePos)
                {
                    uActWritePosPrev = pAhciPort->uActWritePos;
                    /*
                     * Check if the queue is full. If that is the case
                     * there is no point waiting another round.
                     */
                    if (   (   (pAhciPort->uActReadPos < uActWritePosPrev)
                            && (uActWritePosPrev - pAhciPort->uActReadPos) == AHCI_NR_COMMAND_SLOTS)
                        || (   (pAhciPort->uActReadPos > uActWritePosPrev)
                            && (RT_ELEMENTS(pAhciPort->ahciIOTasks) - pAhciPort->uActReadPos + uActWritePosPrev) == AHCI_NR_COMMAND_SLOTS)  )
                    {
                        Log(("%s: Queue full -> leaving\n", __FUNCTION__));
                        break;
                    }
                    Log(("%s: Another round\n", __FUNCTION__));
                }
                else /* No change break out of the loop. */
                {
#ifdef DEBUG
                    uint8_t uQueuedTasks;
                    if (pAhciPort->uActReadPos < uActWritePosPrev)
                        uQueuedTasks = uActWritePosPrev - pAhciPort->uActReadPos;
                    else
                        uQueuedTasks = RT_ELEMENTS(pAhciPort->ahciIOTasks) - pAhciPort->uActReadPos + uActWritePosPrev;

                    Log(("%s: %u Tasks are queued\n", __FUNCTION__, uQueuedTasks));
#endif
                    break;
                }
            } while (true);
        }

        ASMAtomicXchgBool(&pAhciPort->fNotificationSend, false);

        uint32_t cTasksToProcess = ASMAtomicXchgU32(&pAhciPort->uActTasksActive, 0);

        ahciLog(("%s: Processing %u requests\n", __FUNCTION__, cTasksToProcess));

        /* Process commands. */
        while (   (cTasksToProcess > 0)
               && RT_LIKELY(!pAhciPort->fPortReset))
        {
            int       iTxDir;
            uint8_t   uActTag;

            STAM_PROFILE_START(&pAhciPort->StatProfileProcessTime, a);

            ahciLog(("%s: uActWritePos=%u\n", __FUNCTION__, pAhciPort->uActWritePos));
            ahciLog(("%s: Before uActReadPos=%u\n", __FUNCTION__, pAhciPort->uActReadPos));

            pAhciPortTaskState->uATARegStatus = 0;
            pAhciPortTaskState->uATARegError  = 0;
            uActTag = pAhciPort->ahciIOTasks[pAhciPort->uActReadPos];

            pAhciPortTaskState->uTag = AHCI_TASK_GET_TAG(uActTag);
            AssertMsg(pAhciPortTaskState->uTag < AHCI_NR_COMMAND_SLOTS, ("%s: Invalid Tag number!!\n", __FUNCTION__));

            /** Set current command slot */
            pAhciPort->regCMD |= (AHCI_PORT_CMD_CCS_SHIFT(pAhciPortTaskState->uTag));

            /* Mark the task as processed by the HBA if this is a queued task so that it doesn't occur in the CI register anymore. */
            if (AHCI_TASK_IS_QUEUED(uActTag))
            {
                pAhciPortTaskState->fQueued = true;
                ASMAtomicOrU32(&pAhciPort->u32TasksFinished, (1 << pAhciPortTaskState->uTag));
            }
            else
            {
                pAhciPortTaskState->fQueued = false;
            }

            ahciPortTaskGetCommandFis(pAhciPort, pAhciPortTaskState);

            ahciLog(("%s: Got command at slot %d\n", __FUNCTION__, pAhciPortTaskState->uTag));

            if (!(pAhciPortTaskState->cmdFis[AHCI_CMDFIS_BITS] & AHCI_CMDFIS_C))
            {
                /* If the reset bit is set put the device into reset state. */
                if (pAhciPortTaskState->cmdFis[AHCI_CMDFIS_CTL] & AHCI_CMDFIS_CTL_SRST)
                {
                    ahciLog(("%s: Setting device into reset state\n", __FUNCTION__));
                    pAhciPort->fResetDevice = true;
                    ahciSendD2HFis(pAhciPort, pAhciPortTaskState, &pAhciPortTaskState->cmdFis[0], true);
                }
                else if (pAhciPort->fResetDevice) /* The bit is not set and we are in a reset state. */
                {
                    ahciFinishStorageDeviceReset(pAhciPort, pAhciPortTaskState);
                }
                /* TODO: We are not in a reset state update the control registers. */
            }
            else
            {
                iTxDir = ahciProcessCmd(pAhciPort, pAhciPortTaskState, &pAhciPortTaskState->cmdFis[0]);

                if (iTxDir != PDMBLOCKTXDIR_NONE)
                {
                    uint64_t uOffset;
                    size_t cbTransfer;
                    PPDMDATASEG pSegCurr;
                    PAHCIPORTTASKSTATESGENTRY pSGInfoCurr;

                    rc = ahciScatterGatherListCreate(pAhciPort, pAhciPortTaskState, (iTxDir == PDMBLOCKTXDIR_FROM_DEVICE) ? false : true);
                    if (RT_FAILURE(rc))
                        AssertMsgFailed(("%s: Failed to get number of list elments %Rrc\n", __FUNCTION__, rc));

                    STAM_REL_COUNTER_INC(&pAhciPort->StatDMA);

                    /* Initialize all values. */
                    uOffset     = pAhciPortTaskState->uOffset;
                    cbTransfer  = pAhciPortTaskState->cbTransfer;
                    pSegCurr    = &pAhciPortTaskState->pSGListHead[0];
                    pSGInfoCurr = pAhciPortTaskState->paSGEntries;

                    STAM_PROFILE_START(&pAhciPort->StatProfileReadWrite, a);

                    while (cbTransfer)
                    {
                        size_t cbProcess = (cbTransfer < pSegCurr->cbSeg) ? cbTransfer : pSegCurr->cbSeg;

                        AssertMsg(!(pSegCurr->cbSeg % 512), ("Buffer is not sector aligned cbSeg=%d\n", pSegCurr->cbSeg));
                        AssertMsg(!(uOffset % 512), ("Offset is not sector aligned %llu\n", uOffset));
                        AssertMsg(!(cbProcess % 512), ("Number of bytes to process is not sector aligned %lu\n", cbProcess));

                        if (iTxDir == PDMBLOCKTXDIR_FROM_DEVICE)
                        {
                            pAhciPort->Led.Asserted.s.fReading = pAhciPort->Led.Actual.s.fReading = 1;
                            rc = pAhciPort->pDrvBlock->pfnRead(pAhciPort->pDrvBlock, uOffset,
                                                               pSegCurr->pvSeg, cbProcess);
                            pAhciPort->Led.Actual.s.fReading = 0;
                            if (RT_FAILURE(rc))
                                AssertMsgFailed(("%s: Failed to read data %Rrc\n", __FUNCTION__, rc));

                            STAM_REL_COUNTER_ADD(&pAhciPort->StatBytesRead, cbProcess);
                        }
                        else
                        {
                            pAhciPort->Led.Asserted.s.fWriting = pAhciPort->Led.Actual.s.fWriting = 1;
                            rc = pAhciPort->pDrvBlock->pfnWrite(pAhciPort->pDrvBlock, uOffset,
                                                                pSegCurr->pvSeg, cbProcess);
                            pAhciPort->Led.Actual.s.fWriting = 0;
                            if (RT_FAILURE(rc))
                                AssertMsgFailed(("%s: Failed to write data %Rrc\n", __FUNCTION__, rc));

                            STAM_REL_COUNTER_ADD(&pAhciPort->StatBytesWritten, cbProcess);
                        }

                        /* Go to the next entry. */
                        uOffset    += cbProcess;
                        cbTransfer -= cbProcess;
                        pSegCurr++;
                        pSGInfoCurr++;
                    }

                    STAM_PROFILE_STOP(&pAhciPort->StatProfileReadWrite, a);

                    /* Cleanup. */
                    rc = ahciScatterGatherListDestroy(pAhciPort, pAhciPortTaskState);
                    if (RT_FAILURE(rc))
                        AssertMsgFailed(("Destroying task list failed rc=%Rrc\n", rc));

                    if (RT_LIKELY(!pAhciPort->fPortReset))
                    {
                        pAhciPortTaskState->cmdHdr.u32PRDBC = pAhciPortTaskState->cbTransfer;
                        pAhciPortTaskState->uATARegError = 0;
                        pAhciPortTaskState->uATARegStatus = ATA_STAT_READY | ATA_STAT_SEEK;
                        /* Write updated command header into memory of the guest. */
                        PDMDevHlpPhysWrite(pAhciPort->CTX_SUFF(pDevIns), pAhciPortTaskState->GCPhysCmdHdrAddr,
                                           &pAhciPortTaskState->cmdHdr, sizeof(CmdHdr));

                        if (pAhciPortTaskState->fQueued)
                            uQueuedTasksFinished |= (1 << pAhciPortTaskState->uTag);
                        else
                        {
                            /* Task is not queued send D2H FIS */
                            ahciSendD2HFis(pAhciPort, pAhciPortTaskState, &pAhciPortTaskState->cmdFis[0], true);
                        }

                        uIORequestsProcessed++;
                    }
                }
                else
                {
                    /* Nothing left to do. Notify the guest. */
                    ahciSendD2HFis(pAhciPort, pAhciPortTaskState, &pAhciPortTaskState->cmdFis[0], true);
                }

                STAM_PROFILE_STOP(&pAhciPort->StatProfileProcessTime, a);
            }

#ifdef DEBUG
            /* Be paranoid. */
            memset(&pAhciPortTaskState->cmdHdr, 0, sizeof(CmdHdr));
            memset(&pAhciPortTaskState->cmdFis, 0, AHCI_CMDFIS_TYPE_H2D_SIZE);
            pAhciPortTaskState->GCPhysCmdHdrAddr = 0;
            pAhciPortTaskState->uOffset = 0;
            pAhciPortTaskState->cbTransfer = 0;
            /* Make the port number invalid making it easier to track down bugs. */
            pAhciPort->ahciIOTasks[pAhciPort->uActReadPos] = 0xff;
#endif

            pAhciPort->uActReadPos++;
            pAhciPort->uActReadPos %= RT_ELEMENTS(pAhciPort->ahciIOTasks);
            ahciLog(("%s: After uActReadPos=%u\n", __FUNCTION__, pAhciPort->uActReadPos));
            cTasksToProcess--;
            if (!cTasksToProcess)
                cTasksToProcess = ASMAtomicXchgU32(&pAhciPort->uActTasksActive, 0);
        }

        if (uQueuedTasksFinished && RT_LIKELY(!pAhciPort->fPortReset))
            ahciSendSDBFis(pAhciPort, uQueuedTasksFinished, pAhciPortTaskState, true);

        uQueuedTasksFinished = 0;

        u64StopTime = RTTimeMilliTS();
        /* Check if one second has passed. */
        if (u64StopTime - u64StartTime >= 1000)
        {
            /* Calculate number of I/O requests per second. */
            uIOsPerSec = uIORequestsProcessed / ((u64StopTime - u64StartTime) / 1000);
            ahciLog(("%s: Processed %u requests in %llu ms -> %u requests/s\n", __FUNCTION__, uIORequestsProcessed, u64StopTime - u64StartTime, uIOsPerSec));
            u64StartTime = 0;
            uIORequestsProcessed = 0;
            /* For the release statistics. There is no macro to set the counter to a specific value. */
            pAhciPort->StatIORequestsPerSecond.c = uIOsPerSec;
        }
    }

    if (pAhci->fSignalIdle)
        RTSemEventSignal(pAhci->hEvtIdle);

    /* Free task state memory */
    if (pAhciPortTaskState->pSGListHead)
        RTMemFree(pAhciPortTaskState->pSGListHead);
    if (pAhciPortTaskState->paSGEntries)
        RTMemFree(pAhciPortTaskState->paSGEntries);
    if (pAhciPortTaskState->pvBufferUnaligned)
        RTMemFree(pAhciPortTaskState->pvBufferUnaligned);
    RTMemFree(pAhciPortTaskState);

    ahciLog(("%s: Port %d async IO thread exiting rc=%Rrc\n", __FUNCTION__, pAhciPort->iLUN, rc));
    return rc;
}

/**
 * Unblock the async I/O thread so it can respond to a state change.
 *
 * @returns VBox status code.
 * @param   pDevIns     The pcnet device instance.
 * @param   pThread     The send thread.
 */
static DECLCALLBACK(int) ahciAsyncIOLoopWakeUp(PPDMDEVINS pDevIns, PPDMTHREAD pThread)
{
    PAHCIPort pAhciPort = (PAHCIPort)pThread->pvUser;
    return RTSemEventSignal(pAhciPort->AsyncIORequestSem);
}

/**
 * Called when a media is mounted.
 *
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 */
static DECLCALLBACK(void) ahciMountNotify(PPDMIMOUNTNOTIFY pInterface)
{
    PAHCIPort pAhciPort = PDMIMOUNTNOTIFY_2_PAHCIPORT(pInterface);
    Log(("%s: changing LUN#%d\n", __FUNCTION__, pAhciPort->iLUN));

    /* Ignore the call if we're called while being attached. */
    if (!pAhciPort->pDrvBlock)
        return;

    pAhciPort->cTotalSectors = pAhciPort->pDrvBlock->pfnGetSize(pAhciPort->pDrvBlock) / 512;

    /*
     * Initialize registers
     */
    pAhciPort->regCMD  |= AHCI_PORT_CMD_CPS;
    ASMAtomicOrU32(&pAhciPort->regIS, AHCI_PORT_IS_CPDS | AHCI_PORT_IS_PRCS);
    pAhciPort->regSERR |= AHCI_PORT_SERR_N;
    if (pAhciPort->regIE & AHCI_PORT_IE_CPDE)
        ahciHbaSetInterrupt(pAhciPort->CTX_SUFF(pAhci), pAhciPort->iLUN);
}

/**
 * Called when a media is unmounted
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 */
static DECLCALLBACK(void) ahciUnmountNotify(PPDMIMOUNTNOTIFY pInterface)
{
    PAHCIPort pAhciPort = PDMIMOUNTNOTIFY_2_PAHCIPORT(pInterface);
    Log(("%s:\n", __FUNCTION__));

    pAhciPort->cTotalSectors = 0;

    /*
     * Inform the guest about the removed device.
     */
    pAhciPort->regSSTS = 0;
    pAhciPort->regCMD &= ~AHCI_PORT_CMD_CPS;
    ASMAtomicOrU32(&pAhciPort->regIS, AHCI_PORT_IS_CPDS | AHCI_PORT_IS_PRCS);
    pAhciPort->regSERR |= AHCI_PORT_SERR_N;
    if (pAhciPort->regIE & AHCI_PORT_IE_CPDE)
        ahciHbaSetInterrupt(pAhciPort->CTX_SUFF(pAhci), pAhciPort->iLUN);
}

/**
 * Destroy a driver instance.
 *
 * Most VM resources are freed by the VM. This callback is provided so that any non-VM
 * resources can be freed correctly.
 *
 * @param   pDevIns     The device instance data.
 */
static DECLCALLBACK(int) ahciDestruct(PPDMDEVINS pDevIns)
{
    PAHCI pAhci = PDMINS_2_DATA(pDevIns, PAHCI);
    int rc = VINF_SUCCESS;
    unsigned iActPort = 0;

    /*
     * At this point the async I/O thread is suspended and will not enter
     * this module again. So, no coordination is needed here and PDM
     * will take care of terminating and cleaning up the thread.
     */
    if (PDMCritSectIsInitialized(&pAhci->lock))
    {
        TMR3TimerDestroy(pAhci->CTX_SUFF(pHbaCccTimer));

        Log(("%s: Destruct every port\n", __FUNCTION__));
        for (iActPort = 0; iActPort < pAhci->cPortsImpl; iActPort++)
        {
            PAHCIPort pAhciPort = &pAhci->ahciPort[iActPort];

            if (pAhciPort->pAsyncIOThread)
            {
                /* Destroy the event semaphore. */
                rc = RTSemEventDestroy(pAhciPort->AsyncIORequestSem);
                if (RT_FAILURE(rc))
                {
                    Log(("%s: Destroying event semaphore for port %d failed rc=%Rrc\n", __FUNCTION__, iActPort, rc));
                }
            }

            /* Free all cached tasks. */
            for (uint32_t i = 0; i < AHCI_NR_COMMAND_SLOTS; i++)
            {
                if (pAhciPort->aCachedTasks[i])
                {
                    if (pAhciPort->aCachedTasks[i]->pSGListHead)
                        RTMemFree(pAhciPort->aCachedTasks[i]->pSGListHead);
                    if (pAhciPort->aCachedTasks[i]->paSGEntries)
                        RTMemFree(pAhciPort->aCachedTasks[i]->paSGEntries);

                    RTMemFree(pAhciPort->aCachedTasks[i]);
                }
            }
        }

        /* Destroy emulated ATA controllers. */
        for (unsigned i = 0; i < RT_ELEMENTS(pAhci->aCts); i++)
            ataControllerDestroy(&pAhci->aCts[i]);

        RTSemEventDestroy(pAhci->hEvtIdle);
        pAhci->hEvtIdle = NIL_RTSEMEVENT;

        PDMR3CritSectDelete(&pAhci->lock);
    }

    return rc;
}

/**
 * Configure the attached device for a port.
 *
 * @returns VBox status code
 * @param   pDevIns     The device instance data.
 * @param   pAhciPort   The port for which the device is to be configured.
 */
static int ahciConfigureLUN(PPDMDEVINS pDevIns, PAHCIPort pAhciPort)
{
    int          rc = VINF_SUCCESS;
    PDMBLOCKTYPE enmType;

    /*
     * Query the block and blockbios interfaces.
     */
    pAhciPort->pDrvBlock = (PDMIBLOCK *)pAhciPort->pDrvBase->pfnQueryInterface(pAhciPort->pDrvBase, PDMINTERFACE_BLOCK);
    if (!pAhciPort->pDrvBlock)
    {
        AssertMsgFailed(("Configuration error: LUN#%d hasn't a block interface!\n", pAhciPort->iLUN));
        return VERR_PDM_MISSING_INTERFACE;
    }
    pAhciPort->pDrvBlockBios = (PDMIBLOCKBIOS *)pAhciPort->pDrvBase->pfnQueryInterface(pAhciPort->pDrvBase, PDMINTERFACE_BLOCK_BIOS);
    if (!pAhciPort->pDrvBlockBios)
    {
        AssertMsgFailed(("Configuration error: LUN#%d hasn't a block BIOS interface!\n", pAhciPort->iLUN));
        return VERR_PDM_MISSING_INTERFACE;
    }

    pAhciPort->pDrvMount = (PDMIMOUNT *)pAhciPort->pDrvBase->pfnQueryInterface(pAhciPort->pDrvBase, PDMINTERFACE_MOUNT);

    /* Try to get the optional async block interface. */
    pAhciPort->pDrvBlockAsync = (PDMIBLOCKASYNC *)pAhciPort->pDrvBase->pfnQueryInterface(pAhciPort->pDrvBase, PDMINTERFACE_BLOCK_ASYNC);

    /*
     * Validate type.
     */
    enmType = pAhciPort->pDrvBlock->pfnGetType(pAhciPort->pDrvBlock);

    if (   enmType != PDMBLOCKTYPE_HARD_DISK
        && enmType != PDMBLOCKTYPE_CDROM
        && enmType != PDMBLOCKTYPE_DVD)
    {
        AssertMsgFailed(("Configuration error: LUN#%d isn't a disk or cd/dvd. enmType=%d\n", pAhciPort->iLUN, enmType));
        return VERR_PDM_UNSUPPORTED_BLOCK_TYPE;
    }

    if (   (enmType == PDMBLOCKTYPE_CDROM || enmType == PDMBLOCKTYPE_DVD)
        && !pAhciPort->pDrvMount)
    {
        AssertMsgFailed(("Internal error: CD/DVD-ROM without a mountable interface\n"));
        return VERR_INTERNAL_ERROR;
    }
    pAhciPort->fATAPI = (enmType == PDMBLOCKTYPE_CDROM || enmType == PDMBLOCKTYPE_DVD);

    if (pAhciPort->fATAPI)
    {
        pAhciPort->cTotalSectors = pAhciPort->pDrvBlock->pfnGetSize(pAhciPort->pDrvBlock) / 2048;
        pAhciPort->PCHSGeometry.cCylinders = 0;
        pAhciPort->PCHSGeometry.cHeads     = 0;
        pAhciPort->PCHSGeometry.cSectors   = 0;
        LogRel(("AHCI LUN#%d: CD/DVD, total number of sectors %Ld\n", pAhciPort->iLUN, pAhciPort->cTotalSectors));
    }
    else
    {
        pAhciPort->cTotalSectors = pAhciPort->pDrvBlock->pfnGetSize(pAhciPort->pDrvBlock) / 512;
        rc = pAhciPort->pDrvBlockBios->pfnGetPCHSGeometry(pAhciPort->pDrvBlockBios,
                                                          &pAhciPort->PCHSGeometry);
        if (rc == VERR_PDM_MEDIA_NOT_MOUNTED)
        {
            pAhciPort->PCHSGeometry.cCylinders = 0;
            pAhciPort->PCHSGeometry.cHeads     = 16; /*??*/
            pAhciPort->PCHSGeometry.cSectors   = 63; /*??*/
        }
        else if (rc == VERR_PDM_GEOMETRY_NOT_SET)
        {
            pAhciPort->PCHSGeometry.cCylinders = 0; /* autodetect marker */
            rc = VINF_SUCCESS;
        }
        AssertRC(rc);

        if (   pAhciPort->PCHSGeometry.cCylinders == 0
            || pAhciPort->PCHSGeometry.cHeads == 0
            || pAhciPort->PCHSGeometry.cSectors == 0)
        {
            uint64_t cCylinders = pAhciPort->cTotalSectors / (16 * 63);
            pAhciPort->PCHSGeometry.cCylinders = RT_MAX(RT_MIN(cCylinders, 16383), 1);
            pAhciPort->PCHSGeometry.cHeads = 16;
            pAhciPort->PCHSGeometry.cSectors = 63;
            /* Set the disk geometry information. Ignore errors. */
            pAhciPort->pDrvBlockBios->pfnSetPCHSGeometry(pAhciPort->pDrvBlockBios,
                                                         &pAhciPort->PCHSGeometry);
            rc = VINF_SUCCESS;
        }
        LogRel(("AHCI: LUN#%d: disk, PCHS=%u/%u/%u, total number of sectors %Ld\n",
                 pAhciPort->iLUN, pAhciPort->PCHSGeometry.cCylinders,
                 pAhciPort->PCHSGeometry.cHeads, pAhciPort->PCHSGeometry.cSectors,
                 pAhciPort->cTotalSectors));
    }
    return rc;
}

static bool ahciWaitForAllAsyncIOIsFinished(PPDMDEVINS pDevIns, unsigned cMillies)
{
    PAHCI     pAhci = PDMINS_2_DATA(pDevIns, PAHCI);
    uint64_t  u64Start;
    PAHCIPort pAhciPort;
    bool      fAllFinished;

    ASMAtomicWriteBool(&pAhci->fSignalIdle, true);
    u64Start = RTTimeMilliTS();
    for (;;)
    {
        fAllFinished = true;
        for (uint32_t i = 0; i < RT_ELEMENTS(pAhci->ahciPort); i++)
        {
            pAhciPort = &pAhci->ahciPort[i];

            if (pAhciPort->pDrvBase)
            {
                if (pAhciPort->fAsyncInterface)
                    fAllFinished &= (pAhciPort->uActTasksActive == 0);
                else
                    fAllFinished &= ((pAhciPort->uActTasksActive == 0) && (pAhciPort->fAsyncIOThreadIdle));

                if (!fAllFinished)
                    break;
            }
        }
        if (   fAllFinished
            || RTTimeMilliTS() - u64Start >= cMillies)
            break;

        /* Wait for a port to signal idleness. */
        int rc = RTSemEventWait(pAhci->hEvtIdle, 100 /*ms*/);
        AssertMsg(RT_SUCCESS(rc) || rc == VERR_TIMEOUT, ("%Rrc\n", rc)); NOREF(rc);
    }

    ASMAtomicWriteBool(&pAhci->fSignalIdle, false);
    return fAllFinished;
}

static DECLCALLBACK(int) ahciSavePrep(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PAHCI pAhci = PDMINS_2_DATA(pDevIns, PAHCI);

    if (!ahciWaitForAllAsyncIOIsFinished(pDevIns, 20000))
        AssertMsgFailed(("One port is still active\n"));

    for (uint32_t i = 0; i < RT_ELEMENTS(pAhci->aCts); i++)
    {
        int rc;

        rc = ataControllerSavePrep(&pAhci->aCts[i], pSSM);
        if (RT_FAILURE(rc))
            return rc;
    }

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) ahciLoadPrep(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PAHCI pAhci = PDMINS_2_DATA(pDevIns, PAHCI);

    for (uint32_t i = 0; i < RT_ELEMENTS(pAhci->aCts); i++)
    {
        int rc;

        rc = ataControllerLoadPrep(&pAhci->aCts[i], pSSM);
        if (RT_FAILURE(rc))
            return rc;
    }

    return VINF_SUCCESS;
}

/**
 * Suspend notification.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 */
static DECLCALLBACK(void) ahciSuspend(PPDMDEVINS pDevIns)
{
    PAHCI    pAhci = PDMINS_2_DATA(pDevIns, PAHCI);

    if (!ahciWaitForAllAsyncIOIsFinished(pDevIns, 20000))
        AssertMsgFailed(("AHCI: One port is still active\n"));

    Log(("%s:\n", __FUNCTION__));
    for (uint32_t i = 0; i < RT_ELEMENTS(pAhci->aCts); i++)
    {
        ataControllerSuspend(&pAhci->aCts[i]);
    }
    return;
}


/**
 * Resume notification.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 */
static DECLCALLBACK(void) ahciResume(PPDMDEVINS pDevIns)
{
    PAHCI    pAhci = PDMINS_2_DATA(pDevIns, PAHCI);

    Log(("%s:\n", __FUNCTION__));
    for (uint32_t i = 0; i < RT_ELEMENTS(pAhci->aCts); i++)
    {
        ataControllerResume(&pAhci->aCts[i]);
    }
    return;
}

/**
 * @copydoc FNDEVSSMLIVEEXEC
 */
static DECLCALLBACK(int) ahciLiveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uPass)
{
    PAHCI pThis = PDMINS_2_DATA(pDevIns, PAHCI);

    /* config. */
    SSMR3PutU32(pSSM, pThis->cPortsImpl);
    for (uint32_t i = 0; i < AHCI_MAX_NR_PORTS_IMPL; i++)
    {
        SSMR3PutBool(pSSM, pThis->ahciPort[i].pDrvBase != NULL);
        SSMR3PutStrZ(pSSM, pThis->ahciPort[i].szSerialNumber);
        SSMR3PutStrZ(pSSM, pThis->ahciPort[i].szFirmwareRevision);
        SSMR3PutStrZ(pSSM, pThis->ahciPort[i].szModelNumber);
    }

    static const char *s_apszIdeEmuPortNames[4] = { "PrimaryMaster", "PrimarySlave", "SecondaryMaster", "SecondarySlave" };
    for (size_t i = 0; i < RT_ELEMENTS(s_apszIdeEmuPortNames); i++)
    {
        uint32_t iPort;
        int rc = CFGMR3QueryU32Def(pDevIns->pCfgHandle, s_apszIdeEmuPortNames[i], &iPort, i);
        AssertRCReturn(rc, rc);
        SSMR3PutU32(pSSM, iPort);
    }

    return VINF_SSM_DONT_CALL_AGAIN;
}

/**
 * @copydoc FNDEVSSMSAVEEXEC
 */
static DECLCALLBACK(int) ahciSaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PAHCI pThis = PDMINS_2_DATA(pDevIns, PAHCI);
    uint32_t i;
    int rc;

    Assert(!pThis->f8ByteMMIO4BytesWrittenSuccessfully);

    /* The config */
    rc = ahciLiveExec(pDevIns, pSSM, SSM_PASS_FINAL);
    AssertRCReturn(rc, rc);

    /* The main device structure. */
    SSMR3PutU32(pSSM, pThis->regHbaCap);
    SSMR3PutU32(pSSM, pThis->regHbaCtrl);
    SSMR3PutU32(pSSM, pThis->regHbaIs);
    SSMR3PutU32(pSSM, pThis->regHbaPi);
    SSMR3PutU32(pSSM, pThis->regHbaVs);
    SSMR3PutU32(pSSM, pThis->regHbaCccCtl);
    SSMR3PutU32(pSSM, pThis->regHbaCccPorts);
    SSMR3PutU8(pSSM, pThis->uCccPortNr);
    SSMR3PutU64(pSSM, pThis->uCccTimeout);
    SSMR3PutU32(pSSM, pThis->uCccNr);
    SSMR3PutU32(pSSM, pThis->uCccCurrentNr);
    SSMR3PutU32(pSSM, pThis->u32PortsInterrupted);
    SSMR3PutBool(pSSM, pThis->fReset);
    SSMR3PutBool(pSSM, pThis->f64BitAddr);
    SSMR3PutBool(pSSM, pThis->fR0Enabled);
    SSMR3PutBool(pSSM, pThis->fGCEnabled);

    /* Now every port. */
    for (i = 0; i < AHCI_MAX_NR_PORTS_IMPL; i++)
    {
        Assert(pThis->ahciPort[i].uActTasksActive == 0);
        SSMR3PutU32(pSSM, pThis->ahciPort[i].regCLB);
        SSMR3PutU32(pSSM, pThis->ahciPort[i].regCLBU);
        SSMR3PutU32(pSSM, pThis->ahciPort[i].regFB);
        SSMR3PutU32(pSSM, pThis->ahciPort[i].regFBU);
        SSMR3PutGCPhys(pSSM, pThis->ahciPort[i].GCPhysAddrClb);
        SSMR3PutGCPhys(pSSM, pThis->ahciPort[i].GCPhysAddrFb);
        SSMR3PutU32(pSSM, pThis->ahciPort[i].regIS);
        SSMR3PutU32(pSSM, pThis->ahciPort[i].regIE);
        SSMR3PutU32(pSSM, pThis->ahciPort[i].regCMD);
        SSMR3PutU32(pSSM, pThis->ahciPort[i].regTFD);
        SSMR3PutU32(pSSM, pThis->ahciPort[i].regSIG);
        SSMR3PutU32(pSSM, pThis->ahciPort[i].regSSTS);
        SSMR3PutU32(pSSM, pThis->ahciPort[i].regSCTL);
        SSMR3PutU32(pSSM, pThis->ahciPort[i].regSERR);
        SSMR3PutU32(pSSM, pThis->ahciPort[i].regSACT);
        SSMR3PutU32(pSSM, pThis->ahciPort[i].regCI);
        SSMR3PutU32(pSSM, pThis->ahciPort[i].PCHSGeometry.cCylinders);
        SSMR3PutU32(pSSM, pThis->ahciPort[i].PCHSGeometry.cHeads);
        SSMR3PutU32(pSSM, pThis->ahciPort[i].PCHSGeometry.cSectors);
        SSMR3PutU64(pSSM, pThis->ahciPort[i].cTotalSectors);
        SSMR3PutU32(pSSM, pThis->ahciPort[i].cMultSectors);
        SSMR3PutU8(pSSM, pThis->ahciPort[i].uATATransferMode);
        SSMR3PutBool(pSSM, pThis->ahciPort[i].fResetDevice);

        /* No need to save */
        SSMR3PutU8(pSSM, pThis->ahciPort[i].uActWritePos);
        SSMR3PutU8(pSSM, pThis->ahciPort[i].uActReadPos);
        SSMR3PutBool(pSSM, pThis->ahciPort[i].fPoweredOn);
        SSMR3PutBool(pSSM, pThis->ahciPort[i].fSpunUp);
        SSMR3PutU32(pSSM, pThis->ahciPort[i].u32TasksFinished);
        SSMR3PutU32(pSSM, pThis->ahciPort[i].u32QueuedTasksFinished);
    }

    /* Now the emulated ata controllers. */
    for (i = 0; i < RT_ELEMENTS(pThis->aCts); i++)
    {
        rc = ataControllerSaveExec(&pThis->aCts[i], pSSM);
        if (RT_FAILURE(rc))
            return rc;
    }

    return SSMR3PutU32(pSSM, UINT32_MAX); /* sanity/terminator */
}

/**
 * Loads a saved AHCI device state.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSM  The handle to the saved state.
 * @param   uVersion  The data unit version number.
 * @param   uPass           The data pass.
 */
static DECLCALLBACK(int) ahciLoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    PAHCI pThis = PDMINS_2_DATA(pDevIns, PAHCI);
    uint32_t u32;
    uint32_t i;
    int rc;

    if (    uVersion != AHCI_SAVED_STATE_VERSION
        &&  uVersion != AHCI_SAVED_STATE_VERSION_VBOX_30)
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;

    /* Verify config. */
    if (uVersion > AHCI_SAVED_STATE_VERSION_VBOX_30)
    {
        rc = SSMR3GetU32(pSSM, &u32);
        AssertRCReturn(rc, rc);
        if (u32 != pThis->cPortsImpl)
        {
            LogRel(("AHCI: Config mismatch: cPortsImpl - saved=%u config=%u\n", u32, pThis->cPortsImpl));
            if (    u32 < pThis->cPortsImpl
                ||  u32 > AHCI_MAX_NR_PORTS_IMPL)
                return VERR_SSM_LOAD_CONFIG_MISMATCH;
        }

        for (uint32_t i = 0; i < AHCI_MAX_NR_PORTS_IMPL; i++)
        {
            bool fInUse;
            rc = SSMR3GetBool(pSSM, &fInUse);
            AssertRCReturn(rc, rc);
            if (fInUse != (pThis->ahciPort[i].pDrvBase != NULL))
            {
                LogRel(("AHCI: Port %u config mismatch: fInUse - saved=%RTbool config=%RTbool\n",
                        i, fInUse, (pThis->ahciPort[i].pDrvBase != NULL)));
                return VERR_SSM_LOAD_CONFIG_MISMATCH;
            }

            char szSerialNumber[AHCI_SERIAL_NUMBER_LENGTH+1];
            rc = SSMR3GetStrZ(pSSM, szSerialNumber,     sizeof(szSerialNumber));
            AssertRCReturn(rc, rc);
            if (strcmp(szSerialNumber, pThis->ahciPort[i].szSerialNumber))
                LogRel(("AHCI: Port %u config mismatch: Serial number - saved='%s' config='%s'\n",
                        i, szSerialNumber, pThis->ahciPort[i].szSerialNumber));

            char szFirmwareRevision[AHCI_FIRMWARE_REVISION_LENGTH+1];
            rc = SSMR3GetStrZ(pSSM, szFirmwareRevision, sizeof(szFirmwareRevision));
            AssertRCReturn(rc, rc);
            if (strcmp(szFirmwareRevision, pThis->ahciPort[i].szFirmwareRevision))
                LogRel(("AHCI: Port %u config mismatch: Firmware revision - saved='%s' config='%s'\n",
                        i, szFirmwareRevision, pThis->ahciPort[i].szFirmwareRevision));

            char szModelNumber[AHCI_MODEL_NUMBER_LENGTH+1];
            rc = SSMR3GetStrZ(pSSM, szModelNumber,      sizeof(szModelNumber));
            AssertRCReturn(rc, rc);
            if (strcmp(szModelNumber, pThis->ahciPort[i].szModelNumber))
                LogRel(("AHCI: Port %u config mismatch: Model number - saved='%s' config='%s'\n",
                        i, szModelNumber, pThis->ahciPort[i].szModelNumber));
        }

        static const char *s_apszIdeEmuPortNames[4] = { "PrimaryMaster", "PrimarySlave", "SecondaryMaster", "SecondarySlave" };
        for (size_t i = 0; i < RT_ELEMENTS(s_apszIdeEmuPortNames); i++)
        {
            uint32_t iPort;
            rc = CFGMR3QueryU32Def(pDevIns->pCfgHandle, s_apszIdeEmuPortNames[i], &iPort, i);
            AssertRCReturn(rc, rc);

            uint32_t iPortSaved;
            rc = SSMR3GetU32(pSSM, &iPortSaved);
            AssertRCReturn(rc, rc);

            if (iPortSaved != iPort)
            {
                LogRel(("AHCI: IDE %s config mismatch: saved=%u config=%u\n",
                        s_apszIdeEmuPortNames[i], iPortSaved, iPort));
                return VERR_SSM_LOAD_CONFIG_MISMATCH;
            }
        }
    }

    if (uPass == SSM_PASS_FINAL)
    {
        /* Restore data. */

        /* The main device structure. */
        SSMR3GetU32(pSSM, &pThis->regHbaCap);
        SSMR3GetU32(pSSM, &pThis->regHbaCtrl);
        SSMR3GetU32(pSSM, &pThis->regHbaIs);
        SSMR3GetU32(pSSM, &pThis->regHbaPi);
        SSMR3GetU32(pSSM, &pThis->regHbaVs);
        SSMR3GetU32(pSSM, &pThis->regHbaCccCtl);
        SSMR3GetU32(pSSM, &pThis->regHbaCccPorts);
        SSMR3GetU8(pSSM, &pThis->uCccPortNr);
        SSMR3GetU64(pSSM, &pThis->uCccTimeout);
        SSMR3GetU32(pSSM, &pThis->uCccNr);
        SSMR3GetU32(pSSM, &pThis->uCccCurrentNr);

        SSMR3GetU32(pSSM, &pThis->u32PortsInterrupted);
        SSMR3GetBool(pSSM, &pThis->fReset);
        SSMR3GetBool(pSSM, &pThis->f64BitAddr);
        SSMR3GetBool(pSSM, &pThis->fR0Enabled);
        SSMR3GetBool(pSSM, &pThis->fGCEnabled);

        /* Now every port. */
        for (i = 0; i < AHCI_MAX_NR_PORTS_IMPL; i++)
        {
            SSMR3GetU32(pSSM, &pThis->ahciPort[i].regCLB);
            SSMR3GetU32(pSSM, &pThis->ahciPort[i].regCLBU);
            SSMR3GetU32(pSSM, &pThis->ahciPort[i].regFB);
            SSMR3GetU32(pSSM, &pThis->ahciPort[i].regFBU);
            SSMR3GetGCPhys(pSSM, (RTGCPHYS *)&pThis->ahciPort[i].GCPhysAddrClb);
            SSMR3GetGCPhys(pSSM, (RTGCPHYS *)&pThis->ahciPort[i].GCPhysAddrFb);
            SSMR3GetU32(pSSM, (uint32_t *)&pThis->ahciPort[i].regIS);
            SSMR3GetU32(pSSM, &pThis->ahciPort[i].regIE);
            SSMR3GetU32(pSSM, &pThis->ahciPort[i].regCMD);
            SSMR3GetU32(pSSM, &pThis->ahciPort[i].regTFD);
            SSMR3GetU32(pSSM, &pThis->ahciPort[i].regSIG);
            SSMR3GetU32(pSSM, &pThis->ahciPort[i].regSSTS);
            SSMR3GetU32(pSSM, &pThis->ahciPort[i].regSCTL);
            SSMR3GetU32(pSSM, &pThis->ahciPort[i].regSERR);
            SSMR3GetU32(pSSM, (uint32_t *)&pThis->ahciPort[i].regSACT);
            SSMR3GetU32(pSSM, (uint32_t *)&pThis->ahciPort[i].regCI);
            SSMR3GetU32(pSSM, &pThis->ahciPort[i].PCHSGeometry.cCylinders);
            SSMR3GetU32(pSSM, &pThis->ahciPort[i].PCHSGeometry.cHeads);
            SSMR3GetU32(pSSM, &pThis->ahciPort[i].PCHSGeometry.cSectors);
            SSMR3GetU64(pSSM, &pThis->ahciPort[i].cTotalSectors);
            SSMR3GetU32(pSSM, &pThis->ahciPort[i].cMultSectors);
            SSMR3GetU8(pSSM, &pThis->ahciPort[i].uATATransferMode);
            SSMR3GetBool(pSSM, &pThis->ahciPort[i].fResetDevice);

            if (uVersion <= AHCI_SAVED_STATE_VERSION_VBOX_30)
                SSMR3Skip(pSSM, AHCI_NR_COMMAND_SLOTS * sizeof(uint8_t)); /* no active data here */

            SSMR3GetU8(pSSM, &pThis->ahciPort[i].uActWritePos);
            SSMR3GetU8(pSSM, &pThis->ahciPort[i].uActReadPos);
            SSMR3GetBool(pSSM, &pThis->ahciPort[i].fPoweredOn);
            SSMR3GetBool(pSSM, &pThis->ahciPort[i].fSpunUp);
            SSMR3GetU32(pSSM, (uint32_t *)&pThis->ahciPort[i].u32TasksFinished);
            SSMR3GetU32(pSSM, (uint32_t *)&pThis->ahciPort[i].u32QueuedTasksFinished);
        }

        /* Now the emulated ata controllers. */
        for (i = 0; i < RT_ELEMENTS(pThis->aCts); i++)
        {
            rc = ataControllerLoadExec(&pThis->aCts[i], pSSM);
            if (RT_FAILURE(rc))
                return rc;
        }

        rc = SSMR3GetU32(pSSM, &u32);
        if (RT_FAILURE(rc))
            return rc;
        AssertMsgReturn(u32 == UINT32_MAX, ("%#x\n", u32), VERR_SSM_DATA_UNIT_FORMAT_CHANGED);
    }

    return VINF_SUCCESS;
}

/**
 * Detach notification.
 *
 * One harddisk at one port has been unplugged.
 * The VM is suspended at this point.
 *
 * @param   pDevIns     The device instance.
 * @param   iLUN        The logical unit which is being detached.
 * @param   fFlags      Flags, combination of the PDMDEVATT_FLAGS_* \#defines.
 */
static DECLCALLBACK(void) ahciDetach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    PAHCI           pAhci = PDMINS_2_DATA(pDevIns, PAHCI);
    PAHCIPort       pAhciPort = &pAhci->ahciPort[iLUN];
    int             rc = VINF_SUCCESS;

    Log(("%s:\n", __FUNCTION__));

    AssertMsg(iLUN < pAhci->cPortsImpl, ("iLUN=%u", iLUN));

    if (!pAhciPort->fAsyncInterface)
    {
        int rcThread;
        /* Destroy the thread. */
        rc = PDMR3ThreadDestroy(pAhciPort->pAsyncIOThread, &rcThread);
        if (RT_FAILURE(rc) || RT_FAILURE(rcThread))
            AssertMsgFailed(("%s Failed to destroy async IO thread rc=%Rrc rcThread=%Rrc\n", __FUNCTION__, rc, rcThread));

        rc = RTSemEventDestroy(pAhciPort->AsyncIORequestSem);
        if (RT_FAILURE(rc))
            AssertMsgFailed(("%s: Failed to destroy the event semaphore rc=%Rrc.\n", __FUNCTION__, rc));
    }

    /*
     * Zero some important members.
     */
    pAhciPort->pDrvBase = NULL;
    pAhciPort->pDrvBlock = NULL;
    pAhciPort->pDrvBlockAsync = NULL;
    pAhciPort->pDrvBlockBios = NULL;
}

/**
 * Attach command.
 *
 * This is called when we change block driver for one port.
 * The VM is suspended at this point.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   iLUN        The logical unit which is being detached.
 * @param   fFlags      Flags, combination of the PDMDEVATT_FLAGS_* \#defines.
 */
static DECLCALLBACK(int)  ahciAttach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    PAHCI       pAhci = PDMINS_2_DATA(pDevIns, PAHCI);
    PAHCIPort   pAhciPort = &pAhci->ahciPort[iLUN];
    int         rc;

    Log(("%s:\n", __FUNCTION__));

    /* the usual paranoia */
    AssertMsg(iLUN < pAhci->cPortsImpl, ("iLUN=%u", iLUN));
    AssertRelease(!pAhciPort->pDrvBase);
    AssertRelease(!pAhciPort->pDrvBlock);
    AssertRelease(!pAhciPort->pDrvBlockAsync);
    Assert(pAhciPort->iLUN == iLUN);

    /*
     * Try attach the block device and get the interfaces,
     * required as well as optional.
     */
    rc = PDMDevHlpDriverAttach(pDevIns, pAhciPort->iLUN, &pAhciPort->IBase, &pAhciPort->pDrvBase, NULL);
    if (RT_SUCCESS(rc))
        rc = ahciConfigureLUN(pDevIns, pAhciPort);
    else
        AssertMsgFailed(("Failed to attach LUN#%d. rc=%Rrc\n", pAhciPort->iLUN, rc));

    if (RT_FAILURE(rc))
    {
        pAhciPort->pDrvBase = NULL;
        pAhciPort->pDrvBlock = NULL;
    }
    else
    {
        char szName[24];
        RTStrPrintf(szName, sizeof(szName), "Port%d", iLUN);

        if (pAhciPort->pDrvBlockAsync)
        {
            pAhciPort->fAsyncInterface = true;
        }
        else
        {
            pAhciPort->fAsyncInterface = false;

            /* Create event semaphore. */
            rc = RTSemEventCreate(&pAhciPort->AsyncIORequestSem);
            if (RT_FAILURE(rc))
            {
                Log(("%s: Failed to create event semaphore for %s.\n", __FUNCTION__, szName));
                return rc;
            }

            /* Create the async IO thread. */
            rc = PDMDevHlpPDMThreadCreate(pDevIns, &pAhciPort->pAsyncIOThread, pAhciPort, ahciAsyncIOLoop, ahciAsyncIOLoopWakeUp, 0,
                                          RTTHREADTYPE_IO, szName);
            if (RT_FAILURE(rc))
            {
                AssertMsgFailed(("%s: Async IO Thread creation for %s failed rc=%d\n", __FUNCTION__, szName, rc));
                return rc;
            }
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
static DECLCALLBACK(void) ahciReset(PPDMDEVINS pDevIns)
{
    PAHCI pAhci = PDMINS_2_DATA(pDevIns, PAHCI);

    if (!ahciWaitForAllAsyncIOIsFinished(pDevIns, 20000))
        AssertMsgFailed(("AHCI: One port is still active\n"));

    ahciHBAReset(pAhci);

    /* Hardware reset for the ports. */
    for (uint32_t i = 0; i < RT_ELEMENTS(pAhci->ahciPort); i++)
        ahciPortHwReset(&pAhci->ahciPort[i]);

    for (uint32_t i = 0; i < RT_ELEMENTS(pAhci->aCts); i++)
        ataControllerReset(&pAhci->aCts[i]);
}

/**
 * Poweroff notification.
 *
 * @returns nothing
 * @param   pDevIns Pointer to the device instance
 */
static DECLCALLBACK(void) ahciPowerOff(PPDMDEVINS pDevIns)
{
    PAHCI pAhci = PDMINS_2_DATA(pDevIns, PAHCI);

    if (!ahciWaitForAllAsyncIOIsFinished(pDevIns, 20000))
        AssertMsgFailed(("AHCI: One port is still active\n"));

    for (uint32_t i = 0; i < RT_ELEMENTS(pAhci->aCts); i++)
        ataControllerPowerOff(&pAhci->aCts[i]);
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
static DECLCALLBACK(int) ahciConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfgHandle)
{
    PAHCI      pThis = PDMINS_2_DATA(pDevIns, PAHCI);
    PPDMIBASE  pBase;
    int        rc = VINF_SUCCESS;
    unsigned   i = 0;
    bool       fGCEnabled = false;
    bool       fR0Enabled = false;
    uint32_t   cbTotalBufferSize = 0;

    /*
     * Validate and read configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "GCEnabled\0"
                                          "R0Enabled\0"
                                          "PrimaryMaster\0"
                                          "PrimarySlave\0"
                                          "SecondaryMaster\0"
                                          "SecondarySlave\0"
                                          "PortCount\0"
                                          "UseAsyncInterfaceIfAvailable\0"
                                          "HighIOThreshold\0"
                                          "MillisToSleep\0"))
        return PDMDEV_SET_ERROR(pDevIns, VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES,
                                N_("AHCI configuration error: unknown option specified"));

    rc = CFGMR3QueryBoolDef(pCfgHandle, "GCEnabled", &fGCEnabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("AHCI configuration error: failed to read GCEnabled as boolean"));
    Log(("%s: fGCEnabled=%d\n", __FUNCTION__, fGCEnabled));

    rc = CFGMR3QueryBoolDef(pCfgHandle, "R0Enabled", &fR0Enabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("AHCI configuration error: failed to read R0Enabled as boolean"));
    Log(("%s: fR0Enabled=%d\n", __FUNCTION__, fR0Enabled));

    rc = CFGMR3QueryU32Def(pCfgHandle, "PortCount", &pThis->cPortsImpl, AHCI_MAX_NR_PORTS_IMPL);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("AHCI configuration error: failed to read PortCount as integer"));
    Log(("%s: cPortsImpl=%u\n", __FUNCTION__, pThis->cPortsImpl));
    if (pThis->cPortsImpl > AHCI_MAX_NR_PORTS_IMPL)
        return PDMDevHlpVMSetError(pDevIns, VERR_INVALID_PARAMETER, RT_SRC_POS,
                                   N_("AHCI configuration error: PortCount=%u should not exceed %u"),
                                   pThis->cPortsImpl, AHCI_MAX_NR_PORTS_IMPL);
    if (pThis->cPortsImpl < 1)
        return PDMDevHlpVMSetError(pDevIns, VERR_INVALID_PARAMETER, RT_SRC_POS,
                                   N_("AHCI configuration error: PortCount=%u should be at least 1"),
                                   pThis->cPortsImpl);

    rc = CFGMR3QueryBoolDef(pCfgHandle, "UseAsyncInterfaceIfAvailable", &pThis->fUseAsyncInterfaceIfAvailable, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("AHCI configuration error: failed to read UseAsyncInterfaceIfAvailable as boolean"));
    rc = CFGMR3QueryU32Def(pCfgHandle, "HighIOThreshold", &pThis->cHighIOThreshold, ~0);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("AHCI configuration error: failed to read HighIOThreshold as integer"));
    rc = CFGMR3QueryU32Def(pCfgHandle, "MillisToSleep", &pThis->cMillisToSleep, 0);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("AHCI configuration error: failed to read MillisToSleep as integer"));

    pThis->fR0Enabled = fR0Enabled;
    pThis->fGCEnabled = fGCEnabled;
    pThis->pDevInsR3 = pDevIns;
    pThis->pDevInsR0 = PDMDEVINS_2_R0PTR(pDevIns);
    pThis->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);
    pThis->hEvtIdle  = NIL_RTSEMEVENT;

    PCIDevSetVendorId    (&pThis->dev, 0x8086); /* Intel */
    PCIDevSetDeviceId    (&pThis->dev, 0x2829); /* ICH-8M */
    PCIDevSetCommand     (&pThis->dev, 0x0000);
    PCIDevSetRevisionId  (&pThis->dev, 0x02);
    PCIDevSetClassProg   (&pThis->dev, 0x01);
    PCIDevSetClassSub    (&pThis->dev, 0x06);
    PCIDevSetClassBase   (&pThis->dev, 0x01);
    PCIDevSetBaseAddress (&pThis->dev, 5, false, false, false, 0x00000000);

    pThis->dev.config[0x34] = 0x80; /* Capability pointer. */

    PCIDevSetInterruptLine(&pThis->dev, 0x00);
    PCIDevSetInterruptPin (&pThis->dev, 0x01);

    pThis->dev.config[0x70] = 0x01; /* Capability ID: PCI Power Management Interface */
    pThis->dev.config[0x71] = 0x00;
    pThis->dev.config[0x72] = 0x03;

    pThis->dev.config[0x80] = 0x05; /* Capability ID: Message Signaled Interrupts. Disabled. */
    pThis->dev.config[0x81] = 0x70; /* next. */

    pThis->dev.config[0x90] = 0x40; /* AHCI mode. */
    pThis->dev.config[0x92] = 0x3f;
    pThis->dev.config[0x94] = 0x80;
    pThis->dev.config[0x95] = 0x01;
    pThis->dev.config[0x97] = 0x78;

    /*
     * Register the PCI device, it's I/O regions.
     */
    rc = PDMDevHlpPCIRegister (pDevIns, &pThis->dev);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Solaris 10 U5 fails to map the AHCI register space when the sets (0..5) for the legacy
     * IDE registers are not available.
     * We set up "fake" entries in the PCI configuration register.
     * That means they are available but read and writes from/to them have no effect.
     * No guest should access them anyway because the controller is marked as AHCI in the Programming interface
     * and we don't have an option to change to IDE emulation (real hardware provides an option in the BIOS
     * to switch to it which also changes device Id and other things in the PCI configuration space).
     */
    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 0, 8, PCI_ADDRESS_SPACE_IO, ahciLegacyFakeIORangeMap);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("AHCI cannot register PCI I/O region"));

    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 1, 1, PCI_ADDRESS_SPACE_IO, ahciLegacyFakeIORangeMap);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("AHCI cannot register PCI I/O region"));

    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 2, 8, PCI_ADDRESS_SPACE_IO, ahciLegacyFakeIORangeMap);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("AHCI cannot register PCI I/O region"));

    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 3, 1, PCI_ADDRESS_SPACE_IO, ahciLegacyFakeIORangeMap);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("AHCI cannot register PCI I/O region"));

    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 4, 0x10, PCI_ADDRESS_SPACE_IO, ahciLegacyFakeIORangeMap);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("AHCI cannot register PCI I/O region for BMDMA"));

    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 5, 4352, PCI_ADDRESS_SPACE_MEM, ahciMMIOMap);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("AHCI cannot register PCI memory region for registers"));

    rc = PDMDevHlpCritSectInit(pDevIns, &pThis->lock, "AHCI");
    if (RT_FAILURE(rc))
    {
        Log(("%s: Failed to create critical section.\n", __FUNCTION__));
        return rc;
    }

    rc = RTSemEventCreate(&pThis->hEvtIdle);
    AssertRCReturn(rc, rc);

    /* Create the timer for command completion coalescing feature. */
    rc = PDMDevHlpTMTimerCreate(pDevIns, TMCLOCK_VIRTUAL_SYNC, ahciCccTimer, pThis,
                                TMTIMER_FLAGS_DEFAULT_CRIT_SECT, "AHCI CCC Timer", &pThis->pHbaCccTimerR3);
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("pfnTMTimerCreate -> %Rrc\n", rc));
        return rc;
    }
    pThis->pHbaCccTimerR0 = TMTimerR0Ptr(pThis->pHbaCccTimerR3);
    pThis->pHbaCccTimerRC = TMTimerRCPtr(pThis->pHbaCccTimerR3);

    /* Status LUN. */
    pThis->IBase.pfnQueryInterface = ahciStatus_QueryInterface;
    pThis->ILeds.pfnQueryStatusLed = ahciStatus_QueryStatusLed;

    /*
     * Create the transmit queue.
     */
    rc = PDMDevHlpPDMQueueCreate(pDevIns, sizeof(DEVPORTNOTIFIERQUEUEITEM), 30*32 /*Maximum of 30 ports multiplied with 32 tasks each port*/, 0,
                                 ahciNotifyQueueConsumer, true, "AHCI-Xmit", &pThis->pNotifierQueueR3);
    if (RT_FAILURE(rc))
        return rc;
    pThis->pNotifierQueueR0 = PDMQueueR0Ptr(pThis->pNotifierQueueR3);
    pThis->pNotifierQueueRC = PDMQueueRCPtr(pThis->pNotifierQueueR3);

    /* Initialize static members on every port. */
    for (i = 0; i < AHCI_MAX_NR_PORTS_IMPL; i++)
    {
        /*
         * Init members of the port.
         */
        PAHCIPort pAhciPort      = &pThis->ahciPort[i];
        pAhciPort->pDevInsR3     = pDevIns;
        pAhciPort->pDevInsR0     = PDMDEVINS_2_R0PTR(pDevIns);
        pAhciPort->pDevInsRC     = PDMDEVINS_2_RCPTR(pDevIns);
        pAhciPort->iLUN          = i;
        pAhciPort->pAhciR3       = pThis;
        pAhciPort->pAhciR0       = PDMINS_2_DATA_R0PTR(pDevIns);
        pAhciPort->pAhciRC       = PDMINS_2_DATA_RCPTR(pDevIns);
        pAhciPort->Led.u32Magic  = PDMLED_MAGIC;
        pAhciPort->pDrvBase      = NULL;

        /* Register statistics counter. */
        PDMDevHlpSTAMRegisterF(pDevIns, &pAhciPort->StatDMA, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,
                               "Number of DMA transfers.", "/Devices/SATA%d/Port%d/DMA", iInstance, i);
        PDMDevHlpSTAMRegisterF(pDevIns, &pAhciPort->StatBytesRead, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,
                               "Amount of data read.", "/Devices/SATA%d/Port%d/ReadBytes", iInstance, i);
        PDMDevHlpSTAMRegisterF(pDevIns, &pAhciPort->StatBytesWritten, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,
                               "Amount of data written.", "/Devices/SATA%d/Port%d/WrittenBytes", iInstance, i);
        PDMDevHlpSTAMRegisterF(pDevIns, &pAhciPort->StatIORequestsPerSecond, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,
                               "Number of processed I/O requests per second.", "/Devices/SATA%d/Port%d/IORequestsPerSecond", iInstance, i);
#ifdef VBOX_WITH_STATISTICS
        PDMDevHlpSTAMRegisterF(pDevIns, &pAhciPort->StatProfileProcessTime, STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_NS_PER_CALL,
                               "Amount of time to process one request.", "/Devices/SATA%d/Port%d/ProfileProcessTime", iInstance, i);
        PDMDevHlpSTAMRegisterF(pDevIns, &pAhciPort->StatProfileMapIntoR3, STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_NS_PER_CALL,
                               "Amount of time to map the guest buffers into R3.", "/Devices/SATA%d/Port%d/ProfileMapIntoR3", iInstance, i);
        PDMDevHlpSTAMRegisterF(pDevIns, &pAhciPort->StatProfileReadWrite, STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_NS_PER_CALL,
                               "Amount of time for the read/write operation to complete.", "/Devices/SATA%d/Port%d/ProfileReadWrite", iInstance, i);
        PDMDevHlpSTAMRegisterF(pDevIns, &pAhciPort->StatProfileDestroyScatterGatherList, STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_NS_PER_CALL,
                               "Amount of time to destroy the scatter gather list and free associated ressources.", "/Devices/SATA%d/Port%d/ProfileDestroyScatterGatherList", iInstance, i);
#endif

        ahciPortHwReset(pAhciPort);
    }

    /* Attach drivers to every available port. */
    for (i = 0; i < pThis->cPortsImpl; i++)
    {
        char szName[24];
        RTStrPrintf(szName, sizeof(szName), "Port%u", i);

        PAHCIPort pAhciPort      = &pThis->ahciPort[i];
        /*
         * Init interfaces.
         */
        pAhciPort->IBase.pfnQueryInterface           = ahciPortQueryInterface;
        pAhciPort->IPortAsync.pfnTransferCompleteNotify  = ahciTransferCompleteNotify;
        pAhciPort->IMountNotify.pfnMountNotify       = ahciMountNotify;
        pAhciPort->IMountNotify.pfnUnmountNotify     = ahciUnmountNotify;
        pAhciPort->fAsyncIOThreadIdle                = true;

        /*
         * Attach the block driver
         */
        rc = PDMDevHlpDriverAttach(pDevIns, pAhciPort->iLUN, &pAhciPort->IBase, &pAhciPort->pDrvBase, szName);
        if (RT_SUCCESS(rc))
        {
            rc = ahciConfigureLUN(pDevIns, pAhciPort);
            if (RT_FAILURE(rc))
            {
                Log(("%s: Failed to configure the %s.\n", __FUNCTION__, szName));
                return rc;
            }

            /* Mark that a device is present on that port */
            if (i < 6)
                pThis->dev.config[0x93] |= (1 << i);

            /*
             * Init vendor product data.
             */
            /* Generate a default serial number. */
            char szSerial[AHCI_SERIAL_NUMBER_LENGTH+1];
            RTUUID Uuid;
            if (pAhciPort->pDrvBlock)
                rc = pAhciPort->pDrvBlock->pfnGetUuid(pAhciPort->pDrvBlock, &Uuid);
            else
                RTUuidClear(&Uuid);

            if (RT_FAILURE(rc) || RTUuidIsNull(&Uuid))
            {
                /* Generate a predictable serial for drives which don't have a UUID. */
                RTStrPrintf(szSerial, sizeof(szSerial), "VB%x-1a2b3c4d",
                            pAhciPort->iLUN);
            }
            else
                RTStrPrintf(szSerial, sizeof(szSerial), "VB%08x-%08x", Uuid.au32[0], Uuid.au32[3]);

            /* Get user config if present using defaults otherwise. */
            PCFGMNODE pCfgNode = CFGMR3GetChild(pCfgHandle, szName);
            rc = CFGMR3QueryStringDef(pCfgNode, "SerialNumber", pAhciPort->szSerialNumber, sizeof(pAhciPort->szSerialNumber),
                                      szSerial);
            if (RT_FAILURE(rc))
            {
                if (rc == VERR_CFGM_NOT_ENOUGH_SPACE)
                    return PDMDEV_SET_ERROR(pDevIns, VERR_INVALID_PARAMETER,
                                N_("AHCI configuration error: \"SerialNumber\" is longer than 20 bytes"));
                return PDMDEV_SET_ERROR(pDevIns, rc,
                            N_("AHCI configuration error: failed to read \"SerialNumber\" as string"));
            }

            rc = CFGMR3QueryStringDef(pCfgNode, "FirmwareRevision", pAhciPort->szFirmwareRevision, sizeof(pAhciPort->szFirmwareRevision),
                                      "1.0");
            if (RT_FAILURE(rc))
            {
                if (rc == VERR_CFGM_NOT_ENOUGH_SPACE)
                    return PDMDEV_SET_ERROR(pDevIns, VERR_INVALID_PARAMETER,
                                N_("AHCI configuration error: \"FirmwareRevision\" is longer than 8 bytes"));
                return PDMDEV_SET_ERROR(pDevIns, rc,
                            N_("AHCI configuration error: failed to read \"FirmwareRevision\" as string"));
            }

            rc = CFGMR3QueryStringDef(pCfgNode, "ModelNumber", pAhciPort->szModelNumber, sizeof(pAhciPort->szModelNumber),
                                      pAhciPort->fATAPI ? "VBOX CD-ROM" : "VBOX HARDDISK");
            if (RT_FAILURE(rc))
            {
                if (rc == VERR_CFGM_NOT_ENOUGH_SPACE)
                    return PDMDEV_SET_ERROR(pDevIns, VERR_INVALID_PARAMETER,
                               N_("AHCI configuration error: \"ModelNumber\" is longer than 40 bytes"));
                return PDMDEV_SET_ERROR(pDevIns, rc,
                            N_("AHCI configuration error: failed to read \"ModelNumber\" as string"));
            }

            /* There are three other identification strings for CD drives used for INQUIRY */
            if (pAhciPort->fATAPI)
            {
                rc = CFGMR3QueryStringDef(pCfgNode, "ATAPIVendorId", pAhciPort->szInquiryVendorId, sizeof(pAhciPort->szInquiryVendorId),
                                          "VBOX");
                if (RT_FAILURE(rc))
                {
                        if (rc == VERR_CFGM_NOT_ENOUGH_SPACE)
                        return PDMDEV_SET_ERROR(pDevIns, VERR_INVALID_PARAMETER,
                                        N_("PIIX3 configuration error: \"ATAPIVendorId\" is longer than 16 bytes"));
                        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("PIIX3 configuration error: failed to read \"ATAPIVendorId\" as string"));
                }

                rc = CFGMR3QueryStringDef(pCfgNode, "ATAPIProductId", pAhciPort->szInquiryProductId, sizeof(pAhciPort->szInquiryProductId),
                                          "CD-ROM");
                if (RT_FAILURE(rc))
                {
                        if (rc == VERR_CFGM_NOT_ENOUGH_SPACE)
                        return PDMDEV_SET_ERROR(pDevIns, VERR_INVALID_PARAMETER,
                                        N_("PIIX3 configuration error: \"ATAPIProductId\" is longer than 16 bytes"));
                        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("PIIX3 configuration error: failed to read \"ATAPIProductId\" as string"));
                }

                rc = CFGMR3QueryStringDef(pCfgNode, "ATAPIRevision", pAhciPort->szInquiryRevision, sizeof(pAhciPort->szInquiryRevision),
                                          "1.0");
                if (RT_FAILURE(rc))
                {
                        if (rc == VERR_CFGM_NOT_ENOUGH_SPACE)
                        return PDMDEV_SET_ERROR(pDevIns, VERR_INVALID_PARAMETER,
                                        N_("PIIX3 configuration error: \"ATAPIRevision\" is longer than 4 bytes"));
                        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("PIIX3 configuration error: failed to read \"ATAPIRevision\" as string"));
                }
            }

            /*
             * If the new async interface is available we use a PDMQueue to transmit
             * the requests into R3.
             * Otherwise we use a event semaphore and a async I/O thread which processes them.
             */
            if (pAhciPort->pDrvBlockAsync && pThis->fUseAsyncInterfaceIfAvailable)
            {
                LogRel(("AHCI: LUN#%d: using async I/O\n", pAhciPort->iLUN));
                pAhciPort->fAsyncInterface = true;
            }
            else
            {
                LogRel(("AHCI: LUN#%d: using normal I/O\n", pAhciPort->iLUN));
                pAhciPort->fAsyncInterface = false;

                rc = RTSemEventCreate(&pAhciPort->AsyncIORequestSem);
                AssertMsgRC(rc, ("Failed to create event semaphore for %s rc=%Rrc.\n", szName, rc));


                rc = PDMDevHlpPDMThreadCreate(pDevIns, &pAhciPort->pAsyncIOThread, pAhciPort, ahciAsyncIOLoop, ahciAsyncIOLoopWakeUp, 0,
                                              RTTHREADTYPE_IO, szName);
                AssertMsgRC(rc, ("%s: Async IO Thread creation for %s failed rc=%Rrc\n", szName, rc));
            }
        }
        else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
        {
            pAhciPort->pDrvBase = NULL;
            rc = VINF_SUCCESS;
            LogRel(("%s: no driver attached\n", szName));
        }
        else
            return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                       N_("AHCI: Failed to attach drive to %s"), szName);

#ifdef DEBUG
        for (uint32_t i = 0; i < AHCI_NR_COMMAND_SLOTS; i++)
            pAhciPort->ahciIOTasks[i] = 0xff;
#endif
    }

    /*
     * Attach status driver (optional).
     */
    rc = PDMDevHlpDriverAttach(pDevIns, PDM_STATUS_LUN, &pThis->IBase, &pBase, "Status Port");
    if (RT_SUCCESS(rc))
        pThis->pLedsConnector = (PDMILEDCONNECTORS *)pBase->pfnQueryInterface(pBase, PDMINTERFACE_LED_CONNECTORS);
    else if (rc != VERR_PDM_NO_ATTACHED_DRIVER)
    {
        AssertMsgFailed(("Failed to attach to status driver. rc=%Rrc\n", rc));
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("AHCI cannot attach to status driver"));
    }

    /*
     * Setup IDE emulation.
     * We only emulate the I/O ports but not bus master DMA.
     * If the configuration values are not found the setup of the ports is as follows:
     *     Primary Master:   Port 0
     *     Primary Slave:    Port 1
     *     Secondary Master: Port 2
     *     Secondary Slave:  Port 3
     */

    /*
     * Setup I/O ports for the PCI device.
     */
    pThis->aCts[0].irq          = 12;
    pThis->aCts[0].IOPortBase1  = 0x1e8;
    pThis->aCts[0].IOPortBase2  = 0x3e6;
    pThis->aCts[1].irq          = 11;
    pThis->aCts[1].IOPortBase1  = 0x168;
    pThis->aCts[1].IOPortBase2  = 0x366;

    for (uint32_t i = 0; i < RT_ELEMENTS(pThis->aCts); i++)
    {
        PAHCIATACONTROLLER pCtl = &pThis->aCts[i];
        uint32_t iPortMaster, iPortSlave;
        uint32_t cbSSMState = 0;
        static const char *s_apszDescs[RT_ELEMENTS(pThis->aCts)][RT_ELEMENTS(pCtl->aIfs)] =
        {
            { "PrimaryMaster", "PrimarySlave" },
            { "SecondaryMaster", "SecondarySlave" }
        };

        rc = CFGMR3QueryU32Def(pCfgHandle, s_apszDescs[i][0], &iPortMaster, 2 * i);
        if (RT_FAILURE(rc))
            return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                       N_("AHCI configuration error: failed to read %s as U32"), s_apszDescs[i][0]);

        rc = CFGMR3QueryU32Def(pCfgHandle, s_apszDescs[i][1], &iPortSlave, 2 * i + 1);
        if (RT_FAILURE(rc))
            return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                       N_("AHCI configuration error: failed to read %s as U32"), s_apszDescs[i][1]);

        char szName[24];
        RTStrPrintf(szName, sizeof(szName), "EmulatedATA%d", i);
        rc = ataControllerInit(pDevIns, pCtl, pThis->ahciPort[iPortMaster].pDrvBase, pThis->ahciPort[iPortSlave].pDrvBase,
                               &cbSSMState, szName, &pThis->ahciPort[iPortMaster].Led, &pThis->ahciPort[iPortMaster].StatBytesRead,
                               &pThis->ahciPort[iPortMaster].StatBytesWritten);
        if (RT_FAILURE(rc))
            return rc;

        cbTotalBufferSize += cbSSMState;

        rc = PDMDevHlpIOPortRegister(pDevIns, pCtl->IOPortBase1, 8, (RTHCPTR)i,
                                     ahciIOPortWrite1, ahciIOPortRead1, ahciIOPortWriteStr1, ahciIOPortReadStr1, "AHCI");
        if (RT_FAILURE(rc))
            return rc;

        if (pThis->fR0Enabled)
        {
            rc = PDMDevHlpIOPortRegisterR0(pDevIns, pCtl->IOPortBase1, 8, (RTR0PTR)i,
                                           "ahciIOPortWrite1", "ahciIOPortRead1", NULL, NULL, "AHCI R0");
            if (RT_FAILURE(rc))
                return rc;
        }

        if (pThis->fGCEnabled)
        {
            rc = PDMDevHlpIOPortRegisterGC(pDevIns, pCtl->IOPortBase1, 8, (RTGCPTR)i,
                                            "ahciIOPortWrite1", "ahciIOPortRead1", NULL, NULL, "AHCI GC");
            if (RT_FAILURE(rc))
                return rc;
        }

        rc = PDMDevHlpIOPortRegister(pDevIns, pCtl->IOPortBase2, 1, (RTHCPTR)i,
                                     ahciIOPortWrite2, ahciIOPortRead2, NULL, NULL, "AHCI");
        if (RT_FAILURE(rc))
            return rc;

        if (pThis->fR0Enabled)
        {
            rc = PDMDevHlpIOPortRegisterR0(pDevIns, pCtl->IOPortBase2, 1, (RTR0PTR)i,
                                            "ahciIOPortWrite2", "ahciIOPortRead2", NULL, NULL, "AHCI R0");
            if (RT_FAILURE(rc))
                return rc;
        }

        if (pThis->fGCEnabled)
        {
            rc = PDMDevHlpIOPortRegisterGC(pDevIns, pCtl->IOPortBase2, 1, (RTGCPTR)i,
                                            "ahciIOPortWrite2", "ahciIOPortRead2", NULL, NULL, "AHCI GC");
            if (RT_FAILURE(rc))
                return rc;
        }
    }

    rc = PDMDevHlpSSMRegisterEx(pDevIns, AHCI_SAVED_STATE_VERSION, sizeof(*pThis)+cbTotalBufferSize, NULL,
                                NULL,         ahciLiveExec, NULL,
                                ahciSavePrep, ahciSaveExec, NULL,
                                ahciLoadPrep, ahciLoadExec, NULL);
    if (RT_FAILURE(rc))
        return rc;

    ahciReset(pDevIns);

    return rc;
}

/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceAHCI =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szDeviceName */
    "ahci",
    /* szRCMod */
    "VBoxDDGC.gc",
    /* szR0Mod */
    "VBoxDDR0.r0",
    /* pszDescription */
    "Intel AHCI controller.\n",
    /* fFlags */
    PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_RC | PDM_DEVREG_FLAGS_R0 |
    PDM_DEVREG_FLAGS_FIRST_SUSPEND_NOTIFICATION | PDM_DEVREG_FLAGS_FIRST_POWEROFF_NOTIFICATION,
    /* fClass */
    PDM_DEVREG_CLASS_STORAGE,
    /* cMaxInstances */
    ~0,
    /* cbInstance */
    sizeof(AHCI),
    /* pfnConstruct */
    ahciConstruct,
    /* pfnDestruct */
    ahciDestruct,
    /* pfnRelocate */
    ahciRelocate,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    ahciReset,
    /* pfnSuspend */
    ahciSuspend,
    /* pfnResume */
    ahciResume,
    /* pfnAttach */
    ahciAttach,
    /* pfnDetach */
    ahciDetach,
    /* pfnQueryInterface. */
    NULL,
    /* pfnInitComplete */
    NULL,
    /* pfnPowerOff */
    ahciPowerOff,
    /* pfnSoftReset */
    NULL,
    /* u32VersionEnd */
    PDM_DEVREG_VERSION
};

#endif /* IN_RING3 */
#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */
