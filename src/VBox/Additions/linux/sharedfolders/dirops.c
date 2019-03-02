/* $Id$ */
/** @file
 * vboxsf - VBox Linux Shared Folders VFS, directory inode and file operations.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "vfsmod.h"
#include <iprt/err.h>


/**
 * Reads or re-reads a directory.
 *
 * @note As suggested a couple of other places, we should probably stop
 *       reading in the whole directory on open.
 */
static int vbsf_dir_open_worker(struct dentry *pDirEntry, struct inode *pInode, struct vbsf_inode_info *sf_i,
                                struct vbsf_dir_info *sf_d, struct vbsf_super_info *sf_g, const char *pszCaller)
{
    union SfDirOpenCloseReq
    {
        VBOXSFCREATEREQ Create;
        VBOXSFCLOSEREQ  Close;
    } *pReq;
    int err;

    pReq = (union SfDirOpenCloseReq *)VbglR0PhysHeapAlloc(RT_UOFFSETOF(VBOXSFCREATEREQ, StrPath.String) + sf_i->path->u16Size);
    if (pReq) {
        int rc;

        memcpy(&pReq->Create.StrPath, sf_i->path, SHFLSTRING_HEADER_SIZE + sf_i->path->u16Size);
        RT_ZERO(pReq->Create.CreateParms);
        pReq->Create.CreateParms.Handle      = SHFL_HANDLE_NIL;
        pReq->Create.CreateParms.CreateFlags = SHFL_CF_DIRECTORY
                                             | SHFL_CF_ACT_OPEN_IF_EXISTS
                                             | SHFL_CF_ACT_FAIL_IF_NEW
                                             | SHFL_CF_ACCESS_READ;

        LogFunc(("calling VbglR0SfHostReqCreate on folder %s, flags %#x [caller: %s]\n",
                 sf_i->path->String.utf8, pReq->Create.CreateParms.CreateFlags, pszCaller));
        rc = VbglR0SfHostReqCreate(sf_g->map.root, &pReq->Create);
        if (RT_SUCCESS(rc)) {
            if (pReq->Create.CreateParms.Result == SHFL_FILE_EXISTS) {
                Assert(pReq->Create.CreateParms.Handle != SHFL_HANDLE_NIL);

                /*
                 * Update the inode info with fresh stats and increase the TTL for the
                 * dentry cache chain that got us here.
                 */
                vbsf_update_inode(pInode, sf_i, &pReq->Create.CreateParms.Info, sf_g, true /*fLocked*/ /** @todo inode locking */);
                vbsf_dentry_chain_increase_ttl(pDirEntry);

#ifndef VBSF_BUFFER_DIRS
                sf_d->Handle.hHost  = pReq->Create.CreateParms.Handle;
                sf_d->Handle.cRefs  = 1;
                sf_d->Handle.fFlags = VBSF_HANDLE_F_READ | VBSF_HANDLE_F_DIR | VBSF_HANDLE_F_MAGIC;
                vbsf_handle_append(sf_i, &sf_d->Handle);

                VbglR0PhysHeapFree(pReq);
                return 0;
#else
                vbsf_dir_info_empty(sf_d);
                err = vbsf_dir_read_all(sf_g, sf_i, sf_d, pReq->Create.CreateParms.Handle);
#endif
            } else {
                /*
                 * Directory does not exist, so we probably got some invalid
                 * dir cache and inode info.
                 */
                /** @todo do more to invalidate dentry and inode here. */
                vbsf_dentry_set_update_jiffies(pDirEntry, jiffies + INT_MAX / 2);
                sf_i->force_restat = true;
                err = -ENOENT;
            }

            AssertCompile(RTASSERT_OFFSET_OF(VBOXSFCREATEREQ, CreateParms.Handle) > sizeof(VBOXSFCLOSEREQ)); /* no aliasing issues */
            if (pReq->Create.CreateParms.Handle != SHFL_HANDLE_NIL) {
                rc = VbglR0SfHostReqClose(sf_g->map.root, &pReq->Close, pReq->Create.CreateParms.Handle);
                if (RT_FAILURE(rc))
                    LogFunc(("VbglR0SfHostReqCloseSimple(%s) after err=%d failed rc=%Rrc caller=%s\n",
                             sf_i->path->String.utf8, err, rc, pszCaller));
            }
        } else
            err = -EPERM;

        VbglR0PhysHeapFree(pReq);
    } else {
        LogRelMaxFunc(64, ("failed to allocate %zu bytes for '%s' [caller: %s]\n",
                           RT_UOFFSETOF(VBOXSFCREATEREQ, StrPath.String) + sf_i->path->u16Size,
                           sf_i->path->String.ach, pszCaller));
        err = -ENOMEM;
    }
    return err;
}

/**
 * Open a directory. Read the complete content into a buffer.
 *
 * @param inode     inode
 * @param file      file
 * @returns 0 on success, Linux error code otherwise
 */
static int vbsf_dir_open(struct inode *inode, struct file *file)
{
    struct vbsf_super_info *sf_g = VBSF_GET_SUPER_INFO(inode->i_sb);
    struct vbsf_inode_info *sf_i = VBSF_GET_INODE_INFO(inode);
    struct dentry          *dentry = VBSF_GET_F_DENTRY(file);
    struct vbsf_dir_info   *sf_d;
    int                     rc;

    SFLOGFLOW(("vbsf_dir_open: inode=%p file=%p %s\n", inode, file, sf_i && sf_i->path ? sf_i->path->String.ach : NULL));
    AssertReturn(sf_g, -EINVAL);
    AssertReturn(sf_i, -EINVAL);
    AssertReturn(!file->private_data, 0);

#ifndef VBSF_BUFFER_DIRS
    /*
     * Allocate and initialize our directory info structure.
     * We delay buffer allocation until vbsf_getdent is actually used.
     */
    sf_d = kmalloc(sizeof(*sf_d), GFP_KERNEL);
    if (sf_d) {
        RT_ZERO(*sf_d);
        sf_d->u32Magic = VBSF_DIR_INFO_MAGIC;

        /*
         * Try open the directory.
         */
        rc = vbsf_dir_open_worker(dentry, inode, sf_i, sf_d, sf_g, "vbsf_dir_open");
        if (!rc)
            file->private_data = sf_d;
        else {
            sf_d->u32Magic = VBSF_DIR_INFO_MAGIC_DEAD;
            kfree(sf_d);
        }
    } else
        rc = -ENOMEM;

#else
    sf_d = vbsf_dir_info_alloc();
    if (!sf_d) {
        LogRelFunc(("could not allocate directory info for '%s'\n", sf_i->path->String.ach));
        return -ENOMEM;
    }

    rc = vbsf_dir_open_worker(dentry, inode, sf_i, sf_d, sf_g, "vbsf_dir_open");
    if (!rc)
        file->private_data = sf_d;
    else
        vbsf_dir_info_free(sf_d);
#endif

    return rc;
}

