/* $Id$ */
/** @file
 * DBGF - Debugger Facility, Call Stack Analyser.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DBGF
#include <VBox/dbgf.h>
#include <VBox/selm.h>
#include "DBGFInternal.h"
#include <VBox/vm.h>
#include <VBox/mm.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/alloca.h>



/**
 * Read stack memory.
 */
DECLINLINE(int) dbgfR3Read(PVM pVM, void *pvBuf, PCDBGFADDRESS pSrcAddr, size_t cb, size_t *pcbRead)
{
    /** @todo SMP support! */
    VMCPUID idCpu = 0;

    int rc = DBGFR3MemRead(pVM, idCpu, pSrcAddr, pvBuf, cb);
    if (RT_FAILURE(rc))
    {
        /* fallback: byte by byte and zero the ones we fail to read. */
        size_t cbRead;
        for (cbRead = 0; cbRead < cb; cbRead++)
        {
            DBGFADDRESS Addr = *pSrcAddr;
            rc = DBGFR3MemRead(pVM, idCpu, DBGFR3AddrAdd(&Addr, cbRead), (uint8_t *)pvBuf + cbRead, 1);
            if (RT_FAILURE(rc))
                break;
        }
        if (cbRead)
            rc = VINF_SUCCESS;
        memset((char *)pvBuf + cbRead, 0, cb - cbRead);
        *pcbRead = cbRead;
    }
    else
        *pcbRead = cb;
    return rc;
}


/**
 * Internal worker routine.
 *
 * On x86 the typical stack frame layout is like this:
 *     ..  ..
 *     16  parameter 2
 *     12  parameter 1
 *      8  parameter 0
 *      4  return address
 *      0  old ebp; current ebp points here
 *
 * @todo Add AMD64 support (needs teaming up with the module management for
 *       unwind tables).
 */
