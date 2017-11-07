/* $Id$ */
/** @file
 * IPRT Disk Volume Management API (DVM) - VFS glue.
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
#define LOG_GROUP RTLOGGROUP_FS /** @todo fix log group  */
#include <iprt/types.h>
#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/dvm.h>
#include <iprt/err.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#include <iprt/file.h>
#include <iprt/sg.h>
#include <iprt/vfslowlevel.h>
#include <iprt/poll.h>
#include <iprt/log.h>
#include "internal/dvm.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/**
 * The internal data of a DVM volume I/O stream.
 */
typedef struct RTVFSDVMFILE
{
    /** The volume the VFS file belongs to. */
    RTDVMVOLUME    hVol;
    /** Current position. */
    uint64_t       offCurPos;
} RTVFSDVMFILE;
/** Pointer to a the internal data of a DVM volume file. */
typedef RTVFSDVMFILE *PRTVFSDVMFILE;

/**
 * A volume manager VFS for use in chains (thing pseudo/devfs).
 */
typedef struct RTDVMVFSVOL
{
    /** The volume manager. */
    RTDVM       hVolMgr;
    /** Whether to close it on success. */
    bool        fCloseDvm;
    /** Whether the access is read-only. */
    bool        fReadOnly;
    /** Number of volumes. */
    uint32_t    cVolumes;
} RTDVMVFSVOL;
/** Poitner to a volume manager VFS. */
typedef RTDVMVFSVOL *PRTDVMVFSVOL;

/**
 * The volume manager VFS root dir data.
 */
typedef struct RTDVMVFSROOTDIR
{
    /** Pointer to the VFS volume. */
    PRTDVMVFSVOL    pVfsVol;
    /** The current directory offset. */
    uint32_t        offDir;
} RTDVMVFSROOTDIR;
/** Poitner to a volume manager VFS root dir. */
typedef RTDVMVFSROOTDIR *PRTDVMVFSROOTDIR;


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnClose}
 */
static DECLCALLBACK(int) rtDvmVfsFile_Close(void *pvThis)
{
    PRTVFSDVMFILE pThis = (PRTVFSDVMFILE)pvThis;

    RTDvmVolumeRelease(pThis->hVol);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnQueryInfo}
 */
static DECLCALLBACK(int) rtDvmVfsFile_QueryInfo(void *pvThis, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    NOREF(pvThis);
    NOREF(pObjInfo);
    NOREF(enmAddAttr);
    return VERR_NOT_SUPPORTED;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnRead}
 */
static DECLCALLBACK(int) rtDvmVfsFile_Read(void *pvThis, RTFOFF off, PCRTSGBUF pSgBuf, bool fBlocking, size_t *pcbRead)
{
    PRTVFSDVMFILE pThis = (PRTVFSDVMFILE)pvThis;
    int rc = VINF_SUCCESS;

    Assert(pSgBuf->cSegs == 1);
    NOREF(fBlocking);

    /*
     * Find the current position and check if it's within the volume.
     */
    uint64_t offUnsigned = off < 0 ? pThis->offCurPos : (uint64_t)off;
    if (offUnsigned >= RTDvmVolumeGetSize(pThis->hVol))
    {
        if (pcbRead)
        {
            *pcbRead = 0;
            pThis->offCurPos = offUnsigned;
            return VINF_EOF;
        }
        return VERR_EOF;
    }

    size_t cbLeftToRead;
    if (offUnsigned + pSgBuf->paSegs[0].cbSeg > RTDvmVolumeGetSize(pThis->hVol))
    {
        if (!pcbRead)
            return VERR_EOF;
        *pcbRead = cbLeftToRead = (size_t)(RTDvmVolumeGetSize(pThis->hVol) - offUnsigned);
    }
    else
    {
        cbLeftToRead = pSgBuf->paSegs[0].cbSeg;
        if (pcbRead)
            *pcbRead = cbLeftToRead;
    }

    /*
     * Ok, we've got a valid stretch within the file.  Do the reading.
     */
    if (cbLeftToRead > 0)
    {
        rc = RTDvmVolumeRead(pThis->hVol, (uint64_t)off, pSgBuf->paSegs[0].pvSeg, cbLeftToRead);
        if (RT_SUCCESS(rc))
            offUnsigned += cbLeftToRead;
    }

    pThis->offCurPos = offUnsigned;
    return rc;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnWrite}
 */
