/* $Id$ */
/** @file
 * IPRT - FAT Virtual Filesystem.
 */

/*
 * Copyright (C) 2017 Oracle Corporation
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
#include "internal/iprt.h"
#include <iprt/fs.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/file.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/poll.h>
#include <iprt/string.h>
#include <iprt/sg.h>
#include <iprt/thread.h>
#include <iprt/vfs.h>
#include <iprt/vfslowlevel.h>
#include <iprt/formats/fat.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * A part of the cluster chain covering up to 252 clusters.
 */
typedef struct RTFSFATCHAINPART
{
    /** List entry. */
    RTLISTNODE  ListEntry;
    /** Chain entries. */
    uint32_t    aEntries[256 - 4];
} RTFSFATCHAINPART;
AssertCompile(sizeof(RTFSFATCHAINPART) <= _1K);
typedef RTFSFATCHAINPART *PRTFSFATCHAINPART;
typedef RTFSFATCHAINPART const *PCRTFSFATCHAINPART;

/**
 * A FAT cluster chain.
 */
typedef struct RTFSFATCHAIN
{
    /** The chain size in bytes. */
    uint32_t        cbChain;
    /** The chain size in entries. */
    uint32_t        cClusters;
    /** List of chain parts (RTFSFATCHAINPART). */
    RTLISTANCHOR    ListParts;
} RTFSFATCHAIN;
typedef RTFSFATCHAIN *PRTFSFATCHAIN;
typedef RTFSFATCHAIN const *PCRTFSFATCHAIN;


/**
 * FAT file system object (common part to files and dirs).
 */
typedef struct RTFSFATOBJ
{
    /** The parent directory keeps a list of open objects (RTFSFATOBJ). */
    RTLISTNODE          Entry;
    /** The byte offset of the directory entry.
     *  This is set to UINT64_MAX if special FAT12/16 root directory. */
    uint64_t            offDirEntry;
    /** Attributes. */
    RTFMODE             fAttrib;
    /** The object size. */
    uint32_t            cbObject;
    /** The access time. */
    RTTIMESPEC          AccessTime;
    /** The modificaton time. */
    RTTIMESPEC          ModificationTime;
    /** The birth time. */
    RTTIMESPEC          BirthTime;
    /** Cluster chain. */
    RTFSFATCHAIN        Clusters;
    /** Pointer to the volume. */
    struct RTFSFATVOL  *pVol;
} RTFSFATOBJ;
typedef RTFSFATOBJ *PRTFSFATOBJ;

typedef struct RTFSFATFILE
{
    /** Core FAT object info.  */
    RTFSFATOBJ      Core;
    /** The current file offset. */
    uint32_t        offFile;
} RTFSFATFILE;
typedef RTFSFATFILE *PRTFSFATFILE;


/**
 * FAT directory.
 *
 * We work directories in one of two buffering modes.  If there are few entries
 * or if it's the FAT12/16 root directory, we map the whole thing into memory.
 * If it's too large, we use an inefficient sector buffer for now.
 *
 * Directory entry updates happens exclusively via the directory, so any open
 * files or subdirs have a parent reference for doing that.  The parent OTOH,
 * keeps a list of open children.
 */
typedef struct RTFSFATDIR
{
    /** Core FAT object info.  */
    RTFSFATOBJ          Core;
    /** Open child objects (RTFSFATOBJ). */
    RTLISTNODE          OpenChildren;

    /** Number of directory entries. */
    uint32_t            cEntries;

    /** If fully buffered. */
    bool                fFullyBuffered;
    /** Set if this is a linear root directory. */
    bool                fIsFlatRootDir;

    union
    {
        /** Data for the full buffered mode.
         * No need to messing around with clusters here, as we only uses this for
         * directories with a contiguous mapping on the disk.
         * So, if we grow a directory in a non-contiguous manner, we have to switch
         * to sector buffering on the fly. */
        struct
        {
            /** Directory offset. */
            uint64_t            offDir;
            /** Number of sectors mapped by paEntries and pbDirtySectors. */
            uint32_t            cSectors;
            /** Number of dirty sectors. */
            uint32_t            cDirtySectors;
            /** Pointer to the linear mapping of the directory entries. */
            PFATDIRENTRYUNION   paEntries;
            /** Dirty sector map. */
            uint8_t            *pbDirtySectors;
        } Full;
        /** The simple sector buffering.
         * This only works for clusters, so no FAT12/16 root directory fun. */
        struct
        {
            /** The disk offset of the current sector, UINT64_MAX if invalid. */
            uint64_t            offOnDisk;
            /** The directory offset, UINT32_MAX if invalid. */
            uint32_t            offInDir;
            uint32_t            u32Reserved; /**< Puts pbSector and paEntries at the same location */
            /** Sector buffer. */
            PFATDIRENTRYUNION  *paEntries;
            /** Dirty flag. */
            bool                fDirty;
        } Simple;
    } u;
} RTFSFATDIR;
/** Pointer to a FAT directory instance. */
typedef RTFSFATDIR *PRTFSFATDIR;


/**
 * File allocation table cache entry.
 */
typedef struct RTFSFATCLUSTERMAPENTRY
{
    /** The byte offset into the fat, UINT32_MAX if invalid entry. */
    uint32_t                offFat;
    /** Pointer to the data. */
    uint8_t                *pbData;
    /** Dirty bitmap.  Indexed by byte offset right shifted by
     * RTFSFATCLUSTERMAPCACHE::cDirtyShift. */
    uint64_t                bmDirty;
} RTFSFATCLUSTERMAPENTRY;
/** Pointer to a file allocation table cache entry.  */
typedef RTFSFATCLUSTERMAPENTRY *PRTFSFATCLUSTERMAPENTRY;

/**
 * File allocation table cache.
 */
typedef struct RTFSFATCLUSTERMAPCACHE
{
    /** Number of cache entries. */
    uint32_t                cEntries;
    /** The max size of data in a cache entry. */
    uint32_t                cbEntry;
    /** Dirty bitmap shift count. */
    uint32_t                cDirtyShift;
    /** The dirty cache line size (multiple of two). */
    uint32_t                cbDirtyLine;
    /** The cache name. */
    const char             *pszName;
    /** Cache entries. */
    RTFSFATCLUSTERMAPENTRY  aEntries[RT_FLEXIBLE_ARRAY];
} RTFSFATCLUSTERMAPCACHE;
/** Pointer to a FAT linear metadata cache. */
typedef RTFSFATCLUSTERMAPCACHE *PRTFSFATCLUSTERMAPCACHE;


/**
 * FAT type (format).
 */
typedef enum RTFSFATTYPE
{
    RTFSFATTYPE_INVALID = 0,
    RTFSFATTYPE_FAT12,
    RTFSFATTYPE_FAT16,
    RTFSFATTYPE_FAT32,
    RTFSFATTYPE_END
} RTFSFATTYPE;

/**
 * A FAT volume.
 */
typedef struct RTFSFATVOL
{
    /** Handle to itself. */
    RTVFS           hVfsSelf;
    /** The file, partition, or whatever backing the FAT volume. */
    RTVFSFILE       hVfsBacking;
    /** The size of the backing thingy. */
    uint64_t        cbBacking;
    /** Byte offset of the bootsector relative to the start of the file. */
    uint64_t        offBootSector;
    /** Set if read-only mode. */
    bool            fReadOnly;
    /** Media byte. */
    uint8_t         bMedia;
    /** Reserved sectors. */
    uint32_t        cReservedSectors;

    /** Logical sector size. */
    uint32_t        cbSector;
    /** The cluster size in bytes. */
    uint32_t        cbCluster;
    /** The number of data clusters, including the two reserved ones. */
    uint32_t        cClusters;
    /** The offset of the first cluster. */
    uint64_t        offFirstCluster;
    /** The total size from the BPB, in bytes. */
    uint64_t        cbTotalSize;

    /** The FAT type. */
    RTFSFATTYPE     enmFatType;

    /** Number of FAT entries (clusters). */
    uint32_t        cFatEntries;
    /** The size of a FAT, in bytes. */
    uint32_t        cbFat;
    /** Number of FATs. */
    uint32_t        cFats;
    /** The end of chain marker used by the formatter (FAT entry \#2). */
    uint32_t        idxEndOfChain;
    /** The maximum last cluster supported by the FAT format. */
    uint32_t        idxMaxLastCluster;
    /** FAT byte offsets.  */
    uint64_t        aoffFats[8];
    /** Pointer to the FAT (cluster map) cache. */
    PRTFSFATCLUSTERMAPCACHE pFatCache;

    /** The root directory byte offset. */
    uint64_t        offRootDir;
    /** Root directory cluster, UINT32_MAX if not FAT32. */
    uint32_t        idxRootDirCluster;
    /** Number of root directory entries, if fixed.  UINT32_MAX for FAT32. */
    uint32_t        cRootDirEntries;
    /** The size of the root directory, rounded up to the nearest sector size. */
    uint32_t        cbRootDir;
    /** The root directory handle. */
    RTVFSDIR        hVfsRootDir;
    /** The root directory instance data. */
    PRTFSFATDIR     pRootDir;

    /** Serial number. */
    uint32_t        uSerialNo;
    /** The stripped volume label, if included in EBPB. */
    char            szLabel[12];
    /** The file system type from the EBPB (also stripped).  */
    char            szType[9];
    /** Number of FAT32 boot sector copies.   */
    uint8_t         cBootSectorCopies;
    /** FAT32 flags. */
    uint16_t        fFat32Flags;
    /** Offset of the FAT32 boot sector copies, UINT64_MAX if none. */
    uint64_t        offBootSectorCopies;

    /** The FAT32 info sector byte offset, UINT64_MAX if not present. */
    uint64_t        offFat32InfoSector;
    /** The FAT32 info sector if offFat32InfoSector isn't UINT64_MAX. */
    FAT32INFOSECTOR Fat32InfoSector;
} RTFSFATVOL;
/** Pointer to a FAT volume (VFS instance data). */
typedef RTFSFATVOL *PRTFSFATVOL;



/**
 * Checks if the cluster chain is contiguous on the disk.
 *
 * @returns true / false.
 * @param   pChain              The chain.
 */
static bool rtFsFatChain_IsContiguous(PCRTFSFATCHAIN pChain)
{
    if (pChain->cClusters <= 1)
        return true;

    PRTFSFATCHAINPART   pPart   = RTListGetFirst(&pChain->ListParts, RTFSFATCHAINPART, ListEntry);
    uint32_t            idxNext = pPart->aEntries[0];
    uint32_t            cLeft   = pChain->cClusters;
    for (;;)
    {
        uint32_t const cInPart = RT_MIN(cLeft, RT_ELEMENTS(pPart->aEntries));
        for (uint32_t iPart = 0; iPart < cInPart; iPart++)
            if (pPart->aEntries[iPart] == idxNext)
                idxNext++;
            else
                return false;
        cLeft -= cInPart;
        if (!cLeft)
            return true;
        pPart = RTListGetNext(&pChain->ListParts, pPart, RTFSFATCHAINPART, ListEntry);
    }
}



