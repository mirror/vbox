/** @file
 * VBox HDD Container API.
 * Will replace VBoxHDD.h.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
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
 * Allocates and initializes an empty VBox HDD container.
 * No image files are opened.
 *
 * @returns VBox status code.
 * @param   pszBackend      Name of the image file backend to use.
 * @param   pfnError        Callback for setting extended error information.
 * @param   pvErrorUser     Opaque parameter for pfnError.
 * @param   ppDisk          Where to store the reference to the VBox HDD container.
 */
VBOXDDU_DECL(int) VDCreate(const char *pszBackend, PFNVDERROR pfnError, void *pvErrorUser, PVBOXHDD *ppDisk);

/**
 * Destroys the VBox HDD container.
 * If container has opened image files they will be closed.
 *
 * @param   pDisk           Pointer to VBox HDD container.
 */
VBOXDDU_DECL(void) VDDestroy(PVBOXHDD pDisk);

/**
 * Try to get the backend name which can use this image. 
 *
 * @returns VBox status code.
 * @param   pszFilename     Name of the image file for which the backend is queried.
 * @param   ppszFormat      Where to store the name of the plugin.
 */
VBOXDDU_DECL(int) VDGetFormat(const char *pszFilename, char **ppszFormat);

/**
 * Opens an image file.
 *
 * The first opened image file in a HDD container must have a base image type,
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
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   pszFilename     Name of the image file to open.
 * @param   uOpenFlags      Image file open mode, see VD_OPEN_FLAGS_* constants.
 */
VBOXDDU_DECL(int) VDOpen(PVBOXHDD pDisk, const char *pszFilename, unsigned uOpenFlags);

/**
 * Creates and opens a new base image file.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   pszFilename     Name of the image file to create.
 * @param   enmType         Image type, only base image types are acceptable.
 * @param   cbSize          Image size in bytes.
 * @param   uImageFlags     Flags specifying special image features.
 * @param   pszComment      Pointer to image comment. NULL is ok.
 * @param   cCylinders      Number of cylinders (must be <= 16383).
 * @param   cHeads          Number of heads (must be <= 16).
 * @param   cSectors        Number of sectors (must be <= 63);
 * @param   uOpenFlags      Image file open mode, see VD_OPEN_FLAGS_* constants.
 * @param   pfnProgress     Progress callback. Optional. NULL if not to be used.
 * @param   pvUser          User argument for the progress callback.
 */
VBOXDDU_DECL(int) VDCreateBase(PVBOXHDD pDisk, const char *pszFilename,
                               VDIMAGETYPE enmType, uint64_t cbSize,
                               unsigned uImageFlags, const char *pszComment,
                               unsigned cCylinders, unsigned cHeads,
                               unsigned cSectors, unsigned uOpenFlags,
                               PFNVMPROGRESS pfnProgress, void *pvUser);

/**
 * Creates and opens a new differencing image file in HDD container.
 * See comments for VDOpen function about differencing images.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   pszFilename     Name of the differencing image file to create.
 * @param   uImageFlags     Flags specifying special image features.
 * @param   pszComment      Pointer to image comment. NULL is ok.
 * @param   uOpenFlags      Image file open mode, see VD_OPEN_FLAGS_* constants.
 * @param   pfnProgress     Progress callback. Optional. NULL if not to be used.
 * @param   pvUser          User argument for the progress callback.
 */
VBOXDDU_DECL(int) VDCreateDiff(PVBOXHDD pDisk, const char *pszFilename,
                               unsigned uImageFlags, const char *pszComment,
                               unsigned uOpenFlags,
                               PFNVMPROGRESS pfnProgress, void *pvUser);

/**
 * Merges two images having a parent/child relationship (both directions).
 * As a side effect the source image is deleted from both the disk and
 * the images in the VBox HDD container.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImageFrom      Name of the image file to merge from.
 * @param   nImageTo        Name of the image file to merge to.
 * @param   pfnProgress     Progress callback. Optional. NULL if not to be used.
 * @param   pvUser          User argument for the progress callback.
 */
VBOXDDU_DECL(int) VDMerge(PVBOXHDD pDisk, unsigned nImageFrom, unsigned nImageTo,
                          PFNVMPROGRESS pfnProgress, void *pvUser);

/**
 * Copies an image from one VBox HDD container to another.
 * The copy is opened in the target VBox HDD container.
 * It is possible to convert between different image formats, because the
 * backend for the destination VBox HDD container may be different from the
 * source container.
 * If both the source and destination reference the same VBox HDD container,
 * then the image is moved (by copying/deleting) to the new location.
 * The source container is unchanged if the move operation fails, otherwise
 * the image at the new location is opened in the same way as the old one was.
 *
 * @returns VBox status code.
 * @param   pDiskFrom       Pointer to source VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pDiskTo         Pointer to destination VBox HDD container.
 * @param   pfnProgress     Progress callback. Optional. NULL if not to be used.
 * @param   pvUser          User argument for the progress callback.
 */
