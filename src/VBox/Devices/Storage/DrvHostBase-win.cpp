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
#include <iprt/win/windows.h>
#include <winioctl.h>
#include <ntddscsi.h>
#pragma warning(default : 4163)
#undef _interlockedbittestandset
#undef _interlockedbittestandreset
#undef _interlockedbittestandset64
#undef _interlockedbittestandreset64

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

