/* $Id$ */
/** @file
 * DnD transfer list  tests.
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


int main()
{
    /*
     * Init the runtime, test and say hello.
     */
    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstDnDTransferList", &hTest);
    if (rc)
        return rc;
    RTTestBanner(hTest);

    char szPathWellKnown[RTPATH_MAX];
    RTStrCopy(szPathWellKnown, sizeof(szPathWellKnown),
#ifdef RT_OS_WINDOWS
              "C:\\Windows\\System32\\Boot\\");
#else
              "/bin/");
#endif

    char szPathWellKnownURI[RTPATH_MAX];
    RTStrPrintf(szPathWellKnownURI, sizeof(szPathWellKnownURI), "file:///%s", szPathWellKnown);

    DNDTRANSFERLIST list;
    RT_ZERO(list);

    /* Invalid stuff. */
    RTTEST_CHECK_RC(hTest, DnDTransferListInitEx(&list, "", DNDTRANSFERLISTFMT_NATIVE), VERR_INVALID_PARAMETER);
    RTTEST_CHECK_RC(hTest, DnDTransferListInitEx(&list, szPathWellKnown, DNDTRANSFERLISTFMT_NATIVE), VINF_SUCCESS);
    RTTEST_CHECK_RC(hTest, DnDTransferListInitEx(&list, szPathWellKnown, DNDTRANSFERLISTFMT_NATIVE), VERR_WRONG_ORDER);
    DnDTransferListDestroy(&list);

    /* Empty. */
    RTTEST_CHECK_RC(hTest, DnDTransferListInit(&list), VINF_SUCCESS);
    DnDTransferListDestroy(&list);

    /* Initial status. */
    RTTEST_CHECK_RC(hTest, DnDTransferListInitEx(&list, szPathWellKnown, DNDTRANSFERLISTFMT_NATIVE), VINF_SUCCESS);
    RTTEST_CHECK(hTest, DnDTransferListGetRootCount(&list) == 0);
    RTTEST_CHECK(hTest, DnDTransferListObjCount(&list) == 0);
    RTTEST_CHECK(hTest, DnDTransferListObjTotalBytes(&list) == 0);
    RTTEST_CHECK(hTest, DnDTransferListObjGetFirst(&list) == NULL);
    DnDTransferListDestroy(&list);

    char szPathTest[RTPATH_MAX];

    /* Root path handling. */
    RTTEST_CHECK_RC(hTest, DnDTransferListInitEx(&list, szPathWellKnown, DNDTRANSFERLISTFMT_NATIVE), VINF_SUCCESS);
    RTTEST_CHECK_RC(hTest, DnDTransferListAppendPath(&list, DNDTRANSFERLISTFMT_NATIVE, "/wrong/root/path", DNDTRANSFERLIST_FLAGS_NONE), VERR_INVALID_PARAMETER);
    rc = RTPathJoin(szPathTest, sizeof(szPathTest), szPathWellKnown, "/non/existing");
    AssertRCReturn(rc, RTEXITCODE_FAILURE);
    RTTEST_CHECK_RC(hTest, DnDTransferListAppendPath(&list, DNDTRANSFERLISTFMT_NATIVE, szPathTest, DNDTRANSFERLIST_FLAGS_NONE), VERR_PATH_NOT_FOUND);
    DnDTransferListDestroy(&list);

    /* Adding native stuff. */
    /* No root path set yet and non-recursive -> will set root path to szPathWellKnown, but without any entries added. */
    RTTEST_CHECK_RC(hTest, DnDTransferListInitEx(&list, szPathWellKnown, DNDTRANSFERLISTFMT_NATIVE), VINF_SUCCESS);
    RTTEST_CHECK_RC(hTest, DnDTransferListAppendPath(&list, DNDTRANSFERLISTFMT_NATIVE, szPathWellKnown, DNDTRANSFERLIST_FLAGS_NONE), VINF_SUCCESS);
    RTTEST_CHECK(hTest, DnDTransferListGetRootCount(&list));
    RTTEST_CHECK(hTest, DnDTransferListObjCount(&list));

    /* Add szPathWellKnown again, this time recursively. */
    RTTEST_CHECK_RC(hTest, DnDTransferListAppendPath(&list, DNDTRANSFERLISTFMT_NATIVE, szPathWellKnown, DNDTRANSFERLIST_FLAGS_RECURSIVE), VINF_SUCCESS);
    RTTEST_CHECK(hTest, DnDTransferListGetRootCount(&list));
    RTTEST_CHECK(hTest, DnDTransferListObjCount(&list));

    char *pszString = NULL;
    size_t cbString = 0;
    RTTEST_CHECK_RC_OK(hTest, DnDTransferListGetRoots(&list, DNDTRANSFERLISTFMT_NATIVE, &pszString, &cbString));
    RTTestPrintf(hTest, RTTESTLVL_DEBUG, "Roots:\n%s\n\n", pszString);
    RTStrFree(pszString);

    PDNDTRANSFEROBJECT pObj;
    while ((pObj = DnDTransferListObjGetFirst(&list)))
    {
        RTTestPrintf(hTest, RTTESTLVL_DEBUG, "Obj: %s\n", DnDTransferObjectGetDestPath(pObj));
        DnDTransferListObjRemoveFirst(&list);
    }
    DnDTransferListDestroy(&list);

    char  *pszBuf;
    size_t cbBuf;

    /* To URI data. */
    RTTEST_CHECK_RC(hTest, DnDTransferListInitEx(&list, szPathWellKnownURI, DNDTRANSFERLISTFMT_URI), VINF_SUCCESS);
    RTStrPrintf(szPathTest, sizeof(szPathTest), "%s/foo", szPathWellKnownURI);
    RTTEST_CHECK_RC(hTest, DnDTransferListAppendPath(&list, DNDTRANSFERLISTFMT_URI, szPathWellKnownURI, DNDTRANSFERLIST_FLAGS_NONE), VINF_SUCCESS);
    RTTEST_CHECK_RC(hTest, DnDTransferListAppendPath(&list, DNDTRANSFERLISTFMT_URI, szPathTest, DNDTRANSFERLIST_FLAGS_NONE), VERR_FILE_NOT_FOUND);
    RTTEST_CHECK_RC(hTest, DnDTransferListGetRootsEx(&list, DNDTRANSFERLISTFMT_NATIVE, "" /* pszBasePath */, "\n", &pszBuf, &cbBuf), VINF_SUCCESS);
    RTTestPrintf(hTest, RTTESTLVL_DEBUG, "Roots (native):\n%s\n", pszBuf);
    RTStrFree(pszBuf);
    RTTEST_CHECK_RC(hTest, DnDTransferListGetRootsEx(&list, DNDTRANSFERLISTFMT_URI, "" /* pszBasePath */, "\n", &pszBuf, &cbBuf), VINF_SUCCESS);
    RTTestPrintf(hTest, RTTESTLVL_DEBUG, "Roots (URI):\n%s\n", pszBuf);
    RTStrFree(pszBuf);
    RTTEST_CHECK_RC(hTest, DnDTransferListGetRootsEx(&list, DNDTRANSFERLISTFMT_URI, "\\new\\base\\path", "\n", &pszBuf, &cbBuf), VINF_SUCCESS);
    RTTestPrintf(hTest, RTTESTLVL_ALWAYS, "Roots (URI, new base):\n%s\n", pszBuf);
    RTStrFree(pszBuf);
    RTTEST_CHECK_RC(hTest, DnDTransferListGetRootsEx(&list, DNDTRANSFERLISTFMT_URI, "\\..\\invalid\\path", "\n", &pszBuf, &cbBuf), VERR_INVALID_PARAMETER);
    DnDTransferListDestroy(&list);

    /* From URI data. */
    const char szURI[] = "file:///C:/Windows/System32/Boot/\r\n"
                         "file:///C:/Windows/System/\r\n";

    RTTEST_CHECK_RC(hTest, DnDTransferListAppendPathsFromBuffer(&list, DNDTRANSFERLISTFMT_URI, szURI, sizeof(szURI), "\r\n",
                                                                DNDTRANSFERLIST_FLAGS_NONE), VINF_SUCCESS);
    RTTEST_CHECK(hTest, DnDTransferListGetRootCount(&list) == 2);
    RTTEST_CHECK(hTest, RTPathCompare(DnDTransferListGetRootPathAbs(&list), "C:/Windows/") == 0);
    RTTEST_CHECK_RC(hTest, DnDTransferListGetRootsEx(&list, DNDTRANSFERLISTFMT_NATIVE, "/native/base/path", "\n", &pszBuf, &cbBuf), VINF_SUCCESS);
    RTTestPrintf(hTest, RTTESTLVL_ALWAYS, "Roots (URI, new base):\n%s\n", pszBuf);
    RTTEST_CHECK_RC(hTest, DnDTransferListGetRootsEx(&list, DNDTRANSFERLISTFMT_NATIVE, "\\windows\\path", "\n", &pszBuf, &cbBuf), VINF_SUCCESS);
    RTTestPrintf(hTest, RTTESTLVL_ALWAYS, "Roots (URI, new base):\n%s\n", pszBuf);
    RTTEST_CHECK_RC(hTest, DnDTransferListGetRootsEx(&list, DNDTRANSFERLISTFMT_NATIVE, "\\\\windows\\\\path", "\n", &pszBuf, &cbBuf), VINF_SUCCESS);
    RTTestPrintf(hTest, RTTESTLVL_ALWAYS, "Roots (URI, new base):\n%s\n", pszBuf);
    RTStrFree(pszBuf);

    DnDTransferListDestroy(&list);
    DnDTransferListDestroy(&list); /* Doing this twice here is intentional. */

    return RTTestSummaryAndDestroy(hTest);
}

