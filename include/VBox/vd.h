/** @file
 * VBox HDD Container API.
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

#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/mem.h>
#include <iprt/file.h>
#include <iprt/net.h>
#include <iprt/sg.h>
#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/err.h>

RT_C_DECLS_BEGIN

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

/** Placeholder for VDCopyEx to indicate that the image content is unknown. */
#define VD_IMAGE_CONTENT_UNKNOWN    0xffffffffU

/** @name VBox HDD container image flags
 * @{
 */
/** No flags. */
#define VD_IMAGE_FLAGS_NONE                     (0)
/** Fixed image. */
#define VD_IMAGE_FLAGS_FIXED                    (0x10000)
/** Diff image. Mutually exclusive with fixed image. */
#define VD_IMAGE_FLAGS_DIFF                     (0x20000)
/** VMDK: Split image into 2GB extents. */
#define VD_VMDK_IMAGE_FLAGS_SPLIT_2G            (0x0001)
/** VMDK: Raw disk image (giving access to a number of host partitions). */
#define VD_VMDK_IMAGE_FLAGS_RAWDISK             (0x0002)
/** VMDK: stream optimized image, read only. */
#define VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED    (0x0004)
/** VMDK: ESX variant, use in addition to other flags. */
#define VD_VMDK_IMAGE_FLAGS_ESX                 (0x0008)
/** VDI: Fill new blocks with zeroes while expanding image file. Only valid
 * for newly created images, never set for opened existing images. */
#define VD_VDI_IMAGE_FLAGS_ZERO_EXPAND          (0x0100)

/** Mask of valid image flags for VMDK. */
#define VD_VMDK_IMAGE_FLAGS_MASK            (   VD_IMAGE_FLAGS_FIXED | VD_IMAGE_FLAGS_DIFF | VD_IMAGE_FLAGS_NONE \
                                             |  VD_VMDK_IMAGE_FLAGS_SPLIT_2G | VD_VMDK_IMAGE_FLAGS_RAWDISK \
                                             | VD_VMDK_IMAGE_FLAGS_STREAM_OPTIMIZED | VD_VMDK_IMAGE_FLAGS_ESX)

/** Mask of valid image flags for VDI. */
#define VD_VDI_IMAGE_FLAGS_MASK             (VD_IMAGE_FLAGS_FIXED | VD_IMAGE_FLAGS_DIFF | VD_IMAGE_FLAGS_NONE | VD_VDI_IMAGE_FLAGS_ZERO_EXPAND)

/** Mask of all valid image flags for all formats. */
#define VD_IMAGE_FLAGS_MASK                 (VD_VMDK_IMAGE_FLAGS_MASK | VD_VDI_IMAGE_FLAGS_MASK)

/** Default image flags. */
#define VD_IMAGE_FLAGS_DEFAULT              (VD_IMAGE_FLAGS_NONE)
/** @} */


/**
 * Auxiliary type for describing partitions on raw disks. The entries must be
 * in ascending order (as far as uStart is concerned), and must not overlap.
 * Note that this does not correspond 1:1 to partitions, it is describing the
 * general meaning of contiguous areas on the disk.
 */
typedef struct VBOXHDDRAWPARTDESC
{
    /** Device to use for this partition/data area. Can be the disk device if
     * the offset field is set appropriately. If this is NULL, then this
     * partition will not be accessible to the guest. The size of the data area
     * must still be set correctly. */
    const char      *pszRawDevice;
    /** Pointer to the partitioning info. NULL means this is a regular data
     * area on disk, non-NULL denotes data which should be copied to the
     * partition data overlay. */
    const void      *pvPartitionData;
    /** Offset where the data starts in this device. */
    uint64_t        uStartOffset;
    /** Offset where the data starts in the disk. */
    uint64_t        uStart;
    /** Size of the data area. */
    uint64_t        cbData;
} VBOXHDDRAWPARTDESC, *PVBOXHDDRAWPARTDESC;

/**
 * Auxiliary data structure for creating raw disks.
 */
