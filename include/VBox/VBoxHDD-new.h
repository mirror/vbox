/** @file
 * VBox HDD Container API.
 * Will replace VBoxHDD.h.
 */

/*
 * Copyright (C) 2006-2008 innotek GmbH
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

#ifndef ___VBox_VD_h
#define ___VBox_VD_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/pdm.h>

__BEGIN_DECLS

#ifdef IN_RING0
# error "There are no VBox HDD Container APIs available in Ring-0 Host Context!"
#endif

/** @defgroup grp_vd            VBox HDD Container
 * @{
 */

/** Current VMDK image version. */
#define VMDK_IMAGE_VERSION          (0x0001)

/** Current VDI image major version. */
#define VDI_IMAGE_VERSION_MAJOR     (0x0001)
/** Current VDI image minor version. */
#define VDI_IMAGE_VERSION_MINOR     (0x0001)
/** Current VDI image version. */
#define VDI_IMAGE_VERSION           ((VDI_IMAGE_VERSION_MAJOR << 16) | VDI_IMAGE_VERSION_MINOR)

/** Get VDI major version from combined version. */
#define VDI_GET_VERSION_MAJOR(uVer)    ((uVer) >> 16)
/** Get VDI minor version from combined version. */
#define VDI_GET_VERSION_MINOR(uVer)    ((uVer) & 0xffff)

/** Placeholder for specifying the last opened image. */
#define VD_LAST_IMAGE               0xffffffffU

/** @name VBox HDD container image types
 * @{ */
typedef enum VDIMAGETYPE
{
    /** Normal dynamically growing base image file. */
    VD_IMAGE_TYPE_NORMAL    = 1,
    /** Preallocated base image file of a fixed size. */
    VD_IMAGE_TYPE_FIXED,
    /** Dynamically growing image file for undo/commit changes support. */
    VD_IMAGE_TYPE_UNDO,
    /** Dynamically growing image file for differencing support. */
    VD_IMAGE_TYPE_DIFF,

    /** First valid image type value. */
    VD_IMAGE_TYPE_FIRST     = VD_IMAGE_TYPE_NORMAL,
    /** Last valid image type value. */
    VD_IMAGE_TYPE_LAST      = VD_IMAGE_TYPE_DIFF
} VDIMAGETYPE;
/** Pointer to VBox HDD container image type. */
typedef VDIMAGETYPE *PVDIMAGETYPE;
/** @} */

/** @name VBox HDD container image flags
 * @{
 */
/** No flags. */
#define VD_IMAGE_FLAGS_NONE                 (0)
/** VMDK: Split image into 2GB extents. */
#define VD_VMDK_IMAGE_FLAGS_SPLIT_2G        (0x0001)
/** VMDK: Raw disk image (giving access to a number of host partitions). */
#define VD_VMDK_IMAGE_FLAGS_RAWDISK         (0x0002)
/** VDI: Fill new blocks with zeroes while expanding image file. Only valid
 * for newly created images, never set for opened existing images. */
#define VD_VDI_IMAGE_FLAGS_ZERO_EXPAND      (0x0100)

/** Mask of valid image flags for VMDK. */
#define VD_VMDK_IMAGE_FLAGS_MASK            (VD_IMAGE_FLAGS_NONE | VD_VMDK_IMAGE_FLAGS_SPLIT_2G | VD_VMDK_IMAGE_FLAGS_RAWDISK)

/** Mask of valid image flags for VDI. */
#define VD_VDI_IMAGE_FLAGS_MASK             (VD_IMAGE_FLAGS_NONE | VD_VDI_IMAGE_FLAGS_ZERO_EXPAND)

/** Mask of all valid image flags for all formats. */
#define VD_IMAGE_FLAGS_MASK                 (VD_VMDK_IMAGE_FLAGS_MASK | VD_VDI_IMAGE_FLAGS_MASK)

/** Default image flags. */
#define VD_IMAGE_FLAGS_DEFAULT              (VD_IMAGE_FLAGS_NONE)
/** @} */


/**
 * Auxiliary type for describing partitions on raw disks.
 */
