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

#define VIRTIOSCSI_MAX_TARGETS                      1            /**< Can probably determined from higher layers     */

/**
 * @name VirtIO 1.0 SCSI Host feature bits (See VirtIO 1.0 specification, Section 5.6.3)
 * @{  */
#define VIRTIOSCSI_F_INOUT                RT_BIT_64(0)           /** Request is device readable AND writeable         */
#define VIRTIOSCSI_F_HOTPLUG              RT_BIT_64(1)           /** Host allows hotplugging SCSI LUNs & targets      */
#define VIRTIOSCSI_F_CHANGE               RT_BIT_64(2)           /** Host LUNs chgs via VIRTIOSCSI_T_PARAM_CHANGE evt */
#define VIRTIOSCSI_F_T10_PI               RT_BIT_64(3)           /** Add T10 port info (DIF/DIX) in SCSI req hdr      */
/** @} */

#define VIRTIOSCSI_HOST_SCSI_FEATURES_OFFERED \
            (VIRTIOSCSI_F_INOUT | VIRTIOSCSI_F_HOTPLUG | VIRTIOSCSI_F_CHANGE | VIRTIOSCSI_F_T10_PI)

/**
 * TEMPORARY NOTE: following parameter is set to 1 for early development. Will be increased later
 */
#define VIRTIOSCSI_REQ_QUEUE_CNT                    1            /**< Number of req queues exposed by dev.            */
#define VIRTIOSCSI_SAVED_STATE_MINOR_VERSION        0x01         /**< SSM version #                                   */
#define PCI_DEVICE_ID_VIRTIOSCSI_HOST               0x1048       /**< Informs guest driver of type of VirtIO device   */
#define PCI_CLASS_BASE_MASS_STORAGE                 0x01         /**< PCI Mass Storage device class                   */
#define PCI_CLASS_SUB_SCSI_STORAGE_CONTROLLER       0x00         /**< PCI SCSI Controller subclass                    */
#define PCI_CLASS_PROG_UNSPECIFIED                  0x00         /**< Programming interface. N/A.                     */
#define VIRTIOSCSI_PCI_CLASS                        0x01         /**< Base class Mass Storage?                        */
#define REQ_ALLOC_SIZE                              1024         /**< Allocation size. Need to investigate optimal    */

/**
 * VirtIO SCSI Host Device device-specific queue indicies
 *
 * Virtqs (and their indices) are specified for a SCSI Host Device as described in the VirtIO 1.0 specification
 * section 5.6. Thus there is no need to explicitly indicate the number of queues needed by this device. The number
 * of req queues is variable and determined by virtio_scsi_config.num_queues. See VirtIO 1.0 spec section 5.6.4
 */
#define VIRTIOSCSI_VIRTQ_CONTROLQ                   0            /* Index of control queue                            */
#define VIRTIOSCSI_VIRTQ_EVENTQ                     1            /* Index of event queue                              */
#define VIRTIOSCSI_VIRTQ_REQ_BASE                   2            /* Base index of req queues                          */

/**
 * The following struct is the VirtIO SCSI Host Device device-specific configuration described in section 5.6.4
 * of the VirtIO 1.0 specification. This layout maps an MMIO area shared VirtIO guest driver. The VBox VirtIO
 * this virtual controller device implementation is a client of. The frame work calls back whenever the guest driver
 * accesses any part of field in this struct
 */
typedef struct virtio_scsi_config
{
    uint32_t uNumQueues;                                         /**< num_queues       # of req q's exposed by dev    */
    uint32_t uSegMax;                                            /**< seg_max          Max # of segs allowed in cmd   */
    uint32_t uMaxSectors;                                        /**< max_sectors      Hint to guest max xfer to use  */
    uint32_t uCmdPerLun;                                         /**< cmd_per_lun      Max # of link cmd sent per lun */
    uint32_t uEventInfoSize;                                     /**< event_info_size  Fill max, evtq bufs            */
    uint32_t uSenseSize;                                         /**< sense_size       Max sense data size dev writes */
    uint32_t uCdbSize;                                           /**< cdb_size         Max CDB size driver writes     */
    uint16_t uMaxChannel;                                        /**< max_channel      Hint to guest driver           */
    uint16_t uMaxTarget;                                         /**< max_target       Hint to guest driver           */
    uint32_t uMaxLun;                                            /**< max_lun          Hint to guest driver           */
} VIRTIO_SCSI_CONFIG_T, PVIRTIO_SCSI_CONFIG_T;

/**
 * Device operation: controlq
 */
typedef struct virtio_scsi_ctrl
{
    uint32_t type;                                              /**< type                                             */
    uint8_t  response;                                          /**< response                                         */
} VIRTIO_SCSI_CTRL_T, *PVIRTIO_SCSI_CTRL_T;

/**
 * @name VirtIO 1.0 SCSI Host Device device specific control types
 * @{  */
