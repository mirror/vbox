/** @file
 * VBox VMDK HDD Container API.
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

#ifndef __VBox_VmdkHDD_h__
#define __VBox_VmdkHDD_h__

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/param.h>
#include <VBox/pdm.h>
#include <VBox/vmapi.h>

__BEGIN_DECLS

#ifdef IN_RING0
# error "There are no VMDK APIs available in Ring-0 Host Context!"
#endif

/** @defgroup grp_vmdk_hdd     VBox VMDK HDD Container
 * @{
 */

/** Current image version. */
#define VMDK_IMAGE_VERSION      (0x0001)

/** @name VMDK image types
 * @{ */
typedef enum VMDKIMAGETYPE
{
    /** Normal dynamically growing base image file. */
    VMDK_IMAGE_TYPE_NORMAL  = 1,
    /** Preallocated base image file of a fixed size. */
    VMDK_IMAGE_TYPE_FIXED,
    /** Dynamically growing image file for undo/commit changes support. */
    VMDK_IMAGE_TYPE_UNDO,
    /** Dynamically growing image file for differencing support. */
    VMDK_IMAGE_TYPE_DIFF,

    /** First valid image type value. */
    VMDK_IMAGE_TYPE_FIRST   = VMDK_IMAGE_TYPE_NORMAL,
    /** Last valid image type value. */
    VMDK_IMAGE_TYPE_LAST    = VMDK_IMAGE_TYPE_DIFF
} VMDKIMAGETYPE;
/** Pointer to VMDK image type. */
typedef VMDKIMAGETYPE *PVMDKIMAGETYPE;
/** @} */

/** @name VMDK image flags
 * @{
 */
/** No flags. */
#define VMDK_IMAGE_FLAGS_NONE           (0)
/** Split image into 2GB extents. */
#define VMDK_IMAGE_FLAGS_SPLIT_2G       (1)
/** Split image into 2GB extents. */
#define VMDK_IMAGE_FLAGS_RAWDISK        (2)

/** Mask of valid image flags. */
#define VMDK_IMAGE_FLAGS_MASK           (VMDK_IMAGE_FLAGS_SPLIT_2G | VMDK_IMAGE_FLAGS_RAWDISK)

/** Default image flafs. */
#define VMDK_IMAGE_FLAGS_DEFAULT        (VMDK_IMAGE_FLAGS_NONE)
/** @} */

/** @name VMDK image open mode flags
 * @{
 */
/** Try to open image in read/write exclusive access mode if possible, or in read-only elsewhere. */
#define VMDK_OPEN_FLAGS_NORMAL          (0)
/** Open image in read-only mode with sharing access with others. */
#define VMDK_OPEN_FLAGS_READONLY        (1)
/** Honor zero block writes instead of ignoring them if possible. */
#define VMDK_OPEN_FLAGS_HONOR_ZEROES    (2)
/** Mask of valid flags. */
#define VMDK_OPEN_FLAGS_MASK            (VMDK_OPEN_FLAGS_NORMAL | VMDK_OPEN_FLAGS_READONLY | VMDK_OPEN_FLAGS_HONOR_ZEROES)
/** @}*/

/**
 * VBox VMDK HDD Container main structure.
 */
/* Forward declaration, VMDKDISK structure is visible only inside VMDK module. */
struct VMDKDISK;
typedef struct VMDKDISK VMDKDISK;
typedef VMDKDISK *PVMDKDISK;

/**
 * Creates a new base image file.
 *
 * @returns VBox status code.
 * @param   pszFilename     Name of the image file to create.
 * @param   enmType         Image type, only base image types are acceptable.
 * @param   cbSize          Image size in bytes.
 * @param   uImageFlags     Flags specifying special image features.
 * @param   pszComment      Pointer to image comment. NULL is ok.
 * @param   pfnProgress     Progress callback. Optional. NULL if not to be used.
 * @param   pvUser          User argument for the progress callback.
 */
VBOXDDU_DECL(int) VMDKCreateBaseImage(const char *pszFilename, VMDKIMAGETYPE enmType,
                                      uint64_t cbSize, unsigned uImageFlags, const char *pszComment,
                                      PFNVMPROGRESS pfnProgress, void *pvUser);

