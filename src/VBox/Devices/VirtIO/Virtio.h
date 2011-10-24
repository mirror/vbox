/* $Id$ */
/** @file
 * Virtio.h - Virtio Declarations
 */

/*
 * Copyright (C) 2009 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBox_Virtio_h
#define ___VBox_Virtio_h

#include <iprt/ctype.h>

#define VIRTIO_RELOCATE(p, o) *(RTHCUINTPTR *)&p += o

/*
 * The saved state version is changed if either common or any of specific
 * parts are changed. That is, it is perfectly possible that the version
 * of saved vnet state will increase as a result of change in vblk structure
 * for example.
 */
#define VIRTIO_SAVEDSTATE_VERSION_3_1_BETA1 1
#define VIRTIO_SAVEDSTATE_VERSION           2

#define DEVICE_PCI_VENDOR_ID                0x1AF4
#define DEVICE_PCI_DEVICE_ID                0x1000
#define DEVICE_PCI_SUBSYSTEM_VENDOR_ID      0x1AF4

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

#define VPCI_F_NOTIFY_ON_EMPTY              0x01000000
#define VPCI_F_BAD_FEATURE                  0x40000000

#define VRINGDESC_MAX_SIZE                  (2 * 1024 * 1024)
#define VRINGDESC_F_NEXT                    0x01
#define VRINGDESC_F_WRITE                   0x02

struct VRingDesc
{
    uint64_t u64Addr;
    uint32_t uLen;
    uint16_t u16Flags;
    uint16_t u16Next;
};
typedef struct VRingDesc VRINGDESC;
typedef VRINGDESC *PVRINGDESC;

#define VRINGAVAIL_F_NO_INTERRUPT 0x01

struct VRingAvail
{
    uint16_t uFlags;
    uint16_t uNextFreeIndex;
    uint16_t auRing[1];
};
typedef struct VRingAvail VRINGAVAIL;

struct VRingUsedElem
{
    uint32_t uId;
    uint32_t uLen;
};
typedef struct VRingUsedElem VRINGUSEDELEM;

#define VRINGUSED_F_NO_NOTIFY 0x01

struct VRingUsed
{
    uint16_t      uFlags;
    uint16_t      uIndex;
    VRINGUSEDELEM aRing[1];
};
typedef struct VRingUsed VRINGUSED;
typedef VRINGUSED *PVRINGUSED;

#define VRING_MAX_SIZE 1024

struct VRing
{
    uint16_t   uSize;
    uint16_t   padding[3];
    RTGCPHYS   addrDescriptors;
    RTGCPHYS   addrAvail;
    RTGCPHYS   addrUsed;
};
typedef struct VRing VRING;
typedef VRING *PVRING;

struct VQueue
{
    VRING    VRing;
    uint16_t uNextAvailIndex;
    uint16_t uNextUsedIndex;
    uint32_t uPageNumber;
#ifdef IN_RING3
    void   (*pfnCallback)(void *pvState, struct VQueue *pQueue);
#else
    RTR3UINTPTR pfnCallback;
#endif
    R3PTRTYPE(const char *) pcszName;
};
typedef struct VQueue VQUEUE;
typedef VQUEUE *PVQUEUE;

struct VQueueElemSeg
{
    RTGCPHYS addr;
    void    *pv;
    uint32_t cb;
};
typedef struct VQueueElemSeg VQUEUESEG;

struct VQueueElem
{
    uint32_t  uIndex;
    uint32_t  nIn;
    uint32_t  nOut;
    VQUEUESEG aSegsIn[VRING_MAX_SIZE];
    VQUEUESEG aSegsOut[VRING_MAX_SIZE];
};
typedef struct VQueueElem VQUEUEELEM;
typedef VQUEUEELEM *PVQUEUEELEM;


enum VirtioDeviceType
{
    VIRTIO_NET_ID = 0,
    VIRTIO_BLK_ID = 1,
    VIRTIO_32BIT_HACK = 0x7fffffff
};


/**
 * The state of the VirtIO PCI device
 *
 * @implements  PDMILEDPORTS
 */
struct VPCIState_st
{
    PDMCRITSECT            cs;      /**< Critical section - what is it protecting? */
    /* Read-only part, never changes after initialization. */
    char                   szInstance[8];         /**< Instance name, e.g. VNet#1. */

#if HC_ARCH_BITS != 64
    uint32_t               padding1;
#endif

    /** Status LUN: Base interface. */
    PDMIBASE               IBase;
    /** Status LUN: LED port interface. */
    PDMILEDPORTS           ILeds;
    /** Status LUN: LED connector (peer). */
    R3PTRTYPE(PPDMILEDCONNECTORS) pLedsConnector;

    PPDMDEVINSR3           pDevInsR3;                   /**< Device instance - R3. */
    PPDMDEVINSR0           pDevInsR0;                   /**< Device instance - R0. */
    PPDMDEVINSRC           pDevInsRC;                   /**< Device instance - RC. */

#if HC_ARCH_BITS == 64
    uint32_t               padding2;
#endif

    /** TODO */
    PCIDEVICE              pciDevice;
    /** Base port of I/O space region. */
    RTIOPORT               addrIOPort;

    /* Read/write part, protected with critical section. */
    /** Status LED. */
    PDMLED                 led;

    uint32_t               uGuestFeatures;
    uint16_t               uQueueSelector;         /**< An index in aQueues array. */
    uint8_t                uStatus; /**< Device Status (bits are device-specific). */
    uint8_t                uISR;                   /**< Interrupt Status Register. */

#if HC_ARCH_BITS != 64
    uint32_t               padding3;
#endif

