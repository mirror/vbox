/* $Id$ */
/** @file
 * NetAdpUninstall - VBoxNetAdp uninstaller command line tool
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

static int UninstallNetAdp()
{
    int r = 0;
    VBoxNetCfgWinSetLogging(winNetCfgLogger);

    printf("uninstalling all Host-Only interfaces..\n");

    HRESULT hr = CoInitialize(NULL);
    if(hr == S_OK)
    {
        hr = VBoxNetCfgWinRemoveAllNetDevicesOfId(L"sun_VBoxNetAdp");
        if(hr == S_OK)
        {
            printf("uninstalled successfully\n");
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
        r = 1;
    }

    VBoxNetCfgWinSetLogging(NULL);

    return r;
}

int __cdecl main(int argc, char **argv)
{
    return UninstallNetAdp();
}
