/** @file
 *
 * VBox Remote Desktop Protocol:
 * External Authentication Library:
 * Windows Logon Authentication.
 */

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

/* If defined, debug messages will be written to the specified file. */
// #define VRDPAUTH_DEBUG_FILE_NAME "\\VRDPAuth.log"

#include <stdio.h>
#include <string.h>

#include <Windows.h>

#include <VBox/VRDPAuth.h>

static void dprintf(const char *fmt, ...)
{
   va_list va;

   va_start(va, fmt);

   char buffer[1024];

   _vsnprintf (buffer, sizeof (buffer), fmt, va);

   OutputDebugStringA(buffer);

#ifdef VRDPAUTH_DEBUG_FILE_NAME
   FILE *f = fopen (VRDPAUTH_DEBUG_FILE_NAME, "ab");
   fprintf (f, "%s", buffer);
   fclose (f);
#endif

   va_end (va);
}

extern "C"
#if defined(_MSC_VER)
__declspec(dllexport)
#endif
VRDPAuthResult VRDPAUTHCALL VRDPAuth (PVRDPAUTHUUID pUuid,
                                      VRDPAuthGuestJudgement guestJudgement,
                                      const char *szUser,
                                      const char *szPassword,
                                      const char *szDomain)
{
    VRDPAuthResult result = VRDPAuthAccessDenied;

    LPTSTR lpszUsername = (char *)szUser;
    LPTSTR lpszDomain   = (char *)szDomain;
    LPTSTR lpszPassword = (char *)szPassword;

    /* LOGON32_LOGON_INTERACTIVE is intended for users who will be interactively using the computer,
     * such as a user being logged on by a terminal server, remote shell, or similar process.
     */
    DWORD dwLogonType     = LOGON32_LOGON_INTERACTIVE;
    DWORD dwLogonProvider = LOGON32_PROVIDER_DEFAULT;

    HANDLE hToken;

    dprintf("u[%s], d[%s], p[%s]\n", lpszUsername, lpszDomain, lpszPassword);

    BOOL fSuccess = LogonUser(lpszUsername,
                              lpszDomain,
                              lpszPassword,
                              dwLogonType,
                              dwLogonProvider,
                              &hToken);

    if (fSuccess)
    {
        dprintf("LogonUser success. hToken = %p\n", hToken);

        result = VRDPAuthAccessGranted;

        CloseHandle (hToken);
    }
    else
    {
        dprintf("LogonUser failed %08X\n", GetLastError ());
    }

    return result;
}

/* Verify the function prototype. */
static PVRDPAUTHENTRY gpfnAuthEntry = VRDPAuth;