/**
 * Creates a cache for the file allocation table (cluster map).
 *
 * @returns Pointer to the cache.
 * @param   pThis               The FAT volume instance.
 * @param   pbFirst512FatBytes  The first 512 bytes of the first FAT.
 */
static int rtFsFatClusterMap_Create(PRTFSFATVOL pThis, uint8_t const *pbFirst512FatBytes, PRTERRINFO pErrInfo)
{
    Assert(RT_ALIGN_32(pThis->cbFat, pThis->cbSector) == pThis->cbFat);
    Assert(pThis->cbFat != 0);

    /*
     * Figure the cache size.  Keeping it _very_ simple for now as we just need
     * something that works, not anything the performs like crazy.
     */
    uint32_t cEntries;
    uint32_t cbEntry = pThis->cbFat;
    if (cbEntry <= _512K)
        cEntries = 1;
    else
    {
        Assert(pThis->cbSector < _512K / 8);
        cEntries = 8;
        cbEntry  = pThis->cbSector;
    }

    /*
     * Allocate and initialize it all.
     */
    PRTFSFATCLUSTERMAPCACHE pCache;
    pThis->pFatCache = pCache = (PRTFSFATCLUSTERMAPCACHE)RTMemAllocZ(RT_OFFSETOF(RTFSFATCLUSTERMAPCACHE, aEntries[cEntries]));
    if (!pCache)
        return RTErrInfoSet(pErrInfo, VERR_NO_MEMORY, "Failed to allocate FAT cache");
    pCache->cEntries  = cEntries;
    pCache->cbEntry   = cbEntry;

    unsigned i = cEntries;
    while (i-- > 0)
    {
        pCache->aEntries[i].pbData = (uint8_t *)RTMemAlloc(cbEntry);
        if (pCache->aEntries[i].pbData == NULL)
        {
            for (i++; i < cEntries; i++)
                RTMemFree(pCache->aEntries[i].pbData);
            RTMemFree(pCache);
            return RTErrInfoSetF(pErrInfo, VERR_NO_MEMORY, "Failed to allocate FAT cache entry (%#x bytes)", cbEntry);
        }

        pCache->aEntries[i].offFat  = UINT32_MAX;
        pCache->aEntries[i].bmDirty = 0;
    }

    /*
     * Calc the dirty shift factor.
     */
    cbEntry /= 64;
    if (cbEntry < pThis->cbSector)
        cbEntry = pThis->cbSector;

    pCache->cDirtyShift = 1;
    pCache->cbDirtyLine = 1;
    while (pCache->cbDirtyLine < cbEntry)
    {
        pCache->cDirtyShift++;
        pCache->cbDirtyLine <<= 1;
    }

    /*
     * Fill the cache if single entry or entry size is 512.
     */
    if (pCache->cEntries == 1 || pCache->cbEntry == 512)
    {
        memcpy(pCache->aEntries[0].pbData, pbFirst512FatBytes, RT_MIN(512, pCache->cbEntry));
        if (pCache->cbEntry > 512)
        {
            int rc = RTVfsFileReadAt(pThis->hVfsBacking, pThis->aoffFats[0] + 512,
                                     &pCache->aEntries[0].pbData[512], pCache->cbEntry - 512, NULL);
            if (RT_FAILURE(rc))
                return RTErrInfoSet(pErrInfo, rc, "Error reading FAT into memory");
        }
        pCache->aEntries[0].offFat  = 0;
        pCache->aEntries[0].bmDirty = 0;
    }

    return VINF_SUCCESS;
}


/**
 * Worker for rtFsFatClusterMap_Flush and rtFsFatClusterMap_FlushEntry.
 *
 * @returns IPRT status code.  On failure, we're currently kind of screwed.
 * @param   pThis       The FAT volume instance.
 * @param   iFirstEntry Entry to start flushing at.
 * @param   iLastEntry  Last entry to flush.
 */
static int rtFsFatClusterMap_FlushWorker(PRTFSFATVOL pThis, uint32_t const iFirstEntry, uint32_t const iLastEntry)
{
    PRTFSFATCLUSTERMAPCACHE pCache = pThis->pFatCache;

    /*
     * Walk the cache entries, accumulating segments to flush.
     */
    int      rc      = VINF_SUCCESS;
    uint64_t off     = UINT64_MAX;
    uint64_t offEdge = UINT64_MAX;
    RTSGSEG  aSgSegs[8];
    RTSGBUF  SgBuf;
    RTSgBufInit(&SgBuf, aSgSegs, RT_ELEMENTS(aSgSegs));
    SgBuf.cSegs = 0; /** @todo RTSgBuf API is stupid, make it smarter. */

    for (uint32_t iFatCopy = 0; iFatCopy < pThis->cFats; iFatCopy++)
    {
        for (uint32_t iEntry = iFirstEntry; iEntry <= iLastEntry; iEntry++)
        {
            uint64_t bmDirty = pCache->aEntries[iEntry].bmDirty;
            if (   bmDirty != 0
                && pCache->aEntries[iEntry].offFat != UINT32_MAX)
            {
                uint32_t offEntry   = 0;
                uint64_t iDirtyLine = 1;
                while (offEntry < pCache->cbEntry)
                {
                    if (pCache->aEntries[iEntry].bmDirty & iDirtyLine)
                    {
                        /*
                         * Found dirty cache line.
                         */
                        uint64_t offDirtyLine = pThis->aoffFats[iFatCopy] + pCache->aEntries[iEntry].offFat + offEntry;

                        /* Can we simply extend the last segment? */
                        if (   offDirtyLine == offEdge
                            && offEntry)
                        {
                            Assert(   (uintptr_t)aSgSegs[SgBuf.cSegs - 1].pvSeg + aSgSegs[SgBuf.cSegs - 1].cbSeg
                                   == (uintptr_t)&pCache->aEntries[iEntry].pbData[offEntry]);
                            aSgSegs[SgBuf.cSegs - 1].cbSeg += pCache->cbDirtyLine;
                            offEdge += pCache->cbDirtyLine;
                        }
                        else
                        {
                            /* flush if not adjacent or if we're out of segments. */
                            if (   offDirtyLine != offEdge
                                || SgBuf.cSegs >= RT_ELEMENTS(aSgSegs))
                            {
                                int rc2 = RTVfsFileSgWrite(pThis->hVfsBacking, off, &SgBuf, true /*fBlocking*/, NULL);
                                if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
                                    rc = rc2;
                                RTSgBufReset(&SgBuf);
                                SgBuf.cSegs = 0;
                            }

                            /* Append segment. */
                            aSgSegs[SgBuf.cSegs].cbSeg = pCache->cbDirtyLine;
                            aSgSegs[SgBuf.cSegs].pvSeg = &pCache->aEntries[iEntry].pbData[offEntry];
                            SgBuf.cSegs++;
                            offEdge = offDirtyLine + pCache->cbDirtyLine;
                        }

                        bmDirty &= ~iDirtyLine;
                        if (!bmDirty)
                            break;
                    }
                }
                iDirtyLine++;
                offEntry += pCache->cbDirtyLine;
            }
        }
    }

    /*
     * Final flush job.
     */
    if (SgBuf.cSegs > 0)
    {
        int rc2 = RTVfsFileSgWrite(pThis->hVfsBacking, off, &SgBuf, true /*fBlocking*/, NULL);
        if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
            rc = rc2;
    }

    /*
     * Clear the dirty flags on success.
     */
    if (RT_SUCCESS(rc))
        for (uint32_t iEntry = iFirstEntry; iEntry <= iLastEntry; iEntry++)
            pCache->aEntries[iEntry].bmDirty = 0;

    return rc;
}


/**
 * Flushes out all dirty lines in the entire file allocation table cache.
 *
 * @returns IPRT status code.  On failure, we're currently kind of screwed.
 * @param   pThis       The FAT volume instance.
 */
static int rtFsFatClusterMap_Flush(PRTFSFATVOL pThis)
{
    return rtFsFatClusterMap_FlushWorker(pThis, 0, pThis->pFatCache->cEntries - 1);
}


/**
 * Flushes out all dirty lines in the file allocation table (cluster map) cache.
 *
 * This is typically called prior to reusing the cache entry.
 *
 * @returns IPRT status code.  On failure, we're currently kind of screwed.
 * @param   pThis       The FAT volume instance.
 * @param   iEntry      The cache entry to flush.
 */
static int rtFsFatClusterMap_FlushEntry(PRTFSFATVOL pThis, uint32_t iEntry)
{
    return rtFsFatClusterMap_FlushWorker(pThis, iEntry, iEntry);
}


/**
 * Destroys the file allcation table cache, first flushing any dirty lines.
 *
 * @returns IRPT status code from flush (we've destroyed it regardless of the
 *          status code).
 * @param   pThis       The FAT volume instance which cluster map shall be
 *                      destroyed.
 */
static int rtFsFatClusterMap_Destroy(PRTFSFATVOL pThis)
{
    int                     rc     = VINF_SUCCESS;
    PRTFSFATCLUSTERMAPCACHE pCache = pThis->pFatCache;
    if (pCache)
    {
        /* flush stuff. */
        rc = rtFsFatClusterMap_Flush(pThis);

        /* free everything. */
        uint32_t i = pCache->cEntries;
        while (i-- > 0)
        {
            RTMemFree(pCache->aEntries[i].pbData);
            pCache->aEntries[i].pbData = NULL;
        }
        pCache->cEntries = 0;
        RTMemFree(pCache);

        pThis->pFatCache = NULL;
    }

    return rc;
}


/**
 * Reads a cluster chain into memory
 *
 * @returns IPRT status code.
 * @param   pThis           The FAT volume instance.
 * @param   idxFirstCluster The first cluster.
 * @param   pChain          The chain element to read into (and thereby
 *                          initialize).
 */
static int rtFsFatClusterMap_ReadClusterChain(PRTFSFATVOL pThis, uint32_t idxFirstCluster, PRTFSFATCHAIN pChain)
{
    pChain->cClusters = 0;
    pChain->cbChain   = 0;
    RTListInit(&pChain->ListParts);

    if (   idxFirstCluster >= pThis->cClusters
        && idxFirstCluster >= FAT_FIRST_DATA_CLUSTER)
        return VERR_VFS_BOGUS_OFFSET;

    return VERR_NOT_IMPLEMENTED;
}


