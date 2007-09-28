/** @file
 *
 * Shared Folders:
 * VBox Shared Folders.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "mappings.h"
#include "vbsf.h"
#include "shflhandle.h"

#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/fs.h>
#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/uni.h>

#undef LogFlow
#define LogFlow Log

void vbsfStripLastComponent (char *pszFullPath, uint32_t cbFullPathRoot)
{
    RTUNICP cp;

    /* Do not strip root. */
    char *s = pszFullPath + cbFullPathRoot;
    char *delimSecondLast = NULL;
    char *delimLast = NULL;

    LogFlowFunc(("%s -> %s\n", pszFullPath, s));

    for (;;)
    {
        cp = RTStrGetCp(s);

        if (cp == RTUNICP_INVALID || cp == 0)
        {
            break;
        }

        if (cp == RTPATH_DELIMITER)
        {
            if (delimLast != NULL)
            {
                delimSecondLast = delimLast;
            }

            delimLast = s;
        }

        s = RTStrNextCp (s);
    }

    if (cp == 0)
    {
        if (delimLast + 1 == s)
        {
            if (delimSecondLast)
            {
                *delimSecondLast = 0;
            }
            else if (delimLast)
            {
                *delimLast = 0;
            }
        }
        else
        {
            if (delimLast)
            {
                *delimLast = 0;
            }
        }
    }

    LogFlowFunc(("%s, %s, %s\n", pszFullPath, delimLast, delimSecondLast));
}

static int vbsfCorrectCasing(char *pszFullPath, char *pszStartComponent)
{
    PRTDIRENTRYEX  pDirEntry = NULL;
    uint32_t       cbDirEntry, cbComponent;
    int            rc = VERR_FILE_NOT_FOUND;
    PRTDIR         hSearch = 0;
    char           szWildCard[4];

    Log2(("vbsfCorrectCasing: %s %s\n", pszFullPath, pszStartComponent));

    cbComponent = strlen(pszStartComponent);

    cbDirEntry = 4096;
    pDirEntry  = (PRTDIRENTRYEX)RTMemAlloc(cbDirEntry);
    if (pDirEntry == 0)
    {
        AssertFailed();
        return VERR_NO_MEMORY;
    }

    /** @todo this is quite inefficient, especially for directories with many files */
    Assert(pszFullPath < pszStartComponent-1);
    Assert(*(pszStartComponent-1) == RTPATH_DELIMITER);
    *(pszStartComponent-1) = 0;
    strcpy(pDirEntry->szName, pszFullPath);
    szWildCard[0] = RTPATH_DELIMITER;
    szWildCard[1] = '*';
    szWildCard[2] = 0;
    strcat(pDirEntry->szName, szWildCard);

    rc = RTDirOpenFiltered (&hSearch, pDirEntry->szName, RTDIRFILTER_WINNT);
    *(pszStartComponent-1) = RTPATH_DELIMITER;
    if (VBOX_FAILURE(rc))
        goto end;

    for(;;)
    {
        uint32_t cbDirEntrySize = cbDirEntry;

        rc = RTDirReadEx(hSearch, pDirEntry, &cbDirEntrySize, RTFSOBJATTRADD_NOTHING);
        if (rc == VERR_NO_MORE_FILES)
            break;

        if (VINF_SUCCESS != rc && rc != VWRN_NO_DIRENT_INFO)
        {
            AssertFailed();
            if (rc != VERR_NO_TRANSLATION)
                break;
            else
                continue;
        }

        Log2(("vbsfCorrectCasing: found %s\n", &pDirEntry->szName[0]));
        if (    pDirEntry->cbName == cbComponent
            &&  !RTStrICmp(pszStartComponent, &pDirEntry->szName[0]))
        {
            Log(("Found original name %s (%s)\n", &pDirEntry->szName[0], pszStartComponent));
            strcpy(pszStartComponent, &pDirEntry->szName[0]);
            rc = VINF_SUCCESS;
            break;
        }
    }

end:
    if (VBOX_FAILURE(rc))
        Log(("vbsfCorrectCasing %s failed with %d\n", pszStartComponent, rc));

    if (pDirEntry)
        RTMemFree(pDirEntry);

    if (hSearch)
        RTDirClose(hSearch);
    return rc;
}

