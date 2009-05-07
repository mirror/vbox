/* $Id$ */
/** @file
 * VMM Testcase.
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
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/vm.h>
#include <VBox/vmm.h>
#include <VBox/cpum.h>
#include <VBox/pdm.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/initterm.h>
#include <iprt/semaphore.h>
#include <iprt/stream.h>
#include <iprt/test.h>
#include <iprt/getopt.h>
#include <iprt/ctype.h>
#include <iprt/string.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#define TESTCASE    "tstVMM"



/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static uint32_t g_cCpus = 1;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
VMMR3DECL(int) VMMDoTest(PVM pVM); /* Linked into VMM, see ../VMMTests.cpp. */


/** Dummy timer callback. */
static DECLCALLBACK(void) tstTMDummyCallback(PVM pVM, PTMTIMER pTimer, void *pvUser)
{
    NOREF(pVM);
    NOREF(pTimer);
    NOREF(pvUser);
}


/**
 * This is called on each EMT and will beat TM.
 *
 * @returns VINF_SUCCESS, test failure is reported via RTTEST.
 * @param   pVM         The VM handle.
 * @param   hTest       The test handle.
 */
DECLCALLBACK(int) tstTMWorker(PVM pVM, RTTEST hTest)
{
    VMCPUID idCpu = VMMGetCpuId(pVM);
    RTTestPrintfNl(hTest,  RTTESTLVL_ALWAYS,  "idCpu=%d STARTING\n", idCpu);

    /*
     * Create the test set.
     */
    int rc;
    PTMTIMER apTimers[5];
    for (size_t i = 0; i < RT_ELEMENTS(apTimers); i++)
    {
        int rc = TMR3TimerCreateInternal(pVM, i & 1 ? TMCLOCK_VIRTUAL :  TMCLOCK_VIRTUAL_SYNC,
                                         tstTMDummyCallback, NULL, "test timer",  &apTimers[i]);
        RTTEST_CHECK_RET(hTest, RT_SUCCESS(rc), rc);
    }

    /*
     * The run loop.
     */
    unsigned        uPrevPct = 0;
    uint32_t const  cLoops   = 100000;
    for (uint32_t iLoop = 0; iLoop < cLoops; iLoop++)
    {
        size_t      cLeft = RT_ELEMENTS(apTimers);
        unsigned    i     = iLoop % RT_ELEMENTS(apTimers);
        while (cLeft-- > 0)
        {
            PTMTIMER pTimer = apTimers[i];

            if (    cLeft == RT_ELEMENTS(apTimers) / 2
                &&  TMTimerIsActive(pTimer))
            {
//                rc = TMTimerStop(pTimer);
//                RTTEST_CHECK_MSG(hTest, RT_SUCCESS(rc), (hTest, "TMTimerStop: %Rrc\n",  rc));
            }
            else
            {
                rc = TMTimerSetMicro(pTimer, 50 + cLeft);
                RTTEST_CHECK_MSG(hTest, RT_SUCCESS(rc), (hTest, "TMTimerSetMicro: %Rrc\n", rc));
            }

            /* next */
            i = (i + 1) % RT_ELEMENTS(apTimers);
        }

        if (i % 3)
            TMR3TimerQueuesDo(pVM);

        /* Progress report. */
        unsigned uPct = (unsigned)(100.0 * iLoop / cLoops);
        if (uPct != uPrevPct)
        {
            uPrevPct = uPct;
            if (!(uPct % 10))
                RTTestPrintfNl(hTest,  RTTESTLVL_ALWAYS,  "idCpu=%d - %3u%%\n", idCpu, uPct);
        }
    }

    RTTestPrintfNl(hTest,  RTTESTLVL_ALWAYS,  "idCpu=%d DONE\n", idCpu);
    return 0;
}