typedef struct VBOXHDDRAWPART
{
    /** Device to use for this partition. Can be the disk device if the offset
     * field is set appropriately. If this is NULL, then this partition will
     * not be accessible to the guest. The size of the partition must still
     * be set correctly. */
    const char      *pszRawDevice;
    /** Offset where the partition data starts in this device. */
    uint64_t        uPartitionStartOffset;
    /** Offset where the partition data starts in the disk. */
    uint64_t        uPartitionStart;
    /** Size of the partition. */
    uint64_t        cbPartition;
    /** Size of the partitioning info to prepend. */
    uint64_t        cbPartitionData;
    /** Offset where the partitioning info starts in the disk. */
    uint64_t        uPartitionDataStart;
    /** Pointer to the partitioning info to prepend. */
    const void      *pvPartitionData;
} VBOXHDDRAWPART, *PVBOXHDDRAWPART;

/**
 * Auxiliary data structure for creating raw disks.
 */
typedef struct VBOXHDDRAW
{
    /** Signature for structure. Must be 'R', 'A', 'W', '\0'. Actually a trick
     * to make logging of the comment string produce sensible results. */
    char            szSignature[4];
    /** Flag whether access to full disk should be given (ignoring the
     * partition information below). */
    bool            fRawDisk;
    /** Filename for the raw disk. Ignored for partitioned raw disks.
     * For Linux e.g. /dev/sda, and for Windows e.g. \\.\PhysicalDisk0. */
    const char      *pszRawDisk;
    /** Number of entries in the partitions array. */
    unsigned        cPartitions;
    /** Pointer to the partitions array. */
    PVBOXHDDRAWPART pPartitions;
} VBOXHDDRAW, *PVBOXHDDRAW;

/** @name VBox HDD container image open mode flags
 * @{
 */
/** Try to open image in read/write exclusive access mode if possible, or in read-only elsewhere. */
#define VD_OPEN_FLAGS_NORMAL        0
/** Open image in read-only mode with sharing access with others. */
#define VD_OPEN_FLAGS_READONLY      RT_BIT(0)
/** Honor zero block writes instead of ignoring them whenever possible.
 * This is not supported by all formats. It is silently ignored in this case. */
#define VD_OPEN_FLAGS_HONOR_ZEROES  RT_BIT(1)
/** Honor writes of the same data instead of ignoring whenever possible.
 * This is handled generically, and is only meaningful for differential image
 * formats. It is silently ignored otherwise. */
#define VD_OPEN_FLAGS_HONOR_SAME    RT_BIT(2)
/** Do not perform the base/diff image check on open. This internally implies
 * opening the image as readonly. Images opened with this flag should only be
 * used for querying information, and nothing else. */
#define VD_OPEN_FLAGS_INFO          RT_BIT(3)
/** Mask of valid flags. */
#define VD_OPEN_FLAGS_MASK          (VD_OPEN_FLAGS_NORMAL | VD_OPEN_FLAGS_READONLY | VD_OPEN_FLAGS_HONOR_ZEROES | VD_OPEN_FLAGS_HONOR_SAME | VD_OPEN_FLAGS_INFO)
/** @}*/


/** @name VBox HDD container backend capability flags
 * @{
 */
/** Supports UUIDs as expected by VirtualBox code. */
#define VD_CAP_UUID                 RT_BIT(0)
/** Supports creating fixed size images, allocating all space instantly. */
#define VD_CAP_CREATE_FIXED         RT_BIT(1)
/** Supports creating dynamically growing images, allocating space on demand. */
#define VD_CAP_CREATE_DYNAMIC       RT_BIT(2)
/** Supports creating images split in chunks of a bit less than 2GBytes. */
#define VD_CAP_CREATE_SPLIT_2G      RT_BIT(3)
/** Supports being used as differencing image format backend. */
#define VD_CAP_DIFF                 RT_BIT(4)
/** @}*/


/**
 * Data structure for returning a list of backend capabilities.
 */
typedef struct VDBACKENDINFO
{
    /** Name of the backend. */
    char *pszBackend;
    /** Capabilities of the backend (a combination of the VD_CAP_* flags). */
    uint64_t uBackendCaps;
} VDBACKENDINFO, *PVDBACKENDINFO;


