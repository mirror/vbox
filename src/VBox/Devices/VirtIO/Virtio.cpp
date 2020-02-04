/* $Id$ */
/** @file
 * Virtio - Virtio Common Functions (VRing, VQueue, Virtio PCI)
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_VIRTIO

#include <iprt/param.h>
#include <iprt/uuid.h>
#include <VBox/vmm/pdmdev.h>
#include <VBox/AssertGuest.h>
#include "Virtio.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#define INSTANCE(pThis) (pThis->szInstance)


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
    pQueue->VRing.addrDescriptors = (uint64_t)uPageNumber << PAGE_SHIFT;
    pQueue->VRing.addrAvail       = pQueue->VRing.addrDescriptors + sizeof(VRINGDESC) * pQueue->VRing.uSize;
    pQueue->VRing.addrUsed        = RT_ALIGN(pQueue->VRing.addrAvail + RT_UOFFSETOF_DYN(VRINGAVAIL, auRing[pQueue->VRing.uSize]),
                                             PAGE_SIZE); /* The used ring must start from the next page. */
    pQueue->uNextAvailIndex       = 0;
    pQueue->uNextUsedIndex        = 0;
}

// void vqueueElemFree(PVQUEUEELEM pElem)
// {
// }

static void vringReadDesc(PPDMDEVINS pDevIns, PVRING pVRing, uint32_t uIndex, PVRINGDESC pDesc)
{
    //Log(("%s vringReadDesc: ring=%p idx=%u\n", INSTANCE(pThis), pVRing, uIndex));
    PDMDevHlpPhysRead(pDevIns,
                      pVRing->addrDescriptors + sizeof(VRINGDESC) * (uIndex % pVRing->uSize),
                      pDesc, sizeof(VRINGDESC));
    /** @todo r=bird: Why exactly are we sometimes using PDMDevHlpPhysRead rather
     *        than PDMDevHlpPCIPhysRead? */
}

static uint16_t vringReadAvail(PPDMDEVINS pDevIns, PVRING pVRing, uint32_t uIndex)
{
    uint16_t tmp = 0;
    PDMDevHlpPhysRead(pDevIns, pVRing->addrAvail + RT_UOFFSETOF_DYN(VRINGAVAIL, auRing[uIndex % pVRing->uSize]),
                      &tmp, sizeof(tmp));
    return tmp;
}

static uint16_t vringReadAvailFlags(PPDMDEVINS pDevIns, PVRING pVRing)
{
    uint16_t tmp = 0;
    PDMDevHlpPhysRead(pDevIns, pVRing->addrAvail + RT_UOFFSETOF(VRINGAVAIL, uFlags), &tmp, sizeof(tmp));
    return tmp;
}

void vringSetNotification(PPDMDEVINS pDevIns, PVRING pVRing, bool fEnabled)
{
    uint16_t fState = 0;
    PDMDevHlpPhysRead(pDevIns, pVRing->addrUsed + RT_UOFFSETOF(VRINGUSED, uFlags), &fState, sizeof(fState));

    if (fEnabled)
        fState &= ~ VRINGUSED_F_NO_NOTIFY;
    else
        fState |= VRINGUSED_F_NO_NOTIFY;

    PDMDevHlpPCIPhysWrite(pDevIns, pVRing->addrUsed + RT_UOFFSETOF(VRINGUSED, uFlags), &fState, sizeof(fState));
}

bool vqueueSkip(PPDMDEVINS pDevIns, PVPCISTATE pThis, PVQUEUE pQueue)
{
    if (vqueueIsEmpty(pDevIns, pQueue))
        return false;

    Log2(("%s vqueueSkip: %s avail_idx=%u\n", INSTANCE(pThis), pQueue->szName, pQueue->uNextAvailIndex));
    RT_NOREF(pThis);
    pQueue->uNextAvailIndex++;
    return true;
}

