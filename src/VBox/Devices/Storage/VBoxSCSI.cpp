/* $Id$ */
/** @file
 *
 * VBox storage devices:
 * Simple SCSI interface for BIOS access
 */

/*
 * Copyright (C) 2006-2009 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
//#define DEBUG
#define LOG_GROUP LOG_GROUP_DEV_BUSLOGIC /* @todo: Create extra group. */

#if defined(IN_R0) || defined(IN_RC)
# error This device has no R0 or GC components
#endif

#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/pgm.h>
#include <iprt/asm.h>
#include <iprt/mem.h>
#include <iprt/thread.h>
#include <iprt/string.h>

#include "VBoxSCSI.h"

static void vboxscsiReset(PVBOXSCSI pVBoxSCSI)
{
    pVBoxSCSI->regIdentify = 0;
    pVBoxSCSI->cbCDB       = 0;
    memset(pVBoxSCSI->aCDB, 0, sizeof(pVBoxSCSI->aCDB));
    pVBoxSCSI->iCDB        = 0;
    pVBoxSCSI->fBusy       = false;
    pVBoxSCSI->cbBuf       = 0;
    pVBoxSCSI->iBuf        = 0;
    if (pVBoxSCSI->pBuf)
        RTMemFree(pVBoxSCSI->pBuf);

    pVBoxSCSI->pBuf        = NULL;
    pVBoxSCSI->enmState    = VBOXSCSISTATE_NO_COMMAND;

}

/**
 * Initializes the state for the SCSI interface.
 *
 * @returns VBox status code.
 * @param   pVBoxSCSI    Pointer to the unitialized SCSI state.
 */
int vboxscsiInitialize(PVBOXSCSI pVBoxSCSI)
{
    pVBoxSCSI->pBuf        = NULL;
    vboxscsiReset(pVBoxSCSI);

    return VINF_SUCCESS;
}

/**
 * Reads a register value.
 *
 * @returns VBox status code.
 * @param   pVBoxSCSI    Pointer to the SCSI state.
 * @param   iRegister    Index of the register to read.
 * @param   pu32Value    Where to store the content of the register.
 */
int vboxscsiReadRegister(PVBOXSCSI pVBoxSCSI, uint8_t iRegister, uint32_t *pu32Value)
{
    uint8_t uVal = 0;

    switch (iRegister)
    {
        case 0:
        {
            if (ASMAtomicReadBool(&pVBoxSCSI->fBusy) == true)
            {
                uVal |= VBOX_SCSI_BUSY;
                /* There is an I/O operation in progress.
                 * Yield the execution thread to let the I/O thread make progress.
                 */
                RTThreadYield();
            }
            else
                uVal &= ~VBOX_SCSI_BUSY;
            break;
        }
        case 1:
        {
            /* If we're not in the 'command ready' state, there may not even be a buffer yet. */
            if ((pVBoxSCSI->enmState == VBOXSCSISTATE_COMMAND_READY) && pVBoxSCSI->cbBuf > 0)
            {
                AssertMsg(pVBoxSCSI->pBuf, ("pBuf is NULL\n"));
                Assert(pVBoxSCSI->enmState == VBOXSCSISTATE_COMMAND_READY);
                Assert(!pVBoxSCSI->fBusy);
                uVal = pVBoxSCSI->pBuf[pVBoxSCSI->iBuf];
                pVBoxSCSI->iBuf++;
                pVBoxSCSI->cbBuf--;
                if (pVBoxSCSI->cbBuf == 0)
                {
                    /** The guest read the last byte from the data in buffer.
                     *  Clear everything and reset command buffer.
                     */
                    RTMemFree(pVBoxSCSI->pBuf);
                    pVBoxSCSI->pBuf  = NULL;
                    pVBoxSCSI->cbCDB = 0;
                    pVBoxSCSI->iCDB  = 0;
                    pVBoxSCSI->iBuf  = 0;
                    pVBoxSCSI->uTargetDevice = 0;
                    pVBoxSCSI->enmState = VBOXSCSISTATE_NO_COMMAND;
                    memset(pVBoxSCSI->aCDB, 0, sizeof(pVBoxSCSI->aCDB));
                }
            }
            break;
        }
        case 2:
        {
            uVal = pVBoxSCSI->regIdentify;
            break;
        }
        default:
            AssertMsgFailed(("Invalid register to read from %u\n", iRegister));
    }

    *pu32Value = uVal;

    return VINF_SUCCESS;
}

