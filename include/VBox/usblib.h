/** @file
 * USBLIB - USB Support Library.
 * This module implements the basic low-level OS interfaces.
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

#ifndef ___VBox_usblib_h
#define ___VBox_usblib_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/usb.h>

#ifdef RT_OS_WINDOWS
# include <VBox/usblib-win.h>
#endif
/** @todo merge the usblib-win.h interface into the darwin and linux ports where suitable. */

/** @defgroup grp_USBLib    USBLib - USB Support Library
 * This module implements the basic low-level OS interfaces and common USB code.
 * @{
 */

/** @} */
#endif

