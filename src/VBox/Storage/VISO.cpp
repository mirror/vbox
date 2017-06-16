/* $Id$ */
/** @file
 * VISO - Virtual ISO disk image, Core Code.
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
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_VD
#include <VBox/vd-plugin.h>
#include <VBox/err.h>

#include <VBox/log.h>
//#include <VBox/scsiinline.h>
#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/fsisomaker.h>
#include <iprt/getopt.h>
#include <iprt/mem.h>
#include <iprt/string.h>

#include "VDBackends.h"
#include "VDBackendsInline.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** The maximum file size. */
#if ARCH_BITS >= 64
# define VISO_MAX_FILE_SIZE     _64M
#else
# define VISO_MAX_FILE_SIZE     _16M
#endif


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/**
 * VBox ISO maker image instance.
 */
typedef struct VISOIMAGE
{
    /** The ISO maker output file handle. */
    RTVFSFILE           hIsoFile;
    /** The image size. */
    uint64_t            cbImage;
    /** Open flags passed by VD layer. */
    uint32_t            fOpenFlags;

    /** Image name (for debug).
     * Allocation follows the region list, no need to free. */
    const char         *pszFilename;

    /** I/O interface. */
    PVDINTERFACEIOINT   pIfIo;
    /** Error interface. */
    PVDINTERFACEERROR   pIfError;

    /** Internal region list (variable size). */
    VDREGIONLIST        RegionList;
} VISOIMAGE;
/** Pointer to an VBox ISO make image instance. */
typedef VISOIMAGE *PVISOIMAGE;


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/

/** NULL-terminated array of supported file extensions. */
static const VDFILEEXTENSION g_aVBoXIsoMakerFileExtensions[] =
{
    { "vbox-iso-maker", VDTYPE_OPTICAL_DISC },
    { "viso",           VDTYPE_OPTICAL_DISC },
    { NULL,             VDTYPE_INVALID      }
};



/**
 * @interface_method_impl{VDIMAGEBACKEND,pfnProbe}
 */
static DECLCALLBACK(int) visoProbe(const char *pszFilename, PVDINTERFACE pVDIfsDisk, PVDINTERFACE pVDIfsImage, VDTYPE *penmType)
{
    /*
     * Validate input.
     */
    AssertPtrReturn(penmType, VERR_INVALID_POINTER);
    *penmType = VDTYPE_INVALID;

    AssertPtrReturn(pszFilename, VERR_INVALID_POINTER);
    AssertReturn(*pszFilename, VERR_INVALID_POINTER);

    PVDINTERFACEIOINT pIfIo = VDIfIoIntGet(pVDIfsImage);
    AssertPtrReturn(pIfIo, VERR_INVALID_PARAMETER);

    RT_NOREF(pVDIfsDisk);

    /*
     * Open the file.
     */
    PVDIOSTORAGE pStorage = NULL;
    int rc = vdIfIoIntFileOpen(pIfIo, pszFilename, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE, &pStorage);
    if (RT_SUCCESS(rc))
    {
        /*
         * Read a chunk so we can look for the eye-catcher in the first line.
         */
        uint64_t cbFile = 0;
        rc = vdIfIoIntFileGetSize(pIfIo, pStorage, &cbFile);
        if (RT_SUCCESS(rc))
        {
            char szChunk[256];
            size_t cbToRead = (size_t)RT_MIN(sizeof(szChunk) - 1, cbFile);
            rc = vdIfIoIntFileReadSync(pIfIo, pStorage, 0 /*off*/, szChunk, cbToRead);
            if (RT_SUCCESS(rc))
            {
                szChunk[cbToRead] = '\0';
                const char *psz = szChunk;
                while (RT_C_IS_SPACE(*psz))
                    psz++;
                if (strcmp(psz, "--iprt-iso-maker-file-marker") == 0)
                {
                    if (cbFile <= VISO_MAX_FILE_SIZE)
                    {
                        *penmType = VDTYPE_OPTICAL_DISC;
                        rc = VINF_SUCCESS;
                    }
                    else
                    {
                        LogRel(("visoProbe: VERR_FILE_TOO_BIG - cbFile=%#RX64 cbMaxFile=%#RX64\n",
                                cbFile, (uint64_t)VISO_MAX_FILE_SIZE));
                        rc = VERR_FILE_TOO_BIG;
                    }
                }
                else
                    rc = VERR_VD_GEN_INVALID_HEADER;
            }
        }
        vdIfIoIntFileClose(pIfIo, pStorage);
    }
    LogFlowFunc(("returns %Rrc - *penmType=%d\n", rc, *penmType));
    return rc;
}

