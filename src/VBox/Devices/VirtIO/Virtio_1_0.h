/* $Id$ */
/** @file
 * Virtio_1_0.h - Virtio Declarations
 */

/*
 * Copyright (C) 2009-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_INCLUDED_SRC_VirtIO_Virtio_1_0_h
#define VBOX_INCLUDED_SRC_VirtIO_Virtio_1_0_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/ctype.h>
#include <iprt/sg.h>

/** Pointer to the virt i/o state. */
typedef struct VIRTIOSTATE *PVIRTIOSTATE;

/**
 * Important sizing and bounds params for this impl. of VirtIO 1.0 PCI device
 */
 /**
  * TEMPORARY NOTE: Some of these values are experimental during development and will likely change.
  */
#define VIRTIO_MAX_QUEUE_NAME_SIZE          32                   /**< Maximum length of a queue name           */
#define VIRTQ_MAX_SIZE                      1024                 /**< Max size (# desc elements) of a virtq    */
#define VIRTQ_MAX_CNT                       24                   /**< Max queues we allow guest to create      */
#define VIRTIO_NOTIFY_OFFSET_MULTIPLIER     2                    /**< VirtIO Notify Cap. MMIO config param     */
#define VIRTIO_REGION_PCI_CAP               2                    /**< BAR for VirtIO Cap. MMIO (impl specific) */
#define VIRTIO_REGION_MSIX_CAP              0                    /**< Bar for MSI-X handling                   */

#ifdef LOG_ENABLED
# define VIRTIO_HEX_DUMP(logLevel, pv, cb, base, title) \
    do { \
        if (LogIsItEnabled(logLevel, LOG_GROUP)) \
            virtioHexDump((pv), (cb), (base), (title)); \
    } while (0)
#else
# define VIRTIO_HEX_DUMP(logLevel, pv, cb, base, title) do { } while (0)
#endif


/**
 * The following structure holds the pre-processed context of descriptor chain pulled from a virtio queue
 * to conduct a transaction between the client of this virtio implementation and the guest VM's virtio driver.
 * It contains the head index of the descriptor chain, the output data from the client that has been
 * converted to a contiguous virtual memory and a physical memory scatter-gather buffer for use by by
 * the virtio framework to complete the transaction in the final phase of round-trip processing.
 *
 * The client should not modify the contents of this buffer. The primary field of interest to the
 * client is pVirtSrc, which contains the VirtIO "OUT" (to device) buffer from the guest.
 *
 * Typical use is, When the client (worker thread) detects available data on the queue, it pulls the
 * next one of these descriptor chain structs off the queue using virtioR3QueueGet(), processes the
 * virtual memory buffer pVirtSrc, produces result data to pass back to the guest driver and calls
 * virtioR3QueuePut() to return the result data to the client.
 */
typedef struct VIRTIO_DESC_CHAIN
{
    uint32_t  uHeadIdx;                                          /**< Head idx of associated desc chain        */
    uint32_t  cbPhysSend;                                        /**< Total size of src buffer                 */
    PRTSGBUF  pSgPhysSend;                                       /**< Phys S/G/ buf for data from guest        */
    uint32_t  cbPhysReturn;                                      /**< Total size of dst buffer                 */
    PRTSGBUF  pSgPhysReturn;                                     /**< Phys S/G buf to store result for guest   */
} VIRTIO_DESC_CHAIN_T, *PVIRTIO_DESC_CHAIN_T, **PPVIRTIO_DESC_CHAIN_T;

/**
 * The following structure is used to pass the PCI parameters from the consumer
 * to this generic VirtIO framework. This framework provides the Vendor ID as Virtio.
 */