/**
 * This is called when reference count of [file] goes to zero. Notify
 * the host that it can free whatever is associated with this directory
 * and deallocate our own internal buffers
 *
 * @param inode     inode
 * @param file      file
 * returns 0 on success, Linux error code otherwise
 */
static int vbsf_dir_release(struct inode *inode, struct file *file)
{
    struct vbsf_dir_info *sf_d = (struct vbsf_dir_info *)file->private_data;

    TRACE();

    if (sf_d) {
#ifndef VBSF_BUFFER_DIRS
        struct vbsf_super_info *sf_g = VBSF_GET_SUPER_INFO(inode->i_sb);

        /* Invalidate the non-handle part. */
        sf_d->u32Magic     = VBSF_DIR_INFO_MAGIC_DEAD;
        sf_d->cEntriesLeft = 0;
        sf_d->cbValid      = 0;
        sf_d->pEntry       = NULL;
        sf_d->fNoMoreFiles = false;
        if (sf_d->pBuf) {
            kfree(sf_d->pBuf);
            sf_d->pBuf = NULL;
        }

        /* Closes the handle and frees the structure when the last reference is released. */
        vbsf_handle_release(&sf_d->Handle, sf_g, "vbsf_dir_release");
#else
        vbsf_dir_info_free(file->private_data);
#endif
    }

    return 0;
}


/**
 * Translate RTFMODE into DT_xxx (in conjunction to rtDirType()).
 * @param fMode     file mode
 * returns d_type
 */
static int vbsf_get_d_type(RTFMODE fMode)
{
    switch (fMode & RTFS_TYPE_MASK) {
        case RTFS_TYPE_FIFO:        return DT_FIFO;
        case RTFS_TYPE_DEV_CHAR:    return DT_CHR;
        case RTFS_TYPE_DIRECTORY:   return DT_DIR;
        case RTFS_TYPE_DEV_BLOCK:   return DT_BLK;
        case RTFS_TYPE_FILE:        return DT_REG;
        case RTFS_TYPE_SYMLINK:     return DT_LNK;
        case RTFS_TYPE_SOCKET:      return DT_SOCK;
        case RTFS_TYPE_WHITEOUT:    return DT_WHT;
    }
    return DT_UNKNOWN;
}

#ifndef VBSF_BUFFER_DIRS

/**
 * Refills the buffer with more entries.
 *
 * @returns 0 on success, negative errno on error,
 */
static int vbsf_dir_read_more(struct vbsf_dir_info *sf_d, struct vbsf_super_info *sf_g, bool fRestart)
{
    int               rc;
    VBOXSFLISTDIRREQ *pReq;

    /*
     * Don't call the host again if we've reached the end of the
     * directory entries already.
     */
    if (sf_d->fNoMoreFiles) {
        if (!fRestart)
            return 0;
        sf_d->fNoMoreFiles = false;
    }


    /*
     * Make sure we've got some kind of buffers.
     */
    if (sf_d->pBuf) {
        /* Likely, except for the first time. */
    } else {
        sf_d->pBuf = (PSHFLDIRINFO)kmalloc(_64K, GFP_KERNEL);
        if (sf_d->pBuf)
            sf_d->cbBuf = _64K;
        else {
            sf_d->pBuf = (PSHFLDIRINFO)kmalloc(_4K, GFP_KERNEL);
            sf_d->cbBuf = _4K;
            if (!sf_d->pBuf) {
                LogRelMax(10, ("vbsf_dir_read_more: Failed to allocate buffer!\n"));
                return -ENOMEM;
            }
        }
    }

    /*
     * Allocate a request buffer.
     */
    pReq = (VBOXSFLISTDIRREQ *)VbglR0PhysHeapAlloc(sizeof(*pReq));
    if (pReq) {
        rc = VbglR0SfHostReqListDirContig2x(sf_g->map.root, pReq, sf_d->Handle.hHost, NULL, NIL_RTGCPHYS64,
                                            fRestart ? SHFL_LIST_RESTART : SHFL_LIST_NONE,
                                            sf_d->pBuf, virt_to_phys(sf_d->pBuf), sf_d->cbBuf);
        if (RT_SUCCESS(rc)) {
            sf_d->pEntry       = sf_d->pBuf;
            sf_d->cbValid      = pReq->Parms.cb32Buffer.u.value32;
            sf_d->cEntriesLeft = pReq->Parms.c32Entries.u.value32;
            sf_d->fNoMoreFiles = pReq->Parms.f32More.u.value32 == 0;
        } else {
            sf_d->pEntry       = sf_d->pBuf;
            sf_d->cbValid      = 0;
            sf_d->cEntriesLeft = 0;
            if (rc == VERR_NO_MORE_FILES) {
                sf_d->fNoMoreFiles = true;
                rc = 0;
            } else {
                /* In theory we could end up here with a buffer overflow, but
                   with a 4KB minimum buffer size that's very unlikely with the
                   typical filename length of today's file systems (2019). */
                LogRelMax(16, ("vbsf_dir_read_more: VbglR0SfHostReqListDirContig2x -> %Rrc\n", rc));
                rc = -EPROTO;
            }
        }
        VbglR0PhysHeapFree(pReq);
    } else
        rc = -ENOMEM;
    return rc;
}

/**
 * Helper function for when we need to convert the name, avoids wasting stack in
 * the UTF-8 code path.
 */
