/* $Id$ */
/** @file
 * vboxsf - VBox Linux Shared Folders VFS, module init/term, super block management.
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

/**
 * @note Anyone wishing to make changes here might wish to take a look at
 * https://github.com/torvalds/linux/blob/master/Documentation/filesystems/vfs.txt
 * which seems to be the closest there is to official documentation on
 * writing filesystem drivers for Linux.
 *
 * See also: http://us1.samba.org/samba/ftp/cifs-cvs/ols2006-fs-tutorial-smf.odp
 */

/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "vfsmod.h"
#include "version-generated.h"
#include "revision-generated.h"
#include "product-generated.h"
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 3, 0)
# include <linux/mount.h>
#endif
#include <linux/seq_file.h>
#include <linux/vfs.h>
#include <linux/nfs_fs.h> /* for NFS_SUPER_MAGIC */
#include <iprt/path.h>


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
VBGLSFCLIENT client_handle;
VBGLSFCLIENT g_SfClient;      /* temporary? */

uint32_t g_fHostFeatures = 0; /* temporary? */

/** Protects all the sf_inode_info::HandleList lists. */
spinlock_t g_SfHandleLock;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 52)
static int g_fFollowSymlinks = 0;
#endif

/* forward declaration */
static struct super_operations sf_super_ops;



/**
 * Copies options from the mount info structure into @a sf_g.
 *
 * This is used both by sf_super_info_alloc() and sf_remount_fs().
 */
static void sf_super_info_copy_remount_options(struct vbsf_super_info *sf_g, struct vbsf_mount_info_new *info)
{
	sf_g->ttl_msec = info->ttl;
	if (info->ttl > 0)
		sf_g->ttl = msecs_to_jiffies(info->ttl);
	else if (info->ttl == 0 || info->ttl != -1)
		sf_g->ttl = sf_g->ttl_msec = 0;
	else
		sf_g->ttl = msecs_to_jiffies(VBSF_DEFAULT_TTL_MS);

	sf_g->uid = info->uid;
	sf_g->gid = info->gid;

	if ((unsigned)info->length >= RT_UOFFSETOF(struct vbsf_mount_info_new, tag)) {
		/* new fields */
		sf_g->dmode = info->dmode;
		sf_g->fmode = info->fmode;
		sf_g->dmask = info->dmask;
		sf_g->fmask = info->fmask;
	} else {
		sf_g->dmode = ~0;
		sf_g->fmode = ~0;
	}

	if ((unsigned)info->length >= RT_UOFFSETOF(struct vbsf_mount_info_new, cMaxIoPages)) {
		AssertCompile(sizeof(sf_g->tag) >= sizeof(info->tag));
		memcpy(sf_g->tag, info->tag, sizeof(info->tag));
		sf_g->tag[sizeof(sf_g->tag) - 1] = '\0';
	} else {
		sf_g->tag[0] = '\0';
	}

	/* The max number of pages in an I/O request.  This must take into
	   account that the physical heap generally grows in 64 KB chunks,
	   so we should not try push that limit.   It also needs to take
	   into account that the host will allocate temporary heap buffers
	   for the I/O bytes we send/receive, so don't push the host heap
	   too hard as we'd have to retry with smaller requests when this
	   happens, which isn't too efficient. */
	sf_g->cMaxIoPages = RT_MIN(_16K / sizeof(RTGCPHYS64) /* => 8MB buffer */,
				   VMMDEV_MAX_HGCM_DATA_SIZE >> PAGE_SHIFT);
	if (   (unsigned)info->length >= sizeof(struct vbsf_mount_info_new)
	    && info->cMaxIoPages != 0) {
		if (info->cMaxIoPages <= VMMDEV_MAX_HGCM_DATA_SIZE >> PAGE_SHIFT)
			sf_g->cMaxIoPages = info->cMaxIoPages;
		else
			printk(KERN_WARNING "vboxsf: max I/O page count (%#x) is out of range, using default (%#x) instead.\n",
			        info->cMaxIoPages, sf_g->cMaxIoPages);
	}
}

