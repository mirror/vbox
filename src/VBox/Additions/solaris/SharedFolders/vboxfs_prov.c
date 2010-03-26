/** @file
 * VirtualBox File System for Solaris Guests, provider implementation.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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
/*
 * Provider interfaces for shared folder file system.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mntent.h>
#include <sys/param.h>
#include <sys/modctl.h>
#include <sys/mount.h>
#include <sys/policy.h>
#include <sys/atomic.h>
#include <sys/sysmacros.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include "vboxfs_prov.h"
#ifdef u
#undef u
#endif
#include "../../common/VBoxGuestLib/VBoxCalls.h"

#define	SFPROV_VERSION	1

static VBSFCLIENT vbox_client;

/*
 * utility to create strings
 */
static SHFLSTRING *
sfprov_string(char *path, int *sz)
{
	SHFLSTRING *str;
	int len = strlen(path);

	*sz = len + 1 + sizeof (*str) - sizeof (str->String);
	str = kmem_zalloc(*sz, KM_SLEEP);
	str->u16Size = len + 1;
	str->u16Length = len;
	strcpy(str->String.utf8, path);
	return (str);
}

sfp_connection_t *
sfprov_connect(int version)
{
	/*
	 * only one version for now, so must match
	 */
	int rc = -1;
	if (version != SFPROV_VERSION)
	{
		cmn_err(CE_WARN, "sfprov_connect: wrong version");
		return NULL;
	}
	rc = vboxInit();
	if (RT_SUCCESS(rc))
	{
	    rc = vboxConnect(&vbox_client);
        if (RT_SUCCESS(rc))
        {
            rc = vboxCallSetUtf8(&vbox_client);
            if (RT_SUCCESS(rc))
            {
            	return ((sfp_connection_t *)&vbox_client);
            }
            else
        		cmn_err(CE_WARN, "sfprov_connect: vboxCallSetUtf8() failed");

		    vboxDisconnect(&vbox_client);
        }
        else
    		cmn_err(CE_WARN, "sfprov_connect: vboxConnect() failed rc=%d", rc);
		vboxUninit();
	}
	else
		cmn_err(CE_WARN, "sfprov_connect: vboxInit() failed rc=%d", rc);
}

void
sfprov_disconnect(sfp_connection_t *conn)
{
	if (conn != (sfp_connection_t *)&vbox_client)
		cmn_err(CE_WARN, "sfprov_disconnect: bad argument");
	vboxDisconnect(&vbox_client);
	vboxUninit();
}


/*
 * representation of an active mount point
 */
struct sfp_mount {
	VBSFMAP	map;
};

int
sfprov_mount(sfp_connection_t *conn, char *path, sfp_mount_t **mnt)
{
	sfp_mount_t *m;
	SHFLSTRING *str;
	int size;
	int rc;

	m = kmem_zalloc(sizeof (*m), KM_SLEEP);
	str = sfprov_string(path, &size);
	rc = vboxCallMapFolder(&vbox_client, str, &m->map);
	if (!RT_SUCCESS(rc)) {
		cmn_err(CE_WARN, "sfprov_mount: vboxCallMapFolder() failed");
		kmem_free(m, sizeof (*m));
		*mnt = NULL;
		rc = EINVAL;
	} else {
		*mnt = m;
		rc = 0;
	}
	kmem_free(str, size);
	return (rc);
}

int
sfprov_unmount(sfp_mount_t *mnt)
{
	int rc;

	rc = vboxCallUnmapFolder(&vbox_client, &mnt->map);
	if (!RT_SUCCESS(rc)) {
		cmn_err(CE_WARN, "sfprov_mount: vboxCallUnmapFolder() failed");
		rc = EINVAL;
	} else {
		rc = 0;
	}
	kmem_free(mnt, sizeof (*mnt));
	return (rc);
}

/*
 * query information about a mounted file system
 */
