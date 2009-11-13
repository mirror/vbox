/* $Id$ */
/** @file
 * DevVirtioNet - Virtio Network Device
 *
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


#define LOG_GROUP LOG_GROUP_DEV_VIRTIO_NET
#define VNET_GC_SUPPORT

#include <iprt/ctype.h>
#ifdef IN_RING3
# include <iprt/mem.h>
#endif /* IN_RING3 */
#include <iprt/param.h>
#include <iprt/semaphore.h>
#include <VBox/pdmdev.h>
#include <VBox/tm.h>
#include "../Builtins.h"
#if 0
#include <iprt/crc32.h>
#include <iprt/string.h>
#include <VBox/vm.h>
#endif

// TODO: move declarations to the header file: #include "DevVirtioNet.h"

#ifndef VBOX_DEVICE_STRUCT_TESTCASE

#define INSTANCE(pState) pState->szInstance
#define IFACE_TO_STATE(pIface, ifaceName) ((VPCISTATE *)((char*)pIface - RT_OFFSETOF(VPCISTATE, ifaceName)))

#define VIRTIO_RELOCATE(p, o) *(RTHCUINTPTR *)&p += o

#ifdef DEBUG
#define QUEUENAME(s, q) (q->pcszName)
#endif /* DEBUG */

#endif /* VBOX_DEVICE_STRUCT_TESTCASE */

//- TODO: Move to Virtio.h ----------------------------------------------------

/*
 * The saved state version is changed if either common or any of specific
 * parts are changed. That is, it is perfectly possible that the version
 * of saved vnet state will increase as a result of change in vblk structure
 * for example.
 */
#define VIRTIO_SAVEDSTATE_VERSION_3_1_BETA1 1
#define VIRTIO_SAVEDSTATE_VERSION           2

#define VPCI_F_NOTIFY_ON_EMPTY 0x01000000
#define VPCI_F_BAD_FEATURE     0x40000000

#define VRINGDESC_MAX_SIZE (2 * 1024 * 1024)
#define VRINGDESC_F_NEXT   0x01
#define VRINGDESC_F_WRITE  0x02

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

#define VIRTIO_MAX_NQUEUES 3
#define VNET_NQUEUES       3

struct VPCIState_st
{
    PDMCRITSECT            cs;      /**< Critical section - what is it protecting? */
    /* Read-only part, never changes after initialization. */
#if HC_ARCH_BITS == 64
    uint32_t               padding1;
#endif
    VirtioDeviceType       enmDevType;               /**< Device type: net or blk. */
    char                   szInstance[8];         /**< Instance name, e.g. VNet#1. */

    PDMIBASE               IBase;
    PDMILEDPORTS           ILeds;                               /**< LED interface */
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
#endif /* VBOX_WITH_STATISTICS */
};
typedef struct VPCIState_st VPCISTATE;
typedef VPCISTATE *PVPCISTATE;

//- TODO: Move to VirtioPCI.cpp -----------------------------------------------

/*****************************************************************************/
RT_C_DECLS_BEGIN
PDMBOTHCBDECL(uint32_t) vnetGetHostFeatures(void *pState);
PDMBOTHCBDECL(uint32_t) vnetGetHostMinimalFeatures(void *pState);
PDMBOTHCBDECL(void)     vnetSetHostFeatures(void *pState, uint32_t uFeatures);
PDMBOTHCBDECL(int)      vnetGetConfig(void *pState, uint32_t port, uint32_t cb, void *data);
PDMBOTHCBDECL(int)      vnetSetConfig(void *pState, uint32_t port, uint32_t cb, void *data);
PDMBOTHCBDECL(void)     vnetReset(void *pState);
PDMBOTHCBDECL(void)     vnetReady(void *pState);
RT_C_DECLS_END

/*****************************************************************************/

#ifndef VBOX_DEVICE_STRUCT_TESTCASE

#ifdef IN_RING3
const struct VirtioPCIDevices
{
    uint16_t    uPCIVendorId;
    uint16_t    uPCIDeviceId;
    uint16_t    uPCISubsystemVendorId;
    uint16_t    uPCISubsystemId;
    uint16_t    uPCIClass;
    unsigned    nQueues;
    const char *pcszName;
    const char *pcszNameFmt;
    uint32_t  (*pfnGetHostFeatures)(void *pvState);
    uint32_t  (*pfnGetHostMinimalFeatures)(void *pvState);
    void      (*pfnSetHostFeatures)(void *pvState, uint32_t uFeatures);
    int       (*pfnGetConfig)(void *pvState, uint32_t port, uint32_t cb, void *data);
    int       (*pfnSetConfig)(void *pvState, uint32_t port, uint32_t cb, void *data);
    void      (*pfnReset)(void *pvState);
    void      (*pfnReady)(void *pvState);
} g_VPCIDevices[] =
{
    /*  Vendor  Device SSVendor SubSys             Class   NQ Name          Instance */
    { /* Virtio Network Device */
        0x1AF4, 0x1000, 0x1AF4, 1 + VIRTIO_NET_ID, 0x0200, VNET_NQUEUES,
                                                              "virtio-net", "vnet%d",
        vnetGetHostFeatures, vnetGetHostMinimalFeatures, vnetSetHostFeatures,
        vnetGetConfig, vnetSetConfig, vnetReset, vnetReady
    },
    { /* Virtio Block Device */
        0x1AF4, 0x1001, 0x1AF4, 1 + VIRTIO_BLK_ID, 0x0180, 2, "virtio-blk", "vblk%d",
        NULL, NULL, NULL, NULL, NULL, NULL, NULL
    },
};
#endif

#endif /* VBOX_DEVICE_STRUCT_TESTCASE */

/*****************************************************************************/

#define VPCI_HOST_FEATURES  0x0
#define VPCI_GUEST_FEATURES 0x4
#define VPCI_QUEUE_PFN      0x8
#define VPCI_QUEUE_NUM      0xC
#define VPCI_QUEUE_SEL      0xE
#define VPCI_QUEUE_NOTIFY   0x10
#define VPCI_STATUS         0x12
#define VPCI_ISR            0x13
#define VPCI_ISR_QUEUE      0x1
#define VPCI_ISR_CONFIG     0x3
#define VPCI_CONFIG         0x14

#define VPCI_STATUS_ACK     0x01
#define VPCI_STATUS_DRV     0x02
#define VPCI_STATUS_DRV_OK  0x04
#define VPCI_STATUS_FAILED  0x80

/** @todo use+extend RTNETIPV4 */

/** @todo use+extend RTNETTCP */

#ifndef VBOX_DEVICE_STRUCT_TESTCASE

/* Forward declarations ******************************************************/
RT_C_DECLS_BEGIN
PDMBOTHCBDECL(int) vpciIOPortIn (PPDMDEVINS pDevIns, void *pvUser, RTIOPORT port, uint32_t *pu32, unsigned cb);
PDMBOTHCBDECL(int) vpciIOPortOut(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT port, uint32_t u32, unsigned cb);
PDMBOTHCBDECL(int) vpciRaiseInterrupt(VPCISTATE *pState, int rcBusy, uint8_t u8IntCause);
RT_C_DECLS_END


static void vqueueReset(PVQUEUE pQueue)
{
    pQueue->VRing.addrDescriptors = 0;
    pQueue->VRing.addrAvail       = 0;
    pQueue->VRing.addrUsed        = 0;
    pQueue->uNextAvailIndex       = 0;
    pQueue->uNextUsedIndex        = 0;
    pQueue->uPageNumber           = 0;
}

static void vqueueInit(PVQUEUE pQueue, uint32_t uPageNumber)
{
    pQueue->VRing.addrDescriptors = uPageNumber << PAGE_SHIFT;
    pQueue->VRing.addrAvail       = pQueue->VRing.addrDescriptors
        + sizeof(VRINGDESC) * pQueue->VRing.uSize;
    pQueue->VRing.addrUsed        = RT_ALIGN(
        pQueue->VRing.addrAvail + RT_OFFSETOF(VRINGAVAIL, auRing[pQueue->VRing.uSize]),
        PAGE_SIZE); /* The used ring must start from the next page. */
    pQueue->uNextAvailIndex       = 0;
    pQueue->uNextUsedIndex        = 0;
}

uint16_t vringReadAvailIndex(PVPCISTATE pState, PVRING pVRing)
{
    uint16_t tmp;

    PDMDevHlpPhysRead(pState->CTX_SUFF(pDevIns),
                      pVRing->addrAvail + RT_OFFSETOF(VRINGAVAIL, uNextFreeIndex),
                      &tmp, sizeof(tmp));
    return tmp;
}

DECLINLINE(bool) vqueueIsReady(PVPCISTATE pState, PVQUEUE pQueue)
{
    return !!pQueue->VRing.addrAvail;
}

DECLINLINE(bool) vqueueIsEmpty(PVPCISTATE pState, PVQUEUE pQueue)
{
    return (vringReadAvailIndex(pState, &pQueue->VRing) == pQueue->uNextAvailIndex);
}

void vqueueElemFree(PVQUEUEELEM pElem)
{
}

void vringReadDesc(PVPCISTATE pState, PVRING pVRing, uint32_t uIndex, PVRINGDESC pDesc)
{
    //Log(("%s vringReadDesc: ring=%p idx=%u\n", INSTANCE(pState), pVRing, uIndex));
    PDMDevHlpPhysRead(pState->CTX_SUFF(pDevIns),
                      pVRing->addrDescriptors + sizeof(VRINGDESC) * (uIndex % pVRing->uSize),
                      pDesc, sizeof(VRINGDESC));
}

uint16_t vringReadAvail(PVPCISTATE pState, PVRING pVRing, uint32_t uIndex)
{
    uint16_t tmp;

    PDMDevHlpPhysRead(pState->CTX_SUFF(pDevIns),
                      pVRing->addrAvail + RT_OFFSETOF(VRINGAVAIL, auRing[uIndex % pVRing->uSize]),
                      &tmp, sizeof(tmp));
    return tmp;
}

uint16_t vringReadAvailFlags(PVPCISTATE pState, PVRING pVRing)
{
    uint16_t tmp;

    PDMDevHlpPhysRead(pState->CTX_SUFF(pDevIns),
                      pVRing->addrAvail + RT_OFFSETOF(VRINGAVAIL, uFlags),
                      &tmp, sizeof(tmp));
    return tmp;
}

DECLINLINE(void) vringSetNotification(PVPCISTATE pState, PVRING pVRing, bool fEnabled)
{
    uint16_t tmp;

    PDMDevHlpPhysRead(pState->CTX_SUFF(pDevIns),
                      pVRing->addrUsed + RT_OFFSETOF(VRINGUSED, uFlags),
                      &tmp, sizeof(tmp));

    if (fEnabled)
        tmp &= ~ VRINGUSED_F_NO_NOTIFY;
    else
        tmp |= VRINGUSED_F_NO_NOTIFY;

    PDMDevHlpPhysWrite(pState->CTX_SUFF(pDevIns),
                       pVRing->addrUsed + RT_OFFSETOF(VRINGUSED, uFlags),
                       &tmp, sizeof(tmp));
}