#define VIRTIOSCSI_T_NO_EVENT                   0
#define VIRTIOSCSI_T_TRANSPORT_RESET            1
#define VIRTIOSCSI_T_ASYNC_NOTIFY               2              /**< Asynchronous notification                         */
#define VIRTIOSCSI_T_PARAM_CHANGE               3
/** @} */

/**
 * Device operation: eventq
 */
#define VIRTIOSCSI_T_EVENTS_MISSED             0x80000000
typedef struct virtio_scsi_event {
    // Device-writable part
    uint32_t uEvent;                                           /**< event:                                           */
    uint8_t  uLUN[8];                                          /**< lun                                              */
    uint32_t uReason;                                          /**< reason                                           */
} VIRTIO_SCSI_EVENT_T, *PVIRTIO_SCSI_EVENT_T;

/**
 * @name VirtIO 1.0 SCSI Host Device device specific event types
 * @{  */
#define VIRTIOSCSI_EVT_RESET_HARD               0              /**<                                                  */
#define VIRTIOSCSI_EVT_RESET_RESCAN             1              /**<                                                  */
#define VIRTIOSCSI_EVT_RESET_REMOVED            2              /**<                                                  */
/** @} */

/**
 * Device operation: reqestq
 *
 * The following struct is described in VirtIO 1.0 Specification, section 5.6.6.1
 */
#define CDB_SIZE                                 32             /**< Mandated by VirtIO 1.0 spec section 5.6.6.1     */
#define SENSE_SIZE                               96             /**< See section VirtIO 1.0, section 5.6.4           */
#define PI_BYTES_IN                              1              /**< Value TBD (see section 5.6.6.1)                 */
#define PI_BYTES_OUT                             1              /**< Value TBD (see section 5.6.6.1)                 */
#define DATA_OUT                                512             /**< Value TBD (see section 5.6.6.1)                 */

typedef struct virtio_scsi_req_cmd
{
    /* Device-readable part */
    uint8_t  uLUN[8];                                           /**< lun                                             */
    uint64_t uId;                                               /**< id                                              */
    uint8_t  uTaskAttr;                                         /**< task_attr                                       */
    uint8_t  uPrio;                                             /**< prio                                            */
    uint8_t  uCrn;                                              /**< crn                                             */
    uint8_t  uCdb[CDB_SIZE];                                    /**< cdb              VirtIO 1.0 mandates 32-bytes   */

    /** Following three fields only present if VIRTIOSCSI_F_T10_PI negotiated */

    uint32_t uPiBytesOut;                                       /**< pi_bytesout                                     */
    uint32_t uPiBytesIn;                                        /**< pi_bytesin                                      */
    uint8_t  uPiOut[PI_BYTES_OUT];                              /**< pi_out[]        TBD                             */

    uint8_t  uDataOut[DATA_OUT];                                /**< dataout                                         */

    /* Device-writable part */
    uint32_t uSenseLen;                                         /**< sense_len                                       */
    uint32_t uResidual;                                         /**< residual                                        */
    uint16_t uStatusQualifier;                                  /**< status_qualifier                                */
    uint8_t  uStatus;                                           /**< status                                          */
    uint8_t  uResponse;                                         /**< response                                        */
    uint8_t  uSense[SENSE_SIZE];                                /**< sense                                           */

    /** Following two fields only present if VIRTIOSCSI_F_T10_PI negotiated */
    uint8_t  uPiIn[PI_BYTES_IN];                                /**< pi_in[]                                         */
    uint8_t  uDataIn[];                                         /**< detain;                                         */
} VIRTIO_SCSI_REQ_CMD_T, *PVIRTIO_SCSI_REQ_CMD_T;

/**
 * @name VirtIO 1.0 SCSI Host Device Req command-specific response values
 * @{  */
#define VIRTIOSCSI_S_OK                            0           /**< control, command                                 */
#define VIRTIOSCSI_S_OVERRUN                       1           /**< control                                          */
#define VIRTIOSCSI_S_ABORTED                       2           /**< control                                          */
#define VIRTIOSCSI_S_BAD_TARGET                    3           /**< control, command                                 */
#define VIRTIOSCSI_S_RESET                         4           /**< control                                          */
#define VIRTIOSCSI_S_BUSY                          5           /**< control, command                                 */
#define VIRTIOSCSI_S_TRANSPORT_FAILURE             6           /**< control, command                                 */
#define VIRTIOSCSI_S_TARGET_FAILURE                7           /**< control, command                                 */
#define VIRTIOSCSI_S_NEXUS_FAILURE                 8           /**< control, command                                 */
#define VIRTIOSCSI_S_FAILURE                       9           /**< control, command                                 */
#define VIRTIOSCSI_S_INCORRECT_LUN                12           /**< command                                          */
/** @} */

/**
 * @name VirtIO 1.0 SCSI Host Device command-specific task_attr values
 * @{  */
