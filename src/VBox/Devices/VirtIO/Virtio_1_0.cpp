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

#include <VBox/log.h>
#include <iprt/param.h>
#include <iprt/assert.h>
#include <iprt/uuid.h>
#include <VBox/vmm/pdmdev.h>
#include "Virtio_1_0.h"

#define INSTANCE(pState) pState->szInstance
#define IFACE_TO_STATE(pIface, ifaceName) ((VIRTIOSTATE *)((char*)(pIface) - RT_UOFFSETOF(VIRTIOSTATE, ifaceName)))

#ifdef LOG_ENABLED
# define QUEUENAME(s, q) (q->pcszName)
#endif

/* These ACCESSOR macros handle the most basic kinds of MMIO accesses to fields
 * virtio 1.0 spec's virtio_pci_common_cfg  avoiding a lot of visual bloat.
 */

 /**
  * If the physical address and access length is within the mapped capability struct
  * the ptrByType will be set to the mapped capability start. Otherwise ptrByType will be NULL.
  *
  * Implied parameters:
  *     GPhysAddr   - Physical address accessed (via MMIO callback)
  *     cb          - Number of bytes to access
  *
  * Actual Parameters:
  *     [IN]  pCapStruct - Pointer to MMIO mapped capability struct
  *     [IN]  type       - Capability struct type
  *     [OUT] result     - A pointer of type capType that will be set to a mapped capability
  *                       if phys. addr / access len is within it's span.
  *     [OUT] offset     - The offset of the physical address into the capability if applicable.
  */

#define MATCH_VIRTIO_CAP_STRUCT(pCapStruct, type, result, offset) \
        type *result = NULL; \
        if (   GCPhysAddr >= (RTGCPHYS)pCapStruct \
            && GCPhysAddr < ((RTGCPHYS)pCapStruct + sizeof(type)) \
            && cb <= sizeof(type)) \
        { \
            offset = GCPhysAddr - (RTGCPHYS)pCapStruct; \
            result = (type *)pCapStruct; \
        }