bool vqueueGet(PVPCISTATE pState, PVQUEUE pQueue, PVQUEUEELEM pElem)
{
    if (vqueueIsEmpty(pState, pQueue))
        return false;

    pElem->nIn = pElem->nOut = 0;

    Log2(("%s vqueueGet: %s avail_idx=%u\n", INSTANCE(pState),
          QUEUENAME(pState, pQueue), pQueue->uNextAvailIndex));

    VRINGDESC desc;
    uint16_t  idx = vringReadAvail(pState, &pQueue->VRing, pQueue->uNextAvailIndex++);
    pElem->uIndex = idx;
    do
    {
        VQUEUESEG *pSeg;

        vringReadDesc(pState, &pQueue->VRing, idx, &desc);
        if (desc.u16Flags & VRINGDESC_F_WRITE)
        {
            Log2(("%s vqueueGet: %s IN  seg=%u desc_idx=%u addr=%p cb=%u\n", INSTANCE(pState),
                  QUEUENAME(pState, pQueue), pElem->nIn, idx, desc.u64Addr, desc.uLen));
            pSeg = &pElem->aSegsIn[pElem->nIn++];
        }
        else
        {
            Log2(("%s vqueueGet: %s OUT seg=%u desc_idx=%u addr=%p cb=%u\n", INSTANCE(pState),
                  QUEUENAME(pState, pQueue), pElem->nOut, idx, desc.u64Addr, desc.uLen));
            pSeg = &pElem->aSegsOut[pElem->nOut++];
        }

        pSeg->addr = desc.u64Addr;
        pSeg->cb   = desc.uLen;
        pSeg->pv   = NULL;

        idx = desc.u16Next;
    } while (desc.u16Flags & VRINGDESC_F_NEXT);

    Log2(("%s vqueueGet: %s head_desc_idx=%u nIn=%u nOut=%u\n", INSTANCE(pState),
          QUEUENAME(pState, pQueue), pElem->uIndex, pElem->nIn, pElem->nOut));
    return true;
}

uint16_t vringReadUsedIndex(PVPCISTATE pState, PVRING pVRing)
{
    uint16_t tmp;
    PDMDevHlpPhysRead(pState->CTX_SUFF(pDevIns),
                      pVRing->addrUsed + RT_OFFSETOF(VRINGUSED, uIndex),
                      &tmp, sizeof(tmp));
    return tmp;
}

void vringWriteUsedIndex(PVPCISTATE pState, PVRING pVRing, uint16_t u16Value)
{
    PDMDevHlpPhysWrite(pState->CTX_SUFF(pDevIns),
                       pVRing->addrUsed + RT_OFFSETOF(VRINGUSED, uIndex),
                       &u16Value, sizeof(u16Value));
}

void vringWriteUsedElem(PVPCISTATE pState, PVRING pVRing, uint32_t uIndex, uint32_t uId, uint32_t uLen)
{
    VRINGUSEDELEM elem;

    elem.uId = uId;
    elem.uLen = uLen;
    PDMDevHlpPhysWrite(pState->CTX_SUFF(pDevIns),
                       pVRing->addrUsed + RT_OFFSETOF(VRINGUSED, aRing[uIndex % pVRing->uSize]),
                       &elem, sizeof(elem));
}

void vqueuePut(PVPCISTATE pState, PVQUEUE pQueue, PVQUEUEELEM pElem, uint32_t uLen)
{
    unsigned int i, uOffset;

    Log2(("%s vqueuePut: %s desc_idx=%u acb=%u\n", INSTANCE(pState),
          QUEUENAME(pState, pQueue), pElem->uIndex, uLen));
    for (i = uOffset = 0; i < pElem->nIn && uOffset < uLen; i++)
    {
        uint32_t cbSegLen = RT_MIN(uLen - uOffset, pElem->aSegsIn[i].cb);
        if (pElem->aSegsIn[i].pv)
        {
            Log2(("%s vqueuePut: %s used_idx=%u seg=%u addr=%p pv=%p cb=%u acb=%u\n", INSTANCE(pState),
                  QUEUENAME(pState, pQueue), pQueue->uNextUsedIndex, i, pElem->aSegsIn[i].addr, pElem->aSegsIn[i].pv, pElem->aSegsIn[i].cb, cbSegLen));
            PDMDevHlpPhysWrite(pState->CTX_SUFF(pDevIns), pElem->aSegsIn[i].addr,
                               pElem->aSegsIn[i].pv, cbSegLen);
        }
        uOffset += cbSegLen;
    }

    Log2(("%s vqueuePut: %s used_idx=%u guest_used_idx=%u id=%u len=%u\n", INSTANCE(pState),
          QUEUENAME(pState, pQueue), pQueue->uNextUsedIndex, vringReadUsedIndex(pState, &pQueue->VRing), pElem->uIndex, uLen));
    vringWriteUsedElem(pState, &pQueue->VRing, pQueue->uNextUsedIndex++, pElem->uIndex, uLen);
}

void vqueueNotify(PVPCISTATE pState, PVQUEUE pQueue)
{
    LogFlow(("%s vqueueNotify: %s availFlags=%x guestFeatures=%x vqueue is %sempty\n",
             INSTANCE(pState), QUEUENAME(pState, pQueue),
             vringReadAvailFlags(pState, &pQueue->VRing),
             pState->uGuestFeatures, vqueueIsEmpty(pState, pQueue)?"":"not "));
    if (!(vringReadAvailFlags(pState, &pQueue->VRing) & VRINGAVAIL_F_NO_INTERRUPT)
        || ((pState->uGuestFeatures & VPCI_F_NOTIFY_ON_EMPTY) && vqueueIsEmpty(pState, pQueue)))
    {
        int rc = vpciRaiseInterrupt(pState, VERR_INTERNAL_ERROR, VPCI_ISR_QUEUE);
        if (RT_FAILURE(rc))
            Log(("%s vqueueNotify: Failed to raise an interrupt (%Vrc).\n", INSTANCE(pState), rc));
    }
}

void vqueueSync(PVPCISTATE pState, PVQUEUE pQueue)
{
    Log2(("%s vqueueSync: %s old_used_idx=%u new_used_idx=%u\n", INSTANCE(pState),
          QUEUENAME(pState, pQueue), vringReadUsedIndex(pState, &pQueue->VRing), pQueue->uNextUsedIndex));
    vringWriteUsedIndex(pState, &pQueue->VRing, pQueue->uNextUsedIndex);
    vqueueNotify(pState, pQueue);
}

#ifdef IN_RING3
void vpciReset(PVPCISTATE pState)
{
    pState->uGuestFeatures = 0;
    pState->uQueueSelector = 0;
    pState->uStatus        = 0;
    pState->uISR           = 0;

    for (unsigned i = 0; i < pState->nQueues; i++)
        vqueueReset(&pState->Queues[i]);
}
#endif


DECLINLINE(int) vpciCsEnter(VPCISTATE *pState, int iBusyRc)
{
    return PDMCritSectEnter(&pState->cs, iBusyRc);
}

DECLINLINE(void) vpciCsLeave(VPCISTATE *pState)
{
    PDMCritSectLeave(&pState->cs);
}

/**
 * Raise interrupt.
 *
 * @param   pState      The device state structure.
 * @param   rcBusy      Status code to return when the critical section is busy.
 * @param   u8IntCause  Interrupt cause bit mask to set in PCI ISR port.
 */
PDMBOTHCBDECL(int) vpciRaiseInterrupt(VPCISTATE *pState, int rcBusy, uint8_t u8IntCause)
{
    int rc = vpciCsEnter(pState, rcBusy);
    if (RT_UNLIKELY(rc != VINF_SUCCESS))
        return rc;

    LogFlow(("%s vpciRaiseInterrupt: u8IntCause=%x\n",
             INSTANCE(pState), u8IntCause));

    pState->uISR |= u8IntCause;
    PDMDevHlpPCISetIrq(pState->CTX_SUFF(pDevIns), 0, 1);
    vpciCsLeave(pState);
    return VINF_SUCCESS;
}

/**
 * Lower interrupt.
 *
 * @param   pState      The device state structure.
 */
PDMBOTHCBDECL(void) vpciLowerInterrupt(VPCISTATE *pState)
{
    LogFlow(("%s vpciLowerInterrupt\n", INSTANCE(pState)));
    PDMDevHlpPCISetIrq(pState->CTX_SUFF(pDevIns), 0, 0);
}

#ifdef IN_RING3
DECLINLINE(uint32_t) vpciGetHostFeatures(PVPCISTATE pState)
{
    return g_VPCIDevices[pState->enmDevType].pfnGetHostFeatures(pState)
        | VPCI_F_NOTIFY_ON_EMPTY;
}
#endif

/**
 * Port I/O Handler for IN operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      Pointer to the device state structure.
 * @param   port        Port number used for the IN operation.
 * @param   pu32        Where to store the result.
 * @param   cb          Number of bytes read.
 * @thread  EMT
 */
PDMBOTHCBDECL(int) vpciIOPortIn(PPDMDEVINS pDevIns, void *pvUser,
                                RTIOPORT port, uint32_t *pu32, unsigned cb)
{
    VPCISTATE  *pState = PDMINS_2_DATA(pDevIns, VPCISTATE *);
    int         rc     = VINF_SUCCESS;
    const char *szInst = INSTANCE(pState);
    STAM_PROFILE_ADV_START(&pState->CTXSUFF(StatIORead), a);

    port -= pState->addrIOPort;
    switch (port)
    {
        case VPCI_HOST_FEATURES:
            /* Tell the guest what features we support. */
#ifdef IN_RING3
            *pu32 = vpciGetHostFeatures(pState) | VPCI_F_BAD_FEATURE;
#else
            rc = VINF_IOM_HC_IOPORT_READ;
#endif
            break;

        case VPCI_GUEST_FEATURES:
            *pu32 = pState->uGuestFeatures;
            break;

        case VPCI_QUEUE_PFN:
            *pu32 = pState->Queues[pState->uQueueSelector].uPageNumber;
            break;

        case VPCI_QUEUE_NUM:
            Assert(cb == 2);
            *(uint16_t*)pu32 = pState->Queues[pState->uQueueSelector].VRing.uSize;
            break;

        case VPCI_QUEUE_SEL:
            Assert(cb == 2);
            *(uint16_t*)pu32 = pState->uQueueSelector;
            break;

        case VPCI_STATUS:
            Assert(cb == 1);
            *(uint8_t*)pu32 = pState->uStatus;
            break;

        case VPCI_ISR:
            Assert(cb == 1);
            *(uint8_t*)pu32 = pState->uISR;
            pState->uISR = 0; /* read clears all interrupts */
            vpciLowerInterrupt(pState);
            break;

        default:
            if (port >= VPCI_CONFIG)
            {
#ifdef IN_RING3
                rc = g_VPCIDevices[pState->enmDevType].pfnGetConfig(pState, port - VPCI_CONFIG, cb, pu32);
#else
                rc = VINF_IOM_HC_IOPORT_READ;
#endif
            }
            else
            {
                *pu32 = 0xFFFFFFFF;
                rc = PDMDeviceDBGFStop(pDevIns, RT_SRC_POS, "%s virtioIOPortIn: no valid port at offset port=%RTiop cb=%08x\n", szInst, port, cb);
            }
            break;
    }
    Log3(("%s virtioIOPortIn:  At %RTiop in  %0*x\n", szInst, port, cb*2, *pu32));
    STAM_PROFILE_ADV_STOP(&pState->CTXSUFF(StatIORead), a);
    return rc;
}


/**
 * Port I/O Handler for OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   Port        Port number used for the IN operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 * @thread  EMT
 */
