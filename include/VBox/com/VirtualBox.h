/** @file
 * MS COM / XPCOM Abstraction Layer:
 * VirtualBox COM Library definitions.
 *
 * Note: This is the main header file that COM/XPCOM clients
 * include; however, it is only a wrapper around another
 * platform-dependent include file that contains the real
 * COM/XPCOM interface declarations. That other include file
 * is generated automatically at build time from
 * /src/VBox/Main/idl/VirtualBox.xidl, which contains all
 * the VirtualBox interfaces; the include file is called
 * VirtualBox.h on Windows hosts and VirtualBox_XPCOM.h
 * on Linux hosts. The build process places it in
 * out/<platform>/bin/sdk/include, from where it gets
 * included by the rest of the VirtualBox code.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBox_com_VirtualBox_h
#define ___VBox_com_VirtualBox_h

// generated VirtualBox COM library definition file
#if !defined (VBOXCOM_NOINCLUDE)
#if defined (RT_OS_WINDOWS)
#include <VirtualBox.h>
#else
#include <VirtualBox_XPCOM.h>
#endif
#endif // !defined (VBOXCOM_NOINCLUDE)

// for convenience
#include "VBox/com/defs.h"

#endif