#define LOG_ACCESSOR(member, type) \
        LogFunc(("Guest %s 0x%x %s %s\n", fWrite ? "wrote" : "read ", \
                 *(type *)pv, fWrite ? "  to" : "from", #member));

#define LOG_INDEXED_ACCESSOR(member, type, idx) \
        LogFunc(("Guest %s 0x%x %s %s[%d]\n", fWrite ? "wrote" : "read ", \
                 *(type *)pv, fWrite ? "  to" : "from", #member, idx));

#define ACCESSOR(member, type) \
    { \
        if (fWrite) \
        { \
            pVirtio->member = *(type *)pv; \
        } \
        else \
        { \
            *(type *)pv = pVirtio->member; \
        } \
        LOG_ACCESSOR(member, type); \
    }
#define ACCESSOR_WITH_IDX(member, type, idx) \
    { \
        if (fWrite) \
        { \
            pVirtio->member[idx] = *(type *)pv; \
        } \
        else \
        { \
            *(type *)pv = pVirtio->member[idx]; \
        } \
        LOG_INDEXED_ACCESSOR(member, type, idx); \
    }

#define ACCESSOR_READONLY(member, type) \
    { \
        if (fWrite) \
        { \
            LogFunc(("Guest attempted to write readonly virtio_pci_common_cfg.%s\n", #member)); \
            AssertMsgFailed(("bad access\n")); \
        } \
        else \
        { \
            *(type *)pv = pVirtio->member; \
            LOG_ACCESSOR(member, type); \
        } \
    }

#define ACCESSOR_READONLY_WITH_IDX(member, type, idx) \
    { \
        if (fWrite) \
        { \
            LogFunc(("Guest attempted to write readonly virtio_pci_common_cfg.%s[%d]\n", #member, idx)); \
            AssertMsgFailed(("bad access\n")); \
        } \
        else \
        { \
            *(type *)pv = pVirtio->member[idx]; \
            LOG_INDEXED_ACCESSOR(member, type, idx); \
        } \
    }


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

int virtioReset(PVIRTIOSTATE pVirtio)
{
    RT_NOREF(pVirtio);
/* PK TODO Adapt to VirtIO 1.09
    pState->uGuestFeatures = 0;
    pState->uQueueSelector = 0;
    pState->uStatus        = 0;
    pState->uISR           = 0;

    for (unsigned i = 0; i < pState->nQueues; i++)
        virtQueueReset(&pState->Queues[i]);
*/
    virtioNotify(pVirtio);
    return VINF_SUCCESS;
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

void virtioNotify(PVIRTIOSTATE pVirtio)
{
    RT_NOREF(pVirtio);
}

static void virtioResetDevice(PVIRTIOSTATE pVirtio)
{
    RT_NOREF(pVirtio);
    LogFunc((""));
    pVirtio->uDeviceStatus = 0;
}

/**
 * Handle accesses to Common Configuration capability
 *
 * @returns VBox status code
 *
 * @param   pVirtio     Virtio instance state
 * @param   fWrite      If write access (otherwise read access)
 * @param   pv          Pointer to location to write to or read from
 * @param   cb          Number of bytes to read or write
 */
int virtioCommonCfgAccessed(PVIRTIOSTATE pVirtio, int fWrite, off_t uoff, void const *pv)
{
    int rv = VINF_SUCCESS;
    if (uoff == RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, uDeviceFeature))
    {
        if (fWrite) /* Guest WRITE pCommonCfg->uDeviceFeature */
        {
            AssertMsgFailed(("Guest attempted to write readonly virtio_pci_common_cfg.device_feature\n"));
        }
        else /* Guest WRITE pCommonCfg->uDeviceFeature */
        {
            switch(pVirtio->uFeaturesOfferedSelect)
            {
                case 0:
                    pVirtio->uFeaturesOffered = *(uint32_t *)pv;
                    LOG_ACCESSOR(uFeaturesOffered, uint32_t);
                    break;
                case 1:
                    pVirtio->uFeaturesOffered = (uint64_t)(*(uint32_t *)pv) << 32;
                    LOG_ACCESSOR(uFeaturesOffered, uint32_t);
                    break;
                default:
                    Log(("Guest selected out of range pVirtio->uDeviceFeature (%d), returning 0\n",
                        pVirtio->uFeaturesOfferedSelect));
            }
        }
    }
    else if (uoff == RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, uDriverFeature))
    {
        if (fWrite) /* Guest WRITE pCommonCfg->uDriverFeature */
        {
            switch(pVirtio->uFeaturesOfferedSelect)
            {
                case 0:
                    pVirtio->uFeaturesAccepted = *(uint32_t *)pv;
                    LOG_ACCESSOR(uFeaturesAccepted, uint32_t);
                    break;
                case 1:
                    pVirtio->uFeaturesAccepted = (uint64_t)(*(uint32_t *)pv) << 32;
                    LOG_ACCESSOR(uFeaturesAccepted, uint32_t);
                    break;
            }
        }
        else /* Guest READ pCommonCfg->uDriverFeature */
        {
            switch(pVirtio->uFeaturesOfferedSelect)
            {
                case 0:
                    *(uint32_t *)pv = pVirtio->uFeaturesAccepted & 0xffffffff;
                    LOG_ACCESSOR(uFeaturesAccepted, uint32_t);
                    break;
                case 1:
                    *(uint32_t *)pv = (pVirtio->uFeaturesAccepted >> 32) & 0xffffffff;
                    LOG_ACCESSOR(uFeaturesAccepted, uint32_t);
                    break;
            }
        }
    }
    else if (uoff == RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, uNumQueues))
    {
        if (fWrite)
        {
            AssertMsgFailed(("Guest attempted to write readonly virtio_pci_common_cfg.num_queues\n"));
        }
        else
        {
            *(uint16_t *)pv = MAX_QUEUES;
            Log(("Guest read 0x%x from pVirtio->uNumQueues\n", MAX_QUEUES));
        }
    }
    else if (uoff == RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, uDeviceStatus))
    {
        if (fWrite) /* Guest WRITE pCommonCfg->uDeviceStatus */
        {
            pVirtio->uDeviceStatus = *(uint8_t *)pv;
            LogFunc(("Driver wrote uDeviceStatus:\n"));
            showDeviceStatus(pVirtio->uDeviceStatus);
            if (pVirtio->uDeviceStatus == 0)
                virtioResetDevice(pVirtio);
        }
        else /* Guest READ pCommonCfg->uDeviceStatus */
        {
            LogFunc(("Driver read uDeviceStatus:\n"));
            *(uint32_t *)pv = pVirtio->uDeviceStatus;
            showDeviceStatus(pVirtio->uDeviceStatus);
        }
    }
    else if (uoff == RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, uMsixConfig))
    {
        ACCESSOR(uMsixConfig, uint32_t);
    }
    else if (uoff == RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, uDeviceFeatureSelect))
    {
        ACCESSOR(uFeaturesOfferedSelect, uint32_t);
    }
    else if (uoff == RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, uDriverFeatureSelect))
    {
        ACCESSOR(uFeaturesAcceptedSelect, uint32_t);
    }
    else if (uoff == RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, uConfigGeneration))
    {
        ACCESSOR_READONLY(uConfigGeneration, uint8_t);
    }
    else if (uoff == RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, uQueueSelect))
    {
        ACCESSOR(uQueueSelect, uint16_t);
    }
    else if (uoff == RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, uQueueSize))
    {
        ACCESSOR_WITH_IDX(uQueueSize, uint16_t, pVirtio->uQueueSelect);
    }
    else if (uoff == RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, uQueueMsixVector))
    {
        ACCESSOR_WITH_IDX(uQueueMsixVector, uint16_t, pVirtio->uQueueSelect);
    }
    else if (uoff == RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, uQueueEnable))
    {
        ACCESSOR_WITH_IDX(uQueueEnable, uint16_t, pVirtio->uQueueSelect);
    }
    else if (uoff == RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, uQueueNotifyOff))
    {
        ACCESSOR_READONLY_WITH_IDX(uQueueNotifyOff, uint16_t, pVirtio->uQueueSelect);
    }
    else if (uoff == RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, uQueueDesc))
    {
        ACCESSOR_WITH_IDX(uQueueDesc, uint64_t, pVirtio->uQueueSelect);
    }
    else if (uoff == RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, uQueueAvail))
    {
        ACCESSOR_WITH_IDX(uQueueAvail, uint64_t, pVirtio->uQueueSelect);
    }
    else if (uoff == RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, uQueueUsed))
    {
        ACCESSOR_WITH_IDX(uQueueUsed, uint64_t, pVirtio->uQueueSelect);
    }
    else
    {

        AssertMsgFailed(("virtio: Bad common cfg offset   \n"));
    }
    return rv;
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
PDMBOTHCBDECL(int) virtioR3MmioRead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb)
{
    RT_NOREF(pvUser);
    PVIRTIOSTATE pVirtio = PDMINS_2_DATA(pDevIns, PVIRTIOSTATE);
    int rc = VINF_SUCCESS;

//#ifdef LOG_ENABLED
//    LogFlowFunc(("pVirtio=%#p GCPhysAddr=%RGp pv=%#p{%.*Rhxs} cb=%u\n", pVirtio, GCPhysAddr, pv, cb, pv, cb));
//#endif
    off_t uoff = 0;
    MATCH_VIRTIO_CAP_STRUCT(pVirtio->pGcPhysCommonCfg, VIRTIO_PCI_COMMON_CFG_T, pCommonCfg,  uoff);
    MATCH_VIRTIO_CAP_STRUCT(pVirtio->pGcPhysIsrCap,    VIRTIO_PCI_ISR_CAP_T,    pIsrCap,     uoff);
#if HAVE_VIRTIO_DEVICE_SPECIFIC_CAP
    MATCH_VIRTIO_CAP_STRUCT(pVirtio->pGcPhysDeviceCap, VIRTIO_PCI_DEVICE_CAP_T, pDeviceCap,  uoff);
#endif
    /* Note: The notify capability is handled differently as per VirtIO 1.0 spec 4.1.4.4 */

    if (pCommonCfg)
        virtioCommonCfgAccessed(pVirtio, 0 /* fWrite */, uoff, pv);
    else if (pIsrCap)
    {
        *(uint8_t *)pv = pVirtio->fQueueInterrupt | pVirtio->fDeviceConfigInterrupt << 1;
        LogFunc(("Read 0x%s from pIsrCap\n", *(uint8_t *)pv));
    }
#if HAVE_VIRTIO_DEVICE_SPECIFIC_CAP
    else if (pDeviceCap)
        rc = pThis->pfnVirtioDevCapRead(pDevIns, GCPhysAddr, pv, cb);
#endif
    else
        AssertMsgFailed(("virtio: Write outside of capabilities region\n"));

    return rc;
}
#if TBD
/**
  * Callback function for reading from the PCI configuration space.
  *
  * @returns The register value.
  * @param   pDevIns         Pointer to the device instance the PCI device
  *                          belongs to.
  * @param   pPciDev         Pointer to PCI device. Use pPciDev->pDevIns to get the device instance.
  * @param   uAddress        The configuration space register address. [0..4096]
  * @param   cb              The register size. [1,2,4]
  *
  * @remarks Called with the PDM lock held.  The device lock is NOT take because
  *          that is very likely be a lock order violation.
  */