static DECLCALLBACK(int) rtDvmVfsFile_Write(void *pvThis, RTFOFF off, PCRTSGBUF pSgBuf, bool fBlocking, size_t *pcbWritten)
{
    PRTVFSDVMFILE pThis = (PRTVFSDVMFILE)pvThis;
    int rc = VINF_SUCCESS;

    Assert(pSgBuf->cSegs == 1);
    NOREF(fBlocking);

    /*
     * Find the current position and check if it's within the volume.
     * Writing beyond the end of a volume is not supported.
     */
    uint64_t offUnsigned = off < 0 ? pThis->offCurPos : (uint64_t)off;
    if (offUnsigned >= RTDvmVolumeGetSize(pThis->hVol))
    {
        if (pcbWritten)
        {
            *pcbWritten = 0;
            pThis->offCurPos = offUnsigned;
        }
        return VERR_NOT_SUPPORTED;
    }

    size_t cbLeftToWrite;
    if (offUnsigned + pSgBuf->paSegs[0].cbSeg > RTDvmVolumeGetSize(pThis->hVol))
    {
        if (!pcbWritten)
            return VERR_EOF;
        *pcbWritten = cbLeftToWrite = (size_t)(RTDvmVolumeGetSize(pThis->hVol) - offUnsigned);
    }
    else
    {
        cbLeftToWrite = pSgBuf->paSegs[0].cbSeg;
        if (pcbWritten)
            *pcbWritten = cbLeftToWrite;
    }

    /*
     * Ok, we've got a valid stretch within the file.  Do the reading.
     */
    if (cbLeftToWrite > 0)
    {
        rc = RTDvmVolumeWrite(pThis->hVol, (uint64_t)off, pSgBuf->paSegs[0].pvSeg, cbLeftToWrite);
        if (RT_SUCCESS(rc))
            offUnsigned += cbLeftToWrite;
    }

    pThis->offCurPos = offUnsigned;
    return rc;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnFlush}
 */
static DECLCALLBACK(int) rtDvmVfsFile_Flush(void *pvThis)
{
    NOREF(pvThis);
    return VINF_SUCCESS; /** @todo Implement missing DVM API. */
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnPollOne}
 */
static DECLCALLBACK(int) rtDvmVfsFile_PollOne(void *pvThis, uint32_t fEvents, RTMSINTERVAL cMillies, bool fIntr,
                                              uint32_t *pfRetEvents)
{
    NOREF(pvThis);
    int rc;
    if (fEvents != RTPOLL_EVT_ERROR)
    {
        *pfRetEvents = fEvents & ~RTPOLL_EVT_ERROR;
        rc = VINF_SUCCESS;
    }
    else
        rc = RTVfsUtilDummyPollOne(fEvents, cMillies, fIntr, pfRetEvents);
    return rc;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnTell}
 */
static DECLCALLBACK(int) rtDvmVfsFile_Tell(void *pvThis, PRTFOFF poffActual)
{
    PRTVFSDVMFILE pThis = (PRTVFSDVMFILE)pvThis;
    *poffActual = pThis->offCurPos;
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnMode}
 */
static DECLCALLBACK(int) rtDvmVfsFile_SetMode(void *pvThis, RTFMODE fMode, RTFMODE fMask)
{
    NOREF(pvThis);
    NOREF(fMode);
    NOREF(fMask);
    return VERR_NOT_SUPPORTED;
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnSetTimes}
 */
static DECLCALLBACK(int) rtDvmVfsFile_SetTimes(void *pvThis, PCRTTIMESPEC pAccessTime, PCRTTIMESPEC pModificationTime,
                                               PCRTTIMESPEC pChangeTime, PCRTTIMESPEC pBirthTime)
{
    NOREF(pvThis);
    NOREF(pAccessTime);
    NOREF(pModificationTime);
    NOREF(pChangeTime);
    NOREF(pBirthTime);
    return VERR_NOT_SUPPORTED;
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnSetOwner}
 */
static DECLCALLBACK(int) rtDvmVfsFile_SetOwner(void *pvThis, RTUID uid, RTGID gid)
{
    NOREF(pvThis);
    NOREF(uid);
    NOREF(gid);
    return VERR_NOT_SUPPORTED;
}