PDMBOTHCBDECL(int) vpciIOPortOut(PPDMDEVINS pDevIns, void *pvUser,
                                 RTIOPORT port, uint32_t u32, unsigned cb)
{
    VPCISTATE  *pState = PDMINS_2_DATA(pDevIns, VPCISTATE *);
    int         rc     = VINF_SUCCESS;
    const char *szInst = INSTANCE(pState);
    bool        fHasBecomeReady;
    STAM_PROFILE_ADV_START(&pState->CTXSUFF(StatIOWrite), a);

    port -= pState->addrIOPort;
    Log3(("%s virtioIOPortOut: At %RTiop out          %0*x\n", szInst, port, cb*2, u32));

    switch (port)
    {
        case VPCI_GUEST_FEATURES:
            /* Check if the guest negotiates properly, fall back to basics if it does not. */
#ifdef IN_RING3
            if (VPCI_F_BAD_FEATURE & u32)
            {
                Log(("%s WARNING! Guest failed to negotiate properly (guest=%x)\n",
                     INSTANCE(pState), u32));
                pState->uGuestFeatures = g_VPCIDevices[pState->enmDevType].pfnGetHostMinimalFeatures(pState);
            }
            /* The guest may potentially desire features we don't support! */
            else if (~vpciGetHostFeatures(pState) & u32)
            {
                Log(("%s Guest asked for features host does not support! (host=%x guest=%x)\n",
                     INSTANCE(pState), vpciGetHostFeatures(pState), u32));
                pState->uGuestFeatures = vpciGetHostFeatures(pState);
            }
            else
                pState->uGuestFeatures = u32;
            g_VPCIDevices[pState->enmDevType].pfnSetHostFeatures(pState, pState->uGuestFeatures);
#else
            rc = VINF_IOM_HC_IOPORT_WRITE;
#endif
            break;

        case VPCI_QUEUE_PFN:
#ifdef IN_RING3
            /*
             * The guest is responsible for allocating the pages for queues,
             * here it provides us with the page number of descriptor table.
             * Note that we provide the size of the queue to the guest via
             * VIRTIO_PCI_QUEUE_NUM.
             */
            pState->Queues[pState->uQueueSelector].uPageNumber = u32;
            if (u32)
                vqueueInit(&pState->Queues[pState->uQueueSelector], u32);
            else
                g_VPCIDevices[pState->enmDevType].pfnReset(pState);
#else
            rc = VINF_IOM_HC_IOPORT_WRITE;
#endif
            break;

        case VPCI_QUEUE_SEL:
            Assert(cb == 2);
            u32 &= 0xFFFF;
            if (u32 < pState->nQueues)
                pState->uQueueSelector = u32;
            else
                Log3(("%s virtioIOPortOut: Invalid queue selector %08x\n", szInst, u32));
            break;

        case VPCI_QUEUE_NOTIFY:
#ifdef IN_RING3
            Assert(cb == 2);
            u32 &= 0xFFFF;
            if (u32 < pState->nQueues)
                if (pState->Queues[u32].VRing.addrDescriptors)
                    pState->Queues[u32].pfnCallback(pState, &pState->Queues[u32]);
                else
                    Log(("%s The queue (#%d) being notified has not been initialized.\n",
                         INSTANCE(pState), u32));
            else
                Log(("%s Invalid queue number (%d)\n", INSTANCE(pState), u32));
#else
            rc = VINF_IOM_HC_IOPORT_WRITE;
#endif
            break;

        case VPCI_STATUS:
#ifdef IN_RING3
            Assert(cb == 1);
            u32 &= 0xFF;
            fHasBecomeReady = !(pState->uStatus & VPCI_STATUS_DRV_OK) && (u32 & VPCI_STATUS_DRV_OK);
            pState->uStatus = u32;
            /* Writing 0 to the status port triggers device reset. */
            if (u32 == 0)
                g_VPCIDevices[pState->enmDevType].pfnReset(pState);
            else if (fHasBecomeReady)
                g_VPCIDevices[pState->enmDevType].pfnReady(pState);
#else
            rc = VINF_IOM_HC_IOPORT_WRITE;
#endif
            break;

        default:
#ifdef IN_RING3
            if (port >= VPCI_CONFIG)
                rc = g_VPCIDevices[pState->enmDevType].pfnSetConfig(pState, port - VPCI_CONFIG, cb, &u32);
            else
                rc = PDMDeviceDBGFStop(pDevIns, RT_SRC_POS, "%s virtioIOPortOut: no valid port at offset port=%RTiop cb=%08x\n", szInst, port, cb);
#else
            rc = VINF_IOM_HC_IOPORT_WRITE;
#endif
            break;
    }

    STAM_PROFILE_ADV_STOP(&pState->CTXSUFF(StatIOWrite), a);
    return rc;
}

#ifdef IN_RING3
// Common
/**
 * Map PCI I/O region.
 *
 * @return  VBox status code.
 * @param   pPciDev         Pointer to PCI device. Use pPciDev->pDevIns to get the device instance.
 * @param   iRegion         The region number.
 * @param   GCPhysAddress   Physical address of the region. If iType is PCI_ADDRESS_SPACE_IO, this is an
 *                          I/O port, else it's a physical address.
 *                          This address is *NOT* relative to pci_mem_base like earlier!
 * @param   cb              Region size.
 * @param   enmType         One of the PCI_ADDRESS_SPACE_* values.
 * @thread  EMT
 */
static DECLCALLBACK(int) vpciMap(PPCIDEVICE pPciDev, int iRegion,
                                 RTGCPHYS GCPhysAddress, uint32_t cb, PCIADDRESSSPACE enmType)
{
    int       rc;
    VPCISTATE *pState = PDMINS_2_DATA(pPciDev->pDevIns, VPCISTATE*);

    if (enmType != PCI_ADDRESS_SPACE_IO)
    {
        /* We should never get here */
        AssertMsgFailed(("Invalid PCI address space param in map callback"));
        return VERR_INTERNAL_ERROR;
    }

    pState->addrIOPort = (RTIOPORT)GCPhysAddress;
    rc = PDMDevHlpIOPortRegister(pPciDev->pDevIns, pState->addrIOPort, cb, 0,
                                 vpciIOPortOut, vpciIOPortIn, NULL, NULL, "VirtioNet");
#ifdef VNET_GC_SUPPORT
    AssertRCReturn(rc, rc);
    rc = PDMDevHlpIOPortRegisterR0(pPciDev->pDevIns, pState->addrIOPort, cb, 0,
                                   "vpciIOPortOut", "vpciIOPortIn", NULL, NULL, "VirtioNet");
    AssertRCReturn(rc, rc);
    rc = PDMDevHlpIOPortRegisterGC(pPciDev->pDevIns, pState->addrIOPort, cb, 0,
                                   "vpciIOPortOut", "vpciIOPortIn", NULL, NULL, "VirtioNet");
#endif
    AssertRC(rc);
    return rc;
}

/**
 * Provides interfaces to the driver.
 *
 * @returns Pointer to interface. NULL if the interface is not supported.
 * @param   pInterface          Pointer to this interface structure.
 * @param   enmInterface        The requested interface identification.
 * @thread  EMT
 */
static DECLCALLBACK(void *) vpciQueryInterface(struct PDMIBASE *pInterface, PDMINTERFACE enmInterface)
{
    VPCISTATE *pState = IFACE_TO_STATE(pInterface, IBase);
    Assert(&pState->IBase == pInterface);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pState->IBase;
        case PDMINTERFACE_LED_PORTS:
            return &pState->ILeds;
        default:
            return NULL;
    }
}

/**
 * Gets the pointer to the status LED of a unit.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the interface structure.
 * @param   iLUN            The unit which status LED we desire.
 * @param   ppLed           Where to store the LED pointer.
 * @thread  EMT
 */
static DECLCALLBACK(int) vpciQueryStatusLed(PPDMILEDPORTS pInterface, unsigned iLUN, PPDMLED *ppLed)
{
    VPCISTATE *pState = IFACE_TO_STATE(pInterface, ILeds);
    int        rc     = VERR_PDM_LUN_NOT_FOUND;

    if (iLUN == 0)
    {
        *ppLed = &pState->led;
        rc     = VINF_SUCCESS;
    }
    return rc;
}

/**
 * Turns on/off the write status LED.
 *
 * @returns VBox status code.
 * @param   pState          Pointer to the device state structure.
 * @param   fOn             New LED state.
 */
void vpciSetWriteLed(PVPCISTATE pState, bool fOn)
{
    LogFlow(("%s vpciSetWriteLed: %s\n", INSTANCE(pState), fOn?"on":"off"));
    if (fOn)
        pState->led.Asserted.s.fWriting = pState->led.Actual.s.fWriting = 1;
    else
        pState->led.Actual.s.fWriting = fOn;
}

/**
 * Turns on/off the read status LED.
 *
 * @returns VBox status code.
 * @param   pState          Pointer to the device state structure.
 * @param   fOn             New LED state.
 */
void vpciSetReadLed(PVPCISTATE pState, bool fOn)
{
    LogFlow(("%s vpciSetReadLed: %s\n", INSTANCE(pState), fOn?"on":"off"));
    if (fOn)
        pState->led.Asserted.s.fReading = pState->led.Actual.s.fReading = 1;
    else
        pState->led.Actual.s.fReading = fOn;
}

/**
 * Sets 8-bit register in PCI configuration space.
 * @param   refPciDev   The PCI device.
 * @param   uOffset     The register offset.
 * @param   u16Value    The value to store in the register.
 * @thread  EMT
 */
DECLINLINE(void) vpciCfgSetU8(PCIDEVICE& refPciDev, uint32_t uOffset, uint8_t u8Value)
{
    Assert(uOffset < sizeof(refPciDev.config));
    refPciDev.config[uOffset] = u8Value;
}

/**
 * Sets 16-bit register in PCI configuration space.
 * @param   refPciDev   The PCI device.
 * @param   uOffset     The register offset.
 * @param   u16Value    The value to store in the register.
 * @thread  EMT
 */
DECLINLINE(void) vpciCfgSetU16(PCIDEVICE& refPciDev, uint32_t uOffset, uint16_t u16Value)
{
    Assert(uOffset+sizeof(u16Value) <= sizeof(refPciDev.config));
    *(uint16_t*)&refPciDev.config[uOffset] = u16Value;
}

/**
 * Sets 32-bit register in PCI configuration space.
 * @param   refPciDev   The PCI device.
 * @param   uOffset     The register offset.
 * @param   u32Value    The value to store in the register.
 * @thread  EMT
 */
DECLINLINE(void) vpciCfgSetU32(PCIDEVICE& refPciDev, uint32_t uOffset, uint32_t u32Value)
{
    Assert(uOffset+sizeof(u32Value) <= sizeof(refPciDev.config));
    *(uint32_t*)&refPciDev.config[uOffset] = u32Value;
}


#ifdef DEBUG
static void vpciDumpState(PVPCISTATE pState, const char *pcszCaller)
{
    Log2(("vpciDumpState: (called from %s)\n"
          "  uGuestFeatures = 0x%08x\n"
          "  uQueueSelector = 0x%04x\n"
          "  uStatus        = 0x%02x\n"
          "  uISR           = 0x%02x\n",
          pcszCaller,
          pState->uGuestFeatures,
          pState->uQueueSelector,
          pState->uStatus,
          pState->uISR));

    for (unsigned i = 0; i < pState->nQueues; i++)
        Log2((" %s queue:\n"
              "  VRing.uSize           = %u\n"
              "  VRing.addrDescriptors = %p\n"
              "  VRing.addrAvail       = %p\n"
              "  VRing.addrUsed        = %p\n"
              "  uNextAvailIndex       = %u\n"
              "  uNextUsedIndex        = %u\n"
              "  uPageNumber           = %x\n",
              pState->Queues[i].pcszName,
              pState->Queues[i].VRing.uSize,
              pState->Queues[i].VRing.addrDescriptors,
              pState->Queues[i].VRing.addrAvail,
              pState->Queues[i].VRing.addrUsed,
              pState->Queues[i].uNextAvailIndex,
              pState->Queues[i].uNextUsedIndex,
              pState->Queues[i].uPageNumber));
}
#else
# define vpciDumpState(x, s)  do {} while (0)
#endif