typedef struct VBOXHDDRAW
{
    /** Signature for structure. Must be 'R', 'A', 'W', '\\0'. Actually a trick
     * to make logging of the comment string produce sensible results. */
    char            szSignature[4];
    /** Flag whether access to full disk should be given (ignoring the
     * partition information below). */
    bool            fRawDisk;
    /** Filename for the raw disk. Ignored for partitioned raw disks.
     * For Linux e.g. /dev/sda, and for Windows e.g. \\\\.\\PhysicalDisk0. */
    const char      *pszRawDisk;
    /** Number of entries in the partition descriptor array. */
    unsigned        cPartDescs;
    /** Pointer to the partition descriptor array. */
    PVBOXHDDRAWPARTDESC pPartDescs;
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
/** Do not perform the base/diff image check on open. This does NOT imply
 * opening the image as readonly (would break e.g. adding UUIDs to VMDK files
 * created by other products). Images opened with this flag should only be
 * used for querying information, and nothing else. */
#define VD_OPEN_FLAGS_INFO          RT_BIT(3)
/** Open image for asynchronous access. Only available if VD_CAP_ASYNC_IO is
 * set. VDOpen fails with VERR_NOT_SUPPORTED if this operation is not supported for
 * this kind of image. */
#define VD_OPEN_FLAGS_ASYNC_IO      RT_BIT(4)
/** Allow sharing of the image for writable images. May be ignored if the
 * format backend doesn't support this type of concurrent access. */
#define VD_OPEN_FLAGS_SHAREABLE     RT_BIT(5)
/** Ask the backend to switch to sequential accesses if possible. Opening
 * will not fail if it cannot do this, the flag will be simply ignored. */
#define VD_OPEN_FLAGS_SEQUENTIAL    RT_BIT(6)
/** Mask of valid flags. */
#define VD_OPEN_FLAGS_MASK          (VD_OPEN_FLAGS_NORMAL | VD_OPEN_FLAGS_READONLY | VD_OPEN_FLAGS_HONOR_ZEROES | VD_OPEN_FLAGS_HONOR_SAME | VD_OPEN_FLAGS_INFO | VD_OPEN_FLAGS_ASYNC_IO | VD_OPEN_FLAGS_SHAREABLE | VD_OPEN_FLAGS_SEQUENTIAL)
/** @}*/

/**
 * Helper functions to handle open flags.
 */

/**
 * Translate VD_OPEN_FLAGS_* to RTFile open flags.
 *
 * @return  RTFile open flags.
 * @param   uOpenFlags      VD_OPEN_FLAGS_* open flags.
 * @param   fCreate         Flag that the file should be created.
 */
DECLINLINE(uint32_t) VDOpenFlagsToFileOpenFlags(unsigned uOpenFlags, bool fCreate)
{
    AssertMsg(!((uOpenFlags & VD_OPEN_FLAGS_READONLY) && fCreate), ("Image can't be opened readonly while being created\n"));

    uint32_t fOpen = 0;

    if (RT_UNLIKELY(uOpenFlags & VD_OPEN_FLAGS_READONLY))
        fOpen |= RTFILE_O_READ | RTFILE_O_DENY_NONE;
    else
    {
        fOpen |= RTFILE_O_READWRITE;

        if (RT_UNLIKELY(uOpenFlags & VD_OPEN_FLAGS_SHAREABLE))
            fOpen |= RTFILE_O_DENY_NONE;
        else
            fOpen |= RTFILE_O_DENY_WRITE;
    }

    if (RT_UNLIKELY(fCreate))
        fOpen |= RTFILE_O_CREATE | RTFILE_O_NOT_CONTENT_INDEXED;
    else
        fOpen |= RTFILE_O_OPEN;

    return fOpen;
}


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
/** Supports asynchronous I/O operations for at least some configurations. */
#define VD_CAP_ASYNC                RT_BIT(5)
/** The backend operates on files. The caller needs to know to handle the
 * location appropriately. */
#define VD_CAP_FILE                 RT_BIT(6)
/** The backend uses the config interface. The caller needs to know how to
 * provide the mandatory configuration parts this way. */
#define VD_CAP_CONFIG               RT_BIT(7)
/** The backend uses the network stack interface. The caller has to provide
 * the appropriate interface. */
#define VD_CAP_TCPNET               RT_BIT(8)
/** The backend supports VFS (virtual filesystem) functionality since it uses
 * VDINTERFACEIO exclusively for all file operations. */
#define VD_CAP_VFS                  RT_BIT(9)
/** @}*/

/** @name VBox HDD container type.
 * @{
 */
typedef enum VDTYPE
{
    /** Invalid. */
    VDTYPE_INVALID = 0,
    /** HardDisk */
    VDTYPE_HDD,
    /** CD/DVD */
    VDTYPE_DVD,
    /** Floppy. */
    VDTYPE_FLOPPY
} VDTYPE;
/** @}*/

/**
 * Supported interface types.
 */
typedef enum VDINTERFACETYPE
{
    /** First valid interface. */
    VDINTERFACETYPE_FIRST = 0,
    /** Interface to pass error message to upper layers. Per-disk. */
    VDINTERFACETYPE_ERROR = VDINTERFACETYPE_FIRST,
    /** Interface for I/O operations. Per-image. */
    VDINTERFACETYPE_IO,
    /** Interface for progress notification. Per-operation. */
    VDINTERFACETYPE_PROGRESS,
    /** Interface for configuration information. Per-image. */
    VDINTERFACETYPE_CONFIG,
    /** Interface for TCP network stack. Per-image. */
    VDINTERFACETYPE_TCPNET,
    /** Interface for getting parent image state. Per-operation. */
    VDINTERFACETYPE_PARENTSTATE,
    /** Interface for synchronizing accesses from several threads. Per-disk. */
    VDINTERFACETYPE_THREADSYNC,
    /** Interface for I/O between the generic VBoxHDD code and the backend. Per-image (internal).
     * This interface is completely internal and must not be used elsewhere. */
    VDINTERFACETYPE_IOINT,
    /** invalid interface. */
    VDINTERFACETYPE_INVALID
} VDINTERFACETYPE;

/**
 * Common structure for all interfaces.
 */
typedef struct VDINTERFACE
{
    /** Human readable interface name. */
    const char         *pszInterfaceName;
    /** The size of the struct. */
    uint32_t            cbSize;
    /** Pointer to the next common interface structure. */
    struct VDINTERFACE *pNext;
    /** Interface type. */
    VDINTERFACETYPE     enmInterface;
    /** Opaque user data which is passed on every call. */
    void               *pvUser;
    /** Pointer to the function call table of the interface.
     *  As this is opaque this must be casted to the right interface
     *  struct defined below based on the interface type in enmInterface. */
    void               *pCallbacks;
} VDINTERFACE;
/** Pointer to a VDINTERFACE. */
typedef VDINTERFACE *PVDINTERFACE;
/** Pointer to a const VDINTERFACE. */
typedef const VDINTERFACE *PCVDINTERFACE;

/**
 * Helper functions to handle interface lists.
 *
 * @note These interface lists are used consistently to pass per-disk,
 * per-image and/or per-operation callbacks. Those three purposes are strictly
 * separate. See the individual interface declarations for what context they
 * apply to. The caller is responsible for ensuring that the lifetime of the
 * interface descriptors is appropriate for the category of interface.
 */

/**
 * Get a specific interface from a list of interfaces specified by the type.
 *
 * @return  Pointer to the matching interface or NULL if none was found.
 * @param   pVDIfs          Pointer to the VD interface list.
 * @param   enmInterface    Interface to search for.
 */
DECLINLINE(PVDINTERFACE) VDInterfaceGet(PVDINTERFACE pVDIfs, VDINTERFACETYPE enmInterface)
{
    AssertMsgReturn(   enmInterface >= VDINTERFACETYPE_FIRST
                    && enmInterface < VDINTERFACETYPE_INVALID,
                    ("enmInterface=%u", enmInterface), NULL);

    while (pVDIfs)
    {
        /* Sanity checks. */
        AssertMsgBreak(pVDIfs->cbSize == sizeof(VDINTERFACE),
                       ("cbSize=%u\n", pVDIfs->cbSize));

        if (pVDIfs->enmInterface == enmInterface)
            return pVDIfs;
        pVDIfs = pVDIfs->pNext;
    }

    /* No matching interface was found. */
    return NULL;
}

/**
 * Add an interface to a list of interfaces.
 *
 * @return VBox status code.
 * @param  pInterface   Pointer to an unitialized common interface structure.
 * @param  pszName      Name of the interface.
 * @param  enmInterface Type of the interface.
 * @param  pCallbacks   The callback table of the interface.
 * @param  pvUser       Opaque user data passed on every function call.
 * @param  ppVDIfs      Pointer to the VD interface list.
 */
DECLINLINE(int) VDInterfaceAdd(PVDINTERFACE pInterface, const char *pszName,
                               VDINTERFACETYPE enmInterface, void *pCallbacks,
                               void *pvUser, PVDINTERFACE *ppVDIfs)
{
    /* Argument checks. */
    AssertMsgReturn(   enmInterface >= VDINTERFACETYPE_FIRST
                    && enmInterface < VDINTERFACETYPE_INVALID,
                    ("enmInterface=%u", enmInterface), VERR_INVALID_PARAMETER);

    AssertMsgReturn(VALID_PTR(pCallbacks),
                    ("pCallbacks=%#p", pCallbacks),
                    VERR_INVALID_PARAMETER);

    AssertMsgReturn(VALID_PTR(ppVDIfs),
                    ("pInterfaceList=%#p", ppVDIfs),
                    VERR_INVALID_PARAMETER);

    /* Fill out interface descriptor. */
    pInterface->cbSize           = sizeof(VDINTERFACE);
    pInterface->pszInterfaceName = pszName;
    pInterface->enmInterface     = enmInterface;
    pInterface->pCallbacks       = pCallbacks;
    pInterface->pvUser           = pvUser;
    pInterface->pNext            = *ppVDIfs;

    /* Remember the new start of the list. */
    *ppVDIfs = pInterface;

    return VINF_SUCCESS;
}

/**
 * Removes an interface from a list of interfaces.
 *
 * @return VBox status code
 * @param  pInterface   Pointer to an initialized common interface structure to remove.
 * @param  ppVDIfs      Pointer to the VD interface list to remove from.
 */
DECLINLINE(int) VDInterfaceRemove(PVDINTERFACE pInterface, PVDINTERFACE *ppVDIfs)
{
    int rc = VERR_NOT_FOUND;

    /* Argument checks. */
    AssertMsgReturn(VALID_PTR(pInterface),
                    ("pInterface=%#p", pInterface),
                    VERR_INVALID_PARAMETER);

    AssertMsgReturn(VALID_PTR(ppVDIfs),
                    ("pInterfaceList=%#p", ppVDIfs),
                    VERR_INVALID_PARAMETER);

    if (*ppVDIfs)
    {
        PVDINTERFACE pPrev = NULL;
        PVDINTERFACE pCurr = *ppVDIfs;

        while (   pCurr
               && (pCurr != pInterface))
        {
            pPrev = pCurr;
            pCurr = pCurr->pNext;
        }

        /* First interface */
        if (!pPrev)
        {
            *ppVDIfs = pCurr->pNext;
            rc = VINF_SUCCESS;
        }
        else if (pCurr)
        {
            pPrev = pCurr->pNext;
            rc = VINF_SUCCESS;
        }
    }

    return rc;
}

/**
 * Interface to deliver error messages (and also informational messages)
 * to upper layers.
 *
 * Per-disk interface. Optional, but think twice if you want to miss the
 * opportunity of reporting better human-readable error messages.
 */
typedef struct VDINTERFACEERROR
{
    /**
     * Size of the error interface.
     */
    uint32_t    cbSize;

    /**
     * Interface type.
     */
    VDINTERFACETYPE enmInterface;

    /**
     * Error message callback. Must be able to accept special IPRT format
     * strings.
     *
     * @param   pvUser          The opaque data passed on container creation.
     * @param   rc              The VBox error code.
     * @param   RT_SRC_POS_DECL Use RT_SRC_POS.
     * @param   pszFormat       Error message format string.
     * @param   va              Error message arguments.
     */
    DECLR3CALLBACKMEMBER(void, pfnError, (void *pvUser, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list va));

    /**
     * Informational message callback. May be NULL. Used e.g. in
     * VDDumpImages(). Must be able to accept special IPRT format strings.
     *
     * @return  VBox status code.
     * @param   pvUser          The opaque data passed on container creation.
     * @param   pszFormat       Message format string.
     * @param   va              Message arguments.
     */
    DECLR3CALLBACKMEMBER(int, pfnMessage, (void *pvUser, const char *pszFormat, va_list va));

} VDINTERFACEERROR, *PVDINTERFACEERROR;

/**
 * Get error interface from opaque callback table.
 *
 * @return Pointer to the callback table.
 * @param  pInterface Pointer to the interface descriptor.
 */
DECLINLINE(PVDINTERFACEERROR) VDGetInterfaceError(PVDINTERFACE pInterface)
{
    PVDINTERFACEERROR pInterfaceError;

    /* Check that the interface descriptor is a error interface. */
    AssertMsgReturn(   pInterface->enmInterface == VDINTERFACETYPE_ERROR
                    && pInterface->cbSize == sizeof(VDINTERFACE),
                    ("Not an error interface"), NULL);

    pInterfaceError = (PVDINTERFACEERROR)pInterface->pCallbacks;

    /* Do basic checks. */
    AssertMsgReturn(   pInterfaceError->cbSize == sizeof(VDINTERFACEERROR)
                    && pInterfaceError->enmInterface == VDINTERFACETYPE_ERROR,
                    ("A non error callback table attached to a error interface descriptor\n"), NULL);

    return pInterfaceError;
}

/**
 * Completion callback which is called by the interface owner
 * to inform the backend that a task finished.
 *
 * @return  VBox status code.
 * @param   pvUser          Opaque user data which is passed on request submission.
 * @param   rcReq           Status code of the completed request.
 */
typedef DECLCALLBACK(int) FNVDCOMPLETED(void *pvUser, int rcReq);
/** Pointer to FNVDCOMPLETED() */
typedef FNVDCOMPLETED *PFNVDCOMPLETED;

/**
 * Support interface for I/O
 *
 * Per-image. Optional as input.
 */
typedef struct VDINTERFACEIO
{
    /**
     * Size of the I/O interface.
     */
    uint32_t    cbSize;

    /**
     * Interface type.
     */
    VDINTERFACETYPE enmInterface;

    /**
     * Open callback
     *
     * @return  VBox status code.
     * @param   pvUser          The opaque data passed on container creation.
     * @param   pszLocation     Name of the location to open.
     * @param   fOpen           Flags for opening the backend.
     *                          See RTFILE_O_* #defines, inventing another set
     *                          of open flags is not worth the mapping effort.
     * @param   pfnCompleted    The callback which is called whenever a task
     *                          completed. The backend has to pass the user data
     *                          of the request initiator (ie the one who calls
     *                          VDAsyncRead or VDAsyncWrite) in pvCompletion
     *                          if this is NULL.
     * @param   ppStorage       Where to store the opaque storage handle.
     */
    DECLR3CALLBACKMEMBER(int, pfnOpen, (void *pvUser, const char *pszLocation,
                                        uint32_t fOpen,
                                        PFNVDCOMPLETED pfnCompleted,
                                        void **ppStorage));

    /**
     * Close callback.
     *
     * @return  VBox status code.
     * @param   pvUser          The opaque data passed on container creation.
     * @param   pStorage        The opaque storage handle to close.
     */
    DECLR3CALLBACKMEMBER(int, pfnClose, (void *pvUser, void *pStorage));

    /**
     * Delete callback.
     *
     * @return  VBox status code.
     * @param   pvUser          The opaque data passed on container creation.
     * @param   pcszFilename    Name of the file to delete.
     */
    DECLR3CALLBACKMEMBER(int, pfnDelete, (void *pvUser, const char *pcszFilename));

    /**
     * Move callback.
     *
     * @return  VBox status code.
     * @param   pvUser          The opaque data passed on container creation.
     * @param   pcszSrc         The path to the source file.
     * @param   pcszDst         The path to the destination file.
     *                          This file will be created.
     * @param   fMove           A combination of the RTFILEMOVE_* flags.
     */
    DECLR3CALLBACKMEMBER(int, pfnMove, (void *pvUser, const char *pcszSrc, const char *pcszDst, unsigned fMove));

    /**
     * Returns the free space on a disk.
     *
     * @return  VBox status code.
     * @param   pvUser          The opaque data passed on container creation.
     * @param   pcszFilename    Name of a file to identify the disk.
     * @param   pcbFreeSpace    Where to store the free space of the disk.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetFreeSpace, (void *pvUser, const char *pcszFilename, int64_t *pcbFreeSpace));

    /**
     * Returns the last modification timestamp of a file.
     *
     * @return  VBox status code.
     * @param   pvUser          The opaque data passed on container creation.
     * @param   pcszFilename    Name of a file to identify the disk.
     * @param   pModificationTime   Where to store the timestamp of the file.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetModificationTime, (void *pvUser, const char *pcszFilename, PRTTIMESPEC pModificationTime));

    /**
     * Returns the size of the opened storage backend.
     *
     * @return  VBox status code.
     * @param   pvUser          The opaque data passed on container creation.
     * @param   pStorage        The opaque storage handle to close.
     * @param   pcbSize         Where to store the size of the storage backend.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetSize, (void *pvUser, void *pStorage, uint64_t *pcbSize));

    /**
     * Sets the size of the opened storage backend if possible.
     *
     * @return  VBox status code.
     * @retval  VERR_NOT_SUPPORTED if the backend does not support this operation.
     * @param   pvUser          The opaque data passed on container creation.
     * @param   pStorage        The opaque storage handle to close.
     * @param   cbSize          The new size of the image.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetSize, (void *pvUser, void *pStorage, uint64_t cbSize));

    /**
     * Synchronous write callback.
     *
     * @return  VBox status code.
     * @param   pvUser          The opaque data passed on container creation.
     * @param   pStorage        The storage handle to use.
     * @param   uOffset         The offset to start from.
     * @param   pvBuffer        Pointer to the bits need to be written.
     * @param   cbBuffer        How many bytes to write.
     * @param   pcbWritten      Where to store how many bytes were actually written.
     */
    DECLR3CALLBACKMEMBER(int, pfnWriteSync, (void *pvUser, void *pStorage, uint64_t uOffset,
                                             const void *pvBuffer, size_t cbBuffer, size_t *pcbWritten));

    /**
     * Synchronous read callback.
     *
     * @return  VBox status code.
     * @param   pvUser          The opaque data passed on container creation.
     * @param   pStorage        The storage handle to use.
     * @param   uOffset         The offset to start from.
     * @param   pvBuffer        Where to store the read bits.
     * @param   cbBuffer        How many bytes to read.
     * @param   pcbRead         Where to store how many bytes were actually read.
     */
    DECLR3CALLBACKMEMBER(int, pfnReadSync, (void *pvUser, void *pStorage, uint64_t uOffset,
                                            void *pvBuffer, size_t cbBuffer, size_t *pcbRead));

    /**
     * Flush data to the storage backend.
     *
     * @return  VBox status code.
     * @param   pvUser          The opaque data passed on container creation.
     * @param   pStorage        The storage handle to flush.
     */
    DECLR3CALLBACKMEMBER(int, pfnFlushSync, (void *pvUser, void *pStorage));

    /**
     * Initiate an asynchronous read request.
     *
     * @return  VBox status code.
     * @param   pvUser         The opaque user data passed on container creation.
     * @param   pStorage       The storage handle.
     * @param   uOffset        The offset to start reading from.
     * @param   paSegments     Scatter gather list to store the data in.
     * @param   cSegments      Number of segments in the list.
     * @param   cbRead         How many bytes to read.
     * @param   pvCompletion   The opaque user data which is returned upon completion.
     * @param   ppTask         Where to store the opaque task handle.
     */
    DECLR3CALLBACKMEMBER(int, pfnReadAsync, (void *pvUser, void *pStorage, uint64_t uOffset,
                                             PCRTSGSEG paSegments, size_t cSegments,
                                             size_t cbRead, void *pvCompletion,
                                             void **ppTask));

    /**
     * Initiate an asynchronous write request.
     *
     * @return  VBox status code.
     * @param   pvUser         The opaque user data passed on conatiner creation.
     * @param   pStorage       The storage handle.
     * @param   uOffset        The offset to start writing to.
     * @param   paSegments     Scatter gather list of the data to write
     * @param   cSegments      Number of segments in the list.
     * @param   cbWrite        How many bytes to write.
     * @param   pvCompletion   The opaque user data which is returned upon completion.
     * @param   ppTask         Where to store the opaque task handle.
     */
    DECLR3CALLBACKMEMBER(int, pfnWriteAsync, (void *pvUser, void *pStorage, uint64_t uOffset,
                                              PCRTSGSEG paSegments, size_t cSegments,
                                              size_t cbWrite, void *pvCompletion,
                                              void **ppTask));

    /**
     * Initiates an async flush request.
     *
     * @return  VBox status code.
     * @param   pvUser          The opaque data passed on container creation.
     * @param   pStorage        The storage handle to flush.
     * @param   pvCompletion    The opaque user data which is returned upon completion.
     * @param   ppTask          Where to store the opaque task handle.
     */
    DECLR3CALLBACKMEMBER(int, pfnFlushAsync, (void *pvUser, void *pStorage,
                                              void *pvCompletion, void **ppTask));

} VDINTERFACEIO, *PVDINTERFACEIO;

/**
 * Get I/O interface from opaque callback table.
 *
 * @return Pointer to the callback table.
 * @param  pInterface Pointer to the interface descriptor.
 */
DECLINLINE(PVDINTERFACEIO) VDGetInterfaceIO(PVDINTERFACE pInterface)
{
    PVDINTERFACEIO pInterfaceIO;

    /* Check that the interface descriptor is an I/O interface. */
    AssertMsgReturn(   (pInterface->enmInterface == VDINTERFACETYPE_IO)
                    && (pInterface->cbSize == sizeof(VDINTERFACE)),
                    ("Not an I/O interface"), NULL);

    pInterfaceIO = (PVDINTERFACEIO)pInterface->pCallbacks;

    /* Do basic checks. */
    AssertMsgReturn(   (pInterfaceIO->cbSize == sizeof(VDINTERFACEIO))
                    && (pInterfaceIO->enmInterface == VDINTERFACETYPE_IO),
                    ("A non I/O callback table attached to an I/O interface descriptor\n"), NULL);

    return pInterfaceIO;
}

/**
 * Callback which provides progress information about a currently running
 * lengthy operation.
 *
 * @return  VBox status code.
 * @param   pvUser          The opaque user data associated with this interface.
 * @param   uPercent        Completion percentage.
 */
typedef DECLCALLBACK(int) FNVDPROGRESS(void *pvUser, unsigned uPercentage);
/** Pointer to FNVDPROGRESS() */
typedef FNVDPROGRESS *PFNVDPROGRESS;

/**
 * Progress notification interface
 *
 * Per-operation. Optional.
 */
typedef struct VDINTERFACEPROGRESS
{
    /**
     * Size of the progress interface.
     */
    uint32_t    cbSize;

    /**
     * Interface type.
     */
    VDINTERFACETYPE enmInterface;

    /**
     * Progress notification callbacks.
     */
    PFNVDPROGRESS pfnProgress;

} VDINTERFACEPROGRESS, *PVDINTERFACEPROGRESS;

/**
 * Get progress interface from opaque callback table.
 *
 * @return Pointer to the callback table.
 * @param  pInterface Pointer to the interface descriptor.
 */
DECLINLINE(PVDINTERFACEPROGRESS) VDGetInterfaceProgress(PVDINTERFACE pInterface)
{
    PVDINTERFACEPROGRESS pInterfaceProgress;

    /* Check that the interface descriptor is a progress interface. */
    AssertMsgReturn(   (pInterface->enmInterface == VDINTERFACETYPE_PROGRESS)
                    && (pInterface->cbSize == sizeof(VDINTERFACE)),
                    ("Not a progress interface"), NULL);


    pInterfaceProgress = (PVDINTERFACEPROGRESS)pInterface->pCallbacks;

    /* Do basic checks. */
    AssertMsgReturn(   (pInterfaceProgress->cbSize == sizeof(VDINTERFACEPROGRESS))
                    && (pInterfaceProgress->enmInterface == VDINTERFACETYPE_PROGRESS),
                    ("A non progress callback table attached to a progress interface descriptor\n"), NULL);

    return pInterfaceProgress;
}


/**
 * Configuration information interface
 *
 * Per-image. Optional for most backends, but mandatory for images which do
 * not operate on files (including standard block or character devices).
 */
typedef struct VDINTERFACECONFIG
{
    /**
     * Size of the configuration interface.
     */
    uint32_t    cbSize;

    /**
     * Interface type.
     */
    VDINTERFACETYPE enmInterface;

    /**
     * Validates that the keys are within a set of valid names.
     *
     * @return  true if all key names are found in pszzAllowed.
     * @return  false if not.
     * @param   pvUser          The opaque user data associated with this interface.
     * @param   pszzValid       List of valid key names separated by '\\0' and ending with
     *                          a double '\\0'.
     */
    DECLR3CALLBACKMEMBER(bool, pfnAreKeysValid, (void *pvUser, const char *pszzValid));

    /**
     * Retrieves the length of the string value associated with a key (including
     * the terminator, for compatibility with CFGMR3QuerySize).
     *
     * @return  VBox status code.
     *          VERR_CFGM_VALUE_NOT_FOUND means that the key is not known.
     * @param   pvUser          The opaque user data associated with this interface.
     * @param   pszName         Name of the key to query.
     * @param   pcbValue        Where to store the value length. Non-NULL.
     */
    DECLR3CALLBACKMEMBER(int, pfnQuerySize, (void *pvUser, const char *pszName, size_t *pcbValue));

    /**
     * Query the string value associated with a key.
     *
     * @return  VBox status code.
     *          VERR_CFGM_VALUE_NOT_FOUND means that the key is not known.
     *          VERR_CFGM_NOT_ENOUGH_SPACE means that the buffer is not big enough.
     * @param   pvUser          The opaque user data associated with this interface.
     * @param   pszName         Name of the key to query.
     * @param   pszValue        Pointer to buffer where to store value.
     * @param   cchValue        Length of value buffer.
     */
    DECLR3CALLBACKMEMBER(int, pfnQuery, (void *pvUser, const char *pszName, char *pszValue, size_t cchValue));

} VDINTERFACECONFIG, *PVDINTERFACECONFIG;

/**
 * Get configuration information interface from opaque callback table.
 *
 * @return Pointer to the callback table.
 * @param  pInterface Pointer to the interface descriptor.
 */
DECLINLINE(PVDINTERFACECONFIG) VDGetInterfaceConfig(PVDINTERFACE pInterface)
{
    PVDINTERFACECONFIG pInterfaceConfig;

    /* Check that the interface descriptor is a config interface. */
    AssertMsgReturn(   (pInterface->enmInterface == VDINTERFACETYPE_CONFIG)
                    && (pInterface->cbSize == sizeof(VDINTERFACE)),
                    ("Not a config interface"), NULL);

    pInterfaceConfig = (PVDINTERFACECONFIG)pInterface->pCallbacks;

    /* Do basic checks. */
    AssertMsgReturn(   (pInterfaceConfig->cbSize == sizeof(VDINTERFACECONFIG))
                    && (pInterfaceConfig->enmInterface == VDINTERFACETYPE_CONFIG),
                    ("A non config callback table attached to a config interface descriptor\n"), NULL);

    return pInterfaceConfig;
}

/**
 * Query configuration, validates that the keys are within a set of valid names.
 *
 * @return  true if all key names are found in pszzAllowed.
 * @return  false if not.
 * @param   pCfgIf      Pointer to configuration callback table.
 * @param   pvUser      The opaque user data associated with this interface.
 * @param   pszzValid   List of valid names separated by '\\0' and ending with
 *                      a double '\\0'.
 */
DECLINLINE(bool) VDCFGAreKeysValid(PVDINTERFACECONFIG pCfgIf, void *pvUser,
                                   const char *pszzValid)
{
    return pCfgIf->pfnAreKeysValid(pvUser, pszzValid);
}

/**
 * Query configuration, unsigned 64-bit integer value with default.
 *
 * @return  VBox status code.
 * @param   pCfgIf      Pointer to configuration callback table.
 * @param   pvUser      The opaque user data associated with this interface.
 * @param   pszName     Name of an integer value
 * @param   pu64        Where to store the value. Set to default on failure.
 * @param   u64Def      The default value.
 */
DECLINLINE(int) VDCFGQueryU64Def(PVDINTERFACECONFIG pCfgIf, void *pvUser,
                                 const char *pszName, uint64_t *pu64,
                                 uint64_t u64Def)
{
    char aszBuf[32];
    int rc = pCfgIf->pfnQuery(pvUser, pszName, aszBuf, sizeof(aszBuf));
    if (RT_SUCCESS(rc))
    {
        rc = RTStrToUInt64Full(aszBuf, 0, pu64);
    }
    else if (rc == VERR_CFGM_VALUE_NOT_FOUND)
    {
        rc = VINF_SUCCESS;
        *pu64 = u64Def;
    }
    return rc;
}

/**
 * Query configuration, unsigned 32-bit integer value with default.
 *
 * @return  VBox status code.
 * @param   pCfgIf      Pointer to configuration callback table.
 * @param   pvUser      The opaque user data associated with this interface.
 * @param   pszName     Name of an integer value
 * @param   pu32        Where to store the value. Set to default on failure.
 * @param   u32Def      The default value.
 */
DECLINLINE(int) VDCFGQueryU32Def(PVDINTERFACECONFIG pCfgIf, void *pvUser,
                                 const char *pszName, uint32_t *pu32,
                                 uint32_t u32Def)
{
    uint64_t u64;
    int rc = VDCFGQueryU64Def(pCfgIf, pvUser, pszName, &u64, u32Def);
    if (RT_SUCCESS(rc))
    {
        if (!(u64 & UINT64_C(0xffffffff00000000)))
            *pu32 = (uint32_t)u64;
        else
            rc = VERR_CFGM_INTEGER_TOO_BIG;
    }
    return rc;
}

/**
 * Query configuration, bool value with default.
 *
 * @return  VBox status code.
 * @param   pCfgIf      Pointer to configuration callback table.
 * @param   pvUser      The opaque user data associated with this interface.
 * @param   pszName     Name of an integer value
 * @param   pf          Where to store the value. Set to default on failure.
 * @param   fDef        The default value.
 */
DECLINLINE(int) VDCFGQueryBoolDef(PVDINTERFACECONFIG pCfgIf, void *pvUser,
                                  const char *pszName, bool *pf,
                                  bool fDef)
{
    uint64_t u64;
    int rc = VDCFGQueryU64Def(pCfgIf, pvUser, pszName, &u64, fDef);
    if (RT_SUCCESS(rc))
        *pf = u64 ? true : false;
    return rc;
}

/**
 * Query configuration, dynamically allocated (RTMemAlloc) zero terminated
 * character value.
 *
 * @return  VBox status code.
 * @param   pCfgIf      Pointer to configuration callback table.
 * @param   pvUser      The opaque user data associated with this interface.
 * @param   pszName     Name of an zero terminated character value
 * @param   ppszString  Where to store the string pointer. Not set on failure.
 *                      Free this using RTMemFree().
 */
DECLINLINE(int) VDCFGQueryStringAlloc(PVDINTERFACECONFIG pCfgIf,
                                      void *pvUser, const char *pszName,
                                      char **ppszString)
{
    size_t cb;
    int rc = pCfgIf->pfnQuerySize(pvUser, pszName, &cb);
    if (RT_SUCCESS(rc))
    {
        char *pszString = (char *)RTMemAlloc(cb);
        if (pszString)
        {
            rc = pCfgIf->pfnQuery(pvUser, pszName, pszString, cb);
            if (RT_SUCCESS(rc))
                *ppszString = pszString;
            else
                RTMemFree(pszString);
        }
        else
            rc = VERR_NO_MEMORY;
    }
    return rc;
}

/**
 * Query configuration, dynamically allocated (RTMemAlloc) zero terminated
 * character value with default.
 *
 * @return  VBox status code.
 * @param   pCfgIf      Pointer to configuration callback table.
 * @param   pvUser      The opaque user data associated with this interface.
 * @param   pszName     Name of an zero terminated character value
 * @param   ppszString  Where to store the string pointer. Not set on failure.
 *                      Free this using RTMemFree().
 * @param   pszDef      The default value.
 */
DECLINLINE(int) VDCFGQueryStringAllocDef(PVDINTERFACECONFIG pCfgIf,
                                         void *pvUser, const char *pszName,
                                         char **ppszString,
                                         const char *pszDef)
{
    size_t cb;
    int rc = pCfgIf->pfnQuerySize(pvUser, pszName, &cb);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND || rc == VERR_CFGM_NO_PARENT)
    {
        cb = strlen(pszDef) + 1;
        rc = VINF_SUCCESS;
    }
    if (RT_SUCCESS(rc))
    {
        char *pszString = (char *)RTMemAlloc(cb);
        if (pszString)
        {
            rc = pCfgIf->pfnQuery(pvUser, pszName, pszString, cb);
            if (rc == VERR_CFGM_VALUE_NOT_FOUND || rc == VERR_CFGM_NO_PARENT)
            {
                memcpy(pszString, pszDef, cb);
                rc = VINF_SUCCESS;
            }
            if (RT_SUCCESS(rc))
                *ppszString = pszString;
            else
                RTMemFree(pszString);
        }
        else
            rc = VERR_NO_MEMORY;
    }
    return rc;
}

