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

#ifndef ___iprt_vfslowlevel_h
#define ___iprt_vfslowlevel_h

#include <iprt/vfs.h>
#include <iprt/param.h>


RT_C_DECLS_BEGIN

/** @defgroup grp_rt_vfs_lowlevel   RTVfs - Low-level Interface.
 * @ingroup grp_rt_vfs
 * @{
 */

/**
 * The VFS operations.
 */
typedef struct RTVFSOPS
{
    /** The structure version (RTVFSOPS_VERSION). */
    uint32_t                uVersion;
    /** The virtual file system feature mask.  */
    uint32_t                fFeatures;
    /** The name of the operations. */
    const char             *pszName;

    /**
     * Destructor.
     *
    * @param   pvThis      The implementation specific data.
     */
    DECLCALLBACKMEMBER(void, pfnDestroy)(void *pvThis);

    /**
     * Opens the root directory.
     *
     * @returns IPRT status code.
     * @param   pvThis      The implementation specific data.
     * @param   phVfsDir    Where to return the handle to the root directory.
     */
    DECLCALLBACKMEMBER(int, pfnOpenRoot)(void *pvThis, PRTVFSDIR phVfsDir);

    /** @todo There will be more methods here to optimize opening and
     *        querying. */

#if 0
    /**
     * Optional entry point for optimizing path traversal within the file system.
     *
     * @returns IPRT status code.
     * @param   pvThis      The implementation specific data.
     * @param   pszPath     The path to resolve.
     * @param   poffPath    The current path offset on input, what we've
     *                      traversed to on successful return.
     * @param   phVfs???    Return handle to what we've traversed.
     * @param   p???        Return other stuff...
     */
    DECLCALLBACKMEMBER(int, pfnTraverse)(void *pvThis, const char *pszPath, size_t *poffPath, PRTVFS??? phVfs?, ???* p???);
#endif

    /** Marks the end of the structure (RTVFSOPS_VERSION). */
    uintptr_t               uEndMarker;
} RTVFSOPS;
/** Pointer to constant VFS operations. */
typedef RTVFSOPS const *PCRTVFSOPS;

/** The RTVFSOPS structure version. */
#define RTVFSOPS_VERSION            RT_MAKE_U32_FROM_U8(0xff,0x0f,1,0)

/** @name RTVFSOPS::fFeatures
 * @{ */
/** The VFS supports attaching other systems. */
#define RTVFSOPS_FEAT_ATTACH        RT_BIT_32(0)
/** @}  */


/**
 * The object type.
 */
typedef enum RTVFSOBJTYPE
{
    /** Invalid type. */
    RTVFSOBJTYPE_INVALID = 0,
    /** Directory. */
    RTVFSOBJTYPE_DIR,
    /** Pure I/O stream. */
    RTVFSOBJTYPE_IOSTREAM,
    /** File. */
    RTVFSOBJTYPE_FILE,
    /** Symbolic link. */
    RTVFSOBJTYPE_SYMLINK,
    /** End of valid object types. */
    RTVFSOBJTYPE_END,
    /** Pure I/O stream. */
    RTVFSOBJTYPE_32BIT_HACK = 0x7fffffff
} RTVFSOBJTYPE;

/**
 * The basis for all virtual file system objects except RTVFS.
 */
typedef struct RTVFSOBJOPS
{
    /** The structure version (RTVFSOBJOPS_VERSION). */
    uint32_t                uVersion;
    /** The object type for type introspection. */
    RTVFSOBJTYPE            enmType;
    /** The name of the operations. */
    const char             *pszName;

    /**
     * Close the object.
     *
     * @returns IPRT status code.
     * @param   pvThis      The implementation specific file data.
     */
    DECLCALLBACKMEMBER(int, pfnClose)(void *pvThis);

    /**
     * Get information about the file.
     *
     * @returns IPRT status code.
     * @param   pvThis      The implementation specific file data.
     * @param   pObjInfo    Where to return the object info on success.
     * @param   enmAddAttr  Which set of additional attributes to request.
     * @sa      RTFileQueryInfo
     */
    DECLCALLBACKMEMBER(int, pfnQueryInfo)(void *pvThis, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr);

    /** Marks the end of the structure (RTVFSOBJOPS_VERSION). */
    uintptr_t               uEndMarker;
} RTVFSOBJOPS;
/** Pointer to constant VFS object operations. */
typedef RTVFSOBJOPS const *PCRTVFSOBJOPS;