#define VIRTIOSCSI_S_SIMPLE                        0           /**<                                                  */
#define VIRTIOSCSI_S_ORDERED                       1           /**<                                                  */
#define VIRTIOSCSI_S_HEAD                          2           /**<                                                  */
#define VIRTIOSCSI_S_ACA                           3           /**<                                                  */
/** @} */

/**
 * @name VirtIO 1.0 SCSI Host Device command-specific TMF values
 * @{  */
#define VIRTIOSCSI_T_TMF                           0           /**<                                                  */
#define VIRTIOSCSI_T_TMF_ABORT_TASK                0           /**<                                                  */
#define VIRTIOSCSI_T_TMF_ABORT_TASK_SET            1           /**<                                                  */
#define VIRTIOSCSI_T_TMF_CLEAR_ACA                 2           /**<                                                  */
#define VIRTIOSCSI_T_TMF_CLEAR_TASK_SET            3           /**<                                                  */
#define VIRTIOSCSI_T_TMF_I_T_NEXUS_RESET           4           /**<                                                  */
#define VIRTIOSCSI_T_TMF_LOGICAL_UNIT_RESET        5           /**<                                                  */
#define VIRTIOSCSI_T_TMF_QUERY_TASK                6           /**<                                                  */
#define VIRTIOSCSI_T_TMF_QUERY_TASK_SET            7           /**<                                                  */
/*** @} */

typedef struct virtio_scsi_ctrl_tmf
{
     // Device-readable part
    uint32_t uType;                                            /** type                                              */
    uint32_t uSubtype;                                         /** subtype                                           */
    uint8_t  uLUN[8];                                          /** lun                                               */
    uint64_t uId;                                              /** id                                                */
    // Device-writable part
    uint8_t  uResponse;                                        /** response                                          */
} VIRTIO_SCSI_CTRL_BUF_T, *PVIRTIO_SCSI_CTRL_BUF_T;

/**
 * @name VirtIO 1.0 SCSI Host Device device specific tmf control response values
 * @{  */
#define VIRTIOSCSI_S_FUNCTION_COMPLETE            0           /**<                                                   */
#define VIRTIOSCSI_S_FUNCTION_SUCCEEDED           10          /**<                                                   */
#define VIRTIOSCSI_S_FUNCTION_REJECTED            11          /**<                                                   */
/** @} */

#define VIRTIOSCSI_T_AN_QUERY                     1           /** Asynchronous notification query                    */
#define VIRTIOSCSI_T_AN_SUBSCRIBE                 2           /** Asynchronous notification subscription             */

typedef struct virtio_scsi_ctrl_an
{
    // Device-readable part
    uint32_t  uType;                                          /** type                                               */
    uint8_t   uLUN[8];                                        /** lun                                                */
    uint32_t  uEventRequested;                                /** event_requested                                    */
    // Device-writable part
    uint32_t  uEventActual;                                   /** event_actual                                       */
    uint8_t   uResponse;                                      /** response                                           */
}  VIRTIO_SCSI_CTRL_AN, *PVIRTIO_SCSI_CTRL_AN_T;

/**
 * @name VirtIO 1.0 SCSI Host Device device specific tmf control response values
 * @{  */
#define VIRTIOSCSI_EVT_ASYNC_OPERATIONAL_CHANGE  2           /**<                                                   */
#define VIRTIOSCSI_EVT_ASYNC_POWER_MGMT          4           /**<                                                   */
#define VIRTIOSCSI_EVT_ASYNC_EXTERNAL_REQUEST    8           /**<                                                   */
#define VIRTIOSCSI_EVT_ASYNC_MEDIA_CHANGE        16          /**<                                                   */
#define VIRTIOSCSI_EVT_ASYNC_MULTI_HOST          32          /**<                                                   */
#define VIRTIOSCSI_EVT_ASYNC_DEVICE_BUSY         64          /**<                                                   */
/** @} */

/**
 * State of a target attached to the VirtIO SCSI Host
 */
typedef struct VIRTIOSCSITARGET
{
    /** Pointer to PCI device that owns this target instance. - R3 pointer */
    R3PTRTYPE(struct VIRTIOSCSI *)  pVirtioScsiR3;

    /** Pointer to attached driver's base interface. */
    R3PTRTYPE(PPDMIBASE)            pDrvBase;

    /** Target LUN */
    RTUINT                          iLUN;

    /** Target LUN Description */
    char *                          pszLunName;

    /** Target base interface. */
    PDMIBASE                        IBase;

    /** Flag whether device is present. */
    bool                            fPresent;

    /** Media port interface. */
    PDMIMEDIAPORT                   IMediaPort;

    /** Pointer to the attached driver's media interface. */
    R3PTRTYPE(PPDMIMEDIA)           pDrvMedia;

    /** Extended media port interface. */
    PDMIMEDIAEXPORT                 IMediaExPort;

     /** Pointer to the attached driver's extended media interface. */
    R3PTRTYPE(PPDMIMEDIAEX)         pDrvMediaEx;

    /** Status LED interface */
    PDMILEDPORTS                    ILed;
    /** The status LED state for this device. */
    PDMLED                          led;

    /** Number of outstanding tasks on the port. */
    volatile uint32_t               cOutstandingRequests;

} VIRTIOSCSITARGET, *PVIRTIOSCSITARGET;


