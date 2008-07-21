/* $Id$ */
/** @file
 * IPRT Testcase - Handle Tables.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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
#include <iprt/handletable.h>
#include <iprt/stream.h>
#include <iprt/initterm.h>
#include <iprt/err.h>
#include <iprt/getopt.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static unsigned g_cErrors;

static DECLCALLBACK(void) tstHandleTableTest1Delete(RTHANDLETABLE hHandleTable, uint32_t h, void *pvObj, void *pvCtx, void *pvUser)
{
    uint32_t *pcCalls = (uint32_t *)pvUser;
    (*pcCalls)++;
}

static DECLCALLBACK(int) tstHandleTableTest1Retain(RTHANDLETABLE hHandleTable, void *pvObj, void *pvCtx, void *pvUser)
{
    uint32_t *pcCalls = (uint32_t *)pvUser;
    (*pcCalls)++;
    return VINF_SUCCESS;
}

static int tstHandleTableTest1(uint32_t fFlags, uint32_t uBase, uint32_t cMax, uint32_t cDelta, uint32_t cUnitsPerDot, bool fCallbacks)
{
    int rc;
    uint32_t cRetainerCalls = 0;

    RTPrintf("tstHandleTable: TESTING RTHandleTableCreateEx(, 0");
    fFlags |= RTHANDLETABLE_FLAGS_CONTEXT;
    if (fFlags & RTHANDLETABLE_FLAGS_LOCKED)    RTPrintf(" | LOCKED");
    if (fFlags & RTHANDLETABLE_FLAGS_CONTEXT)   RTPrintf(" | CONTEXT");
    RTPrintf(", %#x, %#x,,)...\n", uBase, cMax);

    RTHANDLETABLE hHT;
    rc = RTHandleTableCreateEx(&hHT, fFlags, uBase, cMax,
                               fCallbacks ? tstHandleTableTest1Retain : NULL,
                               fCallbacks ? &cRetainerCalls : NULL);
    if (RT_FAILURE(rc))
    {
        RTPrintf("\ntstHandleTable: FAILURE - RTHandleTableCreateEx failed, %Rrc!\n", rc);
        return 1;
    }

    /* fill it */
    RTPrintf("tstHandleTable: TESTING   RTHandleTableAllocWithCtx.."); RTStrmFlush(g_pStdOut);
    uint32_t i = uBase;
    for (;; i++)
    {
        uint32_t h;
        rc = RTHandleTableAllocWithCtx(hHT, (void *)((uintptr_t)&i + (uintptr_t)i * 4), NULL, &h);
        if (RT_SUCCESS(rc))
        {
            if (h != i)
            {
                RTPrintf("\ntstHandleTable: FAILURE (%d) - h=%d, expected %d!\n", __LINE__, h, i);
                g_cErrors++;
            }
        }
        else if (rc == VERR_NO_MORE_HANDLES)
        {
            if (i < cMax)
            {
                RTPrintf("\ntstHandleTable: FAILURE (%d) - i=%d, expected > 65534!\n", __LINE__, i);
                g_cErrors++;
            }
            break;
        }
        else
        {
            RTPrintf("\ntstHandleTable: FAILURE (%d) - i=%d, rc=%Rrc!\n", __LINE__, i, rc);
            g_cErrors++;
        }
        if (!(i % cUnitsPerDot))
        {
            RTPrintf(".");
            RTStrmFlush(g_pStdOut);
        }
    }
    uint32_t const c = i;
    RTPrintf(" c=%#x\n", c);
    if (fCallbacks && cRetainerCalls != 0)
    {
        RTPrintf("tstHandleTable: FAILURE (%d) - cRetainerCalls=%#x expected 0!\n", __LINE__, i, cRetainerCalls);
        g_cErrors++;
    }

    /* look up all the entries */
    RTPrintf("tstHandleTable: TESTING   RTHandleTableLookupWithCtx.."); RTStrmFlush(g_pStdOut);
    cRetainerCalls = 0;
    for (i = uBase; i < c; i++)
    {
        void *pvExpect = (void *)((uintptr_t)&i + (uintptr_t)i * 4);
        void *pvObj = RTHandleTableLookupWithCtx(hHT, i, NULL);
        if (!pvObj)
        {
            RTPrintf("\ntstHandleTable: FAILURE (%d) - i=%d, RTHandleTableLookupWithCtx failed!\n", __LINE__, i);
            g_cErrors++;
        }
        else if (pvObj != pvExpect)
        {
            RTPrintf("\ntstHandleTable: FAILURE (%d) - i=%d, pvObj=%p expected %p\n", __LINE__, i, pvObj, pvExpect);
            g_cErrors++;
        }
        if (!(i % cUnitsPerDot))
        {
            RTPrintf(".");
            RTStrmFlush(g_pStdOut);
        }
    }
    RTPrintf("\n");
    if (fCallbacks && cRetainerCalls != c - uBase)
    {
        RTPrintf("tstHandleTable: FAILURE (%d) - cRetainerCalls=%#x expected %#x!\n", __LINE__, cRetainerCalls, c - uBase);
        g_cErrors++;
    }

    /* remove all the entries (in order) */
    RTPrintf("tstHandleTable: TESTING   RTHandleTableFreeWithCtx.."); RTStrmFlush(g_pStdOut);
    cRetainerCalls = 0;
    for (i = 1; i < c; i++)
    {
        void *pvExpect = (void *)((uintptr_t)&i + (uintptr_t)i * 4);
        void *pvObj = RTHandleTableFreeWithCtx(hHT, i, NULL);
        if (!pvObj)
        {
            RTPrintf("\ntstHandleTable: FAILURE (%d) - i=%d, RTHandleTableLookupWithCtx failed!\n", __LINE__, i);
            g_cErrors++;
        }
        else if (pvObj != pvExpect)
        {
            RTPrintf("\ntstHandleTable: FAILURE (%d) - i=%d, pvObj=%p expected %p\n", __LINE__, i, pvObj, pvExpect);
            g_cErrors++;
        }
        else if (RTHandleTableLookupWithCtx(hHT, i, NULL))
        {
            RTPrintf("\ntstHandleTable: FAILURE (%d) - i=%d, RTHandleTableLookupWithCtx succeeded after free!\n", __LINE__, i);
            g_cErrors++;
        }
        if (!(i % cUnitsPerDot))
        {
            RTPrintf(".");
            RTStrmFlush(g_pStdOut);
        }
    }
    RTPrintf("\n");
    if (fCallbacks && cRetainerCalls != c - uBase)
    {
        RTPrintf("tstHandleTable: FAILURE (%d) - cRetainerCalls=%#x expected %#x!\n", __LINE__, cRetainerCalls, c - uBase);
        g_cErrors++;
    }

    /* do a mix of alloc, lookup and free where there is a constant of cDelta handles in the table. */
    RTPrintf("tstHandleTable: TESTING   Alloc,Lookup,Free mix [cDelta=%#x]..", cDelta); RTStrmFlush(g_pStdOut);
    for (i = 1; i < c * 2; i++)
    {
        /* alloc */
        uint32_t hExpect = ((i - 1) % (c - 1)) + 1;
        uint32_t h;
        rc = RTHandleTableAllocWithCtx(hHT, (void *)((uintptr_t)&i + (uintptr_t)hExpect * 4), NULL, &h);
        if (RT_FAILURE(rc))
        {
            RTPrintf("\ntstHandleTable: FAILURE (%d) - i=%d, RTHandleTableAllocWithCtx: rc=%Rrc!\n", __LINE__, i, rc);
            g_cErrors++;
        }
        else if (h != hExpect)
        {
            RTPrintf("\ntstHandleTable: FAILURE (%d) - i=%d, RTHandleTableAllocWithCtx: rc=%Rrc!\n", __LINE__, i, rc);
            g_cErrors++;
        }

        if (i > cDelta)
        {
            /* lookup */
            for (uint32_t j = i - cDelta; j <= i; j++)
            {
                uint32_t hLookup = ((j - 1) % (c - 1)) + 1;
                void *pvExpect = (void *)((uintptr_t)&i + (uintptr_t)hLookup * 4);
                void *pvObj = RTHandleTableLookupWithCtx(hHT, hLookup, NULL);
                if (pvObj != pvExpect)
                {
                    RTPrintf("\ntstHandleTable: FAILURE (%d) - i=%d, j=%d, RTHandleTableLookupWithCtx(,%u,): pvObj=%p expected %p!\n",
                             __LINE__, i, j, hLookup, pvObj, pvExpect);
                    g_cErrors++;
                }
                else if (RTHandleTableLookupWithCtx(hHT, hLookup, &i))
                {
                    RTPrintf("\ntstHandleTable: FAILURE (%d) - i=%d, j=%d, RTHandleTableLookupWithCtx: succeeded with bad context\n",
                             __LINE__, i, j, pvObj, pvExpect);
                    g_cErrors++;
                }
            }

            /* free */
            uint32_t hFree = ((i - 1 - cDelta) % (c - 1)) + 1;
            void *pvExpect = (void *)((uintptr_t)&i + (uintptr_t)hFree * 4);
            void *pvObj = RTHandleTableFreeWithCtx(hHT, hFree, NULL);
            if (pvObj != pvExpect)
            {
                RTPrintf("\ntstHandleTable: FAILURE (%d) - i=%d, RTHandleTableFreeWithCtx: pvObj=%p expected %p!\n",
                         __LINE__, i, pvObj, pvExpect);
                g_cErrors++;
            }
            else if (   RTHandleTableLookupWithCtx(hHT, hFree, NULL)
                     || RTHandleTableFreeWithCtx(hHT, hFree, NULL))
            {
                RTPrintf("\ntstHandleTable: FAILURE (%d) - i=%d, RTHandleTableLookup/FreeWithCtx: succeeded after free\n",
                         __LINE__, i);
                g_cErrors++;
            }
        }
        if (!(i % (cUnitsPerDot * 2)))
        {
            RTPrintf(".");
            RTStrmFlush(g_pStdOut);
        }
    }
    RTPrintf("\n");

    /* finally, destroy the table (note that there are 128 entries in it). */
    cRetainerCalls = 0;
    uint32_t cDeleteCalls = 0;
    rc = RTHandleTableDestroy(hHT,
                              fCallbacks ? tstHandleTableTest1Delete : NULL,
                              fCallbacks ? &cDeleteCalls : NULL);
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstHandleTable: FAILURE (%d) - RTHandleTableDestroy failed, %Rrc!\n", __LINE__, rc);
        g_cErrors++;
    }

    return 0;
}

