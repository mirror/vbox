/** @file
 *
 * SVCMAIN - COM out-of-proc server main entry
 */

/*
 * Copyright (C) 2004-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <Windows.h>
#include <stdio.h>
#include <stdlib.h>

#include "VBox/com/defs.h"

#include "VBox/com/com.h"

#include "VBox/com/VirtualBox.h"

#include "VirtualBoxImpl.h"
#include "Logging.h"

#include "svchlp.h"

#include <VBox/err.h>
#include <iprt/buildconfig.h>
#include <iprt/initterm.h>
#include <iprt/string.h>
#include <iprt/uni.h>
#include <iprt/path.h>
#include <iprt/getopt.h>

#include <atlbase.h>
#include <atlcom.h>

#define _ATL_FREE_THREADED

class CExeModule : public CComModule
{
public:
    LONG Unlock();
    DWORD dwThreadID;
    HANDLE hEventShutdown;
    void MonitorShutdown();
    bool StartMonitor();
    bool bActivity;
};

const DWORD dwTimeOut = 5000; /* time for EXE to be idle before shutting down */
const DWORD dwPause = 1000; /* time to wait for threads to finish up */

/* Passed to CreateThread to monitor the shutdown event */
static DWORD WINAPI MonitorProc(void* pv)
{
    CExeModule* p = (CExeModule*)pv;
    p->MonitorShutdown();
    return 0;
}

LONG CExeModule::Unlock()
{
    LONG l = CComModule::Unlock();
    if (l == 0)
    {
        bActivity = true;
        SetEvent(hEventShutdown); /* tell monitor that we transitioned to zero */
    }
    return l;
}

/* Monitors the shutdown event */
void CExeModule::MonitorShutdown()
{
    while (1)
    {
        WaitForSingleObject(hEventShutdown, INFINITE);
        DWORD dwWait=0;
        do
        {
            bActivity = false;
            dwWait = WaitForSingleObject(hEventShutdown, dwTimeOut);
        } while (dwWait == WAIT_OBJECT_0);
        /* timed out */
        if (!bActivity && m_nLockCnt == 0) /* if no activity let's really bail */
        {
#if _WIN32_WINNT >= 0x0400 & defined(_ATL_FREE_THREADED)
            CoSuspendClassObjects();
            if (!bActivity && m_nLockCnt == 0)
#endif
                break;
        }
    }
    CloseHandle(hEventShutdown);
    PostThreadMessage(dwThreadID, WM_QUIT, 0, 0);
}

bool CExeModule::StartMonitor()
{
    hEventShutdown = CreateEvent(NULL, false, false, NULL);
    if (hEventShutdown == NULL)
        return false;
    DWORD dwThreadID;
    HANDLE h = CreateThread(NULL, 0, MonitorProc, this, 0, &dwThreadID);
    return (h != NULL);
}

CExeModule _Module;

BEGIN_OBJECT_MAP(ObjectMap)
    OBJECT_ENTRY(CLSID_VirtualBox, VirtualBox)
END_OBJECT_MAP()


LPCTSTR FindOneOf(LPCTSTR p1, LPCTSTR p2)
{
    while (p1 != NULL && *p1 != NULL)
    {
        LPCTSTR p = p2;
        while (p != NULL && *p != NULL)
        {
            if (*p1 == *p)
                return CharNext(p1);
            p = CharNext(p);
        }
        p1 = CharNext(p1);
    }
    return NULL;
}

static int WordCmpI(LPCTSTR psz1, LPCTSTR psz2) throw()
{
    TCHAR c1 = (TCHAR)CharUpper((LPTSTR)*psz1);
    TCHAR c2 = (TCHAR)CharUpper((LPTSTR)*psz2);
    while (c1 != NULL && c1 == c2 && c1 != ' ' && c1 != '\t')
    {
        psz1 = CharNext(psz1);
        psz2 = CharNext(psz2);
        c1 = (TCHAR)CharUpper((LPTSTR)*psz1);
        c2 = (TCHAR)CharUpper((LPTSTR)*psz2);
    }
    if ((c1 == NULL || c1 == ' ' || c1 == '\t') && (c2 == NULL || c2 == ' ' || c2 == '\t'))
        return 0;

    return (c1 < c2) ? -1 : 1;
}