bool vqueueGet(PPDMDEVINS pDevIns, PVPCISTATE pThis, PVQUEUE pQueue, PVQUEUEELEM pElem, bool fRemove)
{
    if (vqueueIsEmpty(pDevIns, pQueue))
        return false;

    pElem->cIn = pElem->cOut = 0;

    Log2(("%s vqueueGet: %s avail_idx=%u\n", INSTANCE(pThis), pQueue->szName, pQueue->uNextAvailIndex));

    VRINGDESC desc;
    uint16_t  idx = vringReadAvail(pDevIns, &pQueue->VRing, pQueue->uNextAvailIndex);
    if (fRemove)
        pQueue->uNextAvailIndex++;
    pElem->uIndex = idx;
    do
    {
        VQUEUESEG *pSeg;

        /*
         * Malicious guests may try to trick us into writing beyond aSegsIn or
         * aSegsOut boundaries by linking several descriptors into a loop. We
         * cannot possibly get a sequence of linked descriptors exceeding the
         * total number of descriptors in the ring (see @bugref{8620}).
         */
        if (pElem->cIn + pElem->cOut >= VRING_MAX_SIZE)
        {
            static volatile uint32_t s_cMessages  = 0;
            static volatile uint32_t s_cThreshold = 1;
            if (ASMAtomicIncU32(&s_cMessages) == ASMAtomicReadU32(&s_cThreshold))
            {
                LogRel(("%s: too many linked descriptors; check if the guest arranges descriptors in a loop.\n",
                        INSTANCE(pThis)));
                if (ASMAtomicReadU32(&s_cMessages) != 1)
                    LogRel(("%s: (the above error has occured %u times so far)\n",
                            INSTANCE(pThis), ASMAtomicReadU32(&s_cMessages)));
                ASMAtomicWriteU32(&s_cThreshold, ASMAtomicReadU32(&s_cThreshold) * 10);
            }
            break;
        }
        RT_UNTRUSTED_VALIDATED_FENCE();

        vringReadDesc(pDevIns, &pQueue->VRing, idx, &desc);
        if (desc.u16Flags & VRINGDESC_F_WRITE)
        {
            Log2(("%s vqueueGet: %s IN  seg=%u desc_idx=%u addr=%p cb=%u\n", INSTANCE(pThis),
                  pQueue->szName, pElem->cIn, idx, desc.u64Addr, desc.uLen));
            pSeg = &pElem->aSegsIn[pElem->cIn++];
        }
        else
        {
            Log2(("%s vqueueGet: %s OUT seg=%u desc_idx=%u addr=%p cb=%u\n", INSTANCE(pThis),
                  pQueue->szName, pElem->cOut, idx, desc.u64Addr, desc.uLen));
            pSeg = &pElem->aSegsOut[pElem->cOut++];
        }

        pSeg->addr = desc.u64Addr;
        pSeg->cb   = desc.uLen;
        pSeg->pv   = NULL;

        idx = desc.u16Next;
    } while (desc.u16Flags & VRINGDESC_F_NEXT);

    Log2(("%s vqueueGet: %s head_desc_idx=%u nIn=%u nOut=%u\n", INSTANCE(pThis),
          pQueue->szName, pElem->uIndex, pElem->cIn, pElem->cOut));
    return true;
}

#ifdef LOG_ENABLED
static uint16_t vringReadUsedIndex(PPDMDEVINS pDevIns, PVRING pVRing)
{
    uint16_t tmp = 0;
    PDMDevHlpPhysRead(pDevIns, pVRing->addrUsed + RT_UOFFSETOF(VRINGUSED, uIndex), &tmp, sizeof(tmp));
    return tmp;
}
#endif

static void vringWriteUsedIndex(PPDMDEVINS pDevIns, PVRING pVRing, uint16_t u16Value)
{
    PDMDevHlpPCIPhysWrite(pDevIns,
                          pVRing->addrUsed + RT_UOFFSETOF(VRINGUSED, uIndex),
                          &u16Value, sizeof(u16Value));
}

static void vringWriteUsedElem(PPDMDEVINS pDevIns, PVRING pVRing, uint32_t uIndex, uint32_t uId, uint32_t uLen)
{
    VRINGUSEDELEM elem;

    elem.uId = uId;
    elem.uLen = uLen;
    PDMDevHlpPCIPhysWrite(pDevIns,
                          pVRing->addrUsed + RT_UOFFSETOF_DYN(VRINGUSED, aRing[uIndex % pVRing->uSize]),
                          &elem, sizeof(elem));
}

void vqueuePut(PPDMDEVINS pDevIns, PVPCISTATE pThis, PVQUEUE pQueue, PVQUEUEELEM pElem, uint32_t uTotalLen, uint32_t uReserved)
{
    Log2(("%s vqueuePut: %s desc_idx=%u acb=%u (%u)\n", INSTANCE(pThis), pQueue->szName, pElem->uIndex, uTotalLen, uReserved));
    RT_NOREF(pThis);

    Assert(uReserved < uTotalLen);

    uint32_t cbLen = uTotalLen - uReserved;
    uint32_t cbSkip = uReserved;

    for (unsigned i = 0; i < pElem->cIn && cbLen > 0; ++i)
    {
        if (cbSkip >= pElem->aSegsIn[i].cb) /* segment completely skipped? */
        {
            cbSkip -= pElem->aSegsIn[i].cb;
            continue;
        }

        uint32_t cbSegLen = pElem->aSegsIn[i].cb - cbSkip;
        if (cbSegLen > cbLen)   /* last segment only partially used? */
            cbSegLen = cbLen;

        /*
         * XXX: We should assert pv != NULL, but we need to check and
         * fix all callers first.
         */
        if (pElem->aSegsIn[i].pv != NULL)
        {
            Log2(("%s vqueuePut: %s used_idx=%u seg=%u addr=%RGp pv=%p cb=%u acb=%u\n", INSTANCE(pThis), pQueue->szName,
                  pQueue->uNextUsedIndex, i, pElem->aSegsIn[i].addr, pElem->aSegsIn[i].pv, pElem->aSegsIn[i].cb, cbSegLen));

            PDMDevHlpPCIPhysWrite(pDevIns,
                                  pElem->aSegsIn[i].addr + cbSkip,
                                  pElem->aSegsIn[i].pv,
                                  cbSegLen);
        }

        cbSkip = 0;
        cbLen -= cbSegLen;
    }

    Log2(("%s vqueuePut: %s used_idx=%u guest_used_idx=%u id=%u len=%u\n", INSTANCE(pThis), pQueue->szName,
          pQueue->uNextUsedIndex, vringReadUsedIndex(pDevIns, &pQueue->VRing), pElem->uIndex, uTotalLen));

    vringWriteUsedElem(pDevIns, &pQueue->VRing,
                       pQueue->uNextUsedIndex++,
                       pElem->uIndex, uTotalLen);
}

