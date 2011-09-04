/* $Id$ */
/** @file
 * VHD Disk image, Core Code.
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
#define LOG_GROUP LOG_GROUP_VD_VHD
#include <VBox/vd-plugin.h>
#include <VBox/err.h>

#include <VBox/log.h>
#include <VBox/version.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/uuid.h>
#include <iprt/path.h>
#include <iprt/string.h>

#define VHD_RELATIVE_MAX_PATH 512
#define VHD_ABSOLUTE_MAX_PATH 512

#define VHD_SECTOR_SIZE 512
#define VHD_BLOCK_SIZE  (2 * _1M)

/* This is common to all VHD disk types and is located at the end of the image */
#pragma pack(1)
typedef struct VHDFooter
{
    char     Cookie[8];
    uint32_t Features;
    uint32_t Version;
    uint64_t DataOffset;
    uint32_t TimeStamp;
    uint8_t  CreatorApp[4];
    uint32_t CreatorVer;
    uint32_t CreatorOS;
    uint64_t OrigSize;
    uint64_t CurSize;
    uint16_t DiskGeometryCylinder;
    uint8_t  DiskGeometryHeads;
    uint8_t  DiskGeometrySectors;
    uint32_t DiskType;
    uint32_t Checksum;
    char     UniqueID[16];
    uint8_t  SavedState;
    uint8_t  Reserved[427];
} VHDFooter;
#pragma pack()

/* this really is spelled with only one n */
#define VHD_FOOTER_COOKIE "conectix"
#define VHD_FOOTER_COOKIE_SIZE 8

#define VHD_FOOTER_FEATURES_NOT_ENABLED   0
#define VHD_FOOTER_FEATURES_TEMPORARY     1
#define VHD_FOOTER_FEATURES_RESERVED      2

#define VHD_FOOTER_FILE_FORMAT_VERSION    0x00010000
#define VHD_FOOTER_DATA_OFFSET_FIXED      UINT64_C(0xffffffffffffffff)
#define VHD_FOOTER_DISK_TYPE_FIXED        2
#define VHD_FOOTER_DISK_TYPE_DYNAMIC      3
#define VHD_FOOTER_DISK_TYPE_DIFFERENCING 4

#define VHD_MAX_LOCATOR_ENTRIES           8
#define VHD_PLATFORM_CODE_NONE            0
#define VHD_PLATFORM_CODE_WI2R            0x57693272
#define VHD_PLATFORM_CODE_WI2K            0x5769326B
#define VHD_PLATFORM_CODE_W2RU            0x57327275
#define VHD_PLATFORM_CODE_W2KU            0x57326B75
#define VHD_PLATFORM_CODE_MAC             0x4D163220
#define VHD_PLATFORM_CODE_MACX            0x4D163258

/* Header for expanding disk images. */
#pragma pack(1)
typedef struct VHDParentLocatorEntry
{
    uint32_t u32Code;
    uint32_t u32DataSpace;
    uint32_t u32DataLength;
    uint32_t u32Reserved;
    uint64_t u64DataOffset;
} VHDPLE, *PVHDPLE;

typedef struct VHDDynamicDiskHeader
{
    char     Cookie[8];
    uint64_t DataOffset;
    uint64_t TableOffset;
    uint32_t HeaderVersion;
    uint32_t MaxTableEntries;
    uint32_t BlockSize;
    uint32_t Checksum;
    uint8_t  ParentUuid[16];
    uint32_t ParentTimeStamp;
    uint32_t Reserved0;
    uint16_t ParentUnicodeName[256];
    VHDPLE   ParentLocatorEntry[VHD_MAX_LOCATOR_ENTRIES];
    uint8_t  Reserved1[256];
} VHDDynamicDiskHeader;
#pragma pack()

#define VHD_DYNAMIC_DISK_HEADER_COOKIE "cxsparse"
#define VHD_DYNAMIC_DISK_HEADER_COOKIE_SIZE 8
#define VHD_DYNAMIC_DISK_HEADER_VERSION 0x00010000

/**
 * Complete VHD image data structure.
 */
typedef struct VHDIMAGE
{
    /** Image file name. */
    const char       *pszFilename;
    /** Opaque storage handle. */
    PVDIOSTORAGE      pStorage;

    /** Pointer to the per-disk VD interface list. */
    PVDINTERFACE      pVDIfsDisk;
    /** Pointer to the per-image VD interface list. */
    PVDINTERFACE      pVDIfsImage;
    /** Error interface. */
    PVDINTERFACEERROR pIfError;
    /** I/O interface. */
    PVDINTERFACEIOINT pIfIo;

    /** Open flags passed by VBoxHDD layer. */
    unsigned        uOpenFlags;
    /** Image flags defined during creation or determined during open. */
    unsigned        uImageFlags;
    /** Total size of the image. */
    uint64_t        cbSize;

    /** Physical geometry of this image. */
    VDGEOMETRY      PCHSGeometry;
    /** Logical geometry of this image. */
    VDGEOMETRY      LCHSGeometry;

    /** Image UUID. */
    RTUUID          ImageUuid;
    /** Parent image UUID. */
    RTUUID          ParentUuid;

    /** Parent's time stamp at the time of image creation. */
    uint32_t        u32ParentTimeStamp;
    /** Relative path to the parent image. */
    char            *pszParentFilename;

    /** The Block Allocation Table. */
    uint32_t        *pBlockAllocationTable;
    /** Number of entries in the table. */
    uint32_t        cBlockAllocationTableEntries;

    /** Size of one data block. */
    uint32_t        cbDataBlock;
    /** Sectors per data block. */
    uint32_t        cSectorsPerDataBlock;
    /** Length of the sector bitmap in bytes. */
    uint32_t        cbDataBlockBitmap;
    /** A copy of the disk footer. */
    VHDFooter       vhdFooterCopy;
    /** Current end offset of the file (without the disk footer). */
    uint64_t        uCurrentEndOfFile;
    /** Size of the data block bitmap in sectors. */
    uint32_t        cDataBlockBitmapSectors;
    /** Start of the block allocation table. */
    uint64_t        uBlockAllocationTableOffset;
    /** Buffer to hold block's bitmap for bit search operations. */
    uint8_t         *pu8Bitmap;
    /** Offset to the next data structure (dynamic disk header). */
    uint64_t        u64DataOffset;
    /** Flag to force dynamic disk header update. */
    bool            fDynHdrNeedsUpdate;
} VHDIMAGE, *PVHDIMAGE;

/**
 * Structure tracking the expansion process of the image
 * for async access.
 */
typedef struct VHDIMAGEEXPAND
{
    /** Flag indicating the status of each step. */
    volatile uint32_t fFlags;
    /** The index in the  block allocation table which is written. */
    uint32_t          idxBatAllocated;
    /** Big endian representation of the block index
     * which is written in the BAT. */
    uint32_t          idxBlockBe;
    /** Old end of the file - used for rollback in case of an error. */
    uint64_t          cbEofOld;
    /** Sector bitmap written to the new block - variable in size. */
    uint8_t           au8Bitmap[1];
} VHDIMAGEEXPAND, *PVHDIMAGEEXPAND;

/**
 * Flag defines
 */
#define VHDIMAGEEXPAND_STEP_IN_PROGRESS (0x0)
#define VHDIMAGEEXPAND_STEP_FAILED      (0x2)
#define VHDIMAGEEXPAND_STEP_SUCCESS     (0x3)
/** All steps completed successfully. */
#define VHDIMAGEEXPAND_ALL_SUCCESS      (0xff)
/** All steps completed (no success indicator) */
#define VHDIMAGEEXPAND_ALL_COMPLETE     (0xaa)

/** Every status field has 2 bits so we can encode 4 steps in one byte. */
#define VHDIMAGEEXPAND_STATUS_MASK              0x03
#define VHDIMAGEEXPAND_BLOCKBITMAP_STATUS_SHIFT 0x00
#define VHDIMAGEEXPAND_USERBLOCK_STATUS_SHIFT   0x02
#define VHDIMAGEEXPAND_FOOTER_STATUS_SHIFT      0x04
#define VHDIMAGEEXPAND_BAT_STATUS_SHIFT         0x06

/**
 * Helper macros to get and set the status field.
 */
#define VHDIMAGEEXPAND_STATUS_GET(fFlags, cShift) \
    (((fFlags) >> (cShift)) & VHDIMAGEEXPAND_STATUS_MASK)
#define VHDIMAGEEXPAND_STATUS_SET(fFlags, cShift, uVal) \
    ASMAtomicOrU32(&(fFlags), ((uVal) & VHDIMAGEEXPAND_STATUS_MASK) << (cShift))

/*******************************************************************************
*   Static Variables                                                           *
*******************************************************************************/

/** NULL-terminated array of supported file extensions. */
static const VDFILEEXTENSION s_aVhdFileExtensions[] =
{
    {"vhd", VDTYPE_HDD},
    {NULL, VDTYPE_INVALID}
};

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/


/**
 * Internal: Compute and update header checksum.
 */
static uint32_t vhdChecksum(void *pHeader, uint32_t cbSize)
{
    uint32_t checksum = 0;
    for (uint32_t i = 0; i < cbSize; i++)
        checksum += ((unsigned char *)pHeader)[i];
    return ~checksum;
}

/**
 * Internal: Convert filename to UTF16 with appropriate endianness.
 */
static int vhdFilenameToUtf16(const char *pszFilename, uint16_t *pu16Buf,
                              uint32_t cbBufSize, uint32_t *pcbActualSize,
                              bool fBigEndian)
{
    int      rc;
    PRTUTF16 tmp16 = NULL;
    size_t   cTmp16Len;

    rc = RTStrToUtf16(pszFilename, &tmp16);
    if (RT_FAILURE(rc))
        goto out;
    cTmp16Len = RTUtf16Len(tmp16);
    if (cTmp16Len * sizeof(*tmp16) > cbBufSize)
    {
        rc = VERR_FILENAME_TOO_LONG;
        goto out;
    }

    if (fBigEndian)
        for (unsigned i = 0; i < cTmp16Len; i++)
            pu16Buf[i] = RT_H2BE_U16(tmp16[i]);
    else
        memcpy(pu16Buf, tmp16, cTmp16Len * sizeof(*tmp16));
    if (pcbActualSize)
        *pcbActualSize = (uint32_t)(cTmp16Len * sizeof(*tmp16));

out:
    if (tmp16)
        RTUtf16Free(tmp16);
    return rc;
}

/**
 * Internal: Update one locator entry.
 */
static int vhdLocatorUpdate(PVHDIMAGE pImage, PVHDPLE pLocator, const char *pszFilename)
{
    int      rc;
    uint32_t cb, cbMaxLen = RT_BE2H_U32(pLocator->u32DataSpace) * VHD_SECTOR_SIZE;
    void     *pvBuf = RTMemTmpAllocZ(cbMaxLen);
    char     *pszTmp;

    if (!pvBuf)
    {
        rc = VERR_NO_MEMORY;
        goto out;
    }

    switch (RT_BE2H_U32(pLocator->u32Code))
    {
        case VHD_PLATFORM_CODE_WI2R:
            /* Update plain relative name. */
            cb = (uint32_t)strlen(pszFilename);
            if (cb > cbMaxLen)
            {
                rc = VERR_FILENAME_TOO_LONG;
                goto out;
            }
            memcpy(pvBuf, pszFilename, cb);
            pLocator->u32DataLength = RT_H2BE_U32(cb);
            break;
        case VHD_PLATFORM_CODE_WI2K:
            /* Update plain absolute name. */
            rc = RTPathAbs(pszFilename, (char *)pvBuf, cbMaxLen);
            if (RT_FAILURE(rc))
                goto out;
            pLocator->u32DataLength = RT_H2BE_U32((uint32_t)strlen((const char *)pvBuf));
            break;
        case VHD_PLATFORM_CODE_W2RU:
            /* Update unicode relative name. */
            rc = vhdFilenameToUtf16(pszFilename, (uint16_t *)pvBuf, cbMaxLen, &cb, false);
            if (RT_FAILURE(rc))
                goto out;
            pLocator->u32DataLength = RT_H2BE_U32(cb);
            break;
        case VHD_PLATFORM_CODE_W2KU:
            /* Update unicode absolute name. */
            pszTmp = (char*)RTMemTmpAllocZ(cbMaxLen);
            if (!pszTmp)
            {
                rc = VERR_NO_MEMORY;
                goto out;
            }
            rc = RTPathAbs(pszFilename, pszTmp, cbMaxLen);
            if (RT_FAILURE(rc))
            {
                RTMemTmpFree(pszTmp);
                goto out;
            }
            rc = vhdFilenameToUtf16(pszTmp, (uint16_t *)pvBuf, cbMaxLen, &cb, false);
            RTMemTmpFree(pszTmp);
            if (RT_FAILURE(rc))
                goto out;
            pLocator->u32DataLength = RT_H2BE_U32(cb);
            break;
        default:
            rc = VERR_NOT_IMPLEMENTED;
            goto out;
    }
    rc = vdIfIoIntFileWriteSync(pImage->pIfIo, pImage->pStorage,
                                RT_BE2H_U64(pLocator->u64DataOffset),
                                pvBuf, RT_BE2H_U32(pLocator->u32DataSpace) * VHD_SECTOR_SIZE,
                                NULL);

out:
    if (pvBuf)
        RTMemTmpFree(pvBuf);
    return rc;
}

/**
 * Internal: Update dynamic disk header from VHDIMAGE.
 */
static int vhdDynamicHeaderUpdate(PVHDIMAGE pImage)
{
    VHDDynamicDiskHeader ddh;
    int rc, i;

    if (!pImage)
        return VERR_VD_NOT_OPENED;

    rc = vdIfIoIntFileReadSync(pImage->pIfIo, pImage->pStorage,
                               pImage->u64DataOffset, &ddh, sizeof(ddh), NULL);
    if (RT_FAILURE(rc))
        return rc;
    if (memcmp(ddh.Cookie, VHD_DYNAMIC_DISK_HEADER_COOKIE, VHD_DYNAMIC_DISK_HEADER_COOKIE_SIZE) != 0)
        return VERR_VD_VHD_INVALID_HEADER;

    uint32_t u32Checksum = RT_BE2H_U32(ddh.Checksum);
    ddh.Checksum = 0;
    if (u32Checksum != vhdChecksum(&ddh, sizeof(ddh)))
        return VERR_VD_VHD_INVALID_HEADER;

    /* Update parent's timestamp. */
    ddh.ParentTimeStamp = RT_H2BE_U32(pImage->u32ParentTimeStamp);
    /* Update parent's filename. */
    if (pImage->pszParentFilename)
    {
        rc = vhdFilenameToUtf16(RTPathFilename(pImage->pszParentFilename),
             ddh.ParentUnicodeName, sizeof(ddh.ParentUnicodeName) - 1, NULL, true);
        if (RT_FAILURE(rc))
            return rc;
    }

    /* Update parent's locators. */
    for (i = 0; i < VHD_MAX_LOCATOR_ENTRIES; i++)
    {
        /* Skip empty locators */
        if (ddh.ParentLocatorEntry[i].u32Code != RT_H2BE_U32(VHD_PLATFORM_CODE_NONE))
        {
            if (pImage->pszParentFilename)
            {
                rc = vhdLocatorUpdate(pImage, &ddh.ParentLocatorEntry[i], pImage->pszParentFilename);
                if (RT_FAILURE(rc))
                    return rc;
            }
            else
            {
                /* The parent was deleted. */
                ddh.ParentLocatorEntry[i].u32Code = RT_H2BE_U32(VHD_PLATFORM_CODE_NONE);
            }
        }
    }
    /* Update parent's UUID */
    memcpy(ddh.ParentUuid, pImage->ParentUuid.au8, sizeof(ddh.ParentUuid));

    /* Update data offset and number of table entries. */
    ddh.MaxTableEntries = RT_H2BE_U32(pImage->cBlockAllocationTableEntries);

    ddh.Checksum = 0;
    ddh.Checksum = RT_H2BE_U32(vhdChecksum(&ddh, sizeof(ddh)));
    rc = vdIfIoIntFileWriteSync(pImage->pIfIo, pImage->pStorage,
                                pImage->u64DataOffset, &ddh, sizeof(ddh), NULL);
    return rc;
}

