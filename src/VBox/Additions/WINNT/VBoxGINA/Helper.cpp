/** @file
 *
 * VBoxGINA -- Windows Logon DLL for VirtualBox Helper Functions
 *
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

#include <windows.h>
#include "winwlx.h"
#include "Helper.h"
#include "VBoxGINA.h"

#include <VBox/VBoxGuest.h>
#include <iprt/string.h>

/* the credentials */
wchar_t g_Username[VMMDEV_CREDENTIALS_STRLEN];
wchar_t g_Password[VMMDEV_CREDENTIALS_STRLEN];
wchar_t g_Domain[VMMDEV_CREDENTIALS_STRLEN];


HANDLE getVBoxDriver(void)
{
    static HANDLE sVBoxDriver = INVALID_HANDLE_VALUE;
    if (sVBoxDriver == INVALID_HANDLE_VALUE)
    {
        sVBoxDriver = CreateFile(L"\\\\.\\VBoxGuest", /** @todo use define */
                                 GENERIC_READ | GENERIC_WRITE,
                                 FILE_SHARE_READ | FILE_SHARE_WRITE,
                                 NULL,
                                 OPEN_EXISTING,
                                 FILE_ATTRIBUTE_NORMAL,
                                 NULL);
        if (sVBoxDriver == INVALID_HANDLE_VALUE)
            Log(("VBoxGINA::sVBoxDriver: failed to open VBoxGuest driver, last error = %d\n", GetLastError()));
    }
    return sVBoxDriver;
}

void credentialsReset(void)
{
    memset(g_Username, '\0', sizeof(g_Username));
    memset(g_Password, '\0', sizeof(g_Password));
    memset(g_Domain, '\0', sizeof(g_Domain));
}

bool credentialsAvailable(void)
{
    HANDLE vboxDriver = getVBoxDriver();
    if (vboxDriver ==  INVALID_HANDLE_VALUE)
        return false;

    /* query the VMMDev whether there are credentials */
    VMMDevCredentials vmmreqCredentials = {0};
    vmmdevInitRequest((VMMDevRequestHeader*)&vmmreqCredentials, VMMDevReq_QueryCredentials);
    vmmreqCredentials.u32Flags |= VMMDEV_CREDENTIALS_QUERYPRESENCE;
    DWORD cbReturned;
    if (!DeviceIoControl(vboxDriver, IOCTL_VBOXGUEST_VMMREQUEST, &vmmreqCredentials, sizeof(vmmreqCredentials),
                         &vmmreqCredentials, sizeof(vmmreqCredentials), &cbReturned, NULL))
    {
        Log(("VBoxGINA::credentialsAvailable: error doing IOCTL, last error: %d\n", GetLastError()));
        return false;
    }
    bool fAvailable = ((vmmreqCredentials.u32Flags & VMMDEV_CREDENTIALS_PRESENT) != 0);
    Log(("VBoxGINA::credentialsAvailable: fAvailable: %d\n", fAvailable));
    return fAvailable;
}

bool credentialsRetrieve(void)
{
    Log(("VBoxGINA::credentialsRetrieve\n"));

    HANDLE vboxDriver = getVBoxDriver();
    if (vboxDriver == INVALID_HANDLE_VALUE)
        return false;

    /* to be safe, reset the credentials */
    credentialsReset();

    /* query the credentials */
    VMMDevCredentials vmmreqCredentials = {0};
    vmmdevInitRequest((VMMDevRequestHeader*)&vmmreqCredentials, VMMDevReq_QueryCredentials);
    vmmreqCredentials.u32Flags |= VMMDEV_CREDENTIALS_READ;
    vmmreqCredentials.u32Flags |= VMMDEV_CREDENTIALS_CLEAR;
    DWORD cbReturned;
    if (!DeviceIoControl(vboxDriver, IOCTL_VBOXGUEST_VMMREQUEST, &vmmreqCredentials, sizeof(vmmreqCredentials),
                         &vmmreqCredentials, sizeof(vmmreqCredentials), &cbReturned, NULL))
    {
        Log(("VBoxGINA::credentialsRetrieve: error doing IOCTL, last error: %d\n", GetLastError()));
        return false;
    }
    /* convert from UTF-8 to UCS2 and store in global variables */
    PRTUCS2 ptr = NULL;
    if (VBOX_SUCCESS(RTStrUtf8ToUcs2(&ptr, vmmreqCredentials.szUserName)) && ptr)
    {
        wcscpy(g_Username, ptr);
        RTStrUcs2Free(ptr);
    }
    ptr = NULL;
    if (VBOX_SUCCESS(RTStrUtf8ToUcs2(&ptr, vmmreqCredentials.szPassword)) && ptr)
    {
        wcscpy(g_Password, ptr);
        RTStrUcs2Free(ptr);
    }
    ptr = NULL;
    if (VBOX_SUCCESS(RTStrUtf8ToUcs2(&ptr, vmmreqCredentials.szDomain)) && ptr)
    {
        wcscpy(g_Domain, ptr);
        RTStrUcs2Free(ptr);
    }
    Log(("VBoxGINA::credentialsRetrieve: returning user '%s', password '%s', domain '%s'\n",
         vmmreqCredentials.szUserName, vmmreqCredentials.szPassword, vmmreqCredentials.szDomain));
    return true;
}

