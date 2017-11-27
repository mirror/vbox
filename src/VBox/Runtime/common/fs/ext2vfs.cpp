/* $Id$ */
/** @file
 * IPRT Filesystem API (FileSys) - ext2/3 format.
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

#include <iprt/assert.h>
#include <iprt/log.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/vfs.h>
#include <iprt/vfslowlevel.h>
#include <iprt/formats/ext2.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Cached block group descriptor data.
 */
typedef struct RTFILESYSTEMEXTBLKGRP
{
    /** Start offset (in bytes and from the start of the disk). */
    uint64_t    offStart;
    /** Last offset in the block group (inclusive). */
    uint64_t    offLast;
    /** Block bitmap - variable in size (depends on the block size
     * and number of blocks per group). */
    uint8_t     abBlockBitmap[1];
} RTFILESYSTEMEXTBLKGRP;
/** Pointer to block group descriptor data. */
typedef RTFILESYSTEMEXTBLKGRP *PRTFILESYSTEMEXTBLKGRP;

/**
 * Ext2/3 filesystem data.
 */
typedef struct RTFILESYSTEMEXT
{
    /** VFS file handle. */
    RTVFSFILE                 hVfsFile;
    /** Block number of the superblock. */
    uint32_t                  iSbBlock;
    /** Size of one block. */
    size_t                    cbBlock;
    /** Number of blocks in one group. */
    unsigned                  cBlocksPerGroup;
    /** Number of blocks groups in the volume. */
    unsigned                  cBlockGroups;
    /** Cached block group descriptor data. */
    PRTFILESYSTEMEXTBLKGRP    pBlkGrpDesc;
} RTFILESYSTEMEXT;
/** Pointer to the ext filesystem data. */
typedef RTFILESYSTEMEXT *PRTFILESYSTEMEXT;



/**
 * Loads the block descriptor of the given block group from the medium.
 *
 * @returns IPRT status code.
 * @param   pThis    EXT filesystem instance data.
 * @param   iBlkGrp  Block group number to load.
 */
static int rtFsExtLoadBlkGrpDesc(PRTFILESYSTEMEXT pThis, uint32_t iBlkGrp)
{
    int rc = VINF_SUCCESS;
    PRTFILESYSTEMEXTBLKGRP pBlkGrpDesc = pThis->pBlkGrpDesc;
    uint64_t offRead = (pThis->iSbBlock + 1) * pThis->cbBlock;
    EXT2BLOCKGROUPDESC BlkDesc;
    size_t cbBlockBitmap;

    cbBlockBitmap = pThis->cBlocksPerGroup / 8;
    if (pThis->cBlocksPerGroup % 8)
        cbBlockBitmap++;

    if (!pBlkGrpDesc)
    {
        size_t cbBlkDesc = RT_OFFSETOF(RTFILESYSTEMEXTBLKGRP, abBlockBitmap[cbBlockBitmap]);
        pBlkGrpDesc = (PRTFILESYSTEMEXTBLKGRP)RTMemAllocZ(cbBlkDesc);
        if (!pBlkGrpDesc)
            return VERR_NO_MEMORY;
    }

    rc = RTVfsFileReadAt(pThis->hVfsFile, offRead, &BlkDesc, sizeof(BlkDesc), NULL);
    if (RT_SUCCESS(rc))
    {
        pBlkGrpDesc->offStart = pThis->iSbBlock + (uint64_t)iBlkGrp * pThis->cBlocksPerGroup * pThis->cbBlock;
        pBlkGrpDesc->offLast  = pBlkGrpDesc->offStart + pThis->cBlocksPerGroup * pThis->cbBlock;
        rc = RTVfsFileReadAt(pThis->hVfsFile, BlkDesc.offBlockBitmap * pThis->cbBlock,
                             &pBlkGrpDesc->abBlockBitmap[0], cbBlockBitmap, NULL);
    }

    pThis->pBlkGrpDesc = pBlkGrpDesc;

    return rc;
}

static bool rtFsExtIsBlockRangeInUse(PRTFILESYSTEMEXTBLKGRP pBlkGrpDesc, uint32_t offBlockStart, size_t cBlocks)
{
    bool fUsed = false;

    while (cBlocks)
    {
        uint32_t idxByte = offBlockStart / 8;
        uint32_t iBit = offBlockStart % 8;

        if (pBlkGrpDesc->abBlockBitmap[idxByte] & RT_BIT(iBit))
        {
            fUsed = true;
            break;
        }

        cBlocks--;
        offBlockStart++;
    }

    return fUsed;
}


/**
 * @interface_method_impl{RTVFSOBJOPS::Obj,pfnClose}
 */
static DECLCALLBACK(int) rtFsExt2Vol_Close(void *pvThis)
{
    PRTFILESYSTEMEXT pThis = (PRTFILESYSTEMEXT)pvThis;

    if (pThis->pBlkGrpDesc)
        RTMemFree(pThis->pBlkGrpDesc);

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSOBJOPS::Obj,pfnQueryInfo}
 */