static void vqueueNotify(PPDMDEVINS pDevIns, PVPCISTATE pThis, PVQUEUE pQueue)
{
    uint16_t const fAvail = vringReadAvailFlags(pDevIns, &pQueue->VRing);
    LogFlow(("%s vqueueNotify: %s availFlags=%x guestFeatures=%x vqueue is %sempty\n", INSTANCE(pThis), pQueue->szName,
             fAvail, pThis->uGuestFeatures, vqueueIsEmpty(pDevIns, pQueue)?"":"not "));
    if (   !(fAvail & VRINGAVAIL_F_NO_INTERRUPT)
        || ((pThis->uGuestFeatures & VPCI_F_NOTIFY_ON_EMPTY) && vqueueIsEmpty(pDevIns, pQueue)))
    {
        int rc = vpciRaiseInterrupt(pDevIns, pThis, VERR_INTERNAL_ERROR, VPCI_ISR_QUEUE);
        if (RT_FAILURE(rc))
            Log(("%s vqueueNotify: Failed to raise an interrupt (%Rrc).\n", INSTANCE(pThis), rc));
    }
    else
        STAM_REL_COUNTER_INC(&pThis->StatIntsSkipped);

}

void vqueueSync(PPDMDEVINS pDevIns, PVPCISTATE pThis, PVQUEUE pQueue)
{
    Log2(("%s vqueueSync: %s old_used_idx=%u new_used_idx=%u\n", INSTANCE(pThis),
          pQueue->szName, vringReadUsedIndex(pDevIns, &pQueue->VRing), pQueue->uNextUsedIndex));
    vringWriteUsedIndex(pDevIns, &pQueue->VRing, pQueue->uNextUsedIndex);
    vqueueNotify(pDevIns, pThis, pQueue);
}


/**
 * Raise interrupt.
 *
 * @param   pDevIns     The device instance.
 * @param   pThis       The shared virtio core instance data.
 * @param   rcBusy      Status code to return when the critical section is busy.
 * @param   u8IntCause  Interrupt cause bit mask to set in PCI ISR port.
 */
int vpciRaiseInterrupt(PPDMDEVINS pDevIns, PVPCISTATE pThis, int rcBusy, uint8_t u8IntCause)
{
    RT_NOREF_PV(rcBusy);
    // int rc = vpciCsEnter(pThis, rcBusy);
    // if (RT_UNLIKELY(rc != VINF_SUCCESS))
    //     return rc;

    STAM_REL_COUNTER_INC(&pThis->StatIntsRaised);
    LogFlow(("%s vpciRaiseInterrupt: u8IntCause=%x\n", INSTANCE(pThis), u8IntCause));

    pThis->uISR |= u8IntCause;
    PDMDevHlpPCISetIrq(pDevIns, 0, 1);
    // vpciCsLeave(pThis);
    return VINF_SUCCESS;
}

/**
 * Lower interrupt.
 *
 * @param   pDevIns     The device instance.
 * @param   pThis       The shared virtio core instance data.
 */
static void vpciLowerInterrupt(PPDMDEVINS pDevIns, PVPCISTATE pThis)
{
    LogFlow(("%s vpciLowerInterrupt\n", INSTANCE(pThis)));
    RT_NOREF(pThis);
    PDMDevHlpPCISetIrq(pDevIns, 0, 0);
}


void vpciReset(PPDMDEVINS pDevIns, PVPCISTATE pThis)
{
    /* No interrupts should survive device reset, see @bugref(9556). */
    if (pThis->uISR)
        vpciLowerInterrupt(pDevIns, pThis);

    pThis->uGuestFeatures = 0;
    pThis->uQueueSelector = 0;
    pThis->uStatus        = 0;
    pThis->uISR           = 0;

    for (unsigned i = 0; i < pThis->cQueues; i++)
        vqueueReset(&pThis->Queues[i]);
}


DECLINLINE(uint32_t) vpciGetHostFeatures(PVPCISTATE pThis, PCVPCIIOCALLBACKS  pCallbacks)
{
    return pCallbacks->pfnGetHostFeatures(pThis) | VPCI_F_NOTIFY_ON_EMPTY;
}

/**
 * Port I/O Handler for IN operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pThis       The shared virtio core instance data.
 * @param   offPort     The offset into the I/O range of the port being read.
 * @param   pu32        Where to store the result.
 * @param   cb          Number of bytes read.
 * @param   pCallbacks  Pointer to the callbacks.
 * @thread  EMT
 */
