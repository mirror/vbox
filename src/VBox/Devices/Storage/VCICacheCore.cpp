/* $Id$ */
/** @file
 * VCICacheCore - VirtualBox Cache Image, Core Code.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_VD_RAW /** @todo logging group */
#include <VBox/VBoxHDD-CachePlugin.h>
#include <VBox/err.h>

#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/file.h>


/*******************************************************************************
*   Constants And Macros, Structures and Typedefs                              *
*******************************************************************************/

/**
 * Raw image data structure.
 */
typedef struct VCICACHE
{
    /** Image name. */
    const char       *pszFilename;
    /** Storage handle. */
    PVDIOSTORAGE      pStorage;
    /** I/O interface. */
    PVDINTERFACE      pInterfaceIO;
    /** Async I/O interface callbacks. */
    PVDINTERFACEIO    pInterfaceIOCallbacks;

    /** Pointer to the per-disk VD interface list. */
    PVDINTERFACE      pVDIfsDisk;
    /** Pointer to the per-image VD interface list. */
    PVDINTERFACE      pVDIfsImage;

    /** Error callback. */
    PVDINTERFACE      pInterfaceError;
    /** Opaque data for error callback. */
    PVDINTERFACEERROR pInterfaceErrorCallbacks;

    /** Open flags passed by VBoxHD layer. */
    unsigned          uOpenFlags;
    /** Image flags defined during creation or determined during open. */
    unsigned          uImageFlags;
    /** Total size of the image. */
    uint64_t          cbSize;

} VCICACHE, *PVCICACHE;

/*******************************************************************************
*   Static Variables                                                           *
*******************************************************************************/

/** NULL-terminated array of supported file extensions. */
static const char *const s_apszVciFileExtensions[] =
{
    "vci",
    NULL
};

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/

static int vciFlushImage(PVCICACHE pImage);
static void vciFreeImage(PVCICACHE pImage, bool fDelete);


/**
 * Internal: signal an error to the frontend.
 */
DECLINLINE(int) vciError(PVCICACHE pImage, int rc, RT_SRC_POS_DECL,
                         const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    if (pImage->pInterfaceError)
        pImage->pInterfaceErrorCallbacks->pfnError(pImage->pInterfaceError->pvUser, rc, RT_SRC_POS_ARGS,
                                                   pszFormat, va);
    va_end(va);
    return rc;
}

static int vciFileOpen(PVCICACHE pImage, bool fReadonly, bool fCreate)
{
    int rc = VINF_SUCCESS;

    AssertMsg(!(fReadonly && fCreate), ("Image can't be opened readonly while being created\n"));

    unsigned uOpenFlags = fReadonly ? VD_INTERFACEASYNCIO_OPEN_FLAGS_READONLY : 0;

    if (fCreate)
        uOpenFlags |= VD_INTERFACEASYNCIO_OPEN_FLAGS_CREATE;

    rc = pImage->pInterfaceIOCallbacks->pfnOpen(pImage->pInterfaceIO->pvUser,
                                                pImage->pszFilename,
                                                uOpenFlags,
                                                &pImage->pStorage);

    return rc;
}

static int vciFileClose(PVCICACHE pImage)
{
    int rc = VINF_SUCCESS;

    if (pImage->pStorage)
        rc = pImage->pInterfaceIOCallbacks->pfnClose(pImage->pInterfaceIO->pvUser,
                                                     pImage->pStorage);

    pImage->pStorage = NULL;

    return rc;
}

static int vciFileFlushSync(PVCICACHE pImage)
{
    return pImage->pInterfaceIOCallbacks->pfnFlushSync(pImage->pInterfaceIO->pvUser,
                                                       pImage->pStorage);
}

static int vciFileGetSize(PVCICACHE pImage, uint64_t *pcbSize)
{
    return pImage->pInterfaceIOCallbacks->pfnGetSize(pImage->pInterfaceIO->pvUser,
                                                     pImage->pStorage,
                                                     pcbSize);
}

static int vciFileSetSize(PVCICACHE pImage, uint64_t cbSize)
{
    return pImage->pInterfaceIOCallbacks->pfnSetSize(pImage->pInterfaceIO->pvUser,
                                                     pImage->pStorage,
                                                     cbSize);
}


