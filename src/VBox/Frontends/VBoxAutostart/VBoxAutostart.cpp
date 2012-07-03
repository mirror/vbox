/* $Id$ */
/** @file
 * VBoxAutostart - VirtualBox Autostart service.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <VBox/com/com.h>
#include <VBox/com/string.h>
#include <VBox/com/Guid.h>
#include <VBox/com/array.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint.h>

#include <VBox/com/EventQueue.h>
#include <VBox/com/listeners.h>
#include <VBox/com/VirtualBox.h>

#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/version.h>

#include <package-generated.h>

#include <iprt/asm.h>
#include <iprt/buildconfig.h>
#include <iprt/critsect.h>
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/message.h>
#include <iprt/path.h>
#include <iprt/process.h>
#include <iprt/semaphore.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/system.h>
#include <iprt/time.h>


#include <algorithm>
#include <list>
#include <string>
#include <signal.h>

using namespace com;

/**
 * VM list entry.
 */
typedef struct AUTOSTARTVM
{
    Bstr  strId;
    ULONG uStartupDelay;
} AUTOSTARTVM;

static ComPtr<IVirtualBoxClient> g_pVirtualBoxClient = NULL;
static bool                      g_fVerbose    = false;
static ComPtr<IVirtualBox>       g_pVirtualBox = NULL;
static ComPtr<ISession>          g_pSession    = NULL;

/** Logging parameters. */
static uint32_t      g_cHistory = 10;                   /* Enable log rotation, 10 files. */
static uint32_t      g_uHistoryFileTime = RT_SEC_1DAY;  /* Max 1 day per file. */
static uint64_t      g_uHistoryFileSize = 100 * _1M;    /* Max 100MB per file. */

/** Run in background. */
static bool          g_fDaemonize = false;

/**
 * Command line arguments.
 */
static const RTGETOPTDEF g_aOptions[] = {
#if defined(RT_OS_LINUX) || defined (RT_OS_SOLARIS) || defined(RT_OS_FREEBSD)
    { "--background",           'b',                                       RTGETOPT_REQ_NOTHING },
#endif
    /** For displayHelp(). */
    { "--help",                 'h',                                       RTGETOPT_REQ_NOTHING },
    { "--verbose",              'v',                                       RTGETOPT_REQ_NOTHING },
    { "--start",                's',                                       RTGETOPT_REQ_NOTHING },
    { "--stop",                 'd',                                       RTGETOPT_REQ_NOTHING },
    { "--config",               'c',                                       RTGETOPT_REQ_STRING },
    { "--logfile",              'F',                                       RTGETOPT_REQ_STRING },
    { "--logrotate",            'R',                                       RTGETOPT_REQ_UINT32 },
    { "--logsize",              'S',                                       RTGETOPT_REQ_UINT64 },
    { "--loginterval",          'I',                                       RTGETOPT_REQ_UINT32 },
    { "--quiet",                'Q',                                       RTGETOPT_REQ_NOTHING }
};


static void serviceLog(const char *pszFormat, ...)
{
    va_list args;
    va_start(args, pszFormat);
    char *psz = NULL;
    RTStrAPrintfV(&psz, pszFormat, args);
    va_end(args);

    LogRel(("%s", psz));

    RTStrFree(psz);
}

#define serviceLogVerbose(a) if (g_fVerbose) { serviceLog a; }

static DECLCALLBACK(bool) autostartVMCmp(const AUTOSTARTVM &vm1, const AUTOSTARTVM &vm2)
{
    return vm1.uStartupDelay <= vm2.uStartupDelay;
}

/**
 * Main routine for the autostart daemon.
 *
 * @returns exit status code.
 */
