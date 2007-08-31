/** @file
 *
 * VBoxGuestInternal -- Private windows additions declarations
 *
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * innotek GmbH confidential
 * All rights reserved
 */
#ifndef __VBoxGuestInternal_h__
#define __VBoxGuestInternal_h__

/** Uncomment to enable VRDP status checks */
#define VBOX_WITH_VRDP_SESSION_HANDLING

/** IOCTL for VBoxGuest to enable a VRDP session */
#define IOCTL_VBOXGUEST_ENABLE_VRDP_SESSION     IOCTL_CODE(FILE_DEVICE_UNKNOWN, 2100, METHOD_BUFFERED, FILE_WRITE_ACCESS, 0)

/** IOCTL for VBoxGuest to disable a VRDP session */
#define IOCTL_VBOXGUEST_DISABLE_VRDP_SESSION    IOCTL_CODE(FILE_DEVICE_UNKNOWN, 2101, METHOD_BUFFERED, FILE_WRITE_ACCESS, 0)


#endif /* __VBoxGuestInternal_h__ */
