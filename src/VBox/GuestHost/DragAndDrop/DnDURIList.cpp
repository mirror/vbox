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

DnDURIObject::DnDURIObject(Type type,
                           const RTCString &strSrcPath,
                           const RTCString &strDstPath,
                           uint32_t fMode, uint64_t cbSize)
    : m_Type(type)
    , m_strSrcPath(strSrcPath)
    , m_strDstPath(strDstPath)
    , m_fMode(fMode)
    , m_cbSize(cbSize)
    , m_cbProcessed(0)
{
    RT_ZERO(u);
}

DnDURIObject::~DnDURIObject(void)
{
    closeInternal();
}

void DnDURIObject::closeInternal(void)
{
    if (m_Type == File)
    {
        if (u.m_hFile)
        {
            RTFileClose(u.m_hFile);
            u.m_hFile = NULL;
        }
    }
}

bool DnDURIObject::IsComplete(void) const
{
    bool fComplete = false;

    Assert(m_cbProcessed <= m_cbSize);
    if (m_cbProcessed == m_cbSize)
        fComplete = true;

    switch (m_Type)
    {
        case File:
            if (!fComplete)
                fComplete = !u.m_hFile;
            break;

        case Directory:
            fComplete = true;
            break;

        default:
            break;
    }

    return fComplete;
}

/* static */
/** @todo Put this into an own class like DnDURIPath : public RTCString? */
int DnDURIObject::RebaseURIPath(RTCString &strPath,
                                const RTCString &strBaseOld,
                                const RTCString &strBaseNew)
{
    int rc;
    const char *pszPath = RTUriPath(strPath.c_str());
    if (pszPath)
    {
        const char *pszPathStart = pszPath;
        const char *pszBaseOld = strBaseOld.c_str();
        if (   pszBaseOld
            && RTPathStartsWith(pszPath, pszBaseOld))
        {
            pszPathStart += strlen(pszBaseOld);
        }

        rc = VINF_SUCCESS;

        if (RT_SUCCESS(rc))
        {
            char *pszPathNew = RTPathJoinA(strBaseNew.c_str(), pszPathStart);
            if (pszPathNew)
            {
                char *pszPathURI = RTUriCreate("file" /* pszScheme */, "/" /* pszAuthority */,
                                               pszPathNew /* pszPath */,
                                               NULL /* pszQuery */, NULL /* pszFragment */);
                if (pszPathURI)
                {
#ifdef DEBUG_andy
                    LogFlowFunc(("Rebasing \"%s\" to \"%s\"", strPath.c_str(), pszPathURI));
#endif
                    strPath = RTCString(pszPathURI) + "\r\n";
                    RTStrFree(pszPathURI);

                    rc = VINF_SUCCESS;
                }
                else
                    rc = VERR_INVALID_PARAMETER;

                RTStrFree(pszPathNew);
            }
            else
                rc = VERR_NO_MEMORY;
        }
    }
    else
        rc = VERR_INVALID_PARAMETER;

#ifdef DEBUG_andy
    LogFlowFuncLeaveRC(rc);
#endif
    return rc;
}

int DnDURIObject::Read(void *pvBuf, uint32_t cbToRead, uint32_t *pcbRead)
{
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    AssertReturn(cbToRead, VERR_INVALID_PARAMETER);
    /* pcbRead is optional. */

    int rc;
    switch (m_Type)
    {
        case File:
        {
            if (!u.m_hFile)
            {
                /* Open files on the source with RTFILE_O_DENY_WRITE to prevent races
                 * where the OS writes to the file while the destination side transfers
                 * it over. */
                rc = RTFileOpen(&u.m_hFile, m_strSrcPath.c_str(),
                                RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_WRITE);
            }
            else
                rc = VINF_SUCCESS;

            bool fDone = false;
            if (RT_SUCCESS(rc))
            {
                size_t cbRead;
                rc = RTFileRead(u.m_hFile, pvBuf, cbToRead, &cbRead);
                if (RT_SUCCESS(rc))
                {
                    if (pcbRead)
                        *pcbRead = (uint32_t)cbRead;

                    m_cbProcessed += cbRead;
                    Assert(m_cbProcessed <= m_cbSize);

                    /* End of file reached or error occurred? */
                    if (   m_cbProcessed == m_cbSize
                        || RT_FAILURE(rc))
                    {
                        closeInternal();
                    }
                }
            }

            break;
        }

        case Directory:
        {
            rc = VINF_SUCCESS;
            break;
        }

        default:
            rc = VERR_NOT_IMPLEMENTED;
            break;
    }

    LogFlowFunc(("Returning strSourcePath=%s, rc=%Rrc\n",
                 m_strSrcPath.c_str(), rc));
    return rc;
}

/*** */

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

    m_lstTree.append(DnDURIObject(  RTFS_IS_DIRECTORY(objInfo.Attr.fMode)
                                  ? DnDURIObject::Directory
                                  : DnDURIObject::File,
                                  pcszPath, &pcszPath[cbBaseLen],
                                  objInfo.Attr.fMode, cbSize));
    m_cbTotal += cbSize;
