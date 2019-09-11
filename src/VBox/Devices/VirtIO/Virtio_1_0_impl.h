/* $Id$ $Revision$ $Date$ $Author$ */
/** @file
 * Virtio_1_0_impl.h - Virtio Declarations
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

#ifndef VBOX_INCLUDED_SRC_VirtIO_Virtio_1_0_impl_h
#define VBOX_INCLUDED_SRC_VirtIO_Virtio_1_0_impl_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "Virtio_1_0.h"

/** @name Saved state versions.
 * The saved state version is changed if either common or any of specific
 * parts are changed. That is, it is perfectly possible that the version
 * of saved vnet state will increase as a result of change in vblk structure
 * for example.
 */
#define VIRTIO_SAVEDSTATE_VERSION                       1
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
 * IN/OUT Descriptor chains descriptor chain associated with one element of virtq avail ring represented
 * as respective arrays of SG segments.
 */
typedef struct VIRTQ_DESC_CHAIN                                  /**< Describes a single queue element          */
{
    RTSGSEG     aSegsIn[VIRTQ_MAX_SIZE];                         /**< List of segments to write to guest        */
    RTSGSEG     aSegsOut[VIRTQ_MAX_SIZE];                        /**< List of segments read from guest          */
    uint32_t    uHeadIdx;                                        /**< Index at head desc (source of seg arrays) */
    uint32_t    cSegsIn;                                         /**< Count of segments in aSegsIn[]            */
    uint32_t    cSegsOut;                                        /**< Count of segments in aSegsOut[]           */
} VIRTQ_DESC_CHAIN_T, *PVIRTQ_DESC_CHAIN_T;

/**
 * Local implementation's usage context of a queue (e.g. not part of VirtIO specification)
 */
typedef struct VIRTQ_PROXY
{
    RTSGBUF     inSgBuf;                                         /**< host-to-guest buffers                     */
    RTSGBUF     outSgBuf;                                        /**< guest-to-host buffers                     */
    const char  szVirtqName[32];                                 /**< Dev-specific name of queue                */
    uint16_t    uAvailIdx;                                       /**< Consumer's position in avail ring         */
    uint16_t    uUsedIdx;                                        /**< Consumer's position in used ring          */
    bool        fEventThresholdReached;                          /**< Don't lose track while queueing ahead     */
    PVIRTQ_DESC_CHAIN_T pDescChain;                              /**< Per-queue s/g data.                       */
} VIRTQ_PROXY_T, *PVIRTQ_PROXY_T;

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
    uint64_t  pGcPhysQueueDesc;                                  /**< RW (driver writes desc table phys addr)   */
    uint64_t  pGcPhysQueueAvail;                                 /**< RW (driver writes avail ring phys addr)   */
    uint64_t  pGcPhysQueueUsed;                                  /**< RW (driver writes used ring  phys addr)   */
} VIRTIO_PCI_COMMON_CFG_T, *PVIRTIO_PCI_COMMON_CFG_T;

typedef struct virtio_pci_notify_cap
{
    struct virtio_pci_cap pciCap;                                /**< Notification MMIO mapping capability      */
    uint32_t uNotifyOffMultiplier;                               /**< notify_off_multiplier                     */
}  VIRTIO_PCI_NOTIFY_CAP_T,   *PVIRTIO_PCI_NOTIFY_CAP_T;

typedef struct virtio_pci_cfg_cap
{
    struct virtio_pci_cap pciCap;                                /**< Cap. defines the BAR/off/len to access    */
    uint8_t uPciCfgData[4];                                      /**< I/O buf for above cap.                    */
} VIRTIO_PCI_CFG_CAP_T,   *PVIRTIO_PCI_CFG_CAP_T;

/**
 * The core (/common) state of the VirtIO PCI device
 *
 * @implements  PDMILEDPORTS
 */