/**
 * Internal: Update the VHD footer.
 */
static int vhdUpdateFooter(PVHDIMAGE pImage)
{
    int rc = VINF_SUCCESS;

    /* Update fields which can change. */
    pImage->vhdFooterCopy.CurSize              = RT_H2BE_U64(pImage->cbSize);
    pImage->vhdFooterCopy.DiskGeometryCylinder = RT_H2BE_U16(pImage->PCHSGeometry.cCylinders);
    pImage->vhdFooterCopy.DiskGeometryHeads    = pImage->PCHSGeometry.cHeads;
    pImage->vhdFooterCopy.DiskGeometrySectors  = pImage->PCHSGeometry.cSectors;

    pImage->vhdFooterCopy.Checksum = 0;
    pImage->vhdFooterCopy.Checksum = RT_H2BE_U32(vhdChecksum(&pImage->vhdFooterCopy, sizeof(VHDFooter)));

    if (pImage->pBlockAllocationTable)
        rc = vdIfIoIntFileWriteSync(pImage->pIfIo, pImage->pStorage, 0,
                                    &pImage->vhdFooterCopy, sizeof(VHDFooter), NULL);

    if (RT_SUCCESS(rc))
        rc = vdIfIoIntFileWriteSync(pImage->pIfIo, pImage->pStorage,
                                    pImage->uCurrentEndOfFile, &pImage->vhdFooterCopy,
                                    sizeof(VHDFooter), NULL);

    return rc;
}

/**
 * Internal. Flush image data to disk.
 */
static int vhdFlushImage(PVHDIMAGE pImage)
{
    int rc = VINF_SUCCESS;

    if (pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
        return VINF_SUCCESS;

    if (pImage->pBlockAllocationTable)
    {
        /*
         * This is an expanding image. Write the BAT and copy of the disk footer.
         */
        size_t   cbBlockAllocationTableToWrite = pImage->cBlockAllocationTableEntries * sizeof(uint32_t);
        uint32_t *pBlockAllocationTableToWrite = (uint32_t *)RTMemAllocZ(cbBlockAllocationTableToWrite);

        if (!pBlockAllocationTableToWrite)
            return VERR_NO_MEMORY;

        /*
         * The BAT entries have to be stored in big endian format.
         */
        for (unsigned i = 0; i < pImage->cBlockAllocationTableEntries; i++)
            pBlockAllocationTableToWrite[i] = RT_H2BE_U32(pImage->pBlockAllocationTable[i]);

        /*
         * Write the block allocation table after the copy of the disk footer and the dynamic disk header.
         */
        vdIfIoIntFileWriteSync(pImage->pIfIo, pImage->pStorage, pImage->uBlockAllocationTableOffset,
                               pBlockAllocationTableToWrite, cbBlockAllocationTableToWrite, NULL);
        if (pImage->fDynHdrNeedsUpdate)
            rc = vhdDynamicHeaderUpdate(pImage);
        RTMemFree(pBlockAllocationTableToWrite);
    }

    if (RT_SUCCESS(rc))
        rc = vhdUpdateFooter(pImage);

    if (RT_SUCCESS(rc))
        rc = vdIfIoIntFileFlushSync(pImage->pIfIo, pImage->pStorage);

    return rc;
}

/**
 * Internal. Free all allocated space for representing an image except pImage,
 * and optionally delete the image from disk.
 */
static int vhdFreeImage(PVHDIMAGE pImage, bool fDelete)
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
                vhdFlushImage(pImage);

            vdIfIoIntFileClose(pImage->pIfIo, pImage->pStorage);
            pImage->pStorage = NULL;
        }

        if (pImage->pszParentFilename)
        {
            RTStrFree(pImage->pszParentFilename);
            pImage->pszParentFilename = NULL;
        }
        if (pImage->pBlockAllocationTable)
        {
            RTMemFree(pImage->pBlockAllocationTable);
            pImage->pBlockAllocationTable = NULL;
        }
        if (pImage->pu8Bitmap)
        {
            RTMemFree(pImage->pu8Bitmap);
            pImage->pu8Bitmap = NULL;
        }

        if (fDelete && pImage->pszFilename)
            rc = vdIfIoIntFileDelete(pImage->pIfIo, pImage->pszFilename);
    }

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/* 946684800 is the number of seconds between 1/1/1970 and 1/1/2000 */
#define VHD_TO_UNIX_EPOCH_SECONDS UINT64_C(946684800)

static uint32_t vhdRtTime2VhdTime(PCRTTIMESPEC pRtTimeStamp)
{
    uint64_t u64Seconds = RTTimeSpecGetSeconds(pRtTimeStamp);
    return (uint32_t)(u64Seconds - VHD_TO_UNIX_EPOCH_SECONDS);
}

static void vhdTime2RtTime(PRTTIMESPEC pRtTimeStamp, uint32_t u32VhdTimeStamp)
{
    RTTimeSpecSetSeconds(pRtTimeStamp, VHD_TO_UNIX_EPOCH_SECONDS + u32VhdTimeStamp);
}

/**
 * Internal: Allocates the block bitmap rounding up to the next 32bit or 64bit boundary.
 *           Can be freed with RTMemFree. The memory is zeroed.
 */
DECLINLINE(uint8_t *)vhdBlockBitmapAllocate(PVHDIMAGE pImage)
{
#ifdef RT_ARCH_AMD64
    return (uint8_t *)RTMemAllocZ(pImage->cbDataBlockBitmap + 8);
#else
    return (uint8_t *)RTMemAllocZ(pImage->cbDataBlockBitmap + 4);
#endif
}

/**
 * Internal: called when the async expansion process completed (failure or success).
 *           Will do the necessary rollback if an error occurred.
 */
static int vhdAsyncExpansionComplete(PVHDIMAGE pImage, PVDIOCTX pIoCtx, PVHDIMAGEEXPAND pExpand)
{
    int rc = VINF_SUCCESS;
    uint32_t fFlags = ASMAtomicReadU32(&pExpand->fFlags);
    bool fIoInProgress = false;

    /* Quick path, check if everything succeeded. */
    if (fFlags == VHDIMAGEEXPAND_ALL_SUCCESS)
    {
        RTMemFree(pExpand);
    }
    else
    {
        uint32_t uStatus;

        uStatus = VHDIMAGEEXPAND_STATUS_GET(pExpand->fFlags, VHDIMAGEEXPAND_BAT_STATUS_SHIFT);
        if (   uStatus == VHDIMAGEEXPAND_STEP_FAILED
            || uStatus == VHDIMAGEEXPAND_STEP_SUCCESS)
        {
            /* Undo and restore the old value. */
            pImage->pBlockAllocationTable[pExpand->idxBatAllocated] = ~0U;

            /* Restore the old value on the disk.
             * No need for a completion callback because we can't
             * do anything if this fails. */
            if (uStatus == VHDIMAGEEXPAND_STEP_SUCCESS)
            {
                rc = vdIfIoIntFileWriteMetaAsync(pImage->pIfIo, pImage->pStorage,
                                                   pImage->uBlockAllocationTableOffset
                                                 + pExpand->idxBatAllocated * sizeof(uint32_t),
                                                 &pImage->pBlockAllocationTable[pExpand->idxBatAllocated],
                                                 sizeof(uint32_t), pIoCtx, NULL, NULL);
                fIoInProgress |= rc == VERR_VD_ASYNC_IO_IN_PROGRESS;
            }
        }

        /* Restore old size (including the footer because another application might
         * fill up the free space making it impossible to add the footer)
         * and add the footer at the right place again. */
        rc = vdIfIoIntFileSetSize(pImage->pIfIo, pImage->pStorage,
                                  pExpand->cbEofOld + sizeof(VHDFooter));
        AssertRC(rc);

        pImage->uCurrentEndOfFile = pExpand->cbEofOld;
        rc = vdIfIoIntFileWriteMetaAsync(pImage->pIfIo, pImage->pStorage,
                                         pImage->uCurrentEndOfFile,
                                         &pImage->vhdFooterCopy, sizeof(VHDFooter),
                                         pIoCtx, NULL, NULL);
        fIoInProgress |= rc == VERR_VD_ASYNC_IO_IN_PROGRESS;
    }

    return fIoInProgress ? VERR_VD_ASYNC_IO_IN_PROGRESS : rc;
}

static int vhdAsyncExpansionStepCompleted(void *pBackendData, PVDIOCTX pIoCtx, void *pvUser, int rcReq, unsigned iStep)
{
   PVHDIMAGE pImage = (PVHDIMAGE)pBackendData;
   PVHDIMAGEEXPAND pExpand = (PVHDIMAGEEXPAND)pvUser;

   LogFlowFunc(("pBackendData=%#p pIoCtx=%#p pvUser=%#p rcReq=%Rrc iStep=%u\n",
                pBackendData, pIoCtx, pvUser, rcReq, iStep));

   if (RT_SUCCESS(rcReq))
       VHDIMAGEEXPAND_STATUS_SET(pExpand->fFlags, iStep, VHDIMAGEEXPAND_STEP_SUCCESS);
   else
       VHDIMAGEEXPAND_STATUS_SET(pExpand->fFlags, iStep, VHDIMAGEEXPAND_STEP_FAILED);

   if ((pExpand->fFlags & VHDIMAGEEXPAND_ALL_COMPLETE) == VHDIMAGEEXPAND_ALL_COMPLETE)
       return vhdAsyncExpansionComplete(pImage, pIoCtx, pExpand);

   return VERR_VD_ASYNC_IO_IN_PROGRESS;
}

static int vhdAsyncExpansionDataBlockBitmapComplete(void *pBackendData, PVDIOCTX pIoCtx, void *pvUser, int rcReq)
{
    return vhdAsyncExpansionStepCompleted(pBackendData, pIoCtx, pvUser, rcReq, VHDIMAGEEXPAND_BLOCKBITMAP_STATUS_SHIFT);
}

static int vhdAsyncExpansionDataComplete(void *pBackendData, PVDIOCTX pIoCtx, void *pvUser, int rcReq)
{
    return vhdAsyncExpansionStepCompleted(pBackendData, pIoCtx, pvUser, rcReq, VHDIMAGEEXPAND_USERBLOCK_STATUS_SHIFT);
}

static int vhdAsyncExpansionBatUpdateComplete(void *pBackendData, PVDIOCTX pIoCtx, void *pvUser, int rcReq)
{
    return vhdAsyncExpansionStepCompleted(pBackendData, pIoCtx, pvUser, rcReq, VHDIMAGEEXPAND_BAT_STATUS_SHIFT);
}

static int vhdAsyncExpansionFooterUpdateComplete(void *pBackendData, PVDIOCTX pIoCtx, void *pvUser, int rcReq)
{
    return vhdAsyncExpansionStepCompleted(pBackendData, pIoCtx, pvUser, rcReq, VHDIMAGEEXPAND_FOOTER_STATUS_SHIFT);
}

static int vhdLoadDynamicDisk(PVHDIMAGE pImage, uint64_t uDynamicDiskHeaderOffset)
{
    VHDDynamicDiskHeader vhdDynamicDiskHeader;
    int rc = VINF_SUCCESS;
    uint32_t *pBlockAllocationTable;
    uint64_t uBlockAllocationTableOffset;
    unsigned i = 0;

    Log(("Open a dynamic disk.\n"));

    /*
     * Read the dynamic disk header.
     */
    rc = vdIfIoIntFileReadSync(pImage->pIfIo, pImage->pStorage, uDynamicDiskHeaderOffset,
                         &vhdDynamicDiskHeader, sizeof(VHDDynamicDiskHeader),
                         NULL);
    if (memcmp(vhdDynamicDiskHeader.Cookie, VHD_DYNAMIC_DISK_HEADER_COOKIE, VHD_DYNAMIC_DISK_HEADER_COOKIE_SIZE))
        return VERR_INVALID_PARAMETER;

    pImage->cbDataBlock = RT_BE2H_U32(vhdDynamicDiskHeader.BlockSize);
    LogFlowFunc(("BlockSize=%u\n", pImage->cbDataBlock));
    pImage->cBlockAllocationTableEntries = RT_BE2H_U32(vhdDynamicDiskHeader.MaxTableEntries);
    LogFlowFunc(("MaxTableEntries=%lu\n", pImage->cBlockAllocationTableEntries));
    AssertMsg(!(pImage->cbDataBlock % VHD_SECTOR_SIZE), ("%s: Data block size is not a multiple of %!\n", __FUNCTION__, VHD_SECTOR_SIZE));

    pImage->cSectorsPerDataBlock = pImage->cbDataBlock / VHD_SECTOR_SIZE;
    LogFlowFunc(("SectorsPerDataBlock=%u\n", pImage->cSectorsPerDataBlock));

    /*
     * Every block starts with a bitmap indicating which sectors are valid and which are not.
     * We store the size of it to be able to calculate the real offset.
     */
    pImage->cbDataBlockBitmap = pImage->cSectorsPerDataBlock / 8;
    pImage->cDataBlockBitmapSectors = pImage->cbDataBlockBitmap / VHD_SECTOR_SIZE;
    /* Round up to full sector size */
    if (pImage->cbDataBlockBitmap % VHD_SECTOR_SIZE > 0)
        pImage->cDataBlockBitmapSectors++;
    LogFlowFunc(("cbDataBlockBitmap=%u\n", pImage->cbDataBlockBitmap));
    LogFlowFunc(("cDataBlockBitmapSectors=%u\n", pImage->cDataBlockBitmapSectors));

    pImage->pu8Bitmap = vhdBlockBitmapAllocate(pImage);
    if (!pImage->pu8Bitmap)
        return VERR_NO_MEMORY;

    pBlockAllocationTable = (uint32_t *)RTMemAllocZ(pImage->cBlockAllocationTableEntries * sizeof(uint32_t));
    if (!pBlockAllocationTable)
        return VERR_NO_MEMORY;

    /*
     * Read the table.
     */
    uBlockAllocationTableOffset = RT_BE2H_U64(vhdDynamicDiskHeader.TableOffset);
    LogFlowFunc(("uBlockAllocationTableOffset=%llu\n", uBlockAllocationTableOffset));
    pImage->uBlockAllocationTableOffset = uBlockAllocationTableOffset;
    rc = vdIfIoIntFileReadSync(pImage->pIfIo, pImage->pStorage,
                               uBlockAllocationTableOffset, pBlockAllocationTable,
                               pImage->cBlockAllocationTableEntries * sizeof(uint32_t),
                               NULL);

    /*
     * Because the offset entries inside the allocation table are stored big endian
     * we need to convert them into host endian.
     */
    pImage->pBlockAllocationTable = (uint32_t *)RTMemAllocZ(pImage->cBlockAllocationTableEntries * sizeof(uint32_t));
    if (!pImage->pBlockAllocationTable)
    {
        RTMemFree(pBlockAllocationTable);
        return VERR_NO_MEMORY;
    }

    for (i = 0; i < pImage->cBlockAllocationTableEntries; i++)
        pImage->pBlockAllocationTable[i] = RT_BE2H_U32(pBlockAllocationTable[i]);

    RTMemFree(pBlockAllocationTable);

    if (pImage->uImageFlags & VD_IMAGE_FLAGS_DIFF)
        memcpy(pImage->ParentUuid.au8, vhdDynamicDiskHeader.ParentUuid, sizeof(pImage->ParentUuid));

    return rc;
}

