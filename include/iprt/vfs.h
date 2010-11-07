/** @file
 * IPRT - Virtual Filesystem.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
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

#ifndef ___iprt_vfs_h
#define ___iprt_vfs_h

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/dir.h>
#include <iprt/fs.h>
#include <iprt/symlink.h>
#include <iprt/sg.h>
#include <iprt/time.h>


RT_C_DECLS_BEGIN

/** @defgroup grp_rt_fs    RTVfs - Virtual Filesystem
 * @ingroup grp_rt
 * @{
 */

/** Virtual Filesystem handle. */
typedef struct RTVFSINTERNAL           *RTVFS;
/** Pointer to a VFS handle. */
typedef RTVFS                          *PRTVFS;
/** A NIL VFS directory handle. */
#define NIL_RTVFS                       ((RTVFS)~(uintptr_t)0)

/** Virtual Filesystem directory handle. */
typedef struct RTVFSDIRINTERNAL        *RTVFSDIR;
/** Pointer to a VFS directory handle. */
typedef RTVFSDIR                       *PRTVFSDIR;
/** A NIL VFS directory handle. */
#define NIL_RTVFSDIR                    ((RTVFSDIR)~(uintptr_t)0)

/** Virtual Filesystem I/O stream handle. */
typedef struct RTVFSIOSTREAMINTERNAL   *RTVFSIOSTREAM;
/** Pointer to a VFS I/O stream handle. */
typedef RTVFSIOSTREAM                  *PRTVFSIOSTREAM;
/** A NIL VFS I/O stream handle. */
#define NIL_RTVFSIOSTREAM               ((RTVFSIOSTREAM)~(uintptr_t)0)

/** Virtual Filesystem file handle. */
typedef struct RTVFSFILEINTERNAL       *RTVFSFILE;
/** Pointer to a VFS file handle. */
typedef RTVFSFILE                      *PRTVFSFILE;
/** A NIL VFS file handle. */
#define NIL_RTVFSFILE                   ((RTVFSFILE)~(uintptr_t)0)

/** Virtual Filesystem symbolic link handle. */
typedef struct RTVFSSYMLINKINTERNAL    *RTVFSSYMLINK;
/** Pointer to a VFS symbolic link handle. */
typedef RTVFSSYMLINK                   *PRTVFSSYMLINK;
/** A NIL VFS symbolic link handle. */
#define NIL_RTVFSSYMLINK                ((RTVFSSYMLINK)~(uintptr_t)0)


/** @name RTVfsCreate flags
 * @{ */
/** Whether the file system is read-only. */
#define RTVFS_C_READONLY                RT_BIT(0)
/** Whether we the VFS should be thread safe (i.e. automaticaly employ
 * locks). */
#define RTVFS_C_THREAD_SAFE             RT_BIT(1)
/** @}  */

/**
 * Creates an empty virtual filesystem.
 *
 * @returns IPRT status code.
 * @param   pszName     Name, for logging and such.
 * @param   fFlags      Flags, MBZ.
 * @param   phVfs       Where to return the VFS handle.  Release the returned
 *                      reference by calling RTVfsRelease.
 */
RTDECL(int)         RTVfsCreate(const char *pszName, uint32_t fFlags, PRTVFS phVfs);
RTDECL(uint32_t)    RTVfsRetain(RTVFS phVfs);
RTDECL(uint32_t)    RTVfsRelease(RTVFS phVfs);
RTDECL(int)         RTVfsAttach(RTVFS hVfs, const char *pszMountPoint, uint32_t fFlags, RTVFS hVfsAttach);
RTDECL(int)         RTVfsDetach(RTVFS hVfs, const char *pszMountPoint, RTVFS hVfsToDetach, PRTVFS *phVfsDetached);
RTDECL(uint32_t)    RTVfsGetAttachmentCount(RTVFS hVfs);
RTDECL(int)         RTVfsGetAttachment(RTVFS hVfs, uint32_t iOrdinal, PRTVFS *phVfsAttached, uint32_t *pfFlags,
                                       char *pszMountPoint, size_t cbMountPoint);

/** @defgroup grp_vfs_dir           VFS Directory API
 * @{
 */
/** @}  */


/** @defgroup grp_vfs_iostream      VFS I/O Stream
 * @{
 */
