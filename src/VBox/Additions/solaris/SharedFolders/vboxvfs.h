/* $Id$ */
/** @file
 * VirtualBox File System Driver for Solaris Guests, Internal Header.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
 *
 * Sun Microsystems, Inc. confidential
 * All rights reserved
 */

#ifndef ___VBoxVFS_Solaris_h
#define ___VBoxVFS_Solaris_h

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_HOST_NAME   256
#define MAX_NLS_NAME    32

/* Not sure if we need this; it seems only necessary for kernel mounts. */
#if 0
typedef struct vboxvfs_mountinfo
{
    char name[MAX_HOST_NAME];
    char nls_name[MAX_NLS_NAME];
    int uid;
    int gid;
    int ttl;
} vboxvfs_mountinfo_t;
#endif

#ifdef _KERNEL

#include "../../common/VBoxGuestLib/VBoxCalls.h"
#include <sys/vfs.h>
#include <sys/vfs_opreg.h>

/** Per-file system mount instance data. */
typedef struct vboxvfs_globinfo
{
    VBSFMAP Map;
    int Ttl;
    int Uid;
    int Gid;
    vfs_t *pVFS;
    vnode_t *pVNodeDev;
    vnode_t *pVNodeRoot;
} vboxvfs_globinfo_t;

extern struct vnodeops *g_pVBoxVFS_vnodeops;
extern const fs_operation_def_t g_VBoxVFS_vnodeops_template[];

#endif /* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif /* ___VBoxVFS_Solaris_h */

