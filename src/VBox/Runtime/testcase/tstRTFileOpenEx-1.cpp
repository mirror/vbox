/* $Id$ */
/** @file
 * IPRT Testcase - File Opening, extended API.
 */

/*
 * Copyright (C) 2019 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/file.h>

#include <iprt/err.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/test.h>


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static const char g_szTestFile[] = "tstFileOpenEx-1.tst";


void tstFileActionTaken(RTTEST hTest)
{
    uint64_t const  fMode = (0644 << RTFILE_O_CREATE_MODE_SHIFT);
    RTFILE          hFile;
    RTFILEACTION    enmTaken;
    int             rc;
    uint8_t         abBuf[512];

    RTTestSub(hTest, "Action taken");
    RTFileDelete(g_szTestFile);

    /*
     * RTFILE_O_OPEN and RTFILE_O_OPEN_CREATE.
     */
    /* RTFILE_O_OPEN - non-existing: */
    RTTEST_CHECK(hTest, !RTPathExists(g_szTestFile));
    hFile = NIL_RTFILE;
    enmTaken = RTFILEACTION_END;
    rc = RTFileOpenEx(g_szTestFile, RTFILE_O_READWRITE | RTFILE_O_OPEN | RTFILE_O_DENY_NONE, &hFile, &enmTaken);
    if (rc != VERR_FILE_NOT_FOUND && rc != VERR_OPEN_FAILED)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_OPEN(#1) -> %Rrc, expected VERR_FILE_NOT_FOUND or VERR_OPEN_FAILED", rc);
    if (enmTaken != RTFILEACTION_INVALID)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_OPEN(#1) -> enmTaken=%d, expected %d (RTFILEACTION_INVALID)",
                     enmTaken, RTFILEACTION_INVALID);
    if (hFile != NIL_RTFILE)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_OPEN(#1) -> hFile=%p, expected %p (NIL_RTFILE)", hFile, NIL_RTFILE);

    /* RTFILE_O_OPEN_CREATE - non-existing: */
    RTTEST_CHECK(hTest, !RTPathExists(g_szTestFile));
    hFile    = NIL_RTFILE;
    enmTaken = RTFILEACTION_END;
    rc = RTFileOpenEx(g_szTestFile, RTFILE_O_READWRITE | RTFILE_O_OPEN_CREATE | RTFILE_O_DENY_NONE | fMode, &hFile, &enmTaken);
    if (rc != VINF_SUCCESS)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_OPEN_CREATE(#1) -> %Rrc, expected VINF_SUCCESS", rc);
    if (enmTaken != RTFILEACTION_CREATED)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_OPEN_CREATE(#1) -> enmTaken=%d, expected %d (RTFILEACTION_CREATED)",
                     enmTaken, RTFILEACTION_CREATED);
    if (hFile == NIL_RTFILE)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_OPEN_CREATE(#1) -> hFile=%p (NIL_RTFILE)", hFile);
    RTTESTI_CHECK_RC(RTFileClose(hFile), VINF_SUCCESS);

    /* RTFILE_O_OPEN_CREATE - existing: */
    RTTEST_CHECK(hTest, RTPathExists(g_szTestFile));
    hFile    = NIL_RTFILE;
    enmTaken = RTFILEACTION_END;
    rc = RTFileOpenEx(g_szTestFile, RTFILE_O_READWRITE | RTFILE_O_OPEN_CREATE | RTFILE_O_DENY_NONE | fMode, &hFile, &enmTaken);
    if (rc != VINF_SUCCESS)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_OPEN_CREATE(#2) -> %Rrc, expected VINF_SUCCESS", rc);
    if (enmTaken != RTFILEACTION_OPENED)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_OPEN_CREATE(#2) -> enmTaken=%d, expected %d (RTFILEACTION_OPENED)",
                     enmTaken, RTFILEACTION_OPENED);
    if (hFile == NIL_RTFILE)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_OPEN_CREATE(#2) -> hFile=%p (NIL_RTFILE)", hFile);
    RTTESTI_CHECK_RC(RTFileClose(hFile), VINF_SUCCESS);

    /* RTFILE_O_OPEN - existing: */
    RTTEST_CHECK(hTest, RTPathExists(g_szTestFile));
    hFile    = NIL_RTFILE;
    enmTaken = RTFILEACTION_END;
    rc = RTFileOpenEx(g_szTestFile, RTFILE_O_READWRITE | RTFILE_O_OPEN | RTFILE_O_DENY_NONE, &hFile, &enmTaken);
    if (rc != VINF_SUCCESS)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_OPEN(#2) -> %Rrc, expected VINF_SUCCESS", rc);
    if (enmTaken != RTFILEACTION_OPENED)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_OPEN(#2) -> enmTaken=%d, expected %d (RTFILEACTION_OPENED)",
                     enmTaken, RTFILEACTION_OPENED);
    if (hFile == NIL_RTFILE)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_OPEN(#3) -> hFile=%p (NIL_RTFILE)", hFile);
    RTTESTI_CHECK_RC(RTFileClose(hFile), VINF_SUCCESS);


    /*
     * RTFILE_O_OPEN and RTFILE_O_OPEN_CREATE w/ TRUNCATE variations.
     */
    /* RTFILE_O_OPEN + TRUNCATE - existing zero sized file: */
    RTTEST_CHECK(hTest, RTPathExists(g_szTestFile));
    hFile    = NIL_RTFILE;
    enmTaken = RTFILEACTION_END;
    rc = RTFileOpenEx(g_szTestFile, RTFILE_O_READWRITE | RTFILE_O_OPEN | RTFILE_O_TRUNCATE | RTFILE_O_DENY_NONE, &hFile, &enmTaken);
    if (rc != VINF_SUCCESS)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_OPEN+TRUNCATE(#1) -> %Rrc, expected VINF_SUCCESS", rc);
    if (enmTaken != RTFILEACTION_TRUNCATED)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_OPEN+TRUNCATE(#1) -> enmTaken=%d, expected %d (RTFILEACTION_TRUNCATED)",
                     enmTaken, RTFILEACTION_TRUNCATED);
    if (hFile == NIL_RTFILE)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_OPEN+TRUNCATE(#1) -> hFile=%p (NIL_RTFILE)", hFile);
    RTTESTI_CHECK_RC(RTFileClose(hFile), VINF_SUCCESS);

    /* RTFILE_O_OPEN_CREATE + TRUNCATE - existing zero sized file: */
    RTTEST_CHECK(hTest, RTPathExists(g_szTestFile));
    hFile    = NIL_RTFILE;
    enmTaken = RTFILEACTION_END;
    rc = RTFileOpenEx(g_szTestFile, RTFILE_O_READWRITE | RTFILE_O_OPEN_CREATE | RTFILE_O_TRUNCATE | RTFILE_O_DENY_NONE, &hFile, &enmTaken);
    if (rc != VINF_SUCCESS)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_OPEN_CREATE+TRUNCATE(#1) -> %Rrc, expected VINF_SUCCESS", rc);
    if (enmTaken != RTFILEACTION_TRUNCATED)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_OPEN_CREATE+TRUNCATE(#1) -> enmTaken=%d, expected %d (RTFILEACTION_TRUNCATED)",
                     enmTaken, RTFILEACTION_TRUNCATED);
    if (hFile == NIL_RTFILE)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_OPEN_CREATE+TRUNCATE(#1) -> hFile=%p (NIL_RTFILE)", hFile);
    RTTESTI_CHECK_RC(RTFileRead(hFile, abBuf, 1, NULL), VERR_EOF); /* check that it was truncated */
    RTTESTI_CHECK_RC(RTFileWrite(hFile, RT_STR_TUPLE("test"),  NULL), VINF_SUCCESS); /* for the trunction in the next test  */
    RTTESTI_CHECK_RC(RTFileClose(hFile), VINF_SUCCESS);

    /* RTFILE_O_OPEN_CREATE + TRUNCATE - existing non-zero sized file: */
    RTTEST_CHECK(hTest, RTPathExists(g_szTestFile));
    hFile    = NIL_RTFILE;
    enmTaken = RTFILEACTION_END;
    rc = RTFileOpenEx(g_szTestFile, RTFILE_O_READWRITE | RTFILE_O_OPEN_CREATE | RTFILE_O_TRUNCATE | RTFILE_O_DENY_NONE, &hFile, &enmTaken);
    if (rc != VINF_SUCCESS)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_OPEN_CREATE+TRUNCATE(#2) -> %Rrc, expected VINF_SUCCESS", rc);
    if (enmTaken != RTFILEACTION_TRUNCATED)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_OPEN_CREATE+TRUNCATE(#2) -> enmTaken=%d, expected %d (RTFILEACTION_TRUNCATED)",
                     enmTaken, RTFILEACTION_TRUNCATED);
    if (hFile == NIL_RTFILE)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_OPEN_CREATE+TRUNCATE(#2) -> hFile=%p (NIL_RTFILE)", hFile);
    RTTESTI_CHECK_RC(RTFileRead(hFile, abBuf, 1, NULL), VERR_EOF); /* check that it was truncated */
    RTTESTI_CHECK_RC(RTFileWrite(hFile, RT_STR_TUPLE("test"),  NULL), VINF_SUCCESS); /* for the trunction in the next test  */
    RTTESTI_CHECK_RC(RTFileClose(hFile), VINF_SUCCESS);

    /* RTFILE_O_OPEN_CREATE + TRUNCATE - non-existing file: */
    RTTEST_CHECK_RC(hTest, RTFileDelete(g_szTestFile), VINF_SUCCESS);
    RTTEST_CHECK(hTest, !RTPathExists(g_szTestFile));
    hFile    = NIL_RTFILE;
    enmTaken = RTFILEACTION_END;
    rc = RTFileOpenEx(g_szTestFile, RTFILE_O_READWRITE | RTFILE_O_OPEN_CREATE | RTFILE_O_TRUNCATE | RTFILE_O_DENY_NONE, &hFile, &enmTaken);
    if (rc != VINF_SUCCESS)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_OPEN_CREATE+TRUNCATE(#2) -> %Rrc, expected VINF_SUCCESS", rc);
    if (enmTaken != RTFILEACTION_CREATED)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_OPEN_CREATE+TRUNCATE(#2) -> enmTaken=%d, expected %d (RTFILEACTION_CREATED)",
                     enmTaken, RTFILEACTION_CREATED);
    if (hFile == NIL_RTFILE)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_OPEN_CREATE+TRUNCATE(#2) -> hFile=%p (NIL_RTFILE)", hFile);
    RTTESTI_CHECK_RC(RTFileWrite(hFile, RT_STR_TUPLE("test"),  NULL), VINF_SUCCESS); /* for the trunction in the next test  */
    RTTESTI_CHECK_RC(RTFileClose(hFile), VINF_SUCCESS);

    /* RTFILE_O_OPEN + TRUNCATE - existing non-zero sized file: */
    RTTEST_CHECK(hTest, RTPathExists(g_szTestFile));
    hFile    = NIL_RTFILE;
    enmTaken = RTFILEACTION_END;
    rc = RTFileOpenEx(g_szTestFile, RTFILE_O_READWRITE | RTFILE_O_OPEN | RTFILE_O_TRUNCATE | RTFILE_O_DENY_NONE, &hFile, &enmTaken);
    if (rc != VINF_SUCCESS)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_OPEN+TRUNCATE(#2) -> %Rrc, expected VINF_SUCCESS", rc);
    if (enmTaken != RTFILEACTION_TRUNCATED)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_OPEN+TRUNCATE(#2) -> enmTaken=%d, expected %d (RTFILEACTION_TRUNCATED)",
                     enmTaken, RTFILEACTION_TRUNCATED);
    if (hFile == NIL_RTFILE)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_OPEN+TRUNCATE(#2) -> hFile=%p (NIL_RTFILE)", hFile);
    RTTESTI_CHECK_RC(RTFileRead(hFile, abBuf, 1, NULL), VERR_EOF); /* check that it was truncated */
    RTTESTI_CHECK_RC(RTFileWrite(hFile, RT_STR_TUPLE("test"),  NULL), VINF_SUCCESS); /* for the replacement in the next test  */
    RTTESTI_CHECK_RC(RTFileClose(hFile), VINF_SUCCESS);


    /*
     * RTFILE_O_CREATE and RTFILE_O_CREATE_REPLACE.
     */
    /* RTFILE_O_CREATE_REPLACE - existing: */
    RTTEST_CHECK(hTest, RTPathExists(g_szTestFile));
    hFile    = NIL_RTFILE;
    enmTaken = RTFILEACTION_END;
    rc = RTFileOpenEx(g_szTestFile, RTFILE_O_READWRITE | RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_NONE | fMode, &hFile, &enmTaken);
    if (rc != VINF_SUCCESS)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_CREATE_REPLACE(#1) -> %Rrc, expected VINF_SUCCESS", rc);
    if (enmTaken != RTFILEACTION_REPLACED)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_CREATE_REPLACE(#1) -> enmTaken=%d, expected %d (RTFILEACTION_REPLACED)",
                     enmTaken, RTFILEACTION_REPLACED);
    if (hFile == NIL_RTFILE)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_CREATE_REPLACE(#1) -> hFile=%p (NIL_RTFILE)", hFile);
    RTTESTI_CHECK_RC(RTFileRead(hFile, abBuf, 1, NULL), VERR_EOF); /* check that it was replaced */
    RTTESTI_CHECK_RC(RTFileClose(hFile), VINF_SUCCESS);

    /* RTFILE_O_CREATE_REPLACE - non-existing: */
    RTTESTI_CHECK_RC(RTFileDelete(g_szTestFile), VINF_SUCCESS);
    RTTEST_CHECK(hTest, !RTPathExists(g_szTestFile));
    hFile    = NIL_RTFILE;
    enmTaken = RTFILEACTION_END;
    rc = RTFileOpenEx(g_szTestFile, RTFILE_O_READWRITE | RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_NONE | fMode, &hFile, &enmTaken);
    if (rc != VINF_SUCCESS)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_CREATE_REPLACE(#2) -> %Rrc, expected VINF_SUCCESS", rc);
    if (enmTaken != RTFILEACTION_CREATED)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_CREATE_REPLACE(#2) -> enmTaken=%d, expected %d (RTFILEACTION_CREATED)",
                     enmTaken, RTFILEACTION_CREATED);
    if (hFile == NIL_RTFILE)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_CREATE_REPLACE(#2) -> hFile=%p (NIL_RTFILE)", hFile);
    RTTESTI_CHECK_RC(RTFileClose(hFile), VINF_SUCCESS);

    /* RTFILE_O_CREATE - existing: */
    RTTEST_CHECK(hTest, RTPathExists(g_szTestFile));
    hFile    = NIL_RTFILE;
    enmTaken = RTFILEACTION_END;
    rc = RTFileOpenEx(g_szTestFile, RTFILE_O_READWRITE | RTFILE_O_CREATE | RTFILE_O_DENY_NONE | fMode, &hFile, &enmTaken);
    if (rc != VERR_ALREADY_EXISTS && rc != VERR_OPEN_FAILED)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_CREATE(#1) -> %Rrc, expected VERR_ALREADY_EXISTS or VERR_OPEN_FAILED", rc);
    if (enmTaken != RTFILEACTION_ALREADY_EXISTS)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_CREATE(#1) -> enmTaken=%d, expected %d (RTFILEACTION_ALREADY_EXISTS)",
                     enmTaken, RTFILEACTION_ALREADY_EXISTS);
    if (hFile != NIL_RTFILE)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_CREATE(#1) -> hFile=%p, expected %p (NIL_RTFILE)", hFile, NIL_RTFILE);

    /* RTFILE_O_CREATE - non-existing: */
    RTTESTI_CHECK_RC(RTFileDelete(g_szTestFile), VINF_SUCCESS);
    RTTEST_CHECK(hTest, !RTPathExists(g_szTestFile));
    hFile    = NIL_RTFILE;
    enmTaken = RTFILEACTION_END;
    rc = RTFileOpenEx(g_szTestFile, RTFILE_O_READWRITE | RTFILE_O_CREATE | RTFILE_O_DENY_NONE | fMode, &hFile, &enmTaken);
    if (rc != VINF_SUCCESS)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_CREATE(#2) -> %Rrc, expected VINF_SUCCESS", rc);
    if (enmTaken != RTFILEACTION_CREATED)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_CREATE(#2) -> enmTaken=%d, expected %d (RTFILEACTION_CREATED)",
                     enmTaken, RTFILEACTION_CREATED);
    if (hFile == NIL_RTFILE)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_CREATE(#2) -> hFile=%p (NIL_RTFILE)", hFile);
    RTTESTI_CHECK_RC(RTFileClose(hFile), VINF_SUCCESS);

    /*
     * RTFILE_O_CREATE and RTFILE_O_CREATE_REPLACE w/ TRUNCATE variations.
     */
    /* RTFILE_O_CREATE+TRUNCATE - non-existing: */
    RTTESTI_CHECK_RC(RTFileDelete(g_szTestFile), VINF_SUCCESS);
    RTTEST_CHECK(hTest, !RTPathExists(g_szTestFile));
    hFile    = NIL_RTFILE;
    enmTaken = RTFILEACTION_END;
    rc = RTFileOpenEx(g_szTestFile, RTFILE_O_READWRITE | RTFILE_O_CREATE | RTFILE_O_TRUNCATE | RTFILE_O_DENY_NONE | fMode, &hFile, &enmTaken);
    if (rc != VINF_SUCCESS)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_CREATE+TRUNCATE(#1) -> %Rrc, expected VINF_SUCCESS", rc);
    if (enmTaken != RTFILEACTION_CREATED)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_CREATE+TRUNCATE(#1) -> enmTaken=%d, expected %d (RTFILEACTION_CREATED)",
                     enmTaken, RTFILEACTION_CREATED);
    if (hFile == NIL_RTFILE)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_CREATE+TRUNCATE(#1) -> hFile=%p (NIL_RTFILE)", hFile);
    RTTESTI_CHECK_RC(RTFileClose(hFile), VINF_SUCCESS);

    /* RTFILE_O_CREATE+TRUNCATE - existing: */
    RTTEST_CHECK(hTest, RTPathExists(g_szTestFile));
    hFile    = NIL_RTFILE;
    enmTaken = RTFILEACTION_END;
    rc = RTFileOpenEx(g_szTestFile, RTFILE_O_READWRITE | RTFILE_O_CREATE | RTFILE_O_TRUNCATE | RTFILE_O_DENY_NONE | fMode, &hFile, &enmTaken);
    if (rc != VERR_ALREADY_EXISTS && rc != VERR_OPEN_FAILED)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_CREATE+TRUNCATE(#2) -> %Rrc, expected VERR_ALREADY_EXISTS or VERR_OPEN_FAILED", rc);
    if (enmTaken != RTFILEACTION_ALREADY_EXISTS)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_CREATE+TRUNCATE(#2) -> enmTaken=%d, expected %d (RTFILEACTION_ALREADY_EXISTS)",
                     enmTaken, RTFILEACTION_ALREADY_EXISTS);
    if (hFile != NIL_RTFILE)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_CREATE+TRUNCATE(#2) -> hFile=%p, expected %p (NIL_RTFILE)", hFile, NIL_RTFILE);

    /* RTFILE_O_CREATE_REPLACE+TRUNCATE - existing: */
    RTTEST_CHECK(hTest, RTPathExists(g_szTestFile));
    hFile    = NIL_RTFILE;
    enmTaken = RTFILEACTION_END;
    rc = RTFileOpenEx(g_szTestFile, RTFILE_O_READWRITE | RTFILE_O_CREATE_REPLACE | RTFILE_O_TRUNCATE | RTFILE_O_DENY_NONE | fMode, &hFile, &enmTaken);
    if (rc != VINF_SUCCESS)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_CREATE_REPLACE+TRUNCATE(#1) -> %Rrc, expected VINF_SUCCESS", rc);
    if (enmTaken != RTFILEACTION_REPLACED)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_CREATE_REPLACE+TRUNCATE(#1) -> enmTaken=%d, expected %d (RTFILEACTION_REPLACED)",
                     enmTaken, RTFILEACTION_REPLACED);
    if (hFile == NIL_RTFILE)
        RTTestFailed(hTest, "RTFileOpenEx+RTFILE_O_CREATE_REPLACE+TRUNCATE(#1) -> hFile=%p (NIL_RTFILE)", hFile);
    RTTESTI_CHECK_RC(RTFileClose(hFile), VINF_SUCCESS);

    RTTESTI_CHECK_RC(RTFileDelete(g_szTestFile), VINF_SUCCESS);
}


int main()
{
    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstRTFileOpenEx-1", &hTest);
    if (rc)
        return rc;
    RTTestBanner(hTest);
    tstFileActionTaken(hTest);
    RTFileDelete(g_szTestFile);
    return RTTestSummaryAndDestroy(hTest);
}