int vpciIOPortIn(PPDMDEVINS         pDevIns,
                 PVPCISTATE         pThis,
                 RTIOPORT           offPort,
                 uint32_t          *pu32,
                 unsigned           cb,
                 PCVPCIIOCALLBACKS  pCallbacks)
{
    STAM_PROFILE_ADV_START(&pThis->CTX_SUFF(StatIORead), a);

    /*
     * We probably do not need to enter critical section when reading registers
     * as the most of them are either constant or being changed during
     * initialization only, the exception being ISR which can be raced by all
     * threads but I see no big harm in it. It also happens to be the most read
     * register as it gets read in interrupt handler. By dropping cs protection
     * here we gain the ability to deliver RX packets to the guest while TX is
     * holding cs transmitting queued packets.
     *
    int rc = vpciCsEnter(pThis, VINF_IOM_R3_IOPORT_READ);
    if (RT_UNLIKELY(rc != VINF_SUCCESS))
    {
        STAM_PROFILE_ADV_STOP(&pThis->CTX_SUFF(StatIORead), a);
        return rc;
        }*/
    int rc = VINF_SUCCESS;

    switch (offPort)
    {
        case VPCI_HOST_FEATURES:
            /* Tell the guest what features we support. */
            ASSERT_GUEST_MSG(cb == 4, ("%d\n", cb));
            *pu32 = vpciGetHostFeatures(pThis, pCallbacks) | VPCI_F_BAD_FEATURE;
            break;

        case VPCI_GUEST_FEATURES:
            ASSERT_GUEST_MSG(cb == 4, ("%d\n", cb));
            *pu32 = pThis->uGuestFeatures;
            break;

        case VPCI_QUEUE_PFN:
            ASSERT_GUEST_MSG(cb == 4, ("%d\n", cb));
            *pu32 = pThis->Queues[pThis->uQueueSelector].uPageNumber;
            break;

        case VPCI_QUEUE_NUM:
            ASSERT_GUEST_MSG(cb == 2, ("%d\n", cb));
            *pu32 = pThis->Queues[pThis->uQueueSelector].VRing.uSize;
            break;

        case VPCI_QUEUE_SEL:
            ASSERT_GUEST_MSG(cb == 2, ("%d\n", cb));
            *pu32 = pThis->uQueueSelector;
            break;

        case VPCI_STATUS:
            ASSERT_GUEST_MSG(cb == 1, ("%d\n", cb));
            *pu32 = pThis->uStatus;
            break;

        case VPCI_ISR:
            ASSERT_GUEST_MSG(cb == 1, ("%d\n", cb));
            *pu32 = pThis->uISR;
            pThis->uISR = 0; /* read clears all interrupts */
            vpciLowerInterrupt(pDevIns, pThis);
            break;

        default:
            if (offPort >= VPCI_CONFIG)
                rc = pCallbacks->pfnGetConfig(pThis, offPort - VPCI_CONFIG, cb, pu32);
            else
            {
                *pu32 = UINT32_MAX;
                rc = PDMDevHlpDBGFStop(pDevIns, RT_SRC_POS, "%s vpciIOPortIn: no valid port at offset port=%RTiop cb=%08x\n",
                                       INSTANCE(pThis), offPort, cb);
            }
            break;
    }
    Log3(("%s vpciIOPortIn:  At %RTiop in  %0*x\n", INSTANCE(pThis), offPort, cb*2, *pu32));

    //vpciCsLeave(pThis);

    STAM_PROFILE_ADV_STOP(&pThis->CTX_SUFF(StatIORead), a);
    return rc;
}


/**
 * Port I/O Handler for OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pThis       The shared virtio core instance data.
 * @param   offPort     The offset into the I/O range of the port being written.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 * @param   pCallbacks  Pointer to the callbacks.
 * @thread  EMT
 */
