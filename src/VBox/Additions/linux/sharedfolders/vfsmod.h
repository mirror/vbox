/** @file
 *
 * vboxvfs -- VirtualBox Guest Additions for Linux
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VFSMOD_H
#define VFSMOD_H

#include "the-linux-kernel.h"
#include "version-generated.h"

#include "VBoxCalls.h"
#include "vbsfmount.h"

/* structs */
struct sf_glob_info {
        VBSFMAP map;
        struct nls_table *nls;
        int ttl;
        int uid;
        int gid;
};

struct sf_inode_info {
        SHFLSTRING *path;
        int force_restat;
};

struct sf_dir_info {
        struct list_head info_list;
};

struct sf_dir_buf {
        size_t nb_entries;
        size_t free_bytes;
        size_t used_bytes;
        void *buf;
        struct list_head head;
};

struct sf_reg_info {
        SHFLHANDLE handle;
};

/* globals */
extern VBSFCLIENT client_handle;

/* forward declarations */
extern struct inode_operations  sf_dir_iops;
extern struct inode_operations  sf_reg_iops;
extern struct file_operations   sf_dir_fops;
extern struct file_operations   sf_reg_fops;
extern struct dentry_operations sf_dentry_ops;

extern void
sf_init_inode (struct sf_glob_info *sf_g, struct inode *inode,
               RTFSOBJINFO *info);
extern int
sf_stat (const char *caller, struct sf_glob_info *sf_g,
         SHFLSTRING *path, RTFSOBJINFO *result, int ok_to_fail);
extern int
sf_inode_revalidate (struct dentry *dentry);
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2, 6, 0)
extern int
sf_getattr (struct vfsmount *mnt, struct dentry *dentry, struct kstat *kstat);
#endif
extern int
sf_path_from_dentry (const char *caller, struct sf_glob_info *sf_g,
                     struct sf_inode_info *sf_i, struct dentry *dentry,
                     SHFLSTRING **result);
extern int
sf_nlscpy (struct sf_glob_info *sf_g,
           char *name, size_t name_bound_len,
           const unsigned char *utf8_name, size_t utf8_len);
extern void
sf_dir_info_free (struct sf_dir_info *p);
extern struct sf_dir_info *
sf_dir_info_alloc (void);
extern int
sf_dir_read_all (struct sf_glob_info *sf_g, struct sf_inode_info *sf_i,
                 struct sf_dir_info *sf_d, SHFLHANDLE handle);

#if LINUX_VERSION_CODE < KERNEL_VERSION (2, 6, 0)
#define STRUCT_STATFS  struct statfs
#else
#define STRUCT_STATFS  struct kstatfs
#endif
int sf_get_volume_info(struct super_block *sb,STRUCT_STATFS *stat);

#ifdef ALIGN
#undef ALIGN
#endif

#define CMC_API __attribute__ ((cdecl, regparm (0)))

#define TRACE() LogFunc (("tracepoint\n"))

/* Following casts are here to prevent assignment of void * to
   pointers of arbitrary type */
#if LINUX_VERSION_CODE < KERNEL_VERSION (2, 6, 0)
#define GET_GLOB_INFO(sb) ((struct sf_glob_info *) (sb)->u.generic_sbp)
#define SET_GLOB_INFO(sb, sf_g) (sb)->u.generic_sbp = sf_g
#else
#define GET_GLOB_INFO(sb) ((struct sf_glob_info *) (sb)->s_fs_info)
#define SET_GLOB_INFO(sb, sf_g) (sb)->s_fs_info = sf_g
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION (2, 6, 19) || defined(KERNEL_FC6)
/* FC6 kernel 2.6.18, vanilla kernel 2.6.19+ */
#define GET_INODE_INFO(i) ((struct sf_inode_info *) (i)->i_private)
#define SET_INODE_INFO(i, sf_i) (i)->i_private = sf_i
#else
/* vanilla kernel up to 2.6.18 */
#define GET_INODE_INFO(i) ((struct sf_inode_info *) (i)->u.generic_ip)
#define SET_INODE_INFO(i, sf_i) (i)->u.generic_ip = sf_i
#endif

#endif /* vfsmod.h */
