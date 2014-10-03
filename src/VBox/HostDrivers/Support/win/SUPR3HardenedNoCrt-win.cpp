/* $Id$ */
/** @file
 * VirtualBox Support Library - Hardened main(), windows bits.
 */

/*
 * Copyright (C) 2006-2014 Oracle Corporation
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/nt/nt-and-windows.h>
#include <AccCtrl.h>
#include <AclApi.h>
#ifndef PROCESS_SET_LIMITED_INFORMATION
# define PROCESS_SET_LIMITED_INFORMATION 0x2000
#endif

#include <VBox/sup.h>
#include <VBox/err.h>
#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/string.h>
#include <iprt/initterm.h>
#include <iprt/param.h>
#include <iprt/mem.h>

#include "SUPLibInternal.h"
#include "win/SUPHardenedVerify-win.h"


/*
 * assert.cpp
 */

RTDATADECL(char)                     g_szRTAssertMsg1[1024];
RTDATADECL(char)                     g_szRTAssertMsg2[4096];
RTDATADECL(const char * volatile)    g_pszRTAssertExpr;
RTDATADECL(const char * volatile)    g_pszRTAssertFile;
RTDATADECL(uint32_t volatile)        g_u32RTAssertLine;
RTDATADECL(const char * volatile)    g_pszRTAssertFunction;

RTDECL(bool) RTAssertMayPanic(void)
{
    return true;
}


RTDECL(void) RTAssertMsg1(const char *pszExpr, unsigned uLine, const char *pszFile, const char *pszFunction)
{
    /*
     * Fill in the globals.
     */
    g_pszRTAssertExpr       = pszExpr;
    g_pszRTAssertFile       = pszFile;
    g_pszRTAssertFunction   = pszFunction;
    g_u32RTAssertLine       = uLine;
    RTStrPrintf(g_szRTAssertMsg1, sizeof(g_szRTAssertMsg1),
                "\n!!Assertion Failed!!\n"
                "Expression: %s\n"
                "Location  : %s(%d) %s\n",
                pszExpr, pszFile, uLine, pszFunction);
}


RTDECL(void) RTAssertMsg2V(const char *pszFormat, va_list va)
{
    RTStrPrintfV(g_szRTAssertMsg2, sizeof(g_szRTAssertMsg2), pszFormat, va);
    if (g_enmSupR3HardenedMainState < SUPR3HARDENEDMAINSTATE_CALLED_TRUSTED_MAIN)
        supR3HardenedFatalMsg(g_pszRTAssertExpr, kSupInitOp_Misc, VERR_INTERNAL_ERROR,
                              "%s%s", g_szRTAssertMsg1,  g_szRTAssertMsg2);
    else
        supR3HardenedError(VERR_INTERNAL_ERROR, false/*fFatal*/, "%s%s", g_szRTAssertMsg1,  g_szRTAssertMsg2);
}


/*
 * Memory allocator.
 */

/** The heap we're using. */
static HANDLE g_hSupR3HardenedHeap = NULL;


/**
 * Lazy heap initialization function.
 *
 * @returns Heap handle.
 */
static HANDLE supR3HardenedHeapInit(void)
{
    HANDLE hHeap = RtlCreateHeap(HEAP_GROWABLE | HEAP_CLASS_PRIVATE, NULL /*HeapBase*/,
                                 0 /*ReserveSize*/, 0 /*CommitSize*/,  NULL /*Lock*/, NULL /*Parameters*/);
    if (hHeap)
    {
        g_hSupR3HardenedHeap = hHeap;
        return hHeap;
    }

    supR3HardenedFatal("RtlCreateHeap failed.\n");
    return NULL;
}


/**
 * Compacts the heaps before enter wait for parent/child.
 */
DECLHIDDEN(void) supR3HardenedWinCompactHeaps(void)
{
    if (g_hSupR3HardenedHeap)
        RtlCompactHeap(g_hSupR3HardenedHeap, 0 /*dwFlags*/);
    RtlCompactHeap(GetProcessHeap(), 0 /*dwFlags*/);
}



RTDECL(void *) RTMemTmpAllocTag(size_t cb, const char *pszTag) RT_NO_THROW
{
    return RTMemAllocTag(cb, pszTag);
}


