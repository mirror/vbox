/* $Id$ */
/** @file
 * NetAdpInstall - VBoxNetAdp installer command line tool
 */

/*
 * Copyright (C) 2009 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <vbox/WinNetConfig.h>
#include <stdio.h>

#define VBOX_NETADP_INF L".\\VBoxNetAdp.inf"

static VOID winNetCfgLogger (LPCWSTR szString)
{
    wprintf(L"%s", szString);
}

static int InstallNetAdp()
{
    int r = 1;
    VBoxNetCfgWinSetLogging(winNetCfgLogger);

    HRESULT hr = CoInitialize(NULL);
    if(hr == S_OK)
    {
#ifndef DEBUG_misha
        printf("not implemented yet, please use device manager for Host-Only net interface installation.. sorry :( \n");
#else
        /* this installs the Net Adp from the pre-installed driver package,
         * it does NOT install the new driver package, so the installation might use old drivers
         * or fail in case no NetAdp package is currently installed
         * the code is here for debugging NetAdp installation only */
        GUID guid;
        BSTR name, errMsg;
        printf("adding host-only interface..\n");
        DWORD WinEr;
        WCHAR MpInf[MAX_PATH];
        GetFullPathNameW(VBOX_NETADP_INF, sizeof(MpInf)/sizeof(MpInf[0]), MpInf, NULL);
        WinEr = GetLastError();
        if(WinEr == ERROR_SUCCESS)
        {
            hr = VBoxNetCfgWinCreateHostOnlyNetworkInterface (MpInf, true, &guid, &name, &errMsg);
            if(hr == S_OK)
            {
                ULONG ip, mask;
                hr = VBoxNetCfgWinGenHostOnlyNetworkNetworkIp(&ip, &mask);
                if(hr == S_OK)
                {
                    /* ip returned by VBoxNetCfgWinGenHostOnlyNetworkNetworkIp is a network ip,
                     * i.e. 192.168.xxx.0, assign  192.168.xxx.1 for the hostonly adapter */
                    ip = ip | (1 << 24);
                    hr = VBoxNetCfgWinEnableStaticIpConfig(&guid, ip, mask);
                    if(hr != S_OK)
                    {
                        printf("VBoxNetCfgWinEnableStaticIpConfig failed: hr = 0x%x\n", hr);
                    }
                    else
                    {
                        r = 0;
                    }
                }
                else
                {
                    printf("VBoxNetCfgWinGenHostOnlyNetworkNetworkIp failed: hr = 0x%x\n", hr);
                }
            }
            else
            {
                printf("VBoxNetCfgWinCreateHostOnlyNetworkInterface failed: hr = 0x%x\n", hr);
            }
        }
        else
        {
            printf("GetFullPathNameW failed: winEr = %d\n", WinEr);
        }
#endif

        CoUninitialize();
    }
    else
    {
        wprintf(L"Error initializing COM (0x%x)\n", hr);
    }

    VBoxNetCfgWinSetLogging(NULL);

    return r;
}

int __cdecl main(int argc, char **argv)
{
    return InstallNetAdp();
}
