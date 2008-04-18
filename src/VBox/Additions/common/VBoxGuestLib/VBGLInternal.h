/** @file
 *
 * VBoxGuestLib - A support library for VirtualBox guest additions:
 * Internal header
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

#ifndef __VBGLINTERNAL__H
#define __VBGLINTERNAL__H

/* I have added this include here as
   a) This file is always included before VBGLInternal and
   b) It contains a definition for VBGLHGCMHANDLE, so we definitely do not
      need to redefine that here.  The C (without ++) compiler was complaining
      that it was defined twice.
*/
#include <VBox/VBoxGuestLib.h>

#if (defined(DEBUG) && !defined(NO_LOGGING)) || defined(LOG_ENABLED)
#include <VBox/log.h>
# define dprintf(a) RTLogBackdoorPrintf a
#else
# define dprintf(a) do {} while (0)
#endif

#include "SysHlp.h"

#pragma pack(4)

struct _VBGLPHYSHEAPBLOCK;
typedef struct _VBGLPHYSHEAPBLOCK VBGLPHYSHEAPBLOCK;
struct _VBGLPHYSHEAPCHUNK;
typedef struct _VBGLPHYSHEAPCHUNK VBGLPHYSHEAPCHUNK;

#ifndef VBGL_VBOXGUEST
struct VBGLHGCMHANDLEDATA
{
    uint32_t fAllocated;
    VBGLDRIVER driver;
};
#endif

enum VbglLibStatus
{
    VbglStatusNotInitialized = 0,
    VbglStatusInitializing,
    VbglStatusReady
};

typedef struct _VBGLDATA
{
    enum VbglLibStatus status;

    VBGLIOPORT portVMMDev;

    VMMDevMemory *pVMMDevMemory;

    /**
     * Physical memory heap data.
     * @{
     */

    VBGLPHYSHEAPBLOCK *pFreeBlocksHead;
    VBGLPHYSHEAPBLOCK *pAllocBlocksHead;
    VBGLPHYSHEAPCHUNK *pChunkHead;

    RTSEMFASTMUTEX mutexHeap;
    /** @} */

#ifndef VBGL_VBOXGUEST
    /**
     * Fast heap for HGCM handles data.
     * @{
     */

    RTSEMFASTMUTEX mutexHGCMHandle;

    struct VBGLHGCMHANDLEDATA aHGCMHandleData[64];

    /** @} */
#endif
} VBGLDATA;

#pragma pack()

#ifndef VBGL_DECL_DATA
extern VBGLDATA g_vbgldata;
#endif

/* Check if library has been initialized before entering
 * a public library function.
 */
int VbglEnter (void);

#ifdef VBOX_HGCM
#ifndef VBGL_VBOXGUEST
/* Initialize HGCM subsystem. */
int vbglHGCMInit (void);
/* Terminate HGCM subsystem. */
int vbglHGCMTerminate (void);
#endif
#endif

#endif /* __VBGLINTERNAL__H */
