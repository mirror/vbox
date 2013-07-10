/* $Id$ */
/** @file
 * IPRT Testcase - RTLocalIpc.
 */

/*
 * Copyright (C) 2013 Oracle Corporation
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
#include <iprt/env.h>
#include <iprt/localipc.h>
#include <iprt/path.h>
#include <iprt/process.h>
#include <iprt/string.h>
#include <iprt/test.h>
#include <iprt/thread.h>
#include <iprt/time.h>


typedef struct LOCALIPCTHREADCTX
{
    /** The IPC server handle. */
    RTLOCALIPCSERVER hServer;
    /** The test handle. */
    RTTEST hTest;
} LOCALIPCTHREADCTX, *PLOCALIPCTHREADCTX;

static int testServerListenAndCancel2(const char *pszExecPath)
{
    const char *apszArgs[4] = { pszExecPath, "child", "testServerListenAndCancel", NULL };
    RTPROCESS hProc;
    int rc = RTProcCreate(pszExecPath, apszArgs, RTENV_DEFAULT, 0 /* fFlags*/, &hProc);

    return rc;
}

static DECLCALLBACK(int) testServerListenAndCancelThread(RTTHREAD hSelf, void *pvUser)
{
    PRTLOCALIPCSERVER pServer = (PRTLOCALIPCSERVER)pvUser;
    AssertPtr(pServer);

    RTThreadSleep(5000); /* Wait a bit to simulate waiting in main thread. */

    int rc = RTLocalIpcServerCancel(*pServer);
    AssertRC(rc);

    return 0;
}

static int testServerListenAndCancel(RTTEST hTest, const char *pszExecPath)
{
    RTTestISub("testServerListenAndCancel");

    RTLOCALIPCSERVER ipcServer;
    int rc = RTLocalIpcServerCreate(&ipcServer, "testServerListenAndCancel",
                                    RTLOCALIPC_FLAGS_MULTI_SESSION);
    if (RT_SUCCESS(rc))
    {
        /* Spawn a simple worker thread and let it listen for incoming connections.
         * In the meanwhile we try to cancel the server and see what happens. */
        RTTHREAD hThread;
        rc = RTThreadCreate(&hThread, testServerListenAndCancelThread,
                            &ipcServer, 0 /* Stack */, RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, "tstIpc1");
        if (RT_SUCCESS(rc))
        {
            do
            {
                RTTestPrintf(hTest, RTTESTLVL_INFO, "Listening for incoming connections ...\n");
                RTLOCALIPCSESSION ipcSession;
                RTTEST_CHECK_RC_BREAK(hTest, RTLocalIpcServerListen(ipcServer, &ipcSession), VERR_CANCELLED);
                RTTEST_CHECK_RC_BREAK(hTest, RTLocalIpcServerCancel(ipcServer), VINF_SUCCESS);
                RTTEST_CHECK_RC_BREAK(hTest, RTLocalIpcServerDestroy(ipcServer), VINF_SUCCESS);

                RTTestPrintf(hTest, RTTESTLVL_INFO, "Waiting for thread to exit ...\n");
                RTTEST_CHECK_RC(hTest, RTThreadWait(hThread,
                                                    30 * 1000 /* 30s timeout */, NULL), VINF_SUCCESS);
            } while (0);
        }
        else
            RTTestIFailed("Unable to create thread for cancelling server, rc=%Rrc\n", rc);
    }
    else
        RTTestIFailed("Unable to create IPC server, rc=%Rrc\n", rc);

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) testSessionConnectionThread(RTTHREAD hSelf, void *pvUser)
{
    PLOCALIPCTHREADCTX pCtx = (PLOCALIPCTHREADCTX)pvUser;
    AssertPtr(pCtx);

    int rc;
    RTTestPrintf(pCtx->hTest, RTTESTLVL_INFO, "testSessionConnectionThread: Listening for incoming connections ...\n");
    for (;;)
    {
        RTLOCALIPCSESSION ipcSession;
        rc = RTLocalIpcServerListen(pCtx->hServer, &ipcSession);
#ifdef DEBUG_andy
        RTTestPrintf(pCtx->hTest, RTTESTLVL_INFO, "testSessionConnectionThread: Listening returned with rc=%Rrc\n", rc);
#endif
        if (RT_SUCCESS(rc))
        {
            RTTestPrintf(pCtx->hTest, RTTESTLVL_INFO, "testSessionConnectionThread: Got new client connection\n");
        }
        else
            break;
    }

    RTTestPrintf(pCtx->hTest, RTTESTLVL_INFO, "testSessionConnectionThread: Ended with rc=%Rrc\n", rc);
    return rc;
}

