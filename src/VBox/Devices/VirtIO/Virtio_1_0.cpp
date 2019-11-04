/* $Id$ */
/** @file
 * Virtio_1_0 - Virtio Common (PCI, feature & config mgt, queue mgt & proxy, notification mgt)
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
#include <VBox/msi.h>
#include <iprt/param.h>
#include <iprt/assert.h>
#include <iprt/uuid.h>
#include <iprt/mem.h>
#include <iprt/assert.h>
#include <iprt/sg.h>
#include <VBox/vmm/pdmdev.h>
#include "Virtio_1_0.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#define INSTANCE(a_pVirtio)                 (a_pVirtio)->szInstance
#define QUEUENAME(a_pVirtio, a_idxQueue)    ((a_pVirtio)->virtqState[(a_idxQueue)].szVirtqName)

/**
 * This macro returns true if the implied parameter GCPhysAddr address and access length are
 * within the range of the mapped capability struct specified with the explicit parameters.
 *
 * @param[in]  a_GCPhysCapData  Pointer to MMIO mapped capability struct
 * @param[in]  a_pCfgCap        Pointer to capability in PCI configuration area
 * @param[out] a_fMatched       True if GCPhysAddr is within the physically mapped capability.
 *
 * Implied parameters:
 *     - GCPhysAddr     - [input, implied] Physical address accessed (via MMIO callback)
 *     - cb             - [input, implied] Number of bytes to access
 *
 * @todo r=bird: Make this a predicate macro (I will probably simplify this a
 *       lot later when 'GCPhysAddr' becomes an 'off').
 */
#define MATCH_VIRTIO_CAP_STRUCT(a_GCPhysCapData, a_pCfgCap, a_fMatched) \
        bool const a_fMatched = (a_GCPhysCapData) != 0 \
                             && (a_pCfgCap) != NULL \
                             && GCPhysAddr >= (RTGCPHYS)(a_GCPhysCapData) \
                             && GCPhysAddr < ((RTGCPHYS)(a_GCPhysCapData) + ((PVIRTIO_PCI_CAP_T)(a_pCfgCap))->uLength) \
                             && cb <= ((PVIRTIO_PCI_CAP_T)a_pCfgCap)->uLength

#define IS_DRIVER_OK(pVirtio) (pVirtio->uDeviceStatus & VIRTIO_STATUS_DRIVER_OK)


/** Marks the start of the virtio saved state (just for sanity). */
#define VIRTIO_SAVEDSTATE_MARKER                        UINT64_C(0x1133557799bbddff)
/** The current saved state version for the virtio core. */
#define VIRTIO_SAVEDSTATE_VERSION                       UINT32_C(1)




/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * virtq related structs
 * (struct names follow VirtIO 1.0 spec, typedef use VBox style)
 */
typedef struct virtq_desc
{
    uint64_t  GCPhysBuf;                                         /**< addr       GC Phys. address of buffer     */
    uint32_t  cb;                                                /**< len        Buffer length                  */
    uint16_t  fFlags;                                            /**< flags      Buffer specific flags          */
    uint16_t  uDescIdxNext;                                      /**< next       Idx set if VIRTIO_DESC_F_NEXT  */
} VIRTQ_DESC_T, *PVIRTQ_DESC_T;

typedef struct virtq_avail
{
    uint16_t  fFlags;                                            /**< flags      avail ring drv to dev flags    */
    uint16_t  uIdx;                                              /**< idx        Index of next free ring slot   */
    uint16_t  auRing[RT_FLEXIBLE_ARRAY];                         /**< ring       Ring: avail drv to dev bufs    */
    /* uint16_t  uUsedEventIdx;                                     - used_event (if VIRTQ_USED_F_EVENT_IDX)    */
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
    VIRTQ_USED_ELEM_T aRing[RT_FLEXIBLE_ARRAY];                  /**< ring        Ring: used dev to drv bufs    */
    /** @todo r=bird: From the usage, this member shouldn't be here and will only
     * confuse compilers . */
    /* uint16_t  uAvailEventIdx;                                    - avail_event if (VIRTQ_USED_F_EVENT_IDX)   */
} VIRTQ_USED_T, *PVIRTQ_USED_T;


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static void virtioNotifyGuestDriver(PVIRTIOSTATE pVirtio, uint16_t idxQueue, bool fForce);
static int  virtioKick(PVIRTIOSTATE pVirtio, uint8_t uCause, uint16_t uVec, bool fForce);

/** @name Internal queue operations
 * @{ */

#if 0 /* unused */
DECLINLINE(int) virtqIsEventNeeded(uint16_t uEventIdx, uint16_t uDescIdxNew, uint16_t uDescIdxOld)
{
    return (uint16_t)(uDescIdxNew - uEventIdx - 1) < (uint16_t)(uDescIdxNew - uDescIdxOld);
}
#endif

/**
 * Accessor for virtq descriptor
 */
DECLINLINE(void) virtioReadDesc(PVIRTIOSTATE pVirtio, uint16_t idxQueue, uint32_t uDescIdx, PVIRTQ_DESC_T pDesc)
{
    //Log(("%s virtioQueueReadDesc: ring=%p idx=%u\n", INSTANCE(pState), pVirtQ, idx));
    AssertMsg(pVirtio->uDeviceStatus & VIRTIO_STATUS_DRIVER_OK, ("Called with guest driver not ready\n"));
    PDMDevHlpPhysRead(pVirtio->CTX_SUFF(pDevIns),
                      pVirtio->aGCPhysQueueDesc[idxQueue] + sizeof(VIRTQ_DESC_T) * (uDescIdx % pVirtio->uQueueSize[idxQueue]),
                      pDesc, sizeof(VIRTQ_DESC_T));
}

/**
 * Accessors for virtq avail ring
 */
DECLINLINE(uint16_t) virtioReadAvailDescIdx(PVIRTIOSTATE pVirtio, uint16_t idxQueue, uint32_t availIdx)
{
    uint16_t uDescIdx;
    AssertMsg(pVirtio->uDeviceStatus & VIRTIO_STATUS_DRIVER_OK, ("Called with guest driver not ready\n"));
    PDMDevHlpPhysRead(pVirtio->CTX_SUFF(pDevIns),
                        pVirtio->aGCPhysQueueAvail[idxQueue]
                      + RT_UOFFSETOF_DYN(VIRTQ_AVAIL_T, auRing[availIdx % pVirtio->uQueueSize[idxQueue]]),
                      &uDescIdx, sizeof(uDescIdx));
    return uDescIdx;
}

DECLINLINE(uint16_t) virtioReadAvailRingIdx(PVIRTIOSTATE pVirtio, uint16_t idxQueue)
{
    uint16_t uIdx = 0;
    AssertMsg(pVirtio->uDeviceStatus & VIRTIO_STATUS_DRIVER_OK, ("Called with guest driver not ready\n"));
    PDMDevHlpPhysRead(pVirtio->CTX_SUFF(pDevIns),
                      pVirtio->aGCPhysQueueAvail[idxQueue] + RT_UOFFSETOF(VIRTQ_AVAIL_T, uIdx),
                      &uIdx, sizeof(uIdx));
    return uIdx;
}

DECLINLINE(bool) virtqIsEmpty(PVIRTIOSTATE pVirtio, uint16_t idxQueue)
{
    return virtioReadAvailRingIdx(pVirtio, idxQueue) == pVirtio->virtqState[idxQueue].uAvailIdx;
}

#if 0 /* unused */
DECLINLINE(uint16_t) virtioReadAvailFlags(PVIRTIOSTATE pVirtio, uint16_t idxQueue)
{
    uint16_t fFlags;
    AssertMsg(pVirtio->uDeviceStatus & VIRTIO_STATUS_DRIVER_OK, ("Called with guest driver not ready\n"));
    PDMDevHlpPhysRead(pVirtio->CTX_SUFF(pDevIns),
                      pVirtio->aGCPhysQueueAvail[idxQueue] + RT_UOFFSETOF(VIRTQ_AVAIL_T, fFlags),
                      &fFlags, sizeof(fFlags));
    return fFlags;
}
#endif

DECLINLINE(uint16_t) virtioReadAvailUsedEvent(PVIRTIOSTATE pVirtio, uint16_t idxQueue)
{
    uint16_t uUsedEventIdx;
    /** VirtIO 1.0 uUsedEventIdx (used_event) immediately follows ring */
    AssertMsg(pVirtio->uDeviceStatus & VIRTIO_STATUS_DRIVER_OK, ("Called with guest driver not ready\n"));
    PDMDevHlpPhysRead(pVirtio->CTX_SUFF(pDevIns),
                      pVirtio->aGCPhysQueueAvail[idxQueue] + RT_UOFFSETOF_DYN(VIRTQ_AVAIL_T, auRing[pVirtio->uQueueSize[idxQueue]]),
                      &uUsedEventIdx, sizeof(uUsedEventIdx));
    return uUsedEventIdx;
}
/** @} */

/** @name Accessors for virtq used ring
 * @{
 */
DECLINLINE(void) virtioWriteUsedElem(PVIRTIOSTATE pVirtio, uint16_t idxQueue, uint32_t usedIdx, uint32_t uDescIdx, uint32_t uLen)
{
    VIRTQ_USED_ELEM_T elem = { uDescIdx,  uLen };
    AssertMsg(pVirtio->uDeviceStatus & VIRTIO_STATUS_DRIVER_OK, ("Called with guest driver not ready\n"));
    PDMDevHlpPCIPhysWrite(pVirtio->CTX_SUFF(pDevIns),
                            pVirtio->aGCPhysQueueUsed[idxQueue]
                          + RT_UOFFSETOF_DYN(VIRTQ_USED_T, aRing[usedIdx % pVirtio->uQueueSize[idxQueue]]),
                          &elem, sizeof(elem));
}

DECLINLINE(void) virtioWriteUsedRingIdx(PVIRTIOSTATE pVirtio, uint16_t idxQueue, uint16_t uIdx)
{
    AssertMsg(pVirtio->uDeviceStatus & VIRTIO_STATUS_DRIVER_OK, ("Called with guest driver not ready\n"));
    PDMDevHlpPCIPhysWrite(pVirtio->CTX_SUFF(pDevIns),
                          pVirtio->aGCPhysQueueUsed[idxQueue] + RT_UOFFSETOF(VIRTQ_USED_T, uIdx),
                          &uIdx, sizeof(uIdx));
}

#ifdef LOG_ENABLED
DECLINLINE(uint16_t) virtioReadUsedRingIdx(PVIRTIOSTATE pVirtio, uint16_t idxQueue)
{
    uint16_t uIdx;
    AssertMsg(pVirtio->uDeviceStatus & VIRTIO_STATUS_DRIVER_OK, ("Called with guest driver not ready\n"));
    PDMDevHlpPhysRead(pVirtio->CTX_SUFF(pDevIns),
                      pVirtio->aGCPhysQueueUsed[idxQueue] + RT_UOFFSETOF(VIRTQ_USED_T, uIdx),
                      &uIdx, sizeof(uIdx));
    return uIdx;
}
#endif

DECLINLINE(uint16_t) virtioReadUsedFlags(PVIRTIOSTATE pVirtio, uint16_t idxQueue)
{
    uint16_t fFlags;
    AssertMsg(pVirtio->uDeviceStatus & VIRTIO_STATUS_DRIVER_OK, ("Called with guest driver not ready\n"));
    PDMDevHlpPhysRead(pVirtio->CTX_SUFF(pDevIns),
                      pVirtio->aGCPhysQueueUsed[idxQueue] + RT_UOFFSETOF(VIRTQ_USED_T, fFlags),
                      &fFlags, sizeof(fFlags));
    return fFlags;
}

