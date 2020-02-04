/* $Id$ */
/** @file
 * VirtualBox Autostart Service - Windows Specific Code.
 */

/*
 * Copyright (C) 2012-2020 Oracle Corporation
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
#include <iprt/win/windows.h>
#include <tchar.h>

#define SECURITY_WIN32
#include <Security.h>

#include <VBox/com/array.h>
#include <VBox/com/com.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint.h>
#include <VBox/com/Guid.h>
#include <VBox/com/listeners.h>
#include <VBox/com/NativeEventQueue.h>
#include <VBox/com/string.h>
#include <VBox/com/VirtualBox.h>

#include <VBox/log.h>
#include <VBox/version.h>

#include <iprt/env.h>
#include <iprt/errcore.h>
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/mem.h>
#include <iprt/process.h>
#include <iprt/path.h>
#include <iprt/semaphore.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/thread.h>

#include "VBoxAutostart.h"
#include "PasswordInput.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** The service name. */
#define AUTOSTART_SERVICE_NAME             "VBoxAutostartSvc"
/** The service display name. */
#define AUTOSTART_SERVICE_DISPLAY_NAME     "VirtualBox Autostart Service"

ComPtr<IVirtualBoxClient> g_pVirtualBoxClient = NULL;
bool                      g_fVerbose    = false;
ComPtr<IVirtualBox>       g_pVirtualBox = NULL;
ComPtr<ISession>          g_pSession    = NULL;


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** The service control handler handle. */
static SERVICE_STATUS_HANDLE g_hSupSvcWinCtrlHandler = NULL;
/** The service status. */
static uint32_t volatile g_u32SupSvcWinStatus = SERVICE_STOPPED;
/** The semaphore the main service thread is waiting on in autostartSvcWinServiceMain. */
static RTSEMEVENTMULTI g_hSupSvcWinEvent = NIL_RTSEMEVENTMULTI;
/** The service name is used for send to service main. */
static com::Bstr g_bstrServiceName;


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static SC_HANDLE autostartSvcWinOpenSCManager(const char *pszAction, DWORD dwAccess);

static int autostartGetProcessDomainUser(com::Utf8Str &aUser)
{
    int rc = VERR_NOT_SUPPORTED;

    RTUTF16 wszUsername[1024] = { 0 };
    ULONG   cwcUsername = RT_ELEMENTS(wszUsername);
    char *pszUser = NULL;
    if (!GetUserNameExW(NameSamCompatible, &wszUsername[0], &cwcUsername))
        return RTErrConvertFromWin32(GetLastError());
    rc = RTUtf16ToUtf8(wszUsername, &pszUser);
    aUser = pszUser;
    RTStrFree(pszUser);
    return rc;
}

static int autostartGetLocalDomain(com::Utf8Str &aDomain)
{
    RTUTF16 pwszDomain[MAX_COMPUTERNAME_LENGTH + 1] = { 0 };
    uint32_t cwcDomainSize =  MAX_COMPUTERNAME_LENGTH + 1;
    if (!GetComputerNameW(pwszDomain, (LPDWORD)&cwcDomainSize))
        return RTErrConvertFromWin32(GetLastError());
    char *pszDomain = NULL;
    int rc = RTUtf16ToUtf8(pwszDomain, &pszDomain);
    aDomain = pszDomain;
    RTStrFree(pszDomain);
    return rc;
}

static int autostartGetDomainAndUser(const com::Utf8Str &aDomainAndUser, com::Utf8Str &aDomain, com::Utf8Str &aUser)
{
    size_t offDelim = aDomainAndUser.find("\\");
    if (offDelim != aDomainAndUser.npos)
    {
        // if only domain is specified
        if (aDomainAndUser.length() - offDelim == 1)
            return VERR_INVALID_PARAMETER;

        if (offDelim == 1 && aDomainAndUser[0] == '.')
        {
            int rc = autostartGetLocalDomain(aDomain);
            aUser = aDomainAndUser.substr(offDelim + 1);
            return rc;
        }
        aDomain = aDomainAndUser.substr(0, offDelim);
        aUser   = aDomainAndUser.substr(offDelim + 1);
        return VINF_SUCCESS;
    }

    offDelim = aDomainAndUser.find("@");
    if (offDelim != aDomainAndUser.npos)
    {
        // if only domain is specified
        if (offDelim == 0)
            return VERR_INVALID_PARAMETER;

        // with '@' but without domain
        if (aDomainAndUser.length() - offDelim == 1)
        {
            int rc = autostartGetLocalDomain(aDomain);
            aUser = aDomainAndUser.substr(0, offDelim);
            return rc;
        }
        aDomain = aDomainAndUser.substr(offDelim + 1);
        aUser   = aDomainAndUser.substr(0, offDelim);
        return VINF_SUCCESS;
    }

    // only user is specified
    int rc = autostartGetLocalDomain(aDomain);
    aUser = aDomainAndUser;
    return rc;
}