/** PDMR3LdrEnumModules callback, see FNPDMR3ENUM. */
static DECLCALLBACK(int)
tstVMMLdrEnum(PVM pVM, const char *pszFilename, const char *pszName, RTUINTPTR ImageBase, size_t cbImage, bool fGC, void *pvUser)
{
    NOREF(pVM); NOREF(pszFilename); NOREF(fGC); NOREF(pvUser);
    RTPrintf("tstVMM: %RTptr %s\n", ImageBase, pszName);
    return VINF_SUCCESS;
}

static DECLCALLBACK(int)
tstVMMConfigConstructor(PVM pVM, void *pvUser)
{
    int rc = CFGMR3ConstructDefaultTree(pVM);
    if (    RT_SUCCESS(rc)
        &&  g_cCpus > 1)
    {
        PCFGMNODE pRoot = CFGMR3GetRoot(pVM);
        CFGMR3RemoveValue(pRoot, "NumCPUs");
        rc = CFGMR3InsertInteger(pRoot, "NumCPUs", g_cCpus);
        RTTESTI_CHECK_MSG_RET(RT_SUCCESS(rc), ("CFGMR3InsertInteger(pRoot,\"NumCPUs\",) -> %Rrc\n", rc), rc);

        CFGMR3RemoveValue(pRoot, "HwVirtExtForced");
        rc = CFGMR3InsertInteger(pRoot, "HwVirtExtForced", true);
        RTTESTI_CHECK_MSG_RET(RT_SUCCESS(rc), ("CFGMR3InsertInteger(pRoot,\"HwVirtExtForced\",) -> %Rrc\n", rc), rc);

        PCFGMNODE pHwVirtExt = CFGMR3GetChild(pRoot, "HWVirtExt");
        CFGMR3RemoveNode(pHwVirtExt);
        rc = CFGMR3InsertNode(pRoot, "HWVirtExt", &pHwVirtExt);
        RTTESTI_CHECK_MSG_RET(RT_SUCCESS(rc), ("CFGMR3InsertNode(pRoot,\"HWVirtExt\",) -> %Rrc\n", rc), rc);
        rc = CFGMR3InsertInteger(pHwVirtExt, "Enabled", true);
        RTTESTI_CHECK_MSG_RET(RT_SUCCESS(rc), ("CFGMR3InsertInteger(pHwVirtExt,\"Enabled\",) -> %Rrc\n", rc), rc);
        rc = CFGMR3InsertInteger(pHwVirtExt, "64bitEnabled", false);
        RTTESTI_CHECK_MSG_RET(RT_SUCCESS(rc), ("CFGMR3InsertInteger(pHwVirtExt,\"64bitEnabled\",) -> %Rrc\n", rc), rc);
    }
    return rc;
}


