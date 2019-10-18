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
//#define LOG_GROUP LOG_GROUP_DRV_SCSI
#define LOG_GROUP LOG_GROUP_DEV_VIRTIO

#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pdmstorageifs.h>
#include <VBox/vmm/pdmcritsect.h>
#include <VBox/msi.h>
#include <VBox/version.h>
#include <VBox/log.h>
#include <iprt/errcore.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include "../build/VBoxDD.h"
#include <VBox/scsi.h>
#ifdef IN_RING3
# include <iprt/alloc.h>
# include <iprt/memcache.h>
# include <iprt/semaphore.h>
# include <iprt/sg.h>
# include <iprt/param.h>
# include <iprt/uuid.h>
#endif
#include "../VirtIO/Virtio_1_0.h"

#include "VBoxSCSI.h"
#include "VBoxDD.h"

/*
 * RT log-levels used:
 *
 *    Level 1:   The most important (but usually rare) things to note
 *    Level 2:   SCSI command logging
 *    Level 3:   Vector and I/O transfer summary (shows what client sent an expects and fulfillment)
 *    Level 6:   Device ⟷ Guest Driver negotation, traffic, notifications and state handling
 *    Level 12:  Brief formatted hex dumps of I/O data
 */

#define LUN0    0
/** @name VirtIO 1.0 SCSI Host feature bits (See VirtIO 1.0 specification, Section 5.6.3)
 * @{  */
#define VIRTIO_SCSI_F_INOUT                RT_BIT_64(0)           /** Request is device readable AND writeable         */
#define VIRTIO_SCSI_F_HOTPLUG              RT_BIT_64(1)           /** Host allows hotplugging SCSI LUNs & targets      */
#define VIRTIO_SCSI_F_CHANGE               RT_BIT_64(2)           /** Host LUNs chgs via VIRTIOSCSI_T_PARAM_CHANGE evt */
#define VIRTIO_SCSI_F_T10_PI               RT_BIT_64(3)           /** Add T10 port info (DIF/DIX) in SCSI req hdr      */
/** @} */


#define VIRTIOSCSI_HOST_SCSI_FEATURES_ALL \
            (VIRTIO_SCSI_F_INOUT | VIRTIO_SCSI_F_HOTPLUG | VIRTIO_SCSI_F_CHANGE | VIRTIO_SCSI_F_T10_PI)

#define VIRTIOSCSI_HOST_SCSI_FEATURES_NONE          0

#define VIRTIOSCSI_HOST_SCSI_FEATURES_OFFERED \
             VIRTIOSCSI_HOST_SCSI_FEATURES_NONE

/*
 * TEMPORARY NOTE: following parameter is set to 1 for early development. Will be increased later
 */
//#define VIRTIOSCSI_REQ_QUEUE_CNT                    2            /**< Number of req queues exposed by dev.            */
//#define VIRTIOSCSI_QUEUE_CNT                        VIRTIOSCSI_REQ_QUEUE_CNT + 2
//#define VIRTIOSCSI_MAX_LUN                          256          /* < VirtIO specification, section 5.6.4             */
//#define VIRTIOSCSI_MAX_COMMANDS_PER_LUN             1            /* < T.B.D. What is a good value for this?           */
//#define VIRTIOSCSI_MAX_SEG_COUNT                    1024         /* < T.B.D. What is a good value for this?           */
//#define VIRTIOSCSI_MAX_SECTORS_HINT                 0x10000      /* < VirtIO specification, section 5.6.4             */
//#define VIRTIOSCSI_MAX_CHANNEL_HINT                 0            /* < VirtIO specification, section 5.6.4 should be 0 */
//#define VIRTIOSCSI_SAVED_STATE_MINOR_VERSION        0x01         /**< SSM version #                                   */


#define VIRTIOSCSI_REQ_QUEUE_CNT                    1            /**< Number of req queues exposed by dev.            */
#define VIRTIOSCSI_QUEUE_CNT                        VIRTIOSCSI_REQ_QUEUE_CNT + 2
#define VIRTIOSCSI_MAX_LUN                          256          /* < VirtIO specification, section 5.6.4             */
#define VIRTIOSCSI_MAX_COMMANDS_PER_LUN             128            /* < T.B.D. What is a good value for this?           */
#define VIRTIOSCSI_MAX_SEG_COUNT                    126         /* < T.B.D. What is a good value for this?           */
#define VIRTIOSCSI_MAX_SECTORS_HINT                 0x10000      /* < VirtIO specification, section 5.6.4             */
#define VIRTIOSCSI_MAX_CHANNEL_HINT                 0            /* < VirtIO specification, section 5.6.4 should be 0 */
#define VIRTIOSCSI_SAVED_STATE_MINOR_VERSION        0x01         /**< SSM version #                                   */

#define PCI_DEVICE_ID_VIRTIOSCSI_HOST               0x1048       /**< Informs guest driver of type of VirtIO device   */
#define PCI_CLASS_BASE_MASS_STORAGE                 0x01         /**< PCI Mass Storage device class                   */
#define PCI_CLASS_SUB_SCSI_STORAGE_CONTROLLER       0x00         /**< PCI SCSI Controller subclass                    */
#define PCI_CLASS_PROG_UNSPECIFIED                  0x00         /**< Programming interface. N/A.                     */
#define VIRTIOSCSI_PCI_CLASS                        0x01         /**< Base class Mass Storage?                        */

#define VIRTIOSCSI_SENSE_SIZE_DEFAULT               96          /**< VirtIO 1.0: 96 on reset, guest can change       */
#define VIRTIOSCSI_CDB_SIZE_DEFAULT                 32          /**< VirtIO 1.0: 32 on reset, guest can change       */
#define VIRTIOSCSI_PI_BYTES_IN                      1           /**< Value TBD (see section 5.6.6.1)                 */
#define VIRTIOSCSI_PI_BYTES_OUT                     1           /**< Value TBD (see section 5.6.6.1)                 */
#define VIRTIOSCSI_DATA_OUT                         512         /**< Value TBD (see section 5.6.6.1)                 */

/**
 * VirtIO SCSI Host Device device-specific queue indicies
 *
 * Virtqs (and their indices) are specified for a SCSI Host Device as described in the VirtIO 1.0 specification
 * section 5.6. Thus there is no need to explicitly indicate the number of queues needed by this device. The number
 * of req queues is variable and determined by virtio_scsi_config.num_queues. See VirtIO 1.0 spec section 5.6.4
 */
#define CONTROLQ_IDX                                0            /**< Spec-defined Index of control queue             */
#define EVENTQ_IDX                                  1            /**< Spec-defined Index of event queue               */
#define VIRTQ_REQ_BASE                              2            /**< Spec-defined base index of request queues       */

#define QUEUENAME(qIdx) (pThis->szQueueNames[qIdx])              /**< Macro to get queue name from its index          */
#define CBQUEUENAME(qIdx) RTStrNLen(QUEUENAME(qIdx), sizeof(QUEUENAME(qIdx)))


#define IS_REQ_QUEUE(qIdx) (qIdx >= VIRTQ_REQ_BASE && qIdx < VIRTIOSCSI_QUEUE_CNT)

/**
 * Resolves to boolean true if uOffset matches a field offset and size exactly,
 * (or if 64-bit field, if it accesses either 32-bit part as a 32-bit access)
 * It is *assumed* this critereon is mandated by section 4.1.3.1 of the VirtIO 1.0 spec)
 * This macro can be re-written to allow unaligned access to a field (within bounds).
 *
 * @param   member   - Member of VIRTIO_PCI_COMMON_CFG_T
 * @result           - true or false
 */
#define MATCH_SCSI_CONFIG(member) \
            (RT_SIZEOFMEMB(VIRTIOSCSI_CONFIG_T, member) == 8 \
             && (   uOffset == RT_UOFFSETOF(VIRTIOSCSI_CONFIG_T, member) \
                 || uOffset == RT_UOFFSETOF(VIRTIOSCSI_CONFIG_T, member) + sizeof(uint32_t)) \
             && cb == sizeof(uint32_t)) \
         || (uOffset == RT_UOFFSETOF(VIRTIOSCSI_CONFIG_T, member) \
               && cb == RT_SIZEOFMEMB(VIRTIOSCSI_CONFIG_T, member))

#define LOG_ACCESSOR(member) \
        virtioLogMappedIoValue(__FUNCTION__, #member, RT_SIZEOFMEMB(VIRTIOSCSI_CONFIG_T, member), \
            pv, cb, uIntraOffset, fWrite, false, 0);

#define SCSI_CONFIG_ACCESSOR(member) \
    do \
    { \
        uint32_t uIntraOffset = uOffset - RT_UOFFSETOF(VIRTIOSCSI_CONFIG_T, member); \
        if (fWrite) \
            memcpy(((char *)&pThis->virtioScsiConfig.member) + uIntraOffset, (const char *)pv, cb); \
        else \
            memcpy((char *)pv, (const char *)(((char *)&pThis->virtioScsiConfig.member) + uIntraOffset), cb); \
        LOG_ACCESSOR(member); \
    } while(0)

