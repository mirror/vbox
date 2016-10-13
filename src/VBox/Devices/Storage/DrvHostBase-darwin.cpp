/* $Id$ */
/** @file
 * DrvHostBase - Host base drive access driver, OS X specifics.
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
#include <mach/mach.h>
#include <Carbon/Carbon.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>
#include <IOKit/scsi/SCSITaskLib.h>
#include <IOKit/scsi/SCSICommandOperationCodes.h>
#include <IOKit/IOBSD.h>
#include <DiskArbitration/DiskArbitration.h>
#include <mach/mach_error.h>
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