int
sfprov_get_blksize(sfp_mount_t *mnt, uint64_t *blksize)
{
	int rc;
	SHFLVOLINFO info;
	uint32_t bytes = sizeof(SHFLVOLINFO);

	rc = vboxCallFSInfo(&vbox_client, &mnt->map, 0,
	    (SHFL_INFO_GET | SHFL_INFO_VOLUME), &bytes, (SHFLDIRINFO *)&info);
	if (RT_FAILURE(rc))
		return (EINVAL);
	*blksize = info.ulBytesPerAllocationUnit;
	return (0);
}

int
sfprov_get_blksused(sfp_mount_t *mnt, uint64_t *blksused)
{
	int rc;
	SHFLVOLINFO info;
	uint32_t bytes = sizeof(SHFLVOLINFO);

	rc = vboxCallFSInfo(&vbox_client, &mnt->map, 0,
	    (SHFL_INFO_GET | SHFL_INFO_VOLUME), &bytes, (SHFLDIRINFO *)&info);
	if (RT_FAILURE(rc))
		return (EINVAL);
	*blksused = (info.ullTotalAllocationBytes -
	    info.ullAvailableAllocationBytes) / info.ulBytesPerAllocationUnit;
	return (0);
}

int
sfprov_get_blksavail(sfp_mount_t *mnt, uint64_t *blksavail)
{
	int rc;
	SHFLVOLINFO info;
	uint32_t bytes = sizeof(SHFLVOLINFO);

	rc = vboxCallFSInfo(&vbox_client, &mnt->map, 0,
	    (SHFL_INFO_GET | SHFL_INFO_VOLUME), &bytes, (SHFLDIRINFO *)&info);
	if (RT_FAILURE(rc))
		return (EINVAL);
	*blksavail =
	    info.ullAvailableAllocationBytes / info.ulBytesPerAllocationUnit;
	return (0);
}

int
sfprov_get_maxnamesize(sfp_mount_t *mnt, uint32_t *maxnamesize)
{
	int rc;
	SHFLVOLINFO info;
	uint32_t bytes = sizeof(SHFLVOLINFO);

	rc = vboxCallFSInfo(&vbox_client, &mnt->map, 0,
	    (SHFL_INFO_GET | SHFL_INFO_VOLUME), &bytes, (SHFLDIRINFO *)&info);
	if (RT_FAILURE(rc))
		return (EINVAL);
	*maxnamesize = info.fsProperties.cbMaxComponent;
	return (0);
}

int
sfprov_get_readonly(sfp_mount_t *mnt, uint32_t *readonly)
{
	int rc;
	SHFLVOLINFO info;
	uint32_t bytes = sizeof(SHFLVOLINFO);

	rc = vboxCallFSInfo(&vbox_client, &mnt->map, 0,
	    (SHFL_INFO_GET | SHFL_INFO_VOLUME), &bytes, (SHFLDIRINFO *)&info);
	if (RT_FAILURE(rc))
		return (EINVAL);
	*readonly = info.fsProperties.fReadOnly;
	return (0);
}

/*
 * File operations: open/close/read/write/etc.
 *
 * open/create can return any relevant errno, however ENOENT
 * generally means that the host file didn't exist.
 */
struct sfp_file {
	SHFLHANDLE handle;
	VBSFMAP map;	/* need this again for the close operation */
};