#define SCSI_CONFIG_ACCESSOR_READONLY(member) \
    do \
    { \
        uint32_t uIntraOffset = uOffset - RT_UOFFSETOF(VIRTIOSCSI_CONFIG_T, member); \
        if (fWrite) \
            LogFunc(("Guest attempted to write readonly virtio_pci_common_cfg.%s\n", #member)); \
        else \
        { \
            memcpy((char *)pv, (const char *)(((char *)&pThis->virtioScsiConfig.member) + uIntraOffset), cb); \
            LOG_ACCESSOR(member); \
        } \
    } while(0)

#define VIRTIO_IN_DIRECTION(pMediaExTxDirEnumValue) \
            pMediaExTxDirEnumValue == PDMMEDIAEXIOREQSCSITXDIR_FROM_DEVICE

#define VIRTIO_OUT_DIRECTION(pMediaExTxDirEnumValue) \
            pMediaExTxDirEnumValue == PDMMEDIAEXIOREQSCSITXDIR_TO_DEVICE
/**
 * The following struct is the VirtIO SCSI Host Device device-specific configuration described
 * in section 5.6.4 of the VirtIO 1.0 spec. The VBox VirtIO framework calls back to this driver
 * to handle MMIO accesses to the device-specific configuration parameters whenever any bytes in the
 * device-specific region areaccessed, since which the generic portion shouldn't know anything about
 * the device-specific VirtIO cfg data.
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
} VIRTIOSCSI_CONFIG_T, PVIRTIOSCSI_CONFIG_T;

/** @name VirtIO 1.0 SCSI Host Device device specific control types
 * @{  */
#define VIRTIOSCSI_T_NO_EVENT                       0
#define VIRTIOSCSI_T_TRANSPORT_RESET                1
#define VIRTIOSCSI_T_ASYNC_NOTIFY                   2           /**< Asynchronous notification                       */
#define VIRTIOSCSI_T_PARAM_CHANGE                   3
/** @} */

/**
 * Device operation: eventq
 */
#define VIRTIOSCSI_T_EVENTS_MISSED             0x80000000
typedef struct virtio_scsi_event {
    // Device-writable part
    uint32_t uEvent;                                            /**< event:                                          */
    uint8_t  uVirtioLun[8];                                     /**< lun                                             */
    uint32_t uReason;                                           /**< reason                                          */
} VIRTIOSCSI_EVENT_T, *PVIRTIOSCSI_EVENT_T;

/** @name VirtIO 1.0 SCSI Host Device device specific event types
 * @{  */
#define VIRTIOSCSI_EVT_RESET_HARD                   0           /**<                                                 */
#define VIRTIOSCSI_EVT_RESET_RESCAN                 1           /**<                                                 */
#define VIRTIOSCSI_EVT_RESET_REMOVED                2           /**<                                                 */
/** @} */


#pragma pack(1)

/**
 * Device operation: reqestq
 */
struct REQ_CMD_HDR
{
    uint8_t  uVirtioLun[8];                                     /**< lun                                          */
    uint64_t uId;                                               /**< id                                           */
    uint8_t  uTaskAttr;                                         /**< task_attr                                    */
    uint8_t  uPrio;                                             /**< prio                                         */
    uint8_t  uCrn;                                              /**< crn                                          */
};

struct REQ_CMD_PI
{
    uint32_t uPiBytesOut;                                       /**< pi_bytesout                                  */
    uint32_t uPiBytesIn;                                        /**< pi_bytesin                                   */
};

struct REQ_RESP_HDR
{
    uint32_t uSenseLen;                                         /**< sense_len                                    */
    uint32_t uResidual;                                         /**< residual                                     */
    uint16_t uStatusQualifier;                                  /**< status_qualifier                             */
    uint8_t  uStatus;                                           /**< status            SCSI status code           */
    uint8_t  uResponse;                                         /**< response                                     */
};

typedef struct virtio_scsi_req_cmd
{
    /* Device-readable section */

    struct REQ_CMD_HDR  ReqHdr;
    uint8_t  uCdb[1];                                           /**< cdb                                          */

    struct REQ_CMD_PI piHdr;                                    /** T10 Pi block integrity (optional feature)     */
    uint8_t  uPiOut[1];                                         /**< pi_out[]          T10 pi block integrity     */
    uint8_t  uDataOut[1];                                       /**< dataout                                      */

    /** Device writable section */

    struct REQ_RESP_HDR  respHdr;
    uint8_t  uSense[1];                                         /**< sense                                        */
    uint8_t  uPiIn[1];                                          /**< pi_in[]           T10 Pi block integrity     */
    uint8_t  uDataIn[1];                                        /**< detain;                                      */

}  VIRTIOSCSI_REQ_CMD_T, *PVIRTIOSCSI_REQ_CMD_T;
#pragma pack()

/** @name VirtIO 1.0 SCSI Host Device Req command-specific response values
 * @{  */
#define VIRTIOSCSI_S_OK                             0          /**< control, command                                 */
#define VIRTIOSCSI_S_OVERRUN                        1          /**< control                                          */
#define VIRTIOSCSI_S_ABORTED                        2          /**< control                                          */
#define VIRTIOSCSI_S_BAD_TARGET                     3          /**< control, command                                 */
#define VIRTIOSCSI_S_RESET                          4          /**< control                                          */
#define VIRTIOSCSI_S_BUSY                           5          /**< control, command                                 */
#define VIRTIOSCSI_S_TRANSPORT_FAILURE              6          /**< control, command                                 */
#define VIRTIOSCSI_S_TARGET_FAILURE                 7          /**< control, command                                 */
#define VIRTIOSCSI_S_NEXUS_FAILURE                  8          /**< control, command                                 */
#define VIRTIOSCSI_S_FAILURE                        9          /**< control, command                                 */
#define VIRTIOSCSI_S_INCORRECT_LUN                  12         /**< command                                          */
/** @} */

/** @name VirtIO 1.0 SCSI Host Device command-specific task_attr values
 * @{  */
#define VIRTIOSCSI_S_SIMPLE                        0           /**<                                                  */
#define VIRTIOSCSI_S_ORDERED                       1           /**<                                                  */
#define VIRTIOSCSI_S_HEAD                          2           /**<                                                  */
#define VIRTIOSCSI_S_ACA                           3           /**<                                                  */
/** @} */

/**
 * VirtIO 1.0 SCSI Host Device Control command before we know type (5.6.6.2)
 */
typedef struct virtio_scsi_ctrl
{
    uint32_t uType;
} VIRTIOSCSI_CTRL_T, *PVIRTIOSCSI_CTRL_T;

/** @name VirtIO 1.0 SCSI Host Device command-specific TMF values
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
/** @} */

#pragma pack(1)
typedef struct virtio_scsi_ctrl_tmf
{
     // Device-readable part
    uint32_t uType;                                            /** type                                              */
    uint32_t uSubtype;                                         /** subtype                                           */
    uint8_t  uScsiLun[8];                                      /** lun                                               */
    uint64_t uId;                                              /** id                                                */
    // Device-writable part
    uint8_t  uResponse;                                        /** response                                          */
} VIRTIOSCSI_CTRL_TMF_T, *PVIRTIOSCSI_CTRL_TMF_T;
#pragma pack()

/** @name VirtIO 1.0 SCSI Host Device device specific tmf control response values
 * @{  */
#define VIRTIOSCSI_S_FUNCTION_COMPLETE            0           /**<                                                   */
#define VIRTIOSCSI_S_FUNCTION_SUCCEEDED           10          /**<                                                   */
#define VIRTIOSCSI_S_FUNCTION_REJECTED            11          /**<                                                   */
/** @} */

#define VIRTIOSCSI_T_AN_QUERY                     1           /** Asynchronous notification query                    */
#define VIRTIOSCSI_T_AN_SUBSCRIBE                 2           /** Asynchronous notification subscription             */

#pragma pack(1)
typedef struct virtio_scsi_ctrl_an
{
    // Device-readable part
    uint32_t  uType;                                          /** type                                               */
    uint8_t   uScsiLun[8];                                    /** lun                                                */
    uint32_t  uEventsRequested;                               /** event_requested                                    */
    // Device-writable part
    uint32_t  uEventActual;                                   /** event_actual                                       */
    uint8_t   uResponse;                                      /** response                                           */
}  VIRTIOSCSI_CTRL_AN_T, *PVIRTIOSCSI_CTRL_AN_T;
#pragma pack()

typedef union virtio_scsi_ctrl_union
{
    VIRTIOSCSI_CTRL_T      scsiCtrl;
    VIRTIOSCSI_CTRL_TMF_T  scsiCtrlTmf;
    VIRTIOSCSI_CTRL_AN_T   scsiCtrlAsyncNotify;
} VIRTIO_SCSI_CTRL_UNION_T, *PVIRTIO_SCSI_CTRL_UNION_T;

/** @name VirtIO 1.0 SCSI Host Device device specific tmf control response values
 * @{  */
#define VIRTIOSCSI_EVT_ASYNC_OPERATIONAL_CHANGE  2           /**<                                                   */
#define VIRTIOSCSI_EVT_ASYNC_POWER_MGMT          4           /**<                                                   */
#define VIRTIOSCSI_EVT_ASYNC_EXTERNAL_REQUEST    8           /**<                                                   */
#define VIRTIOSCSI_EVT_ASYNC_MEDIA_CHANGE        16          /**<                                                   */
#define VIRTIOSCSI_EVT_ASYNC_MULTI_HOST          32          /**<                                                   */
#define VIRTIOSCSI_EVT_ASYNC_DEVICE_BUSY         64          /**<                                                   */
/** @} */

#define SUBSCRIBABLE_EVENTS \
              VIRTIOSCSI_EVT_ASYNC_OPERATIONAL_CHANGE \
            & VIRTIOSCSI_EVT_ASYNC_POWER_MGMT \
            & VIRTIOSCSI_EVT_ASYNC_EXTERNAL_REQUEST \
            & VIRTIOSCSI_EVT_ASYNC_MEDIA_CHANGE \
            & VIRTIOSCSI_EVT_ASYNC_MULTI_HOST \
            & VIRTIOSCSI_EVT_ASYNC_DEVICE_BUSY

/**
 * Worker thread context
 */
typedef struct WORKER
{
    R3PTRTYPE(PPDMTHREAD)           pThread;                  /**< pointer to worker thread's handle                 */
    SUPSEMEVENT                     hEvtProcess;              /**< handle of associated sleep/wake-up semaphore      */
    bool                            fSleeping;                /**< Flags whether worker thread is sleeping or not    */
    bool                            fNotified;                /**< Flags whether worker thread notified              */
} WORKER, *PWORKER;

/**
 * State of a target attached to the VirtIO SCSI Host
 */
typedef struct VIRTIOSCSITARGET
{
    /** Pointer to PCI device that owns this target instance. - R3 pointer */
    R3PTRTYPE(struct VIRTIOSCSI *)  pVirtioScsi;

    /** Pointer to attached driver's base interface. */
    R3PTRTYPE(PPDMIBASE)            pDrvBase;

    /** Target number (PDM LUN) */
    RTUINT                          iTarget;

    /** Target Description */
    char *                          pszTargetName;

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

    PPDMIMEDIANOTIFY                pMediaNotify;

     /** Pointer to the attached driver's extended media interface. */
    R3PTRTYPE(PPDMIMEDIAEX)         pDrvMediaEx;

    /** Status LED interface */
    PDMILEDPORTS                    ILed;

    /** The status LED state for this device. */
    PDMLED                          led;

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

    /** Number of targets detected */
    uint64_t                        cTargets;

    R3PTRTYPE(PVIRTIOSCSITARGET)    paTargetInstances;
#if HC_ARCH_BITS == 32
      RTR3PTR                 R3PtrPadding0;
#endif

    /** Per device-bound virtq worker-thread contexts (eventq slot unused) */
    WORKER                          aWorker[VIRTIOSCSI_QUEUE_CNT];

    bool                            fBootable;
    bool                            fRCEnabled;
    bool                            fR0Enabled;
    /** Instance name */
    const char                      szInstance[16];

    /** Device-specific spec-based VirtIO queuenames */
    const char                      szQueueNames[VIRTIOSCSI_QUEUE_CNT][VIRTIO_MAX_QUEUE_NAME_SIZE];

    /** Track which VirtIO queues we've attached to */
    bool                            fQueueAttached[VIRTIOSCSI_QUEUE_CNT];

    /** Device base interface. */
    PDMIBASE                        IBase;

    /** Pointer to the device instance. - R3 ptr. */
    PPDMDEVINSR3                    pDevInsR3;

    /** Pointer to the device instance. - R0 ptr. */
    PPDMDEVINSR0                    pDevInsR0;

    /** Pointer to the device instance. - RC ptr. */
    PPDMDEVINSRC                    pDevInsRC;

    /** Status Target: LEDs port interface. */
    PDMILEDPORTS                    ILeds;

    /** Status Target: Partner of ILeds. */
    R3PTRTYPE(PPDMILEDCONNECTORS)   pLedsConnector;

    /** IMediaExPort: Media ejection notification */
    R3PTRTYPE(PPDMIMEDIANOTIFY)     pMediaNotify;

    /** Queue to send tasks to R3. - HC ptr */
    R3PTRTYPE(PPDMQUEUE)            pNotifierQueueR3;

    /** The support driver session handle. */
    R3R0PTRTYPE(PSUPDRVSESSION)     pSupDrvSession;

    /** Mask of VirtIO Async Event types this device will deliver */
    uint32_t                        uAsyncEvtsEnabled;

    /** Total number of requests active across all targets */
    volatile uint32_t               cActiveReqs;

    /** True if PDMDevHlpAsyncNotificationCompleted should be called when port goes idle */
    bool volatile                   fSignalIdle;

    /** Events the guest has subscribed to get notifications of */
    uint32_t                        uSubscribedEvents;

    /** Set if events missed due to lack of bufs avail on eventq */
    bool                            fEventsMissed;

    /** VirtIO Host SCSI device runtime configuration parameters */
    VIRTIOSCSI_CONFIG_T             virtioScsiConfig;

    /** True if the guest/driver and VirtIO framework are in the ready state */
    unsigned                        fVirtioReady;

    /** True if VIRTIO_SCSI_F_T10_PI was negotiated */
    unsigned                        fHasT10pi;

    /** True if VIRTIO_SCSI_F_T10_PI was negotiated */
    unsigned                        fHasHotplug;

    /** True if VIRTIO_SCSI_F_T10_PI was negotiated */
    unsigned                        fHasInOutBufs;

    /** True if VIRTIO_SCSI_F_T10_PI was negotiated */
    unsigned                        fHasLunChange;

    /** True if in the process of resetting */
    unsigned                        fResetting;

    /** True if in the process of quiescing I/O */
    unsigned                        fQuiescing;

} VIRTIOSCSI, *PVIRTIOSCSI;

/**
 * Request structure for IMediaEx (Associated Interfaces implemented by DrvSCSI)
 * (NOTE: cbIn, cbOUt, cbDataOut mostly for debugging)
 */
typedef struct VIRTIOSCSIREQ
{
    PDMMEDIAEXIOREQ                hIoReq;                   /**< Handle of I/O request                             */
    PVIRTIOSCSITARGET              pTarget;                  /**< Target                                            */
    uint16_t                       qIdx;                     /**< Index of queue this request arrived on            */
    PVIRTIO_DESC_CHAIN_T           pDescChain;               /**< Prepared desc chain pulled from virtq avail ring  */
    uint32_t                       cbDataIn;                 /**< size of dataout buffer                            */
    uint32_t                       cbDataOut;                /**< size of dataout buffer                            */
    uint16_t                       uDataInOff;               /**< Fixed size of respHdr + sense (precede datain)    */
    uint16_t                       uDataOutOff;              /**< Fixed size of respHdr + sense (precede datain)    */
    uint32_t                       cbSense;                  /**< Size of sense buffer                              */
    size_t                         uSenseLen;                /**< Receives # bytes written into sense buffer        */
    uint8_t                       *pbSense;                  /**< Pointer to R3 sense buffer                        */
    PDMMEDIAEXIOREQSCSITXDIR       enmTxDir;                 /**< Receives transfer direction of I/O req            */
    uint8_t                        uStatus;                  /**< SCSI status code                                  */
} VIRTIOSCSIREQ;

DECLINLINE(const char *) virtioGetTxDirText(uint32_t enmTxDir)
{
    switch (enmTxDir)
    {
        case PDMMEDIAEXIOREQSCSITXDIR_UNKNOWN:          return "<UNKNOWN>";
        case PDMMEDIAEXIOREQSCSITXDIR_FROM_DEVICE:      return "<DEV-TO-GUEST>";
        case PDMMEDIAEXIOREQSCSITXDIR_TO_DEVICE:        return "<GUEST-TO-DEV>";
        case PDMMEDIAEXIOREQSCSITXDIR_NONE:             return "<NONE>";
        default:                                        return "<BAD ENUM>";
    }
}

DECLINLINE(const char *) virtioGetTMFTypeText(uint32_t uSubType)
{
    switch (uSubType)
    {
        case VIRTIOSCSI_T_TMF_ABORT_TASK:               return "ABORT TASK";
        case VIRTIOSCSI_T_TMF_ABORT_TASK_SET:           return "ABORT TASK SET";
        case VIRTIOSCSI_T_TMF_CLEAR_ACA:                return "CLEAR ACA";
        case VIRTIOSCSI_T_TMF_CLEAR_TASK_SET:           return "CLEAR TASK SET";
        case VIRTIOSCSI_T_TMF_I_T_NEXUS_RESET:          return "I T NEXUS RESET";
        case VIRTIOSCSI_T_TMF_LOGICAL_UNIT_RESET:       return "LOGICAL UNIT RESET";
        case VIRTIOSCSI_T_TMF_QUERY_TASK:               return "QUERY TASK";
        case VIRTIOSCSI_T_TMF_QUERY_TASK_SET:           return "QUERY TASK SET";
        default:                                        return "<unknown>";
    }
}

DECLINLINE(const char *) virtioGetReqRespText(uint32_t vboxRc)
{
    switch (vboxRc)
    {
        case VIRTIOSCSI_S_OK:                           return "OK";
        case VIRTIOSCSI_S_OVERRUN:                      return "OVERRRUN";
        case VIRTIOSCSI_S_ABORTED:                      return "ABORTED";
        case VIRTIOSCSI_S_BAD_TARGET:                   return "BAD TARGET";
        case VIRTIOSCSI_S_RESET:                        return "RESET";
        case VIRTIOSCSI_S_TRANSPORT_FAILURE:            return "TRANSPORT FAILURE";
        case VIRTIOSCSI_S_TARGET_FAILURE:               return "TARGET FAILURE";
        case VIRTIOSCSI_S_NEXUS_FAILURE:                return "NEXUS FAILURE";
        case VIRTIOSCSI_S_BUSY:                         return "BUSY";
        case VIRTIOSCSI_S_FAILURE:                      return "FAILURE";
        default:                                        return "<unknown>";
    }
}

DECLINLINE(const char *) virtioGetCtrlRespText(uint32_t vboxRc)
{
    switch (vboxRc)
    {
        case VIRTIOSCSI_S_OK:                           return "OK/COMPLETE";
        case VIRTIOSCSI_S_BAD_TARGET:                   return "BAD TARGET";
        case VIRTIOSCSI_S_BUSY:                         return "BUSY";
        case VIRTIOSCSI_S_NEXUS_FAILURE:                return "NEXUS FAILURE";
        case VIRTIOSCSI_S_TRANSPORT_FAILURE:            return "TRANSPORT FAILURE";
        case VIRTIOSCSI_S_TARGET_FAILURE:               return "TARGET FAILURE";
        case VIRTIOSCSI_S_FAILURE:                      return "FAILURE";
        case VIRTIOSCSI_S_INCORRECT_LUN:                return "INCORRECT LUN";
        case VIRTIOSCSI_S_FUNCTION_SUCCEEDED:           return "FUNCTION SUCCEEDED";
        case VIRTIOSCSI_S_FUNCTION_REJECTED:            return "FUNCTION REJECTED";
        default:                                        return "<unknown>";
    }
}

DECLINLINE(void) virtioGetControlAsyncMaskText(char *pszOutput, uint32_t cbOutput, uint32_t uAsyncTypesMask)
{
    RTStrPrintf(pszOutput, cbOutput, "%s%s%s%s%s%s",
        (uAsyncTypesMask & VIRTIOSCSI_EVT_ASYNC_OPERATIONAL_CHANGE) ? "CHANGE_OPERATION  "   : "",
        (uAsyncTypesMask & VIRTIOSCSI_EVT_ASYNC_POWER_MGMT)         ? "POWER_MGMT  "         : "",
        (uAsyncTypesMask & VIRTIOSCSI_EVT_ASYNC_EXTERNAL_REQUEST)   ? "EXTERNAL_REQ  "       : "",
        (uAsyncTypesMask & VIRTIOSCSI_EVT_ASYNC_MEDIA_CHANGE)       ? "MEDIA_CHANGE  "       : "",
        (uAsyncTypesMask & VIRTIOSCSI_EVT_ASYNC_MULTI_HOST)         ? "MULTI_HOST  "         : "",
        (uAsyncTypesMask & VIRTIOSCSI_EVT_ASYNC_DEVICE_BUSY)        ? "DEVICE_BUSY  "        : "");
}

uint8_t virtioScsiEstimateCdbLen(uint8_t uCmd, uint8_t cbMax)
{
    if (uCmd < 0x1f)
        return 6;
    else if (uCmd >= 0x20 && uCmd < 0x60)
        return 10;
    else if (uCmd >= 0x60 && uCmd < 0x80)
        return cbMax;
    else if (uCmd >= 0x80 && uCmd < 0xa0)
        return 16;
    else if (uCmd >= 0xa0 && uCmd < 0xC0)
        return 12;
    else
        return cbMax;
}

typedef struct VIRTIOSCSIREQ *PVIRTIOSCSIREQ;

#ifdef BOOTABLE_SUPPORT_TBD
/** @callback_method_impl{FNIOMIOPORTIN} */
static DECLCALLBACK(int) virtioScsiBiosIoPortRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT uPort, uint8_t *pbDst,
                                                    uint32_t *pcTransfers, unsigned cb);
{
}
/** @callback_method_impl{FNIOMIOPORTOUT} */
static DECLCALLBACK(int) virtioScsiBiosIoPortWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT uPort, uint32_t u32, unsigned cb);
{
}
/** @callback_method_impl{FNIOMIOPORTOUTSTRING} */
static DECLCALLBACK(int) virtioScsiBiosIoPortWriteStr(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT uPort, const uint8_t *pbSrc,
                                                        uint32_t *pcTransfers, unsigned cb);
{
}
/** @callback_method_impl{FNIOMIOPORTINSTRING} */
static DECLCALLBACK(int) virtioScsiBiosIoPortReadStr(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT uPort, uint8_t *pbDst,
                                                       uint32_t *pcTransfers, unsigned cb);
{
}
#endif

