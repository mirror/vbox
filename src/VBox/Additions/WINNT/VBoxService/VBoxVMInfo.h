/* $Id$ */
/** @file
 * VBoxVMInfo - Virtual machine (guest) information for the host.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___VBOXSERVICEVMINFO_H
#define ___VBOXSERVICEVMINFO_H

/* The guest management service prototypes. */
int                vboxVMInfoInit    (const VBOXSERVICEENV *pEnv, void **ppInstance, bool *pfStartThread);
unsigned __stdcall vboxVMInfoThread  (void *pInstance);
void               vboxVMInfoDestroy (const VBOXSERVICEENV *pEnv, void *pInstance);

/* The following constant may be defined by including NtStatus.h. */
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)

/* Prototypes for dynamic loading. */
typedef DWORD (WINAPI* fnWTSGetActiveConsoleSessionId)();

/* The information context. */
typedef struct _VBOXINFORMATIONCONTEXT
{
    const VBOXSERVICEENV *pEnv;
    uint32_t iInfoSvcClientID;
    fnWTSGetActiveConsoleSessionId pfnWTSGetActiveConsoleSessionId;
    BOOL fFirstRun;
} VBOXINFORMATIONCONTEXT;

/* Some wrappers. */
int vboxVMInfoWriteProp (VBOXINFORMATIONCONTEXT* a_pCtx, char *a_pszKey, char *a_pszValue);
int vboxVMInfoWritePropInt (VBOXINFORMATIONCONTEXT* a_pCtx, char *a_pszKey, int a_iValue);

#endif /* !___VBOXSERVICEVMINFO_H */

