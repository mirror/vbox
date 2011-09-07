/* $Id$ */
/** @file
 * Virtual SCSI driver: Internal defines
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#ifndef ___VSCSIInternal_h
#define ___VSCSIInternal_h

#include <VBox/vscsi.h>
#include <VBox/scsi.h>
#include <iprt/memcache.h>
#include <iprt/sg.h>

#include "VSCSIInline.h"

/** Pointer to an internal virtual SCSI device. */
typedef VSCSIDEVICEINT        *PVSCSIDEVICEINT;
/** Pointer to an internal virtual SCSI device LUN. */
typedef VSCSILUNINT           *PVSCSILUNINT;
/** Pointer to an internal virtual SCSI device LUN pointer. */
typedef PVSCSILUNINT          *PPVSCSILUNINT;
/** Pointer to a virtual SCSI LUN descriptor. */
typedef struct VSCSILUNDESC   *PVSCSILUNDESC;
/** Pointer to a virtual SCSI request. */
typedef VSCSIREQINT           *PVSCSIREQINT;
/** Pointer to a virtual SCSI I/O request. */
typedef VSCSIIOREQINT         *PVSCSIIOREQINT;
/** Pointer to virtual SCSI sense data state. */
typedef struct VSCSISENSE     *PVSCSISENSE;

/**
 * Virtual SCSI sense data handling.
 */
typedef struct VSCSISENSE
{
    /** Buffer holding the sense data. */
    uint8_t              abSenseBuf[32];
} VSCSISENSE;

/**
 * Virtual SCSI device.
 */
typedef struct VSCSIDEVICEINT
{
    /** Request completion callback */
    PFNVSCSIREQCOMPLETED pfnVScsiReqCompleted;
    /** Opaque user data. */
    void                *pvVScsiDeviceUser;
    /** Number of LUNs currently attached. */
    uint32_t             cLunsAttached;
    /** How many LUNs are fitting in the array. */
    uint32_t             cLunsMax;
    /** Request cache */
    RTMEMCACHE           hCacheReq;
    /** Sense data handling. */
    VSCSISENSE           VScsiSense;
    /** Pointer to the array of LUN handles.
     *  The index is the LUN id. */
    PPVSCSILUNINT        papVScsiLun;
} VSCSIDEVICEINT;

/**
 * Virtual SCSI device LUN.
 */
typedef struct VSCSILUNINT
{
    /** Pointer to the parent SCSI device. */
    PVSCSIDEVICEINT      pVScsiDevice;
    /** Opaque user data */
    void                *pvVScsiLunUser;
    /** I/O callback table */
    PVSCSILUNIOCALLBACKS pVScsiLunIoCallbacks;
    /** Pointer to the LUN type descriptor. */
    PVSCSILUNDESC        pVScsiLunDesc;
    /** I/O request processing data */
    struct
    {
        /** Number of outstanding tasks on this LUN. */
        volatile uint32_t cReqOutstanding;
    } IoReq;
} VSCSILUNINT;

/**
 * Virtual SCSI request.
 */
typedef struct VSCSIREQINT
{
    /** The LUN the request is for. */
    uint32_t             iLun;
    /** The CDB */
    uint8_t             *pbCDB;
    /** Size of the CDB */
    size_t               cbCDB;
    /** S/G buffer. */
    RTSGBUF              SgBuf;
    /** Pointer to the sense buffer. */
    uint8_t             *pbSense;
    /** Size of the sense buffer */
    size_t               cbSense;
    /** Opaque user data associated with this request */
    void                *pvVScsiReqUser;
} VSCSIREQINT;

/**
 * Virtual SCSI I/O request.
 */
typedef struct VSCSIIOREQINT
{
    /** The associated request. */
    PVSCSIREQINT        pVScsiReq;
    /** Lun for this I/O request. */
    PVSCSILUNINT        pVScsiLun;
    /** Transfer direction */
    VSCSIIOREQTXDIR     enmTxDir;
    /** Start offset */
    uint64_t            uOffset;
    /** Number of bytes to transfer */
    size_t              cbTransfer;
    /** Number of bytes the S/G list holds */
    size_t              cbSeg;
    /** Number of segments. */
    unsigned            cSeg;
    /** Segment array. */
    PCRTSGSEG           paSeg;
} VSCSIIOREQINT;

/**
 * Virtual SCSI LUN descriptor.
 */
typedef struct VSCSILUNDESC
{
    /** Device type this descriptor emulates. */
    VSCSILUNTYPE         enmLunType;
    /** Descriptor name */
    const char          *pcszDescName;
    /** LUN type size */
    size_t               cbLun;

    /**
     * Initialise a Lun instance.
     *
     * @returns VBox status code.
     * @param   pVScsiLun    The SCSI LUN instance.
     */
    DECLR3CALLBACKMEMBER(int, pfnVScsiLunInit, (PVSCSILUNINT pVScsiLun));

    /**
     * Destroy a Lun instance.
     *
     * @returns VBox status code.
     * @param   pVScsiLun    The SCSI LUN instance.
     */
    DECLR3CALLBACKMEMBER(int, pfnVScsiLunDestroy, (PVSCSILUNINT pVScsiLun));

    /**
     * Processes a SCSI request.
     *
     * @returns VBox status code.
     * @param   pVScsiLun    The SCSI LUN instance.
     * @param   pVScsiReq    The SCSi request to process.
     */
    DECLR3CALLBACKMEMBER(int, pfnVScsiLunReqProcess, (PVSCSILUNINT pVScsiLun, PVSCSIREQINT pVScsiReq));

} VSCSILUNDESC;

