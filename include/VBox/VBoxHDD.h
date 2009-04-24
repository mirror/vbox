/** @file
 * VBox HDD Container API.
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

#ifndef ___VBox_VD_h
#define ___VBox_VD_h

#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/mem.h>
#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/err.h>
#include <VBox/pdmifs.h>
/** @todo remove this dependency, using PFNVMPROGRESS outside VMM is *WRONG*. */
#include <VBox/vmapi.h>

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
/** Do not perform the base/diff image check on open. This does NOT imply
 * opening the image as readonly (would break e.g. adding UUIDs to VMDK files
 * created by other products). Images opened with this flag should only be
 * used for querying information, and nothing else. */
#define VD_OPEN_FLAGS_INFO          RT_BIT(3)
/** Open image for asynchronous access.
 *  Only available if VD_CAP_ASYNC_IO is set
 *  Check with VDIsAsynchonousIoSupported wether
 *  asynchronous I/O is really supported for this file.
 */
#define VD_OPEN_FLAGS_ASYNC_IO      RT_BIT(4)
/** Mask of valid flags. */
#define VD_OPEN_FLAGS_MASK          (VD_OPEN_FLAGS_NORMAL | VD_OPEN_FLAGS_READONLY | VD_OPEN_FLAGS_HONOR_ZEROES | VD_OPEN_FLAGS_HONOR_SAME | VD_OPEN_FLAGS_INFO | VD_OPEN_FLAGS_ASYNC_IO)
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
    /** Interface for asynchronous I/O operations. Per-disk. */
    VDINTERFACETYPE_ASYNCIO,
    /** Interface for progress notification. Per-operation. */
    VDINTERFACETYPE_PROGRESS,
    /** Interface for configuration information. Per-image. */
    VDINTERFACETYPE_CONFIG,
    /** Interface for TCP network stack. Per-disk. */
    VDINTERFACETYPE_TCPNET,
    /** Interface for getting parent image state. Per-operation. */
    VDINTERFACETYPE_PARENTSTATE,
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
    AssertMsgReturn(   (enmInterface >= VDINTERFACETYPE_FIRST)
                    && (enmInterface < VDINTERFACETYPE_INVALID),
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

    /** Argument checks. */
    AssertMsgReturn(   (enmInterface >= VDINTERFACETYPE_FIRST)
                    && (enmInterface < VDINTERFACETYPE_INVALID),
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
 * Interface to deliver error messages to upper layers.
 *
 * Per disk interface. Optional, but think twice if you want to miss the
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
     * Error message callback.
     *
     * @param   pvUser          The opaque data passed on container creation.
     * @param   rc              The VBox error code.
     * @param   RT_SRC_POS_DECL Use RT_SRC_POS.
     * @param   pszFormat       Error message format string.
     * @param   va              Error message arguments.
     */
    DECLR3CALLBACKMEMBER(void, pfnError, (void *pvUser, int rc, RT_SRC_POS_DECL, const char *pszFormat, va_list va));

} VDINTERFACEERROR, *PVDINTERFACEERROR;

/**
 * Get error interface from opaque callback table.
 *
 * @return Pointer to the callback table.
 * @param  pInterface Pointer to the interface descriptor.
 */
DECLINLINE(PVDINTERFACEERROR) VDGetInterfaceError(PVDINTERFACE pInterface)
{
    /* Check that the interface descriptor is a error interface. */
    AssertMsgReturn(   (pInterface->enmInterface == VDINTERFACETYPE_ERROR)
                    && (pInterface->cbSize == sizeof(VDINTERFACE)),
                    ("Not an error interface"), NULL);

    PVDINTERFACEERROR pInterfaceError = (PVDINTERFACEERROR)pInterface->pCallbacks;

    /* Do basic checks. */
    AssertMsgReturn(   (pInterfaceError->cbSize == sizeof(VDINTERFACEERROR))
                    && (pInterfaceError->enmInterface == VDINTERFACETYPE_ERROR),
                    ("A non error callback table attached to a error interface descriptor\n"), NULL);

    return pInterfaceError;
}

/**
 * Completion callback which is called by the interface owner
 * to inform the backend that a task finished.
 *
 * @return  VBox status code.
 * @param   pvUser          Opaque user data which is passed on request submission.
 */
typedef DECLCALLBACK(int) FNVDCOMPLETED(void *pvUser);
/** Pointer to FNVDCOMPLETED() */
typedef FNVDCOMPLETED *PFNVDCOMPLETED;


/**
 * Support interface for asynchronous I/O
 *
 * Per-disk. Optional.
 */
typedef struct VDINTERFACEASYNCIO
{
    /**
     * Size of the async interface.
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
     * @param   fReadonly       Whether to open the storage medium read only.
     * @param   ppStorage       Where to store the opaque storage handle.
     */
    DECLR3CALLBACKMEMBER(int, pfnOpen, (void *pvUser, const char *pszLocation, bool fReadonly, void **ppStorage));

    /**
     * Close callback.
     *
     * @return  VBox status code.
     * @param   pvUser          The opaque data passed on container creation.
     * @param   pStorage        The opaque storage handle to close.
     */
    DECLR3CALLBACKMEMBER(int, pfnClose, (void *pvUser, void *pStorage));

    /**
     * Synchronous write callback.
     *
     * @return  VBox status code.
     * @param   pvUser          The opaque data passed on container creation.
     * @param   pStorage        The storage handle to use.
     * @param   uOffset         The offset to start from.
     * @param   cbWrite         How many bytes to write.
     * @param   pvBuf           Pointer to the bits need to be written.
     * @param   pcbWritten      Where to store how many bytes where actually written.
     */
    DECLR3CALLBACKMEMBER(int, pfnWrite, (void *pvUser, void *pStorage, uint64_t uOffset,
                                         size_t cbWrite, const void *pvBuf, size_t *pcbWritten));

    /**
     * Synchronous read callback.
     *
     * @return  VBox status code.
     * @param   pvUser          The opaque data passed on container creation.
     * @param   pStorage        The storage handle to use.
     * @param   uOffset         The offset to start from.
     * @param   cbRead          How many bytes to read.
     * @param   pvBuf           Where to store the read bits.
     * @param   pcbRead         Where to store how many bytes where actually read.
     */
    DECLR3CALLBACKMEMBER(int, pfnRead, (void *pvUser, void *pStorage, uint64_t uOffset,
                                        size_t cbRead, void *pvBuf, size_t *pcbRead));

    /**
     * Flush data to the storage backend.
     *
     * @return  VBox statis code.
     * @param   pvUser          The opaque data passed on container creation.
     * @param   pStorage        The storage handle to flush.
     */
    DECLR3CALLBACKMEMBER(int, pfnFlush, (void *pvUser, void *pStorage));

    /**
     * Prepare an asynchronous read task.
     *
     * @return  VBox status code.
     * @param   pvUser         The opqaue user data passed on container creation.
     * @param   pStorage       The storage handle.
     * @param   uOffset        The offset to start reading from.
     * @param   pvBuf          Where to store read bits.
     * @param   cbRead         How many bytes to read.
     * @param   ppTask         Where to store the opaque task handle.
     */
    DECLR3CALLBACKMEMBER(int, pfnPrepareRead, (void *pvUser, void *pStorage, uint64_t uOffset,
                                               void *pvBuf, size_t cbRead, void **ppTask));

    /**
     * Prepare an asynchronous write task.
     *
     * @return  VBox status code.
     * @param   pvUser         The opaque user data passed on conatiner creation.
     * @param   pStorage       The storage handle.
     * @param   uOffset        The offset to start writing to.
     * @param   pvBuf          Where to read the data from.
     * @param   cbWrite        How many bytes to write.
     * @param   ppTask         Where to store the opaque task handle.
     */
    DECLR3CALLBACKMEMBER(int, pfnPrepareWrite, (void *pvUser, void *pStorage, uint64_t uOffset,
                                                void *pvBuf, size_t cbWrite, void **ppTask));

    /**
     * Submit an array of tasks for processing
     *
     * @return  VBox status code.
     * @param   pvUser        The opaque user data passed on container creation.
     * @param   apTasks       Array of task handles to submit.
     * @param   cTasks        How many tasks to submit.
     * @param   pvUser2       User data which is passed on completion.
     * @param   pvUserCaller  Opaque user data the caller of VDAsyncWrite/Read passed.
     * @param   pfnTasksCompleted Pointer to callback which is called on request completion.
     */
    DECLR3CALLBACKMEMBER(int, pfnTasksSubmit, (void *pvUser, void *apTasks[], unsigned cTasks, void *pvUser2,
                                               void *pvUserCaller, PFNVDCOMPLETED pfnTasksCompleted));

} VDINTERFACEASYNCIO, *PVDINTERFACEASYNCIO;

/**
 * Get async I/O interface from opaque callback table.
 *
 * @return Pointer to the callback table.
 * @param  pInterface Pointer to the interface descriptor.
 */
DECLINLINE(PVDINTERFACEASYNCIO) VDGetInterfaceAsyncIO(PVDINTERFACE pInterface)
{
    /* Check that the interface descriptor is a async I/O interface. */
    AssertMsgReturn(   (pInterface->enmInterface == VDINTERFACETYPE_ASYNCIO)
                    && (pInterface->cbSize == sizeof(VDINTERFACE)),
                    ("Not an async I/O interface"), NULL);

    PVDINTERFACEASYNCIO pInterfaceAsyncIO = (PVDINTERFACEASYNCIO)pInterface->pCallbacks;

    /* Do basic checks. */
    AssertMsgReturn(   (pInterfaceAsyncIO->cbSize == sizeof(VDINTERFACEASYNCIO))
                    && (pInterfaceAsyncIO->enmInterface == VDINTERFACETYPE_ASYNCIO),
                    ("A non async I/O callback table attached to a async I/O interface descriptor\n"), NULL);

    return pInterfaceAsyncIO;
}

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
     * @todo r=bird: Why the heck are we using PFNVMPROGRESS here?
     */
    PFNVMPROGRESS   pfnProgress;
} VDINTERFACEPROGRESS, *PVDINTERFACEPROGRESS;

