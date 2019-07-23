/* $Id$ $Revision$ $Date$ $Author$ */
/** @file
 * VBox storage devices - Virtio SCSI Driver
 *
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DRV_SCSI

#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pdmstorageifs.h>
#include <VBox/vmm/pdmcritsect.h>
#include <VBox/version.h>
#include <iprt/errcore.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include "../build/VBoxDD.h"
#include <VBox/scsi.h>
#ifdef IN_RING3
# include <iprt/alloc.h>
# include <iprt/memcache.h>
# include <iprt/param.h>
# include <iprt/uuid.h>
#endif
#include "../VirtIO/Virtio_1_0.h"

#include "VBoxSCSI.h"
#include "VBoxDD.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
#define VIRTIOSCSI_MAX_TARGETS  1

/**
 * Device Instance Data.
 */
typedef struct VIRTIOSCSITARGET
{


    /** Pointer to the owning virtioScsi device instance. - R3 pointer */
    R3PTRTYPE(struct VIRTIOSCSI *)  pVirtioScsiR3;
    /** Pointer to the owning virtioScsi device instance. - R0 pointer */
    R3PTRTYPE(struct VIRTIOSCSI *)  pVirtioScsiR0;
    /** Pointer to the owning virtioScsi device instance. - RC pointer */
    R3PTRTYPE(struct VIRTIOSCSI *)  pVirtioScsiRC;

    /** Pointer to attached driver's base interface. */
    R3PTRTYPE(PPDMIBASE)            pUpstreamDrvBase;

    /** LUN of the device. */
    RTUINT                          iLUN;

    /** Our base interface. */
    PDMIBASE                        IBase;

    /** Flag whether device is present. */
    bool                            fPresent;

    /** Media port interface. */
    PDMIMEDIAPORT                   IMediaPort;

    /** Extended media port interface. */
    PDMIMEDIAEXPORT                 IMediaExPort;

    /** Number of outstanding tasks on the port. */
    volatile uint32_t               cOutstandingRequests;

    /** LUN Description */
    char *                          pszLunName;

    /** Pointer to the attached driver's media interface. */
    R3PTRTYPE(PPDMIMEDIA)           pDrvMedia;

     /** Pointer to the attached driver's extended media interface. */
    R3PTRTYPE(PPDMIMEDIAEX)         pDrvMediaEx;

    R3PTRTYPE(PVQUEUE)              pCtlQueue; // ? TBD
    R3PTRTYPE(PVQUEUE)              pEvtQueue; // ? TBD
    R3PTRTYPE(PVQUEUE)              pReqQueue; // ? TBD

} VIRTIOSCSITARGET, *PVIRTIOSCSITARGET;


/**
 * Main VirtIO SCSI device state.
 *
 * @extends     PDMPCIDEV
 */
typedef struct VIRTIOSCSI
{

    PPDMDEVINSR3           pDevInsR3;              /**< Device instance - R3. */
    PPDMDEVINSR0           pDevInsR0;              /**< Device instance - R0. */
    PPDMDEVINSRC           pDevInsRC;              /**< Device instance - RC. */

    /* virtioState must be first member */
    VIRTIOSTATE                     virtioState;

    /* SCSI target instances data */
    VIRTIOSCSITARGET                aTargetInstances[VIRTIOSCSI_MAX_TARGETS];

    /** Base address of the I/O ports. */
    RTIOPORT                        IOPortBase;

    /** Base address of the memory mapping. */
    RTGCPHYS                        GCPhysMMIOBase;

    /** IMediaExPort: Media ejection notification */
    R3PTRTYPE(PPDMIMEDIANOTIFY)     pMediaNotify;

    /** Queue to send tasks to R3. - HC ptr */
    R3PTRTYPE(PPDMQUEUE)            pNotifierQueueR3;

    /** The support driver session handle. */
    R3R0PTRTYPE(PSUPDRVSESSION)     pSupDrvSession;

    /** Worker thread. */
    R3PTRTYPE(PPDMTHREAD)           pThreadWrk;

    /** The event semaphore the processing thread waits on. */
    SUPSEMEVENT                     hEvtProcess;

    /** Number of ports detected */
    uint64_t                        cTargets;

    /** True if PDMDevHlpAsyncNotificationCompleted should be called when port goes idle */
    bool volatile                   fSignalIdle;

} VIRTIOSCSI, *PVIRTIOSCSI;



//pk: Needed for virtioIO (e.g. to talk to devSCSI? TBD ??
/**
 * Task state for a CCB request.
 */
typedef struct VIRTIOSCSIREQ
{
    /** Device this task is assigned to. */
    PVIRTIOSCSITARGET              pTargetDevice;
    /** The command control block from the guest. */
//    CCBU                           CCBGuest;
    /** Guest physical address of th CCB. */
    RTGCPHYS                       GCPhysAddrCCB;
    /** Pointer to the R3 sense buffer. */
    uint8_t                        *pbSenseBuffer;
    /** Flag whether this is a request from the BIOS. */
    bool                           fBIOS;
    /** 24-bit request flag (default is 32-bit). */
    bool                           fIs24Bit;
    /** SCSI status code. */
    uint8_t                        u8ScsiSts;
} VIRTIOSCSIREQ;
typedef struct VIRTIOSCSIREQ *PVIRTIOSCSIREQ;


#define VIRTIOSCSI_SAVED_STATE_MINOR_VERSION        0x01

