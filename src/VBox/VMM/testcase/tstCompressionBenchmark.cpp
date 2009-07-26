/* $Id$ */
/** @file
 * Compression Benchmark for SSM.
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/mem.h>
#include <iprt/param.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/time.h>
#include <iprt/zip.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static size_t   g_cPages = 20*_1M / PAGE_SIZE;
static uint8_t *g_pabSrc;
static uint8_t *g_pabDecompr;
static size_t   g_cbCompr;
static uint8_t *g_pabCompr;

static DECLCALLBACK(int) ComprOutCallback(void *pvUser, const void *pvBuf, size_t cbBuf)
{
    return VERR_NOT_IMPLEMENTED;
}


/** Prints an error message and returns 1 for quick return from main use. */
static int Error(const char *pszMsgFmt, ...)
{
    RTStrmPrintf(g_pStdErr, "error: ");
    va_list va;
    va_start(va, pszMsgFmt);
    RTStrmPrintfV(g_pStdErr, pszMsgFmt, va);
    va_end(va);
    return 1;
}


int main(int argc, char **argv)
{
    RTR3Init();

    /*
     * Parse arguments.
     */
    static const RTGETOPTDEF    s_aOptions[] =
    {
        { "--interations",    'i', RTGETOPT_REQ_UINT32 },
        { "--num-pages",      'n', RTGETOPT_REQ_UINT32 },
        { "--page-file",      'f', RTGETOPT_REQ_STRING },
    };

    const char     *pszPageFile = NULL;
    uint32_t        cIterations = 1;
    RTGETOPTUNION   Val;
    RTGETOPTSTATE   State;
    int rc = RTGetOptInit(&State, argc, argv, &s_aOptions[0], RT_ELEMENTS(s_aOptions), 1, 0);
    AssertRCReturn(rc, 1);

    while ((rc = RTGetOpt(&State, &Val)))
    {
        switch (rc)
        {
            case 'n':
                g_cPages = Val.u32;
                if (g_cPages * PAGE_SHIFT * 8 / (PAGE_SIZE * 8) != g_cPages)
                    return Error("The specified page count is too high: %#x\n", g_cPages);
                if (g_cPages < 1)
                    return Error("The specified page count is too low: %#x\n", g_cPages);
                break;

            case 'i':
                cIterations = Val.u32;
                if (cIterations < 1)
                    return Error("The number of iterations must be 1 or higher\n");
                break;

            case 'f':
                pszPageFile = Val.psz;
                break;

            default:
                if (rc > 0)
                {
                    if (RT_C_IS_GRAPH(rc))
                        Error("unhandled option: -%c\n", rc);
                    else
                        Error("unhandled option: %d\n", rc);
                }
                else if (rc == VERR_GETOPT_UNKNOWN_OPTION)
                    Error("unknown option: %s\n", Val.psz);
                else if (rc == VINF_GETOPT_NOT_OPTION)
                    Error("unknown argument: %s\n", Val.psz);
                else if (Val.pDef)
                    Error("%s: %Rrs\n", Val.pDef->pszLong, rc);
                else
                    Error("%Rrs\n", rc);
                return 1;
        }
    }

    /*
     * Gather the test memory.
     */
    if (pszPageFile)
    {
        size_t cbFile;
        rc = RTFileReadAllEx(pszPageFile, 0, g_cPages * PAGE_SIZE, RTFILE_RDALL_O_DENY_NONE, (void **)&g_pabSrc, &cbFile);
        if (RT_FAILURE(rc))
            return Error("Error reading %zu bytes from %s: %Rrc\n", g_cPages * PAGE_SIZE, pszPageFile, rc);
        if (cbFile != g_cPages * PAGE_SIZE)
            return Error("Error reading %zu bytes from %s: got %zu bytes\n", g_cPages * PAGE_SIZE, pszPageFile, cbFile);
    }
    else
    {
        g_pabSrc = (uint8_t *)RTMemAlloc(g_cPages * PAGE_SIZE);
        if (g_pabSrc)
        {
            /* just fill it with something. */
            uint8_t *pb    = g_pabSrc;
            uint8_t *pbEnd = &g_pabSrc[g_cPages * PAGE_SIZE];
            for (; pb != pbEnd; pb += 16)
            {
                char szTmp[17];
                RTStrPrintf(szTmp, sizeof(szTmp), "aaaa%08Xzzzz", (uint32_t)(uintptr_t)pb);
                memcpy(pb, szTmp, 16);
            }
        }
    }

    g_pabDecompr = (uint8_t *)RTMemAlloc(g_cPages * PAGE_SIZE);
    g_cbCompr    = g_cPages * PAGE_SIZE * 2;
    g_pabCompr   = (uint8_t *)RTMemAlloc(g_cbCompr);
    if (!g_pabSrc || !g_pabDecompr || !g_pabCompr)
        return Error("failed to allocate memory buffers (g_cPages=%#x)\n", g_cPages);

    /*
     * Double loop compressing and uncompressing the data, where the outer does
     * the specified number of interations while the inner applies the different
     * compression algorithms.
     */
    struct
    {
        /** The time spent decompressing. */
        uint64_t    cNanoDecompr;
        /** The time spent compressing. */
        uint64_t    cNanoCompr;
        /** The size of the compressed data. */
        uint64_t    cbCompr;
        /** Number of errrors. */
        uint32_t    cErrors;
        /** Compresstion type.  */
        RTZIPTYPE   enmType;
        /** Compresison level.  */
        RTZIPLEVEL  enmLevel;
        /** Method name. */
        const char *pszName;
    } aTests[] =
    {
        { 0, 0, 0, 0, RTZIPTYPE_LZF, RTZIPLEVEL_DEFAULT, "RTZip/LZF" }
    };
    for (uint32_t i = 0; i < cIterations; i++)
    {
        for (uint32_t j = 0; j < RT_ELEMENTS(aTests); j++)
        {
            memset(g_pabCompr,   0, g_cbCompr);
            memset(g_pabDecompr, 0, g_cPages * PAGE_SIZE);

            /*
             * Compress it.
             */
            PRTZIPCOMP pZip;
            rc = RTZipCompCreate(&pZip, NULL, ComprOutCallback, aTests[j].enmType, aTests[j].enmLevel);
            if (RT_FAILURE(rc))
            {
                Error("Failed to create compressor for '%s' (#%u): %Rrc\n", aTests[j].pszName, j, rc);
                aTests[j].cErrors++;
                continue;
            }

            uint8_t const  *pbPage = &g_pabSrc[0];
            size_t          cLeft  = g_cPages;
            uint64_t        NanoTS = RTTimeNanoTS();
            while (cLeft-- > 0)
            {
                pbPage += PAGE_SIZE;
            }
            NanoTS = RTTimeNanoTS() - NanoTS;

        }
    }


    /*
     * Report the results.
     */
    RTPrintf("tstCompressionBenchmark: BEGIN RESULTS\n");
    rc = 0;
    for (uint32_t j = 0; j < RT_ELEMENTS(aTests); j++)
    {
        RTPrintf("\n");

        if (aTests[j].cErrors)
            rc = 1;
    }
    RTPrintf("tstCompressionBenchmark: END RESULTS\n");

    return rc;
}

