/* $Id$ */
/** @file
 * IPRT - Testcase Framework.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/test.h>
#include <iprt/mem.h>
#include <iprt/param.h>
#include <iprt/string.h>
#include <iprt/stream.h>
#include <iprt/critsect.h>
#include <iprt/once.h>
#include <iprt/err.h>
#include <iprt/asm.h>

#include "internal/magics.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Guarded memory allocation record.
 */
typedef struct RTTESTGUARDEDMEM
{
    /** Pointer to the next record. */
    struct RTTESTGUARDEDMEM *pNext;
    /** The address we return to the user. */
    void           *pvUser;
    /** The base address of the allocation. */
    void           *pvAlloc;
    /** The size of the allocation. */
    size_t          cbAlloc;
    /** Guards. */
    struct
    {
        /** The guard address. */
        void       *pv;
        /** The guard size. */
        size_t      cb;
    }               aGuards[2];
} RTTESTGUARDEDMEM;
/** Pointer to an guarded memory allocation. */
typedef RTTESTGUARDEDMEM *PRTTESTGUARDEDMEM;

/**
 * Test instance structure.
 */
typedef struct RTTESTINT
{
    /** Magic. */
    uint32_t            u32Magic;
    /** The number of errors. */
    volatile uint32_t   cErrors;
    /** The test name. */
    const char         *pszTest;
    /** The length of the test name.  */
    unsigned            cchTest;
    /** The size of a guard. Multiple of PAGE_SIZE. */
    uint32_t            cbGuard;

    /** Critical section seralizing access to the members following it. */
    RTCRITSECT          Lock;
    /** The list of guarded memory allocations. */
    PRTTESTGUARDEDMEM   pGuardedMem;

    /** Critical section seralizing output. */
    RTCRITSECT          OutputLock;
    /** The output stream. */
    PRTSTREAM           pOutStrm;
    /** Whether we're currently at a newline. */
    bool                fNewLine;

} RTTESTINT;
/** Pointer to a test instance. */
typedef RTTESTINT *PRTTESTINT;


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Validate a test instance. */
#define RTTEST_VALID_RETURN(pTest)  \
    do { \
        AssertPtrReturn(pTest, VERR_INVALID_HANDLE); \
        AssertReturn(pTest->u32Magic == RTTESTINT_MAGIC, VERR_INVALID_HANDLE); \
    } while (0)

/** Gets and validates a test instance.
 * If the handle is nil, we will try retrive it from the test TLS entry.
 */
#define RTTEST_GET_VALID_RETURN(pTest)  \
    do { \
        if (pTest == NIL_RTTEST) \
            pTest = (PRTTESTINT)RTTlsGet(g_iTestTls); \
        AssertPtrReturn(pTest, VERR_INVALID_HANDLE); \
        AssertReturn(pTest->u32Magic == RTTESTINT_MAGIC, VERR_INVALID_MAGIC); \
    } while (0)


/** Gets and validates a test instance.
 * If the handle is nil, we will try retrive it from the test TLS entry.
 */
#define RTTEST_GET_VALID_RETURN_RC(pTest, rc)  \
    do { \
        if (pTest == NIL_RTTEST) \
            pTest = (PRTTESTINT)RTTlsGet(g_iTestTls); \
        AssertPtrReturn(pTest, (rc)); \
        AssertReturn(pTest->u32Magic == RTTESTINT_MAGIC, (rc)); \
    } while (0)


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static void rtTestGuardedFreeOne(PRTTESTGUARDEDMEM pMem);


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** For serializing TLS init. */
static RTONCE   g_TestInitOnce = RTONCE_INITIALIZER;
/** Our TLS entry. */
static RTTLS    g_iTestTls = NIL_RTTLS;



/**
 * Init TLS index once.
 *
 * @returns IPRT status code.
 * @param   pvUser1     Ignored.
 * @param   pvUser2     Ignored.
 */
static DECLCALLBACK(int32_t) rtTestInitOnce(void *pvUser1, void *pvUser2)
{
    NOREF(pvUser1);
    NOREF(pvUser2);
    return RTTlsAllocEx(&g_iTestTls, NULL);
}



/**
 * Creates a test instance.
 *
 * @returns IPRT status code.
 * @param   pszTest     The test name.
 * @param   phTest      Where to store the test instance handle.
 */
