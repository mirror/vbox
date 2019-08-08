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
#define VIRTIO_SAVEDSTATE_VERSION_3_1_BETA1 1
#define VIRTIO_SAVEDSTATE_VERSION           2
/** @} */

#define DEVICE_PCI_VENDOR_ID_VIRTIO         0x1AF4  /** Mandatory, so VirtIO driver can find this device */
#define DEVICE_PCI_REVISION_ID_VIRTIO       1       /** Virtio spec suggests >= 1 for non-transitional drivers */


/**
 * The following is the PCI capability struct common to all VirtIO capability types
 */
#define VIRTIO_PCI_CAP_ID_VENDOR       0x09 /** Vendor-specific PCI CFG Device Capability ID */

typedef struct virtio_pci_cap
{
    /* All little-endian */
    uint8_t   uCapVndr;                     /** Generic PCI field: PCI_CAP_ID_VNDR          */
    uint8_t   uCapNext;                     /** Generic PCI field: next ptr.                */
    uint8_t   uCapLen;                      /** Generic PCI field: capability length        */
    uint8_t   uCfgType;                     /** Identifies the structure.                   */
    uint8_t   uBar;                         /** Where to find it.                           */
    uint8_t   uPadding[3];                  /** Pad to full dword.                          */
    uint32_t  uOffset;                      /** Offset within bar.  (L.E.)                  */
    uint32_t  uLength;                      /** Length of struct, in bytes. (L.E.)          */
}  VIRTIO_PCI_CAP_T, *PVIRTIO_PCI_CAP_T;

/**
 * The following structs correspond to the VirtIO capability types and describe
 * fields accessed by MMIO via the relevant BAR. virtio_pci_dev_cap is not listed
 * here because it is device specific, thus the device specific implementation
 * consumer of this framework passes a ref/size in the constructor
 *
 */

/* Virtio Device PCI Capabilities type codes */
#define VIRTIO_PCI_CAP_COMMON_CFG      1    /** Common configuration PCI capability ID */
#define VIRTIO_PCI_CAP_NOTIFY_CFG      2    /** Notification area PCI capability ID */
#define VIRTIO_PCI_CAP_ISR_CFG         3    /** ISR PCI capability id */
#define VIRTIO_PCI_CAP_DEVICE_CFG      4    /** Device specific configuration PCI capability ID */
#define VIRTIO_PCI_CAP_PCI_CFG         5    /** PCI CFG capability ID */

typedef struct virtio_pci_common_cfg /* VirtIO 1.0 specification name of this struct  */
{
    /* Per device fields */
    uint32_t  uDeviceFeaturesSelect;        /** RW (driver selects device features)         */
    uint32_t  uDeviceFeatures;              /** RO (device reports features to driver)      */
    uint32_t  uDriverFeaturesSelect;        /** RW (driver selects driver features)         */
    uint32_t  uDriverFeatures;              /** RW (driver accepts device features)         */
    uint16_t  uMsixConfig;                  /** RW (driver sets MSI-X config vector)        */
    uint16_t  uNumQueues;                   /** RO (device specifies max queues)            */
    uint8_t   uDeviceStatus;                /** RW (driver writes device status, 0 resets)  */
    uint8_t   uConfigGeneration;            /** RO (device changes when changing configs)   */

    /* Per virtqueue fields (as determined by uQueueSelect) */
    uint16_t  uQueueSelect;                 /** RW (selects queue focus for these fields)   */
    uint16_t  uQueueSize;                   /** RW (queue size, 0 - 2^n)                    */
    uint16_t  uQueueMsixVector;             /** RW (driver selects MSI-X queue vector)      */
    uint16_t  uQueueEnable;                 /** RW (driver controls usability of queue)     */
    uint16_t  uQueueNotifyOff;              /** RO (offset uto virtqueue; see spec)         */
    uint64_t  uQueueDesc;                   /** RW (driver writes desc table phys addr)     */
    uint64_t  uQueueAvail;                  /** RW (driver writes avail ring phys addr)     */
    uint64_t  uQueueUsed;                   /** RW (driver writes used ring  phys addr)     */
} VIRTIO_PCI_COMMON_CFG_T, *PVIRTIO_PCI_COMMON_CFG_T;

typedef struct virtio_pci_notify_cap
{
    struct virtio_pci_cap pciCap;
    uint32_t uNotifyOffMultiplier;          /* notify_off_multiplier                       */
} VIRTIO_PCI_NOTIFY_CAP_T, *PVIRTIO_PCI_NOTIFY_CAP_T;

typedef struct virtio_pci_cfg_cap
{
    struct virtio_pci_cap pciCap;
    uint8_t uPciCfgData[4]; /* Data for BAR access. */
} VIRTIO_PCI_CFG_CAP_T, *PVIRTIO_PCI_CFG_CAP_T;