static int dbgfR3StackWalk(PVM pVM, PDBGFSTACKFRAME pFrame)
{
    VMCPUID idCpu = 0; /** @todo SMP */

    /*
     * Stop if we got a read error in the previous run.
     */
    if (pFrame->fFlags & DBGFSTACKFRAME_FLAGS_LAST)
        return VERR_NO_MORE_FILES;

    /*
     * Read the raw frame data.
     */
    const DBGFADDRESS AddrOldPC = pFrame->AddrPC;
    const unsigned cbRetAddr = DBGFReturnTypeSize(pFrame->enmReturnType);
    unsigned cbStackItem;
    switch (AddrOldPC.fFlags & DBGFADDRESS_FLAGS_TYPE_MASK)
    {
        case DBGFADDRESS_FLAGS_FAR16: cbStackItem = 2; break;
        case DBGFADDRESS_FLAGS_FAR32: cbStackItem = 4; break;
        case DBGFADDRESS_FLAGS_FAR64: cbStackItem = 8; break;
        default:                      cbStackItem = 4; break; /// @todo 64-bit guests.
    }

    union
    {
        uint64_t *pu64;
        uint32_t *pu32;
        uint16_t *pu16;
        uint8_t  *pb;
        void     *pv;
    } u, uRet, uArgs, uBp;
    size_t cbRead = cbRetAddr + cbStackItem + sizeof(pFrame->Args);
    u.pv = alloca(cbRead);
    uBp = u;
    uRet.pb = u.pb + cbStackItem;
    uArgs.pb = u.pb + cbStackItem + cbRetAddr;

    Assert(DBGFADDRESS_IS_VALID(&pFrame->AddrFrame));
    int rc = dbgfR3Read(pVM, u.pv,
                        pFrame->fFlags & DBGFSTACKFRAME_FLAGS_ALL_VALID
                        ? &pFrame->AddrReturnFrame
                        : &pFrame->AddrFrame,
                        cbRead, &cbRead);
    if (    RT_FAILURE(rc)
        ||  cbRead < cbRetAddr + cbStackItem)
        pFrame->fFlags |= DBGFSTACKFRAME_FLAGS_LAST;

    /*
     * The first step is taken in a different way than the others.
     */
    if (!(pFrame->fFlags & DBGFSTACKFRAME_FLAGS_ALL_VALID))
    {
        pFrame->fFlags |= DBGFSTACKFRAME_FLAGS_ALL_VALID;
        pFrame->iFrame = 0;

        /* Current PC - set by caller, just find symbol & line. */
        if (DBGFADDRESS_IS_VALID(&pFrame->AddrPC))
        {
            pFrame->pSymPC  = DBGFR3SymbolByAddrAlloc(pVM, pFrame->AddrPC.FlatPtr, NULL);
            pFrame->pLinePC = DBGFR3LineByAddrAlloc(pVM, pFrame->AddrPC.FlatPtr, NULL);
        }
    }
    else /* 2nd and subsequent steps */
    {
        /* frame, pc and stack is taken from the existing frames return members. */
        pFrame->AddrFrame = pFrame->AddrReturnFrame;
        pFrame->AddrPC    = pFrame->AddrReturnPC;
        pFrame->pSymPC    = pFrame->pSymReturnPC;
        pFrame->pLinePC   = pFrame->pLineReturnPC;

        /* increment the frame number. */
        pFrame->iFrame++;
    }

    /*
     * Return Frame address.
     */
    pFrame->AddrReturnFrame = pFrame->AddrFrame;
    switch (cbStackItem)
    {
        case 2:    pFrame->AddrReturnFrame.off = *uBp.pu16; break;
        case 4:    pFrame->AddrReturnFrame.off = *uBp.pu32; break;
        case 8:    pFrame->AddrReturnFrame.off = *uBp.pu64; break;
        default:    AssertMsgFailed(("cbStackItem=%d\n", cbStackItem)); return VERR_INTERNAL_ERROR;
    }
    pFrame->AddrReturnFrame.FlatPtr += pFrame->AddrReturnFrame.off - pFrame->AddrFrame.off;

    /*
     * Return PC and Stack Addresses.
     */
    /** @todo AddrReturnStack is not correct for stdcall and pascal. (requires scope info) */
    pFrame->AddrReturnStack          = pFrame->AddrFrame;
    pFrame->AddrReturnStack.off     += cbStackItem + cbRetAddr;
    pFrame->AddrReturnStack.FlatPtr += cbStackItem + cbRetAddr;

    pFrame->AddrReturnPC             = pFrame->AddrPC;
    switch (pFrame->enmReturnType)
    {
        case DBGFRETURNTYPE_NEAR16:
            if (DBGFADDRESS_IS_VALID(&pFrame->AddrReturnPC))
            {
                pFrame->AddrReturnPC.FlatPtr += *uRet.pu16 - pFrame->AddrReturnPC.off;
                pFrame->AddrReturnPC.off      = *uRet.pu16;
            }
            else
                DBGFR3AddrFromFlat(pVM, &pFrame->AddrReturnPC, *uRet.pu16);
            break;
        case DBGFRETURNTYPE_NEAR32:
            if (DBGFADDRESS_IS_VALID(&pFrame->AddrReturnPC))
            {
                pFrame->AddrReturnPC.FlatPtr += *uRet.pu32 - pFrame->AddrReturnPC.off;
                pFrame->AddrReturnPC.off      = *uRet.pu32;
            }
            else
                DBGFR3AddrFromFlat(pVM, &pFrame->AddrReturnPC, *uRet.pu32);
            break;
        case DBGFRETURNTYPE_NEAR64:
            if (DBGFADDRESS_IS_VALID(&pFrame->AddrReturnPC))
            {
                pFrame->AddrReturnPC.FlatPtr += *uRet.pu64 - pFrame->AddrReturnPC.off;
                pFrame->AddrReturnPC.off      = *uRet.pu64;
            }
            else
                DBGFR3AddrFromFlat(pVM, &pFrame->AddrReturnPC, *uRet.pu64);
            break;
        case DBGFRETURNTYPE_FAR16:
            DBGFR3AddrFromSelOff(pVM, idCpu, &pFrame->AddrReturnPC, uRet.pu16[1], uRet.pu16[0]);
            break;
        case DBGFRETURNTYPE_FAR32:
            DBGFR3AddrFromSelOff(pVM, idCpu, &pFrame->AddrReturnPC, uRet.pu16[2], uRet.pu32[0]);
            break;
        case DBGFRETURNTYPE_FAR64:
            DBGFR3AddrFromSelOff(pVM, idCpu, &pFrame->AddrReturnPC, uRet.pu16[4], uRet.pu64[0]);
            break;
        case DBGFRETURNTYPE_IRET16:
            DBGFR3AddrFromSelOff(pVM, idCpu, &pFrame->AddrReturnPC, uRet.pu16[1], uRet.pu16[0]);
            break;
        case DBGFRETURNTYPE_IRET32:
            DBGFR3AddrFromSelOff(pVM, idCpu, &pFrame->AddrReturnPC, uRet.pu16[2], uRet.pu32[0]);
            break;
        case DBGFRETURNTYPE_IRET32_PRIV:
            DBGFR3AddrFromSelOff(pVM, idCpu, &pFrame->AddrReturnPC, uRet.pu16[2], uRet.pu32[0]);
            break;
        case DBGFRETURNTYPE_IRET32_V86:
            DBGFR3AddrFromSelOff(pVM, idCpu, &pFrame->AddrReturnPC, uRet.pu16[2], uRet.pu32[0]);
            break;
        case DBGFRETURNTYPE_IRET64:
            DBGFR3AddrFromSelOff(pVM, idCpu, &pFrame->AddrReturnPC, uRet.pu16[4], uRet.pu64[0]);
            break;
        default:
            AssertMsgFailed(("enmReturnType=%d\n", pFrame->enmReturnType));
            return VERR_INVALID_PARAMETER;
    }

    pFrame->pSymReturnPC  = DBGFR3SymbolByAddrAlloc(pVM, pFrame->AddrReturnPC.FlatPtr, NULL);
    pFrame->pLineReturnPC = DBGFR3LineByAddrAlloc(pVM, pFrame->AddrReturnPC.FlatPtr, NULL);

    /*
     * The arguments.
     */
    memcpy(&pFrame->Args, uArgs.pv, sizeof(pFrame->Args));

    return VINF_SUCCESS;
}


