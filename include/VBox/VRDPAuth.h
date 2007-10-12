/** @file
 * VBox Remote Desktop Protocol:
 * External Authentication Library Interface.
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

#ifndef ___VBox_vrdpauth_h
#define ___VBox_vrdpauth_h

/* The following 2 enums are 32 bits values.*/
typedef enum _VRDPAuthResult
{
    VRDPAuthAccessDenied    = 0,
    VRDPAuthAccessGranted   = 1,
    VRDPAuthDelegateToGuest = 2,
    VRDPAuthSizeHack        = 0x7fffffff
} VRDPAuthResult;

typedef enum _VRDPAuthGuestJudgement
{
    VRDPAuthGuestNotAsked      = 0,
    VRDPAuthGuestAccessDenied  = 1,
    VRDPAuthGuestNoJudgement   = 2,
    VRDPAuthGuestAccessGranted = 3,
    VRDPAuthGuestNotReacted    = 4,
    VRDPAuthGuestSizeHack      = 0x7fffffff
} VRDPAuthGuestJudgement;

/* UUID memory representation. Array of 16 bytes. */
typedef unsigned char VRDPAUTHUUID[16];
typedef VRDPAUTHUUID *PVRDPAUTHUUID;


/* The library entry point calling convention. */
#ifdef _MSC_VER
# define VRDPAUTHCALL __cdecl
#elif defined(__GNUC__)
# define VRDPAUTHCALL
#else
# error "Unsupported compiler"
#endif


/**
 * Authentication library entry point. Decides whether to allow
 * a client connection.
 *
 * Parameters:
 *
 *   pUuid            Pointer to the UUID of the virtual machine
 *                    which the client connected to.
 *   guestJudgement   Result of the guest authentication.
 *   szUser           User name passed in by the client (UTF8).
 *   szPassword       Password passed in by the client (UTF8).
 *   szDomain         Domain passed in by the client (UTF8).
 *
 * Return code:
 *
 *   VRDPAuthAccessDenied    Client access has been denied.
 *   VRDPAuthAccessGranted   Client has the right to use the
 *                           virtual machine.
 *   VRDPAuthDelegateToGuest Guest operating system must
 *                           authenticate the client and the
 *                           library must be called again with
 *                           the result of the guest
 *                           authentication.
 */
typedef VRDPAuthResult VRDPAUTHCALL VRDPAUTHENTRY(PVRDPAUTHUUID pUuid,
                                                  VRDPAuthGuestJudgement guestJudgement,
                                                  const char *szUser,
                                                  const char *szPassword,
                                                  const char *szDomain);


typedef VRDPAUTHENTRY *PVRDPAUTHENTRY;

/**
 * Authentication library entry point version 2. Decides whether to allow
 * a client connection.
 *
 * Parameters:
 *
 *   pUuid            Pointer to the UUID of the virtual machine
 *                    which the client connected to.
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
 *   VRDPAuthAccessDenied    Client access has been denied.
 *   VRDPAuthAccessGranted   Client has the right to use the
 *                           virtual machine.
 *   VRDPAuthDelegateToGuest Guest operating system must
 *                           authenticate the client and the
 *                           library must be called again with
 *                           the result of the guest
 *                           authentication.
 *
 * Note: When 'fLogon' is 0, only pUuid and clientId are valid and the return
 *       code is ignored.
 */
typedef VRDPAuthResult VRDPAUTHCALL VRDPAUTHENTRY2(PVRDPAUTHUUID pUuid,
                                                   VRDPAuthGuestJudgement guestJudgement,
                                                   const char *szUser,
                                                   const char *szPassword,
                                                   const char *szDomain,
                                                   int fLogon,
                                                   unsigned clientId);


typedef VRDPAUTHENTRY2 *PVRDPAUTHENTRY2;

#endif
