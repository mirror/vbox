/** @file
 *
 * tstSessionHack
 *
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#define _WIN32_WINNT 0x0500
#include <windows.h>
#include <VBox/VBoxDev.h>
#include <VBox/VBoxGuest.h>
#include <VBoxGuestInternal.h>
#include <iprt/assert.h>
#include <stdio.h>

void main(int argc, char *argv[])
{
    DWORD cbReturned;
    DWORD status = NO_ERROR;
    HANDLE gVBoxDriver;

    /* open VBox guest driver */
    gVBoxDriver = CreateFile(VBOXGUEST_DEVICE_NAME,
                             GENERIC_READ | GENERIC_WRITE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE,
                             NULL,
                             OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                             NULL);
    if (gVBoxDriver == INVALID_HANDLE_VALUE)
    {
        printf("Could not open VBox Guest Additions driver! rc = %d\n", GetLastError());
        return;
    }

    if (argc == 1)
        printf("Installing session hack\n");
    else
        printf("Removing session hack\n");
 
    if (!DeviceIoControl (gVBoxDriver, (argc == 1) ? IOCTL_VBOXGUEST_ENABLE_VRDP_SESSION : IOCTL_VBOXGUEST_DISABLE_VRDP_SESSION, NULL, 0, NULL, 0, &cbReturned, NULL))
    {
        printf("VBoxRestoreThread: DeviceIOControl(CtlMask) failed, SeamlessChangeThread exited\n");
    }
    CloseHandle(gVBoxDriver);
}

