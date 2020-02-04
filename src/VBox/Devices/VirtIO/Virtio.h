/* $Id$ */
/** @file
 * Virtio.h - Virtio Declarations
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

#ifndef VBOX_INCLUDED_SRC_VirtIO_Virtio_h
#define VBOX_INCLUDED_SRC_VirtIO_Virtio_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/types.h>


/** Pointer to the core shared state of a VirtIO PCI device */
typedef struct VPCISTATE *PVPCISTATE;
/** Pointer to the core ring-3 state of a VirtIO PCI device */
typedef struct VPCISTATER3 *PVPCISTATER3;
/** Pointer to the core ring-0 state of a VirtIO PCI device */
typedef struct VPCISTATER0 *PVPCISTATER0;
/** Pointer to the core raw-mode state of a VirtIO PCI device */
typedef struct VPCISTATERC *PVPCISTATERC;

/** Pointer to the core current context state of a VirtIO PCI device */
typedef CTX_SUFF(PVPCISTATE) PVPCISTATECC;
/** The core current context state of a VirtIO PCI device */
typedef struct CTX_SUFF(VPCISTATE) VPCISTATECC;


/** @name Saved state versions.
 * The saved state version is changed if either common or any of specific
 * parts are changed. That is, it is perfectly possible that the version
 * of saved vnet state will increase as a result of change in vblk structure
 * for example.
 */
#define VIRTIO_SAVEDSTATE_VERSION_3_1_BETA1 1
#define VIRTIO_SAVEDSTATE_VERSION           2
/** @} */

#define DEVICE_PCI_VENDOR_ID                0x1AF4
#define DEVICE_PCI_BASE_ID                  0x1000
#define DEVICE_PCI_SUBSYSTEM_VENDOR_ID      0x1AF4
#define DEVICE_PCI_SUBSYSTEM_BASE_ID        1

#define VIRTIO_MAX_NQUEUES                  3

#define VPCI_HOST_FEATURES                  0x0
#define VPCI_GUEST_FEATURES                 0x4
#define VPCI_QUEUE_PFN                      0x8
#define VPCI_QUEUE_NUM                      0xC
#define VPCI_QUEUE_SEL                      0xE
#define VPCI_QUEUE_NOTIFY                   0x10
#define VPCI_STATUS                         0x12
#define VPCI_ISR                            0x13
#define VPCI_CONFIG                         0x14

#define VPCI_ISR_QUEUE                      0x1
#define VPCI_ISR_CONFIG                     0x3

#define VPCI_STATUS_ACK                     0x01
#define VPCI_STATUS_DRV                     0x02
#define VPCI_STATUS_DRV_OK                  0x04
#define VPCI_STATUS_FAILED                  0x80

#define VPCI_F_NOTIFY_ON_EMPTY              UINT32_C(0x01000000)
#define VPCI_F_ANY_LAYOUT                   UINT32_C(0x08000000)
#define VPCI_F_RING_INDIRECT_DESC           UINT32_C(0x10000000)
#define VPCI_F_RING_EVENT_IDX               UINT32_C(0x20000000)
#define VPCI_F_BAD_FEATURE                  UINT32_C(0x40000000)

#define VRINGDESC_MAX_SIZE                  (2 * 1024 * 1024)
#define VRINGDESC_F_NEXT                    0x01
#define VRINGDESC_F_WRITE                   0x02
#define VRINGDESC_F_INDIRECT                0x04

typedef struct VRINGDESC
{
    uint64_t u64Addr;
    uint32_t uLen;
    uint16_t u16Flags;
    uint16_t u16Next;
} VRINGDESC;
typedef VRINGDESC *PVRINGDESC;

#define VRINGAVAIL_F_NO_INTERRUPT           0x01

typedef struct VRINGAVAIL
{
    uint16_t uFlags;
    uint16_t uNextFreeIndex;
    uint16_t auRing[1];
} VRINGAVAIL;

typedef struct VRINGUSEDELEM
{
    uint32_t uId;
    uint32_t uLen;
} VRINGUSEDELEM;

#define VRINGUSED_F_NO_NOTIFY 0x01

typedef struct VRingUsed
{
    uint16_t      uFlags;
    uint16_t      uIndex;
    VRINGUSEDELEM aRing[1];
} VRINGUSED;
typedef VRINGUSED *PVRINGUSED;

#define VRING_MAX_SIZE                      1024

typedef struct VRING
{
    uint16_t   uSize;
    uint16_t   padding[3];
    RTGCPHYS   addrDescriptors;
    RTGCPHYS   addrAvail;
    RTGCPHYS   addrUsed;
} VRING;
typedef VRING *PVRING;

typedef struct VQUEUE
{
    VRING       VRing;
    uint16_t    uNextAvailIndex;
    uint16_t    uNextUsedIndex;
    uint32_t    uPageNumber;
    char        szName[16];
} VQUEUE;
typedef VQUEUE *PVQUEUE;

/**
 * Queue callback (consumer?).
 *
 * @param   pDevIns         The device instance.
 * @param   pQueue          Pointer to the queue structure.
 */