/**
 * Query configuration, dynamically allocated (RTMemAlloc) byte string value.
 *
 * @return  VBox status code.
 * @param   pCfgIf      Pointer to configuration callback table.
 * @param   pvUser      The opaque user data associated with this interface.
 * @param   pszName     Name of an zero terminated character value
 * @param   ppvData     Where to store the byte string pointer. Not set on failure.
 *                      Free this using RTMemFree().
 * @param   pcbData     Where to store the byte string length.
 */
DECLINLINE(int) VDCFGQueryBytesAlloc(PVDINTERFACECONFIG pCfgIf,
                                     void *pvUser, const char *pszName,
                                     void **ppvData, size_t *pcbData)
{
    size_t cb;
    int rc = pCfgIf->pfnQuerySize(pvUser, pszName, &cb);
    if (RT_SUCCESS(rc))
    {
        char *pbData;
        Assert(cb);

        pbData = (char *)RTMemAlloc(cb);
        if (pbData)
        {
            rc = pCfgIf->pfnQuery(pvUser, pszName, pbData, cb);
            if (RT_SUCCESS(rc))
            {
                *ppvData = pbData;
                *pcbData = cb - 1; /* Exclude terminator of the queried string. */
            }
            else
                RTMemFree(pbData);
        }
        else
            rc = VERR_NO_MEMORY;
    }
    return rc;
}

