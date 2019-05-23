/* $Id$ */
/** @file
 * Shared Clipboard - Cache handling.
 */

/*
 * Copyright (C) 2019 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_SHARED_CLIPBOARD
#include <VBox/GuestHost/SharedClipboard-uri.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/dir.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/path.h>
#include <iprt/string.h>


#include <VBox/log.h>

SharedClipboardCache::SharedClipboardCache(void)
    : m_cRefs(0)
    , m_fOpen(0)
    , m_hDir(NIL_RTDIR)
    , m_uID(NIL_SHAREDCLIPBOARDAREAID)
{
    int rc = initInternal();
    if (RT_FAILURE(rc))
        throw rc;
}

SharedClipboardCache::SharedClipboardCache(const char *pszPath,
                                           SHAREDCLIPBOARDAREAID uID /* = NIL_SHAREDCLIPBOARDAREAID */,
                                           SHAREDCLIPBOARDCACHEFLAGS fFlags /* = SHAREDCLIPBOARDCACHE_FLAGS_NONE */)
    : m_cRefs(0)
    , m_fOpen(0)
    , m_hDir(NIL_RTDIR)
    , m_uID(uID)
{
    int rc = initInternal();
    if (RT_SUCCESS(rc))
        rc = OpenEx(pszPath, fFlags);

    if (RT_FAILURE(rc))
        throw rc;
}

SharedClipboardCache::~SharedClipboardCache(void)
{
    /* Only make sure to not leak any handles and stuff, don't delete any
     * directories / files here. */
    closeInternal();

    int rc = destroyInternal();
    AssertRC(rc);
}

/**
 * Adds a reference to a Shared Clipboard cache.
 *
 * @returns New reference count.
 */
uint16_t SharedClipboardCache::AddRef(void)
{
    return ASMAtomicIncU32(&m_cRefs);
}

/**
 * Removes a reference from a Shared Clipboard cache.
 *
 * @returns New reference count.
 */
uint16_t SharedClipboardCache::Release(void)
{
    Assert(m_cRefs);
    return ASMAtomicDecU32(&m_cRefs);
}

/**
 * Locks a Shared Clipboard cache.
 *
 * @returns VBox status code.
 */
int SharedClipboardCache::Lock(void)
{
    return RTCritSectEnter(&m_CritSect);
}

/**
 * Unlocks a Shared Clipboard cache.
 *
 * @returns VBox status code.
 */
int SharedClipboardCache::Unlock(void)
{
    return RTCritSectLeave(&m_CritSect);
}

int SharedClipboardCache::AddFile(const char *pszFile)
{
    AssertPtrReturn(pszFile, VERR_INVALID_POINTER);

    if (!this->m_lstFiles.contains(pszFile))
        this->m_lstFiles.append(pszFile);
    return VINF_SUCCESS;
}

int SharedClipboardCache::AddDir(const char *pszDir)
{
    AssertPtrReturn(pszDir, VERR_INVALID_POINTER);

    if (!this->m_lstDirs.contains(pszDir))
        this->m_lstDirs.append(pszDir);
    return VINF_SUCCESS;
}

int SharedClipboardCache::initInternal(void)
{
    return RTCritSectInit(&m_CritSect);
}

int SharedClipboardCache::destroyInternal(void)
{
    return RTCritSectDelete(&m_CritSect);
}

