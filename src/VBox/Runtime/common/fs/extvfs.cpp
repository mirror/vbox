/* $Id$ */
/** @file
 * IPRT - Ext2/3/4 Virtual Filesystem.
 */

/*
 * Copyright (C) 2012-2018 Oracle Corporation
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

#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/log.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/vfs.h>
#include <iprt/vfslowlevel.h>
#include <iprt/formats/ext.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Cached block group descriptor data.
 */
typedef struct RTFSEXTBLKGRP
{
    /** Start offset (in bytes and from the start of the disk). */
    uint64_t    offStart;
    /** Last offset in the block group (inclusive). */
    uint64_t    offLast;
    /** Block bitmap - variable in size (depends on the block size
     * and number of blocks per group). */
    uint8_t     abBlockBitmap[1];
} RTFSEXTBLKGRP;
/** Pointer to block group descriptor data. */
typedef RTFSEXTBLKGRP *PRTFSEXTBLKGRP;

/**
 * Ext2/3/4 filesystem volume.
 */
typedef struct RTFSEXTVOL
{
    /** Handle to itself. */
    RTVFS               hVfsSelf;
    /** The file, partition, or whatever backing the ext volume. */
    RTVFSFILE           hVfsBacking;
    /** The size of the backing thingy. */
    uint64_t            cbBacking;
    /** The formatted size of the volume. */
    uint64_t            cbVolume;
    /** cbVolume expressed as a cluster count. */
    uint64_t            cClusters;

    /** RTVFSMNT_F_XXX. */
    uint32_t            fMntFlags;
    /** RTFSEXTVFS_F_XXX (currently none defined). */
    uint32_t            fExtFlags;

    /** The (logical) sector size. */
    uint32_t            cbSector;

    /** The (logical) cluster size. */
    uint32_t            cbCluster;

    /** Block number of the superblock. */
    uint32_t            iSbBlock;
    /** Size of one block. */
    size_t              cbBlock;
    /** Number of blocks in one group. */
    unsigned            cBlocksPerGroup;
    /** Number of blocks groups in the volume. */
    unsigned            cBlockGroups;
    /** Cached block group descriptor data. */
    PRTFSEXTBLKGRP      pBlkGrpDesc;
} RTFSEXTVOL;
/** Pointer to the ext filesystem data. */
typedef RTFSEXTVOL *PRTFSEXTVOL;



/**
 * Loads the block descriptor of the given block group from the medium.
 *
 * @returns IPRT status code.
 * @param   pThis    EXT filesystem instance data.
 * @param   iBlkGrp  Block group number to load.
 */
static int rtFsExtLoadBlkGrpDesc(PRTFSEXTVOL pThis, uint32_t iBlkGrp)
{
    size_t cbBlockBitmap = pThis->cBlocksPerGroup / 8;
    if (pThis->cBlocksPerGroup % 8)
        cbBlockBitmap++;

    PRTFSEXTBLKGRP pBlkGrpDesc = pThis->pBlkGrpDesc;
    if (!pBlkGrpDesc)
    {
        size_t cbBlkDesc = RT_UOFFSETOF_DYN(RTFSEXTBLKGRP, abBlockBitmap[cbBlockBitmap]);
        pBlkGrpDesc = (PRTFSEXTBLKGRP)RTMemAllocZ(cbBlkDesc);
        if (!pBlkGrpDesc)
            return VERR_NO_MEMORY;
    }

    uint64_t           offRead = (pThis->iSbBlock + 1) * pThis->cbBlock;
    EXTBLOCKGROUPDESC  BlkDesc;
    int rc = RTVfsFileReadAt(pThis->hVfsBacking, offRead, &BlkDesc, sizeof(BlkDesc), NULL);
    if (RT_SUCCESS(rc))
    {
        pBlkGrpDesc->offStart = pThis->iSbBlock + (uint64_t)iBlkGrp * pThis->cBlocksPerGroup * pThis->cbBlock;
        pBlkGrpDesc->offLast  = pBlkGrpDesc->offStart + pThis->cBlocksPerGroup * pThis->cbBlock;
        rc = RTVfsFileReadAt(pThis->hVfsBacking, BlkDesc.offBlockBitmap * pThis->cbBlock,
                             &pBlkGrpDesc->abBlockBitmap[0], cbBlockBitmap, NULL);
    }

    pThis->pBlkGrpDesc = pBlkGrpDesc;
    return rc;
}


