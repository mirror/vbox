/** @file
 *
 * vboxvfs -- VirtualBox Guest Additions for Linux:
 * Utility functions.
 * Mainly conversion from/to VirtualBox/Linux data structures
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

#include "vfsmod.h"
#include <linux/nfs_fs.h>
#include <linux/vfs.h>

/* #define USE_VMALLOC */

#if LINUX_VERSION_CODE >= KERNEL_VERSION (2, 6, 0)
/*
 * sf_aops and sf_backing_dev_info are just quick implementations to make
 * sendfile work. For more information have a look at
 *
 *   http://us1.samba.org/samba/ftp/cifs-cvs/ols2006-fs-tutorial-smf.odp
 *
 * and the sample implementation
 *
 *   http://pserver.samba.org/samba/ftp/cifs-cvs/samplefs.tar.gz
 */
static struct address_space_operations sf_aops = {
        .readpage      = simple_readpage,
        .prepare_write = simple_prepare_write,
        .commit_write  = simple_commit_write,
};

static struct backing_dev_info sf_backing_dev_info = {
        .ra_pages      = 0, /* No readahead */
# if LINUX_VERSION_CODE >= KERNEL_VERSION (2, 6, 12)
        .capabilities  = BDI_CAP_MAP_DIRECT    /* MAP_SHARED */
                       | BDI_CAP_MAP_COPY      /* MAP_PRIVATE */
                       | BDI_CAP_READ_MAP      /* can be mapped for reading */
                       | BDI_CAP_WRITE_MAP     /* can be mapped for writing */
                       | BDI_CAP_EXEC_MAP,     /* can be mapped for execution */
# endif
};
#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION (2, 6, 0) */

#if LINUX_VERSION_CODE < KERNEL_VERSION (2, 6, 0)
static void
sf_ftime_from_timespec (time_t *time, RTTIMESPEC *ts)
{
        int64_t t = RTTimeSpecGetNano (ts);

        do_div (t, 1000000000);
        *time = t;
}
#else
static void
sf_ftime_from_timespec (struct timespec *tv, RTTIMESPEC *ts)
{
        int64_t t = RTTimeSpecGetNano (ts);
        int64_t nsec;

        nsec = do_div (t, 1000000000);
        tv->tv_sec = t;
        tv->tv_nsec = nsec;
}
#endif

/* set [inode] attributes based on [info], uid/gid based on [sf_g] */
void
sf_init_inode (struct sf_glob_info *sf_g, struct inode *inode,
               RTFSOBJINFO *info)
{
        int is_dir;
        RTFSOBJATTR *attr;
        int mode;

        TRACE ();

        attr = &info->Attr;
        is_dir = RTFS_IS_DIRECTORY (attr->fMode);

#define mode_set(r) attr->fMode & (RTFS_UNIX_##r) ? (S_##r) : 0;
        mode = mode_set (ISUID);
        mode |= mode_set (ISGID);

        mode |= mode_set (IRUSR);
        mode |= mode_set (IWUSR);
        mode |= mode_set (IXUSR);

        mode |= mode_set (IRGRP);
        mode |= mode_set (IWGRP);
        mode |= mode_set (IXGRP);

        mode |= mode_set (IROTH);
        mode |= mode_set (IWOTH);
        mode |= mode_set (IXOTH);
#undef mode_set

#if LINUX_VERSION_CODE >= KERNEL_VERSION (2, 6, 0)
        inode->i_mapping->a_ops = &sf_aops;
        inode->i_mapping->backing_dev_info = &sf_backing_dev_info;
#endif

        if (is_dir) {
                inode->i_mode = S_IFDIR | mode;
                inode->i_op = &sf_dir_iops;
                inode->i_fop = &sf_dir_fops;
                /* XXX: this probably should be set to the number of entries
                   in the directory plus two (. ..) */
                inode->i_nlink = 1;
        }
        else {
                inode->i_mode = S_IFREG | mode;
                inode->i_op = &sf_reg_iops;
                inode->i_fop = &sf_reg_fops;
                inode->i_nlink = 1;
        }

        inode->i_uid = sf_g->uid;
        inode->i_gid = sf_g->gid;
        inode->i_size = info->cbObject;
#if LINUX_VERSION_CODE < KERNEL_VERSION (2, 6, 19) && !defined(KERNEL_FC6)
        inode->i_blksize = 4096;
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2, 4, 11)
        inode->i_blkbits = 12;
