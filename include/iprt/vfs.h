/** @file
 * IPRT - Virtual Filesystem.
 */

/*
 * Copyright (C) 2010-2015 Oracle Corporation
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
#include <iprt/handle.h>
#include <iprt/symlink.h>
#include <iprt/sg.h>
#include <iprt/time.h>


RT_C_DECLS_BEGIN

/** @defgroup grp_rt_vfs   RTVfs - Virtual Filesystem
 * @ingroup grp_rt
 *
 * The virtual filesystem APIs are intended to make it possible to work on
 * container files, file system sub-trees, file system overlays and other custom
 * filesystem configurations.  It also makes it possible to create filters, like
 * automatically gunzipping a tar.gz file before feeding it to the RTTar API for
 * unpacking - or wise versa.
 *
 * The virtual filesystem APIs are intended to mirror the RTDir, RTFile, RTPath
 * and RTFs APIs pretty closely so that rewriting a piece of code to work with
 * it should be easy.  However there are some differences to the way the APIs
 * works and the user should heed the documentation.  The differences are
 * usually motivated by simplification and in some case to make the VFS more
 * flexible.
 *
 * @{
 */

/**
 * The object type.
 */
typedef enum RTVFSOBJTYPE
{
    /** Invalid type. */
    RTVFSOBJTYPE_INVALID = 0,
    /** Pure base object.
     * This is returned by the filesystem stream to represent directories,
     * devices, fifos and similar that needs to be created. */
    RTVFSOBJTYPE_BASE,
    /** Virtual filesystem. */
    RTVFSOBJTYPE_VFS,
    /** Filesystem stream. */
    RTVFSOBJTYPE_FS_STREAM,
    /** Pure I/O stream. */
    RTVFSOBJTYPE_IO_STREAM,
    /** Directory. */
    RTVFSOBJTYPE_DIR,
    /** File. */
    RTVFSOBJTYPE_FILE,
    /** Symbolic link. */
    RTVFSOBJTYPE_SYMLINK,
    /** End of valid object types. */
    RTVFSOBJTYPE_END,
    /** Pure I/O stream. */
    RTVFSOBJTYPE_32BIT_HACK = 0x7fffffff
} RTVFSOBJTYPE;
/** Pointer to a VFS object type. */
typedef RTVFSOBJTYPE *PRTVFSOBJTYPE;



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

/**
 * Checks whether a given range is in use by the virtual filesystem.
 *
 * @returns IPRT status code.
 * @param   hVfs        VFS handle.
 * @param   off         Start offset to check.
 * @param   cb          Number of bytes to check.
 * @param   pfUsed      Where to store the result.
 */
RTDECL(int)         RTVfsIsRangeInUse(RTVFS hVfs, uint64_t off, size_t cb,
                                      bool *pfUsed);

/** @defgroup grp_vfs_obj           VFS Base Object API
 * @{
 */

/**
 * Retains a reference to the VFS base object handle.
 *
 * @returns New reference count on success, UINT32_MAX on failure.
 * @param   hVfsObj         The VFS base object handle.
 */
RTDECL(uint32_t)        RTVfsObjRetain(RTVFSOBJ hVfsObj);

/**
 * Releases a reference to the VFS base handle.
 *
 * @returns New reference count on success (0 if closed), UINT32_MAX on failure.
 * @param   hVfsObj         The VFS base object handle.
 */
RTDECL(uint32_t)        RTVfsObjRelease(RTVFSOBJ hVfsObj);

/**
 * Query information about the object.
 *
 * @returns IPRT status code.
 * @retval  VERR_NOT_SUPPORTED if the @a enmAddAttr value is not handled by the
 *          implementation.
 *
 * @param   hVfsObj         The VFS object handle.
 * @param   pObjInfo        Where to return the info.
 * @param   enmAddAttr      Which additional attributes should be retrieved.
 * @sa      RTVfsIoStrmQueryInfo, RTVfsFileQueryInfo, RTFileQueryInfo,
 *          RTPathQueryInfo
 */
RTDECL(int)             RTVfsObjQueryInfo(RTVFSOBJ hVfsObj, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr);


/**
 * Converts a VFS base object handle to a VFS handle.
 *
 * @returns Referenced handle on success, NIL on failure.
 * @param   hVfsObj         The VFS base object handle.
 */
RTDECL(RTVFS)           RTVfsObjToVfs(RTVFSOBJ hVfsObj);

/**
 * Converts a VFS base object handle to a VFS filesystem stream handle.
 *
 * @returns Referenced handle on success, NIL on failure.
 * @param   hVfsObj         The VFS base object handle.
 */
RTDECL(RTVFSFSSTREAM)   RTVfsObjToFsStream(RTVFSOBJ hVfsObj);

/**
 * Converts a VFS base object handle to a VFS directory handle.
 *
 * @returns Referenced handle on success, NIL on failure.
 * @param   hVfsObj         The VFS base object handle.
 */
RTDECL(RTVFSDIR)        RTVfsObjToDir(RTVFSOBJ hVfsObj);

