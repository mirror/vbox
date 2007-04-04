/** @file
 * VBox HDD Container API.
 * Will replace VBoxHDD.h.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef __VBox_VBoxHDDNew_h__
#define __VBox_VBoxHDDNew_h__

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/pdm.h>

__BEGIN_DECLS

#ifdef IN_RING0
# error "There are no VBox HDD Container APIs available in Ring-0 Host Context!"
#endif

/** @defgroup grp_vbox_hdd     VBox HDD Container
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
typedef enum VBOXHDDIMAGETYPE
{
    /** Normal dynamically growing base image file. */
    VBOXHDD_IMAGE_TYPE_NORMAL   = 1,
    /** Preallocated base image file of a fixed size. */
    VBOXHDD_IMAGE_TYPE_FIXED,
    /** Dynamically growing image file for undo/commit changes support. */
    VBOXHDD_IMAGE_TYPE_UNDO,
    /** Dynamically growing image file for differencing support. */
    VBOXHDD_IMAGE_TYPE_DIFF,

    /** First valid image type value. */
    VBOXHDD_IMAGE_TYPE_FIRST    = VBOXHDD_IMAGE_TYPE_NORMAL,
    /** Last valid image type value. */
    VBOXHDD_IMAGE_TYPE_LAST     = VBOXHDD_IMAGE_TYPE_DIFF
} VBOXHDDIMAGETYPE;
/** Pointer to VBox HDD container image type. */
typedef VBOXHDDIMAGETYPE *PVBOXHDDIMAGETYPE;
/** @} */

/** @name VBox HDD container image flags
 * @{
 */
/** No flags. */
#define VBOXHDD_IMAGE_FLAGS_NONE                (0)
/** VMDK: Split image into 2GB extents. */
#define VBOXHDD_VMDK_IMAGE_FLAGS_SPLIT_2G       (0x0001)
/** VMDK: Split image into 2GB extents. */
#define VBOXHDD_VMDK_IMAGE_FLAGS_RAWDISK        (0x0002)
/** VDI: Fill new blocks with zeroes while expanding image file. */
#define VBOXHDD_VDI_IMAGE_FLAGS_ZERO_EXPAND     (0x0100)

/** Mask of valid image flags for VMDK. */
#define VBOXHDD_VMDK_IMAGE_FLAGS_MASK           (VBOXHDD_IMAGE_FLAGS_NONE | VBOXHDD_VMDK_IMAGE_FLAGS_SPLIT_2G | VBOXHDD_VMDK_IMAGE_FLAGS_RAWDISK)

/** Mask of valid image flags for VDI. */
#define VBOXHDD_VDI_IMAGE_FLAGS_MASK            (VBOXHDD_IMAGE_FLAGS_NONE | VBOXHDD_VDI_IMAGE_FLAGS_ZERO_EXPAND)

/** Default image flags. */
#define VBOXHDD_IMAGE_FLAGS_DEFAULT             (VBOXHDD_IMAGE_FLAGS_NONE)
/** @} */

/** @name VBox HDD container image open mode flags
 * @{
 */
/** Try to open image in read/write exclusive access mode if possible, or in read-only elsewhere. */
#define VBOXHDD_OPEN_FLAGS_NORMAL       (0)
/** Open image in read-only mode with sharing access with others. */
#define VBOXHDD_OPEN_FLAGS_READONLY     (1)
/** Honor zero block writes instead of ignoring them whenever possible. */
#define VBOXHDD_OPEN_FLAGS_HONOR_ZEROES (2)
/** Mask of valid flags. */
#define VBOXHDD_OPEN_FLAGS_MASK         (VBOXHDD_OPEN_FLAGS_NORMAL | VBOXHDD_OPEN_FLAGS_READONLY | VBOXHDD_OPEN_FLAGS_HONOR_ZEROES)
/** @}*/

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
 * @returns Pointer to newly created empty HDD container.
 * @returns NULL on failure, typically out of memory.
 * @param   pszBackend      Name of the image file backend to use.
 */