#endif
        inode->i_blocks = (info->cbObject + 4095) / 4096;

        sf_ftime_from_timespec (&inode->i_atime, &info->AccessTime);
        sf_ftime_from_timespec (&inode->i_ctime, &info->ChangeTime);
        sf_ftime_from_timespec (&inode->i_mtime, &info->ModificationTime);
}

int
sf_stat (const char *caller, struct sf_glob_info *sf_g,
         SHFLSTRING *path, RTFSOBJINFO *result, int ok_to_fail)
{
        int rc;
        SHFLCREATEPARMS params;

        TRACE ();
        params.CreateFlags = SHFL_CF_LOOKUP | SHFL_CF_ACT_FAIL_IF_NEW;
        LogFunc(("calling vboxCallCreate, file %s, flags %#x\n",
                 path->String.utf8, params.CreateFlags));
        rc = vboxCallCreate (&client_handle, &sf_g->map, path, &params);
        if (VBOX_FAILURE (rc)) {
                LogFunc(("vboxCallCreate(%s) failed.  caller=%s, rc=%Vrc\n",
                         path->String.utf8, rc, caller));
                return -EPROTO;
        }

        if (params.Result != SHFL_FILE_EXISTS) {
                if (!ok_to_fail) {
                        LogFunc(("vboxCallCreate(%s) file does not exist.  caller=%s, result=%d\n",
                                 path->String.utf8, params.Result, caller));
                }
                return -ENOENT;
        }

        *result = params.Info;
        return 0;
}

/* this is called directly as iop on 2.4, indirectly as dop
   [sf_dentry_revalidate] on 2.4/2.6, indirectly as iop through
   [sf_getattr] on 2.6. the job is to find out whether dentry/inode is
   still valid. the test is failed if [dentry] does not have an inode
   or [sf_stat] is unsuccessful, otherwise we return success and
   update inode attributes */
int
sf_inode_revalidate (struct dentry *dentry)
{
        int err;
        struct sf_glob_info *sf_g;
        struct sf_inode_info *sf_i;
        RTFSOBJINFO info;

        TRACE ();
        if (!dentry || !dentry->d_inode) {
                LogFunc(("no dentry(%p) or inode(%p)\n", dentry, dentry->d_inode));
                return -EINVAL;
        }

        sf_g = GET_GLOB_INFO (dentry->d_inode->i_sb);
        sf_i = GET_INODE_INFO (dentry->d_inode);

#if 0
        printk ("%s called by %p:%p\n",
                sf_i->path->String.utf8,
                __builtin_return_address (0),
                __builtin_return_address (1));
#endif

        BUG_ON (!sf_g);
        BUG_ON (!sf_i);

        if (!sf_i->force_restat) {
                if (jiffies - dentry->d_time < sf_g->ttl) {
                        return 0;
                }
        }

        err = sf_stat (__func__, sf_g, sf_i->path, &info, 1);
        if (err) {
                return err;
        }

        dentry->d_time = jiffies;
        sf_init_inode (sf_g, dentry->d_inode, &info);
        return 0;
}

/* this is called during name resolution/lookup to check if the
   [dentry] in the cache is still valid. the job is handled by
   [sf_inode_revalidate] */
static int
#if LINUX_VERSION_CODE < KERNEL_VERSION (2, 6, 0)
sf_dentry_revalidate (struct dentry *dentry, int flags)
#else
        sf_dentry_revalidate (struct dentry *dentry, struct nameidata *nd)
#endif
{
        TRACE ();
        if (sf_inode_revalidate (dentry)) {
                return 0;
        }
        return 1;
}

