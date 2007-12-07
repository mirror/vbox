/** @file
 * VirtualBox - Global Guest Operating System definition.
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

#ifndef ___VBox_ostypes_h
#define ___VBox_ostypes_h

#include <iprt/cdefs.h>

__BEGIN_DECLS

/**
 * Global list of guest operating system types. They are grouped
 * into families. A family identifer is always has mod 0x10000 == 0.
 * New entries can be added, however other components might depend
 * on the values (e.g. the Qt GUI) so if possible, the values should
 * stay the same.
 * 
 * @todo This typedef crashes with a core Mac OS X typedef, please rename it.
 */
typedef enum
{
    OSTypeUnknown   = 0,
    OSTypeDOS       = 0x10000,
    OSTypeWin31     = 0x15000,
    OSTypeWin9x     = 0x20000,
    OSTypeWin95     = 0x21000,
    OSTypeWin98     = 0x22000,
    OSTypeWinMe     = 0x23000,
    OSTypeWinNT     = 0x30000,
    OSTypeWinNT4    = 0x31000,
    OSTypeWin2k     = 0x32000,
    OSTypeWinXP     = 0x33000,
    OSTypeWin2k3    = 0x34000,
    OSTypeWinVista  = 0x35000,
    OSTypeOS2       = 0x40000,
    OSTypeOS2Warp3  = 0x41000,
    OSTypeOS2Warp4  = 0x42000,
    OSTypeOS2Warp45 = 0x43000,
    OSTypeLinux     = 0x50000,
    OSTypeLinux22   = 0x51000,
    OSTypeLinux24   = 0x52000,
    OSTypeLinux26   = 0x53000,
    OSTypeFreeBSD   = 0x60000,
    OSTypeOpenBSD   = 0x61000,
    OSTypeNetBSD    = 0x62000,
    OSTypeNetware   = 0x70000,
    OSTypeSolaris   = 0x80000,
    OSTypeL4        = 0x90000
} OSType, VBOXOSTYPE;

__END_DECLS

#endif