int SharedClipboardCache::closeInternal(void)
{
    int rc;
    if (this->m_hDir != NIL_RTDIR)
    {
        rc = RTDirClose(this->m_hDir);
        if (RT_SUCCESS(rc))
            this->m_hDir = NIL_RTDIR;
    }
    else
        rc = VINF_SUCCESS;

    this->m_fOpen = SHAREDCLIPBOARDCACHE_FLAGS_NONE;
    this->m_uID   = NIL_SHAREDCLIPBOARDAREAID;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Construcuts the cache's base path.
 *
 * @returns VBox status code.
 * @param   pszBase             Base path to use for the cache.
 * @param   pszPath             Where to store the constructured cache base path.
 * @param   cbPath              Size (in bytes) of the constructured cache base path.
 */
int SharedClipboardCache::pathConstruct(const char *pszBase, char *pszPath, size_t cbPath)
{
    RTStrPrintf(pszPath, cbPath, "%s", pszBase);

    /** @todo On Windows we also could use the registry to override
     *        this path, on Posix a dotfile and/or a guest property
     *        can be used. */

    /* Append our base cache directory. */
    int rc = RTPathAppend(pszPath, sizeof(pszPath), "VirtualBox Shared Clipboards"); /** @todo Make this tag configurable? */
    if (RT_FAILURE(rc))
        return rc;

    /* Create it when necessary. */
    if (!RTDirExists(pszPath))
    {
        rc = RTDirCreateFullPath(pszPath, RTFS_UNIX_IRWXU);
        if (RT_FAILURE(rc))
            return rc;
    }

    rc = RTPathAppend(pszPath, sizeof(pszPath), "Clipboard");
    return rc;
}

int SharedClipboardCache::Close(void)
{
    return closeInternal();
}

SHAREDCLIPBOARDAREAID SharedClipboardCache::GetAreaID(void) const
{
    return this->m_uID;
}

const char *SharedClipboardCache::GetDirAbs(void) const
{
    return this->m_strPathAbs.c_str();
}

bool SharedClipboardCache::IsOpen(void) const
{
    return (this->m_hDir != NULL);
}

int SharedClipboardCache::OpenEx(const char *pszPath,
                                 SHAREDCLIPBOARDAREAID uID /* = NIL_SHAREDCLIPBOARDAREAID */,
                                 SHAREDCLIPBOARDCACHEFLAGS fFlags /* = SHAREDCLIPBOARDCACHE_FLAGS_NONE */)
{
    AssertPtrReturn(pszPath, VERR_INVALID_POINTER);
    AssertReturn(fFlags == 0, VERR_INVALID_PARAMETER); /* Flags not supported yet. */

    char szCacheDir[RTPATH_MAX];
    int rc = pathConstruct(pszPath, szCacheDir, sizeof(szCacheDir));
    if (RT_SUCCESS(rc))
    {
        /* Create it (only accessible by the current user) */
        rc = RTDirCreateUniqueNumbered(szCacheDir, sizeof(szCacheDir), RTFS_UNIX_IRWXU, 3, '-');
        if (RT_SUCCESS(rc))
        {
            RTDIR hDir;
            rc = RTDirOpen(&hDir, szCacheDir);
            if (RT_SUCCESS(rc))
            {
                this->m_hDir       = hDir;
                this->m_strPathAbs = szCacheDir;
                this->m_fOpen      = fFlags;
                this->m_uID        = uID;
            }
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardCache::OpenTemp(SHAREDCLIPBOARDAREAID uID,
                                   SHAREDCLIPBOARDCACHEFLAGS fFlags /* = SHAREDCLIPBOARDCACHE_FLAGS_NONE */)
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
        rc = OpenEx(szTemp, uID, fFlags);

    return rc;
}

int SharedClipboardCache::Reset(bool fDeleteContent)
{
    int rc = closeInternal();
    if (RT_SUCCESS(rc))
    {
        if (fDeleteContent)
        {
            rc = Rollback();
        }
        else
        {
            this->m_lstDirs.clear();
            this->m_lstFiles.clear();
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardCache::Reopen(void)
{
    if (this->m_strPathAbs.isEmpty())
        return VERR_NOT_FOUND;

    return OpenEx(this->m_strPathAbs.c_str(), this->m_fOpen);
}

int SharedClipboardCache::Rollback(void)
{
    if (this->m_strPathAbs.isEmpty())
        return VINF_SUCCESS;

    int rc = VINF_SUCCESS;

    /* Rollback by removing any stuff created.
     * Note: Only remove empty directories, never ever delete
     *       anything recursive here! Steam (tm) knows best ... :-) */
    int rc2;
    for (size_t i = 0; i < this->m_lstFiles.size(); i++)
    {
        rc2 = RTFileDelete(this->m_lstFiles.at(i).c_str());
        if (RT_SUCCESS(rc2))
            this->m_lstFiles.removeAt(i);
        else if (RT_SUCCESS(rc))
           rc = rc2;
        /* Keep going. */
    }

    for (size_t i = 0; i < this->m_lstDirs.size(); i++)
    {
        rc2 = RTDirRemove(this->m_lstDirs.at(i).c_str());
        if (RT_SUCCESS(rc2))
            this->m_lstDirs.removeAt(i);
        else if (RT_SUCCESS(rc))
            rc = rc2;
        /* Keep going. */
    }

    if (RT_SUCCESS(rc))
    {
        Assert(this->m_lstFiles.isEmpty());
        Assert(this->m_lstDirs.isEmpty());

        rc2 = closeInternal();
        if (RT_SUCCESS(rc2))
        {
            /* Try to remove the empty root dropped files directory as well.
             * Might return VERR_DIR_NOT_EMPTY or similar. */
            rc2 = RTDirRemove(this->m_strPathAbs.c_str());
        }
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

