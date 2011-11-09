/* $Id$ */
/** @file
 * VirtualBox File System for Solaris Guests, provider header.
 * Portions contributed by: Ronald.
 */

/*
 * Copyright (C) 2009-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef	___VBoxFS_prov_Solaris_h
#define	___VBoxFS_prov_Solaris_h

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * These are the provider interfaces used by sffs to access the underlying
 * shared file system.
 */
#define	SFPROV_VERSION	1

/*
 * Initialization and termination.
 * sfprov_connect() is called once before any other interfaces and returns
 * a handle used in further calls. The argument should be SFPROV_VERSION
 * from above. On failure it returns a NULL pointer.
 *
 * sfprov_disconnect() must only be called after all sf file systems have been
 * unmounted.
 */
typedef struct sfp_connection sfp_connection_t;

extern sfp_connection_t *sfprov_connect(int);
extern void sfprov_disconnect(sfp_connection_t *);


/*
 * Mount / Unmount a shared folder.
 *
 * sfprov_mount() takes as input the connection pointer and the name of
 * the shared folder. On success, it returns zero and supplies an
 * sfp_mount_t handle. On failure it returns any relevant errno value.
 *
 * sfprov_unmount() unmounts the mounted file system. It returns 0 on
 * success and any relevant errno on failure.
 */
typedef struct sfp_mount sfp_mount_t;

extern int sfprov_mount(sfp_connection_t *, char *, sfp_mount_t **);
extern int sfprov_unmount(sfp_mount_t *);

/*
 * query information about a mounted file system
 */
extern int sfprov_get_blksize(sfp_mount_t *, uint64_t *);
extern int sfprov_get_blksused(sfp_mount_t *, uint64_t *);
extern int sfprov_get_blksavail(sfp_mount_t *, uint64_t *);
extern int sfprov_get_maxnamesize(sfp_mount_t *, uint32_t *);
extern int sfprov_get_readonly(sfp_mount_t *, uint32_t *);

/*
 * File operations: open/close/read/write/etc.
 *
 * open/create can return any relevant errno, however ENOENT
 * generally means that the host file didn't exist.
 */
typedef struct sfp_file sfp_file_t;

extern int sfprov_create(sfp_mount_t *, char *path, sfp_file_t **fp);
extern int sfprov_open(sfp_mount_t *, char *path, sfp_file_t **fp);
extern int sfprov_close(sfp_file_t *fp);
extern int sfprov_read(sfp_file_t *, char * buffer, uint64_t offset,
    uint32_t *numbytes);
extern int sfprov_write(sfp_file_t *, char * buffer, uint64_t offset,
    uint32_t *numbytes);
extern int sfprov_fsync(sfp_file_t *fp);


/*
 * get/set information about a file (or directory) using pathname
 */
extern int sfprov_get_mode(sfp_mount_t *, char *, mode_t *);
extern int sfprov_get_size(sfp_mount_t *, char *, uint64_t *);
extern int sfprov_get_atime(sfp_mount_t *, char *, timestruc_t *);
extern int sfprov_get_mtime(sfp_mount_t *, char *, timestruc_t *);
extern int sfprov_get_ctime(sfp_mount_t *, char *, timestruc_t *);
extern int sfprov_get_attr(sfp_mount_t *, char *, mode_t *, uint64_t *,
   timestruc_t *, timestruc_t *, timestruc_t *);
extern int sfprov_set_attr(sfp_mount_t *, char *, uint_t, mode_t,
   timestruc_t, timestruc_t, timestruc_t);
extern int sfprov_set_size(sfp_mount_t *, char *, uint64_t);


/*
 * File/Directory operations
 */
extern int sfprov_trunc(sfp_mount_t *, char *);
extern int sfprov_remove(sfp_mount_t *, char *path);
extern int sfprov_mkdir(sfp_mount_t *, char *path, sfp_file_t **fp);
extern int sfprov_rmdir(sfp_mount_t *, char *path);
extern int sfprov_rename(sfp_mount_t *, char *from, char *to, uint_t is_dir);

typedef struct sffs_stat {
	mode_t		sf_mode;
	off_t		sf_size;
	timestruc_t	sf_atime;
	timestruc_t	sf_mtime;
	timestruc_t	sf_ctime;
} sffs_stat_t;

/*
 * Read directory entries.
 */
/*
 * a singly linked list of buffers, each containing an array of stat's+dirent's.
 * sf_len is length of the sf_entries array, in bytes.
 */
typedef struct sffs_dirents {
	struct sffs_dirents	*sf_next;
	len_t			sf_len;
	struct sffs_dirent {
		sffs_stat_t	sf_stat;
		dirent64_t	sf_entry;	/* this is variable length */
	}			sf_entries[1];
} sffs_dirents_t;

#define SFFS_DIRENTS_SIZE	8192
#define SFFS_DIRENTS_OFF	(offsetof(sffs_dirents_t, sf_entries[0]))

extern int sfprov_readdir(sfp_mount_t *mnt, char *path,
	sffs_dirents_t **dirents);

#ifdef	__cplusplus
}
#endif

#endif	/* !___VBoxFS_prov_Solaris_h */
