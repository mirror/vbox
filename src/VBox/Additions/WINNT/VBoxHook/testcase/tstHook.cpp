/* $Id$ */
/** @file
 * VBoxHook testcase.
 */

/*
 * Copyright (C) 2006-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/win/windows.h>
#include <VBoxHook.h>
#include <iprt/types.h>


int main()
{
    DWORD cbIgnores;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), RT_STR_TUPLE("Enabling global hook\r\n"), &cbIgnores, NULL);

    HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, VBOXHOOK_GLOBAL_WT_EVENT_NAME);

    VBoxHookInstallWindowTracker(GetModuleHandle("VBoxHook.dll"));

    /* wait for input. */
    uint8_t ch;
    ReadFile(GetStdHandle(STD_INPUT_HANDLE), &ch, sizeof(ch), &cbIgnores, NULL);

    /* disable hook. */
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), RT_STR_TUPLE("Disabling global hook\r\n"), &cbIgnores, NULL);
    VBoxHookRemoveWindowTracker();
    CloseHandle(hEvent);

    return 0;
}

