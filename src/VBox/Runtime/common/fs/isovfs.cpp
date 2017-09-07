/* $Id$ */
/** @file
 * IPRT - ISO 9660 and UDF Virtual Filesystem (read only).
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
#define LOG_GROUP RTLOGGROUP_FS
#include "internal/iprt.h"
#include <iprt/fsvfs.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/ctype.h>
#include <iprt/file.h>
#include <iprt/log.h>
#include <iprt/mem.h>
#include <iprt/poll.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/vfs.h>
#include <iprt/vfslowlevel.h>
#include <iprt/formats/iso9660.h>
#include <iprt/formats/udf.h>



/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/** Pointer to an ISO volume (VFS instance data). */
typedef struct RTFSISOVOL *PRTFSISOVOL;
/** Pointer to a const ISO volume (VFS instance data). */
typedef struct RTFSISOVOL const *PCRTFSISOVOL;

/** Pointer to a ISO directory instance. */
typedef struct RTFSISODIRSHRD *PRTFSISODIRSHRD;



/**
 * ISO extent (internal to the VFS not a disk structure).
 */
typedef struct RTFSISOEXTENT
{
    /** The disk offset. */
    uint64_t            offDisk;
    /** The size of the extent in bytes. */
    uint64_t            cbExtent;
} RTFSISOEXTENT;
/** Pointer to an ISO 9660 extent. */
typedef RTFSISOEXTENT *PRTFSISOEXTENT;
/** Pointer to a const ISO 9660 extent. */
typedef RTFSISOEXTENT const *PCRTFSISOEXTENT;


/**
 * ISO file system object, shared part.
 */
typedef struct RTFSISOCORE
{
    /** The parent directory keeps a list of open objects (RTFSISOCORE). */
    RTLISTNODE          Entry;
    /** Reference counter.   */
    uint32_t volatile   cRefs;
    /** The parent directory (not released till all children are close). */
    PRTFSISODIRSHRD     pParentDir;
    /** The byte offset of the first directory record. */
    uint64_t            offDirRec;
    /** Attributes. */
    RTFMODE             fAttrib;
    /** The object size. */
    uint64_t            cbObject;
    /** The access time. */
    RTTIMESPEC          AccessTime;
    /** The modificaton time. */
    RTTIMESPEC          ModificationTime;
    /** The change time. */
    RTTIMESPEC          ChangeTime;
    /** The birth time. */
    RTTIMESPEC          BirthTime;
    /** Pointer to the volume. */
    PRTFSISOVOL         pVol;
    /** The version number. */
    uint32_t            uVersion;
    /** Number of extents. */
    uint32_t            cExtents;
    /** The first extent. */
    RTFSISOEXTENT       FirstExtent;
    /** Array of additional extents. */
    PRTFSISOEXTENT      paExtents;
} RTFSISOCORE;
typedef RTFSISOCORE *PRTFSISOCORE;

/**
 * ISO file, shared data.
 */
typedef struct RTFSISOFILESHRD
{
    /** Core ISO9660 object info.  */
    RTFSISOCORE         Core;
} RTFSISOFILESHRD;
/** Pointer to a ISO 9660 file object. */
typedef RTFSISOFILESHRD *PRTFSISOFILESHRD;


/**
 * ISO directory, shared data.
 *
 * We will always read in the whole directory just to keep things really simple.
 */
typedef struct RTFSISODIRSHRD
{
    /** Core ISO 9660 object info.  */
    RTFSISOCORE         Core;
    /** Open child objects (RTFSISOCORE). */
    RTLISTNODE          OpenChildren;

    /** Pointer to the directory content. */
    uint8_t            *pbDir;
    /** The size of the directory content (duplicate of Core.cbObject). */
    uint32_t            cbDir;
} RTFSISODIRSHRD;
/** Pointer to a ISO directory instance. */
typedef RTFSISODIRSHRD *PRTFSISODIRSHRD;


/**
 * Private data for a VFS file object.
 */
typedef struct RTFSISOFILEOBJ
{
    /** Pointer to the shared data. */
    PRTFSISOFILESHRD    pShared;
    /** The current file offset. */
    uint64_t            offFile;
} RTFSISOFILEOBJ;
typedef RTFSISOFILEOBJ *PRTFSISOFILEOBJ;

/**
 * Private data for a VFS directory object.
 */
typedef struct RTFSISODIROBJ
{
    /** Pointer to the shared data. */
    PRTFSISODIRSHRD     pShared;
    /** The current directory offset. */
    uint32_t            offDir;
} RTFSISODIROBJ;
typedef RTFSISODIROBJ *PRTFSISODIROBJ;


/**
 * A ISO volume.
 */
typedef struct RTFSISOVOL
{
    /** Handle to itself. */
    RTVFS               hVfsSelf;
    /** The file, partition, or whatever backing the ISO 9660 volume. */
    RTVFSFILE           hVfsBacking;
    /** The size of the backing thingy. */
    uint64_t            cbBacking;
    /** The size of the backing thingy in sectors (cbSector). */
    uint64_t            cBackingSectors;
    /** Flags. */
    uint32_t            fFlags;
    /** The sector size (in bytes). */
    uint32_t            cbSector;

    /** @name ISO 9660 specific data
     *  @{ */
    /** The size of a logical block in bytes. */
    uint32_t            cbBlock;
    /** The primary volume space size in blocks. */
    uint32_t            cBlocksInPrimaryVolumeSpace;
    /** The primary volume space size in bytes. */
    uint64_t            cbPrimaryVolumeSpace;
    /** The number of volumes in the set. */
    uint32_t            cVolumesInSet;
    /** The primary volume sequence ID. */
    uint32_t            idPrimaryVol;
    /** Set if using UTF16-2 (joliet). */
    bool                fIsUtf16;
    /** @} */

    /** @name UDF specific data.
     * @{ */
    /** Offset of the Anchor volume descriptor sequence. */
    uint64_t            offAvdp;
    /** Length of the anchor volume descriptor sequence. */
    uint32_t            cbAvdp;
    /** @} */

    /** The root directory shared data. */
    PRTFSISODIRSHRD     pRootDir;
} RTFSISOVOL;



/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static void rtFsIsoDirShrd_AddOpenChild(PRTFSISODIRSHRD pDir, PRTFSISOCORE pChild);
static void rtFsIsoDirShrd_RemoveOpenChild(PRTFSISODIRSHRD pDir, PRTFSISOCORE pChild);
static int  rtFsIsoDir_New9660(PRTFSISOVOL pThis, PRTFSISODIRSHRD pParentDir, PCISO9660DIRREC pDirRec,
                           uint32_t cDirRecs, uint64_t offDirRec, PRTVFSDIR phVfsDir);
static PRTFSISOCORE rtFsIsoDir_LookupShared(PRTFSISODIRSHRD pThis, uint64_t offDirRec);


/**
 * Returns the length of the version suffix in the given name.
 *
 * @returns Number of UTF16-BE chars in the version suffix.
 * @param   pawcName        The name to examine.
 * @param   cwcName         The length of the name.
 * @param   puValue         Where to return the value.
 */
static size_t rtFsIso9660GetVersionLengthUtf16Big(PCRTUTF16 pawcName, size_t cwcName, uint32_t *puValue)
{
    *puValue = 0;

    /* -1: */
    if (cwcName <= 2)
        return 0;
    RTUTF16 wc1 = RT_BE2H_U16(pawcName[cwcName - 1]);
    if (!RT_C_IS_DIGIT(wc1))
        return 0;
    Assert(wc1 < 0x3a); /* ASSUMES the RT_C_IS_DIGIT macro works just fine on wide chars too. */

    /* -2: */
    RTUTF16 wc2 = RT_BE2H_U16(pawcName[cwcName - 2]);
    if (wc2 == ';')
    {
        *puValue = wc1 - '0';
        return 2;
    }
    if (!RT_C_IS_DIGIT(wc2) || cwcName <= 3)
        return 0;

    /* -3: */
    RTUTF16 wc3 = RT_BE2H_U16(pawcName[cwcName - 3]);
    if (wc3 == ';')
    {
        *puValue = (wc1 - '0')
                 + (wc2 - '0') * 10;
        return 3;
    }
    if (!RT_C_IS_DIGIT(wc3) || cwcName <= 4)
        return 0;

    /* -4: */
    RTUTF16 wc4 = RT_BE2H_U16(pawcName[cwcName - 4]);
    if (wc4 == ';')
    {
        *puValue = (wc1 - '0')
                 + (wc2 - '0') * 10
                 + (wc3 - '0') * 100;
        return 4;
    }
    if (!RT_C_IS_DIGIT(wc4) || cwcName <= 5)
        return 0;

    /* -5: */
    RTUTF16 wc5 = RT_BE2H_U16(pawcName[cwcName - 5]);
    if (wc5 == ';')
    {
        *puValue = (wc1 - '0')
                 + (wc2 - '0') * 10
                 + (wc3 - '0') * 100
                 + (wc4 - '0') * 1000;
        return 5;
    }
    if (!RT_C_IS_DIGIT(wc5) || cwcName <= 6)
        return 0;

    /* -6: */
    RTUTF16 wc6 = RT_BE2H_U16(pawcName[cwcName - 6]);
    if (wc6 == ';')
    {
        *puValue = (wc1 - '0')
                 + (wc2 - '0') * 10
                 + (wc3 - '0') * 100
                 + (wc4 - '0') * 1000
                 + (wc5 - '0') * 10000;
        return 6;
    }
    return 0;
}


/**
 * Returns the length of the version suffix in the given name.
 *
 * @returns Number of chars in the version suffix.
 * @param   pachName        The name to examine.
 * @param   cchName         The length of the name.
 * @param   puValue         Where to return the value.
 */
static size_t rtFsIso9660GetVersionLengthAscii(const char *pachName, size_t cchName, uint32_t *puValue)
{
    *puValue = 0;

    /* -1: */
    if (cchName <= 2)
        return 0;
    char ch1 = pachName[cchName - 1];
    if (!RT_C_IS_DIGIT(ch1))
        return 0;

    /* -2: */
    char ch2 = pachName[cchName - 2];
    if (ch2 == ';')
    {
        *puValue = ch1 - '0';
        return 2;
    }
    if (!RT_C_IS_DIGIT(ch2) || cchName <= 3)
        return 0;

    /* -3: */
    char ch3 = pachName[cchName - 3];
    if (ch3 == ';')
    {
        *puValue = (ch1 - '0')
                 + (ch2 - '0') * 10;
        return 3;
    }
    if (!RT_C_IS_DIGIT(ch3) || cchName <= 4)
        return 0;

    /* -4: */
    char ch4 = pachName[cchName - 4];
    if (ch4 == ';')
    {
        *puValue = (ch1 - '0')
                 + (ch2 - '0') * 10
                 + (ch3 - '0') * 100;
        return 4;
    }
    if (!RT_C_IS_DIGIT(ch4) || cchName <= 5)
        return 0;

    /* -5: */
    char ch5 = pachName[cchName - 5];
    if (ch5 == ';')
    {
        *puValue = (ch1 - '0')
                 + (ch2 - '0') * 10
                 + (ch3 - '0') * 100
                 + (ch4 - '0') * 1000;
        return 5;
    }
    if (!RT_C_IS_DIGIT(ch5) || cchName <= 6)
        return 0;

    /* -6: */
    if (pachName[cchName - 6] == ';')
    {
        *puValue = (ch1 - '0')
                 + (ch2 - '0') * 10
                 + (ch3 - '0') * 100
                 + (ch4 - '0') * 1000
                 + (ch5 - '0') * 10000;
        return 6;
    }
    return 0;
}


/**
 * Converts a ISO 9660 binary timestamp into an IPRT timesspec.
 *
 * @param   pTimeSpec       Where to return the IRPT time.
 * @param   pIso9660        The ISO 9660 binary timestamp.
 */
static void rtFsIso9660DateTime2TimeSpec(PRTTIMESPEC pTimeSpec, PCISO9660RECTIMESTAMP pIso9660)
{
    RTTIME Time;
    Time.fFlags         = RTTIME_FLAGS_TYPE_UTC;
    Time.offUTC         = 0;
    Time.i32Year        = pIso9660->bYear + 1900;
    Time.u8Month        = RT_MIN(RT_MAX(pIso9660->bMonth, 1), 12);
    Time.u8MonthDay     = RT_MIN(RT_MAX(pIso9660->bDay, 1), 31);
    Time.u8WeekDay      = UINT8_MAX;
    Time.u16YearDay     = 0;
    Time.u8Hour         = RT_MIN(pIso9660->bHour, 23);
    Time.u8Minute       = RT_MIN(pIso9660->bMinute, 59);
    Time.u8Second       = RT_MIN(pIso9660->bSecond, 59);
    Time.u32Nanosecond  = 0;
    RTTimeImplode(pTimeSpec, RTTimeNormalize(&Time));

    /* Only apply the UTC offset if it's within reasons. */
    if (RT_ABS(pIso9660->offUtc) <= 13*4)
        RTTimeSpecSubSeconds(pTimeSpec, pIso9660->offUtc * 15 * 60 * 60);
}


/**
 * Initialization of a RTFSISOCORE structure from a directory record.
 *
 * @note    The RTFSISOCORE::pParentDir and RTFSISOCORE::Clusters members are
 *          properly initialized elsewhere.
 *
 * @returns IRPT status code.  Either VINF_SUCCESS or VERR_NO_MEMORY, the latter
 *          only if @a cDirRecs is above 1.
 * @param   pCore           The structure to initialize.
 * @param   pDirRec         The primary directory record.
 * @param   cDirRecs        Number of directory records.
 * @param   offDirRec       The offset of the primary directory record.
 * @param   uVersion        The file version number.
 * @param   pVol            The volume.
 */
static int rtFsIsoCore_InitFrom9660DirRec(PRTFSISOCORE pCore, PCISO9660DIRREC pDirRec, uint32_t cDirRecs,
                                          uint64_t offDirRec, uint32_t uVersion, PRTFSISOVOL pVol)
{
    RTListInit(&pCore->Entry);
    pCore->cRefs                = 1;
    pCore->pParentDir           = NULL;
    pCore->pVol                 = pVol;
    pCore->offDirRec            = offDirRec;
    pCore->fAttrib              = pDirRec->fFileFlags & ISO9660_FILE_FLAGS_DIRECTORY
                                ? 0755 | RTFS_TYPE_DIRECTORY | RTFS_DOS_DIRECTORY
                                : 0644 | RTFS_TYPE_FILE;
    if (pDirRec->fFileFlags & ISO9660_FILE_FLAGS_HIDDEN)
        pCore->fAttrib |= RTFS_DOS_HIDDEN;
    pCore->cbObject             = ISO9660_GET_ENDIAN(&pDirRec->cbData);
    pCore->uVersion             = uVersion;
    pCore->cExtents             = 1;
    pCore->FirstExtent.cbExtent = pCore->cbObject;
    pCore->FirstExtent.offDisk  = (ISO9660_GET_ENDIAN(&pDirRec->offExtent) + pDirRec->cExtAttrBlocks) * (uint64_t)pVol->cbBlock;

    rtFsIso9660DateTime2TimeSpec(&pCore->ModificationTime, &pDirRec->RecTime);
    pCore->BirthTime  = pCore->ModificationTime;
    pCore->AccessTime = pCore->ModificationTime;
    pCore->ChangeTime = pCore->ModificationTime;

    /*
     * Deal with multiple extents.
     */
    if (RT_LIKELY(cDirRecs == 1))
    { /* done */ }
    else
    {
        PRTFSISOEXTENT pCurExtent = &pCore->FirstExtent;
        while (cDirRecs > 1)
        {
            offDirRec += pDirRec->cbDirRec;
            pDirRec = (PCISO9660DIRREC)((uintptr_t)pDirRec + pDirRec->cbDirRec);
            if (pDirRec->cbDirRec != 0)
            {
                uint64_t offDisk = ISO9660_GET_ENDIAN(&pDirRec->offExtent) * (uint64_t)pVol->cbBlock;
                uint32_t cbExtent  = ISO9660_GET_ENDIAN(&pDirRec->cbData);
                pCore->cbObject += cbExtent;

                if (pCurExtent->offDisk + pCurExtent->cbExtent == offDisk)
                    pCurExtent->cbExtent += cbExtent;
                else
                {
                    void *pvNew = RTMemRealloc(pCore->paExtents, pCore->cExtents * sizeof(pCore->paExtents[0]));
                    if (pvNew)
                        pCore->paExtents = (PRTFSISOEXTENT)pvNew;
                    else
                    {
                        RTMemFree(pCore->paExtents);
                        return VERR_NO_MEMORY;
                    }
                    pCurExtent = &pCore->paExtents[pCore->cExtents - 1];
                    pCurExtent->cbExtent = cbExtent;
                    pCurExtent->offDisk  = offDisk;
                    pCore->cExtents++;
                }
                cDirRecs--;
            }
            else
            {
                size_t cbSkip = (offDirRec + pVol->cbSector) & ~(pVol->cbSector - 1U);
                offDirRec += cbSkip;
                pDirRec = (PCISO9660DIRREC)((uintptr_t)pDirRec + cbSkip);
            }
        }
    }
    return VINF_SUCCESS;
}