/* allocate super info, try to map host share */
static int sf_super_info_alloc(struct vbsf_mount_info_new *info, struct vbsf_super_info **sf_gp)
{
	int err, rc;
	SHFLSTRING *str_name;
	size_t name_len, str_len;
	struct vbsf_super_info *sf_g;

	TRACE();
	sf_g = kmalloc(sizeof(*sf_g), GFP_KERNEL);
	if (!sf_g) {
		err = -ENOMEM;
		LogRelFunc(("could not allocate memory for super info\n"));
		goto fail0;
	}

	RT_ZERO(*sf_g);

	if (info->nullchar != '\0'
	    || info->signature[0] != VBSF_MOUNT_SIGNATURE_BYTE_0
	    || info->signature[1] != VBSF_MOUNT_SIGNATURE_BYTE_1
	    || info->signature[2] != VBSF_MOUNT_SIGNATURE_BYTE_2) {
		err = -EINVAL;
		goto fail1;
	}

	info->name[sizeof(info->name) - 1] = 0;
	info->nls_name[sizeof(info->nls_name) - 1] = 0;

	name_len = strlen(info->name);
	str_len = offsetof(SHFLSTRING, String.utf8) + name_len + 1;
	str_name = kmalloc(str_len, GFP_KERNEL);
	if (!str_name) {
		err = -ENOMEM;
		LogRelFunc(("could not allocate memory for host name\n"));
		goto fail1;
	}

	str_name->u16Length = name_len;
	str_name->u16Size = name_len + 1;
	memcpy(str_name->String.utf8, info->name, name_len + 1);

#define _IS_UTF8(_str)  (strcmp(_str, "utf8") == 0)
#define _IS_EMPTY(_str) (strcmp(_str, "") == 0)

	/* Check if NLS charset is valid and not points to UTF8 table */
	if (info->nls_name[0]) {
		if (_IS_UTF8(info->nls_name))
			sf_g->nls = NULL;
		else {
			sf_g->nls = load_nls(info->nls_name);
			if (!sf_g->nls) {
				err = -EINVAL;
				LogFunc(("failed to load nls %s\n",
					 info->nls_name));
				kfree(str_name);
				goto fail1;
			}
		}
	} else {
#ifdef CONFIG_NLS_DEFAULT
		/* If no NLS charset specified, try to load the default
		 * one if it's not points to UTF8. */
		if (!_IS_UTF8(CONFIG_NLS_DEFAULT)
		    && !_IS_EMPTY(CONFIG_NLS_DEFAULT))
			sf_g->nls = load_nls_default();
		else
			sf_g->nls = NULL;
#else
		sf_g->nls = NULL;
#endif
	}

#undef _IS_UTF8
#undef _IS_EMPTY

	rc = VbglR0SfHostReqMapFolderWithContigSimple(str_name, virt_to_phys(str_name), RTPATH_DELIMITER,
						      true /*fCaseSensitive*/, &sf_g->map.root);
	kfree(str_name);

	if (RT_FAILURE(rc)) {
		err = -EPROTO;
		LogFunc(("SHFL_FN_MAP_FOLDER failed rc=%Rrc\n", rc));
		goto fail2;
	}

	/* The rest is shared with remount. */
	sf_super_info_copy_remount_options(sf_g, info);

	*sf_gp = sf_g;
	return 0;

 fail2:
	if (sf_g->nls)
		unload_nls(sf_g->nls);

 fail1:
	kfree(sf_g);

 fail0:
	return err;
}

/* unmap the share and free super info [sf_g] */
static void sf_super_info_free(struct vbsf_super_info *sf_g)
{
	int rc;

	TRACE();
	rc = VbglR0SfHostReqUnmapFolderSimple(sf_g->map.root);
	if (RT_FAILURE(rc))
		LogFunc(("VbglR0SfHostReqUnmapFolderSimple failed rc=%Rrc\n", rc));

	if (sf_g->nls)
		unload_nls(sf_g->nls);

	kfree(sf_g);
}


/**
 * Initialize backing device related matters.
 */