/**
 * The core (/common) state of the VirtIO PCI device
 *
 * @implements  PDMILEDPORTS
 */
typedef struct VIRTIOSTATE
{
    PDMPCIDEV                    dev;
    PDMCRITSECT                  cs;                                    /**< Critical section - what is it protecting? */
    /* Read-only part, never changes after initialization. */
    char                         szInstance[8];                         /**< Instance name, e.g. VNet#1. */

#if HC_ARCH_BITS != 64
    uint32_t                     padding1;
#endif

    /** Status LUN: Base interface. */
    PDMIBASE                     IBase;

    /** Status LUN: LED port interface. */
    PDMILEDPORTS                 ILed;

    /* Read/write part, protected with critical section. */
    /** Status LED. */
    PDMLED                       led;

    VIRTIOCALLBACKS              virtioCallbacks;

    /** Status LUN: LED connector (peer). */
    R3PTRTYPE(PPDMILEDCONNECTORS) pLedsConnector;

    PPDMDEVINSR3                 pDevInsR3;                           /**< Device instance - R3. */
    PPDMDEVINSR0                 pDevInsR0;                           /**< Device instance - R0. */
    PPDMDEVINSRC                 pDevInsRC;                           /**< Device instance - RC. */


    uint8_t                      uPciCfgDataOff;
#if HC_ARCH_BITS == 64
    uint32_t                     padding2;
#endif

    /** Base port of I/O space region. */
    RTIOPORT                     IOPortBase;

    uint32_t                     uGuestFeatures;
    uint16_t                     uQueueSelector;                      /** An index in aQueues array. */
    uint8_t                      uStatus;                             /** Device Status (bits are device-specific). */
    uint8_t                      uISR;                                /** Interrupt Status Register. */

#if HC_ARCH_BITS != 64
    uint32_t                     padding3;
#endif

    VQUEUE                       Queues[VIRTIO_MAX_QUEUES];
    uint32_t                     uNotifyOffMultiplier;                /* Multiplier for uQueueNotifyOff[idx] */

    /** Whole virtio device */
    uint32_t                     uDeviceFeaturesSelect;               /** Select hi/lo 32-bit uDeviceFeatures to r/w */
    uint64_t                     uDeviceFeatures;                     /** Host features offered */
    uint32_t                     uDriverFeaturesSelect;               /** Selects hi/lo 32-bit uDriverFeatures to r/w*/
    uint64_t                     uDriverFeatures;                     /** Host features accepted by guest */
    uint32_t                     uMsixConfig;
    uint32_t                     uNumQueues;                          /** Actual number of queues used. */
    uint8_t                      uDeviceStatus;
    uint8_t                      uConfigGeneration;

    /** Specific virtqueue */
    uint16_t                     uQueueSelect;
    uint16_t                     uQueueSize[VIRTIO_MAX_QUEUES];       /** Device sets on reset, driver can modify */
    uint16_t                     uQueueNotifyOff[VIRTIO_MAX_QUEUES];  /** Device sets this */
    uint16_t                     uQueueMsixVector[VIRTIO_MAX_QUEUES]; /** Driver specifies queue vector for MSI-X */
    uint16_t                     uQueueEnable[VIRTIO_MAX_QUEUES];     /** Driver controls device access to queue */
    uint64_t                     uQueueDesc[VIRTIO_MAX_QUEUES];       /** Driver provides this */
    uint64_t                     uQueueAvail[VIRTIO_MAX_QUEUES];      /** Driver provides this */
    uint64_t                     uQueueUsed[VIRTIO_MAX_QUEUES];       /** Driver provides this */


    uint8_t                      uVirtioCapBar;                       /* Capabilities BAR (region) assigned by client */

    /** Callbacks when guest driver reads or writes VirtIO device-specific capabilities(s) */
    PFNPCICONFIGREAD             pfnPciConfigReadOld;
    PFNPCICONFIGWRITE            pfnPciConfigWriteOld;

    uint32_t                     cbDevSpecificCap;                    /* Size of client's dev-specific config data  */
    void                        *pDevSpecificCap;                     /* Pointer to client's struct                 */
    void                        *pPrevDevSpecificCap;                 /* Previous read dev-specific cfg of client */
    bool                         fGenUpdatePending;                   /* If set, update cfg gen. after driver reads */

    PVIRTIO_PCI_CFG_CAP_T        pPciCfgCap;                          /** Pointer to struct in configuration area */
    PVIRTIO_PCI_NOTIFY_CAP_T     pNotifyCap;                          /** Pointer to struct in configuration area */
    PVIRTIO_PCI_CAP_T            pCommonCfgCap;                       /** Pointer to struct in configuration area */
    PVIRTIO_PCI_CAP_T            pIsrCap;                             /** Pointer to struct in configuration area */
    PVIRTIO_PCI_CAP_T            pDeviceCap;                          /** Pointer to struct in configuration area */

    /** Base address of PCI capabilities */
    RTGCPHYS                     GCPhysPciCapBase;
    RTGCPHYS                     pGcPhysCommonCfg;                    /** Pointer to MMIO mapped capability data */
    RTGCPHYS                     pGcPhysNotifyCap;                    /** Pointer to MMIO mapped capability data */
    RTGCPHYS                     pGcPhysIsrCap;                       /** Pointer to MMIO mapped capability data */
    RTGCPHYS                     pGcPhysDeviceCap;                    /** Pointer to MMIO mapped capability data */

    bool fDeviceConfigInterrupt;
    bool fQueueInterrupt;

} VIRTIOSTATE, *PVIRTIOSTATE;