/** Forward declaration of a VD socket. */
typedef struct VDSOCKETINT *VDSOCKET;
/** Pointer to a VD socket. */
typedef VDSOCKET *PVDSOCKET;
/** Nil socket handle. */
#define NIL_VDSOCKET ((VDSOCKET)0)

/** Connect flag to indicate that the backend wants to use the extended
 * socket I/O multiplexing call. This might not be supported on all configurations
 * (internal networking and iSCSI)
 * and the backend needs to take appropriate action.
 */
#define VD_INTERFACETCPNET_CONNECT_EXTENDED_SELECT RT_BIT_32(0)

/** @name Select events
 * @{ */
/** Readable without blocking. */
#define VD_INTERFACETCPNET_EVT_READ         RT_BIT_32(0)
/** Writable without blocking. */
#define VD_INTERFACETCPNET_EVT_WRITE        RT_BIT_32(1)
/** Error condition, hangup, exception or similar. */
#define VD_INTERFACETCPNET_EVT_ERROR        RT_BIT_32(2)
/** Hint for the select that getting interrupted while waiting is more likely.
 * The interface implementation can optimize the waiting strategy based on this.
 * It is assumed that it is more likely to get one of the above socket events
 * instead of being interrupted if the flag is not set. */
#define VD_INTERFACETCPNET_HINT_INTERRUPT   RT_BIT_32(3)
/** Mask of the valid bits. */
#define VD_INTERFACETCPNET_EVT_VALID_MASK   UINT32_C(0x0000000f)
/** @} */

/**
 * TCP network stack interface
 *
 * Per-image. Mandatory for backends which have the VD_CAP_TCPNET bit set.
 */
typedef struct VDINTERFACETCPNET
{
    /**
     * Size of the configuration interface.
     */
    uint32_t    cbSize;

    /**
     * Interface type.
     */
    VDINTERFACETYPE enmInterface;

    /**
     * Creates a socket. The socket is not connected if this succeeds.
     *
     * @return  iprt status code.
     * @retval  VERR_NOT_SUPPORTED if the combination of flags is not supported.
     * @param   fFlags    Combination of the VD_INTERFACETCPNET_CONNECT_* #defines.
     * @param   pSock     Where to store the handle.
     */
    DECLR3CALLBACKMEMBER(int, pfnSocketCreate, (uint32_t fFlags, PVDSOCKET pSock));

    /**
     * Destroys the socket.
     *
     * @return iprt status code.
     * @param  Sock       Socket descriptor.
     */
    DECLR3CALLBACKMEMBER(int, pfnSocketDestroy, (VDSOCKET Sock));

    /**
     * Connect as a client to a TCP port.
     *
     * @return  iprt status code.
     * @param   Sock            Socket descriptor.
     * @param   pszAddress      The address to connect to.
     * @param   uPort           The port to connect to.
     */
    DECLR3CALLBACKMEMBER(int, pfnClientConnect, (VDSOCKET Sock, const char *pszAddress, uint32_t uPort));

    /**
     * Close a TCP connection.
     *
     * @return  iprt status code.
     * @param   Sock            Socket descriptor.
     */
    DECLR3CALLBACKMEMBER(int, pfnClientClose, (VDSOCKET Sock));

    /**
     * Returns whether the socket is currently connected to the client.
     *
     * @returns true if the socket is connected.
     *          false otherwise.
     * @param   Sock        Socket descriptor.
     */
    DECLR3CALLBACKMEMBER(bool, pfnIsClientConnected, (VDSOCKET Sock));

    /**
     * Socket I/O multiplexing.
     * Checks if the socket is ready for reading.
     *
     * @return  iprt status code.
     * @param   Sock        Socket descriptor.
     * @param   cMillies    Number of milliseconds to wait for the socket.
     *                      Use RT_INDEFINITE_WAIT to wait for ever.
     */
    DECLR3CALLBACKMEMBER(int, pfnSelectOne, (VDSOCKET Sock, RTMSINTERVAL cMillies));

    /**
     * Receive data from a socket.
     *
     * @return  iprt status code.
     * @param   Sock        Socket descriptor.
     * @param   pvBuffer    Where to put the data we read.
     * @param   cbBuffer    Read buffer size.
     * @param   pcbRead     Number of bytes read.
     *                      If NULL the entire buffer will be filled upon successful return.
     *                      If not NULL a partial read can be done successfully.
     */
    DECLR3CALLBACKMEMBER(int, pfnRead, (VDSOCKET Sock, void *pvBuffer, size_t cbBuffer, size_t *pcbRead));

    /**
     * Send data to a socket.
     *
     * @return  iprt status code.
     * @param   Sock        Socket descriptor.
     * @param   pvBuffer    Buffer to write data to socket.
     * @param   cbBuffer    How much to write.
     */
    DECLR3CALLBACKMEMBER(int, pfnWrite, (VDSOCKET Sock, const void *pvBuffer, size_t cbBuffer));

    /**
     * Send data from scatter/gather buffer to a socket.
     *
     * @return  iprt status code.
     * @param   Sock        Socket descriptor.
     * @param   pSgBuffer   Scatter/gather buffer to write data to socket.
     */
    DECLR3CALLBACKMEMBER(int, pfnSgWrite, (VDSOCKET Sock, PCRTSGBUF pSgBuffer));

    /**
     * Receive data from a socket - not blocking.
     *
     * @return  iprt status code.
     * @param   Sock        Socket descriptor.
     * @param   pvBuffer    Where to put the data we read.
     * @param   cbBuffer    Read buffer size.
     * @param   pcbRead     Number of bytes read.
     */
    DECLR3CALLBACKMEMBER(int, pfnReadNB, (VDSOCKET Sock, void *pvBuffer, size_t cbBuffer, size_t *pcbRead));

    /**
     * Send data to a socket - not blocking.
     *
     * @return  iprt status code.
     * @param   Sock        Socket descriptor.
     * @param   pvBuffer    Buffer to write data to socket.
     * @param   cbBuffer    How much to write.
     * @param   pcbWritten  Number of bytes written.
     */
    DECLR3CALLBACKMEMBER(int, pfnWriteNB, (VDSOCKET Sock, const void *pvBuffer, size_t cbBuffer, size_t *pcbWritten));

    /**
     * Send data from scatter/gather buffer to a socket - not blocking.
     *
     * @return  iprt status code.
     * @param   Sock        Socket descriptor.
     * @param   pSgBuffer   Scatter/gather buffer to write data to socket.
     * @param   pcbWritten  Number of bytes written.
     */
    DECLR3CALLBACKMEMBER(int, pfnSgWriteNB, (VDSOCKET Sock, PRTSGBUF pSgBuffer, size_t *pcbWritten));

    /**
     * Flush socket write buffers.
     *
     * @return  iprt status code.
     * @param   Sock        Socket descriptor.
     */
    DECLR3CALLBACKMEMBER(int, pfnFlush, (VDSOCKET Sock));

    /**
     * Enables or disables delaying sends to coalesce packets.
     *
     * @return  iprt status code.
     * @param   Sock        Socket descriptor.
     * @param   fEnable     When set to true enables coalescing.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetSendCoalescing, (VDSOCKET Sock, bool fEnable));

    /**
     * Gets the address of the local side.
     *
     * @return  iprt status code.
     * @param   Sock        Socket descriptor.
     * @param   pAddr       Where to store the local address on success.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetLocalAddress, (VDSOCKET Sock, PRTNETADDR pAddr));

    /**
     * Gets the address of the other party.
     *
     * @return  iprt status code.
     * @param   Sock        Socket descriptor.
     * @param   pAddr       Where to store the peer address on success.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetPeerAddress, (VDSOCKET Sock, PRTNETADDR pAddr));

    /**
     * Socket I/O multiplexing - extended version which can be woken up.
     * Checks if the socket is ready for reading or writing.
     *
     * @return  iprt status code.
     * @retval  VERR_INTERRUPTED if the thread was woken up by a pfnPoke call.
     * @param   Sock        Socket descriptor.
     * @param   fEvents     Mask of events to wait for.
     * @param   pfEvents    Where to store the received events.
     * @param   cMillies    Number of milliseconds to wait for the socket.
     *                      Use RT_INDEFINITE_WAIT to wait for ever.
     */
    DECLR3CALLBACKMEMBER(int, pfnSelectOneEx, (VDSOCKET Sock, uint32_t fEvents,
                                               uint32_t *pfEvents, RTMSINTERVAL cMillies));

    /**
     * Wakes up the thread waiting in pfnSelectOneEx.
     *
     * @return iprt status code.
     * @param  Sock        Socket descriptor.
     */
    DECLR3CALLBACKMEMBER(int, pfnPoke, (VDSOCKET Sock));

} VDINTERFACETCPNET, *PVDINTERFACETCPNET;

/**
 * Get TCP network stack interface from opaque callback table.
 *
 * @return Pointer to the callback table.
 * @param  pInterface Pointer to the interface descriptor.
 */
DECLINLINE(PVDINTERFACETCPNET) VDGetInterfaceTcpNet(PVDINTERFACE pInterface)
{
    PVDINTERFACETCPNET pInterfaceTcpNet;

    /* Check that the interface descriptor is a TCP network stack interface. */
    AssertMsgReturn(   (pInterface->enmInterface == VDINTERFACETYPE_TCPNET)
                    && (pInterface->cbSize == sizeof(VDINTERFACE)),
                    ("Not a TCP network stack interface"), NULL);

    pInterfaceTcpNet = (PVDINTERFACETCPNET)pInterface->pCallbacks;

    /* Do basic checks. */
    AssertMsgReturn(   (pInterfaceTcpNet->cbSize == sizeof(VDINTERFACETCPNET))
                    && (pInterfaceTcpNet->enmInterface == VDINTERFACETYPE_TCPNET),
                    ("A non TCP network stack callback table attached to a TCP network stack interface descriptor\n"), NULL);

    return pInterfaceTcpNet;
}

/**
 * Interface to get the parent state.
 *
 * Per-operation interface. Optional, present only if there is a parent, and
 * used only internally for compacting.
 */
typedef struct VDINTERFACEPARENTSTATE
{
    /**
     * Size of the parent state interface.
     */
    uint32_t    cbSize;

    /**
     * Interface type.
     */
    VDINTERFACETYPE enmInterface;

    /**
     * Read data callback.
     *
     * @return  VBox status code.
     * @return  VERR_VD_NOT_OPENED if no image is opened in HDD container.
     * @param   pvUser          The opaque data passed for the operation.
     * @param   uOffset         Offset of first reading byte from start of disk.
     *                          Must be aligned to a sector boundary.
     * @param   pvBuffer        Pointer to buffer for reading data.
     * @param   cbBuffer        Number of bytes to read.
     *                          Must be aligned to a sector boundary.
     */
    DECLR3CALLBACKMEMBER(int, pfnParentRead, (void *pvUser, uint64_t uOffset, void *pvBuffer, size_t cbBuffer));

} VDINTERFACEPARENTSTATE, *PVDINTERFACEPARENTSTATE;


/**
 * Get parent state interface from opaque callback table.
 *
 * @return Pointer to the callback table.
 * @param  pInterface Pointer to the interface descriptor.
 */
DECLINLINE(PVDINTERFACEPARENTSTATE) VDGetInterfaceParentState(PVDINTERFACE pInterface)
{
    PVDINTERFACEPARENTSTATE pInterfaceParentState;

    /* Check that the interface descriptor is a parent state interface. */
    AssertMsgReturn(   (pInterface->enmInterface == VDINTERFACETYPE_PARENTSTATE)
                    && (pInterface->cbSize == sizeof(VDINTERFACE)),
                    ("Not a parent state interface"), NULL);

    pInterfaceParentState = (PVDINTERFACEPARENTSTATE)pInterface->pCallbacks;

    /* Do basic checks. */
    AssertMsgReturn(   (pInterfaceParentState->cbSize == sizeof(VDINTERFACEPARENTSTATE))
                    && (pInterfaceParentState->enmInterface == VDINTERFACETYPE_PARENTSTATE),
                    ("A non parent state callback table attached to a parent state interface descriptor\n"), NULL);

    return pInterfaceParentState;
}

