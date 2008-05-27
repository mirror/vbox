/* $Id$ */
/** @file
 * VirtualBox File System Driver for Solaris Guests.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
 *
 * Sun Microsystems, Inc. confidential
 * All rights reserved
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/uio.h>
#include <sys/cred.h>
#include <sys/vnode.h>
#include "vboxvfs.h"

static int VBoxVFS_Open(vnode_t **vpp, int flag, struct cred *cr, caller_context_t *ct)
{
}

static int VBoxVFS_Close(vnode_t *vp, int flag, int count, offset_t offset, struct cred *cr, caller_context_t *ct)
{
}

static int VBoxVFS_Read(vnode_t *vp, struct uio *uiop, int ioflag, struct cred *cr, caller_context_t *ct)
{
}

static int VBoxVFS_Write(vnode_t *vp, struct uio *uiop, int ioflag, struct cred *cr, caller_context_t *ct)
{
}

static int VBoxVFS_IOCtl(vnode_t *vp, int cmd, intptr_t arg, int flag, struct cred *cr, int *rvalp, caller_context_t *ct)
{
}

static int VBoxVFS_Setfl(vnode_t *vp, int oflags, int nflags, cred_t *cr, caller_context_t *ct)
{
}

static int VBoxVFS_Getattr(vnode_t *vp, struct vattr *vap, int flags, struct cred *cr, caller_context_t *ct)
{
}

static int VBoxVFS_Setattr(vnode_t *vp, struct vattr *vap, int flags, struct cred *cr, caller_context_t *ct)
{
}

static int VBoxVFS_Access(vnode_t *vp, int mode, int flags, struct cred *cr, caller_context_t *ct)
{
}

static int VBoxVFS_Fsync(vnode_t *vp, int syncflag, struct cred *cr, caller_context_t *ct)
{
}

static void VBoxVFS_Inactive(vnode_t *vp, struct cred *cr, caller_context_t *ct)
{
}

static int VBoxVFS_Fid(vnode_t *vp, struct fid *fidp, caller_context_t *ct)
{
}

static int VBoxVFS_Lookup(vnode_t *dvp, char *nm, vnode_t **vpp, struct pathname *pnp, int flags, vnode_t *rdir,
                struct cred *cr, caller_context_t *ct, int *direntflags, pathname_t *realpnp)
{
}

static int VBoxVFS_Create(vnode_t *dvp, char *nm, struct vattr *va, enum vcexcl exclusive, int mode, vnode_t **vpp,
                struct cred *cr, int flag, caller_context_t *ct, vsecattr_t *vsecp)
{
}

static int VBoxVFS_Remove(vnode_t *dvp, char *nm, struct cred *cr, caller_context_t *ct, int flags)
{
}

static int VBoxVFS_Link(vnode_t *tdvp, vnode_t *vp, char *tnm, struct cred *cr, caller_context_t *ct, int flags)
{
}

static int VBoxVFS_Rename(vnode_t *odvp, char *onm, vnode_t *ndvp, char *nnm, struct cred *cr, caller_context_t *ct, int flags)
{
}

static int VBoxVFS_Mkdir(vnode_t *dvp, char *nm, struct vattr *va, vnode_t **vpp, struct cred *cr, caller_context_t *ct,
                int flags, vsecattr_t *vsecp)
{
}

static int VBoxVFS_Realvp(vnode_t *vp, vnode_t **vpp, caller_context_t *ct)
{
}

static int VBoxVFS_Rmdir(vnode_t *dvp, char *nm, vnode_t *cdir, struct cred *cr, caller_context_t *ct, int flags)
{
}

static int VBoxVFS_Symlink(vnode_t *dvp, char *lnm, struct vattr *tva, char *tnm, struct cred *cr,
                caller_context_t *ct, int flags)
{
}

static int VBoxVFS_Readlink(vnode_t *vp, struct uio *uiop, struct cred *cr, caller_context_t *ct)
{
}

static int VBoxVFS_Readdir(vnode_t *vp, struct uio *uiop, struct cred *cr, int *eofp, caller_context_t *ct, int flags)
{
}

static int VBoxVFS_Rwlock(vnode_t *vp, int write_lock, caller_context_t *ct)
{
}

static void VBoxVFS_Rwunlock(vnode_t *vp, int write_lock, caller_context_t *ct)
{
}

static int VBoxVFS_Seek(vnode_t *vp, offset_t ooff, offset_t *noffp, caller_context_t *ct)
{
}

static int VBoxVFS_Cmp(vnode_t *vp1, vnode_t *vp2, caller_context_t *ct)
{
}

static int VBoxVFS_Frlock(vnode_t *vp, int cmd, struct flock64 *bfp, int flag, offset_t offset, struct flk_callback *flk_cbp,
                cred_t *cr, caller_context_t *ct)
{
}

static int VBoxVFS_Space(vnode_t *vp, int cmd, struct flock64 *bfp, int flag, offset_t offset, struct cred *cr,
                caller_context_t *ct)
{
}

static int VBoxVFS_Getpage(vnode_t *vp, offset_t off, size_t len, uint_t *prot, struct page *parr[], size_t psz,
                struct seg *seg, caddr_t addr, enum seg_rw rw, struct cred *cr, caller_context_t *ct)
{
}

static int VBoxVFS_Putpage(vnode_t *vp, offset_t off, size_t len, int flags, struct cred *cr, caller_context_t *ct)
{
}

static int VBoxVFS_Map(vnode_t *vp, offset_t off, struct as *as, caddr_t *addrp, size_t len, uchar_t prot,
                uchar_t maxprot, uint_t flags, struct cred *cr, caller_context_t *ct)
{
}

static int VBoxVFS_Addmap(vnode_t *vp, offset_t off, struct as *as, caddr_t addr, size_t len, uchar_t prot,
                uchar_t maxprot, uint_t flags, struct cred *cr, caller_context_t *ct)
{
}

static int VBoxVFS_Delmap(vnode_t *vp, offset_t off, struct as *as, caddr_t addr, size_t len, uint_t prot, uint_t maxprot,
            uint_t flags, struct cred *cr, caller_context_t *ct)
{
}

static int VBoxVFS_Pathconf(vnode_t *vp, int cmd, ulong_t *valp, struct cred *cr, caller_context_t *ct)
{
}

static int VBoxVFS_Shrlock(vnode_t *vp, int cmd, struct shrlock *shr, int flag, cred_t *cr, caller_context_t *ct)
{
}

/**
 * VBoxVFS: vnode operations vector.
 */