int vpciIOPortOut(PPDMDEVINS         pDevIns,
                  PVPCISTATE         pThis,
                  PVPCISTATECC       pThisCC,
                  RTIOPORT           offPort,
                  uint32_t           u32,
                  unsigned           cb,
                  PCVPCIIOCALLBACKS  pCallbacks)
{
    STAM_PROFILE_ADV_START(&pThis->CTX_SUFF(StatIOWrite), a);
    int  rc = VINF_SUCCESS;
    bool fHasBecomeReady;
#ifndef IN_RING3
    RT_NOREF_PV(pThisCC);
#endif

    Log3(("%s virtioIOPortOut: At offPort=%RTiop out  %0*x\n", INSTANCE(pThis), offPort, cb*2, u32));

    switch (offPort)
    {
        case VPCI_GUEST_FEATURES:
        {
            const uint32_t fHostFeatures = vpciGetHostFeatures(pThis, pCallbacks);

            if (RT_LIKELY((u32 & ~fHostFeatures) == 0))
                pThis->uGuestFeatures = u32;
            else
            {
                /*
                 * Guest requests features we don't advertise.  Stick
                 * to the minimum if negotiation looks completely
                 * botched, otherwise restrict to advertised features.
                 */
                if (u32 & VPCI_F_BAD_FEATURE)
                {
                    Log(("%s WARNING! Guest failed to negotiate properly (guest=%x)\n",
                         INSTANCE(pThis), u32));
                    pThis->uGuestFeatures = pCallbacks->pfnGetHostMinimalFeatures(pThis);
                }
                else
                {
                    Log(("%s Guest asked for features host does not support! (host=%x guest=%x)\n",
                         INSTANCE(pThis), fHostFeatures, u32));
                    pThis->uGuestFeatures = u32 & fHostFeatures;
                }
            }
            pCallbacks->pfnSetHostFeatures(pThis, pThis->uGuestFeatures);
            break;
        }

        case VPCI_QUEUE_PFN:
            /*
             * The guest is responsible for allocating the pages for queues,
             * here it provides us with the page number of descriptor table.
             * Note that we provide the size of the queue to the guest via
             * VIRTIO_PCI_QUEUE_NUM.
             */
            pThis->Queues[pThis->uQueueSelector].uPageNumber = u32;
            if (u32)
                vqueueInit(&pThis->Queues[pThis->uQueueSelector], u32);
            else
                rc = pCallbacks->pfnReset(pDevIns);
            break;

        case VPCI_QUEUE_SEL:
            ASSERT_GUEST_MSG(cb == 2, ("cb=%u\n", cb));
            u32 &= 0xFFFF;
            if (u32 < pThis->cQueues)
                pThis->uQueueSelector = u32;
            else
                Log3(("%s vpciIOPortOut: Invalid queue selector %08x\n", INSTANCE(pThis), u32));
            break;

        case VPCI_QUEUE_NOTIFY:
#ifdef IN_RING3
            ASSERT_GUEST_MSG(cb == 2, ("cb=%u\n", cb));
            u32 &= 0xFFFF;
            if (u32 < pThis->cQueues)
            {
                RT_UNTRUSTED_VALIDATED_FENCE();
                if (pThis->Queues[u32].VRing.addrDescriptors)
                {

                    // rc = vpciCsEnter(pThis, VERR_SEM_BUSY);
                    // if (RT_LIKELY(rc == VINF_SUCCESS))
                    // {
                        pThisCC->Queues[u32].pfnCallback(pDevIns, &pThis->Queues[u32]);
                    //     vpciCsLeave(pThis);
                    // }
                }
                else
                    Log(("%s The queue (#%d) being notified has not been initialized.\n",
                         INSTANCE(pThis), u32));
            }
            else
                Log(("%s Invalid queue number (%d)\n", INSTANCE(pThis), u32));
#else
            rc = VINF_IOM_R3_IOPORT_WRITE;
#endif
            break;

        case VPCI_STATUS:
            ASSERT_GUEST_MSG(cb == 1, ("cb=%u\n", cb));
            u32 &= 0xFF;
            fHasBecomeReady = !(pThis->uStatus & VPCI_STATUS_DRV_OK) && (u32 & VPCI_STATUS_DRV_OK);
            pThis->uStatus = u32;
            /* Writing 0 to the status port triggers device reset. */
            if (u32 == 0)
                rc = pCallbacks->pfnReset(pDevIns);
            else if (fHasBecomeReady)
            {
                /* Older hypervisors were lax and did not enforce bus mastering. Older guests
                 * (Linux prior to 2.6.34, NetBSD 6.x) were lazy and did not enable bus mastering.
                 * We automagically enable bus mastering on driver initialization to make existing
                 * drivers work.
                 */
                PPDMPCIDEV pPciDev = pDevIns->apPciDevs[0];
                PDMPciDevSetCommand(pPciDev, PDMPciDevGetCommand(pPciDev) | PCI_COMMAND_BUSMASTER);

                pCallbacks->pfnReady(pDevIns);
            }
            break;

        default:
            if (offPort >= VPCI_CONFIG)
                rc = pCallbacks->pfnSetConfig(pThis, offPort - VPCI_CONFIG, cb, &u32);
            else
                rc = PDMDevHlpDBGFStop(pDevIns, RT_SRC_POS, "%s vpciIOPortOut: no valid port at offset offPort=%RTiop cb=%08x\n",
                                       INSTANCE(pThis), offPort, cb);
            break;
    }

    STAM_PROFILE_ADV_STOP(&pThis->CTX_SUFF(StatIOWrite), a);
    return rc;
}

#ifdef IN_RING3

/**
 * Handles common IBase.pfnQueryInterface requests.
 */
void *vpciR3QueryInterface(PVPCISTATECC pThisCC, const char *pszIID)
{
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pThisCC->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMILEDPORTS, &pThisCC->ILeds);
    return NULL;
}

/**
 * @interface_method_impl{PDMILEDPORTS,pfnQueryStatusLed}
 */
static DECLCALLBACK(int) vpciR3QueryStatusLed(PPDMILEDPORTS pInterface, unsigned iLUN, PPDMLED *ppLed)
{
    PVPCISTATECC pThisCC = RT_FROM_MEMBER(pInterface, VPCISTATECC, ILeds);
    if (iLUN == 0)
    {
        *ppLed = &pThisCC->pShared->led;
        return VINF_SUCCESS;
    }
    return VERR_PDM_LUN_NOT_FOUND;
}

/**
 * Turns on/off the write status LED.
 *
 * @returns VBox status code.
 * @param   pThis          Pointer to the device state structure.
 * @param   fOn             New LED state.
 */
void vpciR3SetWriteLed(PVPCISTATE pThis, bool fOn)
{
    LogFlow(("%s vpciR3SetWriteLed: %s\n", INSTANCE(pThis), fOn?"on":"off"));
    if (fOn)
        pThis->led.Asserted.s.fWriting = pThis->led.Actual.s.fWriting = 1;
    else
        pThis->led.Actual.s.fWriting = fOn;
}

/**
 * Turns on/off the read status LED.
 *
 * @returns VBox status code.
 * @param   pThis          Pointer to the device state structure.
 * @param   fOn             New LED state.
 */