/**
 * Walks the entire stack allocating memory as we walk.
 */
static DECLCALLBACK(int) dbgfR3StackWalkCtxFull(PVM pVM, PDBGFSTACKFRAME pFrame, PCCPUMCTXCORE pCtxCore, bool fGuest)
{
    VMCPUID idCpu = 0; /** @todo SMP */

    pFrame->pNext = NULL;
    pFrame->pFirst = NULL;

    /* alloc first frame. */
    PDBGFSTACKFRAME pCur = (PDBGFSTACKFRAME)MMR3HeapAllocZ(pVM, MM_TAG_DBGF_STACK, sizeof(*pCur));
    if (!pCur)
        return VERR_NO_MEMORY;

    /* copy input frame */
    pCur->AddrFrame = pFrame->AddrFrame;
    pCur->AddrStack = pFrame->AddrStack;
    pCur->AddrPC    = pFrame->AddrPC;
    pCur->enmReturnType = pFrame->enmReturnType;
    pCur->pNext = NULL;
    pCur->pFirst = pCur;

    int rc = VINF_SUCCESS;
    if (!DBGFADDRESS_IS_VALID(&pCur->AddrPC))
        rc = DBGFR3AddrFromSelOff(pVM, idCpu, &pCur->AddrPC, pCtxCore->cs, pCtxCore->eip);
    if (RT_SUCCESS(rc) /*&& pCur->enmReturnType == DBGFRETURNTYPE_INVALID*/)
    {
        switch (pCur->AddrPC.fFlags & DBGFADDRESS_FLAGS_TYPE_MASK)
        {
            case DBGFADDRESS_FLAGS_FAR16: pCur->enmReturnType = DBGFRETURNTYPE_NEAR16; break;
            case DBGFADDRESS_FLAGS_FAR32: pCur->enmReturnType = DBGFRETURNTYPE_NEAR32; break;
            case DBGFADDRESS_FLAGS_FAR64: pCur->enmReturnType = DBGFRETURNTYPE_NEAR64; break;
            default:                      pCur->enmReturnType = DBGFRETURNTYPE_NEAR32; break; /// @todo 64-bit guests
        }
    }
    uint64_t u64Mask = UINT64_MAX;
    if (RT_SUCCESS(rc) && DBGFADDRESS_IS_FAR16(&pCur->AddrPC) && fGuest)
        u64Mask = UINT16_MAX;
    if (RT_SUCCESS(rc) && !DBGFADDRESS_IS_VALID(&pCur->AddrStack))
        rc = DBGFR3AddrFromSelOff(pVM, idCpu, &pCur->AddrStack, pCtxCore->ss, pCtxCore->esp & u64Mask);
    if (RT_SUCCESS(rc) && !DBGFADDRESS_IS_VALID(&pCur->AddrFrame))
        rc = DBGFR3AddrFromSelOff(pVM, idCpu, &pCur->AddrFrame, pCtxCore->ss, pCtxCore->ebp & u64Mask);

    /*
     * The first frame.
     */
    if (RT_SUCCESS(rc))
        rc = dbgfR3StackWalk(pVM, pCur);
    if (RT_FAILURE(rc))
    {
        DBGFR3StackWalkEnd(pVM, pCur);
        return rc;
    }

    /*
     * The other frames.
     */
    DBGFSTACKFRAME Next = *pCur;
    while (!(pCur->fFlags & (DBGFSTACKFRAME_FLAGS_LAST | DBGFSTACKFRAME_FLAGS_MAX_DEPTH | DBGFSTACKFRAME_FLAGS_LOOP)))
    {
        /* try walk. */
        rc = dbgfR3StackWalk(pVM, &Next);
        if (RT_FAILURE(rc))
            break;

        /* add the next frame to the chain. */
        PDBGFSTACKFRAME pNext = (PDBGFSTACKFRAME)MMR3HeapAlloc(pVM, MM_TAG_DBGF_STACK, sizeof(*pNext));
        if (!pNext)
        {
            DBGFR3StackWalkEnd(pVM, pCur);
            return VERR_NO_MEMORY;
        }
        *pNext = Next;
        pCur = pCur->pNext = pNext;
        Assert(pCur->pNext == NULL);

        /* check for loop */
        for (PDBGFSTACKFRAME pLoop = pCur->pFirst; pLoop && pLoop != pCur; pLoop = pLoop->pNext)
            if (pLoop->AddrFrame.FlatPtr == pCur->AddrFrame.FlatPtr)
            {
                pCur->fFlags |= DBGFSTACKFRAME_FLAGS_LOOP;
                break;
            }

        /* check for insane recursion */
        if (pCur->iFrame >= 2048)
            pCur->fFlags |= DBGFSTACKFRAME_FLAGS_MAX_DEPTH;
    }

    *pFrame = *pCur->pFirst;
    return rc;
}


