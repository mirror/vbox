/* $Id$ */
/** @file
 * IPRT - Win32 DllMain (Ring-3).
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <Windows.h>
#include <iprt/thread.h>
#include "internal/thread.h"



/**
 * The Dll main entry point.
 */
BOOL __stdcall DllMain(HANDLE hModule, DWORD dwReason, PVOID pvReserved)
{
    switch (dwReason)
    {
        case DLL_PROCESS_ATTACH:
        case DLL_PROCESS_DETACH:
        case DLL_THREAD_ATTACH:
        default:
            /* ignore */
            break;

        case DLL_THREAD_DETACH:
            rtThreadNativeDetach();
            break;
    }
    return TRUE;
}