void vpciR3SetReadLed(PVPCISTATE pThis, bool fOn)
{
    LogFlow(("%s vpciR3SetReadLed: %s\n", INSTANCE(pThis), fOn?"on":"off"));
    if (fOn)
        pThis->led.Asserted.s.fReading = pThis->led.Actual.s.fReading = 1;
    else
        pThis->led.Actual.s.fReading = fOn;
}

# if 0 /* unused */
/**
 * Sets 32-bit register in PCI configuration space.
 * @param   refPciDev   The PCI device.
 * @param   uOffset     The register offset.
 * @param   u32Value    The value to store in the register.
 * @thread  EMT
 */
DECLINLINE(void) vpciCfgSetU32(PDMPCIDEV& refPciDev, uint32_t uOffset, uint32_t u32Value)
{
    Assert(uOffset+sizeof(u32Value) <= sizeof(refPciDev.config));
    *(uint32_t*)&refPciDev.config[uOffset] = u32Value;
}
# endif /* unused */

/**
 * Dumps the state (useful for both logging and info items).
 */
void vpciR3DumpStateWorker(PVPCISTATE pThis, PCDBGFINFOHLP pHlp)
{

    pHlp->pfnPrintf(pHlp,
                    "  uGuestFeatures = 0x%08x\n"
                    "  uQueueSelector = 0x%04x\n"
                    "  uStatus        = 0x%02x\n"
                    "  uISR           = 0x%02x\n",
                    pThis->uGuestFeatures,
                    pThis->uQueueSelector,
                    pThis->uStatus,
                    pThis->uISR);

    for (unsigned i = 0; i < pThis->cQueues; i++)
        pHlp->pfnPrintf(pHlp,
                        " %s queue:\n"
                        "  VRing.uSize           = %u\n"
                        "  VRing.addrDescriptors = %p\n"
                        "  VRing.addrAvail       = %p\n"
                        "  VRing.addrUsed        = %p\n"
                        "  uNextAvailIndex       = %u\n"
                        "  uNextUsedIndex        = %u\n"
                        "  uPageNumber           = %x\n",
                        pThis->Queues[i].szName,
                        pThis->Queues[i].VRing.uSize,
                        pThis->Queues[i].VRing.addrDescriptors,
                        pThis->Queues[i].VRing.addrAvail,
                        pThis->Queues[i].VRing.addrUsed,
                        pThis->Queues[i].uNextAvailIndex,
                        pThis->Queues[i].uNextUsedIndex,
                        pThis->Queues[i].uPageNumber);
}

# ifdef LOG_ENABLED
void vpciR3DumpState(PVPCISTATE pThis, const char *pcszCaller)
{
    if (LogIs2Enabled())
    {
        Log2(("vpciR3DumpState: (called from %s)\n", pcszCaller));
        vpciR3DumpStateWorker(pThis, DBGFR3InfoLogHlp());
    }
}
# else
#  define vpciR3DumpState(x, s)  do {} while (0)
# endif

/**
 * Saved the core virtio state.
 *
 * @returns VBox status code.
 * @param   pHlp        The device helpers.
 * @param   pThis       The shared virtio core instance data.
 * @param   pSSM        The handle to the saved state.
 */
int vpciR3SaveExec(PCPDMDEVHLPR3 pHlp, PVPCISTATE pThis, PSSMHANDLE pSSM)
{
    vpciR3DumpState(pThis, "vpciR3SaveExec");

    pHlp->pfnSSMPutU32(pSSM, pThis->uGuestFeatures);
    pHlp->pfnSSMPutU16(pSSM, pThis->uQueueSelector);
    pHlp->pfnSSMPutU8( pSSM, pThis->uStatus);
    pHlp->pfnSSMPutU8( pSSM, pThis->uISR);

    /* Save queue states */
    int rc = pHlp->pfnSSMPutU32(pSSM, pThis->cQueues);
    AssertRCReturn(rc, rc);
    for (unsigned i = 0; i < pThis->cQueues; i++)
    {
        pHlp->pfnSSMPutU16(pSSM, pThis->Queues[i].VRing.uSize);
        pHlp->pfnSSMPutU32(pSSM, pThis->Queues[i].uPageNumber);
        pHlp->pfnSSMPutU16(pSSM, pThis->Queues[i].uNextAvailIndex);
        rc = pHlp->pfnSSMPutU16(pSSM, pThis->Queues[i].uNextUsedIndex);
        AssertRCReturn(rc, rc);
    }

    return VINF_SUCCESS;
}

/**
 * Loads a saved device state.
 *
 * @returns VBox status code.
 * @param   pHlp        The device helpers.
 * @param   pThis       The shared virtio core instance data.
 * @param   pSSM        The handle to the saved state.
 * @param   uVersion    The data unit version number.
 * @param   uPass       The data pass.
 * @param   cQueues     The default queue count (for old states).
 */
