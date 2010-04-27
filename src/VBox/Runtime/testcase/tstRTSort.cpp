/* $Id$ */
/** @file
 * IPRT Testcase - Sorting.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
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
#include <iprt/sort.h>

#include <iprt/err.h>
#include <iprt/rand.h>
#include <iprt/string.h>
#include <iprt/test.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef struct TSTRTSORTAPV
{
    uint32_t    aValues[8192];
    void       *apv[8192];
    size_t      cElements;
} TSTRTSORTAPV;


static DECLCALLBACK(int) testApvCompare(void const *pvElement1, void const *pvElement2, void *pvUser)
{
    TSTRTSORTAPV   *pData        = (TSTRTSORTAPV *)pvUser;
    uint32_t const *pu32Element1 = (uint32_t const *)pvElement1;
    uint32_t const *pu32Element2 = (uint32_t const *)pvElement2;
    RTTESTI_CHECK(VALID_PTR(pData) && pData->cElements <= RT_ELEMENTS(pData->aValues));
    RTTESTI_CHECK((uintptr_t)(pu32Element1 - &pData->aValues[0]) < pData->cElements);
    RTTESTI_CHECK((uintptr_t)(pu32Element2 - &pData->aValues[0]) < pData->cElements);

    if (*pu32Element1 < *pu32Element2)
        return -1;
    if (*pu32Element1 > *pu32Element2)
        return 1;
    return 0;
}

static void testApvSorter(FNRTSORTAPV pfnSorter, const char *pszName)
{
    RTTestISub(pszName);

    RTRAND hRand;
    RTTESTI_CHECK_RC_OK_RETV(RTRandAdvCreateParkMiller(&hRand));

    TSTRTSORTAPV Data;
    for (size_t cElements = 0; cElements < RT_ELEMENTS(Data.apv); cElements++)
    {
        RT_ZERO(Data);
        Data.cElements = cElements;

        /* popuplate the array */
        for (size_t i = 0; i < cElements; i++)
        {
            Data.aValues[i] = RTRandAdvU32(hRand);
            Data.apv[i]     = &Data.aValues[i];
        }

        /* sort it */
        pfnSorter(&Data.apv[0], cElements, testApvCompare, &Data);

        /* verify it */
        if (!RTSortApvIsSorted(&Data.apv[0], cElements, testApvCompare, &Data))
            RTTestIFailed("failed sorting %u elements", cElements);
    }
}


int main()
{
    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstRTTemp", &hTest);
    if (rc)
        return rc;
    RTTestBanner(hTest);

    /*
     * Test the different algorithms.
     */
    testApvSorter(RTSortApvShell, "RTSortApvShell - shell sort, pointer array");

    /*
     * Summary.
     */
    return RTTestSummaryAndDestroy(hTest);
}

