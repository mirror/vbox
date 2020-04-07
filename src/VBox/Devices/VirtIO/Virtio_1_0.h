/* $Id$ */
/** @file
 * Virtio_1_0.h - Virtio Declarations
 */

/*
 * Copyright (C) 2009-2020 Oracle Corporation
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

/** Pointer to the shared VirtIO state. */
typedef struct VIRTIOCORE *PVIRTIOCORE;
/** Pointer to the ring-3 VirtIO state. */
typedef struct VIRTIOCORER3 *PVIRTIOCORER3;
/** Pointer to the ring-0 VirtIO state. */
typedef struct VIRTIOCORER0 *PVIRTIOCORER0;
/** Pointer to the raw-mode VirtIO state. */
typedef struct VIRTIOCORERC *PVIRTIOCORERC;
/** Pointer to the instance data for the current context. */
typedef CTX_SUFF(PVIRTIOCORE) PVIRTIOCORECC;

typedef enum VIRTIOVMSTATECHANGED
{
    kvirtIoVmStateChangedInvalid = 0,
    kvirtIoVmStateChangedReset,
    kvirtIoVmStateChangedSuspend,
    kvirtIoVmStateChangedPowerOff,
    kvirtIoVmStateChangedResume,
    kvirtIoVmStateChangedFor32BitHack = 0x7fffffff
} VIRTIOVMSTATECHANGED;

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
            virtioCoreHexDump((pv), (cb), (base), (title)); \
    } while (0)
#else
# define VIRTIO_HEX_DUMP(logLevel, pv, cb, base, title) do { } while (0)
#endif

typedef struct VIRTIOSGSEG                                      /**< An S/G entry                              */
{
    RTGCPHYS gcPhys;                                            /**< Pointer to the segment buffer             */
    size_t  cbSeg;                                              /**< Size of the segment buffer                */
} VIRTIOSGSEG;

typedef VIRTIOSGSEG *PVIRTIOSGSEG;
typedef const VIRTIOSGSEG *PCVIRTIOSGSEG;
typedef PVIRTIOSGSEG *PPVIRTIOSGSEG;

typedef struct VIRTIOSGBUF
{
    PVIRTIOSGSEG paSegs;                                       /**< Pointer to the scatter/gather array       */
    unsigned  cSegs;                                            /**< Number of segments                        */
    unsigned  idxSeg;                                           /**< Current segment we are in                 */
    /** @todo r=bird: s/gcPhys/GCPhys/g as this is how we write it everywhere. */
    RTGCPHYS  gcPhysCur;                                        /**< Ptr to byte within the current seg        */
    size_t    cbSegLeft;                                        /**< # of bytes left in the current segment    */
} VIRTIOSGBUF;

typedef VIRTIOSGBUF *PVIRTIOSGBUF;
typedef const VIRTIOSGBUF *PCVIRTIOSGBUF;
typedef PVIRTIOSGBUF *PPVIRTIOSGBUF;

/**
 * Virtio descriptor chain representation.
 */
typedef struct VIRTIO_DESC_CHAIN
{
    uint32_t            u32Magic;                                   /**< Magic value, VIRTIO_DESC_CHAIN_MAGIC.    */
    uint32_t volatile   cRefs;                                      /**< Reference counter. */
    uint32_t            uHeadIdx;                                   /**< Head idx of associated desc chain        */
    uint32_t            cbPhysSend;                                 /**< Total size of src buffer                 */
    PVIRTIOSGBUF        pSgPhysSend;                                /**< Phys S/G/ buf for data from guest        */
    uint32_t            cbPhysReturn;                               /**< Total size of dst buffer                 */
    PVIRTIOSGBUF        pSgPhysReturn;                              /**< Phys S/G buf to store result for guest   */

    /** @name Internal (bird combined 5 allocations into a single), fingers off.
     * @{ */
    VIRTIOSGBUF         SgBufIn;
    VIRTIOSGBUF         SgBufOut;
    VIRTIOSGSEG         aSegsIn[VIRTQ_MAX_SIZE];
    VIRTIOSGSEG         aSegsOut[VIRTQ_MAX_SIZE];
    /** @} */
} VIRTIO_DESC_CHAIN_T;
/** Pointer to a Virtio descriptor chain. */
typedef VIRTIO_DESC_CHAIN_T *PVIRTIO_DESC_CHAIN_T;
/** Pointer to a Virtio descriptor chain pointer. */
typedef VIRTIO_DESC_CHAIN_T **PPVIRTIO_DESC_CHAIN_T;
/** Magic value for VIRTIO_DESC_CHAIN_T::u32Magic. */
#define VIRTIO_DESC_CHAIN_MAGIC             UINT32_C(0x19600219)