static int vbsfBuildFullPath (SHFLCLIENTDATA *pClient, SHFLROOT root, SHFLSTRING *pPath,
                              uint32_t cbPath, char **ppszFullPath, uint32_t *pcbFullPathRoot, bool fWildCard = false)
{
    int rc = VINF_SUCCESS;

    char *pszFullPath = NULL;

    /* Query UCS2 root prefix for the path, cbRoot is the length in bytes including trailing (RTUCS2)0. */
    uint32_t cbRoot = 0;
    const RTUCS2 *pszRoot = vbsfMappingsQueryHostRoot (root, &cbRoot);

    if (!pszRoot || cbRoot == 0)
    {
        Log(("vbsfBuildFullPath: invalid root!\n"));
        return VERR_INVALID_PARAMETER;
    }

    if (BIT_FLAG(pClient->fu32Flags, SHFL_CF_UTF8))
    {
        int rc;
        char *utf8Root;

        rc = RTUtf16ToUtf8 (pszRoot, &utf8Root);
        if (VBOX_SUCCESS (rc))
        {
            size_t cbUtf8Root, cbUtf8FullPath;
            char *utf8FullPath;

            cbUtf8Root = strlen (utf8Root);
            cbUtf8FullPath = cbUtf8Root + 1 + pPath->u16Length + 1;
            utf8FullPath = (char *) RTMemAllocZ (cbUtf8FullPath);

            if (!utf8FullPath)
            {
                rc = VERR_NO_MEMORY;
                *ppszFullPath = NULL;
                Log(("RTMemAllocZ %x failed!!\n", cbUtf8FullPath));
            }
            else
            {
                memcpy (utf8FullPath, utf8Root, cbUtf8Root);
                memcpy (utf8FullPath + cbUtf8Root + 1,
                        &pPath->String.utf8[0],
                        pPath->u16Length);

                utf8FullPath[cbUtf8Root] = '/';
                utf8FullPath[cbUtf8FullPath - 1] = 0;
                pszFullPath = utf8FullPath;

                if (pcbFullPathRoot)
                    *pcbFullPathRoot = cbUtf8Root; /* Must index the path delimiter.  */
            }

            RTStrFree (utf8Root);
        }
        else
        {
            Log (("vbsfBuildFullPath: RTUtf16ToUtf8 failed with %Vrc\n", rc));
        }
    }
    else
    {
        /* Client sends us UCS2, so convert it to UTF8. */
        Log(("Root %ls path %.*ls\n", pszRoot, pPath->u16Length/sizeof(pPath->String.ucs2[0]), pPath->String.ucs2));

        /* Allocate buffer that will be able to contain
         * the root prefix and the pPath converted to UTF8.
         * Expect a 2 bytes UCS2 to be converted to 8 bytes UTF8
         * in worst case.
         */
        uint32_t cbFullPath = (cbRoot/sizeof (RTUCS2) + ShflStringLength (pPath)) * 4;

        pszFullPath = (char *)RTMemAllocZ (cbFullPath);

        if (!pszFullPath)
        {
            rc = VERR_NO_MEMORY;
        }
        else
        {
            uint32_t cb = cbFullPath;

            rc = RTStrUcs2ToUtf8Ex (&pszFullPath, cb, pszRoot);
            if (VBOX_FAILURE(rc))
            {
                AssertFailed();
                return rc;
            }

            char *dst = pszFullPath;

            cbRoot = strlen(dst);
            if (dst[cbRoot - 1] != RTPATH_DELIMITER)
            {
                dst[cbRoot] = RTPATH_DELIMITER;
                cbRoot++;
            }

            if (pcbFullPathRoot)
                *pcbFullPathRoot = cbRoot - 1; /* Must index the path delimiter.  */

            dst   += cbRoot;
            cb    -= cbRoot;

            if (pPath->u16Length)
            {
                /* Convert and copy components. */
                RTUCS2 *src = &pPath->String.ucs2[0];

                /* Correct path delimiters */
                if (pClient->PathDelimiter != RTPATH_DELIMITER)
                {
                    LogFlow(("Correct path delimiter in %ls\n", src));
                    while (*src)
                    {
                        if (*src == pClient->PathDelimiter)
                            *src = RTPATH_DELIMITER;
                        src++;
                    }
                    src = &pPath->String.ucs2[0];
                    LogFlow(("Corrected string %ls\n", src));
                }
                if (*src == RTPATH_DELIMITER)
                    src++;  /* we already appended a delimiter to the first part */

                rc = RTStrUcs2ToUtf8Ex (&dst, cb, src);
                if (VBOX_FAILURE(rc))
                {
                    AssertFailed();
                    return rc;
                }

                uint32_t l = strlen (dst);

                cb -= l;
                dst += l;

                Assert(cb > 0);
            }

            /* Nul terminate the string */
            *dst = 0;
        }
    }

    if (VBOX_SUCCESS (rc))
    {
        /* When the host file system is case sensitive and the guest expects a case insensitive fs, then problems can occur */
        if (     vbsfIsHostMappingCaseSensitive (root)
            &&  !vbsfIsGuestMappingCaseSensitive(root))
        {
            RTFSOBJINFO info;
            char *pszWildCardComponent = NULL;

            if (fWildCard)
            {
                /* strip off the last path component, that contains the wildcard(s) */
                uint32_t len = strlen(pszFullPath);
                char    *src = pszFullPath + len - 1;

                while(src > pszFullPath)
                {
                    if (*src == RTPATH_DELIMITER)
                        break;
                    src--;
                }
                if (*src == RTPATH_DELIMITER)
                {
                    bool fHaveWildcards = false;
                    char *temp = src;

                    while(*temp)
                    {
                        char uc = *temp;
                        if (uc == '*' || uc == '?' || uc == '>' || uc == '<' || uc == '"')
                        {
                            fHaveWildcards = true;
                            break;
                        }
                        temp++;
                    }

                    if (fHaveWildcards)
                    {
                        pszWildCardComponent = src;
                        *pszWildCardComponent = 0;
                    }
                }
            }

            /** @todo don't check when creating files or directories; waste of time */
            rc = RTPathQueryInfo(pszFullPath, &info, RTFSOBJATTRADD_NOTHING);
            if (rc == VERR_FILE_NOT_FOUND || rc == VERR_PATH_NOT_FOUND)
            {
                uint32_t len = strlen(pszFullPath);
                char    *src = pszFullPath + len - 1;
        
                Log(("Handle case insenstive guest fs on top of host case sensitive fs for %s\n", pszFullPath));

                /* Find partial path that's valid */
                while(src > pszFullPath)
                {
                    if (*src == RTPATH_DELIMITER)
                    {
                        *src = 0;
                        rc = RTPathQueryInfo (pszFullPath, &info, RTFSOBJATTRADD_NOTHING);
                        *src = RTPATH_DELIMITER;
                        if (rc == VINF_SUCCESS)
                        {
#ifdef DEBUG
                            *src = 0;
                            Log(("Found valid partial path %s\n", pszFullPath));
                            *src = RTPATH_DELIMITER;
#endif
                            break;
                        }
                    }

                    src--;
                }
                Assert(*src == RTPATH_DELIMITER && VBOX_SUCCESS(rc));
                if (    *src == RTPATH_DELIMITER 
                    &&  VBOX_SUCCESS(rc))
                {
                    src++;
                    for(;;)
                    {
                        char *end = src;
                        bool fEndOfString = true;

                        while(*end)
                        {
                            if (*end == RTPATH_DELIMITER)
                                break;
                            end++;
                        }

                        if (*end == RTPATH_DELIMITER)
                        {
                            fEndOfString = false;
                            *end = 0;
                            rc = RTPathQueryInfo(src, &info, RTFSOBJATTRADD_NOTHING);
                            Assert(rc == VINF_SUCCESS || rc == VERR_FILE_NOT_FOUND || rc == VERR_PATH_NOT_FOUND);
                        }
                        else
                        if (end == src)
                            rc = VINF_SUCCESS;  /* trailing delimiter */
                        else
                            rc = VERR_FILE_NOT_FOUND;
            
                        if (rc == VERR_FILE_NOT_FOUND || rc == VERR_PATH_NOT_FOUND)
                        {
                            /* path component is invalid; try to correct the casing */
                            rc = vbsfCorrectCasing(pszFullPath, src);
                            if (VBOX_FAILURE(rc))
                            {
                                if (!fEndOfString)
                                    *end = RTPATH_DELIMITER; /* restore the original full path */
                                break;
                            }
                        }

                        if (fEndOfString)
                            break;

                        *end = RTPATH_DELIMITER;
                        src = end + 1;
                    }
                    if (VBOX_FAILURE(rc))
                        Log(("Unable to find suitable component rc=%d\n", rc));
                }
                else
                    rc = VERR_FILE_NOT_FOUND;

            }
            if (pszWildCardComponent)
                *pszWildCardComponent = RTPATH_DELIMITER;

            /* might be a new file so don't fail here! */
            rc = VINF_SUCCESS;
        }
        *ppszFullPath = pszFullPath;

        LogFlow(("vbsfBuildFullPath: %s rc=%Vrc\n", pszFullPath, rc));
    }

    return rc;
}

static void vbsfFreeFullPath (char *pszFullPath)
{
    RTMemFree (pszFullPath);
}


