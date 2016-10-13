/* $Id$ */
/** @file
 * DrvHostBase - Host base drive access driver, Windows specifics.
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
 */
#define LOG_GROUP LOG_GROUP_DRV_HOST_BASE
#pragma warning(disable : 4163)
#define _interlockedbittestandset      they_messed_it_up_in_winnt_h_this_time_sigh__interlockedbittestandset
#define _interlockedbittestandreset    they_messed_it_up_in_winnt_h_this_time_sigh__interlockedbittestandreset
#define _interlockedbittestandset64    they_messed_it_up_in_winnt_h_this_time_sigh__interlockedbittestandset64
#define _interlockedbittestandreset64  they_messed_it_up_in_winnt_h_this_time_sigh__interlockedbittestandreset64

#define WIN32_NO_STATUS
#include <iprt/win/windows.h>
#include <dbt.h>
#undef WIN32_NO_STATUS

#include <winioctl.h>
#include <ntddscsi.h>
#pragma warning(default : 4163)
#undef _interlockedbittestandset
#undef _interlockedbittestandreset
#undef _interlockedbittestandset64
#undef _interlockedbittestandreset64
#include <ntstatus.h>

/* from ntdef.h */
typedef LONG NTSTATUS;

/* from ntddk.h */
typedef struct _IO_STATUS_BLOCK {
    union {
        NTSTATUS Status;
        PVOID Pointer;
    };
    ULONG_PTR Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;


/* from ntinternals.com */
typedef enum _FS_INFORMATION_CLASS {
    FileFsVolumeInformation=1,
    FileFsLabelInformation,
    FileFsSizeInformation,
    FileFsDeviceInformation,
    FileFsAttributeInformation,
    FileFsControlInformation,
    FileFsFullSizeInformation,
    FileFsObjectIdInformation,
    FileFsMaximumInformation
} FS_INFORMATION_CLASS, *PFS_INFORMATION_CLASS;

typedef struct _FILE_FS_SIZE_INFORMATION {
    LARGE_INTEGER   TotalAllocationUnits;
    LARGE_INTEGER   AvailableAllocationUnits;
    ULONG           SectorsPerAllocationUnit;
    ULONG           BytesPerSector;
} FILE_FS_SIZE_INFORMATION, *PFILE_FS_SIZE_INFORMATION;

extern "C"
NTSTATUS __stdcall NtQueryVolumeInformationFile(
        /*IN*/ HANDLE               FileHandle,
        /*OUT*/ PIO_STATUS_BLOCK    IoStatusBlock,
        /*OUT*/ PVOID               FileSystemInformation,
        /*IN*/ ULONG                Length,
        /*IN*/ FS_INFORMATION_CLASS FileSystemInformationClass );

#include <iprt/file.h>
#include <VBox/scsi.h>

#include "DrvHostBase.h"

DECLHIDDEN(int) drvHostBaseScsiCmdOs(PDRVHOSTBASE pThis, const uint8_t *pbCmd, size_t cbCmd, PDMMEDIATXDIR enmTxDir,
                                     void *pvBuf, uint32_t *pcbBuf, uint8_t *pbSense, size_t cbSense, uint32_t cTimeoutMillies)
{
    /*
     * Minimal input validation.
     */
    Assert(enmTxDir == PDMMEDIATXDIR_NONE || enmTxDir == PDMMEDIATXDIR_FROM_DEVICE || enmTxDir == PDMMEDIATXDIR_TO_DEVICE);
    Assert(!pvBuf || pcbBuf);
    Assert(pvBuf || enmTxDir == PDMMEDIATXDIR_NONE);
    Assert(pbSense || !cbSense);
    AssertPtr(pbCmd);
    Assert(cbCmd <= 16 && cbCmd >= 1); RT_NOREF(cbCmd);

    int rc = VERR_GENERAL_FAILURE;
    int direction;
    struct _REQ
    {
        SCSI_PASS_THROUGH_DIRECT spt;
        uint8_t aSense[64];
    } Req;
    DWORD cbReturned = 0;

    switch (enmTxDir)
    {
        case PDMMEDIATXDIR_NONE:
            direction = SCSI_IOCTL_DATA_UNSPECIFIED;
            break;
        case PDMMEDIATXDIR_FROM_DEVICE:
            Assert(*pcbBuf != 0);
            /* Make sure that the buffer is clear for commands reading
             * data. The actually received data may be shorter than what
             * we expect, and due to the unreliable feedback about how much
             * data the ioctl actually transferred, it's impossible to
             * prevent that. Returning previous buffer contents may cause
             * security problems inside the guest OS, if users can issue
             * commands to the CDROM device. */
            memset(pvBuf, '\0', *pcbBuf);
            direction = SCSI_IOCTL_DATA_IN;
            break;
        case PDMMEDIATXDIR_TO_DEVICE:
            direction = SCSI_IOCTL_DATA_OUT;
            break;
        default:
            AssertMsgFailed(("enmTxDir invalid!\n"));
            direction = SCSI_IOCTL_DATA_UNSPECIFIED;
    }
    memset(&Req, '\0', sizeof(Req));
    Req.spt.Length = sizeof(Req.spt);
    Req.spt.CdbLength = 12;
    memcpy(Req.spt.Cdb, pbCmd, Req.spt.CdbLength);
    Req.spt.DataBuffer = pvBuf;
    Req.spt.DataTransferLength = *pcbBuf;
    Req.spt.DataIn = direction;
    Req.spt.TimeOutValue = (cTimeoutMillies + 999) / 1000; /* Convert to seconds */
    Assert(cbSense <= sizeof(Req.aSense));
    Req.spt.SenseInfoLength = (UCHAR)RT_MIN(sizeof(Req.aSense), cbSense);
    Req.spt.SenseInfoOffset = RT_OFFSETOF(struct _REQ, aSense);
    if (DeviceIoControl((HANDLE)RTFileToNative(pThis->hFileDevice), IOCTL_SCSI_PASS_THROUGH_DIRECT,
                        &Req, sizeof(Req), &Req, sizeof(Req), &cbReturned, NULL))
    {
        if (cbReturned > RT_OFFSETOF(struct _REQ, aSense))
            memcpy(pbSense, Req.aSense, cbSense);
        else
            memset(pbSense, '\0', cbSense);
        /* Windows shares the property of not properly reflecting the actually
         * transferred data size. See above. Assume that everything worked ok.
         * Except if there are sense information. */
        rc = (pbSense[2] & 0x0f) == SCSI_SENSE_NONE
                 ? VINF_SUCCESS
                 : VERR_DEV_IO_ERROR;
    }
    else
        rc = RTErrConvertFromWin32(GetLastError());
    Log2(("%s: scsistatus=%d bytes returned=%d tlength=%d\n", __FUNCTION__, Req.spt.ScsiStatus, cbReturned, Req.spt.DataTransferLength));

    return rc;
}

DECLHIDDEN(int) drvHostBaseGetMediaSizeOs(PDRVHOSTBASE pThis, uint64_t *pcb)
{
    int rc = VERR_GENERAL_FAILURE;

    if (PDMMEDIATYPE_IS_FLOPPY(pThis->enmType))
    {
        DISK_GEOMETRY   geom;
        DWORD           cbBytesReturned;
        int             cbSectors;

        memset(&geom, 0, sizeof(geom));
        rc = DeviceIoControl((HANDLE)RTFileToNative(pThis->hFileDevice), IOCTL_DISK_GET_DRIVE_GEOMETRY,
                             NULL, 0, &geom, sizeof(geom), &cbBytesReturned,  NULL);
        if (rc) {
            cbSectors = geom.Cylinders.QuadPart * geom.TracksPerCylinder * geom.SectorsPerTrack;
            *pcb = cbSectors * geom.BytesPerSector;
            rc = VINF_SUCCESS;
        }
        else
        {
            DWORD   dwLastError;

            dwLastError = GetLastError();
            rc = RTErrConvertFromWin32(dwLastError);
            Log(("DrvHostFloppy: IOCTL_DISK_GET_DRIVE_GEOMETRY(%s) failed, LastError=%d rc=%Rrc\n",
                 pThis->pszDevice, dwLastError, rc));
            return rc;
        }
    }
    else
    {
        /* use NT api, retry a few times if the media is being verified. */
        IO_STATUS_BLOCK             IoStatusBlock = {0};
        FILE_FS_SIZE_INFORMATION    FsSize= {0};
        NTSTATUS rcNt = NtQueryVolumeInformationFile((HANDLE)RTFileToNative(pThis->hFileDevice),  &IoStatusBlock,
                                                     &FsSize, sizeof(FsSize), FileFsSizeInformation);
        int cRetries = 5;
        while (rcNt == STATUS_VERIFY_REQUIRED && cRetries-- > 0)
        {
            RTThreadSleep(10);
            rcNt = NtQueryVolumeInformationFile((HANDLE)RTFileToNative(pThis->hFileDevice),  &IoStatusBlock,
                                                &FsSize, sizeof(FsSize), FileFsSizeInformation);
        }
        if (rcNt >= 0)
        {
            *pcb = FsSize.TotalAllocationUnits.QuadPart * FsSize.BytesPerSector;
            return VINF_SUCCESS;
        }

        /* convert nt status code to VBox status code. */
        /** @todo Make conversion function!. */
        switch (rcNt)
        {
            case STATUS_NO_MEDIA_IN_DEVICE:     rc = VERR_MEDIA_NOT_PRESENT; break;
            case STATUS_VERIFY_REQUIRED:        rc = VERR_TRY_AGAIN; break;
        }
        LogFlow(("drvHostBaseGetMediaSize: NtQueryVolumeInformationFile -> %#lx\n", rcNt, rc));
    }
    return rc;
}


DECLHIDDEN(int) drvHostBaseReadOs(PDRVHOSTBASE pThis, uint64_t off, void *pvBuf, size_t cbRead)
{
    return RTFileReadAt(pThis->hFileDevice, off, pvBuf, cbRead, NULL);
}


DECLHIDDEN(int) drvHostBaseWriteOs(PDRVHOSTBASE pThis, uint64_t off, const void *pvBuf, size_t cbWrite)
{
    return RTFileWriteAt(pThis->hFileDevice, off, pvBuf, cbWrite, NULL);
}


DECLHIDDEN(int) drvHostBaseFlushOs(PDRVHOSTBASE pThis)
{
    return RTFileFlush(pThis->hFileDevice);
}


DECLHIDDEN(int) drvHostBasePollerWakeupOs(PDRVHOSTBASE pThis)
{
    if (pThis->hwndDeviceChange)
        PostMessage(pThis->hwndDeviceChange, WM_CLOSE, 0, 0); /* default win proc will destroy the window */

    return VINF_SUCCESS;
}


DECLHIDDEN(void) drvHostBaseDestructOs(PDRVHOSTBASE pThis)
{
    if (pThis->EventPoller != NULL)
    {
        RTSemEventDestroy(pThis->EventPoller);
        pThis->EventPoller = NULL;
    }

    if (pThis->hwndDeviceChange)
    {
        if (SetWindowLongPtr(pThis->hwndDeviceChange, GWLP_USERDATA, 0) == (LONG_PTR)pThis)
            PostMessage(pThis->hwndDeviceChange, WM_CLOSE, 0, 0); /* default win proc will destroy the window */
        pThis->hwndDeviceChange = NULL;
    }

    if (pThis->hFileDevice != NIL_RTFILE)
    {
        int rc = RTFileClose(pThis->hFileDevice);
        AssertRC(rc);
        pThis->hFileDevice = NIL_RTFILE;
    }
}