#ifdef DEBUG_andy
    LogFlowFunc(("strSrcPath=%s, strDstPath=%s, fMode=0x%x, cbSize=%RU64, cbTotal=%zu\n",
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

                char *pszRecDir = RTPathJoinA(pcszPath, DirEntry.szName);
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
                char *pszNewFile = RTPathJoinA(pcszPath, DirEntry.szName);
                if (pszNewFile)
                {
                    /* We need the size and the mode of the file. */
                    RTFSOBJINFO objInfo1;
                    rc = RTPathQueryInfo(pszNewFile, &objInfo1, RTFSOBJATTRADD_NOTHING);
                    if (RT_FAILURE(rc))
                        return rc;
                    rc = RTFileQuerySize(pszNewFile, &cbSize);
                    if (rc == VERR_IS_A_DIRECTORY) /* Happens for symlinks. */
                        rc = VINF_SUCCESS;

                    if (RT_FAILURE(rc))
                        break;

                    if (RTFS_IS_FILE(objInfo.Attr.fMode))
                    {
                        m_lstTree.append(DnDURIObject(DnDURIObject::File,
                                                      pszNewFile, &pszNewFile[cbBaseLen],
                                                      objInfo1.Attr.fMode, cbSize));
                        m_cbTotal += cbSize;
                    }
                    else /* Handle symlink directories. */
                        rc = appendPathRecursive(pszNewFile, cbBaseLen, fFlags);
#ifdef DEBUG_andy
                    LogFlowFunc(("strSrcPath=%s, strDstPath=%s, fMode=0x%x, cbSize=%RU64, cbTotal=%zu\n",
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

int DnDURIList::AppendNativePath(const char *pszPath, uint32_t fFlags)
{
    AssertPtrReturn(pszPath, VERR_INVALID_POINTER);

    int rc;
    char *pszPathNative = RTStrDup(pszPath);
    if (pszPathNative)
    {
        RTPathChangeToUnixSlashes(pszPathNative, true /* fForce */);

        char *pszPathURI = RTUriCreate("file" /* pszScheme */, "/" /* pszAuthority */,
                                       pszPathNative, NULL /* pszQuery */, NULL /* pszFragment */);
        if (pszPathURI)
        {
            rc = AppendURIPath(pszPathURI, fFlags);
            RTStrFree(pszPathURI);
        }
        else
            rc = VERR_INVALID_PARAMETER;

        RTStrFree(pszPathNative);
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}

int DnDURIList::AppendNativePathsFromList(const char *pszNativePaths, size_t cbNativePaths,
                                          uint32_t fFlags)
{
    AssertPtrReturn(pszNativePaths, VERR_INVALID_POINTER);
    AssertReturn(cbNativePaths, VERR_INVALID_PARAMETER);

    RTCList<RTCString> lstPaths
        = RTCString(pszNativePaths, cbNativePaths - 1).split("\r\n");
    return AppendNativePathsFromList(lstPaths, fFlags);
}

int DnDURIList::AppendNativePathsFromList(const RTCList<RTCString> &lstNativePaths,
                                          uint32_t fFlags)
{
    int rc = VINF_SUCCESS;

    for (size_t i = 0; i < lstNativePaths.size(); i++)
    {
        const RTCString &strPath = lstNativePaths.at(i);
        rc = AppendNativePath(strPath.c_str(), fFlags);
        if (RT_FAILURE(rc))
            break;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int DnDURIList::AppendURIPath(const char *pszURI, uint32_t fFlags)
{
    AssertPtrReturn(pszURI, VERR_INVALID_POINTER);

#ifdef DEBUG_andy
    LogFlowFunc(("pszPath=%s, fFlags=0x%x\n", pszURI, fFlags));
#endif
    int rc = VINF_SUCCESS;

    /* Query the path component of a file URI. If this hasn't a
     * file scheme NULL is returned. */
    char *pszFilePath = RTUriFilePath(pszURI, URI_FILE_FORMAT_AUTO);
    if (pszFilePath)
    {
        /* Add the path to our internal file list (recursive in
         * the case of a directory). */
        char *pszFileName = RTPathFilename(pszFilePath);
        if (pszFileName)
        {
            Assert(pszFileName >= pszFilePath);
            char *pszRoot = &pszFilePath[pszFileName - pszFilePath];
            m_lstRoot.append(pszRoot);
#ifdef DEBUG_andy
            LogFlowFunc(("pszFilePath=%s, pszFileName=%s, pszRoot=%s\n",
                         pszFilePath, pszFileName, pszRoot));
#endif
            rc = appendPathRecursive(pszFilePath,
                                     pszFileName - pszFilePath,
                                     fFlags);
        }
        else
            rc = VERR_NOT_FOUND;

        RTStrFree(pszFilePath);
    }
    else
        rc = VERR_INVALID_PARAMETER;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int DnDURIList::AppendURIPathsFromList(const char *pszURIPaths, size_t cbURIPaths,
                                       uint32_t fFlags)
{
    AssertPtrReturn(pszURIPaths, VERR_INVALID_POINTER);
    AssertReturn(cbURIPaths, VERR_INVALID_PARAMETER);

    RTCList<RTCString> lstPaths
        = RTCString(pszURIPaths, cbURIPaths - 1).split("\r\n");
    return AppendURIPathsFromList(lstPaths, fFlags);
}

int DnDURIList::AppendURIPathsFromList(const RTCList<RTCString> &lstURI,
                                       uint32_t fFlags)
{
    int rc = VINF_SUCCESS;

    for (size_t i = 0; i < lstURI.size(); i++)
    {
        RTCString strURI = lstURI.at(i);
        rc = AppendURIPath(strURI.c_str(), fFlags);

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

#if 0
int DnDURIList::FromData(const void *pvData, size_t cbData,

                         uint32_t fFlags)
{
    AssertPtrReturn(pvData, VERR_INVALID_POINTER);
    AssertReturn(cbData, VERR_INVALID_PARAMETER);

    RTCList<RTCString> lstURI =
        RTCString(static_cast<const char*>(pvData), cbData - 1).split("\r\n");
    if (lstURI.isEmpty())
        return VINF_SUCCESS;

    int rc = VINF_SUCCESS;

    for (size_t i = 0; i < lstURI.size(); ++i)
    {
        const RTCString &strUri = lstURI.at(i);
        /* Query the path component of a file URI. If this hasn't a
         * file scheme, null is returned. */
        char *pszFilePath = RTUriFilePath(strUri.c_str(), URI_FILE_FORMAT_AUTO);
        if (pszFilePath)
        {
            rc = DnDPathSanitize(pszFilePath, strlen(pszFilePath));
            if (RT_SUCCESS(rc))
            {
                /** @todo Use RTPathJoin? */
                RTCString strFullPath;
                if (strBasePath.isNotEmpty())
                    strFullPath = RTCString().printf("%s%c%s", strBasePath.c_str(),
                                                     RTPATH_SLASH, pszFilePath);
                else
                    strFullPath = pszFilePath;

                char *pszNewUri = RTUriFileCreate(strFullPath.c_str());
                if (pszNewUri)
                {
                    m_lstRoot.append(pszNewUri);
                    RTStrFree(pszNewUri);
                }
            }
        }
        else
            rc = VERR_INVALID_PARAMETER;

        if (RT_FAILURE(rc))
            break;
    }

    return rc;
}
#endif

void DnDURIList::RemoveFirst(void)
{
    DnDURIObject &curPath = m_lstTree.first();

    uint64_t cbSize = curPath.GetSize();
    Assert(m_cbTotal >= cbSize);
    m_cbTotal -= cbSize; /* Adjust total size. */

    m_lstTree.removeFirst();
}

int DnDURIList::RootFromURIData(const void *pvData, size_t cbData,
                                uint32_t fFlags)
{
    AssertPtrReturn(pvData, VERR_INVALID_POINTER);
    AssertReturn(cbData, VERR_INVALID_PARAMETER);

    RTCList<RTCString> lstURI =
        RTCString(static_cast<const char*>(pvData), cbData - 1).split("\r\n");
    if (lstURI.isEmpty())
        return VINF_SUCCESS;

    int rc = VINF_SUCCESS;

    for (size_t i = 0; i < lstURI.size(); ++i)
    {
        /* Query the path component of a file URI. If this hasn't a
         * file scheme, NULL is returned. */
        const char *pszURI = lstURI.at(i).c_str();
        char *pszFilePath = RTUriFilePath(pszURI,
                                          URI_FILE_FORMAT_AUTO);
#ifdef DEBUG_andy
        LogFlowFunc(("pszURI=%s, pszFilePath=%s\n", pszURI, pszFilePath));
#endif
        if (pszFilePath)
        {
            rc = DnDPathSanitize(pszFilePath, strlen(pszFilePath));
            if (RT_SUCCESS(rc))
                m_lstRoot.append(pszFilePath);

            RTStrFree(pszFilePath);
        }
        else
            rc = VERR_INVALID_PARAMETER;

        if (RT_FAILURE(rc))
            break;
    }

    return rc;
}

RTCString DnDURIList::RootToString(const RTCString &strBasePath /* = "" */,
                                   const RTCString &strSeparator /* = "\r\n" */)
{
    RTCString strRet;
    for (size_t i = 0; i < m_lstRoot.size(); i++)
    {
        const char *pszCurRoot = m_lstRoot.at(i).c_str();
        if (strBasePath.isNotEmpty())
        {
            char *pszPath = RTPathJoinA(strBasePath.c_str(), pszCurRoot);
            if (pszPath)
            {
                char *pszPathURI = RTUriFileCreate(pszPath);
                if (pszPathURI)
                {
                    strRet += RTCString(pszPathURI) + strSeparator;
                    RTStrFree(pszPathURI);
                }
                else
                    break;
                RTStrFree(pszPath);
            }
            else
                break;
        }
        else
        {
            char *pszPathURI = RTUriFileCreate(pszCurRoot);
            if (pszPathURI)
            {
                strRet += RTCString(pszPathURI) + strSeparator;
                RTStrFree(pszPathURI);
            }
            else
                break;
        }
    }

    return strRet;
}