static DECLCALLBACK(uint32_t) virtioPciConfigRead(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev,
                                       uint32_t uAddress, unsigned cb)
{
    PVIRTIOSTATE pVirtio = PDMINS_2_DATA(pDevIns, PVIRTIOSTATE);
    int rc = VINF_SUCCESS;

    LogFunc(("uAddress: %d, uPciConfigDataOffset: %d\n", uAddress, pVirtio->uPciConfigDataOffset));

    if (uAddress == pVirtio->uPciConfigDataOffset)
        Log(("Read uPciConfigDataOffset\n"));
    rc = pVirtio->pfnPciConfigReadOld(pDevIns, pPciDev, uAddress, cb);
    return rc;

}

/**
 * Callback function for writing to the PCI configuration space.
 *
 * @returns VINF_SUCCESS or PDMDevHlpDBGFStop status.
 *
 * @param   pDevIns         Pointer to the device instance the PCI device
 *                          belongs to.
 * @param   pPciDev         Pointer to PCI device. Use pPciDev->pDevIns to get the device instance.
 * @param   uAddress        The configuration space register address. [0..4096]
 * @param   u32Value        The value that's being written. The number of bits actually used from
 *                          this value is determined by the cb parameter.
 * @param   cb              The register size. [1,2,4]
 *
 * @remarks Called with the PDM lock held.  The device lock is NOT take because
 *          that is very likely be a lock order violation.
 */