int main(int argc, char **argv)
{
    /*
     * Init runtime and the test environment.
     */
    int rc = RTR3InitAndSUPLib();
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstVMM: RTR3InitAndSUPLib failed: %Rrc\n", rc);
        return 1;
    }
    RTTEST hTest;
    rc = RTTestCreate("tstVMM", &hTest);
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstVMM: RTTestCreate failed: %Rrc\n", rc);
        return 1;
    }

    /*
     * Parse arguments.
     */
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--cpus",          'c', RTGETOPT_REQ_UINT8 },
        { "--test",          't', RTGETOPT_REQ_STRING },
        { "--help",          'h', 0 },
    };
    enum
    {
        kTstVMMTest_VMM,  kTstVMMTest_TM
    } enmTestOpt = kTstVMMTest_VMM;

    int ch;
    int i = 1;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, 0);
    while ((ch = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (ch)
        {
            case 'c':
                g_cCpus = ValueUnion.u8;
                break;

            case 't':
                if (!strcmp("vmm", ValueUnion.psz))
                    enmTestOpt = kTstVMMTest_VMM;
                else if (!strcmp("tm", ValueUnion.psz))
                    enmTestOpt = kTstVMMTest_TM;
                else
                {
                    RTPrintf("tstVMM: unknown test: '%s'\n", ValueUnion.psz);
                    return 1;
                }
                break;

            case 'h':
                RTPrintf("usage: tstVMM [--cpus|-c cpus] [--test <vmm|tm>]\n");
                return 1;

            case VINF_GETOPT_NOT_OPTION:
                RTPrintf("tstVMM: syntax error: non option '%s'\n", ValueUnion.psz);
                break;

            default:
                if (ch > 0)
                {
                    if (RT_C_IS_GRAPH(ch))
                        RTPrintf("tstVMM: unhandled option: -%c\n", ch);
                    else
                        RTPrintf("tstVMM: unhandled option: %i\n", ch);
                }
                else if (ch == VERR_GETOPT_UNKNOWN_OPTION)
                    RTPrintf("tstVMM: unknown option: %s\n", ValueUnion.psz);
                else if (ValueUnion.pDef)
                    RTPrintf("tstVMM: %s: %Rrs\n", ValueUnion.pDef->pszLong, ch);
                else
                    RTPrintf("tstVMM: %Rrs\n", ch);
                return 1;
        }
    }

    /*
     * Create the test VM.
     */
    RTPrintf(TESTCASE ": Initializing...\n");
    PVM pVM;
    rc = VMR3Create(g_cCpus, NULL, NULL, tstVMMConfigConstructor, NULL, &pVM);
    if (RT_SUCCESS(rc))
    {
        PDMR3LdrEnumModules(pVM, tstVMMLdrEnum, NULL);
        RTStrmFlush(g_pStdOut);
        RTThreadSleep(256);

        /*
         * Do the requested testing.
         */
        switch (enmTestOpt)
        {
            case kTstVMMTest_VMM:
            {
                RTTestSub(hTest, "VMM");
                PVMREQ pReq1 = NULL;
                rc = VMR3ReqCall(pVM, VMCPUID_ANY, &pReq1, RT_INDEFINITE_WAIT, (PFNRT)VMMDoTest, 1, pVM);
                if (RT_FAILURE(rc))
                    RTTestFailed(hTest, "VMR3ReqCall failed: rc=%Rrc\n", rc);
                else if (RT_FAILURE(pReq1->iStatus))
                    RTTestFailed(hTest, "VMMDoTest failed: rc=%Rrc\n", rc);
                VMR3ReqFree(pReq1);
                break;
            }

            case kTstVMMTest_TM:
            {
                RTTestSub(hTest, "TM");
                for (VMCPUID idCpu = 1; idCpu < g_cCpus; idCpu++)
                {
                    PVMREQ pReq = NULL;
                    rc = VMR3ReqCallU(pVM->pUVM, idCpu, &pReq, 0, VMREQFLAGS_NO_WAIT,  (PFNRT)tstTMWorker, 2, pVM, hTest);
                    if (RT_FAILURE(rc))
                        RTTestFailed(hTest, "VMR3ReqCall failed: rc=%Rrc\n", rc);
                }

                PVMREQ pReq1 = NULL;
                rc = VMR3ReqCall(pVM, 0, &pReq1, RT_INDEFINITE_WAIT, (PFNRT)tstTMWorker, 2, pVM, hTest);
                if (RT_FAILURE(rc))
                    RTTestFailed(hTest, "VMR3ReqCall failed: rc=%Rrc\n", rc);
                else if (RT_FAILURE(pReq1->iStatus))
                    RTTestFailed(hTest, "VMMDoTest failed: rc=%Rrc\n", rc);
                VMR3ReqFree(pReq1);
                break;
            }
        }

        STAMR3Dump(pVM, "*");

        /*
         * Cleanup.
         */
        rc = VMR3Destroy(pVM);
        if (RT_FAILURE(rc))
            RTTestFailed(hTest, "VMR3Destroy failed: rc=%Rrc\n", rc);
    }
    else
        RTTestFailed(hTest, "VMR3Create failed: rc=%Rrc\n", rc);

    return RTTestSummaryAndDestroy(hTest);
}
