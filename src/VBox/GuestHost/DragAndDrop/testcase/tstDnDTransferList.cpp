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
              "C:\\Windows\\INF\\");
#else
              "/bin/");
#endif

    DNDTRANSFERLIST list;
    RT_ZERO(list);

    /* Invalid stuff. */
    /*RTTEST_CHECK_RC(hTest, DnDTransferListInit(&list, NULL), VERR_INVALID_POINTER);*/
    RTTEST_CHECK_RC(hTest, DnDTransferListInit(&list, ""), VERR_INVALID_PARAMETER);
    RTTEST_CHECK_RC(hTest, DnDTransferListInit(&list, szPathWellKnown), VINF_SUCCESS);
    RTTEST_CHECK_RC(hTest, DnDTransferListInit(&list, szPathWellKnown), VERR_WRONG_ORDER);
    DnDTransferListDestroy(&list);

    /* Initial status. */
    RTTEST_CHECK_RC(hTest, DnDTransferListInit(&list, szPathWellKnown), VINF_SUCCESS);
    RTTEST_CHECK(hTest, DnDTransferListGetRootCount(&list) == 0);
    RTTEST_CHECK(hTest, DnDTransferListObjCount(&list) == 0);
    RTTEST_CHECK(hTest, DnDTransferListObjTotalBytes(&list) == 0);
    RTTEST_CHECK(hTest, DnDTransferListObjGetFirst(&list) == NULL);

    char szPathTest[RTPATH_MAX];

    /* Root path handling. */
    RTTEST_CHECK_RC(hTest, DnDTransferListAppendPath(&list, DNDTRANSFERLISTFMT_NATIVE, "/wrong/root/path", DNDTRANSFERLIST_FLAGS_NONE), VERR_INVALID_PARAMETER);
    rc = RTPathJoin(szPathTest, sizeof(szPathTest), szPathWellKnown, "/non/existing");
    AssertRCReturn(rc, RTEXITCODE_FAILURE);
    RTTEST_CHECK_RC(hTest, DnDTransferListAppendPath(&list, DNDTRANSFERLISTFMT_NATIVE, szPathTest, DNDTRANSFERLIST_FLAGS_NONE), VERR_PATH_NOT_FOUND);

    /* Adding stuff. */
    RTTEST_CHECK_RC(hTest, DnDTransferListAppendPath(&list, DNDTRANSFERLISTFMT_NATIVE, szPathWellKnown, DNDTRANSFERLIST_FLAGS_NONE), VINF_SUCCESS);
    RTTEST_CHECK(hTest, DnDTransferListGetRootCount(&list));
    RTTEST_CHECK(hTest, DnDTransferListObjCount(&list));

    char *pszString = NULL;
    size_t cbString = 0;
    RTTEST_CHECK_RC_OK(hTest, DnDTransferListGetRoots(&list, DNDTRANSFERLISTFMT_NATIVE, &pszString, &cbString));
    RTTestPrintf(hTest, RTTESTLVL_DEBUG, "Root: %s\n", pszString);
    RTStrFree(pszString);
    RTTestPrintf(hTest, RTTESTLVL_DEBUG, "\n");

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
    RTTEST_CHECK_RC(hTest, DnDTransferListInit(&list, szPathWellKnown), VINF_SUCCESS);
    RTTEST_CHECK_RC(hTest, DnDTransferListAppendPath(&list, DNDTRANSFERLISTFMT_NATIVE, szPathWellKnown, DNDTRANSFERLIST_FLAGS_NONE), VINF_SUCCESS);
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
    const char szURI[] = "file:///file1\r\n"
                         "file:///file2\r\n"
                         "file:///dir1\r\n"
                         "file:///dir%20with%20spaces\r\n";
    RTTEST_CHECK_RC(hTest, DnDTransferListAppendPathsFromBuffer(&list, DNDTRANSFERLISTFMT_URI, szURI, sizeof(szURI), "\r\n",
                                                                DNDTRANSFERLIST_FLAGS_NONE), VINF_SUCCESS);
    RTTEST_CHECK(hTest, DnDTransferListGetRootCount(&list) == 4);
    RTTEST_CHECK_RC(hTest, DnDTransferListGetRootsEx(&list, DNDTRANSFERLISTFMT_NATIVE, "/native/base/path", "\n", &pszBuf, &cbBuf), VINF_SUCCESS);
    RTTestPrintf(hTest, RTTESTLVL_ALWAYS, "Roots (URI, new base):\n%s\n", pszBuf);
    RTStrFree(pszBuf);

    return RTTestSummaryAndDestroy(hTest);
}