static int vhdOpenImage(PVHDIMAGE pImage, unsigned uOpenFlags)
{
    uint64_t FileSize;
    VHDFooter vhdFooter;

    pImage->uOpenFlags = uOpenFlags;

    pImage->pIfError = VDIfErrorGet(pImage->pVDIfsDisk);
    pImage->pIfIo = VDIfIoIntGet(pImage->pVDIfsImage);
    AssertPtrReturn(pImage->pIfIo, VERR_INVALID_PARAMETER);

    /*
     * Open the image.
     */
    int rc = vdIfIoIntFileOpen(pImage->pIfIo, pImage->pszFilename,
                               VDOpenFlagsToFileOpenFlags(uOpenFlags,
                                                          false /* fCreate */),
                               &pImage->pStorage);
    if (RT_FAILURE(rc))
    {
        /* Do NOT signal an appropriate error here, as the VD layer has the
         * choice of retrying the open if it failed. */
        return rc;
    }

    rc = vdIfIoIntFileGetSize(pImage->pIfIo, pImage->pStorage, &FileSize);
    pImage->uCurrentEndOfFile = FileSize - sizeof(VHDFooter);

    rc = vdIfIoIntFileReadSync(pImage->pIfIo, pImage->pStorage, pImage->uCurrentEndOfFile,
                               &vhdFooter, sizeof(VHDFooter), NULL);
    if (memcmp(vhdFooter.Cookie, VHD_FOOTER_COOKIE, VHD_FOOTER_COOKIE_SIZE) != 0)
        return VERR_VD_VHD_INVALID_HEADER;

    switch (RT_BE2H_U32(vhdFooter.DiskType))
    {
        case VHD_FOOTER_DISK_TYPE_FIXED:
            pImage->uImageFlags |= VD_IMAGE_FLAGS_FIXED;
            break;
        case VHD_FOOTER_DISK_TYPE_DYNAMIC:
            pImage->uImageFlags &= ~VD_IMAGE_FLAGS_FIXED;
            break;
        case VHD_FOOTER_DISK_TYPE_DIFFERENCING:
            pImage->uImageFlags |= VD_IMAGE_FLAGS_DIFF;
            pImage->uImageFlags &= ~VD_IMAGE_FLAGS_FIXED;
            break;
        default:
            return VERR_NOT_IMPLEMENTED;
    }

    pImage->cbSize       = RT_BE2H_U64(vhdFooter.CurSize);
    pImage->LCHSGeometry.cCylinders   = 0;
    pImage->LCHSGeometry.cHeads       = 0;
    pImage->LCHSGeometry.cSectors     = 0;
    pImage->PCHSGeometry.cCylinders   = RT_BE2H_U16(vhdFooter.DiskGeometryCylinder);
    pImage->PCHSGeometry.cHeads       = vhdFooter.DiskGeometryHeads;
    pImage->PCHSGeometry.cSectors     = vhdFooter.DiskGeometrySectors;

    /*
     * Copy of the disk footer.
     * If we allocate new blocks in differencing disks on write access
     * the footer is overwritten. We need to write it at the end of the file.
     */
    memcpy(&pImage->vhdFooterCopy, &vhdFooter, sizeof(VHDFooter));

    /*
     * Is there a better way?
     */
    memcpy(&pImage->ImageUuid, &vhdFooter.UniqueID, 16);

    pImage->u64DataOffset = RT_BE2H_U64(vhdFooter.DataOffset);
    LogFlowFunc(("DataOffset=%llu\n", pImage->u64DataOffset));

    if (!(pImage->uImageFlags & VD_IMAGE_FLAGS_FIXED))
        rc = vhdLoadDynamicDisk(pImage, pImage->u64DataOffset);

    if (RT_FAILURE(rc))
        vhdFreeImage(pImage, false);
    return rc;
}

/**
 * Internal: Checks if a sector in the block bitmap is set
 */
DECLINLINE(bool) vhdBlockBitmapSectorContainsData(PVHDIMAGE pImage, uint32_t cBlockBitmapEntry)
{
    uint32_t iBitmap = (cBlockBitmapEntry / 8); /* Byte in the block bitmap. */

    /*
     * The index of the bit in the byte of the data block bitmap.
     * The most significant bit stands for a lower sector number.
     */
    uint8_t  iBitInByte = (8-1) - (cBlockBitmapEntry % 8);
    uint8_t *puBitmap = pImage->pu8Bitmap + iBitmap;

    AssertMsg(puBitmap < (pImage->pu8Bitmap + pImage->cbDataBlockBitmap),
                ("VHD: Current bitmap position exceeds maximum size of the bitmap\n"));

    return ASMBitTest(puBitmap, iBitInByte);
}

/**
 * Internal: Sets the given sector in the sector bitmap.
 */
DECLINLINE(bool) vhdBlockBitmapSectorSet(PVHDIMAGE pImage, uint8_t *pu8Bitmap, uint32_t cBlockBitmapEntry)
{
    uint32_t iBitmap = (cBlockBitmapEntry / 8); /* Byte in the block bitmap. */

    /*
     * The index of the bit in the byte of the data block bitmap.
     * The most significant bit stands for a lower sector number.
     */
    uint8_t  iBitInByte = (8-1) - (cBlockBitmapEntry % 8);
    uint8_t  *puBitmap  = pu8Bitmap + iBitmap;

    AssertMsg(puBitmap < (pu8Bitmap + pImage->cbDataBlockBitmap),
                ("VHD: Current bitmap position exceeds maximum size of the bitmap\n"));

    return !ASMBitTestAndSet(puBitmap, iBitInByte);
}

/**
 * Internal: Derive drive geometry from its size.
 */
static void vhdSetDiskGeometry(PVHDIMAGE pImage, uint64_t cbSize)
{
    uint64_t u64TotalSectors = cbSize / VHD_SECTOR_SIZE;
    uint32_t u32CylinderTimesHeads, u32Heads, u32SectorsPerTrack;

    if (u64TotalSectors > 65535 * 16 * 255)
    {
        /* ATA disks limited to 127 GB. */
        u64TotalSectors = 65535 * 16 * 255;
    }

    if (u64TotalSectors >= 65535 * 16 * 63)
    {
        u32SectorsPerTrack    = 255;
        u32Heads              = 16;
        u32CylinderTimesHeads = u64TotalSectors / u32SectorsPerTrack;
    }
    else
    {
        u32SectorsPerTrack    = 17;
        u32CylinderTimesHeads = u64TotalSectors / u32SectorsPerTrack;

        u32Heads = (u32CylinderTimesHeads + 1023) / 1024;

        if (u32Heads < 4)
        {
            u32Heads = 4;
        }
        if (u32CylinderTimesHeads >= (u32Heads * 1024) || u32Heads > 16)
        {
            u32SectorsPerTrack    = 31;
            u32Heads              = 16;
            u32CylinderTimesHeads = u64TotalSectors / u32SectorsPerTrack;
        }
        if (u32CylinderTimesHeads >= (u32Heads * 1024))
        {
            u32SectorsPerTrack    = 63;
            u32Heads              = 16;
            u32CylinderTimesHeads = u64TotalSectors / u32SectorsPerTrack;
        }
    }
    pImage->PCHSGeometry.cCylinders = u32CylinderTimesHeads / u32Heads;
    pImage->PCHSGeometry.cHeads     = u32Heads;
    pImage->PCHSGeometry.cSectors   = u32SectorsPerTrack;
    pImage->LCHSGeometry.cCylinders = 0;
    pImage->LCHSGeometry.cHeads     = 0;
    pImage->LCHSGeometry.cSectors   = 0;
}


static uint32_t vhdAllocateParentLocators(PVHDIMAGE pImage, VHDDynamicDiskHeader *pDDH, uint64_t u64Offset)
{
    PVHDPLE pLocator = pDDH->ParentLocatorEntry;
    /* Relative Windows path. */
    pLocator->u32Code = RT_H2BE_U32(VHD_PLATFORM_CODE_WI2R);
    pLocator->u32DataSpace = RT_H2BE_U32(VHD_RELATIVE_MAX_PATH / VHD_SECTOR_SIZE);
    pLocator->u64DataOffset = RT_H2BE_U64(u64Offset);
    u64Offset += VHD_RELATIVE_MAX_PATH;
    pLocator++;
    /* Absolute Windows path. */
    pLocator->u32Code = RT_H2BE_U32(VHD_PLATFORM_CODE_WI2K);
    pLocator->u32DataSpace = RT_H2BE_U32(VHD_ABSOLUTE_MAX_PATH / VHD_SECTOR_SIZE);
    pLocator->u64DataOffset = RT_H2BE_U64(u64Offset);
    u64Offset += VHD_ABSOLUTE_MAX_PATH;
    pLocator++;
    /* Unicode relative Windows path. */
    pLocator->u32Code = RT_H2BE_U32(VHD_PLATFORM_CODE_W2RU);
    pLocator->u32DataSpace = RT_H2BE_U32(VHD_RELATIVE_MAX_PATH * sizeof(RTUTF16) / VHD_SECTOR_SIZE);
    pLocator->u64DataOffset = RT_H2BE_U64(u64Offset);
    u64Offset += VHD_RELATIVE_MAX_PATH * sizeof(RTUTF16);
    pLocator++;
    /* Unicode absolute Windows path. */
    pLocator->u32Code = RT_H2BE_U32(VHD_PLATFORM_CODE_W2KU);
    pLocator->u32DataSpace = RT_H2BE_U32(VHD_ABSOLUTE_MAX_PATH * sizeof(RTUTF16) / VHD_SECTOR_SIZE);
    pLocator->u64DataOffset = RT_H2BE_U64(u64Offset);
    return u64Offset + VHD_ABSOLUTE_MAX_PATH * sizeof(RTUTF16);
}

/**
 * Internal: Additional code for dynamic VHD image creation.
 */
static int vhdCreateDynamicImage(PVHDIMAGE pImage, uint64_t cbSize)
{
    int rc;
    VHDDynamicDiskHeader DynamicDiskHeader;
    uint32_t u32BlockAllocationTableSectors;
    void    *pvTmp = NULL;

    memset(&DynamicDiskHeader, 0, sizeof(DynamicDiskHeader));

    pImage->u64DataOffset           = sizeof(VHDFooter);
    pImage->cbDataBlock             = VHD_BLOCK_SIZE; /* 2 MB */
    pImage->cSectorsPerDataBlock    = pImage->cbDataBlock / VHD_SECTOR_SIZE;
    pImage->cbDataBlockBitmap       = pImage->cSectorsPerDataBlock / 8;
    pImage->cDataBlockBitmapSectors = pImage->cbDataBlockBitmap / VHD_SECTOR_SIZE;
    /* Align to sector boundary */
    if (pImage->cbDataBlockBitmap % VHD_SECTOR_SIZE > 0)
        pImage->cDataBlockBitmapSectors++;
    pImage->pu8Bitmap               = vhdBlockBitmapAllocate(pImage);
    if (!pImage->pu8Bitmap)
        return vdIfError(pImage->pIfError, VERR_NO_MEMORY, RT_SRC_POS, N_("VHD: cannot allocate memory for bitmap storage"));

    /* Initialize BAT. */
    pImage->uBlockAllocationTableOffset = (uint64_t)sizeof(VHDFooter) + sizeof(VHDDynamicDiskHeader);
    pImage->cBlockAllocationTableEntries = (uint32_t)((cbSize + pImage->cbDataBlock - 1) / pImage->cbDataBlock); /* Align table to the block size. */
    u32BlockAllocationTableSectors = (pImage->cBlockAllocationTableEntries * sizeof(uint32_t) + VHD_SECTOR_SIZE - 1) / VHD_SECTOR_SIZE;
    pImage->pBlockAllocationTable = (uint32_t *)RTMemAllocZ(pImage->cBlockAllocationTableEntries * sizeof(uint32_t));
    if (!pImage->pBlockAllocationTable)
        return vdIfError(pImage->pIfError, VERR_NO_MEMORY, RT_SRC_POS, N_("VHD: cannot allocate memory for BAT"));

    for (unsigned i = 0; i < pImage->cBlockAllocationTableEntries; i++)
    {
        pImage->pBlockAllocationTable[i] = 0xFFFFFFFF; /* It is actually big endian. */
    }

    /* Round up to the sector size. */
    if (pImage->uImageFlags & VD_IMAGE_FLAGS_DIFF) /* fix hyper-v unreadable error */
        pImage->uCurrentEndOfFile = vhdAllocateParentLocators(pImage, &DynamicDiskHeader,
                                                              pImage->uBlockAllocationTableOffset + u32BlockAllocationTableSectors * VHD_SECTOR_SIZE);
    else
        pImage->uCurrentEndOfFile = pImage->uBlockAllocationTableOffset + u32BlockAllocationTableSectors * VHD_SECTOR_SIZE;

    /* Set dynamic image size. */
    pvTmp = RTMemTmpAllocZ(pImage->uCurrentEndOfFile + sizeof(VHDFooter));
    if (!pvTmp)
        return vdIfError(pImage->pIfError, VERR_NO_MEMORY, RT_SRC_POS, N_("VHD: cannot set the file size for '%s'"), pImage->pszFilename);

    rc = vdIfIoIntFileWriteSync(pImage->pIfIo, pImage->pStorage, 0, pvTmp,
                          pImage->uCurrentEndOfFile + sizeof(VHDFooter), NULL);
    if (RT_FAILURE(rc))
    {
        RTMemTmpFree(pvTmp);
        return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VHD: cannot set the file size for '%s'"), pImage->pszFilename);
    }

    RTMemTmpFree(pvTmp);

    /* Initialize and write the dynamic disk header. */
    memcpy(DynamicDiskHeader.Cookie, VHD_DYNAMIC_DISK_HEADER_COOKIE, sizeof(DynamicDiskHeader.Cookie));
    DynamicDiskHeader.DataOffset      = UINT64_C(0xFFFFFFFFFFFFFFFF); /* Initially the disk has no data. */
    DynamicDiskHeader.TableOffset     = RT_H2BE_U64(pImage->uBlockAllocationTableOffset);
    DynamicDiskHeader.HeaderVersion   = RT_H2BE_U32(VHD_DYNAMIC_DISK_HEADER_VERSION);
    DynamicDiskHeader.BlockSize       = RT_H2BE_U32(pImage->cbDataBlock);
    DynamicDiskHeader.MaxTableEntries = RT_H2BE_U32(pImage->cBlockAllocationTableEntries);
    /* Compute and update checksum. */
    DynamicDiskHeader.Checksum = 0;
    DynamicDiskHeader.Checksum = RT_H2BE_U32(vhdChecksum(&DynamicDiskHeader, sizeof(DynamicDiskHeader)));

    rc = vdIfIoIntFileWriteSync(pImage->pIfIo, pImage->pStorage, sizeof(VHDFooter),
                                &DynamicDiskHeader, sizeof(DynamicDiskHeader), NULL);
    if (RT_FAILURE(rc))
        return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VHD: cannot write dynamic disk header to image '%s'"), pImage->pszFilename);

    /* Write BAT. */
    rc = vdIfIoIntFileWriteSync(pImage->pIfIo, pImage->pStorage, pImage->uBlockAllocationTableOffset,
                                pImage->pBlockAllocationTable,
                                pImage->cBlockAllocationTableEntries * sizeof(uint32_t),
                                NULL);
    if (RT_FAILURE(rc))
        return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VHD: cannot write BAT to image '%s'"), pImage->pszFilename);

    return rc;
}