/**
 * Error message callback.
 *
 * @param   pvUser          The opaque data passed on container creation.
 * @param   rc              The VBox error code.
 * @param   RT_SRC_POS_DECL Use RT_SRC_POS.
 * @param   pszFormat       Error message format string.
 * @param   va              Error message arguments.
 */
typedef DECLCALLBACK(void) FNVDERROR(void *pvUser, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list va);
/** Pointer to a FNVDERROR(). */
typedef FNVDERROR *PFNVDERROR;


/**
 * VBox HDD Container main structure.
 */
/* Forward declaration, VBOXHDD structure is visible only inside VBox HDD module. */
struct VBOXHDD;
typedef struct VBOXHDD VBOXHDD;
typedef VBOXHDD *PVBOXHDD;


/**
 * Lists all HDD backends and their capabilities in a caller-provided buffer.
 * Free all returned names with RTStrFree() when you no longer need them.
 *
 * @returns VBox status code.
 *          VERR_BUFFER_OVERFLOW if not enough space is passed.
 * @param   cEntriesAlloc   Number of list entries available.
 * @param   pEntries        Pointer to array for the entries.
 * @param   pcEntriesUsed   Number of entries returned.
 */
VBOXDDU_DECL(int) VDBackendInfo(unsigned cEntriesAlloc, PVDBACKENDINFO pEntries,
                                unsigned *pcEntriesUsed);


/**
 * Allocates and initializes an empty HDD container.
 * No image files are opened.
 *
 * @returns VBox status code.
 * @param   pfnError        Callback for setting extended error information.
 * @param   pvErrorUser     Opaque parameter for pfnError.
 * @param   ppDisk          Where to store the reference to HDD container.
 */
VBOXDDU_DECL(int) VDCreate(PFNVDERROR pfnError, void *pvErrorUser,
                           PVBOXHDD *ppDisk);

/**
 * Destroys HDD container.
 * If container has opened image files they will be closed.
 *
 * @param   pDisk           Pointer to HDD container.
 */
VBOXDDU_DECL(void) VDDestroy(PVBOXHDD pDisk);

/**
 * Try to get the backend name which can use this image.
 *
 * @returns VBox status code.
 * @param   pszFilename     Name of the image file for which the backend is queried.
 * @param   ppszFormat      Receives pointer of the UTF-8 string which contains the format name.
 *                          The returned pointer must be freed using RTStrFree().
 */
VBOXDDU_DECL(int) VDGetFormat(const char *pszFilename, char **ppszFormat);

/**
 * Opens an image file.
 *
 * The first opened image file in HDD container must have a base image type,
 * others (next opened images) must be differencing or undo images.
 * Linkage is checked for differencing image to be consistent with the previously opened image.
 * When another differencing image is opened and the last image was opened in read/write access
 * mode, then the last image is reopened in read-only with deny write sharing mode. This allows
 * other processes to use images in read-only mode too.
 *
 * Note that the image is opened in read-only mode if a read/write open is not possible.
 * Use VDIsReadOnly to check open mode.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to HDD container.
 * @param   pszBackend      Name of the image file backend to use.
 * @param   pszFilename     Name of the image file to open.
 * @param   uOpenFlags      Image file open mode, see VD_OPEN_FLAGS_* constants.
 */
VBOXDDU_DECL(int) VDOpen(PVBOXHDD pDisk, const char *pszBackend,
                         const char *pszFilename, unsigned uOpenFlags);

/**
 * Creates and opens a new base image file.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to HDD container.
 * @param   pszBackend      Name of the image file backend to use.
 * @param   pszFilename     Name of the image file to create.
 * @param   enmType         Image type, only base image types are acceptable.
 * @param   cbSize          Image size in bytes.
 * @param   uImageFlags     Flags specifying special image features.
 * @param   pszComment      Pointer to image comment. NULL is ok.
 * @param   pPCHSGeometry   Pointer to physical disk geometry <= (16383,16,63). Not NULL.
 * @param   pLCHSGeometry   Pointer to logical disk geometry <= (1024,255,63). Not NULL.
 * @param   uOpenFlags      Image file open mode, see VD_OPEN_FLAGS_* constants.
 * @param   pfnProgress     Progress callback. Optional. NULL if not to be used.
 * @param   pvUser          User argument for the progress callback.
 */
