/* $Id$ */
/** @file
 * IPRT - ISO 9660 Virtual Filesystem.
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
#include <iprt/file.h>
#include <iprt/log.h>
#include <iprt/mem.h>
#include <iprt/poll.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/vfs.h>
#include <iprt/vfslowlevel.h>
#include <iprt/formats/iso9660.h>



/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/** Pointer to an ISO 9660 volume (VFS instance data). */
typedef struct RTFSISO9660VOL *PRTFSISO9660VOL;
/** Pointer to a const ISO 9660 volume (VFS instance data). */
typedef struct RTFSISO9660VOL const *PCRTFSISO9660VOL;

/** Pointer to a ISO 9660 directory instance. */
typedef struct RTFSISO9660DIR *PRTFSISO9660DIR;



/**
 * ISO 9660 extent (internal to the VFS not a disk structure).
 */
typedef struct RTFSISO9660EXTENT
{
    /** The disk offset. */
    uint64_t            offDisk;
    /** The size of the extent in bytes. */
    uint64_t            cbExtent;
} RTFSISO9660EXTENT;
/** Pointer to an ISO 9660 extent. */
typedef RTFSISO9660EXTENT *PRTFSISO9660EXTENT;
/** Pointer to a const ISO 9660 extent. */
typedef RTFSISO9660EXTENT const *PCRTFSISO9660EXTENT;


/**
 * ISO 9660 file system object (common part to files and dirs).
 */
typedef struct RTFSISO9660OBJ
{
    /** The parent directory keeps a list of open objects (RTFSISO9660OBJ). */
    RTLISTNODE          Entry;
    /** The parent directory (not released till all children are close). */
    PRTFSISO9660DIR     pParentDir;
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
    PRTFSISO9660VOL     pVol;
    /** Number of extents. */
    uint32_t            cExtents;
    /** The first extent. */
    RTFSISO9660EXTENT   FirstExtent;
    /** Array of additional extents. */
    PRTFSISO9660EXTENT  paExtents;
} RTFSISO9660OBJ;
typedef RTFSISO9660OBJ *PRTFSISO9660OBJ;

/**
 * ISO 9660 file.
 */
typedef struct RTFSISO9660FILE
{
    /** Core ISO9660 object info.  */
    RTFSISO9660OBJ      Core;
    /** The current file offset. */
    uint64_t            offFile;
} RTFSISO9660FILE;
/** Pointer to a ISO 9660 file object. */
typedef RTFSISO9660FILE *PRTFSISO9660FILE;


/**
 * ISO 9660 directory.
 *
 * We will always read in the whole directory just to keep things really simple.
 */
typedef struct RTFSISO9660DIR
{
    /** Core ISO 9660 object info.  */
    RTFSISO9660OBJ      Core;
    /** The VFS handle for this directory (for reference counting). */
    RTVFSDIR            hVfsSelf;
    /** Open child objects (RTFSISO9660OBJ). */
    RTLISTNODE          OpenChildren;

    /** Pointer to the directory content. */
    uint8_t            *pbDir;
    /** The size of the directory content (duplicate of Core.cbObject). */
    uint32_t            cbDir;
} RTFSISO9660DIR;
/** Pointer to a ISO 9660 directory instance. */
typedef RTFSISO9660DIR *PRTFSISO9660DIR;



/**
 * A ISO 9660 volume.
 */
typedef struct RTFSISO9660VOL
{
    /** Handle to itself. */
    RTVFS           hVfsSelf;
    /** The file, partition, or whatever backing the ISO 9660 volume. */
    RTVFSFILE       hVfsBacking;
    /** The size of the backing thingy. */
    uint64_t        cbBacking;
    /** Flags. */
    uint32_t        fFlags;
    /** The sector size (in bytes). */
    uint32_t        cbSector;
    /** The size of a logical block in bytes. */
    uint32_t        cbBlock;
    /** The primary volume space size in blocks. */
    uint32_t        cBlocksInPrimaryVolumeSpace;
    /** The primary volume space size in bytes. */
    uint64_t        cbPrimaryVolumeSpace;
    /** The number of volumes in the set. */
    uint32_t        cVolumesInSet;
    /** The primary volume sequence ID. */
    uint32_t        idPrimaryVol;
    /** Set if using UTF16-2 (joliet). */
    bool            fIsUtf16;

    /** The root directory handle. */
    RTVFSDIR        hVfsRootDir;
    /** The root directory instance data. */
    PRTFSISO9660DIR pRootDir;
} RTFSISO9660VOL;



/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static void rtFsIso9660Dir_AddOpenChild(PRTFSISO9660DIR pDir, PRTFSISO9660OBJ pChild);
static void rtFsIso9660Dir_RemoveOpenChild(PRTFSISO9660DIR pDir, PRTFSISO9660OBJ pChild);
static int  rtFsIso9660Dir_New(PRTFSISO9660VOL pThis, PRTFSISO9660DIR pParentDir, PCISO9660DIRREC pDirRec, uint32_t cDirRecs,
                               uint64_t offDirRec, PRTVFSDIR phVfsDir, PRTFSISO9660DIR *ppDir);



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
 * Initialization of a RTFSISO9660OBJ structure from a directory record.
 *
 * @note    The RTFSISO9660OBJ::pParentDir and RTFSISO9660OBJ::Clusters members are
 *          properly initialized elsewhere.
 *
 * @param   pObj            The structure to initialize.
 * @param   pDirRec         The primary directory record.
 * @param   cDirRecs        Number of directory records.
 * @param   offDirRec       The offset of the primary directory record.
 * @param   pVol            The volume.
 */
