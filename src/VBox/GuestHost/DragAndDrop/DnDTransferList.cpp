/* $Id$ */
/** @file
 * DnD - transfer list implemenation.
 */

/*
 * Copyright (C) 2014-2020 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_GUEST_DND
#include <VBox/GuestHost/DragAndDrop.h>

#include <iprt/dir.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/fs.h>
#include <iprt/mem.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/symlink.h>
#include <iprt/uri.h>

#include <VBox/log.h>


/*********************************************************************************************************************************
*   Prototypes                                                                                                                   *
*********************************************************************************************************************************/
static int dndTransferListSetRootPath(PDNDTRANSFERLIST pList, const char *pcszRootPathAbs);

static int dndTransferListRootAdd(PDNDTRANSFERLIST pList, const char *pcszRoot);
static void dndTransferListRootFree(PDNDTRANSFERLIST pList, PDNDTRANSFERLISTROOT pRootObj);

static int dndTransferListObjAdd(PDNDTRANSFERLIST pList, const char *pcszSrcAbs, RTFMODE fMode, DNDTRANSFERLISTFLAGS fFlags);
static void dndTransferListObjFree(PDNDTRANSFERLIST pList, PDNDTRANSFEROBJECT pLstObj);


/** The size of the directory entry buffer we're using. */
#define DNDTRANSFERLIST_DIRENTRY_BUF_SIZE (sizeof(RTDIRENTRYEX) + RTPATH_MAX)


/**
 * Initializes a transfer list.
 *
 * @returns VBox status code.
 * @param   pList               Transfer list to initialize.
 * @param   pcszRootPathAbs     Absolute root path to use for this list. Optional and can be NULL.
 */
int DnDTransferListInit(PDNDTRANSFERLIST pList, const char *pcszRootPathAbs)
{
    AssertPtrReturn(pList, VERR_INVALID_POINTER);
    /* pcszRootPathAbs is optional. */

    if (!strlen(pcszRootPathAbs))
        return VERR_INVALID_PARAMETER;

    if (pList->pszPathRootAbs)
        return VERR_WRONG_ORDER;

    pList->pszPathRootAbs = NULL;

    RTListInit(&pList->lstRoot);
    pList->cRoots = 0;

    RTListInit(&pList->lstObj);
    pList->cObj = 0;
    pList->cbObjTotal = 0;

    if (pcszRootPathAbs)
        return dndTransferListSetRootPath(pList, pcszRootPathAbs);

    return VINF_SUCCESS;
}

/**
 * Destroys a transfer list.
 *
 * @param   pList               Transfer list to destroy.
 */
void DnDTransferListDestroy(PDNDTRANSFERLIST pList)
{
    if (!pList)
        return;

    DnDTransferListReset(pList);

    RTStrFree(pList->pszPathRootAbs);
    pList->pszPathRootAbs = NULL;
}

/**
 * Resets a transfer list.
 *
 * Note: Does *not* clear the root path!
 *
 * @param   pList               Transfer list to clear.
 */
void DnDTransferListReset(PDNDTRANSFERLIST pList)
{
    AssertPtrReturnVoid(pList);

    /* Note: This does not clear the root path! */

    if (!pList->pszPathRootAbs)
        return;

    PDNDTRANSFERLISTROOT pRootCur, pRootNext;
    RTListForEachSafe(&pList->lstRoot, pRootCur, pRootNext, DNDTRANSFERLISTROOT, Node)
        dndTransferListRootFree(pList, pRootCur);
    Assert(RTListIsEmpty(&pList->lstRoot));

    PDNDTRANSFEROBJECT pObjCur, pObjNext;
    RTListForEachSafe(&pList->lstObj, pObjCur, pObjNext, DNDTRANSFEROBJECT, Node)
        dndTransferListObjFree(pList, pObjCur);
    Assert(RTListIsEmpty(&pList->lstObj));

    Assert(pList->cRoots == 0);
    Assert(pList->cObj == 0);

    pList->cbObjTotal = 0;
}

/**
 * Adds a single transfer object entry to a transfer List.
 *
 * @returns VBox status code.
 * @param   pList               Transfer list to add entry to.
 * @param   pcszSrcAbs          Absolute source path (local) to use.
 * @param   fMode               File mode of entry to add.
 * @param   fFlags              Transfer list flags to use for appending.
 */
