/* $Id$ */
/** @file
 * Virtio_1_0 - Virtio Common Functions (VirtQueue, VQueue, Virtio PCI)
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_VIRTIO

#include <iprt/param.h>
#include <iprt/uuid.h>
#include <VBox/vmm/pdmdev.h>
#include "Virtio_1_0.h"

#define INSTANCE(pState) pState->szInstance
#define IFACE_TO_STATE(pIface, ifaceName) ((VIRTIOSTATE *)((char*)(pIface) - RT_UOFFSETOF(VIRTIOSTATE, ifaceName)))

#ifdef LOG_ENABLED
# define QUEUENAME(s, q) (q->pcszName)
#endif

#ifdef VBOX_DEVICE_STRUCT_TESTCASE
#  define virtioDumpState(x, s)  do {} while (0)
#else
#  ifdef DEBUG /* This still needs to be migrated to VirtIO 1.0 */
__attribute__((unused))
static void virtioDumpState(PVIRTIOSTATE pState, const char *pcszCaller)
{
    RT_NOREF2(pState, pcszCaller);
    /* PK TODO, dump state features, selector, status, ISR, queue info (iterate),
                descriptors, avail, used, size, indices, address
                each by variable name on new line, indented slightly */
}
#endif


void virtQueueReadDesc(PVIRTIOSTATE pState, PVIRTQUEUE pVirtQueue, uint32_t idx, PVIRTQUEUEDESC pDesc)
{
    //Log(("%s virtQueueReadDesc: ring=%p idx=%u\n", INSTANCE(pState), pVirtQueue, idx));
    PDMDevHlpPhysRead(pState->CTX_SUFF(pDevIns),
                      pVirtQueue->pGcPhysVirtqDescriptors + sizeof(VIRTQUEUEDESC) * (idx % pVirtQueue->cbQueue),
                      pDesc, sizeof(VIRTQUEUEDESC));
}

uint16_t virtQueueReadAvail(PVIRTIOSTATE pState, PVIRTQUEUE pVirtQueue, uint32_t idx)
{
    uint16_t tmp;
    PDMDevHlpPhysRead(pState->CTX_SUFF(pDevIns),
                      pVirtQueue->pGcPhysVirtqAvail + RT_UOFFSETOF_DYN(VIRTQUEUEAVAIL, auRing[idx % pVirtQueue->cbQueue]),
                      &tmp, sizeof(tmp));
    return tmp;
}

uint16_t virtQueueReadAvailFlags(PVIRTIOSTATE pState, PVIRTQUEUE pVirtQueue)
{
    uint16_t tmp;
    PDMDevHlpPhysRead(pState->CTX_SUFF(pDevIns),
                      pVirtQueue->pGcPhysVirtqAvail + RT_UOFFSETOF(VIRTQUEUEAVAIL, fFlags),
                      &tmp, sizeof(tmp));
    return tmp;
}

uint16_t virtQueueReadUsedIndex(PVIRTIOSTATE pState, PVIRTQUEUE pVirtQueue)
{
    uint16_t tmp;
    PDMDevHlpPhysRead(pState->CTX_SUFF(pDevIns),
                      pVirtQueue->pGcPhysVirtqUsed + RT_UOFFSETOF(VIRTQUEUEUSED, uIdx),
                      &tmp, sizeof(tmp));
    return tmp;
}

void virtQueueWriteUsedIndex(PVIRTIOSTATE pState, PVIRTQUEUE pVirtQueue, uint16_t u16Value)
{
    PDMDevHlpPCIPhysWrite(pState->CTX_SUFF(pDevIns),
                          pVirtQueue->pGcPhysVirtqAvail + RT_UOFFSETOF(VIRTQUEUEUSED, uIdx),
                          &u16Value, sizeof(u16Value));
}

void virtQueueWriteUsedElem(PVIRTIOSTATE pState, PVIRTQUEUE pVirtQueue, uint32_t idx, uint32_t id, uint32_t uLen)
{

    RT_NOREF5(pState, pVirtQueue, idx, id, uLen);
    /* PK TODO: Adapt to VirtIO 1.0
    VIRTQUEUEUSEDELEM elem;

    elem.id = id;
    elem.uLen = uLen;
    PDMDevHlpPCIPhysWrite(pState->CTX_SUFF(pDevIns),
                          pVirtQueue->pGcPhysVirtqUsed + RT_UOFFSETOF_DYN(VIRTQUEUEUSED, ring[idx % pVirtQueue->cbQueue]),
                          &elem, sizeof(elem));
    */
}