RTDECL(void *) RTMemTmpAllocZTag(size_t cb, const char *pszTag) RT_NO_THROW
{
    return RTMemAllocZTag(cb, pszTag);
}


RTDECL(void) RTMemTmpFree(void *pv) RT_NO_THROW
{
    RTMemFree(pv);
}


RTDECL(void *) RTMemAllocTag(size_t cb, const char *pszTag) RT_NO_THROW
{
    HANDLE hHeap = g_hSupR3HardenedHeap;
    if (!hHeap)
        hHeap = supR3HardenedHeapInit();
    void *pv = RtlAllocateHeap(hHeap, 0 /*fFlags*/, cb);
    if (!pv)
        supR3HardenedFatal("RtlAllocateHeap failed to allocate %zu bytes.\n", cb);
    return pv;
}


RTDECL(void *) RTMemAllocZTag(size_t cb, const char *pszTag) RT_NO_THROW
{
    HANDLE hHeap = g_hSupR3HardenedHeap;
    if (!hHeap)
        hHeap = supR3HardenedHeapInit();
    void *pv = RtlAllocateHeap(hHeap, HEAP_ZERO_MEMORY, cb);
    if (!pv)
        supR3HardenedFatal("RtlAllocateHeap failed to allocate %zu bytes.\n", cb);
    return pv;
}


RTDECL(void *) RTMemAllocVarTag(size_t cbUnaligned, const char *pszTag) RT_NO_THROW
{
    size_t cbAligned;
    if (cbUnaligned >= 16)
        cbAligned = RT_ALIGN_Z(cbUnaligned, 16);
    else
        cbAligned = RT_ALIGN_Z(cbUnaligned, sizeof(void *));
    return RTMemAllocTag(cbAligned, pszTag);
}


RTDECL(void *) RTMemAllocZVarTag(size_t cbUnaligned, const char *pszTag) RT_NO_THROW
{
    size_t cbAligned;
    if (cbUnaligned >= 16)
        cbAligned = RT_ALIGN_Z(cbUnaligned, 16);
    else
        cbAligned = RT_ALIGN_Z(cbUnaligned, sizeof(void *));
    return RTMemAllocZTag(cbAligned, pszTag);
}


RTDECL(void *) RTMemReallocTag(void *pvOld, size_t cbNew, const char *pszTag) RT_NO_THROW
{
    if (!pvOld)
        return RTMemAllocZTag(cbNew, pszTag);

    HANDLE hHeap = g_hSupR3HardenedHeap;
    Assert(hHeap != NULL);
    void *pv = RtlReAllocateHeap(hHeap, 0 /*dwFlags*/, pvOld, cbNew);
    if (!pv)
        supR3HardenedFatal("RtlReAllocateHeap failed to allocate %zu bytes.\n", cbNew);
    return pv;
}


RTDECL(void) RTMemFree(void *pv) RT_NO_THROW
{
    if (pv)
    {
        HANDLE hHeap = g_hSupR3HardenedHeap;
        Assert(hHeap != NULL);
        RtlFreeHeap(hHeap, 0 /* dwFlags*/, pv);
    }
}


/*
 * Simplified version of RTMemWipeThoroughly that avoids dragging in the
 * random number code.
 */

RTDECL(void) RTMemWipeThoroughly(void *pv, size_t cb, size_t cMinPasses) RT_NO_THROW
{
    size_t cPasses = RT_MIN(cMinPasses, 6);
    static const uint32_t s_aPatterns[] = { 0x00, 0xaa, 0x55, 0xff, 0xf0, 0x0f, 0xcc, 0x3c, 0xc3 };
    uint32_t iPattern = 0;
    do
    {
        memset(pv, s_aPatterns[iPattern], cb);
        iPattern = (iPattern + 1) % RT_ELEMENTS(s_aPatterns);
        ASMMemoryFence();

        memset(pv, s_aPatterns[iPattern], cb);
        iPattern = (iPattern + 1) % RT_ELEMENTS(s_aPatterns);
        ASMMemoryFence();

        memset(pv, s_aPatterns[iPattern], cb);
        iPattern = (iPattern + 1) % RT_ELEMENTS(s_aPatterns);
        ASMMemoryFence();
    } while (cPasses-- > 0);

    memset(pv, 0xff, cb);
    ASMMemoryFence();
}