static void rtFsIso9660Obj_InitFromDirRec(PRTFSISO9660OBJ pObj, PCISO9660DIRREC pDirRec, uint32_t cDirRecs,
                                          uint64_t offDirRec, PRTFSISO9660VOL pVol)
{
    Assert(cDirRecs == 1); RT_NOREF(cDirRecs);

    RTListInit(&pObj->Entry);
    pObj->pParentDir           = NULL;
    pObj->pVol                 = pVol;
    pObj->offDirRec            = offDirRec;
    pObj->fAttrib              = pDirRec->fFileFlags & ISO9660_FILE_FLAGS_DIRECTORY
                               ? RTFS_DOS_DIRECTORY | RTFS_TYPE_DIRECTORY : RTFS_TYPE_FILE;
    if (pDirRec->fFileFlags & ISO9660_FILE_FLAGS_HIDDEN)
        pObj->fAttrib |= RTFS_DOS_HIDDEN;
    pObj->cbObject             = ISO9660_GET_ENDIAN(&pDirRec->cbData);
    pObj->cExtents             = 1;
    pObj->FirstExtent.cbExtent = pObj->cbObject;
    pObj->FirstExtent.offDisk  = ISO9660_GET_ENDIAN(&pDirRec->offExtent) * (uint64_t)pVol->cbBlock;

    rtFsIso9660DateTime2TimeSpec(&pObj->ModificationTime, &pDirRec->RecTime);
    pObj->BirthTime  = pObj->ModificationTime;
    pObj->AccessTime = pObj->ModificationTime;
    pObj->ChangeTime = pObj->ModificationTime;
}



/**
 * Worker for rtFsIso9660File_Close and rtFsIso9660Dir_Close that does common work.
 *
 * @returns IPRT status code.
 * @param   pObj            The common object structure.
 */
static int rtFsIso9660Obj_Close(PRTFSISO9660OBJ pObj)
{
    if (pObj->pParentDir)
        rtFsIso9660Dir_RemoveOpenChild(pObj->pParentDir, pObj);
    if (pObj->paExtents)
    {
        RTMemFree(pObj->paExtents);
        pObj->paExtents = NULL;
    }
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnClose}
 */
static DECLCALLBACK(int) rtFsIso9660File_Close(void *pvThis)
{
    PRTFSISO9660FILE pThis = (PRTFSISO9660FILE)pvThis;
    LogFlow(("rtFsIso9660File_Close(%p)\n", pThis));
    return rtFsIso9660Obj_Close(&pThis->Core);
}


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnQueryInfo}
 */