int vpciR3LoadExec(PCPDMDEVHLPR3 pHlp, PVPCISTATE pThis, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass, uint32_t cQueues)
{
    int rc;

    if (uPass == SSM_PASS_FINAL)
    {
        /* Restore state data */
        pHlp->pfnSSMGetU32(pSSM, &pThis->uGuestFeatures);
        pHlp->pfnSSMGetU16(pSSM, &pThis->uQueueSelector);
        pHlp->pfnSSMGetU8( pSSM, &pThis->uStatus);
        pHlp->pfnSSMGetU8( pSSM, &pThis->uISR);

        /* Restore queues */
        if (uVersion > VIRTIO_SAVEDSTATE_VERSION_3_1_BETA1)
        {
            rc = pHlp->pfnSSMGetU32(pSSM, &pThis->cQueues);
            AssertRCReturn(rc, rc);
        }
        else
            pThis->cQueues = cQueues;
        AssertLogRelMsgReturn(pThis->cQueues <= VIRTIO_MAX_NQUEUES, ("%#x\n", pThis->cQueues), VERR_SSM_LOAD_CONFIG_MISMATCH);
        AssertLogRelMsgReturn(pThis->uQueueSelector < pThis->cQueues || (pThis->cQueues == 0 && pThis->uQueueSelector),
                              ("uQueueSelector=%u cQueues=%u\n", pThis->uQueueSelector, pThis->cQueues),
                              VERR_SSM_LOAD_CONFIG_MISMATCH);

        for (unsigned i = 0; i < pThis->cQueues; i++)
        {
            rc = pHlp->pfnSSMGetU16(pSSM, &pThis->Queues[i].VRing.uSize);
            AssertRCReturn(rc, rc);
            rc = pHlp->pfnSSMGetU32(pSSM, &pThis->Queues[i].uPageNumber);
            AssertRCReturn(rc, rc);

            if (pThis->Queues[i].uPageNumber)
                vqueueInit(&pThis->Queues[i], pThis->Queues[i].uPageNumber);

            rc = pHlp->pfnSSMGetU16(pSSM, &pThis->Queues[i].uNextAvailIndex);
            AssertRCReturn(rc, rc);
            rc = pHlp->pfnSSMGetU16(pSSM, &pThis->Queues[i].uNextUsedIndex);
            AssertRCReturn(rc, rc);
        }
    }

    vpciR3DumpState(pThis, "vpciLoadExec");

    return VINF_SUCCESS;
}

PVQUEUE vpciR3AddQueue(PVPCISTATE pThis, PVPCISTATECC pThisCC, unsigned uSize,
                       PFNVPCIQUEUECALLBACK pfnCallback, const char *pcszName)
{
    /* Find an empty queue slot */
    for (unsigned i = 0; i < pThis->cQueues; i++)
    {
        if (pThis->Queues[i].VRing.uSize == 0)
        {
            PVQUEUE pQueue = &pThis->Queues[i];
            pQueue->VRing.uSize = uSize;
            pQueue->VRing.addrDescriptors = 0;
            pQueue->uPageNumber = 0;
            int rc = RTStrCopy(pQueue->szName, sizeof(pQueue->szName), pcszName);
            AssertRC(rc);
            pThisCC->Queues[i].pfnCallback = pfnCallback;
            return pQueue;
        }
    }
    AssertMsgFailedReturn(("%s Too many queues being added, no empty slots available!\n", INSTANCE(pThis)), NULL);
}

/**
 * Destruct PCI-related part of device.
 *
 * We need to free non-VM resources only.
 *
 * @returns VBox status code.
 * @param   pThis      The shared virtio core instance data.
 */
int vpciR3Term(PPDMDEVINS pDevIns, PVPCISTATE pThis)
{
    Log(("%s Destroying PCI instance\n", INSTANCE(pThis)));

    if (PDMDevHlpCritSectIsInitialized(pDevIns, &pThis->cs))
        PDMDevHlpCritSectDelete(pDevIns, &pThis->cs);

    return VINF_SUCCESS;
}

/**
 * Set PCI configuration space registers.
 *
 * @param   pPciDev      Pointer to the PCI device structure.
 * @param   uDeviceId    VirtiO Device Id
 * @param   uClass       Class of PCI device (network, etc)
 * @thread  EMT
 */
static void vpciConfigure(PPDMPCIDEV pPciDev, uint16_t uDeviceId, uint16_t uClass)
{
    /* Configure PCI Device, assume 32-bit mode ******************************/
    PDMPciDevSetVendorId(pPciDev,                           DEVICE_PCI_VENDOR_ID);
    PDMPciDevSetDeviceId(pPciDev,                           DEVICE_PCI_BASE_ID + uDeviceId);
    PDMPciDevSetWord(pPciDev, VBOX_PCI_SUBSYSTEM_VENDOR_ID, DEVICE_PCI_SUBSYSTEM_VENDOR_ID);
    PDMPciDevSetWord(pPciDev, VBOX_PCI_SUBSYSTEM_ID,        DEVICE_PCI_SUBSYSTEM_BASE_ID + uDeviceId);

    /* ABI version, must be equal 0 as of 2.6.30 kernel. */
    PDMPciDevSetByte(pPciDev, VBOX_PCI_REVISION_ID,         0x00);
    /* Ethernet adapter */
    PDMPciDevSetByte(pPciDev, VBOX_PCI_CLASS_PROG,          0x00);
    PDMPciDevSetWord(pPciDev, VBOX_PCI_CLASS_DEVICE,        uClass);
    /* Interrupt Pin: INTA# */
    PDMPciDevSetByte(pPciDev, VBOX_PCI_INTERRUPT_PIN,       0x01);

# ifdef VBOX_WITH_MSI_DEVICES
    PDMPciDevSetCapabilityList(pPciDev,                     0x80);
    PDMPciDevSetStatus(pPciDev,                             VBOX_PCI_STATUS_CAP_LIST);
# endif
}

