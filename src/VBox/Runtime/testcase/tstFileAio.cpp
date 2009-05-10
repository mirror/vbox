/* $Id$ */
/** @file
 * IPRT Testcase - File Async I/O.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
#include <iprt/file.h>
#include <iprt/types.h>
#include <iprt/err.h>
#include <iprt/string.h>
#include <iprt/stream.h>
#include <iprt/mem.h>
#include <iprt/initterm.h>

/** @todo make configurable through cmd line. */
#define TSTFILEAIO_MAX_REQS_IN_FLIGHT 64
#define TSTFILEAIO_BUFFER_SIZE 64*_1K

/* Global error counter. */
int         cErrors = 0;

void tstFileAioTestReadWriteBasic(RTFILE File, bool fWrite, void *pvTestBuf, size_t cbTestBuf, size_t cbTestFile, uint32_t cMaxReqsInFlight)
{
    int rc = VINF_SUCCESS;
    RTFILEAIOCTX hAioContext;
    uint64_t NanoTS = RTTimeNanoTS();

    RTPrintf("tstFileAio: Starting simple %s test...\n", fWrite ? "write" : "read");

    rc = RTFileAioCtxCreate(&hAioContext, cMaxReqsInFlight);
    if (RT_SUCCESS(rc))
    {
        RTFILEAIOREQ    *paReqs  = NULL;
        void           **papvBuf = NULL;
        RTFOFF           Offset = 0;
        size_t           cbLeft = cbTestFile;
        int cRun = 0;

        /* Allocate request array. */
        paReqs = (PRTFILEAIOREQ)RTMemAllocZ(cMaxReqsInFlight * sizeof(RTFILEAIOREQ));
        if (paReqs)
        {
            /* Allocate array holding pointer to data buffers. */
            papvBuf = (void **)RTMemAllocZ(cMaxReqsInFlight * sizeof(void *));
            if (papvBuf)
            {
                /* Allocate array holding completed requests. */
                RTFILEAIOREQ *paReqsCompleted = NULL;
                paReqsCompleted = (PRTFILEAIOREQ)RTMemAllocZ(cMaxReqsInFlight * sizeof(RTFILEAIOREQ));
                if (paReqsCompleted)
                {
                    /* Associate file with context.*/
                    rc = RTFileAioCtxAssociateWithFile(hAioContext, File);
                    if (RT_SUCCESS(rc))
                    {
                        /* Initialize buffers. */
                        for (unsigned i = 0; i < cMaxReqsInFlight; i++)
                        {
                            papvBuf[i] = RTMemPageAllocZ(cbTestBuf);

                            if (fWrite)
                                memcpy(papvBuf[i], pvTestBuf, cbTestBuf);
                        }

                        /* Initialize requests. */
                        for (unsigned i = 0; i < cMaxReqsInFlight; i++)
                            RTFileAioReqCreate(&paReqs[i]);

                        while (cbLeft)
                        {
                            int cReqs = 0;

                            for (unsigned i = 0; i < cMaxReqsInFlight; i++)
                            {
                                size_t cbTransfer = (cbLeft < cbTestBuf) ? cbLeft : cbTestBuf;

                                if (!cbTransfer)
                                    break;

                                if (fWrite)
                                    rc = RTFileAioReqPrepareWrite(paReqs[i], File, Offset, papvBuf[i],
                                                                  cbTransfer, papvBuf[i]);
                                else
                                    rc = RTFileAioReqPrepareRead(paReqs[i], File, Offset, papvBuf[i],
                                                                 cbTransfer, papvBuf[i]);

                                cbLeft -= cbTransfer;
                                Offset += cbTransfer;
                                cReqs++;
                            }

                            rc = RTFileAioCtxSubmit(hAioContext, paReqs, cReqs);
                            if (RT_FAILURE(rc))
                            {
                                RTPrintf("tstFileAio: FATAL ERROR - Failed to submit tasks after %d runs. rc=%Rrc\n", cRun, rc);
                                cErrors++;
                                break;
                            }

                            /* Wait */
                            uint32_t cCompleted = 0;
                            rc = RTFileAioCtxWait(hAioContext, cReqs, RT_INDEFINITE_WAIT,
                                                  paReqsCompleted, cMaxReqsInFlight,
                                                  &cCompleted);
                            if (RT_FAILURE(rc))
                            {
                                RTPrintf("tstFileAio: FATAL ERROR - Waiting failed. rc=%Rrc\n", rc);
                                cErrors++;
                                break;
                            }

                            if (!fWrite)
                            {
                                for (uint32_t i = 0; i < cCompleted; i++)
                                {
                                    /* Compare that we read the right stuff. */
                                    void *pvBuf = RTFileAioReqGetUser(paReqsCompleted[i]);

                                    size_t cbTransfered;
                                    int rcReq = RTFileAioReqGetRC(paReqsCompleted[i], &cbTransfered);
                                    if (RT_FAILURE(rcReq) || (cbTransfered != cbTestBuf))
                                    {
                                        RTPrintf("tstFileAio: FATAL ERROR - Request %d failed with rc=%Rrc cbTransfered=%d.\n",
                                                 i, rcReq, cbTransfered);
                                        cErrors++;
                                        rc = rcReq;
                                        break;
                                    }

                                    if (memcmp(pvBuf, pvTestBuf, cbTestBuf) != 0)
                                    {
                                        RTPrintf("tstFileAio: FATAL ERROR - Unexpected content in memory.\n");
                                        cErrors++;
                                        break;
                                    }
                                    memset(pvBuf, 0, cbTestBuf);
                                }
                            }
                            cRun++;
                            if (RT_FAILURE(rc))
                                break;
                        }

                        /* Free buffers. */
                        for (unsigned i = 0; i < cMaxReqsInFlight; i++)
                            RTMemPageFree(papvBuf[i]);

                        /* Free requests. */
                        for (unsigned i = 0; i < cMaxReqsInFlight; i++)
                        {
                            rc = RTFileAioReqDestroy(paReqs[i]);
                            if (RT_FAILURE(rc))
                            {
                                RTPrintf("tstFileAio: ERROR - Failed to destroy request %d. rc=%Rrc\n", i, rc);
                                cErrors++;
                            }
                        }

                        NanoTS = RTTimeNanoTS() - NanoTS;
                        unsigned SpeedKBs = cbTestFile / (NanoTS / 1000000000.0) / 1024;

                        RTPrintf("tstFileAio: Completed simple %s test: %d.%03d MB/sec\n",
                                 fWrite ? "write" : "read",
                                 SpeedKBs / 1000,
                                 SpeedKBs % 1000);
                    }
                    else
                    {
                        RTPrintf("tstFileAio: FATAL ERROR - Failed to asssociate file with async I/O context. rc=%Rrc\n", rc);
                        cErrors++;
                    }
                }
                else
                {
                    RTPrintf("tstFileAio: FATAL ERROR - Failed to allocate memory for completed request array.\n");
                    cErrors++;
                }
                RTMemFree(papvBuf);
            }
            else
            {
                RTPrintf("tstFileAio: FATAL ERROR - Failed to allocate memory for buffer array.\n");
                cErrors++;
            }
            RTMemFree(paReqs);
        }
        else
        {
            RTPrintf("tstFileAio: FATAL ERROR - Failed to allocate memory for request array.\n");
            cErrors++;
        }

        rc = RTFileAioCtxDestroy(hAioContext);
        if (RT_FAILURE(rc))
        {
            RTPrintf("tstFileAio: FATAL ERROR - Failed to destroy async I/O context. rc=%Rrc\n", rc);
            cErrors++;
        }
    }
    else
    {
        RTPrintf("tstFileAio: FATAL ERROR - Failed to create async I/O context. rc=%Rrc\n", rc);
        cErrors++;
    }
}

