/* $Id$ */
/** @file
 * Virtio_1_0p .h - Virtio Declarations
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

/** @name Saved state versions.
 * The saved state version is changed if either common or any of specific
 * parts are changed. That is, it is perfectly possible that the version
 * of saved vnet state will increase as a result of change in vblk structure
 * for example.
 */
#define VIRTIO_SAVEDSTATE_VERSION_3_1_BETA1 1
#define VIRTIO_SAVEDSTATE_VERSION           2
/** @} */

/** Mandatory for Virtio PCI device identification, per spec */
#define DEVICE_PCI_VENDOR_ID_VIRTIO         0x1AF4

/** Virtio spec suggests >= 1 for non-transitional drivers */
#define DEVICE_PCI_REVISION_ID_VIRTIO       1

/* Virtio Device PCI Capabilities type codes */
#define VIRTIO_PCI_CAP_COMMON_CFG           1
#define VIRTIO_PCI_CAP_NOTIFY_CFG           2
#define VIRTIO_PCI_CAP_ISR_CFG              3
#define VIRTIO_PCI_CAP_DEVICE_CFG           4
#define VIRTIO_PCI_CAP_PCI_CFG              5

/** Device Status field constants (from Virtio 1.0 spec) */
#define VIRTIO_STATUS_ACKNOWLEDGE           0x01    /** Guest found this emulated PCI device     */
#define VIRTIO_STATUS_DRIVER                0x02    /** Guest can drive this PCI device          */
#define VIRTIO_STATUS_DRIVER_OK             0x04    /** Guest VirtIO driver setup and ready      */
#define VIRTIO_STATUS_FEATURES_OK           0x08    /** Guest says feature negotiation complete  */
#define VIRTIO_STATUS_FAILED                0x80    /** Fatal: Something wrong, guest gave up    */
#define VIRTIO_STATUS_DEVICE_NEEDS_RESET    0x40    /** Device experienced unrecoverable error   */


#define VIRTIO_MAX_NQUEUES                  256     /** Max queues we allow guest to create      */
#define VIRTQUEUEDESC_MAX_SIZE              (2 * 1024 * 1024)   /** Legacy only? */
#define VIRTQUEUE_MAX_SIZE                  1024    /** According to? Legacy only? */
#define VIRTIO_ISR_QUEUE                    0x1     /* Legacy only? Now derived from feature negotiation? */
#define VIRTIO_ISR_CONFIG                   0x3     /* Legacy only? Now derived from feature negotiation? */

#define VIRTIO_PCI_CAP_ID_VENDOR            0x09

/* Vector value used to disable MSI for queue */
#define VIRTIO_MSI_NO_VECTOR                0xffff

typedef DECLCALLBACK(uint32_t) FNGETHOSTFEATURES(void *pvState);
typedef FNGETHOSTFEATURES *PFNGETHOSTFEATURES;

/** @name VirtIO port I/O callbacks.
 * @{ */
typedef struct VIRTIOIOCALLBACKS
{
     DECLCALLBACKMEMBER(uint32_t, pfnGetHostFeatures)(void *pvState);
     DECLCALLBACKMEMBER(uint32_t, pfnGetHostMinimalFeatures)(void *pvState);
     DECLCALLBACKMEMBER(void,     pfnSetHostFeatures)(void *pvState, uint32_t fFeatures);
     DECLCALLBACKMEMBER(int,      pfnGetConfig)(void *pvState, uint32_t offCfg, uint32_t cb, void *pvData);
     DECLCALLBACKMEMBER(int,      pfnSetConfig)(void *pvState, uint32_t offCfg, uint32_t cb, void *pvData);
     DECLCALLBACKMEMBER(int,      pfnReset)(void *pvState);
     DECLCALLBACKMEMBER(void,     pfnReady)(void *pvState);
} VIRTIOIOCALLBACKS;
/** Pointer to a const VirtIO port I/O callback structure. */
typedef const VIRTIOIOCALLBACKS *PCVIRTIOIOCALLBACKS;
/** @} */


/** Most of the following definitions attempt to adhere to the names and
 *  and forms used by the VirtIO 1.0 specification to increase the maintainability
 *  and speed of ramp-up, in other words, to as unambiguously as possible
 *  map the spec to the VirtualBox implementation of it */

/** Indicates that this descriptor chains to another */
#define VIRTQ_DESC_F_NEXT                   1

