/* $Id$ */
/** @file
 * Virtual SCSI driver: I/O request handling.
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
#include <VBox/err.h>
#include <VBox/types.h>
#include <VBox/vscsi.h>
#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/asm.h>

#include "VSCSIInternal.h"

int vscsiIoReqFlushEnqueue(PVSCSILUNINT pVScsiLun, PVSCSIREQINT pVScsiReq)
{
    int rc = VINF_SUCCESS;
    PVSCSIIOREQINT pVScsiIoReq = NULL;

    pVScsiIoReq = (PVSCSIIOREQINT)RTMemAllocZ(sizeof(VSCSIIOREQINT));
    if (!pVScsiIoReq)
        return VERR_NO_MEMORY;

    pVScsiIoReq->pVScsiReq = pVScsiReq;
    pVScsiIoReq->pVScsiLun = pVScsiLun;
    pVScsiIoReq->enmTxDir  = VSCSIIOREQTXDIR_FLUSH;

    rc = vscsiLunReqTransferEnqueue(pVScsiLun, pVScsiIoReq);
    if (RT_SUCCESS(rc))
        ASMAtomicIncU32(&pVScsiLun->IoReq.cReqOutstanding);
    else
        RTMemFree(pVScsiIoReq);

    return rc;
}


int vscsiIoReqTransferEnqueue(PVSCSILUNINT pVScsiLun, PVSCSIREQINT pVScsiReq,
                              VSCSIIOREQTXDIR enmTxDir, uint64_t uOffset,
                              size_t cbTransfer)
{
    int rc = VINF_SUCCESS;
    PVSCSIIOREQINT pVScsiIoReq = NULL;

    LogFlowFunc(("pVScsiLun=%#p pVScsiReq=%#p enmTxDir=%u uOffset=%llu cbTransfer=%u\n",
                 pVScsiLun, pVScsiReq, enmTxDir, uOffset, cbTransfer));

    pVScsiIoReq = (PVSCSIIOREQINT)RTMemAllocZ(sizeof(VSCSIIOREQINT));
    if (!pVScsiIoReq)
        return VERR_NO_MEMORY;

    pVScsiIoReq->pVScsiReq  = pVScsiReq;
    pVScsiIoReq->pVScsiLun  = pVScsiLun;
    pVScsiIoReq->enmTxDir   = enmTxDir;
    pVScsiIoReq->uOffset    = uOffset;
    pVScsiIoReq->cbTransfer = cbTransfer;
    pVScsiIoReq->paSeg      = pVScsiReq->IoMemCtx.paDataSeg;
    pVScsiIoReq->cSeg       = pVScsiReq->IoMemCtx.cSegments;

    rc = vscsiLunReqTransferEnqueue(pVScsiLun, pVScsiIoReq);
    if (RT_SUCCESS(rc))
        ASMAtomicIncU32(&pVScsiLun->IoReq.cReqOutstanding);
    else
        RTMemFree(pVScsiIoReq);

    return rc;
}


uint32_t vscsiIoReqOutstandingCountGet(PVSCSILUNINT pVScsiLun)
{
    return ASMAtomicReadU32(&pVScsiLun->IoReq.cReqOutstanding);
}


VBOXDDU_DECL(int) VSCSIIoReqCompleted(VSCSIIOREQ hVScsiIoReq, int rcIoReq)
{
    PVSCSIIOREQINT pVScsiIoReq = hVScsiIoReq;
    PVSCSILUNINT pVScsiLun;
    PVSCSIREQINT pVScsiReq;
    int rcReq = SCSI_STATUS_OK;

    AssertPtrReturn(pVScsiIoReq, VERR_INVALID_HANDLE);

    pVScsiLun = pVScsiIoReq->pVScsiLun;
    pVScsiReq = pVScsiIoReq->pVScsiReq;

    AssertMsg(pVScsiLun->IoReq.cReqOutstanding > 0,
              ("Unregistered I/O request completed\n"));

    ASMAtomicDecU32(&pVScsiLun->IoReq.cReqOutstanding);

    /** @todo error reporting */
    if (RT_SUCCESS(rcIoReq))
        rcReq = vscsiReqSenseOkSet(pVScsiReq);

    /* Free the I/O request */
    RTMemFree(pVScsiIoReq);

    /* Notify completion of the SCSI request. */
    vscsiDeviceReqComplete(pVScsiLun->pVScsiDevice, pVScsiReq, rcReq);

    return VINF_SUCCESS;
}


VBOXDDU_DECL(VSCSIIOREQTXDIR) VSCSIIoReqTxDirGet(VSCSIIOREQ hVScsiIoReq)
{
    PVSCSIIOREQINT pVScsiIoReq = hVScsiIoReq;

    AssertPtrReturn(pVScsiIoReq, VSCSIIOREQTXDIR_INVALID);

    return pVScsiIoReq->enmTxDir;
}


VBOXDDU_DECL(int) VSCSIIoReqParamsGet(VSCSIIOREQ hVScsiIoReq, uint64_t *puOffset,
                                      size_t *pcbTransfer, unsigned *pcSeg,
                                      size_t *pcbSeg, PCPDMDATASEG *ppaSeg)
{
    PVSCSIIOREQINT pVScsiIoReq = hVScsiIoReq;

    AssertPtrReturn(pVScsiIoReq, VERR_INVALID_HANDLE);
    AssertReturn(pVScsiIoReq->enmTxDir != VSCSIIOREQTXDIR_FLUSH, VERR_NOT_SUPPORTED);

    *puOffset    = pVScsiIoReq->uOffset;
    *pcbTransfer = pVScsiIoReq->cbTransfer;
    *pcSeg       = pVScsiIoReq->cSeg;
    *pcbSeg      = pVScsiIoReq->cbSeg;
    *ppaSeg      = pVScsiIoReq->paSeg;

    return VINF_SUCCESS;
}