typedef struct VIRTIOSTATE
{
    PDMPCIDEV                 dev;                               /**< PCI device                                */
    char                      szInstance[16];                    /**< Instance name, e.g. "VIRTIOSCSI0"         */
    void *                    pClientContext;                    /**< Client callback returned on callbacks     */

    PPDMDEVINSR3              pDevInsR3;                         /**< Device instance - R3                      */
    PPDMDEVINSR0              pDevInsR0;                         /**< Device instance - R0                      */
    PPDMDEVINSRC              pDevInsRC;                         /**< Device instance - RC                      */

    RTGCPHYS                  pGcPhysPciCapBase;                 /**< Pointer to MMIO mapped capability data    */
    RTGCPHYS                  pGcPhysCommonCfg;                  /**< Pointer to MMIO mapped capability data    */
    RTGCPHYS                  pGcPhysNotifyCap;                  /**< Pointer to MMIO mapped capability data    */
    RTGCPHYS                  pGcPhysIsrCap;                     /**< Pointer to MMIO mapped capability data    */
    RTGCPHYS                  pGcPhysDeviceCap;                  /**< Pointer to MMIO mapped capability data    */

    RTGCPHYS                  pGcPhysQueueDesc[VIRTQ_MAX_CNT];   /**< (MMIO) PhysAdr per-Q desc structs   GUEST */
    RTGCPHYS                  pGcPhysQueueAvail[VIRTQ_MAX_CNT];  /**< (MMIO) PhysAdr per-Q avail structs  GUEST */
    RTGCPHYS                  pGcPhysQueueUsed[VIRTQ_MAX_CNT];   /**< (MMIO) PhysAdr per-Q used structs   GUEST */
    uint16_t                  uQueueNotifyOff[VIRTQ_MAX_CNT];    /**< (MMIO) per-Q notify offset           HOST */
    uint16_t                  uQueueMsixVector[VIRTQ_MAX_CNT];   /**< (MMIO) Per-queue vector for MSI-X   GUEST */
    uint16_t                  uQueueEnable[VIRTQ_MAX_CNT];       /**< (MMIO) Per-queue enable             GUEST */
    uint16_t                  uQueueSize[VIRTQ_MAX_CNT];         /**< (MMIO) Per-queue size          HOST/GUEST */
    uint16_t                  uQueueSelect;                      /**< (MMIO) queue selector               GUEST */
    uint16_t                  padding;
    uint64_t                  uDeviceFeatures;                   /**< (MMIO) Host features offered         HOST */
    uint64_t                  uDriverFeatures;                   /**< (MMIO) Host features accepted       GUEST */
    uint32_t                  uDeviceFeaturesSelect;             /**< (MMIO) hi/lo select uDeviceFeatures GUEST */
    uint32_t                  uDriverFeaturesSelect;             /**< (MMIO) hi/lo select uDriverFeatures GUEST */
    uint32_t                  uMsixConfig;                       /**< (MMIO) MSI-X vector                 GUEST */
    uint32_t                  uNumQueues;                        /**< (MMIO) Actual number of queues      GUEST */
    uint8_t                   uDeviceStatus;                     /**< (MMIO) Device Status                GUEST */
    uint8_t                   uPrevDeviceStatus;                 /**< (MMIO) Prev Device Status           GUEST */
    uint8_t                   uConfigGeneration;                 /**< (MMIO) Device config sequencer       HOST */

    VIRTQ_PROXY_T             virtqProxy[VIRTQ_MAX_CNT];         /**< Local impl-specific queue context         */
    VIRTIOCALLBACKS           virtioCallbacks;                   /**< Callback vectors to client                */

    PFNPCICONFIGREAD          pfnPciConfigReadOld;               /**< Prev rd. cb. intercepting PCI Cfg I/O     */
    PFNPCICONFIGWRITE         pfnPciConfigWriteOld;              /**< Prev wr. cb. intercepting PCI Cfg I/O     */

    PVIRTIO_PCI_CFG_CAP_T     pPciCfgCap;                        /**< Pointer to struct in configuration area   */
    PVIRTIO_PCI_NOTIFY_CAP_T  pNotifyCap;                        /**< Pointer to struct in configuration area   */
    PVIRTIO_PCI_CAP_T         pCommonCfgCap;                     /**< Pointer to struct in configuration area   */
    PVIRTIO_PCI_CAP_T         pIsrCap;                           /**< Pointer to struct in configuration area   */
    PVIRTIO_PCI_CAP_T         pDeviceCap;                        /**< Pointer to struct in configuration area   */

    uint32_t                  cbDevSpecificCfg;                  /**< Size of client's dev-specific config data */
    void                     *pDevSpecificCfg;                   /**< Pointer to client's struct                */
    void                     *pPrevDevSpecificCfg;               /**< Previous read dev-specific cfg of client  */
    bool                      fGenUpdatePending;                 /**< If set, update cfg gen after driver reads */
    uint8_t                   uPciCfgDataOff;
    uint8_t                   uISR;                              /**< Interrupt Status Register.                */

} VIRTIOSTATE, *PVIRTIOSTATE;