/** Marks buffer as write-only (default ro) */
#define VIRTQ_DESC_F_WRITE                  2

/** This means the buffer contains a list of buffer descriptors. */
#define VIRTQ_DESC_F_INDIRECT               4

/** Optimization: Device advises driver (unreliably):  Don't kick me when adding buffer */
#define VIRTQ_USED_F_NO_NOTIFY              1

/** Optimization: Driver advises device (unreliably): Don't interrupt when buffer consumed */
#define VIRTQ_AVAIL_F_NO_INTERRUPT          1

/** Using indirect descriptors */
#define VIRTIO_F_INDIRECT_DESC              28

/** Indicates either avail_event, or used_event fields are in play */
#define VIRTIO_F_EVENT_IDX                  29

/** Allow aribtary descriptor layouts */
#define VIRTIO_F_ANY_LAYOUT                 27

typedef struct VIRTIOPCIPARAMS
{
    uint16_t  uDeviceId;
    uint16_t  uClassBase;
    uint16_t  uClassSub;
    uint16_t  uClassProg;
    uint16_t  uSubsystemId;
    uint16_t  uSubsystemVendorId;
    uint16_t  uRevisionId;
    uint16_t  uInterruptLine;
    uint16_t  uInterruptPin;
} VIRTIOPCIPARAMS, *PVIRTIOPCIPARAMS;

typedef struct virtio_pci_cap
{
    /* All little-endian */
    uint8_t   uCapVndr;               /** Generic PCI field: PCI_CAP_ID_VNDR          */
    uint8_t   uCapNext;               /** Generic PCI field: next ptr.                */
    uint8_t   uCapLen;                /** Generic PCI field: capability length        */
    uint8_t   uCfgType;               /** Identifies the structure.                   */
    uint8_t   uBar;                   /** Where to find it.                           */
    uint8_t   uPadding[3];            /** Pad to full dword.                          */
    uint32_t  uOffset;                /** Offset within bar.  (L.E.)                  */
    uint32_t  uLength;                /** Length of struct, in bytes. (L.E.)          */
}  VIRTIOPCICAP, *PVIRTIOPCICAP;

typedef struct virtio_pci_common_cfg /* VirtIO 1.0 specification name of this struct  */
{
    /**
     *
     * RW = driver (guest) writes value into this field.
     * RO = device (host)  writes value into this field.
     */

    /* Per device fields */
    uint32_t  uDeviceFeatureSelect;   /** RW (driver selects device features)         */
    uint32_t  uDeviceFeature;         /** RO (device reports features to driver)      */
    uint32_t  uDriverFeatureSelect;   /** RW (driver selects driver features)         */
    uint32_t  uDriverFeature;         /** RW (driver accepts device features)         */
    uint16_t  uMsixConfig;            /** RW (driver sets MSI-X config vector)        */
    uint16_t  uNumQueues;             /** RO (device specifies max queues)            */
    uint8_t   uDeviceStatus;          /** RW (driver writes device status, 0 resets)  */
    uint8_t   uConfigGeneration;      /** RO (device changes when changing configs)   */

    /* Per virtqueue fields (as determined by uQueueSelect) */
    uint16_t  uQueueSelect;           /** RW (selects queue focus for these fields)   */
    uint16_t  uQueueSize;             /** RW (queue size, 0 - 2^n)                    */
    uint16_t  uQueueMsixVector;       /** RW (driver selects MSI-X queue vector)      */
    uint16_t  uQueueEnable;           /** RW (driver controls usability of queue)     */
    uint16_t  uQueueNotifyOff;        /** RO (offset to virtqueue; see spec)          */
    uint64_t  uQueueDesc;             /** RW (driver writes desc table phys addr)     */
    uint64_t  uQueueAvail;            /** RW (driver writes avail ring phys addr)     */
    uint64_t  uQueueUsed;             /** RW (driver writes used ring  phys addr)     */
} VIRTIOCOMMONCFG, *PVIRTIOCOMMONCFG;

typedef struct virtio_pci_notify_cap
{
        struct virtio_pci_cap cap;
        uint32_t uNotifyOffMultiplier; /* notify_off_multiplier                        */
} VIRTIONOTIFYCAP, *PVIRTIONOTIFYCAP;

typedef uint8_t VIRTIOISRCAP, *PVIRTIOISRCAP;