static int sf_init_backing_dev(struct super_block *sb, struct vbsf_super_info *sf_g)
{
	int rc = 0;
/** @todo this needs sorting out between 3.19 and 4.11   */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0) //&& LINUX_VERSION_CODE <= KERNEL_VERSION(3, 19, 0)
	/* Each new shared folder map gets a new uint64_t identifier,
	 * allocated in sequence.  We ASSUME the sequence will not wrap. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 26)
	static uint64_t s_u64Sequence = 0;
	uint64_t u64CurrentSequence = ASMAtomicIncU64(&s_u64Sequence);
#endif
	struct backing_dev_info *bdi;

#  if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 11, 0)
	rc = super_setup_bdi_name(sb, "vboxsf-%llu", (unsigned long long)u64CurrentSequence);
	if (!rc)
		bdi = sb->s_bdi;
	else
		return rc;
#  else
	bdi = &sf_g->bdi;
#  endif

	bdi->ra_pages = 0;                      /* No readahead */

# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 12)
	bdi->capabilities = 0
#  ifdef BDI_CAP_MAP_DIRECT
		          | BDI_CAP_MAP_DIRECT  /* MAP_SHARED */
#  endif
#  ifdef BDI_CAP_MAP_COPY
	                  | BDI_CAP_MAP_COPY    /* MAP_PRIVATE */
#  endif
#  ifdef BDI_CAP_READ_MAP
	                  | BDI_CAP_READ_MAP    /* can be mapped for reading */
#  endif
#  ifdef BDI_CAP_WRITE_MAP
	                  | BDI_CAP_WRITE_MAP   /* can be mapped for writing */
#  endif
#  ifdef BDI_CAP_EXEC_MAP
	                  | BDI_CAP_EXEC_MAP    /* can be mapped for execution */
#  endif
#  ifdef BDI_CAP_STRICTLIMIT
	                  | BDI_CAP_STRICTLIMIT;
#  endif
			  ;
#  ifdef BDI_CAP_STRICTLIMIT
	/* Smalles possible amount of dirty pages: %1 of RAM */
	bdi_set_max_ratio(bdi, 1);
#  endif
# endif	/* >= 2.6.12 */

# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24) && LINUX_VERSION_CODE < KERNEL_VERSION(4, 11, 0)
	rc = bdi_init(&sf_g->bdi);
#  if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 26)
	if (!rc)
		rc = bdi_register(&sf_g->bdi, NULL, "vboxsf-%llu",
				  (unsigned long long)u64CurrentSequence);
#  endif /* >= 2.6.26 */
# endif	 /* >= 2.6.24 */
#endif   /* >= 2.6.0 */
	return rc;
}


/**
 * Undoes what sf_init_backing_dev did.
 */
static void sf_done_backing_dev(struct super_block *sb, struct vbsf_super_info *sf_g)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24) && LINUX_VERSION_CODE <= KERNEL_VERSION(3, 19, 0)
	bdi_destroy(&sf_g->bdi);	/* includes bdi_unregister() */
#endif
}


/**
 * This is called by sf_read_super_24() and sf_read_super_26() when vfs mounts
 * the fs and wants to read super_block.
 *
 * Calls sf_super_info_alloc() to map the folder and allocate super information
 * structure.
 *
 * Initializes @a sb, initializes root inode and dentry.
 *
 * Should respect @a flags.
 */