int vpciR3Init(PPDMDEVINS pDevIns, PVPCISTATE pThis, PVPCISTATECC pThisCC, uint16_t uDeviceId, uint16_t uClass, uint32_t cQueues)
{
    /* Init data members. */
    pThis->cQueues                   = cQueues;
    pThis->led.u32Magic              = PDMLED_MAGIC;
    pThisCC->pShared                 = pThis;
    pThisCC->ILeds.pfnQueryStatusLed = vpciR3QueryStatusLed;
    AssertReturn(pThisCC->IBase.pfnQueryInterface, VERR_INVALID_POINTER);
    AssertReturn(pThis->szInstance[0], VERR_INVALID_PARAMETER);
    AssertReturn(strlen(pThis->szInstance) < sizeof(pThis->szInstance), VERR_INVALID_PARAMETER);

    /* Initialize critical section. */
    int rc = PDMDevHlpCritSectInit(pDevIns, &pThis->cs, RT_SRC_POS, "%s", pThis->szInstance);
    AssertRCReturn(rc, rc);

    /*
     * Set up the PCI device.
     */
    PPDMPCIDEV pPciDev = pDevIns->apPciDevs[0];
    PDMPCIDEV_ASSERT_VALID(pDevIns, pPciDev);

    /* Set PCI config registers */
    vpciConfigure(pPciDev, uDeviceId, uClass);

    /* Register PCI device */
    rc = PDMDevHlpPCIRegister(pDevIns, pPciDev);
    AssertRCReturn(rc, rc);

# ifdef VBOX_WITH_MSI_DEVICES
#  if 0
    {
        PDMMSIREG aMsiReg;

        RT_ZERO(aMsiReg);
        aMsiReg.cMsixVectors = 1;
        aMsiReg.iMsixCapOffset = 0x80;
        aMsiReg.iMsixNextOffset = 0x0;
        aMsiReg.iMsixBar = 0;
        rc = PDMDevHlpPCIRegisterMsi(pDevIns, &aMsiReg);
        if (RT_FAILURE (rc))
            PCIDevSetCapabilityList(&pThis->pciDevice, 0x0);
    }
#  endif
# endif

    /*
     * Attach the status driver (optional).
     */
    PPDMIBASE pBase;
    rc = PDMDevHlpDriverAttach(pDevIns, PDM_STATUS_LUN, &pThisCC->IBase, &pBase, "Status Port");
    if (RT_SUCCESS(rc))
        pThisCC->pLedsConnector = PDMIBASE_QUERY_INTERFACE(pBase, PDMILEDCONNECTORS);
    else if (rc != VERR_PDM_NO_ATTACHED_DRIVER)
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Failed to attach the status LUN"));

    /*
     * Statistics.
     */
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatIntsRaised,  STAMTYPE_COUNTER, "Interrupts/Raised",  STAMUNIT_OCCURENCES,     "Number of raised interrupts");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatIntsSkipped, STAMTYPE_COUNTER, "Interrupts/Skipped", STAMUNIT_OCCURENCES,     "Number of skipped interrupts");
# ifdef VBOX_WITH_STATISTICS
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatIOReadR3,    STAMTYPE_PROFILE, "IO/ReadR3",          STAMUNIT_TICKS_PER_CALL, "Profiling IO reads in R3");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatIOReadR0,    STAMTYPE_PROFILE, "IO/ReadR0",          STAMUNIT_TICKS_PER_CALL, "Profiling IO reads in R0");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatIOReadRC,    STAMTYPE_PROFILE, "IO/ReadRC",          STAMUNIT_TICKS_PER_CALL, "Profiling IO reads in RC");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatIOWriteR3,   STAMTYPE_PROFILE, "IO/WriteR3",         STAMUNIT_TICKS_PER_CALL, "Profiling IO writes in R3");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatIOWriteR0,   STAMTYPE_PROFILE, "IO/WriteR0",         STAMUNIT_TICKS_PER_CALL, "Profiling IO writes in R0");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatIOWriteRC,   STAMTYPE_PROFILE, "IO/WriteRC",         STAMUNIT_TICKS_PER_CALL, "Profiling IO writes in RC");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatCsR3,        STAMTYPE_PROFILE, "Cs/CsR3",            STAMUNIT_TICKS_PER_CALL, "Profiling CS wait in R3");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatCsR0,        STAMTYPE_PROFILE, "Cs/CsR0",            STAMUNIT_TICKS_PER_CALL, "Profiling CS wait in R0");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatCsRC,        STAMTYPE_PROFILE, "Cs/CsRC",            STAMUNIT_TICKS_PER_CALL, "Profiling CS wait in RC");
# endif /* VBOX_WITH_STATISTICS */

    return VINF_SUCCESS;
}

#else  /* !IN_RING3 */

/**
 * Does ring-0/raw-mode initialization.
 */
int vpciRZInit(PPDMDEVINS pDevIns, PVPCISTATE pThis, PVPCISTATECC pThisCC)
{
    RT_NOREF(pDevIns, pThis, pThisCC);
    return VINF_SUCCESS;
}

#endif /* !IN_RING3 */