int
sfprov_create(sfp_mount_t *mnt, char *path, sfp_file_t **fp)
{
	int rc;
	SHFLCREATEPARMS parms;
	SHFLSTRING *str;
	int size;
	sfp_file_t *newfp;

	str = sfprov_string(path, &size);
	parms.Handle = 0;
	parms.Info.cbObject = 0;
	parms.CreateFlags = SHFL_CF_ACT_CREATE_IF_NEW |
	    SHFL_CF_ACT_REPLACE_IF_EXISTS | SHFL_CF_ACCESS_READWRITE;
	rc = vboxCallCreate(&vbox_client, &mnt->map, str, &parms);
	kmem_free(str, size);

	if (RT_FAILURE(rc))
		return (EINVAL);
	if (parms.Handle == SHFL_HANDLE_NIL) {
		if (parms.Result == SHFL_FILE_EXISTS)
			return (EEXIST);
		return (ENOENT);
	}
	newfp = kmem_alloc(sizeof(sfp_file_t), KM_SLEEP);
	newfp->handle = parms.Handle;
	newfp->map = mnt->map;
	*fp = newfp;
	return (0);
}

int
sfprov_open(sfp_mount_t *mnt, char *path, sfp_file_t **fp)
{
	int rc;
	SHFLCREATEPARMS parms;
	SHFLSTRING *str;
	int size;
	sfp_file_t *newfp;

	/*
	 * First we attempt to open it read/write. If that fails we
	 * try read only.
	 */
	bzero(&parms, sizeof(parms));
	str = sfprov_string(path, &size);
	parms.Handle = SHFL_HANDLE_NIL;
	parms.Info.cbObject = 0;
	parms.CreateFlags = SHFL_CF_ACT_FAIL_IF_NEW | SHFL_CF_ACCESS_READWRITE;
	rc = vboxCallCreate(&vbox_client, &mnt->map, str, &parms);
	if (RT_FAILURE(rc) && rc != VERR_ACCESS_DENIED) {
		kmem_free(str, size);
		return RTErrConvertToErrno(rc);
	}
	if (parms.Handle == SHFL_HANDLE_NIL) {
		if (parms.Result == SHFL_PATH_NOT_FOUND ||
		    parms.Result == SHFL_FILE_NOT_FOUND) {
			kmem_free(str, size);
			return (ENOENT);
		}
		parms.CreateFlags =
		    SHFL_CF_ACT_FAIL_IF_NEW | SHFL_CF_ACCESS_READ;
		rc = vboxCallCreate(&vbox_client, &mnt->map, str, &parms);
		if (RT_FAILURE(rc)) {
			kmem_free(str, size);
			return RTErrConvertToErrno(rc);
		}
		if (parms.Handle == SHFL_HANDLE_NIL) {
			kmem_free(str, size);
			return (ENOENT);
		}
	}
	newfp = kmem_alloc(sizeof(sfp_file_t), KM_SLEEP);
	newfp->handle = parms.Handle;
	newfp->map = mnt->map;
	*fp = newfp;
	return (0);
}

int
sfprov_trunc(sfp_mount_t *mnt, char *path)
{
	int rc;
	SHFLCREATEPARMS parms;
	SHFLSTRING *str;
	int size;
	sfp_file_t *newfp;

	/*
	 * open it read/write.
	 */
	str = sfprov_string(path, &size);
	parms.Handle = 0;
	parms.Info.cbObject = 0;
	parms.CreateFlags = SHFL_CF_ACT_FAIL_IF_NEW | SHFL_CF_ACCESS_READWRITE |
	    SHFL_CF_ACT_OVERWRITE_IF_EXISTS;
	rc = vboxCallCreate(&vbox_client, &mnt->map, str, &parms);

	if (RT_FAILURE(rc)) {
		kmem_free(str, size);
		return (EINVAL);
	}
	(void)vboxCallClose(&vbox_client, &mnt->map, parms.Handle);
	return (0);
}

int
sfprov_close(sfp_file_t *fp)
{
	int rc;

	rc = vboxCallClose(&vbox_client, &fp->map, fp->handle);
	kmem_free(fp, sizeof(sfp_file_t));
	return (0);
}

int
sfprov_read(sfp_file_t *fp, char *buffer, uint64_t offset, uint32_t *numbytes)
{
	int rc;

	rc = vboxCallRead(&vbox_client, &fp->map, fp->handle, offset,
	    numbytes, (uint8_t *)buffer, 0);	/* what is that last arg? */
	if (RT_FAILURE(rc))
		return (EINVAL);
	return (0);
}

