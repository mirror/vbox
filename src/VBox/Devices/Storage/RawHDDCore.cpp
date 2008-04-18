/** $Id$ */
/** @file
 * Raw Disk image, Core Code.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_VD_RAW
#include "VBoxHDD-newInternal.h"
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
typedef struct RAWIMAGE
{
    /** Base image name. */
    const char      *pszFilename;
    /** File descriptor. */
    RTFILE          File;

    /** Error callback. */
    PFNVDERROR      pfnError;
    /** Opaque data for error callback. */
    void            *pvErrorUser;

    /** Open flags passed by VBoxHD layer. */
    unsigned        uOpenFlags;
    /** Image type. */
    VDIMAGETYPE     enmImageType;
    /** Image flags defined during creation or determined during open. */
    unsigned        uImageFlags;
    /** Total size of the image. */
    uint64_t        cbSize;
    /** Physical geometry of this image. */
    PDMMEDIAGEOMETRY PCHSGeometry;
    /** Logical geometry of this image. */
    PDMMEDIAGEOMETRY LCHSGeometry;

} RAWIMAGE, *PRAWIMAGE;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/

static int rawFlushImage(PRAWIMAGE pImage);
static void rawFreeImage(PRAWIMAGE pImage, bool fDelete);


/**
 * Internal: signal an error to the frontend.
 */
DECLINLINE(int) rawError(PRAWIMAGE pImage, int rc, RT_SRC_POS_DECL,
                         const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    if (pImage->pfnError)
        pImage->pfnError(pImage->pvErrorUser, rc, RT_SRC_POS_ARGS,
                         pszFormat, va);
    va_end(va);
    return rc;
}

/**
 * Internal: Open an image, constructing all necessary data structures.
 */
static int rawOpenImage(PRAWIMAGE pImage, unsigned uOpenFlags)
{
    int rc;
    RTFILE File;

    pImage->uOpenFlags = uOpenFlags;

    /*
     * Open the image.
     */
    rc = RTFileOpen(&File, pImage->pszFilename,
                    uOpenFlags & VD_OPEN_FLAGS_READONLY
                     ? RTFILE_O_READ      | RTFILE_O_OPEN | RTFILE_O_DENY_NONE
                     : RTFILE_O_READWRITE | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE);
    if (VBOX_FAILURE(rc))
    {
        /* Do NOT signal an appropriate error here, as the VD layer has the
         * choice of retrying the open if it failed. */
        goto out;
    }
    pImage->File = File;

    rc = RTFileGetSize(pImage->File, &pImage->cbSize);
    if (VBOX_FAILURE(rc))
        goto out;
    if (pImage->cbSize % 512)
    {
        rc = VERR_VDI_INVALID_HEADER;
        goto out;
    }
    pImage->enmImageType = VD_IMAGE_TYPE_FIXED;

out:
    if (VBOX_FAILURE(rc))
        rawFreeImage(pImage, false);
    return rc;
}

/**
 * Internal: Create a raw image.
 */
