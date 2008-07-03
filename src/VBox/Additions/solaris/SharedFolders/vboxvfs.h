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

/** The module name. */
#define DEVICE_NAME              "vboxvfs"
/** The module description as seen in 'modinfo'. */
#define DEVICE_DESC              "filesystem for VirtualBox Shared Folders"

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

/** VNode for VBoxVFS */
typedef struct vboxvfs_vnode
{
    vnode_t     *pVNode;
    vattr_t     Attr;
    SHFLSTRING  *pPath;
    kmutex_t    MtxContents;
} vboxvfs_vnode_t;


/** Per-file system mount instance data. */
typedef struct vboxvfs_globinfo
{
    VBSFMAP         Map;
    int             Ttl;
    int             Uid;
    int             Gid;
    vfs_t           *pVFS;
    vnode_t         *pVNodeDev;
    vboxvfs_vnode_t *pVNodeRoot;
    kmutex_t        MtxFS;
} vboxvfs_globinfo_t;

extern struct vnodeops *g_pVBoxVFS_vnodeops;
extern const fs_operation_def_t g_VBoxVFS_vnodeops_template[];
extern VBSFCLIENT g_VBoxVFSClient;

/** Helper functions */
extern int vboxvfs_Stat(const char *pszCaller, vboxvfs_globinfo_t *pVBoxVFSGlobalInfo, SHFLSTRING *pPath,
            RTFSOBJINFO *pResult, boolean_t fAllowFailure);
extern void vboxvfs_InitVNode(vboxvfs_globinfo_t *pVBoxVFSGlobalInfo, vboxvfs_vnode_t *pVBoxVNode,
            RTFSOBJINFO *pFSInfo);


/** Helper macros */
#define VFS_TO_VBOXVFS(vfs)      ((vboxvfs_globinfo_t *)((vfs)->vfs_data))
#define VBOXVFS_TO_VFS(vboxvfs)  ((vboxvfs)->pVFS)
#define VN_TO_VBOXVN(vnode)      ((vboxvfs_vnode_t *)((vnode)->v_data))
#define VBOXVN_TO_VN(vboxvnode)  ((vboxvnode)->pVNode)

#endif /* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif /* ___VBoxVFS_Solaris_h */

