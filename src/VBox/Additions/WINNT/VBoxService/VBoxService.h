/* $Id$ */
/** @file
 * VBoxService - Guest Additions Service
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * Sun Microsystems, Inc. confidential
 * All rights reserved
 */

#ifndef ___VBOXSERVICE_H
#define ___VBOXSERVICE_H

#include <winsock2.h>
#include <ws2tcpip.h>

#include <tchar.h>
#include <stdio.h>      /* Get rid of this - use unicode only later! */
#include <stdarg.h>
#include <process.h>
#include <aclapi.h>

#include <iprt/alloc.h>
#include <iprt/initterm.h>
#include <iprt/string.h>
#include <iprt/log.h>

#include <VBox/version.h>
#include <VBox/VBoxGuest.h>
#include <VBox/Log.h>

/* The environment information for services. */
typedef struct _VBOXSERVICEENV
{
    HINSTANCE hInstance;
    HANDLE    hDriver;
    HANDLE    hStopEvent;
} VBOXSERVICEENV;

/* The service initialization info and runtime variables. */
typedef struct _VBOXSERVICEINFO
{
    char     *pszName;
    int      (* pfnInit)             (const VBOXSERVICEENV *pEnv, void **ppInstance, bool *pfStartThread);
    unsigned (__stdcall * pfnThread) (void *pInstance);
    void     (* pfnDestroy)          (const VBOXSERVICEENV *pEnv, void *pInstance);

    /* Variables. */
    HANDLE hThread;
    void  *pInstance;
    bool   fStarted;

} VBOXSERVICEINFO;


extern HWND  gToolWindow;

#endif /* !___VBOXSERVICE_H */