static int sf_read_super_aux(struct super_block *sb, void *data, int flags)
{
	int err;
	struct dentry *droot;
	struct inode *iroot;
	struct sf_inode_info *sf_i;
	struct vbsf_super_info *sf_g;
	SHFLFSOBJINFO fsinfo;
	struct vbsf_mount_info_new *info;
	bool fInodePut = true;

	TRACE();
	if (!data) {
		LogFunc(("no mount info specified\n"));
		return -EINVAL;
	}

	info = data;

	if (flags & MS_REMOUNT) {
		LogFunc(("remounting is not supported\n"));
		return -ENOSYS;
	}

	err = sf_super_info_alloc(info, &sf_g);
	if (err)
		goto fail0;

	sf_i = kmalloc(sizeof(*sf_i), GFP_KERNEL);
	if (!sf_i) {
		err = -ENOMEM;
		LogRelFunc(("could not allocate memory for root inode info\n"));
		goto fail1;
	}

	sf_i->handle = SHFL_HANDLE_NIL;
	sf_i->path = kmalloc(sizeof(SHFLSTRING) + 1, GFP_KERNEL);
	if (!sf_i->path) {
		err = -ENOMEM;
		LogRelFunc(("could not allocate memory for root inode path\n"));
		goto fail2;
	}

	sf_i->path->u16Length = 1;
	sf_i->path->u16Size = 2;
	sf_i->path->String.utf8[0] = '/';
	sf_i->path->String.utf8[1] = 0;
	sf_i->force_reread = 0;
	RTListInit(&sf_i->HandleList);
#ifdef VBOX_STRICT
	sf_i->u32Magic = SF_INODE_INFO_MAGIC;
#endif

	err = sf_stat(__func__, sf_g, sf_i->path, &fsinfo, 0);
	if (err) {
		LogFunc(("could not stat root of share\n"));
		goto fail3;
	}

	sb->s_magic = 0xface;
	sb->s_blocksize = 1024;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 3)
	/* Required for seek/sendfile (see 'loff_t max' in fs/read_write.c / do_sendfile()). */
# if defined MAX_LFS_FILESIZE
	sb->s_maxbytes = MAX_LFS_FILESIZE;
# elif BITS_PER_LONG == 32
	sb->s_maxbytes = (loff_t)ULONG_MAX << PAGE_SHIFT;
# else
	sb->s_maxbytes = INT64_MAX;
# endif
#endif
	sb->s_op = &sf_super_ops;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 38)
	sb->s_d_op = &sf_dentry_ops;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 25)
	iroot = iget_locked(sb, 0);
#else
	iroot = iget(sb, 0);
#endif
	if (!iroot) {
		err = -ENOMEM;	/* XXX */
		LogFunc(("could not get root inode\n"));
		goto fail3;
	}

	if (sf_init_backing_dev(sb, sf_g)) {
		err = -EINVAL;
		LogFunc(("could not init bdi\n"));
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 25)
		unlock_new_inode(iroot);
#endif
		goto fail4;
	}

	sf_init_inode(iroot, sf_i, &fsinfo, sf_g);
	SET_INODE_INFO(iroot, sf_i);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 25)
	unlock_new_inode(iroot);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
	droot = d_make_root(iroot);
#else
	droot = d_alloc_root(iroot);
#endif
	if (!droot) {
		err = -ENOMEM;	/* XXX */
		LogFunc(("d_alloc_root failed\n"));
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
		fInodePut = false;
#endif
		goto fail5;
	}

	sb->s_root = droot;
	VBSF_SET_SUPER_INFO(sb, sf_g);
	return 0;

 fail5:
	sf_done_backing_dev(sb, sf_g);

 fail4:
	if (fInodePut)
		iput(iroot);

 fail3:
	kfree(sf_i->path);

 fail2:
	kfree(sf_i);

 fail1:
	sf_super_info_free(sf_g);

 fail0:
	return err;
}


/**
 * This is called when vfs is about to destroy the @a inode.
 *
 * We must free the inode info structure here.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
static void sf_evict_inode(struct inode *inode)
#else
static void sf_clear_inode(struct inode *inode)
#endif
{
	struct sf_inode_info *sf_i;

	TRACE();

	/*
	 * Flush stuff.
	 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
	truncate_inode_pages(&inode->i_data, 0);
# if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 5, 0)
	clear_inode(inode);
# else
	end_writeback(inode);
# endif
#endif
	/*
	 * Clean up our inode info.
	 */
	sf_i = GET_INODE_INFO(inode);
	if (sf_i) {
		SET_INODE_INFO(inode, NULL);

		Assert(sf_i->u32Magic == SF_INODE_INFO_MAGIC);
		BUG_ON(!sf_i->path);
		kfree(sf_i->path);
		sf_handle_drop_chain(sf_i);
# ifdef VBOX_STRICT
		sf_i->u32Magic = SF_INODE_INFO_MAGIC_DEAD;
# endif
		kfree(sf_i);
	}
}