/**
 * Creates a differencing dynamically growing image file for specified parent image.
 *
 * @returns VBox status code.
 * @param   pszFilename     Name of the differencing image file to create.
 * @param   pszParent       Name of the parent image file. May be base or diff image type.
 * @param   uImageFlags     Flags specifying special image features.
 * @param   pszComment      Pointer to image comment. NULL is ok.
 * @param   pfnProgress     Progress callback. Optional. NULL if not to be used.
 * @param   pvUser          User argument for the progress callback.
 */
VBOXDDU_DECL(int) VMDKCreateDifferenceImage(const char *pszFilename, const char *pszParent,
                                            unsigned uImageFlags, const char *pszComment,
                                            PFNVMPROGRESS pfnProgress, void *pvUser);

/**
 * Checks if image is available and not broken, returns some useful image parameters if requested.
 *
 * @returns VBox status code.
 * @param   pszFilename     Name of the image file to check.
 * @param   puVersion       Where to store the version of image. NULL is ok.
 * @param   penmType        Where to store the type of image. NULL is ok.
 * @param   pcbSize         Where to store the size of image in bytes. NULL is ok.
 * @param   pUuid           Where to store the uuid of image creation. NULL is ok.
 * @param   pParentUuid     Where to store the uuid of the parent image (if any). NULL is ok.
 * @param   pszComment      Where to store the comment string of image. NULL is ok.
 * @param   cbComment       The size of pszComment buffer. 0 is ok.
 */
VBOXDDU_DECL(int) VMDKCheckImage(const char *pszFilename, unsigned *puVersion,
                                 PVMDKIMAGETYPE penmType, uint64_t *pcbSize,
                                 PRTUUID pUuid, PRTUUID pParentUuid,
                                 char *pszComment, unsigned cbComment);

/**
 * Changes an image's comment string.
 *
 * @returns VBox status code.
 * @param   pszFilename     Name of the image file to operate on.
 * @param   pszComment      New comment string (UTF-8). NULL is allowed to reset the comment.
 */
VBOXDDU_DECL(int) VMDKSetImageComment(const char *pszFilename, const char *pszComment);

/**
 * Deletes a valid image file. Fails if specified file is not an image.
 *
 * @returns VBox status code.
 * @param   pszFilename     Name of the image file to check.
 */
VBOXDDU_DECL(int) VMDKDeleteImage(const char *pszFilename);

/**
 * Makes a copy of image file with a new (other) creation uuid.
 *
 * @returns VBox status code.
 * @param   pszDstFilename  Name of the image file to create.
 * @param   pszSrcFilename  Name of the image file to copy from.
 * @param   pszComment      Pointer to image comment. If NULL, the comment
 *                          will be copied from the source image.
 * @param   pfnProgress     Progress callback. Optional. NULL if not to be used.
 * @param   pvUser          User argument for the progress callback.
 */
VBOXDDU_DECL(int) VMDKCopyImage(const char *pszDstFilename, const char *pszSrcFilename, const char *pszComment,
                               PFNVMPROGRESS pfnProgress, void *pvUser);

/**
 * Shrinks growing image file by removing zeroed data blocks.
 *
 * @returns VBox status code.
 * @param   pszFilename     Name of the image file to shrink.
 * @param   pfnProgress     Progress callback. Optional. NULL if not to be used.
 * @param   pvUser          User argument for the progress callback.
 */
VBOXDDU_DECL(int) VMDKShrinkImage(const char *pszFilename, PFNVMPROGRESS pfnProgress, void *pvUser);

/**
 * Queries the image's UUID and parent UUIDs.
 *
 * @returns VBox status code.
 * @param   pszFilename             Name of the image file to operate on.
 * @param   pUuid                   Where to store image UUID (can be NULL).
 * @param   pModificationUuid       Where to store modification UUID (can be NULL).
 * @param   pParentUuuid            Where to store parent UUID (can be NULL).
 * @param   pParentModificationUuid Where to store parent modification UUID (can be NULL).
 */
