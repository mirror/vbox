/* $Id$ */
/** @file
 * VirtualBox File System for Solaris Guests, VNode implementation.
 */

/*
 * Copyright (C) 2010 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef __VBoxFS_node_Solaris_h
#define	__VBoxFS_node_Solaris_h

#include <sys/t_lock.h>
#include <sys/avl.h>
#include <vm/seg.h>
#include <vm/seg_vn.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * sfnode is the file system dependent vnode data for vboxsf.
 * sfnode's also track all files ever accessed, both open and closed.
 * It duplicates some of the information in vnode, since it holds
 * information for files that may have been completely closed.
 *
 * The sfnode_t's are stored in an AVL tree sorted by:
 *	sf_sffs, sf_path
 */
typedef struct sfnode {
	avl_node_t	sf_linkage;	/* AVL tree linkage */
	struct sffs_data *sf_sffs;	/* containing mounted file system */
	char		*sf_path;	/* full pathname to file or dir */
	uint64_t	sf_ino;		/* assigned unique ID number */
	vnode_t		*sf_vnode;	/* vnode if active */
	sfp_file_t	*sf_file;	/* non NULL if open */
	struct sfnode	*sf_parent;	/* parent sfnode of this one */
	uint16_t	sf_children;	/* number of children sfnodes */
	uint8_t		sf_type;	/* VDIR or VREG */
	uint8_t		sf_is_stale;	/* this is stale and should be purged */
} sfnode_t;

#define VN2SFN(vp) ((sfnode_t *)(vp)->v_data)

#ifdef _KERNEL
extern int sffs_vnode_init(void);
extern void sffs_vnode_fini(void);
extern sfnode_t *sfnode_make(struct sffs_data *, char *, vtype_t, sfp_file_t *,
 sfnode_t *parent);
extern vnode_t *sfnode_get_vnode(sfnode_t *);

/*
 * Purge all cached information about a shared file system at unmount
 */
extern int sffs_purge(struct sffs_data *);

extern kmutex_t sffs_lock;
#endif /* _KERNEL */

#ifdef	__cplusplus
}
#endif

#endif	/* __VBoxFS_node_Solaris_h */
