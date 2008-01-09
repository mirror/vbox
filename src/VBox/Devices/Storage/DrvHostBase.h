/* $Id$ */
/** @file
 * DrvHostBase - Host base drive access driver.
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

#ifndef __HostDrvBase_h__
#define __HostDrvBase_h__

#include <VBox/cdefs.h>

__BEGIN_DECLS


/** Pointer to host base drive access driver instance data. */
typedef struct DRVHOSTBASE *PDRVHOSTBASE;
/**
 * Host base drive access driver instance data.
 */
typedef struct DRVHOSTBASE
{
    /** Critical section used to serialize access to the handle and other
     * members of this struct. */
    RTCRITSECT              CritSect;
    /** Pointer driver instance. */
    PPDMDRVINS              pDrvIns;
    /** Drive type. */
    PDMBLOCKTYPE            enmType;
    /** Visible to the BIOS. */
    bool                    fBiosVisible;
    /** The configuration readonly value. */
    bool                    fReadOnlyConfig;
    /** The current readonly status. */
    bool                    fReadOnly;
    /** Flag whether failure to attach is an error or not. */
    bool                    fAttachFailError;
    /** Flag whether to keep instance working (as unmounted though). */
    bool                    fKeepInstance;
    /** Device name (MMHeap). */
    char                   *pszDevice;
    /** Device name to open (RTStrFree). */
    char                   *pszDeviceOpen;
#ifdef RT_OS_SOLARIS
    /** Device name of raw device (RTStrFree). */
    char                   *pszRawDeviceOpen;
#endif
    /** Uuid of the drive. */
    RTUUID                  Uuid;

    /** Pointer to the block port interface above us. */
    PPDMIBLOCKPORT          pDrvBlockPort;
    /** Pointer to the mount notify interface above us. */
    PPDMIMOUNTNOTIFY        pDrvMountNotify;
    /** Our block interface. */
    PDMIBLOCK               IBlock;
    /** Our block interface. */
    PDMIBLOCKBIOS           IBlockBios;
    /** Our mountable interface. */
    PDMIMOUNT               IMount;

    /** Media present indicator. */
    bool volatile           fMediaPresent;
    /** Locked indicator. */
    bool                    fLocked;
    /** The size of the media currently in the drive.
     * This is invalid if no drive is in the drive. */
    uint64_t volatile       cbSize;
#ifndef RT_OS_DARWIN
    /** The filehandle of the device. */
    RTFILE                  FileDevice;
#endif
#ifdef RT_OS_SOLARIS
    /** The raw filehandle of the device. */
    RTFILE                  FileRawDevice;
#endif

    /** Handle of the poller thread. */
    RTTHREAD                ThreadPoller;
#ifndef RT_OS_WINDOWS
    /** Event semaphore the thread will wait on. */
    RTSEMEVENT              EventPoller;
#endif
    /** The poller interval. */
    unsigned                cMilliesPoller;
    /** The shutdown indicator. */
    bool volatile           fShutdownPoller;

    /** BIOS PCHS geometry. */
    PDMMEDIAGEOMETRY        PCHSGeometry;
    /** BIOS LCHS geometry. */
    PDMMEDIAGEOMETRY        LCHSGeometry;

    /** The number of errors that could go into the release log. (flood gate) */
    uint32_t                cLogRelErrors;

#ifdef RT_OS_DARWIN
    /** The master port. */
    mach_port_t             MasterPort;
    /** The MMC-2 Device Interface. (This is only used to get the scsi task interface.) */
    MMCDeviceInterface    **ppMMCDI;
    /** The SCSI Task Device Interface. */
    SCSITaskDeviceInterface **ppScsiTaskDI;
    /** The block size. Set when querying the media size. */
    uint32_t                cbBlock;
    /** The disk arbitration session reference. NULL if we didn't have to claim & unmount the device. */
    DASessionRef            pDASession;
    /** The disk arbritation disk reference. NULL if we didn't have to claim & unmount the device. */
    DADiskRef               pDADisk;
#endif

#ifdef RT_OS_WINDOWS
    /** Handle to the window we use to catch the device change broadcast messages. */
    volatile HWND           hwndDeviceChange;
    /** The unit mask. */
    DWORD                   fUnitMask;
#endif


    /**
     * Performs the locking / unlocking of the device.
     *
     * This callback pointer should be set to NULL if the device doesn't support this action.
     *
     * @returns VBox status code.
     * @param   pThis       Pointer to the instance data.
     * @param   fLock       Set if locking, clear if unlocking.
     */
    DECLCALLBACKMEMBER(int, pfnDoLock)(PDRVHOSTBASE pThis, bool fLock);

    /**
     * Queries the media size.
     * Can also be used to perform actions on media change.
     *
     * This callback pointer should be set to NULL if the default action is fine for this device.
     *
     * @returns VBox status code.
     * @param   pThis       Pointer to the instance data.
     * @param   pcb         Where to store the media size in bytes.
     */
    DECLCALLBACKMEMBER(int, pfnGetMediaSize)(PDRVHOSTBASE pThis, uint64_t *pcb);

    /***
     * Performs the polling operation.
     *
     * @returns VBox status code. (Failure means retry.)
     * @param   pThis       Pointer to the instance data.
     */
    DECLCALLBACKMEMBER(int, pfnPoll)(PDRVHOSTBASE pThis);
} DRVHOSTBASE;


int DRVHostBaseInitData(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle, PDMBLOCKTYPE enmType);
int DRVHostBaseInitFinish(PDRVHOSTBASE pThis);
int DRVHostBaseMediaPresent(PDRVHOSTBASE pThis);
void DRVHostBaseMediaNotPresent(PDRVHOSTBASE pThis);
DECLCALLBACK(void) DRVHostBaseDestruct(PPDMDRVINS pDrvIns);
#ifdef RT_OS_DARWIN
DECLCALLBACK(int) DRVHostBaseScsiCmd(PDRVHOSTBASE pThis, const uint8_t *pbCmd, size_t cbCmd, PDMBLOCKTXDIR enmTxDir,
                                     void *pvBuf, size_t *pcbBuf, uint8_t *pbSense, size_t cbSense, uint32_t cTimeoutMillies);
#endif


/** Makes a PDRVHOSTBASE out of a PPDMIMOUNT. */
#define PDMIMOUNT_2_DRVHOSTBASE(pInterface)        ( (PDRVHOSTBASE)((uintptr_t)pInterface - RT_OFFSETOF(DRVHOSTBASE, IMount)) )

/** Makes a PDRVHOSTBASE out of a PPDMIBLOCK. */
#define PDMIBLOCK_2_DRVHOSTBASE(pInterface)        ( (PDRVHOSTBASE)((uintptr_t)pInterface - RT_OFFSETOF(DRVHOSTBASE, IBlock)) )

__END_DECLS

#endif