/**
 * Writes to a register.
 *
 * @returns VBox status code.
 *          VERR_MORE_DATA if a command is ready to be sent to the SCSI driver.
 * @param   pVBoxSCSI    Pointer to the SCSI state.
 * @param   iRegister    Index of the register to write to.
 * @param   uVal         Value to write.
 */
int vboxscsiWriteRegister(PVBOXSCSI pVBoxSCSI, uint8_t iRegister, uint8_t uVal)
{
    int rc = VINF_SUCCESS;

    switch (iRegister)
    {
        case 0:
        {
            if (pVBoxSCSI->enmState == VBOXSCSISTATE_NO_COMMAND)
            {
                pVBoxSCSI->enmState = VBOXSCSISTATE_READ_TXDIR;
                pVBoxSCSI->uTargetDevice = uVal;
            }
            else if (pVBoxSCSI->enmState == VBOXSCSISTATE_READ_TXDIR)
            {
                if (uVal != VBOXSCSI_TXDIR_FROM_DEVICE && uVal != VBOXSCSI_TXDIR_TO_DEVICE)
                    vboxscsiReset(pVBoxSCSI);
                else
                {
                    pVBoxSCSI->enmState = VBOXSCSISTATE_READ_CDB_SIZE_BUFHI;
                    pVBoxSCSI->uTxDir = uVal;
                }
            }
            else if (pVBoxSCSI->enmState == VBOXSCSISTATE_READ_CDB_SIZE_BUFHI)
            {
                uint8_t     cbCDB = uVal & 0x0F;

                if (cbCDB > VBOXSCSI_CDB_SIZE_MAX)
                    vboxscsiReset(pVBoxSCSI);
                else
                {
                    pVBoxSCSI->enmState = VBOXSCSISTATE_READ_BUFFER_SIZE_LSB;
                    pVBoxSCSI->cbCDB = cbCDB;
                    pVBoxSCSI->cbBuf = (uVal & 0xF0) << 12;     /* Bits 16-19 of buffer size. */
                }
            }
            else if (pVBoxSCSI->enmState == VBOXSCSISTATE_READ_BUFFER_SIZE_LSB)
            {
                pVBoxSCSI->enmState = VBOXSCSISTATE_READ_BUFFER_SIZE_MID;
                pVBoxSCSI->cbBuf |= uVal;                       /* Bits 0-7 of buffer size. */
            }
            else if (pVBoxSCSI->enmState == VBOXSCSISTATE_READ_BUFFER_SIZE_MID)
            {
                pVBoxSCSI->enmState = VBOXSCSISTATE_READ_COMMAND;
                pVBoxSCSI->cbBuf |= (((uint16_t)uVal) << 8);    /* Bits 8-15 of buffer size. */
            }
            else if (pVBoxSCSI->enmState == VBOXSCSISTATE_READ_COMMAND)
            {
                pVBoxSCSI->aCDB[pVBoxSCSI->iCDB] = uVal;
                pVBoxSCSI->iCDB++;

                /* Check if we have all necessary command data. */
                if (pVBoxSCSI->iCDB == pVBoxSCSI->cbCDB)
                {
                    Log(("%s: Command ready for processing\n", __FUNCTION__));
                    pVBoxSCSI->enmState = VBOXSCSISTATE_COMMAND_READY;
                    if (pVBoxSCSI->uTxDir == VBOXSCSI_TXDIR_TO_DEVICE)
                    {
                        /* This is a write allocate buffer. */
                        pVBoxSCSI->pBuf = (uint8_t *)RTMemAllocZ(pVBoxSCSI->cbBuf);
                        if (!pVBoxSCSI->pBuf)
                            return VERR_NO_MEMORY;
                    }
                    else
                    {
                        /* This is a read from the device. */
                        ASMAtomicXchgBool(&pVBoxSCSI->fBusy, true);
                        rc = VERR_MORE_DATA; /** @todo Better return value to indicate ready command? */
                    }
                }
            }
            else
                AssertMsgFailed(("Invalid state %d\n", pVBoxSCSI->enmState));
            break;
        }
        case 1:
        {
            if (   pVBoxSCSI->enmState != VBOXSCSISTATE_COMMAND_READY
                || pVBoxSCSI->uTxDir != VBOXSCSI_TXDIR_TO_DEVICE)
            {
                /* Reset the state */
                vboxscsiReset(pVBoxSCSI);
            }
            else
            {
                pVBoxSCSI->pBuf[pVBoxSCSI->iBuf++] = uVal;
                if (pVBoxSCSI->iBuf == pVBoxSCSI->cbBuf)
                {
                    rc = VERR_MORE_DATA;
                    ASMAtomicXchgBool(&pVBoxSCSI->fBusy, true);
                }
            }
            break;
        }
        case 2:
        {
            pVBoxSCSI->regIdentify = uVal;
            break;
        }
        case 3:
        {
            /* Reset */
            vboxscsiReset(pVBoxSCSI);
            break;
        }
        default:
            AssertMsgFailed(("Invalid register to write to %u\n", iRegister));
    }

    return rc;
}