/* handle of the poller thread */
RTTHREAD gThreadPoller = NIL_RTTHREAD;


/**
 * Poller thread. Checks periodically whether there are credentials.
 */
static DECLCALLBACK(int) credentialsPoller(RTTHREAD ThreadSelf, void *pvUser)
{
    Log(("VBoxGINA::credentialsPoller\n"));

    do
    {
        if (credentialsAvailable())
        {
            Log(("VBoxGINA::credentialsPoller: got credentials, simulating C-A-D\n"));
            /* tell WinLogon to start the attestation process */
            pWlxFuncs->WlxSasNotify(hGinaWlx, WLX_SAS_TYPE_CTRL_ALT_DEL);
            /* time to say goodbye */
            return 0;
        }
        /* wait a bit */
        if (RTThreadUserWait(ThreadSelf, 500) == VINF_SUCCESS)
        {
            Log(("VBoxGINA::credentialsPoller: we were asked to terminate\n"));
            /* we were asked to terminate, do that instantly! */
            return 0;
        }
    }
    while (1);

    return 0;
}

bool credentialsPollerCreate(void)
{
    Log(("VBoxGINA::credentialsPollerCreate\n"));

    /* don't create more than one of them */
    if (gThreadPoller != NIL_RTTHREAD)
    {
        Log(("VBoxGINA::credentialsPollerCreate: thread already running, returning!\n"));
        return false;
    }

    /* create the poller thread */
    int rc = RTThreadCreate(&gThreadPoller, credentialsPoller, NULL, 0, RTTHREADTYPE_INFREQUENT_POLLER,
                            RTTHREADFLAGS_WAITABLE, "creds");
    if (VBOX_FAILURE(rc))
    {
        Log(("VBoxGINA::credentialsPollerCreate: failed to create thread, rc = %Vrc\n", rc));
        return false;
    }
    return true;
}

bool credentialsPollerTerminate(void)
{
    Log(("VBoxGINA::credentialsPollerTerminate\n"));

    if (gThreadPoller == NIL_RTTHREAD)
    {
        Log(("VBoxGINA::credentialsPollerTerminate: either thread or exit sem is NULL!\n"));
        return false;
    }
    /* post termination event semaphore */
    int rc = RTThreadUserSignal(gThreadPoller);
    if (VBOX_SUCCESS(rc))
    {
        Log(("VBoxGINA::credentialsPollerTerminate: waiting for thread to terminate\n"));
        /* wait until the thread has terminated */
        rc = RTThreadWait(gThreadPoller, RT_INDEFINITE_WAIT, NULL);
        Log(("VBoxGINA::credentialsPollerTermiante: thread has (probably) terminated (rc = %Vrc)\n", rc));
    }
    else
    {
        /* failed to signal the thread - very unlikely - so no point in waiting long. */
        Log(("VBoxGINA::credentialsPollerTermiante: failed to signal semaphore, rc = %Vrc\n", rc));
        rc = RTThreadWait(gThreadPoller, 100, NULL);
        Log(("VBoxGINA::credentialsPollerTermiante: thread has terminated? wait rc = %Vrc\n", rc));
    }
    /* now cleanup */
    gThreadPoller = NIL_RTTHREAD;
    return true;
}
