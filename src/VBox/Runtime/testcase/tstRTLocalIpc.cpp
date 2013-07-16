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
#include <iprt/mem.h>
#include <iprt/path.h>
#include <iprt/process.h>
#include <iprt/rand.h>
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
    RTTestSub(hTest, "testServerListenAndCancel");

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
        RTTestPrintf(pCtx->hTest, RTTESTLVL_DEBUG, "testSessionConnectionThread: Listening returned with rc=%Rrc\n", rc);
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
    RTTestSub(hTest, "testSessionConnection");

    RTLOCALIPCSERVER ipcServer;
    int rc = RTLocalIpcServerCreate(&ipcServer, "tstRTLocalIpcSessionConnection",
                                    RTLOCALIPC_FLAGS_MULTI_SESSION);
    if (RT_SUCCESS(rc))
    {
#ifndef VBOX_TESTCASES_WITH_NO_THREADING
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
                RTTEST_CHECK_RC_BREAK(hTest, threadRc,  VERR_CANCELLED);
                RTTestPrintf(hTest, RTTESTLVL_INFO, "Server thread terminated successfully\n");
                RTTEST_CHECK_RC_BREAK(hTest, RTLocalIpcServerDestroy(ipcServer), VINF_SUCCESS);
                RTTEST_CHECK_BREAK(hTest, stsChild.enmReason == RTPROCEXITREASON_NORMAL);
                RTTEST_CHECK_BREAK(hTest, stsChild.iStatus == 0);
            }
            while (0);
        }
        else
            RTTestFailed(hTest, "Unable to create thread for cancelling server, rc=%Rrc\n", rc);
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
                RTTestFailed(hTest, "Error while listening, rc=%Rrc\n", rc);

        } while (0);
#endif
    }
    else
        RTTestFailed(hTest, "Unable to create IPC server, rc=%Rrc\n", rc);

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) testSessionWaitThread(RTTHREAD hSelf, void *pvUser)
{
    PLOCALIPCTHREADCTX pCtx = (PLOCALIPCTHREADCTX)pvUser;
    AssertPtr(pCtx);

    int rc;
    for (;;)
    {
        RTTestPrintf(pCtx->hTest, RTTESTLVL_INFO, "testSessionWaitThread: Listening for incoming connections ...\n");
        RTLOCALIPCSESSION ipcSession;
        rc = RTLocalIpcServerListen(pCtx->hServer, &ipcSession);
        if (RT_SUCCESS(rc))
        {
            RTTestPrintf(pCtx->hTest, RTTESTLVL_INFO, "testSessionWaitThread: Got new client connection, waiting a bit ...\n");
            RTThreadSleep(2000);
            rc = RTLocalIpcSessionClose(ipcSession);
        }
        else
        {
            RTTestPrintf(pCtx->hTest, RTTESTLVL_INFO, "testSessionWaitThread: Listening ended with rc=%Rrc\n", rc);
            break;
        }
    }

    RTTestPrintf(pCtx->hTest, RTTESTLVL_INFO, "testSessionWaitThread: Ended with rc=%Rrc\n", rc);
    return rc;
}

