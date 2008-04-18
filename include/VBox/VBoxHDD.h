/** @file
 * VBox HDD Container, Virtual Disk Image (VDI) API.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___VBox_VBoxHDD_h
#define ___VBox_VBoxHDD_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/param.h>
#include <VBox/pdm.h>
#include <VBox/vmapi.h>

__BEGIN_DECLS

#ifdef IN_RING0
# error "There are no VDI APIs available in Ring-0 Host Context!"
#endif

/** @defgroup grp_vbox_hdd     VBox HDD Container
 * @{
 */

/** Image info, not handled anyhow.
 *  Must be less than 64 bytes in length, including the trailing 0.
 */
#define VDI_IMAGE_FILE_INFO   "<<< Sun xVM VirtualBox Disk Image >>>\n"

/** Current image major version. */
#define VDI_IMAGE_VERSION_MAJOR    (0x0001)
/** Current image minor version. */
#define VDI_IMAGE_VERSION_MINOR    (0x0001)
/** Current image version. */
#define VDI_IMAGE_VERSION          ((VDI_IMAGE_VERSION_MAJOR << 16) | VDI_IMAGE_VERSION_MINOR)

/** Get major version from combined version. */
#define VDI_GET_VERSION_MAJOR(uVer)    ((uVer) >> 16)
/** Get minor version from combined version. */
#define VDI_GET_VERSION_MINOR(uVer)    ((uVer) & 0xffff)

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

/** @name VDI image flags
 * @{  */
/** No flags. */
#define VDI_IMAGE_FLAGS_NONE          (0x00)
/** Fill new blocks with zeroes while expanding image file. */
#define VDI_IMAGE_FLAGS_ZERO_EXPAND   (0x01)

/** Mask of valid image flags. */
#define VDI_IMAGE_FLAGS_MASK          (VDI_IMAGE_FLAGS_NONE | VDI_IMAGE_FLAGS_ZERO_EXPAND)

/** Default image flags. */
#define VDI_IMAGE_FLAGS_DEFAULT       (VDI_IMAGE_FLAGS_NONE)
/** @} */

/** @name VDI image open mode flags
 * @{
 */
/** Try to open image in read/write exclusive access mode if possible, or in read-only elsewhere. */
#define VDI_OPEN_FLAGS_NORMAL     (0)
/** Open image in read-only mode with sharing access with others. */
#define VDI_OPEN_FLAGS_READONLY   (1)
/** Mask of valid flags. */
#define VDI_OPEN_FLAGS_MASK (VDI_OPEN_FLAGS_NORMAL | VDI_OPEN_FLAGS_READONLY)
/** @}*/

/**
 * VBox VDI disk Container main structure.
 */
/* Forward declaration, VDIDISK structure is visible only inside VDI module. */
struct VDIDISK;
typedef struct VDIDISK VDIDISK;
typedef VDIDISK *PVDIDISK;

/**
 * Creates a new base image file.
 *
 * @returns VBox status code.
 * @param   pszFilename     Name of the image file to create.
 * @param   enmType         Image type, only base image types are acceptable.
 * @param   cbSize          Image size in bytes.
 * @param   pszComment      Pointer to image comment. NULL is ok.
 * @param   pfnProgress     Progress callback. Optional. NULL if not to be used.
 * @param   pvUser          User argument for the progress callback.
 */
VBOXDDU_DECL(int) VDICreateBaseImage(const char *pszFilename, VDIIMAGETYPE enmType, uint64_t cbSize, const char *pszComment,
                                     PFNVMPROGRESS pfnProgress, void *pvUser);

/**
 * Creates a differencing dynamically growing image file for specified parent image.
 *
 * @returns VBox status code.
 * @param   pszFilename     Name of the differencing image file to create.
 * @param   pszParent       Name of the parent image file. May be base or diff image type.
 * @param   pszComment      Pointer to image comment. NULL is ok.
 * @param   pfnProgress     Progress callback. Optional. NULL if not to be used.
 * @param   pvUser          User argument for the progress callback.
 */
