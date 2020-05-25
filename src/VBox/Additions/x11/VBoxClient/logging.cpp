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


#include <stdlib.h>
#include <iprt/buildconfig.h>
#include <iprt/file.h>
#include <iprt/process.h>
#include <iprt/stream.h>
#include <iprt/system.h>
#include <VBox/VBoxGuestLib.h>
#include <package-generated.h>
#include "VBoxClient.h"

/** Logging parameters. */
/** @todo Make this configurable later. */
static PRTLOGGER     g_pLoggerRelease = NULL;
static uint32_t      g_cHistory = 10;                   /* Enable log rotation, 10 files. */
static uint32_t      g_uHistoryFileTime = RT_SEC_1DAY;  /* Max 1 day per file. */
static uint64_t      g_uHistoryFileSize = 100 * _1M;    /* Max 100MB per file. */

extern unsigned      g_cRespawn;
extern unsigned      g_cVerbosity;

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

void foo()
{
}
