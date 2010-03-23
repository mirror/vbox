/* $Id$ */
/** @file
 * Virtual SCSI driver: Sense handling
 */

/*
 * Copyright (C) 2006-2010 Sun Microsystems, Inc.
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
#define LOG_GROUP LOG_GROUP_VSCSI
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/string.h>

#include "VSCSIInternal.h"

int vscsiReqSenseOkSet(PVSCSIREQINT pVScsiReq)
{
    if (pVScsiReq->cbSense < 14)
        return SCSI_STATUS_OK;

    AssertMsgReturn(pVScsiReq->pbSense, ("Sense buffer pointer is NULL\n"), SCSI_STATUS_OK);
    memset(pVScsiReq->pbSense, 0, pVScsiReq->cbSense);

    pVScsiReq->pbSense[0]  = (1 << 7) | SCSI_SENSE_RESPONSE_CODE_CURR_FIXED; /* Fixed format */
    pVScsiReq->pbSense[2]  = SCSI_SENSE_NONE;
    pVScsiReq->pbSense[7]  = 10;
    pVScsiReq->pbSense[12] = SCSI_ASC_NONE;
    pVScsiReq->pbSense[13] = SCSI_ASC_NONE; /* Should be ASCQ but it has the same value for success. */

    return SCSI_STATUS_OK;
}

int vscsiReqSenseErrorSet(PVSCSIREQINT pVScsiReq, uint8_t uSCSISenseKey, uint8_t uSCSIASC)
{
    AssertMsgReturn(pVScsiReq->cbSense >= 18, ("Sense buffer is not big enough\n"), SCSI_STATUS_OK);
    AssertMsgReturn(pVScsiReq->pbSense, ("Sense buffer pointer is NULL\n"), SCSI_STATUS_OK);
    memset(pVScsiReq->pbSense, 0, pVScsiReq->cbSense);
    pVScsiReq->pbSense[0] = (1 << 7) | SCSI_SENSE_RESPONSE_CODE_CURR_FIXED; /* Fixed format */
    pVScsiReq->pbSense[2] = uSCSISenseKey;
    pVScsiReq->pbSense[7]  = 10;
    pVScsiReq->pbSense[12] = uSCSIASC;
    pVScsiReq->pbSense[13] = 0x00; /** @todo: Provide more info. */
    return SCSI_STATUS_CHECK_CONDITION;
}