static int rawCreateImage(PRAWIMAGE pImage, VDIMAGETYPE enmType,
                          uint64_t cbSize, unsigned uImageFlags,
                          const char *pszComment,
                          PCPDMMEDIAGEOMETRY pPCHSGeometry,
                          PCPDMMEDIAGEOMETRY pLCHSGeometry,
                          PFNVMPROGRESS pfnProgress, void *pvUser,
                          unsigned uPercentStart, unsigned uPercentSpan)
{
    int rc;
    RTFILE File;
    RTFOFF cbFree = 0;
    uint64_t uOff;
    size_t cbBuf = 128 * _1K;
    void *pvBuf = NULL;

    if (enmType != VD_IMAGE_TYPE_FIXED)
    {
        rc = rawError(pImage, VERR_VDI_INVALID_TYPE, RT_SRC_POS, N_("Raw: cannot create diff image '%s'"), pImage->pszFilename);
        goto out;
    }

    pImage->enmImageType = enmType;
    pImage->uImageFlags = uImageFlags;
    pImage->PCHSGeometry = *pPCHSGeometry;
    pImage->LCHSGeometry = *pLCHSGeometry;

    /* Create image file. */
    rc = RTFileOpen(&File, pImage->pszFilename,
                    RTFILE_O_READWRITE | RTFILE_O_CREATE | RTFILE_O_DENY_ALL);
    if (VBOX_FAILURE(rc))
    {
        rc = rawError(pImage, rc, RT_SRC_POS, N_("Raw: cannot create image '%s'"), pImage->pszFilename);
        goto out;
    }
    pImage->File = File;

    /* Check the free space on the disk and leave early if there is not
     * sufficient space available. */
    rc = RTFsQuerySizes(pImage->pszFilename, NULL, &cbFree, NULL, NULL);
    if (VBOX_SUCCESS(rc) /* ignore errors */ && ((uint64_t)cbFree < cbSize))
    {
        rc = rawError(pImage, VERR_DISK_FULL, RT_SRC_POS, N_("Raw: disk would overflow creating image '%s'"), pImage->pszFilename);
        goto out;
    }

    /* Allocate & commit whole file if fixed image, it must be more
     * effective than expanding file by write operations. */
    rc = RTFileSetSize(File, cbSize);
    if (VBOX_FAILURE(rc))
    {
        rc = rawError(pImage, rc, RT_SRC_POS, N_("Raw: setting image size failed for '%s'"), pImage->pszFilename);
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

        rc = RTFileWriteAt(File, uOff, pvBuf, cbChunk, NULL);
        if (VBOX_FAILURE(rc))
        {
            rc = rawError(pImage, rc, RT_SRC_POS, N_("Raw: writing block failed for '%s'"), pImage->pszFilename);
            goto out;
        }

        uOff += cbChunk;

        if (pfnProgress)
        {
            rc = pfnProgress(NULL /* WARNING! pVM=NULL  */,
                             uPercentStart + uOff * uPercentSpan * 98 / (cbSize * 100),
                             pvUser);
            if (VBOX_FAILURE(rc))
                goto out;
        }
    }
    RTMemTmpFree(pvBuf);

    if (VBOX_SUCCESS(rc) && pfnProgress)
        pfnProgress(NULL /* WARNING! pVM=NULL  */,
                    uPercentStart + uPercentSpan * 98 / 100, pvUser);

    pImage->enmImageType = enmType;
    pImage->cbSize = cbSize;

    rc = rawFlushImage(pImage);

out:
    if (VBOX_SUCCESS(rc) && pfnProgress)
        pfnProgress(NULL /* WARNING! pVM=NULL  */,
                    uPercentStart + uPercentSpan, pvUser);

    if (VBOX_FAILURE(rc))
        rawFreeImage(pImage, rc != VERR_ALREADY_EXISTS);
    return rc;
}

/**
 * Internal. Free all allocated space for representing an image, and optionally
 * delete the image from disk.
 */
static void rawFreeImage(PRAWIMAGE pImage, bool fDelete)
{
    Assert(pImage);

    if (pImage->enmImageType)
    {
        if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
            rawFlushImage(pImage);
    }
    if (pImage->File != NIL_RTFILE)
    {
        RTFileClose(pImage->File);
        pImage->File = NIL_RTFILE;
    }
    if (fDelete && pImage->pszFilename)
        RTFileDelete(pImage->pszFilename);
}

/**
 * Internal. Flush image data to disk.
 */
