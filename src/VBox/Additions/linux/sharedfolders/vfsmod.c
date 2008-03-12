/** @file
 *
 * vboxvfs -- VirtualBox Guest Additions for Linux:
 * Virtual File System for VirtualBox Shared Folders
 *
 * Module initialization/finalization
 * File system registration/deregistration
 * Superblock reading
 * Few utility functions
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

/**
 * @note Anyone wishing to make changes here might wish to take a look at
 *  http://www.atnf.csiro.au/people/rgooch/linux/vfs.txt
 * which seems to be the closest there is to official documentation on
 * writing filesystem drivers for Linux.
 */

/*
 * Suppress the definition of wchar_t from stddef.h that occurs below.
 * This makes (at least) RHEL3U5 happy.
 */
#if 0
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 0)
# define _WCHAR_T
#endif
#endif

#include "vfsmod.h"

// #define wchar_t linux_wchar_t

#define xstr(s) str(s)
#define str(s) #s

MODULE_DESCRIPTION ("Host file system access VFS for VirtualBox");
MODULE_AUTHOR ("innotek GmbH");
MODULE_LICENSE ("GPL");
#ifdef MODULE_VERSION
MODULE_VERSION(VBOX_VERSION_STRING " (interface " xstr(VMMDEV_VERSION) ")");
#endif

/* globals */
VBSFCLIENT client_handle;

/* forward declarations */
static struct super_operations  sf_super_ops;

// #include "utils.c"
// #include "dirops.c"
// #include "regops.c"

/* allocate global info, try to map host share */
static int
sf_glob_alloc (struct vbsf_mount_info *info, struct sf_glob_info **sf_gp)
{
        int err, rc;
        SHFLSTRING *str_name;
        size_t name_len, str_len;
        struct sf_glob_info *sf_g;

        TRACE ();
        sf_g = kmalloc (sizeof (*sf_g), GFP_KERNEL);
        if (!sf_g) {
                err = -ENOMEM;
                LogRelFunc(("could not allocate memory for global info\n"));
                goto fail0;
        }

        info->name[sizeof (info->name) - 1] = 0;
        info->nls_name[sizeof (info->nls_name) - 1] = 0;

        name_len = strlen (info->name);
        if (name_len > 0xfffe) {
                err = -ENAMETOOLONG;
                LogFunc(("map name too big\n"));
                goto fail1;
        }

        str_len = offsetof (SHFLSTRING, String.utf8) + name_len + 1;
        str_name = kmalloc (str_len, GFP_KERNEL);
        if (!str_name) {
                err = -ENOMEM;
                LogRelFunc(("could not allocate memory for host name\n"));
                goto fail1;
        }

        str_name->u16Length = name_len;
        str_name->u16Size = name_len + 1;
        memcpy (str_name->String.utf8, info->name, name_len + 1);

        if (info->nls_name[0] && strcmp (info->nls_name, "utf8")) {
                sf_g->nls = load_nls (info->nls_name);
                if (!sf_g->nls) {
                        err = -EINVAL;
                        LogFunc(("failed to load nls %s\n", info->nls_name));
                        goto fail1;
                }
        }
        else {
                sf_g->nls = NULL;
        }

        rc = vboxCallMapFolder (&client_handle, str_name, &sf_g->map);
        kfree (str_name);

        if (VBOX_FAILURE (rc)) {
                err = -EPROTO;
                LogFunc(("vboxCallMapFolder failed rc=%d\n", rc));
                goto fail2;
        }

        sf_g->ttl = info->ttl;
        sf_g->uid = info->uid;
        sf_g->gid = info->gid;

        *sf_gp = sf_g;
        return 0;

 fail2:
        if (sf_g->nls) {
                unload_nls (sf_g->nls);
        }
 fail1:
        kfree (sf_g);
 fail0:
        return err;
}

/* unmap the share and free global info [sf_g] */
static void
sf_glob_free (struct sf_glob_info *sf_g)
{
        int rc;

        TRACE ();
        rc = vboxCallUnmapFolder (&client_handle, &sf_g->map);
        if (VBOX_FAILURE (rc)) {
                LogFunc(("vboxCallUnmapFolder failed rc=%d\n", rc));
        }

        if (sf_g->nls) {
                unload_nls (sf_g->nls);
        }
        kfree (sf_g);
}

/* this is called (by sf_read_super_[24|26] when vfs mounts the fs and
   wants to read super_block.

   calls [sf_glob_alloc] to map the folder and allocate global
   information structure.

   initializes [sb], initializes root inode and dentry.

   should respect [flags] */