VBOXDDU_DECL(PVBOXHDD) VBOXHDDCreate(const char *pszBackend);

/**
 * Destroys the VBox HDD container.
 * If container has opened image files they will be closed.
 *
 * @param   pDisk           Pointer to VBox HDD container.
 */
VBOXDDU_DECL(void) VBOXHDDDestroy(PVBOXHDD pDisk);

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
 * Use VBOXHDDIsReadOnly to check open mode.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   pszFilename     Name of the image file to open.
 * @param   uOpenFlags      Image file open mode, see VBOXHDD_OPEN_FLAGS_* constants.
 */
VBOXDDU_DECL(int) VBOXHDDOpenImage(PVBOXHDD pDisk, const char *pszFilename, unsigned uOpenFlags);

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
 * @param   uOpenFlags      Image file open mode, see VBOXHDD_OPEN_FLAGS_* constants.
 * @param   pfnProgress     Progress callback. Optional. NULL if not to be used.
 * @param   pvUser          User argument for the progress callback.
 */
VBOXDDU_DECL(int) VBOXHDDCreateBaseImage(PVBOXHDD pDisk, const char *pszFilename,
                                         VBOXHDDIMAGETYPE enmType, uint64_t cbSize,
                                         unsigned uImageFlags, const char *pszComment,
                                         unsigned uOpenFlags,
                                         PFNVMPROGRESS pfnProgress, void *pvUser);

/**
 * Creates and opens a new differencing image file in HDD container.
 * See comments for VBOXHDDOpenImage function about differencing images.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   pszFilename     Name of the differencing image file to create.
 * @param   uImageFlags     Flags specifying special image features.
 * @param   pszComment      Pointer to image comment. NULL is ok.
 * @param   uOpenFlags      Image file open mode, see VBOXHDD_OPEN_FLAGS_* constants.
 * @param   pfnProgress     Progress callback. Optional. NULL if not to be used.
 * @param   pvUser          User argument for the progress callback.
 */