static int rawFlushImage(PRAWIMAGE pImage)
{
    int rc = VINF_SUCCESS;

    if (   pImage->File != NIL_RTFILE
        && !(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
        rc = RTFileFlush(pImage->File);

    return rc;
}


/** @copydoc VBOXHDDBACKEND::pfnCheckIfValid */
static int rawCheckIfValid(const char *pszFilename)
{
    LogFlowFunc(("pszFilename=\"%s\"\n", pszFilename));
    int rc = VINF_SUCCESS;

    if (   !VALID_PTR(pszFilename)
        || !*pszFilename)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    /* Always return failure, to avoid opening everything as a raw image. */
    rc = VERR_VDI_INVALID_HEADER;

out:
    LogFlowFunc(("returns %Vrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnOpen */
static int rawOpen(const char *pszFilename, unsigned uOpenFlags,
                    PFNVDERROR pfnError, void *pvErrorUser,
                    void **ppBackendData)
{
    LogFlowFunc(("pszFilename=\"%s\" uOpenFlags=%#x ppBackendData=%#p\n", pszFilename, uOpenFlags, ppBackendData));
    int rc;
    PRAWIMAGE pImage;

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


    pImage = (PRAWIMAGE)RTMemAllocZ(sizeof(RAWIMAGE));
    if (!pImage)
    {
        rc = VERR_NO_MEMORY;
        goto out;
    }
    pImage->pszFilename = pszFilename;
    pImage->File = NIL_RTFILE;
    pImage->pfnError = pfnError;
    pImage->pvErrorUser = pvErrorUser;

    rc = rawOpenImage(pImage, uOpenFlags);
    if (VBOX_SUCCESS(rc))
        *ppBackendData = pImage;

out:
    LogFlowFunc(("returns %Vrc (pBackendData=%#p)\n", rc, *ppBackendData));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnCreate */
static int rawCreate(const char *pszFilename, VDIMAGETYPE enmType,
                     uint64_t cbSize, unsigned uImageFlags,
                     const char *pszComment,
                     PCPDMMEDIAGEOMETRY pPCHSGeometry,
                     PCPDMMEDIAGEOMETRY pLCHSGeometry,
                     unsigned uOpenFlags, PFNVMPROGRESS pfnProgress,
                     void *pvUser, unsigned uPercentStart,
                     unsigned uPercentSpan, PFNVDERROR pfnError,
                     void *pvErrorUser, void **ppBackendData)
{
    LogFlowFunc(("pszFilename=\"%s\" enmType=%d cbSize=%llu uImageFlags=%#x pszComment=\"%s\" pPCHSGeometry=%#p pLCHSGeometry=%#p uOpenFlags=%#x pfnProgress=%#p pvUser=%#p uPercentStart=%u uPercentSpan=%u pfnError=%#p pvErrorUser=%#p ppBackendData=%#p", pszFilename, enmType, cbSize, uImageFlags, pszComment, pPCHSGeometry, pLCHSGeometry, uOpenFlags, pfnProgress, pvUser, uPercentStart, uPercentSpan, pfnError, pvErrorUser, ppBackendData));
    int rc;
    PRAWIMAGE pImage;

    /* Check open flags. All valid flags are supported. */
    if (uOpenFlags & ~VD_OPEN_FLAGS_MASK)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    /* Check remaining arguments. */
    if (   !VALID_PTR(pszFilename)
        || !*pszFilename
        || (enmType != VD_IMAGE_TYPE_FIXED)
        || !VALID_PTR(pPCHSGeometry)
        || !VALID_PTR(pLCHSGeometry))
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    pImage = (PRAWIMAGE)RTMemAllocZ(sizeof(RAWIMAGE));
    if (!pImage)
    {
        rc = VERR_NO_MEMORY;
        goto out;
    }
    pImage->pszFilename = pszFilename;
    pImage->File = NIL_RTFILE;
    pImage->pfnError = pfnError;
    pImage->pvErrorUser = pvErrorUser;

    rc = rawCreateImage(pImage, enmType, cbSize, uImageFlags, pszComment,
                        pPCHSGeometry, pLCHSGeometry,
                        pfnProgress, pvUser, uPercentStart, uPercentSpan);
    if (VBOX_SUCCESS(rc))
    {
        /* So far the image is opened in read/write mode. Make sure the
         * image is opened in read-only mode if the caller requested that. */
        if (uOpenFlags & VD_OPEN_FLAGS_READONLY)
        {
            rawFreeImage(pImage, false);
            rc = rawOpenImage(pImage, uOpenFlags);
            if (VBOX_FAILURE(rc))
                goto out;
        }
        *ppBackendData = pImage;
    }

out:
    LogFlowFunc(("returns %Vrc (pBackendData=%#p)\n", rc, *ppBackendData));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnRename */
static int rawRename(void *pBackendData, const char *pszFilename)
{
    LogFlowFunc(("pBackendData=%#p pszFilename=%#p\n", pBackendData, pszFilename));
    int rc = VERR_NOT_IMPLEMENTED;

    LogFlowFunc(("returns %Vrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnClose */
static int rawClose(void *pBackendData, bool fDelete)
{
    LogFlowFunc(("pBackendData=%#p fDelete=%d\n", pBackendData, fDelete));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    int rc = VINF_SUCCESS;

    /* Freeing a never allocated image (e.g. because the open failed) is
     * not signalled as an error. After all nothing bad happens. */
    if (pImage)
        rawFreeImage(pImage, fDelete);

    LogFlowFunc(("returns %Vrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnRead */
static int rawRead(void *pBackendData, uint64_t uOffset, void *pvBuf,
                   size_t cbToRead, size_t *pcbActuallyRead)
{
    LogFlowFunc(("pBackendData=%#p uOffset=%llu pvBuf=%#p cbToRead=%zu pcbActuallyRead=%#p\n", pBackendData, uOffset, pvBuf, cbToRead, pcbActuallyRead));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
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

    rc = RTFileReadAt(pImage->File, uOffset, pvBuf, cbToRead, NULL);
    *pcbActuallyRead = cbToRead;

out:
    LogFlowFunc(("returns %Vrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnWrite */
static int rawWrite(void *pBackendData, uint64_t uOffset, const void *pvBuf,
                    size_t cbToWrite, size_t *pcbWriteProcess,
                    size_t *pcbPreRead, size_t *pcbPostRead, unsigned fWrite)
{
    LogFlowFunc(("pBackendData=%#p uOffset=%llu pvBuf=%#p cbToWrite=%zu pcbWriteProcess=%#p pcbPreRead=%#p pcbPostRead=%#p\n", pBackendData, uOffset, pvBuf, cbToWrite, pcbWriteProcess, pcbPreRead, pcbPostRead));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    int rc;

    Assert(pImage);
    Assert(uOffset % 512 == 0);
    Assert(cbToWrite % 512 == 0);

    if (pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
    {
        rc = VERR_VDI_IMAGE_READ_ONLY;
        goto out;
    }

    if (   uOffset + cbToWrite > pImage->cbSize
        || cbToWrite == 0)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    rc = RTFileWriteAt(pImage->File, uOffset, pvBuf, cbToWrite, NULL);
    if (pcbWriteProcess)
        *pcbWriteProcess = cbToWrite;

out:
    LogFlowFunc(("returns %Vrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnFlush */
static int rawFlush(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    int rc;

    rc = rawFlushImage(pImage);
    LogFlowFunc(("returns %Vrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetVersion */
static unsigned rawGetVersion(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;

    Assert(pImage);

    if (pImage)
        return 1;
    else
        return 0;
}

/** @copydoc VBOXHDDBACKEND::pfnGetImageType */
static int rawGetImageType(void *pBackendData, PVDIMAGETYPE penmImageType)
{
    LogFlowFunc(("pBackendData=%#p penmImageType=%#p\n", pBackendData, penmImageType));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    int rc = VINF_SUCCESS;

    Assert(pImage);
    Assert(penmImageType);

    if (pImage)
        *penmImageType = pImage->enmImageType;
    else
        rc = VERR_VDI_NOT_OPENED;

    LogFlowFunc(("returns %Vrc enmImageType=%u\n", rc, *penmImageType));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetSize */
static uint64_t rawGetSize(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;

    Assert(pImage);

    if (pImage)
        return pImage->cbSize;
    else
        return 0;
}

/** @copydoc VBOXHDDBACKEND::pfnGetFileSize */
static uint64_t rawGetFileSize(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    uint64_t cb = 0;

    Assert(pImage);

    if (pImage)
    {
        uint64_t cbFile;
        if (pImage->File != NIL_RTFILE)
        {
            int rc = RTFileGetSize(pImage->File, &cbFile);
            if (VBOX_SUCCESS(rc))
                cb += cbFile;
        }
    }

    LogFlowFunc(("returns %lld\n", cb));
    return cb;
}

/** @copydoc VBOXHDDBACKEND::pfnGetPCHSGeometry */
static int rawGetPCHSGeometry(void *pBackendData,
                              PPDMMEDIAGEOMETRY pPCHSGeometry)
{
    LogFlowFunc(("pBackendData=%#p pPCHSGeometry=%#p\n", pBackendData, pPCHSGeometry));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
    {
        if (pImage->PCHSGeometry.cCylinders)
        {
            *pPCHSGeometry = pImage->PCHSGeometry;
            rc = VINF_SUCCESS;
        }
        else
            rc = VERR_VDI_GEOMETRY_NOT_SET;
    }
    else
        rc = VERR_VDI_NOT_OPENED;

    LogFlowFunc(("returns %Vrc (PCHS=%u/%u/%u)\n", rc, pPCHSGeometry->cCylinders, pPCHSGeometry->cHeads, pPCHSGeometry->cSectors));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetPCHSGeometry */
static int rawSetPCHSGeometry(void *pBackendData,
                              PCPDMMEDIAGEOMETRY pPCHSGeometry)
{
    LogFlowFunc(("pBackendData=%#p pPCHSGeometry=%#p PCHS=%u/%u/%u\n", pBackendData, pPCHSGeometry, pPCHSGeometry->cCylinders, pPCHSGeometry->cHeads, pPCHSGeometry->cSectors));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
    {
        if (pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
        {
            rc = VERR_VDI_IMAGE_READ_ONLY;
            goto out;
        }

        pImage->PCHSGeometry = *pPCHSGeometry;
        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_VDI_NOT_OPENED;

out:
    LogFlowFunc(("returns %Vrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetLCHSGeometry */
static int rawGetLCHSGeometry(void *pBackendData,
                              PPDMMEDIAGEOMETRY pLCHSGeometry)
{
     LogFlowFunc(("pBackendData=%#p pLCHSGeometry=%#p\n", pBackendData, pLCHSGeometry));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
    {
        if (pImage->LCHSGeometry.cCylinders)
        {
            *pLCHSGeometry = pImage->LCHSGeometry;
            rc = VINF_SUCCESS;
        }
        else
            rc = VERR_VDI_GEOMETRY_NOT_SET;
    }
    else
        rc = VERR_VDI_NOT_OPENED;

    LogFlowFunc(("returns %Vrc (LCHS=%u/%u/%u)\n", rc, pLCHSGeometry->cCylinders, pLCHSGeometry->cHeads, pLCHSGeometry->cSectors));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetLCHSGeometry */
static int rawSetLCHSGeometry(void *pBackendData,
                               PCPDMMEDIAGEOMETRY pLCHSGeometry)
{
    LogFlowFunc(("pBackendData=%#p pLCHSGeometry=%#p LCHS=%u/%u/%u\n", pBackendData, pLCHSGeometry, pLCHSGeometry->cCylinders, pLCHSGeometry->cHeads, pLCHSGeometry->cSectors));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
    {
        if (pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
        {
            rc = VERR_VDI_IMAGE_READ_ONLY;
            goto out;
        }

        pImage->LCHSGeometry = *pLCHSGeometry;
        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_VDI_NOT_OPENED;

out:
    LogFlowFunc(("returns %Vrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetImageFlags */
static unsigned rawGetImageFlags(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    unsigned uImageFlags;

    Assert(pImage);

    if (pImage)
        uImageFlags = pImage->uImageFlags;
    else
        uImageFlags = 0;

    LogFlowFunc(("returns %#x\n", uImageFlags));
    return uImageFlags;
}

/** @copydoc VBOXHDDBACKEND::pfnGetOpenFlags */
static unsigned rawGetOpenFlags(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    unsigned uOpenFlags;

    Assert(pImage);

    if (pImage)
        uOpenFlags = pImage->uOpenFlags;
    else
        uOpenFlags = 0;

    LogFlowFunc(("returns %#x\n", uOpenFlags));
    return uOpenFlags;
}

/** @copydoc VBOXHDDBACKEND::pfnSetOpenFlags */
static int rawSetOpenFlags(void *pBackendData, unsigned uOpenFlags)
{
    LogFlowFunc(("pBackendData=%#p\n uOpenFlags=%#x", pBackendData, uOpenFlags));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    int rc;
    const char *pszFilename;

    /* Image must be opened and the new flags must be valid. Just readonly flag
     * is supported. */
    if (!pImage || uOpenFlags & ~VD_OPEN_FLAGS_READONLY)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    /* Implement this operation via reopening the image. */
    pszFilename = pImage->pszFilename;
    rawFreeImage(pImage, false);
    rc = rawOpenImage(pImage, uOpenFlags);

out:
    LogFlowFunc(("returns %Vrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetComment */
static int rawGetComment(void *pBackendData, char *pszComment,
                          size_t cbComment)
{
    LogFlowFunc(("pBackendData=%#p pszComment=%#p cbComment=%zu\n", pBackendData, pszComment, cbComment));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
    {
        if (pszComment)
            *pszComment = '\0';
        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_VDI_NOT_OPENED;

    LogFlowFunc(("returns %Vrc comment='%s'\n", rc, pszComment));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetComment */
static int rawSetComment(void *pBackendData, const char *pszComment)
{
    LogFlowFunc(("pBackendData=%#p pszComment=\"%s\"\n", pBackendData, pszComment));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
    {
        rc = VERR_VDI_IMAGE_READ_ONLY;
        goto out;
    }

    if (pImage)
        rc = VERR_NOT_SUPPORTED;
    else
        rc = VERR_VDI_NOT_OPENED;

out:
    LogFlowFunc(("returns %Vrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetUuid */
static int rawGetUuid(void *pBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p pUuid=%#p\n", pBackendData, pUuid));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
        rc = VERR_NOT_SUPPORTED;
    else
        rc = VERR_VDI_NOT_OPENED;

    LogFlowFunc(("returns %Vrc (%Vuuid)\n", rc, pUuid));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetUuid */
static int rawSetUuid(void *pBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p Uuid=%Vuuid\n", pBackendData, pUuid));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    int rc;

    LogFlowFunc(("%Vuuid\n", pUuid));
    Assert(pImage);

    if (pImage)
    {
        if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
            rc = VERR_NOT_SUPPORTED;
        else
            rc = VERR_VDI_IMAGE_READ_ONLY;
    }
    else
        rc = VERR_VDI_NOT_OPENED;

    LogFlowFunc(("returns %Vrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetModificationUuid */
static int rawGetModificationUuid(void *pBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p pUuid=%#p\n", pBackendData, pUuid));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
        rc = VERR_NOT_SUPPORTED;
    else
        rc = VERR_VDI_NOT_OPENED;

    LogFlowFunc(("returns %Vrc (%Vuuid)\n", rc, pUuid));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetModificationUuid */
static int rawSetModificationUuid(void *pBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p Uuid=%Vuuid\n", pBackendData, pUuid));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
    {
        if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
            rc = VERR_NOT_SUPPORTED;
        else
            rc = VERR_VDI_IMAGE_READ_ONLY;
    }
    else
        rc = VERR_VDI_NOT_OPENED;

    LogFlowFunc(("returns %Vrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetParentUuid */
static int rawGetParentUuid(void *pBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p pUuid=%#p\n", pBackendData, pUuid));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
        rc = VERR_NOT_SUPPORTED;
    else
        rc = VERR_VDI_NOT_OPENED;

    LogFlowFunc(("returns %Vrc (%Vuuid)\n", rc, pUuid));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetParentUuid */
static int rawSetParentUuid(void *pBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p Uuid=%Vuuid\n", pBackendData, pUuid));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
    {
        if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
            rc = VERR_NOT_SUPPORTED;
        else
            rc = VERR_VDI_IMAGE_READ_ONLY;
    }
    else
        rc = VERR_VDI_NOT_OPENED;

    LogFlowFunc(("returns %Vrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetParentModificationUuid */
static int rawGetParentModificationUuid(void *pBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p pUuid=%#p\n", pBackendData, pUuid));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
        rc = VERR_NOT_SUPPORTED;
    else
        rc = VERR_VDI_NOT_OPENED;

    LogFlowFunc(("returns %Vrc (%Vuuid)\n", rc, pUuid));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetParentModificationUuid */
static int rawSetParentModificationUuid(void *pBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p Uuid=%Vuuid\n", pBackendData, pUuid));
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
    {
        if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
            rc = VERR_NOT_SUPPORTED;
        else
            rc = VERR_VDI_IMAGE_READ_ONLY;
    }
    else
        rc = VERR_VDI_NOT_OPENED;

    LogFlowFunc(("returns %Vrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnDump */
static void rawDump(void *pBackendData)
{
    PRAWIMAGE pImage = (PRAWIMAGE)pBackendData;

    Assert(pImage);
    if (pImage)
    {
        RTLogPrintf("Header: Geometry PCHS=%u/%u/%u LCHS=%u/%u/%u cbSector=%llu\n",
                    pImage->PCHSGeometry.cCylinders, pImage->PCHSGeometry.cHeads, pImage->PCHSGeometry.cSectors,
                    pImage->LCHSGeometry.cCylinders, pImage->LCHSGeometry.cHeads, pImage->LCHSGeometry.cSectors,
                    pImage->cbSize / 512);
    }
}


VBOXHDDBACKEND g_RawBackend =
{
    /* pszBackendName */
    "raw",
    /* cbSize */
    sizeof(VBOXHDDBACKEND),
    /* uBackendCaps */
    VD_CAP_CREATE_FIXED,
    /* pfnCheckIfValid */
    rawCheckIfValid,
    /* pfnOpen */
    rawOpen,
    /* pfnCreate */
    rawCreate,
    /* pfnRename */
    rawRename,
    /* pfnClose */
    rawClose,
    /* pfnRead */
    rawRead,
    /* pfnWrite */
    rawWrite,
    /* pfnFlush */
    rawFlush,
    /* pfnGetVersion */
    rawGetVersion,
    /* pfnGetImageType */
    rawGetImageType,
    /* pfnGetSize */
    rawGetSize,
    /* pfnGetFileSize */
    rawGetFileSize,
    /* pfnGetPCHSGeometry */
    rawGetPCHSGeometry,
    /* pfnSetPCHSGeometry */
    rawSetPCHSGeometry,
    /* pfnGetLCHSGeometry */
    rawGetLCHSGeometry,
    /* pfnSetLCHSGeometry */
    rawSetLCHSGeometry,
    /* pfnGetImageFlags */
    rawGetImageFlags,
    /* pfnGetOpenFlags */
    rawGetOpenFlags,
    /* pfnSetOpenFlags */
    rawSetOpenFlags,
    /* pfnGetComment */
    rawGetComment,
    /* pfnSetComment */
    rawSetComment,
    /* pfnGetUuid */
    rawGetUuid,
    /* pfnSetUuid */
    rawSetUuid,
    /* pfnGetModificationUuid */
    rawGetModificationUuid,
    /* pfnSetModificationUuid */
    rawSetModificationUuid,
    /* pfnGetParentUuid */
    rawGetParentUuid,
    /* pfnSetParentUuid */
    rawSetParentUuid,
    /* pfnGetParentModificationUuid */
    rawGetParentModificationUuid,
    /* pfnSetParentModificationUuid */
    rawSetParentModificationUuid,
    /* pfnDump */
    rawDump
};