/**
 * Interface to synchronize concurrent accesses by several threads.
 *
 * @note The scope of this interface is to manage concurrent accesses after
 * the HDD container has been created, and they must stop before destroying the
 * container. Opening or closing images is covered by the synchronization, but
 * that does not mean it is safe to close images while a thread executes
 * <link to="VDMerge"/> or <link to="VDCopy"/> operating on these images.
 * Making them safe would require the lock to be held during the entire
 * operation, which prevents other concurrent acitivities.
 *
 * @note Right now this is kept as simple as possible, and does not even
 * attempt to provide enough information to allow e.g. concurrent write
 * accesses to different areas of the disk. The reason is that it is very
 * difficult to predict which area of a disk is affected by a write,
 * especially when different image formats are mixed. Maybe later a more
 * sophisticated interface will be provided which has the necessary information
 * about worst case affected areas.
 *
 * Per-disk interface. Optional, needed if the disk is accessed concurrently
 * by several threads, e.g. when merging diff images while a VM is running.
 */
typedef struct VDINTERFACETHREADSYNC
{
    /**
     * Size of the thread synchronization interface.
     */
    uint32_t    cbSize;

    /**
     * Interface type.
     */
    VDINTERFACETYPE enmInterface;

    /**
     * Start a read operation.
     */
    DECLR3CALLBACKMEMBER(int, pfnStartRead, (void *pvUser));

    /**
     * Finish a read operation.
     */
    DECLR3CALLBACKMEMBER(int, pfnFinishRead, (void *pvUser));

    /**
     * Start a write operation.
     */
    DECLR3CALLBACKMEMBER(int, pfnStartWrite, (void *pvUser));

    /**
     * Finish a write operation.
     */
    DECLR3CALLBACKMEMBER(int, pfnFinishWrite, (void *pvUser));

} VDINTERFACETHREADSYNC, *PVDINTERFACETHREADSYNC;

/**
 * Get thread synchronization interface from opaque callback table.
 *
 * @return Pointer to the callback table.
 * @param  pInterface Pointer to the interface descriptor.
 */
DECLINLINE(PVDINTERFACETHREADSYNC) VDGetInterfaceThreadSync(PVDINTERFACE pInterface)
{
    PVDINTERFACETHREADSYNC pInterfaceThreadSync;

    /* Check that the interface descriptor is a thread synchronization interface. */
    AssertMsgReturn(   (pInterface->enmInterface == VDINTERFACETYPE_THREADSYNC)
                    && (pInterface->cbSize == sizeof(VDINTERFACE)),
                    ("Not a thread synchronization interface"), NULL);

    pInterfaceThreadSync = (PVDINTERFACETHREADSYNC)pInterface->pCallbacks;

    /* Do basic checks. */
    AssertMsgReturn(   (pInterfaceThreadSync->cbSize == sizeof(VDINTERFACETHREADSYNC))
                    && (pInterfaceThreadSync->enmInterface == VDINTERFACETYPE_THREADSYNC),
                    ("A non thread synchronization callback table attached to a thread synchronization interface descriptor\n"), NULL);

    return pInterfaceThreadSync;
}

/** @name Configuration interface key handling flags.
 * @{
 */
/** Mandatory config key. Not providing a value for this key will cause
 * the backend to fail. */
#define VD_CFGKEY_MANDATORY         RT_BIT(0)
/** Expert config key. Not showing it by default in the GUI is is probably
 * a good idea, as the average user won't understand it easily. */
#define VD_CFGKEY_EXPERT            RT_BIT(1)
/** @}*/


/**
 * Configuration value type for configuration information interface.
 */
typedef enum VDCFGVALUETYPE
{
    /** Integer value. */
    VDCFGVALUETYPE_INTEGER = 1,
    /** String value. */
    VDCFGVALUETYPE_STRING,
    /** Bytestring value. */
    VDCFGVALUETYPE_BYTES
} VDCFGVALUETYPE;


/**
 * Structure describing configuration keys required/supported by a backend
 * through the config interface.
 */
typedef struct VDCONFIGINFO
{
    /** Key name of the configuration. */
    const char *pszKey;
    /** Pointer to default value (descriptor). NULL if no useful default value
     * can be specified. */
    const char *pszDefaultValue;
    /** Value type for this key. */
    VDCFGVALUETYPE enmValueType;
    /** Key handling flags (a combination of VD_CFGKEY_* flags). */
    uint64_t uKeyFlags;
} VDCONFIGINFO;

/** Pointer to structure describing configuration keys. */
typedef VDCONFIGINFO *PVDCONFIGINFO;

/** Pointer to const structure describing configuration keys. */
typedef const VDCONFIGINFO *PCVDCONFIGINFO;

/**
 * Structure describing a file extension.
 */
typedef struct VDFILEEXTENSION
{
    /** Pointer to the NULL-terminated string containing the extension. */
    const char *pszExtension;
    /** The device type the extension supports. */
    VDTYPE      enmType;
} VDFILEEXTENSION;

/** Pointer to a structure describing a file extension. */
typedef VDFILEEXTENSION *PVDFILEEXTENSION;

/** Pointer to a const structure describing a file extension. */
typedef const VDFILEEXTENSION *PCVDFILEEXTENSION;

/**
 * Data structure for returning a list of backend capabilities.
 */
typedef struct VDBACKENDINFO
{
    /** Name of the backend. Must be unique even with case insensitive comparison. */
    const char *pszBackend;
    /** Capabilities of the backend (a combination of the VD_CAP_* flags). */
    uint64_t uBackendCaps;
    /** Pointer to a NULL-terminated array of strings, containing the supported
     * file extensions. Note that some backends do not work on files, so this
     * pointer may just contain NULL. */
    PCVDFILEEXTENSION paFileExtensions;
    /** Pointer to an array of structs describing each supported config key.
     * Terminated by a NULL config key. Note that some backends do not support
     * the configuration interface, so this pointer may just contain NULL.
     * Mandatory if the backend sets VD_CAP_CONFIG. */
    PCVDCONFIGINFO paConfigInfo;
    /** Returns a human readable hard disk location string given a
     *  set of hard disk configuration keys. The returned string is an
     *  equivalent of the full file path for image-based hard disks.
     *  Mandatory for backends with no VD_CAP_FILE and NULL otherwise. */
    DECLR3CALLBACKMEMBER(int, pfnComposeLocation, (PVDINTERFACE pConfig, char **pszLocation));
    /** Returns a human readable hard disk name string given a
     *  set of hard disk configuration keys. The returned string is an
     *  equivalent of the file name part in the full file path for
     *  image-based hard disks. Mandatory for backends with no
     *  VD_CAP_FILE and NULL otherwise. */
    DECLR3CALLBACKMEMBER(int, pfnComposeName, (PVDINTERFACE pConfig, char **pszName));
} VDBACKENDINFO, *PVDBACKENDINFO;


/** Forward declaration. Only visible in the VBoxHDD module. */
/** I/O context */
typedef struct VDIOCTX *PVDIOCTX;
/** Storage backend handle. */
typedef struct VDIOSTORAGE *PVDIOSTORAGE;
/** Pointer to a storage backend handle. */
typedef PVDIOSTORAGE *PPVDIOSTORAGE;

/**
 * Completion callback for meta/userdata reads or writes.
 *
 * @return  VBox status code.
 *          VINF_SUCCESS if everything was successful and the transfer can continue.
 *          VERR_VD_ASYNC_IO_IN_PROGRESS if there is another data transfer pending.
 * @param   pBackendData    The opaque backend data.
 * @param   pIoCtx          I/O context associated with this request.
 * @param   pvUser          Opaque user data passed during a read/write request.
 * @param   rcReq           Status code for the completed request.
 */
typedef DECLCALLBACK(int) FNVDXFERCOMPLETED(void *pBackendData, PVDIOCTX pIoCtx, void *pvUser, int rcReq);
/** Pointer to FNVDXFERCOMPLETED() */
typedef FNVDXFERCOMPLETED *PFNVDXFERCOMPLETED;

/** Metadata transfer handle. */
typedef struct VDMETAXFER *PVDMETAXFER;
/** Pointer to a metadata transfer handle. */
typedef PVDMETAXFER *PPVDMETAXFER;


/**
 * Internal I/O interface between the generic VD layer and the backends.
 *
 * Per-image. Always passed to backends.
 */