typedef struct VIRTIOPCIPARAMS
{
    uint16_t  uDeviceId;                                         /**< PCI Cfg Device ID                        */
    uint16_t  uClassBase;                                        /**< PCI Cfg Base Class                       */
    uint16_t  uClassSub;                                         /**< PCI Cfg Subclass                         */
    uint16_t  uClassProg;                                        /**< PCI Cfg Programming Interface Class      */
    uint16_t  uSubsystemId;                                      /**< PCI Cfg Card Manufacturer Vendor ID      */
    uint16_t  uSubsystemVendorId;                                /**< PCI Cfg Chipset Manufacturer Vendor ID   */
    uint16_t  uRevisionId;                                       /**< PCI Cfg Revision ID                      */
    uint16_t  uInterruptLine;                                    /**< PCI Cfg Interrupt line                   */
    uint16_t  uInterruptPin;                                     /**< PCI Cfg Interrupt pin                    */
} VIRTIOPCIPARAMS, *PVIRTIOPCIPARAMS;

/** @name VirtIO port I/O callbacks.
 * @{ */
typedef struct VIRTIOCALLBACKS
{
     /**
      * Implementation-specific client callback to notify client of significant device status
      * changes.
      *
      * @param   pVirtio    Pointer to virtio state.
      * @param   fDriverOk  True if guest driver is okay (thus queues, etc... are
      *                     valid)
      */
     DECLCALLBACKMEMBER(void, pfnStatusChanged)(PVIRTIOSTATE pVirtio, uint32_t fDriverOk);

     /**
      * When guest-to-host queue notifications are enabled, the guest driver notifies the host
      * that the avail queue has buffers, and this callback informs the client.
      *
      * @param   pVirtio    Pointer to virtio state.
      * @param   idxQueue   Index of the notified queue
      */
     DECLCALLBACKMEMBER(void, pfnQueueNotified)(PVIRTIOSTATE pVirtio, uint16_t idxQueue);

     /**
      * Implementation-specific client callback to access VirtIO Device-specific capabilities
      * (other VirtIO capabilities and features are handled in VirtIO implementation)
      *
      * @param   pDevIns    The device instance.
      * @param   offCap     Offset within device specific capabilities struct.
      * @param   pvBuf      Buffer in which to save read data.
      * @param   cbToRead   Number of bytes to read.
      */
     DECLCALLBACKMEMBER(int,  pfnDevCapRead)(PPDMDEVINS pDevIns, uint32_t offCap, void *pvBuf, uint32_t cbToRead);

     /**
      * Implementation-specific client ballback to access VirtIO Device-specific capabilities
      * (other VirtIO capabilities and features are handled in VirtIO implementation)
      *
      * @param   pDevIns    The device instance.
      * @param   offCap     Offset within device specific capabilities struct.
      * @param   pvBuf      Buffer with the bytes to write.
      * @param   cbToWrite  Number of bytes to write.
      */
     DECLCALLBACKMEMBER(int,  pfnDevCapWrite)(PPDMDEVINS pDevIns, uint32_t offCap, const void *pvBuf, uint32_t cbWrite);
} VIRTIOCALLBACKS;
/** @} */

#define VIRTIO_F_VERSION_1                  RT_BIT_64(32)        /**< Required feature bit for 1.0 devices      */

#define VIRTIO_F_INDIRECT_DESC              RT_BIT_64(28)        /**< Allow descs to point to list of descs     */
#define VIRTIO_F_EVENT_IDX                  RT_BIT_64(29)        /**< Allow notification disable for n elems    */
#define VIRTIO_F_RING_INDIRECT_DESC         RT_BIT_64(28)        /**< Doc bug: Goes under two names in spec     */
#define VIRTIO_F_RING_EVENT_IDX             RT_BIT_64(29)        /**< Doc bug: Goes under two names in spec     */

#define VIRTIO_DEV_INDEPENDENT_FEATURES_OFFERED ( 0 )            /**< TBD: Add VIRTIO_F_INDIRECT_DESC     */

#define VIRTIO_ISR_VIRTQ_INTERRUPT           RT_BIT_32(0)        /**< Virtq interrupt bit of ISR register       */
#define VIRTIO_ISR_DEVICE_CONFIG             RT_BIT_32(1)        /**< Device configuration changed bit of ISR   */
#define DEVICE_PCI_VENDOR_ID_VIRTIO                0x1AF4        /**< Guest driver locates dev via (mandatory)  */
#define DEVICE_PCI_REVISION_ID_VIRTIO                   1        /**< VirtIO 1.0 non-transitional drivers >= 1  */

