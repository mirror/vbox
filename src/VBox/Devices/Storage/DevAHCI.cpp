/* $Id$ */
/** @file
 * DevAHCI - AHCI controller device (disk and cdrom).
 *
 * Implements the AHCI standard 1.1
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
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
 * The data is transferred in an asynchronous way using one thread per implemented
 * port or using the new async completion interface which is still under
 * development. [not quite up to date]
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_AHCI
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pdmstorageifs.h>
#include <VBox/vmm/pdmqueue.h>
#include <VBox/vmm/pdmthread.h>
#include <VBox/vmm/pdmcritsect.h>
#include <VBox/sup.h>
#include <VBox/scsi.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#include <iprt/list.h>
#ifdef IN_RING3
# include <iprt/param.h>
# include <iprt/thread.h>
# include <iprt/semaphore.h>
# include <iprt/alloc.h>
# include <iprt/uuid.h>
# include <iprt/time.h>
#endif
#include "PIIX3ATABmDma.h"
#include "ide.h"
#include "ATAPIPassthrough.h"
#include "VBoxDD.h"

#if   defined(VBOX_WITH_DTRACE) \
   && defined(IN_RING3) \
   && !defined(VBOX_DEVICE_STRUCT_TESTCASE)
# include "dtrace/VBoxDD.h"
#else
# define VBOXDD_AHCI_REQ_SUBMIT(a,b,c,d)           do { } while (0)
# define VBOXDD_AHCI_REQ_SUBMIT_TIMESTAMP(a,b)     do { } while (0)
# define VBOXDD_AHCI_REQ_COMPLETED(a,b,c,d,e)      do { } while (0)
# define VBOXDD_AHCI_REQ_COMPLETED_TIMESTAMP(a,b)  do { } while (0)
#endif

/** Maximum number of ports available.
 * Spec defines 32 but we have one allocated for command completion coalescing
 * and another for a reserved future feature.
 */
#define AHCI_MAX_NR_PORTS_IMPL  30
/** Maximum number of command slots available. */
#define AHCI_NR_COMMAND_SLOTS   32

#define AHCI_MAX_ALLOC_TOO_MUCH 20

/** The current saved state version. */
#define AHCI_SAVED_STATE_VERSION                        8
/** The saved state version before changing the port reset logic in an incompatible way. */
#define AHCI_SAVED_STATE_VERSION_PRE_PORT_RESET_CHANGES 7
/** Saved state version before the per port hotplug port was added. */
#define AHCI_SAVED_STATE_VERSION_PRE_HOTPLUG_FLAG       6
/** Saved state version before legacy ATA emulation was dropped. */
#define AHCI_SAVED_STATE_VERSION_IDE_EMULATION          5
/** Saved state version before ATAPI support was added. */
#define AHCI_SAVED_STATE_VERSION_PRE_ATAPI              3
/** The saved state version use in VirtualBox 3.0 and earlier.
 * This was before the config was added and ahciIOTasks was dropped. */
#define AHCI_SAVED_STATE_VERSION_VBOX_30                2
/* for Older ATA state Read handling */
#define ATA_CTL_SAVED_STATE_VERSION 3
#define ATA_CTL_SAVED_STATE_VERSION_WITHOUT_FULL_SENSE 1
#define ATA_CTL_SAVED_STATE_VERSION_WITHOUT_EVENT_STATUS 2

/** The maximum number of release log entries per device. */
#define MAX_LOG_REL_ERRORS 1024

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

/* MediaEventStatus */
#define ATA_EVENT_STATUS_UNCHANGED              0    /**< medium event status not changed */
#define ATA_EVENT_STATUS_MEDIA_NEW              1    /**< new medium inserted */
#define ATA_EVENT_STATUS_MEDIA_REMOVED          2    /**< medium removed */
#define ATA_EVENT_STATUS_MEDIA_CHANGED          3    /**< medium was removed + new medium was inserted */
#define ATA_EVENT_STATUS_MEDIA_EJECT_REQUESTED  4    /**< medium eject requested (eject button pressed) */

/* Media track type */
#define ATA_MEDIA_TYPE_UNKNOWN                  0    /**< unknown CD type */

/** ATAPI sense info size. */
#define ATAPI_SENSE_SIZE 64

/**
 * Command Header.
 */
#pragma pack(1)
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
#pragma pack()
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
#define AHCI_CMDFIS_D                     RT_BIT(5)

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

/** Pointer to a task state. */
typedef struct AHCIREQ *PAHCIREQ;

/**
 * Data processing callback
 *
 * @returns VBox status code.
 * @param   pAhciReq    The task state.
 * @param   ppvProc     Where to store the pointer to the buffer holding the processed data on success.
 *                      Must be freed with RTMemFree().
 * @param   pcbProc     Where to store the size of the buffer on success.
 */
typedef DECLCALLBACK(int)   FNAHCIPOSTPROCESS(PAHCIREQ pAhciReq, void **ppvProc, size_t *pcbProc);
/** Pointer to a FNAHCIPOSTPROCESS() function. */
typedef FNAHCIPOSTPROCESS *PFNAHCIPOSTPROCESS;

/**
 * Transfer type.
 */
typedef enum AHCITXDIR
{
    /** Invalid */
    AHCITXDIR_INVALID = 0,
    /** None */
    AHCITXDIR_NONE,
    /** Read */
    AHCITXDIR_READ,
    /** Write */
    AHCITXDIR_WRITE,
    /** Flush */
    AHCITXDIR_FLUSH,
    /** Trim */
    AHCITXDIR_TRIM
} AHCITXDIR;

/**
 * Task state.
 */
typedef enum AHCITXSTATE
{
    /** Invalid. */
    AHCITXSTATE_INVALID = 0,
    /** Task is not active. */
    AHCITXSTATE_FREE,
    /** Task is active */
    AHCITXSTATE_ACTIVE,
    /** Task was canceled but the request didn't completed yet. */
    AHCITXSTATE_CANCELED,
    /** 32bit hack. */
    AHCITXSTATE_32BIT_HACK = 0x7fffffff
} AHCITXSTATE, *PAHCITXSTATE;
AssertCompileSize(AHCITXSTATE, sizeof(uint32_t));

/** Task encountered a buffer overflow. */
#define AHCI_REQ_OVERFLOW    RT_BIT_32(0)
/** Request is a PIO data command, if this flag is not set it either is
 * a command which does not transfer data or a DMA command based on the transfer size. */
#define AHCI_REQ_PIO_DATA    RT_BIT_32(1)
/** The request has the SACT register set. */
#define AHCI_REQ_CLEAR_SACT  RT_BIT_32(2)
/** Flag whether the request is queued. */
#define AHCI_REQ_IS_QUEUED   RT_BIT_32(3)
/** Flag whether the request is stored on the stack. */
#define AHCI_REQ_IS_ON_STACK RT_BIT_32(4)

/**
 * A task state.
 */
typedef struct AHCIREQ
{
    /** List node for the free list if the request is not in use. */
    RTLISTNODE                 NodeList;
    /** Task state. */
    volatile AHCITXSTATE       enmTxState;
    /** Start timestamp of the request. */
    uint64_t                   tsStart;
    /** Tag of the task. */
    uint32_t                   uTag;
    /** The command header for this task. */
    CmdHdr                     cmdHdr;
    /** The command Fis for this task. */
    uint8_t                    cmdFis[AHCI_CMDFIS_TYPE_H2D_SIZE];
    /** The ATAPI command data. */
    uint8_t                    aATAPICmd[ATAPI_PACKET_SIZE];
    /** Size of one sector for the ATAPI transfer. */
    size_t                     cbATAPISector;
    /** Physical address of the command header. - GC */
    RTGCPHYS                   GCPhysCmdHdrAddr;
    /** Physical address if the PRDT */
    RTGCPHYS                   GCPhysPrdtl;
    /** Number of entries in the PRDTL. */
    unsigned                   cPrdtlEntries;
    /** Data direction. */
    AHCITXDIR                  enmTxDir;
    /** Start offset. */
    uint64_t                   uOffset;
    /** Number of bytes to transfer. */
    uint32_t                   cbTransfer;
    /** ATA error register */
    uint8_t                    uATARegError;
    /** ATA status register */
    uint8_t                    uATARegStatus;
    /** Flags for this task. */
    uint32_t                   fFlags;
    /** Additional memory allocation for this task. */
    void                      *pvAlloc;
    /** Siize of the allocation. */
    size_t                     cbAlloc;
    /** Number of times we had too much memory allocated for the request. */
    unsigned                   cAllocTooMuch;
    /** Data dependent on the transfer direction. */
    union
    {
        /** Data for an I/O request. */
        struct
        {
            /** Data segment. */
            RTSGSEG            DataSeg;
            /** Post processing callback.
             * If this is set we will use a buffer for the data
             * and the callback returns a buffer with the final data. */
            PFNAHCIPOSTPROCESS pfnPostProcess;
        } Io;
        /** Data for a trim request. */
        struct
        {
            /** Pointer to the array of ranges to trim. */
            PRTRANGE           paRanges;
            /** Number of entries in the array. */
            unsigned           cRanges;
        } Trim;
    } u;
} AHCIREQ;

/**
 * Notifier queue item.
 */
typedef struct DEVPORTNOTIFIERQUEUEITEM
{
    /** The core part owned by the queue manager. */
    PDMQUEUEITEMCORE    Core;
    /** The port to process. */
    uint8_t             iPort;
} DEVPORTNOTIFIERQUEUEITEM, *PDEVPORTNOTIFIERQUEUEITEM;


/**
 * @implements PDMIBASE
 * @implements PDMIMEDIAPORT
 * @implements PDMIMEDIAASYNCPORT
 * @implements PDMIMOUNTNOTIFY
 */
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
    volatile uint32_t               regSACT;
    /** Command Issue. */
    uint32_t                        regCI;

    /** Current number of active tasks. */
    volatile uint32_t               cTasksActive;
    /** Command List Base Address */
    volatile RTGCPHYS               GCPhysAddrClb;
    /** FIS Base Address */
    volatile RTGCPHYS               GCPhysAddrFb;

    /** Device is powered on. */
    bool                            fPoweredOn;
    /** Device has spun up. */
    bool                            fSpunUp;
    /** First D2H FIS was send. */
    bool                            fFirstD2HFisSend;
    /** Mark the drive as having a non-rotational medium (i.e. as a SSD). */
    bool                            fNonRotational;
    /** Attached device is a CD/DVD drive. */
    bool                            fATAPI;
    /** Passthrough SCSI commands. */
    bool                            fATAPIPassthrough;
    /** Flag whether this port is in a reset state. */
    volatile bool                   fPortReset;
    /** If we use the new async interface. */
    bool                            fAsyncInterface;
    /** Flag if we are in a device reset. */
    bool                            fResetDevice;
    /** Flag whether this port is hot plug capable. */
    bool                            fHotpluggable;
    /** Flag whether the port is in redo task mode. */
    volatile bool                   fRedo;
    /** Flag whether the worker thread is sleeping. */
    volatile bool                   fWrkThreadSleeping;

    bool                            afAlignment[3];

    /** Number of total sectors. */
    uint64_t                        cTotalSectors;
    /** Size of one sector. */
    uint32_t                        cbSector;
    /** Currently configured number of sectors in a multi-sector transfer. */
    uint32_t                        cMultSectors;
    /** Currently active transfer mode (MDMA/UDMA) and speed. */
    uint8_t                         uATATransferMode;
    /** ATAPI sense data. */
    uint8_t                         abATAPISense[ATAPI_SENSE_SIZE];
    /** HACK: Countdown till we report a newly unmounted drive as mounted. */
    uint8_t                         cNotifiedMediaChange;
    /** Exponent of logical sectors in a physical sector, number of logical sectors is 2^exp. */
    uint8_t                         cLogSectorsPerPhysicalExp;
    /** The same for GET_EVENT_STATUS for mechanism */
    volatile uint32_t               MediaEventStatus;
    /** Media type if known. */
    volatile uint32_t               MediaTrackType;
    /** The LUN. */
    RTUINT                          iLUN;

    /** Bitmap for finished tasks (R3 -> Guest). */
    volatile uint32_t               u32TasksFinished;
    /** Bitmap for finished queued tasks (R3 -> Guest). */
    volatile uint32_t               u32QueuedTasksFinished;
    /** Bitmap for new queued tasks (Guest -> R3). */
    volatile uint32_t               u32TasksNew;
    /** Bitmap of tasks which must be redone because of a non fatal error. */
    volatile uint32_t               u32TasksRedo;

    /** Current command slot processed.
     * Accessed by the guest by reading the CMD register.
     * Holds the command slot of the command processed at the moment. */
    volatile uint32_t               u32CurrentCommandSlot;

#if HC_ARCH_BITS == 64
    uint32_t                        u32Alignment2;
#endif

    /** Device specific settings (R3 only stuff). */
    /** Pointer to the attached driver's base interface. */
    R3PTRTYPE(PPDMIBASE)            pDrvBase;
    /** Pointer to the attached driver's block interface. */
    R3PTRTYPE(PPDMIMEDIA)           pDrvMedia;
    /** Pointer to the attached driver's async block interface. */
    R3PTRTYPE(PPDMIMEDIAASYNC)      pDrvMediaAsync;
    /** Pointer to the attached driver's mount interface. */
    R3PTRTYPE(PPDMIMOUNT)           pDrvMount;
    /** The base interface. */
    PDMIBASE                        IBase;
    /** The block port interface. */
    PDMIMEDIAPORT                   IPort;
    /** The optional block async port interface. */
    PDMIMEDIAASYNCPORT              IPortAsync;
    /** The mount notify interface. */
    PDMIMOUNTNOTIFY                 IMountNotify;
    /** Physical geometry of this image. */
    PDMMEDIAGEOMETRY                PCHSGeometry;
    /** The status LED state for this drive. */
    PDMLED                          Led;

#if HC_ARCH_BITS == 64
    uint32_t                        u32Alignment3;
#endif

    /** Async IO Thread. */
    R3PTRTYPE(PPDMTHREAD)           pAsyncIOThread;
    /** Array of active tasks indexed by the tag. */
    R3PTRTYPE(volatile PAHCIREQ)    aActiveTasks[AHCI_NR_COMMAND_SLOTS];
    /** First task throwing an error. */
    R3PTRTYPE(volatile PAHCIREQ)    pTaskErr;
    /** The current tracklist of the loaded medium if passthrough is used. */
    R3PTRTYPE(PTRACKLIST)           pTrackList;

    /** The event semaphore the processing thread waits on. */
    SUPSEMEVENT                     hEvtProcess;

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
    /** Statistics: Amount of time to read/write data. */
    STAMPROFILE                     StatProfileReadWrite;
#endif /* VBOX_WITH_STATISTICS */

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
    /** Error counter */
    uint32_t                        cErrors;

    uint32_t                        u32Alignment5;

    /** Critical section protecting the global free list. */
    RTCRITSECT                      CritSectReqsFree;
    /** Head of the global free request list. */
    R3PTRTYPE(PRTLISTANCHOR)        pListReqsFree;

    /** Align to a multiple of 8. */
#if HC_ARCH_BITS == 32
    uint32_t                        u32Alignment6;
#endif

} AHCIPort;
/** Pointer to the state of an AHCI port. */
typedef AHCIPort *PAHCIPort;

AssertCompileMemberAlignment(AHCIPort, StatDMA, 8);
AssertCompileSizeAlignment(AHCIPort, 8);

/**
 * Main AHCI device state.
 *
 * @implements  PDMILEDPORTS
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

    /** Status LUN: The base interface. */
    PDMIBASE                        IBase;
    /** Status LUN: Leds interface. */
    PDMILEDPORTS                    ILeds;
    /** Status LUN: Partner of ILeds. */
    R3PTRTYPE(PPDMILEDCONNECTORS)   pLedsConnector;
    /** Status LUN: Media Notifys. */
    R3PTRTYPE(PPDMIMEDIANOTIFY)     pMediaNotify;

#if HC_ARCH_BITS == 32
    uint32_t                        Alignment1;
#endif

    /** Base address of the MMIO region. */
    RTGCPHYS                        MMIOBase;
    /** Base address of the I/O port region for Idx/Data. */
    RTIOPORT                        IOPortBase;

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

    /** Index register for BIOS access. */
    uint32_t                        regIdx;

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

    /** The critical section. */
    PDMCRITSECT                     lock;

    /** Bitmask of ports which asserted an interrupt. */
    volatile uint32_t               u32PortsInterrupted;
    /** Number of I/O threads currently active - used for async controller reset handling. */
    volatile uint32_t               cThreadsActive;
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
    /** Indicates that PDMDevHlpAsyncNotificationCompleted should be called when
     * a port is entering the idle state. */
    bool volatile                   fSignalIdle;
    /** Flag whether the controller has BIOS access enabled. */
    bool                            fBootable;
    /** Flag whether the legacy port reset method should be used to make it work with saved states. */
    bool                            fLegacyPortResetMethod;

    /** Number of usable ports on this controller. */
    uint32_t                        cPortsImpl;
    /** Number of usable command slots for each port. */
    uint32_t                        cCmdSlotsAvail;

    /** Flag whether we have written the first 4bytes in an 8byte MMIO write successfully. */
    volatile bool                   f8ByteMMIO4BytesWrittenSuccessfully;

#if HC_ARCH_BITS == 64
    uint32_t                        Alignment7;
#endif

    /** The support driver session handle. */
    R3R0PTRTYPE(PSUPDRVSESSION)     pSupDrvSession;
} AHCI;
/** Pointer to the state of an AHCI device. */
typedef AHCI *PAHCI;

AssertCompileMemberAlignment(AHCI, ahciPort, 8);

/**
 * Scatter gather list entry.
 */
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

#define AHCI_PORT_FB_RESERVED  0xffffff00 /* For masking out the reserved bits. */

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

/** Mask to get the LBA value from a LBA range. */
#define AHCI_RANGE_LBA_MASK    UINT64_C(0xffffffffffff)
/** Mas to get the length value from a LBA range. */
#define AHCI_RANGE_LENGTH_MASK UINT64_C(0xffff000000000000)
/** Returns the length of the range in sectors. */
#define AHCI_RANGE_LENGTH_GET(val) (((val) & AHCI_RANGE_LENGTH_MASK) >> 48)

/**
 * AHCI register operator.
 */
typedef struct ahci_opreg
{
    const char *pszName;
    int (*pfnRead )(PAHCI pAhci, uint32_t iReg, uint32_t *pu32Value);
    int (*pfnWrite)(PAHCI pAhci, uint32_t iReg, uint32_t u32Value);
} AHCIOPREG;

/**
 * AHCI port register operator.
 */
typedef struct pAhciPort_opreg
{
    const char *pszName;
    int (*pfnRead )(PAHCI pAhci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t *pu32Value);
    int (*pfnWrite)(PAHCI pAhci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t u32Value);
} AHCIPORTOPREG;

#ifndef VBOX_DEVICE_STRUCT_TESTCASE
RT_C_DECLS_BEGIN
static void ahciHBAReset(PAHCI pThis);
#ifdef IN_RING3
static int  ahciPostFisIntoMemory(PAHCIPort pAhciPort, unsigned uFisType, uint8_t *cmdFis);
static void ahciPostFirstD2HFisIntoMemory(PAHCIPort pAhciPort);
static size_t ahciCopyToPrdtl(PPDMDEVINS pDevIns, PAHCIREQ pAhciReq,
                              void *pvBuf, size_t cbBuf);
static size_t ahciCopyFromPrdtl(PPDMDEVINS pDevIns, PAHCIREQ pAhciReq,
                                void *pvBuf, size_t cbBuf);
static bool ahciCancelActiveTasks(PAHCIPort pAhciPort, PAHCIREQ pAhciReqExcept);
static void ahciReqMemFree(PAHCIPort pAhciPort, PAHCIREQ pAhciReq, bool fForceFree);
static void ahciR3PortCachedReqsFree(PAHCIPort pAhciPort);
#endif
RT_C_DECLS_END

#define PCIDEV_2_PAHCI(pPciDev)                  ( (PAHCI)(pPciDev) )
#define PDMIMOUNT_2_PAHCIPORT(pInterface)        ( (PAHCIPort)((uintptr_t)(pInterface) - RT_OFFSETOF(AHCIPort, IMount)) )
#define PDMIMOUNTNOTIFY_2_PAHCIPORT(pInterface)  ( (PAHCIPort)((uintptr_t)(pInterface) - RT_OFFSETOF(AHCIPort, IMountNotify)) )
#define PDMIBASE_2_PAHCIPORT(pInterface)         ( (PAHCIPort)((uintptr_t)(pInterface) - RT_OFFSETOF(AHCIPort, IBase)) )
#define PDMIMEDIAPORT_2_PAHCIPORT(pInterface)    ( (PAHCIPort)((uintptr_t)(pInterface) - RT_OFFSETOF(AHCIPort, IPort)) )
#define PDMIBASE_2_PAHCI(pInterface)             ( (PAHCI)((uintptr_t)(pInterface) - RT_OFFSETOF(AHCI, IBase)) )
#define PDMILEDPORTS_2_PAHCI(pInterface)         ( (PAHCI)((uintptr_t)(pInterface) - RT_OFFSETOF(AHCI, ILeds)) )

#define AHCI_RTGCPHYS_FROM_U32(Hi, Lo)             ( (RTGCPHYS)RT_MAKE_U64(Lo, Hi) )

#ifdef IN_RING3

# ifdef LOG_USE_C99
#  define ahciLog(a) \
     Log(("R3 P%u: %M", pAhciPort->iLUN, _LogRelRemoveParentheseis a))
# else
#  define ahciLog(a) \
     do { Log(("R3 P%u: ", pAhciPort->iLUN)); Log(a); } while(0)
# endif

#elif defined(IN_RING0)

# ifdef LOG_USE_C99
#  define ahciLog(a) \
     Log(("R0 P%u: %M", pAhciPort->iLUN, _LogRelRemoveParentheseis a))
# else
#  define ahciLog(a) \
     do { Log(("R0 P%u: ", pAhciPort->iLUN)); Log(a); } while(0)
# endif

#elif defined(IN_RC)

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
    PDMDevHlpPCISetIrq(pAhci->CTX_SUFF(pDevIns), 0, 0);
}

/**
 * Updates the IRQ level and sets port bit in the global interrupt status register of the HBA.
 */
static int ahciHbaSetInterrupt(PAHCI pAhci, uint8_t iPort, int rcBusy)
{
    Log(("P%u: %s: Setting interrupt\n", iPort, __FUNCTION__));

    int rc = PDMCritSectEnter(&pAhci->lock, rcBusy);
    if (rc != VINF_SUCCESS)
        return rc;

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
                    PDMDevHlpPCISetIrq(pAhci->CTX_SUFF(pDevIns), 0, 1);
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
                PDMDevHlpPCISetIrq(pAhci->CTX_SUFF(pDevIns), 0, 1);
            }
        }
    }

    PDMCritSectLeave(&pAhci->lock);
    return VINF_SUCCESS;
}

#ifdef IN_RING3
/*
 * Assert irq when an CCC timeout occurs
 */
static DECLCALLBACK(void) ahciCccTimer(PPDMDEVINS pDevIns, PTMTIMER pTimer, void *pvUser)
{
    PAHCI pAhci = (PAHCI)pvUser;

    int rc = ahciHbaSetInterrupt(pAhci, pAhci->uCccPortNr, VERR_IGNORED);
    AssertRC(rc);
}

/**
 * Finishes the port reset of the given port.
 *
 * @returns nothing.
 * @param   pAhciPort    The port to finish the reset on.
 */
static void ahciPortResetFinish(PAHCIPort pAhciPort)
{
    /* Cancel all tasks first. */
    bool fAllTasksCanceled = ahciCancelActiveTasks(pAhciPort, NULL);
    Assert(fAllTasksCanceled);

    /* Signature for SATA device. */
    if (pAhciPort->fATAPI)
        pAhciPort->regSIG = AHCI_PORT_SIG_ATAPI;
    else
        pAhciPort->regSIG = AHCI_PORT_SIG_DISK;

    /* We received a COMINIT from the device. Tell the guest. */
    ASMAtomicOrU32(&pAhciPort->regIS, AHCI_PORT_IS_PCS);
    pAhciPort->regSERR |= AHCI_PORT_SERR_X;
    pAhciPort->regTFD  |= ATA_STAT_BUSY;

    if ((pAhciPort->regCMD & AHCI_PORT_CMD_FRE) && (!pAhciPort->fFirstD2HFisSend))
    {
        ahciPostFirstD2HFisIntoMemory(pAhciPort);
        ASMAtomicOrU32(&pAhciPort->regIS, AHCI_PORT_IS_DHRS);

        if (pAhciPort->regIE & AHCI_PORT_IE_DHRE)
        {
            int rc = ahciHbaSetInterrupt(pAhciPort->CTX_SUFF(pAhci), pAhciPort->iLUN, VERR_IGNORED);
            AssertRC(rc);
        }
    }

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

    ASMAtomicXchgBool(&pAhciPort->fPortReset, false);
}
#endif

/**
 * Kicks the I/O thread from RC or R0.
 *
 * @returns nothing.
 * @param   pAhci     The AHCI controller instance.
 * @param   pAhciPort The port to kick.
 */
static void ahciIoThreadKick(PAHCI pAhci, PAHCIPort pAhciPort)
{
#ifdef IN_RC
    PDEVPORTNOTIFIERQUEUEITEM pItem = (PDEVPORTNOTIFIERQUEUEITEM)PDMQueueAlloc(pAhci->CTX_SUFF(pNotifierQueue));
    AssertMsg(VALID_PTR(pItem), ("Allocating item for queue failed\n"));

    if (pItem)
    {
        pItem->iPort = pAhciPort->iLUN;
        PDMQueueInsert(pAhci->CTX_SUFF(pNotifierQueue), (PPDMQUEUEITEMCORE)pItem);
    }
#else
    LogFlowFunc(("Signal event semaphore\n"));
    int rc = SUPSemEventSignal(pAhci->pSupDrvSession, pAhciPort->hEvtProcess);
    AssertRC(rc);
#endif
}

static int PortCmdIssue_w(PAHCI pAhci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t u32Value)
{
    RT_NOREF1(iReg);
    ahciLog(("%s: write u32Value=%#010x\n", __FUNCTION__, u32Value));

    /* Update the CI register first. */
    uint32_t uCIValue = ASMAtomicXchgU32(&pAhciPort->u32TasksFinished, 0);
    pAhciPort->regCI &= ~uCIValue;

    if (   (pAhciPort->regCMD & AHCI_PORT_CMD_CR)
        && u32Value > 0)
    {
        /*
         * Clear all tasks which are already marked as busy. The guest
         * shouldn't write already busy tasks actually.
         */
        u32Value &= ~pAhciPort->regCI;

        ASMAtomicOrU32(&pAhciPort->u32TasksNew, u32Value);

        /* Send a notification to R3 if u32TasksNew was 0 before our write. */
        if (ASMAtomicReadBool(&pAhciPort->fWrkThreadSleeping))
            ahciIoThreadKick(pAhci, pAhciPort);
    }

    pAhciPort->regCI |= u32Value;

    return VINF_SUCCESS;
}

static int PortCmdIssue_r(PAHCI pAhci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t *pu32Value)
{
    RT_NOREF1(iReg);

    uint32_t uCIValue = ASMAtomicXchgU32(&pAhciPort->u32TasksFinished, 0);
    ahciLog(("%s: read regCI=%#010x uCIValue=%#010x\n", __FUNCTION__, pAhciPort->regCI, uCIValue));

    pAhciPort->regCI &= ~uCIValue;
    *pu32Value = pAhciPort->regCI;

    return VINF_SUCCESS;
}

static int PortSActive_w(PAHCI pAhci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t u32Value)
{
    RT_NOREF1(iReg);
    ahciLog(("%s: write u32Value=%#010x\n", __FUNCTION__, u32Value));

    pAhciPort->regSACT |= u32Value;

    return VINF_SUCCESS;
}

static int PortSActive_r(PAHCI pAhci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t *pu32Value)
{
    RT_NOREF1(iReg);

    uint32_t u32TasksFinished = ASMAtomicXchgU32(&pAhciPort->u32QueuedTasksFinished, 0);
    pAhciPort->regSACT &= ~u32TasksFinished;

    ahciLog(("%s: read regSACT=%#010x regCI=%#010x u32TasksFinished=%#010x\n",
             __FUNCTION__, pAhciPort->regSACT, pAhciPort->regCI, u32TasksFinished));

    *pu32Value = pAhciPort->regSACT;

    return VINF_SUCCESS;
}

static int PortSError_w(PAHCI pAhci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t u32Value)
{
    ahciLog(("%s: write u32Value=%#010x\n", __FUNCTION__, u32Value));

    if (   (u32Value & AHCI_PORT_SERR_X)
        && (pAhciPort->regSERR & AHCI_PORT_SERR_X))
    {
        ASMAtomicAndU32(&pAhciPort->regIS, ~AHCI_PORT_IS_PCS);
        pAhciPort->regTFD |= ATA_STAT_ERR;
        pAhciPort->regTFD &= ~(ATA_STAT_DRQ | ATA_STAT_BUSY);
    }

    if (   (u32Value & AHCI_PORT_SERR_N)
        && (pAhciPort->regSERR & AHCI_PORT_SERR_N))
        ASMAtomicAndU32(&pAhciPort->regIS, ~AHCI_PORT_IS_PRCS);

    pAhciPort->regSERR &= ~u32Value;

    return VINF_SUCCESS;
}

static int PortSError_r(PAHCI pAhci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t *pu32Value)
{
    ahciLog(("%s: read regSERR=%#010x\n", __FUNCTION__, pAhciPort->regSERR));
    *pu32Value = pAhciPort->regSERR;
    return VINF_SUCCESS;
}

static int PortSControl_w(PAHCI pAhci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t u32Value)
{
    ahciLog(("%s: write u32Value=%#010x\n", __FUNCTION__, u32Value));
    ahciLog(("%s: IPM=%d SPD=%d DET=%d\n", __FUNCTION__,
             AHCI_PORT_SCTL_IPM_GET(u32Value), AHCI_PORT_SCTL_SPD_GET(u32Value), AHCI_PORT_SCTL_DET_GET(u32Value)));

#ifndef IN_RING3
    return VINF_IOM_R3_MMIO_WRITE;
#else
    if ((u32Value & AHCI_PORT_SCTL_DET) == AHCI_PORT_SCTL_DET_INIT)
    {
        if (!ASMAtomicXchgBool(&pAhciPort->fPortReset, true))
            LogRel(("AHCI#%u: Port %d reset\n", pAhci->CTX_SUFF(pDevIns)->iInstance,
                    pAhciPort->iLUN));

        pAhciPort->regSSTS = 0;
        pAhciPort->regSIG  = ~0;
        pAhciPort->regTFD  = 0x7f;
        pAhciPort->fFirstD2HFisSend = false;
        pAhciPort->regSCTL = u32Value;
    }
    else if (   (u32Value & AHCI_PORT_SCTL_DET) == AHCI_PORT_SCTL_DET_NINIT
             && (pAhciPort->regSCTL & AHCI_PORT_SCTL_DET) == AHCI_PORT_SCTL_DET_INIT
             && pAhciPort->pDrvBase)
    {
        /* Do the port reset here, so the guest sees the new status immediately. */
        if (pAhci->fLegacyPortResetMethod)
        {
            ahciPortResetFinish(pAhciPort);
            pAhciPort->regSCTL = u32Value; /* Update after finishing the reset, so the I/O thread doesn't get a chance to do the reset. */
        }
        else
        {
            pAhciPort->regSSTS = 0x1; /* Indicate device presence detected but communication not established. */
            pAhciPort->regSCTL = u32Value;  /* Update before kicking the I/O thread. */

            /* Kick the thread to finish the reset. */
            ahciIoThreadKick(pAhci, pAhciPort);
        }
    }
    else /* Just update the value if there is no device attached. */
        pAhciPort->regSCTL = u32Value;

    return VINF_SUCCESS;
#endif
}

static int PortSControl_r(PAHCI pAhci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t *pu32Value)
{
    ahciLog(("%s: read regSCTL=%#010x\n", __FUNCTION__, pAhciPort->regSCTL));
    ahciLog(("%s: IPM=%d SPD=%d DET=%d\n", __FUNCTION__,
             AHCI_PORT_SCTL_IPM_GET(pAhciPort->regSCTL), AHCI_PORT_SCTL_SPD_GET(pAhciPort->regSCTL),
             AHCI_PORT_SCTL_DET_GET(pAhciPort->regSCTL)));

    *pu32Value = pAhciPort->regSCTL;
    return VINF_SUCCESS;
}

static int PortSStatus_r(PAHCI pAhci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t *pu32Value)
{
    ahciLog(("%s: read regSSTS=%#010x\n", __FUNCTION__, pAhciPort->regSSTS));
    ahciLog(("%s: IPM=%d SPD=%d DET=%d\n", __FUNCTION__,
             AHCI_PORT_SSTS_IPM_GET(pAhciPort->regSSTS), AHCI_PORT_SSTS_SPD_GET(pAhciPort->regSSTS),
             AHCI_PORT_SSTS_DET_GET(pAhciPort->regSSTS)));

    *pu32Value = pAhciPort->regSSTS;
    return VINF_SUCCESS;
}

static int PortSignature_r(PAHCI pAhci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t *pu32Value)
{
    ahciLog(("%s: read regSIG=%#010x\n", __FUNCTION__, pAhciPort->regSIG));
    *pu32Value = pAhciPort->regSIG;
    return VINF_SUCCESS;
}

static int PortTaskFileData_r(PAHCI pAhci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t *pu32Value)
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
static int PortCmd_r(PAHCI pAhci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t *pu32Value)
{
    ahciLog(("%s: read regCMD=%#010x\n", __FUNCTION__, pAhciPort->regCMD | AHCI_PORT_CMD_CCS_SHIFT(pAhciPort->u32CurrentCommandSlot)));
    ahciLog(("%s: ICC=%d ASP=%d ALPE=%d DLAE=%d ATAPI=%d CPD=%d ISP=%d HPCP=%d PMA=%d CPS=%d CR=%d FR=%d ISS=%d CCS=%d FRE=%d CLO=%d POD=%d SUD=%d ST=%d\n",
             __FUNCTION__, (pAhciPort->regCMD & AHCI_PORT_CMD_ICC) >> 28, (pAhciPort->regCMD & AHCI_PORT_CMD_ASP) >> 27,
             (pAhciPort->regCMD & AHCI_PORT_CMD_ALPE) >> 26, (pAhciPort->regCMD & AHCI_PORT_CMD_DLAE) >> 25,
             (pAhciPort->regCMD & AHCI_PORT_CMD_ATAPI) >> 24, (pAhciPort->regCMD & AHCI_PORT_CMD_CPD) >> 20,
             (pAhciPort->regCMD & AHCI_PORT_CMD_ISP) >> 19, (pAhciPort->regCMD & AHCI_PORT_CMD_HPCP) >> 18,
             (pAhciPort->regCMD & AHCI_PORT_CMD_PMA) >> 17, (pAhciPort->regCMD & AHCI_PORT_CMD_CPS) >> 16,
             (pAhciPort->regCMD & AHCI_PORT_CMD_CR) >> 15, (pAhciPort->regCMD & AHCI_PORT_CMD_FR) >> 14,
             (pAhciPort->regCMD & AHCI_PORT_CMD_ISS) >> 13, pAhciPort->u32CurrentCommandSlot,
             (pAhciPort->regCMD & AHCI_PORT_CMD_FRE) >> 4, (pAhciPort->regCMD & AHCI_PORT_CMD_CLO) >> 3,
             (pAhciPort->regCMD & AHCI_PORT_CMD_POD) >> 2, (pAhciPort->regCMD & AHCI_PORT_CMD_SUD) >> 1,
             (pAhciPort->regCMD & AHCI_PORT_CMD_ST)));
    *pu32Value = pAhciPort->regCMD | AHCI_PORT_CMD_CCS_SHIFT(pAhciPort->u32CurrentCommandSlot);
    return VINF_SUCCESS;
}

/**
 * Write to the port command register.
 * This is the register where all the data transfer is started
 */