/* Device-specific configuration (if any) ... T.B.D. (if and when neeed), provide an
 * interface/callback that lets the client of this code manage it, if and when needed.
 * probably just passing in a struct length and a callback from our capabilities
 * region handler */
typedef struct virtio_pci_dev_cfg
{
} VIRTIODEVCAP, *PVIRTIODEVCAP;

typedef struct virtio_pci_cfg_cap {
        struct virtio_pci_cap cap;
        uint8_t pci_cfg_data[4]; /* Data for BAR access. */
} VIRTIOPCICFGCAP, *PVIRTIOPCICFGCAP;

typedef struct virtq_desc  /* VirtIO 1.0 specification formal name of this struct     */
{
    uint64_t  pGcPhysBuf;             /** addr        GC physical address of buffer   */
    uint32_t  cbBuf;                  /** len         Buffer length                   */
    uint16_t  fFlags;                 /** flags       Buffer specific flags           */
    uint16_t  uNext;                  /** next        Chain idx if VIRTIO_DESC_F_NEXT */
} VIRTQUEUEDESC, *PVIRTQUEUEDESC;

typedef struct virtq_avail  /* VirtIO 1.0 specification formal name of this struct    */
{
    uint16_t  fFlags;                 /** flags       avail ring guest-to-host flags */
    uint16_t  uIdx;                   /** idx         Index of next free ring slot    */
    uint16_t  auRing[1];              /** ring        Ring: avail guest-to-host bufs  */
    uint16_t  uUsedEventIdx;          /** used_event  (if VIRTQ_USED_F_NO_NOTIFY)     */
} VIRTQUEUEAVAIL, *PVIRTQUEUEAVAIL;

typedef struct virtq_used_elem /* VirtIO 1.0 specification formal name of this struct */
{
    uint32_t  uIdx;                   /** idx         Start of used descriptor chain  */
    uint32_t  cbElem;                 /** len         Total len of used desc chain    */
} VIRTQUEUEUSEDELEM;

typedef struct virt_used  /* VirtIO 1.0 specification formal name of this struct */
{
    uint16_t  fFlags;                 /** flags       used ring host-to-guest flags  */
    uint16_t  uIdx;                   /** idx         Index of next ring slot        */
    VIRTQUEUEUSEDELEM aRing[1];       /** ring        Ring: used host-to-guest bufs  */
    uint16_t  uAvailEventIdx;         /** avail_event if (VIRTQ_USED_F_NO_NOTIFY)    */
} VIRTQUEUEUSED, *PVIRTQUEUEUSED;


typedef struct virtq
{
    uint16_t cbQueue;                 /** virtq size                                 */
    uint16_t padding[3];              /** 64-bit-align phys ptrs to start of struct  */
    RTGCPHYS pGcPhysVirtqDescriptors; /** (struct virtq_desc  *)                     */
    RTGCPHYS pGcPhysVirtqAvail;       /** (struct virtq_avail *)                     */
    RTGCPHYS pGcPhysVirtqUsed;        /** (struct virtq_used  *)                     */
} VIRTQUEUE, *PVIRTQUEUE;

/**
 * Queue callback (consumer?).
 *
 * @param   pvState         Pointer to the VirtIO PCI core state, VIRTIOSTATE.
 * @param   pQueue          Pointer to the queue structure.
 */
typedef DECLCALLBACK(void) FNVIRTIOQUEUECALLBACK(void *pvState, struct VQueue *pQueue);
/** Pointer to a VQUEUE callback function. */
typedef FNVIRTIOQUEUECALLBACK *PFNVIRTIOQUEUECALLBACK;

//PCIDevGetCapabilityList
//PCIDevSetCapabilityList
//DECLINLINE(void) PDMPciDevSetCapabilityList(PPDMPCIDEV pPciDev, uint8_t u8Offset)
//    PDMPciDevSetByte(pPciDev, VBOX_PCI_CAPABILITY_LIST, u8Offset);
//DECLINLINE(uint8_t) PDMPciDevGetCapabilityList(PPDMPCIDEV pPciDev)
//    return PDMPciDevGetByte(pPciDev, VBOX_PCI_CAPABILITY_LIST);