#if 0 /* unused */
DECLINLINE(void) virtioWriteUsedFlags(PVIRTIOSTATE pVirtio, uint16_t idxQueue, uint32_t fFlags)
{
    AssertMsg(pVirtio->uDeviceStatus & VIRTIO_STATUS_DRIVER_OK, ("Called with guest driver not ready\n"));
    RT_UNTRUSTED_VALIDATED_FENCE(); /* VirtIO 1.0, Section 3.2.1.4.1 */
    PDMDevHlpPCIPhysWrite(pVirtio->CTX_SUFF(pDevIns),
                          pVirtio->aGCPhysQueueUsed[idxQueue] + RT_UOFFSETOF(VIRTQ_USED_T, fFlags),
                          &fFlags, sizeof(fFlags));
}
#endif

#if 0 /* unused */
DECLINLINE(uint16_t) virtioReadUsedAvailEvent(PVIRTIOSTATE pVirtio, uint16_t idxQueue)
{
    uint16_t uAvailEventIdx;
    RT_UNTRUSTED_VALIDATED_FENCE(); /* VirtIO 1.0, Section 3.2.1.4.1 */
    /** VirtIO 1.0 uAvailEventIdx (avail_event) immediately follows ring */
    AssertMsg(pVirtio->uDeviceStatus & VIRTIO_STATUS_DRIVER_OK, ("Called with guest driver not ready\n"));
    PDMDevHlpPhysRead(pVirtio->CTX_SUFF(pDevIns),
                      pVirtio->aGCPhysQueueUsed[idxQueue] + RT_UOFFSETOF_DYN(VIRTQ_USED_T, aRing[pVirtio->uQueueSize[idxQueue]]),
                      &uAvailEventIdx, sizeof(uAvailEventIdx));
    return uAvailEventIdx;
}
#endif

#if 0 /* unused */
DECLINLINE(void) virtioWriteUsedAvailEvent(PVIRTIOSTATE pVirtio, uint16_t idxQueue, uint32_t uAvailEventIdx)
{
    /** VirtIO 1.0 uAvailEventIdx (avail_event) immediately follows ring */
    AssertMsg(pVirtio->uDeviceStatus & VIRTIO_STATUS_DRIVER_OK, ("Called with guest driver not ready\n"));
    PDMDevHlpPCIPhysWrite(pVirtio->CTX_SUFF(pDevIns),
                          pVirtio->aGCPhysQueueUsed[idxQueue] + RT_UOFFSETOF_DYN(VIRTQ_USED_T, aRing[pVirtio->uQueueSize[idxQueue]]),
                          &uAvailEventIdx, sizeof(uAvailEventIdx));
}
#endif

/** @} */

#ifdef LOG_ENABLED

/**
 * Does a formatted hex dump using Log(()), recommend using VIRTIO_HEX_DUMP() macro to
 * control enabling of logging efficiently.
 *
 * @param   pv          pointer to buffer to dump contents of
 * @param   cb          count of characters to dump from buffer
 * @param   uBase       base address of per-row address prefixing of hex output
 * @param   pszTitle    Optional title. If present displays title that lists
 *                      provided text with value of cb to indicate size next to it.
 */
void virtioHexDump(uint8_t *pv, uint32_t cb, uint32_t uBase, const char *pszTitle)
{
    if (pszTitle)
        Log(("%s [%d bytes]:\n", pszTitle, cb));
    for (uint32_t row = 0; row < RT_MAX(1, (cb / 16) + 1) && row * 16 < cb; row++)
    {
        Log(("%04x: ", row * 16 + uBase)); /* line address */
        for (uint8_t col = 0; col < 16; col++)
        {
           uint32_t idx = row * 16 + col;
           if (idx >= cb)
               Log(("-- %s", (col + 1) % 8 ? "" : "  "));
           else
               Log(("%02x %s", pv[idx], (col + 1) % 8 ? "" : "  "));
        }
        for (uint32_t idx = row * 16; idx < row * 16 + 16; idx++)
           Log(("%c", (idx >= cb) ? ' ' : (pv[idx] >= 0x20 && pv[idx] <= 0x7e ? pv[idx] : '.')));
        Log(("\n"));
    }
    Log(("\n"));
    RT_NOREF2(uBase, pv);
}

/**
 * Log memory-mapped I/O input or output value.
 *
 * This is designed to be invoked by macros that can make contextual assumptions
 * (e.g. implicitly derive MACRO parameters from the invoking function). It is exposed
 * for the VirtIO client doing the device-specific implementation in order to log in a
 * similar fashion accesses to the device-specific MMIO configuration structure. Macros
 * that leverage this function are found in virtioCommonCfgAccessed() and can be
 * used as an example of how to use this effectively for the device-specific
 * code.
 *
 * @param   pszFunc     To avoid displaying this function's name via __FUNCTION__ or LogFunc()
 * @param   pszMember   Name of struct member
 * @param   pv          pointer to value
 * @param   cb          size of value
 * @param   uOffset     offset into member where value starts
 * @param   fWrite      True if write I/O
 * @param   fHasIndex   True if the member is indexed
 * @param   idx         The index if fHasIndex
 */
void virtioLogMappedIoValue(const char *pszFunc, const char *pszMember, uint32_t uMemberSize,
                            const void *pv, uint32_t cb, uint32_t uOffset, int fWrite,
                            int fHasIndex, uint32_t idx)
{

#define FMTHEX(fmtout, val, cNybbles) \
    fmtout[cNybbles] = '\0'; \
    for (uint8_t i = 0; i < cNybbles; i++) \
        fmtout[(cNybbles - i) - 1] = "0123456789abcdef"[(val >> (i * 4)) & 0xf];

#define MAX_STRING   64
    char pszIdx[MAX_STRING] = { 0 };
    char pszDepiction[MAX_STRING] = { 0 };
    char pszFormattedVal[MAX_STRING] = { 0 };
    if (fHasIndex)
        RTStrPrintf(pszIdx, sizeof(pszIdx), "[%d]", idx);
    if (cb == 1 || cb == 2 || cb == 4 || cb == 8)
    {
        /* manually padding with 0's instead of \b due to different impl of %x precision than printf() */
        uint64_t val = 0;
        memcpy((char *)&val, pv, cb);
        FMTHEX(pszFormattedVal, val, cb * 2);
        if (uOffset != 0 || cb != uMemberSize) /* display bounds if partial member access */
            RTStrPrintf(pszDepiction, sizeof(pszDepiction), "%s%s[%d:%d]",
                        pszMember, pszIdx, uOffset, uOffset + cb - 1);
        else
            RTStrPrintf(pszDepiction, sizeof(pszDepiction), "%s%s", pszMember, pszIdx);
        RTStrPrintf(pszDepiction, sizeof(pszDepiction), "%-30s", pszDepiction);
        uint32_t first = 0;
        for (uint8_t i = 0; i < sizeof(pszDepiction); i++)
            if (pszDepiction[i] == ' ' && first++)
                pszDepiction[i] = '.';
        Log6Func(("%s: Guest %s %s 0x%s\n",
                  pszFunc, fWrite ? "wrote" : "read ", pszDepiction, pszFormattedVal));
    }
    else /* odd number or oversized access, ... log inline hex-dump style */
    {
        Log6Func(("%s: Guest %s %s%s[%d:%d]: %.*Rhxs\n",
                  pszFunc, fWrite ? "wrote" : "read ", pszMember,
                  pszIdx, uOffset, uOffset + cb, cb, pv));
    }
    RT_NOREF2(fWrite, pszFunc);
}

#endif /* LOG_ENABLED */

/**
 * Makes the MMIO-mapped Virtio uDeviceStatus registers non-cryptic
 */
DECLINLINE(void) virtioLogDeviceStatus(uint8_t bStatus)
{
    if (bStatus == 0)
        Log6(("RESET"));
    else
    {
        int primed = 0;
        if (bStatus & VIRTIO_STATUS_ACKNOWLEDGE)
            Log6(("%sACKNOWLEDGE", primed++ ? "" : ""));
        if (bStatus & VIRTIO_STATUS_DRIVER)
            Log6(("%sDRIVER",      primed++ ? " | " : ""));
        if (bStatus & VIRTIO_STATUS_FEATURES_OK)
            Log6(("%sFEATURES_OK", primed++ ? " | " : ""));
        if (bStatus & VIRTIO_STATUS_DRIVER_OK)
            Log6(("%sDRIVER_OK",   primed++ ? " | " : ""));
        if (bStatus & VIRTIO_STATUS_FAILED)
            Log6(("%sFAILED",      primed++ ? " | " : ""));
        if (bStatus & VIRTIO_STATUS_DEVICE_NEEDS_RESET)
            Log6(("%sNEEDS_RESET", primed++ ? " | " : ""));
        (void)primed;
    }
}

#ifdef IN_RING3
/**
 * Allocate client context for client to work with VirtIO-provided with queue
 *
 * @param   pVirtio     Pointer to the virtio state.
 * @param   idxQueue    Queue number
 * @param   pcszName    Name to give queue
 *
 * @returns VBox status code.
 */
int virtioR3QueueAttach(PVIRTIOSTATE pVirtio, uint16_t idxQueue, const char *pcszName)
{
    LogFunc(("%s\n", pcszName));
    PVIRTQSTATE pVirtq = &pVirtio->virtqState[idxQueue];
    pVirtq->uAvailIdx = 0;
    pVirtq->uUsedIdx  = 0;
    pVirtq->fEventThresholdReached = false;
    RTStrCopy(pVirtq->szVirtqName, sizeof(pVirtq->szVirtqName), pcszName);
    return VINF_SUCCESS;
}
#endif /* IN_RING3 */

#if 0 /** @todo r=bird: no prototype or docs for this one  */
/**
 * See API comments in header file for description
 */
int virtioQueueSkip(PVIRTIOSTATE pVirtio, uint16_t idxQueue)
{
    Assert(idxQueue < RT_ELEMENTS(pVirtio->virtqState));
    PVIRTQSTATE pVirtq = &pVirtio->virtqState[idxQueue];

    AssertMsgReturn(IS_DRIVER_OK(pVirtio) && pVirtio->uQueueEnable[idxQueue],
                    ("Guest driver not in ready state.\n"), VERR_INVALID_STATE);

    if (virtioQueueIsEmpty(pVirtio, idxQueue))
        return VERR_NOT_AVAILABLE;

    Log2Func(("%s avail_idx=%u\n", pVirtq->szVirtqName, pVirtq->uAvailIdx));
    pVirtq->uAvailIdx++;

    return VINF_SUCCESS;
}
#endif

/**
 * Check if the associated queue is empty
 *
 * @param hVirtio       Handle for VirtIO framework
 * @param idxQueue          Queue number
 *
 * @retval true  Queue is empty or unavailable.
 * @retval false Queue is available and has entries
 */
bool virtioQueueIsEmpty(PVIRTIOSTATE pVirtio, uint16_t idxQueue)
{
    if (pVirtio->uDeviceStatus & VIRTIO_STATUS_DRIVER_OK)
        return virtqIsEmpty(pVirtio, idxQueue);
    return true;
}

#ifdef IN_RING3

/**
 * Removes descriptor chain from avail ring of indicated queue and converts the descriptor
 * chain into its OUT (to device) and IN to guest components.
 *
 * Additionally it converts the OUT desc chain data to a contiguous virtual
 * memory buffer for easy consumption by the caller. The caller must return the
 * descriptor chain pointer via virtioR3QueuePut() and then call virtioQueueSync()
 * at some point to return the data to the guest and complete the transaction.
 *
 * @param   pVirtio     Pointer to the virtio state.
 * @param   idxQueue    Queue number
 * @param   fRemove     flags whether to remove desc chain from queue (false = peek)
 * @param   ppDescChain Address to store pointer to descriptor chain that contains the
 *                      pre-processed transaction information pulled from the virtq.
 *
 * @returns VBox status code:
 * @retval  VINF_SUCCESS         Success
 * @retval  VERR_INVALID_STATE   VirtIO not in ready state (asserted).
 * @retval  VERR_NOT_AVAILABLE   If the queue is empty.
 */
