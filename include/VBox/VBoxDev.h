/** @file
 * Virtual Device for Guest <-> VMM/Host communication
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

#ifndef ___VBox_VBoxDev_h
#define ___VBox_VBoxDev_h

#include <VBox/cdefs.h>

__BEGIN_DECLS

/** Mouse capability bits
 * @{ */
/** the guest requests absolute mouse coordinates (guest additions installed) */
#define VMMDEV_MOUSEGUESTWANTSABS                           RT_BIT(0)
/** the host wants to send absolute mouse coordinates (input not captured) */
#define VMMDEV_MOUSEHOSTWANTSABS                            RT_BIT(1)
/** the guest needs a hardware cursor on host. When guest additions are installed
 *  and the host has promised to display the cursor itself, the guest installs a
 *  hardware mouse driver. Don't ask the guest to switch to a software cursor then. */
#define VMMDEV_MOUSEGUESTNEEDSHOSTCUR                       RT_BIT(2)
/** the host is NOT able to draw the cursor itself (e.g. L4 console) */
#define VMMDEV_MOUSEHOSTCANNOTHWPOINTER                     RT_BIT(3)
/** @} */

/** Flags for pfnSetCredentials
 * @{ */
/** the guest should perform a logon with the credentials */
#define VMMDEV_SETCREDENTIALS_GUESTLOGON                    RT_BIT(0)
/** the guest should prevent local logons */
#define VMMDEV_SETCREDENTIALS_NOLOCALLOGON                  RT_BIT(1)
/** the guest should verify the credentials */
#define VMMDEV_SETCREDENTIALS_JUDGE                         RT_BIT(15)
/** @} */

/** Guest capability bits
 * @{ */
/** the guest supports seamless display rendering */
#define VMMDEV_GUEST_SUPPORTS_SEAMLESS                      RT_BIT(0)
/** the guest supports mapping guest to host windows */
#define VMMDEV_GUEST_SUPPORTS_GUEST_HOST_WINDOW_MAPPING     RT_BIT(1)
/** @} */

/** Size of VMMDev RAM region accessible by guest.
 *  Must be big enough to contain VMMDevMemory structure (see VBoxGuest.h)
 *  For now: 4 megabyte.
 */
#define VMMDEV_RAM_SIZE (4 * 256 * PAGE_SIZE)

__END_DECLS

#endif
