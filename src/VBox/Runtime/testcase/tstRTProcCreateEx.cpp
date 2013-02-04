/* $Id$ */
/** @file
 * IPRT Testcase - RTProcCreateEx.
 */

/*
 * Copyright (C) 2010-2012 Oracle Corporation
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
#include <iprt/process.h>

#include <iprt/assert.h>
#include <iprt/env.h>
#include <iprt/err.h>
#include <iprt/initterm.h>
#include <iprt/mem.h>
#include <iprt/message.h>
#include <iprt/param.h>
#include <iprt/pipe.h>
#include <iprt/string.h>
#include <iprt/stream.h>
#include <iprt/test.h>
#include <iprt/thread.h>

#ifdef RT_OS_WINDOWS
# define SECURITY_WIN32
# include <windows.h>
# include <Security.h>
#endif

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static char g_szExecName[RTPATH_MAX];


static const char * const g_apszArgs4[] =
{
    /* 0 */ "non existing non executable file",
    /* 1 */ "--testcase-child-4",
    /* 2 */ "a b",
    /* 3 */ " cdef",
    /* 4 */ "ghijkl ",
    /* 5 */ "\"",
    /* 6 */ "\\",
    /* 7 */ "\\\"",
    /* 8 */ "\\\"\\",
    /* 9 */ "\\\\\"\\",
    /*10 */ "%TEMP%",
    /*11 */ "%TEMP%\filename",
    /*12 */ "%TEMP%postfix",
    /*13 */ "Prefix%TEMP%postfix",
    /*14 */ "%",
    /*15 */ "%%",
    /*16 */ "%%%",
    /*17 */ "%X",
    /*18 */ "%%X",
    NULL
};


static int tstRTCreateProcEx5Child(int argc, char **argv)
{
    int rc = RTR3InitExeNoArguments(0);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

#ifdef RT_OS_WINDOWS
    char szUser[_1K];
    DWORD cbLen = sizeof(szUser);
    /** @todo Does not yet handle ERROR_MORE_DATA for user names longer than 32767. */
    if (!GetUserName(szUser, &cbLen))
    {
        RTPrintf("GetUserName failed with last error=%ld\n", GetLastError());
        return RTEXITCODE_FAILURE;
    }
# if 0 /* Does not work on NT4 (yet). */
    DWORD cbSid = 0;
    DWORD cbDomain = 0;
    SID_NAME_USE sidUse;
    /* First try to figure out how much space for SID + domain name we need. */
    BOOL bRet = LookupAccountName(NULL /* current system*/,
                                  szUser,
                                  NULL,
                                  &cbSid,
                                  NULL,
                                  &cbDomain,
                                  &sidUse);
    if (!bRet)
    {
        DWORD dwErr = GetLastError();
        if (dwErr != ERROR_INSUFFICIENT_BUFFER)
        {
            RTPrintf("LookupAccountName(1) failed with last error=%ld\n", dwErr);
            return RTEXITCODE_FAILURE;
        }
    }

    /* Now try getting the real SID + domain name. */
    SID *pSid = (SID *)RTMemAlloc(cbSid);
    AssertPtr(pSid);
    char *pszDomain = (char *)RTMemAlloc(cbDomain); /* Size in TCHAR! */
    AssertPtr(pszDomain);

    if (!LookupAccountName(NULL /* Current system */,
                           szUser,
                           pSid,
                           &cbSid,
                           pszDomain,
                           &cbDomain,
                           &sidUse))
    {
        RTPrintf("LookupAccountName(2) failed with last error=%ld\n", GetLastError());
        return RTEXITCODE_FAILURE;
    }
    RTMemFree(pSid);
    RTMemFree(pszDomain);
# endif
#else
    /** @todo Lookup UID/effective UID, maybe GID? */
#endif
    return RTEXITCODE_SUCCESS;
}