#define PCI_VENDOR_ID_VIRTIO                        0x1AF4
#define PCI_DEVICE_ID_VIRTIO_SCSI_HOST              0x1048
#define PCI_CLASS_BASE_MASS_STORAGE                 0x01
#define PCI_CLASS_SUB_SCSI_STORAGE_CONTROLLER       0x00
#define PCI_CLASS_PROG_UNSPECIFIED                  0x00
#define VIRTIO_SCSI_NAME_FMT                        "VIRTIOSCSI%d" /* "VSCSI" *might* be ambiguous with VBoxSCSI? */
#define VIRTIO_SCSI_PCI_CLASS                       0x01     /* Base class Mass Storage? */
#define VIRTIO_SCSI_N_QUEUES                        3        /* Control, Event, Request */
#define VIRTIO_SCSI_REGION_MEM_IO                   0
#define VIRTIO_SCSI_REGION_PORT_IO                  1
#define VIRTIO_SCSI_REGION_PCI_CAP                  2

/**
 * Definitions that follow are based on the VirtIO 1.0 specification.
 * Struct names are the same. The field names have been adapted to VirtualBox
 * data type + camel case annotation, with the original field name from the
 * VirtIO specification in the field's comment.
 */

/** @name VirtIO 1.0 SCSI Host feature bits
 * @{  */
#define VIRTIO_SCSI_F_INOUT                         0      /** Request is device readable AND writeable */
#define VIRTIO_SCSI_F_HOTPLUG                       1      /** Host SHOULD allow hotplugging SCSI LUNs & targets */
#define VIRTIO_SCSI_F_CHANGE                        2      /** Host reports LUN changes via VIRTIO_SCSI_T_PARAM_CHANGE event */
/** @} */

#define CDB_SIZE                                    1      /* logic tbd */
#define SENSE_SIZE                                  1      /* logic tbd */
#define PI_BYTES_OUT                                1      /* logic tbd */
#define PI_BYTES_IN                                 1      /* logic tbd */
#define DATA_OUT                                    1      /* logic tbd */
typedef struct virtio_scsi_req_cmd
{
    /* Device-readable part */
    uint8_t  uLUN[8];                                       /** lun               */
    uint64_t uId;                                           /** id                */
    uint8_t  uTaskAttr;                                     /** task_attr         */
    uint8_t  uPrio;                                         /** prio              */
    uint8_t  uCrn;                                          /** crn               */
    uint8_t  uCdb[CDB_SIZE];                                /** cdb               */

    /** Following three fields only present if VIRTIO_SCSI_F_T10_PI negotiated */

    uint32_t uPiBytesOut;                                   /** pi_bytesout       */
    uint32_t uPiBytesIn;                                    /** pi_bytesin        */
    uint8_t  uPiOut[PI_BYTES_OUT];                          /** pi_out[]          */

    uint8_t  uDataOut[DATA_OUT];                            /** dataout           */

    /* Device-writable part */
    uint32_t uSenseLen;                                     /** sense_len         */
    uint32_t uResidual;                                     /** residual          */
    uint16_t uStatusQualifier;                              /** status_qualifier  */
    uint8_t  uStatus;                                       /** status            */
    uint8_t  uResponse;                                     /** response          */
    uint8_t  uSense[SENSE_SIZE];                            /** sense             */

    /** Following two fields only present if VIRTIO_SCSI_F_T10_PI negotiated */
    uint8_t  uPiIn[PI_BYTES_IN];                            /** pi_in[]           */
    uint8_t  uDataIn[];                                     /** detain;           */
} VIRTIOSCSIREQCMD, *PVIRTIOSCSIREQCMD;

/** Command-specific response values */
#define VIRTIO_SCSI_S_OK                            0       /* control, command   */
#define VIRTIO_SCSI_S_OVERRUN                       1       /* control */
#define VIRTIO_SCSI_S_ABORTED                       2       /* control */
#define VIRTIO_SCSI_S_BAD_TARGET                    3       /* control, command */
#define VIRTIO_SCSI_S_RESET                         4       /* control */
#define VIRTIO_SCSI_S_BUSY                          5       /* control, command */
#define VIRTIO_SCSI_S_TRANSPORT_FAILURE             6       /* control, command */
#define VIRTIO_SCSI_S_TARGET_FAILURE                7       /* control, command */
#define VIRTIO_SCSI_S_NEXUS_FAILURE                 8       /* control, command */
#define VIRTIO_SCSI_S_FAILURE                       9       /* control, command */
#define VIRTIO_SCSI_S_INCORRECT_LUN                12       /* command */


/** task_attr */
#define VIRTIO_SCSI_S_SIMPLE                        0
#define VIRTIO_SCSI_S_ORDERED                       1
#define VIRTIO_SCSI_S_HEAD                          2
#define VIRTIO_SCSI_S_ACA                           3

#define VIRTIO_SCSI_T_TMF                           0
#define VIRTIO_SCSI_T_TMF_ABORT_TASK                0
#define VIRTIO_SCSI_T_TMF_ABORT_TASK_SET            1
#define VIRTIO_SCSI_T_TMF_CLEAR_ACA                 2
#define VIRTIO_SCSI_T_TMF_CLEAR_TASK_SET            3
#define VIRTIO_SCSI_T_TMF_I_T_NEXUS_RESET           4
#define VIRTIO_SCSI_T_TMF_LOGICAL_UNIT_RESET        5
#define VIRTIO_SCSI_T_TMF_QUERY_TASK                6
#define VIRTIO_SCSI_T_TMF_QUERY_TASK_SET            7

