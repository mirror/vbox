/* $Id$ */
/** @file
 * VirtualBox Autostart Service - Windows Specific Code.
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
#include <Windows.h>

#include <VBox/log.h>
#include <VBox/version.h>
#include <iprt/string.h>
#include <iprt/mem.h>
#include <iprt/initterm.h>
#include <iprt/stream.h>
#include <iprt/getopt.h>
#include <iprt/semaphore.h>

#include "VBoxAutostart.h"

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The service name. */
#define AUTOSTART_SERVICE_NAME             "VBoxAutostartSvc"
/** The service display name. */
#define AUTOSTART_SERVICE_DISPLAY_NAME     "VirtualBox Autostart Service"


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The service control handler handle. */
static SERVICE_STATUS_HANDLE g_hSupSvcWinCtrlHandler = NULL;
/** The service status. */
static uint32_t volatile g_u32SupSvcWinStatus = SERVICE_STOPPED;
/** The semaphore the main service thread is waiting on in autostartSvcWinServiceMain. */
static RTSEMEVENTMULTI g_hSupSvcWinEvent = NIL_RTSEMEVENTMULTI;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static SC_HANDLE autostartSvcWinOpenSCManager(const char *pszAction, DWORD dwAccess);


/**
 * Opens the service control manager.
 *
 * When this fails, an error message will be displayed.
 *
 * @returns Valid handle on success.
 *          NULL on failure, will display an error message.
 *
 * @param   pszAction       The action which is requesting access to SCM.
 * @param   dwAccess        The desired access.
 */
static SC_HANDLE autostartSvcWinOpenSCManager(const char *pszAction, DWORD dwAccess)
{
    SC_HANDLE hSCM = OpenSCManager(NULL /* lpMachineName*/, NULL /* lpDatabaseName */, dwAccess);
    if (hSCM == NULL)
    {
        DWORD err = GetLastError();
        switch (err)
        {
            case ERROR_ACCESS_DENIED:
                autostartSvcDisplayError("%s - OpenSCManager failure: access denied\n", pszAction);
                break;
            default:
                autostartSvcDisplayError("%s - OpenSCManager failure: %d\n", pszAction, err);
                break;
        }
    }
    return hSCM;
}


/**
 * Opens the service.
 *
 * Last error is preserved on failure and set to 0 on success.
 *
 * @returns Valid service handle on success.
 *          NULL on failure, will display an error message unless it's ignored.
 *
 * @param   pszAction           The action which is requesting access to the service.
 * @param   dwSCMAccess         The service control manager access.
 * @param   dwSVCAccess         The desired service access.
 * @param   cIgnoredErrors      The number of ignored errors.
 * @param   ...                 Errors codes that should not cause a message to be displayed.
 */
static SC_HANDLE autostartSvcWinOpenService(const char *pszAction, DWORD dwSCMAccess, DWORD dwSVCAccess,
                                      unsigned cIgnoredErrors, ...)
{
    SC_HANDLE hSCM = autostartSvcWinOpenSCManager(pszAction, dwSCMAccess);
    if (!hSCM)
        return NULL;

    SC_HANDLE hSvc = OpenService(hSCM, SUPSVC_SERVICE_NAME, dwSVCAccess);
    if (hSvc)
    {
        CloseServiceHandle(hSCM);
        SetLastError(0);
    }
    else
    {
        DWORD   err = GetLastError();
        bool    fIgnored = false;
        va_list va;
        va_start(va, cIgnoredErrors);
        while (!fIgnored && cIgnoredErrors-- > 0)
            fIgnored = va_arg(va, long) == err;
        va_end(va);
        if (!fIgnored)
        {
            switch (err)
            {
                case ERROR_ACCESS_DENIED:
                    autostartSvcDisplayError("%s - OpenService failure: access denied\n", pszAction);
                    break;
                case ERROR_SERVICE_DOES_NOT_EXIST:
                    autostartSvcDisplayError("%s - OpenService failure: The service does not exist. Reinstall it.\n", pszAction);
                    break;
                default:
                    autostartSvcDisplayError("%s - OpenService failure: %d\n", pszAction, err);
                    break;
            }
        }

        CloseServiceHandle(hSCM);
        SetLastError(err);
    }
    return hSvc;
}



