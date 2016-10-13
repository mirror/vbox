/* $Id$ */
/** @file
 * DrvHostBase - Host base drive access driver, FreeBSD specifics.
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
#include <sys/cdefs.h>
#include <sys/param.h>
#include <errno.h>
#include <stdio.h>
#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/scsi/scsi_message.h>
#include <cam/scsi/scsi_pass.h>
#include <VBox/scsi.h>
#include <iprt/log.h>

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
    Assert(cbCmd <= 16 && cbCmd >= 1);
    const uint32_t cbBuf = pcbBuf ? *pcbBuf : 0;
    if (pcbBuf)
        *pcbBuf = 0;

    Assert(pThis->ppScsiTaskDI);

    int rc = VERR_GENERAL_FAILURE;
    SCSITaskInterface **ppScsiTaskI = (*pThis->ppScsiTaskDI)->CreateSCSITask(pThis->ppScsiTaskDI);
    if (!ppScsiTaskI)
        return VERR_NO_MEMORY;
    do
    {
        /* Setup the scsi command. */
        SCSICommandDescriptorBlock cdb = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
        memcpy(&cdb[0], pbCmd, cbCmd);
        IOReturn irc = (*ppScsiTaskI)->SetCommandDescriptorBlock(ppScsiTaskI, cdb, cbCmd);
        AssertBreak(irc == kIOReturnSuccess);

        /* Setup the buffer. */
        if (enmTxDir == PDMMEDIATXDIR_NONE)
            irc = (*ppScsiTaskI)->SetScatterGatherEntries(ppScsiTaskI, NULL, 0, 0, kSCSIDataTransfer_NoDataTransfer);
        else
        {
            IOVirtualRange Range = { (IOVirtualAddress)pvBuf, cbBuf };
            irc = (*ppScsiTaskI)->SetScatterGatherEntries(ppScsiTaskI, &Range, 1, cbBuf,
                                                          enmTxDir == PDMMEDIATXDIR_FROM_DEVICE
                                                          ? kSCSIDataTransfer_FromTargetToInitiator
                                                          : kSCSIDataTransfer_FromInitiatorToTarget);
        }
        AssertBreak(irc == kIOReturnSuccess);

        /* Set the timeout. */
        irc = (*ppScsiTaskI)->SetTimeoutDuration(ppScsiTaskI, cTimeoutMillies ? cTimeoutMillies : 30000 /*ms*/);
        AssertBreak(irc == kIOReturnSuccess);

        /* Execute the command and get the response. */
        SCSI_Sense_Data SenseData = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
        SCSIServiceResponse     ServiceResponse = kSCSIServiceResponse_Request_In_Process;
        SCSITaskStatus TaskStatus = kSCSITaskStatus_GOOD;
        UInt64 cbReturned = 0;
        irc = (*ppScsiTaskI)->ExecuteTaskSync(ppScsiTaskI, &SenseData, &TaskStatus, &cbReturned);
        AssertBreak(irc == kIOReturnSuccess);
        if (pcbBuf)
            *pcbBuf = (int32_t)cbReturned;

        irc = (*ppScsiTaskI)->GetSCSIServiceResponse(ppScsiTaskI, &ServiceResponse);
        AssertBreak(irc == kIOReturnSuccess);
        AssertBreak(ServiceResponse == kSCSIServiceResponse_TASK_COMPLETE);

        if (TaskStatus == kSCSITaskStatus_GOOD)
            rc = VINF_SUCCESS;
        else if (   TaskStatus == kSCSITaskStatus_CHECK_CONDITION
                 && pbSense)
        {
            memset(pbSense, 0, cbSense); /* lazy */
            memcpy(pbSense, &SenseData, RT_MIN(sizeof(SenseData), cbSense));
            rc = VERR_UNRESOLVED_ERROR;
        }
        /** @todo convert sense codes when caller doesn't wish to do this himself. */
        /*else if (   TaskStatus == kSCSITaskStatus_CHECK_CONDITION
                 && SenseData.ADDITIONAL_SENSE_CODE == 0x3A)
            rc = VERR_MEDIA_NOT_PRESENT; */
        else
        {
            rc = enmTxDir == PDMMEDIATXDIR_NONE
               ? VERR_DEV_IO_ERROR
               : enmTxDir == PDMMEDIATXDIR_FROM_DEVICE
               ? VERR_READ_ERROR
               : VERR_WRITE_ERROR;
            if (pThis->cLogRelErrors++ < 10)
                LogRel(("DVD scsi error: cmd={%.*Rhxs} TaskStatus=%#x key=%#x ASC=%#x ASCQ=%#x (%Rrc)\n",
                        cbCmd, pbCmd, TaskStatus, SenseData.SENSE_KEY, SenseData.ADDITIONAL_SENSE_CODE,
                        SenseData.ADDITIONAL_SENSE_CODE_QUALIFIER, rc));
        }
    } while (0);

    (*ppScsiTaskI)->Release(ppScsiTaskI);

    return rc;
}


DECLHIDDEN(int) drvHostBaseGetMediaSizeOs(PDRVHOSTBASE pThis, uint64_t *pcb)
{
    /*
     * Try a READ_CAPACITY command...
     */
    struct
    {
        uint32_t cBlocks;
        uint32_t cbBlock;
    }           Buf = {0, 0};
    uint32_t    cbBuf = sizeof(Buf);
    uint8_t     abCmd[16] =
    {
        SCSI_READ_CAPACITY, 0, 0, 0, 0, 0, 0,
        0,0,0,0,0,0,0,0,0
    };
    int rc = drvHostBaseScsiCmdOs(pThis, abCmd, 6, PDMMEDIATXDIR_FROM_DEVICE, &Buf, &cbBuf, NULL, 0, 0);
    if (RT_SUCCESS(rc))
    {
        Assert(cbBuf == sizeof(Buf));
        Buf.cBlocks = RT_BE2H_U32(Buf.cBlocks);
        Buf.cbBlock = RT_BE2H_U32(Buf.cbBlock);
        //if (Buf.cbBlock > 2048) /* everyone else is doing this... check if it needed/right.*/
        //    Buf.cbBlock = 2048;
        pThis->cbBlock = Buf.cbBlock;

        *pcb = (uint64_t)Buf.cBlocks * Buf.cbBlock;
    }
    return rc;
}

