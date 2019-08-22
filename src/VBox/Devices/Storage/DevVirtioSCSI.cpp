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


/**
 * @name VirtIO 1.0 SCSI Host feature bits (See VirtIO 1.0 specification, Section 5.6.3)
 * @{  */
#define VIRTIOSCSI_F_INOUT                RT_BIT_64(0)           /** Request is device readable AND writeable         */
#define VIRTIOSCSI_F_HOTPLUG              RT_BIT_64(1)           /** Host allows hotplugging SCSI LUNs & targets      */
#define VIRTIOSCSI_F_CHANGE               RT_BIT_64(2)           /** Host LUNs chgs via VIRTIOSCSI_T_PARAM_CHANGE evt */
#define VIRTIOSCSI_F_T10_PI               RT_BIT_64(3)           /** Add T10 port info (DIF/DIX) in SCSI req hdr      */
/** @} */

#define VIRTIOSCSI_HOST_SCSI_ALL_FEATURES \
            (VIRTIOSCSI_F_INOUT | VIRTIOSCSI_F_HOTPLUG | VIRTIOSCSI_F_CHANGE | VIRTIOSCSI_F_T10_PI)

#define VIRTIOSCSI_HOST_SCSI_FEATURES_OFFERED       0            /**< TBD, support at least hotplug & in/out          */

/**
 * TEMPORARY NOTE: following parameter is set to 1 for early development. Will be increased later
 */
#define VIRTIOSCSI_REQ_QUEUE_CNT                    1            /**< Number of req queues exposed by dev.            */
#define VIRTIOSCSI_QUEUE_CNT                        VIRTIOSCSI_REQ_QUEUE_CNT + 2
#define VIRTIOSCSI_MAX_TARGETS                      1            /**< Can probably determined from higher layers       */
#define VIRTIOSCSI_MAX_LUN                          16383        /* < VirtIO specification, section 5.6.4             */
#define VIRTIOSCSI_MAX_COMMANDS_PER_LUN             1            /* < T.B.D. What is a good value for this?           */
#define VIRTIOSCSI_MAX_SEG_COUNT                    1024         /* < T.B.D. What is a good value for this?           */
#define VIRTIOSCSI_MAX_SECTORS_HINT                 0x10000      /* < VirtIO specification, section 5.6.4             */
#define VIRTIOSCSI_MAX_CHANNEL_HINT                 0            /* < VirtIO specification, section 5.6.4             */
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
#define CONTROLQ_IDX                                0            /**< Spec-defined Index of control queue             */
#define EVENTQ_IDX                                  1            /**< Spec-defined Index of event queue               */
#define VIRTQ_REQ_BASE                              2            /**< Spec-defined base index of request queues       */
#define QUEUENAME(qIdx) (pThis->szQueueNames[qIdx])        /**< Macro to get queue name from its index          */

#define IS_REQ_QUEUE(qIdx) (qIdx >= VIRTQ_REQ_BASE && qIdx < VIRTIOSCSI_QUEUE_CNT)
/**
 * The following struct is the VirtIO SCSI Host Device   device-specific configuration described in section 5.6.4
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
} VIRTIOSCSI_CONFIG_T, PVIRTIOSCSI_CONFIG_T;


/**
 * @name VirtIO 1.0 SCSI Host Device device specific control types
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
    uint8_t  uLUN[8];                                           /**< lun                                             */
    uint32_t uReason;                                           /**< reason                                          */
} VIRTIOSCSI_EVENT_T, *PVIRTIOSCSI_EVENT_T;

/**
 * @name VirtIO 1.0 SCSI Host Device device specific event types
 * @{  */
#define VIRTIOSCSI_EVT_RESET_HARD                   0           /**<                                                 */
#define VIRTIOSCSI_EVT_RESET_RESCAN                 1           /**<                                                 */
#define VIRTIOSCSI_EVT_RESET_REMOVED                2           /**<                                                 */
/** @} */

/**
 * Device operation: reqestq
 *
 * The following struct is described in VirtIO 1.0 Specification, section 5.6.6.1
 */
#define VIRTIOSCSI_SENSE_SIZE_DEFAULT               96          /**< VirtIO 1.0: 96 on reset, guest can change       */
#define VIRTIOSCSI_CDB_SIZE_DEFAULT                 32          /**< VirtIO 1.0: 32 on reset, guest can change       */
#define VIRTIOSCSI_PI_BYTES_IN                      1           /**< Value TBD (see section 5.6.6.1)                 */
#define VIRTIOSCSI_PI_BYTES_OUT                     1           /**< Value TBD (see section 5.6.6.1)                 */
#define VIRTIOSCSI_DATA_OUT                         512         /**< Value TBD (see section 5.6.6.1)                 */