/**
 * Sets up a SCSI request which the owning SCSI device can process.
 *
 * @returns VBox status code.
 * @param   pVBoxSCSI      Pointer to the SCSI state.
 * @param   pScsiRequest   Pointer to a scsi request to setup.
 * @param   puTargetDevice Where to store the target device ID.
 */
int vboxscsiSetupRequest(PVBOXSCSI pVBoxSCSI, PPDMSCSIREQUEST pScsiRequest, uint32_t *puTargetDevice)
{
    int rc = VINF_SUCCESS;

    LogFlowFunc(("pVBoxSCSI=%#p pScsiRequest=%#p puTargetDevice=%#p\n", pVBoxSCSI, pScsiRequest, puTargetDevice));

    AssertMsg(pVBoxSCSI->enmState == VBOXSCSISTATE_COMMAND_READY, ("Invalid state %u\n", pVBoxSCSI->enmState));

    if (pVBoxSCSI->uTxDir == VBOXSCSI_TXDIR_FROM_DEVICE)
    {
        if (pVBoxSCSI->pBuf)
            RTMemFree(pVBoxSCSI->pBuf);

        pVBoxSCSI->pBuf = (uint8_t *)RTMemAllocZ(pVBoxSCSI->cbBuf);
        if (!pVBoxSCSI->pBuf)
            return VERR_NO_MEMORY;
    }

    /* Allocate scatter gather element. */
    pScsiRequest->paScatterGatherHead = (PRTSGSEG)RTMemAllocZ(sizeof(RTSGSEG) * 1); /* Only one element. */
    if (!pScsiRequest->paScatterGatherHead)
    {
        RTMemFree(pVBoxSCSI->pBuf);
        pVBoxSCSI->pBuf = NULL;
        return VERR_NO_MEMORY;
    }

    /* Allocate sense buffer. */
    pScsiRequest->cbSenseBuffer = 18;
    pScsiRequest->pbSenseBuffer = (uint8_t *)RTMemAllocZ(pScsiRequest->cbSenseBuffer);

    pScsiRequest->cbCDB = pVBoxSCSI->cbCDB;
    pScsiRequest->pbCDB = pVBoxSCSI->aCDB;
    pScsiRequest->uLogicalUnit = 0;
    pScsiRequest->cbScatterGather = pVBoxSCSI->cbBuf;
    pScsiRequest->cScatterGatherEntries = 1;

    pScsiRequest->paScatterGatherHead[0].cbSeg = pVBoxSCSI->cbBuf;
    pScsiRequest->paScatterGatherHead[0].pvSeg = pVBoxSCSI->pBuf;

    *puTargetDevice = pVBoxSCSI->uTargetDevice;

    return rc;
}

/**
 * Notifies the device that a request finished and the incoming data
 * is ready at the incoming data port.
 */
int vboxscsiRequestFinished(PVBOXSCSI pVBoxSCSI, PPDMSCSIREQUEST pScsiRequest)
{
    LogFlowFunc(("pVBoxSCSI=%#p pScsiRequest=%#p\n", pVBoxSCSI, pScsiRequest));
    RTMemFree(pScsiRequest->paScatterGatherHead);
    RTMemFree(pScsiRequest->pbSenseBuffer);

    if (pVBoxSCSI->uTxDir == VBOXSCSI_TXDIR_TO_DEVICE)
    {
        if (pVBoxSCSI->pBuf)
            RTMemFree(pVBoxSCSI->pBuf);
        pVBoxSCSI->pBuf  = NULL;
        pVBoxSCSI->cbBuf = 0;
        pVBoxSCSI->cbCDB = 0;
        pVBoxSCSI->iCDB  = 0;
        pVBoxSCSI->iBuf  = 0;
        pVBoxSCSI->uTargetDevice = 0;
        pVBoxSCSI->enmState = VBOXSCSISTATE_NO_COMMAND;
        memset(pVBoxSCSI->aCDB, 0, sizeof(pVBoxSCSI->aCDB));
    }

    ASMAtomicXchgBool(&pVBoxSCSI->fBusy, false);

    return VINF_SUCCESS;
}