void autostartSvcOsLogErrorStr(const char *pszMsg)
{
    HANDLE hEventLog = RegisterEventSource(NULL /* local computer */, "VBoxAutostartSvc");
    AssertReturnVoid(hEventLog != NULL);
    const char *apsz[2];
    apsz[0] = "VBoxAutostartSvc";
    apsz[1] = pszMsg;
    BOOL fRc = ReportEvent(hEventLog,               /* hEventLog */
                           EVENTLOG_ERROR_TYPE,     /* wType */
                           0,                       /* wCategory */
                           0 /** @todo mc */,       /* dwEventID */
                           NULL,                    /* lpUserSid */
                           RT_ELEMENTS(apsz),       /* wNumStrings */
                           0,                       /* dwDataSize */
                           apsz,                    /* lpStrings */
                           NULL);                   /* lpRawData */
    AssertMsg(fRc, ("%d\n", GetLastError()));
    DeregisterEventSource(hEventLog);
}


static RTEXITCODE autostartSvcWinInterrogate(int argc, char **argv)
{
    RTPrintf("VBoxAutostartSvc: The \"interrogate\" action is not implemented.\n");
    return RTEXITCODE_FAILURE;
}


static RTEXITCODE autostartSvcWinStop(int argc, char **argv)
{
    RTPrintf("VBoxAutostartSvc: The \"stop\" action is not implemented.\n");
    return RTEXITCODE_FAILURE;
}


static RTEXITCODE autostartSvcWinContinue(int argc, char **argv)
{
    RTPrintf("VBoxAutostartSvc: The \"continue\" action is not implemented.\n");
    return RTEXITCODE_FAILURE;
}


static RTEXITCODE autostartSvcWinPause(int argc, char **argv)
{
    RTPrintf("VBoxAutostartSvc: The \"pause\" action is not implemented.\n");
    return RTEXITCODE_FAILURE;
}


static RTEXITCODE autostartSvcWinStart(int argc, char **argv)
{
    RTPrintf("VBoxAutostartSvc: The \"start\" action is not implemented.\n");
    return RTEXITCODE_FAILURE;
}


static RTEXITCODE autostartSvcWinQueryDescription(int argc, char **argv)
{
    RTPrintf("VBoxAutostartSvc: The \"qdescription\" action is not implemented.\n");
    return RTEXITCODE_FAILURE;
}


static RTEXITCODE autostartSvcWinQueryConfig(int argc, char **argv)
{
    RTPrintf("VBoxAutostartSvc: The \"qconfig\" action is not implemented.\n");
    return RTEXITCODE_FAILURE;
}


static RTEXITCODE autostartSvcWinDisable(int argc, char **argv)
{
    RTPrintf("VBoxAutostartSvc: The \"disable\" action is not implemented.\n");
    return RTEXITCODE_FAILURE;
}

static RTEXITCODE autostartSvcWinEnable(int argc, char **argv)
{
    RTPrintf("VBoxAutostartSvc: The \"enable\" action is not implemented.\n");
    return RTEXITCODE_FAILURE;
}


/**
 * Handle the 'delete' action.
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE.
 * @param   argc    The action argument count.
 * @param   argv    The action argument vector.
 */