static int PortCmd_w(PAHCI pAhci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t u32Value)
{
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

    /* The PxCMD.CCS bits are R/O and maintained separately. */
    u32Value &= ~AHCI_PORT_CMD_CCS;

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
            /*
             * Set engine state to running if there is a device attached and
             * IS.PCS is clear.
             */
            if (   pAhciPort->pDrvBase
                && !(pAhciPort->regIS & AHCI_PORT_IS_PCS))
            {
                ahciLog(("%s: Engine starts\n", __FUNCTION__));
                u32Value |= AHCI_PORT_CMD_CR;

                /* If there is something in CI, kick the I/O thread. */
                if (   pAhciPort->regCI > 0
                    && ASMAtomicReadBool(&pAhciPort->fWrkThreadSleeping))
                {
                    ASMAtomicOrU32(&pAhciPort->u32TasksNew, pAhciPort->regCI);
#ifdef IN_RC
                    PDEVPORTNOTIFIERQUEUEITEM pItem = (PDEVPORTNOTIFIERQUEUEITEM)PDMQueueAlloc(pAhci->CTX_SUFF(pNotifierQueue));
                    AssertMsg(VALID_PTR(pItem), ("Allocating item for queue failed\n"));

                    pItem->iPort = pAhciPort->iLUN;
                    PDMQueueInsert(pAhci->CTX_SUFF(pNotifierQueue), (PPDMQUEUEITEMCORE)pItem);
#else
                    LogFlowFunc(("Signal event semaphore\n"));
                    int rc = SUPSemEventSignal(pAhci->pSupDrvSession, pAhciPort->hEvtProcess);
                    AssertRC(rc);
#endif
                }
            }
            else
                u32Value &= ~AHCI_PORT_CMD_CR;
        }
        else
        {
            ahciLog(("%s: Engine stops\n", __FUNCTION__));
            /* Clear command issue register. */
            pAhciPort->regCI = 0;
            pAhciPort->regSACT = 0;
            /* Clear current command slot. */
            pAhciPort->u32CurrentCommandSlot = 0;
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
            if (pAhciPort->fATAPI)
                pAhciPort->regSIG = AHCI_PORT_SIG_ATAPI;
            else
                pAhciPort->regSIG = AHCI_PORT_SIG_DISK;
            pAhciPort->regSSTS = (0x01 << 8) | /* Interface is active. */
                                 (0x02 << 4) | /* Generation 2 (3.0GBps) speed. */
                                 (0x03 << 0);  /* Device detected and communication established. */

            if (pAhciPort->regCMD & AHCI_PORT_CMD_FRE)
            {
#ifndef IN_RING3
                return VINF_IOM_R3_MMIO_WRITE;
#else
                ahciPostFirstD2HFisIntoMemory(pAhciPort);
                ASMAtomicOrU32(&pAhciPort->regIS, AHCI_PORT_IS_DHRS);

                if (pAhciPort->regIE & AHCI_PORT_IE_DHRE)
                {
                    int rc = ahciHbaSetInterrupt(pAhciPort->CTX_SUFF(pAhci), pAhciPort->iLUN, VERR_IGNORED);
                    AssertRC(rc);
                }
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
        if (   !pAhciPort->fFirstD2HFisSend
            && pAhciPort->pDrvBase)
        {
#ifndef IN_RING3
            return VINF_IOM_R3_MMIO_WRITE;
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

    return VINF_SUCCESS;
}

/**
 * Read from the port interrupt enable register.
 */
static int PortIntrEnable_r(PAHCI pAhci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t *pu32Value)
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
static int PortIntrEnable_w(PAHCI pAhci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t u32Value)
{
    int rc = VINF_SUCCESS;
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

    u32Value &= AHCI_PORT_IE_READONLY;

    /* Check if some a interrupt status bit changed*/
    uint32_t u32IntrStatus = ASMAtomicReadU32(&pAhciPort->regIS);

    if (u32Value & u32IntrStatus)
        rc = ahciHbaSetInterrupt(pAhci, pAhciPort->iLUN, VINF_IOM_R3_MMIO_WRITE);

    if (rc == VINF_SUCCESS)
        pAhciPort->regIE = u32Value;

    return rc;
}

/**
 * Read from the port interrupt status register.
 */
static int PortIntrSts_r(PAHCI pAhci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t *pu32Value)
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
static int PortIntrSts_w(PAHCI pAhci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t u32Value)
{
    ahciLog(("%s: write u32Value=%#010x\n", __FUNCTION__, u32Value));
    ASMAtomicAndU32(&pAhciPort->regIS, ~(u32Value & AHCI_PORT_IS_READONLY));

    return VINF_SUCCESS;
}

/**
 * Read from the port FIS base address upper 32bit register.
 */
static int PortFisAddrUp_r(PAHCI pAhci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t *pu32Value)
{
    ahciLog(("%s: read regFBU=%#010x\n", __FUNCTION__, pAhciPort->regFBU));
    *pu32Value = pAhciPort->regFBU;
    return VINF_SUCCESS;
}

/**
 * Write to the port FIS base address upper 32bit register.
 */
static int PortFisAddrUp_w(PAHCI pAhci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t u32Value)
{
    ahciLog(("%s: write u32Value=%#010x\n", __FUNCTION__, u32Value));

    pAhciPort->regFBU = u32Value;
    pAhciPort->GCPhysAddrFb = AHCI_RTGCPHYS_FROM_U32(pAhciPort->regFBU, pAhciPort->regFB);

    return VINF_SUCCESS;
}

/**
 * Read from the port FIS base address register.
 */
static int PortFisAddr_r(PAHCI pAhci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t *pu32Value)
{
    ahciLog(("%s: read regFB=%#010x\n", __FUNCTION__, pAhciPort->regFB));
    *pu32Value = pAhciPort->regFB;
    return VINF_SUCCESS;
}

/**
 * Write to the port FIS base address register.
 */
static int PortFisAddr_w(PAHCI pAhci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t u32Value)
{
    ahciLog(("%s: write u32Value=%#010x\n", __FUNCTION__, u32Value));

    Assert(!(u32Value & ~AHCI_PORT_FB_RESERVED));

    pAhciPort->regFB = (u32Value & AHCI_PORT_FB_RESERVED);
    pAhciPort->GCPhysAddrFb = AHCI_RTGCPHYS_FROM_U32(pAhciPort->regFBU, pAhciPort->regFB);

    return VINF_SUCCESS;
}

/**
 * Write to the port command list base address upper 32bit register.
 */
static int PortCmdLstAddrUp_w(PAHCI pAhci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t u32Value)
{
    ahciLog(("%s: write u32Value=%#010x\n", __FUNCTION__, u32Value));

    pAhciPort->regCLBU = u32Value;
    pAhciPort->GCPhysAddrClb = AHCI_RTGCPHYS_FROM_U32(pAhciPort->regCLBU, pAhciPort->regCLB);

    return VINF_SUCCESS;
}

/**
 * Read from the port command list base address upper 32bit register.
 */
static int PortCmdLstAddrUp_r(PAHCI pAhci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t *pu32Value)
{
    ahciLog(("%s: read regCLBU=%#010x\n", __FUNCTION__, pAhciPort->regCLBU));
    *pu32Value = pAhciPort->regCLBU;
    return VINF_SUCCESS;
}

/**
 * Read from the port command list base address register.
 */
static int PortCmdLstAddr_r(PAHCI pAhci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t *pu32Value)
{
    ahciLog(("%s: read regCLB=%#010x\n", __FUNCTION__, pAhciPort->regCLB));
    *pu32Value = pAhciPort->regCLB;
    return VINF_SUCCESS;
}

/**
 * Write to the port command list base address register.
 */
static int PortCmdLstAddr_w(PAHCI pAhci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t u32Value)
{
    ahciLog(("%s: write u32Value=%#010x\n", __FUNCTION__, u32Value));

    Assert(!(u32Value & ~AHCI_PORT_CLB_RESERVED));

    pAhciPort->regCLB = (u32Value & AHCI_PORT_CLB_RESERVED);
    pAhciPort->GCPhysAddrClb = AHCI_RTGCPHYS_FROM_U32(pAhciPort->regCLBU, pAhciPort->regCLB);

    return VINF_SUCCESS;
}

/**
 * Read from the global Version register.
 */
static int HbaVersion_r(PAHCI pAhci, uint32_t iReg, uint32_t *pu32Value)
{
    Log(("%s: read regHbaVs=%#010x\n", __FUNCTION__, pAhci->regHbaVs));
    *pu32Value = pAhci->regHbaVs;
    return VINF_SUCCESS;
}

/**
 * Read from the global Ports implemented register.
 */
static int HbaPortsImplemented_r(PAHCI pAhci, uint32_t iReg, uint32_t *pu32Value)
{
    Log(("%s: read regHbaPi=%#010x\n", __FUNCTION__, pAhci->regHbaPi));
    *pu32Value = pAhci->regHbaPi;
    return VINF_SUCCESS;
}

/**
 * Write to the global interrupt status register.
 */
static int HbaInterruptStatus_w(PAHCI pAhci, uint32_t iReg, uint32_t u32Value)
{
    int rc;
    Log(("%s: write u32Value=%#010x\n", __FUNCTION__, u32Value));

    rc = PDMCritSectEnter(&pAhci->lock, VINF_IOM_R3_MMIO_WRITE);
    if (rc != VINF_SUCCESS)
        return rc;

    pAhci->regHbaIs &= ~(u32Value);

    /*
     * Update interrupt status register and check for ports who
     * set the interrupt inbetween.
     */
    bool fClear = true;
    pAhci->regHbaIs |= ASMAtomicXchgU32(&pAhci->u32PortsInterrupted, 0);
    if (!pAhci->regHbaIs)
    {
        unsigned i = 0;

        /* Check if the cleared ports have a interrupt status bit set. */
        while ((u32Value > 0) && (i < AHCI_MAX_NR_PORTS_IMPL))
        {
            if (u32Value & 0x01)
            {
                PAHCIPort pAhciPort = &pAhci->ahciPort[i];

                if (pAhciPort->regIE & pAhciPort->regIS)
                {
                    Log(("%s: Interrupt status of port %u set -> Set interrupt again\n", __FUNCTION__, i));
                    ASMAtomicOrU32(&pAhci->u32PortsInterrupted, 1 << i);
                    fClear = false;
                    break;
                }
            }
            u32Value >>= 1;
            i++;
        }
    }
    else
        fClear = false;

    if (fClear)
        ahciHbaClearInterrupt(pAhci);
    else
    {
        Log(("%s: Not clearing interrupt: u32PortsInterrupted=%#010x\n", __FUNCTION__, pAhci->u32PortsInterrupted));
        /*
         * We need to set the interrupt again because the I/O APIC does not set it again even if the
         * line is still high.
         * We need to clear it first because the PCI bus only calls the interrupt controller if the state changes.
         */
        PDMDevHlpPCISetIrq(pAhci->CTX_SUFF(pDevIns), 0, 0);
        PDMDevHlpPCISetIrq(pAhci->CTX_SUFF(pDevIns), 0, 1);
    }

    PDMCritSectLeave(&pAhci->lock);
    return VINF_SUCCESS;
}

/**
 * Read from the global interrupt status register.
 */
static int HbaInterruptStatus_r(PAHCI pAhci, uint32_t iReg, uint32_t *pu32Value)
{
    uint32_t u32PortsInterrupted;
    int rc;

    rc = PDMCritSectEnter(&pAhci->lock, VINF_IOM_R3_MMIO_READ);
    if (rc != VINF_SUCCESS)
        return rc;

    u32PortsInterrupted = ASMAtomicXchgU32(&pAhci->u32PortsInterrupted, 0);

    PDMCritSectLeave(&pAhci->lock);
    Log(("%s: read regHbaIs=%#010x u32PortsInterrupted=%#010x\n", __FUNCTION__, pAhci->regHbaIs, u32PortsInterrupted));

    pAhci->regHbaIs |= u32PortsInterrupted;

#ifdef LOG_ENABLED
    Log(("%s:", __FUNCTION__));
    unsigned i;
    for (i = 0; i < pAhci->cPortsImpl; i++)
    {
        if ((pAhci->regHbaIs >> i) & 0x01)
            Log((" P%d", i));
    }
    Log(("\n"));
#endif

    *pu32Value = pAhci->regHbaIs;

    return VINF_SUCCESS;
}

/**
 * Write to the global control register.
 */
static int HbaControl_w(PAHCI pAhci, uint32_t iReg, uint32_t u32Value)
{
    Log(("%s: write u32Value=%#010x\n"
         "%s: AE=%d IE=%d HR=%d\n",
         __FUNCTION__, u32Value,
         __FUNCTION__, (u32Value & AHCI_HBA_CTRL_AE) >> 31, (u32Value & AHCI_HBA_CTRL_IE) >> 1,
         (u32Value & AHCI_HBA_CTRL_HR)));

#ifndef IN_RING3
    return VINF_IOM_R3_MMIO_WRITE;
#else
    /*
     * Increase the active thread counter because we might set the host controller
     * reset bit.
     */
    ASMAtomicIncU32(&pAhci->cThreadsActive);
    ASMAtomicWriteU32(&pAhci->regHbaCtrl, (u32Value & AHCI_HBA_CTRL_RW_MASK) | AHCI_HBA_CTRL_AE);

    /*
     * Do the HBA reset if requested and there is no other active thread at the moment,
     * the work is deferred to the last active thread otherwise.
     */
    uint32_t cThreadsActive = ASMAtomicDecU32(&pAhci->cThreadsActive);
    if (   (u32Value & AHCI_HBA_CTRL_HR)
        && !cThreadsActive)
        ahciHBAReset(pAhci);

    return VINF_SUCCESS;
#endif
}

/**
 * Read the global control register.
 */
static int HbaControl_r(PAHCI pAhci, uint32_t iReg, uint32_t *pu32Value)
{
    Log(("%s: read regHbaCtrl=%#010x\n"
         "%s: AE=%d IE=%d HR=%d\n",
         __FUNCTION__, pAhci->regHbaCtrl,
         __FUNCTION__, (pAhci->regHbaCtrl & AHCI_HBA_CTRL_AE) >> 31, (pAhci->regHbaCtrl & AHCI_HBA_CTRL_IE) >> 1,
         (pAhci->regHbaCtrl & AHCI_HBA_CTRL_HR)));
    *pu32Value = pAhci->regHbaCtrl;
    return VINF_SUCCESS;
}

/**
 * Read the global capabilities register.
 */
static int HbaCapabilities_r(PAHCI pAhci, uint32_t iReg, uint32_t *pu32Value)
{
    Log(("%s: read regHbaCap=%#010x\n"
         "%s: S64A=%d SNCQ=%d SIS=%d SSS=%d SALP=%d SAL=%d SCLO=%d ISS=%d SNZO=%d SAM=%d SPM=%d PMD=%d SSC=%d PSC=%d NCS=%d NP=%d\n",
          __FUNCTION__, pAhci->regHbaCap,
          __FUNCTION__, (pAhci->regHbaCap & AHCI_HBA_CAP_S64A) >> 31, (pAhci->regHbaCap & AHCI_HBA_CAP_SNCQ) >> 30,
          (pAhci->regHbaCap & AHCI_HBA_CAP_SIS) >> 28, (pAhci->regHbaCap & AHCI_HBA_CAP_SSS) >> 27,
          (pAhci->regHbaCap & AHCI_HBA_CAP_SALP) >> 26, (pAhci->regHbaCap & AHCI_HBA_CAP_SAL) >> 25,
          (pAhci->regHbaCap & AHCI_HBA_CAP_SCLO) >> 24, (pAhci->regHbaCap & AHCI_HBA_CAP_ISS) >> 20,
          (pAhci->regHbaCap & AHCI_HBA_CAP_SNZO) >> 19, (pAhci->regHbaCap & AHCI_HBA_CAP_SAM) >> 18,
          (pAhci->regHbaCap & AHCI_HBA_CAP_SPM) >> 17, (pAhci->regHbaCap & AHCI_HBA_CAP_PMD) >> 15,
          (pAhci->regHbaCap & AHCI_HBA_CAP_SSC) >> 14, (pAhci->regHbaCap & AHCI_HBA_CAP_PSC) >> 13,
          (pAhci->regHbaCap & AHCI_HBA_CAP_NCS) >> 8, (pAhci->regHbaCap & AHCI_HBA_CAP_NP)));
    *pu32Value = pAhci->regHbaCap;
    return VINF_SUCCESS;
}

/**
 * Write to the global command completion coalescing control register.
 */
static int HbaCccCtl_w(PAHCI pAhci, uint32_t iReg, uint32_t u32Value)
{
    Log(("%s: write u32Value=%#010x\n"
         "%s: TV=%d CC=%d INT=%d EN=%d\n",
         __FUNCTION__, u32Value,
         __FUNCTION__, AHCI_HBA_CCC_CTL_TV_GET(u32Value), AHCI_HBA_CCC_CTL_CC_GET(u32Value),
         AHCI_HBA_CCC_CTL_INT_GET(u32Value), (u32Value & AHCI_HBA_CCC_CTL_EN)));

    pAhci->regHbaCccCtl = u32Value;
    pAhci->uCccTimeout  = AHCI_HBA_CCC_CTL_TV_GET(u32Value);
    pAhci->uCccPortNr   = AHCI_HBA_CCC_CTL_INT_GET(u32Value);
    pAhci->uCccNr       = AHCI_HBA_CCC_CTL_CC_GET(u32Value);

    if (u32Value & AHCI_HBA_CCC_CTL_EN)
    {
        /* Arm the timer */
        TMTimerSetMillies(pAhci->CTX_SUFF(pHbaCccTimer), pAhci->uCccTimeout);
    }
    else
    {
        TMTimerStop(pAhci->CTX_SUFF(pHbaCccTimer));
    }

    return VINF_SUCCESS;
}

/**
 * Read the global command completion coalescing control register.
 */
static int HbaCccCtl_r(PAHCI pAhci, uint32_t iReg, uint32_t *pu32Value)
{
    Log(("%s: read regHbaCccCtl=%#010x\n"
         "%s: TV=%d CC=%d INT=%d EN=%d\n",
         __FUNCTION__, pAhci->regHbaCccCtl,
         __FUNCTION__, AHCI_HBA_CCC_CTL_TV_GET(pAhci->regHbaCccCtl), AHCI_HBA_CCC_CTL_CC_GET(pAhci->regHbaCccCtl),
         AHCI_HBA_CCC_CTL_INT_GET(pAhci->regHbaCccCtl), (pAhci->regHbaCccCtl & AHCI_HBA_CCC_CTL_EN)));
    *pu32Value = pAhci->regHbaCccCtl;
    return VINF_SUCCESS;
}

/**
 * Write to the global command completion coalescing ports register.
 */
static int HbaCccPorts_w(PAHCI pAhci, uint32_t iReg, uint32_t u32Value)
{
    Log(("%s: write u32Value=%#010x\n", __FUNCTION__, u32Value));

    pAhci->regHbaCccPorts = u32Value;

    return VINF_SUCCESS;
}

/**
 * Read the global command completion coalescing ports register.
 */
static int HbaCccPorts_r(PAHCI pAhci, uint32_t iReg, uint32_t *pu32Value)
{
    Log(("%s: read regHbaCccPorts=%#010x\n", __FUNCTION__, pAhci->regHbaCccPorts));

#ifdef LOG_ENABLED
    Log(("%s:", __FUNCTION__));
    unsigned i;
    for (i = 0; i < pAhci->cPortsImpl; i++)
    {
        if ((pAhci->regHbaCccPorts >> i) & 0x01)
            Log((" P%d", i));
    }
    Log(("\n"));
#endif

    *pu32Value = pAhci->regHbaCccPorts;
    return VINF_SUCCESS;
}

/**
 * Invalid write to global register
 */
static int HbaInvalid_w(PAHCI pAhci, uint32_t iReg, uint32_t u32Value)
{
    Log(("%s: Write denied!!! iReg=%u u32Value=%#010x\n", __FUNCTION__, iReg, u32Value));
    return VINF_SUCCESS;
}

/**
 * Invalid Port write.
 */
static int PortInvalid_w(PAHCI pAhci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t u32Value)
{
    ahciLog(("%s: Write denied!!! iReg=%u u32Value=%#010x\n", __FUNCTION__, iReg, u32Value));
    return VINF_SUCCESS;
}

/**
 * Invalid Port read.
 */
static int PortInvalid_r(PAHCI pAhci, PAHCIPort pAhciPort, uint32_t iReg, uint32_t *pu32Value)
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

#ifdef IN_RING3
/**
 * Reset initiated by system software for one port.
 *
 * @param pAhciPort     The port to reset.
 */
static void ahciPortSwReset(PAHCIPort pAhciPort)
{
    bool fAllTasksCanceled;

    /* Cancel all tasks first. */
    fAllTasksCanceled = ahciCancelActiveTasks(pAhciPort, NULL);
    Assert(fAllTasksCanceled);

    Assert(pAhciPort->cTasksActive == 0);

    pAhciPort->regIS   = 0;
    pAhciPort->regIE   = 0;
    pAhciPort->regCMD  = AHCI_PORT_CMD_CPD  | /* Cold presence detection */
                         AHCI_PORT_CMD_SUD  | /* Device has spun up. */
                         AHCI_PORT_CMD_POD;   /* Port is powered on. */

    /* Hotplugging supported?. */
    if (pAhciPort->fHotpluggable)
        pAhciPort->regCMD |= AHCI_PORT_CMD_HPCP;

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
    pAhciPort->cMultSectors = ATA_MAX_MULT_SECTORS;
    pAhciPort->uATATransferMode = ATA_MODE_UDMA | 6;

    pAhciPort->u32TasksNew = 0;
    pAhciPort->u32TasksRedo = 0;
    pAhciPort->u32TasksFinished = 0;
    pAhciPort->u32QueuedTasksFinished = 0;
    pAhciPort->u32CurrentCommandSlot = 0;

    ASMAtomicWriteU32(&pAhciPort->MediaEventStatus, ATA_EVENT_STATUS_UNCHANGED);
    ASMAtomicWriteU32(&pAhciPort->MediaTrackType, ATA_MEDIA_TYPE_UNKNOWN);

    if (pAhciPort->pDrvBase)
    {
        pAhciPort->regCMD |= AHCI_PORT_CMD_CPS; /* Indicate that there is a device on that port */

        if (pAhciPort->fPoweredOn)
        {
            /*
             * Set states in the Port Signature and SStatus registers.
             */
            if (pAhciPort->fATAPI)
                pAhciPort->regSIG = AHCI_PORT_SIG_ATAPI;
            else
                pAhciPort->regSIG = AHCI_PORT_SIG_DISK;
            pAhciPort->regSSTS = (0x01 << 8) | /* Interface is active. */
                                 (0x02 << 4) | /* Generation 2 (3.0GBps) speed. */
                                 (0x03 << 0);  /* Device detected and communication established. */
        }
    }
}

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

    LogRel(("AHCI#%u: Reset the HBA\n", pThis->CTX_SUFF(pDevIns)->iInstance));

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
                            AHCI_HBA_CAP_NCS_SET(pThis->cCmdSlotsAvail) | /* Number of command slots we support */
                            AHCI_HBA_CAP_NP_SET(pThis->cPortsImpl); /* Number of supported ports */
    pThis->regHbaCtrl     = AHCI_HBA_CTRL_AE;
    pThis->regHbaPi       = ahciGetPortsImplemented(pThis->cPortsImpl);
    pThis->regHbaVs       = AHCI_HBA_VS_MJR | AHCI_HBA_VS_MNR;
    pThis->regHbaCccCtl   = 0;
    pThis->regHbaCccPorts = 0;
    pThis->uCccTimeout    = 0;
    pThis->uCccPortNr     = 0;
    pThis->uCccNr         = 0;

    /* Clear pending interrupts. */
    pThis->regHbaIs            = 0;
    pThis->u32PortsInterrupted = 0;
    ahciHbaClearInterrupt(pThis);

    pThis->f64BitAddr = false;
    pThis->u32PortsInterrupted = 0;
    pThis->f8ByteMMIO4BytesWrittenSuccessfully = false;
    /* Clear the HBA Reset bit */
    pThis->regHbaCtrl &= ~AHCI_HBA_CTRL_HR;
}
#endif

/**
 * Reads from a AHCI controller register.
 *
 * @returns VBox status code.
 *
 * @param   pAhci       The AHCI instance.
 * @param   uReg        The register to write.
 * @param   pv          Where to store the result.
 * @param   cb          Number of bytes read.
 */
static int ahciRegisterRead(PAHCI pAhci, uint32_t uReg, void *pv, unsigned cb)
{
    int rc = VINF_SUCCESS;
    uint32_t iReg;

    /*
     * If the access offset is smaller than AHCI_HBA_GLOBAL_SIZE the guest accesses the global registers.
     * Otherwise it accesses the registers of a port.
     */
    if (uReg < AHCI_HBA_GLOBAL_SIZE)
    {
        iReg = uReg >> 2;
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
        uReg -= AHCI_HBA_GLOBAL_SIZE;
        iPort = uReg / AHCI_PORT_REGISTER_SIZE;
        iRegOffset  = (uReg % AHCI_PORT_REGISTER_SIZE);
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
                    AssertMsgFailed(("%s: unsupported access width cb=%d iPort=%x iRegOffset=%x iReg=%x!!!\n",
                                     __FUNCTION__, cb, iPort, iRegOffset, iReg));
            }
        }
    }

    return rc;
}

/**
 * Writes a value to one of the AHCI controller registers.
 *
 * @returns VBox status code.
 *
 * @param   pAhci       The AHCI instance.
 * @param   offReg      The offset of the register to write to.
 * @param   u32Value    The value to write.
 */
static int ahciRegisterWrite(PAHCI pAhci, uint32_t offReg, uint32_t u32Value)
{
    int rc;
    uint32_t iReg;

    /*
     * If the access offset is smaller than 100h the guest accesses the global registers.
     * Otherwise it accesses the registers of a port.
     */
    if (offReg < AHCI_HBA_GLOBAL_SIZE)
    {
        Log3(("Write global HBA register\n"));
        iReg = offReg >> 2;
        if (iReg < RT_ELEMENTS(g_aOpRegs))
        {
            const AHCIOPREG *pReg = &g_aOpRegs[iReg];
            rc = pReg->pfnWrite(pAhci, iReg, u32Value);
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
        offReg -= AHCI_HBA_GLOBAL_SIZE;
        iPort   =  offReg / AHCI_PORT_REGISTER_SIZE;
        iReg    = (offReg % AHCI_PORT_REGISTER_SIZE) >> 2;
        Log3(("%s: Trying to write to port %u and register %u\n", __FUNCTION__, iPort, iReg));
        if (RT_LIKELY(   iPort < pAhci->cPortsImpl
                      && iReg < RT_ELEMENTS(g_aPortOpRegs)))
        {
            const AHCIPORTOPREG *pPortReg = &g_aPortOpRegs[iReg];
            rc = pPortReg->pfnWrite(pAhci, &pAhci->ahciPort[iPort], iReg, u32Value);
        }
        else
        {
            Log3(("%s: Trying to write port %u register %u/%u!!!\n", __FUNCTION__, iPort, iReg, RT_ELEMENTS(g_aPortOpRegs)));
            rc = VINF_SUCCESS;
        }
    }

    return rc;
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
    Log2(("#%d ahciMMIORead: pvUser=%p:{%.*Rhxs} cb=%d GCPhysAddr=%RGp\n",
          pDevIns->iInstance, pv, cb, pv, cb, GCPhysAddr));

    int rc = ahciRegisterRead(pAhci, GCPhysAddr - pAhci->MMIOBase, pv, cb);

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
PDMBOTHCBDECL(int) ahciMMIOWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void const *pv, unsigned cb)
{
    PAHCI pAhci = PDMINS_2_DATA(pDevIns, PAHCI);
    Assert(cb == 4 || cb == 8);
    Assert(!(GCPhysAddr & (cb - 1)));

    /* Break up 64 bits writes into two dword writes. */
    /** @todo Eliminate this code once the IOM/EM starts taking care of these
         *    situations. */
    if (cb == 8)
    {
        /*
         * Only write the first 4 bytes if they weren't already.
         * It is possible that the last write to the register caused a world
         * switch and we entered this function again.
         * Writing the first 4 bytes again could cause indeterminate behavior
         * which can cause errors in the guest.
         */
        int rc = VINF_SUCCESS;
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

    /* Do the access. */
    Log2(("#%d ahciMMIOWrite: pvUser=%p:{%.*Rhxs} cb=%d GCPhysAddr=%RGp\n", pDevIns->iInstance, pv, cb, pv, cb, GCPhysAddr));
    return ahciRegisterWrite(pAhci, GCPhysAddr - pAhci->MMIOBase, *(uint32_t const *)pv);
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

/**
 * I/O port handler for writes to the index/data register pair.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   Port        Port address where the write starts.
 * @param   pv          Where to fetch the result.
 * @param   cb          Number of bytes to write.
 */
PDMBOTHCBDECL(int) ahciIdxDataWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    PAHCI pAhci = PDMINS_2_DATA(pDevIns, PAHCI);
    int   rc = VINF_SUCCESS;

    if (Port - pAhci->IOPortBase >= 8)
    {
        unsigned iReg = (Port - pAhci->IOPortBase - 8) / 4;

        Assert(cb == 4);

        if (iReg == 0)
        {
            /* Write the index register. */
            pAhci->regIdx = u32;
        }
        else
        {
            /** @todo range check? */
            Assert(iReg == 1);
            rc = ahciRegisterWrite(pAhci, pAhci->regIdx, u32);
            if (rc == VINF_IOM_R3_MMIO_WRITE)
                rc = VINF_IOM_R3_IOPORT_WRITE;
        }
    }
    /* else: ignore */

    Log2(("#%d ahciIdxDataWrite: pu32=%p:{%.*Rhxs} cb=%d Port=%#x rc=%Rrc\n",
          pDevIns->iInstance, &u32, cb, &u32, cb, Port, rc));
    return rc;
}

/**
 * I/O port handler for reads from the index/data register pair.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   Port        Port address where the read starts.
 * @param   pv          Where to fetch the result.
 * @param   cb          Number of bytes to write.
 */
PDMBOTHCBDECL(int) ahciIdxDataRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    PAHCI pAhci = PDMINS_2_DATA(pDevIns, PAHCI);
    int   rc = VINF_SUCCESS;

    if (Port - pAhci->IOPortBase >= 8)
    {
        unsigned iReg = (Port - pAhci->IOPortBase - 8) / 4;

        Assert(cb == 4);

        if (iReg == 0)
        {
            /* Read the index register. */
            *pu32 = pAhci->regIdx;
        }
        else
        {
            Assert(iReg == 1);
            /** @todo range check? */
            rc = ahciRegisterRead(pAhci, pAhci->regIdx, pu32, cb);
            if (rc == VINF_IOM_R3_MMIO_READ)
                rc = VINF_IOM_R3_IOPORT_READ;
            else if (rc == VINF_IOM_MMIO_UNUSED_00)
                rc = VERR_IOM_IOPORT_UNUSED;
        }
    }
    else
        *pu32 = UINT32_C(0xffffffff);

    Log2(("#%d ahciIdxDataRead: pu32=%p:{%.*Rhxs} cb=%d Port=%#x rc=%Rrc\n",
          pDevIns->iInstance, pu32, cb, pu32, cb, Port, rc));
    return rc;
}

#ifdef IN_RING3

static DECLCALLBACK(int) ahciR3MMIOMap(PPCIDEVICE pPciDev, /*unsigned*/ int iRegion, RTGCPHYS GCPhysAddress, uint32_t cb, PCIADDRESSSPACE enmType)
{
    PAHCI pThis = PCIDEV_2_PAHCI(pPciDev);
    PPDMDEVINS pDevIns = pPciDev->pDevIns;
    int   rc = VINF_SUCCESS;

    Log2(("%s: registering MMIO area at GCPhysAddr=%RGp cb=%u\n", __FUNCTION__, GCPhysAddress, cb));

    Assert(enmType == PCI_ADDRESS_SPACE_MEM);
    Assert(cb >= 4352);

    /* We use the assigned size here, because we currently only support page aligned MMIO ranges. */
    /** @todo change this to IOMMMIO_FLAGS_WRITE_ONLY_DWORD once EM/IOM starts
     * handling 2nd DWORD failures on split accesses correctly. */
    rc = PDMDevHlpMMIORegister(pDevIns, GCPhysAddress, cb, NULL /*pvUser*/,
                               IOMMMIO_FLAGS_READ_DWORD | IOMMMIO_FLAGS_WRITE_ONLY_DWORD_QWORD,
                               ahciMMIOWrite, ahciMMIORead, "AHCI");
    if (RT_FAILURE(rc))
        return rc;

    if (pThis->fR0Enabled)
    {
        rc = PDMDevHlpMMIORegisterR0(pDevIns, GCPhysAddress, cb, NIL_RTR0PTR /*pvUser*/, "ahciMMIOWrite", "ahciMMIORead");
        if (RT_FAILURE(rc))
            return rc;
    }

    if (pThis->fGCEnabled)
    {
        rc = PDMDevHlpMMIORegisterRC(pDevIns, GCPhysAddress, cb, NIL_RTRCPTR /*pvUser*/, "ahciMMIOWrite", "ahciMMIORead");
        if (RT_FAILURE(rc))
            return rc;
    }

    pThis->MMIOBase = GCPhysAddress;
    return rc;
}

/**
 * Map the legacy I/O port ranges to make Solaris work with the controller.
 */
static DECLCALLBACK(int) ahciR3LegacyFakeIORangeMap(PPCIDEVICE pPciDev, /*unsigned*/ int iRegion, RTGCPHYS GCPhysAddress, uint32_t cb, PCIADDRESSSPACE enmType)
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
        rc = PDMDevHlpIOPortRegisterRC(pDevIns, (RTIOPORT)GCPhysAddress, cb, 0,
                                       "ahciLegacyFakeWrite", "ahciLegacyFakeRead", NULL, NULL, "AHCI Fake");
        if (RT_FAILURE(rc))
            return rc;
    }

    return rc;
}

/**
 * Map the BMDMA I/O port range (used for the Index/Data pair register access)
 */
static DECLCALLBACK(int) ahciR3IdxDataIORangeMap(PPCIDEVICE pPciDev, /*unsigned*/ int iRegion, RTGCPHYS GCPhysAddress, uint32_t cb, PCIADDRESSSPACE enmType)
{
    PAHCI pThis = PCIDEV_2_PAHCI(pPciDev);
    PPDMDEVINS pDevIns = pPciDev->pDevIns;
    int   rc = VINF_SUCCESS;

    Log2(("%s: registering fake I/O area at GCPhysAddr=%RGp cb=%u\n", __FUNCTION__, GCPhysAddress, cb));

    Assert(enmType == PCI_ADDRESS_SPACE_IO);

    /* We use the assigned size here, because we currently only support page aligned MMIO ranges. */
    rc = PDMDevHlpIOPortRegister(pDevIns, (RTIOPORT)GCPhysAddress, cb, NULL,
                                 ahciIdxDataWrite, ahciIdxDataRead, NULL, NULL, "AHCI IDX/DATA");
    if (RT_FAILURE(rc))
        return rc;

    if (pThis->fR0Enabled)
    {
        rc = PDMDevHlpIOPortRegisterR0(pDevIns, (RTIOPORT)GCPhysAddress, cb, 0,
                                       "ahciIdxDataWrite", "ahciIdxDataRead", NULL, NULL, "AHCI IDX/DATA");
        if (RT_FAILURE(rc))
            return rc;
    }

    if (pThis->fGCEnabled)
    {
        rc = PDMDevHlpIOPortRegisterRC(pDevIns, (RTIOPORT)GCPhysAddress, cb, 0,
                                       "ahciIdxDataWrite", "ahciIdxDataRead", NULL, NULL, "AHCI IDX/DATA");
        if (RT_FAILURE(rc))
            return rc;
    }

    pThis->IOPortBase = (RTIOPORT)GCPhysAddress;
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
static DECLCALLBACK(int) ahciR3Status_QueryStatusLed(PPDMILEDPORTS pInterface, unsigned iLUN, PPDMLED *ppLed)
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
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) ahciR3Status_QueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PAHCI pThis = PDMIBASE_2_PAHCI(pInterface);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pThis->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMILEDPORTS, &pThis->ILeds);
    return NULL;
}

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) ahciR3PortQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PAHCIPort pAhciPort = PDMIBASE_2_PAHCIPORT(pInterface);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pAhciPort->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIMEDIAPORT, &pAhciPort->IPort);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIMEDIAASYNCPORT, &pAhciPort->IPortAsync);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIMOUNTNOTIFY, &pAhciPort->IMountNotify);
    return NULL;
}

/**
 * @interface_method_impl{PDMIMEDIAPORT,pfnQueryDeviceLocation}
 */
static DECLCALLBACK(int) ahciR3PortQueryDeviceLocation(PPDMIMEDIAPORT pInterface, const char **ppcszController,
                                                       uint32_t *piInstance, uint32_t *piLUN)
{
    PAHCIPort pAhciPort = PDMIMEDIAPORT_2_PAHCIPORT(pInterface);
    PPDMDEVINS pDevIns = pAhciPort->CTX_SUFF(pDevIns);

    AssertPtrReturn(ppcszController, VERR_INVALID_POINTER);
    AssertPtrReturn(piInstance, VERR_INVALID_POINTER);
    AssertPtrReturn(piLUN, VERR_INVALID_POINTER);

    *ppcszController = pDevIns->pReg->szName;
    *piInstance = pDevIns->iInstance;
    *piLUN = pAhciPort->iLUN;

    return VINF_SUCCESS;
}

#ifdef LOG_ENABLED

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
                ahciLog(("%s: Command register update\n", __FUNCTION__));
            else
                ahciLog(("%s: Control register update\n", __FUNCTION__));
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
                ahciLog(("%s: Reset bit is set\n", __FUNCTION__));
            break;
        }
        case AHCI_CMDFIS_TYPE_D2H:
        {
            ahciLog(("%s: Command Fis type D2H\n", __FUNCTION__));
            ahciLog(("%s: Command Fis size: %d\n", __FUNCTION__, AHCI_CMDFIS_TYPE_D2H_SIZE));
            break;
        }
        case AHCI_CMDFIS_TYPE_SETDEVBITS:
        {
            ahciLog(("%s: Command Fis type Set Device Bits\n", __FUNCTION__));
            ahciLog(("%s: Command Fis size: %d\n", __FUNCTION__, AHCI_CMDFIS_TYPE_SETDEVBITS_SIZE));
            break;
        }
        case AHCI_CMDFIS_TYPE_DMAACTD2H:
        {
            ahciLog(("%s: Command Fis type DMA Activate H2D\n", __FUNCTION__));
            ahciLog(("%s: Command Fis size: %d\n", __FUNCTION__, AHCI_CMDFIS_TYPE_DMAACTD2H_SIZE));
            break;
        }
        case AHCI_CMDFIS_TYPE_DMASETUP:
        {
            ahciLog(("%s: Command Fis type DMA Setup\n", __FUNCTION__));
            ahciLog(("%s: Command Fis size: %d\n", __FUNCTION__, AHCI_CMDFIS_TYPE_DMASETUP_SIZE));
            break;
        }
        case AHCI_CMDFIS_TYPE_PIOSETUP:
        {
            ahciLog(("%s: Command Fis type PIO Setup\n", __FUNCTION__));
            ahciLog(("%s: Command Fis size: %d\n", __FUNCTION__, AHCI_CMDFIS_TYPE_PIOSETUP_SIZE));
            break;
        }
        case AHCI_CMDFIS_TYPE_DATA:
        {
            ahciLog(("%s: Command Fis type Data\n", __FUNCTION__));
            break;
        }
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
 * @param   pAhciPort   Pointer to the port the command header was read from.
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