/**
 * Saves the state of device.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSM        The handle to the saved state.
 */
static DECLCALLBACK(int) vpciSaveExec(PVPCISTATE pState, PSSMHANDLE pSSM)
{
    int rc;

    vpciDumpState(pState, "vpciSaveExec");

    rc = SSMR3PutU32(pSSM, pState->uGuestFeatures);
    AssertRCReturn(rc, rc);
    rc = SSMR3PutU16(pSSM, pState->uQueueSelector);
    AssertRCReturn(rc, rc);
    rc = SSMR3PutU8( pSSM, pState->uStatus);
    AssertRCReturn(rc, rc);
    rc = SSMR3PutU8( pSSM, pState->uISR);
    AssertRCReturn(rc, rc);

    /* Save queue states */
    rc = SSMR3PutU32(pSSM, pState->nQueues);
    AssertRCReturn(rc, rc);
    for (unsigned i = 0; i < pState->nQueues; i++)
    {
        rc = SSMR3PutU16(pSSM, pState->Queues[i].VRing.uSize);
        AssertRCReturn(rc, rc);
        rc = SSMR3PutU32(pSSM, pState->Queues[i].uPageNumber);
        AssertRCReturn(rc, rc);
        rc = SSMR3PutU16(pSSM, pState->Queues[i].uNextAvailIndex);
        AssertRCReturn(rc, rc);
        rc = SSMR3PutU16(pSSM, pState->Queues[i].uNextUsedIndex);
        AssertRCReturn(rc, rc);
    }

    return VINF_SUCCESS;
}

/**
 * Loads a saved device state.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSM        The handle to the saved state.
 * @param   uVersion    The data unit version number.
 * @param   uPass       The data pass.
 */
static DECLCALLBACK(int) vpciLoadExec(PVPCISTATE pState, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    int rc;

    if (uPass == SSM_PASS_FINAL)
    {
        /* Restore state data */
        rc = SSMR3GetU32(pSSM, &pState->uGuestFeatures);
        AssertRCReturn(rc, rc);
        rc = SSMR3GetU16(pSSM, &pState->uQueueSelector);
        AssertRCReturn(rc, rc);
        rc = SSMR3GetU8( pSSM, &pState->uStatus);
        AssertRCReturn(rc, rc);
        rc = SSMR3GetU8( pSSM, &pState->uISR);
        AssertRCReturn(rc, rc);

        /* Restore queues */
        if (uVersion > VIRTIO_SAVEDSTATE_VERSION_3_1_BETA1)
        {
            rc = SSMR3GetU32(pSSM, &pState->nQueues);
            AssertRCReturn(rc, rc);
        }
        else
            pState->nQueues = g_VPCIDevices[pState->enmDevType].nQueues;
        for (unsigned i = 0; i < pState->nQueues; i++)
        {
            rc = SSMR3GetU16(pSSM, &pState->Queues[i].VRing.uSize);
            AssertRCReturn(rc, rc);
            rc = SSMR3GetU32(pSSM, &pState->Queues[i].uPageNumber);
            AssertRCReturn(rc, rc);

            if (pState->Queues[i].uPageNumber)
                vqueueInit(&pState->Queues[i], pState->Queues[i].uPageNumber);

            rc = SSMR3GetU16(pSSM, &pState->Queues[i].uNextAvailIndex);
            AssertRCReturn(rc, rc);
            rc = SSMR3GetU16(pSSM, &pState->Queues[i].uNextUsedIndex);
            AssertRCReturn(rc, rc);
        }
    }

    vpciDumpState(pState, "vpciLoadExec");

    return VINF_SUCCESS;
}

/**
 * Set PCI configuration space registers.
 *
 * @param   pci         Reference to PCI device structure.
 * @thread  EMT
 */
static DECLCALLBACK(void) vpciConfigure(PCIDEVICE& pci, VirtioDeviceType enmType)
{
    Assert(enmType < (int)RT_ELEMENTS(g_VPCIDevices));
    /* Configure PCI Device, assume 32-bit mode ******************************/
    PCIDevSetVendorId(&pci, g_VPCIDevices[enmType].uPCIVendorId);
    PCIDevSetDeviceId(&pci, g_VPCIDevices[enmType].uPCIDeviceId);
    vpciCfgSetU16(pci, VBOX_PCI_SUBSYSTEM_VENDOR_ID, g_VPCIDevices[enmType].uPCISubsystemVendorId);
    vpciCfgSetU16(pci, VBOX_PCI_SUBSYSTEM_ID, g_VPCIDevices[enmType].uPCISubsystemId);

    /* ABI version, must be equal 0 as of 2.6.30 kernel. */
    vpciCfgSetU8( pci, VBOX_PCI_REVISION_ID,          0x00);
    /* Ethernet adapter */
    vpciCfgSetU8( pci, VBOX_PCI_CLASS_PROG,           0x00);
    vpciCfgSetU16(pci, VBOX_PCI_CLASS_DEVICE, g_VPCIDevices[enmType].uPCIClass);
    /* Interrupt Pin: INTA# */
    vpciCfgSetU8( pci, VBOX_PCI_INTERRUPT_PIN,        0x01);
}

// TODO: header
DECLCALLBACK(int) vpciConstruct(PPDMDEVINS pDevIns, VPCISTATE *pState,
                                int iInstance, VirtioDeviceType enmDevType,
                                unsigned uConfigSize)
{
    int rc = VINF_SUCCESS;
    /* Init handles and log related stuff. */
    RTStrPrintf(pState->szInstance, sizeof(pState->szInstance), g_VPCIDevices[enmDevType].pcszNameFmt, iInstance);
    pState->enmDevType   = enmDevType;

    pState->pDevInsR3    = pDevIns;
    pState->pDevInsR0    = PDMDEVINS_2_R0PTR(pDevIns);
    pState->pDevInsRC    = PDMDEVINS_2_RCPTR(pDevIns);
    pState->led.u32Magic = PDMLED_MAGIC;

    pState->ILeds.pfnQueryStatusLed = vpciQueryStatusLed;

    /* Initialize critical section. */
    rc = PDMDevHlpCritSectInit(pDevIns, &pState->cs, pState->szInstance);
    if (RT_FAILURE(rc))
        return rc;

    /* Set PCI config registers */
    vpciConfigure(pState->pciDevice, VIRTIO_NET_ID);
    /* Register PCI device */
    rc = PDMDevHlpPCIRegister(pDevIns, &pState->pciDevice);
    if (RT_FAILURE(rc))
        return rc;

    /* Map our ports to IO space. */
    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 0, VPCI_CONFIG + uConfigSize,
                                      PCI_ADDRESS_SPACE_IO, vpciMap);
    if (RT_FAILURE(rc))
        return rc;

    /* Status driver */
    PPDMIBASE pBase;
    rc = PDMDevHlpDriverAttach(pDevIns, PDM_STATUS_LUN, &pState->IBase, &pBase, "Status Port");
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Failed to attach the status LUN"));
    pState->pLedsConnector = (PPDMILEDCONNECTORS)pBase->pfnQueryInterface(pBase, PDMINTERFACE_LED_CONNECTORS);

    pState->nQueues = g_VPCIDevices[pState->enmDevType].nQueues;

#if defined(VBOX_WITH_STATISTICS)
    PDMDevHlpSTAMRegisterF(pDevIns, &pState->StatIOReadGC,           STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL, "Profiling IO reads in GC",           "/Devices/VNet%d/IO/ReadGC", iInstance);
    PDMDevHlpSTAMRegisterF(pDevIns, &pState->StatIOReadHC,           STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL, "Profiling IO reads in HC",           "/Devices/VNet%d/IO/ReadHC", iInstance);
    PDMDevHlpSTAMRegisterF(pDevIns, &pState->StatIOWriteGC,          STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL, "Profiling IO writes in GC",          "/Devices/VNet%d/IO/WriteGC", iInstance);
    PDMDevHlpSTAMRegisterF(pDevIns, &pState->StatIOWriteHC,          STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL, "Profiling IO writes in HC",          "/Devices/VNet%d/IO/WriteHC", iInstance);
    PDMDevHlpSTAMRegisterF(pDevIns, &pState->StatIntsRaised,         STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,     "Number of raised interrupts",        "/Devices/VNet%d/Interrupts/Raised", iInstance);
#endif /* VBOX_WITH_STATISTICS */

    return rc;
}

/**
 * Destruct PCI-related part of device.
 *
 * We need to free non-VM resources only.
 *
 * @returns VBox status.
 * @param   pState      The device state structure.
 */
static DECLCALLBACK(int) vpciDestruct(VPCISTATE* pState)
{
    Log(("%s Destroying PCI instance\n", INSTANCE(pState)));

    if (PDMCritSectIsInitialized(&pState->cs))
        PDMR3CritSectDelete(&pState->cs);
    
    return VINF_SUCCESS;
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
static DECLCALLBACK(void) vpciRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    VPCISTATE* pState = PDMINS_2_DATA(pDevIns, VPCISTATE*);
    pState->pDevInsRC     = PDMDEVINS_2_RCPTR(pDevIns);
    // TBD
}

PVQUEUE vpciAddQueue(VPCISTATE* pState, unsigned uSize,
                     void (*pfnCallback)(void *pvState, PVQUEUE pQueue),
                     const char *pcszName)
{
    PVQUEUE pQueue = NULL;
    /* Find an empty queue slot */
    for (unsigned i = 0; i < pState->nQueues; i++)
    {
        if (pState->Queues[i].VRing.uSize == 0)
        {
            pQueue = &pState->Queues[i];
            break;
        }
    }

    if (!pQueue)
    {
        Log(("%s Too many queues being added, no empty slots available!\n", INSTANCE(pState)));
    }
    else
    {
        pQueue->VRing.uSize = uSize;
        pQueue->VRing.addrDescriptors = 0;
        pQueue->uPageNumber = 0;
        pQueue->pfnCallback = pfnCallback;
        pQueue->pcszName = pcszName;
    }

    return pQueue;
}


#endif /* IN_RING3 */

#endif /* VBOX_DEVICE_STRUCT_TESTCASE */

//------------------------- Tear off here: vnet -------------------------------

//- TODO: Move to VirtioNet.h -------------------------------------------------

#define VNET_TX_DELAY           150   /* 150 microseconds */
#define VNET_MAX_FRAME_SIZE     65536  // TODO: Is it the right limit?

/* Virtio net features */
#define VNET_F_CSUM       0x00000001  /* Host handles pkts w/ partial csum */
#define VNET_F_GUEST_CSUM 0x00000002  /* Guest handles pkts w/ partial csum */
#define VNET_F_MAC        0x00000020  /* Host has given MAC address. */
#define VNET_F_GSO        0x00000040  /* Host handles pkts w/ any GSO type */
#define VNET_F_GUEST_TSO4 0x00000080  /* Guest can handle TSOv4 in. */
#define VNET_F_GUEST_TSO6 0x00000100  /* Guest can handle TSOv6 in. */
#define VNET_F_GUEST_ECN  0x00000200  /* Guest can handle TSO[6] w/ ECN in. */
#define VNET_F_GUEST_UFO  0x00000400  /* Guest can handle UFO in. */
#define VNET_F_HOST_TSO4  0x00000800  /* Host can handle TSOv4 in. */
#define VNET_F_HOST_TSO6  0x00001000  /* Host can handle TSOv6 in. */
#define VNET_F_HOST_ECN   0x00002000  /* Host can handle TSO[6] w/ ECN in. */
#define VNET_F_HOST_UFO   0x00004000  /* Host can handle UFO in. */
#define VNET_F_MRG_RXBUF  0x00008000  /* Host can merge receive buffers. */
#define VNET_F_STATUS     0x00010000  /* virtio_net_config.status available */
#define VNET_F_CTRL_VQ    0x00020000  /* Control channel available */
#define VNET_F_CTRL_RX    0x00040000  /* Control channel RX mode support */
#define VNET_F_CTRL_VLAN  0x00080000  /* Control channel VLAN filtering */

