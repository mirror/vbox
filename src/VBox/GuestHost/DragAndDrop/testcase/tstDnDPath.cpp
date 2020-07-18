/* $Id$ */
/** @file
 * DnD path tests.
 */

/*
 * Copyright (C) 2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <iprt/assert.h>
#include <iprt/env.h>
#include <iprt/err.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/test.h>

#include <VBox/GuestHost/DragAndDrop.h>


static void tstPathRebase(RTTEST hTest)
{
    struct
    {
        char const *pszPath;
        char const *pszPathOld;
        char const *pszPathNew;
        int rc;
        char const *pszResult;
    } aTests[] = {
        /* Invalid stuff. */
        { NULL, NULL, NULL, VERR_INVALID_POINTER, NULL },
        { "foo", "old", NULL, VERR_INVALID_POINTER, NULL },
        /* Actual rebasing. */
        { "old/foo", "old", "new", VINF_SUCCESS, "new/foo" },
        { "old\\foo", "old", "new", VINF_SUCCESS, "new/foo" },
        { "\\totally\\different\\path\\foo", "/totally/different/path", "/totally/different/path", VINF_SUCCESS, "/totally/different/path/foo" },
        { "\\old\\path\\foo", "", "/new/root/", VINF_SUCCESS, "/new/root/old/path/foo" },
        { "\\\\old\\path\\\\foo", "", "/new/root/", VINF_SUCCESS, "/new/root/old/path\\\\foo" }
    };

    char *pszPath = NULL;
    for (size_t i = 0; i < RT_ELEMENTS(aTests); i++)
    {
        RTTEST_CHECK_RC(hTest, DnDPathRebase(aTests[i].pszPath, aTests[i].pszPathOld, aTests[i].pszPathNew, &pszPath), aTests[i].rc);
        if (RT_SUCCESS(aTests[i].rc))
        {
            if (aTests[i].pszResult)
                RTTEST_CHECK_MSG(hTest, RTPathCompare(pszPath, aTests[i].pszResult) == 0,
                                 (hTest, "Test #%zu failed: Got '%s', expected '%s'", i, pszPath, aTests[i].pszResult));
            RTStrFree(pszPath);
            pszPath = NULL;
        }
    }
}

int main()
{
    /*
     * Init the runtime, test and say hello.
     */
    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstDnDPath", &hTest);
    if (rc)
        return rc;
    RTTestBanner(hTest);

    tstPathRebase(hTest);

    return RTTestSummaryAndDestroy(hTest);
}