typedef struct VQueue
{
    VIRTQUEUE    VirtQueue;
    uint16_t uNextAvailIndex;
    uint16_t uNextUsedIndex;
    uint32_t uPageNumber;
    R3PTRTYPE(PFNVIRTIOQUEUECALLBACK) pfnCallback;
    R3PTRTYPE(const char *)           pcszName;
} VQUEUE;
typedef VQUEUE *PVQUEUE;

typedef struct VQueueElemSeg
{
    RTGCPHYS addr;
    void    *pv;
    uint32_t cb;
} VQUEUESEG;

typedef struct VQueueElem
{
    uint32_t  idx;
    uint32_t  nIn;
    uint32_t  nOut;
    VQUEUESEG aSegsIn[VIRTQUEUE_MAX_SIZE];
    VQUEUESEG aSegsOut[VIRTQUEUE_MAX_SIZE];
} VQUEUEELEM;
typedef VQUEUEELEM *PVQUEUEELEM;


/**
 * Interface this library uses to let the client handle VirtioIO device-specific capabilities I/O
 */

typedef DECLCALLBACK(int)   FNVIRTIODEVCAPWRITE(PPDMDEVINS pDevIns, RTGCPHYS GCPhysAddr, const void *pvBuf, size_t cbWrite);
typedef FNVIRTIODEVCAPWRITE *PFNVIRTIODEVCAPWRITE;

typedef DECLCALLBACK(int)   FNVIRTIODEVCAPREAD(PPDMDEVINS pDevIns, RTGCPHYS GCPhysAddr, const void *pvBuf, size_t cbRead);
typedef FNVIRTIODEVCAPREAD *PFNVIRTIODEVCAPREAD;

/**
 * The core (/common) state of the VirtIO PCI device
 *
 * @implements  PDMILEDPORTS
 */
typedef struct VIRTIOSTATE
{
    PDMPCIDEV              dev;
    PDMCRITSECT            cs;                     /**< Critical section - what is it protecting? */
    /* Read-only part, never changes after initialization. */
    char                   szInstance[8];          /**< Instance name, e.g. VNet#1. */

#if HC_ARCH_BITS != 64
    uint32_t               padding1;
#endif

    /** Status LUN: Base interface. */
    PDMIBASE               IBase;
    /** Status LUN: LED port interface. */
    PDMILEDPORTS           ILeds;
    /** Status LUN: LED connector (peer). */
    R3PTRTYPE(PPDMILEDCONNECTORS) pLedsConnector;

    PPDMDEVINSR3           pDevInsR3;              /**< Device instance - R3. */
    PPDMDEVINSR0           pDevInsR0;              /**< Device instance - R0. */
    PPDMDEVINSRC           pDevInsRC;              /**< Device instance - RC. */

    /** Base address of PCI capabilities */
    RTGCPHYS               GCPhysPciCapBase;

    VIRTIOCOMMONCFG        pGcPhysVirtioCommonCfg;
    VIRTIONOTIFYCAP        pGcPhysVirtioNotifyCap;
    VIRTIOISRCAP           pGcPhysVirtioIsrCap;
    VIRTIODEVCAP           pGcPhysVirtioDevCap;
    VIRTIOPCICFGCAP        pGcPhysVirtioPciCap;

    /** Callbacks when guest driver reads or writes VirtIO device-specific capabilities(s) */

    PFNVIRTIODEVCAPWRITE   pfnVirtioDevCapWrite;
    PFNVIRTIODEVCAPREAD    pfnVirtioDevCapRead;

#if HC_ARCH_BITS == 64
    uint32_t               padding2;
#endif

    /** Base port of I/O space region. */
    RTIOPORT               IOPortBase;

    /* Read/write part, protected with critical section. */
    /** Status LED. */
    PDMLED                 led;

    uint32_t               uGuestFeatures;
    uint16_t               uQueueSelector;         /**< An index in aQueues array. */
    uint8_t                uStatus;                /**< Device Status (bits are device-specific). */
    uint8_t                uISR;                   /**< Interrupt Status Register. */

#if HC_ARCH_BITS != 64
    uint32_t               padding3;
#endif

    uint32_t               nQueues;       /**< Actual number of queues used. */
    VQUEUE                 Queues[VIRTIO_MAX_NQUEUES];

    PVIRTIOPCICAP pCommonCfg;
    PVIRTIOPCICAP pNotifyCap;
    PVIRTIOPCICAP pISRCap;
    PVIRTIOPCICAP pPCICfgCap;
    PVIRTIOPCICAP pDeviceCap;
} VIRTIOSTATE, *PVIRTIOSTATE;