/* this is called by vfs when it wants to populate [inode] with data.
   the only thing that is known about inode at this point is its index
   hence we can't do anything here, and let lookup/whatever with the
   job to properly fill then [inode] */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 25)
static void sf_read_inode(struct inode *inode)
{
}
#endif


/* vfs is done with [sb] (umount called) call [sf_super_info_free] to unmap
   the folder and free [sf_g] */
static void sf_put_super(struct super_block *sb)
{
	struct vbsf_super_info *sf_g;

	sf_g = VBSF_GET_SUPER_INFO(sb);
	BUG_ON(!sf_g);
	sf_done_backing_dev(sb, sf_g);
	sf_super_info_free(sf_g);
}


/**
 * Get file system statistics.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 18)
static int sf_statfs(struct dentry *dentry, struct kstatfs *stat)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 73)
static int sf_statfs(struct super_block *sb, struct kstatfs *stat)
#else
static int sf_statfs(struct super_block *sb, struct statfs *stat)
#endif
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 18)
	struct super_block *sb = dentry->d_inode->i_sb;
#endif
	int rc;
	VBOXSFVOLINFOREQ *pReq = (VBOXSFVOLINFOREQ *)VbglR0PhysHeapAlloc(sizeof(*pReq));
	if (pReq) {
		SHFLVOLINFO            *pVolInfo = &pReq->VolInfo;
		struct vbsf_super_info *sf_g     = VBSF_GET_SUPER_INFO(sb);
		rc = VbglR0SfHostReqQueryVolInfo(sf_g->map.root, pReq, SHFL_HANDLE_ROOT);
		if (RT_SUCCESS(rc)) {
			stat->f_type   = NFS_SUPER_MAGIC; /** @todo vboxsf type? */
			stat->f_bsize  = pVolInfo->ulBytesPerAllocationUnit;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 73)
			stat->f_frsize = pVolInfo->ulBytesPerAllocationUnit;
#endif
			stat->f_blocks = pVolInfo->ullTotalAllocationBytes
				       / pVolInfo->ulBytesPerAllocationUnit;
			stat->f_bfree  = pVolInfo->ullAvailableAllocationBytes
				       / pVolInfo->ulBytesPerAllocationUnit;
			stat->f_bavail = pVolInfo->ullAvailableAllocationBytes
				       / pVolInfo->ulBytesPerAllocationUnit;
			stat->f_files  = 1000;
			stat->f_ffree  = 1000; /* don't return 0 here since the guest may think
						* that it is not possible to create any more files */
			stat->f_fsid.val[0] = 0;
			stat->f_fsid.val[1] = 0;
			stat->f_namelen = 255;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
			stat->f_flags = 0; /* not valid */
#endif
			RT_ZERO(stat->f_spare);
			rc = 0;
		} else
			rc = -RTErrConvertToErrno(rc);
		VbglR0PhysHeapFree(pReq);
	} else
		rc = -ENOMEM;
	return rc;
}

static int sf_remount_fs(struct super_block *sb, int *flags, char *data)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 23)
	struct vbsf_super_info *sf_g;
	struct sf_inode_info *sf_i;
	struct inode *iroot;
	SHFLFSOBJINFO fsinfo;
	int err;

	sf_g = VBSF_GET_SUPER_INFO(sb);
	BUG_ON(!sf_g);
	if (data && data[0] != 0) {
		struct vbsf_mount_info_new *info = (struct vbsf_mount_info_new *)data;
		if (   info->nullchar == '\0'
		    && info->signature[0] == VBSF_MOUNT_SIGNATURE_BYTE_0
		    && info->signature[1] == VBSF_MOUNT_SIGNATURE_BYTE_1
		    && info->signature[2] == VBSF_MOUNT_SIGNATURE_BYTE_2) {
			sf_super_info_copy_remount_options(sf_g, info);
		}
	}

	iroot = ilookup(sb, 0);
	if (!iroot)
		return -ENOSYS;

	sf_i = GET_INODE_INFO(iroot);
	err = sf_stat(__func__, sf_g, sf_i->path, &fsinfo, 0);
	BUG_ON(err != 0);
	sf_init_inode(iroot, sf_i, &fsinfo, sf_g);
	/*unlock_new_inode(iroot); */
	return 0;