static int virtioScsiSendEvent(PVIRTIOSCSI pThis, uint16_t uTarget, uint32_t uEventType, uint32_t uReason)
{

    VIRTIOSCSI_EVENT_T event;
    event.uEvent = uEventType;
    event.uReason = uReason;
    event.uVirtioLun[0] = 1;
    event.uVirtioLun[1] = uTarget;
    event.uVirtioLun[2] = (LUN0 >> 8) & 0x40;
    event.uVirtioLun[3] = LUN0 & 0xff;
    event.uVirtioLun[4] = event.uVirtioLun[5] = event.uVirtioLun[6] = event.uVirtioLun[7] = 0;

    switch(uEventType)
    {
        case VIRTIOSCSI_T_NO_EVENT:
            if (uEventType & VIRTIOSCSI_T_EVENTS_MISSED)
                Log6Func(("(target=%d, LUN=%d) Warning driver that events were missed\n", uTarget, LUN0));
            else
                Log6Func(("(target=%d, LUN=%d): Warning event info guest queued is shorter than configured\n",
                          uTarget, LUN0));
            break;
        case VIRTIOSCSI_T_TRANSPORT_RESET:
            switch(uReason)
            {
                case VIRTIOSCSI_EVT_RESET_REMOVED:
                    Log6Func(("(target=%d, LUN=%d): Target or LUN removed\n", uTarget, LUN0));
                    break;
                case VIRTIOSCSI_EVT_RESET_RESCAN:
                    Log6Func(("(target=%d, LUN=%d): Target or LUN added\n", uTarget, LUN0));
                    break;
                case VIRTIOSCSI_EVT_RESET_HARD:
                    Log6Func(("(target=%d, LUN=%d): Target was reset\n", uTarget, LUN0));
                    break;
            }
            break;
        case VIRTIOSCSI_T_ASYNC_NOTIFY:
            char szTypeText[128];
            virtioGetControlAsyncMaskText(szTypeText, sizeof(szTypeText), uReason);
            Log6Func(("(target=%d, LUN=%d): Delivering subscribed async notification %s\n",
                         uTarget, LUN0, szTypeText));
            break;
        case VIRTIOSCSI_T_PARAM_CHANGE:
            LogFunc(("(target=%d, LUN=%d): PARAM_CHANGE sense code: 0x%x sense qualifier: 0x%x\n",
                        uTarget, LUN0, uReason & 0xff, (uReason >> 8) & 0xff));
            break;
        default:
            Log6Func(("(target=%d, LUN=%d): Unknown event type: %d, ignoring\n",
                        uTarget, LUN0, uEventType));
            return VINF_SUCCESS;
    }

    if (virtioQueueIsEmpty(pThis->hVirtio, EVENTQ_IDX))
    {
        LogFunc(("eventq is empty, events missed (driver didn't preload queue)!\n"));
        ASMAtomicWriteBool(&pThis->fEventsMissed, true);
        return VINF_SUCCESS;
    }

    PVIRTIO_DESC_CHAIN_T pDescChain;
    virtioQueueGet(pThis->hVirtio, EVENTQ_IDX, &pDescChain, true);

    RTSGBUF reqSegBuf;
    RTSGSEG aReqSegs[] = { { &event, sizeof(event) } };
    RTSgBufInit(&reqSegBuf, aReqSegs, sizeof(aReqSegs) / sizeof(RTSGSEG));

    virtioQueuePut (pThis->hVirtio, EVENTQ_IDX, &reqSegBuf, pDescChain, true);
    virtioQueueSync(pThis->hVirtio, EVENTQ_IDX);

    return VINF_SUCCESS;
}

static void virtioScsiFreeReq(PVIRTIOSCSITARGET pTarget, PVIRTIOSCSIREQ pReq)
{
    RTMemFree(pReq->pbSense);
    pTarget->pDrvMediaEx->pfnIoReqFree(pTarget->pDrvMediaEx, pReq->hIoReq);
}

/**
 * This is called to complete a request immediately
 *
 * @param pThis      - PDM driver instance state
 * @param qIdx       - Queue index
 * @param pDescChain - Pointer to pre-processed descriptor chain pulled from virtq
 * @param pRespHdr   - Response header
 * @param pbSense    - Pointer to sense buffer or NULL if none.
 *
 * @returns virtual box status code
 */
static int virtioScsiReqErr(PVIRTIOSCSI pThis, uint16_t qIdx, PVIRTIO_DESC_CHAIN_T pDescChain,
                            struct REQ_RESP_HDR *pRespHdr, uint8_t *pbSense)
{
    uint8_t *abSenseBuf = (uint8_t *)RTMemAllocZ(pThis->virtioScsiConfig.uSenseSize);
    AssertReturn(abSenseBuf, VERR_NO_MEMORY);

    const char *pszCtrlRespText = virtioGetCtrlRespText(pRespHdr->uResponse);
    Log2Func(("   status: %s    response: %s\n",
              SCSIStatusText(pRespHdr->uStatus),  pszCtrlRespText));
    RT_NOREF(pszCtrlRespText);

    RTSGSEG aReqSegs[2];
    aReqSegs[0].cbSeg = sizeof(pRespHdr);
    aReqSegs[0].pvSeg = pRespHdr;
    aReqSegs[1].cbSeg = pThis->virtioScsiConfig.uSenseSize;
    aReqSegs[1].pvSeg = abSenseBuf;

    if (pbSense && pRespHdr->uSenseLen)
        memcpy(abSenseBuf, pbSense, pRespHdr->uSenseLen);
    else
        pRespHdr->uSenseLen = 0;

    RTSGBUF reqSegBuf;
    RTSgBufInit(&reqSegBuf, aReqSegs, RT_ELEMENTS(aReqSegs));

    if (pThis->fResetting)
        pRespHdr->uResponse = VIRTIOSCSI_S_RESET;

    virtioQueuePut(pThis->hVirtio, qIdx, &reqSegBuf, pDescChain, true /* fFence */);
    virtioQueueSync(pThis->hVirtio, qIdx);

    RTMemFree(abSenseBuf);

    if (!ASMAtomicDecU32(&pThis->cActiveReqs) && pThis->fQuiescing)
        PDMDevHlpAsyncNotificationCompleted(pThis->CTX_SUFF(pDevIns));

    Log2(("---------------------------------------------------------------------------------\n"));

    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMIMEDIAEXPORT,pfnIoReqCompleteNotify}
 */
static DECLCALLBACK(int) virtioScsiIoReqFinish(PPDMIMEDIAEXPORT pInterface, PDMMEDIAEXIOREQ hIoReq,
                                               void *pvIoReqAlloc, int rcReq)
{
    RT_NOREF(pInterface);

    PVIRTIOSCSIREQ    pReq      = (PVIRTIOSCSIREQ)pvIoReqAlloc;
    PVIRTIOSCSITARGET pTarget   = pReq->pTarget;
    PVIRTIOSCSI       pThis     = pTarget->pVirtioScsi;
    PPDMIMEDIAEX      pIMediaEx = pTarget->pDrvMediaEx;

    size_t cbResidual = 0;
    int rc = pIMediaEx->pfnIoReqQueryResidual(pIMediaEx, hIoReq, &cbResidual);
    AssertRC(rc);

    size_t cbXfer = 0;
    rc = pIMediaEx->pfnIoReqQueryXferSize(pIMediaEx, hIoReq, &cbXfer);
    AssertRC(rc);

    /* Masking used to deal with datatype size differences between APIs (Windows complains otherwise) */
    Assert(!(cbXfer & 0xffffffff00000000));
    uint32_t cbXfer32 = cbXfer & 0xffffffff;
    struct REQ_RESP_HDR respHdr = { 0 };
    respHdr.uSenseLen = pReq->pbSense[2] == SCSI_SENSE_NONE ? 0 : (uint32_t)pReq->uSenseLen;
    AssertMsg(!(cbResidual & 0xffffffff00000000),
            ("WARNING: Residual size larger than sizeof(uint32_t), truncating"));
    respHdr.uResidual = (uint32_t)(cbResidual & 0xffffffff);
    respHdr.uStatus   = pReq->uStatus;

    /*  VirtIO 1.0 spec 5.6.6.1.1 says device MUST return a VirtIO response byte value.
     *  Some are returned during the submit phase, and a few are not mapped at all,
     *  wherein anything that can't map specifically gets mapped to VIRTIOSCSI_S_FAILURE
     */
    if (pThis->fResetting)
        respHdr.uResponse = VIRTIOSCSI_S_RESET;
    else
    {
        switch(rcReq)
        {
            case SCSI_STATUS_OK:
            {
                if (pReq->uStatus != SCSI_STATUS_CHECK_CONDITION)
                {
                    respHdr.uResponse = VIRTIOSCSI_S_OK;
                    break;
                }
                uint8_t uSenseKey = pReq->pbSense[2];
                switch (uSenseKey)
                {
                    case SCSI_SENSE_ABORTED_COMMAND:
                        respHdr.uResponse = VIRTIOSCSI_S_ABORTED;
                        break;
                    case SCSI_SENSE_COPY_ABORTED:
                        respHdr.uResponse = VIRTIOSCSI_S_ABORTED;
                        break;
                    case SCSI_SENSE_UNIT_ATTENTION:
                        respHdr.uResponse = VIRTIOSCSI_S_TARGET_FAILURE;
                        break;
                    case SCSI_SENSE_HARDWARE_ERROR:
                        respHdr.uResponse = VIRTIOSCSI_S_TARGET_FAILURE;
                        break;
                    case SCSI_SENSE_NOT_READY:
                        respHdr.uResponse = VIRTIOSCSI_S_BUSY; /* e.g. re-tryable */
                        break;
                    default:
                        respHdr.uResponse = VIRTIOSCSI_S_FAILURE;
                        break;
                }
                break;
            }
            case SCSI_STATUS_CHECK_CONDITION:
                {
                    uint8_t uSenseKey = pReq->pbSense[2];
                    switch (uSenseKey)
                    {
                        case SCSI_SENSE_ABORTED_COMMAND:
                            respHdr.uResponse = VIRTIOSCSI_S_ABORTED;
                            break;
                        case SCSI_SENSE_COPY_ABORTED:
                            respHdr.uResponse = VIRTIOSCSI_S_ABORTED;
                            break;
                        case SCSI_SENSE_UNIT_ATTENTION:
                            respHdr.uResponse = VIRTIOSCSI_S_TARGET_FAILURE;
                            break;
                        case SCSI_SENSE_HARDWARE_ERROR:
                            respHdr.uResponse = VIRTIOSCSI_S_TARGET_FAILURE;
                            break;
                        case SCSI_SENSE_NOT_READY:
                            respHdr.uResponse = VIRTIOSCSI_S_BUSY; /* e.g. re-tryable */
                            break;
                        default:
                            respHdr.uResponse = VIRTIOSCSI_S_FAILURE;
                            break;
                    }
                }
                break;

            default:
                respHdr.uResponse = VIRTIOSCSI_S_FAILURE;
                break;
        }
    }

    const char *getReqRespText = virtioGetReqRespText(respHdr.uResponse);
    Log2Func(("status: (%d) %s,   response: (%d) %s\n",
              pReq->uStatus, SCSIStatusText(pReq->uStatus),
              respHdr.uResponse, getReqRespText));
    RT_NOREF(getReqRespText);

    if (RT_FAILURE(rcReq))
        Log2Func(("rcReq:  %s\n", RTErrGetDefine(rcReq)));

    if (LogIs3Enabled())
    {
        LogFunc(("cbDataIn = %u, cbDataOut = %u (cbIn = %u, cbOut = %u)\n",
                  pReq->cbDataIn, pReq->cbDataOut, pReq->pDescChain->cbPhysReturn, pReq->pDescChain->cbPhysSend));
        LogFunc(("xfer = %lu, residual = %u\n", cbXfer, cbResidual));
        const char *pszTxDirText = virtioGetTxDirText(pReq->enmTxDir);
        LogFunc(("xfer direction: %s, sense written = %d, sense size = %d\n",
             pszTxDirText, respHdr.uSenseLen, pThis->virtioScsiConfig.uSenseSize));
        RT_NOREF(pszTxDirText);
    }

    if (respHdr.uSenseLen && LogIs2Enabled())
    {
        LogFunc(("Sense: %s\n", SCSISenseText(pReq->pbSense[2])));
        LogFunc(("Sense Ext3: %s\n", SCSISenseExtText(pReq->pbSense[12], pReq->pbSense[13])));
    }

    int cSegs = 0;

    if (   (VIRTIO_IN_DIRECTION(pReq->enmTxDir)  && cbXfer32 > pReq->cbDataIn)
        || (VIRTIO_OUT_DIRECTION(pReq->enmTxDir) && cbXfer32 > pReq->cbDataOut))
    {
        Log2Func((" * * * * Data overrun, returning sense\n"));
        uint8_t abSense[] = { RT_BIT(7) | SCSI_SENSE_RESPONSE_CODE_CURR_FIXED,
                              0, SCSI_SENSE_ILLEGAL_REQUEST, 0, 0, 0, 0, 10, 0, 0, 0 };
        respHdr.uSenseLen = sizeof(abSense);
        respHdr.uStatus   = SCSI_STATUS_CHECK_CONDITION;
        respHdr.uResponse = VIRTIOSCSI_S_OVERRUN;
        respHdr.uResidual = pReq->cbDataIn;

        virtioScsiReqErr(pThis, pReq->qIdx, pReq->pDescChain, &respHdr, abSense);
        return VINF_SUCCESS;
    }
    else
    {
        Assert(pReq->pbSense != NULL);

        /* req datain bytes already in guest phys mem. via virtioScsiIoReqCopyFromBuf() */

        RTSGSEG aReqSegs[4];
        aReqSegs[cSegs].pvSeg = &respHdr;
        aReqSegs[cSegs++].cbSeg = sizeof(respHdr);

        aReqSegs[cSegs].pvSeg = pReq->pbSense;
        aReqSegs[cSegs++].cbSeg = pReq->cbSense; /* VirtIO 1.0 spec 5.6.4/5.6.6.1 */

        RTSGBUF reqSegBuf;
        RTSgBufInit(&reqSegBuf, aReqSegs, cSegs);

        size_t cbReqSgBuf = RTSgBufCalcTotalLength(&reqSegBuf);
        AssertMsgReturn(cbReqSgBuf <= pReq->pDescChain->cbPhysReturn,
                       ("Guest expected less req data (space needed: %d, avail: %d)\n",
                         cbReqSgBuf, pReq->pDescChain->cbPhysReturn),
                       VERR_BUFFER_OVERFLOW);


        virtioQueuePut(pThis->hVirtio, pReq->qIdx, &reqSegBuf, pReq->pDescChain, true /* fFence TBD */);
        virtioQueueSync(pThis->hVirtio, pReq->qIdx);


        Log2(("-----------------------------------------------------------------------------------------\n"));
    }

    virtioScsiFreeReq(pTarget, pReq);

    if (!ASMAtomicDecU32(&pThis->cActiveReqs) && pThis->fQuiescing)
        PDMDevHlpAsyncNotificationCompleted(pThis->CTX_SUFF(pDevIns));

    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMIMEDIAEXPORT,pfnIoReqCopyFromBuf}
 *
 * Copy virtual memory from VSCSI layer to guest physical memory
 */