RTR3DECL(int) RTTestCreate(const char *pszTest, PRTTEST phTest)
{
    /*
     * Global init.
     */
    int rc = RTOnce(&g_TestInitOnce, rtTestInitOnce, NULL, NULL);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Create the instance.
     */
    PRTTESTINT pTest = (PRTTESTINT)RTMemAllocZ(sizeof(*pTest));
    if (!pTest)
        return VERR_NO_MEMORY;
    pTest->u32Magic = RTTESTINT_MAGIC;
    pTest->cbGuard  = PAGE_SIZE * 7;
    pTest->pszTest  = RTStrDup(pszTest);
    pTest->cchTest  = (unsigned)strlen(pszTest);
    pTest->pOutStrm = g_pStdOut;
    pTest->fNewLine = true;

    rc = RTCritSectInit(&pTest->Lock);
    if (RT_SUCCESS(rc))
    {
        rc = RTCritSectInit(&pTest->OutputLock);
        if (RT_SUCCESS(rc))
        {

            /*
             * Associate it with our TLS entry unless there is already
             * an instance there.
             */
            if (!RTTlsGet(g_iTestTls))
                rc = RTTlsSet(g_iTestTls, pTest);
            if (RT_SUCCESS(rc))
            {
                *phTest = pTest;
                return VINF_SUCCESS;
            }

            /* bail out. */
            RTCritSectDelete(&pTest->OutputLock);
        }
        RTCritSectDelete(&pTest->Lock);
    }
    pTest->u32Magic = 0;
    RTStrFree((char *)pTest->pszTest);
    RTMemFree(pTest);
    return rc;
}


/**
 * Destroys a test instance previously created by RTTestCreate.
 *
 * @returns IPRT status code.
 * @param   hTest       The test handle. NIL_RTTEST is ignored.
 */
RTR3DECL(int) RTTestDestroy(RTTEST hTest)
{
    /*
     * Validate
     */
    if (hTest == NIL_RTTEST)
        return VINF_SUCCESS;
    RTTESTINT *pTest = hTest;
    RTTEST_VALID_RETURN(pTest);

    /*
     * Clean up.
     */
    if ((RTTESTINT *)RTTlsGet(g_iTestTls) == pTest)
        RTTlsSet(g_iTestTls, NULL);

    ASMAtomicWriteU32(&pTest->u32Magic, ~RTTESTINT_MAGIC);
    RTCritSectDelete(&pTest->Lock);
    RTCritSectDelete(&pTest->OutputLock);

    /* free guarded memory. */
    PRTTESTGUARDEDMEM pMem = pTest->pGuardedMem;
    pTest->pGuardedMem = NULL;
    while (pMem)
    {
        PRTTESTGUARDEDMEM pFree = pMem;
        pMem = pMem->pNext;
        rtTestGuardedFreeOne(pFree);
    }

    RTStrFree((char *)pTest->pszTest);
    pTest->pszTest = NULL;
    RTMemFree(pTest);
    return VINF_SUCCESS;
}



/**
 * Allocate a block of guarded memory.
 *
 * @returns IPRT status code.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   cb          The amount of memory to allocate.
 * @param   cbAlign     The alignment of the returned block.
 * @param   fHead       Head or tail optimized guard.
 * @param   ppvUser     Where to return the pointer to the block.
 */