/** Maximum number of LUNs a device can have. */
#define VSCSI_DEVICE_LUN_MAX 128

/**
 * Completes a SCSI request and calls the completion handler.
 *
 * @returns nothing.
 * @param   pVScsiDevice    The virtual SCSI device.
 * @param   pVScsiReq       The request which completed.
 * @param   rcScsiCode      The status code
 *                          One of the SCSI_STATUS_* #defines.
 * @param   fRedoPossible   Flag whether redo is possible.
 * @param   rcReq           Informational return code of the request.
 */
void vscsiDeviceReqComplete(PVSCSIDEVICEINT pVScsiDevice, PVSCSIREQINT pVScsiReq,
                            int rcScsiCode, bool fRedoPossible, int rcReq);

/**
 * Init the sense data state.
 *
 * @returns nothing.
 * @param   pVScsiSense  The SCSI sense data state to init.
 */
void vscsiSenseInit(PVSCSISENSE pVScsiSense);

/**
 * Sets a ok sense code.
 *
 * @returns SCSI status code.
 * @param   pVScsiSense  The SCSI sense state to use.
 * @param   pVScsiReq    The SCSI request.
 */
int vscsiReqSenseOkSet(PVSCSISENSE pVScsiSense, PVSCSIREQINT pVScsiReq);

/**
 * Sets a error sense code.
 *
 * @returns SCSI status code.
 * @param   pVScsiSense   The SCSI sense state to use.
 * @param   pVScsiReq     The SCSI request.
 * @param   uSCSISenseKey The SCSI sense key to set.
 * @param   uSCSIASC      The ASC value.
 */
int vscsiReqSenseErrorSet(PVSCSISENSE pVScsiSense, PVSCSIREQINT pVScsiReq, uint8_t uSCSISenseKey, uint8_t uSCSIASC);

/**
 * Process a request sense command.
 *
 * @returns SCSI status code.
 * @param   pVScsiSense   The SCSI sense state to use.
 * @param   pVScsiReq     The SCSI request.
 */
int vscsiReqSenseCmd(PVSCSISENSE pVScsiSense, PVSCSIREQINT pVScsiReq);

/**
 * Enqueues a new flush request
 *
 * @returns VBox status code.
 * @param   pVScsiLun    The LUN instance which issued the request.
 * @param   pVScsiReq    The virtual SCSI request associated with the flush.
 */
int vscsiIoReqFlushEnqueue(PVSCSILUNINT pVScsiLun, PVSCSIREQINT pVScsiReq);

/**
 * Enqueue a new data transfer request.
 *
 * @returns VBox status code.
 * @param   pVScsiLun   The LUN instance which issued the request.
 * @param   pVScsiReq   The virtual SCSI request associated with the transfer.
 * @param   enmTxDir    Transfer direction.
 * @param   uOffset     Start offset of the transfer.
 * @param   cbTransfer  Number of bytes to transfer.
 */
int vscsiIoReqTransferEnqueue(PVSCSILUNINT pVScsiLun, PVSCSIREQINT pVScsiReq,
                              VSCSIIOREQTXDIR enmTxDir, uint64_t uOffset,
                              size_t cbTransfer);

/**
 * Returns the current number of outstanding tasks on the given LUN.
 *
 * @returns Number of outstanding tasks.
 * @param   pVScsiLun   The LUN to check.
 */
uint32_t vscsiIoReqOutstandingCountGet(PVSCSILUNINT pVScsiLun);

/**
 * Wrapper for the get medium size I/O callback.
 *
 * @returns VBox status code.
 * @param   pVScsiLun   The LUN.
 * @param   pcbSize     Where to store the size on success.
 */
DECLINLINE(int) vscsiLunMediumGetSize(PVSCSILUNINT pVScsiLun, uint64_t *pcbSize)
{
    return pVScsiLun->pVScsiLunIoCallbacks->pfnVScsiLunMediumGetSize(pVScsiLun,
                                                                     pVScsiLun->pvVScsiLunUser,
                                                                     pcbSize);
}

/**
 * Wrapper for the I/O request enqueue I/O callback.
 *
 * @returns VBox status code.
 * @param   pVScsiLun   The LUN.
 * @param   pVScsiIoReq The I/O request to enqueue.
 */
DECLINLINE(int) vscsiLunReqTransferEnqueue(PVSCSILUNINT pVScsiLun, PVSCSIIOREQINT pVScsiIoReq)
{
    return pVScsiLun->pVScsiLunIoCallbacks->pfnVScsiLunReqTransferEnqueue(pVScsiLun,
                                                                          pVScsiLun->pvVScsiLunUser,
                                                                          pVScsiIoReq);
}

/**
 * Wrapper around vscsiReqSenseOkSet()
 */
DECLINLINE(int) vscsiLunReqSenseOkSet(PVSCSILUNINT pVScsiLun, PVSCSIREQINT pVScsiReq)
{
    return vscsiReqSenseOkSet(&pVScsiLun->pVScsiDevice->VScsiSense, pVScsiReq);
}

/**
 * Wrapper around vscsiReqSenseErrorSet()
 */
DECLINLINE(int) vscsiLunReqSenseErrorSet(PVSCSILUNINT pVScsiLun, PVSCSIREQINT pVScsiReq, uint8_t uSCSISenseKey, uint8_t uSCSIASC)
{
    return vscsiReqSenseErrorSet(&pVScsiLun->pVScsiDevice->VScsiSense, pVScsiReq, uSCSISenseKey, uSCSIASC);
}


#endif /* ___VSCSIInternal_h */