static void tstRTCreateProcEx5(const char *pszUser, const char *pszPassword)
{
    RTTestISubF("As user \"%s\" with password \"%s\"", pszUser, pszPassword);

    const char * apszArgs[3] =
    {
        "test", /* user name */
        "--testcase-child-5",
        NULL
    };

    RTPROCESS hProc;

    /* Test for invalid logons. */
    RTTESTI_CHECK_RC_RETV(RTProcCreateEx(g_szExecName, apszArgs, RTENV_DEFAULT, 0 /*fFlags*/, NULL,
                                         NULL, NULL, "non-existing-user", "wrong-password", &hProc), VERR_AUTHENTICATION_FAILURE);
    /* Test for invalid application. */
    RTTESTI_CHECK_RC_RETV(RTProcCreateEx("non-existing-app", apszArgs, RTENV_DEFAULT, 0 /*fFlags*/, NULL,
                                         NULL, NULL, NULL, NULL, &hProc), VERR_FILE_NOT_FOUND);
    /* Test a (hopefully) valid user/password logon (given by parameters of this function). */
    RTTESTI_CHECK_RC_RETV(RTProcCreateEx(g_szExecName, apszArgs, RTENV_DEFAULT, 0 /*fFlags*/, NULL,
                                         NULL, NULL, pszUser, pszPassword, &hProc), VINF_SUCCESS);
    RTPROCSTATUS ProcStatus = { -1, RTPROCEXITREASON_ABEND };
    RTTESTI_CHECK_RC(RTProcWait(hProc, RTPROCWAIT_FLAGS_BLOCK, &ProcStatus), VINF_SUCCESS);

    if (ProcStatus.enmReason != RTPROCEXITREASON_NORMAL || ProcStatus.iStatus != 0)
        RTTestIFailed("enmReason=%d iStatus=%d", ProcStatus.enmReason, ProcStatus.iStatus);
    else
        RTTestIPassed(NULL);
}