DECLINLINE(uint16_t) vringReadAvail(PVIRTIOSTATE pVirtio, PVIRTQ pVirtQ)
{
    uint16_t dataWord;
    PDMDevHlpPhysRead(pVirtio->CTX_SUFF(pDevIns),
                      pVirtQ->pGcPhysVirtqAvail + RT_UOFFSETOF(VIRTQ_AVAIL_T, uIdx),
                      &dataWord, sizeof(dataWord));
    return dataWord;
}

DECLINLINE(bool) virtQueuePeek(PVIRTIOSTATE pVirtio, PVQUEUE pQueue, PVQUEUEELEM pElem)
{
    return virtQueueGet(pVirtio, pQueue, pElem, /* fRemove */ false);
}

DECLINLINE(bool) virtQueueIsReady(PVIRTIOSTATE pVirtio, PVQUEUE pQueue)
{
    NOREF(pVirtio);
    return !!pQueue->VirtQ.pGcPhysVirtqAvail;
}

DECLINLINE(bool) virtQueueIsEmpty(PVIRTIOSTATE pVirtio, PVQUEUE pQueue)
{
    return (vringReadAvail(pVirtio, &pQueue->VirtQ) == pQueue->uNextAvailIndex);
}
/**
* This macro returns true if physical address and access length are within the mapped capability struct.
*
* Actual Parameters:
*     @oaram    pPhysCapStruct - [input]  Pointer to MMIO mapped capability struct
*     @param    pCfgCap        - [input]  Pointer to capability in PCI configuration area
*     @param fMatched       - [output] True if GCPhysAddr is within the physically mapped capability.
*
* Implied parameters:
*     @param    GCPhysAddr     - [input, implied] Physical address accessed (via MMIO callback)
*     @param    cb             - [input, implied] Number of bytes to access
*
*/
#define MATCH_VIRTIO_CAP_STRUCT(pGcPhysCapData, pCfgCap, fMatched) \
        bool fMatched = false; \
        if (pGcPhysCapData && pCfgCap && GCPhysAddr >= (RTGCPHYS)pGcPhysCapData \
            && GCPhysAddr < ((RTGCPHYS)pGcPhysCapData + ((PVIRTIO_PCI_CAP_T)pCfgCap)->uLength) \
            && cb <= ((PVIRTIO_PCI_CAP_T)pCfgCap)->uLength) \
                fMatched = true;

/**
 * This macro resolves to boolean true if uOffset matches a field offset and size exactly,
 * (or if it is a 64-bit field, if it accesses either 32-bit part as a 32-bit access)
 * This is mandated by section 4.1.3.1 of the VirtIO 1.0 specification)
 *
 * @param   member   - Member of VIRTIO_PCI_COMMON_CFG_T
 * @param   uOffset  - Implied parameter: Offset into VIRTIO_PCI_COMMON_CFG_T
 * @param   cb       - Implied parameter: Number of bytes to access
 * @result           - true or false
 */
#define COMMON_CFG(member) \
        (RT_SIZEOFMEMB(VIRTIO_PCI_COMMON_CFG_T, member) == 64 \
         && (   uOffset == RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, member) \
             || uOffset == RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, member) + sizeof(uint32_t)) \
         && cb == sizeof(uint32_t)) \
     || (uOffset == RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, member) \
           && cb == RT_SIZEOFMEMB(VIRTIO_PCI_COMMON_CFG_T, member))