int
sfprov_write(sfp_file_t *fp, char *buffer, uint64_t offset, uint32_t *numbytes)
{
	int rc;

	rc = vboxCallWrite(&vbox_client, &fp->map, fp->handle, offset,
	    numbytes, (uint8_t *)buffer, 0);	/* what is that last arg? */
	if (RT_FAILURE(rc))
		return (EINVAL);
	return (0);
}


static int
sfprov_getinfo(sfp_mount_t *mnt, char *path, RTFSOBJINFO *info)
{
	int rc;
	SHFLCREATEPARMS parms;
	SHFLSTRING *str;
	int size;

	str = sfprov_string(path, &size);
	parms.Handle = 0;
	parms.Info.cbObject = 0;
	parms.CreateFlags = SHFL_CF_LOOKUP | SHFL_CF_ACT_FAIL_IF_NEW;
	rc = vboxCallCreate(&vbox_client, &mnt->map, str, &parms);
	kmem_free(str, size);

	if (RT_FAILURE(rc))
		return (EINVAL);
	if (parms.Result != SHFL_FILE_EXISTS)
		return (ENOENT);
	*info = parms.Info;
	return (0);
}

/*
 * get information about a file (or directory)
 */
int
sfprov_get_mode(sfp_mount_t *mnt, char *path, mode_t *mode)
{
	int rc;
	RTFSOBJINFO info;
	mode_t m = 0;

	rc = sfprov_getinfo(mnt, path, &info);
	if (rc)
		return (rc);
	if (RTFS_IS_DIRECTORY(info.Attr.fMode))
		m |= S_IFDIR;
	else if (RTFS_IS_FILE(info.Attr.fMode))
		m |= S_IFREG;
	else if (RTFS_IS_FIFO(info.Attr.fMode))
		m |= S_IFDIR;
	else if (RTFS_IS_DEV_CHAR(info.Attr.fMode))
		m |= S_IFCHR;
	else if (RTFS_IS_DEV_BLOCK(info.Attr.fMode))
		m |= S_IFBLK;
	else if (RTFS_IS_SYMLINK(info.Attr.fMode))
		m |= S_IFLNK;
	else if (RTFS_IS_SOCKET(info.Attr.fMode))
		m |= S_IFSOCK;

	if (info.Attr.fMode & RTFS_UNIX_IRUSR)
		m |= S_IRUSR;
	if (info.Attr.fMode & RTFS_UNIX_IWUSR)
		m |= S_IWUSR;
	if (info.Attr.fMode & RTFS_UNIX_IXUSR)
		m |= S_IXUSR;
	if (info.Attr.fMode & RTFS_UNIX_IRGRP)
		m |= S_IRGRP;
	if (info.Attr.fMode & RTFS_UNIX_IWGRP)
		m |= S_IWGRP;
	if (info.Attr.fMode & RTFS_UNIX_IXGRP)
		m |= S_IXGRP;
	if (info.Attr.fMode & RTFS_UNIX_IROTH)
		m |= S_IROTH;
	if (info.Attr.fMode & RTFS_UNIX_IWOTH)
		m |= S_IWOTH;
	if (info.Attr.fMode & RTFS_UNIX_IXOTH)
		m |= S_IXOTH;
	*mode = m;
	return (0);
}

int
sfprov_get_size(sfp_mount_t *mnt, char *path, uint64_t *size)
{
	int rc;
	RTFSOBJINFO info;

	rc = sfprov_getinfo(mnt, path, &info);
	if (rc)
		return (rc);
	*size = info.cbObject;
	return (0);
}