DECL_NO_INLINE(static, bool) vbsf_dir_emit_nls(
# if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
                                               struct dir_context *ctx,
# else
                                               void *opaque, filldir_t filldir, loff_t offPos,
# endif
                                               const char *pszSrcName, uint16_t cchSrcName, ino_t d_ino, int d_type,
                                               struct vbsf_super_info *sf_g)
{
    char szDstName[NAME_MAX];
    int rc = vbsf_nlscpy(sf_g, szDstName, sizeof(szDstName), pszSrcName, cchSrcName + 1);
    if (rc == 0) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
        return dir_emit(ctx, szDstName, strlen(szDstName), d_ino, d_type);
#else
        return filldir(opaque, szDstName, strlen(szDstName), offPos, d_ino, d_type) == 0;
#endif
    }
    /* Assuming this is a buffer overflow issue, just silently skip it. */
    return true;
}

#else /* VBSF_BUFFER_DIRS */

/**
 * Extract element ([dir]->f_pos) from the directory [dir] into [d_name].
 *
 * @returns 0 for success, 1 for end reached, Linux error code otherwise.
 */
static int vbsf_getdent(struct file *dir, char d_name[NAME_MAX], int *d_type)
{
    loff_t cur;
    struct vbsf_super_info *sf_g;
    struct vbsf_dir_info *sf_d;
    struct vbsf_inode_info *sf_i;
    struct inode *inode;
    struct list_head *pos, *list;

    TRACE();

    inode = VBSF_GET_F_DENTRY(dir)->d_inode;
    sf_i = VBSF_GET_INODE_INFO(inode);
    sf_g = VBSF_GET_SUPER_INFO(inode->i_sb);
    sf_d = dir->private_data;

    BUG_ON(!sf_g);
    BUG_ON(!sf_d);
    BUG_ON(!sf_i);

    if (sf_i->force_reread) {
        struct dentry *dentry = VBSF_GET_F_DENTRY(dir);
        int err = vbsf_dir_open_worker(dentry, inode, sf_i, sf_d, sf_g, "vbsf_getdent");
        if (!err) {
            sf_i->force_reread = false;
        } else {
            if (err == -ENOENT) {
                vbsf_dir_info_free(sf_d);
                dir->private_data = NULL;
            }
            return err;
        }
    }

    cur = 0;
    list = &sf_d->info_list;
    list_for_each(pos, list) {
        struct vbsf_dir_buf *b;
        SHFLDIRINFO *info;
        loff_t i;

        b = list_entry(pos, struct vbsf_dir_buf, head);
        if (dir->f_pos >= cur + b->cEntries) {
            cur += b->cEntries;
            continue;
        }

        for (i = 0, info = b->buf; i < dir->f_pos - cur; ++i) {
            size_t size;

            size = offsetof(SHFLDIRINFO, name.String)
                 + info->name.u16Size;
            info = (SHFLDIRINFO *)((uintptr_t)info + size);
        }

        *d_type = vbsf_get_d_type(info->Info.Attr.fMode);

        return vbsf_nlscpy(sf_g, d_name, NAME_MAX, info->name.String.utf8, info->name.u16Length);
    }

    return 1;
}
#endif