/**
 * @interface_method_impl{RTVFSFILEOPS,pfnSeek}
 */
static DECLCALLBACK(int) rtDvmVfsFile_Seek(void *pvThis, RTFOFF offSeek, unsigned uMethod, PRTFOFF poffActual)
{
    PRTVFSDVMFILE pThis = (PRTVFSDVMFILE)pvThis;

    /*
     * Seek relative to which position.
     */
    uint64_t offWrt;
    switch (uMethod)
    {
        case RTFILE_SEEK_BEGIN:
            offWrt = 0;
            break;

        case RTFILE_SEEK_CURRENT:
            offWrt = pThis->offCurPos;
            break;

        case RTFILE_SEEK_END:
            offWrt = RTDvmVolumeGetSize(pThis->hVol);
            break;

        default:
            return VERR_INTERNAL_ERROR_5;
    }

    /*
     * Calc new position, take care to stay within bounds.
     *
     * @todo: Setting position beyond the end of the volume does not make sense.
     */
    uint64_t offNew;
    if (offSeek == 0)
        offNew = offWrt;
    else if (offSeek > 0)
    {
        offNew = offWrt + offSeek;
        if (   offNew < offWrt
            || offNew > RTFOFF_MAX)
            offNew = RTFOFF_MAX;
    }
    else if ((uint64_t)-offSeek < offWrt)
        offNew = offWrt + offSeek;
    else
        offNew = 0;

    /*
     * Update the state and set return value.
     */
    pThis->offCurPos = offNew;

    *poffActual = offNew;
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSFILEOPS,pfnQuerySize}
 */
static DECLCALLBACK(int) rtDvmVfsFile_QuerySize(void *pvThis, uint64_t *pcbFile)
{
    PRTVFSDVMFILE pThis = (PRTVFSDVMFILE)pvThis;
    *pcbFile = RTDvmVolumeGetSize(pThis->hVol);
    return VINF_SUCCESS;
}


/**
 * Standard file operations.
 */
DECL_HIDDEN_CONST(const RTVFSFILEOPS) g_rtDvmVfsStdFileOps =
{
    { /* Stream */
        { /* Obj */
            RTVFSOBJOPS_VERSION,
            RTVFSOBJTYPE_FILE,
            "DvmFile",
            rtDvmVfsFile_Close,
            rtDvmVfsFile_QueryInfo,
            RTVFSOBJOPS_VERSION
        },
        RTVFSIOSTREAMOPS_VERSION,
        RTVFSIOSTREAMOPS_FEAT_NO_SG,
        rtDvmVfsFile_Read,
        rtDvmVfsFile_Write,
        rtDvmVfsFile_Flush,
        rtDvmVfsFile_PollOne,
        rtDvmVfsFile_Tell,
        NULL /*Skip*/,
        NULL /*ZeroFill*/,
        RTVFSIOSTREAMOPS_VERSION,
    },
    RTVFSFILEOPS_VERSION,
    /*RTVFSIOFILEOPS_FEAT_NO_AT_OFFSET*/ 0,
    { /* ObjSet */
        RTVFSOBJSETOPS_VERSION,
        RT_OFFSETOF(RTVFSFILEOPS, Stream.Obj) - RT_OFFSETOF(RTVFSFILEOPS, ObjSet),
        rtDvmVfsFile_SetMode,
        rtDvmVfsFile_SetTimes,
        rtDvmVfsFile_SetOwner,
        RTVFSOBJSETOPS_VERSION
    },
    rtDvmVfsFile_Seek,
    rtDvmVfsFile_QuerySize,
    RTVFSFILEOPS_VERSION
};


RTDECL(int) RTDvmVolumeCreateVfsFile(RTDVMVOLUME hVol, PRTVFSFILE phVfsFileOut)
{
    AssertPtrReturn(hVol, VERR_INVALID_HANDLE);
    AssertPtrReturn(phVfsFileOut, VERR_INVALID_POINTER);

    uint32_t cRefs = RTDvmVolumeRetain(hVol);
    AssertReturn(cRefs != UINT32_MAX, VERR_INVALID_HANDLE);

    /*
     * Create the volume file.
     */
    RTVFSFILE       hVfsFile;
    PRTVFSDVMFILE   pThis;
    int rc = RTVfsNewFile(&g_rtDvmVfsStdFileOps, sizeof(*pThis), RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_WRITE,
                          NIL_RTVFS, NIL_RTVFSLOCK, &hVfsFile, (void **)&pThis);
    if (RT_SUCCESS(rc))
    {
        pThis->offCurPos = 0;
        pThis->hVol      = hVol;

        *phVfsFileOut = hVfsFile;
        return VINF_SUCCESS;
    }
    else
        RTDvmVolumeRelease(hVol);
    return rc;
}




