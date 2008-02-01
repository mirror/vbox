/** @file
 * vboxvfs -- VirtualBox Guest Additions for Linux: mount(2) parameter structure.
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

#ifndef VBFS_MOUNT_H
#define VBFS_MOUNT_H

#define MAX_HOST_NAME 256
#define MAX_NLS_NAME 32

/* Linux constraints the size of data mount argument to PAGE_SIZE - 1 */
struct vbsf_mount_info {
    char name[MAX_HOST_NAME];
    char nls_name[MAX_NLS_NAME];
    int uid;
    int gid;
    int ttl;
};

#endif /* vbsfmount.h */
