/* $Id$ */
/** @file
 * IPRT - Fuzzing framework API, target state recorder.
 */

/*
 * Copyright (C) 2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/fuzz.h>
#include "internal/iprt.h"

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/avl.h>
#include <iprt/ctype.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/list.h>
#include <iprt/mem.h>
#include <iprt/pipe.h>
#include <iprt/semaphore.h>
#include <iprt/string.h>



/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/** Pointer to the internal fuzzed target recorder state. */
typedef struct RTFUZZTGTRECINT *PRTFUZZTGTRECINT;


/**
 * Stdout/Stderr buffer.
 */
typedef struct RTFUZZTGTSTDOUTERRBUF
{
    /** Current amount buffered. */
    size_t                      cbBuf;
    /** Maxmium amount to buffer. */
    size_t                      cbBufMax;
    /** Base pointer to the data buffer. */
    uint8_t                     *pbBase;
} RTFUZZTGTSTDOUTERRBUF;
/** Pointer to a stdout/stderr buffer. */
typedef RTFUZZTGTSTDOUTERRBUF *PRTFUZZTGTSTDOUTERRBUF;


/**
 * Internal fuzzed target state.
 */
typedef struct RTFUZZTGTSTATEINT
{
    /** Node for the list of states. */
    RTLISTNODE                  NdStates;
    /** Magic identifying the structure. */
    uint32_t                    u32Magic;
    /** Reference counter. */
    volatile uint32_t           cRefs;
    /** The owning recorder instance. */
    PRTFUZZTGTRECINT            pTgtRec;
    /** Flag whether the state is finalized. */
    bool                        fFinalized;
    /** Flag whether the state is contained in the recorded set. */
    bool                        fInRecSet;
    /** The stdout data buffer. */
    RTFUZZTGTSTDOUTERRBUF       StdOutBuf;
    /** The stderr data buffer. */
    RTFUZZTGTSTDOUTERRBUF       StdErrBuf;
} RTFUZZTGTSTATEINT;
/** Pointer to an internal fuzzed target state. */
typedef RTFUZZTGTSTATEINT *PRTFUZZTGTSTATEINT;


/**
 * Recorder states node in the AVL tree.
 */
typedef struct RTFUZZTGTRECNODE
{
    /** The AVL tree core (keyed by stdout/stderr buffer sizes). */
    AVLU64NODECORE              Core;
    /** The list anchor for the individual states. */
    RTLISTANCHOR                LstStates;
} RTFUZZTGTRECNODE;
/** Pointer to a recorder states node. */
typedef RTFUZZTGTRECNODE *PRTFUZZTGTRECNODE;


/**
 * Internal fuzzed target recorder state.
 */
typedef struct RTFUZZTGTRECINT
{
    /** Magic value for identification. */
    uint32_t                    u32Magic;
    /** Reference counter. */
    volatile uint32_t           cRefs;
    /** Semaphore protecting the states tree. */
    RTSEMRW                     hSemRwStates;
    /** The AVL tree for indexing the recorded state (keyed by stdout/stderr buffer size). */
    AVLU64TREE                  TreeStates;
} RTFUZZTGTRECINT;


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/

/**
 * Initializes the given stdout/stderr buffer.
 *
 * @returns nothing.
 * @param   pBuf                The buffer to initialize.
 */
static void rtFuzzTgtStdOutErrBufInit(PRTFUZZTGTSTDOUTERRBUF pBuf)
{
    pBuf->cbBuf    = 0;
    pBuf->cbBufMax = 0;
    pBuf->pbBase   = NULL;
}


/**
 * Frees all allocated resources in the given stdout/stderr buffer.
 *
 * @returns nothing.
 * @param   pBuf                The buffer to free.
 */
static void rtFuzzTgtStdOutErrBufFree(PRTFUZZTGTSTDOUTERRBUF pBuf)
{
    if (pBuf->pbBase)
        RTMemFree(pBuf->pbBase);
}


/**
 * Fills the given stdout/stderr buffer from the given pipe.
 *
 * @returns IPRT status code.
 * @param   pBuf                The buffer to fill.
 * @param   hPipeRead           The pipe to read from.
 */
