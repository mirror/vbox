/** @file
 *
 * SVCMAIN - COM out-of-proc server main entry
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

#include <stdio.h>

#include "VBox/com/defs.h"

#include "svchlp.h"

#include <VBox/err.h>
#include <iprt/runtime.h>

#include "MachineImpl.h"
#include "HardDiskImpl.h"
#include "DVDImageImpl.h"
#include "FloppyImageImpl.h"
#include "GuestOSTypeImpl.h"
#include "ProgressImpl.h"
#include "SystemPropertiesImpl.h"
#include "VirtualBoxImpl.h"

#include "Logging.h"

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
extern "C" int WINAPI _tWinMain(HINSTANCE hInstance,
    HINSTANCE /*hPrevInstance*/, LPTSTR lpCmdLine, int /*nShowCmd*/)
{
    /*
     * Initialize the VBox runtime without loading
     * the support driver.
     */
    RTR3Init(false);

    lpCmdLine = GetCommandLine(); /* this line necessary for _ATL_MIN_CRT */

#if _WIN32_WINNT >= 0x0400 & defined(_ATL_FREE_THREADED)
    HRESULT hRes = CoInitializeEx(NULL, COINIT_MULTITHREADED);
#else
    HRESULT hRes = CoInitialize(NULL);
#endif
    _ASSERTE(SUCCEEDED(hRes));
    _Module.Init(ObjectMap, hInstance, &LIBID_VirtualBox);
    _Module.dwThreadID = GetCurrentThreadId();
    TCHAR szTokens[] = _T("-/");

    int nRet = 0;
    BOOL bRun = TRUE;
    LPCTSTR lpszToken = FindOneOf(lpCmdLine, szTokens);
    while (lpszToken != NULL)
    {
        if (WordCmpI(lpszToken, _T("UnregServer")) == 0)
        {
            _Module.UpdateRegistryFromResource(IDR_VIRTUALBOX, FALSE);
            nRet = _Module.UnregisterServer(TRUE);
            bRun = FALSE;
            break;
        }
        else if (WordCmpI(lpszToken, _T("RegServer")) == 0)
        {
            _Module.UpdateRegistryFromResource(IDR_VIRTUALBOX, TRUE);
            nRet = _Module.RegisterServer(TRUE);
            bRun = FALSE;
            break;
        }
        else if (WordCmpI(lpszToken, _T("ReregServer")) == 0)
        {
            _Module.UpdateRegistryFromResource(IDR_VIRTUALBOX, FALSE);
            nRet = _Module.UnregisterServer(TRUE);
            _Module.UpdateRegistryFromResource(IDR_VIRTUALBOX, TRUE);
            nRet = _Module.RegisterServer(TRUE);
            bRun = FALSE;
            break;
        }
        else if (   (WordCmpI(lpszToken, _T("h")) == 0)
                 || (WordCmpI(lpszToken, _T("?")) == 0))
        {
            TCHAR txt[]= L"Options:\n\n"
                         L"/RegServer:\tregister COM out-of-proc server\n"
                         L"/UnregServer:\tunregister COM out-of-proc server\n"
                         L"/ReregServer:\tunregister and register COM server\n"
                         L"no options:\trun the server";
            TCHAR title[]=_T("Usage");
            nRet = -1;
            bRun = FALSE;
            MessageBox(NULL, txt, title, MB_OK);
            break;
        }
        else if (WordCmpI (lpszToken, _T("Helper")) == 0)
        {
            Log (("SVCMAIN: Processing Helper request (cmdline=\"%ls\")...\n",
                  lpszToken + 6));

            TCHAR szTokens[] = _T (" \t");

            int vrc = VINF_SUCCESS;
            Utf8Str pipeName;

            lpszToken = FindOneOf (lpszToken, szTokens);
            if (lpszToken)
            {
                while (*lpszToken != NULL &&
                       (*lpszToken == ' ' || *lpszToken == '\t'))
                    ++ lpszToken;

                if (*lpszToken != NULL)
                {
                    Bstr str (lpszToken);
                    LPCTSTR lpszToken2 = FindOneOf (lpszToken, szTokens);
                    if (lpszToken2)
                        str.mutableRaw() [lpszToken2 - lpszToken] = '\0';
                    pipeName = Utf8Str (lpszToken);
                }
            }

            if (pipeName.isEmpty())
                vrc = VERR_INVALID_PARAMETER;

            if (VBOX_SUCCESS (vrc))
            {
                /* do the helper job */
                SVCHlpServer server;
                vrc = server.open (pipeName);
                if (VBOX_SUCCESS (vrc))
                    vrc = server.run();
            }
            if (VBOX_FAILURE (vrc))
            {
                Utf8Str err = Utf8StrFmt (
                    "Failed to process Helper request (%Vrc).", vrc);
                Log (("SVCMAIN: %s\n", err.raw()));
            }

            /* don't run the COM server */
            bRun = FALSE;
            break;
        }

        lpszToken = FindOneOf(lpszToken, szTokens);
    }

    if (bRun)
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
    CoUninitialize();
    Log(("SVCMAIN: Returning, COM server process ends.\n"));
    return nRet;
}