/** Reserved (*negotiated*) Feature Bits (e.g. device independent features, VirtIO 1.0 spec,section 6) */

#define VIRTIO_MSI_NO_VECTOR                       0xffff        /**< Vector value to disable MSI for queue     */

/** Device Status field constants (from Virtio 1.0 spec) */
#define VIRTIO_STATUS_ACKNOWLEDGE                    0x01        /**< Guest driver: Located this VirtIO device  */
#define VIRTIO_STATUS_DRIVER                         0x02        /**< Guest driver: Can drive this VirtIO dev.  */
#define VIRTIO_STATUS_DRIVER_OK                      0x04        /**< Guest driver: Driver set-up and ready     */
#define VIRTIO_STATUS_FEATURES_OK                    0x08        /**< Guest driver: Feature negotiation done    */
#define VIRTIO_STATUS_FAILED                         0x80        /**< Guest driver: Fatal error, gave up        */
#define VIRTIO_STATUS_DEVICE_NEEDS_RESET             0x40        /**< Device experienced unrecoverable error    */

/** @def Virtio Device PCI Capabilities type codes */
#define VIRTIO_PCI_CAP_COMMON_CFG                       1        /**< Common configuration PCI capability ID    */
#define VIRTIO_PCI_CAP_NOTIFY_CFG                       2        /**< Notification area PCI capability ID       */
#define VIRTIO_PCI_CAP_ISR_CFG                          3        /**< ISR PCI capability id                     */
#define VIRTIO_PCI_CAP_DEVICE_CFG                       4        /**< Device-specific PCI cfg capability ID     */
#define VIRTIO_PCI_CAP_PCI_CFG                          5        /**< PCI CFG capability ID                     */

#define VIRTIO_PCI_CAP_ID_VENDOR                     0x09        /**< Vendor-specific PCI CFG Device Cap. ID    */

/**
 * The following is the PCI capability struct common to all VirtIO capability types
 */
typedef struct virtio_pci_cap
{
    /* All little-endian */
    uint8_t   uCapVndr;                                          /**< Generic PCI field: PCI_CAP_ID_VNDR        */
    uint8_t   uCapNext;                                          /**< Generic PCI field: next ptr.              */
    uint8_t   uCapLen;                                           /**< Generic PCI field: capability length      */
    uint8_t   uCfgType;                                          /**< Identifies the structure.                 */
    uint8_t   uBar;                                              /**< Where to find it.                         */
    uint8_t   uPadding[3];                                       /**< Pad to full dword.                        */
    uint32_t  uOffset;                                           /**< Offset within bar.  (L.E.)                */
    uint32_t  uLength;                                           /**< Length of struct, in bytes. (L.E.)        */
}  VIRTIO_PCI_CAP_T, *PVIRTIO_PCI_CAP_T;

/**
 * Local implementation's usage context of a queue (e.g. not part of VirtIO specification)
 */
typedef struct VIRTQSTATE
{
    char        szVirtqName[32];                                 /**< Dev-specific name of queue                */
    uint16_t    uAvailIdx;                                       /**< Consumer's position in avail ring         */
    uint16_t    uUsedIdx;                                        /**< Consumer's position in used ring          */
    bool        fEventThresholdReached;                          /**< Don't lose track while queueing ahead     */
} VIRTQSTATE, *PVIRTQSTATE;

/**
 * VirtIO 1.0 Capabilities' related MMIO-mapped structs:
 *
 * Note: virtio_pci_device_cap is dev-specific, implemented by client. Definition unknown here.
 */