RTDECL(uint32_t)    RTVfsIoStrmRetain(RTVFSIOSTREAM hVfsIos);
RTDECL(uint32_t)    RTVfsIoStrmRelease(RTVFSIOSTREAM hVfsIos);
RTDECL(RTVFSFILE)   RTVfsIoStrmToFile(RTVFSIOSTREAM hVfsIos);
RTDECL(int)         RTVfsIoStrmQueryInfo(RTVFSIOSTREAM hVfsIos, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr);
RTDECL(int)         RTVfsIoStrmRead(RTVFSIOSTREAM hVfsIos, void *pvBuf, size_t cbToRead, size_t *pcbRead);
RTDECL(int)         RTVfsIoStrmWrite(RTVFSIOSTREAM hVfsIos, const void *pvBuf, size_t cbToWrite, size_t *pcbWritten);
RTDECL(int)         RTVfsIoStrmSgRead(RTVFSIOSTREAM hVfsIos, PCRTSGBUF pSgBuf, size_t *pcbRead);
RTDECL(int)         RTVfsIoStrmSgWrite(RTVFSIOSTREAM hVfsIos, PCRTSGBUF pSgBuf, size_t *pcbWritten);
RTDECL(int)         RTVfsIoStrmFlush(RTVFSIOSTREAM hVfsIos);
RTDECL(RTFOFF)      RTVfsIoStrmPoll(RTVFSIOSTREAM hVfsIos, uint32_t fEvents, RTMSINTERVAL cMillies, bool fIntr,
                                    uint32_t *pfRetEvents);
RTDECL(RTFOFF)      RTVfsIoStrmTell(RTVFSIOSTREAM hVfsIos);
/** @} */


/** @defgroup grp_vfs_file          VFS File API
 * @{
 */
RTDECL(int)         RTVfsFileOpen(RTVFS hVfs, const char *pszFilename, uint32_t fOpen, PRTVFSFILE phVfsFile);

/**
 * Create a VFS file handle from a standard IPRT file handle (RTFILE).
 *
 * @returns IPRT status code.
 * @param   hFile           The standard IPRT file handle.
 * @param   fOpen           The flags the handle was opened with.  Pass 0 to
 *                          have these detected.
 * @param   fLeaveOpen      Whether to leave the handle open when the VFS file
 *                          is released, or to close it (@c false).
 * @param   phVfsFile       Where to return the VFS file handle.
 */
RTDECL(int)         RTVfsFileFromRTFile(RTFILE hFile, uint32_t fOpen, bool fLeaveOpen, PRTVFSFILE phVfsFile);
RTDECL(RTHCUINTPTR) RTVfsFileToNative(RTFILE hVfsFile);

/**
 * Convert the VFS file handle to a VFS I/O stream handle.
 *
 * @returns The VFS I/O stream handle on success, this must be released.
 *          NIL_RTVFSIOSTREAM if the file handle is invalid.
 * @param   hVfsFile        The VFS file handle.
 */
RTDECL(RTVFSIOSTREAM) RTVfsFileToIoStream(RTVFSFILE hVfsFile);

/**
 * Retains a reference to the VFS file handle.
 *
 * @returns New reference count on success, UINT32_MAX on failure.
 * @param   hVfsFile        The VFS file handle.
 */
RTDECL(uint32_t)    RTVfsFileRetain(RTVFSFILE hVfsFile);

/**
 * Releases a reference to the VFS file handle.
 *
 * @returns New reference count on success (0 if closed), UINT32_MAX on failure.
 * @param   hVfsFile        The VFS file handle.
 */
RTDECL(uint32_t)    RTVfsFileRelease(RTVFSFILE hVfsFile);

RTDECL(int)         RTVfsFileQueryInfo(RTVFSFILE hVfsFile, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr);
RTDECL(int)         RTVfsFileRead(RTVFSFILE hVfsFile, void *pvBuf, size_t cbToRead, size_t *pcbRead);
RTDECL(int)         RTVfsFileReadAt(RTVFSFILE hVfsFile, RTFOFF off, void *pvBuf, size_t cbToRead, size_t *pcbRead);
RTDECL(int)         RTVfsFileWrite(RTVFSFILE hVfsFile, const void *pvBuf, size_t cbToWrite, size_t *pcbWritten);
RTDECL(int)         RTVfsFileWriteAt(RTVFSFILE hVfsFile, RTFOFF off, const void *pvBuf, size_t cbToWrite, size_t *pcbWritten);
RTDECL(int)         RTVfsFileFlush(RTVFSFILE hVfsFile);
RTDECL(RTFOFF)      RTVfsFilePoll(RTVFSFILE hVfsFile, uint32_t fEvents, RTMSINTERVAL cMillies, bool fIntr,
                                  uint32_t *pfRetEvents);
RTDECL(RTFOFF)      RTVfsFileTell(RTVFSFILE hVfsFile);

RTDECL(int)         RTVfsFileSeek(RTVFSFILE hVfsFile, RTFOFF offSeek, uint32_t uMethod, uint64_t *poffActual);
RTDECL(int)         RTVfsFileSetSize(RTVFSFILE hVfsFile, uint64_t cbSize);
RTDECL(int)         RTVfsFileGetSize(RTVFSFILE hVfsFile, uint64_t *pcbSize);
RTDECL(RTFOFF)      RTVfsFileGetMaxSize(RTVFSFILE hVfsFile);
RTDECL(int)         RTVfsFileGetMaxSizeEx(RTVFSFILE hVfsFile, PRTFOFF pcbMax);

/** @} */

/** @} */

RT_C_DECLS_END

#endif /* !___iprt_vfs_h */