/**
 * Converts a VFS base object handle to a VFS I/O stream handle.
 *
 * @returns Referenced handle on success, NIL on failure.
 * @param   hVfsObj         The VFS base object handle.
 */
RTDECL(RTVFSIOSTREAM)   RTVfsObjToIoStream(RTVFSOBJ hVfsObj);

/**
 * Converts a VFS base object handle to a VFS file handle.
 *
 * @returns Referenced handle on success, NIL on failure.
 * @param   hVfsObj         The VFS base object handle.
 */
RTDECL(RTVFSFILE)       RTVfsObjToFile(RTVFSOBJ hVfsObj);

/**
 * Converts a VFS base object handle to a VFS symbolic link handle.
 *
 * @returns Referenced handle on success, NIL on failure.
 * @param   hVfsObj         The VFS base object handle.
 */
RTDECL(RTVFSSYMLINK)    RTVfsObjToSymlink(RTVFSOBJ hVfsObj);


/**
 * Converts a VFS handle to a VFS base object handle.
 *
 * @returns Referenced handle on success, NIL if the input handle was invalid.
 * @param   hVfs            The VFS handle.
 */
RTDECL(RTVFSOBJ)        RTVfsObjFromVfs(RTVFS hVfs);

/**
 * Converts a VFS filesystem stream handle to a VFS base object handle.
 *
 * @returns Referenced handle on success, NIL if the input handle was invalid.
 * @param   hVfsFSs         The VFS filesystem stream handle.
 */
RTDECL(RTVFSOBJ)        RTVfsObjFromFsStream(RTVFSFSSTREAM hVfsFss);

/**
 * Converts a VFS directory handle to a VFS base object handle.
 *
 * @returns Referenced handle on success, NIL if the input handle was invalid.
 * @param   hVfsDir          The VFS directory handle.
 */
RTDECL(RTVFSOBJ)        RTVfsObjFromDir(RTVFSDIR hVfsDir);

/**
 * Converts a VFS I/O stream handle to a VFS base object handle.
 *
 * @returns Referenced handle on success, NIL if the input handle was invalid.
 * @param   hVfsIos          The VFS I/O stream handle.
 */
RTDECL(RTVFSOBJ)        RTVfsObjFromIoStream(RTVFSIOSTREAM hVfsIos);

/**
 * Converts a VFS file handle to a VFS base object handle.
 *
 * @returns Referenced handle on success, NIL if the input handle was invalid.
 * @param   hVfsFile         The VFS file handle.
 */
RTDECL(RTVFSOBJ)        RTVfsObjFromFile(RTVFSFILE hVfsFile);

/**
 * Converts a VFS symbolic link handle to a VFS base object handle.
 *
 * @returns Referenced handle on success, NIL if the input handle was invalid.
 * @param   hVfsSym            The VFS symbolic link handle.
 */
RTDECL(RTVFSOBJ)        RTVfsObjFromSymlink(RTVFSSYMLINK hVfsSym);

/** @} */


/** @defgroup grp_vfs_fsstream      VFS Filesystem Stream API
 *
 * Filesystem streams are for tar, cpio and similar.  Any virtual filesystem can
 * be turned into a filesystem stream using RTVfsFsStrmFromVfs.
 *
 * @{
 */

RTDECL(uint32_t)    RTVfsFsStrmRetain(RTVFSFSSTREAM hVfsFss);
RTDECL(uint32_t)    RTVfsFsStrmRelease(RTVFSFSSTREAM hVfsFss);
RTDECL(int)         RTVfsFsStrmQueryInfo(RTVFSFSSTREAM hVfsFss, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr);

/**
 * Gets the next object in the stream.
 *
 * This call may affect the stream posision of a previously returned object.
 *
 * The type of object returned here typically boils down to three types:
 *      - I/O streams (representing files),
 *      - symbolic links
 *      - base object
 * The base objects represent anything not convered by the two other, i.e.
 * directories, device nodes, fifos, sockets and whatnot.  The details can be
 * queried using RTVfsObjQueryInfo.
 *
 * That said, absolutely any object except for filesystem stream objects can be
 * returned by this call.  Any generic code is adviced to just deal with it all.
 *
 * @returns IPRT status code.
 * @retval  VINF_SUCCESS if a new object was retrieved.
 * @retval  VERR_EOF when there are no more objects.
 *
 * @param   pvThis      The implementation specific directory data.
 * @param   ppszName    Where to return the object name.  Must be freed by
 *                      calling RTStrFree.
 * @param   penmType    Where to return the object type.
 * @param   hVfsObj     Where to return the object handle (referenced).
 *                      This must be cast to the desired type before use.
 */
RTDECL(int)         RTVfsFsStrmNext(RTVFSFSSTREAM hVfsFss, char **ppszName, RTVFSOBJTYPE *penmType, PRTVFSOBJ phVfsObj);

/** @}  */


/** @defgroup grp_vfs_dir           VFS Directory API
 * @{
 */

