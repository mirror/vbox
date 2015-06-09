/** @file
 * VirtualBox External Authentication Library Interface.
 */

/*
 * Copyright (C) 2006-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef ___VBox_vboxauth_h
#define ___VBox_vboxauth_h

/* The following 2 enums are 32 bits values.*/
typedef enum AuthResult
{
    AuthResultAccessDenied    = 0,
    AuthResultAccessGranted   = 1,
    AuthResultDelegateToGuest = 2,
    AuthResultSizeHack        = 0x7fffffff
} AuthResult;

typedef enum AuthGuestJudgement
{
    AuthGuestNotAsked      = 0,
    AuthGuestAccessDenied  = 1,
    AuthGuestNoJudgement   = 2,
    AuthGuestAccessGranted = 3,
    AuthGuestNotReacted    = 4,
    AuthGuestSizeHack      = 0x7fffffff
} AuthGuestJudgement;

/* UUID memory representation. Array of 16 bytes. */
typedef unsigned char AUTHUUID[16];
typedef AUTHUUID *PAUTHUUID;
/*
Note: VirtualBox uses a consistent binary representation of UUIDs on all platforms. For this reason
the integer fields comprising the UUID are stored as little endian values. If you want to pass such
UUIDs to code which assumes that the integer fields are big endian (often also called network byte
order), you need to adjust the contents of the UUID to e.g. achieve the same string representation.
The required changes are:
 * reverse the order of byte 0, 1, 2 and 3
 * reverse the order of byte 4 and 5
 * reverse the order of byte 6 and 7.
Using this conversion you will get identical results when converting the binary UUID to the string
representation.
*/

/* The library entry point calling convention. */
#ifdef _MSC_VER
# define AUTHCALL __cdecl
#elif defined(__GNUC__)
# define AUTHCALL
#else
# error "Unsupported compiler"
#endif


/**
 * Authentication library entry point.
 *
 * Parameters:
 *
 *   pUuid            Pointer to the UUID of the accessed virtual machine. Can be NULL.
 *   guestJudgement   Result of the guest authentication.
 *   szUser           User name passed in by the client (UTF8).
 *   szPassword       Password passed in by the client (UTF8).
 *   szDomain         Domain passed in by the client (UTF8).
 *
 * Return code:
 *
 *   AuthAccessDenied    Client access has been denied.
 *   AuthAccessGranted   Client has the right to use the
 *                       virtual machine.
 *   AuthDelegateToGuest Guest operating system must
 *                       authenticate the client and the
 *                       library must be called again with
 *                       the result of the guest
 *                       authentication.
 */
typedef AuthResult AUTHCALL AUTHENTRY(PAUTHUUID pUuid,
                                      AuthGuestJudgement guestJudgement,
                                      const char *szUser,
                                      const char *szPassword,
                                      const char *szDomain);


typedef AUTHENTRY *PAUTHENTRY;

#define AUTHENTRY_NAME "VRDPAuth"

/**
 * Authentication library entry point version 2.
 *
 * Parameters:
 *
 *   pUuid            Pointer to the UUID of the accessed virtual machine. Can be NULL.
 *   guestJudgement   Result of the guest authentication.
 *   szUser           User name passed in by the client (UTF8).
 *   szPassword       Password passed in by the client (UTF8).
 *   szDomain         Domain passed in by the client (UTF8).
 *   fLogon           Boolean flag. Indicates whether the entry point is called
 *                    for a client logon or the client disconnect.
 *   clientId         Server side unique identifier of the client.
 *
 * Return code:
 *
 *   AuthAccessDenied    Client access has been denied.
 *   AuthAccessGranted   Client has the right to use the
 *                       virtual machine.
 *   AuthDelegateToGuest Guest operating system must
 *                       authenticate the client and the
 *                       library must be called again with
 *                       the result of the guest
 *                       authentication.
 *
 * Note: When 'fLogon' is 0, only pUuid and clientId are valid and the return
 *       code is ignored.
 */
typedef AuthResult AUTHCALL AUTHENTRY2(PAUTHUUID pUuid,
                                       AuthGuestJudgement guestJudgement,
                                       const char *szUser,
                                       const char *szPassword,
                                       const char *szDomain,
                                       int fLogon,
                                       unsigned clientId);


typedef AUTHENTRY2 *PAUTHENTRY2;

#define AUTHENTRY2_NAME "VRDPAuth2"

/**
 * Authentication library entry point version 3.
 *
 * Parameters:
 *
 *   szCaller         The name of the component which calls the library (UTF8).
 *   pUuid            Pointer to the UUID of the accessed virtual machine. Can be NULL.
 *   guestJudgement   Result of the guest authentication.
 *   szUser           User name passed in by the client (UTF8).
 *   szPassword       Password passed in by the client (UTF8).
 *   szDomain         Domain passed in by the client (UTF8).
 *   fLogon           Boolean flag. Indicates whether the entry point is called
 *                    for a client logon or the client disconnect.
 *   clientId         Server side unique identifier of the client.
 *
 * Return code:
 *
 *   AuthResultAccessDenied    Client access has been denied.
 *   AuthResultAccessGranted   Client has the right to use the
 *                             virtual machine.
 *   AuthResultDelegateToGuest Guest operating system must
 *                             authenticate the client and the
 *                             library must be called again with
 *                             the result of the guest
 *                             authentication.
 *
 * Note: When 'fLogon' is 0, only pszCaller, pUuid and clientId are valid and the return
 *       code is ignored.
 */
typedef AuthResult AUTHCALL AUTHENTRY3(const char *szCaller,
                                       PAUTHUUID pUuid,
                                       AuthGuestJudgement guestJudgement,
                                       const char *szUser,
                                       const char *szPassword,
                                       const char *szDomain,
                                       int fLogon,
                                       unsigned clientId);


typedef AUTHENTRY3 *PAUTHENTRY3;

#define AUTHENTRY3_NAME "AuthEntry"

#endif
