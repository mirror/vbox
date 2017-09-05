/** @file
 * VBoxGuest - VirtualBox Guest Additions, Core Types.
 *
 * This contains types that are used both in the VBoxGuest I/O control interface
 * and the VBoxGuestLib.  The goal is to avoid having to include VBoxGuest.h
 * everwhere VBoxGuestLib.h is used.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


#ifndef ___VBoxGuestCoreTypes_h
#define ___VBoxGuestCoreTypes_h

#include <iprt/types.h>

/** @addtogroup grp_vboxguest
 * @{ */

/**
 * Mouse event noticification callback function.
 * @param   pvUser      Argument given when setting the callback.
 */
typedef DECLCALLBACK(void) FNVBOXGUESTMOUSENOTIFY(void *pvUser);
/** Pointer to a mouse event notification callback function. */
typedef FNVBOXGUESTMOUSENOTIFY *PFNVBOXGUESTMOUSENOTIFY; /**< @todo fix type prefix */

/** @} */

#endif