VBOXDDU_DECL(int) VDCreateBase(PVBOXHDD pDisk, const char *pszBackend,
                               const char *pszFilename, VDIMAGETYPE enmType,
                               uint64_t cbSize, unsigned uImageFlags,
                               const char *pszComment,
                               PCPDMMEDIAGEOMETRY pPCHSGeometry,
                               PCPDMMEDIAGEOMETRY pLCHSGeometry,
                               unsigned uOpenFlags, PFNVMPROGRESS pfnProgress,
                               void *pvUser);

/**
 * Creates and opens a new differencing image file in HDD container.
 * See comments for VDOpen function about differencing images.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to HDD container.
 * @param   pszBackend      Name of the image file backend to use.
 * @param   pszFilename     Name of the differencing image file to create.
 * @param   uImageFlags     Flags specifying special image features.
 * @param   pszComment      Pointer to image comment. NULL is ok.
 * @param   uOpenFlags      Image file open mode, see VD_OPEN_FLAGS_* constants.
 * @param   pfnProgress     Progress callback. Optional. NULL if not to be used.
 * @param   pvUser          User argument for the progress callback.
 */
VBOXDDU_DECL(int) VDCreateDiff(PVBOXHDD pDisk, const char *pszBackend,
                               const char *pszFilename, unsigned uImageFlags,
                               const char *pszComment, unsigned uOpenFlags,
                               PFNVMPROGRESS pfnProgress, void *pvUser);

/**
 * Merges two images (not necessarily with direct parent/child relationship).
 * As a side effect the source image and potentially the other images which
 * are also merged to the destination are deleted from both the disk and the
 * images in the HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImageFrom      Name of the image file to merge from.
 * @param   nImageTo        Name of the image file to merge to.
 * @param   pfnProgress     Progress callback. Optional. NULL if not to be used.
 * @param   pvUser          User argument for the progress callback.
 */
VBOXDDU_DECL(int) VDMerge(PVBOXHDD pDisk, unsigned nImageFrom,
                          unsigned nImageTo, PFNVMPROGRESS pfnProgress,
                          void *pvUser);

/**
 * Copies an image from one HDD container to another.
 * The copy is opened in the target HDD container.
 * It is possible to convert between different image formats, because the
 * backend for the destination may be different from the source.
 * If both the source and destination reference the same HDD container,
 * then the image is moved (by copying/deleting or renaming) to the new location.
 * The source container is unchanged if the move operation fails, otherwise
 * the image at the new location is opened in the same way as the old one was.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDiskFrom       Pointer to source HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pDiskTo         Pointer to destination HDD container.
 * @param   pszBackend      Name of the image file backend to use (may be NULL to use the same as the source).
 * @param   pszFilename     New name of the image (may be NULL if pDiskFrom == pDiskTo).
 * @param   fMoveByRename   If true, attempt to perform a move by renaming (if successful the new size is ignored).
 * @param   cbSize          New image size (0 means leave unchanged).
 * @param   pfnProgress     Progress callback. Optional. NULL if not to be used.
 * @param   pvUser          User argument for the progress callback.
 */
VBOXDDU_DECL(int) VDCopy(PVBOXHDD pDiskFrom, unsigned nImage, PVBOXHDD pDiskTo,
                         const char *pszBackend, const char *pszFilename,
                         bool fMoveByRename, uint64_t cbSize,
                         PFNVMPROGRESS pfnProgress, void *pvUser);

/**
 * Closes the last opened image file in HDD container.
 * If previous image file was opened in read-only mode (that is normal) and closing image
 * was opened in read-write mode (the whole disk was in read-write mode) - the previous image
 * will be reopened in read/write mode.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_NOT_OPENED if no image is opened in HDD container.
 * @param   pDisk           Pointer to HDD container.
 * @param   fDelete         If true, delete the image from the host disk.
 */
VBOXDDU_DECL(int) VDClose(PVBOXHDD pDisk, bool fDelete);

/**
 * Closes all opened image files in HDD container.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to HDD container.
 */
VBOXDDU_DECL(int) VDCloseAll(PVBOXHDD pDisk);