static DECLCALLBACK(int) virtioScsiIoReqCopyFromBuf(PPDMIMEDIAEXPORT pInterface, PDMMEDIAEXIOREQ hIoReq,
                                                      void *pvIoReqAlloc, uint32_t offDst, PRTSGBUF pSgBuf, size_t cbCopy)
{
    RT_NOREF(hIoReq, cbCopy);

    PVIRTIOSCSIREQ pReq = (PVIRTIOSCSIREQ)pvIoReqAlloc;

    if (!pReq->cbDataIn)
        return VINF_SUCCESS;

    PVIRTIOSCSITARGET pTarget = RT_FROM_MEMBER(pInterface, VIRTIOSCSITARGET, IMediaExPort);
    PVIRTIOSCSI pThis = pTarget->pVirtioScsi;

    AssertReturn(pReq->pDescChain, VERR_INVALID_PARAMETER);

    PRTSGBUF pSgPhysReturn = pReq->pDescChain->pSgPhysReturn;
    RTSgBufAdvance(pSgPhysReturn, offDst);

    size_t cbCopied = 0;
    size_t cbRemain = pReq->cbDataIn;

    if (!pSgPhysReturn->idxSeg && pSgPhysReturn->cbSegLeft == pSgPhysReturn->paSegs[0].cbSeg)
        RTSgBufAdvance(pSgPhysReturn, pReq->uDataInOff);

    while (cbRemain)
    {
        PCRTSGSEG paSeg = &pSgPhysReturn->paSegs[pSgPhysReturn->idxSeg];
        uint64_t dstSgStart = (uint64_t)paSeg->pvSeg;
        uint64_t dstSgLen   = (uint64_t)paSeg->cbSeg;
        uint64_t dstSgCur   = (uint64_t)pSgPhysReturn->pvSegCur;
        cbCopied = RT_MIN((uint64_t)pSgBuf->cbSegLeft, dstSgLen - (dstSgCur - dstSgStart));
        PDMDevHlpPhysWrite(pThis->CTX_SUFF(pDevIns),
                          (RTGCPHYS)pSgPhysReturn->pvSegCur, pSgBuf->pvSegCur, cbCopied);
        RTSgBufAdvance(pSgBuf, cbCopied);
        RTSgBufAdvance(pSgPhysReturn, cbCopied);
        cbRemain -= cbCopied;
    }
    RT_UNTRUSTED_NONVOLATILE_COPY_FENCE(); /* needed? */

    Log2Func((".... Copied %lu bytes from %lu byte guest buffer, residual=%lu\n",
         cbCopy, pReq->pDescChain->cbPhysReturn, pReq->pDescChain->cbPhysReturn - cbCopy));

    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMIMEDIAEXPORT,pfnIoReqCopyToBuf}
 *
 * Copy guest physical memory to VSCSI layer virtual memory
 */
static DECLCALLBACK(int) virtioScsiIoReqCopyToBuf(PPDMIMEDIAEXPORT pInterface, PDMMEDIAEXIOREQ hIoReq,
                                                    void *pvIoReqAlloc, uint32_t offSrc, PRTSGBUF pSgBuf, size_t cbCopy)
{
    RT_NOREF(hIoReq, cbCopy);

    PVIRTIOSCSIREQ pReq = (PVIRTIOSCSIREQ)pvIoReqAlloc;

    if (!pReq->cbDataOut)
        return VINF_SUCCESS;

    PVIRTIOSCSITARGET pTarget = RT_FROM_MEMBER(pInterface, VIRTIOSCSITARGET, IMediaExPort);
    PVIRTIOSCSI pThis = pTarget->pVirtioScsi;

    PRTSGBUF pSgPhysSend = pReq->pDescChain->pSgPhysSend;
    RTSgBufAdvance(pSgPhysSend, offSrc);

    size_t cbCopied = 0;
    size_t cbRemain = pReq->cbDataOut;
    while (cbRemain)
    {
        PCRTSGSEG paSeg     = &pSgPhysSend->paSegs[pSgPhysSend->idxSeg];
        uint64_t srcSgStart = (uint64_t)paSeg->pvSeg;
        uint64_t srcSgLen   = (uint64_t)paSeg->cbSeg;
        uint64_t srcSgCur   = (uint64_t)pSgPhysSend->pvSegCur;
        cbCopied = RT_MIN((uint64_t)pSgBuf->cbSegLeft, srcSgLen - (srcSgCur - srcSgStart));
        PDMDevHlpPhysRead(pThis->CTX_SUFF(pDevIns),
                          (RTGCPHYS)pSgPhysSend->pvSegCur, pSgBuf->pvSegCur, cbCopied);
        RTSgBufAdvance(pSgBuf, cbCopied);
        RTSgBufAdvance(pSgPhysSend, cbCopied);
        cbRemain -= cbCopied;
    }

    Log2Func((".... Copied %lu bytes to %lu byte guest buffer, residual=%lu\n",
         cbCopy, pReq->pDescChain->cbPhysReturn, pReq->pDescChain->cbPhysReturn - cbCopy));

    return VINF_SUCCESS;
}

static int virtioScsiReqSubmit(PVIRTIOSCSI pThis, uint16_t qIdx, PVIRTIO_DESC_CHAIN_T pDescChain)
{
    AssertReturn(pDescChain->cbPhysSend, VERR_INVALID_PARAMETER);

    ASMAtomicIncU32(&pThis->cActiveReqs);

    /* Extract command header and CDB from guest physical memory */

    size_t cbReqHdr = sizeof(struct REQ_CMD_HDR) + pThis->virtioScsiConfig.uCdbSize;
    PVIRTIOSCSI_REQ_CMD_T pVirtqReq = (PVIRTIOSCSI_REQ_CMD_T)RTMemAlloc(cbReqHdr);
    AssertReturn(pVirtqReq, VERR_NO_MEMORY);
    uint8_t *pb = (uint8_t *)pVirtqReq;
    for (size_t cb = RT_MIN(pDescChain->cbPhysSend, cbReqHdr); cb; )
    {
        size_t cbSeg = cb;
        RTGCPHYS GCPhys = (RTGCPHYS)RTSgBufGetNextSegment(pDescChain->pSgPhysSend, &cbSeg);
        PDMDevHlpPhysRead(pThis->CTX_SUFF(pDevIns), GCPhys, pb, cbSeg);
        pb += cbSeg;
        cb -= cbSeg;
    }

    uint8_t  uTarget  = pVirtqReq->ReqHdr.uVirtioLun[1];
    uint32_t uScsiLun = (pVirtqReq->ReqHdr.uVirtioLun[2] << 8 | pVirtqReq->ReqHdr.uVirtioLun[3]) & 0x3fff;
    PVIRTIOSCSITARGET pTarget = &pThis->paTargetInstances[uTarget];

    LogFunc(("[%s] (Target: %d LUN: %d)  CDB: %.*Rhxs\n",
        SCSICmdText(pVirtqReq->uCdb[0]), uTarget, uScsiLun,
        virtioScsiEstimateCdbLen(pVirtqReq->uCdb[0],
        pThis->virtioScsiConfig.uCdbSize), pVirtqReq->uCdb));

    Log3Func(("cmd id: %RX64, attr: %x, prio: %d, crn: %x\n",
        pVirtqReq->ReqHdr.uId, pVirtqReq->ReqHdr.uTaskAttr, pVirtqReq->ReqHdr.uPrio, pVirtqReq->ReqHdr.uCrn));

    /*
     * Calculate request offsets
     */
    off_t uDataOutOff = sizeof(REQ_CMD_HDR)  + pThis->virtioScsiConfig.uCdbSize;
    off_t uDataInOff  = sizeof(REQ_RESP_HDR) + pThis->virtioScsiConfig.uSenseSize;
    uint32_t cbDataOut = pDescChain->cbPhysSend - uDataOutOff;
    uint32_t cbDataIn  = pDescChain->cbPhysReturn - uDataInOff;
    /**
     * Handle submission errors
     */

    if (RT_UNLIKELY(pThis->fResetting))
    {
        Log2Func(("Aborting req submission because reset is in progress\n"));
        struct REQ_RESP_HDR respHdr = { 0 };
        respHdr.uSenseLen = 0;
        respHdr.uStatus   = SCSI_STATUS_OK;
        respHdr.uResponse = VIRTIOSCSI_S_RESET;
        respHdr.uResidual = cbDataIn + cbDataOut;
        virtioScsiReqErr(pThis, qIdx, pDescChain, &respHdr, NULL);
        return VINF_SUCCESS;
    }
    else
    if (RT_UNLIKELY(uScsiLun != 0))
    {
        Log2Func(("Error submitting request to bad target (%d) or bad LUN (%d)\n", uTarget, uScsiLun));
        uint8_t abSense[] = { RT_BIT(7) | SCSI_SENSE_RESPONSE_CODE_CURR_FIXED,
                              0, SCSI_SENSE_ILLEGAL_REQUEST,
                              0, 0, 0, 0, 10, SCSI_ASC_LOGICAL_UNIT_NOT_SUPPORTED, 0, 0 };
        struct REQ_RESP_HDR respHdr = { 0 };
        respHdr.uSenseLen = sizeof(abSense);
        respHdr.uStatus   = SCSI_STATUS_CHECK_CONDITION;
        respHdr.uResponse = VIRTIOSCSI_S_OK;
        respHdr.uResidual = cbDataOut + cbDataIn;
        virtioScsiReqErr(pThis, qIdx, pDescChain, &respHdr, abSense);
        return VINF_SUCCESS;
    }
    else
    if (RT_UNLIKELY(uTarget >= pThis->cTargets || !pTarget->fPresent))
    {
        Log2Func(("Error submitting request, target not present!!\n"));
        uint8_t abSense[] = { RT_BIT(7) | SCSI_SENSE_RESPONSE_CODE_CURR_FIXED,
                              0, SCSI_SENSE_NOT_READY, 0, 0, 0, 0, 10, 0, 0, 0 };
        struct REQ_RESP_HDR respHdr = { 0 };
        respHdr.uSenseLen = sizeof(abSense);
        respHdr.uStatus   = SCSI_STATUS_CHECK_CONDITION;
        respHdr.uResponse = VIRTIOSCSI_S_BAD_TARGET;
        respHdr.uResidual = cbDataIn + cbDataOut;
        virtioScsiReqErr(pThis, qIdx, pDescChain, &respHdr , abSense);
        return VINF_SUCCESS;
    }
    else
    if (RT_UNLIKELY(cbDataIn && cbDataOut && !pThis->fHasInOutBufs)) /* VirtIO 1.0, 5.6.6.1.1 */
    {
        Log2Func(("Error submitting request, got datain & dataout bufs w/o INOUT feature negotated\n"));
        uint8_t abSense[] = { RT_BIT(7) | SCSI_SENSE_RESPONSE_CODE_CURR_FIXED,
                              0, SCSI_SENSE_ILLEGAL_REQUEST, 0, 0, 0, 0, 10, 0, 0, 0 };
        struct REQ_RESP_HDR respHdr = { 0 };
        respHdr.uSenseLen = sizeof(abSense);
        respHdr.uStatus   = SCSI_STATUS_CHECK_CONDITION;
        respHdr.uResponse = VIRTIOSCSI_S_FAILURE;
        respHdr.uResidual = cbDataIn + cbDataOut;
        virtioScsiReqErr(pThis, qIdx, pDescChain, &respHdr , abSense);
        return VINF_SUCCESS;
    }

    /*
     * Have underlying driver allocate a req of size set during initialization of this device.
     */
    PDMMEDIAEXIOREQ hIoReq = NULL;
    PVIRTIOSCSIREQ  pReq;
    PPDMIMEDIAEX    pIMediaEx = pTarget->pDrvMediaEx;

    int rc = pIMediaEx->pfnIoReqAlloc(pIMediaEx, &hIoReq, (void **)&pReq, 0 /* uIoReqId */,
                                  PDMIMEDIAEX_F_SUSPEND_ON_RECOVERABLE_ERR);

    AssertMsgRCReturn(rc, ("Failed to allocate I/O request, rc=%Rrc\n", rc), rc);

    pReq->hIoReq      = hIoReq;
    pReq->pTarget     = pTarget;
    pReq->qIdx        = qIdx;
    pReq->cbDataIn    = cbDataIn;
    pReq->cbDataOut   = cbDataOut;
    pReq->pDescChain  = pDescChain;
    pReq->uDataInOff  = uDataInOff;
    pReq->uDataOutOff = uDataOutOff;

    pReq->cbSense = pThis->virtioScsiConfig.uSenseSize;
    pReq->pbSense = (uint8_t *)RTMemAlloc(pReq->cbSense);
    AssertMsgReturn(pReq->pbSense,  ("Out of memory allocating sense buffer"),  VERR_NO_MEMORY);

    /* Note: DrvSCSI allocates one virtual memory buffer for input and output phases of the request */
    rc = pIMediaEx->pfnIoReqSendScsiCmd(pIMediaEx, pReq->hIoReq, uScsiLun,
                                        pVirtqReq->uCdb, (size_t)pThis->virtioScsiConfig.uCdbSize,
                                        PDMMEDIAEXIOREQSCSITXDIR_UNKNOWN, &pReq->enmTxDir,
                                        (size_t)RT_MAX(cbDataIn, cbDataOut),
                                        pReq->pbSense, (size_t)pReq->cbSense, &pReq->uSenseLen,
                                        &pReq->uStatus, 30 * RT_MS_1SEC);

    if (rc != VINF_PDM_MEDIAEX_IOREQ_IN_PROGRESS)
    {
        /*
         * Getting here means the request failed in early in the submission to the lower level driver,
         * and there will be no callback to the finished/completion function for this request
         */
        Log2Func(("Request submission error from lower-level driver\n"));
        uint8_t uASC, uASCQ = 0;
        switch (rc)
        {
            case VERR_NO_MEMORY:
                uASC = SCSI_ASC_SYSTEM_RESOURCE_FAILURE;
                break;
            default:
                uASC = SCSI_ASC_INTERNAL_TARGET_FAILURE;
                break;
        }
        uint8_t abSense[] = { RT_BIT(7) | SCSI_SENSE_RESPONSE_CODE_CURR_FIXED,
                              0, SCSI_SENSE_VENDOR_SPECIFIC,
                              0, 0, 0, 0, 10, uASC, uASCQ, 0 };
        struct REQ_RESP_HDR respHdr = { 0 };
        respHdr.uSenseLen = sizeof(abSense);
        respHdr.uStatus   = SCSI_STATUS_CHECK_CONDITION;
        respHdr.uResponse = VIRTIOSCSI_S_FAILURE;
        respHdr.uResidual = cbDataIn + cbDataOut;
        virtioScsiReqErr(pThis, qIdx, pDescChain, &respHdr, abSense);
        virtioScsiFreeReq(pTarget, pReq);
        return VINF_SUCCESS;
    }

    return VINF_SUCCESS;
}

