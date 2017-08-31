/* $Id$ */
/** @file
 * VBoxGuestLib - Ring-0 Support Library for VBoxGuest, IDC, Windows specific.
 */

/*
 * Copyright (C) 2008-2017 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "VBoxGuestR0LibInternal.h"
#include <VBox/VBoxGuest.h>
#include <VBox/err.h>
#include <VBox/log.h>


/**
 * Implementation of a IO_COMPLETION_ROUTINE.
 *
 * @returns STATUS_MORE_PROCESSING_REQUIRED
 * @param   pDeviceObject   The device object.
 * @param   pIrp            The request.
 * @param   pvUser          The event object.
 */
static NTSTATUS vbglR0NtDriverIoCtlCompletion(IN PDEVICE_OBJECT pDeviceObject, IN PIRP pIrp, IN PVOID pvUser)
{
    PKEVENT pEvent = (PKEVENT )pvUser;
    Log(("vbglR0NtDriverIoCtlCompletion: pIrp=%p pEvent=%p\n", pIrp, pEvent));
    RT_NOREF2(pDeviceObject, pIrp);

    KeSetEvent(pEvent, IO_NO_INCREMENT, FALSE /*fWait*/);
    return STATUS_MORE_PROCESSING_REQUIRED;
}


/**
 * Internal I/O Control call worker.
 *
 * @returns VBox status code.
 * @param   pDeviceObject   The device object to call.
 * @param   pFileObject     The file object for the connection.
 * @param   uReq            The request.
 * @param   pReq            The request packet.
 */
static int vbglR0IdcNtCallInternal(PDEVICE_OBJECT pDeviceObject, PFILE_OBJECT pFileObject, uint32_t uReq, PVBGLREQHDR pReq)
{
    int                 rc;
    IO_STATUS_BLOCK     IoStatusBlock;
    KEVENT              Event;
    PIRP                pIrp;
    NTSTATUS            rcNt;

    /*
     * Build the request.
     */
    KeInitializeEvent(&Event, NotificationEvent, FALSE);
    pIrp = IoBuildDeviceIoControlRequest(uReq,              /* IoControlCode */
                                         pDeviceObject,
                                         pReq,              /* InputBuffer */
                                         pReq->cbIn,        /* InputBufferLength */
                                         pReq,              /* OutputBuffer */
                                         pReq->cbOut,       /* OutputBufferLength */
                                         TRUE,              /* InternalDeviceIoControl (=> IRP_MJ_INTERNAL_DEVICE_CONTROL) */
                                         &Event,            /* Event */
                                         &IoStatusBlock);   /* IoStatusBlock */
    if (pIrp)
    {
        IoGetNextIrpStackLocation(pIrp)->FileObject = pFileObject;

        /* A completion routine is required to signal the Event. */
        IoSetCompletionRoutine(pIrp, vbglR0NtDriverIoCtlCompletion, &Event,
                               TRUE /* InvokeOnSuccess */, TRUE /* InvokeOnError */, TRUE /* InvokeOnCancel */);

        /*
         * Call the driver, wait for an async request to complete (should never happen).
         */
        rcNt = IoCallDriver(pDeviceObject, pIrp);
        if (rcNt == STATUS_PENDING)
        {
            rcNt = KeWaitForSingleObject(&Event,            /* Object */
                                         Executive,         /* WaitReason */
                                         KernelMode,        /* WaitMode */
                                         FALSE,             /* Alertable */
                                         NULL);             /* TimeOut */
            rcNt = IoStatusBlock.Status;
        }
        if (NT_SUCCESS(rcNt))
            rc = pReq->rc;
        else
            rc = RTErrConvertFromNtStatus(rcNt);
    }
    else
        rc = VERR_NO_MEMORY;
    return rc;
}


int VBOXCALL vbglR0IdcNativeOpen(PVBGLIDCHANDLE pHandle, PVBGLIOCIDCCONNECT pReq)
{
    PDEVICE_OBJECT  pDeviceObject = NULL;
    PFILE_OBJECT    pFileObject = NULL;
    UNICODE_STRING  wszDeviceName;
    NTSTATUS        rcNt;
    int             rc;

    /*
     * Get the device object pointer.
     */
    RtlInitUnicodeString(&wszDeviceName, VBOXGUEST_DEVICE_NAME_NT);
    rcNt = IoGetDeviceObjectPointer(&wszDeviceName, FILE_ALL_ACCESS, &pFileObject, &pDeviceObject);
    if (NT_SUCCESS(rcNt))
    {
        /*
         * Make the connection call.
         */
        rc = vbglR0IdcNtCallInternal(pDeviceObject, pFileObject, VBGL_IOCTL_IDC_CONNECT, &pReq->Hdr);
        if (RT_SUCCESS(rc))
        {
            pHandle->s.pDeviceObject = pDeviceObject;
            pHandle->s.pFileObject   = pFileObject;
            return rc;
        }

        /* only the file object. */
        ObDereferenceObject(pFileObject);
    }
    else
        rc = RTErrConvertFromNtStatus(rcNt);

    pHandle->s.pDeviceObject = NULL;
    pHandle->s.pFileObject = NULL;
    return rc;
}


int VBOXCALL vbglR0IdcNativeClose(PVBGLIDCHANDLE pHandle, PVBGLIOCIDCDISCONNECT pReq)
{
    PFILE_OBJECT pFileObject = pHandle->s.pFileObject;
    int rc = vbglR0IdcNtCallInternal(pHandle->s.pDeviceObject, pFileObject, VBGL_IOCTL_IDC_DISCONNECT, &pReq->Hdr);
    if (RT_SUCCESS(rc))
    {
        pHandle->s.pDeviceObject = NULL;
        pHandle->s.pFileObject   = NULL;
        ObDereferenceObject(pFileObject);
    }

    return rc;
}


/**
 * Makes an IDC call, returning only the I/O control status code.
 *
 * @returns VBox status code (the I/O control failure status).
 * @param   pHandle             The IDC handle.
 * @param   uReq                The request number.
 * @param   pReqHdr             The request header.
 * @param   cbReq               The request size.
 */
DECLR0VBGL(int) VbglR0IdcCallRaw(PVBGLIDCHANDLE pHandle, size_t uReq, PVBGLREQHDR pReqHdr, uint32_t cbReq)
{
    NOREF(cbReq);
    return vbglR0IdcNtCallInternal(pHandle->s.pDeviceObject, pHandle->s.pFileObject, (uint32_t)uReq, pReqHdr);
}

