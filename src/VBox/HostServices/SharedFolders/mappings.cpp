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

/* Shared folders order in the saved state and in the FolderMapping can differ.
 * So a translation array of root handle is needed.
 */

static MAPPING FolderMapping[SHFL_MAX_MAPPINGS];
static SHFLROOT aIndexFromRoot[SHFL_MAX_MAPPINGS];

void vbsfMappingInit(void)
{
    int root;
    for (root = 0; root < RT_ELEMENTS(aIndexFromRoot); root++)
    {
        aIndexFromRoot[root] = ~0;
    }
}

int vbsfMappingLoaded (const MAPPING *pLoadedMapping, SHFLROOT root)
{
    /* Mapping loaded from the saved state with the index. Which means
     * the guest uses the iMapping as root handle for this folder.
     * Check whether there is the same mapping in FolderMapping and
     * update the aIndexFromRoot.
     *
     * Also update the mapping properties, which were lost: cMappings.
     */
    if (root >= SHFL_MAX_MAPPINGS)
    {
        return VERR_INVALID_PARAMETER;
    }

    int i;

    for (i = 0; i < RT_ELEMENTS(FolderMapping); i++)
    {
        MAPPING *pMapping = &FolderMapping[i];

        /* Equal? */
        if (   pLoadedMapping->fValid == pMapping->fValid
            && ShflStringSizeOfBuffer(pLoadedMapping->pMapName) == ShflStringSizeOfBuffer(pMapping->pMapName)
            && memcmp(pLoadedMapping->pMapName, pMapping->pMapName, ShflStringSizeOfBuffer(pMapping->pMapName)) == 0)
        {
            /* Actual index is i. */
            aIndexFromRoot[root] = i;

            /* Update the mapping properties. */
            pMapping->cMappings = pLoadedMapping->cMappings;

            return VINF_SUCCESS;
        }
    }

    return VERR_INVALID_PARAMETER;
}

MAPPING *vbsfMappingGetByRoot(SHFLROOT root)
{
    if (root < RT_ELEMENTS(aIndexFromRoot))
    {
        int iMapping = aIndexFromRoot[root];

        if (0 <= iMapping && iMapping < RT_ELEMENTS(FolderMapping))
        {
            return &FolderMapping[iMapping];
        }
    }

    return NULL;
}

static SHFLROOT vbsfMappingGetRootFromIndex(int iMapping)
{
    int root;
    for (root = 0; root < RT_ELEMENTS(aIndexFromRoot); root++)
    {
        if (iMapping == aIndexFromRoot[root])
        {
            return root;
        }
    }

    return ~0;
}

static MAPPING *vbsfMappingGetByName (PRTUTF16 utf16Name, SHFLROOT *pRoot)
{
    int i;

    for (i=0;i<SHFL_MAX_MAPPINGS;i++)
    {
        if (FolderMapping[i].fValid == true)
        {
            if (!RTUtf16LocaleICmp(FolderMapping[i].pMapName->String.ucs2, utf16Name))
            {
                SHFLROOT root = vbsfMappingGetRootFromIndex(i);
                
                if (root != ~0)
                {
                    if (pRoot)
                    {
                        *pRoot = root;
                    }
                    return &FolderMapping[i];
                }
                else
                {
                    AssertFailed();
                }
            }
        }
    }

    return NULL;
}

static void vbsfRootHandleAdd(int iMapping)
{
    int root;

    for (root = 0; root < RT_ELEMENTS(aIndexFromRoot); root++)
    {
        if (aIndexFromRoot[root] == ~0)
        {
            aIndexFromRoot[root] = iMapping;
            return;
        }
    }

    AssertFailed();
    return;
}

static void vbsfRootHandleRemove(int iMapping)
{
    int root;

    for (root = 0; root < RT_ELEMENTS(aIndexFromRoot); root++)
    {
        if (aIndexFromRoot[root] == iMapping)
        {
            aIndexFromRoot[root] = ~0;
            return;
        }
    }

    AssertFailed();
    return;
}



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
            FolderMapping[i].fWritable = !!fWritable;

            /* Check if the host file system is case sensitive */
            RTFSPROPERTIES prop;
            char *utf8Root, *asciiroot;

            int rc = RTUtf16ToUtf8(FolderMapping[i].pFolderName->String.ucs2, &utf8Root);
            AssertRC(rc);

            if (RT_SUCCESS(rc))
            {
                rc = RTStrUtf8ToCurrentCP(&asciiroot, utf8Root);
                if (RT_SUCCESS(rc))
                {
                    rc = RTFsQueryProperties(asciiroot, &prop);
                    AssertRC(rc);
                    RTStrFree(asciiroot);
                }
                RTStrFree(utf8Root);
            }
            FolderMapping[i].fHostCaseSensitive = RT_SUCCESS(rc) ? prop.fCaseSensitive : false;
            vbsfRootHandleAdd(i);
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
                vbsfRootHandleRemove(i);
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
    MAPPING *pFolderMapping = vbsfMappingGetByRoot(root);
    if (pFolderMapping == NULL)
    {
        AssertFailed();
        return NULL;
    }

    *pcbRoot = pFolderMapping->pFolderName->u16Size;
    return &pFolderMapping->pFolderName->String.ucs2[0];
}

bool vbsfIsGuestMappingCaseSensitive (SHFLROOT root)
{
    MAPPING *pFolderMapping = vbsfMappingGetByRoot(root);
    if (pFolderMapping == NULL)
    {
        AssertFailed();
        return false;
    }

    return pFolderMapping->fGuestCaseSensitive;
}

bool vbsfIsHostMappingCaseSensitive (SHFLROOT root)
{
    MAPPING *pFolderMapping = vbsfMappingGetByRoot(root);
    if (pFolderMapping == NULL)
    {
        AssertFailed();
        return false;
    }

    return pFolderMapping->fHostCaseSensitive;
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
        MAPPING *pFolderMapping = vbsfMappingGetByRoot(i);
        if (pFolderMapping != NULL && pFolderMapping->fValid == true)
        {
            pMappings[*pcMappings].u32Status = SHFL_MS_NEW;
            pMappings[*pcMappings].root = i;
            *pcMappings = *pcMappings + 1;
        }
    }

    LogFlow(("vbsfMappingsQuery: return rc = %Rrc\n", rc));

    return rc;
}

int vbsfMappingsQueryName (SHFLCLIENTDATA *pClient, SHFLROOT root, SHFLSTRING *pString)
{
    int rc = VINF_SUCCESS;

    LogFlow(("vbsfMappingsQuery: pClient = %p, root = %d, *pString = %p\n",
             pClient, root, pString));

    MAPPING *pFolderMapping = vbsfMappingGetByRoot(root);
    if (pFolderMapping == NULL)
    {
        return VERR_INVALID_PARAMETER;
    }

    if (BIT_FLAG(pClient->fu32Flags, SHFL_CF_UTF8))
    {
        /* not implemented */
        AssertFailed();
        return VERR_INVALID_PARAMETER;
    }

    if (pFolderMapping->fValid == true)
    {
        pString->u16Length = pFolderMapping->pMapName->u16Length;
        memcpy(pString->String.ucs2, pFolderMapping->pMapName->String.ucs2, pString->u16Size);
    }
    else
        rc = VERR_FILE_NOT_FOUND;

    LogFlow(("vbsfMappingsQuery:Name return rc = %Rrc\n", rc));

    return rc;
}

int vbsfMappingsQueryWritable (SHFLCLIENTDATA *pClient, SHFLROOT root, bool *fWritable)
{
    int rc = VINF_SUCCESS;

    LogFlow(("vbsfMappingsQueryWritable: pClient = %p, root = %d\n",
             pClient, root));

    MAPPING *pFolderMapping = vbsfMappingGetByRoot(root);
    if (pFolderMapping == NULL)
    {
        return VERR_INVALID_PARAMETER;
    }

    if (pFolderMapping->fValid == true)
        *fWritable = pFolderMapping->fWritable;
    else
        rc = VERR_FILE_NOT_FOUND;

    LogFlow(("vbsfMappingsQuery:Writable return rc = %Rrc\n", rc));

    return rc;
}

int vbsfMapFolder (SHFLCLIENTDATA *pClient, PSHFLSTRING pszMapName, RTUTF16 delimiter, bool fCaseSensitive, SHFLROOT *pRoot)
{
    MAPPING *pFolderMapping = NULL;

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
        if (RT_FAILURE (rc))
            return rc;

        pFolderMapping = vbsfMappingGetByName(utf16Name, pRoot);
        RTUtf16Free (utf16Name);
    }
    else
    {
        pFolderMapping = vbsfMappingGetByName(pszMapName->String.ucs2, pRoot);
    }

    if (!pFolderMapping)
    {
        return VERR_FILE_NOT_FOUND;
    }

    pFolderMapping->cMappings++;
    Assert(pFolderMapping->cMappings == 1 || pFolderMapping->fGuestCaseSensitive == fCaseSensitive);
    pFolderMapping->fGuestCaseSensitive = fCaseSensitive;
    return VINF_SUCCESS;
}

int vbsfUnmapFolder (SHFLCLIENTDATA *pClient, SHFLROOT root)
{
    int rc = VINF_SUCCESS;

    MAPPING *pFolderMapping = vbsfMappingGetByRoot(root);
    if (pFolderMapping == NULL)
    {
        AssertFailed();
        return VERR_FILE_NOT_FOUND;
    }

    Assert(pFolderMapping->fValid == true && pFolderMapping->cMappings > 0);
    if (pFolderMapping->cMappings > 0)
        pFolderMapping->cMappings--;

    Log(("vbsfUnmapFolder\n"));
    return rc;
}
