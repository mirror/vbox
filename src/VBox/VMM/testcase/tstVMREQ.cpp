/** @file
 *
 * VMM Testcase.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/vm.h>
#include <VBox/vmm.h>
#include <VBox/cpum.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/runtime.h>
#include <iprt/semaphore.h>
#include <iprt/stream.h>
#include <iprt/thread.h>
#include <iprt/time.h>




/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#define TESTCASE    "tstVMREQ"

/**
 * Thread function which allocates and frees requests like wildfire.
 */
DECLCALLBACK(int) Thread(RTTHREAD Thread, void *pvUser)
{
    int rc = VINF_SUCCESS;
    PVM pVM = (PVM)pvUser;
    for (unsigned i = 0; i < 100000; i++)
    {
        PVMREQ          apReq[17];
        const unsigned  cReqs = i % ELEMENTS(apReq);
        unsigned        iReq;
        for (iReq = 0; iReq < cReqs; iReq++)
        {
            rc = VMR3ReqAlloc(pVM, &apReq[iReq], VMREQTYPE_INTERNAL);
            if (VBOX_FAILURE(rc))
            {
                RTPrintf(TESTCASE ": i=%d iReq=%d cReqs=%d rc=%Vrc (alloc)\n", i, iReq, cReqs, rc);
                return rc;
            }
            apReq[iReq]->iStatus = iReq + i;
        }

        for (iReq = 0; iReq < cReqs; iReq++)
        {
            if (apReq[iReq]->iStatus != (int)(iReq + i))
            {
                RTPrintf(TESTCASE ": i=%d iReq=%d cReqs=%d: iStatus=%d != %d\n", i, iReq, cReqs, apReq[iReq]->iStatus, iReq + i);
                return VERR_GENERAL_FAILURE;
            }
            rc = VMR3ReqFree(apReq[iReq]);
            if (VBOX_FAILURE(rc))
            {
                RTPrintf(TESTCASE ": i=%d iReq=%d cReqs=%d rc=%Vrc (free)\n", i, iReq, cReqs, rc);
                return rc;
            }
        }
        //if (!(i % 10000))
        //    RTPrintf(TESTCASE ": i=%d\n", i);
    }

    return VINF_SUCCESS;
}



int main(int argc, char **argv)
{
    int     cErrors = 0;

    RTR3Init();
    RTPrintf(TESTCASE ": TESTING...\n");

    /*
     * Create empty VM.
     */
    PVM pVM;
    int rc = VMR3Create(NULL, NULL, NULL, NULL, &pVM);
    if (VBOX_SUCCESS(rc))
    {
        /*
         * Do testing.
         */
        uint64_t u64StartTS = RTTimeNanoTS();
        RTTHREAD Thread0;
        rc = RTThreadCreate(&Thread0, Thread, pVM, 0, RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, "REQ1");
        if (VBOX_SUCCESS(rc))
        {
            RTTHREAD Thread1;
            rc = RTThreadCreate(&Thread1, Thread, pVM, 0, RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, "REQ1");
            if (VBOX_SUCCESS(rc))
            {
                int rcThread1;
                rc = RTThreadWait(Thread1, RT_INDEFINITE_WAIT, &rcThread1);
                if (VBOX_FAILURE(rc))
                {
                    RTPrintf(TESTCASE ": RTThreadWait(Thread1,,) failed, rc=%Vrc\n", rc);
                    cErrors++;
                }
                if (VBOX_FAILURE(rcThread1))
                    cErrors++;
            }
            else
            {
                RTPrintf(TESTCASE ": RTThreadCreate(&Thread1,,,,) failed, rc=%Vrc\n", rc);
                cErrors++;
            }

            int rcThread0;
            rc = RTThreadWait(Thread0, RT_INDEFINITE_WAIT, &rcThread0);
            if (VBOX_FAILURE(rc))
            {
                RTPrintf(TESTCASE ": RTThreadWait(Thread1,,) failed, rc=%Vrc\n", rc);
                cErrors++;
            }
            if (VBOX_FAILURE(rcThread0))
                cErrors++;
        }
        else
        {
            RTPrintf(TESTCASE ": RTThreadCreate(&Thread0,,,,) failed, rc=%Vrc\n", rc);
            cErrors++;
        }
        uint64_t u64ElapsedTS = RTTimeNanoTS() - u64StartTS;
        RTPrintf(TESTCASE  ": %llu ns elapsed\n", u64ElapsedTS);

        /*
         * Print stats.
         */
        STAMR3Print(pVM, "/VM/Req/*");

        /*
         * Cleanup.
         */
        rc = VMR3Destroy(pVM);
        if (!VBOX_SUCCESS(rc))
        {
            RTPrintf(TESTCASE ": error: failed to destroy vm! rc=%Vrc\n", rc);
            cErrors++;
        }
    }
    else
    {
        RTPrintf(TESTCASE ": fatal error: failed to create vm! rc=%Vrc\n", rc);
        cErrors++;
    }

    /*
     * Summary and return.
     */
    if (!cErrors)
        RTPrintf(TESTCASE ": SUCCESS\n");
    else
        RTPrintf(TESTCASE ": FAILURE - %d errors\n", cErrors);

    return !!cErrors;
}