void virtQueueSetNotification(PVIRTIOSTATE pState, PVIRTQUEUE pVirtQueue, bool fEnabled)
{
    RT_NOREF3(pState, pVirtQueue, fEnabled);

/* PK TODO: Adapt to VirtIO 1.0
    uint16_t tmp;

    PDMDevHlpPhysRead(pState->CTX_SUFF(pDevIns),
                      pVirtQueue->pGcPhysVirtqAvail + RT_UOFFSETOF(VIRTQUEUEUSED, uFlags),
                      &tmp, sizeof(tmp));

    if (fEnabled)
        tmp &= ~ VIRTQUEUEUSED_F_NO_NOTIFY;
    else
        tmp |= VIRTQUEUEUSED_F_NO_NOTIFY;

    PDMDevHlpPCIPhysWrite(pState->CTX_SUFF(pDevIns),
                          pVirtQueue->pGcPhysVirtqAvail + RT_UOFFSETOF(VIRTQUEUEUSED, uFlags),
                          &tmp, sizeof(tmp));
*/
}

bool virtQueueSkip(PVIRTIOSTATE pState, PVQUEUE pQueue)
{

    RT_NOREF2(pState, pQueue);
/* PK TODO Adapt to VirtIO 1.0
    if (virtQueueIsEmpty(pState, pQueue))
        return false;

    Log2(("%s virtQueueSkip: %s avail_idx=%u\n", INSTANCE(pState),
          QUEUENAME(pState, pQueue), pQueue->uNextAvailIndex));
    pQueue->uNextAvailIndex++;
*/
    return true;
}

bool virtQueueGet(PVIRTIOSTATE pState, PVQUEUE pQueue, PVQUEUEELEM pElem, bool fRemove)
{

    RT_NOREF4(pState, pQueue, pElem, fRemove);

/* PK TODO: Adapt to VirtIO 1.0
    if (virtQueueIsEmpty(pState, pQueue))
        return false;

    pElem->nIn = pElem->nOut = 0;

    Log2(("%s virtQueueGet: %s avail_idx=%u\n", INSTANCE(pState),
          QUEUENAME(pState, pQueue), pQueue->uNextAvailIndex));

    VIRTQUEUEDESC desc;
    uint16_t  idx = virtQueueReadAvail(pState, &pQueue->VirtQueue, pQueue->uNextAvailIndex);
    if (fRemove)
        pQueue->uNextAvailIndex++;
    pElem->idx = idx;
    do
    {
        VQUEUESEG *pSeg;

        //
        // Malicious guests may try to trick us into writing beyond aSegsIn or
        // aSegsOut boundaries by linking several descriptors into a loop. We
        // cannot possibly get a sequence of linked descriptors exceeding the
        // total number of descriptors in the ring (see @bugref{8620}).
        ///
        if (pElem->nIn + pElem->nOut >= VIRTQUEUE_MAX_SIZE)
        {
            static volatile uint32_t s_cMessages  = 0;
            static volatile uint32_t s_cThreshold = 1;
            if (ASMAtomicIncU32(&s_cMessages) == ASMAtomicReadU32(&s_cThreshold))
            {
                LogRel(("%s: too many linked descriptors; check if the guest arranges descriptors in a loop.\n",
                        INSTANCE(pState)));
                if (ASMAtomicReadU32(&s_cMessages) != 1)
                    LogRel(("%s: (the above error has occured %u times so far)\n",
                            INSTANCE(pState), ASMAtomicReadU32(&s_cMessages)));
                ASMAtomicWriteU32(&s_cThreshold, ASMAtomicReadU32(&s_cThreshold) * 10);
            }
            break;
        }
        RT_UNTRUSTED_VALIDATED_FENCE();

        virtQueueReadDesc(pState, &pQueue->VirtQueue, idx, &desc);
        if (desc.u16Flags & VIRTQUEUEDESC_F_WRITE)
        {
            Log2(("%s virtQueueGet: %s IN  seg=%u desc_idx=%u addr=%p cb=%u\n", INSTANCE(pState),
                  QUEUENAME(pState, pQueue), pElem->nIn, idx, desc.addr, desc.uLen));
            pSeg = &pElem->aSegsIn[pElem->nIn++];
        }
        else
        {
            Log2(("%s virtQueueGet: %s OUT seg=%u desc_idx=%u addr=%p cb=%u\n", INSTANCE(pState),
                  QUEUENAME(pState, pQueue), pElem->nOut, idx, desc.addr, desc.uLen));
            pSeg = &pElem->aSegsOut[pElem->nOut++];
        }

        pSeg->addr = desc.addr;
        pSeg->cb   = desc.uLen;
        pSeg->pv   = NULL;

        idx = desc.next;
    } while (desc.u16Flags & VIRTQUEUEDESC_F_NEXT);

    Log2(("%s virtQueueGet: %s head_desc_idx=%u nIn=%u nOut=%u\n", INSTANCE(pState),
          QUEUENAME(pState, pQueue), pElem->idx, pElem->nIn, pElem->nOut));
*/
    return true;
}