static int autostartSvcWinDelete(int argc, char **argv)
{
    /*
     * Parse the arguments.
     */
    bool fVerbose = false;
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--verbose", 'v', RTGETOPT_REQ_NOTHING }
    };
    int ch;
    RTGETOPTUNION Value;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 0, RTGETOPTINIT_FLAGS_NO_STD_OPTS);
    while ((ch = RTGetOpt(&GetState, &Value)))
        switch (ch)
        {
            case 'v':
                fVerbose = true;
                break;
            case VINF_GETOPT_NOT_OPTION:
                return autostartSvcDisplayTooManyArgsError("delete", argc, argv, iArg);
            default:
                return autostartSvcDisplayGetOptError("delete", ch, argc, argv, iArg, &Value);
        }

    /*
     * Create the service.
     */
    int rc = RTEXITCODE_FAILURE;
    SC_HANDLE hSvc = autostartSvcWinOpenService("delete", SERVICE_CHANGE_CONFIG, DELETE,
                                          1, ERROR_SERVICE_DOES_NOT_EXIST);
    if (hSvc)
    {
        if (DeleteService(hSvc))
        {
            RTPrintf("Successfully deleted the %s service.\n", SUPSVC_SERVICE_NAME);
            rc = RTEXITCODE_SUCCESS;
        }
        else
            autostartSvcDisplayError("delete - DeleteService failed, err=%d.\n", GetLastError());
        CloseServiceHandle(hSvc);
    }
    else if (GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST)
    {

        if (fVerbose)
            RTPrintf("The service %s was not installed, nothing to be done.", SUPSVC_SERVICE_NAME);
        else
            RTPrintf("Successfully deleted the %s service.\n", SUPSVC_SERVICE_NAME);
        rc = RTEXITCODE_SUCCESS;
    }
    return rc;
}


/**
 * Handle the 'create' action.
 *
 * @returns 0 or 1.
 * @param   argc    The action argument count.
 * @param   argv    The action argument vector.
 */
static RTEXITCODE autostartSvcWinCreate(int argc, char **argv)
{
    /*
     * Parse the arguments.
     */
    bool fVerbose = false;
    const char *pszUser = NULL;
    const char *pszPwd = NULL;
    static const RTOPTIONDEF s_aOptions[] =
    {
        { "--verbose",  'v', RTGETOPT_REQ_NOTHING },
        { "--user",     'u', RTGETOPT_REQ_STRIN },
        { "--password", 'p', RTGETOPT_REQ_STRIN }
    };
    int iArg = 0;
    int ch;
    RTGETOPTUNION Value;
    while ((ch = RTGetOpt(argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), &iArg, &Value)))
        switch (ch)
        {
            case 'v':
                fVerbose = true;
                break;
            case 'u':
                pszUser = Value.psz;
                break;
            case 'p'
                pszPwd = Value.psz;
                break;
            default:
                return autostartSvcDisplayGetOptError("create", ch, argc, argv, iArg, &Value);
        }
    if (iArg != argc)
        return autostartSvcDisplayTooManyArgsError("create", argc, argv, iArg);

    /*
     * Create the service.
     */
    int rc = RTEXITCODE_FAILURE;
    SC_HANDLE hSCM = autostartSvcWinOpenSCManager("create", SC_MANAGER_CREATE_SERVICE); /*SC_MANAGER_ALL_ACCESS*/
    if (hSCM)
    {
        char szExecPath[MAX_PATH];
        if (GetModuleFileName(NULL /* the executable */, szExecPath, sizeof(szExecPath)))
        {
            if (fVerbose)
                RTPrintf("Creating the %s service, binary \"%s\"...\n",
                         SUPSVC_SERVICE_NAME, szExecPath); /* yea, the binary name isn't UTF-8, but wtf. */

            SC_HANDLE hSvc = CreateService(hSCM,                            /* hSCManager */
                                           SUPSVC_SERVICE_NAME,             /* lpServiceName */
                                           SUPSVC_SERVICE_DISPLAY_NAME,     /* lpDisplayName */
                                           SERVICE_CHANGE_CONFIG | SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG, /* dwDesiredAccess */
                                           SERVICE_WIN32_OWN_PROCESS,       /* dwServiceType ( | SERVICE_INTERACTIVE_PROCESS? ) */
                                           SERVICE_DEMAND_START/*_AUTO*/,   /* dwStartType */
                                           SERVICE_ERROR_NORMAL,            /* dwErrorControl */
                                           szExecPath,                      /* lpBinaryPathName */
                                           NULL,                            /* lpLoadOrderGroup */
                                           NULL,                            /* lpdwTagId */
                                           NULL,                            /* lpDependencies */
                                           pszUser,                         /* lpServiceStartName (NULL => LocalSystem) */
                                           pszPwd);                         /* lpPassword */
            if (hSvc)
            {
                RTPrintf("Successfully created the %s service.\n", SUPSVC_SERVICE_NAME);
                /** @todo Set the service description or it'll look weird in the vista service manager.
                 *  Anything else that should be configured? Start access or something? */
                rc = RTEXITCODE_SUCCESS;
                CloseServiceHandle(hSvc);
            }
            else
            {
                DWORD err = GetLastError();
                switch (err)
                {
                    case ERROR_SERVICE_EXISTS:
                        autostartSvcDisplayError("create - The service already exists.\n");
                        break;
                    default:
                        autostartSvcDisplayError("create - CreateService failed, err=%d.\n", GetLastError());
                        break;
                }
            }
            CloseServiceHandle(hSvc);
        }
        else
            autostartSvcDisplayError("create - Failed to obtain the executable path: %d\n", GetLastError());
    }
    return rc;
}