#pragma pack(1)
typedef struct virtio_scsi_req_cmd
{
    /* Device-readable part */
    struct VIRTIOSCSI_REQ_CMD_HDR
    {
        uint8_t  uLUN[8];                                           /**< lun                                          */
        uint64_t uId;                                               /**< id                                           */
        uint8_t  uTaskAttr;                                         /**< task_attr                                    */
        uint8_t  uPrio;                                             /**< prio                                         */
        uint8_t  uCrn;                                              /**< crn                                          */
    } cmdHdr;
        uint8_t  uCdb[1];                                           /**< cdb                                          */

    /** T10 Pi (protection) fields only present if negotiated at VirtIO init */
    struct VIRTIOSCSI_REQ_CMD_PI
    {
        uint32_t uPiBytesOut;                                       /**< pi_bytesout                                  */
        uint32_t uPiBytesIn;                                        /**< pi_bytesin                                   */
        uint8_t  uPiOut[1];                                         /**< pi_out[]        TBD                          */
    } cmdPi;

    uint8_t      uDataOut[1];                                       /**< dataout                                      */

    struct VIRTIOSCSI_REQ_RESP_HDR
    {
        uint32_t uSenseLen;                                         /**< sense_len                                    */
        uint32_t uResidual;                                         /**< residual                                     */
        uint16_t uStatusQualifier;                                  /**< status_qualifier                             */
        uint8_t  uStatus;                                           /**< status                                       */
        uint8_t  uResponse;                                         /**< response                                     */
        uint8_t  uSense[1];                                         /**< sense                                        */
    } respHdr;
    /** T10 Pi (protection) field only present if negotiated at VirtIO init */
    uint8_t      uPiIn[1];                                          /**< pi_in[]                                      */
    uint8_t      uDataIn[1];                                        /**< detain;                                      */
}  VIRTIOSCSI_REQ_CMD_T, *PVIRTIOSCSI_REQ_CMD_T;
#pragma pack()

/**
 * @name VirtIO 1.0 SCSI Host Device Req command-specific response values
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

/**
 * @name VirtIO 1.0 SCSI Host Device command-specific task_attr values
 * @{  */
#define VIRTIOSCSI_S_SIMPLE                        0           /**<                                                  */
#define VIRTIOSCSI_S_ORDERED                       1           /**<                                                  */
#define VIRTIOSCSI_S_HEAD                          2           /**<                                                  */
#define VIRTIOSCSI_S_ACA                           3           /**<                                                  */
/** @} */

/**
 * @name VirtIO 1.0 SCSI Host Device Control command before we know type (5.6.6.2)
 * @{  */
typedef struct virtio_scsi_ctrl
{
    uint32_t uType;
} VIRTIOSCSI_CTRL, *PVIRTIOSCSI_CTRL_T;

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

#pragma pack(1)
typedef struct virtio_scsi_ctrl_tmf
{
     // Device-readable part
    uint32_t uType;                                            /** type                                              */
    uint32_t uSubtype;                                         /** subtype                                           */
    uint8_t  uLUN[8];                                          /** lun                                               */
    uint64_t uId;                                              /** id                                                */
    // Device-writable part
    uint8_t  uResponse;                                        /** response                                          */
} VIRTIOSCSI_CTRL_TMF_T, *PVIRTIOSCSI_CTRL_TMF_T;
#pragma pack(0)

/**
 * @name VirtIO 1.0 SCSI Host Device device specific tmf control response values
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
    uint8_t   uLUN[8];                                        /** lun                                                */
    uint32_t  uEventRequested;                                /** event_requested                                    */
    // Device-writable part
    uint32_t  uEventActual;                                   /** event_actual                                       */
    uint8_t   uResponse;                                      /** response                                           */
}  VIRTIOSCSI_CTRL_AN, *PVIRTIOSCSI_CTRL_AN_T;
#pragma pack()

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
 * Worker thread context
 */
typedef struct WORKER
{
    R3PTRTYPE(PPDMTHREAD)           pThread;
    SUPSEMEVENT                     hEvtProcess;
    bool                            fSleeping;
    bool                            fNotified;
} WORKER, *PWORKER;

/**
 * State of a target attached to the VirtIO SCSI Host
 */