/**
 * Get progress interface from opaque callback table.
 *
 * @return Pointer to the callback table.
 * @param  pInterface Pointer to the interface descriptor.
 */
DECLINLINE(PVDINTERFACEPROGRESS) VDGetInterfaceProgress(PVDINTERFACE pInterface)
{
    /* Check that the interface descriptor is a progress interface. */
    AssertMsgReturn(   (pInterface->enmInterface == VDINTERFACETYPE_PROGRESS)
                    && (pInterface->cbSize == sizeof(VDINTERFACE)),
                    ("Not a progress interface"), NULL);


    PVDINTERFACEPROGRESS pInterfaceProgress = (PVDINTERFACEPROGRESS)pInterface->pCallbacks;

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
    /* Check that the interface descriptor is a config interface. */
    AssertMsgReturn(   (pInterface->enmInterface == VDINTERFACETYPE_CONFIG)
                    && (pInterface->cbSize == sizeof(VDINTERFACE)),
                    ("Not a config interface"), NULL);

    PVDINTERFACECONFIG pInterfaceConfig = (PVDINTERFACECONFIG)pInterface->pCallbacks;

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
        char *pvData = (char *)RTMemAlloc(cb);
        if (pvData)
        {
            rc = pCfgIf->pfnQuery(pvUser, pszName, pvData, cb);
            if (RT_SUCCESS(rc))
            {
                *ppvData = pvData;
                *pcbData = cb;
            }
            else
                RTMemFree(pvData);
        }
        else
            rc = VERR_NO_MEMORY;
    }
    return rc;
}