/**
 * Sets the service status, just a SetServiceStatus Wrapper.
 *
 * @returns See SetServiceStatus.
 * @param   dwStatus        The current status.
 * @param   iWaitHint       The wait hint, if < 0 then supply a default.
 * @param   dwExitCode      The service exit code.
 */
static bool autostartSvcWinSetServiceStatus(DWORD dwStatus, int iWaitHint, DWORD dwExitCode)
{
    SERVICE_STATUS SvcStatus;
    SvcStatus.dwServiceType         = SERVICE_WIN32_OWN_PROCESS;
    SvcStatus.dwWin32ExitCode       = dwExitCode;
    SvcStatus.dwServiceSpecificExitCode = 0;
    SvcStatus.dwWaitHint            = iWaitHint >= 0 ? iWaitHint : 3000;
    SvcStatus.dwCurrentState        = dwStatus;
    LogFlow(("autostartSvcWinSetServiceStatus: %d -> %d\n", g_u32SupSvcWinStatus, dwStatus));
    g_u32SupSvcWinStatus            = dwStatus;
    switch (dwStatus)
    {
        case SERVICE_START_PENDING:
            SvcStatus.dwControlsAccepted = 0;
            break;
        default:
            SvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
            break;
    }

    static DWORD dwCheckPoint = 0;
    switch (dwStatus)
    {
        case SERVICE_RUNNING:
        case SERVICE_STOPPED:
            SvcStatus.dwCheckPoint       = 0;
        default:
            SvcStatus.dwCheckPoint       = ++dwCheckPoint;
            break;
    }
    return SetServiceStatus(g_hSupSvcWinCtrlHandler, &SvcStatus) != FALSE;
}


/**
 * Service control handler (extended).
 *
 * @returns Windows status (see HandlerEx).
 * @retval  NO_ERROR if handled.
 * @retval  ERROR_CALL_NOT_IMPLEMENTED if not handled.
 *
 * @param   dwControl       The control code.
 * @param   dwEventType     Event type. (specific to the control?)
 * @param   pvEventData     Event data, specific to the event.
 * @param   pvContext       The context pointer registered with the handler.
 *                          Currently not used.
 */
