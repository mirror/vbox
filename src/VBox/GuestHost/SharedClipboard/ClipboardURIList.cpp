/* $Id$ */
/** @file
 * Shared Clipboard - URI list class.
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

#include <iprt/dir.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/fs.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/symlink.h>
#include <iprt/uri.h>

#include <VBox/log.h>


SharedClipboardURIList::SharedClipboardURIList(void)
    : m_cTotal(0)
    , m_cbTotal(0)
{
}

SharedClipboardURIList::~SharedClipboardURIList(void)
{
    Clear();
}

int SharedClipboardURIList::appendEntry(const char *pcszSource, const char *pcszTarget, SHAREDCLIPBOARDURILISTFLAGS fFlags)
{
    AssertPtrReturn(pcszSource, VERR_INVALID_POINTER);
    AssertPtrReturn(pcszTarget, VERR_INVALID_POINTER);
    AssertReturn(!(fFlags & ~SHAREDCLIPBOARDURILIST_FLAGS_VALID_MASK), VERR_INVALID_FLAGS);

    LogFlowFunc(("pcszSource=%s, pcszTarget=%s, fFlags=0x%x\n", pcszSource, pcszTarget, fFlags));

    RTFSOBJINFO objInfo;
    int rc = RTPathQueryInfo(pcszSource, &objInfo, RTFSOBJATTRADD_NOTHING);
    if (RT_SUCCESS(rc))
    {
        if (RTFS_IS_FILE(objInfo.Attr.fMode))
        {
            LogFlowFunc(("File '%s' -> '%s' (%RU64 bytes, file mode 0x%x)\n",
                         pcszSource, pcszTarget, (uint64_t)objInfo.cbObject, objInfo.Attr.fMode));

            SharedClipboardURIObject *pObjFile = new SharedClipboardURIObject(SharedClipboardURIObject::Type_File,
                                                                              pcszSource, pcszTarget);
            if (pObjFile)
            {
                /** @todo Add a standard fOpen mode for this list. */
                rc = pObjFile->OpenFile(SharedClipboardURIObject::View_Source,
                                        RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_WRITE);
                if (RT_SUCCESS(rc))
                {
                    rc = appendObject(pObjFile);
                    if (!(fFlags & SHAREDCLIPBOARDURILIST_FLAGS_KEEP_OPEN)) /* Shall we keep the file open while being added to this list? */
                        pObjFile->Close();
                }
                else
                    delete pObjFile;
            }
            else
                rc = VERR_NO_MEMORY;
        }
        else if (RTFS_IS_DIRECTORY(objInfo.Attr.fMode))
        {
            LogFlowFunc(("Directory '%s' -> '%s' (file mode 0x%x)\n", pcszSource, pcszTarget, objInfo.Attr.fMode));

            SharedClipboardURIObject *pObjDir = new SharedClipboardURIObject(SharedClipboardURIObject::Type_Directory,
                                                                             pcszSource, pcszTarget);
            if (pObjDir)
            {
                m_lstTree.append(pObjDir);

                /** @todo Add SHAREDCLIPBOARDURILIST_FLAGS_KEEP_OPEN handling? */
                m_cTotal++;
            }
            else
                rc = VERR_NO_MEMORY;
        }
        /* Note: Symlinks already should have been resolved at this point. */
        else
            rc = VERR_NOT_SUPPORTED;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardURIList::appendPathRecursive(const char *pcszSrcPath,
                                                const char *pcszDstPath, const char *pcszDstBase, size_t cchDstBase,
                                                SHAREDCLIPBOARDURILISTFLAGS fFlags)
{
    AssertPtrReturn(pcszSrcPath, VERR_INVALID_POINTER);
    AssertPtrReturn(pcszDstBase, VERR_INVALID_POINTER);
    AssertPtrReturn(pcszDstPath, VERR_INVALID_POINTER);

    LogFlowFunc(("pcszSrcPath=%s, pcszDstPath=%s, pcszDstBase=%s, cchDstBase=%zu, fFlags=0x%x\n",
                 pcszSrcPath, pcszDstPath, pcszDstBase, cchDstBase, fFlags));

    RTFSOBJINFO objInfo;
    int rc = RTPathQueryInfo(pcszSrcPath, &objInfo, RTFSOBJATTRADD_NOTHING);
    if (RT_SUCCESS(rc))
    {
        if (RTFS_IS_DIRECTORY(objInfo.Attr.fMode))
        {
            rc = appendEntry(pcszSrcPath, &pcszDstPath[cchDstBase], fFlags);
            if (RT_SUCCESS(rc))
            {
                RTDIR hDir;
                rc = RTDirOpen(&hDir, pcszSrcPath);
                if (RT_SUCCESS(rc))
                {
                    size_t        cbDirEntry = 0;
                    PRTDIRENTRYEX pDirEntry  = NULL;
                    do
                    {
                        /* Retrieve the next directory entry. */
                        rc = RTDirReadExA(hDir, &pDirEntry, &cbDirEntry, RTFSOBJATTRADD_NOTHING, RTPATH_F_ON_LINK);
                        if (RT_FAILURE(rc))
                        {
                            if (rc == VERR_NO_MORE_FILES)
                                rc = VINF_SUCCESS;
                            break;
                        }

                        switch (pDirEntry->Info.Attr.fMode & RTFS_TYPE_MASK)
                        {
                            case RTFS_TYPE_DIRECTORY:
                            {
                                /* Skip "." and ".." entries. */
                                if (RTDirEntryExIsStdDotLink(pDirEntry))
                                    break;

                                char *pszSrc = RTPathJoinA(pcszSrcPath, pDirEntry->szName);
                                if (pszSrc)
                                {
                                    char *pszDst = RTPathJoinA(pcszDstPath, pDirEntry->szName);
                                    if (pszDst)
                                    {
                                        rc = appendPathRecursive(pszSrc, pszDst, pcszDstBase, cchDstBase, fFlags);
                                        RTStrFree(pszDst);
                                    }
                                    else
                                        rc = VERR_NO_MEMORY;

                                    RTStrFree(pszSrc);
                                }
                                else
                                    rc = VERR_NO_MEMORY;
                                break;
                            }

                            case RTFS_TYPE_FILE:
                            {
                                char *pszSrc = RTPathJoinA(pcszSrcPath, pDirEntry->szName);
                                if (pszSrc)
                                {
                                    char *pszDst = RTPathJoinA(pcszDstPath, pDirEntry->szName);
                                    if (pszDst)
                                    {
                                        rc = appendEntry(pszSrc, &pszDst[cchDstBase], fFlags);
                                        RTStrFree(pszDst);
                                    }
                                    else
                                        rc = VERR_NO_MEMORY;
                                    RTStrFree(pszSrc);
                                }
                                else
                                    rc = VERR_NO_MEMORY;
                                break;
                            }
                            case RTFS_TYPE_SYMLINK:
                            {
                                if (fFlags & SHAREDCLIPBOARDURILIST_FLAGS_RESOLVE_SYMLINKS)
                                {
                                    char *pszSrc = RTPathRealDup(pcszDstBase);
                                    if (pszSrc)
                                    {
                                        rc = RTPathQueryInfo(pszSrc, &objInfo, RTFSOBJATTRADD_NOTHING);
                                        if (RT_SUCCESS(rc))
                                        {
                                            if (RTFS_IS_DIRECTORY(objInfo.Attr.fMode))
                                            {
                                                LogFlowFunc(("Directory entry is symlink to directory\n"));
                                                rc = appendPathRecursive(pszSrc, pcszDstPath, pcszDstBase, cchDstBase, fFlags);
                                            }
                                            else if (RTFS_IS_FILE(objInfo.Attr.fMode))
                                            {
                                                LogFlowFunc(("Directory entry is symlink to file\n"));
                                                rc = appendEntry(pszSrc, &pcszDstPath[cchDstBase], fFlags);
                                            }
                                            else
                                                rc = VERR_NOT_SUPPORTED;
                                        }

                                        RTStrFree(pszSrc);
                                    }
                                    else
                                        rc = VERR_NO_MEMORY;
                                }
                                break;
                            }

                            default:
                                break;
                        }

                    } while (RT_SUCCESS(rc));

                    RTDirReadExAFree(&pDirEntry, &cbDirEntry);
                    RTDirClose(hDir);
                }
            }
        }
        else if (RTFS_IS_FILE(objInfo.Attr.fMode))
        {
            rc = appendEntry(pcszSrcPath, &pcszDstPath[cchDstBase], fFlags);
        }
        else if (RTFS_IS_SYMLINK(objInfo.Attr.fMode))
        {
            if (fFlags & SHAREDCLIPBOARDURILIST_FLAGS_RESOLVE_SYMLINKS)
            {
                char *pszSrc = RTPathRealDup(pcszSrcPath);
                if (pszSrc)
                {
                    rc = RTPathQueryInfo(pszSrc, &objInfo, RTFSOBJATTRADD_NOTHING);
                    if (RT_SUCCESS(rc))
                    {
                        if (RTFS_IS_DIRECTORY(objInfo.Attr.fMode))
                        {
                            LogFlowFunc(("Symlink to directory\n"));
                            rc = appendPathRecursive(pszSrc, pcszDstPath, pcszDstBase, cchDstBase, fFlags);
                        }
                        else if (RTFS_IS_FILE(objInfo.Attr.fMode))
                        {
                            LogFlowFunc(("Symlink to file\n"));
                            rc = appendEntry(pszSrc, &pcszDstPath[cchDstBase], fFlags);
                        }
                        else
                            rc = VERR_NOT_SUPPORTED;
                    }

                    RTStrFree(pszSrc);
                }
                else
                    rc = VERR_NO_MEMORY;
            }
        }
        else
            rc = VERR_NOT_SUPPORTED;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardURIList::appendObject(SharedClipboardURIObject *pObject)
{
    int rc = VINF_SUCCESS;

    m_lstTree.append(pObject);
    m_lstRoot.append(pObject->GetSourcePathAbs());;

    m_cTotal++;
    m_cbTotal += pObject->GetSize();

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardURIList::AppendNativePath(const char *pszPath, SHAREDCLIPBOARDURILISTFLAGS fFlags)
{
    AssertPtrReturn(pszPath, VERR_INVALID_POINTER);

    char *pszPathURI;
    int rc = SharedClipboardMetaDataConvertToFormat(pszPath, strlen(pszPath), SHAREDCLIPBOARDMETADATAFMT_URI_LIST,
                                                    (void **)&pszPathURI, NULL /* cbData */);
    if (RT_SUCCESS(rc))
    {
        rc = AppendURIPath(pszPathURI, fFlags);
        RTStrFree(pszPathURI);
    }

    return rc;
}

int SharedClipboardURIList::AppendNativePathsFromList(const char *pszNativePaths, size_t cbNativePaths,
                                                      SHAREDCLIPBOARDURILISTFLAGS fFlags)
{
    AssertPtrReturn(pszNativePaths, VERR_INVALID_POINTER);
    AssertReturn(cbNativePaths, VERR_INVALID_PARAMETER);

    RTCList<RTCString> lstPaths
        = RTCString(pszNativePaths, cbNativePaths - 1).split("\r\n");
    return AppendNativePathsFromList(lstPaths, fFlags);
}

int SharedClipboardURIList::AppendNativePathsFromList(const RTCList<RTCString> &lstNativePaths,
                                                      SHAREDCLIPBOARDURILISTFLAGS fFlags)
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

int SharedClipboardURIList::AppendURIObject(SharedClipboardURIObject *pObject)
{
    AssertPtrReturn(pObject, VERR_INVALID_POINTER);

    return appendObject(pObject);
}

int SharedClipboardURIList::AppendURIPath(const char *pszURI, SHAREDCLIPBOARDURILISTFLAGS fFlags)
{
    AssertPtrReturn(pszURI, VERR_INVALID_POINTER);
    AssertReturn(!(fFlags & ~SHAREDCLIPBOARDURILIST_FLAGS_VALID_MASK), VERR_INVALID_FLAGS);
    /** @todo Check for string termination?  */

    RTURIPARSED Parsed;
    int rc = RTUriParse(pszURI, &Parsed);
    if (RT_FAILURE(rc))
        return rc;

    char *pszSrcPath = NULL;

    /* file://host.example.com/path/to/file.txt */
    const char *pszParsedAuthority = RTUriParsedAuthority(pszURI, &Parsed);
    if (   pszParsedAuthority
        && pszParsedAuthority[0] != '\0') /* Authority present? */
    {
        const char *pszParsedPath = RTUriParsedPath(pszURI, &Parsed);
        if (pszParsedPath)
        {
            /* Always use UNIXy paths internally. */
            if (RTStrAPrintf(&pszSrcPath,  "//%s%s", pszParsedAuthority, pszParsedPath) == -1)
                rc = VERR_NO_MEMORY;
        }
        else
            rc = VERR_INVALID_PARAMETER;
    }
    else
    {
        pszSrcPath = RTUriFilePath(pszURI);
        if (!pszSrcPath)
            rc = VERR_INVALID_PARAMETER;
    }

    LogFlowFunc(("pszURI=%s, fFlags=0x%x -> pszParsedAuthority=%s, pszSrcPath=%s, rc=%Rrc\n",
                 pszURI, fFlags, pszParsedAuthority ? pszParsedAuthority : "<None>", pszSrcPath, rc));

    if (RT_SUCCESS(rc))
    {
        /* Add the path to our internal file list (recursive in
         * the case of a directory). */
        size_t cbPathLen = RTPathStripTrailingSlash(pszSrcPath);
        if (cbPathLen)
        {
            char *pszFileName = RTPathFilename(pszSrcPath);
            if (pszFileName)
            {
                Assert(pszFileName >= pszSrcPath);
                size_t cchDstBase = (fFlags & SHAREDCLIPBOARDURILIST_FLAGS_ABSOLUTE_PATHS)
                                  ? 0 /* Use start of path as root. */
                                  : pszFileName - pszSrcPath;
                char *pszDstPath = &pszSrcPath[cchDstBase];

                m_lstRoot.append(pszDstPath);

                LogFlowFunc(("pszSrcPath=%s, pszFileName=%s, pszRoot=%s\n", pszSrcPath, pszFileName, pszDstPath));
                rc = appendPathRecursive(pszSrcPath, pszSrcPath, pszSrcPath, cchDstBase, fFlags);
            }
            else
                rc = VERR_PATH_NOT_FOUND;
        }
        else
            rc = VERR_INVALID_PARAMETER;
    }

    RTStrFree(pszSrcPath);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardURIList::AppendURIPathsFromList(const char *pszURIPaths, size_t cbURIPaths,
                                                   SHAREDCLIPBOARDURILISTFLAGS fFlags)
{
    AssertPtrReturn(pszURIPaths, VERR_INVALID_POINTER);
    AssertReturn(cbURIPaths, VERR_INVALID_PARAMETER);

    RTCList<RTCString> lstPaths
        = RTCString(pszURIPaths, cbURIPaths - 1).split("\r\n");
    return AppendURIPathsFromList(lstPaths, fFlags);
}

int SharedClipboardURIList::AppendURIPathsFromList(const RTCList<RTCString> &lstURI,
                                                   SHAREDCLIPBOARDURILISTFLAGS fFlags)
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

void SharedClipboardURIList::Clear(void)
{
    LogFlowThisFuncEnter();

    m_lstRoot.clear();

    for (size_t i = 0; i < m_lstTree.size(); i++)
    {
        SharedClipboardURIObject *pCurObj = m_lstTree.at(i);
        AssertPtr(pCurObj);
        delete pCurObj;
    }

    m_lstTree.clear();

    m_cTotal  = 0;
    m_cbTotal = 0;

    LogFlowThisFuncLeave();
}

void SharedClipboardURIList::RemoveFirst(void)
{
    if (m_lstTree.isEmpty())
        return;

    SharedClipboardURIObject *pCurObj = m_lstTree.first();
    AssertPtr(pCurObj);

    uint64_t cbSize = pCurObj->GetSize();
    Assert(m_cbTotal >= cbSize);
    m_cbTotal -= cbSize; /* Adjust total size. */

    pCurObj->Close();
    delete pCurObj;

    m_lstTree.removeFirst();
}

int SharedClipboardURIList::SetFromURIData(const void *pvData, size_t cbData, SHAREDCLIPBOARDURILISTFLAGS fFlags)
{
    AssertPtrReturn(pvData, VERR_INVALID_POINTER);
    AssertReturn(cbData, VERR_INVALID_PARAMETER);
    AssertReturn(!(fFlags & ~SHAREDCLIPBOARDURILIST_FLAGS_VALID_MASK), VERR_INVALID_FLAGS);

    if (!RTStrIsValidEncoding(static_cast<const char *>(pvData)))
        return VERR_INVALID_PARAMETER;

    RTCList<RTCString> lstURI =
        RTCString(static_cast<const char *>(pvData), cbData - 1).split("\r\n");
    if (lstURI.isEmpty())
        return VINF_SUCCESS;

    int rc = VINF_SUCCESS;

    for (size_t i = 0; i < lstURI.size(); ++i)
    {
        /* Query the path component of a file URI. If this hasn't a
         * file scheme, NULL is returned. */
        const char *pszURI = lstURI.at(i).c_str();
        char *pszFilePath = RTUriFilePath(pszURI);
#ifdef DEBUG_andy
        LogFlowFunc(("pszURI=%s, pszFilePath=%s\n", pszURI, pszFilePath));
#endif
        if (pszFilePath)
        {
            rc = SharedClipboardPathSanitize(pszFilePath, strlen(pszFilePath));
            if (RT_SUCCESS(rc))
            {
                m_lstRoot.append(pszFilePath);
                m_cTotal++;
            }

            RTStrFree(pszFilePath);
        }
        else
            rc = VERR_INVALID_PARAMETER;

        if (RT_FAILURE(rc))
            break;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

RTCString SharedClipboardURIList::GetRootEntries(const RTCString &strPathBase /* = "" */,
                                                 const RTCString &strSeparator /* = "\r\n" */) const
{
    RTCString strRet;
    for (size_t i = 0; i < m_lstRoot.size(); i++)
    {
        const char *pszCurRoot = m_lstRoot.at(i).c_str();
#ifdef DEBUG_andy
        LogFlowFunc(("pszCurRoot=%s\n", pszCurRoot));
#endif
        if (strPathBase.isNotEmpty())
        {
            char *pszPath = RTPathJoinA(strPathBase.c_str(), pszCurRoot);
            if (pszPath)
            {
                char *pszPathURI = RTUriFileCreate(pszPath);
                if (pszPathURI)
                {
                    strRet += RTCString(pszPathURI) + strSeparator;
                    LogFlowFunc(("URI (Base): %s\n", strRet.c_str()));
                    RTStrFree(pszPathURI);
                }

                RTStrFree(pszPath);

                if (!pszPathURI)
                    break;
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
                LogFlowFunc(("URI: %s\n", strRet.c_str()));
                RTStrFree(pszPathURI);
            }
            else
                break;
        }
    }

    return strRet;
}

