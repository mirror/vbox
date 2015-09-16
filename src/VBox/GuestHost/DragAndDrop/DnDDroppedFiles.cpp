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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/

#include <iprt/assert.h>
#include <iprt/dir.h>
#include <iprt/path.h>
#include <iprt/string.h>

#include <VBox/GuestHost/DragAndDrop.h>

DnDDroppedFiles::DnDDroppedFiles(void)
    : hDir(NULL)
    , fOpen(false) { }

DnDDroppedFiles::DnDDroppedFiles(const char *pszPath, uint32_t fFlags)
    : hDir(NULL)
    , fOpen(false)
{
    OpenEx(pszPath, fFlags);
}

DnDDroppedFiles::~DnDDroppedFiles(void)
{
    Reset(true /* fRemoveDropDir */);
}

int DnDDroppedFiles::AddFile(const char *pszFile)
{
    AssertPtrReturn(pszFile, VERR_INVALID_POINTER);

    if (!this->lstFiles.contains(pszFile))
        this->lstFiles.append(pszFile);
    return VINF_SUCCESS;
}

int DnDDroppedFiles::AddDir(const char *pszDir)
{
    AssertPtrReturn(pszDir, VERR_INVALID_POINTER);

    if (!this->lstDirs.contains(pszDir))
        this->lstDirs.append(pszDir);
    return VINF_SUCCESS;
}

const char *DnDDroppedFiles::GetDirAbs(void) const
{
    return this->strPathAbs.c_str();
}

bool DnDDroppedFiles::IsOpen(void) const
{
    return this->fOpen;
}

int DnDDroppedFiles::OpenEx(const char *pszPath, uint32_t fFlags)
{
    AssertPtrReturn(pszPath, VERR_INVALID_POINTER);
    AssertReturn(fFlags == 0, VERR_INVALID_PARAMETER); /* Flags not supported yet. */

    int rc;

    do
    {
        char pszDropDir[RTPATH_MAX];
        size_t cchDropDir = RTStrPrintf(pszDropDir, sizeof(pszDropDir), "%s", pszPath);

        /** @todo On Windows we also could use the registry to override
         *        this path, on Posix a dotfile and/or a guest property
         *        can be used. */

        /* Append our base drop directory. */
        rc = RTPathAppend(pszDropDir, sizeof(pszDropDir), "VirtualBox Dropped Files"); /** @todo Make this tag configurable? */
        if (RT_FAILURE(rc))
            break;

        /* Create it when necessary. */
        if (!RTDirExists(pszDropDir))
        {
            rc = RTDirCreateFullPath(pszDropDir, RTFS_UNIX_IRWXU);
            if (RT_FAILURE(rc))
                break;
        }

        /* The actually drop directory consist of the current time stamp and a
         * unique number when necessary. */
        char pszTime[64];
        RTTIMESPEC time;
        if (!RTTimeSpecToString(RTTimeNow(&time), pszTime, sizeof(pszTime)))
        {
            rc = VERR_BUFFER_OVERFLOW;
            break;
        }

        rc = DnDPathSanitizeFilename(pszTime, sizeof(pszTime));
        if (RT_FAILURE(rc))
            break;

        rc = RTPathAppend(pszDropDir, sizeof(pszDropDir), pszTime);
        if (RT_FAILURE(rc))
            break;

        /* Create it (only accessible by the current user) */
        rc = RTDirCreateUniqueNumbered(pszDropDir, sizeof(pszDropDir), RTFS_UNIX_IRWXU, 3, '-');
        if (RT_SUCCESS(rc))
        {
            PRTDIR phDir;
            rc = RTDirOpen(&phDir, pszDropDir);
            if (RT_SUCCESS(rc))
            {
                this->hDir       = phDir;
                this->strPathAbs = pszDropDir;
                this->fOpen      = true;
            }
        }

    } while (0);

    return rc;
}

int DnDDroppedFiles::OpenTemp(uint32_t fFlags)
{
    AssertReturn(fFlags == 0, VERR_INVALID_PARAMETER); /* Flags not supported yet. */

    /*
     * Get the user's temp directory. Don't use the user's root directory (or
     * something inside it) because we don't know for how long/if the data will
     * be kept after the guest OS used it.
     */
    char szTemp[RTPATH_MAX];
    int rc = RTPathTemp(szTemp, sizeof(szTemp));
    if (RT_SUCCESS(rc))
        rc = OpenEx(szTemp, fFlags);

    return rc;
}

int DnDDroppedFiles::Reset(bool fRemoveDropDir)
{
    int rc = VINF_SUCCESS;
    if (this->fOpen)
    {
        rc = RTDirClose(this->hDir);
        if (RT_SUCCESS(rc))
        {
            this->fOpen = false;
            this->hDir  = NULL;
        }
    }
    if (RT_SUCCESS(rc))
    {
        this->lstDirs.clear();
        this->lstFiles.clear();

        if (   fRemoveDropDir
            && this->strPathAbs.isNotEmpty())
        {
            /* Try removing the (empty) drop directory in any case. */
            rc = RTDirRemove(this->strPathAbs.c_str());
            if (RT_SUCCESS(rc)) /* Only clear if successfully removed. */
                this->strPathAbs = "";
        }
    }

    return rc;
}

int DnDDroppedFiles::Rollback(void)
{
    if (this->strPathAbs.isEmpty())
        return VINF_SUCCESS;

    Assert(this->fOpen);
    Assert(this->hDir != NULL);

    int rc = VINF_SUCCESS;
    int rc2;

    /* Rollback by removing any stuff created.
     * Note: Only remove empty directories, never ever delete
     *       anything recursive here! Steam (tm) knows best ... :-) */
    for (size_t i = 0; i < this->lstFiles.size(); i++)
    {
        rc2 = RTFileDelete(this->lstFiles.at(i).c_str());
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    for (size_t i = 0; i < this->lstDirs.size(); i++)
    {
        rc2 = RTDirRemove(this->lstDirs.at(i).c_str());
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    /* Try to remove the empty root dropped files directory as well. */
    rc2 = RTDirRemove(this->strPathAbs.c_str());
    if (RT_SUCCESS(rc))
        rc = rc2;

    return rc;
}

