/** $Id$ */
/** @file
 * Virtual Disk Image (VDI), Core Code Header (internal).
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

#ifndef __VDICore_h__


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#ifndef VBOX_VDICORE_VD
#include <VBox/VBoxHDD.h>
#else /* VBOX_VDICORE_VD */
#include <VBox/VBoxHDD-new.h>
#endif /* VBOX_VDICORE_VD */
#include <VBox/pdm.h>
#include <VBox/mm.h>
#include <VBox/err.h>

#include <VBox/log.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/uuid.h>
#include <iprt/file.h>
#include <iprt/string.h>
#include <iprt/asm.h>


/*******************************************************************************
*   Constants And Macros, Structures and Typedefs                              *
*******************************************************************************/

/** Image info, not handled anyhow.
 *  Must be less than 64 bytes in length, including the trailing 0.
 */
#define VDI_IMAGE_FILE_INFO   "<<< innotek VirtualBox Disk Image >>>\n"

/** The Sector size.
 * Currently we support only 512 bytes sectors.
 */
#define VDI_GEOMETRY_SECTOR_SIZE    (512)
/**  512 = 2^^9 */
#define VDI_GEOMETRY_SECTOR_SHIFT   (9)

/**
 * Harddisk geometry.
 */
#pragma pack(1)
typedef struct VDIDISKGEOMETRY
{
    /** Cylinders. */
    uint32_t    cCylinders;
    /** Heads. */
    uint32_t    cHeads;
    /** Sectors per track. */
    uint32_t    cSectors;
    /** Sector size. (bytes per sector) */
    uint32_t    cbSector;
} VDIDISKGEOMETRY, *PVDIDISKGEOMETRY;
#pragma pack()

/** Image signature. */
#define VDI_IMAGE_SIGNATURE   (0xbeda107f)

/**
 * Pre-Header to be stored in image file - used for version control.
 */
#pragma pack(1)
typedef struct VDIPREHEADER
{
    /** Just text info about image type, for eyes only. */
    char            szFileInfo[64];
    /** The image signature (VDI_IMAGE_SIGNATURE). */
    uint32_t        u32Signature;
    /** The image version (VDI_IMAGE_VERSION). */
    uint32_t        u32Version;
} VDIPREHEADER, *PVDIPREHEADER;
#pragma pack()

/**
 * Size of szComment field of HDD image header.
 */
#define VDI_IMAGE_COMMENT_SIZE    256

/**
 * Header to be stored in image file, VDI_IMAGE_VERSION_MAJOR = 0.
 * Prepended by VDIPREHEADER.
 */
#pragma pack(1)
typedef struct VDIHEADER0
{
    /** The image type (VDI_IMAGE_TYPE_*). */
    uint32_t        u32Type;
    /** Image flags (VDI_IMAGE_FLAGS_*). */
    uint32_t        fFlags;
    /** Image comment. (UTF-8) */
    char            szComment[VDI_IMAGE_COMMENT_SIZE];
    /** Legacy image geometry (previous code stored PCHS there). */
    VDIDISKGEOMETRY LegacyGeometry;
    /** Size of disk (in bytes). */
    uint64_t        cbDisk;
    /** Block size. (For instance VDI_IMAGE_BLOCK_SIZE.) */
    uint32_t        cbBlock;
    /** Number of blocks. */
    uint32_t        cBlocks;
    /** Number of allocated blocks. */
    uint32_t        cBlocksAllocated;
    /** UUID of image. */
    RTUUID          uuidCreate;
    /** UUID of image's last modification. */
    RTUUID          uuidModify;
    /** Only for secondary images - UUID of primary image. */
    RTUUID          uuidLinkage;
} VDIHEADER0, *PVDIHEADER0;
#pragma pack()

/**
 * Header to be stored in image file, VDI_IMAGE_VERSION_MAJOR = 1,
 * VDI_IMAGE_VERSION_MINOR = 1. Prepended by VDIPREHEADER.
 */