/**
 * Internal: The actual code for VHD image creation, both fixed and dynamic.
 */
static int vhdCreateImage(PVHDIMAGE pImage, uint64_t cbSize,
                          unsigned uImageFlags, const char *pszComment,
                          PCVDGEOMETRY pPCHSGeometry,
                          PCVDGEOMETRY pLCHSGeometry, PCRTUUID pUuid,
                          unsigned uOpenFlags,
                          PFNVDPROGRESS pfnProgress, void *pvUser,
                          unsigned uPercentStart, unsigned uPercentSpan)
{
    int rc;
    VHDFooter Footer;
    RTTIMESPEC now;

    pImage->uOpenFlags = uOpenFlags;
    pImage->uImageFlags = uImageFlags;

    pImage->pIfError = VDIfErrorGet(pImage->pVDIfsDisk);

    rc = vdIfIoIntFileOpen(pImage->pIfIo, pImage->pszFilename,
                           VDOpenFlagsToFileOpenFlags(uOpenFlags & ~VD_OPEN_FLAGS_READONLY,
                                                      true /* fCreate */),
                           &pImage->pStorage);
    if (RT_FAILURE(rc))
        return vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VHD: cannot create image '%s'"), pImage->pszFilename);


    pImage->cbSize = cbSize;
    pImage->ImageUuid = *pUuid;
    RTUuidClear(&pImage->ParentUuid);
    vhdSetDiskGeometry(pImage, cbSize);

    /* Initialize the footer. */
    memset(&Footer, 0, sizeof(Footer));
    memcpy(Footer.Cookie, VHD_FOOTER_COOKIE, sizeof(Footer.Cookie));
    Footer.Features = RT_H2BE_U32(0x2);
    Footer.Version  = RT_H2BE_U32(VHD_FOOTER_FILE_FORMAT_VERSION);
    Footer.TimeStamp = RT_H2BE_U32(vhdRtTime2VhdTime(RTTimeNow(&now)));
    memcpy(Footer.CreatorApp, "vbox", sizeof(Footer.CreatorApp));
    Footer.CreatorVer = RT_H2BE_U32(VBOX_VERSION);
#ifdef RT_OS_DARWIN
    Footer.CreatorOS  = RT_H2BE_U32(0x4D616320); /* "Mac " */
#else /* Virtual PC supports only two platforms atm, so everything else will be Wi2k. */
    Footer.CreatorOS  = RT_H2BE_U32(0x5769326B); /* "Wi2k" */
#endif
    Footer.OrigSize   = RT_H2BE_U64(cbSize);
    Footer.CurSize    = Footer.OrigSize;
    Footer.DiskGeometryCylinder = RT_H2BE_U16(pImage->PCHSGeometry.cCylinders);
    Footer.DiskGeometryHeads    = pImage->PCHSGeometry.cHeads;
    Footer.DiskGeometrySectors  = pImage->PCHSGeometry.cSectors;
    memcpy(Footer.UniqueID, pImage->ImageUuid.au8, sizeof(Footer.UniqueID));
    Footer.SavedState = 0;

    if (uImageFlags & VD_IMAGE_FLAGS_FIXED)
    {
        Footer.DiskType   = RT_H2BE_U32(VHD_FOOTER_DISK_TYPE_FIXED);
        /*
         * Initialize fixed image.
         * "The size of the entire file is the size of the hard disk in
         * the guest operating system plus the size of the footer."
         */
        pImage->u64DataOffset     = VHD_FOOTER_DATA_OFFSET_FIXED;
        pImage->uCurrentEndOfFile = cbSize;
        /** @todo r=klaus replace this with actual data writes, see the experience
         * with VDI files on Windows, can cause long freezes when writing. */
        rc = vdIfIoIntFileSetSize(pImage->pIfIo, pImage->pStorage,
                                  pImage->uCurrentEndOfFile + sizeof(VHDFooter));
        if (RT_FAILURE(rc))
        {
            vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VHD: cannot set the file size for '%s'"), pImage->pszFilename);
            goto out;
        }
    }
    else
    {
        /*
         * Initialize dynamic image.
         *
         * The overall structure of dynamic disk is:
         *
         * [Copy of hard disk footer (512 bytes)]
         * [Dynamic disk header (1024 bytes)]
         * [BAT (Block Allocation Table)]
         * [Parent Locators]
         * [Data block 1]
         * [Data block 2]
         * ...
         * [Data block N]
         * [Hard disk footer (512 bytes)]
         */
        Footer.DiskType   = (uImageFlags & VD_IMAGE_FLAGS_DIFF)
                              ? RT_H2BE_U32(VHD_FOOTER_DISK_TYPE_DIFFERENCING)
                              : RT_H2BE_U32(VHD_FOOTER_DISK_TYPE_DYNAMIC);
        /* We are half way thorough with creation of image, let the caller know. */
        if (pfnProgress)
            pfnProgress(pvUser, (uPercentStart + uPercentSpan) / 2);

        rc = vhdCreateDynamicImage(pImage, cbSize);
        if (RT_FAILURE(rc))
            goto out;
    }

    Footer.DataOffset = RT_H2BE_U64(pImage->u64DataOffset);

    /* Compute and update the footer checksum. */
    Footer.Checksum = 0;
    Footer.Checksum = RT_H2BE_U32(vhdChecksum(&Footer, sizeof(Footer)));

    pImage->vhdFooterCopy = Footer;

    /* Store the footer */
    rc = vdIfIoIntFileWriteSync(pImage->pIfIo, pImage->pStorage, pImage->uCurrentEndOfFile,
                                &Footer, sizeof(Footer), NULL);
    if (RT_FAILURE(rc))
    {
        vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VHD: cannot write footer to image '%s'"), pImage->pszFilename);
        goto out;
    }

    /* Dynamic images contain a copy of the footer at the very beginning of the file. */
    if (!(uImageFlags & VD_IMAGE_FLAGS_FIXED))
    {
        /* Write the copy of the footer. */
        rc = vdIfIoIntFileWriteSync(pImage->pIfIo, pImage->pStorage, 0, &Footer, sizeof(Footer), NULL);
        if (RT_FAILURE(rc))
        {
            vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("VHD: cannot write a copy of footer to image '%s'"), pImage->pszFilename);
            goto out;
        }
    }

out:
    if (RT_SUCCESS(rc) && pfnProgress)
        pfnProgress(pvUser, uPercentStart + uPercentSpan);

    if (RT_FAILURE(rc))
        vhdFreeImage(pImage, rc != VERR_ALREADY_EXISTS);
    return rc;
}