/* on 2.6 this is a proxy for [sf_inode_revalidate] which (as a side
   effect) updates inode attributes for [dentry] (given that [dentry]
   has inode at all) from these new attributes we derive [kstat] via
   [generic_fillattr] */
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2, 6, 0)
int
sf_getattr (struct vfsmount *mnt, struct dentry *dentry, struct kstat *kstat)
{
        int err;

        TRACE ();
        err = sf_inode_revalidate (dentry);
        if (err) {
                return err;
        }

        generic_fillattr (dentry->d_inode, kstat);
        return 0;
}
#endif

static int
sf_make_path (const char *caller, struct sf_inode_info *sf_i,
              const char *d_name, size_t d_len, SHFLSTRING **result)
{
        size_t path_len, shflstring_len;
        SHFLSTRING *tmp;
        uint16_t p_len;
        uint8_t *p_name;
        uint8_t *dst;
        int is_root = 0;

        TRACE ();
        p_len = sf_i->path->u16Length;
        p_name = sf_i->path->String.utf8;

        if (p_len == 1 && *p_name == '/') {
                path_len = d_len + 1;
                is_root = 1;
        }
        else {
                /* lengths of constituents plus terminating zero plus slash  */
                path_len = p_len + d_len + 2;
                if (path_len > 0xffff) {
                        LogFunc(("path too long.  caller=%s, path_len=%zu\n", caller, path_len));
                        return -ENAMETOOLONG;
                }
        }

        shflstring_len = offsetof (SHFLSTRING, String.utf8) + path_len;
        tmp = kmalloc (shflstring_len, GFP_KERNEL);
        if (!tmp) {
                LogRelFunc(("kmalloc failed, caller=%s\n", caller));
                return -ENOMEM;
        }
        tmp->u16Length = path_len - 1;
        tmp->u16Size = path_len;

        if (is_root) {
                memcpy (tmp->String.utf8, d_name, d_len + 1);
        }
        else {
                dst = tmp->String.utf8;
                memcpy (dst, p_name, p_len);
                dst += p_len; *dst++ = '/';
                memcpy (dst, d_name, d_len);
                dst[d_len] = 0;
        }

        *result = tmp;
        return 0;
}

/* [dentry] contains string encoded in coding system that corresponds
   to [sf_g]->nls, we must convert it to UTF8 here and pass down to
   [sf_make_path] which will allocate SHFLSTRING and fill it in */
int
sf_path_from_dentry (const char *caller, struct sf_glob_info *sf_g,
                     struct sf_inode_info *sf_i, struct dentry *dentry,
                     SHFLSTRING **result)
{
        int err;
        const char *d_name;
        size_t d_len;
        const char *name;
        size_t len = 0;

        TRACE ();
        d_name = dentry->d_name.name;
        d_len = dentry->d_name.len;

        if (sf_g->nls) {
                size_t in_len, i, out_bound_len;
                const char *in;
                char *out;

                in = d_name;
                in_len = d_len;

                out_bound_len = PATH_MAX;
                out = kmalloc (out_bound_len, GFP_KERNEL);
                name = out;

                for (i = 0; i < d_len; ++i) {
                        /* We renamed the linux kernel wchar_t type to linux_wchar_t in
                           the-linux-kernel.h, as it conflicts with the C++ type of that name. */
                        linux_wchar_t uni;
                        int nb;

                        nb = sf_g->nls->char2uni (in, in_len, &uni);
                        if (nb < 0) {
                                LogFunc(("nls->char2uni failed %x %d\n",
                                         *in, in_len));
                                err = -EINVAL;
                                goto fail1;
                        }
                        in_len -= nb;
                        in += nb;

                        nb = utf8_wctomb (out, uni, out_bound_len);
                        if (nb < 0) {
                                LogFunc(("nls->uni2char failed %x %d\n",
                                         uni, out_bound_len));
                                err = -EINVAL;
                                goto fail1;
                        }
                        out_bound_len -= nb;
                        out += nb;
                        len += nb;
                }
                if (len >= PATH_MAX - 1) {
                        err = -ENAMETOOLONG;
                        goto fail1;
                }

                LogFunc(("result(%d) = %.*s\n", len, len, name));
                *out = 0;
        }
        else {
                name = d_name;
                len = d_len;
        }

        err = sf_make_path (caller, sf_i, name, len, result);
        if (name != d_name) {
                kfree (name);
        }
        return err;

 fail1:
        kfree (name);
        return err;
}