static bool rtFsExtIsBlockRangeInUse(PRTFSEXTBLKGRP pBlkGrpDesc, uint32_t offBlockStart, size_t cBlocks)
{
    while (cBlocks)
    {
        uint32_t idxByte = offBlockStart / 8;
        uint32_t iBit = offBlockStart % 8;

        if (pBlkGrpDesc->abBlockBitmap[idxByte] & RT_BIT(iBit))
            return true;

        cBlocks--;
        offBlockStart++;
    }

    return false;
}


/**
 * @interface_method_impl{RTVFSOBJOPS::Obj,pfnClose}
 */
static DECLCALLBACK(int) rtFsExtVol_Close(void *pvThis)
{
    PRTFSEXTVOL pThis = (PRTFSEXTVOL)pvThis;

    if (pThis->pBlkGrpDesc)
        RTMemFree(pThis->pBlkGrpDesc);

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSOBJOPS::Obj,pfnQueryInfo}
 */
static DECLCALLBACK(int) rtFsExtVol_QueryInfo(void *pvThis, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    RT_NOREF(pvThis, pObjInfo, enmAddAttr);
    return VERR_WRONG_TYPE;
}


/**
 * @interface_method_impl{RTVFSOBJOPS::Obj,pfnOpenRoot}
 */
static DECLCALLBACK(int) rtFsExtVol_OpenRoot(void *pvThis, PRTVFSDIR phVfsDir)
{
    NOREF(pvThis);
    NOREF(phVfsDir);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * @interface_method_impl{RTVFSOBJOPS::Obj,pfnQueryRangeState}
 */
static DECLCALLBACK(int) rtFsExtVol_QueryRangeState(void *pvThis, uint64_t off, size_t cb, bool *pfUsed)
{
    int            rc    = VINF_SUCCESS;
    PRTFSEXTVOL    pThis = (PRTFSEXTVOL)pvThis;

    *pfUsed = false;

    while (cb > 0)
    {
        uint32_t const offBlockStart    = (uint32_t)(off / pThis->cbBlock);
        uint32_t const iBlockGroup      = (offBlockStart - pThis->iSbBlock) / pThis->cBlocksPerGroup;
        uint32_t const offBlockRelStart = offBlockStart - iBlockGroup * pThis->cBlocksPerGroup;

        if (   off < pThis->pBlkGrpDesc->offStart
            || off > pThis->pBlkGrpDesc->offLast)
        {
            /* Load new block descriptor. */
            rc = rtFsExtLoadBlkGrpDesc(pThis, iBlockGroup);
            if (RT_FAILURE(rc))
                break;
        }

        size_t cbThis = RT_MIN(cb, pThis->pBlkGrpDesc->offLast - off + 1);
        if (rtFsExtIsBlockRangeInUse(pThis->pBlkGrpDesc,
                                     offBlockRelStart,
                                     cbThis / pThis->cbBlock + (cbThis % pThis->cbBlock ? 1 : 0)) )
        {
            *pfUsed = true;
            break;
        }

        cb  -= cbThis;
        off += cbThis;
    }

    return rc;
}


DECL_HIDDEN_CONST(const RTVFSOPS) g_rtFsExtVolOps =
{
    /* .Obj = */
    {
        /* .uVersion = */       RTVFSOBJOPS_VERSION,
        /* .enmType = */        RTVFSOBJTYPE_VFS,
        /* .pszName = */        "ExtVol",
        /* .pfnClose = */       rtFsExtVol_Close,
        /* .pfnQueryInfo = */   rtFsExtVol_QueryInfo,
        /* .uEndMarker = */     RTVFSOBJOPS_VERSION
    },
    /* .uVersion = */           RTVFSOPS_VERSION,
    /* .fFeatures = */          0,
    /* .pfnOpenRoot = */        rtFsExtVol_OpenRoot,
    /* .pfnQueryRangeState = */ rtFsExtVol_QueryRangeState,
    /* .uEndMarker = */         RTVFSOPS_VERSION
};


RTDECL(int) RTFsExtVolOpen(RTVFSFILE hVfsFileIn, uint32_t fMntFlags, uint32_t fExtFlags, PRTVFS phVfs, PRTERRINFO pErrInfo)
{
    AssertPtrReturn(phVfs, VERR_INVALID_POINTER);
    AssertReturn(!(fMntFlags & ~RTVFSMNT_F_VALID_MASK), VERR_INVALID_FLAGS);
    AssertReturn(!fExtFlags, VERR_INVALID_FLAGS);

    uint32_t cRefs = RTVfsFileRetain(hVfsFileIn);
    AssertReturn(cRefs != UINT32_MAX, VERR_INVALID_HANDLE);

    PRTFSEXTVOL pThis;
    int rc = RTVfsNew(&g_rtFsExtVolOps, sizeof(*pThis), NIL_RTVFS, RTVFSLOCK_CREATE_RW, phVfs, (void **)&pThis);
    if (RT_SUCCESS(rc))
    {
        pThis->hVfsBacking = hVfsFileIn;
        pThis->pBlkGrpDesc = NULL;

        EXTSUPERBLOCK SuperBlock;
        rc = RTVfsFileReadAt(hVfsFileIn, 1024, &SuperBlock, sizeof(EXTSUPERBLOCK), NULL);
        if (RT_SUCCESS(rc))
        {
#if defined(RT_BIGENDIAN)
            /** @todo Convert to host endianess. */
#endif
            if (SuperBlock.u16FilesystemState == EXT_STATE_ERRORS)
                rc = RTERRINFO_LOG_SET(pErrInfo, VERR_FILESYSTEM_CORRUPT, "EXT_STATE_ERRORS");
            else
            {
                pThis->iSbBlock = SuperBlock.iBlockOfSuperblock;
                pThis->cbBlock = 1024 << SuperBlock.cBitsShiftLeftBlockSize;
                pThis->cBlocksPerGroup = SuperBlock.cBlocksPerGroup;
                pThis->cBlockGroups = SuperBlock.cBlocksTotal / pThis->cBlocksPerGroup;

                /* Load first block group descriptor. */
                rc = rtFsExtLoadBlkGrpDesc(pThis, 0);
            }
            if (RT_SUCCESS(rc))
            {
                return VINF_SUCCESS;
            }
        }
        else
            rc = RTERRINFO_LOG_SET(pErrInfo, rc, "Error reading super block");

        RTVfsRelease(*phVfs);
        *phVfs = NIL_RTVFS;
    }
    else
        RTVfsFileRelease(hVfsFileIn);

    return rc;
}


/**
 * @interface_method_impl{RTVFSCHAINELEMENTREG,pfnValidate}
 */
static DECLCALLBACK(int) rtVfsChainExtVol_Validate(PCRTVFSCHAINELEMENTREG pProviderReg, PRTVFSCHAINSPEC pSpec,
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
static DECLCALLBACK(int) rtVfsChainExtVol_Instantiate(PCRTVFSCHAINELEMENTREG pProviderReg, PCRTVFSCHAINSPEC pSpec,
                                                      PCRTVFSCHAINELEMSPEC pElement, RTVFSOBJ hPrevVfsObj,
                                                      PRTVFSOBJ phVfsObj, uint32_t *poffError, PRTERRINFO pErrInfo)
{
    RT_NOREF(pProviderReg, pSpec, poffError);

    int         rc;
    RTVFSFILE   hVfsFileIn = RTVfsObjToFile(hPrevVfsObj);
    if (hVfsFileIn != NIL_RTVFSFILE)
    {
        RTVFS hVfs;
        rc = RTFsExtVolOpen(hVfsFileIn, (uint32_t)pElement->uProvider, (uint32_t)(pElement->uProvider >> 32), &hVfs, pErrInfo);
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
static DECLCALLBACK(bool) rtVfsChainExtVol_CanReuseElement(PCRTVFSCHAINELEMENTREG pProviderReg,
                                                           PCRTVFSCHAINSPEC pSpec, PCRTVFSCHAINELEMSPEC pElement,
                                                           PCRTVFSCHAINSPEC pReuseSpec, PCRTVFSCHAINELEMSPEC pReuseElement)
{
    RT_NOREF(pProviderReg, pSpec, pReuseSpec);
    if (   pElement->paArgs[0].uProvider == pReuseElement->paArgs[0].uProvider
        || !pReuseElement->paArgs[0].uProvider)
        return true;
    return false;
}


/** VFS chain element 'ext'. */
static RTVFSCHAINELEMENTREG g_rtVfsChainExtVolReg =
{
    /* uVersion = */            RTVFSCHAINELEMENTREG_VERSION,
    /* fReserved = */           0,
    /* pszName = */             "ext",
    /* ListEntry = */           { NULL, NULL },
    /* pszHelp = */             "Open a EXT file system, requires a file object on the left side.\n"
                                "First argument is an optional 'ro' (read-only) or 'rw' (read-write) flag.\n",
    /* pfnValidate = */         rtVfsChainExtVol_Validate,
    /* pfnInstantiate = */      rtVfsChainExtVol_Instantiate,
    /* pfnCanReuseElement = */  rtVfsChainExtVol_CanReuseElement,
    /* uEndMarker = */          RTVFSCHAINELEMENTREG_VERSION
};

RTVFSCHAIN_AUTO_REGISTER_ELEMENT_PROVIDER(&g_rtVfsChainExtVolReg, rtVfsChainExtVolReg);

