/* $Id$ */
/** @file
 * Shared Clipboard - Transfers interface implementation for local file systems.
 */

/*
 * Copyright (C) 2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_SHARED_CLIPBOARD
#include <VBox/GuestHost/SharedClipboard-transfers.h>

#include <iprt/dir.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/path.h>
#include <iprt/string.h>


/**
 * Converts Shared Clipboard create flags (see SharedClipboard-transfers.h) into IPRT create flags.
 *
 * @returns IPRT status code.
 * @param       fShClFlags  Shared clipboard create flags.
 * @param[out]  pfOpen      Where to store the RTFILE_O_XXX flags for
 *                          RTFileOpen.
 *
 * @sa Initially taken from vbsfConvertFileOpenFlags().
 */
static int shClConvertFileCreateFlags(uint32_t fShClFlags, uint64_t *pfOpen)
{
    AssertMsgReturnStmt(!(fShClFlags & ~SHCL_OBJ_CF_VALID_MASK), ("%#x4\n", fShClFlags), *pfOpen = 0, VERR_INVALID_FLAGS);

    uint64_t fOpen = 0;

    switch (fShClFlags & SHCL_OBJ_CF_ACCESS_MASK_RW)
    {
        case SHCL_OBJ_CF_ACCESS_NONE:
        {
#ifdef RT_OS_WINDOWS
            if ((fShClFlags & SHCL_OBJ_CF_ACCESS_MASK_ATTR) != SHCL_OBJ_CF_ACCESS_ATTR_NONE)
                fOpen |= RTFILE_O_OPEN | RTFILE_O_ATTR_ONLY;
            else
#endif
                fOpen |= RTFILE_O_OPEN | RTFILE_O_READ;
            LogFlowFunc(("SHCL_OBJ_CF_ACCESS_NONE\n"));
            break;
        }

        case SHCL_OBJ_CF_ACCESS_READ:
        {
            fOpen |= RTFILE_O_OPEN | RTFILE_O_READ;
            LogFlowFunc(("SHCL_OBJ_CF_ACCESS_READ\n"));
            break;
        }

        default:
            AssertFailedReturn(VERR_IPE_NOT_REACHED_DEFAULT_CASE);
    }

    switch (fShClFlags & SHCL_OBJ_CF_ACCESS_MASK_ATTR)
    {
        case SHCL_OBJ_CF_ACCESS_ATTR_NONE:
        {
            fOpen |= RTFILE_O_ACCESS_ATTR_DEFAULT;
            LogFlowFunc(("SHCL_OBJ_CF_ACCESS_ATTR_NONE\n"));
            break;
        }

        case SHCL_OBJ_CF_ACCESS_ATTR_READ:
        {
            fOpen |= RTFILE_O_ACCESS_ATTR_READ;
            LogFlowFunc(("SHCL_OBJ_CF_ACCESS_ATTR_READ\n"));
            break;
        }

        default:
            AssertFailedReturn(VERR_IPE_NOT_REACHED_DEFAULT_CASE);
    }

    /* Sharing mask */
    switch (fShClFlags & SHCL_OBJ_CF_ACCESS_MASK_DENY)
    {
        case SHCL_OBJ_CF_ACCESS_DENYNONE:
            fOpen |= RTFILE_O_DENY_NONE;
            LogFlowFunc(("SHCL_OBJ_CF_ACCESS_DENYNONE\n"));
            break;

        case SHCL_OBJ_CF_ACCESS_DENYWRITE:
            fOpen |= RTFILE_O_DENY_WRITE;
            LogFlowFunc(("SHCL_OBJ_CF_ACCESS_DENYWRITE\n"));
            break;

        default:
            AssertFailedReturn(VERR_IPE_NOT_REACHED_DEFAULT_CASE);
    }

    *pfOpen = fOpen;

    LogFlowFuncLeaveRC(VINF_SUCCESS);
    return VINF_SUCCESS;
}

/**
 * Resolves a relative path of a specific transfer to its absolute path.
 *
 * @returns VBox status code.
 * @param   pTransfer           Clipboard transfer to resolve path for.
 * @param   pszPath             Path to resolve.
 * @param   fFlags              Resolve flags. Currently not used and must be 0.
 * @param   ppszResolved        Where to store the allocated resolved path. Must be free'd by the called using RTStrFree().
 */