/** The RTVFSOBJOPS structure version. */
#define RTVFSOBJOPS_VERSION         RT_MAKE_U32_FROM_U8(0xff,0x1f,1,0)


/**
 * Additional operations for setting object attributes.
 */
typedef struct RTVFSOBJSETOPS
{
    /** The structure version (RTVFSOBJSETOPS_VERSION). */
    uint32_t                uVersion;
    /** The offset to the RTVFSOBJOPS structure. */
    int32_t                 offObjOps;

    /**
     * Set the unix style owner and group.
     *
     * @returns IPRT status code.
     * @param   pvThis              The implementation specific file data.
     * @param   fMode               The new mode bits.
     * @param   fMask               The mask indicating which bits we are
     *                              changing.
     * @sa      RTFileSetMode
     */
    DECLCALLBACKMEMBER(int, pfnSetMode)(void *pvThis, RTFMODE fMode, RTFMODE fMask);

    /**
     * Set the timestamps associated with the object.
     *
     * @returns IPRT status code.
     * @param   pvThis              The implementation specific file data.
     * @param   pAccessTime         Pointer to the new access time. NULL if not
     *                              to be changed.
     * @param   pModificationTime   Pointer to the new modifcation time. NULL if
     *                              not to be changed.
     * @param   pChangeTime         Pointer to the new change time. NULL if not
     *                              to be changed.
     * @param   pBirthTime          Pointer to the new time of birth. NULL if
     *                              not to be changed.
     * @remarks See RTFileSetTimes for restrictions and behavior imposed by the
     *          host OS or underlying VFS provider.
     * @sa      RTFileSetTimes
     */
    DECLCALLBACKMEMBER(int, pfnSetTimes)(void *pvThis, PCRTTIMESPEC pAccessTime, PCRTTIMESPEC pModificationTime,
                                         PCRTTIMESPEC pChangeTime, PCRTTIMESPEC pBirthTime);

    /**
     * Set the unix style owner and group.
     *
     * @returns IPRT status code.
     * @param   pvThis      The implementation specific file data.
     * @param   uid         The user ID of the new owner.  NIL_RTUID if
     *                      unchanged.
     * @param   gid         The group ID of the new owner group.  NIL_RTGID if
     *                      unchanged.
     * @sa      RTFileSetOwner
     */
    DECLCALLBACKMEMBER(int, pfnSetOwner)(void *pvThis, RTUID uid, RTGID gid);

    /** Marks the end of the structure (RTVFSOBJSETOPS_VERSION). */
    uintptr_t               uEndMarker;
} RTVFSOBJSETOPS;
/** Pointer to const object attribute setter operations. */
typedef RTVFSOBJSETOPS const *PCRTVFSOBJSETOPS;

/** The RTVFSOBJSETOPS structure version. */
#define RTVFSOBJSETOPS_VERSION      RT_MAKE_U32_FROM_U8(0xff,0x2f,1,0)


/**
 * The directory operations.
 *
 * @extends RTVFSOBJOPS
 * @extends RTVFSOBJSETOPS
 */