static int virtioScsiCtrl(PVIRTIOSCSI pThis, uint16_t qIdx, PVIRTIO_DESC_CHAIN_T pDescChain)
{
    RT_NOREF2(pThis, qIdx);

    uint8_t uResponse = VIRTIOSCSI_S_OK;

    PVIRTIO_SCSI_CTRL_UNION_T pScsiCtrlUnion =
        (PVIRTIO_SCSI_CTRL_UNION_T)RTMemAlloc(sizeof(VIRTIO_SCSI_CTRL_UNION_T));

    uint8_t *pb = (uint8_t *)pScsiCtrlUnion;
    for (size_t cb = RT_MIN(pDescChain->cbPhysSend, sizeof(VIRTIO_SCSI_CTRL_UNION_T)); cb; )
    {
        size_t cbSeg = cb;
        RTGCPHYS GCPhys = (RTGCPHYS)RTSgBufGetNextSegment(pDescChain->pSgPhysSend, &cbSeg);
        PDMDevHlpPhysRead(pThis->CTX_SUFF(pDevIns), GCPhys, pb, cbSeg);
        pb += cbSeg;
        cb -= cbSeg;
    }

    /*
     * Mask of events to tell guest driver this device supports
     * See VirtIO 1.0 specification section 5.6.6.2
     */
    uint32_t uSubscribedEvents  =
                            VIRTIOSCSI_EVT_ASYNC_POWER_MGMT
                          | VIRTIOSCSI_EVT_ASYNC_EXTERNAL_REQUEST
                          | VIRTIOSCSI_EVT_ASYNC_MEDIA_CHANGE
                          | VIRTIOSCSI_EVT_ASYNC_DEVICE_BUSY;

    RTSGBUF reqSegBuf;


    switch(pScsiCtrlUnion->scsiCtrl.uType)
    {
        case VIRTIOSCSI_T_TMF: /* Task Management Functions */
        {
            uint8_t  uTarget  = pScsiCtrlUnion->scsiCtrlTmf.uScsiLun[1];
            uint32_t uScsiLun = (pScsiCtrlUnion->scsiCtrlTmf.uScsiLun[2] << 8
                               | pScsiCtrlUnion->scsiCtrlTmf.uScsiLun[3]) & 0x3fff;
            if (LogIs2Enabled())
            {
                const char *pszTmfTypeText = virtioGetTMFTypeText(pScsiCtrlUnion->scsiCtrlTmf.uSubtype);
                Log2Func(("[%s] (Target: %d LUN: %d)  Task Mgt Function: %s\n",
                    QUEUENAME(qIdx), uTarget, uScsiLun, pszTmfTypeText));
                RT_NOREF3(pszTmfTypeText, uTarget, uScsiLun);
            }

            PVIRTIOSCSITARGET pTarget = NULL;
            if (uTarget < pThis->cTargets)
                pTarget = &pThis->paTargetInstances[uTarget];

            if (uTarget >= pThis->cTargets || !pTarget->fPresent)
                uResponse = VIRTIOSCSI_S_BAD_TARGET;
            else
            if (uScsiLun != 0)
                uResponse = VIRTIOSCSI_S_INCORRECT_LUN;
            else
            switch(pScsiCtrlUnion->scsiCtrlTmf.uSubtype)
            {
                case VIRTIOSCSI_T_TMF_ABORT_TASK:
                    uResponse = VIRTIOSCSI_S_FUNCTION_SUCCEEDED;
                    break;
                case VIRTIOSCSI_T_TMF_ABORT_TASK_SET:
                    uResponse = VIRTIOSCSI_S_FUNCTION_SUCCEEDED;
                    break;
                case VIRTIOSCSI_T_TMF_CLEAR_ACA:
                    uResponse = VIRTIOSCSI_S_FUNCTION_SUCCEEDED;
                    break;
                case VIRTIOSCSI_T_TMF_CLEAR_TASK_SET:
                    uResponse = VIRTIOSCSI_S_FUNCTION_SUCCEEDED;
                    break;
                case VIRTIOSCSI_T_TMF_I_T_NEXUS_RESET:
                    uResponse = VIRTIOSCSI_S_FUNCTION_SUCCEEDED;
                    break;
                case VIRTIOSCSI_T_TMF_LOGICAL_UNIT_RESET:
                    uResponse = VIRTIOSCSI_S_FUNCTION_SUCCEEDED;
                    break;
                case VIRTIOSCSI_T_TMF_QUERY_TASK:
                    uResponse = VIRTIOSCSI_S_FUNCTION_REJECTED;
                    //uResponse = VIRTIOSCSI_S_FUNCTION_SUCCEEDED;
                    break;
                case VIRTIOSCSI_T_TMF_QUERY_TASK_SET:
                    uResponse = VIRTIOSCSI_S_FUNCTION_REJECTED;
                    //uResponse = VIRTIOSCSI_S_FUNCTION_SUCCEEDED;
                    break;
                default:
                    LogFunc(("Unknown TMF type\n"));
                    uResponse = VIRTIOSCSI_S_FAILURE;
            }

            RTSGSEG aReqSegs[] = { { &uResponse,  sizeof(uResponse) } };
            RTSgBufInit(&reqSegBuf, aReqSegs, sizeof(aReqSegs) / sizeof(RTSGSEG));

            break;
        }
        case VIRTIOSCSI_T_AN_QUERY: /* Guest SCSI driver is querying supported async event notifications */
        {

            PVIRTIOSCSI_CTRL_AN_T pScsiCtrlAnQuery = &pScsiCtrlUnion->scsiCtrlAsyncNotify;

            uSubscribedEvents &= pScsiCtrlAnQuery->uEventsRequested;

            uint8_t  uTarget  = pScsiCtrlAnQuery->uScsiLun[1];
            uint32_t uScsiLun = (pScsiCtrlAnQuery->uScsiLun[2] << 8 | pScsiCtrlAnQuery->uScsiLun[3]) & 0x3fff;

            PVIRTIOSCSITARGET pTarget = NULL;
            if (uTarget < pThis->cTargets)
                pTarget = &pThis->paTargetInstances[uTarget];

            if (uTarget >= pThis->cTargets || !pTarget->fPresent)
                uResponse = VIRTIOSCSI_S_BAD_TARGET;
            else
            if (uScsiLun != 0)
                uResponse = VIRTIOSCSI_S_INCORRECT_LUN;
            else
                uResponse = VIRTIOSCSI_S_FUNCTION_COMPLETE;

            if (LogIs2Enabled())
            {
                char szTypeText[128];
                virtioGetControlAsyncMaskText(szTypeText, sizeof(szTypeText),
                    pScsiCtrlAnQuery->uEventsRequested);
                Log2Func(("[%s] (Target: %d LUN: %d)  Async. Notification Query: %s\n",
                    QUEUENAME(qIdx), uTarget, uScsiLun, szTypeText));
                RT_NOREF3(szTypeText, uTarget, uScsiLun);

            }
            RTSGSEG aReqSegs[] = { { &uSubscribedEvents, sizeof(uSubscribedEvents) },  { &uResponse, sizeof(uResponse)  } };
            RTSgBufInit(&reqSegBuf, aReqSegs, sizeof(aReqSegs) / sizeof(RTSGSEG));

            break;
        }
        case VIRTIOSCSI_T_AN_SUBSCRIBE: /* Guest SCSI driver is subscribing to async event notification(s) */
        {

            PVIRTIOSCSI_CTRL_AN_T pScsiCtrlAnSubscribe = &pScsiCtrlUnion->scsiCtrlAsyncNotify;

            if (pScsiCtrlAnSubscribe->uEventsRequested & ~SUBSCRIBABLE_EVENTS)
                LogFunc(("Unsupported bits in event subscription event mask: 0x%x\n",
                    pScsiCtrlAnSubscribe->uEventsRequested));

            uSubscribedEvents &= pScsiCtrlAnSubscribe->uEventsRequested;
            pThis->uAsyncEvtsEnabled = uSubscribedEvents;

            uint8_t  uTarget  = pScsiCtrlAnSubscribe->uScsiLun[1];
            uint32_t uScsiLun = (pScsiCtrlAnSubscribe->uScsiLun[2] << 8
                               | pScsiCtrlAnSubscribe->uScsiLun[3]) & 0x3fff;

            if (LogIs2Enabled())
            {
                char szTypeText[128];
                virtioGetControlAsyncMaskText(szTypeText, sizeof(szTypeText), pScsiCtrlAnSubscribe->uEventsRequested);
                Log2Func(("[%s] (Target: %d LUN: %d)  Async. Notification Subscribe: %s\n",
                    QUEUENAME(qIdx), uTarget, uScsiLun, szTypeText));
                RT_NOREF3(szTypeText, uTarget, uScsiLun);

            }

            PVIRTIOSCSITARGET pTarget = NULL;
            if (uTarget < pThis->cTargets)
                pTarget = &pThis->paTargetInstances[uTarget];

            if (uTarget >= pThis->cTargets || !pTarget->fPresent)
                uResponse = VIRTIOSCSI_S_BAD_TARGET;
            else
            if (uScsiLun != 0)
                uResponse = VIRTIOSCSI_S_INCORRECT_LUN;
            else
            {
                /*
                 * TBD: Verify correct status code if request mask is only partially fulfillable
                 *      and confirm when to use 'complete' vs. 'succeeded' See VirtIO 1.0 spec section 5.6.6.2
                 *      and read SAM docs*/
                if (uSubscribedEvents == pScsiCtrlAnSubscribe->uEventsRequested)
                    uResponse = VIRTIOSCSI_S_FUNCTION_SUCCEEDED;
                else
                    uResponse = VIRTIOSCSI_S_FUNCTION_COMPLETE;
            }
            RTSGSEG aReqSegs[] = { { &uSubscribedEvents, sizeof(uSubscribedEvents) },  { &uResponse, sizeof(uResponse)  } };
            RTSgBufInit(&reqSegBuf, aReqSegs, sizeof(aReqSegs) / sizeof(RTSGSEG));

            break;
        }
        default:
            LogFunc(("Unknown control type extracted from %s: %d\n",
                QUEUENAME(qIdx),  &pScsiCtrlUnion->scsiCtrl.uType));

            uResponse = VIRTIOSCSI_S_FAILURE;

            RTSGSEG aReqSegs[] = { { &uResponse,  sizeof(uResponse) } };
            RTSgBufInit(&reqSegBuf, aReqSegs, sizeof(aReqSegs) / sizeof(RTSGSEG));
    }

    const char *pszCtrlRespText = virtioGetCtrlRespText(uResponse);
    LogFunc(("Response code: %s\n", pszCtrlRespText));
    RT_NOREF(pszCtrlRespText);
    virtioQueuePut (pThis->hVirtio, qIdx, &reqSegBuf, pDescChain, true);
    virtioQueueSync(pThis->hVirtio, qIdx);

    return VINF_SUCCESS;
}