typedef struct virtio_scsi_ctrl_tmf
{
     // Device-readable part
    uint32_t uType;                                          /** type           */
    uint32_t uSubtype;                                       /** subtype        */
    uint8_t  uLUN[8];                                        /** lun            */
    uint64_t uId;                                            /** id             */
    // Device-writable part
    uint8_t  uResponse;                                      /** response       */
} VIRTIOSCSICTRLBUF, *PVIRTIOSCSICTRLBUF;

/* command-specific response values */

#define VIRTIO_SCSI_S_FUNCTION_COMPLETE            0
#define VIRTIO_SCSI_S_FUNCTION_SUCCEEDED           10
#define VIRTIO_SCSI_S_FUNCTION_REJECTED            11

#define VIRTIO_SCSI_T_AN_QUERY                     1         /** Asynchronous notification query */
#define VIRTIO_SCSI_T_AN_SUBSCRIBE                 2         /** Asynchronous notification subscription */

typedef struct virtio_scsi_ctrl_an
{
    // Device-readable part
    uint32_t  uType;                                         /** type            */
    uint8_t   uLUN[8];                                       /** lun             */
    uint32_t  uEventRequested;                               /** event_requested */
    // Device-writable part
    uint32_t  uEventActual;                                  /** event_actual    */
    uint8_t   uResponse;                                     /** response        */
}  VIRTIOSCSICTRLAN, *PVIRTIOSCSICTRLAN;

#define VIRTIO_SCSI_EVT_ASYNC_OPERATIONAL_CHANGE  2
#define VIRTIO_SCSI_EVT_ASYNC_POWER_MGMT          4
#define VIRTIO_SCSI_EVT_ASYNC_EXTERNAL_REQUEST    8
#define VIRTIO_SCSI_EVT_ASYNC_MEDIA_CHANGE        16
#define VIRTIO_SCSI_EVT_ASYNC_MULTI_HOST          32
#define VIRTIO_SCSI_EVT_ASYNC_DEVICE_BUSY         64

/** Device operation: controlq */

typedef struct virtio_scsi_ctrl
{
    uint32_t type;                                           /** type            */
    uint8_t  response;                                       /** response        */
} VIRTIOSCSICTRL, *PVIRTIOSCSICTRL;

#define VIRTIO_SCSI_T_NO_EVENT                   0

#define VIRTIO_SCSI_T_TRANSPORT_RESET            1
#define VIRTIO_SCSI_T_ASYNC_NOTIFY               2           /** Asynchronous notification */
#define VIRTIO_SCSI_T_PARAM_CHANGE               3

#define VIRTIO_SCSI_EVT_RESET_HARD               0
#define VIRTIO_SCSI_EVT_RESET_RESCAN             1
#define VIRTIO_SCSI_EVT_RESET_REMOVED            2

/** Device operation: eventq */

#define VIRTIO_SCSI_T_EVENTS_MISSED             0x80000000
typedef struct virtio_scsi_event {
    // Device-writable part
    uint32_t uEvent;                                         /** event:          */
    uint8_t  uLUN[8];                                        /** lun             */
    uint32_t uReason;                                        /** reason          */
} VIRTIOSCSIEVENT, *PVIRTIOSCSIEVENT;


/*********************************************************************************************************************************/

#ifdef BOOTABLE_SUPPORT_TBD
/** @callback_method_impl{FNIOMIOPORTIN} */
static DECLCALLBACK(int) virtioScsiR3BiosIoPortRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT uPort, uint8_t *pbDst,
                                                    uint32_t *pcTransfers, unsigned cb);
{
}
/** @callback_method_impl{FNIOMIOPORTOUT} */
static DECLCALLBACK(int) virtioScsiR3BiosIoPortWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT uPort, uint32_t u32, unsigned cb);
{
}
/** @callback_method_impl{FNIOMIOPORTOUTSTRING} */
static DECLCALLBACK(int) virtioScsiR3BiosIoPortWriteStr(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT uPort, const uint8_t *pbSrc,
                                                        uint32_t *pcTransfers, unsigned cb);
{
}
/** @callback_method_impl{FNIOMIOPORTINSTRING} */
static DECLCALLBACK(int) virtioScsiR3BiosIoPortReadStr(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT uPort, uint8_t *pbDst,
                                                       uint32_t *pcTransfers, unsigned cb);
{
}
#endif

/**
 * virtio-scsi debugger info callback.
 *
 * @param   pDevIns     The device instance.
 * @param   pHlp        The output helpers.
 * @param   pszArgs     The arguments.
 */
static DECLCALLBACK(void) virtioScsiR3Info(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    bool fVerbose = false;

    /* Parse arguments. */
    if (pszArgs)
        fVerbose = strstr(pszArgs, "verbose") != NULL;

    /* Show basic information. */
    pHlp->pfnPrintf(pHlp, "%s#%d: virtio-scsci ",
                    pDevIns->pReg->szName,
                    pDevIns->iInstance);
    pHlp->pfnPrintf(pHlp, "numTargets=%lu", pThis->cTargets);
}

/** @callback_method_impl{FNSSMDEVLIVEEXEC}  */
static DECLCALLBACK(int) virtioScsiR3LiveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uPass)
{
    LogFunc(("callback"));
    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    RT_NOREF(pThis);
    RT_NOREF(uPass);
    RT_NOREF(pSSM);
    return VINF_SSM_DONT_CALL_AGAIN;
}