static DWORD WINAPI autostartSvcWinServiceCtrlHandlerEx(DWORD dwControl, DWORD dwEventType, LPVOID pvEventData, LPVOID pvContext)
{
    LogFlow(("autostartSvcWinServiceCtrlHandlerEx: dwControl=%#x dwEventType=%#x pvEventData=%p\n",
             dwControl, dwEventType, pvEventData));

    switch (dwControl)
    {
        /*
         * Interrogate the service about it's current status.
         * MSDN says that this should just return NO_ERROR and does
         * not need to set the status again.
         */
        case SERVICE_CONTROL_INTERROGATE:
            return NO_ERROR;

        /*
         * Request to stop the service.
         */
        case SERVICE_CONTROL_STOP:
        {
            /*
             * Check if the real services can be stopped and then tell them to stop.
             */
            autostartSvcWinSetServiceStatus(SERVICE_STOP_PENDING, 3000, NO_ERROR);
            int rc = autostartSvcTryStopServices();
            if (RT_SUCCESS(rc))
            {
                /*
                 * Notify the main thread that we're done, it will wait for the
                 * real services to stop, destroy them, and finally set the windows
                 * service status to SERVICE_STOPPED and return.
                 */
                rc = RTSemEventMultiSignal(g_hSupSvcWinEvent);
                if (RT_FAILURE(rc))
                    autostartSvcLogError("SERVICE_CONTROL_STOP: RTSemEventMultiSignal failed, %Rrc\n", rc);
            }
            return NO_ERROR;
        }

        case SERVICE_CONTROL_PAUSE:
        case SERVICE_CONTROL_CONTINUE:
        case SERVICE_CONTROL_SHUTDOWN:
        case SERVICE_CONTROL_PARAMCHANGE:
        case SERVICE_CONTROL_NETBINDADD:
        case SERVICE_CONTROL_NETBINDREMOVE:
        case SERVICE_CONTROL_NETBINDENABLE:
        case SERVICE_CONTROL_NETBINDDISABLE:
        case SERVICE_CONTROL_DEVICEEVENT:
        case SERVICE_CONTROL_HARDWAREPROFILECHANGE:
        case SERVICE_CONTROL_POWEREVENT:
        case SERVICE_CONTROL_SESSIONCHANGE:
#ifdef SERVICE_CONTROL_PRESHUTDOWN /* vista */
        case SERVICE_CONTROL_PRESHUTDOWN:
#endif
        default:
            return ERROR_CALL_NOT_IMPLEMENTED;
    }

    NOREF(dwEventType);
    NOREF(pvEventData);
    NOREF(pvContext);
    return NO_ERROR;
}


/**
 * Windows Service Main.
 *
 * This is invoked when the service is started and should not return until
 * the service has been stopped.
 *
 * @param   cArgs           Argument count.
 * @param   papszArgs       Argument vector.
 */