/**
 * This is called when vfs wants to populate internal buffers with
 * directory [dir]s contents. [opaque] is an argument to the
 * [filldir]. [filldir] magically modifies it's argument - [opaque]
 * and takes following additional arguments (which i in turn get from
 * the host via vbsf_getdent):
 *
 * name : name of the entry (i must also supply it's length huh?)
 * type : type of the entry (FILE | DIR | etc) (i ellect to use DT_UNKNOWN)
 * pos : position/index of the entry
 * ino : inode number of the entry (i fake those)
 *
 * [dir] contains:
 * f_pos : cursor into the directory listing
 * private_data : mean of communication with the host side
 *
 * Extract elements from the directory listing (incrementing f_pos
 * along the way) and feed them to [filldir] until:
 *
 * a. there are no more entries (i.e. vbsf_getdent set done to 1)
 * b. failure to compute fake inode number
 * c. filldir returns an error (see comment on that)
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
static int vbsf_dir_iterate(struct file *dir, struct dir_context *ctx)
#else
static int vbsf_dir_read(struct file *dir, void *opaque, filldir_t filldir)
#endif
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
    loff_t                  offPos  = ctx->pos;
#else
    loff_t                  offPos  = dir->f_pos;
#endif
#ifndef VBSF_BUFFER_DIRS
    struct vbsf_dir_info   *sf_d    = (struct vbsf_dir_info *)dir->private_data;
    struct vbsf_super_info *sf_g    = VBSF_GET_SUPER_INFO(VBSF_GET_F_DENTRY(dir)->d_sb);

    /*
     * Any seek performed in the mean time?
     */
    if (offPos == sf_d->offPos) {
        /* likely */
    } else {
        int rc;

        /* Restart the search if iPos is lower than the current buffer position. */
        loff_t offCurEntry = sf_d->offPos;
        if (offPos < offCurEntry) {
            rc = vbsf_dir_read_more(sf_d, sf_g, true /*fRestart*/);
            if (rc == 0)
                offCurEntry = 0;
            else
                return rc;
        }

        /* Skip ahead to offPos. */
        while (offCurEntry < offPos) {
            uint32_t cEntriesLeft = sf_d->cEntriesLeft;
            if ((uint64_t)(offPos - offCurEntry) >= cEntriesLeft) {
                /* Skip the current buffer and read the next: */
                offCurEntry       += cEntriesLeft;
                sf_d->offPos       = offCurEntry;
                sf_d->cEntriesLeft = 0;
                rc = vbsf_dir_read_more(sf_d, sf_g, false /*fRestart*/);
                if (rc != 0 || sf_d->cEntriesLeft == 0)
                    return rc;
            } else {
                do
                {
                    PSHFLDIRINFO pEntry = sf_d->pEntry;
                    pEntry = (PSHFLDIRINFO)&pEntry->name.String.utf8[pEntry->name.u16Length];
                    AssertLogRelBreakStmt(   cEntriesLeft == 1
                                          ||    (uintptr_t)pEntry - (uintptr_t)sf_d->pBuf
                                             <= sf_d->cbValid - RT_UOFFSETOF(SHFLDIRINFO, name.String),
                                          sf_d->cEntriesLeft = 0);
                    sf_d->cEntriesLeft  = --cEntriesLeft;
                    sf_d->offPos        = ++offCurEntry;
                } while (offPos < sf_d->offPos);
            }
        }
    }

    /*
     * Handle '.' and '..' specially so we get the inode numbers right.
     * We'll skip any '.' or '..' returned by the host (included in pos,
     * however, to simplify the above skipping code).
     */
    if (offPos < 2) {
# if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
        if (offPos == 0) {
            if (dir_emit_dot(dir, ctx))
                dir->f_pos = ctx->pos = sf_d->offPos = offPos = 1;
            else
                return 0;
        }
        if (offPos == 1) {
            if (dir_emit_dotdot(dir, ctx))
                dir->f_pos = ctx->pos = sf_d->offPos = offPos = 2;
            else
                return 0;
        }
# else
        if (offPos == 0) {
            int rc = filldir(opaque, ".", 1, 0, VBSF_GET_F_DENTRY(dir)->d_inode->i_ino, DT_DIR);
            if (!rc)
                dir->f_pos = sf_d->offPos = offPos = 1;
            else
                return 0;
        }
        if (offPos == 1) {
#  if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 5)
            int rc = filldir(opaque, "..", 2, 1, parent_ino(VBSF_GET_F_DENTRY(dir)), DT_DIR);
#  else
            int rc = filldir(opaque, "..", 2, 1, VBSF_GET_F_DENTRY(dir)->d_parent->d_inode->i_ino, DT_DIR);
#  endif
            if (!rc)
                dir->f_pos = sf_d->offPos = offPos = 2;
            else
                return 0;
        }
# endif
    }

    /*
     * Produce stuff.
     */
    Assert(offPos == sf_d->offPos);
    for (;;) {
        PSHFLDIRINFO pBuf;
        PSHFLDIRINFO pEntry;

        /*
         * Do we need to read more?
         */
        uint32_t cbValid      = sf_d->cbValid;
        uint32_t cEntriesLeft = sf_d->cEntriesLeft;
        if (!cEntriesLeft) {
            int rc = vbsf_dir_read_more(sf_d, sf_g, false /*fRestart*/);
            if (rc == 0) {
                cEntriesLeft = sf_d->cEntriesLeft;
                if (!cEntriesLeft)
                    return 0;
                cbValid = sf_d->cbValid;
            }
            else
                return rc;
        }

        /*
         * Feed entries to the caller.
         */
        pBuf   = sf_d->pBuf;
        pEntry = sf_d->pEntry;
        do {
            /*
             * Validate the entry in case the host is messing with us.
             * We're ASSUMING the host gives us a zero terminated string (UTF-8) here.
             */
            uintptr_t const offEntryInBuf = (uintptr_t)pEntry - (uintptr_t)pBuf;
            uint16_t        cbSrcName;
            uint16_t        cchSrcName;
            AssertLogRelMsgBreak(offEntryInBuf + RT_UOFFSETOF(SHFLDIRINFO, name.String) <= cbValid,
                                 ("%#llx + %#x vs %#x\n", offEntryInBuf, RT_UOFFSETOF(SHFLDIRINFO, name.String), cbValid));
            cbSrcName  = pEntry->name.u16Size;
            cchSrcName = pEntry->name.u16Length;
            AssertLogRelBreak(offEntryInBuf + RT_UOFFSETOF(SHFLDIRINFO, name.String) + cbSrcName <= cbValid);
            AssertLogRelBreak(cchSrcName < cbSrcName);
            AssertLogRelBreak(pEntry->name.String.ach[cchSrcName] == '\0');

            /*
             * Filter out '.' and '..' entires.
             */
            if (   cchSrcName > 2
                || pEntry->name.String.ach[0] != '.'
                || (   cchSrcName == 2
                    && pEntry->name.String.ach[1] != '.')) {
                int const   d_type = vbsf_get_d_type(pEntry->Info.Attr.fMode);
                ino_t const d_ino  = (ino_t)offPos + 0xbeef; /* very fake */
                bool        fContinue;
                if (sf_g->fNlsIsUtf8) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
                    fContinue = dir_emit(ctx, pEntry->name.String.ach, cchSrcName, d_ino, d_type);
#else
                    fContinue = filldir(opaque, pEntry->name.String.ach, cchSrcName, offPos, d_ino, d_type) == 0;
#endif
                } else {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
                    fContinue = vbsf_dir_emit_nls(ctx, pEntry->name.String.ach, cchSrcName, d_ino, d_type, sf_g);
#else
                    fContinue = vbsf_dir_emit_nls(opaque, filldir, offPos, pEntry->name.String.ach, cchSrcName,
                                                  d_ino, d_type, sf_g);
#endif
                }
                if (fContinue) {
                    /* likely */
                } else  {
                    sf_d->cEntriesLeft = cEntriesLeft;
                    sf_d->pEntry       = pEntry;
                    sf_d->offPos       = offPos;
                    return 0;
                }
            }

            /*
             * Advance to the next entry.
             */
            pEntry        = (PSHFLDIRINFO)((uintptr_t)pEntry + RT_UOFFSETOF(SHFLDIRINFO, name.String) + cbSrcName);
            offPos       += 1;
            dir->f_pos    = offPos;
# if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
            ctx->pos      = offPos;
# endif
            cEntriesLeft -= 1;
        } while (cEntriesLeft > 0);

        /* Done with all available entries. */
        sf_d->offPos       = offPos + cEntriesLeft;
        sf_d->pEntry       = pBuf;
        sf_d->cEntriesLeft = 0;
    }

#else /* !VBSF_BUFFER_DIRS */

    TRACE();
    for (;;) {
        int err;
        ino_t fake_ino;
        loff_t sanity;
        char d_name[NAME_MAX];
        int d_type = DT_UNKNOWN;

        err = vbsf_getdent(dir, d_name, &d_type);
        switch (err) {
            case 1:
                return 0;

            case 0:
                break;

            case -1:
            default:
                /* skip erroneous entry and proceed */
                LogFunc(("vbsf_getdent error %d\n", err));
                offPos++;
                dir->f_pos = offPos;
# if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
                ctx->pos  = offPos;
# endif
                continue;
        }

        /* d_name now contains a valid entry name */

        sanity = offPos + 0xbeef;
        fake_ino = sanity;
        if (sanity - fake_ino) {
            LogRelFunc(("can not compute ino\n"));
            return -EINVAL;
        }
# if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
        if (!dir_emit(ctx, d_name, strlen(d_name), fake_ino, d_type)) {
            LogFunc(("dir_emit failed\n"));
            return 0;
        }
# else
        err = filldir(opaque, d_name, strlen(d_name), dir->f_pos, fake_ino, d_type);
        if (err) {
            LogFunc(("filldir returned error %d\n", err));
            /* Rely on the fact that filldir returns error
               only when it runs out of space in opaque */
            return 0;
        }
# endif

        offPos++;
        dir->f_pos = offPos;
# if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
        ctx->pos   = offPos;
# endif
    }

    BUG();