int
sfprov_get_atime(sfp_mount_t *mnt, char *path, timestruc_t *time)
{
	int rc;
	RTFSOBJINFO info;
	uint64_t nanosec;

	rc = sfprov_getinfo(mnt, path, &info);
	if (rc)
		return (rc);
	nanosec = RTTimeSpecGetNano(&info.AccessTime);
	time->tv_sec = nanosec / 1000000000;
	time->tv_nsec = nanosec % 1000000000;
	return (0);
}

int
sfprov_get_mtime(sfp_mount_t *mnt, char *path, timestruc_t *time)
{
	int rc;
	RTFSOBJINFO info;
	uint64_t nanosec;

	rc = sfprov_getinfo(mnt, path, &info);
	if (rc)
		return (rc);
	nanosec = RTTimeSpecGetNano(&info.ModificationTime);
	time->tv_sec = nanosec / 1000000000;
	time->tv_nsec = nanosec % 1000000000;
	return (0);
}

int
sfprov_get_ctime(sfp_mount_t *mnt, char *path, timestruc_t *time)
{
	int rc;
	RTFSOBJINFO info;
	uint64_t nanosec;

	rc = sfprov_getinfo(mnt, path, &info);
	if (rc)
		return (rc);
	nanosec = RTTimeSpecGetNano(&info.ChangeTime);
	time->tv_sec = nanosec / 1000000000;
	time->tv_nsec = nanosec % 1000000000;
	return (0);
}

/*
 * Directory operations
 */
int
sfprov_mkdir(sfp_mount_t *mnt, char *path, sfp_file_t **fp)
{
	int rc;
	SHFLCREATEPARMS parms;
	SHFLSTRING *str;
	int size;
	sfp_file_t *newfp;

	str = sfprov_string(path, &size);
	parms.Handle = 0;
	parms.Info.cbObject = 0;
	parms.CreateFlags = SHFL_CF_DIRECTORY | SHFL_CF_ACT_CREATE_IF_NEW |
	    SHFL_CF_ACT_FAIL_IF_EXISTS | SHFL_CF_ACCESS_READ;
	rc = vboxCallCreate(&vbox_client, &mnt->map, str, &parms);
	kmem_free(str, size);

	if (RT_FAILURE(rc))
		return (EINVAL);
	if (parms.Handle == SHFL_HANDLE_NIL) {
		if (parms.Result == SHFL_FILE_EXISTS)
			return (EEXIST);
		return (ENOENT);
	}
	newfp = kmem_alloc(sizeof(sfp_file_t), KM_SLEEP);
	newfp->handle = parms.Handle;
	newfp->map = mnt->map;
	*fp = newfp;
	return (0);
}

int
sfprov_remove(sfp_mount_t *mnt, char *path)
{
	int rc;
	SHFLSTRING *str;
	int size;

	str = sfprov_string(path, &size);
	rc = vboxCallRemove(&vbox_client, &mnt->map, str, SHFL_REMOVE_FILE);
	kmem_free(str, size);
	if (RT_FAILURE(rc))
		return (EINVAL);
	return (0);
}

int
sfprov_rmdir(sfp_mount_t *mnt, char *path)
{
	int rc;
	SHFLSTRING *str;
	int size;

	str = sfprov_string(path, &size);
	rc = vboxCallRemove(&vbox_client, &mnt->map, str, SHFL_REMOVE_DIR);
	kmem_free(str, size);
	if (RT_FAILURE(rc))
		return (RTErrConvertToErrno(rc));
	return (0);
}

int
sfprov_rename(sfp_mount_t *mnt, char *from, char *to, uint_t is_dir)
{
	int rc;
	SHFLSTRING *old, *new;
	int old_size, new_size;

	old = sfprov_string(from, &old_size);
	new = sfprov_string(to, &new_size);
	rc = vboxCallRename(&vbox_client, &mnt->map, old, new,
	    (is_dir ? SHFL_RENAME_DIR : SHFL_RENAME_FILE) |
	    SHFL_RENAME_REPLACE_IF_EXISTS);
	kmem_free(old, old_size);
	kmem_free(new, new_size);
	if (RT_FAILURE(rc))
		return (RTErrConvertToErrno(rc));
	return (0);
}