#endif /* LOG_ENABLED */

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
    if (!pAhciPort->fATAPI)
        pAhciPort->regTFD |= ATA_STAT_READY;

    ahciPostFisIntoMemory(pAhciPort, AHCI_CMDFIS_TYPE_D2H, d2hFis);
}

/**
 * Post the FIS in the memory area allocated by the guest and set interrupt if necessary.
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
                break;
            }
            case AHCI_CMDFIS_TYPE_SETDEVBITS:
            {
                GCPhysAddrRecFis += AHCI_RECFIS_SDBFIS_OFFSET;
                cbFis = AHCI_CMDFIS_TYPE_SETDEVBITS_SIZE;
                break;
            }
            case AHCI_CMDFIS_TYPE_DMASETUP:
            {
                GCPhysAddrRecFis += AHCI_RECFIS_DSFIS_OFFSET;
                cbFis = AHCI_CMDFIS_TYPE_DMASETUP_SIZE;
                break;
            }
            case AHCI_CMDFIS_TYPE_PIOSETUP:
            {
                GCPhysAddrRecFis += AHCI_RECFIS_PSFIS_OFFSET;
                cbFis = AHCI_CMDFIS_TYPE_PIOSETUP_SIZE;
                break;
            }
            default:
                /*
                 * We should post the unknown FIS into memory too but this never happens because
                 * we know which FIS types we generate. ;)
                 */
                AssertMsgFailed(("%s: Unknown FIS type!\n", __FUNCTION__));
        }

        /* Post the FIS into memory. */
        ahciLog(("%s: PDMDevHlpPCIPhysWrite GCPhysAddrRecFis=%RGp cbFis=%u\n", __FUNCTION__, GCPhysAddrRecFis, cbFis));
        PDMDevHlpPCIPhysWrite(pAhciPort->CTX_SUFF(pDevIns), GCPhysAddrRecFis, pCmdFis, cbFis);
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

static void atapiCmdOK(PAHCIPort pAhciPort, PAHCIREQ pAhciReq)
{
    pAhciReq->uATARegError = 0;
    pAhciReq->uATARegStatus = ATA_STAT_READY | ATA_STAT_SEEK;
    pAhciReq->cmdFis[AHCI_CMDFIS_SECTN] = (pAhciReq->cmdFis[AHCI_CMDFIS_SECTN] & ~7)
        | ((pAhciReq->enmTxDir != AHCITXDIR_WRITE) ? ATAPI_INT_REASON_IO : 0)
        | (!pAhciReq->cbTransfer ? ATAPI_INT_REASON_CD : 0);
    memset(pAhciPort->abATAPISense, '\0', sizeof(pAhciPort->abATAPISense));
    pAhciPort->abATAPISense[0] = 0x70;
    pAhciPort->abATAPISense[7] = 10;
}

static void atapiCmdError(PAHCIPort pAhciPort, PAHCIREQ pAhciReq, const uint8_t *pabATAPISense, size_t cbATAPISense)
{
    Log(("%s: sense=%#x (%s) asc=%#x ascq=%#x (%s)\n", __FUNCTION__, pabATAPISense[2] & 0x0f, SCSISenseText(pabATAPISense[2] & 0x0f),
             pabATAPISense[12], pabATAPISense[13], SCSISenseExtText(pabATAPISense[12], pabATAPISense[13])));
    pAhciReq->uATARegError = pabATAPISense[2] << 4;
    pAhciReq->uATARegStatus = ATA_STAT_READY | ATA_STAT_ERR;
    pAhciReq->cmdFis[AHCI_CMDFIS_SECTN] = (pAhciReq->cmdFis[AHCI_CMDFIS_SECTN] & ~7) |
                                                     ATAPI_INT_REASON_IO | ATAPI_INT_REASON_CD;
    memset(pAhciPort->abATAPISense, '\0', sizeof(pAhciPort->abATAPISense));
    memcpy(pAhciPort->abATAPISense, pabATAPISense, RT_MIN(cbATAPISense, sizeof(pAhciPort->abATAPISense)));
}

/** @todo deprecated function - doesn't provide enough info. Replace by direct
 * calls to atapiCmdError()  with full data. */
static void atapiCmdErrorSimple(PAHCIPort pAhciPort, PAHCIREQ pAhciReq, uint8_t uATAPISenseKey, uint8_t uATAPIASC)
{
    uint8_t abATAPISense[ATAPI_SENSE_SIZE];
    memset(abATAPISense, '\0', sizeof(abATAPISense));
    abATAPISense[0] = 0x70 | (1 << 7);
    abATAPISense[2] = uATAPISenseKey & 0x0f;
    abATAPISense[7] = 10;
    abATAPISense[12] = uATAPIASC;
    atapiCmdError(pAhciPort, pAhciReq, abATAPISense, sizeof(abATAPISense));
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

static uint32_t ataChecksum(void* ptr, size_t count)
{
    uint8_t u8Sum = 0xa5, *p = (uint8_t*)ptr;
    size_t i;

    for (i = 0; i < count; i++)
    {
      u8Sum += *p++;
    }

    return (uint8_t)-(int32_t)u8Sum;
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
    if (   pAhciPort->pDrvMedia->pfnDiscard
        || ( pAhciPort->fAsyncInterface
            && pAhciPort->pDrvMediaAsync->pfnStartDiscard)
        || pAhciPort->cbSector != 512
        || pAhciPort->fNonRotational)
    {
        p[80] = RT_H2LE_U16(0x1f0); /* support everything up to ATA/ATAPI-8 ACS */
        p[81] = RT_H2LE_U16(0x28); /* conforms to ATA/ATAPI-8 ACS */
    }
    else
    {
        p[80] = RT_H2LE_U16(0x7e); /* support everything up to ATA/ATAPI-6 */
        p[81] = RT_H2LE_U16(0x22); /* conforms to ATA/ATAPI-6 */
    }
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

    /* valid information, more than one logical sector per physical sector, 2^cLogSectorsPerPhysicalExp logical sectors per physical sector */
    if (pAhciPort->cLogSectorsPerPhysicalExp)
        p[106] = RT_H2LE_U16(RT_BIT(14) | RT_BIT(13) | pAhciPort->cLogSectorsPerPhysicalExp);

    if (pAhciPort->cbSector != 512)
    {
        uint32_t cSectorSizeInWords = pAhciPort->cbSector / sizeof(uint16_t);
        /* Enable reporting of logical sector size. */
        p[106] |= RT_H2LE_U16(RT_BIT(12) | RT_BIT(14));
        p[117] = RT_H2LE_U16(cSectorSizeInWords);
        p[118] = RT_H2LE_U16(cSectorSizeInWords >> 16);
    }

    if (pAhciPort->fNonRotational)
        p[217] = RT_H2LE_U16(1); /* Non-rotational medium */

    if (   pAhciPort->pDrvMedia->pfnDiscard
        || (   pAhciPort->fAsyncInterface
            && pAhciPort->pDrvMediaAsync->pfnStartDiscard)) /** @todo: Set bit 14 in word 69 too? (Deterministic read after TRIM). */
        p[169] = RT_H2LE_U16(1); /* DATA SET MANAGEMENT command supported. */

    /* The following are SATA specific */
    p[75] = RT_H2LE_U16(pAhciPort->CTX_SUFF(pAhci)->cCmdSlotsAvail-1); /* Number of commands we support, 0's based */
    p[76] = RT_H2LE_U16((1 << 8) | (1 << 2)); /* Native command queuing and Serial ATA Gen2 (3.0 Gbps) speed supported */

    uint32_t uCsum = ataChecksum(p, 510);
    p[255] = RT_H2LE_U16(0xa5 | (uCsum << 8)); /* Integrity word */

    return VINF_SUCCESS;
}

typedef int (*PAtapiFunc)(PAHCIREQ, PAHCIPort, size_t, size_t *);

static int atapiGetConfigurationSS(PAHCIREQ, PAHCIPort, size_t, size_t *);
static int atapiGetEventStatusNotificationSS(PAHCIREQ, PAHCIPort, size_t, size_t *);
static int atapiIdentifySS(PAHCIREQ, PAHCIPort, size_t, size_t *);
static int atapiInquirySS(PAHCIREQ, PAHCIPort, size_t, size_t *);
static int atapiMechanismStatusSS(PAHCIREQ, PAHCIPort, size_t, size_t *);
static int atapiModeSenseErrorRecoverySS(PAHCIREQ, PAHCIPort, size_t, size_t *);
static int atapiModeSenseCDStatusSS(PAHCIREQ, PAHCIPort, size_t, size_t *);
static int atapiReadCapacitySS(PAHCIREQ, PAHCIPort, size_t, size_t *);
static int atapiReadDiscInformationSS(PAHCIREQ, PAHCIPort, size_t, size_t *);
static int atapiReadTOCNormalSS(PAHCIREQ, PAHCIPort, size_t, size_t *);
static int atapiReadTOCMultiSS(PAHCIREQ, PAHCIPort, size_t, size_t *);
static int atapiReadTOCRawSS(PAHCIREQ, PAHCIPort, size_t, size_t *);
static int atapiReadTrackInformationSS(PAHCIREQ, PAHCIPort, size_t, size_t *);
static int atapiRequestSenseSS(PAHCIREQ, PAHCIPort, size_t, size_t *);
static int atapiPassthroughSS(PAHCIREQ, PAHCIPort, size_t, size_t *);
static int atapiReadDVDStructureSS(PAHCIREQ, PAHCIPort, size_t, size_t *);

/**
 * Source/sink function indexes for g_apfnAtapiFuncs.
 */
typedef enum ATAPIFN
{
    ATAFN_SS_NULL = 0,
    ATAFN_SS_ATAPI_GET_CONFIGURATION,
    ATAFN_SS_ATAPI_GET_EVENT_STATUS_NOTIFICATION,
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
    ATAFN_SS_ATAPI_PASSTHROUGH,
    ATAFN_SS_ATAPI_READ_DVD_STRUCTURE,
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
    atapiGetEventStatusNotificationSS,
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
    atapiRequestSenseSS,
    atapiPassthroughSS,
    atapiReadDVDStructureSS
};

static int atapiIdentifySS(PAHCIREQ pAhciReq, PAHCIPort pAhciPort, size_t cbData, size_t *pcbData)
{
    uint16_t p[256];

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
    *pcbData =  ahciCopyToPrdtl(pAhciPort->pDevInsR3, pAhciReq, (void *)&p[0],
                                RT_MIN(cbData, sizeof(p)));

    atapiCmdOK(pAhciPort, pAhciReq);
    return VINF_SUCCESS;
}

static int atapiReadCapacitySS(PAHCIREQ pAhciReq, PAHCIPort pAhciPort, size_t cbData, size_t *pcbData)
{
    uint8_t aBuf[8];

    ataH2BE_U32(aBuf, pAhciPort->cTotalSectors - 1);
    ataH2BE_U32(aBuf + 4, 2048);

    /* Copy the buffer in to the scatter gather list. */
    *pcbData = ahciCopyToPrdtl(pAhciPort->pDevInsR3, pAhciReq, (void *)&aBuf[0],
                               RT_MIN(cbData, sizeof(aBuf)));

    atapiCmdOK(pAhciPort, pAhciReq);
    return VINF_SUCCESS;
}


static int atapiReadDiscInformationSS(PAHCIREQ pAhciReq, PAHCIPort pAhciPort, size_t cbData, size_t *pcbData)
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
    *pcbData = ahciCopyToPrdtl(pAhciPort->pDevInsR3, pAhciReq, (void *)&aBuf[0],
                               RT_MIN(cbData, sizeof(aBuf)));

    atapiCmdOK(pAhciPort, pAhciReq);
    return VINF_SUCCESS;
}


static int atapiReadTrackInformationSS(PAHCIREQ pAhciReq, PAHCIPort pAhciPort, size_t cbData, size_t *pcbData)
{
    uint8_t aBuf[36];

    /* Accept address/number type of 1 only, and only track 1 exists. */
    if ((pAhciReq->aATAPICmd[1] & 0x03) != 1 || ataBE2H_U32(&pAhciReq->aATAPICmd[2]) != 1)
    {
        atapiCmdErrorSimple(pAhciPort, pAhciReq, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_INV_FIELD_IN_CMD_PACKET);
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
    *pcbData = ahciCopyToPrdtl(pAhciPort->pDevInsR3, pAhciReq, (void *)&aBuf[0],
                               RT_MIN(cbData, sizeof(aBuf)));

    atapiCmdOK(pAhciPort, pAhciReq);
    return VINF_SUCCESS;
}

static size_t atapiGetConfigurationFillFeatureListProfiles(PAHCIPort pAhciPort, uint8_t *pbBuf, size_t cbBuf)
{
    if (cbBuf < 3*4)
        return 0;

    ataH2BE_U16(pbBuf, 0x0); /* feature 0: list of profiles supported */
    pbBuf[2] = (0 << 2) | (1 << 1) | (1 << 0); /* version 0, persistent, current */
    pbBuf[3] = 8; /* additional bytes for profiles */
    /* The MMC-3 spec says that DVD-ROM read capability should be reported
     * before CD-ROM read capability. */
    ataH2BE_U16(pbBuf + 4, 0x10); /* profile: read-only DVD */
    pbBuf[6] = (0 << 0); /* NOT current profile */
    ataH2BE_U16(pbBuf + 8, 0x08); /* profile: read only CD */
    pbBuf[10] = (1 << 0); /* current profile */

    return 3*4; /* Header + 2 profiles entries */
}

static size_t atapiGetConfigurationFillFeatureCore(PAHCIPort pAhciPort, uint8_t *pbBuf, size_t cbBuf)
{
    if (cbBuf < 12)
        return 0;

    ataH2BE_U16(pbBuf, 0x1); /* feature 0001h: Core Feature */
    pbBuf[2] = (0x2 << 2) | RT_BIT(1) | RT_BIT(0); /* Version | Persistent | Current */
    pbBuf[3] = 8; /* Additional length */
    ataH2BE_U16(pbBuf + 4, 0x00000002); /* Physical interface ATAPI. */
    pbBuf[8] = RT_BIT(0); /* DBE */
    /* Rest is reserved. */

    return 12;
}

static size_t atapiGetConfigurationFillFeatureMorphing(PAHCIPort pAhciPort, uint8_t *pbBuf, size_t cbBuf)
{
    if (cbBuf < 8)
        return 0;

    ataH2BE_U16(pbBuf, 0x2); /* feature 0002h: Morphing Feature */
    pbBuf[2] = (0x1 << 2) | RT_BIT(1) | RT_BIT(0); /* Version | Persistent | Current */
    pbBuf[3] = 4; /* Additional length */
    pbBuf[4] = RT_BIT(1) | 0x0; /* OCEvent | !ASYNC */
    /* Rest is reserved. */

    return 8;
}

static size_t atapiGetConfigurationFillFeatureRemovableMedium(PAHCIPort pAhciPort, uint8_t *pbBuf, size_t cbBuf)
{
    if (cbBuf < 8)
        return 0;

    ataH2BE_U16(pbBuf, 0x3); /* feature 0003h: Removable Medium Feature */
    pbBuf[2] = (0x2 << 2) | RT_BIT(1) | RT_BIT(0); /* Version | Persistent | Current */
    pbBuf[3] = 4; /* Additional length */
    /* Tray type loading | Load | Eject | !Pvnt Jmpr | !DBML | Lock */
    pbBuf[4] = (0x2 << 5) | RT_BIT(4) | RT_BIT(3) | (0x0 << 2) | (0x0 << 1) | RT_BIT(0);
    /* Rest is reserved. */

    return 8;
}

static size_t atapiGetConfigurationFillFeatureRandomReadable(PAHCIPort pAhciPort, uint8_t *pbBuf, size_t cbBuf)
{
    if (cbBuf < 12)
        return 0;

    ataH2BE_U16(pbBuf, 0x10); /* feature 0010h: Random Readable Feature */
    pbBuf[2] = (0x0 << 2) | RT_BIT(1) | RT_BIT(0); /* Version | Persistent | Current */
    pbBuf[3] = 8; /* Additional length */
    ataH2BE_U32(pbBuf + 4, 2048); /* Logical block size. */
    ataH2BE_U16(pbBuf + 8, 0x10); /* Blocking (0x10 for DVD, CD is not defined). */
    pbBuf[10] = 0; /* PP not present */
    /* Rest is reserved. */

    return 12;
}

static size_t atapiGetConfigurationFillFeatureCDRead(PAHCIPort pAhciPort, uint8_t *pbBuf, size_t cbBuf)
{
    if (cbBuf < 8)
        return 0;

    ataH2BE_U16(pbBuf, 0x1e); /* feature 001Eh: CD Read Feature */
    pbBuf[2] = (0x2 << 2) | RT_BIT(1) | RT_BIT(0); /* Version | Persistent | Current */
    pbBuf[3] = 0; /* Additional length */
    pbBuf[4] = (0x0 << 7) | (0x0 << 1) | 0x0; /* !DAP | !C2-Flags | !CD-Text. */
    /* Rest is reserved. */

    return 8;
}

static size_t atapiGetConfigurationFillFeaturePowerManagement(PAHCIPort pAhciPort, uint8_t *pbBuf, size_t cbBuf)
{
    if (cbBuf < 4)
        return 0;

    ataH2BE_U16(pbBuf, 0x100); /* feature 0100h: Power Management Feature */
    pbBuf[2] = (0x0 << 2) | RT_BIT(1) | RT_BIT(0); /* Version | Persistent | Current */
    pbBuf[3] = 0; /* Additional length */

    return 4;
}

static size_t atapiGetConfigurationFillFeatureTimeout(PAHCIPort pAhciPort, uint8_t *pbBuf, size_t cbBuf)
{
    if (cbBuf < 8)
        return 0;

    ataH2BE_U16(pbBuf, 0x105); /* feature 0105h: Timeout Feature */
    pbBuf[2] = (0x0 << 2) | RT_BIT(1) | RT_BIT(0); /* Version | Persistent | Current */
    pbBuf[3] = 4; /* Additional length */
    pbBuf[4] = 0x0; /* !Group3 */

    return 8;
}

static int atapiGetConfigurationSS(PAHCIREQ pAhciReq, PAHCIPort pAhciPort, size_t cbData, size_t *pcbData)
{
    uint8_t aBuf[80];
    uint8_t *pbBuf = &aBuf[0];
    size_t cbBuf = sizeof(aBuf);
    size_t cbCopied = 0;

    /* Accept valid request types only, and only starting feature 0. */
    if ((pAhciReq->aATAPICmd[1] & 0x03) == 3 || ataBE2H_U16(&pAhciReq->aATAPICmd[2]) != 0)
    {
        atapiCmdErrorSimple(pAhciPort, pAhciReq, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_INV_FIELD_IN_CMD_PACKET);
        return VINF_SUCCESS;
    }
    /** @todo implement switching between CD-ROM and DVD-ROM profile (the only
     * way to differentiate them right now is based on the image size). */
    if (pAhciPort->cTotalSectors)
        ataH2BE_U16(pbBuf + 6, 0x08); /* current profile: read-only CD */
    else
        ataH2BE_U16(pbBuf + 6, 0x00); /* current profile: none -> no media */
    cbBuf    -= 8;
    pbBuf    += 8;

    cbCopied = atapiGetConfigurationFillFeatureListProfiles(pAhciPort, pbBuf, cbBuf);
    cbBuf -= cbCopied;
    pbBuf += cbCopied;

    cbCopied = atapiGetConfigurationFillFeatureCore(pAhciPort, pbBuf, cbBuf);
    cbBuf -= cbCopied;
    pbBuf += cbCopied;

    cbCopied = atapiGetConfigurationFillFeatureMorphing(pAhciPort, pbBuf, cbBuf);
    cbBuf -= cbCopied;
    pbBuf += cbCopied;

    cbCopied = atapiGetConfigurationFillFeatureRemovableMedium(pAhciPort, pbBuf, cbBuf);
    cbBuf -= cbCopied;
    pbBuf += cbCopied;

    cbCopied = atapiGetConfigurationFillFeatureRandomReadable(pAhciPort, pbBuf, cbBuf);
    cbBuf -= cbCopied;
    pbBuf += cbCopied;

    cbCopied = atapiGetConfigurationFillFeatureCDRead(pAhciPort, pbBuf, cbBuf);
    cbBuf -= cbCopied;
    pbBuf += cbCopied;

    cbCopied = atapiGetConfigurationFillFeaturePowerManagement(pAhciPort, pbBuf, cbBuf);
    cbBuf -= cbCopied;
    pbBuf += cbCopied;

    cbCopied = atapiGetConfigurationFillFeatureTimeout(pAhciPort, pbBuf, cbBuf);
    cbBuf -= cbCopied;
    pbBuf += cbCopied;

    /* Set data length now. */
    ataH2BE_U32(&aBuf[0], sizeof(aBuf) - cbBuf);

    /* Copy the buffer in to the scatter gather list. */
    *pcbData = ahciCopyToPrdtl(pAhciPort->pDevInsR3, pAhciReq, (void *)&aBuf[0],
                               RT_MIN(cbData, sizeof(aBuf)));

    atapiCmdOK(pAhciPort, pAhciReq);
    return VINF_SUCCESS;
}


static int atapiGetEventStatusNotificationSS(PAHCIREQ pAhciReq, PAHCIPort pAhciPort, size_t cbData, size_t *pcbData)
{
    uint8_t abBuf[8];

    Assert(pAhciReq->enmTxDir == AHCITXDIR_READ);
    Assert(pAhciReq->cbTransfer <= 8);

    if (!(pAhciReq->aATAPICmd[1] & 1))
    {
        /* no asynchronous operation supported */
        atapiCmdErrorSimple(pAhciPort, pAhciReq, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_INV_FIELD_IN_CMD_PACKET);
        return VINF_SUCCESS;
    }

    uint32_t OldStatus, NewStatus;
    do
    {
        OldStatus = ASMAtomicReadU32(&pAhciPort->MediaEventStatus);
        NewStatus = ATA_EVENT_STATUS_UNCHANGED;
        switch (OldStatus)
        {
            case ATA_EVENT_STATUS_MEDIA_NEW:
                /* mount */
                ataH2BE_U16(abBuf + 0, 6);
                abBuf[2] = 0x04; /* media */
                abBuf[3] = 0x5e; /* supported = busy|media|external|power|operational */
                abBuf[4] = 0x02; /* new medium */
                abBuf[5] = 0x02; /* medium present / door closed */
                abBuf[6] = 0x00;
                abBuf[7] = 0x00;
                break;

            case ATA_EVENT_STATUS_MEDIA_CHANGED:
            case ATA_EVENT_STATUS_MEDIA_REMOVED:
                /* umount */
                ataH2BE_U16(abBuf + 0, 6);
                abBuf[2] = 0x04; /* media */
                abBuf[3] = 0x5e; /* supported = busy|media|external|power|operational */
                abBuf[4] = 0x03; /* media removal */
                abBuf[5] = 0x00; /* medium absent / door closed */
                abBuf[6] = 0x00;
                abBuf[7] = 0x00;
                if (OldStatus == ATA_EVENT_STATUS_MEDIA_CHANGED)
                    NewStatus = ATA_EVENT_STATUS_MEDIA_NEW;
                break;

            case ATA_EVENT_STATUS_MEDIA_EJECT_REQUESTED: /* currently unused */
                ataH2BE_U16(abBuf + 0, 6);
                abBuf[2] = 0x04; /* media */
                abBuf[3] = 0x5e; /* supported = busy|media|external|power|operational */
                abBuf[4] = 0x01; /* eject requested (eject button pressed) */
                abBuf[5] = 0x02; /* medium present / door closed */
                abBuf[6] = 0x00;
                abBuf[7] = 0x00;
                break;

            case ATA_EVENT_STATUS_UNCHANGED:
            default:
                ataH2BE_U16(abBuf + 0, 6);
                abBuf[2] = 0x01; /* operational change request / notification */
                abBuf[3] = 0x5e; /* supported = busy|media|external|power|operational */
                abBuf[4] = 0x00;
                abBuf[5] = 0x00;
                abBuf[6] = 0x00;
                abBuf[7] = 0x00;
                break;
        }
    } while (!ASMAtomicCmpXchgU32(&pAhciPort->MediaEventStatus, NewStatus, OldStatus));

    *pcbData = ahciCopyToPrdtl(pAhciPort->pDevInsR3, pAhciReq, (void *)&abBuf[0],
                               RT_MIN(cbData, sizeof(abBuf)));

    atapiCmdOK(pAhciPort, pAhciReq);
    return VINF_SUCCESS;
}


static int atapiInquirySS(PAHCIREQ pAhciReq, PAHCIPort pAhciPort, size_t cbData, size_t *pcbData)
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
    *pcbData = ahciCopyToPrdtl(pAhciPort->pDevInsR3, pAhciReq, (void *)&aBuf[0],
                               RT_MIN(cbData, sizeof(aBuf)));

    atapiCmdOK(pAhciPort, pAhciReq);
    return VINF_SUCCESS;
}


static int atapiModeSenseErrorRecoverySS(PAHCIREQ pAhciReq, PAHCIPort pAhciPort, size_t cbData, size_t *pcbData)
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
    *pcbData = ahciCopyToPrdtl(pAhciPort->pDevInsR3, pAhciReq, (void *)&aBuf[0],
                               RT_MIN(cbData, sizeof(aBuf)));

    atapiCmdOK(pAhciPort, pAhciReq);
    return VINF_SUCCESS;
}


static int atapiModeSenseCDStatusSS(PAHCIREQ pAhciReq, PAHCIPort pAhciPort, size_t cbData, size_t *pcbData)
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
    *pcbData = ahciCopyToPrdtl(pAhciPort->pDevInsR3, pAhciReq, (void *)&aBuf[0],
                               RT_MIN(cbData, sizeof(aBuf)));

    atapiCmdOK(pAhciPort, pAhciReq);
    return VINF_SUCCESS;
}


static int atapiRequestSenseSS(PAHCIREQ pAhciReq, PAHCIPort pAhciPort, size_t cbData, size_t *pcbData)
{
    /* Copy the buffer in to the scatter gather list. */
    *pcbData = ahciCopyToPrdtl(pAhciPort->pDevInsR3, pAhciReq,
                               pAhciPort->abATAPISense, RT_MIN(cbData, sizeof(pAhciPort->abATAPISense)));

    atapiCmdOK(pAhciPort, pAhciReq);
    return VINF_SUCCESS;
}


static int atapiMechanismStatusSS(PAHCIREQ pAhciReq, PAHCIPort pAhciPort, size_t cbData, size_t *pcbData)
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
    *pcbData = ahciCopyToPrdtl(pAhciPort->pDevInsR3, pAhciReq, (void *)&aBuf[0],
                               RT_MIN(cbData, sizeof(aBuf)));

    atapiCmdOK(pAhciPort, pAhciReq);
    return VINF_SUCCESS;
}


static int atapiReadTOCNormalSS(PAHCIREQ pAhciReq, PAHCIPort pAhciPort, size_t cbData, size_t *pcbData)
{
    uint8_t aBuf[20], *q, iStartTrack;
    bool fMSF;
    uint32_t cbSize;

    fMSF = (pAhciReq->aATAPICmd[1] >> 1) & 1;
    iStartTrack = pAhciReq->aATAPICmd[6];
    if (iStartTrack > 1 && iStartTrack != 0xaa)
    {
        atapiCmdErrorSimple(pAhciPort, pAhciReq, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_INV_FIELD_IN_CMD_PACKET);
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
    *pcbData = ahciCopyToPrdtl(pAhciPort->pDevInsR3, pAhciReq, (void *)&aBuf[0],
                               RT_MIN(cbData, cbSize));

    atapiCmdOK(pAhciPort, pAhciReq);
    return VINF_SUCCESS;
}


static int atapiReadTOCMultiSS(PAHCIREQ pAhciReq, PAHCIPort pAhciPort, size_t cbData, size_t *pcbData)
{
    uint8_t aBuf[12];
    bool fMSF;

    fMSF = (pAhciReq->aATAPICmd[1] >> 1) & 1;
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
    *pcbData = ahciCopyToPrdtl(pAhciPort->pDevInsR3, pAhciReq, (void *)&aBuf[0],
                               RT_MIN(cbData, sizeof(aBuf)));

    atapiCmdOK(pAhciPort, pAhciReq);
    return VINF_SUCCESS;
}


static int atapiReadTOCRawSS(PAHCIREQ pAhciReq, PAHCIPort pAhciPort, size_t cbData, size_t *pcbData)
{
    uint8_t aBuf[50]; /* Counted a maximum of 45 bytes but better be on the safe side. */
    uint8_t *q, iStartTrack;
    bool fMSF;
    uint32_t cbSize;

    fMSF = (pAhciReq->aATAPICmd[1] >> 1) & 1;
    iStartTrack = pAhciReq->aATAPICmd[6];

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
    *pcbData = ahciCopyToPrdtl(pAhciPort->pDevInsR3, pAhciReq, (void *)&aBuf[0],
                               RT_MIN(cbData, cbSize));

    atapiCmdOK(pAhciPort, pAhciReq);
    return VINF_SUCCESS;
}

/**
 * Sets the given media track type.
 */
static uint32_t ahciMediumTypeSet(PAHCIPort pAhciPort, uint32_t MediaTrackType)
{
    return ASMAtomicXchgU32(&pAhciPort->MediaTrackType, MediaTrackType);
}

static int atapiPassthroughSS(PAHCIREQ pAhciReq, PAHCIPort pAhciPort, size_t cbData, size_t *pcbData)
{
    int rc = VINF_SUCCESS;
    uint8_t abATAPISense[ATAPI_SENSE_SIZE];
    uint32_t cbTransfer;
    void *pvBuf = NULL;

    cbTransfer = pAhciReq->cbTransfer;

    if (cbTransfer)
    {
        pvBuf = (uint8_t *)RTMemAlloc(cbTransfer);
        if (!pvBuf)
            return VERR_NO_MEMORY;

        if (pAhciReq->enmTxDir == AHCITXDIR_WRITE)
        {
            ahciCopyFromPrdtl(pAhciPort->pDevInsR3, pAhciReq, pvBuf, cbTransfer);
            if (pAhciReq->fFlags & AHCI_REQ_OVERFLOW)
                return VINF_SUCCESS;
        }
    }

    /* Simple heuristics: if there is at least one sector of data
     * to transfer, it's worth updating the LEDs. */
    if (cbTransfer >= 2048)
    {
        if (pAhciReq->enmTxDir != AHCITXDIR_WRITE)
            pAhciPort->Led.Asserted.s.fReading = pAhciPort->Led.Actual.s.fReading = 1;
        else
            pAhciPort->Led.Asserted.s.fWriting = pAhciPort->Led.Actual.s.fWriting = 1;
    }

    if (cbTransfer > SCSI_MAX_BUFFER_SIZE)
    {
        /* Linux accepts commands with up to 100KB of data, but expects
         * us to handle commands with up to 128KB of data. The usual
         * imbalance of powers. */
        uint8_t aATAPICmd[ATAPI_PACKET_SIZE];
        uint32_t iATAPILBA, cSectors, cReqSectors, cbCurrTX;
        uint8_t *pbBuf = (uint8_t *)pvBuf;

        switch (pAhciReq->aATAPICmd[0])
        {
            case SCSI_READ_10:
            case SCSI_WRITE_10:
            case SCSI_WRITE_AND_VERIFY_10:
                iATAPILBA = ataBE2H_U32(pAhciReq->aATAPICmd + 2);
                cSectors = ataBE2H_U16(pAhciReq->aATAPICmd + 7);
                break;
            case SCSI_READ_12:
            case SCSI_WRITE_12:
                iATAPILBA = ataBE2H_U32(pAhciReq->aATAPICmd + 2);
                cSectors = ataBE2H_U32(pAhciReq->aATAPICmd + 6);
                break;
            case SCSI_READ_CD:
                iATAPILBA = ataBE2H_U32(pAhciReq->aATAPICmd + 2);
                cSectors = ataBE2H_U24(pAhciReq->aATAPICmd + 6);
                break;
            case SCSI_READ_CD_MSF:
                iATAPILBA = ataMSF2LBA(pAhciReq->aATAPICmd + 3);
                cSectors = ataMSF2LBA(pAhciReq->aATAPICmd + 6) - iATAPILBA;
                break;
            default:
                AssertMsgFailed(("Don't know how to split command %#04x\n", pAhciReq->aATAPICmd[0]));
                if (pAhciPort->cErrors++ < MAX_LOG_REL_ERRORS)
                    LogRel(("AHCI: LUN#%d: CD-ROM passthrough split error\n", pAhciPort->iLUN));
                atapiCmdErrorSimple(pAhciPort, pAhciReq, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_ILLEGAL_OPCODE);
                RTMemFree(pvBuf);
                return VINF_SUCCESS;
        }
        memcpy(aATAPICmd, pAhciReq->aATAPICmd, ATAPI_PACKET_SIZE);
        cReqSectors = 0;
        for (uint32_t i = cSectors; i > 0; i -= cReqSectors)
        {
            if (i * pAhciReq->cbATAPISector > SCSI_MAX_BUFFER_SIZE)
                cReqSectors = SCSI_MAX_BUFFER_SIZE / pAhciReq->cbATAPISector;
            else
                cReqSectors = i;
            cbCurrTX = pAhciReq->cbATAPISector * cReqSectors;
            switch (pAhciReq->aATAPICmd[0])
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
                    ataH2BE_U32(aATAPICmd + 2, iATAPILBA);
                    ataH2BE_U24(aATAPICmd + 6, cReqSectors);
                    break;
                case SCSI_READ_CD_MSF:
                    ataLBA2MSF(aATAPICmd + 3, iATAPILBA);
                    ataLBA2MSF(aATAPICmd + 6, iATAPILBA + cReqSectors);
                    break;
            }
            rc = pAhciPort->pDrvMedia->pfnSendCmd(pAhciPort->pDrvMedia,
                                                  aATAPICmd,
                                                  pAhciReq->enmTxDir == AHCITXDIR_READ
                                                  ? PDMMEDIATXDIR_FROM_DEVICE
                                                  : PDMMEDIATXDIR_TO_DEVICE,
                                                  pbBuf,
                                                  &cbCurrTX,
                                                  abATAPISense,
                                                  sizeof(abATAPISense),
                                                  30000 /**< @todo timeout */);
            if (rc != VINF_SUCCESS)
                break;
            iATAPILBA += cReqSectors;
            pbBuf += pAhciReq->cbATAPISector * cReqSectors;
        }
    }
    else
    {
        PDMMEDIATXDIR enmBlockTxDir = PDMMEDIATXDIR_NONE;

        if (pAhciReq->enmTxDir == AHCITXDIR_READ)
            enmBlockTxDir = PDMMEDIATXDIR_FROM_DEVICE;
        else if (pAhciReq->enmTxDir == AHCITXDIR_WRITE)
            enmBlockTxDir = PDMMEDIATXDIR_TO_DEVICE;
        else if (pAhciReq->enmTxDir == AHCITXDIR_NONE)
            enmBlockTxDir = PDMMEDIATXDIR_NONE;
        else
            AssertMsgFailed(("Invalid transfer direction %d\n", pAhciReq->enmTxDir));

        rc = pAhciPort->pDrvMedia->pfnSendCmd(pAhciPort->pDrvMedia,
                                              pAhciReq->aATAPICmd,
                                              enmBlockTxDir,
                                              pvBuf,
                                              &cbTransfer,
                                              abATAPISense,
                                              sizeof(abATAPISense),
                                              30000 /**< @todo timeout */);
    }

    /* Update the LEDs and the read/write statistics. */
    if (cbTransfer >= 2048)
    {
        if (pAhciReq->enmTxDir != AHCITXDIR_WRITE)
        {
            pAhciPort->Led.Actual.s.fReading = 0;
            STAM_REL_COUNTER_ADD(&pAhciPort->StatBytesRead, cbTransfer);
        }
        else
        {
            pAhciPort->Led.Actual.s.fWriting = 0;
            STAM_REL_COUNTER_ADD(&pAhciPort->StatBytesWritten, cbTransfer);
        }
    }

    if (RT_SUCCESS(rc))
    {
        /* Do post processing for certain commands. */
        switch (pAhciReq->aATAPICmd[0])
        {
            case SCSI_SEND_CUE_SHEET:
            case SCSI_READ_TOC_PMA_ATIP:
            {
                if (!pAhciPort->pTrackList)
                    rc = ATAPIPassthroughTrackListCreateEmpty(&pAhciPort->pTrackList);

                if (RT_SUCCESS(rc))
                    rc = ATAPIPassthroughTrackListUpdate(pAhciPort->pTrackList, pAhciReq->aATAPICmd, pvBuf);

                if (   RT_FAILURE(rc)
                    && pAhciPort->cErrors++ < MAX_LOG_REL_ERRORS)
                    LogRel(("AHCI: Error (%Rrc) while updating the tracklist during %s, burning the disc might fail\n",
                            rc, pAhciReq->aATAPICmd[0] == SCSI_SEND_CUE_SHEET ? "SEND CUE SHEET" : "READ TOC/PMA/ATIP"));
                break;
            }
            case SCSI_SYNCHRONIZE_CACHE:
            {
                if (pAhciPort->pTrackList)
                    ATAPIPassthroughTrackListClear(pAhciPort->pTrackList);
                break;
            }
        }

        if (pAhciReq->enmTxDir == AHCITXDIR_READ)
        {
           Assert(cbTransfer <= pAhciReq->cbTransfer);

            if (pAhciReq->aATAPICmd[0] == SCSI_INQUIRY)
            {
                /* Make sure that the real drive cannot be identified.
                 * Motivation: changing the VM configuration should be as
                 *             invisible as possible to the guest. */
                if (cbTransfer >= 8 + 8)
                    ataSCSIPadStr((uint8_t *)pvBuf + 8, "VBOX", 8);
                if (cbTransfer >= 16 + 16)
                    ataSCSIPadStr((uint8_t *)pvBuf + 16, "CD-ROM", 16);
                if (cbTransfer >= 32 + 4)
                    ataSCSIPadStr((uint8_t *)pvBuf + 32, "1.0", 4);
            }

            if (cbTransfer)
            {
                Log3(("ATAPI PT data read (%d): %.*Rhxs\n", cbTransfer, cbTransfer, (uint8_t *)pvBuf));

                /* Reply with the same amount of data as the real drive. */
                *pcbData = ahciCopyToPrdtl(pAhciPort->pDevInsR3, pAhciReq, pvBuf,
                                           cbTransfer);
            }
            else
                *pcbData = 0;
        }
        else
            *pcbData = cbTransfer;
        atapiCmdOK(pAhciPort, pAhciReq);
    }
    else
    {
        if (pAhciPort->cErrors < MAX_LOG_REL_ERRORS)
        {
            uint8_t u8Cmd = pAhciReq->aATAPICmd[0];
            do
            {
                /* don't log superfluous errors */
                if (    rc == VERR_DEV_IO_ERROR
                    && (   u8Cmd == SCSI_TEST_UNIT_READY
                        || u8Cmd == SCSI_READ_CAPACITY
                        || u8Cmd == SCSI_READ_DVD_STRUCTURE
                        || u8Cmd == SCSI_READ_TOC_PMA_ATIP))
                    break;
                pAhciPort->cErrors++;
                LogRel(("PIIX3 ATA: LUN#%d: CD-ROM passthrough cmd=%#04x sense=%d ASC=%#02x ASCQ=%#02x %Rrc\n",
                            pAhciPort->iLUN, u8Cmd, abATAPISense[2] & 0x0f, abATAPISense[12], abATAPISense[13], rc));
            } while (0);
        }
        atapiCmdError(pAhciPort, pAhciReq, abATAPISense, sizeof(abATAPISense));
    }

    if (pvBuf)
        RTMemFree(pvBuf);

    return VINF_SUCCESS;
}