#endif
}

/**
 * Directory file operations.
 */
struct file_operations vbsf_dir_fops = {
    .open = vbsf_dir_open,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 7, 0)
    .iterate = vbsf_dir_iterate, /** @todo Consider .iterate_shared (shared vs exclusive inode lock) here.  Need to consider
                                  * whether struct vbsf_dir_info is safe in multithreaded apps doing readdir in parallel on
                                  * the same handle.  */
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
    .iterate = vbsf_dir_iterate,
#else
    .readdir = vbsf_dir_read,
#endif
    .release = vbsf_dir_release,
    .read = generic_read_dir
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37)
    , .llseek = generic_file_llseek
#endif
};

/* iops */

/**
 * Worker for vbsf_inode_lookup() and vbsf_inode_instantiate().
 */
static struct inode *vbsf_create_inode(struct inode *parent, struct dentry *dentry, PSHFLSTRING path,
                                       PSHFLFSOBJINFO pObjInfo, struct vbsf_super_info *sf_g, bool fInstantiate)
{
    /*
     * Allocate memory for our additional inode info and create an inode.
     */
    struct vbsf_inode_info *sf_new_i = (struct vbsf_inode_info *)kmalloc(sizeof(*sf_new_i), GFP_KERNEL);
    if (sf_new_i) {
        ino_t         iNodeNo = iunique(parent->i_sb, 16);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 25)
        struct inode *pInode  = iget_locked(parent->i_sb, iNodeNo);
#else
        struct inode *pInode  = iget(parent->i_sb, iNodeNo);
#endif
        if (pInode) {
            /*
             * Initialize the two structures.
             */
#ifdef VBOX_STRICT
            sf_new_i->u32Magic      = SF_INODE_INFO_MAGIC;
#endif
            sf_new_i->path          = path;
            sf_new_i->force_restat  = false;
#ifdef VBSF_BUFFER_DIRS
            sf_new_i->force_reread  = false;
#endif
            sf_new_i->ts_up_to_date = jiffies;
            RTListInit(&sf_new_i->HandleList);
            sf_new_i->handle        = SHFL_HANDLE_NIL;

            VBSF_SET_INODE_INFO(pInode, sf_new_i);
            vbsf_init_inode(pInode, sf_new_i, pObjInfo, sf_g);

            /*
             * Before we unlock the new inode, we may need to call d_instantiate.
             */
            if (fInstantiate)
                d_instantiate(dentry, pInode);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 25)
            unlock_new_inode(pInode);
#endif
            return pInode;

        }
        LogFunc(("iget failed\n"));
        kfree(sf_new_i);
    } else
        LogRelFunc(("could not allocate memory for new inode info\n"));
    return NULL;
}

/**
 * This is called when vfs failed to locate dentry in the cache. The
 * job of this function is to allocate inode and link it to dentry.
 * [dentry] contains the name to be looked in the [parent] directory.
 * Failure to locate the name is not a "hard" error, in this case NULL
 * inode is added to [dentry] and vfs should proceed trying to create
 * the entry via other means. NULL(or "positive" pointer) ought to be
 * returned in case of success and "negative" pointer on error
 */
static struct dentry *vbsf_inode_lookup(struct inode *parent, struct dentry *dentry
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)
                                        , unsigned int flags
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
                                        , struct nameidata *nd
#endif
                                        )
{
    struct vbsf_super_info *sf_g = VBSF_GET_SUPER_INFO(parent->i_sb);
    struct vbsf_inode_info *sf_i = VBSF_GET_INODE_INFO(parent);
    SHFLSTRING             *path;
    struct dentry          *dret;
    int                     rc;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)
    SFLOGFLOW(("vbsf_inode_lookup: parent=%p dentry=%p flags=%#x\n", parent, dentry, flags));
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
    SFLOGFLOW(("vbsf_inode_lookup: parent=%p dentry=%p nd=%p{.flags=%#x}\n", parent, dentry, nd, nd ? nd->flags : 0));
#else
    SFLOGFLOW(("vbsf_inode_lookup: parent=%p dentry=%p\n", parent, dentry));
#endif

    Assert(sf_g);
    Assert(sf_i && sf_i->u32Magic == SF_INODE_INFO_MAGIC);

    /*
     * Build the path.  We'll associate the path with dret's inode on success.
     */
    rc = vbsf_path_from_dentry(__func__, sf_g, sf_i, dentry, &path);
    if (rc == 0) {
        /*
         * Do a lookup on the host side.
         */
        VBOXSFCREATEREQ *pReq = (VBOXSFCREATEREQ *)VbglR0PhysHeapAlloc(sizeof(*pReq) + path->u16Size);
        if (pReq) {
            struct inode *pInode = NULL;

            RT_ZERO(*pReq);
            memcpy(&pReq->StrPath, path, SHFLSTRING_HEADER_SIZE + path->u16Size);
            pReq->CreateParms.Handle = SHFL_HANDLE_NIL;
            pReq->CreateParms.CreateFlags = SHFL_CF_LOOKUP | SHFL_CF_ACT_FAIL_IF_NEW;

            LogFunc(("Calling VbglR0SfHostReqCreate on %s\n", path->String.utf8));
            rc = VbglR0SfHostReqCreate(sf_g->map.root, pReq);
            if (RT_SUCCESS(rc)) {
                if (pReq->CreateParms.Result == SHFL_FILE_EXISTS) {
                    /*
                     * Create an inode for the result.  Since this also confirms
                     * the existence of all parent dentries, we increase their TTL.
                     */
                    pInode = vbsf_create_inode(parent, dentry, path, &pReq->CreateParms.Info, sf_g, false /*fInstantiate*/);
                    if (rc == 0) {
                        path = NULL; /* given to the inode */
                        dret = dentry;
                    } else
                        dret = (struct dentry *)ERR_PTR(-ENOMEM);
                    vbsf_dentry_chain_increase_parent_ttl(dentry);
                } else if (   pReq->CreateParms.Result == SHFL_FILE_NOT_FOUND
                       || pReq->CreateParms.Result == SHFL_PATH_NOT_FOUND /*this probably should happen*/) {
                    dret = dentry;
                } else {
                    AssertMsgFailed(("%d\n", pReq->CreateParms.Result));
                    dret = (struct dentry *)ERR_PTR(-EPROTO);
                }
            } else if (rc == VERR_INVALID_NAME) {
                dret = dentry; /* this can happen for names like 'foo*' on a Windows host */
            } else {
                LogFunc(("VbglR0SfHostReqCreate failed on %s: %Rrc\n", path->String.utf8, rc));
                dret = (struct dentry *)ERR_PTR(-EPROTO);
            }
            VbglR0PhysHeapFree(pReq);

            /*
             * When dret is set to dentry we got something to insert,
             * though it may be negative (pInode == NULL).
             */
            if (dret == dentry) {
                vbsf_dentry_set_update_jiffies(dentry, jiffies);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 38)
                Assert(dentry->d_op == &vbsf_dentry_ops); /* (taken from the superblock) */
#else
                dentry->d_op = &vbsf_dentry_ops;
#endif
                d_add(dentry, pInode);
                dret = NULL;
            }
        } else
            dret = (struct dentry *)ERR_PTR(-ENOMEM);
        if (path)
            kfree(path);
    } else
        dret = (struct dentry *)ERR_PTR(rc);
    return dret;
}