#else  /* LINUX_VERSION_CODE < 2.4.23 */
	return -ENOSYS;
#endif /* LINUX_VERSION_CODE < 2.4.23 */
}


/**
 * Show mount options.
 *
 * This is needed by the VBoxService automounter in order for it to pick up
 * the the 'tag' option value it sets on its mount.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 3, 0)
static int sf_show_options(struct seq_file *m, struct vfsmount *mnt)
#else
static int sf_show_options(struct seq_file *m, struct dentry *root)
#endif
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 3, 0)
	struct super_block *sb = mnt->mnt_sb;
#else
	struct super_block *sb = root->d_sb;
#endif
	struct vbsf_super_info *sf_g = VBSF_GET_SUPER_INFO(sb);
	if (sf_g) {
		seq_printf(m, ",uid=%u,gid=%u,ttl=%d,dmode=0%o,fmode=0%o,dmask=0%o,fmask=0%o,maxiopages=%u",
			   sf_g->uid, sf_g->gid, sf_g->ttl_msec, sf_g->dmode, sf_g->fmode, sf_g->dmask,
			   sf_g->fmask, sf_g->cMaxIoPages);
		if (sf_g->tag[0] != '\0') {
			seq_puts(m, ",tag=");
			seq_escape(m, sf_g->tag, " \t\n\\");
		}
	}
	return 0;
}


/**
 * Super block operations.
 */
static struct super_operations sf_super_ops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36)
	.clear_inode = sf_clear_inode,
#else
	.evict_inode = sf_evict_inode,
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 25)
	.read_inode = sf_read_inode,
#endif
	.put_super = sf_put_super,
	.statfs = sf_statfs,
	.remount_fs = sf_remount_fs,
	.show_options = sf_show_options
};


/*
 * File system type related stuff.
 */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 4)

static int sf_read_super_26(struct super_block *sb, void *data, int flags)
{
	int err;

	TRACE();
	err = sf_read_super_aux(sb, data, flags);
	if (err)
		printk(KERN_DEBUG "sf_read_super_aux err=%d\n", err);

	return err;
}

# if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 18)
static struct super_block *sf_get_sb(struct file_system_type *fs_type,
				     int flags, const char *dev_name,
				     void *data)
{
	TRACE();
	return get_sb_nodev(fs_type, flags, data, sf_read_super_26);
}
# elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 39)
static int sf_get_sb(struct file_system_type *fs_type, int flags,
		     const char *dev_name, void *data, struct vfsmount *mnt)
{
	TRACE();
	return get_sb_nodev(fs_type, flags, data, sf_read_super_26, mnt);
}
# else /* LINUX_VERSION_CODE >= 2.6.39 */
static struct dentry *sf_mount(struct file_system_type *fs_type, int flags,
			       const char *dev_name, void *data)
{
	TRACE();
	return mount_nodev(fs_type, flags, data, sf_read_super_26);
}
# endif /* LINUX_VERSION_CODE >= 2.6.39 */

static struct file_system_type vboxsf_fs_type = {
	.owner = THIS_MODULE,
	.name = "vboxsf",
# if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 39)
	.get_sb = sf_get_sb,
# else
	.mount = sf_mount,
# endif
	.kill_sb = kill_anon_super
};

#else  /* LINUX_VERSION_CODE < 2.5.4 */

static struct super_block *sf_read_super_24(struct super_block *sb, void *data,
					    int flags)
{
	int err;

	TRACE();
	err = sf_read_super_aux(sb, data, flags);
	if (err) {
		printk(KERN_DEBUG "sf_read_super_aux err=%d\n", err);
		return NULL;
	}

	return sb;
}