static int vciFileWriteSync(PVCICACHE pImage, uint64_t off, const void *pcvBuf, size_t cbWrite, size_t *pcbWritten)
{
    return pImage->pInterfaceIOCallbacks->pfnWriteSync(pImage->pInterfaceIO->pvUser,
                                                       pImage->pStorage,
                                                       off, cbWrite, pcvBuf,
                                                       pcbWritten);
}

static int vciFileReadSync(PVCICACHE pImage, uint64_t off, void *pvBuf, size_t cbRead, size_t *pcbRead)
{
    return pImage->pInterfaceIOCallbacks->pfnReadSync(pImage->pInterfaceIO->pvUser,
                                                      pImage->pStorage,
                                                      off, cbRead, pvBuf,
                                                      pcbRead);
}

static bool vciFileOpened(PVCICACHE pImage)
{
    return pImage->pStorage != NULL;
}

/**
 * Internal: Open an image, constructing all necessary data structures.
 */
static int vciOpenImage(PVCICACHE pImage, unsigned uOpenFlags)
{
    int rc;

    if (uOpenFlags & VD_OPEN_FLAGS_ASYNC_IO)
        return VERR_NOT_SUPPORTED;

    pImage->uOpenFlags = uOpenFlags;

    pImage->pInterfaceError = VDInterfaceGet(pImage->pVDIfsDisk, VDINTERFACETYPE_ERROR);
    if (pImage->pInterfaceError)
        pImage->pInterfaceErrorCallbacks = VDGetInterfaceError(pImage->pInterfaceError);

#ifdef VBOX_WITH_NEW_IO_CODE
    /* Try to get I/O interface. */
    pImage->pInterfaceIO = VDInterfaceGet(pImage->pVDIfsImage, VDINTERFACETYPE_IO);
    AssertPtr(pImage->pInterfaceIO);
    pImage->pInterfaceIOCallbacks = VDGetInterfaceIO(pImage->pInterfaceIO);
    AssertPtr(pImage->pInterfaceIOCallbacks);
#endif

    /*
     * Open the image.
     */
    rc = vciFileOpen(pImage, !!(uOpenFlags & VD_OPEN_FLAGS_READONLY), false);
    if (RT_FAILURE(rc))
    {
        /* Do NOT signal an appropriate error here, as the VD layer has the
         * choice of retrying the open if it failed. */
        goto out;
    }

    rc = vciFileGetSize(pImage, &pImage->cbSize);
    if (RT_FAILURE(rc))
        goto out;
    if (pImage->cbSize % 512)
    {
        rc = VERR_VD_RAW_INVALID_HEADER;
        goto out;
    }
    pImage->uImageFlags |= VD_IMAGE_FLAGS_FIXED;

out:
    if (RT_FAILURE(rc))
        vciFreeImage(pImage, false);
    return rc;
}

/**
 * Internal: Create a vci image.
 */
