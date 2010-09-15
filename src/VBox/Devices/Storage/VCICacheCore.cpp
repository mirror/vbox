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
 * VCI image data structure.
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

/**
 * Internal: signal an informational message to the frontend.
 */
DECLINLINE(int) vciMessage(PVCICACHE pImage, const char *pszFormat, ...)
{
    int rc = VINF_SUCCESS;
    va_list va;
    va_start(va, pszFormat);
    if (pImage->pInterfaceError)
        rc = pImage->pInterfaceErrorCallbacks->pfnMessage(pImage->pInterfaceError->pvUser,
                                                          pszFormat, va);
    va_end(va);
    return rc;
}


DECLINLINE(int) vciFileOpen(PVCICACHE pImage, const char *pszFilename,
                            uint32_t fOpen)
{
    return pImage->pInterfaceIOCallbacks->pfnOpen(pImage->pInterfaceIO->pvUser,
                                                  pszFilename, fOpen,
                                                  &pImage->pStorage);
}

DECLINLINE(int) vciFileClose(PVCICACHE pImage)
{
    return pImage->pInterfaceIOCallbacks->pfnClose(pImage->pInterfaceIO->pvUser,
                                                   pImage->pStorage);
}

DECLINLINE(int) vciFileDelete(PVCICACHE pImage, const char *pszFilename)
{
    return pImage->pInterfaceIOCallbacks->pfnDelete(pImage->pInterfaceIO->pvUser,
                                                    pszFilename);
}

DECLINLINE(int) vciFileMove(PVCICACHE pImage, const char *pszSrc,
                            const char *pszDst, unsigned fMove)
{
    return pImage->pInterfaceIOCallbacks->pfnMove(pImage->pInterfaceIO->pvUser,
                                                  pszSrc, pszDst, fMove);
}

DECLINLINE(int) vciFileGetFreeSpace(PVCICACHE pImage, const char *pszFilename,
                                    int64_t *pcbFree)
{
    return pImage->pInterfaceIOCallbacks->pfnGetFreeSpace(pImage->pInterfaceIO->pvUser,
                                                          pszFilename, pcbFree);
}

DECLINLINE(int) vciFileGetSize(PVCICACHE pImage, uint64_t *pcbSize)
{
    return pImage->pInterfaceIOCallbacks->pfnGetSize(pImage->pInterfaceIO->pvUser,
                                                     pImage->pStorage, pcbSize);
}

DECLINLINE(int) vciFileSetSize(PVCICACHE pImage, uint64_t cbSize)
{
    return pImage->pInterfaceIOCallbacks->pfnSetSize(pImage->pInterfaceIO->pvUser,
                                                     pImage->pStorage, cbSize);
}

DECLINLINE(int) vciFileWriteSync(PVCICACHE pImage, uint64_t uOffset,
                                 const void *pvBuffer, size_t cbBuffer,
                                 size_t *pcbWritten)
{
    return pImage->pInterfaceIOCallbacks->pfnWriteSync(pImage->pInterfaceIO->pvUser,
                                                       pImage->pStorage, uOffset,
                                                       pvBuffer, cbBuffer, pcbWritten);
}

DECLINLINE(int) vciFileReadSync(PVCICACHE pImage, uint64_t uOffset,
                                void *pvBuffer, size_t cbBuffer, size_t *pcbRead)
{
    return pImage->pInterfaceIOCallbacks->pfnReadSync(pImage->pInterfaceIO->pvUser,
                                                      pImage->pStorage, uOffset,
                                                      pvBuffer, cbBuffer, pcbRead);
}

DECLINLINE(int) vciFileFlushSync(PVCICACHE pImage)
{
    return pImage->pInterfaceIOCallbacks->pfnFlushSync(pImage->pInterfaceIO->pvUser,
                                                       pImage->pStorage);
}

DECLINLINE(int) vciFileReadUserAsync(PVCICACHE pImage, uint64_t uOffset,
                                     PVDIOCTX pIoCtx, size_t cbRead)
{
    return pImage->pInterfaceIOCallbacks->pfnReadUserAsync(pImage->pInterfaceIO->pvUser,
                                                           pImage->pStorage,
                                                           uOffset, pIoCtx,
                                                           cbRead);
}