typedef struct virtio_pci_common_cfg
{
    /* Per device fields */
    uint32_t  uDeviceFeaturesSelect;                             /**< RW (driver selects device features)       */
    uint32_t  uDeviceFeatures;                                   /**< RO (device reports features to driver)    */
    uint32_t  uDriverFeaturesSelect;                             /**< RW (driver selects driver features)       */
    uint32_t  uDriverFeatures;                                   /**< RW (driver-accepted device features)      */
    uint16_t  uMsixConfig;                                       /**< RW (driver sets MSI-X config vector)      */
    uint16_t  uNumQueues;                                        /**< RO (device specifies max queues)          */
    uint8_t   uDeviceStatus;                                     /**< RW (driver writes device status, 0=reset) */
    uint8_t   uConfigGeneration;                                 /**< RO (device changes when changing configs) */

    /* Per virtqueue fields (as determined by uQueueSelect) */
    uint16_t  uQueueSelect;                                      /**< RW (selects queue focus for these fields) */
    uint16_t  uQueueSize;                                        /**< RW (queue size, 0 - 2^n)                  */
    uint16_t  uQueueMsixVector;                                  /**< RW (driver selects MSI-X queue vector)    */
    uint16_t  uQueueEnable;                                      /**< RW (driver controls usability of queue)   */
    uint16_t  uQueueNotifyOff;                                   /**< RO (offset uto virtqueue; see spec)       */
    uint64_t  aGCPhysQueueDesc;                                  /**< RW (driver writes desc table phys addr)   */
    uint64_t  aGCPhysQueueAvail;                                 /**< RW (driver writes avail ring phys addr)   */
    uint64_t  aGCPhysQueueUsed;                                  /**< RW (driver writes used ring  phys addr)   */
} VIRTIO_PCI_COMMON_CFG_T, *PVIRTIO_PCI_COMMON_CFG_T;

typedef struct virtio_pci_notify_cap
{
    struct virtio_pci_cap pciCap;                                /**< Notification MMIO mapping capability      */
    uint32_t uNotifyOffMultiplier;                               /**< notify_off_multiplier                     */
} VIRTIO_PCI_NOTIFY_CAP_T, *PVIRTIO_PCI_NOTIFY_CAP_T;

typedef struct virtio_pci_cfg_cap
{
    struct virtio_pci_cap pciCap;                                /**< Cap. defines the BAR/off/len to access    */
    uint8_t uPciCfgData[4];                                      /**< I/O buf for above cap.                    */
} VIRTIO_PCI_CFG_CAP_T, *PVIRTIO_PCI_CFG_CAP_T;

/**
 * The core (/common) state of the VirtIO PCI device
 */