/** @callback_method_impl{FNSSMDEVLOADEXEC}  */
static DECLCALLBACK(int) virtioScsiR3LoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    LogFunc(("callback"));
    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    RT_NOREF(pThis);
    RT_NOREF(uPass);
    RT_NOREF(pSSM);
    RT_NOREF(uVersion);
    return VINF_SSM_DONT_CALL_AGAIN;
}

/** @callback_method_impl{FNSSMDEVSAVEEXEC}  */
static DECLCALLBACK(int) virtioScsiR3SaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    LogFunc(("callback"));
    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    RT_NOREF(pThis);
    RT_NOREF(pSSM);
    return VINF_SUCCESS;
}

/** @callback_method_impl{FNSSMDEVLOADDONE}  */
static DECLCALLBACK(int) virtioScsiR3LoadDone(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    LogFunc(("callback"));
    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    RT_NOREF(pThis);
    RT_NOREF(pSSM);
    return VINF_SUCCESS;
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
PDMBOTHCBDECL(int) virtioScsiMMIORead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb)
{
    RT_NOREF_PV(pDevIns); RT_NOREF_PV(pvUser); RT_NOREF_PV(GCPhysAddr); RT_NOREF_PV(pv); RT_NOREF_PV(cb);
    LogFunc(("Read from MMIO area\n"));
    return VINF_SUCCESS;
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
PDMBOTHCBDECL(int) virtioScsiMMIOWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void const *pv, unsigned cb)
{
    RT_NOREF_PV(pDevIns); RT_NOREF_PV(pvUser); RT_NOREF_PV(GCPhysAddr); RT_NOREF_PV(pv); RT_NOREF_PV(cb);
    LogFunc(("Write to MMIO area\n"));
    return VINF_SUCCESS;
}

/**
 * @callback_method_impl{FNPCIIOREGIONMAP}
 */
static DECLCALLBACK(int) virtioScsiR3Map(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev, uint32_t iRegion,
                                           RTGCPHYS GCPhysAddress, RTGCPHYS cb, PCIADDRESSSPACE enmType)
{
    RT_NOREF(pPciDev, iRegion);
    PVIRTIOSCSI  pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    int rc = VINF_SUCCESS;

    Assert(cb >= 32);

    switch (iRegion)
    {
        case 0:
            LogFunc(("virtio-scsi controller MMIO mapped at GCPhysAddr=%RGp cb=%RGp\n", GCPhysAddress, cb));

            /* We use the assigned size here, because we currently only support page aligned MMIO ranges. */
            rc = PDMDevHlpMMIORegister(pDevIns, GCPhysAddress, cb, NULL /*pvUser*/,
                                   IOMMMIO_FLAGS_READ_PASSTHRU | IOMMMIO_FLAGS_WRITE_PASSTHRU,
                                   virtioScsiMMIOWrite, virtioScsiMMIORead,
                                   "virtio-scsi MMIO");
            pThis->GCPhysMMIOBase = RT_SUCCESS(rc) ? GCPhysAddress : 0;
            return rc;
        case 1:
            /* VirtIO 1.0 doesn't uses Port I/O (Virtio 0.95 e.g. "legacy", does) */
            AssertMsgFailed(("virtio-scsi: Port I/O not supported by this Host SCSI device\n"));
        default:
            AssertMsgFailed(("Invalid enmType=%d\n", enmType));
    }
    return VERR_GENERAL_FAILURE; /* Should never get here */
}


/**
 * @copydoc FNPDMDEVRESET
 */
static DECLCALLBACK(void) virtioScsiR3Reset(PPDMDEVINS pDevIns)
{
    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    NOREF(pThis);

//    ASMAtomicWriteBool(&pThis->fSignalIdle, true);
//    if (!virtioScsiR3AllAsyncIOIsFinished(pDevIns))
//        PDMDevHlpSetAsyncNotification(pDevIns, virtioScsiR3IsAsyncResetDone);
//    else
//    {
//        ASMAtomicWriteBool(&pThis->fSignalIdle, false);
//   }
}

static DECLCALLBACK(void) virtioScsiR3Relocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    LogFunc(("Relocating virtio-scsi"));
    RT_NOREF(offDelta);
    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);

    pThis->pDevInsR3 = pDevIns;

    for (uint32_t i = 0; i < VIRTIOSCSI_MAX_TARGETS; i++)
    {
        PVIRTIOSCSITARGET pTarget = &pThis->aTargetInstances[i];
        pTarget->pVirtioScsiR3 = pThis;;
    }

}

static DECLCALLBACK(int) virtioScsiR3QueryDeviceLocation(PPDMIMEDIAPORT pInterface, const char **ppcszController,
                                                       uint32_t *piInstance, uint32_t *piLUN)
{
    PVIRTIOSCSITARGET pVirtioScsiTarget = RT_FROM_MEMBER(pInterface, VIRTIOSCSITARGET, IMediaPort);
    PPDMDEVINS pDevIns = pVirtioScsiTarget->CTX_SUFF(pVirtioScsi)->CTX_SUFF(pDevIns);

    AssertPtrReturn(ppcszController, VERR_INVALID_POINTER);
    AssertPtrReturn(piInstance, VERR_INVALID_POINTER);
    AssertPtrReturn(piLUN, VERR_INVALID_POINTER);

    *ppcszController = pDevIns->pReg->szName;
    *piInstance = pDevIns->iInstance;
    *piLUN = pVirtioScsiTarget->iLUN;

    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMIMEDIAEXPORT,pfnIoReqCopyFromBuf}
 */