/**
 *  PDM instance data (state) for VirtIO Host SCSI device
 *
 * @extends     PDMPCIDEV
 */
typedef struct VIRTIOSCSI
{
    /* Opaque handle to VirtIO common framework (must be first item
     * in this struct so PDMINS_2_DATA macro's casting works) */
    VIRTIOHANDLE                    hVirtio;

    /* SCSI target instances data */
    VIRTIOSCSITARGET                aTargetInstances[VIRTIOSCSI_MAX_TARGETS];

    const char                      szInstance[16];

    /** Device base interface. */
    PDMIBASE                        IBase;


    /** Pointer to the device instance. - R3 ptr. */
    PPDMDEVINSR3                    pDevInsR3;
    /** Pointer to the device instance. - R0 ptr. */
    PPDMDEVINSR0                    pDevInsR0;
    /** Pointer to the device instance. - RC ptr. */
    PPDMDEVINSRC                    pDevInsRC;

    /** Status LUN: LEDs port interface. */
    PDMILEDPORTS                    ILeds;

    /** Status LUN: Partner of ILeds. */
    R3PTRTYPE(PPDMILEDCONNECTORS)   pLedsConnector;

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

    VIRTIO_SCSI_CONFIG_T            virtioScsiConfig;


} VIRTIOSCSI, *PVIRTIOSCSI;


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

/**
 * This macro resolves to boolean true if uOffset matches a field offset and size exactly,
 * (or if it is a 64-bit field, if it accesses either 32-bit part as a 32-bit access)
 * ASSUMED this critereon is mandated by section 4.1.3.1 of the VirtIO 1.0 specification)
 * This MACRO can be re-written to allow unaligned access to a field (within bounds).
 *
 * @param   member   - Member of VIRTIO_PCI_COMMON_CFG_T
 * @result           - true or false
 */
#define MATCH_SCSI_CONFIG(member) \
            (RT_SIZEOFMEMB(VIRTIO_SCSI_CONFIG_T, member) == 8 \
             && (   uOffset == RT_UOFFSETOF(VIRTIO_SCSI_CONFIG_T, member) \
                 || uOffset == RT_UOFFSETOF(VIRTIO_SCSI_CONFIG_T, member) + sizeof(uint32_t)) \
             && cb == sizeof(uint32_t)) \
         || (uOffset == RT_UOFFSETOF(VIRTIO_SCSI_CONFIG_T, member) \
               && cb == RT_SIZEOFMEMB(VIRTIO_SCSI_CONFIG_T, member))

#define LOG_ACCESSOR(member) \
        virtioLogMappedIoValue(__FUNCTION__, #member, RT_SIZEOFMEMB(VIRTIO_SCSI_CONFIG_T, member), \
            pv, cb, uMemberOffset, fWrite, false, 0);

#define SCSI_CONFIG_ACCESSOR(member) \
    { \
        uint32_t uMemberOffset = uOffset - RT_UOFFSETOF(VIRTIO_SCSI_CONFIG_T, member); \
        if (fWrite) \
            memcpy(((char *)&pVirtioScsi->virtioScsiConfig.member) + uMemberOffset, (const char *)pv, cb); \
        else \
            memcpy((char *)pv, (const char *)(((char *)&pVirtioScsi->virtioScsiConfig.member) + uMemberOffset), cb); \
        LOG_ACCESSOR(member); \
    }