#define LOG_ACCESSOR(member) \
        virtioLogMappedIoValue(__FUNCTION__, #member, pv, cb, uIntraOff, fWrite, false, 0);

#define LOG_INDEXED_ACCESSOR(member, idx) \
        virtioLogMappedIoValue(__FUNCTION__, #member, pv, cb, uIntraOff, fWrite, true, idx);

#define ACCESSOR(member) \
    { \
        uint32_t uIntraOff = uOffset - RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, member); \
        if (fWrite) \
            memcpy(((char *)&pVirtio->member) + uOffset, (const char *)pv, cb); \
        else \
            memcpy((char *)pv, (const char *)(((char *)&pVirtio->member) + uOffset), cb); \
        LOG_ACCESSOR(member); \
    }

#define ACCESSOR_WITH_IDX(member, idx) \
    { \
        uint32_t uIntraOff = uOffset - RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, member); \
        if (fWrite) \
            memcpy(((char *)(pVirtio->member + idx)) + uOffset, (const char *)pv, cb); \
        else \
            memcpy((char *)pv, (const char *)(((char *)(pVirtio->member + idx)) + uOffset), cb); \
        LOG_INDEXED_ACCESSOR(member, idx); \
    }

#define ACCESSOR_READONLY(member) \
    { \
        uint32_t uIntraOff = uOffset - RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, member); \
        if (fWrite) \
            LogFunc(("Guest attempted to write readonly virtio_pci_common_cfg.%s\n", #member)); \
        else \
        { \
            memcpy((char *)pv, (const char *)(((char *)&pVirtio->member) + uOffset), cb); \
            LOG_ACCESSOR(member); \
        } \
    }

#define ACCESSOR_READONLY_WITH_IDX(member, idx) \
    { \
        uint32_t uIntraOff = uOffset - RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, member); \
        if (fWrite) \
            LogFunc(("Guest attempted to write readonly virtio_pci_common_cfg.%s[%d]\n", #member, idx)); \
        else \
        { \
            memcpy((char *)pv, ((char *)(pVirtio->member + idx)) + uOffset, cb); \
            LOG_INDEXED_ACCESSOR(member, idx); \
        } \
    }

#ifdef VBOX_DEVICE_STRUCT_TESTCASE
#  define virtioDumpState(x, s)  do {} while (0)
#else
#  ifdef DEBUG
        static void virtioDumpState(PVIRTIOSTATE pVirtio, const char *pcszCaller)
        {
            RT_NOREF2(pVirtio, pcszCaller);
            /* PK TODO, dump state features, selector, status, ISR, queue info (iterate),
                        descriptors, avail, used, size, indices, address
                        each by variable name on new line, indented slightly */
        }
#  endif
#endif

DECLINLINE(void) virtioLogDeviceStatus( uint8_t status)
{
    if (status == 0)
        Log(("RESET"));
    else
    {
        int primed = 0;
        if (status & VIRTIO_STATUS_ACKNOWLEDGE)
            Log(("ACKNOWLEDGE",   primed++));
        if (status & VIRTIO_STATUS_DRIVER)
            Log(("%sDRIVER",      primed++ ? " | " : ""));
        if (status & VIRTIO_STATUS_DRIVER_OK)
            Log(("%sDRIVER_OK",   primed++ ? " | " : ""));
        if (status & VIRTIO_STATUS_FEATURES_OK)
            Log(("%sFEATURES_OK", primed++ ? " | " : ""));
        if (status & VIRTIO_STATUS_FAILED)
            Log(("%sFAILED",      primed++ ? " | " : ""));
        if (status & VIRTIO_STATUS_DEVICE_NEEDS_RESET)
            Log(("%sACKNOWLEDGE", primed++ ? " | " : ""));
    }
}

/*  FROM Virtio 1.0 SPEC, NYI
      static inline int virtq_need_event(uint16_t event_idx, uint16_t new_idx, uint16_t old_idx)
            return (uint16_t)(new_idx - event_idx - 1) < (uint16_t)(new_idx - old_idx);
      }
      // Get location of event indices (only with VIRTIO_F_EVENT_IDX)
      static inline le16 *virtq_used_event(struct virtq *vq)
      {
              // For backwards compat, used event index is at *end* of avail ring.
              return &vq->avail->ring[vq->num];
}
      static inline le16 *virtq_avail_event(struct virtq *vq)
      {
              // For backwards compat, avail event index is at *end* of used ring.
              return (le16 *)&vq->used->ring[vq->num];
      }
}
*/
void    virtioRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta);
void   *virtioQueryInterface(struct PDMIBASE *pInterface, const char *pszIID);
int     virtioSaveExec(PVIRTIOSTATE pVirtio, PSSMHANDLE pSSM);
int     virtioLoadExec(PVIRTIOSTATE pVirtio, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass, uint32_t uNumQueues);

#endif /* !VBOX_INCLUDED_SRC_VirtIO_Virtio_1_0_impl_h */