typedef struct VDINTERFACEIOINT
{
    /**
     * Size of the internal I/O interface.
     */
    uint32_t    cbSize;

    /**
     * Interface type.
     */
    VDINTERFACETYPE enmInterface;

    /**
     * Open callback
     *
     * @return  VBox status code.
     * @param   pvUser          The opaque data passed on container creation.
     * @param   pszLocation     Name of the location to open.
     * @param   fOpen           Flags for opening the backend.
     *                          See RTFILE_O_* #defines, inventing another set
     *                          of open flags is not worth the mapping effort.
     * @param   ppStorage       Where to store the storage handle.
     */
    DECLR3CALLBACKMEMBER(int, pfnOpen, (void *pvUser, const char *pszLocation,
                                        uint32_t fOpen, PPVDIOSTORAGE ppStorage));

    /**
     * Close callback.
     *
     * @return  VBox status code.
     * @param   pvUser          The opaque data passed on container creation.
     * @param   pStorage        The storage handle to close.
     */
    DECLR3CALLBACKMEMBER(int, pfnClose, (void *pvUser, PVDIOSTORAGE pStorage));

    /**
     * Delete callback.
     *
     * @return  VBox status code.
     * @param   pvUser          The opaque data passed on container creation.
     * @param   pcszFilename    Name of the file to delete.
     */
    DECLR3CALLBACKMEMBER(int, pfnDelete, (void *pvUser, const char *pcszFilename));

    /**
     * Move callback.
     *
     * @return  VBox status code.
     * @param   pvUser          The opaque data passed on container creation.
     * @param   pcszSrc         The path to the source file.
     * @param   pcszDst         The path to the destination file.
     *                          This file will be created.
     * @param   fMove           A combination of the RTFILEMOVE_* flags.
     */
    DECLR3CALLBACKMEMBER(int, pfnMove, (void *pvUser, const char *pcszSrc, const char *pcszDst, unsigned fMove));

    /**
     * Returns the free space on a disk.
     *
     * @return  VBox status code.
     * @param   pvUser          The opaque data passed on container creation.
     * @param   pcszFilename    Name of a file to identify the disk.
     * @param   pcbFreeSpace    Where to store the free space of the disk.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetFreeSpace, (void *pvUser, const char *pcszFilename, int64_t *pcbFreeSpace));

    /**
     * Returns the last modification timestamp of a file.
     *
     * @return  VBox status code.
     * @param   pvUser          The opaque data passed on container creation.
     * @param   pcszFilename    Name of a file to identify the disk.
     * @param   pModificationTime   Where to store the timestamp of the file.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetModificationTime, (void *pvUser, const char *pcszFilename, PRTTIMESPEC pModificationTime));

    /**
     * Returns the size of the opened storage backend.
     *
     * @return  VBox status code.
     * @param   pvUser          The opaque data passed on container creation.
     * @param   pStorage        The storage handle to get the size from.
     * @param   pcbSize         Where to store the size of the storage backend.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetSize, (void *pvUser, PVDIOSTORAGE pStorage,
                                           uint64_t *pcbSize));

    /**
     * Sets the size of the opened storage backend if possible.
     *
     * @return  VBox status code.
     * @retval  VERR_NOT_SUPPORTED if the backend does not support this operation.
     * @param   pvUser          The opaque data passed on container creation.
     * @param   pStorage        The storage handle.
     * @param   cbSize          The new size of the image.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetSize, (void *pvUser, PVDIOSTORAGE pStorage,
                                           uint64_t cbSize));

    /**
     * Synchronous write callback.
     *
     * @return  VBox status code.
     * @param   pvUser          The opaque data passed on container creation.
     * @param   pStorage        The storage handle to use.
     * @param   uOffset         The offset to start from.
     * @param   pvBuffer        Pointer to the bits need to be written.
     * @param   cbBuffer        How many bytes to write.
     * @param   pcbWritten      Where to store how many bytes were actually written.
     *
     * @notes Do not use in code called from the async read/write entry points in the backends.
     *        This should be only used during open/close of images and for the support functions
     *        which are not called while a VM is running (pfnCompact).
     */
    DECLR3CALLBACKMEMBER(int, pfnWriteSync, (void *pvUser, PVDIOSTORAGE pStorage, uint64_t uOffset,
                                             const void *pvBuffer, size_t cbBuffer, size_t *pcbWritten));

    /**
     * Synchronous read callback.
     *
     * @return  VBox status code.
     * @param   pvUser          The opaque data passed on container creation.
     * @param   pStorage        The storage handle to use.
     * @param   uOffset         The offset to start from.
     * @param   pvBuffer        Where to store the read bits.
     * @param   cbBuffer        How many bytes to read.
     * @param   pcbRead         Where to store how many bytes were actually read.
     *
     * @notes See pfnWriteSync()
     */
    DECLR3CALLBACKMEMBER(int, pfnReadSync, (void *pvUser, PVDIOSTORAGE pStorage, uint64_t uOffset,
                                            void *pvBuffer, size_t cbBuffer, size_t *pcbRead));

    /**
     * Flush data to the storage backend.
     *
     * @return  VBox status code.
     * @param   pvUser          The opaque data passed on container creation.
     * @param   pStorage        The storage handle to flush.
     *
     * @notes See pfnWriteSync()
     */
    DECLR3CALLBACKMEMBER(int, pfnFlushSync, (void *pvUser, PVDIOSTORAGE pStorage));

    /**
     * Initiate an asynchronous read request for user data.
     *
     * @return  VBox status code.
     * @param   pvUser         The opaque user data passed on container creation.
     * @param   pStorage       The storage handle.
     * @param   uOffset        The offset to start reading from.
     * @param   pIoCtx         I/O context passed in VDAsyncRead/Write.
     * @param   cbRead         How many bytes to read.
     */
    DECLR3CALLBACKMEMBER(int, pfnReadUserAsync, (void *pvUser, PVDIOSTORAGE pStorage,
                                                 uint64_t uOffset, PVDIOCTX pIoCtx,
                                                 size_t cbRead));

    /**
     * Initiate an asynchronous write request for user data.
     *
     * @return  VBox status code.
     * @param   pvUser         The opaque user data passed on container creation.
     * @param   pStorage       The storage handle.
     * @param   uOffset        The offset to start writing to.
     * @param   pIoCtx         I/O context passed in VDAsyncRead/Write
     * @param   cbWrite        How many bytes to write.
     * @param   pfnCompleted   Completion callback.
     * @param   pvCompleteUser Opaque user data passed in the completion callback.
     */
    DECLR3CALLBACKMEMBER(int, pfnWriteUserAsync, (void *pvUser, PVDIOSTORAGE pStorage,
                                                  uint64_t uOffset, PVDIOCTX pIoCtx,
                                                  size_t cbWrite,
                                                  PFNVDXFERCOMPLETED pfnComplete,
                                                  void *pvCompleteUser));

    /**
     * Reads metadata asynchronously from storage.
     * The current I/O context will be halted.
     *
     * @returns VBox status code.
     * @param   pvUser         The opaque user data passed on container creation.
     * @param   pStorage       The storage handle.
     * @param   uOffset        Offset to start reading from.
     * @param   pvBuffer       Where to store the data.
     * @param   cbBuffer       How many bytes to read.
     * @param   pIoCtx         The I/O context which triggered the read.
     * @param   ppMetaXfer     Where to store the metadata transfer handle on success.
     * @param   pfnCompleted   Completion callback.
     * @param   pvCompleteUser Opaque user data passed in the completion callback.
     */
    DECLR3CALLBACKMEMBER(int, pfnReadMetaAsync, (void *pvUser, PVDIOSTORAGE pStorage,
                                                 uint64_t uOffset, void *pvBuffer,
                                                 size_t cbBuffer, PVDIOCTX pIoCtx,
                                                 PPVDMETAXFER ppMetaXfer,
                                                 PFNVDXFERCOMPLETED pfnComplete,
                                                 void *pvCompleteUser));

    /**
     * Writes metadata asynchronously to storage.
     *
     * @returns VBox status code.
     * @param   pvUser         The opaque user data passed on container creation.
     * @param   pStorage       The storage handle.
     * @param   uOffset        Offset to start writing to.
     * @param   pvBuffer       Written data.
     * @param   cbBuffer       How many bytes to write.
     * @param   pIoCtx         The I/O context which triggered the write.
     * @param   pfnCompleted   Completion callback.
     * @param   pvCompleteUser Opaque user data passed in the completion callback.
     */
    DECLR3CALLBACKMEMBER(int, pfnWriteMetaAsync, (void *pvUser, PVDIOSTORAGE pStorage,
                                                  uint64_t uOffset, void *pvBuffer,
                                                  size_t cbBuffer, PVDIOCTX pIoCtx,
                                                  PFNVDXFERCOMPLETED pfnComplete,
                                                  void *pvCompleteUser));

    /**
     * Releases a metadata transfer handle.
     * The free space can be used for another transfer.
     *
     * @returns nothing.
     * @param   pvUser         The opaque user data passed on container creation.
     * @param   pMetaXfer      The metadata transfer handle to release.
     */
    DECLR3CALLBACKMEMBER(void, pfnMetaXferRelease, (void *pvUser, PVDMETAXFER pMetaXfer));

    /**
     * Initiates an async flush request.
     *
     * @return  VBox status code.
     * @param   pvUser         The opaque data passed on container creation.
     * @param   pStorage       The storage handle to flush.
     * @param   pIoCtx         I/O context which triggered the flush.
     * @param   pfnCompleted   Completion callback.
     * @param   pvCompleteUser Opaque user data passed in the completion callback.
     */
    DECLR3CALLBACKMEMBER(int, pfnFlushAsync, (void *pvUser, PVDIOSTORAGE pStorage,
                                              PVDIOCTX pIoCtx,
                                              PFNVDXFERCOMPLETED pfnComplete,
                                              void *pvCompleteUser));

    /**
     * Copies a buffer into the I/O context.
     *
     * @return Number of bytes copied.
     * @param  pvUser          The opaque user data passed on container creation.
     * @param  pIoCtx          I/O context to copy the data to.
     * @param  pvBuffer        Buffer to copy.
     * @param  cbBuffer        Number of bytes to copy.
     */
    DECLR3CALLBACKMEMBER(size_t, pfnIoCtxCopyTo, (void *pvUser, PVDIOCTX pIoCtx,
                                                  void *pvBuffer, size_t cbBuffer));

    /**
     * Copies data from the I/O context into a buffer.
     *
     * @return Number of bytes copied.
     * @param  pvUser          The opaque user data passed on container creation.
     * @param  pIoCtx          I/O context to copy the data from.
     * @param  pvBuffer        Destination buffer.
     * @param  cbBuffer        Number of bytes to copy.
     */
    DECLR3CALLBACKMEMBER(size_t, pfnIoCtxCopyFrom, (void *pvUser, PVDIOCTX pIoCtx,
                                                    void *pvBuffer, size_t cbBuffer));

    /**
     * Sets the buffer of the given context to a specific byte.
     *
     * @return Number of bytes set.
     * @param  pvUser          The opaque user data passed on container creation.
     * @param  pIoCtx          I/O context to copy the data from.
     * @param  ch              The byte to set.
     * @param  cbSet           Number of bytes to set.
     */
    DECLR3CALLBACKMEMBER(size_t, pfnIoCtxSet, (void *pvUser, PVDIOCTX pIoCtx,
                                               int ch, size_t cbSet));

    /**
     * Creates a segment array from the I/O context data buffer.
     *
     * @returns Number of bytes the array describes.
     * @param  pvUser          The opaque user data passed on container creation.
     * @param  pIoCtx          I/O context to copy the data from.
     * @param  paSeg           The uninitialized segment array.
     *                         If NULL pcSeg will contain the number of segments needed
     *                         to describe the requested amount of data.
     * @param  pcSeg           The number of segments the given array has.
     *                         This will hold the actual number of entries needed upon return.
     * @param  cbData          Number of bytes the new array should describe.
     */
    DECLR3CALLBACKMEMBER(size_t, pfnIoCtxSegArrayCreate, (void *pvUser, PVDIOCTX pIoCtx,
                                                          PRTSGSEG paSeg, unsigned *pcSeg,
                                                          size_t cbData));
    /**
     * Marks the given number of bytes as completed and continues the I/O context.
     *
     * @returns nothing.
     * @param   pvUser         The opaque user data passed on container creation.
     * @param   pIoCtx         The I/O context.
     * @param   rcReq          Status code the request completed with.
     * @param   cbCompleted    Number of bytes completed.
     */
    DECLR3CALLBACKMEMBER(void, pfnIoCtxCompleted, (void *pvUser, PVDIOCTX pIoCtx,
                                                   int rcReq, size_t cbCompleted));
} VDINTERFACEIOINT, *PVDINTERFACEIOINT;

/**
 * Get internal I/O interface from opaque callback table.
 *
 * @return Pointer to the callback table.
 * @param  pInterface Pointer to the interface descriptor.
 */
DECLINLINE(PVDINTERFACEIOINT) VDGetInterfaceIOInt(PVDINTERFACE pInterface)
{
    PVDINTERFACEIOINT pInterfaceIOInt;

    /* Check that the interface descriptor is an internal I/O interface. */
    AssertMsgReturn(   (pInterface->enmInterface == VDINTERFACETYPE_IOINT)
                    && (pInterface->cbSize == sizeof(VDINTERFACE)),
                    ("Not an internal I/O interface"), NULL);

    pInterfaceIOInt = (PVDINTERFACEIOINT)pInterface->pCallbacks;

    /* Do basic checks. */
    AssertMsgReturn(   (pInterfaceIOInt->cbSize == sizeof(VDINTERFACEIOINT))
                    && (pInterfaceIOInt->enmInterface == VDINTERFACETYPE_IOINT),
                    ("A non internal I/O callback table attached to an internal I/O interface descriptor\n"), NULL);

    return pInterfaceIOInt;
}

/**
 * Request completion callback for the async read/write API.
 */
typedef void (FNVDASYNCTRANSFERCOMPLETE) (void *pvUser1, void *pvUser2, int rcReq);
/** Pointer to a transfer compelte callback. */
typedef FNVDASYNCTRANSFERCOMPLETE *PFNVDASYNCTRANSFERCOMPLETE;

/**
 * Disk geometry.
 */
typedef struct VDGEOMETRY
{
    /** Number of cylinders. */
    uint32_t    cCylinders;
    /** Number of heads. */
    uint32_t    cHeads;
    /** Number of sectors. */
    uint32_t    cSectors;
} VDGEOMETRY;

/** Pointer to disk geometry. */
typedef VDGEOMETRY *PVDGEOMETRY;
/** Pointer to constant disk geometry. */
typedef const VDGEOMETRY *PCVDGEOMETRY;

/**
 * VBox HDD Container main structure.
 */
/* Forward declaration, VBOXHDD structure is visible only inside VBox HDD module. */
struct VBOXHDD;
typedef struct VBOXHDD VBOXHDD;
typedef VBOXHDD *PVBOXHDD;

/**
 * Initializes HDD backends.
 *
 * @returns VBox status code.
 */
VBOXDDU_DECL(int) VDInit(void);

/**
 * Destroys loaded HDD backends.
 *
 * @returns VBox status code.
 */
VBOXDDU_DECL(int) VDShutdown(void);

/**
 * Lists all HDD backends and their capabilities in a caller-provided buffer.
 *
 * @return  VBox status code.
 *          VERR_BUFFER_OVERFLOW if not enough space is passed.
 * @param   cEntriesAlloc   Number of list entries available.
 * @param   pEntries        Pointer to array for the entries.
 * @param   pcEntriesUsed   Number of entries returned.
 */
VBOXDDU_DECL(int) VDBackendInfo(unsigned cEntriesAlloc, PVDBACKENDINFO pEntries,
                                unsigned *pcEntriesUsed);

/**
 * Lists the capabilities of a backend identified by its name.
 *
 * @return  VBox status code.
 * @param   pszBackend      The backend name (case insensitive).
 * @param   pEntries        Pointer to an entry.
 */
VBOXDDU_DECL(int) VDBackendInfoOne(const char *pszBackend, PVDBACKENDINFO pEntry);

/**
 * Allocates and initializes an empty HDD container.
 * No image files are opened.
 *
 * @return  VBox status code.
 * @param   pVDIfsDisk      Pointer to the per-disk VD interface list.
 * @param   enmType         Type of the image container.
 * @param   ppDisk          Where to store the reference to HDD container.
 */