#define VNET_S_LINK_UP    1


#ifdef _MSC_VER
struct VNetPCIConfig
#else /* !_MSC_VER */
struct __attribute__ ((__packed__)) VNetPCIConfig
#endif /* !_MSC_VER */
{
    RTMAC    mac;
    uint16_t uStatus;
};
AssertCompileMemberOffset(struct VNetPCIConfig, uStatus, 6);

/**
 * Device state structure. Holds the current state of device.
 */

struct VNetState_st
{
    /* VPCISTATE must be the first member! */
    VPCISTATE               VPCI;

    PDMINETWORKPORT         INetworkPort;
    PDMINETWORKCONFIG       INetworkConfig;
    R3PTRTYPE(PPDMIBASE)    pDrvBase;                 /**< Attached network driver. */
    R3PTRTYPE(PPDMINETWORKCONNECTOR) pDrv;    /**< Connector of attached network driver. */

    R3PTRTYPE(PPDMQUEUE)    pCanRxQueueR3;           /**< Rx wakeup signaller - R3. */
    R0PTRTYPE(PPDMQUEUE)    pCanRxQueueR0;           /**< Rx wakeup signaller - R0. */
    RCPTRTYPE(PPDMQUEUE)    pCanRxQueueRC;           /**< Rx wakeup signaller - RC. */

#if HC_ARCH_BITS == 64
    uint32_t    padding;
#endif

    /** transmit buffer */
    R3PTRTYPE(uint8_t*)     pTxBuf;
    /**< Link Up(/Restore) Timer. */
    PTMTIMERR3              pLinkUpTimer; 
#ifdef VNET_TX_DELAY
    /**< Transmit Delay Timer - R3. */
    PTMTIMERR3              pTxTimerR3; 
    /**< Transmit Delay Timer - R0. */
    PTMTIMERR0              pTxTimerR0; 
    /**< Transmit Delay Timer - GC. */
    PTMTIMERRC              pTxTimerRC; 
#if HC_ARCH_BITS == 64
    uint32_t    padding2;
#endif

#endif /* VNET_TX_DELAY */

    /** PCI config area holding MAC address as well as TBD. */
    struct VNetPCIConfig    config;
    /** MAC address obtained from the configuration. */
    RTMAC                   macConfigured;
    /** True if physical cable is attached in configuration. */
    bool                    fCableConnected;

    /** Number of packet being sent/received to show in debug log. */
    uint32_t                u32PktNo;

    /** Locked state -- no state alteration possible. */
    bool                    fLocked;

    /** N/A: */
    bool volatile           fMaybeOutOfSpace;

    R3PTRTYPE(PVQUEUE)      pRxQueue;
    R3PTRTYPE(PVQUEUE)      pTxQueue;
    R3PTRTYPE(PVQUEUE)      pCtlQueue;
    /* Receive-blocking-related fields ***************************************/

    /** EMT: Gets signalled when more RX descriptors become available. */
    RTSEMEVENT  hEventMoreRxDescAvail;

    /* Statistic fields ******************************************************/

    STAMCOUNTER             StatReceiveBytes;
    STAMCOUNTER             StatTransmitBytes;
#if defined(VBOX_WITH_STATISTICS)
    STAMPROFILEADV          StatReceive;
    STAMPROFILEADV          StatTransmit;
    STAMPROFILEADV          StatTransmitSend;
    STAMPROFILE             StatRxOverflow;
    STAMCOUNTER             StatRxOverflowWakeup;
#endif /* VBOX_WITH_STATISTICS */

};
typedef struct VNetState_st VNETSTATE;
typedef VNETSTATE *PVNETSTATE;

#ifndef VBOX_DEVICE_STRUCT_TESTCASE

#define VNETHDR_GSO_NONE 0

struct VNetHdr
{
    uint8_t  u8Flags;
    uint8_t  u8GSOType;
    uint16_t u16HdrLen;
    uint16_t u16GSOSize;
    uint16_t u16CSumStart;
    uint16_t u16CSumOffset;
};
typedef struct VNetHdr VNETHDR;
typedef VNETHDR *PVNETHDR;
AssertCompileSize(VNETHDR, 10);

AssertCompileMemberOffset(VNETSTATE, VPCI, 0);

//- TODO: Leave here ----------------------------------------------------------

#undef INSTANCE
#define INSTANCE(pState) pState->VPCI.szInstance
#undef IFACE_TO_STATE
#define IFACE_TO_STATE(pIface, ifaceName) ((VNETSTATE *)((char*)pIface - RT_OFFSETOF(VNETSTATE, ifaceName)))
#define STATUS pState->config.uStatus

DECLINLINE(int) vnetCsEnter(PVNETSTATE pState, int rcBusy)
{
    return vpciCsEnter(&pState->VPCI, rcBusy);
}

DECLINLINE(void) vnetCsLeave(PVNETSTATE pState)
{
    vpciCsLeave(&pState->VPCI);
}


PDMBOTHCBDECL(uint32_t) vnetGetHostFeatures(void *pvState)
{
    // TODO: implement
    return VNET_F_MAC | VNET_F_STATUS;
}

PDMBOTHCBDECL(uint32_t) vnetGetHostMinimalFeatures(void *pvState)
{
    return VNET_F_MAC;
}

PDMBOTHCBDECL(void) vnetSetHostFeatures(void *pvState, uint32_t uFeatures)
{
    // TODO: Nothing to do here yet
    VNETSTATE *pState = (VNETSTATE *)pvState;
    LogFlow(("%s vnetSetHostFeatures: uFeatures=%x\n", INSTANCE(pState), uFeatures));
}

PDMBOTHCBDECL(int) vnetGetConfig(void *pvState, uint32_t port, uint32_t cb, void *data)
{
    VNETSTATE *pState = (VNETSTATE *)pvState;
    if (port + cb > sizeof(struct VNetPCIConfig))
    {
        Log(("%s vnetGetConfig: Read beyond the config structure is attempted (port=%RTiop cb=%x).\n", INSTANCE(pState), port, cb));
        return VERR_INTERNAL_ERROR;
    }
    memcpy(data, ((uint8_t*)&pState->config) + port, cb);
    return VINF_SUCCESS;
}

PDMBOTHCBDECL(int) vnetSetConfig(void *pvState, uint32_t port, uint32_t cb, void *data)
{
    VNETSTATE *pState = (VNETSTATE *)pvState;
    if (port + cb > sizeof(struct VNetPCIConfig))
    {
        Log(("%s vnetGetConfig: Write beyond the config structure is attempted (port=%RTiop cb=%x).\n", INSTANCE(pState), port, cb));
        return VERR_INTERNAL_ERROR;
    }
    memcpy(((uint8_t*)&pState->config) + port, data, cb);
    return VINF_SUCCESS;
}

#ifdef IN_RING3
/**
 * Hardware reset. Revert all registers to initial values.
 *
 * @param   pState      The device state structure.
 */
PDMBOTHCBDECL(void) vnetReset(void *pvState)
{
    VNETSTATE *pState = (VNETSTATE*)pvState;
    Log(("%s Reset triggered\n", INSTANCE(pState)));
    vpciReset(&pState->VPCI);
    // TODO: Implement reset
    if (pState->fCableConnected)
        STATUS = VNET_S_LINK_UP;
    else
        STATUS = 0;
}

/**
 * Wakeup the RX thread.
 */
static void vnetWakeupReceive(PPDMDEVINS pDevIns)
{
    VNETSTATE *pState = PDMINS_2_DATA(pDevIns, VNETSTATE *);
    if (    pState->fMaybeOutOfSpace
        &&  pState->hEventMoreRxDescAvail != NIL_RTSEMEVENT)
    {
        STAM_COUNTER_INC(&pState->StatRxOverflowWakeup);
        Log(("%s Waking up Out-of-RX-space semaphore\n",  INSTANCE(pState)));
        RTSemEventSignal(pState->hEventMoreRxDescAvail);
    }
}

/**
 * Link Up Timer handler.
 *
 * @param   pDevIns     Pointer to device instance structure.
 * @param   pTimer      Pointer to the timer.
 * @param   pvUser      NULL.
 * @thread  EMT
 */
static DECLCALLBACK(void) vnetLinkUpTimer(PPDMDEVINS pDevIns, PTMTIMER pTimer, void *pvUser)
{
    VNETSTATE *pState = (VNETSTATE *)pvUser;

    STATUS |= VNET_S_LINK_UP;
    vpciRaiseInterrupt(&pState->VPCI, VERR_SEM_BUSY, VPCI_ISR_CONFIG);
    vnetWakeupReceive(pDevIns);
}




/**
 * Handler for the wakeup signaller queue.
 */
static DECLCALLBACK(bool) vnetCanRxQueueConsumer(PPDMDEVINS pDevIns, PPDMQUEUEITEMCORE pItem)
{
    vnetWakeupReceive(pDevIns);
    return true;
}

#endif /* IN_RING3 */

/**
 * This function is called when the driver becomes ready.
 *
 * @param   pState      The device state structure.
 */
PDMBOTHCBDECL(void) vnetReady(void *pvState)
{
    VNETSTATE *pState = (VNETSTATE*)pvState;
    Log(("%s Driver became ready, waking up RX thread...\n", INSTANCE(pState)));
#ifdef IN_RING3
    vnetWakeupReceive(pState->VPCI.CTX_SUFF(pDevIns));
#else
    PPDMQUEUEITEMCORE pItem = PDMQueueAlloc(pState->CTX_SUFF(pCanRxQueue));
    if (pItem)
        PDMQueueInsert(pState->CTX_SUFF(pCanRxQueue), pItem);
#endif
}


#ifdef IN_RING3

/**
 * Check if the device can receive data now.
 * This must be called before the pfnRecieve() method is called.
 *
 * @remarks As a side effect this function enables queue notification
 *          if it cannot receive because the queue is empty.
 *          It disables notification if it can receive.
 *
 * @returns VERR_NET_NO_BUFFER_SPACE if it cannot.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @thread  RX
 */
static int vnetCanReceive(VNETSTATE *pState)
{
    int rc = vnetCsEnter(pState, VERR_SEM_BUSY);
    LogFlow(("%s vnetCanReceive\n", INSTANCE(pState)));
    if (!(pState->VPCI.uStatus & VPCI_STATUS_DRV_OK))
        rc = VERR_NET_NO_BUFFER_SPACE;
    else if (!vqueueIsReady(&pState->VPCI, pState->pRxQueue))
        rc = VERR_NET_NO_BUFFER_SPACE;
    else if (vqueueIsEmpty(&pState->VPCI, pState->pRxQueue))
    {
        vringSetNotification(&pState->VPCI, &pState->pRxQueue->VRing, true);
        rc = VERR_NET_NO_BUFFER_SPACE;
    }
    else
    {
        vringSetNotification(&pState->VPCI, &pState->pRxQueue->VRing, false);
        rc = VINF_SUCCESS;
    }

    LogFlow(("%s vnetCanReceive -> %Vrc\n", INSTANCE(pState), rc));
    vnetCsLeave(pState);
    return rc;
}

