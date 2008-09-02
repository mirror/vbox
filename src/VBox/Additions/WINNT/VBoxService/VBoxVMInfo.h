/* $Id$ */
/** @file
 * VBoxVMInfo - Virtual machine (guest) information for the host.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * Sun Microsystems, Inc. confidential
 * All rights reserved
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