VBOXDDU_DECL(int) VDCopy(PVBOXHDD pDiskFrom, unsigned nImage, PVBOXHDD pDiskTo,
                         PFNVMPROGRESS pfnProgress, void *pvUser);

/**
 * Compacts a growing image file by removing zeroed data blocks.
 * Optionally defragments data in the image so that ascending sector numbers
 * are stored in ascending location in the image file.
 *
 * @todo maybe include this function in VDCopy.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_NOT_OPENED if no image is opened in HDD container.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   fDefragment     If true, reorder file data so that sectors are stored in ascending order.
 * @param   pfnProgress     Progress callback. Optional. NULL if not to be used.
 * @param   pvUser          User argument for the progress callback.
 */
VBOXDDU_DECL(int) VDCompact(PVBOXHDD pDisk, unsigned nImage,
                            bool fDefragment,
                            PFNVMPROGRESS pfnProgress, void *pvUser);

/**
 * Resizes an image. Allows setting the disk size to both larger and smaller
 * values than the current disk size.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_NOT_OPENED if no image is opened in HDD container.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   cbSize          New image size in bytes.
 * @param   pfnProgress     Progress callback. Optional. NULL if not to be used.
 * @param   pvUser          User argument for the progress callback.
 */
VBOXDDU_DECL(int) VDResize(PVBOXHDD pDisk, unsigned nImage, uint64_t cbSize,
                           PFNVMPROGRESS pfnProgress, void *pvUser);

/**
 * Closes the last opened image file in the HDD container. Leaves all changes inside it.
 * If previous image file was opened in read-only mode (that is normal) and closing image
 * was opened in read-write mode (the whole disk was in read-write mode) - the previous image
 * will be reopened in read/write mode.
 *
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   fDelete         If true, delete the image from the host disk.
 */
VBOXDDU_DECL(int) VDClose(PVBOXHDD pDisk, bool fDelete);

/**
 * Closes all opened image files in HDD container.
 *
 * @param   pDisk           Pointer to VBox HDD container.
 */
VBOXDDU_DECL(int) VDCloseAll(PVBOXHDD pDisk);

/**
 * Read data from virtual HDD.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_NOT_OPENED if no image is opened in HDD container.
 * @param   pDisk           Pointer to VBox HDD container.
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
 * @param   pDisk           Pointer to VBox HDD container.
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
 * @param   pDisk           Pointer to VBox HDD container.
 */
VBOXDDU_DECL(int) VDFlush(PVBOXHDD pDisk);

/**
 * Get number of opened images in HDD container.
 *
 * @returns Number of opened images for HDD container. 0 if no images have been opened.
 * @param   pDisk           Pointer to VBox HDD container.
 */
VBOXDDU_DECL(unsigned) VDGetCount(PVBOXHDD pDisk);

/**
 * Get read/write mode of the VBox HDD container.
 *
 * @returns Virtual disk ReadOnly status.
 * @returns true if no image is opened in HDD container.
 * @param   pDisk           Pointer to VBox HDD container.
 */
VBOXDDU_DECL(bool) VDIsReadOnly(PVBOXHDD pDisk);

/**
 * Get total disk size of the VBox HDD container.
 *
 * @returns Virtual disk size in bytes.
 * @returns 0 if no image is opened in HDD container.
 * @param   pDisk           Pointer to VBox HDD container.
 */
VBOXDDU_DECL(uint64_t) VDGetSize(PVBOXHDD pDisk);

/**
 * Get virtual disk geometry stored in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_NOT_OPENED if no image is opened in HDD container.
 * @returns VERR_VDI_GEOMETRY_NOT_SET if no geometry present in the HDD container.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   pcCylinders     Where to store the number of cylinders. NULL is ok.
 * @param   pcHeads         Where to store the number of heads. NULL is ok.
 * @param   pcSectors       Where to store the number of sectors. NULL is ok.
 */
VBOXDDU_DECL(int) VDGetGeometry(PVBOXHDD pDisk,
                                unsigned *pcCylinders, unsigned *pcHeads, unsigned *pcSectors);

/**
 * Store virtual disk geometry in HDD container.
 *
 * Note that in case of unrecoverable error all images in HDD container will be closed.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_NOT_OPENED if no image is opened in HDD container.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   cCylinders      Number of cylinders.
 * @param   cHeads          Number of heads.
 * @param   cSectors        Number of sectors.
 */
VBOXDDU_DECL(int) VDSetGeometry(PVBOXHDD pDisk,
                                unsigned cCylinders, unsigned cHeads, unsigned cSectors);

/**
 * Get virtual disk translation mode stored in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_NOT_OPENED if no image is opened in HDD container.
 * @returns VERR_VDI_GEOMETRY_NOT_SET if no geometry present in the HDD container.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   penmTranslation Where to store the translation mode (see pdm.h).
 */
