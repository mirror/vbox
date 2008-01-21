/** @file
 *
 * vboxvfs -- VirtualBox Guest Additions for Linux:
 * Directory inode and file operations
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

#include "vfsmod.h"

static int
sf_dir_open (struct inode *inode, struct file *file)
{
        int rc;
        int err;
        struct sf_glob_info *sf_g = GET_GLOB_INFO (inode->i_sb);
        struct sf_dir_info *sf_d;
        struct sf_inode_info *sf_i = GET_INODE_INFO (inode);
        SHFLCREATEPARMS params;

        TRACE ();
        BUG_ON (!sf_g);
        BUG_ON (!sf_i);

        if (file->private_data) {
                LogFunc(("dir_open called on already opened directory %s\n",
                         sf_i->path->String.utf8));
                return 0;
        }

        sf_d = sf_dir_info_alloc ();

        if (!sf_d) {
                LogRelFunc(("could not allocate directory info for %s\n",
                            sf_i->path->String.utf8));
                return -ENOMEM;
        }

        params.CreateFlags = 0
                | SHFL_CF_DIRECTORY
                | SHFL_CF_ACT_OPEN_IF_EXISTS
                | SHFL_CF_ACT_FAIL_IF_NEW
                | SHFL_CF_ACCESS_READ
                ;

        LogFunc(("sf_dir_open: calling vboxCallCreate, folder %s, flags %#x\n",
                 sf_i->path->String.utf8, params.CreateFlags));
        rc = vboxCallCreate (&client_handle, &sf_g->map, sf_i->path, &params);
        if (VBOX_FAILURE (rc)) {
                LogFunc(("vboxCallCreate(%s) failed rc=%Vrc\n",
                         sf_i->path->String.utf8, rc));
                sf_dir_info_free (sf_d);
                return -EPERM;
        }

        if (params.Result != SHFL_FILE_EXISTS) {
                LogFunc(("directory %s does not exist\n", sf_i->path->String.utf8));
                sf_dir_info_free (sf_d);
                return -ENOENT;
        }

        err = sf_dir_read_all (sf_g, sf_i, sf_d, params.Handle);
        if (err) {
                rc = vboxCallClose (&client_handle, &sf_g->map, params.Handle);
                if (VBOX_FAILURE (rc)) {
                        LogFunc(("vboxCallClose(%s) after err=%d failed rc=%Vrc\n",
                                 sf_i->path->String.utf8, err, rc));
                }
                sf_dir_info_free (sf_d);
                return err;
        }


        rc = vboxCallClose (&client_handle, &sf_g->map, params.Handle);
        if (VBOX_FAILURE (rc)) {
                LogFunc(("vboxCallClose(%s) failed rc=%Vrc\n",
                          sf_i->path->String.utf8, rc));
        }

        file->private_data = sf_d;
        return 0;
}

/* This is called when reference count of [file] goes to zero. Notify
   the host that it can free whatever is associated with this
   directory and deallocate our own internal buffers */
static int
sf_dir_release (struct inode *inode, struct file *file)
{
        struct sf_inode_info *sf_i = GET_INODE_INFO (inode);

        TRACE ();
        BUG_ON (!sf_i);

        if (file->private_data) {
                sf_dir_info_free (file->private_data);
        }
        return 0;
}

/* Extract element ([dir]->f_pos) from the directory [dir] into
   [d_name], return:
   0       all ok
   1       end reached
   -errno  some form of error*/
int
sf_getdent (struct file *dir, char d_name[NAME_MAX])
{
        loff_t cur;
        struct sf_glob_info *sf_g;
        struct sf_dir_info *sf_d;
        struct list_head *pos, *list;

        TRACE ();
        sf_g = GET_GLOB_INFO (dir->f_dentry->d_inode->i_sb);
        sf_d = dir->private_data;

        BUG_ON (!sf_g);
        BUG_ON (!sf_d);

        cur = 0;
        list = &sf_d->info_list;
        list_for_each (pos, list) {
                struct sf_dir_buf *b;
                SHFLDIRINFO *info;
                loff_t i;
                size_t name_len;
                char *name_ptr;

                b = list_entry (pos, struct sf_dir_buf, head);
                if (dir->f_pos >= cur + b->nb_entries) {
                        cur += b->nb_entries;
                        continue;
                }

                for (i = 0, info = b->buf; i < dir->f_pos - cur; ++i) {
                        size_t size;

                        size = offsetof (SHFLDIRINFO, name.String) + info->name.u16Size;
                        info = (SHFLDIRINFO *) ((uintptr_t) info + size);
                }

                name_ptr = info->name.String.utf8;
                name_len = info->name.u16Length;

                return sf_nlscpy (sf_g, d_name, NAME_MAX, name_ptr, name_len);
        }
        return 1;
}