typedef struct VIRTIOPCIPARAMS
{
    uint16_t  uDeviceId;                                         /**< PCI Cfg Device ID                        */
    uint16_t  uClassBase;                                        /**< PCI Cfg Base Class                       */
    uint16_t  uClassSub;                                         /**< PCI Cfg Subclass                         */
    uint16_t  uClassProg;                                        /**< PCI Cfg Programming Interface Class      */
    uint16_t  uSubsystemId;                                      /**< PCI Cfg Card Manufacturer Vendor ID      */
    uint16_t  uInterruptLine;                                    /**< PCI Cfg Interrupt line                   */
    uint16_t  uInterruptPin;                                     /**< PCI Cfg Interrupt pin                    */
} VIRTIOPCIPARAMS, *PVIRTIOPCIPARAMS;

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
    char      szVirtqName[32];                                 /**< Dev-specific name of queue                */
    uint16_t  uAvailIdx;                                       /**< Consumer's position in avail ring         */
    uint16_t  uUsedIdx;                                        /**< Consumer's position in used ring          */
    bool      fEventThresholdReached;                          /**< Don't lose track while queueing ahead     */
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
 * PCI capability data locations (PCI CFG and MMIO).
 */
typedef struct VIRTIO_PCI_CAP_LOCATIONS_T
{
    uint16_t        offMmio;
    uint16_t        cbMmio;
    uint16_t        offPci;
    uint16_t        cbPci;
} VIRTIO_PCI_CAP_LOCATIONS_T;

/**
 * The core/common state of the VirtIO PCI devices, shared edition.
 */
typedef struct VIRTIOCORE
{
    char                        szInstance[16];                     /**< Instance name, e.g. "VIRTIOSCSI0"         */
    PPDMDEVINS                  pDevIns;                            /**< Client device instance                    */
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
    uint8_t                     uDeviceStatus;                      /**< (MMIO) Device Status                GUEST */
    uint8_t                     uPrevDeviceStatus;                  /**< (MMIO) Prev Device Status           GUEST */
    uint8_t                     uConfigGeneration;                  /**< (MMIO) Device config sequencer       HOST */

    VIRTQSTATE                  virtqState[VIRTQ_MAX_CNT];          /**< Local impl-specific queue context         */

    /** @name The locations of the capability structures in PCI config space and the BAR.
     * @{ */
    VIRTIO_PCI_CAP_LOCATIONS_T  LocPciCfgCap;                      /**< VIRTIO_PCI_CFG_CAP_T  */
    VIRTIO_PCI_CAP_LOCATIONS_T  LocNotifyCap;                      /**< VIRTIO_PCI_NOTIFY_CAP_T */
    VIRTIO_PCI_CAP_LOCATIONS_T  LocCommonCfgCap;                   /**< VIRTIO_PCI_CAP_T */
    VIRTIO_PCI_CAP_LOCATIONS_T  LocIsrCap;                         /**< VIRTIO_PCI_CAP_T */
    VIRTIO_PCI_CAP_LOCATIONS_T  LocDeviceCap;                      /**< VIRTIO_PCI_CAP_T + custom data.  */
    /** @} */

    bool                        fGenUpdatePending;                  /**< If set, update cfg gen after driver reads */
    uint8_t                     uPciCfgDataOff;
    uint8_t                     uISR;                               /**< Interrupt Status Register.                */
    uint8_t                     fMsiSupport;

    /** The MMIO handle for the PCI capability region (\#2). */
    IOMMMIOHANDLE               hMmioPciCap;

    /** @name Statistics
     * @{ */
    STAMCOUNTER                 StatDescChainsAllocated;
    STAMCOUNTER                 StatDescChainsFreed;
    STAMCOUNTER                 StatDescChainsSegsIn;
    STAMCOUNTER                 StatDescChainsSegsOut;
    /** @} */
} VIRTIOCORE;