/**
 * Worker for rtFsIsoFile_QueryInfo and rtFsIsoDir_QueryInfo.
 */
static int rtFsIsoCore_QueryInfo(PRTFSISOCORE pCore, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    pObjInfo->cbObject              = pCore->cbObject;
    pObjInfo->cbAllocated           = RT_ALIGN_64(pCore->cbObject, pCore->pVol->cbBlock);
    pObjInfo->AccessTime            = pCore->AccessTime;
    pObjInfo->ModificationTime      = pCore->ModificationTime;
    pObjInfo->ChangeTime            = pCore->ChangeTime;
    pObjInfo->BirthTime             = pCore->BirthTime;
    pObjInfo->Attr.fMode            = pCore->fAttrib;
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
            pObjInfo->Attr.u.Unix.GenerationId  = pCore->uVersion;
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
 * Worker for rtFsIsoFile_Close and rtFsIsoDir_Close that does common work.
 *
 * @param   pCore           The common shared structure.
 */
static void rtFsIsoCore_Destroy(PRTFSISOCORE pCore)
{
    if (pCore->pParentDir)
        rtFsIsoDirShrd_RemoveOpenChild(pCore->pParentDir, pCore);
    if (pCore->paExtents)
    {
        RTMemFree(pCore->paExtents);
        pCore->paExtents = NULL;
    }
}


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnClose}
 */
static DECLCALLBACK(int) rtFsIsoFile_Close(void *pvThis)
{
    PRTFSISOFILEOBJ  pThis   = (PRTFSISOFILEOBJ)pvThis;
    LogFlow(("rtFsIsoFile_Close(%p/%p)\n", pThis, pThis->pShared));

    PRTFSISOFILESHRD pShared = pThis->pShared;
    pThis->pShared = NULL;
    if (pShared)
    {
        if (ASMAtomicDecU32(&pShared->Core.cRefs) == 0)
        {
            LogFlow(("rtFsIsoFile_Close: Destroying shared structure %p\n", pShared));
            rtFsIsoCore_Destroy(&pShared->Core);
            RTMemFree(pShared);
        }
    }
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnQueryInfo}
 */
static DECLCALLBACK(int) rtFsIsoFile_QueryInfo(void *pvThis, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    PRTFSISOFILEOBJ pThis = (PRTFSISOFILEOBJ)pvThis;
    return rtFsIsoCore_QueryInfo(&pThis->pShared->Core, pObjInfo, enmAddAttr);
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnRead}
 */
static DECLCALLBACK(int) rtFsIsoFile_Read(void *pvThis, RTFOFF off, PCRTSGBUF pSgBuf, bool fBlocking, size_t *pcbRead)
{
    PRTFSISOFILEOBJ  pThis   = (PRTFSISOFILEOBJ)pvThis;
    PRTFSISOFILESHRD pShared = pThis->pShared;
    AssertReturn(pSgBuf->cSegs != 0, VERR_INTERNAL_ERROR_3);
    RT_NOREF(fBlocking);

    /*
     * Check for EOF.
     */
    if (off == -1)
        off = pThis->offFile;
    if ((uint64_t)off >= pShared->Core.cbObject)
    {
        if (pcbRead)
        {
            *pcbRead = 0;
            return VINF_EOF;
        }
        return VERR_EOF;
    }


    /*
     * Simple case: File has a single extent.
     */
    int      rc         = VINF_SUCCESS;
    size_t   cbRead     = 0;
    uint64_t cbFileLeft = pShared->Core.cbObject - (uint64_t)off;
    size_t   cbLeft     = pSgBuf->paSegs[0].cbSeg;
    uint8_t *pbDst      = (uint8_t *)pSgBuf->paSegs[0].pvSeg;
    if (pShared->Core.cExtents == 1)
    {
        if (cbLeft > 0)
        {
            size_t cbToRead = cbLeft;
            if (cbToRead > cbFileLeft)
                cbToRead = (size_t)cbFileLeft;
            rc = RTVfsFileReadAt(pShared->Core.pVol->hVfsBacking, pShared->Core.FirstExtent.offDisk + off, pbDst, cbToRead, NULL);
            if (RT_SUCCESS(rc))
            {
                off         += cbToRead;
                pbDst       += cbToRead;
                cbRead      += cbToRead;
                cbFileLeft  -= cbToRead;
                cbLeft      -= cbToRead;
            }
        }
    }
    /*
     * Complicated case: Work the file content extent by extent.
     */
    else
    {
        return VERR_NOT_IMPLEMENTED; /** @todo multi-extent stuff . */
    }

    /* Update the offset and return. */
    pThis->offFile = off;
    if (pcbRead)
        *pcbRead = cbRead;
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnWrite}
 */
static DECLCALLBACK(int) rtFsIsoFile_Write(void *pvThis, RTFOFF off, PCRTSGBUF pSgBuf, bool fBlocking, size_t *pcbWritten)
{
    RT_NOREF(pvThis, off, pSgBuf, fBlocking, pcbWritten);
    return VERR_WRITE_PROTECT;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnFlush}
 */
static DECLCALLBACK(int) rtFsIsoFile_Flush(void *pvThis)
{
    RT_NOREF(pvThis);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnPollOne}
 */
