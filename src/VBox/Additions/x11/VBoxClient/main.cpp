/* $Id$ */
/** @file
 * VirtualBox Guest Additions - X11 Client.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>       /* For exit */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <package-generated.h>
#include "product-generated.h"

#include <iprt/buildconfig.h>
#include <iprt/critsect.h>
#include <iprt/env.h>
#include <iprt/file.h>
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/message.h>
#include <iprt/path.h>
#include <iprt/param.h>
#include <iprt/process.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/system.h>
#include <iprt/types.h>
#include <VBox/VBoxGuestLib.h>
#include <VBox/err.h>
#include <VBox/log.h>

#include "VBoxClient.h"


/*********************************************************************************************************************************
*   Defines                                                                                                                      *
*********************************************************************************************************************************/
#define VBOXCLIENT_OPT_NORESPAWN            950

#define VBOXCLIENT_OPT_SERVICES             980
#define VBOXCLIENT_OPT_CHECKHOSTVERSION     VBOXCLIENT_OPT_SERVICES
#define VBOXCLIENT_OPT_CLIPBOARD            VBOXCLIENT_OPT_SERVICES + 1
#define VBOXCLIENT_OPT_DRAGANDDROP          VBOXCLIENT_OPT_SERVICES + 3
#define VBOXCLIENT_OPT_SEAMLESS             VBOXCLIENT_OPT_SERVICES + 4
#define VBOXCLIENT_OPT_VMSVGA               VBOXCLIENT_OPT_SERVICES + 5
#define VBOXCLIENT_OPT_VMSVGA_X11           VBOXCLIENT_OPT_SERVICES + 6


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/*static int (*gpfnOldIOErrorHandler)(Display *) = NULL; - unused */

/** Object representing the service we are running.  This has to be global
 * so that the cleanup routine can access it. */
struct VBCLSERVICE **g_pService;
/** The name of our pidfile.  It is global for the benefit of the cleanup
 * routine. */
static char          g_szPidFile[RTPATH_MAX] = "";
/** The file handle of our pidfile.  It is global for the benefit of the
 * cleanup routine. */
static RTFILE        g_hPidFile;
/** Global critical section held during the clean-up routine (to prevent it
 * being called on multiple threads at once) or things which may not happen
 * during clean-up (e.g. pausing and resuming the service).
 */
static RTCRITSECT    g_critSect;
/** Counter of how often our daemon has been respawned. */
static unsigned      g_cRespawn = 0;
/** Logging verbosity level. */
static unsigned      g_cVerbosity = 0;
static char          g_szLogFile[RTPATH_MAX + 128] = "";
/** Logging parameters. */
/** @todo Make this configurable later. */
static PRTLOGGER     g_pLoggerRelease = NULL;
static uint32_t      g_cHistory = 10;                   /* Enable log rotation, 10 files. */
static uint32_t      g_uHistoryFileTime = RT_SEC_1DAY;  /* Max 1 day per file. */
static uint64_t      g_uHistoryFileSize = 100 * _1M;    /* Max 100MB per file. */

/**
 * Notifies the desktop environment with a message.
 *
 * @param   pszMessage          Message to notify desktop environment with.
 */
