/** $Id$ */
/** @file
 * VBoxService - Guest Additions Service Skeleton.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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
#ifndef _MSC_VER
# include <unistd.h>
#endif
#include <errno.h>

#include <iprt/thread.h>
#include <iprt/string.h>
#include <iprt/stream.h>
#include <iprt/initterm.h>
#include <iprt/asm.h>
#include <iprt/path.h>
#include <VBox/Log.h>
#include <VBox/VBoxGuest.h>
#include "VBoxServiceInternal.h"

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The program name (derived from argv[0]). */
char *g_pszProgName = "";
/** The current verbosity level. */
int g_cVerbosity = 0;
/** The default service interval (the -i | --interval) option). */
uint32_t g_DefaultInterval = 0;
/** Shutdown the main thread. (later, for signals). */
bool volatile g_fShutdown;

/**
 * The details of the services that has been compiled in.
 */
static struct
{
    /** Pointer to the service descriptor. */
    PCVBOXSERVICE   pDesc;
    /** The worker thread. NIL_RTTHREAD if it's the main thread. */
    RTTHREAD        Thread;
    /** Shutdown indicator. */
    bool volatile   fShutdown;
    /** Indicator set by the service thread exiting. */
    bool volatile   fStopped;
    /** Whether the service was started or not. */
    bool            fStarted;
    /** Whether the service is enabled or not. */
    bool            fEnabled;
} g_aServices[] =
{
#ifdef VBOXSERVICE_CONTROL
    { &g_Control,   NIL_RTTHREAD, false, false, false, true },
#endif
#ifdef VBOXSERVICE_TIMESYNC
    { &g_TimeSync,  NIL_RTTHREAD, false, false, false, true },
#endif
#ifdef VBOXSERVICE_CLIPBOARD
    { &g_Clipboard, NIL_RTTHREAD, false, false, false, true },
#endif
};


/**
 * Displays the program usage message.
 *
 * @returns 1.
 */
static int VBoxServiceUsage(void)
{
    RTPrintf("usage: %s [-f|--foreground] [-i|--interval <seconds>]"
             " [--disable-<service>] [--enable-<service>] [-h|-?|--help] [-v|--verbose]", g_pszProgName);

#if defined(RT_OS_WINDOWS)
    RTPrintf(" [-r|--register] [-u|--unregister]");
#endif

    for (unsigned j = 0; j < RT_ELEMENTS(g_aServices); j++)
        RTPrintf(" %s\n", g_aServices[j].pDesc->pszUsage);

    RTPrintf("\n"
             "Options:\n"
#if !defined(RT_OS_WINDOWS)
             "    -h | -? | --help         Show this message and exit with status 1.\n"
#else
             "    -h | -? | /? | --help    Show this message and exit with status 1.\n"
#endif
             "    -i | --interval          The default interval.\n"
             "    -f | --foreground        Don't daemonzie the program. For debugging.\n"
#if defined(RT_OS_WINDOWS)
             "    -r | --register          Installs the service.\n"
             "    -u | --unregister        Uninstall service.\n"
#endif
             "    -v | --verbose           Increment the verbosity level. For debugging.\n");

    RTPrintf("\n"
             "Service specific options:\n");
    for (unsigned j = 0; j < RT_ELEMENTS(g_aServices); j++)
    {
        RTPrintf("    --enable-%-10s Enables the %s service. (default)\n", g_aServices[j].pDesc->pszName, g_aServices[j].pDesc->pszName);
        RTPrintf("    --disable-%-9s Disables the %s service.\n", g_aServices[j].pDesc->pszName, g_aServices[j].pDesc->pszName);
        RTPrintf("%s", g_aServices[j].pDesc->pszOptions);
    }
    RTPrintf("\n"
             " Copyright (C) 2009 Sun Microsystems, Inc.\n");

    return 1;
}


/**
 * Displays a syntax error message.
 *
 * @returns 1
 * @param   pszFormat   The message text.
 * @param   ...         Format arguments.
 */
int VBoxServiceSyntax(const char *pszFormat, ...)
{
    RTStrmPrintf(g_pStdErr, "%s: syntax error: ", g_pszProgName);

    va_list va;
    va_start(va, pszFormat);
    RTStrmPrintfV(g_pStdErr, pszFormat, va);
    va_end(va);

    return 1;
}


/**
 * Displays an error message.
 *
 * @returns 1
 * @param   pszFormat   The message text.
 * @param   ...         Format arguments.
 */
