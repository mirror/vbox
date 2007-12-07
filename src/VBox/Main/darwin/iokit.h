/* $Id$ */
/** @file
 * Main - Darwin IOKit Routines.
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
 */

#ifndef ___darwin_iokit_h___
#define ___darwin_iokit_h___

#include <iprt/cdefs.h>
#include <iprt/types.h>
#ifdef VBOX_WITH_USB
# include <VBox/usb.h>
#endif

/**
 * Darwin DVD descriptor as returned by DarwinGetDVDDrives().
 */
typedef struct DARWINDVD
{
    /** Pointer to the next DVD. */
    struct DARWINDVD *pNext;
    /** Variable length name / identifier. */
    char szName[1];
} DARWINDVD, *PDARWINDVD;

/** The run loop mode string used by iokit.cpp when it registers
 * notifications events. */
#define VBOX_IOKIT_MODE_STRING "VBoxIOKitMode"

__BEGIN_DECLS
PDARWINDVD  DarwinGetDVDDrives(void);
#ifdef VBOX_WITH_USB
void *      DarwinSubscribeUSBNotifications(void);
void        DarwinUnsubscribeUSBNotifications(void *pvOpaque);
PUSBDEVICE  DarwinGetUSBDevices(void);
void        DarwinFreeUSBDeviceFromIOKit(PUSBDEVICE pCur);
int         DarwinReEnumerateUSBDevice(PCUSBDEVICE pCur);
#endif
__END_DECLS

#endif