RTR3DECL(int) RTTestGuardedAlloc(RTTEST hTest, size_t cb, uint32_t cbAlign, bool fHead, void **ppvUser)
{
    PRTTESTINT pTest = hTest;
    RTTEST_GET_VALID_RETURN(pTest);
    if (cbAlign == 0)
        cbAlign = 1;
    AssertReturn(cbAlign <= PAGE_SIZE, VERR_INVALID_PARAMETER);
    AssertReturn(cbAlign == (UINT32_C(1) << (ASMBitFirstSetU32(cbAlign) - 1)), VERR_INVALID_PARAMETER);

    /*
     * Allocate the record and block and initialize them.
     */
    int                 rc = VERR_NO_MEMORY;
    PRTTESTGUARDEDMEM   pMem = (PRTTESTGUARDEDMEM)RTMemAlloc(sizeof(*pMem));
    if (RT_LIKELY(pMem))
    {
        size_t const    cbAligned = RT_ALIGN_Z(cb, PAGE_SIZE);
        pMem->aGuards[0].cb = pMem->aGuards[1].cb = pTest->cbGuard;
        pMem->cbAlloc       = pMem->aGuards[0].cb + pMem->aGuards[1].cb + cbAligned;
        pMem->pvAlloc       = RTMemPageAlloc(pMem->cbAlloc);
        if (pMem->pvAlloc)
        {
            pMem->aGuards[0].pv = pMem->pvAlloc;
            pMem->pvUser        = (uint8_t *)pMem->pvAlloc + pMem->aGuards[0].cb;
            pMem->aGuards[1].pv = (uint8_t *)pMem->pvUser + cbAligned;
            if (!fHead)
            {
                size_t off = cb & PAGE_OFFSET_MASK;
                if (off)
                {
                    off = PAGE_SIZE - RT_ALIGN_Z(off, cbAlign);
                    pMem->pvUser = (uint8_t *)pMem->pvUser + off;
                }
            }

            /*
             * Set up the guards and link the record.
             */
            ASMMemFill32(pMem->aGuards[0].pv, pMem->aGuards[0].cb, 0xdeadbeef);
            ASMMemFill32(pMem->aGuards[1].pv, pMem->aGuards[1].cb, 0xdeadbeef);
            rc = RTMemProtect(pMem->aGuards[0].pv, pMem->aGuards[0].cb, RTMEM_PROT_NONE);
            if (RT_SUCCESS(rc))
            {
                rc = RTMemProtect(pMem->aGuards[1].pv, pMem->aGuards[1].cb, RTMEM_PROT_NONE);
                if (RT_SUCCESS(rc))
                {
                    *ppvUser = pMem->pvUser;

                    RTCritSectEnter(&pTest->Lock);
                    pMem->pNext = pTest->pGuardedMem;
                    pTest->pGuardedMem = pMem;
                    RTCritSectLeave(&pTest->Lock);

                    return VINF_SUCCESS;
                }

                RTMemProtect(pMem->aGuards[0].pv, pMem->aGuards[0].cb, RTMEM_PROT_WRITE | RTMEM_PROT_READ);
            }

            RTMemPageFree(pMem->pvAlloc);
        }
        RTMemFree(pMem);
    }
    return rc;
}


/**
 * Allocates a block of guarded memory where the guarded is immediately after
 * the user memory.
 *
 * @returns Pointer to the allocated memory. NULL on failure.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   cb          The amount of memory to allocate.
 */
RTR3DECL(void *) RTTestGuardedAllocTail(RTTEST hTest, size_t cb)
{
    void *pvUser;
    int rc = RTTestGuardedAlloc(hTest, cb, 1, false /*fHead*/, &pvUser);
    if (RT_SUCCESS(rc))
        return pvUser;
    return NULL;
}


/**
 * Allocates a block of guarded memory where the guarded is right in front of
 * the user memory.
 *
 * @returns Pointer to the allocated memory. NULL on failure.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   cb          The amount of memory to allocate.
 */
RTR3DECL(void *) RTTestGuardedAllocHead(RTTEST hTest, size_t cb)
{
    void *pvUser;
    int rc = RTTestGuardedAlloc(hTest, cb, 1, true /*fHead*/, &pvUser);
    if (RT_SUCCESS(rc))
        return pvUser;
    return NULL;
}


/**
 * Frees one block of guarded memory.
 *
 * The caller is responsible for unlinking it.
 *
 * @param   pMem        The memory record.
 */
static void rtTestGuardedFreeOne(PRTTESTGUARDEDMEM pMem)
{
    int rc;
    rc = RTMemProtect(pMem->aGuards[0].pv, pMem->aGuards[0].cb, RTMEM_PROT_WRITE | RTMEM_PROT_READ); AssertRC(rc);
    rc = RTMemProtect(pMem->aGuards[1].pv, pMem->aGuards[1].cb, RTMEM_PROT_WRITE | RTMEM_PROT_READ); AssertRC(rc);
    RTMemPageFree(pMem->pvAlloc);
    RTMemFree(pMem);
}


