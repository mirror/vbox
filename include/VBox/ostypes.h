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
 * Global list of guest operating system types.
 *
 * They are grouped into families. A family identifer is always has
 * mod 0x10000 == 0. New entries can be added, however other components
 * depend on the values (e.g. the Qt GUI and guest additions) so the
 * existing values MUST stay the same.
 */
typedef enum VBOXOSTYPE
{
    VBOXOSTYPE_Unknown   = 0,
    VBOXOSTYPE_DOS       = 0x10000,
    VBOXOSTYPE_Win31     = 0x15000,
    VBOXOSTYPE_Win9x     = 0x20000,
    VBOXOSTYPE_Win95     = 0x21000,
    VBOXOSTYPE_Win98     = 0x22000,
    VBOXOSTYPE_WinMe     = 0x23000,
    VBOXOSTYPE_WinNT     = 0x30000,
    VBOXOSTYPE_WinNT4    = 0x31000,
    VBOXOSTYPE_Win2k     = 0x32000,
    VBOXOSTYPE_WinXP     = 0x33000,
    VBOXOSTYPE_Win2k3    = 0x34000,
    VBOXOSTYPE_WinVista  = 0x35000,
    VBOXOSTYPE_OS2       = 0x40000,
    VBOXOSTYPE_OS2Warp3  = 0x41000,
    VBOXOSTYPE_OS2Warp4  = 0x42000,
    VBOXOSTYPE_OS2Warp45 = 0x43000,
    VBOXOSTYPE_Linux     = 0x50000,
    VBOXOSTYPE_Linux22   = 0x51000,
    VBOXOSTYPE_Linux24   = 0x52000,
    VBOXOSTYPE_Linux26   = 0x53000,
    VBOXOSTYPE_FreeBSD   = 0x60000,
    VBOXOSTYPE_OpenBSD   = 0x61000,
    VBOXOSTYPE_NetBSD    = 0x62000,
    VBOXOSTYPE_Netware   = 0x70000,
    VBOXOSTYPE_Solaris   = 0x80000,
    VBOXOSTYPE_L4        = 0x90000,
    /** The usual 32-bit hack. */
    VBOXOSTYPE_32BIT_HACK = 0x7fffffff
} VBOXOSTYPE;

__END_DECLS

#endif