static int vbsfOpenFile (SHFLHANDLE *phHandle, const char *pszPath, SHFLCREATEPARMS *pParms, bool fCreate)
{
    int rc = VINF_SUCCESS;

    LogFlow(("vbsfOpenFile: pszPath = %s, pParms = %p, fCreate = %d\n",
             pszPath, pParms, fCreate));

    /** @todo r=bird: You should've requested a better RTFileOpen API! This code could certainly have
     * benefitted from it. I've done the long overdue adjustment of RTFileOpen so it better reflect
     * what a decent OS should be able to do. I've also added some OS specific flags (non-blocking,
     * delete sharing), and I'm not picky about adding more if that required. (I'm only picky about
     * how they are treated on platforms which doesn't support them.)
     * Because of the restrictions in the old version of RTFileOpen this code contains dangerous race
     * conditions. File creation is one example where you may easily kill a file just created by
     * another user.
     */

    /* Open or create a file. */
    unsigned fOpen;

    Log(("SHFL create flags %08x\n", pParms->CreateFlags));

    if (BIT_FLAG(pParms->CreateFlags, SHFL_CF_DIRECTORY))
    {
        fOpen = RTFILE_O_OPEN;
    }
    else
        fOpen = fCreate? RTFILE_O_CREATE_REPLACE: RTFILE_O_OPEN;

    switch (BIT_FLAG(pParms->CreateFlags, SHFL_CF_ACCESS_MASK_RW))
    {
        default:
        case SHFL_CF_ACCESS_NONE:
        {
            /** @todo treat this as read access, but theoretically this could be a no access requested. */
            fOpen |= RTFILE_O_READ;
            Log(("FLAG: SHFL_CF_ACCESS_NONE\n"));
            break;
        }

        case SHFL_CF_ACCESS_READ:
        {
            fOpen |= RTFILE_O_READ;
            Log(("FLAG: SHFL_CF_ACCESS_READ\n"));
            break;
        }

        case SHFL_CF_ACCESS_WRITE:
        {
            fOpen |= RTFILE_O_WRITE;
            Log(("FLAG: SHFL_CF_ACCESS_WRITE\n"));
            break;
        }

        case SHFL_CF_ACCESS_READWRITE:
        {
            fOpen |= RTFILE_O_READWRITE;
            Log(("FLAG: SHFL_CF_ACCESS_READWRITE\n"));
            break;
        }
    }

    /* Sharing mask */
    switch (BIT_FLAG(pParms->CreateFlags, SHFL_CF_ACCESS_MASK_DENY))
    {
    default:
    case SHFL_CF_ACCESS_DENYNONE:
        fOpen |= RTFILE_O_DENY_NONE;
        Log(("FLAG: SHFL_CF_ACCESS_DENYNONE\n"));
        break;

    case SHFL_CF_ACCESS_DENYREAD:
        fOpen |= RTFILE_O_DENY_READ;
        Log(("FLAG: SHFL_CF_ACCESS_DENYREAD\n"));
        break;

    case SHFL_CF_ACCESS_DENYWRITE:
        fOpen |= RTFILE_O_DENY_WRITE;
        Log(("FLAG: SHFL_CF_ACCESS_DENYWRITE\n"));
        break;

    case SHFL_CF_ACCESS_DENYALL:
        fOpen |= RTFILE_O_DENY_ALL;
        Log(("FLAG: SHFL_CF_ACCESS_DENYALL\n"));
        break;
    }

    SHFLHANDLE      handle;
    SHFLFILEHANDLE *pHandle;

    if (BIT_FLAG(pParms->CreateFlags, SHFL_CF_DIRECTORY))
    {
        handle  = vbsfAllocDirHandle();
        pHandle = (SHFLFILEHANDLE *)vbsfQueryHandle(handle, SHFL_HF_TYPE_DIR);
    }
    else
    {
        handle  = vbsfAllocFileHandle();
        pHandle = (SHFLFILEHANDLE *)vbsfQueryHandle(handle, SHFL_HF_TYPE_FILE);
    }

    if (pHandle == NULL)
    {
        rc = VERR_NO_MEMORY;
    }
    else
    {
        /* Must obviously create the directory, before trying to open it. */
        if (BIT_FLAG(pParms->CreateFlags, SHFL_CF_DIRECTORY))
        {
            if (fCreate)
            {
                /** @todo render supplied attributes.
                * bird: The guest should specify this. For windows guests RTFS_DOS_DIRECTORY should suffice. */
                RTFMODE fMode = 0777;

                rc = RTDirCreate(pszPath, fMode);
                if (VBOX_FAILURE(rc))
                {
                    vbsfFreeHandle (handle);
                    return rc;
                }
            }
            /* Open the directory now */
            if (VBOX_SUCCESS(rc))
            {
                rc = RTDirOpen (&pHandle->dir.Handle, pszPath);
                if (VBOX_FAILURE (rc))
                {
                    vbsfFreeHandle (handle);
                    *phHandle = SHFL_HANDLE_NIL;
                    return rc;
                }
            }
        }
        else
        {
            rc = RTFileOpen(&pHandle->file.Handle, pszPath, fOpen);
        }

        if (VBOX_SUCCESS (rc))
        {
            *phHandle = handle;
        }
        else
        {
             vbsfFreeHandle (handle);
        }
    }

    LogFlow(("vbsfOpenFile: rc = %Vrc\n", rc));


    return rc;
}

static int vbsfOpenExisting (const char *pszFullPath, SHFLCREATEPARMS *pParms)
{
    int rc = VINF_SUCCESS;

    LogFlow(("vbsfOpenExisting: pszFullPath = %s, pParms = %p\n",
             pszFullPath, pParms));

    /* Open file. */
    SHFLHANDLE      handle;

    rc = vbsfOpenFile (&handle, pszFullPath, pParms, false);
    if (VBOX_SUCCESS (rc))
    {
        pParms->Handle = handle;
    }

    LogFlow(("vbsfOpenExisting: rc = %d\n", rc));

    return rc;
}


static int vbsfOpenReplace (const char *pszPath, SHFLCREATEPARMS *pParms, bool bReplace, RTFSOBJINFO *pInfo)
{
    int rc = VINF_SUCCESS;

    LogFlow(("vbsfOpenReplace: pszPath = %s, pParms = %p, bReplace = %d\n",
             pszPath, pParms, bReplace));

    if (BIT_FLAG(pParms->CreateFlags, SHFL_CF_DIRECTORY))
    {
        /* Replace operation is not applicable to a directory. */
        rc = VERR_INVALID_PARAMETER;
    }
    else
    {
        SHFLHANDLE handle = SHFL_HANDLE_NIL;
        SHFLFILEHANDLE *pHandle;

        rc = vbsfOpenFile (&handle, pszPath, pParms, true);
        // We are loosing an information regarding the cause of failure here
        // -- malc
        pHandle = (SHFLFILEHANDLE *)vbsfQueryHandle(handle, SHFL_HF_TYPE_FILE);
        if (!pHandle)
        {
            AssertFailed();
            rc = VERR_INVALID_HANDLE;
        }

        if (VBOX_SUCCESS (rc))
        {

            /* Set new file attributes */

            rc = RTFileSetSize(pHandle->file.Handle, pParms->Info.cbObject);
            if (rc != VINF_SUCCESS)
            {
                AssertMsg(rc == VINF_SUCCESS, ("RTFileSetSize failed with %d\n", rc));
                return rc;
            }

            if (bReplace)
            {
#if 0
                /* @todo */
                /* Set new attributes. */
                RTFileSetTimes(pHandle->file.Handle,
                               &pParms->Info.AccessTime,
                               &pParms->Info.ModificationTime,
                               &pParms->Info.ChangeTime,
                               &pParms->Info.BirthTime
                              );

                RTFileSetMode (pHandle->file.Handle, pParms->Info.Attr.fMode);
#endif
            }

            pParms->Result = SHFL_FILE_REPLACED;
            pParms->Handle = handle;
        }
    }

    LogFlow(("vbsfOpenReplace: rc = %Vrc\n", rc));

    return rc;
}

static int vbsfOpenCreate (const char *pszPath, SHFLCREATEPARMS *pParms)
{
    int rc = VINF_SUCCESS;

    LogFlow(("vbsfOpenCreate: pszPath = %s, pParms = %p\n",
             pszPath, pParms));

    SHFLHANDLE      handle = SHFL_HANDLE_NIL;
    SHFLFILEHANDLE *pHandle;

    rc = vbsfOpenFile (&handle, pszPath, pParms, true);
    pHandle = (SHFLFILEHANDLE *)vbsfQueryHandle(handle, SHFL_HF_TYPE_FILE | SHFL_HF_TYPE_DIR);
    if (!pHandle)
    {
        AssertFailed();
        rc = VERR_INVALID_HANDLE;
    }

    if (VBOX_SUCCESS (rc))
    {
#if 0
        if (!BIT_FLAG(pParms->CreateFlags, SHFL_CF_DIRECTORY))
        {
            /* @todo */
            RTFileSetSize(pHandle->file.Handle, pParms->Info.cbObject);

            RTFileSetTimes(pHandle->file.Handle,
                           &pParms->Info.AccessTime,
                           &pParms->Info.ModificationTime,
                           &pParms->Info.ChangeTime,
                           &pParms->Info.BirthTime
                          );

            RTFileSetMode (pHandle->file.Handle, pParms->Info.Attr.fMode);
        }
#endif

        pParms->Result = SHFL_FILE_CREATED;
        pParms->Handle = handle;
    }

    LogFlow(("vbsfOpenCreate: rc = %Vrc\n", rc));

    return rc;
}