void virtQueuePut(PVIRTIOSTATE pState, PVQUEUE pQueue,
               PVQUEUEELEM pElem, uint32_t uTotalLen, uint32_t uReserved)
{

    RT_NOREF5(pState, pQueue, pElem, uTotalLen, uReserved);

/* PK TODO Re-work this for VirtIO 1.0
    Log2(("%s virtQueuePut: %s"
          " desc_idx=%u acb=%u (%u)\n",
          INSTANCE(pState), QUEUENAME(pState, pQueue),
          pElem->idx, uTotalLen, uReserved));

    Assert(uReserved < uTotalLen);

    uint32_t cbLen = uTotalLen - uReserved;
    uint32_t cbSkip = uReserved;

    for (unsigned i = 0; i < pElem->nIn && cbLen > 0; ++i)
    {
        if (cbSkip >= pElem->aSegsIn[i].cb) // segment completely skipped?
        {
            cbSkip -= pElem->aSegsIn[i].cb;
            continue;
        }

        uint32_t cbSegLen = pElem->aSegsIn[i].cb - cbSkip;
        if (cbSegLen > cbLen)   // last segment only partially used?
            cbSegLen = cbLen;

        //
        // XXX: We should assert pv != NULL, but we need to check and
        // fix all callers first.
        //
        if (pElem->aSegsIn[i].pv != NULL)
        {
            Log2(("%s virtQueuePut: %s"
                  " used_idx=%u seg=%u addr=%p pv=%p cb=%u acb=%u\n",
                  INSTANCE(pState), QUEUENAME(pState, pQueue),
                  pQueue->uNextUsedIndex, i,
                  (void *)pElem->aSegsIn[i].addr, pElem->aSegsIn[i].pv,
                  pElem->aSegsIn[i].cb, cbSegLen));

            PDMDevHlpPCIPhysWrite(pState->CTX_SUFF(pDevIns),
                                  pElem->aSegsIn[i].addr + cbSkip,
                                  pElem->aSegsIn[i].pv,
                                  cbSegLen);
        }

        cbSkip = 0;
        cbLen -= cbSegLen;
    }

    Log2(("%s virtQueuePut: %s"
          " used_idx=%u guest_used_idx=%u id=%u len=%u\n",
          INSTANCE(pState), QUEUENAME(pState, pQueue),
          pQueue->uNextUsedIndex, virtQueueReadUsedIndex(pState, &pQueue->VirtQueue),
          pElem->idx, uTotalLen));

    virtQueueWriteUsedElem(pState, &pQueue->VirtQueue,
                       pQueue->uNextUsedIndex++,
                       pElem->idx, uTotalLen);
*/

}


void virtQueueNotify(PVIRTIOSTATE pState, PVQUEUE pQueue)
{

    RT_NOREF2(pState, pQueue);
/* PK TODO Adapt to VirtIO 1.0
    LogFlow(("%s virtQueueNotify: %s availFlags=%x guestFeatures=%x virtQueue is %sempty\n",
             INSTANCE(pState), QUEUENAME(pState, pQueue),
             virtQueueReadAvailFlags(pState, &pQueue->VirtQueue),
             pState->uGuestFeatures, virtQueueIsEmpty(pState, pQueue)?"":"not "));
    if (!(virtQueueReadAvailFlags(pState, &pQueue->VirtQueue) & VIRTQUEUEAVAIL_F_NO_INTERRUPT)
        || ((pState->uGuestFeatures & VIRTIO_F_NOTIFY_ON_EMPTY) && virtQueueIsEmpty(pState, pQueue)))
    {
        int rc = virtioRaiseInterrupt(pState, VERR_INTERNAL_ERROR, VIRTIO_ISR_QUEUE);
        if (RT_FAILURE(rc))
            Log(("%s virtQueueNotify: Failed to raise an interrupt (%Rrc).\n", INSTANCE(pState), rc));
    }
*/
}