int virtioR3QueueGet(PVIRTIOSTATE pVirtio, uint16_t idxQueue, PPVIRTIO_DESC_CHAIN_T ppDescChain,  bool fRemove)
{
    AssertReturn(ppDescChain, VERR_INVALID_PARAMETER);

    Assert(idxQueue < RT_ELEMENTS(pVirtio->virtqState));
    PVIRTQSTATE pVirtq  = &pVirtio->virtqState[idxQueue];

    PRTSGSEG paSegsIn = (PRTSGSEG)RTMemAlloc(VIRTQ_MAX_SIZE * sizeof(RTSGSEG));
    AssertReturn(paSegsIn, VERR_NO_MEMORY);

    PRTSGSEG paSegsOut = (PRTSGSEG)RTMemAlloc(VIRTQ_MAX_SIZE * sizeof(RTSGSEG));
    AssertReturn(paSegsOut, VERR_NO_MEMORY);

    AssertMsgReturn(IS_DRIVER_OK(pVirtio) && pVirtio->uQueueEnable[idxQueue],
                    ("Guest driver not in ready state.\n"), VERR_INVALID_STATE);

    if (virtqIsEmpty(pVirtio, idxQueue))
        return VERR_NOT_AVAILABLE;

    uint16_t uHeadIdx = virtioReadAvailDescIdx(pVirtio, idxQueue, pVirtq->uAvailIdx);
    uint16_t uDescIdx = uHeadIdx;

    Log3Func(("%s DESC CHAIN: (head) desc_idx=%u [avail_idx=%u]\n", pVirtq->szVirtqName, uHeadIdx, pVirtq->uAvailIdx));

    if (fRemove)
        pVirtq->uAvailIdx++;

    VIRTQ_DESC_T desc;

    uint32_t cbIn = 0, cbOut = 0, cSegsIn = 0, cSegsOut = 0;

    do
    {
        RTSGSEG *pSeg;

        /*
         * Malicious guests may go beyond paSegsIn or paSegsOut boundaries by linking
         * several descriptors into a loop. Since there is no legitimate way to get a sequences of
         * linked descriptors exceeding the total number of descriptors in the ring (see @bugref{8620}),
         * the following aborts I/O if breach and employs a simple log throttling algorithm to notify.
         */
        if (cSegsIn + cSegsOut >= VIRTQ_MAX_SIZE)
        {
            static volatile uint32_t s_cMessages  = 0;
            static volatile uint32_t s_cThreshold = 1;
            if (ASMAtomicIncU32(&s_cMessages) == ASMAtomicReadU32(&s_cThreshold))
            {
                LogRelMax(64, ("Too many linked descriptors; check if the guest arranges descriptors in a loop.\n"));
                if (ASMAtomicReadU32(&s_cMessages) != 1)
                    LogRelMax(64, ("(the above error has occured %u times so far)\n", ASMAtomicReadU32(&s_cMessages)));
                ASMAtomicWriteU32(&s_cThreshold, ASMAtomicReadU32(&s_cThreshold) * 10);
            }
            break;
        }
        RT_UNTRUSTED_VALIDATED_FENCE();

        virtioReadDesc(pVirtio, idxQueue, uDescIdx, &desc);

        if (desc.fFlags & VIRTQ_DESC_F_WRITE)
        {
            Log3Func(("%s IN  desc_idx=%u seg=%u addr=%RGp cb=%u\n", QUEUENAME(pVirtio, idxQueue), uDescIdx, cSegsIn, desc.GCPhysBuf, desc.cb));
            cbIn += desc.cb;
            pSeg = &(paSegsIn[cSegsIn++]);
        }
        else
        {
            Log3Func(("%s OUT desc_idx=%u seg=%u addr=%RGp cb=%u\n", QUEUENAME(pVirtio, idxQueue), uDescIdx, cSegsOut, desc.GCPhysBuf, desc.cb));
            cbOut += desc.cb;
            pSeg = &(paSegsOut[cSegsOut++]);
        }

        pSeg->pvSeg = (void *)desc.GCPhysBuf;
        pSeg->cbSeg = desc.cb;

        uDescIdx = desc.uDescIdxNext;
    } while (desc.fFlags & VIRTQ_DESC_F_NEXT);

    PRTSGBUF pSgPhysIn = (PRTSGBUF)RTMemAllocZ(sizeof(RTSGBUF));
    AssertReturn(pSgPhysIn, VERR_NO_MEMORY);

    RTSgBufInit(pSgPhysIn, (PCRTSGSEG)paSegsIn, cSegsIn);

    PRTSGBUF pSgPhysOut = (PRTSGBUF)RTMemAllocZ(sizeof(RTSGBUF));
    AssertReturn(pSgPhysOut, VERR_NO_MEMORY);

    RTSgBufInit(pSgPhysOut, (PCRTSGSEG)paSegsOut, cSegsOut);

    PVIRTIO_DESC_CHAIN_T pDescChain = (PVIRTIO_DESC_CHAIN_T)RTMemAllocZ(sizeof(VIRTIO_DESC_CHAIN_T));
    AssertReturn(pDescChain, VERR_NO_MEMORY);

    pDescChain->uHeadIdx   = uHeadIdx;
    pDescChain->cbPhysSend  = cbOut;
    pDescChain->pSgPhysSend = pSgPhysOut;
    pDescChain->cbPhysReturn  = cbIn;
    pDescChain->pSgPhysReturn = pSgPhysIn;
    *ppDescChain = pDescChain;

    Log3Func(("%s -- segs OUT: %u (%u bytes)   IN: %u (%u bytes) --\n", pVirtq->szVirtqName, cSegsOut, cbOut, cSegsIn, cbIn));

    return VINF_SUCCESS;
}

/**
 * Returns data to the guest to complete a transaction initiated by virtQueueGet().
 *
 * The caller passes in a pointer to a scatter-gather buffer of virtual memory segments
 * and a pointer to the descriptor chain context originally derived from the pulled
 * queue entry, and this function will write the virtual memory s/g buffer into the
 * guest's physical memory free the descriptor chain. The caller handles the freeing
 * (as needed) of the virtual memory buffer.
 *
 * @note This does a write-ahead to the used ring of the guest's queue. The data
 *       written won't be seen by the guest until the next call to virtioQueueSync()
 *
 *
 * @param   pVirtio         Pointer to the virtio state.
 * @param   idxQueue        Queue number
 *
 * @param   pSgVirtReturn   Points toscatter-gather buffer of virtual memory
 *                          segments the caller is returning to the guest.
 *
 * @param   pDescChain      This contains the context of the scatter-gather
 *                          buffer originally pulled from the queue.
 *
 * @param   fFence          If true, put up copy fence (memory barrier) after
 *                          copying to guest phys. mem.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS       Success
 * @retval  VERR_INVALID_STATE VirtIO not in ready state
 * @retval  VERR_NOT_AVAILABLE Queue is empty
 */
int virtioR3QueuePut(PVIRTIOSTATE pVirtio, uint16_t idxQueue, PRTSGBUF pSgVirtReturn,
                     PVIRTIO_DESC_CHAIN_T pDescChain, bool fFence)
{
    Assert(idxQueue < RT_ELEMENTS(pVirtio->virtqState));
    PVIRTQSTATE pVirtq = &pVirtio->virtqState[idxQueue];
    PRTSGBUF pSgPhysReturn = pDescChain->pSgPhysReturn;

    AssertMsgReturn(IS_DRIVER_OK(pVirtio) /*&& pVirtio->uQueueEnable[idxQueue]*/,
                    ("Guest driver not in ready state.\n"), VERR_INVALID_STATE);

    Log3Func(("Copying client data to %s, desc chain (head desc_idx %d)\n",
              QUEUENAME(pVirtio, idxQueue), virtioReadUsedRingIdx(pVirtio, idxQueue)));

    /*
     * Copy s/g buf (virtual memory) to guest phys mem (IN direction). This virtual memory
     * block will be small (fixed portion of response header + sense buffer area or
     * control commands or error return values)... The bulk of req data xfers to phys mem
     * is handled by client */

    size_t cbCopy = 0;
    size_t cbRemain = RTSgBufCalcTotalLength(pSgVirtReturn);
    RTSgBufReset(pSgPhysReturn); /* Reset ptr because req data may have already been written */
    while (cbRemain)
    {
        PCRTSGSEG paSeg = &pSgPhysReturn->paSegs[pSgPhysReturn->idxSeg];
        uint64_t dstSgStart = (uint64_t)paSeg->pvSeg;
        uint64_t dstSgLen   = (uint64_t)paSeg->cbSeg;
        uint64_t dstSgCur   = (uint64_t)pSgPhysReturn->pvSegCur;
        cbCopy = RT_MIN((uint64_t)pSgVirtReturn->cbSegLeft, dstSgLen - (dstSgCur - dstSgStart));
        PDMDevHlpPhysWrite(pVirtio->CTX_SUFF(pDevIns),
                           (RTGCPHYS)pSgPhysReturn->pvSegCur, pSgVirtReturn->pvSegCur, cbCopy);
        RTSgBufAdvance(pSgVirtReturn, cbCopy);
        RTSgBufAdvance(pSgPhysReturn, cbCopy);
        cbRemain -= cbCopy;
    }

    if (fFence)
        RT_UNTRUSTED_NONVOLATILE_COPY_FENCE(); /* needed? */

    /** If this write-ahead crosses threshold where the driver wants to get an event flag it */
    if (pVirtio->uDriverFeatures & VIRTIO_F_EVENT_IDX)
        if (pVirtq->uUsedIdx == virtioReadAvailUsedEvent(pVirtio, idxQueue))
            pVirtq->fEventThresholdReached = true;

    Assert(!(cbCopy & UINT64_C(0xffffffff00000000)));

    /*
     * Place used buffer's descriptor in used ring but don't update used ring's slot index.
     * That will be done with a subsequent client call to virtioQueueSync() */
    virtioWriteUsedElem(pVirtio, idxQueue, pVirtq->uUsedIdx++, pDescChain->uHeadIdx, (uint32_t)(cbCopy & UINT32_C(0xffffffff)));

    Log2Func((".... Copied %lu bytes to %lu byte buffer, residual=%lu\n",
              cbCopy, pDescChain->cbPhysReturn, pDescChain->cbPhysReturn - cbCopy));

    Log6Func(("Write ahead used_idx=%d, %s used_idx=%d\n",
              pVirtq->uUsedIdx, QUEUENAME(pVirtio, idxQueue), virtioReadUsedRingIdx(pVirtio, idxQueue)));

    RTMemFree((void *)pDescChain->pSgPhysSend->paSegs);
    RTMemFree(pDescChain->pSgPhysSend);
    RTMemFree((void *)pSgPhysReturn->paSegs);
    RTMemFree(pSgPhysReturn);
    RTMemFree(pDescChain);

    return VINF_SUCCESS;
}

#endif /* IN_RING3 */