/** virtq related flags */
#define VIRTQ_DESC_F_NEXT                               1        /**< Indicates this descriptor chains to next  */
#define VIRTQ_DESC_F_WRITE                              2        /**< Marks buffer as write-only (default ro)   */
#define VIRTQ_DESC_F_INDIRECT                           4        /**< Buffer is list of buffer descriptors      */

#define VIRTQ_USED_F_NO_NOTIFY                          1        /**< Dev to Drv: Don't notify when buf added   */
#define VIRTQ_AVAIL_F_NO_INTERRUPT                      1        /**< Drv to Dev: Don't notify when buf eaten   */

/**
 * virtq related structs
 * (struct names follow VirtIO 1.0 spec, typedef use VBox style)
 */
typedef struct virtq_desc
{
    uint64_t  pGcPhysBuf;                                        /**< addr       GC Phys. address of buffer     */
    uint32_t  cb;                                                /**< len        Buffer length                  */
    uint16_t  fFlags;                                            /**< flags      Buffer specific flags          */
    uint16_t  uDescIdxNext;                                      /**< next       Idx set if VIRTIO_DESC_F_NEXT  */
} VIRTQ_DESC_T, *PVIRTQ_DESC_T;

typedef struct virtq_avail
{
    uint16_t  fFlags;                                            /**< flags      avail ring drv to dev flags    */
    uint16_t  uIdx;                                              /**< idx        Index of next free ring slot   */
    uint16_t  auRing[1];                                         /**< ring       Ring: avail drv to dev bufs    */
    uint16_t  uUsedEventIdx;                                     /**< used_event (if VIRTQ_USED_F_EVENT_IDX)    */
} VIRTQ_AVAIL_T, *PVIRTQ_AVAIL_T;

typedef struct virtq_used_elem
{
    uint32_t  uDescIdx;                                          /**< idx         Start of used desc chain      */
    uint32_t  cbElem;                                            /**< len         Total len of used desc chain  */
} VIRTQ_USED_ELEM_T;

typedef struct virt_used
{
    uint16_t  fFlags;                                            /**< flags       used ring host-to-guest flags */
    uint16_t  uIdx;                                              /**< idx         Index of next ring slot       */
    VIRTQ_USED_ELEM_T auRing[1];                                 /**< ring        Ring: used dev to drv bufs    */
    uint16_t  uAvailEventIdx;                                    /**< avail_event if (VIRTQ_USED_F_EVENT_IDX)   */
} VIRTQ_USED_T, *PVIRTQ_USED_T;

/**
* This macro returns true if the implied parameter GCPhysAddr address and access length are
* within mapped capability struct specified with the explicit parameters.
*
* Actual Parameters:
*     @oaram    pPhysCapStruct - [input]  Pointer to MMIO mapped capability struct
*     @param    pCfgCap        - [input]  Pointer to capability in PCI configuration area
*     @param    fMatched       - [output] True if GCPhysAddr is within the physically mapped capability.
*
* Implied parameters:
*     @param    GCPhysAddr     - [input, implied] Physical address accessed (via MMIO callback)
*     @param    cb             - [input, implied] Number of bytes to access
*/
#define MATCH_VIRTIO_CAP_STRUCT(pGcPhysCapData, pCfgCap, fMatched) \
        bool fMatched = false; \
        if (pGcPhysCapData && pCfgCap && GCPhysAddr >= (RTGCPHYS)pGcPhysCapData \
            && GCPhysAddr < ((RTGCPHYS)pGcPhysCapData + ((PVIRTIO_PCI_CAP_T)pCfgCap)->uLength) \
            && cb <= ((PVIRTIO_PCI_CAP_T)pCfgCap)->uLength) \
                fMatched = true;

/**
 * This macro resolves to boolean true if the implied parameters, uOffset and cb, match the field
 * offset and size of a field in the Common Cfg struct, (or if it is a 64-bit field, if it accesses
 * either 32-bit part as a 32-bit access)
 * This is mandated by section 4.1.3.1 of the VirtIO 1.0 specification)
 *
 * @param   member   - Member of VIRTIO_PCI_COMMON_CFG_T
 * @param   uOffset  - Implied parameter: Offset into VIRTIO_PCI_COMMON_CFG_T
 * @param   cb       - Implied parameter: Number of bytes to access
 * @result           - true or false
 */