/*
 * Unblock the worker thread so it can respond to a state change.
 *
 * @returns VBox status code.
 * @param   pDevIns     The pcnet device instance.
 * @param   pThread     The send thread.
 */
static DECLCALLBACK(int) virtioScsiWorkerWakeUp(PPDMDEVINS pDevIns, PPDMTHREAD pThread)
{
    RT_NOREF(pThread);
    uint16_t qIdx = ((uint64_t)pThread->pvUser) & 0xffff;
    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    return SUPSemEventSignal(pThis->pSupDrvSession, pThis->aWorker[qIdx].hEvtProcess);
}

static int virtioScsiWorker(PPDMDEVINS pDevIns, PPDMTHREAD pThread)
{
    int rc;
    uint16_t qIdx = ((uint64_t)pThread->pvUser) & 0xffff;
    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    PWORKER pWorker = &pThis->aWorker[qIdx];

    if (pThread->enmState == PDMTHREADSTATE_INITIALIZING)
        return VINF_SUCCESS;

    while (pThread->enmState == PDMTHREADSTATE_RUNNING)
    {
        if (virtioQueueIsEmpty(pThis->hVirtio, qIdx))
        {
            /* Atomic interlocks avoid missing alarm while going to sleep & notifier waking the awoken */
            ASMAtomicWriteBool(&pWorker->fSleeping, true);
            bool fNotificationSent = ASMAtomicXchgBool(&pWorker->fNotified, false);
            if (!fNotificationSent)
            {
                Log6Func(("%s worker sleeping...\n", QUEUENAME(qIdx)));
                Assert(ASMAtomicReadBool(&pWorker->fSleeping));
                rc = SUPSemEventWaitNoResume(pThis->pSupDrvSession, pWorker->hEvtProcess, RT_INDEFINITE_WAIT);
                AssertLogRelMsgReturn(RT_SUCCESS(rc) || rc == VERR_INTERRUPTED, ("%Rrc\n", rc), rc);
                if (RT_UNLIKELY(pThread->enmState != PDMTHREADSTATE_RUNNING))
                    break;
                Log6Func(("%s worker woken\n", QUEUENAME(qIdx)));
                ASMAtomicWriteBool(&pWorker->fNotified, false);
            }
            ASMAtomicWriteBool(&pWorker->fSleeping, false);
        }

        if (!pThis->fQueueAttached[qIdx])
        {
            LogFunc(("%s queue not attached, worker aborting...\n", QUEUENAME(qIdx)));
            break;
        }
        if (!pThis->fQuiescing)
        {
             Log6Func(("fetching next descriptor chain from %s\n", QUEUENAME(qIdx)));
             PVIRTIO_DESC_CHAIN_T pDescChain;
             rc = virtioQueueGet(pThis->hVirtio, qIdx, &pDescChain, true);
             if (rc == VERR_NOT_AVAILABLE)
             {
                Log6Func(("Nothing found in %s\n", QUEUENAME(qIdx)));
                continue;
             }

             AssertRC(rc);
             if (qIdx == CONTROLQ_IDX)
                 virtioScsiCtrl(pThis, qIdx, pDescChain);
             else /* request queue index */
             {
                  rc = virtioScsiReqSubmit(pThis, qIdx, pDescChain);
                  if (RT_FAILURE(rc))
                  {
                     LogRel(("Error submitting req packet, resetting %Rrc", rc));
                  }
             }
        }
    }
    return VINF_SUCCESS;
}


DECLINLINE(void) virtioScsiReportEventsMissed(PVIRTIOSCSI pThis, uint16_t uTarget)
{
    virtioScsiSendEvent(pThis, uTarget, VIRTIOSCSI_T_NO_EVENT | VIRTIOSCSI_T_EVENTS_MISSED, 0);
}

#if 0
/* Only invoke this if VIRTIOSCSI_F_HOTPLUG is negotiated during intiailization
 * This effectively removes the SCSI Target/LUN on the guest side
 */
DECLINLINE(void) virtioScsiReportTargetRemoved(PVIRTIOSCSI pThis, uint16_t uTarget)
{
    if (pThis->fHasHotplug)
        virtioScsiSendEvent(pThis, uTarget, VIRTIOSCSI_T_TRANSPORT_RESET,
                VIRTIOSCSI_EVT_RESET_REMOVED);
}

/* Only invoke thi if VIRTIOSCSI_F_HOTPLUG is negotiated during intiailization
 * This effectively adds the SCSI Target/LUN on the guest side
 */
DECLINLINE(void) virtioScsiReportTargetAdded(PVIRTIOSCSI pThis, uint16_t uTarget)
{
    if (pThis->fHasHotplug)
        virtioScsiSendEvent(pThis, uTarget, VIRTIOSCSI_T_TRANSPORT_RESET,
                VIRTIOSCSI_EVT_RESET_RESCAN);
}

DECLINLINE(void) virtioScsiReportTargetReset(PVIRTIOSCSI pThis, uint16_t uTarget)
{
    virtioScsiSendEvent(pThis, uTarget, VIRTIOSCSI_T_TRANSPORT_RESET,
            VIRTIOSCSI_EVT_RESET_HARD);
}

DECLINLINE(void) virtioScsiReportOperChange(PVIRTIOSCSI pThis, uint16_t uTarget)
{
    if (pThis->uSubscribedEvents & VIRTIOSCSI_EVT_ASYNC_OPERATIONAL_CHANGE)
        virtioScsiSendEvent(pThis, uTarget, VIRTIOSCSI_T_ASYNC_NOTIFY,
                VIRTIOSCSI_EVT_ASYNC_OPERATIONAL_CHANGE);
}

DECLINLINE(void) virtioScsiReportPowerMsg(PVIRTIOSCSI pThis, uint16_t uTarget)
{
    if (pThis->uSubscribedEvents & VIRTIOSCSI_EVT_ASYNC_POWER_MGMT)
        virtioScsiSendEvent(pThis, uTarget, VIRTIOSCSI_T_ASYNC_NOTIFY,
                VIRTIOSCSI_EVT_ASYNC_POWER_MGMT);
}

DECLINLINE(void) virtioScsiReportExtReq(PVIRTIOSCSI pThis, uint16_t uTarget)
{
    if (pThis->uSubscribedEvents & VIRTIOSCSI_EVT_ASYNC_EXTERNAL_REQUEST)
        virtioScsiSendEvent(pThis, uTarget, VIRTIOSCSI_T_ASYNC_NOTIFY,
                VIRTIOSCSI_EVT_ASYNC_EXTERNAL_REQUEST);
}

DECLINLINE(void) virtioScsiReportMediaChange(PVIRTIOSCSI pThis, uint16_t uTarget)
{
    if (pThis->uSubscribedEvents & VIRTIOSCSI_EVT_ASYNC_MEDIA_CHANGE)
        virtioScsiSendEvent(pThis, uTarget, VIRTIOSCSI_T_ASYNC_NOTIFY,
                VIRTIOSCSI_EVT_ASYNC_MEDIA_CHANGE);
}

DECLINLINE(void) virtioScsiReportMultiHost(PVIRTIOSCSI pThis, uint16_t uTarget)
{
    if (pThis->uSubscribedEvents & VIRTIOSCSI_EVT_ASYNC_MULTI_HOST)
        virtioScsiSendEvent(pThis, uTarget, VIRTIOSCSI_T_ASYNC_NOTIFY,
                VIRTIOSCSI_EVT_ASYNC_MULTI_HOST);
}

DECLINLINE(void) virtioScsiReportDeviceBusy(PVIRTIOSCSI pThis, uint16_t uTarget)
{
    if (pThis->uSubscribedEvents & VIRTIOSCSI_EVT_ASYNC_DEVICE_BUSY)
        virtioScsiSendEvent(pThis, uTarget, VIRTIOSCSI_T_ASYNC_NOTIFY,
                VIRTIOSCSI_EVT_ASYNC_DEVICE_BUSY);
}

DECLINLINE(void) virtioScsiReportParamChange(PVIRTIOSCSI pThis, uint16_t uTarget, uint32_t uSenseCode, uint32_t uSenseQualifier)
{
    uint32_t uReason = uSenseQualifier << 8 | uSenseCode;
    virtioScsiSendEvent(pThis, uTarget, VIRTIOSCSI_T_PARAM_CHANGE, uReason);

}

#endif

static DECLCALLBACK(void) virtioScsiNotified(VIRTIOHANDLE hVirtio, void *pClient, uint16_t qIdx)
{
    RT_NOREF(hVirtio);

    AssertReturnVoid(qIdx < VIRTIOSCSI_QUEUE_CNT);
    PVIRTIOSCSI pThis = (PVIRTIOSCSI)pClient;
    PWORKER pWorker = &pThis->aWorker[qIdx];

    RTLogFlush(RTLogDefaultInstanceEx(RT_MAKE_U32(0, UINT16_MAX)));

    if (qIdx == CONTROLQ_IDX || IS_REQ_QUEUE(qIdx))
    {
        Log6Func(("%s has available data\n", QUEUENAME(qIdx)));
        /* Wake queue's worker thread up if sleeping */
        if (!ASMAtomicXchgBool(&pWorker->fNotified, true))
        {
            if (ASMAtomicReadBool(&pWorker->fSleeping))
            {
                Log6Func(("waking %s worker.\n", QUEUENAME(qIdx)));
                int rc = SUPSemEventSignal(pThis->pSupDrvSession, pWorker->hEvtProcess);
                AssertRC(rc);
            }
        }
    }
    else if (qIdx == EVENTQ_IDX)
    {
        Log3Func(("Driver queued buffer(s) to %s\n", QUEUENAME(qIdx)));
        if (ASMAtomicXchgBool(&pThis->fEventsMissed, false))
            virtioScsiReportEventsMissed(pThis, 0);
    }
    else
        LogFunc(("Unexpected queue idx (ignoring): %d\n", qIdx));
}

static DECLCALLBACK(void) virtioScsiStatusChanged(VIRTIOHANDLE hVirtio, void *pClient,  uint32_t fVirtioReady)
{
    RT_NOREF(hVirtio);
    PVIRTIOSCSI pThis = (PVIRTIOSCSI)pClient;

    pThis->fVirtioReady = fVirtioReady;

    if (fVirtioReady)
    {
        LogFunc(("VirtIO ready\n-----------------------------------------------------------------------------------------\n"));
        uint64_t features    = virtioGetNegotiatedFeatures(hVirtio);
        pThis->fHasT10pi     = features & VIRTIO_SCSI_F_T10_PI;
        pThis->fHasHotplug   = features & VIRTIO_SCSI_F_HOTPLUG;
        pThis->fHasInOutBufs = features & VIRTIO_SCSI_F_INOUT;
        pThis->fHasLunChange = features & VIRTIO_SCSI_F_CHANGE;
        pThis->fQuiescing    = false;
        pThis->fResetting    = false;

        for (int i = 0; i < VIRTIOSCSI_QUEUE_CNT; i++)
            pThis->fQueueAttached[i] = true;
    }
    else
    {
        LogFunc(("VirtIO is resetting\n"));
        for (int i = 0; i < VIRTIOSCSI_QUEUE_CNT; i++)
            pThis->fQueueAttached[i] = false;
    }
}

/**
 * virtio-scsi debugger info callback.
 *
 * @param   pDevIns     The device instance.
 * @param   pHlp        The output helpers.
 * @param   pszArgs     The arguments.
 */
static DECLCALLBACK(void) virtioScsiInfo(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
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

/**
 * @interface_method_impl{PDMIMEDIAEXPORT,pfnMediumEjected}
 */
static DECLCALLBACK(void) virtioScsiMediumEjected(PPDMIMEDIAEXPORT pInterface)
{
    PVIRTIOSCSITARGET pTarget = RT_FROM_MEMBER(pInterface, VIRTIOSCSITARGET, IMediaExPort);
    PVIRTIOSCSI pThis = pTarget->pVirtioScsi;
    LogFunc(("LUN %d Ejected!\n", pTarget->iTarget));
    if (pThis->pMediaNotify)
    {
         int rc = VMR3ReqCallNoWait(PDMDevHlpGetVM(pThis->CTX_SUFF(pDevIns)), VMCPUID_ANY,
                                   (PFNRT)pThis->pMediaNotify->pfnEjected, 2,
                                    pThis->pMediaNotify, pTarget->iTarget);
         AssertRC(rc);
    }
}

/** @callback_method_impl{FNSSMDEVLIVEEXEC}  */
static DECLCALLBACK(int) virtioScsiLiveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uPass)
{
    LogFunc(("callback"));
    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    RT_NOREF(pThis);
    RT_NOREF(uPass);
    RT_NOREF(pSSM);
    return VINF_SSM_DONT_CALL_AGAIN;
}

/** @callback_method_impl{FNSSMDEVLOADEXEC}  */
static DECLCALLBACK(int) virtioScsiLoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
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
static DECLCALLBACK(int) virtioScsiSaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    LogFunc(("callback"));
    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);

    RT_NOREF(pThis);
    RT_NOREF(pSSM);
    return VINF_SUCCESS;
}

/** @callback_method_impl{FNSSMDEVLOADDONE}  */
static DECLCALLBACK(int) virtioScsiLoadDone(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    LogFunc(("callback"));
    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    RT_NOREF(pThis);
    RT_NOREF(pSSM);
    return VINF_SUCCESS;
}

/**
 * Is asynchronous handling of suspend or power off notification completed?
 *
 * This is called to check whether the device has quiesced.  Don't deadlock.
 * Avoid blocking.  Do NOT wait for anything.
 *
 * @returns true if done, false if more work to be done.
 *
 * @param   pDevIns             The device instance.
 * @remarks The caller will enter the device critical section.
 * @thread  EMT(0)
 */
