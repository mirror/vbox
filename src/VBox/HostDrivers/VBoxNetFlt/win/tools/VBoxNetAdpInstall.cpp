/* $Id$ */
/** @file
 * NetAdpInstall - VBoxNetAdp installer command line tool.
 */

/*
 * Copyright (C) 2009-2012 Oracle Corporation
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

#define VBOX_NETADP_INF L"VBoxNetAdp.inf"

static VOID winNetCfgLogger(LPCSTR szString)
{
    printf("%s\n", szString);
}

static int VBoxNetAdpInstall(void)
{
    VBoxNetCfgWinSetLogging(winNetCfgLogger);

    HRESULT hr = CoInitialize(NULL);
    if (SUCCEEDED(hr))
    {
        printf("adding host-only interface..\n");

        DWORD dwErr = ERROR_SUCCESS;
        WCHAR MpInf[MAX_PATH];

        if (!GetFullPathNameW(VBOX_NETADP_INF, sizeof(MpInf)/sizeof(MpInf[0]), MpInf, NULL))
            dwErr = GetLastError();

        if (dwErr == ERROR_SUCCESS)
        {
            GUID guid;
            BSTR name, errMsg;

            hr = VBoxNetCfgWinCreateHostOnlyNetworkInterface (MpInf, true, &guid, &name, &errMsg);
            if (SUCCEEDED(hr))
            {
                ULONG ip, mask;
                hr = VBoxNetCfgWinGenHostOnlyNetworkNetworkIp(&ip, &mask);
                if (SUCCEEDED(hr))
                {
                    /* ip returned by VBoxNetCfgWinGenHostOnlyNetworkNetworkIp is a network ip,
                     * i.e. 192.168.xxx.0, assign  192.168.xxx.1 for the hostonly adapter */
                    ip = ip | (1 << 24);
                    hr = VBoxNetCfgWinEnableStaticIpConfig(&guid, ip, mask);
                    if (SUCCEEDED(hr))
                    {
                        printf("installation successful\n");
                    }
                    else
                        printf("VBoxNetCfgWinEnableStaticIpConfig failed: hr = 0x%x\n", hr);
                }
                else
                    printf("VBoxNetCfgWinGenHostOnlyNetworkNetworkIp failed: hr = 0x%x\n", hr);
            }
            else
                printf("VBoxNetCfgWinCreateHostOnlyNetworkInterface failed: hr = 0x%x\n", hr);
        }
        else
        {
            printf("GetFullPathNameW failed: winEr = %d\n", dwErr);
            hr = HRESULT_FROM_WIN32(dwErr);

        }
        CoUninitialize();
    }
    else
        printf("Error initializing COM (0x%x)\n", hr);

    VBoxNetCfgWinSetLogging(NULL);

    return SUCCEEDED(hr) ? 0 : 1;
}

static int VBoxNetAdpUninstall(void)
{
    VBoxNetCfgWinSetLogging(winNetCfgLogger);

    printf("uninstalling all host-only interfaces..\n");

    HRESULT hr = CoInitialize(NULL);
    if (SUCCEEDED(hr))
    {
        hr = VBoxNetCfgWinRemoveAllNetDevicesOfId(L"sun_VBoxNetAdp");
        if (SUCCEEDED(hr))
        {
            hr = VBoxDrvCfgInfUninstallAllSetupDi(&GUID_DEVCLASS_NET, L"Net", L"sun_VBoxNetAdp", 0/* could be SUOI_FORCEDELETE */);
            if (SUCCEEDED(hr))
            {
                printf("uninstallation successful\n");
            }
            else
                printf("uninstalled successfully, but failed to remove infs\n");
        }
        else
            printf("uninstall failed, hr = 0x%x\n", hr);
        CoUninitialize();
    }
    else
        printf("Error initializing COM (0x%x)\n", hr);

    VBoxNetCfgWinSetLogging(NULL);

    return SUCCEEDED(hr) ? 0 : 1;
}

static int VBoxNetAdpUpdate(void)
{
    VBoxNetCfgWinSetLogging(winNetCfgLogger);

    printf("uninstalling all host-only interfaces..\n");

    HRESULT hr = CoInitialize(NULL);
    if (SUCCEEDED(hr))
    {
        BOOL fRebootRequired = FALSE;
        hr = VBoxNetCfgWinUpdateHostOnlyNetworkInterface(VBOX_NETADP_INF, &fRebootRequired);
        if (SUCCEEDED(hr))
        {
            if (fRebootRequired)
                printf("!!REBOOT REQUIRED!!\n");
            printf("updated successfully\n");
        }
        else
            printf("update failed, hr = 0x%x\n", hr);

        CoUninitialize();
    }
    else
        printf("Error initializing COM (0x%x)\n", hr);

    VBoxNetCfgWinSetLogging(NULL);

    return SUCCEEDED(hr) ? 0 : 1;
}

static int VBoxNetAdpDisable(void)
{
    VBoxNetCfgWinSetLogging(winNetCfgLogger);

    printf("disabling all host-only interfaces..\n");

    HRESULT hr = CoInitialize(NULL);
    if (SUCCEEDED(hr))
    {
        hr = VBoxNetCfgWinPropChangeAllNetDevicesOfId(L"sun_VBoxNetAdp", VBOXNECTFGWINPROPCHANGE_TYPE_DISABLE);
        if (SUCCEEDED(hr))
        {
            printf("disabling successful\n");
        }
        else
            printf("disable failed, hr = 0x%x\n", hr);

        CoUninitialize();
    }
    else
        printf("Error initializing COM (0x%x)\n", hr);

    VBoxNetCfgWinSetLogging(NULL);

    return SUCCEEDED(hr) ? 0 : 1;
}

static int VBoxNetAdpEnable(void)
{
    VBoxNetCfgWinSetLogging(winNetCfgLogger);

    printf("enabling all host-only interfaces..\n");

    HRESULT hr = CoInitialize(NULL);
    if (SUCCEEDED(hr))
    {
        hr = VBoxNetCfgWinPropChangeAllNetDevicesOfId(L"sun_VBoxNetAdp", VBOXNECTFGWINPROPCHANGE_TYPE_ENABLE);
        if (SUCCEEDED(hr))
        {
            printf("enabling successful\n");
        }
        else
            printf("enabling failed, hr = 0x%x\n", hr);

        CoUninitialize();
    }
    else
        printf("Error initializing COM (0x%x)\n", hr);

    VBoxNetCfgWinSetLogging(NULL);

    return SUCCEEDED(hr) ? 0 : 1;
}

static void printUsage(void)
{
    printf("host-only network adapter configuration tool\n"
            "  Usage: VBoxNetAdpInstall [cmd]\n"
            "    cmd can be one of the following values:\n"
            "       i  - install a new host-only interface (default command)\n"
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
