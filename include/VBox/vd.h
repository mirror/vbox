/** @file
 * VBox HDD Container API.
 */

/*
 * Copyright (C) 2006-2015 Oracle Corporation
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
#include <iprt/vfs.h>
#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/err.h>
#include <VBox/vd-ifs.h>

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
 * Same values as MediumVariant API enum.
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

/** @name VD image repair flags
 * @{
 */
/** Don't repair the image but check what needs to be done. */
#define VD_REPAIR_DRY_RUN                       RT_BIT_32(0)

/** Mask of all valid repair flags. */
#define VD_REPAIR_FLAGS_MASK                    (VD_REPAIR_DRY_RUN)
/** @} */

/** @name VD image VFS file flags
 * @{
 */
/** Destroy the VD disk container when the VFS file is released. */
#define VD_VFSFILE_DESTROY_ON_RELEASE           RT_BIT_32(0)

/** Mask of all valid repair flags. */
#define VD_VFSFILE_FLAGS_MASK                   (VD_VFSFILE_DESTROY_ON_RELEASE)
/** @} */

/** @name VBox raw disk or partition flags
 * @{
 */
/** No special treatment. */
#define VBOXHDDRAW_NORMAL       0
/** Whether this is a raw disk (where the partition information is ignored) or
 * not. Valid only in the raw disk descriptor. */
#define VBOXHDDRAW_DISK         RT_BIT(0)
/** Open the corresponding raw disk or partition for reading only, no matter
 * how the image is created or opened. */
#define VBOXHDDRAW_READONLY     RT_BIT(1)
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
    /** Flags for special treatment, see VBOXHDDRAW_FLAGS_*. */
    uint32_t        uFlags;
} VBOXHDDRAWPARTDESC, *PVBOXHDDRAWPARTDESC;

/**
 * Auxiliary data structure for difference between GPT and MBR
 * disks.
 */
typedef enum VBOXHDDPARTTYPE
{
    MBR,
    GPT
} VBOXHDDPARTTYPE;

/**
 * Auxiliary data structure for creating raw disks.
 */