static DECLCALLBACK(VBOXSTRICTRC) virtioPciConfigWrite(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev,
                                        uint32_t uAddress, uint32_t u32Value, unsigned cb)
{
    PVIRTIOSTATE pVirtio = PDMINS_2_DATA(pDevIns, PVIRTIOSTATE);
    VBOXSTRICTRC strictRc;

    LogFunc(("uAddress: %d, uPciConfigDataOffset: %d\n", uAddress, pVirtio->uPciConfigDataOffset));
    if (uAddress == pVirtio->uPciConfigDataOffset)
        Log(("Wrote uPciConfigDataOffset\n"));
    strictRc = pVirtio->pfnPciConfigWriteOld(pDevIns, pPciDev, uAddress, u32Value, cb);
    return strictRc;
}
#endif
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
PDMBOTHCBDECL(int) virtioR3MmioWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void const *pv, unsigned cb)
{

    RT_NOREF(pvUser);
    PVIRTIOSTATE pVirtio = PDMINS_2_DATA(pDevIns, PVIRTIOSTATE);
    int rc = VINF_SUCCESS;

#ifdef LOG_ENABLED
//    LogFunc(("pVirtio=%#p GCPhysAddr=%RGp pv=%#p{%.*Rhxs} cb=%u\n", pVirtio, GCPhysAddr, pv, cb, pv, cb));
#endif
    off_t uoff = 0;

    MATCH_VIRTIO_CAP_STRUCT(pVirtio->pGcPhysCommonCfg, VIRTIO_PCI_COMMON_CFG_T, pCommonCfg,  uoff);
    MATCH_VIRTIO_CAP_STRUCT(pVirtio->pGcPhysIsrCap,    VIRTIO_PCI_ISR_CAP_T,    pIsrCap,     uoff);
#if HAVE_VIRTIO_DEVICE_SPECIFIC_CAP
    MATCH_VIRTIO_CAP_STRUCT(pVirtio->pGcPhysDeviceCap, VIRTIO_PCI_DEVICE_CAP_T, pDeviceCap,  uoff);
#endif

    if (pCommonCfg)
        virtioCommonCfgAccessed(pVirtio, 1 /* fWrite */, uoff, pv);
    else if (pIsrCap)
    {
        pVirtio->fQueueInterrupt = (*(uint8_t *)pv) & 1;
        pVirtio->fDeviceConfigInterrupt = !!(*((uint8_t *)pv) & 2);
        Log(("pIsrCap... setting fQueueInterrupt=%d fDeviceConfigInterrupt=%d\n",
              pVirtio->fQueueInterrupt, pVirtio->fDeviceConfigInterrupt));
    }
#if HAVE_VIRTIO_DEVICE_SPECIFIC_CAP
    else if (pDeviceCap)
        rc = pThis->pfnVirtioDevCapWrite(pDevIns, GCPhysAddr, pv, cb);
#endif
    else
        AssertMsgFailed(("virtio: Write outside of capabilities region\n"));

    return rc;
}


