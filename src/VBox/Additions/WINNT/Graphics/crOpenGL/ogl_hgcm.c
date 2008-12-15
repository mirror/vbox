/* $Id$ */

/** @file
 * VBox OpenGL hgcm related functions
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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

#include "ogl_hgcm.h"
#include <VBox/VBoxGuest.h>
#include <stdio.h>

static DWORD dwOGLTlsIndex      = TLS_OUT_OF_INDEXES;
static VBOX_OGL_CTX vboxOGLCtx  = {0};

/**
 * Initialize the OpenGL guest-host communication channel
 *
 * @return success or failure (boolean)
 * @param   hDllInst        Dll instance handle
 */
BOOL VBoxOGLInit()
{
    dwOGLTlsIndex = TlsAlloc();
    if (dwOGLTlsIndex == TLS_OUT_OF_INDEXES)
    {
        DbgPrintf(("TlsAlloc failed with %d\n", GetLastError()));
        return FALSE;
    }
    DbgPrintf(("VBoxOGLInit TLS index=%d\n", dwOGLTlsIndex));

    /* open VBox guest driver */
    vboxOGLCtx.hGuestDrv = CreateFile(VBOXGUEST_DEVICE_NAME,
                                      GENERIC_READ | GENERIC_WRITE,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      NULL,
                                      OPEN_EXISTING,
                                      FILE_ATTRIBUTE_NORMAL,
                                      NULL);
    if (vboxOGLCtx.hGuestDrv == INVALID_HANDLE_VALUE)
    {
        DbgPrintf(("VBoxService: could not open VBox Guest Additions driver! rc = %d\n", GetLastError()));
        return FALSE;
    }

    VBoxOGLThreadAttach();
    return TRUE;
}

/**
 * Destroy the OpenGL guest-host communication channel
 *
 * @return success or failure (boolean)
 */
BOOL VBoxOGLExit()
{
    DbgPrintf(("VBoxOGLExit\n"));
    VBoxOGLThreadDetach();
    CloseHandle(vboxOGLCtx.hGuestDrv);

    return TRUE;
}

/**
 * Initialize new thread
 *
 * @return success or failure (boolean)
 */
BOOL VBoxOGLThreadAttach()
{
    PVBOX_OGL_THREAD_CTX pCtx;
    VBoxGuestHGCMConnectInfo info;
    DWORD cbReturned;

    DbgPrintf(("VBoxOGLThreadAttach id=%x\n", GetCurrentThreadId()));

    pCtx = (PVBOX_OGL_THREAD_CTX)malloc(sizeof(*pCtx));
    AssertReturn(pCtx, FALSE);
    if (!pCtx)
        return FALSE;

    VBoxOGLSetThreadCtx(pCtx);

    memset (&info, 0, sizeof (info));

    info.Loc.type = VMMDevHGCMLoc_LocalHost_Existing;

    strcpy (info.Loc.u.host.achName, "VBoxSharedCrOpenGL");

    if (DeviceIoControl(vboxOGLCtx.hGuestDrv,
                        VBOXGUEST_IOCTL_HGCM_CONNECT,
                        &info, sizeof (info),
                        &info, sizeof (info),
                        &cbReturned,
                        NULL))
    {
        if (info.result == VINF_SUCCESS)
        {
            pCtx->u32ClientID = info.u32ClientID;
            DbgPrintf(("HGCM connect was successful: client id =%x\n", pCtx->u32ClientID));
        }
        else DbgPrintf(("HGCM connect failed with rc=%x\n", info.result));
    }
    else
    {
        DbgPrintf(("HGCM connect failed with rc=%x\n", GetLastError()));
        return FALSE;
    }

    return TRUE;
}

/**
 * Clean up for terminating thread
 *
 * @return success or failure (boolean)
 */
BOOL VBoxOGLThreadDetach()
{
    PVBOX_OGL_THREAD_CTX pCtx;
    VBoxGuestHGCMDisconnectInfo info;
    DWORD cbReturned;
    BOOL bRet;

    DbgPrintf(("VBoxOGLThreadDetach id=%x\n", GetCurrentThreadId()));

    pCtx = VBoxOGLGetThreadCtx();
    if (pCtx && pCtx->u32ClientID)
    {        
        memset (&info, 0, sizeof (info));

        info.u32ClientID = pCtx->u32ClientID;

        bRet = DeviceIoControl(vboxOGLCtx.hGuestDrv,
                                    VBOXGUEST_IOCTL_HGCM_DISCONNECT,
                                    &info, sizeof (info),
                                    &info, sizeof (info),
                                    &cbReturned,
                                    NULL);

        if (!bRet)
        {
            DbgPrintf(("Disconnect failed with %x\n", GetLastError()));
        }
    }
    if (pCtx)
    {
        free(pCtx);
        VBoxOGLSetThreadCtx(NULL);
    }

    return TRUE;
}

/**
 * Set the thread local OpenGL context
 *
 * @param   pCtx        thread local OpenGL context ptr
 */
void VBoxOGLSetThreadCtx(PVBOX_OGL_THREAD_CTX pCtx)
{
    BOOL ret;

    ret = TlsSetValue(dwOGLTlsIndex, pCtx);
    Assert(ret);
}


/**
 * Return the thread local OpenGL context
 *
 * @return thread local OpenGL context ptr or NULL if failure
 */
PVBOX_OGL_THREAD_CTX VBoxOGLGetThreadCtx()
{
    PVBOX_OGL_THREAD_CTX pCtx;

    pCtx = (PVBOX_OGL_THREAD_CTX)TlsGetValue(dwOGLTlsIndex);
    if (!pCtx)
    {
        /* lazy init */
        VBoxOGLThreadAttach();
        pCtx = (PVBOX_OGL_THREAD_CTX)TlsGetValue(dwOGLTlsIndex);
    }
    Assert(pCtx);
    return pCtx;
}

#ifdef DEBUG
/**
 * Log to the debug output device
 *
 * @param   pszFormat   Format string
 * @param   ...         Variable parameters
 */
void VBoxDbgLog(char *pszFormat, ...)
{
   va_list va;
   CHAR  Buffer[10240];

   va_start(va, pszFormat);
   
   if (strlen(pszFormat) < 512)
   {
      vsprintf (Buffer, pszFormat, va);

      printf(Buffer);
//      OutputDebugStringA(Buffer);
   }

   va_end (va);
}
#endif