static int rtFuzzTgtStdOutErrBufFillFromPipe(PRTFUZZTGTSTDOUTERRBUF pBuf, RTPIPE hPipeRead)
{
    int rc = VINF_SUCCESS;

    size_t cbRead = 0;
    size_t cbThisRead = 0;
    do
    {
        cbThisRead = pBuf->cbBufMax - pBuf->cbBuf;
        if (!cbThisRead)
        {
            /* Try to increase the buffer. */
            uint8_t *pbNew = (uint8_t *)RTMemRealloc(pBuf->pbBase, pBuf->cbBufMax + _4K);
            if (RT_LIKELY(pbNew))
            {
                pBuf->cbBufMax += _4K;
                pBuf->pbBase   = pbNew;
            }
            cbThisRead = pBuf->cbBufMax - pBuf->cbBuf;
        }

        if (cbThisRead)
        {
            rc = RTPipeRead(hPipeRead, pBuf->pbBase + pBuf->cbBuf, cbThisRead, &cbRead);
            if (RT_SUCCESS(rc))
                pBuf->cbBuf += cbRead;
        }
        else
            rc = VERR_NO_MEMORY;
    } while (   RT_SUCCESS(rc)
             && cbRead == cbThisRead);

    return rc;
}


/**
 * Destorys the given fuzzer target recorder freeing all allocated resources.
 *
 * @returns nothing.
 * @param   pThis               The fuzzer target recorder instance.
 */
static void rtFuzzTgtRecDestroy(PRTFUZZTGTRECINT pThis)
{
    RT_NOREF(pThis);
}


/**
 * Destorys the given fuzzer target recorder freeing all allocated resources.
 *
 * @returns nothing.
 * @param   pThis               The fuzzed target state instance.
 */
static void rtFuzzTgtStateDestroy(PRTFUZZTGTSTATEINT pThis)
{
    pThis->u32Magic = ~(uint32_t)0; /** @todo Dead magic */
    rtFuzzTgtStdOutErrBufFree(&pThis->StdOutBuf);
    rtFuzzTgtStdOutErrBufFree(&pThis->StdErrBuf);
    RTMemFree(pThis);
}


