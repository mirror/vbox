/* $Id$ */
/** @file
 * DnD: URI list class.
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

#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/fs.h>
#include <iprt/path.h>
#include <iprt/uri.h>

#ifdef LOG_GROUP
 #undef LOG_GROUP
#endif
#define LOG_GROUP LOG_GROUP_GUEST_DND
#include <VBox/log.h>

#include <VBox/GuestHost/DragAndDrop.h>

DnDURIList::DnDURIList(void)
    : m_cbTotal(0)
{
}

DnDURIList::~DnDURIList(void)
{
}

int DnDURIList::appendPathRecursive(const char *pcszPath, size_t cbBaseLen,
                                    uint32_t fFlags)
{
    AssertPtrReturn(pcszPath, VERR_INVALID_POINTER);
    AssertReturn(cbBaseLen, VERR_INVALID_PARAMETER);

    RTFSOBJINFO objInfo;
    int rc = RTPathQueryInfo(pcszPath, &objInfo, RTFSOBJATTRADD_NOTHING);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * These are the types we currently support. Symlinks are not directly
     * supported. First the guest could be an OS which doesn't support it and
     * second the symlink could point to a file which is out of the base tree.
     * Both things are hard to support. For now we just copy the target file in
     * this case.
     */
    if (!(   RTFS_IS_DIRECTORY(objInfo.Attr.fMode)
          || RTFS_IS_FILE(objInfo.Attr.fMode)
          || RTFS_IS_SYMLINK(objInfo.Attr.fMode)))
        return VINF_SUCCESS;

    uint64_t cbSize = 0;
    rc = RTFileQuerySize(pcszPath, &cbSize);
    if (rc == VERR_IS_A_DIRECTORY)
        rc = VINF_SUCCESS;

    if (RT_FAILURE(rc))
        return rc;

    m_lstTree.append(DnDURIPath(pcszPath, &pcszPath[cbBaseLen],
                                objInfo.Attr.fMode, cbSize));
    m_cbTotal += cbSize;
#ifdef DEBUG_andy
    LogFlowFunc(("strHostPath=%s, strGuestPath=%s, fMode=0x%x, cbSize=%RU64, cbTotal=%zu\n",
                 pcszPath, &pcszPath[cbBaseLen], objInfo.Attr.fMode, cbSize, m_cbTotal));
#endif

    PRTDIR hDir;
    /* We have to try to open even symlinks, cause they could
     * be symlinks to directories. */
    rc = RTDirOpen(&hDir, pcszPath);
    /* The following error happens when this was a symlink
     * to an file or a regular file. */
    if (   rc == VERR_PATH_NOT_FOUND
        || rc == VERR_NOT_A_DIRECTORY)
        return VINF_SUCCESS;
    if (RT_FAILURE(rc))
        return rc;

    while (RT_SUCCESS(rc))
    {
        RTDIRENTRY DirEntry;
        rc = RTDirRead(hDir, &DirEntry, NULL);
        if (RT_FAILURE(rc))
        {
            if (rc == VERR_NO_MORE_FILES)
                rc = VINF_SUCCESS;
            break;
        }
        switch (DirEntry.enmType)
        {
            case RTDIRENTRYTYPE_DIRECTORY:
            {
                /* Skip "." and ".." entries. */
                if (   RTStrCmp(DirEntry.szName, ".")  == 0
                    || RTStrCmp(DirEntry.szName, "..") == 0)
                    break;

                char *pszRecDir = RTStrAPrintf2("%s%c%s", pcszPath,
                                                RTPATH_DELIMITER, DirEntry.szName);
                if (pszRecDir)
                {
                    rc = appendPathRecursive(pszRecDir, cbBaseLen, fFlags);
                    RTStrFree(pszRecDir);
                }
                else
                    rc = VERR_NO_MEMORY;
                break;
            }
            case RTDIRENTRYTYPE_SYMLINK:
            case RTDIRENTRYTYPE_FILE:
            {
                char *pszNewFile = RTStrAPrintf2("%s%c%s",
                                                 pcszPath, RTPATH_DELIMITER, DirEntry.szName);
                if (pszNewFile)
                {
                    /* We need the size and the mode of the file. */
                    RTFSOBJINFO objInfo1;
                    rc = RTPathQueryInfo(pszNewFile, &objInfo1, RTFSOBJATTRADD_NOTHING);
                    if (RT_FAILURE(rc))
                        return rc;
                    rc = RTFileQuerySize(pszNewFile, &cbSize);
                    if (RT_FAILURE(rc))
                        break;

                    m_lstTree.append(DnDURIPath(pszNewFile, &pszNewFile[cbBaseLen],
                                                objInfo1.Attr.fMode, cbSize));
                    m_cbTotal += cbSize;
#ifdef DEBUG_andy
                    LogFlowFunc(("strHostPath=%s, strGuestPath=%s, fMode=0x%x, cbSize=%RU64, cbTotal=%zu\n",
                                 pszNewFile, &pszNewFile[cbBaseLen], objInfo1.Attr.fMode, cbSize, m_cbTotal));
#endif
                    RTStrFree(pszNewFile);
                }
                else
                    rc = VERR_NO_MEMORY;
                break;
            }

            default:
                break;
        }
    }

    RTDirClose(hDir);
    return rc;
}

int DnDURIList::AppendPath(const char *pszPath, uint32_t fFlags)
{
    AssertPtrReturn(pszPath, VERR_INVALID_POINTER);

#ifdef DEBUG_andy
    LogFlowFunc(("pszPath=%s, fFlags=0x%x\n", pszPath, fFlags));
#endif
    int rc = VINF_SUCCESS;

    /* Query the path component of a file URI. If this hasn't a
     * file scheme NULL is returned. */
    char *pszFilePath = RTUriFilePath(pszPath, URI_FILE_FORMAT_AUTO);
    if (pszFilePath)
    {
        /* Add the path to our internal file list (recursive in
         * the case of a directory). */
        char *pszFileName = RTPathFilename(pszFilePath);
        if (pszFileName)
        {
            char *pszNewURI = RTUriFileCreate(pszFileName);
            if (pszNewURI)
            {
                m_lstRoot.append(pszNewURI);
                RTStrFree(pszNewURI);

                rc = appendPathRecursive(pszFilePath,
                                         pszFileName - pszFilePath,
                                         fFlags);
            }
        }

        RTStrFree(pszFilePath);
    }
    else /* Just append the raw data. */
        m_lstRoot.append(pszPath);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int DnDURIList::AppendPathsFromList(const RTCList<RTCString> &lstURI,
                                    uint32_t fFlags)
{
    int rc = VINF_SUCCESS;

    for (size_t i = 0; i < lstURI.size(); i++)
    {
        const RTCString &strURI = lstURI.at(i);
        rc = AppendPath(strURI.c_str(), fFlags);
        if (RT_FAILURE(rc))
            break;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

void DnDURIList::Clear(void)
{
    m_lstRoot.clear();
    m_lstTree.clear();

    m_cbTotal = 0;
}

void DnDURIList::RemoveFirst(void)
{
    const DnDURIPath &curPath = m_lstTree.first();

    Assert(m_cbTotal >= curPath.m_cbSize);
    m_cbTotal -= curPath.m_cbSize; /* Adjust total size. */

    m_lstTree.removeFirst();
}