static DECLCALLBACK(int) virtioScsiR3IoReqCopyFromBuf(PPDMIMEDIAEXPORT pInterface, PDMMEDIAEXIOREQ hIoReq,
                                                    void *pvIoReqAlloc, uint32_t offDst, PRTSGBUF pSgBuf,
                                                    size_t cbCopy)
{
    PVIRTIOSCSITARGET pTarget = RT_FROM_MEMBER(pInterface, VIRTIOSCSITARGET, IMediaExPort);
    PVIRTIOSCSIREQ pReq = (PVIRTIOSCSIREQ)pvIoReqAlloc;
    size_t cbCopied = 0;
    RT_NOREF(pTarget);
    RT_NOREF(pReq);
    RT_NOREF(pInterface);
    RT_NOREF(pvIoReqAlloc);
    RT_NOREF(offDst);
    RT_NOREF(pSgBuf);
    RT_NOREF(hIoReq);
    RT_NOREF(cbCopy);
    RT_NOREF(cbCopied);

/*
    if (RT_UNLIKELY(pReq->fBIOS))
        cbCopied = vboxscsiCopyToBuf(&pTarget->CTX_SUFF(pVirtioScsi)->VBoxSCSI, pSgBuf, offDst, cbCopy);
    else
        cbCopied = virtioScsiR3CopySgBufToGuest(pTarget->CTX_SUFF(pVirtioScsi), pReq, pSgBuf, offDst, cbCopy);
    return cbCopied == cbCopy ? VINF_SUCCESS : VERR_PDM_MEDIAEX_IOBUF_OVERFLOW;
*/
    return 0; /* placeholder */
}

/**
 * @interface_method_impl{PDMIMEDIAEXPORT,pfnIoReqCopyToBuf}
 */
static DECLCALLBACK(int) virtioScsiR3IoReqCopyToBuf(PPDMIMEDIAEXPORT pInterface, PDMMEDIAEXIOREQ hIoReq,
                                                  void *pvIoReqAlloc, uint32_t offSrc, PRTSGBUF pSgBuf,
                                                  size_t cbCopy)
{
    PVIRTIOSCSITARGET pTarget = RT_FROM_MEMBER(pInterface, VIRTIOSCSITARGET, IMediaExPort);
    PVIRTIOSCSIREQ pReq = (PVIRTIOSCSIREQ)pvIoReqAlloc;
    size_t cbCopied = 0;
    RT_NOREF(pTarget);
    RT_NOREF(pReq);
    RT_NOREF(pInterface);
    RT_NOREF(pvIoReqAlloc);
    RT_NOREF(offSrc);
    RT_NOREF(pSgBuf);
    RT_NOREF(hIoReq);
    RT_NOREF(cbCopy);
    RT_NOREF(cbCopied);

/*
    if (RT_UNLIKELY(pReq->fBIOS))
        cbCopied = vboxscsiCopyFromBuf(&pTarget->CTX_SUFF(pVirtioScsi)->VBoxSCSI, pSgBuf, offSrc, cbCopy);
    else
        cbCopied = vboxscsiR3CopySgBufFromGuest(pTarget->CTX_SUFF(pVirtioScsi), pReq, pSgBuf, offSrc, cbCopy);
    return cbCopied == cbCopy ? VINF_SUCCESS : VERR_PDM_MEDIAEX_IOBUF_UNDERRUN;
*/
    return 0; /* placeholder */

}

/**
 * @interface_method_impl{PDMIMEDIAEXPORT,pfnIoReqCompleteNotify}
 */
static DECLCALLBACK(int) virtioScsiR3IoReqCompleteNotify(PPDMIMEDIAEXPORT pInterface, PDMMEDIAEXIOREQ hIoReq,
                                                       void *pvIoReqAlloc, int rcReq)
{
    PVIRTIOSCSITARGET pTarget = RT_FROM_MEMBER(pInterface, VIRTIOSCSITARGET, IMediaExPort);
    RT_NOREF(pTarget);
    RT_NOREF(pInterface);
    RT_NOREF(pvIoReqAlloc);
    RT_NOREF(rcReq);
    RT_NOREF(hIoReq);
//    virtioScsiR3ReqComplete(pTarget->CTX_SUFF(pVirtioScsi), (VIRTIOSCSIREQ)pvIoReqAlloc, rcReq);
    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMIMEDIAEXPORT,pfnIoReqStateChanged}
 */
static DECLCALLBACK(void) virtioScsiR3IoReqStateChanged(PPDMIMEDIAEXPORT pInterface, PDMMEDIAEXIOREQ hIoReq,
                                                      void *pvIoReqAlloc, PDMMEDIAEXIOREQSTATE enmState)
{

    RT_NOREF4(pInterface, hIoReq, pvIoReqAlloc, enmState);
    PVIRTIOSCSITARGET pTarget = RT_FROM_MEMBER(pInterface, VIRTIOSCSITARGET, IMediaExPort);

    switch (enmState)
    {
        case PDMMEDIAEXIOREQSTATE_SUSPENDED:
        {
            /* Make sure the request is not accounted for so the VM can suspend successfully. */
            uint32_t cTasksActive = ASMAtomicDecU32(&pTarget->cOutstandingRequests);
            if (!cTasksActive && pTarget->CTX_SUFF(pVirtioScsi)->fSignalIdle)
                PDMDevHlpAsyncNotificationCompleted(pTarget->CTX_SUFF(pVirtioScsi)->pDevInsR3);
            break;
        }
        case PDMMEDIAEXIOREQSTATE_ACTIVE:
            /* Make sure the request is accounted for so the VM suspends only when the request is complete. */
            ASMAtomicIncU32(&pTarget->cOutstandingRequests);
            break;
        default:
            AssertMsgFailed(("Invalid request state given %u\n", enmState));
    }
}

