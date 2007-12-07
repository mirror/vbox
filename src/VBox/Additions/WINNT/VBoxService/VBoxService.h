/** @file
 *
 * VBoxService - Guest Additions Service
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
 
#ifndef __VBOXSERVICE__H
#define __VBOXSERVICE__H

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <stdarg.h>
#include <process.h>

#include <VBox/VBoxGuest.h>
#include <VBox/version.h>


#define WM_VBOX_RESTORED                     0x2005
#define WM_VBOX_CHECK_VRDP                   0x2006


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

#endif /* __VBOXSERVICE__H */