typedef struct VIRTIOSCSITARGET
{
    /** Pointer to PCI device that owns this target instance. - R3 pointer */
    R3PTRTYPE(struct VIRTIOSCSI *)  pThisR3;

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
    /** Opaque handle to VirtIO common framework (must be first item
     *  in this struct so PDMINS_2_DATA macro's casting works) */
    VIRTIOHANDLE                    hVirtio;

    /** SCSI target instances data */
    VIRTIOSCSITARGET                aTargetInstances[VIRTIOSCSI_MAX_TARGETS];

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

    /** The event semaphore the processing thread waits on. */

    /** Number of ports detected */
    uint64_t                        cTargets;

    /** True if PDMDevHlpAsyncNotificationCompleted should be called when port goes idle */
    bool volatile                   fSignalIdle;

    VIRTIOSCSI_CONFIG_T             virtioScsiConfig;
    bool                            fVirtioReady;
    bool                            fHasT10pi;
    bool                            fHasHotplug;
    bool                            fHasInOutBufs;
    bool                            fHasLunChange;



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
    { \
        uint32_t uIntraOffset = uOffset - RT_UOFFSETOF(VIRTIOSCSI_CONFIG_T, member); \
        if (fWrite) \
            memcpy(((char *)&pThis->virtioScsiConfig.member) + uIntraOffset, (const char *)pv, cb); \
        else \
            memcpy((char *)pv, (const char *)(((char *)&pThis->virtioScsiConfig.member) + uIntraOffset), cb); \
        LOG_ACCESSOR(member); \
    }

#define SCSI_CONFIG_ACCESSOR_READONLY(member) \
    { \
        uint32_t uIntraOffset = uOffset - RT_UOFFSETOF(VIRTIOSCSI_CONFIG_T, member); \
        if (fWrite) \
            LogFunc(("Guest attempted to write readonly virtio_pci_common_cfg.%s\n", #member)); \
        else \
        { \
            memcpy((char *)pv, (const char *)(((char *)&pThis->virtioScsiConfig.member) + uIntraOffset), cb); \
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

#if 0
 */
typedef struct virtio_scsi_config
{
    uint32_t uNumQueues;                                         /**< num_queues      # of req q's exposed by dev    */
    uint32_t uSegMax;                                            /**< seg_max         Max # of segs allowed in cmd   */
    uint32_t uMaxSectors;                                        /**< max_sectors     Hint to guest max xfer to use  */
    uint32_t uCmdPerLun;                                         /**< cmd_per_lun     Max # of link cmd sent per lun */
    uint32_t uEventInfoSize;                                     /**< event_info_size Fill max, evtq bufs            */
    uint32_t uSenseSize;                                         /**< sense_size      Max sense data size dev writes */
    uint32_t uCdbSize;                                           /**< cdb_size        Max CDB size driver writes     */
    uint16_t uMaxChannel;                                        /**< max_channel     Hint to guest driver           */
    uint16_t uMaxTarget;                                         /**< max_target      Hint to guest driver           */
    uint32_t uMaxLun;                                            /**< max_lun         Hint to guest driver           */
} VIRTIOSCSI_CONFIG_T, PVIRTIOSCSI_CONFIG_T;

typedef struct virtio_scsi_req_cmd
{
    /* Device-readable part */
    struct VIRTIOSCSI_REQ_CMD_HDR
    {
        uint8_t  uLUN[8];                                           /**< lun                                          */
        uint64_t uId;                                               /**< id                                           */
        uint8_t  uTaskAttr;                                         /**< task_attr                                    */
        uint8_t  uPrio;                                             /**< prio                                         */
        uint8_t  uCrn;                                              /**< crn                                          */
    } cmdHdr;
        uint8_t  uCdb[1];                                           /**< cdb                                          */

    /** T10 Pi (protection) fields only present if negotiated at VirtIO init */
    struct VIRTIOSCSI_REQ_CMD_PI
    {
        uint32_t uPiBytesOut;                                       /**< pi_bytesout                                  */
        uint32_t uPiBytesIn;                                        /**< pi_bytesin                                   */
        uint8_t  uPiOut[1];                                         /**< pi_out[]        TBD                          */
    } cmdPi;

    uint8_t      uDataOut[1];                                       /**< dataout                                      */

    struct VIRTIOSCSI_REQ_RESP_HDR
    {
        uint32_t uSenseLen;                                         /**< sense_len                                    */
        uint32_t uResidual;                                         /**< residual                                     */
        uint16_t uStatusQualifier;                                  /**< status_qualifier                             */
        uint8_t  uStatus;                                           /**< status                                       */
        uint8_t  uResponse;                                         /**< response                                     */
    } respHdr;
        uint8_t  uSense[1];                                         /**< sense                                        */
    /** T10 Pi (protection) field only present if negotiated at VirtIO init */
    uint8_t      uPiIn[1];                                          /**< pi_in[]                                      */
    uint8_t      uDataIn[1];                                        /**< detain;                                      */
}  VIRTIOSCSI_REQ_CMD_T, *PVIRTIOSCSI_REQ_CMD_T;
    pThis->fHasT10pi     = features & VIRTIOSCSI_F_T10_PI;
    pThis->fHasHotplug   = features & VIRTIOSCSI_F_HOTPLUG;
    pThis->fHasInOutBufs = features & VIRTIOSCSI_F_INOUT;
    pThis->fHasLunChange = features & VIRTIOSCSI_F_CHANGE;

#endif
static int virtioScsiHandleReq(PVIRTIOSCSI pThis, uint16_t qIdx, PRTSGBUF pInSgBuf, PRTSGBUF pOutSgBuf)
{
    RT_NOREF(pInSgBuf);
    RT_NOREF(qIdx);
    AssertMsgReturn(pOutSgBuf->cSegs, ("Req. has no OUT data, unexpected/TBD\n"), VERR_NOT_IMPLEMENTED);

    size_t cbOut = RTSgBufCalcTotalLength(pOutSgBuf);
    size_t cbCdb = pThis->virtioScsiConfig.uCdbSize;
    size_t cbSense = pThis->virtioScsiConfig.uSenseSize;
    size_t cbCmdHdr =  RT_SIZEOFMEMB(VIRTIOSCSI_REQ_CMD_T, cmdHdr)  + cbCdb;
    size_t cbDataOut = cbOut - cbCmdHdr;

    RT_NOREF(cbSense);

    AssertMsgReturn(cbOut >= RT_SIZEOFMEMB(VIRTIOSCSI_REQ_CMD_T, cmdHdr), ("Req to short"), VERR_BUFFER_UNDERFLOW);

    /** Actual CDB bytes didn't fill negotiated space allocated for it, adjust size */
    if (cbOut <= cbCmdHdr)
        cbCdb -= (cbCmdHdr - cbOut);

    /**
     * For the time being, assume one descriptor chain has the complete req data to write to device,
     * and that it's not too much to shove into virtual memory at once
     */
    PVIRTIOSCSI_REQ_CMD_T pVirtqReq = (PVIRTIOSCSI_REQ_CMD_T)RTMemAllocZ(cbOut);
    off_t cbOff = 0;
    size_t cbSeg = 0;
    size_t cbLeft = cbOut;
    while (cbLeft)
    {
        RTGCPHYS pvSeg = (RTGCPHYS)RTSgBufGetNextSegment(pOutSgBuf, &cbSeg);
        PDMDevHlpPhysRead(pThis->CTX_SUFF(pDevIns), pvSeg, pVirtqReq + cbOff, cbSeg);
        cbLeft -= cbSeg;
        cbOff += cbSeg;
    }

    LogFunc(("LUN: %.8Rhxs, id: %RX64, attr: %x, prio: %d, crn: %x\n",
            pVirtqReq->cmdHdr.uLUN, pVirtqReq->cmdHdr.uId, pVirtqReq->cmdHdr.uTaskAttr,
            pVirtqReq->cmdHdr.uPrio, pVirtqReq->cmdHdr.uCrn));
    LogFunc(("CDB: %.*Rhxs\n", cbCdb, &(pVirtqReq->uCdb)));

    if (cbDataOut)
        LogFunc(("dataout[]: %.*Rhxs\n", cbDataOut, pVirtqReq->uDataOut));
    else
        LogFunc(("dataout[]: empty\n"));

    PDMMEDIAEXIOREQ hIoReq = NULL;
    RT_NOREF(hIoReq);
    uint8_t uTarget = pVirtqReq->cmdHdr.uLUN[1];

    Assert(uTarget <= pThis->cTargets);
    PPDMIMEDIAEX pMediaEx = pThis->aTargetInstances[uTarget].pDrvMediaEx;
    RT_NOREF(pMediaEx);
//    pMediaEx->pfnIoReqAlloc(pMediaX, &hIoReq, (void **)
//        pThis->Lun0.pIMediaEx, &hIoReq, (void **)&pReq, 0 /* uTag */, PDMIMEDIAEX_F_DEFAULT);

//   size_t cbRespHdr  =  RT_SIZEOFMEMB(VIRTIOSCSI_REQ_CMD_T, respHdr) +  cbSense;
//    size_t cbOut = RTSgBufCalcTotalLength(pOutSgBuf);

    return VINF_SUCCESS;
}


static int virtioScsiHandleControlCmd(PVIRTIOSCSI pThis, uint16_t qIdx, PRTSGBUF pInSgBuf, PRTSGBUF pOutSgBuf)
{
    RT_NOREF(pThis);
    RT_NOREF(qIdx);
    RT_NOREF(pInSgBuf);

    Log2Func(("qidx=%d, %s\n", qIdx, QUEUENAME(qIdx)));
    /**
     * According to the VirtIO 1.0 SCSI Host device, spec, section 5.6.6.2, control packets are
     * extremely small, so more than one segment is highly unlikely but not a bug. Get the
     * the controlq sg buffer into virtual memory. */

    size_t cbOut = RTSgBufCalcTotalLength(pOutSgBuf);

    PVIRTIOSCSI_CTRL_T pScsiCtrl = (PVIRTIOSCSI_CTRL_T)RTMemAllocZ(cbOut);
    AssertMsgReturn(pScsiCtrl, ("Out of memory"), VERR_NO_MEMORY);

    off_t cbOff = 0;
    size_t cbSeg = 0;
    while (cbOut)
    {
        RTGCPHYS pvSeg = (RTGCPHYS)RTSgBufGetNextSegment(pOutSgBuf, &cbSeg);
        PDMDevHlpPhysRead(pThis->CTX_SUFF(pDevIns), pvSeg, pScsiCtrl + cbOff, cbSeg);
        cbOut -= cbSeg;
        cbOff += cbSeg;
    }

    switch(pScsiCtrl->uType)
    {
        case VIRTIOSCSI_T_TMF:
        {
            PVIRTIOSCSI_CTRL_TMF_T pScsiCtrlTmf = (PVIRTIOSCSI_CTRL_TMF_T)pScsiCtrl;
            Log(("Task Mgt Function, LUN: %.8Rhxs   ", pScsiCtrlTmf->uLUN));

            switch(pScsiCtrlTmf->uSubtype)
            {
                case VIRTIOSCSI_T_TMF_ABORT_TASK:
                    Log(("ABORT TASK\n"));
                    break;
                case VIRTIOSCSI_T_TMF_ABORT_TASK_SET:
                    Log(("ABORT TASK SET\n"));
                    break;
                case VIRTIOSCSI_T_TMF_CLEAR_ACA:
                    Log(("CLEAR ACA\n"));
                    break;
                case VIRTIOSCSI_T_TMF_CLEAR_TASK_SET:
                    Log(("CLEAR TASK SET\n"));
                    break;
                case VIRTIOSCSI_T_TMF_I_T_NEXUS_RESET:
                    Log(("I T NEXUS RESET\n"));
                    break;
                case VIRTIOSCSI_T_TMF_LOGICAL_UNIT_RESET:
                    Log(("LOGICAL UNIT RESET\n"));
                    break;
                case VIRTIOSCSI_T_TMF_QUERY_TASK:
                    Log(("QUERY TASK\n"));
                    break;
                case VIRTIOSCSI_T_TMF_QUERY_TASK_SET:
                    Log(("QUERY TASK SET\n"));
                    break;
                default:
                    Log(("<unknown>\n"));
                    break;
            }
            break;
        }
        case VIRTIOSCSI_T_AN_QUERY:
        {
            PVIRTIOSCSI_CTRL_AN_T pScsiCtrlAnQuery = (PVIRTIOSCSI_CTRL_AN_T)pScsiCtrl;
            Log(("Async Query, LUN: %.8Rhxs   ", pScsiCtrlAnQuery->uLUN));
            switch(pScsiCtrlAnQuery->uEventRequested)
            {
                case VIRTIOSCSI_EVT_ASYNC_OPERATIONAL_CHANGE:
                    Log(("OPERATIONAL CHANGE\n"));
                    break;
                case VIRTIOSCSI_EVT_ASYNC_POWER_MGMT:
                    Log(("POWER MGMT\n"));
                    break;
                case VIRTIOSCSI_EVT_ASYNC_EXTERNAL_REQUEST:
                    Log(("EXTERNAL REQUEST\n"));
                    break;
                case VIRTIOSCSI_EVT_ASYNC_MEDIA_CHANGE:
                    Log(("MEDIA CHANGE\n"));
                    break;
                case VIRTIOSCSI_EVT_ASYNC_MULTI_HOST:
                    Log(("MULTI HOST\n"));
                    break;
                case VIRTIOSCSI_EVT_ASYNC_DEVICE_BUSY:
                    Log(("DEVICE BUSY\n"));
                    break;
                default:
                    Log(("<unknown>\n"));
                    break;
            }
            break;
        }
        case VIRTIOSCSI_T_AN_SUBSCRIBE:
        {
            PVIRTIOSCSI_CTRL_AN_T pScsiCtrlAnSubscribe = (PVIRTIOSCSI_CTRL_AN_T)pScsiCtrl;
            Log(("Async Subscribe, LUN %.8Rxhs   ", pScsiCtrlAnSubscribe->uLUN));
            switch(pScsiCtrlAnSubscribe->uEventRequested)
            {
                case VIRTIOSCSI_EVT_ASYNC_OPERATIONAL_CHANGE:
                    Log(("OPERATIONAL CHANGE\n"));
                    break;
                case VIRTIOSCSI_EVT_ASYNC_POWER_MGMT:
                    Log(("POWER MGMT\n"));
                    break;
                case VIRTIOSCSI_EVT_ASYNC_EXTERNAL_REQUEST:
                    Log(("EXTERNAL REQUEST\n"));
                    break;
                case VIRTIOSCSI_EVT_ASYNC_MEDIA_CHANGE:
                    Log(("MEDIA CHANGE\n"));
                    break;
                case VIRTIOSCSI_EVT_ASYNC_MULTI_HOST:
                    Log(("MULTI HOST\n"));
                    break;
                case VIRTIOSCSI_EVT_ASYNC_DEVICE_BUSY:
                    Log(("DEVICE BUSY\n"));
                    break;
                default:
                    Log(("<unknown>\n"));
                    break;
            }
            break;
        }
        default:
            Log(("Unknown control type extracted from %s: %d\n", QUEUENAME(qIdx), pScsiCtrl->uType));
    }

    return VINF_SUCCESS;
}

/*
 * Unblock the worker thread so it can respond to a state change.
 *
 * @returns VBox status code.
 * @param   pDevIns     The pcnet device instance.
 * @param   pThread     The send thread.
 */
static DECLCALLBACK(int) virtioScsiR3WorkerWakeUp(PPDMDEVINS pDevIns, PPDMTHREAD pThread)
{
    RT_NOREF(pThread);
    uint16_t qIdx = ((uint64_t)pThread->pvUser) & 0xffff;
    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    return SUPSemEventSignal(pThis->pSupDrvSession, pThis->aWorker[qIdx].hEvtProcess);
}

static int virtioScsiWorkIncoming(PPDMDEVINS pDevIns, PPDMTHREAD pThread)
{
    int rc;
    uint16_t qIdx = ((uint64_t)pThread->pvUser) & 0xffff;
    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    PWORKER pWorker = &pThis->aWorker[qIdx];
    PRTSGBUF    pInSgBuf;
    PRTSGBUF    pOutSgBuf;

    if (pThread->enmState == PDMTHREADSTATE_INITIALIZING)
        return VINF_SUCCESS;

    while (pThread->enmState == PDMTHREADSTATE_RUNNING)
    {
        /** Interlocks avoid missing alarm while going to sleep & notifier doesn't wake the awoken */
        ASMAtomicWriteBool(&pWorker->fSleeping, true);
        bool fNotificationSent = ASMAtomicXchgBool(&pWorker->fNotified, false);
        if (!fNotificationSent)
        {
            Assert(ASMAtomicReadBool(&pWorker->fSleeping));
            rc = SUPSemEventWaitNoResume(pThis->pSupDrvSession, pWorker->hEvtProcess, RT_INDEFINITE_WAIT);
            AssertLogRelMsgReturn(RT_SUCCESS(rc) || rc == VERR_INTERRUPTED, ("%Rrc\n", rc), rc);
            if (RT_UNLIKELY(pThread->enmState != PDMTHREADSTATE_RUNNING))
                break;
            Log3Func(("Woken-up\n"));
            ASMAtomicWriteBool(&pWorker->fNotified, false);
        }
        ASMAtomicWriteBool(&pWorker->fSleeping, false);

        Log2Func(("Fetching next descriptor chain\n"));
        rc = virtioQueueGet(pThis->hVirtio, qIdx, true, &pInSgBuf, &pOutSgBuf);
        if (rc == VERR_NOT_AVAILABLE)
        {
            Log2Func(("Nothing found in %s\n", QUEUENAME(qIdx)));
            continue;
        }

        AssertRC(rc);
        if (qIdx == CONTROLQ_IDX)
            virtioScsiHandleControlCmd(pThis, qIdx, pInSgBuf, pOutSgBuf);
        else
            virtioScsiHandleReq(pThis, qIdx, pInSgBuf, pOutSgBuf);
    }
    return VINF_SUCCESS;
}

static DECLCALLBACK(void) virtioScsiQueueNotified(VIRTIOHANDLE hVirtio, void *pClient, uint16_t qIdx)
{
    RT_NOREF(hVirtio);

    AssertReturnVoid(qIdx < VIRTIOSCSI_QUEUE_CNT);
    PVIRTIOSCSI pThis = (PVIRTIOSCSI)pClient;
    PWORKER pWorker = &pThis->aWorker[qIdx];

    Log2Func(("%s has available data\n", QUEUENAME(qIdx)));
    if (qIdx == CONTROLQ_IDX || IS_REQ_QUEUE(qIdx))
    {
        /** Wake queue's worker thread up if sleeping */
        if (!ASMAtomicXchgBool(&pWorker->fNotified, true))
        {
            if (ASMAtomicReadBool(&pWorker->fSleeping))
            {
                Log2Func(("Waking %s worker.\n", QUEUENAME(qIdx)));
                int rc = SUPSemEventSignal(pThis->pSupDrvSession, pWorker->hEvtProcess);
                AssertRC(rc);
            }
        }
    }
    else
        Log2Func(("%s has available data\n", QUEUENAME(qIdx)));
}

static DECLCALLBACK(void) virtioScsiStatusChanged(VIRTIOHANDLE hVirtio, void *pClient,  bool fVirtioReady)
{
    Log2Func(("\n"));
    RT_NOREF(hVirtio);
    PVIRTIOSCSI pThis = (PVIRTIOSCSI)pClient;
    pThis->fVirtioReady = fVirtioReady;
    if (fVirtioReady)
    {
        Log2Func(("VirtIO ready\n"));
        uint64_t features = virtioGetNegotiatedFeatures(hVirtio);
        pThis->fHasT10pi     = features & VIRTIOSCSI_F_T10_PI;
        pThis->fHasHotplug   = features & VIRTIOSCSI_F_HOTPLUG;
        pThis->fHasInOutBufs = features & VIRTIOSCSI_F_INOUT;
        pThis->fHasLunChange = features & VIRTIOSCSI_F_CHANGE;
    }
    else
    {
        Log2Func(("VirtIO is resetting\n"));
        for (int i = 0; i < VIRTIOSCSI_QUEUE_CNT; i++)
            pThis->fQueueAttached[i] = false;
    }
}

/*static void virtioScsiEventToClient(PPDMDEVINS pDevIns, PPDMTHREAD pThread)
{ } */


/**
 * Implementation invokes this to reset the VirtIO device
 */
static void virtioScsiDeviceReset(PVIRTIOSCSI pThis)
{
    pThis->virtioScsiConfig.uSenseSize = VIRTIOSCSI_SENSE_SIZE_DEFAULT;
    pThis->virtioScsiConfig.uCdbSize = VIRTIOSCSI_CDB_SIZE_DEFAULT;
    virtioResetAll(pThis->hVirtio);
}

static int virtioScsiR3CfgAccessed(PVIRTIOSCSI pThis, uint32_t uOffset,
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
    PVIRTIOSCSI  pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);

//    LogFunc(("Read from Device-Specific capabilities: uOffset: 0x%x, cb: 0x%x\n",
//              uOffset, cb));

    rc = virtioScsiR3CfgAccessed(pThis, uOffset, pv, cb, false);

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
    PVIRTIOSCSI  pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);

//    LogFunc(("Write to Device-Specific capabilities: uOffset: 0x%x, cb: 0x%x\n",
//              uOffset, cb));

    rc = virtioScsiR3CfgAccessed(pThis, uOffset, pv, cb, true);

    return rc;
}


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
static DECLCALLBACK(void) virtioScsiR3PDMReset(PPDMDEVINS pDevIns)
{
    PVIRTIOSCSI pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);
    virtioScsiDeviceReset(pThis);

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
        pTarget->pThisR3 = pThis;;
    }

    /**
     * Important: Forward to virtio framework!
     */
    virtioRelocate(pDevIns, offDelta);

}

static DECLCALLBACK(int) virtioScsiR3QueryDeviceLocation(PPDMIMEDIAPORT pInterface, const char **ppcszController,
                                                       uint32_t *piInstance, uint32_t *piLUN)
{
    PVIRTIOSCSITARGET pThisTarget = RT_FROM_MEMBER(pInterface, VIRTIOSCSITARGET, IMediaPort);
    PPDMDEVINS pDevIns = pThisTarget->CTX_SUFF(pThis)->CTX_SUFF(pDevIns);

    AssertPtrReturn(ppcszController, VERR_INVALID_POINTER);
    AssertPtrReturn(piInstance, VERR_INVALID_POINTER);
    AssertPtrReturn(piLUN, VERR_INVALID_POINTER);

    *ppcszController = pDevIns->pReg->szName;
    *piInstance = pDevIns->iInstance;
    *piLUN = pThisTarget->iLUN;

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
        cbCopied = vboxscsiCopyToBuf(&pTarget->CTX_SUFF(pThis)->VBoxSCSI, pSgBuf, offDst, cbCopy);
    else
        cbCopied = virtioScsiR3CopySgBufToGuest(pTarget->CTX_SUFF(pThis), pReq, pSgBuf, offDst, cbCopy);
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
        cbCopied = vboxscsiCopyFromBuf(&pTarget->CTX_SUFF(pThis)->VBoxSCSI, pSgBuf, offSrc, cbCopy);
    else
        cbCopied = vboxscsiR3CopySgBufFromGuest(pTarget->CTX_SUFF(pThis), pReq, pSgBuf, offSrc, cbCopy);
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
//    virtioScsiR3ReqComplete(pTarget->CTX_SUFF(pThis), (VIRTIOSCSIREQ)pvIoReqAlloc, rcReq);
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
            if (!cTasksActive && pTarget->CTX_SUFF(pThis)->fSignalIdle)
                PDMDevHlpAsyncNotificationCompleted(pTarget->CTX_SUFF(pThis)->pDevInsR3);
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
    PVIRTIOSCSI pThis = pTarget->CTX_SUFF(pThis);

    if (pThis->pMediaNotify)
        virtioScsiSetWriteLed(pTarget, false);
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
static DECLCALLBACK(void *) virtioScsiR3TargetQueryInterface(PPDMIBASE pInterface, const char *pszIID)
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
static DECLCALLBACK(void *) virtioScsiR3DeviceQueryInterface(PPDMIBASE pInterface, const char *pszIID)
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

    PVIRTIOSCSI  pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSCSI);

    for (int qIdx = 0; qIdx < VIRTQ_MAX_CNT; qIdx++)
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

    pThis->IBase.pfnQueryInterface = virtioScsiR3DeviceQueryInterface;

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
                         virtioScsiR3DevCapRead,
                         virtioScsiR3DevCapWrite,
                         virtioScsiStatusChanged,
                         virtioScsiQueueNotified,
                         virtioScsiR3LiveExec,
                         virtioScsiR3SaveExec,
                         virtioScsiR3LoadExec,
                         virtioScsiR3LoadDone,
                         sizeof(VIRTIOSCSI_CONFIG_T) /* cbDevSpecificCap */,
                         (void *)&pThis->virtioScsiConfig /* pDevSpecificCap */);

    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("virtio-scsi: failed to initialize VirtIO"));


    RTStrCopy((char *)pThis->szQueueNames[CONTROLQ_IDX], VIRTIO_MAX_QUEUE_NAME_SIZE, "controlq");
    RTStrCopy((char *)pThis->szQueueNames[EVENTQ_IDX],   VIRTIO_MAX_QUEUE_NAME_SIZE, "eventq");
    for (uint16_t qIdx = VIRTQ_REQ_BASE; qIdx < VIRTQ_REQ_BASE + VIRTIOSCSI_REQ_QUEUE_CNT; qIdx++)
        RTStrPrintf((char *)pThis->szQueueNames[qIdx], VIRTIO_MAX_QUEUE_NAME_SIZE,
            "requestq_%d", qIdx - VIRTQ_REQ_BASE);

    /**
     * Create one worker per incoming-work-related queue (eventq is outgoing status to guest,
     * wherein guest is supposed to keep the queue loaded-up with buffer vectors the host
     * can quickly fill-in send back. Should be light-duty and fast enough to be handled on
     * requestq or controlq thread.  The Linux virtio_scsi driver limits the number of request
     * queues to MIN(<# Guest CPUs>, <Device's req queue max>), so queue count is ultimately
     * constrained from host side at negotiation time and initialization and later through
     * bounds-checking.
     */
    for (uint16_t qIdx = 0; qIdx < VIRTIOSCSI_QUEUE_CNT; qIdx++)
    {
        rc = virtioQueueAttach(pThis->hVirtio, qIdx, QUEUENAME(qIdx));
        AssertMsgReturn(rc == VINF_SUCCESS, ("Failed to attach queue %s\n", QUEUENAME(qIdx)), rc);
        pThis->fQueueAttached[qIdx] = (rc == VINF_SUCCESS);

        if (qIdx == CONTROLQ_IDX || IS_REQ_QUEUE(qIdx))
        {
            rc = PDMDevHlpThreadCreate(pDevIns, &pThis->aWorker[qIdx].pThread,
                                       (void *)(uint64_t)qIdx, virtioScsiWorkIncoming,
                                       virtioScsiR3WorkerWakeUp, 0, RTTHREADTYPE_IO, QUEUENAME(qIdx));
            if (rc != VINF_SUCCESS)
            {
                LogRel(("Error creating thread for Virtual Queue %s\n", QUEUENAME(qIdx)));
                return rc;
            }

            rc = SUPSemEventCreate(pThis->pSupDrvSession, &pThis->aWorker[qIdx].hEvtProcess);
            if (RT_FAILURE(rc))
                return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                     N_("LsiLogic: Failed to create SUP event semaphore"));
         }
    }

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

    /* Initialize per device instance. */
    for (RTUINT iLUN = 0; iLUN < VIRTIOSCSI_MAX_TARGETS; iLUN++)
    {
        PVIRTIOSCSITARGET pTarget = &pThis->aTargetInstances[iLUN];

        if (RTStrAPrintf(&pTarget->pszLunName, "VSCSILUN%u", iLUN) < 0)
            AssertLogRelFailedReturn(VERR_NO_MEMORY);

        /* Initialize static parts of the device. */
        pTarget->iLUN = iLUN;
        pTarget->pThisR3 = pThis;

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

        AssertReturn(iLUN < RT_ELEMENTS(pThis->aTargetInstances), VERR_PDM_NO_SUCH_LUN);
        rc = PDMDevHlpDriverAttach(pDevIns, iLUN, &pTarget->IBase, &pTarget->pDrvBase, (const char *)&pTarget->pszLunName);
        if (RT_SUCCESS(rc))
        {
            pTarget->fPresent = true;

            /* DrvSCSI.cpp currently implements the IMedia and IMediaEx interfaces, so those
             * are the interfaces that will be used to pass SCSI requests down to the
             * DrvSCSI driver to eventually make it down to the VD layer */
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
    virtioScsiR3PDMReset,
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