/** Common helper for formatting the service name. */
static void autostartFormatServiceName(const com::Utf8Str &aDomain, const com::Utf8Str &aUser, com::Utf8Str &aServiceName)
{
    aServiceName.printf("%s%s%s", AUTOSTART_SERVICE_NAME, aDomain.c_str(), aUser.c_str());
}

/** Used by the delete service operation. */
static int autostartGetServiceName(const com::Utf8Str &aDomainAndUser, com::Utf8Str &aServiceName)
{
    com::Utf8Str sDomain;
    com::Utf8Str sUser;
    int rc = autostartGetDomainAndUser(aDomainAndUser, sDomain, sUser);
    if (RT_FAILURE(rc))
        return rc;
    autostartFormatServiceName(sDomain, sUser, aServiceName);
    return VINF_SUCCESS;
}

/**
 * Print out progress on the console.
 *
 * This runs the main event queue every now and then to prevent piling up
 * unhandled things (which doesn't cause real problems, just makes things
 * react a little slower than in the ideal case).
 */
DECLHIDDEN(HRESULT) showProgress(ComPtr<IProgress> progress)
{
    using namespace com;

    BOOL fCompleted = FALSE;
    ULONG uCurrentPercent = 0;
    Bstr bstrOperationDescription;

    NativeEventQueue::getMainEventQueue()->processEventQueue(0);

    ULONG cOperations = 1;
    HRESULT hrc = progress->COMGETTER(OperationCount)(&cOperations);
    if (FAILED(hrc))
        return hrc;

    /* setup signal handling if cancelable */
    bool fCanceledAlready = false;
    BOOL fCancelable;
    hrc = progress->COMGETTER(Cancelable)(&fCancelable);
    if (FAILED(hrc))
        fCancelable = FALSE;

    hrc = progress->COMGETTER(Completed(&fCompleted));
    while (SUCCEEDED(hrc))
    {
        progress->COMGETTER(Percent(&uCurrentPercent));

        if (fCompleted)
            break;

        /* process async cancelation */
        if (!fCanceledAlready)
        {
            hrc = progress->Cancel();
            if (SUCCEEDED(hrc))
                fCanceledAlready = true;
        }

        /* make sure the loop is not too tight */
        progress->WaitForCompletion(100);

        NativeEventQueue::getMainEventQueue()->processEventQueue(0);
        hrc = progress->COMGETTER(Completed(&fCompleted));
    }

    /* complete the line. */
    LONG iRc = E_FAIL;
    hrc = progress->COMGETTER(ResultCode)(&iRc);
    if (SUCCEEDED(hrc))
    {
        hrc = iRc;
    }

    return hrc;
}