/**
 * Begins a stack walk.
 * This will construct and obtain the first frame.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VERR_NO_MEMORY if we're out of memory.
 * @param   pVM         The VM handle.
 * @param   pFrame      The stack frame info structure.
 *                      On input this structure must be memset to zero.
 *                      If wanted, the AddrPC, AddrStack and AddrFrame fields may be set
 *                      to valid addresses after memsetting it. Any of those fields not set
 *                      will be fetched from the guest CPU state.
 *                      On output the structure will contain all the information we were able to
 *                      obtain about the stack frame.
 */
VMMR3DECL(int) DBGFR3StackWalkBeginGuest(PVM pVM, PDBGFSTACKFRAME pFrame)
{
    pFrame->pFirst = NULL;
    pFrame->pNext = NULL;

    PVMREQ pReq;
    int rc = VMR3ReqCall(pVM, VMREQDEST_ANY, &pReq, RT_INDEFINITE_WAIT, (PFNRT)dbgfR3StackWalkCtxFull, 4,
                         pVM, pFrame, CPUMGetGuestCtxCore(VMMGetCpu(pVM)), true); /* @todo SMP support!! */
    if (RT_SUCCESS(rc))
        rc = pReq->iStatus;
    VMR3ReqFree(pReq);

    return rc;
}