#pragma pack(1)
typedef struct VDIHEADER1
{
    /** Size of this structure in bytes. */
    uint32_t        cbHeader;
    /** The image type (VDI_IMAGE_TYPE_*). */
    uint32_t        u32Type;
    /** Image flags (VDI_IMAGE_FLAGS_*). */
    uint32_t        fFlags;
    /** Image comment. (UTF-8) */
    char            szComment[VDI_IMAGE_COMMENT_SIZE];
    /** Offset of Blocks array from the begining of image file.
     * Should be sector-aligned for HDD access optimization. */
    uint32_t        offBlocks;
    /** Offset of image data from the begining of image file.
     * Should be sector-aligned for HDD access optimization. */
    uint32_t        offData;
    /** Legacy image geometry (previous code stored PCHS there). */
    VDIDISKGEOMETRY LegacyGeometry;
    /** Was BIOS HDD translation mode, now unused. */
    uint32_t        u32Dummy;
    /** Size of disk (in bytes). */
    uint64_t        cbDisk;
    /** Block size. (For instance VDI_IMAGE_BLOCK_SIZE.) Should be a power of 2! */
    uint32_t        cbBlock;
    /** Size of additional service information of every data block.
     * Prepended before block data. May be 0.
     * Should be a power of 2 and sector-aligned for optimization reasons. */
    uint32_t        cbBlockExtra;
    /** Number of blocks. */
    uint32_t        cBlocks;
    /** Number of allocated blocks. */
    uint32_t        cBlocksAllocated;
    /** UUID of image. */
    RTUUID          uuidCreate;
    /** UUID of image's last modification. */
    RTUUID          uuidModify;
    /** Only for secondary images - UUID of previous image. */
    RTUUID          uuidLinkage;
    /** Only for secondary images - UUID of previous image's last modification. */
    RTUUID          uuidParentModify;
} VDIHEADER1, *PVDIHEADER1;
#pragma pack()

/**
 * Header to be stored in image file, VDI_IMAGE_VERSION_MAJOR = 1,
 * VDI_IMAGE_VERSION_MINOR = 1, the slightly changed variant necessary as the
 * old released code doesn't support changing the minor version at all.
 */
#pragma pack(1)
typedef struct VDIHEADER1PLUS
{
    /** Size of this structure in bytes. */
    uint32_t        cbHeader;
    /** The image type (VDI_IMAGE_TYPE_*). */
    uint32_t        u32Type;
    /** Image flags (VDI_IMAGE_FLAGS_*). */
    uint32_t        fFlags;
    /** Image comment. (UTF-8) */
    char            szComment[VDI_IMAGE_COMMENT_SIZE];
    /** Offset of Blocks array from the begining of image file.
     * Should be sector-aligned for HDD access optimization. */
    uint32_t        offBlocks;
    /** Offset of image data from the begining of image file.
     * Should be sector-aligned for HDD access optimization. */
    uint32_t        offData;
    /** Legacy image geometry (previous code stored PCHS there). */
    VDIDISKGEOMETRY LegacyGeometry;
    /** Was BIOS HDD translation mode, now unused. */
    uint32_t        u32Dummy;
    /** Size of disk (in bytes). */
    uint64_t        cbDisk;
    /** Block size. (For instance VDI_IMAGE_BLOCK_SIZE.) Should be a power of 2! */
    uint32_t        cbBlock;
    /** Size of additional service information of every data block.
     * Prepended before block data. May be 0.
     * Should be a power of 2 and sector-aligned for optimization reasons. */
    uint32_t        cbBlockExtra;
    /** Number of blocks. */
    uint32_t        cBlocks;
    /** Number of allocated blocks. */
    uint32_t        cBlocksAllocated;
    /** UUID of image. */
    RTUUID          uuidCreate;
    /** UUID of image's last modification. */
    RTUUID          uuidModify;
    /** Only for secondary images - UUID of previous image. */
    RTUUID          uuidLinkage;
    /** Only for secondary images - UUID of previous image's last modification. */
    RTUUID          uuidParentModify;
    /** LCHS image geometry (new field in VDI1.2 version. */
    VDIDISKGEOMETRY LCHSGeometry;
} VDIHEADER1PLUS, *PVDIHEADER1PLUS;
#pragma pack()

/**
 * Header structure for all versions.
 */
typedef struct VDIHEADER
{
    unsigned        uVersion;
    union
    {
        VDIHEADER0    v0;
        VDIHEADER1    v1;
        VDIHEADER1PLUS v1plus;
    } u;
} VDIHEADER, *PVDIHEADER;

/** Block 'pointer'. */
typedef uint32_t    VDIIMAGEBLOCKPOINTER;
/** Pointer to a block 'pointer'. */
typedef VDIIMAGEBLOCKPOINTER *PVDIIMAGEBLOCKPOINTER;

/**
 * Block marked as free is not allocated in image file, read from this
 * block may returns any random data.
 */
#define VDI_IMAGE_BLOCK_FREE   ((VDIIMAGEBLOCKPOINTER)~0)

/**
 * Block marked as zero is not allocated in image file, read from this
 * block returns zeroes.
 */