/**
 * This should allocate memory for vbsf_inode_info, compute a unique inode
 * number, get an inode from vfs, initialize inode info, instantiate
 * dentry.
 *
 * @param parent        inode entry of the directory
 * @param dentry        directory cache entry
 * @param path          path name.  Consumed on success.
 * @param info          file information
 * @param handle        handle
 * @returns 0 on success, Linux error code otherwise
 */
static int vbsf_inode_instantiate(struct inode *parent, struct dentry *dentry, PSHFLSTRING path,
                                  PSHFLFSOBJINFO info, SHFLHANDLE handle)
{
    struct vbsf_super_info *sf_g   = VBSF_GET_SUPER_INFO(parent->i_sb);
    struct inode           *pInode = vbsf_create_inode(parent, dentry, path, info, sf_g, true /*fInstantiate*/);
    if (pInode) {
        /* Store this handle if we leave the handle open. */
        struct vbsf_inode_info *sf_new_i = VBSF_GET_INODE_INFO(pInode);
        sf_new_i->handle = handle;
        return 0;
    }
    return -ENOMEM;
}

/**
 * Create a new regular file / directory.
 *
 * @param parent        inode of the directory
 * @param dentry        directory cache entry
 * @param mode          file mode
 * @param fDirectory    true if directory, false otherwise
 * @returns 0 on success, Linux error code otherwise
 */
static int vbsf_create_worker(struct inode *parent, struct dentry *dentry, umode_t mode, int fDirectory)
{
    int rc, err;
    struct vbsf_inode_info *sf_parent_i = VBSF_GET_INODE_INFO(parent);
    struct vbsf_super_info *sf_g = VBSF_GET_SUPER_INFO(parent->i_sb);
    SHFLSTRING *path;
    union CreateAuxReq
    {
        VBOXSFCREATEREQ Create;
        VBOXSFCLOSEREQ  Close;
    } *pReq;

    TRACE();
    BUG_ON(!sf_parent_i);
    BUG_ON(!sf_g);

    err = vbsf_path_from_dentry(__func__, sf_g, sf_parent_i, dentry, &path);
    if (err)
        goto fail0;

    /** @todo combine with vbsf_path_from_dentry? */
    pReq = (union CreateAuxReq *)VbglR0PhysHeapAlloc(RT_UOFFSETOF(VBOXSFCREATEREQ, StrPath.String) + path->u16Size);
    if (pReq) {
        memcpy(&pReq->Create.StrPath, path, SHFLSTRING_HEADER_SIZE + path->u16Size);
    } else {
        err = -ENOMEM;
        goto fail1;
    }

    RT_ZERO(pReq->Create.CreateParms);
    pReq->Create.CreateParms.Handle      = SHFL_HANDLE_NIL;
    pReq->Create.CreateParms.CreateFlags = SHFL_CF_ACT_CREATE_IF_NEW
                                         | SHFL_CF_ACT_FAIL_IF_EXISTS
                                         | SHFL_CF_ACCESS_READWRITE
                                         | (fDirectory ? SHFL_CF_DIRECTORY : 0);
/** @todo use conversion function from utils.c here!   */
    pReq->Create.CreateParms.Info.Attr.fMode = (fDirectory ? RTFS_TYPE_DIRECTORY : RTFS_TYPE_FILE) | (mode & S_IRWXUGO);
    pReq->Create.CreateParms.Info.Attr.enmAdditional = RTFSOBJATTRADD_NOTHING;

    LogFunc(("calling VbglR0SfHostReqCreate, folder %s, flags %#x\n", path->String.ach, pReq->Create.CreateParms.CreateFlags));
    rc = VbglR0SfHostReqCreate(sf_g->map.root, &pReq->Create);
    if (RT_FAILURE(rc)) {
        if (rc == VERR_WRITE_PROTECT) {
            err = -EROFS;
            goto fail2;
        }
        err = -EPROTO;
        LogFunc(("(%d): SHFL_FN_CREATE(%s) failed rc=%Rrc\n",
             fDirectory, sf_parent_i->path->String.utf8, rc));
        goto fail2;
    }

    if (pReq->Create.CreateParms.Result != SHFL_FILE_CREATED) {
        err = -EPERM;
        LogFunc(("(%d): could not create file %s result=%d\n",
             fDirectory, sf_parent_i->path->String.utf8, pReq->Create.CreateParms.Result));
        goto fail2;
    }

    vbsf_dentry_chain_increase_parent_ttl(dentry);

    err = vbsf_inode_instantiate(parent, dentry, path, &pReq->Create.CreateParms.Info,
                                 fDirectory ? SHFL_HANDLE_NIL : pReq->Create.CreateParms.Handle);
    if (err) {
        LogFunc(("(%d): could not instantiate dentry for %s err=%d\n", fDirectory, path->String.utf8, err));
        goto fail3;
    }

    /*
     * Don't close this handle right now. We assume that the same file is
     * opened with vbsf_reg_open() and later closed with sf_reg_close(). Save
     * the handle in between. Does not apply to directories. True?
     */
    if (fDirectory) {
        AssertCompile(RTASSERT_OFFSET_OF(VBOXSFCREATEREQ, CreateParms.Handle) > sizeof(VBOXSFCLOSEREQ)); /* no aliasing issues */
        rc = VbglR0SfHostReqClose(sf_g->map.root, &pReq->Close, pReq->Create.CreateParms.Handle);
        if (RT_FAILURE(rc))
            LogFunc(("(%d): VbglR0SfHostReqClose failed rc=%Rrc\n", fDirectory, rc));
    }

    sf_parent_i->force_restat = 1;
    VbglR0PhysHeapFree(pReq);
    return 0;

 fail3:
    rc = VbglR0SfHostReqClose(sf_g->map.root, &pReq->Close, pReq->Create.CreateParms.Handle);
    if (RT_FAILURE(rc))
        LogFunc(("(%d): VbglR0SfHostReqCloseSimple failed rc=%Rrc\n", fDirectory, rc));
 fail2:
    VbglR0PhysHeapFree(pReq);
 fail1:
    kfree(path);

 fail0:
    return err;
}

