/* $Id$ */
/** @file
 * NetFltInstall - VBoxNetFlt installer command line tool
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
 *
 * Sun Microsystems, Inc. confidential
 * All rights reserved
 */

#include <vbox/WinNetConfig.h>
#include <devguid.h>
#include <stdio.h>

#define NETFLT_ID  L"sun_VBoxNetFlt"
#define VBOX_NETCFG_APP_NAME L"NetFltInstall"
#define VBOX_NETFLT_PT_INF L".\\VBoxNetFlt.inf"
#define VBOX_NETFLT_MP_INF L".\\VBoxNetFlt_m.inf"
#define VBOX_NETFLT_RETRIES 10


static VOID winNetCfgLogger (LPCWSTR szString)
{
    wprintf(L"%s", szString);
}

static int InstallNetFlt()
{
    WCHAR PtInf[MAX_PATH];
    WCHAR MpInf[MAX_PATH];
    INetCfg *pnc;
    LPWSTR lpszLockedBy = NULL;
    int r = 1;

    VBoxNetCfgWinSetLogging(winNetCfgLogger);

    HRESULT hr = CoInitialize(NULL);
    if(hr == S_OK)
    {
        int i = 0;
        do
        {
            hr = VBoxNetCfgWinQueryINetCfg(TRUE, VBOX_NETCFG_APP_NAME, &pnc, &lpszLockedBy);
            if(hr == S_OK)
            {
                DWORD WinEr;
                GetFullPathNameW(VBOX_NETFLT_PT_INF, sizeof(PtInf)/sizeof(PtInf[0]), PtInf, NULL);
                WinEr = GetLastError();
                if(WinEr == ERROR_SUCCESS)
                {
                    GetFullPathNameW(VBOX_NETFLT_MP_INF, sizeof(MpInf)/sizeof(MpInf[0]), MpInf, NULL);
                    WinEr = GetLastError();
                    if(WinEr == ERROR_SUCCESS)
                    {
                        LPCWSTR aInfs[] = {PtInf, MpInf};
                        hr = VBoxNetCfgWinNetFltInstall(pnc, aInfs, 2);
                        if(hr == S_OK)
                        {
                            wprintf(L"installed successfully\n");
                            r = 0;
                        }
                        else
                        {
                            wprintf(L"error installing VBoxNetFlt (0x%x)\n", hr);
                        }
                    }
                    else
                    {
                        hr =  HRESULT_FROM_WIN32(WinEr);
                        wprintf(L"error getting full inf path for VBoxNetFlt_m.inf (0x%x)\n", hr);
                    }
                }
                else
                {
                    hr =  HRESULT_FROM_WIN32(WinEr);
                    wprintf(L"error getting full inf path for VBoxNetFlt.inf (0x%x)\n", hr);
                }


                VBoxNetCfgWinReleaseINetCfg(pnc, TRUE);
                break;
            }
            else if(hr == NETCFG_E_NO_WRITE_LOCK && lpszLockedBy)
            {
                if(i < VBOX_NETFLT_RETRIES && !wcscmp(lpszLockedBy, L"6to4svc.dll"))
                {
                    wprintf(L"6to4svc.dll is holding the lock, retrying %d out of %d\n", ++i, VBOX_NETFLT_RETRIES);
                    CoTaskMemFree(lpszLockedBy);
                }
                else
                {
                    wprintf(L"Error: write lock is owned by another application (%s), close the application and retry installing\n", lpszLockedBy);
                    r = 1;
                    CoTaskMemFree(lpszLockedBy);
                    break;
                }
            }
            else
            {
                wprintf(L"Error getting the INetCfg interface (0x%x)\n", hr);
                r = 1;
                break;
            }
        } while(true);

        CoUninitialize();
    }
    else
    {
        wprintf(L"Error initializing COM (0x%x)\n", hr);
        r = 1;
    }

    VBoxNetCfgWinSetLogging(NULL);

    return r;
}

int __cdecl main(int argc, char **argv)
{
    return InstallNetFlt();
}
