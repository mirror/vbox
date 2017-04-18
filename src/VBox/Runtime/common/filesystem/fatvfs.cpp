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
//#include <iprt/???.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/file.h>
#include <iprt/err.h>
#include <iprt/poll.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/vfs.h>
#include <iprt/vfslowlevel.h>


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

/**
 * FAT file system object (common part to files and dirs).
 */
typedef struct RTFSFATOBJ
{
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
 */
typedef struct RTFSFATDIR
{
    /** Core FAT object info.  */
    RTFSFATOBJ      Core;
} RTFSFATDIR;
typedef RTFSFATDIR *PRTFSFATDIR;


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
    /** Set if read-only mode. */
    bool        fReadOnly;

    /** The cluster size in bytes. */
    uint32_t    cbCluster;
    /** The number of data clusters. */
    uint32_t    cClusters;
    /** The offset of the first cluster. */
    uint64_t    offFirstCluster;

    /** The FAT type. */
    RTFSFATTYPE enmFatType;
    /** Number of FAT entries (clusters). */
    uint32_t    cFatEntries;
    /** Number of FATs. */
    uint32_t    cFats;
    /** FAT byte offsets.  */
    uint64_t    aoffFats[8];

    /** The root directory byte offset. */
    uint64_t    offRootDir;

} RTFSFATVOL;


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
 * @interface_method_impl{RTVFSOPS,pfnDestroy}
 */
static DECLCALLBACK(void) rtFsFatVol_Destroy(void *pvThis)
{
    RT_NOREF(pvThis);
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
    RTVFSOPS_VERSION,
    0 /* fFeatures */,
    "FatVol",
    rtFsFatVol_Destroy,
    rtFsFatVol_OpenRoot,
    rtFsFatVol_IsRangeInUse,
    RTVFSOPS_VERSION
};



RTDECL(int) RTFsFatVolOpen(RTVFSFILE hVfsFileIn, bool fReadOnly, PRTVFS phVfs, PRTERRINFO pErrInfo)
{
    RT_NOREF(hVfsFileIn, fReadOnly, pErrInfo, phVfs);
    return VERR_NOT_IMPLEMENTED;
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
        rc = RTFsFatVolOpen(hVfsFileIn, pElement->uProvider != false, &hVfs, pErrInfo);
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