/**
 * Retains a reference to the VFS directory handle.
 *
 * @returns New reference count on success, UINT32_MAX on failure.
 * @param   hVfsDir         The VFS directory handle.
 */
RTDECL(uint32_t)    RTVfsDirRetain(RTVFSDIR hVfsDir);

/**
 * Releases a reference to the VFS directory handle.
 *
 * @returns New reference count on success (0 if closed), UINT32_MAX on failure.
 * @param   hVfsIos         The VFS directory handle.
 */
RTDECL(uint32_t)    RTVfsDirRelease(RTVFSDIR hVfsDir);

/** @}  */


/** @defgroup grp_vfs_symlink       VFS Symbolic Link API
 *
 * @remarks The TAR VFS and filesystem stream uses symbolic links for
 *          describing hard links as well.  The users must use RTFS_IS_SYMLINK
 *          to check if it is a real symlink in those cases.
 *
 * @remarks Any VFS which is backed by a real file system may be subject to
 *          races with other processes or threads, so the user may get
 *          unexpected errors when this happends.  This is a bit host specific,
 *          i.e. it might be prevent on windows if we care.
 *
 * @{
 */


/**
 * Retains a reference to the VFS symbolic link handle.
 *
 * @returns New reference count on success, UINT32_MAX on failure.
 * @param   hVfsSym         The VFS symbolic link handle.
 */
RTDECL(uint32_t)    RTVfsSymlinkRetain(RTVFSSYMLINK hVfsSym);

/**
 * Releases a reference to the VFS symbolic link handle.
 *
 * @returns New reference count on success (0 if closed), UINT32_MAX on failure.
 * @param   hVfsSym         The VFS symbolic link handle.
 */
RTDECL(uint32_t)    RTVfsSymlinkRelease(RTVFSSYMLINK hVfsSym);

/**
 * Query information about the symbolic link.
 *
 * @returns IPRT status code.
 * @param   hVfsSym         The VFS symbolic link handle.
 * @param   pObjInfo        Where to return the info.
 * @param   enmAddAttr      Which additional attributes should be retrieved.
 *
 * @sa      RTFileQueryInfo, RTPathQueryInfo, RTPathQueryInfoEx
 */
RTDECL(int)         RTVfsSymlinkQueryInfo(RTVFSSYMLINK hVfsSym, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr);

/**
 * Set the unix style owner and group.
 *
 * @returns IPRT status code.
 * @param   hVfsSym         The VFS symbolic link handle.
 * @param   fMode           The new mode bits.
 * @param   fMask           The mask indicating which bits we are changing.
 * @sa      RTFileSetMode, RTPathSetMode
 */
RTDECL(int)         RTVfsSymlinkSetMode(RTVFSSYMLINK hVfsSym, RTFMODE fMode, RTFMODE fMask);

/**
 * Set the timestamps associated with the object.
 *
 * @returns IPRT status code.
 * @param   hVfsSym         The VFS symbolic link handle.
 * @param   pAccessTime     Pointer to the new access time. NULL if not
 *                          to be changed.
 * @param   pModificationTime   Pointer to the new modifcation time. NULL if
 *                              not to be changed.
 * @param   pChangeTime     Pointer to the new change time. NULL if not to be
 *                          changed.
 * @param   pBirthTime      Pointer to the new time of birth. NULL if not to be
 *                          changed.
 * @remarks See RTFileSetTimes for restrictions and behavior imposed by the
 *          host OS or underlying VFS provider.
 * @sa      RTFileSetTimes, RTPathSetTimes
 */
RTDECL(int)         RTVfsSymlinkSetTimes(RTVFSSYMLINK hVfsSym, PCRTTIMESPEC pAccessTime, PCRTTIMESPEC pModificationTime,
                                         PCRTTIMESPEC pChangeTime, PCRTTIMESPEC pBirthTime);

/**
 * Set the unix style owner and group.
 *
 * @returns IPRT status code.
 * @param   hVfsSym         The VFS symbolic link handle.
 * @param   uid             The user ID of the new owner.  NIL_RTUID if
 *                          unchanged.
 * @param   gid             The group ID of the new owner group. NIL_RTGID if
 *                          unchanged.
 * @sa      RTFileSetOwner, RTPathSetOwner.
 */
RTDECL(int)         RTVfsSymlinkSetOwner(RTVFSSYMLINK hVfsSym, RTUID uid, RTGID gid);

/**
 * Read the symbolic link target.
 *
 * @returns IPRT status code.
 * @param   hVfsSym         The VFS symbolic link handle.
 * @param   pszTarget       The target buffer.
 * @param   cbTarget        The size of the target buffer.
 * @sa      RTSymlinkRead
 */
RTDECL(int)         RTVfsSymlinkRead(RTVFSSYMLINK hVfsSym, char *pszTarget, size_t cbTarget);

/** @}  */



/** @defgroup grp_vfs_iostream      VFS I/O Stream API
 * @{
 */