void virtQueueSync(PVIRTIOSTATE pState, PVQUEUE pQueue)
{
    RT_NOREF(pState, pQueue);
/* PK TODO Adapt to VirtIO 1.0
    Log2(("%s virtQueueSync: %s old_used_idx=%u new_used_idx=%u\n", INSTANCE(pState),
          QUEUENAME(pState, pQueue), virtQueueReadUsedIndex(pState, &pQueue->VirtQueue), pQueue->uNextUsedIndex));
    virtQueueWriteUsedIndex(pState, &pQueue->VirtQueue, pQueue->uNextUsedIndex);
    virtQueueNotify(pState, pQueue);
*/
}

void virtioReset(PVIRTIOSTATE pState)
{

    RT_NOREF(pState);
/* PK TODO Adapt to VirtIO 1.0
    pState->uGuestFeatures = 0;
    pState->uQueueSelector = 0;
    pState->uStatus        = 0;
    pState->uISR           = 0;

    for (unsigned i = 0; i < pState->nQueues; i++)
        virtQueueReset(&pState->Queues[i]);
*/
}


/**
 * Raise interrupt.
 *
 * @param   pState      The device state structure.
 * @param   rcBusy      Status code to return when the critical section is busy.
 * @param   u8IntCause  Interrupt cause bit mask to set in PCI ISR port.
 */
int virtioRaiseInterrupt(VIRTIOSTATE *pState, int rcBusy, uint8_t u8IntCause)
{
    RT_NOREF2(pState, u8IntCause);
    RT_NOREF_PV(rcBusy);
/* PK TODO: Adapt to VirtIO 1.0
    STAM_COUNTER_INC(&pState->StatIntsRaised);
    LogFlow(("%s virtioRaiseInterrupt: u8IntCause=%x\n",
             INSTANCE(pState), u8IntCause));

    pState->uISR |= u8IntCause;
    PDMDevHlpPCISetIrq(pState->CTX_SUFF(pDevIns), 0, 1);
*/
    return VINF_SUCCESS;
}

/**
 * Lower interrupt.
 *
 * @param   pState      The device state structure.
 */
__attribute__((unused))
static void virtioLowerInterrupt(VIRTIOSTATE *pState)
{
    LogFlow(("%s virtioLowerInterrupt\n", INSTANCE(pState)));
    PDMDevHlpPCISetIrq(pState->CTX_SUFF(pDevIns), 0, 0);
}

DECLINLINE(uint32_t) virtioGetHostFeatures(PVIRTIOSTATE pState,
                                         PFNGETHOSTFEATURES pfnGetHostFeatures)
{
    return pfnGetHostFeatures(pState) /*| VIRTIO_F_NOTIFY_ON_EMPTY */;
}


#ifdef IN_RING3

/**
 * Gets the pointer to the status LED of a unit.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the interface structure.
 * @param   iLUN            The unit which status LED we desire.
 * @param   ppLed           Where to store the LED pointer.
 * @thread  EMT
 */
