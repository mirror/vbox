/* $Id$ */
/** @file
 * USBLIB - Internal header.
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
 *
 * Oracle Corporation confidential
 * All rights reserved
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

