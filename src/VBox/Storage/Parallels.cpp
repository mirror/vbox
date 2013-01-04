/* $Id$ */
/** @file
 *
 * Parallels hdd disk image, core code.
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

#define LOG_GROUP LOG_GROUP_VD_PARALLELS
#include <VBox/vd-plugin.h>
#include <VBox/err.h>

#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/uuid.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/asm.h>

#define PARALLELS_HEADER_MAGIC "WithoutFreeSpace"
#define PARALLELS_DISK_VERSION 2

/** The header of the parallels disk. */
#pragma pack(1)
typedef struct ParallelsHeader
{
    /** The magic header to identify a parallels hdd image. */
    char        HeaderIdentifier[16];
    /** The version of the disk image. */
    uint32_t    uVersion;
    /** The number of heads the hdd has. */
    uint32_t    cHeads;
    /** Number of cylinders. */
    uint32_t    cCylinders;
    /** Number of sectors per track. */
    uint32_t    cSectorsPerTrack;
    /** Number of entries in the allocation bitmap. */
    uint32_t    cEntriesInAllocationBitmap;
    /** Total number of sectors. */
    uint32_t    cSectors;
    /** Padding. */
    char        Padding[24];
} ParallelsHeader;
#pragma pack()

/**
 * Parallels image structure.
 */
typedef struct PARALLELSIMAGE
{
    /** Image file name. */
    const char         *pszFilename;
    /** Opaque storage handle. */
    PVDIOSTORAGE        pStorage;

    /** Pointer to the per-disk VD interface list. */
    PVDINTERFACE        pVDIfsDisk;
    /** Pointer to the per-image VD interface list. */
    PVDINTERFACE        pVDIfsImage;
    /** Error interface. */
    PVDINTERFACEERROR   pIfError;
    /** I/O interface. */
    PVDINTERFACEIOINT   pIfIo;

    /** Open flags passed by VBoxHDD layer. */
    unsigned            uOpenFlags;
    /** Image flags defined during creation or determined during open. */
    unsigned            uImageFlags;
    /** Total size of the image. */
    uint64_t            cbSize;

    /** Physical geometry of this image. */
    VDGEOMETRY          PCHSGeometry;
    /** Logical geometry of this image. */
    VDGEOMETRY          LCHSGeometry;

    /** Pointer to the allocation bitmap. */
    uint32_t           *pAllocationBitmap;
    /** Entries in the allocation bitmap. */
    uint64_t            cAllocationBitmapEntries;
    /** Flag whether the allocation bitmap was changed. */
    bool                fAllocationBitmapChanged;
    /** Current file size. */
    uint64_t            cbFileCurrent;
} PARALLELSIMAGE, *PPARALLELSIMAGE;

/*******************************************************************************
*   Static Variables                                                           *
*******************************************************************************/

/** NULL-terminated array of supported file extensions. */
static const VDFILEEXTENSION s_aParallelsFileExtensions[] =
{
    {"hdd", VDTYPE_HDD},
    {NULL, VDTYPE_INVALID}
};

/***************************************************
 * Internal functions                              *
 **************************************************/

/**
 * Internal. Flush image data to disk.
 */
static int parallelsFlushImage(PPARALLELSIMAGE pImage)
{
    int rc = VINF_SUCCESS;

    if (pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
        return VINF_SUCCESS;

    if (   !(pImage->uImageFlags & VD_IMAGE_FLAGS_FIXED)
        && (pImage->fAllocationBitmapChanged))
    {
        pImage->fAllocationBitmapChanged = false;
        /* Write the allocation bitmap to the file. */
        rc = vdIfIoIntFileWriteSync(pImage->pIfIo, pImage->pStorage,
                                    sizeof(ParallelsHeader), pImage->pAllocationBitmap,
                                    pImage->cAllocationBitmapEntries * sizeof(uint32_t),
                                    NULL);
        if (RT_FAILURE(rc))
            return rc;
    }

    /* Flush file. */
    rc = vdIfIoIntFileFlushSync(pImage->pIfIo, pImage->pStorage);

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/**
 * Internal. Free all allocated space for representing an image except pImage,
 * and optionally delete the image from disk.
 */
static int parallelsFreeImage(PPARALLELSIMAGE pImage, bool fDelete)
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
                parallelsFlushImage(pImage);

            vdIfIoIntFileClose(pImage->pIfIo, pImage->pStorage);
            pImage->pStorage = NULL;
        }

        if (pImage->pAllocationBitmap)
        {
            RTMemFree(pImage->pAllocationBitmap);
            pImage->pAllocationBitmap = NULL;
        }

        if (fDelete && pImage->pszFilename)
            vdIfIoIntFileDelete(pImage->pIfIo, pImage->pszFilename);
    }

    return rc;
}