/**
 * Updates the indicated virtq's "used ring" descriptor index to match the
 * current write-head index, thus exposing the data added to the used ring by all
 * virtioR3QueuePut() calls since the last sync. This should be called after one or
 * more virtQueuePut() calls to inform the guest driver there is data in the queue.
 * Explicit notifications (e.g. interrupt or MSI-X) will be sent to the guest,
 * depending on VirtIO features negotiated and conditions, otherwise the guest
 * will detect the update by polling. (see VirtIO 1.0
 * specification, Section 2.4 "Virtqueues").
 *
 * @param   pVirtio     Pointer to the virtio state.
 * @param   idxQueue    Queue number
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS       Success
 * @retval  VERR_INVALID_STATE VirtIO not in ready state
 */
int virtioQueueSync(PVIRTIOSTATE pVirtio, uint16_t idxQueue)
{
    Assert(idxQueue < RT_ELEMENTS(pVirtio->virtqState));
    PVIRTQSTATE pVirtq = &pVirtio->virtqState[idxQueue];

    AssertMsgReturn(IS_DRIVER_OK(pVirtio) && pVirtio->uQueueEnable[idxQueue],
                    ("Guest driver not in ready state.\n"), VERR_INVALID_STATE);

    Log6Func(("Updating %s used_idx from %u to %u\n",
              QUEUENAME(pVirtio, idxQueue), virtioReadUsedRingIdx(pVirtio, idxQueue), pVirtq->uUsedIdx));

    virtioWriteUsedRingIdx(pVirtio, idxQueue, pVirtq->uUsedIdx);
    virtioNotifyGuestDriver(pVirtio, idxQueue, false);

    return VINF_SUCCESS;
}

#ifdef IN_RING3
/**
 */
static void virtior3QueueNotified(PVIRTIOSTATE pVirtio, uint16_t idxQueue, uint16_t uNotifyIdx)
{
    /* See VirtIO 1.0, section 4.1.5.2 It implies that idxQueue and uNotifyIdx should match.
     * Disregarding this notification may cause throughput to stop, however there's no way to know
     * which was queue was intended for wake-up if the two parameters disagree. */

    AssertMsg(uNotifyIdx == idxQueue,
              ("Notification param disagreement. Guest kicked virtq %d's notify addr w/non-corresponding virtq idx %d\n",
               idxQueue, uNotifyIdx));

//    AssertMsgReturn(uNotifyIdx == idxQueue,
//                    ("Notification param disagreement. Guest kicked virtq %d's notify addr w/non-corresponding virtq idx %d\n",
//                     idxQueue, uNotifyIdx));
    RT_NOREF(uNotifyIdx);

    PVIRTQSTATE pVirtq = &pVirtio->virtqState[idxQueue];
    Log6Func(("%s\n", pVirtq->szVirtqName));
    RT_NOREF(pVirtq);

    /* Inform client */
    pVirtio->Callbacks.pfnQueueNotified(pVirtio, idxQueue);
}
#endif /* IN_RING3 */

/**
 * Trigger MSI-X or INT# interrupt to notify guest of data added to used ring of
 * the specified virtq, depending on the interrupt configuration of the device
 * and depending on negotiated and realtime constraints flagged by the guest driver.
 *
 * See VirtIO 1.0 specification (section 2.4.7).
 *
 * @param   pVirtio     Pointer to the virtio state.
 * @param   idxQueue    Queue to check for guest interrupt handling preference
 * @param   fForce      Overrides idxQueue, forcing notification regardless of driver's
 *                      notification preferences. This is a safeguard to prevent
 *                      stalls upon resuming the VM. VirtIO 1.0 specification Section 4.1.5.5
 *                      indicates spurious interrupts are harmless to guest driver's state,
 *                      as they only cause the guest driver to [re]scan queues for work to do.
 */
static void virtioNotifyGuestDriver(PVIRTIOSTATE pVirtio, uint16_t idxQueue, bool fForce)
{
    Assert(idxQueue < RT_ELEMENTS(pVirtio->virtqState));
    PVIRTQSTATE pVirtq = &pVirtio->virtqState[idxQueue];

    AssertMsgReturnVoid(IS_DRIVER_OK(pVirtio), ("Guest driver not in ready state.\n"));
    if (pVirtio->uDriverFeatures & VIRTIO_F_EVENT_IDX)
    {
        if (pVirtq->fEventThresholdReached)
        {
            virtioKick(pVirtio, VIRTIO_ISR_VIRTQ_INTERRUPT, pVirtio->uQueueMsixVector[idxQueue], fForce);
            pVirtq->fEventThresholdReached = false;
            return;
        }
        Log6Func(("...skipping interrupt: VIRTIO_F_EVENT_IDX set but threshold not reached\n"));
    }
    else
    {
        /** If guest driver hasn't suppressed interrupts, interrupt  */
        if (fForce || !(virtioReadUsedFlags(pVirtio, idxQueue) & VIRTQ_AVAIL_F_NO_INTERRUPT))
        {
            virtioKick(pVirtio, VIRTIO_ISR_VIRTQ_INTERRUPT, pVirtio->uQueueMsixVector[idxQueue], fForce);
            return;
        }
        Log6Func(("...skipping interrupt. Guest flagged VIRTQ_AVAIL_F_NO_INTERRUPT for queue\n"));
    }
}

/**
 * Raise interrupt or MSI-X
 *
 * @param   pVirtio         The device state structure.
 * @param   uCause          Interrupt cause bit mask to set in PCI ISR port.
 * @param   uVec            MSI-X vector, if enabled
 * @param   uForce          True of out-of-band
 */
static int virtioKick(PVIRTIOSTATE pVirtio, uint8_t uCause, uint16_t uMsixVector, bool fForce)
{
   if (fForce)
       Log6Func(("reason: resumed after suspend\n"));
   else
   if (uCause == VIRTIO_ISR_VIRTQ_INTERRUPT)
       Log6Func(("reason: buffer added to 'used' ring.\n"));
   else
   if (uCause == VIRTIO_ISR_DEVICE_CONFIG)
       Log6Func(("reason: device config change\n"));

    if (!pVirtio->fMsiSupport)
    {
        pVirtio->uISR |= uCause;
        PDMDevHlpPCISetIrq(pVirtio->CTX_SUFF(pDevIns), 0, PDM_IRQ_LEVEL_HIGH);
    }
    else if (uMsixVector != VIRTIO_MSI_NO_VECTOR)
    {
        Log6Func(("MSI-X enabled, calling PDMDevHlpPCISetIrq with vector: 0x%x\n", uMsixVector));
        PDMDevHlpPCISetIrq(pVirtio->CTX_SUFF(pDevIns), uMsixVector, 1);
    }
    return VINF_SUCCESS;
}

/**
 * Lower interrupt. (Called when guest reads ISR)
 *
 * @param   pVirtio      The device state structure.
 */
static void virtioLowerInterrupt(PVIRTIOSTATE pVirtio)
{
    PDMDevHlpPCISetIrq(pVirtio->CTX_SUFF(pDevIns), 0, PDM_IRQ_LEVEL_LOW);
}

static void virtioResetQueue(PVIRTIOSTATE pVirtio, uint16_t idxQueue)
{
    Assert(idxQueue < RT_ELEMENTS(pVirtio->virtqState));
    PVIRTQSTATE pVirtQ = &pVirtio->virtqState[idxQueue];
    pVirtQ->uAvailIdx = 0;
    pVirtQ->uUsedIdx  = 0;
    pVirtio->uQueueEnable[idxQueue] = false;
    pVirtio->uQueueSize[idxQueue] = VIRTQ_MAX_SIZE;
    pVirtio->uQueueNotifyOff[idxQueue] = idxQueue;

    pVirtio->uQueueMsixVector[idxQueue] = idxQueue + 2;
    if (!pVirtio->fMsiSupport) /* VirtIO 1.0, 4.1.4.3 and 4.1.5.1.2 */
        pVirtio->uQueueMsixVector[idxQueue] = VIRTIO_MSI_NO_VECTOR;
}

static void virtioResetDevice(PVIRTIOSTATE pVirtio)
{
    Log2Func(("\n"));
    pVirtio->uDeviceFeaturesSelect  = 0;
    pVirtio->uDriverFeaturesSelect  = 0;
    pVirtio->uConfigGeneration      = 0;
    pVirtio->uDeviceStatus          = 0;
    pVirtio->uISR                   = 0;

    virtioLowerInterrupt(pVirtio);

    if (!pVirtio->fMsiSupport)  /* VirtIO 1.0, 4.1.4.3 and 4.1.5.1.2 */
        pVirtio->uMsixConfig = VIRTIO_MSI_NO_VECTOR;

    pVirtio->uNumQueues = VIRTQ_MAX_CNT;
    for (uint16_t idxQueue = 0; idxQueue < pVirtio->uNumQueues; idxQueue++)
        virtioResetQueue(pVirtio, idxQueue);
}

#if 0 /** @todo r=bird: Probably not needed. */
/**
 * Enable or disable queue
 *
 * @param   pVirtio     Pointer to the virtio state.
 * @param   idxQueue    Queue number
 * @param   fEnabled    Flag indicating whether to enable queue or not
 */
void virtioQueueEnable(PVIRTIOSTATE pVirtio, uint16_t idxQueue, bool fEnabled)
{
    if (fEnabled)
        pVirtio->uQueueSize[idxQueue] = VIRTQ_MAX_SIZE;
    else
        pVirtio->uQueueSize[idxQueue] = 0;
}
#endif

#if 0 /** @todo r=bird: This isn't invoked by anyone. Why? */
/**
 * Initiate orderly reset procedure.
 * Invoked by client to reset the device and driver (see VirtIO 1.0 section 2.1.1/2.1.2)
 */
void virtioResetAll(PVIRTIOSTATE pVirtio)
{
    LogFunc(("VIRTIO RESET REQUESTED!!!\n"));
    pVirtio->uDeviceStatus |= VIRTIO_STATUS_DEVICE_NEEDS_RESET;
    if (pVirtio->uDeviceStatus & VIRTIO_STATUS_DRIVER_OK)
    {
        pVirtio->fGenUpdatePending = true;
        virtioKick(pVirtio, VIRTIO_ISR_DEVICE_CONFIG, pVirtio->uMsixConfig, false /* fForce */);
    }
}
#endif

#ifdef IN_RING3
/**
 * Invoked by this implementation when guest driver resets the device.
 * The driver itself will not  until the device has read the status change.
 */
static void virtioGuestR3Resetted(PVIRTIOSTATE pVirtio)
{
    LogFunc(("Guest reset the device\n"));

    /* Let the client know */
    pVirtio->Callbacks.pfnStatusChanged(pVirtio, 0);
    virtioResetDevice(pVirtio);
}
#endif /* IN_RING3 */

/**
 * Handle accesses to Common Configuration capability
 *
 * @returns VBox status code
 *
 * @param   pVirtio     Virtio instance state
 * @param   fWrite      Set if write access, clear if read access.
 * @param   offCfg      The common configuration capability offset.
 * @param   cb          Number of bytes to read or write
 * @param   pv          Pointer to location to write to or read from
 */
static int virtioCommonCfgAccessed(PVIRTIOSTATE pVirtio, int fWrite, off_t offCfg, unsigned cb, void *pv)
{
/**
 * This macro resolves to boolean true if the implied parameters, offCfg and cb,
 * match the field offset and size of a field in the Common Cfg struct, (or if
 * it is a 64-bit field, if it accesses either 32-bit part as a 32-bit access)
 * This is mandated by section 4.1.3.1 of the VirtIO 1.0 specification)
 *
 * @param   member  Member of VIRTIO_PCI_COMMON_CFG_T
 * @param   offCfg  Implied parameter: Offset into VIRTIO_PCI_COMMON_CFG_T
 * @param   cb      Implied parameter: Number of bytes to access
 * @result true or false
 */
#define MATCH_COMMON_CFG(member) \
    (   (   RT_SIZEOFMEMB(VIRTIO_PCI_COMMON_CFG_T, member) == 8 \
         && (   offCfg == RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, member) \
             || offCfg == RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, member) + sizeof(uint32_t)) \
         && cb == sizeof(uint32_t)) \
     || (   offCfg == RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, member) \
         && cb == RT_SIZEOFMEMB(VIRTIO_PCI_COMMON_CFG_T, member)) )

