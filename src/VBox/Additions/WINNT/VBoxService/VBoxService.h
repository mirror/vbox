/* $Id$ */
/** @file
 * VBoxService - Guest Additions Service
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
#include <iprt/assert.h>
#include <iprt/initterm.h>
#include <iprt/log.h>
#include <iprt/string.h>
#include <iprt/time.h>

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