static DECLCALLBACK(bool) virtioScsiDeviceQuiesced(PPDMDEVINS pDevIns)
{
    LogFunc(("Device I/O activity quiesced.\n"));
    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);

    pThis->fQuiescing = false;

    return true;
}

static void virtioScsiQuiesceDevice(PPDMDEVINS pDevIns)
{
    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);

    /* Prevent worker threads from removing/processing elements from virtq's */
    pThis->fQuiescing = true;

    PDMDevHlpSetAsyncNotification(pDevIns, virtioScsiDeviceQuiesced);

    /* If already quiesced invoke async callback.  */
    if (!ASMAtomicReadU32(&pThis->cActiveReqs))
        PDMDevHlpAsyncNotificationCompleted(pDevIns);
}

/**
 * @interface_method_impl{PDMIMEDIAEXPORT,pfnIoReqStateChanged}
 */
static DECLCALLBACK(void) virtioScsiIoReqStateChanged(PPDMIMEDIAEXPORT pInterface, PDMMEDIAEXIOREQ hIoReq,
                                                      void *pvIoReqAlloc, PDMMEDIAEXIOREQSTATE enmState)
{
    RT_NOREF4(pInterface, hIoReq, pvIoReqAlloc, enmState);
    PVIRTIOSCSI pThis = RT_FROM_MEMBER(pInterface, VIRTIOSCSI, IBase);

    switch (enmState)
    {
        case PDMMEDIAEXIOREQSTATE_SUSPENDED:
        {
            /* Stop considering this request active */
            if (!ASMAtomicDecU32(&pThis->cActiveReqs) && pThis->fQuiescing)
                PDMDevHlpAsyncNotificationCompleted(pThis->CTX_SUFF(pDevIns));
            break;
        }
        case PDMMEDIAEXIOREQSTATE_ACTIVE:
            ASMAtomicIncU32(&pThis->cActiveReqs);
            break;
        default:
            AssertMsgFailed(("Invalid request state given %u\n", enmState));
    }
}

/**
 * @copydoc FNPDMDEVRESET
 */
static DECLCALLBACK(void) virtioScsiReset(PPDMDEVINS pDevIns)
{
    LogFunc(("\n"));
    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    pThis->fResetting = true;
    virtioScsiQuiesceDevice(pDevIns);
}

/**
 * @interface_method_impl{PDMDEVREG,pfnResume}
 */
static DECLCALLBACK(void) virtioScsiResume(PPDMDEVINS pDevIns)
{
    LogFunc(("\n"));

    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);

    pThis->fQuiescing = false;

    /* Wake worker threads flagged to skip pulling queue entries during quiesce
     * to ensure they re-check their queues. Active request queues may already
     * be awake due to new reqs coming in.
     */
     for (uint16_t qIdx = 0; qIdx < VIRTIOSCSI_REQ_QUEUE_CNT; qIdx++)
    {
        PWORKER pWorker = &pThis->aWorker[qIdx];

        if (ASMAtomicReadBool(&pWorker->fSleeping))
        {
            Log6Func(("waking %s worker.\n", QUEUENAME(qIdx)));
            int rc = SUPSemEventSignal(pThis->pSupDrvSession, pWorker->hEvtProcess);
            AssertRC(rc);
        }
    }

    /* Ensure guest is working the queues too. */
    virtioPropagateResumeNotification(pThis->hVirtio);
}

/**
 * @interface_method_impl{PDMDEVREG,pfnSuspend}
 */
static DECLCALLBACK(void) virtioScsiSuspendOrPoweroff(PPDMDEVINS pDevIns)
{
    LogFunc(("\n"));

    virtioScsiQuiesceDevice(pDevIns);

    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);

    /* VM is halted, thus no new I/O being dumped into queues by the guest.
     * Workers have been flagged to stop pulling stuff already queued-up by the guest.
     * Now tell lower-level to to suspend reqs (for example, DrvVD suspends all reqs
     * on its wait queue, and we will get a callback as the state changes to
     * suspended (and later, resumed) for each).
     */
    for (uint32_t i = 0; i < pThis->cTargets; i++)
    {
        PVIRTIOSCSITARGET pTarget = &pThis->paTargetInstances[i];
        if (pTarget->pDrvBase)
            if (pTarget->pDrvMediaEx)
                pTarget->pDrvMediaEx->pfnNotifySuspend(pTarget->pDrvMediaEx);
    }
}

/**
 * Turns on/off the write status LED.
 *
 * @param   pTarget         Pointer to the target device
 * @param   fOn             New LED state.
 */
void virtioScsiSetWriteLed(PVIRTIOSCSITARGET pTarget, bool fOn)
{
    LogFlow(("%s virtioSetWriteLed: %s\n", pTarget->pszTargetName, fOn ? "on" : "off"));
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
    LogFlow(("%s virtioSetReadLed: %s\n", pTarget->pszTargetName, fOn ? "on" : "off"));
    if (fOn)
        pTarget->led.Asserted.s.fReading = pTarget->led.Actual.s.fReading = 1;
    else
        pTarget->led.Actual.s.fReading = fOn;
}

/**
 * Gets the pointer to the status LED of a unit.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   iTarget         The unit which status LED we desire.
 * @param   ppLed           Where to store the LED pointer.
 */
static DECLCALLBACK(int) virtioScsiTargetQueryStatusLed(PPDMILEDPORTS pInterface, unsigned iTarget, PPDMLED *ppLed)
{
  PVIRTIOSCSITARGET pTarget = RT_FROM_MEMBER(pInterface, VIRTIOSCSITARGET, ILed);
    if (iTarget == 0)
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
 * @param   iTarget         The unit which status LED we desire.
 * @param   ppLed           Where to store the LED pointer.
 */
static DECLCALLBACK(int) virtioScsiDeviceQueryStatusLed(PPDMILEDPORTS pInterface, unsigned iTarget, PPDMLED *ppLed)
{
    PVIRTIOSCSI pThis = RT_FROM_MEMBER(pInterface, VIRTIOSCSI, ILeds);
    if (iTarget < pThis->cTargets)
    {
        *ppLed = &pThis->paTargetInstances[iTarget].led;
        Assert((*ppLed)->u32Magic == PDMLED_MAGIC);
        return VINF_SUCCESS;
    }
    return VERR_PDM_LUN_NOT_FOUND;
 }

static int virtioScsiCfgAccessed(PVIRTIOSCSI pThis, uint32_t uOffset,
                                    const void *pv, uint32_t cb, bool fWrite)
{

    AssertReturn(pv && cb <= sizeof(uint32_t), fWrite ? VINF_SUCCESS : VINF_IOM_MMIO_UNUSED_00);

    if (MATCH_SCSI_CONFIG(uNumQueues))
        SCSI_CONFIG_ACCESSOR_READONLY(uNumQueues);
    else
    if (MATCH_SCSI_CONFIG(uSegMax))
        SCSI_CONFIG_ACCESSOR_READONLY(uSegMax);
    else
    if (MATCH_SCSI_CONFIG(uMaxSectors))
        SCSI_CONFIG_ACCESSOR_READONLY(uMaxSectors);
    else
    if (MATCH_SCSI_CONFIG(uCmdPerLun))
        SCSI_CONFIG_ACCESSOR_READONLY(uCmdPerLun);
    else
    if (MATCH_SCSI_CONFIG(uEventInfoSize))
        SCSI_CONFIG_ACCESSOR_READONLY(uEventInfoSize);
    else
    if (MATCH_SCSI_CONFIG(uSenseSize))
        SCSI_CONFIG_ACCESSOR(uSenseSize);
    else
    if (MATCH_SCSI_CONFIG(uCdbSize))
        SCSI_CONFIG_ACCESSOR(uCdbSize);
    else
    if (MATCH_SCSI_CONFIG(uMaxChannel))
        SCSI_CONFIG_ACCESSOR_READONLY(uMaxChannel);
    else
    if (MATCH_SCSI_CONFIG(uMaxTarget))
        SCSI_CONFIG_ACCESSOR_READONLY(uMaxTarget);
    else
    if (MATCH_SCSI_CONFIG(uMaxLun))
        SCSI_CONFIG_ACCESSOR_READONLY(uMaxLun);
    else
    {
        LogFunc(("Bad access by guest to virtio_scsi_config: uoff=%d, cb=%d\n", uOffset, cb));
        return fWrite ? VINF_SUCCESS : VINF_IOM_MMIO_UNUSED_00;
    }
    return VINF_SUCCESS;
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
static DECLCALLBACK(int) virtioScsiDevCapRead(PPDMDEVINS pDevIns, uint32_t uOffset, const void *pv, uint32_t cb)
{
    int rc = VINF_SUCCESS;
    PVIRTIOSCSI  pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);

    rc = virtioScsiCfgAccessed(pThis, uOffset, pv, cb, false);

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
static DECLCALLBACK(int) virtioScsiDevCapWrite(PPDMDEVINS pDevIns, uint32_t uOffset, const void *pv, uint32_t cb)
{
    int rc = VINF_SUCCESS;
    PVIRTIOSCSI  pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);

    rc = virtioScsiCfgAccessed(pThis, uOffset, pv, cb, true);

    return rc;
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
static DECLCALLBACK(void) virtioScsiRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    LogFunc(("Relocating virtio-scsi"));
    RT_NOREF(offDelta);
    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);

    pThis->pDevInsR3 = pDevIns;

    for (uint32_t i = 0; i < pThis->cTargets; i++)
    {
        PVIRTIOSCSITARGET pTarget = &pThis->paTargetInstances[i];
        pTarget->pVirtioScsi = pThis;
    }

    /*
     * Important: Forward to virtio framework!
     */
    virtioRelocate(pDevIns, offDelta);

}

static DECLCALLBACK(int) virtioScsiQueryDeviceLocation(PPDMIMEDIAPORT pInterface, const char **ppcszController,
                                                       uint32_t *piInstance, uint32_t *piTarget)
{
    PVIRTIOSCSITARGET pTarget = RT_FROM_MEMBER(pInterface, VIRTIOSCSITARGET, IMediaPort);
    PPDMDEVINS pDevIns = pTarget->pVirtioScsi->CTX_SUFF(pDevIns);

    AssertPtrReturn(ppcszController, VERR_INVALID_POINTER);
    AssertPtrReturn(piInstance, VERR_INVALID_POINTER);
    AssertPtrReturn(piTarget, VERR_INVALID_POINTER);

    *ppcszController = pDevIns->pReg->szName;
    *piInstance = pDevIns->iInstance;
    *piTarget = pTarget->iTarget;

    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) virtioScsiTargetQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
     PVIRTIOSCSITARGET pTarget = RT_FROM_MEMBER(pInterface, VIRTIOSCSITARGET, IBase);
     PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE,        &pTarget->IBase);
     PDMIBASE_RETURN_INTERFACE(pszIID, PDMIMEDIAPORT,   &pTarget->IMediaPort);
     PDMIBASE_RETURN_INTERFACE(pszIID, PDMIMEDIAEXPORT, &pTarget->IMediaExPort);
     PDMIBASE_RETURN_INTERFACE(pszIID, PDMILEDPORTS,    &pTarget->ILed);
     return NULL;
}

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) virtioScsiDeviceQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PVIRTIOSCSI pThis = RT_FROM_MEMBER(pInterface, VIRTIOSCSI, IBase);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE,         &pThis->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMILEDPORTS,     &pThis->ILeds);

    return NULL;
}

/**
 * Detach notification.
 *
 * One harddisk at one port has been unplugged.
 * The VM is suspended at this point.
 *
 * @param   pDevIns     The device instance.
 * @param   iTarget        The logical unit which is being detached.
 * @param   fFlags      Flags, combination of the PDMDEVATT_FLAGS_* \#defines.
 */
static DECLCALLBACK(void) virtioScsiDetach(PPDMDEVINS pDevIns, unsigned iTarget, uint32_t fFlags)
{
    RT_NOREF(fFlags);
    PVIRTIOSCSI       pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    PVIRTIOSCSITARGET pTarget = &pThis->paTargetInstances[iTarget];

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
 * @param   iTarget        The logical unit which is being detached.
 * @param   fFlags      Flags, combination of the PDMDEVATT_FLAGS_* \#defines.
 */
static DECLCALLBACK(int)  virtioScsiAttach(PPDMDEVINS pDevIns, unsigned iTarget, uint32_t fFlags)
{
    PVIRTIOSCSI       pThis   = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    PVIRTIOSCSITARGET pTarget = &pThis->paTargetInstances[iTarget];
    int rc;

    pThis->pDevInsR3    = pDevIns;
    pThis->pDevInsR0    = PDMDEVINS_2_R0PTR(pDevIns);
    pThis->pDevInsRC    = PDMDEVINS_2_RCPTR(pDevIns);

    AssertMsgReturn(fFlags & PDM_TACH_FLAGS_NOT_HOT_PLUG,
                    ("virtio-scsi: Device does not support hotplugging\n"),
                    VERR_INVALID_PARAMETER);

    /* the usual paranoia */
    AssertRelease(!pTarget->pDrvBase);
    Assert(pTarget->iTarget == iTarget);

    /*
     * Try attach the SCSI driver and get the interfaces,
     * required as well as optional.
     */
    rc = PDMDevHlpDriverAttach(pDevIns, pTarget->iTarget, &pDevIns->IBase,
                               &pTarget->pDrvBase, (const char *)&pTarget->pszTargetName);
    if (RT_SUCCESS(rc))
        pTarget->fPresent = true;
    else
        AssertMsgFailed(("Failed to attach %s. rc=%Rrc\n", pTarget->pszTargetName, rc));

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

    PVIRTIOSCSI  pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);

    RTMemFree(pThis->paTargetInstances);
    pThis->paTargetInstances = NULL;
    for (int qIdx = 0; qIdx < VIRTIOSCSI_QUEUE_CNT; qIdx++)
    {
        PWORKER pWorker = &pThis->aWorker[qIdx];
        if (pWorker->hEvtProcess != NIL_SUPSEMEVENT)
        {
            SUPSemEventClose(pThis->pSupDrvSession, pWorker->hEvtProcess);
            pWorker->hEvtProcess = NIL_SUPSEMEVENT;
        }
    }
     return VINF_SUCCESS;
}