/**
 * The core/common state of the VirtIO PCI devices, ring-3 edition.
 */
typedef struct VIRTIOCORER3
{
    /** @name Callbacks filled by the device before calling virtioCoreR3Init.
     * @{  */
    /**
     * Implementation-specific client callback to notify client of significant device status
     * changes.
     *
     * @param   pVirtio    Pointer to the shared virtio state.
     * @param   pVirtioCC  Pointer to the ring-3 virtio state.
     * @param   fDriverOk  True if guest driver is okay (thus queues, etc... are
     *                     valid)
     */
    DECLCALLBACKMEMBER(void, pfnStatusChanged)(PVIRTIOCORE pVirtio, PVIRTIOCORECC pVirtioCC, uint32_t fDriverOk);

    /**
     * When guest-to-host queue notifications are enabled, the guest driver notifies the host
     * that the avail queue has buffers, and this callback informs the client.
     *
     * @param   pVirtio    Pointer to the shared virtio state.
     * @param   pVirtioCC  Pointer to the ring-3 virtio state.
     * @param   idxQueue   Index of the notified queue
     */
    DECLCALLBACKMEMBER(void, pfnQueueNotified)(PVIRTIOCORE pVirtio, PVIRTIOCORECC pVirtioCC, uint16_t idxQueue);

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
    /** @} */

    R3PTRTYPE(PVIRTIO_PCI_CFG_CAP_T)    pPciCfgCap;                 /**< Pointer to struct in the PCI configuration area. */
    R3PTRTYPE(PVIRTIO_PCI_NOTIFY_CAP_T) pNotifyCap;                 /**< Pointer to struct in the PCI configuration area. */
    R3PTRTYPE(PVIRTIO_PCI_CAP_T)        pCommonCfgCap;              /**< Pointer to struct in the PCI configuration area. */
    R3PTRTYPE(PVIRTIO_PCI_CAP_T)        pIsrCap;                    /**< Pointer to struct in the PCI configuration area. */
    R3PTRTYPE(PVIRTIO_PCI_CAP_T)        pDeviceCap;                 /**< Pointer to struct in the PCI configuration area. */

    uint32_t                    cbDevSpecificCfg;                   /**< Size of client's dev-specific config data */
    R3PTRTYPE(uint8_t *)        pbDevSpecificCfg;                   /**< Pointer to client's struct                */
    R3PTRTYPE(uint8_t *)        pbPrevDevSpecificCfg;               /**< Previous read dev-specific cfg of client  */
    bool                        fGenUpdatePending;                  /**< If set, update cfg gen after driver reads */
} VIRTIOCORER3;


/**
 * The core/common state of the VirtIO PCI devices, ring-0 edition.
 */
typedef struct VIRTIOCORER0
{
    uint64_t                    uUnusedAtTheMoment;
} VIRTIOCORER0;


/**
 * The core/common state of the VirtIO PCI devices, raw-mode edition.
 */
typedef struct VIRTIOCORERC
{
    uint64_t                    uUnusedAtTheMoment;
} VIRTIOCORERC;


/** @typedef VIRTIOCORECC
 * The instance data for the current context. */
typedef CTX_SUFF(VIRTIOCORE) VIRTIOCORECC;


/** @name virtq related flags
 * @{ */