static DECLCALLBACK(int) virtioQueryStatusLed(PPDMILEDPORTS pInterface, unsigned iLUN, PPDMLED *ppLed)
{
    VIRTIOSTATE *pState = IFACE_TO_STATE(pInterface, ILeds);
    int rc = VERR_PDM_LUN_NOT_FOUND;

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
void virtioSetWriteLed(PVIRTIOSTATE pState, bool fOn)
{
    LogFlow(("%s virtioSetWriteLed: %s\n", INSTANCE(pState), fOn ? "on" : "off"));
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
void virtioSetReadLed(PVIRTIOSTATE pState, bool fOn)
{
    LogFlow(("%s virtioSetReadLed: %s\n", INSTANCE(pState), fOn ? "on" : "off"));
    if (fOn)
        pState->led.Asserted.s.fReading = pState->led.Actual.s.fReading = 1;
    else
        pState->led.Actual.s.fReading = fOn;
}


/**
 * Saves the state of device.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSM        The handle to the saved state.
 */
int virtioSaveExec(PVIRTIOSTATE pState, PSSMHANDLE pSSM)
{
    RT_NOREF2(pState, pSSM);
    int rc = VINF_SUCCESS;
//    virtioDumpState(pState, "virtioSaveExec");
    /*
     * PK TODO save guest features, queue selector, sttus ISR,
     *              and per queue info (size, address, indicies)...
     * using calls like SSMR3PutU8(), SSMR3PutU16(), SSMR3PutU16()...
     * and AssertRCReturn(rc, rc)
     */

    return rc;
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
int virtioLoadExec(PVIRTIOSTATE pState, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass, uint32_t nQueues)
{
    RT_NOREF5(pState, pSSM, uVersion, uPass, nQueues);
    int rc = VINF_SUCCESS;

    /*
     * PK TODO, restore everything saved in virtioSaveExect, using
     * using calls like SSMR3PutU8(), SSMR3PutU16(), SSMR3PutU16()...
     * and AssertRCReturn(rc, rc)
     */
    if (uPass == SSM_PASS_FINAL)
    {
    }

//    virtioDumpState(pState, "virtioLoadExec");

    return rc;
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
void virtioRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    RT_NOREF(offDelta);
    VIRTIOSTATE *pState = PDMINS_2_DATA(pDevIns, VIRTIOSTATE*);
    pState->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);
    // TBD
}

PVQUEUE virtioAddQueue(VIRTIOSTATE* pState, unsigned cbQueue, PFNVIRTIOQUEUECALLBACK pfnCallback, const char *pcszName)
{

    RT_NOREF4(pState, cbQueue, pfnCallback, pcszName);
/* PK TODO Adapt to VirtIO 1.0

    PVQUEUE pQueue = NULL;
    // Find an empty queue slot
    for (unsigned i = 0; i < pState->nQueues; i++)
    {
        if (pState->Queues[i].VirtQueue.cbQueue == 0)
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
        pQueue->VirtQueue.cbQueue = cbQueue;
        pQueue->VirtQueue.addrDescriptors = 0;
        pQueue->uPageNumber = 0;
        pQueue->pfnCallback = pfnCallback;
        pQueue->pcszName    = pcszName;
    }
    return pQueue;
*/
    return NULL;// Temporary
}


__attribute__((unused))
static void virtQueueReset(PVQUEUE pQueue)
{
    RT_NOREF(pQueue);
/* PK TODO Adapt to VirtIO 1.0
    pQueue->VirtQueue.addrDescriptors = 0;
    pQueue->VirtQueue.addrAvail       = 0;
    pQueue->VirtQueue.addrUsed        = 0;
    pQueue->uNextAvailIndex           = 0;
    pQueue->uNextUsedIndex            = 0;
    pQueue->uPageNumber               = 0;
*/
}

__attribute__((unused))
static void virtQueueInit(PVQUEUE pQueue, uint32_t uPageNumber)
{
    RT_NOREF2(pQueue, uPageNumber);

/* PK TODO, re-work this for VirtIO 1.0
    pQueue->VirtQueue.addrDescriptors = (uint64_t)uPageNumber << PAGE_SHIFT;

    pQueue->VirtQueue.addrAvail = pQueue->VirtQueue.addrDescriptors
                                + sizeof(VIRTQUEUEDESC) * pQueue->VirtQueue.cbQueue;

    pQueue->VirtQueue.addrUsed  = RT_ALIGN(pQueue->VirtQueue.addrAvail
            + RT_UOFFSETOF_DYN(VIRTQUEUEAVAIL, ring[pQueue->VirtQueue.cbQueue])
            + sizeof(uint16_t), // virtio 1.0 adds a 16-bit field following ring data
              PAGE_SIZE); // The used ring must start from the next page.

    pQueue->uNextAvailIndex       = 0;
    pQueue->uNextUsedIndex        = 0;
*/

}


/**
 * Memory mapped I/O Handler for PCI Capabilities read operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   GCPhysAddr  Physical address (in GC) where the read starts.
 * @param   pv          Where to store the result.
 * @param   cb          Number of bytes read.
 */
PDMBOTHCBDECL(int) virtioPciCapMemRead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb)
{
/* PK New feature! */

    RT_NOREF(pvUser);
    int rc = VINF_SUCCESS;

    LogFunc(("Read (MMIO) VirtIO capabilities\n"));
    PVIRTIOSTATE pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSTATE);

    /* TBD: This should be called only if VIRTIO_PCI_CAP_DEVICE_CFG capability is being accessed */
    rc = pThis->pfnVirtioDevCapRead(pDevIns, GCPhysAddr, pv, cb);
    return rc;
}