DECLHIDDEN(void) autostartSvcOsLogStr(const char *pszMsg, AUTOSTARTLOGTYPE enmLogType)
{
    HANDLE hEventLog = RegisterEventSourceA(NULL /* local computer */, "VBoxAutostartSvc");
    AssertReturnVoid(hEventLog != NULL);
    WORD wType = 0;
    const char *apsz[2];
    apsz[0] = "VBoxAutostartSvc";
    apsz[1] = pszMsg;

    switch (enmLogType)
    {
        case AUTOSTARTLOGTYPE_INFO:
            wType = 0;
            break;
        case AUTOSTARTLOGTYPE_ERROR:
            wType = EVENTLOG_ERROR_TYPE;
            break;
        case AUTOSTARTLOGTYPE_WARNING:
            wType = EVENTLOG_WARNING_TYPE;
            break;
        case AUTOSTARTLOGTYPE_VERBOSE:
            if (!g_fVerbose)
                return;
            wType = EVENTLOG_INFORMATION_TYPE;
            break;
        default:
            AssertMsgFailed(("Invalid log type %d\n", enmLogType));
    }

    BOOL fRc = ReportEventA(hEventLog,               /* hEventLog */
                            wType,                   /* wType */
                            0,                       /* wCategory */
                            0 /** @todo mc */,       /* dwEventID */
                            NULL,                    /* lpUserSid */
                            RT_ELEMENTS(apsz),       /* wNumStrings */
                            0,                       /* dwDataSize */
                            apsz,                    /* lpStrings */
                            NULL);                   /* lpRawData */
    AssertMsg(fRc, ("%u\n", GetLastError())); NOREF(fRc);
    DeregisterEventSource(hEventLog);
}

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
static SC_HANDLE autostartSvcWinOpenService(const PRTUTF16 pwszServiceName, const char *pszAction, DWORD dwSCMAccess, DWORD dwSVCAccess,
                                            unsigned cIgnoredErrors, ...)
{
    SC_HANDLE hSCM = autostartSvcWinOpenSCManager(pszAction, dwSCMAccess);
    if (!hSCM)
        return NULL;

    SC_HANDLE hSvc = OpenServiceW(hSCM, pwszServiceName, dwSVCAccess);
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
            fIgnored = (DWORD)va_arg(va, int) == err;
        va_end(va);
        if (!fIgnored)
        {
            switch (err)
            {
                case ERROR_ACCESS_DENIED:
                    autostartSvcDisplayError("%s - OpenService failure: access denied\n", pszAction);
                    break;
                case ERROR_SERVICE_DOES_NOT_EXIST:
                    autostartSvcDisplayError("%s - OpenService failure: The service %ls does not exist. Reinstall it.\n",
                                             pszAction, pwszServiceName);
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

static RTEXITCODE autostartSvcWinInterrogate(int argc, char **argv)
{
    RT_NOREF(argc, argv);
    RTPrintf("VBoxAutostartSvc: The \"interrogate\" action is not implemented.\n");
    return RTEXITCODE_FAILURE;
}


static RTEXITCODE autostartSvcWinStop(int argc, char **argv)
{
    RT_NOREF(argc, argv);
    RTPrintf("VBoxAutostartSvc: The \"stop\" action is not implemented.\n");
    return RTEXITCODE_FAILURE;
}


static RTEXITCODE autostartSvcWinContinue(int argc, char **argv)
{
    RT_NOREF(argc, argv);
    RTPrintf("VBoxAutostartSvc: The \"continue\" action is not implemented.\n");
    return RTEXITCODE_FAILURE;
}


static RTEXITCODE autostartSvcWinPause(int argc, char **argv)
{
    RT_NOREF(argc, argv);
    RTPrintf("VBoxAutostartSvc: The \"pause\" action is not implemented.\n");
    return RTEXITCODE_FAILURE;
}


static RTEXITCODE autostartSvcWinStart(int argc, char **argv)
{
    RT_NOREF(argc, argv);
    RTPrintf("VBoxAutostartSvc: The \"start\" action is not implemented.\n");
    return RTEXITCODE_SUCCESS;
}


static RTEXITCODE autostartSvcWinQueryDescription(int argc, char **argv)
{
    RT_NOREF(argc, argv);
    RTPrintf("VBoxAutostartSvc: The \"qdescription\" action is not implemented.\n");
    return RTEXITCODE_FAILURE;
}


static RTEXITCODE autostartSvcWinQueryConfig(int argc, char **argv)
{
    RT_NOREF(argc, argv);
    RTPrintf("VBoxAutostartSvc: The \"qconfig\" action is not implemented.\n");
    return RTEXITCODE_FAILURE;
}


static RTEXITCODE autostartSvcWinDisable(int argc, char **argv)
{
    RT_NOREF(argc, argv);
    RTPrintf("VBoxAutostartSvc: The \"disable\" action is not implemented.\n");
    return RTEXITCODE_FAILURE;
}

static RTEXITCODE autostartSvcWinEnable(int argc, char **argv)
{
    RT_NOREF(argc, argv);
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
    const char *pszUser = NULL;
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--verbose", 'v', RTGETOPT_REQ_NOTHING },
        { "--user",    'u', RTGETOPT_REQ_STRING  },
    };
    int ch;
    RTGETOPTUNION Value;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 0, RTGETOPTINIT_FLAGS_NO_STD_OPTS);
    while ((ch = RTGetOpt(&GetState, &Value)))
    {
        switch (ch)
        {
            case 'v':
                fVerbose = true;
                break;
            case 'u':
                pszUser = Value.psz;
                break;
            default:
                return autostartSvcDisplayGetOptError("delete", ch, &Value);
        }
    }

    if (!pszUser)
        return autostartSvcDisplayError("delete - DeleteService failed, user name required.\n");

    com::Utf8Str sServiceName;
    int vrc = autostartGetServiceName(pszUser, sServiceName);
    if (RT_FAILURE(vrc))
        return autostartSvcDisplayError("delete - DeleteService failed, service name for user %s can not be constructed.\n",
                                        pszUser);
    /*
     * Create the service.
     */
    RTEXITCODE rc = RTEXITCODE_FAILURE;
    SC_HANDLE hSvc = autostartSvcWinOpenService(com::Bstr(sServiceName).raw(), "delete", SERVICE_CHANGE_CONFIG, DELETE,
                                                1, ERROR_SERVICE_DOES_NOT_EXIST);
    if (hSvc)
    {
        if (DeleteService(hSvc))
        {
            RTPrintf("Successfully deleted the %s service.\n", sServiceName.c_str());
            rc = RTEXITCODE_SUCCESS;
        }
        else
            autostartSvcDisplayError("delete - DeleteService failed, err=%d.\n", GetLastError());
        CloseServiceHandle(hSvc);
    }
    else if (GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST)
    {

        if (fVerbose)
            RTPrintf("The service %s was not installed, nothing to be done.", sServiceName.c_str());
        else
            RTPrintf("Successfully deleted the %s service.\n", sServiceName.c_str());
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
    com::Utf8Str strPwd;
    const char *pszPwdFile = NULL;
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--verbose",       'v', RTGETOPT_REQ_NOTHING },
        { "--user",          'u', RTGETOPT_REQ_STRING  },
        { "--password-file", 'p', RTGETOPT_REQ_STRING  }
    };
    int ch;
    RTGETOPTUNION Value;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 0, RTGETOPTINIT_FLAGS_NO_STD_OPTS);
    while ((ch = RTGetOpt(&GetState, &Value)))
    {
        switch (ch)
        {
            case 'v':
                fVerbose = true;
                break;
            case 'u':
                pszUser = Value.psz;
                break;
            case 'p':
                pszPwdFile = Value.psz;
                break;
            default:
                return autostartSvcDisplayGetOptError("create", ch, &Value);
        }
    }

    if (!pszUser)
        return autostartSvcDisplayError("Username is missing");

    if (pszPwdFile)
    {
        /* Get password from file. */
        RTEXITCODE rcExit = readPasswordFile(pszPwdFile, &strPwd);
        if (rcExit == RTEXITCODE_FAILURE)
            return rcExit;
    }
    else
    {
        /* Get password from console. */
        RTEXITCODE rcExit = readPasswordFromConsole(&strPwd, "Enter password:");
        if (rcExit == RTEXITCODE_FAILURE)
            return rcExit;
    }

    if (strPwd.isEmpty())
        return autostartSvcDisplayError("Password is missing");

    com::Utf8Str sDomain;
    com::Utf8Str sUserTmp;
    int vrc = autostartGetDomainAndUser(pszUser, sDomain, sUserTmp);
    if (RT_FAILURE(vrc))
        return autostartSvcDisplayError("create - CreateService failed, failed to get domain and user from string %s (%d).\n",
                                        pszUser, vrc);
    com::Utf8StrFmt sUserFullName("%s\\%s", sDomain.c_str(), sUserTmp.c_str());
    com::Utf8StrFmt sDisplayName("%s %s@%s", AUTOSTART_SERVICE_DISPLAY_NAME, sUserTmp.c_str(), sDomain.c_str());
    com::Utf8Str    sServiceName;
    autostartFormatServiceName(sDomain, sUserTmp, sServiceName);

    /*
     * Create the service.
     */
    RTEXITCODE rc = RTEXITCODE_FAILURE;
    SC_HANDLE hSCM = autostartSvcWinOpenSCManager("create", SC_MANAGER_CREATE_SERVICE); /*SC_MANAGER_ALL_ACCESS*/
    if (hSCM)
    {
        char szExecPath[RTPATH_MAX];
        if (RTProcGetExecutablePath(szExecPath, sizeof(szExecPath)))
        {
            if (fVerbose)
                RTPrintf("Creating the %s service, binary \"%s\"...\n",
                         sServiceName.c_str(), szExecPath); /* yea, the binary name isn't UTF-8, but wtf. */

            /*
             * Add service name as command line parameter for the service
             */
            com::Utf8StrFmt sCmdLine("\"%s\" --service=%s", szExecPath, sServiceName.c_str());
            com::Bstr bstrServiceName(sServiceName);
            com::Bstr bstrDisplayName(sDisplayName);
            com::Bstr bstrCmdLine(sCmdLine);
            com::Bstr bstrUserFullName(sUserFullName);
            com::Bstr bstrPwd(strPwd);

            SC_HANDLE hSvc = CreateServiceW(hSCM,                            /* hSCManager */
                                            bstrServiceName.raw(),           /* lpServiceName */
                                            bstrDisplayName.raw(),           /* lpDisplayName */
                                            SERVICE_CHANGE_CONFIG | SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG, /* dwDesiredAccess */
                                            SERVICE_WIN32_OWN_PROCESS,       /* dwServiceType ( | SERVICE_INTERACTIVE_PROCESS? ) */
                                            SERVICE_AUTO_START,              /* dwStartType */
                                            SERVICE_ERROR_NORMAL,            /* dwErrorControl */
                                            bstrCmdLine.raw(),               /* lpBinaryPathName */
                                            NULL,                            /* lpLoadOrderGroup */
                                            NULL,                            /* lpdwTagId */
                                            NULL,                            /* lpDependencies */
                                            bstrUserFullName.raw(),          /* lpServiceStartName (NULL => LocalSystem) */
                                            bstrPwd.raw());                  /* lpPassword */
            if (hSvc)
            {
                RTPrintf("Successfully created the %s service.\n", sServiceName.c_str());
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
            /*
             * Notify the main thread that we're done, it will wait for the
             * VMs to stop, and set the windows service status to SERVICE_STOPPED
             * and return.
             */
            int rc = RTSemEventMultiSignal(g_hSupSvcWinEvent);
            if (RT_FAILURE(rc))
                autostartSvcLogError("SERVICE_CONTROL_STOP: RTSemEventMultiSignal failed, %Rrc\n", rc);

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
    /* not reached */
}

static int autostartStartVMs()
{
    int rc = autostartSetup();

    const char *pszConfigFile = RTEnvGet("VBOXAUTOSTART_CONFIG");
    if (!pszConfigFile)
        return autostartSvcLogError("Starting VMs failed. VBOXAUTOSTART_CONFIG environment variable is not defined.\n");
    bool fAllow = false;

    PCFGAST pCfgAst = NULL;
    rc = autostartParseConfig(pszConfigFile, &pCfgAst);
    if (RT_FAILURE(rc))
        return autostartSvcLogError("Starting VMs failed. Failed to parse the config file. Check the access permissions and file structure.\n");

    PCFGAST pCfgAstPolicy = autostartConfigAstGetByName(pCfgAst, "default_policy");
    /* Check default policy. */
    if (pCfgAstPolicy)
    {
        if (   pCfgAstPolicy->enmType == CFGASTNODETYPE_KEYVALUE
            && (   !RTStrCmp(pCfgAstPolicy->u.KeyValue.aszValue, "allow")
                || !RTStrCmp(pCfgAstPolicy->u.KeyValue.aszValue, "deny")))
        {
            if (!RTStrCmp(pCfgAstPolicy->u.KeyValue.aszValue, "allow"))
                fAllow = true;
        }
        else
        {
            autostartConfigAstDestroy(pCfgAst);
            return autostartSvcLogError("'default_policy' must be either 'allow' or 'deny'.\n");
        }
    }

    com::Utf8Str sUser;
    rc = autostartGetProcessDomainUser(sUser);
    if (RT_FAILURE(rc))
    {
        autostartConfigAstDestroy(pCfgAst);
        return autostartSvcLogError("Failed to query username of the process (%Rrc).\n", rc);
    }

    PCFGAST pCfgAstUser = NULL;
    for (unsigned i = 0; i < pCfgAst->u.Compound.cAstNodes; i++)
    {
        PCFGAST pNode = pCfgAst->u.Compound.apAstNodes[i];
        com::Utf8Str sDomain;
        com::Utf8Str sUserTmp;
        int rc = autostartGetDomainAndUser(pNode->pszKey, sDomain, sUserTmp);
        if (RT_FAILURE(rc))
            continue;
        com::Utf8StrFmt sDomainUser("%s\\%s", sDomain.c_str(), sUserTmp.c_str());
        if (sDomainUser == sUser)
        {
            pCfgAstUser = pNode;
            break;
        }
    }

    if (   pCfgAstUser
        && pCfgAstUser->enmType == CFGASTNODETYPE_COMPOUND)
    {
        pCfgAstPolicy = autostartConfigAstGetByName(pCfgAstUser, "allow");
        if (pCfgAstPolicy)
        {
            if (   pCfgAstPolicy->enmType == CFGASTNODETYPE_KEYVALUE
                && (   !RTStrCmp(pCfgAstPolicy->u.KeyValue.aszValue, "true")
                    || !RTStrCmp(pCfgAstPolicy->u.KeyValue.aszValue, "false")))
                fAllow = RTStrCmp(pCfgAstPolicy->u.KeyValue.aszValue, "true") == 0;
            else
            {
                autostartConfigAstDestroy(pCfgAst);
                return autostartSvcLogError("'allow' must be either 'true' or 'false'.\n");
            }
        }
    }
    else if (pCfgAstUser)
    {
        autostartConfigAstDestroy(pCfgAst);
        return autostartSvcLogError("Invalid config, user is not a compound node.\n");
    }

    if (!fAllow)
    {
        autostartConfigAstDestroy(pCfgAst);
        return autostartSvcLogError("User is not allowed to autostart VMs.\n");
    }

    rc = autostartStartMain(pCfgAstUser);
    autostartConfigAstDestroy(pCfgAst);
    if (RT_FAILURE(rc))
        autostartSvcLogError("Starting VMs failed, rc=%Rrc\n", rc);

    return rc;
}

/**
 * Windows Service Main.
 *
 * This is invoked when the service is started and should not return until
 * the service has been stopped.
 *
 * @param   cArgs           Argument count.
 * @param   papwszArgs      Argument vector.
 */
static VOID WINAPI autostartSvcWinServiceMain(DWORD cArgs, LPWSTR *papwszArgs)
{
    RT_NOREF(papwszArgs);
    LogFlowFuncEnter();

    /*
     * Register the control handler function for the service and report to SCM.
     */
    Assert(g_u32SupSvcWinStatus == SERVICE_STOPPED);
    g_hSupSvcWinCtrlHandler = RegisterServiceCtrlHandlerExW(g_bstrServiceName.raw(), autostartSvcWinServiceCtrlHandlerEx, NULL);
    if (g_hSupSvcWinCtrlHandler)
    {
        DWORD err = ERROR_GEN_FAILURE;
        if (autostartSvcWinSetServiceStatus(SERVICE_START_PENDING, 3000, NO_ERROR))
        {
            if (cArgs == 1)
            {
                /*
                 * Create the event semaphore we'll be waiting on and
                 * then instantiate the actual services.
                 */
                int rc = RTSemEventMultiCreate(&g_hSupSvcWinEvent);
                if (RT_SUCCESS(rc))
                {
                    /*
                     * Update the status and enter the work loop.
                     */
                    if (autostartSvcWinSetServiceStatus(SERVICE_RUNNING, 0, 0))
                    {
                        LogFlow(("autostartSvcWinServiceMain: calling autostartStartVMs\n"));
                        rc = autostartStartVMs();
                        if (RT_SUCCESS(rc))
                        {
                            LogFlow(("autostartSvcWinServiceMain: done string VMs\n"));
                            err = NO_ERROR;
                            rc = RTSemEventMultiWait(g_hSupSvcWinEvent, RT_INDEFINITE_WAIT);
                            if (RT_SUCCESS(rc))
                            {
                                LogFlow(("autostartSvcWinServiceMain: woke up\n"));
                                /** @todo Autostop part. */
                                err = NO_ERROR;
                            }
                            else
                                autostartSvcLogError("RTSemEventWait failed, rc=%Rrc", rc);
                        }

                        autostartShutdown();
                    }
                    else
                    {
                        err = GetLastError();
                        autostartSvcLogError("SetServiceStatus failed, err=%u", err);
                    }

                    RTSemEventMultiDestroy(g_hSupSvcWinEvent);
                    g_hSupSvcWinEvent = NIL_RTSEMEVENTMULTI;
                }
                else
                    autostartSvcLogError("RTSemEventMultiCreate failed, rc=%Rrc", rc);
            }
            else
                autostartSvcLogTooManyArgsError("main", cArgs, NULL, 0);
        }
        else
        {
            err = GetLastError();
            autostartSvcLogError("SetServiceStatus failed, err=%u", err);
        }
        autostartSvcWinSetServiceStatus(SERVICE_STOPPED, 0, err);
    }
    else
        autostartSvcLogError("RegisterServiceCtrlHandlerEx failed, err=%u", GetLastError());

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
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--service", 's', RTGETOPT_REQ_STRING },
    };

    const char *pszServiceName = NULL;
    int ch;
    RTGETOPTUNION Value;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 0, RTGETOPTINIT_FLAGS_NO_STD_OPTS);
    while ((ch = RTGetOpt(&GetState, &Value)))
    {
        switch (ch)
        {
            case 's':
                pszServiceName = Value.psz;
                try
                {
                    g_bstrServiceName = com::Bstr(Value.psz);
                }
                catch (...)
                {
                    autostartSvcLogError("runit failed, service name is not valid utf-8 string or out of memory");
                    return RTEXITCODE_FAILURE;
                }
                break;
            default:
                return autostartSvcDisplayGetOptError("runit", ch, &Value);
        }
    }

    if (!pszServiceName)
    {
        autostartSvcLogError("runit failed, service name is missing");
        return RTEXITCODE_FAILURE;
    }

    /*
     * Register the service with the service control manager
     * and start dispatching requests from it (all done by the API).
     */
    SERVICE_TABLE_ENTRYW const s_aServiceStartTable[] =
    {
        { g_bstrServiceName.raw(), autostartSvcWinServiceMain },
        { NULL, NULL}
    };
    if (StartServiceCtrlDispatcherW(&s_aServiceStartTable[0]))
    {
        LogFlowFuncLeave();
        return RTEXITCODE_SUCCESS; /* told to quit, so quit. */
    }

    DWORD err = GetLastError();
    switch (err)
    {
        case ERROR_FAILED_SERVICE_CONTROLLER_CONNECT:
            autostartSvcWinServiceMain(0, NULL);//autostartSvcDisplayError("Cannot run a service from the command line. Use the 'start' action to start it the right way.\n");
            break;
        default:
            autostartSvcLogError("StartServiceCtrlDispatcher failed, err=%u", err);
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
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--brief", 'b', RTGETOPT_REQ_NOTHING }
    };
    int ch;
    RTGETOPTUNION Value;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 0, RTGETOPTINIT_FLAGS_NO_STD_OPTS);
    while ((ch = RTGetOpt(&GetState, &Value)))
        switch (ch)
        {
            case 'b':   fBrief = true;  break;
            default:    return autostartSvcDisplayGetOptError("version", ch, &Value);
        }

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
    return RTEXITCODE_SUCCESS;
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
             "VBoxAutostartSvc <uninstall|delete|/UnregServer|/u>\n"
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
int main(int argc, char **argv)
{
    /*
     * Initialize the IPRT first of all.
     */
    int rc = RTR3InitExe(argc, &argv, 0);
    if (RT_FAILURE(rc))
    {
        autostartSvcLogError("RTR3InitExe failed with rc=%Rrc", rc);
        return RTEXITCODE_FAILURE;
    }

    RTThreadSleep(10 * 1000);

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