VBOXDDU_DECL(int) VDCreate(PVDINTERFACE pVDIfsDisk, VDTYPE enmType, PVBOXHDD *ppDisk);

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
 * @return  VBox status code.
 *          VINF_SUCCESS if a plugin was found.
 *                       ppszFormat contains the string which can be used as backend name.
 *          VERR_NOT_SUPPORTED if no backend was found.
 * @param   pVDIfsDisk      Pointer to the per-disk VD interface list.
 * @param   pVDIfsImage     Pointer to the per-image VD interface list.
 * @param   pszFilename     Name of the image file for which the backend is queried.
 * @param   ppszFormat      Receives pointer of the UTF-8 string which contains the format name.
 *                          The returned pointer must be freed using RTStrFree().
 * @param   penmType        Where to store the type of the image.
 */
VBOXDDU_DECL(int) VDGetFormat(PVDINTERFACE pVDIfsDisk, PVDINTERFACE pVDIfsImage,
                              const char *pszFilename, char **ppszFormat, VDTYPE *penmType);

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
 * @return  VBox status code.
 * @param   pDisk           Pointer to HDD container.
 * @param   pszBackend      Name of the image file backend to use (case insensitive).
 * @param   pszFilename     Name of the image file to open.
 * @param   uOpenFlags      Image file open mode, see VD_OPEN_FLAGS_* constants.
 * @param   pVDIfsImage     Pointer to the per-image VD interface list.
 */
VBOXDDU_DECL(int) VDOpen(PVBOXHDD pDisk, const char *pszBackend,
                         const char *pszFilename, unsigned uOpenFlags,
                         PVDINTERFACE pVDIfsImage);

/**
 * Opens a cache image.
 *
 * @return  VBox status code.
 * @param   pDisk           Pointer to the HDD container which should use the cache image.
 * @param   pszBackend      Name of the cache file backend to use (case insensitive).
 * @param   pszFilename     Name of the cache image to open.
 * @param   uOpenFlags      Image file open mode, see VD_OPEN_FLAGS_* constants.
 * @param   pVDIfsCache     Pointer to the per-cache VD interface list.
 */
VBOXDDU_DECL(int) VDCacheOpen(PVBOXHDD pDisk, const char *pszBackend,
                              const char *pszFilename, unsigned uOpenFlags,
                              PVDINTERFACE pVDIfsCache);

/**
 * Creates and opens a new base image file.
 *
 * @return  VBox status code.
 * @param   pDisk           Pointer to HDD container.
 * @param   pszBackend      Name of the image file backend to use (case insensitive).
 * @param   pszFilename     Name of the image file to create.
 * @param   cbSize          Image size in bytes.
 * @param   uImageFlags     Flags specifying special image features.
 * @param   pszComment      Pointer to image comment. NULL is ok.
 * @param   pPCHSGeometry   Pointer to physical disk geometry <= (16383,16,63). Not NULL.
 * @param   pLCHSGeometry   Pointer to logical disk geometry <= (x,255,63). Not NULL.
 * @param   pUuid           New UUID of the image. If NULL, a new UUID is created.
 * @param   uOpenFlags      Image file open mode, see VD_OPEN_FLAGS_* constants.
 * @param   pVDIfsImage     Pointer to the per-image VD interface list.
 * @param   pVDIfsOperation Pointer to the per-operation VD interface list.
 */
VBOXDDU_DECL(int) VDCreateBase(PVBOXHDD pDisk, const char *pszBackend,
                               const char *pszFilename, uint64_t cbSize,
                               unsigned uImageFlags, const char *pszComment,
                               PCVDGEOMETRY pPCHSGeometry,
                               PCVDGEOMETRY pLCHSGeometry,
                               PCRTUUID pUuid, unsigned uOpenFlags,
                               PVDINTERFACE pVDIfsImage,
                               PVDINTERFACE pVDIfsOperation);

/**
 * Creates and opens a new differencing image file in HDD container.
 * See comments for VDOpen function about differencing images.
 *
 * @return  VBox status code.
 * @param   pDisk           Pointer to HDD container.
 * @param   pszBackend      Name of the image file backend to use (case insensitive).
 * @param   pszFilename     Name of the differencing image file to create.
 * @param   uImageFlags     Flags specifying special image features.
 * @param   pszComment      Pointer to image comment. NULL is ok.
 * @param   pUuid           New UUID of the image. If NULL, a new UUID is created.
 * @param   pParentUuid     New parent UUID of the image. If NULL, the UUID is queried automatically.
 * @param   uOpenFlags      Image file open mode, see VD_OPEN_FLAGS_* constants.
 * @param   pVDIfsImage     Pointer to the per-image VD interface list.
 * @param   pVDIfsOperation Pointer to the per-operation VD interface list.
 */
VBOXDDU_DECL(int) VDCreateDiff(PVBOXHDD pDisk, const char *pszBackend,
                               const char *pszFilename, unsigned uImageFlags,
                               const char *pszComment, PCRTUUID pUuid,
                               PCRTUUID pParentUuid, unsigned uOpenFlags,
                               PVDINTERFACE pVDIfsImage,
                               PVDINTERFACE pVDIfsOperation);

/**
 * Creates and opens new cache image file in HDD container.
 *
 * @return  VBox status code.
 * @param   pDisk           Name of the cache file backend to use (case insensitive).
 * @param   pszFilename     Name of the differencing cache file to create.
 * @param   cbSize          Maximum size of the cache.
 * @param   uImageFlags     Flags specifying special cache features.
 * @param   pszComment      Pointer to image comment. NULL is ok.
 * @param   pUuid           New UUID of the image. If NULL, a new UUID is created.
 * @param   uOpenFlags      Image file open mode, see VD_OPEN_FLAGS_* constants.
 * @param   pVDIfsCache     Pointer to the per-cache VD interface list.
 * @param   pVDIfsOperation Pointer to the per-operation VD interface list.
 */
VBOXDDU_DECL(int) VDCreateCache(PVBOXHDD pDisk, const char *pszBackend,
                                const char *pszFilename, uint64_t cbSize,
                                unsigned uImageFlags, const char *pszComment,
                                PCRTUUID pUuid, unsigned uOpenFlags,
                                PVDINTERFACE pVDIfsCache, PVDINTERFACE pVDIfsOperation);

/**
 * Merges two images (not necessarily with direct parent/child relationship).
 * As a side effect the source image and potentially the other images which
 * are also merged to the destination are deleted from both the disk and the
 * images in the HDD container.
 *
 * @return  VBox status code.
 * @return  VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImageFrom      Image number to merge from, counts from 0. 0 is always base image of container.
 * @param   nImageTo        Image number to merge to, counts from 0. 0 is always base image of container.
 * @param   pVDIfsOperation Pointer to the per-operation VD interface list.
 */
VBOXDDU_DECL(int) VDMerge(PVBOXHDD pDisk, unsigned nImageFrom,
                          unsigned nImageTo, PVDINTERFACE pVDIfsOperation);

/**
 * Copies an image from one HDD container to another - extended version.
 * The copy is opened in the target HDD container.
 * It is possible to convert between different image formats, because the
 * backend for the destination may be different from the source.
 * If both the source and destination reference the same HDD container,
 * then the image is moved (by copying/deleting or renaming) to the new location.
 * The source container is unchanged if the move operation fails, otherwise
 * the image at the new location is opened in the same way as the old one was.
 *
 * @note The read/write accesses across disks are not synchronized, just the
 * accesses to each disk. Once there is a use case which requires a defined
 * read/write behavior in this situation this needs to be extended.
 *
 * @return  VBox status code.
 * @return  VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDiskFrom       Pointer to source HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pDiskTo         Pointer to destination HDD container.
 * @param   pszBackend      Name of the image file backend to use (may be NULL to use the same as the source, case insensitive).
 * @param   pszFilename     New name of the image (may be NULL to specify that the
 *                          copy destination is the destination container, or
 *                          if pDiskFrom == pDiskTo, i.e. when moving).
 * @param   fMoveByRename   If true, attempt to perform a move by renaming (if successful the new size is ignored).
 * @param   cbSize          New image size (0 means leave unchanged).
 * @param   nImageSameFrom  The number of the last image in the source chain having the same content as the
 *                          image in the destination chain given by nImageSameTo or
 *                          VD_IMAGE_CONTENT_UNKNOWN to indicate that the content of both containers is unknown.
 *                          See the notes for further information.
 * @param   nImageSameTo    The number of the last image in the destination chain having the same content as the
 *                          image in the source chain given by nImageSameFrom or
 *                          VD_IMAGE_CONTENT_UNKNOWN to indicate that the content of both containers is unknown.
 *                          See the notes for further information.
 * @param   uImageFlags     Flags specifying special destination image features.
 * @param   pDstUuid        New UUID of the destination image. If NULL, a new UUID is created.
 *                          This parameter is used if and only if a true copy is created.
 *                          In all rename/move cases or copy to existing image cases the modification UUIDs are copied over.
 * @param   uOpenFlags      Image file open mode, see VD_OPEN_FLAGS_* constants.
 *                          Only used if the destination image is created.
 * @param   pVDIfsOperation Pointer to the per-operation VD interface list.
 * @param   pDstVDIfsImage  Pointer to the per-image VD interface list, for the
 *                          destination image.
 * @param   pDstVDIfsOperation Pointer to the per-operation VD interface list,
 *                          for the destination operation.
 *
 * @note Using nImageSameFrom and nImageSameTo can lead to a significant speedup
 *       when copying an image but can also lead to a corrupted copy if used incorrectly.
 *       It is mainly useful when cloning a chain of images and it is known that
 *       the virtual disk content of the two chains is exactly the same upto a certain image.
 *       Example:
 *          Imagine the chain of images which consist of a base and one diff image.
 *          Copying the chain starts with the base image. When copying the first
 *          diff image VDCopy() will read the data from the diff of the source chain
 *          and probably from the base image again in case the diff doesn't has data
 *          for the block. However the block will be optimized away because VDCopy()
 *          reads data from the base image of the destination chain compares the to
 *          and suppresses the write because the data is unchanged.
 *          For a lot of diff images this will be a huge waste of I/O bandwidth if
 *          the diff images contain only few changes.
 *          Because it is known that the base image of the source and the destination chain
 *          have the same content it is enough to check the diff image for changed data
 *          and copy it to the destination diff image which is achieved with
 *          nImageSameFrom and nImageSameTo. Setting both to 0 can suppress a lot of I/O.
 */
VBOXDDU_DECL(int) VDCopyEx(PVBOXHDD pDiskFrom, unsigned nImage, PVBOXHDD pDiskTo,
                           const char *pszBackend, const char *pszFilename,
                           bool fMoveByRename, uint64_t cbSize,
                           unsigned nImageFromSame, unsigned nImageToSame,
                           unsigned uImageFlags, PCRTUUID pDstUuid,
                           unsigned uOpenFlags, PVDINTERFACE pVDIfsOperation,
                           PVDINTERFACE pDstVDIfsImage,
                           PVDINTERFACE pDstVDIfsOperation);

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
 * @note The read/write accesses across disks are not synchronized, just the
 * accesses to each disk. Once there is a use case which requires a defined
 * read/write behavior in this situation this needs to be extended.
 *
 * @return  VBox status code.
 * @return  VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDiskFrom       Pointer to source HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pDiskTo         Pointer to destination HDD container.
 * @param   pszBackend      Name of the image file backend to use (may be NULL to use the same as the source, case insensitive).
 * @param   pszFilename     New name of the image (may be NULL to specify that the
 *                          copy destination is the destination container, or
 *                          if pDiskFrom == pDiskTo, i.e. when moving).
 * @param   fMoveByRename   If true, attempt to perform a move by renaming (if successful the new size is ignored).
 * @param   cbSize          New image size (0 means leave unchanged).
 * @param   uImageFlags     Flags specifying special destination image features.
 * @param   pDstUuid        New UUID of the destination image. If NULL, a new UUID is created.
 *                          This parameter is used if and only if a true copy is created.
 *                          In all rename/move cases or copy to existing image cases the modification UUIDs are copied over.
 * @param   uOpenFlags      Image file open mode, see VD_OPEN_FLAGS_* constants.
 *                          Only used if the destination image is created.
 * @param   pVDIfsOperation Pointer to the per-operation VD interface list.
 * @param   pDstVDIfsImage  Pointer to the per-image VD interface list, for the
 *                          destination image.
 * @param   pDstVDIfsOperation Pointer to the per-operation VD interface list,
 *                          for the destination operation.
 */
VBOXDDU_DECL(int) VDCopy(PVBOXHDD pDiskFrom, unsigned nImage, PVBOXHDD pDiskTo,
                         const char *pszBackend, const char *pszFilename,
                         bool fMoveByRename, uint64_t cbSize,
                         unsigned uImageFlags, PCRTUUID pDstUuid,
                         unsigned uOpenFlags, PVDINTERFACE pVDIfsOperation,
                         PVDINTERFACE pDstVDIfsImage,
                         PVDINTERFACE pDstVDIfsOperation);

