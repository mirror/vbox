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


/**
* Returns true if physical address and access length are within the mapped capability struct.
*
* Actual Parameters:
*     [IN]  pPhysCapStruct - Pointer to MMIO mapped capability struct
*     [IN]  pCfgCap        - Pointer to capability in PCI configuration area
*     [OUT] fMatched       - True if GCPhysAddr is within the physically mapped capability.
*
* Implied parameters:
*     [IN]  GCPhysAddr     - Physical address accessed (via MMIO callback)
*     [IN]  cb             - Number of bytes to access
*     [OUT] result          - true or false
*
*/
#define MATCH_VIRTIO_CAP_STRUCT(pGcPhysCapData, pCfgCap, fMatched) \
        bool fMatched = false; \
        if (pGcPhysCapData && pCfgCap && GCPhysAddr >= (RTGCPHYS)pGcPhysCapData \
            && GCPhysAddr < ((RTGCPHYS)pGcPhysCapData + ((PVIRTIO_PCI_CAP_T)pCfgCap)->uLength) \
            && cb <= ((PVIRTIO_PCI_CAP_T)pCfgCap)->uLength) \
                fMatched = true;

/**
 * This macro resolves to boolean true if uoff is within the specified member offset and length.
 *
 * Actual Parameters:
 *     [IN] member      - Member of VIRTIO_PCI_COMMON_CFG_T
 *
 * Implied Parameters:
 *     [IN]  uoff        - Offset into VIRTIO_PCI_COMMON_CFG_T
 *     [IN]  cb          - Number of bytes to access
 *     [OUT] result      - true or false
 */
#define COMMON_CFG(member) \
           (uoff >= RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, member) \
        &&  uoff < (uint32_t)(RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, member) \
                             + RT_SIZEOFMEMB(VIRTIO_PCI_COMMON_CFG_T, member)) \
        &&   cb <= RT_SIZEOFMEMB(VIRTIO_PCI_COMMON_CFG_T, member) \
                  - (uoff - RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, member)))