/**
 * @callback_method_impl{FNPCIIOREGIONMAP}
 */
static DECLCALLBACK(int) virtioR3Map(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev, uint32_t iRegion,
                                     RTGCPHYS GCPhysAddress, RTGCPHYS cb, PCIADDRESSSPACE enmType)
{
    RT_NOREF3(pPciDev, iRegion, enmType);
    PVIRTIOSTATE  pVirtio = PDMINS_2_DATA(pDevIns, PVIRTIOSTATE);
    int rc = VINF_SUCCESS;

    Assert(cb >= 32);

    /* We use the assigned size here, because we currently only support page aligned MMIO ranges. */
    rc = PDMDevHlpMMIORegister(pDevIns, GCPhysAddress, cb, NULL /*pvUser*/,
                           IOMMMIO_FLAGS_READ_PASSTHRU | IOMMMIO_FLAGS_WRITE_PASSTHRU,
                           virtioR3MmioWrite, virtioR3MmioRead,
                           "virtio-scsi MMIO");

    LogFunc(("virtio: PCI Capabilities mapped at GCPhysAddr=%RGp cb=%RGp, region=%d\n", GCPhysAddress, cb, iRegion));

    if (RT_SUCCESS(rc))
    {
        pVirtio->GCPhysPciCapBase = GCPhysAddress;
        pVirtio->pGcPhysCommonCfg = (PVIRTIO_PCI_COMMON_CFG_T)(GCPhysAddress + pVirtio->uCommonCfgOffset);
        pVirtio->pGcPhysNotifyCap = (PVIRTIO_PCI_NOTIFY_CAP_T)(GCPhysAddress + pVirtio->uNotifyCapOffset);
        pVirtio->pGcPhysIsrCap    = (PVIRTIO_PCI_ISR_CAP_T)(GCPhysAddress + pVirtio->uIsrCapOffset);
        pVirtio->pGcPhysPciCfgCap = (PVIRTIO_PCI_CFG_CAP_T)(GCPhysAddress + pVirtio->uPciCfgCapOffset);
#ifdef HAVE_VIRTIO_DEVICE_SPECIFIC_CAP
        pVirtio->pGcPhysDeviceCap = (PVIRTIO_PCI_DEVICE_CAP_T)(GCPhysAddress + pVirtio->uuDeviceCapOffset);
#endif
    }
    return rc;
}