static DECLARE_FSTYPE(vboxsf_fs_type, "vboxsf", sf_read_super_24, 0);

#endif /* LINUX_VERSION_CODE < 2.5.4 */


/* Module initialization/finalization handlers */
static int __init init(void)
{
	int vrc;
	int rcRet = 0;
	int err;

	TRACE();

	if (sizeof(struct vbsf_mount_info_new) > PAGE_SIZE) {
		printk(KERN_ERR
		       "Mount information structure is too large %lu\n"
		       "Must be less than or equal to %lu\n",
		       (unsigned long)sizeof(struct vbsf_mount_info_new),
		       (unsigned long)PAGE_SIZE);
		return -EINVAL;
	}

	/** @todo Init order is wrong, file system reigstration is the very last
	 *        thing we should do. */
	spin_lock_init(&g_SfHandleLock);
	err = register_filesystem(&vboxsf_fs_type);
	if (err) {
		LogFunc(("register_filesystem err=%d\n", err));
		return err;
	}

	vrc = VbglR0SfInit();
	if (RT_FAILURE(vrc)) {
		LogRelFunc(("VbglR0SfInit failed, vrc=%Rrc\n", vrc));
		rcRet = -EPROTO;
		goto fail0;
	}

	vrc = VbglR0SfConnect(&client_handle);
	g_SfClient = client_handle; /* temporary */
	if (RT_FAILURE(vrc)) {
		LogRelFunc(("VbglR0SfConnect failed, vrc=%Rrc\n", vrc));
		rcRet = -EPROTO;
		goto fail1;
	}

	vrc = VbglR0QueryHostFeatures(&g_fHostFeatures);
	if (RT_FAILURE(vrc))
	{
		LogRelFunc(("VbglR0QueryHostFeatures failed: vrc=%Rrc (ignored)\n", vrc));
		g_fHostFeatures = 0;
	}
	LogRelFunc(("g_fHostFeatures=%#x\n", g_fHostFeatures));

	vrc = VbglR0SfSetUtf8(&client_handle);
	if (RT_FAILURE(vrc)) {
		LogRelFunc(("VbglR0SfSetUtf8 failed, vrc=%Rrc\n", vrc));
		rcRet = -EPROTO;
		goto fail2;
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
	if (!g_fFollowSymlinks) {
		vrc = VbglR0SfSetSymlinks(&client_handle);
		if (RT_FAILURE(vrc)) {
			printk(KERN_WARNING
			       "vboxsf: Host unable to show symlinks, vrc=%d\n",
			       vrc);
		}
	}
#endif

	printk(KERN_DEBUG
	       "vboxsf: Successfully loaded version " VBOX_VERSION_STRING
	       " (interface " RT_XSTR(VMMDEV_VERSION) ")\n");

	return 0;

 fail2:
	VbglR0SfDisconnect(&client_handle);
	g_SfClient = client_handle; /* temporary */

 fail1:
	VbglR0SfTerm();

 fail0:
	unregister_filesystem(&vboxsf_fs_type);
	return rcRet;
}

static void __exit fini(void)
{
	TRACE();

	VbglR0SfDisconnect(&client_handle);
	g_SfClient = client_handle; /* temporary */
	VbglR0SfTerm();
	unregister_filesystem(&vboxsf_fs_type);
}


/*
 * Module parameters.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 52)
module_param_named(follow_symlinks, g_fFollowSymlinks, int, 0);
MODULE_PARM_DESC(follow_symlinks,
		 "Let host resolve symlinks rather than showing them");
#endif


/*
 * Module declaration related bits.
 */
module_init(init);
module_exit(fini);

MODULE_DESCRIPTION(VBOX_PRODUCT " VFS Module for Host File System Access");
MODULE_AUTHOR(VBOX_VENDOR);
MODULE_LICENSE("GPL and additional rights");
#ifdef MODULE_ALIAS_FS
MODULE_ALIAS_FS("vboxsf");
#endif
#ifdef MODULE_VERSION
MODULE_VERSION(VBOX_VERSION_STRING " r" RT_XSTR(VBOX_SVN_REV));
#endif