/**
 * Creates a VFS I/O stream handle from a standard IPRT file handle (RTFILE).
 *
 * @returns IPRT status code.
 * @param   hFile           The standard IPRT file handle.
 * @param   fOpen           The flags the handle was opened with.  Pass 0 to
 *                          have these detected.
 * @param   fLeaveOpen      Whether to leave the handle open when the VFS file
 *                          is released, or to close it (@c false).
 * @param   phVfsIos        Where to return the VFS I/O stream handle.
 */
RTDECL(int)         RTVfsIoStrmFromRTFile(RTFILE hFile, uint64_t fOpen, bool fLeaveOpen, PRTVFSIOSTREAM phVfsIos);

/**
 * Creates a VFS I/O stream handle from a standard IPRT pipe handle (RTPIPE).
 *
 * @returns IPRT status code.
 * @param   hPipe           The standard IPRT pipe handle.
 * @param   fLeaveOpen      Whether to leave the handle open when the VFS file
 *                          is released, or to close it (@c false).
 * @param   phVfsIos        Where to return the VFS I/O stream handle.
 */
RTDECL(int)         RTVfsIoStrmFromRTPipe(RTPIPE hPipe, bool fLeaveOpen, PRTVFSIOSTREAM phVfsIos);

/**
 * Convenience function combining RTFileOpen with RTVfsIoStrmFromRTFile.
 *
 * @returns IPRT status code.
 * @param   pszFilename     The path to the file in the normal file system.
 * @param   fOpen           The flags to pass to RTFileOpen when opening the
 *                          file, i.e. RTFILE_O_XXX.
 * @param   phVfsIos        Where to return the VFS I/O stream handle.
 */
RTDECL(int)         RTVfsIoStrmOpenNormal(const char *pszFilename, uint64_t fOpen, PRTVFSIOSTREAM phVfsIos);

/**
 * Create a VFS I/O stream handle from one of the standard handles.
 *
 * @returns IPRT status code.
 * @param   enmStdHandle    The standard IPRT file handle.
 * @param   fOpen           The flags the handle was opened with.  Pass 0 to
 *                          have these detected.
 * @param   fLeaveOpen      Whether to leave the handle open when the VFS file
 *                          is released, or to close it (@c false).
 * @param   phVfsIos        Where to return the VFS I/O stream handle.
 */
RTDECL(int)         RTVfsIoStrmFromStdHandle(RTHANDLESTD enmStdHandle, uint64_t fOpen, bool fLeaveOpen,
                                             PRTVFSIOSTREAM phVfsIos);

/**
 * Retains a reference to the VFS I/O stream handle.
 *
 * @returns New reference count on success, UINT32_MAX on failure.
 * @param   hVfsIos         The VFS I/O stream handle.
 */
RTDECL(uint32_t)    RTVfsIoStrmRetain(RTVFSIOSTREAM hVfsIos);

/**
 * Releases a reference to the VFS I/O stream handle.
 *
 * @returns New reference count on success (0 if closed), UINT32_MAX on failure.
 * @param   hVfsIos         The VFS I/O stream handle.
 */
RTDECL(uint32_t)    RTVfsIoStrmRelease(RTVFSIOSTREAM hVfsIos);

/**
 * Convert the VFS I/O stream handle to a VFS file handle.
 *
 * @returns The VFS file handle on success, this must be released.
 *          NIL_RTVFSFILE if the I/O stream handle is invalid.
 * @param   hVfsIos         The VFS I/O stream handle.
 * @sa      RTVfsFileToIoStream
 */
RTDECL(RTVFSFILE)   RTVfsIoStrmToFile(RTVFSIOSTREAM hVfsIos);

/**
 * Query information about the I/O stream.
 *
 * @returns IPRT status code.
 * @param   hVfsIos         The VFS I/O stream handle.
 * @param   pObjInfo        Where to return the info.
 * @param   enmAddAttr      Which additional attributes should be retrieved.
 * @sa      RTFileQueryInfo
 */
RTDECL(int)         RTVfsIoStrmQueryInfo(RTVFSIOSTREAM hVfsIos, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr);

/**
 * Read bytes from the I/O stream.
 *
 * @returns IPRT status code.
 * @retval  VINF_SUCCESS and the number of bytes read written to @a pcbRead.
 * @retval  VINF_TRY_AGAIN if @a fBlocking is @c false, @a pcbRead is not NULL,
 *          and no data was available. @a *pcbRead will be set to 0.
 * @retval  VINF_EOF when trying to read __beyond__ the end of the stream and
 *          @a pcbRead is not NULL (it will be set to the number of bytes read,
 *          or 0 if the end of the stream was reached before this call).
 *          When the last byte of the read request is the last byte in the
 *          stream, this status code will not be used.  However, VINF_EOF is
 *          returned when attempting to read 0 bytes while standing at the end
 *          of the stream.
 * @retval  VERR_EOF when trying to read __beyond__ the end of the stream and
 *          @a pcbRead is NULL.
 * @retval  VERR_ACCESS_DENIED if the stream is not readable.
 *
 * @param   hVfsIos         The VFS I/O stream handle.
 * @param   pvBuf           Where to store the read bytes.
 * @param   cbToRead        The number of bytes to read.
 * @param   fBlocking       Whether the call is blocking (@c true) or not.  If
 *                          not, the @a pcbRead parameter must not be NULL.
 * @param   pcbRead         Where to always store the number of bytes actually
 *                          read.  This can be NULL if @a fBlocking is true.
 * @sa      RTVfsFileRead, RTFileRead, RTPipeRead, RTPipeReadBlocking,
 *          RTSocketRead
 */