static int dndTransferListObjAdd(PDNDTRANSFERLIST pList, const char *pcszSrcAbs, RTFMODE fMode, DNDTRANSFERLISTFLAGS fFlags)
{
    AssertPtrReturn(pList, VERR_INVALID_POINTER);
    AssertPtrReturn(pcszSrcAbs, VERR_INVALID_POINTER);

    LogFlowFunc(("pcszSrcAbs=%s, fMode=%#x, fFlags=0x%x\n", pcszSrcAbs, fMode, fFlags));

    int rc = VINF_SUCCESS;

    if (   !RTFS_IS_FILE(fMode)
        && !RTFS_IS_DIRECTORY(fMode))
        /** @todo Symlinks not allowed. */
    {
        rc = VERR_NOT_SUPPORTED;
    }

    if (RT_SUCCESS(rc))
    {
        /* Calculate the path to add as the destination path to our URI object. */
        const size_t idxPathToAdd = strlen(pList->pszPathRootAbs);
        AssertReturn(strlen(pcszSrcAbs) > idxPathToAdd, VERR_INVALID_PARAMETER); /* Should never happen (tm). */

        PDNDTRANSFEROBJECT pObj = (PDNDTRANSFEROBJECT)RTMemAllocZ(sizeof(DNDTRANSFEROBJECT));
        if (pObj)
        {
            pObj = (PDNDTRANSFEROBJECT)RTMemAllocZ(sizeof(DNDTRANSFEROBJECT));
            if (pObj)
            {
                const bool fIsFile = RTFS_IS_FILE(fMode);

                rc = DnDTransferObjectInit(pObj, fIsFile ? DNDTRANSFEROBJTYPE_FILE : DNDTRANSFEROBJTYPE_DIRECTORY,
                                           pList->pszPathRootAbs, &pcszSrcAbs[idxPathToAdd]);
                if (RT_SUCCESS(rc))
                {
                    if (fIsFile)
                        rc = DnDTransferObjectOpen(pObj,
                                                   RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_WRITE, /** @todo Add a standard fOpen mode for this list. */
                                                   0 /* fMode */, DNDTRANSFEROBJECT_FLAGS_NONE);
                    if (RT_SUCCESS(rc))
                    {
                        RTListAppend(&pList->lstObj, &pObj->Node);

                        pList->cObj++;
                        if (fIsFile)
                            pList->cbObjTotal += DnDTransferObjectGetSize(pObj);

                        if (   fIsFile
                            && !(fFlags & DNDTRANSFERLIST_FLAGS_KEEP_OPEN)) /* Shall we keep the file open while being added to this list? */
                            DnDTransferObjectClose(pObj);
                    }

                    if (RT_FAILURE(rc))
                        DnDTransferObjectDestroy(pObj);
                }

                if (RT_FAILURE(rc))
                    RTMemFree(pObj);
            }
            else
                rc = VERR_NO_MEMORY;

            if (RT_FAILURE(rc))
                RTMemFree(pObj);
        }
        else
            rc = VERR_NO_MEMORY;
    }

    if (RT_FAILURE(rc))
        LogRel(("DnD: Adding entry '%s' of type %#x failed with rc=%Rrc\n", pcszSrcAbs, fMode & RTFS_TYPE_MASK, rc));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Frees an internal DnD transfer list object.
 *
 * @param   pList               Transfer list to free object for.
 * @param   pLstObj             transfer list object to free. The pointer will be invalid after calling.
 */
static void dndTransferListObjFree(PDNDTRANSFERLIST pList, PDNDTRANSFEROBJECT pObj)
{
    if (!pObj)
        return;

    DnDTransferObjectDestroy(pObj);
    RTListNodeRemove(&pObj->Node);
    RTMemFree(pObj);

    AssertReturnVoid(pList->cObj);
    pList->cObj--;
}

/**
 * Helper routine for handling adding sub directories.
 *
 * @return  IPRT status code.
 * @param   pList           transfer list to add found entries to.
 * @param   pszDir          Pointer to the directory buffer.
 * @param   cchDir          The length of pszDir in pszDir.
 * @param   pDirEntry       Pointer to the directory entry.
 * @param   fFlags          Flags of type DNDTRANSFERLISTFLAGS.
 */
static int dndTransferListAppendPathRecursiveSub(PDNDTRANSFERLIST pList,
                                                 char *pszDir, size_t cchDir, PRTDIRENTRYEX pDirEntry,
                                                 DNDTRANSFERLISTFLAGS fFlags)

{
    Assert(cchDir > 0); Assert(pszDir[cchDir] == '\0');

    /* Make sure we've got some room in the path, to save us extra work further down. */
    if (cchDir + 3 >= RTPATH_MAX)
        return VERR_BUFFER_OVERFLOW;

    /* Open directory. */
    RTDIR hDir;
    int rc = RTDirOpen(&hDir, pszDir);
    if (RT_FAILURE(rc))
        return rc;

    /* Ensure we've got a trailing slash (there is space for it see above). */
    if (!RTPATH_IS_SEP(pszDir[cchDir - 1]))
    {
        pszDir[cchDir++] = RTPATH_SLASH;
        pszDir[cchDir]   = '\0';
    }

    LogFlowFunc(("pszDir=%s\n", pszDir));

    /*
     * Process the files and subdirs.
     */
    for (;;)
    {
        /* Get the next directory. */
        size_t cbDirEntry = DNDTRANSFERLIST_DIRENTRY_BUF_SIZE;
        rc = RTDirReadEx(hDir, pDirEntry, &cbDirEntry, RTFSOBJATTRADD_UNIX, RTPATH_F_ON_LINK);
        if (RT_FAILURE(rc))
            break;

        /* Check length. */
        if (pDirEntry->cbName + cchDir + 3 >= RTPATH_MAX)
        {
            rc = VERR_BUFFER_OVERFLOW;
            break;
        }

        switch (pDirEntry->Info.Attr.fMode & RTFS_TYPE_MASK)
        {
            case RTFS_TYPE_SYMLINK:
            {
                if (!(fFlags & DNDTRANSFERLIST_FLAGS_RESOLVE_SYMLINKS))
                    break;
                RT_FALL_THRU();
            }
            case RTFS_TYPE_DIRECTORY:
            {
                if (RTDirEntryExIsStdDotLink(pDirEntry))
                    continue;

                memcpy(&pszDir[cchDir], pDirEntry->szName, pDirEntry->cbName + 1);
                int rc2 = dndTransferListAppendPathRecursiveSub(pList, pszDir, cchDir + pDirEntry->cbName, pDirEntry, fFlags);
                if (RT_SUCCESS(rc))
                    rc = rc2;
                break;
            }

            case RTFS_TYPE_FILE:
            {
                memcpy(&pszDir[cchDir], pDirEntry->szName, pDirEntry->cbName + 1);
                rc = dndTransferListObjAdd(pList, pszDir, pDirEntry->Info.Attr.fMode, fFlags);
                break;
            }

            default:
            {

                break;
            }
        }
    }

    if (rc == VERR_NO_MORE_FILES) /* Done reading current directory? */
    {
        rc = VINF_SUCCESS;
    }
    else if (RT_FAILURE(rc))
        LogRel(("DnD: Error while adding files recursively, rc=%Rrc\n", rc));

    int rc2 = RTDirClose(hDir);
    if (RT_FAILURE(rc2))
    {
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    return rc;
}

/**
 * Appends a native system path recursively by adding these entries as transfer objects.
 *
 * @returns VBox status code.
 * @param   pList               Transfer list to add found entries to.
 * @param   pcszPathAbs         Absolute path to add.
 * @param   fFlags              Flags of type DNDTRANSFERLISTFLAGS.
 */
static int dndTransferListAppendPathNativeRecursive(PDNDTRANSFERLIST pList,
                                                    const char *pcszPathAbs, DNDTRANSFERLISTFLAGS fFlags)
{
    char szPathAbs[RTPATH_MAX];
    int rc = RTStrCopy(szPathAbs, sizeof(szPathAbs), pcszPathAbs);
    if (RT_FAILURE(rc))
        return rc;

    union
    {
        uint8_t         abPadding[DNDTRANSFERLIST_DIRENTRY_BUF_SIZE];
        RTDIRENTRYEX    DirEntry;
    } uBuf;
    const size_t cchPathAbs = strlen(szPathAbs);
    if (!cchPathAbs)
        return VINF_SUCCESS;
    return dndTransferListAppendPathRecursiveSub(pList, szPathAbs, cchPathAbs, &uBuf.DirEntry, fFlags);
}

static int dndTransferListAppendPathNative(PDNDTRANSFERLIST pList, const char *pcszPath, DNDTRANSFERLISTFLAGS fFlags)
{
    /* We don't want to have a relative directory here. */
    if (!RTPathStartsWithRoot(pcszPath))
        return VERR_INVALID_PARAMETER;

    int rc = DnDPathValidate(pcszPath, false /* fMustExist */);
    if (RT_FAILURE(rc))
        return rc;

    char szPathAbs[RTPATH_MAX];
    rc = RTStrCopy(szPathAbs, sizeof(szPathAbs), pcszPath);
    if (RT_FAILURE(rc))
        return rc;

    size_t cchPathAbs = RTStrNLen(szPathAbs, sizeof(szPathAbs));
    AssertReturn(cchPathAbs, VERR_INVALID_PARAMETER);

    /* Convert path to transport style. */
    rc = DnDPathConvert(szPathAbs, sizeof(szPathAbs), DNDPATHCONVERT_FLAGS_TRANSPORT);
    if (RT_SUCCESS(rc))
    {
        /* Make sure the path has the same root path as our list. */
        if (RTPathStartsWith(szPathAbs, pList->pszPathRootAbs))
        {
            RTDIR hDir;
            rc = RTDirOpen(&hDir, szPathAbs);
            if (RT_SUCCESS(rc))
            {
                for (;;)
                {
                    /* Get the next directory. */
                    RTDIRENTRYEX dirEntry;
                    rc = RTDirReadEx(hDir, &dirEntry, NULL, RTFSOBJATTRADD_UNIX, RTPATH_F_ON_LINK /** @todo No symlinks yet. */);
                    if (RT_SUCCESS(rc))
                    {
                        if (RTDirEntryExIsStdDotLink(&dirEntry))
                            continue;

                        /* Check length. */
                        if (dirEntry.cbName + cchPathAbs + 3 >= sizeof(szPathAbs))
                        {
                            rc = VERR_BUFFER_OVERFLOW;
                            break;
                        }

                        /* Append the directory entry to our absolute path. */
                        memcpy(&szPathAbs[cchPathAbs], dirEntry.szName, dirEntry.cbName + 1);

                        LogFlowFunc(("szName=%s, szPathAbs=%s\n", dirEntry.szName, szPathAbs));

                        switch (dirEntry.Info.Attr.fMode & RTFS_TYPE_MASK)
                        {
                            case RTFS_TYPE_DIRECTORY:
                            {
                                rc = dndTransferListAppendPathNativeRecursive(pList, szPathAbs, fFlags);
                                break;
                            }

                            case RTFS_TYPE_FILE:
                            {
                                rc = dndTransferListObjAdd(pList, szPathAbs, dirEntry.Info.Attr.fMode, fFlags);
                                break;
                            }

                            default:
                                rc = VERR_NOT_SUPPORTED;
                                break;
                        }

                        if (RT_SUCCESS(rc))
                            rc = dndTransferListRootAdd(pList, dirEntry.szName);
                    }
                    else if (rc == VERR_NO_MORE_FILES)
                    {
                        rc = VINF_SUCCESS;
                        break;
                    }
                    else
                        break;

                    if (RT_FAILURE(rc))
                        break;
                }
            }
        }
        else
            rc = VERR_INVALID_PARAMETER;
    }

    if (RT_FAILURE(rc))
        LogRel(("DnD: Adding native path '%s' failed with rc=%Rrc\n", pcszPath, rc));

    return rc;
}

static int dndTransferListAppendPathURI(PDNDTRANSFERLIST pList, const char *pcszPath, DNDTRANSFERLISTFLAGS fFlags)
{
    RT_NOREF(fFlags);

    /* Query the path component of a file URI. If this hasn't a
     * file scheme, NULL is returned. */
    char *pszFilePath;
    int rc = RTUriFilePathEx(pcszPath, RTPATH_STR_F_STYLE_UNIX, &pszFilePath, 0 /*cbPath*/, NULL /*pcchPath*/);
    if (RT_SUCCESS(rc))
    {
        LogFlowFunc(("pcszPath=%s -> pszFilePath=%s\n", pcszPath, pszFilePath));
        rc = dndTransferListRootAdd(pList, pszFilePath);
        RTStrFree(pszFilePath);

    }

    if (RT_FAILURE(rc))
        LogRel(("DnD: Adding URI path '%s' failed with rc=%Rrc\n", pcszPath, rc));

    return rc;
}

/**
 * Appends a single path to a transfer list.
 *
 * @returns VBox status code. VERR_NOT_SUPPORTED if the path is not supported.
 * @param   pList               Transfer list to append to.
 * @param   enmFmt              Format of \a pszPaths to append.
 * @param   pcszPath            Path to append. Must be part of the list's set root path.
 * @param   fFlags              Transfer list flags to use for appending.
 */
int DnDTransferListAppendPath(PDNDTRANSFERLIST pList,
                              DNDTRANSFERLISTFMT enmFmt, const char *pcszPath, DNDTRANSFERLISTFLAGS fFlags)
{
    AssertPtrReturn(pList, VERR_INVALID_POINTER);
    AssertPtrReturn(pcszPath, VERR_INVALID_POINTER);
    AssertReturn(!(fFlags & ~DNDTRANSFERLIST_FLAGS_VALID_MASK), VERR_INVALID_FLAGS);
    AssertReturn(!(fFlags & DNDTRANSFERLIST_FLAGS_RESOLVE_SYMLINKS), VERR_NOT_SUPPORTED);

    int rc;

    switch (enmFmt)
    {
        case DNDTRANSFERLISTFMT_NATIVE:
            rc = dndTransferListAppendPathNative(pList, pcszPath, fFlags);
            break;

        case DNDTRANSFERLISTFMT_URI:
            rc = dndTransferListAppendPathURI(pList, pcszPath, fFlags);
            break;

        default:
            AssertFailedStmt(rc = VERR_NOT_SUPPORTED);
            break; /* Never reached */
    }

    return rc;
}

/**
 * Appends native paths to a transfer list.
 *
 * @returns VBox status code.
 * @param   pList               Transfer list to append to.
 * @param   enmFmt              Format of \a pszPaths to append.
 * @param   pszPaths            Buffer of paths to append.
 * @param   cbPaths             Size (in bytes) of buffer of paths to append.
 * @param   pcszSeparator       Separator string to use.
 * @param   fFlags              Transfer list flags to use for appending.
 */
int DnDTransferListAppendPathsFromBuffer(PDNDTRANSFERLIST pList,
                                         DNDTRANSFERLISTFMT enmFmt, const char *pszPaths, size_t cbPaths,
                                         const char *pcszSeparator, DNDTRANSFERLISTFLAGS fFlags)
{
    AssertPtrReturn(pList, VERR_INVALID_POINTER);
    AssertPtrReturn(pszPaths, VERR_INVALID_POINTER);
    AssertReturn(cbPaths, VERR_INVALID_PARAMETER);

    char **papszPaths = NULL;
    size_t cPaths = 0;
    int rc = RTStrSplit(pszPaths, cbPaths, pcszSeparator, &papszPaths, &cPaths);
    if (RT_SUCCESS(rc))
        rc = DnDTransferListAppendPathsFromArray(pList, enmFmt, papszPaths, cPaths, fFlags);

    for (size_t i = 0; i < cPaths; ++i)
        RTStrFree(papszPaths[i]);
    RTMemFree(papszPaths);

    return rc;
}

/**
 * Appends paths to a transfer list.
 *
 * @returns VBox status code. Will return VERR_INVALID_PARAMETER if a common root path could not be found.
 * @param   pList               Transfer list to append path to.
 * @param   enmFmt              Format of \a papcszPaths to append.
 * @param   papcszPaths         Array of paths to append.
 * @param   cPaths              Number of paths in \a papcszPaths to append.
 * @param   fFlags              Transfer list flags to use for appending.
 */
int DnDTransferListAppendPathsFromArray(PDNDTRANSFERLIST pList,
                                        DNDTRANSFERLISTFMT enmFmt,
                                        const char * const *papcszPaths, size_t cPaths, DNDTRANSFERLISTFLAGS fFlags)
{
    AssertPtrReturn(pList, VERR_INVALID_POINTER);
    AssertPtrReturn(papcszPaths, VERR_INVALID_POINTER);
    AssertReturn(!(fFlags & ~DNDTRANSFERLIST_FLAGS_VALID_MASK), VERR_INVALID_FLAGS);

    int rc = VINF_SUCCESS;

    if (!cPaths) /* Nothing to add? Bail out. */
        return VINF_SUCCESS;

    /* If we don't have a root path set, try to find the common path of all handed-in paths. */
    if (!pList->pszPathRootAbs)
    {
        size_t cchRootPath = RTPathFindCommon(papcszPaths, cPaths);
        if (cchRootPath)
        {
            /* Just use the first path in the array as the reference. */
            char *pszRootPath = RTStrDupN(papcszPaths[0], cchRootPath);
            if (pszRootPath)
            {
                LogRel2(("DnD: Determined root path is '%s'\n", pszRootPath));
                rc = dndTransferListSetRootPath(pList, pszRootPath);
                RTStrFree(pszRootPath);
            }
            else
                rc = VERR_NO_MEMORY;
        }
    }

    /*
     * Go through the created list and make sure all entries have the same root path.
     */
    for (size_t i = 0; i < cPaths; i++)
    {
        rc = DnDTransferListAppendPath(pList, enmFmt, papcszPaths[i], fFlags);
        if (RT_FAILURE(rc))
            break;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Returns the first transfer object in a list.
 *
 * @returns Pointer to transfer object if found, or NULL if not found.
 * @param   pList               Transfer list to get first transfer object from.
 */
PDNDTRANSFEROBJECT DnDTransferListObjGetFirst(PDNDTRANSFERLIST pList)
{
    AssertPtrReturn(pList, NULL);

    return RTListGetFirst(&pList->lstObj, DNDTRANSFEROBJECT, Node);
}

/**
 * Removes the first DnD transfer object from a transfer list.
 *
 * @param   pList               Transfer list to remove first entry for.
 */
void DnDTransferListObjRemoveFirst(PDNDTRANSFERLIST pList)
{
    AssertPtrReturnVoid(pList);

    if (!pList->cObj)
        return;

    PDNDTRANSFEROBJECT pObj = RTListGetFirst(&pList->lstObj, DNDTRANSFEROBJECT, Node);
    AssertPtr(pObj);

    uint64_t cbSize = DnDTransferObjectGetSize(pObj);
    Assert(pList->cbObjTotal >= cbSize);
    pList->cbObjTotal -= cbSize; /* Adjust total size. */

    dndTransferListObjFree(pList, pObj);
}

/**
 * Returns all root entries of a transfer list as a string.
 *
 * @returns VBox status code.
 * @param   pList               Transfer list to return root paths for.
 * @param   pcszPathBase        Root path to use as a base path. If NULL, the list's absolute root path will be used (if any).
 * @param   pcszSeparator       Separator to use for separating the root entries.
 * @param   ppszBuffer          Where to return the allocated string on success. Needs to be free'd with RTStrFree().
 * @param   pcbBuffer           Where to return the size (in bytes) of the allocated string on success, including terminator.
 */
int DnDTransferListGetRootsEx(PDNDTRANSFERLIST pList,
                              DNDTRANSFERLISTFMT enmFmt, const char *pcszPathBase, const char *pcszSeparator,
                              char **ppszBuffer, size_t *pcbBuffer)
{
    AssertPtrReturn(pList, VERR_INVALID_POINTER);
    /* pcszPathBase can be NULL. */
    AssertPtrReturn(pcszSeparator, VERR_INVALID_POINTER);
    AssertPtrReturn(ppszBuffer, VERR_INVALID_POINTER);
    AssertPtrReturn(pcbBuffer, VERR_INVALID_POINTER);

    char *pszString = NULL;

    /* Find out which root path to use. */
    const char *pcszPathRootTmp = pcszPathBase ? pcszPathBase : pList->pszPathRootAbs;
    /* pcszPathRootTmp can be NULL*/

    LogFlowFunc(("Using root path '%s'\n", pcszPathRootTmp ? pcszPathRootTmp : "<None>"));

    int rc = DnDPathValidate(pcszPathRootTmp, false /* fMustExist */);
    if (RT_FAILURE(rc))
        return rc;

    char szPath[RTPATH_MAX];

    PDNDTRANSFERLISTROOT pRoot;
    RTListForEach(&pList->lstRoot, pRoot, DNDTRANSFERLISTROOT, Node)
    {
        if (pcszPathRootTmp)
        {
            rc = RTStrCopy(szPath, sizeof(szPath), pcszPathRootTmp);
            AssertRCBreak(rc);
        }

        rc = RTPathAppend(szPath, sizeof(szPath), pRoot->pszPathRoot);
        AssertRCBreak(rc);

        if (enmFmt == DNDTRANSFERLISTFMT_URI)
        {
            char *pszPathURI = RTUriFileCreate(szPath);
            AssertPtrBreakStmt(pszPathURI, rc = VERR_NO_MEMORY);

            rc = RTStrAAppend(&pszString, pszPathURI);
            RTStrFree(pszPathURI);
            AssertRCBreak(rc);
        }
        else
        {
            rc = RTStrAAppend(&pszString, szPath);
            AssertRCBreak(rc);
        }

        rc = RTStrAAppend(&pszString, pcszSeparator);
        AssertRCBreak(rc);
    }

    if (RT_SUCCESS(rc))
    {
        *ppszBuffer = pszString;
        *pcbBuffer  = pszString ? strlen(pszString) + 1 /* Include termination */ : 0;
    }
    else
        RTStrFree(pszString);
    return rc;
}

int DnDTransferListGetRoots(PDNDTRANSFERLIST pList,
                            DNDTRANSFERLISTFMT enmFmt, char **ppszBuffer, size_t *pcbBuffer)
{
    return DnDTransferListGetRootsEx(pList, enmFmt, "" /* pcszPathRoot */, DND_PATH_SEPARATOR,
                                     ppszBuffer, pcbBuffer);
}

/**
 * Returns the total root entries count for a DnD transfer list.
 *
 * @returns Total number of root entries.
 * @param   pList               Transfer list to return total number of root entries for.
 */
uint64_t DnDTransferListGetRootCount(PDNDTRANSFERLIST pList)
{
    AssertPtrReturn(pList, 0);
    return pList->cRoots;
}

/**
 * Returns the absolute root path for a DnD transfer list.
 *
 * @returns Pointer to the root path.
 * @param   pList               Transfer list to return root path for.
 */
const char *DnDTransferListGetRootPathAbs(PDNDTRANSFERLIST pList)
{
    AssertPtrReturn(pList, NULL);
    return pList->pszPathRootAbs;
}

/**
 * Returns the total transfer object count for a DnD transfer list.
 *
 * @returns Total number of transfer objects.
 * @param   pList               Transfer list to return total number of transfer objects for.
 */
uint64_t DnDTransferListObjCount(PDNDTRANSFERLIST pList)
{
    AssertPtrReturn(pList, 0);
    return pList->cObj;
}

/**
 * Returns the total bytes of all handled transfer objects for a DnD transfer list.
 *
 * @returns VBox status code.
 * @param   pList               Transfer list to return total bytes for.
 */
uint64_t DnDTransferListObjTotalBytes(PDNDTRANSFERLIST pList)
{
    AssertPtrReturn(pList, 0);
    return pList->cbObjTotal;
}

/**
 * Sets the absolute root path of a transfer list.
 *
 * @returns VBox status code.
 * @param   pList               Transfer list to set root path for.
 * @param   pcszRootPathAbs     Absolute root path to set.
 */
static int dndTransferListSetRootPath(PDNDTRANSFERLIST pList, const char *pcszRootPathAbs)
{
    AssertPtrReturn(pList, VERR_INVALID_POINTER);
    AssertPtrReturn(pcszRootPathAbs, VERR_INVALID_POINTER);
    AssertReturn(pList->pszPathRootAbs == NULL, VERR_WRONG_ORDER); /* Already initialized? */

    LogFlowFunc(("pcszRootPathAbs=%s\n", pcszRootPathAbs));

    char szRootPath[RTPATH_MAX];
    int rc = RTStrCopy(szRootPath, sizeof(szRootPath), pcszRootPathAbs);
    if (RT_FAILURE(rc))
        return rc;

    /* Note: The list's root path is kept in native style, so no conversion needed here. */

    RTPathEnsureTrailingSeparatorEx(szRootPath, sizeof(szRootPath), RTPATH_STR_F_STYLE_HOST);

    pList->pszPathRootAbs = RTStrDup(szRootPath);
    if (pList->pszPathRootAbs)
    {
        LogFlowFunc(("Root path is '%s'\n", pList->pszPathRootAbs));
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}

static int dndTransferListRootAdd(PDNDTRANSFERLIST pList, const char *pcszRoot)
{
    int rc;

    PDNDTRANSFERLISTROOT pRoot = (PDNDTRANSFERLISTROOT)RTMemAllocZ(sizeof(DNDTRANSFERLISTROOT));
    if (pRoot)
    {
        pRoot->pszPathRoot = RTStrDup(pcszRoot);
        if (pRoot->pszPathRoot)
        {
            RTListAppend(&pList->lstRoot, &pRoot->Node);
            pList->cRoots++;

            rc = VINF_SUCCESS;
        }
        else
            rc = VERR_NO_MEMORY;

        if (RT_FAILURE(rc))
        {
            RTMemFree(pRoot);
            pRoot = NULL;
        }
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}

/**
 * Frees an internal DnD transfer root.
 *
 * @param   pList               Transfer list to free root for.
 * @param   pRootObj            Transfer list root to free. The pointer will be invalid after calling.
 */
static void dndTransferListRootFree(PDNDTRANSFERLIST pList, PDNDTRANSFERLISTROOT pRootObj)
{
    if (!pRootObj)
        return;

    RTStrFree(pRootObj->pszPathRoot);

    RTListNodeRemove(&pRootObj->Node);
    RTMemFree(pRootObj);

    AssertReturnVoid(pList->cRoots);
    pList->cRoots--;
}


#if 0
/**
 * Appends a single URI path to a transfer list.
 *
 * @returns VBox status code.
 * @param   pList               Transfer list to append URI path to.
 * @param   pszURIPath          URI path to append.
 * @param   fFlags              Transfer list flags to use for appending.
 */
int DnDTransferListURIAppendPath(PDNDTRANSFERLIST pList, const char *pszURIPath, DNDTRANSFERLISTFLAGS fFlags)
{
    AssertPtrReturn(pList, VERR_INVALID_POINTER);
    AssertPtrReturn(pszURIPath, VERR_INVALID_POINTER);
    AssertReturn(!(fFlags & ~DNDTRANSFERLIST_FLAGS_VALID_MASK), VERR_INVALID_FLAGS);

    in rc;

    /* Query the path component of a file URI. If this hasn't a
     * file scheme, NULL is returned. */
    char *pszFilePath = RTUriFilePathEx(pszURIPath, RTPATH_STR_F_STYLE_UNIX, &pszFilePath, 0 /*cbPath*/, NULL /*pcchPath*/);
    LogFlowFunc(("pszPath=%s, pszFilePath=%s\n", pszFilePath));
    if (pszFilePath)
    {
        rc = DnDPathValidate(pszFilePath, false /* fMustExist */);
        if (RT_SUCCESS(rc))
        {
            uint32_t fPathConvert = DNDPATHCONVERT_FLAGS_TRANSPORT;
#ifdef RT_OS_WINDOWS
            fPathConvert |= DNDPATHCONVERT_FLAGS_TO_DOS;
#endif
            rc = DnDPathConvert(pszFilePath, strlen(pszFilePath) + 1, fPathConvert);
            if (RT_SUCCESS(rc))
            {
                LogRel2(("DnD: Got URI data item '%s'\n", pszFilePath));

                PDNDTRANSFERLISTROOT pRoot = (PDNDTRANSFERLISTROOT)RTMemAlloc(sizeof(DNDTRANSFERLISTROOT));
                if (pRoot)
                {
                    pRoot->pszPathRoot = pszFilePath;

                    RTListAppend(&pList->lstRoot, &pRoot->Node);
                    pList->cRoots++;

                }
                else
                    rc = VERR_NO_MEMORY;
            }
            else
                LogRel(("DnD: Path conversion of URI data item '%s' failed with %Rrc\n", pszFilePath, rc));
        }
        else
            LogRel(("DnD: Path validation for URI data item '%s' failed with %Rrc\n", pszFilePath, rc));

        if (RT_FAILURE(rc))
            RTStrFree(pszFilePath);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Appends transfer list items from an URI string buffer.
 *
 * @returns VBox status code.
 * @param   pList               Transfer list to append list data to.
 * @param   pszURIPaths         String list to append.
 * @param   cbURIPaths          Size (in bytes) of string list to append.
 * @param   pcszSeparator       Separator string to use for separating strings of \a pszURIPathsAbs.
 * @param   fFlags              Transfer list flags to use for appending.
 */
int DnDTransferListURIAppendFromBuffer(PDNDTRANSFERLIST pList,
                                       const char *pszURIPaths, size_t cbURIPaths,
                                       const char *pcszSeparator, DNDTRANSFERLISTFLAGS fFlags)
{
    AssertPtrReturn(pList, VERR_INVALID_POINTER);
    AssertPtrReturn(pszURIPaths, VERR_INVALID_POINTER);
    AssertReturn(cbURIPaths, VERR_INVALID_PARAMETER);
    AssertReturn(!(fFlags & ~DNDTRANSFERLIST_FLAGS_VALID_MASK), VERR_INVALID_FLAGS);

    char **papszPaths = NULL;
    size_t cPaths = 0;
    int rc = RTStrSplit(pszURIPaths, cbURIPaths, pcszSeparator, &papszPaths, &cPaths);
    if (RT_SUCCESS(rc))
    {
        for (size_t i = 0; i < cPaths; i++)
        {
            rc = DnDTransferListURIAppendPath(pList, papszPaths[i], fFlags);
            if (RT_FAILURE(rc))
                break;
        }

        for (size_t i = 0; i < cPaths; ++i)
            RTStrFree(papszPaths[i]);
        RTMemFree(papszPaths);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}
#endif