static int tstRTCreateProcEx4Child(int argc, char **argv)
{
    int rc = RTR3InitExeNoArguments(0);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    int cErrors = 0;
    for (int i = 0; i < argc; i++)
        if (strcmp(argv[i], g_apszArgs4[i]))
        {
            RTStrmPrintf(g_pStdErr,
                         "child4: argv[%2u]='%s'\n"
                         "child4: expected='%s'\n",
                         i, argv[i], g_apszArgs4[i]);
            cErrors++;
        }

    return cErrors == 0 ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

static void tstRTCreateProcEx4(const char *pszAsUser, const char *pszPassword)
{
    RTTestISub("Argument with spaces and stuff");

    RTPROCESS hProc;
    RTTESTI_CHECK_RC_RETV(RTProcCreateEx(g_szExecName, g_apszArgs4, RTENV_DEFAULT, 0 /*fFlags*/, NULL,
                                         NULL, NULL, pszAsUser, pszPassword, &hProc), VINF_SUCCESS);
    RTPROCSTATUS ProcStatus = { -1, RTPROCEXITREASON_ABEND };
    RTTESTI_CHECK_RC(RTProcWait(hProc, RTPROCWAIT_FLAGS_BLOCK, &ProcStatus), VINF_SUCCESS);

    if (ProcStatus.enmReason != RTPROCEXITREASON_NORMAL || ProcStatus.iStatus != 0)
        RTTestIFailed("enmReason=%d iStatus=%d", ProcStatus.enmReason, ProcStatus.iStatus);
    else
        RTTestIPassed(NULL);
}


static int tstRTCreateProcEx3Child(void)
{
    int rc = RTR3InitExeNoArguments(0);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    RTStrmPrintf(g_pStdOut, "w"); RTStrmFlush(g_pStdOut);
    RTStrmPrintf(g_pStdErr, "o"); RTStrmFlush(g_pStdErr);
    RTStrmPrintf(g_pStdOut, "r"); RTStrmFlush(g_pStdOut);
    RTStrmPrintf(g_pStdErr, "k"); RTStrmFlush(g_pStdErr);
    RTStrmPrintf(g_pStdOut, "s");

    return RTEXITCODE_SUCCESS;
}

static void tstRTCreateProcEx3(const char *pszAsUser, const char *pszPassword)
{
    RTTestISub("Standard Out+Err");

    RTPIPE hPipeR, hPipeW;
    RTTESTI_CHECK_RC_RETV(RTPipeCreate(&hPipeR, &hPipeW, RTPIPE_C_INHERIT_WRITE), VINF_SUCCESS);
    const char * apszArgs[3] =
    {
        "non-existing-non-executable-file",
        "--testcase-child-3",
        NULL
    };
    RTHANDLE Handle;
    Handle.enmType = RTHANDLETYPE_PIPE;
    Handle.u.hPipe = hPipeW;
    RTPROCESS hProc;
    RTTESTI_CHECK_RC_RETV(RTProcCreateEx(g_szExecName, apszArgs, RTENV_DEFAULT, 0 /*fFlags*/, NULL,
                                         &Handle, &Handle, pszAsUser, pszPassword, &hProc), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTPipeClose(hPipeW), VINF_SUCCESS);

    char    szOutput[_4K];
    size_t  offOutput = 0;
    for (;;)
    {
        size_t cbLeft = sizeof(szOutput) - 1 - offOutput;
        RTTESTI_CHECK(cbLeft > 0);
        if (cbLeft == 0)
            break;

        size_t cbRead;
        int rc = RTPipeReadBlocking(hPipeR, &szOutput[offOutput], cbLeft, &cbRead);
        if (RT_FAILURE(rc))
        {
            RTTESTI_CHECK_RC(rc, VERR_BROKEN_PIPE);
            break;
        }
        offOutput += cbRead;
    }
    szOutput[offOutput] = '\0';
    RTTESTI_CHECK_RC(RTPipeClose(hPipeR), VINF_SUCCESS);

    RTPROCSTATUS ProcStatus = { -1, RTPROCEXITREASON_ABEND };
    RTTESTI_CHECK_RC(RTProcWait(hProc, RTPROCWAIT_FLAGS_BLOCK, &ProcStatus), VINF_SUCCESS);
    RTThreadSleep(10);

    if (ProcStatus.enmReason != RTPROCEXITREASON_NORMAL || ProcStatus.iStatus != 0)
        RTTestIFailed("enmReason=%d iStatus=%d", ProcStatus.enmReason, ProcStatus.iStatus);
    else if (   offOutput != sizeof("works") - 1
             || strcmp(szOutput, "works"))
        RTTestIFailed("wrong output: \"%s\" (len=%u)", szOutput, offOutput);
    else
        RTTestIPassed(NULL);
}


static int tstRTCreateProcEx2Child(void)
{
    int rc = RTR3InitExeNoArguments(0);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    RTStrmPrintf(g_pStdErr, "howdy");
    RTStrmPrintf(g_pStdOut, "ignore this output\n");

    return RTEXITCODE_SUCCESS;
}

static void tstRTCreateProcEx2(const char *pszAsUser, const char *pszPassword)
{
    RTTestISub("Standard Err");

    RTPIPE hPipeR, hPipeW;
    RTTESTI_CHECK_RC_RETV(RTPipeCreate(&hPipeR, &hPipeW, RTPIPE_C_INHERIT_WRITE), VINF_SUCCESS);
    const char * apszArgs[3] =
    {
        "non-existing-non-executable-file",
        "--testcase-child-2",
        NULL
    };
    RTHANDLE Handle;
    Handle.enmType = RTHANDLETYPE_PIPE;
    Handle.u.hPipe = hPipeW;
    RTPROCESS hProc;
    RTTESTI_CHECK_RC_RETV(RTProcCreateEx(g_szExecName, apszArgs, RTENV_DEFAULT, 0 /*fFlags*/, NULL,
                                         NULL, &Handle, pszAsUser, pszPassword, &hProc), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTPipeClose(hPipeW), VINF_SUCCESS);

    char    szOutput[_4K];
    size_t  offOutput = 0;
    for (;;)
    {
        size_t cbLeft = sizeof(szOutput) - 1 - offOutput;
        RTTESTI_CHECK(cbLeft > 0);
        if (cbLeft == 0)
            break;

        size_t cbRead;
        int rc = RTPipeReadBlocking(hPipeR, &szOutput[offOutput], cbLeft, &cbRead);
        if (RT_FAILURE(rc))
        {
            RTTESTI_CHECK_RC(rc, VERR_BROKEN_PIPE);
            break;
        }
        offOutput += cbRead;
    }
    szOutput[offOutput] = '\0';
    RTTESTI_CHECK_RC(RTPipeClose(hPipeR), VINF_SUCCESS);

    RTPROCSTATUS ProcStatus = { -1, RTPROCEXITREASON_ABEND };
    RTTESTI_CHECK_RC(RTProcWait(hProc, RTPROCWAIT_FLAGS_BLOCK, &ProcStatus), VINF_SUCCESS);
    RTThreadSleep(10);

    if (ProcStatus.enmReason != RTPROCEXITREASON_NORMAL || ProcStatus.iStatus != 0)
        RTTestIFailed("enmReason=%d iStatus=%d", ProcStatus.enmReason, ProcStatus.iStatus);
    else if (   offOutput != sizeof("howdy") - 1
             || strcmp(szOutput, "howdy"))
        RTTestIFailed("wrong output: \"%s\" (len=%u)", szOutput, offOutput);
    else
        RTTestIPassed(NULL);
}


static int tstRTCreateProcEx1Child(void)
{
    int rc = RTR3InitExeNoArguments(0);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    RTPrintf("it works");
    RTStrmPrintf(g_pStdErr, "ignore this output\n");

    return RTEXITCODE_SUCCESS;
}


static void tstRTCreateProcEx1(const char *pszAsUser, const char *pszPassword)
{
    RTTestISub("Standard Out");

    RTPIPE hPipeR, hPipeW;
    RTTESTI_CHECK_RC_RETV(RTPipeCreate(&hPipeR, &hPipeW, RTPIPE_C_INHERIT_WRITE), VINF_SUCCESS);
    const char * apszArgs[3] =
    {
        "non-existing-non-executable-file",
        "--testcase-child-1",
        NULL
    };
    RTHANDLE Handle;
    Handle.enmType = RTHANDLETYPE_PIPE;
    Handle.u.hPipe = hPipeW;
    RTPROCESS hProc;
    RTTESTI_CHECK_RC_RETV(RTProcCreateEx(g_szExecName, apszArgs, RTENV_DEFAULT, 0 /*fFlags*/, NULL,
                                         &Handle, NULL, pszAsUser, pszPassword, &hProc), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTPipeClose(hPipeW), VINF_SUCCESS);

    char    szOutput[_4K];
    size_t  offOutput = 0;
    for (;;)
    {
        size_t cbLeft = sizeof(szOutput) - 1 - offOutput;
        RTTESTI_CHECK(cbLeft > 0);
        if (cbLeft == 0)
            break;

        size_t cbRead;
        int rc = RTPipeReadBlocking(hPipeR, &szOutput[offOutput], cbLeft, &cbRead);
        if (RT_FAILURE(rc))
        {
            RTTESTI_CHECK_RC(rc, VERR_BROKEN_PIPE);
            break;
        }
        offOutput += cbRead;
    }
    szOutput[offOutput] = '\0';
    RTTESTI_CHECK_RC(RTPipeClose(hPipeR), VINF_SUCCESS);

    RTPROCSTATUS ProcStatus = { -1, RTPROCEXITREASON_ABEND };
    RTTESTI_CHECK_RC(RTProcWait(hProc, RTPROCWAIT_FLAGS_BLOCK, &ProcStatus), VINF_SUCCESS);

    if (ProcStatus.enmReason != RTPROCEXITREASON_NORMAL || ProcStatus.iStatus != 0)
        RTTestIFailed("enmReason=%d iStatus=%d", ProcStatus.enmReason, ProcStatus.iStatus);
    else if (   offOutput != sizeof("it works") - 1
             || strcmp(szOutput, "it works"))
        RTTestIFailed("wrong output: \"%s\" (len=%u)", szOutput, offOutput);
    else
        RTTestIPassed(NULL);
}


int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "--testcase-child-1"))
        return tstRTCreateProcEx1Child();
    if (argc == 2 && !strcmp(argv[1], "--testcase-child-2"))
        return tstRTCreateProcEx2Child();
    if (argc == 2 && !strcmp(argv[1], "--testcase-child-3"))
        return tstRTCreateProcEx3Child();
    if (argc >= 5 && !strcmp(argv[1], "--testcase-child-4"))
        return tstRTCreateProcEx4Child(argc, argv);
    if (argc == 2 && !strcmp(argv[1], "--testcase-child-5"))
        return tstRTCreateProcEx5Child(argc, argv);
    const char *pszAsUser   = NULL;
    const char *pszPassword = NULL;
    if (argc != 1)
    {
        if (argc != 4 || strcmp(argv[1], "--as-user"))
            return 99;
        pszAsUser   = argv[2];
        pszPassword = argv[3];
    }

    RTTEST hTest;
    int rc = RTTestInitAndCreate("tstRTProcCreateEx", &hTest);
    if (rc)
        return rc;
    RTTestBanner(hTest);

    if (!RTProcGetExecutablePath(g_szExecName, sizeof(g_szExecName)))
        RTStrCopy(g_szExecName, sizeof(g_szExecName), argv[0]);

    /*
     * The tests.
     */
    tstRTCreateProcEx1(pszAsUser, pszPassword);
    tstRTCreateProcEx2(pszAsUser, pszPassword);
    tstRTCreateProcEx3(pszAsUser, pszPassword);
    tstRTCreateProcEx4(pszAsUser, pszPassword);
    if (pszAsUser)
        tstRTCreateProcEx5(pszAsUser, pszPassword);
    /** @todo Cover files, ++ */

    /*
     * Summary.
     */
    return RTTestSummaryAndDestroy(hTest);
}