/**
 * Memory mapped I/O Handler for PCI Capabilities write operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument.
 * @param   GCPhysAddr  Physical address (in GC) where the write starts.
 * @param   pv          Where to fetch the result.
 * @param   cb          Number of bytes to write.
 */
PDMBOTHCBDECL(int) virtioPciCapMemWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void const *pv, unsigned cb)
{

/* PK New feature! */
    RT_NOREF(pvUser);
    int rc = VINF_SUCCESS;
    LogFunc(("Write (MMIO) Virtio capabilities\n"));
    PVIRTIOSTATE pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSTATE);

    /* TBD: This should be called only if VIRTIO_PCI_CAP_DEVICE_CFG capability is being accessed */
    rc = pThis->pfnVirtioDevCapWrite(pDevIns, GCPhysAddr, pv,cb);

    return rc;
}

/**
 * @callback_method_impl{FNPCIIOREGIONMAP}
 */
static DECLCALLBACK(int) virtioR3Map(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev, uint32_t iRegion,
                                     RTGCPHYS GCPhysAddress, RTGCPHYS cb, PCIADDRESSSPACE enmType)
{
    RT_NOREF3(pPciDev, iRegion, enmType);
    PVIRTIOSTATE  pThis = PDMINS_2_DATA(pDevIns, PVIRTIOSTATE);
    int rc = VINF_SUCCESS;

    Assert(cb >= 32);

    LogFunc(("virtIO controller PCI Capabilities mapped at GCPhysAddr=%RGp cb=%RGp\n", GCPhysAddress, cb));

    /* We use the assigned size here, because we currently only support page aligned MMIO ranges. */
    rc = PDMDevHlpMMIORegister(pDevIns, GCPhysAddress, cb, NULL /*pvUser*/,
                           IOMMMIO_FLAGS_READ_PASSTHRU | IOMMMIO_FLAGS_WRITE_PASSTHRU,
                           virtioPciCapMemWrite, virtioPciCapMemRead,
                           "virtio-scsi MMIO");
    pThis->GCPhysPciCapBase = RT_SUCCESS(rc) ? GCPhysAddress : 0;
    return rc;
}


/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
void *virtioQueryInterface(struct PDMIBASE *pInterface, const char *pszIID)
{
    VIRTIOSTATE *pThis = IFACE_TO_STATE(pInterface, IBase);
    Assert(&pThis->IBase == pInterface);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pThis->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMILEDPORTS, &pThis->ILeds);
    return NULL;
}


/**
 * Destruct PCI-related part of device.
 *
 * We need to free non-VM resources only.
 *
 * @returns VBox status code.
 * @param   pState      The device state structure.
 */
int virtioDestruct(VIRTIOSTATE* pState)
{
    Log(("%s Destroying PCI instance\n", INSTANCE(pState)));

    return VINF_SUCCESS;
}

/** PK (temp note to self):
 *
 *  Device needs to negotiate capabilities,
 *  then get queue size address information from driver.
 *
 * Still need consumer to pass in:
 *
 *  num_queues
 *  config_generation
 *  Needs to manage feature negotiation
 *  That means consumer needs to pass in device-specific feature bits/values
 *  Device has to provie at least one notifier capability
 *
 *  ISR config value are set by the device (config interrupt vs. queue interrupt)
 *
 */

/**
 * Setup PCI device controller and Virtio state
 *
 * @param   pDevIns               Device instance data
 * @param   pVirtio               Device State
 * @param   iInstance             Instance number
 * @param   pPciParams            Values to populate industry standard PCI Configuration Space data structure
 * @param   pcszNameFmt           Device instance name (format-specifier)
 * @param   nQueues               Number of Virtio Queues created by consumer (driver)
 * @param   uVirtioRegion         Region number to map for PCi Capabilities structs
 * @param   devCapReadCallback    Client function to call back to handle device specific capabilities
 * @param   devCapWriteCallback   Client function to call back to handle device specific capabilities
 * @param   cbDevSpecificCap      Size of device specific struct
 */