/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
void *virtioQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    VIRTIOSTATE *pThis = IFACE_TO_STATE(pInterface, IBase);
    Assert(&pThis->IBase == pInterface);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pThis->IBase);
    return NULL;
}

/**
 * Get VirtIO available host-side features
 *
 * @returns feature bits selected or 0 if selector out of range.
 *
 * @param   pState          Virtio state
 * @param   uSelect         Selects which 32-bit set of feature information to return
 */

static uint64_t virtioGetHostFeatures(PVIRTIOSTATE pVirtio)
{
    return pVirtio->uFeaturesOffered;
}

/**
 *
 * Set VirtIO available host-side features
 *
 * @returns VBox status code
 *
 * @param   pState              Virtio state
 * @param   uFeaturesOffered    Feature bits (0-63) to set
 */

static int virtioSetHostFeatures(PVIRTIOSTATE pVirtio, uint64_t uFeaturesOffered)
{
    pVirtio->uFeaturesOffered = VIRTIO_F_VERSION_1 | uFeaturesOffered;
    return VINF_SUCCESS;
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
 * @param   uNotifyOffMultiplier  See VirtIO 1.0 spec 4.1.4.4 re: virtio_pci_notify_cap
 */

int   virtioConstruct(PPDMDEVINS pDevIns, PVIRTIOSTATE pVirtio, int iInstance, PVIRTIOPCIPARAMS pPciParams,
                    PPVIRTIOCALLBACKS ppVirtioCallbacks, const char *pcszNameFmt, uint32_t nQueues, uint32_t uVirtioRegion,
                    PFNVIRTIODEVCAPREAD devCapReadCallback, PFNVIRTIODEVCAPWRITE devCapWriteCallback,
                    uint16_t cbDevSpecificCap, uint32_t uNotifyOffMultiplier)
{
    pVirtio->nQueues = nQueues;
    pVirtio->uNotifyOffMultiplier = uNotifyOffMultiplier;

    /* Init handles and log related stuff. */
    RTStrPrintf(pVirtio->szInstance, sizeof(pVirtio->szInstance), pcszNameFmt, iInstance);

    VIRTIOCALLBACKS virtioCallbacks;
    virtioCallbacks.pfnSetHostFeatures = virtioSetHostFeatures;
    virtioCallbacks.pfnGetHostFeatures = virtioGetHostFeatures;
    virtioCallbacks.pfnReset = virtioReset;
    *ppVirtioCallbacks = &virtioCallbacks;

    pVirtio->pDevInsR3 = pDevIns;
    pVirtio->pDevInsR0 = PDMDEVINS_2_R0PTR(pDevIns);
    pVirtio->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);
    pVirtio->uDeviceStatus = 0;
    pVirtio->pfnVirtioDevCapRead     = devCapReadCallback;
    pVirtio->pfnVirtioDevCapWrite    = devCapWriteCallback;

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

    int rc = VINF_SUCCESS;
    /* Register PCI device */
    rc = PDMDevHlpPCIRegister(pDevIns, &pVirtio->dev);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
            N_("virtio: cannot register PCI Device")); /* can we put params in this error? */

    pVirtio->IBase = pDevIns->IBase;

    /** Construct & map PCI vendor-specific capabilities for virtio host negotiation with guest driver */