int VBoxServiceError(const char *pszFormat, ...)
{
    char szBuffer[1024] = { 0 };
    RTStrmPrintf(g_pStdErr, "%s: error: ", g_pszProgName);

    va_list va;
    va_start(va, pszFormat);
    RTStrmPrintfV(g_pStdErr, pszFormat, va);

    RTStrPrintfV(szBuffer, sizeof(szBuffer), pszFormat, va);
    LogRel((szBuffer));

    va_end(va);

    return 1;
}


/**
 * Displays a verbose message.
 *
 * @returns 1
 * @param   pszFormat   The message text.
 * @param   ...         Format arguments.
 */
void VBoxServiceVerbose(int iLevel, const char *pszFormat, ...)
{
    if (iLevel <= g_cVerbosity)
    {
        RTStrmPrintf(g_pStdOut, "%s: ", g_pszProgName);
        va_list va;
        va_start(va, pszFormat);
        RTStrmPrintfV(g_pStdOut, pszFormat, va);

#ifdef _DEBUG
        char szBuffer[1024] = { 0 };
        RTStrPrintfV(szBuffer, sizeof(szBuffer), pszFormat, va);
        LogRel((szBuffer));
#endif

        va_end(va);
    }
}


/**
 * Gets a 32-bit value argument.
 *
 * @returns 0 on success, non-zero exit code on error.
 * @param   argc    The argument count.
 * @param   argv    The argument vector
 * @param   psz     Where in *pi to start looking for the value argument.
 * @param   pi      Where to find and perhaps update the argument index.
 * @param   pu32    Where to store the 32-bit value.
 * @param   u32Min  The minimum value.
 * @param   u32Max  The maximum value.
 */
int VBoxServiceArgUInt32(int argc, char **argv, const char *psz, int *pi, uint32_t *pu32, uint32_t u32Min, uint32_t u32Max)
{
    if (*psz == ':' || *psz == '=')
        psz++;
    if (!*psz)
    {
        if (*pi + 1 >= argc)
            return VBoxServiceSyntax("Missing value for the '%s' argument\n", argv[*pi]);
        psz = argv[++*pi];
    }

    char *pszNext;
    int rc = RTStrToUInt32Ex(psz, &pszNext, 0, pu32);
    if (RT_FAILURE(rc) || *pszNext)
        return VBoxServiceSyntax("Failed to convert interval '%s' to a number.\n", psz);
    if (*pu32 < u32Min || *pu32 > u32Max)
        return VBoxServiceSyntax("The timesync interval of %RU32 secconds is out of range [%RU32..%RU32].\n",
                                 *pu32, u32Min, u32Max);
    return 0;
}


/**
 * The service thread.
 *
 * @returns Whatever the worker function returns.
 * @param   ThreadSelf      My thread handle.
 * @param   pvUser          The service index.
 */
static DECLCALLBACK(int) VBoxServiceThread(RTTHREAD ThreadSelf, void *pvUser)
{
    const unsigned i = (uintptr_t)pvUser;
    int rc = g_aServices[i].pDesc->pfnWorker(&g_aServices[i].fShutdown);
    ASMAtomicXchgBool(&g_aServices[i].fShutdown, true);
    RTThreadUserSignal(ThreadSelf);
    return rc;
}

int VBoxServiceStartServices(int iMain)
{
    int rc = 0;

    /*
     * Initialize the services.
     */
    for (unsigned j = 0; j < RT_ELEMENTS(g_aServices); j++)
    {
        rc = g_aServices[j].pDesc->pfnInit();
        if (RT_FAILURE(rc))
            return VBoxServiceError("Service '%s' failed pre-init: %Rrc\n", g_aServices[j].pDesc->pszName);
    }

    if (iMain == 0) /* If 0 is specified, start all services. Useful for the Windows SCM stuff. */
        iMain = RT_ELEMENTS(g_aServices);

    /*
     * Start the service(s).
     */
    for (unsigned j = 0; j < RT_ELEMENTS(g_aServices); j++)
    {
        if (    !g_aServices[j].fEnabled
            ||  j == iMain)
            continue;

        rc = RTThreadCreate(&g_aServices[j].Thread, VBoxServiceThread, (void *)(uintptr_t)j, 0,
                            RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, g_aServices[j].pDesc->pszName);
        if (RT_FAILURE(rc))
        {
            VBoxServiceError("RTThreadCreate failed, rc=%Rrc\n", rc);
            break;
        }
        g_aServices[j].fStarted = true;

        /* wait for the thread to initialize */
        RTThreadUserWait(g_aServices[j].Thread, 60 * 1000);
        if (g_aServices[j].fShutdown)
            rc = VERR_GENERAL_FAILURE;
    }
    if (RT_SUCCESS(rc))
    {
        /* The final service runs in the main thread. */
        VBoxServiceVerbose(1, "starting '%s' in the main thread\n", g_aServices[iMain].pDesc->pszName);
        rc = g_aServices[iMain].pDesc->pfnWorker(&g_fShutdown);
        VBoxServiceError("service '%s' stopped unexpected; rc=%Rrc\n", g_aServices[iMain].pDesc->pszName, rc);
    }

    return rc;
}

