/** @file
 * PDM - Pluggable Device Manager, Storage related interfaces.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
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

#ifndef ___VBox_vmm_pdmstorageifs_h
#define ___VBox_vmm_pdmstorageifs_h

#include <iprt/sg.h>
#include <VBox/types.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_pdm_ifs_storage       PDM Storage Interfaces
 * @ingroup grp_pdm_interfaces
 * @{
 */


/** Pointer to a mount interface. */
typedef struct PDMIMOUNTNOTIFY *PPDMIMOUNTNOTIFY;
/**
 * Block interface (up).
 * Pair with PDMIMOUNT.
 */
typedef struct PDMIMOUNTNOTIFY
{
    /**
     * Called when a media is mounted.
     *
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnMountNotify,(PPDMIMOUNTNOTIFY pInterface));

    /**
     * Called when a media is unmounted
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(void, pfnUnmountNotify,(PPDMIMOUNTNOTIFY pInterface));
} PDMIMOUNTNOTIFY;
/** PDMIMOUNTNOTIFY interface ID. */
#define PDMIMOUNTNOTIFY_IID                     "fa143ac9-9fc6-498e-997f-945380a558f9"


/** Pointer to mount interface. */
typedef struct PDMIMOUNT *PPDMIMOUNT;
/**
 * Mount interface (down).
 * Pair with PDMIMOUNTNOTIFY.
 */
typedef struct PDMIMOUNT
{
    /**
     * Unmount the media.
     *
     * The driver will validate and pass it on. On the rebounce it will decide whether or not to detach it self.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  The emulation thread.
     * @param   fForce          Force the unmount, even for locked media.
     * @param   fEject          Eject the medium. Only relevant for host drives.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnUnmount,(PPDMIMOUNT pInterface, bool fForce, bool fEject));

    /**
     * Checks if a media is mounted.
     *
     * @returns true if mounted.
     * @returns false if not mounted.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(bool, pfnIsMounted,(PPDMIMOUNT pInterface));

    /**
     * Locks the media, preventing any unmounting of it.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnLock,(PPDMIMOUNT pInterface));

    /**
     * Unlocks the media, canceling previous calls to pfnLock().
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnUnlock,(PPDMIMOUNT pInterface));

    /**
     * Checks if a media is locked.
     *
     * @returns true if locked.
     * @returns false if not locked.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(bool, pfnIsLocked,(PPDMIMOUNT pInterface));
} PDMIMOUNT;
/** PDMIMOUNT interface ID. */
#define PDMIMOUNT_IID                           "34fc7a4c-623a-4806-a6bf-5be1be33c99f"


/**
 * Callback which provides progress information.
 *
 * @return  VBox status code.
 * @param   pvUser          Opaque user data.
 * @param   uPercent        Completion percentage.
 */
typedef DECLCALLBACK(int) FNSIMPLEPROGRESS(void *pvUser, unsigned uPercentage);
/** Pointer to FNSIMPLEPROGRESS() */
typedef FNSIMPLEPROGRESS *PFNSIMPLEPROGRESS;


/**
 * Media type.
 */
typedef enum PDMMEDIATYPE
{
    /** Error (for the query function). */
    PDMMEDIATYPE_ERROR = 1,
    /** 360KB 5 1/4" floppy drive. */
    PDMMEDIATYPE_FLOPPY_360,
    /** 720KB 3 1/2" floppy drive. */
    PDMMEDIATYPE_FLOPPY_720,
    /** 1.2MB 5 1/4" floppy drive. */
    PDMMEDIATYPE_FLOPPY_1_20,
    /** 1.44MB 3 1/2" floppy drive. */
    PDMMEDIATYPE_FLOPPY_1_44,
    /** 2.88MB 3 1/2" floppy drive. */
    PDMMEDIATYPE_FLOPPY_2_88,
    /** Fake drive that can take up to 15.6 MB images.
     * C=255, H=2, S=63.  */
    PDMMEDIATYPE_FLOPPY_FAKE_15_6,
    /** Fake drive that can take up to 63.5 MB images.
     * C=255, H=2, S=255.  */
    PDMMEDIATYPE_FLOPPY_FAKE_63_5,
    /** CDROM drive. */
    PDMMEDIATYPE_CDROM,
    /** DVD drive. */
    PDMMEDIATYPE_DVD,
    /** Hard disk drive. */
    PDMMEDIATYPE_HARD_DISK
} PDMMEDIATYPE;

/** Check if the given block type is a floppy. */
#define PDMMEDIATYPE_IS_FLOPPY(a_enmType) ( (a_enmType) >= PDMMEDIATYPE_FLOPPY_360 && (a_enmType) <= PDMMEDIATYPE_FLOPPY_2_88 )

/**
 * Raw command data transfer direction.
 */
typedef enum PDMMEDIATXDIR
{
    PDMMEDIATXDIR_NONE = 0,
    PDMMEDIATXDIR_FROM_DEVICE,
    PDMMEDIATXDIR_TO_DEVICE
} PDMMEDIATXDIR;

/**
 * Media geometry structure.
 */
typedef struct PDMMEDIAGEOMETRY
{
    /** Number of cylinders. */
    uint32_t    cCylinders;
    /** Number of heads. */
    uint32_t    cHeads;
    /** Number of sectors. */
    uint32_t    cSectors;
} PDMMEDIAGEOMETRY;

/** Pointer to media geometry structure. */
typedef PDMMEDIAGEOMETRY *PPDMMEDIAGEOMETRY;
/** Pointer to constant media geometry structure. */
typedef const PDMMEDIAGEOMETRY *PCPDMMEDIAGEOMETRY;

/** Pointer to a media port interface. */
typedef struct PDMIMEDIAPORT *PPDMIMEDIAPORT;
/**
 * Media port interface (down).
 */
typedef struct PDMIMEDIAPORT
{
    /**
     * Returns the storage controller name, instance and LUN of the attached medium.
     *
     * @returns VBox status.
     * @param   pInterface      Pointer to this interface.
     * @param   ppcszController Where to store the name of the storage controller.
     * @param   piInstance      Where to store the instance number of the controller.
     * @param   piLUN           Where to store the LUN of the attached device.
     */
    DECLR3CALLBACKMEMBER(int, pfnQueryDeviceLocation, (PPDMIMEDIAPORT pInterface, const char **ppcszController,
                                                       uint32_t *piInstance, uint32_t *piLUN));

} PDMIMEDIAPORT;
/** PDMIMEDIAPORT interface ID. */
#define PDMIMEDIAPORT_IID                           "9f7e8c9e-6d35-4453-bbef-1f78033174d6"

/** Pointer to a media interface. */
typedef struct PDMIMEDIA *PPDMIMEDIA;
/**
 * Media interface (up).
 * Pairs with PDMIMEDIAPORT.
 */
typedef struct PDMIMEDIA
{
    /**
     * Read bits.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   off             Offset to start reading from. The offset must be aligned to a sector boundary.
     * @param   pvBuf           Where to store the read bits.
     * @param   cbRead          Number of bytes to read. Must be aligned to a sector boundary.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnRead,(PPDMIMEDIA pInterface, uint64_t off, void *pvBuf, size_t cbRead));

    /**
     * Read bits - version for DevPcBios.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   off             Offset to start reading from. The offset must be aligned to a sector boundary.
     * @param   pvBuf           Where to store the read bits.
     * @param   cbRead          Number of bytes to read. Must be aligned to a sector boundary.
     * @thread  Any thread.
     *
     * @note: Special version of pfnRead which doesn't try to suspend the VM when the DEKs for encrypted disks
     *        are missing but just returns an error.
     */
    DECLR3CALLBACKMEMBER(int, pfnReadPcBios,(PPDMIMEDIA pInterface, uint64_t off, void *pvBuf, size_t cbRead));

    /**
     * Write bits.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   off             Offset to start writing at. The offset must be aligned to a sector boundary.
     * @param   pvBuf           Where to store the write bits.
     * @param   cbWrite         Number of bytes to write. Must be aligned to a sector boundary.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnWrite,(PPDMIMEDIA pInterface, uint64_t off, const void *pvBuf, size_t cbWrite));

    /**
     * Make sure that the bits written are actually on the storage medium.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnFlush,(PPDMIMEDIA pInterface));

    /**
     * Send a raw command to the underlying device (CDROM).
     * This method is optional (i.e. the function pointer may be NULL).
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pbCmd           Offset to start reading from.
     * @param   enmTxDir        Direction of transfer.
     * @param   pvBuf           Pointer tp the transfer buffer.
     * @param   cbBuf           Size of the transfer buffer.
     * @param   pbSenseKey      Status of the command (when return value is VERR_DEV_IO_ERROR).
     * @param   cTimeoutMillies Command timeout in milliseconds.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnSendCmd,(PPDMIMEDIA pInterface, const uint8_t *pbCmd, PDMMEDIATXDIR enmTxDir, void *pvBuf, uint32_t *pcbBuf, uint8_t *pabSense, size_t cbSense, uint32_t cTimeoutMillies));

    /**
     * Merge medium contents during a live snapshot deletion. All details
     * must have been configured through CFGM or this will fail.
     * This method is optional (i.e. the function pointer may be NULL).
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pfnProgress     Function pointer for progress notification.
     * @param   pvUser          Opaque user data for progress notification.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnMerge,(PPDMIMEDIA pInterface, PFNSIMPLEPROGRESS pfnProgress, void *pvUser));

    /**
     * Sets the secret key retrieval interface to use to get secret keys.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pIfSecKey       The secret key interface to use.
     *                          Use NULL to clear the currently set interface and clear all secret
     *                          keys from the user.
     * @param   pIfSecKeyHlp    The secret key helper interface to use.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnSetSecKeyIf,(PPDMIMEDIA pInterface, PPDMISECKEY pIfSecKey,
                                              PPDMISECKEYHLP pIfSecKeyHlp));

    /**
     * Get the media size in bytes.
     *
     * @returns Media size in bytes.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(uint64_t, pfnGetSize,(PPDMIMEDIA pInterface));

    /**
     * Gets the media sector size in bytes.
     *
     * @returns Media sector size in bytes.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(uint32_t, pfnGetSectorSize,(PPDMIMEDIA pInterface));

    /**
     * Check if the media is readonly or not.
     *
     * @returns true if readonly.
     * @returns false if read/write.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(bool, pfnIsReadOnly,(PPDMIMEDIA pInterface));

    /**
     * Get stored media geometry (physical CHS, PCHS) - BIOS property.
     * This is an optional feature of a media.
     *
     * @returns VBox status code.
     * @returns VERR_NOT_IMPLEMENTED if the media doesn't support storing the geometry.
     * @returns VERR_PDM_GEOMETRY_NOT_SET if the geometry hasn't been set using pfnBiosSetPCHSGeometry() yet.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pPCHSGeometry   Pointer to PCHS geometry (cylinders/heads/sectors).
     * @remark  This has no influence on the read/write operations.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnBiosGetPCHSGeometry,(PPDMIMEDIA pInterface, PPDMMEDIAGEOMETRY pPCHSGeometry));

    /**
     * Store the media geometry (physical CHS, PCHS) - BIOS property.
     * This is an optional feature of a media.
     *
     * @returns VBox status code.
     * @returns VERR_NOT_IMPLEMENTED if the media doesn't support storing the geometry.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pPCHSGeometry   Pointer to PCHS geometry (cylinders/heads/sectors).
     * @remark  This has no influence on the read/write operations.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnBiosSetPCHSGeometry,(PPDMIMEDIA pInterface, PCPDMMEDIAGEOMETRY pPCHSGeometry));

    /**
     * Get stored media geometry (logical CHS, LCHS) - BIOS property.
     * This is an optional feature of a media.
     *
     * @returns VBox status code.
     * @returns VERR_NOT_IMPLEMENTED if the media doesn't support storing the geometry.
     * @returns VERR_PDM_GEOMETRY_NOT_SET if the geometry hasn't been set using pfnBiosSetLCHSGeometry() yet.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pLCHSGeometry   Pointer to LCHS geometry (cylinders/heads/sectors).
     * @remark  This has no influence on the read/write operations.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnBiosGetLCHSGeometry,(PPDMIMEDIA pInterface, PPDMMEDIAGEOMETRY pLCHSGeometry));

    /**
     * Store the media geometry (logical CHS, LCHS) - BIOS property.
     * This is an optional feature of a media.
     *
     * @returns VBox status code.
     * @returns VERR_NOT_IMPLEMENTED if the media doesn't support storing the geometry.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pLCHSGeometry   Pointer to LCHS geometry (cylinders/heads/sectors).
     * @remark  This has no influence on the read/write operations.
     * @thread  The emulation thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnBiosSetLCHSGeometry,(PPDMIMEDIA pInterface, PCPDMMEDIAGEOMETRY pLCHSGeometry));

    /**
     * Checks if the device should be visible to the BIOS or not.
     *
     * @returns true if the device is visible to the BIOS.
     * @returns false if the device is not visible to the BIOS.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(bool, pfnBiosIsVisible,(PPDMIMEDIA pInterface));

    /**
     * Gets the media type.
     *
     * @returns media type.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(PDMMEDIATYPE, pfnGetType,(PPDMIMEDIA pInterface));

    /**
     * Gets the UUID of the media drive.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pUuid           Where to store the UUID on success.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnGetUuid,(PPDMIMEDIA pInterface, PRTUUID pUuid));

    /**
     * Discards the given range.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   paRanges        Array of ranges to discard.
     * @param   cRanges         Number of entries in the array.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnDiscard,(PPDMIMEDIA pInterface, PCRTRANGE paRanges, unsigned cRanges));

    /**
     * Allocate buffer memory which is suitable for I/O and might have special proerties for secure
     * environments (non-pageable memory for sensitive data which should not end up on the disk).
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   cb              Amount of memory to allocate.
     * @param   ppvNew          Where to store the pointer to the buffer on success.
     */
    DECLR3CALLBACKMEMBER(int, pfnIoBufAlloc, (PPDMIMEDIA pInterface, size_t cb, void **ppvNew));

    /**
     * Free memory allocated with PDMIMEDIA::pfnIoBufAlloc().
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pv              Pointer to the memory to free.
     * @param   cb              Amount of bytes given in PDMIMEDIA::pfnIoBufAlloc().
     */
    DECLR3CALLBACKMEMBER(int, pfnIoBufFree, (PPDMIMEDIA pInterface, void *pv, size_t cb));

} PDMIMEDIA;
/** PDMIMEDIA interface ID. */
#define PDMIMEDIA_IID                           "352f2fa2-52c0-4e51-ba3c-9f59b7043218"


/** Pointer to an asynchronous notification interface. */
typedef struct PDMIMEDIAASYNCPORT *PPDMIMEDIAASYNCPORT;
/**
 * Asynchronous version of the media interface (up).
 * Pair with PDMIMEDIAASYNC.
 */
typedef struct PDMIMEDIAASYNCPORT
{
    /**
     * Notify completion of a task.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pvUser          The user argument given in pfnStartWrite.
     * @param   rcReq           IPRT Status code of the completed request.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnTransferCompleteNotify, (PPDMIMEDIAASYNCPORT pInterface, void *pvUser, int rcReq));
} PDMIMEDIAASYNCPORT;
/** PDMIMEDIAASYNCPORT interface ID. */
#define PDMIMEDIAASYNCPORT_IID                  "22d38853-901f-4a71-9670-4d9da6e82317"


/** Pointer to an asynchronous media interface. */
typedef struct PDMIMEDIAASYNC *PPDMIMEDIAASYNC;
/**
 * Asynchronous version of PDMIMEDIA (down).
 * Pair with PDMIMEDIAASYNCPORT.
 */
typedef struct PDMIMEDIAASYNC
{
    /**
     * Start reading task.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   off             Offset to start reading from. Must be aligned to a sector boundary.
     * @param   paSegs          Pointer to the S/G segment array.
     * @param   cSegs           Number of entries in the array.
     * @param   cbRead          Number of bytes to read. Must be aligned to a sector boundary.
     * @param   pvUser          User data.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnStartRead,(PPDMIMEDIAASYNC pInterface, uint64_t off, PCRTSGSEG paSegs, unsigned cSegs, size_t cbRead, void *pvUser));

    /**
     * Start writing task.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   off             Offset to start writing at. Must be aligned to a sector boundary.
     * @param   paSegs          Pointer to the S/G segment array.
     * @param   cSegs           Number of entries in the array.
     * @param   cbWrite         Number of bytes to write. Must be aligned to a sector boundary.
     * @param   pvUser          User data.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnStartWrite,(PPDMIMEDIAASYNC pInterface, uint64_t off, PCRTSGSEG paSegs, unsigned cSegs, size_t cbWrite, void *pvUser));

    /**
     * Flush everything to disk.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pvUser          User argument which is returned in completion callback.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnStartFlush,(PPDMIMEDIAASYNC pInterface, void *pvUser));

    /**
     * Discards the given range.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   paRanges        Array of ranges to discard.
     * @param   cRanges         Number of entries in the array.
     * @param   pvUser          User argument which is returned in completion callback.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnStartDiscard,(PPDMIMEDIAASYNC pInterface, PCRTRANGE paRanges, unsigned cRanges, void *pvUser));

} PDMIMEDIAASYNC;
/** PDMIMEDIAASYNC interface ID. */
#define PDMIMEDIAASYNC_IID                      "4be209d3-ccb5-4297-82fe-7d8018bc6ab4"


/**
 * Opaque I/O request handle.
 *
 * The specific content depends on the driver implementing this interface.
 */
typedef struct PDMMEDIAEXIOREQINT *PDMMEDIAEXIOREQ;
/** Pointer to an I/O request handle. */
typedef PDMMEDIAEXIOREQ *PPDMMEDIAEXIOREQ;

/** A I/O request ID. */
typedef uint64_t PDMMEDIAEXIOREQID;

/**
 * I/O Request Type.
 */
typedef enum PDMMEDIAEXIOREQTYPE
{
    /** Invalid tpe. */
    PDMMEDIAEXIOREQTYPE_INVALID = 0,
    /** Flush request. */
    PDMMEDIAEXIOREQTYPE_FLUSH,
    /** Write request. */
    PDMMEDIAEXIOREQTYPE_WRITE,
    /** Read request. */
    PDMMEDIAEXIOREQTYPE_READ,
    /** Discard request. */
    PDMMEDIAEXIOREQTYPE_DISCARD
} PDMMEDIAEXIOREQTYPE;
/** Pointer to a I/O request type. */
typedef PDMMEDIAEXIOREQTYPE *PPDMMEDIAEXIOREQTYPE;

/**
 * I/O request state.
 */
typedef enum PDMMEDIAEXIOREQSTATE
{
    /** Invalid state. */
    PDMMEDIAEXIOREQSTATE_INVALID = 0,
    /** The request is active and being processed. */ 
    PDMMEDIAEXIOREQSTATE_ACTIVE,
    /** The request is suspended due to an error and no processing will take place. */
    PDMMEDIAEXIOREQSTATE_SUSPENDED,
    /** 32bit hack. */
    PDMMEDIAEXIOREQSTATE_32BIT_HACK = 0x7fffffff
} PDMMEDIAEXIOREQSTATE;
/** Pointer to a I/O request state. */
typedef PDMMEDIAEXIOREQSTATE *PPDMMEDIAEXIOREQSTATE;

/** @name I/O request specific flags
 * @{ */
/** Default behavior (async I/O).*/
#define PDMIMEDIAEX_F_DEFAULT                    (0)
/** The I/O request will be executed synchronously. */
#define PDMIMEDIAEX_F_SYNC                       RT_BIT_32(0)
/** Whether to suspend the VM on a recoverable error with
 * an appropriate error message (disk full, etc.).
 * The request will be retried by the driver implementing the interface
 * when the VM resumes the next time. However before suspending the request
 * the owner of the request will be notified using the PDMMEDIAEXPORT::pfnIoReqStateChanged.
 * The same goes for resuming the request after the VM was resumed.
 */
#define PDMIMEDIAEX_F_SUSPEND_ON_RECOVERABLE_ERR RT_BIT_32(1)
 /** Mask of valid flags. */
#define PDMIMEDIAEX_F_VALID                      (PDMIMEDIAEX_F_SYNC | PDMIMEDIAEX_F_SUSPEND_ON_RECOVERABLE_ERR)
/** @} */

/** Pointer to an extended media notification interface. */
typedef struct PDMIMEDIAEXPORT *PPDMIMEDIAEXPORT;

/**
 * Asynchronous version of the media interface (up).
 * Pair with PDMIMEDIAEXPORT.
 */
typedef struct PDMIMEDIAEXPORT
{
    /**
     * Notify completion of a I/O request.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   hIoReq          The I/O request handle.
     * @param   pvIoReqAlloc    The allocator specific memory for this request.
     * @param   rcReq           IPRT Status code of the completed request.
     *                          VERR_PDM_MEDIAEX_IOREQ_CANCELED if the request was canceled by a call to
     *                          PDMIMEDIAEX::pfnIoReqCancel.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnIoReqCompleteNotify, (PPDMIMEDIAEXPORT pInterface, PDMMEDIAEXIOREQ hIoReq,
                                                       void *pvIoReqAlloc, int rcReq));

    /**
     * Copy data from the memory buffer of the caller to the callees memory buffer for the given request.
     *
     * @returns VBox status code.
     * @retval  VERR_PDM_MEDIAEX_IOBUF_OVERFLOW if there is not enough room to store the data.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   hIoReq          The I/O request handle.
     * @param   pvIoReqAlloc    The allocator specific memory for this request.
     * @param   offDst          The destination offset from the start to write the data to.
     * @param   pSgBuf          The S/G buffer to read the data from.
     * @param   cbCopy          How many bytes to copy.
     */
    DECLR3CALLBACKMEMBER(int, pfnIoReqCopyFromBuf, (PPDMIMEDIAEXPORT pInterface, PDMMEDIAEXIOREQ hIoReq,
                                                    void *pvIoReqAlloc, uint32_t offDst, PRTSGBUF pSgBuf,
                                                    size_t cbCopy));

    /**
     * Copy data to the memory buffer of the caller from the callees memory buffer for the given request.
     *
     * @returns VBox status code.
     * @retval  VERR_PDM_MEDIAEX_IOBUF_UNDERRUN if there is not enough data to copy from the buffer.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   hIoReq          The I/O request handle.
     * @param   pvIoReqAlloc    The allocator specific memory for this request.
     * @param   offSrc          The offset from the start of the buffer to read the data from.
     * @param   pSgBuf          The S/G buffer to write the data to.
     * @param   cbCopy          How many bytes to copy.
     */
    DECLR3CALLBACKMEMBER(int, pfnIoReqCopyToBuf, (PPDMIMEDIAEXPORT pInterface, PDMMEDIAEXIOREQ hIoReq,
                                                  void *pvIoReqAlloc, uint32_t offSrc, PRTSGBUF pSgBuf,
                                                  size_t cbCopy));

    /**
     * Notify the request owner about a state change for the request.
     *
     * @returns nothing.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   hIoReq          The I/O request handle.
     * @param   pvIoReqAlloc    The allocator specific memory for this request.
     * @param   enmState        The new state of the request.
     */
    DECLR3CALLBACKMEMBER(void, pfnIoReqStateChanged, (PPDMIMEDIAEXPORT pInterface, PDMMEDIAEXIOREQ hIoReq,
                                                      void *pvIoReqAlloc, PDMMEDIAEXIOREQSTATE enmState));

} PDMIMEDIAEXPORT;

/** PDMIMEDIAAEXPORT interface ID. */
#define PDMIMEDIAEXPORT_IID                  "779f38d0-bcaa-4a49-af2b-6f63edd4181a"


/** Pointer to an extended media interface. */
typedef struct PDMIMEDIAEX *PPDMIMEDIAEX;

/**
 * Extended version of PDMIMEDIA (down).
 * Pair with PDMIMEDIAEXPORT.
 */
typedef struct PDMIMEDIAEX
{
    /**
     * Sets the size of the allocator specific memory for a I/O request.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   cbIoReqAlloc    The size of the allocator specific memory in bytes.
     * @thread  EMT.
     */
    DECLR3CALLBACKMEMBER(int, pfnIoReqAllocSizeSet, (PPDMIMEDIAEX pInterface, size_t cbIoReqAlloc));

    /**
     * Allocates a new I/O request.
     *
     * @returns VBox status code.
     * @retval  VERR_PDM_MEDIAEX_IOREQID_CONFLICT if the ID belongs to a still active request.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   phIoReq         Where to store the handle to the new I/O request on success.
     * @param   ppvIoReqAlloc   Where to store the pointer to the allocator specific memory on success.
     *                          NULL if the memory size was not set or set to 0.
     * @param   uIoReqId        A custom request ID which can be used to cancel the request.
     * @param   fFlags          A combination of PDMIMEDIAEX_F_* flags.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnIoReqAlloc, (PPDMIMEDIAEX pInterface, PPDMMEDIAEXIOREQ phIoReq, void **ppvIoReqAlloc,
                                              PDMMEDIAEXIOREQID uIoReqId, uint32_t fFlags));

    /**
     * Frees a given I/O request.
     *
     * @returns VBox status code.
     * @retval  VERR_PDM_MEDIAEX_IOREQ_INVALID_STATE if the given request is still active.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   hIoReq          The I/O request to free.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnIoReqFree, (PPDMIMEDIAEX pInterface, PDMMEDIAEXIOREQ hIoReq));

    /**
     * Cancels a I/O request identified by the ID.
     *
     * @returns VBox status code.
     * @retval  VERR_PDM_MEDIAEX_IOREQID_NOT_FOUND if the given ID could not be found in the active request list.
     *          (The request has either completed already or an invalid ID was given).
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   uIoReqId        The I/O request ID
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnIoReqCancel, (PPDMIMEDIAEX pInterface, PDMMEDIAEXIOREQID uIoReqId));

    /**
     * Start a reading request.
     *
     * @returns VBox status code.
     * @retval  VERR_PDM_MEDIAEX_IOREQ_CANCELED if the request was canceled  by a call to
     *          PDMIMEDIAEX::pfnIoReqCancel.
     * @retval  VINF_PDM_MEDIAEX_IOREQ_IN_PROGRESS if the request was successfully submitted but is still in progress.
     *          Completion will be notified through PDMIMEDIAEXPORT::pfnIoReqCompleteNotify with the appropriate status code.
     * @retval  VINF_SUCCESS if the request completed successfully.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   hIoReq          The I/O request to associate the read with.
     * @param   off             Offset to start reading from. Must be aligned to a sector boundary.
     * @param   cbRead          Number of bytes to read. Must be aligned to a sector boundary.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnIoReqRead, (PPDMIMEDIAEX pInterface, PDMMEDIAEXIOREQ hIoReq, uint64_t off, size_t cbRead));

    /**
     * Start a writing request.
     *
     * @returns VBox status code.
     * @retval  VERR_PDM_MEDIAEX_IOREQ_CANCELED if the request was canceled  by a call to
     *          PDMIMEDIAEX::pfnIoReqCancel.
     * @retval  VINF_PDM_MEDIAEX_IOREQ_IN_PROGRESS if the request was successfully submitted but is still in progress.
     *          Completion will be notified through PDMIMEDIAEXPORT::pfnIoReqCompleteNotify with the appropriate status code.
     * @retval  VINF_SUCCESS if the request completed successfully.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   hIoReq          The I/O request to associate the write with.
     * @param   off             Offset to start reading from. Must be aligned to a sector boundary.
     * @param   cbWrite         Number of bytes to write. Must be aligned to a sector boundary.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnIoReqWrite, (PPDMIMEDIAEX pInterface, PDMMEDIAEXIOREQ hIoReq, uint64_t off, size_t cbWrite));

    /**
     * Flush everything to disk.
     *
     * @returns VBox status code.
     * @retval  VERR_PDM_MEDIAEX_IOREQ_CANCELED if the request was canceled  by a call to
     *          PDMIMEDIAEX::pfnIoReqCancel.
     * @retval  VINF_PDM_MEDIAEX_IOREQ_IN_PROGRESS if the request was successfully submitted but is still in progress.
     *          Completion will be notified through PDMIMEDIAEXPORT::pfnIoReqCompleteNotify with the appropriate status code.
     * @retval  VINF_SUCCESS if the request completed successfully.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   hIoReq          The I/O request to associate the flush with.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnIoReqFlush, (PPDMIMEDIAEX pInterface, PDMMEDIAEXIOREQ hIoReq));

    /**
     * Discards the given range.
     *
     * @returns VBox status code.
     * @retval  VERR_PDM_MEDIAEX_IOREQ_CANCELED if the request was canceled  by a call to
     *          PDMIMEDIAEX::pfnIoReqCancel.
     * @retval  VINF_PDM_MEDIAEX_IOREQ_IN_PROGRESS if the request was successfully submitted but is still in progress.
     *          Completion will be notified through PDMIMEDIAEXPORT::pfnIoReqCompleteNotify with the appropriate status code.
     * @retval  VINF_SUCCESS if the request completed successfully.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   hIoReq          The I/O request to associate the discard with.
     * @param   paRanges        Array of ranges to discard.
     * @param   cRanges         Number of entries in the array.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(int, pfnIoReqDiscard, (PPDMIMEDIAEX pInterface, PDMMEDIAEXIOREQ hIoReq, PCRTRANGE paRanges, unsigned cRanges));

    /**
     * Returns the number of active I/O requests.
     *
     * @returns Number of active I/O requests.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(uint32_t, pfnIoReqGetActiveCount, (PPDMIMEDIAEX pInterface));

    /**
     * Returns the number of suspended requests.
     *
     * @returns Number of suspended I/O requests.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @thread  Any thread.
     */
    DECLR3CALLBACKMEMBER(uint32_t, pfnIoReqGetSuspendedCount, (PPDMIMEDIAEX pInterface));

    /**
     * Gets the first suspended request handle.
     *
     * @returns VBox status code.
     * @retval  VERR_NOT_FOUND if there is no suspended request waiting.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   phIoReq         Where to store the request handle on success.
     * @param   ppvIoReqAlloc   Where to store the pointer to the allocator specific memory on success.
     * @thread  Any thread.
     *
     * @note This should only be called when the VM is suspended to make sure the request doesn't suddenly
     *       changes into the active state again. The only purpose for this method for now is to make saving the state
     *       possible without breaking saved state versions.
     */
    DECLR3CALLBACKMEMBER(int, pfnIoReqQuerySuspendedStart, (PPDMIMEDIAEX pInterface, PPDMMEDIAEXIOREQ phIoReq, void **ppvIoReqAlloc));

    /**
     * Gets the next suspended request handle.
     *
     * @returns VBox status code.
     * @retval  VERR_NOT_FOUND if there is no suspended request waiting.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   hIoReq          The current request handle.
     * @param   phIoReqNext     Where to store the request handle on success.
     * @param   ppvIoReqAllocNext Where to store the pointer to the allocator specific memory on success.
     * @thread  Any thread.
     *
     * @note This should only be called when the VM is suspended to make sure the request doesn't suddenly
     *       changes into the active state again. The only purpose for this method for now is to make saving the state
     *       possible without breaking saved state versions.
     */
    DECLR3CALLBACKMEMBER(int, pfnIoReqQuerySuspendedNext, (PPDMIMEDIAEX pInterface, PDMMEDIAEXIOREQ hIoReq,
                                                           PPDMMEDIAEXIOREQ phIoReqNext, void **ppvIoReqAllocNext));

    /**
     * Saves the given I/O request state in the provided saved state unit.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pSSM            The SSM handle.
     * @param   hIoReq          The request handle to save.
     */
    DECLR3CALLBACKMEMBER(int, pfnIoReqSuspendedSave, (PPDMIMEDIAEX pInterface, PSSMHANDLE pSSM, PDMMEDIAEXIOREQ hIoReq));

    /**
     * Load a suspended request state from the given saved state unit and link it into the suspended list.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to the interface structure containing the called function pointer.
     * @param   pSSM            The SSM handle to read the state from.
     * @param   hIoReq          The request handle to load the state into.
     */
    DECLR3CALLBACKMEMBER(int, pfnIoReqSuspendedLoad, (PPDMIMEDIAEX pInterface, PSSMHANDLE pSSM, PDMMEDIAEXIOREQ hIoReq));

} PDMIMEDIAEX;
/** PDMIMEDIAEX interface ID. */
#define PDMIMEDIAEX_IID                      "8856ba6a-773b-40ce-92a2-431cd06e678e"

/**
 * Data direction.
 */
typedef enum PDMSCSIREQUESTTXDIR
{
    PDMSCSIREQUESTTXDIR_UNKNOWN     = 0x00,
    PDMSCSIREQUESTTXDIR_FROM_DEVICE = 0x01,
    PDMSCSIREQUESTTXDIR_TO_DEVICE   = 0x02,
    PDMSCSIREQUESTTXDIR_NONE        = 0x03,
    PDMSCSIREQUESTTXDIR_32BIT_HACK  = 0x7fffffff
} PDMSCSIREQUESTTXDIR;

/**
 * SCSI request structure.
 */
typedef struct PDMSCSIREQUEST
{
    /** The logical unit. */
    uint32_t               uLogicalUnit;
    /** Direction of the data flow. */
    PDMSCSIREQUESTTXDIR    uDataDirection;
    /** Size of the SCSI CDB. */
    uint32_t               cbCDB;
    /** Pointer to the SCSI CDB. */
    uint8_t               *pbCDB;
    /** Overall size of all scatter gather list elements
     *  for data transfer if any. */
    uint32_t               cbScatterGather;
    /** Number of elements in the scatter gather list. */
    uint32_t               cScatterGatherEntries;
    /** Pointer to the head of the scatter gather list. */
    PRTSGSEG               paScatterGatherHead;
    /** Size of the sense buffer. */
    uint32_t               cbSenseBuffer;
    /** Pointer to the sense buffer. *
     * Current assumption that the sense buffer is not scattered. */
    uint8_t               *pbSenseBuffer;
    /** Opaque user data for use by the device. Left untouched by everything else! */
    void                  *pvUser;
} PDMSCSIREQUEST, *PPDMSCSIREQUEST;
/** Pointer to a const SCSI request structure. */
typedef const PDMSCSIREQUEST *PCSCSIREQUEST;

/** Pointer to a SCSI port interface. */
typedef struct PDMISCSIPORT *PPDMISCSIPORT;
/**
 * SCSI command execution port interface (down).
 * Pair with PDMISCSICONNECTOR.
 */
typedef struct PDMISCSIPORT
{

    /**
     * Notify the device on request completion.
     *
     * @returns VBox status code.
     * @param   pInterface    Pointer to this interface.
     * @param   pSCSIRequest  Pointer to the finished SCSI request.
     * @param   rcCompletion  SCSI_STATUS_* code for the completed request.
     * @param   fRedo         Flag whether the request can to be redone
     *                        when it failed.
     * @param   rcReq         The status code the request completed with (VERR_*)
     *                        Should be only used to choose the correct error message
     *                        displayed to the user if the error can be fixed by him
     *                        (fRedo is true).
     */
     DECLR3CALLBACKMEMBER(int, pfnSCSIRequestCompleted, (PPDMISCSIPORT pInterface, PPDMSCSIREQUEST pSCSIRequest,
                                                         int rcCompletion, bool fRedo, int rcReq));

    /**
     * Returns the storage controller name, instance and LUN of the attached medium.
     *
     * @returns VBox status.
     * @param   pInterface      Pointer to this interface.
     * @param   ppcszController Where to store the name of the storage controller.
     * @param   piInstance      Where to store the instance number of the controller.
     * @param   piLUN           Where to store the LUN of the attached device.
     */
    DECLR3CALLBACKMEMBER(int, pfnQueryDeviceLocation, (PPDMISCSIPORT pInterface, const char **ppcszController,
                                                       uint32_t *piInstance, uint32_t *piLUN));

} PDMISCSIPORT;
/** PDMISCSIPORT interface ID. */
#define PDMISCSIPORT_IID                        "05d9fc3b-e38c-4b30-8344-a323feebcfe5"

/**
 * LUN type.
 */
typedef enum PDMSCSILUNTYPE
{
    PDMSCSILUNTYPE_INVALID = 0,
    PDMSCSILUNTYPE_SBC,         /** Hard disk (SBC) */
    PDMSCSILUNTYPE_MMC,         /** CD/DVD drive (MMC) */
    PDMSCSILUNTYPE_SSC,         /** Tape drive (SSC) */
    PDMSCSILUNTYPE_32BIT_HACK = 0x7fffffff
} PDMSCSILUNTYPE, *PPDMSCSILUNTYPE;


/** Pointer to a SCSI connector interface. */
typedef struct PDMISCSICONNECTOR *PPDMISCSICONNECTOR;
/**
 * SCSI command execution connector interface (up).
 * Pair with PDMISCSIPORT.
 */
typedef struct PDMISCSICONNECTOR
{

    /**
     * Submits a SCSI request for execution.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to this interface.
     * @param   pSCSIRequest    Pointer to the SCSI request to execute.
     */
     DECLR3CALLBACKMEMBER(int, pfnSCSIRequestSend, (PPDMISCSICONNECTOR pInterface, PPDMSCSIREQUEST pSCSIRequest));

    /**
     * Queries the type of the attached LUN.
     *
     * @returns VBox status code.
     * @param   pInterface      Pointer to this interface.
     * @param   iLUN            The logical unit number.
     * @param   pSCSIRequest    Pointer to the LUN to be returned.
     */
     DECLR3CALLBACKMEMBER(int, pfnQueryLUNType, (PPDMISCSICONNECTOR pInterface, uint32_t iLun, PPDMSCSILUNTYPE pLUNType));

} PDMISCSICONNECTOR;
/** PDMISCSICONNECTOR interface ID. */
#define PDMISCSICONNECTOR_IID                   "94465fbd-a2f2-447e-88c9-7366421bfbfe"

/** @} */

RT_C_DECLS_END

#endif