typedef struct VBOXHDDRAW
{
    /** Signature for structure. Must be 'R', 'A', 'W', '\\0'. Actually a trick
     * to make logging of the comment string produce sensible results. */
    char            szSignature[4];
    /** Flags for special treatment, see VBOXHDDRAW_FLAGS_*. */
    /** Flag whether access to full disk should be given (ignoring the
     * partition information below). */
    uint32_t        uFlags;
    /** Filename for the raw disk. Ignored for partitioned raw disks.
     * For Linux e.g. /dev/sda, and for Windows e.g. \\\\.\\PhysicalDisk0. */
    const char      *pszRawDisk;
    /** Number of entries in the partition descriptor array. */
    unsigned        cPartDescs;
    /** Pointer to the partition descriptor array. */
    PVBOXHDDRAWPARTDESC pPartDescs;
    /** Partitioning type of the disk */
    VBOXHDDPARTTYPE uPartitioningType;
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
/** Allow the discard operation if supported. Only available if VD_CAP_DISCARD
 * is set. VDOpen fails with VERR_VD_DISCARD_NOT_SUPPORTED if discarding is not
 * supported. */
#define VD_OPEN_FLAGS_DISCARD       RT_BIT(7)
/** Ignore all flush requests to workaround certain filesystems which are slow
 * when writing a lot of cached data to the medium.
 * Use with extreme care as a host crash can result in completely corrupted and
 * unusable images.
 */
#define VD_OPEN_FLAGS_IGNORE_FLUSH  RT_BIT(8)
/**
 * Return VINF_VD_NEW_ZEROED_BLOCK for reads from unallocated blocks.
 * The caller who uses the flag has to make sure that the read doesn't cross
 * a block boundary. Because the block size can differ between images reading one
 * sector at a time is the safest solution.
 */
#define VD_OPEN_FLAGS_INFORM_ABOUT_ZERO_BLOCKS RT_BIT(9)
/**
 * Don't do unnecessary consistency checks when opening the image.
 * Only valid when the image is opened in readonly because inconsistencies
 * can lead to corrupted images in read-write mode.
 */
#define VD_OPEN_FLAGS_SKIP_CONSISTENCY_CHECKS  RT_BIT(10)
/** Mask of valid flags. */
#define VD_OPEN_FLAGS_MASK          (VD_OPEN_FLAGS_NORMAL | VD_OPEN_FLAGS_READONLY | VD_OPEN_FLAGS_HONOR_ZEROES | VD_OPEN_FLAGS_HONOR_SAME | VD_OPEN_FLAGS_INFO | VD_OPEN_FLAGS_ASYNC_IO | VD_OPEN_FLAGS_SHAREABLE | VD_OPEN_FLAGS_SEQUENTIAL | VD_OPEN_FLAGS_DISCARD | VD_OPEN_FLAGS_IGNORE_FLUSH | VD_OPEN_FLAGS_INFORM_ABOUT_ZERO_BLOCKS | VD_OPEN_FLAGS_SKIP_CONSISTENCY_CHECKS)
/** @}*/

/** @name VBox HDD container filter flags
 * @{
 */
/** The filter is applied during writes. */
#define VD_FILTER_FLAGS_WRITE RT_BIT(0)
/** The filter is applied during reads. */
#define VD_FILTER_FLAGS_READ  RT_BIT(1)
/** Open the filter in info mode. */
#define VD_FILTER_FLAGS_INFO  RT_BIT(2)
/** Default set of filter flags. */
#define VD_FILTER_FLAGS_DEFAULT (VD_FILTER_FLAGS_WRITE | VD_FILTER_FLAGS_READ)
/** Mask of valid flags. */
#define VD_FILTER_FLAGS_MASK    (VD_FILTER_FLAGS_WRITE | VD_FILTER_FLAGS_READ | VD_FILTER_FLAGS_INFO)
/** @} */

/**
 * Helper functions to handle open flags.
 */

/**
 * Translate VD_OPEN_FLAGS_* to RTFile open flags.
 *
 * @return  RTFile open flags.
 * @param   fOpenFlags      VD_OPEN_FLAGS_* open flags.
 * @param   fCreate         Flag that the file should be created.
 */
DECLINLINE(uint32_t) VDOpenFlagsToFileOpenFlags(unsigned fOpenFlags, bool fCreate)
{
    AssertMsg(!(fOpenFlags & VD_OPEN_FLAGS_READONLY) || !fCreate, ("Image can't be opened readonly while being created\n"));

    uint32_t fOpen;
    if (fOpenFlags & VD_OPEN_FLAGS_READONLY)
        fOpen = RTFILE_O_READ | RTFILE_O_DENY_NONE;
    else
    {
        fOpen = RTFILE_O_READWRITE;

        if (fOpenFlags & VD_OPEN_FLAGS_SHAREABLE)
            fOpen |= RTFILE_O_DENY_NONE;
        else
            fOpen |= RTFILE_O_DENY_WRITE;
    }

    if (!fCreate)
        fOpen |= RTFILE_O_OPEN;
    else
        fOpen |= RTFILE_O_CREATE | RTFILE_O_NOT_CONTENT_INDEXED;

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
/** The backend supports the discard operation. */
#define VD_CAP_DISCARD              RT_BIT(10)
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

/**
 * Data structure for returning a list of filter capabilities.
 */
typedef struct VDFILTERINFO
{
    /** Name of the filter. Must be unique even with case insensitive comparison. */
    const char *pszFilter;
    /** Pointer to an array of structs describing each supported config key.
     * Terminated by a NULL config key. Note that some filters do not support
     * the configuration interface, so this pointer may just contain NULL. */
    PCVDCONFIGINFO paConfigInfo;
} VDFILTERINFO, *PVDFILTERINFO;


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
 * Loads a single plugin given by filename.
 *
 * @returns VBox status code.
 * @param   pszFilename     The plugin filename to load.
 */
VBOXDDU_DECL(int) VDPluginLoadFromFilename(const char *pszFilename);

/**
 * Load all plugins from a given path.
 *
 * @returns VBox statuse code.
 * @param   pszPath         The path to load plugins from.
 */
VBOXDDU_DECL(int) VDPluginLoadFromPath(const char *pszPath);

/**
 * Unloads a single plugin given by filename.
 *
 * @returns VBox status code.
 * @param   pszFilename     The plugin filename to unload.
 */
VBOXDDU_DECL(int) VDPluginUnloadFromFilename(const char *pszFilename);

/**
 * Unload all plugins from a given path.
 *
 * @returns VBox statuse code.
 * @param   pszPath         The path to unload plugins from.
 */
VBOXDDU_DECL(int) VDPluginUnloadFromPath(const char *pszPath);

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
 * Lists all filters and their capabilities in a caller-provided buffer.
 *
 * @return  VBox status code.
 *          VERR_BUFFER_OVERFLOW if not enough space is passed.
 * @param   cEntriesAlloc   Number of list entries available.
 * @param   pEntries        Pointer to array for the entries.
 * @param   pcEntriesUsed   Number of entries returned.
 */
VBOXDDU_DECL(int) VDFilterInfo(unsigned cEntriesAlloc, PVDFILTERINFO pEntries,
                               unsigned *pcEntriesUsed);

/**
 * Lists the capabilities of a filter identified by its name.
 *
 * @return  VBox status code.
 * @param   pszFilter       The filter name (case insensitive).
 * @param   pEntries        Pointer to an entry.
 */
VBOXDDU_DECL(int) VDFilterInfoOne(const char *pszFilter, PVDFILTERINFO pEntry);

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
 * @return  VBox status code.
 * @param   pDisk           Pointer to HDD container.
 */
VBOXDDU_DECL(int) VDDestroy(PVBOXHDD pDisk);

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
 * Adds a filter to the disk.
 *
 * @returns VBox status code.
 * @param   pDisk           Pointer to the HDD container which should use the filter.
 * @param   pszFilter       Name of the filter backend to use (case insensitive).
 * @param   fFlags          Flags which apply to the filter, combination of VD_FILTER_FLAGS_*
 *                          defines.
 * @param   pVDIfsFilter    Pointer to the per-filter VD interface list.
 */
VBOXDDU_DECL(int) VDFilterAdd(PVBOXHDD pDisk, const char *pszFilter, uint32_t fFlags,
                              PVDINTERFACE pVDIfsFilter);

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
 * Resizes the given disk image to the given size. It is OK if there are
 * multiple images open in the container. In this case the last disk image
 * will be resized.
 *
 * @return  VBox status
 * @return  VERR_VD_IMAGE_READ_ONLY if image is not writable.
 * @return  VERR_NOT_SUPPORTED if this kind of image can't be compacted.
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
 * Prepares the given disk for use by the added filters. This applies to all
 * opened images in the chain which might be opened read/write temporary.
 *
 * @return  VBox status code.
 *
 * @param   pDisk           Pointer to the HDD container.
 * @param   pVDIfsOperation Pointer to the per-operation VD interface list.
 */
VBOXDDU_DECL(int) VDPrepareWithFilters(PVBOXHDD pDisk, PVDINTERFACE pVDIfsOperation);

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
 * Removes the last added filter in the HDD container from the specified chain.
 *
 * @return  VBox status code.
 * @retval  VERR_VD_NOT_OPENED if no filter is present for the disk.
 * @param   pDisk           Pointer to HDD container.
 * @param   fFlags          Combination of VD_FILTER_FLAGS_* defines.
 */
VBOXDDU_DECL(int) VDFilterRemove(PVBOXHDD pDisk, uint32_t fFlags);

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
 * Removes all filters of the given HDD container.
 *
 * @return  VBox status code.
 * @param   pDisk           Pointer to HDD container.
 */
VBOXDDU_DECL(int) VDFilterRemoveAll(PVBOXHDD pDisk);

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
 * Get sector size of an image in HDD container.
 *
 * @return  Virtual disk sector size in bytes.
 * @return  0 if image with specified number was not opened.
 * @param   pDisk           Pointer to HDD container.
 * @param   nImage          Image number, counts from 0. 0 is always base image of container.
 */
VBOXDDU_DECL(uint32_t) VDGetSectorSize(PVBOXHDD pDisk, unsigned nImage);

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
 * Discards unused ranges given as a list.
 *
 * @return  VBox status code.
 * @param   pDisk           Pointer to HDD container.
 * @param   paRanges        The array of ranges to discard.
 * @param   cRanges         Number of entries in the array.
 *
 * @note In contrast to VDCompact() the ranges are always discarded even if they
 *       appear to contain data. This method is mainly used to implement TRIM support.
 */
VBOXDDU_DECL(int) VDDiscardRanges(PVBOXHDD pDisk, PCRTRANGE paRanges, unsigned cRanges);


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

/**
 * Start an asynchronous discard request.
 *
 * @return  VBox status code.
 * @param   pDisk           Pointer to HDD container.
 * @param   paRanges        The array of ranges to discard.
 * @param   cRanges         Number of entries in the array.
 * @param   pfnComplete     Completion callback.
 * @param   pvUser1         User data which is passed on completion.
 * @param   pvUser2         User data which is passed on completion.
 */
VBOXDDU_DECL(int) VDAsyncDiscardRanges(PVBOXHDD pDisk, PCRTRANGE paRanges, unsigned cRanges,
                                       PFNVDASYNCTRANSFERCOMPLETE pfnComplete,
                                       void *pvUser1, void *pvUser);

/**
 * Tries to repair a corrupted image.
 *
 * @return  VBox status code.
 * @retval  VERR_VD_IMAGE_REPAIR_NOT_SUPPORTED if the backend does not support repairing the image.
 * @retval  VERR_VD_IMAGE_REPAIR_IMPOSSIBLE if the corruption is to severe to repair the image.
 * @param   pVDIfsDisk      Pointer to the per-disk VD interface list.
 * @param   pVDIfsImage     Pointer to the per-image VD interface list.
 * @param   pszFilename     Name of the image file to repair.
 * @param   pszFormat       The backend to use.
 * @param   fFlags          Combination of the VD_REPAIR_* flags.
 */
VBOXDDU_DECL(int) VDRepair(PVDINTERFACE pVDIfsDisk, PVDINTERFACE pVDIfsImage,
                           const char *pszFilename, const char *pszBackend,
                           uint32_t fFlags);

/**
 * Create a VFS file handle from the given HDD container.
 *
 * @return  VBox status code.
 * @param   pDisk           Pointer to HDD container.
 * @param   fFlags          Combination of the VD_VFSFILE_* flags.
 * @param   phVfsFile       Where to store the handle to the VFS file on
 *                          success.
 */
VBOXDDU_DECL(int) VDCreateVfsFileFromDisk(PVBOXHDD pDisk, uint32_t fFlags,
                                          PRTVFSFILE phVfsFile);

RT_C_DECLS_END

/** @} */

#endif
