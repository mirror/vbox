/* $Id$ */

/** @file
 *
 * VirtualBox Windows NT/2000/XP guest OpenGL hgcm related
 *
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

#ifndef __VBOXOGL_HGCM_H__
#define __VBOXOGL_HGCM_H__

#include <windows.h>
#include <iprt/cdefs.h>
#include <iprt/assert.h>

typedef struct
{
    HANDLE      hGuestDrv;

} VBOX_OGL_CTX, *PVBOX_OGL_CTX;

typedef struct
{
    uint32_t        u32ClientID;
} VBOX_OGL_THREAD_CTX, *PVBOX_OGL_THREAD_CTX;

/**
 * Initialize the OpenGL guest-host communication channel
 *
 * @return success or failure (boolean)
 */
BOOL VBoxOGLInit();

/**
 * Destroy the OpenGL guest-host communication channel
 *
 * @return success or failure (boolean)
 */
BOOL VBoxOGLExit();

/**
 * Initialize new thread
 *
 * @return success or failure (boolean)
 */
BOOL VBoxOGLThreadAttach();

/**
 * Clean up for terminating thread
 *
 * @return success or failure (boolean)
 */
BOOL VBoxOGLThreadDetach();

/**
 * Set the thread local OpenGL context
 *
 * @param   pCtx        thread local OpenGL context ptr
 */
void VBoxOGLSetThreadCtx(PVBOX_OGL_THREAD_CTX pCtx);

/**
 * Return the thread local OpenGL context
 *
 * @return thread local OpenGL context ptr or NULL if failure
 */
PVBOX_OGL_THREAD_CTX VBoxOGLGetThreadCtx();

#ifdef DEBUG
#define glLogError(a)               \
    {                               \
        /** @todo log error */      \
        glSetError(a);              \
    }
#define DbgPrintf(a)        VBoxDbgLog a

#ifdef VBOX_DEBUG_LVL2
#define DbgPrintf2(a)       VBoxDbgLog a
#else
#define DbgPrintf2(a)
#endif

#else
#define glLogError(a)       glSetError(a)
#define DbgPrintf(a)
#define DbgPrintf2(a)
#endif

#ifdef DEBUG
/**
 * Log to the debug output device
 *
 * @param   pszFormat   Format string
 * @param   ...         Variable parameters
 */
void VBoxDbgLog(char *pszFormat, ...);
#endif

#endif /* __VBOXOGL_HGCM_H__ */
