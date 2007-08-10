/** @file
 *
 * vboxvfs -- VirtualBox Guest Additions for Linux:
 * Regular file inode and file operations
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

/*
 * Limitations: only COW memory mapping is supported
 */

#define CHUNK_SIZE 4096

/* fops */
static int
sf_reg_read_aux (const char *caller, struct sf_glob_info *sf_g,
                 struct sf_reg_info *sf_r, void *buf, uint32_t *nread,
                 uint64_t pos)
{
        int rc = vboxCallRead (&client_handle, &sf_g->map, sf_r->handle,
                               pos, nread, buf, false /* already locked? */);
        if (VBOX_FAILURE (rc)) {
                elog3 ("%s: %s: vboxCallRead failed rc=%d\n",
                       caller, __func__, rc);
                return -EPROTO;
        }
        return 0;
}

static ssize_t
sf_reg_read (struct file *file, char *buf, size_t size, loff_t *off)
{
        int err;
        void *tmp;
        size_t left = size;
        ssize_t total_bytes_read = 0;
        struct inode *inode = file->f_dentry->d_inode;
        struct sf_glob_info *sf_g = GET_GLOB_INFO (inode->i_sb);
        struct sf_reg_info *sf_r = file->private_data;
        loff_t pos = *off;

        TRACE ();
        if (!S_ISREG (inode->i_mode)) {
                elog ("read from non regular file %d\n", inode->i_mode);
                return -EINVAL;
        }

        if (!size) {
                return 0;
        }

        tmp = kmalloc (CHUNK_SIZE, GFP_KERNEL);
        if (!tmp) {
                elog ("could not allocate bounce buffer memory %u bytes\n",
                      CHUNK_SIZE);
                return -ENOMEM;
        }

        while (left) {
                uint32_t to_read, nread;

                to_read = CHUNK_SIZE;
                if (to_read > left) {
                        to_read = (uint32_t) left;
                }
                nread = to_read;

                err = sf_reg_read_aux (__func__, sf_g, sf_r, tmp, &nread, pos);
                if (err) {
                        goto fail;
                }

                if (copy_to_user (buf, tmp, nread)) {
                        err = -EFAULT;
                        goto fail;
                }

                pos += nread;
                left -= nread;
                buf += nread;
                total_bytes_read += nread;
                if (nread != to_read) {
                        break;
                }
        }

        *off += total_bytes_read;
        kfree (tmp);
        return total_bytes_read;

 fail:
        kfree (tmp);
        return err;
}

static ssize_t
sf_reg_write (struct file *file, const char *buf, size_t size, loff_t *off)
{
        int err;
        void *tmp;
        size_t left = size;
        ssize_t total_bytes_written = 0;
        struct inode *inode = file->f_dentry->d_inode;
        struct sf_inode_info *sf_i = GET_INODE_INFO (inode);
        struct sf_glob_info *sf_g = GET_GLOB_INFO (inode->i_sb);
        struct sf_reg_info *sf_r = file->private_data;
        loff_t pos = *off;

        TRACE ();
        BUG_ON (!sf_i);
        BUG_ON (!sf_g);
        BUG_ON (!sf_r);

        if (!S_ISREG (inode->i_mode)) {
                elog ("write to non regular file %d\n",  inode->i_mode);
                return -EINVAL;
        }

        if (!size) {
                return 0;
        }

        tmp = kmalloc (CHUNK_SIZE, GFP_KERNEL);
        if (!tmp) {
                elog ("could not allocate bounce buffer memory %u bytes\n",
                      CHUNK_SIZE);
                return -ENOMEM;
        }

        while (left) {
                int rc;
                uint32_t to_write, nwritten;

                to_write = CHUNK_SIZE;
                if (to_write > left) {
                        to_write = (uint32_t) left;
                }
                nwritten = to_write;

                if (copy_from_user (tmp, buf, to_write)) {
                        err = -EFAULT;
                        goto fail;
                }

                rc = vboxCallWrite (&client_handle, &sf_g->map, sf_r->handle,
                                    pos, &nwritten, tmp, false /* already locked? */);
                if (VBOX_FAILURE (rc)) {
                        err = -EPROTO;
                        elog ("vboxCallWrite(%s) failed rc=%d\n",
                              sf_i->path->String.utf8, rc);
                        goto fail;
                }

                pos += nwritten;
                left -= nwritten;
                buf += nwritten;
                total_bytes_written += nwritten;
                if (nwritten != to_write) {
                        break;
                }
        }

#if 1                           /* XXX: which way is correct? */
        *off += total_bytes_written;
#else
        file->f_pos += total_bytes_written;
#endif
        sf_i->force_restat = 1;
        kfree (tmp);
        return total_bytes_written;

 fail:
        kfree (tmp);
        return err;
}

