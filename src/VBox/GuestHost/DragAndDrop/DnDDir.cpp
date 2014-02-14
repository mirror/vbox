/* $Id$ */
/** @file
 * DnD: Directory handling.
 */

/*
 * Copyright (C) 2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/******************************************************************************
 *   Header Files                                                             *
 ******************************************************************************/

#include <iprt/assert.h>
#include <iprt/dir.h>
#include <iprt/path.h>
#include <iprt/string.h>

#include <VBox/GuestHost/DragAndDrop.h>

int DnDDirCreateDroppedFilesEx(const char *pszPath,
                               char *pszDropDir, size_t cbDropDir)
{
    AssertPtrReturn(pszPath, VERR_INVALID_POINTER);
    AssertPtrReturn(pszDropDir, VERR_INVALID_POINTER);
    AssertReturn(cbDropDir, VERR_INVALID_PARAMETER);

    if (RTStrPrintf(pszDropDir, cbDropDir, "%s", pszPath) <= 0)
        return VERR_NO_MEMORY;

    /** @todo On Windows we also could use the registry to override
     *        this path, on Posix a dotfile and/or a guest property
     *        can be used. */

    /* Append our base drop directory. */
    int rc = RTPathAppend(pszDropDir, cbDropDir, "VirtualBox Dropped Files");
    if (RT_FAILURE(rc))
        return rc;

    /* Create it when necessary. */
    if (!RTDirExists(pszDropDir))
    {
        rc = RTDirCreateFullPath(pszDropDir, RTFS_UNIX_IRWXU);
        if (RT_FAILURE(rc))
            return rc;
    }

    /* The actually drop directory consist of the current time stamp and a
     * unique number when necessary. */
    char pszTime[64];
    RTTIMESPEC time;
    if (!RTTimeSpecToString(RTTimeNow(&time), pszTime, sizeof(pszTime)))
        return VERR_BUFFER_OVERFLOW;
    rc = DnDPathSanitizeFilename(pszTime, sizeof(pszTime));
    if (RT_FAILURE(rc))
        return rc;

    rc = RTPathAppend(pszDropDir, cbDropDir, pszTime);
    if (RT_FAILURE(rc))
        return rc;

    /* Create it (only accessible by the current user) */
    return RTDirCreateUniqueNumbered(pszDropDir, cbDropDir, RTFS_UNIX_IRWXU, 3, '-');
}

int DnDDirCreateDroppedFiles(char *pszDropDir, size_t cbDropDir)
{
    AssertPtrReturn(pszDropDir, VERR_INVALID_POINTER);
    AssertReturn(cbDropDir, VERR_INVALID_PARAMETER);

    char szTemp[RTPATH_MAX];

    /* Get the user's temp directory. Don't use the user's root directory (or
     * something inside it) because we don't know for how long/if the data will
     * be kept after the guest OS used it. */
    int rc = RTPathTemp(szTemp, sizeof(szTemp));
    if (RT_FAILURE(rc))
        return rc;

    return DnDDirCreateDroppedFilesEx(szTemp, pszDropDir, cbDropDir);
}