static void rtFsFatDosDateTimeToSpec(PRTTIMESPEC pTimeSpec, uint16_t uDate, uint16_t uTime, uint8_t cCentiseconds)
{
    RTTIME Time;
    Time.i32Year    = 1980 + (uDate >> 9);
    Time.u8Month    = ((uDate >> 5) & 0xf);
    Time.u8WeekDay  = UINT8_MAX;
    Time.u16YearDay = UINT16_MAX;
    Time.u8MonthDay = RT_MAX(uDate & 0x1f, 1);
    Time.u8Hour     = uTime >> 11;
    Time.u8Minute   = (uTime >> 5) & 0x3f;
    Time.u8Second   = (uTime & 0x1f) << 1;
    if (cCentiseconds > 0 && cCentiseconds < 200) /* screw complicated stuff for now. */
    {
        if (cCentiseconds >= 100)
        {
            cCentiseconds -= 100;
            Time.u8Second++;
        }
        Time.u32Nanosecond = cCentiseconds * UINT64_C(100000000);
    }

    RTTimeImplode(pTimeSpec, RTTimeNormalize(&Time));
}


static void rtFsFatObj_InitFromDirEntry(PRTFSFATOBJ pObj, PCFATDIRENTRY pDirEntry, uint64_t offDirEntry, PRTFSFATVOL pThis)
{
    RTListInit(&pObj->Entry);
    pObj->pVol          = pThis;
    pObj->offDirEntry   = offDirEntry;
    pObj->fAttrib       = ((RTFMODE)pDirEntry->fAttrib << RTFS_DOS_SHIFT) & RTFS_DOS_MASK_OS2;
    pObj->cbObject      = pDirEntry->cbFile;
    rtFsFatDosDateTimeToSpec(&pObj->ModificationTime, pDirEntry->uAccessDate, pDirEntry->uModifyTime, 0);
    rtFsFatDosDateTimeToSpec(&pObj->BirthTime, pDirEntry->uBirthDate, pDirEntry->uBirthTime, pDirEntry->uBirthCentiseconds);
    rtFsFatDosDateTimeToSpec(&pObj->AccessTime, pDirEntry->uAccessDate, 0, 0);
}


static void rtFsFatObj_InitDummy(PRTFSFATOBJ pObj, uint64_t offDirEntry, uint32_t cbObject, RTFMODE fAttrib, PRTFSFATVOL pThis)
{
    RTListInit(&pObj->Entry);
    pObj->pVol          = pThis;
    pObj->offDirEntry   = offDirEntry;
    pObj->fAttrib       = fAttrib;
    pObj->cbObject      = cbObject;
    RTTimeSpecSetDosSeconds(&pObj->AccessTime, 0);
    RTTimeSpecSetDosSeconds(&pObj->ModificationTime, 0);
    RTTimeSpecSetDosSeconds(&pObj->BirthTime, 0);
}


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnClose}
 */
static DECLCALLBACK(int) rtFsFatFile_Close(void *pvThis)
{
    PRTFSFATFILE pThis = (PRTFSFATFILE)pvThis;
    RT_NOREF(pThis);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnQueryInfo}
 */
static DECLCALLBACK(int) rtFsFatObj_QueryInfo(void *pvThis, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    PRTFSFATFILE pThis = (PRTFSFATFILE)pvThis;

    pObjInfo->cbObject              = pThis->Core.cbObject;
    pObjInfo->cbAllocated           = pThis->Core.Clusters.cClusters * pThis->Core.pVol->cbCluster;
    pObjInfo->AccessTime            = pThis->Core.AccessTime;
    pObjInfo->ModificationTime      = pThis->Core.ModificationTime;
    pObjInfo->ChangeTime            = pThis->Core.ModificationTime;
    pObjInfo->BirthTime             = pThis->Core.BirthTime;
    pObjInfo->Attr.fMode            = pThis->Core.fAttrib;
    pObjInfo->Attr.enmAdditional    = enmAddAttr;

    switch (enmAddAttr)
    {
        case RTFSOBJATTRADD_NOTHING: /* fall thru */
        case RTFSOBJATTRADD_UNIX:
            pObjInfo->Attr.u.Unix.uid           = NIL_RTUID;
            pObjInfo->Attr.u.Unix.gid           = NIL_RTGID;
            pObjInfo->Attr.u.Unix.cHardlinks    = 1;
            pObjInfo->Attr.u.Unix.INodeIdDevice = 0;
            pObjInfo->Attr.u.Unix.INodeId       = 0; /* Could probably use the directory entry offset. */
            pObjInfo->Attr.u.Unix.fFlags        = 0;
            pObjInfo->Attr.u.Unix.GenerationId  = 0;
            pObjInfo->Attr.u.Unix.Device        = 0;
            break;
        case RTFSOBJATTRADD_UNIX_OWNER:
            pObjInfo->Attr.u.UnixOwner.uid       = 0;
            pObjInfo->Attr.u.UnixOwner.szName[0] = '\0';
            break;
        case RTFSOBJATTRADD_UNIX_GROUP:
            pObjInfo->Attr.u.UnixGroup.gid       = 0;
            pObjInfo->Attr.u.UnixGroup.szName[0] = '\0';
            break;
        case RTFSOBJATTRADD_EASIZE:
            pObjInfo->Attr.u.EASize.cb = 0;
            break;
        default:
            return VERR_INVALID_PARAMETER;
    }
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnRead}
 */
static DECLCALLBACK(int) rtFsFatFile_Read(void *pvThis, RTFOFF off, PCRTSGBUF pSgBuf, bool fBlocking, size_t *pcbRead)
{
    PRTFSFATFILE pThis = (PRTFSFATFILE)pvThis;
    RT_NOREF(pThis, off, pSgBuf, fBlocking, pcbRead);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnWrite}
 */
static DECLCALLBACK(int) rtFsFatFile_Write(void *pvThis, RTFOFF off, PCRTSGBUF pSgBuf, bool fBlocking, size_t *pcbWritten)
{
    PRTFSFATFILE pThis = (PRTFSFATFILE)pvThis;
    RT_NOREF(pThis, off, pSgBuf, fBlocking, pcbWritten);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnFlush}
 */
static DECLCALLBACK(int) rtFsFatFile_Flush(void *pvThis)
{
    PRTFSFATFILE pThis = (PRTFSFATFILE)pvThis;
    RT_NOREF(pThis);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnPollOne}
 */
static DECLCALLBACK(int) rtFsFatFile_PollOne(void *pvThis, uint32_t fEvents, RTMSINTERVAL cMillies, bool fIntr,
                                             uint32_t *pfRetEvents)
{
    NOREF(pvThis);
    int rc;
    if (fEvents != RTPOLL_EVT_ERROR)
    {
        *pfRetEvents = fEvents & ~RTPOLL_EVT_ERROR;
        rc = VINF_SUCCESS;
    }
    else if (fIntr)
        rc = RTThreadSleep(cMillies);
    else
    {
        uint64_t uMsStart = RTTimeMilliTS();
        do
            rc = RTThreadSleep(cMillies);
        while (   rc == VERR_INTERRUPTED
               && !fIntr
               && RTTimeMilliTS() - uMsStart < cMillies);
        if (rc == VERR_INTERRUPTED)
            rc = VERR_TIMEOUT;
    }
    return rc;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnTell}
 */
static DECLCALLBACK(int) rtFsFatFile_Tell(void *pvThis, PRTFOFF poffActual)
{
    PRTFSFATFILE pThis = (PRTFSFATFILE)pvThis;
    *poffActual = pThis->offFile;
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnMode}
 */
static DECLCALLBACK(int) rtFsFatObj_SetMode(void *pvThis, RTFMODE fMode, RTFMODE fMask)
{
#if 0
    PRTFSFATFILE pThis = (PRTFSFATFILE)pvThis;
    if (fMask != ~RTFS_TYPE_MASK)
    {
        fMode |= ~fMask & ObjInfo.Attr.fMode;
    }
#else
    RT_NOREF(pvThis, fMode, fMask);
    return VERR_NOT_IMPLEMENTED;
#endif
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnSetTimes}
 */
static DECLCALLBACK(int) rtFsFatObj_SetTimes(void *pvThis, PCRTTIMESPEC pAccessTime, PCRTTIMESPEC pModificationTime,
                                             PCRTTIMESPEC pChangeTime, PCRTTIMESPEC pBirthTime)
{
#if 0
    PRTFSFATFILE pThis = (PRTFSFATFILE)pvThis;
#else
    RT_NOREF(pvThis, pAccessTime, pModificationTime, pChangeTime, pBirthTime);
    return VERR_NOT_IMPLEMENTED;
#endif
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnSetOwner}
 */
static DECLCALLBACK(int) rtFsFatObj_SetOwner(void *pvThis, RTUID uid, RTGID gid)
{
    RT_NOREF(pvThis, uid, gid);
    return VERR_NOT_SUPPORTED;
}


/**
 * @interface_method_impl{RTVFSFILEOPS,pfnSeek}
 */
static DECLCALLBACK(int) rtFsFatFile_Seek(void *pvThis, RTFOFF offSeek, unsigned uMethod, PRTFOFF poffActual)
{
    PRTFSFATFILE pThis = (PRTFSFATFILE)pvThis;
    RTFOFF offNew;
    switch (uMethod)
    {
        case RTFILE_SEEK_BEGIN:
            offNew = offSeek;
            break;
        case RTFILE_SEEK_END:
            offNew = (RTFOFF)pThis->Core.cbObject + offSeek;
            break;
        case RTFILE_SEEK_CURRENT:
            offNew = (RTFOFF)pThis->offFile + offSeek;
            break;
        default:
            return VERR_INVALID_PARAMETER;
    }
    if (offNew >= 0)
    {
        if (offNew <= _4G)
        {
            pThis->offFile = offNew;
            *poffActual    = offNew;
            return VINF_SUCCESS;
        }
        return VERR_OUT_OF_RANGE;
    }
    return VERR_NEGATIVE_SEEK;
}


/**
 * @interface_method_impl{RTVFSFILEOPS,pfnQuerySize}
 */
static DECLCALLBACK(int) rtFsFatFile_QuerySize(void *pvThis, uint64_t *pcbFile)
{
    PRTFSFATFILE pThis = (PRTFSFATFILE)pvThis;
    *pcbFile = pThis->Core.cbObject;
    return VINF_SUCCESS;
}


/**
 * FAT file operations.
 */
DECL_HIDDEN_CONST(const RTVFSFILEOPS) g_rtFsFatFileOps =
{
    { /* Stream */
        { /* Obj */
            RTVFSOBJOPS_VERSION,
            RTVFSOBJTYPE_FILE,
            "FatFile",
            rtFsFatFile_Close,
            rtFsFatObj_QueryInfo,
            RTVFSOBJOPS_VERSION
        },
        RTVFSIOSTREAMOPS_VERSION,
        0,
        rtFsFatFile_Read,
        rtFsFatFile_Write,
        rtFsFatFile_Flush,
        rtFsFatFile_PollOne,
        rtFsFatFile_Tell,
        NULL /*pfnSkip*/,
        NULL /*pfnZeroFill*/,
        RTVFSIOSTREAMOPS_VERSION,
    },
    RTVFSFILEOPS_VERSION,
    0,
    { /* ObjSet */
        RTVFSOBJSETOPS_VERSION,
        RT_OFFSETOF(RTVFSFILEOPS, Stream.Obj) - RT_OFFSETOF(RTVFSFILEOPS, ObjSet),
        rtFsFatObj_SetMode,
        rtFsFatObj_SetTimes,
        rtFsFatObj_SetOwner,
        RTVFSOBJSETOPS_VERSION
    },
    rtFsFatFile_Seek,
    rtFsFatFile_QuerySize,
    RTVFSFILEOPS_VERSION
};


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnClose}
 */