int   virtioConstruct(PPDMDEVINS pDevIns, PVIRTIOSTATE pVirtio, int iInstance,
                    PVIRTIOPCIPARAMS pPciParams, const char *pcszNameFmt,
                    uint32_t nQueues, uint32_t uVirtioRegion,
                    PFNVIRTIODEVCAPREAD devCapReadCallback, PFNVIRTIODEVCAPWRITE devCapWriteCallback,
                    uint16_t cbDevSpecificCap)
{

    RT_NOREF(nQueues);

    /* Init handles and log related stuff. */
    RTStrPrintf(pVirtio->szInstance, sizeof(pVirtio->szInstance), pcszNameFmt, iInstance);

    pVirtio->pDevInsR3    = pDevIns;
    pVirtio->pDevInsR0    = PDMDEVINS_2_R0PTR(pDevIns);
    pVirtio->pDevInsRC    = PDMDEVINS_2_RCPTR(pDevIns);
    pVirtio->led.u32Magic = PDMLED_MAGIC;
    pVirtio->ILeds.pfnQueryStatusLed = virtioQueryStatusLed;
    pVirtio->pfnVirtioDevCapRead     = devCapReadCallback;
    pVirtio->pfnVirtioDevCapWrite    = devCapWriteCallback;

    int rc = VINF_SUCCESS;
    rc = PDMDevHlpPCIIORegionRegister(pDevIns, uVirtioRegion, 32, PCI_ADDRESS_SPACE_MEM, virtioR3Map);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
            N_("virtio: cannot register PCI Capabilities address space")); /* can we put params in this error? */


    /* Set PCI config registers (assume 32-bit mode) */
    PCIDevSetRevisionId        (&pVirtio->dev, DEVICE_PCI_REVISION_ID_VIRTIO);
    PCIDevSetVendorId          (&pVirtio->dev, DEVICE_PCI_VENDOR_ID_VIRTIO);
    PCIDevSetSubSystemVendorId (&pVirtio->dev, DEVICE_PCI_VENDOR_ID_VIRTIO);
    PCIDevSetDeviceId          (&pVirtio->dev, pPciParams->uDeviceId);
    PCIDevSetClassBase         (&pVirtio->dev, pPciParams->uClassBase);
    PCIDevSetClassSub          (&pVirtio->dev, pPciParams->uClassSub);
    PCIDevSetClassProg         (&pVirtio->dev, pPciParams->uClassProg);
    PCIDevSetSubSystemId       (&pVirtio->dev, pPciParams->uSubsystemId);
    PCIDevSetInterruptLine     (&pVirtio->dev, pPciParams->uInterruptLine);
    PCIDevSetInterruptPin      (&pVirtio->dev, pPciParams->uInterruptPin);

    /** Build PCI vendor-specific capabilities list for exchanging
     *  VirtIO device capabilities with driver */

    uint8_t fMsiSupport = false;
#if 0 && defined(VBOX_WITH_MSI_DEVICES)  /* T.B.D. */
        fMsiSupport = true;