/**
 * Read data from virtual HDD.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_NOT_OPENED if no image is opened in HDD container.
 * @param   pDisk           Pointer to HDD container.
 * @param   uOffset         Offset of first reading byte from start of disk.
 * @param   pvBuf           Pointer to buffer for reading data.
 * @param   cbRead          Number of bytes to read.
 */
VBOXDDU_DECL(int) VDRead(PVBOXHDD pDisk, uint64_t uOffset, void *pvBuf, size_t cbRead);

/**
 * Write data to virtual HDD.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_NOT_OPENED if no image is opened in HDD container.
 * @param   pDisk           Pointer to HDD container.
 * @param   uOffset         Offset of first writing byte from start of disk.
 * @param   pvBuf           Pointer to buffer for writing data.
 * @param   cbWrite         Number of bytes to write.
 */
VBOXDDU_DECL(int) VDWrite(PVBOXHDD pDisk, uint64_t uOffset, const void *pvBuf, size_t cbWrite);

/**
 * Make sure the on disk representation of a virtual HDD is up to date.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_NOT_OPENED if no image is opened in HDD container.
 * @param   pDisk           Pointer to HDD container.
 */
VBOXDDU_DECL(int) VDFlush(PVBOXHDD pDisk);

/**
 * Get number of opened images in HDD container.
 *
 * @returns Number of opened images for HDD container. 0 if no images have been opened.
 * @param   pDisk           Pointer to HDD container.
 */
VBOXDDU_DECL(unsigned) VDGetCount(PVBOXHDD pDisk);

/**
 * Get read/write mode of HDD container.
 *
 * @returns Virtual disk ReadOnly status.
 * @returns true if no image is opened in HDD container.
 * @param   pDisk           Pointer to HDD container.
 */
VBOXDDU_DECL(bool) VDIsReadOnly(PVBOXHDD pDisk);

/**
 * Get total capacity of an image in HDD container.
 *
 * @returns Virtual disk size in bytes.
 * @returns 0 if image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 */
VBOXDDU_DECL(uint64_t) VDGetSize(PVBOXHDD pDisk, unsigned nImage);

/**
 * Get total file size of an image in HDD container.
 *
 * @returns Virtual disk size in bytes.
 * @returns 0 if image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 */
VBOXDDU_DECL(uint64_t) VDGetFileSize(PVBOXHDD pDisk, unsigned nImage);

/**
 * Get virtual disk PCHS geometry of an image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @returns VERR_VDI_GEOMETRY_NOT_SET if no geometry present in the HDD container.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pPCHSGeometry   Where to store PCHS geometry. Not NULL.
 */
VBOXDDU_DECL(int) VDGetPCHSGeometry(PVBOXHDD pDisk, unsigned nImage,
                                    PPDMMEDIAGEOMETRY pPCHSGeometry);

/**
 * Store virtual disk PCHS geometry of an image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pPCHSGeometry   Where to load PCHS geometry from. Not NULL.
 */
VBOXDDU_DECL(int) VDSetPCHSGeometry(PVBOXHDD pDisk, unsigned nImage,
                                    PCPDMMEDIAGEOMETRY pPCHSGeometry);

/**
 * Get virtual disk LCHS geometry of an image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @returns VERR_VDI_GEOMETRY_NOT_SET if no geometry present in the HDD container.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pLCHSGeometry   Where to store LCHS geometry. Not NULL.
 */
VBOXDDU_DECL(int) VDGetLCHSGeometry(PVBOXHDD pDisk, unsigned nImage,
                                    PPDMMEDIAGEOMETRY pLCHSGeometry);

/**
 * Store virtual disk LCHS geometry of an image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pLCHSGeometry   Where to load LCHS geometry from. Not NULL.
 */
VBOXDDU_DECL(int) VDSetLCHSGeometry(PVBOXHDD pDisk, unsigned nImage,
                                    PCPDMMEDIAGEOMETRY pLCHSGeometry);

/**
 * Get version of image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   puVersion       Where to store the image version.
 */
VBOXDDU_DECL(int) VDGetVersion(PVBOXHDD pDisk, unsigned nImage,
                               unsigned *puVersion);

/**
 * Get type of image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   penmType        Where to store the image type.
 */