/**
 * Begins a stack walk.
 * This will construct and obtain the first frame.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VERR_NO_MEMORY if we're out of memory.
 * @param   pVM         The VM handle.
 * @param   pFrame      The stack frame info structure.
 *                      On input this structure must be memset to zero.
 *                      If wanted, the AddrPC, AddrStack and AddrFrame fields may be set
 *                      to valid addresses after memsetting it. Any of those fields not set
 *                      will be fetched from the hypervisor CPU state.
 *                      On output the structure will contain all the information we were able to
 *                      obtain about the stack frame.
 */
VMMR3DECL(int) DBGFR3StackWalkBeginHyper(PVM pVM, PDBGFSTACKFRAME pFrame)
{
    /* @todo SMP support! */
    PVMCPU pVCpu = &pVM->aCpus[0];

    pFrame->pFirst = NULL;
    pFrame->pNext = NULL;

    PVMREQ pReq;
    int rc = VMR3ReqCall(pVM, VMREQDEST_ANY, &pReq, RT_INDEFINITE_WAIT, (PFNRT)dbgfR3StackWalkCtxFull, 4,
                         pVM, pFrame, CPUMGetHyperCtxCore(pVCpu), 4);
    if (RT_SUCCESS(rc))
        rc = pReq->iStatus;
    VMR3ReqFree(pReq);

    return rc;
}


/**
 * Gets the next stack frame.
 *
 * @returns VINF_SUCCESS
 * @returns VERR_NO_MORE_FILES if not more stack frames.
 * @param   pVM         The VM handle.
 * @param   pFrame      Pointer to the current frame on input, content is replaced with the next frame on successful return.
 */
VMMR3DECL(int) DBGFR3StackWalkNext(PVM pVM, PDBGFSTACKFRAME pFrame)
{
    if (pFrame->pNext)
    {
        *pFrame = *pFrame->pNext;
        return VINF_SUCCESS;
    }
    return VERR_NO_MORE_FILES;
}


/**
 * Ends a stack walk process.
 *
 * This *must* be called after a successful first call to any of the stack
 * walker functions. If not called we will leak memory or other resources.
 *
 * @param   pVM         The VM handle.
 * @param   pFrame      The stackframe as returned by the last stack walk call.
 */
VMMR3DECL(void) DBGFR3StackWalkEnd(PVM pVM, PDBGFSTACKFRAME pFrame)
{
    if (!pFrame || !pFrame->pFirst)
        return;

    pFrame = pFrame->pFirst;
    while (pFrame)
    {
        PDBGFSTACKFRAME pCur = pFrame;
        pFrame = pCur->pNext;
        if (pFrame)
        {
            if (pCur->pSymReturnPC == pFrame->pSymPC)
                pFrame->pSymPC = NULL;
            if (pCur->pSymReturnPC == pFrame->pSymReturnPC)
                pFrame->pSymReturnPC = NULL;

            if (pCur->pSymPC == pFrame->pSymPC)
                pFrame->pSymPC = NULL;
            if (pCur->pSymPC == pFrame->pSymReturnPC)
                pFrame->pSymReturnPC = NULL;

            if (pCur->pLineReturnPC == pFrame->pLinePC)
                pFrame->pLinePC = NULL;
            if (pCur->pLineReturnPC == pFrame->pLineReturnPC)
                pFrame->pLineReturnPC = NULL;

            if (pCur->pLinePC == pFrame->pLinePC)
                pFrame->pLinePC = NULL;
            if (pCur->pLinePC == pFrame->pLineReturnPC)
                pFrame->pLineReturnPC = NULL;
        }

        DBGFR3SymbolFree(pCur->pSymPC);
        DBGFR3SymbolFree(pCur->pSymReturnPC);
        DBGFR3LineFree(pCur->pLinePC);
        DBGFR3LineFree(pCur->pLineReturnPC);

        pCur->pNext = NULL;
        pCur->pFirst = NULL;
        pCur->fFlags = 0;
        MMR3HeapFree(pCur);
    }
}