/* This is called when vfs wants to populate internal buffers with
   directory [dir]s contents. [opaque] is an argument to the
   [filldir]. [filldir] magically modifies it's argument - [opaque]
   and takes following additional arguments (which i in turn get from
   the host via sf_getdent):

   name : name of the entry (i must also supply it's length huh?)
   type : type of the entry (FILE | DIR | etc) (i ellect to use DT_UNKNOWN)
   pos : position/index of the entry
   ino : inode number of the entry (i fake those)

   [dir] contains:
   f_pos : cursor into the directory listing
   private_data : mean of communcation with the host side

   Extract elements from the directory listing (incrementing f_pos
   along the way) and feed them to [filldir] until:

   a. there are no more entries (i.e. sf_getdent set done to 1)
   b. failure to compute fake inode number
   c. filldir returns an error (see comment on that) */
static int
sf_dir_read (struct file *dir, void *opaque, filldir_t filldir)
{
        TRACE ();
        for (;;) {
                int err;
                ino_t fake_ino;
                loff_t sanity;
                char d_name[NAME_MAX];

                err = sf_getdent (dir, d_name);
                switch (err) {
                        case 1:
                                return 0;

                        case 0:
                                break;

                        case -1:
                        default:
                                /* skip erroneous entry and proceed */
                                LogFunc(("sf_getdent error %d\n", err));
                                dir->f_pos += 1;
                                continue;
                }

                /* d_name now contains valid entry name */

                sanity = dir->f_pos + 0xbeef;
                fake_ino = sanity;
                if (sanity - fake_ino) {
                        LogRelFunc(("can not compute ino\n"));
                        return -EINVAL;
                }

                err = filldir (opaque, d_name, strlen (d_name),
                               dir->f_pos, fake_ino, DT_UNKNOWN);
                if (err) {
                        LogFunc(("filldir returned error %d\n", err));
                        /* Rely on the fact that filldir returns error
                           only when it runs out of space in opaque */
                        return 0;
                }

                dir->f_pos += 1;
        }

        BUG ();
}

struct file_operations sf_dir_fops = {
        .open    = sf_dir_open,
        .readdir = sf_dir_read,
        .release = sf_dir_release,
        .read    = generic_read_dir
};


/* iops */

/* This is called when vfs failed to locate dentry in the cache. The
   job of this function is to allocate inode and link it to dentry.
   [dentry] contains the name to be looked in the [parent] directory.
   Failure to locate the name is not a "hard" error, in this case NULL
   inode is added to [dentry] and vfs should proceed trying to create
   the entry via other means. NULL(or "positive" pointer) ought to be
   returned in case of succes and "negative" pointer on error */
static struct dentry *
sf_lookup (struct inode *parent, struct dentry *dentry
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2, 6, 0)
           , struct nameidata *nd
#endif
        )
{
        int err;
        struct sf_inode_info *sf_i, *sf_new_i;
        struct sf_glob_info *sf_g;
        SHFLSTRING *path;
        struct inode *inode;
        ino_t ino;
        RTFSOBJINFO fsinfo;

        TRACE ();
        sf_g = GET_GLOB_INFO (parent->i_sb);
        sf_i = GET_INODE_INFO (parent);

        BUG_ON (!sf_g);
        BUG_ON (!sf_i);

        err = sf_path_from_dentry (__func__, sf_g, sf_i, dentry, &path);
        if (err) {
                goto fail0;
        }

        err = sf_stat (__func__, sf_g, path, &fsinfo, 1);
        if (err) {
                if (err == -ENOENT) {
                        /* -ENOENT add NULL inode to dentry so it later can be
                           created via call to create/mkdir/open */
                        inode = NULL;
                }
                else goto fail1;
        }
        else {
                sf_new_i = kmalloc (sizeof (*sf_new_i), GFP_KERNEL);
                if (!sf_new_i) {
                        LogRelFunc(("could not allocate memory for new inode info\n"));
                        err = -ENOMEM;
                        goto fail1;
                }

                ino = iunique (parent->i_sb, 1);
                inode = iget (parent->i_sb, ino);
                if (!inode) {
                        LogFunc(("iget failed\n"));
                        err = -ENOMEM;          /* XXX: ??? */
                        goto fail2;
                }

                SET_INODE_INFO (inode, sf_new_i);
                sf_init_inode (sf_g, inode, &fsinfo);
                sf_new_i->path = path;
        }

        sf_i->force_restat = 0;
        dentry->d_time = jiffies;
        dentry->d_op = &sf_dentry_ops;
        d_add (dentry, inode);
        return NULL;

 fail2:
        kfree (sf_new_i);
 fail1:
        kfree (path);
 fail0:
        return ERR_PTR (err);
}

