/** @file
 * VBox frontends: Qt GUI ("VirtualBox"):
 * Declaration of OS/2-specific helpers that require to reside in a DLL
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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

#ifndef ___VBoxHlp_h
#define ___VBoxHlp_h

#include <iprt/cdefs.h>

#ifdef IN_VBOXHLP
# define VBOXHLPDECL(type) DECLEXPORT(type) RTCALL
#else
# define VBOXHLPDECL(type) DECLIMPORT(type) RTCALL
#endif

VBOXHLPDECL(bool) VBoxHlpInstallKbdHook (HAB aHab, HWND aHwnd,
                                           unsigned long aMsg);

VBOXHLPDECL(bool) VBoxHlpUninstallKbdHook (HAB aHab, HWND aHwnd,
                                           unsigned long aMsg);

#endif /* !___VBoxHlp_h */

