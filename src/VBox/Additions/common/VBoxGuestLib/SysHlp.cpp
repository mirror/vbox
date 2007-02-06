/** @file
 *
 * VBoxGuestLib - A support library for VirtualBox guest additions:
 * Physical memory heap
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#include <VBox/VBoxGuestLib.h>
#include "SysHlp.h"

#define LOG_GROUP LOG_GROUP_HGCM
#include <VBox/log.h>
#include <iprt/assert.h>

#ifndef VBGL_VBOXGUEST

#if defined (__LINUX__) && !defined (__KERNEL__)
# include <unistd.h>
# include <errno.h>
# include <sys/fcntl.h>
# include <sys/ioctl.h>
#endif

#ifndef __WIN__
# ifdef __cplusplus
extern "C" {
# endif
extern DECLVBGL(void *) vboxadd_cmc_open (void);
extern DECLVBGL(void) vboxadd_cmc_close (void *);
extern DECLVBGL(int) vboxadd_cmc_call (void *opaque, uint32_t func, void *data);
# ifdef __cplusplus
}
# endif
#endif

int vbglDriverOpen (VBGLDRIVER *pDriver)
{
#ifdef __WIN__
    UNICODE_STRING uszDeviceName;
    RtlInitUnicodeString (&uszDeviceName, L"\\Device\\VBoxGuest");

    PDEVICE_OBJECT pDeviceObject = NULL;
    PFILE_OBJECT pFileObject = NULL;

    NTSTATUS rc = IoGetDeviceObjectPointer (&uszDeviceName, FILE_ALL_ACCESS,
                                            &pFileObject, &pDeviceObject);

    if (NT_SUCCESS (rc))
    {
        Log(("vbglDriverOpen VBoxGuest successful pDeviceObject=%x\n", pDeviceObject));
        pDriver->pDeviceObject = pDeviceObject;
        pDriver->pFileObject = pFileObject;
        return VINF_SUCCESS;
    }
    /** @todo return RTErrConvertFromNtStatus(rc)! */
    Log(("vbglDriverOpen VBoxGuest failed with ntstatus=%x\n", rc));
    return rc;
#else
    void *opaque;

    opaque = (void *) vboxadd_cmc_open ();
    if (!opaque)
    {
        return VERR_NOT_IMPLEMENTED;
    }
    pDriver->opaque = opaque;
    return VINF_SUCCESS;
#endif
}

int vbglDriverIOCtl (VBGLDRIVER *pDriver, uint32_t u32Function, void *pvData, uint32_t cbData)
{
#ifdef __WIN__
    IO_STATUS_BLOCK ioStatusBlock;

    KEVENT event;
    KeInitializeEvent (&event, NotificationEvent, FALSE);

    PIRP irp = IoBuildDeviceIoControlRequest (u32Function,
                                              pDriver->pDeviceObject,
                                              pvData,
                                              cbData,
                                              pvData,
                                              cbData,
                                              FALSE,
                                              &event,
                                              &ioStatusBlock);
    if (irp == NULL)
    {
        Log(("vbglDriverIOCtl: IoBuildDeviceIoControlRequest failed\n"));
        return VERR_NO_MEMORY;
    }

    NTSTATUS rc = IoCallDriver (pDriver->pDeviceObject, irp);

    if (!NT_SUCCESS(rc))
        Log(("vbglDriverIOCtl: IoCallDriver failed with ntstatus=%x\n", rc));

    return NT_SUCCESS(rc)? VINF_SUCCESS: VERR_VBGL_IOCTL_FAILED;
#else
    return vboxadd_cmc_call (pDriver->opaque, u32Function, pvData);
#endif
}

void vbglDriverClose (VBGLDRIVER *pDriver)
{
#ifdef __WIN__
    Log(("vbglDriverClose pDeviceObject=%x\n", pDriver->pDeviceObject));
    ObDereferenceObject (pDriver->pFileObject);
#else
    vboxadd_cmc_close (pDriver->opaque);
#endif
}

#endif /* !VBGL_VBOXGUEST */