#if 0 && defined(VBOX_WITH_MSI_DEVICES)  /* T.B.D. */
    uint8_t fMsiSupport = true;
#else
    uint8_t fMsiSupport = false;
#endif

    uint8_t uCfgCapOffset = 0x40;
    PVIRTIO_PCI_NOTIFY_CAP_T pNotifyCap;
    PVIRTIO_PCI_CAP_T pCommonCfg, pIsrCap;
#ifdef HAVE_VIRTIO_DEVICE_SPECIFIC_CAP
    PVIRTIO_PCI_CAP_T pDeviceCap;
#endif
    uint32_t cbVirtioCaps = 0;

    /* Capability will be mapped via VirtIO 1.0: struct virtio_pci_cfg_cap (VIRTIO_PCI_CAP_T) */
    pVirtio->pPciCfgCap = (PVIRTIO_PCI_CFG_CAP_T)&pVirtio->dev.abConfig[uCfgCapOffset];
    pVirtio->uPciConfigDataOffset = 0x40 + sizeof(VIRTIO_PCI_CAP_T);
    PVIRTIO_PCI_CAP_T pCfg = (PVIRTIO_PCI_CAP_T)pVirtio->pPciCfgCap;
    pCfg->uCapVndr = VIRTIO_PCI_CAP_ID_VENDOR;
    pCfg->uCapNext = uCfgCapOffset += sizeof(VIRTIO_PCI_CFG_CAP_T);
    pCfg->uCapLen  = sizeof(VIRTIO_PCI_CFG_CAP_T);
    pCfg->uCfgType = VIRTIO_PCI_CAP_PCI_CFG;
    pCfg->uBar     = uVirtioRegion;
    pCfg->uOffset  = pVirtio->uPciCfgCapOffset = 0;
    pCfg->uLength  = sizeof(VIRTIO_PCI_CFG_CAP_T);
    cbVirtioCaps  += pCfg->uLength;

    /* Capability will be mapped via VirtIO 1.0: struct virtio_pci_common_cfg (VIRTIO_PCI_COMMON_CFG_T)*/
    pCfg = pCommonCfg = (PVIRTIO_PCI_CAP_T)&pVirtio->dev.abConfig[uCfgCapOffset];
    pCfg->uCapVndr = VIRTIO_PCI_CAP_ID_VENDOR;
    pCfg->uCapNext = uCfgCapOffset += sizeof(VIRTIO_PCI_CAP_T);
    pCfg->uCapLen  = sizeof(VIRTIO_PCI_CAP_T);
    pCfg->uCfgType = VIRTIO_PCI_CAP_COMMON_CFG;
    pCfg->uBar     = uVirtioRegion;
    pCfg->uOffset  = pVirtio->uCommonCfgOffset = pVirtio->pPciCfgCap->pciCap.uOffset + sizeof(VIRTIO_PCI_CFG_CAP_T);
    pCfg->uLength  = sizeof(VIRTIO_PCI_COMMON_CFG_T);
    cbVirtioCaps  += pCfg->uLength;

    /* Capability will be mapped via VirtIO 1.0: struct virtio_pci_notify_cap (VIRTIO_PCI_NOTIFY_CAP_T)*/
    pNotifyCap = (PVIRTIO_PCI_NOTIFY_CAP_T)&pVirtio->dev.abConfig[uCfgCapOffset];
    pCfg = (PVIRTIO_PCI_CAP_T)pNotifyCap;
    pCfg->uCapVndr = VIRTIO_PCI_CAP_ID_VENDOR;
    pCfg->uCapNext = uCfgCapOffset += sizeof(VIRTIO_PCI_NOTIFY_CAP_T);
    pCfg->uCapLen  = sizeof(VIRTIO_PCI_NOTIFY_CAP_T);
    pCfg->uCfgType = VIRTIO_PCI_CAP_NOTIFY_CFG;
    pCfg->uBar     = uVirtioRegion;
    pCfg->uOffset  = pVirtio->uNotifyCapOffset = pCommonCfg->uOffset + sizeof(VIRTIO_PCI_COMMON_CFG_T);
    pCfg->uLength  = sizeof(VIRTIO_PCI_NOTIFY_CAP_T);
    pNotifyCap->uNotifyOffMultiplier = uNotifyOffMultiplier;
    cbVirtioCaps  += pCfg->uLength;

    /* Capability will be mapped via VirtIO 1.0: uint8_t (VIRTIO_PCI_ISR_CAP_T)  */
    pCfg = pIsrCap = (PVIRTIO_PCI_CAP_T)&pVirtio->dev.abConfig[uCfgCapOffset];
    pCfg->uCapVndr = VIRTIO_PCI_CAP_ID_VENDOR;
    pCfg->uCapNext = (uint8_t)(fMsiSupport || cbDevSpecificCap ? (uCfgCapOffset += sizeof(VIRTIO_PCI_ISR_CAP_T)) : 0);
    pCfg->uCapLen  = sizeof(VIRTIO_PCI_CAP_T);
    pCfg->uCfgType = VIRTIO_PCI_CAP_ISR_CFG;
    pCfg->uBar     = uVirtioRegion;
    pCfg->uOffset  = pVirtio->uIsrCapOffset = pNotifyCap->pciCap.uOffset + sizeof(VIRTIO_PCI_NOTIFY_CAP_T);
    pCfg->uLength  = sizeof(VIRTIO_PCI_ISR_CAP_T);
    cbVirtioCaps  += pCfg->uLength;