#define SCSI_CONFIG_ACCESSOR_READONLY(member) \
    { \
        uint32_t uMemberOffset = uOffset - RT_UOFFSETOF(VIRTIO_SCSI_CONFIG_T, member); \
        if (fWrite) \
            LogFunc(("Guest attempted to write readonly virtio_pci_common_cfg.%s\n", #member)); \
        else \
        { \
            memcpy((char *)pv, (const char *)(((char *)&pVirtioScsi->virtioScsiConfig.member) + uMemberOffset), cb); \
            LOG_ACCESSOR(member); \
        } \
    }

typedef struct VIRTIOSCSIREQ *PVIRTIOSCSIREQ;

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
 * Turns on/off the write status LED.
 *
 * @param   pTarget         Pointer to the target device
 * @param   fOn             New LED state.
 */
void virtioScsiSetWriteLed(PVIRTIOSCSITARGET pTarget, bool fOn)
{
    LogFlow(("%s virtioSetWriteLed: %s\n", pTarget->pszLunName, fOn ? "on" : "off"));
    if (fOn)
        pTarget->led.Asserted.s.fWriting = pTarget->led.Actual.s.fWriting = 1;
    else
        pTarget->led.Actual.s.fWriting = fOn;
}

/**
 * Turns on/off the read status LED.
 *
 * @param   pTarget         Pointer to the device state structure.
 * @param   fOn             New LED state.
 */
void virtioScsiSetReadLed(PVIRTIOSCSITARGET pTarget, bool fOn)
{
    LogFlow(("%s virtioSetReadLed: %s\n", pTarget->pszLunName, fOn ? "on" : "off"));
    if (fOn)
        pTarget->led.Asserted.s.fReading = pTarget->led.Actual.s.fReading = 1;
    else
        pTarget->led.Actual.s.fReading = fOn;
}

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

/**
 * Device relocation callback.
 *
 * When this callback is called the device instance data, and if the
 * device have a GC component, is being relocated, or/and the selectors
 * have been changed. The device must use the chance to perform the
 * necessary pointer relocations and data updates.
 *
 * Before the GC code is executed the first time, this function will be
 * called with a 0 delta so GC pointer calculations can be one in one place.
 *
 * @param   pDevIns     Pointer to the device instance.
 * @param   offDelta    The relocation delta relative to the old location.
 *
 * @remark  A relocation CANNOT fail.
 */
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

    /**
     * Important: Forward to virtio framework!
     */
    virtioRelocate(pDevIns, offDelta);

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
        virtioScsiSetWriteLed(pTarget, false);
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
 * Gets the pointer to the status LED of a unit.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   iLUN            The unit which status LED we desire.
 * @param   ppLed           Where to store the LED pointer.
 */
static DECLCALLBACK(int) virtioScsiR3TargetQueryStatusLed(PPDMILEDPORTS pInterface, unsigned iLUN, PPDMLED *ppLed)
{
    PVIRTIOSCSITARGET pTarget = RT_FROM_MEMBER(pInterface, VIRTIOSCSITARGET, ILed);
    if (iLUN == 0)
    {
        *ppLed = &pTarget->led;
        Assert((*ppLed)->u32Magic == PDMLED_MAGIC);
        return VINF_SUCCESS;
    }
    return VERR_PDM_LUN_NOT_FOUND;
  }


/**
 * Gets the pointer to the status LED of a unit.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   iLUN            The unit which status LED we desire.
 * @param   ppLed           Where to store the LED pointer.
 */
static DECLCALLBACK(int) virtioScsiR3DeviceQueryStatusLed(PPDMILEDPORTS pInterface, unsigned iLUN, PPDMLED *ppLed)
{
    PVIRTIOSCSI pThis = RT_FROM_MEMBER(pInterface, VIRTIOSCSI, ILeds);
    if (iLUN < pThis->cTargets)
    {
        *ppLed = &pThis->aTargetInstances[iLUN].led;
        Assert((*ppLed)->u32Magic == PDMLED_MAGIC);
        return VINF_SUCCESS;
    }
    return VERR_PDM_LUN_NOT_FOUND;
}

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) virtioScsiR3TargetQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
     PVIRTIOSCSITARGET pTarget = RT_FROM_MEMBER(pInterface, VIRTIOSCSITARGET, IBase);
     PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE,        &pTarget->IBase);
     PDMIBASE_RETURN_INTERFACE(pszIID, PDMIMEDIAPORT,   &pTarget->IMediaPort);
     PDMIBASE_RETURN_INTERFACE(pszIID, PDMIMEDIAEXPORT, &pTarget->IMediaExPort);
     PDMIBASE_RETURN_INTERFACE(pszIID, PDMILEDPORTS,    &pTarget->ILed);
     return NULL;
}

static int virtioScsiR3CfgAccessed(PVIRTIOSCSI pVirtioScsi, uint32_t uOffset,
                                    const void *pv, size_t cb, uint8_t fWrite)
{
    int rc = VINF_SUCCESS;
    if (MATCH_SCSI_CONFIG(uNumQueues))
    {
        SCSI_CONFIG_ACCESSOR_READONLY(uNumQueues);
    }
    else
    if (MATCH_SCSI_CONFIG(uSegMax))
    {
        SCSI_CONFIG_ACCESSOR_READONLY(uSegMax);
    }
    else
    if (MATCH_SCSI_CONFIG(uMaxSectors))
    {
        SCSI_CONFIG_ACCESSOR_READONLY(uMaxSectors);
    }
    else
    if (MATCH_SCSI_CONFIG(uCmdPerLun))
    {
        SCSI_CONFIG_ACCESSOR_READONLY(uCmdPerLun);
    }
    else
    if (MATCH_SCSI_CONFIG(uEventInfoSize))
    {
        SCSI_CONFIG_ACCESSOR_READONLY(uEventInfoSize);
    }
    else
    if (MATCH_SCSI_CONFIG(uSenseSize))
    {
        SCSI_CONFIG_ACCESSOR(uSenseSize);
    }
    else
    if (MATCH_SCSI_CONFIG(uCdbSize))
    {
        SCSI_CONFIG_ACCESSOR(uCdbSize);
    }
    else
    if (MATCH_SCSI_CONFIG(uMaxChannel))
    {
        SCSI_CONFIG_ACCESSOR_READONLY(uMaxChannel);
    }
    else
    if (MATCH_SCSI_CONFIG(uMaxTarget))
    {
        SCSI_CONFIG_ACCESSOR_READONLY(uMaxTarget);
    }
    else
    if (MATCH_SCSI_CONFIG(uMaxLun))
    {
        SCSI_CONFIG_ACCESSOR_READONLY(uMaxLun);
    }
    else
    {
        LogFunc(("Bad access by guest to virtio_scsi_config: uoff=%d, cb=%d\n", uOffset, cb));
        rc = VERR_ACCESS_DENIED;
    }
    return rc;
}


