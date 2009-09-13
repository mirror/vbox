/* $Id$ */
/** @file
 *
 * Parallels hdd disk image, core code.
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
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

#define LOG_GROUP LOG_GROUP_VD_VMDK /** @todo: Logging group */
#include "VBoxHDD-Internal.h"
#include <VBox/err.h>

#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/uuid.h>
#include <iprt/file.h>
#include <iprt/path.h>
#include <iprt/string.h>

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
    /** Pointer to the per-disk VD interface list. */
    PVDINTERFACE         pVDIfsDisk;
    /** Error interface. */
    PVDINTERFACE        pInterfaceError;
    /** Error interface callbacks. */
    PVDINTERFACEERROR   pInterfaceErrorCallbacks;
#ifdef VBOX_WITH_NEW_IO_CODE
    /** Async I/O interface. */
    PVDINTERFACE        pInterfaceAsyncIO;
    /** Async I/O interface callbacks. */
    PVDINTERFACEASYNCIO pInterfaceAsyncIOCallbacks;
#endif

    /** Image file name. */
    const char         *pszFilename;
#ifndef VBOX_WITH_NEW_IO_CODE
    /** File descriptor. */
    RTFILE              File;
#else
    /** Opaque storage handle. */
    void               *pvStorage;
#endif
    /** Open flags passed by VBoxHD layer. */
    unsigned            uOpenFlags;
    /** Image flags defined during creation or determined during open. */
    unsigned            uImageFlags;
    /** Total size of the image. */
    uint64_t            cbSize;
    /** Physical geometry of this image. */
    PDMMEDIAGEOMETRY    PCHSGeometry;
    /** Logical geometry of this image. */
    PDMMEDIAGEOMETRY    LCHSGeometry;
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
static const char *const s_apszParallelsFileExtensions[] =
{
    "hdd",
    NULL
};

/***************************************************
 * Internal functions                              *
 **************************************************/

/**
 * Internal: signal an error to the frontend.
 */
DECLINLINE(int) parallelsError(PPARALLELSIMAGE pImage, int rc, RT_SRC_POS_DECL,
                               const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    if (pImage->pInterfaceError && pImage->pInterfaceErrorCallbacks)
        pImage->pInterfaceErrorCallbacks->pfnError(pImage->pInterfaceError->pvUser, rc, RT_SRC_POS_ARGS,
                                                   pszFormat, va);
    va_end(va);
    return rc;
}

static int parallelsFileOpen(PPARALLELSIMAGE pImage, bool fReadonly, bool fCreate)
{
    int rc = VINF_SUCCESS;

    AssertMsg(!(fReadonly && fCreate), ("Image can't be opened readonly while being created\n"));

#ifndef VBOX_WITH_NEW_IO_CODE
    unsigned uFileFlags = fReadonly ? RTFILE_O_READ      | RTFILE_O_DENY_NONE
                                    : RTFILE_O_READWRITE | RTFILE_O_DENY_WRITE;

    if (fCreate)
        uFileFlags |= RTFILE_O_CREATE;
    else
        uFileFlags |= RTFILE_O_OPEN;

    rc = RTFileOpen(&pImage->File, pImage->pszFilename, uFileFlags);
#else

    unsigned uOpenFlags = fReadonly ? VD_INTERFACEASYNCIO_OPEN_FLAGS_READONLY : 0;

    if (fCreate)
        uOpenFlags |= VD_INTERFACEASYNCIO_OPEN_FLAGS_CREATE;

    rc = pImage->pInterfaceAsyncIOCallbacks->pfnOpen(pImage->pInterfaceAsyncIO->pvUser,
                                                     pImage->pszFilename,
                                                     uOpenFlags,
                                                     NULL, &pImage->pvStorage);
#endif

    return rc;
}

static int parallelsFileClose(PPARALLELSIMAGE pImage)
{
    int rc = VINF_SUCCESS;

#ifndef VBOX_WITH_NEW_IO_CODE
    if (pImage->File != NIL_RTFILE)
        rc = RTFileClose(pImage->File);

    pImage->File = NIL_RTFILE;
#else
    if (pImage->pvStorage)
        rc = pImage->pInterfaceAsyncIOCallbacks->pfnClose(pImage->pInterfaceAsyncIO->pvUser,
                                                          pImage->pvStorage);

    pImage->pvStorage = NULL;
#endif

    return rc;
}