typedef DECLCALLBACK(void) FNVPCIQUEUECALLBACK(PPDMDEVINS pDevIns, PVQUEUE pQueue);
/** Pointer to a VQUEUE callback function. */
typedef FNVPCIQUEUECALLBACK *PFNVPCIQUEUECALLBACK;

typedef struct VQUEUER3
{
    R3PTRTYPE(PFNVPCIQUEUECALLBACK) pfnCallback;
} VQUEUER3;
typedef VQUEUER3 *PVQUEUER3;

typedef struct VQUEUESEG
{
    RTGCPHYS addr;
    void    *pv;
    uint32_t cb;
} VQUEUESEG;

typedef struct VQUEUEELEM
{
    uint32_t  uIndex;
    uint32_t  cIn;
    uint32_t  cOut;
    VQUEUESEG aSegsIn[VRING_MAX_SIZE];
    VQUEUESEG aSegsOut[VRING_MAX_SIZE];
} VQUEUEELEM;
typedef VQUEUEELEM *PVQUEUEELEM;


enum VirtioDeviceType
{
    VIRTIO_NET_ID = 0,
    VIRTIO_BLK_ID = 1,
    VIRTIO_32BIT_HACK = 0x7fffffff
};


/**
 * The core shared state of a VirtIO PCI device
 */
typedef struct VPCISTATE
{
    PDMCRITSECT             cs;      /**< Critical section - what is it protecting? */
    /** Read-only part, never changes after initialization. */
    char                    szInstance[8];         /**< Instance name, e.g. VNet#1. */

    /* Read/write part, protected with critical section. */
    /** Status LED. */
    PDMLED                  led;

    uint32_t                uGuestFeatures;
    uint16_t                uQueueSelector;         /**< An index in aQueues array. */
    uint8_t                 uStatus; /**< Device Status (bits are device-specific). */
    uint8_t                 uISR;                   /**< Interrupt Status Register. */

    /** Number of queues actually used. */
    uint32_t                cQueues;
    uint32_t                u32Padding;
    /** Shared queue data. */
    VQUEUE                  Queues[VIRTIO_MAX_NQUEUES];

    STAMCOUNTER             StatIntsRaised;
    STAMCOUNTER             StatIntsSkipped;

#ifdef VBOX_WITH_STATISTICS
    STAMPROFILEADV          StatIOReadR3;
    STAMPROFILEADV          StatIOReadR0;
    STAMPROFILEADV          StatIOReadRC;
    STAMPROFILEADV          StatIOWriteR3;
    STAMPROFILEADV          StatIOWriteR0;
    STAMPROFILEADV          StatIOWriteRC;
    STAMPROFILE             StatCsR3;
    STAMPROFILE             StatCsR0;
    STAMPROFILE             StatCsRC;
#endif
} VPCISTATE;


/**
 * The core ring-3 state of a VirtIO PCI device
 *
 * @implements  PDMILEDPORTS
 */
typedef struct VPCISTATER3
{
    /** Status LUN: Base interface. */
    PDMIBASE                        IBase;
    /** Status LUN: LED port interface. */
    PDMILEDPORTS                    ILeds;
    /** Status LUN: LED connector (peer). */
    R3PTRTYPE(PPDMILEDCONNECTORS)   pLedsConnector;
    /** Pointer to the shared state. */
    R3PTRTYPE(PVPCISTATE)           pShared;
    /** Ring-3 per-queue data. */
    VQUEUER3                        Queues[VIRTIO_MAX_NQUEUES];
} VPCISTATER3;


/**
 * The core ring-0 state of a VirtIO PCI device
 */
typedef struct VPCISTATER0
{
    uint64_t                uUnused;
} VPCISTATER0;


/**
 * The core raw-mode state of a VirtIO PCI device
 */
typedef struct VPCISTATERC
{
    uint64_t                uUnused;
} VPCISTATERC;


/** @name VirtIO port I/O callbacks.
 * @{ */
typedef struct VPCIIOCALLBACKS
{
     DECLCALLBACKMEMBER(uint32_t, pfnGetHostFeatures)(PVPCISTATE pVPciState);
     DECLCALLBACKMEMBER(uint32_t, pfnGetHostMinimalFeatures)(PVPCISTATE pVPciState);
     DECLCALLBACKMEMBER(void,     pfnSetHostFeatures)(PVPCISTATE pVPciState, uint32_t fFeatures);
     DECLCALLBACKMEMBER(int,      pfnGetConfig)(PVPCISTATE pVPciState, uint32_t offCfg, uint32_t cb, void *pvData);
     DECLCALLBACKMEMBER(int,      pfnSetConfig)(PVPCISTATE pVPciState, uint32_t offCfg, uint32_t cb, void *pvData);
     DECLCALLBACKMEMBER(int,      pfnReset)(PPDMDEVINS pDevIns);
     DECLCALLBACKMEMBER(void,     pfnReady)(PPDMDEVINS pDevIns);
} VPCIIOCALLBACKS;
/** Pointer to a const VirtIO port I/O callback structure. */
typedef const VPCIIOCALLBACKS *PCVPCIIOCALLBACKS;
/** @} */

