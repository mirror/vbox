/* $Id$ */
/** @file
 * VBoxSF - Darwin Shared Folders, mount interface.
 */

/*
 * Copyright (C) 2013-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBoxSFMount_h___
#define ___VBoxSFMount_h___

#include <iprt/types.h>

/** The shared folders file system name.   */
#define VBOXSF_DARWIN_FS_NAME "vboxsf"

/**
 * Mount information that gets passed from userland on mount.
 */
typedef struct VBOXSFDRWNMOUNTINFO
{
    /** Magic value (VBOXSFDRWNMOUNTINFO_MAGIC).   */
    uint32_t    u32Magic;
    /** The shared folder name.   */
    char        szFolder[260];
} VBOXSFDRWNMOUNTINFO;
typedef VBOXSFDRWNMOUNTINFO *PVBOXSFDRWNMOUNTINFO;
/** Magic value for VBOXSFDRWNMOUNTINFO::u32Magic.   */
#define VBOXSFDRWNMOUNTINFO_MAGIC     UINT32_C(0xc001cafe)

#endif