RTDECL(int) RTFuzzTgtRecorderCreate(PRTFUZZTGTREC phFuzzTgtRec)
{
    int rc;
    PRTFUZZTGTRECINT pThis = (PRTFUZZTGTRECINT)RTMemAllocZ(sizeof(*pThis));
    if (RT_LIKELY(pThis))
    {
        pThis->u32Magic         = 0; /** @todo */
        pThis->cRefs            = 1;
        pThis->TreeStates       = NULL;

        rc = RTSemRWCreate(&pThis->hSemRwStates);
        if (RT_SUCCESS(rc))
        {
            *phFuzzTgtRec = pThis;
            return VINF_SUCCESS;
        }

        RTMemFree(pThis);
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}


RTDECL(uint32_t) RTFuzzTgtRecorderRetain(RTFUZZTGTREC hFuzzTgtRec)
{
    PRTFUZZTGTRECINT pThis = hFuzzTgtRec;

    AssertPtrReturn(pThis, UINT32_MAX);

    uint32_t cRefs = ASMAtomicIncU32(&pThis->cRefs);
    AssertMsg(cRefs > 1 && cRefs < _1M, ("%#x %p\n", cRefs, pThis));
    return cRefs;
}


RTDECL(uint32_t) RTFuzzTgtRecorderRelease(RTFUZZTGTREC hFuzzTgtRec)
{
    PRTFUZZTGTRECINT pThis = hFuzzTgtRec;
    if (pThis == NIL_RTFUZZTGTREC)
        return 0;
    AssertPtrReturn(pThis, UINT32_MAX);

    uint32_t cRefs = ASMAtomicDecU32(&pThis->cRefs);
    AssertMsg(cRefs < _1M, ("%#x %p\n", cRefs, pThis));
    if (cRefs == 0)
        rtFuzzTgtRecDestroy(pThis);
    return cRefs;
}


RTDECL(int) RTFuzzTgtRecorderCreateNewState(RTFUZZTGTREC hFuzzTgtRec, PRTFUZZTGTSTATE phFuzzTgtState)
{
    PRTFUZZTGTRECINT pThis = hFuzzTgtRec;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertPtrReturn(phFuzzTgtState, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;
    PRTFUZZTGTSTATEINT pState = (PRTFUZZTGTSTATEINT)RTMemAllocZ(sizeof(*pState));
    if (RT_LIKELY(pState))
    {
        pState->u32Magic   = 0; /** @todo */
        pState->cRefs      = 1;
        pState->pTgtRec    = pThis;
        pState->fFinalized = false;
        rtFuzzTgtStdOutErrBufInit(&pState->StdOutBuf);
        rtFuzzTgtStdOutErrBufInit(&pState->StdErrBuf);
        *phFuzzTgtState = pState;
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}


RTDECL(uint32_t) RTFuzzTgtStateRetain(RTFUZZTGTSTATE hFuzzTgtState)
{
    PRTFUZZTGTSTATEINT pThis = hFuzzTgtState;

    AssertPtrReturn(pThis, UINT32_MAX);

    uint32_t cRefs = ASMAtomicIncU32(&pThis->cRefs);
    AssertMsg(cRefs > 1 && cRefs < _1M, ("%#x %p\n", cRefs, pThis));
    return cRefs;
}


RTDECL(uint32_t) RTFuzzTgtStateRelease(RTFUZZTGTSTATE hFuzzTgtState)
{
    PRTFUZZTGTSTATEINT pThis = hFuzzTgtState;
    if (pThis == NIL_RTFUZZTGTSTATE)
        return 0;
    AssertPtrReturn(pThis, UINT32_MAX);

    uint32_t cRefs = ASMAtomicDecU32(&pThis->cRefs);
    AssertMsg(cRefs < _1M, ("%#x %p\n", cRefs, pThis));
    if (cRefs == 0 && !pThis->fInRecSet)
        rtFuzzTgtStateDestroy(pThis);
    return cRefs;
}


RTDECL(int) RTFuzzTgtStateReset(RTFUZZTGTSTATE hFuzzTgtState)
{
    PRTFUZZTGTSTATEINT pThis = hFuzzTgtState;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    /* Clear the buffers. */
    pThis->StdOutBuf.cbBuf = 0;
    pThis->StdErrBuf.cbBuf = 0;
    pThis->fFinalized      = false;
    return VINF_SUCCESS;
}


RTDECL(int) RTFuzzTgtStateFinalize(RTFUZZTGTSTATE hFuzzTgtState)
{
    PRTFUZZTGTSTATEINT pThis = hFuzzTgtState;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    /*
     * As we key both the stdout and stderr sizes in one 64bit
     * AVL tree core we only have 32bit for each and refuse buffers
     * exceeding this size (very unlikely for now).
     */
    if (RT_UNLIKELY(   pThis->StdOutBuf.cbBuf > UINT32_MAX
                    || pThis->StdErrBuf.cbBuf > UINT32_MAX))
        return VERR_BUFFER_OVERFLOW;

    pThis->fFinalized = true;
    return VINF_SUCCESS;
}


RTDECL(int) RTFuzzTgtStateAddToRecorder(RTFUZZTGTSTATE hFuzzTgtState)
{
    PRTFUZZTGTSTATEINT pThis = hFuzzTgtState;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);

    if (!pThis->fFinalized)
    {
        int rc = RTFuzzTgtStateFinalize(pThis);
        if (RT_FAILURE(rc))
            return rc;
    }

    PRTFUZZTGTRECINT pTgtRec = pThis->pTgtRec;
    uint64_t uKey = ((uint64_t)pThis->StdOutBuf.cbBuf << 32) | pThis->StdErrBuf.cbBuf;

    /* Try to find a node matching the stdout and sterr sizes first. */
    int rc = RTSemRWRequestRead(pTgtRec->hSemRwStates, RT_INDEFINITE_WAIT); AssertRC(rc);
    PRTFUZZTGTRECNODE pNode = (PRTFUZZTGTRECNODE)RTAvlU64Get(&pTgtRec->TreeStates, uKey);
    if (pNode)
    {
        /* Traverse the states and check if any matches the stdout and stderr buffers exactly. */
        PRTFUZZTGTSTATEINT pIt;
        bool fMatchFound = false;
        RTListForEach(&pNode->LstStates, pIt, RTFUZZTGTSTATEINT, NdStates)
        {
            Assert(   pThis->StdOutBuf.cbBuf == pIt->StdOutBuf.cbBuf
                   && pThis->StdErrBuf.cbBuf == pIt->StdErrBuf.cbBuf);
            if (   (   !pThis->StdOutBuf.cbBuf
                    || !memcmp(pThis->StdOutBuf.pbBase, pIt->StdOutBuf.pbBase, pThis->StdOutBuf.cbBuf))
                && (   !pThis->StdErrBuf.cbBuf
                    || !memcmp(pThis->StdErrBuf.pbBase, pIt->StdErrBuf.pbBase, pThis->StdErrBuf.cbBuf)))
            {
                fMatchFound = true;
                break;
            }
        }

        rc = RTSemRWReleaseRead(pTgtRec->hSemRwStates); AssertRC(rc);
        if (!fMatchFound)
        {
            rc = RTSemRWRequestWrite(pTgtRec->hSemRwStates, RT_INDEFINITE_WAIT); AssertRC(rc);
            RTListAppend(&pNode->LstStates, &pThis->NdStates);
            rc = RTSemRWReleaseWrite(pTgtRec->hSemRwStates); AssertRC(rc);
            pThis->fInRecSet = true;
        }
        else
            rc = VERR_ALREADY_EXISTS;
    }
    else
    {
        rc = RTSemRWReleaseRead(pTgtRec->hSemRwStates); AssertRC(rc);

        /* No node found, create new one and insert in to the tree right away. */
        pNode = (PRTFUZZTGTRECNODE)RTMemAllocZ(sizeof(*pNode));
        if (RT_LIKELY(pNode))
        {
            pNode->Core.Key = uKey;
            RTListInit(&pNode->LstStates);
            RTListAppend(&pNode->LstStates, &pThis->NdStates);
            rc = RTSemRWRequestWrite(pTgtRec->hSemRwStates, RT_INDEFINITE_WAIT); AssertRC(rc);
            bool fIns = RTAvlU64Insert(&pTgtRec->TreeStates, &pNode->Core);
            if (!fIns)
            {
                /* Someone raced us, get the new node and append there. */
                RTMemFree(pNode);
                pNode = (PRTFUZZTGTRECNODE)RTAvlU64Get(&pTgtRec->TreeStates, uKey);
                AssertPtr(pNode);
                RTListAppend(&pNode->LstStates, &pThis->NdStates);
            }
            rc = RTSemRWReleaseWrite(pTgtRec->hSemRwStates); AssertRC(rc);
            pThis->fInRecSet = true;
        }
        else
            rc = VERR_NO_MEMORY;
    }

    return rc;
}


RTDECL(int) RTFuzzTgtStateAppendStdoutFromBuf(RTFUZZTGTSTATE hFuzzTgtState, const void *pvStdOut, size_t cbStdOut)
{
    PRTFUZZTGTSTATEINT pThis = hFuzzTgtState;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(!pThis->fFinalized, VERR_WRONG_ORDER);

    RT_NOREF(pvStdOut, cbStdOut);
    return VERR_NOT_IMPLEMENTED;
}


RTDECL(int) RTFuzzTgtStateAppendStderrFromBuf(RTFUZZTGTSTATE hFuzzTgtState, const void *pvStdErr, size_t cbStdErr)
{
    PRTFUZZTGTSTATEINT pThis = hFuzzTgtState;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(!pThis->fFinalized, VERR_WRONG_ORDER);

    RT_NOREF(pvStdErr, cbStdErr);
    return VERR_NOT_IMPLEMENTED;
}


RTDECL(int) RTFuzzTgtStateAppendStdoutFromPipe(RTFUZZTGTSTATE hFuzzTgtState, RTPIPE hPipe)
{
    PRTFUZZTGTSTATEINT pThis = hFuzzTgtState;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(!pThis->fFinalized, VERR_WRONG_ORDER);

    return rtFuzzTgtStdOutErrBufFillFromPipe(&pThis->StdOutBuf, hPipe);
}


RTDECL(int) RTFuzzTgtStateAppendStderrFromPipe(RTFUZZTGTSTATE hFuzzTgtState, RTPIPE hPipe)
{
    PRTFUZZTGTSTATEINT pThis = hFuzzTgtState;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(!pThis->fFinalized, VERR_WRONG_ORDER);

    return rtFuzzTgtStdOutErrBufFillFromPipe(&pThis->StdErrBuf, hPipe);
}