/** @todo: Revise ASAP. */
/* Keep in sync with DevATA.cpp! */
static int atapiReadDVDStructureSS(PAHCIREQ pAhciReq, PAHCIPort pAhciPort, size_t cbData, size_t *pcbData)
{
    uint8_t aBuf[25]; /* Counted a maximum of 20 bytes but better be on the safe side. */
    uint8_t *buf = aBuf;
    int media = pAhciReq->aATAPICmd[1];
    int format = pAhciReq->aATAPICmd[7];

    uint16_t max_len = RT_MIN(ataBE2H_U16(&pAhciReq->aATAPICmd[8]), sizeof(aBuf));

    memset(buf, 0, max_len);

    switch (format) {
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
        case 0x08:
        case 0x09:
        case 0x0a:
        case 0x0b:
        case 0x0c:
        case 0x0d:
        case 0x0e:
        case 0x0f:
        case 0x10:
        case 0x11:
        case 0x30:
        case 0x31:
        case 0xff:
            if (media == 0)
            {
                int uASC = SCSI_ASC_NONE;

                switch (format)
                {
                    case 0x0: /* Physical format information */
                    {
                        int layer = pAhciReq->aATAPICmd[6];
                        uint64_t total_sectors;

                        if (layer != 0)
                        {
                            uASC = -SCSI_ASC_INV_FIELD_IN_CMD_PACKET;
                            break;
                        }

                        total_sectors = pAhciPort->cTotalSectors;
                        total_sectors >>= 2;
                        if (total_sectors == 0)
                        {
                            uASC = -SCSI_ASC_MEDIUM_NOT_PRESENT;
                            break;
                        }

                        buf[4] = 1;   /* DVD-ROM, part version 1 */
                        buf[5] = 0xf; /* 120mm disc, minimum rate unspecified */
                        buf[6] = 1;   /* one layer, read-only (per MMC-2 spec) */
                        buf[7] = 0;   /* default densities */

                        /* FIXME: 0x30000 per spec? */
                        ataH2BE_U32(buf + 8, 0); /* start sector */
                        ataH2BE_U32(buf + 12, total_sectors - 1); /* end sector */
                        ataH2BE_U32(buf + 16, total_sectors - 1); /* l0 end sector */

                        /* Size of buffer, not including 2 byte size field */
                        ataH2BE_U32(&buf[0], 2048 + 2);

                        /* 2k data + 4 byte header */
                        uASC = (2048 + 4);
                        break;
                    }
                    case 0x01: /* DVD copyright information */
                        buf[4] = 0; /* no copyright data */
                        buf[5] = 0; /* no region restrictions */

                        /* Size of buffer, not including 2 byte size field */
                        ataH2BE_U16(buf, 4 + 2);

                        /* 4 byte header + 4 byte data */
                        uASC = (4 + 4);
                        break;

                    case 0x03: /* BCA information - invalid field for no BCA info */
                        uASC = -SCSI_ASC_INV_FIELD_IN_CMD_PACKET;
                        break;

                    case 0x04: /* DVD disc manufacturing information */
                        /* Size of buffer, not including 2 byte size field */
                        ataH2BE_U16(buf, 2048 + 2);

                        /* 2k data + 4 byte header */
                        uASC = (2048 + 4);
                        break;
                    case 0xff:
                        /*
                         * This lists all the command capabilities above.  Add new ones
                         * in order and update the length and buffer return values.
                         */

                        buf[4] = 0x00; /* Physical format */
                        buf[5] = 0x40; /* Not writable, is readable */
                        ataH2BE_U16((buf + 6), 2048 + 4);

                        buf[8] = 0x01; /* Copyright info */
                        buf[9] = 0x40; /* Not writable, is readable */
                        ataH2BE_U16((buf + 10), 4 + 4);

                        buf[12] = 0x03; /* BCA info */
                        buf[13] = 0x40; /* Not writable, is readable */
                        ataH2BE_U16((buf + 14), 188 + 4);

                        buf[16] = 0x04; /* Manufacturing info */
                        buf[17] = 0x40; /* Not writable, is readable */
                        ataH2BE_U16((buf + 18), 2048 + 4);

                        /* Size of buffer, not including 2 byte size field */
                        ataH2BE_U16(buf, 16 + 2);

                        /* data written + 4 byte header */
                        uASC = (16 + 4);
                        break;
                    default: /* TODO: formats beyond DVD-ROM requires */
                        uASC = -SCSI_ASC_INV_FIELD_IN_CMD_PACKET;
                }

                if (uASC < 0)
                {
                    atapiCmdErrorSimple(pAhciPort, pAhciReq, SCSI_SENSE_ILLEGAL_REQUEST, -uASC);
                    return false;
                }
                break;
            }
            /* TODO: BD support, fall through for now */

        /* Generic disk structures */
        case 0x80: /* TODO: AACS volume identifier */
        case 0x81: /* TODO: AACS media serial number */
        case 0x82: /* TODO: AACS media identifier */
        case 0x83: /* TODO: AACS media key block */
        case 0x90: /* TODO: List of recognized format layers */
        case 0xc0: /* TODO: Write protection status */
        default:
            atapiCmdErrorSimple(pAhciPort, pAhciReq, SCSI_SENSE_ILLEGAL_REQUEST,
                                SCSI_ASC_INV_FIELD_IN_CMD_PACKET);
            return false;
    }

    /* Copy the buffer into the scatter gather list. */
    *pcbData = ahciCopyToPrdtl(pAhciPort->pDevInsR3, pAhciReq, (void *)&aBuf[0],
                               RT_MIN(cbData, max_len));

    atapiCmdOK(pAhciPort, pAhciReq);
    return false;
}

static int atapiDoTransfer(PAHCIPort pAhciPort, PAHCIREQ pAhciReq, size_t cbMax, ATAPIFN iSourceSink)
{
    size_t cbTransfered = 0;
    int rcSourceSink;

    rcSourceSink = g_apfnAtapiFuncs[iSourceSink](pAhciReq, pAhciPort, cbMax,
                                                 &cbTransfered);

    pAhciReq->cmdHdr.u32PRDBC = cbTransfered;
    pAhciReq->cbTransfer = cbTransfered;

    LogFlow(("cbTransfered=%d\n", cbTransfered));

    /* Write updated command header into memory of the guest. */
    PDMDevHlpPCIPhysWrite(pAhciPort->CTX_SUFF(pDevIns), pAhciReq->GCPhysCmdHdrAddr, &pAhciReq->cmdHdr, sizeof(CmdHdr));

    return rcSourceSink;
}

static DECLCALLBACK(int) atapiReadSectors2352PostProcess(PAHCIREQ pAhciReq, void **ppvProc, size_t *pcbProc)
{
    uint8_t *pbBuf = NULL;
    uint32_t cSectors  = pAhciReq->cbTransfer / 2048;
    uint32_t iATAPILBA = pAhciReq->uOffset / 2048;
    uint8_t *pbBufDst;
    uint8_t *pbBufSrc  = (uint8_t *)pAhciReq->u.Io.DataSeg.pvSeg;
    size_t cbAlloc = pAhciReq->cbTransfer + cSectors * (1 + 11 + 3 + 1 + 288); /* Per sector data like ECC. */

    pbBuf = (uint8_t *)RTMemAlloc(cbAlloc);
    if (RT_UNLIKELY(!pbBuf))
        return VERR_NO_MEMORY;

    pbBufDst = pbBuf;

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

    *ppvProc = pbBuf;
    *pcbProc = cbAlloc;

    return VINF_SUCCESS;
}

static int atapiReadSectors(PAHCIPort pAhciPort, PAHCIREQ pAhciReq, uint32_t iATAPILBA, uint32_t cSectors, uint32_t cbSector)
{
    Log(("%s: %d sectors at LBA %d\n", __FUNCTION__, cSectors, iATAPILBA));

    switch (cbSector)
    {
        case 2048:
            pAhciReq->uOffset = (uint64_t)iATAPILBA * cbSector;
            pAhciReq->cbTransfer = cSectors * cbSector;
            break;
        case 2352:
        {
            pAhciReq->u.Io.pfnPostProcess = atapiReadSectors2352PostProcess;
            pAhciReq->uOffset = (uint64_t)iATAPILBA * 2048;
            pAhciReq->cbTransfer = cSectors * 2048;
            break;
        }
        default:
            AssertMsgFailed(("Unsupported sectors size\n"));
            break;
    }

    return VINF_SUCCESS;
}

static AHCITXDIR atapiParseCmdVirtualATAPI(PAHCIPort pAhciPort, PAHCIREQ pAhciReq)
{
    AHCITXDIR enmTxDir = AHCITXDIR_NONE;
    const uint8_t *pbPacket;
    uint32_t cbMax;

    pbPacket = pAhciReq->aATAPICmd;

    ahciLog(("%s: ATAPI CMD=%#04x \"%s\"\n", __FUNCTION__, pbPacket[0], SCSICmdText(pbPacket[0])));

    switch (pbPacket[0])
    {
        case SCSI_TEST_UNIT_READY:
            if (pAhciPort->cNotifiedMediaChange > 0)
            {
                if (pAhciPort->cNotifiedMediaChange-- > 2)
                    atapiCmdErrorSimple(pAhciPort, pAhciReq, SCSI_SENSE_NOT_READY, SCSI_ASC_MEDIUM_NOT_PRESENT);
                else
                    atapiCmdErrorSimple(pAhciPort, pAhciReq, SCSI_SENSE_UNIT_ATTENTION, SCSI_ASC_MEDIUM_MAY_HAVE_CHANGED); /* media changed */
            }
            else if (pAhciPort->pDrvMount->pfnIsMounted(pAhciPort->pDrvMount))
                atapiCmdOK(pAhciPort, pAhciReq);
            else
                atapiCmdErrorSimple(pAhciPort, pAhciReq, SCSI_SENSE_NOT_READY, SCSI_ASC_MEDIUM_NOT_PRESENT);
            break;
        case SCSI_GET_EVENT_STATUS_NOTIFICATION:
            cbMax = ataBE2H_U16(pbPacket + 7);
            atapiDoTransfer(pAhciPort, pAhciReq, cbMax, ATAFN_SS_ATAPI_GET_EVENT_STATUS_NOTIFICATION);
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
                            atapiDoTransfer(pAhciPort, pAhciReq, cbMax, ATAFN_SS_ATAPI_MODE_SENSE_ERROR_RECOVERY);
                            break;
                        case SCSI_MODEPAGE_CD_STATUS:
                            atapiDoTransfer(pAhciPort, pAhciReq, cbMax, ATAFN_SS_ATAPI_MODE_SENSE_CD_STATUS);
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
                    atapiCmdErrorSimple(pAhciPort, pAhciReq, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_SAVING_PARAMETERS_NOT_SUPPORTED);
                    break;
            }
            break;
        }
        case SCSI_REQUEST_SENSE:
            cbMax = pbPacket[4];
            atapiDoTransfer(pAhciPort, pAhciReq, cbMax, ATAFN_SS_ATAPI_REQUEST_SENSE);
            break;
        case SCSI_PREVENT_ALLOW_MEDIUM_REMOVAL:
            if (pAhciPort->pDrvMount->pfnIsMounted(pAhciPort->pDrvMount))
            {
                if (pbPacket[4] & 1)
                    pAhciPort->pDrvMount->pfnLock(pAhciPort->pDrvMount);
                else
                    pAhciPort->pDrvMount->pfnUnlock(pAhciPort->pDrvMount);
                atapiCmdOK(pAhciPort, pAhciReq);
            }
            else
                atapiCmdErrorSimple(pAhciPort, pAhciReq, SCSI_SENSE_NOT_READY, SCSI_ASC_MEDIUM_NOT_PRESENT);
            break;
        case SCSI_READ_10:
        case SCSI_READ_12:
        {
            uint32_t cSectors, iATAPILBA;

            if (pAhciPort->cNotifiedMediaChange > 0)
            {
                pAhciPort->cNotifiedMediaChange-- ;
                atapiCmdErrorSimple(pAhciPort, pAhciReq, SCSI_SENSE_UNIT_ATTENTION, SCSI_ASC_MEDIUM_MAY_HAVE_CHANGED); /* media changed */
                break;
            }
            if (!pAhciPort->pDrvMount->pfnIsMounted(pAhciPort->pDrvMount))
            {
                atapiCmdErrorSimple(pAhciPort, pAhciReq, SCSI_SENSE_NOT_READY, SCSI_ASC_MEDIUM_NOT_PRESENT);
                break;
            }
            if (pbPacket[0] == SCSI_READ_10)
                cSectors = ataBE2H_U16(pbPacket + 7);
            else
                cSectors = ataBE2H_U32(pbPacket + 6);
            iATAPILBA = ataBE2H_U32(pbPacket + 2);
            if (cSectors == 0)
            {
                atapiCmdOK(pAhciPort, pAhciReq);
                break;
            }
            if ((uint64_t)iATAPILBA + cSectors > pAhciPort->cTotalSectors)
            {
                /* Rate limited logging, one log line per second. For
                 * guests that insist on reading from places outside the
                 * valid area this often generates too many release log
                 * entries otherwise. */
                static uint64_t s_uLastLogTS = 0;
                if (RTTimeMilliTS() >= s_uLastLogTS + 1000)
                {
                    LogRel(("AHCI ATAPI: LUN#%d: CD-ROM block number %Ld invalid (READ)\n", pAhciPort->iLUN, (uint64_t)iATAPILBA + cSectors));
                    s_uLastLogTS = RTTimeMilliTS();
                }
                atapiCmdErrorSimple(pAhciPort, pAhciReq, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_LOGICAL_BLOCK_OOR);
                break;
            }
            atapiReadSectors(pAhciPort, pAhciReq, iATAPILBA, cSectors, 2048);
            enmTxDir = AHCITXDIR_READ;
            break;
        }
        case SCSI_READ_CD:
        {
            uint32_t cSectors, iATAPILBA;

            if (pAhciPort->cNotifiedMediaChange > 0)
            {
                pAhciPort->cNotifiedMediaChange-- ;
                atapiCmdErrorSimple(pAhciPort, pAhciReq, SCSI_SENSE_UNIT_ATTENTION, SCSI_ASC_MEDIUM_MAY_HAVE_CHANGED); /* media changed */
                break;
            }
            else if (!pAhciPort->pDrvMount->pfnIsMounted(pAhciPort->pDrvMount))
            {
                atapiCmdErrorSimple(pAhciPort, pAhciReq, SCSI_SENSE_NOT_READY, SCSI_ASC_MEDIUM_NOT_PRESENT);
                break;
            }
            cSectors = (pbPacket[6] << 16) | (pbPacket[7] << 8) | pbPacket[8];
            iATAPILBA = ataBE2H_U32(pbPacket + 2);
            if (cSectors == 0)
            {
                atapiCmdOK(pAhciPort, pAhciReq);
                break;
            }
            if ((uint64_t)iATAPILBA + cSectors > pAhciPort->cTotalSectors)
            {
                /* Rate limited logging, one log line per second. For
                 * guests that insist on reading from places outside the
                 * valid area this often generates too many release log
                 * entries otherwise. */
                static uint64_t s_uLastLogTS = 0;
                if (RTTimeMilliTS() >= s_uLastLogTS + 1000)
                {
                    LogRel(("AHCI ATA: LUN#%d: CD-ROM block number %Ld invalid (READ CD)\n", pAhciPort->iLUN, (uint64_t)iATAPILBA + cSectors));
                    s_uLastLogTS = RTTimeMilliTS();
                }
                atapiCmdErrorSimple(pAhciPort, pAhciReq, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_LOGICAL_BLOCK_OOR);
                break;
            }
            switch (pbPacket[9] & 0xf8)
            {
                case 0x00:
                    /* nothing */
                    atapiCmdOK(pAhciPort, pAhciReq);
                    break;
                case 0x10:
                    /* normal read */
                    atapiReadSectors(pAhciPort, pAhciReq, iATAPILBA, cSectors, 2048);
                    enmTxDir = AHCITXDIR_READ;
                    break;
                case 0xf8:
                    /* read all data */
                    atapiReadSectors(pAhciPort, pAhciReq, iATAPILBA, cSectors, 2352);
                    enmTxDir = AHCITXDIR_READ;
                    break;
                default:
                    LogRel(("AHCI ATAPI: LUN#%d: CD-ROM sector format not supported\n", pAhciPort->iLUN));
                    atapiCmdErrorSimple(pAhciPort, pAhciReq, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_INV_FIELD_IN_CMD_PACKET);
                    break;
            }
            break;
        }
        case SCSI_SEEK_10:
        {
            uint32_t iATAPILBA;
            if (pAhciPort->cNotifiedMediaChange > 0)
            {
                pAhciPort->cNotifiedMediaChange-- ;
                atapiCmdErrorSimple(pAhciPort, pAhciReq, SCSI_SENSE_UNIT_ATTENTION, SCSI_ASC_MEDIUM_MAY_HAVE_CHANGED); /* media changed */
                break;
            }
            else if (!pAhciPort->pDrvMount->pfnIsMounted(pAhciPort->pDrvMount))
            {
                atapiCmdErrorSimple(pAhciPort, pAhciReq, SCSI_SENSE_NOT_READY, SCSI_ASC_MEDIUM_NOT_PRESENT);
                break;
            }
            iATAPILBA = ataBE2H_U32(pbPacket + 2);
            if (iATAPILBA > pAhciPort->cTotalSectors)
            {
                /* Rate limited logging, one log line per second. For
                 * guests that insist on seeking to places outside the
                 * valid area this often generates too many release log
                 * entries otherwise. */
                static uint64_t s_uLastLogTS = 0;
                if (RTTimeMilliTS() >= s_uLastLogTS + 1000)
                {
                    LogRel(("AHCI ATAPI: LUN#%d: CD-ROM block number %Ld invalid (SEEK)\n", pAhciPort->iLUN, (uint64_t)iATAPILBA));
                    s_uLastLogTS = RTTimeMilliTS();
                }
                atapiCmdErrorSimple(pAhciPort, pAhciReq, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_LOGICAL_BLOCK_OOR);
                break;
            }
            atapiCmdOK(pAhciPort, pAhciReq);
            pAhciReq->uATARegStatus |= ATA_STAT_SEEK; /* Linux expects this. */
            break;
        }
        case SCSI_START_STOP_UNIT:
        {
            int rc = VINF_SUCCESS;
            switch (pbPacket[4] & 3)
            {
                case 0: /* 00 - Stop motor */
                case 1: /* 01 - Start motor */
                    break;
                case 2: /* 10 - Eject media */
                {
                    /* This must be done from EMT. */
                    PAHCI pAhci = pAhciPort->CTX_SUFF(pAhci);
                    PPDMDEVINS pDevIns = pAhci->CTX_SUFF(pDevIns);

                    /*
                     * Free the I/O memory of all cached tasks of this port
                     * because the driver providing I/O memory allocation interface
                     * is about to be destroyed.
                     */
                    ahciR3PortCachedReqsFree(pAhciPort);

                    /*
                     * Also make sure that the current request has no memory allocated
                     * from the driver below us. We don't require it here anyway.
                     */
                    ahciReqMemFree(pAhciPort, pAhciReq, true /* fForceFree */);

                    rc = VMR3ReqPriorityCallWait(PDMDevHlpGetVM(pDevIns), VMCPUID_ANY,
                                                 (PFNRT)pAhciPort->pDrvMount->pfnUnmount, 3,
                                                 pAhciPort->pDrvMount, false/*=fForce*/, true/*=fEject*/);
                    Assert(RT_SUCCESS(rc) || rc == VERR_PDM_MEDIA_LOCKED || rc == VERR_PDM_MEDIA_NOT_MOUNTED);
                    if (RT_SUCCESS(rc) && pAhci->pMediaNotify)
                    {
                        rc = VMR3ReqCallNoWait(PDMDevHlpGetVM(pDevIns), VMCPUID_ANY,
                                               (PFNRT)pAhci->pMediaNotify->pfnEjected, 2,
                                               pAhci->pMediaNotify, pAhciPort->iLUN);
                        AssertRC(rc);
                    }
                    break;
                }
                case 3: /* 11 - Load media */
                    /** @todo rc = s->pDrvMount->pfnLoadMedia(s->pDrvMount) */
                    break;
            }
            if (RT_SUCCESS(rc))
                atapiCmdOK(pAhciPort, pAhciReq);
            else
                atapiCmdErrorSimple(pAhciPort, pAhciReq, SCSI_SENSE_NOT_READY, SCSI_ASC_MEDIA_LOAD_OR_EJECT_FAILED);
            break;
        }
        case SCSI_MECHANISM_STATUS:
        {
            cbMax = ataBE2H_U16(pbPacket + 8);
            atapiDoTransfer(pAhciPort, pAhciReq, cbMax, ATAFN_SS_ATAPI_MECHANISM_STATUS);
            break;
        }
        case SCSI_READ_TOC_PMA_ATIP:
        {
            uint8_t format;

            if (pAhciPort->cNotifiedMediaChange > 0)
            {
                pAhciPort->cNotifiedMediaChange-- ;
                atapiCmdErrorSimple(pAhciPort, pAhciReq, SCSI_SENSE_UNIT_ATTENTION, SCSI_ASC_MEDIUM_MAY_HAVE_CHANGED); /* media changed */
                break;
            }
            else if (!pAhciPort->pDrvMount->pfnIsMounted(pAhciPort->pDrvMount))
            {
                atapiCmdErrorSimple(pAhciPort, pAhciReq, SCSI_SENSE_NOT_READY, SCSI_ASC_MEDIUM_NOT_PRESENT);
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
                    atapiDoTransfer(pAhciPort, pAhciReq, cbMax, ATAFN_SS_ATAPI_READ_TOC_NORMAL);
                    break;
                case 1:
                    atapiDoTransfer(pAhciPort, pAhciReq, cbMax, ATAFN_SS_ATAPI_READ_TOC_MULTI);
                    break;
                case 2:
                    atapiDoTransfer(pAhciPort, pAhciReq, cbMax, ATAFN_SS_ATAPI_READ_TOC_RAW);
                    break;
                default:
                  error_cmd:
                    atapiCmdErrorSimple(pAhciPort, pAhciReq, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_INV_FIELD_IN_CMD_PACKET);
                    break;
            }
            break;
        }
        case SCSI_READ_CAPACITY:
            if (pAhciPort->cNotifiedMediaChange > 0)
            {
                pAhciPort->cNotifiedMediaChange-- ;
                atapiCmdErrorSimple(pAhciPort, pAhciReq, SCSI_SENSE_UNIT_ATTENTION, SCSI_ASC_MEDIUM_MAY_HAVE_CHANGED); /* media changed */
                break;
            }
            else if (!pAhciPort->pDrvMount->pfnIsMounted(pAhciPort->pDrvMount))
            {
                atapiCmdErrorSimple(pAhciPort, pAhciReq, SCSI_SENSE_NOT_READY, SCSI_ASC_MEDIUM_NOT_PRESENT);
                break;
            }
            atapiDoTransfer(pAhciPort, pAhciReq, 8 /* cbMax */, ATAFN_SS_ATAPI_READ_CAPACITY);
            break;
        case SCSI_READ_DISC_INFORMATION:
            if (pAhciPort->cNotifiedMediaChange > 0)
            {
                pAhciPort->cNotifiedMediaChange-- ;
                atapiCmdErrorSimple(pAhciPort, pAhciReq, SCSI_SENSE_UNIT_ATTENTION, SCSI_ASC_MEDIUM_MAY_HAVE_CHANGED); /* media changed */
                break;
            }
            else if (!pAhciPort->pDrvMount->pfnIsMounted(pAhciPort->pDrvMount))
            {
                atapiCmdErrorSimple(pAhciPort, pAhciReq, SCSI_SENSE_NOT_READY, SCSI_ASC_MEDIUM_NOT_PRESENT);
                break;
            }
            cbMax = ataBE2H_U16(pbPacket + 7);
            atapiDoTransfer(pAhciPort, pAhciReq, cbMax, ATAFN_SS_ATAPI_READ_DISC_INFORMATION);
            break;
        case SCSI_READ_TRACK_INFORMATION:
            if (pAhciPort->cNotifiedMediaChange > 0)
            {
                pAhciPort->cNotifiedMediaChange-- ;
                atapiCmdErrorSimple(pAhciPort, pAhciReq, SCSI_SENSE_UNIT_ATTENTION, SCSI_ASC_MEDIUM_MAY_HAVE_CHANGED); /* media changed */
                break;
            }
            else if (!pAhciPort->pDrvMount->pfnIsMounted(pAhciPort->pDrvMount))
            {
                atapiCmdErrorSimple(pAhciPort, pAhciReq, SCSI_SENSE_NOT_READY, SCSI_ASC_MEDIUM_NOT_PRESENT);
                break;
            }
            cbMax = ataBE2H_U16(pbPacket + 7);
            atapiDoTransfer(pAhciPort, pAhciReq, cbMax, ATAFN_SS_ATAPI_READ_TRACK_INFORMATION);
            break;
        case SCSI_GET_CONFIGURATION:
            /* No media change stuff here, it can confuse Linux guests. */
            cbMax = ataBE2H_U16(pbPacket + 7);
            atapiDoTransfer(pAhciPort, pAhciReq, cbMax, ATAFN_SS_ATAPI_GET_CONFIGURATION);
            break;
        case SCSI_INQUIRY:
            cbMax = pbPacket[4];
            atapiDoTransfer(pAhciPort, pAhciReq, cbMax, ATAFN_SS_ATAPI_INQUIRY);
            break;
        case SCSI_READ_DVD_STRUCTURE:
            cbMax = ataBE2H_U16(pbPacket + 8);
            atapiDoTransfer(pAhciPort, pAhciReq, cbMax, ATAFN_SS_ATAPI_READ_DVD_STRUCTURE);
            break;
        default:
            atapiCmdErrorSimple(pAhciPort, pAhciReq, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_ILLEGAL_OPCODE);
            break;
    }

    return enmTxDir;
}

/*
 * Parse ATAPI commands, passing them directly to the CD/DVD drive.
 */
static AHCITXDIR atapiParseCmdPassthrough(PAHCIPort pAhciPort, PAHCIREQ pAhciReq)
{
    const uint8_t *pbPacket;
    uint32_t cSectors, iATAPILBA;
    uint32_t cbTransfer = 0;
    AHCITXDIR enmTxDir = AHCITXDIR_NONE;

    pbPacket = pAhciReq->aATAPICmd;
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
            enmTxDir = AHCITXDIR_WRITE;
            goto sendcmd;
        case SCSI_FORMAT_UNIT:
            cbTransfer = pAhciReq->cmdFis[AHCI_CMDFIS_CYLL] | (pAhciReq->cmdFis[AHCI_CMDFIS_CYLH] << 8); /* use ATAPI transfer length */
            enmTxDir = AHCITXDIR_WRITE;
            goto sendcmd;
        case SCSI_GET_CONFIGURATION:
            cbTransfer = ataBE2H_U16(pbPacket + 7);
            enmTxDir = AHCITXDIR_READ;
            goto sendcmd;
        case SCSI_GET_EVENT_STATUS_NOTIFICATION:
            cbTransfer = ataBE2H_U16(pbPacket + 7);
            if (ASMAtomicReadU32(&pAhciPort->MediaEventStatus) != ATA_EVENT_STATUS_UNCHANGED)
            {
                pAhciReq->cbTransfer = RT_MIN(cbTransfer, 8);
                atapiDoTransfer(pAhciPort, pAhciReq, pAhciReq->cbTransfer, ATAFN_SS_ATAPI_GET_EVENT_STATUS_NOTIFICATION);
                break;
            }
            enmTxDir = AHCITXDIR_READ;
            goto sendcmd;
        case SCSI_GET_PERFORMANCE:
            cbTransfer = pAhciReq->cmdFis[AHCI_CMDFIS_CYLL] | (pAhciReq->cmdFis[AHCI_CMDFIS_CYLH] << 8); /* use ATAPI transfer length */
            enmTxDir = AHCITXDIR_READ;
            goto sendcmd;
        case SCSI_INQUIRY:
            cbTransfer = ataBE2H_U16(pbPacket + 3);
            enmTxDir = AHCITXDIR_READ;
            goto sendcmd;
        case SCSI_LOAD_UNLOAD_MEDIUM:
            goto sendcmd;
        case SCSI_MECHANISM_STATUS:
            cbTransfer = ataBE2H_U16(pbPacket + 8);
            enmTxDir = AHCITXDIR_READ;
            goto sendcmd;
        case SCSI_MODE_SELECT_10:
            cbTransfer = ataBE2H_U16(pbPacket + 7);
            enmTxDir = AHCITXDIR_WRITE;
            goto sendcmd;
        case SCSI_MODE_SENSE_10:
            cbTransfer = ataBE2H_U16(pbPacket + 7);
            enmTxDir = AHCITXDIR_READ;
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
            pAhciReq->cbATAPISector = 2048; /**< @todo this size is not always correct */
            cbTransfer = cSectors * pAhciReq->cbATAPISector;
            enmTxDir = AHCITXDIR_READ;
            goto sendcmd;
        case SCSI_READ_12:
            iATAPILBA = ataBE2H_U32(pbPacket + 2);
            cSectors = ataBE2H_U32(pbPacket + 6);
            Log2(("ATAPI PT: lba %d sectors %d\n", iATAPILBA, cSectors));
            pAhciReq->cbATAPISector = 2048; /**< @todo this size is not always correct */
            cbTransfer = cSectors * pAhciReq->cbATAPISector;
            enmTxDir = AHCITXDIR_READ;
            goto sendcmd;
        case SCSI_READ_BUFFER:
            cbTransfer = ataBE2H_U24(pbPacket + 6);
            enmTxDir = AHCITXDIR_READ;
            goto sendcmd;
        case SCSI_READ_BUFFER_CAPACITY:
            cbTransfer = ataBE2H_U16(pbPacket + 7);
            enmTxDir = AHCITXDIR_READ;
            goto sendcmd;
        case SCSI_READ_CAPACITY:
            cbTransfer = 8;
            enmTxDir = AHCITXDIR_READ;
            goto sendcmd;
        case SCSI_READ_CD:
        case SCSI_READ_CD_MSF:
        {
            /* Get sector size based on the expected sector type field. */
            switch ((pbPacket[1] >> 2) & 0x7)
            {
                case 0x0: /* All types. */
                {
                    uint32_t iLbaStart;

                    if (pbPacket[0] == SCSI_READ_CD)
                        iLbaStart = ataBE2H_U32(&pbPacket[2]);
                    else
                        iLbaStart = ataMSF2LBA(&pbPacket[3]);

                    if (pAhciPort->pTrackList)
                        pAhciReq->cbATAPISector = ATAPIPassthroughTrackListGetSectorSizeFromLba(pAhciPort->pTrackList, iLbaStart);
                    else
                        pAhciReq->cbATAPISector = 2048; /* Might be incorrect if we couldn't determine the type. */
                    break;
                }
                case 0x1: /* CD-DA */
                    pAhciReq->cbATAPISector = 2352;
                    break;
                case 0x2: /* Mode 1 */
                    pAhciReq->cbATAPISector = 2048;
                    break;
                case 0x3: /* Mode 2 formless */
                    pAhciReq->cbATAPISector = 2336;
                    break;
                case 0x4: /* Mode 2 form 1 */
                    pAhciReq->cbATAPISector = 2048;
                    break;
                case 0x5: /* Mode 2 form 2 */
                    pAhciReq->cbATAPISector = 2324;
                    break;
                default: /* Reserved */
                    AssertMsgFailed(("Unknown sector type\n"));
                    pAhciReq->cbATAPISector = 0; /** @todo we should probably fail the command here already. */
            }

            if (pbPacket[0] == SCSI_READ_CD)
                cbTransfer = ataBE2H_U24(pbPacket + 6) * pAhciReq->cbATAPISector;
            else /* SCSI_READ_MSF */
            {
                cSectors = ataMSF2LBA(pbPacket + 6) - ataMSF2LBA(pbPacket + 3);
                if (cSectors > 32)
                    cSectors = 32; /* Limit transfer size to 64~74K. Safety first. In any case this can only harm software doing CDDA extraction. */
                cbTransfer = cSectors * pAhciReq->cbATAPISector;
            }
            enmTxDir = AHCITXDIR_READ;
            goto sendcmd;
        }
        case SCSI_READ_DISC_INFORMATION:
            cbTransfer = ataBE2H_U16(pbPacket + 7);
            enmTxDir = AHCITXDIR_READ;
            goto sendcmd;
        case SCSI_READ_DVD_STRUCTURE:
            cbTransfer = ataBE2H_U16(pbPacket + 8);
            enmTxDir = AHCITXDIR_READ;
            goto sendcmd;
        case SCSI_READ_FORMAT_CAPACITIES:
            cbTransfer = ataBE2H_U16(pbPacket + 7);
            enmTxDir = AHCITXDIR_READ;
            goto sendcmd;
        case SCSI_READ_SUBCHANNEL:
            cbTransfer = ataBE2H_U16(pbPacket + 7);
            enmTxDir = AHCITXDIR_READ;
            goto sendcmd;
        case SCSI_READ_TOC_PMA_ATIP:
            cbTransfer = ataBE2H_U16(pbPacket + 7);
            enmTxDir = AHCITXDIR_READ;
            goto sendcmd;
        case SCSI_READ_TRACK_INFORMATION:
            cbTransfer = ataBE2H_U16(pbPacket + 7);
            enmTxDir = AHCITXDIR_READ;
            goto sendcmd;
        case SCSI_REPAIR_TRACK:
            goto sendcmd;
        case SCSI_REPORT_KEY:
            cbTransfer = ataBE2H_U16(pbPacket + 8);
            enmTxDir = AHCITXDIR_READ;
            goto sendcmd;
        case SCSI_REQUEST_SENSE:
            cbTransfer = pbPacket[4];
            if ((pAhciPort->abATAPISense[2] & 0x0f) != SCSI_SENSE_NONE)
            {
                pAhciReq->cbTransfer = cbTransfer;
                pAhciReq->enmTxDir = AHCITXDIR_READ;
                atapiDoTransfer(pAhciPort, pAhciReq, cbTransfer, ATAFN_SS_ATAPI_REQUEST_SENSE);
                break;
            }
            enmTxDir = AHCITXDIR_READ;
            goto sendcmd;
        case SCSI_RESERVE_TRACK:
            goto sendcmd;
        case SCSI_SCAN:
            goto sendcmd;
        case SCSI_SEEK_10:
            goto sendcmd;
        case SCSI_SEND_CUE_SHEET:
            cbTransfer = ataBE2H_U24(pbPacket + 6);
            enmTxDir = AHCITXDIR_WRITE;
            goto sendcmd;
        case SCSI_SEND_DVD_STRUCTURE:
            cbTransfer = ataBE2H_U16(pbPacket + 8);
            enmTxDir = AHCITXDIR_WRITE;
            goto sendcmd;
        case SCSI_SEND_EVENT:
            cbTransfer = ataBE2H_U16(pbPacket + 8);
            enmTxDir = AHCITXDIR_WRITE;
            goto sendcmd;
        case SCSI_SEND_KEY:
            cbTransfer = ataBE2H_U16(pbPacket + 8);
            enmTxDir = AHCITXDIR_WRITE;
            goto sendcmd;
        case SCSI_SEND_OPC_INFORMATION:
            cbTransfer = ataBE2H_U16(pbPacket + 7);
            enmTxDir = AHCITXDIR_WRITE;
            goto sendcmd;
        case SCSI_SET_CD_SPEED:
            goto sendcmd;
        case SCSI_SET_READ_AHEAD:
            goto sendcmd;
        case SCSI_SET_STREAMING:
            cbTransfer = ataBE2H_U16(pbPacket + 9);
            enmTxDir = AHCITXDIR_WRITE;
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
        case SCSI_WRITE_AND_VERIFY_10:
            iATAPILBA = ataBE2H_U32(pbPacket + 2);
            cSectors = ataBE2H_U16(pbPacket + 7);
            Log2(("ATAPI PT: lba %d sectors %d\n", iATAPILBA, cSectors));
            if (pAhciPort->pTrackList)
                pAhciReq->cbATAPISector = ATAPIPassthroughTrackListGetSectorSizeFromLba(pAhciPort->pTrackList, iATAPILBA);
            else
                pAhciReq->cbATAPISector = 2048;
            cbTransfer = cSectors * pAhciReq->cbATAPISector;
            enmTxDir = AHCITXDIR_WRITE;
            goto sendcmd;
        case SCSI_WRITE_12:
            iATAPILBA = ataBE2H_U32(pbPacket + 2);
            cSectors = ataBE2H_U32(pbPacket + 6);
            Log2(("ATAPI PT: lba %d sectors %d\n", iATAPILBA, cSectors));
            if (pAhciPort->pTrackList)
                pAhciReq->cbATAPISector = ATAPIPassthroughTrackListGetSectorSizeFromLba(pAhciPort->pTrackList, iATAPILBA);
            else
                pAhciReq->cbATAPISector = 2048;
            cbTransfer = cSectors * pAhciReq->cbATAPISector;
            enmTxDir = AHCITXDIR_WRITE;
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
                    LogRel(("PIIX3 ATA: LUN#%d: CD-ROM passthrough command attempted to update firmware, blocked\n", pAhciPort->iLUN));
                    atapiCmdErrorSimple(pAhciPort, pAhciReq, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_INV_FIELD_IN_CMD_PACKET);
                    break;
                default:
                    cbTransfer = ataBE2H_U16(pbPacket + 6);
                    enmTxDir = AHCITXDIR_WRITE;
                    goto sendcmd;
            }
            break;
        case SCSI_REPORT_LUNS: /* Not part of MMC-3, but used by Windows. */
            cbTransfer = ataBE2H_U32(pbPacket + 6);
            enmTxDir = AHCITXDIR_READ;
            goto sendcmd;
        case SCSI_REZERO_UNIT:
            /* Obsolete command used by cdrecord. What else would one expect?
             * This command is not sent to the drive, it is handled internally,
             * as the Linux kernel doesn't like it (message "scsi: unknown
             * opcode 0x01" in syslog) and replies with a sense code of 0,
             * which sends cdrecord to an endless loop. */
            atapiCmdErrorSimple(pAhciPort, pAhciReq, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_ILLEGAL_OPCODE);
            break;
        default:
            LogRel(("AHCI: LUN#%d: passthrough unimplemented for command %#x\n", pAhciPort->iLUN, pbPacket[0]));
            atapiCmdErrorSimple(pAhciPort, pAhciReq, SCSI_SENSE_ILLEGAL_REQUEST, SCSI_ASC_ILLEGAL_OPCODE);
            break;
        sendcmd:
            /* Send a command to the drive, passing data in/out as required. */
            Log2(("ATAPI PT: max size %d\n", cbTransfer));
            if (cbTransfer == 0)
                enmTxDir = AHCITXDIR_NONE;
            pAhciReq->enmTxDir = enmTxDir;
            pAhciReq->cbTransfer = cbTransfer;
            atapiDoTransfer(pAhciPort, pAhciReq, cbTransfer, ATAFN_SS_ATAPI_PASSTHROUGH);
    }

    return AHCITXDIR_NONE;
}