void    virtioRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta);
void   *virtioQueryInterface(struct PDMIBASE *pInterface, const char *pszIID);

int     virtioIOPortIn(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT port,
                        uint32_t *pu32, unsigned cb, PCVIRTIOIOCALLBACKS pCallbacks);

int     virtioIOPortOut(PPDMDEVINS pDevIns, void  *pvUser, RTIOPORT port,
                        uint32_t u32, unsigned cb, PCVIRTIOIOCALLBACKS pCallbacks);

int     virtioConstruct(PPDMDEVINS pDevIns, PVIRTIOSTATE pVirtio, int iInstance,
                    PVIRTIOPCIPARAMS pPciParams, const char *pcszNameFmt,
                    uint32_t nQueues, uint32_t uVirtioRegion,
                    PFNVIRTIODEVCAPREAD devCapReadCallback, PFNVIRTIODEVCAPWRITE devCapWriteCallback,
                    uint16_t cbDevSpecificCap);


int     virtioDestruct(       PVIRTIOSTATE pState);
void    virtioReset(          PVIRTIOSTATE pState);
int     virtioSaveExec(       PVIRTIOSTATE pState, PSSMHANDLE pSSM);
int     virtioLoadExec(       PVIRTIOSTATE pState, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass, uint32_t nQueues);
void    virtioSetReadLed(     PVIRTIOSTATE pState, bool fOn);
void    virtioSetWriteLed(    PVIRTIOSTATE pState, bool fOn);
int     virtioRaiseInterrupt( PVIRTIOSTATE pState, int rcBusy, uint8_t uint8_tIntCause);

PVQUEUE virtioAddQueue(       PVIRTIOSTATE pState, unsigned uSize, PFNVIRTIOQUEUECALLBACK pfnCallback, const char *pcszName);
bool    virtQueueSkip(        PVIRTIOSTATE pState, PVQUEUE pQueue);
bool    virtQueueGet(         PVIRTIOSTATE pState, PVQUEUE pQueue, PVQUEUEELEM pElem, bool fRemove = true);
void    virtQueuePut(         PVIRTIOSTATE pState, PVQUEUE pQueue, PVQUEUEELEM pElem, uint32_t len, uint32_t uReserved = 0);
void    virtQueueNotify(      PVIRTIOSTATE pState, PVQUEUE pQueue);
void    virtQueueSync(        PVIRTIOSTATE pState, PVQUEUE pQueue);
void    vringSetNotification( PVIRTIOSTATE pState, PVIRTQUEUE pVirtQueue, bool fEnabled);


/*
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

DECLINLINE(uint16_t) vringReadAvail(PVIRTIOSTATE pState, PVIRTQUEUE pVirtQueue)
{
    uint16_t dataWord;
    PDMDevHlpPhysRead(pState->CTX_SUFF(pDevIns),
                      pVirtQueue->pGcPhysVirtqAvail + RT_UOFFSETOF(VIRTQUEUEAVAIL, uIdx),
                      &dataWord, sizeof(dataWord));
    return dataWord;
}

DECLINLINE(bool) virtQueuePeek(PVIRTIOSTATE pState, PVQUEUE pQueue, PVQUEUEELEM pElem)
{
    return virtQueueGet(pState, pQueue, pElem, /* fRemove */ false);
}

DECLINLINE(bool) virtQueueIsReady(PVIRTIOSTATE pState, PVQUEUE pQueue)
{
    NOREF(pState);
    return !!pQueue->VirtQueue.pGcPhysVirtqAvail;
}

DECLINLINE(bool) virtQueueIsEmpty(PVIRTIOSTATE pState, PVQUEUE pQueue)
{
    return (vringReadAvail(pState, &pQueue->VirtQueue) == pQueue->uNextAvailIndex);
}

DECLINLINE(int) virtioLock(VIRTIOSTATE *pState, int rcBusy)
{
    int rc = PDMCritSectEnter(&pState->cs, rcBusy);
    return rc;
}

DECLINLINE(void) virtioUnlock(VIRTIOSTATE *pState)
{
    PDMCritSectLeave(&pState->cs);
}

#endif /* !VBOX_INCLUDED_SRC_VirtIO_Virtio_1_0_h */