#define VIRTQ_DESC_F_NEXT                               1        /**< Indicates this descriptor chains to next  */
#define VIRTQ_DESC_F_WRITE                              2        /**< Marks buffer as write-only (default ro)   */
#define VIRTQ_DESC_F_INDIRECT                           4        /**< Buffer is list of buffer descriptors      */

#define VIRTQ_USED_F_NO_NOTIFY                          1        /**< Dev to Drv: Don't notify when buf added   */
#define VIRTQ_AVAIL_F_NO_INTERRUPT                      1        /**< Drv to Dev: Don't notify when buf eaten   */
/** @} */


/** @name API for VirtIO parent device
 * @{ */

int  virtioCoreR3QueueAttach(PVIRTIOCORE pVirtio, uint16_t idxQueue, const char *pcszName);

int  virtioCoreR3DescChainGet(PPDMDEVINS pDevIns, PVIRTIOCORE pVirtio, uint16_t idxQueue,
                             uint16_t uHeadIdx, PPVIRTIO_DESC_CHAIN_T ppDescChain);
uint32_t virtioCoreR3DescChainRetain(PVIRTIO_DESC_CHAIN_T pDescChain);
uint32_t virtioCoreR3DescChainRelease(PVIRTIOCORE pVirtio, PVIRTIO_DESC_CHAIN_T pDescChain);

int  virtioCoreR3QueuePeek(PPDMDEVINS pDevIns, PVIRTIOCORE pVirtio, uint16_t idxQueue,
                           PPVIRTIO_DESC_CHAIN_T ppDescChain);

int  virtioCoreR3QueueSkip(PVIRTIOCORE pVirtio, uint16_t idxQueue);

int  virtioCoreR3QueueGet(PPDMDEVINS pDevIns, PVIRTIOCORE pVirtio, uint16_t idxQueue,
                          PPVIRTIO_DESC_CHAIN_T ppDescChain, bool fRemove);

int  virtioCoreR3QueuePut(PPDMDEVINS pDevIns, PVIRTIOCORE pVirtio, uint16_t idxQueue, PRTSGBUF pSgVirtReturn,
                          PVIRTIO_DESC_CHAIN_T pDescChain, bool fFence);

int  virtioCoreR3QueuePendingCount(PPDMDEVINS pDevIns, PVIRTIOCORE pVirtio, uint16_t idxQueue);
int  virtioCoreQueueSync(PPDMDEVINS pDevIns, PVIRTIOCORE pVirtio, uint16_t idxQueue);
bool virtioCoreQueueIsEmpty(PPDMDEVINS pDevIns, PVIRTIOCORE pVirtio, uint16_t idxQueue);
void virtioCoreQueueEnable(PVIRTIOCORE pVirtio, uint16_t idxQueue, bool fEnabled);
void virtioCoreQueueSetNotify(PVIRTIOCORE pVirtio, uint16_t idxQueue, bool fEnabled);
void virtioCoreNotifyConfigChanged(PVIRTIOCORE pVirtio);
void virtioCoreResetAll(PVIRTIOCORE pVirtio);

/**
 * Return queue enable state
 *
 * @param   pVirtio     Pointer to the virtio state.
 * @param   idxQueue    Queue number.
 * @param   fEnabled    Flag indicating whether to enable queue or not
 */
DECLINLINE(bool) virtioCoreIsQueueEnabled(PVIRTIOCORE pVirtio, uint16_t idxQueue)
{
    Assert(idxQueue < RT_ELEMENTS(pVirtio->virtqState));
    return pVirtio->uQueueEnable[idxQueue] != 0;
}

/**
 * Get name of queue, by idxQueue, assigned at virtioCoreR3QueueAttach()
 *
 * @param   pVirtio     Pointer to the virtio state.
 * @param   idxQueue    Queue number.
 *
 * @returns Pointer to read-only queue name.
 */