#define VDI_IMAGE_BLOCK_ZERO   ((VDIIMAGEBLOCKPOINTER)~1)

/**
 * Block 'pointer' >= VDI_IMAGE_BLOCK_UNALLOCATED indicates block is not
 * allocated in image file.
 */
#define VDI_IMAGE_BLOCK_UNALLOCATED   (VDI_IMAGE_BLOCK_ZERO)
#define IS_VDI_IMAGE_BLOCK_ALLOCATED(bp)   (bp < VDI_IMAGE_BLOCK_UNALLOCATED)

#define GET_MAJOR_HEADER_VERSION(ph) (VDI_GET_VERSION_MAJOR((ph)->uVersion))
#define GET_MINOR_HEADER_VERSION(ph) (VDI_GET_VERSION_MINOR((ph)->uVersion))

#ifdef VBOX_VDICORE_VD
/** @name VDI image types
 * @{ */
typedef enum VDIIMAGETYPE
{
    /** Normal dynamically growing base image file. */
    VDI_IMAGE_TYPE_NORMAL = 1,
    /** Preallocated base image file of a fixed size. */
    VDI_IMAGE_TYPE_FIXED,
    /** Dynamically growing image file for undo/commit changes support. */
    VDI_IMAGE_TYPE_UNDO,
    /** Dynamically growing image file for differencing support. */
    VDI_IMAGE_TYPE_DIFF,

    /** First valid image type value. */
    VDI_IMAGE_TYPE_FIRST  = VDI_IMAGE_TYPE_NORMAL,
    /** Last valid image type value. */
    VDI_IMAGE_TYPE_LAST   = VDI_IMAGE_TYPE_DIFF
} VDIIMAGETYPE;
/** Pointer to VDI image type. */
typedef VDIIMAGETYPE *PVDIIMAGETYPE;
/** @} */
#endif /* VBOX_VDICORE_VD */

/*******************************************************************************
*   Internal Functions for header access                                       *
*******************************************************************************/
DECLINLINE(VDIIMAGETYPE) getImageType(PVDIHEADER ph)
{
    switch (GET_MAJOR_HEADER_VERSION(ph))
    {
        case 0: return (VDIIMAGETYPE)ph->u.v0.u32Type;
        case 1: return (VDIIMAGETYPE)ph->u.v1.u32Type;
    }
    AssertFailed();
    return (VDIIMAGETYPE)0;
}

DECLINLINE(unsigned) getImageFlags(PVDIHEADER ph)
{
    switch (GET_MAJOR_HEADER_VERSION(ph))
    {
        case 0: return ph->u.v0.fFlags;
        case 1: return ph->u.v1.fFlags;
    }
    AssertFailed();
    return 0;
}

DECLINLINE(char *) getImageComment(PVDIHEADER ph)
{
    switch (GET_MAJOR_HEADER_VERSION(ph))
    {
        case 0: return &ph->u.v0.szComment[0];
        case 1: return &ph->u.v1.szComment[0];
    }
    AssertFailed();
    return NULL;
}

DECLINLINE(unsigned) getImageBlocksOffset(PVDIHEADER ph)
{
    switch (GET_MAJOR_HEADER_VERSION(ph))
    {
        case 0: return (sizeof(VDIPREHEADER) + sizeof(VDIHEADER0));
        case 1: return ph->u.v1.offBlocks;
    }
    AssertFailed();
    return 0;
}

DECLINLINE(unsigned) getImageDataOffset(PVDIHEADER ph)
{
    switch (GET_MAJOR_HEADER_VERSION(ph))
    {
        case 0: return sizeof(VDIPREHEADER) + sizeof(VDIHEADER0) + \
                       (ph->u.v0.cBlocks * sizeof(VDIIMAGEBLOCKPOINTER));
        case 1: return ph->u.v1.offData;
    }
    AssertFailed();
    return 0;
}

DECLINLINE(PVDIDISKGEOMETRY) getImageLCHSGeometry(PVDIHEADER ph)
{
    switch (GET_MAJOR_HEADER_VERSION(ph))
    {
        case 0: return NULL;
        case 1:
            switch (GET_MINOR_HEADER_VERSION(ph))
            {
                case 1:
                    if (ph->u.v1.cbHeader < sizeof(ph->u.v1plus))
                        return NULL;
                    else
                        return &ph->u.v1plus.LCHSGeometry;
            }
    }
    AssertFailed();
    return NULL;
}