static VOID WINAPI autostartSvcWinServiceMain(DWORD cArgs, LPSTR *papszArgs)
{
    LogFlowFuncEnter();

    /*
     * Register the control handler function for the service and report to SCM.
     */
    Assert(g_u32SupSvcWinStatus == SERVICE_STOPPED);
    g_hSupSvcWinCtrlHandler = RegisterServiceCtrlHandlerEx(SUPSVC_SERVICE_NAME, autostartSvcWinServiceCtrlHandlerEx, NULL);
    if (g_hSupSvcWinCtrlHandler)
    {
        DWORD err = ERROR_GEN_FAILURE;
        if (autostartSvcWinSetServiceStatus(SERVICE_START_PENDING, 3000, NO_ERROR))
        {
            /*
             * Parse arguments.
             */
            static const RTOPTIONDEF s_aOptions[] =
            {
                { "--dummy", 'd', RTGETOPT_REQ_NOTHING }
            };
            int iArg = 1; /* the first arg is the service name */
            int ch;
            int rc = 0;
            RTGETOPTUNION Value;
            while (   !rc
                   && (ch = RTGetOpt(cArgs, papszArgs, s_aOptions, RT_ELEMENTS(s_aOptions), &iArg, &Value)))
                switch (ch)
                {
                    default:    rc = autostartSvcLogGetOptError("main", ch, cArgs, papszArgs, iArg, &Value); break;
                }
            if (iArg != cArgs)
                rc = autostartSvcLogTooManyArgsError("main", cArgs, papszArgs, iArg);
            if (!rc)
            {
                /*
                 * Create the event semaphore we'll be waiting on and
                 * then instantiate the actual services.
                 */
                int rc = RTSemEventMultiCreate(&g_hSupSvcWinEvent);
                if (RT_SUCCESS(rc))
                {
                    rc = autostartSvcCreateAndStartServices();
                    if (RT_SUCCESS(rc))
                    {
                        /*
                         * Update the status and enter the work loop.
                         *
                         * The work loop is just a dummy wait here as the services run
                         * in independent threads.
                         */
                        if (autostartSvcWinSetServiceStatus(SERVICE_RUNNING, 0, 0))
                        {
                            LogFlow(("autostartSvcWinServiceMain: calling RTSemEventMultiWait\n"));
                            /** @todo: Implement autostart */
                            rc = RTSemEventMultiWait(g_hSupSvcWinEvent, RT_INDEFINITE_WAIT);
                            if (RT_SUCCESS(rc))
                            {
                                LogFlow(("autostartSvcWinServiceMain: woke up\n"));
                                err = NO_ERROR;
                            }
                            else
                                autostartSvcLogError("RTSemEventWait failed, rc=%Rrc", rc);
                        }
                        else
                        {
                            err = GetLastError();
                            autostartSvcLogError("SetServiceStatus failed, err=%d", err);
                        }

                        /*
                         * Destroy the service instances, stopping them if
                         * they're still running (weird failure cause).
                         */
                        autostartSvcStopAndDestroyServices();
                    }

                    RTSemEventMultiDestroy(g_hSupSvcWinEvent);
                    g_hSupSvcWinEvent = NIL_RTSEMEVENTMULTI;
                }
                else
                    autostartSvcLogError("RTSemEventMultiCreate failed, rc=%Rrc", rc);
            }
            /* else: bad args */
        }
        else
        {
            err = GetLastError();
            autostartSvcLogError("SetServiceStatus failed, err=%d", err);
        }
        autostartSvcWinSetServiceStatus(SERVICE_STOPPED, 0, err);
    }
    else
        autostartSvcLogError("RegisterServiceCtrlHandlerEx failed, err=%d", GetLastError());
    LogFlowFuncLeave();
}


/**
 * Handle the 'create' action.
 *
 * @returns RTEXITCODE_SUCCESS or RTEXITCODE_FAILURE.
 * @param   argc    The action argument count.
 * @param   argv    The action argument vector.
 */
static int autostartSvcWinRunIt(int argc, char **argv)
{
    LogFlowFuncEnter();

    /*
     * Initialize release logging.
     */
    /** @todo release logging of the system-wide service. */

    /*
     * Parse the arguments.
     */
    static const RTOPTIONDEF s_aOptions[] =
    {
        { "--dummy", 'd', RTGETOPT_REQ_NOTHING }
    };
    int iArg = 0;
    int ch;
    RTGETOPTUNION Value;
    while ((ch = RTGetOpt(argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), &iArg, &Value)))
        switch (ch)
        {
            default:    return autostartSvcDisplayGetOptError("runit", ch, argc, argv, iArg, &Value);
        }
    if (iArg != argc)
        return autostartSvcDisplayTooManyArgsError("runit", argc, argv, iArg);

    /*
     * Register the service with the service control manager
     * and start dispatching requests from it (all done by the API).
     */
    static SERVICE_TABLE_ENTRY const s_aServiceStartTable[] =
    {
        { SUPSVC_SERVICE_NAME, autostartSvcWinServiceMain },
        { NULL, NULL}
    };
    if (StartServiceCtrlDispatcher(&s_aServiceStartTable[0]))
    {
        LogFlowFuncLeave();
        return RTEXITCODE_SUCCESS; /* told to quit, so quit. */
    }

    DWORD err = GetLastError();
    switch (err)
    {
        case ERROR_FAILED_SERVICE_CONTROLLER_CONNECT:
            autostartSvcDisplayError("Cannot run a service from the command line. Use the 'start' action to start it the right way.\n");
            break;
        default:
            autostartSvcLogError("StartServiceCtrlDispatcher failed, err=%d", err);
            break;
    }
    return RTEXITCODE_FAILURE;
}