/**
 * Frees a block of guarded memory.
 *
 * @returns IPRT status code.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   pv          The memory. NULL is ignored.
 */
RTR3DECL(int) RTTestGuardedFree(RTTEST hTest, void *pv)
{
    PRTTESTINT pTest = hTest;
    RTTEST_GET_VALID_RETURN(pTest);
    if (!pv)
        return VINF_SUCCESS;

    /*
     * Find it.
     */
    int                 rc = VERR_INVALID_POINTER;
    PRTTESTGUARDEDMEM   pPrev = NULL;

    RTCritSectEnter(&pTest->Lock);
    for (PRTTESTGUARDEDMEM pMem = pTest->pGuardedMem; pMem; pMem = pMem->pNext)
    {
        if (pMem->pvUser == pv)
        {
            if (pPrev)
                pPrev->pNext = pMem->pNext;
            else
                pTest->pGuardedMem = pMem->pNext;
            rtTestGuardedFreeOne(pMem);
            rc = VINF_SUCCESS;
            break;
        }
        pPrev = pMem;
    }
    RTCritSectLeave(&pTest->Lock);

    return VINF_SUCCESS;
}


/**
 * Output callback.
 *
 * @returns number of bytes written.
 * @param   pvArg       User argument.
 * @param   pachChars   Pointer to an array of utf-8 characters.
 * @param   cbChars     Number of bytes in the character array pointed to by pachChars.
 */
static DECLCALLBACK(size_t) rtTestPrintfOutput(void *pvArg, const char *pachChars, size_t cbChars)
{
    size_t      cch   = 0;
    PRTTESTINT  pTest = (PRTTESTINT)pvArg;
    if (cbChars)
    {
        do
        {
            /* insert prefix if at a newline. */
            if (pTest->fNewLine)
            {
                RTStrmWrite(pTest->pOutStrm, pTest->pszTest, pTest->cchTest);
                RTStrmWrite(pTest->pOutStrm, ": ", 2);
                cch += 2 + pTest->cchTest;
            }

            /* look for newline and write the stuff. */
            const char *pchEnd = (const char *)memchr(pachChars, '\n', cbChars);
            if (!pchEnd)
            {
                pTest->fNewLine = false;
                RTStrmWrite(pTest->pOutStrm, pachChars, cbChars);
                cch += cbChars;
                break;
            }

            pTest->fNewLine = true;
            size_t const cchPart = pchEnd - pachChars + 1;
            RTStrmWrite(pTest->pOutStrm, pachChars, cchPart);
            cch       += cchPart;
            pachChars += cchPart;
            cbChars   -= cchPart;
        } while (cbChars);
    }
    else
        RTStrmFlush(pTest->pOutStrm);
    return cch;
}


/**
 * Internal output worker.
 *
 * Caller takes the lock.
 *
 * @returns Number of chars printed.
 * @param   pTest           The test instance.
 * @param   pszFormat       The message.
 * @param   va              The arguments.
 */
static int rtTestPrintfV(PRTTESTINT pTest, const char *pszFormat, va_list va)
{
    return (int)RTStrFormatV(rtTestPrintfOutput, pTest, NULL, NULL, pszFormat, va);
}


/**
 * Internal output worker.
 *
 * Caller takes the lock.
 *
 * @returns Number of chars printed.
 * @param   pTest           The test instance.
 * @param   pszFormat       The message.
 * @param   ...             The arguments.
 */
static int rtTestPrintf(PRTTESTINT pTest, const char *pszFormat, ...)
{
    va_list va;

    va_start(va, pszFormat);
    int cch = rtTestPrintfV(pTest, pszFormat, va);
    va_end(va);

    return cch;
}


/**
 * Test vprintf making sure the output starts on a new line.
 *
 * @returns Number of chars printed.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   pszFormat   The message.
 * @param   va          Arguments.
 */
RTR3DECL(int) RTTestPrintfNlV(RTTEST hTest, const char *pszFormat, va_list va)
{
    PRTTESTINT pTest = hTest;
    RTTEST_GET_VALID_RETURN_RC(pTest, -1);

    RTCritSectEnter(&pTest->OutputLock);

    int cch = 0;
    if (!pTest->fNewLine)
        cch += rtTestPrintf(pTest, "\n");
    cch += rtTestPrintfV(pTest, pszFormat, va);

    RTCritSectLeave(&pTest->OutputLock);

    return cch;
}