/* This should allocate memory for sf_inode_info, compute unique inode
   number, get an inode from vfs, initialize inode info, instantiate
   dentry */
static int
sf_instantiate (const char *caller, struct inode *parent,
                struct dentry *dentry, SHFLSTRING *path,
                RTFSOBJINFO *info)
{
        int err;
        ino_t ino;
        struct inode *inode;
        struct sf_inode_info *sf_new_i;
        struct sf_glob_info *sf_g = GET_GLOB_INFO (parent->i_sb);

        TRACE ();
        BUG_ON (!sf_g);

        sf_new_i = kmalloc (sizeof (*sf_new_i), GFP_KERNEL);
        if (!sf_new_i) {
                LogRelFunc(("could not allocate inode info.  caller=%s\n", caller));
                err = -ENOMEM;
                goto fail0;
        }

        ino = iunique (parent->i_sb, 1);
        inode = iget (parent->i_sb, ino);
        if (!inode) {
                LogFunc(("iget failed.  caller=%s\n", caller));
                err = -ENOMEM;
                goto fail1;
        }

        sf_init_inode (sf_g, inode, info);
        sf_new_i->path = path;
        SET_INODE_INFO (inode, sf_new_i);

        dentry->d_time = jiffies;
        dentry->d_op = &sf_dentry_ops;
        sf_new_i->force_restat = 1;

        d_instantiate (dentry, inode);
        return 0;

 fail1:
        kfree (sf_new_i);
 fail0:
        return err;

}

static int
sf_create_aux (struct inode *parent, struct dentry *dentry, int dirop)
{
        int rc, err;
        SHFLCREATEPARMS params;
        SHFLSTRING *path;
        struct sf_inode_info *sf_i = GET_INODE_INFO (parent);
        struct sf_glob_info *sf_g = GET_GLOB_INFO (parent->i_sb);

        TRACE ();
        BUG_ON (!sf_i);
        BUG_ON (!sf_g);

#if 0
        printk ("create_aux %s/%s\n", sf_i->path->String.utf8,
                dentry->d_name.name);
#endif

        params.CreateFlags = 0
                | SHFL_CF_ACT_CREATE_IF_NEW
                | SHFL_CF_ACT_FAIL_IF_EXISTS
                | SHFL_CF_ACCESS_READWRITE
                | (dirop ? SHFL_CF_DIRECTORY : 0)
                ;

        params.Info.Attr.fMode = 0
                | (dirop ? RTFS_TYPE_DIRECTORY : RTFS_TYPE_FILE)
                | RTFS_UNIX_IRUSR
                | RTFS_UNIX_IWUSR
                | RTFS_UNIX_IXUSR
                ;

        params.Info.Attr.enmAdditional = RTFSOBJATTRADD_NOTHING;

        err = sf_path_from_dentry (__func__, sf_g, sf_i, dentry, &path);
        if (err) {
                goto fail0;
        }

        LogFunc(("calling vboxCallCreate, folder %s, flags %#x\n",
                 path->String.utf8, params.CreateFlags));
        rc = vboxCallCreate (&client_handle, &sf_g->map, path, &params);
        if (VBOX_FAILURE (rc)) {
                if (rc == VERR_WRITE_PROTECT)
                {
                    err = -EROFS;
                    goto fail0;
                }
                err = -EPROTO;
                LogFunc(("(%d): vboxCallCreate(%s) failed rc=%Vrc\n", dirop,
                         sf_i->path->String.utf8, rc));
                goto fail0;
        }

        if (params.Result != SHFL_FILE_CREATED) {
                err = -EPERM;
                LogFunc(("(%d): could not create file %s result=%d\n", dirop,
                         sf_i->path->String.utf8, params.Result));
                goto fail0;
        }

        err = sf_instantiate (__func__, parent, dentry, path, &params.Info);
        if (err) {
                LogFunc(("(%d): could not instantiate dentry for %s err=%d\n",
                         dirop, sf_i->path->String.utf8, err));
                goto fail1;
        }

        rc = vboxCallClose (&client_handle, &sf_g->map, params.Handle);
        if (VBOX_FAILURE (rc)) {
                LogFunc(("(%d): vboxCallClose failed rc=%Vrc\n", dirop, rc));
        }

        sf_i->force_restat = 1;
        return 0;

 fail1:
        rc = vboxCallClose (&client_handle, &sf_g->map, params.Handle);
        if (VBOX_FAILURE (rc)) {
                LogFunc(("(%d): vboxCallClose failed rc=%Vrc\n", dirop, rc));
        }

 fail0:
        kfree (path);
        return err;
}