static DECLCALLBACK(int) rtFsIsoFile_PollOne(void *pvThis, uint32_t fEvents, RTMSINTERVAL cMillies, bool fIntr,
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
static DECLCALLBACK(int) rtFsIsoFile_Tell(void *pvThis, PRTFOFF poffActual)
{
    PRTFSISOFILEOBJ pThis = (PRTFSISOFILEOBJ)pvThis;
    *poffActual = pThis->offFile;
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnMode}
 */
static DECLCALLBACK(int) rtFsIsoFile_SetMode(void *pvThis, RTFMODE fMode, RTFMODE fMask)
{
    RT_NOREF(pvThis, fMode, fMask);
    return VERR_WRITE_PROTECT;
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnSetTimes}
 */
static DECLCALLBACK(int) rtFsIsoFile_SetTimes(void *pvThis, PCRTTIMESPEC pAccessTime, PCRTTIMESPEC pModificationTime,
                                                 PCRTTIMESPEC pChangeTime, PCRTTIMESPEC pBirthTime)
{
    RT_NOREF(pvThis, pAccessTime, pModificationTime, pChangeTime, pBirthTime);
    return VERR_WRITE_PROTECT;
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnSetOwner}
 */
static DECLCALLBACK(int) rtFsIsoFile_SetOwner(void *pvThis, RTUID uid, RTGID gid)
{
    RT_NOREF(pvThis, uid, gid);
    return VERR_WRITE_PROTECT;
}


/**
 * @interface_method_impl{RTVFSFILEOPS,pfnSeek}
 */
static DECLCALLBACK(int) rtFsIsoFile_Seek(void *pvThis, RTFOFF offSeek, unsigned uMethod, PRTFOFF poffActual)
{
    PRTFSISOFILEOBJ pThis = (PRTFSISOFILEOBJ)pvThis;
    RTFOFF offNew;
    switch (uMethod)
    {
        case RTFILE_SEEK_BEGIN:
            offNew = offSeek;
            break;
        case RTFILE_SEEK_END:
            offNew = (RTFOFF)pThis->pShared->Core.cbObject + offSeek;
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
static DECLCALLBACK(int) rtFsIsoFile_QuerySize(void *pvThis, uint64_t *pcbFile)
{
    PRTFSISOFILEOBJ pThis = (PRTFSISOFILEOBJ)pvThis;
    *pcbFile = pThis->pShared->Core.cbObject;
    return VINF_SUCCESS;
}


/**
 * ISO FS file operations.
 */
DECL_HIDDEN_CONST(const RTVFSFILEOPS) g_rtFsIos9660FileOps =
{
    { /* Stream */
        { /* Obj */
            RTVFSOBJOPS_VERSION,
            RTVFSOBJTYPE_FILE,
            "FatFile",
            rtFsIsoFile_Close,
            rtFsIsoFile_QueryInfo,
            RTVFSOBJOPS_VERSION
        },
        RTVFSIOSTREAMOPS_VERSION,
        RTVFSIOSTREAMOPS_FEAT_NO_SG,
        rtFsIsoFile_Read,
        rtFsIsoFile_Write,
        rtFsIsoFile_Flush,
        rtFsIsoFile_PollOne,
        rtFsIsoFile_Tell,
        NULL /*pfnSkip*/,
        NULL /*pfnZeroFill*/,
        RTVFSIOSTREAMOPS_VERSION,
    },
    RTVFSFILEOPS_VERSION,
    0,
    { /* ObjSet */
        RTVFSOBJSETOPS_VERSION,
        RT_OFFSETOF(RTVFSFILEOPS, Stream.Obj) - RT_OFFSETOF(RTVFSFILEOPS, ObjSet),
        rtFsIsoFile_SetMode,
        rtFsIsoFile_SetTimes,
        rtFsIsoFile_SetOwner,
        RTVFSOBJSETOPS_VERSION
    },
    rtFsIsoFile_Seek,
    rtFsIsoFile_QuerySize,
    RTVFSFILEOPS_VERSION
};


/**
 * Instantiates a new directory, from 9660.
 *
 * @returns IPRT status code.
 * @param   pThis           The FAT volume instance.
 * @param   pParentDir      The parent directory (shared part).
 * @param   pDirRec         The directory record.
 * @param   cDirRecs        Number of directory records if more than one.
 * @param   offDirRec       The byte offset of the directory record.
 * @param   offEntryInDir   The byte offset of the directory entry in the parent
 *                          directory.
 * @param   fOpen           RTFILE_O_XXX flags.
 * @param   uVersion        The file version number (since the caller already
 *                          parsed the filename, we don't want to repeat the
 *                          effort here).
 * @param   phVfsFile       Where to return the file handle.
 */
static int rtFsIsoFile_New9660(PRTFSISOVOL pThis, PRTFSISODIRSHRD pParentDir, PCISO9660DIRREC pDirRec, uint32_t cDirRecs,
                               uint64_t offDirRec, uint64_t fOpen, uint32_t uVersion, PRTVFSFILE phVfsFile)
{
    AssertPtr(pParentDir);

    /*
     * Create a VFS object.
     */
    PRTFSISOFILEOBJ pNewFile;
    int rc = RTVfsNewFile(&g_rtFsIos9660FileOps, sizeof(*pNewFile), fOpen, pThis->hVfsSelf, NIL_RTVFSLOCK /*use volume lock*/,
                          phVfsFile, (void **)&pNewFile);
    if (RT_SUCCESS(rc))
    {
        /*
         * Look for existing shared object, create a new one if necessary.
         */
        PRTFSISOFILESHRD pShared = (PRTFSISOFILESHRD)rtFsIsoDir_LookupShared(pParentDir, offDirRec);
        if (!pShared)
        {
            pShared = (PRTFSISOFILESHRD)RTMemAllocZ(sizeof(*pShared));
            if (pShared)
            {
                /*
                 * Initialize it all so rtFsIsoFile_Close doesn't trip up in anyway.
                 */
                rc = rtFsIsoCore_InitFrom9660DirRec(&pShared->Core, pDirRec, cDirRecs, offDirRec, uVersion, pThis);
                if (RT_SUCCESS(rc))
                    rtFsIsoDirShrd_AddOpenChild(pParentDir, &pShared->Core);
                else
                {
                    RTMemFree(pShared);
                    pShared = NULL;
                }
            }
        }
        if (pShared)
        {
            LogFlow(("rtFsIsoFile_New9660: cbObject=%#RX64 First Extent: off=%#RX64 cb=%#RX64\n",
                     pShared->Core.cbObject, pShared->Core.FirstExtent.offDisk, pShared->Core.FirstExtent.cbExtent));
            pNewFile->offFile = 0;
            pNewFile->pShared = pShared;
            return VINF_SUCCESS;
        }

        rc = VERR_NO_MEMORY;
    }
    *phVfsFile = NIL_RTVFSFILE;
    return rc;
}


/**
 * Looks up the shared structure for a child.
 *
 * @returns Referenced pointer to the shared structure, NULL if not found.
 * @param   pThis           The directory.
 * @param   offDirRec       The directory record offset of the child.
 */
static PRTFSISOCORE rtFsIsoDir_LookupShared(PRTFSISODIRSHRD pThis, uint64_t offDirRec)
{
    PRTFSISOCORE pCur;
    RTListForEach(&pThis->OpenChildren, pCur, RTFSISOCORE, Entry)
    {
        if (pCur->offDirRec == offDirRec)
        {
            uint32_t cRefs = ASMAtomicIncU32(&pCur->cRefs);
            Assert(cRefs > 1); RT_NOREF(cRefs);
            return pCur;
        }
    }
    return NULL;
}


#ifdef RT_STRICT
/**
 * Checks if @a pNext is an extent of @a pFirst.
 *
 * @returns true if @a pNext is the next extent, false if not
 * @param   pFirst      The directory record describing the first or the
 *                      previous extent.
 * @param   pNext       The directory record alleged to be the next extent.
 */
DECLINLINE(bool) rtFsIsoDir_Is9660DirRecNextExtent(PCISO9660DIRREC pFirst, PCISO9660DIRREC pNext)
{
    if (RT_LIKELY(pNext->bFileIdLength == pFirst->bFileIdLength))
    {
        if (RT_LIKELY((pNext->fFileFlags | ISO9660_FILE_FLAGS_MULTI_EXTENT) == pFirst->fFileFlags))
        {
            if (RT_LIKELY(memcmp(pNext->achFileId, pFirst->achFileId, pNext->bFileIdLength) == 0))
                return true;
        }
    }
    return false;
}
#endif /* RT_STRICT */


/**
 * Worker for rtFsIsoDir_FindEntry that compares a UTF-16BE name with a
 * directory record.
 *
 * @returns true if equal, false if not.
 * @param   pDirRec             The directory record.
 * @param   pwszEntry           The UTF-16BE string to compare with.
 * @param   cbEntry             The compare string length in bytes (sans zero
 *                              terminator).
 * @param   cwcEntry            The compare string length in RTUTF16 units.
 * @param   puVersion           Where to return any file version number.
 */
DECL_FORCE_INLINE(bool) rtFsIsoDir_IsEntryEqualUtf16Big(PCISO9660DIRREC pDirRec, PCRTUTF16 pwszEntry, size_t cbEntry,
                                                        size_t cwcEntry, uint32_t *puVersion)
{
    /* ASSUME directories cannot have any version tags. */
    if (pDirRec->fFileFlags & ISO9660_FILE_FLAGS_DIRECTORY)
    {
        if (RT_LIKELY(pDirRec->bFileIdLength != cbEntry))
            return false;
        if (RT_LIKELY(RTUtf16BigNICmp((PCRTUTF16)pDirRec->achFileId, pwszEntry, cwcEntry) != 0))
            return false;
    }
    else
    {
        size_t cbNameDelta = (size_t)pDirRec->bFileIdLength - cbEntry;
        if (RT_LIKELY(cbNameDelta > (size_t)12 /* ;12345 */))
            return false;
        if (cbNameDelta == 0)
        {
            if (RT_LIKELY(RTUtf16BigNICmp((PCRTUTF16)pDirRec->achFileId, pwszEntry, cwcEntry) != 0))
                return false;
            *puVersion = 1;
        }
        else
        {
            if (RT_LIKELY(RT_MAKE_U16(pDirRec->achFileId[cbEntry + 1], pDirRec->achFileId[cbEntry]) != ';'))
                return false;
            if (RT_LIKELY(RTUtf16BigNICmp((PCRTUTF16)pDirRec->achFileId, pwszEntry, cwcEntry) != 0))
                return false;
            uint32_t uVersion;
            size_t  cwcVersion = rtFsIso9660GetVersionLengthUtf16Big((PCRTUTF16)pDirRec->achFileId,
                                                                     pDirRec->bFileIdLength, &uVersion);
            if (RT_LIKELY(cwcVersion * sizeof(RTUTF16) == cbNameDelta))
                *puVersion = uVersion;
            else
                return false;
        }
    }

    /* (No need to check for dot and dot-dot here, because cbEntry must be a
       multiple of two.) */
    Assert(!(cbEntry & 1));
    return true;
}


/**
 * Worker for rtFsIsoDir_FindEntry that compares an ASCII name with a
 * directory record.
 *
 * @returns true if equal, false if not.
 * @param   pDirRec             The directory record.
 * @param   pszEntry            The uppercased ASCII string to compare with.
 * @param   cchEntry            The length of the compare string.
 * @param   puVersion           Where to return any file version number.
 */
DECL_FORCE_INLINE(bool) rtFsIsoDir_IsEntryEqualAscii(PCISO9660DIRREC pDirRec, const char *pszEntry, size_t cchEntry,
                                                         uint32_t *puVersion)
{
    /* ASSUME directories cannot have any version tags. */
    if (pDirRec->fFileFlags & ISO9660_FILE_FLAGS_DIRECTORY)
    {
        if (RT_LIKELY(pDirRec->bFileIdLength != cchEntry))
            return false;
        if (RT_LIKELY(memcmp(pDirRec->achFileId, pszEntry, cchEntry) != 0))
            return false;
    }
    else
    {
        size_t cchNameDelta = (size_t)pDirRec->bFileIdLength - cchEntry;
        if (RT_LIKELY(cchNameDelta > (size_t)6 /* ;12345 */))
            return false;
        if (cchNameDelta == 0)
        {
            if (RT_LIKELY(memcmp(pDirRec->achFileId, pszEntry, cchEntry) != 0))
                return false;
            *puVersion = 1;
        }
        else
        {
            if (RT_LIKELY(pDirRec->achFileId[cchEntry] != ';'))
                return false;
            if (RT_LIKELY(memcmp(pDirRec->achFileId, pszEntry, cchEntry) != 0))
                return false;
            uint32_t uVersion;
            size_t  cchVersion = rtFsIso9660GetVersionLengthAscii(pDirRec->achFileId, pDirRec->bFileIdLength, &uVersion);
            if (RT_LIKELY(cchVersion == cchNameDelta))
                *puVersion = uVersion;
            else
                return false;
        }
    }

    /* Don't match the 'dot' and 'dot-dot' directory records. */
    if (RT_LIKELY(   pDirRec->bFileIdLength != 1
                  || (uint8_t)pDirRec->achFileId[0] > (uint8_t)0x01))
        return true;
    return false;
}


/**
 * Locates a directory entry in a directory.
 *
 * @returns IPRT status code.
 * @retval  VERR_FILE_NOT_FOUND if not found.
 * @param   pThis           The directory to search.
 * @param   pszEntry        The entry to look for.
 * @param   poffDirRec      Where to return the offset of the directory record
 *                          on the disk.
 * @param   ppDirRec        Where to return the pointer to the directory record
 *                          (the whole directory is buffered).
 * @param   pcDirRecs       Where to return the number of directory records
 *                          related to this entry.
 * @param   pfMode          Where to return the file type, rock ridge adjusted.
 * @param   puVersion       Where to return the file version number.
 */
static int rtFsIsoDir_FindEntry(PRTFSISODIRSHRD pThis, const char *pszEntry,  uint64_t *poffDirRec,
                                PCISO9660DIRREC *ppDirRec, uint32_t *pcDirRecs, PRTFMODE pfMode, uint32_t *puVersion)
{
    /* Set return values. */
    *poffDirRec = UINT64_MAX;
    *ppDirRec   = NULL;
    *pcDirRecs  = 1;
    *pfMode     = UINT32_MAX;
    *puVersion  = 0;

    /*
     * If we're in UTF-16BE mode, convert the input name to UTF-16BE.  Otherwise try
     * uppercase it into a ISO 9660 compliant name.
     */
    int         rc;
    bool const  fIsUtf16  = pThis->Core.pVol->fIsUtf16;
    size_t      cwcEntry  = 0;
    size_t      cbEntry   = 0;
    size_t      cchUpper  = ~(size_t)0;
    union
    {
        RTUTF16  wszEntry[260 + 1];
        struct
        {
            char  szUpper[255 + 1];
            char  szRock[260 + 1];
        } s;
    } uBuf;
    if (fIsUtf16)
    {
        PRTUTF16 pwszEntry = uBuf.wszEntry;
        rc = RTStrToUtf16BigEx(pszEntry, RTSTR_MAX, &pwszEntry, RT_ELEMENTS(uBuf.wszEntry), &cwcEntry);
        if (RT_FAILURE(rc))
            return rc;
        cbEntry = cwcEntry * 2;
    }
    else
    {
        rc = RTStrCopy(uBuf.s.szUpper, sizeof(uBuf.s.szUpper), pszEntry);
        if (RT_SUCCESS(rc))
        {
            RTStrToUpper(uBuf.s.szUpper);
            cchUpper = strlen(uBuf.s.szUpper);
        }
    }

    /*
     * Scan the directory buffer by buffer.
     */
    uint32_t        offEntryInDir   = 0;
    uint32_t const  cbDir           = pThis->Core.cbObject;
    while (offEntryInDir + RT_UOFFSETOF(ISO9660DIRREC, achFileId) <= cbDir)
    {
        PCISO9660DIRREC pDirRec = (PCISO9660DIRREC)&pThis->pbDir[offEntryInDir];

        /* If null length, skip to the next sector. */
        if (pDirRec->cbDirRec == 0)
            offEntryInDir = (offEntryInDir + pThis->Core.pVol->cbSector) & ~(pThis->Core.pVol->cbSector - 1U);
        else
        {
            /* Try match the filename. */
            if (fIsUtf16)
            {
                if (RT_LIKELY(!rtFsIsoDir_IsEntryEqualUtf16Big(pDirRec, uBuf.wszEntry, cbEntry, cwcEntry, puVersion)))
                {
                    /* Advance */
                    offEntryInDir += pDirRec->cbDirRec;
                    continue;
                }
            }
            else
            {
                if (RT_LIKELY(!rtFsIsoDir_IsEntryEqualAscii(pDirRec, uBuf.s.szUpper, cchUpper, puVersion)))
                {
                    /** @todo check rock. */
                    if (1)
                    {
                        /* Advance */
                        offEntryInDir += pDirRec->cbDirRec;
                        continue;
                    }
                }
            }

            *poffDirRec = pThis->Core.FirstExtent.offDisk + offEntryInDir;
            *ppDirRec   = pDirRec;
            *pfMode     = pDirRec->fFileFlags & ISO9660_FILE_FLAGS_DIRECTORY
                        ? 0755 | RTFS_TYPE_DIRECTORY | RTFS_DOS_DIRECTORY
                        : 0644 | RTFS_TYPE_FILE;

            /*
             * Deal with the unlikely scenario of multi extent records.
             */
            if (!(pDirRec->fFileFlags & ISO9660_FILE_FLAGS_MULTI_EXTENT))
                *pcDirRecs = 1;
            else
            {
                offEntryInDir += pDirRec->cbDirRec;

                uint32_t cDirRecs = 1;
                while (offEntryInDir + RT_UOFFSETOF(ISO9660DIRREC, achFileId) <= cbDir)
                {
                    PCISO9660DIRREC pDirRec2 = (PCISO9660DIRREC)&pThis->pbDir[offEntryInDir];
                    if (pDirRec2->cbDirRec != 0)
                    {
                        Assert(rtFsIsoDir_Is9660DirRecNextExtent(pDirRec, pDirRec2));
                        cDirRecs++;
                        if (!(pDirRec2->fFileFlags & ISO9660_FILE_FLAGS_MULTI_EXTENT))
                            break;
                        offEntryInDir += pDirRec2->cbDirRec;
                    }
                    else
                        offEntryInDir = (offEntryInDir + pThis->Core.pVol->cbSector) & ~(pThis->Core.pVol->cbSector - 1U);
                }

                *pcDirRecs = cDirRecs;
            }
            return VINF_SUCCESS;
        }
    }

    return VERR_FILE_NOT_FOUND;
}


/**
 * Releases a reference to a shared directory structure.
 *
 * @param   pShared             The shared directory structure.
 */
static void rtFsIsoDirShrd_Release(PRTFSISODIRSHRD pShared)
{
    uint32_t cRefs = ASMAtomicDecU32(&pShared->Core.cRefs);
    Assert(cRefs < UINT32_MAX / 2);
    if (cRefs == 0)
    {
        LogFlow(("rtFsIsoDirShrd_Release: Destroying shared structure %p\n", pShared));
        Assert(pShared->Core.cRefs == 0);
        if (pShared->pbDir)
        {
            RTMemFree(pShared->pbDir);
            pShared->pbDir = NULL;
        }
        rtFsIsoCore_Destroy(&pShared->Core);
        RTMemFree(pShared);
    }
}


/**
 * Retains a reference to a shared directory structure.
 *
 * @param   pShared             The shared directory structure.
 */
static void rtFsIsoDirShrd_Retain(PRTFSISODIRSHRD pShared)
{
    uint32_t cRefs = ASMAtomicIncU32(&pShared->Core.cRefs);
    Assert(cRefs > 1); NOREF(cRefs);
}



/**
 * @interface_method_impl{RTVFSOBJOPS,pfnClose}
 */
static DECLCALLBACK(int) rtFsIsoDir_Close(void *pvThis)
{
    PRTFSISODIROBJ pThis = (PRTFSISODIROBJ)pvThis;
    LogFlow(("rtFsIsoDir_Close(%p/%p)\n", pThis, pThis->pShared));

    PRTFSISODIRSHRD pShared = pThis->pShared;
    pThis->pShared = NULL;
    if (pShared)
        rtFsIsoDirShrd_Release(pShared);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnQueryInfo}
 */
static DECLCALLBACK(int) rtFsIsoDir_QueryInfo(void *pvThis, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    PRTFSISODIROBJ pThis = (PRTFSISODIROBJ)pvThis;
    return rtFsIsoCore_QueryInfo(&pThis->pShared->Core, pObjInfo, enmAddAttr);
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnMode}
 */
static DECLCALLBACK(int) rtFsIsoDir_SetMode(void *pvThis, RTFMODE fMode, RTFMODE fMask)
{
    RT_NOREF(pvThis, fMode, fMask);
    return VERR_WRITE_PROTECT;
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnSetTimes}
 */
static DECLCALLBACK(int) rtFsIsoDir_SetTimes(void *pvThis, PCRTTIMESPEC pAccessTime, PCRTTIMESPEC pModificationTime,
                                                 PCRTTIMESPEC pChangeTime, PCRTTIMESPEC pBirthTime)
{
    RT_NOREF(pvThis, pAccessTime, pModificationTime, pChangeTime, pBirthTime);
    return VERR_WRITE_PROTECT;
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnSetOwner}
 */
static DECLCALLBACK(int) rtFsIsoDir_SetOwner(void *pvThis, RTUID uid, RTGID gid)
{
    RT_NOREF(pvThis, uid, gid);
    return VERR_WRITE_PROTECT;
}


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnTraversalOpen}
 */
static DECLCALLBACK(int) rtFsIsoDir_TraversalOpen(void *pvThis, const char *pszEntry, PRTVFSDIR phVfsDir,
                                                  PRTVFSSYMLINK phVfsSymlink, PRTVFS phVfsMounted)
{
    /*
     * We may have symbolic links if rock ridge is being used, though currently
     * we won't have nested mounts.
     */
    int rc;
    if (phVfsMounted)
        *phVfsMounted = NIL_RTVFS;
    if (phVfsDir || phVfsSymlink)
    {
        if (phVfsSymlink)
            *phVfsSymlink = NIL_RTVFSSYMLINK;
        if (phVfsDir)
            *phVfsDir = NIL_RTVFSDIR;

        PRTFSISODIROBJ      pThis = (PRTFSISODIROBJ)pvThis;
        PRTFSISODIRSHRD     pShared = pThis->pShared;
        PCISO9660DIRREC     pDirRec;
        uint64_t            offDirRec;
        uint32_t            cDirRecs;
        RTFMODE             fMode;
        uint32_t            uVersion;
        rc = rtFsIsoDir_FindEntry(pShared, pszEntry, &offDirRec, &pDirRec, &cDirRecs, &fMode, &uVersion);
        Log2(("rtFsIsoDir_TraversalOpen: FindEntry(,%s,) -> %Rrc\n", pszEntry, rc));
        if (RT_SUCCESS(rc))
        {
            switch (fMode & RTFS_TYPE_MASK)
            {
                case RTFS_TYPE_DIRECTORY:
                    if (phVfsDir)
                        rc = rtFsIsoDir_New9660(pShared->Core.pVol, pShared, pDirRec, cDirRecs, offDirRec, phVfsDir);
                    else
                        rc = VERR_NOT_SYMLINK;
                    break;

                case RTFS_TYPE_SYMLINK:
                    rc = VERR_NOT_IMPLEMENTED;
                    break;
                case RTFS_TYPE_FILE:
                case RTFS_TYPE_DEV_BLOCK:
                case RTFS_TYPE_DEV_CHAR:
                case RTFS_TYPE_FIFO:
                case RTFS_TYPE_SOCKET:
                    rc = VERR_NOT_A_DIRECTORY;
                    break;
                default:
                case RTFS_TYPE_WHITEOUT:
                    rc = VERR_PATH_NOT_FOUND;
                    break;
            }
        }
        else if (rc == VERR_FILE_NOT_FOUND)
            rc = VERR_PATH_NOT_FOUND;
    }
    else
        rc = VERR_PATH_NOT_FOUND;
    return rc;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnOpenFile}
 */
static DECLCALLBACK(int) rtFsIsoDir_OpenFile(void *pvThis, const char *pszFilename, uint32_t fOpen, PRTVFSFILE phVfsFile)
{
    PRTFSISODIROBJ  pThis   = (PRTFSISODIROBJ)pvThis;
    PRTFSISODIRSHRD pShared = pThis->pShared;

    /*
     * We cannot create or replace anything, just open stuff.
     */
    if (   (fOpen & RTFILE_O_ACTION_MASK) == RTFILE_O_CREATE
        || (fOpen & RTFILE_O_ACTION_MASK) == RTFILE_O_CREATE_REPLACE)
        return VERR_WRITE_PROTECT;

    /*
     * Try open file.
     */
    PCISO9660DIRREC pDirRec;
    uint64_t        offDirRec;
    uint32_t        cDirRecs;
    RTFMODE         fMode;
    uint32_t        uVersion;
    int rc = rtFsIsoDir_FindEntry(pShared, pszFilename, &offDirRec, &pDirRec, &cDirRecs, &fMode, &uVersion);
    Log2(("rtFsIsoDir_OpenFile: FindEntry(,%s,) -> %Rrc\n", pszFilename, rc));
    if (RT_SUCCESS(rc))
    {
        switch (fMode & RTFS_TYPE_MASK)
        {
            case RTFS_TYPE_FILE:
                rc = rtFsIsoFile_New9660(pShared->Core.pVol, pShared, pDirRec, cDirRecs, offDirRec, fOpen, uVersion, phVfsFile);
                break;

            case RTFS_TYPE_SYMLINK:
            case RTFS_TYPE_DEV_BLOCK:
            case RTFS_TYPE_DEV_CHAR:
            case RTFS_TYPE_FIFO:
            case RTFS_TYPE_SOCKET:
            case RTFS_TYPE_WHITEOUT:
                rc = VERR_NOT_IMPLEMENTED;
                break;

            case RTFS_TYPE_DIRECTORY:
                rc = VERR_NOT_A_FILE;
                break;

            default:
                rc = VERR_PATH_NOT_FOUND;
                break;
        }
    }
    return rc;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnOpenDir}
 */
static DECLCALLBACK(int) rtFsIsoDir_OpenDir(void *pvThis, const char *pszSubDir, uint32_t fFlags, PRTVFSDIR phVfsDir)
{
    PRTFSISODIROBJ  pThis   = (PRTFSISODIROBJ)pvThis;
    PRTFSISODIRSHRD pShared = pThis->pShared;
    AssertReturn(!fFlags, VERR_INVALID_FLAGS);

    /*
     * Try open file.
     */
    PCISO9660DIRREC pDirRec;
    uint64_t        offDirRec;
    uint32_t        cDirRecs;
    RTFMODE         fMode;
    uint32_t        uVersion;
    int rc = rtFsIsoDir_FindEntry(pShared, pszSubDir, &offDirRec, &pDirRec, &cDirRecs, &fMode, &uVersion);
    Log2(("rtFsIsoDir_OpenDir: FindEntry(,%s,) -> %Rrc\n", pszSubDir, rc));
    if (RT_SUCCESS(rc))
    {
        switch (fMode & RTFS_TYPE_MASK)
        {
            case RTFS_TYPE_DIRECTORY:
                rc = rtFsIsoDir_New9660(pShared->Core.pVol, pShared, pDirRec, cDirRecs, offDirRec, phVfsDir);
                break;

            case RTFS_TYPE_FILE:
            case RTFS_TYPE_SYMLINK:
            case RTFS_TYPE_DEV_BLOCK:
            case RTFS_TYPE_DEV_CHAR:
            case RTFS_TYPE_FIFO:
            case RTFS_TYPE_SOCKET:
            case RTFS_TYPE_WHITEOUT:
                rc = VERR_NOT_A_DIRECTORY;
                break;

            default:
                rc = VERR_PATH_NOT_FOUND;
                break;
        }
    }
    return rc;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnCreateDir}
 */
static DECLCALLBACK(int) rtFsIsoDir_CreateDir(void *pvThis, const char *pszSubDir, RTFMODE fMode, PRTVFSDIR phVfsDir)
{
    RT_NOREF(pvThis, pszSubDir, fMode, phVfsDir);
    return VERR_WRITE_PROTECT;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnOpenSymlink}
 */
static DECLCALLBACK(int) rtFsIsoDir_OpenSymlink(void *pvThis, const char *pszSymlink, PRTVFSSYMLINK phVfsSymlink)
{
    RT_NOREF(pvThis, pszSymlink, phVfsSymlink);
RTAssertMsg2("%s: %s\n", __FUNCTION__, pszSymlink);
    return VERR_NOT_SUPPORTED;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnCreateSymlink}
 */
static DECLCALLBACK(int) rtFsIsoDir_CreateSymlink(void *pvThis, const char *pszSymlink, const char *pszTarget,
                                                  RTSYMLINKTYPE enmType, PRTVFSSYMLINK phVfsSymlink)
{
    RT_NOREF(pvThis, pszSymlink, pszTarget, enmType, phVfsSymlink);
    return VERR_WRITE_PROTECT;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnQueryEntryInfo}
 */
static DECLCALLBACK(int) rtFsIsoDir_QueryEntryInfo(void *pvThis, const char *pszEntry,
                                                   PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    /*
     * Try locate the entry.
     */
    PRTFSISODIROBJ      pThis   = (PRTFSISODIROBJ)pvThis;
    PRTFSISODIRSHRD     pShared = pThis->pShared;
    PCISO9660DIRREC     pDirRec;
    uint64_t            offDirRec;
    uint32_t            cDirRecs;
    RTFMODE             fMode;
    uint32_t            uVersion;
    int rc = rtFsIsoDir_FindEntry(pShared, pszEntry, &offDirRec, &pDirRec, &cDirRecs, &fMode, &uVersion);
    Log2(("rtFsIsoDir_QueryEntryInfo: FindEntry(,%s,) -> %Rrc\n", pszEntry, rc));
    if (RT_SUCCESS(rc))
    {
        /*
         * To avoid duplicating code in rtFsIsoCore_InitFrom9660DirRec and
         * rtFsIsoCore_QueryInfo, we create a dummy RTFSISOCORE on the stack.
         */
        RTFSISOCORE TmpObj;
        RT_ZERO(TmpObj);
        rc = rtFsIsoCore_InitFrom9660DirRec(&TmpObj, pDirRec, cDirRecs, offDirRec, uVersion, pShared->Core.pVol);
        if (RT_SUCCESS(rc))
        {
            rc = rtFsIsoCore_QueryInfo(&TmpObj, pObjInfo, enmAddAttr);
            RTMemFree(TmpObj.paExtents);
        }
    }
    return rc;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnUnlinkEntry}
 */
static DECLCALLBACK(int) rtFsIsoDir_UnlinkEntry(void *pvThis, const char *pszEntry, RTFMODE fType)
{
    RT_NOREF(pvThis, pszEntry, fType);
    return VERR_WRITE_PROTECT;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnRenameEntry}
 */
static DECLCALLBACK(int) rtFsIsoDir_RenameEntry(void *pvThis, const char *pszEntry, RTFMODE fType, const char *pszNewName)
{
    RT_NOREF(pvThis, pszEntry, fType, pszNewName);
    return VERR_WRITE_PROTECT;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnRewindDir}
 */
static DECLCALLBACK(int) rtFsIsoDir_RewindDir(void *pvThis)
{
    PRTFSISODIROBJ pThis = (PRTFSISODIROBJ)pvThis;
    pThis->offDir = 0;
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnReadDir}
 */
static DECLCALLBACK(int) rtFsIsoDir_ReadDir(void *pvThis, PRTDIRENTRYEX pDirEntry, size_t *pcbDirEntry,
                                            RTFSOBJATTRADD enmAddAttr)
{
    PRTFSISODIROBJ  pThis   = (PRTFSISODIROBJ)pvThis;
    PRTFSISODIRSHRD pShared = pThis->pShared;

    while (pThis->offDir + RT_UOFFSETOF(ISO9660DIRREC, achFileId) <= pShared->cbDir)
    {
        PCISO9660DIRREC pDirRec = (PCISO9660DIRREC)&pShared->pbDir[pThis->offDir];

        /* If null length, skip to the next sector. */
        if (pDirRec->cbDirRec == 0)
            pThis->offDir = (pThis->offDir + pShared->Core.pVol->cbSector) & ~(pShared->Core.pVol->cbSector - 1U);
        else
        {
            /*
             * Do names first as they may cause overflows.
             */
            uint32_t uVersion = 0;
            if (   pDirRec->bFileIdLength == 1
                && pDirRec->achFileId[0]  == '\0')
            {
                if (*pcbDirEntry < RT_UOFFSETOF(RTDIRENTRYEX, szName) + 2)
                {
                    *pcbDirEntry = RT_UOFFSETOF(RTDIRENTRYEX, szName) + 2;
                    Log3(("rtFsIsoDir_ReadDir: VERR_BUFFER_OVERFLOW (dot)\n"));
                    return VERR_BUFFER_OVERFLOW;
                }
                pDirEntry->cbName    = 1;
                pDirEntry->szName[0] = '.';
                pDirEntry->szName[1] = '\0';
            }
            else if (   pDirRec->bFileIdLength == 1
                     && pDirRec->achFileId[0]  == '\1')
            {
                if (*pcbDirEntry < RT_UOFFSETOF(RTDIRENTRYEX, szName) + 3)
                {
                    *pcbDirEntry = RT_UOFFSETOF(RTDIRENTRYEX, szName) + 3;
                    Log3(("rtFsIsoDir_ReadDir: VERR_BUFFER_OVERFLOW (dot-dot)\n"));
                    return VERR_BUFFER_OVERFLOW;
                }
                pDirEntry->cbName    = 2;
                pDirEntry->szName[0] = '.';
                pDirEntry->szName[1] = '.';
                pDirEntry->szName[2] = '\0';
            }
            else if (pShared->Core.pVol->fIsUtf16)
            {
                PCRTUTF16 pawcSrc   = (PCRTUTF16)&pDirRec->achFileId[0];
                size_t    cwcSrc    = pDirRec->bFileIdLength / sizeof(RTUTF16);
                size_t    cwcVer    = !(pDirRec->fFileFlags & ISO9660_FILE_FLAGS_DIRECTORY)
                                    ? rtFsIso9660GetVersionLengthUtf16Big(pawcSrc, cwcSrc, &uVersion) : 0;
                size_t    cchNeeded = 0;
                size_t    cbDst     = *pcbDirEntry - RT_UOFFSETOF(RTDIRENTRYEX, szName);
                char     *pszDst    = pDirEntry->szName;

                int rc = RTUtf16BigToUtf8Ex(pawcSrc, cwcSrc - cwcVer, &pszDst, cbDst, &cchNeeded);
                if (RT_SUCCESS(rc))
                    pDirEntry->cbName = (uint16_t)cchNeeded;
                else if (rc == VERR_BUFFER_OVERFLOW)
                {
                    *pcbDirEntry = RT_UOFFSETOF(RTDIRENTRYEX, szName) + cchNeeded + 1;
                    Log3(("rtFsIsoDir_ReadDir: VERR_BUFFER_OVERFLOW - cbDst=%zu cchNeeded=%zu (UTF-16BE)\n", cbDst, cchNeeded));
                    return VERR_BUFFER_OVERFLOW;
                }
                else
                {
                    ssize_t cchNeeded2 = RTStrPrintf2(pszDst, cbDst, "bad-name-%#x", pThis->offDir);
                    if (cchNeeded2 >= 0)
                        pDirEntry->cbName = (uint16_t)cchNeeded2;
                    else
                    {
                        *pcbDirEntry = RT_UOFFSETOF(RTDIRENTRYEX, szName) + (size_t)-cchNeeded2;
                        return VERR_BUFFER_OVERFLOW;
                    }
                }
            }
            else
            {
                /* This is supposed to be upper case ASCII, however, purge the encoding anyway. */
                size_t cchVer   = !(pDirRec->fFileFlags & ISO9660_FILE_FLAGS_DIRECTORY)
                                ? rtFsIso9660GetVersionLengthAscii(pDirRec->achFileId, pDirRec->bFileIdLength, &uVersion) : 0;
                size_t cchName  = pDirRec->bFileIdLength - cchVer;
                size_t cbNeeded = RT_UOFFSETOF(RTDIRENTRYEX, szName) + cchName + 1;
                if (*pcbDirEntry < cbNeeded)
                {
                    Log3(("rtFsIsoDir_ReadDir: VERR_BUFFER_OVERFLOW - cbDst=%zu cbNeeded=%zu (ASCII)\n", *pcbDirEntry, cbNeeded));
                    *pcbDirEntry = cbNeeded;
                    return VERR_BUFFER_OVERFLOW;
                }
                pDirEntry->cbName = (uint16_t)cchName;
                memcpy(pDirEntry->szName, pDirRec->achFileId, cchName);
                pDirEntry->szName[cchName] = '\0';
                RTStrPurgeEncoding(pDirEntry->szName);

                /** @todo check for rock ridge names here.   */
            }
            pDirEntry->cwcShortName    = 0;
            pDirEntry->wszShortName[0] = '\0';

            /*
             * To avoid duplicating code in rtFsIsoCore_InitFrom9660DirRec and
             * rtFsIsoCore_QueryInfo, we create a dummy RTFSISOCORE on the stack.
             */
            RTFSISOCORE TmpObj;
            RT_ZERO(TmpObj);
            rtFsIsoCore_InitFrom9660DirRec(&TmpObj, pDirRec, 1 /* cDirRecs - see below why 1 */,
                                           pThis->offDir + pShared->Core.FirstExtent.offDisk, uVersion, pShared->Core.pVol);
            int rc = rtFsIsoCore_QueryInfo(&TmpObj, &pDirEntry->Info, enmAddAttr);

            /*
             * Update the directory location and handle multi extent records.
             *
             * Multi extent records only affect the file size and the directory location,
             * so we deal with it here instead of involving * rtFsIsoCore_InitFrom9660DirRec
             * which would potentially require freeing memory and such.
             */
            if (!(pDirRec->fFileFlags & ISO9660_FILE_FLAGS_MULTI_EXTENT))
            {
                Log3(("rtFsIsoDir_ReadDir: offDir=%#07x: %s (rc=%Rrc)\n", pThis->offDir, pDirEntry->szName, rc));
                pThis->offDir += pDirRec->cbDirRec;
            }
            else
            {
                uint32_t cExtents = 1;
                uint32_t offDir   = pThis->offDir + pDirRec->cbDirRec;
                while (offDir + RT_UOFFSETOF(ISO9660DIRREC, achFileId) <= pShared->cbDir)
                {
                    PCISO9660DIRREC pDirRec2 = (PCISO9660DIRREC)&pShared->pbDir[offDir];
                    if (pDirRec2->cbDirRec != 0)
                    {
                        pDirEntry->Info.cbObject += ISO9660_GET_ENDIAN(&pDirRec2->cbData);
                        offDir += pDirRec2->cbDirRec;
                        cExtents++;
                        if (!(pDirRec2->fFileFlags & ISO9660_FILE_FLAGS_MULTI_EXTENT))
                            break;
                    }
                    else
                        offDir = (offDir + pShared->Core.pVol->cbSector) & ~(pShared->Core.pVol->cbSector - 1U);
                }
                Log3(("rtFsIsoDir_ReadDir: offDir=%#07x, %u extents ending at %#07x: %s (rc=%Rrc)\n",
                      pThis->offDir, cExtents, offDir, pDirEntry->szName, rc));
                pThis->offDir = offDir;
            }

            return rc;
        }
    }

    Log3(("rtFsIsoDir_ReadDir: offDir=%#07x: VERR_NO_MORE_FILES\n", pThis->offDir));
    return VERR_NO_MORE_FILES;
}


/**
 * FAT file operations.
 */
static const RTVFSDIROPS g_rtFsIsoDirOps =
{
    { /* Obj */
        RTVFSOBJOPS_VERSION,
        RTVFSOBJTYPE_DIR,
        "ISO 9660 Dir",
        rtFsIsoDir_Close,
        rtFsIsoDir_QueryInfo,
        RTVFSOBJOPS_VERSION
    },
    RTVFSDIROPS_VERSION,
    0,
    { /* ObjSet */
        RTVFSOBJSETOPS_VERSION,
        RT_OFFSETOF(RTVFSDIROPS, Obj) - RT_OFFSETOF(RTVFSDIROPS, ObjSet),
        rtFsIsoDir_SetMode,
        rtFsIsoDir_SetTimes,
        rtFsIsoDir_SetOwner,
        RTVFSOBJSETOPS_VERSION
    },
    rtFsIsoDir_TraversalOpen,
    rtFsIsoDir_OpenFile,
    rtFsIsoDir_OpenDir,
    rtFsIsoDir_CreateDir,
    rtFsIsoDir_OpenSymlink,
    rtFsIsoDir_CreateSymlink,
    rtFsIsoDir_QueryEntryInfo,
    rtFsIsoDir_UnlinkEntry,
    rtFsIsoDir_RenameEntry,
    rtFsIsoDir_RewindDir,
    rtFsIsoDir_ReadDir,
    RTVFSDIROPS_VERSION,
};


/**
 * Adds an open child to the parent directory's shared structure.
 *
 * Maintains an additional reference to the parent dir to prevent it from going
 * away.  If @a pDir is the root directory, it also ensures the volume is
 * referenced and sticks around until the last open object is gone.
 *
 * @param   pDir        The directory.
 * @param   pChild      The child being opened.
 * @sa      rtFsIsoDirShrd_RemoveOpenChild
 */
static void rtFsIsoDirShrd_AddOpenChild(PRTFSISODIRSHRD pDir, PRTFSISOCORE pChild)
{
    rtFsIsoDirShrd_Retain(pDir);

    RTListAppend(&pDir->OpenChildren, &pChild->Entry);
    pChild->pParentDir = pDir;
}


/**
 * Removes an open child to the parent directory.
 *
 * @param   pDir        The directory.
 * @param   pChild      The child being removed.
 *
 * @remarks This is the very last thing you do as it may cause a few other
 *          objects to be released recursively (parent dir and the volume).
 *
 * @sa      rtFsIsoDirShrd_AddOpenChild
 */
static void rtFsIsoDirShrd_RemoveOpenChild(PRTFSISODIRSHRD pDir, PRTFSISOCORE pChild)
{
    AssertReturnVoid(pChild->pParentDir == pDir);
    RTListNodeRemove(&pChild->Entry);
    pChild->pParentDir = NULL;

    rtFsIsoDirShrd_Release(pDir);
}


#ifdef LOG_ENABLED
/**
 * Logs the content of a directory.
 */
static void rtFsIsoDirShrd_Log9660Content(PRTFSISODIRSHRD pThis)
{
    if (LogIs2Enabled())
    {
        uint32_t offRec = 0;
        while (offRec < pThis->cbDir)
        {
            PCISO9660DIRREC pDirRec = (PCISO9660DIRREC)&pThis->pbDir[offRec];
            if (pDirRec->cbDirRec == 0)
                break;

            RTUTF16 wszName[128];
            if (pThis->Core.pVol->fIsUtf16)
            {
                PRTUTF16  pwszDst = &wszName[pDirRec->bFileIdLength / sizeof(RTUTF16)];
                PCRTUTF16 pwszSrc = (PCRTUTF16)&pDirRec->achFileId[pDirRec->bFileIdLength];
                pwszSrc--;
                *pwszDst-- = '\0';
                while ((uintptr_t)pwszDst >= (uintptr_t)&wszName[0])
                {
                    *pwszDst = RT_BE2H_U16(*pwszSrc);
                    pwszDst--;
                    pwszSrc--;
                }
            }
            else
            {
                PRTUTF16 pwszDst = wszName;
                for (uint32_t off = 0; off < pDirRec->bFileIdLength; off++)
                    *pwszDst++ = pDirRec->achFileId[off];
                *pwszDst = '\0';
            }

            Log2(("ISO9660:  %04x: rec=%#x ea=%#x cb=%#010RX32 off=%#010RX32 fl=%#04x %04u-%02u-%02u %02u:%02u:%02u%+03d unit=%#x igap=%#x idVol=%#x '%ls'\n",
                  offRec,
                  pDirRec->cbDirRec,
                  pDirRec->cExtAttrBlocks,
                  ISO9660_GET_ENDIAN(&pDirRec->cbData),
                  ISO9660_GET_ENDIAN(&pDirRec->offExtent),
                  pDirRec->fFileFlags,
                  pDirRec->RecTime.bYear + 1900,
                  pDirRec->RecTime.bMonth,
                  pDirRec->RecTime.bDay,
                  pDirRec->RecTime.bHour,
                  pDirRec->RecTime.bMinute,
                  pDirRec->RecTime.bSecond,
                  pDirRec->RecTime.offUtc*4/60,
                  pDirRec->bFileUnitSize,
                  pDirRec->bInterleaveGapSize,
                  ISO9660_GET_ENDIAN(&pDirRec->VolumeSeqNo),
                  wszName));

            uint32_t offSysUse = RT_OFFSETOF(ISO9660DIRREC, achFileId[pDirRec->bFileIdLength]) + !(pDirRec->bFileIdLength & 1);
            if (offSysUse < pDirRec->cbDirRec)
            {
                Log2(("ISO9660:       system use (%#x bytes):\n%.*Rhxd\n", pDirRec->cbDirRec - offSysUse,
                      pDirRec->cbDirRec - offSysUse, (uint8_t *)pDirRec + offSysUse));
            }

            /* advance */
            offRec += pDirRec->cbDirRec;
        }
    }
}
#endif /* LOG_ENABLED */


/**
 * Instantiates a new shared directory structure, given 9660 records.
 *
 * @returns IPRT status code.
 * @param   pThis           The FAT volume instance.
 * @param   pParentDir      The parent directory.  This is NULL for the root
 *                          directory.
 * @param   pDirRec         The directory record.  Will access @a cDirRecs
 *                          records.
 * @param   cDirRecs        Number of directory records if more than one.
 * @param   offDirRec       The byte offset of the directory record.
 * @param   ppShared        Where to return the shared directory structure.
 */
static int rtFsIsoDirShrd_New9660(PRTFSISOVOL pThis, PRTFSISODIRSHRD pParentDir, PCISO9660DIRREC pDirRec,
                                  uint32_t cDirRecs, uint64_t offDirRec, PRTFSISODIRSHRD *ppShared)
{
    /*
     * Allocate a new structure and initialize it.
     */
    int rc = VERR_NO_MEMORY;
    PRTFSISODIRSHRD pShared = (PRTFSISODIRSHRD)RTMemAllocZ(sizeof(*pShared));
    if (pShared)
    {
        rc = rtFsIsoCore_InitFrom9660DirRec(&pShared->Core, pDirRec, cDirRecs, offDirRec, 0 /*uVersion*/, pThis);
        if (RT_SUCCESS(rc))
        {
            RTListInit(&pShared->OpenChildren);
            pShared->cbDir = ISO9660_GET_ENDIAN(&pDirRec->cbData);
            pShared->pbDir = (uint8_t *)RTMemAllocZ(pShared->cbDir + 256);
            if (pShared->pbDir)
            {
                rc = RTVfsFileReadAt(pThis->hVfsBacking, pShared->Core.FirstExtent.offDisk, pShared->pbDir, pShared->cbDir, NULL);
                if (RT_SUCCESS(rc))
                {
#ifdef LOG_ENABLED
                    rtFsIsoDirShrd_Log9660Content(pShared);
#endif

                    /*
                     * Link into parent directory so we can use it to update
                     * our directory entry.
                     */
                    if (pParentDir)
                        rtFsIsoDirShrd_AddOpenChild(pParentDir, &pShared->Core);
                    *ppShared = pShared;
                    return VINF_SUCCESS;
                }
            }
        }
        RTMemFree(pShared);
    }
    *ppShared = NULL;
    return rc;
}


/**
 * Instantiates a new directory with a shared structure presupplied.
 *
 * @returns IPRT status code.
 * @param   pThis           The FAT volume instance.
 * @param   pShared         Referenced pointer to the shared structure.  The
 *                          reference is always CONSUMED.
 * @param   phVfsDir        Where to return the directory handle.
 */
static int rtFsIsoDir_NewWithShared(PRTFSISOVOL pThis, PRTFSISODIRSHRD pShared, PRTVFSDIR phVfsDir)
{
    /*
     * Create VFS object around the shared structure.
     */
    PRTFSISODIROBJ pNewDir;
    int rc = RTVfsNewDir(&g_rtFsIsoDirOps, sizeof(*pNewDir), 0 /*fFlags*/, pThis->hVfsSelf,
                         NIL_RTVFSLOCK /*use volume lock*/, phVfsDir, (void **)&pNewDir);
    if (RT_SUCCESS(rc))
    {
        /*
         * Look for existing shared object, create a new one if necessary.
         * We CONSUME a reference to pShared here.
         */
        pNewDir->offDir  = 0;
        pNewDir->pShared = pShared;
        return VINF_SUCCESS;
    }

    rtFsIsoDirShrd_Release(pShared);
    *phVfsDir = NIL_RTVFSDIR;
    return rc;
}



/**
 * Instantiates a new directory VFS instance for ISO 9660, creating the shared
 * structure as necessary.
 *
 * @returns IPRT status code.
 * @param   pThis           The FAT volume instance.
 * @param   pParentDir      The parent directory.  This is NULL for the root
 *                          directory.
 * @param   pDirRec         The directory record.
 * @param   cDirRecs        Number of directory records if more than one.
 * @param   offDirRec       The byte offset of the directory record.
 * @param   phVfsDir        Where to return the directory handle.
 */
static int  rtFsIsoDir_New9660(PRTFSISOVOL pThis, PRTFSISODIRSHRD pParentDir, PCISO9660DIRREC pDirRec,
                               uint32_t cDirRecs, uint64_t offDirRec, PRTVFSDIR phVfsDir)
{
    /*
     * Look for existing shared object, create a new one if necessary.
     */
    PRTFSISODIRSHRD pShared = (PRTFSISODIRSHRD)rtFsIsoDir_LookupShared(pParentDir, offDirRec);
    if (!pShared)
    {
        int rc = rtFsIsoDirShrd_New9660(pThis, pParentDir, pDirRec, cDirRecs, offDirRec, &pShared);
        if (RT_FAILURE(rc))
        {
            *phVfsDir = NIL_RTVFSDIR;
            return rc;
        }
    }
    return rtFsIsoDir_NewWithShared(pThis, pShared, phVfsDir);
}



/**
 * @interface_method_impl{RTVFSOBJOPS::Obj,pfnClose}
 */
static DECLCALLBACK(int) rtFsIsoVol_Close(void *pvThis)
{
    PRTFSISOVOL pThis = (PRTFSISOVOL)pvThis;
    Log(("rtFsIsoVol_Close(%p)\n", pThis));

    if (pThis->pRootDir)
    {
        Assert(RTListIsEmpty(&pThis->pRootDir->OpenChildren));
        Assert(pThis->pRootDir->Core.cRefs == 1);
        rtFsIsoDirShrd_Release(pThis->pRootDir);
        pThis->pRootDir = NULL;
    }

    RTVfsFileRelease(pThis->hVfsBacking);
    pThis->hVfsBacking = NIL_RTVFSFILE;

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSOBJOPS::Obj,pfnQueryInfo}
 */
static DECLCALLBACK(int) rtFsIsoVol_QueryInfo(void *pvThis, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    RT_NOREF(pvThis, pObjInfo, enmAddAttr);
    return VERR_WRONG_TYPE;
}


/**
 * @interface_method_impl{RTVFSOPS,pfnOpenRoo}
 */
static DECLCALLBACK(int) rtFsIsoVol_OpenRoot(void *pvThis, PRTVFSDIR phVfsDir)
{
    PRTFSISOVOL pThis = (PRTFSISOVOL)pvThis;

    rtFsIsoDirShrd_Retain(pThis->pRootDir); /* consumed by the next call */
    return rtFsIsoDir_NewWithShared(pThis, pThis->pRootDir, phVfsDir);
}


/**
 * @interface_method_impl{RTVFSOPS,pfnIsRangeInUse}
 */
static DECLCALLBACK(int) rtFsIsoVol_IsRangeInUse(void *pvThis, RTFOFF off, size_t cb, bool *pfUsed)
{
    RT_NOREF(pvThis, off, cb, pfUsed);
    return VERR_NOT_IMPLEMENTED;
}


DECL_HIDDEN_CONST(const RTVFSOPS) g_rtFsIsoVolOps =
{
    { /* Obj */
        RTVFSOBJOPS_VERSION,
        RTVFSOBJTYPE_VFS,
        "ISO 9660/UDF",
        rtFsIsoVol_Close,
        rtFsIsoVol_QueryInfo,
        RTVFSOBJOPS_VERSION
    },
    RTVFSOPS_VERSION,
    0 /* fFeatures */,
    rtFsIsoVol_OpenRoot,
    rtFsIsoVol_IsRangeInUse,
    RTVFSOPS_VERSION
};


static int rtFsIsoVolValidateUdfDescTag(PCUDFTAG pTag, uint16_t idTag, uint32_t offTag, PRTERRINFO pErrInfo)
{
    /*
     * Checksum the tag first.
     */
    const uint8_t *pbTag     = (const uint8_t *)pTag;
    uint8_t const  bChecksum = pbTag[0]
                             + pbTag[1]
                             + pbTag[2]
                             + pbTag[3]
                             + pbTag[5] /* skipping byte 4 as that's the checksum. */
                             + pbTag[6]
                             + pbTag[7]
                             + pbTag[8]
                             + pbTag[9]
                             + pbTag[10]
                             + pbTag[11]
                             + pbTag[12]
                             + pbTag[13]
                             + pbTag[14]
                             + pbTag[15];
    if (pTag->uChecksum == bChecksum)
    {
        /*
         * Do the matching.
         */
        if (   pTag->uVersion == 3
            || pTag->uVersion == 2)
        {
            if (   pTag->idTag == idTag
                || idTag == UINT16_MAX)
            {
                if (pTag->offTag == offTag)
                    return VINF_SUCCESS;

                Log(("rtFsIsoVolValidateUdfDescTag(,%#x,%#010RX32,): Sector mismatch: %#RX32 (%.*Rhxs)\n",
                     idTag, offTag, pTag->offTag, sizeof(*pTag), pTag));
                return RTErrInfoSetF(pErrInfo, VERR_MISMATCH, "Descriptor tag sector number mismatch: %#x, expected %#x (%.*Rhxs)",
                                     pTag->offTag, offTag, sizeof(*pTag), pTag);
            }
            Log(("rtFsIsoVolValidateUdfDescTag(,%#x,%#010RX32,): Tag ID mismatch: %#x (%.*Rhxs)\n",
                 idTag, offTag, pTag->idTag, sizeof(*pTag), pTag));
            return RTErrInfoSetF(pErrInfo, VERR_MISMATCH, "Descriptor tag ID mismatch: %#x, expected %#x (%.*Rhxs)",
                                 pTag->idTag, idTag, sizeof(*pTag), pTag);
        }
        Log(("rtFsIsoVolValidateUdfDescTag(,%#x,%#010RX32,): Unsupported version: %#x (%.*Rhxs)\n",
             idTag, offTag, pTag->uVersion, sizeof(*pTag), pTag));
        return RTErrInfoSetF(pErrInfo, VERR_MISMATCH, "Unsupported descriptor tag version: %#x, expected 2 or 3 (%.*Rhxs)",
                             pTag->uVersion, sizeof(*pTag), pTag);
    }
    Log(("rtFsIsoVolValidateUdfDescTag(,%#x,%#010RX32,): checksum error: %#x, calc %#x (%.*Rhxs)\n",
         idTag, offTag, pTag->uChecksum, bChecksum, sizeof(*pTag), pTag));
    return RTErrInfoSetF(pErrInfo, VERR_MISMATCH,
                         "Descriptor tag checksum error: %#x, calculated %#x (%.*Rhxs)",
                         pTag->uChecksum, bChecksum, sizeof(*pTag), pTag);
}

typedef struct RTFSISOSEENSEQENCES
{
    /** Number of sequences we've seen thus far. */
    uint32_t cSequences;
    /** The per sequence data. */
    struct
    {
        uint64_t off;   /**< Byte offset of the sequence. */
        uint32_t cb;    /**< Size of the sequence. */
    } aSequences[8];
} RTFSISOSEENSEQENCES;
typedef RTFSISOSEENSEQENCES *PRTFSISOSEENSEQENCES;


/**
 * Process a VDS sequence, recursively dealing with volume descriptor pointers.
 *
 * @returns
 * @param   pThis           .
 * @param   offSeq          .
 * @param   cbSeq           .
 * @param   pbBuf           .
 * @param   cbBuf           .
 * @param   cNestings       The VDS nesting depth.
 * @param   pErrInfo        .
 */
static int rtFsIsoVolReadAndProcessUdfVdsSeq(PRTFSISOVOL pThis, uint64_t offSeq, uint32_t cbSeq, uint8_t *pbBuf, size_t cbBuf,
                                             uint32_t cNestings, PRTERRINFO pErrInfo)
{
    Assert(cbBuf >= _2K);

    /*
     * Check nesting depth.
     */
    if (cNestings > 5)
        return RTErrInfoSet(pErrInfo, VERR_TOO_MUCH_DATA, "The volume descriptor sequence (VDS) is nested too deeply.");


    /*
     * Do the processing sector by sector to keep things simple.
     */
    uint32_t offInSeq = 0;
    while (offInSeq < cbSeq)
    {
        int rc;

        /*
         * Read the next sector.  Zero pad if less that a sector.
         */
        Assert((offInSeq & (pThis->cbSector - 1)) == 0);
        rc = RTVfsFileReadAt(pThis->hVfsBacking, offSeq + offInSeq, pbBuf, pThis->cbSector, NULL);
        if (RT_FAILURE(rc))
            return RTErrInfoSetF(pErrInfo, rc, "Error reading VDS content at %RX64 (LB %#x): %Rrc",
                                 offSeq + offInSeq, pThis->cbSector, rc);
        if (cbSeq - offInSeq < pThis->cbSector)
            memset(&pbBuf[cbSeq - offInSeq], 0, pThis->cbSector - (cbSeq - offInSeq));

        /*
         * Check tag.
         */
        PCUDFTAG pTag = (PCUDFTAG)pbBuf;
        rc = rtFsIsoVolValidateUdfDescTag(pTag, UINT16_MAX, (offSeq + offInSeq) / pThis->cbSector, pErrInfo);
        if (RT_FAILURE(rc))
            return rc;

        switch (pTag->idTag)
        {
            case UDF_TAG_ID_PRIMARY_VOL_DESC:                 /* UDFPRIMARYVOLUMEDESC */
                break;
            case UDF_TAG_ID_ANCHOR_VOLUME_DESC_PTR:           /* UDFANCHORVOLUMEDESCPTR */
                break;
            case UDF_TAG_ID_VOLUME_DESC_PTR:                  /* UDFVOLUMEDESCPTR */
                break;
            case UDF_TAG_ID_IMPLEMENATION_USE_VOLUME_DESC:    /* UDFIMPLEMENTATIONUSEVOLUMEDESC */
                break;
            case UDF_TAG_ID_PARTITION_DESC:                   /* UDFPARTITIONDESC */
                break;
            case UDF_TAG_ID_LOGICAL_VOLUME_DESC:              /* UDFLOGICALVOLUMEDESC */
                break;
            case UDF_TAG_ID_UNALLOCATED_SPACE_DESC:           /* UDFUNALLOCATEDSPACEDESC */
                break;
            case UDF_TAG_ID_TERMINATING_DESC:                 /* UDFTERMINATINGDESC */
                break;
            case UDF_TAG_ID_LOGICAL_VOLUME_INTEGRITY_DESC:    /* UDFLOGICALVOLINTEGRITYDESC */
                break;

        }

        /*
         * Advance.
         */
        offInSeq += pThis->cbSector;
    }

    return VINF_SUCCESS;
}



/**
 * Processes a volume descriptor sequence (VDS).
 *
 * @returns IPRT status code.
 * @param   pThis           The instance.
 * @param   offSeq          The byte offset of the sequence.
 * @param   cbSeq           The length of the sequence.
 * @param   pSeenSequences  Structure where to keep track of VDSes we've already
 *                          processed, to avoid redoing one that we don't
 *                          understand.
 * @param   pbBuf           Read buffer.
 * @param   cbBuf           Size of the read buffer.  This is at least one
 *                          sector big.
 * @param   pErrInfo        Where to report extended error information.
 */
static int rtFsIsoVolReadAndProcessUdfVds(PRTFSISOVOL pThis, uint64_t offSeq, uint32_t cbSeq,
                                          PRTFSISOSEENSEQENCES pSeenSequences, uint8_t *pbBuf, size_t cbBuf,
                                          PRTERRINFO pErrInfo)
{
    /*
     * Skip if already seen.
     */
    uint32_t i = pSeenSequences->cSequences;
    while (i-- > 0)
        if (   pSeenSequences->aSequences[i].off == offSeq
            && pSeenSequences->aSequences[i].cb  == cbSeq)
            return VERR_NOT_FOUND;

    /* Not seen, so add it. */
    Assert(pSeenSequences->cSequences + 1 <= RT_ELEMENTS(pSeenSequences->aSequences));
    pSeenSequences->aSequences[pSeenSequences->cSequences].cb = cbSeq;
    pSeenSequences->aSequences[pSeenSequences->cSequences].off = offSeq;
    pSeenSequences->cSequences++;

    LogFlow(("ISO/UDF: Processing anchor volume descriptor sequence at %#RX64 LB %#RX32\n", offSeq, cbSeq));

    /*
     * Reset the state before we start processing the descriptors.
     */

    NOREF(pThis); NOREF(pbBuf); NOREF(cbBuf); NOREF(pErrInfo);
    return VINF_SUCCESS;
}


static int rtFsIsoVolReadAndHandleUdfAvdp(PRTFSISOVOL pThis, uint64_t offAvdp, uint8_t *pbBuf, size_t cbBuf,
                                          PRTFSISOSEENSEQENCES pSeenSequences, PRTERRINFO pErrInfo)
{
    /*
     * Try read the descriptor and validate its tag.
     */
    PUDFANCHORVOLUMEDESCPTR pAvdp = (PUDFANCHORVOLUMEDESCPTR)pbBuf;
    size_t cbAvdpRead = RT_MIN(pThis->cbSector, cbBuf);
    int rc = RTVfsFileReadAt(pThis->hVfsBacking, offAvdp, pAvdp, cbAvdpRead, NULL);
    if (RT_SUCCESS(rc))
    {
        rc = rtFsIsoVolValidateUdfDescTag(&pAvdp->Tag, UDF_TAG_ID_ANCHOR_VOLUME_DESC_PTR, offAvdp / pThis->cbSector, pErrInfo);
        if (RT_SUCCESS(rc))
        {
            Log2(("ISO/UDF: AVDP: MainVolumeDescSeq=%#RX32 LB %#RX32, ReserveVolumeDescSeq=%#RX32 LB %#RX32\n",
                  pAvdp->MainVolumeDescSeq.off, pAvdp->MainVolumeDescSeq.cb,
                  pAvdp->ReserveVolumeDescSeq.off, pAvdp->ReserveVolumeDescSeq.cb));

            /*
             * Try the main sequence if it looks sane.
             */
            UDFEXTENTAD const ReserveVolumeDescSeq = pAvdp->ReserveVolumeDescSeq;
            if (   pAvdp->MainVolumeDescSeq.off < pThis->cBackingSectors
                &&     (uint64_t)pAvdp->MainVolumeDescSeq.off
                     + (pAvdp->MainVolumeDescSeq.cb + pThis->cbSector - 1) / pThis->cbSector
                   <= pThis->cBackingSectors)
            {
                rc = rtFsIsoVolReadAndProcessUdfVds(pThis, (uint64_t)pAvdp->MainVolumeDescSeq.off * pThis->cbSector,
                                                    pAvdp->MainVolumeDescSeq.cb, pSeenSequences, pbBuf, cbBuf, pErrInfo);
                if (RT_SUCCESS(rc))
                    return rc;
            }
            else
                rc = RTErrInfoSetF(pErrInfo, VERR_NOT_FOUND,
                                   "MainVolumeDescSeq is out of bounds: sector %#RX32 LB %#RX32 bytes, image is %#RX64 sectors",
                                   pAvdp->MainVolumeDescSeq.off, pAvdp->MainVolumeDescSeq.cb, pThis->cBackingSectors);
            if (ReserveVolumeDescSeq.cb > 0)
            {
                if (   ReserveVolumeDescSeq.off < pThis->cBackingSectors
                    &&     (uint64_t)ReserveVolumeDescSeq.off
                         + (ReserveVolumeDescSeq.cb + pThis->cbSector - 1) / pThis->cbSector
                       <= pThis->cBackingSectors)
                {
                    rc = rtFsIsoVolReadAndProcessUdfVds(pThis, (uint64_t)ReserveVolumeDescSeq.off * pThis->cbSector,
                                                        ReserveVolumeDescSeq.cb, pSeenSequences, pbBuf, cbBuf, pErrInfo);
                    if (RT_SUCCESS(rc))
                        return rc;
                }
                else if (RT_SUCCESS(rc))
                    rc = RTErrInfoSetF(pErrInfo, VERR_NOT_FOUND,
                                       "ReserveVolumeDescSeq is out of bounds: sector %#RX32 LB %#RX32 bytes, image is %#RX64 sectors",
                                       ReserveVolumeDescSeq.off, ReserveVolumeDescSeq.cb, pThis->cBackingSectors);
            }
        }
    }
    else
        rc = RTErrInfoSetF(pErrInfo, rc,
                           "Error reading sector at offset %#RX64 (anchor volume descriptor pointer): %Rrc", offAvdp, rc);

    return rc;
}


/**
 * Goes looking for UDF when we've seens a volume recognition sequence.
 *
 * @returns IPRT status code.
 * @param   pThis               The volume instance data.
 * @param   puUdfLevel          The UDF level indicated by the VRS.
 * @param   offUdfBootVolDesc   The offset of the BOOT2 descriptor, UINT64_MAX
 *                              if not encountered.
 * @param   pbBuf               Buffer for reading into.
 * @param   cbBuf               The size of the buffer.  At least one sector.
 * @param   pErrInfo            Where to return extended error info.
 */
static int rtFsIsoVolHandleUdfDetection(PRTFSISOVOL pThis, uint8_t *puUdfLevel, uint64_t offUdfBootVolDesc,
                                        uint8_t *pbBuf, size_t cbBuf, PRTERRINFO pErrInfo)
{
    NOREF(offUdfBootVolDesc);

    /*
     * There are up to three anchor volume descriptor pointers that can give us
     * two different descriptor sequences each.  Usually, the different AVDP
     * structures points to the same two sequences.  The idea here is that
     * sectors may deteriorate and become unreadable, and we're supposed to try
     * out alternative sectors to get the job done.  If we really took this
     * seriously, we could try read all sequences in parallel and use the
     * sectors that are good.  However, we'll try keep things reasonably simple
     * since we'll most likely be reading from hard disks rather than optical
     * media.
     *
     * We keep track of which sequences we've processed so we don't try to do it
     * again when alternative AVDP sectors points to the same sequences.
     */
    RTFSISOSEENSEQENCES SeenSequences = { 0 };
    int rc = rtFsIsoVolReadAndHandleUdfAvdp(pThis, 256 * pThis->cbSector, pbBuf, cbBuf,
                                            &SeenSequences, pErrInfo);
    if (RT_FAILURE(rc))
        rc = rtFsIsoVolReadAndHandleUdfAvdp(pThis,  pThis->cbBacking - 256 * pThis->cbSector,
                                            pbBuf, cbBuf, &SeenSequences, pErrInfo);
    if (RT_FAILURE(rc))
        rc = rtFsIsoVolReadAndHandleUdfAvdp(pThis,  pThis->cbBacking - pThis->cbSector,
                                            pbBuf, cbBuf, &SeenSequences, pErrInfo);
    if (RT_SUCCESS(rc))
        return rc;
    *puUdfLevel = 0;
    return VINF_SUCCESS;
}



#ifdef LOG_ENABLED

/** Logging helper. */
static size_t rtFsIsoVolGetStrippedLength(const char *pachField, size_t cchField)
{
    while (cchField > 0 && pachField[cchField - 1] == ' ')
        cchField--;
    return cchField;
}

/** Logging helper. */
static char *rtFsIsoVolGetMaybeUtf16Be(const char *pachField, size_t cchField, char *pszDst, size_t cbDst)
{
    /* Check the format by looking for zero bytes.  ISO-9660 doesn't allow zeros.
       This doesn't have to be a UTF-16BE string.  */
    size_t cFirstZeros  = 0;
    size_t cSecondZeros = 0;
    for (size_t off = 0; off + 1 < cchField; off += 2)
    {
        cFirstZeros  += pachField[off]     == '\0';
        cSecondZeros += pachField[off + 1] == '\0';
    }

    int    rc     = VINF_SUCCESS;
    char  *pszTmp = &pszDst[10];
    size_t cchRet = 0;
    if (cFirstZeros > cSecondZeros)
    {
        /* UTF-16BE / UTC-2BE: */
        if (cchField & 1)
        {
            if (pachField[cchField - 1] == '\0' || pachField[cchField - 1] == ' ')
                cchField--;
            else
                rc = VERR_INVALID_UTF16_ENCODING;
        }
        if (RT_SUCCESS(rc))
        {
            while (   cchField >= 2
                   && pachField[cchField - 1] == ' '
                   && pachField[cchField - 2] == '\0')
                cchField -= 2;

            rc = RTUtf16BigToUtf8Ex((PCRTUTF16)pachField, cchField / sizeof(RTUTF16), &pszTmp, cbDst - 10 - 1, &cchRet);
        }
        if (RT_SUCCESS(rc))
        {
            pszDst[0] = 'U';
            pszDst[1] = 'T';
            pszDst[2] = 'F';
            pszDst[3] = '-';
            pszDst[4] = '1';
            pszDst[5] = '6';
            pszDst[6] = 'B';
            pszDst[7] = 'E';
            pszDst[8] = ':';
            pszDst[9] = '\'';
            pszDst[10 + cchRet] = '\'';
            pszDst[10 + cchRet + 1] = '\0';
        }
        else
            RTStrPrintf(pszDst, cbDst, "UTF-16BE: %.*Rhxs", cchField, pachField);
    }
    else if (cSecondZeros > 0)
    {
        /* Little endian UTF-16 / UCS-2 (ASSUMES host is little endian, sorry) */
        if (cchField & 1)
        {
            if (pachField[cchField - 1] == '\0' || pachField[cchField - 1] == ' ')
                cchField--;
            else
                rc = VERR_INVALID_UTF16_ENCODING;
        }
        if (RT_SUCCESS(rc))
        {
            while (   cchField >= 2
                   && pachField[cchField - 1] == '\0'
                   && pachField[cchField - 2] == ' ')
                cchField -= 2;

            rc = RTUtf16ToUtf8Ex((PCRTUTF16)pachField, cchField / sizeof(RTUTF16), &pszTmp, cbDst - 10 - 1, &cchRet);
        }
        if (RT_SUCCESS(rc))
        {
            pszDst[0] = 'U';
            pszDst[1] = 'T';
            pszDst[2] = 'F';
            pszDst[3] = '-';
            pszDst[4] = '1';
            pszDst[5] = '6';
            pszDst[6] = 'L';
            pszDst[7] = 'E';
            pszDst[8] = ':';
            pszDst[9] = '\'';
            pszDst[10 + cchRet] = '\'';
            pszDst[10 + cchRet + 1] = '\0';
        }
        else
            RTStrPrintf(pszDst, cbDst, "UTF-16LE: %.*Rhxs", cchField, pachField);
    }
    else
    {
        /* ASSUME UTF-8/ASCII. */
        while (   cchField > 0
               && pachField[cchField - 1] == ' ')
            cchField--;
        rc = RTStrValidateEncodingEx(pachField, cchField, RTSTR_VALIDATE_ENCODING_EXACT_LENGTH);
        if (RT_SUCCESS(rc))
            RTStrPrintf(pszDst, cbDst, "UTF-8: '%.*s'", cchField, pachField);
        else
            RTStrPrintf(pszDst, cbDst, "UNK-8: %.*Rhxs", cchField, pachField);
    }
    return pszDst;
}


/**
 * Logs the primary or supplementary volume descriptor
 *
 * @param   pVolDesc            The descriptor.
 */
static void rtFsIsoVolLogPrimarySupplementaryVolDesc(PCISO9660SUPVOLDESC pVolDesc)
{
    if (LogIs2Enabled())
    {
        char szTmp[384];
        Log2(("ISO9660:  fVolumeFlags:              %#RX8\n", pVolDesc->fVolumeFlags));
        Log2(("ISO9660:  achSystemId:               %s\n", rtFsIsoVolGetMaybeUtf16Be(pVolDesc->achSystemId, sizeof(pVolDesc->achSystemId), szTmp, sizeof(szTmp)) ));
        Log2(("ISO9660:  achVolumeId:               %s\n", rtFsIsoVolGetMaybeUtf16Be(pVolDesc->achVolumeId, sizeof(pVolDesc->achVolumeId), szTmp, sizeof(szTmp)) ));
        Log2(("ISO9660:  Unused73:                  {%#RX32,%#RX32}\n", RT_BE2H_U32(pVolDesc->Unused73.be), RT_LE2H_U32(pVolDesc->Unused73.le)));
        Log2(("ISO9660:  VolumeSpaceSize:           {%#RX32,%#RX32}\n", RT_BE2H_U32(pVolDesc->VolumeSpaceSize.be), RT_LE2H_U32(pVolDesc->VolumeSpaceSize.le)));
        Log2(("ISO9660:  abEscapeSequences:         '%.*s'\n", rtFsIsoVolGetStrippedLength((char *)pVolDesc->abEscapeSequences, sizeof(pVolDesc->abEscapeSequences)), pVolDesc->abEscapeSequences));
        Log2(("ISO9660:  cVolumesInSet:             {%#RX16,%#RX16}\n", RT_BE2H_U16(pVolDesc->cVolumesInSet.be), RT_LE2H_U16(pVolDesc->cVolumesInSet.le)));
        Log2(("ISO9660:  VolumeSeqNo:               {%#RX16,%#RX16}\n", RT_BE2H_U16(pVolDesc->VolumeSeqNo.be), RT_LE2H_U16(pVolDesc->VolumeSeqNo.le)));
        Log2(("ISO9660:  cbLogicalBlock:            {%#RX16,%#RX16}\n", RT_BE2H_U16(pVolDesc->cbLogicalBlock.be), RT_LE2H_U16(pVolDesc->cbLogicalBlock.le)));
        Log2(("ISO9660:  cbPathTable:               {%#RX32,%#RX32}\n", RT_BE2H_U32(pVolDesc->cbPathTable.be), RT_LE2H_U32(pVolDesc->cbPathTable.le)));
        Log2(("ISO9660:  offTypeLPathTable:         %#RX32\n", RT_LE2H_U32(pVolDesc->offTypeLPathTable)));
        Log2(("ISO9660:  offOptionalTypeLPathTable: %#RX32\n", RT_LE2H_U32(pVolDesc->offOptionalTypeLPathTable)));
        Log2(("ISO9660:  offTypeMPathTable:         %#RX32\n", RT_BE2H_U32(pVolDesc->offTypeMPathTable)));
        Log2(("ISO9660:  offOptionalTypeMPathTable: %#RX32\n", RT_BE2H_U32(pVolDesc->offOptionalTypeMPathTable)));
        Log2(("ISO9660:  achVolumeSetId:            %s\n", rtFsIsoVolGetMaybeUtf16Be(pVolDesc->achVolumeSetId, sizeof(pVolDesc->achVolumeSetId), szTmp, sizeof(szTmp)) ));
        Log2(("ISO9660:  achPublisherId:            %s\n", rtFsIsoVolGetMaybeUtf16Be(pVolDesc->achPublisherId, sizeof(pVolDesc->achPublisherId), szTmp, sizeof(szTmp)) ));
        Log2(("ISO9660:  achDataPreparerId:         %s\n", rtFsIsoVolGetMaybeUtf16Be(pVolDesc->achDataPreparerId, sizeof(pVolDesc->achDataPreparerId), szTmp, sizeof(szTmp)) ));
        Log2(("ISO9660:  achApplicationId:          %s\n", rtFsIsoVolGetMaybeUtf16Be(pVolDesc->achApplicationId, sizeof(pVolDesc->achApplicationId), szTmp, sizeof(szTmp)) ));
        Log2(("ISO9660:  achCopyrightFileId:        %s\n", rtFsIsoVolGetMaybeUtf16Be(pVolDesc->achCopyrightFileId, sizeof(pVolDesc->achCopyrightFileId), szTmp, sizeof(szTmp)) ));
        Log2(("ISO9660:  achAbstractFileId:         %s\n", rtFsIsoVolGetMaybeUtf16Be(pVolDesc->achAbstractFileId, sizeof(pVolDesc->achAbstractFileId), szTmp, sizeof(szTmp)) ));
        Log2(("ISO9660:  achBibliographicFileId:    %s\n", rtFsIsoVolGetMaybeUtf16Be(pVolDesc->achBibliographicFileId, sizeof(pVolDesc->achBibliographicFileId), szTmp, sizeof(szTmp)) ));
        Log2(("ISO9660:  BirthTime:                 %.4s-%.2s-%.2s %.2s:%.2s:%.2s.%.2s%+03d\n",
              pVolDesc->BirthTime.achYear,
              pVolDesc->BirthTime.achMonth,
              pVolDesc->BirthTime.achDay,
              pVolDesc->BirthTime.achHour,
              pVolDesc->BirthTime.achMinute,
              pVolDesc->BirthTime.achSecond,
              pVolDesc->BirthTime.achCentisecond,
              pVolDesc->BirthTime.offUtc*4/60));
        Log2(("ISO9660:  ModifyTime:                %.4s-%.2s-%.2s %.2s:%.2s:%.2s.%.2s%+03d\n",
              pVolDesc->ModifyTime.achYear,
              pVolDesc->ModifyTime.achMonth,
              pVolDesc->ModifyTime.achDay,
              pVolDesc->ModifyTime.achHour,
              pVolDesc->ModifyTime.achMinute,
              pVolDesc->ModifyTime.achSecond,
              pVolDesc->ModifyTime.achCentisecond,
              pVolDesc->ModifyTime.offUtc*4/60));
        Log2(("ISO9660:  ExpireTime:                %.4s-%.2s-%.2s %.2s:%.2s:%.2s.%.2s%+03d\n",
              pVolDesc->ExpireTime.achYear,
              pVolDesc->ExpireTime.achMonth,
              pVolDesc->ExpireTime.achDay,
              pVolDesc->ExpireTime.achHour,
              pVolDesc->ExpireTime.achMinute,
              pVolDesc->ExpireTime.achSecond,
              pVolDesc->ExpireTime.achCentisecond,
              pVolDesc->ExpireTime.offUtc*4/60));
        Log2(("ISO9660:  EffectiveTime:             %.4s-%.2s-%.2s %.2s:%.2s:%.2s.%.2s%+03d\n",
              pVolDesc->EffectiveTime.achYear,
              pVolDesc->EffectiveTime.achMonth,
              pVolDesc->EffectiveTime.achDay,
              pVolDesc->EffectiveTime.achHour,
              pVolDesc->EffectiveTime.achMinute,
              pVolDesc->EffectiveTime.achSecond,
              pVolDesc->EffectiveTime.achCentisecond,
              pVolDesc->EffectiveTime.offUtc*4/60));
        Log2(("ISO9660:  bFileStructureVersion:     %#RX8\n", pVolDesc->bFileStructureVersion));
        Log2(("ISO9660:  bReserved883:              %#RX8\n", pVolDesc->bReserved883));

        Log2(("ISO9660:  RootDir.cbDirRec:                   %#RX8\n", pVolDesc->RootDir.DirRec.cbDirRec));
        Log2(("ISO9660:  RootDir.cExtAttrBlocks:             %#RX8\n", pVolDesc->RootDir.DirRec.cExtAttrBlocks));
        Log2(("ISO9660:  RootDir.offExtent:                  {%#RX32,%#RX32}\n", RT_BE2H_U32(pVolDesc->RootDir.DirRec.offExtent.be), RT_LE2H_U32(pVolDesc->RootDir.DirRec.offExtent.le)));
        Log2(("ISO9660:  RootDir.cbData:                     {%#RX32,%#RX32}\n", RT_BE2H_U32(pVolDesc->RootDir.DirRec.cbData.be), RT_LE2H_U32(pVolDesc->RootDir.DirRec.cbData.le)));
        Log2(("ISO9660:  RootDir.RecTime:                    %04u-%02u-%02u %02u:%02u:%02u%+03d\n",
              pVolDesc->RootDir.DirRec.RecTime.bYear + 1900,
              pVolDesc->RootDir.DirRec.RecTime.bMonth,
              pVolDesc->RootDir.DirRec.RecTime.bDay,
              pVolDesc->RootDir.DirRec.RecTime.bHour,
              pVolDesc->RootDir.DirRec.RecTime.bMinute,
              pVolDesc->RootDir.DirRec.RecTime.bSecond,
              pVolDesc->RootDir.DirRec.RecTime.offUtc*4/60));
        Log2(("ISO9660:  RootDir.RecTime.fFileFlags:         %RX8\n", pVolDesc->RootDir.DirRec.fFileFlags));
        Log2(("ISO9660:  RootDir.RecTime.bFileUnitSize:      %RX8\n", pVolDesc->RootDir.DirRec.bFileUnitSize));
        Log2(("ISO9660:  RootDir.RecTime.bInterleaveGapSize: %RX8\n", pVolDesc->RootDir.DirRec.bInterleaveGapSize));
        Log2(("ISO9660:  RootDir.RecTime.VolumeSeqNo:        {%#RX16,%#RX16}\n", RT_BE2H_U16(pVolDesc->RootDir.DirRec.VolumeSeqNo.be), RT_LE2H_U16(pVolDesc->RootDir.DirRec.VolumeSeqNo.le)));
        Log2(("ISO9660:  RootDir.RecTime.bFileIdLength:      %RX8\n", pVolDesc->RootDir.DirRec.bFileIdLength));
        Log2(("ISO9660:  RootDir.RecTime.achFileId:          '%.*s'\n", pVolDesc->RootDir.DirRec.bFileIdLength, pVolDesc->RootDir.DirRec.achFileId));
        uint32_t offSysUse = RT_OFFSETOF(ISO9660DIRREC, achFileId[pVolDesc->RootDir.DirRec.bFileIdLength])
                           + !(pVolDesc->RootDir.DirRec.bFileIdLength & 1);
        if (offSysUse < pVolDesc->RootDir.DirRec.cbDirRec)
        {
            Log2(("ISO9660:  RootDir System Use:\n%.*Rhxd\n",
                  pVolDesc->RootDir.DirRec.cbDirRec - offSysUse, &pVolDesc->RootDir.ab[offSysUse]));
        }
    }
}

#endif /* LOG_ENABLED */

/**
 * Deal with a root directory from a primary or supplemental descriptor.
 *
 * @returns IPRT status code.
 * @param   pThis           The ISO 9660 instance being initialized.
 * @param   pRootDir        The root directory record to check out.
 * @param   pDstRootDir     Where to store a copy of the root dir record.
 * @param   pErrInfo        Where to return additional error info.  Can be NULL.
 */
static int rtFsIsoVolHandleRootDir(PRTFSISOVOL pThis, PCISO9660DIRREC pRootDir,
                                   PISO9660DIRREC pDstRootDir, PRTERRINFO pErrInfo)
{
    if (pRootDir->cbDirRec < RT_OFFSETOF(ISO9660DIRREC, achFileId))
        return RTErrInfoSetF(pErrInfo, VERR_VFS_BOGUS_FORMAT, "Root dir record size is too small: %#x (min %#x)",
                             pRootDir->cbDirRec, RT_OFFSETOF(ISO9660DIRREC, achFileId));

    if (!(pRootDir->fFileFlags & ISO9660_FILE_FLAGS_DIRECTORY))
        return RTErrInfoSetF(pErrInfo, VERR_VFS_BOGUS_FORMAT,
                             "Root dir is not flagged as directory: %#x", pRootDir->fFileFlags);
    if (pRootDir->fFileFlags & ISO9660_FILE_FLAGS_MULTI_EXTENT)
        return RTErrInfoSetF(pErrInfo, VERR_VFS_BOGUS_FORMAT,
                             "Root dir is cannot be multi-extent: %#x", pRootDir->fFileFlags);

    if (RT_LE2H_U32(pRootDir->cbData.le) != RT_BE2H_U32(pRootDir->cbData.be))
        return RTErrInfoSetF(pErrInfo, VERR_VFS_BOGUS_FORMAT, "Invalid root dir size: {%#RX32,%#RX32}",
                             RT_BE2H_U32(pRootDir->cbData.be), RT_LE2H_U32(pRootDir->cbData.le));
    if (RT_LE2H_U32(pRootDir->cbData.le) == 0)
        return RTErrInfoSet(pErrInfo, VERR_VFS_BOGUS_FORMAT, "Zero sized root dir");

    if (RT_LE2H_U32(pRootDir->offExtent.le) != RT_BE2H_U32(pRootDir->offExtent.be))
        return RTErrInfoSetF(pErrInfo, VERR_VFS_BOGUS_FORMAT, "Invalid root dir extent: {%#RX32,%#RX32}",
                             RT_BE2H_U32(pRootDir->offExtent.be), RT_LE2H_U32(pRootDir->offExtent.le));

    if (RT_LE2H_U16(pRootDir->VolumeSeqNo.le) != RT_BE2H_U16(pRootDir->VolumeSeqNo.be))
        return RTErrInfoSetF(pErrInfo, VERR_VFS_BOGUS_FORMAT, "Invalid root dir volume sequence ID: {%#RX16,%#RX16}",
                             RT_BE2H_U16(pRootDir->VolumeSeqNo.be), RT_LE2H_U16(pRootDir->VolumeSeqNo.le));
    if (RT_LE2H_U16(pRootDir->VolumeSeqNo.le) != pThis->idPrimaryVol)
        return RTErrInfoSetF(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT,
                             "Expected root dir to have same volume sequence number as primary volume: %#x, expected %#x",
                             RT_LE2H_U16(pRootDir->VolumeSeqNo.le), pThis->idPrimaryVol);

    /*
     * Seems okay, copy it.
     */
    *pDstRootDir = *pRootDir;
    return VINF_SUCCESS;
}


/**
 * Deal with a primary volume descriptor.
 *
 * @returns IPRT status code.
 * @param   pThis           The ISO 9660 instance being initialized.
 * @param   pVolDesc        The volume descriptor to handle.
 * @param   offVolDesc      The disk offset of the volume descriptor.
 * @param   pRootDir        Where to return a copy of the root directory record.
 * @param   poffRootDirRec  Where to return the disk offset of the root dir.
 * @param   pErrInfo        Where to return additional error info.  Can be NULL.
 */
static int rtFsIsoVolHandlePrimaryVolDesc(PRTFSISOVOL pThis, PCISO9660PRIMARYVOLDESC pVolDesc, uint32_t offVolDesc,
                                          PISO9660DIRREC pRootDir, uint64_t *poffRootDirRec, PRTERRINFO pErrInfo)
{
    if (pVolDesc->bFileStructureVersion != ISO9660_FILE_STRUCTURE_VERSION)
        return RTErrInfoSetF(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT,
                             "Unsupported file structure version: %#x", pVolDesc->bFileStructureVersion);

    /*
     * We need the block size ...
     */
    pThis->cbBlock = RT_LE2H_U16(pVolDesc->cbLogicalBlock.le);
    if (   pThis->cbBlock != RT_BE2H_U16(pVolDesc->cbLogicalBlock.be)
        || !RT_IS_POWER_OF_TWO(pThis->cbBlock)
        || pThis->cbBlock / pThis->cbSector < 1)
        return RTErrInfoSetF(pErrInfo, VERR_VFS_BOGUS_FORMAT, "Invalid logical block size: {%#RX16,%#RX16}",
                             RT_BE2H_U16(pVolDesc->cbLogicalBlock.be), RT_LE2H_U16(pVolDesc->cbLogicalBlock.le));
    if (pThis->cbBlock / pThis->cbSector > 128)
        return RTErrInfoSetF(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT, "Unsupported block size: %#x\n", pThis->cbBlock);

    /*
     * ... volume space size ...
     */
    pThis->cBlocksInPrimaryVolumeSpace = RT_LE2H_U32(pVolDesc->VolumeSpaceSize.le);
    if (pThis->cBlocksInPrimaryVolumeSpace != RT_BE2H_U32(pVolDesc->VolumeSpaceSize.be))
        return RTErrInfoSetF(pErrInfo, VERR_VFS_BOGUS_FORMAT, "Invalid volume space size: {%#RX32,%#RX32}",
                             RT_BE2H_U32(pVolDesc->VolumeSpaceSize.be), RT_LE2H_U32(pVolDesc->VolumeSpaceSize.le));
    pThis->cbPrimaryVolumeSpace = pThis->cBlocksInPrimaryVolumeSpace * (uint64_t)pThis->cbBlock;

    /*
     * ... number of volumes in the set ...
     */
    pThis->cVolumesInSet = RT_LE2H_U16(pVolDesc->cVolumesInSet.le);
    if (   pThis->cVolumesInSet != RT_BE2H_U16(pVolDesc->cVolumesInSet.be)
        || pThis->cVolumesInSet == 0)
        return RTErrInfoSetF(pErrInfo, VERR_VFS_BOGUS_FORMAT, "Invalid volume set size: {%#RX16,%#RX16}",
                             RT_BE2H_U16(pVolDesc->cVolumesInSet.be), RT_LE2H_U16(pVolDesc->cVolumesInSet.le));
    if (pThis->cVolumesInSet > 32)
        return RTErrInfoSetF(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT, "Too large volume set size: %#x\n", pThis->cVolumesInSet);

    /*
     * ... primary volume sequence ID ...
     */
    pThis->idPrimaryVol = RT_LE2H_U16(pVolDesc->VolumeSeqNo.le);
    if (pThis->idPrimaryVol != RT_BE2H_U16(pVolDesc->VolumeSeqNo.be))
        return RTErrInfoSetF(pErrInfo, VERR_VFS_BOGUS_FORMAT, "Invalid volume sequence ID: {%#RX16,%#RX16}",
                             RT_BE2H_U16(pVolDesc->VolumeSeqNo.be), RT_LE2H_U16(pVolDesc->VolumeSeqNo.le));
    if (   pThis->idPrimaryVol > pThis->cVolumesInSet
        || pThis->idPrimaryVol < 1)
        return RTErrInfoSetF(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT,
                             "Volume sequence ID out of of bound: %#x (1..%#x)\n", pThis->idPrimaryVol, pThis->cVolumesInSet);

    /*
     * ... and the root directory record.
     */
    *poffRootDirRec = offVolDesc + RT_OFFSETOF(ISO9660PRIMARYVOLDESC, RootDir.DirRec);
    return rtFsIsoVolHandleRootDir(pThis, &pVolDesc->RootDir.DirRec, pRootDir, pErrInfo);
}


/**
 * Deal with a supplementary volume descriptor.
 *
 * @returns IPRT status code.
 * @param   pThis           The ISO 9660 instance being initialized.
 * @param   pVolDesc        The volume descriptor to handle.
 * @param   offVolDesc      The disk offset of the volume descriptor.
 * @param   pbUcs2Level     Where to return the joliet level, if found. Caller
 *                          initializes this to zero, we'll return 1, 2 or 3 if
 *                          joliet was detected.
 * @param   pRootDir        Where to return the root directory, if found.
 * @param   poffRootDirRec  Where to return the disk offset of the root dir.
 * @param   pErrInfo        Where to return additional error info.  Can be NULL.
 */
static int rtFsIsoVolHandleSupplementaryVolDesc(PRTFSISOVOL pThis, PCISO9660SUPVOLDESC pVolDesc, uint32_t offVolDesc,
                                                uint8_t *pbUcs2Level, PISO9660DIRREC pRootDir, uint64_t *poffRootDirRec,
                                                PRTERRINFO pErrInfo)
{
    if (pVolDesc->bFileStructureVersion != ISO9660_FILE_STRUCTURE_VERSION)
        return RTErrInfoSetF(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT,
                             "Unsupported file structure version: %#x", pVolDesc->bFileStructureVersion);

    /*
     * Is this a joliet volume descriptor?  If not, we probably don't need to
     * care about it.
     */
    if (   pVolDesc->abEscapeSequences[0] != ISO9660_JOLIET_ESC_SEQ_0
        || pVolDesc->abEscapeSequences[1] != ISO9660_JOLIET_ESC_SEQ_1
        || (   pVolDesc->abEscapeSequences[2] != ISO9660_JOLIET_ESC_SEQ_2_LEVEL_1
            && pVolDesc->abEscapeSequences[2] != ISO9660_JOLIET_ESC_SEQ_2_LEVEL_2
            && pVolDesc->abEscapeSequences[2] != ISO9660_JOLIET_ESC_SEQ_2_LEVEL_3))
        return VINF_SUCCESS;

    /*
     * Skip if joliet is unwanted.
     */
    if (pThis->fFlags & RTFSISO9660_F_NO_JOLIET)
        return VINF_SUCCESS;

    /*
     * Check that the joliet descriptor matches the primary one.
     * Note! These are our assumptions and may be wrong.
     */
    if (pThis->cbBlock == 0)
        return RTErrInfoSet(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT,
                            "Supplementary joliet volume descriptor is not supported when appearing before the primary volume descriptor");
    if (ISO9660_GET_ENDIAN(&pVolDesc->cbLogicalBlock) != pThis->cbBlock)
        return RTErrInfoSetF(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT,
                             "Logical block size for joliet volume descriptor differs from primary: %#RX16 vs %#RX16\n",
                             ISO9660_GET_ENDIAN(&pVolDesc->cbLogicalBlock), pThis->cbBlock);
    if (ISO9660_GET_ENDIAN(&pVolDesc->VolumeSpaceSize) != pThis->cBlocksInPrimaryVolumeSpace)
        return RTErrInfoSetF(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT,
                             "Volume space size for joliet volume descriptor differs from primary: %#RX32 vs %#RX32\n",
                             ISO9660_GET_ENDIAN(&pVolDesc->VolumeSpaceSize), pThis->cBlocksInPrimaryVolumeSpace);
    if (ISO9660_GET_ENDIAN(&pVolDesc->cVolumesInSet) != pThis->cVolumesInSet)
        return RTErrInfoSetF(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT,
                             "Volume set size for joliet volume descriptor differs from primary: %#RX16 vs %#RX32\n",
                             ISO9660_GET_ENDIAN(&pVolDesc->cVolumesInSet), pThis->cVolumesInSet);
    if (ISO9660_GET_ENDIAN(&pVolDesc->VolumeSeqNo) != pThis->idPrimaryVol)
        return RTErrInfoSetF(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT,
                             "Volume sequence ID for joliet volume descriptor differs from primary: %#RX16 vs %#RX32\n",
                             ISO9660_GET_ENDIAN(&pVolDesc->VolumeSeqNo), pThis->idPrimaryVol);

    if (*pbUcs2Level != 0)
        return RTErrInfoSet(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT, "More than one supplementary joliet volume descriptor");

    /*
     * Switch to the joliet root dir as it has UTF-16 stuff in it.
     */
    int rc = rtFsIsoVolHandleRootDir(pThis, &pVolDesc->RootDir.DirRec, pRootDir, pErrInfo);
    if (RT_SUCCESS(rc))
    {
        *poffRootDirRec = offVolDesc + RT_OFFSETOF(ISO9660SUPVOLDESC, RootDir.DirRec);
        *pbUcs2Level    = pVolDesc->abEscapeSequences[2] == ISO9660_JOLIET_ESC_SEQ_2_LEVEL_1 ? 1
                        : pVolDesc->abEscapeSequences[2] == ISO9660_JOLIET_ESC_SEQ_2_LEVEL_2 ? 2 : 3;
        Log(("ISO9660: Joliet with UCS-2 level %u\n", *pbUcs2Level));
    }
    return rc;
}



/**
 * Worker for RTFsIso9660VolOpen.
 *
 * @returns IPRT status code.
 * @param   pThis           The ISO VFS instance to initialize.
 * @param   hVfsSelf        The ISO VFS handle (no reference consumed).
 * @param   hVfsBacking     The file backing the alleged FAT file system.
 *                          Reference is consumed (via rtFsIsoVol_Close).
 * @param   fFlags          Flags, RTFSISO9660_F_XXX.
 * @param   pErrInfo        Where to return additional error info.  Can be NULL.
 */
static int rtFsIsoVolTryInit(PRTFSISOVOL pThis, RTVFS hVfsSelf, RTVFSFILE hVfsBacking, uint32_t fFlags, PRTERRINFO pErrInfo)
{
    uint32_t const cbSector = 2048;

    /*
     * First initialize the state so that rtFsIsoVol_Destroy won't trip up.
     */
    pThis->hVfsSelf                     = hVfsSelf;
    pThis->hVfsBacking                  = hVfsBacking; /* Caller referenced it for us, we consume it; rtFsIsoVol_Destroy releases it. */
    pThis->cbBacking                    = 0;
    pThis->cBackingSectors              = 0;
    pThis->fFlags                       = fFlags;
    pThis->cbSector                     = cbSector;
    pThis->cbBlock                      = 0;
    pThis->cBlocksInPrimaryVolumeSpace  = 0;
    pThis->cbPrimaryVolumeSpace         = 0;
    pThis->cVolumesInSet                = 0;
    pThis->idPrimaryVol                 = UINT32_MAX;
    pThis->fIsUtf16                     = false;
    pThis->pRootDir                     = NULL;

    /*
     * Get stuff that may fail.
     */
    int rc = RTVfsFileGetSize(hVfsBacking, &pThis->cbBacking);
    if (RT_SUCCESS(rc))
        pThis->cBackingSectors = pThis->cbBacking / pThis->cbSector;
    else
        return rc;

    /*
     * Read the volume descriptors starting at logical sector 16.
     */
    union
    {
        uint8_t                 ab[_4K];
        uint16_t                au16[_4K / 2];
        uint32_t                au32[_4K / 4];
        ISO9660VOLDESCHDR       VolDescHdr;
        ISO9660BOOTRECORD       BootRecord;
        ISO9660PRIMARYVOLDESC   PrimaryVolDesc;
        ISO9660SUPVOLDESC       SupVolDesc;
        ISO9660VOLPARTDESC      VolPartDesc;
    } Buf;
    RT_ZERO(Buf);

    uint64_t        offRootDirRec           = UINT64_MAX;
    ISO9660DIRREC   RootDir;
    RT_ZERO(RootDir);

    uint64_t        offJolietRootDirRec     = UINT64_MAX;
    uint8_t         bJolietUcs2Level        = 0;
    ISO9660DIRREC   JolietRootDir;
    RT_ZERO(JolietRootDir);

    uint8_t         uUdfLevel               = 0;
    uint64_t        offUdfBootVolDesc       = UINT64_MAX;

    uint32_t        cPrimaryVolDescs        = 0;
    uint32_t        cSupplementaryVolDescs  = 0;
    uint32_t        cBootRecordVolDescs     = 0;
    uint32_t        offVolDesc              = 16 * cbSector;
    enum
    {
        kStateStart = 0,
        kStateNoSeq,
        kStateCdSeq,
        kStateUdfSeq
    }               enmState = kStateStart;
    for (uint32_t iVolDesc = 0; ; iVolDesc++, offVolDesc += cbSector)
    {
        if (iVolDesc > 32)
            return RTErrInfoSet(pErrInfo, VERR_VFS_BOGUS_FORMAT, "More than 32 volume descriptors, doesn't seem right...");

        /* Read the next one and check the signature. */
        rc = RTVfsFileReadAt(hVfsBacking, offVolDesc, &Buf, cbSector, NULL);
        if (RT_FAILURE(rc))
            return RTErrInfoSetF(pErrInfo, rc, "Unable to read volume descriptor #%u", iVolDesc);

#define MATCH_STD_ID(a_achStdId1, a_szStdId2) \
            (   (a_achStdId1)[0] == (a_szStdId2)[0] \
             && (a_achStdId1)[1] == (a_szStdId2)[1] \
             && (a_achStdId1)[2] == (a_szStdId2)[2] \
             && (a_achStdId1)[3] == (a_szStdId2)[3] \
             && (a_achStdId1)[4] == (a_szStdId2)[4] )
#define MATCH_HDR(a_pStd, a_bType2, a_szStdId2, a_bVer2) \
            (    MATCH_STD_ID((a_pStd)->achStdId, a_szStdId2) \
             && (a_pStd)->bDescType    == (a_bType2) \
             && (a_pStd)->bDescVersion == (a_bVer2) )

        /*
         * ISO 9660 ("CD001").
         */
        if (   (   enmState == kStateStart
                || enmState == kStateCdSeq
                || enmState == kStateNoSeq)
            && MATCH_STD_ID(Buf.VolDescHdr.achStdId, ISO9660VOLDESC_STD_ID) )
        {
            enmState = kStateCdSeq;

            /* Do type specific handling. */
            Log(("ISO9660: volume desc #%u: type=%#x\n", iVolDesc, Buf.VolDescHdr.bDescType));
            if (Buf.VolDescHdr.bDescType == ISO9660VOLDESC_TYPE_PRIMARY)
            {
                cPrimaryVolDescs++;
                if (Buf.VolDescHdr.bDescVersion != ISO9660PRIMARYVOLDESC_VERSION)
                    return RTErrInfoSetF(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT,
                                         "Unsupported primary volume descriptor version: %#x", Buf.VolDescHdr.bDescVersion);
#ifdef LOG_ENABLED
                rtFsIsoVolLogPrimarySupplementaryVolDesc(&Buf.SupVolDesc);
#endif
                if (cPrimaryVolDescs > 1)
                    return RTErrInfoSet(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT, "More than one primary volume descriptor");
                rc = rtFsIsoVolHandlePrimaryVolDesc(pThis, &Buf.PrimaryVolDesc, offVolDesc, &RootDir, &offRootDirRec, pErrInfo);
            }
            else if (Buf.VolDescHdr.bDescType == ISO9660VOLDESC_TYPE_SUPPLEMENTARY)
            {
                cSupplementaryVolDescs++;
                if (Buf.VolDescHdr.bDescVersion != ISO9660SUPVOLDESC_VERSION)
                    return RTErrInfoSetF(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT,
                                         "Unsupported supplemental volume descriptor version: %#x", Buf.VolDescHdr.bDescVersion);
#ifdef LOG_ENABLED
                rtFsIsoVolLogPrimarySupplementaryVolDesc(&Buf.SupVolDesc);
#endif
                rc = rtFsIsoVolHandleSupplementaryVolDesc(pThis, &Buf.SupVolDesc, offVolDesc, &bJolietUcs2Level, &JolietRootDir,
                                                          &offJolietRootDirRec, pErrInfo);
            }
            else if (Buf.VolDescHdr.bDescType == ISO9660VOLDESC_TYPE_BOOT_RECORD)
            {
                cBootRecordVolDescs++;
            }
            else if (Buf.VolDescHdr.bDescType == ISO9660VOLDESC_TYPE_TERMINATOR)
            {
                if (!cPrimaryVolDescs)
                    return RTErrInfoSet(pErrInfo, VERR_VFS_BOGUS_FORMAT, "No primary volume descriptor");
                enmState = kStateNoSeq;
            }
            else
                return RTErrInfoSetF(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT,
                                     "Unknown volume descriptor: %#x", Buf.VolDescHdr.bDescType);
        }
        /*
         * UDF volume recognition sequence (VRS).
         */
        else if (   (   enmState == kStateNoSeq
                     || enmState == kStateStart)
                 && MATCH_HDR(&Buf.VolDescHdr, UDF_EXT_VOL_DESC_TYPE, UDF_EXT_VOL_DESC_STD_ID_BEGIN, UDF_EXT_VOL_DESC_VERSION) )
        {
            if (uUdfLevel == 0)
                enmState = kStateUdfSeq;
            else
                return RTErrInfoSetF(pErrInfo, VERR_VFS_BOGUS_FORMAT, "Only one BEA01 sequence is supported");
        }
        else if (   enmState == kStateUdfSeq
                 && MATCH_HDR(&Buf.VolDescHdr, UDF_EXT_VOL_DESC_TYPE, UDF_EXT_VOL_DESC_STD_ID_NSR_02, UDF_EXT_VOL_DESC_VERSION) )
            uUdfLevel = 2;
        else if (   enmState == kStateUdfSeq
                 && MATCH_HDR(&Buf.VolDescHdr, UDF_EXT_VOL_DESC_TYPE, UDF_EXT_VOL_DESC_STD_ID_NSR_03, UDF_EXT_VOL_DESC_VERSION) )
            uUdfLevel = 3;
        else if (   enmState == kStateUdfSeq
                 && MATCH_HDR(&Buf.VolDescHdr, UDF_EXT_VOL_DESC_TYPE, UDF_EXT_VOL_DESC_STD_ID_BOOT, UDF_EXT_VOL_DESC_VERSION) )
        {
            if (offUdfBootVolDesc == UINT64_MAX)
                offUdfBootVolDesc = iVolDesc * cbSector;
            else
                return RTErrInfoSetF(pErrInfo, VERR_VFS_BOGUS_FORMAT, "Only one BOOT2 descriptor is supported");
        }
        else if (   enmState == kStateUdfSeq
                 && MATCH_HDR(&Buf.VolDescHdr, UDF_EXT_VOL_DESC_TYPE, UDF_EXT_VOL_DESC_STD_ID_TERM, UDF_EXT_VOL_DESC_VERSION) )
        {
            if (uUdfLevel != 0)
                enmState = kStateNoSeq;
            else
                return RTErrInfoSetF(pErrInfo, VERR_VFS_BOGUS_FORMAT, "Found BEA01 & TEA01, but no NSR02 or NSR03 descriptors");
        }
        /*
         * Unknown, probably the end.
         */
        else if (enmState == kStateNoSeq)
            break;
        else if (enmState == kStateStart)
                return RTErrInfoSetF(pErrInfo, VERR_VFS_UNKNOWN_FORMAT,
                                     "Not ISO? Unable to recognize volume descriptor signature: %.5Rhxs", Buf.VolDescHdr.achStdId);
        else if (enmState == kStateCdSeq)
            return RTErrInfoSetF(pErrInfo, VERR_VFS_BOGUS_FORMAT,
                                 "Missing ISO 9660 terminator volume descriptor? (Found %.5Rhxs)", Buf.VolDescHdr.achStdId);
        else if (enmState == kStateUdfSeq)
            return RTErrInfoSetF(pErrInfo, VERR_VFS_BOGUS_FORMAT,
                                 "Missing UDF terminator volume descriptor? (Found %.5Rhxs)", Buf.VolDescHdr.achStdId);
        else
            return RTErrInfoSetF(pErrInfo, VERR_VFS_UNKNOWN_FORMAT,
                                 "Unknown volume descriptor signature found at sector %u: %.5Rhxs",
                                 16 + iVolDesc, Buf.VolDescHdr.achStdId);
        if (RT_FAILURE(rc))
            return rc;
    }

    /*
     * If we found a UDF VRS and are interested in UDF, we have more work to do here.
     */
    if (uUdfLevel > 0 && !(fFlags & RTFSISO9660_F_NO_UDF) && /* Just disable this code for now: */ (fFlags & RT_BIT(24)))
    {
        Log(("rtFsIsoVolTryInit: uUdfLevel=%d\n", uUdfLevel));
        rc = rtFsIsoVolHandleUdfDetection(pThis, &uUdfLevel, offUdfBootVolDesc, Buf.ab, sizeof(Buf), pErrInfo);
        if (RT_FAILURE(rc))
            return rc;
    }

    /*
     * We may be faced with choosing between joliet and rock ridge (we won't
     * have this choice when RTFSISO9660_F_NO_JOLIET is set).  The joliet
     * option is generally favorable as we don't have to guess wrt to the file
     * name encoding.  So, we'll pick that for now.
     *
     * Note! Should we change this preference for joliet, there fun wrt making sure
     *       there really is rock ridge stuff in the primary volume as well as
     *       making sure there really is anything of value in the primary volume.
     */
    if (bJolietUcs2Level != 0)
    {
        pThis->fIsUtf16 = true;
        return rtFsIsoDirShrd_New9660(pThis, NULL, &JolietRootDir, 1, offJolietRootDirRec, &pThis->pRootDir);
    }
    return rtFsIsoDirShrd_New9660(pThis, NULL, &RootDir, 1, offRootDirRec, &pThis->pRootDir);
}


/**
 * Opens an ISO 9660 file system volume.
 *
 * @returns IPRT status code.
 * @param   hVfsFileIn      The file or device backing the volume.
 * @param   fFlags          RTFSISO9660_F_XXX.
 * @param   phVfs           Where to return the virtual file system handle.
 * @param   pErrInfo        Where to return additional error information.
 */
RTDECL(int) RTFsIso9660VolOpen(RTVFSFILE hVfsFileIn, uint32_t fFlags, PRTVFS phVfs, PRTERRINFO pErrInfo)
{
    /*
     * Quick input validation.
     */
    AssertPtrReturn(phVfs, VERR_INVALID_POINTER);
    *phVfs = NIL_RTVFS;
    AssertReturn(!(fFlags & ~RTFSISO9660_F_VALID_MASK), VERR_INVALID_FLAGS);

    uint32_t cRefs = RTVfsFileRetain(hVfsFileIn);
    AssertReturn(cRefs != UINT32_MAX, VERR_INVALID_HANDLE);

    /*
     * Create a new FAT VFS instance and try initialize it using the given input file.
     */
    RTVFS hVfs   = NIL_RTVFS;
    void *pvThis = NULL;
    int rc = RTVfsNew(&g_rtFsIsoVolOps, sizeof(RTFSISOVOL), NIL_RTVFS, RTVFSLOCK_CREATE_RW, &hVfs, &pvThis);
    if (RT_SUCCESS(rc))
    {
        rc = rtFsIsoVolTryInit((PRTFSISOVOL)pvThis, hVfs, hVfsFileIn, fFlags, pErrInfo);
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
static DECLCALLBACK(int) rtVfsChainIso9660Vol_Validate(PCRTVFSCHAINELEMENTREG pProviderReg, PRTVFSCHAINSPEC pSpec,
                                                       PRTVFSCHAINELEMSPEC pElement, uint32_t *poffError, PRTERRINFO pErrInfo)
{
    RT_NOREF(pProviderReg, pSpec);

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
    uint32_t fFlags = 0;
    if (pElement->cArgs > 0)
    {
        for (uint32_t iArg = 0; iArg < pElement->cArgs; iArg++)
        {
            const char *psz = pElement->paArgs[iArg].psz;
            if (*psz)
            {
                if (!strcmp(psz, "nojoliet"))
                    fFlags |= RTFSISO9660_F_NO_JOLIET;
                else if (!strcmp(psz, "norock"))
                    fFlags |= RTFSISO9660_F_NO_ROCK;
                else if (!strcmp(psz, "noudf"))
                    fFlags |= RTFSISO9660_F_NO_UDF;
                else
                {
                    *poffError = pElement->paArgs[iArg].offSpec;
                    return RTErrInfoSet(pErrInfo, VERR_VFS_CHAIN_INVALID_ARGUMENT, "Only knows: 'nojoliet' and 'norock'");
                }
            }
        }
    }

    pElement->uProvider = fFlags;
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSCHAINELEMENTREG,pfnInstantiate}
 */
static DECLCALLBACK(int) rtVfsChainIso9660Vol_Instantiate(PCRTVFSCHAINELEMENTREG pProviderReg, PCRTVFSCHAINSPEC pSpec,
                                                          PCRTVFSCHAINELEMSPEC pElement, RTVFSOBJ hPrevVfsObj,
                                                          PRTVFSOBJ phVfsObj, uint32_t *poffError, PRTERRINFO pErrInfo)
{
    RT_NOREF(pProviderReg, pSpec, poffError);

    int         rc;
    RTVFSFILE   hVfsFileIn = RTVfsObjToFile(hPrevVfsObj);
    if (hVfsFileIn != NIL_RTVFSFILE)
    {
        RTVFS hVfs;
        rc = RTFsIso9660VolOpen(hVfsFileIn, pElement->uProvider, &hVfs, pErrInfo);
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
static DECLCALLBACK(bool) rtVfsChainIso9660Vol_CanReuseElement(PCRTVFSCHAINELEMENTREG pProviderReg,
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
static RTVFSCHAINELEMENTREG g_rtVfsChainIso9660VolReg =
{
    /* uVersion = */            RTVFSCHAINELEMENTREG_VERSION,
    /* fReserved = */           0,
    /* pszName = */             "isofs",
    /* ListEntry = */           { NULL, NULL },
    /* pszHelp = */             "Open a ISO 9660 file system, requires a file object on the left side.\n"
                                "The 'noudf' option make it ignore any UDF.\n"
                                "The 'nojoliet' option make it ignore any joliet supplemental volume.\n"
                                "The 'norock' option make it ignore any rock ridge info.\n",
    /* pfnValidate = */         rtVfsChainIso9660Vol_Validate,
    /* pfnInstantiate = */      rtVfsChainIso9660Vol_Instantiate,
    /* pfnCanReuseElement = */  rtVfsChainIso9660Vol_CanReuseElement,
    /* uEndMarker = */          RTVFSCHAINELEMENTREG_VERSION
};

RTVFSCHAIN_AUTO_REGISTER_ELEMENT_PROVIDER(&g_rtVfsChainIso9660VolReg, rtVfsChainIso9660VolReg);