static DECLCALLBACK(int) vnetWaitReceiveAvail(PPDMINETWORKPORT pInterface, unsigned cMillies)
{
    VNETSTATE *pState = IFACE_TO_STATE(pInterface, INetworkPort);
    LogFlow(("%s vnetWaitReceiveAvail(cMillies=%u)\n", INSTANCE(pState), cMillies));
    int rc = vnetCanReceive(pState);

    if (RT_SUCCESS(rc))
        return VINF_SUCCESS;
    if (RT_UNLIKELY(cMillies == 0))
        return VERR_NET_NO_BUFFER_SPACE;

    rc = VERR_INTERRUPTED;
    ASMAtomicXchgBool(&pState->fMaybeOutOfSpace, true);
    STAM_PROFILE_START(&pState->StatRxOverflow, a);

    VMSTATE enmVMState;
    while (RT_LIKELY(   (enmVMState = PDMDevHlpVMState(pState->VPCI.CTX_SUFF(pDevIns))) == VMSTATE_RUNNING
                     ||  enmVMState == VMSTATE_RUNNING_LS))
    {
        int rc2 = vnetCanReceive(pState);
        if (RT_SUCCESS(rc2))
        {
            rc = VINF_SUCCESS;
            break;
        }
        Log(("%s vnetWaitReceiveAvail: waiting cMillies=%u...\n",
                INSTANCE(pState), cMillies));
        RTSemEventWait(pState->hEventMoreRxDescAvail, cMillies);
    }
    STAM_PROFILE_STOP(&pState->StatRxOverflow, a);
    ASMAtomicXchgBool(&pState->fMaybeOutOfSpace, false);

    LogFlow(("%s vnetWaitReceiveAvail -> %d\n", INSTANCE(pState), rc));
    return rc;
}


/**
 * Provides interfaces to the driver.
 *
 * @returns Pointer to interface. NULL if the interface is not supported.
 * @param   pInterface          Pointer to this interface structure.
 * @param   enmInterface        The requested interface identification.
 * @thread  EMT
 */
static DECLCALLBACK(void *) vnetQueryInterface(struct PDMIBASE *pInterface, PDMINTERFACE enmInterface)
{
    VNETSTATE *pState = IFACE_TO_STATE(pInterface, VPCI.IBase);
    Assert(&pState->VPCI.IBase == pInterface);
    switch (enmInterface)
    {
        case PDMINTERFACE_NETWORK_PORT:
            return &pState->INetworkPort;
        case PDMINTERFACE_NETWORK_CONFIG:
            return &pState->INetworkConfig;
        default:
            return vpciQueryInterface(pInterface, enmInterface);
    }
}

/**
 * Determines if the packet is to be delivered to upper layer. The following
 * filters supported:
 * - Exact Unicast/Multicast
 * - Promiscuous Unicast/Multicast
 * - Multicast
 * - VLAN
 *
 * @returns true if packet is intended for this node.
 * @param   pState          Pointer to the state structure.
 * @param   pvBuf           The ethernet packet.
 * @param   cb              Number of bytes available in the packet.
 */
static bool vnetAddressFilter(PVNETSTATE pState, const void *pvBuf, size_t cb)
{
    return true; // TODO: Implement!
}

/**
 * Pad and store received packet.
 *
 * @remarks Make sure that the packet appears to upper layer as one coming
 *          from real Ethernet: pad it and insert FCS.
 *
 * @returns VBox status code.
 * @param   pState          The device state structure.
 * @param   pvBuf           The available data.
 * @param   cb              Number of bytes available in the buffer.
 * @thread  RX
 */
static int vnetHandleRxPacket(PVNETSTATE pState, const void *pvBuf, size_t cb)
{
    VNETHDR hdr;

    hdr.u8Flags   = 0;
    hdr.u8GSOType = VNETHDR_GSO_NONE;

    unsigned int uOffset = 0;
    for (unsigned int nElem = 0; uOffset < cb; nElem++)
    {
        VQUEUEELEM elem;
        unsigned int nSeg = 0, uElemSize = 0;

        if (!vqueueGet(&pState->VPCI, pState->pRxQueue, &elem))
        {
            Log(("%s vnetHandleRxPacket: Suddenly there is no space in receive queue!\n", INSTANCE(pState)));
            return VERR_INTERNAL_ERROR;
        }

        if (elem.nIn < 1)
        {
            Log(("%s vnetHandleRxPacket: No writable descriptors in receive queue!\n", INSTANCE(pState)));
            return VERR_INTERNAL_ERROR;
        }

        if (nElem == 0)
        {
            /* The very first segment of the very first element gets the header. */
            if (elem.aSegsIn[nSeg].cb != sizeof(VNETHDR))
            {
                Log(("%s vnetHandleRxPacket: The first descriptor does match the header size!\n", INSTANCE(pState)));
                return VERR_INTERNAL_ERROR;
            }

            elem.aSegsIn[nSeg++].pv = &hdr;
            uElemSize += sizeof(VNETHDR);
        }

        while (nSeg < elem.nIn && uOffset < cb)
        {
            unsigned int uSize = RT_MIN(elem.aSegsIn[nSeg].cb, cb - uOffset);
            elem.aSegsIn[nSeg++].pv = (uint8_t*)pvBuf + uOffset;
            uOffset += uSize;
            uElemSize += uSize;
        }
        vqueuePut(&pState->VPCI, pState->pRxQueue, &elem, uElemSize);
    }
    vqueueSync(&pState->VPCI, pState->pRxQueue);

    return VINF_SUCCESS;
}

/**
 * Receive data from the network.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   pvBuf           The available data.
 * @param   cb              Number of bytes available in the buffer.
 * @thread  RX
 */
static DECLCALLBACK(int) vnetReceive(PPDMINETWORKPORT pInterface, const void *pvBuf, size_t cb)
{
    VNETSTATE *pState = IFACE_TO_STATE(pInterface, INetworkPort);
    int        rc = VINF_SUCCESS;

    Log2(("%s vnetReceive: pvBuf=%p cb=%u\n", INSTANCE(pState), pvBuf, cb));
    rc = vnetCanReceive(pState);
    if (RT_FAILURE(rc))
        return rc;

    vpciSetReadLed(&pState->VPCI, true);
    if (vnetAddressFilter(pState, pvBuf, cb))
    {
        rc = vnetHandleRxPacket(pState, pvBuf, cb);
        STAM_REL_COUNTER_ADD(&pState->StatReceiveBytes, cb);
    }
    vpciSetReadLed(&pState->VPCI, false);

    return rc;
}

/**
 * Gets the current Media Access Control (MAC) address.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   pMac            Where to store the MAC address.
 * @thread  EMT
 */
static DECLCALLBACK(int) vnetGetMac(PPDMINETWORKCONFIG pInterface, PRTMAC pMac)
{
    VNETSTATE *pState = IFACE_TO_STATE(pInterface, INetworkConfig);
    memcpy(pMac, pState->config.mac.au8, sizeof(RTMAC));
    return VINF_SUCCESS;
}

/**
 * Gets the new link state.
 *
 * @returns The current link state.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @thread  EMT
 */
static DECLCALLBACK(PDMNETWORKLINKSTATE) vnetGetLinkState(PPDMINETWORKCONFIG pInterface)
{
    VNETSTATE *pState = IFACE_TO_STATE(pInterface, INetworkConfig);
    if (STATUS & VNET_S_LINK_UP)
        return PDMNETWORKLINKSTATE_UP;
    return PDMNETWORKLINKSTATE_DOWN;
}


/**
 * Sets the new link state.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   enmState        The new link state
 */
static DECLCALLBACK(int) vnetSetLinkState(PPDMINETWORKCONFIG pInterface, PDMNETWORKLINKSTATE enmState)
{
    VNETSTATE *pState = IFACE_TO_STATE(pInterface, INetworkConfig);
    bool fOldUp = !!(STATUS & VNET_S_LINK_UP);
    bool fNewUp = enmState == PDMNETWORKLINKSTATE_UP;

    if (fNewUp != fOldUp)
    {
        if (fNewUp)
        {
            Log(("%s Link is up\n", INSTANCE(pState)));
            STATUS |= VNET_S_LINK_UP;
            vpciRaiseInterrupt(&pState->VPCI, VERR_SEM_BUSY, VPCI_ISR_CONFIG);
        }
        else
        {
            Log(("%s Link is down\n", INSTANCE(pState)));
            STATUS &= ~VNET_S_LINK_UP;
            vpciRaiseInterrupt(&pState->VPCI, VERR_SEM_BUSY, VPCI_ISR_CONFIG);
        }
        if (pState->pDrv)
            pState->pDrv->pfnNotifyLinkChanged(pState->pDrv, enmState);
    }
    return VINF_SUCCESS;
}

static DECLCALLBACK(void) vnetQueueReceive(void *pvState, PVQUEUE pQueue)
{
    VNETSTATE *pState = (VNETSTATE*)pvState;
    Log(("%s Receive buffers has been added, waking up receive thread.\n", INSTANCE(pState)));
    vnetWakeupReceive(pState->VPCI.CTX_SUFF(pDevIns));
}

static DECLCALLBACK(void) vnetTransmitPendingPackets(PVNETSTATE pState, PVQUEUE pQueue)
{
    if ((pState->VPCI.uStatus & VPCI_STATUS_DRV_OK) == 0)
    {
        Log(("%s Ignoring transmit requests from non-existent driver (status=0x%x).\n",
             INSTANCE(pState), pState->VPCI.uStatus));
        return;
    }

    vpciSetWriteLed(&pState->VPCI, true);

    VQUEUEELEM elem;
    while (vqueueGet(&pState->VPCI, pQueue, &elem))
    {
        unsigned int uOffset = 0;
        if (elem.nOut < 2 || elem.aSegsOut[0].cb != sizeof(VNETHDR))
        {
            Log(("%s vnetQueueTransmit: The first segment is not the header! (%u < 2 || %u != %u).\n",
                 INSTANCE(pState), elem.nOut, elem.aSegsOut[0].cb, sizeof(VNETHDR)));
            vqueueElemFree(&elem);
            break; /* For now we simply ignore the header, but it must be there anyway! */
        }
        else
        {
            /* Assemble a complete frame. */
            for (unsigned int i = 1; i < elem.nOut && uOffset < VNET_MAX_FRAME_SIZE; i++)
            {
                unsigned int uSize = elem.aSegsOut[i].cb;
                if (uSize > VNET_MAX_FRAME_SIZE - uOffset)
                {
                    Log(("%s vnetQueueTransmit: Packet is too big (>64k), truncating...\n", INSTANCE(pState)));
                    uSize = VNET_MAX_FRAME_SIZE - uOffset;
                }
                PDMDevHlpPhysRead(pState->VPCI.CTX_SUFF(pDevIns), elem.aSegsOut[i].addr,
                                  pState->pTxBuf + uOffset, uSize);
                uOffset += uSize;
            }
            STAM_PROFILE_ADV_START(&pState->StatTransmitSend, a);
            int rc = pState->pDrv->pfnSend(pState->pDrv, pState->pTxBuf, uOffset);
            STAM_PROFILE_ADV_STOP(&pState->StatTransmitSend, a);
            STAM_REL_COUNTER_ADD(&pState->StatTransmitBytes, uOffset);
        }
        vqueuePut(&pState->VPCI, pQueue, &elem, sizeof(VNETHDR) + uOffset);
        vqueueSync(&pState->VPCI, pQueue);
    }
    vpciSetWriteLed(&pState->VPCI, false);
}