RTDECL(int)         RTVfsIoStrmRead(RTVFSIOSTREAM hVfsIos, void *pvBuf, size_t cbToRead, bool fBlocking, size_t *pcbRead);
RTDECL(int)         RTVfsIoStrmReadAt(RTVFSIOSTREAM hVfsIos, RTFOFF off, void *pvBuf, size_t cbToRead, bool fBlocking, size_t *pcbRead);

/**
 * Write bytes to the I/O stream.
 *
 * @returns IPRT status code.
 * @retval  VERR_ACCESS_DENIED if the stream is not writable.
 *
 * @param   hVfsIos         The VFS I/O stream handle.
 * @param   pvBuf           The bytes to write.
 * @param   cbToWrite       The number of bytes to write.
 * @param   fBlocking       Whether the call is blocking (@c true) or not.  If
 *                          not, the @a pcbWritten parameter must not be NULL.
 * @param   pcbRead         Where to always store the number of bytes actually
 *                          written.  This can be NULL if @a fBlocking is true.
 * @sa      RTVfsFileWrite, RTFileWrite, RTPipeWrite, RTPipeWriteBlocking,
 *          RTSocketWrite
 */
RTDECL(int)         RTVfsIoStrmWrite(RTVFSIOSTREAM hVfsIos, const void *pvBuf, size_t cbToWrite, bool fBlocking, size_t *pcbWritten);
RTDECL(int)         RTVfsIoStrmWriteAt(RTVFSIOSTREAM hVfsIos, RTFOFF off, const void *pvBuf, size_t cbToWrite, bool fBlocking, size_t *pcbWritten);

/**
 * Reads bytes from the I/O stream into a scatter buffer.
 *
 * @returns IPRT status code.
 * @retval  VINF_SUCCESS and the number of bytes read written to @a pcbRead.
 * @retval  VINF_TRY_AGAIN if @a fBlocking is @c false, @a pcbRead is not NULL,
 *          and no data was available. @a *pcbRead will be set to 0.
 * @retval  VINF_EOF when trying to read __beyond__ the end of the stream and
 *          @a pcbRead is not NULL (it will be set to the number of bytes read,
 *          or 0 if the end of the stream was reached before this call).
 *          When the last byte of the read request is the last byte in the
 *          stream, this status code will not be used.  However, VINF_EOF is
 *          returned when attempting to read 0 bytes while standing at the end
 *          of the stream.
 * @retval  VERR_EOF when trying to read __beyond__ the end of the stream and
 *          @a pcbRead is NULL.
 * @retval  VERR_ACCESS_DENIED if the stream is not readable.
 *
 * @param   hVfsIos         The VFS I/O stream handle.
 * @param   pSgBuf          Pointer to a scatter buffer descriptor.  The number
 *                          of bytes described by the segments is what will be
 *                          attemted read.
 * @param   fBlocking       Whether the call is blocking (@c true) or not.  If
 *                          not, the @a pcbRead parameter must not be NULL.
 * @param   pcbRead         Where to always store the number of bytes actually
 *                          read.  This can be NULL if @a fBlocking is true.
 * @sa      RTFileSgRead, RTSocketSgRead, RTPipeRead, RTPipeReadBlocking
 */
RTDECL(int)         RTVfsIoStrmSgRead(RTVFSIOSTREAM hVfsIos, PCRTSGBUF pSgBuf, bool fBlocking, size_t *pcbRead);

/**
 * Write bytes to the I/O stream from a gather buffer.
 *
 * @returns IPRT status code.
 * @retval  VERR_ACCESS_DENIED if the stream is not writable.
 *
 * @param   hVfsIos         The VFS I/O stream handle.
 * @param   pSgBuf          Pointer to a gather buffer descriptor.  The number
 *                          of bytes described by the segments is what will be
 *                          attemted written.
 * @param   fBlocking       Whether the call is blocking (@c true) or not.  If
 *                          not, the @a pcbWritten parameter must not be NULL.
 * @param   pcbWritten      Where to always store the number of bytes actually
 *                          written.  This can be NULL if @a fBlocking is true.
 * @sa      RTFileSgWrite, RTSocketSgWrite
 */
RTDECL(int)         RTVfsIoStrmSgWrite(RTVFSIOSTREAM hVfsIos, PCRTSGBUF pSgBuf, bool fBlocking, size_t *pcbWritten);

/**
 * Flush any buffered data to the I/O stream.
 *
 * @returns IPRT status code.
 * @param   hVfsIos         The VFS I/O stream handle.
 * @sa      RTVfsFileFlush, RTFileFlush, RTPipeFlush
 */