DECLINLINE(const char *) virtioCoreQueueGetName(PVIRTIOCORE pVirtio, uint16_t idxQueue)
{
    Assert((size_t)idxQueue < RT_ELEMENTS(pVirtio->virtqState));
    return pVirtio->virtqState[idxQueue].szVirtqName;
}

/**
 * Get the features VirtIO is running withnow.
 *
 * @returns Features the guest driver has accepted, finalizing the operational features
 */
DECLINLINE(uint64_t) virtioCoreGetNegotiatedFeatures(PVIRTIOCORE pVirtio)
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
DECLINLINE(uint64_t) virtioCoreGetAcceptedFeatures(PVIRTIOCORE pVirtio)
{
    return pVirtio->uDriverFeatures;
}

/**
 * Calculate the length of a GCPhys s/g buffer by tallying the size of each segment.
 *
 * @param   pGcSgBuf        GC S/G buffer to calculate length of
 */
DECLINLINE(size_t) virtioCoreSgBufCalcTotalLength(PCVIRTIOSGBUF pGcSgBuf)
{
    size_t   cb = 0;
    unsigned i  = pGcSgBuf->cSegs;
    while (i-- > 0)
        cb += pGcSgBuf->paSegs[i].cbSeg;
    return cb;
}

void virtioCoreLogMappedIoValue(const char *pszFunc, const char *pszMember, uint32_t uMemberSize,
                                const void *pv, uint32_t cb, uint32_t uOffset,
                                int fWrite, int fHasIndex, uint32_t idx);

void virtioCoreHexDump(uint8_t *pv, uint32_t cb, uint32_t uBase, const char *pszTitle);
void virtioCoreGcPhysHexDump(PPDMDEVINS pDevIns, RTGCPHYS gcPhys, uint32_t cb, uint32_t uBase, const char *pszTitle);

void     virtioCoreSgBufInit(PVIRTIOSGBUF pGcSgBuf, PVIRTIOSGSEG paSegs, size_t cSegs);
void     virtioCoreSgBufReset(PVIRTIOSGBUF pGcSgBuf);
RTGCPHYS virtioCoreSgBufGetNextSegment(PVIRTIOSGBUF pGcSgBuf, size_t *pcbSeg);
RTGCPHYS virtioCoreSgBufAdvance(PVIRTIOSGBUF pGcSgBuf, size_t cbAdvance);
void     virtioCoreSgBufInit(PVIRTIOSGBUF pSgBuf, PVIRTIOSGSEG paSegs, size_t cSegs);
size_t   virtioCoreSgBufCalcTotalLength(PCVIRTIOSGBUF pGcSgBuf);
void     virtioCoreSgBufReset(PVIRTIOSGBUF pGcSgBuf);
int      virtioCoreR3SaveExec(PVIRTIOCORE pVirtio, PCPDMDEVHLPR3 pHlp, PSSMHANDLE pSSM);
int      virtioCoreR3LoadExec(PVIRTIOCORE pVirtio, PCPDMDEVHLPR3 pHlp, PSSMHANDLE pSSM);
void     virtioCoreR3VmStateChanged(PVIRTIOCORE pVirtio, VIRTIOVMSTATECHANGED enmState);
void     virtioCoreR3Term(PPDMDEVINS pDevIns, PVIRTIOCORE pVirtio, PVIRTIOCORECC pVirtioCC);
int      virtioCoreR3Init(PPDMDEVINS pDevIns, PVIRTIOCORE pVirtio, PVIRTIOCORECC pVirtioCC, PVIRTIOPCIPARAMS pPciParams,
                      const char *pcszInstance, uint64_t fDevSpecificFeatures, void *pvDevSpecificCfg, uint16_t cbDevSpecificCfg);
int      virtioCoreRZInit(PPDMDEVINS pDevIns, PVIRTIOCORE pVirtio, PVIRTIOCORECC pVirtioCC);
const char *virtioCoreGetStateChangeText(VIRTIOVMSTATECHANGED enmState);

/** @} */


#endif /* !VBOX_INCLUDED_SRC_VirtIO_Virtio_1_0_h */