static DECLCALLBACK(int) rtFsIso9660Obj_QueryInfo(void *pvThis, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    PRTFSISO9660OBJ pThis = (PRTFSISO9660OBJ)pvThis;

    pObjInfo->cbObject              = pThis->cbObject;
    pObjInfo->cbAllocated           = RT_ALIGN_64(pThis->cbObject, pThis->pVol->cbBlock);
    pObjInfo->AccessTime            = pThis->AccessTime;
    pObjInfo->ModificationTime      = pThis->ModificationTime;
    pObjInfo->ChangeTime            = pThis->ChangeTime;
    pObjInfo->BirthTime             = pThis->BirthTime;
    pObjInfo->Attr.fMode            = pThis->fAttrib;
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
static DECLCALLBACK(int) rtFsIso9660File_Read(void *pvThis, RTFOFF off, PCRTSGBUF pSgBuf, bool fBlocking, size_t *pcbRead)
{
    PRTFSISO9660FILE pThis = (PRTFSISO9660FILE)pvThis;
    AssertReturn(pSgBuf->cSegs != 0, VERR_INTERNAL_ERROR_3);
    RT_NOREF(fBlocking);

    /*
     * Check for EOF.
     */
    if (off == -1)
        off = pThis->offFile;
    if ((uint64_t)off >= pThis->Core.cbObject)
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
    uint64_t cbFileLeft = pThis->Core.cbObject - (uint64_t)off;
    size_t   cbLeft     = pSgBuf->paSegs[0].cbSeg;
    uint8_t *pbDst      = (uint8_t *)pSgBuf->paSegs[0].pvSeg;
    if (pThis->Core.cExtents == 1)
    {
        if (cbLeft > 0)
        {
            size_t cbToRead = cbLeft;
            if (cbToRead > cbFileLeft)
                cbToRead = (size_t)cbFileLeft;
            rc = RTVfsFileReadAt(pThis->Core.pVol->hVfsBacking, pThis->Core.FirstExtent.offDisk + off, pbDst, cbToRead, NULL);
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
static DECLCALLBACK(int) rtFsIso9660File_Write(void *pvThis, RTFOFF off, PCRTSGBUF pSgBuf, bool fBlocking, size_t *pcbWritten)
{
    RT_NOREF(pvThis, off, pSgBuf, fBlocking, pcbWritten);
    return VERR_WRITE_PROTECT;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnFlush}
 */
static DECLCALLBACK(int) rtFsIso9660File_Flush(void *pvThis)
{
    RT_NOREF(pvThis);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnPollOne}
 */
static DECLCALLBACK(int) rtFsIso9660File_PollOne(void *pvThis, uint32_t fEvents, RTMSINTERVAL cMillies, bool fIntr,
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
static DECLCALLBACK(int) rtFsIso9660File_Tell(void *pvThis, PRTFOFF poffActual)
{
    PRTFSISO9660FILE pThis = (PRTFSISO9660FILE)pvThis;
    *poffActual = pThis->offFile;
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnMode}
 */
static DECLCALLBACK(int) rtFsIso9660Obj_SetMode(void *pvThis, RTFMODE fMode, RTFMODE fMask)
{
    RT_NOREF(pvThis, fMode, fMask);
    return VERR_WRITE_PROTECT;
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnSetTimes}
 */
static DECLCALLBACK(int) rtFsIso9660Obj_SetTimes(void *pvThis, PCRTTIMESPEC pAccessTime, PCRTTIMESPEC pModificationTime,
                                                 PCRTTIMESPEC pChangeTime, PCRTTIMESPEC pBirthTime)
{
    RT_NOREF(pvThis, pAccessTime, pModificationTime, pChangeTime, pBirthTime);
    return VERR_WRITE_PROTECT;
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnSetOwner}
 */
static DECLCALLBACK(int) rtFsIso9660Obj_SetOwner(void *pvThis, RTUID uid, RTGID gid)
{
    RT_NOREF(pvThis, uid, gid);
    return VERR_WRITE_PROTECT;
}


/**
 * @interface_method_impl{RTVFSFILEOPS,pfnSeek}
 */
static DECLCALLBACK(int) rtFsIso9660File_Seek(void *pvThis, RTFOFF offSeek, unsigned uMethod, PRTFOFF poffActual)
{
    PRTFSISO9660FILE pThis = (PRTFSISO9660FILE)pvThis;
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
static DECLCALLBACK(int) rtFsIso9660File_QuerySize(void *pvThis, uint64_t *pcbFile)
{
    PRTFSISO9660FILE pThis = (PRTFSISO9660FILE)pvThis;
    *pcbFile = pThis->Core.cbObject;
    return VINF_SUCCESS;
}


/**
 * FAT file operations.
 */
DECL_HIDDEN_CONST(const RTVFSFILEOPS) g_rtFsIso9660FileOps =
{
    { /* Stream */
        { /* Obj */
            RTVFSOBJOPS_VERSION,
            RTVFSOBJTYPE_FILE,
            "FatFile",
            rtFsIso9660File_Close,
            rtFsIso9660Obj_QueryInfo,
            RTVFSOBJOPS_VERSION
        },
        RTVFSIOSTREAMOPS_VERSION,
        RTVFSIOSTREAMOPS_FEAT_NO_SG,
        rtFsIso9660File_Read,
        rtFsIso9660File_Write,
        rtFsIso9660File_Flush,
        rtFsIso9660File_PollOne,
        rtFsIso9660File_Tell,
        NULL /*pfnSkip*/,
        NULL /*pfnZeroFill*/,
        RTVFSIOSTREAMOPS_VERSION,
    },
    RTVFSFILEOPS_VERSION,
    0,
    { /* ObjSet */
        RTVFSOBJSETOPS_VERSION,
        RT_OFFSETOF(RTVFSFILEOPS, Stream.Obj) - RT_OFFSETOF(RTVFSFILEOPS, ObjSet),
        rtFsIso9660Obj_SetMode,
        rtFsIso9660Obj_SetTimes,
        rtFsIso9660Obj_SetOwner,
        RTVFSOBJSETOPS_VERSION
    },
    rtFsIso9660File_Seek,
    rtFsIso9660File_QuerySize,
    RTVFSFILEOPS_VERSION
};


/**
 * Instantiates a new directory.
 *
 * @returns IPRT status code.
 * @param   pThis           The FAT volume instance.
 * @param   pParentDir      The parent directory.
 * @param   pDirRec         The directory record.
 * @param   cDirRecs        Number of directory records if more than one.
 * @param   offDirRec       The byte offset of the directory record.
 * @param   offEntryInDir   The byte offset of the directory entry in the parent
 *                          directory.
 * @param   fOpen           RTFILE_O_XXX flags.
 * @param   phVfsFile       Where to return the file handle.
 */
static int rtFsIso9660File_New(PRTFSISO9660VOL pThis, PRTFSISO9660DIR pParentDir, PCISO9660DIRREC pDirRec, uint32_t cDirRecs,
                                uint64_t offDirRec, uint64_t fOpen, PRTVFSFILE phVfsFile)
{
    AssertPtr(pParentDir);

    PRTFSISO9660FILE pNewFile;
    int rc = RTVfsNewFile(&g_rtFsIso9660FileOps, sizeof(*pNewFile), fOpen, pThis->hVfsSelf, NIL_RTVFSLOCK /*use volume lock*/,
                          phVfsFile, (void **)&pNewFile);
    if (RT_SUCCESS(rc))
    {
        /*
         * Initialize it all so rtFsIso9660File_Close doesn't trip up in anyway.
         */
        rtFsIso9660Obj_InitFromDirRec(&pNewFile->Core, pDirRec, cDirRecs, offDirRec, pThis);
        pNewFile->offFile = 0;

        /*
         * Link into parent directory so we can use it to update
         * our directory entry.
         */
        rtFsIso9660Dir_AddOpenChild(pParentDir, &pNewFile->Core);

        return VINF_SUCCESS;

    }
    *phVfsFile = NIL_RTVFSFILE;
    return rc;
}


/**
 * RTStrToUtf16Ex returning big-endian UTF-16.
 */
static int rtFsIso9660_StrToUtf16BigEndian(const char *pszString, size_t cchString, PRTUTF16 *ppwsz, size_t cwc, size_t *pcwc)
{
    int rc = RTStrToUtf16Ex(pszString, cchString, ppwsz, cwc, pcwc);
#ifndef RT_BIG_ENDIAN
    if (RT_SUCCESS(rc))
    {
        PRTUTF16 pwc = *ppwsz;
        RTUTF16  wc;
        while ((wc = *pwc))
            *pwc++ = RT_H2BE_U16(wc);
    }
#endif
    return rc;
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
 */
static int rtFsIso9660Dir_FindEntry(PRTFSISO9660DIR pThis, const char *pszEntry, uint64_t *poffDirRec, PCISO9660DIRREC *ppDirRec,
                                    uint32_t *pcDirRecs, PRTFMODE pfMode)
{
    /* Set return values. */
    *poffDirRec = UINT64_MAX;
    *ppDirRec   = NULL;
    *pcDirRecs  = 1;
    *pfMode     = UINT32_MAX;

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
        rc = rtFsIso9660_StrToUtf16BigEndian(pszEntry, RTSTR_MAX, &pwszEntry, RT_ELEMENTS(uBuf.wszEntry), &cwcEntry);
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
    while (offEntryInDir + RT_OFFSETOF(ISO9660DIRREC, achFileId) <= cbDir)
    {
        PCISO9660DIRREC pDirRec = (PCISO9660DIRREC)&pThis->pbDir[offEntryInDir];

        /* If null length, skip to the next sector. */
        if (pDirRec->cbDirRec == 0)
            offEntryInDir = (offEntryInDir + pThis->Core.pVol->cbSector) & ~(pThis->Core.pVol->cbSector - 1U);
        else
        {
            /* Try match the filename. */
            /** @todo deal with multi-extend filenames, or however that works... */
            if (fIsUtf16)
            {
                if (   ((size_t)pDirRec->bFileIdLength - cbEntry) > (size_t)12 /* ;12345 */
                    || RTUtf16BigNICmp((PCRTUTF16)pDirRec->achFileId, uBuf.wszEntry, cwcEntry) != 0
                    || (   (size_t)pDirRec->bFileIdLength != cbEntry
                        && RT_MAKE_U16(pDirRec->achFileId[cbEntry + 1], pDirRec->achFileId[cbEntry]) != ';') )
                {
                    /* Advance */
                    offEntryInDir += pDirRec->cbDirRec;
                    continue;
                }
            }
            else
            {
                if (   ((size_t)pDirRec->bFileIdLength - cchUpper) > (size_t)6 /* ;12345 */
                     || memcmp(pDirRec->achFileId, uBuf.s.szUpper, cchUpper) != 0
                     || (   (size_t)pDirRec->bFileIdLength != cchUpper
                         && pDirRec->achFileId[cchUpper] != ';') )
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
            *pcDirRecs  = 1;
            *pfMode     = pDirRec->fFileFlags & ISO9660_FILE_FLAGS_DIRECTORY ? RTFS_TYPE_DIRECTORY : RTFS_TYPE_FILE;
            return VINF_SUCCESS;

        }
    }

    return VERR_FILE_NOT_FOUND;
}


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnClose}
 */
static DECLCALLBACK(int) rtFsIso9660Dir_Close(void *pvThis)
{
    PRTFSISO9660DIR pThis = (PRTFSISO9660DIR)pvThis;
    LogFlow(("rtFsIso9660Dir_Close(%p)\n", pThis));
    if (pThis->pbDir)
    {
        RTMemFree(pThis->pbDir);
        pThis->pbDir = NULL;
    }
    return rtFsIso9660Obj_Close(&pThis->Core);
}


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnTraversalOpen}
 */
static DECLCALLBACK(int) rtFsIso9660Dir_TraversalOpen(void *pvThis, const char *pszEntry, PRTVFSDIR phVfsDir,
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

        PRTFSISO9660DIR pThis = (PRTFSISO9660DIR)pvThis;
        PCISO9660DIRREC pDirRec;
        uint64_t        offDirRec;
        uint32_t        cDirRecs;
        RTFMODE         fMode;
        rc = rtFsIso9660Dir_FindEntry(pThis, pszEntry, &offDirRec, &pDirRec, &cDirRecs, &fMode);
        Log2(("rtFsIso9660Dir_TraversalOpen: FindEntry(,%s,) -> %Rrc\n", pszEntry, rc));
        if (RT_SUCCESS(rc))
        {
            switch (fMode & RTFS_TYPE_MASK)
            {
                case RTFS_TYPE_DIRECTORY:
                    rc = rtFsIso9660Dir_New(pThis->Core.pVol, pThis, pDirRec, cDirRecs, offDirRec, phVfsDir, NULL /*ppDir*/);
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
static DECLCALLBACK(int) rtFsIso9660Dir_OpenFile(void *pvThis, const char *pszFilename, uint32_t fOpen, PRTVFSFILE phVfsFile)
{
    PRTFSISO9660DIR pThis = (PRTFSISO9660DIR)pvThis;

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
    int rc = rtFsIso9660Dir_FindEntry(pThis, pszFilename, &offDirRec, &pDirRec, &cDirRecs, &fMode);
    Log2(("rtFsIso9660Dir_OpenFile: FindEntry(,%s,) -> %Rrc\n", pszFilename, rc));
    if (RT_SUCCESS(rc))
    {
        switch (fMode & RTFS_TYPE_MASK)
        {
            case RTFS_TYPE_FILE:
                rc = rtFsIso9660File_New(pThis->Core.pVol, pThis, pDirRec, cDirRecs, offDirRec, fOpen, phVfsFile);
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
static DECLCALLBACK(int) rtFsIso9660Dir_OpenDir(void *pvThis, const char *pszSubDir, uint32_t fFlags, PRTVFSDIR phVfsDir)
{
    RT_NOREF(pvThis, pszSubDir, fFlags, phVfsDir);
RTAssertMsg2("%s: %s\n", __FUNCTION__, pszSubDir);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnCreateDir}
 */
static DECLCALLBACK(int) rtFsIso9660Dir_CreateDir(void *pvThis, const char *pszSubDir, RTFMODE fMode, PRTVFSDIR phVfsDir)
{
    RT_NOREF(pvThis, pszSubDir, fMode, phVfsDir);
    return VERR_WRITE_PROTECT;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnOpenSymlink}
 */
static DECLCALLBACK(int) rtFsIso9660Dir_OpenSymlink(void *pvThis, const char *pszSymlink, PRTVFSSYMLINK phVfsSymlink)
{
    RT_NOREF(pvThis, pszSymlink, phVfsSymlink);
RTAssertMsg2("%s: %s\n", __FUNCTION__, pszSymlink);
    return VERR_NOT_SUPPORTED;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnCreateSymlink}
 */
static DECLCALLBACK(int) rtFsIso9660Dir_CreateSymlink(void *pvThis, const char *pszSymlink, const char *pszTarget,
                                                  RTSYMLINKTYPE enmType, PRTVFSSYMLINK phVfsSymlink)
{
    RT_NOREF(pvThis, pszSymlink, pszTarget, enmType, phVfsSymlink);
    return VERR_WRITE_PROTECT;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnUnlinkEntry}
 */
static DECLCALLBACK(int) rtFsIso9660Dir_UnlinkEntry(void *pvThis, const char *pszEntry, RTFMODE fType)
{
    RT_NOREF(pvThis, pszEntry, fType);
    return VERR_WRITE_PROTECT;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnRewindDir}
 */
static DECLCALLBACK(int) rtFsIso9660Dir_RewindDir(void *pvThis)
{
    RT_NOREF(pvThis);
RTAssertMsg2("%s\n", __FUNCTION__);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnReadDir}
 */
static DECLCALLBACK(int) rtFsIso9660Dir_ReadDir(void *pvThis, PRTDIRENTRYEX pDirEntry, size_t *pcbDirEntry,
                                                RTFSOBJATTRADD enmAddAttr)
{
    RT_NOREF(pvThis, pDirEntry, pcbDirEntry, enmAddAttr);
RTAssertMsg2("%s\n", __FUNCTION__);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * FAT file operations.
 */
static const RTVFSDIROPS g_rtFsIso9660DirOps =
{
    { /* Obj */
        RTVFSOBJOPS_VERSION,
        RTVFSOBJTYPE_DIR,
        "ISO 9660 Dir",
        rtFsIso9660Dir_Close,
        rtFsIso9660Obj_QueryInfo,
        RTVFSOBJOPS_VERSION
    },
    RTVFSDIROPS_VERSION,
    0,
    { /* ObjSet */
        RTVFSOBJSETOPS_VERSION,
        RT_OFFSETOF(RTVFSDIROPS, Obj) - RT_OFFSETOF(RTVFSDIROPS, ObjSet),
        rtFsIso9660Obj_SetMode,
        rtFsIso9660Obj_SetTimes,
        rtFsIso9660Obj_SetOwner,
        RTVFSOBJSETOPS_VERSION
    },
    rtFsIso9660Dir_TraversalOpen,
    rtFsIso9660Dir_OpenFile,
    rtFsIso9660Dir_OpenDir,
    rtFsIso9660Dir_CreateDir,
    rtFsIso9660Dir_OpenSymlink,
    rtFsIso9660Dir_CreateSymlink,
    rtFsIso9660Dir_UnlinkEntry,
    rtFsIso9660Dir_RewindDir,
    rtFsIso9660Dir_ReadDir,
    RTVFSDIROPS_VERSION,
};


/**
 * Adds an open child to the parent directory.
 *
 * Maintains an additional reference to the parent dir to prevent it from going
 * away.  If @a pDir is the root directory, it also ensures the volume is
 * referenced and sticks around until the last open object is gone.
 *
 * @param   pDir        The directory.
 * @param   pChild      The child being opened.
 * @sa      rtFsIso9660Dir_RemoveOpenChild
 */
static void rtFsIso9660Dir_AddOpenChild(PRTFSISO9660DIR pDir, PRTFSISO9660OBJ pChild)
{
    /* First child that gets opened retains the parent directory.  This is
       released by the final open child. */
    if (RTListIsEmpty(&pDir->OpenChildren))
    {
        uint32_t cRefs = RTVfsDirRetain(pDir->hVfsSelf);
        Assert(cRefs != UINT32_MAX); NOREF(cRefs);

        /* Root also retains the whole file system. */
        if (pDir->Core.pVol->pRootDir == pDir)
        {
            Assert(pDir->Core.pVol);
            Assert(pDir->Core.pVol == pChild->pVol);
            cRefs = RTVfsRetain(pDir->Core.pVol->hVfsSelf);
            Assert(cRefs != UINT32_MAX); NOREF(cRefs);
            LogFlow(("rtFsIso9660Dir_AddOpenChild(%p,%p) retains volume (%d)\n", pDir, pChild, cRefs));
        }
        else
            LogFlow(("rtFsIso9660Dir_AddOpenChild(%p,%p) retains parent only (%d)\n", pDir, pChild, cRefs));
    }
    else
        LogFlow(("rtFsIso9660Dir_AddOpenChild(%p,%p) retains nothing\n", pDir, pChild));
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
 * @sa      rtFsIso9660Dir_AddOpenChild
 */
static void rtFsIso9660Dir_RemoveOpenChild(PRTFSISO9660DIR pDir, PRTFSISO9660OBJ pChild)
{
    AssertReturnVoid(pChild->pParentDir == pDir);
    RTListNodeRemove(&pChild->Entry);
    pChild->pParentDir = NULL;

    /* Final child? If so, release directory. */
    if (RTListIsEmpty(&pDir->OpenChildren))
    {
        bool const fIsRootDir = pDir->Core.pVol->pRootDir == pDir;

        uint32_t cRefs = RTVfsDirRelease(pDir->hVfsSelf);
        Assert(cRefs != UINT32_MAX); NOREF(cRefs);

        /* Root directory releases the file system as well.  Since the volume
           holds a reference to the root directory, it will remain valid after
           the above release. */
        if (fIsRootDir)
        {
            Assert(cRefs > 0);
            Assert(pDir->Core.pVol);
            Assert(pDir->Core.pVol == pChild->pVol);
            cRefs = RTVfsRelease(pDir->Core.pVol->hVfsSelf);
            Assert(cRefs != UINT32_MAX); NOREF(cRefs);
            LogFlow(("rtFsIso9660Dir_RemoveOpenChild(%p,%p) releases volume (%d)\n", pDir, pChild, cRefs));
        }
        else
            LogFlow(("rtFsIso9660Dir_RemoveOpenChild(%p,%p) releases parent only (%d)\n", pDir, pChild, cRefs));
    }
    else
        LogFlow(("rtFsIso9660Dir_RemoveOpenChild(%p,%p) releases nothing\n", pDir, pChild));
}


#ifdef LOG_ENABLED
/**
 * Logs the content of a directory.
 */
static void rtFsIso9660Dir_LogContent(PRTFSISO9660DIR pThis)
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
                  pDirRec->cbExtAttr,
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
                Log2(("ISO9660:       system use:\n%.*Rhxd\n", pDirRec->cbDirRec - offSysUse, (uint8_t *)pDirRec + offSysUse));
            }

            /* advance */
            offRec += pDirRec->cbDirRec;
        }
    }
}
#endif /* LOG_ENABLED */


/**
 * Instantiates a new directory.
 *
 * @returns IPRT status code.
 * @param   pThis           The FAT volume instance.
 * @param   pParentDir      The parent directory.  This is NULL for the root
 *                          directory.
 * @param   pDirRec         The directory record.
 * @param   cDirRecs        Number of directory records if more than one.
 * @param   offDirRec       The byte offset of the directory record.
 * @param   phVfsDir        Where to return the directory handle.
 * @param   ppDir           Where to return the FAT directory instance data.
 */
static int  rtFsIso9660Dir_New(PRTFSISO9660VOL pThis, PRTFSISO9660DIR pParentDir, PCISO9660DIRREC pDirRec, uint32_t cDirRecs,
                               uint64_t offDirRec, PRTVFSDIR phVfsDir, PRTFSISO9660DIR *ppDir)
{
    if (ppDir)
        *ppDir = NULL;

    PRTFSISO9660DIR pNewDir;
    int rc = RTVfsNewDir(&g_rtFsIso9660DirOps, sizeof(*pNewDir), pParentDir ? 0 : RTVFSDIR_F_NO_VFS_REF,
                         pThis->hVfsSelf, NIL_RTVFSLOCK /*use volume lock*/, phVfsDir, (void **)&pNewDir);
    if (RT_SUCCESS(rc))
    {
        /*
         * Initialize it all so rtFsIso9660Dir_Close doesn't trip up in anyway.
         */
        rtFsIso9660Obj_InitFromDirRec(&pNewDir->Core, pDirRec, cDirRecs, offDirRec, pThis);
        RTListInit(&pNewDir->OpenChildren);
        pNewDir->hVfsSelf           = *phVfsDir;
        pNewDir->cbDir              = ISO9660_GET_ENDIAN(&pDirRec->cbData);
        pNewDir->pbDir              = (uint8_t *)RTMemAllocZ(pNewDir->cbDir + 256);
        if (pNewDir->pbDir)
        {
            rc = RTVfsFileReadAt(pThis->hVfsBacking, pNewDir->Core.FirstExtent.offDisk, pNewDir->pbDir, pNewDir->cbDir, NULL);
            if (RT_SUCCESS(rc))
            {
#ifdef LOG_ENABLED
                rtFsIso9660Dir_LogContent(pNewDir);
#endif

                /*
                 * Link into parent directory so we can use it to update
                 * our directory entry.
                 */
                if (pParentDir)
                    rtFsIso9660Dir_AddOpenChild(pParentDir, &pNewDir->Core);
                if (ppDir)
                    *ppDir = pNewDir;
                return VINF_SUCCESS;
            }
        }
        else
            rc = VERR_NO_MEMORY;
        RTVfsDirRelease(*phVfsDir);
    }
    *phVfsDir = NIL_RTVFSDIR;
    if (ppDir)
        *ppDir = NULL;
    return rc;
}




/**
 * @interface_method_impl{RTVFSOBJOPS::Obj,pfnClose}
 */
static DECLCALLBACK(int) rtFsIso9660Vol_Close(void *pvThis)
{
    PRTFSISO9660VOL pThis = (PRTFSISO9660VOL)pvThis;
    Log(("rtFsIso9660Vol_Close(%p)\n", pThis));

    if (pThis->hVfsRootDir != NIL_RTVFSDIR)
    {
        Assert(RTListIsEmpty(&pThis->pRootDir->OpenChildren));
        uint32_t cRefs = RTVfsDirRelease(pThis->hVfsRootDir);
        Assert(cRefs == 0); NOREF(cRefs);
        pThis->hVfsRootDir = NIL_RTVFSDIR;
        pThis->pRootDir    = NULL;
    }

    RTVfsFileRelease(pThis->hVfsBacking);
    pThis->hVfsBacking = NIL_RTVFSFILE;

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSOBJOPS::Obj,pfnQueryInfo}
 */
static DECLCALLBACK(int) rtFsIso9660Vol_QueryInfo(void *pvThis, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    RT_NOREF(pvThis, pObjInfo, enmAddAttr);
    return VERR_WRONG_TYPE;
}


/**
 * @interface_method_impl{RTVFSOPS,pfnOpenRoo}
 */
static DECLCALLBACK(int) rtFsIso9660Vol_OpenRoot(void *pvThis, PRTVFSDIR phVfsDir)
{
    PRTFSISO9660VOL pThis = (PRTFSISO9660VOL)pvThis;
    uint32_t cRefs = RTVfsDirRetain(pThis->hVfsRootDir);
    if (cRefs != UINT32_MAX)
    {
        *phVfsDir = pThis->hVfsRootDir;
        LogFlow(("rtFsIso9660Vol_OpenRoot -> %p\n", *phVfsDir));
        return VINF_SUCCESS;
    }
    return VERR_INTERNAL_ERROR_5;
}


/**
 * @interface_method_impl{RTVFSOPS,pfnIsRangeInUse}
 */
static DECLCALLBACK(int) rtFsIso9660Vol_IsRangeInUse(void *pvThis, RTFOFF off, size_t cb, bool *pfUsed)
{
    RT_NOREF(pvThis, off, cb, pfUsed);
    return VERR_NOT_IMPLEMENTED;
}


DECL_HIDDEN_CONST(const RTVFSOPS) g_rtFsIso9660VolOps =
{
    { /* Obj */
        RTVFSOBJOPS_VERSION,
        RTVFSOBJTYPE_VFS,
        "ISO 9660",
        rtFsIso9660Vol_Close,
        rtFsIso9660Vol_QueryInfo,
        RTVFSOBJOPS_VERSION
    },
    RTVFSOPS_VERSION,
    0 /* fFeatures */,
    rtFsIso9660Vol_OpenRoot,
    rtFsIso9660Vol_IsRangeInUse,
    RTVFSOPS_VERSION
};



#ifdef LOG_ENABLED

/** Logging helper. */
static size_t rtFsIso9660VolGetStrippedLength(const char *pachField, size_t cchField)
{
    while (cchField > 0 && pachField[cchField - 1] == ' ')
        cchField--;
    return cchField;
}


/**
 * Logs the primary or supplementary volume descriptor
 *
 * @param   pVolDesc            The descriptor.
 */
static void rtFsIso9660VolLogPrimarySupplementaryVolDesc(PCISO9660SUPVOLDESC pVolDesc)
{
    if (LogIs2Enabled())
    {
        Log2(("ISO9660:  fVolumeFlags:              %#RX8\n", pVolDesc->fVolumeFlags));
        Log2(("ISO9660:  achSystemId:               '%.*s'\n", rtFsIso9660VolGetStrippedLength(pVolDesc->achSystemId, sizeof(pVolDesc->achSystemId)), pVolDesc->achSystemId));
        Log2(("ISO9660:  achVolumeId:               '%.*s'\n", rtFsIso9660VolGetStrippedLength(pVolDesc->achVolumeId, sizeof(pVolDesc->achVolumeId)), pVolDesc->achVolumeId));
        Log2(("ISO9660:  Unused73:                  {%#RX32,%#RX32}\n", RT_BE2H_U32(pVolDesc->Unused73.be), RT_LE2H_U32(pVolDesc->Unused73.le)));
        Log2(("ISO9660:  VolumeSpaceSize:           {%#RX32,%#RX32}\n", RT_BE2H_U32(pVolDesc->VolumeSpaceSize.be), RT_LE2H_U32(pVolDesc->VolumeSpaceSize.le)));
        Log2(("ISO9660:  abEscapeSequences:         '%.*s'\n", rtFsIso9660VolGetStrippedLength((char *)pVolDesc->abEscapeSequences, sizeof(pVolDesc->abEscapeSequences)), pVolDesc->abEscapeSequences));
        Log2(("ISO9660:  cVolumesInSet:             {%#RX16,%#RX16}\n", RT_BE2H_U16(pVolDesc->cVolumesInSet.be), RT_LE2H_U16(pVolDesc->cVolumesInSet.le)));
        Log2(("ISO9660:  VolumeSeqNo:               {%#RX16,%#RX16}\n", RT_BE2H_U16(pVolDesc->VolumeSeqNo.be), RT_LE2H_U16(pVolDesc->VolumeSeqNo.le)));
        Log2(("ISO9660:  cbLogicalBlock:            {%#RX16,%#RX16}\n", RT_BE2H_U16(pVolDesc->cbLogicalBlock.be), RT_LE2H_U16(pVolDesc->cbLogicalBlock.le)));
        Log2(("ISO9660:  cbPathTable:               {%#RX32,%#RX32}\n", RT_BE2H_U32(pVolDesc->cbPathTable.be), RT_LE2H_U32(pVolDesc->cbPathTable.le)));
        Log2(("ISO9660:  offTypeLPathTable:         %#RX32\n", RT_LE2H_U32(pVolDesc->offTypeLPathTable)));
        Log2(("ISO9660:  offOptionalTypeLPathTable: %#RX32\n", RT_LE2H_U32(pVolDesc->offOptionalTypeLPathTable)));
        Log2(("ISO9660:  offTypeMPathTable:         %#RX32\n", RT_BE2H_U32(pVolDesc->offTypeMPathTable)));
        Log2(("ISO9660:  offOptionalTypeMPathTable: %#RX32\n", RT_BE2H_U32(pVolDesc->offOptionalTypeMPathTable)));
        Log2(("ISO9660:  achVolumeSetId:            '%.*s'\n", rtFsIso9660VolGetStrippedLength(pVolDesc->achVolumeSetId, sizeof(pVolDesc->achVolumeSetId)), pVolDesc->achVolumeSetId));
        Log2(("ISO9660:  achPublisherId:            '%.*s'\n", rtFsIso9660VolGetStrippedLength(pVolDesc->achPublisherId, sizeof(pVolDesc->achPublisherId)), pVolDesc->achPublisherId));
        Log2(("ISO9660:  achDataPreparerId:         '%.*s'\n", rtFsIso9660VolGetStrippedLength(pVolDesc->achDataPreparerId, sizeof(pVolDesc->achDataPreparerId)), pVolDesc->achDataPreparerId));
        Log2(("ISO9660:  achApplicationId:          '%.*s'\n", rtFsIso9660VolGetStrippedLength(pVolDesc->achApplicationId, sizeof(pVolDesc->achApplicationId)), pVolDesc->achApplicationId));
        Log2(("ISO9660:  achCopyrightFileId:        '%.*s'\n", rtFsIso9660VolGetStrippedLength(pVolDesc->achCopyrightFileId, sizeof(pVolDesc->achCopyrightFileId)), pVolDesc->achCopyrightFileId));
        Log2(("ISO9660:  achAbstractFileId:         '%.*s'\n", rtFsIso9660VolGetStrippedLength(pVolDesc->achAbstractFileId, sizeof(pVolDesc->achAbstractFileId)), pVolDesc->achAbstractFileId));
        Log2(("ISO9660:  achBibliographicFileId:    '%.*s'\n", rtFsIso9660VolGetStrippedLength(pVolDesc->achBibliographicFileId, sizeof(pVolDesc->achBibliographicFileId)), pVolDesc->achBibliographicFileId));
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
        Log2(("ISO9660:  RootDir.cbExtAttr:                  %#RX8\n", pVolDesc->RootDir.DirRec.cbExtAttr));
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
static int rtFsIso9660VolHandleRootDir(PRTFSISO9660VOL pThis, PCISO9660DIRREC pRootDir,
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
static int rtFsIso9660VolHandlePrimaryVolDesc(PRTFSISO9660VOL pThis, PCISO9660PRIMARYVOLDESC pVolDesc, uint32_t offVolDesc,
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
    return rtFsIso9660VolHandleRootDir(pThis, &pVolDesc->RootDir.DirRec, pRootDir, pErrInfo);
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
static int rtFsIso9660VolHandleSupplementaryVolDesc(PRTFSISO9660VOL pThis, PCISO9660SUPVOLDESC pVolDesc, uint32_t offVolDesc,
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
    int rc = rtFsIso9660VolHandleRootDir(pThis, &pVolDesc->RootDir.DirRec, pRootDir, pErrInfo);
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
 * @param   pThis           The FAT VFS instance to initialize.
 * @param   hVfsSelf        The FAT VFS handle (no reference consumed).
 * @param   hVfsBacking     The file backing the alleged FAT file system.
 *                          Reference is consumed (via rtFsIso9660Vol_Close).
 * @param   fFlags          Flags, MBZ.
 * @param   pErrInfo        Where to return additional error info.  Can be NULL.
 */
static int rtFsIso9660VolTryInit(PRTFSISO9660VOL pThis, RTVFS hVfsSelf, RTVFSFILE hVfsBacking, uint32_t fFlags, PRTERRINFO pErrInfo)
{
    uint32_t const cbSector = 2048;

    /*
     * First initialize the state so that rtFsIso9660Vol_Destroy won't trip up.
     */
    pThis->hVfsSelf                     = hVfsSelf;
    pThis->hVfsBacking                  = hVfsBacking; /* Caller referenced it for us, we consume it; rtFsIso9660Vol_Destroy releases it. */
    pThis->cbBacking                    = 0;
    pThis->fFlags                       = fFlags;
    pThis->cbSector                     = cbSector;
    pThis->cbBlock                      = 0;
    pThis->cBlocksInPrimaryVolumeSpace  = 0;
    pThis->cbPrimaryVolumeSpace         = 0;
    pThis->cVolumesInSet                = 0;
    pThis->idPrimaryVol                 = UINT32_MAX;
    pThis->fIsUtf16                     = false;
    pThis->hVfsRootDir                  = NIL_RTVFSDIR;
    pThis->pRootDir                     = NULL;

    /*
     * Get stuff that may fail.
     */
    int rc = RTVfsFileGetSize(hVfsBacking, &pThis->cbBacking);
    if (RT_FAILURE(rc))
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

    uint32_t        cPrimaryVolDescs        = 0;
    uint32_t        cSupplementaryVolDescs  = 0;
    uint32_t        cBootRecordVolDescs     = 0;
    uint32_t        offVolDesc              = 16 * cbSector;
    for (uint32_t iVolDesc = 0; ; iVolDesc++, offVolDesc += cbSector)
    {
        if (iVolDesc > 32)
            return RTErrInfoSet(pErrInfo, rc, "More than 32 volume descriptors, doesn't seem right...");

        /* Read the next one and check the signature. */
        rc = RTVfsFileReadAt(hVfsBacking, offVolDesc, &Buf, cbSector, NULL);
        if (RT_FAILURE(rc))
            return RTErrInfoSetF(pErrInfo, rc, "Unable to read volume descriptor #%u", iVolDesc);

        if (   Buf.VolDescHdr.achStdId[0] != ISO9660VOLDESC_STD_ID_0
            || Buf.VolDescHdr.achStdId[1] != ISO9660VOLDESC_STD_ID_1
            || Buf.VolDescHdr.achStdId[2] != ISO9660VOLDESC_STD_ID_2
            || Buf.VolDescHdr.achStdId[3] != ISO9660VOLDESC_STD_ID_3
            || Buf.VolDescHdr.achStdId[4] != ISO9660VOLDESC_STD_ID_4)
        {
            if (!iVolDesc)
                return RTErrInfoSetF(pErrInfo, VERR_VFS_UNKNOWN_FORMAT,
                                     "No ISO 9660 CD001 signature, instead found: %.5Rhxs", Buf.VolDescHdr.achStdId);
            return RTErrInfoSet(pErrInfo, VERR_VFS_BOGUS_FORMAT, "Missing terminator volume descriptor?");
        }

        /* Do type specific handling. */
        Log(("ISO9660: volume desc #%u: type=%#x\n", iVolDesc, Buf.VolDescHdr.bDescType));
        if (Buf.VolDescHdr.bDescType == ISO9660VOLDESC_TYPE_PRIMARY)
        {
            cPrimaryVolDescs++;
            if (Buf.VolDescHdr.bDescVersion != ISO9660PRIMARYVOLDESC_VERSION)
                return RTErrInfoSetF(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT,
                                     "Unsupported primary volume descriptor version: %#x", Buf.VolDescHdr.bDescVersion);
#ifdef LOG_ENABLED
            rtFsIso9660VolLogPrimarySupplementaryVolDesc(&Buf.SupVolDesc);
#endif
            if (cPrimaryVolDescs > 1)
                return RTErrInfoSet(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT, "More than one primary volume descriptor");
            rc = rtFsIso9660VolHandlePrimaryVolDesc(pThis, &Buf.PrimaryVolDesc, offVolDesc, &RootDir, &offRootDirRec, pErrInfo);
        }
        else if (Buf.VolDescHdr.bDescType == ISO9660VOLDESC_TYPE_SUPPLEMENTARY)
        {
            cSupplementaryVolDescs++;
            if (Buf.VolDescHdr.bDescVersion != ISO9660SUPVOLDESC_VERSION)
                return RTErrInfoSetF(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT,
                                     "Unsupported supplemental volume descriptor version: %#x", Buf.VolDescHdr.bDescVersion);
#ifdef LOG_ENABLED
            rtFsIso9660VolLogPrimarySupplementaryVolDesc(&Buf.SupVolDesc);
#endif
            rc = rtFsIso9660VolHandleSupplementaryVolDesc(pThis, &Buf.SupVolDesc, offVolDesc, &bJolietUcs2Level, &JolietRootDir,
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
            break;
        }
        else
            return RTErrInfoSetF(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT,
                                 "Unknown volume descriptor: %#x", Buf.VolDescHdr.bDescType);
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
        return rtFsIso9660Dir_New(pThis, NULL, &JolietRootDir, 1, offJolietRootDirRec, &pThis->hVfsRootDir, &pThis->pRootDir);
    }
    return rtFsIso9660Dir_New(pThis, NULL, &RootDir, 1, offRootDirRec, &pThis->hVfsRootDir, &pThis->pRootDir);
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
    int rc = RTVfsNew(&g_rtFsIso9660VolOps, sizeof(RTFSISO9660VOL), NIL_RTVFS, RTVFSLOCK_CREATE_RW, &hVfs, &pvThis);
    if (RT_SUCCESS(rc))
    {
        rc = rtFsIso9660VolTryInit((PRTFSISO9660VOL)pvThis, hVfs, hVfsFileIn, fFlags, pErrInfo);
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
                                "The 'nojoliet' option make it ignore any joliet supplemental volume.\n"
                                "The 'norock' option make it ignore any rock ridge info.\n",
    /* pfnValidate = */         rtVfsChainIso9660Vol_Validate,
    /* pfnInstantiate = */      rtVfsChainIso9660Vol_Instantiate,
    /* pfnCanReuseElement = */  rtVfsChainIso9660Vol_CanReuseElement,
    /* uEndMarker = */          RTVFSCHAINELEMENTREG_VERSION
};

RTVFSCHAIN_AUTO_REGISTER_ELEMENT_PROVIDER(&g_rtVfsChainIso9660VolReg, rtVfsChainIso9660VolReg);