typedef struct RTVFSDIROPS
{
    /** The basic object operation.  */
    RTVFSOBJOPS             Obj;
    /** The structure version (RTVFSDIROPS_VERSION). */
    uint32_t                uVersion;
    /** Reserved field, MBZ. */
    uint32_t                fReserved;
    /** The object setter operations. */
    RTVFSOBJSETOPS          ObjSet;

    /**
     * Opens a directory entry for traversal purposes.
     *
     * Method which sole purpose is helping the path traversal.  Only one of
     * the three output variables will be set, the others will left untouched
     * (caller sets them to NIL).
     *
     * @returns IPRT status code.
     * @retval  VERR_PATH_NOT_FOUND if @a pszEntry was not found.
     * @param   pvThis          The implementation specific directory data.
     * @param   pszEntry        The name of the directory entry to remove.
     * @param   phVfsDir        If not NULL and it is a directory, open it and
     *                          return the handle here.
     * @param   phVfsSymlink    If not NULL and it is a symbolic link, open it
     *                          and return the handle here.
     * @param   phVfsMounted    If not NULL and it is a mounted VFS directory,
     *                          reference it and return the handle here.
     * @todo    Should com dir, symlinks and mount points using some common
     *          ancestor "class".
     */
    DECLCALLBACKMEMBER(int, pfnTraversalOpen)(void *pvThis, const char *pszEntry, PRTVFSDIR phVfsDir,
                                              PRTVFSSYMLINK phVfsSymlink, PRTVFS phVfsMounted);

    /**
     * Open or create a file.
     *
     * @returns IPRT status code.
     * @param   pvThis      The implementation specific directory data.
     * @param   pszFilename The name of the immediate file to open or create.
     * @param   fOpen       The open flags (RTFILE_O_XXX).
     * @param   phVfsFile   Where to return the thandle to the opened file.
     * @sa      RTFileOpen.
     */
    DECLCALLBACKMEMBER(int, pfnOpenFile)(void *pvThis, const char *pszFilename, uint32_t fOpen, PRTVFSFILE phVfsFile);

    /**
     * Open an existing subdirectory.
     *
     * @returns IPRT status code.
     * @param   pvThis      The implementation specific directory data.
     * @param   pszSubDir   The name of the immediate subdirectory to open.
     * @param   phVfsDir    Where to return the handle to the opened directory.
     * @sa      RTDirOpen.
     */
    DECLCALLBACKMEMBER(int, pfnOpenDir)(void *pvThis, const char *pszSubDir, PRTVFSDIR phVfsDir);

    /**
     * Creates a new subdirectory.
     *
     * @returns IPRT status code.
     * @param   pvThis      The implementation specific directory data.
     * @param   pszSubDir   The name of the immediate subdirectory to create.
     * @param   fMode       The mode mask of the new directory.
     * @param   phVfsDir    Where to optionally return the handle to the newly
     *                      create directory.
     * @sa      RTDirCreate.
     */
    DECLCALLBACKMEMBER(int, pfnCreateDir)(void *pvThis, const char *pszSubDir, RTFMODE fMode, PRTVFSDIR phVfsDir);

    /**
     * Opens an existing symbolic link.
     *
     * @returns IPRT status code.
     * @param   pvThis      The implementation specific directory data.
     * @param   pszSymlink  The name of the immediate symbolic link to open.
     * @param   phVfsSymlink    Where to optionally return the handle to the
     *                      newly create symbolic link.
     * @sa      RTSymlinkCreate.
     */
    DECLCALLBACKMEMBER(int, pfnOpenSymlink)(void *pvThis, const char *pszSymlink, PRTVFSSYMLINK phVfsSymlink);

    /**
     * Creates a new symbolic link.
     *
     * @returns IPRT status code.
     * @param   pvThis      The implementation specific directory data.
     * @param   pszSymlink  The name of the immediate symbolic link to create.
     * @param   pszTarget   The symbolic link target.
     * @param   enmType     The symbolic link type.
     * @param   phVfsSymlink    Where to optionally return the handle to the
     *                      newly create symbolic link.
     * @sa      RTSymlinkCreate.
     */
    DECLCALLBACKMEMBER(int, pfnCreateSymlink)(void *pvThis, const char *pszSymlink, const char *pszTarget,
                                              RTSYMLINKTYPE enmType, PRTVFSSYMLINK phVfsSymlink);

    /**
     * Removes a directory entry.
     *
     * @returns IPRT status code.
     * @param   pvThis      The implementation specific directory data.
     * @param   pszEntry    The name of the directory entry to remove.
     * @param   fType       If non-zero, this restricts the type of the entry to
     *                      the object type indicated by the mask
     *                      (RTFS_TYPE_XXX).
     * @sa      RTFileRemove, RTDirRemove, RTSymlinkRemove.
     */
    DECLCALLBACKMEMBER(int, pfnUnlinkEntry)(void *pvThis, const char *pszEntry, RTFMODE fType, PRTVFSDIR phVfsDir);

    /**
     * Rewind the directory stream so that the next read returns the first
     * entry.
     *
     * @returns IPRT status code.
     * @param   pvThis      The implementation specific directory data.
     */
    DECLCALLBACKMEMBER(int, pfnRewindDir)(void *pvThis);

    /**
     * Rewind the directory stream so that the next read returns the first
     * entry.
     *
     * @returns IPRT status code.
     * @param   pvThis      The implementation specific directory data.
     * @param   pDirEntry   Output buffer.
     * @param   pcbDirEntry Complicated, see RTDirReadEx.
     * @param   enmAddAttr  Which set of additional attributes to request.
     * @sa      RTDirReadEx
     */
    DECLCALLBACKMEMBER(int, pfnReadDir)(void *pvThis, PRTDIRENTRYEX pDirEntry, size_t *pcbDirEntry, RTFSOBJATTRADD enmAddAttr);


    /** Marks the end of the structure (RTVFSDIROPS_VERSION). */
    uintptr_t               uEndMarker;
} RTVFSDIROPS;
/** Pointer to const directory operations. */
typedef RTVFSDIROPS const *PCRTVFSDIROPS;
/** The RTVFSDIROPS structure version. */
#define RTVFSDIROPS_VERSION         RT_MAKE_U32_FROM_U8(0xff,0x3f,1,0)