static int vbsfCloseDir (SHFLFILEHANDLE *pHandle)
{
    int rc = VINF_SUCCESS;

    LogFlow(("vbsfCloseDir: Handle = %08X Search Handle = %08X\n",
             pHandle->dir.Handle, pHandle->dir.SearchHandle));

    RTDirClose (pHandle->dir.Handle);

    if (pHandle->dir.SearchHandle)
        RTDirClose(pHandle->dir.SearchHandle);

    if (pHandle->dir.pLastValidEntry)
    {
        RTMemFree(pHandle->dir.pLastValidEntry);
        pHandle->dir.pLastValidEntry = NULL;
    }

    LogFlow(("vbsfCloseDir: rc = %d\n", rc));

    return rc;
}


static int vbsfCloseFile (SHFLFILEHANDLE *pHandle)
{
    int rc = VINF_SUCCESS;

    LogFlow(("vbsfCloseFile: Handle = %08X\n",
             pHandle->file.Handle));

    rc = RTFileClose (pHandle->file.Handle);

    LogFlow(("vbsfCloseFile: rc = %d\n", rc));

    return rc;
}


int vbsfCreate (SHFLCLIENTDATA *pClient, SHFLROOT root, SHFLSTRING *pPath, uint32_t cbPath, SHFLCREATEPARMS *pParms)
{
    int rc = VINF_SUCCESS;

    LogFlow(("vbsfCreate: pClient = %p, pPath = %p, cbPath = %d, pParms = %p CreateFlags=%x\n",
             pClient, pPath, cbPath, pParms, pParms->CreateFlags));

    /* Check the client access rights to the root. */
    /** @todo */

    /* Build a host full path for the given path
     * and convert ucs2 to utf8 if necessary.
     */
    char *pszFullPath = NULL;
    uint32_t cbFullPathRoot = 0;

    rc = vbsfBuildFullPath (pClient, root, pPath, cbPath, &pszFullPath, &cbFullPathRoot);

    /* @todo This mess needs to change. RTFileOpen supports all the open/creation methods */

    if (VBOX_SUCCESS (rc))
    {
        /* Reset return values in case client forgot to do so. */
        pParms->Result = SHFL_NO_RESULT;
        pParms->Handle = SHFL_HANDLE_NIL;

        /* Query path information. */
        RTFSOBJINFO info;

        /** r=bird: This is likely to create race conditions.
         * What is a file now can be a directory when you open it. */
        rc = RTPathQueryInfo (pszFullPath, &info, RTFSOBJATTRADD_NOTHING);
        LogFlow(("RTPathQueryInfo returned %Vrc\n", rc));

        if (BIT_FLAG(pParms->CreateFlags, SHFL_CF_LOOKUP))
        {
            LogFlow(("SHFL_CF_LOOKUP\n"));
            /* Client just wants to know if the object exists. */
            switch (rc)
            {
                case VINF_SUCCESS:
                {
                    pParms->Info = info;
                    pParms->Result = SHFL_FILE_EXISTS;
                    break;
                }

                case VERR_FILE_NOT_FOUND:
                {
                    pParms->Result = SHFL_FILE_NOT_FOUND;
                    rc = VINF_SUCCESS;
                    break;
                }

                case VERR_PATH_NOT_FOUND:
                {
                    pParms->Result = SHFL_PATH_NOT_FOUND;
                    rc = VINF_SUCCESS;
                    break;
                }
            }
        }
        else if (rc == VINF_SUCCESS)
        {
            /* File object exists. */
            pParms->Result = SHFL_FILE_EXISTS;

            /* Mark it as a directory in case the caller didn't. */
            if (BIT_FLAG(info.Attr.fMode, RTFS_DOS_DIRECTORY))
            {
                pParms->CreateFlags |= SHFL_CF_DIRECTORY;
            }

            if (BIT_FLAG(pParms->CreateFlags, SHFL_CF_OPEN_TARGET_DIRECTORY))
            {
                pParms->Info = info;
                vbsfStripLastComponent (pszFullPath, cbFullPathRoot);
                rc = vbsfOpenExisting (pszFullPath, pParms);
            }
            else
            {
                if (    BIT_FLAG(pParms->CreateFlags, SHFL_CF_DIRECTORY)
                    && !BIT_FLAG(info.Attr.fMode, RTFS_DOS_DIRECTORY))
                {
                    /* Caller wanted a directory but the existing object is not a directory.
                     * Do not open the object then.
                     */
                    ; /* do nothing */
                }
                else
                {
                    switch (BIT_FLAG(pParms->CreateFlags, SHFL_CF_ACT_MASK_IF_EXISTS))
                    {
                        case SHFL_CF_ACT_OPEN_IF_EXISTS:
                        {
                            pParms->Info = info;
                            rc = vbsfOpenExisting (pszFullPath, pParms);
                            break;
                        }

                        case SHFL_CF_ACT_FAIL_IF_EXISTS:
                        {
                            /* NIL handle value will tell client that object was not opened.
                             * Just copy information about the object.
                             */
                            pParms->Info = info;
                            break;
                        }

                        case SHFL_CF_ACT_REPLACE_IF_EXISTS:
                        {
                            rc = vbsfOpenReplace (pszFullPath, pParms, true, &info);
                            break;
                        }

                        case SHFL_CF_ACT_OVERWRITE_IF_EXISTS:
                        {
                            rc = vbsfOpenReplace (pszFullPath, pParms, false, &info);
                            pParms->Info = info;
                            break;
                        }

                        default:
                        {
                            rc = VERR_INVALID_PARAMETER;
                        }
                    }
                }
            }
        }
        else 
        if (rc == VERR_FILE_NOT_FOUND)
        {
            LogFlow(("pParms->CreateFlags = %x\n", pParms->CreateFlags));

            rc = VINF_SUCCESS;

            /* File object does not exist. */
            pParms->Result = SHFL_FILE_NOT_FOUND;

            if (BIT_FLAG(pParms->CreateFlags, SHFL_CF_OPEN_TARGET_DIRECTORY))
            {
                switch (BIT_FLAG(pParms->CreateFlags, SHFL_CF_ACT_MASK_IF_NEW))
                {
                    case SHFL_CF_ACT_CREATE_IF_NEW:
                    {
                        rc = vbsfOpenCreate (pszFullPath, pParms);
                        break;
                    }

                    case SHFL_CF_ACT_FAIL_IF_NEW:
                    {
                        /* NIL handle value will tell client that object was not created. */
                        pParms->Result = SHFL_PATH_NOT_FOUND;
                        break;
                    }

                    default:
                    {
                        rc = VERR_INVALID_PARAMETER;
                        break;
                    }
                }
            }
            else
            {
                switch (BIT_FLAG(pParms->CreateFlags, SHFL_CF_ACT_MASK_IF_NEW))
                {
                    case SHFL_CF_ACT_CREATE_IF_NEW:
                    {
                        rc = vbsfOpenCreate (pszFullPath, pParms);
                        break;
                    }

                    case SHFL_CF_ACT_FAIL_IF_NEW:
                    {
                        /* NIL handle value will tell client that object was not created. */
                        break;
                    }

                    default:
                    {
                        rc = VERR_INVALID_PARAMETER;
                    }
                }
            }
        }
        else
        if (rc == VERR_PATH_NOT_FOUND)
        {
            rc = VINF_SUCCESS;
 
            pParms->Result = SHFL_PATH_NOT_FOUND;
        }

        if (rc == VINF_SUCCESS && pParms->Handle != SHFL_HANDLE_NIL)
        {
            uint32_t bufsize = sizeof(pParms->Info);

            rc = vbsfQueryFileInfo(pClient, root, pParms->Handle, SHFL_INFO_GET|SHFL_INFO_FILE, &bufsize, (uint8_t *)&pParms->Info);
            AssertRC(rc);
        }

        /* free the path string */
        vbsfFreeFullPath(pszFullPath);
    }

    Log(("vbsfCreate: handle = %RX64 rc = %Vrc result=%x\n", (uint64_t)pParms->Handle, rc, pParms->Result));

    return rc;
}