int vboxscsiReadString(PPDMDEVINS pDevIns, PVBOXSCSI pVBoxSCSI, uint8_t iRegister,
                       RTGCPTR *pGCPtrDst, PRTGCUINTREG pcTransfer, unsigned cb)
{
    RTGCPTR  GCDst      = *pGCPtrDst;
    uint32_t cbTransfer = *pcTransfer * cb;

    LogFlowFunc(("pDevIns=%#p pVBoxSCSI=%#p iRegister=%d cTransfer=%u cb=%u\n",
                 pDevIns, pVBoxSCSI, iRegister, *pcTransfer, cb));

    /* Read string only valid for data in register. */
    AssertMsg(iRegister == 1, ("Hey only register 1 can be read from with string\n"));

    /* Accesses without a valid buffer will be ignored. */
    if (!pVBoxSCSI->pBuf)
        return VINF_SUCCESS;

    /* Also ignore attempts to read more data than is available. */
    Assert(cbTransfer <= pVBoxSCSI->cbBuf);
    if (cbTransfer > pVBoxSCSI->cbBuf)
        cbTransfer = pVBoxSCSI->cbBuf;  /* Ignore excess data (not supposed to happen). */

    int rc = PGMPhysSimpleDirtyWriteGCPtr(PDMDevHlpGetVMCPU(pDevIns), GCDst, pVBoxSCSI->pBuf + pVBoxSCSI->iBuf, cbTransfer);
    AssertRC(rc);

    *pGCPtrDst = (RTGCPTR)((RTGCUINTPTR)GCDst + cbTransfer);
    *pcTransfer = 0;

    /* Advance current buffer position. */
    pVBoxSCSI->iBuf  += cbTransfer;
    pVBoxSCSI->cbBuf -= cbTransfer;

    if (pVBoxSCSI->cbBuf == 0)
    {
        /** The guest read the last byte from the data in buffer.
         *  Clear everything and reset command buffer.
         */
        RTMemFree(pVBoxSCSI->pBuf);
        pVBoxSCSI->pBuf  = NULL;
        pVBoxSCSI->cbCDB = 0;
        pVBoxSCSI->iCDB  = 0;
        pVBoxSCSI->iBuf  = 0;
        pVBoxSCSI->uTargetDevice = 0;
        pVBoxSCSI->enmState = VBOXSCSISTATE_NO_COMMAND;
        memset(pVBoxSCSI->aCDB, 0, sizeof(pVBoxSCSI->aCDB));
    }

    return rc;
}

int vboxscsiWriteString(PPDMDEVINS pDevIns, PVBOXSCSI pVBoxSCSI, uint8_t iRegister,
                        RTGCPTR *pGCPtrSrc, PRTGCUINTREG pcTransfer, unsigned cb)
{
    RTGCPTR  GCSrc      = *pGCPtrSrc;
    uint32_t cbTransfer = *pcTransfer * cb;

    /* Write string only valid for data in/out register. */
    AssertMsg(iRegister == 1, ("Hey only register 1 can be written to with string\n"));

    /* Accesses without a valid buffer will be ignored. */
    if (!pVBoxSCSI->pBuf)
        return VINF_SUCCESS;

    Assert(cbTransfer <= pVBoxSCSI->cbBuf);
    if (cbTransfer > pVBoxSCSI->cbBuf)
        cbTransfer = pVBoxSCSI->cbBuf;  /* Ignore excess data (not supposed to happen). */

    int rc = PDMDevHlpPhysReadGCVirt(pDevIns, pVBoxSCSI->pBuf + pVBoxSCSI->iBuf, GCSrc, cbTransfer);
    AssertRC(rc);

    /* Advance current buffer position. */
    pVBoxSCSI->iBuf  += cbTransfer;
    pVBoxSCSI->cbBuf -= cbTransfer;

    *pGCPtrSrc = (RTGCPTR)((RTGCUINTPTR)GCSrc + cbTransfer);
    *pcTransfer = 0;

    ASMAtomicXchgBool(&pVBoxSCSI->fBusy, true);
    return VERR_MORE_DATA;
}

void vboxscsiSetRequestRedo(PVBOXSCSI pVBoxSCSI, PPDMSCSIREQUEST pScsiRequest)
{
    AssertMsg(pVBoxSCSI->fBusy, ("No request to redo\n"));

    RTMemFree(pScsiRequest->paScatterGatherHead);
    RTMemFree(pScsiRequest->pbSenseBuffer);

    if (pVBoxSCSI->uTxDir == VBOXSCSI_TXDIR_FROM_DEVICE)
    {
        AssertPtr(pVBoxSCSI->pBuf);
    }
}