DECLINLINE(int) vciFileWriteUserAsync(PVCICACHE pImage, uint64_t uOffset,
                                      PVDIOCTX pIoCtx, size_t cbWrite,
                                      PFNVDXFERCOMPLETED pfnComplete,
                                      void *pvCompleteUser)
{
    return pImage->pInterfaceIOCallbacks->pfnWriteUserAsync(pImage->pInterfaceIO->pvUser,
                                                            pImage->pStorage,
                                                            uOffset, pIoCtx,
                                                            cbWrite,
                                                            pfnComplete,
                                                            pvCompleteUser);
}

DECLINLINE(int) vciFileFlushAsync(PVCICACHE pImage, PVDIOCTX pIoCtx,
                                  PFNVDXFERCOMPLETED pfnComplete,
                                  void *pvCompleteUser)
{
    return pImage->pInterfaceIOCallbacks->pfnFlushAsync(pImage->pInterfaceIO->pvUser,
                                                        pImage->pStorage,
                                                        pIoCtx, pfnComplete,
                                                        pvCompleteUser);
}


/**
 * Internal. Flush image data to disk.
 */
static int vciFlushImage(PVCICACHE pImage)
{
    int rc = VINF_SUCCESS;

    if (   pImage->pStorage
        && !(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
        rc = vciFileFlushSync(pImage);

    return rc;
}

/**
 * Internal. Free all allocated space for representing an image except pImage,
 * and optionally delete the image from disk.
 */
static int vciFreeImage(PVCICACHE pImage, bool fDelete)
{
    int rc = VINF_SUCCESS;

    /* Freeing a never allocated image (e.g. because the open failed) is
     * not signalled as an error. After all nothing bad happens. */
    if (pImage)
    {
        if (pImage->pStorage)
        {
            /* No point updating the file that is deleted anyway. */
            if (!fDelete)
                vciFlushImage(pImage);

            vciFileClose(pImage);
            pImage->pStorage = NULL;
        }

        if (fDelete && pImage->pszFilename)
            vciFileDelete(pImage, pImage->pszFilename);
    }

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/**
 * Internal: Open an image, constructing all necessary data structures.
 */
static int vciOpenImage(PVCICACHE pImage, unsigned uOpenFlags)
{
    int rc;

    pImage->uOpenFlags = uOpenFlags;

    pImage->pInterfaceError = VDInterfaceGet(pImage->pVDIfsDisk, VDINTERFACETYPE_ERROR);
    if (pImage->pInterfaceError)
        pImage->pInterfaceErrorCallbacks = VDGetInterfaceError(pImage->pInterfaceError);

    /* Get I/O interface. */
    pImage->pInterfaceIO = VDInterfaceGet(pImage->pVDIfsImage, VDINTERFACETYPE_IO);
    AssertPtrReturn(pImage->pInterfaceIO, VERR_INVALID_PARAMETER);
    pImage->pInterfaceIOCallbacks = VDGetInterfaceIO(pImage->pInterfaceIO);
    AssertPtrReturn(pImage->pInterfaceIOCallbacks, VERR_INVALID_PARAMETER);

    /*
     * Open the image.
     */
    rc = vciFileOpen(pImage, pImage->pszFilename,
                     VDOpenFlagsToFileOpenFlags(uOpenFlags,
                                                false /* fCreate */));
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
                          unsigned uOpenFlags, PFNVDPROGRESS pfnProgress,
                          void *pvUser, unsigned uPercentStart,
                          unsigned uPercentSpan)
{
    int rc;
    RTFOFF cbFree = 0;
    uint64_t uOff;
    size_t cbBuf = 128 * _1K;
    void *pvBuf = NULL;

    if (uImageFlags & VD_IMAGE_FLAGS_DIFF)
    {
        rc = vciError(pImage, VERR_VD_RAW_INVALID_TYPE, RT_SRC_POS, N_("VCI: cannot create diff image '%s'"), pImage->pszFilename);
        goto out;
    }
    uImageFlags |= VD_IMAGE_FLAGS_FIXED;

    pImage->uImageFlags = uImageFlags;

    pImage->uOpenFlags = uOpenFlags & ~VD_OPEN_FLAGS_READONLY;

    pImage->pInterfaceError = VDInterfaceGet(pImage->pVDIfsDisk, VDINTERFACETYPE_ERROR);
    if (pImage->pInterfaceError)
        pImage->pInterfaceErrorCallbacks = VDGetInterfaceError(pImage->pInterfaceError);

    /* Get I/O interface. */
    pImage->pInterfaceIO = VDInterfaceGet(pImage->pVDIfsImage, VDINTERFACETYPE_IO);
    AssertPtrReturn(pImage->pInterfaceIO, VERR_INVALID_PARAMETER);
    pImage->pInterfaceIOCallbacks = VDGetInterfaceIO(pImage->pInterfaceIO);
    AssertPtrReturn(pImage->pInterfaceIOCallbacks, VERR_INVALID_PARAMETER);

    /* Create image file. */
    rc = vciFileOpen(pImage, pImage->pszFilename,
                     VDOpenFlagsToFileOpenFlags(uOpenFlags & ~VD_OPEN_FLAGS_READONLY,
                                                true /* fCreate */));
    if (RT_FAILURE(rc))
    {
        rc = vciError(pImage, rc, RT_SRC_POS, N_("VCI: cannot create image '%s'"), pImage->pszFilename);
        goto out;
    }

    /* Check the free space on the disk and leave early if there is not
     * sufficient space available. */
    rc = vciFileGetFreeSpace(pImage, pImage->pszFilename, &cbFree);
    if (RT_SUCCESS(rc) /* ignore errors */ && ((uint64_t)cbFree < cbSize))
    {
        rc = vciError(pImage, VERR_DISK_FULL, RT_SRC_POS, N_("VCI: disk would overflow creating image '%s'"), pImage->pszFilename);
        goto out;
    }

    /* Allocate & commit whole file if fixed image, it must be more
     * effective than expanding file by write operations. */
    rc = vciFileSetSize(pImage, cbSize);
    if (RT_FAILURE(rc))
    {
        rc = vciError(pImage, rc, RT_SRC_POS, N_("VCI: setting image size failed for '%s'"), pImage->pszFilename);
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
            rc = vciError(pImage, rc, RT_SRC_POS, N_("VCI: writing block failed for '%s'"), pImage->pszFilename);
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


/** @copydoc VDCACHEBACKEND::pfnProbe */
static int vciProbe(const char *pszFilename, PVDINTERFACE pVDIfsCache,
                    PVDINTERFACE pVDIfsImage)
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
                   void **ppBackendData)
{
    LogFlowFunc(("pszFilename=\"%s\" uOpenFlags=%#x pVDIfsDisk=%#p pVDIfsImage=%#p ppBackendData=%#p\n", pszFilename, uOpenFlags, pVDIfsDisk, pVDIfsImage, ppBackendData));
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
    pImage->pStorage = NULL;
    pImage->pVDIfsDisk = pVDIfsDisk;
    pImage->pVDIfsImage = pVDIfsImage;

    rc = vciOpenImage(pImage, uOpenFlags);
    if (RT_SUCCESS(rc))
        *ppBackendData = pImage;
    else
        RTMemFree(pImage);

out:
    LogFlowFunc(("returns %Rrc (pBackendData=%#p)\n", rc, *ppBackendData));
    return rc;
}

/** @copydoc VDCACHEBACKEND::pfnCreate */
static int vciCreate(const char *pszFilename, uint64_t cbSize,
                     unsigned uImageFlags, const char *pszComment,
                     PCRTUUID pUuid, unsigned uOpenFlags,
                     unsigned uPercentStart, unsigned uPercentSpan,
                     PVDINTERFACE pVDIfsDisk, PVDINTERFACE pVDIfsImage,
                     PVDINTERFACE pVDIfsOperation, void **ppBackendData)
{
    LogFlowFunc(("pszFilename=\"%s\" cbSize=%llu uImageFlags=%#x pszComment=\"%s\" Uuid=%RTuuid uOpenFlags=%#x uPercentStart=%u uPercentSpan=%u pVDIfsDisk=%#p pVDIfsImage=%#p pVDIfsOperation=%#p ppBackendData=%#p",
                 pszFilename, cbSize, uImageFlags, pszComment, pUuid, uOpenFlags, uPercentStart, uPercentSpan, pVDIfsDisk, pVDIfsImage, pVDIfsOperation, ppBackendData));
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
    pImage->pStorage = NULL;
    pImage->pVDIfsDisk = pVDIfsDisk;
    pImage->pVDIfsImage = pVDIfsImage;

    rc = vciCreateImage(pImage, cbSize, uImageFlags, pszComment, uOpenFlags,
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
        *ppBackendData = pImage;
    }
    else
        RTMemFree(pImage);

out:
    LogFlowFunc(("returns %Rrc (pBackendData=%#p)\n", rc, *ppBackendData));
    return rc;
}

/** @copydoc VDCACHEBACKEND::pfnClose */
static int vciClose(void *pBackendData, bool fDelete)
{
    LogFlowFunc(("pBackendData=%#p fDelete=%d\n", pBackendData, fDelete));
    PVCICACHE pImage = (PVCICACHE)pBackendData;
    int rc;

    rc = vciFreeImage(pImage, fDelete);
    RTMemFree(pImage);

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VDCACHEBACKEND::pfnRead */
static int vciRead(void *pBackendData, uint64_t uOffset, void *pvBuf,
                   size_t cbToRead, size_t *pcbActuallyRead)
{
    LogFlowFunc(("pBackendData=%#p uOffset=%llu pvBuf=%#p cbToRead=%zu pcbActuallyRead=%#p\n", pBackendData, uOffset, pvBuf, cbToRead, pcbActuallyRead));
    PVCICACHE pImage = (PVCICACHE)pBackendData;
    int rc;

    AssertPtr(pImage);
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
static int vciWrite(void *pBackendData, uint64_t uOffset, const void *pvBuf,
                    size_t cbToWrite, size_t *pcbWriteProcess)
{
    LogFlowFunc(("pBackendData=%#p uOffset=%llu pvBuf=%#p cbToWrite=%zu pcbWriteProcess=%#p\n",
                 pBackendData, uOffset, pvBuf, cbToWrite, pcbWriteProcess));
    PVCICACHE pImage = (PVCICACHE)pBackendData;
    int rc;

    AssertPtr(pImage);
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
static int vciFlush(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PVCICACHE pImage = (PVCICACHE)pBackendData;
    int rc;

    rc = vciFlushImage(pImage);
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VDCACHEBACKEND::pfnGetVersion */
static unsigned vciGetVersion(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PVCICACHE pImage = (PVCICACHE)pBackendData;

    AssertPtr(pImage);

    if (pImage)
        return 1;
    else
        return 0;
}

/** @copydoc VDCACHEBACKEND::pfnGetSize */
static uint64_t vciGetSize(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PVCICACHE pImage = (PVCICACHE)pBackendData;
    uint64_t cb = 0;

    AssertPtr(pImage);

    if (pImage && pImage->pStorage)
        cb = pImage->cbSize;

    LogFlowFunc(("returns %llu\n", cb));
    return cb;
}

/** @copydoc VDCACHEBACKEND::pfnGetFileSize */
static uint64_t vciGetFileSize(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PVCICACHE pImage = (PVCICACHE)pBackendData;
    uint64_t cb = 0;

    AssertPtr(pImage);

    if (pImage)
    {
        uint64_t cbFile;
        if (pImage->pStorage)
        {
            int rc = vciFileGetSize(pImage, &cbFile);
            if (RT_SUCCESS(rc))
                cb = cbFile;
        }
    }

    LogFlowFunc(("returns %lld\n", cb));
    return cb;
}

/** @copydoc VDCACHEBACKEND::pfnGetImageFlags */
static unsigned vciGetImageFlags(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PVCICACHE pImage = (PVCICACHE)pBackendData;
    unsigned uImageFlags;

    AssertPtr(pImage);

    if (pImage)
        uImageFlags = pImage->uImageFlags;
    else
        uImageFlags = 0;

    LogFlowFunc(("returns %#x\n", uImageFlags));
    return uImageFlags;
}

/** @copydoc VDCACHEBACKEND::pfnGetOpenFlags */
static unsigned vciGetOpenFlags(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PVCICACHE pImage = (PVCICACHE)pBackendData;
    unsigned uOpenFlags;

    AssertPtr(pImage);

    if (pImage)
        uOpenFlags = pImage->uOpenFlags;
    else
        uOpenFlags = 0;

    LogFlowFunc(("returns %#x\n", uOpenFlags));
    return uOpenFlags;
}

/** @copydoc VDCACHEBACKEND::pfnSetOpenFlags */
static int vciSetOpenFlags(void *pBackendData, unsigned uOpenFlags)
{
    LogFlowFunc(("pBackendData=%#p\n uOpenFlags=%#x", pBackendData, uOpenFlags));
    PVCICACHE pImage = (PVCICACHE)pBackendData;
    int rc;

    /* Image must be opened and the new flags must be valid. Just readonly and
     * info flags are supported. */
    if (!pImage || (uOpenFlags & ~(VD_OPEN_FLAGS_READONLY | VD_OPEN_FLAGS_INFO)))
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    /* Implement this operation via reopening the image. */
    rc = vciFreeImage(pImage, false);
    if (RT_FAILURE(rc))
        goto out;
    rc = vciOpenImage(pImage, uOpenFlags);

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VDCACHEBACKEND::pfnGetComment */
static int vciGetComment(void *pBackendData, char *pszComment,
                          size_t cbComment)
{
    LogFlowFunc(("pBackendData=%#p pszComment=%#p cbComment=%zu\n", pBackendData, pszComment, cbComment));
    PVCICACHE pImage = (PVCICACHE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
        rc = VERR_NOT_SUPPORTED;
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc comment='%s'\n", rc, pszComment));
    return rc;
}

/** @copydoc VDCACHEBACKEND::pfnSetComment */
static int vciSetComment(void *pBackendData, const char *pszComment)
{
    LogFlowFunc(("pBackendData=%#p pszComment=\"%s\"\n", pBackendData, pszComment));
    PVCICACHE pImage = (PVCICACHE)pBackendData;
    int rc;

    AssertPtr(pImage);

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
static int vciGetUuid(void *pBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p pUuid=%#p\n", pBackendData, pUuid));
    PVCICACHE pImage = (PVCICACHE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
        rc = VERR_NOT_SUPPORTED;
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc (%RTuuid)\n", rc, pUuid));
    return rc;
}

/** @copydoc VDCACHEBACKEND::pfnSetUuid */
static int vciSetUuid(void *pBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p Uuid=%RTuuid\n", pBackendData, pUuid));
    PVCICACHE pImage = (PVCICACHE)pBackendData;
    int rc;

    LogFlowFunc(("%RTuuid\n", pUuid));
    AssertPtr(pImage);

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
static int vciGetModificationUuid(void *pBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p pUuid=%#p\n", pBackendData, pUuid));
    PVCICACHE pImage = (PVCICACHE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
        rc = VERR_NOT_SUPPORTED;
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc (%RTuuid)\n", rc, pUuid));
    return rc;
}

/** @copydoc VDCACHEBACKEND::pfnSetModificationUuid */
static int vciSetModificationUuid(void *pBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p Uuid=%RTuuid\n", pBackendData, pUuid));
    PVCICACHE pImage = (PVCICACHE)pBackendData;
    int rc;

    AssertPtr(pImage);

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
static void vciDump(void *pBackendData)
{
    NOREF(pBackendData);
}

/** @copydoc VDCACHEBACKEND::pfnAsyncRead */
static int vciAsyncRead(void *pBackendData, uint64_t uOffset, size_t cbRead,
                        PVDIOCTX pIoCtx, size_t *pcbActuallyRead)
{
    int rc = VINF_SUCCESS;
    PVCICACHE pImage = (PVCICACHE)pBackendData;

    rc = vciFileReadUserAsync(pImage, uOffset, pIoCtx, cbRead);
    if (RT_SUCCESS(rc))
        *pcbActuallyRead = cbRead;

    return rc;
}

/** @copydoc VDCACHEBACKEND::pfnAsyncWrite */
static int vciAsyncWrite(void *pBackendData, uint64_t uOffset, size_t cbWrite,
                         PVDIOCTX pIoCtx, size_t *pcbWriteProcess)
{
    int rc = VINF_SUCCESS;
    PVCICACHE pImage = (PVCICACHE)pBackendData;

    rc = vciFileWriteUserAsync(pImage, uOffset, pIoCtx, cbWrite, NULL, NULL);
    if (RT_SUCCESS(rc))
        *pcbWriteProcess = cbWrite;

    return rc;
}

/** @copydoc VDCACHEBACKEND::pfnAsyncFlush */
static int vciAsyncFlush(void *pBackendData, PVDIOCTX pIoCtx)
{
    int rc = VINF_SUCCESS;
    PVCICACHE pImage = (PVCICACHE)pBackendData;

    if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
        rc = vciFileFlushAsync(pImage, pIoCtx, NULL, NULL);

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