int vbsfClose (SHFLCLIENTDATA *pClient, SHFLROOT root, SHFLHANDLE Handle)
{
    int rc = VINF_SUCCESS;

    LogFlow(("vbsfClose: pClient = %p, Handle = %RX64\n",
             pClient, Handle));

    SHFLFILEHANDLE *pHandle = (SHFLFILEHANDLE *)vbsfQueryHandle(Handle, SHFL_HF_TYPE_DIR|SHFL_HF_TYPE_FILE);
    Assert(pHandle);
    if (!pHandle)
        return VERR_INVALID_HANDLE;

    switch (ShflHandleType (&pHandle->Header))
    {
        case SHFL_HF_TYPE_DIR:
        {
            rc = vbsfCloseDir (pHandle);
            break;
        }
        case SHFL_HF_TYPE_FILE:
        {
            rc = vbsfCloseFile (pHandle);
            break;
        }
    }
    vbsfFreeHandle(Handle);

    Log(("vbsfClose: rc = %Rrc\n", rc));

    return rc;
}

int vbsfRead  (SHFLCLIENTDATA *pClient, SHFLROOT root, SHFLHANDLE Handle, uint64_t offset, uint32_t *pcbBuffer, uint8_t *pBuffer)
{
    SHFLFILEHANDLE *pHandle = (SHFLFILEHANDLE *)vbsfQueryHandle(Handle, SHFL_HF_TYPE_FILE);
    size_t count = 0;
    int rc;

    if (pHandle == 0 || pcbBuffer == 0 || pBuffer == 0)
    {
        AssertFailed();
        return VERR_INVALID_PARAMETER;
    }

    Log(("vbsfRead %RX64 offset %RX64 bytes %x\n", Handle, offset, *pcbBuffer));

    if (*pcbBuffer == 0)
        return VINF_SUCCESS; /* @todo correct? */


    rc = RTFileSeek(pHandle->file.Handle, offset, RTFILE_SEEK_BEGIN, NULL);
    if (rc != VINF_SUCCESS)
    {
        AssertRC(rc);
        return rc;
    }

    rc = RTFileRead(pHandle->file.Handle, pBuffer, *pcbBuffer, &count);
    *pcbBuffer = (uint32_t)count;
    Log(("RTFileRead returned %Vrc bytes read %x\n", rc, count));
    return rc;
}

int vbsfWrite (SHFLCLIENTDATA *pClient, SHFLROOT root, SHFLHANDLE Handle, uint64_t offset, uint32_t *pcbBuffer, uint8_t *pBuffer)
{
    SHFLFILEHANDLE *pHandle = (SHFLFILEHANDLE *)vbsfQueryHandle(Handle, SHFL_HF_TYPE_FILE);
    size_t count = 0;
    int rc;

    if (pHandle == 0 || pcbBuffer == 0 || pBuffer == 0)
    {
        AssertFailed();
        return VERR_INVALID_PARAMETER;
    }

    Log(("vbsfWrite %RX64 offset %RX64 bytes %x\n", Handle, offset, *pcbBuffer));

    if (*pcbBuffer == 0)
        return VINF_SUCCESS; /** @todo correct? */

    rc = RTFileSeek(pHandle->file.Handle, offset, RTFILE_SEEK_BEGIN, NULL);
    if (rc != VINF_SUCCESS)
    {
        AssertRC(rc);
        return rc;
    }

    rc = RTFileWrite(pHandle->file.Handle, pBuffer, *pcbBuffer, &count);
    *pcbBuffer = (uint32_t)count;
    Log(("RTFileWrite returned %Vrc bytes written %x\n", rc, count));
    return rc;
}


int vbsfFlush(SHFLCLIENTDATA *pClient, SHFLROOT root, SHFLHANDLE Handle)
{
    SHFLFILEHANDLE *pHandle = (SHFLFILEHANDLE *)vbsfQueryHandle(Handle, SHFL_HF_TYPE_FILE);
    int rc = VINF_SUCCESS;

    if (pHandle == 0)
    {
        AssertFailed();
        return VERR_INVALID_HANDLE;
    }

    Log(("vbsfFlush %RX64\n", Handle));
    rc = RTFileFlush(pHandle->file.Handle);
    AssertRC(rc);
    return rc;
}

