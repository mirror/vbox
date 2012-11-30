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


#ifndef Graphics_DevVGASavedState_h
#define Graphics_DevVGASavedState_h

#define VGA_SAVEDSTATE_VERSION              9
#define VGA_SAVEDSTATE_VERSION_INV_GCMDFIFO 8 /* <- states upto and including this version may contain invalid completed Guest Commands fifo entries */
#define VGA_SAVEDSTATE_VERSION_INV_VHEIGHT  8 /* <- states upto and including this version may contain invalid vbe_regs[VBE_DISPI_INDEX_VIRT_HEIGHT] value */
#define VGA_SAVEDSTATE_VERSION_WDDM         7
#define VGA_SAVEDSTATE_VERSION_PRE_WDDM     6
#define VGA_SAVEDSTATE_VERSION_HOST_HEAP    5
#define VGA_SAVEDSTATE_VERSION_WITH_CONFIG  4
#define VGA_SAVEDSTATE_VERSION_HGSMI        3
#define VGA_SAVEDSTATE_VERSION_PRE_HGSMI    2
#define VGA_SAVEDSTATE_VERSION_ANCIENT      1

#endif