#define MATCH_COMMON_CFG(member) \
        (RT_SIZEOFMEMB(VIRTIO_PCI_COMMON_CFG_T, member) == 8 \
         && (   uOffset == RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, member) \
             || uOffset == RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, member) + sizeof(uint32_t)) \
         && cb == sizeof(uint32_t)) \
     || (uOffset == RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, member) \
           && cb == RT_SIZEOFMEMB(VIRTIO_PCI_COMMON_CFG_T, member))

#define LOG_COMMON_CFG_ACCESS(member) \
        virtioLogMappedIoValue(__FUNCTION__, #member, RT_SIZEOFMEMB(VIRTIO_PCI_COMMON_CFG_T, member), \
                                pv, cb, uIntraOff, fWrite, false, 0);

#define LOG_COMMON_CFG_ACCESS_INDEXED(member, idx) \
        virtioLogMappedIoValue(__FUNCTION__, #member, RT_SIZEOFMEMB(VIRTIO_PCI_COMMON_CFG_T, member), \
                               pv, cb, uIntraOff, fWrite, true, idx);

#define COMMON_CFG_ACCESSOR(member) \
    do \
    { \
        uint32_t uIntraOff = uOffset - RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, member); \
        if (fWrite) \
            memcpy(((char *)&pVirtio->member) + uIntraOff, (const char *)pv, cb); \
        else \
            memcpy((char *)pv, (const char *)(((char *)&pVirtio->member) + uIntraOff), cb); \
        LOG_COMMON_CFG_ACCESS(member); \
    } while(0)

#define COMMON_CFG_ACCESSOR_INDEXED(member, idx) \
    do \
    { \
        uint32_t uIntraOff = uOffset - RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, member); \
        if (fWrite) \
            memcpy(((char *)(pVirtio->member + idx)) + uIntraOff, (const char *)pv, cb); \
        else \
            memcpy((char *)pv, (const char *)(((char *)(pVirtio->member + idx)) + uIntraOff), cb); \
        LOG_COMMON_CFG_ACCESS_INDEXED(member, idx); \
    } while(0)