/*
 * Stops and terminates the services.
 */
int VBoxServiceStopServices()
{
    int rc = 0;
    for (unsigned j = 0; j < RT_ELEMENTS(g_aServices); j++)
        ASMAtomicXchgBool(&g_aServices[j].fShutdown, true);
    for (unsigned j = 0; j < RT_ELEMENTS(g_aServices); j++)
        if (g_aServices[j].fStarted)
            g_aServices[j].pDesc->pfnStop();
    for (unsigned j = 0; j < RT_ELEMENTS(g_aServices); j++)
    {
        if (g_aServices[j].Thread != NIL_RTTHREAD)
        {
            rc = RTThreadWait(g_aServices[j].Thread, 30*1000, NULL);
            if (RT_FAILURE(rc))
                VBoxServiceError("service '%s' failed to stop. (%Rrc)\n", g_aServices[j].pDesc->pszName, rc);
        }
        g_aServices[j].pDesc->pfnTerm();
    }

    return rc;
}

int main(int argc, char **argv)
{
    int rc = 0;

#if defined(RT_OS_WINDOWS)
    /* Do not use a global namespace ("Global\\") for mutex name here, will blow up NT4 compatibility! */
    HANDLE hMutexAppRunning = CreateMutex (NULL, FALSE, VBOXSERVICE_NAME);
    if (   (hMutexAppRunning != NULL)
        && (GetLastError() == ERROR_ALREADY_EXISTS))
    {
        VBoxServiceError("%s is already running! Terminating.", g_pszProgName);

        /* Close the mutex for this application instance. */
        CloseHandle(hMutexAppRunning);
        hMutexAppRunning = NULL;
    }
#endif

    /*
     * Init globals and such.
     */
    RTR3Init();
    g_pszProgName = RTPathFilename(argv[0]);
    for (unsigned j = 0; j < RT_ELEMENTS(g_aServices); j++)
    {
        rc = g_aServices[j].pDesc->pfnPreInit();
        if (RT_FAILURE(rc))
            return VBoxServiceError("Service '%s' failed pre-init: %Rrc\n", g_aServices[j].pDesc->pszName);
    }

    /*
     * Parse the arguments.
     */
    bool fDaemonize = true;
    bool fDaemonzied = false;
    for (int i = 1; i < argc; i++)
    {
        const char *psz = argv[i];
        if(    (*psz != '-')
#if defined(RT_OS_WINDOWS)
            && (*psz != '/'))
#endif
            return VBoxServiceSyntax("Unknown argument '%s'\n", psz);
        psz++;

        /* translate long argument to short */
        if (*psz == '-')
        {
            psz++;
            size_t cch = strlen(psz);
#define MATCHES(strconst)       (   cch == sizeof(strconst) - 1 \
                                 && !memcmp(psz, strconst, sizeof(strconst) - 1) )
            if (MATCHES("foreground"))
                psz = "f";
            else if (MATCHES("verbose"))
                psz = "v";
            else if (MATCHES("help"))
                psz = "h";
            else if (MATCHES("interval"))
                psz = "i";
#if defined(RT_OS_WINDOWS)
            else if (MATCHES("register"))
                psz = "r";
            else if (MATCHES("unregister"))
                psz = "u";
#endif
            else if (MATCHES("daemonized"))
            {
                fDaemonzied = true;
                continue;
            }
            else
            {
                bool fFound = false;

                if (cch > sizeof("enable-") && !memcmp(psz, "enable-", sizeof("enable-") - 1))
                    for (unsigned j = 0; !fFound && j < RT_ELEMENTS(g_aServices); j++)
                        if ((fFound = !RTStrICmp(psz + sizeof("enable-") - 1, g_aServices[j].pDesc->pszName)))
                            g_aServices[j].fEnabled = true;

                if (cch > sizeof("disable-") && !memcmp(psz, "disable-", sizeof("disable-") - 1))
                    for (unsigned j = 0; !fFound && j < RT_ELEMENTS(g_aServices); j++)
                        if ((fFound = !RTStrICmp(psz + sizeof("disable-") - 1, g_aServices[j].pDesc->pszName)))
                            g_aServices[j].fEnabled = false;

                if (!fFound)
                    for (unsigned j = 0; !fFound && j < RT_ELEMENTS(g_aServices); j++)
                    {
                        rc = g_aServices[j].pDesc->pfnOption(NULL, argc, argv, &i);
                        fFound = rc == 0;
                        if (fFound)
                            break;
                        if (rc != -1)
                            return rc;
                    }
                if (!fFound)
                    return VBoxServiceSyntax("Unknown option '%s'\n", argv[i]);
                continue;
            }
#undef MATCHES
        }

        /* handle the string of short options. */
        do
        {
            switch (*psz)
            {
                case 'i':
                    rc = VBoxServiceArgUInt32(argc, argv, psz + 1, &i,
                                              &g_DefaultInterval, 1, (UINT32_MAX / 1000) - 1);
                    if (rc)
                        return rc;
                    psz = NULL;
                    break;

                case 'f':
                    fDaemonize = false;
                    break;

                case 'v':
                    g_cVerbosity++;
                    break;

#if defined(RT_OS_WINDOWS)
                case 'r':
                    return VBoxServiceWinInstall();

                case 'u':
                    return VBoxServiceWinUninstall();
#endif

                case 'h':
#if defined(RT_OS_WINDOWS)
                case '?':
#endif
                    return VBoxServiceUsage();

                default:
                {
                    bool fFound = false;
                    for (unsigned j = 0; j < RT_ELEMENTS(g_aServices); j++)
                    {
                        rc = g_aServices[j].pDesc->pfnOption(&psz, argc, argv, &i);
                        fFound = rc == 0;
                        if (fFound)
                            break;
                        if (rc != -1)
                            return rc;
                    }
                    if (!fFound)
                        return VBoxServiceSyntax("Unknown option '%c' (%s)\n", *psz, argv[i]);
                    break;
                }
            }
        } while (psz && *++psz);
    }

    /*
     * Check that at least one service is enabled.
     */
    unsigned iMain = ~0U;
    for (unsigned j = 0; j < RT_ELEMENTS(g_aServices); j++)
        if (g_aServices[j].fEnabled)
        {
            iMain = j;
            break;
        }
    if (iMain == ~0U)
        return VBoxServiceSyntax("At least one service must be enabled.\n");

    /*
     * Connect to the kernel part before daemonizing so we can fail
     * and complain if there is some kind of problem.
     */
    VBoxServiceVerbose(2, "Calling VbgR3Init()\n");
    rc = VbglR3Init();
    if (RT_FAILURE(rc))
        return VBoxServiceError("VbglR3Init failed with rc=%Rrc.\n", rc);

    /*
     * Daemonize if requested.
     */
    if (fDaemonize && !fDaemonzied)
    {
        VBoxServiceVerbose(1, "Daemonizing...\n");
#if defined(RT_OS_WINDOWS)
        /** @todo Replace StartServiceCtrlDispatcher() with
                  VbglR3Daemonize() once this has been ported
                  to Windows later. */
        StartServiceCtrlDispatcher (gs_serviceTable);
#else
        rc = VbglR3Daemonize(false /* fNoChDir */, false /* fNoClose */);
        if (RT_FAILURE(rc))
            return VBoxServiceError("daemon failed: %Rrc\n", rc);
        /* in-child */
#endif
    }

/** @todo Make the main thread responsive to signal so it can shutdown/restart the threads on non-SIGKILL signals. */

    rc = VBoxServiceStartServices(iMain);

#if defined(RT_OS_WINDOWS)
    if (fDaemonize && fDaemonzied)
    {
        while (1) {
            Sleep (100);       /** @todo */
        }
    }
#endif

    rc = VBoxServiceStopServices();

#if defined(RT_OS_WINDOWS)
    /* Release instance mutex. */
    if (hMutexAppRunning != NULL) {
        CloseHandle (hMutexAppRunning);
        hMutexAppRunning = NULL;
    }
#endif
    return rc;
}

