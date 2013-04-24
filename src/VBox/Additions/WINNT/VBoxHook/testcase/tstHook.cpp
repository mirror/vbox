/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <windows.h>
#include <VBoxHook.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    printf("Enabling global hook\n");

    HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, VBOXHOOK_GLOBAL_WT_EVENT_NAME);

    VBoxHookInstallWindowTracker(GetModuleHandle("VBoxHook.dll"));
    getchar();

    printf("Disabling global hook\n");
    VBoxHookRemoveWindowTracker();
    return 0;
}