#define COMMON_CFG_ACCESSOR_READONLY(member) \
    do \
    { \
        uint32_t uIntraOff = uOffset - RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, member); \
        if (fWrite) \
            LogFunc(("Guest attempted to write readonly virtio_pci_common_cfg.%s\n", #member)); \
        else \
        { \
            memcpy((char *)pv, (const char *)(((char *)&pVirtio->member) + uIntraOff), cb); \
            LOG_COMMON_CFG_ACCESS(member); \
        } \
    } while(0)

#define COMMON_CFG_ACCESSOR_INDEXED_READONLY(member, idx) \
    do \
    { \
        uint32_t uIntraOff = uOffset - RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, member); \
        if (fWrite) \
            LogFunc(("Guest attempted to write readonly virtio_pci_common_cfg.%s[%d]\n", #member, idx)); \
        else \
        { \
            memcpy((char *)pv, ((char *)(pVirtio->member + idx)) + uIntraOff, cb); \
            LOG_COMMON_CFG_ACCESS_INDEXED(member, idx); \
        } \
    } while(0)

#define DRIVER_OK(pVirtio) (pVirtio->uDeviceStatus & VIRTIO_STATUS_DRIVER_OK)

/**
 * Internal queue operations
 */

static int         virtqIsEventNeeded         (uint16_t uEventIdx, uint16_t uDescIdxNew, uint16_t uDescIdxOld);
static bool        virtqIsEmpty               (PVIRTIOSTATE pVirtio, uint16_t qIdx);
static void        virtioReadDesc             (PVIRTIOSTATE pVirtio, uint16_t qIdx, uint32_t uDescIdx, PVIRTQ_DESC_T pDesc);
static uint16_t    virtioReadAvailDescIdx     (PVIRTIOSTATE pVirtio, uint16_t qIdx, uint32_t availIdx);
static uint16_t    virtioReadAvailRingIdx     (PVIRTIOSTATE pVirtio, uint16_t qIdx);
static uint16_t    virtioReadAvailFlags       (PVIRTIOSTATE pVirtio, uint16_t qIdx);
static uint16_t    virtioReadAvailUsedEvent   (PVIRTIOSTATE pVirtio, uint16_t qIdx);
static void        virtioWriteUsedElem        (PVIRTIOSTATE pVirtio, uint16_t qIdx, uint32_t usedIdx, uint32_t uDescIdx, uint32_t uLen);
static void        virtioWriteUsedRingIdx     (PVIRTIOSTATE pVirtio, uint16_t qIdx, uint16_t uDescIdx);
static uint16_t    virtioReadUsedRingIdx      (PVIRTIOSTATE pVirtio, uint16_t qIdx);
static uint16_t    virtioReadUsedFlags        (PVIRTIOSTATE pVirtio, uint16_t qIdx);
static void        virtioWriteUsedFlags       (PVIRTIOSTATE pVirtio, uint16_t qIdx, uint32_t fFlags);
static uint16_t    virtioReadUsedAvailEvent   (PVIRTIOSTATE pVirtio, uint16_t qIdx);
static void        virtioWriteUsedAvailEvent  (PVIRTIOSTATE pVirtio, uint16_t qIdx, uint32_t uAvailEventIdx);


DECLINLINE(int) virtqIsEventNeeded(uint16_t uEventIdx, uint16_t uDescIdxNew, uint16_t uDescIdxOld)
{
    return (uint16_t)(uDescIdxNew - uEventIdx - 1) < (uint16_t)(uDescIdxNew - uDescIdxOld);
}

DECLINLINE(bool) virtqIsEmpty(PVIRTIOSTATE pVirtio, uint16_t qIdx)
{
    return virtioReadAvailRingIdx(pVirtio, qIdx) == pVirtio->virtqProxy[qIdx].uAvailIdx;
}

/**
 * Accessor for virtq descriptor
 */
DECLINLINE(void) virtioReadDesc(PVIRTIOSTATE pVirtio, uint16_t qIdx, uint32_t uDescIdx, PVIRTQ_DESC_T pDesc)
{
    //Log(("%s virtioQueueReadDesc: ring=%p idx=%u\n", INSTANCE(pState), pVirtQ, idx));
    AssertMsg(pVirtio->uDeviceStatus & VIRTIO_STATUS_DRIVER_OK, ("Called with guest driver not ready\n"));
    PDMDevHlpPhysRead(pVirtio->CTX_SUFF(pDevIns),
                      pVirtio->pGcPhysQueueDesc[qIdx]
                    + sizeof(VIRTQ_DESC_T) * (uDescIdx % pVirtio->uQueueSize[qIdx]),
                      pDesc, sizeof(VIRTQ_DESC_T));
}

/**
 * Accessors for virtq avail ring
 */
DECLINLINE(uint16_t) virtioReadAvailDescIdx(PVIRTIOSTATE pVirtio, uint16_t qIdx, uint32_t availIdx)
{
    uint16_t uDescIdx;
    AssertMsg(pVirtio->uDeviceStatus & VIRTIO_STATUS_DRIVER_OK, ("Called with guest driver not ready\n"));
    PDMDevHlpPhysRead(pVirtio->CTX_SUFF(pDevIns),
                      pVirtio->pGcPhysQueueAvail[qIdx]
                    + RT_UOFFSETOF_DYN(VIRTQ_AVAIL_T, auRing[availIdx % pVirtio->uQueueSize[qIdx]]),
                      &uDescIdx, sizeof(uDescIdx));
    return uDescIdx;
}

DECLINLINE(uint16_t) virtioReadAvailRingIdx(PVIRTIOSTATE pVirtio, uint16_t qIdx)
{
    uint16_t uIdx = 0;
    AssertMsg(pVirtio->uDeviceStatus & VIRTIO_STATUS_DRIVER_OK, ("Called with guest driver not ready\n"));
    PDMDevHlpPhysRead(pVirtio->CTX_SUFF(pDevIns),
                      pVirtio->pGcPhysQueueAvail[qIdx] + RT_UOFFSETOF(VIRTQ_AVAIL_T, uIdx),
                      &uIdx, sizeof(uIdx));
    return uIdx;
}

DECLINLINE(uint16_t) virtioReadAvailFlags(PVIRTIOSTATE pVirtio, uint16_t qIdx)
{
    uint16_t fFlags;
    AssertMsg(pVirtio->uDeviceStatus & VIRTIO_STATUS_DRIVER_OK, ("Called with guest driver not ready\n"));
    PDMDevHlpPhysRead(pVirtio->CTX_SUFF(pDevIns),
                      pVirtio->pGcPhysQueueAvail[qIdx] + RT_UOFFSETOF(VIRTQ_AVAIL_T, fFlags),
                      &fFlags, sizeof(fFlags));
    return fFlags;
}

DECLINLINE(uint16_t) virtioReadAvailUsedEvent(PVIRTIOSTATE pVirtio, uint16_t qIdx)
{
    uint16_t uUsedEventIdx;
    /** VirtIO 1.0 uUsedEventIdx (used_event) immediately follows ring */
    AssertMsg(pVirtio->uDeviceStatus & VIRTIO_STATUS_DRIVER_OK, ("Called with guest driver not ready\n"));
    PDMDevHlpPhysRead(pVirtio->CTX_SUFF(pDevIns),
                      pVirtio->pGcPhysQueueAvail[qIdx]
                    + RT_UOFFSETOF_DYN(VIRTQ_AVAIL_T, auRing[pVirtio->uQueueSize[qIdx]]),
                      &uUsedEventIdx, sizeof(uUsedEventIdx));
    return uUsedEventIdx;
}

/**
 * Accessors for virtq used ring
 */
DECLINLINE(void) virtioWriteUsedElem(PVIRTIOSTATE pVirtio, uint16_t qIdx, uint32_t usedIdx, uint32_t uDescIdx, uint32_t uLen)
{
    VIRTQ_USED_ELEM_T elem = { uDescIdx,  uLen };
    AssertMsg(pVirtio->uDeviceStatus & VIRTIO_STATUS_DRIVER_OK, ("Called with guest driver not ready\n"));
    PDMDevHlpPCIPhysWrite(pVirtio->CTX_SUFF(pDevIns),
                          pVirtio->pGcPhysQueueUsed[qIdx]
                        + RT_UOFFSETOF_DYN(VIRTQ_USED_T, auRing[usedIdx % pVirtio->uQueueSize[qIdx]]),
                          &elem, sizeof(elem));
}

DECLINLINE(void) virtioWriteUsedRingIdx(PVIRTIOSTATE pVirtio, uint16_t qIdx, uint16_t uIdx)
{
    AssertMsg(pVirtio->uDeviceStatus & VIRTIO_STATUS_DRIVER_OK, ("Called with guest driver not ready\n"));
    PDMDevHlpPCIPhysWrite(pVirtio->CTX_SUFF(pDevIns),
                          pVirtio->pGcPhysQueueUsed[qIdx] + RT_UOFFSETOF(VIRTQ_USED_T, uIdx),
                          &uIdx, sizeof(uIdx));
}

DECLINLINE(uint16_t)virtioReadUsedRingIdx(PVIRTIOSTATE pVirtio, uint16_t qIdx)
{
    uint16_t uIdx;
    AssertMsg(pVirtio->uDeviceStatus & VIRTIO_STATUS_DRIVER_OK, ("Called with guest driver not ready\n"));
    PDMDevHlpPhysRead(pVirtio->CTX_SUFF(pDevIns),
                      pVirtio->pGcPhysQueueUsed[qIdx] + RT_UOFFSETOF(VIRTQ_USED_T, uIdx),
                      &uIdx, sizeof(uIdx));
    return uIdx;
}

DECLINLINE(uint16_t) virtioReadUsedFlags(PVIRTIOSTATE pVirtio, uint16_t qIdx)
{
    uint16_t fFlags;
    AssertMsg(pVirtio->uDeviceStatus & VIRTIO_STATUS_DRIVER_OK, ("Called with guest driver not ready\n"));
    PDMDevHlpPhysRead(pVirtio->CTX_SUFF(pDevIns),
                      pVirtio->pGcPhysQueueUsed[qIdx] + RT_UOFFSETOF(VIRTQ_USED_T, fFlags),
                      &fFlags, sizeof(fFlags));
    return fFlags;
}

DECLINLINE(void) virtioWriteUsedFlags(PVIRTIOSTATE pVirtio, uint16_t qIdx, uint32_t fFlags)
{
    AssertMsg(pVirtio->uDeviceStatus & VIRTIO_STATUS_DRIVER_OK, ("Called with guest driver not ready\n"));
    PDMDevHlpPCIPhysWrite(pVirtio->CTX_SUFF(pDevIns),
                          pVirtio->pGcPhysQueueUsed[qIdx] + RT_UOFFSETOF(VIRTQ_USED_T, fFlags),
                          &fFlags, sizeof(fFlags));
}

DECLINLINE(uint16_t) virtioReadUsedAvailEvent(PVIRTIOSTATE pVirtio, uint16_t qIdx)
{
    uint16_t uAvailEventIdx;
    /** VirtIO 1.0 uAvailEventIdx (avail_event) immediately follows ring */
    AssertMsg(pVirtio->uDeviceStatus & VIRTIO_STATUS_DRIVER_OK, ("Called with guest driver not ready\n"));
    PDMDevHlpPhysRead(pVirtio->CTX_SUFF(pDevIns),
                      pVirtio->pGcPhysQueueUsed[qIdx]
                    + RT_UOFFSETOF_DYN(VIRTQ_USED_T, auRing[pVirtio->uQueueSize[qIdx]]),
                      &uAvailEventIdx, sizeof(uAvailEventIdx));
    return uAvailEventIdx;
}

DECLINLINE(void) virtioWriteUsedAvailEvent(PVIRTIOSTATE pVirtio, uint16_t qIdx, uint32_t uAvailEventIdx)
{
    /** VirtIO 1.0 uAvailEventIdx (avail_event) immediately follows ring */
    AssertMsg(pVirtio->uDeviceStatus & VIRTIO_STATUS_DRIVER_OK, ("Called with guest driver not ready\n"));
    PDMDevHlpPCIPhysWrite(pVirtio->CTX_SUFF(pDevIns),
                          pVirtio->pGcPhysQueueUsed[qIdx]
                        + RT_UOFFSETOF_DYN(VIRTQ_USED_T, auRing[pVirtio->uQueueSize[qIdx]]),
                          &uAvailEventIdx, sizeof(uAvailEventIdx));
}


/**
 * Makes the MMIO-mapped Virtio uDeviceStatus registers non-cryptic */
DECLINLINE(void) virtioLogDeviceStatus( uint8_t status)
{
    if (status == 0)
        Log6(("RESET"));
    else
    {
        int primed = 0;
        if (status & VIRTIO_STATUS_ACKNOWLEDGE)
            Log6(("ACKNOWLEDGE",   primed++));
        if (status & VIRTIO_STATUS_DRIVER)
            Log6(("%sDRIVER",      primed++ ? " | " : ""));
        if (status & VIRTIO_STATUS_FEATURES_OK)
            Log6(("%sFEATURES_OK", primed++ ? " | " : ""));
        if (status & VIRTIO_STATUS_DRIVER_OK)
            Log6(("%sDRIVER_OK",   primed++ ? " | " : ""));
        if (status & VIRTIO_STATUS_FAILED)
            Log6(("%sFAILED",      primed++ ? " | " : ""));
        if (status & VIRTIO_STATUS_DEVICE_NEEDS_RESET)
            Log6(("%sNEEDS_RESET", primed++ ? " | " : ""));
    }
}

static void virtioResetQueue        (PVIRTIOSTATE pVirtio, uint16_t qIdx);
static void virtioNotifyGuestDriver (PVIRTIOSTATE pVirtio, uint16_t qIdx, bool fForce);
static int  virtioRaiseInterrupt    (PVIRTIOSTATE pVirtio, uint8_t uCause, bool fForce);
static void virtioLowerInterrupt    (PVIRTIOSTATE pVirtio);
static void virtioQueueNotified     (PVIRTIOSTATE pVirtio, uint16_t qidx, uint16_t uDescIdx);
static int  virtioCommonCfgAccessed (PVIRTIOSTATE pVirtio, int fWrite, off_t uOffset, unsigned cb, void const *pv);
static void virtioGuestResetted     (PVIRTIOSTATE pVirtio);

static DECLCALLBACK(int) virtioR3SaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM);
static DECLCALLBACK(int) virtioR3LoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass);
static DECLCALLBACK(int) virtioR3LoadDone(PPDMDEVINS pDevIns, PSSMHANDLE pSSM);
static DECLCALLBACK(int) virtioR3LiveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uPass);

#endif /* !VBOX_INCLUDED_SRC_VirtIO_Virtio_1_0_impl_h */