/**
 * Create a new regular file.
 *
 * @param parent        inode of the directory
 * @param dentry        directory cache entry
 * @param mode          file mode
 * @param excl          Possible O_EXCL...
 * @returns 0 on success, Linux error code otherwise
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0) || defined(DOXYGEN_RUNNING)
static int vbsf_inode_create(struct inode *parent, struct dentry *dentry, umode_t mode, bool excl)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 3, 0)
static int vbsf_inode_create(struct inode *parent, struct dentry *dentry, umode_t mode, struct nameidata *nd)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
static int vbsf_inode_create(struct inode *parent, struct dentry *dentry, int mode, struct nameidata *nd)
#else
static int vbsf_inode_create(struct inode *parent, struct dentry *dentry, int mode)
#endif
{
/** @todo @a nd (struct nameidata) contains intent with partial open flags for
 *        2.6.0-3.5.999.  In 3.6.0 atomic_open was introduced and stuff
 *        changed (investigate)... */
    TRACE();
    return vbsf_create_worker(parent, dentry, mode, 0 /*fDirectory*/);
}

/**
 * Create a new directory.
 *
 * @param parent        inode of the directory
 * @param dentry        directory cache entry
 * @param mode          file mode
 * @returns 0 on success, Linux error code otherwise
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 3, 0)
static int vbsf_inode_mkdir(struct inode *parent, struct dentry *dentry, umode_t mode)
#else
static int vbsf_inode_mkdir(struct inode *parent, struct dentry *dentry, int mode)
#endif
{
    TRACE();
    return vbsf_create_worker(parent, dentry, mode, 1 /*fDirectory*/);
}

/**
 * Remove a regular file / directory.
 *
 * @param parent        inode of the directory
 * @param dentry        directory cache entry
 * @param fDirectory    true if directory, false otherwise
 * @returns 0 on success, Linux error code otherwise
 */
static int vbsf_unlink_worker(struct inode *parent, struct dentry *dentry, int fDirectory)
{
    int rc, err;
    struct vbsf_super_info *sf_g = VBSF_GET_SUPER_INFO(parent->i_sb);
    struct vbsf_inode_info *sf_parent_i = VBSF_GET_INODE_INFO(parent);
    SHFLSTRING *path;

    TRACE();
    BUG_ON(!sf_g);

    err = vbsf_path_from_dentry(__func__, sf_g, sf_parent_i, dentry, &path);
    if (!err) {
        VBOXSFREMOVEREQ *pReq = (VBOXSFREMOVEREQ *)VbglR0PhysHeapAlloc(RT_UOFFSETOF(VBOXSFREMOVEREQ, StrPath.String)
                                           + path->u16Size);
        if (pReq) {
            memcpy(&pReq->StrPath, path, SHFLSTRING_HEADER_SIZE + path->u16Size);
            uint32_t fFlags = fDirectory ? SHFL_REMOVE_DIR : SHFL_REMOVE_FILE;
            if (dentry->d_inode && ((dentry->d_inode->i_mode & S_IFLNK) == S_IFLNK))
                fFlags |= SHFL_REMOVE_SYMLINK;

            rc = VbglR0SfHostReqRemove(sf_g->map.root, pReq, fFlags);

            if (dentry->d_inode) {
                struct vbsf_inode_info *sf_i = VBSF_GET_INODE_INFO(dentry->d_inode);
                sf_i->force_restat = true;
#ifdef VBSF_BUFFER_DIRS
                sf_i->force_reread = true;
#endif
            }

            if (RT_SUCCESS(rc)) {
                /* directory access/change time changed */
                sf_parent_i->force_restat = true;
#ifdef VBSF_BUFFER_DIRS
                /* directory content changed */
                sf_parent_i->force_reread = true;
#endif

                err = 0;
            } else if (rc == VERR_FILE_NOT_FOUND || rc == VERR_PATH_NOT_FOUND) {
                LogFunc(("(%d): VbglR0SfRemove(%s) failed rc=%Rrc; calling d_drop on %p\n",
                         fDirectory, path->String.utf8, rc, dentry));
                d_drop(dentry);
            } else {
                LogFunc(("(%d): VbglR0SfRemove(%s) failed rc=%Rrc\n", fDirectory, path->String.utf8, rc));
                err = -RTErrConvertToErrno(rc);
            }
            VbglR0PhysHeapFree(pReq);
        } else
            err = -ENOMEM;
        kfree(path);
    }
    return err;
}

/**
 * Remove a regular file.
 *
 * @param parent        inode of the directory
 * @param dentry        directory cache entry
 * @returns 0 on success, Linux error code otherwise
 */
static int vbsf_inode_unlink(struct inode *parent, struct dentry *dentry)
{
    TRACE();
    return vbsf_unlink_worker(parent, dentry, false /*fDirectory*/);
}