/**
 * @interface_method_impl{RTVFSOBJOPS::Obj,pfnClose}
 */
static DECLCALLBACK(int) rtDvmVfsVol_Close(void *pvThis)
{
    PRTDVMVFSVOL pThis = (PRTDVMVFSVOL)pvThis;
    LogFlow(("rtDvmVfsVol_Close(%p)\n", pThis));

    if (   pThis->fCloseDvm
        && pThis->hVolMgr != NIL_RTDVM )
        RTDvmRelease(pThis->hVolMgr);
    pThis->hVolMgr = NIL_RTDVM;

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSOBJOPS::Obj,pfnQueryInfo}
 */
static DECLCALLBACK(int) rtDvmVfsVol_QueryInfo(void *pvThis, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    RT_NOREF(pvThis, pObjInfo, enmAddAttr);
    return VERR_WRONG_TYPE;
}


/**
 * @interface_method_impl{RTVFSOPS,pfnOpenRoot}
 */
static DECLCALLBACK(int) rtDvmVfsVol_OpenRoot(void *pvThis, PRTVFSDIR phVfsDir)
{
    //PRTDVMVFSVOL pThis = (PRTDVMVFSVOL)pvThis;

    //rtDvmDirShrd_Retain(pThis->pRootDir); /* consumed by the next call */
    //return rtDvmDir_NewWithShared(pThis, pThis->pRootDir, phVfsDir);
    RT_NOREF(pvThis, phVfsDir);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * @interface_method_impl{RTVFSOPS,pfnIsRangeInUse}
 */
static DECLCALLBACK(int) rtDvmVfsVol_IsRangeInUse(void *pvThis, RTFOFF off, size_t cb, bool *pfUsed)
{
    RT_NOREF(pvThis, off, cb, pfUsed);
    return VERR_NOT_IMPLEMENTED;
}


DECL_HIDDEN_CONST(const RTVFSOPS) g_rtDvmVfsVolOps =
{
    { /* Obj */
        RTVFSOBJOPS_VERSION,
        RTVFSOBJTYPE_VFS,
        "DvmVol",
        rtDvmVfsVol_Close,
        rtDvmVfsVol_QueryInfo,
        RTVFSOBJOPS_VERSION
    },
    RTVFSOPS_VERSION,
    0 /* fFeatures */,
    rtDvmVfsVol_OpenRoot,
    rtDvmVfsVol_IsRangeInUse,
    RTVFSOPS_VERSION
};



/**
 * @interface_method_impl{RTVFSCHAINELEMENTREG,pfnValidate}
 */
static DECLCALLBACK(int) rtDvmVfsChain_Validate(PCRTVFSCHAINELEMENTREG pProviderReg, PRTVFSCHAINSPEC pSpec,
                                                PRTVFSCHAINELEMSPEC pElement, uint32_t *poffError, PRTERRINFO pErrInfo)
{
    RT_NOREF(pProviderReg, pSpec);

    /*
     * Basic checks.
     */
    if (pElement->enmTypeIn != RTVFSOBJTYPE_FILE)
        return pElement->enmTypeIn == RTVFSOBJTYPE_INVALID ? VERR_VFS_CHAIN_CANNOT_BE_FIRST_ELEMENT : VERR_VFS_CHAIN_TAKES_FILE;
    if (pElement->enmType != RTVFSOBJTYPE_VFS)
        return VERR_VFS_CHAIN_ONLY_VFS;

    if (pElement->cArgs > 1)
        return VERR_VFS_CHAIN_AT_MOST_ONE_ARG;

    /*
     * Parse the flag if present, save in pElement->uProvider.
     */
    /** @todo allow specifying sector size   */
    bool fReadOnly = (pSpec->fOpenFile & RTFILE_O_ACCESS_MASK) == RTFILE_O_READ;
    if (pElement->cArgs > 0)
    {
        const char *psz = pElement->paArgs[0].psz;
        if (*psz)
        {
            if (   !strcmp(psz, "ro")
                || !strcmp(psz, "r"))
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
static DECLCALLBACK(int) rtDvmVfsChain_Instantiate(PCRTVFSCHAINELEMENTREG pProviderReg, PCRTVFSCHAINSPEC pSpec,
                                                   PCRTVFSCHAINELEMSPEC pElement, RTVFSOBJ hPrevVfsObj,
                                                   PRTVFSOBJ phVfsObj, uint32_t *poffError, PRTERRINFO pErrInfo)
{
    RT_NOREF(pProviderReg, pSpec, poffError, pErrInfo);
    AssertReturn(hPrevVfsObj != NIL_RTVFSOBJ, VERR_VFS_CHAIN_IPE);

    /*
     * Instantiate the volume manager and open the map stuff.
     */
    RTVFSFILE hPrevVfsFile = RTVfsObjToFile(hPrevVfsObj);
    AssertReturn(hPrevVfsFile != NIL_RTVFSFILE, VERR_VFS_CHAIN_CAST_FAILED);

    RTDVM hVolMgr;
    int rc = RTDvmCreate(&hVolMgr, hPrevVfsFile, 512, 0 /*fFlags*/);
    RTVfsFileRelease(hPrevVfsFile);
    if (RT_SUCCESS(rc))
    {
        rc = RTDvmMapOpen(hVolMgr);
        if (RT_SUCCESS(rc))
        {
            /*
             * Create a VFS instance for the volume manager.
             */
            RTVFS        hVfs  = NIL_RTVFS;
            PRTDVMVFSVOL pThis = NULL;
            int rc = RTVfsNew(&g_rtDvmVfsVolOps, sizeof(RTDVMVFSVOL), NIL_RTVFS, RTVFSLOCK_CREATE_RW, &hVfs, (void **)&pThis);
            if (RT_SUCCESS(rc))
            {
                pThis->hVolMgr   = hVolMgr;
                pThis->fCloseDvm = true;
                pThis->fReadOnly = pElement->uProvider == (uint64_t)true;
                pThis->cVolumes  = RTDvmMapGetValidVolumes(hVolMgr);

                *phVfsObj = RTVfsObjFromVfs(hVfs);
                RTVfsRelease(hVfs);
                return *phVfsObj != NIL_RTVFSOBJ ? VINF_SUCCESS : VERR_VFS_CHAIN_CAST_FAILED;
            }
        }
        else
            rc = RTErrInfoSetF(pErrInfo, rc, "RTDvmMapOpen failed: %Rrc", rc);
        RTDvmRelease(hVolMgr);
    }
    else
        rc = RTErrInfoSetF(pErrInfo, rc, "RTDvmCreate failed: %Rrc", rc);
    return rc;
}


/**
 * @interface_method_impl{RTVFSCHAINELEMENTREG,pfnCanReuseElement}
 */
static DECLCALLBACK(bool) rtDvmVfsChain_CanReuseElement(PCRTVFSCHAINELEMENTREG pProviderReg,
                                                        PCRTVFSCHAINSPEC pSpec, PCRTVFSCHAINELEMSPEC pElement,
                                                        PCRTVFSCHAINSPEC pReuseSpec, PCRTVFSCHAINELEMSPEC pReuseElement)
{
    RT_NOREF(pProviderReg, pSpec, pElement, pReuseSpec, pReuseElement);
    return false;
}


/** VFS chain element 'file'. */
static RTVFSCHAINELEMENTREG g_rtVfsChainIsoFsVolReg =
{
    /* uVersion = */            RTVFSCHAINELEMENTREG_VERSION,
    /* fReserved = */           0,
    /* pszName = */             "dvm",
    /* ListEntry = */           { NULL, NULL },
    /* pszHelp = */             "Opens a container image using the VD API.\n",
    /* pfnValidate = */         rtDvmVfsChain_Validate,
    /* pfnInstantiate = */      rtDvmVfsChain_Instantiate,
    /* pfnCanReuseElement = */  rtDvmVfsChain_CanReuseElement,
    /* uEndMarker = */          RTVFSCHAINELEMENTREG_VERSION
};

RTVFSCHAIN_AUTO_REGISTER_ELEMENT_PROVIDER(&g_rtVfsChainIsoFsVolReg, rtVfsChainIsoFsVolReg);