/////////////////////////////////////////////////////////////////////////////
//
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPSTR /*lpCmdLine*/, int /*nShowCmd*/)
{
    LPCTSTR lpCmdLine = GetCommandLine(); /* this line necessary for _ATL_MIN_CRT */

    /*
     * Need to parse the command line before initializing the VBox runtime.
     */
    TCHAR szTokens[] = _T("-/");
    LPCTSTR lpszToken = FindOneOf(lpCmdLine, szTokens);
    while (lpszToken != NULL)
    {
        if (WordCmpI(lpszToken, _T("Embedding")) == 0)
        {
            /* %HOMEDRIVE%%HOMEPATH% */
            wchar_t wszHome[RTPATH_MAX];
            DWORD cEnv = GetEnvironmentVariable(L"HOMEDRIVE", &wszHome[0], RTPATH_MAX);
            if (cEnv && cEnv < RTPATH_MAX)
            {
                DWORD cwc = cEnv; /* doesn't include NUL */
                cEnv = GetEnvironmentVariable(L"HOMEPATH", &wszHome[cEnv], RTPATH_MAX - cwc);
                if (cEnv && cEnv < RTPATH_MAX - cwc)
                {
                    /* If this fails there is nothing we can do. Ignore. */
                    SetCurrentDirectory(wszHome);
                }
            }
        }

        lpszToken = FindOneOf(lpszToken, szTokens);
    }

    /*
     * Initialize the VBox runtime without loading
     * the support driver.
     */
    int    argc = __argc;
    char **argv = __argv;
    RTR3InitExe(argc, &argv, 0);

    /* Note that all options are given lowercase/camel case/uppercase to
     * approximate case insensitive matching, which RTGetOpt doesn't offer. */
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--embedding",    'e',    RTGETOPT_REQ_NOTHING | RTGETOPT_FLAG_ICASE },
        { "-embedding",     'e',    RTGETOPT_REQ_NOTHING | RTGETOPT_FLAG_ICASE },
        { "/embedding",     'e',    RTGETOPT_REQ_NOTHING | RTGETOPT_FLAG_ICASE },
        { "--unregserver",  'u',    RTGETOPT_REQ_NOTHING | RTGETOPT_FLAG_ICASE },
        { "-unregserver",   'u',    RTGETOPT_REQ_NOTHING | RTGETOPT_FLAG_ICASE },
        { "/unregserver",   'u',    RTGETOPT_REQ_NOTHING | RTGETOPT_FLAG_ICASE },
        { "--regserver",    'r',    RTGETOPT_REQ_NOTHING | RTGETOPT_FLAG_ICASE },
        { "-regserver",     'r',    RTGETOPT_REQ_NOTHING | RTGETOPT_FLAG_ICASE },
        { "/regserver",     'r',    RTGETOPT_REQ_NOTHING | RTGETOPT_FLAG_ICASE },
        { "--reregserver",  'f',    RTGETOPT_REQ_NOTHING | RTGETOPT_FLAG_ICASE },
        { "-reregserver",   'f',    RTGETOPT_REQ_NOTHING | RTGETOPT_FLAG_ICASE },
        { "/reregserver",   'f',    RTGETOPT_REQ_NOTHING | RTGETOPT_FLAG_ICASE },
        { "--helper",       'H',    RTGETOPT_REQ_STRING | RTGETOPT_FLAG_ICASE },
        { "-helper",        'H',    RTGETOPT_REQ_STRING | RTGETOPT_FLAG_ICASE },
        { "/helper",        'H',    RTGETOPT_REQ_STRING | RTGETOPT_FLAG_ICASE },
        { "--logfile",      'F',    RTGETOPT_REQ_STRING | RTGETOPT_FLAG_ICASE },
        { "-logfile",       'F',    RTGETOPT_REQ_STRING | RTGETOPT_FLAG_ICASE },
        { "/logfile",       'F',    RTGETOPT_REQ_STRING | RTGETOPT_FLAG_ICASE },
        { "--logrotate",    'R',    RTGETOPT_REQ_UINT32 | RTGETOPT_FLAG_ICASE },
        { "-logrotate",     'R',    RTGETOPT_REQ_UINT32 | RTGETOPT_FLAG_ICASE },
        { "/logrotate",     'R',    RTGETOPT_REQ_UINT32 | RTGETOPT_FLAG_ICASE },
        { "--logsize",      'S',    RTGETOPT_REQ_UINT64 | RTGETOPT_FLAG_ICASE },
        { "-logsize",       'S',    RTGETOPT_REQ_UINT64 | RTGETOPT_FLAG_ICASE },
        { "/logsize",       'S',    RTGETOPT_REQ_UINT64 | RTGETOPT_FLAG_ICASE },
        { "--loginterval",  'I',    RTGETOPT_REQ_UINT32 | RTGETOPT_FLAG_ICASE },
        { "-loginterval",   'I',    RTGETOPT_REQ_UINT32 | RTGETOPT_FLAG_ICASE },
        { "/loginterval",   'I',    RTGETOPT_REQ_UINT32 | RTGETOPT_FLAG_ICASE },
    };

    bool            fRun = true;
    bool            fRegister = false;
    bool            fUnregister = false;
    const char      *pszPipeName = NULL;
    const char      *pszLogFile = NULL;
    uint32_t        cHistory = 10;                  // enable log rotation, 10 files
    uint32_t        uHistoryFileTime = RT_SEC_1DAY; // max 1 day per file
    uint64_t        uHistoryFileSize = 100 * _1M;   // max 100MB per file

    RTGETOPTSTATE   GetOptState;
    int vrc = RTGetOptInit(&GetOptState, argc, argv, &s_aOptions[0], RT_ELEMENTS(s_aOptions), 1, 0 /*fFlags*/);
    AssertRC(vrc);

    RTGETOPTUNION   ValueUnion;
    while ((vrc = RTGetOpt(&GetOptState, &ValueUnion)))
    {
        switch (vrc)
        {
            case 'e':
                /* already handled above */
                break;

            case 'u':
                fUnregister = true;
                fRun = false;
                break;

            case 'r':
                fRegister = true;
                fRun = false;
                break;

            case 'f':
                fUnregister = true;
                fRegister = true;
                fRun = false;
                break;

            case 'H':
                pszPipeName = ValueUnion.psz;
                if (!pszPipeName)
                    pszPipeName = "";
                fRun = false;
                break;

            case 'F':
                pszLogFile = ValueUnion.psz;
                break;

            case 'R':
                cHistory = ValueUnion.u32;
                break;

            case 'S':
                uHistoryFileSize = ValueUnion.u64;
                break;

            case 'I':
                uHistoryFileTime = ValueUnion.u32;
                break;

            case 'h':
            {
                TCHAR txt[]= L"Options:\n\n"
                             L"/RegServer:\tregister COM out-of-proc server\n"
                             L"/UnregServer:\tunregister COM out-of-proc server\n"
                             L"/ReregServer:\tunregister and register COM server\n"
                             L"no options:\trun the server";
                TCHAR title[]=_T("Usage");
                fRun = false;
                MessageBox(NULL, txt, title, MB_OK);
                return 0;
            }

            case 'V':
            {
                char *psz = NULL;
                RTStrAPrintf(&psz, "%sr%s\n", RTBldCfgVersion(), RTBldCfgRevisionStr());
                PRTUTF16 txt = NULL;
                RTStrToUtf16(psz, &txt);
                TCHAR title[]=_T("Version");
                fRun = false;
                MessageBox(NULL, txt, title, MB_OK);
                RTStrFree(psz);
                RTUtf16Free(txt);
                return 0;
            }

            default:
                /** @todo this assumes that stderr is visible, which is not
                 * true for standard Windows applications. */
                /* continue on command line errors... */
                RTGetOptPrintError(vrc, &ValueUnion);
        }
    }

    /* Only create the log file when running VBoxSVC normally, but not when
     * registering/unregistering or calling the helper functionality. */
    if (fRun)
    {
        if (!pszLogFile)
        {
            char szLogFile[RTPATH_MAX];
            vrc = com::GetVBoxUserHomeDirectory(szLogFile, sizeof(szLogFile));
            if (RT_SUCCESS(vrc))
                vrc = RTPathAppend(szLogFile, sizeof(szLogFile), "VBoxSVC.log");
            if (RT_SUCCESS(vrc))
                pszLogFile = RTStrDup(szLogFile);
        }
        vrc = com::VBoxLogRelCreate("COM Server", pszLogFile,
                                    RTLOGFLAGS_PREFIX_THREAD | RTLOGFLAGS_PREFIX_TIME_PROG,
                                    "all", "VBOXSVC_RELEASE_LOG",
                                    RTLOGDEST_FILE, UINT32_MAX /* cMaxEntriesPerGroup */,
                                    cHistory, uHistoryFileTime, uHistoryFileSize,
                                    szError, sizeof(szError));
        if (RT_FAILURE(vrc))
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "failed to open release log (%s, %Rrc)", szError, vrc);
    }

    int nRet = 0;
    HRESULT hRes = com::Initialize();

    _ASSERTE(SUCCEEDED(hRes));
    _Module.Init(ObjectMap, hInstance, &LIBID_VirtualBox);
    _Module.dwThreadID = GetCurrentThreadId();

    if (!fRun)
    {
        if (fUnregister)
        {
            _Module.UpdateRegistryFromResource(IDR_VIRTUALBOX, FALSE);
            nRet = _Module.UnregisterServer(TRUE);
        }
        if (fRegister)
        {
            _Module.UpdateRegistryFromResource(IDR_VIRTUALBOX, TRUE);
            nRet = _Module.RegisterServer(TRUE);
        }
        if (pszPipeName)
        {
            Log(("SVCMAIN: Processing Helper request (cmdline=\"%s\")...\n", pszPipeName));

            if (!*pszPipeName)
                vrc = VERR_INVALID_PARAMETER;

            if (RT_SUCCESS(vrc))
            {
                /* do the helper job */
                SVCHlpServer server;
                vrc = server.open(pszPipeName);
                if (RT_SUCCESS(vrc))
                    vrc = server.run();
            }
            if (RT_FAILURE(vrc))
            {
                Log(("SVCMAIN: Failed to process Helper request (%Rrc).", vrc));
                nRet = 1;
            }
        }
    }
    else
    {
        _Module.StartMonitor();
#if _WIN32_WINNT >= 0x0400 & defined(_ATL_FREE_THREADED)
        hRes = _Module.RegisterClassObjects(CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE | REGCLS_SUSPENDED);
        _ASSERTE(SUCCEEDED(hRes));
        hRes = CoResumeClassObjects();
#else
        hRes = _Module.RegisterClassObjects(CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE);
#endif
        _ASSERTE(SUCCEEDED(hRes));

        MSG msg;
        while (GetMessage(&msg, 0, 0, 0))
            DispatchMessage(&msg);

        _Module.RevokeClassObjects();
        Sleep(dwPause); //wait for any threads to finish
    }

    _Module.Term();

    com::Shutdown();

    Log(("SVCMAIN: Returning, COM server process ends.\n"));
    return nRet;
}