/**
 * Get this callback from the virtio framework when the driver is ready so we know
 * the request for the number of queues is valid (for now presuming the driver(s) aren't
 * dynamically adding req queues but create them all while initializing
 * based on config information on host).
 */
static DECLCALLBACK(void) virtioScsiStatusChanged(VIRTIOHANDLE hVirtio, bool fVirtioReady)
{
#define MAX_QUEUENAME_SIZE 20
    Log2Func(("\n"));
    char pszQueueName[MAX_QUEUENAME_SIZE];
    if (fVirtioReady)
    {
        Log2Func(("VirtIO reports ready... Initializing queues\n"));
        virtioQueueInit(hVirtio, VIRTIOSCSI_VIRTQ_CONTROLQ, "controlq");
        virtioQueueInit(hVirtio, VIRTIOSCSI_VIRTQ_EVENTQ,   "eventq");
        for (uint16_t qIdx = VIRTIOSCSI_VIRTQ_REQ_BASE; qIdx < virtioGetNumQueues(hVirtio); qIdx++)
        {
            RTStrPrintf(pszQueueName, sizeof(pszQueueName), "requestq_%d", qIdx - 2);
            bool fEnabled = virtioQueueInit(hVirtio, qIdx, (const char *)pszQueueName);
            if (!fEnabled)
                break;
        }
    } else
        Log2Func(("VirtIO not ready\n"));
}

static DECLCALLBACK(void) virtioScsiQueueNotified(VIRTIOHANDLE hVirtio, uint16_t qIdx)
{
    Log2Func(("virtio callback: %s has avail data\n", virtioQueueGetName(hVirtio, qIdx)));
}

/**
 * virtio-scsi VirtIO Device-specific capabilities read callback
 * (other VirtIO capabilities and features are handled in VirtIO implementation)
 *
 * @param   pDevIns     The device instance.
 * @param   uOffset     Offset within device specific capabilities struct
 * @param   pv          Buffer in which to save read data
 * @param   cb          Number of bytes to read
 */
static DECLCALLBACK(int) virtioScsiR3DevCapRead(PPDMDEVINS pDevIns, uint32_t uOffset, const void *pv, size_t cb)
{
    int rc = VINF_SUCCESS;
    PVIRTIOSCSI  pVirtioScsi = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);

//    LogFunc(("Read from Device-Specific capabilities: uOffset: 0x%x, cb: 0x%x\n",
//              uOffset, cb));

    rc = virtioScsiR3CfgAccessed(pVirtioScsi, uOffset, pv, cb, false);

    return rc;
}

/**
 * virtio-scsi VirtIO Device-specific capabilities read callback
 * (other VirtIO capabilities and features are handled in VirtIO implementation)
 *
 * @param   pDevIns     The device instance.
 * @param   uOffset     Offset within device specific capabilities struct
 * @param   pv          Buffer in which to save read data
 * @param   cb          Number of bytes to write
 */
static DECLCALLBACK(int) virtioScsiR3DevCapWrite(PPDMDEVINS pDevIns, uint32_t uOffset, const void *pv, size_t cb)
{
    int rc = VINF_SUCCESS;
    PVIRTIOSCSI  pVirtioScsi = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);

