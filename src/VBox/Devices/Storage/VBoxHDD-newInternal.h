/** @file
 * Internal header file for VBox HDD Container.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __VBoxHDD_newInternal_h__


#include <VBox/pdm.h>
#include <VBox/VBoxHDD-new.h>

/**
 * Image format backend interface used by VBox HDD Container implementation.
 */
typedef struct VBOXHDDBACKEND
{
    /**
     * The size of the structure.
     */
    uint32_t cbSize;

    /**
     * Check if an file is valid for the backend.
     *
     * @returns VBox status code.
     * @param   pszFilename     Name of the image file to open. Guaranteed to be available and
     *                          unchanged during the lifetime of this image.
     */
    DECLR3CALLBACKMEMBER(int, pfnCheckIfValid, (const char *pszFilename));

    /**
     * Open a disk image.
     *
     * @returns VBox status code.
     * @param   pszFilename     Name of the image file to open. Guaranteed to be available and
     *                          unchanged during the lifetime of this image.
     * @param   uOpenFlags      Image file open mode, see VD_OPEN_FLAGS_* constants.
     * @param   pfnError        Callback for setting extended error information.
     * @param   pvErrorUser     Opaque parameter for pfnError.
     * @param   ppvBackendData  Opaque state data for this image.
     */
    DECLR3CALLBACKMEMBER(int, pfnOpen, (const char *pszFilename, unsigned uOpenFlags, PFNVDERROR pfnError, void *pvErrorUser, void **ppvBackendData));

    /**
     * Create a disk image.
     *
     * @returns VBox status code.
     * @param   pszFilename     Name of the image file to create. Guaranteed to be available and
     *                          unchanged during the lifetime of this image.
     * @param   enmType         Image type. Both base and diff image types are valid.
     * @param   cbSize          Image size in bytes.
     * @param   uImageFlags     Flags specifying special image features.
     * @param   pszComment      Pointer to image comment. NULL is ok.
     * @param   cCylinders      Number of cylinders (must be <= 16383).
     * @param   cHeads          Number of heads (must be <= 16).
     * @param   cSectors        Number of sectors (must be <= 63).
     * @param   uOpenFlags      Image file open mode, see VD_OPEN_FLAGS_* constants.
     * @param   pfnProgress     Progress callback. Optional. NULL if not to be used.
     * @param   pvUser          User argument for the progress callback.
     * @param   pfnError        Callback for setting extended error information.
     * @param   pvErrorUser     Opaque parameter for pfnError.
     * @param   ppvBackendData  Opaque state data for this image.
     */
    DECLR3CALLBACKMEMBER(int, pfnCreate, (const char *pszFilename, VDIMAGETYPE enmType, uint64_t cbSize, unsigned uImageFlags, const char *pszComment, uint32_t cCylinders, uint32_t cHeads, uint32_t cSectors, unsigned uOpenFlags, PFNVMPROGRESS pfnProgress, void *pvUser, PFNVDERROR pfnError, void *pvErrorUser, void **ppvBackendData));

    /**
     * Close a disk image.
     *
     * @returns VBox status code.
     * @param   pvBackendData   Opaque state data for this image.
     * @param   fDelete         If true, delete the image from the host disk.
     */
    DECLR3CALLBACKMEMBER(int, pfnClose, (void *pvBackendData, bool fDelete));

    /**
     * Read data from a disk image. The area read never crosses a block
     * boundary.
     *
     * @returns VBox status code.
     * @returns VINF_VDI_BLOCK_FREE if this image contains no data for this block.
     * @param   pvBackendData   Opaque state data for this image.
     * @param   off             Offset to start reading from.
     * @param   pvBuf           Where to store the read bits.
     * @param   cbRead          Number of bytes to read.
     * @param   pcbActuallyRead Pointer to returned number of bytes read.
     */
    DECLR3CALLBACKMEMBER(int, pfnRead, (void *pvBackendData, uint64_t off, void *pvBuf, size_t cbRead, size_t *pcbActuallyRead));

    /**
     * Write data to a disk image. The area written never crosses a block
     * boundary.
     *
     * @returns VBox status code.
     * @returns VINF_VDI_BLOCK_FREE if this image contains no data for this block and
     *          this is not a full-block write. The write must be repeated with
     *          the correct amount of prefix/postfix data read from the images below
     *          in the image stack. This might not be the most convenient interface,
     *          but it works with arbitrary block sizes, especially when the image
     *          stack uses different block sizes.
     * @param   pvBackendData   Opaque state data for this image.
     * @param   off             Offset to start writing to.
     * @param   pvBuf           Where to retrieve the written bits.
     * @param   cbWrite         Number of bytes to write.
     * @param   pcbWriteProcess Pointer to returned number of bytes that could
     *                          be processed. In case the function returned
     *                          VINF_VDI_BLOCK_FREE this is the number of bytes
     *                          that could be written in a full block write,
     *                          when prefixed/postfixed by the appropriate
     *                          amount of (previously read) padding data.
     * @param   pcbPreRead      Pointer to the returned amount of data that must
     *                          be prefixed to perform a full block write.
     * @param   pcbPostRead     Pointer to the returned amount of data that must
     *                          be postfixed to perform a full block write.
     */
    DECLR3CALLBACKMEMBER(int, pfnWrite, (void *pvBackendData, uint64_t off, const void *pvBuf, size_t cbWrite, size_t *pcbWriteProcess, size_t *pcbPreRead, size_t *pcbPostRead));

    /**
     * Flush data to disk.
     *
     * @returns VBox status code.
     * @param   pvBackendData   Opaque state data for this image.
     */
    DECLR3CALLBACKMEMBER(int, pfnFlush, (void *pvBackendData));

    /**
     * Get the type information for a disk image.
     *
     * @returns VBox status code.
     * @param   pvBackendData   Opaque state data for this image.
     * @param   penmType        Image type of this image.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetImageType, (void *pvBackendData, PVDIMAGETYPE penmType));

    /**
     * Get the size of a disk image.
     *
     * @returns size of disk image.
     * @param   pvBackendData   Opaque state data for this image.
     */
    DECLR3CALLBACKMEMBER(uint64_t, pfnGetSize, (void *pvBackendData));

    /**
     * Get virtual disk geometry stored in a disk image.
     *
     * @returns VBox status code.
     * @returns VERR_VDI_GEOMETRY_NOT_SET if no geometry present in the image.
     * @param   pvBackendData   Opaque state data for this image.
     * @param   pcCylinders     Where to store the number of cylinders. Never NULL.
     * @param   pcHeads         Where to store the number of heads. Never NULL.
     * @param   pcSectors       Where to store the number of sectors. Never NULL.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetGeometry, (void *pvBackendData, unsigned *pcCylinders, unsigned *pcHeads, unsigned *pcSectors));

    /**
     * Set virtual disk geometry stored in a disk image.
     * Only called if geometry is different than before.
     *
     * @returns VBox status code.
     * @param   pvBackendData   Opaque state data for this image.
     * @param   cCylinders      Number of cylinders.
     * @param   cHeads          Number of heads.
     * @param   cSectors        Number of sectors.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetGeometry, (void *pvBackendData, unsigned cCylinders, unsigned cHeads, unsigned cSectors));

    /**
     * Get virtual disk translation mode stored in a disk image.
     *
     * @returns VBox status code.
     * @returns VERR_VDI_GEOMETRY_NOT_SET if no geometry present in the image.
     * @param   pvBackendData   Opaque state data for this image.
     * @param   penmTranslation Where to store the translation mode. Never NULL.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetTranslation, (void *pvBackendData, PPDMBIOSTRANSLATION penmTranslation));

    /**
     * Set virtual disk translation mode stored in a disk image.
     * Only called if translation mode is different than before.
     *
     * @returns VBox status code.
     * @param   pvBackendData   Opaque state data for this image.
     * @param   enmTranslation  New translation mode.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetTranslation, (void *pvBackendData, PDMBIOSTRANSLATION enmTranslation));

    /**
     * Get the open flags of a disk image.
     *
     * @returns open flags of disk image.
     * @param   pvBackendData   Opaque state data for this image.
     */
    DECLR3CALLBACKMEMBER(unsigned, pfnGetOpenFlags, (void *pvBackendData));

    /**
     * Set the open flags of a disk image. May cause the image to be locked
     * in a different mode or be reopened (which can fail).
     *
     * @returns VBox status code.
     * @param   pvBackendData   Opaque state data for this image.
     * @param   uOpenFlags      New open flags for this image.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetOpenFlags, (void *pvBackendData, unsigned uOpenFlags));

    /**
     * Get comment of a disk image.
     *
     * @returns VBox status code.
     * @param   pvBackendData   Opaque state data for this image.
     * @param   pszComment      Where to store the comment.
     * @param   cbComment       Size of the comment buffer.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetComment, (void *pvBackendData, char *pszComment, size_t cbComment));

    /**
     * Set comment of a disk image.
     *
     * @returns VBox status code.
     * @param   pvBackendData   Opaque state data for this image.
     * @param   pszComment      Where to get the comment from. NULL resets comment.
     *                          The comment is silently truncated if the image format
     *                          limit is exceeded.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetComment, (void *pvBackendData, const char *pszComment));

    /**
     * Get UUID of a disk image.
     *
     * @returns VBox status code.
     * @param   pvBackendData   Opaque state data for this image.
     * @param   pUuid           Where to store the image UUID.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetUuid, (void *pvBackendData, PRTUUID pUuid));

    /**
     * Set UUID of a disk image.
     *
     * @returns VBox status code.
     * @param   pvBackendData   Opaque state data for this image.
     * @param   pUuid           Where to get the image UUID from.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetUuid, (void *pvBackendData, PCRTUUID pUuid));

    /**
     * Get last modification UUID of a disk image.
     *
     * @returns VBox status code.
     * @param   pvBackendData   Opaque state data for this image.
     * @param   pUuid           Where to store the image modification UUID.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetModificationUuid, (void *pvBackendData, PRTUUID pUuid));

    /**
     * Set last modification UUID of a disk image.
     *
     * @returns VBox status code.
     * @param   pvBackendData   Opaque state data for this image.
     * @param   pUuid           Where to get the image modification UUID from.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetModificationUuid, (void *pvBackendData, PCRTUUID pUuid));

    /**
     * Get parent UUID of a disk image.
     *
     * @returns VBox status code.
     * @param   pvBackendData   Opaque state data for this image.
     * @param   pUuid           Where to store the parent image UUID.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetParentUuid, (void *pvBackendData, PRTUUID pUuid));

    /**
     * Set parent UUID of a disk image.
     *
     * @returns VBox status code.
     * @param   pvBackendData   Opaque state data for this image.
     * @param   pUuid           Where to get the parent image UUID from.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetParentUuid, (void *pvBackendData, PCRTUUID pUuid));

} VBOXHDDBACKEND, *PVBOXHDDBACKEND;

/** Initialization entry point. */
typedef DECLCALLBACK(int) VBOXHDDFORMATLOAD(PVBOXHDDBACKEND *ppBackendTable);
typedef VBOXHDDFORMATLOAD *PFNVBOXHDDFORMATLOAD;
#define VBOX_HDDFORMAT_LOAD_NAME "VBoxHDDFormatLoad"

/** The prefix to identify Storage Plugins. */
#define VBOX_HDDFORMAT_PLUGIN_PREFIX "VBoxHDD"
/** The size of the prefix excluding the '\0' terminator. */
#define VBOX_HDDFORMAT_PLUGIN_PREFIX_LENGTH (sizeof(VBOX_HDDFORMAT_PLUGIN_PREFIX)-1)

#endif