static DECLCALLBACK(int) rtFsFatDir_Close(void *pvThis)
{
    PRTFSFATDIR pThis = (PRTFSFATDIR)pvThis;
    RT_NOREF(pThis);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnTraversalOpen}
 */
static DECLCALLBACK(int) rtFsFatDir_TraversalOpen(void *pvThis, const char *pszEntry, PRTVFSDIR phVfsDir,
                                                  PRTVFSSYMLINK phVfsSymlink, PRTVFS phVfsMounted)
{
    RT_NOREF(pvThis, pszEntry, phVfsDir, phVfsSymlink, phVfsMounted);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnOpenFile}
 */
static DECLCALLBACK(int) rtFsFatDir_OpenFile(void *pvThis, const char *pszFilename, uint32_t fOpen, PRTVFSFILE phVfsFile)
{
    RT_NOREF(pvThis, pszFilename, fOpen, phVfsFile);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnOpenDir}
 */
static DECLCALLBACK(int) rtFsFatDir_OpenDir(void *pvThis, const char *pszSubDir, uint32_t fFlags, PRTVFSDIR phVfsDir)
{
    RT_NOREF(pvThis, pszSubDir, fFlags, phVfsDir);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnCreateDir}
 */
static DECLCALLBACK(int) rtFsFatDir_CreateDir(void *pvThis, const char *pszSubDir, RTFMODE fMode, PRTVFSDIR phVfsDir)
{
    RT_NOREF(pvThis, pszSubDir, fMode, phVfsDir);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnOpenSymlink}
 */
static DECLCALLBACK(int) rtFsFatDir_OpenSymlink(void *pvThis, const char *pszSymlink, PRTVFSSYMLINK phVfsSymlink)
{
    RT_NOREF(pvThis, pszSymlink, phVfsSymlink);
    return VERR_NOT_SUPPORTED;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnCreateSymlink}
 */
static DECLCALLBACK(int) rtFsFatDir_CreateSymlink(void *pvThis, const char *pszSymlink, const char *pszTarget,
                                                  RTSYMLINKTYPE enmType, PRTVFSSYMLINK phVfsSymlink)
{
    RT_NOREF(pvThis, pszSymlink, pszTarget, enmType, phVfsSymlink);
    return VERR_NOT_SUPPORTED;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnUnlinkEntry}
 */
static DECLCALLBACK(int) rtFsFatDir_UnlinkEntry(void *pvThis, const char *pszEntry, RTFMODE fType)
{
    RT_NOREF(pvThis, pszEntry, fType);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnRewindDir}
 */
static DECLCALLBACK(int) rtFsFatDir_RewindDir(void *pvThis)
{
    RT_NOREF(pvThis);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnReadDir}
 */