VBOXDDU_DECL(int) VBOXHDDCreateDifferenceImage(PVBOXHDD pDisk, const char *pszFilename,
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
VBOXDDU_DECL(int) VBOXHDDMergeImage(PVBOXHDD pDisk, unsigned nImageFrom, unsigned nImageTo,
                                    PFNVMPROGRESS pfnProgress, void *pvUser);

/**
 * Copies an image from one VBox HDD container to another.
 * The copy is opened in the target VBox HDD container.
 * It is possible to convert between different image formats, because the
 * backend for the destination VBox HDD container may be different from the
 * source container.
 *
 * @returns VBox status code.
 * @param   pDiskFrom       Pointer to source VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pDiskTo         Pointer to destination VBox HDD container.
 * @param   pfnProgress     Progress callback. Optional. NULL if not to be used.
 * @param   pvUser          User argument for the progress callback.
 */
VBOXDDU_DECL(int) VBOXHDDCopyImage(PVBOXHDD pDiskFrom, unsigned nImage, PVBOXHDD pDiskTo,
                                   PFNVMPROGRESS pfnProgress, void *pvUser);

/**
 * Compacts a growing image file by removing zeroed data blocks.
 * Optionally defragments data in the image so that ascending sector numbers
 * are stored in ascending location in the image file.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   fDefragment     If true, reorder file data so that sectors are stored in ascending order.
 * @param   pfnProgress     Progress callback. Optional. NULL if not to be used.
 * @param   pvUser          User argument for the progress callback.
 */
VBOXDDU_DECL(int) VBOXHDDCompactImage(PVBOXHDD pDisk, unsigned nImage,
                                      bool fDefragment,
                                      PFNVMPROGRESS pfnProgress, void *pvUser);

/**
 * Resizes an image. Allows setting the disk size to both larger and smaller
 * values than the current disk size.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   cbSize          New image size in bytes.
 * @param   pfnProgress     Progress callback. Optional. NULL if not to be used.
 * @param   pvUser          User argument for the progress callback.
 */
VBOXDDU_DECL(int) VBOXHDDResizeImage(PVBOXHDD pDisk, unsigned nImage, uint64_t cbSize,
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
VBOXDDU_DECL(void) VBOXHDDCloseImage(PVBOXHDD pDisk, bool fDelete);

/**
 * Closes all opened image files in HDD container.
 *
 * @param   pDisk           Pointer to VBox HDD container.
 */
VBOXDDU_DECL(void) VBOXHDDCloseAllImages(PVBOXHDD pDisk);

/**
 * Read data from virtual HDD.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   offStart        Offset of first reading byte from start of disk.
 * @param   pvBuf           Pointer to buffer for reading data.
 * @param   cbToRead        Number of bytes to read.
 */
VBOXDDU_DECL(int) VBOXHDDRead(PVBOXHDD pDisk, uint64_t offStart, void *pvBuf, unsigned cbToRead);

/**
 * Write data to virtual HDD.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   offStart        Offset of first writing byte from start of disk.
 * @param   pvBuf           Pointer to buffer of writing data.
 * @param   cbToWrite       Number of bytes to write.
 */
VBOXDDU_DECL(int) VBOXHDDWrite(PVBOXHDD pDisk, uint64_t offStart, const void *pvBuf, unsigned cbToWrite);

/**
 * Get number of opened images in HDD container.
 *
 * @returns Number of opened images for HDD container. 0 if no images has been opened.
 * @param   pDisk           Pointer to VBox HDD container.
 */
VBOXDDU_DECL(int) VBOXHDDGetImagesCount(PVBOXHDD pDisk);

/**
 * Get read/write mode of the VBox HDD container.
 *
 * @returns Virtual disk ReadOnly status.
 * @returns true if no image is opened in HDD container.
 * @param   pDisk           Pointer to VBox HDD container.
 */
VBOXDDU_DECL(bool) VBOXHDDIsReadOnly(PVBOXHDD pDisk);

/**
 * Get total disk size of the VBox HDD container.
 *
 * @returns Virtual disk size in bytes.
 * @returns 0 if no image is opened in HDD container.
 * @param   pDisk           Pointer to VBox HDD container.
 */
VBOXDDU_DECL(uint64_t) VBOXHDDGetSize(PVBOXHDD pDisk);

/**
 * Get block size of the VBox HDD container.
 *
 * @returns Virtual disk block size in bytes.
 * @returns 0 if no image is opened in HDD container.
 * @param   pDisk           Pointer to VBox HDD container.
 */
VBOXDDU_DECL(unsigned) VBOXHDDGetBlockSize(PVBOXHDD pDisk);

/**
 * Get optimal buffer size for working with the VBox HDD container.
 * The working buffer size is an integral multiple of the block size.
 *
 * @returns Virtual disk working buffer size in bytes.
 * @param   pDisk           Pointer to VBox HDD container.
 */
VBOXDDU_DECL(unsigned) VBOXHDDGetBufferSize(PVBOXHDD pDisk);

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
VBOXDDU_DECL(int) VBOXHDDGetGeometry(PVBOXHDD pDisk, unsigned *pcCylinders, unsigned *pcHeads, unsigned *pcSectors);

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
VBOXDDU_DECL(int) VBOXHDDSetGeometry(PVBOXHDD pDisk, unsigned cCylinders, unsigned cHeads, unsigned cSectors);

/**
 * Get virtual disk translation mode stored in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_NOT_OPENED if no image is opened in HDD container.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   penmTranslation Where to store the translation mode (see pdm.h).
 */
VBOXDDU_DECL(int) VBOXHDDGetTranslation(PVBOXHDD pDisk, PPDMBIOSTRANSLATION penmTranslation);

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
VBOXDDU_DECL(int) VBOXHDDSetTranslation(PVBOXHDD pDisk, PDMBIOSTRANSLATION enmTranslation);

/**
 * Get version of opened image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   puVersion       Where to store the image version.
 */
VBOXDDU_DECL(int) VBOXHDDGetImageVersion(PVBOXHDD pDisk, unsigned nImage, unsigned *puVersion);

/**
 * Get type of opened image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   penmType        Where to store the image type.
 */
VBOXDDU_DECL(int) VBOXHDDGetImageType(PVBOXHDD pDisk, unsigned nImage, PVBOXHDDIMAGETYPE penmType);

/**
 * Get flags of opened image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   puImageFlags    Where to store the image flags.
 */
VBOXDDU_DECL(int) VBOXHDDGetImageFlags(PVBOXHDD pDisk, unsigned nImage, unsigned *puImageFlags);

/**
 * Get base filename of opened image in HDD container. Some image formats use
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
VBOXDDU_DECL(int) VBOXHDDGetImageFilename(PVBOXHDD pDisk, unsigned nImage, char *pszFilename, unsigned cbFilename);

/**
 * Get the comment line of opened image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @returns VERR_BUFFER_OVERFLOW if pszComment buffer too small to hold comment text.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pszComment      Where to store the comment string of image. NULL is ok.
 * @param   cbComment       The size of pszComment buffer. 0 is ok.
 */
VBOXDDU_DECL(int) VBOXHDDGetImageComment(PVBOXHDD pDisk, unsigned nImage, char *pszComment, unsigned cbComment);

/**
 * Changes the comment line of opened image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pszComment      New comment string (UTF-8). NULL is allowed to reset the comment.
 */
VBOXDDU_DECL(int) VBOXHDDSetImageComment(PVBOXHDD pDisk, unsigned nImage, const char *pszComment);

/**
 * Get UUID of opened image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pUuid           Where to store the image creation uuid.
 */
VBOXDDU_DECL(int) VBOXHDDGetImageUuid(PVBOXHDD pDisk, unsigned nImage, PRTUUID pUuid);

/**
 * Set the opened image's UUID. Should not be used by normal applications.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pUuid           Optional parameter, new UUID of the image.
 */
VBOXDDU_DECL(int) VBOXHDDSetImageUuid(PVBOXHDD pDisk, unsigned nImage, PCRTUUID pUuid);

/**
 * Get last modification UUID of opened image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pUuid           Where to store the image modification uuid.
 */
VBOXDDU_DECL(int) VBOXHDDGetImageModificationUuid(PVBOXHDD pDisk, unsigned nImage, PRTUUID pUuid);

/**
 * Set the opened image's last modification UUID. Should not be used by normal applications.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pUuid           Optional parameter, new last modification UUID of the image.
 */
VBOXDDU_DECL(int) VBOXHDDSetImageModificationUuid(PVBOXHDD pDisk, unsigned nImage, PCRTUUID pUuid);

/**
 * Get parent UUID of opened image in HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of the container.
 * @param   pUuid           Where to store the image creation uuid.
 */
VBOXDDU_DECL(int) VBOXHDDGetParentImageUuid(PVBOXHDD pDisk, unsigned nImage, PRTUUID pUuid);

/**
 * Set the image's parent UUID. Should not be used by normal applications.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to VBox HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pUuid           Optional parameter, new parent UUID of the image.
 */
VBOXDDU_DECL(int) VBOXHDDSetImageParentUuid(PVBOXHDD pDisk, unsigned nImage, PCRTUUID pUuid);



/**
 * Debug helper - dumps all opened images in HDD container into the log file.
 *
 * @param   pDisk           Pointer to VBox HDD container.
 */
VBOXDDU_DECL(void) VBOXHDDDumpImages(PVBOXHDD pDisk);

__END_DECLS

/** @} */

#endif