DECLINLINE(uint64_t) getImageDiskSize(PVDIHEADER ph)
{
    switch (GET_MAJOR_HEADER_VERSION(ph))
    {
        case 0: return ph->u.v0.cbDisk;
        case 1: return ph->u.v1.cbDisk;
    }
    AssertFailed();
    return 0;
}

DECLINLINE(unsigned) getImageBlockSize(PVDIHEADER ph)
{
    switch (GET_MAJOR_HEADER_VERSION(ph))
    {
        case 0: return ph->u.v0.cbBlock;
        case 1: return ph->u.v1.cbBlock;
    }
    AssertFailed();
    return 0;
}

DECLINLINE(unsigned) getImageExtraBlockSize(PVDIHEADER ph)
{
    switch (GET_MAJOR_HEADER_VERSION(ph))
    {
        case 0: return 0;
        case 1: return ph->u.v1.cbBlockExtra;
    }
    AssertFailed();
    return 0;
}

DECLINLINE(unsigned) getImageBlocks(PVDIHEADER ph)
{
    switch (GET_MAJOR_HEADER_VERSION(ph))
    {
        case 0: return ph->u.v0.cBlocks;
        case 1: return ph->u.v1.cBlocks;
    }
    AssertFailed();
    return 0;
}

DECLINLINE(unsigned) getImageBlocksAllocated(PVDIHEADER ph)
{
    switch (GET_MAJOR_HEADER_VERSION(ph))
    {
        case 0: return ph->u.v0.cBlocksAllocated;
        case 1: return ph->u.v1.cBlocksAllocated;
    }
    AssertFailed();
    return 0;
}

DECLINLINE(void) setImageBlocksAllocated(PVDIHEADER ph, unsigned cBlocks)
{
    switch (GET_MAJOR_HEADER_VERSION(ph))
    {
        case 0: ph->u.v0.cBlocksAllocated = cBlocks; return;
        case 1: ph->u.v1.cBlocksAllocated = cBlocks; return;
    }
    AssertFailed();
}

DECLINLINE(PRTUUID) getImageCreationUUID(PVDIHEADER ph)
{
    switch (GET_MAJOR_HEADER_VERSION(ph))
    {
        case 0: return &ph->u.v0.uuidCreate;
        case 1: return &ph->u.v1.uuidCreate;
    }
    AssertFailed();
    return NULL;
}

DECLINLINE(PRTUUID) getImageModificationUUID(PVDIHEADER ph)
{
    switch (GET_MAJOR_HEADER_VERSION(ph))
    {
        case 0: return &ph->u.v0.uuidModify;
        case 1: return &ph->u.v1.uuidModify;
    }
    AssertFailed();
    return NULL;
}

DECLINLINE(PRTUUID) getImageParentUUID(PVDIHEADER ph)
{
    switch (GET_MAJOR_HEADER_VERSION(ph))
    {
        case 0: return &ph->u.v0.uuidLinkage;
        case 1: return &ph->u.v1.uuidLinkage;
    }
    AssertFailed();
    return NULL;
}

DECLINLINE(PRTUUID) getImageParentModificationUUID(PVDIHEADER ph)
{
    switch (GET_MAJOR_HEADER_VERSION(ph))
    {
        case 1: return &ph->u.v1.uuidParentModify;
    }
    AssertFailed();
    return NULL;
}

#ifndef VBOX_VDICORE_VD
/**
 * Default image block size, may be changed by setBlockSize/getBlockSize.
 *
 * Note: for speed reasons block size should be a power of 2 !
 */
#define VDI_IMAGE_DEFAULT_BLOCK_SIZE            _1M
#endif /* !VBOX_VDICORE_VD */

#ifndef VBOX_VDICORE_VD
/**
 * fModified bit flags.
 */
#define VDI_IMAGE_MODIFIED_FLAG                 RT_BIT(0)
#define VDI_IMAGE_MODIFIED_FIRST                RT_BIT(1)
#define VDI_IMAGE_MODIFIED_DISABLE_UUID_UPDATE  RT_BIT(2)
#endif /* !VBOX_VDICORE_VD */

/**
 * Image structure
 */