int
sf_nlscpy (struct sf_glob_info *sf_g,
           char *name, size_t name_bound_len,
           const unsigned char *utf8_name, size_t utf8_len)
{
        if (sf_g->nls) {
                const char *in;
                char *out;
                size_t out_len;
                size_t out_bound_len;
                size_t in_bound_len;

                in = utf8_name;
                in_bound_len = utf8_len;

                out = name;
                out_len = 0;
                out_bound_len = name_bound_len;

                while (in_bound_len) {
                        int nb;
                        wchar_t uni;

                        nb = utf8_mbtowc (&uni, in, in_bound_len);
                        if (nb < 0) {
                                LogFunc(("utf8_mbtowc failed(%s) %x:%d\n",
                                         (const char *) utf8_name, *in, in_bound_len));
                                return -EINVAL;
                        }
                        in += nb;
                        in_bound_len -= nb;

                        nb = sf_g->nls->uni2char (uni, out, out_bound_len);
                        if (nb < 0) {
                                LogFunc(("nls->uni2char failed(%s) %x:%d\n",
                                         utf8_name, uni, out_bound_len));
                                return nb;
                        }
                        out += nb;
                        out_bound_len -= nb;
                        out_len += nb;
                }

                *out = 0;
                return 0;
        }
        else {
                if (utf8_len + 1 > name_bound_len) {
                        return -ENAMETOOLONG;
                }
                else {
                        memcpy (name, utf8_name, utf8_len + 1);
                }
                return 0;
        }
}

static struct sf_dir_buf *
sf_dir_buf_alloc (void)
{
        struct sf_dir_buf *b;

        TRACE ();
        b = kmalloc (sizeof (*b), GFP_KERNEL);
        if (!b) {
                LogRelFunc(("could not alloc directory buffer\n"));
                return NULL;
        }

#ifdef USE_VMALLOC
        b->buf = vmalloc (16384);
#else
        b->buf = kmalloc (16384, GFP_KERNEL);
#endif
        if (!b->buf) {
                kfree (b);
                LogRelFunc(("could not alloc directory buffer storage\n"));
                return NULL;
        }

        INIT_LIST_HEAD (&b->head);
        b->nb_entries = 0;
        b->used_bytes = 0;
        b->free_bytes = 16384;
        return b;
}

static void
sf_dir_buf_free (struct sf_dir_buf *b)
{
        BUG_ON (!b || !b->buf);

        TRACE ();
        list_del (&b->head);
#ifdef USE_VMALLOC
        vfree (b->buf);
#else
        kfree (b->buf);
#endif
        kfree (b);
}

void
sf_dir_info_free (struct sf_dir_info *p)
{
        struct list_head *list, *pos, *tmp;

        TRACE ();
        list = &p->info_list;
        list_for_each_safe (pos, tmp, list) {
                struct sf_dir_buf *b;

                b = list_entry (pos, struct sf_dir_buf, head);
                sf_dir_buf_free (b);
        }
        kfree (p);
}

struct sf_dir_info *
sf_dir_info_alloc (void)
{
        struct sf_dir_info *p;

        TRACE ();
        p = kmalloc (sizeof (*p), GFP_KERNEL);
        if (!p) {
                LogRelFunc(("could not alloc directory info\n"));
                return NULL;
        }

        INIT_LIST_HEAD (&p->info_list);
        return p;
}