static int vciCreateImage(PVCICACHE pImage, uint64_t cbSize,
                          unsigned uImageFlags, const char *pszComment,
                          PFNVDPROGRESS pfnProgress, void *pvUser,
                          unsigned uPercentStart, unsigned uPercentSpan)
{
    int rc;
    RTFOFF cbFree = 0;
    uint64_t uOff;
    size_t cbBuf = 128 * _1K;
    void *pvBuf = NULL;

    if (uImageFlags & VD_IMAGE_FLAGS_DIFF)
    {
        rc = vciError(pImage, VERR_VD_RAW_INVALID_TYPE, RT_SRC_POS, N_("Raw: cannot create diff image '%s'"), pImage->pszFilename);
        goto out;
    }
    uImageFlags |= VD_IMAGE_FLAGS_FIXED;

    pImage->uImageFlags = uImageFlags;

    pImage->pInterfaceError = VDInterfaceGet(pImage->pVDIfsDisk, VDINTERFACETYPE_ERROR);
    if (pImage->pInterfaceError)
        pImage->pInterfaceErrorCallbacks = VDGetInterfaceError(pImage->pInterfaceError);

#ifdef VBOX_WITH_NEW_IO_CODE
    /* Try to get async I/O interface. */
    pImage->pInterfaceIO = VDInterfaceGet(pImage->pVDIfsImage, VDINTERFACETYPE_IO);
    AssertPtr(pImage->pInterfaceIO);
    pImage->pInterfaceIOCallbacks = VDGetInterfaceIO(pImage->pInterfaceIO);
    AssertPtr(pImage->pInterfaceIOCallbacks);
#endif

    /* Create image file. */
    rc = vciFileOpen(pImage, false, true);
    if (RT_FAILURE(rc))
    {
        rc = vciError(pImage, rc, RT_SRC_POS, N_("Raw: cannot create image '%s'"), pImage->pszFilename);
        goto out;
    }

    /* Check the free space on the disk and leave early if there is not
     * sufficient space available. */
    rc = RTFsQuerySizes(pImage->pszFilename, NULL, &cbFree, NULL, NULL);
    if (RT_SUCCESS(rc) /* ignore errors */ && ((uint64_t)cbFree < cbSize))
    {
        rc = vciError(pImage, VERR_DISK_FULL, RT_SRC_POS, N_("Raw: disk would overflow creating image '%s'"), pImage->pszFilename);
        goto out;
    }

    /* Allocate & commit whole file if fixed image, it must be more
     * effective than expanding file by write operations. */
    rc = vciFileSetSize(pImage, cbSize);
    if (RT_FAILURE(rc))
    {
        rc = vciError(pImage, rc, RT_SRC_POS, N_("Raw: setting image size failed for '%s'"), pImage->pszFilename);
        goto out;
    }

    /* Fill image with zeroes. We do this for every fixed-size image since on
     * some systems (for example Windows Vista), it takes ages to write a block
     * near the end of a sparse file and the guest could complain about an ATA
     * timeout. */
    pvBuf = RTMemTmpAllocZ(cbBuf);
    if (!pvBuf)
    {
        rc = VERR_NO_MEMORY;
        goto out;
    }

    uOff = 0;
    /* Write data to all image blocks. */
    while (uOff < cbSize)
    {
        unsigned cbChunk = (unsigned)RT_MIN(cbSize, cbBuf);

        rc = vciFileWriteSync(pImage, uOff, pvBuf, cbChunk, NULL);
        if (RT_FAILURE(rc))
        {
            rc = vciError(pImage, rc, RT_SRC_POS, N_("Raw: writing block failed for '%s'"), pImage->pszFilename);
            goto out;
        }

        uOff += cbChunk;

        if (pfnProgress)
        {
            rc = pfnProgress(pvUser,
                             uPercentStart + uOff * uPercentSpan * 98 / (cbSize * 100));
            if (RT_FAILURE(rc))
                goto out;
        }
    }
    RTMemTmpFree(pvBuf);

    if (RT_SUCCESS(rc) && pfnProgress)
        pfnProgress(pvUser, uPercentStart + uPercentSpan * 98 / 100);

    pImage->cbSize = cbSize;

    rc = vciFlushImage(pImage);

out:
    if (RT_SUCCESS(rc) && pfnProgress)
        pfnProgress(pvUser, uPercentStart + uPercentSpan);

    if (RT_FAILURE(rc))
        vciFreeImage(pImage, rc != VERR_ALREADY_EXISTS);
    return rc;
}

/**
 * Internal. Free all allocated space for representing an image, and optionally
 * delete the image from disk.
 */
static void vciFreeImage(PVCICACHE pImage, bool fDelete)
{
    Assert(pImage);

    if (vciFileOpened(pImage))
    {
        if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
            vciFlushImage(pImage);
        vciFileClose(pImage);
    }
    if (fDelete && pImage->pszFilename)
        RTFileDelete(pImage->pszFilename);
}

/**
 * Internal. Flush image data to disk.
 */
