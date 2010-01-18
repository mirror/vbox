/** @file
 * VirtualBox File System for Solaris Guests, provider header.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
 *
 * Sun Microsystems, Inc. confidential
 * All rights reserved
 */

#ifndef	__VBoxFS_prov_Solaris_h
#define	__VBoxFS_prov_Solaris_h

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


/*
 * get information about a file (or directory) using pathname
 */
extern int sfprov_get_mode(sfp_mount_t *, char *, mode_t *);
extern int sfprov_get_size(sfp_mount_t *, char *, uint64_t *);
extern int sfprov_get_atime(sfp_mount_t *, char *, timestruc_t *);
extern int sfprov_get_mtime(sfp_mount_t *, char *, timestruc_t *);
extern int sfprov_get_ctime(sfp_mount_t *, char *, timestruc_t *);


/*
 * File/Directory operations
 */
extern int sfprov_trunc(sfp_mount_t *, char *);
extern int sfprov_remove(sfp_mount_t *, char *path);
extern int sfprov_mkdir(sfp_mount_t *, char *path, sfp_file_t **fp);
extern int sfprov_rmdir(sfp_mount_t *, char *path);
extern int sfprov_rename(sfp_mount_t *, char *from, char *to, uint_t is_dir);

/*
 * Read directory entries.
 */
extern int sfprov_readdir(sfp_mount_t *mnt, char *path, void **buffer,
	size_t *buffersize, uint32_t *nents);

#ifdef	__cplusplus
}
#endif

#endif	/* __VBoxFS_prov_Solaris_h */