#define LOG_ACCESSOR(member) \
        LogFunc(("Guest %s %s[%d:%d]: %.*Rhxs\n", \
                  fWrite ? "wrote" : "read ", #member, foff, foff + cb, cb, pv));

#define LOG_INDEXED_ACCESSOR(member, idx) \
        LogFunc(("Guest %s %s[%d][%d:%d]: %.*Rhxs\n", \
                  fWrite ? "wrote" : "read ", #member, idx, foff, foff + cb, cb, pv));

#define ACCESSOR(member) \
    { \
        uint32_t foff = uoff - RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, member); \
        if (fWrite) \
            memcpy(((char *)&pVirtio->member) + uoff, (const char *)pv, cb); \
        else \
            memcpy((char *)pv, (const char *)(((char *)&pVirtio->member) + uoff), cb); \
        LOG_ACCESSOR(member); \
    }

#define ACCESSOR_WITH_IDX(member, idx) \
    { \
        uint32_t foff = uoff - RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, member); \
        if (fWrite) \
            memcpy(((char *)(pVirtio->member + idx)) + uoff, (const char *)pv, cb); \
        else \
            memcpy((char *)pv, (const char *)(((char *)(pVirtio->member + idx)) + uoff), cb); \
        LOG_INDEXED_ACCESSOR(member, idx); \
    }

#define ACCESSOR_READONLY(member) \
    { \
        uint32_t foff = uoff - RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, member); \
        if (fWrite) \
            LogFunc(("Guest attempted to write readonly virtio_pci_common_cfg.%s\n", #member)); \
        else \
        { \
            memcpy((char *)pv, (const char *)(((char *)&pVirtio->member) + uoff), cb); \
            LOG_ACCESSOR(member); \
        } \
    }

#define ACCESSOR_READONLY_WITH_IDX(member, idx) \
    { \
        uint32_t foff = uoff - RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, member); \
        if (fWrite) \
            LogFunc(("Guest attempted to write readonly virtio_pci_common_cfg.%s[%d]\n", #member, idx)); \
        else \
        { \
            memcpy((char *)pv, ((char *)(pVirtio->member + idx)) + uoff, cb); \
            LOG_INDEXED_ACCESSOR(member, idx); \
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
__attribute__((unused))
int virtioRaiseInterrupt(VIRTIOSTATE *pState, int rcBusy, uint8_t u8IntCause)
{
    RT_NOREF2(pState, u8IntCause);
    RT_NOREF_PV(rcBusy);
    LogFlow(("%s virtioRaiseInterrupt: u8IntCause=%x\n",
             INSTANCE(pState), u8IntCause));

    pState->uISR |= u8IntCause;
    PDMDevHlpPCISetIrq(pState->CTX_SUFF(pDevIns), 0, 1);
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


int virtioCommonCfgAccessed(PVIRTIOSTATE pVirtio, int fWrite, off_t uoff, unsigned cb, void const *pv)
{
    int rv = VINF_SUCCESS;
    uint64_t val;
    if (COMMON_CFG(uDeviceFeatures))
    {
        if (fWrite) /* Guest WRITE pCommonCfg>uDeviceFeatures */
            Log(("Guest attempted to write readonly virtio_pci_common_cfg.device_feature\n"));
        else /* Guest READ pCommonCfg->uDeviceFeatures */
        {
            uint32_t foff = uoff - RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, uDeviceFeatures);
            switch(pVirtio->uDeviceFeaturesSelect)
            {
                case 0:
                    val = pVirtio->uDeviceFeatures & 0xffffffff;
                    memcpy((void *)pv, (const void *)&val, cb);
                    LogFunc(("Guest read  uDeviceFeatures(LO)[%d:%d]: %.*Rhxs\n",
                              foff, foff + cb, cb, pv));
                    break;
                case 1:
                    val = (pVirtio->uDeviceFeatures >> 32) & 0xffffffff;
                    memcpy((void *)pv, (const void *)&val, cb);
                    LogFunc(("Guest read  uDeviceFeatures(HI)[%d:%d]: %.*Rhxs\n",
                              foff, foff + cb, cb, pv));
                    break;
                default:
                    Log(("Guest read uDeviceFeatures with out of range selector (%d), returning 0\n",
                          pVirtio->uDeviceFeaturesSelect));
            }
        }
    }
    else if (COMMON_CFG(uDriverFeatures))
    {
        if (fWrite) /* Guest WRITE pCommonCfg->udriverFeatures */
        {
            uint32_t foff = uoff - RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, uDriverFeatures);
            switch(pVirtio->uDriverFeaturesSelect)
            {
                case 0:
                    memcpy(&pVirtio->uDriverFeatures, pv, cb);
                    LogFunc(("Guest wrote uDriverFeatures(LO)[%d:%d]: %.*Rhxs\n",
                              foff, foff + cb, cb, pv));
                    break;
                case 1:
                    memcpy(((char *)&pVirtio->uDriverFeatures) + sizeof(uint32_t), pv, cb);
                    LogFunc(("Guest wrote uDriverFeatures(HI)[%d:%d]: %.*Rhxs\n",
                              foff, foff + cb, cb, pv));
                    break;
            }
        }
        else /* Guest READ pCommonCfg->udriverFeatures */
        {
            uint32_t foff = uoff - RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, uDriverFeatures);
            switch(pVirtio->uDriverFeaturesSelect)
            {
                case 0:
                    val = pVirtio->uDriverFeatures & 0xffffffff;
                    memcpy((void *)pv, (const void *)&val, cb);
                    LogFunc(("Guest read  uDriverFeatures(LO)[%d:%d]: %.*Rhxs\n",
                              foff, foff + cb, cb, pv));
                    break;
                case 1:
                    val = (pVirtio->uDriverFeatures >> 32) & 0xffffffff;
                    memcpy((void *)pv, (const void *)&val, cb);
                    LogFunc(("Guest read  uDriverFeatures(HI)[%d:%d]: %.*Rhxs\n",
                              foff, foff + cb, cb, pv));
                    break;
                default:
                    Log(("Guest read uDriverFeatures with out of range selector (%d), returning 0\n",
                         pVirtio->uDriverFeaturesSelect));
            }
        }
    }
    else if (COMMON_CFG(uNumQueues))
    {
        if (fWrite)
            Log(("Guest attempted to write readonly virtio_pci_common_cfg.num_queues\n"));
        else
        {
            *(uint16_t *)pv = MAX_QUEUES;
            Log(("Guest read 0x%x from pVirtio->uNumQueues\n", MAX_QUEUES));
        }
    }
    else if (COMMON_CFG(uDeviceStatus))
    {
        if (fWrite) /* Guest WRITE pCommonCfg->uDeviceStatus */
        {
            pVirtio->uDeviceStatus = *(uint8_t *)pv;
            LogFunc(("Driver wrote uDeviceStatus:\n"));
            logDeviceStatus(pVirtio->uDeviceStatus);
            if (pVirtio->uDeviceStatus == 0)
                virtioResetDevice(pVirtio);
        }
        else /* Guest READ pCommonCfg->uDeviceStatus */
        {
            LogFunc(("Driver read uDeviceStatus:\n"));
            *(uint32_t *)pv = pVirtio->uDeviceStatus;
            logDeviceStatus(pVirtio->uDeviceStatus);
        }
    }
    else if (COMMON_CFG(uMsixConfig))
    {
        ACCESSOR(uMsixConfig);
    }
    else if (COMMON_CFG(uDeviceFeaturesSelect))
    {
        ACCESSOR(uDeviceFeaturesSelect);
    }
    else if (COMMON_CFG(uDriverFeaturesSelect))
    {
        ACCESSOR(uDriverFeaturesSelect);
    }
    else if (COMMON_CFG(uConfigGeneration))
    {
        ACCESSOR_READONLY(uConfigGeneration);
    }
    else if (COMMON_CFG(uQueueSelect))
    {
        ACCESSOR(uQueueSelect);
    }
    else if (COMMON_CFG(uQueueSize))
    {
        ACCESSOR_WITH_IDX(uQueueSize, pVirtio->uQueueSelect);
    }
    else if (COMMON_CFG(uQueueMsixVector))
    {
        ACCESSOR_WITH_IDX(uQueueMsixVector, pVirtio->uQueueSelect);
    }
    else if (COMMON_CFG(uQueueEnable))
    {
        ACCESSOR_WITH_IDX(uQueueEnable, pVirtio->uQueueSelect);
    }
    else if (COMMON_CFG(uQueueNotifyOff))
    {
        ACCESSOR_READONLY_WITH_IDX(uQueueNotifyOff, pVirtio->uQueueSelect);
    }
    else if (COMMON_CFG(uQueueDesc))
    {
        ACCESSOR_WITH_IDX(uQueueDesc, pVirtio->uQueueSelect);
    }
    else if (COMMON_CFG(uQueueAvail))
    {
        ACCESSOR_WITH_IDX(uQueueAvail, pVirtio->uQueueSelect);
    }
    else if (COMMON_CFG(uQueueUsed))
    {
        ACCESSOR_WITH_IDX(uQueueUsed, pVirtio->uQueueSelect);
    }
    else
    {
        LogFunc(("Bad access by guest to virtio_pci_common_cfg: uoff=%d, cb=%d\n", uoff, cb));
        rv = VERR_ACCESS_DENIED;
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
//    LogFunc(("pVirtio=%#p GCPhysAddr=%RGp pv=%#p{%.*Rhxs} cb=%u\n", pVirtio, GCPhysAddr, pv, cb, pv, cb));
//#endif

    MATCH_VIRTIO_CAP_STRUCT(pVirtio->pGcPhysDeviceCap, pVirtio->pDeviceCap,     fDevSpecific);
    MATCH_VIRTIO_CAP_STRUCT(pVirtio->pGcPhysCommonCfg, pVirtio->pCommonCfgCap,  fCommonCfg);
    MATCH_VIRTIO_CAP_STRUCT(pVirtio->pGcPhysIsrCap,    pVirtio->pIsrCap,        fIsrCap);

    if (fDevSpecific)
    {
        uint32_t uDevSpecificDataOffset = GCPhysAddr - pVirtio->pGcPhysDeviceCap;
        rc = pVirtio->virtioCallbacks.pfnVirtioDevCapRead(pDevIns, uDevSpecificDataOffset, pv, cb);
    }
    else
    if (fCommonCfg)
    {
        uint32_t uCommonCfgDataOffset = GCPhysAddr - pVirtio->pGcPhysCommonCfg;
        virtioCommonCfgAccessed(pVirtio, 0 /* fWrite */, uCommonCfgDataOffset, cb, pv);
    }
    else
    if (fIsrCap)
    {
        *(uint8_t *)pv = pVirtio->fQueueInterrupt | pVirtio->fDeviceConfigInterrupt << 1;
        LogFunc(("Read 0x%s from pIsrCap\n", *(uint8_t *)pv));
    }
    else {

        AssertMsgFailed(("virtio: Read outside of capabilities region: GCPhysAddr=%RGp cb=%RGp\n", GCPhysAddr, cb));
    }
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
PDMBOTHCBDECL(int) virtioR3MmioWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void const *pv, unsigned cb)
{
    RT_NOREF(pvUser);
    PVIRTIOSTATE pVirtio = PDMINS_2_DATA(pDevIns, PVIRTIOSTATE);
    int rc = VINF_SUCCESS;

//#ifdef LOG_ENABLED
//    LogFunc(("pVirtio=%#p GCPhysAddr=%RGp pv=%#p{%.*Rhxs} cb=%u\n", pVirtio, GCPhysAddr, pv, cb, pv, cb));
//#endif

    MATCH_VIRTIO_CAP_STRUCT(pVirtio->pGcPhysDeviceCap, pVirtio->pDeviceCap,     fDevSpecific);
    MATCH_VIRTIO_CAP_STRUCT(pVirtio->pGcPhysCommonCfg, pVirtio->pCommonCfgCap,  fCommonCfg);
    MATCH_VIRTIO_CAP_STRUCT(pVirtio->pGcPhysIsrCap,    pVirtio->pIsrCap,        fIsrCap);

    if (fDevSpecific)
    {
        uint32_t uDevSpecificDataOffset = GCPhysAddr - pVirtio->pGcPhysDeviceCap;
        rc = pVirtio->virtioCallbacks.pfnVirtioDevCapWrite(pDevIns, uDevSpecificDataOffset, pv, cb);
    }
    else
    if (fCommonCfg)
    {
        uint32_t uCommonCfgDataOffset = GCPhysAddr - pVirtio->pGcPhysCommonCfg;
        virtioCommonCfgAccessed(pVirtio, 1 /* fWrite */, uCommonCfgDataOffset, cb, pv);
    }
    else
    if (fIsrCap)
    {
        pVirtio->fQueueInterrupt = (*(uint8_t *)pv) & 1;
        pVirtio->fDeviceConfigInterrupt = !!(*((uint8_t *)pv) & 2);
        Log(("pIsrCap... setting fQueueInterrupt=%d fDeviceConfigInterrupt=%d\n",
              pVirtio->fQueueInterrupt, pVirtio->fDeviceConfigInterrupt));
    }
    else
    {
       LogFunc(("virtio: Write outside of capabilities region:\nGCPhysAddr=%RGp cb=%RGp,", GCPhysAddr, cb));
    }
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

    if (iRegion == pVirtio->uVirtioCapRegion)
    {
        /* We use the assigned size here, because we currently only support page aligned MMIO ranges. */
        rc = PDMDevHlpMMIORegister(pDevIns, GCPhysAddress, cb, NULL /*pvUser*/,
                           IOMMMIO_FLAGS_READ_PASSTHRU | IOMMMIO_FLAGS_WRITE_PASSTHRU,
                           virtioR3MmioWrite, virtioR3MmioRead,
                           "virtio-scsi MMIO");

        if (RT_FAILURE(rc))
        {
            LogFunc(("virtio: PCI Capabilities failed to map GCPhysAddr=%RGp cb=%RGp, region=%d\n",
                    GCPhysAddress, cb, iRegion));
            return rc;
        }
        LogFunc(("virtio: PCI Capabilities mapped at GCPhysAddr=%RGp cb=%RGp, region=%d\n",
                GCPhysAddress, cb, iRegion));
        pVirtio->GCPhysPciCapBase = GCPhysAddress;
        pVirtio->pGcPhysCommonCfg = GCPhysAddress + pVirtio->pCommonCfgCap->uOffset;
        pVirtio->pGcPhysNotifyCap = GCPhysAddress + pVirtio->pNotifyCap->pciCap.uOffset;
        pVirtio->pGcPhysIsrCap    = GCPhysAddress + pVirtio->pIsrCap->uOffset;
        if (pVirtio->fHaveDevSpecificCap)
            pVirtio->pGcPhysDeviceCap = GCPhysAddress + pVirtio->pDeviceCap->uOffset;
    }
    return rc;
}

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

    if (uAddress == (uint64_t)&pVirtio->pPciCfgCap->uPciCfgData)
    {
        /* VirtIO 1.0 spec section 4.1.4.7 describes a required alternative access capability
         * whereby the guest driver can specify a bar, offset, and length via the PCI configuration space
         * (the virtio_pci_cfg_cap capability), and access data items. */
        uint32_t uLength = pVirtio->pPciCfgCap->pciCap.uLength;
        uint32_t uOffset = pVirtio->pPciCfgCap->pciCap.uOffset;
        uint8_t  uBar    = pVirtio->pPciCfgCap->pciCap.uBar;
        uint32_t pv = 0;
        if (uBar == pVirtio->uVirtioCapRegion)
            (void)virtioR3MmioRead(pDevIns, NULL, (RTGCPHYS)((uint32_t)pVirtio->GCPhysPciCapBase + uOffset),
                                    &pv, uLength);
        else
        {
            LogFunc(("Guest read virtio_pci_cfg_cap.pci_cfg_data using unconfigured BAR. Ignoring"));
            return 0;
        }
        LogFunc(("virtio: Guest read  virtio_pci_cfg_cap.pci_cfg_data, bar=%d, offset=%d, length=%d, result=%d\n",
                uBar, uOffset, uLength, pv));
        return pv;
    }
    return pVirtio->pfnPciConfigReadOld(pDevIns, pPciDev, uAddress, cb);
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

    if (uAddress == pVirtio->uPciCfgDataOff)
    {
        /* VirtIO 1.0 spec section 4.1.4.7 describes a required alternative access capability
         * whereby the guest driver can specify a bar, offset, and length via the PCI configuration space
         * (the virtio_pci_cfg_cap capability), and access data items. */
        uint32_t uLength = pVirtio->pPciCfgCap->pciCap.uLength;
        uint32_t uOffset = pVirtio->pPciCfgCap->pciCap.uOffset;
        uint8_t  uBar    = pVirtio->pPciCfgCap->pciCap.uBar;
        if (uBar == pVirtio->uVirtioCapRegion)
            (void)virtioR3MmioWrite(pDevIns, NULL, (RTGCPHYS)((uint32_t)pVirtio->GCPhysPciCapBase + uOffset),
                                    (void *)&u32Value, uLength);
        else
        {
            LogFunc(("Guest wrote virtio_pci_cfg_cap.pci_cfg_data using unconfigured BAR. Ignoring"));
            return VINF_SUCCESS;
        }
        LogFunc(("Guest wrote  virtio_pci_cfg_cap.pci_cfg_data, bar=%d, offset=%x, length=%x, value=%d\n",
                uBar, uOffset, uLength, u32Value));
        return VINF_SUCCESS;
    }
    return pVirtio->pfnPciConfigWriteOld(pDevIns, pPciDev, uAddress, u32Value, cb);
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
 * Get VirtIO accepted host-side features
 *
 * @returns feature bits selected or 0 if selector out of range.
 *
 * @param   pState          Virtio state
 * @param   uSelect         Selects which 32-bit set of feature information to return
 */

__attribute__((unused))
static uint64_t virtioGetHostFeatures(PVIRTIOSTATE pVirtio)
{
    return pVirtio->uDriverFeatures;
}

/**
 *
 * Set VirtIO available host-side features
 *
 * @returns VBox status code
 *
 * @param   pState              Virtio state
 * @param   uDeviceFeatures    Feature bits (0-63) to set
 */

void virtioSetHostFeatures(PVIRTIOSTATE pVirtio, uint64_t uDeviceFeatures)
{
    pVirtio->uDeviceFeatures = VIRTIO_F_VERSION_1 | uDeviceFeatures;
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
                    const char *pcszNameFmt, uint32_t nQueues, uint32_t uVirtioCapRegion,
                    PFNVIRTIODEVCAPREAD devCapReadCallback, PFNVIRTIODEVCAPWRITE devCapWriteCallback,
                    uint16_t cbDevSpecificCap, bool fHaveDevSpecificCap,  uint32_t uNotifyOffMultiplier)
{
    pVirtio->nQueues = nQueues;
    pVirtio->uNotifyOffMultiplier = uNotifyOffMultiplier;

    /* Init handles and log related stuff. */
    RTStrPrintf(pVirtio->szInstance, sizeof(pVirtio->szInstance), pcszNameFmt, iInstance);

    pVirtio->pDevInsR3 = pDevIns;
    pVirtio->pDevInsR0 = PDMDEVINS_2_R0PTR(pDevIns);
    pVirtio->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);
    pVirtio->uDeviceStatus = 0;
    pVirtio->cbDevSpecificCap = cbDevSpecificCap;
    pVirtio->uVirtioCapRegion = uVirtioCapRegion;
    pVirtio->fHaveDevSpecificCap = fHaveDevSpecificCap;
    pVirtio->virtioCallbacks.pfnVirtioDevCapRead = devCapReadCallback;
    pVirtio->virtioCallbacks.pfnVirtioDevCapWrite = devCapWriteCallback;

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


    PDMDevHlpPCISetConfigCallbacks(pDevIns, &pVirtio->dev,
                virtioPciConfigRead,  &pVirtio->pfnPciConfigReadOld,
                virtioPciConfigWrite, &pVirtio->pfnPciConfigWriteOld);

    /** Construct & map PCI vendor-specific capabilities for virtio host negotiation with guest driver */

#if 0 && defined(VBOX_WITH_MSI_DEVICES)  /* T.B.D. */
    uint8_t fMsiSupport = true;
#else
    uint8_t fMsiSupport = false;
#endif

#define CFGADDR2IDX(addr) ((uint64_t)addr - (uint64_t)&pVirtio->dev.abConfig)

    /* The following capability mapped via VirtIO 1.0: struct virtio_pci_cfg_cap (VIRTIO_PCI_CFG_CAP_T)
     * as a mandatory but suboptimal alternative interface to host device capabilities, facilitating
     * access the memory of any BAR. If the guest uses it (the VirtIO driver on Linux doesn't),
     * Unlike Common, Notify, ISR and Device capabilities, it is accessed directly via PCI Config region.
     * therefore does not contribute to the capabilities region (BAR) the other capabilities use.
     */
    PVIRTIO_PCI_CAP_T pCfg;

    /* Common capability (VirtIO 1.0 spec) */
    pCfg = (PVIRTIO_PCI_CAP_T)&pVirtio->dev.abConfig[0x40];
    pCfg->uCfgType = VIRTIO_PCI_CAP_COMMON_CFG;
    pCfg->uCapVndr = VIRTIO_PCI_CAP_ID_VENDOR;
    pCfg->uCapLen  = sizeof(VIRTIO_PCI_CAP_T);
    pCfg->uCapNext = CFGADDR2IDX(pCfg) + pCfg->uCapLen;
    pCfg->uBar     = uVirtioCapRegion;
    pCfg->uOffset  = 0;
    pCfg->uLength  = sizeof(VIRTIO_PCI_COMMON_CFG_T);
    pVirtio->pCommonCfgCap = pCfg;

    /* Notify capability (VirtIO 1.0 spec) */
    pCfg = (PVIRTIO_PCI_CAP_T)&pVirtio->dev.abConfig[pCfg->uCapNext];
    pCfg->uCfgType = VIRTIO_PCI_CAP_NOTIFY_CFG;
    pCfg->uCapVndr = VIRTIO_PCI_CAP_ID_VENDOR;
    pCfg->uCapLen  = sizeof(VIRTIO_PCI_NOTIFY_CAP_T);
    pCfg->uCapNext = CFGADDR2IDX(pCfg) + pCfg->uCapLen;
    pCfg->uBar     = uVirtioCapRegion;
    pCfg->uOffset  = pVirtio->pCommonCfgCap->uOffset + pVirtio->pCommonCfgCap->uLength;
    pCfg->uLength  = 4; /* This needs to be calculated differently */
    pVirtio->pNotifyCap = (PVIRTIO_PCI_NOTIFY_CAP_T)pCfg;
    pVirtio->pNotifyCap->uNotifyOffMultiplier = uNotifyOffMultiplier;

    /* ISR capability (VirtIO 1.0 spec) */
    pCfg = (PVIRTIO_PCI_CAP_T)&pVirtio->dev.abConfig[pCfg->uCapNext];
    pCfg->uCfgType = VIRTIO_PCI_CAP_ISR_CFG;
    pCfg->uCapVndr = VIRTIO_PCI_CAP_ID_VENDOR;
    pCfg->uCapLen  = sizeof(VIRTIO_PCI_CAP_T);
    pCfg->uCapNext = CFGADDR2IDX(pCfg) + pCfg->uCapLen;
    pCfg->uBar     = uVirtioCapRegion;
    pCfg->uOffset  = pVirtio->pNotifyCap->pciCap.uOffset + pVirtio->pNotifyCap->pciCap.uLength;
    pCfg->uLength  = sizeof(uint32_t);
    pVirtio->pIsrCap = pCfg;

    /* PCI Cfg capability (VirtIO 1.0 spec) */
    pCfg = (PVIRTIO_PCI_CAP_T)&pVirtio->dev.abConfig[pCfg->uCapNext];
    pCfg->uCfgType = VIRTIO_PCI_CAP_PCI_CFG;
    pCfg->uCapVndr = VIRTIO_PCI_CAP_ID_VENDOR;
    pCfg->uCapLen  = sizeof(VIRTIO_PCI_CFG_CAP_T);
    pCfg->uCapNext = (fMsiSupport || pVirtio->fHaveDevSpecificCap) ? CFGADDR2IDX(pCfg) + pCfg->uCapLen : 0;
    pCfg->uBar     = uVirtioCapRegion;
    pCfg->uOffset  = pVirtio->pIsrCap->uOffset + pVirtio->pIsrCap->uLength;
    pCfg->uLength  = 4;  /* Initialize a non-zero 4-byte aligned so Linux virtio_pci module recognizes this cap */
    pVirtio->pPciCfgCap = (PVIRTIO_PCI_CFG_CAP_T)pCfg;

    if (pVirtio->fHaveDevSpecificCap)
    {
        /* Following capability mapped via VirtIO 1.0: struct virtio_pci_dev_cap (VIRTIODEVCAP)*/
        pCfg = (PVIRTIO_PCI_CAP_T)&pVirtio->dev.abConfig[pCfg->uCapNext];
        pCfg->uCfgType = VIRTIO_PCI_CAP_DEVICE_CFG;
        pCfg->uCapVndr = VIRTIO_PCI_CAP_ID_VENDOR;
        pCfg->uCapLen  = sizeof(VIRTIO_PCI_CAP_T);
        pCfg->uCapNext = fMsiSupport ? CFGADDR2IDX(pCfg) + pCfg->uCapLen : 0;
        pCfg->uBar     = uVirtioCapRegion;
        pCfg->uOffset  = pVirtio->pIsrCap->uOffset + pVirtio->pIsrCap->uLength;
        pCfg->uLength  = cbDevSpecificCap;
        pVirtio->pDeviceCap = pCfg;
    }

    /* Set offset to first capability and enable PCI dev capabilities */
    PCIDevSetCapabilityList    (&pVirtio->dev, 0x40);
    PCIDevSetStatus            (&pVirtio->dev, VBOX_PCI_STATUS_CAP_LIST);

    if (fMsiSupport)
    {
        PDMMSIREG aMsiReg;
        RT_ZERO(aMsiReg);
        aMsiReg.iMsixCapOffset  = pCfg->uCapNext;
        aMsiReg.iMsixNextOffset = 0;
        aMsiReg.iMsixBar        = 0;
        aMsiReg.cMsixVectors    = 1;
        rc = PDMDevHlpPCIRegisterMsi(pDevIns, &aMsiReg); /* see MsixR3init() */
        if (RT_FAILURE (rc))
            /* PK TODO: The following is moot, we need to flag no MSI-X support */
            PCIDevSetCapabilityList(&pVirtio->dev, 0x40);
    }

    rc = PDMDevHlpPCIIORegionRegister(pDevIns, uVirtioCapRegion, 4096,
                                      PCI_ADDRESS_SPACE_MEM, virtioR3Map);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
            N_("virtio: cannot register PCI Capabilities address space"));

    return rc;
}

#endif /* IN_RING3 */

#endif /* VBOX_DEVICE_STRUCT_TESTCASE */