static DECLCALLBACK(int) virtioScsiConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg){

    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);

    PVIRTIOSCSI  pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    int  rc = VINF_SUCCESS;

    pThis->pDevInsR3 = pDevIns;
    pThis->pDevInsR0 = PDMDEVINS_2_R0PTR(pDevIns);
    pThis->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);
    pThis->pSupDrvSession = PDMDevHlpGetSupDrvSession(pDevIns);

    LogFunc(("PDM device instance: %d\n", iInstance));
    RTStrPrintf((char *)pThis->szInstance, sizeof(pThis->szInstance), "VIRTIOSCSI%d", iInstance);

    /* Usable defaults */
    pThis->cTargets = 1;

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

    rc = CFGMR3QueryBoolDef(pCfg, "Bootable", &pThis->fBootable, true);
    if (RT_FAILURE(rc))
         return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("virtio-scsi configuration error: failed to read Bootable as boolean"));
    LogFunc(("Bootable=%RTbool (unimplemented)\n", pThis->fBootable));

    rc = CFGMR3QueryBoolDef(pCfg, "R0Enabled", &pThis->fR0Enabled, false);
    if (RT_FAILURE(rc))
         return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("virtio-scsi configuration error: failed to read R0Enabled as boolean"));

    rc = CFGMR3QueryBoolDef(pCfg, "RCEnabled", &pThis->fRCEnabled, false);
    if (RT_FAILURE(rc))
         return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("virtio-scsi configuration error: failed to read RCEnabled as boolean"));

    VIRTIOPCIPARAMS virtioPciParams, *pVirtioPciParams = &virtioPciParams;
    pVirtioPciParams->uDeviceId      = PCI_DEVICE_ID_VIRTIOSCSI_HOST;
    pVirtioPciParams->uClassBase     = PCI_CLASS_BASE_MASS_STORAGE;
    pVirtioPciParams->uClassSub      = PCI_CLASS_SUB_SCSI_STORAGE_CONTROLLER;
    pVirtioPciParams->uClassProg     = PCI_CLASS_PROG_UNSPECIFIED;
    pVirtioPciParams->uSubsystemId   = PCI_DEVICE_ID_VIRTIOSCSI_HOST;  /* Virtio 1.0 spec allows PCI Device ID here */
    pVirtioPciParams->uInterruptLine = 0x00;
    pVirtioPciParams->uInterruptPin  = 0x01;

    pThis->IBase.pfnQueryInterface = virtioScsiDeviceQueryInterface;

    /* Configure virtio_scsi_config that transacts via VirtIO implementation's Dev. Specific Cap callbacks */
    pThis->virtioScsiConfig.uNumQueues      = VIRTIOSCSI_REQ_QUEUE_CNT;
    pThis->virtioScsiConfig.uSegMax         = VIRTIOSCSI_MAX_SEG_COUNT;
    pThis->virtioScsiConfig.uMaxSectors     = VIRTIOSCSI_MAX_SECTORS_HINT;
    pThis->virtioScsiConfig.uCmdPerLun      = VIRTIOSCSI_MAX_COMMANDS_PER_LUN;
    pThis->virtioScsiConfig.uEventInfoSize  = sizeof(VIRTIOSCSI_EVENT_T); /* Spec says at least this size! */
    pThis->virtioScsiConfig.uSenseSize      = VIRTIOSCSI_SENSE_SIZE_DEFAULT;
    pThis->virtioScsiConfig.uCdbSize        = VIRTIOSCSI_CDB_SIZE_DEFAULT;
    pThis->virtioScsiConfig.uMaxChannel     = VIRTIOSCSI_MAX_CHANNEL_HINT;
    pThis->virtioScsiConfig.uMaxTarget      = pThis->cTargets;
    pThis->virtioScsiConfig.uMaxLun         = VIRTIOSCSI_MAX_LUN;

    rc = virtioConstruct(pDevIns, pThis, &pThis->hVirtio, pVirtioPciParams, pThis->szInstance,
                         VIRTIOSCSI_HOST_SCSI_FEATURES_OFFERED,
                         virtioScsiDevCapRead,
                         virtioScsiDevCapWrite,
                         virtioScsiStatusChanged,
                         virtioScsiNotified,
                         virtioScsiLiveExec,
                         virtioScsiSaveExec,
                         virtioScsiLoadExec,
                         virtioScsiLoadDone,
                         sizeof(VIRTIOSCSI_CONFIG_T) /* cbDevSpecificCap */,
                         (void *)&pThis->virtioScsiConfig /* pDevSpecificCap */);

    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("virtio-scsi: failed to initialize VirtIO"));

    RTStrCopy((char *)pThis->szQueueNames[CONTROLQ_IDX], VIRTIO_MAX_QUEUE_NAME_SIZE, "controlq");
    RTStrCopy((char *)pThis->szQueueNames[EVENTQ_IDX],   VIRTIO_MAX_QUEUE_NAME_SIZE, "eventq");
    for (uint16_t qIdx = VIRTQ_REQ_BASE; qIdx < VIRTQ_REQ_BASE + VIRTIOSCSI_REQ_QUEUE_CNT; qIdx++)
        RTStrPrintf((char *)pThis->szQueueNames[qIdx], VIRTIO_MAX_QUEUE_NAME_SIZE,
            "requestq<%d>", qIdx - VIRTQ_REQ_BASE);

    for (uint16_t qIdx = 0; qIdx < VIRTIOSCSI_QUEUE_CNT; qIdx++)
    {
        rc = virtioQueueAttach(pThis->hVirtio, qIdx, QUEUENAME(qIdx));
        AssertMsgReturn(rc == VINF_SUCCESS, ("Failed to attach queue %s\n", QUEUENAME(qIdx)), rc);
        pThis->fQueueAttached[qIdx] = (rc == VINF_SUCCESS);

        if (qIdx == CONTROLQ_IDX || IS_REQ_QUEUE(qIdx))
        {
            rc = PDMDevHlpThreadCreate(pDevIns, &pThis->aWorker[qIdx].pThread,
                                       (void *)(uint64_t)qIdx, virtioScsiWorker,
                                       virtioScsiWorkerWakeUp, 0, RTTHREADTYPE_IO, QUEUENAME(qIdx));
            if (rc != VINF_SUCCESS)
            {
                LogRel(("Error creating thread for Virtual Queue %s\n", QUEUENAME(qIdx)));
                return rc;
            }

            rc = SUPSemEventCreate(pThis->pSupDrvSession, &pThis->aWorker[qIdx].hEvtProcess);
            if (RT_FAILURE(rc))
                return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                     N_("DevVirtioSCSI: Failed to create SUP event semaphore"));
         }
    }

#ifdef BOOTABLE_SUPPORT_TBD
    if (fBootable)
    {
        /* Register I/O port space for BIOS access. */
        rc = PDMDevHlpIOPortRegister(pDevIns, VIRTIOSCSI_BIOS_IO_PORT, 4, NULL,
                                     virtioScsiBiosIoPortWrite, virtioScsiBiosIoPortRead,
                                     virtioScsiBiosIoPortWriteStr, virtioScsiBiosIoPortReadStr,
                                     "virtio-scsi BIOS");
        if (RT_FAILURE(rc))
            return PDMDEV_SET_ERROR(pDevIns, rc, N_("virtio-scsi cannot register BIOS I/O handlers"));
    }
#endif

    /* Initialize per device instance. */

    Log2Func(("Found %d targets attached to controller\n", pThis->cTargets));

     pThis->paTargetInstances = (PVIRTIOSCSITARGET)RTMemAllocZ(sizeof(VIRTIOSCSITARGET) * pThis->cTargets);
     if (!pThis->paTargetInstances)
          return PDMDEV_SET_ERROR(pDevIns, rc, N_("Failed to allocate memory for target states"));

    for (RTUINT iTarget = 0; iTarget < pThis->cTargets; iTarget++)
    {
        PVIRTIOSCSITARGET pTarget = &pThis->paTargetInstances[iTarget];

        if (RTStrAPrintf(&pTarget->pszTargetName, "VSCSI%u", iTarget) < 0)
            AssertLogRelFailedReturn(VERR_NO_MEMORY);

        /* Initialize static parts of the device. */
        pTarget->iTarget = iTarget;
        pTarget->pVirtioScsi = pThis;
        pTarget->led.u32Magic = PDMLED_MAGIC;

        pTarget->IBase.pfnQueryInterface                 = virtioScsiTargetQueryInterface;

        /* IMediaPort and IMediaExPort interfaces provide callbacks for VD media and downstream driver access */
        pTarget->IMediaPort.pfnQueryDeviceLocation       = virtioScsiQueryDeviceLocation;
        pTarget->IMediaExPort.pfnIoReqCompleteNotify     = virtioScsiIoReqFinish;
        pTarget->IMediaExPort.pfnIoReqCopyFromBuf        = virtioScsiIoReqCopyFromBuf;
        pTarget->IMediaExPort.pfnIoReqCopyToBuf          = virtioScsiIoReqCopyToBuf;
        pTarget->IMediaExPort.pfnIoReqStateChanged       = virtioScsiIoReqStateChanged;
        pTarget->IMediaExPort.pfnMediumEjected           = virtioScsiMediumEjected;
        pTarget->IMediaExPort.pfnIoReqQueryBuf           = NULL; /* When used avoids copyFromBuf CopyToBuf*/
        pTarget->IMediaExPort.pfnIoReqQueryDiscardRanges = NULL;


        pTarget->IBase.pfnQueryInterface                 = virtioScsiTargetQueryInterface;
        pTarget->ILed.pfnQueryStatusLed                  = virtioScsiTargetQueryStatusLed;
        pThis->ILeds.pfnQueryStatusLed                   = virtioScsiDeviceQueryStatusLed;
        pTarget->led.u32Magic                            = PDMLED_MAGIC;

        LogFunc(("Attaching LUN: %s\n", pTarget->pszTargetName));

        AssertReturn(iTarget < pThis->cTargets, VERR_PDM_NO_SUCH_LUN);
        rc = PDMDevHlpDriverAttach(pDevIns, iTarget, &pTarget->IBase, &pTarget->pDrvBase, pTarget->pszTargetName);
        if (RT_SUCCESS(rc))
        {
            pTarget->fPresent = true;

            pTarget->pDrvMedia = PDMIBASE_QUERY_INTERFACE(pTarget->pDrvBase, PDMIMEDIA);
            AssertMsgReturn(VALID_PTR(pTarget->pDrvMedia),
                         ("virtio-scsi configuration error: LUN#%d missing basic media interface!\n", iTarget),
                         VERR_PDM_MISSING_INTERFACE);

            /* Get the extended media interface. */
            pTarget->pDrvMediaEx = PDMIBASE_QUERY_INTERFACE(pTarget->pDrvBase, PDMIMEDIAEX);
            AssertMsgReturn(VALID_PTR(pTarget->pDrvMediaEx),
                         ("virtio-scsi configuration error: LUN#%d missing extended media interface!\n", iTarget),
                         VERR_PDM_MISSING_INTERFACE);

            rc = pTarget->pDrvMediaEx->pfnIoReqAllocSizeSet(pTarget->pDrvMediaEx, sizeof(VIRTIOSCSIREQ));
            AssertMsgReturn(VALID_PTR(pTarget->pDrvMediaEx),
                         ("virtio-scsi configuration error: LUN#%u: Failed to set I/O request size!\n", iTarget),
                         rc);

            pTarget->pMediaNotify = PDMIBASE_QUERY_INTERFACE(pTarget->pDrvBase, PDMIMEDIANOTIFY);
            AssertMsgReturn(VALID_PTR(pTarget->pDrvMediaEx),
                         ("virtio-scsi configuration error: LUN#%u: Failed to get set Media notify obj!\n",
                          iTarget), rc);

        }
        else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
        {
            pTarget->fPresent = false;
            pTarget->pDrvBase = NULL;
            rc = VINF_SUCCESS;
            Log(("virtio-scsi: no driver attached to device %s\n", pTarget->pszTargetName));
        }
        else
        {
            AssertLogRelMsgFailed(("virtio-scsi: Failed to attach %s: %Rrc\n", pTarget->pszTargetName, rc));
            return rc;
        }
    }

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
    PDMDevHlpDBGFInfoRegister(pDevIns, szTmp, "virtio-scsi info", virtioScsiInfo);

    return rc;
}

/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceVirtioSCSI =
{
    /* .u32Version = */             PDM_DEVREG_VERSION,
    /* .uReserved0 = */             0,
    /* .szName = */                 "virtio-scsi",
#ifdef VIRTIOSCSI_GC_SUPPORT
    /* .fFlags = */                 PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_RC | PDM_DEVREG_FLAGS_R0
                                    | PDM_DEVREG_FLAGS_FIRST_SUSPEND_NOTIFICATION
                                    | PDM_DEVREG_FLAGS_FIRST_POWEROFF_NOTIFICATION,
#else
    /* .fFlags = */                 PDM_DEVREG_FLAGS_DEFAULT_BITS
                                    | PDM_DEVREG_FLAGS_FIRST_SUSPEND_NOTIFICATION
                                    | PDM_DEVREG_FLAGS_FIRST_POWEROFF_NOTIFICATION,
#endif
    /* .fClass = */                 PDM_DEVREG_CLASS_MISC,
    /* .cMaxInstances = */          ~0U,
    /* .uSharedVersion = */         42,
    /* .cbInstanceShared = */       sizeof(VIRTIOSCSI),
    /* .cbInstanceCC = */           0,
    /* .cbInstanceRC = */           0,
    /* .cMaxPciDevices = */         1,
    /* .cMaxMsixVectors = */        VBOX_MSIX_MAX_ENTRIES,
    /* .pszDescription = */         "Virtio Host SCSI.\n",
#if defined(IN_RING3)
    /* .pszRCMod = */               "VBoxDDRC.rc",
    /* .pszR0Mod = */               "VBoxDDR0.r0",
    /* .pfnConstruct = */           virtioScsiConstruct,
    /* .pfnDestruct = */            virtioScsiDestruct,
    /* .pfnRelocate = */            virtioScsiRelocate,
    /* .pfnMemSetup = */            NULL,
    /* .pfnPowerOn = */             NULL,
    /* .pfnReset = */               virtioScsiReset,
    /* .pfnSuspend = */             virtioScsiSuspendOrPoweroff,
    /* .pfnResume = */              virtioScsiResume,
    /* .pfnAttach = */              virtioScsiAttach,
    /* .pfnDetach = */              virtioScsiDetach,
    /* .pfnQueryInterface = */      NULL,
    /* .pfnInitComplete = */        NULL,
    /* .pfnPowerOff = */            virtioScsiSuspendOrPoweroff,
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
    /* .pfnConstruct = */           NULL,
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
    /* .pfnConstruct = */           NULL,
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