static AHCITXDIR atapiParseCmd(PAHCIPort pAhciPort, PAHCIREQ pAhciReq)
{
    AHCITXDIR enmTxDir = AHCITXDIR_NONE;
    const uint8_t *pbPacket;

    pbPacket = pAhciReq->aATAPICmd;
    Log(("%s: LUN#%d CMD=%#04x \"%s\"\n", __FUNCTION__, pAhciPort->iLUN, pbPacket[0], SCSICmdText(pbPacket[0])));
    Log2(("%s: limit=%#x packet: %.*Rhxs\n", __FUNCTION__, pAhciReq->cmdFis[AHCI_CMDFIS_CYLL] | (pAhciReq->cmdFis[AHCI_CMDFIS_CYLH] << 8), ATAPI_PACKET_SIZE, pbPacket));

    if (pAhciPort->fATAPIPassthrough)
        enmTxDir = atapiParseCmdPassthrough(pAhciPort, pAhciReq);
    else
        enmTxDir = atapiParseCmdVirtualATAPI(pAhciPort, pAhciReq);

    return enmTxDir;
}

/**
 * Reset all values after a reset of the attached storage device.
 *
 * @returns nothing
 * @param pAhciPort             The port the device is attached to.
 * @param pAhciReq    The state to get the tag number from.
 */
static void ahciFinishStorageDeviceReset(PAHCIPort pAhciPort, PAHCIREQ pAhciReq)
{
    int rc;

    /* Send a status good D2H FIS. */
    ASMAtomicWriteU32(&pAhciPort->MediaEventStatus, ATA_EVENT_STATUS_UNCHANGED);
    pAhciPort->fResetDevice = false;
    if (pAhciPort->regCMD & AHCI_PORT_CMD_FRE)
        ahciPostFirstD2HFisIntoMemory(pAhciPort);

    /* As this is the first D2H FIS after the reset update the signature in the SIG register of the port. */
    if (pAhciPort->fATAPI)
        pAhciPort->regSIG = AHCI_PORT_SIG_ATAPI;
    else
        pAhciPort->regSIG = AHCI_PORT_SIG_DISK;
    ASMAtomicOrU32(&pAhciPort->u32TasksFinished, (1 << pAhciReq->uTag));

    rc = ahciHbaSetInterrupt(pAhciPort->CTX_SUFF(pAhci), pAhciPort->iLUN, VERR_IGNORED);
    AssertRC(rc);
}

/**
 * Initiates a device reset caused by ATA_DEVICE_RESET (ATAPI only).
 *
 * @returns nothing.
 * @param   pAhciPort          The device to reset.
 * @param   pAhciReq The task state.
 */
static void ahciDeviceReset(PAHCIPort pAhciPort, PAHCIREQ pAhciReq)
{
    ASMAtomicWriteBool(&pAhciPort->fResetDevice, true);

    /*
     * Because this ATAPI only and ATAPI can't have
     * more than one command active at a time the task counter should be 0
     * and it is possible to finish the reset now.
     */
    Assert(ASMAtomicReadU32(&pAhciPort->cTasksActive) == 0);
    ahciFinishStorageDeviceReset(pAhciPort, pAhciReq);
}

/**
 * Create a PIO setup FIS and post it into the memory area of the guest.
 *
 * @returns nothing.
 * @param   pAhciPort          The port of the SATA controller.
 * @param   pAhciReq           The state of the task.
 * @param   pCmdFis            Pointer to the command FIS from the guest.
 * @param   fInterrupt         If an interrupt should be send to the guest.
 */
static void ahciSendPioSetupFis(PAHCIPort pAhciPort, PAHCIREQ pAhciReq, uint8_t *pCmdFis,
                                bool fInterrupt)
{
    uint8_t abPioSetupFis[20];
    bool fAssertIntr = false;
    PAHCI pAhci = pAhciPort->CTX_SUFF(pAhci);

    ahciLog(("%s: building PIO setup Fis\n", __FUNCTION__));

    AssertMsg(   pAhciReq->cbTransfer > 0
              && pAhciReq->cbTransfer <= 65534,
              ("Can't send PIO setup FIS for requests with 0 bytes to transfer or greater than 65534\n"));

    if (pAhciPort->regCMD & AHCI_PORT_CMD_FRE)
    {
        memset(&abPioSetupFis[0], 0, sizeof(abPioSetupFis));
        abPioSetupFis[AHCI_CMDFIS_TYPE]  = AHCI_CMDFIS_TYPE_PIOSETUP;
        abPioSetupFis[AHCI_CMDFIS_BITS]  = (fInterrupt ? AHCI_CMDFIS_I : 0);
        if (pAhciReq->enmTxDir == AHCITXDIR_READ)
            abPioSetupFis[AHCI_CMDFIS_BITS] |= AHCI_CMDFIS_D;
        abPioSetupFis[AHCI_CMDFIS_STS]   = pAhciReq->uATARegStatus;
        abPioSetupFis[AHCI_CMDFIS_ERR]   = pAhciReq->uATARegError;
        abPioSetupFis[AHCI_CMDFIS_SECTN] = pCmdFis[AHCI_CMDFIS_SECTN];
        abPioSetupFis[AHCI_CMDFIS_CYLL]  = pCmdFis[AHCI_CMDFIS_CYLL];
        abPioSetupFis[AHCI_CMDFIS_CYLH]  = pCmdFis[AHCI_CMDFIS_CYLH];
        abPioSetupFis[AHCI_CMDFIS_HEAD]  = pCmdFis[AHCI_CMDFIS_HEAD];
        abPioSetupFis[AHCI_CMDFIS_SECTNEXP] = pCmdFis[AHCI_CMDFIS_SECTNEXP];
        abPioSetupFis[AHCI_CMDFIS_CYLLEXP]  = pCmdFis[AHCI_CMDFIS_CYLLEXP];
        abPioSetupFis[AHCI_CMDFIS_CYLHEXP]  = pCmdFis[AHCI_CMDFIS_CYLHEXP];
        abPioSetupFis[AHCI_CMDFIS_SECTC]    = pCmdFis[AHCI_CMDFIS_SECTC];
        abPioSetupFis[AHCI_CMDFIS_SECTCEXP] = pCmdFis[AHCI_CMDFIS_SECTCEXP];

        /* Set transfer count. */
        abPioSetupFis[16] = (pAhciReq->cbTransfer >> 8) & 0xff;
        abPioSetupFis[17] = pAhciReq->cbTransfer & 0xff;

        /* Update registers. */
        pAhciPort->regTFD = (pAhciReq->uATARegError << 8) | pAhciReq->uATARegStatus;

        ahciPostFisIntoMemory(pAhciPort, AHCI_CMDFIS_TYPE_PIOSETUP, abPioSetupFis);

        if (fInterrupt)
        {
            ASMAtomicOrU32(&pAhciPort->regIS, AHCI_PORT_IS_PSS);
            /* Check if we should assert an interrupt */
            if (pAhciPort->regIE & AHCI_PORT_IE_PSE)
                fAssertIntr = true;
        }

        if (fAssertIntr)
        {
            int rc = ahciHbaSetInterrupt(pAhci, pAhciPort->iLUN, VERR_IGNORED);
            AssertRC(rc);
        }
    }
}

/**
 * Build a D2H FIS and post into the memory area of the guest.
 *
 * @returns Nothing
 * @param   pAhciPort          The port of the SATA controller.
 * @param   pAhciReq           The state of the task.
 * @param   pCmdFis            Pointer to the command FIS from the guest.
 * @param   fInterrupt         If an interrupt should be send to the guest.
 */
static void ahciSendD2HFis(PAHCIPort pAhciPort, PAHCIREQ pAhciReq, uint8_t *pCmdFis, bool fInterrupt)
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
        d2hFis[AHCI_CMDFIS_STS]   = pAhciReq->uATARegStatus;
        d2hFis[AHCI_CMDFIS_ERR]   = pAhciReq->uATARegError;
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
        pAhciPort->regTFD = (pAhciReq->uATARegError << 8) | pAhciReq->uATARegStatus;

        ahciPostFisIntoMemory(pAhciPort, AHCI_CMDFIS_TYPE_D2H, d2hFis);

        if (pAhciReq->uATARegStatus & ATA_STAT_ERR)
        {
            /* Error bit is set. */
            ASMAtomicOrU32(&pAhciPort->regIS, AHCI_PORT_IS_TFES);
            if (pAhciPort->regIE & AHCI_PORT_IE_TFEE)
                fAssertIntr = true;
            /*
             * Don't mark the command slot as completed because the guest
             * needs it to identify the failed command.
             */
        }
        else if (fInterrupt)
        {
            ASMAtomicOrU32(&pAhciPort->regIS, AHCI_PORT_IS_DHRS);
            /* Check if we should assert an interrupt */
            if (pAhciPort->regIE & AHCI_PORT_IE_DHRE)
                fAssertIntr = true;

            /* Mark command as completed. */
            ASMAtomicOrU32(&pAhciPort->u32TasksFinished, (1 << pAhciReq->uTag));
        }

        if (fAssertIntr)
        {
            int rc = ahciHbaSetInterrupt(pAhci, pAhciPort->iLUN, VERR_IGNORED);
            AssertRC(rc);
        }
    }
}

/**
 * Build a SDB Fis and post it into the memory area of the guest.
 *
 * @returns Nothing
 * @param   pAhciPort           The port for which the SDB Fis is send.
 * @param   uFinishedTasks      Bitmask of finished tasks.
 * @param   fInterrupt          If an interrupt should be asserted.
 */
