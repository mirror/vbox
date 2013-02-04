/* $Id$ */
/** @file
 * IPRT Testcase - StrCache.
 */

/*
 * Copyright (C) 2009-2010 Oracle Corporation
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
#include <iprt/strcache.h>

#include <iprt/asm.h>
#include <iprt/err.h>
#include <iprt/initterm.h>
#include <iprt/string.h>
#include <iprt/test.h>
#include <iprt/thread.h>
#include <iprt/rand.h>


/**
 * Basic API checks.
 * We'll return if any of these fails.
 */
static void tst1(RTSTRCACHE hStrCache)
{
    const char *psz;

    /* Simple string entering and length. */
    RTTESTI_CHECK_RETV(psz = RTStrCacheEnter(hStrCache, "abcdefgh"));
    RTTESTI_CHECK_RETV(strcmp(psz, "abcdefgh") == 0);
    RTTESTI_CHECK_RETV(RTStrCacheLength(psz) == strlen("abcdefgh"));
    RTTESTI_CHECK_RETV(RTStrCacheRelease(hStrCache, psz) == 0);

    RTTESTI_CHECK_RETV(psz = RTStrCacheEnter(hStrCache, "abcdefghijklmnopqrstuvwxyz"));
    RTTESTI_CHECK_RETV(strcmp(psz, "abcdefghijklmnopqrstuvwxyz") == 0);
    RTTESTI_CHECK_RETV(RTStrCacheLength(psz) == strlen("abcdefghijklmnopqrstuvwxyz"));
    RTTESTI_CHECK_RETV(RTStrCacheRelease(hStrCache, psz) == 0);

    /* Unterminated strings. */
    RTTESTI_CHECK_RETV(psz = RTStrCacheEnterN(hStrCache, "0123456789", 3));
    RTTESTI_CHECK_RETV(strcmp(psz, "012") == 0);
    RTTESTI_CHECK_RETV(RTStrCacheLength(psz) == strlen("012"));
    RTTESTI_CHECK_RETV(RTStrCacheRelease(hStrCache, psz) == 0);

    RTTESTI_CHECK_RETV(psz = RTStrCacheEnterN(hStrCache, "0123456789abcdefghijklmnopqrstuvwxyz", 16));
    RTTESTI_CHECK_RETV(strcmp(psz, "0123456789abcdef") == 0);
    RTTESTI_CHECK_RETV(RTStrCacheLength(psz) == strlen("0123456789abcdef"));
    RTTESTI_CHECK_RETV(RTStrCacheRelease(hStrCache, psz) == 0);

    /* String referencing. */
    char szTest[4096+16];
    memset(szTest, 'a', sizeof(szTest));
    char szTest2[4096+16];
    memset(szTest2, 'f', sizeof(szTest));
    for (int32_t i = 4096; i > 3; i /= 3)
    {
        void *pv2;
        RTTESTI_CHECK_RETV(psz = RTStrCacheEnterN(hStrCache, szTest, i));
        RTTESTI_CHECK_MSG_RETV((pv2 = ASMMemIsAll8(psz, i, 'a')) == NULL && !psz[i], ("i=%#x psz=%p off=%#x\n", i, psz, (uintptr_t)pv2 - (uintptr_t)psz));
        RTTESTI_CHECK(RTStrCacheRetain(psz) == 2);
        RTTESTI_CHECK(RTStrCacheRetain(psz) == 3);
        RTTESTI_CHECK(RTStrCacheRetain(psz) == 4);
        RTTESTI_CHECK_MSG_RETV((pv2 = ASMMemIsAll8(psz, i, 'a')) == NULL && !psz[i], ("i=%#x psz=%p off=%#x\n", i, psz, (uintptr_t)pv2 - (uintptr_t)psz));
        RTTESTI_CHECK(RTStrCacheRelease(hStrCache, psz) == 3);
        RTTESTI_CHECK_MSG_RETV((pv2 = ASMMemIsAll8(psz, i, 'a')) == NULL && !psz[i], ("i=%#x psz=%p off=%#x\n", i, psz, (uintptr_t)pv2 - (uintptr_t)psz));
        RTTESTI_CHECK(RTStrCacheRetain(psz) == 4);
        RTTESTI_CHECK(RTStrCacheRetain(psz) == 5);
        RTTESTI_CHECK(RTStrCacheRetain(psz) == 6);
        RTTESTI_CHECK(RTStrCacheRelease(NIL_RTSTRCACHE, psz) == 5);
        RTTESTI_CHECK(RTStrCacheRelease(NIL_RTSTRCACHE, psz) == 4);
        RTTESTI_CHECK_MSG_RETV((pv2 = ASMMemIsAll8(psz, i, 'a')) == NULL && !psz[i], ("i=%#x psz=%p off=%#x\n", i, psz, (uintptr_t)pv2 - (uintptr_t)psz));

        for (uint32_t cRefs = 3;; cRefs--)
        {
            RTTESTI_CHECK(RTStrCacheRelease(hStrCache, psz) == cRefs);
            if (cRefs == 0)
                break;
            RTTESTI_CHECK_MSG_RETV((pv2 = ASMMemIsAll8(psz, i, 'a')) == NULL && !psz[i], ("i=%#x psz=%p off=%#x cRefs=%d\n", i, psz, (uintptr_t)pv2 - (uintptr_t)psz, cRefs));
            for (uint32_t j = 0; j < 42; j++)
            {
                const char *psz2;
                RTTESTI_CHECK_RETV(psz2 = RTStrCacheEnterN(hStrCache, szTest2, i));
                RTTESTI_CHECK_RETV(psz2 != psz);
                RTTESTI_CHECK(RTStrCacheRelease(hStrCache, psz2) == 0);
                RTTESTI_CHECK_MSG_RETV((pv2 = ASMMemIsAll8(psz, i, 'a')) == NULL && !psz[i], ("i=%#x psz=%p off=%#x cRefs=%d\n", i, psz, (uintptr_t)pv2 - (uintptr_t)psz, cRefs));
            }
        }
    }
}


int main()
{
    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstRTStrCache", &hTest);
    if (rc)
        return rc;
    RTTestBanner(hTest);

    /*
     * Smoke tests using first the default and then a custom pool.
     */
    RTTestSub(hTest, "Smoke test on default cache");
    tst1(RTSTRCACHE_DEFAULT);

    RTTestSub(hTest, "Smoke test on custom cache");
    RTSTRCACHE hStrCache;
    RTTESTI_CHECK_RC(rc = RTStrCacheCreate(&hStrCache, "test 2a"), VINF_SUCCESS);
    if (RT_SUCCESS(rc))
        RTTESTI_CHECK_RC(rc = RTStrCacheDestroy(hStrCache), VINF_SUCCESS);
    RTTESTI_CHECK_RC(rc = RTStrCacheDestroy(NIL_RTSTRCACHE), VINF_SUCCESS);
    RTTESTI_CHECK_RC(rc = RTStrCacheDestroy(RTSTRCACHE_DEFAULT), VINF_SUCCESS);
    RTTESTI_CHECK_RC(rc = RTStrCacheDestroy(RTSTRCACHE_DEFAULT), VINF_SUCCESS);

    RTTESTI_CHECK_RC(rc = RTStrCacheCreate(&hStrCache, "test 2b"), VINF_SUCCESS);
    if (RT_SUCCESS(rc))
    {
        tst1(hStrCache);
        RTTESTI_CHECK_RC(rc = RTStrCacheDestroy(hStrCache), VINF_SUCCESS);
    }

    /*
     * Summary.
     */
    return RTTestSummaryAndDestroy(hTest);
}