VBOXDDU_DECL(int) VMDKGetImageUUIDs(const char *pszFilename,
                                   PRTUUID pUuid, PRTUUID pModificationUuid,
                                   PRTUUID pParentUuid, PRTUUID pParentModificationUuid);


/**
 * Changes the image's UUID and parent UUIDs.
 *
 * @returns VBox status code.
 * @param   pszFilename             Name of the image file to operate on.
 * @param   pUuid                   Optional parameter, new UUID of the image.
 * @param   pModificationUuid       Optional parameter, new modification UUID of the image.
 * @param   pParentUuuid            Optional parameter, new parent UUID of the image.
 * @param   pParentModificationUuid Optional parameter, new parent modification UUID of the image.
 */
VBOXDDU_DECL(int) VMDKSetImageUUIDs(const char *pszFilename,
                                   PCRTUUID pUuid, PCRTUUID pModificationUuid,
                                   PCRTUUID pParentUuid, PCRTUUID pParentModificationUuid);

/**
 * Merges two images having a parent/child relationship (both directions).
 *
 * @returns VBox status code.
 * @param   pszFilenameFrom         Name of the image file to merge from.
 * @param   pszFilenameTo           Name of the image file to merge into.
 * @param   pfnProgress             Progress callback. Optional. NULL if not to be used.
 * @param   pvUser                  User argument for the progress callback.
 */
VBOXDDU_DECL(int) VMDKMergeImage(const char *pszFilenameFrom, const char *pszFilenameTo,
                                PFNVMPROGRESS pfnProgress, void *pvUser);


/**
 * Allocates and initializes an empty VMDK HDD container.
 * No image files are opened.
 *
 * @returns Pointer to newly created empty HDD container.
 * @returns NULL on failure, typically out of memory.
 */
VBOXDDU_DECL(PVMDKDISK) VMDKDiskCreate(void);

/**
 * Destroys the VMDK HDD container. If container has opened image files they will be closed.
 *
 * @param   pDisk           Pointer to VMDK HDD container.
 */
VBOXDDU_DECL(void) VMDKDiskDestroy(PVMDKDISK pDisk);

/**
 * Opens an image file.
 *
 * The first opened image file in a HDD container must have a base image type,
 * others (next opened images) must be a differencing or undo images.
 * Linkage is checked for differencing image to be in consistence with the previously opened image.
 * When a next differencing image is opened and the last image was opened in read/write access
 * mode, then the last image is reopened in read-only with deny write sharing mode. This allows
 * other processes to use images in read-only mode too.
 *
 * Note that the image can be opened in read-only mode if a read/write open is not possible.
 * Use VMDKDiskIsReadOnly to check open mode.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to VMDK HDD container.
 * @param   pszFilename     Name of the image file to open.
 * @param   fOpen           Image file open mode, see VMDK_OPEN_FLAGS_* constants.
 */
VBOXDDU_DECL(int) VMDKDiskOpenImage(PVMDKDISK pDisk, const char *pszFilename, unsigned fOpen);

/**
 * Creates and opens a new differencing image file in HDD container.
 * See comments for VMDKDiskOpenImage function about differencing images.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to VMDK HDD container.
 * @param   pszFilename     Name of the image file to create and open.
 * @param   pszComment      Pointer to image comment. NULL is ok.
 * @param   pfnProgress     Progress callback. Optional. NULL if not to be used.
 * @param   pvUser          User argument for the progress callback.
 */
VBOXDDU_DECL(int) VMDKDiskCreateOpenDifferenceImage(PVMDKDISK pDisk, const char *pszFilename, const char *pszComment,
                                                   PFNVMPROGRESS pfnProgress, void *pvUser);

/**
 * Closes the last opened image file in the HDD container. Leaves all changes inside it.
 * If previous image file was opened in read-only mode (that is normal) and closing image
 * was opened in read-write mode (the whole disk was in read-write mode) - the previous image
 * will be reopened in read/write mode.
 *
 * @param   pDisk           Pointer to VMDK HDD container.
 */
VBOXDDU_DECL(void) VMDKDiskCloseImage(PVMDKDISK pDisk);

/**
 * Closes all opened image files in HDD container.
 *
 * @param   pDisk           Pointer to VMDK HDD container.
 */