/**
 * Test printf making sure the output starts on a new line.
 *
 * @returns Number of chars printed.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   pszFormat   The message.
 * @param   ...         Arguments.
 */
RTR3DECL(int) RTTestPrintfNl(RTTEST hTest, const char *pszFormat, ...)
{
    va_list va;

    va_start(va, pszFormat);
    int cch = RTTestPrintfNlV(hTest, pszFormat, va);
    va_end(va);

    return cch;
}


/**
 * Test vprintf, makes sure lines are prefixed and so forth.
 *
 * @returns Number of chars printed.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   pszFormat   The message.
 * @param   va          Arguments.
 */
RTR3DECL(int) RTTestPrintfV(RTTEST hTest, const char *pszFormat, va_list va)
{
    PRTTESTINT pTest = hTest;
    RTTEST_GET_VALID_RETURN_RC(pTest, -1);

    RTCritSectEnter(&pTest->OutputLock);
    int cch = rtTestPrintfV(pTest, pszFormat, va);
    RTCritSectLeave(&pTest->OutputLock);

    return cch;
}


/**
 * Test printf, makes sure lines are prefixed and so forth.
 *
 * @returns Number of chars printed.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   pszFormat   The message.
 * @param   ...         Arguments.
 */
RTR3DECL(int) RTTestPrintf(RTTEST hTest, const char *pszFormat, ...)
{
    va_list va;

    va_start(va, pszFormat);
    int cch = RTTestPrintfV(hTest, pszFormat, va);
    va_end(va);

    return cch;
}


/**
 * Prints the test banner.
 *
 * @returns Number of chars printed.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 */
RTR3DECL(int) RTTestBanner(RTTEST hTest)
{
    return RTTestPrintfNl(hTest, "TESTING...\n");
}


/**
 * Summaries the test, destroys the test instance and return an exit code.
 *
 * @returns Test program exit code.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 */
RTR3DECL(int) RTTestSummaryAndDestroy(RTTEST hTest)
{
    PRTTESTINT pTest = hTest;
    RTTEST_GET_VALID_RETURN_RC(pTest, 2);

    int rc;
    if (!pTest->cErrors)
    {
        RTTestPrintfNl(hTest, "SUCCESS\n", pTest->cErrors);
        rc = 0;
    }
    else
    {
        RTTestPrintfNl(hTest, "FAILURE - %u errors\n", pTest->cErrors);
        rc = 1;
    }

    RTTestDestroy(pTest);
    return rc;
}


/**
 * Increments the error counter.
 *
 * @returns IPRT status code.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 */
RTR3DECL(int) RTTestErrorInc(RTTEST hTest)
{
    PRTTESTINT pTest = hTest;
    RTTEST_GET_VALID_RETURN(pTest);
    ASMAtomicIncU32(&pTest->cErrors);
    return VINF_SUCCESS;
}


/**
 * Increments the error counter and prints a failure message.
 *
 * @returns IPRT status code.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   pszFormat   The message. No trailing newline.
 * @param   va          The arguments.
 */
RTR3DECL(int) RTTestFailedV(RTTEST hTest, const char *pszFormat, va_list va)
{
    PRTTESTINT pTest = hTest;
    RTTEST_GET_VALID_RETURN_RC(pTest, -1);

    va_list va2;
    va_copy(va2, va);

    RTCritSectEnter(&pTest->OutputLock);
    ASMAtomicIncU32(&pTest->cErrors);

    int cch = rtTestPrintf(pTest, "FAILED - %N\n", pszFormat, &va2);

    RTCritSectLeave(&pTest->OutputLock);

    va_end(va2);

    return cch;
}


/**
 * Increments the error counter and prints a failure message.
 *
 * @returns IPRT status code.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   pszFormat   The message. No trailing newline.
 * @param   ...         The arguments.
 */
RTR3DECL(int) RTTestFailed(RTTEST hTest, const char *pszFormat, ...)
{
    va_list va;

    va_start(va, pszFormat);
    int cch = RTTestFailedV(hTest, pszFormat, va);
    va_end(va);

    return cch;
}