/**
 * Show the version info.
 *
 * @returns RTEXITCODE_SUCCESS.
 */
static RTEXITCODE autostartSvcWinShowVersion(int argc, char **argv)
{
    /*
     * Parse the arguments.
     */
    bool fBrief = false;
    static const RTOPTIONDEF s_aOptions[] =
    {
        { "--brief", 'b', RTGETOPT_REQ_NOTHING }
    };
    int iArg = 0;
    int ch;
    RTGETOPTUNION Value;
    while ((ch = RTGetOpt(argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), &iArg, &Value)))
        switch (ch)
        {
            case 'b':   fBrief = true;  break;
            default:    return autostartSvcDisplayGetOptError("version", ch, argc, argv, iArg, &Value);

        }
    if (iArg != argc)
        return autostartSvcDisplayTooManyArgsError("version", argc, argv, iArg);

    /*
     * Do the printing.
     */
    if (fBrief)
        RTPrintf("%s\n", VBOX_VERSION_STRING);
    else
        RTPrintf("VirtualBox Autostart Service Version %s\n"
                 "(C) 2012 Oracle Corporation\n"
                 "All rights reserved.\n",
                 VBOX_VERSION_STRING);
    return 0;
}


/**
 * Show the usage help screen.
 *
 * @returns RTEXITCODE_SUCCESS.
 */
static RTEXITCODE autostartSvcWinShowHelp(void)
{
    RTPrintf("VirtualBox Autostart Service Version %s\n"
             "(C) 2012 Oracle Corporation\n"
             "All rights reserved.\n"
             "\n",
             VBOX_VERSION_STRING);
    RTPrintf("Usage:\n"
             "\n"
             "VBoxAutostartSvc\n"
             "      Runs the service.\n"
             "VBoxAutostartSvc <version|-v|--version> [-brief]\n"
             "      Displays the version.\n"
             "VBoxAutostartSvc <help|-?|-h|--help> [...]\n"
             "      Displays this help screen.\n"
             "\n"
             "VBoxAutostartSvc <install|/RegServer|/i>\n"
             "      Installs the service.\n"
             "VBoxAutostartSvc <install|delete|/UnregServer|/u>\n"
             "      Uninstalls the service.\n"
             );
    return RTEXITCODE_SUCCESS;
}


/**
 * VBoxAutostart main(), Windows edition.
 *
 *
 * @returns 0 on success.
 *
 * @param   argc    Number of arguments in argv.
 * @param   argv    Argument vector.
 */