static int
sf_create (struct inode *parent, struct dentry *dentry, int mode
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2, 6, 0)
           , struct nameidata *nd
#endif
        )
{
        TRACE ();
        return sf_create_aux (parent, dentry, 0);
}

static int
sf_mkdir (struct inode *parent, struct dentry *dentry, int mode)
{
        TRACE ();
        return sf_create_aux (parent, dentry, 1);
}

static int
sf_unlink_aux (struct inode *parent, struct dentry *dentry, int dirop)
{
        int rc, err;
        struct sf_glob_info *sf_g = GET_GLOB_INFO (parent->i_sb);
        struct sf_inode_info *sf_i = GET_INODE_INFO (parent);
        SHFLSTRING *path;

        TRACE ();
        BUG_ON (!sf_g);

        err = sf_path_from_dentry (__func__, sf_g, sf_i, dentry, &path);
        if (err) {
                goto fail0;
        }

        rc = vboxCallRemove (&client_handle, &sf_g->map, path,
                             dirop ? SHFL_REMOVE_DIR : SHFL_REMOVE_FILE);
        if (VBOX_FAILURE (rc)) {
                LogFunc(("(%d): vboxCallRemove(%s) failed rc=%Vrc\n", dirop,
                         path->String.utf8, rc));
                         err = -RTErrConvertToErrno (rc);
                goto fail1;
        }

        kfree (path);
        return 0;

 fail1:
        kfree (path);
 fail0:
        return err;
}

static int
sf_unlink (struct inode *parent, struct dentry *dentry)
{
        TRACE ();
        return sf_unlink_aux (parent, dentry, 0);
}

static int
sf_rmdir (struct inode *parent, struct dentry *dentry)
{
        TRACE ();
        return sf_unlink_aux (parent, dentry, 1);
}

static int
sf_rename (struct inode *old_parent, struct dentry *old_dentry,
           struct inode *new_parent, struct dentry *new_dentry)
{
        int err = 0, rc = VINF_SUCCESS;
        struct sf_glob_info *sf_g = GET_GLOB_INFO (old_parent->i_sb);

        TRACE ();

        if (sf_g != GET_GLOB_INFO (new_parent->i_sb)) {
                LogFunc(("rename with different roots\n"));
                err = -EINVAL;
        } else {
                struct sf_inode_info *sf_old_i = GET_INODE_INFO (old_parent);
                struct sf_inode_info *sf_new_i = GET_INODE_INFO (new_parent);
                /* As we save the relative path inside the inode structure, we need to change
                  this if the rename is successful. */
                struct sf_inode_info *sf_file_i = GET_INODE_INFO (old_dentry->d_inode);
                SHFLSTRING *old_path;
                SHFLSTRING *new_path;

                BUG_ON (!sf_old_i);
                BUG_ON (!sf_new_i);
                BUG_ON (!sf_file_i);

                old_path = sf_file_i->path;
                err = sf_path_from_dentry (__func__, sf_g, sf_new_i,
                                          new_dentry, &new_path);
                if (err) {
                        LogFunc(("failed to create new path\n"));
                } else {
                        int is_dir = ((old_dentry->d_inode->i_mode & S_IFDIR) != 0);

                        rc = vboxCallRename (&client_handle, &sf_g->map, old_path,
                                             new_path, is_dir ? 0 : SHFL_RENAME_FILE | SHFL_RENAME_REPLACE_IF_EXISTS);
                        if (RT_SUCCESS(rc)) {
                                kfree (old_path);
                                sf_new_i->force_restat = 1;
                                sf_old_i->force_restat = 1; /* XXX: needed? */
                                /* Set the new relative path in the inode. */
                                sf_file_i->path = new_path;
                        } else {
                                LogFunc(("vboxCallRename failed rc=%Vrc\n", rc));
                                err = -RTErrConvertToErrno (rc);
                        }
                        if (0 != err)
                                kfree (new_path);
                }
        }
        return err;
}

struct inode_operations sf_dir_iops = {
        .lookup     = sf_lookup,
        .create     = sf_create,
        .mkdir      = sf_mkdir,
        .rmdir      = sf_rmdir,
        .unlink     = sf_unlink,
        .rename     = sf_rename,
#if LINUX_VERSION_CODE < KERNEL_VERSION (2, 6, 0)
        .revalidate = sf_inode_revalidate
#else
        .getattr    = sf_getattr
#endif
};