#ifdef LOG_ENABLED
# define LOG_COMMON_CFG_ACCESS(member, a_offIntra) \
    virtioLogMappedIoValue(__FUNCTION__, #member, RT_SIZEOFMEMB(VIRTIO_PCI_COMMON_CFG_T, member), \
                           pv, cb, a_offIntra, fWrite, false, 0);
# define LOG_COMMON_CFG_ACCESS_INDEXED(member, idx, a_offIntra) \
    virtioLogMappedIoValue(__FUNCTION__, #member, RT_SIZEOFMEMB(VIRTIO_PCI_COMMON_CFG_T, member), \
                           pv, cb, a_offIntra, fWrite, true, idx);
#else
# define LOG_COMMON_CFG_ACCESS(member, a_offIntra)              do { } while (0)
# define LOG_COMMON_CFG_ACCESS_INDEXED(member, idx, a_offIntra) do { } while (0)
#endif

#define COMMON_CFG_ACCESSOR(member) \
    do \
    { \
        uint32_t offIntra = offCfg - RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, member); \
        if (fWrite) \
            memcpy((char *)&pVirtio->member + offIntra, (const char *)pv, cb); \
        else \
            memcpy(pv, (const char *)&pVirtio->member + offIntra, cb); \
        LOG_COMMON_CFG_ACCESS(member, offIntra); \
    } while(0)

#define COMMON_CFG_ACCESSOR_INDEXED(member, idx) \
    do \
    { \
        uint32_t offIntra = offCfg - RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, member); \
        if (fWrite) \
            memcpy(((char *)(pVirtio->member + idx)) + offIntra, (const char *)pv, cb); \
        else \
            memcpy((char *)pv, (const char *)(((char *)(pVirtio->member + idx)) + offIntra), cb); \
        LOG_COMMON_CFG_ACCESS_INDEXED(member, idx, offIntra); \
    } while(0)

