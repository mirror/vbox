/** @file
 *
 * VBoxGuest -- VirtualBox Win 2000/XP guest video driver
 *
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

#ifndef __HELPER_h__
#define __HELPER_h__

// Windows version identifier
typedef enum
{
    UNKNOWN_WINVERSION = 0,
    WINNT4 = 1,
    WIN2K  = 2,
    WINXP  = 3
} winVersion_t;


extern "C"
{
BOOLEAN vboxQueryDisplayRequest(uint32_t *xres, uint32_t *yres, uint32_t *bpp);
BOOLEAN vboxLikesVideoMode(uint32_t width, uint32_t height, uint32_t bpp);
ULONG vboxGetHeightReduction();
BOOLEAN vboxQueryPointerPos(uint16_t *pointerXPos, uint16_t *pointerYPos);
BOOLEAN vboxQueryHostWantsAbsolute();
winVersion_t vboxQueryWinVersion();
BOOLEAN vboxUpdatePointerShape(PVIDEO_POINTER_ATTRIBUTES pointerAttr, uint32_t cbLength);

#include "vboxioctl.h"

int vboxVbvaEnable (ULONG ulEnable, VBVAENABLERESULT *pVbvaResult);
}


/* debug printf */
# define OSDBGPRINT(a) DbgPrint a

/* dprintf */
#if (defined(DEBUG) && !defined(NO_LOGGING)) || defined(LOG_ENABLED)
# ifdef LOG_TO_BACKDOOR
#  include <VBox/log.h>
#  define dprintf(a) RTLogBackdoorPrintf a
# else
#  define dprintf(a) OSDBGPRINT(a)
# endif
#else
# define dprintf(a) do {} while (0)
#endif

/* dprintf2 - extended logging. */
#if 0
# define dprintf2 dprintf
#else
# define dprintf2(a) do { } while (0)
#endif


#endif // __HELPER_h__
