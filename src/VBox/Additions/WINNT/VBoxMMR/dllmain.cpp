/* $Id$ */
/** @file
 * VBoxMMR - Multimedia Redirection
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "stdafx.h"
#include "tsmfhook.h"
#include "logging.h"

#include <Winternl.h>
#include <iprt/initterm.h>
#include <VBox/VBoxGuestLib.h>

bool isWMP = false;

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:

        DisableThreadLibraryCalls(hModule);

        {
            CHAR buffer[MAX_PATH];
            GetModuleFileNameA(NULL, buffer, sizeof(buffer));
            const PCHAR pc = strrchr(buffer, '\\');
            isWMP = (0 == strcmp(pc + 1, "wmplayer.exe"));
            
            if (isWMP)
            {
                RTR3InitDll(0);
                VbglR3Init();
                VBoxMMRHookLog("VBoxMMR: Hooking wmplayer process\n");
            }
        }

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        Shutdown();
        if (isWMP)
        {
            VBoxMMRHookLog("VBoxMMR: Unhooking wmplayer process\n");
            VbglR3Term();
        }
        break;
    }
    return TRUE;
}