static int vciFlushImage(PVCICACHE pImage)
{
    int rc = VINF_SUCCESS;

    if (   vciFileOpened(pImage)
        && !(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
        rc = vciFileFlushSync(pImage);

    return rc;
}


/** @copydoc VDCACHEBACKEND::pfnProbe */
static int vciProbe(const char *pszFilename, PVDINTERFACE pVDIfsDisk)
{
    LogFlowFunc(("pszFilename=\"%s\"\n", pszFilename));
    int rc = VINF_SUCCESS;

    if (   !VALID_PTR(pszFilename)
        || !*pszFilename)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    /* Always return failure, to avoid opening everything as a vci image. */
    rc = VERR_VD_RAW_INVALID_HEADER;

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VDCACHEBACKEND::pfnOpen */
static int vciOpen(const char *pszFilename, unsigned uOpenFlags,
                   PVDINTERFACE pVDIfsDisk, PVDINTERFACE pVDIfsImage,
                   void **ppvBackendData)
{
    LogFlowFunc(("pszFilename=\"%s\" uOpenFlags=%#x pVDIfsDisk=%#p pVDIfsImage=%#p ppvBackendData=%#p\n", pszFilename, uOpenFlags, pVDIfsDisk, pVDIfsImage, ppvBackendData));
    int rc;
    PVCICACHE pImage;

    /* Check open flags. All valid flags are supported. */
    if (uOpenFlags & ~VD_OPEN_FLAGS_MASK)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    /* Check remaining arguments. */
    if (   !VALID_PTR(pszFilename)
        || !*pszFilename)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }


    pImage = (PVCICACHE)RTMemAllocZ(sizeof(VCICACHE));
    if (!pImage)
    {
        rc = VERR_NO_MEMORY;
        goto out;
    }
    pImage->pszFilename = pszFilename;
    pImage->pStorage    = NULL;
    pImage->pVDIfsDisk  = pVDIfsDisk;
    pImage->pVDIfsImage = pVDIfsImage;

    rc = vciOpenImage(pImage, uOpenFlags);
    if (RT_SUCCESS(rc))
        *ppvBackendData = pImage;
    else
        RTMemFree(pImage);

out:
    LogFlowFunc(("returns %Rrc (pvBackendData=%#p)\n", rc, *ppvBackendData));
    return rc;
}

/** @copydoc VDCACHEBACKEND::pfnCreate */
static int vciCreate(const char *pszFilename, uint64_t cbSize,
                     unsigned uImageFlags, const char *pszComment,
                     PCRTUUID pUuid,
                     unsigned uOpenFlags, unsigned uPercentStart,
                     unsigned uPercentSpan, PVDINTERFACE pVDIfsDisk,
                     PVDINTERFACE pVDIfsImage, PVDINTERFACE pVDIfsOperation,
                     void **ppvBackendData)
{
    LogFlowFunc(("pszFilename=\"%s\" cbSize=%llu uImageFlags=%#x pszComment=\"%s\" Uuid=%RTuuid uOpenFlags=%#x uPercentStart=%u uPercentSpan=%u pVDIfsDisk=%#p pVDIfsImage=%#p pVDIfsOperation=%#p ppvBackendData=%#p",
                 pszFilename, cbSize, uImageFlags, pszComment, pUuid, uOpenFlags, uPercentStart, uPercentSpan, pVDIfsDisk, pVDIfsImage, pVDIfsOperation, ppvBackendData));
    int rc;
    PVCICACHE pImage;

    PFNVDPROGRESS pfnProgress = NULL;
    void *pvUser = NULL;
    PVDINTERFACE pIfProgress = VDInterfaceGet(pVDIfsOperation,
                                              VDINTERFACETYPE_PROGRESS);
    PVDINTERFACEPROGRESS pCbProgress = NULL;
    if (pIfProgress)
    {
        pCbProgress = VDGetInterfaceProgress(pIfProgress);
        if (pCbProgress)
            pfnProgress = pCbProgress->pfnProgress;
        pvUser = pIfProgress->pvUser;
    }

    /* Check open flags. All valid flags are supported. */
    if (uOpenFlags & ~VD_OPEN_FLAGS_MASK)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    /* Check remaining arguments. */
    if (   !VALID_PTR(pszFilename)
        || !*pszFilename)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    pImage = (PVCICACHE)RTMemAllocZ(sizeof(VCICACHE));
    if (!pImage)
    {
        rc = VERR_NO_MEMORY;
        goto out;
    }
    pImage->pszFilename = pszFilename;
    pImage->pStorage    = NULL;
    pImage->pVDIfsDisk  = pVDIfsDisk;
    pImage->pVDIfsImage = pVDIfsImage;

    rc = vciCreateImage(pImage, cbSize, uImageFlags, pszComment,
                        pfnProgress, pvUser, uPercentStart, uPercentSpan);
    if (RT_SUCCESS(rc))
    {
        /* So far the image is opened in read/write mode. Make sure the
         * image is opened in read-only mode if the caller requested that. */
        if (uOpenFlags & VD_OPEN_FLAGS_READONLY)
        {
            vciFreeImage(pImage, false);
            rc = vciOpenImage(pImage, uOpenFlags);
            if (RT_FAILURE(rc))
            {
                RTMemFree(pImage);
                goto out;
            }
        }
        *ppvBackendData = pImage;
    }
    else
        RTMemFree(pImage);

out:
    LogFlowFunc(("returns %Rrc (pvBackendData=%#p)\n", rc, *ppvBackendData));
    return rc;
}

/** @copydoc VDCACHEBACKEND::pfnClose */
static int vciClose(void *pvBackendData, bool fDelete)
{
    LogFlowFunc(("pvBackendData=%#p fDelete=%d\n", pvBackendData, fDelete));
    PVCICACHE pImage = (PVCICACHE)pvBackendData;
    int rc = VINF_SUCCESS;

    /* Freeing a never allocated image (e.g. because the open failed) is
     * not signalled as an error. After all nothing bad happens. */
    if (pImage)
    {
        vciFreeImage(pImage, fDelete);
        RTMemFree(pImage);
    }

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VDCACHEBACKEND::pfnRead */
static int vciRead(void *pvBackendData, uint64_t uOffset, void *pvBuf,
                   size_t cbToRead, size_t *pcbActuallyRead)
{
    LogFlowFunc(("pvBackendData=%#p uOffset=%llu pvBuf=%#p cbToRead=%zu pcbActuallyRead=%#p\n", pvBackendData, uOffset, pvBuf, cbToRead, pcbActuallyRead));
    PVCICACHE pImage = (PVCICACHE)pvBackendData;
    int rc;

    Assert(pImage);
    Assert(uOffset % 512 == 0);
    Assert(cbToRead % 512 == 0);

    if (   uOffset + cbToRead > pImage->cbSize
        || cbToRead == 0)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    rc = vciFileReadSync(pImage, uOffset, pvBuf, cbToRead, NULL);
    *pcbActuallyRead = cbToRead;

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VDCACHEBACKEND::pfnWrite */
static int vciWrite(void *pvBackendData, uint64_t uOffset, const void *pvBuf,
                    size_t cbToWrite, size_t *pcbWriteProcess)
{
    LogFlowFunc(("pvBackendData=%#p uOffset=%llu pvBuf=%#p cbToWrite=%zu pcbWriteProcess=%#p\n",
                 pvBackendData, uOffset, pvBuf, cbToWrite, pcbWriteProcess));
    PVCICACHE pImage = (PVCICACHE)pvBackendData;
    int rc;

    Assert(pImage);
    Assert(uOffset % 512 == 0);
    Assert(cbToWrite % 512 == 0);

    if (pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
    {
        rc = VERR_VD_IMAGE_READ_ONLY;
        goto out;
    }

    if (   uOffset + cbToWrite > pImage->cbSize
        || cbToWrite == 0)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    rc = vciFileWriteSync(pImage, uOffset, pvBuf, cbToWrite, NULL);
    if (pcbWriteProcess)
        *pcbWriteProcess = cbToWrite;

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VDCACHEBACKEND::pfnFlush */
static int vciFlush(void *pvBackendData)
{
    LogFlowFunc(("pvBackendData=%#p\n", pvBackendData));
    PVCICACHE pImage = (PVCICACHE)pvBackendData;
    int rc;

    rc = vciFlushImage(pImage);
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VDCACHEBACKEND::pfnGetVersion */
static unsigned vciGetVersion(void *pvBackendData)
{
    LogFlowFunc(("pvBackendData=%#p\n", pvBackendData));
    PVCICACHE pImage = (PVCICACHE)pvBackendData;

    Assert(pImage);

    if (pImage)
        return 1;
    else
        return 0;
}

/** @copydoc VDCACHEBACKEND::pfnGetSize */
static uint64_t vciGetSize(void *pvBackendData)
{
    LogFlowFunc(("pvBackendData=%#p\n", pvBackendData));
    PVCICACHE pImage = (PVCICACHE)pvBackendData;

    Assert(pImage);

    if (pImage)
        return pImage->cbSize;
    else
        return 0;
}

/** @copydoc VDCACHEBACKEND::pfnGetFileSize */
static uint64_t vciGetFileSize(void *pvBackendData)
{
    LogFlowFunc(("pvBackendData=%#p\n", pvBackendData));
    PVCICACHE pImage = (PVCICACHE)pvBackendData;
    uint64_t cb = 0;

    Assert(pImage);

    if (pImage)
    {
        uint64_t cbFile;
        if (vciFileOpened(pImage))
        {
            int rc = vciFileGetSize(pImage, &cbFile);
            if (RT_SUCCESS(rc))
                cb += cbFile;
        }
    }

    LogFlowFunc(("returns %lld\n", cb));
    return cb;
}

/** @copydoc VDCACHEBACKEND::pfnGetImageFlags */
static unsigned vciGetImageFlags(void *pvBackendData)
{
    LogFlowFunc(("pvBackendData=%#p\n", pvBackendData));
    PVCICACHE pImage = (PVCICACHE)pvBackendData;
    unsigned uImageFlags;

    Assert(pImage);

    if (pImage)
        uImageFlags = pImage->uImageFlags;
    else
        uImageFlags = 0;

    LogFlowFunc(("returns %#x\n", uImageFlags));
    return uImageFlags;
}

/** @copydoc VDCACHEBACKEND::pfnGetOpenFlags */
static unsigned vciGetOpenFlags(void *pvBackendData)
{
    LogFlowFunc(("pvBackendData=%#p\n", pvBackendData));
    PVCICACHE pImage = (PVCICACHE)pvBackendData;
    unsigned uOpenFlags;

    Assert(pImage);

    if (pImage)
        uOpenFlags = pImage->uOpenFlags;
    else
        uOpenFlags = 0;

    LogFlowFunc(("returns %#x\n", uOpenFlags));
    return uOpenFlags;
}

/** @copydoc VDCACHEBACKEND::pfnSetOpenFlags */
static int vciSetOpenFlags(void *pvBackendData, unsigned uOpenFlags)
{
    LogFlowFunc(("pvBackendData=%#p\n uOpenFlags=%#x", pvBackendData, uOpenFlags));
    PVCICACHE pImage = (PVCICACHE)pvBackendData;
    int rc;

    /* Image must be opened and the new flags must be valid. Just readonly and
     * info flags are supported. */
    if (!pImage || (uOpenFlags & ~(VD_OPEN_FLAGS_READONLY | VD_OPEN_FLAGS_INFO)))
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    /* Implement this operation via reopening the image. */
    vciFreeImage(pImage, false);
    rc = vciOpenImage(pImage, uOpenFlags);

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VDCACHEBACKEND::pfnGetComment */
static int vciGetComment(void *pvBackendData, char *pszComment,
                          size_t cbComment)
{
    LogFlowFunc(("pvBackendData=%#p pszComment=%#p cbComment=%zu\n", pvBackendData, pszComment, cbComment));
    PVCICACHE pImage = (PVCICACHE)pvBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
    {
        if (pszComment)
            *pszComment = '\0';
        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc comment='%s'\n", rc, pszComment));
    return rc;
}

/** @copydoc VDCACHEBACKEND::pfnSetComment */
static int vciSetComment(void *pvBackendData, const char *pszComment)
{
    LogFlowFunc(("pvBackendData=%#p pszComment=\"%s\"\n", pvBackendData, pszComment));
    PVCICACHE pImage = (PVCICACHE)pvBackendData;
    int rc;

    Assert(pImage);

    if (pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
    {
        rc = VERR_VD_IMAGE_READ_ONLY;
        goto out;
    }

    if (pImage)
        rc = VERR_NOT_SUPPORTED;
    else
        rc = VERR_VD_NOT_OPENED;

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VDCACHEBACKEND::pfnGetUuid */
static int vciGetUuid(void *pvBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pvBackendData=%#p pUuid=%#p\n", pvBackendData, pUuid));
    PVCICACHE pImage = (PVCICACHE)pvBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
        rc = VERR_NOT_SUPPORTED;
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc (%RTuuid)\n", rc, pUuid));
    return rc;
}

/** @copydoc VDCACHEBACKEND::pfnSetUuid */
static int vciSetUuid(void *pvBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pvBackendData=%#p Uuid=%RTuuid\n", pvBackendData, pUuid));
    PVCICACHE pImage = (PVCICACHE)pvBackendData;
    int rc;

    LogFlowFunc(("%RTuuid\n", pUuid));
    Assert(pImage);

    if (pImage)
    {
        if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
            rc = VERR_NOT_SUPPORTED;
        else
            rc = VERR_VD_IMAGE_READ_ONLY;
    }
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VDCACHEBACKEND::pfnGetModificationUuid */
static int vciGetModificationUuid(void *pvBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pvBackendData=%#p pUuid=%#p\n", pvBackendData, pUuid));
    PVCICACHE pImage = (PVCICACHE)pvBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
        rc = VERR_NOT_SUPPORTED;
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc (%RTuuid)\n", rc, pUuid));
    return rc;
}

/** @copydoc VDCACHEBACKEND::pfnSetModificationUuid */
static int vciSetModificationUuid(void *pvBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pvBackendData=%#p Uuid=%RTuuid\n", pvBackendData, pUuid));
    PVCICACHE pImage = (PVCICACHE)pvBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
    {
        if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
            rc = VERR_NOT_SUPPORTED;
        else
            rc = VERR_VD_IMAGE_READ_ONLY;
    }
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VDCACHEBACKEND::pfnDump */
static void vciDump(void *pvBackendData)
{
    NOREF(pvBackendData);
}

static int vciAsyncRead(void *pvBackendData, uint64_t uOffset, size_t cbRead,
                        PVDIOCTX pIoCtx, size_t *pcbActuallyRead)
{
    int rc = VINF_SUCCESS;
    PVCICACHE pImage = (PVCICACHE)pvBackendData;

    rc = pImage->pInterfaceIOCallbacks->pfnReadUserAsync(pImage->pInterfaceIO->pvUser,
                                                         pImage->pStorage,
                                                         uOffset, pIoCtx, cbRead);
    if (RT_SUCCESS(rc))
        *pcbActuallyRead = cbRead;

    return rc;
}

static int vciAsyncWrite(void *pvBackendData, uint64_t uOffset, size_t cbWrite,
                         PVDIOCTX pIoCtx, size_t *pcbWriteProcess)
{
    int rc = VINF_SUCCESS;
    PVCICACHE pImage = (PVCICACHE)pvBackendData;

    rc = pImage->pInterfaceIOCallbacks->pfnWriteUserAsync(pImage->pInterfaceIO->pvUser,
                                                         pImage->pStorage,
                                                         uOffset, pIoCtx, cbWrite,
                                                         NULL, NULL);

    if (RT_SUCCESS(rc))
        *pcbWriteProcess = cbWrite;

    return rc;
}

static int vciAsyncFlush(void *pvBackendData, PVDIOCTX pIoCtx)
{
    int rc = VERR_NOT_IMPLEMENTED;
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

VDCACHEBACKEND g_VciCacheBackend =
{
    /* pszBackendName */
    "vci",
    /* cbSize */
    sizeof(VDCACHEBACKEND),
    /* uBackendCaps */
    VD_CAP_CREATE_FIXED | VD_CAP_CREATE_DYNAMIC | VD_CAP_FILE | VD_CAP_ASYNC,
    /* papszFileExtensions */
    s_apszVciFileExtensions,
    /* paConfigInfo */
    NULL,
    /* hPlugin */
    NIL_RTLDRMOD,
    /* pfnProbe */
    vciProbe,
    /* pfnOpen */
    vciOpen,
    /* pfnCreate */
    vciCreate,
    /* pfnClose */
    vciClose,
    /* pfnRead */
    vciRead,
    /* pfnWrite */
    vciWrite,
    /* pfnFlush */
    vciFlush,
    /* pfnGetVersion */
    vciGetVersion,
    /* pfnGetSize */
    vciGetSize,
    /* pfnGetFileSize */
    vciGetFileSize,
    /* pfnGetImageFlags */
    vciGetImageFlags,
    /* pfnGetOpenFlags */
    vciGetOpenFlags,
    /* pfnSetOpenFlags */
    vciSetOpenFlags,
    /* pfnGetComment */
    vciGetComment,
    /* pfnSetComment */
    vciSetComment,
    /* pfnGetUuid */
    vciGetUuid,
    /* pfnSetUuid */
    vciSetUuid,
    /* pfnGetModificationUuid */
    vciGetModificationUuid,
    /* pfnSetModificationUuid */
    vciSetModificationUuid,
    /* pfnDump */
    vciDump,
    /* pfnAsyncRead */
    vciAsyncRead,
    /* pfnAsyncWrite */
    vciAsyncWrite,
    /* pfnAsyncFlush */
    vciAsyncFlush,
    /* pfnComposeLocation */
    NULL,
    /* pfnComposeName */
    NULL
};