VBOXDDU_DECL(int) VDGetImageType(PVBOXHDD pDisk, unsigned nImage,
                                 PVDIMAGETYPE penmType);

/**
 * Get flags of image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   puImageFlags    Where to store the image flags.
 */
VBOXDDU_DECL(int) VDGetImageFlags(PVBOXHDD pDisk, unsigned nImage, unsigned *puImageFlags);

/**
 * Get open flags of image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   puOpenFlags     Where to store the image open flags.
 */
VBOXDDU_DECL(int) VDGetOpenFlags(PVBOXHDD pDisk, unsigned nImage,
                                 unsigned *puOpenFlags);

/**
 * Set open flags of image in HDD container.
 * This operation may cause file locking changes and/or files being reopened.
 * Note that in case of unrecoverable error all images in HDD container will be closed.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   uOpenFlags      Image file open mode, see VD_OPEN_FLAGS_* constants.
 */
VBOXDDU_DECL(int) VDSetOpenFlags(PVBOXHDD pDisk, unsigned nImage,
                                 unsigned uOpenFlags);

/**
 * Get base filename of image in HDD container. Some image formats use
 * other filenames as well, so don't use this for anything but informational
 * purposes.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @returns VERR_BUFFER_OVERFLOW if pszFilename buffer too small to hold filename.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pszFilename     Where to store the image file name.
 * @param   cbFilename      Size of buffer pszFilename points to.
 */
VBOXDDU_DECL(int) VDGetFilename(PVBOXHDD pDisk, unsigned nImage,
                                char *pszFilename, unsigned cbFilename);

/**
 * Get the comment line of image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @returns VERR_BUFFER_OVERFLOW if pszComment buffer too small to hold comment text.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pszComment      Where to store the comment string of image. NULL is ok.
 * @param   cbComment       The size of pszComment buffer. 0 is ok.
 */
VBOXDDU_DECL(int) VDGetComment(PVBOXHDD pDisk, unsigned nImage,
                               char *pszComment, unsigned cbComment);

/**
 * Changes the comment line of image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pszComment      New comment string (UTF-8). NULL is allowed to reset the comment.
 */
VBOXDDU_DECL(int) VDSetComment(PVBOXHDD pDisk, unsigned nImage,
                               const char *pszComment);

/**
 * Get UUID of image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pUuid           Where to store the image UUID.
 */
VBOXDDU_DECL(int) VDGetUuid(PVBOXHDD pDisk, unsigned nImage, PRTUUID pUuid);

/**
 * Set the image's UUID. Should not be used by normal applications.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pUuid           New UUID of the image. If NULL, a new UUID is created.
 */
VBOXDDU_DECL(int) VDSetUuid(PVBOXHDD pDisk, unsigned nImage, PCRTUUID pUuid);

/**
 * Get last modification UUID of image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pUuid           Where to store the image modification UUID.
 */
VBOXDDU_DECL(int) VDGetModificationUuid(PVBOXHDD pDisk, unsigned nImage,
                                        PRTUUID pUuid);

/**
 * Set the image's last modification UUID. Should not be used by normal applications.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pUuid           New modification UUID of the image. If NULL, a new UUID is created.
 */
VBOXDDU_DECL(int) VDSetModificationUuid(PVBOXHDD pDisk, unsigned nImage,
                                        PCRTUUID pUuid);

/**
 * Get parent UUID of image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of the container.
 * @param   pUuid           Where to store the parent image UUID.
 */
VBOXDDU_DECL(int) VDGetParentUuid(PVBOXHDD pDisk, unsigned nImage,
                                  PRTUUID pUuid);

/**
 * Set the image's parent UUID. Should not be used by normal applications.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pUuid           New parent UUID of the image. If NULL, a new UUID is created.
 */
VBOXDDU_DECL(int) VDSetParentUuid(PVBOXHDD pDisk, unsigned nImage,
                                  PCRTUUID pUuid);


/**
 * Debug helper - dumps all opened images in HDD container into the log file.
 *
 * @param   pDisk           Pointer to HDD container.
 */
VBOXDDU_DECL(void) VDDumpImages(PVBOXHDD pDisk);

__END_DECLS

/** @} */

#endif