static DECLCALLBACK(int) rtFsExt2Vol_QueryInfo(void *pvThis, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    RT_NOREF(pvThis, pObjInfo, enmAddAttr);
    return VERR_WRONG_TYPE;
}


/**
 * @interface_method_impl{RTVFSOBJOPS::Obj,pfnOpenRoot}
 */
static DECLCALLBACK(int) rtFsExt2_OpenRoot(void *pvThis, PRTVFSDIR phVfsDir)
{
    NOREF(pvThis);
    NOREF(phVfsDir);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * @interface_method_impl{RTVFSOBJOPS::Obj,pfnQueryRangeState}
 */
static DECLCALLBACK(int) rtFsExt2_QueryRangeState(void *pvThis, uint64_t off, size_t cb, bool *pfUsed)
{
    int rc = VINF_SUCCESS;
    uint64_t offStart = (uint64_t)off;
    PRTFILESYSTEMEXT pThis = (PRTFILESYSTEMEXT)pvThis;

    *pfUsed = false;

    while (cb > 0)
    {
        bool fUsed;
        uint32_t offBlockStart = (uint32_t)(offStart / pThis->cbBlock);
        uint32_t iBlockGroup = (offBlockStart - pThis->iSbBlock) / pThis->cBlocksPerGroup;
        uint32_t offBlockRelStart = offBlockStart - iBlockGroup * pThis->cBlocksPerGroup;
        size_t cbThis = 0;

        if (   offStart < pThis->pBlkGrpDesc->offStart
            || offStart > pThis->pBlkGrpDesc->offLast)
        {
            /* Load new block descriptor. */
            rc = rtFsExtLoadBlkGrpDesc(pThis, iBlockGroup);
            if (RT_FAILURE(rc))
                break;
        }

        cbThis = RT_MIN(cb, pThis->pBlkGrpDesc->offLast - offStart + 1);
        fUsed = rtFsExtIsBlockRangeInUse(pThis->pBlkGrpDesc, offBlockRelStart,
                                         cbThis / pThis->cbBlock +
                                         (cbThis % pThis->cbBlock ? 1 : 0));

        if (fUsed)
        {
            *pfUsed = true;
            break;
        }

        cb       -= cbThis;
        offStart += cbThis;
    }

    return rc;
}


DECL_HIDDEN_CONST(const RTVFSOPS) g_rtFsExt2VolOps =
{
    /* .Obj = */
    {
        /* .uVersion = */       RTVFSOBJOPS_VERSION,
        /* .enmType = */        RTVFSOBJTYPE_VFS,
        /* .pszName = */        "Ext2Vol",
        /* .pfnClose = */       rtFsExt2Vol_Close,
        /* .pfnQueryInfo = */   rtFsExt2Vol_QueryInfo,
        /* .uEndMarker = */     RTVFSOBJOPS_VERSION
    },
    /* .uVersion = */           RTVFSOPS_VERSION,
    /* .fFeatures = */          0,
    /* .pfnOpenRoot = */        rtFsExt2_OpenRoot,
    /* .pfnQueryRangeState = */ rtFsExt2_QueryRangeState,
    /* .uEndMarker = */         RTVFSOPS_VERSION
};


RTDECL(int) RTFsExt2VolOpen(RTVFSFILE hVfsFileIn, uint32_t fMntFlags, uint32_t fExtFlags, PRTVFS phVfs, PRTERRINFO pErrInfo)
{
    AssertPtrReturn(phVfs, VERR_INVALID_POINTER);
    AssertReturn(!(fMntFlags & RTVFSMNT_F_VALID_MASK), VERR_INVALID_FLAGS);
    AssertReturn(!fExtFlags, VERR_INVALID_FLAGS);

    uint32_t cRefs = RTVfsFileRetain(hVfsFileIn);
    AssertReturn(cRefs != UINT32_MAX, VERR_INVALID_HANDLE);

    PRTFILESYSTEMEXT pThis;
    int rc = RTVfsNew(&g_rtFsExt2VolOps, sizeof(*pThis), NIL_RTVFS, RTVFSLOCK_CREATE_RW, phVfs, (void **)&pThis);
    if (RT_SUCCESS(rc))
    {
        pThis->hVfsFile    = hVfsFileIn;
        pThis->pBlkGrpDesc = NULL;

        EXT2SUPERBLOCK SuperBlock;
        rc = RTVfsFileReadAt(hVfsFileIn, 1024, &SuperBlock, sizeof(EXT2SUPERBLOCK), NULL);
        if (RT_SUCCESS(rc))
        {
#if defined(RT_BIGENDIAN)
            /** @todo Convert to host endianess. */
#endif
            if (SuperBlock.u16FilesystemState == EXT2_STATE_ERRORS)
                rc = RTERRINFO_LOG_SET(pErrInfo, VERR_FILESYSTEM_CORRUPT, "EXT2_STATE_ERRORS");
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