VBOXDDU_DECL(int) VDGetTranslation(PVBOXHDD pDisk, PPDMBIOSTRANSLATION penmTranslation);

/**
 * Store virtual disk translation mode in HDD container.
 *
 * Note that in case of unrecoverable error all images in HDD container will be closed.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_NOT_OPENED if no image is opened in HDD container.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   enmTranslation  Translation mode (see pdm.h).
 */
VBOXDDU_DECL(int) VDSetTranslation(PVBOXHDD pDisk, PDMBIOSTRANSLATION enmTranslation);

/**
 * Get version of image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   puVersion       Where to store the image version.
 */
VBOXDDU_DECL(int) VDGetVersion(PVBOXHDD pDisk, unsigned nImage, unsigned *puVersion);

/**
 * Get type of image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   penmType        Where to store the image type.
 */
VBOXDDU_DECL(int) VDGetImageType(PVBOXHDD pDisk, unsigned nImage, PVDIMAGETYPE penmType);

/**
 * Get flags of image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   puImageFlags    Where to store the image flags.
 */
VBOXDDU_DECL(int) VDGetImageFlags(PVBOXHDD pDisk, unsigned nImage, unsigned *puImageFlags);

/**
 * Get open flags of last opened image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_NOT_OPENED if no image is opened in HDD container.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   puOpenFlags     Where to store the image open flags.
 */
VBOXDDU_DECL(int) VDGetOpenFlags(PVBOXHDD pDisk, unsigned *puOpenFlags);

/**
 * Set open flags of last opened image in HDD container.
 * This operation may cause file locking changes and/or files being reopened.
 * Note that in case of unrecoverable error all images in HDD container will be closed.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   uOpenFlags      Image file open mode, see VD_OPEN_FLAGS_* constants.
 */
VBOXDDU_DECL(int) VDSetOpenFlags(PVBOXHDD pDisk, unsigned uOpenFlags);

/**
 * Get base filename of image in HDD container. Some image formats use
 * other filenames as well, so don't use this for anything but for informational
 * purposes.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @returns VERR_BUFFER_OVERFLOW if pszFilename buffer too small to hold filename.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pszFilename     Where to store the image file name.
 * @param   cbFilename      Size of buffer pszFilename points to.
 */
VBOXDDU_DECL(int) VDGetFilename(PVBOXHDD pDisk, unsigned nImage, char *pszFilename, unsigned cbFilename);

/**
 * Get the comment line of image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @returns VERR_BUFFER_OVERFLOW if pszComment buffer too small to hold comment text.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pszComment      Where to store the comment string of image. NULL is ok.
 * @param   cbComment       The size of pszComment buffer. 0 is ok.
 */
VBOXDDU_DECL(int) VDGetComment(PVBOXHDD pDisk, unsigned nImage, char *pszComment, unsigned cbComment);

/**
 * Changes the comment line of image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pszComment      New comment string (UTF-8). NULL is allowed to reset the comment.
 */
VBOXDDU_DECL(int) VDSetComment(PVBOXHDD pDisk, unsigned nImage, const char *pszComment);

/**
 * Get UUID of image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pUuid           Where to store the image UUID.
 */
VBOXDDU_DECL(int) VDGetUuid(PVBOXHDD pDisk, unsigned nImage, PRTUUID pUuid);

/**
 * Set the image's UUID. Should not be used by normal applications.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pUuid           Optional parameter, new UUID of the image.
 */
VBOXDDU_DECL(int) VDSetUuid(PVBOXHDD pDisk, unsigned nImage, PCRTUUID pUuid);

/**
 * Get last modification UUID of image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pUuid           Where to store the image modification UUID.
 */
VBOXDDU_DECL(int) VDGetModificationUuid(PVBOXHDD pDisk, unsigned nImage, PRTUUID pUuid);

/**
 * Set the image's last modification UUID. Should not be used by normal applications.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pUuid           Optional parameter, new last modification UUID of the image.
 */
VBOXDDU_DECL(int) VDSetModificationUuid(PVBOXHDD pDisk, unsigned nImage, PCRTUUID pUuid);

/**
 * Get parent UUID of image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of the container.
 * @param   pUuid           Where to store the parent image UUID.
 */
VBOXDDU_DECL(int) VDGetParentUuid(PVBOXHDD pDisk, unsigned nImage, PRTUUID pUuid);

/**
 * Set the image's parent UUID. Should not be used by normal applications.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pUuid           Optional parameter, new parent UUID of the image.
 */
VBOXDDU_DECL(int) VDSetParentUuid(PVBOXHDD pDisk, unsigned nImage, PCRTUUID pUuid);


/**
 * Debug helper - dumps all opened images in HDD container into the log file.
 *
 * @param   pDisk           Pointer to VBox HDD container.
 */
VBOXDDU_DECL(void) VDDumpImages(PVBOXHDD pDisk);

__END_DECLS

/** @} */

#endif