struct vnodeops *g_pVBoxVFS_vnodeops;

const fs_operation_def_t g_VBoxVFS_vnodeops_template[] =
{
    VOPNAME_OPEN,        { .vop_open = VBoxVFS_Open },
    VOPNAME_CLOSE,       { .vop_close = VBoxVFS_Close },
    VOPNAME_READ,        { .vop_read = VBoxVFS_Read },
    VOPNAME_WRITE,       { .vop_write = VBoxVFS_Write },
    VOPNAME_IOCTL,       { .vop_ioctl = VBoxVFS_IOCtl },
    VOPNAME_SETFL,       { .vop_setfl = VBoxVFS_Setfl },
    VOPNAME_GETATTR,     { .vop_getattr = VBoxVFS_Getattr },
    VOPNAME_SETATTR,     { .vop_setattr = VBoxVFS_Setattr },
    VOPNAME_ACCESS,      { .vop_access = VBoxVFS_Access },
    VOPNAME_LOOKUP,      { .vop_lookup = VBoxVFS_Lookup },
    VOPNAME_CREATE,      { .vop_create = VBoxVFS_Create },
    VOPNAME_REMOVE,      { .vop_remove = VBoxVFS_Remove },
    VOPNAME_LINK,        { .vop_link = VBoxVFS_Link },
    VOPNAME_RENAME,      { .vop_rename = VBoxVFS_Rename },
    VOPNAME_MKDIR,       { .vop_mkdir = VBoxVFS_Mkdir },
    VOPNAME_RMDIR,       { .vop_rmdir = VBoxVFS_Rmdir },
    VOPNAME_READDIR,     { .vop_readdir = VBoxVFS_Readdir },
    VOPNAME_SYMLINK,     { .vop_symlink = VBoxVFS_Symlink },
    VOPNAME_READLINK,    { .vop_readlink = VBoxVFS_Readlink },
    VOPNAME_FSYNC,       { .vop_fsync = VBoxVFS_Fsync },
    VOPNAME_INACTIVE,    { .vop_inactive = VBoxVFS_Inactive },
    VOPNAME_FID,         { .vop_fid = VBoxVFS_Fid },
    VOPNAME_RWLOCK,      { .vop_rwlock = VBoxVFS_Rwlock },
    VOPNAME_RWUNLOCK,    { .vop_rwunlock = VBoxVFS_Rwunlock },
    VOPNAME_SEEK,        { .vop_seek = VBoxVFS_Seek },
    VOPNAME_CMP,         { .vop_cmp = VBoxVFS_Cmp },
    VOPNAME_FRLOCK,      { .vop_frlock = VBoxVFS_Frlock },
    VOPNAME_SPACE,       { .vop_space = VBoxVFS_Space },
    VOPNAME_REALVP,      { .vop_realvp = VBoxVFS_Realvp },
    VOPNAME_GETPAGE,     { .vop_getpage = VBoxVFS_Getpage },
    VOPNAME_PUTPAGE,     { .vop_putpage = VBoxVFS_Putpage },
    VOPNAME_MAP,         { .vop_map = VBoxVFS_Map },
    VOPNAME_ADDMAP,      { .vop_addmap = VBoxVFS_Addmap },
    VOPNAME_DELMAP,      { .vop_delmap = VBoxVFS_Delmap },
    VOPNAME_PATHCONF,    { .vop_pathconf = VBoxVFS_Pathconf },
    VOPNAME_SHRLOCK,     { .vop_shrlock = VBoxVFS_Shrlock },
    NULL,                NULL
};

