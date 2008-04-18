/** @file
 *
 * VBoxGuestInternal -- Private windows additions declarations
 *
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
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */
#ifndef __VBoxGuestInternal_h__
#define __VBoxGuestInternal_h__

/** Uncomment to enable VRDP status checks */
//#define VBOX_WITH_VRDP_SESSION_HANDLING

/** Uncomment to enable the guest management extension in VBoxService */
#define VBOX_WITH_MANAGEMENT

/** IOCTL for VBoxGuest to enable a VRDP session */
#define IOCTL_VBOXGUEST_ENABLE_VRDP_SESSION     IOCTL_CODE(FILE_DEVICE_UNKNOWN, 2100, METHOD_BUFFERED, FILE_WRITE_ACCESS, 0)

/** IOCTL for VBoxGuest to disable a VRDP session */
#define IOCTL_VBOXGUEST_DISABLE_VRDP_SESSION    IOCTL_CODE(FILE_DEVICE_UNKNOWN, 2101, METHOD_BUFFERED, FILE_WRITE_ACCESS, 0)


#endif /* __VBoxGuestInternal_h__ */
