/* $Id$ */
/** @file
 * IPRT Disk Volume Management API (DVM) - generic code.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
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

#include <iprt/types.h>
#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/dvm.h>
#include <iprt/err.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#include "internal/dvm.h"

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

/**
 * The internal volume manager structure.
 */
typedef struct RTDVMINTERNAL
{
    /** The DVM magic (RTDVM_MAGIC). */
    uint32_t          u32Magic;
    /** The disk descriptor. */
    RTDVMDISK         DvmDisk;
    /** Pointer to the backend operations table after a successful probe. */
    PCRTDVMFMTOPS     pDvmFmtOps;
    /** The format specific volume manager data. */
    RTDVMFMT          hVolMgrFmt;
    /** Reference counter. */
    uint32_t volatile cRefs;
} RTDVMINTERNAL;
/** Pointer to an internal volume manager. */
typedef RTDVMINTERNAL *PRTDVMINTERNAL;

/**
 * The internal volume structure.
 */
typedef struct RTDVMVOLUMEINTERNAL
{
    /** The DVM volume magic (RTDVMVOLUME_MAGIC). */
    uint32_t          u32Magic;
    /** Pointer to the owning volume manager. */
    PRTDVMINTERNAL    pVolMgr;
    /** Format specific volume data. */
    RTDVMVOLUMEFMT    hVolFmt;
    /** Reference counter. */
    uint32_t volatile cRefs;
} RTDVMVOLUMEINTERNAL;
/** Pointer to an internal volume. */
typedef RTDVMVOLUMEINTERNAL *PRTDVMVOLUMEINTERNAL;

/*******************************************************************************
*  Global variables                                                            *
*******************************************************************************/
extern RTDVMFMTOPS g_DvmFmtMbr;
extern RTDVMFMTOPS g_DvmFmtGpt;

/**
 * Supported volume formats.
 */
static PCRTDVMFMTOPS g_aDvmFmts[] =
{
    &g_DvmFmtMbr,
    &g_DvmFmtGpt
};

/**
 * Descriptions of the volume types.
 */
static const char * g_apcszDvmVolTypes[] =
{
    "Invalid",
    "Unknown",
    "NTFS",
    "FAT16",
    "FAT32",
    "Linux swap",
    "Linux native",
    "Linux LVM",
    "Linux SoftRaid",
    "FreeBSD",
    "NetBSD",
    "OpenBSD",
    "Mac OS X HFS or HFS+",
    "Solaris"
};

RTDECL(int) RTDvmCreate(PRTDVM phVolMgr, PFNDVMREAD pfnRead,
                        PFNDVMWRITE pfnWrite, uint64_t cbDisk,
                        uint64_t cbSector, void *pvUser)
{
    int rc = VINF_SUCCESS;
    PRTDVMINTERNAL pThis;

    pThis = (PRTDVMINTERNAL)RTMemAllocZ(sizeof(RTDVMINTERNAL));
    if (VALID_PTR(pThis))
    {
        pThis->u32Magic         = RTDVM_MAGIC;
        pThis->DvmDisk.cbDisk   = cbDisk;
        pThis->DvmDisk.cbSector = cbSector;
        pThis->DvmDisk.pvUser   = pvUser;
        pThis->DvmDisk.pfnRead  = pfnRead;
        pThis->DvmDisk.pfnWrite = pfnWrite;
        pThis->pDvmFmtOps       = NULL;
        pThis->hVolMgrFmt       = NIL_RTDVMFMT;
        pThis->cRefs            = 1;
        *phVolMgr = pThis;
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}

RTDECL(uint32_t) RTDvmRetain(RTDVM hVolMgr)
{
    PRTDVMINTERNAL pThis = hVolMgr;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->u32Magic == RTDVM_MAGIC, UINT32_MAX);

    uint32_t cRefs = ASMAtomicIncU32(&pThis->cRefs);
    AssertMsg(cRefs > 1 && cRefs < _1M, ("%#x %p\n", cRefs, pThis));
    return cRefs;
}

/**
 * Destroys a volume manager handle.
 *
 * @param   pThis               The volume manager to destroy.
 */
static void rtDvmDestroy(PRTDVMINTERNAL pThis)
{
    if (pThis->hVolMgrFmt != NIL_RTDVMFMT)
    {
        AssertPtr(pThis->pDvmFmtOps);

        /* Let the backend do it's own cleanup first. */
        pThis->pDvmFmtOps->pfnClose(pThis->hVolMgrFmt);
        pThis->hVolMgrFmt = NIL_RTDVMFMT;
    }

    pThis->DvmDisk.cbDisk   = 0;
    pThis->DvmDisk.pvUser   = NULL;
    pThis->DvmDisk.pfnRead  = NULL;
    pThis->DvmDisk.pfnWrite = NULL;
    pThis->u32Magic         = RTDVM_MAGIC_DEAD;
    RTMemFree(pThis);
}

RTDECL(uint32_t) RTDvmRelease(RTDVM hVolMgr)
{
    PRTDVMINTERNAL pThis = hVolMgr;
    if (pThis == NIL_RTDVM)
        return 0;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->u32Magic == RTDVM_MAGIC, UINT32_MAX);

    uint32_t cRefs = ASMAtomicDecU32(&pThis->cRefs);
    AssertMsg(cRefs < _1M, ("%#x %p\n", cRefs, pThis));
    if (cRefs == 0)
        rtDvmDestroy(pThis);
    return cRefs;
}

RTDECL(int) RTDvmMapOpen(RTDVM hVolMgr)
{
    int            rc = VINF_SUCCESS;
    uint32_t       uScoreMax = RTDVM_MATCH_SCORE_UNSUPPORTED;
    PCRTDVMFMTOPS  pDvmFmtOpsMatch = NULL;
    PRTDVMINTERNAL pThis = hVolMgr;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTDVM_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(pThis->hVolMgrFmt == NIL_RTDVMFMT, VERR_INVALID_HANDLE);

    Assert(!pThis->pDvmFmtOps);

    for (unsigned i = 0; i < RT_ELEMENTS(g_aDvmFmts); i++)
    {
        uint32_t uScore;
        PCRTDVMFMTOPS pDvmFmtOps = g_aDvmFmts[i];

        rc = pDvmFmtOps->pfnProbe(&pThis->DvmDisk, &uScore);
        if (   RT_SUCCESS(rc)
            && uScore > uScoreMax)
        {
            pDvmFmtOpsMatch = pDvmFmtOps;
            uScoreMax       = uScore;
        }
        else if (RT_FAILURE(rc))
            break;
    }

    if (RT_SUCCESS(rc))
    {
        if (uScoreMax > RTDVM_MATCH_SCORE_UNSUPPORTED)
        {
            AssertPtr(pDvmFmtOpsMatch);

            /* Open the format. */
            rc = pDvmFmtOpsMatch->pfnOpen(&pThis->DvmDisk, &pThis->hVolMgrFmt);
            if (RT_SUCCESS(rc))
                pThis->pDvmFmtOps = pDvmFmtOpsMatch;
        }
        else
            rc = VERR_NOT_FOUND;
    }

    return rc;
}

RTDECL(int) RTDvmMapInitialize(RTDVM hVolMgr, const char *pszFmt)
{
    int rc = VINF_SUCCESS;
    PRTDVMINTERNAL pThis = hVolMgr;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertPtrReturn(pszFmt, VERR_INVALID_POINTER);
    AssertReturn(pThis->u32Magic == RTDVM_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(pThis->hVolMgrFmt == NIL_RTDVMFMT, VERR_INVALID_HANDLE);

    for (unsigned i = 0; i < RT_ELEMENTS(g_aDvmFmts); i++)
    {
        PCRTDVMFMTOPS pDvmFmtOps = g_aDvmFmts[i];

        if (!RTStrCmp(pDvmFmtOps->pcszFmt, pszFmt))
        {
            rc = pDvmFmtOps->pfnInitialize(&pThis->DvmDisk, &pThis->hVolMgrFmt);
            if (RT_SUCCESS(rc))
                pThis->pDvmFmtOps = pDvmFmtOps;

            break;
        }
    }

    return rc;
}

RTDECL(const char *) RTDvmMapGetFormat(RTDVM hVolMgr)
{
    PRTDVMINTERNAL pThis = hVolMgr;
    AssertPtrReturn(pThis, NULL);
    AssertReturn(pThis->u32Magic == RTDVM_MAGIC, NULL);
    AssertReturn(pThis->hVolMgrFmt != NIL_RTDVMFMT, NULL);

    return pThis->pDvmFmtOps->pcszFmt;
}

RTDECL(uint32_t) RTDvmMapGetValidVolumes(RTDVM hVolMgr)
{
    PRTDVMINTERNAL pThis = hVolMgr;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->u32Magic == RTDVM_MAGIC, UINT32_MAX);
    AssertReturn(pThis->hVolMgrFmt != NIL_RTDVMFMT, UINT32_MAX);

    return pThis->pDvmFmtOps->pfnGetValidVolumes(pThis->hVolMgrFmt);
}

RTDECL(uint32_t) RTDvmMapGetMaxVolumes(RTDVM hVolMgr)
{
    PRTDVMINTERNAL pThis = hVolMgr;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->u32Magic == RTDVM_MAGIC, UINT32_MAX);
    AssertReturn(pThis->hVolMgrFmt != NIL_RTDVMFMT, UINT32_MAX);

    return pThis->pDvmFmtOps->pfnGetMaxVolumes(pThis->hVolMgrFmt);
}

static int rtDvmVolumeCreate(PRTDVMINTERNAL pThis, RTDVMVOLUMEFMT hVolFmt,
                             PRTDVMVOLUME phVol)
{
    int rc = VINF_SUCCESS;
    PRTDVMVOLUMEINTERNAL pVol = NULL;

    pVol = (PRTDVMVOLUMEINTERNAL)RTMemAllocZ(sizeof(RTDVMVOLUMEINTERNAL));
    if (VALID_PTR(pVol))
    {
        pVol->u32Magic = RTDVMVOLUME_MAGIC;
        pVol->cRefs    = 1;
        pVol->pVolMgr  = pThis;
        pVol->hVolFmt  = hVolFmt;

        /* Reference the volume manager. */
        RTDvmRetain(pThis);
        *phVol = pVol;
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}

RTDECL(int) RTDvmMapQueryFirstVolume(RTDVM hVolMgr, PRTDVMVOLUME phVol)
{
    int rc = VINF_SUCCESS;
    PRTDVMINTERNAL pThis = hVolMgr;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTDVM_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(pThis->hVolMgrFmt != NIL_RTDVMFMT, VERR_INVALID_HANDLE);
    AssertPtrReturn(phVol, VERR_INVALID_POINTER);

    RTDVMVOLUMEFMT hVolFmt = NIL_RTDVMVOLUMEFMT;
    rc = pThis->pDvmFmtOps->pfnQueryFirstVolume(pThis->hVolMgrFmt, &hVolFmt);
    if (RT_SUCCESS(rc))
    {
        rc = rtDvmVolumeCreate(pThis, hVolFmt, phVol);
        if (RT_FAILURE(rc))
            pThis->pDvmFmtOps->pfnVolumeClose(hVolFmt);
    }

    return rc;
}

RTDECL(int) RTDvmMapQueryNextVolume(RTDVM hVolMgr, RTDVMVOLUME hVol, PRTDVMVOLUME phVolNext)
{
    int rc = VINF_SUCCESS;
    PRTDVMINTERNAL       pThis = hVolMgr;
    PRTDVMVOLUMEINTERNAL pVol = hVol;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTDVM_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(pThis->hVolMgrFmt != NIL_RTDVMFMT, VERR_INVALID_HANDLE);
    AssertPtrReturn(pVol, VERR_INVALID_HANDLE);
    AssertReturn(pVol->u32Magic == RTDVMVOLUME_MAGIC, VERR_INVALID_HANDLE);
    AssertPtrReturn(phVolNext, VERR_INVALID_POINTER);

    RTDVMVOLUMEFMT hVolFmtNext = NIL_RTDVMVOLUMEFMT;
    rc = pThis->pDvmFmtOps->pfnQueryNextVolume(pThis->hVolMgrFmt, pVol->hVolFmt, &hVolFmtNext);
    if (RT_SUCCESS(rc))
    {
        rc = rtDvmVolumeCreate(pThis, hVolFmtNext, phVolNext);
        if (RT_FAILURE(rc))
            pThis->pDvmFmtOps->pfnVolumeClose(hVolFmtNext);
    }

    return rc;
}

RTDECL(uint32_t) RTDvmVolumeRetain(RTDVMVOLUME hVol)
{
    PRTDVMVOLUMEINTERNAL pThis = hVol;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->u32Magic == RTDVMVOLUME_MAGIC, UINT32_MAX);

    uint32_t cRefs = ASMAtomicIncU32(&pThis->cRefs);
    AssertMsg(cRefs > 1 && cRefs < _1M, ("%#x %p\n", cRefs, pThis));
    return cRefs;
}

/**
 * Destroys a volume handle.
 *
 * @param   pThis               The volume to destroy.
 */
static void rtDvmVolumeDestroy(PRTDVMVOLUMEINTERNAL pThis)
{
    PRTDVMINTERNAL pVolMgr = pThis->pVolMgr;

    AssertPtr(pVolMgr);

    /* Close the volume. */
    pVolMgr->pDvmFmtOps->pfnVolumeClose(pThis->hVolFmt);

    pThis->u32Magic = RTDVMVOLUME_MAGIC_DEAD;
    pThis->pVolMgr  = NULL;
    pThis->hVolFmt  = NIL_RTDVMVOLUMEFMT;
    RTMemFree(pThis);

    /* Release the reference of the volume manager. */
    RTDvmRelease(pVolMgr);
}

RTDECL(uint32_t) RTDvmVolumeRelease(RTDVMVOLUME hVol)
{
    PRTDVMVOLUMEINTERNAL pThis = hVol;
    if (pThis == NIL_RTDVMVOLUME)
        return 0;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->u32Magic == RTDVMVOLUME_MAGIC, UINT32_MAX);

    uint32_t cRefs = ASMAtomicDecU32(&pThis->cRefs);
    AssertMsg(cRefs < _1M, ("%#x %p\n", cRefs, pThis));
    if (cRefs == 0)
        rtDvmVolumeDestroy(pThis);
    return cRefs;
}

RTDECL(uint64_t) RTDvmVolumeGetSize(RTDVMVOLUME hVol)
{
    PRTDVMVOLUMEINTERNAL pThis = hVol;
    AssertPtrReturn(pThis, 0);
    AssertReturn(pThis->u32Magic == RTDVMVOLUME_MAGIC, 0);

    return pThis->pVolMgr->pDvmFmtOps->pfnVolumeGetSize(pThis->hVolFmt);
}

RTDECL(int) RTDvmVolumeQueryName(RTDVMVOLUME hVol, char **ppszVolName)
{
    PRTDVMVOLUMEINTERNAL pThis = hVol;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTDVMVOLUME_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(ppszVolName, VERR_INVALID_POINTER);

    return pThis->pVolMgr->pDvmFmtOps->pfnVolumeQueryName(pThis->hVolFmt, ppszVolName);
}

RTDECL(RTDVMVOLTYPE) RTDvmVolumeGetType(RTDVMVOLUME hVol)
{
    PRTDVMVOLUMEINTERNAL pThis = hVol;
    AssertPtrReturn(pThis, RTDVMVOLTYPE_INVALID);
    AssertReturn(pThis->u32Magic == RTDVMVOLUME_MAGIC, RTDVMVOLTYPE_INVALID);

    return pThis->pVolMgr->pDvmFmtOps->pfnVolumeGetType(pThis->hVolFmt);
}

RTDECL(uint64_t) RTDvmVolumeGetFlags(RTDVMVOLUME hVol)
{
    PRTDVMVOLUMEINTERNAL pThis = hVol;
    AssertPtrReturn(pThis, UINT64_MAX);
    AssertReturn(pThis->u32Magic == RTDVMVOLUME_MAGIC, UINT64_MAX);

    return pThis->pVolMgr->pDvmFmtOps->pfnVolumeGetFlags(pThis->hVolFmt);
}

RTDECL(int) RTDvmVolumeRead(RTDVMVOLUME hVol, uint64_t off, void *pvBuf, size_t cbRead)
{
    PRTDVMVOLUMEINTERNAL pThis = hVol;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTDVMVOLUME_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(pvBuf, VERR_INVALID_POINTER);
    AssertReturn(cbRead > 0, VERR_INVALID_PARAMETER);

    return pThis->pVolMgr->pDvmFmtOps->pfnVolumeRead(pThis->hVolFmt, off, pvBuf, cbRead);
}

RTDECL(int) RTDvmVolumeWrite(RTDVMVOLUME hVol, uint64_t off, const void *pvBuf, size_t cbWrite)
{
    PRTDVMVOLUMEINTERNAL pThis = hVol;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTDVMVOLUME_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(pvBuf, VERR_INVALID_POINTER);
    AssertReturn(cbWrite > 0, VERR_INVALID_PARAMETER);

    return pThis->pVolMgr->pDvmFmtOps->pfnVolumeWrite(pThis->hVolFmt, off, pvBuf, cbWrite);
}

RTDECL(const char *) RTDvmVolumeTypeGetDescr(RTDVMVOLTYPE enmVolType)
{
    AssertReturn(enmVolType >= RTDVMVOLTYPE_INVALID && enmVolType < RTDVMVOLTYPE_32BIT_HACK, NULL);

    return g_apcszDvmVolTypes[enmVolType];
}
