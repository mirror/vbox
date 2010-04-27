/* $Id$ */
/** @file
 * NetAdpUninstall - VBoxNetAdp uninstaller command line tool
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

#include <devguid.h>


static VOID winNetCfgLogger (LPCWSTR szString)
{
    wprintf(L"%s", szString);
}

static int UninstallNetAdp()
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
            hr = VBoxNetCfgWinUninstallInfs (&GUID_DEVCLASS_NET, L"sun_VBoxNetAdp", 0/* could be SUOI_FORCEDELETE */);
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

int __cdecl main(int argc, char **argv)
{
    return UninstallNetAdp();
}