    uint32_t               nQueues;       /**< Actual number of queues used. */
    VQUEUE                 Queues[VIRTIO_MAX_NQUEUES];

#if defined(VBOX_WITH_STATISTICS)
    STAMPROFILEADV         StatIOReadGC;
    STAMPROFILEADV         StatIOReadHC;
    STAMPROFILEADV         StatIOWriteGC;
    STAMPROFILEADV         StatIOWriteHC;
    STAMCOUNTER            StatIntsRaised;
    STAMCOUNTER            StatIntsSkipped;
    STAMPROFILE            StatCsGC;
    STAMPROFILE            StatCsHC;
#endif /* VBOX_WITH_STATISTICS */
};
typedef struct VPCIState_st VPCISTATE;
typedef VPCISTATE *PVPCISTATE;

/* Callbacks *****************************************************************/
typedef uint32_t (*PFNGETHOSTFEATURES)(void *pState);
typedef uint32_t (*PFNGETHOSTMINIMALFEATURES)(void *pState);
typedef void     (*PFNSETHOSTFEATURES)(void *pState, uint32_t uFeatures);
typedef int      (*PFNGETCONFIG)(void *pState, uint32_t port, uint32_t cb, void *data);
typedef int      (*PFNSETCONFIG)(void *pState, uint32_t port, uint32_t cb, void *data);
typedef int      (*PFNRESET)(void *pState);
typedef void     (*PFNREADY)(void *pState);
/*****************************************************************************/

int vpciRaiseInterrupt(VPCISTATE *pState, int rcBusy, uint8_t u8IntCause);
int vpciIOPortIn(PPDMDEVINS         pDevIns,
                 void              *pvUser,
                 RTIOPORT           port,
                 uint32_t          *pu32,
                 unsigned           cb,
                 PFNGETHOSTFEATURES pfnGetHostFeatures,
                 PFNGETCONFIG       pfnGetConfig);

int vpciIOPortOut(PPDMDEVINS                pDevIns,
                  void                     *pvUser,
                  RTIOPORT                  port,
                  uint32_t                  u32,
                  unsigned                  cb,
                  PFNGETHOSTMINIMALFEATURES pfnGetHostMinimalFeatures,
                  PFNGETHOSTFEATURES        pfnGetHostFeatures,
                  PFNSETHOSTFEATURES        pfnSetHostFeatures,
                  PFNRESET                  pfnReset,
                  PFNREADY                  pfnReady,
                  PFNSETCONFIG              pfnSetConfig);

void  vpciSetWriteLed(PVPCISTATE pState, bool fOn);
void  vpciSetReadLed(PVPCISTATE pState, bool fOn);
int   vpciSaveExec(PVPCISTATE pState, PSSMHANDLE pSSM);
int   vpciLoadExec(PVPCISTATE pState, PSSMHANDLE pSSM,
                   uint32_t uVersion, uint32_t uPass,
                   uint32_t nQueues);
int   vpciConstruct(PPDMDEVINS pDevIns, VPCISTATE *pState,
                    int iInstance, const char *pcszNameFmt,
                    uint16_t uSubsystemId, uint16_t uClass,
                    uint32_t nQueues);
int   vpciDestruct(VPCISTATE* pState);
void  vpciRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta);
void  vpciReset(PVPCISTATE pState);
void *vpciQueryInterface(struct PDMIBASE *pInterface, const char *pszIID);
PVQUEUE vpciAddQueue(VPCISTATE* pState, unsigned uSize,
                     void (*pfnCallback)(void *pvState, PVQUEUE pQueue),
                     const char *pcszName);

#define VPCI_CS
DECLINLINE(int) vpciCsEnter(VPCISTATE *pState, int iBusyRc)
{
#ifdef VPCI_CS
    STAM_PROFILE_START(&pState->CTXSUFF(StatCs), a);
    int rc = PDMCritSectEnter(&pState->cs, iBusyRc);
    STAM_PROFILE_STOP(&pState->CTXSUFF(StatCs), a);
    return rc;
#else
    return VINF_SUCCESS;
#endif
}

DECLINLINE(void) vpciCsLeave(VPCISTATE *pState)
{
#ifdef VPCI_CS
    PDMCritSectLeave(&pState->cs);
#endif
}

void vringSetNotification(PVPCISTATE pState, PVRING pVRing, bool fEnabled);

DECLINLINE(uint16_t) vringReadAvailIndex(PVPCISTATE pState, PVRING pVRing)
{
    uint16_t tmp;

    PDMDevHlpPhysRead(pState->CTX_SUFF(pDevIns),
                      pVRing->addrAvail + RT_OFFSETOF(VRINGAVAIL, uNextFreeIndex),
                      &tmp, sizeof(tmp));
    return tmp;
}

bool vqueueGet(PVPCISTATE pState, PVQUEUE pQueue, PVQUEUEELEM pElem);
void vqueuePut(PVPCISTATE pState, PVQUEUE pQueue, PVQUEUEELEM pElem, uint32_t uLen, uint32_t uReserved = 0);
void vqueueNotify(PVPCISTATE pState, PVQUEUE pQueue);
void vqueueSync(PVPCISTATE pState, PVQUEUE pQueue);


DECLINLINE(bool) vqueueIsReady(PVPCISTATE pState, PVQUEUE pQueue)
{
    NOREF(pState);
    return !!pQueue->VRing.addrAvail;
}

DECLINLINE(bool) vqueueIsEmpty(PVPCISTATE pState, PVQUEUE pQueue)
{
    return (vringReadAvailIndex(pState, &pQueue->VRing) == pQueue->uNextAvailIndex);
}

#endif /* ___VBox_Virtio_h */