/**
 * @interface_method_impl{PDMIMEDIAEXPORT,pfnMediumEjected}
 */
static DECLCALLBACK(void) virtioScsiR3MediumEjected(PPDMIMEDIAEXPORT pInterface)
{
    PVIRTIOSCSITARGET pTarget = RT_FROM_MEMBER(pInterface, VIRTIOSCSITARGET, IMediaExPort);
    PVIRTIOSCSI pThis = pTarget->CTX_SUFF(pVirtioScsi);

    if (pThis->pMediaNotify)
        virtioSetWriteLed(&(pThis->virtioState), false);
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
static DECLCALLBACK(bool) virtioScsiR3NotifyQueueConsumer(PPDMDEVINS pDevIns, PPDMQUEUEITEMCORE pItem)
{
    RT_NOREF(pItem);
    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);

    int rc = SUPSemEventSignal(pThis->pSupDrvSession, pThis->hEvtProcess);
    AssertRC(rc);

    return true;
}


/**
 * virtio-scsi VirtIO Device-specific capabilities read callback
 * (other VirtIO capabilities and features are handled in VirtIO implementation)
 *
 * @param   pDevIns     The device instance.
 * @param   GCPhysAddr  Guest driver physical address to read
 * @param   pvBuf       Buffer in which to save read data
 * @param   cbRead      Number of bytes to read
 */
static DECLCALLBACK(int) virtioScsiR3DevCapRead(PPDMDEVINS pDevIns, RTGCPHYS GCPhysAddr, const void *pvBuf, size_t cbRead)
{
/*TBD*/
    LogFlowFunc(("Read from Device-Specific capabilities, callback to VirtIO client\n"));
    RT_NOREF(pDevIns);
    RT_NOREF(GCPhysAddr);
    RT_NOREF(pvBuf);
    RT_NOREF(cbRead);
    int rv = VINF_SUCCESS;
    return rv;
}

/**
 * virtio-scsi VirtIO Device-specific capabilities read callback
 * (other VirtIO capabilities and features are handled in VirtIO implementation)
 *
 * @param   pDevIns     The device instance.
 * @param   GCPhysAddr  Guest driver physical address to write
 * @param   pvBuf       Buffer in which to save read data
 * @param   cbWrite     Number of bytes to write
 */
static DECLCALLBACK(int) virtioScsiR3DevCapWrite(PPDMDEVINS pDevIns, RTGCPHYS GCPhysAddr, const void *pvBuf, size_t cbWrite)
{
/*TBD*/
    LogFlowFunc(("Write to Device-Specific capabilities, callback to VirtIO client\n"));
    RT_NOREF(pDevIns);
    RT_NOREF(GCPhysAddr);
    RT_NOREF(pvBuf);
    RT_NOREF(cbWrite);
    int rv = VINF_SUCCESS;
    return rv;
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
static DECLCALLBACK(void) virtioScsiR3Detach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    RT_NOREF(fFlags);
    PVIRTIOSCSI       pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    PVIRTIOSCSITARGET pTarget = &pThis->aTargetInstances[iLUN];

    Log(("%s:\n", __FUNCTION__));

    AssertMsg(fFlags & PDM_TACH_FLAGS_NOT_HOT_PLUG,
              ("virtio-scsi: Device does not support hotplugging\n"));

    /*
     * Zero some important members.
     */
    pTarget->fPresent    = false;
    pTarget->pUpstreamDrvBase    = NULL;
}


/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) virtioScsiR3DeviceQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS pDevIns = PDMIBASE_2_PDMDRV(pInterface);
    PVIRTIOSCSITARGET pTarget = RT_FROM_MEMBER(pInterface, VIRTIOSCSITARGET, IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDevIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIMEDIAPORT, &pTarget->IMediaPort);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIMEDIAEXPORT, &pTarget->IMediaExPort);
    return NULL;
}

/**
 * Attach command.
 *
 * This is called when we change block driver.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   iLUN        The logical unit which is being detached.
 * @param   fFlags      Flags, combination of the PDMDEVATT_FLAGS_* \#defines.
 */