//    LogFunc(("Write to Device-Specific capabilities: uOffset: 0x%x, cb: 0x%x\n",
//              uOffset, cb));

    rc = virtioScsiR3CfgAccessed(pVirtioScsi, uOffset, pv, cb, true);

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
            LogFunc(("virtio-scsi MMIO mapped at GCPhysAddr=%RGp cb=%RGp\n", GCPhysAddress, cb));

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
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) virtioScsiR3DeviceQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PVIRTIOSCSI pVirtioScsi = RT_FROM_MEMBER(pInterface, VIRTIOSCSI, IBase);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE,         &pVirtioScsi->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMILEDPORTS,     &pVirtioScsi->ILeds);

    return NULL;
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

    LogFunc((""));

    AssertMsg(fFlags & PDM_TACH_FLAGS_NOT_HOT_PLUG,
              ("virtio-scsi: Device does not support hotplugging\n"));

    /*
     * Zero some important members.
     */
    pTarget->fPresent    = false;
    pTarget->pDrvBase    = NULL;
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
    AssertRelease(!pTarget->pDrvBase);
    Assert(pTarget->iLUN == iLUN);

    /*
     * Try attach the SCSI driver and get the interfaces,
     * required as well as optional.
     */
    rc = PDMDevHlpDriverAttach(pDevIns, pTarget->iLUN, &pDevIns->IBase,
                               &pTarget->pDrvBase, (const char *)&pTarget->pszLunName);
    if (RT_SUCCESS(rc))
        pTarget->fPresent = true;
    else
        AssertMsgFailed(("Failed to attach %s. rc=%Rrc\n", pTarget->pszLunName, rc));

    if (RT_FAILURE(rc))
    {
        pTarget->fPresent = false;
        pTarget->pDrvBase = NULL;
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

static DECLCALLBACK(int) virtioScsiConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg){

    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);

    PVIRTIOSCSI  pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    int  rc = VINF_SUCCESS;
    bool fBootable = false;

    pThis->pDevInsR3 = pDevIns;
    pThis->pDevInsR0 = PDMDEVINS_2_R0PTR(pDevIns);
    pThis->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);

    LogFunc(("PDM device instance: %d\n", iInstance));
    RTStrPrintf((char *)pThis->szInstance, sizeof(pThis->szInstance), "VIRTIOSCSI%d", iInstance);

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
    pVirtioPciParams->uDeviceId      = PCI_DEVICE_ID_VIRTIOSCSI_HOST;
    pVirtioPciParams->uClassBase     = PCI_CLASS_BASE_MASS_STORAGE;
    pVirtioPciParams->uClassSub      = PCI_CLASS_SUB_SCSI_STORAGE_CONTROLLER;
    pVirtioPciParams->uClassProg     = PCI_CLASS_PROG_UNSPECIFIED;
    pVirtioPciParams->uSubsystemId   = PCI_DEVICE_ID_VIRTIOSCSI_HOST;  /* Virtio 1.0 spec allows PCI Device ID here */
    pVirtioPciParams->uInterruptLine = 0x00;
    pVirtioPciParams->uInterruptPin  = 0x01;

    pThis->IBase.pfnQueryInterface = virtioScsiR3DeviceQueryInterface;

    /* Configure virtio_scsi_config that transacts via VirtIO implementation's Dev. Specific Cap callbacks */
    pThis->virtioScsiConfig.uNumQueues      = VIRTIOSCSI_REQ_QUEUE_CNT;
    pThis->virtioScsiConfig.uSegMax         = 1024; /* initial guess */
    pThis->virtioScsiConfig.uMaxSectors     = 0x10000;
    pThis->virtioScsiConfig.uCmdPerLun      = 64;   /* initial guess */
    pThis->virtioScsiConfig.uEventInfoSize  = 100;  /* VirtIO 1.0 spec says this should based on # of negotiated features */
    pThis->virtioScsiConfig.uSenseSize      = 96;   /* from VirtIO 1.0 spec, must restore this on reset */
    pThis->virtioScsiConfig.uCdbSize        = 32;   /* from VirtIO 1.0 spec, must restore this on reset */
    pThis->virtioScsiConfig.uMaxChannel     = 0;    /* from VirtIO 1.0 spec */
    pThis->virtioScsiConfig.uMaxTarget      = pThis->cTargets;
    pThis->virtioScsiConfig.uMaxLun         = 16383; /* from VirtIO 1.0 spec */

    rc = virtioConstruct(pDevIns, &pThis->hVirtio, pVirtioPciParams, pThis->szInstance,
                         VIRTIOSCSI_HOST_SCSI_FEATURES_OFFERED,
                         virtioScsiR3DevCapRead,
                         virtioScsiR3DevCapWrite,
                         virtioScsiStatusChanged,
                         virtioScsiQueueNotified,
                         virtioScsiR3LiveExec,
                         virtioScsiR3SaveExec,
                         virtioScsiR3LoadExec,
                         virtioScsiR3LoadDone,
                         sizeof(VIRTIO_SCSI_CONFIG_T) /* cbDevSpecificCap */,
                         (void *)&pThis->virtioScsiConfig /* pDevSpecificCap */);

    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("virtio-scsi: failed to initialize VirtIO"));


    rc = PDMDevHlpPCIIORegionRegister(pDevIns, VIRTIOSCSI_REGION_MEM_IO, 32,
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

        if (RTStrAPrintf(&pTarget->pszLunName, "VSCSILUN%u", iLUN) < 0)
            AssertLogRelFailedReturn(VERR_NO_MEMORY);

        /* Initialize static parts of the device. */
        pTarget->iLUN = iLUN;
        pTarget->pVirtioScsiR3 = pThis;

        pTarget->IBase.pfnQueryInterface                 = virtioScsiR3TargetQueryInterface;

        /* IMediaPort and IMediaExPort interfaces provide callbacks for VD media and downstream driver access */
        pTarget->IMediaPort.pfnQueryDeviceLocation       = virtioScsiR3QueryDeviceLocation;
        pTarget->IMediaExPort.pfnIoReqCompleteNotify     = virtioScsiR3IoReqCompleteNotify;
        pTarget->IMediaExPort.pfnIoReqCopyFromBuf        = virtioScsiR3IoReqCopyFromBuf;
        pTarget->IMediaExPort.pfnIoReqCopyToBuf          = virtioScsiR3IoReqCopyToBuf;
        pTarget->IMediaExPort.pfnIoReqStateChanged       = virtioScsiR3IoReqStateChanged;
        pTarget->IMediaExPort.pfnMediumEjected           = virtioScsiR3MediumEjected;
        pTarget->IMediaExPort.pfnIoReqQueryBuf           = NULL;
        pTarget->IMediaExPort.pfnIoReqQueryDiscardRanges = NULL;
        pTarget->IBase.pfnQueryInterface                 = virtioScsiR3TargetQueryInterface;
        pTarget->ILed.pfnQueryStatusLed                  = virtioScsiR3TargetQueryStatusLed;
        pThis->ILeds.pfnQueryStatusLed                   = virtioScsiR3DeviceQueryStatusLed;
        pTarget->led.u32Magic                            = PDMLED_MAGIC;

        LogFunc(("Attaching LUN: %s\n", pTarget->pszLunName));

        /* Attach this SCSI driver (upstream driver pre-determined statically outside this module) */
        AssertReturn(iLUN < RT_ELEMENTS(pThis->aTargetInstances), VERR_PDM_NO_SUCH_LUN);
        rc = PDMDevHlpDriverAttach(pDevIns, iLUN, &pTarget->IBase, &pTarget->pDrvBase, (const char *)&pTarget->pszLunName);
        if (RT_SUCCESS(rc))
        {
            pTarget->fPresent = true;

            pTarget->pDrvMedia = PDMIBASE_QUERY_INTERFACE(pTarget->pDrvBase, PDMIMEDIA);
            AssertMsgReturn(VALID_PTR(pTarget->pDrvMedia),
                         ("virtio-scsi configuration error: LUN#%d missing basic media interface!\n", pTarget->iLUN),
                         VERR_PDM_MISSING_INTERFACE);

            /* Get the extended media interface. */
            pTarget->pDrvMediaEx = PDMIBASE_QUERY_INTERFACE(pTarget->pDrvBase, PDMIMEDIAEX);
            AssertMsgReturn(VALID_PTR(pTarget->pDrvMediaEx),
                         ("virtio-scsi configuration error: LUN#%d missing extended media interface!\n", pTarget->iLUN),
                         VERR_PDM_MISSING_INTERFACE);

            rc = pTarget->pDrvMediaEx->pfnIoReqAllocSizeSet(pTarget->pDrvMediaEx, REQ_ALLOC_SIZE /*TBD*/);
            AssertMsgReturn(VALID_PTR(pTarget->pDrvMediaEx),
                         ("virtio-scsi configuration error: LUN#%u: Failed to set I/O request size!\n", pTarget->iLUN),
                         rc);

        }
        else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
        {
            pTarget->fPresent = false;
            pTarget->pDrvBase = NULL;
            rc = VINF_SUCCESS;
            Log(("virtio-scsi: no driver attached to device %s\n", pTarget->pszLunName));
        }
        else
        {
            AssertLogRelMsgFailed(("virtio-scsi: Failed to attach %s\n", pTarget->pszLunName));
            return rc;
        }
    }

/*    rc = PDMDevHlpSSMRegisterEx(pDevIns, VIRTIOSCSI_SAVED_STATE_MINOR_VERSION, sizeof(*pThis), NULL,
                                NULL, virtioScsiR3LiveExec, NULL,
                                NULL, virtioScsiR3SaveExec, NULL,
                                NULL, virtioScsiR3LoadExec, virtioScsiR3LoadDone);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("virtio-scsi cannot register save state handlers"));
*/

    /* Status driver */
    PPDMIBASE pUpBase;
    rc = PDMDevHlpDriverAttach(pDevIns, PDM_STATUS_LUN, &pThis->IBase, &pUpBase, "Status Port");
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Failed to attach the status LUN"));

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
    "VBoxDDRC.rc",
    /* szR0Mod */
    "VBoxDDR0.r0",
    /* pszDescription */
    "Virtio Host SCSI.\n",
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