static void ahciSendSDBFis(PAHCIPort pAhciPort, uint32_t uFinishedTasks, bool fInterrupt)
{
    uint32_t sdbFis[2];
    bool fAssertIntr = false;
    PAHCI pAhci = pAhciPort->CTX_SUFF(pAhci);
    PAHCIREQ pTaskErr = ASMAtomicReadPtrT(&pAhciPort->pTaskErr, PAHCIREQ);

    ahciLog(("%s: Building SDB FIS\n", __FUNCTION__));

    if (pAhciPort->regCMD & AHCI_PORT_CMD_FRE)
    {
        memset(&sdbFis[0], 0, sizeof(sdbFis));
        sdbFis[0] = AHCI_CMDFIS_TYPE_SETDEVBITS;
        sdbFis[0] |= (fInterrupt ? (1 << 14) : 0);
        if (RT_UNLIKELY(pTaskErr))
        {
            sdbFis[0]  = pTaskErr->uATARegError;
            sdbFis[0] |= (pTaskErr->uATARegStatus & 0x77) << 16; /* Some bits are marked as reserved and thus are masked out. */

            /* Update registers. */
            pAhciPort->regTFD = (pTaskErr->uATARegError << 8) | pTaskErr->uATARegStatus;
        }
        else
        {
            sdbFis[0]  = 0;
            sdbFis[0] |= (ATA_STAT_READY | ATA_STAT_SEEK) << 16;
            pAhciPort->regTFD = ATA_STAT_READY | ATA_STAT_SEEK;
        }

        sdbFis[1] = pAhciPort->u32QueuedTasksFinished | uFinishedTasks;

        ahciPostFisIntoMemory(pAhciPort, AHCI_CMDFIS_TYPE_SETDEVBITS, (uint8_t *)sdbFis);

        if (RT_UNLIKELY(pTaskErr))
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

        ASMAtomicOrU32(&pAhciPort->u32QueuedTasksFinished, uFinishedTasks);

        if (fAssertIntr)
        {
            int rc = ahciHbaSetInterrupt(pAhci, pAhciPort->iLUN, VERR_IGNORED);
            AssertRC(rc);
        }
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

/**
 * Allocates memory for the given request using already allocated memory if possible.
 *
 * @returns Pointer to the memory or NULL on failure
 * @param   pAhciPort   The AHCI port.
 * @param   pAhciReq    The request to allocate memory for.
 * @param   cb          The amount of memory to allocate.
 */
static void *ahciReqMemAlloc(PAHCIPort pAhciPort, PAHCIREQ pAhciReq, size_t cb)
{
    if (pAhciReq->cbAlloc > cb)
    {
        pAhciReq->cAllocTooMuch++;
    }
    else if (pAhciReq->cbAlloc < cb)
    {
        if (pAhciReq->cbAlloc)
            pAhciPort->pDrvMedia->pfnIoBufFree(pAhciPort->pDrvMedia, pAhciReq->pvAlloc, pAhciReq->cbAlloc);

        pAhciReq->pvAlloc = NULL;
        pAhciReq->cbAlloc = RT_ALIGN_Z(cb, _4K);
        int rc = pAhciPort->pDrvMedia->pfnIoBufAlloc(pAhciPort->pDrvMedia, pAhciReq->cbAlloc, &pAhciReq->pvAlloc);
        if (RT_FAILURE(rc))
            pAhciReq->pvAlloc = NULL;

        pAhciReq->cAllocTooMuch = 0;
        if (RT_UNLIKELY(!pAhciReq->pvAlloc))
            pAhciReq->cbAlloc = 0;
    }

    return pAhciReq->pvAlloc;
}

/**
 * Frees memory allocated for the given request.
 *
 * @returns nothing.
 * @param   pAhciPort   The AHCI port.
 * @param   pAhciReq    The request.
 * @param   fForceFree  Flag whether to force a free
 */
static void ahciReqMemFree(PAHCIPort pAhciPort, PAHCIREQ pAhciReq, bool fForceFree)
{
    if (   pAhciReq->cAllocTooMuch >= AHCI_MAX_ALLOC_TOO_MUCH
        || fForceFree)
    {
        if (pAhciReq->cbAlloc)
        {
            pAhciPort->pDrvMedia->pfnIoBufFree(pAhciPort->pDrvMedia, pAhciReq->pvAlloc, pAhciReq->cbAlloc);
            pAhciReq->cbAlloc = 0;
            pAhciReq->cAllocTooMuch = 0;
        }
    }
}

/**
 * Copies a data buffer into the S/G buffer set up by the guest.
 *
 * @returns Amount of bytes copied to the PRDTL.
 * @param   pDevIns        Pointer to the device instance data.
 * @param   pAhciReq       AHCI request structure.
 * @param   pvBuf          The buffer to copy from.
 * @param   cbBuf          The size of the buffer.
 */
static size_t ahciCopyToPrdtl(PPDMDEVINS pDevIns, PAHCIREQ pAhciReq,
                              void *pvBuf, size_t cbBuf)
{
    uint8_t *pbBuf = (uint8_t *)pvBuf;
    SGLEntry aPrdtlEntries[32];
    RTGCPHYS GCPhysPrdtl = pAhciReq->GCPhysPrdtl;
    unsigned cPrdtlEntries = pAhciReq->cPrdtlEntries;
    size_t cbCopied = 0;

    AssertMsgReturn(cPrdtlEntries > 0, ("Copying 0 bytes is not possible\n"), 0);

    do
    {
        uint32_t cPrdtlEntriesRead = cPrdtlEntries < RT_ELEMENTS(aPrdtlEntries)
                                   ? cPrdtlEntries
                                   : RT_ELEMENTS(aPrdtlEntries);

        PDMDevHlpPhysRead(pDevIns, GCPhysPrdtl, &aPrdtlEntries[0], cPrdtlEntriesRead * sizeof(SGLEntry));

        for (uint32_t i = 0; (i < cPrdtlEntriesRead) && cbBuf; i++)
        {
            RTGCPHYS GCPhysAddrDataBase = AHCI_RTGCPHYS_FROM_U32(aPrdtlEntries[i].u32DBAUp, aPrdtlEntries[i].u32DBA);
            uint32_t cbThisCopy = (aPrdtlEntries[i].u32DescInf & SGLENTRY_DESCINF_DBC) + 1;

            cbThisCopy = RT_MIN(cbThisCopy, cbBuf);

            /* Copy into SG entry. */
            PDMDevHlpPCIPhysWrite(pDevIns, GCPhysAddrDataBase, pbBuf, cbThisCopy);

            pbBuf    += cbThisCopy;
            cbBuf    -= cbThisCopy;
            cbCopied += cbThisCopy;
        }

        GCPhysPrdtl   += cPrdtlEntriesRead * sizeof(SGLEntry);
        cPrdtlEntries -= cPrdtlEntriesRead;
    } while (cPrdtlEntries && cbBuf);

    if (cbCopied < cbBuf)
        pAhciReq->fFlags |= AHCI_REQ_OVERFLOW;

    return cbCopied;
}

/**
 * Copies the S/G buffer into a data buffer.
 *
 * @returns Amount of bytes copied to the PRDTL.
 * @param   pDevIns        Pointer to the device instance data.
 * @param   pAhciReq       AHCI request structure.
 * @param   pvBuf          The buffer to copy to.
 * @param   cbBuf          The size of the buffer.
 */
static size_t ahciCopyFromPrdtl(PPDMDEVINS pDevIns, PAHCIREQ pAhciReq,
                                void *pvBuf, size_t cbBuf)
{
    uint8_t *pbBuf = (uint8_t *)pvBuf;
    SGLEntry aPrdtlEntries[32];
    RTGCPHYS GCPhysPrdtl = pAhciReq->GCPhysPrdtl;
    unsigned cPrdtlEntries = pAhciReq->cPrdtlEntries;
    size_t cbCopied = 0;

    AssertMsgReturn(cPrdtlEntries > 0, ("Copying 0 bytes is not possible\n"), 0);

    do
    {
        uint32_t cPrdtlEntriesRead =   (cPrdtlEntries < RT_ELEMENTS(aPrdtlEntries))
                                     ? cPrdtlEntries
                                     : RT_ELEMENTS(aPrdtlEntries);

        PDMDevHlpPhysRead(pDevIns, GCPhysPrdtl, &aPrdtlEntries[0], cPrdtlEntriesRead * sizeof(SGLEntry));

        for (uint32_t i = 0; (i < cPrdtlEntriesRead) && cbBuf; i++)
        {
            RTGCPHYS GCPhysAddrDataBase = AHCI_RTGCPHYS_FROM_U32(aPrdtlEntries[i].u32DBAUp, aPrdtlEntries[i].u32DBA);
            uint32_t cbThisCopy = (aPrdtlEntries[i].u32DescInf & SGLENTRY_DESCINF_DBC) + 1;

            cbThisCopy = RT_MIN(cbThisCopy, cbBuf);

            /* Copy into buffer. */
            PDMDevHlpPhysRead(pDevIns, GCPhysAddrDataBase, pbBuf, cbThisCopy);

            pbBuf    += cbThisCopy;
            cbBuf    -= cbThisCopy;
            cbCopied += cbThisCopy;
        }

        GCPhysPrdtl   += cPrdtlEntriesRead * sizeof(SGLEntry);
        cPrdtlEntries -= cPrdtlEntriesRead;
    } while (cPrdtlEntries && cbBuf);

    if (cbCopied < cbBuf)
        pAhciReq->fFlags |= AHCI_REQ_OVERFLOW;

    return cbCopied;
}

/**
 * Allocate I/O memory and copies the guest buffer for writes.
 *
 * @returns VBox status code.
 * @param   pAhciPort   The AHCI port.
 * @param   pAhciReq    The request state.
 * @param   cbTransfer  Amount of bytes to allocate.
 */
static int ahciIoBufAllocate(PAHCIPort pAhciPort, PAHCIREQ pAhciReq, size_t cbTransfer)
{
    AssertMsg(   pAhciReq->enmTxDir == AHCITXDIR_READ
              || pAhciReq->enmTxDir == AHCITXDIR_WRITE,
              ("Allocating I/O memory for a non I/O request is not allowed\n"));

    pAhciReq->u.Io.DataSeg.pvSeg = ahciReqMemAlloc(pAhciPort, pAhciReq, cbTransfer);
    if (!pAhciReq->u.Io.DataSeg.pvSeg)
        return VERR_NO_MEMORY;

    pAhciReq->u.Io.DataSeg.cbSeg = cbTransfer;
    if (pAhciReq->enmTxDir == AHCITXDIR_WRITE)
    {
        ahciCopyFromPrdtl(pAhciPort->pDevInsR3, pAhciReq,
                          pAhciReq->u.Io.DataSeg.pvSeg,
                          cbTransfer);
    }
    return VINF_SUCCESS;
}

/**
 * Frees the I/O memory of the given request and updates the guest buffer if necessary.
 *
 * @returns nothing.
 * @param   pAhciPort    The AHCI port.
 * @param   pAhciReq     The request state.
 * @param   fCopyToGuest Flag whether to update the guest buffer if necessary.
 *                       Nothing is copied if false even if the request was a read.
 */
static void ahciIoBufFree(PAHCIPort pAhciPort, PAHCIREQ pAhciReq,
                          bool fCopyToGuest)
{
    AssertMsg(   pAhciReq->enmTxDir == AHCITXDIR_READ
              || pAhciReq->enmTxDir == AHCITXDIR_WRITE,
              ("Freeing I/O memory for a non I/O request is not allowed\n"));

    if (   pAhciReq->enmTxDir == AHCITXDIR_READ
        && fCopyToGuest)
    {
        if (pAhciReq->u.Io.pfnPostProcess)
        {
            void *pv = NULL;
            size_t cb = 0;
            int rc = pAhciReq->u.Io.pfnPostProcess(pAhciReq, &pv, &cb);

            if (RT_SUCCESS(rc))
            {
                pAhciReq->cbTransfer = ahciCopyToPrdtl(pAhciPort->pDevInsR3, pAhciReq, pv, cb);
                RTMemFree(pv);
            }
        }
        else
            ahciCopyToPrdtl(pAhciPort->pDevInsR3, pAhciReq,
                            pAhciReq->u.Io.DataSeg.pvSeg,
                            pAhciReq->u.Io.DataSeg.cbSeg);
    }

    ahciReqMemFree(pAhciPort, pAhciReq, false /* fForceFree */);
    pAhciReq->u.Io.DataSeg.pvSeg = NULL;
    pAhciReq->u.Io.DataSeg.cbSeg = 0;
}


/**
 * Free all cached tasks on the given port.
 *
 * @returns nothing.
 * @param   pAhciPort    The AHCI port.
 */
static void ahciR3PortCachedReqsFree(PAHCIPort pAhciPort)
{
    if (pAhciPort->pListReqsFree)
    {
        PAHCIREQ pReq = NULL;
        PAHCIREQ pReqNext = NULL;

        RTCritSectEnter(&pAhciPort->CritSectReqsFree);
        RTListForEachSafe(pAhciPort->pListReqsFree, pReq, pReqNext, AHCIREQ, NodeList)
        {
            RTListNodeRemove(&pReq->NodeList);
            ahciReqMemFree(pAhciPort, pReq, true /* fForceFree */);
            RTMemFree(pReq);
        }
        RTCritSectLeave(&pAhciPort->CritSectReqsFree);
    }
}

/**
 * Cancels all active tasks on the port.
 *
 * @returns Whether all active tasks were canceled.
 * @param   pAhciPort        The AHCI port.
 * @param   pAhciReqExcept   The given request is excepted from the cancelling
 *                           (used for error page reading).
 */
static bool ahciCancelActiveTasks(PAHCIPort pAhciPort, PAHCIREQ pAhciReqExcept)
{
    for (unsigned i = 0; i < RT_ELEMENTS(pAhciPort->aActiveTasks); i++)
    {
        PAHCIREQ pAhciReq = ASMAtomicReadPtrT(&pAhciPort->aActiveTasks[i], PAHCIREQ);
        if (pAhciReq != pAhciReqExcept)
        {
            pAhciReq = (PAHCIREQ)ASMAtomicXchgPtr((void * volatile *)&pAhciPort->aActiveTasks[i], NULL);

            if (VALID_PTR(pAhciReq))
            {
                bool fXchg = ASMAtomicCmpXchgU32((volatile uint32_t *)&pAhciReq->enmTxState, AHCITXSTATE_CANCELED, AHCITXSTATE_ACTIVE);
                if (fXchg)
                {
                    /* Task is active and was canceled. */
                    AssertReleaseMsg(ASMAtomicReadU32(&pAhciPort->cTasksActive) > 0,
                                     ("Task was canceled but none is active\n"));
                    ASMAtomicDecU32(&pAhciPort->cTasksActive);

                    /*
                     * Clear the pointer in the cached array. The controller will allocate a
                     * a new task structure for this tag.
                     */
                    ASMAtomicWriteNullPtr(&pAhciPort->aActiveTasks[i]);
                    LogRel(("AHCI#%uP%u: Cancelled task %u\n", pAhciPort->CTX_SUFF(pDevIns)->iInstance,
                            pAhciPort->iLUN, pAhciReq->uTag));
                }
                else
                    AssertMsg(pAhciReq->enmTxState == AHCITXSTATE_FREE,
                              ("Invalid task state, must be free!\n"));
            }
        }
    }

    return true; /* always true for now because tasks don't use guest memory as the buffer which makes canceling a task impossible. */
}

/* -=-=-=-=- IMediaAsyncPort -=-=-=-=- */

/** Makes a PAHCIPort out of a PPDMIMEDIAASYNCPORT. */
#define PDMIMEDIAASYNCPORT_2_PAHCIPORT(pInterface)    ( (PAHCIPort)((uintptr_t)pInterface - RT_OFFSETOF(AHCIPort, IPortAsync)) )

static void ahciWarningDiskFull(PPDMDEVINS pDevIns)
{
    int rc;
    LogRel(("AHCI: Host disk full\n"));
    rc = PDMDevHlpVMSetRuntimeError(pDevIns, VMSETRTERR_FLAGS_SUSPEND | VMSETRTERR_FLAGS_NO_WAIT, "DevAHCI_DISKFULL",
                                    N_("Host system reported disk full. VM execution is suspended. You can resume after freeing some space"));
    AssertRC(rc);
}

static void ahciWarningFileTooBig(PPDMDEVINS pDevIns)
{
    int rc;
    LogRel(("AHCI: File too big\n"));
    rc = PDMDevHlpVMSetRuntimeError(pDevIns, VMSETRTERR_FLAGS_SUSPEND | VMSETRTERR_FLAGS_NO_WAIT, "DevAHCI_FILETOOBIG",
                                    N_("Host system reported that the file size limit of the host file system has been exceeded. VM execution is suspended. You need to move your virtual hard disk to a filesystem which allows bigger files"));
    AssertRC(rc);
}

static void ahciWarningISCSI(PPDMDEVINS pDevIns)
{
    int rc;
    LogRel(("AHCI: iSCSI target unavailable\n"));
    rc = PDMDevHlpVMSetRuntimeError(pDevIns, VMSETRTERR_FLAGS_SUSPEND | VMSETRTERR_FLAGS_NO_WAIT, "DevAHCI_ISCSIDOWN",
                                    N_("The iSCSI target has stopped responding. VM execution is suspended. You can resume when it is available again"));
    AssertRC(rc);
}

bool ahciIsRedoSetWarning(PAHCIPort pAhciPort, int rc)
{
    if (rc == VERR_DISK_FULL)
    {
        if (ASMAtomicCmpXchgBool(&pAhciPort->fRedo, true, false))
            ahciWarningDiskFull(pAhciPort->CTX_SUFF(pDevIns));
        return true;
    }
    if (rc == VERR_FILE_TOO_BIG)
    {
        if (ASMAtomicCmpXchgBool(&pAhciPort->fRedo, true, false))
            ahciWarningFileTooBig(pAhciPort->CTX_SUFF(pDevIns));
        return true;
    }
    if (rc == VERR_BROKEN_PIPE || rc == VERR_NET_CONNECTION_REFUSED)
    {
        /* iSCSI connection abort (first error) or failure to reestablish
         * connection (second error). Pause VM. On resume we'll retry. */
        if (ASMAtomicCmpXchgBool(&pAhciPort->fRedo, true, false))
            ahciWarningISCSI(pAhciPort->CTX_SUFF(pDevIns));
        return true;
    }
    if (rc == VERR_VD_DEK_MISSING)
    {
        /* Error message already set. */
        ASMAtomicCmpXchgBool(&pAhciPort->fRedo, true, false);
        return true;
    }

    return false;
}

/**
 * Creates the array of ranges to trim.
 *
 * @returns VBox status code.
 * @param   pAhciPort    AHCI port state.
 * @param   pAhciReq     The request handling the TRIM request.
 */
static int ahciTrimRangesCreate(PAHCIPort pAhciPort, PAHCIREQ pAhciReq)
{
    SGLEntry aPrdtlEntries[32];
    uint64_t aRanges[64];
    unsigned cRangesMax;
    unsigned cRanges = 0;
    uint32_t cPrdtlEntries = pAhciReq->cPrdtlEntries;
    RTGCPHYS GCPhysPrdtl   = pAhciReq->GCPhysPrdtl;
    PPDMDEVINS pDevIns     = pAhciPort->CTX_SUFF(pDevIns);
    int rc = VINF_SUCCESS;

    LogFlowFunc(("pAhciPort=%#p pAhciReq=%#p\n", pAhciPort, pAhciReq));

    AssertMsgReturn(pAhciReq->enmTxDir == AHCITXDIR_TRIM, ("This is not a trim request\n"), VERR_INVALID_PARAMETER);

    /* The data buffer contains LBA range entries. Each range is 8 bytes big. */
    if (!pAhciReq->cmdFis[AHCI_CMDFIS_SECTC] && !pAhciReq->cmdFis[AHCI_CMDFIS_SECTCEXP])
        cRangesMax = 65536 * 512 / 8;
    else
        cRangesMax = pAhciReq->cmdFis[AHCI_CMDFIS_SECTC] * 512 / 8;

    if (!cPrdtlEntries)
    {
        pAhciReq->fFlags |= AHCI_REQ_OVERFLOW;
        return VINF_SUCCESS;
    }

    do
    {
        uint32_t cPrdtlEntriesRead =   (cPrdtlEntries < RT_ELEMENTS(aPrdtlEntries))
                                     ? cPrdtlEntries
                                     : RT_ELEMENTS(aPrdtlEntries);

        PDMDevHlpPhysRead(pDevIns, GCPhysPrdtl, &aPrdtlEntries[0], cPrdtlEntriesRead * sizeof(SGLEntry));

        for (uint32_t i = 0; i < cPrdtlEntriesRead; i++)
        {
            RTGCPHYS GCPhysAddrDataBase = AHCI_RTGCPHYS_FROM_U32(aPrdtlEntries[i].u32DBAUp, aPrdtlEntries[i].u32DBA);
            uint32_t cbThisCopy = (aPrdtlEntries[i].u32DescInf & SGLENTRY_DESCINF_DBC) + 1;

            cbThisCopy = RT_MIN(cbThisCopy, sizeof(aRanges));

            /* Copy into buffer. */
            PDMDevHlpPhysRead(pDevIns, GCPhysAddrDataBase, aRanges, cbThisCopy);

            for (unsigned idxRange = 0; idxRange < RT_ELEMENTS(aRanges); idxRange++)
            {
                aRanges[idxRange] = RT_H2LE_U64(aRanges[idxRange]);
                if (AHCI_RANGE_LENGTH_GET(aRanges[idxRange]) != 0)
                    cRanges++;
                else
                    break;
            }
        }

        GCPhysPrdtl   += cPrdtlEntriesRead * sizeof(SGLEntry);
        cPrdtlEntries -= cPrdtlEntriesRead;
    } while (cPrdtlEntries);

    if (RT_UNLIKELY(!cRanges))
    {
        return VERR_BUFFER_OVERFLOW;
    }

    pAhciReq->u.Trim.cRanges = cRanges;
    pAhciReq->u.Trim.paRanges = (PRTRANGE)RTMemAllocZ(sizeof(RTRANGE) * cRanges);
    if (pAhciReq->u.Trim.paRanges)
    {
        uint32_t idxRange = 0;

        cPrdtlEntries = pAhciReq->cPrdtlEntries;
        GCPhysPrdtl   = pAhciReq->GCPhysPrdtl;

        /* Convert the ranges from the guest to our format. */
        do
        {
            uint32_t cPrdtlEntriesRead =   (cPrdtlEntries < RT_ELEMENTS(aPrdtlEntries))
                                         ? cPrdtlEntries
                                         : RT_ELEMENTS(aPrdtlEntries);

            PDMDevHlpPhysRead(pDevIns, GCPhysPrdtl, &aPrdtlEntries[0], cPrdtlEntriesRead * sizeof(SGLEntry));

            for (uint32_t i = 0; i < cPrdtlEntriesRead; i++)
            {
                RTGCPHYS GCPhysAddrDataBase = AHCI_RTGCPHYS_FROM_U32(aPrdtlEntries[i].u32DBAUp, aPrdtlEntries[i].u32DBA);
                uint32_t cbThisCopy = (aPrdtlEntries[i].u32DescInf & SGLENTRY_DESCINF_DBC) + 1;

                cbThisCopy = RT_MIN(cbThisCopy, sizeof(aRanges));

                /* Copy into buffer. */
                PDMDevHlpPhysRead(pDevIns, GCPhysAddrDataBase, aRanges, cbThisCopy);

                for (unsigned idxRangeSrc = 0; idxRangeSrc < RT_ELEMENTS(aRanges); idxRangeSrc++)
                {
                    aRanges[idxRangeSrc] = RT_H2LE_U64(aRanges[idxRangeSrc]);
                    if (AHCI_RANGE_LENGTH_GET(aRanges[idxRangeSrc]) != 0)
                    {
                        pAhciReq->u.Trim.paRanges[idxRange].offStart = (aRanges[idxRangeSrc] & AHCI_RANGE_LBA_MASK) * pAhciPort->cbSector;
                        pAhciReq->u.Trim.paRanges[idxRange].cbRange = AHCI_RANGE_LENGTH_GET(aRanges[idxRangeSrc]) * pAhciPort->cbSector;
                        idxRange++;
                    }
                    else
                        break;
                }
            }

            GCPhysPrdtl   += cPrdtlEntriesRead * sizeof(SGLEntry);
            cPrdtlEntries -= cPrdtlEntriesRead;
        } while (idxRange < cRanges);
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFunc(("returns rc=%Rrc\n", rc));
    return rc;
}

/**
 * Destroy the trim range list.
 *
 * @returns nothing.
 * @param   pAhciReq    The task state.
 */
static void ahciTrimRangesDestroy(PAHCIREQ pAhciReq)
{
    AssertReturnVoid(pAhciReq->enmTxDir == AHCITXDIR_TRIM);
    RTMemFree(pAhciReq->u.Trim.paRanges);
}

/**
 * Allocates a new AHCI request.
 *
 * @returns A new AHCI request structure or NULL if out of memory.
 * @param   pAhciPort    The AHCI port.
 */
static PAHCIREQ ahciR3ReqAlloc(PAHCIPort pAhciPort)
{
    PAHCIREQ pAhciReq = NULL;

    /* Check the global free list first. */
    RTCritSectEnter(&pAhciPort->CritSectReqsFree);
    pAhciReq = RTListGetFirst(pAhciPort->pListReqsFree, AHCIREQ, NodeList);
    if (pAhciReq)
    {
        AssertMsg(pAhciReq->enmTxState == AHCITXSTATE_FREE, ("Should be free!\n"));
        RTListNodeRemove(&pAhciReq->NodeList);
    }
    RTCritSectLeave(&pAhciPort->CritSectReqsFree);

    if (!pAhciReq)
        pAhciReq = (PAHCIREQ)RTMemAllocZ(sizeof(AHCIREQ));

    pAhciReq->enmTxState = AHCITXSTATE_ACTIVE;
    return pAhciReq;
}

/**
 * Frees a given AHCI request structure.
 *
 * @returns nothing.
 * @param   pAhciPort    The AHCI port.
 */
static void ahciR3ReqFree(PAHCIPort pAhciPort, PAHCIREQ pAhciReq)
{
    AssertMsg(pAhciReq->enmTxState != AHCITXSTATE_FREE, ("Double free!\n"));
    pAhciReq->enmTxState = AHCITXSTATE_FREE;

    RTCritSectEnter(&pAhciPort->CritSectReqsFree);
    RTListAppend(pAhciPort->pListReqsFree, &pAhciReq->NodeList);
    RTCritSectLeave(&pAhciPort->CritSectReqsFree);
}

/**
 * Complete a data transfer task by freeing all occupied resources
 * and notifying the guest.
 *
 * @returns Flag whether the given request was canceled inbetween;
 *
 * @param pAhciPort    Pointer to the port where to request completed.
 * @param pAhciReq     Pointer to the task which finished.
 * @param rcReq        IPRT status code of the completed request.
 */
static bool ahciTransferComplete(PAHCIPort pAhciPort, PAHCIREQ pAhciReq, int rcReq)
{
    bool fRedo = false;
    bool fCanceled = false;
    uint64_t tsNow = RTTimeMilliTS();
    AHCITXSTATE enmTxState = AHCITXSTATE_INVALID;

    LogFlowFunc(("pAhciPort=%p pAhciReq=%p rcReq=%d\n",
                 pAhciPort, pAhciReq, rcReq));

    enmTxState = (AHCITXSTATE)ASMAtomicReadU32((volatile uint32_t *)&pAhciReq->enmTxState);
    VBOXDD_AHCI_REQ_COMPLETED(pAhciReq, rcReq, enmTxState, pAhciReq->uOffset, pAhciReq->cbTransfer);
    VBOXDD_AHCI_REQ_COMPLETED_TIMESTAMP(pAhciReq, tsNow);

    /*
     * Clear the request structure from the active request list first so it doesn't get cancelled
     * while we complete it. If the request is not in the active list anymore it was already canceled
     * and we have to make sure to not copy anything to guest memory because the guest might use it
     * for other things already.
     */
    bool fPortReset = ASMAtomicReadBool(&pAhciPort->fPortReset);
    bool fXchg = ASMAtomicCmpXchgPtr(&pAhciPort->aActiveTasks[pAhciReq->uTag], NULL, pAhciReq);
    bool fReqErrSaved = false;

    /*
     * Leave a release log entry if the request was active for more than 25 seconds
     * (30 seconds is the timeout of the guest).
     */
    if (tsNow - pAhciReq->tsStart >= 25 * 1000)
    {
        const char *pcszReq = NULL;

        switch (pAhciReq->enmTxDir)
        {
            case AHCITXDIR_READ:
                pcszReq = "Read";
                break;
            case AHCITXDIR_WRITE:
                pcszReq = "Write";
                break;
            case AHCITXDIR_FLUSH:
                pcszReq = "Flush";
                break;
            case AHCITXDIR_TRIM:
                pcszReq = "Trim";
                break;
            default:
                pcszReq = "<Invalid>";
        }

        LogRel(("AHCI#%uP%u: %s request was active for %llu seconds\n",
                pAhciPort->CTX_SUFF(pDevIns)->iInstance, pAhciPort->iLUN, pcszReq, (tsNow - pAhciReq->tsStart) / 1000));
    }

    if (fXchg && !fPortReset)
    {
        if (pAhciReq->enmTxDir == AHCITXDIR_READ)
        {
            ahciIoBufFree(pAhciPort, pAhciReq, true /* fCopyToGuest */);
            STAM_REL_COUNTER_ADD(&pAhciPort->StatBytesRead, pAhciReq->cbTransfer);
            pAhciPort->Led.Actual.s.fReading = 0;
        }
        else if (pAhciReq->enmTxDir == AHCITXDIR_WRITE)
        {
            ahciIoBufFree(pAhciPort, pAhciReq, false /* fCopyToGuest */);
            STAM_REL_COUNTER_ADD(&pAhciPort->StatBytesWritten, pAhciReq->cbTransfer);
            pAhciPort->Led.Actual.s.fWriting = 0;
        }
        else if (pAhciReq->enmTxDir == AHCITXDIR_TRIM)
        {
            ahciTrimRangesDestroy(pAhciReq);
            pAhciPort->Led.Actual.s.fWriting = 0;
        }

        if (RT_FAILURE(rcReq))
        {
            /* Log the error. */
            if (pAhciPort->cErrors++ < MAX_LOG_REL_ERRORS)
            {
                if (pAhciReq->enmTxDir == AHCITXDIR_FLUSH)
                    LogRel(("AHCI#%uP%u: Flush returned rc=%Rrc\n",
                            pAhciPort->CTX_SUFF(pDevIns)->iInstance, pAhciPort->iLUN, rcReq));
                else if (pAhciReq->enmTxDir == AHCITXDIR_TRIM)
                    LogRel(("AHCI#%uP%u: Trim returned rc=%Rrc\n",
                            pAhciPort->CTX_SUFF(pDevIns)->iInstance, pAhciPort->iLUN, rcReq));
                else
                    LogRel(("AHCI#%uP%u: %s at offset %llu (%u bytes left) returned rc=%Rrc\n",
                            pAhciPort->CTX_SUFF(pDevIns)->iInstance, pAhciPort->iLUN,
                            pAhciReq->enmTxDir == AHCITXDIR_READ
                            ? "Read"
                            : "Write",
                            pAhciReq->uOffset,
                            pAhciReq->cbTransfer, rcReq));
            }

            fRedo = ahciIsRedoSetWarning(pAhciPort, rcReq);
            if (!fRedo)
            {
                pAhciReq->cmdHdr.u32PRDBC = 0;
                pAhciReq->uATARegError    = ID_ERR;
                pAhciReq->uATARegStatus   = ATA_STAT_READY | ATA_STAT_ERR;
                fReqErrSaved = ASMAtomicCmpXchgPtr(&pAhciPort->pTaskErr, pAhciReq, NULL);
            }
            else
                ASMAtomicOrU32(&pAhciPort->u32TasksRedo, RT_BIT_32(pAhciReq->uTag));
        }
        else
        {
            pAhciReq->cmdHdr.u32PRDBC = pAhciReq->cbTransfer;

            /* Status will be set already for non I/O requests. */
            if (pAhciReq->enmTxDir != AHCITXDIR_NONE)
            {
                pAhciReq->uATARegError = 0;
                pAhciReq->uATARegStatus = ATA_STAT_READY | ATA_STAT_SEEK;
            }

            /* Write updated command header into memory of the guest. */
            PDMDevHlpPCIPhysWrite(pAhciPort->CTX_SUFF(pDevIns), pAhciReq->GCPhysCmdHdrAddr, &pAhciReq->cmdHdr, sizeof(CmdHdr));

            if (pAhciReq->fFlags & AHCI_REQ_OVERFLOW)
            {
                /*
                 * The guest tried to transfer more data than there is space in the buffer.
                 * Terminate task and set the overflow bit.
                 */
                /* Notify the guest. */
                ASMAtomicOrU32(&pAhciPort->regIS, AHCI_PORT_IS_OFS);
                if (pAhciPort->regIE & AHCI_PORT_IE_OFE)
                    ahciHbaSetInterrupt(pAhciPort->CTX_SUFF(pAhci), pAhciPort->iLUN, VERR_IGNORED);
            }
        }

        if (!fRedo)
        {

            /* Post a PIO setup FIS first if this is a PIO command which transfers data. */
            if (pAhciReq->fFlags & AHCI_REQ_PIO_DATA)
                ahciSendPioSetupFis(pAhciPort, pAhciReq, pAhciReq->cmdFis, false /* fInterrupt */);

            if (pAhciReq->fFlags & AHCI_REQ_CLEAR_SACT)
            {
                if (RT_SUCCESS(rcReq) && !ASMAtomicReadPtrT(&pAhciPort->pTaskErr, PAHCIREQ))
                    ASMAtomicOrU32(&pAhciPort->u32QueuedTasksFinished, RT_BIT_32(pAhciReq->uTag));
            }

            if (pAhciReq->fFlags & AHCI_REQ_IS_QUEUED)
            {
                /*
                 * Always raise an interrupt after task completion; delaying
                 * this (interrupt coalescing) increases latency and has a significant
                 * impact on performance (see @bugref{5071})
                 */
                ahciSendSDBFis(pAhciPort, 0, true);
            }
            else
                ahciSendD2HFis(pAhciPort, pAhciReq, pAhciReq->cmdFis, true);
        }
    }
    else
    {
        /*
         * Task was canceled, do the cleanup but DO NOT access the guest memory!
         * The guest might use it for other things now because it doesn't know about that task anymore.
         */
        AssertMsg(pAhciReq->enmTxState == AHCITXSTATE_CANCELED || fPortReset,
                  ("Task is not active but wasn't canceled and no port reset is active!\n"));

        fCanceled = true;

        if (pAhciReq->enmTxDir == AHCITXDIR_TRIM)
            ahciTrimRangesDestroy(pAhciReq);
        else if (pAhciReq->enmTxDir != AHCITXDIR_FLUSH)
            ahciIoBufFree(pAhciPort, pAhciReq, false /* fCopyToGuest */);

        /* Leave a log message about the canceled request. */
        if (pAhciPort->cErrors++ < MAX_LOG_REL_ERRORS)
        {
            if (pAhciReq->enmTxDir == AHCITXDIR_FLUSH)
                LogRel(("AHCI#%uP%u: Canceled flush returned rc=%Rrc\n",
                        pAhciPort->CTX_SUFF(pDevIns)->iInstance, pAhciPort->iLUN, rcReq));
            else if (pAhciReq->enmTxDir == AHCITXDIR_TRIM)
                LogRel(("AHCI#%uP%u: Canceled trim returned rc=%Rrc\n",
                        pAhciPort->CTX_SUFF(pDevIns)->iInstance,pAhciPort->iLUN, rcReq));
            else
                LogRel(("AHCI#%uP%u: Canceled %s at offset %llu (%u bytes left) returned rc=%Rrc\n",
                        pAhciPort->CTX_SUFF(pDevIns)->iInstance, pAhciPort->iLUN,
                        pAhciReq->enmTxDir == AHCITXDIR_READ
                        ? "read"
                        : "write",
                        pAhciReq->uOffset,
                        pAhciReq->cbTransfer, rcReq));
         }
    }

    /*
     * Decrement the active task counter as the last step or we might run into a
     * hang during power off otherwise (see @bugref{7859}).
     * Before it could happen that we signal PDM that we are done while we still have to
     * copy the data to the guest but EMT might be busy destroying the driver chains
     * below us while we have to delegate copying data to EMT instead of doing it
     * on this thread.
     */
    if (fXchg)
    {
        AssertReleaseMsg(ASMAtomicReadU32(&pAhciPort->cTasksActive) > 0,
                         ("Inconsistent request counter\n"));
        ASMAtomicDecU32(&pAhciPort->cTasksActive);
    }

    if (pAhciPort->cTasksActive == 0 && pAhciPort->pAhciR3->fSignalIdle)
        PDMDevHlpAsyncNotificationCompleted(pAhciPort->pDevInsR3);

    /* Don't free the request yet when it is saved for the error log page. */
    if (   pAhciReq
        && !(pAhciReq->fFlags & AHCI_REQ_IS_ON_STACK)
        && !fReqErrSaved)
        ahciR3ReqFree(pAhciPort, pAhciReq);
    return fCanceled;
}

/**
 * Notification callback for a completed transfer.
 *
 * @returns VBox status code.
 * @param   pInterface   Pointer to the interface.
 * @param   pvUser       User data.
 * @param   rcReq        IPRT Status code of the completed request.
 */
static DECLCALLBACK(int) ahciR3TransferCompleteNotify(PPDMIMEDIAASYNCPORT pInterface, void *pvUser, int rcReq)
{
    PAHCIPort pAhciPort = PDMIMEDIAASYNCPORT_2_PAHCIPORT(pInterface);
    PAHCIREQ pAhciReq = (PAHCIREQ)pvUser;

    ahciLog(("%s: pInterface=%p pvUser=%p uTag=%u\n",
             __FUNCTION__, pInterface, pvUser, pAhciReq->uTag));

    ahciTransferComplete(pAhciPort, pAhciReq, rcReq);

    return VINF_SUCCESS;
}

/**
 * Process an non read/write ATA command.
 *
 * @returns The direction of the data transfer
 * @param   pCmdHdr Pointer to the command header.
 */
static AHCITXDIR ahciProcessCmd(PAHCIPort pAhciPort, PAHCIREQ pAhciReq, uint8_t *pCmdFis)
{
    AHCITXDIR enmTxDir = AHCITXDIR_NONE;
    bool fLBA48 = false;
    CmdHdr   *pCmdHdr = &pAhciReq->cmdHdr;

    AssertMsg(pCmdFis[AHCI_CMDFIS_TYPE] == AHCI_CMDFIS_TYPE_H2D, ("FIS is not a host to device Fis!!\n"));

    pAhciReq->cbTransfer = 0;

    switch (pCmdFis[AHCI_CMDFIS_CMD])
    {
        case ATA_IDENTIFY_DEVICE:
        {
            if (pAhciPort->pDrvMedia && !pAhciPort->fATAPI)
            {
                uint16_t u16Temp[256];
                size_t cbCopied;

                /* Fill the buffer. */
                ahciIdentifySS(pAhciPort, u16Temp);

                /* Copy the buffer. */
                cbCopied = ahciCopyToPrdtl(pAhciPort->pDevInsR3, pAhciReq,
                                           &u16Temp[0], sizeof(u16Temp));

                pAhciReq->fFlags |= AHCI_REQ_PIO_DATA;
                pAhciReq->cbTransfer = cbCopied;
            }
            else
            {
                pAhciReq->uATARegError = ABRT_ERR;
                pAhciReq->uATARegStatus = ATA_STAT_READY | ATA_STAT_SEEK  | ATA_STAT_ERR;
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
                    pAhciReq->uATARegError = 0;
                    pAhciReq->uATARegStatus = ATA_STAT_READY | ATA_STAT_SEEK;
                    break;
                case 0x82: /* write cache disable */
                    enmTxDir = AHCITXDIR_FLUSH;
                    break;
                case 0x03:
                {
                    /* set transfer mode */
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
                    pAhciReq->uATARegError = ABRT_ERR;
                    pAhciReq->uATARegStatus = ATA_STAT_READY | ATA_STAT_ERR;
            }
            break;
        }
        case ATA_DEVICE_RESET:
        {
            if (!pAhciPort->fATAPI)
            {
                pAhciReq->uATARegError = ABRT_ERR;
                pAhciReq->uATARegStatus = ATA_STAT_READY | ATA_STAT_ERR;
            }
            else
            {
                /* Reset the device. */
                ahciDeviceReset(pAhciPort, pAhciReq);
            }
            break;
        }
        case ATA_FLUSH_CACHE_EXT:
        case ATA_FLUSH_CACHE:
            enmTxDir = AHCITXDIR_FLUSH;
            break;
        case ATA_PACKET:
            if (!pAhciPort->fATAPI)
            {
                pAhciReq->uATARegError = ABRT_ERR;
                pAhciReq->uATARegStatus = ATA_STAT_READY | ATA_STAT_ERR;
            }
            else
                enmTxDir = atapiParseCmd(pAhciPort, pAhciReq);
            break;
        case ATA_IDENTIFY_PACKET_DEVICE:
            if (!pAhciPort->fATAPI)
            {
                pAhciReq->uATARegError = ABRT_ERR;
                pAhciReq->uATARegStatus = ATA_STAT_READY | ATA_STAT_ERR;
            }
            else
            {
                atapiDoTransfer(pAhciPort, pAhciReq, 512, ATAFN_SS_ATAPI_IDENTIFY);

                pAhciReq->fFlags |= AHCI_REQ_PIO_DATA;
                pAhciReq->uATARegError = 0;
                pAhciReq->uATARegStatus = ATA_STAT_READY | ATA_STAT_SEEK;
            }
            break;
        case ATA_SET_MULTIPLE_MODE:
            if (    pCmdFis[AHCI_CMDFIS_SECTC] != 0
                &&  (   pCmdFis[AHCI_CMDFIS_SECTC] > ATA_MAX_MULT_SECTORS
                     || (pCmdFis[AHCI_CMDFIS_SECTC] & (pCmdFis[AHCI_CMDFIS_SECTC] - 1)) != 0))
            {
                pAhciReq->uATARegError = ABRT_ERR;
                pAhciReq->uATARegStatus = ATA_STAT_READY | ATA_STAT_ERR;
            }
            else
            {
                Log2(("%s: set multi sector count to %d\n", __FUNCTION__, pCmdFis[AHCI_CMDFIS_SECTC]));
                pAhciPort->cMultSectors = pCmdFis[AHCI_CMDFIS_SECTC];
                pAhciReq->uATARegError = 0;
                pAhciReq->uATARegStatus = ATA_STAT_READY | ATA_STAT_SEEK;
            }
            break;
        case ATA_STANDBY_IMMEDIATE:
            break; /* Do nothing. */
        case ATA_CHECK_POWER_MODE:
            pAhciReq->cmdFis[AHCI_CMDFIS_SECTC] = 0xff; /* drive active or idle */
            /* fall through */
        case ATA_INITIALIZE_DEVICE_PARAMETERS:
        case ATA_IDLE_IMMEDIATE:
        case ATA_RECALIBRATE:
        case ATA_NOP:
        case ATA_READ_VERIFY_SECTORS_EXT:
        case ATA_READ_VERIFY_SECTORS:
        case ATA_READ_VERIFY_SECTORS_WITHOUT_RETRIES:
        case ATA_SLEEP:
            pAhciReq->uATARegError = 0;
            pAhciReq->uATARegStatus = ATA_STAT_READY | ATA_STAT_SEEK;
            break;
        case ATA_READ_DMA_EXT:
            fLBA48 = true;
        case ATA_READ_DMA:
        {
            pAhciReq->cbTransfer = ahciGetNSectors(pCmdFis, fLBA48) * pAhciPort->cbSector;
            pAhciReq->uOffset = ahciGetSector(pAhciPort, pCmdFis, fLBA48) * pAhciPort->cbSector;
            enmTxDir = AHCITXDIR_READ;
            break;
        }
        case ATA_WRITE_DMA_EXT:
            fLBA48 = true;
        case ATA_WRITE_DMA:
        {
            pAhciReq->cbTransfer = ahciGetNSectors(pCmdFis, fLBA48) * pAhciPort->cbSector;
            pAhciReq->uOffset = ahciGetSector(pAhciPort, pCmdFis, fLBA48) * pAhciPort->cbSector;
            enmTxDir = AHCITXDIR_WRITE;
            break;
        }
        case ATA_READ_FPDMA_QUEUED:
        {
            pAhciReq->cbTransfer = ahciGetNSectorsQueued(pCmdFis) * pAhciPort->cbSector;
            pAhciReq->uOffset = ahciGetSectorQueued(pCmdFis) * pAhciPort->cbSector;
            pAhciReq->fFlags |= AHCI_REQ_IS_QUEUED;
            enmTxDir = AHCITXDIR_READ;
            break;
        }
        case ATA_WRITE_FPDMA_QUEUED:
        {
            pAhciReq->cbTransfer = ahciGetNSectorsQueued(pCmdFis) * pAhciPort->cbSector;
            pAhciReq->uOffset = ahciGetSectorQueued(pCmdFis) * pAhciPort->cbSector;
            pAhciReq->fFlags |= AHCI_REQ_IS_QUEUED;
            enmTxDir = AHCITXDIR_WRITE;
            break;
        }
        case ATA_READ_LOG_EXT:
        {
            size_t cbLogRead = ((pCmdFis[AHCI_CMDFIS_SECTCEXP] << 8) | pCmdFis[AHCI_CMDFIS_SECTC]) * 512;
            unsigned offLogRead = ((pCmdFis[AHCI_CMDFIS_CYLLEXP] << 8) | pCmdFis[AHCI_CMDFIS_CYLL]) * 512;
            unsigned iPage = pCmdFis[AHCI_CMDFIS_SECTN];
            size_t cbCopied;

            LogFlow(("Trying to read %zu bytes starting at offset %u from page %u\n", cbLogRead, offLogRead, iPage));

            uint8_t aBuf[512];

            memset(aBuf, 0, sizeof(aBuf));

            if (offLogRead + cbLogRead <= sizeof(aBuf))
            {
                switch (iPage)
                {
                    case 0x10:
                    {
                        LogFlow(("Reading error page\n"));
                        PAHCIREQ pTaskErr = ASMAtomicXchgPtrT(&pAhciPort->pTaskErr, NULL, PAHCIREQ);
                        if (pTaskErr)
                        {
                            aBuf[0] = (pTaskErr->fFlags & AHCI_REQ_IS_QUEUED) ? pTaskErr->uTag : (1 << 7);
                            aBuf[2] = pTaskErr->uATARegStatus;
                            aBuf[3] = pTaskErr->uATARegError;
                            aBuf[4] = pTaskErr->cmdFis[AHCI_CMDFIS_SECTN];
                            aBuf[5] = pTaskErr->cmdFis[AHCI_CMDFIS_CYLL];
                            aBuf[6] = pTaskErr->cmdFis[AHCI_CMDFIS_CYLH];
                            aBuf[7] = pTaskErr->cmdFis[AHCI_CMDFIS_HEAD];
                            aBuf[8] = pTaskErr->cmdFis[AHCI_CMDFIS_SECTNEXP];
                            aBuf[9] = pTaskErr->cmdFis[AHCI_CMDFIS_CYLLEXP];
                            aBuf[10] = pTaskErr->cmdFis[AHCI_CMDFIS_CYLHEXP];
                            aBuf[12] = pTaskErr->cmdFis[AHCI_CMDFIS_SECTC];
                            aBuf[13] = pTaskErr->cmdFis[AHCI_CMDFIS_SECTCEXP];

                            /* Calculate checksum */
                            uint8_t uChkSum = 0;
                            for (unsigned i = 0; i < RT_ELEMENTS(aBuf)-1; i++)
                                uChkSum += aBuf[i];

                            aBuf[511] = (uint8_t)-(int8_t)uChkSum;

                            if (pTaskErr->enmTxDir == AHCITXDIR_TRIM)
                                ahciTrimRangesDestroy(pTaskErr);
                            else if (pTaskErr->enmTxDir != AHCITXDIR_FLUSH)
                                ahciReqMemFree(pAhciPort, pTaskErr, true /* fForceFree */);

                            /* Finally free the error task state structure because it is completely unused now. */
                            RTMemFree(pTaskErr);
                        }

                        /*
                         * Reading this log page results in an abort of all outstanding commands
                         * and clearing the SActive register and TaskFile register.
                         *
                         * See SATA2 1.2 spec chapter 4.2.3.4
                         */
                        bool fAbortedAll = ahciCancelActiveTasks(pAhciPort, pAhciReq);
                        Assert(fAbortedAll);
                        ahciSendSDBFis(pAhciPort, 0xffffffff, true);

                        break;
                    }
                }

                /* Copy the buffer. */
                cbCopied = ahciCopyToPrdtl(pAhciPort->pDevInsR3, pAhciReq,
                                           &aBuf[offLogRead], cbLogRead);

                pAhciReq->fFlags |= AHCI_REQ_PIO_DATA;
                pAhciReq->cbTransfer = cbCopied;
            }

            break;
        }
        case ATA_DATA_SET_MANAGEMENT:
        {
            if (   (   !pAhciPort->fAsyncInterface
                    && pAhciPort->pDrvMedia->pfnDiscard)
                || (   pAhciPort->fAsyncInterface
                    && pAhciPort->pDrvMediaAsync->pfnStartDiscard))
            {
                /* Check that the trim bit is set and all other bits are 0. */
                if (   !(pAhciReq->cmdFis[AHCI_CMDFIS_FET] & UINT16_C(0x01))
                    || (pAhciReq->cmdFis[AHCI_CMDFIS_FET] & ~UINT16_C(0x1)))
                {
                    pAhciReq->uATARegError = ABRT_ERR;
                    pAhciReq->uATARegStatus = ATA_STAT_READY | ATA_STAT_SEEK;
                }
                else
                    enmTxDir = AHCITXDIR_TRIM;
                break;
            }
            /* else: fall through and report error to the guest. */
        }
        /* All not implemented commands go below. */
        case ATA_SECURITY_FREEZE_LOCK:
        case ATA_SMART:
        case ATA_NV_CACHE:
        case ATA_IDLE:
        case ATA_TRUSTED_RECEIVE_DMA: /* Windows 8+ */
            pAhciReq->uATARegError = ABRT_ERR;
            pAhciReq->uATARegStatus = ATA_STAT_READY | ATA_STAT_ERR;
            break;
        default: /* For debugging purposes. */
            AssertMsgFailed(("Unknown command issued (%#x)\n", pCmdFis[AHCI_CMDFIS_CMD]));
            pAhciReq->uATARegError = ABRT_ERR;
            pAhciReq->uATARegStatus = ATA_STAT_READY | ATA_STAT_ERR;
    }

    return enmTxDir;
}

/**
 * Retrieve a command FIS from guest memory.
 *
 * @returns whether the H2D FIS was successfully read from the guest memory.
 * @param pAhciReq The state of the actual task.
 */
static bool ahciPortTaskGetCommandFis(PAHCIPort pAhciPort, PAHCIREQ pAhciReq)
{
    RTGCPHYS  GCPhysAddrCmdTbl;

    AssertMsgReturn(pAhciPort->GCPhysAddrClb && pAhciPort->GCPhysAddrFb,
                    ("%s: GCPhysAddrClb and/or GCPhysAddrFb are 0\n", __FUNCTION__),
                    false);

    /*
     * First we are reading the command header pointed to by regCLB.
     * From this we get the address of the command table which we are reading too.
     * We can process the Command FIS afterwards.
     */
    pAhciReq->GCPhysCmdHdrAddr = pAhciPort->GCPhysAddrClb + pAhciReq->uTag * sizeof(CmdHdr);
    LogFlow(("%s: PDMDevHlpPhysRead GCPhysAddrCmdLst=%RGp cbCmdHdr=%u\n", __FUNCTION__,
             pAhciReq->GCPhysCmdHdrAddr, sizeof(CmdHdr)));
    PDMDevHlpPhysRead(pAhciPort->CTX_SUFF(pDevIns), pAhciReq->GCPhysCmdHdrAddr, &pAhciReq->cmdHdr, sizeof(CmdHdr));

#ifdef LOG_ENABLED
    /* Print some infos about the command header. */
    ahciDumpCmdHdrInfo(pAhciPort, &pAhciReq->cmdHdr);
#endif

    GCPhysAddrCmdTbl = AHCI_RTGCPHYS_FROM_U32(pAhciReq->cmdHdr.u32CmdTblAddrUp, pAhciReq->cmdHdr.u32CmdTblAddr);

    AssertMsgReturn((pAhciReq->cmdHdr.u32DescInf & AHCI_CMDHDR_CFL_MASK) * sizeof(uint32_t) == AHCI_CMDFIS_TYPE_H2D_SIZE,
                    ("This is not a command FIS!!\n"),
                    false);

    /* Read the command Fis. */
    LogFlow(("%s: PDMDevHlpPhysRead GCPhysAddrCmdTbl=%RGp cbCmdFis=%u\n", __FUNCTION__, GCPhysAddrCmdTbl, AHCI_CMDFIS_TYPE_H2D_SIZE));
    PDMDevHlpPhysRead(pAhciPort->CTX_SUFF(pDevIns), GCPhysAddrCmdTbl, &pAhciReq->cmdFis[0], AHCI_CMDFIS_TYPE_H2D_SIZE);

    AssertMsgReturn(pAhciReq->cmdFis[AHCI_CMDFIS_TYPE] == AHCI_CMDFIS_TYPE_H2D,
                    ("This is not a command FIS\n"),
                    false);

    /* Set transfer direction. */
    pAhciReq->enmTxDir = (pAhciReq->cmdHdr.u32DescInf & AHCI_CMDHDR_W) ? AHCITXDIR_WRITE : AHCITXDIR_READ;

    /* If this is an ATAPI command read the atapi command. */
    if (pAhciReq->cmdHdr.u32DescInf & AHCI_CMDHDR_A)
    {
        GCPhysAddrCmdTbl += AHCI_CMDHDR_ACMD_OFFSET;
        PDMDevHlpPhysRead(pAhciPort->CTX_SUFF(pDevIns), GCPhysAddrCmdTbl, &pAhciReq->aATAPICmd[0], ATAPI_PACKET_SIZE);
    }

    /* We "received" the FIS. Clear the BSY bit in regTFD. */
    if ((pAhciReq->cmdHdr.u32DescInf & AHCI_CMDHDR_C) && (pAhciReq->fFlags & AHCI_REQ_CLEAR_SACT))
    {
        /*
         * We need to send a FIS which clears the busy bit if this is a queued command so that the guest can queue other commands.
         * but this FIS does not assert an interrupt
         */
        ahciSendD2HFis(pAhciPort, pAhciReq, pAhciReq->cmdFis, false);
        pAhciPort->regTFD &= ~AHCI_PORT_TFD_BSY;
    }

    pAhciReq->GCPhysPrdtl = AHCI_RTGCPHYS_FROM_U32(pAhciReq->cmdHdr.u32CmdTblAddrUp, pAhciReq->cmdHdr.u32CmdTblAddr) + AHCI_CMDHDR_PRDT_OFFSET;
    pAhciReq->cPrdtlEntries = AHCI_CMDHDR_PRDTL_ENTRIES(pAhciReq->cmdHdr.u32DescInf);

#ifdef LOG_ENABLED
    /* Print some infos about the FIS. */
    ahciDumpFisInfo(pAhciPort, &pAhciReq->cmdFis[0]);

    /* Print the PRDT */
    ahciLog(("PRDT address %RGp number of entries %u\n", pAhciReq->GCPhysPrdtl, pAhciReq->cPrdtlEntries));
    RTGCPHYS GCPhysPrdtl = pAhciReq->GCPhysPrdtl;

    for (unsigned i = 0; i < pAhciReq->cPrdtlEntries; i++)
    {
        SGLEntry SGEntry;

        ahciLog(("Entry %u at address %RGp\n", i, GCPhysPrdtl));
        PDMDevHlpPhysRead(pAhciPort->CTX_SUFF(pDevIns), GCPhysPrdtl, &SGEntry, sizeof(SGLEntry));

        RTGCPHYS GCPhysDataAddr = AHCI_RTGCPHYS_FROM_U32(SGEntry.u32DBAUp, SGEntry.u32DBA);
        ahciLog(("GCPhysAddr=%RGp Size=%u\n", GCPhysDataAddr, SGEntry.u32DescInf & SGLENTRY_DESCINF_DBC));

        GCPhysPrdtl += sizeof(SGLEntry);
    }
#endif

    return true;
}

/**
 * Submits a given request for execution.
 *
 * @returns Flag whether the request was canceled inbetween.
 * @param   pAhciPort    The port the request is for.
 * @param   pAhciReq     The request to submit.
 * @param   enmTxDir     The request type.
 */
static bool ahciR3ReqSubmit(PAHCIPort pAhciPort, PAHCIREQ pAhciReq, AHCITXDIR enmTxDir)
{
    int rc = VINF_SUCCESS;
    bool fReqCanceled = false;

    if (pAhciPort->fAsyncInterface)
    {
        VBOXDD_AHCI_REQ_SUBMIT(pAhciReq, pAhciReq->enmTxDir, pAhciReq->uOffset, pAhciReq->cbTransfer);
        VBOXDD_AHCI_REQ_SUBMIT_TIMESTAMP(pAhciReq, pAhciReq->tsStart);
        if (enmTxDir == AHCITXDIR_FLUSH)
        {
            rc = pAhciPort->pDrvMediaAsync->pfnStartFlush(pAhciPort->pDrvMediaAsync,
                                                          pAhciReq);
        }
        else if (enmTxDir == AHCITXDIR_TRIM)
        {
            rc = ahciTrimRangesCreate(pAhciPort, pAhciReq);
            if (RT_SUCCESS(rc))
            {
                pAhciPort->Led.Asserted.s.fWriting = pAhciPort->Led.Actual.s.fWriting = 1;
                rc = pAhciPort->pDrvMediaAsync->pfnStartDiscard(pAhciPort->pDrvMediaAsync, pAhciReq->u.Trim.paRanges,
                                                                pAhciReq->u.Trim.cRanges, pAhciReq);
            }
        }
        else if (enmTxDir == AHCITXDIR_READ)
        {
            pAhciPort->Led.Asserted.s.fReading = pAhciPort->Led.Actual.s.fReading = 1;
            rc = pAhciPort->pDrvMediaAsync->pfnStartRead(pAhciPort->pDrvMediaAsync, pAhciReq->uOffset,
                                                         &pAhciReq->u.Io.DataSeg, 1,
                                                         pAhciReq->cbTransfer,
                                                         pAhciReq);
        }
        else
        {
            pAhciPort->Led.Asserted.s.fWriting = pAhciPort->Led.Actual.s.fWriting = 1;
            rc = pAhciPort->pDrvMediaAsync->pfnStartWrite(pAhciPort->pDrvMediaAsync, pAhciReq->uOffset,
                                                          &pAhciReq->u.Io.DataSeg, 1,
                                                          pAhciReq->cbTransfer,
                                                          pAhciReq);
        }
        if (rc == VINF_VD_ASYNC_IO_FINISHED)
            fReqCanceled = ahciTransferComplete(pAhciPort, pAhciReq, VINF_SUCCESS);
        else if (RT_FAILURE(rc) && rc != VERR_VD_ASYNC_IO_IN_PROGRESS)
            fReqCanceled = ahciTransferComplete(pAhciPort, pAhciReq, rc);
    }
    else
    {
        if (enmTxDir == AHCITXDIR_FLUSH)
            rc = pAhciPort->pDrvMedia->pfnFlush(pAhciPort->pDrvMedia);
        else if (enmTxDir == AHCITXDIR_TRIM)
        {
            rc = ahciTrimRangesCreate(pAhciPort, pAhciReq);
            if (RT_SUCCESS(rc))
            {
                pAhciPort->Led.Asserted.s.fWriting = pAhciPort->Led.Actual.s.fWriting = 1;
                rc = pAhciPort->pDrvMedia->pfnDiscard(pAhciPort->pDrvMedia, pAhciReq->u.Trim.paRanges,
                                                      pAhciReq->u.Trim.cRanges);
                pAhciPort->Led.Asserted.s.fWriting = pAhciPort->Led.Actual.s.fWriting = 0;
            }
        }
        else if (enmTxDir == AHCITXDIR_READ)
        {
            pAhciPort->Led.Asserted.s.fReading = pAhciPort->Led.Actual.s.fReading = 1;
            rc = pAhciPort->pDrvMedia->pfnRead(pAhciPort->pDrvMedia, pAhciReq->uOffset,
                                               pAhciReq->u.Io.DataSeg.pvSeg,
                                               pAhciReq->cbTransfer);
            pAhciPort->Led.Asserted.s.fReading = pAhciPort->Led.Actual.s.fReading = 0;
        }
        else
        {
            pAhciPort->Led.Asserted.s.fWriting = pAhciPort->Led.Actual.s.fWriting = 1;
            rc = pAhciPort->pDrvMedia->pfnWrite(pAhciPort->pDrvMedia, pAhciReq->uOffset,
                                                pAhciReq->u.Io.DataSeg.pvSeg,
                                                pAhciReq->cbTransfer);
            pAhciPort->Led.Asserted.s.fWriting = pAhciPort->Led.Actual.s.fWriting = 0;
        }
        fReqCanceled = ahciTransferComplete(pAhciPort, pAhciReq, rc);
    }

    return fReqCanceled;
}

/**
 * Prepares the command for execution coping it from guest memory and doing a few
 * validation checks on it.
 *
 * @returns Whether the command was successfully fetched from guest memory and
 *          can be continued.
 * @param   pAhciPort    The AHCI port the request is for.
 * @param   pAhciReq     Request structure to copy the command to.
 */
static bool ahciR3CmdPrepare(PAHCIPort pAhciPort, PAHCIREQ pAhciReq)
{
    pAhciReq->tsStart       = RTTimeMilliTS();
    pAhciReq->uATARegStatus = 0;
    pAhciReq->uATARegError  = 0;

    /* Set current command slot */
    ASMAtomicWriteU32(&pAhciPort->u32CurrentCommandSlot, pAhciReq->uTag);
    ASMAtomicWritePtr(&pAhciPort->aActiveTasks[pAhciReq->uTag], pAhciReq);

    bool fContinue = ahciPortTaskGetCommandFis(pAhciPort, pAhciReq);
    if (fContinue)
    {
        /* Mark the task as processed by the HBA if this is a queued task so that it doesn't occur in the CI register anymore. */
        if (pAhciPort->regSACT & RT_BIT_32(pAhciReq->uTag))
        {
            pAhciReq->fFlags |= AHCI_REQ_CLEAR_SACT;
            ASMAtomicOrU32(&pAhciPort->u32TasksFinished, RT_BIT_32(pAhciReq->uTag));
        }

        if (pAhciReq->cmdFis[AHCI_CMDFIS_BITS] & AHCI_CMDFIS_C)
        {
            /*
             * It is possible that the request counter can get one higher than the maximum because
             * the request counter is decremented after the guest was notified about the completed
             * request (see @bugref{7859}). If the completing thread is preempted in between the
             * guest might already issue another request before the request counter is decremented
             * which would trigger the following assertion incorrectly in the past.
             */
            AssertLogRelMsg(ASMAtomicReadU32(&pAhciPort->cTasksActive) <= AHCI_NR_COMMAND_SLOTS,
                            ("AHCI#%uP%u: There are more than %u (+1) requests active",
                             pAhciPort->CTX_SUFF(pDevIns)->iInstance, pAhciPort->iLUN,
                             AHCI_NR_COMMAND_SLOTS));
            ASMAtomicIncU32(&pAhciPort->cTasksActive);
        }
        else
        {
            /* If the reset bit is set put the device into reset state. */
            if (pAhciReq->cmdFis[AHCI_CMDFIS_CTL] & AHCI_CMDFIS_CTL_SRST)
            {
                ahciLog(("%s: Setting device into reset state\n", __FUNCTION__));
                pAhciPort->fResetDevice = true;
                ahciSendD2HFis(pAhciPort, pAhciReq, pAhciReq->cmdFis, true);
            }
            else if (pAhciPort->fResetDevice) /* The bit is not set and we are in a reset state. */
                ahciFinishStorageDeviceReset(pAhciPort, pAhciReq);
            else /* We are not in a reset state update the control registers. */
                AssertMsgFailed(("%s: Update the control register\n", __FUNCTION__));

            fContinue = false;
        }
    }
    else
    {
        /*
         * Couldn't find anything in either the AHCI or SATA spec which
         * indicates what should be done if the FIS is not read successfully.
         * The closest thing is in the state machine, stating that the device
         * should go into idle state again (SATA spec 1.0 chapter 8.7.1).
         * Do the same here and ignore any corrupt FIS types, after all
         * the guest messed up everything and this behavior is undefined.
         */
        fContinue = false;
    }

    return fContinue;
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
    PAHCI                     pThis = PDMINS_2_DATA(pDevIns, PAHCI);
    PAHCIPort                 pAhciPort = &pThis->ahciPort[pNotifierItem->iPort];
    int                       rc = VINF_SUCCESS;

    ahciLog(("%s: Got notification from GC\n", __FUNCTION__));
    /* Notify the async IO thread. */
    rc = SUPSemEventSignal(pThis->pSupDrvSession, pAhciPort->hEvtProcess);
    AssertRC(rc);

    return true;
}

/* The async IO thread for one port. */
static DECLCALLBACK(int) ahciAsyncIOLoop(PPDMDEVINS pDevIns, PPDMTHREAD pThread)
{
    PAHCIPort pAhciPort = (PAHCIPort)pThread->pvUser;
    PAHCI     pAhci     = pAhciPort->CTX_SUFF(pAhci);
    int rc = VINF_SUCCESS;
    uint64_t u64StartTime = 0;
    uint64_t u64StopTime  = 0;
    uint32_t uIORequestsProcessed = 0;
    uint32_t uIOsPerSec = 0;
    uint32_t fTasksToProcess = 0;

    ahciLog(("%s: Port %d entering async IO loop.\n", __FUNCTION__, pAhciPort->iLUN));

    if (pThread->enmState == PDMTHREADSTATE_INITIALIZING)
        return VINF_SUCCESS;

    while (pThread->enmState == PDMTHREADSTATE_RUNNING)
    {
        unsigned idx = 0;
        uint32_t u32Tasks = 0;
        uint32_t u32RegHbaCtrl = 0;

        ASMAtomicWriteBool(&pAhciPort->fWrkThreadSleeping, true);
        u32Tasks = ASMAtomicXchgU32(&pAhciPort->u32TasksNew, 0);
        if (!u32Tasks)
        {
            Assert(ASMAtomicReadBool(&pAhciPort->fWrkThreadSleeping));
            rc = SUPSemEventWaitNoResume(pAhci->pSupDrvSession, pAhciPort->hEvtProcess, RT_INDEFINITE_WAIT);
            AssertLogRelMsgReturn(RT_SUCCESS(rc) || rc == VERR_INTERRUPTED, ("%Rrc\n", rc), rc);
            if (RT_UNLIKELY(pThread->enmState != PDMTHREADSTATE_RUNNING))
                break;
            LogFlowFunc(("Woken up with rc=%Rrc\n", rc));
            u32Tasks = ASMAtomicXchgU32(&pAhciPort->u32TasksNew, 0);
        }

        ASMAtomicWriteBool(&pAhciPort->fWrkThreadSleeping, false);
        ASMAtomicIncU32(&pAhci->cThreadsActive);

        /* Check whether the thread should be suspended. */
        if (pAhci->fSignalIdle)
        {
            if (!ASMAtomicDecU32(&pAhci->cThreadsActive))
                PDMDevHlpAsyncNotificationCompleted(pAhciPort->pDevInsR3);
            continue;
        }

        /*
         * Check whether the global host controller bit is set and go to sleep immediately again
         * if it is set.
         */
        u32RegHbaCtrl = ASMAtomicReadU32(&pAhci->regHbaCtrl);
        if (   u32RegHbaCtrl & AHCI_HBA_CTRL_HR
            && !ASMAtomicDecU32(&pAhci->cThreadsActive))
        {
            ahciHBAReset(pAhci);
            if (pAhci->fSignalIdle)
                PDMDevHlpAsyncNotificationCompleted(pAhciPort->pDevInsR3);
            continue;
        }

        idx = ASMBitFirstSetU32(u32Tasks);
        while (   idx
               && !pAhciPort->fPortReset)
        {
            bool fReqCanceled = false;
            AHCITXDIR enmTxDir;
            PAHCIREQ pAhciReq;

            /* Decrement to get the slot number. */
            idx--;
            ahciLog(("%s: Processing command at slot %d\n", __FUNCTION__, idx));

            /* Check whether the request is already active and ignore. */
            if (ASMAtomicReadPtr((void * volatile *)&pAhciPort->aActiveTasks[idx]))
            {
                ahciLog(("%s: Ignoring command at slot %d because it is already active\n", __FUNCTION__, idx));
                continue;
            }

            pAhciReq = ahciR3ReqAlloc(pAhciPort);
            if (RT_LIKELY(pAhciReq))
            {
                pAhciReq->uTag          = idx;
                pAhciReq->fFlags        = 0;

                bool fContinue = ahciR3CmdPrepare(pAhciPort, pAhciReq);
                if (fContinue)
                {
                    enmTxDir = ahciProcessCmd(pAhciPort, pAhciReq, pAhciReq->cmdFis);
                    pAhciReq->enmTxDir = enmTxDir;

                    if (enmTxDir != AHCITXDIR_NONE)
                    {
                        if (   enmTxDir != AHCITXDIR_FLUSH
                            && enmTxDir != AHCITXDIR_TRIM)
                        {
                            STAM_REL_COUNTER_INC(&pAhciPort->StatDMA);

                            rc = ahciIoBufAllocate(pAhciPort, pAhciReq, pAhciReq->cbTransfer);
                            if (RT_FAILURE(rc))
                            {
                                /* In case we can't allocate enough memory fail the request with an overflow error. */
                                AssertMsgFailed(("%s: Failed to process command %Rrc\n", __FUNCTION__, rc));
                                pAhciReq->fFlags |= AHCI_REQ_OVERFLOW;
                            }
                        }

                        if (!(pAhciReq->fFlags & AHCI_REQ_OVERFLOW))
                            fReqCanceled = ahciR3ReqSubmit(pAhciPort, pAhciReq, enmTxDir);
                        else /* Overflow is handled in completion routine. */
                            fReqCanceled = ahciTransferComplete(pAhciPort, pAhciReq, VINF_SUCCESS);
                    }
                    else
                        fReqCanceled = ahciTransferComplete(pAhciPort, pAhciReq, VINF_SUCCESS);
                } /* Command */
                else
                {
                    ASMAtomicWriteNullPtr(&pAhciPort->aActiveTasks[pAhciReq->uTag]);
                    ahciR3ReqFree(pAhciPort, pAhciReq);
                }
            }
            else /* !Request allocated, use on stack variant to signal the error. */
            {
                AHCIREQ Req;
                Req.uTag   = idx;
                Req.fFlags = AHCI_REQ_IS_ON_STACK;

                bool fContinue = ahciR3CmdPrepare(pAhciPort, &Req);
                if (fContinue)
                    fReqCanceled = ahciTransferComplete(pAhciPort, &Req, VERR_NO_MEMORY);
            }

            /*
             * Don't process other requests if the last one was canceled,
             * the others are not valid anymore.
             */
            if (fReqCanceled)
                break;

            u32Tasks &= ~RT_BIT_32(idx); /* Clear task bit. */
            idx = ASMBitFirstSetU32(u32Tasks);
        } /* while tasks available */

        /* Check whether a port reset was active. */
        if (   ASMAtomicReadBool(&pAhciPort->fPortReset)
            && (pAhciPort->regSCTL & AHCI_PORT_SCTL_DET) == AHCI_PORT_SCTL_DET_NINIT)
            ahciPortResetFinish(pAhciPort);

        /*
         * Check whether a host controller reset is pending and execute the reset
         * if this is the last active thread.
         */
        u32RegHbaCtrl = ASMAtomicReadU32(&pAhci->regHbaCtrl);
        uint32_t cThreadsActive = ASMAtomicDecU32(&pAhci->cThreadsActive);
        if (   (u32RegHbaCtrl & AHCI_HBA_CTRL_HR)
            && !cThreadsActive)
            ahciHBAReset(pAhci);

        if (!cThreadsActive && pAhci->fSignalIdle)
            PDMDevHlpAsyncNotificationCompleted(pAhciPort->pDevInsR3);
    } /* While running */

    ahciLog(("%s: Port %d async IO thread exiting\n", __FUNCTION__, pAhciPort->iLUN));
    return VINF_SUCCESS;
}

/**
 * Unblock the async I/O thread so it can respond to a state change.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pThread     The send thread.
 */
static DECLCALLBACK(int) ahciAsyncIOLoopWakeUp(PPDMDEVINS pDevIns, PPDMTHREAD pThread)
{
    PAHCI pThis = PDMINS_2_DATA(pDevIns, PAHCI);
    PAHCIPort pAhciPort = (PAHCIPort)pThread->pvUser;
    return SUPSemEventSignal(pThis->pSupDrvSession, pAhciPort->hEvtProcess);
}

/* -=-=-=-=- DBGF -=-=-=-=- */

/**
 * AHCI status info callback.
 *
 * @param   pDevIns     The device instance.
 * @param   pHlp        The output helpers.
 * @param   pszArgs     The arguments.
 */
static DECLCALLBACK(void) ahciR3Info(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PAHCI pThis = PDMINS_2_DATA(pDevIns, PAHCI);

    /*
     * Show info.
     */
    pHlp->pfnPrintf(pHlp,
                    "%s#%d: mmio=%RGp ports=%u GC=%RTbool R0=%RTbool\n",
                    pDevIns->pReg->szName,
                    pDevIns->iInstance,
                    pThis->MMIOBase,
                    pThis->cPortsImpl,
                    pThis->fGCEnabled ? true : false,
                    pThis->fR0Enabled ? true : false);

    /*
     * Show global registers.
     */
    pHlp->pfnPrintf(pHlp, "HbaCap=%#x\n", pThis->regHbaCap);
    pHlp->pfnPrintf(pHlp, "HbaCtrl=%#x\n", pThis->regHbaCtrl);
    pHlp->pfnPrintf(pHlp, "HbaIs=%#x\n", pThis->regHbaIs);
    pHlp->pfnPrintf(pHlp, "HbaPi=%#x", pThis->regHbaPi);
    pHlp->pfnPrintf(pHlp, "HbaVs=%#x\n", pThis->regHbaVs);
    pHlp->pfnPrintf(pHlp, "HbaCccCtl=%#x\n", pThis->regHbaCccCtl);
    pHlp->pfnPrintf(pHlp, "HbaCccPorts=%#x\n", pThis->regHbaCccPorts);
    pHlp->pfnPrintf(pHlp, "PortsInterrupted=%#x\n", pThis->u32PortsInterrupted);

    /*
     * Per port data.
     */
    for (unsigned i = 0; i < pThis->cPortsImpl; i++)
    {
        PAHCIPort pThisPort = &pThis->ahciPort[i];

        pHlp->pfnPrintf(pHlp, "Port %d: async=%RTbool device-attached=%RTbool\n",
                        pThisPort->iLUN, pThisPort->fAsyncInterface, pThisPort->pDrvBase != NULL);
        pHlp->pfnPrintf(pHlp, "PortClb=%#x\n", pThisPort->regCLB);
        pHlp->pfnPrintf(pHlp, "PortClbU=%#x\n", pThisPort->regCLBU);
        pHlp->pfnPrintf(pHlp, "PortFb=%#x\n", pThisPort->regFB);
        pHlp->pfnPrintf(pHlp, "PortFbU=%#x\n", pThisPort->regFBU);
        pHlp->pfnPrintf(pHlp, "PortIs=%#x\n", pThisPort->regIS);
        pHlp->pfnPrintf(pHlp, "PortIe=%#x\n", pThisPort->regIE);
        pHlp->pfnPrintf(pHlp, "PortCmd=%#x\n", pThisPort->regCMD);
        pHlp->pfnPrintf(pHlp, "PortTfd=%#x\n", pThisPort->regTFD);
        pHlp->pfnPrintf(pHlp, "PortSig=%#x\n", pThisPort->regSIG);
        pHlp->pfnPrintf(pHlp, "PortSSts=%#x\n", pThisPort->regSSTS);
        pHlp->pfnPrintf(pHlp, "PortSCtl=%#x\n", pThisPort->regSCTL);
        pHlp->pfnPrintf(pHlp, "PortSErr=%#x\n", pThisPort->regSERR);
        pHlp->pfnPrintf(pHlp, "PortSAct=%#x\n", pThisPort->regSACT);
        pHlp->pfnPrintf(pHlp, "PortCi=%#x\n", pThisPort->regCI);
        pHlp->pfnPrintf(pHlp, "PortPhysClb=%RGp\n", pThisPort->GCPhysAddrClb);
        pHlp->pfnPrintf(pHlp, "PortPhysFb=%RGp\n", pThisPort->GCPhysAddrFb);
        pHlp->pfnPrintf(pHlp, "PortActTasksActive=%u\n", pThisPort->cTasksActive);
        pHlp->pfnPrintf(pHlp, "PortPoweredOn=%RTbool\n", pThisPort->fPoweredOn);
        pHlp->pfnPrintf(pHlp, "PortSpunUp=%RTbool\n", pThisPort->fSpunUp);
        pHlp->pfnPrintf(pHlp, "PortFirstD2HFisSend=%RTbool\n", pThisPort->fFirstD2HFisSend);
        pHlp->pfnPrintf(pHlp, "PortATAPI=%RTbool\n", pThisPort->fATAPI);
        pHlp->pfnPrintf(pHlp, "PortTasksFinished=%#x\n", pThisPort->u32TasksFinished);
        pHlp->pfnPrintf(pHlp, "PortQueuedTasksFinished=%#x\n", pThisPort->u32QueuedTasksFinished);
        pHlp->pfnPrintf(pHlp, "PortTasksNew=%#x\n", pThisPort->u32TasksNew);
        pHlp->pfnPrintf(pHlp, "\n");
    }
}

/* -=-=-=-=- Helper -=-=-=-=- */

/**
 * Checks if all asynchronous I/O is finished, both AHCI and IDE.
 *
 * Used by ahciR3Reset, ahciR3Suspend and ahciR3PowerOff. ahciR3SavePrep makes
 * use of it in strict builds (which is why it's up here).
 *
 * @returns true if quiesced, false if busy.
 * @param   pDevIns         The device instance.
 */
static bool ahciR3AllAsyncIOIsFinished(PPDMDEVINS pDevIns)
{
    PAHCI pThis = PDMINS_2_DATA(pDevIns, PAHCI);

    if (pThis->cThreadsActive)
        return false;

    for (uint32_t i = 0; i < RT_ELEMENTS(pThis->ahciPort); i++)
    {
        PAHCIPort pThisPort = &pThis->ahciPort[i];
        if (pThisPort->pDrvBase)
        {
            if (   (pThisPort->cTasksActive != 0)
                || (pThisPort->u32TasksNew != 0))
               return false;
        }
    }
    return true;
}

/* -=-=-=-=- Saved State -=-=-=-=- */

/**
 * @callback_method_impl{FNSSMDEVSAVEPREP}
 */
static DECLCALLBACK(int) ahciR3SavePrep(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    Assert(ahciR3AllAsyncIOIsFinished(pDevIns));
    return VINF_SUCCESS;
}

/**
 * @callback_method_impl{FNSSMDEVLOADPREP}
 */
static DECLCALLBACK(int) ahciR3LoadPrep(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    Assert(ahciR3AllAsyncIOIsFinished(pDevIns));
    return VINF_SUCCESS;
}

/**
 * @callback_method_impl{FNSSMDEVLIVEEXEC}
 */
static DECLCALLBACK(int) ahciR3LiveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uPass)
{
    PAHCI pThis = PDMINS_2_DATA(pDevIns, PAHCI);

    /* config. */
    SSMR3PutU32(pSSM, pThis->cPortsImpl);
    for (uint32_t i = 0; i < AHCI_MAX_NR_PORTS_IMPL; i++)
    {
        SSMR3PutBool(pSSM, pThis->ahciPort[i].pDrvBase != NULL);
        SSMR3PutBool(pSSM, pThis->ahciPort[i].fHotpluggable);
        SSMR3PutStrZ(pSSM, pThis->ahciPort[i].szSerialNumber);
        SSMR3PutStrZ(pSSM, pThis->ahciPort[i].szFirmwareRevision);
        SSMR3PutStrZ(pSSM, pThis->ahciPort[i].szModelNumber);
    }

    static const char *s_apszIdeEmuPortNames[4] = { "PrimaryMaster", "PrimarySlave", "SecondaryMaster", "SecondarySlave" };
    for (size_t i = 0; i < RT_ELEMENTS(s_apszIdeEmuPortNames); i++)
    {
        uint32_t iPort;
        int rc = CFGMR3QueryU32Def(pDevIns->pCfg, s_apszIdeEmuPortNames[i], &iPort, i);
        AssertRCReturn(rc, rc);
        SSMR3PutU32(pSSM, iPort);
    }

    return VINF_SSM_DONT_CALL_AGAIN;
}

/**
 * @callback_method_impl{FNSSMDEVSAVEEXEC}
 */
static DECLCALLBACK(int) ahciR3SaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PAHCI pThis = PDMINS_2_DATA(pDevIns, PAHCI);
    uint32_t i;
    int rc;

    Assert(!pThis->f8ByteMMIO4BytesWrittenSuccessfully);

    /* The config */
    rc = ahciR3LiveExec(pDevIns, pSSM, SSM_PASS_FINAL);
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
    SSMR3PutBool(pSSM, pThis->fLegacyPortResetMethod);

    /* Now every port. */
    for (i = 0; i < AHCI_MAX_NR_PORTS_IMPL; i++)
    {
        Assert(pThis->ahciPort[i].cTasksActive == 0);
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
        SSMR3PutBool(pSSM, pThis->ahciPort[i].fPoweredOn);
        SSMR3PutBool(pSSM, pThis->ahciPort[i].fSpunUp);
        SSMR3PutU32(pSSM, pThis->ahciPort[i].u32TasksFinished);
        SSMR3PutU32(pSSM, pThis->ahciPort[i].u32QueuedTasksFinished);
        SSMR3PutU32(pSSM, pThis->ahciPort[i].u32CurrentCommandSlot);

        /* ATAPI saved state. */
        SSMR3PutBool(pSSM, pThis->ahciPort[i].fATAPI);
        SSMR3PutMem(pSSM, &pThis->ahciPort[i].abATAPISense[0], sizeof(pThis->ahciPort[i].abATAPISense));
        SSMR3PutU8(pSSM, pThis->ahciPort[i].cNotifiedMediaChange);
        SSMR3PutU32(pSSM, pThis->ahciPort[i].MediaEventStatus);
    }

    return SSMR3PutU32(pSSM, UINT32_MAX); /* sanity/terminator */
}

/**
 * Loads a saved legacy ATA emulated device state.
 *
 * @returns VBox status code.
 * @param   pSSM  The handle to the saved state.
 */
static int ahciR3LoadLegacyEmulationState(PSSMHANDLE pSSM)
{
    int             rc;
    uint32_t        u32Version;
    uint32_t        u32;
    uint32_t        u32IOBuffer;

    /* Test for correct version. */
    rc = SSMR3GetU32(pSSM, &u32Version);
    AssertRCReturn(rc, rc);
    LogFlow(("LoadOldSavedStates u32Version = %d\n", u32Version));

    if (   u32Version != ATA_CTL_SAVED_STATE_VERSION
        && u32Version != ATA_CTL_SAVED_STATE_VERSION_WITHOUT_FULL_SENSE
        && u32Version != ATA_CTL_SAVED_STATE_VERSION_WITHOUT_EVENT_STATUS)
    {
        AssertMsgFailed(("u32Version=%d\n", u32Version));
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }

    SSMR3Skip(pSSM, 19 + 5 * sizeof(bool) + sizeof(BMDMAState));

    for (uint32_t j = 0; j < 2; j++)
    {
        SSMR3Skip(pSSM, 88 + 5 * sizeof(bool) );

        if (u32Version > ATA_CTL_SAVED_STATE_VERSION_WITHOUT_FULL_SENSE)
            SSMR3Skip(pSSM, 64);
        else
            SSMR3Skip(pSSM, 2);
        /** @todo triple-check this hack after passthrough is working */
        SSMR3Skip(pSSM, 1);

        if (u32Version > ATA_CTL_SAVED_STATE_VERSION_WITHOUT_EVENT_STATUS)
            SSMR3Skip(pSSM, 4);

        SSMR3Skip(pSSM, sizeof(PDMLED));
        SSMR3GetU32(pSSM, &u32IOBuffer);
        if (u32IOBuffer)
            SSMR3Skip(pSSM, u32IOBuffer);
    }

    rc = SSMR3GetU32(pSSM, &u32);
    if (RT_FAILURE(rc))
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
 * @callback_method_impl{FNSSMDEVLOADEXEC}
 */
static DECLCALLBACK(int) ahciR3LoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    PAHCI pThis = PDMINS_2_DATA(pDevIns, PAHCI);
    uint32_t u32;
    int rc;

    if (   uVersion > AHCI_SAVED_STATE_VERSION
        || uVersion < AHCI_SAVED_STATE_VERSION_VBOX_30)
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;

    /* Deal with the priod after removing the saved IDE bits where the saved
       state version remained unchanged. */
    if (   uVersion == AHCI_SAVED_STATE_VERSION_IDE_EMULATION
        && SSMR3HandleRevision(pSSM) >= 79045
        && SSMR3HandleRevision(pSSM) <  79201)
        uVersion++;

    /*
     * Check whether we have to resort to the legacy port reset method to
     * prevent older BIOS versions from failing after a reset.
     */
    if (uVersion <= AHCI_SAVED_STATE_VERSION_PRE_PORT_RESET_CHANGES)
        pThis->fLegacyPortResetMethod = true;

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
                return SSMR3SetCfgError(pSSM, RT_SRC_POS, N_("Config mismatch: cPortsImpl - saved=%u config=%u"),
                                        u32, pThis->cPortsImpl);
        }

        for (uint32_t i = 0; i < AHCI_MAX_NR_PORTS_IMPL; i++)
        {
            bool fInUse;
            rc = SSMR3GetBool(pSSM, &fInUse);
            AssertRCReturn(rc, rc);
            if (fInUse != (pThis->ahciPort[i].pDrvBase != NULL))
                return SSMR3SetCfgError(pSSM, RT_SRC_POS,
                                        N_("The %s VM is missing a device on port %u. Please make sure the source and target VMs have compatible storage configurations"),
                                        fInUse ? "target" : "source", i );

            if (uVersion > AHCI_SAVED_STATE_VERSION_PRE_HOTPLUG_FLAG)
            {
                bool fHotpluggable;
                rc = SSMR3GetBool(pSSM, &fHotpluggable);
                AssertRCReturn(rc, rc);
                if (fHotpluggable != pThis->ahciPort[i].fHotpluggable)
                    return SSMR3SetCfgError(pSSM, RT_SRC_POS,
                                            N_("AHCI: Port %u config mismatch: Hotplug flag - saved=%RTbool config=%RTbool\n"),
                                            i, fHotpluggable, pThis->ahciPort[i].fHotpluggable);
            }
            else
                Assert(pThis->ahciPort[i].fHotpluggable);

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
            rc = CFGMR3QueryU32Def(pDevIns->pCfg, s_apszIdeEmuPortNames[i], &iPort, i);
            AssertRCReturn(rc, rc);

            uint32_t iPortSaved;
            rc = SSMR3GetU32(pSSM, &iPortSaved);
            AssertRCReturn(rc, rc);

            if (iPortSaved != iPort)
                return SSMR3SetCfgError(pSSM, RT_SRC_POS, N_("IDE %s config mismatch: saved=%u config=%u"),
                                        s_apszIdeEmuPortNames[i], iPortSaved, iPort);
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

        SSMR3GetU32(pSSM, (uint32_t *)&pThis->u32PortsInterrupted);
        SSMR3GetBool(pSSM, &pThis->fReset);
        SSMR3GetBool(pSSM, &pThis->f64BitAddr);
        SSMR3GetBool(pSSM, &pThis->fR0Enabled);
        SSMR3GetBool(pSSM, &pThis->fGCEnabled);
        if (uVersion > AHCI_SAVED_STATE_VERSION_PRE_PORT_RESET_CHANGES)
            SSMR3GetBool(pSSM, &pThis->fLegacyPortResetMethod);

        /* Now every port. */
        for (uint32_t i = 0; i < AHCI_MAX_NR_PORTS_IMPL; i++)
        {
            PAHCIPort pAhciPort = &pThis->ahciPort[i];

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

            if (uVersion < AHCI_SAVED_STATE_VERSION_IDE_EMULATION)
            {
                /* The old positions in the FIFO, not required. */
                SSMR3Skip(pSSM, 2*sizeof(uint8_t));
            }
            SSMR3GetBool(pSSM, &pThis->ahciPort[i].fPoweredOn);
            SSMR3GetBool(pSSM, &pThis->ahciPort[i].fSpunUp);
            SSMR3GetU32(pSSM, (uint32_t *)&pThis->ahciPort[i].u32TasksFinished);
            SSMR3GetU32(pSSM, (uint32_t *)&pThis->ahciPort[i].u32QueuedTasksFinished);

            if (uVersion >= AHCI_SAVED_STATE_VERSION_IDE_EMULATION)
                SSMR3GetU32(pSSM, (uint32_t *)&pThis->ahciPort[i].u32CurrentCommandSlot);

            if (uVersion > AHCI_SAVED_STATE_VERSION_PRE_ATAPI)
            {
                SSMR3GetBool(pSSM, &pThis->ahciPort[i].fATAPI);
                SSMR3GetMem(pSSM, pThis->ahciPort[i].abATAPISense, sizeof(pThis->ahciPort[i].abATAPISense));
                SSMR3GetU8(pSSM, &pThis->ahciPort[i].cNotifiedMediaChange);
                SSMR3GetU32(pSSM, (uint32_t*)&pThis->ahciPort[i].MediaEventStatus);
            }
            else if (pThis->ahciPort[i].fATAPI)
                return SSMR3SetCfgError(pSSM, RT_SRC_POS, N_("Config mismatch: atapi - saved=false config=true"));

            /* Check if we have tasks pending. */
            uint32_t fTasksOutstanding = pAhciPort->regCI & ~pAhciPort->u32TasksFinished;
            uint32_t fQueuedTasksOutstanding = pAhciPort->regSACT & ~pAhciPort->u32QueuedTasksFinished;

            pAhciPort->u32TasksNew = fTasksOutstanding | fQueuedTasksOutstanding;

            if (pAhciPort->u32TasksNew)
            {
                /*
                 * There are tasks pending. The VM was saved after a task failed
                 * because of non-fatal error. Set the redo flag.
                 */
                pAhciPort->fRedo = true;
            }
        }

        if (uVersion <= AHCI_SAVED_STATE_VERSION_IDE_EMULATION)
        {
            for (uint32_t i = 0; i < 2; i++)
            {
                rc = ahciR3LoadLegacyEmulationState(pSSM);
                if(RT_FAILURE(rc))
                    return rc;
            }
        }

        rc = SSMR3GetU32(pSSM, &u32);
        if (RT_FAILURE(rc))
            return rc;
        AssertMsgReturn(u32 == UINT32_MAX, ("%#x\n", u32), VERR_SSM_DATA_UNIT_FORMAT_CHANGED);
    }

    return VINF_SUCCESS;
}

/* -=-=-=-=- device PDM interface -=-=-=-=- */

static DECLCALLBACK(void) ahciR3Relocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
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
}

