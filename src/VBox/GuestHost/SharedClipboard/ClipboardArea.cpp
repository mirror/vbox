/* $Id$ */
/** @file
 * Shared Clipboard - Area handling.
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

SharedClipboardArea::SharedClipboardArea(void)
    : m_cRefs(0)
    , m_fOpen(0)
    , m_hDir(NIL_RTDIR)
    , m_uID(NIL_SHAREDCLIPBOARDAREAID)
{
    int rc = initInternal();
    if (RT_FAILURE(rc))
        throw rc;
}

SharedClipboardArea::SharedClipboardArea(const char *pszPath,
                                         SHAREDCLIPBOARDAREAID uID /* = NIL_SHAREDCLIPBOARDAREAID */,
                                         SHAREDCLIPBOARDAREAOPENFLAGS fFlags /* = SHAREDCLIPBOARDAREA_OPEN_FLAGS_NONE */)
    : m_tsCreatedMs(0)
    , m_cRefs(0)
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

SharedClipboardArea::~SharedClipboardArea(void)
{
    /* Only make sure to not leak any handles and stuff, don't delete any
     * directories / files here. */
    closeInternal();

    int rc = destroyInternal();
    AssertRC(rc);
}

/**
 * Adds a reference to a Shared Clipboard area.
 *
 * @returns New reference count.
 */
uint32_t SharedClipboardArea::AddRef(void)
{
    return ++m_cRefs;
}

/**
 * Removes a reference from a Shared Clipboard area.
 *
 * @returns New reference count.
 */
uint32_t SharedClipboardArea::Release(void)
{
    if (m_cRefs)
        m_cRefs--;

    return m_cRefs;
}

/**
 * Locks a Shared Clipboard area.
 *
 * @returns VBox status code.
 */
int SharedClipboardArea::Lock(void)
{
    return RTCritSectEnter(&m_CritSect);
}

/**
 * Unlocks a Shared Clipboard area.
 *
 * @returns VBox status code.
 */
int SharedClipboardArea::Unlock(void)
{
    return RTCritSectLeave(&m_CritSect);
}

int SharedClipboardArea::AddFile(const char *pszFile)
{
    AssertPtrReturn(pszFile, VERR_INVALID_POINTER);

    if (!this->m_lstFiles.contains(pszFile))
        this->m_lstFiles.append(pszFile);
    return VINF_SUCCESS;
}

int SharedClipboardArea::AddDir(const char *pszDir)
{
    AssertPtrReturn(pszDir, VERR_INVALID_POINTER);

    if (!this->m_lstDirs.contains(pszDir))
        this->m_lstDirs.append(pszDir);
    return VINF_SUCCESS;
}

int SharedClipboardArea::initInternal(void)
{
    return RTCritSectInit(&m_CritSect);
}

int SharedClipboardArea::destroyInternal(void)
{
    return RTCritSectDelete(&m_CritSect);
}