VBOXDDU_DECL(int) VDICreateDifferenceImage(const char *pszFilename, const char *pszParent, const char *pszComment,
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
VBOXDDU_DECL(int) VDICheckImage(const char *pszFilename,
                                unsigned *puVersion,
                                PVDIIMAGETYPE penmType,
                                uint64_t *pcbSize,
                                PRTUUID pUuid,
                                PRTUUID pParentUuid,
                                char *pszComment,
                                unsigned cbComment);

/**
 * Changes an image's comment string.
 *
 * @returns VBox status code.
 * @param   pszFilename     Name of the image file to operate on.
 * @param   pszComment      New comment string (UTF-8). NULL is allowed to reset the comment.
 */
VBOXDDU_DECL(int) VDISetImageComment(const char *pszFilename, const char *pszComment);

/**
 * Deletes a valid image file. Fails if specified file is not an image.
 *
 * @returns VBox status code.
 * @param   pszFilename     Name of the image file to check.
 */
VBOXDDU_DECL(int) VDIDeleteImage(const char *pszFilename);

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
VBOXDDU_DECL(int) VDICopyImage(const char *pszDstFilename, const char *pszSrcFilename, const char *pszComment,
                               PFNVMPROGRESS pfnProgress, void *pvUser);

/**
 * Converts image file from older VDI formats to current one.
 *
 * @returns VBox status code.
 * @param   pszFilename     Name of the image file to convert.
 * @param   pfnProgress     Progress callback. Optional. NULL if not to be used.
 * @param   pvUser          User argument for the progress callback.
 */
VBOXDDU_DECL(int) VDIConvertImage(const char *pszFilename, PFNVMPROGRESS pfnProgress, void *pvUser);

/**
 * Shrinks growing image file by removing zeroed data blocks.
 *
 * @returns VBox status code.
 * @param   pszFilename     Name of the image file to shrink.
 * @param   pfnProgress     Progress callback. Optional. NULL if not to be used.
 * @param   pvUser          User argument for the progress callback.
 */
VBOXDDU_DECL(int) VDIShrinkImage(const char *pszFilename, PFNVMPROGRESS pfnProgress, void *pvUser);

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
VBOXDDU_DECL(int) VDIGetImageUUIDs(const char *pszFilename,
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
VBOXDDU_DECL(int) VDISetImageUUIDs(const char *pszFilename,
                                   PCRTUUID pUuid, PCRTUUID pModificationUuid,
                                   PCRTUUID pParentUuid, PCRTUUID pParentModificationUuid);

/**
 * Merges two images having a parent/child relationship (both directions).
 *
 * @returns VBox status code.
 * @param   pszFilenameFrom         Name of the image file to merge from.
 * @param   pszFilenameTo           Name of the image file to merge into.
 * @param   pfnProgress     Progress callback. Optional. NULL if not to be used.
 * @param   pvUser          User argument for the progress callback.
 */
VBOXDDU_DECL(int) VDIMergeImage(const char *pszFilenameFrom, const char *pszFilenameTo,
                                PFNVMPROGRESS pfnProgress, void *pvUser);


/**
 * Allocates and initializes an empty VDI HDD container.
 * No image files are opened.
 *
 * @returns Pointer to newly created empty HDD container.
 * @returns NULL on failure, typically out of memory.
 */
VBOXDDU_DECL(PVDIDISK) VDIDiskCreate(void);

/**
 * Destroys the VDI HDD container. If container has opened image files they will be closed.
 *
 * @param   pDisk           Pointer to VDI HDD container.
 */
VBOXDDU_DECL(void) VDIDiskDestroy(PVDIDISK pDisk);

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
 * Use VDIDiskIsReadOnly to check open mode.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to VDI HDD container.
 * @param   pszFilename     Name of the image file to open.
 * @param   fOpen           Image file open mode, see VDI_OPEN_FLAGS_* constants.
 */
VBOXDDU_DECL(int) VDIDiskOpenImage(PVDIDISK pDisk, const char *pszFilename, unsigned fOpen);

/**
 * Creates and opens a new differencing image file in HDD container.
 * See comments for VDIDiskOpenImage function about differencing images.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to VDI HDD container.
 * @param   pszFilename     Name of the image file to create and open.
 * @param   pszComment      Pointer to image comment. NULL is ok.
 * @param   pfnProgress     Progress callback. Optional. NULL if not to be used.
 * @param   pvUser          User argument for the progress callback.
 */
VBOXDDU_DECL(int) VDIDiskCreateOpenDifferenceImage(PVDIDISK pDisk, const char *pszFilename, const char *pszComment,
                                                   PFNVMPROGRESS pfnProgress, void *pvUser);

/**
 * Closes the last opened image file in the HDD container. Leaves all changes inside it.
 * If previous image file was opened in read-only mode (that is normal) and closing image
 * was opened in read-write mode (the whole disk was in read-write mode) - the previous image
 * will be reopened in read/write mode.
 *
 * @param   pDisk           Pointer to VDI HDD container.
 */
VBOXDDU_DECL(void) VDIDiskCloseImage(PVDIDISK pDisk);

/**
 * Closes all opened image files in HDD container.
 *
 * @param   pDisk           Pointer to VDI HDD container.
 */
VBOXDDU_DECL(void) VDIDiskCloseAllImages(PVDIDISK pDisk);

/**
 * Commits last opened differencing/undo image file of the HDD container to previous image.
 * If the previous image file was opened in read-only mode (that must be always so) it is reopened
 * as read/write to do commit operation.
 * After successfull commit the previous image file again reopened in read-only mode, last opened
 * image file is cleared of data and remains open and active in HDD container.
 * If you want to delete image after commit you must do it manually by VDIDiskCloseImage and
 * VDIDeleteImage calls.
 *
 * Note that in case of unrecoverable error all images of HDD container will be closed.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to VDI HDD container.
 * @param   pfnProgress     Progress callback. Optional.
 * @param   pvUser          User argument for the progress callback.
 */
VBOXDDU_DECL(int) VDIDiskCommitLastDiff(PVDIDISK pDisk, PFNVMPROGRESS pfnProgress, void *pvUser);

/**
 * Get read/write mode of VDI HDD.
 *
 * @returns Disk ReadOnly status.
 * @returns true if no one VDI image is opened in HDD container.
 */
VBOXDDU_DECL(bool) VDIDiskIsReadOnly(PVDIDISK pDisk);

/**
 * Get total disk size of the VDI HDD container.
 *
 * @returns Virtual disk size in bytes.
 * @returns 0 if no one VDI image is opened in HDD container.
 */
VBOXDDU_DECL(uint64_t) VDIDiskGetSize(PVDIDISK pDisk);

/**
 * Get block size of the VDI HDD container.
 *
 * @returns VDI image block size in bytes.
 * @returns 0 if no one VDI image is opened in HDD container.
 */
VBOXDDU_DECL(unsigned) VDIDiskGetBlockSize(PVDIDISK pDisk);

/**
 * Get working buffer size of the VDI HDD container.
 *
 * @returns Working buffer size in bytes.
 */
VBOXDDU_DECL(unsigned) VDIDiskGetBufferSize(PVDIDISK pDisk);

/**
 * Get virtual disk PCHS geometry stored in image file.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_NOT_OPENED if no one VDI image is opened in HDD container.
 * @returns VERR_VDI_GEOMETRY_NOT_SET if no geometry present in the HDD container.
 * @param   pDisk           Pointer to VDI HDD container.
 * @param   pPCHSGeometry   Where to store PCHS geometry. Not NULL.
 */
VBOXDDU_DECL(int) VDIDiskGetPCHSGeometry(PVDIDISK pDisk, PPDMMEDIAGEOMETRY pPCHSGeometry);

/**
 * Store virtual disk PCHS geometry into base image file of HDD container.
 *
 * Note that in case of unrecoverable error all images of HDD container will be closed.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_NOT_OPENED if no one VDI image is opened in HDD container.
 * @param   pDisk           Pointer to VDI HDD container.
 * @param   pPCHSGeometry   Where to store LCHS geometry. Not NULL.
 */
VBOXDDU_DECL(int) VDIDiskSetPCHSGeometry(PVDIDISK pDisk, PCPDMMEDIAGEOMETRY pPCHSGeometry);

/**
 * Get virtual disk LCHS geometry stored in image file.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_NOT_OPENED if no one VDI image is opened in HDD container.
 * @returns VERR_VDI_GEOMETRY_NOT_SET if no geometry present in the HDD container.
 * @param   pDisk           Pointer to VDI HDD container.
 * @param   pLCHSGeometry   Where to store LCHS geometry. Not NULL.
 */
VBOXDDU_DECL(int) VDIDiskGetLCHSGeometry(PVDIDISK pDisk, PPDMMEDIAGEOMETRY pLCHSGeometry);

/**
 * Store virtual disk LCHS geometry into base image file of HDD container.
 *
 * Note that in case of unrecoverable error all images of HDD container will be closed.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_NOT_OPENED if no one VDI image is opened in HDD container.
 * @param   pDisk           Pointer to VDI HDD container.
 * @param   pLCHSGeometry   Where to store LCHS geometry. Not NULL.
 */
VBOXDDU_DECL(int) VDIDiskSetLCHSGeometry(PVDIDISK pDisk, PCPDMMEDIAGEOMETRY pLCHSGeometry);

/**
 * Get number of opened images in HDD container.
 *
 * @returns Number of opened images for HDD container. 0 if no images has been opened.
 * @param   pDisk           Pointer to VDI HDD container.
 */
VBOXDDU_DECL(int) VDIDiskGetImagesCount(PVDIDISK pDisk);

/**
 * Get version of opened image of HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to VDI HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   puVersion       Where to store the image version.
 */
VBOXDDU_DECL(int) VDIDiskGetImageVersion(PVDIDISK pDisk, int nImage, unsigned *puVersion);

/**
 * Get type of opened image of HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to VDI HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   penmType        Where to store the image type.
 */
VBOXDDU_DECL(int) VDIDiskGetImageType(PVDIDISK pDisk, int nImage, PVDIIMAGETYPE penmType);

/**
 * Get flags of opened image of HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to VDI HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pfFlags         Where to store the image flags.
 */
VBOXDDU_DECL(int) VDIDiskGetImageFlags(PVDIDISK pDisk, int nImage, unsigned *pfFlags);

/**
 * Get filename of opened image of HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @returns VERR_BUFFER_OVERFLOW if pszFilename buffer too small to hold filename.
 * @param   pDisk           Pointer to VDI HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pszFilename     Where to store the image file name.
 * @param   cbFilename      Size of buffer pszFilename points to.
 */
VBOXDDU_DECL(int) VDIDiskGetImageFilename(PVDIDISK pDisk, int nImage, char *pszFilename, unsigned cbFilename);

/**
 * Get the comment line of opened image of HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @returns VERR_BUFFER_OVERFLOW if pszComment buffer too small to hold comment text.
 * @param   pDisk           Pointer to VDI HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pszComment      Where to store the comment string of image. NULL is ok.
 * @param   cbComment       The size of pszComment buffer. 0 is ok.
 */
VBOXDDU_DECL(int) VDIDiskGetImageComment(PVDIDISK pDisk, int nImage, char *pszComment, unsigned cbComment);

/**
 * Get Uuid of opened image of HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to VDI HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pUuid           Where to store the image creation uuid.
 */
VBOXDDU_DECL(int) VDIDiskGetImageUuid(PVDIDISK pDisk, int nImage, PRTUUID pUuid);

/**
 * Get last modification Uuid of opened image of HDD container.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to VDI HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pUuid           Where to store the image modification uuid.
 */
VBOXDDU_DECL(int) VDIDiskGetImageModificationUuid(PVDIDISK pDisk, int nImage, PRTUUID pUuid);

/**
 * Get Uuid of opened image's parent image.
 *
 * @returns VBox status code.
 * @returns VERR_VDI_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to VDI HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of the container.
 * @param   pUuid           Where to store the image creation uuid.
 */
VBOXDDU_DECL(int) VDIDiskGetParentImageUuid(PVDIDISK pDisk, int nImage, PRTUUID pUuid);

/**
 * Read data from virtual HDD.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to VDI HDD container.
 * @param   offStart        Offset of first reading byte from start of disk.
 * @param   pvBuf           Pointer to buffer for reading data.
 * @param   cbToRead        Number of bytes to read.
 */
VBOXDDU_DECL(int) VDIDiskRead(PVDIDISK pDisk, uint64_t offStart, void *pvBuf, size_t cbToRead);

/**
 * Write data to virtual HDD.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to VDI HDD container.
 * @param   offStart        Offset of first writing byte from start of HDD.
 * @param   pvBuf           Pointer to buffer of writing data.
 * @param   cbToWrite       Number of bytes to write.
 */
VBOXDDU_DECL(int) VDIDiskWrite(PVDIDISK pDisk, uint64_t offStart, const void *pvBuf, size_t cbToWrite);



/**
 * Debug helper - dumps all opened images of HDD container into the log file.
 *
 * @param   pDisk           Pointer to VDI HDD container.
 */
VBOXDDU_DECL(void) VDIDiskDumpImages(PVDIDISK pDisk);

__END_DECLS

/** @} */

#endif