/** @copydoc VBOXHDDBACKEND::pfnCheckIfValid */
static int vhdCheckIfValid(const char *pszFilename, PVDINTERFACE pVDIfsDisk,
                           PVDINTERFACE pVDIfsImage, VDTYPE *penmType)
{
    LogFlowFunc(("pszFilename=\"%s\" pVDIfsDisk=%#p pVDIfsImage=%#p\n", pszFilename, pVDIfsDisk, pVDIfsImage));
    int rc;
    PVDIOSTORAGE pStorage;
    uint64_t cbFile;
    VHDFooter vhdFooter;

    PVDINTERFACEIOINT pIfIo = VDIfIoIntGet(pVDIfsImage);
    AssertPtrReturn(pIfIo, VERR_INVALID_PARAMETER);

    rc = vdIfIoIntFileOpen(pIfIo, pszFilename,
                           VDOpenFlagsToFileOpenFlags(VD_OPEN_FLAGS_READONLY,
                                                      false /* fCreate */),
                           &pStorage);
    if (RT_FAILURE(rc))
        goto out;

    rc = vdIfIoIntFileGetSize(pIfIo, pStorage, &cbFile);
    if (RT_FAILURE(rc))
    {
        vdIfIoIntFileClose(pIfIo, pStorage);
        rc = VERR_VD_VHD_INVALID_HEADER;
        goto out;
    }

    rc = vdIfIoIntFileReadSync(pIfIo, pStorage, cbFile - sizeof(VHDFooter),
                               &vhdFooter, sizeof(VHDFooter), NULL);
    if (RT_FAILURE(rc) || (memcmp(vhdFooter.Cookie, VHD_FOOTER_COOKIE, VHD_FOOTER_COOKIE_SIZE) != 0))
        rc = VERR_VD_VHD_INVALID_HEADER;
    else
    {
        *penmType = VDTYPE_HDD;
        rc = VINF_SUCCESS;
    }

    vdIfIoIntFileClose(pIfIo, pStorage);

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnOpen */
static int vhdOpen(const char *pszFilename, unsigned uOpenFlags,
                   PVDINTERFACE pVDIfsDisk, PVDINTERFACE pVDIfsImage,
                   VDTYPE enmType, void **ppBackendData)
{
    LogFlowFunc(("pszFilename=\"%s\" uOpenFlags=%#x pVDIfsDisk=%#p pVDIfsImage=%#p ppBackendData=%#p\n", pszFilename, uOpenFlags, pVDIfsDisk, pVDIfsImage, ppBackendData));
    int rc = VINF_SUCCESS;
    PVHDIMAGE pImage;

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

    pImage = (PVHDIMAGE)RTMemAllocZ(sizeof(VHDIMAGE));
    if (!pImage)
    {
        rc = VERR_NO_MEMORY;
        goto out;
    }

    pImage->pszFilename = pszFilename;
    pImage->pStorage = NULL;
    pImage->pVDIfsDisk = pVDIfsDisk;
    pImage->pVDIfsImage = pVDIfsImage;

    rc = vhdOpenImage(pImage, uOpenFlags);
    if (RT_SUCCESS(rc))
        *ppBackendData = pImage;
    else
        RTMemFree(pImage);

out:
    LogFlowFunc(("returns %Rrc (pBackendData=%#p)\n", rc, *ppBackendData));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnCreate */
static int vhdCreate(const char *pszFilename, uint64_t cbSize,
                     unsigned uImageFlags, const char *pszComment,
                     PCVDGEOMETRY pPCHSGeometry, PCVDGEOMETRY pLCHSGeometry,
                     PCRTUUID pUuid, unsigned uOpenFlags,
                     unsigned uPercentStart, unsigned uPercentSpan,
                     PVDINTERFACE pVDIfsDisk, PVDINTERFACE pVDIfsImage,
                     PVDINTERFACE pVDIfsOperation, void **ppBackendData)
{
    LogFlowFunc(("pszFilename=\"%s\" cbSize=%llu uImageFlags=%#x pszComment=\"%s\" pPCHSGeometry=%#p pLCHSGeometry=%#p Uuid=%RTuuid uOpenFlags=%#x uPercentStart=%u uPercentSpan=%u pVDIfsDisk=%#p pVDIfsImage=%#p pVDIfsOperation=%#p ppBackendData=%#p", pszFilename, cbSize, uImageFlags, pszComment, pPCHSGeometry, pLCHSGeometry, pUuid, uOpenFlags, uPercentStart, uPercentSpan, pVDIfsDisk, pVDIfsImage, pVDIfsOperation, ppBackendData));
    int rc = VINF_SUCCESS;
    PVHDIMAGE pImage;

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
        return rc;
    }

    /* @todo Check the values of other params */

    pImage = (PVHDIMAGE)RTMemAllocZ(sizeof(VHDIMAGE));
    if (!pImage)
    {
        rc = VERR_NO_MEMORY;
        return rc;
    }
    pImage->pszFilename = pszFilename;
    pImage->pStorage = NULL;
    pImage->pVDIfsDisk = pVDIfsDisk;
    pImage->pVDIfsImage = pVDIfsImage;

    /* Get I/O interface. */
    pImage->pIfIo = VDIfIoIntGet(pImage->pVDIfsImage);
    if (RT_UNLIKELY(!VALID_PTR(pImage->pIfIo)))
    {
        RTMemFree(pImage);
        return VERR_INVALID_PARAMETER;
    }

    rc = vhdCreateImage(pImage, cbSize, uImageFlags, pszComment,
                        pPCHSGeometry, pLCHSGeometry, pUuid, uOpenFlags,
                        pfnProgress, pvUser, uPercentStart, uPercentSpan);

    if (RT_SUCCESS(rc))
    {
        /* So far the image is opened in read/write mode. Make sure the
         * image is opened in read-only mode if the caller requested that. */
        if (uOpenFlags & VD_OPEN_FLAGS_READONLY)
        {
            vhdFreeImage(pImage, false);
            rc = vhdOpenImage(pImage, uOpenFlags);
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
static int vhdRename(void *pBackendData, const char *pszFilename)
{
    LogFlowFunc(("pBackendData=%#p pszFilename=%#p\n", pBackendData, pszFilename));
    int rc = VINF_SUCCESS;
    PVHDIMAGE pImage = (PVHDIMAGE)pBackendData;

    /* Check arguments. */
    if (   !pImage
        || !pszFilename
        || !*pszFilename)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    /* Close the image. */
    rc = vhdFreeImage(pImage, false);
    if (RT_FAILURE(rc))
        goto out;

    /* Rename the file. */
    rc = vdIfIoIntFileMove(pImage->pIfIo, pImage->pszFilename, pszFilename, 0);
    if (RT_FAILURE(rc))
    {
        /* The move failed, try to reopen the original image. */
        int rc2 = vhdOpenImage(pImage, pImage->uOpenFlags);
        if (RT_FAILURE(rc2))
            rc = rc2;

        goto out;
    }

    /* Update pImage with the new information. */
    pImage->pszFilename = pszFilename;

    /* Open the old file with new name. */
    rc = vhdOpenImage(pImage, pImage->uOpenFlags);
    if (RT_FAILURE(rc))
        goto out;

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnClose */
static int vhdClose(void *pBackendData, bool fDelete)
{
    LogFlowFunc(("pBackendData=%#p fDelete=%d\n", pBackendData, fDelete));
    PVHDIMAGE pImage = (PVHDIMAGE)pBackendData;
    int rc;

    rc = vhdFreeImage(pImage, fDelete);
    RTMemFree(pImage);

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnRead */
static int vhdRead(void *pBackendData, uint64_t uOffset, void *pvBuf,
                   size_t cbBuf, size_t *pcbActuallyRead)
{
    LogFlowFunc(("pBackendData=%p uOffset=%#llu pvBuf=%p cbBuf=%u pcbActuallyRead=%p\n", pBackendData, uOffset, pvBuf, cbBuf, pcbActuallyRead));
    PVHDIMAGE pImage = (PVHDIMAGE)pBackendData;
    int rc = VINF_SUCCESS;

    if (uOffset + cbBuf > pImage->cbSize)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    /*
     * If we have a dynamic disk image, we need to find the data block and sector to read.
     */
    if (pImage->pBlockAllocationTable)
    {
        /*
         * Get the data block first.
         */
        uint32_t cBlockAllocationTableEntry = (uOffset / VHD_SECTOR_SIZE) / pImage->cSectorsPerDataBlock;
        uint32_t cBATEntryIndex = (uOffset / VHD_SECTOR_SIZE) % pImage->cSectorsPerDataBlock;
        uint64_t uVhdOffset;

        LogFlowFunc(("cBlockAllocationTableEntry=%u cBatEntryIndex=%u\n", cBlockAllocationTableEntry, cBATEntryIndex));
        LogFlowFunc(("BlockAllocationEntry=%u\n", pImage->pBlockAllocationTable[cBlockAllocationTableEntry]));

        /*
         * Clip read range to remain in this data block.
         */
        cbBuf = RT_MIN(cbBuf, (pImage->cbDataBlock - (cBATEntryIndex * VHD_SECTOR_SIZE)));

        /*
         * If the block is not allocated the content of the entry is ~0
         */
        if (pImage->pBlockAllocationTable[cBlockAllocationTableEntry] == ~0U)
            rc = VERR_VD_BLOCK_FREE;
        else
        {
            uVhdOffset = ((uint64_t)pImage->pBlockAllocationTable[cBlockAllocationTableEntry] + pImage->cDataBlockBitmapSectors + cBATEntryIndex) * VHD_SECTOR_SIZE;
            LogFlowFunc(("uVhdOffset=%llu cbBuf=%u\n", uVhdOffset, cbBuf));

            /* Read in the block's bitmap. */
            rc = vdIfIoIntFileReadSync(pImage->pIfIo, pImage->pStorage,
                                       ((uint64_t)pImage->pBlockAllocationTable[cBlockAllocationTableEntry]) * VHD_SECTOR_SIZE,
                                       pImage->pu8Bitmap, pImage->cbDataBlockBitmap,
                                       NULL);
            if (RT_SUCCESS(rc))
            {
                uint32_t cSectors = 0;

                if (vhdBlockBitmapSectorContainsData(pImage, cBATEntryIndex))
                {
                    cBATEntryIndex++;
                    cSectors = 1;

                    /*
                     * The first sector being read is marked dirty, read as much as we
                     * can from child. Note that only sectors that are marked dirty
                     * must be read from child.
                     */
                    while (   (cSectors < (cbBuf / VHD_SECTOR_SIZE))
                           && vhdBlockBitmapSectorContainsData(pImage, cBATEntryIndex))
                    {
                        cBATEntryIndex++;
                        cSectors++;
                    }

                    cbBuf = cSectors * VHD_SECTOR_SIZE;

                    LogFlowFunc(("uVhdOffset=%llu cbBuf=%u\n", uVhdOffset, cbBuf));
                    rc = vdIfIoIntFileReadSync(pImage->pIfIo, pImage->pStorage,
                                               uVhdOffset, pvBuf, cbBuf, NULL);
                }
                else
                {
                    /*
                     * The first sector being read is marked clean, so we should read from
                     * our parent instead, but only as much as there are the following
                     * clean sectors, because the block may still contain dirty sectors
                     * further on. We just need to compute the number of clean sectors
                     * and pass it to our caller along with the notification that they
                     * should be read from the parent.
                     */
                    cBATEntryIndex++;
                    cSectors = 1;

                    while (   (cSectors < (cbBuf / VHD_SECTOR_SIZE))
                           && !vhdBlockBitmapSectorContainsData(pImage, cBATEntryIndex))
                    {
                        cBATEntryIndex++;
                        cSectors++;
                    }

                    cbBuf = cSectors * VHD_SECTOR_SIZE;
                    LogFunc(("Sectors free: uVhdOffset=%llu cbBuf=%u\n", uVhdOffset, cbBuf));
                    rc = VERR_VD_BLOCK_FREE;
                }
            }
            else
                AssertMsgFailed(("Reading block bitmap failed rc=%Rrc\n", rc));
        }
    }
    else
        rc = vdIfIoIntFileReadSync(pImage->pIfIo, pImage->pStorage, uOffset, pvBuf, cbBuf, NULL);

    if (   RT_SUCCESS(rc)
        || rc == VERR_VD_BLOCK_FREE)
    {
        if (pcbActuallyRead)
            *pcbActuallyRead = cbBuf;

        Log2(("vhdRead: off=%#llx pvBuf=%p cbBuf=%d\n"
                "%.*Rhxd\n",
                uOffset, pvBuf, cbBuf, cbBuf, pvBuf));
    }

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnWrite */
static int vhdWrite(void *pBackendData, uint64_t uOffset, const void *pvBuf,
                    size_t cbBuf, size_t *pcbWriteProcess,
                    size_t *pcbPreRead, size_t *pcbPostRead, unsigned fWrite)
{
    LogFlowFunc(("pBackendData=%#p uOffset=%llu pvBuf=%#p cbBuf=%zu pcbWriteProcess=%#p\n", pBackendData, uOffset, pvBuf, cbBuf, pcbWriteProcess));
    PVHDIMAGE pImage = (PVHDIMAGE)pBackendData;
    int rc = VINF_SUCCESS;

    LogFlowFunc(("pBackendData=%p uOffset=%llu pvBuf=%p cbBuf=%u pcbWriteProcess=%p pcbPreRead=%p pcbPostRead=%p fWrite=%u\n",
             pBackendData, uOffset, pvBuf, cbBuf, pcbWriteProcess, pcbPreRead, pcbPostRead, fWrite));

    AssertPtr(pImage);
    Assert(uOffset % VHD_SECTOR_SIZE == 0);
    Assert(cbBuf % VHD_SECTOR_SIZE == 0);

    if (pImage->pBlockAllocationTable)
    {
        /*
         * Get the data block first.
         */
        uint32_t cSector = uOffset / VHD_SECTOR_SIZE;
        uint32_t cBlockAllocationTableEntry = cSector / pImage->cSectorsPerDataBlock;
        uint32_t cBATEntryIndex = cSector % pImage->cSectorsPerDataBlock;
        uint64_t uVhdOffset;

        /*
         * Clip write range.
         */
        cbBuf = RT_MIN(cbBuf, (pImage->cbDataBlock - (cBATEntryIndex * VHD_SECTOR_SIZE)));

        /*
         * If the block is not allocated the content of the entry is ~0
         * and we need to allocate a new block. Note that while blocks are
         * allocated with a relatively big granularity, each sector has its
         * own bitmap entry, indicating whether it has been written or not.
         * So that means for the purposes of the higher level that the
         * granularity is invisible. This means there's no need to return
         * VERR_VD_BLOCK_FREE unless the block hasn't been allocated yet.
         */
        if (pImage->pBlockAllocationTable[cBlockAllocationTableEntry] == ~0U)
        {
            /* Check if the block allocation should be suppressed. */
            if (fWrite & VD_WRITE_NO_ALLOC)
            {
                *pcbPreRead = cBATEntryIndex * VHD_SECTOR_SIZE;
                *pcbPostRead = pImage->cSectorsPerDataBlock * VHD_SECTOR_SIZE - cbBuf - *pcbPreRead;

                if (pcbWriteProcess)
                    *pcbWriteProcess = cbBuf;
                rc = VERR_VD_BLOCK_FREE;
                goto out;
            }

            size_t  cbNewBlock = pImage->cbDataBlock + (pImage->cDataBlockBitmapSectors * VHD_SECTOR_SIZE);
            uint8_t *pNewBlock = (uint8_t *)RTMemAllocZ(cbNewBlock);

            if (!pNewBlock)
            {
                rc = VERR_NO_MEMORY;
                goto out;
            }

            /*
             * Write the new block at the current end of the file.
             */
            rc = vdIfIoIntFileWriteSync(pImage->pIfIo, pImage->pStorage, pImage->uCurrentEndOfFile,
                                        pNewBlock, cbNewBlock, NULL);
            AssertRC(rc);

            /*
             * Set the new end of the file and link the new block into the BAT.
             */
            pImage->pBlockAllocationTable[cBlockAllocationTableEntry] = pImage->uCurrentEndOfFile / VHD_SECTOR_SIZE;
            pImage->uCurrentEndOfFile += cbNewBlock;
            RTMemFree(pNewBlock);

            /* Write the updated BAT and the footer to remain in a consistent state. */
            rc = vhdFlushImage(pImage);
            AssertRC(rc);
        }

        /*
         * Calculate the real offset in the file.
         */
        uVhdOffset = ((uint64_t)pImage->pBlockAllocationTable[cBlockAllocationTableEntry] + pImage->cDataBlockBitmapSectors + cBATEntryIndex) * VHD_SECTOR_SIZE;

        /* Write data. */
        vdIfIoIntFileWriteSync(pImage->pIfIo, pImage->pStorage, uVhdOffset,
                               pvBuf, cbBuf, NULL);

        /* Read in the block's bitmap. */
        rc = vdIfIoIntFileReadSync(pImage->pIfIo, pImage->pStorage,
                                   ((uint64_t)pImage->pBlockAllocationTable[cBlockAllocationTableEntry]) * VHD_SECTOR_SIZE,
                                   pImage->pu8Bitmap, pImage->cbDataBlockBitmap,
                                   NULL);
        if (RT_SUCCESS(rc))
        {
            bool fChanged = false;

            /* Set the bits for all sectors having been written. */
            for (uint32_t iSector = 0; iSector < (cbBuf / VHD_SECTOR_SIZE); iSector++)
            {
                fChanged |= vhdBlockBitmapSectorSet(pImage, pImage->pu8Bitmap, cBATEntryIndex);
                cBATEntryIndex++;
            }

            if (fChanged)
            {
                /* Write the bitmap back. */
                rc = vdIfIoIntFileWriteSync(pImage->pIfIo, pImage->pStorage,
                                            ((uint64_t)pImage->pBlockAllocationTable[cBlockAllocationTableEntry]) * VHD_SECTOR_SIZE,
                                            pImage->pu8Bitmap, pImage->cbDataBlockBitmap,
                                            NULL);
            }
        }
    }
    else
    {
        rc = vdIfIoIntFileWriteSync(pImage->pIfIo, pImage->pStorage, uOffset, pvBuf, cbBuf, NULL);
    }

    if (pcbWriteProcess)
        *pcbWriteProcess = cbBuf;

    /* Stay on the safe side. Do not run the risk of confusing the higher
     * level, as that can be pretty lethal to image consistency. */
    *pcbPreRead = 0;
    *pcbPostRead = 0;

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnFlush */
static int vhdFlush(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p", pBackendData));
    PVHDIMAGE pImage = (PVHDIMAGE)pBackendData;
    int rc;

    rc = vhdFlushImage(pImage);
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetVersion */
static unsigned vhdGetVersion(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PVHDIMAGE pImage = (PVHDIMAGE)pBackendData;
    unsigned ver = 0;

    AssertPtr(pImage);

    if (pImage)
        ver = 1; /**< @todo use correct version */

    LogFlowFunc(("returns %u\n", ver));
    return ver;
}

/** @copydoc VBOXHDDBACKEND::pfnGetSize */
static uint64_t vhdGetSize(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PVHDIMAGE pImage = (PVHDIMAGE)pBackendData;
    uint64_t cb = 0;

    AssertPtr(pImage);

    if (pImage && pImage->pStorage)
        cb = pImage->cbSize;

    LogFlowFunc(("returns %llu\n", cb));
    return cb;
}

/** @copydoc VBOXHDDBACKEND::pfnGetFileSize */
static uint64_t vhdGetFileSize(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PVHDIMAGE pImage = (PVHDIMAGE)pBackendData;
    uint64_t cb = 0;

    AssertPtr(pImage);

    if (pImage && pImage->pStorage)
        cb = pImage->uCurrentEndOfFile + sizeof(VHDFooter);

    LogFlowFunc(("returns %lld\n", cb));
    return cb;
}

/** @copydoc VBOXHDDBACKEND::pfnGetPCHSGeometry */
static int vhdGetPCHSGeometry(void *pBackendData, PVDGEOMETRY pPCHSGeometry)
{
    LogFlowFunc(("pBackendData=%#p pPCHSGeometry=%#p\n", pBackendData, pPCHSGeometry));
    PVHDIMAGE pImage = (PVHDIMAGE)pBackendData;
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

    LogFlowFunc(("returns %Rrc (CHS=%u/%u/%u)\n", rc, pImage->PCHSGeometry.cCylinders, pImage->PCHSGeometry.cHeads, pImage->PCHSGeometry.cSectors));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetPCHSGeometry */
static int vhdSetPCHSGeometry(void *pBackendData, PCVDGEOMETRY pPCHSGeometry)
{
    LogFlowFunc(("pBackendData=%#p pPCHSGeometry=%#p PCHS=%u/%u/%u\n", pBackendData, pPCHSGeometry, pPCHSGeometry->cCylinders, pPCHSGeometry->cHeads, pPCHSGeometry->cSectors));
    PVHDIMAGE pImage = (PVHDIMAGE)pBackendData;
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
static int vhdGetLCHSGeometry(void *pBackendData, PVDGEOMETRY pLCHSGeometry)
{
LogFlowFunc(("pBackendData=%#p pLCHSGeometry=%#p\n", pBackendData, pLCHSGeometry));
    PVHDIMAGE pImage = (PVHDIMAGE)pBackendData;
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

    LogFlowFunc(("returns %Rrc (CHS=%u/%u/%u)\n", rc, pImage->LCHSGeometry.cCylinders, pImage->LCHSGeometry.cHeads, pImage->LCHSGeometry.cSectors));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetLCHSGeometry */
static int vhdSetLCHSGeometry(void *pBackendData, PCVDGEOMETRY pLCHSGeometry)
{
    PVHDIMAGE pImage = (PVHDIMAGE)pBackendData;
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
static unsigned vhdGetImageFlags(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PVHDIMAGE pImage = (PVHDIMAGE)pBackendData;
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
static unsigned vhdGetOpenFlags(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PVHDIMAGE pImage = (PVHDIMAGE)pBackendData;
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
static int vhdSetOpenFlags(void *pBackendData, unsigned uOpenFlags)
{
    LogFlowFunc(("pBackendData=%#p\n uOpenFlags=%#x", pBackendData, uOpenFlags));
    PVHDIMAGE pImage = (PVHDIMAGE)pBackendData;
    int rc;

    /* Image must be opened and the new flags must be valid. */
    if (!pImage || (uOpenFlags & ~(VD_OPEN_FLAGS_READONLY | VD_OPEN_FLAGS_INFO | VD_OPEN_FLAGS_ASYNC_IO | VD_OPEN_FLAGS_SHAREABLE | VD_OPEN_FLAGS_SEQUENTIAL)))
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    /* Implement this operation via reopening the image. */
    rc = vhdFreeImage(pImage, false);
    if (RT_FAILURE(rc))
        goto out;
    rc = vhdOpenImage(pImage, uOpenFlags);

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetComment */
static int vhdGetComment(void *pBackendData, char *pszComment,
                         size_t cbComment)
{
    LogFlowFunc(("pBackendData=%#p pszComment=%#p cbComment=%zu\n", pBackendData, pszComment, cbComment));
    PVHDIMAGE pImage = (PVHDIMAGE)pBackendData;
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
static int vhdSetComment(void *pBackendData, const char *pszComment)
{
    LogFlowFunc(("pBackendData=%#p pszComment=\"%s\"\n", pBackendData, pszComment));
    PVHDIMAGE pImage = (PVHDIMAGE)pBackendData;
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
static int vhdGetUuid(void *pBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p pUuid=%#p\n", pBackendData, pUuid));
    PVHDIMAGE pImage = (PVHDIMAGE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
    {
        *pUuid = pImage->ImageUuid;
        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc (%RTuuid)\n", rc, pUuid));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetUuid */
static int vhdSetUuid(void *pBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p Uuid=%RTuuid\n", pBackendData, pUuid));
    PVHDIMAGE pImage = (PVHDIMAGE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
    {
        if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
        {
            pImage->ImageUuid = *pUuid;
            /* Update the footer copy. It will get written to disk when the image is closed. */
            memcpy(&pImage->vhdFooterCopy.UniqueID, pUuid, 16);
            /* Update checksum. */
            pImage->vhdFooterCopy.Checksum = 0;
            pImage->vhdFooterCopy.Checksum = RT_H2BE_U32(vhdChecksum(&pImage->vhdFooterCopy, sizeof(VHDFooter)));

            /* Need to update the dynamic disk header to update the disk footer copy at the beginning. */
            if (!(pImage->uImageFlags & VD_IMAGE_FLAGS_FIXED))
                pImage->fDynHdrNeedsUpdate = true;
            rc = VINF_SUCCESS;
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
static int vhdGetModificationUuid(void *pBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p pUuid=%#p\n", pBackendData, pUuid));
    PVHDIMAGE pImage = (PVHDIMAGE)pBackendData;
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
static int vhdSetModificationUuid(void *pBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p Uuid=%RTuuid\n", pBackendData, pUuid));
    PVHDIMAGE pImage = (PVHDIMAGE)pBackendData;
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
static int vhdGetParentUuid(void *pBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p pUuid=%#p\n", pBackendData, pUuid));
    PVHDIMAGE pImage = (PVHDIMAGE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
    {
        *pUuid = pImage->ParentUuid;
        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc (%RTuuid)\n", rc, pUuid));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetParentUuid */
static int vhdSetParentUuid(void *pBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p Uuid=%RTuuid\n", pBackendData, pUuid));
    PVHDIMAGE pImage = (PVHDIMAGE)pBackendData;
    int rc = VINF_SUCCESS;

    AssertPtr(pImage);

    if (pImage && pImage->pStorage)
    {
        if (!(pImage->uImageFlags & VD_IMAGE_FLAGS_FIXED))
        {
            pImage->ParentUuid = *pUuid;
            pImage->fDynHdrNeedsUpdate = true;
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
static int vhdGetParentModificationUuid(void *pBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p pUuid=%#p\n", pBackendData, pUuid));
    PVHDIMAGE pImage = (PVHDIMAGE)pBackendData;
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
static int vhdSetParentModificationUuid(void *pBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p Uuid=%RTuuid\n", pBackendData, pUuid));
    PVHDIMAGE pImage = (PVHDIMAGE)pBackendData;
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
static void vhdDump(void *pBackendData)
{
    PVHDIMAGE pImage = (PVHDIMAGE)pBackendData;

    AssertPtr(pImage);
    if (pImage)
    {
        vdIfErrorMessage(pImage->pIfError, "Header: Geometry PCHS=%u/%u/%u LCHS=%u/%u/%u cbSector=%llu\n",
                         pImage->PCHSGeometry.cCylinders, pImage->PCHSGeometry.cHeads, pImage->PCHSGeometry.cSectors,
                         pImage->LCHSGeometry.cCylinders, pImage->LCHSGeometry.cHeads, pImage->LCHSGeometry.cSectors,
                         VHD_SECTOR_SIZE);
        vdIfErrorMessage(pImage->pIfError, "Header: uuidCreation={%RTuuid}\n", &pImage->ImageUuid);
        vdIfErrorMessage(pImage->pIfError, "Header: uuidParent={%RTuuid}\n", &pImage->ParentUuid);
    }
}

/** @copydoc VBOXHDDBACKEND::pfnGetTimestamp */
static int vhdGetTimeStamp(void *pBackendData, PRTTIMESPEC pTimeStamp)
{
    int rc = VINF_SUCCESS;
    PVHDIMAGE pImage = (PVHDIMAGE)pBackendData;

    AssertPtr(pImage);

    if (pImage)
        rc = vdIfIoIntFileGetModificationTime(pImage->pIfIo, pImage->pszFilename, pTimeStamp);
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetParentTimeStamp */
static int vhdGetParentTimeStamp(void *pBackendData, PRTTIMESPEC pTimeStamp)
{
    int rc = VINF_SUCCESS;
    PVHDIMAGE pImage = (PVHDIMAGE)pBackendData;

    AssertPtr(pImage);

    if (pImage)
        vhdTime2RtTime(pTimeStamp, pImage->u32ParentTimeStamp);
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetParentTimeStamp */
static int vhdSetParentTimeStamp(void *pBackendData, PCRTTIMESPEC pTimeStamp)
{
    int rc = VINF_SUCCESS;
    PVHDIMAGE pImage = (PVHDIMAGE)pBackendData;

    AssertPtr(pImage);
    if (pImage)
    {
        if (pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
            rc = VERR_VD_IMAGE_READ_ONLY;
        else
        {
            pImage->u32ParentTimeStamp = vhdRtTime2VhdTime(pTimeStamp);
            pImage->fDynHdrNeedsUpdate = true;
        }
    }
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetParentFilename */
static int vhdGetParentFilename(void *pBackendData, char **ppszParentFilename)
{
    int rc = VINF_SUCCESS;
    PVHDIMAGE pImage = (PVHDIMAGE)pBackendData;

    AssertPtr(pImage);
    if (pImage)
        *ppszParentFilename = RTStrDup(pImage->pszParentFilename);
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetParentFilename */
static int vhdSetParentFilename(void *pBackendData, const char *pszParentFilename)
{
    int rc = VINF_SUCCESS;
    PVHDIMAGE pImage = (PVHDIMAGE)pBackendData;

    AssertPtr(pImage);
    if (pImage)
    {
        if (pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
            rc = VERR_VD_IMAGE_READ_ONLY;
        else
        {
            if (pImage->pszParentFilename)
                RTStrFree(pImage->pszParentFilename);
            pImage->pszParentFilename = RTStrDup(pszParentFilename);
            if (!pImage->pszParentFilename)
                rc = VERR_NO_MEMORY;
            else
                pImage->fDynHdrNeedsUpdate = true;
        }
    }
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnAsyncRead */
static int vhdAsyncRead(void *pBackendData, uint64_t uOffset, size_t cbRead,
                        PVDIOCTX pIoCtx, size_t *pcbActuallyRead)
{
    PVHDIMAGE pImage = (PVHDIMAGE)pBackendData;
    int rc = VINF_SUCCESS;

    LogFlowFunc(("pBackendData=%p uOffset=%#llx pIoCtx=%#p cbRead=%u pcbActuallyRead=%p\n", pBackendData, uOffset, pIoCtx, cbRead, pcbActuallyRead));

    if (uOffset + cbRead > pImage->cbSize)
        return VERR_INVALID_PARAMETER;

    /*
     * If we have a dynamic disk image, we need to find the data block and sector to read.
     */
    if (pImage->pBlockAllocationTable)
    {
        /*
         * Get the data block first.
         */
        uint32_t cBlockAllocationTableEntry = (uOffset / VHD_SECTOR_SIZE) / pImage->cSectorsPerDataBlock;
        uint32_t cBATEntryIndex = (uOffset / VHD_SECTOR_SIZE) % pImage->cSectorsPerDataBlock;
        uint64_t uVhdOffset;

        LogFlowFunc(("cBlockAllocationTableEntry=%u cBatEntryIndex=%u\n", cBlockAllocationTableEntry, cBATEntryIndex));
        LogFlowFunc(("BlockAllocationEntry=%u\n", pImage->pBlockAllocationTable[cBlockAllocationTableEntry]));

        /*
         * Clip read range to remain in this data block.
         */
        cbRead = RT_MIN(cbRead, (pImage->cbDataBlock - (cBATEntryIndex * VHD_SECTOR_SIZE)));

        /*
         * If the block is not allocated the content of the entry is ~0
         */
        if (pImage->pBlockAllocationTable[cBlockAllocationTableEntry] == ~0U)
            rc = VERR_VD_BLOCK_FREE;
        else
        {
            uVhdOffset = ((uint64_t)pImage->pBlockAllocationTable[cBlockAllocationTableEntry] + pImage->cDataBlockBitmapSectors + cBATEntryIndex) * VHD_SECTOR_SIZE;
            LogFlowFunc(("uVhdOffset=%llu cbRead=%u\n", uVhdOffset, cbRead));

            /* Read in the block's bitmap. */
            PVDMETAXFER pMetaXfer;
            rc = vdIfIoIntFileReadMetaAsync(pImage->pIfIo, pImage->pStorage,
                                            ((uint64_t)pImage->pBlockAllocationTable[cBlockAllocationTableEntry]) * VHD_SECTOR_SIZE,
                                            pImage->pu8Bitmap, pImage->cbDataBlockBitmap,
                                            pIoCtx, &pMetaXfer, NULL, NULL);

            if (RT_SUCCESS(rc))
            {
                uint32_t cSectors = 0;

                vdIfIoIntMetaXferRelease(pImage->pIfIo, pMetaXfer);
                if (vhdBlockBitmapSectorContainsData(pImage, cBATEntryIndex))
                {
                    cBATEntryIndex++;
                    cSectors = 1;

                    /*
                     * The first sector being read is marked dirty, read as much as we
                     * can from child. Note that only sectors that are marked dirty
                     * must be read from child.
                     */
                    while (   (cSectors < (cbRead / VHD_SECTOR_SIZE))
                           && vhdBlockBitmapSectorContainsData(pImage, cBATEntryIndex))
                    {
                        cBATEntryIndex++;
                        cSectors++;
                    }

                    cbRead = cSectors * VHD_SECTOR_SIZE;

                    LogFlowFunc(("uVhdOffset=%llu cbRead=%u\n", uVhdOffset, cbRead));
                    rc = vdIfIoIntFileReadUserAsync(pImage->pIfIo, pImage->pStorage,
                                                    uVhdOffset, pIoCtx, cbRead);
                }
                else
                {
                    /*
                     * The first sector being read is marked clean, so we should read from
                     * our parent instead, but only as much as there are the following
                     * clean sectors, because the block may still contain dirty sectors
                     * further on. We just need to compute the number of clean sectors
                     * and pass it to our caller along with the notification that they
                     * should be read from the parent.
                     */
                    cBATEntryIndex++;
                    cSectors = 1;

                    while (   (cSectors < (cbRead / VHD_SECTOR_SIZE))
                           && !vhdBlockBitmapSectorContainsData(pImage, cBATEntryIndex))
                    {
                        cBATEntryIndex++;
                        cSectors++;
                    }

                    cbRead = cSectors * VHD_SECTOR_SIZE;
                    LogFunc(("Sectors free: uVhdOffset=%llu cbRead=%u\n", uVhdOffset, cbRead));
                    rc = VERR_VD_BLOCK_FREE;
                }
            }
            else
                AssertMsg(rc == VERR_VD_NOT_ENOUGH_METADATA, ("Reading block bitmap failed rc=%Rrc\n", rc));
        }
    }
    else
        rc = vdIfIoIntFileReadUserAsync(pImage->pIfIo, pImage->pStorage, uOffset, pIoCtx, cbRead);

    if (pcbActuallyRead)
        *pcbActuallyRead = cbRead;

    LogFlowFunc(("returns rc=%Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnAsyncWrite */
static int vhdAsyncWrite(void *pBackendData, uint64_t uOffset, size_t cbWrite,
                         PVDIOCTX pIoCtx,
                         size_t *pcbWriteProcess, size_t *pcbPreRead,
                         size_t *pcbPostRead, unsigned fWrite)
{
    PVHDIMAGE pImage = (PVHDIMAGE)pBackendData;
    int rc = VINF_SUCCESS;

    LogFlowFunc(("pBackendData=%p uOffset=%llu pIoCtx=%#p cbWrite=%u pcbWriteProcess=%p pcbPreRead=%p pcbPostRead=%p fWrite=%u\n",
             pBackendData, uOffset, pIoCtx, cbWrite, pcbWriteProcess, pcbPreRead, pcbPostRead, fWrite));

    AssertPtr(pImage);
    Assert(uOffset % VHD_SECTOR_SIZE == 0);
    Assert(cbWrite % VHD_SECTOR_SIZE == 0);

    if (pImage->pBlockAllocationTable)
    {
        /*
         * Get the data block first.
         */
        uint32_t cSector = uOffset / VHD_SECTOR_SIZE;
        uint32_t cBlockAllocationTableEntry = cSector / pImage->cSectorsPerDataBlock;
        uint32_t cBATEntryIndex = cSector % pImage->cSectorsPerDataBlock;
        uint64_t uVhdOffset;

        /*
         * Clip write range.
         */
        cbWrite = RT_MIN(cbWrite, (pImage->cbDataBlock - (cBATEntryIndex * VHD_SECTOR_SIZE)));

        /*
         * If the block is not allocated the content of the entry is ~0
         * and we need to allocate a new block. Note that while blocks are
         * allocated with a relatively big granularity, each sector has its
         * own bitmap entry, indicating whether it has been written or not.
         * So that means for the purposes of the higher level that the
         * granularity is invisible. This means there's no need to return
         * VERR_VD_BLOCK_FREE unless the block hasn't been allocated yet.
         */
        if (pImage->pBlockAllocationTable[cBlockAllocationTableEntry] == ~0U)
        {
            /* Check if the block allocation should be suppressed. */
            if (fWrite & VD_WRITE_NO_ALLOC)
            {
                *pcbPreRead = cBATEntryIndex * VHD_SECTOR_SIZE;
                *pcbPostRead = pImage->cSectorsPerDataBlock * VHD_SECTOR_SIZE - cbWrite - *pcbPreRead;

                if (pcbWriteProcess)
                    *pcbWriteProcess = cbWrite;
                return VERR_VD_BLOCK_FREE;
            }

            PVHDIMAGEEXPAND pExpand = (PVHDIMAGEEXPAND)RTMemAllocZ(RT_OFFSETOF(VHDIMAGEEXPAND, au8Bitmap[pImage->cDataBlockBitmapSectors * VHD_SECTOR_SIZE]));
            bool fIoInProgress = false;

            if (!pExpand)
                return VERR_NO_MEMORY;

            pExpand->cbEofOld = pImage->uCurrentEndOfFile;
            pExpand->idxBatAllocated = cBlockAllocationTableEntry;
            pExpand->idxBlockBe = RT_H2BE_U32(pImage->uCurrentEndOfFile / VHD_SECTOR_SIZE);

            /* Set the bits for all sectors having been written. */
            for (uint32_t iSector = 0; iSector < (cbWrite / VHD_SECTOR_SIZE); iSector++)
            {
                /* No need to check for a changed value because this is an initial write. */
                vhdBlockBitmapSectorSet(pImage, pExpand->au8Bitmap, cBATEntryIndex);
                cBATEntryIndex++;
            }

            do
            {
                /*
                 * Start with the sector bitmap.
                 */
                rc = vdIfIoIntFileWriteMetaAsync(pImage->pIfIo, pImage->pStorage,
                                                 pImage->uCurrentEndOfFile,
                                                 pExpand->au8Bitmap,
                                                 pImage->cbDataBlockBitmap, pIoCtx,
                                                 vhdAsyncExpansionDataBlockBitmapComplete,
                                                 pExpand);
                if (RT_SUCCESS(rc))
                    VHDIMAGEEXPAND_STATUS_SET(pExpand->fFlags, VHDIMAGEEXPAND_BLOCKBITMAP_STATUS_SHIFT, VHDIMAGEEXPAND_STEP_SUCCESS);
                else if (rc == VERR_VD_ASYNC_IO_IN_PROGRESS)
                    fIoInProgress = true;
                else
                {
                    VHDIMAGEEXPAND_STATUS_SET(pExpand->fFlags, VHDIMAGEEXPAND_BLOCKBITMAP_STATUS_SHIFT, VHDIMAGEEXPAND_STEP_FAILED);
                    VHDIMAGEEXPAND_STATUS_SET(pExpand->fFlags, VHDIMAGEEXPAND_USERBLOCK_STATUS_SHIFT, VHDIMAGEEXPAND_STEP_FAILED);
                    VHDIMAGEEXPAND_STATUS_SET(pExpand->fFlags, VHDIMAGEEXPAND_BAT_STATUS_SHIFT, VHDIMAGEEXPAND_STEP_FAILED);
                    VHDIMAGEEXPAND_STATUS_SET(pExpand->fFlags, VHDIMAGEEXPAND_FOOTER_STATUS_SHIFT, VHDIMAGEEXPAND_STEP_FAILED);
                    break;
                }


                /*
                 * Write the new block at the current end of the file.
                 */
                rc = vdIfIoIntFileWriteUserAsync(pImage->pIfIo, pImage->pStorage,
                                                 pImage->uCurrentEndOfFile + pImage->cbDataBlockBitmap,
                                                 pIoCtx, cbWrite,
                                                 vhdAsyncExpansionDataComplete,
                                                 pExpand);
                if (RT_SUCCESS(rc))
                    VHDIMAGEEXPAND_STATUS_SET(pExpand->fFlags, VHDIMAGEEXPAND_USERBLOCK_STATUS_SHIFT, VHDIMAGEEXPAND_STEP_SUCCESS);
                else if (rc == VERR_VD_ASYNC_IO_IN_PROGRESS)
                    fIoInProgress = true;
                else
                {
                    VHDIMAGEEXPAND_STATUS_SET(pExpand->fFlags, VHDIMAGEEXPAND_USERBLOCK_STATUS_SHIFT, VHDIMAGEEXPAND_STEP_FAILED);
                    VHDIMAGEEXPAND_STATUS_SET(pExpand->fFlags, VHDIMAGEEXPAND_BAT_STATUS_SHIFT, VHDIMAGEEXPAND_STEP_FAILED);
                    VHDIMAGEEXPAND_STATUS_SET(pExpand->fFlags, VHDIMAGEEXPAND_FOOTER_STATUS_SHIFT, VHDIMAGEEXPAND_STEP_FAILED);
                    break;
                }

                /*
                 * Write entry in the BAT.
                 */
                rc = vdIfIoIntFileWriteMetaAsync(pImage->pIfIo, pImage->pStorage,
                                                 pImage->uBlockAllocationTableOffset + cBlockAllocationTableEntry * sizeof(uint32_t),
                                                 &pExpand->idxBlockBe,
                                                 sizeof(uint32_t), pIoCtx,
                                                 vhdAsyncExpansionBatUpdateComplete,
                                                 pExpand);
                if (RT_SUCCESS(rc))
                    VHDIMAGEEXPAND_STATUS_SET(pExpand->fFlags, VHDIMAGEEXPAND_BAT_STATUS_SHIFT, VHDIMAGEEXPAND_STEP_SUCCESS);
                else if (rc == VERR_VD_ASYNC_IO_IN_PROGRESS)
                    fIoInProgress = true;
                else
                {
                    VHDIMAGEEXPAND_STATUS_SET(pExpand->fFlags, VHDIMAGEEXPAND_BAT_STATUS_SHIFT, VHDIMAGEEXPAND_STEP_FAILED);
                    VHDIMAGEEXPAND_STATUS_SET(pExpand->fFlags, VHDIMAGEEXPAND_FOOTER_STATUS_SHIFT, VHDIMAGEEXPAND_STEP_FAILED);
                    break;
                }

                /*
                 * Set the new end of the file and link the new block into the BAT.
                 */
                pImage->pBlockAllocationTable[cBlockAllocationTableEntry] = pImage->uCurrentEndOfFile / VHD_SECTOR_SIZE;
                pImage->uCurrentEndOfFile += pImage->cbDataBlockBitmap + pImage->cbDataBlock;

                /* Update the footer. */
                rc = vdIfIoIntFileWriteMetaAsync(pImage->pIfIo, pImage->pStorage,
                                                 pImage->uCurrentEndOfFile,
                                                 &pImage->vhdFooterCopy,
                                                 sizeof(VHDFooter), pIoCtx,
                                                 vhdAsyncExpansionFooterUpdateComplete,
                                                 pExpand);
                if (RT_SUCCESS(rc))
                    VHDIMAGEEXPAND_STATUS_SET(pExpand->fFlags, VHDIMAGEEXPAND_FOOTER_STATUS_SHIFT, VHDIMAGEEXPAND_STEP_SUCCESS);
                else if (rc == VERR_VD_ASYNC_IO_IN_PROGRESS)
                    fIoInProgress = true;
                else
                {
                    VHDIMAGEEXPAND_STATUS_SET(pExpand->fFlags, VHDIMAGEEXPAND_FOOTER_STATUS_SHIFT, VHDIMAGEEXPAND_STEP_FAILED);
                    break;
                }

            } while (0);

            if (!fIoInProgress)
                vhdAsyncExpansionComplete(pImage, pIoCtx, pExpand);
            else
                rc = VERR_VD_ASYNC_IO_IN_PROGRESS;
        }
        else
        {
            /*
             * Calculate the real offset in the file.
             */
            uVhdOffset = ((uint64_t)pImage->pBlockAllocationTable[cBlockAllocationTableEntry] + pImage->cDataBlockBitmapSectors + cBATEntryIndex) * VHD_SECTOR_SIZE;

            /* Read in the block's bitmap. */
            PVDMETAXFER pMetaXfer;
            rc = vdIfIoIntFileReadMetaAsync(pImage->pIfIo, pImage->pStorage,
                                            ((uint64_t)pImage->pBlockAllocationTable[cBlockAllocationTableEntry]) * VHD_SECTOR_SIZE,
                                            pImage->pu8Bitmap,
                                            pImage->cbDataBlockBitmap, pIoCtx,
                                            &pMetaXfer, NULL, NULL);
            if (RT_SUCCESS(rc))
            {
                vdIfIoIntMetaXferRelease(pImage->pIfIo, pMetaXfer);

                /* Write data. */
                rc = vdIfIoIntFileWriteUserAsync(pImage->pIfIo, pImage->pStorage,
                                                 uVhdOffset, pIoCtx, cbWrite,
                                                 NULL, NULL);
                if (RT_SUCCESS(rc) || rc == VERR_VD_ASYNC_IO_IN_PROGRESS)
                {
                    bool fChanged = false;

                    /* Set the bits for all sectors having been written. */
                    for (uint32_t iSector = 0; iSector < (cbWrite / VHD_SECTOR_SIZE); iSector++)
                    {
                        fChanged |= vhdBlockBitmapSectorSet(pImage, pImage->pu8Bitmap, cBATEntryIndex);
                        cBATEntryIndex++;
                    }

                    /* Only write the bitmap if it was changed. */
                    if (fChanged)
                    {
                        /*
                         * Write the bitmap back.
                         *
                         * @note We don't have a completion callback here because we
                         * can't do anything if the write fails for some reason.
                         * The error will propagated to the device/guest
                         * by the generic VD layer already and we don't need
                         * to rollback anything here.
                         */
                        rc = vdIfIoIntFileWriteMetaAsync(pImage->pIfIo, pImage->pStorage,
                                                         ((uint64_t)pImage->pBlockAllocationTable[cBlockAllocationTableEntry]) * VHD_SECTOR_SIZE,
                                                         pImage->pu8Bitmap,
                                                         pImage->cbDataBlockBitmap,
                                                         pIoCtx, NULL, NULL);
                    }
                }
            }
        }
    }
    else
        rc = vdIfIoIntFileWriteUserAsync(pImage->pIfIo, pImage->pStorage,
                                         uOffset, pIoCtx, cbWrite, NULL, NULL);

    if (pcbWriteProcess)
        *pcbWriteProcess = cbWrite;

    /* Stay on the safe side. Do not run the risk of confusing the higher
     * level, as that can be pretty lethal to image consistency. */
    *pcbPreRead = 0;
    *pcbPostRead = 0;

    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnAsyncFlush */
static int vhdAsyncFlush(void *pBackendData, PVDIOCTX pIoCtx)
{
    PVHDIMAGE pImage = (PVHDIMAGE)pBackendData;

    /* No need to write anything here. Data is always updated on a write. */
    return vdIfIoIntFileFlushAsync(pImage->pIfIo, pImage->pStorage,
                                   pIoCtx, NULL, NULL);
}

/** @copydoc VBOXHDDBACKEND::pfnCompact */
static int vhdCompact(void *pBackendData, unsigned uPercentStart,
                      unsigned uPercentSpan, PVDINTERFACE pVDIfsDisk,
                      PVDINTERFACE pVDIfsImage, PVDINTERFACE pVDIfsOperation)
{
    PVHDIMAGE pImage = (PVHDIMAGE)pBackendData;
    int rc = VINF_SUCCESS;
    void *pvBuf = NULL, *pvReplace = NULL;
    uint32_t *paBlocks = NULL;

    int (*pfnParentRead)(void *, uint64_t, void *, size_t) = NULL;
    void *pvParent = NULL;
    PVDINTERFACEPARENTSTATE pIfParentState = VDIfParentStateGet(pVDIfsOperation);
    if (pIfParentState)
    {
        pfnParentRead = pIfParentState->pfnParentRead;
        pvParent = pIfParentState->Core.pvUser;
    }

    PFNVDPROGRESS pfnProgress = NULL;
    void *pvUser = NULL;
    PVDINTERFACEPROGRESS pIfProgress = VDIfProgressGet(pVDIfsOperation);
    if (pIfProgress)
    {
        pfnProgress = pIfProgress->pfnProgress;
        pvUser = pIfProgress->Core.pvUser;
    }

    do
    {
        AssertBreakStmt(pImage, rc = VERR_INVALID_PARAMETER);

        AssertBreakStmt(!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY),
                        rc = VERR_VD_IMAGE_READ_ONLY);

        /* Reject fixed images as they don't have a BAT. */
        if (pImage->uImageFlags & VD_IMAGE_FLAGS_FIXED)
        {
            rc = VERR_NOT_SUPPORTED;
            break;
        }

        if (pfnParentRead)
        {
            pvParent = RTMemTmpAlloc(pImage->cbDataBlock);
            AssertBreakStmt(VALID_PTR(pvBuf), rc = VERR_NO_MEMORY);
        }
        pvBuf = RTMemTmpAlloc(pImage->cbDataBlock);
        AssertBreakStmt(VALID_PTR(pvBuf), rc = VERR_NO_MEMORY);

        unsigned cBlocksAllocated = 0;
        unsigned cBlocksToMove    = 0;
        unsigned cBlocks          = pImage->cBlockAllocationTableEntries;
        uint32_t offBlocksStart   = ~0U; /* Start offset of data blocks in sectors. */
        uint32_t *paBat           = pImage->pBlockAllocationTable;

        /* Count the number of allocated blocks and find the start offset for the data blocks. */
        for (unsigned i = 0; i < cBlocks; i++)
            if (paBat[i] != ~0U)
            {
                cBlocksAllocated++;
                if (paBat[i] < offBlocksStart)
                    offBlocksStart = paBat[i];
            }

        if (!cBlocksAllocated)
        {
            /* Nothing to do. */
            rc = VINF_SUCCESS;
            break;
        }

        paBlocks = (uint32_t *)RTMemTmpAllocZ(cBlocksAllocated * sizeof(uint32_t));
        AssertBreakStmt(VALID_PTR(paBlocks), rc = VERR_NO_MEMORY);

        /* Invalidate the back resolving array. */
        for (unsigned i = 0; i < cBlocksAllocated; i++)
            paBlocks[i] = ~0U;

        /* Fill the back resolving table. */
        for (unsigned i = 0; i < cBlocks; i++)
            if (paBat[i] != ~0U)
            {
                unsigned idxBlock = (paBat[i] - offBlocksStart) / pImage->cSectorsPerDataBlock;
                if (   idxBlock < cBlocksAllocated
                    && paBlocks[idxBlock] == ~0U)
                    paBlocks[idxBlock] = i;
                else
                {
                    /* The image is in an inconsistent state. Don't go further. */
                    rc = VERR_INVALID_STATE;
                    break;
                }
            }

        if (RT_FAILURE(rc))
            break;

        /* Find redundant information and update the block pointers
         * accordingly, creating bubbles. Keep disk up to date, as this
         * enables cancelling. */
        for (unsigned i = 0; i < cBlocks; i++)
        {
            if (paBat[i] != ~0U)
            {
                unsigned idxBlock = (paBat[i] - offBlocksStart) / pImage->cSectorsPerDataBlock;

                /* Block present in image file, read relevant data. */
                uint64_t u64Offset = ((uint64_t)paBat[i] + pImage->cDataBlockBitmapSectors) * VHD_SECTOR_SIZE;
                rc = vdIfIoIntFileReadSync(pImage->pIfIo, pImage->pStorage,
                                           u64Offset, pvBuf, pImage->cbDataBlock, NULL);
                if (RT_FAILURE(rc))
                    break;

                if (ASMBitFirstSet((volatile void *)pvBuf, (uint32_t)pImage->cbDataBlock * 8) == -1)
                {
                    paBat[i] = ~0;
                    paBlocks[idxBlock] = ~0U;
                    /* Adjust progress info, one block to be relocated. */
                    cBlocksToMove++;
                }
                else if (pfnParentRead)
                {
                    rc = pfnParentRead(pvParent, i * pImage->cbDataBlock, pvParent, pImage->cbDataBlock);
                    if (RT_FAILURE(rc))
                        break;
                    if (!memcmp(pvParent, pvBuf, pImage->cbDataBlock))
                    {
                        paBat[i] = ~0U;
                        paBlocks[idxBlock] = ~0U;
                        /* Adjust progress info, one block to be relocated. */
                        cBlocksToMove++;
                    }
                }
            }

            if (pIfProgress && pIfProgress->pfnProgress)
            {
                rc = pIfProgress->pfnProgress(pIfProgress->Core.pvUser,
                                              (uint64_t)i * uPercentSpan / (cBlocks + cBlocksToMove) + uPercentStart);
                if (RT_FAILURE(rc))
                    break;
            }
        }

        if (RT_SUCCESS(rc))
        {
            /* Fill bubbles with other data (if available). */
            unsigned cBlocksMoved = 0;
            unsigned uBlockUsedPos = cBlocksAllocated;
            size_t   cbBlock = pImage->cbDataBlock + pImage->cbDataBlockBitmap; /** < Size of whole block containing the bitmap and the user data. */

            /* Allocate data buffer to hold the data block and allocation bitmap in front of the actual data. */
            RTMemTmpFree(pvBuf);
            pvBuf = RTMemTmpAllocZ(cbBlock);
            AssertBreakStmt(VALID_PTR(pvBuf), rc = VERR_NO_MEMORY);

            for (unsigned i = 0; i < cBlocksAllocated; i++)
            {
                unsigned uBlock = paBlocks[i];
                if (uBlock == ~0U)
                {
                    unsigned uBlockData = ~0U;
                    while (uBlockUsedPos > i && uBlockData == ~0U)
                    {
                        uBlockUsedPos--;
                        uBlockData = paBlocks[uBlockUsedPos];
                    }
                    /* Terminate early if there is no block which needs copying. */
                    if (uBlockUsedPos == i)
                        break;
                    uint64_t u64Offset = (uint64_t)uBlockUsedPos * cbBlock
                                       + (offBlocksStart * VHD_SECTOR_SIZE);
                    rc = vdIfIoIntFileReadSync(pImage->pIfIo, pImage->pStorage,
                                               u64Offset, pvBuf, cbBlock, NULL);
                    if (RT_FAILURE(rc))
                        break;

                    u64Offset = (uint64_t)i * cbBlock
                                       + (offBlocksStart * VHD_SECTOR_SIZE);
                    rc = vdIfIoIntFileWriteSync(pImage->pIfIo, pImage->pStorage,
                                                u64Offset, pvBuf, cbBlock, NULL);
                    if (RT_FAILURE(rc))
                        break;

                    paBat[uBlockData] = i*(pImage->cSectorsPerDataBlock + pImage->cDataBlockBitmapSectors) + offBlocksStart;

                    /* Truncate the file but leave enough room for the footer to avoid
                     * races if other processes fill the whole harddisk. */
                    rc = vdIfIoIntFileSetSize(pImage->pIfIo, pImage->pStorage,
                                              pImage->uCurrentEndOfFile - cbBlock + VHD_SECTOR_SIZE);
                    if (RT_FAILURE(rc))
                        break;

                    /* Update pointers and write footer. */
                    pImage->uCurrentEndOfFile -= cbBlock;

                    /* We're kinda screwed if this failes. */
                    rc = vhdUpdateFooter(pImage);
                    if (RT_FAILURE(rc))
                        break;

                    paBlocks[i] = uBlockData;
                    paBlocks[uBlockUsedPos] = ~0U;
                    cBlocksMoved++;
                }

                if (pIfProgress && pIfProgress->pfnProgress)
                {
                    rc = pIfProgress->pfnProgress(pIfProgress->Core.pvUser,
                                                  (uint64_t)(cBlocks + cBlocksMoved) * uPercentSpan / (cBlocks + cBlocksToMove) + uPercentStart);

                    if (RT_FAILURE(rc))
                        break;
                }
            }
        }

        /* Write the new BAT in any case. */
        rc = vhdFlushImage(pImage);
    } while (0);

    if (paBlocks)
        RTMemTmpFree(paBlocks);
    if (pvParent)
        RTMemTmpFree(pvParent);
    if (pvBuf)
        RTMemTmpFree(pvBuf);

    if (RT_SUCCESS(rc) && pIfProgress && pIfProgress->pfnProgress)
    {
        pIfProgress->pfnProgress(pIfProgress->Core.pvUser,
                                 uPercentStart + uPercentSpan);
    }

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnResize */
static int vhdResize(void *pBackendData, uint64_t cbSize,
                     PCVDGEOMETRY pPCHSGeometry, PCVDGEOMETRY pLCHSGeometry,
                     unsigned uPercentStart, unsigned uPercentSpan,
                     PVDINTERFACE pVDIfsDisk, PVDINTERFACE pVDIfsImage,
                     PVDINTERFACE pVDIfsOperation)
{
    PVHDIMAGE pImage = (PVHDIMAGE)pBackendData;
    int rc = VINF_SUCCESS;

    PFNVDPROGRESS pfnProgress = NULL;
    void *pvUser = NULL;
    PVDINTERFACEPROGRESS pIfProgress = VDIfProgressGet(pVDIfsOperation);
    if (pIfProgress)
    {
        pfnProgress = pIfProgress->pfnProgress;
        pvUser = pIfProgress->Core.pvUser;
    }

    /* Making the image smaller is not supported at the moment. */
    if (   cbSize < pImage->cbSize
        || pImage->uImageFlags & VD_IMAGE_FLAGS_FIXED)
        rc = VERR_NOT_SUPPORTED;
    else if (cbSize > pImage->cbSize)
    {
        unsigned cBlocksAllocated = 0;
        size_t cbBlock = pImage->cbDataBlock + pImage->cbDataBlockBitmap;     /** < Size of a block including the sector bitmap. */
        uint32_t cBlocksNew = cbSize / pImage->cbDataBlock;                   /** < New number of blocks in the image after the resize */
        if (cbSize % pImage->cbDataBlock)
            cBlocksNew++;

        uint32_t cBlocksOld      = pImage->cBlockAllocationTableEntries;      /** < Number of blocks before the resize. */
        uint64_t cbBlockspaceNew = RT_ALIGN_32(cBlocksNew * sizeof(uint32_t), VHD_SECTOR_SIZE);                         /** < Required space for the block array after the resize. */
        uint64_t offStartDataNew = RT_ALIGN_32(pImage->uBlockAllocationTableOffset + cbBlockspaceNew, VHD_SECTOR_SIZE); /** < New start offset for block data after the resize */
        uint64_t offStartDataOld = ~0ULL;

        /* Go through the BAT and find the data start offset. */
        for (unsigned idxBlock = 0; idxBlock < pImage->cBlockAllocationTableEntries; idxBlock++)
        {
            if (pImage->pBlockAllocationTable[idxBlock] != ~0U)
            {
                uint64_t offStartBlock = pImage->pBlockAllocationTable[idxBlock] * VHD_SECTOR_SIZE;
                if (offStartBlock < offStartDataOld)
                    offStartDataOld = offStartBlock;
                cBlocksAllocated++;
            }
        }

        if (   offStartDataOld != offStartDataNew
            && cBlocksAllocated > 0)
        {
            /* Calculate how many sectors nee to be relocated. */
            uint64_t cbOverlapping = offStartDataNew - offStartDataOld;
            unsigned cBlocksReloc = (unsigned)(cbOverlapping / cbBlock);
            if (cbOverlapping % cbBlock)
                cBlocksReloc++;

            cBlocksReloc = RT_MIN(cBlocksReloc, cBlocksAllocated);
            offStartDataNew = offStartDataOld;

            /* Do the relocation. */
            LogFlow(("Relocating %u blocks\n", cBlocksReloc));

            /*
             * Get the blocks we need to relocate first, they are appended to the end
             * of the image.
             */
            void *pvBuf = NULL, *pvZero = NULL;
            do
            {
                /* Allocate data buffer. */
                pvBuf = RTMemAllocZ(cbBlock);
                if (!pvBuf)
                {
                    rc = VERR_NO_MEMORY;
                    break;
                }

                /* Allocate buffer for overwriting with zeroes. */
                pvZero = RTMemAllocZ(cbBlock);
                if (!pvZero)
                {
                    rc = VERR_NO_MEMORY;
                    break;
                }

                for (unsigned i = 0; i < cBlocksReloc; i++)
                {
                    uint32_t uBlock = offStartDataNew / VHD_SECTOR_SIZE;

                    /* Search the index in the block table. */
                    for (unsigned idxBlock = 0; idxBlock < cBlocksOld; idxBlock++)
                    {
                        if (pImage->pBlockAllocationTable[idxBlock] == uBlock)
                        {
                            /* Read data and append to the end of the image. */
                            rc = vdIfIoIntFileReadSync(pImage->pIfIo, pImage->pStorage,
                                                       offStartDataNew, pvBuf, cbBlock, NULL);
                            if (RT_FAILURE(rc))
                                break;

                            rc = vdIfIoIntFileWriteSync(pImage->pIfIo, pImage->pStorage,
                                                        pImage->uCurrentEndOfFile, pvBuf, cbBlock, NULL);
                            if (RT_FAILURE(rc))
                                break;

                            /* Zero out the old block area. */
                            rc = vdIfIoIntFileWriteSync(pImage->pIfIo, pImage->pStorage,
                                                        offStartDataNew, pvZero, cbBlock, NULL);
                            if (RT_FAILURE(rc))
                                break;

                            /* Update block counter. */
                            pImage->pBlockAllocationTable[idxBlock] = pImage->uCurrentEndOfFile / VHD_SECTOR_SIZE;

                            pImage->uCurrentEndOfFile += cbBlock;

                            /* Continue with the next block. */
                            break;
                        }
                    }

                    if (RT_FAILURE(rc))
                        break;

                    offStartDataNew += cbBlock;
                }
            } while (0);

            if (pvBuf)
                RTMemFree(pvBuf);
            if (pvZero)
                RTMemFree(pvZero);
        }

        /*
         * Relocation done, expand the block array and update the header with
         * the new data.
         */
        if (RT_SUCCESS(rc))
        {
            uint32_t *paBlocksNew = (uint32_t *)RTMemRealloc(pImage->pBlockAllocationTable, cBlocksNew * sizeof(uint32_t));
            if (paBlocksNew)
            {
                pImage->pBlockAllocationTable = paBlocksNew;

                /* Mark the new blocks as unallocated. */
                for (unsigned idxBlock = cBlocksOld; idxBlock < cBlocksNew; idxBlock++)
                    pImage->pBlockAllocationTable[idxBlock] = ~0U;
            }
            else
                rc = VERR_NO_MEMORY;

            if (RT_SUCCESS(rc))
            {
                /* Write the block array before updating the rest. */
                rc = vdIfIoIntFileWriteSync(pImage->pIfIo, pImage->pStorage,
                                            pImage->uBlockAllocationTableOffset,
                                            pImage->pBlockAllocationTable,
                                            cBlocksNew * sizeof(uint32_t), NULL);
            }

            if (RT_SUCCESS(rc))
            {
                /* Update size and new block count. */
                pImage->cBlockAllocationTableEntries = cBlocksNew;
                pImage->cbSize = cbSize;

                /* Update geometry. */
                pImage->PCHSGeometry = *pPCHSGeometry;
                pImage->LCHSGeometry = *pLCHSGeometry;
            }
        }

        /* Update header information in base image file. */
        pImage->fDynHdrNeedsUpdate = true;
        vhdFlush(pImage);
    }
    /* Same size doesn't change the image at all. */

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}


VBOXHDDBACKEND g_VhdBackend =
{
    /* pszBackendName */
    "VHD",
    /* cbSize */
    sizeof(VBOXHDDBACKEND),
    /* uBackendCaps */
    VD_CAP_UUID | VD_CAP_DIFF | VD_CAP_FILE |
    VD_CAP_CREATE_FIXED | VD_CAP_CREATE_DYNAMIC |
    VD_CAP_ASYNC | VD_CAP_VFS,
    /* paFileExtensions */
    s_aVhdFileExtensions,
    /* paConfigInfo */
    NULL,
    /* hPlugin */
    NIL_RTLDRMOD,
    /* pfnCheckIfValid */
    vhdCheckIfValid,
    /* pfnOpen */
    vhdOpen,
    /* pfnCreate */
    vhdCreate,
    /* pfnRename */
    vhdRename,
    /* pfnClose */
    vhdClose,
    /* pfnRead */
    vhdRead,
    /* pfnWrite */
    vhdWrite,
    /* pfnFlush */
    vhdFlush,
    /* pfnGetVersion */
    vhdGetVersion,
    /* pfnGetSize */
    vhdGetSize,
    /* pfnGetFileSize */
    vhdGetFileSize,
    /* pfnGetPCHSGeometry */
    vhdGetPCHSGeometry,
    /* pfnSetPCHSGeometry */
    vhdSetPCHSGeometry,
    /* pfnGetLCHSGeometry */
    vhdGetLCHSGeometry,
    /* pfnSetLCHSGeometry */
    vhdSetLCHSGeometry,
    /* pfnGetImageFlags */
    vhdGetImageFlags,
    /* pfnGetOpenFlags */
    vhdGetOpenFlags,
    /* pfnSetOpenFlags */
    vhdSetOpenFlags,
    /* pfnGetComment */
    vhdGetComment,
    /* pfnSetComment */
    vhdSetComment,
    /* pfnGetUuid */
    vhdGetUuid,
    /* pfnSetUuid */
    vhdSetUuid,
    /* pfnGetModificationUuid */
    vhdGetModificationUuid,
    /* pfnSetModificationUuid */
    vhdSetModificationUuid,
    /* pfnGetParentUuid */
    vhdGetParentUuid,
    /* pfnSetParentUuid */
    vhdSetParentUuid,
    /* pfnGetParentModificationUuid */
    vhdGetParentModificationUuid,
    /* pfnSetParentModificationUuid */
    vhdSetParentModificationUuid,
    /* pfnDump */
    vhdDump,
    /* pfnGetTimeStamp */
    vhdGetTimeStamp,
    /* pfnGetParentTimeStamp */
    vhdGetParentTimeStamp,
    /* pfnSetParentTimeStamp */
    vhdSetParentTimeStamp,
    /* pfnGetParentFilename */
    vhdGetParentFilename,
    /* pfnSetParentFilename */
    vhdSetParentFilename,
    /* pfnAsyncRead */
    vhdAsyncRead,
    /* pfnAsyncWrite */
    vhdAsyncWrite,
    /* pfnAsyncFlush */
    vhdAsyncFlush,
    /* pfnComposeLocation */
    genericFileComposeLocation,
    /* pfnComposeName */
    genericFileComposeName,
    /* pfnCompact */
    vhdCompact,
    /* pfnResize */
    vhdResize,
    /* pfnDiscard */
    NULL
};
