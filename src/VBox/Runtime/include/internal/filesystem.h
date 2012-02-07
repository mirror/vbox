/* $Id$ */
/** @file
 * IPRT - Filesystem implementations.
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

#ifndef ___internal_filesystem_h
#define ___internal_filesystem_h

#include <iprt/types.h>
#include <iprt/err.h>
#include <iprt/assert.h>
#include "internal/magics.h"

RT_C_DECLS_BEGIN

/** A filesystem format handle. */
typedef struct RTFILESYSTEMFMTINT   *RTFILESYSTEMFMT;
/** Pointer to a filesystem format handle. */
typedef RTFILESYSTEMFMT             *PRTFILESYSTEMFMT;
/** NIL filesystem format handle. */
#define NIL_RTFILESYSTEMFMT         ((RTFILESYSTEMFMT)~0)

/** A medium handle. */
typedef struct RTFILESYSTEMMEDIUMINT *RTFILESYSTEMMEDIUM;
/** Pointer to a medium handle. */
typedef RTFILESYSTEMMEDIUM *PRTFILESYSTEMMEDIUM;

/** Score to indicate that the backend can't handle the format at all */
#define RTFILESYSTEM_MATCH_SCORE_UNSUPPORTED 0
/** Score to indicate that a backend supports the format
 * but there can be other backends. */
#define RTFILESYSTEM_MATCH_SCORE_SUPPORTED   (UINT32_MAX/2)
/** Score to indicate a perfect match. */
#define RTFILESYSTEM_MATCH_SCORE_PERFECT     UINT32_MAX

/**
 * Filesystem format operations.
 */
typedef struct RTFILESYSTEMFMTOPS
{
    /** Name of the format. */
    const char *pcszFmt;

    /**
     * Probes the given disk for known structures.
     *
     * @returns IPRT status code.
     * @param   hMedium          Medium handle.
     * @param   puScore          Where to store the match score on success.
     */
    DECLCALLBACKMEMBER(int, pfnProbe)(RTFILESYSTEMMEDIUM hMedium, uint32_t *puScore);

    /**
     * Opens the format to set up all structures.
     *
     * @returns IPRT status code.
     * @param   hMedium          Medium handle.
     * @param   phFileSysFmt     Where to store the filesystem format instance on success.
     */
    DECLCALLBACKMEMBER(int, pfnOpen)(RTFILESYSTEMMEDIUM hMedium, PRTFILESYSTEMFMT phFsFmt);

    /**
     * Closes the filesystem format.
     *
     * @returns IPRT status code.
     * @param   hFsFmt           The format specific filesystem handle.
     */
    DECLCALLBACKMEMBER(int, pfnClose)(RTFILESYSTEMFMT hFsFmt);

    /**
     * Returns block size of the given filesystem.
     *
     * @returns Block size of filesystem.
     * @param   hFsFmt           The format specific filesystem handle.
     */
    DECLCALLBACKMEMBER(uint64_t, pfnGetBlockSize)(RTFILESYSTEMFMT hFsFmt);

    /**
     * Query the use of the given range in the filesystem.
     *
     * @returns IPRT status code.
     * @param   hFsFmt           The format specific filesystem handle.
     * @param   offStart         Start offset to check.
     * @param   cb               Size of the range to check.
     * @param   pfUsed           Where to store whether the range is in use
     *                           by the filesystem.
     */
    DECLCALLBACKMEMBER(int, pfnQueryRangeUse)(RTFILESYSTEMFMT hFsFmt, uint64_t offStart,
                                              size_t cb, bool *pfUsed);

} RTFILESYSTEMFMTOPS;
/** Pointer to a filesystem format ops table. */
typedef RTFILESYSTEMFMTOPS *PRTFILESYSTEMFMTOPS;
/** Pointer to a const filesystem format ops table. */
typedef const RTFILESYSTEMFMTOPS *PCRTFILESYSTEMFMTOPS;

/** Converts a LBA number to the byte offset. */
#define RTFILESYSTEM_LBA2BYTE(lba, disk) ((lba) * (disk)->cbSector)
/** Converts a Byte offset to the LBA number. */
#define RTFILESYSTEM_BYTE2LBA(off, disk) ((off) / (disk)->cbSector)

/**
 * Return size of the medium in bytes.
 *
 * @returns Size of the medium in bytes.
 * @param   hMedium    The medium handle.
 */
DECLHIDDEN(uint64_t) rtFilesystemMediumGetSize(RTFILESYSTEMMEDIUM hMedium);

/**
 * Read from the medium at the given offset.
 *
 * @returns IPRT status code.
 * @param   hMedium  The medium handle to read from.
 * @param   off      Start offset.
 * @param   pvBuf    Destination buffer.
 * @param   cbRead   How much to read.
 */
DECLHIDDEN(int) rtFilesystemMediumRead(RTFILESYSTEMMEDIUM hMedium, uint64_t off,
                                       void *pvBuf, size_t cbRead);

/**
 * Write to the disk at the given offset.
 *
 * @returns IPRT status code.
 * @param   hMedium  The medium handle to write to.
 * @param   off      Start offset.
 * @param   pvBuf    Source buffer.
 * @param   cbWrite  How much to write.
 */
DECLHIDDEN(int) rtFilesystemMediumWrite(RTFILESYSTEMMEDIUM hMedium, uint64_t off,
                                        const void *pvBuf, size_t cbWrite);

RT_C_DECLS_END

#endif /* !__internal_filesystem_h */

