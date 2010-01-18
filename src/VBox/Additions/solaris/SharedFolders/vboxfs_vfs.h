/* $Id$ */
/** @file
 * VirtualBox File System for Solaris Guests, VFS implementation.
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

#ifndef	__VBoxFS_vfs_Solaris_h
#define	__VBoxFS_vfs_Solaris_h

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Shared Folders filesystem per-mount data structure.
 */
typedef struct sffs_data {
	vfs_t		*sf_vfsp;	/* filesystem's vfs struct */
	vnode_t		*sf_rootnode;	/* of vnode of the root directory */
	uid_t		sf_uid;		/* owner of all shared folders */
	gid_t		sf_gid;		/* group of all shared folders */
	char		*sf_share_name;
	char 		*sf_mntpath;	/* name of mount point */
	sfp_mount_t	*sf_handle;
	uint64_t	sf_ino;		/* per FS ino generator */
} sffs_data_t;


#ifdef	__cplusplus
}
#endif

#endif	/* __VBoxFS_vfs_Solaris_h */
