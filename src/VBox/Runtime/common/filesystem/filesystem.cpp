/* $Id$ */
/** @file
 * IPRT Filesystem API (Filesystem) - generic code.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DEFAULT
#include <iprt/types.h>
#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/filesystem.h>
#include <iprt/err.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#include <iprt/list.h>
#include "internal/filesystem.h"

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

/**
 * Medium descriptor.
 */
typedef struct RTFILESYSTEMMEDIUMINT
{
    /** Size of the medium in bytes. */
    uint64_t                cbMedium;
    /** Sector size. */
    uint64_t                cbSector;
    /** Read callback */
    PFNRTFILESYSTEMREAD     pfnRead;
    /** Write callback. */
    PFNRTFILESYSTEMWRITE    pfnWrite;
    /** Opaque user data. */
    void                    *pvUser;
} RTFILESYSTEMMEDIUMINT;
/** Pointer to a disk descriptor. */
typedef RTFILESYSTEMMEDIUMINT *PRTFILESYSTEMMEDIUMINT;
/** Pointer to a const descriptor. */
typedef const RTFILESYSTEMMEDIUMINT *PCRTFILESYSTEMMEDIUMINT;

/**
 * The internal filesystem object structure.
 */
typedef struct RTFILESYSTEMINT
{
    /** The filesytem object magic (RTFILESYSTEM_MAGIC). */
    uint32_t              u32Magic;
    /** Medium descriptor. */
    RTFILESYSTEMMEDIUMINT Medium;
    /** Filesystem format operations */
    PCRTFILESYSTEMFMTOPS  pcFsFmtOps;
    /** Filesystem format handle. */
    RTFILESYSTEMFMT       hFsFmt;
    /** Reference counter. */
    uint32_t volatile     cRefs;
} RTFILESYSTEMINT;
/** Pointer to an internal volume manager. */
typedef RTFILESYSTEMINT *PRTFILESYSTEMINT;

/*******************************************************************************
*  Global variables                                                            *
*******************************************************************************/
extern RTFILESYSTEMFMTOPS g_rtFilesystemFmtExt;

/**
 * Supported volume formats.
 */
static PCRTFILESYSTEMFMTOPS g_aFilesystemFmts[] =
{
    &g_rtFilesystemFmtExt
};

DECLHIDDEN(uint64_t) rtFilesystemMediumGetSize(RTFILESYSTEMMEDIUM hMedium)
{
    PRTFILESYSTEMMEDIUMINT pMedInt = hMedium;
    AssertPtrReturn(pMedInt, 0);

    return pMedInt->cbMedium;
}

DECLHIDDEN(int) rtFilesystemMediumRead(RTFILESYSTEMMEDIUM hMedium, uint64_t off,
                                       void *pvBuf, size_t cbRead)
{
    PRTFILESYSTEMMEDIUMINT pMedInt = hMedium;
    AssertPtrReturn(pMedInt, VERR_INVALID_HANDLE);

    return pMedInt->pfnRead(pMedInt->pvUser, off, pvBuf, cbRead);
}

DECLHIDDEN(int) rtFilesystemMediumWrite(RTFILESYSTEMMEDIUM hMedium, uint64_t off,
                                        const void *pvBuf, size_t cbWrite)
{
    PRTFILESYSTEMMEDIUMINT pMedInt = hMedium;
    AssertPtrReturn(pMedInt, VERR_INVALID_HANDLE);

    return pMedInt->pfnWrite(pMedInt->pvUser, off, pvBuf, cbWrite);
}

RTDECL(uint32_t) RTFilesystemRetain(RTFILESYSTEM hFs)
{
    PRTFILESYSTEMINT pThis = hFs;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->u32Magic == RTFILESYSTEM_MAGIC, UINT32_MAX);

    uint32_t cRefs = ASMAtomicIncU32(&pThis->cRefs);
    AssertMsg(cRefs > 1 && cRefs < _1M, ("%#x %p\n", cRefs, pThis));
    return cRefs;
}

/**
 * Destroys a volume manager handle.
 *
 * @param   pThis               The filesystem object to destroy.
 */
static void rtFilesystemDestroy(PRTFILESYSTEMINT pThis)
{
    if (pThis->hFsFmt != NIL_RTFILESYSTEMFMT)
    {
        AssertPtr(pThis->pcFsFmtOps);

        /* Let the backend do it's own cleanup first. */
        pThis->pcFsFmtOps->pfnClose(pThis->hFsFmt);
        pThis->hFsFmt = NIL_RTFILESYSTEMFMT;
    }

    pThis->Medium.cbMedium   = 0;
    pThis->Medium.pvUser   = NULL;
    pThis->Medium.pfnRead  = NULL;
    pThis->Medium.pfnWrite = NULL;
    pThis->u32Magic        = RTFILESYSTEM_MAGIC_DEAD;
    RTMemFree(pThis);
}

RTDECL(uint32_t) RTFilesystemRelease(RTFILESYSTEM hFs)
{
    PRTFILESYSTEMINT pThis = hFs;
    if (pThis == NIL_RTFILESYSTEM)
        return 0;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->u32Magic == RTFILESYSTEM_MAGIC, UINT32_MAX);

    uint32_t cRefs = ASMAtomicDecU32(&pThis->cRefs);
    AssertMsg(cRefs < _1M, ("%#x %p\n", cRefs, pThis));
    if (cRefs == 0)
        rtFilesystemDestroy(pThis);
    return cRefs;
}

RTDECL(int) RTFilesystemOpen(PRTFILESYSTEM phFs, PFNRTFILESYSTEMREAD pfnRead,
                             PFNRTFILESYSTEMWRITE pfnWrite, uint64_t cbMedium,
                             uint64_t cbSector, void *pvUser, uint32_t fFlags)
{
    int                   rc = VINF_SUCCESS;
    uint32_t              uScoreMax = RTFILESYSTEM_MATCH_SCORE_UNSUPPORTED;
    PCRTFILESYSTEMFMTOPS  pcFsFmtOpsMatch = NULL;
    PRTFILESYSTEMINT      pThis = NULL;
    AssertPtrReturn(phFs, VERR_INVALID_POINTER);
    AssertPtrReturn(pfnRead, VERR_INVALID_POINTER);

    pThis = (PRTFILESYSTEMINT)RTMemAllocZ(sizeof(RTFILESYSTEMINT));
    if (!pThis)
        return VERR_NO_MEMORY;

    pThis->u32Magic        = RTFILESYSTEM_MAGIC;
    pThis->Medium.cbMedium = cbMedium;
    pThis->Medium.cbSector = cbSector;
    pThis->Medium.pfnRead  = pfnRead;
    pThis->Medium.pfnWrite = pfnWrite;
    pThis->Medium.pvUser   = pvUser;
    pThis->cRefs           = 1;

    for (unsigned i = 0; i < RT_ELEMENTS(g_aFilesystemFmts); i++)
    {
        uint32_t uScore;
        PCRTFILESYSTEMFMTOPS pcFsFmtOps = g_aFilesystemFmts[i];

        rc = pcFsFmtOps->pfnProbe(&pThis->Medium, &uScore);
        if (   RT_SUCCESS(rc)
            && uScore > uScoreMax)
        {
            pcFsFmtOpsMatch = pcFsFmtOps;
            uScoreMax       = uScore;
        }
        else if (RT_FAILURE(rc))
            break;
    }

    if (RT_SUCCESS(rc))
    {
        if (uScoreMax > RTFILESYSTEM_MATCH_SCORE_UNSUPPORTED)
        {
            AssertPtr(pcFsFmtOpsMatch);

            /* Open the format. */
            rc = pcFsFmtOpsMatch->pfnOpen(&pThis->Medium, &pThis->hFsFmt);
            if (RT_SUCCESS(rc))
                pThis->pcFsFmtOps = pcFsFmtOpsMatch;
        }
        else
            rc = VERR_NOT_SUPPORTED;
    }

    if (RT_SUCCESS(rc))
        *phFs = pThis;
    else
        RTMemFree(pThis);

    return rc;
}

RTDECL(const char *) RTFilesystemGetFormat(RTFILESYSTEM hFs)
{
    PRTFILESYSTEMINT pThis = hFs;
    AssertPtrReturn(pThis, NULL);
    AssertReturn(pThis->u32Magic == RTFILESYSTEM_MAGIC, NULL);

    return pThis->pcFsFmtOps->pcszFmt;
}

RTDECL(uint64_t) RTFilesystemGetBlockSize(RTFILESYSTEM hFs)
{
    PRTFILESYSTEMINT pThis = hFs;
    AssertPtrReturn(pThis, 0);
    AssertReturn(pThis->u32Magic == RTFILESYSTEM_MAGIC, 0);

    return pThis->pcFsFmtOps->pfnGetBlockSize(pThis->hFsFmt);
}

RTDECL(int) RTFilesystemQueryRangeUse(RTFILESYSTEM hFs, uint64_t offStart, size_t cb,
                                      bool *pfUsed)
{
    PRTFILESYSTEMINT pThis = hFs;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTFILESYSTEM_MAGIC, VERR_INVALID_HANDLE);
    AssertPtrReturn(pfUsed, VERR_INVALID_POINTER);
    AssertReturn(offStart + cb <= pThis->Medium.cbMedium, VERR_OUT_OF_RANGE);

    return pThis->pcFsFmtOps->pfnQueryRangeUse(pThis->hFsFmt, offStart, cb, pfUsed);
}