static RTEXITCODE autostartMain(void)
{
    RTEXITCODE rcExit = RTEXITCODE_SUCCESS;
    std::list<AUTOSTARTVM> listVM;

    /*
     * Build a list of all VMs we need to autostart first, apply the overrides
     * from the configuration and start the VMs afterwards.
     */
    com::SafeIfaceArray<IMachine> machines;
    HRESULT rc = g_pVirtualBox->COMGETTER(Machines)(ComSafeArrayAsOutParam(machines));
    if (SUCCEEDED(rc))
    {
        /*
         * Iterate through the collection
         */
        for (size_t i = 0; i < machines.size(); ++i)
        {
            if (machines[i])
            {
                BOOL fAccessible;
                CHECK_ERROR_BREAK(machines[i], COMGETTER(Accessible)(&fAccessible));
                if (!fAccessible)
                    continue;

                BOOL fAutostart;
                CHECK_ERROR_BREAK(machines[i], COMGETTER(AutostartEnabled)(&fAutostart));
                if (fAutostart)
                {
                    AUTOSTARTVM autostartVM;

                    CHECK_ERROR_BREAK(machines[i], COMGETTER(Id)(autostartVM.strId.asOutParam()));
                    CHECK_ERROR_BREAK(machines[i], COMGETTER(AutostartDelay)(&autostartVM.uStartupDelay));

                    listVM.push_back(autostartVM);
                }
            }
        }

        if (   SUCCEEDED(rc)
            && listVM.size())
        {
            /* Sort by startup delay and apply base override. */
            listVM.sort(autostartVMCmp);

            std::list<AUTOSTARTVM>::iterator it;
            for (it = listVM.begin(); it != listVM.end(); it++)
            {
                ComPtr<IMachine> machine;
                ComPtr<IProgress> progress;

                CHECK_ERROR_BREAK(g_pVirtualBox, FindMachine((*it).strId.raw(),
                                                             machine.asOutParam()));

                CHECK_ERROR_BREAK(machine, LaunchVMProcess(g_pSession, Bstr("headless").raw(),
                                                           Bstr("").raw(), progress.asOutParam()));
                if (SUCCEEDED(rc) && !progress.isNull())
                {
                    RTPrintf("Waiting for VM \"%ls\" to power on...\n", (*it).strId.raw());
                    CHECK_ERROR(progress, WaitForCompletion(-1));
                    if (SUCCEEDED(rc))
                    {
                        BOOL completed = true;
                        CHECK_ERROR(progress, COMGETTER(Completed)(&completed));
                        if (SUCCEEDED(rc))
                        {
                            ASSERT(completed);

                            LONG iRc;
                            CHECK_ERROR(progress, COMGETTER(ResultCode)(&iRc));
                            if (SUCCEEDED(rc))
                            {
                                if (FAILED(iRc))
                                {
                                    ProgressErrorInfo info(progress);
                                    com::GluePrintErrorInfo(info);
                                }
                                else
                                {
                                    RTPrintf("VM \"%ls\" has been successfully started.\n", (*it).strId.raw());
                                }
                            }
                        }
                    }
                }
                g_pSession->UnlockMachine();
            }
        }
    }

    return rcExit;
}

static void displayHeader()
{
    RTStrmPrintf(g_pStdErr, VBOX_PRODUCT " Autostart " VBOX_VERSION_STRING "\n"
                 "(C) " VBOX_C_YEAR " " VBOX_VENDOR "\n"
                 "All rights reserved.\n\n");
}

/**
 * Displays the help.
 *
 * @param   pszImage                Name of program name (image).
 */
