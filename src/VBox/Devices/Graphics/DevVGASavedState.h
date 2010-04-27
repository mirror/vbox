/* $Id$ */
/** @file
 * DevVGA - Saved state versions.
 *
 * @remarks HGSMI needs this but doesn't want to deal with DevVGA.h, thus this
 *          dedicated header.
 */

/*
 * Copyright (C) 2006-2009 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


#ifndef ___Graphics_DevVGASavedState_h
#define ___Graphics_DevVGASavedState_h

#define VGA_SAVEDSTATE_VERSION              6
#define VGA_SAVEDSTATE_VERSION_HOST_HEAP    5
#define VGA_SAVEDSTATE_VERSION_WITH_CONFIG  4
#define VGA_SAVEDSTATE_VERSION_HGSMI        3
#define VGA_SAVEDSTATE_VERSION_PRE_HGSMI    2
#define VGA_SAVEDSTATE_VERSION_ANCIENT      1

#endif