int vbclLogNotify(const char *pszMessage)
{
    AssertPtrReturn(pszMessage, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;

    if (g_cRespawn == 0)
    {
        char *pszCommand = RTStrAPrintf2("notify-send \"VBoxClient: %s\"", pszMessage);
        if (pszCommand)
        {
            int status = system(pszCommand);

            RTStrFree(pszCommand);

            if (WEXITSTATUS(status) != 0)  /* Utility or extension not available. */
            {
                pszCommand = RTStrAPrintf2("xmessage -buttons OK:0 -center \"VBoxClient: %s\"",
                                           pszMessage);
                if (pszCommand)
                {
                    status = system(pszCommand);
                    if (WEXITSTATUS(status) != 0)  /* Utility or extension not available. */
                    {
                        RTPrintf("VBoxClient: %s", pszMessage);
                    }

                    RTStrFree(pszCommand);
                }
                else
                    rc = VERR_NO_MEMORY;
            }
        }
        else
            rc = VERR_NO_MEMORY;
    }

    return rc;
}

/**
 * Logs a fatal error, notifies the desktop environment via a message and
 * exits the application immediately.
 *
 * @param   pszFormat           Format string to log.
 * @param   ...                 Variable arguments for format string. Optional.
 */
void VBClLogFatalError(const char *pszFormat, ...)
{
    va_list args;
    va_start(args, pszFormat);
    char *psz = NULL;
    RTStrAPrintfV(&psz, pszFormat, args);
    va_end(args);

    AssertPtr(psz);
    LogFlowFunc(("%s", psz));
    LogRel(("%s", psz));

    vbclLogNotify(psz);

    RTStrFree(psz);
}

/**
 * Logs an error message to the (release) logging instance.
 *
 * @param   pszFormat               Format string to log.
 */
void VBClLogError(const char *pszFormat, ...)
{
    va_list args;
    va_start(args, pszFormat);
    char *psz = NULL;
    RTStrAPrintfV(&psz, pszFormat, args);
    va_end(args);

    AssertPtr(psz);
    LogFlowFunc(("%s", psz));
    LogRel(("%s", psz));

    RTStrFree(psz);
}

/**
 * Logs an info message to the (release) logging instance.
 *
 * @param   pszFormat               Format string to log.
 */
void  VBClLogInfo(const char *pszFormat, ...)
{
    va_list args;
    va_start(args, pszFormat);
    char *psz = NULL;
    RTStrAPrintfV(&psz, pszFormat, args);
    va_end(args);

    AssertPtr(psz);
    LogFlowFunc(("%s", psz));
    LogRel(("%s", psz));

    RTStrFree(psz);
}

/**
 * @callback_method_impl{FNRTLOGPHASE, Release logger callback}
 */
static DECLCALLBACK(void) vbClLogHeaderFooter(PRTLOGGER pLoggerRelease, RTLOGPHASE enmPhase, PFNRTLOGPHASEMSG pfnLog)
{
    /* Some introductory information. */
    static RTTIMESPEC s_TimeSpec;
    char szTmp[256];
    if (enmPhase == RTLOGPHASE_BEGIN)
        RTTimeNow(&s_TimeSpec);
    RTTimeSpecToString(&s_TimeSpec, szTmp, sizeof(szTmp));

    switch (enmPhase)
    {
        case RTLOGPHASE_BEGIN:
        {
            pfnLog(pLoggerRelease,
                   "VBoxClient %s r%s (verbosity: %u) %s (%s %s) release log\n"
                   "Log opened %s\n",
                   RTBldCfgVersion(), RTBldCfgRevisionStr(), g_cVerbosity, VBOX_BUILD_TARGET,
                   __DATE__, __TIME__, szTmp);

            int vrc = RTSystemQueryOSInfo(RTSYSOSINFO_PRODUCT, szTmp, sizeof(szTmp));
            if (RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW)
                pfnLog(pLoggerRelease, "OS Product: %s\n", szTmp);
            vrc = RTSystemQueryOSInfo(RTSYSOSINFO_RELEASE, szTmp, sizeof(szTmp));
            if (RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW)
                pfnLog(pLoggerRelease, "OS Release: %s\n", szTmp);
            vrc = RTSystemQueryOSInfo(RTSYSOSINFO_VERSION, szTmp, sizeof(szTmp));
            if (RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW)
                pfnLog(pLoggerRelease, "OS Version: %s\n", szTmp);
            vrc = RTSystemQueryOSInfo(RTSYSOSINFO_SERVICE_PACK, szTmp, sizeof(szTmp));
            if (RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW)
                pfnLog(pLoggerRelease, "OS Service Pack: %s\n", szTmp);

            /* the package type is interesting for Linux distributions */
            char szExecName[RTPATH_MAX];
            char *pszExecName = RTProcGetExecutablePath(szExecName, sizeof(szExecName));
            pfnLog(pLoggerRelease,
                   "Executable: %s\n"
                   "Process ID: %u\n"
                   "Package type: %s"
#ifdef VBOX_OSE
                   " (OSE)"
#endif
                   "\n",
                   pszExecName ? pszExecName : "unknown",
                   RTProcSelf(),
                   VBOX_PACKAGE_STRING);
            break;
        }

        case RTLOGPHASE_PREROTATE:
            pfnLog(pLoggerRelease, "Log rotated - Log started %s\n", szTmp);
            break;

        case RTLOGPHASE_POSTROTATE:
            pfnLog(pLoggerRelease, "Log continuation - Log started %s\n", szTmp);
            break;

        case RTLOGPHASE_END:
            pfnLog(pLoggerRelease, "End of log file - Log started %s\n", szTmp);
            break;

        default:
            /* nothing */
            break;
    }
}

/**
 * Creates the default release logger outputting to the specified file.
 *
 * Pass NULL to disabled logging.
 *
 * @return  IPRT status code.
 * @param   pszLogFile      Filename for log output.  NULL disables custom handling.
 */
int VBClLogCreate(const char *pszLogFile)
{
    if (!pszLogFile)
        return VINF_SUCCESS;

    /* Create release logger (stdout + file). */
    static const char * const s_apszGroups[] = VBOX_LOGGROUP_NAMES;
    RTUINT fFlags = RTLOGFLAGS_PREFIX_THREAD | RTLOGFLAGS_PREFIX_TIME;
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
    fFlags |= RTLOGFLAGS_USECRLF;
#endif
    int rc = RTLogCreateEx(&g_pLoggerRelease, fFlags, "all",
#ifdef DEBUG
                           "VBOXCLIENT_LOG",
#else
                           "VBOXCLIENT_RELEASE_LOG",
#endif
                           RT_ELEMENTS(s_apszGroups), s_apszGroups, UINT32_MAX /*cMaxEntriesPerGroup*/,
                           RTLOGDEST_STDOUT | RTLOGDEST_USER,
                           vbClLogHeaderFooter, g_cHistory, g_uHistoryFileSize, g_uHistoryFileTime,
                           NULL /*pErrInfo*/, "%s", pszLogFile ? pszLogFile : "");
    if (RT_SUCCESS(rc))
    {
        /* register this logger as the release logger */
        RTLogRelSetDefaultInstance(g_pLoggerRelease);

        /* Explicitly flush the log in case of VBOXSERVICE_RELEASE_LOG=buffered. */
        RTLogFlush(g_pLoggerRelease);
    }

    return rc;
}

/**
 * Destroys the currently active logging instance.
 */
void VBClLogDestroy(void)
{
    RTLogDestroy(RTLogRelSetDefaultInstance(NULL));
}

/**
 * Clean up if we get a signal or something.
 *
 * This is extern so that we can call it from other compilation units.
 */
void VBClCleanUp(bool fExit /*=true*/)
{
    /* We never release this, as we end up with a call to exit(3) which is not
     * async-safe.  Unless we fix this application properly, we should be sure
     * never to exit from anywhere except from this method. */
    int rc = RTCritSectEnter(&g_critSect);
    if (RT_FAILURE(rc))
        VBClLogFatalError("Failure while acquiring the global critical section, rc=%Rrc\n", rc);
    if (g_pService)
        (*g_pService)->cleanup(g_pService);
    if (g_szPidFile[0] && g_hPidFile)
        VbglR3ClosePidFile(g_szPidFile, g_hPidFile);

    VBClLogDestroy();

    if (fExit)
        exit(RTEXITCODE_SUCCESS);
}

/**
 * A standard signal handler which cleans up and exits.
 */
static void vboxClientSignalHandler(int cSignal)
{
    VBClLogInfo("Terminated with signal %d\n", cSignal);
    /** Disable seamless mode */
    VBClLogInfo("Terminating ...\n");
    VBClCleanUp();
}

/**
 * Xlib error handler for certain errors that we can't avoid.
 */
static int vboxClientXLibErrorHandler(Display *pDisplay, XErrorEvent *pError)
{
    char errorText[1024];

    XGetErrorText(pDisplay, pError->error_code, errorText, sizeof(errorText));
    VBClLogError("An X Window protocol error occurred: %s (error code %d).  Request code: %d, minor code: %d, serial number: %d\n", errorText, pError->error_code, pError->request_code, pError->minor_code, pError->serial);
    return 0;
}

/**
 * Xlib error handler for fatal errors.  This often means that the programme is still running
 * when X exits.
 */
static int vboxClientXLibIOErrorHandler(Display *pDisplay)
{
    RT_NOREF1(pDisplay);
    VBClLogError("A fatal guest X Window error occurred.  This may just mean that the Window system was shut down while the client was still running\n");
    VBClCleanUp();
    return 0;  /* We should never reach this. */
}

/**
 * Reset all standard termination signals to call our signal handler, which
 * cleans up and exits.
 */
static void vboxClientSetSignalHandlers(void)
{
    struct sigaction sigAction;

    LogRelFlowFuncEnter();
    sigAction.sa_handler = vboxClientSignalHandler;
    sigemptyset(&sigAction.sa_mask);
    sigAction.sa_flags = 0;
    sigaction(SIGHUP, &sigAction, NULL);
    sigaction(SIGINT, &sigAction, NULL);
    sigaction(SIGQUIT, &sigAction, NULL);
    sigaction(SIGPIPE, &sigAction, NULL);
    sigaction(SIGALRM, &sigAction, NULL);
    sigaction(SIGTERM, &sigAction, NULL);
    sigaction(SIGUSR1, &sigAction, NULL);
    sigaction(SIGUSR2, &sigAction, NULL);
    LogRelFlowFuncLeave();
}

/**
 * Print out a usage message and exit with success.
 */
static void vboxClientUsage(const char *pcszFileName)
{
    RTPrintf("Usage: %s "
#ifdef VBOX_WITH_SHARED_CLIPBOARD
             "--clipboard|"
#endif
#ifdef VBOX_WITH_DRAG_AND_DROP
             "--draganddrop|"
#endif
             "--display|"
# ifdef VBOX_WITH_GUEST_PROPS
             "--checkhostversion|"
#endif
             "--seamless|"
             "--vmsvga|--vmsvga-x11"
             "[-d|--nodaemon]\n", pcszFileName);
    RTPrintf("Starts the VirtualBox DRM/X Window System guest services.\n\n");
    RTPrintf("Options:\n");
#ifdef VBOX_WITH_SHARED_CLIPBOARD
    RTPrintf("  --clipboard        starts the shared clipboard service\n");
#endif
#ifdef VBOX_WITH_DRAG_AND_DROP
    RTPrintf("  --draganddrop      starts the drag and drop service\n");
#endif
    RTPrintf("  --display          starts the display management service\n");
#ifdef VBOX_WITH_GUEST_PROPS
    RTPrintf("  --checkhostversion starts the host version notifier service\n");
#endif
    RTPrintf("  --seamless         starts the seamless windows service\n");
    RTPrintf("  --vmsvga           starts VMSVGA dynamic resizing for DRM\n");
    RTPrintf("  --vmsvga-x11       starts VMSVGA dynamic resizing for X11\n");
    RTPrintf("  -f, --foreground   run in the foreground (no daemonizing)\n");
    RTPrintf("  -d, --nodaemon     continues running as a system service\n");
    RTPrintf("  -h, --help         shows this help text\n");
    RTPrintf("  -v, --verbose      increases logging verbosity level\n");
    RTPrintf("  -V, --version      shows version information\n");
    RTPrintf("\n");
}

/**
 * Complains about seeing more than one service specification.
 *
 * @returns RTEXITCODE_SYNTAX.
 */
static int vbclSyntaxOnlyOneService(void)
{
    RTMsgError("More than one service specified! Only one, please.");
    return RTEXITCODE_SYNTAX;
}

/**
 * The main loop for the VBoxClient daemon.
 * @todo Clean up for readability.
 */
int main(int argc, char *argv[])
{
    /* Initialise our runtime before all else. */
    int rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    /* This should never be called twice in one process - in fact one Display
     * object should probably never be used from multiple threads anyway. */
    if (!XInitThreads())
        VBClLogFatalError("Failed to initialize X11 threads\n");

    /* Get our file name for usage info and hints. */
    const char *pcszFileName = RTPathFilename(argv[0]);
    if (!pcszFileName)
        pcszFileName = "VBoxClient";

    /* Parse our option(s). */
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--nodaemon",                     'd',                                      RTGETOPT_REQ_NOTHING },
        { "--foreground",                   'f',                                      RTGETOPT_REQ_NOTHING },
        { "--help",                         'h',                                      RTGETOPT_REQ_NOTHING },
        { "--logfile",                      'l',                                      RTGETOPT_REQ_STRING  },
        { "--no-respawn",                   VBOXCLIENT_OPT_NORESPAWN,                 RTGETOPT_REQ_NOTHING },
        { "--version",                      'V',                                      RTGETOPT_REQ_NOTHING },
        { "--verbose",                      'v',                                      RTGETOPT_REQ_NOTHING },

        /* Services */
        { "--checkhostversion",             VBOXCLIENT_OPT_CHECKHOSTVERSION,          RTGETOPT_REQ_NOTHING },
#ifdef VBOX_WITH_SHARED_CLIPBOARD
        { "--clipboard",                    VBOXCLIENT_OPT_CLIPBOARD,                 RTGETOPT_REQ_NOTHING },
#endif
#ifdef VBOX_WITH_DRAG_AND_DROP
        { "--draganddrop",                  VBOXCLIENT_OPT_DRAGANDDROP,               RTGETOPT_REQ_NOTHING },
#endif
        { "--seamless",                     VBOXCLIENT_OPT_SEAMLESS,                  RTGETOPT_REQ_NOTHING },
        { "--vmsvga",                       VBOXCLIENT_OPT_VMSVGA,                    RTGETOPT_REQ_NOTHING },
        { "--vmsvga-x11",                   VBOXCLIENT_OPT_VMSVGA_X11,                RTGETOPT_REQ_NOTHING }
    };

    int                     ch;
    RTGETOPTUNION           ValueUnion;
    RTGETOPTSTATE           GetState;
    rc = RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 0, 0 /* fFlags */);
    AssertRC(rc);

    bool fDaemonise = true;
    bool fRespawn   = true;

    while ((ch = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        /* For options that require an argument, ValueUnion has received the value. */
        switch (ch)
        {
            case 'd':
            {
                fDaemonise = false;
                break;
            }

            case 'h':
            {
                vboxClientUsage(pcszFileName);
                return RTEXITCODE_SUCCESS;
            }

            case 'f':
            {
               fDaemonise = false;
               fRespawn   = false;
               break;
            }

            case 'l':
            {
                rc = RTStrCopy(g_szLogFile, sizeof(g_szLogFile), ValueUnion.psz);
                if (RT_FAILURE(rc))
                    VBClLogFatalError("Unable to create log file path, rc=%Rrc\n", rc);
                break;
            }

            case 'n':
            {
                fRespawn   = false;
                break;
            }

            case 'v':
            {
                g_cVerbosity++;
                break;
            }

            case 'V':
            {
                RTPrintf("%sr%s\n", RTBldCfgVersion(), RTBldCfgRevisionStr());
                return RTEXITCODE_SUCCESS;
            }

            /* Services */

            case VBOXCLIENT_OPT_CHECKHOSTVERSION:
            {
                if (g_pService)
                    return vbclSyntaxOnlyOneService();
                g_pService = VBClGetHostVersionService();
                break;
            }

#ifdef VBOX_WITH_SHARED_CLIPBOARD
            case VBOXCLIENT_OPT_CLIPBOARD:
            {
                if (g_pService)
                    return vbclSyntaxOnlyOneService();
                g_pService = VBClGetClipboardService();
                break;
            }
#endif
#ifdef VBOX_WITH_DRAG_AND_DROP
            case VBOXCLIENT_OPT_DRAGANDDROP:
            {
                if (g_pService)
                    return vbclSyntaxOnlyOneService();
                g_pService = VBClGetDragAndDropService();
                break;
            }
#endif
            case VBOXCLIENT_OPT_SEAMLESS:
            {
                if (g_pService)
                    return vbclSyntaxOnlyOneService();
                g_pService = VBClGetSeamlessService();
                break;
            }

            case VBOXCLIENT_OPT_VMSVGA:
            {
                if (g_pService)
                    return vbclSyntaxOnlyOneService();
                g_pService = VBClDisplaySVGAService();
                break;
            }

            case VBOXCLIENT_OPT_VMSVGA_X11:
            {
                if (g_pService)
                    return vbclSyntaxOnlyOneService();
                g_pService = VBClDisplaySVGAX11Service();
                break;
            }

            case VERR_GETOPT_UNKNOWN_OPTION:
            {
                RTMsgError("unrecognized option '%s'", ValueUnion.psz);
                RTMsgInfo("Try '%s --help' for more information", pcszFileName);
                return RTEXITCODE_SYNTAX;
            }

            case VINF_GETOPT_NOT_OPTION:
            default:
                break;

        } /* switch */
    } /* while RTGetOpt */

    if (!g_pService)
    {
        RTMsgError("No service specified. Quitting because nothing to do!");
        return RTEXITCODE_SYNTAX;
    }

    /* Initialize VbglR3 before we do anything else with the logger. */
    rc = VbglR3InitUser();
    if (RT_FAILURE(rc))
        VBClLogFatalError("VbglR3InitUser failed: %Rrc", rc);

    rc = VBClLogCreate(g_szLogFile[0] ? g_szLogFile : NULL);
    if (RT_FAILURE(rc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to create release log '%s', rc=%Rrc\n",
                              g_szLogFile[0] ? g_szLogFile : "<None>", rc);

    LogRel(("Service: %s\n", (*g_pService)->getName()));

    if (!fDaemonise)
    {
        /* If the user is running in "no daemon" mode, send critical logging to stdout as well. */
        PRTLOGGER pReleaseLog = RTLogRelGetDefaultInstance();
        if (pReleaseLog)
        {
            rc = RTLogDestinations(pReleaseLog, "stdout");
            if (RT_FAILURE(rc))
                return RTMsgErrorExitFailure("Failed to redivert error output, rc=%Rrc", rc);
        }
    }

    rc = RTCritSectInit(&g_critSect);
    if (RT_FAILURE(rc))
        VBClLogFatalError("Initialising critical section failed: %Rrc\n", rc);
    if ((*g_pService)->getPidFilePath)
    {
        rc = RTPathUserHome(g_szPidFile, sizeof(g_szPidFile));
        if (RT_FAILURE(rc))
            VBClLogFatalError("Getting home directory for PID file failed: %Rrc\n", rc);
        rc = RTPathAppend(g_szPidFile, sizeof(g_szPidFile),
                          (*g_pService)->getPidFilePath());
        if (RT_FAILURE(rc))
            VBClLogFatalError("Creating PID file path failed: %Rrc\n", rc);
        if (fDaemonise)
            rc = VbglR3Daemonize(false /* fNoChDir */, false /* fNoClose */, fRespawn, &g_cRespawn);
        if (RT_FAILURE(rc))
            VBClLogFatalError("Daemonizing failed: %Rrc\n", rc);
        if (g_szPidFile[0])
            rc = VbglR3PidFile(g_szPidFile, &g_hPidFile);
        if (rc == VERR_FILE_LOCK_VIOLATION)  /* Already running. */
            return RTEXITCODE_SUCCESS;
        if (RT_FAILURE(rc))
            VBClLogFatalError("Creating PID file failed: %Rrc\n", rc);
    }
    /* Set signal handlers to clean up on exit. */
    vboxClientSetSignalHandlers();
#ifndef VBOXCLIENT_WITHOUT_X11
    /* Set an X11 error handler, so that we don't die when we get unavoidable
     * errors. */
    XSetErrorHandler(vboxClientXLibErrorHandler);
    /* Set an X11 I/O error handler, so that we can shutdown properly on
     * fatal errors. */
    XSetIOErrorHandler(vboxClientXLibIOErrorHandler);
#endif
    rc = (*g_pService)->init(g_pService);
    if (RT_SUCCESS(rc))
    {
        rc = (*g_pService)->run(g_pService, fDaemonise);
        if (RT_FAILURE(rc))
            VBClLogError("Running service failed: %Rrc\n", rc);
    }
    else
    {
        /** @todo r=andy Should we return an appropriate exit code if the service failed to init?
         *               Must be tested carefully with our init scripts first. */
        VBClLogError("Initializing service failed: %Rrc\n", rc);
    }
    VBClCleanUp(false /*fExit*/);
    return RTEXITCODE_SUCCESS;
}