/**
 * Remove a directory.
 *
 * @param parent        inode of the directory
 * @param dentry        directory cache entry
 * @returns 0 on success, Linux error code otherwise
 */
static int vbsf_inode_rmdir(struct inode *parent, struct dentry *dentry)
{
    TRACE();
    return vbsf_unlink_worker(parent, dentry, true /*fDirectory*/);
}

/**
 * Rename a regular file / directory.
 *
 * @param old_parent    inode of the old parent directory
 * @param old_dentry    old directory cache entry
 * @param new_parent    inode of the new parent directory
 * @param new_dentry    new directory cache entry
 * @param flags         flags
 * @returns 0 on success, Linux error code otherwise
 */
static int vbsf_inode_rename(struct inode *old_parent, struct dentry *old_dentry,
                             struct inode *new_parent, struct dentry *new_dentry
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0)
                             , unsigned flags
#endif
                             )
{
    int err = 0, rc = VINF_SUCCESS;
    struct vbsf_super_info *sf_g = VBSF_GET_SUPER_INFO(old_parent->i_sb);

    TRACE();

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 9, 0)
    if (flags) {
        LogFunc(("rename with flags=%x\n", flags));
        return -EINVAL;
    }
#endif

    if (sf_g != VBSF_GET_SUPER_INFO(new_parent->i_sb)) {
        LogFunc(("rename with different roots\n"));
        err = -EINVAL;
    } else {
        struct vbsf_inode_info *sf_old_i = VBSF_GET_INODE_INFO(old_parent);
        struct vbsf_inode_info *sf_new_i = VBSF_GET_INODE_INFO(new_parent);
        /* As we save the relative path inside the inode structure, we need to change
           this if the rename is successful. */
        struct vbsf_inode_info *sf_file_i = VBSF_GET_INODE_INFO(old_dentry->d_inode);
        SHFLSTRING *old_path;
        SHFLSTRING *new_path;

        BUG_ON(!sf_old_i);
        BUG_ON(!sf_new_i);
        BUG_ON(!sf_file_i);

        old_path = sf_file_i->path;
        err = vbsf_path_from_dentry(__func__, sf_g, sf_new_i, new_dentry, &new_path);
        if (err)
            LogFunc(("failed to create new path\n"));
        else {
            VBOXSFRENAMEWITHSRCBUFREQ *pReq;
            pReq = (VBOXSFRENAMEWITHSRCBUFREQ *)VbglR0PhysHeapAlloc(RT_UOFFSETOF(VBOXSFRENAMEWITHSRCBUFREQ, StrDstPath.String)
                                                                    + new_path->u16Size);
            if (pReq) {
                memcpy(&pReq->StrDstPath, new_path, SHFLSTRING_HEADER_SIZE + new_path->u16Size);
                rc = VbglR0SfHostReqRenameWithSrcContig(sf_g->map.root, pReq,
                                                        old_path, virt_to_phys(old_path),
                                                        old_dentry->d_inode->i_mode & S_IFDIR ? SHFL_RENAME_DIR
                                                        : SHFL_RENAME_FILE | SHFL_RENAME_REPLACE_IF_EXISTS);
                VbglR0PhysHeapFree(pReq);
            } else
                rc = VERR_NO_MEMORY;
            if (RT_SUCCESS(rc)) {
                kfree(old_path);
                sf_new_i->force_restat = 1;
                sf_old_i->force_restat = 1; /* XXX: needed? */

                /* Set the new relative path in the inode. */
                sf_file_i->path = new_path;
            } else {
                LogFunc(("VbglR0SfRename failed rc=%Rrc\n",
                     rc));
                err = -RTErrConvertToErrno(rc);
                kfree(new_path);
            }
        }
    }
    return err;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
static int vbsf_ino_symlink(struct inode *parent, struct dentry *dentry, const char *symname)
{
    int err;
    int rc;
    struct vbsf_inode_info *sf_i;
    struct vbsf_super_info *sf_g;
    SHFLSTRING *path, *ssymname;
    SHFLFSOBJINFO info;
    int symname_len = strlen(symname) + 1;

    TRACE();
    sf_g = VBSF_GET_SUPER_INFO(parent->i_sb);
    sf_i = VBSF_GET_INODE_INFO(parent);

    BUG_ON(!sf_g);
    BUG_ON(!sf_i);

    err = vbsf_path_from_dentry(__func__, sf_g, sf_i, dentry, &path);
    if (err)
        goto fail0;

    ssymname = kmalloc(offsetof(SHFLSTRING, String.utf8) + symname_len, GFP_KERNEL);
    if (!ssymname) {
        LogRelFunc(("kmalloc failed, caller=sf_symlink\n"));
        err = -ENOMEM;
        goto fail1;
    }

    ssymname->u16Length = symname_len - 1;
    ssymname->u16Size = symname_len;
    memcpy(ssymname->String.utf8, symname, symname_len);

    rc = VbglR0SfSymlink(&g_SfClient, &sf_g->map, path, ssymname, &info);
    kfree(ssymname);

    if (RT_FAILURE(rc)) {
        if (rc == VERR_WRITE_PROTECT) {
            err = -EROFS;
            goto fail1;
        }
        LogFunc(("VbglR0SfSymlink(%s) failed rc=%Rrc\n", sf_i->path->String.utf8, rc));
        err = -EPROTO;
        goto fail1;
    }

    err = vbsf_inode_instantiate(parent, dentry, path, &info, SHFL_HANDLE_NIL);
    if (err) {
        LogFunc(("could not instantiate dentry for %s err=%d\n", sf_i->path->String.utf8, err));
        goto fail1;
    }

    sf_i->force_restat = 1;
    return 0;

 fail1:
    kfree(path);
 fail0:
    return err;
}
#endif /* LINUX_VERSION_CODE >= 2.6.0 */

struct inode_operations vbsf_dir_iops = {
    .lookup = vbsf_inode_lookup,
    .create = vbsf_inode_create,
    .mkdir = vbsf_inode_mkdir,
    .rmdir = vbsf_inode_rmdir,
    .unlink = vbsf_inode_unlink,
    .rename = vbsf_inode_rename,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
    .revalidate = vbsf_inode_revalidate
#else
    .getattr = vbsf_inode_getattr,
    .setattr = vbsf_inode_setattr,
    .symlink = vbsf_ino_symlink
#endif
};