static DECLCALLBACK(int)  virtioScsiR3Attach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    PVIRTIOSCSI       pThis   = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    PVIRTIOSCSITARGET pTarget = &pThis->aTargetInstances[iLUN];
    int rc;

    pThis->pDevInsR3    = pDevIns;
    pThis->pDevInsR0    = PDMDEVINS_2_R0PTR(pDevIns);
    pThis->pDevInsRC    = PDMDEVINS_2_RCPTR(pDevIns);

    AssertMsgReturn(fFlags & PDM_TACH_FLAGS_NOT_HOT_PLUG,
                    ("virtio-scsi: Device does not support hotplugging\n"),
                    VERR_INVALID_PARAMETER);

    /* the usual paranoia */
    AssertRelease(!pTarget->pUpstreamDrvBase);
    Assert(pTarget->iLUN == iLUN);

    /*
     * Try attach the SCSI driver and get the interfaces,
     * required as well as optional.
     */
    rc = PDMDevHlpDriverAttach(pDevIns, pTarget->iLUN, &pDevIns->IBase,
                               &pTarget->pUpstreamDrvBase, (const char *)&pTarget->pszLunName);
    if (RT_SUCCESS(rc))
        pTarget->fPresent = true;
    else
        AssertMsgFailed(("Failed to attach %s. rc=%Rrc\n", pTarget->pszLunName, rc));

    if (RT_FAILURE(rc))
    {
        pTarget->fPresent = false;
        pTarget->pUpstreamDrvBase = NULL;
    }
    return rc;
}

static DECLCALLBACK(int) virtioScsiDestruct(PPDMDEVINS pDevIns)
{
    /*
     * Check the versions here as well since the destructor is *always* called.
     */
    PDMDEV_CHECK_VERSIONS_RETURN_QUIET(pDevIns);
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) virtioScsiConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{

    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);
    PVIRTIOSCSI  pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    int  rc = VINF_SUCCESS;
    bool fBootable = true;

    LogFunc(("PDM device instance: %d\n", iInstance));

    /*
     * Validate and read configuration.
     */
    if (!CFGMR3AreValuesValid(pCfg,"NumTargets\0"
                                   "Bootable\0"
                                /* "GCEnabled\0"    TBD */
                                /* "R0Enabled\0"    TBD */
    ))
    return PDMDEV_SET_ERROR(pDevIns, VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES,
                                N_("virtio-scsi configuration error: unknown option specified"));

    rc = CFGMR3QueryIntegerDef(pCfg, "NumTargets", &pThis->cTargets, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("virtio-scsi configuration error: failed to read NumTargets as integer"));
    LogFunc(("NumTargets=%d\n", pThis->cTargets));

    rc = CFGMR3QueryBoolDef(pCfg, "Bootable", &fBootable, true);
    if (RT_FAILURE(rc))
         return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("virtio-scsi configuration error: failed to read Bootable as boolean"));
    LogFunc(("Bootable=%RTbool (unimplemented)\n", fBootable));

    VIRTIOPCIPARAMS virtioPciParams, *pVirtioPciParams = &virtioPciParams;
    pVirtioPciParams->uDeviceId      = PCI_DEVICE_ID_VIRTIO_SCSI_HOST;
    pVirtioPciParams->uClassBase     = PCI_CLASS_BASE_MASS_STORAGE;
    pVirtioPciParams->uClassSub      = PCI_CLASS_SUB_SCSI_STORAGE_CONTROLLER;
    pVirtioPciParams->uClassProg     = PCI_CLASS_PROG_UNSPECIFIED;
    pVirtioPciParams->uSubsystemId   = PCI_DEVICE_ID_VIRTIO_SCSI_HOST;  /* Virtio 1.0 spec allows PCI Device ID here */
    pVirtioPciParams->uInterruptLine = 0x00;
    pVirtioPciParams->uInterruptPin  = 0x01;


    PVIRTIOSTATE pVirtioScsi = &(pThis->virtioState);
    rc = virtioConstruct(pDevIns, pVirtioScsi, iInstance, pVirtioPciParams,
                         VIRTIO_SCSI_NAME_FMT, VIRTIO_SCSI_N_QUEUES, VIRTIO_SCSI_REGION_PCI_CAP,
                         virtioScsiR3DevCapRead, virtioScsiR3DevCapWrite, sizeof(VIRTIODEVCFG));
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("virtio-scsi: failed to initialize VirtIO"));

    rc = PDMDevHlpPCIIORegionRegister(pDevIns, VIRTIO_SCSI_REGION_PORT_IO, 32,
                                      PCI_ADDRESS_SPACE_MEM, virtioScsiR3Map);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("virtio-scsi: cannot register PCI mmio address space"));

#ifdef BOOTABLE_SUPPORT_TBD
    if (fBootable)
    {
        /* Register I/O port space for BIOS access. */
        rc = PDMDevHlpIOPortRegister(pDevIns, VIRTIOSCSI_BIOS_IO_PORT, 4, NULL,
                                     virtioScsiR3BiosIoPortWrite, virtioScsiR3BiosIoPortRead,
                                     virtioScsiR3BiosIoPortWriteStr, virtioScsiR3BiosIoPortReadStr,
                                     "virtio-scsi BIOS");
        if (RT_FAILURE(rc))
            return PDMDEV_SET_ERROR(pDevIns, rc, N_("virtio-scsi cannot register BIOS I/O handlers"));
    }