static int shClTransferResolvePathAbs(PSHCLTRANSFER pTransfer, const char *pszPath, uint32_t fFlags,
                                      char **ppszResolved)
{
    AssertPtrReturn(pTransfer,   VERR_INVALID_POINTER);
    AssertPtrReturn(pszPath,     VERR_INVALID_POINTER);
    AssertReturn   (fFlags == 0, VERR_INVALID_PARAMETER);

    LogFlowFunc(("pszPathRootAbs=%s, pszPath=%s\n", pTransfer->pszPathRootAbs, pszPath));

    int rc = ShClTransferValidatePath(pszPath, false /* fMustExist */);
    if (RT_SUCCESS(rc))
    {
        char *pszPathAbs = RTPathJoinA(pTransfer->pszPathRootAbs, pszPath);
        if (pszPathAbs)
        {
            char   szResolved[RTPATH_MAX];
            size_t cbResolved = sizeof(szResolved);
            rc = RTPathAbsEx(pTransfer->pszPathRootAbs, pszPathAbs, RTPATH_STR_F_STYLE_HOST, szResolved, &cbResolved);

            RTStrFree(pszPathAbs);

            if (RT_SUCCESS(rc))
            {
                LogFlowFunc(("pszResolved=%s\n", szResolved));

                rc = VERR_PATH_NOT_FOUND; /* Play safe by default. */

                /* Make sure the resolved path is part of the set of root entries. */
                PSHCLLISTROOT pListRoot;
                RTListForEach(&pTransfer->lstRoots, pListRoot, SHCLLISTROOT, Node)
                {
                    if (RTPathStartsWith(szResolved, pListRoot->pszPathAbs))
                    {
                        rc = VINF_SUCCESS;
                        break;
                    }
                }

                if (RT_SUCCESS(rc))
                    *ppszResolved = RTStrDup(szResolved);
            }
        }
        else
            rc = VERR_NO_MEMORY;
    }

    if (RT_FAILURE(rc))
        LogRel(("Shared Clipboard: Resolving absolute path '%s' failed, rc=%Rrc\n", pszPath, rc));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Adds a file to a transfer list header.
 *
 * @returns VBox status code.
 * @param   pHdr                List header to add file to.
 * @param   pszPath             Path of file to add.
 */
static int shclTransferListHdrAddFile(PSHCLLISTHDR pHdr, const char *pszPath)
{
    AssertPtrReturn(pHdr, VERR_INVALID_POINTER);
    AssertPtrReturn(pszPath, VERR_INVALID_POINTER);

    uint64_t cbSize = 0;
    int rc = RTFileQuerySizeByPath(pszPath, &cbSize);
    if (RT_SUCCESS(rc))
    {
        pHdr->cbTotalSize  += cbSize;
        pHdr->cTotalObjects++;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Builds a transfer list header, internal version.
 *
 * @returns VBox status code.
 * @param   pHdr                Where to store the build list header.
 * @param   pcszPathAbs         Absolute path to use for building the transfer list.
 */
static int shclTransferListHdrFromDir(PSHCLLISTHDR pHdr, const char *pcszPathAbs)
{
    AssertPtrReturn(pcszPathAbs, VERR_INVALID_POINTER);

    LogFlowFunc(("pcszPathAbs=%s\n", pcszPathAbs));

    RTFSOBJINFO objInfo;
    int rc = RTPathQueryInfo(pcszPathAbs, &objInfo, RTFSOBJATTRADD_NOTHING);
    if (RT_SUCCESS(rc))
    {
        if (RTFS_IS_DIRECTORY(objInfo.Attr.fMode))
        {
            RTDIR hDir;
            rc = RTDirOpen(&hDir, pcszPathAbs);
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

                            pHdr->cTotalObjects++;
                            break;
                        }
                        case RTFS_TYPE_FILE:
                        {
                            char *pszSrc = RTPathJoinA(pcszPathAbs, pDirEntry->szName);
                            if (pszSrc)
                            {
                                rc = shclTransferListHdrAddFile(pHdr, pszSrc);
                                RTStrFree(pszSrc);
                            }
                            else
                                rc = VERR_NO_MEMORY;
                            break;
                        }
                        case RTFS_TYPE_SYMLINK:
                        {
                            /** @todo Not implemented yet. */
                        }

                        default:
                            break;
                    }

                } while (RT_SUCCESS(rc));

                RTDirReadExAFree(&pDirEntry, &cbDirEntry);
                RTDirClose(hDir);
            }
        }
        else if (RTFS_IS_FILE(objInfo.Attr.fMode))
        {
            rc = shclTransferListHdrAddFile(pHdr, pcszPathAbs);
        }
        else if (RTFS_IS_SYMLINK(objInfo.Attr.fMode))
        {
            /** @todo Not implemented yet. */
        }
        else
            rc = VERR_NOT_SUPPORTED;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Creates a new list handle (local only).
 *
 * @returns New List handle on success, or SHCLLISTHANDLE_INVALID on error.
 * @param   pTransfer           Clipboard transfer to create new list handle for.
 */
DECLINLINE(SHCLLISTHANDLE) shClTransferListHandleNew(PSHCLTRANSFER pTransfer)
{
    return pTransfer->uListHandleNext++; /** @todo Good enough for now. Improve this later. */
}

static DECLCALLBACK(int) vbclTransferIfaceLocalRootsGet(PSHCLTXPROVIDERCTX pCtx, PSHCLROOTLIST *ppRootList)
{
    LogFlowFuncEnter();

    PSHCLROOTLIST pRootList = ShClTransferRootListAlloc();
    if (!pRootList)
        return VERR_NO_MEMORY;

    AssertPtr(pCtx->pTransfer);
    const uint64_t cRoots = (uint32_t)pCtx->pTransfer->cRoots;

    LogFlowFunc(("cRoots=%RU64\n", cRoots));

    int rc = VINF_SUCCESS;

    if (cRoots)
    {
        PSHCLROOTLISTENTRY paRootListEntries
            = (PSHCLROOTLISTENTRY)RTMemAllocZ(cRoots * sizeof(SHCLROOTLISTENTRY));
        if (paRootListEntries)
        {
            for (uint64_t i = 0; i < cRoots; ++i)
            {
                rc = ShClTransferRootsEntry(pCtx->pTransfer, i, &paRootListEntries[i]);
                if (RT_FAILURE(rc))
                    break;
            }

            if (RT_SUCCESS(rc))
                pRootList->paEntries = paRootListEntries;
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else
        rc = VERR_NOT_FOUND;

    if (RT_SUCCESS(rc))
    {
        pRootList->Hdr.cRoots = cRoots;

        *ppRootList = pRootList;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(int) vbclTransferIfaceLocalListOpen(PSHCLTXPROVIDERCTX pCtx, PSHCLLISTOPENPARMS pOpenParms,
                                                        PSHCLLISTHANDLE phList)
{
    LogFlowFunc(("pszPath=%s\n", pOpenParms->pszPath));

    int rc;

    PSHCLTRANSFER const pTransfer = pCtx->pTransfer;
    AssertPtr(pTransfer);

    PSHCLLISTHANDLEINFO pInfo
        = (PSHCLLISTHANDLEINFO)RTMemAllocZ(sizeof(SHCLLISTHANDLEINFO));
    if (pInfo)
    {
        rc = ShClTransferListHandleInfoInit(pInfo);
        if (RT_SUCCESS(rc))
        {
            rc = shClTransferResolvePathAbs(pTransfer, pOpenParms->pszPath, 0 /* fFlags */, &pInfo->pszPathLocalAbs);
            if (RT_SUCCESS(rc))
            {
                RTFSOBJINFO objInfo;
                rc = RTPathQueryInfo(pInfo->pszPathLocalAbs, &objInfo, RTFSOBJATTRADD_NOTHING);
                if (RT_SUCCESS(rc))
                {
                    if (RTFS_IS_DIRECTORY(objInfo.Attr.fMode))
                    {
                        rc = RTDirOpen(&pInfo->u.Local.hDir, pInfo->pszPathLocalAbs);
                        if (RT_SUCCESS(rc))
                        {
                            pInfo->enmType = SHCLOBJTYPE_DIRECTORY;

                            LogRel2(("Shared Clipboard: Opening directory '%s'\n", pInfo->pszPathLocalAbs));
                        }
                        else
                            LogRel(("Shared Clipboard: Opening directory '%s' failed with %Rrc\n", pInfo->pszPathLocalAbs, rc));

                    }
                    else if (RTFS_IS_FILE(objInfo.Attr.fMode))
                    {
                        rc = RTFileOpen(&pInfo->u.Local.hFile, pInfo->pszPathLocalAbs,
                                        RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_WRITE);
                        if (RT_SUCCESS(rc))
                        {
                            pInfo->enmType = SHCLOBJTYPE_FILE;

                            LogRel2(("Shared Clipboard: Opening file '%s'\n", pInfo->pszPathLocalAbs));
                        }
                        else
                            LogRel(("Shared Clipboard: Opening file '%s' failed with %Rrc\n", pInfo->pszPathLocalAbs, rc));
                    }
                    else
                        rc = VERR_NOT_SUPPORTED;

                    if (RT_SUCCESS(rc))
                    {
                        pInfo->hList = shClTransferListHandleNew(pTransfer);

                        RTListAppend(&pTransfer->lstList, &pInfo->Node);
                        pTransfer->cListHandles++;

                        if (phList)
                            *phList = pInfo->hList;

                        LogFlowFunc(("pszPathLocalAbs=%s, hList=%RU64, cListHandles=%RU32\n",
                                     pInfo->pszPathLocalAbs, pInfo->hList, pTransfer->cListHandles));
                    }
                    else
                    {
                        if (RTFS_IS_DIRECTORY(objInfo.Attr.fMode))
                        {
                            if (RTDirIsValid(pInfo->u.Local.hDir))
                                RTDirClose(pInfo->u.Local.hDir);
                        }
                        else if (RTFS_IS_FILE(objInfo.Attr.fMode))
                        {
                            if (RTFileIsValid(pInfo->u.Local.hFile))
                                RTFileClose(pInfo->u.Local.hFile);
                        }
                    }
                }
            }
        }

        if (RT_FAILURE(rc))
        {
            ShClTransferListHandleInfoDestroy(pInfo);

            RTMemFree(pInfo);
            pInfo = NULL;
        }
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(int) vbclTransferIfaceLocalListClose(PSHCLTXPROVIDERCTX pCtx, SHCLLISTHANDLE hList)
{
    LogFlowFuncEnter();

    int rc = VINF_SUCCESS;

    PSHCLTRANSFER const pTransfer = pCtx->pTransfer;
    AssertPtr(pTransfer);

    PSHCLLISTHANDLEINFO pInfo = ShClTransferListGetByHandle(pTransfer, hList);
    if (pInfo)
    {
        switch (pInfo->enmType)
        {
            case SHCLOBJTYPE_DIRECTORY:
            {
                if (RTDirIsValid(pInfo->u.Local.hDir))
                {
                    RTDirClose(pInfo->u.Local.hDir);
                    pInfo->u.Local.hDir = NIL_RTDIR;
                }
                break;
            }

            default:
                rc = VERR_NOT_SUPPORTED;
                break;
        }

        RTListNodeRemove(&pInfo->Node);

        Assert(pTransfer->cListHandles);
        pTransfer->cListHandles--;

        RTMemFree(pInfo);
    }
    else
        rc = VERR_NOT_FOUND;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(int) vbclTransferIfaceLocalListHdrRead(PSHCLTXPROVIDERCTX pCtx,
                                                           SHCLLISTHANDLE hList, PSHCLLISTHDR pListHdr)
{
    LogFlowFuncEnter();

    PSHCLTRANSFER const pTransfer = pCtx->pTransfer;
    AssertPtr(pTransfer);

    int rc;

    PSHCLLISTHANDLEINFO pInfo = ShClTransferListGetByHandle(pTransfer, hList);
    if (pInfo)
    {
        rc = ShClTransferListHdrInit(pListHdr);
        if (RT_SUCCESS(rc))
        {
            switch (pInfo->enmType)
            {
                case SHCLOBJTYPE_DIRECTORY:
                {
                    LogFlowFunc(("DirAbs: %s\n", pInfo->pszPathLocalAbs));

                    rc = shclTransferListHdrFromDir(pListHdr, pInfo->pszPathLocalAbs);
                    break;
                }

                case SHCLOBJTYPE_FILE:
                {
                    LogFlowFunc(("FileAbs: %s\n", pInfo->pszPathLocalAbs));

                    pListHdr->cTotalObjects = 1;

                    RTFSOBJINFO objInfo;
                    rc = RTFileQueryInfo(pInfo->u.Local.hFile, &objInfo, RTFSOBJATTRADD_NOTHING);
                    if (RT_SUCCESS(rc))
                    {
                        pListHdr->cbTotalSize = objInfo.cbObject;
                    }
                    break;
                }

                /* We don't support symlinks (yet). */

                default:
                    rc = VERR_NOT_SUPPORTED;
                    break;
            }
        }

        LogFlowFunc(("cTotalObj=%RU64, cbTotalSize=%RU64\n", pListHdr->cTotalObjects, pListHdr->cbTotalSize));
    }
    else
        rc = VERR_NOT_FOUND;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(int) vbclTransferIfaceLocalListEntryRead(PSHCLTXPROVIDERCTX pCtx,
                                                             SHCLLISTHANDLE hList, PSHCLLISTENTRY pEntry)
{
    LogFlowFuncEnter();

    int rc;

    PSHCLTRANSFER const pTransfer = pCtx->pTransfer;
    AssertPtr(pTransfer);

    PSHCLLISTHANDLEINFO pInfo = ShClTransferListGetByHandle(pTransfer, hList);
    if (pInfo)
    {
        switch (pInfo->enmType)
        {
            case SHCLOBJTYPE_DIRECTORY:
            {
                LogFlowFunc(("\tDirectory: %s\n", pInfo->pszPathLocalAbs));

                for (;;)
                {
                    bool fSkipEntry = false; /* Whether to skip an entry in the enumeration. */

                    size_t        cbDirEntry = 0;
                    PRTDIRENTRYEX pDirEntry  = NULL;
                    rc = RTDirReadExA(pInfo->u.Local.hDir, &pDirEntry, &cbDirEntry, RTFSOBJATTRADD_NOTHING, RTPATH_F_ON_LINK);
                    if (RT_SUCCESS(rc))
                    {
                        switch (pDirEntry->Info.Attr.fMode & RTFS_TYPE_MASK)
                        {
                            case RTFS_TYPE_DIRECTORY:
                            {
                                /* Skip "." and ".." entries. */
                                if (RTDirEntryExIsStdDotLink(pDirEntry))
                                {
                                    fSkipEntry = true;
                                    break;
                                }

                                LogFlowFunc(("Directory: %s\n", pDirEntry->szName));
                                break;
                            }

                            case RTFS_TYPE_FILE:
                            {
                                LogFlowFunc(("File: %s\n", pDirEntry->szName));
                                break;
                            }

                            case RTFS_TYPE_SYMLINK:
                            {
                                rc = VERR_NOT_IMPLEMENTED; /** @todo Not implemented yet. */
                                break;
                            }

                            default:
                                break;
                        }

                        if (   RT_SUCCESS(rc)
                            && !fSkipEntry)
                        {
                            rc = RTStrCopy(pEntry->pszName, pEntry->cbName, pDirEntry->szName);
                            if (RT_SUCCESS(rc))
                            {
                                pEntry->cbName = (uint32_t)strlen(pEntry->pszName) + 1; /* Include termination. */

                                AssertPtr(pEntry->pvInfo);
                                Assert   (pEntry->cbInfo == sizeof(SHCLFSOBJINFO));

                                ShClFsObjFromIPRT(PSHCLFSOBJINFO(pEntry->pvInfo), &pDirEntry->Info);

                                LogFlowFunc(("Entry pszName=%s, pvInfo=%p, cbInfo=%RU32\n",
                                             pEntry->pszName, pEntry->pvInfo, pEntry->cbInfo));
                            }
                        }

                        RTDirReadExAFree(&pDirEntry, &cbDirEntry);
                    }

                    if (   !fSkipEntry /* Do we have a valid entry? Bail out. */
                        || RT_FAILURE(rc))
                    {
                        break;
                    }
                }

                break;
            }

            case SHCLOBJTYPE_FILE:
            {
                LogFlowFunc(("\tSingle file: %s\n", pInfo->pszPathLocalAbs));

                RTFSOBJINFO objInfo;
                rc = RTFileQueryInfo(pInfo->u.Local.hFile, &objInfo, RTFSOBJATTRADD_NOTHING);
                if (RT_SUCCESS(rc))
                {
                    pEntry->pvInfo = (PSHCLFSOBJINFO)RTMemAlloc(sizeof(SHCLFSOBJINFO));
                    if (pEntry->pvInfo)
                    {
                        rc = RTStrCopy(pEntry->pszName, pEntry->cbName, pInfo->pszPathLocalAbs);
                        if (RT_SUCCESS(rc))
                        {
                            ShClFsObjFromIPRT(PSHCLFSOBJINFO(pEntry->pvInfo), &objInfo);

                            pEntry->cbInfo = sizeof(SHCLFSOBJINFO);
                            pEntry->fInfo  = VBOX_SHCL_INFO_FLAG_FSOBJINFO;
                        }
                    }
                    else
                        rc = VERR_NO_MEMORY;
                }

                break;
            }

            default:
                rc = VERR_NOT_SUPPORTED;
                break;
        }
    }
    else
        rc = VERR_NOT_FOUND;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(int) vbclTransferIfaceLocalObjOpen(PSHCLTXPROVIDERCTX pCtx,
                                                       PSHCLOBJOPENCREATEPARMS pCreateParms, PSHCLOBJHANDLE phObj)
{
    LogFlowFuncEnter();

    int rc;

    PSHCLTRANSFER const pTransfer = pCtx->pTransfer;
    AssertPtr(pTransfer);

    PSHCLOBJHANDLEINFO pInfo = (PSHCLOBJHANDLEINFO)RTMemAllocZ(sizeof(SHCLOBJHANDLEINFO));
    if (pInfo)
    {
        rc = ShClTransferObjHandleInfoInit(pInfo);
        if (RT_SUCCESS(rc))
        {
            uint64_t fOpen;
            rc = shClConvertFileCreateFlags(pCreateParms->fCreate, &fOpen);
            if (RT_SUCCESS(rc))
            {
                rc = shClTransferResolvePathAbs(pTransfer, pCreateParms->pszPath, 0 /* fFlags */,
                                                &pInfo->pszPathLocalAbs);
                if (RT_SUCCESS(rc))
                {
                    rc = RTFileOpen(&pInfo->u.Local.hFile, pInfo->pszPathLocalAbs, fOpen);
                    if (RT_SUCCESS(rc))
                        LogRel2(("Shared Clipboard: Opened file '%s'\n", pInfo->pszPathLocalAbs));
                    else
                        LogRel(("Shared Clipboard: Error opening file '%s': rc=%Rrc\n", pInfo->pszPathLocalAbs, rc));
                }
            }
        }

        if (RT_SUCCESS(rc))
        {
            pInfo->hObj    = pTransfer->uObjHandleNext++;
            pInfo->enmType = SHCLOBJTYPE_FILE;

            RTListAppend(&pTransfer->lstObj, &pInfo->Node);
            pTransfer->cObjHandles++;

            LogFlowFunc(("cObjHandles=%RU32\n", pTransfer->cObjHandles));

            *phObj = pInfo->hObj;
        }
        else
        {
            ShClTransferObjHandleInfoDestroy(pInfo);
            RTMemFree(pInfo);
        }
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(int) vbclTransferIfaceLocalObjClose(PSHCLTXPROVIDERCTX pCtx, SHCLOBJHANDLE hObj)
{
    LogFlowFuncEnter();

    int rc;

    PSHCLTRANSFER const pTransfer = pCtx->pTransfer;
    AssertPtr(pTransfer);

    PSHCLOBJHANDLEINFO pInfo = ShClTransferObjGet(pTransfer, hObj);
    if (pInfo)
    {
        switch (pInfo->enmType)
        {
            case SHCLOBJTYPE_DIRECTORY:
            {
                rc = RTDirClose(pInfo->u.Local.hDir);
                if (RT_SUCCESS(rc))
                {
                    pInfo->u.Local.hDir = NIL_RTDIR;

                    LogRel2(("Shared Clipboard: Closed directory '%s'\n", pInfo->pszPathLocalAbs));
                }
                else
                    LogRel(("Shared Clipboard: Closing directory '%s' failed with %Rrc\n", pInfo->pszPathLocalAbs, rc));
                break;
            }

            case SHCLOBJTYPE_FILE:
            {
                rc = RTFileClose(pInfo->u.Local.hFile);
                if (RT_SUCCESS(rc))
                {
                    pInfo->u.Local.hFile = NIL_RTFILE;

                    LogRel2(("Shared Clipboard: Closed file '%s'\n", pInfo->pszPathLocalAbs));
                }
                else
                    LogRel(("Shared Clipboard: Closing file '%s' failed with %Rrc\n", pInfo->pszPathLocalAbs, rc));
                break;
            }

            default:
                rc = VERR_NOT_IMPLEMENTED;
                break;
        }

        RTListNodeRemove(&pInfo->Node);

        Assert(pTransfer->cObjHandles);
        pTransfer->cObjHandles--;

        ShClTransferObjHandleInfoDestroy(pInfo);

        RTMemFree(pInfo);
        pInfo = NULL;
    }
    else
        rc = VERR_NOT_FOUND;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(int) vbclTransferIfaceLocalObjRead(PSHCLTXPROVIDERCTX pCtx,
                                                       SHCLOBJHANDLE hObj, void *pvData, uint32_t cbData,
                                                       uint32_t fFlags, uint32_t *pcbRead)
{
    RT_NOREF(fFlags);

    LogFlowFuncEnter();

    int rc;

    PSHCLTRANSFER const pTransfer = pCtx->pTransfer;
    AssertPtr(pTransfer);

    PSHCLOBJHANDLEINFO pInfo = ShClTransferObjGet(pTransfer, hObj);
    if (pInfo)
    {
        switch (pInfo->enmType)
        {
            case SHCLOBJTYPE_FILE:
            {
                size_t cbRead;
                rc = RTFileRead(pInfo->u.Local.hFile, pvData, cbData, &cbRead);
                if (RT_SUCCESS(rc))
                {
                    if (pcbRead)
                        *pcbRead = (uint32_t)cbRead;
                }
                break;
            }

            default:
                rc = VERR_NOT_SUPPORTED;
                break;
        }
    }
    else
        rc = VERR_NOT_FOUND;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static DECLCALLBACK(int) vbclTransferIfaceLocalObjWrite(PSHCLTXPROVIDERCTX pCtx,
                                                        SHCLOBJHANDLE hObj, void *pvData, uint32_t cbData, uint32_t fFlags,
                                                        uint32_t *pcbWritten)
{
    RT_NOREF(fFlags);

    LogFlowFuncEnter();

    int rc;

    PSHCLTRANSFER const pTransfer = pCtx->pTransfer;
    AssertPtr(pTransfer);

    PSHCLOBJHANDLEINFO pInfo = ShClTransferObjGet(pTransfer, hObj);
    if (pInfo)
    {
        switch (pInfo->enmType)
        {
            case SHCLOBJTYPE_FILE:
            {
                rc = RTFileWrite(pInfo->u.Local.hFile, pvData, cbData, (size_t *)pcbWritten);
                break;
            }

            default:
                rc = VERR_NOT_SUPPORTED;
                break;
        }
    }
    else
        rc = VERR_NOT_FOUND;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Queries (assigns) the local provider to an interface.
 *
 * The local provider is being used for accessing files on local file systems.
 *
 * @returns Interface pointer the provider get assigned to.
 * @param   pIface              Interface to assign provider to.
 */
PSHCLTXPROVIDERIFACE VBClTransferQueryIfaceLocal(PSHCLTXPROVIDERIFACE pIface)
{
    pIface->pfnRootsGet       = vbclTransferIfaceLocalRootsGet;
    pIface->pfnListOpen       = vbclTransferIfaceLocalListOpen;
    pIface->pfnListClose      = vbclTransferIfaceLocalListClose;
    pIface->pfnListHdrRead    = vbclTransferIfaceLocalListHdrRead;
    pIface->pfnListEntryRead  = vbclTransferIfaceLocalListEntryRead;
    pIface->pfnObjOpen        = vbclTransferIfaceLocalObjOpen;
    pIface->pfnObjClose       = vbclTransferIfaceLocalObjClose;
    pIface->pfnObjRead        = vbclTransferIfaceLocalObjRead;
    pIface->pfnObjWrite       = vbclTransferIfaceLocalObjWrite;

    return pIface;
}