int main()
{
    RTPrintf("tstFileAio: TESTING\n");
    RTR3Init();

    /* Check if the API is available. */
    RTFILEAIOLIMITS AioLimits;

    memset(&AioLimits, 0, sizeof(AioLimits));
    int rc = RTFileAioGetLimits(&AioLimits);
    if (RT_FAILURE(rc))
    {
        if (rc == VERR_NOT_SUPPORTED)
            RTPrintf("tstFileAio: FATAL ERROR - File AIO API not available\n");
        else
            RTPrintf("tstFileAio: FATAL ERROR - Unexpected error rc=%Rrc\n", rc);
        return 1;
    }

    RTFILE    File;
    void *pvTestBuf = NULL;
    rc = RTFileOpen(&File, "tstFileAio#1.tst", RTFILE_O_READWRITE | RTFILE_O_CREATE_REPLACE | RTFILE_O_DENY_NONE | RTFILE_O_ASYNC_IO);
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstFileAio: FATAL ERROR - Failed to open file #1. rc=%Rrc\n", rc);
        return 1;
    }

    pvTestBuf = RTMemAllocZ(64 * _1K);
    for (unsigned i = 0; i < 64*_1K; i++)
        ((char *)pvTestBuf)[i] = i % 256;

    uint32_t cReqsMax =   AioLimits.cReqsOutstandingMax < TSTFILEAIO_MAX_REQS_IN_FLIGHT
                        ? AioLimits.cReqsOutstandingMax
                        : TSTFILEAIO_MAX_REQS_IN_FLIGHT;

    /* Basic write test. */
    RTPrintf("tstFileAio: Preparing test file, this can take some time and needs quite a bit of harddisk\n");
    tstFileAioTestReadWriteBasic(File, true, pvTestBuf, 64*_1K, 100*_1M, cReqsMax);
    /* Reopen the file. */
    RTFileClose(File);
    rc = RTFileOpen(&File, "tstFileAio#1.tst", RTFILE_O_READWRITE | RTFILE_O_DENY_NONE | RTFILE_O_ASYNC_IO);
    if (RT_SUCCESS(rc))
    {
        tstFileAioTestReadWriteBasic(File, false, pvTestBuf, 64*_1K, 100*_1M, cReqsMax);
        RTFileClose(File);
    }
    else
    {
        RTPrintf("tstFileAio: FATAL ERROR - Failed to open file #1. rc=%Rrc\n", rc);
        cErrors++;
    }

    /* Cleanup */
    RTMemFree(pvTestBuf);
    RTFileDelete("tstFileAio#1.tst");
    /*
     * Summary
     */
    if (cErrors == 0)
        RTPrintf("tstFileAio: SUCCESS\n");
    else
        RTPrintf("tstFileAio: FAILURE - %d errors\n", cErrors);
    return !!cErrors;
}

