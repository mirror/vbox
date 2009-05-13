/** @file
 * Virtual Device for Guest <-> VMM/Host communication
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
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
/** The guest can read VMMDev events to find out about pointer movement */
#define VMMDEV_MOUSEGUESTUSESVMMDEV                         RT_BIT(4)
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
/** the guest graphical additions are active - used for fast activation
 *  and deactivation of certain graphical operations (e.g. resizing & seamless).
 *  The legacy VMMDevReq_ReportGuestCapabilities request sets this
 *  automatically, but VMMDevReq_SetGuestCapabilities does not. */
#define VMMDEV_GUEST_SUPPORTS_GRAPHICS                      RT_BIT(2)
/** @} */

/** Size of VMMDev RAM region accessible by guest.
 *  Must be big enough to contain VMMDevMemory structure (see VBoxGuest.h)
 *  For now: 4 megabyte.
 */
#define VMMDEV_RAM_SIZE (4 * 256 * PAGE_SIZE)

/** Size of VMMDev heap region accessible by guest.
 *  (must be a power of two (pci range))
 */
#define VMMDEV_HEAP_SIZE (4*PAGE_SIZE)

__END_DECLS

#endif