/**
 * Optimizes the storage consumption of an image. Typically the unused blocks
 * have to be wiped with zeroes to achieve a substantial reduced storage use.
 * Another optimization done is reordering the image blocks, which can provide
 * a significant performance boost, as reads and writes tend to use less random
 * file offsets.
 *
 * @note Compaction is treated as a single operation with regard to thread
 * synchronization, which means that it potentially blocks other activities for
 * a long time. The complexity of compaction would grow even more if concurrent
 * accesses have to be handled.
 *
 * @return  VBox status code.
 * @return  VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @return  VERR_VD_IMAGE_READ_ONLY if image is not writable.
 * @return  VERR_NOT_SUPPORTED if this kind of image can be compacted, but
 *                             this isn't supported yet.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pVDIfsOperation Pointer to the per-operation VD interface list.
 */
VBOXDDU_DECL(int) VDCompact(PVBOXHDD pDisk, unsigned nImage,
                            PVDINTERFACE pVDIfsOperation);

/**
 * Resizes the given disk image to the given size.
 *
 * @return  VBox status
 * @return  VERR_VD_IMAGE_READ_ONLY if image is not writable.
 * @return  VERR_NOT_SUPPORTED if this kind of image can be compacted, but
 *
 * @param   pDisk           Pointer to the HDD container.
 * @param   cbSize          New size of the image.
 * @param   pPCHSGeometry   Pointer to the new physical disk geometry <= (16383,16,63). Not NULL.
 * @param   pLCHSGeometry   Pointer to the new logical disk geometry <= (x,255,63). Not NULL.
 * @param   pVDIfsOperation Pointer to the per-operation VD interface list.
 */
VBOXDDU_DECL(int) VDResize(PVBOXHDD pDisk, uint64_t cbSize,
                           PCVDGEOMETRY pPCHSGeometry,
                           PCVDGEOMETRY pLCHSGeometry,
                           PVDINTERFACE pVDIfsOperation);

/**
 * Closes the last opened image file in HDD container.
 * If previous image file was opened in read-only mode (the normal case) and
 * the last opened image is in read-write mode then the previous image will be
 * reopened in read/write mode.
 *
 * @return  VBox status code.
 * @return  VERR_VD_NOT_OPENED if no image is opened in HDD container.
 * @param   pDisk           Pointer to HDD container.
 * @param   fDelete         If true, delete the image from the host disk.
 */
VBOXDDU_DECL(int) VDClose(PVBOXHDD pDisk, bool fDelete);

/**
 * Closes the currently opened cache image file in HDD container.
 *
 * @return  VBox status code.
 * @return  VERR_VD_NOT_OPENED if no cache is opened in HDD container.
 * @param   pDisk           Pointer to HDD container.
 * @param   fDelete         If true, delete the image from the host disk.
 */
VBOXDDU_DECL(int) VDCacheClose(PVBOXHDD pDisk, bool fDelete);

/**
 * Closes all opened image files in HDD container.
 *
 * @return  VBox status code.
 * @param   pDisk           Pointer to HDD container.
 */
VBOXDDU_DECL(int) VDCloseAll(PVBOXHDD pDisk);

/**
 * Read data from virtual HDD.
 *
 * @return  VBox status code.
 * @return  VERR_VD_NOT_OPENED if no image is opened in HDD container.
 * @param   pDisk           Pointer to HDD container.
 * @param   uOffset         Offset of first reading byte from start of disk.
 *                          Must be aligned to a sector boundary.
 * @param   pvBuffer        Pointer to buffer for reading data.
 * @param   cbBuffer        Number of bytes to read.
 *                          Must be aligned to a sector boundary.
 */
VBOXDDU_DECL(int) VDRead(PVBOXHDD pDisk, uint64_t uOffset, void *pvBuffer, size_t cbBuffer);

/**
 * Write data to virtual HDD.
 *
 * @return  VBox status code.
 * @return  VERR_VD_NOT_OPENED if no image is opened in HDD container.
 * @param   pDisk           Pointer to HDD container.
 * @param   uOffset         Offset of first writing byte from start of disk.
 *                          Must be aligned to a sector boundary.
 * @param   pvBuffer        Pointer to buffer for writing data.
 * @param   cbBuffer        Number of bytes to write.
 *                          Must be aligned to a sector boundary.
 */
VBOXDDU_DECL(int) VDWrite(PVBOXHDD pDisk, uint64_t uOffset, const void *pvBuffer, size_t cbBuffer);

/**
 * Make sure the on disk representation of a virtual HDD is up to date.
 *
 * @return  VBox status code.
 * @return  VERR_VD_NOT_OPENED if no image is opened in HDD container.
 * @param   pDisk           Pointer to HDD container.
 */
VBOXDDU_DECL(int) VDFlush(PVBOXHDD pDisk);

/**
 * Get number of opened images in HDD container.
 *
 * @return  Number of opened images for HDD container. 0 if no images have been opened.
 * @param   pDisk           Pointer to HDD container.
 */
VBOXDDU_DECL(unsigned) VDGetCount(PVBOXHDD pDisk);

/**
 * Get read/write mode of HDD container.
 *
 * @return  Virtual disk ReadOnly status.
 * @return  true if no image is opened in HDD container.
 * @param   pDisk           Pointer to HDD container.
 */
VBOXDDU_DECL(bool) VDIsReadOnly(PVBOXHDD pDisk);

/**
 * Get total capacity of an image in HDD container.
 *
 * @return  Virtual disk size in bytes.
 * @return  0 if image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 */
VBOXDDU_DECL(uint64_t) VDGetSize(PVBOXHDD pDisk, unsigned nImage);

/**
 * Get total file size of an image in HDD container.
 *
 * @return  Virtual disk size in bytes.
 * @return  0 if image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 */
VBOXDDU_DECL(uint64_t) VDGetFileSize(PVBOXHDD pDisk, unsigned nImage);

/**
 * Get virtual disk PCHS geometry of an image in HDD container.
 *
 * @return  VBox status code.
 * @return  VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @return  VERR_VD_GEOMETRY_NOT_SET if no geometry present in the HDD container.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pPCHSGeometry   Where to store PCHS geometry. Not NULL.
 */
VBOXDDU_DECL(int) VDGetPCHSGeometry(PVBOXHDD pDisk, unsigned nImage,
                                    PVDGEOMETRY pPCHSGeometry);

/**
 * Store virtual disk PCHS geometry of an image in HDD container.
 *
 * @return  VBox status code.
 * @return  VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pPCHSGeometry   Where to load PCHS geometry from. Not NULL.
 */
VBOXDDU_DECL(int) VDSetPCHSGeometry(PVBOXHDD pDisk, unsigned nImage,
                                    PCVDGEOMETRY pPCHSGeometry);

/**
 * Get virtual disk LCHS geometry of an image in HDD container.
 *
 * @return  VBox status code.
 * @return  VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @return  VERR_VD_GEOMETRY_NOT_SET if no geometry present in the HDD container.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pLCHSGeometry   Where to store LCHS geometry. Not NULL.
 */
VBOXDDU_DECL(int) VDGetLCHSGeometry(PVBOXHDD pDisk, unsigned nImage,
                                    PVDGEOMETRY pLCHSGeometry);

/**
 * Store virtual disk LCHS geometry of an image in HDD container.
 *
 * @return  VBox status code.
 * @return  VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pLCHSGeometry   Where to load LCHS geometry from. Not NULL.
 */
VBOXDDU_DECL(int) VDSetLCHSGeometry(PVBOXHDD pDisk, unsigned nImage,
                                    PCVDGEOMETRY pLCHSGeometry);

/**
 * Get version of image in HDD container.
 *
 * @return  VBox status code.
 * @return  VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   puVersion       Where to store the image version.
 */
VBOXDDU_DECL(int) VDGetVersion(PVBOXHDD pDisk, unsigned nImage,
                               unsigned *puVersion);

/**
 * List the capabilities of image backend in HDD container.
 *
 * @return  VBox status code.
 * @return  VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to the HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pbackendInfo    Where to store the backend information.
 */
VBOXDDU_DECL(int) VDBackendInfoSingle(PVBOXHDD pDisk, unsigned nImage,
                                      PVDBACKENDINFO pBackendInfo);

/**
 * Get flags of image in HDD container.
 *
 * @return  VBox status code.
 * @return  VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   puImageFlags    Where to store the image flags.
 */
VBOXDDU_DECL(int) VDGetImageFlags(PVBOXHDD pDisk, unsigned nImage, unsigned *puImageFlags);

/**
 * Get open flags of image in HDD container.
 *
 * @return  VBox status code.
 * @return  VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
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
 * @return  VBox status code.
 * @return  VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
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
 * @return  VBox status code.
 * @return  VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @return  VERR_BUFFER_OVERFLOW if pszFilename buffer too small to hold filename.
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
 * @return  VBox status code.
 * @return  VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @return  VERR_BUFFER_OVERFLOW if pszComment buffer too small to hold comment text.
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
 * @return  VBox status code.
 * @return  VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pszComment      New comment string (UTF-8). NULL is allowed to reset the comment.
 */
VBOXDDU_DECL(int) VDSetComment(PVBOXHDD pDisk, unsigned nImage,
                                   const char *pszComment);

/**
 * Get UUID of image in HDD container.
 *
 * @return  VBox status code.
 * @return  VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pUuid           Where to store the image UUID.
 */
VBOXDDU_DECL(int) VDGetUuid(PVBOXHDD pDisk, unsigned nImage, PRTUUID pUuid);

/**
 * Set the image's UUID. Should not be used by normal applications.
 *
 * @return  VBox status code.
 * @return  VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pUuid           New UUID of the image. If NULL, a new UUID is created.
 */
VBOXDDU_DECL(int) VDSetUuid(PVBOXHDD pDisk, unsigned nImage, PCRTUUID pUuid);

/**
 * Get last modification UUID of image in HDD container.
 *
 * @return  VBox status code.
 * @return  VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pUuid           Where to store the image modification UUID.
 */
VBOXDDU_DECL(int) VDGetModificationUuid(PVBOXHDD pDisk, unsigned nImage,
                                        PRTUUID pUuid);

/**
 * Set the image's last modification UUID. Should not be used by normal applications.
 *
 * @return  VBox status code.
 * @return  VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pUuid           New modification UUID of the image. If NULL, a new UUID is created.
 */
VBOXDDU_DECL(int) VDSetModificationUuid(PVBOXHDD pDisk, unsigned nImage,
                                        PCRTUUID pUuid);

/**
 * Get parent UUID of image in HDD container.
 *
 * @return  VBox status code.
 * @return  VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of the container.
 * @param   pUuid           Where to store the parent image UUID.
 */
VBOXDDU_DECL(int) VDGetParentUuid(PVBOXHDD pDisk, unsigned nImage,
                                  PRTUUID pUuid);

/**
 * Set the image's parent UUID. Should not be used by normal applications.
 *
 * @return  VBox status code.
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


/**
 * Start an asynchronous read request.
 *
 * @return  VBox status code.
 * @param   pDisk           Pointer to the HDD container.
 * @param   uOffset         The offset of the virtual disk to read from.
 * @param   cbRead          How many bytes to read.
 * @param   pcSgBuf         Pointer to the S/G buffer to read into.
 * @param   pfnComplete     Completion callback.
 * @param   pvUser          User data which is passed on completion
 */
VBOXDDU_DECL(int) VDAsyncRead(PVBOXHDD pDisk, uint64_t uOffset, size_t cbRead,
                              PCRTSGBUF pcSgBuf,
                              PFNVDASYNCTRANSFERCOMPLETE pfnComplete,
                              void *pvUser1, void *pvUser2);


/**
 * Start an asynchronous write request.
 *
 * @return  VBox status code.
 * @param   pDisk           Pointer to the HDD container.
 * @param   uOffset         The offset of the virtual disk to write to.
 * @param   cbWrtie         How many bytes to write.
 * @param   pcSgBuf         Pointer to the S/G buffer to write from.
 * @param   pfnComplete     Completion callback.
 * @param   pvUser          User data which is passed on completion.
 */
VBOXDDU_DECL(int) VDAsyncWrite(PVBOXHDD pDisk, uint64_t uOffset, size_t cbWrite,
                               PCRTSGBUF pcSgBuf,
                               PFNVDASYNCTRANSFERCOMPLETE pfnComplete,
                               void *pvUser1, void *pvUser2);


/**
 * Start an asynchronous flush request.
 *
 * @return  VBox status code.
 * @param   pDisk           Pointer to the HDD container.
 * @param   pfnComplete     Completion callback.
 * @param   pvUser          User data which is passed on completion.
 */
VBOXDDU_DECL(int) VDAsyncFlush(PVBOXHDD pDisk,
                               PFNVDASYNCTRANSFERCOMPLETE pfnComplete,
                               void *pvUser1, void *pvUser2);
RT_C_DECLS_END

/** @} */

#endif