typedef struct VIRTIOSTATE
{
    char                        szInstance[16];                     /**< Instance name, e.g. "VIRTIOSCSI0"         */
    PPDMDEVINSR3                pDevInsR3;                          /**< Device instance - R3                      */

    RTGCPHYS                    GCPhysPciCapBase;                   /**< Pointer to MMIO mapped capability data    */
    RTGCPHYS                    GCPhysCommonCfg;                    /**< Pointer to MMIO mapped capability data    */
    RTGCPHYS                    GCPhysNotifyCap;                    /**< Pointer to MMIO mapped capability data    */
    RTGCPHYS                    GCPhysIsrCap;                       /**< Pointer to MMIO mapped capability data    */
    RTGCPHYS                    GCPhysDeviceCap;                    /**< Pointer to MMIO mapped capability data    */

    RTGCPHYS                    aGCPhysQueueDesc[VIRTQ_MAX_CNT];    /**< (MMIO) PhysAdr per-Q desc structs   GUEST */
    RTGCPHYS                    aGCPhysQueueAvail[VIRTQ_MAX_CNT];   /**< (MMIO) PhysAdr per-Q avail structs  GUEST */
    RTGCPHYS                    aGCPhysQueueUsed[VIRTQ_MAX_CNT];    /**< (MMIO) PhysAdr per-Q used structs   GUEST */
    uint16_t                    uQueueNotifyOff[VIRTQ_MAX_CNT];     /**< (MMIO) per-Q notify offset           HOST */
    uint16_t                    uQueueMsixVector[VIRTQ_MAX_CNT];    /**< (MMIO) Per-queue vector for MSI-X   GUEST */
    uint16_t                    uQueueEnable[VIRTQ_MAX_CNT];        /**< (MMIO) Per-queue enable             GUEST */
    uint16_t                    uQueueSize[VIRTQ_MAX_CNT];          /**< (MMIO) Per-queue size          HOST/GUEST */
    uint16_t                    uQueueSelect;                       /**< (MMIO) queue selector               GUEST */
    uint16_t                    padding;
    uint64_t                    uDeviceFeatures;                    /**< (MMIO) Host features offered         HOST */
    uint64_t                    uDriverFeatures;                    /**< (MMIO) Host features accepted       GUEST */
    uint32_t                    uDeviceFeaturesSelect;              /**< (MMIO) hi/lo select uDeviceFeatures GUEST */
    uint32_t                    uDriverFeaturesSelect;              /**< (MMIO) hi/lo select uDriverFeatures GUEST */
    uint32_t                    uMsixConfig;                        /**< (MMIO) MSI-X vector                 GUEST */
    uint32_t                    uNumQueues;                         /**< (MMIO) Actual number of queues      GUEST
                                                                     * @todo r=bird: This value is always VIRTQ_MAX_CNT
                                                                     * and only used in for loops.  Guest always see
                                                                     * VIRTQ_MAX_CNT regardless of this value, so
                                                                     * what's the point of having this? */
    uint8_t                     uDeviceStatus;                      /**< (MMIO) Device Status                GUEST */
    uint8_t                     uPrevDeviceStatus;                  /**< (MMIO) Prev Device Status           GUEST */
    uint8_t                     uConfigGeneration;                  /**< (MMIO) Device config sequencer       HOST */

    VIRTQSTATE                  virtqState[VIRTQ_MAX_CNT];          /**< Local impl-specific queue context         */
    VIRTIOCALLBACKS             Callbacks;                          /**< Callback vectors to client                */

    PVIRTIO_PCI_CFG_CAP_T       pPciCfgCap;                         /**< Pointer to struct in configuration area   */
    PVIRTIO_PCI_NOTIFY_CAP_T    pNotifyCap;                         /**< Pointer to struct in configuration area   */
    PVIRTIO_PCI_CAP_T           pCommonCfgCap;                      /**< Pointer to struct in configuration area   */
    PVIRTIO_PCI_CAP_T           pIsrCap;                            /**< Pointer to struct in configuration area   */
    PVIRTIO_PCI_CAP_T           pDeviceCap;                         /**< Pointer to struct in configuration area   */

    uint32_t                    cbDevSpecificCfg;                   /**< Size of client's dev-specific config data */
    void                       *pvDevSpecificCfg;                   /**< Pointer to client's struct                */
    void                       *pvPrevDevSpecificCfg;               /**< Previous read dev-specific cfg of client  */
    bool                        fGenUpdatePending;                  /**< If set, update cfg gen after driver reads */
    uint8_t                     uPciCfgDataOff;
    uint8_t                     uISR;                               /**< Interrupt Status Register.                */
    uint8_t                     fMsiSupport;

} VIRTIOSTATE;


/** virtq related flags */
#define VIRTQ_DESC_F_NEXT                               1        /**< Indicates this descriptor chains to next  */
#define VIRTQ_DESC_F_WRITE                              2        /**< Marks buffer as write-only (default ro)   */
#define VIRTQ_DESC_F_INDIRECT                           4        /**< Buffer is list of buffer descriptors      */

#define VIRTQ_USED_F_NO_NOTIFY                          1        /**< Dev to Drv: Don't notify when buf added   */
#define VIRTQ_AVAIL_F_NO_INTERRUPT                      1        /**< Drv to Dev: Don't notify when buf eaten   */


/** @name API for VirtIO client
 * @{ */

int virtioR3QueueAttach(PVIRTIOSTATE pVirtio, uint16_t idxQueue, const char *pcszName);
#if 0 /* no such function */
/**
 * Detaches from queue and release resources
 *
 * @param hVirtio   Handle for VirtIO framework
 * @param idxQueue      Queue number
 */