static void displayHelp(const char *pszImage)
{
    AssertPtrReturnVoid(pszImage);

    displayHeader();

    RTStrmPrintf(g_pStdErr,
                 "Usage:\n"
                 " %s [-v|--verbose] [-h|-?|--help]\n"
                 " [-F|--logfile=<file>] [-R|--logrotate=<num>] [-S|--logsize=<bytes>]\n"
                 " [-I|--loginterval=<seconds>]\n"
                 " [-c|--config=<config file>]\n", pszImage);

    RTStrmPrintf(g_pStdErr, "\n"
                 "Options:\n");

    for (unsigned i = 0;
         i < RT_ELEMENTS(g_aOptions);
         ++i)
    {
        std::string str(g_aOptions[i].pszLong);
        if (g_aOptions[i].iShort < 1000) /* Don't show short options which are defined by an ID! */
        {
            str += ", -";
            str += g_aOptions[i].iShort;
        }
        str += ":";

        const char *pcszDescr = "";

        switch (g_aOptions[i].iShort)
        {
            case 'h':
                pcszDescr = "Print this help message and exit.";
                break;

#if defined(RT_OS_LINUX) || defined (RT_OS_SOLARIS) || defined(RT_OS_FREEBSD)
            case 'b':
                pcszDescr = "Run in background (daemon mode).";
                break;
#endif
            case 'P':
                pcszDescr = "Name of the PID file which is created when the daemon was started.";
                break;

            case 'F':
                pcszDescr = "Name of file to write log to (no file).";
                break;

            case 'R':
                pcszDescr = "Number of log files (0 disables log rotation).";
                break;

            case 'S':
                pcszDescr = "Maximum size of a log file to trigger rotation (bytes).";
                break;

            case 'I':
                pcszDescr = "Maximum time interval to trigger log rotation (seconds).";
                break;

            case 'c':
                pcszDescr = "Name of the configuration file for the global overrides.";
                break;
        }

        RTStrmPrintf(g_pStdErr, "%-23s%s\n", str.c_str(), pcszDescr);
    }

    RTStrmPrintf(g_pStdErr, "\nUse environment variable VBOXAUTOSTART_RELEASE_LOG for logging options.\n");
}

/**
 * Creates all global COM objects.
 *
 * @return  HRESULT
 */
static int autostartSetup()
{
    serviceLogVerbose(("Setting up ...\n"));

    /*
     * Setup VirtualBox + session interfaces.
     */
    HRESULT rc = g_pVirtualBoxClient->COMGETTER(VirtualBox)(g_pVirtualBox.asOutParam());
    if (SUCCEEDED(rc))
    {
        rc = g_pSession.createInprocObject(CLSID_Session);
        if (FAILED(rc))
            RTMsgError("Failed to create a session object (rc=%Rhrc)!", rc);
    }
    else
        RTMsgError("Failed to get VirtualBox object (rc=%Rhrc)!", rc);

    if (FAILED(rc))
        return VERR_COM_OBJECT_NOT_FOUND;

    return VINF_SUCCESS;
}

static void autostartShutdown()
{
    serviceLogVerbose(("Shutting down ...\n"));

    g_pSession.setNull();
    g_pVirtualBox.setNull();
}

int main(int argc, char *argv[])
{
    /*
     * Before we do anything, init the runtime without loading
     * the support driver.
     */
    int rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    /*
     * Parse the global options
    */
    int c;
    const char *pszLogFile = NULL;
    const char *pszConfigFile = NULL;
    bool fQuiet = false;
    bool fStart = false;
    bool fStop  = false;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv,
                 g_aOptions, RT_ELEMENTS(g_aOptions), 1 /* First */, 0 /*fFlags*/);
    while ((c = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (c)
        {
            case 'h':
                displayHelp(argv[0]);
                return 0;

            case 'v':
                g_fVerbose = true;
                break;

#if defined(RT_OS_LINUX) || defined (RT_OS_SOLARIS) || defined(RT_OS_FREEBSD)
            case 'b':
                g_fDaemonize = true;
                break;
#endif
            case 'V':
                RTPrintf("%sr%s\n", RTBldCfgVersion(), RTBldCfgRevisionStr());
                return 0;

            case 'F':
                pszLogFile = ValueUnion.psz;
                break;

            case 'R':
                g_cHistory = ValueUnion.u32;
                break;

            case 'S':
                g_uHistoryFileSize = ValueUnion.u64;
                break;

            case 'I':
                g_uHistoryFileTime = ValueUnion.u32;
                break;

            case 'Q':
                fQuiet = true;
                break;

            case 'c':
                pszConfigFile = ValueUnion.psz;
                break;

            case 's':
                fStart = true;
                break;

            case 'd':
                fStop = true;
                break;

            default:
                return RTGetOptPrintError(c, &ValueUnion);
        }
    }

    if (fStart == fStop)
    {
        displayHelp(argv[0]);
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "--start or --stop are mutually exclusive");
    }

    if (!fQuiet)
        displayHeader();

    /* create release logger, to stdout */
    char szError[RTPATH_MAX + 128];
    rc = com::VBoxLogRelCreate("Autostart", g_fDaemonize ? NULL : pszLogFile,
                               RTLOGFLAGS_PREFIX_THREAD | RTLOGFLAGS_PREFIX_TIME_PROG,
                               "all", "VBOXAUTOSTART_RELEASE_LOG",
                               RTLOGDEST_STDOUT, UINT32_MAX /* cMaxEntriesPerGroup */,
                               g_cHistory, g_uHistoryFileTime, g_uHistoryFileSize,
                               szError, sizeof(szError));
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "failed to open release log (%s, %Rrc)", szError, rc);