static int parallelsFileFlushSync(PPARALLELSIMAGE pImage)
{
    int rc = VINF_SUCCESS;

#ifndef VBOX_WITH_NEW_IO_CODE
    rc = RTFileFlush(pImage->File);
#else
    if (pImage->pvStorage)
        rc = pImage->pInterfaceAsyncIOCallbacks->pfnFlushSync(pImage->pInterfaceAsyncIO->pvUser,
                                                              pImage->pvStorage);
#endif

    return rc;
}

static int parallelsFileGetSize(PPARALLELSIMAGE pImage, uint64_t *pcbSize)
{
    int rc = VINF_SUCCESS;

#ifndef VBOX_WITH_NEW_IO_CODE
    rc = RTFileGetSize(pImage->File, pcbSize);
#else
    if (pImage->pvStorage)
        rc = pImage->pInterfaceAsyncIOCallbacks->pfnGetSize(pImage->pInterfaceAsyncIO->pvUser,
                                                            pImage->pvStorage,
                                                            pcbSize);
#endif

    return rc;

}

static int parallelsFileSetSize(PPARALLELSIMAGE pImage, uint64_t cbSize)
{
    int rc = VINF_SUCCESS;

#ifndef VBOX_WITH_NEW_IO_CODE
    rc = RTFileSetSize(pImage->File, cbSize);
#else
    if (pImage->pvStorage)
        rc = pImage->pInterfaceAsyncIOCallbacks->pfnSetSize(pImage->pInterfaceAsyncIO->pvUser,
                                                            pImage->pvStorage,
                                                            cbSize);
#endif

    return rc;
}


static int parallelsFileWriteSync(PPARALLELSIMAGE pImage, uint64_t off, const void *pcvBuf, size_t cbWrite, size_t *pcbWritten)
{
    int rc = VINF_SUCCESS;

#ifndef VBOX_WITH_NEW_IO_CODE
    rc = RTFileWriteAt(pImage->File, off, pcvBuf, cbWrite, pcbWritten);
#else
    if (pImage->pvStorage)
        rc = pImage->pInterfaceAsyncIOCallbacks->pfnWriteSync(pImage->pInterfaceAsyncIO->pvUser,
                                                              pImage->pvStorage,
                                                              off, cbWrite, pcvBuf,
                                                              pcbWritten);
#endif

    return rc;
}

static int parallelsFileReadSync(PPARALLELSIMAGE pImage, uint64_t off, void *pvBuf, size_t cbRead, size_t *pcbRead)
{
    int rc = VINF_SUCCESS;

#ifndef VBOX_WITH_NEW_IO_CODE
    rc = RTFileReadAt(pImage->File, off, pvBuf, cbRead, pcbRead);
#else
    if (pImage->pvStorage)
        rc = pImage->pInterfaceAsyncIOCallbacks->pfnReadSync(pImage->pInterfaceAsyncIO->pvUser,
                                                             pImage->pvStorage,
                                                             off, cbRead, pvBuf,
                                                             pcbRead);
#endif

    return rc;
}

static bool parallelsFileOpened(PPARALLELSIMAGE pImage)
{
#ifndef VBOX_WITH_NEW_IO_CODE
    return pImage->File != NIL_RTFILE;
#else
    return pImage->pvStorage != NULL;
#endif
}

