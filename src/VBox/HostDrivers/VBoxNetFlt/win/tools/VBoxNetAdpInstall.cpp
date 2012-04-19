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

#include <VBox/VBoxNetCfg-win.h>
#include <VBox/VBoxDrvCfg-win.h>
#include <stdio.h>

#include <devguid.h>

#define VBOX_NETADP_INF L".\\VBoxNetAdp.inf"

static VOID winNetCfgLogger (LPCSTR szString)
{
    printf("%s", szString);
}

static int VBoxNetAdpInstall()
{
    int r = 1;
    VBoxNetCfgWinSetLogging(winNetCfgLogger);

    HRESULT hr = CoInitialize(NULL);
    if(hr == S_OK)
    {
#if 0 //ndef DEBUG_misha
        printf("not implemented yet, please use device manager for Host-Only net interface installation.. sorry :( \n");
#else
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

static int VBoxNetAdpUninstall()
{
    int r = 1;
    VBoxNetCfgWinSetLogging(winNetCfgLogger);

    printf("uninstalling all Host-Only interfaces..\n");

    HRESULT hr = CoInitialize(NULL);
    if(hr == S_OK)
    {
        hr = VBoxNetCfgWinRemoveAllNetDevicesOfId(L"sun_VBoxNetAdp");
        if(hr == S_OK)
        {
            hr = VBoxDrvCfgInfUninstallAllSetupDi(&GUID_DEVCLASS_NET, L"Net", L"sun_VBoxNetAdp", 0/* could be SUOI_FORCEDELETE */);
            if(hr == S_OK)
            {
                printf("uninstalled successfully\n");
            }
            else
            {
                printf("uninstalled successfully, but failed to remove infs\n");
            }
            r = 0;
        }
        else
        {
            printf("uninstall failed, hr = 0x%x\n", hr);
        }

        CoUninitialize();
    }
    else
    {
        wprintf(L"Error initializing COM (0x%x)\n", hr);
    }

    VBoxNetCfgWinSetLogging(NULL);

    return r;
}

static int VBoxNetAdpUpdate()
{
    int r = 1;
    VBoxNetCfgWinSetLogging(winNetCfgLogger);

    printf("uninstalling all Host-Only interfaces..\n");

    HRESULT hr = CoInitialize(NULL);
    if(hr == S_OK)
    {
        BOOL fRebootRequired = FALSE;
        hr = VBoxNetCfgWinUpdateHostOnlyNetworkInterface(VBOX_NETADP_INF, &fRebootRequired);
        if(hr == S_OK)
        {
            if (fRebootRequired)
                printf("!!REBOOT REQUIRED!!\n");
            printf("updated successfully\n");
            r = 0;
        }
        else
        {
            printf("update failed, hr = 0x%x\n", hr);
        }

        CoUninitialize();
    }
    else
    {
        wprintf(L"Error initializing COM (0x%x)\n", hr);
    }

    VBoxNetCfgWinSetLogging(NULL);

    return r;
}

static int VBoxNetAdpDisable()
{
    int r = 1;
    VBoxNetCfgWinSetLogging(winNetCfgLogger);

    printf("disabling all Host-Only interfaces..\n");

    HRESULT hr = CoInitialize(NULL);
    if(hr == S_OK)
    {
        hr = VBoxNetCfgWinPropChangeAllNetDevicesOfId(L"sun_VBoxNetAdp", VBOXNECTFGWINPROPCHANGE_TYPE_DISABLE);
        if(hr == S_OK)
        {
            printf("disable succeeded!\n");
            r = 0;
        }
        else
        {
            printf("disable failed, hr = 0x%x\n", hr);
        }

        CoUninitialize();
    }
    else
    {
        wprintf(L"Error initializing COM (0x%x)\n", hr);
    }

    VBoxNetCfgWinSetLogging(NULL);

    return r;
}

static int VBoxNetAdpEnable()
{
    int r = 1;
    VBoxNetCfgWinSetLogging(winNetCfgLogger);

    printf("enabling all Host-Only interfaces..\n");

    HRESULT hr = CoInitialize(NULL);
    if(hr == S_OK)
    {
        hr = VBoxNetCfgWinPropChangeAllNetDevicesOfId(L"sun_VBoxNetAdp", VBOXNECTFGWINPROPCHANGE_TYPE_ENABLE);
        if(hr == S_OK)
        {
            printf("disable succeeded!\n");
            r = 0;
        }
        else
        {
            printf("disable failed, hr = 0x%x\n", hr);
        }

        CoUninitialize();
    }
    else
    {
        wprintf(L"Error initializing COM (0x%x)\n", hr);
    }

    VBoxNetCfgWinSetLogging(NULL);

    return r;
}

static void printUsage()
{
    printf("Host-Only network adapter configuration tool\n"
            "  Usage: VBoxNetAdpInstall [cmd]\n"
            "    cmd can be one of the following values:\n"
            "       i  - install a new host-only interface\n"
            "       u  - uninstall all host-only interfaces\n"
            "       a  - update the host-only driver\n"
            "       d  - disable all host-only interfaces\n"
            "       e  - enable all host-only interfaces\n"
            "       h  - print this message\n");
}

int __cdecl main(int argc, char **argv)
{
    if (argc < 2)
        return VBoxNetAdpInstall();
    if (argc > 2)
    {
        printUsage();
        return 1;
    }

    __debugbreak();

    if (!strcmp(argv[1], "i"))
        return VBoxNetAdpInstall();
    if (!strcmp(argv[1], "u"))
        return VBoxNetAdpUninstall();
    if (!strcmp(argv[1], "a"))
        return VBoxNetAdpUpdate();
    if (!strcmp(argv[1], "d"))
        return VBoxNetAdpDisable();
    if (!strcmp(argv[1], "e"))
        return VBoxNetAdpEnable();

    printUsage();
    return !strcmp(argv[1], "h");
}