/**
 * The symbolic link operations.
 *
 * @extends RTVFSOBJOPS
 * @extends RTVFSOBJSETOPS
 */
typedef struct RTVFSSYMLINKOPS
{
    /** The basic object operation.  */
    RTVFSOBJOPS             Obj;
    /** The structure version (RTVFSSYMLINKOPS_VERSION). */
    uint32_t                uVersion;
    /** Reserved field, MBZ. */
    uint32_t                fReserved;
    /** The object setter operations. */
    RTVFSOBJSETOPS          ObjSet;

    /**
     * Read the symbolic link target.
     *
     * @returns IPRT status code.
     * @param   pvThis      The implementation specific symbolic link data.
     * @param   pszTarget   The target buffer.
     * @param   cbTarget    The size of the target buffer.
     * @sa      RTSymlinkRead
     */
    DECLCALLBACKMEMBER(int, pfnRead)(void *pvThis, char *pszTarget, size_t cbTarget);

    /** Marks the end of the structure (RTVFSSYMLINKOPS_VERSION). */
    uintptr_t               uEndMarker;
} RTVFSSYMLINKOPS;
/** Pointer to const symbolic link operations. */
typedef RTVFSSYMLINKOPS const *PCRTVFSSYMLINKOPS;
/** The RTVFSSYMLINKOPS structure version. */
#define RTVFSSYMLINKOPS_VERSION     RT_MAKE_U32_FROM_U8(0xff,0x4f,1,0)


/**
 * The basis for all I/O objects (files, pipes, sockets, devices, ++).
 *
 * @extends RTVFSOBJOPS
 */
typedef struct RTVFSIOSTREAMOPS
{
    /** The basic object operation.  */
    RTVFSOBJOPS             Obj;
    /** The structure version (RTVFSIOSTREAMOPS_VERSION). */
    uint32_t                uVersion;
    /** Reserved field, MBZ. */
    uint32_t                fReserved;

    /**
     * Reads from the file/stream.
     *
     * @returns IPRT status code.
     * @param   pvThis      The implementation specific file data.
     * @param   off         Where to read at, -1 for the current position.
     * @param   pSgBuf      Gather buffer describing the bytes that are to be
     *                      written.
     * @param   fBlocking   If @c true, the call is blocking, if @c false it
     *                      should not block.
     * @param   pcbRead     Where return the number of bytes actually read.  If
     *                      NULL, try read all and fail if incomplete.
     * @sa      RTFileRead, RTFileReadAt.
     */
    DECLCALLBACKMEMBER(int, pfnRead)(void *pvThis, RTFOFF off, PCRTSGBUF pSgBuf, bool fBlocking, size_t *pcbRead);

    /**
     * Writes to the file/stream.
     *
     * @returns IPRT status code.
     * @param   pvThis      The implementation specific file data.
     * @param   off         Where to start wrinting, -1 for the current
     *                      position.
     * @param   pSgBuf      Gather buffers describing the bytes that are to be
     *                      written.
     * @param   fBlocking   If @c true, the call is blocking, if @c false it
     *                      should not block.
     * @param   pcbWrite    Where to return the number of bytes actually
     *                      written.  If  NULL, try write it all and fail if
     *                      incomplete.
     * @sa      RTFileWrite, RTFileWriteAt.
     */
    DECLCALLBACKMEMBER(int, pfnWrite)(void *pvThis, RTFOFF off, PCRTSGBUF pSgBuf, bool fBlocking, size_t *pcbWritten);

    /**
     * Flushes any pending data writes to the stream.
     *
     * @returns IPRT status code.
     * @param   pvThis      The implementation specific file data.
     * @sa      RTFileFlush.
     */
    DECLCALLBACKMEMBER(int, pfnFlush)(void *pvThis);

    /**
     * Poll for events.
     *
     * @returns IPRT status code.
     * @param   pvThis      The implementation specific file data.
     * @param   fEvents     The events to poll for (RTPOLL_EVT_XXX).
     * @param   cMillies    How long to wait for event to eventuate.
     * @param   fIntr       Whether the wait is interruptible and can return
     *                      VERR_INTERRUPTED (@c true) or if this condition
     *                      should be hidden from the caller (@c false).
     * @param   pfRetEvents Where to return the event mask.
     * @sa      RTPollSetAdd, RTPoll, RTPollNoResume.
     */
    DECLCALLBACKMEMBER(int, pfnPollOne)(void *pvThis, uint32_t fEvents, RTMSINTERVAL cMillies, bool fIntr,
                                        uint32_t *pfRetEvents);

    /**
     * Tells the current file/stream position.
     *
     * @returns IPRT status code.
     * @param   pvThis      The implementation specific file data.
     * @param   poffActual  Where to return the actual offset.
     * @sa      RTFileTell
     */
    DECLCALLBACKMEMBER(int, pfnTell)(void *pvThis, PRTFOFF poffActual);

    /**
     * Skips @a cb ahead in the stream.
     *
     * @returns IPRT status code.
     * @param   pvThis      The implementation specific file data.
     * @param   cb          The number bytes to skip.
     * @remarks This is optional and can be NULL.
     */
    DECLCALLBACKMEMBER(int, pfnSkip)(void *pvThis, RTFOFF cb);

    /**
     * Fills the stream with @a cb zeros.
     *
     * @returns IPRT status code.
     * @param   pvThis      The implementation specific file data.
     * @param   cb          The number of zero bytes to insert.
     * @remarks This is optional and can be NULL.
     */
    DECLCALLBACKMEMBER(int, pfnZeroFill)(void *pvThis, RTFOFF cb);

    /** Marks the end of the structure (RTVFSIOSTREAMOPS_VERSION). */
    uintptr_t               uEndMarker;
} RTVFSIOSTREAMOPS;
/** Pointer to const I/O stream operations. */
typedef RTVFSIOSTREAMOPS const *PCRTVFSIOSTREAMOPS;