int vbsfDirList(SHFLCLIENTDATA *pClient, SHFLROOT root, SHFLHANDLE Handle, SHFLSTRING *pPath, uint32_t flags, uint32_t *pcbBuffer, uint8_t *pBuffer,
                uint32_t *pIndex, uint32_t *pcFiles)
{
    SHFLFILEHANDLE *pHandle = (SHFLFILEHANDLE *)vbsfQueryHandle(Handle, SHFL_HF_TYPE_DIR);
    PRTDIRENTRYEX  pDirEntry = 0, pDirEntryOrg;
    uint32_t       cbDirEntry, cbBufferOrg;
    int            rc = VINF_SUCCESS;
    PSHFLDIRINFO   pSFDEntry;
    PRTUCS2        puszString;
    PRTDIR         DirHandle;
    bool           fUtf8;

    fUtf8 = BIT_FLAG(pClient->fu32Flags, SHFL_CF_UTF8) != 0;

    if (pHandle == 0 || pcbBuffer == 0 || pBuffer == 0)
    {
        AssertFailed();
        return VERR_INVALID_PARAMETER;
    }
    Assert(pIndex && *pIndex == 0);
    DirHandle = pHandle->dir.Handle;

    cbDirEntry = 4096;
    pDirEntryOrg = pDirEntry  = (PRTDIRENTRYEX)RTMemAlloc(cbDirEntry);
    if (pDirEntry == 0)
    {
        AssertFailed();
        return VERR_NO_MEMORY;
    }

    cbBufferOrg = *pcbBuffer;
    *pcbBuffer  = 0;
    pSFDEntry   = (PSHFLDIRINFO)pBuffer;

    *pIndex = 1; /* not yet complete */
    *pcFiles = 0;

    if (pPath)
    {
        if (pHandle->dir.SearchHandle == 0)
        {
            /* Build a host full path for the given path
             * and convert ucs2 to utf8 if necessary.
             */
            char *pszFullPath = NULL;

            Assert(pHandle->dir.pLastValidEntry == 0);

            rc = vbsfBuildFullPath (pClient, root, pPath, pPath->u16Size, &pszFullPath, NULL, true);

            if (VBOX_SUCCESS (rc))
            {
                rc = RTDirOpenFiltered (&pHandle->dir.SearchHandle, pszFullPath, RTDIRFILTER_WINNT);

                /* free the path string */
                vbsfFreeFullPath(pszFullPath);

                if (VBOX_FAILURE (rc))
                    goto end;
            }
            else
                goto end;
        }
        Assert(pHandle->dir.SearchHandle);
        DirHandle = pHandle->dir.SearchHandle;
    }

    while(cbBufferOrg)
    {
        uint32_t cbDirEntrySize = cbDirEntry;
        uint32_t cbNeeded;

        /* Do we still have a valid last entry for the active search? If so, then return it here */
        if (pHandle->dir.pLastValidEntry)
        {
            pDirEntry = pHandle->dir.pLastValidEntry;
        }
        else
        {
            pDirEntry = pDirEntryOrg;

            rc = RTDirReadEx(DirHandle, pDirEntry, &cbDirEntrySize, RTFSOBJATTRADD_NOTHING);
            if (rc == VERR_NO_MORE_FILES)
            {
                *pIndex = 0; /* listing completed */
                break;
            }

            if (VINF_SUCCESS != rc && rc != VWRN_NO_DIRENT_INFO)
            {
                AssertFailed();
                if (rc != VERR_NO_TRANSLATION)
                    break;
                else
                    continue;
            }
        }

        cbNeeded = RT_OFFSETOF (SHFLDIRINFO, name.String);
        if (fUtf8)
            cbNeeded += pDirEntry->cbName + 1;
        else
            /* Overestimating, but that's ok */
            cbNeeded += (pDirEntry->cbName + 1) * 2;

        if (cbBufferOrg < cbNeeded)
        {
            /* No room, so save this directory entry, or else it's lost forever */
            pHandle->dir.pLastValidEntry = pDirEntry;

            if (*pcFiles == 0)
            {
                AssertFailed();
                return VINF_BUFFER_OVERFLOW;    /* Return directly and don't free pDirEntry */
            }
            return VINF_SUCCESS;    /* Return directly and don't free pDirEntry */
        }

        pSFDEntry->Info = pDirEntry->Info;
        pSFDEntry->cucShortName = 0;

        if (fUtf8)
        {
            void *src, *dst;

            src = &pDirEntry->szName[0];
            dst = &pSFDEntry->name.String.utf8[0];

            memcpy (dst, src, pDirEntry->cbName + 1);

            pSFDEntry->name.u16Size = pDirEntry->cbName + 1;
            pSFDEntry->name.u16Length = pDirEntry->cbName;
        }
        else
        {
            pSFDEntry->name.String.ucs2[0] = 0;
            puszString = pSFDEntry->name.String.ucs2;
            int rc2 = RTStrUtf8ToUcs2Ex(&puszString, pDirEntry->cbName+1, pDirEntry->szName);
            AssertRC(rc2);

            pSFDEntry->name.u16Length = RTStrUcs2Len (pSFDEntry->name.String.ucs2) * 2;
            pSFDEntry->name.u16Size = pSFDEntry->name.u16Length + 2;

            Log(("SHFL: File name size %d\n", pSFDEntry->name.u16Size));
            Log(("SHFL: File name %ls\n", &pSFDEntry->name.String.ucs2));

            // adjust cbNeeded (it was overestimated before)
            cbNeeded = RT_OFFSETOF (SHFLDIRINFO, name.String) + pSFDEntry->name.u16Size;
        }

        pSFDEntry   = (PSHFLDIRINFO)((uintptr_t)pSFDEntry + cbNeeded);
        *pcbBuffer += cbNeeded;
        cbBufferOrg-= cbNeeded;

        *pcFiles   += 1;

        /* Free the saved last entry, that we've just returned */
        if (pHandle->dir.pLastValidEntry)
        {
            RTMemFree(pHandle->dir.pLastValidEntry);
            pHandle->dir.pLastValidEntry = NULL;
        }

        if (flags & SHFL_LIST_RETURN_ONE)
            break; /* we're done */
    }
    Assert(rc != VINF_SUCCESS || *pcbBuffer > 0);

end:
    if (pDirEntry)
        RTMemFree(pDirEntry);

    return rc;
}

int vbsfQueryFileInfo(SHFLCLIENTDATA *pClient, SHFLROOT root, SHFLHANDLE Handle, uint32_t flags, uint32_t *pcbBuffer, uint8_t *pBuffer)
{
    SHFLFILEHANDLE *pHandle = (SHFLFILEHANDLE *)vbsfQueryHandle(Handle, SHFL_HF_TYPE_DIR|SHFL_HF_TYPE_FILE);
    int            rc = VINF_SUCCESS;
    RTFSOBJINFO   *pObjInfo = (RTFSOBJINFO *)pBuffer;


    if (pHandle == 0 || pcbBuffer == 0 || pObjInfo == 0 || *pcbBuffer < sizeof(RTFSOBJINFO))
    {
        AssertFailed();
        return VERR_INVALID_PARAMETER;
    }

    /* @todo other options */
    Assert(flags == (SHFL_INFO_GET|SHFL_INFO_FILE));

    *pcbBuffer  = 0;

    if (pHandle->Header.u32Flags & SHFL_HF_TYPE_DIR)
    {
        rc = RTDirQueryInfo(pHandle->dir.Handle, pObjInfo, RTFSOBJATTRADD_NOTHING);
    }
    else
    {
        rc = RTFileQueryInfo(pHandle->file.Handle, pObjInfo, RTFSOBJATTRADD_NOTHING);
    }
    if (rc == VINF_SUCCESS)
    {
        *pcbBuffer = sizeof(RTFSOBJINFO);
    }
    else
        AssertFailed();

    return rc;
}

