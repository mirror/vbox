/* $Id$ */
/** @file
 * USBLIB - Internal header.
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___USBLibInternal_h
#define ___USBLibInternal_h

#include <VBox/cdefs.h>
#include <VBox/types.h>

RT_C_DECLS_BEGIN
#ifdef RT_OS_WINDOWS
char  *usblibQueryDeviceName(uint32_t idxDev);
char  *usblibQueryDeviceRegPath(uint32_t idxDev);
void   usbLibEnumerateHostControllers(PUSBDEVICE *ppDevices,  uint32_t *DevicesConnected);
#endif /* RT_OS_WINDOWS */
RT_C_DECLS_END

#endif

