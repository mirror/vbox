/** @file
 * IPRT Filesystem API.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
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

#ifndef ___iprt_filesystem_h
#define ___iprt_filesystem_h

#include <iprt/types.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_filesystem           IPRT Filesystem
 * @{
 */

/** Handle to a filesystem. */
typedef struct RTFILESYSTEMINT         *RTFILESYSTEM;
/** A pointer to a filesystem handle. */
typedef RTFILESYSTEM                   *PRTFILESYSTEM;
/** NIL filesystem handle. */
#define NIL_RTFILESYSTEM               ((RTFILESYSTEM)~0)

/**
 * Callback to read data from the underlying medium of the filesystem.
 *
 * @returns IPRT status code.
 * @param   pvUser    Opaque user data passed on creation.
 * @param   off       Offset to start reading from.
 * @param   pvBuf     Where to store the read data.
 * @param   cbRead    How many bytes to read.
 */
typedef DECLCALLBACK(int) FNRTFILESYSTEMREAD(void *pvUser, uint64_t off, void *pvBuf, size_t cbRead);
/** Pointer to a read callback. */
typedef FNRTFILESYSTEMREAD *PFNRTFILESYSTEMREAD;

/**
 * Callback to write data to the underlying medium of the filesystem.
 *
 * @returns IPRT status code.
 * @param   pvUser    Opaque user data passed on creation.
 * @param   off       Offset to start writing to.
 * @param   pvBuf     The data to write.
 * @param   cbRead    How many bytes to write.
 */
typedef DECLCALLBACK(int) FNRTFILESYSTEMWRITE(void *pvUser, uint64_t off, const void *pvBuf, size_t cbWrite);
/** Pointer to a read callback. */
typedef FNRTFILESYSTEMWRITE *PFNRTFILESYSTEMWRITE;

/**
 * Probes for and opens a filesystem from the given medium.
 *
 * @returns IPRT status code.
 * @retval  VERR_NOT_SUPPORTED if the filesystem on the given medium is unsupported.
 * @param   phFs         Where to store the handle to the filesystem object on success.
 * @param   pfnRead      Read callback for the medium.
 * @param   pfnWrite     Write callback for the medium.
 * @param   cbMedium     Size of the whole medium.
 * @param   cbSector     Size of one sector on the medium.
 * @param   pvUser       Opaque user data used in the read/write callbacks.
 * @param   fFlags       Flags to open the filesystem with.
 */
RTDECL(int) RTFilesystemOpen(PRTFILESYSTEM phFs, PFNRTFILESYSTEMREAD pfnRead,
                             PFNRTFILESYSTEMWRITE pfnWrite, uint64_t cbMedium,
                             uint64_t cbSector, void *pvUser, uint32_t fFlags);

/**
 * Retain a given filesystem object.
 *
 * @returns New reference count on success, UINT32_MAX on failure.
 * @param   hFs          The filesystem object handle.
 */
RTDECL(uint32_t) RTFilesystemRetain(RTFILESYSTEM hFs);

/**
 * Releases a given volume manager.
 *
 * @returns New reference count on success (0 if closed), UINT32_MAX on failure.
 * @param   hFs          The filesystem object handle.
 */
RTDECL(uint32_t) RTFilesystemRelease(RTFILESYSTEM hFs);

/**
 * Returns the format name of the used filesystem.
 *
 * @returns Name of the filesystem format used.
 * @param   hFs          The filesystem object handle.
 */
RTDECL(const char *) RTFilesystemGetFormat(RTFILESYSTEM hFs);

/**
 * Returns the smallest accessible unit of the filesystem.
 *
 * @returns Block size of the given filesystem or 0 on failure.
 * @param   hFs          The filesystem object handle.
 */
RTDECL(uint64_t) RTFilesystemGetBlockSize(RTFILESYSTEM hFs);

/**
 * Queries whether the given range on the medium is used by the filesystem.
 *
 * @returns IPRT status code.
 * @param   hFs          The filesystem object handle.
 * @param   offStart     The start offset on the medium to check for.
 * @param   cb           Size of the range to check for.
 * @param   pfUsed       Where to store whether the range is in use by the filesystem
 *                       on success.
 */
RTDECL(int) RTFilesystemQueryRangeUse(RTFILESYSTEM hFs, uint64_t offStart,
                                      size_t cb, bool *pfUsed);

/** @} */

RT_C_DECLS_END

#endif /* !___iprt_filesystem_h */