#endif

    /* Initialize task queue. */
    rc = PDMDevHlpQueueCreate(pDevIns, sizeof(PDMQUEUEITEMCORE), 5, 0,
                              virtioScsiR3NotifyQueueConsumer, true, "VirtioTask", &pThis->pNotifierQueueR3);
    if (RT_FAILURE(rc))
        return rc;

    /* Initialize per device instance. */
    for (RTUINT iLUN = 0; iLUN < VIRTIOSCSI_MAX_TARGETS; iLUN++)
    {
        PVIRTIOSCSITARGET pTarget = &pThis->aTargetInstances[iLUN];

        if (RTStrAPrintf(&pTarget->pszLunName, "VSCSI%u", iLUN) < 0)
            AssertLogRelFailedReturn(VERR_NO_MEMORY);

        /* Initialize static parts of the device. */
        pTarget->iLUN = iLUN;
        pTarget->pVirtioScsiR3 = pThis;
        /* IMediaPort and IMediaExPort interfaces provide callbacks for VD media and downstream driver access */
        pTarget->IMediaPort.pfnQueryDeviceLocation       = virtioScsiR3QueryDeviceLocation;
        pTarget->IMediaExPort.pfnIoReqCompleteNotify     = virtioScsiR3IoReqCompleteNotify;
        pTarget->IMediaExPort.pfnIoReqCopyFromBuf        = virtioScsiR3IoReqCopyFromBuf;
        pTarget->IMediaExPort.pfnIoReqCopyToBuf          = virtioScsiR3IoReqCopyToBuf;
        pTarget->IMediaExPort.pfnIoReqStateChanged       = virtioScsiR3IoReqStateChanged;
        pTarget->IMediaExPort.pfnMediumEjected           = virtioScsiR3MediumEjected;
        pTarget->IMediaExPort.pfnIoReqQueryBuf           = NULL;
        pTarget->IMediaExPort.pfnIoReqQueryDiscardRanges = NULL;
        pDevIns->IBase.pfnQueryInterface                 = virtioScsiR3DeviceQueryInterface;

        LogFunc(("Attaching LUN: %s\n", pTarget->pszLunName));

        /* Attach this SCSI driver (upstream driver pre-determined statically outside this module) */
        AssertReturn(iLUN < RT_ELEMENTS(pThis->aTargetInstances), VERR_PDM_NO_SUCH_LUN);
        rc = PDMDevHlpDriverAttach(pDevIns, iLUN, &pDevIns->IBase, &pTarget->pUpstreamDrvBase, (const char *)&pTarget->pszLunName);
        if (RT_SUCCESS(rc))
        {
            pTarget->fPresent = true;

            pTarget->pDrvMedia = PDMIBASE_QUERY_INTERFACE(pTarget->pUpstreamDrvBase, PDMIMEDIA);
            AssertMsgReturn(VALID_PTR(pTarget->pDrvMedia),
                         ("virtio-scsi configuration error: LUN#%d missing basic media interface!\n", pTarget->iLUN),
                         VERR_PDM_MISSING_INTERFACE);

             /* Get the extended media interface. */
             pTarget->pDrvMediaEx = PDMIBASE_QUERY_INTERFACE(pTarget->pUpstreamDrvBase, PDMIMEDIAEX);
             AssertMsgReturn(VALID_PTR(pTarget->pDrvMediaEx),
                         ("virtio-scsi configuration error: LUN#%d missing extended media interface!\n", pTarget->iLUN),
                         VERR_PDM_MISSING_INTERFACE);

// pk: Not sure if this is needed here yet with VirtIO or DrvScsi, will investigate after basic VirtIO working.
//            rc = pTarget->pDrvMediaEx->pfnIoReqAllocSizeSet(pTarget->pDrvMediaEx, sizeof(BUSLOGICREQ));
//             AssertMsgRCReturn(rc, ("BusLogic configuration error: LUN#%u: Failed to set I/O request size!", pTarget->iLUN),

        }
        else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
        {
            pTarget->fPresent = false;
            pTarget->pUpstreamDrvBase = NULL;
            rc = VINF_SUCCESS;
            Log(("virtio-scsi: no driver attached to device %s\n", pTarget->pszLunName));
        }
        else
        {
            AssertLogRelMsgFailed(("virtio-scsi: Failed to attach %s\n", pTarget->pszLunName));
            return rc;
        }
    }

    rc = PDMDevHlpSSMRegisterEx(pDevIns, VIRTIOSCSI_SAVED_STATE_MINOR_VERSION, sizeof(*pThis), NULL,
                                NULL, virtioScsiR3LiveExec, NULL,
                                NULL, virtioScsiR3SaveExec, NULL,
                                NULL, virtioScsiR3LoadExec, virtioScsiR3LoadDone);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("virtio-scsi cannot register save state handlers"));
    /*
     * Register the debugger info callback.
     */
    char szTmp[128];
    RTStrPrintf(szTmp, sizeof(szTmp), "%s%d", pDevIns->pReg->szName, pDevIns->iInstance);
    PDMDevHlpDBGFInfoRegister(pDevIns, szTmp, "virtio-scsi info", virtioScsiR3Info);

    return rc;
}

/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceVirtioSCSI =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szName */
    "virtio-scsi",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Virtio SCSI.\n",
    /* fFlags */
#ifdef VIRTIOSCSI_GC_SUPPORT
    PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_RC | PDM_DEVREG_FLAGS_R0,
#else
    PDM_DEVREG_FLAGS_DEFAULT_BITS,
#endif
    /* fClass */
    PDM_DEVREG_CLASS_MISC,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(VIRTIOSCSI),
    /* pfnConstruct */
    virtioScsiConstruct,
    /* pfnDestruct */
    virtioScsiDestruct,
    /* pfnRelocate */
    virtioScsiR3Relocate,
    /* pfnMemSetup */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    virtioScsiR3Reset,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    virtioScsiR3Attach,
    /* pfnDetach */
    virtioScsiR3Detach,
    /* pfnQueryInterface */
    NULL,
    /* pfnInitComplete */
    NULL,
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32VersionEnd */
    PDM_DEVREG_VERSION
};