#endif
    uint8_t uCfgCapOffset = 0;
    PVIRTIOPCICAP pCfgCap = (PVIRTIOPCICAP)0x40; /* First VirtIO vendor-specific cap PCI cfg space addr */
    PVIRTIOPCICAP pCommonCfg  = (PVIRTIOPCICAP)&pVirtio->dev.abConfig[uCfgCapOffset];
    pCommonCfg->uCapVndr = VIRTIO_PCI_CAP_ID_VENDOR;
    pCommonCfg->uCapNext = (uint8_t)((uint64_t)pCfgCap++ & 0xff);
    pCommonCfg->uCfgType = VIRTIO_PCI_CAP_COMMON_CFG;
    pCommonCfg->uBar     = uVirtioRegion;
    pCommonCfg->uOffset  = 0;
    pCommonCfg->uLength  = sizeof(VIRTIOPCICAP);

    PVIRTIOPCICAP pNotifyCfg  = (PVIRTIOPCICAP)&pVirtio->dev.abConfig[uCfgCapOffset];
    pNotifyCfg->uCapVndr = VIRTIO_PCI_CAP_ID_VENDOR;
    pNotifyCfg->uCapNext =  (uint8_t)((uint64_t)pCfgCap++ & 0xff);
    pNotifyCfg->uCfgType = VIRTIO_PCI_CAP_NOTIFY_CFG;
    pNotifyCfg->uBar     = uVirtioRegion;
    pNotifyCfg->uOffset  = pVirtio->pCommonCfg->uOffset + sizeof(VIRTIOCOMMONCFG);
    pNotifyCfg->uLength  = sizeof(VIRTIOPCICAP);

    PVIRTIOPCICAP pISRConfig  = (PVIRTIOPCICAP)&pVirtio->dev.abConfig[uCfgCapOffset];
    pISRConfig->uCapVndr = VIRTIO_PCI_CAP_ID_VENDOR;
    pISRConfig->uCapNext =  (uint8_t)((uint64_t)pCfgCap++ & 0xff);
    pISRConfig->uCfgType = VIRTIO_PCI_CAP_ISR_CFG;
    pISRConfig->uBar     = uVirtioRegion;
    pISRConfig->uOffset  = pVirtio->pNotifyCfg->uOffset + sizeof(VIRTIONOTIFYCFG);
    pISRConfig->uLength  = sizeof(VIRTIOPCICAP);

    PVIRTIOPCICAP pPCIConfig  = (PVIRTIOPCICAP)&pVirtio->dev.abConfig[uCfgCapOffset];
    pPCIConfig->uCapVndr = VIRTIO_PCI_CAP_ID_VENDOR;
    pPCIConfig->uCapNext =  (uint8_t)(fMsiSupport || cbDevSpecificCap ? ((uint64_t)pCfgCap++ & 0xff): 0);
    pPCIConfig->uCfgType = VIRTIO_PCI_CAP_PCI_CFG;
    pPCIConfig->uBar     = uVirtioRegion;
    pPCIConfig->uOffset  = pVirtio->pISRConfig->uOffset + sizeof(VIRTIOISRCFG);
    pPCIConfig->uLength  = sizeof(VIRTIOPCICAP);

    pVirtio->pCommonCfg = pCommonCfg;
    pVirtio->pNotifyCfg = pNotifyCfg;
    pVirtio->pISRConfig = pISRConfig;
    pVirtio->pPCIConfig = pPCIConfig;

    if (cbDevSpecificCap)
    {
        PVIRTIOPCICAP pDeviceCfg  = (PVIRTIOPCICAP)&pVirtio->dev.abConfig[uCfgCapOffset];
        pDeviceCfg->uCapVndr = VIRTIO_PCI_CAP_ID_VENDOR;
        pDeviceCfg->uCapNext = (uint8_t)(fMsiSupport ? ((uint64_t)pCfgCap++ & 0xff) : 0);
        pDeviceCfg->uCfgType = VIRTIO_PCI_CAP_DEVICE_CFG;
        pDeviceCfg->uBar     = uVirtioRegion;
        pDeviceCfg->uOffset  = pVirtio->pPCIConfig->uOffset + sizeof(VIRTIOCAPCFG);
        pDeviceCfg->uLength  = sizeof(VIRTIOPCICAP);
        pVirtio->pDeviceCfg = pDeviceCfg;
    }

    /* Set offset to first capability and enable PCI dev capabilities */
    PCIDevSetCapabilityList    (&pVirtio->dev, 0x40);
    PCIDevSetStatus            (&pVirtio->dev, VBOX_PCI_STATUS_CAP_LIST);

    /* Register PCI device */
    rc = PDMDevHlpPCIRegister(pDevIns, &pVirtio->dev);
    if (RT_FAILURE(rc))
        return rc;

    if (fMsiSupport)
    {
        PDMMSIREG aMsiReg;

        RT_ZERO(aMsiReg);
        aMsiReg.iMsixCapOffset  = uCfgCapOffset;
        aMsiReg.iMsixNextOffset = 0;
        aMsiReg.iMsixBar        = 0;
        aMsiReg.cMsixVectors    = 1;
        rc = PDMDevHlpPCIRegisterMsi(pDevIns, &aMsiReg); /* see MsixR3init() */
        if (RT_FAILURE (rc))
        {
            /* PK TODO: The following is moot, we need to flag no MSI-X support */
            PCIDevSetCapabilityList(&pVirtio->dev, 0x40);
        }
    }

    /* Status driver */
    PPDMIBASE pBase;
    rc = PDMDevHlpDriverAttach(pDevIns, PDM_STATUS_LUN, &pVirtio->IBase, &pBase, "Status Port");
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Failed to attach the status LUN"));
    pVirtio->pLedsConnector = PDMIBASE_QUERY_INTERFACE(pBase, PDMILEDCONNECTORS);

    return rc;
}



#endif /* IN_RING3 */

#endif /* VBOX_DEVICE_STRUCT_TESTCASE */