static int parallelsOpenImage(PPARALLELSIMAGE pImage, unsigned uOpenFlags)
{
    int rc = VINF_SUCCESS;
    ParallelsHeader parallelsHeader;

    pImage->pIfError = VDIfErrorGet(pImage->pVDIfsDisk);
    pImage->pIfIo = VDIfIoIntGet(pImage->pVDIfsImage);
    AssertPtrReturn(pImage->pIfIo, VERR_INVALID_PARAMETER);

    rc = vdIfIoIntFileOpen(pImage->pIfIo, pImage->pszFilename,
                           VDOpenFlagsToFileOpenFlags(uOpenFlags,
                                                      false /* fCreate */),
                           &pImage->pStorage);
    if (RT_FAILURE(rc))
        goto out;

    rc = vdIfIoIntFileGetSize(pImage->pIfIo, pImage->pStorage, &pImage->cbFileCurrent);
    if (RT_FAILURE(rc))
        goto out;
    AssertMsg(pImage->cbFileCurrent % 512 == 0, ("File size is not a multiple of 512\n"));

    rc = vdIfIoIntFileReadSync(pImage->pIfIo, pImage->pStorage, 0,
                               &parallelsHeader, sizeof(parallelsHeader), NULL);
    if (RT_FAILURE(rc))
        goto out;

    if (memcmp(parallelsHeader.HeaderIdentifier, PARALLELS_HEADER_MAGIC, 16))
    {
        /* Check if the file has hdd as extension. It is a fixed size raw image then. */
        char *pszExtension = RTPathExt(pImage->pszFilename);
        if (strcmp(pszExtension, ".hdd"))
        {
            rc = VERR_VD_PARALLELS_INVALID_HEADER;
            goto out;
        }

        /* This is a fixed size image. */
        pImage->uImageFlags |= VD_IMAGE_FLAGS_FIXED;
        pImage->cbSize = pImage->cbFileCurrent;

        pImage->PCHSGeometry.cHeads     = 16;
        pImage->PCHSGeometry.cSectors   = 63;
        uint64_t cCylinders = pImage->cbSize / (512 * pImage->PCHSGeometry.cSectors * pImage->PCHSGeometry.cHeads);
        pImage->PCHSGeometry.cCylinders = (uint32_t)cCylinders;
    }
    else
    {
        if (parallelsHeader.uVersion != PARALLELS_DISK_VERSION)
        {
            rc = VERR_NOT_SUPPORTED;
            goto out;
        }

        if (parallelsHeader.cEntriesInAllocationBitmap > (1 << 30))
        {
            rc = VERR_NOT_SUPPORTED;
            goto out;
        }

        Log(("cSectors=%u\n", parallelsHeader.cSectors));
        pImage->cbSize = ((uint64_t)parallelsHeader.cSectors) * 512;
        pImage->uImageFlags = VD_IMAGE_FLAGS_NONE;
        pImage->cAllocationBitmapEntries = parallelsHeader.cEntriesInAllocationBitmap;
        pImage->pAllocationBitmap = (uint32_t *)RTMemAllocZ((uint32_t)pImage->cAllocationBitmapEntries * sizeof(uint32_t));
        if (!pImage->pAllocationBitmap)
        {
            rc = VERR_NO_MEMORY;
            goto out;
        }

        rc = vdIfIoIntFileReadSync(pImage->pIfIo, pImage->pStorage,
                                   sizeof(ParallelsHeader), pImage->pAllocationBitmap,
                                   pImage->cAllocationBitmapEntries * sizeof(uint32_t),
                                   NULL);
        if (RT_FAILURE(rc))
            goto out;

        pImage->PCHSGeometry.cCylinders = parallelsHeader.cCylinders;
        pImage->PCHSGeometry.cHeads     = parallelsHeader.cHeads;
        pImage->PCHSGeometry.cSectors   = parallelsHeader.cSectorsPerTrack;
    }

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/**
 * Internal: Create a parallels image.
 */
static int parallelsCreateImage(PPARALLELSIMAGE pImage, uint64_t cbSize,
                                unsigned uImageFlags, const char *pszComment,
                                PCVDGEOMETRY pPCHSGeometry,
                                PCVDGEOMETRY pLCHSGeometry, unsigned uOpenFlags,
                                PFNVDPROGRESS pfnProgress, void *pvUser,
                                unsigned uPercentStart, unsigned uPercentSpan)
{
    int rc = VINF_SUCCESS;
    int32_t fOpen;

    if (uImageFlags & VD_IMAGE_FLAGS_FIXED)
    {
        rc = vdIfError(pImage->pIfError, VERR_VD_INVALID_TYPE, RT_SRC_POS, N_("Parallels: cannot create fixed image '%s'. Create a raw image"), pImage->pszFilename);
        goto out;
    }

    pImage->uOpenFlags   = uOpenFlags & ~VD_OPEN_FLAGS_READONLY;
    pImage->uImageFlags  = uImageFlags;
    pImage->PCHSGeometry = *pPCHSGeometry;
    pImage->LCHSGeometry = *pLCHSGeometry;

    if (!pImage->PCHSGeometry.cCylinders)
    {
        /* Set defaults. */
        pImage->PCHSGeometry.cSectors   = 63;
        pImage->PCHSGeometry.cHeads     = 16;
        pImage->PCHSGeometry.cCylinders = pImage->cbSize / (512 * pImage->PCHSGeometry.cSectors * pImage->PCHSGeometry.cHeads);
    }

    pImage->pIfError = VDIfErrorGet(pImage->pVDIfsDisk);
    pImage->pIfIo = VDIfIoIntGet(pImage->pVDIfsImage);
    AssertPtrReturn(pImage->pIfIo, VERR_INVALID_PARAMETER);

    /* Create image file. */
    fOpen = VDOpenFlagsToFileOpenFlags(pImage->uOpenFlags, true /* fCreate */);
    rc = vdIfIoIntFileOpen(pImage->pIfIo, pImage->pszFilename, fOpen, &pImage->pStorage);
    if (RT_FAILURE(rc))
    {
        rc = vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("Parallels: cannot create image '%s'"), pImage->pszFilename);
        goto out;
    }

    if (RT_SUCCESS(rc) && pfnProgress)
        pfnProgress(pvUser, uPercentStart + uPercentSpan * 98 / 100);

    /* Setup image state. */
    pImage->cbSize                   = cbSize;
    pImage->cAllocationBitmapEntries = cbSize / 512 / pImage->PCHSGeometry.cSectors;
    if (pImage->cAllocationBitmapEntries * pImage->PCHSGeometry.cSectors * 512 < cbSize)
        pImage->cAllocationBitmapEntries++;
    pImage->fAllocationBitmapChanged = true;
    pImage->cbFileCurrent            = sizeof(ParallelsHeader) + pImage->cAllocationBitmapEntries * sizeof(uint32_t);
    /* Round to next sector boundary. */
    pImage->cbFileCurrent           += 512 - pImage->cbFileCurrent % 512;
    Assert(!(pImage->cbFileCurrent % 512));
    pImage->pAllocationBitmap        = (uint32_t *)RTMemAllocZ(pImage->cAllocationBitmapEntries * sizeof(uint32_t));
    if (!pImage->pAllocationBitmap)
        rc = VERR_NO_MEMORY;

    if (RT_SUCCESS(rc))
    {
        ParallelsHeader Header;

        memcpy(Header.HeaderIdentifier, PARALLELS_HEADER_MAGIC, sizeof(Header.HeaderIdentifier));
        Header.uVersion                   = RT_H2LE_U32(PARALLELS_DISK_VERSION);
        Header.cHeads                     = RT_H2LE_U32(pImage->PCHSGeometry.cHeads);
        Header.cCylinders                 = RT_H2LE_U32(pImage->PCHSGeometry.cCylinders);
        Header.cSectorsPerTrack           = RT_H2LE_U32(pImage->PCHSGeometry.cSectors);
        Header.cEntriesInAllocationBitmap = RT_H2LE_U32(pImage->cAllocationBitmapEntries);
        Header.cSectors                   = RT_H2LE_U32(pImage->cbSize / 512);
        memset(Header.Padding, 0, sizeof(Header.Padding));

        /* Write header and allocation bitmap. */
        rc = vdIfIoIntFileSetSize(pImage->pIfIo, pImage->pStorage, pImage->cbFileCurrent);
        if (RT_SUCCESS(rc))
            rc = vdIfIoIntFileWriteSync(pImage->pIfIo, pImage->pStorage, 0,
                                        &Header, sizeof(Header), NULL);
        if (RT_SUCCESS(rc))
            rc = parallelsFlushImage(pImage); /* Writes the allocation bitmap. */
    }

out:
    if (RT_SUCCESS(rc) && pfnProgress)
        pfnProgress(pvUser, uPercentStart + uPercentSpan);

    if (RT_FAILURE(rc))
        parallelsFreeImage(pImage, rc != VERR_ALREADY_EXISTS);
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnCheckIfValid */
static int parallelsCheckIfValid(const char *pszFilename, PVDINTERFACE pVDIfsDisk,
                                 PVDINTERFACE pVDIfsImage, VDTYPE *penmType)
{
    int rc;
    PVDIOSTORAGE pStorage;
    ParallelsHeader parallelsHeader;

    PVDINTERFACEIOINT pIfIo = VDIfIoIntGet(pVDIfsImage);
    AssertPtrReturn(pIfIo, VERR_INVALID_PARAMETER);

    rc = vdIfIoIntFileOpen(pIfIo, pszFilename,
                           VDOpenFlagsToFileOpenFlags(VD_OPEN_FLAGS_READONLY,
                                                      false /* fCreate */),
                           &pStorage);
    if (RT_FAILURE(rc))
        return rc;

    rc = vdIfIoIntFileReadSync(pIfIo, pStorage, 0, &parallelsHeader,
                               sizeof(ParallelsHeader), NULL);
    if (RT_SUCCESS(rc))
    {
        if (   !memcmp(parallelsHeader.HeaderIdentifier, PARALLELS_HEADER_MAGIC, 16)
            && (parallelsHeader.uVersion == PARALLELS_DISK_VERSION))
            rc = VINF_SUCCESS;
        else
        {
            /*
             * The image may be an fixed size image.
             * Unfortunately fixed sized parallels images
             * are just raw files hence no magic header to
             * check for.
             * The code succeeds if the file is a multiple
             * of 512 and if the file extensions is *.hdd
             */
            uint64_t cbFile;
            char *pszExtension;

            rc = vdIfIoIntFileGetSize(pIfIo, pStorage, &cbFile);
            if (RT_FAILURE(rc) || ((cbFile % 512) != 0))
            {
                vdIfIoIntFileClose(pIfIo, pStorage);
                return VERR_VD_PARALLELS_INVALID_HEADER;
            }

            pszExtension = RTPathExt(pszFilename);
            if (!pszExtension || strcmp(pszExtension, ".hdd"))
                rc = VERR_VD_PARALLELS_INVALID_HEADER;
            else
                rc = VINF_SUCCESS;
        }
    }

    if (RT_SUCCESS(rc))
        *penmType = VDTYPE_HDD;

    vdIfIoIntFileClose(pIfIo, pStorage);
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnOpen */
static int parallelsOpen(const char *pszFilename, unsigned uOpenFlags,
                         PVDINTERFACE pVDIfsDisk, PVDINTERFACE pVDIfsImage,
                         VDTYPE enmType, void **ppBackendData)
{
    LogFlowFunc(("pszFilename=\"%s\" uOpenFlags=%#x pVDIfsDisk=%#p pVDIfsImage=%#p ppBackendData=%#p\n", pszFilename, uOpenFlags, pVDIfsDisk, pVDIfsImage, ppBackendData));
    int rc;
    PPARALLELSIMAGE pImage;

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

    pImage = (PPARALLELSIMAGE)RTMemAllocZ(sizeof(PARALLELSIMAGE));
    if (!pImage)
    {
        rc = VERR_NO_MEMORY;
        goto out;
    }

    pImage->pszFilename = pszFilename;
    pImage->pStorage = NULL;
    pImage->pVDIfsDisk = pVDIfsDisk;
    pImage->pVDIfsImage = pVDIfsImage;
    pImage->fAllocationBitmapChanged = false;

    rc = parallelsOpenImage(pImage, uOpenFlags);
    if (RT_SUCCESS(rc))
        *ppBackendData = pImage;
    else
        RTMemFree(pImage);

out:
    LogFlowFunc(("returns %Rrc (pBackendData=%#p)\n", rc, *ppBackendData));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnCreate */
static int parallelsCreate(const char *pszFilename, uint64_t cbSize,
                           unsigned uImageFlags, const char *pszComment,
                           PCVDGEOMETRY pPCHSGeometry,
                           PCVDGEOMETRY pLCHSGeometry, PCRTUUID pUuid,
                           unsigned uOpenFlags, unsigned uPercentStart,
                           unsigned uPercentSpan, PVDINTERFACE pVDIfsDisk,
                           PVDINTERFACE pVDIfsImage,
                           PVDINTERFACE pVDIfsOperation, void **ppBackendData)
{
    LogFlowFunc(("pszFilename=\"%s\" cbSize=%llu uImageFlags=%#x pszComment=\"%s\" pPCHSGeometry=%#p pLCHSGeometry=%#p Uuid=%RTuuid uOpenFlags=%#x uPercentStart=%u uPercentSpan=%u pVDIfsDisk=%#p pVDIfsImage=%#p pVDIfsOperation=%#p ppBackendData=%#p",
                 pszFilename, cbSize, uImageFlags, pszComment, pPCHSGeometry, pLCHSGeometry, pUuid, uOpenFlags, uPercentStart, uPercentSpan, pVDIfsDisk, pVDIfsImage, pVDIfsOperation, ppBackendData));
    int rc = VINF_SUCCESS;
    PPARALLELSIMAGE pImage;

    PFNVDPROGRESS pfnProgress = NULL;
    void *pvUser = NULL;
    PVDINTERFACEPROGRESS pIfProgress = VDIfProgressGet(pVDIfsOperation);
    if (pIfProgress)
    {
        pfnProgress = pIfProgress->pfnProgress;
        pvUser = pIfProgress->Core.pvUser;
    }

    /* Check open flags. All valid flags are supported. */
    if (uOpenFlags & ~VD_OPEN_FLAGS_MASK)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    /* Check remaining arguments. */
    if (   !VALID_PTR(pszFilename)
        || !*pszFilename
        || !VALID_PTR(pPCHSGeometry)
        || !VALID_PTR(pLCHSGeometry))
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    pImage = (PPARALLELSIMAGE)RTMemAllocZ(sizeof(PARALLELSIMAGE));
    if (!pImage)
    {
        rc = VERR_NO_MEMORY;
        goto out;
    }
    pImage->pszFilename = pszFilename;
    pImage->pStorage = NULL;
    pImage->pVDIfsDisk = pVDIfsDisk;
    pImage->pVDIfsImage = pVDIfsImage;

    rc = parallelsCreateImage(pImage, cbSize, uImageFlags, pszComment,
                              pPCHSGeometry, pLCHSGeometry, uOpenFlags,
                              pfnProgress, pvUser, uPercentStart, uPercentSpan);
    if (RT_SUCCESS(rc))
    {
        /* So far the image is opened in read/write mode. Make sure the
         * image is opened in read-only mode if the caller requested that. */
        if (uOpenFlags & VD_OPEN_FLAGS_READONLY)
        {
            parallelsFreeImage(pImage, false);
            rc = parallelsOpenImage(pImage, uOpenFlags);
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
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnRename */
static int parallelsRename(void *pBackendData, const char *pszFilename)
{
    LogFlowFunc(("pBackendData=%#p pszFilename=%#p\n", pBackendData, pszFilename));
    int rc = VINF_SUCCESS;
    PPARALLELSIMAGE pImage = (PPARALLELSIMAGE)pBackendData;

    /* Check arguments. */
    if (   !pImage
        || !pszFilename
        || !*pszFilename)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    /* Close the image. */
    rc = parallelsFreeImage(pImage, false);
    if (RT_FAILURE(rc))
        goto out;

    /* Rename the file. */
    rc = vdIfIoIntFileMove(pImage->pIfIo, pImage->pszFilename, pszFilename, 0);
    if (RT_FAILURE(rc))
    {
        /* The move failed, try to reopen the original image. */
        int rc2 = parallelsOpenImage(pImage, pImage->uOpenFlags);
        if (RT_FAILURE(rc2))
            rc = rc2;

        goto out;
    }

    /* Update pImage with the new information. */
    pImage->pszFilename = pszFilename;

    /* Open the old image with new name. */
    rc = parallelsOpenImage(pImage, pImage->uOpenFlags);
    if (RT_FAILURE(rc))
        goto out;

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnClose */
static int parallelsClose(void *pBackendData, bool fDelete)
{
    LogFlowFunc(("pBackendData=%#p fDelete=%d\n", pBackendData, fDelete));
    PPARALLELSIMAGE pImage = (PPARALLELSIMAGE)pBackendData;
    int rc;

    rc = parallelsFreeImage(pImage, fDelete);
    RTMemFree(pImage);

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnRead */
static int parallelsRead(void *pBackendData, uint64_t uOffset, void *pvBuf,
                         size_t cbToRead, size_t *pcbActuallyRead)
{
    LogFlowFunc(("pBackendData=%#p uOffset=%llu pvBuf=%#p cbToRead=%zu pcbActuallyRead=%#p\n",
                 pBackendData, uOffset, pvBuf, cbToRead, pcbActuallyRead));
    PPARALLELSIMAGE pImage = (PPARALLELSIMAGE)pBackendData;
    int rc = VINF_SUCCESS;

    AssertPtr(pImage);
    Assert(uOffset % 512 == 0);
    Assert(cbToRead % 512 == 0);

    if (pImage->uImageFlags & VD_IMAGE_FLAGS_FIXED)
        rc = vdIfIoIntFileReadSync(pImage->pIfIo, pImage->pStorage, uOffset, pvBuf, cbToRead, NULL);
    else
    {
        uint64_t uSector;
        uint32_t iIndexInAllocationTable;

        /* Calculate offset in the real file. */
        uSector = uOffset / 512;

        /* One chunk in the file is always one track big. */
        iIndexInAllocationTable = (uint32_t)(uSector / pImage->PCHSGeometry.cSectors);
        uSector = uSector % pImage->PCHSGeometry.cSectors;

        Assert(iIndexInAllocationTable < pImage->cAllocationBitmapEntries);

        cbToRead = RT_MIN(cbToRead, (pImage->PCHSGeometry.cSectors - uSector)*512);

        LogFlowFunc(("AllocationBitmap[%u]=%u uSector=%u cbToRead=%zu cAllocationBitmapEntries=%u\n",
                     iIndexInAllocationTable, pImage->pAllocationBitmap[iIndexInAllocationTable],
                     uSector, cbToRead, pImage->cAllocationBitmapEntries));

        if (pImage->pAllocationBitmap[iIndexInAllocationTable] == 0)
            rc = VERR_VD_BLOCK_FREE;
        else
        {
            uint64_t uOffsetInFile = ((uint64_t)pImage->pAllocationBitmap[iIndexInAllocationTable] + uSector) * 512;

            LogFlowFunc(("uOffsetInFile=%llu\n", uOffsetInFile));
            rc = vdIfIoIntFileReadSync(pImage->pIfIo, pImage->pStorage, uOffsetInFile,
                                       pvBuf, cbToRead, NULL);
        }
    }

    if (   (   RT_SUCCESS(rc)
            || rc == VERR_VD_BLOCK_FREE)
        && pcbActuallyRead)
        *pcbActuallyRead = cbToRead;

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnWrite */
static int parallelsWrite(void *pBackendData, uint64_t uOffset, const void *pvBuf,
                          size_t cbToWrite, size_t *pcbWriteProcess,
                          size_t *pcbPreRead, size_t *pcbPostRead, unsigned fWrite)
{
    LogFlowFunc(("pBackendData=%#p uOffset=%llu pvBuf=%#p cbToWrite=%zu pcbWriteProcess=%#p\n",
                 pBackendData, uOffset, pvBuf, cbToWrite, pcbWriteProcess));
    PPARALLELSIMAGE pImage = (PPARALLELSIMAGE)pBackendData;
    int rc = VINF_SUCCESS;

    AssertPtr(pImage);
    Assert(uOffset % 512 == 0);
    Assert(cbToWrite % 512 == 0);

    if (pImage->uImageFlags & VD_IMAGE_FLAGS_FIXED)
        rc = vdIfIoIntFileWriteSync(pImage->pIfIo, pImage->pStorage, uOffset,
                                    pvBuf, cbToWrite, NULL);
    else
    {
        uint64_t uSector;
        uint64_t uOffsetInFile;
        uint32_t iIndexInAllocationTable;

        /* Calculate offset in the real file. */
        uSector = uOffset / 512;
        /* One chunk in the file is always one track big. */
        iIndexInAllocationTable = (uint32_t)(uSector / pImage->PCHSGeometry.cSectors);
        uSector = uSector % pImage->PCHSGeometry.cSectors;

        Assert(iIndexInAllocationTable < pImage->cAllocationBitmapEntries);

        cbToWrite = RT_MIN(cbToWrite, (pImage->PCHSGeometry.cSectors - uSector)*512);

        LogFlowFunc(("AllocationBitmap[%u]=%u uSector=%u cbToWrite=%zu cAllocationBitmapEntries=%u\n",
                     iIndexInAllocationTable, pImage->pAllocationBitmap[iIndexInAllocationTable],
                     uSector, cbToWrite, pImage->cAllocationBitmapEntries));

        if (pImage->pAllocationBitmap[iIndexInAllocationTable] == 0)
        {
            if (   cbToWrite == pImage->PCHSGeometry.cSectors * 512
                && !(fWrite & VD_WRITE_NO_ALLOC))
            {
                /* Stay on the safe side. Do not run the risk of confusing the higher
                 * level, as that can be pretty lethal to image consistency. */
                *pcbPreRead = 0;
                *pcbPostRead = 0;

                /* Allocate new chunk in the file. */
                AssertMsg(pImage->cbFileCurrent % 512 == 0, ("File size is not a multiple of 512\n"));
                pImage->pAllocationBitmap[iIndexInAllocationTable] = (uint32_t)(pImage->cbFileCurrent / 512);
                pImage->cbFileCurrent += pImage->PCHSGeometry.cSectors * 512;
                pImage->fAllocationBitmapChanged = true;

                uOffsetInFile = (uint64_t)pImage->pAllocationBitmap[iIndexInAllocationTable] * 512;

                LogFlowFunc(("uOffsetInFile=%llu\n", uOffsetInFile));

                /*
                 * Write the new block at the current end of the file.
                 */
                rc = vdIfIoIntFileWriteSync(pImage->pIfIo, pImage->pStorage,
                                            uOffsetInFile, pvBuf, cbToWrite, NULL);
            }
            else
            {
                /* Trying to do a partial write to an unallocated cluster. Don't do
                 * anything except letting the upper layer know what to do. */
                *pcbPreRead  = uSector * 512;
                *pcbPostRead = (pImage->PCHSGeometry.cSectors * 512) - cbToWrite - *pcbPreRead;
                rc = VERR_VD_BLOCK_FREE;
            }
        }
        else
        {
            uOffsetInFile = ((uint64_t)pImage->pAllocationBitmap[iIndexInAllocationTable] + uSector) * 512;

            LogFlowFunc(("uOffsetInFile=%llu\n", uOffsetInFile));
            rc = vdIfIoIntFileWriteSync(pImage->pIfIo, pImage->pStorage, uOffsetInFile,
                                        pvBuf, cbToWrite, NULL);
        }
    }

    if (pcbWriteProcess)
        *pcbWriteProcess = cbToWrite;

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnFlush */
static int parallelsFlush(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PPARALLELSIMAGE pImage = (PPARALLELSIMAGE)pBackendData;
    int rc;

    AssertPtr(pImage);

    rc = parallelsFlushImage(pImage);

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetVersion */
static unsigned parallelsGetVersion(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PPARALLELSIMAGE pImage = (PPARALLELSIMAGE)pBackendData;

    AssertPtr(pImage);

    if (pImage)
        return PARALLELS_DISK_VERSION;
    else
        return 0;
}

/** @copydoc VBOXHDDBACKEND::pfnGetSize */
static uint64_t parallelsGetSize(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PPARALLELSIMAGE pImage = (PPARALLELSIMAGE)pBackendData;
    uint64_t cb = 0;

    AssertPtr(pImage);

    if (pImage && pImage->pStorage)
        cb = pImage->cbSize;

    LogFlowFunc(("returns %llu\n", cb));
    return cb;
}

/** @copydoc VBOXHDDBACKEND::pfnGetFileSize */
static uint64_t parallelsGetFileSize(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PPARALLELSIMAGE pImage = (PPARALLELSIMAGE)pBackendData;
    uint64_t cb = 0;

    AssertPtr(pImage);

    if (pImage && pImage->pStorage)
        cb = pImage->cbFileCurrent;

    LogFlowFunc(("returns %lld\n", cb));
    return cb;
}

/** @copydoc VBOXHDDBACKEND::pfnGetPCHSGeometry */
static int parallelsGetPCHSGeometry(void *pBackendData,
                                    PVDGEOMETRY pPCHSGeometry)
{
    LogFlowFunc(("pBackendData=%#p pPCHSGeometry=%#p\n", pBackendData, pPCHSGeometry));
    PPARALLELSIMAGE pImage = (PPARALLELSIMAGE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
    {
        if (pImage->PCHSGeometry.cCylinders)
        {
            *pPCHSGeometry = pImage->PCHSGeometry;
            rc = VINF_SUCCESS;
        }
        else
            rc = VERR_VD_GEOMETRY_NOT_SET;
    }
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc (PCHS=%u/%u/%u)\n", rc, pPCHSGeometry->cCylinders, pPCHSGeometry->cHeads, pPCHSGeometry->cSectors));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetPCHSGeometry */
static int parallelsSetPCHSGeometry(void *pBackendData,
                                    PCVDGEOMETRY pPCHSGeometry)
{
    LogFlowFunc(("pBackendData=%#p pPCHSGeometry=%#p PCHS=%u/%u/%u\n", pBackendData, pPCHSGeometry, pPCHSGeometry->cCylinders, pPCHSGeometry->cHeads, pPCHSGeometry->cSectors));
    PPARALLELSIMAGE pImage = (PPARALLELSIMAGE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
    {
        if (pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
        {
            rc = VERR_VD_IMAGE_READ_ONLY;
            goto out;
        }

        pImage->PCHSGeometry = *pPCHSGeometry;
        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_VD_NOT_OPENED;

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetLCHSGeometry */
static int parallelsGetLCHSGeometry(void *pBackendData,
                                    PVDGEOMETRY pLCHSGeometry)
{
    LogFlowFunc(("pBackendData=%#p pLCHSGeometry=%#p\n", pBackendData, pLCHSGeometry));
    PPARALLELSIMAGE pImage = (PPARALLELSIMAGE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
    {
        if (pImage->LCHSGeometry.cCylinders)
        {
            *pLCHSGeometry = pImage->LCHSGeometry;
            rc = VINF_SUCCESS;
        }
        else
            rc = VERR_VD_GEOMETRY_NOT_SET;
    }
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc (LCHS=%u/%u/%u)\n", rc, pLCHSGeometry->cCylinders, pLCHSGeometry->cHeads, pLCHSGeometry->cSectors));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetLCHSGeometry */
static int parallelsSetLCHSGeometry(void *pBackendData,
                                    PCVDGEOMETRY pLCHSGeometry)
{
    LogFlowFunc(("pBackendData=%#p pLCHSGeometry=%#p LCHS=%u/%u/%u\n", pBackendData, pLCHSGeometry, pLCHSGeometry->cCylinders, pLCHSGeometry->cHeads, pLCHSGeometry->cSectors));
    PPARALLELSIMAGE pImage = (PPARALLELSIMAGE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
    {
        if (pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
        {
            rc = VERR_VD_IMAGE_READ_ONLY;
            goto out;
        }

        pImage->LCHSGeometry = *pLCHSGeometry;
        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_VD_NOT_OPENED;

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetImageFlags */
static unsigned parallelsGetImageFlags(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PPARALLELSIMAGE pImage = (PPARALLELSIMAGE)pBackendData;
    unsigned uImageFlags;

    AssertPtr(pImage);

    if (pImage)
        uImageFlags = pImage->uImageFlags;
    else
        uImageFlags = 0;

    LogFlowFunc(("returns %#x\n", uImageFlags));
    return uImageFlags;
}

/** @copydoc VBOXHDDBACKEND::pfnGetOpenFlags */
static unsigned parallelsGetOpenFlags(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PPARALLELSIMAGE pImage = (PPARALLELSIMAGE)pBackendData;
    unsigned uOpenFlags;

    AssertPtr(pImage);

    if (pImage)
        uOpenFlags = pImage->uOpenFlags;
    else
        uOpenFlags = 0;

    LogFlowFunc(("returns %#x\n", uOpenFlags));
    return uOpenFlags;
}

/** @copydoc VBOXHDDBACKEND::pfnSetOpenFlags */
static int parallelsSetOpenFlags(void *pBackendData, unsigned uOpenFlags)
{
    LogFlowFunc(("pBackendData=%#p\n uOpenFlags=%#x", pBackendData, uOpenFlags));
    PPARALLELSIMAGE pImage = (PPARALLELSIMAGE)pBackendData;
    int rc;

    /* Image must be opened and the new flags must be valid. */
    if (!pImage || (uOpenFlags & ~(  VD_OPEN_FLAGS_READONLY | VD_OPEN_FLAGS_INFO
                                   | VD_OPEN_FLAGS_ASYNC_IO | VD_OPEN_FLAGS_SHAREABLE
                                   | VD_OPEN_FLAGS_SEQUENTIAL | VD_OPEN_FLAGS_SKIP_CONSISTENCY_CHECKS)))
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    /* Implement this operation via reopening the image. */
    parallelsFreeImage(pImage, false);
    rc = parallelsOpenImage(pImage, uOpenFlags);

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetComment */
static int parallelsGetComment(void *pBackendData, char *pszComment,
                               size_t cbComment)
{
    LogFlowFunc(("pBackendData=%#p pszComment=%#p cbComment=%zu\n", pBackendData, pszComment, cbComment));
    PPARALLELSIMAGE pImage = (PPARALLELSIMAGE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
        rc = VERR_NOT_SUPPORTED;
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc comment='%s'\n", rc, pszComment));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetComment */
static int parallelsSetComment(void *pBackendData, const char *pszComment)
{
    LogFlowFunc(("pBackendData=%#p pszComment=\"%s\"\n", pBackendData, pszComment));
    PPARALLELSIMAGE pImage = (PPARALLELSIMAGE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
    {
        if (pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
            rc = VERR_VD_IMAGE_READ_ONLY;
        else
            rc = VERR_NOT_SUPPORTED;
    }
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetUuid */
static int parallelsGetUuid(void *pBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p pUuid=%#p\n", pBackendData, pUuid));
    PPARALLELSIMAGE pImage = (PPARALLELSIMAGE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
        rc = VERR_NOT_SUPPORTED;
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc (%RTuuid)\n", rc, pUuid));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetUuid */
static int parallelsSetUuid(void *pBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p Uuid=%RTuuid\n", pBackendData, pUuid));
    PPARALLELSIMAGE pImage = (PPARALLELSIMAGE)pBackendData;
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

/** @copydoc VBOXHDDBACKEND::pfnGetModificationUuid */
static int parallelsGetModificationUuid(void *pBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p pUuid=%#p\n", pBackendData, pUuid));
    PPARALLELSIMAGE pImage = (PPARALLELSIMAGE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
        rc = VERR_NOT_SUPPORTED;
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc (%RTuuid)\n", rc, pUuid));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetModificationUuid */
static int parallelsSetModificationUuid(void *pBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p Uuid=%RTuuid\n", pBackendData, pUuid));
    PPARALLELSIMAGE pImage = (PPARALLELSIMAGE)pBackendData;
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

/** @copydoc VBOXHDDBACKEND::pfnGetParentUuid */
static int parallelsGetParentUuid(void *pBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p pUuid=%#p\n", pBackendData, pUuid));
    PPARALLELSIMAGE pImage = (PPARALLELSIMAGE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
        rc = VERR_NOT_SUPPORTED;
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc (%RTuuid)\n", rc, pUuid));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetParentUuid */
static int parallelsSetParentUuid(void *pBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p Uuid=%RTuuid\n", pBackendData, pUuid));
    PPARALLELSIMAGE pImage = (PPARALLELSIMAGE)pBackendData;
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

/** @copydoc VBOXHDDBACKEND::pfnGetParentModificationUuid */
static int parallelsGetParentModificationUuid(void *pBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p pUuid=%#p\n", pBackendData, pUuid));
    PPARALLELSIMAGE pImage = (PPARALLELSIMAGE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
        rc = VERR_NOT_SUPPORTED;
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc (%RTuuid)\n", rc, pUuid));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetParentModificationUuid */
static int parallelsSetParentModificationUuid(void *pBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p Uuid=%RTuuid\n", pBackendData, pUuid));
    PPARALLELSIMAGE pImage = (PPARALLELSIMAGE)pBackendData;
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

/** @copydoc VBOXHDDBACKEND::pfnDump */
static void parallelsDump(void *pBackendData)
{
    PPARALLELSIMAGE pImage = (PPARALLELSIMAGE)pBackendData;

    AssertPtr(pImage);
    if (pImage)
    {
        vdIfErrorMessage(pImage->pIfError, "Header: Geometry PCHS=%u/%u/%u LCHS=%u/%u/%u\n",
                         pImage->PCHSGeometry.cCylinders, pImage->PCHSGeometry.cHeads, pImage->PCHSGeometry.cSectors,
                         pImage->LCHSGeometry.cCylinders, pImage->LCHSGeometry.cHeads, pImage->LCHSGeometry.cSectors);
    }
}

/** @copydoc VBOXHDDBACKEND::pfnAsyncRead */
static int parallelsAsyncRead(void *pBackendData, uint64_t uOffset, size_t cbToRead,
                              PVDIOCTX pIoCtx, size_t *pcbActuallyRead)
{
    LogFlowFunc(("pBackendData=%#p uOffset=%llu pIoCtx=%#p cbToRead=%zu pcbActuallyRead=%#p\n",
                 pBackendData, uOffset, pIoCtx, cbToRead, pcbActuallyRead));
    int rc = VINF_SUCCESS;
    PPARALLELSIMAGE pImage = (PPARALLELSIMAGE)pBackendData;
    uint64_t uSector;
    uint64_t uOffsetInFile;
    uint32_t iIndexInAllocationTable;

    AssertPtr(pImage);
    Assert(uOffset % 512 == 0);
    Assert(cbToRead % 512 == 0);

    if (pImage->uImageFlags & VD_IMAGE_FLAGS_FIXED)
        rc = vdIfIoIntFileReadUserAsync(pImage->pIfIo, pImage->pStorage, uOffset,
                                        pIoCtx, cbToRead);
    else
    {
        /* Calculate offset in the real file. */
        uSector = uOffset / 512;
        /* One chunk in the file is always one track big. */
        iIndexInAllocationTable = (uint32_t)(uSector / pImage->PCHSGeometry.cSectors);
        uSector = uSector % pImage->PCHSGeometry.cSectors;

        cbToRead = RT_MIN(cbToRead, (pImage->PCHSGeometry.cSectors - uSector)*512);

        if (pImage->pAllocationBitmap[iIndexInAllocationTable] == 0)
        {
            rc = VERR_VD_BLOCK_FREE;
        }
        else
        {
            uOffsetInFile = (pImage->pAllocationBitmap[iIndexInAllocationTable] + uSector) * 512;
            rc = vdIfIoIntFileReadUserAsync(pImage->pIfIo, pImage->pStorage, uOffsetInFile,
                                            pIoCtx, cbToRead);
        }
    }

    *pcbActuallyRead = cbToRead;

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnAsyncWrite */
static int parallelsAsyncWrite(void *pBackendData, uint64_t uOffset, size_t cbToWrite,
                               PVDIOCTX pIoCtx,
                               size_t *pcbWriteProcess, size_t *pcbPreRead,
                               size_t *pcbPostRead, unsigned fWrite)
{
    LogFlowFunc(("pBackendData=%#p uOffset=%llu pIoCtx=%#p cbToWrite=%zu pcbWriteProcess=%#p\n",
                 pBackendData, uOffset, pIoCtx, cbToWrite, pcbWriteProcess));
    int rc = VINF_SUCCESS;
    PPARALLELSIMAGE pImage = (PPARALLELSIMAGE)pBackendData;
    uint64_t uSector;
    uint64_t uOffsetInFile;
    uint32_t iIndexInAllocationTable;

    AssertPtr(pImage);
    Assert(uOffset % 512 == 0);
    Assert(cbToWrite % 512 == 0);

    if (pImage->uImageFlags & VD_IMAGE_FLAGS_FIXED)
        rc = vdIfIoIntFileWriteUserAsync(pImage->pIfIo, pImage->pStorage, uOffset,
                                         pIoCtx, cbToWrite, NULL, NULL);
    else
    {
        /* Calculate offset in the real file. */
        uSector = uOffset / 512;
        /* One chunk in the file is always one track big. */
        iIndexInAllocationTable = (uint32_t)(uSector / pImage->PCHSGeometry.cSectors);
        uSector = uSector % pImage->PCHSGeometry.cSectors;

        cbToWrite = RT_MIN(cbToWrite, (pImage->PCHSGeometry.cSectors - uSector)*512);

        if (pImage->pAllocationBitmap[iIndexInAllocationTable] == 0)
        {
            if (fWrite & VD_WRITE_NO_ALLOC)
            {
                *pcbPreRead  = uSector * 512;
                *pcbPostRead = pImage->PCHSGeometry.cSectors * 512 - cbToWrite - *pcbPreRead;

                if (pcbWriteProcess)
                    *pcbWriteProcess = cbToWrite;
                return VERR_VD_BLOCK_FREE;
            }

            /* Allocate new chunk in the file. */
            Assert(uSector == 0);
            AssertMsg(pImage->cbFileCurrent % 512 == 0, ("File size is not a multiple of 512\n"));
            pImage->pAllocationBitmap[iIndexInAllocationTable] = (uint32_t)(pImage->cbFileCurrent / 512);
            pImage->cbFileCurrent += pImage->PCHSGeometry.cSectors * 512;
            pImage->fAllocationBitmapChanged = true;
            uOffsetInFile = (uint64_t)pImage->pAllocationBitmap[iIndexInAllocationTable] * 512;

            /*
             * Write the new block at the current end of the file.
             */
            rc = vdIfIoIntFileWriteUserAsync(pImage->pIfIo, pImage->pStorage,
                                             uOffsetInFile, pIoCtx, cbToWrite, NULL, NULL);
            if (RT_SUCCESS(rc) || (rc == VERR_VD_ASYNC_IO_IN_PROGRESS))
            {
                /* Write the changed allocation bitmap entry. */
                /** @todo: Error handling. */
                rc = vdIfIoIntFileWriteMetaAsync(pImage->pIfIo, pImage->pStorage,
                                                 sizeof(ParallelsHeader) + iIndexInAllocationTable * sizeof(uint32_t),
                                                 &pImage->pAllocationBitmap[iIndexInAllocationTable],
                                                 sizeof(uint32_t), pIoCtx,
                                                 NULL, NULL);
            }

            *pcbPreRead  = 0;
            *pcbPostRead = 0;
        }
        else
        {
            uOffsetInFile = (pImage->pAllocationBitmap[iIndexInAllocationTable] + uSector) * 512;
            rc = vdIfIoIntFileWriteUserAsync(pImage->pIfIo, pImage->pStorage,
                                             uOffsetInFile, pIoCtx, cbToWrite, NULL, NULL);
        }
    }

    if (pcbWriteProcess)
        *pcbWriteProcess = cbToWrite;

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnAsyncFlush */
static int parallelsAsyncFlush(void *pBackendData, PVDIOCTX pIoCtx)
{
    int rc = VINF_SUCCESS;
    PPARALLELSIMAGE pImage = (PPARALLELSIMAGE)pBackendData;

    LogFlowFunc(("pImage=#%p\n", pImage));

    /* Flush the file, everything is up to date already. */
    rc = vdIfIoIntFileFlushAsync(pImage->pIfIo, pImage->pStorage, pIoCtx, NULL, NULL);

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}


VBOXHDDBACKEND g_ParallelsBackend =
{
    /* pszBackendName */
    "Parallels",
    /* cbSize */
    sizeof(VBOXHDDBACKEND),
    /* uBackendCaps */
    VD_CAP_FILE | VD_CAP_ASYNC | VD_CAP_VFS | VD_CAP_CREATE_DYNAMIC | VD_CAP_DIFF,
    /* paFileExtensions */
    s_aParallelsFileExtensions,
    /* paConfigInfo */
    NULL,
    /* hPlugin */
    NIL_RTLDRMOD,
    /* pfnCheckIfValid */
    parallelsCheckIfValid,
    /* pfnOpen */
    parallelsOpen,
    /* pfnCreate */
    parallelsCreate,
    /* pfnRename */
    parallelsRename,
    /* pfnClose */
    parallelsClose,
    /* pfnRead */
    parallelsRead,
    /* pfnWrite */
    parallelsWrite,
    /* pfnFlush */
    parallelsFlush,
    /* pfnGetVersion */
    parallelsGetVersion,
    /* pfnGetSize */
    parallelsGetSize,
    /* pfnGetFileSize */
    parallelsGetFileSize,
    /* pfnGetPCHSGeometry */
    parallelsGetPCHSGeometry,
    /* pfnSetPCHSGeometry */
    parallelsSetPCHSGeometry,
    /* pfnGetLCHSGeometry */
    parallelsGetLCHSGeometry,
    /* pfnSetLCHSGeometry */
    parallelsSetLCHSGeometry,
    /* pfnGetImageFlags */
    parallelsGetImageFlags,
    /* pfnGetOpenFlags */
    parallelsGetOpenFlags,
    /* pfnSetOpenFlags */
    parallelsSetOpenFlags,
    /* pfnGetComment */
    parallelsGetComment,
    /* pfnSetComment */
    parallelsSetComment,
    /* pfnGetUuid */
    parallelsGetUuid,
    /* pfnSetUuid */
    parallelsSetUuid,
    /* pfnGetModificationUuid */
    parallelsGetModificationUuid,
    /* pfnSetModificationUuid */
    parallelsSetModificationUuid,
    /* pfnGetParentUuid */
    parallelsGetParentUuid,
    /* pfnSetParentUuid */
    parallelsSetParentUuid,
    /* pfnGetParentModificationUuid */
    parallelsGetParentModificationUuid,
    /* pfnSetParentModificationUuid */
    parallelsSetParentModificationUuid,
    /* pfnDump */
    parallelsDump,
    /* pfnGetTimeStamp */
    NULL,
    /* pfnGetParentTimeStamp */
    NULL,
    /* pfnSetParentTimeStamp */
    NULL,
    /* pfnGetParentFilename */
    NULL,
    /* pfnSetParentFilename */
    NULL,
    /* pfnAsyncRead */
    parallelsAsyncRead,
    /* pfnAsyncWrite */
    parallelsAsyncWrite,
    /* pfnAsyncFlush */
    parallelsAsyncFlush,
    /* pfnComposeLocation */
    genericFileComposeLocation,
    /* pfnComposeName */
    genericFileComposeName,
    /* pfnCompact */
    NULL,
    /* pfnResize */
    NULL,
    /* pfnDiscard */
    NULL,
    /* pfnAsyncDiscard */
    NULL,
    /* pfnRepair */
    NULL
};