/*
 * Read all filenames in a directory.
 *
 * - success - all entries read and returned
 * - ENOENT - Couldn't open the directory for reading
 * - EINVAL - Internal error of some kind
 *
 * On successful return, buffer[0] is the start of an array of "char *"
 * pointers to the filenames. The array ends with a NULL pointer.
 * The remaining storage in buffer after that NULL pointer is where the
 * filename strings actually are.
 *
 * On input nents is the max number of filenames the requestor can handle.
 * On output nents is the number of entries at buff[0]
 *
 * The caller is responsible for freeing the returned buffer.
 */
int
sfprov_readdir(
	sfp_mount_t *mnt,
	char *path,
	void **buffer,
	size_t *buffersize,
	uint32_t *nents)
{
	int error;
	char *cp;
	int len;
	SHFLSTRING *mask_str = NULL;	/* must be path with "/*" appended */
	int mask_size;
	sfp_file_t *fp;
	void *buff_start = NULL;
	char **curr_b;
	char *buff_end;
	size_t buff_size;
	static char infobuff[2 * MAXNAMELEN];	/* not on stack!! */
	SHFLDIRINFO *info = (SHFLDIRINFO *)&infobuff;
	uint32_t numbytes = sizeof (infobuff);
	uint32_t justone;
	uint32_t cnt;
	char **name_ptrs;

	*buffer = NULL;
	*buffersize = 0;
	if (*nents == 0)
		return (EINVAL);
	error = sfprov_open(mnt, path, &fp);
	if (error != 0)
		return (ENOENT);

	/*
	 * Create mask that VBox expects. This needs to be the directory path,
	 * plus a "*" wildcard to get all files.
	 */
	len = strlen(path) + 3;
	cp = kmem_alloc(len, KM_SLEEP);
	strcpy(cp, path);
	strcat(cp, "/*");
	mask_str = sfprov_string(cp, &mask_size);
	kmem_free(cp, len);

	/*
	 * Allocate the buffer to use for return values. Each entry
	 * in the buffer will have a pointer and the string itself.
	 * The pointers go in the front of the buffer, the strings
	 * at the end.
	 */
	buff_size = *nents * (sizeof(char *) + MAXNAMELEN);
	name_ptrs = buff_start = kmem_alloc(buff_size, KM_SLEEP);
	cp = (char *)buff_start + buff_size;

	/*
	 * Now loop using vboxCallDirInfo to get one file name at a time
	 */
	cnt = 0;
	for (;;) {
		justone = 1;
		numbytes = sizeof (infobuff);
		error = vboxCallDirInfo(&vbox_client, &fp->map, fp->handle,
		    mask_str, SHFL_LIST_RETURN_ONE, cnt, &numbytes, info,
		    &justone);
		if (error == VERR_NO_MORE_FILES) {
			break;
		}
		if (error == VERR_NO_TRANSLATION) {
			continue;	/* ?? just skip this one */
		}
		if (error != VINF_SUCCESS || justone != 1) {
			error = EINVAL;
	 		goto done;
		}

		/*
		 * Put this name in the buffer, stop if we run out of room.
		 */
		cp -= strlen(info->name.String.utf8) + 1;
		if (cp < (char *)(&name_ptrs[cnt + 2]))
			break;
		strcpy(cp, info->name.String.utf8);
		name_ptrs[cnt] = cp;
		++cnt;
	}
	error = 0;
	name_ptrs[cnt] = NULL;
	*nents = cnt;
	*buffer = buff_start;
	*buffersize = buff_size;
done:
	if (error != 0)
		kmem_free(buff_start, buff_size);
	kmem_free(mask_str, mask_size);
	sfprov_close(fp);
	return (error);
}