#define COMMON_CFG_ACCESSOR_READONLY(member) \
    do \
    { \
        uint32_t offIntra = offCfg - RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, member); \
        if (fWrite) \
            LogFunc(("Guest attempted to write readonly virtio_pci_common_cfg.%s\n", #member)); \
        else \
        { \
            memcpy((char *)pv, (const char *)(((char *)&pVirtio->member) + offIntra), cb); \
            LOG_COMMON_CFG_ACCESS(member, offIntra); \
        } \
    } while(0)

#define COMMON_CFG_ACCESSOR_INDEXED_READONLY(member, idx) \
    do \
    { \
        uint32_t offIntra = offCfg - RT_OFFSETOF(VIRTIO_PCI_COMMON_CFG_T, member); \
        if (fWrite) \
            LogFunc(("Guest attempted to write readonly virtio_pci_common_cfg.%s[%d]\n", #member, idx)); \
        else \
        { \
            memcpy((char *)pv, ((char *)(pVirtio->member + idx)) + offIntra, cb); \
            LOG_COMMON_CFG_ACCESS_INDEXED(member, idx, offIntra); \
        } \
    } while(0)


    int rc = VINF_SUCCESS;
    uint64_t val;
    if (MATCH_COMMON_CFG(uDeviceFeatures))
    {
        if (fWrite) /* Guest WRITE pCommonCfg>uDeviceFeatures */
        {
            LogFunc(("Guest attempted to write readonly virtio_pci_common_cfg.device_feature\n"));
            return VINF_SUCCESS;
        }
        else /* Guest READ pCommonCfg->uDeviceFeatures */
        {
            switch (pVirtio->uDeviceFeaturesSelect)
            {
                case 0:
                    val = pVirtio->uDeviceFeatures & UINT32_C(0xffffffff);
                    memcpy(pv, &val, cb);
                    LOG_COMMON_CFG_ACCESS(uDeviceFeatures, offCfg - RT_UOFFSETOF(VIRTIO_PCI_COMMON_CFG_T, uDeviceFeatures));
                    break;
                case 1:
                    val = pVirtio->uDeviceFeatures >> 32;
                    memcpy(pv, &val, cb);
                    LOG_COMMON_CFG_ACCESS(uDeviceFeatures, offCfg - RT_UOFFSETOF(VIRTIO_PCI_COMMON_CFG_T, uDeviceFeatures) + 4);
                    break;
                default:
                    LogFunc(("Guest read uDeviceFeatures with out of range selector (%#x), returning 0\n",
                             pVirtio->uDeviceFeaturesSelect));
                    return VINF_IOM_MMIO_UNUSED_00;
            }
        }
    }
    else if (MATCH_COMMON_CFG(uDriverFeatures))
    {
        if (fWrite) /* Guest WRITE pCommonCfg->udriverFeatures */
        {
            switch (pVirtio->uDriverFeaturesSelect)
            {
                case 0:
                    memcpy(&pVirtio->uDriverFeatures, pv, cb);
                    LOG_COMMON_CFG_ACCESS(uDriverFeatures, offCfg - RT_UOFFSETOF(VIRTIO_PCI_COMMON_CFG_T, uDriverFeatures));
                    break;
                case 1:
                    memcpy((char *)&pVirtio->uDriverFeatures + sizeof(uint32_t), pv, cb);
                    LOG_COMMON_CFG_ACCESS(uDriverFeatures, offCfg - RT_UOFFSETOF(VIRTIO_PCI_COMMON_CFG_T, uDriverFeatures) + 4);
                    break;
                default:
                    LogFunc(("Guest wrote uDriverFeatures with out of range selector (%#x), returning 0\n",
                             pVirtio->uDriverFeaturesSelect));
                    return VINF_SUCCESS;
            }
        }
        else /* Guest READ pCommonCfg->udriverFeatures */
        {
            switch (pVirtio->uDriverFeaturesSelect)
            {
                case 0:
                    val = pVirtio->uDriverFeatures & 0xffffffff;
                    memcpy(pv, &val, cb);
                    LOG_COMMON_CFG_ACCESS(uDriverFeatures, offCfg - RT_UOFFSETOF(VIRTIO_PCI_COMMON_CFG_T, uDriverFeatures));
                    break;
                case 1:
                    val = (pVirtio->uDriverFeatures >> 32) & 0xffffffff;
                    memcpy(pv, &val, cb);
                    LOG_COMMON_CFG_ACCESS(uDriverFeatures, offCfg - RT_UOFFSETOF(VIRTIO_PCI_COMMON_CFG_T, uDriverFeatures) + 4);
                    break;
                default:
                    LogFunc(("Guest read uDriverFeatures with out of range selector (%#x), returning 0\n",
                             pVirtio->uDriverFeaturesSelect));
                    return VINF_IOM_MMIO_UNUSED_00;
            }
        }
    }
    else if (MATCH_COMMON_CFG(uNumQueues))
    {
        if (fWrite)
        {
            Log2Func(("Guest attempted to write readonly virtio_pci_common_cfg.num_queues\n"));
            return VINF_SUCCESS;
        }
        else
        {
            *(uint16_t *)pv = VIRTQ_MAX_CNT;
            LOG_COMMON_CFG_ACCESS(uNumQueues, 0);
        }
    }
    else if (MATCH_COMMON_CFG(uDeviceStatus))
    {
        if (fWrite) /* Guest WRITE pCommonCfg->uDeviceStatus */
        {
            pVirtio->uDeviceStatus = *(uint8_t *)pv;
            Log6Func(("Guest wrote uDeviceStatus ................ ("));
            virtioLogDeviceStatus(pVirtio->uDeviceStatus);
            Log6((")\n"));
            if (pVirtio->uDeviceStatus == 0)
                virtioGuestR3Resetted(pVirtio);
            /*
             * Notify client only if status actually changed from last time.
             */
            uint32_t const fOkayNow = pVirtio->uDeviceStatus     & VIRTIO_STATUS_DRIVER_OK;
            uint32_t const fWasOkay = pVirtio->uPrevDeviceStatus & VIRTIO_STATUS_DRIVER_OK;
            if (fOkayNow != fWasOkay)
                pVirtio->Callbacks.pfnStatusChanged(pVirtio, fOkayNow);
            pVirtio->uPrevDeviceStatus = pVirtio->uDeviceStatus;
        }
        else /* Guest READ pCommonCfg->uDeviceStatus */
        {
            Log6Func(("Guest read  uDeviceStatus ................ ("));
            *(uint32_t *)pv = pVirtio->uDeviceStatus;   /** @todo r=bird: Why 32-bit write here, the field is 8-bit? */
            virtioLogDeviceStatus(pVirtio->uDeviceStatus);
            Log6((")\n"));
        }
    }
    else
    if (MATCH_COMMON_CFG(uMsixConfig))
        COMMON_CFG_ACCESSOR(uMsixConfig);
    else
    if (MATCH_COMMON_CFG(uDeviceFeaturesSelect))
        COMMON_CFG_ACCESSOR(uDeviceFeaturesSelect);
    else
    if (MATCH_COMMON_CFG(uDriverFeaturesSelect))
        COMMON_CFG_ACCESSOR(uDriverFeaturesSelect);
    else
    if (MATCH_COMMON_CFG(uConfigGeneration))
        COMMON_CFG_ACCESSOR_READONLY(uConfigGeneration);
    else
    if (MATCH_COMMON_CFG(uQueueSelect))
        COMMON_CFG_ACCESSOR(uQueueSelect);
    else
    if (MATCH_COMMON_CFG(uQueueSize))
        COMMON_CFG_ACCESSOR_INDEXED(uQueueSize, pVirtio->uQueueSelect);
    else
    if (MATCH_COMMON_CFG(uQueueMsixVector))
        COMMON_CFG_ACCESSOR_INDEXED(uQueueMsixVector, pVirtio->uQueueSelect);
    else
    if (MATCH_COMMON_CFG(uQueueEnable))
        COMMON_CFG_ACCESSOR_INDEXED(uQueueEnable, pVirtio->uQueueSelect);
    else
    if (MATCH_COMMON_CFG(uQueueNotifyOff))
        COMMON_CFG_ACCESSOR_INDEXED_READONLY(uQueueNotifyOff, pVirtio->uQueueSelect);
    else
    if (MATCH_COMMON_CFG(aGCPhysQueueDesc))
        COMMON_CFG_ACCESSOR_INDEXED(aGCPhysQueueDesc, pVirtio->uQueueSelect);
    else
    if (MATCH_COMMON_CFG(aGCPhysQueueAvail))
        COMMON_CFG_ACCESSOR_INDEXED(aGCPhysQueueAvail, pVirtio->uQueueSelect);
    else
    if (MATCH_COMMON_CFG(aGCPhysQueueUsed))
        COMMON_CFG_ACCESSOR_INDEXED(aGCPhysQueueUsed, pVirtio->uQueueSelect);
    else
    {
        Log2Func(("Bad guest %s access to virtio_pci_common_cfg: offCfg=%#x (%d), cb=%d\n",
                  fWrite ? "write" : "read ", offCfg, offCfg, cb));
        return fWrite ? VINF_SUCCESS : VINF_IOM_MMIO_UNUSED_00;
    }

#undef COMMON_CFG_ACCESSOR_READONLY
#undef COMMON_CFG_ACCESSOR_INDEXED_READONLY
#undef COMMON_CFG_ACCESSOR_INDEXED
#undef COMMON_CFG_ACCESSOR
#undef LOG_COMMON_CFG_ACCESS_INDEXED
#undef LOG_COMMON_CFG_ACCESS
#undef MATCH_COMMON_CFG
    return rc;
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
    PVIRTIOSTATE pVirtio = PDMINS_2_DATA(pDevIns, PVIRTIOSTATE);
    Assert(pVirtio == (PVIRTIOSTATE)pvUser); RT_NOREF(pvUser);
    int rc = VINF_SUCCESS;

    MATCH_VIRTIO_CAP_STRUCT(pVirtio->GCPhysDeviceCap, pVirtio->pDeviceCap,     fDevSpecific);
    MATCH_VIRTIO_CAP_STRUCT(pVirtio->GCPhysCommonCfg, pVirtio->pCommonCfgCap,  fCommonCfg);
    MATCH_VIRTIO_CAP_STRUCT(pVirtio->GCPhysIsrCap,    pVirtio->pIsrCap,        fIsr);

    if (fDevSpecific)
    {
        uint32_t uOffset = GCPhysAddr - pVirtio->GCPhysDeviceCap;
        /*
         * Callback to client to manage device-specific configuration.
         */
        rc = pVirtio->Callbacks.pfnDevCapRead(pDevIns, uOffset, pv, cb);

        /*
         * Additionally, anytime any part of the device-specific configuration (which our client maintains)
         * is READ it needs to be checked to see if it changed since the last time any part was read, in
         * order to maintain the config generation (see VirtIO 1.0 spec, section 4.1.4.3.1)
         */
        bool fDevSpecificFieldChanged = !!memcmp((char *)pVirtio->pvDevSpecificCfg + uOffset,
                                                 (char *)pVirtio->pvPrevDevSpecificCfg + uOffset, cb);

        memcpy(pVirtio->pvPrevDevSpecificCfg, pVirtio->pvDevSpecificCfg, pVirtio->cbDevSpecificCfg);

        if (pVirtio->fGenUpdatePending || fDevSpecificFieldChanged)
        {
            ++pVirtio->uConfigGeneration;
            Log6Func(("Bumped cfg. generation to %d because %s%s\n",
                      pVirtio->uConfigGeneration,
                      fDevSpecificFieldChanged ? "<dev cfg changed> " : "",
                      pVirtio->fGenUpdatePending ? "<update was pending>" : ""));
            pVirtio->fGenUpdatePending = false;
        }
    }
    else
    if (fCommonCfg)
    {
        uint32_t uOffset = GCPhysAddr - pVirtio->GCPhysCommonCfg;
        rc = virtioCommonCfgAccessed(pVirtio, false /* fWrite */, uOffset, cb, pv);
    }
    else
    if (fIsr && cb == sizeof(uint8_t))
    {
        *(uint8_t *)pv = pVirtio->uISR;
        Log6Func(("Read and clear ISR\n"));
        pVirtio->uISR = 0; /* VirtIO specification requires reads of ISR to clear it */
        virtioLowerInterrupt(pVirtio);
    }
    else
    {
       LogFunc(("Bad read access to mapped capabilities region:\n"
                "                  pVirtio=%#p GCPhysAddr=%RGp cb=%u\n",
                pVirtio, GCPhysAddr, cb));
        return VINF_IOM_MMIO_UNUSED_00;
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
    PVIRTIOSTATE pVirtio = PDMINS_2_DATA(pDevIns, PVIRTIOSTATE);
    Assert(pVirtio == (PVIRTIOSTATE)pvUser); RT_NOREF(pvUser);

    MATCH_VIRTIO_CAP_STRUCT(pVirtio->GCPhysDeviceCap, pVirtio->pDeviceCap,     fDevSpecific);
    MATCH_VIRTIO_CAP_STRUCT(pVirtio->GCPhysCommonCfg, pVirtio->pCommonCfgCap,  fCommonCfg);
    MATCH_VIRTIO_CAP_STRUCT(pVirtio->GCPhysIsrCap,    pVirtio->pIsrCap,        fIsr);
    MATCH_VIRTIO_CAP_STRUCT(pVirtio->GCPhysNotifyCap, pVirtio->pNotifyCap,     fNotify);

    if (fDevSpecific)
    {
        uint32_t uOffset = GCPhysAddr - pVirtio->GCPhysDeviceCap;
        /*
         * Pass this MMIO write access back to the client to handle
         */
        (void)pVirtio->Callbacks.pfnDevCapWrite(pDevIns, uOffset, pv, cb);
    }
    else
    if (fCommonCfg)
    {
        uint32_t uOffset = GCPhysAddr - pVirtio->GCPhysCommonCfg;
        (void)virtioCommonCfgAccessed(pVirtio, true /* fWrite */, uOffset, cb, (void *)pv);
    }
    else
    if (fIsr && cb == sizeof(uint8_t))
    {
        pVirtio->uISR = *(uint8_t *)pv;
        Log6Func(("Setting uISR = 0x%02x (virtq interrupt: %d, dev confg interrupt: %d)\n",
                  pVirtio->uISR & 0xff,
                  pVirtio->uISR & VIRTIO_ISR_VIRTQ_INTERRUPT,
                  RT_BOOL(pVirtio->uISR & VIRTIO_ISR_DEVICE_CONFIG)));
    }
    else
    /* This *should* be guest driver dropping index of a new descriptor in avail ring */
    if (fNotify && cb == sizeof(uint16_t))
    {
        uint32_t uNotifyBaseOffset = GCPhysAddr - pVirtio->GCPhysNotifyCap;
        uint16_t idxQueue = uNotifyBaseOffset / VIRTIO_NOTIFY_OFFSET_MULTIPLIER;
        uint16_t uAvailDescIdx = *(uint16_t *)pv;

        virtior3QueueNotified(pVirtio, idxQueue, uAvailDescIdx);
    }
    else
    {
       Log2Func(("Bad write access to mapped capabilities region:\n"
                "                  pVirtio=%#p GCPhysAddr=%RGp pv=%#p{%.*Rhxs} cb=%u\n",
                pVirtio, GCPhysAddr, pv, cb, pv, cb));
    }
    return VINF_SUCCESS;
}

#ifdef IN_RING3

/**
 * @callback_method_impl{FNPCIIOREGIONMAP}
 */
static DECLCALLBACK(int) virtioR3Map(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev, uint32_t iRegion,
                                     RTGCPHYS GCPhysAddress, RTGCPHYS cb, PCIADDRESSSPACE enmType)
{
    PVIRTIOSTATE pVirtio = PDMINS_2_DATA(pDevIns, PVIRTIOSTATE);
    int          rc      = VINF_SUCCESS;

    Assert(pPciDev == pDevIns->apPciDevs[0]);
    Assert(cb >= 32);
    RT_NOREF3(pPciDev, iRegion, enmType);

    if (iRegion == VIRTIO_REGION_PCI_CAP)
    {
        /* We use the assigned size here, because we currently only support page aligned MMIO ranges. */
        rc = PDMDevHlpMMIORegister(pDevIns, GCPhysAddress, cb, pVirtio,
                                   IOMMMIO_FLAGS_READ_PASSTHRU | IOMMMIO_FLAGS_WRITE_PASSTHRU,
                                   virtioR3MmioWrite, virtioR3MmioRead,
                                   "virtio-scsi MMIO");

        if (RT_FAILURE(rc))
        {
            Log2Func(("virtio: PCI Capabilities failed to map GCPhysAddr=%RGp cb=%RGp, region=%d\n", GCPhysAddress, cb, iRegion));
            return rc;
        }
        Log2Func(("virtio: PCI Capabilities mapped at GCPhysAddr=%RGp cb=%RGp, region=%d\n", GCPhysAddress, cb, iRegion));
        pVirtio->GCPhysPciCapBase = GCPhysAddress;
        pVirtio->GCPhysCommonCfg  = GCPhysAddress + pVirtio->pCommonCfgCap->uOffset;
        pVirtio->GCPhysNotifyCap  = GCPhysAddress + pVirtio->pNotifyCap->pciCap.uOffset;
        pVirtio->GCPhysIsrCap     = GCPhysAddress + pVirtio->pIsrCap->uOffset;
        if (pVirtio->pvPrevDevSpecificCfg)
            pVirtio->GCPhysDeviceCap = GCPhysAddress + pVirtio->pDeviceCap->uOffset;
    }
    return rc;
}

/**
 * @callback_method_impl{FNPCICONFIGREAD}
 */
static DECLCALLBACK(VBOXSTRICTRC) virtioR3PciConfigRead(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev,
                                                        uint32_t uAddress, unsigned cb, uint32_t *pu32Value)
{
    PVIRTIOSTATE pVirtio = PDMINS_2_DATA(pDevIns, PVIRTIOSTATE);
    RT_NOREF(pPciDev);

    LogFlowFunc(("pDevIns=%p pPciDev=%p uAddress=%#x cb=%u pu32Value=%p\n",
                 pDevIns, pPciDev, uAddress, cb, pu32Value));
    if (uAddress == pVirtio->uPciCfgDataOff)
    {
        /*
         * VirtIO 1.0 spec section 4.1.4.7 describes a required alternative access capability
         * whereby the guest driver can specify a bar, offset, and length via the PCI configuration space
         * (the virtio_pci_cfg_cap capability), and access data items.
         */
        uint32_t uLength = pVirtio->pPciCfgCap->pciCap.uLength;
        uint32_t uOffset = pVirtio->pPciCfgCap->pciCap.uOffset;
        uint8_t  uBar    = pVirtio->pPciCfgCap->pciCap.uBar;

        if (   (uLength != 1 && uLength != 2 && uLength != 4)
            || cb != uLength
            || uBar != VIRTIO_REGION_PCI_CAP)
        {
            Log2Func(("Guest read virtio_pci_cfg_cap.pci_cfg_data using mismatching config. Ignoring\n"));
            *pu32Value = UINT32_MAX;
            return VINF_SUCCESS;
        }

        int rc = virtioR3MmioRead(pDevIns, pVirtio, pVirtio->GCPhysPciCapBase + uOffset, pu32Value, cb);
        Log2Func(("virtio: Guest read  virtio_pci_cfg_cap.pci_cfg_data, bar=%d, offset=%d, length=%d, result=%d -> %Rrc\n",
                  uBar, uOffset, uLength, *pu32Value, rc));
        return rc;
    }
    return VINF_PDM_PCI_DO_DEFAULT;
}

/**
 * @callback_method_impl{FNPCICONFIGWRITE}
 */
static DECLCALLBACK(VBOXSTRICTRC) virtioR3PciConfigWrite(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev,
                                                         uint32_t uAddress, unsigned cb, uint32_t u32Value)
{
    PVIRTIOSTATE pVirtio = PDMINS_2_DATA(pDevIns, PVIRTIOSTATE);
    RT_NOREF(pPciDev);

    LogFlowFunc(("pDevIns=%p pPciDev=%p uAddress=%#x cb=%u u32Value=%#x\n", pDevIns, pPciDev, uAddress, cb, u32Value));
    if (uAddress == pVirtio->uPciCfgDataOff)
    {
        /* VirtIO 1.0 spec section 4.1.4.7 describes a required alternative access capability
         * whereby the guest driver can specify a bar, offset, and length via the PCI configuration space
         * (the virtio_pci_cfg_cap capability), and access data items. */

        uint32_t uLength = pVirtio->pPciCfgCap->pciCap.uLength;
        uint32_t uOffset = pVirtio->pPciCfgCap->pciCap.uOffset;
        uint8_t  uBar    = pVirtio->pPciCfgCap->pciCap.uBar;

        if (   (uLength != 1 && uLength != 2 && uLength != 4)
            || cb != uLength
            || uBar != VIRTIO_REGION_PCI_CAP)
        {
            Log2Func(("Guest write virtio_pci_cfg_cap.pci_cfg_data using mismatching config. Ignoring\n"));
            return VINF_SUCCESS;
        }

        int rc = virtioR3MmioWrite(pDevIns, pVirtio, pVirtio->GCPhysPciCapBase + uOffset, &u32Value, cb);
        Log2Func(("Guest wrote  virtio_pci_cfg_cap.pci_cfg_data, bar=%d, offset=%x, length=%x, value=%d -> %Rrc\n",
                uBar, uOffset, uLength, u32Value, rc));
        return rc;
    }
    return VINF_PDM_PCI_DO_DEFAULT;
}


/*********************************************************************************************************************************
*   Saved state.                                                                                                                 *
*********************************************************************************************************************************/

/**
 * Called from the FNSSMDEVSAVEEXEC function of the device.
 *
 * @param   pVirtio     Pointer to the virtio state.
 * @param   pHlp        The ring-3 device helpers.
 * @param   pSSM        The saved state handle.
 * @returns VBox status code.
 */
int virtioR3SaveExec(PVIRTIOSTATE pVirtio, PCPDMDEVHLPR3 pHlp, PSSMHANDLE pSSM)
{
    pHlp->pfnSSMPutU64(pSSM, VIRTIO_SAVEDSTATE_MARKER);
    pHlp->pfnSSMPutU32(pSSM, VIRTIO_SAVEDSTATE_VERSION);

    pHlp->pfnSSMPutBool(pSSM,   pVirtio->fGenUpdatePending);
    pHlp->pfnSSMPutU8(pSSM,     pVirtio->uDeviceStatus);
    pHlp->pfnSSMPutU8(pSSM,     pVirtio->uConfigGeneration);
    pHlp->pfnSSMPutU8(pSSM,     pVirtio->uPciCfgDataOff);
    pHlp->pfnSSMPutU8(pSSM,     pVirtio->uISR);
    pHlp->pfnSSMPutU16(pSSM,    pVirtio->uQueueSelect);
    pHlp->pfnSSMPutU32(pSSM,    pVirtio->uDeviceFeaturesSelect);
    pHlp->pfnSSMPutU32(pSSM,    pVirtio->uDriverFeaturesSelect);
    pHlp->pfnSSMPutU64(pSSM,    pVirtio->uDriverFeatures);
    Assert(pVirtio->uNumQueues == VIRTQ_MAX_CNT); /** @todo r=bird: See todo in struct & virtioR3LoadExec. */
    pHlp->pfnSSMPutU32(pSSM,    pVirtio->uNumQueues);

    for (uint32_t i = 0; i < pVirtio->uNumQueues; i++)
    {
        pHlp->pfnSSMPutGCPhys64(pSSM, pVirtio->aGCPhysQueueDesc[i]);
        pHlp->pfnSSMPutGCPhys64(pSSM, pVirtio->aGCPhysQueueAvail[i]);
        pHlp->pfnSSMPutGCPhys64(pSSM, pVirtio->aGCPhysQueueUsed[i]);
        pHlp->pfnSSMPutU16(pSSM,      pVirtio->uQueueNotifyOff[i]);
        pHlp->pfnSSMPutU16(pSSM,      pVirtio->uQueueMsixVector[i]);
        pHlp->pfnSSMPutU16(pSSM,      pVirtio->uQueueEnable[i]);
        pHlp->pfnSSMPutU16(pSSM,      pVirtio->uQueueSize[i]);
        pHlp->pfnSSMPutU16(pSSM,      pVirtio->virtqState[i].uAvailIdx);
        pHlp->pfnSSMPutU16(pSSM,      pVirtio->virtqState[i].uUsedIdx);
        int rc = pHlp->pfnSSMPutMem(pSSM, pVirtio->virtqState[i].szVirtqName, 32);
        AssertRCReturn(rc, rc);
    }

    return VINF_SUCCESS;
}

/**
 * Called from the FNSSMDEVLOADEXEC function of the device.
 *
 * @param   pVirtio     Pointer to the virtio state.
 * @param   pHlp        The ring-3 device helpers.
 * @param   pSSM        The saved state handle.
 * @returns VBox status code.
 */
int virtioR3LoadExec(PVIRTIOSTATE pVirtio, PCPDMDEVHLPR3 pHlp, PSSMHANDLE pSSM)
{
    /*
     * Check the marker and (embedded) version number.
     */
    uint64_t uMarker = 0;
    int rc = pHlp->pfnSSMGetU64(pSSM, &uMarker);
    AssertRCReturn(rc, rc);
    if (uMarker != VIRTIO_SAVEDSTATE_MARKER)
        return pHlp->pfnSSMSetLoadError(pSSM, VERR_SSM_DATA_UNIT_FORMAT_CHANGED, RT_SRC_POS,
                                        N_("Expected marker value %#RX64 found %#RX64 instead"),
                                        VIRTIO_SAVEDSTATE_MARKER, uMarker);
    uint32_t uVersion = 0;
    rc = pHlp->pfnSSMGetU32(pSSM, &uVersion);
    AssertRCReturn(rc, rc);
    if (uVersion != VIRTIO_SAVEDSTATE_VERSION)
        return pHlp->pfnSSMSetLoadError(pSSM, VERR_SSM_DATA_UNIT_FORMAT_CHANGED, RT_SRC_POS,
                                        N_("Unsupported virtio version: %u"), uVersion);

    /*
     * Load the state.
     */
    pHlp->pfnSSMGetBool(pSSM, &pVirtio->fGenUpdatePending);
    pHlp->pfnSSMGetU8(pSSM,   &pVirtio->uDeviceStatus);
    pHlp->pfnSSMGetU8(pSSM,   &pVirtio->uConfigGeneration);
    pHlp->pfnSSMGetU8(pSSM,   &pVirtio->uPciCfgDataOff);
    pHlp->pfnSSMGetU8(pSSM,   &pVirtio->uISR);
    pHlp->pfnSSMGetU16(pSSM,  &pVirtio->uQueueSelect);
    pHlp->pfnSSMGetU32(pSSM,  &pVirtio->uDeviceFeaturesSelect);
    pHlp->pfnSSMGetU32(pSSM,  &pVirtio->uDriverFeaturesSelect);
    pHlp->pfnSSMGetU64(pSSM,  &pVirtio->uDriverFeatures);

    /* Make sure the queue count is within expectations. */
    /** @todo r=bird: Turns out the expectations are exactly VIRTQ_MAX_CNT, bug? */
    rc = pHlp->pfnSSMGetU32(pSSM, &pVirtio->uNumQueues);
    AssertRCReturn(rc, rc);
    AssertReturn(pVirtio->uNumQueues == VIRTQ_MAX_CNT,
                 pHlp->pfnSSMSetLoadError(pSSM, VERR_SSM_DATA_UNIT_FORMAT_CHANGED, RT_SRC_POS,
                                          N_("Saved queue count %u, expected %u"), uVersion, VIRTQ_MAX_CNT));
    AssertCompile(RT_ELEMENTS(pVirtio->virtqState) == VIRTQ_MAX_CNT);
    AssertCompile(RT_ELEMENTS(pVirtio->aGCPhysQueueDesc) == VIRTQ_MAX_CNT);

    for (uint32_t idxQueue = 0; idxQueue < pVirtio->uNumQueues; idxQueue++)
    {
        pHlp->pfnSSMGetGCPhys64(pSSM, &pVirtio->aGCPhysQueueDesc[idxQueue]);
        pHlp->pfnSSMGetGCPhys64(pSSM, &pVirtio->aGCPhysQueueAvail[idxQueue]);
        pHlp->pfnSSMGetGCPhys64(pSSM, &pVirtio->aGCPhysQueueUsed[idxQueue]);
        pHlp->pfnSSMGetU16(pSSM, &pVirtio->uQueueNotifyOff[idxQueue]);
        pHlp->pfnSSMGetU16(pSSM, &pVirtio->uQueueMsixVector[idxQueue]);
        pHlp->pfnSSMGetU16(pSSM, &pVirtio->uQueueEnable[idxQueue]);
        pHlp->pfnSSMGetU16(pSSM, &pVirtio->uQueueSize[idxQueue]);
        pHlp->pfnSSMGetU16(pSSM, &pVirtio->virtqState[idxQueue].uAvailIdx);
        pHlp->pfnSSMGetU16(pSSM, &pVirtio->virtqState[idxQueue].uUsedIdx);
        pHlp->pfnSSMGetMem(pSSM,  pVirtio->virtqState[idxQueue].szVirtqName, sizeof(pVirtio->virtqState[idxQueue].szVirtqName));
    }

    return VINF_SUCCESS;
}


/*********************************************************************************************************************************
*   Device Level                                                                                                                 *
*********************************************************************************************************************************/

/**
 * This should be called from PDMDEVREGR3::pfnReset.
 *
 * @param   pVirtio     Pointer to the virtio state.
 */
void virtioR3PropagateResetNotification(PVIRTIOSTATE pVirtio)
{
    /** @todo r=bird: You probably need to do something here.  See
     *        virtioScsiR3Reset. */
    RT_NOREF(pVirtio);
}


/**
 * This sends notification ('kicks') guest driver to check queues for any new
 * elements in the used queue to process.
 *
 * It should be called after resuming in case anything was added to the queues
 * during suspend/quiescing and a notification was missed, to prevent the guest
 * from stalling after suspend.
 */
void virtioR3PropagateResumeNotification(PVIRTIOSTATE pVirtio)
{
    virtioNotifyGuestDriver(pVirtio, (uint16_t)0 /* idxQueue */, true /* fForce */);
}


/**
 * This should be called from PDMDEVREGR3::pfnDestruct.
 *
 * @param   pVirtio     Pointer to the virtio state.
 * @param   pDevIns     The device instance.
 */
void virtioR3Term(PVIRTIOSTATE pVirtio, PPDMDEVINS pDevIns)
{
    if (pVirtio->pvPrevDevSpecificCfg)
    {
        RTMemFree(pVirtio->pvPrevDevSpecificCfg);
        pVirtio->pvPrevDevSpecificCfg = NULL;
    }
    RT_NOREF(pDevIns);
}


/**
 * Setup PCI device controller and Virtio state
 *
 * This should be called from PDMDEVREGR3::pfnConstruct.
 *
 * @param   pVirtio                 Pointer to the virtio state.  This must be
 *                                  the first member in the shared device
 *                                  instance data!
 * @param   pDevIns                 The device instance.
 * @param   pPciParams              Values to populate industry standard PCI Configuration Space data structure
 * @param   pcszInstance            Device instance name (format-specifier)
 * @param   fDevSpecificFeatures    VirtIO device-specific features offered by
 *                                  client
 * @param   cbDevSpecificCfg        Size of virtio_pci_device_cap device-specific struct
 * @param   pvDevSpecificCfg        Address of client's dev-specific
 *                                  configuration struct.
 */
int virtioR3Init(PVIRTIOSTATE pVirtio, PPDMDEVINS pDevIns, PVIRTIOPCIPARAMS pPciParams, const char *pcszInstance,
                 uint64_t fDevSpecificFeatures, void *pvDevSpecificCfg, uint16_t cbDevSpecificCfg)
{
    /*
     * The pVirtio state must be the first member of the shared device instance
     * data, otherwise we cannot get our bearings in the PCI configuration callbacks.
     */
    AssertLogRelReturn(pVirtio == PDMINS_2_DATA(pDevIns, PVIRTIOSTATE), VERR_STATE_CHANGED);


#if 0 /* Until pdmR3DvHlp_PCISetIrq() impl is fixed and Assert that limits vec to 0 is removed */
# ifdef VBOX_WITH_MSI_DEVICES
    pVirtio->fMsiSupport = true;
# endif
#endif

    /*
     * The host features offered include both device-specific features
     * and reserved feature bits (device independent)
     */
    pVirtio->uDeviceFeatures = VIRTIO_F_VERSION_1
                             | VIRTIO_DEV_INDEPENDENT_FEATURES_OFFERED
                             | fDevSpecificFeatures;

    RTStrCopy(pVirtio->szInstance, sizeof(pVirtio->szInstance), pcszInstance);

    pVirtio->pDevInsR3 = pDevIns;
    pVirtio->uDeviceStatus = 0;
    pVirtio->cbDevSpecificCfg = cbDevSpecificCfg;
    pVirtio->pvDevSpecificCfg = pvDevSpecificCfg;
    pVirtio->pvPrevDevSpecificCfg = RTMemDup(pvDevSpecificCfg, cbDevSpecificCfg);
    AssertLogRelReturn(pVirtio->pvPrevDevSpecificCfg, VERR_NO_MEMORY);

    /* Set PCI config registers (assume 32-bit mode) */
    PPDMPCIDEV pPciDev = pDevIns->apPciDevs[0];
    PDMPCIDEV_ASSERT_VALID(pDevIns, pPciDev);

    PDMPciDevSetRevisionId(pPciDev,         DEVICE_PCI_REVISION_ID_VIRTIO);
    PDMPciDevSetVendorId(pPciDev,           DEVICE_PCI_VENDOR_ID_VIRTIO);
    PDMPciDevSetSubSystemVendorId(pPciDev,  DEVICE_PCI_VENDOR_ID_VIRTIO);
    PDMPciDevSetDeviceId(pPciDev,           pPciParams->uDeviceId);
    PDMPciDevSetClassBase(pPciDev,          pPciParams->uClassBase);
    PDMPciDevSetClassSub(pPciDev,           pPciParams->uClassSub);
    PDMPciDevSetClassProg(pPciDev,          pPciParams->uClassProg);
    PDMPciDevSetSubSystemId(pPciDev,        pPciParams->uSubsystemId);
    PDMPciDevSetInterruptLine(pPciDev,      pPciParams->uInterruptLine);
    PDMPciDevSetInterruptPin(pPciDev,       pPciParams->uInterruptPin);

    /* Register PCI device */
    int rc = PDMDevHlpPCIRegister(pDevIns, pPciDev);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("virtio: cannot register PCI Device")); /* can we put params in this error? */

    rc = PDMDevHlpPCIInterceptConfigAccesses(pDevIns, pPciDev, virtioR3PciConfigRead, virtioR3PciConfigWrite);
    AssertRCReturn(rc, rc);


    /* Construct & map PCI vendor-specific capabilities for virtio host negotiation with guest driver */

    /* The following capability mapped via VirtIO 1.0: struct virtio_pci_cfg_cap (VIRTIO_PCI_CFG_CAP_T)
     * as a mandatory but suboptimal alternative interface to host device capabilities, facilitating
     * access the memory of any BAR. If the guest uses it (the VirtIO driver on Linux doesn't),
     * Unlike Common, Notify, ISR and Device capabilities, it is accessed directly via PCI Config region.
     * therefore does not contribute to the capabilities region (BAR) the other capabilities use.
     */
#define CFGADDR2IDX(addr) ((uint8_t)(((uintptr_t)(addr) - (uintptr_t)&pPciDev->abConfig[0])))

    PVIRTIO_PCI_CAP_T pCfg;
    uint32_t cbRegion = 0;

    /* Common capability (VirtIO 1.0 spec, section 4.1.4.3) */
    pCfg = (PVIRTIO_PCI_CAP_T)&pPciDev->abConfig[0x40];
    pCfg->uCfgType = VIRTIO_PCI_CAP_COMMON_CFG;
    pCfg->uCapVndr = VIRTIO_PCI_CAP_ID_VENDOR;
    pCfg->uCapLen  = sizeof(VIRTIO_PCI_CAP_T);
    pCfg->uCapNext = CFGADDR2IDX(pCfg) + pCfg->uCapLen;
    pCfg->uBar     = VIRTIO_REGION_PCI_CAP;
    pCfg->uOffset  = RT_ALIGN_32(0, 4); /* reminder, in case someone changes offset */
    pCfg->uLength  = sizeof(VIRTIO_PCI_COMMON_CFG_T);
    cbRegion += pCfg->uLength;
    pVirtio->pCommonCfgCap = pCfg;

    /*
     * Notify capability (VirtIO 1.0 spec, section 4.1.4.4). Note: uLength is based the choice
     * of this implementation that each queue's uQueueNotifyOff is set equal to (QueueSelect) ordinal
     * value of the queue */
    pCfg = (PVIRTIO_PCI_CAP_T)&pPciDev->abConfig[pCfg->uCapNext];
    pCfg->uCfgType = VIRTIO_PCI_CAP_NOTIFY_CFG;
    pCfg->uCapVndr = VIRTIO_PCI_CAP_ID_VENDOR;
    pCfg->uCapLen  = sizeof(VIRTIO_PCI_NOTIFY_CAP_T);
    pCfg->uCapNext = CFGADDR2IDX(pCfg) + pCfg->uCapLen;
    pCfg->uBar     = VIRTIO_REGION_PCI_CAP;
    pCfg->uOffset  = pVirtio->pCommonCfgCap->uOffset + pVirtio->pCommonCfgCap->uLength;
    pCfg->uOffset  = RT_ALIGN_32(pCfg->uOffset, 2);
    pCfg->uLength  = VIRTQ_MAX_CNT * VIRTIO_NOTIFY_OFFSET_MULTIPLIER + 2;  /* will change in VirtIO 1.1 */
    cbRegion += pCfg->uLength;
    pVirtio->pNotifyCap = (PVIRTIO_PCI_NOTIFY_CAP_T)pCfg;
    pVirtio->pNotifyCap->uNotifyOffMultiplier = VIRTIO_NOTIFY_OFFSET_MULTIPLIER;

    /* ISR capability (VirtIO 1.0 spec, section 4.1.4.5)
     *
     * VirtIO 1.0 spec says 8-bit, unaligned in MMIO space. Example/diagram
     * of spec shows it as a 32-bit field with upper bits 'reserved'
     * Will take spec words more literally than the diagram for now.
     */
    pCfg = (PVIRTIO_PCI_CAP_T)&pPciDev->abConfig[pCfg->uCapNext];
    pCfg->uCfgType = VIRTIO_PCI_CAP_ISR_CFG;
    pCfg->uCapVndr = VIRTIO_PCI_CAP_ID_VENDOR;
    pCfg->uCapLen  = sizeof(VIRTIO_PCI_CAP_T);
    pCfg->uCapNext = CFGADDR2IDX(pCfg) + pCfg->uCapLen;
    pCfg->uBar     = VIRTIO_REGION_PCI_CAP;
    pCfg->uOffset  = pVirtio->pNotifyCap->pciCap.uOffset + pVirtio->pNotifyCap->pciCap.uLength;
    pCfg->uLength  = sizeof(uint8_t);
    cbRegion += pCfg->uLength;
    pVirtio->pIsrCap = pCfg;

    /*  PCI Cfg capability (VirtIO 1.0 spec, section 4.1.4.7)
     *  This capability doesn't get page-MMIO mapped. Instead uBar, uOffset and uLength are intercepted
     *  by trapping PCI configuration I/O and get modulated by consumers to locate fetch and read/write
     *  values from any region. NOTE: The linux driver not only doesn't use this feature, it will not
     *  even list it as present if uLength isn't non-zero and 4-byte-aligned as the linux driver is
     *  initializing. */

    pVirtio->uPciCfgDataOff = pCfg->uCapNext + RT_OFFSETOF(VIRTIO_PCI_CFG_CAP_T, uPciCfgData);
    pCfg = (PVIRTIO_PCI_CAP_T)&pPciDev->abConfig[pCfg->uCapNext];
    pCfg->uCfgType = VIRTIO_PCI_CAP_PCI_CFG;
    pCfg->uCapVndr = VIRTIO_PCI_CAP_ID_VENDOR;
    pCfg->uCapLen  = sizeof(VIRTIO_PCI_CFG_CAP_T);
    pCfg->uCapNext = (pVirtio->fMsiSupport || pVirtio->pvDevSpecificCfg) ? CFGADDR2IDX(pCfg) + pCfg->uCapLen : 0;
    pCfg->uBar     = 0;
    pCfg->uOffset  = 0;
    pCfg->uLength  = 0;
    cbRegion += pCfg->uLength;
    pVirtio->pPciCfgCap = (PVIRTIO_PCI_CFG_CAP_T)pCfg;

    if (pVirtio->pvDevSpecificCfg)
    {
        /* Following capability (via VirtIO 1.0, section 4.1.4.6). Client defines the
         * device-specific config fields struct and passes size to this constructor */
        pCfg = (PVIRTIO_PCI_CAP_T)&pPciDev->abConfig[pCfg->uCapNext];
        pCfg->uCfgType = VIRTIO_PCI_CAP_DEVICE_CFG;
        pCfg->uCapVndr = VIRTIO_PCI_CAP_ID_VENDOR;
        pCfg->uCapLen  = sizeof(VIRTIO_PCI_CAP_T);
        pCfg->uCapNext = pVirtio->fMsiSupport ? CFGADDR2IDX(pCfg) + pCfg->uCapLen : 0;
        pCfg->uBar     = VIRTIO_REGION_PCI_CAP;
        pCfg->uOffset  = pVirtio->pIsrCap->uOffset + pVirtio->pIsrCap->uLength;
        pCfg->uOffset  = RT_ALIGN_32(pCfg->uOffset, 4);
        pCfg->uLength  = cbDevSpecificCfg;
        cbRegion += pCfg->uLength;
        pVirtio->pDeviceCap = pCfg;
    }

    if (pVirtio->fMsiSupport)
    {
        PDMMSIREG aMsiReg;
        RT_ZERO(aMsiReg);
        aMsiReg.iMsixCapOffset  = pCfg->uCapNext;
        aMsiReg.iMsixNextOffset = 0;
        aMsiReg.iMsixBar        = VIRTIO_REGION_MSIX_CAP;
        aMsiReg.cMsixVectors    = VBOX_MSIX_MAX_ENTRIES;
        rc = PDMDevHlpPCIRegisterMsi(pDevIns, &aMsiReg); /* see MsixR3init() */
        if (RT_FAILURE(rc))
        {
            /* See PDMDevHlp.cpp:pdmR3DevHlp_PCIRegisterMsi */
            LogFunc(("Failed to configure MSI-X (%Rrc). Reverting to INTx\n", rc));
            pVirtio->fMsiSupport = false;
        }
        else
            Log2Func(("Using MSI-X for guest driver notification\n"));
    }
    else
        LogFunc(("MSI-X not available for VBox, using INTx notification\n"));


    /* Set offset to first capability and enable PCI dev capabilities */
    PDMPciDevSetCapabilityList(pPciDev, 0x40);
    PDMPciDevSetStatus(pPciDev,         VBOX_PCI_STATUS_CAP_LIST);

    /* Linux drivers/virtio/virtio_pci_modern.c tries to map at least a page for the
     * 'unknown' device-specific capability without querying the capability to figure
     *  out size, so pad with an extra page */

    rc = PDMDevHlpPCIIORegionRegister(pDevIns, VIRTIO_REGION_PCI_CAP,  RT_ALIGN_32(cbRegion + PAGE_SIZE, PAGE_SIZE),
                                      PCI_ADDRESS_SPACE_MEM, virtioR3Map);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("virtio: cannot register PCI Capabilities address space"));

    return rc;
}

#endif /* IN_RING3 */