#if defined(RT_OS_LINUX) || defined (RT_OS_SOLARIS) || defined(RT_OS_FREEBSD)
    if (g_fDaemonize)
    {
        /* prepare release logging */
        char szLogFile[RTPATH_MAX];

        if (!pszLogFile || !*pszLogFile)
        {
            rc = com::GetVBoxUserHomeDirectory(szLogFile, sizeof(szLogFile));
            if (RT_FAILURE(rc))
                 return RTMsgErrorExit(RTEXITCODE_FAILURE, "could not get base directory for logging: %Rrc", rc);
            rc = RTPathAppend(szLogFile, sizeof(szLogFile), "vboxautostart.log");
            if (RT_FAILURE(rc))
                 return RTMsgErrorExit(RTEXITCODE_FAILURE, "could not construct logging path: %Rrc", rc);
            pszLogFile = szLogFile;
        }

        rc = RTProcDaemonizeUsingFork(false /* fNoChDir */, false /* fNoClose */, NULL);
        if (RT_FAILURE(rc))
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "failed to daemonize, rc=%Rrc. exiting.", rc);
        /* create release logger, to file */
        rc = com::VBoxLogRelCreate("Autostart", pszLogFile,
                                   RTLOGFLAGS_PREFIX_THREAD | RTLOGFLAGS_PREFIX_TIME_PROG,
                                   "all", "VBOXAUTOSTART_RELEASE_LOG",
                                   RTLOGDEST_FILE, UINT32_MAX /* cMaxEntriesPerGroup */,
                                   g_cHistory, g_uHistoryFileTime, g_uHistoryFileSize,
                                   szError, sizeof(szError));
        if (RT_FAILURE(rc))
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "failed to open release log (%s, %Rrc)", szError, rc);
    }
#endif

    /*
     * Initialize COM.
     */
    using namespace com;
    HRESULT hrc = com::Initialize();
# ifdef VBOX_WITH_XPCOM
    if (hrc == NS_ERROR_FILE_ACCESS_DENIED)
    {
        char szHome[RTPATH_MAX] = "";
        com::GetVBoxUserHomeDirectory(szHome, sizeof(szHome));
        return RTMsgErrorExit(RTEXITCODE_FAILURE,
               "Failed to initialize COM because the global settings directory '%s' is not accessible!", szHome);
    }
# endif
    if (FAILED(hrc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to initialize COM (%Rhrc)!", hrc);

    hrc = g_pVirtualBoxClient.createInprocObject(CLSID_VirtualBoxClient);
    if (FAILED(hrc))
    {
        RTMsgError("Failed to create the VirtualBoxClient object (%Rhrc)!", hrc);
        com::ErrorInfo info;
        if (!info.isFullAvailable() && !info.isBasicAvailable())
        {
            com::GluePrintRCMessage(hrc);
            RTMsgError("Most likely, the VirtualBox COM server is not running or failed to start.");
        }
        else
            com::GluePrintErrorInfo(info);
        return RTEXITCODE_FAILURE;
    }

    rc = autostartSetup();
    if (RT_FAILURE(rc))
        return RTEXITCODE_FAILURE;

    RTEXITCODE rcExit = autostartMain();

    EventQueue::getMainEventQueue()->processEventQueue(0);

    autostartShutdown();

    g_pVirtualBoxClient.setNull();

    com::Shutdown();

    return rcExit;
}