RTDECL(int)         RTVfsIoStrmFlush(RTVFSIOSTREAM hVfsIos);

/**
 * Poll for events.
 *
 * @returns IPRT status code.
 * @param   hVfsIos         The VFS I/O stream handle.
 * @param   fEvents         The events to poll for (RTPOLL_EVT_XXX).
 * @param   cMillies        How long to wait for event to eventuate.
 * @param   fIntr           Whether the wait is interruptible and can return
 *                          VERR_INTERRUPTED (@c true) or if this condition
 *                          should be hidden from the caller (@c false).
 * @param   pfRetEvents     Where to return the event mask.
 * @sa      RTVfsFilePoll, RTPollSetAdd, RTPoll, RTPollNoResume.
 */
RTDECL(int)         RTVfsIoStrmPoll(RTVFSIOSTREAM hVfsIos, uint32_t fEvents, RTMSINTERVAL cMillies, bool fIntr,
                                    uint32_t *pfRetEvents);
/**
 * Tells the current I/O stream position.
 *
 * @returns Zero or higher - where to return the I/O stream offset.  Values
 *          below zero are IPRT status codes (VERR_XXX).
 * @param   hVfsIos         The VFS I/O stream handle.
 * @sa      RTFileTell
 */
RTDECL(RTFOFF)      RTVfsIoStrmTell(RTVFSIOSTREAM hVfsIos);

/**
 * Skips @a cb ahead in the stream.
 *
 * @returns IPRT status code.
 * @param   hVfsIos         The VFS I/O stream handle.
 * @param   cb              The number bytes to skip.
 */
RTDECL(int)         RTVfsIoStrmSkip(RTVFSIOSTREAM hVfsIos, RTFOFF cb);

/**
 * Fills the stream with @a cb zeros.
 *
 * @returns IPRT status code.
 * @param   hVfsIos         The VFS I/O stream handle.
 * @param   cb              The number of zero bytes to insert.
 */
RTDECL(int)         RTVfsIoStrmZeroFill(RTVFSIOSTREAM hVfsIos, RTFOFF cb);

/**
 * Checks if we're at the end of the I/O stream.
 *
 * @returns true if at EOS, otherwise false.
 * @param   hVfsIos         The VFS I/O stream handle.
 */
RTDECL(bool)        RTVfsIoStrmIsAtEnd(RTVFSIOSTREAM hVfsIos);

/**
 * Process the rest of the stream, checking if it's all valid UTF-8 encoding.
 *
 * @returns VBox status cod.e
 *
 * @param   hVfsIos         The VFS I/O stream handle.
 * @param   fFlags          Flags governing the validation, see
 *                          RTVFS_VALIDATE_UTF8_XXX.
 * @param   poffError       Where to return the error offset. Optional.
 */
RTDECL(int)        RTVfsIoStrmValidateUtf8Encoding(RTVFSIOSTREAM hVfsIos, uint32_t fFlags, PRTFOFF poffError);

/** @defgroup RTVFS_VALIDATE_UTF8_XXX   RTVfsIoStrmValidateUtf8Encoding flags.
 * @{ */
/** The text must not contain any null terminator codepoints. */
#define RTVFS_VALIDATE_UTF8_NO_NULL         RT_BIT_32(0)
/** The codepoints must be in the range covered by RTC-3629.  */
#define RTVFS_VALIDATE_UTF8_BY_RTC_3629     RT_BIT_32(1)
/** Mask of valid flags. */
#define RTVFS_VALIDATE_UTF8_VALID_MASK      UINT32_C(0x00000003)
/** @}  */

/** @} */


/** @defgroup grp_vfs_file          VFS File API
 * @{
 */
RTDECL(int)         RTVfsFileOpen(RTVFS hVfs, const char *pszFilename, uint64_t fOpen, PRTVFSFILE phVfsFile);

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
RTDECL(int)         RTVfsFileFromRTFile(RTFILE hFile, uint64_t fOpen, bool fLeaveOpen, PRTVFSFILE phVfsFile);
RTDECL(RTHCUINTPTR) RTVfsFileToNative(RTFILE hVfsFile);

/**
 * Convenience function combining RTFileOpen with RTVfsFileFromRTFile.
 *
 * @returns IPRT status code.
 * @param   pszFilename     The path to the file in the normal file system.
 * @param   fOpen           The flags to pass to RTFileOpen when opening the
 *                          file, i.e. RTFILE_O_XXX.
 * @param   phVfsFile       Where to return the VFS file handle.
 */
RTDECL(int)         RTVfsFileOpenNormal(const char *pszFilename, uint64_t fOpen, PRTVFSFILE phVfsFile);