/**
 * @interface_method_impl{VDIMAGEBACKEND,pfnOpen}
 */
static DECLCALLBACK(int) visoOpen(const char *pszFilename, unsigned uOpenFlags, PVDINTERFACE pVDIfsDisk, PVDINTERFACE pVDIfsImage,
                                  VDTYPE enmType, void **ppBackendData)
{
    LogFlowFunc(("pszFilename='%s' fOpenFlags=%#x pVDIfsDisk=%p pVDIfsImage=%p enmType=%u ppBackendData=%p\n",
                 pszFilename, uOpenFlags, pVDIfsDisk, pVDIfsImage, enmType, ppBackendData));

    /*
     * Validate input.
     */
    AssertPtrReturn(ppBackendData, VERR_INVALID_POINTER);
    *ppBackendData = NULL;

    AssertPtrReturn(pszFilename, VERR_INVALID_POINTER);
    AssertReturn(*pszFilename, VERR_INVALID_POINTER);

    AssertReturn(!(uOpenFlags & ~VD_OPEN_FLAGS_MASK), VERR_INVALID_FLAGS);

    PVDINTERFACEIOINT pIfIo = VDIfIoIntGet(pVDIfsImage);
    AssertPtrReturn(pIfIo, VERR_INVALID_PARAMETER);

    PVDINTERFACEERROR pIfError = VDIfErrorGet(pVDIfsDisk);

    AssertReturn(enmType == VDTYPE_OPTICAL_DISC, VERR_NOT_SUPPORTED);

    /*
     * Open the file and read it into memory.
     */
    PVDIOSTORAGE pStorage = NULL;
    int rc = vdIfIoIntFileOpen(pIfIo, pszFilename, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_NONE, &pStorage);
    if (RT_SUCCESS(rc))
    {
        /*
         * Read the file into memory.
         */
        uint64_t cbFile = 0;
        rc = vdIfIoIntFileGetSize(pIfIo, pStorage, &cbFile);
        if (RT_SUCCESS(rc))
        {
            uint64_t const cbMaxFile = ARCH_BITS >= 64 ? _64M : _16M;
            if (cbFile <= cbMaxFile)
            {
                static char const s_szCmdPrefix[] = "VBox-Iso-Maker ";

                char *pszContent = (char *)RTMemTmpAlloc(sizeof(s_szCmdPrefix) + cbFile);
                if (pszContent)
                {
                    char *pszReadDst = &pszContent[sizeof(s_szCmdPrefix) - 1];
                    rc = vdIfIoIntFileReadSync(pIfIo, pStorage, 0 /*off*/, pszReadDst, (size_t)cbFile);
                    if (RT_SUCCESS(rc))
                    {
                        pszReadDst[(size_t)cbFile] = '\0';
                        memcpy(pszContent, s_szCmdPrefix, sizeof(s_szCmdPrefix) - 1);

                        while (RT_C_IS_SPACE(*pszReadDst))
                            pszReadDst++;
                        if (strcmp(pszReadDst, "--iprt-iso-maker-file-marker") == 0)
                        {
                            /*
                             * Convert it into an argument vector.
                             * Free the content afterwards to reduce memory pressure.
                             */
                            char **papszArgs;
                            int    cArgs;
                            rc = RTGetOptArgvFromString(&papszArgs, &cArgs, pszContent, RTGETOPTARGV_CNV_QUOTE_BOURNE_SH, NULL);

                            RTMemTmpFree(pszContent);
                            pszContent = NULL;

                            if (RT_SUCCESS(rc))
                            {
                                /*
                                 * Try instantiate the ISO image maker.
                                 * Free the argument vector afterward to reduce memory pressure.
                                 */
                                RTVFSFILE       hVfsFile;
                                RTERRINFOSTATIC ErrInfo;
                                rc = RTFsIsoMakerCmdEx(cArgs, papszArgs, &hVfsFile, RTErrInfoInitStatic(&ErrInfo));

                                RTGetOptArgvFree(papszArgs);
                                papszArgs = NULL;

                                if (RT_SUCCESS(rc))
                                {
                                    uint64_t cbImage;
                                    rc = RTVfsFileGetSize(hVfsFile, &cbImage);
                                    if (RT_SUCCESS(rc))
                                    {
                                        /*
                                         * We're good! Just allocate and initialize the backend image instance data.
                                         */
                                        size_t     cbFilename = strlen(pszFilename);
                                        PVISOIMAGE pThis;
                                        pThis = (PVISOIMAGE)RTMemAllocZ(  RT_UOFFSETOF(VISOIMAGE, RegionList.aRegions[1])
                                                                        + cbFilename);
                                        if (pThis)
                                        {
                                            pThis->hIsoFile    = hVfsFile;
                                            hVfsFile = NIL_RTVFSFILE;
                                            pThis->cbImage     = cbImage;
                                            pThis->fOpenFlags  = uOpenFlags;
                                            pThis->pszFilename = (char *)memcpy(&pThis->RegionList.aRegions[1],
                                                                                pszFilename, cbFilename);
                                            pThis->pIfIo       = pIfIo;
                                            pThis->pIfError    = pIfError;

                                            pThis->RegionList.fFlags   = 0;
                                            pThis->RegionList.cRegions = 1;
                                            pThis->RegionList.aRegions[0].offRegion            = 0;
                                            pThis->RegionList.aRegions[0].cRegionBlocksOrBytes = pThis->cbImage;
                                            pThis->RegionList.aRegions[0].cbBlock              = 2048;
                                            pThis->RegionList.aRegions[0].enmDataForm          = VDREGIONDATAFORM_RAW;
                                            pThis->RegionList.aRegions[0].enmMetadataForm      = VDREGIONMETADATAFORM_NONE;
                                            pThis->RegionList.aRegions[0].cbData               = 2048;
                                            pThis->RegionList.aRegions[0].cbMetadata           = 0;

                                            *ppBackendData = pThis;
                                            rc = VINF_SUCCESS;
                                        }
                                        else
                                            rc = VERR_NO_MEMORY;
                                    }
                                    RTVfsFileRelease(hVfsFile);
                                }
                                else
                                {
                                    /** @todo better error reporting!  */
                                }
                            }
                        }
                        else
                            rc = VERR_VD_GEN_INVALID_HEADER;
                    }

                    RTMemTmpFree(pszContent);
                }
                else
                    rc = VERR_NO_TMP_MEMORY;
            }
            else
            {
                LogRel(("visoOpen: VERR_FILE_TOO_BIG - cbFile=%#RX64 cbMaxFile=%#RX64\n",
                        cbFile, (uint64_t)VISO_MAX_FILE_SIZE));
                rc = VERR_FILE_TOO_BIG;
            }
        }
        vdIfIoIntFileClose(pIfIo, pStorage);
    }
    LogFlowFunc(("returns %Rrc (pBackendData=%#p)\n", rc, *ppBackendData));
    return rc;
}