int   vpciR3Init(PPDMDEVINS pDevIns, PVPCISTATE pThis, PVPCISTATECC pThisCC, uint16_t uDeviceId, uint16_t uClass, uint32_t cQueues);
int   vpciRZInit(PPDMDEVINS pDevIns, PVPCISTATE pThis, PVPCISTATECC pThisCC);
int   vpciR3Term(PPDMDEVINS pDevIns, PVPCISTATE pThis);
PVQUEUE vpciR3AddQueue(PVPCISTATE pThis, PVPCISTATECC pThisCC, unsigned uSize, PFNVPCIQUEUECALLBACK pfnCallback, const char *pcszName);
void *vpciR3QueryInterface(PVPCISTATECC pThisCC, const char *pszIID);
void  vpciR3SetWriteLed(PVPCISTATE pThis, bool fOn);
void  vpciR3SetReadLed(PVPCISTATE pThis, bool fOn);
int   vpciR3SaveExec(PCPDMDEVHLPR3 pHlp, PVPCISTATE pThis, PSSMHANDLE pSSM);
int   vpciR3LoadExec(PCPDMDEVHLPR3 pHlp, PVPCISTATE pThis, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass, uint32_t cQueues);
void  vpciR3DumpStateWorker(PVPCISTATE pThis, PCDBGFINFOHLP pHlp);

void  vpciReset(PPDMDEVINS pDevIns, PVPCISTATE pThis);
int   vpciRaiseInterrupt(PPDMDEVINS pDevIns, PVPCISTATE pThis, int rcBusy, uint8_t u8IntCause);
int   vpciIOPortIn(PPDMDEVINS pDevIns, PVPCISTATE pThis, RTIOPORT offPort,
                   uint32_t *pu32, unsigned cb,PCVPCIIOCALLBACKS pCallbacks);
int   vpciIOPortOut(PPDMDEVINS pDevIns, PVPCISTATE pThis, PVPCISTATECC pThisCC, RTIOPORT offPort,
                    uint32_t u32, unsigned cb, PCVPCIIOCALLBACKS pCallbacks);

#define VPCI_CS
DECLINLINE(int) vpciCsEnter(PPDMDEVINS pDevIns, PVPCISTATE pThis, int rcBusy)
{
#ifdef VPCI_CS
    STAM_PROFILE_START(&pThis->CTX_SUFF(StatCs), a);
    int rc = PDMDevHlpCritSectEnter(pDevIns, &pThis->cs, rcBusy);
    STAM_PROFILE_STOP(&pThis->CTX_SUFF(StatCs), a);
    return rc;
#else
    RT_NOREF(pDevIns, pThis, rcBusy);
    return VINF_SUCCESS;
#endif
}

DECLINLINE(void) vpciCsLeave(PPDMDEVINS pDevIns, PVPCISTATE pThis)
{
#ifdef VPCI_CS
    PDMDevHlpCritSectLeave(pDevIns, &pThis->cs);
#endif
}

void vringSetNotification(PPDMDEVINS pDevIns, PVRING pVRing, bool fEnabled);

DECLINLINE(uint16_t) vringReadAvailIndex(PPDMDEVINS pDevIns, PVRING pVRing)
{
    uint16_t idx = 0;
    PDMDevHlpPhysRead(pDevIns, pVRing->addrAvail + RT_UOFFSETOF(VRINGAVAIL, uNextFreeIndex), &idx, sizeof(idx));
    return idx;
}

bool vqueueSkip(PPDMDEVINS pDevIns, PVPCISTATE pThis, PVQUEUE pQueue);
bool vqueueGet(PPDMDEVINS pDevIns, PVPCISTATE pThis, PVQUEUE pQueue, PVQUEUEELEM pElem, bool fRemove = true);
void vqueuePut(PPDMDEVINS pDevIns, PVPCISTATE pThis, PVQUEUE pQueue, PVQUEUEELEM pElem, uint32_t uLen, uint32_t uReserved = 0);
void vqueueSync(PPDMDEVINS pDevIns, PVPCISTATE pThis, PVQUEUE pQueue);

DECLINLINE(bool) vqueuePeek(PPDMDEVINS pDevIns, PVPCISTATE pThis, PVQUEUE pQueue, PVQUEUEELEM pElem)
{
    return vqueueGet(pDevIns, pThis, pQueue, pElem, /* fRemove */ false);
}

DECLINLINE(bool) vqueueIsReady(PVQUEUE pQueue)
{
    return !!pQueue->VRing.addrAvail;
}

DECLINLINE(bool) vqueueIsEmpty(PPDMDEVINS pDevIns, PVQUEUE pQueue)
{
    return vringReadAvailIndex(pDevIns, &pQueue->VRing) == pQueue->uNextAvailIndex;
}

#endif /* !VBOX_INCLUDED_SRC_VirtIO_Virtio_h */