#ifdef VNET_TX_DELAY
static DECLCALLBACK(void) vnetQueueTransmit(void *pvState, PVQUEUE pQueue)
{
    VNETSTATE *pState = (VNETSTATE*)pvState;

    if (TMTimerIsActive(pState->CTX_SUFF(pTxTimer)))
    {
        int rc = TMTimerStop(pState->CTX_SUFF(pTxTimer));
        vringSetNotification(&pState->VPCI, &pState->pTxQueue->VRing, true);
        Log3(("%s vnetQueueTransmit: Got kicked with notification disabled, "
              "re-enable notification and flush TX queue\n", INSTANCE(pState)));
        vnetTransmitPendingPackets(pState, pQueue);
    }
    else
    {
        TMTimerSetMicro(pState->CTX_SUFF(pTxTimer), VNET_TX_DELAY);
        vringSetNotification(&pState->VPCI, &pState->pTxQueue->VRing, false);
    }
}

/**
 * Transmit Delay Timer handler.
 *
 * @remarks We only get here when the timer expires.
 *
 * @param   pDevIns     Pointer to device instance structure.
 * @param   pTimer      Pointer to the timer.
 * @param   pvUser      NULL.
 * @thread  EMT
 */
static DECLCALLBACK(void) vnetTxTimer(PPDMDEVINS pDevIns, PTMTIMER pTimer, void *pvUser)
{
    VNETSTATE *pState = (VNETSTATE*)pvUser;

    vringSetNotification(&pState->VPCI, &pState->pTxQueue->VRing, true);
    Log3(("%s vnetTxTimer: Expired, %d packets pending\n", INSTANCE(pState), 
          vringReadAvailIndex(&pState->VPCI, &pState->pTxQueue->VRing) - pState->pTxQueue->uNextAvailIndex));
    vnetTransmitPendingPackets(pState, pState->pTxQueue);
}

#else /* !VNET_TX_DELAY */
static DECLCALLBACK(void) vnetQueueTransmit(void *pvState, PVQUEUE pQueue)
{
    VNETSTATE *pState = (VNETSTATE*)pvState;

    vnetTransmitPendingPackets(pState, pQueue);
}
#endif /* !VNET_TX_DELAY */

static DECLCALLBACK(void) vnetQueueControl(void *pvState, PVQUEUE pQueue)
{
    VNETSTATE *pState = (VNETSTATE*)pvState;
    Log(("%s Pending control message\n", INSTANCE(pState)));
}

/**
 * Saves the configuration.
 *
 * @param   pState      The VNET state.
 * @param   pSSM        The handle to the saved state.
 */
static void vnetSaveConfig(VNETSTATE *pState, PSSMHANDLE pSSM)
{
    SSMR3PutMem(pSSM, &pState->macConfigured, sizeof(pState->macConfigured));
}

/**
 * Live save - save basic configuration.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSM        The handle to the saved state.
 * @param   uPass
 */
static DECLCALLBACK(int) vnetLiveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uPass)
{
    VNETSTATE *pState = PDMINS_2_DATA(pDevIns, VNETSTATE*);
    vnetSaveConfig(pState, pSSM);
    return VINF_SSM_DONT_CALL_AGAIN;
}

/**
 * Prepares for state saving.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSM        The handle to the saved state.
 */
static DECLCALLBACK(int) vnetSavePrep(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    VNETSTATE* pState = PDMINS_2_DATA(pDevIns, VNETSTATE*);

    int rc = vnetCsEnter(pState, VERR_SEM_BUSY);
    if (RT_UNLIKELY(rc != VINF_SUCCESS))
        return rc;
    vnetCsLeave(pState);
    return VINF_SUCCESS;
}

/**
 * Saves the state of device.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSM        The handle to the saved state.
 */
static DECLCALLBACK(int) vnetSaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    VNETSTATE* pState = PDMINS_2_DATA(pDevIns, VNETSTATE*);

    /* Save config first */
    vnetSaveConfig(pState, pSSM);

    /* Save the common part */
    int rc = vpciSaveExec(&pState->VPCI, pSSM);
    AssertRCReturn(rc, rc);
    /* Save device-specific part */
    rc = SSMR3PutMem( pSSM, pState->config.mac.au8, sizeof(pState->config.mac));
    AssertRCReturn(rc, rc);
    Log(("%s State has been saved\n", INSTANCE(pState)));
    return VINF_SUCCESS;
}


/**
 * Serializes the receive thread, it may be working inside the critsect.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSM        The handle to the saved state.
 */
static DECLCALLBACK(int) vnetLoadPrep(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    VNETSTATE* pState = PDMINS_2_DATA(pDevIns, VNETSTATE*);

    int rc = vnetCsEnter(pState, VERR_SEM_BUSY);
    if (RT_UNLIKELY(rc != VINF_SUCCESS))
        return rc;
    vnetCsLeave(pState);
    return VINF_SUCCESS;
}

/* Takes down the link temporarily if it's current status is up.
 *
 * This is used during restore and when replumbing the network link.
 *
 * The temporary link outage is supposed to indicate to the OS that all network
 * connections have been lost and that it for instance is appropriate to
 * renegotiate any DHCP lease.
 *
 * @param  pThis        The PCNet instance data.
 */
static void vnetTempLinkDown(PVNETSTATE pState)
{
    if (STATUS & VNET_S_LINK_UP)
    {
        STATUS &= ~VNET_S_LINK_UP;
        vpciRaiseInterrupt(&pState->VPCI, VERR_SEM_BUSY, VPCI_ISR_CONFIG);
        /* Restore the link back in 5 seconds. */
        int rc = TMTimerSetMillies(pState->pLinkUpTimer, 5000);
        AssertRC(rc);
    }
}


/**
 * Restore previously saved state of device.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSM        The handle to the saved state.
 * @param   uVersion    The data unit version number.
 * @param   uPass       The data pass.
 */
static DECLCALLBACK(int) vnetLoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    VNETSTATE *pState = PDMINS_2_DATA(pDevIns, VNETSTATE*);
    int       rc;

    /* config checks */
    RTMAC macConfigured;
    rc = SSMR3GetMem(pSSM, &macConfigured, sizeof(macConfigured));
    AssertRCReturn(rc, rc);
    if (memcmp(&macConfigured, &pState->macConfigured, sizeof(macConfigured))
        && (uPass == 0 || !PDMDevHlpVMTeleportedAndNotFullyResumedYet(pDevIns)))
        LogRel(("%s: The mac address differs: config=%RTmac saved=%RTmac\n", INSTANCE(pState), &pState->macConfigured, &macConfigured));

    rc = vpciLoadExec(&pState->VPCI, pSSM, uVersion, uPass);
    AssertRCReturn(rc, rc);

    if (uPass == SSM_PASS_FINAL)
    {
        rc = SSMR3GetMem( pSSM, pState->config.mac.au8, sizeof(pState->config.mac));
        AssertRCReturn(rc, rc);
    }

    return rc;
}

/**
 * Link status adjustments after loading.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSM        The handle to the saved state.
 */
static DECLCALLBACK(int) vnetLoadDone(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    VNETSTATE *pState = PDMINS_2_DATA(pDevIns, VNETSTATE*);

    /*
     * Indicate link down to the guest OS that all network connections have
     * been lost, unless we've been teleported here.
     */
    if (!PDMDevHlpVMTeleportedAndNotFullyResumedYet(pDevIns))
        vnetTempLinkDown(pState);

    return VINF_SUCCESS;
}

/**
 * Construct a device instance for a VM.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 *                      If the registration structure is needed, pDevIns->pDevReg points to it.
 * @param   iInstance   Instance number. Use this to figure out which registers and such to use.
 *                      The device number is also found in pDevIns->iInstance, but since it's
 *                      likely to be freqently used PDM passes it as parameter.
 * @param   pCfgHandle  Configuration node handle for the device. Use this to obtain the configuration
 *                      of the device instance. It's also found in pDevIns->pCfgHandle, but like
 *                      iInstance it's expected to be used a bit in this function.
 * @thread  EMT
 */
static DECLCALLBACK(int) vnetConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfgHandle)
{
    VNETSTATE* pState = PDMINS_2_DATA(pDevIns, VNETSTATE*);
    int        rc;

    /* Initialize PCI part first. */
    pState->VPCI.IBase.pfnQueryInterface     = vnetQueryInterface;
    rc = vpciConstruct(pDevIns, &pState->VPCI, iInstance, VIRTIO_NET_ID, sizeof(VNetPCIConfig));
    pState->pRxQueue  = vpciAddQueue(&pState->VPCI, 256, vnetQueueReceive,  "RX ");
    pState->pTxQueue  = vpciAddQueue(&pState->VPCI, 256, vnetQueueTransmit, "TX ");
    pState->pCtlQueue = vpciAddQueue(&pState->VPCI, 16,  vnetQueueControl,  "CTL");

    Log(("%s Constructing new instance\n", INSTANCE(pState)));

    pState->hEventMoreRxDescAvail = NIL_RTSEMEVENT;

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "MAC\0" "CableConnected\0" "LineSpeed\0"))
                    return PDMDEV_SET_ERROR(pDevIns, VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES,
                                            N_("Invalid configuration for VirtioNet device"));

    /* Get config params */
    rc = CFGMR3QueryBytes(pCfgHandle, "MAC", pState->macConfigured.au8,
                          sizeof(pState->macConfigured));
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get MAC address"));
    rc = CFGMR3QueryBool(pCfgHandle, "CableConnected", &pState->fCableConnected);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to get the value of 'CableConnected'"));

    /* Initialize PCI config space */
    memcpy(pState->config.mac.au8, pState->macConfigured.au8, sizeof(pState->config.mac.au8));
    pState->config.uStatus = 0;

    /* Initialize state structure */
    pState->fLocked      = false;
    pState->u32PktNo     = 1;

    /* Interfaces */
    pState->INetworkPort.pfnWaitReceiveAvail = vnetWaitReceiveAvail;
    pState->INetworkPort.pfnReceive          = vnetReceive;
    pState->INetworkConfig.pfnGetMac         = vnetGetMac;
    pState->INetworkConfig.pfnGetLinkState   = vnetGetLinkState;
    pState->INetworkConfig.pfnSetLinkState   = vnetSetLinkState;

    pState->pTxBuf = (uint8_t *)RTMemAllocZ(VNET_MAX_FRAME_SIZE);
    AssertMsgReturn(pState->pTxBuf, 
                    ("Cannot allocate TX buffer for virtio-net device\n"), VERR_NO_MEMORY);

    /* Register save/restore state handlers. */
    rc = PDMDevHlpSSMRegisterEx(pDevIns, VIRTIO_SAVEDSTATE_VERSION, sizeof(VNETSTATE), NULL,
                                NULL,         vnetLiveExec, NULL,
                                vnetSavePrep, vnetSaveExec, NULL,
                                vnetLoadPrep, vnetLoadExec, vnetLoadDone);
    if (RT_FAILURE(rc))
        return rc;

    /* Create the RX notifier signaller. */
    rc = PDMDevHlpPDMQueueCreate(pDevIns, sizeof(PDMQUEUEITEMCORE), 1, 0,
                                 vnetCanRxQueueConsumer, true, "VNet-Rcv", &pState->pCanRxQueueR3);
    if (RT_FAILURE(rc))
        return rc;
    pState->pCanRxQueueR0 = PDMQueueR0Ptr(pState->pCanRxQueueR3);
    pState->pCanRxQueueRC = PDMQueueRCPtr(pState->pCanRxQueueR3);

    /* Create Link Up Timer */
    rc = PDMDevHlpTMTimerCreate(pDevIns, TMCLOCK_VIRTUAL, vnetLinkUpTimer, pState,
                                TMTIMER_FLAGS_DEFAULT_CRIT_SECT, /** @todo check locking here. */
                                "VirtioNet Link Up Timer", &pState->pLinkUpTimer);
    if (RT_FAILURE(rc))
        return rc;