/** The RTVFSIOSTREAMOPS structure version. */
#define RTVFSIOSTREAMOPS_VERSION    RT_MAKE_U32_FROM_U8(0xff,0x5f,1,0)


/**
 * The file operations.
 *
 * @extends RTVFSIOSTREAMOPS
 * @extends RTVFSOBJSETOPS
 */
typedef struct RTVFSFILEOPS
{
    /** The I/O stream and basis object operations. */
    RTVFSIOSTREAMOPS        Stream;
    /** The structure version (RTVFSFILEOPS_VERSION). */
    uint32_t                uVersion;
    /** Reserved field, MBZ. */
    uint32_t                fReserved;
    /** The object setter operations. */
    RTVFSOBJSETOPS          ObjSet;

    /**
     * Changes the current file position.
     *
     * @returns IPRT status code.
     * @param   pvThis      The implementation specific file data.
     * @param   offSeek     The offset to seek.
     * @param   uMethod     The seek method, i.e. what the seek is relative to.
     * @param   poffActual  Where to return the actual offset.
     * @sa      RTFileSeek
     */
    DECLCALLBACKMEMBER(int, pfnSeek)(void *pvThis, RTFOFF offSeek, unsigned uMethod, PRTFOFF poffActual);

    /**
     * Get the current file/stream size.
     *
     * @returns IPRT status code.
     * @param   pvThis      The implementation specific file data.
     * @param   pcbFile     Where to store the current file size.
     * @sa      RTFileGetSize
     */
    DECLCALLBACKMEMBER(int, pfnQuerySize)(void *pvThis, uint64_t *pcbFile);

    /** @todo There will be more methods here. */

    /** Marks the end of the structure (RTVFSFILEOPS_VERSION). */
    uintptr_t               uEndMarker;
} RTVFSFILEOPS;
/** Pointer to const file operations. */
typedef RTVFSFILEOPS const *PCRTVFSFILEOPS;

/** The RTVFSFILEOPS structure version. */
#define RTVFSFILEOPS_VERSION        RT_MAKE_U32_FROM_U8(0xff,0x6f,1,0)

