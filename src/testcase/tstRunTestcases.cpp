/* $Id$ */
/** @file
 * tstRunTescases - Driver program for running VBox testcase (tst* testcase/tst*).
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/runtime.h>
#include <iprt/dir.h>
#include <iprt/process.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/stream.h>
#include <iprt/thread.h>
#include <iprt/err.h>
#include <iprt/env.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The number of passed testcases. */
static unsigned     g_cPasses = 0;
/** The number of failed testcases. */
static unsigned     g_cFailures = 0;
/** The number of skipped testcases. */
static unsigned     g_cSkipped = 0;
/** The exclude list. */
static const char  *g_apszExclude[] =
{
#if 1 // slow stuff
    "testcase/tstFile",
    "testcase/tstAvl",
#endif
    "testcase/tstFileLock",
    "testcase/tstCritSect",
    "testcase/tstCritSectW32",
    "testcase/tstDeadlock",
    "testcase/tstLdr-2",
    "testcase/tstLdr-3",
    "testcase/tstLdr",
    "testcase/tstLdrLoad",
    "testcase/tstLdrObj",
    "testcase/tstLdrObjR0",
    "testcase/tstMove",
    "testcase/tstRunTestcases",
    "testcase/tstSDL",
    "testcase/tstTime-3",
    "./tstRunTestcases",
    "./tstAnimate",
    "./tstAPI",
    "./tstHeadless",
    "./tstMicro",
    "./tstMicroGC",
    "./tstVBoxDbg",
    "./tstVMM-2",
    "./tstTestServMgr",
    "./tstXptDump",
    "./tstnsIFileEnumerator",
    "./tstSimpleTypeLib",
    "./tstTestAtoms",
    "./tstXptLink",
    "./tstTestCallTemplates",
#if 1 // later
    "testcase/tstIntNetR0",
    "./tstVMM",
    "./tstVMReq",
    "./tstVMREQ",
#endif
    /* final entry*/
    ""
};


/**
 * Checks if a testcase is include or should be skipped.
 *
 * @param pszTestcase   The testcase (filename).
 *
 * @return  true if the testcase is included.
 *          false if the testcase should be skipped.
 */
static bool IsTestcaseIncluded(const char *pszTestcase)
{
    char *pszDup = RTStrDup(pszTestcase);
    if (pszDup)
    {
        RTPathStripExt(pszDup);
        for (unsigned i = 0; i < ELEMENTS(g_apszExclude); i++)
        {
            if (!strcmp(g_apszExclude[i], pszDup))
            {
                RTStrFree(pszDup);
                return false;
            }
        }
        RTStrFree(pszDup);
        return true;
    }

    RTPrintf("tstRunTestcases: Out of memory!\n");
    return false;
}


/**
 * Process the testcases found in the filter.
 *
 * @param   pszFilter   The filter (winnt) to pass to RTDirOpenFiltered for
 *                      selecting the testcases.
 * @param   pszDir      The directory we're processing.
 */