static RTEXITCODE testSessionWaitChild(int argc, char **argv, RTTEST hTest)
{
    do
    {
        RTThreadSleep(2000); /* Fudge. */
        RTLOCALIPCSESSION clientSession;
        RTTEST_CHECK_RC_BREAK(hTest, RTLocalIpcSessionConnect(&clientSession, "tstRTLocalIpcSessionWait",
                                                              0 /* Flags */), VINF_SUCCESS);
        RTTEST_CHECK_RC_BREAK(hTest, RTLocalIpcSessionWaitForData(clientSession, 100       /* 100ms timeout */),
                                                                  VERR_TIMEOUT);
        /* Next, try 60s timeout. Should be returning way earlier because the server closed the
         * connection after the first client connected. */
        RTTEST_CHECK_RC_BREAK(hTest, RTLocalIpcSessionWaitForData(clientSession, 60 * 1000),
                                                                  VERR_BROKEN_PIPE);
        /* Last try, also should fail because the server should be not around anymore. */
        RTTEST_CHECK_RC_BREAK(hTest, RTLocalIpcSessionWaitForData(clientSession, 5 * 1000),
                                                                  VERR_BROKEN_PIPE);
        RTTEST_CHECK_RC_BREAK(hTest, RTLocalIpcSessionClose(clientSession), VINF_SUCCESS);

    } while (0);

    return !RTTestErrorCount(hTest) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

static int testSessionWait(RTTEST hTest, const char *pszExecPath)
{
    RTTestSub(hTest, "testSessionWait");

    RTLOCALIPCSERVER ipcServer;
    int rc = RTLocalIpcServerCreate(&ipcServer, "tstRTLocalIpcSessionWait",
                                    RTLOCALIPC_FLAGS_MULTI_SESSION);
    if (RT_SUCCESS(rc))
    {
        LOCALIPCTHREADCTX threadCtx = { ipcServer, hTest };

        /* Spawn a simple worker thread and let it listen for incoming connections.
         * In the meanwhile we try to cancel the server and see what happens. */
        RTTHREAD hThread;
        rc = RTThreadCreate(&hThread, testSessionWaitThread,
                            &threadCtx, 0 /* Stack */, RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, "tstIpc3");
        if (RT_SUCCESS(rc))
        {
            do
            {
                RTPROCESS hProc;
                const char *apszArgs[4] = { pszExecPath, "child", "tstRTLocalIpcSessionWaitFork", NULL };
                RTTEST_CHECK_RC_BREAK(hTest, RTProcCreate(pszExecPath, apszArgs,
                                                          RTENV_DEFAULT, 0 /* fFlags*/, &hProc), VINF_SUCCESS);
                RTThreadSleep(5000); /* Let the server run for some time ... */
                RTTestPrintf(hTest, RTTESTLVL_INFO, "Cancelling server listening\n");
                RTTEST_CHECK_RC_BREAK(hTest, RTLocalIpcServerCancel(ipcServer), VINF_SUCCESS);
                /* Wait for the server thread to terminate. */
                int threadRc;
                RTTEST_CHECK_RC(hTest, RTThreadWait(hThread,
                                                    30 * 1000 /* 30s timeout */, &threadRc), VINF_SUCCESS);
                RTTEST_CHECK_RC_BREAK(hTest, threadRc, VERR_CANCELLED);
                RTTEST_CHECK_RC(hTest, RTLocalIpcServerDestroy(ipcServer), VINF_SUCCESS);
                RTTestPrintf(hTest, RTTESTLVL_INFO, "Server thread terminated successfully\n");
                /* Check if the child ran successfully. */
                RTPROCSTATUS stsChild;
                RTTEST_CHECK_RC_BREAK(hTest, RTProcWait(hProc, RTPROCWAIT_FLAGS_BLOCK, &stsChild), VINF_SUCCESS);
                RTTestPrintf(hTest, RTTESTLVL_INFO, "Child terminated\n");
                RTTEST_CHECK_BREAK(hTest, stsChild.enmReason == RTPROCEXITREASON_NORMAL);
                RTTEST_CHECK_BREAK(hTest, stsChild.iStatus == 0);
            }
            while (0);
        }
        else
            RTTestFailed(hTest, "Unable to create thread for cancelling server, rc=%Rrc\n", rc);
    }
    else
        RTTestFailed(hTest, "Unable to create IPC server, rc=%Rrc\n", rc);

    return VINF_SUCCESS;
}

/**
 * Simple structure holding the test IPC messages.
 */
typedef struct LOCALIPCTESTMSG
{
    /** The actual message. */
    char szOp[255];
} LOCALIPCTESTMSG, *PLOCALIPCTESTMSG;

static int testSessionDataReadTestMsg(RTTEST hTest, RTLOCALIPCSESSION hSession,
                                      void *pvBuffer, size_t cbBuffer, const char *pszMsg)
{
    AssertPtrReturn(pvBuffer, VERR_INVALID_POINTER);
    AssertPtrReturn(pszMsg, VERR_INVALID_POINTER);

    void *pvBufCur = pvBuffer;
    size_t cbReadTotal = 0;
    for (;;)
    {
        size_t cbRead = RTRandU32Ex(1, sizeof(LOCALIPCTESTMSG) - cbReadTotal); /* Force a bit of fragmentation. */
        RTTEST_CHECK_BREAK(hTest, cbRead);
        RTTEST_CHECK_RC_BREAK(hTest, RTLocalIpcSessionRead(hSession, pvBufCur,
                                                           cbBuffer,
                                                           &cbRead), VINF_SUCCESS);
        RTTEST_CHECK_BREAK(hTest, cbRead);
        pvBufCur     = (uint8_t *)pvBufCur + cbRead; /* Advance. */
        cbReadTotal += cbRead;
        RTTEST_CHECK_BREAK(hTest, cbReadTotal <= cbBuffer);
        if (cbReadTotal >= sizeof(LOCALIPCTESTMSG)) /* Got a complete test message? */
        {
            RTTEST_CHECK_BREAK(hTest, cbReadTotal == sizeof(LOCALIPCTESTMSG));
            PLOCALIPCTESTMSG pMsg = (PLOCALIPCTESTMSG)pvBuffer;
            RTTEST_CHECK_BREAK(hTest, pMsg != NULL);
            RTTEST_CHECK_BREAK(hTest, !RTStrCmp(pMsg->szOp, pszMsg));
            break;
        }
        /* Try receiving next part of the message in another round. */
    }

    return !RTTestErrorCount(hTest) ? VINF_SUCCESS : VERR_GENERAL_FAILURE /* Doesn't matter */;
}

static int testSessionDataThreadWorker(PLOCALIPCTHREADCTX pCtx)
{
    AssertPtr(pCtx);

    size_t cbScratchBuf = _1K; /** @todo Make this random in future. */
    uint8_t *pvScratchBuf = (uint8_t*)RTMemAlloc(cbScratchBuf);
    RTTEST_CHECK_RET(pCtx->hTest, pvScratchBuf != NULL, VERR_NO_MEMORY);

    do
    {
        /* Note: At the moment we only support one client per run. */
        RTTestPrintf(pCtx->hTest, RTTESTLVL_INFO, "testSessionDataThread: Listening for incoming connections ...\n");
        RTLOCALIPCSESSION hSession;
        RTTEST_CHECK_RC_BREAK(pCtx->hTest, RTLocalIpcServerListen(pCtx->hServer, &hSession), VINF_SUCCESS);
        RTTestPrintf(pCtx->hTest, RTTESTLVL_INFO, "testSessionDataThread: Got new client connection\n");
        uint32_t cRounds = 256; /** @todo Use RTRand(). */
        /* Write how many rounds we're going to send data. */
        RTTEST_CHECK_RC_BREAK(pCtx->hTest, RTLocalIpcSessionWrite(hSession, &cRounds, sizeof(cRounds)), VINF_SUCCESS);
        RTTestPrintf(pCtx->hTest, RTTESTLVL_INFO, "testSessionDataThread: Written number of rounds\n");
        for (uint32_t i = 0; i < cRounds; i++)
        {
            LOCALIPCTESTMSG msg;
            RTTEST_CHECK_BREAK(pCtx->hTest, RTStrPrintf(msg.szOp, sizeof(msg.szOp),
                                                        "YayIGotRound%RU32FromTheServer", i) > 0);
            RTTEST_CHECK_RC_BREAK(pCtx->hTest, RTLocalIpcSessionWrite(hSession, &msg, sizeof(msg)), VINF_SUCCESS);
        }
        if (!RTTestErrorCount(pCtx->hTest))
            RTTestPrintf(pCtx->hTest, RTTESTLVL_INFO, "testSessionDataThread: Data successfully written\n");
        /* Try to receive the same amount of rounds from the client. */
        for (uint32_t i = 0; i < cRounds; i++)
        {
            RTTEST_CHECK_RC_BREAK(pCtx->hTest, RTLocalIpcSessionWaitForData(hSession, RT_INDEFINITE_WAIT),
                                  VINF_SUCCESS);
            char szMsg[32];
            RTTEST_CHECK_BREAK(pCtx->hTest, RTStrPrintf(szMsg, sizeof(szMsg), "YayIGotRound%RU32FromTheClient", i) > 0);
            RTTEST_CHECK_RC_BREAK(pCtx->hTest, testSessionDataReadTestMsg(pCtx->hTest, hSession,
                                                                          pvScratchBuf, cbScratchBuf,
                                                                          szMsg), VINF_SUCCESS);
            if (RTTestErrorCount(pCtx->hTest))
                break;
        }
        if (!RTTestErrorCount(pCtx->hTest))
            RTTestPrintf(pCtx->hTest, RTTESTLVL_INFO, "testSessionDataThread: Data successfully read\n");
        RTTEST_CHECK_RC_BREAK(pCtx->hTest, RTLocalIpcSessionClose(hSession), VINF_SUCCESS);

    } while (0);

    RTMemFree(pvScratchBuf);
    return !RTTestErrorCount(pCtx->hTest) ? VINF_SUCCESS : VERR_GENERAL_FAILURE /* Doesn't matter */;
}

static DECLCALLBACK(int) testSessionDataThread(RTTHREAD hSelf, void *pvUser)
{
    PLOCALIPCTHREADCTX pCtx = (PLOCALIPCTHREADCTX)pvUser;
    AssertPtr(pCtx);

    return testSessionDataThreadWorker(pCtx);
}

static int testSessionDataChildWorker(RTTEST hTest)
{
    size_t cbScratchBuf = _1K; /** @todo Make this random in future. */
    uint8_t *pvScratchBuf = (uint8_t*)RTMemAlloc(cbScratchBuf);
    RTTEST_CHECK_RET(hTest, pvScratchBuf != NULL, RTEXITCODE_FAILURE);

    do
    {
        RTThreadSleep(2000); /* Fudge. */
        RTLOCALIPCSESSION hSession;
        RTTEST_CHECK_RC_BREAK(hTest, RTLocalIpcSessionConnect(&hSession, "tstRTLocalIpcSessionData",
                                                              0 /* Flags */), VINF_SUCCESS);
        /* Get number of rounds we want to read/write. */
        uint32_t cRounds = 0;
        RTTEST_CHECK_RC_BREAK(hTest, RTLocalIpcSessionWaitForData(hSession, RT_INDEFINITE_WAIT),
                                                                  VINF_SUCCESS);
        RTTEST_CHECK_RC_BREAK(hTest, RTLocalIpcSessionRead(hSession, &cRounds, sizeof(cRounds),
                                                           NULL /* Get exactly sizeof(cRounds) bytes */), VINF_SUCCESS);
        RTTEST_CHECK_BREAK(hTest, cRounds == 256); /** @todo Check for != 0 when using RTRand(). */
        /* Receive all rounds. */
        for (uint32_t i = 0; i < cRounds; i++)
        {
            RTTEST_CHECK_RC_BREAK(hTest, RTLocalIpcSessionWaitForData(hSession, RT_INDEFINITE_WAIT),
                                                                      VINF_SUCCESS);
            char szMsg[32];
            RTTEST_CHECK_BREAK(hTest, RTStrPrintf(szMsg, sizeof(szMsg), "YayIGotRound%RU32FromTheServer", i) > 0);
            RTTEST_CHECK_RC_BREAK(hTest, testSessionDataReadTestMsg(hTest, hSession,
                                                                    pvScratchBuf, cbScratchBuf,
                                                                    szMsg), VINF_SUCCESS);
            if (RTTestErrorCount(hTest))
                break;
        }
        /* Send all rounds back to the server. */
        for (uint32_t i = 0; i < cRounds; i++)
        {
            LOCALIPCTESTMSG msg;
            RTTEST_CHECK_BREAK(hTest, RTStrPrintf(msg.szOp, sizeof(msg.szOp),
                                                  "YayIGotRound%RU32FromTheClient", i) > 0);
            RTTEST_CHECK_RC_BREAK(hTest, RTLocalIpcSessionWrite(hSession, &msg, sizeof(msg)), VINF_SUCCESS);
        }
        RTTEST_CHECK_RC_BREAK(hTest, RTLocalIpcSessionClose(hSession), VINF_SUCCESS);

    } while (0);

    RTMemFree(pvScratchBuf);
    return !RTTestErrorCount(hTest) ? VINF_SUCCESS : VERR_GENERAL_FAILURE /* Doesn't matter */;
}

static DECLCALLBACK(int) testSessionDataChildAsThread(RTTHREAD hSelf, void *pvUser)
{
    PRTTEST phTest = (PRTTEST)pvUser;
    AssertPtr(phTest);
    return testSessionDataChildWorker(*phTest);
}

static RTEXITCODE testSessionDataChild(int argc, char **argv, RTTEST hTest)
{
    return RT_SUCCESS(testSessionDataChildWorker(hTest)) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

static int testSessionData(RTTEST hTest, const char *pszExecPath)
{
    RTTestSub(hTest, "testSessionData");

    RTLOCALIPCSERVER ipcServer;
    int rc = RTLocalIpcServerCreate(&ipcServer, "tstRTLocalIpcSessionData",
                                    RTLOCALIPC_FLAGS_MULTI_SESSION);
    if (RT_SUCCESS(rc))
    {
        LOCALIPCTHREADCTX threadCtx = { ipcServer, hTest };
#if 0
        /* Run server + client in threads instead of fork'ed processes (useful for debugging). */
        RTTHREAD hThreadServer, hThreadClient;
        rc = RTThreadCreate(&hThreadServer, testSessionDataThread,
                            &threadCtx, 0 /* Stack */, RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, "tstIpc4");
        if (RT_SUCCESS(rc))
            rc = RTThreadCreate(&hThreadClient, testSessionDataChildAsThread,
                                &hTest, 0 /* Stack */, RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, "tstIpc5");
        if (RT_SUCCESS(rc))
        {
            do
            {
                int threadRc;
                RTTEST_CHECK_RC(hTest, RTThreadWait(hThreadServer,
                                                    5 * 60 * 1000 /* 5 minutes timeout */, &threadRc), VINF_SUCCESS);
                RTTEST_CHECK_RC_BREAK(hTest, threadRc, VINF_SUCCESS);
                RTTEST_CHECK_RC(hTest, RTThreadWait(hThreadClient,
                                                    5 * 60 * 1000 /* 5 minutes timeout */, &threadRc), VINF_SUCCESS);
                RTTEST_CHECK_RC_BREAK(hTest, threadRc, VINF_SUCCESS);

            } while (0);
        }
#else
        /* Spawn a simple worker thread and let it listen for incoming connections.
         * In the meanwhile we try to cancel the server and see what happens. */
        RTTHREAD hThread;
        rc = RTThreadCreate(&hThread, testSessionDataThread,
                            &threadCtx, 0 /* Stack */, RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, "tstIpc4");
        if (RT_SUCCESS(rc))
        {
            do
            {
                RTPROCESS hProc;
                const char *apszArgs[4] = { pszExecPath, "child", "tstRTLocalIpcSessionDataFork", NULL };
                RTTEST_CHECK_RC_BREAK(hTest, RTProcCreate(pszExecPath, apszArgs,
                                                          RTENV_DEFAULT, 0 /* fFlags*/, &hProc), VINF_SUCCESS);
                /* Wait for the server thread to terminate. */
                int threadRc;
                RTTEST_CHECK_RC(hTest, RTThreadWait(hThread,
                                                    5 * 60 * 1000 /* 5 minutes timeout */, &threadRc), VINF_SUCCESS);
                RTTEST_CHECK_RC_BREAK(hTest, threadRc, VINF_SUCCESS);
                RTTEST_CHECK_RC(hTest, RTLocalIpcServerDestroy(ipcServer), VINF_SUCCESS);
                RTTestPrintf(hTest, RTTESTLVL_INFO, "Server thread terminated successfully\n");
                /* Check if the child ran successfully. */
                RTPROCSTATUS stsChild;
                RTTEST_CHECK_RC_BREAK(hTest, RTProcWait(hProc, RTPROCWAIT_FLAGS_BLOCK, &stsChild), VINF_SUCCESS);
                RTTestPrintf(hTest, RTTESTLVL_INFO, "Child terminated\n");
                RTTEST_CHECK_BREAK(hTest, stsChild.enmReason == RTPROCEXITREASON_NORMAL);
                RTTEST_CHECK_BREAK(hTest, stsChild.iStatus == 0);
            }
            while (0);
        }
        else
            RTTestFailed(hTest, "Unable to create thread for cancelling server, rc=%Rrc\n", rc);
#endif
    }
    else
        RTTestFailed(hTest, "Unable to create IPC server, rc=%Rrc\n", rc);

    return !RTTestErrorCount(hTest) ? VINF_SUCCESS : VERR_GENERAL_FAILURE /* Doesn't matter */;
}

static RTEXITCODE mainChild(int argc, char **argv)
{
    if (argc < 3) /* Safety first. */
        return RTEXITCODE_FAILURE;
    /* Note: We assume argv[2] always contains the actual test type to perform. */
    RTTEST hTest;
    RTEXITCODE rcExit = RTTestInitAndCreate(argv[2], &hTest);
    if (rcExit)
        return rcExit;
    RTTestBanner(hTest);

    RTAssertSetMayPanic(false);
#ifdef DEBUG_andy
    RTAssertSetQuiet(false);
#endif

    if (!RTStrICmp(argv[2], "tstRTLocalIpcSessionConnectionFork"))
        rcExit = testSessionConnectionChild(argc, argv, hTest);
    else if (!RTStrICmp(argv[2], "tstRTLocalIpcSessionWaitFork"))
        rcExit = testSessionWaitChild(argc, argv, hTest);
    else if (!RTStrICmp(argv[2], "tstRTLocalIpcSessionDataFork"))
        rcExit = testSessionDataChild(argc, argv, hTest);

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
    RTAssertSetQuiet(false);
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
        RTTESTI_CHECK_RC_RET(testServerListenAndCancel(hTest, szExecPath), VINF_SUCCESS, 1);
        RTTESTI_CHECK_RC_RET(testSessionConnection(hTest, szExecPath), VINF_SUCCESS, 1);
        RTTESTI_CHECK_RC_RET(testSessionWait(hTest, szExecPath), VINF_SUCCESS, 1);
        RTTESTI_CHECK_RC_RET(testSessionData(hTest, szExecPath), VINF_SUCCESS, 1);
    }

    /*
     * Summary.
     */
    return RTTestSummaryAndDestroy(hTest);
}