/**
 * @interface_method_impl{VDIMAGEBACKEND,pfnClose}
 */
static DECLCALLBACK(int) visoClose(void *pBackendData, bool fDelete)
{
    PVISOIMAGE pThis = (PVISOIMAGE)pBackendData;
    LogFlowFunc(("pThis=%p fDelete=%RTbool\n", pThis, fDelete));

    if (pThis)
    {
        if (fDelete)
            vdIfIoIntFileDelete(pThis->pIfIo, pThis->pszFilename);

        RTVfsFileRelease(pThis->hIsoFile);
        pThis->hIsoFile = NIL_RTVFSFILE;

        RTMemFree(pThis);
    }

    LogFlowFunc(("returns VINF_SUCCESS\n", VINF_SUCCESS));
    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{VDIMAGEBACKEND,pfnRead}
 */
static DECLCALLBACK(int) visoRead(void *pBackendData, uint64_t uOffset, size_t cbToRead, PVDIOCTX pIoCtx, size_t *pcbActuallyRead)
{
    PVISOIMAGE pThis = (PVISOIMAGE)pBackendData;
    uint64_t   off   = uOffset;
    AssertPtrReturn(pThis, VERR_VD_NOT_OPENED);
    AssertReturn(pThis->hIsoFile != NIL_RTVFSFILE, VERR_VD_NOT_OPENED);
    LogFlowFunc(("pThis=%p off=%#RX64 cbToRead=%#zx pIoCtx=%p pcbActuallyRead=%p\n", pThis, off, cbToRead, pIoCtx, pcbActuallyRead));

    /*
     * Check request.
     */
    AssertReturn(   off < pThis->cbImage
                 || (off == pThis->cbImage && cbToRead == 0), VERR_EOF);

    uint64_t cbLeftInImage = pThis->cbImage - off;
    if (cbToRead >= cbLeftInImage)
        cbToRead = cbLeftInImage; /* ASSUMES the caller can deal with this, given the pcbActuallyRead parameter... */

    /*
     * Work the I/O context using vdIfIoIntIoCtxSegArrayCreate.
     */
    int    rc = VINF_SUCCESS;
    size_t cbActuallyRead = 0;
    while (cbToRead > 0)
    {
        RTSGSEG     Seg;
        unsigned    cSegs = 1;
        size_t      cbThisRead = vdIfIoIntIoCtxSegArrayCreate(pThis->pIfIo, pIoCtx, &Seg, &cSegs, cbToRead);
        AssertBreakStmt(cbThisRead != 0, rc = VERR_INTERNAL_ERROR_2);
        Assert(cbThisRead == Seg.cbSeg);

        rc = RTVfsFileReadAt(pThis->hIsoFile, off, Seg.pvSeg, cbThisRead, NULL);
        AssertRCBreak(rc);

        /* advance. */
        cbActuallyRead += cbThisRead;
        off            += cbThisRead;
        cbToRead       -= cbThisRead;
    }

    *pcbActuallyRead = cbActuallyRead;
    return rc;
}

/**
 * @interface_method_impl{VDIMAGEBACKEND,pfnWrite}
 */
static DECLCALLBACK(int) visoWrite(void *pBackendData, uint64_t uOffset, size_t cbToWrite,
                                   PVDIOCTX pIoCtx, size_t *pcbWriteProcess, size_t *pcbPreRead,
                                   size_t *pcbPostRead, unsigned fWrite)
{
    RT_NOREF(uOffset, cbToWrite, pIoCtx, pcbWriteProcess, pcbPreRead, pcbPostRead, fWrite);
    PVISOIMAGE pThis = (PVISOIMAGE)pBackendData;
    AssertPtrReturn(pThis, VERR_VD_NOT_OPENED);
    AssertReturn(pThis->hIsoFile != NIL_RTVFSFILE, VERR_VD_NOT_OPENED);
    LogFlowFunc(("pThis=%p off=%#RX64 pIoCtx=%p cbToWrite=%#zx pcbWriteProcess=%p pcbPreRead=%p pcbPostRead=%p -> VERR_VD_IMAGE_READ_ONLY\n",
                 pThis, uOffset, pIoCtx, cbToWrite, pcbWriteProcess, pcbPreRead, pcbPostRead));
    return VERR_VD_IMAGE_READ_ONLY;
}

/**
 * @interface_method_impl{VDIMAGEBACKEND,pfnFlush}
 */
static DECLCALLBACK(int) visoFlush(void *pBackendData, PVDIOCTX pIoCtx)
{
    PVISOIMAGE pThis = (PVISOIMAGE)pBackendData;
    AssertPtrReturn(pThis, VERR_VD_NOT_OPENED);
    AssertReturn(pThis->hIsoFile != NIL_RTVFSFILE, VERR_VD_NOT_OPENED);
    RT_NOREF(pIoCtx);
    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{VDIMAGEBACKEND,pfnGetVersion}
 */
static DECLCALLBACK(unsigned) visoGetVersion(void *pBackendData)
{
    PVISOIMAGE pThis = (PVISOIMAGE)pBackendData;
    AssertPtrReturn(pThis, 0);
    AssertReturn(pThis->hIsoFile != NIL_RTVFSFILE, 0);
    LogFlowFunc(("pThis=%#p -> 1\n", pThis));
    return 1;
}

/**
 * @interface_method_impl{VDIMAGEBACKEND,pfnGetFileSize}
 */
static DECLCALLBACK(uint64_t) visoGetFileSize(void *pBackendData)
{
    PVISOIMAGE pThis = (PVISOIMAGE)pBackendData;
    AssertPtrReturn(pThis, 0);
    AssertReturn(pThis->hIsoFile != NIL_RTVFSFILE, 0);
    LogFlowFunc(("pThis=%p -> %RX64\n", pThis, pThis->cbImage));
    return pThis->cbImage;
}

/**
 * @interface_method_impl{VDIMAGEBACKEND,pfnGetPCHSGeometry}
 */
static DECLCALLBACK(int) visoGetPCHSGeometry(void *pBackendData, PVDGEOMETRY pPCHSGeometry)
{
    PVISOIMAGE pThis = (PVISOIMAGE)pBackendData;
    AssertPtrReturn(pThis, VERR_VD_NOT_OPENED);
    AssertReturn(pThis->hIsoFile != NIL_RTVFSFILE, VERR_VD_NOT_OPENED);
    LogFlowFunc(("pThis=%p pPCHSGeometry=%p -> VERR_NOT_SUPPORTED\n", pThis, pPCHSGeometry));
    RT_NOREF(pPCHSGeometry);
    return VERR_NOT_SUPPORTED;
}

/**
 * @interface_method_impl{VDIMAGEBACKEND,pfnSetPCHSGeometry}
 */
static DECLCALLBACK(int) visoSetPCHSGeometry(void *pBackendData, PCVDGEOMETRY pPCHSGeometry)
{
    PVISOIMAGE pThis = (PVISOIMAGE)pBackendData;
    AssertPtrReturn(pThis, VERR_VD_NOT_OPENED);
    AssertReturn(pThis->hIsoFile != NIL_RTVFSFILE, VERR_VD_NOT_OPENED);
    LogFlowFunc(("pThis=%p pPCHSGeometry=%p:{%u/%u/%u} -> VERR_VD_IMAGE_READ_ONLY\n",
                 pThis, pPCHSGeometry, pPCHSGeometry->cCylinders, pPCHSGeometry->cHeads, pPCHSGeometry->cSectors));
    RT_NOREF(pPCHSGeometry);
    return VERR_VD_IMAGE_READ_ONLY;
}

/**
 * @interface_method_impl{VDIMAGEBACKEND,pfnGetLCHSGeometry}
 */
static DECLCALLBACK(int) visoGetLCHSGeometry(void *pBackendData, PVDGEOMETRY pLCHSGeometry)
{
    PVISOIMAGE pThis = (PVISOIMAGE)pBackendData;
    AssertPtrReturn(pThis, VERR_VD_NOT_OPENED);
    AssertReturn(pThis->hIsoFile != NIL_RTVFSFILE, VERR_VD_NOT_OPENED);
    LogFlowFunc(("pThis=%p pLCHSGeometry=%p -> VERR_NOT_SUPPORTED\n", pThis, pLCHSGeometry));
    RT_NOREF(pLCHSGeometry);
    return VERR_NOT_SUPPORTED;
}

/**
 * @interface_method_impl{VDIMAGEBACKEND,pfnSetLCHSGeometry}
 */
static DECLCALLBACK(int) visoSetLCHSGeometry(void *pBackendData, PCVDGEOMETRY pLCHSGeometry)
{
    PVISOIMAGE pThis = (PVISOIMAGE)pBackendData;
    AssertPtrReturn(pThis, VERR_VD_NOT_OPENED);
    AssertReturn(pThis->hIsoFile != NIL_RTVFSFILE, VERR_VD_NOT_OPENED);
    LogFlowFunc(("pThis=%p pLCHSGeometry=%p:{%u/%u/%u} -> VERR_VD_IMAGE_READ_ONLY\n",
                 pThis, pLCHSGeometry, pLCHSGeometry->cCylinders, pLCHSGeometry->cHeads, pLCHSGeometry->cSectors));
    RT_NOREF(pLCHSGeometry);
    return VERR_VD_IMAGE_READ_ONLY;
}

/**
 * @interface_method_impl{VDIMAGEBACKEND,pfnQueryRegions}
 */
static DECLCALLBACK(int) visoQueryRegions(void *pBackendData, PCVDREGIONLIST *ppRegionList)
{
    PVISOIMAGE pThis = (PVISOIMAGE)pBackendData;
    LogFlowFunc(("pThis=%p ppRegionList=%p\n", pThis, ppRegionList));

    *ppRegionList = NULL;
    AssertPtrReturn(pThis, VERR_VD_NOT_OPENED);
    AssertReturn(pThis->hIsoFile != NIL_RTVFSFILE, VERR_VD_NOT_OPENED);

    *ppRegionList = &pThis->RegionList;
    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{VDIMAGEBACKEND,pfnRegionListRelease}
 */
static DECLCALLBACK(void) visoRegionListRelease(void *pBackendData, PCVDREGIONLIST pRegionList)
{
    /* Nothing to do here.  Just assert the input to avoid unused parameter warnings. */
    PVISOIMAGE pThis = (PVISOIMAGE)pBackendData;
    LogFlowFunc(("pThis=%p pRegionList=%p\n", pThis, pRegionList));
    AssertPtrReturnVoid(pThis);
    AssertReturnVoid(pRegionList == &pThis->RegionList || pRegionList == 0);
}

/**
 * @interface_method_impl{VDIMAGEBACKEND,pfnGetImageFlags}
 */
static DECLCALLBACK(unsigned) visoGetImageFlags(void *pBackendData)
{
    PVISOIMAGE pThis = (PVISOIMAGE)pBackendData;
    LogFlowFunc(("pThis=%p -> VD_IMAGE_FLAGS_NONE\n", pThis));
    AssertPtr(pThis); NOREF(pThis);
    return VD_IMAGE_FLAGS_NONE;
}

/**
 * @interface_method_impl{VDIMAGEBACKEND,pfnGetOpenFlags}
 */
static DECLCALLBACK(unsigned) visoGetOpenFlags(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PVISOIMAGE pThis = (PVISOIMAGE)pBackendData;
    AssertPtrReturn(pThis, 0);

    LogFlowFunc(("returns %#x\n", pThis->fOpenFlags));
    return pThis->fOpenFlags;
}

/**
 * @interface_method_impl{VDIMAGEBACKEND,pfnSetOpenFlags}
 */
static DECLCALLBACK(int) visoSetOpenFlags(void *pBackendData, unsigned uOpenFlags)
{
    PVISOIMAGE pThis = (PVISOIMAGE)pBackendData;
    LogFlowFunc(("pThis=%p fOpenFlags=%#x\n", pThis, uOpenFlags));

    AssertPtrReturn(pThis, VERR_INVALID_POINTER);
    uint32_t const fSupported = VD_OPEN_FLAGS_READONLY | VD_OPEN_FLAGS_INFO
                              | VD_OPEN_FLAGS_ASYNC_IO | VD_OPEN_FLAGS_SHAREABLE
                              | VD_OPEN_FLAGS_SEQUENTIAL | VD_OPEN_FLAGS_SKIP_CONSISTENCY_CHECKS;
    AssertMsgReturn(!(uOpenFlags & ~fSupported), ("fOpenFlags=%#x\n", uOpenFlags), VERR_INVALID_FLAGS);

    /*
     * No need to re-open the image, since it's always read-only and we ignore
     * all the other flags.  Just remember them for the getter (visoGetOpenFlags).
     */
    pThis->fOpenFlags &= ~fSupported;
    pThis->fOpenFlags |= fSupported & uOpenFlags;
    pThis->fOpenFlags |= VD_OPEN_FLAGS_READONLY;

    return VINF_SUCCESS;
}

#define uOpenFlags fOpenFlags /* sigh */

/**
 * @interface_method_impl{VDIMAGEBACKEND,pfnGetComment}
 */
VD_BACKEND_CALLBACK_GET_COMMENT_DEF_NOT_SUPPORTED(visoGetComment);

/**
 * @interface_method_impl{VDIMAGEBACKEND,pfnSetComment}
 */
VD_BACKEND_CALLBACK_SET_COMMENT_DEF_NOT_SUPPORTED(visoSetComment, PVISOIMAGE);

/**
 * @interface_method_impl{VDIMAGEBACKEND,pfnGetUuid}
 */
VD_BACKEND_CALLBACK_GET_UUID_DEF_NOT_SUPPORTED(visoGetUuid);

/**
 * @interface_method_impl{VDIMAGEBACKEND,pfnSetUuid}
 */
VD_BACKEND_CALLBACK_SET_UUID_DEF_NOT_SUPPORTED(visoSetUuid, PVISOIMAGE);

/**
 * @interface_method_impl{VDIMAGEBACKEND,pfnGetModificationUuid}
 */
VD_BACKEND_CALLBACK_GET_UUID_DEF_NOT_SUPPORTED(visoGetModificationUuid);

/**
 * @interface_method_impl{VDIMAGEBACKEND,pfnSetModificationUuid}
 */
VD_BACKEND_CALLBACK_SET_UUID_DEF_NOT_SUPPORTED(visoSetModificationUuid, PVISOIMAGE);

/**
 * @interface_method_impl{VDIMAGEBACKEND,pfnGetParentUuid}
 */
VD_BACKEND_CALLBACK_GET_UUID_DEF_NOT_SUPPORTED(visoGetParentUuid);

/**
 * @interface_method_impl{VDIMAGEBACKEND,pfnSetParentUuid}
 */
VD_BACKEND_CALLBACK_SET_UUID_DEF_NOT_SUPPORTED(visoSetParentUuid, PVISOIMAGE);

/**
 * @interface_method_impl{VDIMAGEBACKEND,pfnGetParentModificationUuid}
 */
VD_BACKEND_CALLBACK_GET_UUID_DEF_NOT_SUPPORTED(visoGetParentModificationUuid);

/**
 * @interface_method_impl{VDIMAGEBACKEND,pfnSetParentModificationUuid}
 */
VD_BACKEND_CALLBACK_SET_UUID_DEF_NOT_SUPPORTED(visoSetParentModificationUuid, PVISOIMAGE);

#undef uOpenFlags

/**
 * @interface_method_impl{VDIMAGEBACKEND,pfnDump}
 */
static DECLCALLBACK(void) visoDump(void *pBackendData)
{
    PVISOIMAGE pThis = (PVISOIMAGE)pBackendData;
    AssertPtrReturnVoid(pThis);

    vdIfErrorMessage(pThis->pIfError, "Dumping CUE image '%s' fOpenFlags=%x cbImage=%#RX64\n",
                     pThis->pszFilename, pThis->fOpenFlags, pThis->cbImage);
}



/**
 * VBox ISO maker backend.
 */
const VDIMAGEBACKEND g_VBoxIsoMakerBackend =
{
    /* u32Version */
    VD_IMGBACKEND_VERSION,
    /* pszBackendName */
    "VBoxIsoMaker",
    /* uBackendCaps */
    VD_CAP_FILE,
    /* paFileExtensions */
    g_aVBoXIsoMakerFileExtensions,
    /* paConfigInfo */
    NULL,
    /* pfnProbe */
    visoProbe,
    /* pfnOpen */
    visoOpen,
    /* pfnCreate */
    NULL,
    /* pfnRename */
    NULL,
    /* pfnClose */
    visoClose,
    /* pfnRead */
    visoRead,
    /* pfnWrite */
    visoWrite,
    /* pfnFlush */
    visoFlush,
    /* pfnDiscard */
    NULL,
    /* pfnGetVersion */
    visoGetVersion,
    /* pfnGetFileSize */
    visoGetFileSize,
    /* pfnGetPCHSGeometry */
    visoGetPCHSGeometry,
    /* pfnSetPCHSGeometry */
    visoSetPCHSGeometry,
    /* pfnGetLCHSGeometry */
    visoGetLCHSGeometry,
    /* pfnSetLCHSGeometry */
    visoSetLCHSGeometry,
    /* pfnQueryRegions */
    visoQueryRegions,
    /* pfnRegionListRelease */
    visoRegionListRelease,
    /* pfnGetImageFlags */
    visoGetImageFlags,
    /* pfnGetOpenFlags */
    visoGetOpenFlags,
    /* pfnSetOpenFlags */
    visoSetOpenFlags,
    /* pfnGetComment */
    visoGetComment,
    /* pfnSetComment */
    visoSetComment,
    /* pfnGetUuid */
    visoGetUuid,
    /* pfnSetUuid */
    visoSetUuid,
    /* pfnGetModificationUuid */
    visoGetModificationUuid,
    /* pfnSetModificationUuid */
    visoSetModificationUuid,
    /* pfnGetParentUuid */
    visoGetParentUuid,
    /* pfnSetParentUuid */
    visoSetParentUuid,
    /* pfnGetParentModificationUuid */
    visoGetParentModificationUuid,
    /* pfnSetParentModificationUuid */
    visoSetParentModificationUuid,
    /* pfnDump */
    visoDump,
    /* pfnGetTimestamp */
    NULL,
    /* pfnGetParentTimestamp */
    NULL,
    /* pfnSetParentTimestamp */
    NULL,
    /* pfnGetParentFilename */
    NULL,
    /* pfnSetParentFilename */
    NULL,
    /* pfnComposeLocation */
    genericFileComposeLocation,
    /* pfnComposeName */
    genericFileComposeName,
    /* pfnCompact */
    NULL,
    /* pfnResize */
    NULL,
    /* pfnRepair */
    NULL,
    /* pfnTraverseMetadata */
    NULL,
    /* u32VersionEnd */
    VD_IMGBACKEND_VERSION
};