static void Process(const char *pszFilter, const char *pszDir)
{
    RTENV env;
    int rc = RTEnvClone(&env, RTENV_DEFAULT);
    if (RT_FAILURE(rc)) {
        RTPrintf("tstRunTestcases: Failed to clone environment -> %Rrc\n", rc);
        return;
    }
    /*
     * Open and enumerate the directory.
     */
    PRTDIR pDir;
    rc = RTDirOpenFiltered(&pDir, pszFilter, RTDIRFILTER_WINNT);
    if (RT_SUCCESS(rc))
    {
        for (;;)
        {
            RTDIRENTRY DirEntry;
            rc = RTDirRead(pDir, &DirEntry, NULL);
            if (RT_FAILURE(rc))
            {
                if (rc == VERR_NO_MORE_FILES)
                    rc = VINF_SUCCESS;
                else
                    RTPrintf("tstRunTestcases: reading '%s' -> %Rrc\n", pszFilter, rc);
                break;
            }

            /*
             * Construct the testcase name.
             */
            char *pszTestcase;
            RTStrAPrintf(&pszTestcase, "%s/%s", pszDir, DirEntry.szName);
            if (!pszTestcase)
            {
                RTPrintf("tstRunTestcases: out of memory!\n");
                rc = VERR_NO_MEMORY;
                break;
            }
            if (IsTestcaseIncluded(pszTestcase))
            {
                /*
                 * Execute the testcase.
                 */
                RTPrintf("*** %s: Executing...\n", pszTestcase);  RTStrmFlush(g_pStdOut);
                const char *papszArgs[2];
                papszArgs[0] = pszTestcase;
                papszArgs[1] = NULL;
                RTPROCESS Process;
                rc = RTProcCreate(pszTestcase, papszArgs, env, 0, &Process);
                if (RT_SUCCESS(rc))
                {
                    /*
                     * Wait for the process and collect it's return code.
                     * If it takes too long, we'll terminate it and continue.
                     */
                    RTTIMESPEC Start;
                    RTTimeNow(&Start);
                    RTPROCSTATUS ProcStatus;
                    for (;;)
                    {
                        rc = RTProcWait(Process, RTPROCWAIT_FLAGS_NOBLOCK, &ProcStatus);
                        if (rc != VERR_PROCESS_RUNNING)
                            break;
                        RTTIMESPEC Now;
                        if (RTTimeSpecGetMilli(RTTimeSpecSub(RTTimeNow(&Now), &Start)) > 60*1000 /* 1 min */)
                        {
                            RTPrintf("*** %s: FAILED - timed out. killing it.\n", pszTestcase);
                            RTProcTerminate(Process);
                            RTThreadSleep(100);
                            RTProcWait(Process, RTPROCWAIT_FLAGS_NOBLOCK, &ProcStatus);
                            g_cFailures++;
                            break;
                        }
                        RTThreadSleep(100);
                    }

                    /*
                     * Examin the exit status.
                     */
                    if (RT_SUCCESS(rc))
                    {
                        if (    ProcStatus.enmReason == RTPROCEXITREASON_NORMAL
                            &&  ProcStatus.iStatus == 0)
                        {
                            RTPrintf("*** %s: PASSED\n", pszTestcase);
                            g_cPasses++;
                        }
                        else
                        {
                            RTPrintf("*** %s: FAILED\n", pszTestcase);
                            g_cFailures++;
                        }
                    }
                    else if (rc != VERR_PROCESS_RUNNING)
                    {
                        RTPrintf("tstRunTestcases: %s: RTProcWait failed -> %Rrc\n", pszTestcase, rc);
                        g_cFailures++;
                    }
                }
                else
                {
                    RTPrintf("tstRunTestcases: %s: failed to start -> %Rrc\n", pszTestcase, rc);
                    g_cFailures++;
                }

            }
            else
            {
                RTPrintf("tstRunTestcases: %s: SKIPPED\n", pszTestcase);
                g_cSkipped++;
            }
            RTStrFree(pszTestcase);
        } /* enumeration loop */

        RTDirClose(pDir);
    }
    else
        RTPrintf("tstRunTestcases: opening '%s' -> %Rrc\n", pszDir, rc);
    RTEnvDestroy(env);
}



int main(int argc, char **argv)
{
    RTR3Init(false, 0);

    if (argc == 1)
    {
        Process("testcase/tst*", "testcase");
        Process("tst*", ".");
    }
    else
    {
        char szDir[RTPATH_MAX];
        for (int i = 1; i < argc; i++)
        {
            if (argv[i][0] == '-')
            {
                switch (argv[i][1])
                {
                    /* case '':... */

                    default:
                        RTPrintf("syntax error: Option '%s' is not recognized\n", argv[i]);
                        return 1;
                }
            }
            else
            {
                size_t cch = strlen(argv[i]);
                if (cch >= sizeof(szDir))
                {
                    RTPrintf("syntax error: '%s' is too long!\n", argv[i]);
                    return 1;
                }
                memcpy(szDir, argv[i], cch + 1);
                char *pszFilename = RTPathFilename(szDir);
                if (!pszFilename)
                {
                    RTPrintf("syntax error: '%s' does not include a file name or file name mask!\n", argv[i]);
                    return 1;
                }
                RTPathStripFilename(szDir);
                Process(argv[i], szDir);
            }
        }
    }

    RTPrintf("\n"
             "********************\n"
             "***  PASSED: %u\n"
             "***  FAILED: %u\n"
             "*** SKIPPED: %u\n"
             "***   TOTAL: %u\n",
             g_cPasses,
             g_cFailures,
             g_cSkipped,
             g_cPasses + g_cFailures + g_cSkipped);
    return !!g_cFailures;
}