/**
 * Convert the VFS file handle to a VFS I/O stream handle.
 *
 * @returns The VFS I/O stream handle on success, this must be released.
 *          NIL_RTVFSIOSTREAM if the file handle is invalid.
 * @param   hVfsFile        The VFS file handle.
 * @sa      RTVfsIoStrmToFile
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

/**
 * Query information about the object.
 *
 * @returns IPRT status code.
 * @retval  VERR_NOT_SUPPORTED if the @a enmAddAttr value is not handled by the
 *          implementation.
 *
 * @param   hVfsObj         The VFS object handle.
 * @param   pObjInfo        Where to return the info.
 * @param   enmAddAttr      Which additional attributes should be retrieved.
 * @sa      RTVfsObjQueryInfo, RTVfsFsStrmQueryInfo, RTVfsDirQueryInfo,
 *          RTVfsIoStrmQueryInfo, RTVfsFileQueryInfo, RTFileQueryInfo,
 *          RTPathQueryInfo.
 */
RTDECL(int)         RTVfsFileQueryInfo(RTVFSFILE hVfsFile, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr);

/**
 * Read bytes from the file at the current position.
 *
 * @returns IPRT status code.
 * @retval  VINF_SUCCESS and the number of bytes read written to @a pcbRead.
 * @retval  VINF_TRY_AGAIN if @a fBlocking is @c false, @a pcbRead is not NULL,
 *          and no data was available. @a *pcbRead will be set to 0.
 * @retval  VINF_EOF when trying to read __beyond__ the end of the file and
 *          @a pcbRead is not NULL (it will be set to the number of bytes read,
 *          or 0 if the end of the file was reached before this call).
 *          When the last byte of the read request is the last byte in the
 *          file, this status code will not be used.  However, VINF_EOF is
 *          returned when attempting to read 0 bytes while standing at the end
 *          of the file.
 * @retval  VERR_EOF when trying to read __beyond__ the end of the file and
 *          @a pcbRead is NULL.
 * @retval  VERR_ACCESS_DENIED if the file is not readable.
 *
 * @param   hVfsFile        The VFS file handle.
 * @param   pvBuf           Where to store the read bytes.
 * @param   cbToRead        The number of bytes to read.
 * @param   fBlocking       Whether the call is blocking (@c true) or not.  If
 *                          not, the @a pcbRead parameter must not be NULL.
 * @param   pcbRead         Where to always store the number of bytes actually
 *                          read.  This can be NULL if @a fBlocking is true.
 * @sa      RTVfsIoStrmRead, RTFileRead, RTPipeRead, RTPipeReadBlocking,
 *          RTSocketRead
 */
RTDECL(int)         RTVfsFileRead(RTVFSFILE hVfsFile, void *pvBuf, size_t cbToRead, size_t *pcbRead);
RTDECL(int)         RTVfsFileReadAt(RTVFSFILE hVfsFile, RTFOFF off, void *pvBuf, size_t cbToRead, size_t *pcbRead);

/**
 * Write bytes to the file at the current position.
 *
 * @returns IPRT status code.
 * @retval  VERR_ACCESS_DENIED if the file is not writable.
 *
 * @param   hVfsFile        The VFS file handle.
 * @param   pvBuf           The bytes to write.
 * @param   cbToWrite       The number of bytes to write.
 * @param   fBlocking       Whether the call is blocking (@c true) or not.  If
 *                          not, the @a pcbWritten parameter must not be NULL.
 * @param   pcbRead         Where to always store the number of bytes actually
 *                          written.  This can be NULL if @a fBlocking is true.
 * @sa      RTVfsIoStrmRead, RTFileWrite, RTPipeWrite, RTPipeWriteBlocking,
 *          RTSocketWrite
 */
RTDECL(int)         RTVfsFileWrite(RTVFSFILE hVfsFile, const void *pvBuf, size_t cbToWrite, size_t *pcbWritten);
RTDECL(int)         RTVfsFileWriteAt(RTVFSFILE hVfsFile, RTFOFF off, const void *pvBuf, size_t cbToWrite, size_t *pcbWritten);

/**
 * Flush any buffered data to the file.
 *
 * @returns IPRT status code.
 * @param   hVfsFile        The VFS file handle.
 * @sa      RTVfsIoStrmFlush, RTFileFlush, RTPipeFlush
 */
RTDECL(int)         RTVfsFileFlush(RTVFSFILE hVfsFile);

/**
 * Poll for events.
 *
 * @returns IPRT status code.
 * @param   hVfsFile        The VFS file handle.
 * @param   fEvents         The events to poll for (RTPOLL_EVT_XXX).
 * @param   cMillies        How long to wait for event to eventuate.
 * @param   fIntr           Whether the wait is interruptible and can return
 *                          VERR_INTERRUPTED (@c true) or if this condition
 *                          should be hidden from the caller (@c false).
 * @param   pfRetEvents     Where to return the event mask.
 * @sa      RTVfsIoStrmPoll, RTPollSetAdd, RTPoll, RTPollNoResume.
 */
RTDECL(RTFOFF)      RTVfsFilePoll(RTVFSFILE hVfsFile, uint32_t fEvents, RTMSINTERVAL cMillies, bool fIntr,
                                  uint32_t *pfRetEvents);