static int
sf_read_super_aux (struct super_block *sb, void *data, int flags)
{
        int err;
        struct dentry *droot;
        struct inode *iroot;
        struct sf_inode_info *sf_i;
        struct sf_glob_info *sf_g;
        RTFSOBJINFO fsinfo;
        struct vbsf_mount_info *info;

        TRACE ();
        if (!data) {
                LogFunc(("no mount info specified\n"));
                return -EINVAL;
        }

        info = data;

        if (flags & MS_REMOUNT) {
                LogFunc(("remounting is not supported\n"));
                return -ENOSYS;
        }

        err = sf_glob_alloc (info, &sf_g);
        if (err) {
                goto fail0;
        }

        sf_i = kmalloc (sizeof (*sf_i), GFP_KERNEL);
        if (!sf_i) {
                err = -ENOMEM;
                LogRelFunc (("could not allocate memory for root inode info\n"));
                goto fail1;
        }

        sf_i->path = kmalloc (sizeof (SHFLSTRING) + 1, GFP_KERNEL);
        if (!sf_i->path) {
                err = -ENOMEM;
                LogRelFunc (("could not allocate memory for root inode path\n"));
                goto fail2;
        }

        sf_i->path->u16Length = 1;
        sf_i->path->u16Size = 2;
        sf_i->path->String.utf8[0] = '/';
        sf_i->path->String.utf8[1] = 0;

        err = sf_stat (__func__, sf_g, sf_i->path, &fsinfo, 0);
        if (err) {
                LogFunc(("could not stat root of share\n"));
                goto fail3;
        }

        sb->s_magic = 0xface;
        sb->s_blocksize = 1024;
        sb->s_op = &sf_super_ops;

        iroot = iget (sb, 0);
        if (!iroot) {
                err = -ENOMEM;  /* XXX */
                LogFunc(("could not get root inode\n"));
                goto fail3;
        }

        sf_init_inode (sf_g, iroot, &fsinfo);
        SET_INODE_INFO (iroot, sf_i);

        droot = d_alloc_root (iroot);
        if (!droot) {
                err = -ENOMEM;  /* XXX */
                LogFunc(("d_alloc_root failed\n"));
                goto fail4;
        }

        sb->s_root = droot;
        SET_GLOB_INFO (sb, sf_g);
        return 0;

 fail4:
        iput (iroot);
 fail3:
        kfree (sf_i->path);
 fail2:
        kfree (sf_i);
 fail1:
        sf_glob_free (sf_g);
 fail0:
        return err;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION (2, 6, 0)
static struct super_block *
sf_read_super_24 (struct super_block *sb, void *data, int flags)
{
        int err;

        TRACE ();
        err = sf_read_super_aux (sb, data, flags);
        if (err) {
                return NULL;
        }

        return sb;
}
#endif

/* this is called when vfs is about to destroy the [inode]. all
   resources associated with this [inode] must be cleared here */
static void
sf_clear_inode (struct inode *inode)
{
        struct sf_inode_info *sf_i;

        TRACE ();
        sf_i = GET_INODE_INFO (inode);
        if (!sf_i) {
                return;
        }

        BUG_ON (!sf_i->path);
        kfree (sf_i->path);
        kfree (sf_i);
        SET_INODE_INFO (inode, NULL);
}

/* this is called by vfs when it wants to populate [inode] with data.
   the only thing that is known about inode at this point is its index
   hence we can't do anything here, and let lookup/whatever with the
   job to properly fill then [inode] */
static void
sf_read_inode (struct inode *inode)
{
}

/* vfs is done with [sb] (umount called) call [sf_glob_free] to unmap
   the folder and free [sf_g] */
static void
sf_put_super (struct super_block *sb)
{
        struct sf_glob_info *sf_g;

        sf_g = GET_GLOB_INFO (sb);
        BUG_ON (!sf_g);
        sf_glob_free (sf_g);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION (2, 6, 18)
static int
sf_statfs (struct super_block *sb, STRUCT_STATFS *stat)
{
        return sf_get_volume_info(sb, stat);
}
#else
static int
sf_statfs (struct dentry *dentry, STRUCT_STATFS *stat)
{
        struct super_block *sb = dentry->d_inode->i_sb;
        return sf_get_volume_info(sb, stat);
}
#endif

static int
sf_remount_fs (struct super_block *sb, int *flags, char *data)
{
        TRACE ();
        return -ENOSYS;
}

static struct super_operations sf_super_ops = {
        .clear_inode = sf_clear_inode,
        .read_inode  = sf_read_inode,
        .put_super   = sf_put_super,
        .statfs      = sf_statfs,
        .remount_fs  = sf_remount_fs
};

#if LINUX_VERSION_CODE < KERNEL_VERSION (2, 6, 0)
static DECLARE_FSTYPE (vboxsf_fs_type, "vboxsf", sf_read_super_24, 0);
#else
static int
sf_read_super_26 (struct super_block *sb, void *data, int flags)
{
        int err;

        TRACE ();
        err = sf_read_super_aux (sb, data, flags);
        if (err) {
                printk (KERN_DEBUG "sf_read_super_aux err=%d\n", err);
        }
        return err;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION (2, 6, 18)
static struct super_block *
sf_get_sb (struct file_system_type *fs_type, int flags,
           const char *dev_name, void *data)
{
        TRACE ();
        return get_sb_nodev (fs_type, flags, data, sf_read_super_26);
}
#else
static int
sf_get_sb (struct file_system_type *fs_type, int flags,
           const char *dev_name, void *data, struct vfsmount *mnt)
{
        TRACE ();
        return get_sb_nodev (fs_type, flags, data, sf_read_super_26, mnt);
}
#endif

static struct file_system_type vboxsf_fs_type = {
        .owner   = THIS_MODULE,
        .name    = "vboxsf",
        .get_sb  = sf_get_sb,
        .kill_sb = kill_anon_super
};
#endif

extern int CMC_API
vboxadd_cmc_ctl_guest_filter_mask (uint32_t or_mask, uint32_t not_mask);

/* Module initialization/finalization handlers */
static int __init
init (void)
{
        int rcVBox;
        int rcRet = 0;
        int err;

        TRACE ();

        if (sizeof (struct vbsf_mount_info) > PAGE_SIZE) {
                printk (KERN_ERR
                        "Mount information structure is too large %lu\n"
                        "Must be less than or equal to %lu\n",
                        sizeof (struct vbsf_mount_info),
                        PAGE_SIZE);
                return -EINVAL;
        }

        err = register_filesystem (&vboxsf_fs_type);
        if (err) {
                LogFunc(("register_filesystem err=%d\n", err));
                return err;
        }

        if (vboxadd_cmc_ctl_guest_filter_mask (VMMDEV_EVENT_HGCM, 0)) {
                rcRet = -EINVAL;
                goto fail0;
        }

        rcVBox = vboxInit ();
        if (VBOX_FAILURE (rcVBox)) {
                LogRelFunc (("vboxInit failed, rc=%d\n", rcVBox));
                rcRet = -EPROTO;
                goto fail0;
        }

        rcVBox = vboxConnect (&client_handle);
        if (VBOX_FAILURE (rcVBox)) {
                LogRelFunc (("vboxConnect failed, rc=%d\n", rcVBox));
                rcRet = -EPROTO;
                goto fail1;
        }

        rcVBox = vboxCallSetUtf8 (&client_handle);
        if (VBOX_FAILURE (rcVBox)) {
                LogRelFunc (("vboxCallSetUtf8 failed, rc=%d\n", rcVBox));
                rcRet = -EPROTO;
                goto fail2;
        }

        printk(KERN_DEBUG
               "vboxvfs: Successfully loaded version " VBOX_VERSION_STRING
               " (interface " xstr(VMMDEV_VERSION) ")\n");

        return 0;

 fail2:
        vboxDisconnect (&client_handle);
 fail1:
        vboxUninit ();
 fail0:
        vboxadd_cmc_ctl_guest_filter_mask (0, VMMDEV_EVENT_HGCM);
        unregister_filesystem (&vboxsf_fs_type);
        return rcRet;
}

static void __exit
fini (void)
{
        TRACE ();

        vboxDisconnect (&client_handle);
        vboxUninit ();
        vboxadd_cmc_ctl_guest_filter_mask (0, VMMDEV_EVENT_HGCM);
        unregister_filesystem (&vboxsf_fs_type);
}

module_init (init);
module_exit (fini);

/* C++ hack */
int __gxx_personality_v0 = 0xdeadbeef;

#if 0
/* long long hacks (as far as i can see, gcc emits the refs to those
   symbols, notwithstanding the fact that those aren't referenced
   anywhere in the module) */
void __divdi3 (void)
{
        elog ("called from %p\n", __builtin_return_address (0));
        BUG ();
}

void __moddi3 (void)
{
        elog ("called from %p\n", __builtin_return_address (0));
        BUG ();
}
#endif /* 0 */

/*
 * Local Variables:
 * c-mode: linux
 * indent-tabs-mode: nil
 * c-basic-offset: 8
 * End:
 */