static int parallelsOpenImage(PPARALLELSIMAGE pImage, unsigned uOpenFlags)
{
    int rc = VINF_SUCCESS;
    ParallelsHeader parallelsHeader;

    /* Try to get error interface. */
    pImage->pInterfaceError = VDInterfaceGet(pImage->pVDIfsDisk, VDINTERFACETYPE_ERROR);
    if (pImage->pInterfaceError)
        pImage->pInterfaceErrorCallbacks = VDGetInterfaceError(pImage->pInterfaceError);

#ifdef VBOX_WITH_NEW_IO_CODE
    /* Try to get async I/O interface. */
    pImage->pInterfaceAsyncIO = VDInterfaceGet(pImage->pVDIfsDisk, VDINTERFACETYPE_ASYNCIO);
    AssertPtr(pImage->pInterfaceAsyncIO);
    pImage->pInterfaceAsyncIOCallbacks = VDGetInterfaceAsyncIO(pImage->pInterfaceAsyncIO);
    AssertPtr(pImage->pInterfaceAsyncIOCallbacks);
#endif

    rc = parallelsFileOpen(pImage, !!(uOpenFlags & VD_OPEN_FLAGS_READONLY), false);
    if (RT_FAILURE(rc))
        goto out;

    rc = parallelsFileGetSize(pImage, &pImage->cbFileCurrent);
    if (RT_FAILURE(rc))
        goto out;
    AssertMsg(pImage->cbFileCurrent % 512 == 0, ("File size is not a multiple of 512\n"));

    rc = parallelsFileReadSync(pImage, 0, &parallelsHeader, sizeof(parallelsHeader), NULL);
    if (RT_FAILURE(rc))
        goto out;

    if (memcmp(parallelsHeader.HeaderIdentifier, PARALLELS_HEADER_MAGIC, 16))
    {
        /* Check if the file has hdd as extension. It is a fixed size raw image then. */
        char *pszExtension = RTPathExt(pImage->pszFilename);
        if (strcmp(pszExtension, ".hdd"))
        {
            rc = VERR_VD_GEN_INVALID_HEADER;
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

        rc = parallelsFileReadSync(pImage, sizeof(ParallelsHeader),
                                   pImage->pAllocationBitmap,
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

static int parallelsFlushImage(PPARALLELSIMAGE pImage)
{
    LogFlowFunc(("pImage=#%p\n", pImage));
    int rc = VINF_SUCCESS;

    if (   !(pImage->uImageFlags & VD_IMAGE_FLAGS_FIXED)
        && (pImage->fAllocationBitmapChanged))
    {
        pImage->fAllocationBitmapChanged = false;
        /* Write the allocation bitmap to the file. */
        rc = parallelsFileWriteSync(pImage, sizeof(ParallelsHeader),
                                    pImage->pAllocationBitmap,
                                    pImage->cAllocationBitmapEntries * sizeof(uint32_t),
                                    NULL);
        if (RT_FAILURE(rc))
            return rc;
    }

    /* Flush file. */
    rc = parallelsFileFlushSync(pImage);

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

static void parallelsFreeImage(PPARALLELSIMAGE pImage, bool fDelete)
{
    (void)parallelsFlushImage(pImage);

    if (pImage->pAllocationBitmap)
        RTMemFree(pImage->pAllocationBitmap);

    if (parallelsFileOpened(pImage))
        parallelsFileClose(pImage);
}

/** @copydoc VBOXHDDBACKEND::pfnCheckIfValid */
static int parallelsCheckIfValid(const char *pszFilename, PVDINTERFACE pVDIfsDisk)
{
    RTFILE File;
    ParallelsHeader parallelsHeader;
    int rc;

    rc = RTFileOpen(&File, pszFilename, RTFILE_O_READ);
    if (RT_FAILURE(rc))
        return VERR_VD_GEN_INVALID_HEADER;

    rc = RTFileReadAt(File, 0, &parallelsHeader, sizeof(ParallelsHeader), NULL);
    if (RT_FAILURE(rc))
    {
        rc = VERR_VD_GEN_INVALID_HEADER;
    }
    else
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

            rc = RTFileGetSize(File, &cbFile);
            if (RT_FAILURE(rc) || ((cbFile % 512) != 0))
            {
                RTFileClose(File);
                return VERR_VD_GEN_INVALID_HEADER;
            }

            pszExtension = RTPathExt(pszFilename);
            if (!pszExtension || strcmp(pszExtension, ".hdd"))
                rc = VERR_VD_GEN_INVALID_HEADER;
            else
                rc = VINF_SUCCESS;
        }
    }

    RTFileClose(File);
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnOpen */
static int parallelsOpen(const char *pszFilename, unsigned uOpenFlags,
                         PVDINTERFACE pVDIfsDisk, PVDINTERFACE pVDIfsImage,
                         void **ppBackendData)
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
        || !*pszFilename
        || strchr(pszFilename, '"'))
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    if (uOpenFlags & VD_OPEN_FLAGS_ASYNC_IO)
    {
        rc = VERR_NOT_SUPPORTED;
        goto out;
    }

    pImage = (PPARALLELSIMAGE)RTMemAllocZ(sizeof(PARALLELSIMAGE));
    if (!pImage)
    {
        rc = VERR_NO_MEMORY;
        goto out;
    }

#ifndef VBOX_WITH_NEW_IO_CODE
    pImage->File = NIL_RTFILE;
#else
    pImage->pvStorage = NULL;
#endif
    pImage->fAllocationBitmapChanged = false;
    pImage->pszFilename = pszFilename;
    pImage->pVDIfsDisk = pVDIfsDisk;

    rc = parallelsOpenImage(pImage, uOpenFlags);
    if (RT_SUCCESS(rc))
        *ppBackendData = pImage;

out:
    LogFlowFunc(("returns %Rrc (pBackendData=%#p)\n", rc, *ppBackendData));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnCreate */
static int parallelsCreate(const char *pszFilename, uint64_t cbSize,
                           unsigned uImageFlags, const char *pszComment,
                           PCPDMMEDIAGEOMETRY pPCHSGeometry,
                           PCPDMMEDIAGEOMETRY pLCHSGeometry, PCRTUUID pUuid,
                           unsigned uOpenFlags, unsigned uPercentStart,
                           unsigned uPercentSpan, PVDINTERFACE pVDIfsDisk,
                           PVDINTERFACE pVDIfsImage, PVDINTERFACE pVDIfsOperation,
                           void **ppBackendData)
{
    LogFlowFunc(("pszFilename=\"%s\" cbSize=%llu uImageFlags=%#x pszComment=\"%s\" pPCHSGeometry=%#p pLCHSGeometry=%#p Uuid=%RTuuid uOpenFlags=%#x uPercentStart=%u uPercentSpan=%u pVDIfsDisk=%#p pVDIfsImage=%#p pVDIfsOperation=%#p ppBackendData=%#p", pszFilename, cbSize, uImageFlags, pszComment, pPCHSGeometry, pLCHSGeometry, pUuid, uOpenFlags, uPercentStart, uPercentSpan, pVDIfsDisk, pVDIfsImage, pVDIfsOperation, ppBackendData));
    return VERR_NOT_IMPLEMENTED;
}

/** @copydoc VBOXHDDBACKEND::pfnRename */
static int parallelsRename(void *pBackendData, const char *pszFilename)
{
    LogFlowFunc(("pBackendData=%#p pszFilename=%#p\n", pBackendData, pszFilename));
    return VERR_NOT_IMPLEMENTED;
}

/** @copydoc VBOXHDDBACKEND::pfnClose */
static int parallelsClose(void *pBackendData, bool fDelete)
{
    LogFlowFunc(("pBackendData=%#p fDelete=%d\n", pBackendData, fDelete));
    PPARALLELSIMAGE pImage = (PPARALLELSIMAGE)pBackendData;
    int rc = VINF_SUCCESS;

    /* Freeing a never allocated image (e.g. because the open failed) is
     * not signalled as an error. After all nothing bad happens. */
    if (pImage)
        parallelsFreeImage(pImage, fDelete);

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnRead */
static int parallelsRead(void *pBackendData, uint64_t uOffset, void *pvBuf,
                         size_t cbToRead, size_t *pcbActuallyRead)
{
    LogFlowFunc(("pBackendData=%#p uOffset=%llu pvBuf=%#p cbToRead=%zu pcbActuallyRead=%#p\n", pBackendData, uOffset, pvBuf, cbToRead, pcbActuallyRead));
    int rc = VINF_SUCCESS;
    PPARALLELSIMAGE pImage = (PPARALLELSIMAGE)pBackendData;
    uint64_t uSector;
    uint64_t uOffsetInFile;
    uint32_t iIndexInAllocationTable;

    Assert(pImage);
    Assert(uOffset % 512 == 0);
    Assert(cbToRead % 512 == 0);

    if (pImage->uImageFlags & VD_IMAGE_FLAGS_FIXED)
    {
        rc = parallelsFileReadSync(pImage, uOffset,
                                   pvBuf, cbToRead, NULL);
    }
    else
    {
        /** Calculate offset in the real file. */
        uSector = uOffset / 512;
        /** One chunk in the file is always one track big. */
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
            rc = parallelsFileReadSync(pImage, uOffsetInFile,
                                       pvBuf, cbToRead, NULL);
        }
    }

    *pcbActuallyRead = cbToRead;

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnWrite */
static int parallelsWrite(void *pBackendData, uint64_t uOffset, const void *pvBuf,
                          size_t cbToWrite, size_t *pcbWriteProcess,
                          size_t *pcbPreRead, size_t *pcbPostRead, unsigned fWrite)
{
    LogFlowFunc(("pBackendData=%#p uOffset=%llu pvBuf=%#p cbToWrite=%zu pcbWriteProcess=%#p\n", pBackendData, uOffset, pvBuf, cbToWrite, pcbWriteProcess));
    int rc = VINF_SUCCESS;
    PPARALLELSIMAGE pImage = (PPARALLELSIMAGE)pBackendData;
    uint64_t uSector;
    uint64_t uOffsetInFile;
    uint32_t iIndexInAllocationTable;

    Assert(pImage);
    Assert(uOffset % 512 == 0);
    Assert(cbToWrite % 512 == 0);

    if (pImage->uImageFlags & VD_IMAGE_FLAGS_FIXED)
    {
        rc = parallelsFileWriteSync(pImage, uOffset,
                                    pvBuf, cbToWrite, NULL);
    }
    else
    {
        /** Calculate offset in the real file. */
        uSector = uOffset / 512;
        /** One chunk in the file is always one track big. */
        iIndexInAllocationTable = (uint32_t)(uSector / pImage->PCHSGeometry.cSectors);
        uSector = uSector % pImage->PCHSGeometry.cSectors;

        cbToWrite = RT_MIN(cbToWrite, (pImage->PCHSGeometry.cSectors - uSector)*512);

        if (pImage->pAllocationBitmap[iIndexInAllocationTable] == 0)
        {
            /* Allocate new chunk in the file. */
            AssertMsg(pImage->cbFileCurrent % 512 == 0, ("File size is not a multiple of 512\n"));
            pImage->pAllocationBitmap[iIndexInAllocationTable] = (uint32_t)(pImage->cbFileCurrent / 512);
            pImage->cbFileCurrent += pImage->PCHSGeometry.cSectors * 512;
            pImage->fAllocationBitmapChanged = true;

            uint8_t *pNewBlock = (uint8_t *)RTMemAllocZ(pImage->PCHSGeometry.cSectors * 512);

            if (!pNewBlock)
                return VERR_NO_MEMORY;

            uOffsetInFile = (uint64_t)pImage->pAllocationBitmap[iIndexInAllocationTable] * 512;
            memcpy(pNewBlock + (uOffset - ((uint64_t)iIndexInAllocationTable * pImage->PCHSGeometry.cSectors * 512)),
                   pvBuf, cbToWrite);

            /*
             * Write the new block at the current end of the file.
             */
            rc = parallelsFileWriteSync(pImage, uOffsetInFile,
                                        pNewBlock,
                                        pImage->PCHSGeometry.cSectors * 512, NULL);

            RTMemFree(pNewBlock);
        }
        else
        {
            uOffsetInFile = (pImage->pAllocationBitmap[iIndexInAllocationTable] + uSector) * 512;
            rc = parallelsFileWriteSync(pImage, uOffsetInFile,
                                        pvBuf, cbToWrite, NULL);
        }
    }

    *pcbWriteProcess = cbToWrite;

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;

}

static int parallelsFlush(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PPARALLELSIMAGE pImage = (PPARALLELSIMAGE)pBackendData;
    int rc = VINF_SUCCESS;

    Assert(pImage);

    rc = parallelsFlushImage(pImage);

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetVersion */
static unsigned parallelsGetVersion(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PPARALLELSIMAGE pImage = (PPARALLELSIMAGE)pBackendData;

    Assert(pImage);

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

    Assert(pImage);

    if (pImage)
        return pImage->cbSize;
    else
        return 0;
}

/** @copydoc VBOXHDDBACKEND::pfnGetFileSize */
static uint64_t parallelsGetFileSize(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PPARALLELSIMAGE pImage = (PPARALLELSIMAGE)pBackendData;
    uint64_t cb = 0;

    Assert(pImage);

    if (pImage)
    {
        if (parallelsFileOpened(pImage))
            cb = pImage->cbFileCurrent;
    }

    LogFlowFunc(("returns %lld\n", cb));
    return cb;
}

/** @copydoc VBOXHDDBACKEND::pfnGetPCHSGeometry */
static int parallelsGetPCHSGeometry(void *pBackendData,
                                    PPDMMEDIAGEOMETRY pPCHSGeometry)
{
    LogFlowFunc(("pBackendData=%#p pPCHSGeometry=%#p\n", pBackendData, pPCHSGeometry));
    PPARALLELSIMAGE pImage = (PPARALLELSIMAGE)pBackendData;
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
            rc = VERR_VD_GEOMETRY_NOT_SET;
    }
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc (PCHS=%u/%u/%u)\n", rc, pPCHSGeometry->cCylinders, pPCHSGeometry->cHeads, pPCHSGeometry->cSectors));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetPCHSGeometry */
static int parallelsSetPCHSGeometry(void *pBackendData,
                                    PCPDMMEDIAGEOMETRY pPCHSGeometry)
{
    LogFlowFunc(("pBackendData=%#p pPCHSGeometry=%#p PCHS=%u/%u/%u\n", pBackendData, pPCHSGeometry, pPCHSGeometry->cCylinders, pPCHSGeometry->cHeads, pPCHSGeometry->cSectors));
    PPARALLELSIMAGE pImage = (PPARALLELSIMAGE)pBackendData;
    int rc;

    Assert(pImage);

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
                                    PPDMMEDIAGEOMETRY pLCHSGeometry)
{
    LogFlowFunc(("pBackendData=%#p pLCHSGeometry=%#p\n", pBackendData, pLCHSGeometry));
    PPARALLELSIMAGE pImage = (PPARALLELSIMAGE)pBackendData;
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
            rc = VERR_VD_GEOMETRY_NOT_SET;
    }
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc (LCHS=%u/%u/%u)\n", rc, pLCHSGeometry->cCylinders, pLCHSGeometry->cHeads, pLCHSGeometry->cSectors));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetLCHSGeometry */
static int parallelsSetLCHSGeometry(void *pBackendData,
                                    PCPDMMEDIAGEOMETRY pLCHSGeometry)
{
    LogFlowFunc(("pBackendData=%#p pLCHSGeometry=%#p LCHS=%u/%u/%u\n", pBackendData, pLCHSGeometry, pLCHSGeometry->cCylinders, pLCHSGeometry->cHeads, pLCHSGeometry->cSectors));
    PPARALLELSIMAGE pImage = (PPARALLELSIMAGE)pBackendData;
    int rc;

    Assert(pImage);

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

    Assert(pImage);

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

    Assert(pImage);

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

    /* Image must be opened and the new flags must be valid. Just readonly and
     * info flags are supported. */
    if (!pImage || (uOpenFlags & ~(VD_OPEN_FLAGS_READONLY | VD_OPEN_FLAGS_INFO)))
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

    Assert(pImage);

    if (pImage)
    {
        rc = VERR_NOT_SUPPORTED;
    }
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

    Assert(pImage);

    if (pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
    {
        rc = VERR_VD_IMAGE_READ_ONLY;
        goto out;
    }

    if (pImage)
        rc = VINF_SUCCESS;
    else
        rc = VERR_VD_NOT_OPENED;

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetUuid */
static int parallelsGetUuid(void *pBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p pUuid=%#p\n", pBackendData, pUuid));
    PPARALLELSIMAGE pImage = (PPARALLELSIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
    {
        rc = VERR_NOT_SUPPORTED;
    }
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

    LogFlowFunc(("%RTuuid\n", pUuid));
    Assert(pImage);

    if (pImage)
    {
        if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
        {
            rc = VERR_NOT_SUPPORTED;
        }
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

    Assert(pImage);

    if (pImage)
    {
        rc = VERR_NOT_SUPPORTED;
    }
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

    Assert(pImage);

    if (pImage)
    {
        if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
        {
            rc = VERR_NOT_SUPPORTED;
        }
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

    Assert(pImage);

    if (pImage)
    {
        rc = VINF_SUCCESS;
    }
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

    Assert(pImage);

    if (pImage)
    {
        if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
        {
            rc = VERR_NOT_SUPPORTED;
        }
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

    Assert(pImage);

    if (pImage)
    {
        rc = VERR_NOT_SUPPORTED;
    }
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

    Assert(pImage);

    if (pImage)
    {
        if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
        {
            rc = VERR_NOT_SUPPORTED;
        }
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

    Assert(pImage);
    if (pImage)
    {
        pImage->pInterfaceErrorCallbacks->pfnMessage(pImage->pInterfaceError->pvUser, "Header: Geometry PCHS=%u/%u/%u LCHS=%u/%u/%u\n",
                    pImage->PCHSGeometry.cCylinders, pImage->PCHSGeometry.cHeads, pImage->PCHSGeometry.cSectors,
                    pImage->LCHSGeometry.cCylinders, pImage->LCHSGeometry.cHeads, pImage->LCHSGeometry.cSectors);
    }
}


static int parallelsGetTimeStamp(void *pvBackendData, PRTTIMESPEC pTimeStamp)
{
    int rc = VERR_NOT_IMPLEMENTED;
    LogFlow(("%s: returned %Rrc\n", __FUNCTION__, rc));
    return rc;
}

static int parallelsGetParentTimeStamp(void *pvBackendData, PRTTIMESPEC pTimeStamp)
{
    int rc = VERR_NOT_IMPLEMENTED;
    LogFlow(("%s: returned %Rrc\n", __FUNCTION__, rc));
    return rc;
}

static int parallelsSetParentTimeStamp(void *pvBackendData, PCRTTIMESPEC pTimeStamp)
{
    int rc = VERR_NOT_IMPLEMENTED;
    LogFlow(("%s: returned %Rrc\n", __FUNCTION__, rc));
    return rc;
}

static int parallelsGetParentFilename(void *pvBackendData, char **ppszParentFilename)
{
    int rc = VERR_NOT_IMPLEMENTED;
    LogFlow(("%s: returned %Rrc\n", __FUNCTION__, rc));
    return rc;
}

static int parallelsSetParentFilename(void *pvBackendData, const char *pszParentFilename)
{
    int rc = VERR_NOT_IMPLEMENTED;
    LogFlow(("%s: returned %Rrc\n", __FUNCTION__, rc));
    return rc;
}

static bool parallelsIsAsyncIOSupported(void *pvBackendData)
{
    return false;
}

static int parallelsAsyncRead(void *pvBackendData, uint64_t uOffset, size_t cbRead,
                              PPDMDATASEG paSeg, unsigned cSeg, void *pvUser)
{
    return VERR_NOT_SUPPORTED;
}

static int parallelsAsyncWrite(void *pvBackendData, uint64_t uOffset, size_t cbWrite,
                               PPDMDATASEG paSeg, unsigned cSeg, void *pvUser)
{
    return VERR_NOT_SUPPORTED;
}

VBOXHDDBACKEND g_ParallelsBackend =
{
    /* pszBackendName */
    "Parallels",
    /* cbSize */
    sizeof(VBOXHDDBACKEND),
    /* uBackendCaps */
    VD_CAP_FILE | VD_CAP_CREATE_FIXED | VD_CAP_CREATE_DYNAMIC,
    /* papszFileExtensions */
    s_apszParallelsFileExtensions,
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
    parallelsGetTimeStamp,
    /* pfnGetParentTimeStamp */
    parallelsGetParentTimeStamp,
    /* pfnSetParentTimeStamp */
    parallelsSetParentTimeStamp,
    /* pfnGetParentFilename */
    parallelsGetParentFilename,
    /* pfnSetParentFilename */
    parallelsSetParentFilename,
    /* pfnIsAsyncIOSupported */
    parallelsIsAsyncIOSupported,
    /* pfnAsyncRead */
    parallelsAsyncRead,
    /* pfnAsyncWrite */
    parallelsAsyncWrite,
    /* pfnComposeLocation */
    genericFileComposeLocation,
    /* pfnComposeName */
    genericFileComposeName
};