#ifdef VNET_TX_DELAY
    /* Create Transmit Delay Timer */
    rc = PDMDevHlpTMTimerCreate(pDevIns, TMCLOCK_VIRTUAL, vnetTxTimer, pState,
                                TMTIMER_FLAGS_DEFAULT_CRIT_SECT, /** @todo check locking here. */
                                "VirtioNet TX Delay Timer", &pState->pTxTimerR3);
    if (RT_FAILURE(rc))
        return rc;
    pState->pTxTimerR0 = TMTimerR0Ptr(pState->pTxTimerR3);
    pState->pTxTimerRC = TMTimerRCPtr(pState->pTxTimerR3);
#endif /* VNET_TX_DELAY */

    rc = PDMDevHlpDriverAttach(pDevIns, 0, &pState->VPCI.IBase, &pState->pDrvBase, "Network Port");
    if (RT_SUCCESS(rc))
    {
        if (rc == VINF_NAT_DNS)
        {
            PDMDevHlpVMSetRuntimeError(pDevIns, 0 /*fFlags*/, "NoDNSforNAT",
                                       N_("A Domain Name Server (DNS) for NAT networking could not be determined. Ensure that your host is correctly connected to an ISP. If you ignore this warning the guest will not be able to perform nameserver lookups and it will probably observe delays if trying so"));
        }
        pState->pDrv = (PPDMINETWORKCONNECTOR)
            pState->pDrvBase->pfnQueryInterface(pState->pDrvBase, PDMINTERFACE_NETWORK_CONNECTOR);
        if (!pState->pDrv)
        {
            AssertMsgFailed(("%s Failed to obtain the PDMINTERFACE_NETWORK_CONNECTOR interface!\n"));
            return VERR_PDM_MISSING_INTERFACE_BELOW;
        }
    }
    else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
    {
        Log(("%s This adapter is not attached to any network!\n", INSTANCE(pState)));
    }
    else
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Failed to attach the network LUN"));

    rc = RTSemEventCreate(&pState->hEventMoreRxDescAvail);
    if (RT_FAILURE(rc))
        return rc;

    vnetReset(pState);

    PDMDevHlpSTAMRegisterF(pDevIns, &pState->StatReceiveBytes,       STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,          "Amount of data received",            "/Devices/VNet%d/ReceiveBytes", iInstance);
    PDMDevHlpSTAMRegisterF(pDevIns, &pState->StatTransmitBytes,      STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,          "Amount of data transmitted",         "/Devices/VNet%d/TransmitBytes", iInstance);
#if defined(VBOX_WITH_STATISTICS)
    PDMDevHlpSTAMRegisterF(pDevIns, &pState->StatReceive,            STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL, "Profiling receive",                  "/Devices/VNet%d/Receive/Total", iInstance);
    PDMDevHlpSTAMRegisterF(pDevIns, &pState->StatRxOverflow,         STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_OCCURENCE, "Profiling RX overflows",        "/Devices/VNet%d/RxOverflow", iInstance);
    PDMDevHlpSTAMRegisterF(pDevIns, &pState->StatRxOverflowWakeup,   STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,     "Nr of RX overflow wakeups",          "/Devices/VNet%d/RxOverflowWakeup", iInstance);
    PDMDevHlpSTAMRegisterF(pDevIns, &pState->StatTransmit,           STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL, "Profiling transmits in HC",          "/Devices/VNet%d/Transmit/Total", iInstance);
    PDMDevHlpSTAMRegisterF(pDevIns, &pState->StatTransmitSend,       STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL, "Profiling send transmit in HC",      "/Devices/VNet%d/Transmit/Send", iInstance);
#endif /* VBOX_WITH_STATISTICS */

    return VINF_SUCCESS;
}

/**
 * Destruct a device instance.
 *
 * We need to free non-VM resources only.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 * @thread  EMT
 */
static DECLCALLBACK(int) vnetDestruct(PPDMDEVINS pDevIns)
{
    VNETSTATE* pState = PDMINS_2_DATA(pDevIns, VNETSTATE*);

    Log(("%s Destroying instance\n", INSTANCE(pState)));
    if (pState->hEventMoreRxDescAvail != NIL_RTSEMEVENT)
    {
        RTSemEventSignal(pState->hEventMoreRxDescAvail);
        RTSemEventDestroy(pState->hEventMoreRxDescAvail);
        pState->hEventMoreRxDescAvail = NIL_RTSEMEVENT;
    }

    if (pState->pTxBuf)
    {
        RTMemFree(pState->pTxBuf);
        pState->pTxBuf = NULL;
    }

    return vpciDestruct(&pState->VPCI);
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
static DECLCALLBACK(void) vnetRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    VNETSTATE* pState = PDMINS_2_DATA(pDevIns, VNETSTATE*);
    vpciRelocate(pDevIns, offDelta);
    pState->pCanRxQueueRC = PDMQueueRCPtr(pState->pCanRxQueueR3);
#ifdef VNET_TX_DELAY
    pState->pTxTimerRC    = TMTimerRCPtr(pState->pTxTimerR3);
#endif /* VNET_TX_DELAY */
    // TBD
}

/**
 * @copydoc FNPDMDEVSUSPEND
 */
static DECLCALLBACK(void) vnetSuspend(PPDMDEVINS pDevIns)
{
    /* Poke thread waiting for buffer space. */
    vnetWakeupReceive(pDevIns);
}


#ifdef VBOX_DYNAMIC_NET_ATTACH
/**
 * Detach notification.
 *
 * One port on the network card has been disconnected from the network.
 *
 * @param   pDevIns     The device instance.
 * @param   iLUN        The logical unit which is being detached.
 * @param   fFlags      Flags, combination of the PDMDEVATT_FLAGS_* \#defines.
 */
static DECLCALLBACK(void) vnetDetach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    VNETSTATE *pState = PDMINS_2_DATA(pDevIns, VNETSTATE*);
    Log(("%s vnetDetach:\n", INSTANCE(pState)));

    AssertLogRelReturnVoid(iLUN == 0);

    vnetCsEnter(pState, VERR_SEM_BUSY);

    /*
     * Zero some important members.
     */
    pState->pDrvBase = NULL;
    pState->pDrv = NULL;

    vnetCsLeave(pState);
}


/**
 * Attach the Network attachment.
 *
 * One port on the network card has been connected to a network.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   iLUN        The logical unit which is being attached.
 * @param   fFlags      Flags, combination of the PDMDEVATT_FLAGS_* \#defines.
 *
 * @remarks This code path is not used during construction.
 */
static DECLCALLBACK(int) vnetAttach(PPDMDEVINS pDevIns, unsigned iLUN, uint32_t fFlags)
{
    VNETSTATE *pState = PDMINS_2_DATA(pDevIns, VNETSTATE*);
    LogFlow(("%s vnetAttach:\n",  INSTANCE(pState)));

    AssertLogRelReturn(iLUN == 0, VERR_PDM_NO_SUCH_LUN);

    vnetCsEnter(pState, VERR_SEM_BUSY);

    /*
     * Attach the driver.
     */
    int rc = PDMDevHlpDriverAttach(pDevIns, 0, &pState->VPCI.IBase, &pState->pDrvBase, "Network Port");
    if (RT_SUCCESS(rc))
    {
        if (rc == VINF_NAT_DNS)
        {
#ifdef RT_OS_LINUX
            PDMDevHlpVMSetRuntimeError(pDevIns, 0 /*fFlags*/, "NoDNSforNAT",
                                       N_("A Domain Name Server (DNS) for NAT networking could not be determined. Please check your /etc/resolv.conf for <tt>nameserver</tt> entries. Either add one manually (<i>man resolv.conf</i>) or ensure that your host is correctly connected to an ISP. If you ignore this warning the guest will not be able to perform nameserver lookups and it will probably observe delays if trying so"));
#else
            PDMDevHlpVMSetRuntimeError(pDevIns, 0 /*fFlags*/, "NoDNSforNAT",
                                       N_("A Domain Name Server (DNS) for NAT networking could not be determined. Ensure that your host is correctly connected to an ISP. If you ignore this warning the guest will not be able to perform nameserver lookups and it will probably observe delays if trying so"));
#endif
        }
        pState->pDrv = (PPDMINETWORKCONNECTOR)pState->pDrvBase->pfnQueryInterface(pState->pDrvBase, PDMINTERFACE_NETWORK_CONNECTOR);
        if (!pState->pDrv)
        {
            AssertMsgFailed(("Failed to obtain the PDMINTERFACE_NETWORK_CONNECTOR interface!\n"));
            rc = VERR_PDM_MISSING_INTERFACE_BELOW;
        }
    }
    else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
        Log(("%s No attached driver!\n", INSTANCE(pState)));


    /*
     * Temporary set the link down if it was up so that the guest
     * will know that we have change the configuration of the
     * network card
     */
    if (RT_SUCCESS(rc))
        vnetTempLinkDown(pState);

    vnetCsLeave(pState);
    return rc;

}
#endif /* VBOX_DYNAMIC_NET_ATTACH */


/**
 * @copydoc FNPDMDEVPOWEROFF
 */
static DECLCALLBACK(void) vnetPowerOff(PPDMDEVINS pDevIns)
{
    /* Poke thread waiting for buffer space. */
    vnetWakeupReceive(pDevIns);
}

/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceVirtioNet =
{
    /* Structure version. PDM_DEVREG_VERSION defines the current version. */
    PDM_DEVREG_VERSION,
    /* Device name. */
    "virtio-net",
    /* Name of guest context module (no path).
     * Only evalutated if PDM_DEVREG_FLAGS_RC is set. */
    "VBoxDDGC.gc",
    /* Name of ring-0 module (no path).
     * Only evalutated if PDM_DEVREG_FLAGS_RC is set. */
    "VBoxDDR0.r0",
    /* The description of the device. The UTF-8 string pointed to shall, like this structure,
     * remain unchanged from registration till VM destruction. */
    "Virtio Ethernet.\n",

    /* Flags, combination of the PDM_DEVREG_FLAGS_* \#defines. */
#ifdef VNET_GC_SUPPORT
    PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_RC | PDM_DEVREG_FLAGS_R0,
#else
    PDM_DEVREG_FLAGS_DEFAULT_BITS,
#endif
    /* Device class(es), combination of the PDM_DEVREG_CLASS_* \#defines. */
    PDM_DEVREG_CLASS_NETWORK,
    /* Maximum number of instances (per VM). */
    8,
    /* Size of the instance data. */
    sizeof(VNETSTATE),

    /* Construct instance - required. */
    vnetConstruct,
    /* Destruct instance - optional. */
    vnetDestruct,
    /* Relocation command - optional. */
    vnetRelocate,
    /* I/O Control interface - optional. */
    NULL,
    /* Power on notification - optional. */
    NULL,
    /* Reset notification - optional. */
    NULL,
    /* Suspend notification  - optional. */
    vnetSuspend,
    /* Resume notification - optional. */
    NULL,
#ifdef VBOX_DYNAMIC_NET_ATTACH
    /* Attach command - optional. */
    vnetAttach,
    /* Detach notification - optional. */
    vnetDetach,
#else /* !VBOX_DYNAMIC_NET_ATTACH */
    /* Attach command - optional. */
    NULL,
    /* Detach notification - optional. */
    NULL,
#endif /* !VBOX_DYNAMIC_NET_ATTACH */
    /* Query a LUN base interface - optional. */
    NULL,
    /* Init complete notification - optional. */
    NULL,
    /* Power off notification - optional. */
    vnetPowerOff,
    /* pfnSoftReset */
    NULL,
    /* u32VersionEnd */
    PDM_DEVREG_VERSION
};

#endif /* IN_RING3 */
#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */
