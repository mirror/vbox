/* $Id$ */
/** @file
 * IPRT - NTFS Virtual Filesystem, currently only for reading allocation bitmap.
 */

/*
 * Copyright (C) 2012-2017 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP RTLOGGROUP_FS
#include <iprt/fsvfs.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/log.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/vfs.h>
#include <iprt/vfslowlevel.h>
#include <iprt/formats/ntfs.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/**
 * Instance data for an NTFS volume.
 */
typedef struct RTFSNTFSVOL
{
    /** Handle to itself. */
    RTVFS           hVfsSelf;
    /** The file, partition, or whatever backing the NTFS volume. */
    RTVFSFILE       hVfsBacking;
    /** The size of the backing thingy. */
    uint64_t        cbBacking;
    /** The formatted size of the volume. */
    uint64_t        cbVolume;
    /** cbVolume expressed as a cluster count. */
    uint64_t        cClusters;

    /** RTVFSMNT_F_XXX. */
    uint32_t        fMntFlags;
    /** RTFSNTVFS_F_XXX (currently none defined). */
    uint32_t        fNtfsFlags;

    /** The (logical) cluster size. */
    uint32_t        cbCluster;
    /** The (logical) sector size. */
    uint32_t        cbSector;

    /** The shift count for converting between bytes and clusters. */
    uint8_t         cClusterShift;
    /** Explicit padding. */
    uint8_t         abReserved[7];

    /** The logical cluster number of the MFT. */
    uint64_t        uLcnMft;
    /** The logical cluster number of the mirror MFT. */
    uint64_t        uLcnMftMirror;

    /** The MFT record size. */
    uint32_t        cbMftRecord;
    /** The index record size. */
    uint32_t        cbIndexRecord;

    /** The volume serial number. */
    uint64_t        uSerialNo;

} RTFSNTFSVOL;
/** Pointer to the instance data for a NTFS volume. */
typedef RTFSNTFSVOL *PRTFSNTFSVOL;


/**
 * @interface_method_impl{RTVFSOBJOPS::Obj,pfnClose}
 */
static DECLCALLBACK(int) rtFsNtfsVol_Close(void *pvThis)
{
    PRTFSNTFSVOL pThis = (PRTFSNTFSVOL)pvThis;

    RTVfsFileRelease(pThis->hVfsBacking);
    pThis->hVfsBacking = NIL_RTVFSFILE;
    pThis->hVfsSelf    = NIL_RTVFS;

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSOBJOPS::Obj,pfnQueryInfo}
 */
static DECLCALLBACK(int) rtFsNtfsVol_QueryInfo(void *pvThis, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    NOREF(pvThis); NOREF(pObjInfo); NOREF(enmAddAttr);
    return VERR_WRONG_TYPE;
}


/**
 * @interface_method_impl{RTVFSOBJOPS::Obj,pfnOpenRoot}
 */
static DECLCALLBACK(int) rtFsNtfsVol_OpenRoot(void *pvThis, PRTVFSDIR phVfsDir)
{
    NOREF(pvThis); NOREF(phVfsDir);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * @interface_method_impl{RTVFSOBJOPS::Obj,pfnQueryRangeState}
 */
static DECLCALLBACK(int) rtFsNtfsVol_QueryRangeState(void *pvThis, uint64_t off, size_t cb, bool *pfUsed)
{
    NOREF(pvThis); NOREF(off); NOREF(cb); NOREF(pfUsed);
    return VERR_NOT_IMPLEMENTED;
}


DECL_HIDDEN_CONST(const RTVFSOPS) g_rtFsNtfsVolOps =
{
    /* .Obj = */
    {
        /* .uVersion = */       RTVFSOBJOPS_VERSION,
        /* .enmType = */        RTVFSOBJTYPE_VFS,
        /* .pszName = */        "NtfsVol",
        /* .pfnClose = */       rtFsNtfsVol_Close,
        /* .pfnQueryInfo = */   rtFsNtfsVol_QueryInfo,
        /* .uEndMarker = */     RTVFSOBJOPS_VERSION
    },
    /* .uVersion = */           RTVFSOPS_VERSION,
    /* .fFeatures = */          0,
    /* .pfnOpenRoot = */        rtFsNtfsVol_OpenRoot,
    /* .pfnQueryRangeState = */ rtFsNtfsVol_QueryRangeState,
    /* .uEndMarker = */         RTVFSOPS_VERSION
};

static int rtFsNtfsVolLoadMft(PRTFSNTFSVOL pThis, void *pvBuf, size_t cbBuf, PRTERRINFO pErrInfo)
{
    AssertReturn(cbBuf >= _64K, VERR_INTERNAL_ERROR_2);
    NOREF(pThis); NOREF(pvBuf); NOREF(cbBuf); NOREF(pErrInfo);
    /** @todo read MFT, find bitmap allocation, implement
     *        rtFsNtfsVol_QueryRangeState. */
    return VINF_SUCCESS;
}


/**
 * Loads the bootsector and parses it, copying values into the instance data.
 *
 * @returns IRPT status code.
 * @param   pThis           The instance data.
 * @param   pvBuf           The buffer.
 * @param   cbBuf           The buffer size.
 * @param   pErrInfo        Where to return additional error details.
 */
static int rtFsNtfsVolLoadAndParseBootsector(PRTFSNTFSVOL pThis, void *pvBuf, size_t cbBuf, PRTERRINFO pErrInfo)
{
    AssertReturn(cbBuf >= sizeof(FATBOOTSECTOR), VERR_INTERNAL_ERROR_2);

    /*
     * Read the boot sector and check that it makes sense for a NTFS volume.
     *
     * Note! There are two potential backup locations of the boot sector, however we
     *       currently don't implement falling back on these on corruption/read errors.
     */
    PFATBOOTSECTOR pBootSector = (PFATBOOTSECTOR)pvBuf;
    int rc = RTVfsFileRead(pThis->hVfsBacking, pBootSector, sizeof(*pBootSector), NULL);
    if (RT_FAILURE(rc))
        return RTERRINFO_LOG_SET(pErrInfo, rc, "Error reading boot sector");

    if (memcmp(pBootSector->achOemName, RT_STR_TUPLE(NTFS_OEM_ID_MAGIC)) != 0)
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_VFS_UNKNOWN_FORMAT,
                                   "Not NTFS - OEM field mismatch: %.8Rhxs'", pBootSector->achOemName);

    /* Check must-be-zero BPB fields. */
    if (pBootSector->Bpb.Ntfs.Bpb.cReservedSectors != 0)
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_VFS_UNKNOWN_FORMAT, "Not NTFS - MBZ: BPB.cReservedSectors=%u",
                                   RT_LE2H_U16(pBootSector->Bpb.Ntfs.Bpb.cReservedSectors));
    if (pBootSector->Bpb.Ntfs.Bpb.cFats != 0)
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_VFS_UNKNOWN_FORMAT, "Not NTFS - MBZ: BPB.cFats=%u",
                                   pBootSector->Bpb.Ntfs.Bpb.cFats);
    if (pBootSector->Bpb.Ntfs.Bpb.cMaxRootDirEntries != 0)
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_VFS_UNKNOWN_FORMAT, "Not NTFS - MBZ: BPB.cMaxRootDirEntries=%u",
                                   RT_LE2H_U16(pBootSector->Bpb.Ntfs.Bpb.cMaxRootDirEntries));
    if (pBootSector->Bpb.Ntfs.Bpb.cTotalSectors16 != 0)
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_VFS_UNKNOWN_FORMAT, "Not NTFS - MBZ: BPB.cTotalSectors16=%u",
                                   RT_LE2H_U16(pBootSector->Bpb.Ntfs.Bpb.cTotalSectors16));
    if (pBootSector->Bpb.Ntfs.Bpb.cSectorsPerFat != 0)
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_VFS_UNKNOWN_FORMAT, "Not NTFS - MBZ: BPB.cSectorsPerFat=%u",
                                   RT_LE2H_U16(pBootSector->Bpb.Ntfs.Bpb.cSectorsPerFat));

    /* Check other relevant BPB fields. */
    uint32_t cbSector = RT_LE2H_U16(pBootSector->Bpb.Ntfs.Bpb.cbSector);
    if (   cbSector != 512
        && cbSector != 1024
        && cbSector != 2048
        && cbSector != 4096)
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_VFS_UNKNOWN_FORMAT, "Not NTFS - BPB.cbSector is ouf of range: %u", cbSector);
    pThis->cbSector = cbSector;
    Log2(("NTFS BPB: cbSector=%#x\n", cbSector));

    uint32_t cClusterPerSector = RT_LE2H_U16(pBootSector->Bpb.Ntfs.Bpb.cSectorsPerCluster);
    if (   !RT_IS_POWER_OF_TWO(cClusterPerSector)
        || cClusterPerSector == 0)
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_VFS_UNKNOWN_FORMAT,
                                   "Not NTFS - BPB.cCluster is ouf of range: %u", cClusterPerSector);

    pThis->cbCluster = cClusterPerSector * cbSector;
    if (pThis->cbCluster > _64K)
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT,
                                   "cluster size exceeds 64KB: %#x", pThis->cbCluster);
    pThis->cClusterShift = ASMBitFirstSetU32(pThis->cbCluster) - 1;
    Log2(("NTFS BPB: cClusterPerSector=%#x => %#x bytes, %u shift\n", cClusterPerSector, pThis->cbCluster, pThis->cClusterShift));

    /* NTFS BPB: cSectors. */
    uint64_t cSectors = RT_LE2H_U64(pBootSector->Bpb.Ntfs.cSectors);
    if (cSectors > pThis->cbBacking / pThis->cbSector)
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT,
                                   "NTFS sector count exceeds volume size: %#RX64 vs %#RX64",
                                   cSectors, pThis->cbBacking / pThis->cbSector);
    if (cSectors < 256)
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT, "NTFS sector count too small: %#RX64", cSectors);
    pThis->cbVolume  = cSectors * pThis->cbSector;
    pThis->cClusters = cSectors / cClusterPerSector;
    Log2(("NTFS BPB: cSectors=%#RX64 => %#xu bytes (%Rhcb) => cClusters=%#RX64\n",
          cSectors, pThis->cbVolume, pThis->cbVolume, pThis->cClusters));

    /* NTFS BPB: MFT location. */
    uint64_t uLcn = RT_LE2H_U64(pBootSector->Bpb.Ntfs.uLcnMft);
    if (   uLcn < 1
        || uLcn >= pThis->cClusters)
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT,
                                   "NTFS MFT location is out of bounds: %#RX64 (%#RX64 clusters)", uLcn, pThis->cClusters);
    pThis->uLcnMft = uLcn;
    Log2(("NTFS BPB: uLcnMft=%#RX64 (byte offset %#RX64)\n", uLcn, uLcn << pThis->cClusterShift));

    /* NTFS BPB: Mirror MFT location. */
    uLcn = RT_LE2H_U64(pBootSector->Bpb.Ntfs.uLcnMftMirror);
    if (   uLcn < 1
        || uLcn >= pThis->cClusters)
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT,
                                   "NTFS mirror MFT location is out of bounds: %#RX64 (%#RX64 clusters)", uLcn, pThis->cClusters);
    pThis->uLcnMftMirror = uLcn;
    Log2(("NTFS BPB: uLcnMftMirror=%#RX64 (byte offset %#RX64)\n", uLcn, uLcn << pThis->cClusterShift));

    /* NTFS BPB: Size of MFT file record. */
    if (pBootSector->Bpb.Ntfs.cClustersPerMftRecord >= 0)
    {
        if (   !RT_IS_POWER_OF_TWO((uint32_t)pBootSector->Bpb.Ntfs.cClustersPerMftRecord)
            || pBootSector->Bpb.Ntfs.cClustersPerMftRecord == 0)
            return RTERRINFO_LOG_SET_F(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT,
                                       "NTFS clusters-per-mft-record value is zero or not a power of two: %#x",
                                        pBootSector->Bpb.Ntfs.cClustersPerMftRecord);
        pThis->cbMftRecord = (uint32_t)pBootSector->Bpb.Ntfs.cClustersPerMftRecord << pThis->cClusterShift;
        Assert(pThis->cbMftRecord == pBootSector->Bpb.Ntfs.cClustersPerMftRecord * pThis->cbCluster);
    }
    else if (   pBootSector->Bpb.Ntfs.cClustersPerMftRecord < -20
             || pBootSector->Bpb.Ntfs.cClustersPerMftRecord > -9)
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT,
                                   "NTFS clusters-per-mft-record is out of shift range: %d",
                                    pBootSector->Bpb.Ntfs.cClustersPerMftRecord);
    else
        pThis->cbMftRecord = UINT32_C(1) << -pBootSector->Bpb.Ntfs.cClustersPerMftRecord;
    Log2(("NTFS BPB: cbMftRecord=%#x\n", pThis->cbMftRecord));

    /* NTFS BPB: Index block size */
    if (pBootSector->Bpb.Ntfs.cClusterPerIndexBlock >= 0)
    {
        if (   !RT_IS_POWER_OF_TWO((uint32_t)pBootSector->Bpb.Ntfs.cClusterPerIndexBlock)
            || pBootSector->Bpb.Ntfs.cClusterPerIndexBlock == 0)
            return RTERRINFO_LOG_SET_F(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT,
                                       "NTFS clusters-per-index-block is zero or not a power of two: %#x",
                                        pBootSector->Bpb.Ntfs.cClusterPerIndexBlock);
        pThis->cbIndexRecord = (uint32_t)pBootSector->Bpb.Ntfs.cClusterPerIndexBlock << pThis->cClusterShift;
        Assert(pThis->cbIndexRecord == pBootSector->Bpb.Ntfs.cClusterPerIndexBlock * pThis->cbCluster);
    }
    else if (   pBootSector->Bpb.Ntfs.cClusterPerIndexBlock < -32
             || pBootSector->Bpb.Ntfs.cClusterPerIndexBlock > -9)
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT,
                                   "NTFS clusters-per-index-block is out of shift range: %d",
                                    pBootSector->Bpb.Ntfs.cClusterPerIndexBlock);
    else
        pThis->cbIndexRecord = UINT32_C(1) << -pBootSector->Bpb.Ntfs.cClustersPerMftRecord;
    Log2(("NTFS BPB: cbIndexRecord=%#x\n", pThis->cbIndexRecord));

    pThis->uSerialNo = RT_LE2H_U64(pBootSector->Bpb.Ntfs.uSerialNumber);
    Log2(("NTFS BPB: uSerialNo=%#x\n", pThis->uSerialNo));


    return VINF_SUCCESS;
}


RTDECL(int) RTFsNtfsVolOpen(RTVFSFILE hVfsFileIn, uint32_t fMntFlags, uint32_t fNtfsFlags, PRTVFS phVfs, PRTERRINFO pErrInfo)
{
    AssertPtrReturn(phVfs, VERR_INVALID_POINTER);
    AssertReturn(!(fMntFlags & RTVFSMNT_F_VALID_MASK), VERR_INVALID_FLAGS);
    AssertReturn(!fNtfsFlags, VERR_INVALID_FLAGS);

    uint32_t cRefs = RTVfsFileRetain(hVfsFileIn);
    AssertReturn(cRefs != UINT32_MAX, VERR_INVALID_HANDLE);

    /*
     * Create a VFS instance and initialize the data so rtFsNtfsVol_Close works.
     */
    RTVFS        hVfs;
    PRTFSNTFSVOL pThis;
    int rc = RTVfsNew(&g_rtFsNtfsVolOps, sizeof(*pThis), NIL_RTVFS, RTVFSLOCK_CREATE_RW, &hVfs, (void **)&pThis);
    if (RT_SUCCESS(rc))
    {
        pThis->hVfsBacking    = hVfsFileIn;
        pThis->hVfsSelf       = hVfs;
        pThis->fMntFlags      = fMntFlags;
        pThis->fNtfsFlags     = fNtfsFlags;

        rc = RTVfsFileGetSize(pThis->hVfsBacking, &pThis->cbBacking);
        if (RT_SUCCESS(rc))
        {
            void *pvBuf = RTMemTmpAlloc(_64K);
            if (pvBuf)
            {
                rc = rtFsNtfsVolLoadAndParseBootsector(pThis, pvBuf, _64K, pErrInfo);
                if (RT_SUCCESS(rc))
                    rc = rtFsNtfsVolLoadMft(pThis, pvBuf, _64K, pErrInfo);
                RTMemTmpFree(pvBuf);
                if (RT_SUCCESS(rc))
                {
                    *phVfs = hVfs;
                    return VINF_SUCCESS;
                }
            }
            else
                rc = VERR_NO_TMP_MEMORY;
        }

        RTVfsRelease(hVfs);
        *phVfs = NIL_RTVFS;
    }
    else
        RTVfsFileRelease(hVfsFileIn);

    return rc;
}


/**
 * @interface_method_impl{RTVFSCHAINELEMENTREG,pfnValidate}
 */
static DECLCALLBACK(int) rtVfsChainNtfsVol_Validate(PCRTVFSCHAINELEMENTREG pProviderReg, PRTVFSCHAINSPEC pSpec,
                                                    PRTVFSCHAINELEMSPEC pElement, uint32_t *poffError, PRTERRINFO pErrInfo)
{
    RT_NOREF(pProviderReg);

    /*
     * Basic checks.
     */
    if (pElement->enmTypeIn != RTVFSOBJTYPE_FILE)
        return pElement->enmTypeIn == RTVFSOBJTYPE_INVALID ? VERR_VFS_CHAIN_CANNOT_BE_FIRST_ELEMENT : VERR_VFS_CHAIN_TAKES_FILE;
    if (   pElement->enmType != RTVFSOBJTYPE_VFS
        && pElement->enmType != RTVFSOBJTYPE_DIR)
        return VERR_VFS_CHAIN_ONLY_DIR_OR_VFS;
    if (pElement->cArgs > 1)
        return VERR_VFS_CHAIN_AT_MOST_ONE_ARG;

    /*
     * Parse the flag if present, save in pElement->uProvider.
     */
    bool fReadOnly = (pSpec->fOpenFile & RTFILE_O_ACCESS_MASK) == RTFILE_O_READ;
    if (pElement->cArgs > 0)
    {
        const char *psz = pElement->paArgs[0].psz;
        if (*psz)
        {
            if (!strcmp(psz, "ro"))
                fReadOnly = true;
            else if (!strcmp(psz, "rw"))
                fReadOnly = false;
            else
            {
                *poffError = pElement->paArgs[0].offSpec;
                return RTErrInfoSet(pErrInfo, VERR_VFS_CHAIN_INVALID_ARGUMENT, "Expected 'ro' or 'rw' as argument");
            }
        }
    }

    pElement->uProvider = fReadOnly ? RTVFSMNT_F_READ_ONLY : 0;
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSCHAINELEMENTREG,pfnInstantiate}
 */
static DECLCALLBACK(int) rtVfsChainNtfsVol_Instantiate(PCRTVFSCHAINELEMENTREG pProviderReg, PCRTVFSCHAINSPEC pSpec,
                                                       PCRTVFSCHAINELEMSPEC pElement, RTVFSOBJ hPrevVfsObj,
                                                       PRTVFSOBJ phVfsObj, uint32_t *poffError, PRTERRINFO pErrInfo)
{
    RT_NOREF(pProviderReg, pSpec, poffError);

    int         rc;
    RTVFSFILE   hVfsFileIn = RTVfsObjToFile(hPrevVfsObj);
    if (hVfsFileIn != NIL_RTVFSFILE)
    {
        RTVFS hVfs;
        rc = RTFsNtfsVolOpen(hVfsFileIn, (uint32_t)pElement->uProvider, (uint32_t)(pElement->uProvider >> 32), &hVfs, pErrInfo);
        RTVfsFileRelease(hVfsFileIn);
        if (RT_SUCCESS(rc))
        {
            *phVfsObj = RTVfsObjFromVfs(hVfs);
            RTVfsRelease(hVfs);
            if (*phVfsObj != NIL_RTVFSOBJ)
                return VINF_SUCCESS;
            rc = VERR_VFS_CHAIN_CAST_FAILED;
        }
    }
    else
        rc = VERR_VFS_CHAIN_CAST_FAILED;
    return rc;
}


/**
 * @interface_method_impl{RTVFSCHAINELEMENTREG,pfnCanReuseElement}
 */
static DECLCALLBACK(bool) rtVfsChainNtfsVol_CanReuseElement(PCRTVFSCHAINELEMENTREG pProviderReg,
                                                            PCRTVFSCHAINSPEC pSpec, PCRTVFSCHAINELEMSPEC pElement,
                                                            PCRTVFSCHAINSPEC pReuseSpec, PCRTVFSCHAINELEMSPEC pReuseElement)
{
    RT_NOREF(pProviderReg, pSpec, pReuseSpec);
    if (   pElement->paArgs[0].uProvider == pReuseElement->paArgs[0].uProvider
        || !pReuseElement->paArgs[0].uProvider)
        return true;
    return false;
}


/** VFS chain element 'file'. */
static RTVFSCHAINELEMENTREG g_rtVfsChainNtfsVolReg =
{
    /* uVersion = */            RTVFSCHAINELEMENTREG_VERSION,
    /* fReserved = */           0,
    /* pszName = */             "ntfs",
    /* ListEntry = */           { NULL, NULL },
    /* pszHelp = */             "Open a NTFS file system, requires a file object on the left side.\n"
                                "First argument is an optional 'ro' (read-only) or 'rw' (read-write) flag.\n",
    /* pfnValidate = */         rtVfsChainNtfsVol_Validate,
    /* pfnInstantiate = */      rtVfsChainNtfsVol_Instantiate,
    /* pfnCanReuseElement = */  rtVfsChainNtfsVol_CanReuseElement,
    /* uEndMarker = */          RTVFSCHAINELEMENTREG_VERSION
};

RTVFSCHAIN_AUTO_REGISTER_ELEMENT_PROVIDER(&g_rtVfsChainNtfsVolReg, rtVfsChainNtfsVolReg);


