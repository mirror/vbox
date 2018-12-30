/* $Id$ */
/** @file
 * VBoxSF - Darwin Shared Folders, internal header.
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

#ifndef ___VBoxSFInternal_h___
#define ___VBoxSFInternal_h___
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "VBoxSFMount.h"

#include <libkern/libkern.h>
#include <iprt/types.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <mach/mach_port.h>
#include <mach/kmod.h>
#include <mach/mach_types.h>
#include <sys/errno.h>
#include <sys/dirent.h>
#include <sys/lock.h>
#include <sys/fcntl.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/vnode.h>
#include <vfs/vfs_support.h>
#undef PVM

#include <iprt/mem.h>
#include <VBox/VBoxGuest.h>
#include <VBox/VBoxGuestLibSharedFolders.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Private data we associate with a mount.
 */
typedef struct VBOXSFMNTDATA
{
    /** The shared folder mapping */
    VBGLSFMAP           hHostFolder;
    /** The root VNode. */
    vnode_t             pVnRoot;
    /** User that mounted shared folder (anyone but root?). */
    uid_t               uidMounter;
    /** The mount info from the mount() call. */
    VBOXSFDRWNMOUNTINFO MntInfo;
} VBOXSFMNTDATA;
/** Pointer to private mount data.  */
typedef VBOXSFMNTDATA *PVBOXSFMNTDATA;

/**
 * Private data we associate with a VNode.
 */
typedef struct VBOXSFDWNVNDATA
{
    /** The handle to the host object.  */
    SHFLHANDLE      hHandle;
    ///PSHFLSTRING     pPath;                  /** Path within shared folder */
    ///lck_attr_t     *pLockAttr;              /** BSD locking stuff */
    ///lck_rw_t       *pLock;                  /** BSD locking stuff */
} VBOXSFDWNVNDATA;
/** Pointer to private vnode data. */
typedef VBOXSFDWNVNDATA *PVBOXSFDWNVNDATA;



/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
extern VBGLSFCLIENT         g_SfClientDarwin;
extern uint32_t volatile    g_cVBoxSfMounts;
extern struct vfsops        g_VBoxSfVfsOps;
extern struct vnodeopv_desc g_VBoxSfVnodeOpvDesc;
extern int (**g_papfnVBoxSfDwnVnDirOpsVector)(void *);



/*********************************************************************************************************************************
*   Functions                                                                                                                    *
*********************************************************************************************************************************/
bool    vboxSfDwnConnect(void);
vnode_t vboxSfDwnVnAlloc(mount_t pMount, enum vtype enmType, vnode_t pParent, uint64_t cbFile);


#endif