/**
 * TCP network stack interface
 *
 * Per-disk. Mandatory for backends which have the VD_CAP_TCPNET bit set.
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
     * Connect as a client to a TCP port.
     *
     * @return  iprt status code.
     * @param   pszAddress      The address to connect to.
     * @param   uPort           The port to connect to.
     * @param   pSock           Where to store the handle to the established connect
ion.
     */
    DECLR3CALLBACKMEMBER(int, pfnClientConnect, (const char *pszAddress, uint32_t uPort, PRTSOCKET pSock));

    /**
     * Close a TCP connection.
     *
     * @return  iprt status code.
     * @param   Sock            Socket descriptor.
ion.
     */
    DECLR3CALLBACKMEMBER(int, pfnClientClose, (RTSOCKET Sock));

    /**
     * Socket I/O multiplexing.
     * Checks if the socket is ready for reading.
     *
     * @return  iprt status code.
     * @param   Sock        Socket descriptor.
     * @param   cMillies    Number of milliseconds to wait for the socket.
     *                      Use RT_INDEFINITE_WAIT to wait for ever.
     */
    DECLR3CALLBACKMEMBER(int, pfnSelectOne, (RTSOCKET Sock, unsigned cMillies));

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
    DECLR3CALLBACKMEMBER(int, pfnRead, (RTSOCKET Sock, void *pvBuffer, size_t cbBuffer, size_t *pcbRead));

    /**
     * Send data from a socket.
     *
     * @return  iprt status code.
     * @param   Sock        Socket descriptor.
     * @param   pvBuffer    Buffer to write data to socket.
     * @param   cbBuffer    How much to write.
     * @param   pcbRead     Number of bytes read.
     */
    DECLR3CALLBACKMEMBER(int, pfnWrite, (RTSOCKET Sock, const void *pvBuffer, size_t cbBuffer));

    /**
     * Flush socket write buffers.
     *
     * @return  iprt status code.
     * @param   Sock        Socket descriptor.
     */
    DECLR3CALLBACKMEMBER(int, pfnFlush, (RTSOCKET Sock));

} VDINTERFACETCPNET, *PVDINTERFACETCPNET;

