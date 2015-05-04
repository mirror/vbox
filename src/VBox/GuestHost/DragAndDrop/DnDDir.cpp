/* $Id$ */
/** @file
 * DnD: Directory handling.
 */

/*
 * Copyright (C) 2014-2015 Oracle Corporation
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

int DnDDirDroppedAddFile(PDNDDIRDROPPEDFILES pDir, const char *pszFile)
{
    AssertPtrReturn(pDir, VERR_INVALID_POINTER);
    AssertPtrReturn(pszFile, VERR_INVALID_POINTER);

    if (!pDir->lstFiles.contains(pszFile))
        pDir->lstFiles.append(pszFile);
    return VINF_SUCCESS;
}

int DnDDirDroppedAddDir(PDNDDIRDROPPEDFILES pDir, const char *pszDir)
{
    AssertPtrReturn(pDir, VERR_INVALID_POINTER);
    AssertPtrReturn(pszDir, VERR_INVALID_POINTER);

    if (!pDir->lstDirs.contains(pszDir))
        pDir->lstDirs.append(pszDir);
    return VINF_SUCCESS;
}

int DnDDirDroppedFilesCreateAndOpenEx(const char *pszPath, PDNDDIRDROPPEDFILES pDir)
{
    AssertPtrReturn(pszPath, VERR_INVALID_POINTER);
    AssertPtrReturn(pDir, VERR_INVALID_POINTER);

    char pszDropDir[RTPATH_MAX];
    if (RTStrPrintf(pszDropDir, sizeof(pszDropDir), "%s", pszPath) <= 0)
        return VERR_NO_MEMORY;

    /** @todo On Windows we also could use the registry to override
     *        this path, on Posix a dotfile and/or a guest property
     *        can be used. */

    /* Append our base drop directory. */
    int rc = RTPathAppend(pszDropDir, sizeof(pszDropDir), "VirtualBox Dropped Files"); /** @todo Make this tag configurable? */
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

    rc = RTPathAppend(pszDropDir, sizeof(pszDropDir), pszTime);
    if (RT_FAILURE(rc))
        return rc;

    /* Create it (only accessible by the current user) */
    rc = RTDirCreateUniqueNumbered(pszDropDir, sizeof(pszDropDir), RTFS_UNIX_IRWXU, 3, '-');
    if (RT_SUCCESS(rc))
    {
        PRTDIR phDir;
        rc = RTDirOpen(&phDir, pszDropDir);
        if (RT_SUCCESS(rc))
        {
            pDir->hDir       = phDir;
            pDir->strPathAbs = pszDropDir;
        }
    }

    return rc;
}

int DnDDirDroppedFilesCreateAndOpenTemp(PDNDDIRDROPPEDFILES pDir)
{
    AssertPtrReturn(pDir, VERR_INVALID_POINTER);

    char szTemp[RTPATH_MAX];

    /*
     * Get the user's temp directory. Don't use the user's root directory (or
     * something inside it) because we don't know for how long/if the data will
     * be kept after the guest OS used it.
     */
    int rc = RTPathTemp(szTemp, sizeof(szTemp));
    if (RT_FAILURE(rc))
        return rc;

    return DnDDirDroppedFilesCreateAndOpenEx(szTemp, pDir);
}

int DnDDirDroppedFilesClose(PDNDDIRDROPPEDFILES pDir, bool fRemove)
{
    AssertPtrReturn(pDir, VERR_INVALID_POINTER);

    int rc = RTDirClose(pDir->hDir);
    if (RT_SUCCESS(rc))
    {
        pDir->lstDirs.clear();
        pDir->lstFiles.clear();

        if (fRemove)
        {
            /* Try removing the (empty) drop directory in any case. */
            rc = RTDirRemove(pDir->strPathAbs.c_str());
            if (RT_SUCCESS(rc)) /* Only clear if successfully removed. */
                pDir->strPathAbs = "";
        }
    }

    return rc;
}

const char *DnDDirDroppedFilesGetDirAbs(PDNDDIRDROPPEDFILES pDir)
{
    AssertPtrReturn(pDir, NULL);
    return pDir->strPathAbs.c_str();
}

int DnDDirDroppedFilesRollback(PDNDDIRDROPPEDFILES pDir)
{
    AssertPtrReturn(pDir, VERR_INVALID_POINTER);

    if (pDir->strPathAbs.isEmpty())
        return VINF_SUCCESS;

    int rc = VINF_SUCCESS;
    int rc2;

    /* Rollback by removing any stuff created.
     * Note: Only remove empty directories, never ever delete
     *       anything recursive here! Steam (tm) knows best ... :-) */
    for (size_t i = 0; i < pDir->lstFiles.size(); i++)
    {
        rc2 = RTFileDelete(pDir->lstFiles.at(i).c_str());
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    for (size_t i = 0; i < pDir->lstDirs.size(); i++)
    {
        rc2 = RTDirRemove(pDir->lstDirs.at(i).c_str());
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    /* Try to remove the empty root dropped files directory as well. */
    rc2 = RTDirRemove(pDir->strPathAbs.c_str());
    if (RT_SUCCESS(rc))
        rc = rc2;

    return rc;
}

