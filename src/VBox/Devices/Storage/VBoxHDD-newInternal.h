/** @file
 * Internal header file for VBox HDD Container.
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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

#ifndef __VBoxHDD_newInternal_h__


#include <VBox/pdm.h>
#include <VBox/VBoxHDD-new.h>


/** @name VBox HDD backend write flags
 * @{
 */
/** Do not allocate a new block on this write. This is just an advisory
 * flag. The backend may still decide in some circumstances that it wants
 * to ignore this flag (which may cause extra dynamic image expansion). */
#define VD_WRITE_NO_ALLOC   RT_BIT(1)
/** @}*/


/**
 * Image format backend interface used by VBox HDD Container implementation.
 */
typedef struct VBOXHDDBACKEND
{
    /**
     * The name of the backend (constant string).
     */
    const char *pszBackendName;

    /**
     * The size of the structure.
     */
    uint32_t cbSize;

    /**
     * The capabilities of the backend.
     */
    uint64_t uBackendCaps;

    /**
     * Check if a file is valid for the backend.
     *
     * @returns VBox status code.
     * @param   pszFilename     Name of the image file.
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
     * @param   pPCHSGeometry   Physical drive geometry CHS <= (16383,16,255).
     * @param   pLCHSGeometry   Logical drive geometry CHS <= (1024,255,63).
     * @param   uOpenFlags      Image file open mode, see VD_OPEN_FLAGS_* constants.
     * @param   pfnProgress     Progress callback. Optional. NULL if not to be used.
     * @param   pvUser          User argument for the progress callback.
     * @param   uPercentStart   Starting value for progress percentage.
     * @param   uPercentSpan    Span for varying progress percentage.
     * @param   pfnError        Callback for setting extended error information.
     * @param   pvErrorUser     Opaque parameter for pfnError.
     * @param   ppvBackendData  Opaque state data for this image.
     */
    DECLR3CALLBACKMEMBER(int, pfnCreate, (const char *pszFilename, VDIMAGETYPE enmType, uint64_t cbSize, unsigned uImageFlags, const char *pszComment, PCPDMMEDIAGEOMETRY pPCHSGeometry, PCPDMMEDIAGEOMETRY pLCHSGeometry, unsigned uOpenFlags, PFNVMPROGRESS pfnProgress, void *pvUser, unsigned uPercentStart, unsigned uPercentSpan, PFNVDERROR pfnError, void *pvErrorUser, void **ppvBackendData));

    /**
     * Rename a disk image. Only needs to work as long as the operating
     * system's rename file functionality is usable. If an attempt is made to
     * rename an image to a location on another disk/filesystem, this function
     * may just fail with an appropriate error code (not changing the opened
     * image data at all). Also works only on images which actually refer to
     * files (and not for raw disk images).
     *
     * @returns VBox status code.
     * @param   pvBackendData   Opaque state data for this image.
     * @param   pszFilename     New name of the image file. Guaranteed to be available and
     *                          unchanged during the lifetime of this image.
     */
    DECLR3CALLBACKMEMBER(int, pfnRename, (void *pvBackendData, const char *pszFilename));

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
     * @returns VINF_VDI_BLOCK_ZERO if this image contains a zero data block.
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
     * @param   fWrite          Flags which affect write behavior. Combination
     *                          of the VD_WRITE_* flags.
     */
    DECLR3CALLBACKMEMBER(int, pfnWrite, (void *pvBackendData, uint64_t off, const void *pvBuf, size_t cbWrite, size_t *pcbWriteProcess, size_t *pcbPreRead, size_t *pcbPostRead, unsigned fWrite));

    /**
     * Flush data to disk.
     *
     * @returns VBox status code.
     * @param   pvBackendData   Opaque state data for this image.
     */
    DECLR3CALLBACKMEMBER(int, pfnFlush, (void *pvBackendData));

    /**
     * Get the version of a disk image.
     *
     * @returns version of disk image.
     * @param   pvBackendData   Opaque state data for this image.
     */
    DECLR3CALLBACKMEMBER(unsigned, pfnGetVersion, (void *pvBackendData));

    /**
     * Get the type information for a disk image.
     *
     * @returns VBox status code.
     * @param   pvBackendData   Opaque state data for this image.
     * @param   penmType        Image type of this image.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetImageType, (void *pvBackendData, PVDIMAGETYPE penmType));

    /**
     * Get the capacity of a disk image.
     *
     * @returns size of disk image in bytes.
     * @param   pvBackendData   Opaque state data for this image.
     */
    DECLR3CALLBACKMEMBER(uint64_t, pfnGetSize, (void *pvBackendData));

    /**
     * Get the file size of a disk image.
     *
     * @returns size of disk image in bytes.
     * @param   pvBackendData   Opaque state data for this image.
     */
    DECLR3CALLBACKMEMBER(uint64_t, pfnGetFileSize, (void *pvBackendData));

    /**
     * Get virtual disk PCHS geometry stored in a disk image.
     *
     * @returns VBox status code.
     * @returns VERR_VDI_GEOMETRY_NOT_SET if no geometry present in the image.
     * @param   pvBackendData   Opaque state data for this image.
     * @param   pPCHSGeometry   Where to store the geometry. Not NULL.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetPCHSGeometry, (void *pvBackendData, PPDMMEDIAGEOMETRY pPCHSGeometry));

    /**
     * Set virtual disk PCHS geometry stored in a disk image.
     * Only called if geometry is different than before.
     *
     * @returns VBox status code.
     * @param   pvBackendData   Opaque state data for this image.
     * @param   pPCHSGeometry   Where to load the geometry from. Not NULL.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetPCHSGeometry, (void *pvBackendData, PCPDMMEDIAGEOMETRY pPCHSGeometry));

    /**
     * Get virtual disk LCHS geometry stored in a disk image.
     *
     * @returns VBox status code.
     * @returns VERR_VDI_GEOMETRY_NOT_SET if no geometry present in the image.
     * @param   pvBackendData   Opaque state data for this image.
     * @param   pLCHSGeometry   Where to store the geometry. Not NULL.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetLCHSGeometry, (void *pvBackendData, PPDMMEDIAGEOMETRY pLCHSGeometry));

    /**
     * Set virtual disk LCHS geometry stored in a disk image.
     * Only called if geometry is different than before.
     *
     * @returns VBox status code.
     * @param   pvBackendData   Opaque state data for this image.
     * @param   pLCHSGeometry   Where to load the geometry from. Not NULL.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetLCHSGeometry, (void *pvBackendData, PCPDMMEDIAGEOMETRY pLCHSGeometry));

    /**
     * Get the image flags of a disk image.
     *
     * @returns image flags of disk image.
     * @param   pvBackendData   Opaque state data for this image.
     */
    DECLR3CALLBACKMEMBER(unsigned, pfnGetImageFlags, (void *pvBackendData));

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

    /**
     * Get parent modification UUID of a disk image.
     *
     * @returns VBox status code.
     * @param   pvBackendData   Opaque state data for this image.
     * @param   pUuid           Where to store the parent image modification UUID.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetParentModificationUuid, (void *pvBackendData, PRTUUID pUuid));

    /**
     * Set parent modification UUID of a disk image.
     *
     * @returns VBox status code.
     * @param   pvBackendData   Opaque state data for this image.
     * @param   pUuid           Where to get the parent image modification UUID from.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetParentModificationUuid, (void *pvBackendData, PCRTUUID pUuid));

    /**
     * Dump information about a disk image.
     *
     * @param   pvBackendData   Opaque state data for this image.
     */
    DECLR3CALLBACKMEMBER(void, pfnDump, (void *pvBackendData));

} VBOXHDDBACKEND;

/** Pointer to VD backend. */
typedef VBOXHDDBACKEND *PVBOXHDDBACKEND;

/** Constant pointer to VD backend. */
typedef const VBOXHDDBACKEND *PCVBOXHDDBACKEND;

/** Initialization entry point. */
typedef DECLCALLBACK(int) VBOXHDDFORMATLOAD(PVBOXHDDBACKEND *ppBackendTable);
typedef VBOXHDDFORMATLOAD *PFNVBOXHDDFORMATLOAD;
#define VBOX_HDDFORMAT_LOAD_NAME "VBoxHDDFormatLoad"

/** The prefix to identify Storage Plugins. */
#define VBOX_HDDFORMAT_PLUGIN_PREFIX "VBoxHDD"
/** The size of the prefix excluding the '\0' terminator. */
#define VBOX_HDDFORMAT_PLUGIN_PREFIX_LENGTH (sizeof(VBOX_HDDFORMAT_PLUGIN_PREFIX)-1)

#endif