/**
 * Get TCP network stack interface from opaque callback table.
 *
 * @return Pointer to the callback table.
 * @param  pInterface Pointer to the interface descriptor.
 */
DECLINLINE(PVDINTERFACETCPNET) VDGetInterfaceTcpNet(PVDINTERFACE pInterface)
{
    /* Check that the interface descriptor is a TCP network stack interface. */
    AssertMsgReturn(   (pInterface->enmInterface == VDINTERFACETYPE_TCPNET)
                    && (pInterface->cbSize == sizeof(VDINTERFACE)),
                    ("Not a TCP network stack interface"), NULL);

    PVDINTERFACETCPNET pInterfaceTcpNet = (PVDINTERFACETCPNET)pInterface->pCallbacks;

    /* Do basic checks. */
    AssertMsgReturn(   (pInterfaceTcpNet->cbSize == sizeof(VDINTERFACETCPNET))
                    && (pInterfaceTcpNet->enmInterface == VDINTERFACETYPE_TCPNET),
                    ("A non TCP network stack callback table attached to a TCP network stack interface descriptor\n"), NULL);

    return pInterfaceTcpNet;
}

/**
 * Interface to get the parent state.
 *
 * Per operation interface. Optional, present only if there is a parent, and
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
     * @param   pvBuf           Pointer to buffer for reading data.
     * @param   cbRead          Number of bytes to read.
     *                          Must be aligned to a sector boundary.
     */
    DECLR3CALLBACKMEMBER(int, pfnParentRead, (void *pvUser, uint64_t uOffset, void *pvBuf, size_t cbRead));

} VDINTERFACEPARENTSTATE, *PVDINTERFACEPARENTSTATE;


/**
 * Get parent state interface from opaque callback table.
 *
 * @return Pointer to the callback table.
 * @param  pInterface Pointer to the interface descriptor.
 */
DECLINLINE(PVDINTERFACEPARENTSTATE) VDGetInterfaceParentState(PVDINTERFACE pInterface)
{
    /* Check that the interface descriptor is a parent state interface. */
    AssertMsgReturn(   (pInterface->enmInterface == VDINTERFACETYPE_PARENTSTATE)
                    && (pInterface->cbSize == sizeof(VDINTERFACE)),
                    ("Not a parent state interface"), NULL);

    PVDINTERFACEPARENTSTATE pInterfaceParentState = (PVDINTERFACEPARENTSTATE)pInterface->pCallbacks;

    /* Do basic checks. */
    AssertMsgReturn(   (pInterfaceParentState->cbSize == sizeof(VDINTERFACEPARENTSTATE))
                    && (pInterfaceParentState->enmInterface == VDINTERFACETYPE_PARENTSTATE),
                    ("A non parent state callback table attached to a parent state interface descriptor\n"), NULL);

    return pInterfaceParentState;
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
    const char * const *papszFileExtensions;
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
 * Free all returned names with RTStrFree() when you no longer need them.
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
 * Lists the capablities of a backend indentified by its name.
 * Free all returned names with RTStrFree() when you no longer need them.
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
 * @param   ppDisk          Where to store the reference to HDD container.
 */
VBOXDDU_DECL(int) VDCreate(PVDINTERFACE pVDIfsDisk, PVBOXHDD *ppDisk);

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
                               PCPDMMEDIAGEOMETRY pPCHSGeometry,
                               PCPDMMEDIAGEOMETRY pLCHSGeometry,
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
 * Copies an image from one HDD container to another.
 * The copy is opened in the target HDD container.
 * It is possible to convert between different image formats, because the
 * backend for the destination may be different from the source.
 * If both the source and destination reference the same HDD container,
 * then the image is moved (by copying/deleting or renaming) to the new location.
 * The source container is unchanged if the move operation fails, otherwise
 * the image at the new location is opened in the same way as the old one was.
 *
 * @return  VBox status code.
 * @return  VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDiskFrom       Pointer to source HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pDiskTo         Pointer to destination HDD container.
 * @param   pszBackend      Name of the image file backend to use (may be NULL to use the same as the source, case insensitive).
 * @param   pszFilename     New name of the image (may be NULL if pDiskFrom == pDiskTo).
 * @param   fMoveByRename   If true, attempt to perform a move by renaming (if successful the new size is ignored).
 * @param   cbSize          New image size (0 means leave unchanged).
 * @param   uImageFlags     Flags specifying special destination image features.
 * @param   pDstUuid        New UUID of the destination image. If NULL, a new UUID is created.
 *                          This parameter is used if and only if a true copy is created.
 *                          In all rename/move cases the UUIDs are copied over.
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
                         PVDINTERFACE pVDIfsOperation,
                         PVDINTERFACE pDstVDIfsImage,
                         PVDINTERFACE pDstVDIfsOperation);