/**
 * SCSI_GET_EVENT_STATUS_NOTIFICATION should return "medium removed" event
 * from now on, regardless if there was a medium inserted or not.
 */
static void ahciMediumRemoved(PAHCIPort pAhciPort)
{
    ASMAtomicWriteU32(&pAhciPort->MediaEventStatus, ATA_EVENT_STATUS_MEDIA_REMOVED);
}

/**
 * SCSI_GET_EVENT_STATUS_NOTIFICATION should return "medium inserted". If
 * there was already a medium inserted, don't forget to send the "medium
 * removed" event first.
 */
static void ahciMediumInserted(PAHCIPort pAhciPort)
{
    uint32_t OldStatus, NewStatus;
    do
    {
        OldStatus = ASMAtomicReadU32(&pAhciPort->MediaEventStatus);
        switch (OldStatus)
        {
            case ATA_EVENT_STATUS_MEDIA_CHANGED:
            case ATA_EVENT_STATUS_MEDIA_REMOVED:
                /* no change, we will send "medium removed" + "medium inserted" */
                NewStatus = ATA_EVENT_STATUS_MEDIA_CHANGED;
                break;
            default:
                NewStatus = ATA_EVENT_STATUS_MEDIA_NEW;
                break;
        }
    } while (!ASMAtomicCmpXchgU32(&pAhciPort->MediaEventStatus, NewStatus, OldStatus));
}

/**
 * Called when a media is mounted.
 *
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 */
static DECLCALLBACK(void) ahciR3MountNotify(PPDMIMOUNTNOTIFY pInterface)
{
    PAHCIPort pAhciPort = PDMIMOUNTNOTIFY_2_PAHCIPORT(pInterface);
    Log(("%s: changing LUN#%d\n", __FUNCTION__, pAhciPort->iLUN));

    /* Ignore the call if we're called while being attached. */
    if (!pAhciPort->pDrvMedia)
        return;

    if (pAhciPort->fATAPI)
    {
        pAhciPort->cTotalSectors = pAhciPort->pDrvMedia->pfnGetSize(pAhciPort->pDrvMedia) / 2048;

        LogRel(("AHCI: LUN#%d: CD/DVD, total number of sectors %Ld, passthrough unchanged\n", pAhciPort->iLUN, pAhciPort->cTotalSectors));

        /* Report media changed in TEST UNIT and other (probably incorrect) places. */
        if (pAhciPort->cNotifiedMediaChange < 2)
            pAhciPort->cNotifiedMediaChange = 2;
        ahciMediumInserted(pAhciPort);
        ahciMediumTypeSet(pAhciPort, ATA_MEDIA_TYPE_UNKNOWN);
    }
    else
        AssertMsgFailed(("Hard disks don't have a mount interface!\n"));
}

/**
 * Called when a media is unmounted
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 */
static DECLCALLBACK(void) ahciR3UnmountNotify(PPDMIMOUNTNOTIFY pInterface)
{
    PAHCIPort pAhciPort = PDMIMOUNTNOTIFY_2_PAHCIPORT(pInterface);
    Log(("%s:\n", __FUNCTION__));

    /* Free all cached I/O tasks. */
    ahciR3PortCachedReqsFree(pAhciPort);

    pAhciPort->cTotalSectors = 0;

    if (pAhciPort->fATAPI)
    {
        /*
         * Whatever I do, XP will not use the GET MEDIA STATUS nor the EVENT stuff.
         * However, it will respond to TEST UNIT with a 0x6 0x28 (media changed) sense code.
         * So, we'll give it 4 TEST UNIT command to catch up, two which the media is not
         * present and 2 in which it is changed.
         */
        pAhciPort->cNotifiedMediaChange = 4;
        ahciMediumRemoved(pAhciPort);
        ahciMediumTypeSet(pAhciPort, ATA_MEDIA_TYPE_UNKNOWN);
    }
    else
        AssertMsgFailed(("Hard disks don't have a mount interface!\n"));
}

/**
 * Configure the attached device for a port.
 *
 * Used by ahciR3Construct and ahciR3Attach.
 *
 * @returns VBox status code
 * @param   pDevIns     The device instance data.
 * @param   pAhciPort   The port for which the device is to be configured.
 */
static int ahciR3ConfigureLUN(PPDMDEVINS pDevIns, PAHCIPort pAhciPort)
{
    int          rc = VINF_SUCCESS;
    PDMMEDIATYPE enmType;

    /*
     * Query the block and blockbios interfaces.
     */
    pAhciPort->pDrvMedia = PDMIBASE_QUERY_INTERFACE(pAhciPort->pDrvBase, PDMIMEDIA);
    if (!pAhciPort->pDrvMedia)
    {
        AssertMsgFailed(("Configuration error: LUN#%d hasn't a block interface!\n", pAhciPort->iLUN));
        return VERR_PDM_MISSING_INTERFACE;
    }

    pAhciPort->pDrvMount = PDMIBASE_QUERY_INTERFACE(pAhciPort->pDrvBase, PDMIMOUNT);

    /* Try to get the optional async block interface. */
    pAhciPort->pDrvMediaAsync = PDMIBASE_QUERY_INTERFACE(pAhciPort->pDrvBase, PDMIMEDIAASYNC);

    /*
     * Validate type.
     */
    enmType = pAhciPort->pDrvMedia->pfnGetType(pAhciPort->pDrvMedia);

    if (   enmType != PDMMEDIATYPE_HARD_DISK
        && enmType != PDMMEDIATYPE_CDROM
        && enmType != PDMMEDIATYPE_DVD)
    {
        AssertMsgFailed(("Configuration error: LUN#%d isn't a disk or cd/dvd. enmType=%d\n", pAhciPort->iLUN, enmType));
        return VERR_PDM_UNSUPPORTED_BLOCK_TYPE;
    }

    if (   (enmType == PDMMEDIATYPE_CDROM || enmType == PDMMEDIATYPE_DVD)
        && !pAhciPort->pDrvMount)
    {
        AssertMsgFailed(("Internal error: CD/DVD-ROM without a mountable interface\n"));
        return VERR_INTERNAL_ERROR;
    }
    pAhciPort->fATAPI = (enmType == PDMMEDIATYPE_CDROM || enmType == PDMMEDIATYPE_DVD);
    pAhciPort->fATAPIPassthrough = pAhciPort->fATAPI ? (pAhciPort->pDrvMedia->pfnSendCmd != NULL) : false;

    rc = RTCritSectInit(&pAhciPort->CritSectReqsFree);
    if (RT_FAILURE(rc))
        return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                   N_("AHCI initialisation error: Failed to create critical section for free request list"));

    pAhciPort->pListReqsFree = (PRTLISTANCHOR)PDMDevHlpMMHeapAllocZ(pDevIns, sizeof(RTLISTANCHOR));
    if (!pAhciPort->pListReqsFree)
        return PDMDevHlpVMSetError(pDevIns, VERR_NO_MEMORY, RT_SRC_POS,
                                   N_("AHCI initialisation error: Failed to allocate memory for free request list"));

    RTListInit(pAhciPort->pListReqsFree);

    if (pAhciPort->fATAPI)
    {
        pAhciPort->cTotalSectors = pAhciPort->pDrvMedia->pfnGetSize(pAhciPort->pDrvMedia) / 2048;
        pAhciPort->PCHSGeometry.cCylinders = 0;
        pAhciPort->PCHSGeometry.cHeads     = 0;
        pAhciPort->PCHSGeometry.cSectors   = 0;
        LogRel(("AHCI: LUN#%d: CD/DVD, total number of sectors %Ld, passthrough %s\n", pAhciPort->iLUN,
                pAhciPort->cTotalSectors, (pAhciPort->fATAPIPassthrough ? "enabled" : "disabled")));
    }
    else
    {
        pAhciPort->cbSector = pAhciPort->pDrvMedia->pfnGetSectorSize(pAhciPort->pDrvMedia);
        pAhciPort->cTotalSectors = pAhciPort->pDrvMedia->pfnGetSize(pAhciPort->pDrvMedia) / pAhciPort->cbSector;
        rc = pAhciPort->pDrvMedia->pfnBiosGetPCHSGeometry(pAhciPort->pDrvMedia, &pAhciPort->PCHSGeometry);
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
            pAhciPort->pDrvMedia->pfnBiosSetPCHSGeometry(pAhciPort->pDrvMedia, &pAhciPort->PCHSGeometry);
            rc = VINF_SUCCESS;
        }
        LogRel(("AHCI: LUN#%d: disk, PCHS=%u/%u/%u, total number of sectors %Ld\n",
                 pAhciPort->iLUN, pAhciPort->PCHSGeometry.cCylinders,
                 pAhciPort->PCHSGeometry.cHeads, pAhciPort->PCHSGeometry.cSectors,
                 pAhciPort->cTotalSectors));
        if (pAhciPort->pDrvMedia->pfnDiscard)
            LogRel(("AHCI: LUN#%d: Enabled TRIM support\n", pAhciPort->iLUN));
    }
    return rc;
}