static struct sf_dir_buf *
sf_get_non_empty_dir_buf (struct sf_dir_info *sf_d)
{
        struct list_head *list, *pos;

        list = &sf_d->info_list;
        list_for_each (pos, list) {
                struct sf_dir_buf *b;

                b = list_entry (pos, struct sf_dir_buf, head);
                if (!b) {
                        return NULL;
                }
                else {
                        if (b->free_bytes > 0) {
                                return b;
                        }
                }
        }

        return NULL;
}

int
sf_dir_read_all (struct sf_glob_info *sf_g, struct sf_inode_info *sf_i,
                 struct sf_dir_info *sf_d, SHFLHANDLE handle)
{
        int err;
        SHFLSTRING *mask;
        struct sf_dir_buf *b;

        TRACE ();
        err = sf_make_path (__func__, sf_i, "*", 1, &mask);
        if (err) {
                goto fail0;
        }

        b = sf_get_non_empty_dir_buf (sf_d);
        for (;;) {
                int rc;
                void *buf;
                uint32_t buf_size;
                uint32_t nb_ents;

                if (!b) {
                        b = sf_dir_buf_alloc ();
                        if (!b) {
                                err = -ENOMEM;
                                LogRelFunc(("could not alloc directory buffer\n"));
                                goto fail1;
                        }
                }

                list_add (&b->head, &sf_d->info_list);

                buf = b->buf;
                buf_size = b->free_bytes;

                rc = vboxCallDirInfo (
                        &client_handle,
                        &sf_g->map,
                        handle,
                        mask,
                        0,
                        0,
                        &buf_size,
                        buf,
                        &nb_ents
                        );
                switch (rc) {
                        case VINF_SUCCESS:
                                /* fallthrough */
                        case VERR_NO_MORE_FILES:
                                break;

                        case VERR_NO_TRANSLATION:
                                LogFunc(("host could not translate entry\n"));
                                /* XXX */
                                break;

                        default:
                                err = -RTErrConvertToErrno (rc);
                                LogFunc(("vboxCallDirInfo failed rc=%Vrc\n", rc));
                                goto fail1;
                }

                b->nb_entries += nb_ents;
                b->free_bytes -= buf_size;
                b->used_bytes += buf_size;
                b = NULL;

                if (VBOX_FAILURE (rc)) {
                        break;
                }
        }
        return 0;

 fail1:
        kfree (mask);
 fail0:
        return err;
}

int sf_get_volume_info(struct super_block *sb, STRUCT_STATFS *stat)
{
        struct sf_glob_info *sf_g;
        SHFLVOLINFO SHFLVolumeInfo;
        uint32_t cbBuffer;
        int rc;

        sf_g = GET_GLOB_INFO (sb);
        cbBuffer = sizeof(SHFLVolumeInfo);
        rc = vboxCallFSInfo(&client_handle, &sf_g->map, 0, SHFL_INFO_GET | SHFL_INFO_VOLUME,
                            &cbBuffer, (PSHFLDIRINFO)&SHFLVolumeInfo);
        if (VBOX_FAILURE(rc))
            return -RTErrConvertToErrno(rc);

        stat->f_type        = NFS_SUPER_MAGIC; /* XXX vboxsf type? */
        stat->f_bsize       = SHFLVolumeInfo.ulBytesPerAllocationUnit;
        stat->f_blocks      = SHFLVolumeInfo.ullTotalAllocationBytes
                            / SHFLVolumeInfo.ulBytesPerAllocationUnit;
        stat->f_bfree       = SHFLVolumeInfo.ullAvailableAllocationBytes
                            / SHFLVolumeInfo.ulBytesPerAllocationUnit;
        stat->f_bavail      = SHFLVolumeInfo.ullAvailableAllocationBytes
                            / SHFLVolumeInfo.ulBytesPerAllocationUnit;
        stat->f_files       = 1000;
        stat->f_ffree       = 1000; /* don't return 0 here since the guest may think
                                     * that it is not possible to create any more files */
        stat->f_fsid.val[0] = 0;
        stat->f_fsid.val[1] = 0;
        stat->f_namelen     = 255;
        return 0;
}

struct dentry_operations sf_dentry_ops = {
        .d_revalidate = sf_dentry_revalidate
};