/**
 * Tells the current file position.
 *
 * @returns Zero or higher - where to return the file offset.  Values
 *          below zero are IPRT status codes (VERR_XXX).
 * @param   hVfsFile        The VFS file handle.
 * @sa      RTFileTell, RTVfsIoStrmTell.
 */
RTDECL(RTFOFF)      RTVfsFileTell(RTVFSFILE hVfsFile);

/**
 * Changes the current read/write position of a file.
 *
 * @returns IPRT status code.
 *
 * @param   hVfsFile        The VFS file handle.
 * @param   offSeek         The seek offset.
 * @param   uMethod         The seek emthod.
 * @param   poffActual      Where to optionally return the new file offset.
 *
 * @sa      RTFileSeek
 */
RTDECL(int)         RTVfsFileSeek(RTVFSFILE hVfsFile, RTFOFF offSeek, uint32_t uMethod, uint64_t *poffActual);

RTDECL(int)         RTVfsFileSetSize(RTVFSFILE hVfsFile, uint64_t cbSize);
RTDECL(int)         RTVfsFileGetSize(RTVFSFILE hVfsFile, uint64_t *pcbSize);
RTDECL(RTFOFF)      RTVfsFileGetMaxSize(RTVFSFILE hVfsFile);
RTDECL(int)         RTVfsFileGetMaxSizeEx(RTVFSFILE hVfsFile, PRTFOFF pcbMax);

/** @} */


/** @defgroup grp_vfs_misc          VFS Miscellaneous
 * @{
 */

/**
 * Memorizes the I/O stream as a file backed by memory.
 *
 * @returns VBox status code.
 *
 * @param   hVfsIos         The VFS I/O stream to memorize.  This will be read
 *                          to the end on success, on failure its position is
 *                          undefined.
 * @param   fFlags          A combination of RTFILE_O_READ and RTFILE_O_WRITE.
 * @param   phVfsFile       Where to return the handle to the memory file on
 *                          success.
 */
RTDECL(int) RTVfsMemorizeIoStreamAsFile(RTVFSIOSTREAM hVfsIos, uint32_t fFlags, PRTVFSFILE phVfsFile);


/**
 * Pumps data from one I/O stream to another.
 *
 * The data is read in chunks from @a hVfsIosSrc and written to @a hVfsIosDst
 * until @hVfsIosSrc indicates end of stream.
 *
 * @returns IPRT status code
 *
 * @param   hVfsIosSrc  The input stream.
 * @param   hVfsIosDst  The output stream.
 * @param   cbBufHint   Hints at a good temporary buffer size, pass 0 if
 *                      clueless.
 */
RTDECL(int) RTVfsUtilPumpIoStreams(RTVFSIOSTREAM hVfsIosSrc, RTVFSIOSTREAM hVfsIosDst, size_t cbBufHint);

/** @}  */


/** @defgroup grp_rt_vfs_chain  VFS Chains
 *
 * VFS chains is for doing pipe like things with VFS objects from the command
 * line.  Imagine you want to cat the readme.gz of an ISO you could do
 * something like:
 *      RTCat :iprtvfs:vfs(isofs,./mycd.iso)|ios(open,readme.gz)|ios(gunzip)
 * or
 *      RTCat :iprtvfs:ios(isofs,./mycd.iso,/readme.gz)|ios(gunzip)
 *
 * The "isofs", "open" and "gunzip" bits in the above examples are chain
 * element providers registered with IPRT.  See RTVFSCHAINELEMENTREG for how
 * these works.
 *
 * @{ */

/** The path prefix used to identify an VFS chain specification. */
#define RTVFSCHAIN_SPEC_PREFIX   ":iprtvfs:"

RTDECL(int) RTVfsChainOpenVfs(      const char *pszSpec,                 PRTVFS          phVfs,     const char **ppszError);
RTDECL(int) RTVfsChainOpenFsStream( const char *pszSpec,                 PRTVFSFSSTREAM  phVfsFss,  const char **ppszError);
RTDECL(int) RTVfsChainOpenDir(      const char *pszSpec, uint64_t fOpen, PRTVFSDIR       phVfsDir,  const char **ppszError);
RTDECL(int) RTVfsChainOpenFile(     const char *pszSpec, uint64_t fOpen, PRTVFSFILE      phVfsFile, const char **ppszError);
RTDECL(int) RTVfsChainOpenSymlink(  const char *pszSpec,                 PRTVFSSYMLINK   phVfsSym,  const char **ppszError);
RTDECL(int) RTVfsChainOpenIoStream( const char *pszSpec, uint64_t fOpen, PRTVFSIOSTREAM  phVfsIos,  const char **ppszError);

/**
 * Tests if the given string is a chain specification or not.
 *
 * @returns true if it is, false if it isn't.
 * @param   pszSpec         The alleged chain spec.
 */
RTDECL(bool)    RTVfsChainIsSpec(const char *pszSpec);

/** @}  */


/** @} */

RT_C_DECLS_END

#endif /* !___iprt_vfs_h */