int SharedClipboardArea::closeInternal(void)
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

    if (   RT_SUCCESS(rc)
        && m_strPathAbs.isNotEmpty())
        rc = RTDirRemoveRecursive(m_strPathAbs.c_str(), RTDIRRMREC_F_CONTENT_AND_DIR);

    if (RT_SUCCESS(rc))
    {
        this->m_fOpen = SHAREDCLIPBOARDAREA_OPEN_FLAGS_NONE;
        this->m_uID   = NIL_SHAREDCLIPBOARDAREAID;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Construcuts an area's base path.
 * Note: This does *not* create any directories or whatsoever!
 *
 * @returns VBox status code.
 * @param   pszBase             Base path to use for the area.
 * @param   uID                 Area ID to use for the path.
 * @param   pszPath             Where to store the constructured area base path.
 * @param   cbPath              Size (in bytes) of the constructured area base path.
 */
/* static */
int SharedClipboardArea::PathConstruct(const char *pszBase, SHAREDCLIPBOARDAREAID uID, char *pszPath, size_t cbPath)
{
    LogFlowFunc(("pszBase=%s, uAreaID=%RU32\n", pszBase, uID));

    int rc = RTStrCopy(pszPath, cbPath, pszBase);
    if (RT_SUCCESS(rc))
    {
        /** @todo On Windows we also could use the registry to override
         *        this path, on Posix a dotfile and/or a guest property
         *        can be used. */

        /* Append our base area directory. */
        rc = RTPathAppend(pszPath, cbPath, "VirtualBox Shared Clipboards"); /** @todo Make this tag configurable? */
        if (RT_SUCCESS(rc))
        {
            rc = RTPathAppend(pszPath, cbPath, "Clipboard-");
            if (RT_SUCCESS(rc))
            {
                char szID[16];
                ssize_t cchID = RTStrFormatU32(szID, sizeof(szID), uID, 10, 0, 0, 0);
                if (cchID)
                {
                    rc = RTStrCat(pszPath, cbPath, szID);
                }
                else
                    rc = VERR_INVALID_PARAMETER;
            }
        }
    }

    LogFlowFunc(("rc=%Rrc, szPath=%s\n", rc, pszPath));
    return rc;
}

int SharedClipboardArea::Close(void)
{
    return closeInternal();
}

SHAREDCLIPBOARDAREAID SharedClipboardArea::GetID(void) const
{
    return this->m_uID;
}

const char *SharedClipboardArea::GetDirAbs(void) const
{
    return this->m_strPathAbs.c_str();
}

uint32_t SharedClipboardArea::GetRefCount(void)
{
    return ASMAtomicReadU32(&m_cRefs);
}

bool SharedClipboardArea::IsOpen(void) const
{
    return (this->m_hDir != NULL);
}

int SharedClipboardArea::OpenEx(const char *pszPath,
                                SHAREDCLIPBOARDAREAID uID /* = NIL_SHAREDCLIPBOARDAREAID */,
                                SHAREDCLIPBOARDAREAOPENFLAGS fFlags /* = SHAREDCLIPBOARDAREA_OPEN_FLAGS_NONE */)
{
    AssertPtrReturn(pszPath, VERR_INVALID_POINTER);
    AssertReturn(!(fFlags & ~SHAREDCLIPBOARDAREA_OPEN_FLAGS_VALID_MASK), VERR_INVALID_FLAGS);

    char szAreaDir[RTPATH_MAX];
    int rc = SharedClipboardArea::PathConstruct(pszPath, uID, szAreaDir, sizeof(szAreaDir));
    if (RT_SUCCESS(rc))
    {
        if (   RTDirExists(szAreaDir)
            && (fFlags & SHAREDCLIPBOARDAREA_OPEN_FLAGS_MUST_NOT_EXIST))
        {
            rc = VERR_ALREADY_EXISTS;
        }
        else
            rc = RTDirCreateFullPath(szAreaDir, RTFS_UNIX_IRWXU); /** @todo Tweak path mode? */

        if (RT_SUCCESS(rc))
        {
            RTDIR hDir;
            rc = RTDirOpen(&hDir, szAreaDir);
            if (RT_SUCCESS(rc))
            {
                this->m_hDir        = hDir;
                this->m_strPathAbs  = szAreaDir;
                this->m_fOpen       = fFlags;
                this->m_uID         = uID;
                this->m_tsCreatedMs = RTTimeMilliTS();
            }
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardArea::OpenTemp(SHAREDCLIPBOARDAREAID uID,
                                  SHAREDCLIPBOARDAREAOPENFLAGS fFlags /* = SHAREDCLIPBOARDAREA_OPEN_FLAGS_NONE */)
{
    AssertReturn(!(fFlags & ~SHAREDCLIPBOARDAREA_OPEN_FLAGS_VALID_MASK), VERR_INVALID_FLAGS);

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

int SharedClipboardArea::Reset(bool fDeleteContent)
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

int SharedClipboardArea::Reopen(void)
{
    if (this->m_strPathAbs.isEmpty())
        return VERR_NOT_FOUND;

    return OpenEx(this->m_strPathAbs.c_str(), this->m_fOpen);
}

int SharedClipboardArea::Rollback(void)
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
        if (   RT_SUCCESS(rc2)
            && m_strPathAbs.isNotEmpty())
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

