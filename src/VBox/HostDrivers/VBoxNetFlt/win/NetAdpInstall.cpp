/* $Id$ */
/** @file
 * NetAdpInstall - VBoxNetAdp installer command line tool
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
 *
 * Sun Microsystems, Inc. confidential
 * All rights reserved
 */

#include <vbox/WinNetConfig.h>
#include <stdio.h>

static VOID winNetCfgLogger (LPCWSTR szString)
{
    wprintf(L"%s", szString);
}

static int InstallNetAdp()
{
    int r = 0;
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
        hr = VBoxNetCfgWinCreateHostOnlyNetworkInterface (&guid, &name, &errMsg);
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
#endif

        r = 1;

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
    return InstallNetAdp();
}
