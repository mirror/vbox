/** @file
 * VirtualBox File System Driver for Solaris Guests, VFS header.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
 *
 * Sun Microsystems, Inc. confidential
 * All rights reserved
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