static DECLCALLBACK(int) rtFsFatDir_ReadDir(void *pvThis, PRTDIRENTRYEX pDirEntry, size_t *pcbDirEntry,
                                            RTFSOBJATTRADD enmAddAttr)
{
    RT_NOREF(pvThis, pDirEntry, pcbDirEntry, enmAddAttr);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * FAT file operations.
 */
static const RTVFSDIROPS g_rtFsFatDirOps =
{
    { /* Obj */
        RTVFSOBJOPS_VERSION,
        RTVFSOBJTYPE_FILE,
        "FatDir",
        rtFsFatDir_Close,
        rtFsFatObj_QueryInfo,
        RTVFSOBJOPS_VERSION
    },
    RTVFSDIROPS_VERSION,
    0,
    { /* ObjSet */
        RTVFSOBJSETOPS_VERSION,
        RT_OFFSETOF(RTVFSFILEOPS, Stream.Obj) - RT_OFFSETOF(RTVFSFILEOPS, ObjSet),
        rtFsFatObj_SetMode,
        rtFsFatObj_SetTimes,
        rtFsFatObj_SetOwner,
        RTVFSOBJSETOPS_VERSION
    },
    rtFsFatDir_TraversalOpen,
    rtFsFatDir_OpenFile,
    rtFsFatDir_OpenDir,
    rtFsFatDir_CreateDir,
    rtFsFatDir_OpenSymlink,
    rtFsFatDir_CreateSymlink,
    rtFsFatDir_UnlinkEntry,
    rtFsFatDir_RewindDir,
    rtFsFatDir_ReadDir,
    RTVFSDIROPS_VERSION,
};


/**
 * Instantiates a new directory.
 *
 * @returns IPRT status code.
 * @param   pThis       The FAT volume instance.
 * @param   pParentDir  The parent directory.  This is NULL for the root
 *                      directory.
 * @param   pDirEntry   The parent directory entry. This is NULL for the root
 *                      directory.
 * @param   offDirEntry The byte offset of the directory entry.  UINT64_MAX if
 *                      root directory.
 * @param   idxCluster  The cluster where the directory content is to be found.
 *                      This can be UINT32_MAX if a root FAT12/16 directory.
 * @param   offDisk     The disk byte offset of the FAT12/16 root directory.
 *                      This is UINT64_MAX if idxCluster is given.
 * @param   cbDir       The size of the directory.
 * @param   phVfsDir    Where to return the directory handle.
 * @param   ppDir       Where to return the FAT directory instance data.
 */
static int rtFsFatDir_New(PRTFSFATVOL pThis, PRTFSFATDIR pParentDir, PCFATDIRENTRY pDirEntry, uint64_t offDirEntry,
                          uint32_t idxCluster, uint64_t offDisk, uint32_t cbDir, PRTVFSDIR phVfsDir, PRTFSFATDIR *ppDir)
{
    Assert((idxCluster == UINT32_MAX) != (offDisk == UINT64_MAX));
    *ppDir = NULL;

    PRTFSFATDIR pNewDir;
    int rc = RTVfsNewDir(&g_rtFsFatDirOps, sizeof(*pNewDir), 0 /*fFlags*/, pThis->hVfsSelf,
                         NIL_RTVFSLOCK, phVfsDir, (void **)&pNewDir);
    if (RT_SUCCESS(rc))
    {
        /*
         * Initialize it all so rtFsFatDir_Close doesn't trip up in anyway.
         */
        RTListInit(&pNewDir->OpenChildren);
        if (pDirEntry)
            rtFsFatObj_InitFromDirEntry(&pNewDir->Core, pDirEntry, offDirEntry, pThis);
        else
            rtFsFatObj_InitDummy(&pNewDir->Core, offDirEntry, cbDir, RTFS_DOS_DIRECTORY, pThis);

        pNewDir->cEntries       = cbDir / sizeof(FATDIRENTRY);
        pNewDir->fIsFlatRootDir = idxCluster == UINT32_MAX;
        pNewDir->fFullyBuffered = pNewDir->fIsFlatRootDir;
        if (pNewDir->fFullyBuffered)
        {
            pNewDir->u.Full.offDir          = UINT64_MAX;
            pNewDir->u.Full.cSectors        = 0;
            pNewDir->u.Full.cDirtySectors   = 0;
            pNewDir->u.Full.paEntries       = NULL;
            pNewDir->u.Full.pbDirtySectors  = NULL;
        }
        else
        {
            pNewDir->u.Simple.offOnDisk     = UINT64_MAX;
            pNewDir->u.Simple.offInDir      = UINT32_MAX;
            pNewDir->u.Simple.u32Reserved   = 0;
            pNewDir->u.Simple.paEntries     = NULL;
            pNewDir->u.Simple.fDirty        = false;
        }

        /*
         * If clustered backing, read the chain and see if we cannot still do the full buffering.
         */
        if (idxCluster != UINT32_MAX)
        {
            rc = rtFsFatClusterMap_ReadClusterChain(pThis, idxCluster, &pNewDir->Core.Clusters);
            if (RT_SUCCESS(rc))
            {
                if (   pNewDir->Core.Clusters.cClusters >= 1
                    && pNewDir->Core.Clusters.cbChain   <= _64K
                    && rtFsFatChain_IsContiguous(&pNewDir->Core.Clusters))
                    pNewDir->fFullyBuffered = true;
            }
        }

        if (RT_SUCCESS(rc))
        {
            RT_NOREF(pParentDir);
        }

        RTVfsDirRelease(*phVfsDir);
    }
    *phVfsDir = NIL_RTVFSDIR;
    *ppDir    = NULL;
    return rc;
}





/**
 * @interface_method_impl{RTVFSOBJOPS::Obj,pfnClose}
 */
static DECLCALLBACK(int) rtFsFatVol_Close(void *pvThis)
{
    PRTFSFATVOL pThis = (PRTFSFATVOL)pvThis;
    int rc = rtFsFatClusterMap_Destroy(pThis);

    if (pThis->hVfsRootDir != NIL_RTVFSDIR)
    {
        Assert(RTListIsEmpty(&pThis->pRootDir->OpenChildren));
        uint32_t cRefs = RTVfsDirRelease(pThis->hVfsRootDir);
        Assert(cRefs == 0);
        pThis->hVfsRootDir = NIL_RTVFSDIR;
        pThis->pRootDir    = NULL;
    }

    RTVfsFileRelease(pThis->hVfsBacking);
    pThis->hVfsBacking = NIL_RTVFSFILE;

    return rc;
}


/**
 * @interface_method_impl{RTVFSOBJOPS::Obj,pfnQueryInfo}
 */
static DECLCALLBACK(int) rtFsFatVol_QueryInfo(void *pvThis, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    RT_NOREF(pvThis, pObjInfo, enmAddAttr);
    return VERR_WRONG_TYPE;
}


/**
 * @interface_method_impl{RTVFSOPS,pfnOpenRoo}
 */
static DECLCALLBACK(int) rtFsFatVol_OpenRoot(void *pvThis, PRTVFSDIR phVfsDir)
{
    RT_NOREF(pvThis, phVfsDir);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * @interface_method_impl{RTVFSOPS,pfnIsRangeInUse}
 */
static DECLCALLBACK(int) rtFsFatVol_IsRangeInUse(void *pvThis, RTFOFF off, size_t cb, bool *pfUsed)
{
    RT_NOREF(pvThis, off, cb, pfUsed);
    return VERR_NOT_IMPLEMENTED;
}


DECL_HIDDEN_CONST(const RTVFSOPS) g_rtFsFatVolOps =
{
    { /* Obj */
        RTVFSOBJOPS_VERSION,
        RTVFSOBJTYPE_VFS,
        "FatVol",
        rtFsFatVol_Close,
        rtFsFatVol_QueryInfo,
        RTVFSOBJOPS_VERSION
    },
    RTVFSOPS_VERSION,
    0 /* fFeatures */,
    rtFsFatVol_OpenRoot,
    rtFsFatVol_IsRangeInUse,
    RTVFSOPS_VERSION
};


/**
 * Tries to detect a DOS 1.x formatted image and fills in the BPB fields.
 *
 * There is no BPB here, but fortunately, there isn't much variety.
 *
 * @returns IPRT status code.
 * @param   pThis       The FAT volume instance, BPB derived fields are filled
 *                      in on success.
 * @param   pBootSector The boot sector.
 * @param   pbFatSector Points to the FAT sector, or whatever is 512 bytes after
 *                      the boot sector.
 * @param   pErrInfo    Where to return additional error information.
 */
static int rtFsFatVolTryInitDos1x(PRTFSFATVOL pThis, PCFATBOOTSECTOR pBootSector, uint8_t const *pbFatSector,
                                  PRTERRINFO pErrInfo)
{
    /*
     * PC-DOS 1.0 does a 2fh byte short jump w/o any NOP following it.
     * Instead the following are three words and a 9 byte build date
     * string.  The remaining space is zero filled.
     *
     * Note! No idea how this would look like for 8" floppies, only got 5"1/4'.
     *
     * ASSUME all non-BPB disks are using this format.
     */
    if (   pBootSector->abJmp[0] != 0xeb /* jmp rel8 */
        || pBootSector->abJmp[1] <  0x2f
        || pBootSector->abJmp[1] >= 0x80
        || pBootSector->abJmp[2] == 0x90 /* nop */)
        return RTErrInfoSetF(pErrInfo, VERR_VFS_UNKNOWN_FORMAT,
                             "No DOS v1.0 bootsector either - invalid jmp: %.3Rhxs", pBootSector->abJmp);
    uint32_t const offJump      = 2 + pBootSector->abJmp[1];
    uint32_t const offFirstZero = 2 /*jmp */ + 3 * 2 /* words */ + 9 /* date string */;
    Assert(offFirstZero >= RT_OFFSETOF(FATBOOTSECTOR, Bpb));
    uint32_t const cbZeroPad    = RT_MIN(offJump - offFirstZero,
                                         sizeof(pBootSector->Bpb.Bpb20) - (offFirstZero - RT_OFFSETOF(FATBOOTSECTOR, Bpb)));

    if (!ASMMemIsAllU8((uint8_t const *)pBootSector + offFirstZero, cbZeroPad, 0))
        return RTErrInfoSetF(pErrInfo, VERR_VFS_UNKNOWN_FORMAT,
                             "No DOS v1.0 bootsector either - expected zero padding %#x LB %#x: %.*Rhxs",
                             offFirstZero, cbZeroPad, cbZeroPad, (uint8_t const *)pBootSector + offFirstZero);

    /*
     * Check the FAT ID so we can tell if this is double or single sided,
     * as well as being a valid FAT12 start.
     */
    if (   (pbFatSector[0] != 0xfe && pbFatSector[0] != 0xff)
        || pbFatSector[1] != 0xff
        || pbFatSector[2] != 0xff)
        return RTErrInfoSetF(pErrInfo, VERR_VFS_UNKNOWN_FORMAT,
                             "No DOS v1.0 bootsector either - unexpected start of FAT: %.3Rhxs", pbFatSector);

    /*
     * Fixed DOS 1.0 config.
     */
    pThis->enmFatType       = RTFSFATTYPE_FAT12;
    pThis->bMedia           = pbFatSector[0];
    pThis->cReservedSectors = 1;
    pThis->cbSector         = 512;
    pThis->cbCluster        = pThis->bMedia == 0xfe ? 1024 : 512;
    pThis->cFats            = 2;
    pThis->cbFat            = 512;
    pThis->aoffFats[0]      = pThis->offBootSector + pThis->cReservedSectors * 512;
    pThis->aoffFats[1]      = pThis->aoffFats[0] + pThis->cbFat;
    pThis->offRootDir       = pThis->aoffFats[1] + pThis->cbFat;
    pThis->cRootDirEntries  = 512;
    pThis->offFirstCluster  = pThis->offRootDir + RT_ALIGN_32(pThis->cRootDirEntries * sizeof(FATDIRENTRY),
                                                              pThis->cbSector);
    pThis->cbTotalSize      = pThis->bMedia == 0xfe ? 8 * 1 * 40 * 512 : 8 * 2 * 40 * 512;
    pThis->cClusters        = (pThis->cbTotalSize - (pThis->offFirstCluster - pThis->offBootSector)) / pThis->cbCluster;
    return VINF_SUCCESS;
}


/**
 * Worker for rtFsFatVolTryInitDos2Plus that handles remaining BPB fields.
 *
 * @returns IPRT status code.
 * @param   pThis       The FAT volume instance, BPB derived fields are filled
 *                      in on success.
 * @param   pBootSector The boot sector.
 * @param   fMaybe331   Set if it could be a DOS v3.31 BPB.
 * @param   pErrInfo    Where to return additional error information.
 */
static int rtFsFatVolTryInitDos2PlusBpb(PRTFSFATVOL pThis, PCFATBOOTSECTOR pBootSector, bool fMaybe331, PRTERRINFO pErrInfo)
{
    /*
     * Figure total sector count.  Could both be zero, in which case we have to
     * fall back on the size of the backing stuff.
     */
    if (pBootSector->Bpb.Bpb20.cTotalSectors16 != 0)
        pThis->cbTotalSize = pBootSector->Bpb.Bpb20.cTotalSectors16 * pThis->cbSector;
    else if (   pBootSector->Bpb.Bpb331.cTotalSectors32 != 0
             && fMaybe331)
        pThis->cbTotalSize = pBootSector->Bpb.Bpb331.cTotalSectors32 * (uint64_t)pThis->cbSector;
    else
        pThis->cbTotalSize = pThis->cbBacking - pThis->offBootSector;
    if (pThis->cReservedSectors * pThis->cbSector >= pThis->cbTotalSize)
        return RTErrInfoSetF(pErrInfo, VERR_VFS_BOGUS_FORMAT,
                             "Bogus FAT12/16 total or reserved sector count: %#x vs %#x",
                             pThis->cReservedSectors, pThis->cbTotalSize / pThis->cbSector);

    /*
     * The fat size.  Complete FAT offsets.
     */
    if (   pBootSector->Bpb.Bpb20.cSectorsPerFat == 0
        || ((uint32_t)pBootSector->Bpb.Bpb20.cSectorsPerFat * pThis->cFats + 1) * pThis->cbSector > pThis->cbTotalSize)
        return RTErrInfoSetF(pErrInfo, VERR_VFS_BOGUS_FORMAT, "Bogus FAT12/16 sectors per FAT: %#x (total sectors %#RX64)",
                             pBootSector->Bpb.Bpb20.cSectorsPerFat, pThis->cbTotalSize / pThis->cbSector);
    pThis->cbFat = pBootSector->Bpb.Bpb20.cSectorsPerFat * pThis->cbSector;

    AssertReturn(pThis->cFats < RT_ELEMENTS(pThis->aoffFats), VERR_VFS_BOGUS_FORMAT);
    for (unsigned iFat = 1; iFat <= pThis->cFats; iFat++)
        pThis->aoffFats[iFat] = pThis->aoffFats[iFat - 1] + pThis->cbFat;

    /*
     * Do root directory calculations.
     */
    pThis->idxRootDirCluster = UINT32_MAX;
    pThis->offRootDir        = pThis->aoffFats[pThis->cFats];
    if (pThis->cRootDirEntries == 0)
        return RTErrInfoSet(pErrInfo, VERR_VFS_BOGUS_FORMAT,  "Zero FAT12/16 root directory size");
    pThis->cbRootDir         = pThis->cRootDirEntries * sizeof(FATDIRENTRY);
    pThis->cbRootDir         = RT_ALIGN_32(pThis->cbRootDir, pThis->cbSector);

    /*
     * First cluster and cluster count checks and calcs.  Determin FAT type.
     */
    pThis->offFirstCluster = pThis->offRootDir + pThis->cbRootDir;
    uint64_t cbSystemStuff = pThis->offFirstCluster - pThis->offBootSector;
    if (cbSystemStuff >= pThis->cbTotalSize)
        return RTErrInfoSet(pErrInfo, VERR_VFS_BOGUS_FORMAT,  "Bogus FAT12/16 total size, root dir, or fat size");
    pThis->cClusters = (pThis->cbTotalSize - cbSystemStuff) / pThis->cbCluster;

    if (pThis->cClusters >= FAT_LAST_FAT16_DATA_CLUSTER)
    {
        pThis->cClusters  = FAT_LAST_FAT16_DATA_CLUSTER + 1;
        pThis->enmFatType = RTFSFATTYPE_FAT16;
    }
    else if (pThis->cClusters > FAT_LAST_FAT12_DATA_CLUSTER)
        pThis->enmFatType = RTFSFATTYPE_FAT16;
    else
        pThis->enmFatType = RTFSFATTYPE_FAT12; /** @todo Not sure if this is entirely the right way to go about it... */

    uint32_t cClustersPerFat;
    if (pThis->enmFatType == RTFSFATTYPE_FAT16)
        cClustersPerFat = pThis->cbFat / 2;
    else
        cClustersPerFat = pThis->cbFat * 2 / 3;
    if (pThis->cClusters > cClustersPerFat)
        pThis->cClusters = cClustersPerFat;

    return VINF_SUCCESS;
}


/**
 * Worker for rtFsFatVolTryInitDos2Plus and rtFsFatVolTryInitDos2PlusFat32 that
 * handles common extended BPBs fields.
 *
 * @returns IPRT status code.
 * @param   pThis           The FAT volume instance.
 * @param   bExtSignature   The extended BPB signature.
 * @param   uSerialNumber   The serial number.
 * @param   pachLabel       Pointer to the volume label field.
 * @param   pachType        Pointer to the file system type field.
 */
static void rtFsFatVolInitCommonEbpbBits(PRTFSFATVOL pThis, uint8_t bExtSignature, uint32_t uSerialNumber,
                                         char const *pachLabel, char const *pachType)
{
    pThis->uSerialNo = uSerialNumber;
    if (bExtSignature == FATEBPB_SIGNATURE)
    {
        memcpy(pThis->szLabel, pachLabel, RT_SIZEOFMEMB(FATEBPB, achLabel));
        pThis->szLabel[RT_SIZEOFMEMB(FATEBPB, achLabel)] = '\0';
        RTStrStrip(pThis->szLabel);

        memcpy(pThis->szType, pachType, RT_SIZEOFMEMB(FATEBPB, achType));
        pThis->szType[RT_SIZEOFMEMB(FATEBPB, achType)] = '\0';
        RTStrStrip(pThis->szType);
    }
    else
    {
        pThis->szLabel[0] = '\0';
        pThis->szType[0] = '\0';
    }
}


/**
 * Worker for rtFsFatVolTryInitDos2Plus that deals with FAT32.
 *
 * @returns IPRT status code.
 * @param   pThis       The FAT volume instance, BPB derived fields are filled
 *                      in on success.
 * @param   pBootSector The boot sector.
 * @param   pErrInfo    Where to return additional error information.
 */
static int rtFsFatVolTryInitDos2PlusFat32(PRTFSFATVOL pThis, PCFATBOOTSECTOR pBootSector, PRTERRINFO pErrInfo)
{
    pThis->enmFatType  = RTFSFATTYPE_FAT32;
    pThis->fFat32Flags = pBootSector->Bpb.Fat32Ebpb.fFlags;

    if (pBootSector->Bpb.Fat32Ebpb.uVersion != FAT32EBPB_VERSION_0_0)
        return RTErrInfoSetF(pErrInfo, VERR_VFS_BOGUS_FORMAT, "Unsupported FAT32 version: %d.%d (%#x)",
                             RT_HI_U8(pBootSector->Bpb.Fat32Ebpb.uVersion),  RT_LO_U8(pBootSector->Bpb.Fat32Ebpb.uVersion),
                             pBootSector->Bpb.Fat32Ebpb.uVersion);

    /*
     * Figure total sector count.  We expected it to be filled in.
     */
    bool fUsing64BitTotalSectorCount = false;
    if (pBootSector->Bpb.Fat32Ebpb.Bpb.cTotalSectors16 != 0)
        pThis->cbTotalSize = pBootSector->Bpb.Fat32Ebpb.Bpb.cTotalSectors16 * pThis->cbSector;
    else if (pBootSector->Bpb.Fat32Ebpb.Bpb.cTotalSectors32 != 0)
        pThis->cbTotalSize = pBootSector->Bpb.Fat32Ebpb.Bpb.cTotalSectors32 * (uint64_t)pThis->cbSector;
    else if (   pBootSector->Bpb.Fat32Ebpb.u.cTotalSectors64 <= UINT64_MAX / 512
             && pBootSector->Bpb.Fat32Ebpb.u.cTotalSectors64 > 3
             && pBootSector->Bpb.Fat32Ebpb.bExtSignature != FATEBPB_SIGNATURE_OLD)
    {
        pThis->cbTotalSize = pBootSector->Bpb.Fat32Ebpb.u.cTotalSectors64 * pThis->cbSector;
        fUsing64BitTotalSectorCount = true;
    }
    else
        return RTErrInfoSetF(pErrInfo, VERR_VFS_BOGUS_FORMAT, "FAT32 total sector count out of range: %#RX64",
                             pBootSector->Bpb.Fat32Ebpb.u.cTotalSectors64);
    if (pThis->cReservedSectors * pThis->cbSector >= pThis->cbTotalSize)
        return RTErrInfoSetF(pErrInfo, VERR_VFS_BOGUS_FORMAT,
                             "Bogus FAT32 total or reserved sector count: %#x vs %#x",
                             pThis->cReservedSectors, pThis->cbTotalSize / pThis->cbSector);

    /*
     * Fat size. We check the 16-bit field even if it probably should be zero all the time.
     */
    if (pBootSector->Bpb.Fat32Ebpb.Bpb.cSectorsPerFat != 0)
    {
        if (   pBootSector->Bpb.Fat32Ebpb.cSectorsPerFat32 != 0
            && pBootSector->Bpb.Fat32Ebpb.cSectorsPerFat32 != pBootSector->Bpb.Fat32Ebpb.Bpb.cSectorsPerFat)
            return RTErrInfoSetF(pErrInfo, VERR_VFS_BOGUS_FORMAT,
                                 "Both 16-bit and 32-bit FAT size fields are set: %#RX16 vs %#RX32",
                                 pBootSector->Bpb.Fat32Ebpb.Bpb.cSectorsPerFat, pBootSector->Bpb.Fat32Ebpb.cSectorsPerFat32);
        pThis->cbFat = pBootSector->Bpb.Fat32Ebpb.Bpb.cSectorsPerFat * pThis->cbSector;
    }
    else
    {
        uint64_t cbFat = pBootSector->Bpb.Fat32Ebpb.cSectorsPerFat32 * (uint64_t)pThis->cbSector;
        if (   cbFat == 0
            || cbFat >= (FAT_LAST_FAT32_DATA_CLUSTER + 1) * 4 + pThis->cbSector * 16)
            return RTErrInfoSetF(pErrInfo, VERR_VFS_BOGUS_FORMAT,
                                 "Bogus 32-bit FAT size: %#RX32", pBootSector->Bpb.Fat32Ebpb.cSectorsPerFat32);
        pThis->cbFat = (uint32_t)cbFat;
    }

    /*
     * Complete the FAT offsets and first cluster offset, then calculate number
     * of data clusters.
     */
    AssertReturn(pThis->cFats < RT_ELEMENTS(pThis->aoffFats), VERR_VFS_BOGUS_FORMAT);
    for (unsigned iFat = 1; iFat <= pThis->cFats; iFat++)
        pThis->aoffFats[iFat] = pThis->aoffFats[iFat - 1] + pThis->cbFat;
    pThis->offFirstCluster = pThis->aoffFats[pThis->cFats];

    if (pThis->offFirstCluster - pThis->offBootSector >= pThis->cbTotalSize)
        return RTErrInfoSetF(pErrInfo, VERR_VFS_BOGUS_FORMAT,
                             "Bogus 32-bit FAT size or total sector count: cFats=%d cbFat=%#x cbTotalSize=%#x",
                             pThis->cFats, pThis->cbFat, pThis->cbTotalSize);

    uint64_t cClusters = (pThis->cbTotalSize - (pThis->offFirstCluster - pThis->offBootSector)) / pThis->cbCluster;
    if (cClusters <= FAT_LAST_FAT32_DATA_CLUSTER)
        pThis->cClusters = (uint32_t)cClusters;
    else
        pThis->cClusters = FAT_LAST_FAT32_DATA_CLUSTER + 1;
    if (pThis->cClusters > pThis->cbFat / 4)
        pThis->cClusters = pThis->cbFat / 4;

    /*
     * Root dir cluster.
     */
    if (   pBootSector->Bpb.Fat32Ebpb.uRootDirCluster < FAT_FIRST_DATA_CLUSTER
        || pBootSector->Bpb.Fat32Ebpb.uRootDirCluster >= pThis->cClusters)
        return RTErrInfoSetF(pErrInfo, VERR_VFS_BOGUS_FORMAT,
                             "Bogus FAT32 root directory cluster: %#x", pBootSector->Bpb.Fat32Ebpb.uRootDirCluster);
    pThis->idxRootDirCluster = pBootSector->Bpb.Fat32Ebpb.uRootDirCluster;
    pThis->offRootDir        = pThis->offFirstCluster
                             + (pBootSector->Bpb.Fat32Ebpb.uRootDirCluster - FAT_FIRST_DATA_CLUSTER) * pThis->cbCluster;

    /*
     * Info sector.
     */
    if (   pBootSector->Bpb.Fat32Ebpb.uInfoSectorNo == 0
        || pBootSector->Bpb.Fat32Ebpb.uInfoSectorNo == UINT16_MAX)
        pThis->offFat32InfoSector = UINT64_MAX;
    else if (pBootSector->Bpb.Fat32Ebpb.uInfoSectorNo >= pThis->cReservedSectors)
        return RTErrInfoSetF(pErrInfo, VERR_VFS_BOGUS_FORMAT,
                             "Bogus FAT32 info sector number: %#x (reserved sectors %#x)",
                             pBootSector->Bpb.Fat32Ebpb.uInfoSectorNo, pThis->cReservedSectors);
    else
    {
        pThis->offFat32InfoSector = pThis->cbSector * pBootSector->Bpb.Fat32Ebpb.uInfoSectorNo + pThis->offBootSector;
        int rc = RTVfsFileReadAt(pThis->hVfsBacking, pThis->offFat32InfoSector,
                                 &pThis->Fat32InfoSector, sizeof(pThis->Fat32InfoSector), NULL);
        if (RT_FAILURE(rc))
            return RTErrInfoSetF(pErrInfo, rc, "Failed to read FAT32 info sector at offset %#RX64", pThis->offFat32InfoSector);
        if (   pThis->Fat32InfoSector.uSignature1 != FAT32INFOSECTOR_SIGNATURE_1
            || pThis->Fat32InfoSector.uSignature2 != FAT32INFOSECTOR_SIGNATURE_2
            || pThis->Fat32InfoSector.uSignature3 != FAT32INFOSECTOR_SIGNATURE_3)
            return RTErrInfoSetF(pErrInfo, rc, "FAT32 info sector signature mismatch: %#x %#x %#x",
                                 pThis->Fat32InfoSector.uSignature1,  pThis->Fat32InfoSector.uSignature2,
                                 pThis->Fat32InfoSector.uSignature3);
    }

    /*
     * Boot sector copy.
     */
    if (   pBootSector->Bpb.Fat32Ebpb.uBootSectorCopySectorNo == 0
        || pBootSector->Bpb.Fat32Ebpb.uBootSectorCopySectorNo == UINT16_MAX)
    {
        pThis->cBootSectorCopies   = 0;
        pThis->offBootSectorCopies = UINT64_MAX;
    }
    else if (pBootSector->Bpb.Fat32Ebpb.uBootSectorCopySectorNo >= pThis->cReservedSectors)
        return RTErrInfoSetF(pErrInfo, VERR_VFS_BOGUS_FORMAT,
                             "Bogus FAT32 info boot sector copy location: %#x (reserved sectors %#x)",
                             pBootSector->Bpb.Fat32Ebpb.uBootSectorCopySectorNo, pThis->cReservedSectors);
    else
    {
        /** @todo not sure if cbSector is correct here. */
        pThis->cBootSectorCopies = 3;
        if (  (uint32_t)pBootSector->Bpb.Fat32Ebpb.uBootSectorCopySectorNo + pThis->cBootSectorCopies
            > pThis->cReservedSectors)
            pThis->cBootSectorCopies = (uint8_t)(pThis->cReservedSectors - pBootSector->Bpb.Fat32Ebpb.uBootSectorCopySectorNo);
        pThis->offBootSectorCopies = pBootSector->Bpb.Fat32Ebpb.uBootSectorCopySectorNo * pThis->cbSector + pThis->offBootSector;
        if (   pThis->offFat32InfoSector != UINT64_MAX
            && pThis->offFat32InfoSector - pThis->offBootSectorCopies < (uint64_t)(pThis->cBootSectorCopies * pThis->cbSector))
            return RTErrInfoSetF(pErrInfo, VERR_VFS_BOGUS_FORMAT, "FAT32 info sector and boot sector copies overlap: %#x vs %#x",
                                 pBootSector->Bpb.Fat32Ebpb.uInfoSectorNo, pBootSector->Bpb.Fat32Ebpb.uBootSectorCopySectorNo);
    }

    /*
     * Serial number, label and type.
     */
    rtFsFatVolInitCommonEbpbBits(pThis, pBootSector->Bpb.Fat32Ebpb.bExtSignature, pBootSector->Bpb.Fat32Ebpb.uSerialNumber,
                                 pBootSector->Bpb.Fat32Ebpb.achLabel,
                                 fUsing64BitTotalSectorCount ? pBootSector->achOemName : pBootSector->Bpb.Fat32Ebpb.achLabel);
    if (pThis->szType[0] == '\0')
        memcpy(pThis->szType, "FAT32", 6);

    return VINF_SUCCESS;
}


/**
 * Tries to detect a DOS 2.0+ formatted image and fills in the BPB fields.
 *
 * We ASSUME BPB here, but need to figure out which version of the BPB it is,
 * which is lots of fun.
 *
 * @returns IPRT status code.
 * @param   pThis       The FAT volume instance, BPB derived fields are filled
 *                      in on success.
 * @param   pBootSector The boot sector.
 * @param   pbFatSector Points to the FAT sector, or whatever is 512 bytes after
 *                      the boot sector.  On successful return it will contain
 *                      the first FAT sector.
 * @param   pErrInfo    Where to return additional error information.
 */
static int rtFsFatVolTryInitDos2Plus(PRTFSFATVOL pThis, PCFATBOOTSECTOR pBootSector, uint8_t *pbFatSector, PRTERRINFO pErrInfo)
{
    /*
     * Check if we've got a known jump instruction first, because that will
     * give us a max (E)BPB size hint.
     */
    uint8_t offJmp = UINT8_MAX;
    if (   pBootSector->abJmp[0] == 0xeb
        && pBootSector->abJmp[1] <= 0x7f)
        offJmp = pBootSector->abJmp[1] + 2;
    else if (   pBootSector->abJmp[0] == 0x90
             && pBootSector->abJmp[1] == 0xeb
             && pBootSector->abJmp[2] <= 0x7f)
        offJmp = pBootSector->abJmp[2] + 3;
    else if (   pBootSector->abJmp[0] == 0xe9
             && pBootSector->abJmp[2] <= 0x7f)
        offJmp = RT_MIN(127, RT_MAKE_U16(pBootSector->abJmp[1], pBootSector->abJmp[2]));
    uint8_t const cbMaxBpb = offJmp - RT_OFFSETOF(FATBOOTSECTOR, Bpb);

    /*
     * Do the basic DOS v2.0 BPB fields.
     */
    if (cbMaxBpb < sizeof(FATBPB20))
        return RTErrInfoSetF(pErrInfo, VERR_VFS_UNKNOWN_FORMAT,
                             "DOS signature, but jmp too short for any BPB: %#x (max %#x BPB)", offJmp, cbMaxBpb);

    if (pBootSector->Bpb.Bpb20.cFats == 0)
        return RTErrInfoSet(pErrInfo, VERR_VFS_UNKNOWN_FORMAT, "DOS signature, number of FATs is zero, so not FAT file system");
    if (pBootSector->Bpb.Bpb20.cFats > 4)
        return RTErrInfoSetF(pErrInfo, VERR_VFS_BOGUS_FORMAT, "DOS signature, too many FATs: %#x", pBootSector->Bpb.Bpb20.cFats);
    pThis->cFats = pBootSector->Bpb.Bpb20.cFats;

    if (!FATBPB_MEDIA_IS_VALID(pBootSector->Bpb.Bpb20.bMedia))
        return RTErrInfoSetF(pErrInfo, VERR_VFS_UNKNOWN_FORMAT,
                             "DOS signature, invalid media byte: %#x", pBootSector->Bpb.Bpb20.bMedia);
    pThis->bMedia = pBootSector->Bpb.Bpb20.bMedia;

    if (!RT_IS_POWER_OF_TWO(pBootSector->Bpb.Bpb20.cbSector))
        return RTErrInfoSetF(pErrInfo, VERR_VFS_UNKNOWN_FORMAT,
                             "DOS signature, sector size not power of two: %#x", pBootSector->Bpb.Bpb20.cbSector);
    if (   pBootSector->Bpb.Bpb20.cbSector != 512
        && pBootSector->Bpb.Bpb20.cbSector != 4096
        && pBootSector->Bpb.Bpb20.cbSector != 1024
        && pBootSector->Bpb.Bpb20.cbSector != 128)
        return RTErrInfoSetF(pErrInfo, VERR_VFS_UNKNOWN_FORMAT,
                             "DOS signature, unsupported sector size: %#x", pBootSector->Bpb.Bpb20.cbSector);
    pThis->cbSector = pBootSector->Bpb.Bpb20.cbSector;

    if (   !RT_IS_POWER_OF_TWO(pBootSector->Bpb.Bpb20.cSectorsPerCluster)
        || !pBootSector->Bpb.Bpb20.cSectorsPerCluster)
        return RTErrInfoSetF(pErrInfo, VERR_VFS_UNKNOWN_FORMAT, "DOS signature, cluster size not non-zero power of two: %#x",
                             pBootSector->Bpb.Bpb20.cSectorsPerCluster);
    pThis->cbCluster = pBootSector->Bpb.Bpb20.cSectorsPerCluster * pThis->cbSector;

    uint64_t const cMaxRoot = (pThis->cbBacking - pThis->offBootSector - 512) / sizeof(FATDIRENTRY); /* we'll check again later. */
    if (pBootSector->Bpb.Bpb20.cMaxRootDirEntries >= cMaxRoot)
        return RTErrInfoSetF(pErrInfo, VERR_VFS_BOGUS_FORMAT, "DOS signature, too many root entries: %#x (max %#RX64)",
                             pBootSector->Bpb.Bpb20.cSectorsPerCluster, cMaxRoot);
    pThis->cRootDirEntries = pBootSector->Bpb.Bpb20.cMaxRootDirEntries;

    if (   pBootSector->Bpb.Bpb20.cReservedSectors == 0
        || pBootSector->Bpb.Bpb20.cReservedSectors >= _32K)
        return RTErrInfoSetF(pErrInfo, VERR_VFS_BOGUS_FORMAT,
                             "DOS signature, bogus reserved sector count: %#x", pBootSector->Bpb.Bpb20.cReservedSectors);
    pThis->cReservedSectors = pBootSector->Bpb.Bpb20.cReservedSectors;
    pThis->aoffFats[0]      = pThis->offBootSector + pThis->cReservedSectors * pThis->cbSector;

    /*
     * Jump ahead and check for FAT32 EBPB.
     * If found, we simply ASSUME it's a FAT32 file system.
     */
    int rc;
    if (   (   sizeof(FAT32EBPB) <= cbMaxBpb
            && pBootSector->Bpb.Fat32Ebpb.bExtSignature == FATEBPB_SIGNATURE)
        || (   RT_OFFSETOF(FAT32EBPB, achLabel) <= cbMaxBpb
            && pBootSector->Bpb.Fat32Ebpb.bExtSignature == FATEBPB_SIGNATURE_OLD) )
    {
        rc = rtFsFatVolTryInitDos2PlusFat32(pThis, pBootSector, pErrInfo);
        if (RT_FAILURE(rc))
            return rc;
    }
    else
    {
        /*
         * Check for extended BPB, otherwise we'll have to make qualified guesses
         * about what kind of BPB we're up against based on jmp offset and zero fields.
         * ASSUMES either FAT16 or FAT12.
         */
        if (   (   sizeof(FATEBPB) <= cbMaxBpb
                && pBootSector->Bpb.Ebpb.bExtSignature == FATEBPB_SIGNATURE)
            || (   RT_OFFSETOF(FATEBPB, achLabel) <= cbMaxBpb
                && pBootSector->Bpb.Ebpb.bExtSignature == FATEBPB_SIGNATURE_OLD) )
        {
            rtFsFatVolInitCommonEbpbBits(pThis, pBootSector->Bpb.Ebpb.bExtSignature, pBootSector->Bpb.Ebpb.uSerialNumber,
                                         pBootSector->Bpb.Ebpb.achLabel, pBootSector->Bpb.Ebpb.achType);
            rc = rtFsFatVolTryInitDos2PlusBpb(pThis, pBootSector, true /*fMaybe331*/, pErrInfo);
        }
        else
            rc = rtFsFatVolTryInitDos2PlusBpb(pThis, pBootSector, cbMaxBpb >= sizeof(FATBPB331), pErrInfo);
        if (RT_FAILURE(rc))
            return rc;
        if (pThis->szType[0] == '\0')
            memcpy(pThis->szType, pThis->enmFatType == RTFSFATTYPE_FAT12 ? "FAT12" : "FAT16", 6);
    }

    /*
     * Check the FAT ID. May have to read a bit of the FAT into the buffer.
     */
    if (pThis->aoffFats[0] != pThis->offBootSector + 512)
    {
        rc = RTVfsFileReadAt(pThis->hVfsBacking, pThis->aoffFats[0], pbFatSector, 512, NULL);
        if (RT_FAILURE(rc))
            return RTErrInfoSet(pErrInfo, rc, "error reading first FAT sector");
    }
    if (pbFatSector[0] != pThis->bMedia)
        return RTErrInfoSetF(pErrInfo, VERR_VFS_BOGUS_FORMAT,
                             "Media byte and FAT ID mismatch: %#x vs %#x (%.7Rhxs)", pbFatSector[0], pThis->bMedia, pbFatSector);
    switch (pThis->enmFatType)
    {
        case RTFSFATTYPE_FAT12:
            if ((pbFatSector[1] & 0xf) != 0xf)
                return RTErrInfoSetF(pErrInfo, VERR_VFS_BOGUS_FORMAT, "Bogus FAT ID patting (FAT12): %.3Rhxs", pbFatSector);
            pThis->idxMaxLastCluster = FAT_LAST_FAT12_DATA_CLUSTER;
            pThis->idxEndOfChain     = (pbFatSector[1] >> 4) | ((uint32_t)pbFatSector[2] << 4);
            break;

        case RTFSFATTYPE_FAT16:
            if (pbFatSector[1] != 0xff)
                return RTErrInfoSetF(pErrInfo, VERR_VFS_BOGUS_FORMAT, "Bogus FAT ID patting (FAT16): %.4Rhxs", pbFatSector);
            pThis->idxMaxLastCluster = FAT_LAST_FAT16_DATA_CLUSTER;
            pThis->idxEndOfChain     = RT_MAKE_U16(pbFatSector[2], pbFatSector[3]);
            break;

        case RTFSFATTYPE_FAT32:
            if (   pbFatSector[1] != 0xff
                || pbFatSector[2] != 0xff
                || (pbFatSector[3] & 0x0f) != 0x0f)
                return RTErrInfoSetF(pErrInfo, VERR_VFS_BOGUS_FORMAT, "Bogus FAT ID patting (FAT32): %.8Rhxs", pbFatSector);
            pThis->idxMaxLastCluster = FAT_LAST_FAT32_DATA_CLUSTER;
            pThis->idxEndOfChain     = RT_MAKE_U32_FROM_U8(pbFatSector[4], pbFatSector[5], pbFatSector[6], pbFatSector[7]);
            break;

        default: AssertFailedReturn(VERR_INTERNAL_ERROR_2);
    }
    if (pThis->idxEndOfChain <= pThis->idxMaxLastCluster)
        return RTErrInfoSetF(pErrInfo, VERR_VFS_BOGUS_FORMAT, "Bogus formatter end-of-chain value: %#x, must be above %#x",
                             pThis->idxEndOfChain, pThis->idxMaxLastCluster);

    RT_NOREF(pbFatSector);
    return VINF_SUCCESS;
}


/**
 * Worker for RTFsFatVolOpen.
 *
 * @returns IPRT status code.
 * @param   pThis           The FAT VFS instance to initialize.
 * @param   hVfsSelf        The FAT VFS handle (no reference consumed).
 * @param   hVfsBacking     The file backing the alleged FAT file system.
 *                          Reference is consumed (via rtFsFatVol_Destroy).
 * @param   fReadOnly       Readonly or readwrite mount.
 * @param   offBootSector   The boot sector offset in bytes.
 * @param   pErrInfo        Where to return additional error info.  Can be NULL.
 */
static int rtFsFatVolTryInit(PRTFSFATVOL pThis, RTVFS hVfsSelf, RTVFSFILE hVfsBacking,
                             bool fReadOnly, uint64_t offBootSector, PRTERRINFO pErrInfo)
{
    /*
     * First initialize the state so that rtFsFatVol_Destroy won't trip up.
     */
    pThis->hVfsSelf             = hVfsSelf;
    pThis->hVfsBacking          = hVfsBacking; /* Caller referenced it for us, we consume it; rtFsFatVol_Destroy releases it. */
    pThis->cbBacking            = 0;
    pThis->offBootSector        = offBootSector;
    pThis->fReadOnly            = fReadOnly;
    pThis->cReservedSectors     = 1;

    pThis->cbSector             = 512;
    pThis->cbCluster            = 512;
    pThis->cClusters            = 0;
    pThis->offFirstCluster      = 0;
    pThis->cbTotalSize          = 0;

    pThis->enmFatType           = RTFSFATTYPE_INVALID;
    pThis->cFatEntries          = 0;
    pThis->cFats                = 0;
    pThis->cbFat                = 0;
    for (unsigned i = 0; i < RT_ELEMENTS(pThis->aoffFats); i++)
        pThis->aoffFats[i]      = UINT64_MAX;
    pThis->pFatCache            = NULL;

    pThis->offRootDir           = UINT64_MAX;
    pThis->idxRootDirCluster    = UINT32_MAX;
    pThis->cRootDirEntries      = UINT32_MAX;
    pThis->cbRootDir            = 0;
    pThis->hVfsRootDir          = NIL_RTVFSDIR;
    pThis->pRootDir             = NULL;

    pThis->uSerialNo            = 0;
    pThis->szLabel[0]           = '\0';
    pThis->szType[0]            = '\0';
    pThis->cBootSectorCopies    = 0;
    pThis->fFat32Flags          = 0;
    pThis->offBootSectorCopies  = UINT64_MAX;
    pThis->offFat32InfoSector   = UINT64_MAX;
    RT_ZERO(pThis->Fat32InfoSector);

    /*
     * Get stuff that may fail.
     */
    int rc = RTVfsFileGetSize(hVfsBacking, &pThis->cbBacking);
    if (RT_FAILURE(rc))
        return rc;
    pThis->cbTotalSize = pThis->cbBacking - pThis->offBootSector;

    /*
     * Read the boot sector and the following sector (start of the allocation
     * table unless it a FAT32 FS).  We'll then validate the boot sector and
     * start of the FAT, expanding the BPB into the instance data.
     */
    union
    {
        uint8_t             ab[512*2];
        uint16_t            au16[512*2 / 2];
        uint32_t            au32[512*2 / 4];
        FATBOOTSECTOR       BootSector;
        FAT32INFOSECTOR     InfoSector;
    } Buf;
    RT_ZERO(Buf);

    rc = RTVfsFileReadAt(hVfsBacking, offBootSector, &Buf.BootSector, 512 * 2, NULL);
    if (RT_FAILURE(rc))
        return RTErrInfoSet(pErrInfo, rc, "Unable to read bootsect");

    /*
     * Extract info from the BPB and validate the two special FAT entries.
     *
     * Check the DOS signature first.  The PC-DOS 1.0 boot floppy does not have
     * a signature and we ASSUME this is the case for all flopies formated by it.
     */
    if (Buf.BootSector.uSignature != FATBOOTSECTOR_SIGNATURE)
    {
        if (Buf.BootSector.uSignature != 0)
            return RTErrInfoSetF(pErrInfo, rc, "No DOS bootsector signature: %#06x", Buf.BootSector.uSignature);
        rc = rtFsFatVolTryInitDos1x(pThis, &Buf.BootSector, &Buf.ab[512], pErrInfo);
    }
    else
        rc = rtFsFatVolTryInitDos2Plus(pThis, &Buf.BootSector, &Buf.ab[512], pErrInfo);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Setup the FAT cache.
     */
    rc = rtFsFatClusterMap_Create(pThis, &Buf.ab[512], pErrInfo);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Create the root directory fun.
     */
    if (pThis->idxRootDirCluster == UINT32_MAX)
        rc = rtFsFatDir_New(pThis, NULL /*pParentDir*/, NULL /*pDirEntry*/, 0 /*offDirEntry*/,
                            UINT32_MAX, pThis->offRootDir, pThis->cbRootDir,
                            &pThis->hVfsRootDir, &pThis->pRootDir);
    else
        rc = rtFsFatDir_New(pThis, NULL /*pParentDir*/, NULL /*pDirEntry*/, 0 /*offDirEntry*/,
                            pThis->idxRootDirCluster, UINT64_MAX, pThis->cbRootDir,
                            &pThis->hVfsRootDir, &pThis->pRootDir);
    if (RT_FAILURE(rc))
        return rc;


    return RTErrInfoSetF(pErrInfo, VERR_NOT_IMPLEMENTED,
                         "cbSector=%#x cbCluster=%#x cReservedSectors=%#x\n"
                         "cFats=%#x cbFat=%#x offFirstFat=%#RX64 offSecondFat=%#RX64\n"
                         "cbRootDir=%#x offRootDir=%#RX64 offFirstCluster=%#RX64",
                         pThis->cbSector, pThis->cbCluster, pThis->cReservedSectors,
                         pThis->cFats, pThis->cbFat, pThis->aoffFats[0], pThis->aoffFats[1],
                         pThis->cbRootDir, pThis->offRootDir, pThis->offFirstCluster);
}


RTDECL(int) RTFsFatVolOpen(RTVFSFILE hVfsFileIn, bool fReadOnly, uint64_t offBootSector, PRTVFS phVfs, PRTERRINFO pErrInfo)
{
    /*
     * Quick input validation.
     */
    AssertPtrReturn(phVfs, VERR_INVALID_POINTER);
    *phVfs = NIL_RTVFS;

    uint32_t cRefs = RTVfsFileRetain(hVfsFileIn);
    AssertReturn(cRefs != UINT32_MAX, VERR_INVALID_HANDLE);

    /*
     * Create a new FAT VFS instance and try initialize it using the given input file.
     */
    RTVFS hVfs   = NIL_RTVFS;
    void *pvThis = NULL;
    int rc = RTVfsNew(&g_rtFsFatVolOps, sizeof(RTFSFATVOL), NIL_RTVFS, RTVFSLOCK_CREATE_RW, &hVfs, &pvThis);
    if (RT_SUCCESS(rc))
    {
        rc = rtFsFatVolTryInit((PRTFSFATVOL)pvThis, hVfs, hVfsFileIn, fReadOnly, offBootSector, pErrInfo);
        if (RT_SUCCESS(rc))
            *phVfs = hVfs;
        else
            RTVfsRelease(hVfs);
    }
    else
        RTVfsFileRelease(hVfsFileIn);
    return rc;
}


/**
 * @interface_method_impl{RTVFSCHAINELEMENTREG,pfnValidate}
 */
static DECLCALLBACK(int) rtVfsChainFatVol_Validate(PCRTVFSCHAINELEMENTREG pProviderReg, PRTVFSCHAINSPEC pSpec,
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

    pElement->uProvider = fReadOnly;
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSCHAINELEMENTREG,pfnInstantiate}
 */
static DECLCALLBACK(int) rtVfsChainFatVol_Instantiate(PCRTVFSCHAINELEMENTREG pProviderReg, PCRTVFSCHAINSPEC pSpec,
                                                      PCRTVFSCHAINELEMSPEC pElement, RTVFSOBJ hPrevVfsObj,
                                                      PRTVFSOBJ phVfsObj, uint32_t *poffError, PRTERRINFO pErrInfo)
{
    RT_NOREF(pProviderReg, pSpec, poffError);

    int         rc;
    RTVFSFILE   hVfsFileIn = RTVfsObjToFile(hPrevVfsObj);
    if (hVfsFileIn != NIL_RTVFSFILE)
    {
        RTVFS hVfs;
        rc = RTFsFatVolOpen(hVfsFileIn, pElement->uProvider != false, 0, &hVfs, pErrInfo);
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
static DECLCALLBACK(bool) rtVfsChainFatVol_CanReuseElement(PCRTVFSCHAINELEMENTREG pProviderReg,
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
static RTVFSCHAINELEMENTREG g_rtVfsChainFatVolReg =
{
    /* uVersion = */            RTVFSCHAINELEMENTREG_VERSION,
    /* fReserved = */           0,
    /* pszName = */             "fat",
    /* ListEntry = */           { NULL, NULL },
    /* pszHelp = */             "Open a FAT file system, requires a file object on the left side.\n"
                                "First argument is an optional 'ro' (read-only) or 'rw' (read-write) flag.\n",
    /* pfnValidate = */         rtVfsChainFatVol_Validate,
    /* pfnInstantiate = */      rtVfsChainFatVol_Instantiate,
    /* pfnCanReuseElement = */  rtVfsChainFatVol_CanReuseElement,
    /* uEndMarker = */          RTVFSCHAINELEMENTREG_VERSION
};

RTVFSCHAIN_AUTO_REGISTER_ELEMENT_PROVIDER(&g_rtVfsChainFatVolReg, rtVfsChainFatVolReg);