int main(int argc, char **argv)
{
    /*
     * Init the runtime and parse the arguments.
     */
    RTR3Init(false, 0);

    static RTOPTIONDEF const s_aOptions[] =
    {
        { "--base",         'b', RTGETOPT_REQ_UINT32 },
        { "--max",          'm', RTGETOPT_REQ_UINT32 },
    };

    uint32_t uBase = 0;
    uint32_t cMax = 0;

    int ch;
    int iArg = 1;
    RTOPTIONUNION Value;
    while ((ch = RTGetOpt(argc,argv, &s_aOptions[0], RT_ELEMENTS(s_aOptions), &iArg, &Value)))
        switch (ch)
        {
            case 'b':
                uBase = Value.u32;
                break;

            case 'm':
                cMax = Value.u32;
                break;

            case '?':
            case 'h':
                RTPrintf("syntax: tstIntNet-1 [-pSt] [-d <secs>] [-f <file>] [-r <size>] [-s <size>]\n");
                return 1;

            default:
                if (RT_SUCCESS(ch))
                    RTPrintf("tstHandleTable: invalid argument (%#x): %s\n", ch, Value.psz);
                else
                    RTPrintf("tstHandleTable: invalid argument: %Rrc - \n", ch, Value.pDef->pszLong);
                return 1;
        }
    if (iArg < argc)
    {
        RTPrintf("tstHandleTable: invalid argument: %s\n", argv[iArg]);
        return 1;
    }

    /*
     * Do a simple warmup / smoke test first.
     */
    /* these two are for the default case. */
    tstHandleTableTest1(0,                              1,       65534,  128,           2048, false);
    tstHandleTableTest1(RTHANDLETABLE_FLAGS_LOCKED,     1,       65534,   63,           2048, false);
    /* Test that the retain and delete functions work. */
    tstHandleTableTest1(RTHANDLETABLE_FLAGS_LOCKED,     1,       1024,   256,            256,  true);
    /* For testing 1st level expansion / reallocation. */
    tstHandleTableTest1(0,                              1, 1024*1024*8,   3,          150000, false);

    /*
     * Threaded tests.
     */


    /*
     * Summary.
     */
    if (!g_cErrors)
        RTPrintf("tstHandleTable: SUCCESS\n");
    else
        RTPrintf("tstHandleTable: FAILURE - %d errors\n", g_cErrors);

    return !!g_cErrors;
}