VBOXDDU_DECL(void) VMDKDiskCloseAllImages(PVMDKDISK pDisk);

/**
 * Commits last opened differencing/undo image file of the HDD container to previous image.
 * If the previous image file was opened in read-only mode (that must be always so) it is reopened
 * as read/write to do commit operation.
 * After successfull commit the previous image file again reopened in read-only mode, last opened
 * image file is cleared of data and remains open and active in HDD container.
 * If you want to delete image after commit you must do it manually by VMDKDiskCloseImage and
 * VMDKDeleteImage calls.
 *
 * Note that in case of unrecoverable error all images of HDD container will be closed.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to VMDK HDD container.
 * @param   pfnProgress     Progress callback. Optional.
 * @param   pvUser          User argument for the progress callback.
 */
VBOXDDU_DECL(int) VMDKDiskCommitLastDiff(PVMDKDISK pDisk, PFNVMPROGRESS pfnProgress, void *pvUser);

/**
 * Get read/write mode of VMDK HDD.
 *
 * @returns Disk ReadOnly status.
 * @returns true if no VMDK image is opened in HDD container.
 */
VBOXDDU_DECL(bool) VMDKDiskIsReadOnly(PVMDKDISK pDisk);

/**
 * Get total disk size of the VMDK HDD container.
 *
 * @returns Virtual disk size in bytes.
 * @returns 0 if no VMDK image is opened in HDD container.
 */
VBOXDDU_DECL(uint64_t) VMDKDiskGetSize(PVMDKDISK pDisk);

/**
 * Get block size of the VMDK HDD container.
 *
 * @returns VMDK image block size in bytes.
 * @returns 0 if no VMDK image is opened in HDD container.
 */
VBOXDDU_DECL(unsigned) VMDKDiskGetBlockSize(PVMDKDISK pDisk);

/**
 * Get working buffer size of the VMDK HDD container.
 *
 * @returns Working buffer size in bytes.
 */
VBOXDDU_DECL(unsigned) VMDKDiskGetBufferSize(PVMDKDISK pDisk);

/**
 * Get virtual disk geometry stored in image file.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_NOT_OPENED if no VMDK image is opened in HDD container.
 * @returns VERR_VDI_GEOMETRY_NOT_SET if no geometry present in the HDD container.
 * @param   pDisk           Pointer to VMDK HDD container.
 * @param   pcCylinders     Where to store the number of cylinders. NULL is ok.
 * @param   pcHeads         Where to store the number of heads. NULL is ok.
 * @param   pcSectors       Where to store the number of sectors. NULL is ok.
 */
VBOXDDU_DECL(int) VMDKDiskGetGeometry(PVMDKDISK pDisk, unsigned *pcCylinders, unsigned *pcHeads, unsigned *pcSectors);

/**
 * Store virtual disk geometry into base image file of HDD container.
 *
 * Note that in case of unrecoverable error all images of HDD container will be closed.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_NOT_OPENED if no VMDK image is opened in HDD container.
 * @param   pDisk           Pointer to VMDK HDD container.
 * @param   cCylinders      Number of cylinders.
 * @param   cHeads          Number of heads.
 * @param   cSectors        Number of sectors.
 */
VBOXDDU_DECL(int) VMDKDiskSetGeometry(PVMDKDISK pDisk, unsigned cCylinders, unsigned cHeads, unsigned cSectors);

/**
 * Get virtual disk translation mode stored in image file.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_NOT_OPENED if no VMDK image is opened in HDD container.
 * @param   pDisk           Pointer to VMDK HDD container.
 * @param   penmTranslation Where to store the translation mode (see pdm.h).
 */
VBOXDDU_DECL(int) VMDKDiskGetTranslation(PVMDKDISK pDisk, PPDMBIOSTRANSLATION penmTranslation);

/**
 * Store virtual disk translation mode into base image file of HDD container.
 *
 * Note that in case of unrecoverable error all images of HDD container will be closed.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_NOT_OPENED if no VMDK image is opened in HDD container.
 * @param   pDisk           Pointer to VMDK HDD container.
 * @param   enmTranslation  Translation mode (see pdm.h).
 */