/**
 * Optimizes the storage consumption of an image. Typically the unused blocks
 * have to be wiped with zeroes to achieve a substantial reduced storage use.
 * Another optimization done is reordering the image blocks, which can provide
 * a significant performance boost, as reads and writes tend to use less random
 * file offsets.
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
 * Closes the last opened image file in HDD container.
 * If previous image file was opened in read-only mode (that is normal) and closing image
 * was opened in read-write mode (the whole disk was in read-write mode) - the previous image
 * will be reopened in read/write mode.
 *
 * @return  VBox status code.
 * @return  VERR_VD_NOT_OPENED if no image is opened in HDD container.
 * @param   pDisk           Pointer to HDD container.
 * @param   fDelete         If true, delete the image from the host disk.
 */
VBOXDDU_DECL(int) VDClose(PVBOXHDD pDisk, bool fDelete);

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
 * @param   pvBuf           Pointer to buffer for reading data.
 * @param   cbRead          Number of bytes to read.
 *                          Must be aligned to a sector boundary.
 */
VBOXDDU_DECL(int) VDRead(PVBOXHDD pDisk, uint64_t uOffset, void *pvBuf, size_t cbRead);

/**
 * Write data to virtual HDD.
 *
 * @return  VBox status code.
 * @return  VERR_VD_NOT_OPENED if no image is opened in HDD container.
 * @param   pDisk           Pointer to HDD container.
 * @param   uOffset         Offset of first writing byte from start of disk.
 *                          Must be aligned to a sector boundary.
 * @param   pvBuf           Pointer to buffer for writing data.
 * @param   cbWrite         Number of bytes to write.
 *                          Must be aligned to a sector boundary.
 */
VBOXDDU_DECL(int) VDWrite(PVBOXHDD pDisk, uint64_t uOffset, const void *pvBuf, size_t cbWrite);

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
                                    PPDMMEDIAGEOMETRY pPCHSGeometry);

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
                                    PCPDMMEDIAGEOMETRY pPCHSGeometry);

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
                                    PPDMMEDIAGEOMETRY pLCHSGeometry);

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
                                    PCPDMMEDIAGEOMETRY pLCHSGeometry);

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
 * Query if asynchronous operations are supported for this disk.
 *
 * @return  VBox status code.
 * @return  VERR_VD_IMAGE_NOT_FOUND if image with specified number was not opened.
 * @param   pDisk           Pointer to the HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 * @param   pfAIOSupported  Where to store if async IO is supported.
 */
VBOXDDU_DECL(int) VDImageIsAsyncIOSupported(PVBOXHDD pDisk, unsigned nImage, bool *pfAIOSupported);


/**
 * Start a asynchronous read request.
 *
 * @return  VBox status code.
 * @param   pDisk           Pointer to the HDD container.
 * @param   uOffset         The offset of the virtual disk to read from.
 * @param   cbRead          How many bytes to read.
 * @param   paSeg           Pointer to an array of segments.
 * @param   cSeg            Number of segments in the array.
 * @param   pvUser          User data which is passed on completion
 */
VBOXDDU_DECL(int) VDAsyncRead(PVBOXHDD pDisk, uint64_t uOffset, size_t cbRead,
                              PPDMDATASEG paSeg, unsigned cSeg,
                              void *pvUser);


/**
 * Start a asynchronous write request.
 *
 * @return  VBox status code.
 * @param   pDisk           Pointer to the HDD container.
 * @param   uOffset         The offset of the virtual disk to write to.
 * @param   cbWrtie         How many bytes to write.
 * @param   paSeg           Pointer to an array of segments.
 * @param   cSeg            Number of segments in the array.
 * @param   pvUser          User data which is passed on completion.
 */
VBOXDDU_DECL(int) VDAsyncWrite(PVBOXHDD pDisk, uint64_t uOffset, size_t cbWrite,
                               PPDMDATASEG paSeg, unsigned cSeg,
                               void *pvUser);


__END_DECLS

/** @} */

#endif
