/* $Id$ */
/** @file
 * IPRT - Random Numbers.
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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
#include <iprt/rand.h>
#include <iprt/time.h>
#include <iprt/asm.h>
#include <iprt/err.h>
#include <iprt/assert.h>
#include <iprt/thread.h>
#include <iprt/once.h>
#include "internal/rand.h"


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** For lazily initializating of the random generator. */
static RTONCE g_rtRandOnce = RTONCE_INITIALIZER;
/** The default random generator. */
static RTRAND g_hRand = NIL_RTRAND;


/**
 * Perform lazy initialization.
 *
 * @returns IPRT status code.
 * @param   pvUser1     Ignored.
 * @param   pvUser2     Ignored.
 */
static DECLCALLBACK(int) rtRandInitOnce(void *pvUser1, void *pvUser2)
{
    NOREF(pvUser1);
    NOREF(pvUser2);

    RTRAND hRand;
    int rc = RTRandAdvCreateSystemFaster(&hRand);
    if (RT_FAILURE(rc))
        rc = RTRandAdvCreateParkMiller(&hRand);
    if (RT_SUCCESS(rc))
    {
        RTRandAdvSeed(hRand, ASMReadTSC() >> 8);
        g_hRand = hRand;
    }
    else
        AssertRC(rc);

    return rc;
}


RTDECL(void) RTRandBytes(void *pv, size_t cb) RT_NO_THROW
{
    RTOnce(&g_rtRandOnce, rtRandInitOnce, NULL, NULL);
    RTRandAdvBytes(g_hRand, pv, cb);
}


RTDECL(uint32_t) RTRandU32Ex(uint32_t u32First, uint32_t u32Last) RT_NO_THROW
{
    RTOnce(&g_rtRandOnce, rtRandInitOnce, NULL, NULL);
    return RTRandAdvU32Ex(g_hRand, u32First, u32Last);
}


RTDECL(uint32_t) RTRandU32(void) RT_NO_THROW
{
    RTOnce(&g_rtRandOnce, rtRandInitOnce, NULL, NULL);
    return RTRandAdvU32(g_hRand);
}


RTDECL(int32_t) RTRandS32Ex(int32_t i32First, int32_t i32Last) RT_NO_THROW
{
    RTOnce(&g_rtRandOnce, rtRandInitOnce, NULL, NULL);
    return RTRandAdvS32Ex(g_hRand, i32First, i32Last);
}


RTDECL(int32_t) RTRandS32(void) RT_NO_THROW
{
    RTOnce(&g_rtRandOnce, rtRandInitOnce, NULL, NULL);
    return RTRandAdvS32(g_hRand);
}


RTDECL(uint64_t) RTRandU64Ex(uint64_t u64First, uint64_t u64Last) RT_NO_THROW
{
    RTOnce(&g_rtRandOnce, rtRandInitOnce, NULL, NULL);
    return RTRandAdvU64Ex(g_hRand, u64First, u64Last);
}


RTDECL(uint64_t) RTRandU64(void) RT_NO_THROW
{
    RTOnce(&g_rtRandOnce, rtRandInitOnce, NULL, NULL);
    return RTRandAdvU64(g_hRand);
}


RTDECL(int64_t) RTRandS64Ex(int64_t i64First, int64_t i64Last) RT_NO_THROW
{
    RTOnce(&g_rtRandOnce, rtRandInitOnce, NULL, NULL);
    return RTRandAdvS64Ex(g_hRand, i64First, i64Last);
}


RTDECL(int64_t) RTRandS64(void) RT_NO_THROW
{
    RTOnce(&g_rtRandOnce, rtRandInitOnce, NULL, NULL);
    return RTRandAdvS32(g_hRand);
}