int vbsfSetFileInfo(SHFLCLIENTDATA *pClient, SHFLROOT root, SHFLHANDLE Handle, uint32_t flags, uint32_t *pcbBuffer, uint8_t *pBuffer)
{
    SHFLFILEHANDLE *pHandle = (SHFLFILEHANDLE *)vbsfQueryHandle(Handle, SHFL_HF_TYPE_DIR|SHFL_HF_TYPE_FILE);
    int             rc = VINF_SUCCESS;
    RTFSOBJINFO    *pSFDEntry;

    if (pHandle == 0 || pcbBuffer == 0 || pBuffer == 0 || *pcbBuffer < sizeof(RTFSOBJINFO))
    {
        AssertFailed();
        return VERR_INVALID_PARAMETER;
    }

    *pcbBuffer  = 0;
    pSFDEntry   = (RTFSOBJINFO *)pBuffer;

    Assert(flags == (SHFL_INFO_SET | SHFL_INFO_FILE));

    /* Change only the time values that are not zero */
    if (pHandle->Header.u32Flags & SHFL_HF_TYPE_DIR)
    {
        rc = RTDirSetTimes(pHandle->dir.Handle,
                            (RTTimeSpecGetNano(&pSFDEntry->AccessTime)) ?       &pSFDEntry->AccessTime : NULL,
                            (RTTimeSpecGetNano(&pSFDEntry->ModificationTime)) ? &pSFDEntry->ModificationTime: NULL,
                            (RTTimeSpecGetNano(&pSFDEntry->ChangeTime)) ?       &pSFDEntry->ChangeTime: NULL,
                            (RTTimeSpecGetNano(&pSFDEntry->BirthTime)) ?        &pSFDEntry->BirthTime: NULL
                            );
    }
    else
    {
        rc = RTFileSetTimes(pHandle->file.Handle,
                            (RTTimeSpecGetNano(&pSFDEntry->AccessTime)) ?       &pSFDEntry->AccessTime : NULL,
                            (RTTimeSpecGetNano(&pSFDEntry->ModificationTime)) ? &pSFDEntry->ModificationTime: NULL,
                            (RTTimeSpecGetNano(&pSFDEntry->ChangeTime)) ?       &pSFDEntry->ChangeTime: NULL,
                            (RTTimeSpecGetNano(&pSFDEntry->BirthTime)) ?        &pSFDEntry->BirthTime: NULL
                            );
    }
    if (rc != VINF_SUCCESS)
    {
        Log(("RTFileSetTimes failed with %Vrc\n", rc));
        Log(("AccessTime       %VX64\n", RTTimeSpecGetNano(&pSFDEntry->AccessTime)));
        Log(("ModificationTime %VX64\n", RTTimeSpecGetNano(&pSFDEntry->ModificationTime)));
        Log(("ChangeTime       %VX64\n", RTTimeSpecGetNano(&pSFDEntry->ChangeTime)));
        Log(("BirthTime        %VX64\n", RTTimeSpecGetNano(&pSFDEntry->AccessTime)));
        /* temporary hack */
        rc = VINF_SUCCESS;
    }

    if (pHandle->Header.u32Flags & SHFL_HF_TYPE_FILE)
    {
        /* Change file attributes if necessary */
        if (pSFDEntry->Attr.fMode)
        {
            rc = RTFileSetMode((RTFILE)pHandle->file.Handle, pSFDEntry->Attr.fMode);
            if (rc != VINF_SUCCESS)
            {
                Log(("RTFileSetMode %x failed with %Vrc\n", pSFDEntry->Attr.fMode, rc));
                /* silent failure, because this tends to fail with e.g. windows guest & linux host */
                rc = VINF_SUCCESS;
            }
        }
    }

    if (rc == VINF_SUCCESS)
    {
        uint32_t bufsize = sizeof(*pSFDEntry);

        rc = vbsfQueryFileInfo(pClient, root, Handle, SHFL_INFO_GET|SHFL_INFO_FILE, &bufsize, (uint8_t *)pSFDEntry);
        if (rc == VINF_SUCCESS)
        {
            *pcbBuffer = sizeof(RTFSOBJINFO);
        }
        else
            AssertFailed();
    }

    return rc;
}


int vbsfSetEndOfFile(SHFLCLIENTDATA *pClient, SHFLROOT root, SHFLHANDLE Handle, uint32_t flags, uint32_t *pcbBuffer, uint8_t *pBuffer)
{
    SHFLFILEHANDLE *pHandle = (SHFLFILEHANDLE *)vbsfQueryHandle(Handle, SHFL_HF_TYPE_FILE);
    int             rc = VINF_SUCCESS;
    RTFSOBJINFO    *pSFDEntry;

    if (pHandle == 0 || pcbBuffer == 0 || pBuffer == 0 || *pcbBuffer < sizeof(RTFSOBJINFO))
    {
        AssertFailed();
        return VERR_INVALID_PARAMETER;
    }

    *pcbBuffer  = 0;
    pSFDEntry   = (RTFSOBJINFO *)pBuffer;

    if (flags & SHFL_INFO_SIZE)
    {
        rc = RTFileSetSize(pHandle->file.Handle, pSFDEntry->cbObject);
        if (rc != VINF_SUCCESS)
            AssertFailed();
    }
    else
        AssertFailed();

    if (rc == VINF_SUCCESS)
    {
        RTFSOBJINFO fileinfo;

        /* Query the new object info and return it */
        rc = RTFileQueryInfo(pHandle->file.Handle, &fileinfo, RTFSOBJATTRADD_NOTHING);
        if (rc == VINF_SUCCESS)
        {
            *pSFDEntry = fileinfo;
            *pcbBuffer = sizeof(RTFSOBJINFO);
        }
        else
            AssertFailed();
    }

    return rc;
}

int vbsfQueryVolumeInfo(SHFLCLIENTDATA *pClient, SHFLROOT root, uint32_t flags, uint32_t *pcbBuffer, uint8_t *pBuffer)
{
    int            rc = VINF_SUCCESS;
    SHFLVOLINFO   *pSFDEntry;
    char          *pszFullPath = NULL;
    SHFLSTRING     dummy;

    if (pcbBuffer == 0 || pBuffer == 0 || *pcbBuffer < sizeof(SHFLVOLINFO))
    {
        AssertFailed();
        return VERR_INVALID_PARAMETER;
    }

    /* @todo other options */
    Assert(flags == (SHFL_INFO_GET|SHFL_INFO_VOLUME));

    *pcbBuffer  = 0;
    pSFDEntry   = (PSHFLVOLINFO)pBuffer;

    ShflStringInitBuffer(&dummy, sizeof(dummy));
    rc = vbsfBuildFullPath (pClient, root, &dummy, 0, &pszFullPath, NULL);

    if (VBOX_SUCCESS (rc))
    {
        rc = RTFsQuerySizes(pszFullPath, &pSFDEntry->ullTotalAllocationBytes, &pSFDEntry->ullAvailableAllocationBytes, &pSFDEntry->ulBytesPerAllocationUnit, &pSFDEntry->ulBytesPerSector);
        if (rc != VINF_SUCCESS)
            goto exit;

        rc = RTFsQuerySerial(pszFullPath, &pSFDEntry->ulSerial);
        if (rc != VINF_SUCCESS)
            goto exit;

        rc = RTFsQueryProperties(pszFullPath, &pSFDEntry->fsProperties);
        if (rc != VINF_SUCCESS)
            goto exit;

        *pcbBuffer = sizeof(SHFLVOLINFO);
    }
    else AssertFailed();

exit:
    AssertMsg(rc == VINF_SUCCESS, ("failure: rc = %Vrc\n", rc));
    /* free the path string */
    vbsfFreeFullPath(pszFullPath);
    return rc;
}

int vbsfQueryFSInfo(SHFLCLIENTDATA *pClient, SHFLROOT root, SHFLHANDLE Handle, uint32_t flags, uint32_t *pcbBuffer, uint8_t *pBuffer)
{
    if (pcbBuffer == 0 || pBuffer == 0)
    {
        AssertFailed();
        return VERR_INVALID_PARAMETER;
    }

    if (flags & SHFL_INFO_FILE)
        return vbsfQueryFileInfo(pClient, root, Handle, flags, pcbBuffer, pBuffer);

    if (flags & SHFL_INFO_VOLUME)
        return vbsfQueryVolumeInfo(pClient, root, flags, pcbBuffer, pBuffer);

    AssertFailed();
    return VERR_INVALID_PARAMETER;
}

int vbsfSetFSInfo(SHFLCLIENTDATA *pClient, SHFLROOT root, SHFLHANDLE Handle, uint32_t flags, uint32_t *pcbBuffer, uint8_t *pBuffer)
{
    SHFLFILEHANDLE *pHandle = (SHFLFILEHANDLE *)vbsfQueryHandle(Handle, SHFL_HF_TYPE_DIR|SHFL_HF_TYPE_FILE|SHFL_HF_TYPE_VOLUME);

    if (pHandle == 0 || pcbBuffer == 0 || pBuffer == 0)
    {
        AssertFailed();
        return VERR_INVALID_PARAMETER;
    }
    if (flags & SHFL_INFO_FILE)
        return vbsfSetFileInfo(pClient, root, Handle, flags, pcbBuffer, pBuffer);

    if (flags & SHFL_INFO_SIZE)
        return vbsfSetEndOfFile(pClient, root, Handle, flags, pcbBuffer, pBuffer);

//    if (flags & SHFL_INFO_VOLUME)
//        return vbsfVolumeInfo(pClient, root, Handle, flags, pcbBuffer, pBuffer);
    AssertFailed();
    return VERR_INVALID_PARAMETER;
}