int virtioQueueDetach(PVIRTIOSTATE pVirtio, uint16_t idxQueue);
#endif
int  virtioR3QueueGet(PVIRTIOSTATE pVirtio, uint16_t idxQueue, PPVIRTIO_DESC_CHAIN_T ppDescChain, bool fRemove);
int  virtioR3QueuePut(PVIRTIOSTATE pVirtio, uint16_t idxQueue, PRTSGBUF pSgVirtReturn,
                      PVIRTIO_DESC_CHAIN_T pDescChain, bool fFence);

int virtioQueueSync(PVIRTIOSTATE pVirtio, uint16_t idxQueue);
bool virtioQueueIsEmpty(PVIRTIOSTATE pVirtio, uint16_t idxQueue);

void virtioQueueEnable(PVIRTIOSTATE pVirtio, uint16_t idxQueue, bool fEnabled);
void virtioResetAll(PVIRTIOSTATE pVirtio);

/**
 * Return queue enable state
 *
 * @param   pVirtio     Pointer to the virtio state.
 * @param   idxQueue    Queue number.
 * @param   fEnabled    Flag indicating whether to enable queue or not
 */
DECLINLINE(bool) virtioIsQueueEnabled(PVIRTIOSTATE pVirtio, uint16_t idxQueue)
{
    Assert(idxQueue < RT_ELEMENTS(pVirtio->virtqState));
    return pVirtio->uQueueEnable[idxQueue] != 0;
}


/**
 * Get name of queue, by idxQueue, assigned at virtioR3QueueAttach()
 *
 * @param   pVirtio     Pointer to the virtio state.
 * @param   idxQueue    Queue number.
 *
 * @returns Pointer to read-only queue name.
 */
DECLINLINE(const char *) virtioQueueGetName(PVIRTIOSTATE pVirtio, uint16_t idxQueue)
{
    Assert((size_t)idxQueue < RT_ELEMENTS(pVirtio->virtqState));
    return pVirtio->virtqState[idxQueue].szVirtqName;
}

/**
 * Get the features VirtIO is running withnow.
 *
 * @returns Features the guest driver has accepted, finalizing the operational features
 */
DECLINLINE(uint64_t) virtioGetNegotiatedFeatures(PVIRTIOSTATE pVirtio)
{
    return pVirtio->uDriverFeatures;
}

/**
 * Get VirtIO accepted host-side features
 *
 * @returns feature bits selected or 0 if selector out of range.
 *
 * @param   pState          Virtio state
 */
DECLINLINE(uint64_t) virtioGetAcceptedFeatures(PVIRTIOSTATE pVirtio)
{
    return pVirtio->uDriverFeatures;
}


int  virtioR3SaveExec(PVIRTIOSTATE pVirtio, PCPDMDEVHLPR3 pHlp, PSSMHANDLE pSSM);
int  virtioR3LoadExec(PVIRTIOSTATE pVirtio, PCPDMDEVHLPR3 pHlp, PSSMHANDLE pSSM);
void virtioR3PropagateResetNotification(PVIRTIOSTATE pVirtio);
void virtioR3PropagateResumeNotification(PVIRTIOSTATE pVirtio);
void virtioR3Term(PVIRTIOSTATE pVirtio, PPDMDEVINS pDevIns);
int  virtioR3Init(PVIRTIOSTATE pVirtio, PPDMDEVINS pDevIns, PVIRTIOPCIPARAMS pPciParams, const char *pcszInstance,
                  uint64_t fDevSpecificFeatures, void *pvDevSpecificCfg, uint16_t cbDevSpecificCfg);

void virtioLogMappedIoValue(const char *pszFunc, const char *pszMember, uint32_t uMemberSize,
                            const void *pv, uint32_t cb, uint32_t uOffset,
                            int fWrite, int fHasIndex, uint32_t idx);
void virtioHexDump(uint8_t *pv, uint32_t cb, uint32_t uBase, const char *pszTitle);
/** @} */


#endif /* !VBOX_INCLUDED_SRC_VirtIO_Virtio_1_0_h */