static int
sf_reg_open (struct inode *inode, struct file *file)
{
        int rc;
        struct sf_glob_info *sf_g = GET_GLOB_INFO (inode->i_sb);
        struct sf_inode_info *sf_i = GET_INODE_INFO (inode);
        struct sf_reg_info *sf_r;
        SHFLCREATEPARMS params;

        TRACE ();
        BUG_ON (!sf_g);
        BUG_ON (!sf_i);

        sf_r = kmalloc (sizeof (*sf_r), GFP_KERNEL);
        if (!sf_r) {
                elog2 ("could not allocate reg info\n");
                return -ENOMEM;
        }

#if 0
        printk ("open %s\n", sf_i->path->String.utf8);
#endif

        params.CreateFlags = 0;
        params.Info.cbObject = 0;

        if (file->f_flags & O_CREAT) {
                if (file->f_flags & O_EXCL) {
                        params.CreateFlags |= SHFL_CF_ACCESS_READWRITE;
                        params.CreateFlags |= SHFL_CF_ACT_FAIL_IF_EXISTS;
                }
                else {
                        params.CreateFlags |= SHFL_CF_ACT_CREATE_IF_NEW;
                        params.CreateFlags |= SHFL_CF_ACCESS_READWRITE;
                        if (file->f_flags & O_TRUNC) {
                                params.CreateFlags |= SHFL_CF_ACT_OVERWRITE_IF_EXISTS;
                        }
                        else {
                                params.CreateFlags |= SHFL_CF_ACT_OPEN_IF_EXISTS;
                        }
                }
        }
        else {
                if (file->f_flags & O_TRUNC) {
                        params.CreateFlags |= SHFL_CF_ACCESS_READWRITE;
                        params.CreateFlags |= SHFL_CF_ACT_OVERWRITE_IF_EXISTS;
                }
                else {
                        params.CreateFlags |= SHFL_CF_ACT_FAIL_IF_NEW;
                }
        }

        if (!(params.CreateFlags & SHFL_CF_ACCESS_READWRITE)) {
                switch (file->f_flags & O_ACCMODE) {
                        case O_RDONLY:
                                params.CreateFlags |= SHFL_CF_ACCESS_READ;
                                break;

                        case O_WRONLY:
                                params.CreateFlags |= SHFL_CF_ACCESS_WRITE;
                                break;

                        case O_RDWR:
                                params.CreateFlags |= SHFL_CF_ACCESS_READWRITE;
                                break;

                        default:
                                BUG ();
                }
        }

        rc = vboxCallCreate (&client_handle, &sf_g->map, sf_i->path, &params);

        /* XXX: here i probably should check rc and convert some values to
           EEXISTS ENOENT etc */
        if (VBOX_FAILURE (rc)) {
                elog ("vboxCallCreate failed flags=%d,%#x rc=%d\n",
                      file->f_flags, params.CreateFlags, rc);
                kfree (sf_r);
                return -EPROTO;
        }

        sf_i->force_restat = 1;
        sf_r->handle = params.Handle;
        file->private_data = sf_r;
        return 0;
}

static int
sf_reg_release (struct inode *inode, struct file *file)
{
        int rc;
        struct sf_reg_info *sf_r;
        struct sf_glob_info *sf_g;

        TRACE ();
        sf_g = GET_GLOB_INFO (inode->i_sb);
        sf_r = file->private_data;

        BUG_ON (!sf_g);
        BUG_ON (!sf_r);

        rc = vboxCallClose (&client_handle, &sf_g->map, sf_r->handle);
        if (VBOX_FAILURE (rc)) {
                elog ("vboxCallClose failed rc=%d\n", rc);
        }

        kfree (sf_r);
        file->private_data = NULL;
        return 0;
}

static struct page *
#if LINUX_VERSION_CODE < KERNEL_VERSION (2, 6, 0)
sf_reg_nopage (struct vm_area_struct *vma, unsigned long vaddr, int unused)
#define SET_TYPE(t)
#else
sf_reg_nopage (struct vm_area_struct *vma, unsigned long vaddr, int *type)
#define SET_TYPE(t) *type = (t)
#endif
{
        struct page *page;
        char *buf;
        loff_t off;
        uint32_t nread = PAGE_SIZE;
        int err;
        struct file *file = vma->vm_file;
        struct inode *inode = file->f_dentry->d_inode;
        struct sf_glob_info *sf_g = GET_GLOB_INFO (inode->i_sb);
        struct sf_reg_info *sf_r = file->private_data;

        TRACE ();
        if (vaddr > vma->vm_end) {
                SET_TYPE (VM_FAULT_SIGBUS);
                return NOPAGE_SIGBUS;
        }

        page = alloc_page (GFP_HIGHUSER);
        if (!page) {
                elog2 ("failed to allocate page\n");
                SET_TYPE (VM_FAULT_OOM);
                return NOPAGE_OOM;
        }

        buf = kmap (page);
        off = (vaddr - vma->vm_start) + (vma->vm_pgoff << PAGE_SHIFT);

        err = sf_reg_read_aux (__func__, sf_g, sf_r, buf, &nread, off);
        if (err) {
                kunmap (page);
                put_page (page);
                SET_TYPE (VM_FAULT_SIGBUS);
                return NOPAGE_SIGBUS;
        }

        BUG_ON (nread > PAGE_SIZE);
        if (!nread) {
#if LINUX_VERSION_CODE < KERNEL_VERSION (2, 6, 0)
                clear_user_page (page_address (page), vaddr);
#else
                clear_user_page (page_address (page), vaddr, page);
#endif
        }
        else {
                memset (buf + nread, 0, PAGE_SIZE - nread);
        }

        flush_dcache_page (page);
        kunmap (page);
        SET_TYPE (VM_FAULT_MAJOR);
        return page;
}

static struct vm_operations_struct sf_vma_ops = {
        .nopage = sf_reg_nopage
};

static int
sf_reg_mmap (struct file *file, struct vm_area_struct *vma)
{
        TRACE ();
        if (vma->vm_flags & VM_SHARED) {
                elog2 ("shared mmapping not available\n");
                return -EINVAL;
        }

        vma->vm_ops = &sf_vma_ops;
        return 0;
}

static struct file_operations sf_reg_fops = {
        .read    = sf_reg_read,
        .open    = sf_reg_open,
        .write   = sf_reg_write,
        .release = sf_reg_release,
        .mmap    = sf_reg_mmap
};


/* iops */

static struct inode_operations sf_reg_iops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION (2, 6, 0)
        .revalidate = sf_inode_revalidate
#else
        .getattr    = sf_getattr
#endif
};