#ifdef HAVE_VIRTIO_DEVICE_SPECIFIC_CAP
    /* Capability will be mapped via VirtIO 1.0: struct virtio_pci_dev_cap (VIRTIODEVCAP)*/
    pCfg = pDeviceCap = (PVIRTIO_PCI_CAP_T)&pVirtio->dev.abConfig[uCfgCapOffset];
    pCfg->uCapVndr = VIRTIO_PCI_CAP_ID_VENDOR;
    pCfg->uCapNext = (uint8_t)(fMsiSupport ? (uCfgCapOffset += sizeof(VIRTIO_PCI_CAP_T)) : 0);
    pCfg->uCapLen  = sizeof(VIRTIO_PCI_CAP_T);
    pCfg->uCfgType = VIRTIO_PCI_CAP_DEVICE_CFG;
    pCfg->uBar     = uVirtioRegion;
    pCfg->uOffset  = uDeviceCapOffset->uOffset + sizeof(VIRTIO_PCI_ISR_CAP_T);
    pCfg->uLength  = cbDevSpecificCap;
    cbVirtioCaps  += pCfg->uLength;
#endif

    /* Set offset to first capability and enable PCI dev capabilities */
    PCIDevSetCapabilityList    (&pVirtio->dev, 0x40);
    PCIDevSetStatus            (&pVirtio->dev, VBOX_PCI_STATUS_CAP_LIST);

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
            /* PK TODO: The following is moot, we need to flag no MSI-X support */
            PCIDevSetCapabilityList(&pVirtio->dev, 0x40);
    }
/*
    PDMDevHlpPCISetConfigCallbacks(pDevIns, &pVirtio->dev,
                virtioPciConfigRead,  &pVirtio->pfnPciConfigReadOld,
                virtioPciConfigWrite, &pVirtio->pfnPciConfigWriteOld);
*/
    rc = PDMDevHlpPCIIORegionRegister(pDevIns, uVirtioRegion, cbVirtioCaps,
                                      PCI_ADDRESS_SPACE_MEM, virtioR3Map);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
            N_("virtio: cannot register PCI Capabilities address space"));

    return rc;
}

#endif /* IN_RING3 */

#endif /* VBOX_DEVICE_STRUCT_TESTCASE */