static RTEXITCODE testSessionConnectionChild(int argc, char **argv, RTTEST hTest)
{
    do
    {
        RTThreadSleep(2000); /* Fudge */
        RTLOCALIPCSESSION clientSession;
        RTTEST_CHECK_RC_BREAK(hTest, RTLocalIpcSessionConnect(&clientSession, "tstRTLocalIpcSessionConnection",
                                                              0 /* Flags */), VINF_SUCCESS);
        RTThreadSleep(5000); /* Fudge */
        RTTEST_CHECK_RC_BREAK(hTest, RTLocalIpcSessionClose(clientSession), VINF_SUCCESS);

    } while (0);

    return !RTTestErrorCount(hTest) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

static int testSessionConnection(RTTEST hTest, const char *pszExecPath)
{
    RTTestISub("testSessionConnection");

    RTLOCALIPCSERVER ipcServer;
    int rc = RTLocalIpcServerCreate(&ipcServer, "tstRTLocalIpcSessionConnection",
                                    RTLOCALIPC_FLAGS_MULTI_SESSION);
    if (RT_SUCCESS(rc))
    {
#if 0
        LOCALIPCTHREADCTX threadCtx = { ipcServer, hTest };

        /* Spawn a simple worker thread and let it listen for incoming connections.
         * In the meanwhile we try to cancel the server and see what happens. */
        RTTHREAD hThread;
        rc = RTThreadCreate(&hThread, testSessionConnectionThread,
                            &threadCtx, 0 /* Stack */, RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, "tstIpc2");
        if (RT_SUCCESS(rc))
        {
            do
            {
                RTPROCESS hProc;
                const char *apszArgs[4] = { pszExecPath, "child", "tstRTLocalIpcSessionConnectionFork", NULL };
                RTTEST_CHECK_RC_BREAK(hTest, RTProcCreate(pszExecPath, apszArgs,
                                                          RTENV_DEFAULT, 0 /* fFlags*/, &hProc), VINF_SUCCESS);
                RTPROCSTATUS stsChild;
                RTTEST_CHECK_RC_BREAK(hTest, RTProcWait(hProc, RTPROCWAIT_FLAGS_BLOCK, &stsChild), VINF_SUCCESS);
                RTTestPrintf(hTest, RTTESTLVL_INFO, "Child terminated, waiting for server thread ...\n");
                RTTEST_CHECK_RC_BREAK(hTest, RTLocalIpcServerCancel(ipcServer), VINF_SUCCESS);
                int threadRc;
                RTTEST_CHECK_RC(hTest, RTThreadWait(hThread,
                                                    30 * 1000 /* 30s timeout */, &threadRc), VINF_SUCCESS);
                RTTEST_CHECK_RC_BREAK(hTest, threadRc,  VINF_SUCCESS);
                RTTestPrintf(hTest, RTTESTLVL_INFO, "Server thread terminated successfully\n");
                RTTEST_CHECK_RC_BREAK(hTest, RTLocalIpcServerDestroy(ipcServer),  VINF_SUCCESS);
                RTTEST_CHECK_BREAK(hTest, stsChild.enmReason == RTPROCEXITREASON_NORMAL);
                RTTEST_CHECK_BREAK(hTest, stsChild.iStatus == 0);
            }
            while (0);
        }
        else
            RTTestIFailed("Unable to create thread for cancelling server, rc=%Rrc\n", rc);
#else
        do
        {
            RTPROCESS hProc;
            const char *apszArgs[4] = { pszExecPath, "child", "tstRTLocalIpcSessionConnectionFork", NULL };
            RTTEST_CHECK_RC_BREAK(hTest, RTProcCreate(pszExecPath, apszArgs,
                                                      RTENV_DEFAULT, 0 /* fFlags*/, &hProc), VINF_SUCCESS);
            RTLOCALIPCSESSION ipcSession;
            rc = RTLocalIpcServerListen(ipcServer, &ipcSession);
            if (RT_SUCCESS(rc))
            {
                RTTestPrintf(hTest, RTTESTLVL_INFO, "testSessionConnectionThread: Got new client connection\n");
            }
            else
                RTTestIFailed("Error while listening, rc=%Rrc\n", rc);

        } while (0);
#endif
    }
    else
        RTTestIFailed("Unable to create IPC server, rc=%Rrc\n", rc);

    return VINF_SUCCESS;
}

static RTEXITCODE mainChild(int argc, char **argv)
{
    /* Note: We assume argv[2] always contains the actual test type to perform. */
    RTTEST hTest;
    RTEXITCODE rcExit = RTTestInitAndCreate(argv[2], &hTest);
    if (rcExit)
        return rcExit;
    RTTestBanner(hTest);

    if (!RTStrICmp(argv[2], "tstRTLocalIpcSessionConnectionFork"))
        rcExit = testSessionConnectionChild(argc, argv, hTest);

    return RTTestSummaryAndDestroy(hTest);
}

int main(int argc, char **argv)
{
    if (   argc > 2
        && !RTStrICmp(argv[1], "child"))
        return mainChild(argc, argv);

    RTTEST hTest;
    RTEXITCODE rcExit = RTTestInitAndCreate("tstRTLocalIpc", &hTest);
    if (rcExit)
        return rcExit;
    RTTestBanner(hTest);

    char szExecPath[RTPATH_MAX];
    if (!RTProcGetExecutablePath(szExecPath, sizeof(szExecPath)))
        RTStrCopy(szExecPath, sizeof(szExecPath), argv[0]);

    RTTestISub("Basics");

    RTAssertSetMayPanic(false);
#ifdef DEBUG_andy
    RTAssertSetQuiet(true);
#endif

    /* Server-side. */
    RTTESTI_CHECK_RC_RET(RTLocalIpcServerCreate(NULL, NULL, 0), VERR_INVALID_POINTER, 1);
    RTLOCALIPCSERVER ipcServer;
    RTTESTI_CHECK_RC_RET(RTLocalIpcServerCreate(&ipcServer, NULL, 0), VERR_INVALID_POINTER, 1);
    RTTESTI_CHECK_RC_RET(RTLocalIpcServerCreate(&ipcServer, "", 0), VERR_INVALID_PARAMETER, 1);
    RTTESTI_CHECK_RC_RET(RTLocalIpcServerCreate(&ipcServer, "BasicTest", 0 /* Invalid flags */), VERR_INVALID_PARAMETER, 1);
    RTTESTI_CHECK_RC_RET(RTLocalIpcServerCreate(&ipcServer, "BasicTest", 1234 /* Invalid flags */), VERR_INVALID_PARAMETER, 1);
    RTTESTI_CHECK_RC_RET(RTLocalIpcServerCancel(NULL), VERR_INVALID_HANDLE, 1);
    RTTESTI_CHECK_RC_RET(RTLocalIpcServerDestroy(NULL), VINF_SUCCESS, 1);
    /* Basic server creation / destruction. */
    RTTESTI_CHECK_RC_RET(RTLocalIpcServerCreate(&ipcServer, "BasicTest", RTLOCALIPC_FLAGS_MULTI_SESSION), VINF_SUCCESS, 1);
    RTTESTI_CHECK_RC_RET(RTLocalIpcServerCancel(ipcServer), VINF_SUCCESS, 1);
    RTTESTI_CHECK_RC_RET(RTLocalIpcServerDestroy(ipcServer), VINF_SUCCESS, 1);

    /* Client-side (per session). */
    RTTESTI_CHECK_RC_RET(RTLocalIpcSessionConnect(NULL, NULL, 0), VERR_INVALID_POINTER, 1);
    RTLOCALIPCSESSION ipcSession;
    RTTESTI_CHECK_RC_RET(RTLocalIpcSessionConnect(&ipcSession, NULL, 0), VERR_INVALID_POINTER, 1);
    RTTESTI_CHECK_RC_RET(RTLocalIpcSessionConnect(&ipcSession, "", 0), VERR_INVALID_PARAMETER, 1);
    RTTESTI_CHECK_RC_RET(RTLocalIpcSessionConnect(&ipcSession, "BasicTest", 1234 /* Invalid flags */), VERR_INVALID_PARAMETER, 1);
    RTTESTI_CHECK_RC_RET(RTLocalIpcSessionCancel(NULL), VERR_INVALID_HANDLE, 1);
    RTTESTI_CHECK_RC_RET(RTLocalIpcSessionClose(NULL), VINF_SUCCESS, 1);
    /* Basic client creation / destruction. */
    RTTESTI_CHECK_RC_RET(RTLocalIpcSessionConnect(&ipcSession, "BasicTest", 0), VERR_FILE_NOT_FOUND, 1);
    RTTESTI_CHECK_RC_RET(RTLocalIpcServerCancel(ipcServer), VERR_INVALID_MAGIC, 1);
    RTTESTI_CHECK_RC_RET(RTLocalIpcServerDestroy(ipcServer), VERR_INVALID_MAGIC, 1);

    if (RTTestErrorCount(hTest) == 0)
    {
#ifndef DEBUG_andy
        RTTESTI_CHECK_RC_RET(testServerListenAndCancel(hTest, szExecPath), VINF_SUCCESS, 1);
#endif
        RTTESTI_CHECK_RC_RET(testSessionConnection(hTest, szExecPath), VINF_SUCCESS, 1);
    }

    /*
     * Summary.
     */
    return RTTestSummaryAndDestroy(hTest);
}