RTEXITCODE main(int argc, char **argv)
{
    /*
     * Initialize the IPRT first of all.
     */
    int rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
    {
        autostartSvcLogError("RTR3InitExe failed with rc=%Rrc", rc);
        return 1;
    }

    /*
     * Parse the initial arguments to determine the desired action.
     */
    enum
    {
        kAutoSvcAction_RunIt,

        kAutoSvcAction_Create,
        kAutoSvcAction_Delete,

        kAutoSvcAction_Enable,
        kAutoSvcAction_Disable,
        kAutoSvcAction_QueryConfig,
        kAutoSvcAction_QueryDescription,

        kAutoSvcAction_Start,
        kAutoSvcAction_Pause,
        kAutoSvcAction_Continue,
        kAutoSvcAction_Stop,
        kAutoSvcAction_Interrogate,

        kAutoSvcAction_End
    } enmAction = kAutoSvcAction_RunIt;
    int iArg = 1;
    if (argc > 1)
    {
        if (    !stricmp(argv[iArg], "/RegServer")
            ||  !stricmp(argv[iArg], "install")
            ||  !stricmp(argv[iArg], "/i"))
            enmAction = kAutoSvcAction_Create;
        else if (   !stricmp(argv[iArg], "/UnregServer")
                 || !stricmp(argv[iArg], "/u")
                 || !stricmp(argv[iArg], "uninstall")
                 || !stricmp(argv[iArg], "delete"))
            enmAction = kAutoSvcAction_Delete;

        else if (!stricmp(argv[iArg], "enable"))
            enmAction = kAutoSvcAction_Enable;
        else if (!stricmp(argv[iArg], "disable"))
            enmAction = kAutoSvcAction_Disable;
        else if (!stricmp(argv[iArg], "qconfig"))
            enmAction = kAutoSvcAction_QueryConfig;
        else if (!stricmp(argv[iArg], "qdescription"))
            enmAction = kAutoSvcAction_QueryDescription;

        else if (   !stricmp(argv[iArg], "start")
                 || !stricmp(argv[iArg], "/t"))
            enmAction = kAutoSvcAction_Start;
        else if (!stricmp(argv[iArg], "pause"))
            enmAction = kAutoSvcAction_Start;
        else if (!stricmp(argv[iArg], "continue"))
            enmAction = kAutoSvcAction_Continue;
        else if (!stricmp(argv[iArg], "stop"))
            enmAction = kAutoSvcAction_Stop;
        else if (!stricmp(argv[iArg], "interrogate"))
            enmAction = kAutoSvcAction_Interrogate;
        else if (   !stricmp(argv[iArg], "help")
                 || !stricmp(argv[iArg], "?")
                 || !stricmp(argv[iArg], "/?")
                 || !stricmp(argv[iArg], "-?")
                 || !stricmp(argv[iArg], "/h")
                 || !stricmp(argv[iArg], "-h")
                 || !stricmp(argv[iArg], "/help")
                 || !stricmp(argv[iArg], "-help")
                 || !stricmp(argv[iArg], "--help"))
            return autostartSvcWinShowHelp();
        else if (   !stricmp(argv[iArg], "version")
                 || !stricmp(argv[iArg], "/v")
                 || !stricmp(argv[iArg], "-v")
                 || !stricmp(argv[iArg], "/version")
                 || !stricmp(argv[iArg], "-version")
                 || !stricmp(argv[iArg], "--version"))
            return autostartSvcWinShowVersion(argc - iArg - 1, argv + iArg + 1);
        else
            iArg--;
        iArg++;
    }

    /*
     * Dispatch it.
     */
    switch (enmAction)
    {
        case kAutoSvcAction_RunIt:
            return autostartSvcWinRunIt(argc - iArg, argv + iArg);

        case kAutoSvcAction_Create:
            return autostartSvcWinCreate(argc - iArg, argv + iArg);
        case kAutoSvcAction_Delete:
            return autostartSvcWinDelete(argc - iArg, argv + iArg);

        case kAutoSvcAction_Enable:
            return autostartSvcWinEnable(argc - iArg, argv + iArg);
        case kAutoSvcAction_Disable:
            return autostartSvcWinDisable(argc - iArg, argv + iArg);
        case kAutoSvcAction_QueryConfig:
            return autostartSvcWinQueryConfig(argc - iArg, argv + iArg);
        case kAutoSvcAction_QueryDescription:
            return autostartSvcWinQueryDescription(argc - iArg, argv + iArg);

        case kAutoSvcAction_Start:
            return autostartSvcWinStart(argc - iArg, argv + iArg);
        case kAutoSvcAction_Pause:
            return autostartSvcWinPause(argc - iArg, argv + iArg);
        case kAutoSvcAction_Continue:
            return autostartSvcWinContinue(argc - iArg, argv + iArg);
        case kAutoSvcAction_Stop:
            return autostartSvcWinStop(argc - iArg, argv + iArg);
        case kAutoSvcAction_Interrogate:
            return autostartSvcWinInterrogate(argc - iArg, argv + iArg);

        default:
            AssertMsgFailed(("enmAction=%d\n", enmAction));
            return RTEXITCODE_FAILURE;
    }
}