/**
 * Callback employed by ahciR3Suspend and ahciR3PowerOff..
 *
 * @returns true if we've quiesced, false if we're still working.
 * @param   pDevIns     The device instance.
 */
static DECLCALLBACK(bool) ahciR3IsAsyncSuspendOrPowerOffDone(PPDMDEVINS pDevIns)
{
    if (!ahciR3AllAsyncIOIsFinished(pDevIns))
        return false;

    PAHCI pThis = PDMINS_2_DATA(pDevIns, PAHCI);
    ASMAtomicWriteBool(&pThis->fSignalIdle, false);
    /*
     * Free all cached tasks here, not possible on destruct because the driver
     * is destroyed before us.
     */
    for (unsigned iPort = 0; iPort < pThis->cPortsImpl; iPort++)
        ahciR3PortCachedReqsFree(&pThis->ahciPort[iPort]);
    return true;
}

/**
 * Common worker for ahciR3Suspend and ahciR3PowerOff.
 */
static void ahciR3SuspendOrPowerOff(PPDMDEVINS pDevIns)
{
    PAHCI pThis = PDMINS_2_DATA(pDevIns, PAHCI);

    ASMAtomicWriteBool(&pThis->fSignalIdle, true);
    if (!ahciR3AllAsyncIOIsFinished(pDevIns))
        PDMDevHlpSetAsyncNotification(pDevIns, ahciR3IsAsyncSuspendOrPowerOffDone);
    else
    {
        /*
         * Free all cached tasks here, not possible on destruct because the driver
         * is destroyed before us.
         */
        for (unsigned iPort = 0; iPort < pThis->cPortsImpl; iPort++)
            ahciR3PortCachedReqsFree(&pThis->ahciPort[iPort]);

        ASMAtomicWriteBool(&pThis->fSignalIdle, false);
    }
}

/**
 * Suspend notification.
 *
 * @param   pDevIns     The device instance data.
 */
static DECLCALLBACK(void) ahciR3Suspend(PPDMDEVINS pDevIns)
{
    Log(("ahciR3Suspend\n"));
    ahciR3SuspendOrPowerOff(pDevIns);
}

/**
 * Resume notification.
 *
 * @param   pDevIns     The device instance data.
 */
static DECLCALLBACK(void) ahciR3Resume(PPDMDEVINS pDevIns)
{
    PAHCI    pAhci = PDMINS_2_DATA(pDevIns, PAHCI);

    /*
     * Check if one of the ports has pending tasks.
     * Queue a notification item again in this case.
     */
    for (unsigned i = 0; i < RT_ELEMENTS(pAhci->ahciPort); i++)
    {
        PAHCIPort pAhciPort = &pAhci->ahciPort[i];

        if (pAhciPort->u32TasksRedo)
        {
            PDEVPORTNOTIFIERQUEUEITEM pItem = (PDEVPORTNOTIFIERQUEUEITEM)PDMQueueAlloc(pAhci->CTX_SUFF(pNotifierQueue));
            AssertMsg(pItem, ("Allocating item for queue failed\n"));

            pAhciPort->u32TasksNew |= pAhciPort->u32TasksRedo;
            pAhciPort->u32TasksRedo = 0;

            Assert(pAhciPort->fRedo);
            pAhciPort->fRedo = false;

            pItem->iPort = pAhci->ahciPort[i].iLUN;
            PDMQueueInsert(pAhci->CTX_SUFF(pNotifierQueue), (PPDMQUEUEITEMCORE)pItem);
        }
    }

    Log(("%s:\n", __FUNCTION__));
}

/**
 * Initializes the VPD data of a attached device.
 *
 * @returns VBox status code.
 * @param   pDevIns      The device instance.
 * @param   pAhciPort    The attached device.
 * @param   szName       Name of the port to get the CFGM node.
 */
static int ahciR3VpdInit(PPDMDEVINS pDevIns, PAHCIPort pAhciPort, const char *pszName)
{
    int rc = VINF_SUCCESS;
    PAHCI pAhci = PDMINS_2_DATA(pDevIns, PAHCI);

    /* Generate a default serial number. */
    char szSerial[AHCI_SERIAL_NUMBER_LENGTH+1];
    RTUUID Uuid;

    if (pAhciPort->pDrvMedia)
        rc = pAhciPort->pDrvMedia->pfnGetUuid(pAhciPort->pDrvMedia, &Uuid);
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
    PCFGMNODE pCfgNode = CFGMR3GetChild(pDevIns->pCfg, pszName);
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

    rc = CFGMR3QueryBoolDef(pCfgNode, "NonRotationalMedium", &pAhciPort->fNonRotational, false);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                    N_("AHCI configuration error: failed to read \"NonRotationalMedium\" as boolean"));

    rc = CFGMR3QueryU8Def(pCfgNode, "LogicalSectorsPerPhysical", &pAhciPort->cLogSectorsPerPhysicalExp, 0);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                    N_("AHCI configuration error: failed to read \"LogicalSectorsPerPhysical\" as integer"));
    if (pAhciPort->cLogSectorsPerPhysicalExp >= 16)
        return PDMDEV_SET_ERROR(pDevIns, rc,
                    N_("AHCI configuration error: \"LogicalSectorsPerPhysical\" must be between 0 and 15"));

    /* There are three other identification strings for CD drives used for INQUIRY */
    if (pAhciPort->fATAPI)
    {
        rc = CFGMR3QueryStringDef(pCfgNode, "ATAPIVendorId", pAhciPort->szInquiryVendorId, sizeof(pAhciPort->szInquiryVendorId),
                                  "VBOX");
        if (RT_FAILURE(rc))
        {
            if (rc == VERR_CFGM_NOT_ENOUGH_SPACE)
                return PDMDEV_SET_ERROR(pDevIns, VERR_INVALID_PARAMETER,
                                N_("AHCI configuration error: \"ATAPIVendorId\" is longer than 16 bytes"));
            return PDMDEV_SET_ERROR(pDevIns, rc,
                    N_("AHCI configuration error: failed to read \"ATAPIVendorId\" as string"));
        }

        rc = CFGMR3QueryStringDef(pCfgNode, "ATAPIProductId", pAhciPort->szInquiryProductId, sizeof(pAhciPort->szInquiryProductId),
                                  "CD-ROM");
        if (RT_FAILURE(rc))
        {
            if (rc == VERR_CFGM_NOT_ENOUGH_SPACE)
                return PDMDEV_SET_ERROR(pDevIns, VERR_INVALID_PARAMETER,
                                N_("AHCI configuration error: \"ATAPIProductId\" is longer than 16 bytes"));
            return PDMDEV_SET_ERROR(pDevIns, rc,
                    N_("AHCI configuration error: failed to read \"ATAPIProductId\" as string"));
        }

        rc = CFGMR3QueryStringDef(pCfgNode, "ATAPIRevision", pAhciPort->szInquiryRevision, sizeof(pAhciPort->szInquiryRevision),
                                  "1.0");
        if (RT_FAILURE(rc))
        {
            if (rc == VERR_CFGM_NOT_ENOUGH_SPACE)
                return PDMDEV_SET_ERROR(pDevIns, VERR_INVALID_PARAMETER,
                                N_("AHCI configuration error: \"ATAPIRevision\" is longer than 4 bytes"));
            return PDMDEV_SET_ERROR(pDevIns, rc,
                    N_("AHCI configuration error: failed to read \"ATAPIRevision\" as string"));
        }
    }

    return rc;
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
static DECLCALLBACK(void) ahciR3Detach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    PAHCI           pAhci = PDMINS_2_DATA(pDevIns, PAHCI);
    PAHCIPort       pAhciPort = &pAhci->ahciPort[iLUN];
    int             rc = VINF_SUCCESS;

    Log(("%s:\n", __FUNCTION__));

    AssertMsg(iLUN < pAhci->cPortsImpl, ("iLUN=%u", iLUN));
    AssertMsgReturnVoid(   pAhciPort->fHotpluggable
                        || (fFlags & PDM_TACH_FLAGS_NOT_HOT_PLUG),
                        ("AHCI: Port %d is not marked hotpluggable\n", pAhciPort->iLUN));


    if (pAhciPort->pAsyncIOThread)
    {
        int rcThread;
        /* Destroy the thread. */
        rc = PDMR3ThreadDestroy(pAhciPort->pAsyncIOThread, &rcThread);
        if (RT_FAILURE(rc) || RT_FAILURE(rcThread))
            AssertMsgFailed(("%s Failed to destroy async IO thread rc=%Rrc rcThread=%Rrc\n", __FUNCTION__, rc, rcThread));

        pAhciPort->pAsyncIOThread = NULL;
        pAhciPort->fWrkThreadSleeping = true;
    }

    if (pAhciPort->fATAPI)
        ahciMediumRemoved(pAhciPort);

    /* Free all cached I/O tasks. */
    ahciR3PortCachedReqsFree(pAhciPort);

    if (RTCritSectIsInitialized(&pAhciPort->CritSectReqsFree))
        RTCritSectDelete(&pAhciPort->CritSectReqsFree);

    if (pAhciPort->pListReqsFree)
        MMR3HeapFree(pAhciPort->pListReqsFree);

    pAhciPort->pListReqsFree = NULL;

    if (!(fFlags & PDM_TACH_FLAGS_NOT_HOT_PLUG))
    {
        /*
         * Inform the guest about the removed device.
         */
        pAhciPort->regSSTS = 0;
        pAhciPort->regSIG = 0;
        /*
         * Clear CR bit too to prevent submission of new commands when CI is written
         * (AHCI Spec 1.2: 7.4 Interaction of the Command List and Port Change Status).
         */
        ASMAtomicAndU32(&pAhciPort->regCMD, ~(AHCI_PORT_CMD_CPS | AHCI_PORT_CMD_CR));
        ASMAtomicOrU32(&pAhciPort->regIS, AHCI_PORT_IS_CPDS | AHCI_PORT_IS_PRCS | AHCI_PORT_IS_PCS);
        ASMAtomicOrU32(&pAhciPort->regSERR, AHCI_PORT_SERR_X | AHCI_PORT_SERR_N);
        if (   (pAhciPort->regIE & AHCI_PORT_IE_CPDE)
            || (pAhciPort->regIE & AHCI_PORT_IE_PCE)
            || (pAhciPort->regIE & AHCI_PORT_IE_PRCE))
            ahciHbaSetInterrupt(pAhciPort->CTX_SUFF(pAhci), pAhciPort->iLUN, VERR_IGNORED);
    }

    /*
     * Zero some important members.
     */
    pAhciPort->pDrvBase = NULL;
    pAhciPort->pDrvMedia = NULL;
    pAhciPort->pDrvMediaAsync = NULL;
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
static DECLCALLBACK(int)  ahciR3Attach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    PAHCI       pThis = PDMINS_2_DATA(pDevIns, PAHCI);
    PAHCIPort   pAhciPort = &pThis->ahciPort[iLUN];
    int         rc;

    Log(("%s:\n", __FUNCTION__));

    /* the usual paranoia */
    AssertMsg(iLUN < pThis->cPortsImpl, ("iLUN=%u", iLUN));
    AssertRelease(!pAhciPort->pDrvBase);
    AssertRelease(!pAhciPort->pDrvMedia);
    AssertRelease(!pAhciPort->pDrvMediaAsync);
    Assert(pAhciPort->iLUN == iLUN);

    AssertMsgReturn(   pAhciPort->fHotpluggable
                    || (fFlags & PDM_TACH_FLAGS_NOT_HOT_PLUG),
                    ("AHCI: Port %d is not marked hotpluggable\n", pAhciPort->iLUN),
                    VERR_INVALID_PARAMETER);

    /*
     * Try attach the block device and get the interfaces,
     * required as well as optional.
     */
    rc = PDMDevHlpDriverAttach(pDevIns, pAhciPort->iLUN, &pAhciPort->IBase, &pAhciPort->pDrvBase, NULL);
    if (RT_SUCCESS(rc))
    {
        rc = ahciR3ConfigureLUN(pDevIns, pAhciPort);

        /*
         * In case there is a medium inserted.
         */
        ahciMediumInserted(pAhciPort);
        ahciMediumTypeSet(pAhciPort, ATA_MEDIA_TYPE_UNKNOWN);
    }
    else
        AssertMsgFailed(("Failed to attach LUN#%d. rc=%Rrc\n", pAhciPort->iLUN, rc));

    if (RT_FAILURE(rc))
    {
        pAhciPort->pDrvBase = NULL;
        pAhciPort->pDrvMedia = NULL;
    }
    else
    {
        char szName[24];
        RTStrPrintf(szName, sizeof(szName), "Port%d", iLUN);

        if (   pAhciPort->pDrvMediaAsync
            && !pAhciPort->fATAPI)
            pAhciPort->fAsyncInterface = true;
        else
            pAhciPort->fAsyncInterface = false;

        rc = SUPSemEventCreate(pThis->pSupDrvSession, &pAhciPort->hEvtProcess);
        if (RT_FAILURE(rc))
            return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                       N_("AHCI: Failed to create SUP event semaphore"));

        /* Create the async IO thread. */
        rc = PDMDevHlpThreadCreate(pDevIns, &pAhciPort->pAsyncIOThread, pAhciPort, ahciAsyncIOLoop, ahciAsyncIOLoopWakeUp, 0,
                                   RTTHREADTYPE_IO, szName);
        if (RT_FAILURE(rc))
            return rc;

        /*
         * Init vendor product data.
         */
        if (RT_SUCCESS(rc))
            rc = ahciR3VpdInit(pDevIns, pAhciPort, szName);

        /* Inform the guest about the added device in case of hotplugging. */
        if (   RT_SUCCESS(rc)
            && !(fFlags & PDM_TACH_FLAGS_NOT_HOT_PLUG))
        {
            AssertMsgReturn(pAhciPort->fHotpluggable,
                            ("AHCI: Port %d is not marked hotpluggable\n", pAhciPort->iLUN),
                            VERR_NOT_SUPPORTED);

            /*
             * Initialize registers
             */
            ASMAtomicOrU32(&pAhciPort->regCMD, AHCI_PORT_CMD_CPS);
            ASMAtomicOrU32(&pAhciPort->regIS, AHCI_PORT_IS_CPDS | AHCI_PORT_IS_PRCS | AHCI_PORT_IS_PCS);
            ASMAtomicOrU32(&pAhciPort->regSERR, AHCI_PORT_SERR_X | AHCI_PORT_SERR_N);

            if (pAhciPort->fATAPI)
                pAhciPort->regSIG = AHCI_PORT_SIG_ATAPI;
            else
                pAhciPort->regSIG = AHCI_PORT_SIG_DISK;
            pAhciPort->regSSTS = (0x01 << 8) | /* Interface is active. */
                                 (0x02 << 4) | /* Generation 2 (3.0GBps) speed. */
                                 (0x03 << 0);  /* Device detected and communication established. */

            if (   (pAhciPort->regIE & AHCI_PORT_IE_CPDE)
                || (pAhciPort->regIE & AHCI_PORT_IE_PCE)
                || (pAhciPort->regIE & AHCI_PORT_IE_PRCE))
                ahciHbaSetInterrupt(pAhciPort->CTX_SUFF(pAhci), pAhciPort->iLUN, VERR_IGNORED);
        }

    }

    return rc;
}

/**
 * Common reset worker.
 *
 * @param   pDevIns     The device instance data.
 */
static int ahciR3ResetCommon(PPDMDEVINS pDevIns, bool fConstructor)
{
    PAHCI pAhci = PDMINS_2_DATA(pDevIns, PAHCI);

    ahciHBAReset(pAhci);

    /* Hardware reset for the ports. */
    for (uint32_t i = 0; i < RT_ELEMENTS(pAhci->ahciPort); i++)
        ahciPortHwReset(&pAhci->ahciPort[i]);
    return VINF_SUCCESS;
}

/**
 * Callback employed by ahciR3Reset.
 *
 * @returns true if we've quiesced, false if we're still working.
 * @param   pDevIns     The device instance.
 */
static DECLCALLBACK(bool) ahciR3IsAsyncResetDone(PPDMDEVINS pDevIns)
{
    PAHCI pThis = PDMINS_2_DATA(pDevIns, PAHCI);

    if (!ahciR3AllAsyncIOIsFinished(pDevIns))
        return false;
    ASMAtomicWriteBool(&pThis->fSignalIdle, false);

    ahciR3ResetCommon(pDevIns, false /*fConstructor*/);
    return true;
}

/**
 * Reset notification.
 *
 * @param   pDevIns     The device instance data.
 */
static DECLCALLBACK(void) ahciR3Reset(PPDMDEVINS pDevIns)
{
    PAHCI pThis = PDMINS_2_DATA(pDevIns, PAHCI);

    ASMAtomicWriteBool(&pThis->fSignalIdle, true);
    if (!ahciR3AllAsyncIOIsFinished(pDevIns))
        PDMDevHlpSetAsyncNotification(pDevIns, ahciR3IsAsyncResetDone);
    else
    {
        ASMAtomicWriteBool(&pThis->fSignalIdle, false);
        ahciR3ResetCommon(pDevIns, false /*fConstructor*/);
    }
}

/**
 * Poweroff notification.
 *
 * @param   pDevIns Pointer to the device instance
 */
static DECLCALLBACK(void) ahciR3PowerOff(PPDMDEVINS pDevIns)
{
    Log(("achiR3PowerOff\n"));
    ahciR3SuspendOrPowerOff(pDevIns);
}

/**
 * Destroy a driver instance.
 *
 * Most VM resources are freed by the VM. This callback is provided so that any non-VM
 * resources can be freed correctly.
 *
 * @param   pDevIns     The device instance data.
 */
static DECLCALLBACK(int) ahciR3Destruct(PPDMDEVINS pDevIns)
{
    PAHCI       pThis = PDMINS_2_DATA(pDevIns, PAHCI);
    int         rc    = VINF_SUCCESS;
    PDMDEV_CHECK_VERSIONS_RETURN_QUIET(pDevIns);

    /*
     * At this point the async I/O thread is suspended and will not enter
     * this module again. So, no coordination is needed here and PDM
     * will take care of terminating and cleaning up the thread.
     */
    if (PDMCritSectIsInitialized(&pThis->lock))
    {
        TMR3TimerDestroy(pThis->CTX_SUFF(pHbaCccTimer));
        pThis->CTX_SUFF(pHbaCccTimer) = NULL;

        Log(("%s: Destruct every port\n", __FUNCTION__));
        for (unsigned iActPort = 0; iActPort < pThis->cPortsImpl; iActPort++)
        {
            PAHCIPort pAhciPort = &pThis->ahciPort[iActPort];

            if (pAhciPort->hEvtProcess != NIL_SUPSEMEVENT)
            {
                SUPSemEventClose(pThis->pSupDrvSession, pAhciPort->hEvtProcess);
                pAhciPort->hEvtProcess = NIL_SUPSEMEVENT;
            }

            if (RTCritSectIsInitialized(&pAhciPort->CritSectReqsFree))
                RTCritSectDelete(&pAhciPort->CritSectReqsFree);

#ifdef VBOX_STRICT
            for (uint32_t i = 0; i < AHCI_NR_COMMAND_SLOTS; i++)
                Assert(!pAhciPort->aActiveTasks[i]);
#endif
        }

        PDMR3CritSectDelete(&pThis->lock);
    }

    return rc;
}

/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int) ahciR3Construct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    PAHCI      pThis = PDMINS_2_DATA(pDevIns, PAHCI);
    PPDMIBASE  pBase;
    int        rc = VINF_SUCCESS;
    unsigned   i = 0;
    bool       fGCEnabled = false;
    bool       fR0Enabled = false;
    uint32_t   cbTotalBufferSize = 0;
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);

    LogFlowFunc(("pThis=%#p\n", pThis));

    /*
     * Validate and read configuration.
     */
    if (!CFGMR3AreValuesValid(pCfg, "GCEnabled\0"
                                    "R0Enabled\0"
                                    "PrimaryMaster\0"
                                    "PrimarySlave\0"
                                    "SecondaryMaster\0"
                                    "SecondarySlave\0"
                                    "PortCount\0"
                                    "UseAsyncInterfaceIfAvailable\0"
                                    "Bootable\0"
                                    "CmdSlotsAvail\0"))
        return PDMDEV_SET_ERROR(pDevIns, VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES,
                                N_("AHCI configuration error: unknown option specified"));

    rc = CFGMR3QueryBoolDef(pCfg, "GCEnabled", &fGCEnabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("AHCI configuration error: failed to read GCEnabled as boolean"));
    Log(("%s: fGCEnabled=%d\n", __FUNCTION__, fGCEnabled));

    rc = CFGMR3QueryBoolDef(pCfg, "R0Enabled", &fR0Enabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("AHCI configuration error: failed to read R0Enabled as boolean"));
    Log(("%s: fR0Enabled=%d\n", __FUNCTION__, fR0Enabled));

    rc = CFGMR3QueryU32Def(pCfg, "PortCount", &pThis->cPortsImpl, AHCI_MAX_NR_PORTS_IMPL);
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

    rc = CFGMR3QueryBoolDef(pCfg, "UseAsyncInterfaceIfAvailable", &pThis->fUseAsyncInterfaceIfAvailable, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("AHCI configuration error: failed to read UseAsyncInterfaceIfAvailable as boolean"));

    rc = CFGMR3QueryBoolDef(pCfg, "Bootable", &pThis->fBootable, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("AHCI configuration error: failed to read Bootable as boolean"));

    rc = CFGMR3QueryU32Def(pCfg, "CmdSlotsAvail", &pThis->cCmdSlotsAvail, AHCI_NR_COMMAND_SLOTS);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("AHCI configuration error: failed to read CmdSlotsAvail as integer"));
    Log(("%s: cCmdSlotsAvail=%u\n", __FUNCTION__, pThis->cCmdSlotsAvail));
    if (pThis->cCmdSlotsAvail > AHCI_NR_COMMAND_SLOTS)
        return PDMDevHlpVMSetError(pDevIns, VERR_INVALID_PARAMETER, RT_SRC_POS,
                                   N_("AHCI configuration error: CmdSlotsAvail=%u should not exceed %u"),
                                   pThis->cPortsImpl, AHCI_NR_COMMAND_SLOTS);
    if (pThis->cCmdSlotsAvail < 1)
        return PDMDevHlpVMSetError(pDevIns, VERR_INVALID_PARAMETER, RT_SRC_POS,
                                   N_("AHCI configuration error: CmdSlotsAvail=%u should be at least 1"),
                                   pThis->cCmdSlotsAvail);

    /*
     * Initialize the instance data (everything touched by the destructor need
     * to be initialized here!).
     */
    pThis->fR0Enabled = fR0Enabled;
    pThis->fGCEnabled = fGCEnabled;
    pThis->pDevInsR3 = pDevIns;
    pThis->pDevInsR0 = PDMDEVINS_2_R0PTR(pDevIns);
    pThis->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);
    pThis->pSupDrvSession = PDMDevHlpGetSupDrvSession(pDevIns);

    PCIDevSetVendorId    (&pThis->dev, 0x8086); /* Intel */
    PCIDevSetDeviceId    (&pThis->dev, 0x2829); /* ICH-8M */
    PCIDevSetCommand     (&pThis->dev, 0x0000);
#ifdef VBOX_WITH_MSI_DEVICES
    PCIDevSetStatus      (&pThis->dev, VBOX_PCI_STATUS_CAP_LIST);
    PCIDevSetCapabilityList(&pThis->dev, 0x80);
#else
    PCIDevSetCapabilityList(&pThis->dev, 0x70);
#endif
    PCIDevSetRevisionId  (&pThis->dev, 0x02);
    PCIDevSetClassProg   (&pThis->dev, 0x01);
    PCIDevSetClassSub    (&pThis->dev, 0x06);
    PCIDevSetClassBase   (&pThis->dev, 0x01);
    PCIDevSetBaseAddress (&pThis->dev, 5, false, false, false, 0x00000000);

    PCIDevSetInterruptLine(&pThis->dev, 0x00);
    PCIDevSetInterruptPin (&pThis->dev, 0x01);

    pThis->dev.config[0x70] = VBOX_PCI_CAP_ID_PM; /* Capability ID: PCI Power Management Interface */
    pThis->dev.config[0x71] = 0xa8; /* next */
    pThis->dev.config[0x72] = 0x03; /* version ? */

    pThis->dev.config[0x90] = 0x40; /* AHCI mode. */
    pThis->dev.config[0x92] = 0x3f;
    pThis->dev.config[0x94] = 0x80;
    pThis->dev.config[0x95] = 0x01;
    pThis->dev.config[0x97] = 0x78;

    pThis->dev.config[0xa8] = 0x12;                /* SATACR capability */
    pThis->dev.config[0xa9] = 0x00;                /* next */
    PCIDevSetWord(&pThis->dev, 0xaa, 0x0010);      /* Revision */
    PCIDevSetDWord(&pThis->dev, 0xac, 0x00000028); /* SATA Capability Register 1 */

    pThis->cThreadsActive = 0;

    /* Initialize port members. */
    for (i = 0; i < AHCI_MAX_NR_PORTS_IMPL; i++)
    {
        PAHCIPort pAhciPort             = &pThis->ahciPort[i];
        pAhciPort->pDevInsR3            = pDevIns;
        pAhciPort->pDevInsR0            = PDMDEVINS_2_R0PTR(pDevIns);
        pAhciPort->pDevInsRC            = PDMDEVINS_2_RCPTR(pDevIns);
        pAhciPort->iLUN                 = i;
        pAhciPort->pAhciR3              = pThis;
        pAhciPort->pAhciR0              = PDMINS_2_DATA_R0PTR(pDevIns);
        pAhciPort->pAhciRC              = PDMINS_2_DATA_RCPTR(pDevIns);
        pAhciPort->Led.u32Magic         = PDMLED_MAGIC;
        pAhciPort->pDrvBase             = NULL;
        pAhciPort->pAsyncIOThread       = NULL;
        pAhciPort->hEvtProcess          = NIL_SUPSEMEVENT;
        pAhciPort->fHotpluggable        = true;
    }

    /*
     * Init locks, using explicit locking where necessary.
     */
    rc = PDMDevHlpSetDeviceCritSect(pDevIns, PDMDevHlpCritSectGetNop(pDevIns));
    if (RT_FAILURE(rc))
        return rc;

    rc = PDMDevHlpCritSectInit(pDevIns, &pThis->lock, RT_SRC_POS, "AHCI#%u", iInstance);
    if (RT_FAILURE(rc))
    {
        Log(("%s: Failed to create critical section.\n", __FUNCTION__));
        return rc;
    }

    /*
     * Register the PCI device, it's I/O regions.
     */
    rc = PDMDevHlpPCIRegister (pDevIns, &pThis->dev);
    if (RT_FAILURE(rc))
        return rc;

#ifdef VBOX_WITH_MSI_DEVICES
    PDMMSIREG MsiReg;
    RT_ZERO(MsiReg);
    MsiReg.cMsiVectors    = 1;
    MsiReg.iMsiCapOffset  = 0x80;
    MsiReg.iMsiNextOffset = 0x70;
    rc = PDMDevHlpPCIRegisterMsi(pDevIns, &MsiReg);
    if (RT_FAILURE(rc))
    {
        PCIDevSetCapabilityList(&pThis->dev, 0x70);
        /* That's OK, we can work without MSI */
    }
#endif

    /*
     * Solaris 10 U5 fails to map the AHCI register space when the sets (0..5) for the legacy
     * IDE registers are not available.
     * We set up "fake" entries in the PCI configuration register.
     * That means they are available but read and writes from/to them have no effect.
     * No guest should access them anyway because the controller is marked as AHCI in the Programming interface
     * and we don't have an option to change to IDE emulation (real hardware provides an option in the BIOS
     * to switch to it which also changes device Id and other things in the PCI configuration space).
     */
    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 0, 8, PCI_ADDRESS_SPACE_IO, ahciR3LegacyFakeIORangeMap);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("AHCI cannot register PCI I/O region"));

    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 1, 1, PCI_ADDRESS_SPACE_IO, ahciR3LegacyFakeIORangeMap);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("AHCI cannot register PCI I/O region"));

    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 2, 8, PCI_ADDRESS_SPACE_IO, ahciR3LegacyFakeIORangeMap);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("AHCI cannot register PCI I/O region"));

    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 3, 1, PCI_ADDRESS_SPACE_IO, ahciR3LegacyFakeIORangeMap);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("AHCI cannot register PCI I/O region"));

    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 4, 0x10, PCI_ADDRESS_SPACE_IO, ahciR3IdxDataIORangeMap);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("AHCI cannot register PCI I/O region for BMDMA"));

    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 5, 4352, PCI_ADDRESS_SPACE_MEM, ahciR3MMIOMap);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("AHCI cannot register PCI memory region for registers"));

    /* Create the timer for command completion coalescing feature. */
    rc = PDMDevHlpTMTimerCreate(pDevIns, TMCLOCK_VIRTUAL, ahciCccTimer, pThis,
                                TMTIMER_FLAGS_NO_CRIT_SECT, "AHCI CCC Timer", &pThis->pHbaCccTimerR3);
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("pfnTMTimerCreate -> %Rrc\n", rc));
        return rc;
    }
    pThis->pHbaCccTimerR0 = TMTimerR0Ptr(pThis->pHbaCccTimerR3);
    pThis->pHbaCccTimerRC = TMTimerRCPtr(pThis->pHbaCccTimerR3);

    /* Status LUN. */
    pThis->IBase.pfnQueryInterface = ahciR3Status_QueryInterface;
    pThis->ILeds.pfnQueryStatusLed = ahciR3Status_QueryStatusLed;

    /*
     * Create the notification queue.
     *
     * We need 2 items for every port because of SMP races.
     */
    rc = PDMDevHlpQueueCreate(pDevIns, sizeof(DEVPORTNOTIFIERQUEUEITEM), AHCI_MAX_NR_PORTS_IMPL * 2, 0,
                              ahciNotifyQueueConsumer, true, "AHCI-Xmit", &pThis->pNotifierQueueR3);
    if (RT_FAILURE(rc))
        return rc;
    pThis->pNotifierQueueR0 = PDMQueueR0Ptr(pThis->pNotifierQueueR3);
    pThis->pNotifierQueueRC = PDMQueueRCPtr(pThis->pNotifierQueueR3);

    /* Initialize static members on every port. */
    for (i = 0; i < AHCI_MAX_NR_PORTS_IMPL; i++)
    {
        PAHCIPort pAhciPort = &pThis->ahciPort[i];

        PDMDevHlpSTAMRegisterF(pDevIns, &pAhciPort->StatDMA, STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_OCCURENCES,
                               "Number of DMA transfers.", "/Devices/SATA%d/Port%d/DMA", iInstance, i);
        PDMDevHlpSTAMRegisterF(pDevIns, &pAhciPort->StatBytesRead, STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_BYTES,
                               "Amount of data read.", "/Devices/SATA%d/Port%d/ReadBytes", iInstance, i);
        PDMDevHlpSTAMRegisterF(pDevIns, &pAhciPort->StatBytesWritten, STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_BYTES,
                               "Amount of data written.", "/Devices/SATA%d/Port%d/WrittenBytes", iInstance, i);
        PDMDevHlpSTAMRegisterF(pDevIns, &pAhciPort->StatIORequestsPerSecond, STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_OCCURENCES,
                               "Number of processed I/O requests per second.", "/Devices/SATA%d/Port%d/IORequestsPerSecond", iInstance, i);
#ifdef VBOX_WITH_STATISTICS
        PDMDevHlpSTAMRegisterF(pDevIns, &pAhciPort->StatProfileProcessTime, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_NS_PER_CALL,
                               "Amount of time to process one request.", "/Devices/SATA%d/Port%d/ProfileProcessTime", iInstance, i);
        PDMDevHlpSTAMRegisterF(pDevIns, &pAhciPort->StatProfileReadWrite, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_NS_PER_CALL,
                               "Amount of time for the read/write operation to complete.", "/Devices/SATA%d/Port%d/ProfileReadWrite", iInstance, i);
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
        pAhciPort->IBase.pfnQueryInterface              = ahciR3PortQueryInterface;
        pAhciPort->IPortAsync.pfnTransferCompleteNotify = ahciR3TransferCompleteNotify;
        pAhciPort->IPort.pfnQueryDeviceLocation         = ahciR3PortQueryDeviceLocation;
        pAhciPort->IMountNotify.pfnMountNotify          = ahciR3MountNotify;
        pAhciPort->IMountNotify.pfnUnmountNotify        = ahciR3UnmountNotify;
        pAhciPort->fWrkThreadSleeping                   = true;

        /* Query per port configuration options if available. */
        PCFGMNODE pCfgPort = CFGMR3GetChild(pDevIns->pCfg, szName);
        if (pCfgPort)
        {
            rc = CFGMR3QueryBoolDef(pCfgPort, "Hotpluggable", &pAhciPort->fHotpluggable, true);
            if (RT_FAILURE(rc))
                return PDMDEV_SET_ERROR(pDevIns, rc,
                                        N_("AHCI configuration error: failed to read Hotpluggable as boolean"));
        }

        /*
         * Attach the block driver
         */
        rc = PDMDevHlpDriverAttach(pDevIns, pAhciPort->iLUN, &pAhciPort->IBase, &pAhciPort->pDrvBase, szName);
        if (RT_SUCCESS(rc))
        {
            rc = ahciR3ConfigureLUN(pDevIns, pAhciPort);
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
            rc = ahciR3VpdInit(pDevIns, pAhciPort, szName);
            if (RT_FAILURE(rc))
                return rc;

            /*
             * If the new async interface is available we use a PDMQueue to transmit
             * the requests into R3.
             * Otherwise we use a event semaphore and a async I/O thread which processes them.
             */
            if (pAhciPort->pDrvMediaAsync && pThis->fUseAsyncInterfaceIfAvailable)
            {
                LogRel(("AHCI: LUN#%d: using async I/O\n", pAhciPort->iLUN));
                pAhciPort->fAsyncInterface = true;
            }
            else
            {
                LogRel(("AHCI: LUN#%d: using normal I/O\n", pAhciPort->iLUN));
                pAhciPort->fAsyncInterface = false;
            }

            rc = SUPSemEventCreate(pThis->pSupDrvSession, &pAhciPort->hEvtProcess);
            if (RT_FAILURE(rc))
                return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                           N_("AHCI: Failed to create SUP event semaphore"));

            rc = PDMDevHlpThreadCreate(pDevIns, &pAhciPort->pAsyncIOThread, pAhciPort, ahciAsyncIOLoop,
                                       ahciAsyncIOLoopWakeUp, 0, RTTHREADTYPE_IO, szName);
            if (RT_FAILURE(rc))
                return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                           N_("AHCI: Failed to create worker thread %s"), szName);
        }
        else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
        {
            pAhciPort->pDrvBase = NULL;
            rc = VINF_SUCCESS;
            LogRel(("AHCI: %s: No driver attached\n", szName));
        }
        else
            return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                       N_("AHCI: Failed to attach drive to %s"), szName);
    }

    /*
     * Attach status driver (optional).
     */
    rc = PDMDevHlpDriverAttach(pDevIns, PDM_STATUS_LUN, &pThis->IBase, &pBase, "Status Port");
    if (RT_SUCCESS(rc))
    {
        pThis->pLedsConnector = PDMIBASE_QUERY_INTERFACE(pBase, PDMILEDCONNECTORS);
        pThis->pMediaNotify = PDMIBASE_QUERY_INTERFACE(pBase, PDMIMEDIANOTIFY);
    }
    else if (rc != VERR_PDM_NO_ATTACHED_DRIVER)
    {
        AssertMsgFailed(("Failed to attach to status driver. rc=%Rrc\n", rc));
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("AHCI cannot attach to status driver"));
    }
    rc = PDMDevHlpSSMRegisterEx(pDevIns, AHCI_SAVED_STATE_VERSION, sizeof(*pThis) + cbTotalBufferSize, NULL,
                                NULL,           ahciR3LiveExec, NULL,
                                ahciR3SavePrep, ahciR3SaveExec, NULL,
                                ahciR3LoadPrep, ahciR3LoadExec, NULL);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Register the info item.
     */
    char szTmp[128];
    RTStrPrintf(szTmp, sizeof(szTmp), "%s%d", pDevIns->pReg->szName, pDevIns->iInstance);
    PDMDevHlpDBGFInfoRegister(pDevIns, szTmp, "AHCI info", ahciR3Info);

    return ahciR3ResetCommon(pDevIns, true /*fConstructor*/);
}

/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceAHCI =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szName */
    "ahci",
    /* szRCMod */
    "VBoxDDRC.rc",
    /* szR0Mod */
    "VBoxDDR0.r0",
    /* pszDescription */
    "Intel AHCI controller.\n",
    /* fFlags */
    PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_RC | PDM_DEVREG_FLAGS_R0 |
    PDM_DEVREG_FLAGS_FIRST_SUSPEND_NOTIFICATION | PDM_DEVREG_FLAGS_FIRST_POWEROFF_NOTIFICATION |
    PDM_DEVREG_FLAGS_FIRST_RESET_NOTIFICATION,
    /* fClass */
    PDM_DEVREG_CLASS_STORAGE,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(AHCI),
    /* pfnConstruct */
    ahciR3Construct,
    /* pfnDestruct */
    ahciR3Destruct,
    /* pfnRelocate */
    ahciR3Relocate,
    /* pfnMemSetup */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    ahciR3Reset,
    /* pfnSuspend */
    ahciR3Suspend,
    /* pfnResume */
    ahciR3Resume,
    /* pfnAttach */
    ahciR3Attach,
    /* pfnDetach */
    ahciR3Detach,
    /* pfnQueryInterface. */
    NULL,
    /* pfnInitComplete */
    NULL,
    /* pfnPowerOff */
    ahciR3PowerOff,
    /* pfnSoftReset */
    NULL,
    /* u32VersionEnd */
    PDM_DEVREG_VERSION
};

#endif /* IN_RING3 */
#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */
