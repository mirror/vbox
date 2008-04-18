/** @file
 * Shared Folders: Mappings support.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include "mappings.h"
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/string.h>

MAPPING FolderMapping[SHFL_MAX_MAPPINGS];


/*
 *
 * We are always executed from one specific HGCM thread. So thread safe.
 *
 */
int vbsfMappingsAdd (PSHFLSTRING pFolderName, PSHFLSTRING pMapName, uint32_t fWritable)
{
    int i;

    Assert(pFolderName && pMapName);

    Log(("vbsfMappingsAdd %ls\n", pMapName->String.ucs2));

    /* check for duplicates */
    for (i=0;i<SHFL_MAX_MAPPINGS;i++)
    {
        if (FolderMapping[i].fValid == true)
        {
            if (!RTUtf16LocaleICmp(FolderMapping[i].pMapName->String.ucs2, pMapName->String.ucs2))
            {
                AssertMsgFailed(("vbsfMappingsAdd: %ls mapping already exists!!\n", pMapName->String.ucs2));
                return VERR_ALREADY_EXISTS;
            }
        }
    }

    for (i=0;i<SHFL_MAX_MAPPINGS;i++)
    {
        if (FolderMapping[i].fValid == false)
        {
            FolderMapping[i].pFolderName = (PSHFLSTRING)RTMemAlloc(ShflStringSizeOfBuffer(pFolderName));
            Assert(FolderMapping[i].pFolderName);
            if (FolderMapping[i].pFolderName == NULL)
                return VERR_NO_MEMORY;

            FolderMapping[i].pFolderName->u16Length = pFolderName->u16Length;
            FolderMapping[i].pFolderName->u16Size   = pFolderName->u16Size;
            memcpy(FolderMapping[i].pFolderName->String.ucs2, pFolderName->String.ucs2, pFolderName->u16Size);

            FolderMapping[i].pMapName = (PSHFLSTRING)RTMemAlloc(ShflStringSizeOfBuffer(pMapName));
            Assert(FolderMapping[i].pMapName);
            if (FolderMapping[i].pMapName == NULL)
                return VERR_NO_MEMORY;

            FolderMapping[i].pMapName->u16Length = pMapName->u16Length;
            FolderMapping[i].pMapName->u16Size   = pMapName->u16Size;
            memcpy(FolderMapping[i].pMapName->String.ucs2, pMapName->String.ucs2, pMapName->u16Size);

            FolderMapping[i].fValid    = true;
            FolderMapping[i].cMappings = 0;
            FolderMapping[i].fWritable = fWritable;

            /* Check if the host file system is case sensitive */
            RTFSPROPERTIES prop;
            char *utf8Root, *asciiroot;

            int rc = RTUtf16ToUtf8(FolderMapping[i].pFolderName->String.ucs2, &utf8Root);
            AssertRC(rc);

            if (VBOX_SUCCESS(rc))
            {
                rc = RTStrUtf8ToCurrentCP(&asciiroot, utf8Root);
                if (VBOX_SUCCESS(rc))
                {
                    rc = RTFsQueryProperties(asciiroot, &prop);
                    AssertRC(rc);
                    RTStrFree(asciiroot);
                }
                RTStrFree(utf8Root);
            }
            FolderMapping[i].fHostCaseSensitive = VBOX_SUCCESS(rc) ? prop.fCaseSensitive : false;
            break;
        }
    }
    if (i == SHFL_MAX_MAPPINGS)
    {
        AssertMsgFailed(("vbsfMappingsAdd: no more room to add mapping %ls to %ls!!\n", pFolderName->String.ucs2, pMapName->String.ucs2));
        return VERR_TOO_MUCH_DATA;
    }

    Log(("vbsfMappingsAdd: added mapping %ls to %ls\n", pFolderName->String.ucs2, pMapName->String.ucs2));
    return VINF_SUCCESS;
}

int vbsfMappingsRemove (PSHFLSTRING pMapName)
{
    int i;

    Assert(pMapName);

    Log(("vbsfMappingsRemove %ls\n", pMapName->String.ucs2));
    for (i=0;i<SHFL_MAX_MAPPINGS;i++)
    {
        if (FolderMapping[i].fValid == true)
        {
            if (!RTUtf16LocaleICmp(FolderMapping[i].pMapName->String.ucs2, pMapName->String.ucs2))
            {
                if (FolderMapping[i].cMappings != 0)
                {
                    Log(("vbsfMappingsRemove: trying to remove active share %ls\n", pMapName->String.ucs2));
                    return VERR_PERMISSION_DENIED;
                }

                RTMemFree(FolderMapping[i].pFolderName);
                RTMemFree(FolderMapping[i].pMapName);
                FolderMapping[i].pFolderName = NULL;
                FolderMapping[i].pMapName    = NULL;
                FolderMapping[i].fValid      = false;
                break;
            }
        }
    }

    if (i == SHFL_MAX_MAPPINGS)
    {
        AssertMsgFailed(("vbsfMappingsRemove: mapping %ls not found!!!!\n", pMapName->String.ucs2));
        return VERR_FILE_NOT_FOUND;
    }
    Log(("vbsfMappingsRemove: mapping %ls removed\n", pMapName->String.ucs2));
    return VINF_SUCCESS;
}

PCRTUTF16 vbsfMappingsQueryHostRoot (SHFLROOT root, uint32_t *pcbRoot)
{
    if (root > SHFL_MAX_MAPPINGS)
    {
        AssertFailed();
        return NULL;
    }

    *pcbRoot = FolderMapping[root].pFolderName->u16Size;
    return &FolderMapping[root].pFolderName->String.ucs2[0];
}

bool vbsfIsGuestMappingCaseSensitive (SHFLROOT root)
{
    if (root > SHFL_MAX_MAPPINGS)
    {
        AssertFailed();
        return false;
    }

    return FolderMapping[root].fGuestCaseSensitive;
}

bool vbsfIsHostMappingCaseSensitive (SHFLROOT root)
{
    if (root > SHFL_MAX_MAPPINGS)
    {
        AssertFailed();
        return false;
    }

    return FolderMapping[root].fHostCaseSensitive;
}

int vbsfMappingsQuery (SHFLCLIENTDATA *pClient, SHFLMAPPING *pMappings, uint32_t *pcMappings)
{
    int rc = VINF_SUCCESS;
    uint32_t cMaxMappings = RT_MIN(*pcMappings, SHFL_MAX_MAPPINGS);

    LogFlow(("vbsfMappingsQuery: pClient = %p, pMappings = %p, pcMappings = %p, *pcMappings = %d\n",
             pClient, pMappings, pcMappings, *pcMappings));

    *pcMappings = 0;
    for (uint32_t i=0;i<cMaxMappings;i++)
    {
        if (FolderMapping[i].fValid == true)
        {
            pMappings[*pcMappings].u32Status = SHFL_MS_NEW;
            pMappings[*pcMappings].root = i;
            *pcMappings = *pcMappings + 1;
        }
    }

    LogFlow(("vbsfMappingsQuery: return rc = %Vrc\n", rc));

    return rc;
}

int vbsfMappingsQueryName (SHFLCLIENTDATA *pClient, SHFLROOT root, SHFLSTRING *pString)
{
    int rc = VINF_SUCCESS;

    LogFlow(("vbsfMappingsQuery: pClient = %p, root = %d, *pString = %p\n",
             pClient, root, pString));

    if (root >= SHFL_MAX_MAPPINGS)
        return VERR_INVALID_PARAMETER;

    if (BIT_FLAG(pClient->fu32Flags, SHFL_CF_UTF8))
    {
        /* not implemented */
        AssertFailed();
        return VERR_INVALID_PARAMETER;
    }

    if (FolderMapping[root].fValid == true)
    {
        pString->u16Length = FolderMapping[root].pMapName->u16Length;
        memcpy(pString->String.ucs2, FolderMapping[root].pMapName->String.ucs2, pString->u16Size);
    }
    else
        rc = VERR_FILE_NOT_FOUND;

    LogFlow(("vbsfMappingsQuery:Name return rc = %Vrc\n", rc));

    return rc;
}

int vbsfMappingsQueryWritable (SHFLCLIENTDATA *pClient, SHFLROOT root, bool *fWritable)
{
    int rc = VINF_SUCCESS;

    LogFlow(("vbsfMappingsQueryWritable: pClient = %p, root = %d\n",
             pClient, root));

    if (root >= SHFL_MAX_MAPPINGS)
        return VERR_INVALID_PARAMETER;

    if (FolderMapping[root].fValid == true)
        *fWritable = FolderMapping[root].fWritable;
    else
        rc = VERR_FILE_NOT_FOUND;

    LogFlow(("vbsfMappingsQuery:Writable return rc = %Vrc\n", rc));

    return rc;
}

static int vbsfQueryMappingIndex (PRTUTF16 utf16Name, size_t *pIndex)
{
    size_t i;

    for (i=0;i<SHFL_MAX_MAPPINGS;i++)
    {
        if (FolderMapping[i].fValid == true)
        {
            if (!RTUtf16LocaleICmp(FolderMapping[i].pMapName->String.ucs2, utf16Name))
            {
                *pIndex = i;
                return 0;
            }
        }
    }
    return -1;
}

int vbsfMapFolder (SHFLCLIENTDATA *pClient, PSHFLSTRING pszMapName, RTUTF16 delimiter, bool fCaseSensitive, SHFLROOT *pRoot)
{
    size_t index;

    if (BIT_FLAG(pClient->fu32Flags, SHFL_CF_UTF8))
    {
        Log(("vbsfMapFolder %s\n", pszMapName->String.utf8));
    }
    else
    {
        Log(("vbsfMapFolder %ls\n", pszMapName->String.ucs2));
    }

    if (pClient->PathDelimiter == 0)
    {
        pClient->PathDelimiter = delimiter;
    }
    else
    {
        Assert(delimiter == pClient->PathDelimiter);
    }

    if (BIT_FLAG(pClient->fu32Flags, SHFL_CF_UTF8))
    {
        int rc;
        PRTUTF16 utf16Name;

        rc = RTStrToUtf16 ((const char *) pszMapName->String.utf8, &utf16Name);
        if (VBOX_FAILURE (rc))
            return rc;

        rc = vbsfQueryMappingIndex (utf16Name, &index);
        RTUtf16Free (utf16Name);

        if (rc)
        {
            // AssertMsgFailed(("vbsfMapFolder: map %s not found!!\n",
            //                 pszMapName->String.utf8));
            return VERR_FILE_NOT_FOUND;
        }
    }
    else
    {
        if (vbsfQueryMappingIndex (pszMapName->String.ucs2, &index))
        {
            // AssertMsgFailed(("vbsfMapFolder: map %ls not found!!\n",
            //                  pszMapName->String.ucs2));
            return VERR_FILE_NOT_FOUND;
        }
    }

    FolderMapping[index].cMappings++;
    Assert(FolderMapping[index].cMappings == 1 || FolderMapping[index].fGuestCaseSensitive == fCaseSensitive);
    FolderMapping[index].fGuestCaseSensitive = fCaseSensitive;
    *pRoot = index;
    return VINF_SUCCESS;
}

int vbsfUnmapFolder (SHFLCLIENTDATA *pClient, SHFLROOT root)
{
    int rc = VINF_SUCCESS;

    if (root > SHFL_MAX_MAPPINGS)
    {
        AssertFailed();
        return VERR_FILE_NOT_FOUND;
    }

    Assert(FolderMapping[root].fValid == true && FolderMapping[root].cMappings > 0);
    if (FolderMapping[root].cMappings > 0)
        FolderMapping[root].cMappings--;

    Log(("vbsfUnmapFolder\n"));
    return rc;
}