typedef struct VDIIMAGEDESC
{
#ifndef VBOX_VDICORE_VD
    /** Link to parent image descriptor, if any. */
    struct VDIIMAGEDESC    *pPrev;
    /** Link to child image descriptor, if any. */
    struct VDIIMAGEDESC    *pNext;
#endif /* !VBOX_VDICORE_VD */
    /** File handle. */
    RTFILE                  File;
#ifndef VBOX_VDICORE_VD
    /** True if the image is operating in readonly mode. */
    bool                    fReadOnly;
    /** Image open flags, VDI_OPEN_FLAGS_*. */
    unsigned                fOpen;
#else /* VBOX_VDICORE_VD */
    /** Image open flags, VD__OPEN_FLAGS_*. */
    unsigned                uOpenFlags;
#endif /* VBOX_VDICORE_VD */
    /** Image pre-header. */
    VDIPREHEADER            PreHeader;
    /** Image header. */
    VDIHEADER               Header;
    /** Pointer to a block array. */
    PVDIIMAGEBLOCKPOINTER   paBlocks;
#ifndef VBOX_VDICORE_VD
    /** fFlags copy from image header, for speed optimization. */
    unsigned                fFlags;
#else /* VBOX_VDICORE_VD */
    /** fFlags copy from image header, for speed optimization. */
    unsigned                uImageFlags;
#endif /* VBOX_VDICORE_VD */
    /** Start offset of block array in image file, here for speed optimization. */
    unsigned                offStartBlocks;
    /** Start offset of data in image file, here for speed optimization. */
    unsigned                offStartData;
    /** Block mask for getting the offset into a block from a byte hdd offset. */
    unsigned                uBlockMask;
    /** Block shift value for converting byte hdd offset into paBlock index. */
    unsigned                uShiftOffset2Index;
#ifndef VBOX_VDICORE_VD
    /** Block shift value for converting block index into offset in image. */
    unsigned                uShiftIndex2Offset;
#endif /* !VBOX_VDICORE_VD */
    /** Offset of data from the beginning of block. */
    unsigned                offStartBlockData;
#ifndef VBOX_VDICORE_VD
    /** Image is modified flags (VDI_IMAGE_MODIFIED*). */
    unsigned                fModified;
    /** Container filename. (UTF-8)
     * @todo Make this variable length to save a bunch of bytes. (low prio) */
    char                    szFilename[RTPATH_MAX];
#else /* VBOX_VDICORE_VD */
    /** Total size of image block (including the extra data). */
    unsigned                cbTotalBlockData;
    /** Container filename. (UTF-8) */
    const char             *pszFilename;
    /** Physical geometry of this image (never actually stored). */
    PDMMEDIAGEOMETRY        PCHSGeometry;
    /** Error callback. */
    PFNVDERROR              pfnError;
    /** Opaque data for error callback. */
    void                   *pvErrorUser;
#endif /* VBOX_VDICORE_VD */
} VDIIMAGEDESC, *PVDIIMAGEDESC;

#ifndef VBOX_VDICORE_VD
/**
 * Default work buffer size, may be changed by setBufferSize() method.
 *
 * For best speed performance it must be equal to image block size.
 */
#define VDIDISK_DEFAULT_BUFFER_SIZE   (VDI_IMAGE_DEFAULT_BLOCK_SIZE)
#endif /* !VBOX_VDICORE_VD */

/** VDIDISK Signature. */
#define VDIDISK_SIGNATURE (0xbedafeda)

/**
 * VBox HDD Container main structure, private part.
 */
struct VDIDISK
{
    /** Structure signature (VDIDISK_SIGNATURE). */
    uint32_t        u32Signature;

    /** Number of opened images. */
    unsigned        cImages;

    /** Base image. */
    PVDIIMAGEDESC   pBase;

    /** Last opened image in the chain.
     * The same as pBase if only one image is used or the last opened diff image. */
    PVDIIMAGEDESC   pLast;

    /** Default block size for newly created images. */
    unsigned        cbBlock;

    /** Working buffer size, allocated only while committing data,
     * copying block from primary image to secondary and saving previously
     * zero block. Buffer deallocated after operation complete.
     * @remark  For best performance buffer size must be equal to image's
     *          block size, however it may be decreased for memory saving.
     */
    unsigned        cbBuf;

    /** Flag whether zero writes should be handled normally or optimized
     * away if possible. */
    bool            fHonorZeroWrites;

    /** The media interface. */
    PDMIMEDIA       IMedia;
    /** Pointer to the driver instance. */
    PPDMDRVINS      pDrvIns;
};


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
__BEGIN_DECLS

#ifndef VBOX_VDICORE_VD
VBOXDDU_DECL(void) vdiInitVDIDisk(PVDIDISK pDisk);
VBOXDDU_DECL(void) VDIFlushImage(PVDIIMAGEDESC pImage);
VBOXDDU_DECL(int)  vdiChangeImageMode(PVDIIMAGEDESC pImage, bool fReadOnly);
#endif /* !VBOX_VDICORE_VD */

__END_DECLS

#endif