int vbsfLock(SHFLCLIENTDATA *pClient, SHFLROOT root, SHFLHANDLE Handle, uint64_t offset, uint64_t length, uint32_t flags)
{
    SHFLFILEHANDLE *pHandle = (SHFLFILEHANDLE *)vbsfQueryHandle(Handle, SHFL_HF_TYPE_FILE);
    uint32_t        fRTLock = 0;
    int             rc;

    Assert((flags & SHFL_LOCK_MODE_MASK) != SHFL_LOCK_CANCEL);

    if (pHandle == 0)
    {
        AssertFailed();
        return VERR_INVALID_HANDLE;
    }
    if (   ((flags & SHFL_LOCK_MODE_MASK) == SHFL_LOCK_CANCEL)
        || (flags & SHFL_LOCK_ENTIRE)
       )
    {
        AssertFailed();
        return VERR_INVALID_PARAMETER;
    }

    /* Lock type */
    switch(flags & SHFL_LOCK_MODE_MASK)
    {
    case SHFL_LOCK_SHARED:
        fRTLock = RTFILE_LOCK_READ;
        break;

    case SHFL_LOCK_EXCLUSIVE:
        fRTLock = RTFILE_LOCK_READ | RTFILE_LOCK_WRITE;
        break;

    default:
        AssertFailed();
        return VERR_INVALID_PARAMETER;
    }

    /* Lock wait type */
    if (flags & SHFL_LOCK_WAIT)
        fRTLock |= RTFILE_LOCK_WAIT;
    else
        fRTLock |= RTFILE_LOCK_IMMEDIATELY;

#ifdef RT_OS_WINDOWS
    rc = RTFileLock(pHandle->file.Handle, fRTLock, offset, length);
    if (rc != VINF_SUCCESS)
        Log(("RTFileLock %RTfile %RX64 %RX64 failed with %Rrc\n", pHandle->file.Handle, offset, length, rc));
#else
    Log(("vbsfLock: Pretend success handle=%x\n", Handle));
    rc = VINF_SUCCESS;
#endif
    return rc;
}

int vbsfUnlock(SHFLCLIENTDATA *pClient, SHFLROOT root, SHFLHANDLE Handle, uint64_t offset, uint64_t length, uint32_t flags)
{
    SHFLFILEHANDLE *pHandle = (SHFLFILEHANDLE *)vbsfQueryHandle(Handle, SHFL_HF_TYPE_FILE);
    int             rc;

    Assert((flags & SHFL_LOCK_MODE_MASK) == SHFL_LOCK_CANCEL);

    if (pHandle == 0)
    {
        return VERR_INVALID_HANDLE;
    }
    if (   ((flags & SHFL_LOCK_MODE_MASK) != SHFL_LOCK_CANCEL)
        || (flags & SHFL_LOCK_ENTIRE)
       )
    {
       return VERR_INVALID_PARAMETER;
    }

#ifdef RT_OS_WINDOWS
    rc = RTFileUnlock(pHandle->file.Handle, offset, length);
    if (rc != VINF_SUCCESS)
        Log(("RTFileUnlock %RTfile %RX64 %RTX64 failed with %Rrc\n", pHandle->file.Handle, offset, length, rc));
#else
    Log(("vbsfUnlock: Pretend success handle=%x\n", Handle));
    rc = VINF_SUCCESS;
#endif

    return rc;
}


int vbsfRemove(SHFLCLIENTDATA *pClient, SHFLROOT root, SHFLSTRING *pPath, uint32_t cbPath, uint32_t flags)
{
    int rc = VINF_SUCCESS;

    /* Validate input */
    if (   flags & ~(SHFL_REMOVE_FILE|SHFL_REMOVE_DIR)
        || cbPath == 0
        || pPath == 0)
    {
        AssertFailed();
        return VERR_INVALID_PARAMETER;
    }

    /* Build a host full path for the given path
     * and convert ucs2 to utf8 if necessary.
     */
    char *pszFullPath = NULL;

    rc = vbsfBuildFullPath (pClient, root, pPath, cbPath, &pszFullPath, NULL);

    if (VBOX_SUCCESS (rc))
    {
        if (flags & SHFL_REMOVE_FILE)
            rc = RTFileDelete(pszFullPath);
        else
            rc = RTDirRemove(pszFullPath);

#ifndef DEBUG_dmik
        Assert(rc == VINF_SUCCESS || rc == VERR_DIR_NOT_EMPTY);
#endif
        /* free the path string */
        vbsfFreeFullPath(pszFullPath);
    }
    return rc;
}


int vbsfRename(SHFLCLIENTDATA *pClient, SHFLROOT root, SHFLSTRING *pSrc, SHFLSTRING *pDest, uint32_t flags)
{
    int rc = VINF_SUCCESS;

    /* Validate input */
    if (   flags & ~(SHFL_REMOVE_FILE|SHFL_REMOVE_DIR|SHFL_RENAME_REPLACE_IF_EXISTS)
        || pSrc == 0
        || pDest == 0)
    {
        AssertFailed();
        return VERR_INVALID_PARAMETER;
    }

    /* Build a host full path for the given path
     * and convert ucs2 to utf8 if necessary.
     */
    char *pszFullPathSrc = NULL;
    char *pszFullPathDest = NULL;

    rc = vbsfBuildFullPath (pClient, root, pSrc, pSrc->u16Size, &pszFullPathSrc, NULL);
    if (rc != VINF_SUCCESS)
        return rc;

    rc = vbsfBuildFullPath (pClient, root, pDest, pDest->u16Size, &pszFullPathDest, NULL);
    if (VBOX_SUCCESS (rc))
    {
        Log(("Rename %s to %s\n", pszFullPathSrc, pszFullPathDest));
        if (flags & SHFL_RENAME_FILE)
        {
            rc = RTFileMove(pszFullPathSrc, pszFullPathDest, (flags & SHFL_RENAME_REPLACE_IF_EXISTS) ? RTFILEMOVE_FLAGS_REPLACE : 0);
        }
        else
        {
            /* NT ignores the REPLACE flag and simply return and already exists error. */
            rc = RTDirRename(pszFullPathSrc, pszFullPathDest, (flags & SHFL_RENAME_REPLACE_IF_EXISTS) ? RTPATHRENAME_FLAGS_REPLACE : 0);
        }

#ifndef DEBUG_dmik
        AssertRC(rc);
#endif
        /* free the path string */
        vbsfFreeFullPath(pszFullPathDest);
    }
    /* free the path string */
    vbsfFreeFullPath(pszFullPathSrc);
    return rc;
}

/*
 * Clean up our mess by freeing all handles that are still valid.
 *
 */
int vbsfDisconnect(SHFLCLIENTDATA *pClient)
{
    for (int i=0;i<SHFLHANDLE_MAX;i++)
    {
        SHFLFILEHANDLE *pHandle = (SHFLFILEHANDLE *)vbsfQueryHandle(i, SHFL_HF_TYPE_MASK);

        if (pHandle)
        {
            Log(("Open handle %08x\n", i));
            vbsfClose(pClient, SHFL_HANDLE_ROOT /* incorrect, but it's not important */, (SHFLHANDLE)i);
        }
    }
    return VINF_SUCCESS;
}