VBOXDDU_DECL(int) VMDKDiskSetTranslation(PVMDKDISK pDisk, PDMBIOSTRANSLATION enmTranslation);

/**
 * Get number of opened images in HDD container.
 *
 * @returns Number of opened images for HDD container. 0 if no images has been opened.
 * @param   pDisk           Pointer to VMDK HDD container.
 */
VBOXDDU_DECL(int) VMDKDiskGetImagesCount(PVMDKDISK pDisk);

/**
 * Get version of opened image of HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to VMDK HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   puVersion       Where to store the image version.
 */
VBOXDDU_DECL(int) VMDKDiskGetImageVersion(PVMDKDISK pDisk, int nImage, unsigned *puVersion);

/**
 * Get type of opened image of HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to VMDK HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   penmType        Where to store the image type.
 */
VBOXDDU_DECL(int) VMDKDiskGetImageType(PVMDKDISK pDisk, int nImage, PVMDKIMAGETYPE penmType);

/**
 * Get flags of opened image of HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to VMDK HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pfFlags         Where to store the image flags.
 */
VBOXDDU_DECL(int) VMDKDiskGetImageFlags(PVMDKDISK pDisk, int nImage, unsigned *pfFlags);

/**
 * Get base filename of opened image of HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @returns VERR_BUFFER_OVERFLOW if pszFilename buffer too small to hold filename.
 * @param   pDisk           Pointer to VMDK HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pszFilename     Where to store the image file name.
 * @param   cbFilename      Size of buffer pszFilename points to.
 */
VBOXDDU_DECL(int) VMDKDiskGetImageFilename(PVMDKDISK pDisk, int nImage, char *pszFilename, unsigned cbFilename);

/**
 * Get the comment line of opened image of HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @returns VERR_BUFFER_OVERFLOW if pszComment buffer too small to hold comment text.
 * @param   pDisk           Pointer to VMDK HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pszComment      Where to store the comment string of image. NULL is ok.
 * @param   cbComment       The size of pszComment buffer. 0 is ok.
 */
VBOXDDU_DECL(int) VMDKDiskGetImageComment(PVMDKDISK pDisk, int nImage, char *pszComment, unsigned cbComment);

/**
 * Get Uuid of opened image of HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to VMDK HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pUuid           Where to store the image creation uuid.
 */
VBOXDDU_DECL(int) VMDKDiskGetImageUuid(PVMDKDISK pDisk, int nImage, PRTUUID pUuid);

/**
 * Get last modification Uuid of opened image of HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to VMDK HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pUuid           Where to store the image modification uuid.
 */
VBOXDDU_DECL(int) VMDKDiskGetImageModificationUuid(PVMDKDISK pDisk, int nImage, PRTUUID pUuid);

/**
 * Get Uuid of opened image's parent image.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to VMDK HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of the container.
 * @param   pUuid           Where to store the image creation uuid.
 */
VBOXDDU_DECL(int) VMDKDiskGetParentImageUuid(PVMDKDISK pDisk, int nImage, PRTUUID pUuid);

/**
 * Read data from virtual HDD.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to VMDK HDD container.
 * @param   offStart        Offset of first reading byte from start of disk.
 * @param   pvBuf           Pointer to buffer for reading data.
 * @param   cbToRead        Number of bytes to read.
 */
VBOXDDU_DECL(int) VMDKDiskRead(PVMDKDISK pDisk, uint64_t offStart, void *pvBuf, unsigned cbToRead);

/**
 * Write data to virtual HDD.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to VMDK HDD container.
 * @param   offStart        Offset of first writing byte from start of HDD.
 * @param   pvBuf           Pointer to buffer of writing data.
 * @param   cbToWrite       Number of bytes to write.
 */
VBOXDDU_DECL(int) VMDKDiskWrite(PVMDKDISK pDisk, uint64_t offStart, const void *pvBuf, unsigned cbToWrite);



/**
 * Debug helper - dumps all opened images of HDD container into the log file.
 *
 * @param   pDisk           Pointer to VMDK HDD container.
 */
VBOXDDU_DECL(void) VMDKDiskDumpImages(PVMDKDISK pDisk);

__END_DECLS

/** @} */

#endif