/**
 * Creates a new VFS file handle.
 *
 * @returns IPRT status code
 * @param   pFileOps            The file operations.
 * @param   cbInstance          The size of the instance data.
 * @param   fOpen               The open flags.  The minimum is the access mask.
 * @param   hVfs                The VFS handle to associate this file with.
 *                              NIL_VFS is ok.
 * @param   phVfsFile           Where to return the new handle.
 * @param   ppvInstance         Where to return the pointer to the instance data
 *                              (size is @a cbInstance).
 */
RTDECL(int) RTVfsNewFile(PCRTVFSFILEOPS pFileOps, size_t cbInstance, uint32_t fOpen, RTVFS hVfs,
                         PRTVFSFILE phVfsFile, void **ppvInstance);


/** @defgroup grp_rt_vfs_ll_util        VFS Utility APIs
 * @{ */

/**
 * Parsed path.
 */
typedef struct RTVFSPARSEDPATH
{
    /** The length of the path in szCopy. */
    uint16_t        cch;
    /** The number of path components. */
    uint16_t        cComponents;
    /** Set if the path ends with slash, indicating that it's a directory
     * reference and not a file reference.  The slash has been removed from
     * the copy. */
    bool            fDirSlash;
    /** The offset where each path component starts, i.e. the char after the
     * slash.  The array has cComponents + 1 entries, where the final one is
     * cch + 1 so that one can always terminate the current component by
     * szPath[aoffComponent[i] - 1] = '\0'. */
    uint16_t        aoffComponents[RTPATH_MAX / 2 + 1];
    /** A normalized copy of the path.
     * Reserve some extra space so we can be more relaxed about overflow
     * checks and terminator paddings, especially when recursing. */
    char            szPath[RTPATH_MAX];
} RTVFSPARSEDPATH;
/** Pointer to a parsed path. */
typedef RTVFSPARSEDPATH *PRTVFSPARSEDPATH;

/** The max accepted path length.
 * This must be a few chars shorter than RTVFSPARSEDPATH::szPath because we
 * use two terminators and wish be a little bit lazy with checking. */
#define RTVFSPARSEDPATH_MAX     (RTPATH_MAX - 4U)

/**
 * Appends @a pszPath (relative) to the already parsed path @a pPath.
 *
 * @retval  VINF_SUCCESS
 * @retval  VERR_FILENAME_TOO_LONG
 * @retval  VERR_INTERNAL_ERROR_4
 * @param   pPath               The parsed path to append @a pszPath onto.
 *                              This is both input and output.
 * @param   pszPath             The path to append.  This must be relative.
 * @param   piRestartComp       The component to restart parsing at.  This is
 *                              input/output.  The input does not have to be
 *                              within the valid range.  Optional.
 */
RTDECL(int) RTVfsParsePathAppend(PRTVFSPARSEDPATH pPath, const char *pszPath, uint16_t *piRestartComp);

/**
 * Parses a path.
 *
 * @retval  VINF_SUCCESS
 * @retval  VERR_FILENAME_TOO_LONG
 * @param   pPath               Where to store the parsed path.
 * @param   pszPath             The path to parse.  Absolute or relative to @a
 *                              pszCwd.
 * @param   pszCwd              The current working directory.  Must be
 *                              absolute.
 */
RTDECL(int) RTVfsParsePath(PRTVFSPARSEDPATH pPath, const char *pszPath, const char *pszCwd);

/**
 * Same as RTVfsParsePath except that it allocates a temporary buffer.
 *
 * @retval  VINF_SUCCESS
 * @retval  VERR_NO_TMP_MEMORY
 * @retval  VERR_FILENAME_TOO_LONG
 * @param   pszPath             The path to parse.  Absolute or relative to @a
 *                              pszCwd.
 * @param   pszCwd              The current working directory.  Must be
 *                              absolute.
 * @param   ppPath              Where to store the pointer to the allocated
 *                              buffer containing the parsed path.  This must
 *                              be freed by calling RTVfsParsePathFree.  NULL
 *                              will be stored on failured.
 */
RTDECL(int) RTVfsParsePathA(const char *pszPath, const char *pszCwd, PRTVFSPARSEDPATH *ppPath);

/**
 * Frees a buffer returned by RTVfsParsePathA.
 *
 * @param   pPath               The parsed path buffer to free.  NULL is fine.
 */
RTDECL(void) RTVfsParsePathFree(PRTVFSPARSEDPATH pPath);

/** @}  */

/** @} */

RT_C_DECLS_END

#endif /* !___iprt_vfslowlevel_h */




