/** @file
 *
 * VBoxGuestLib - A support library for VirtualBox guest additions:
 * Physical memory heap
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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
#define LOG_GROUP LOG_GROUP_HGCM
#include <VBox/log.h>

#include <VBox/VBoxGuestLib.h>
#include "SysHlp.h"

#include <iprt/assert.h>
#if !defined(__WIN__) && !defined(__LINUX__)
#include <iprt/memobj.h>
#endif 


int vbglLockLinear (void **ppvCtx, void *pv, uint32_t u32Size)
{
    int rc = VINF_SUCCESS;
    
#ifdef __WIN__
    PMDL pMdl = IoAllocateMdl (pv, u32Size, FALSE, FALSE, NULL);

    if (pMdl == NULL)
    {
        rc = VERR_NOT_SUPPORTED;
    }
    else
    {
        __try {
            /* Calls to MmProbeAndLockPages must be enclosed in a try/except block. */
            MmProbeAndLockPages (pMdl,
                                 KernelMode,
                                 IoModifyAccess);
                                 
            *ppvCtx = pMdl;

        } __except(EXCEPTION_EXECUTE_HANDLER) {

            IoFreeMdl (pMdl);
            rc = VERR_INVALID_PARAMETER;
        }
    }

#elif defined(__LINUX__)
    NOREF(ppvCtx);
    NOREF(pv);
    NOREF(u32Size);

#else
    /* Default to IPRT - this ASSUMES that it is USER addresses we're locking. */
    RTR0MEMOBJ MemObj;
    rc = RTR0MemObjLockUser(&MemObj, pv, u32Size, NIL_RTR0PROCESS);
    if (RT_SUCCESS(rc))
        *ppvCtx = MemObj;
    else
        *ppvCtx = NIL_RTR0MEMOBJ;

#endif
    
    return rc;
}

void vbglUnlockLinear (void *pvCtx, void *pv, uint32_t u32Size)
{
    NOREF(pv);
    NOREF(u32Size);

#ifdef __WIN__
    PMDL pMdl = (PMDL)pvCtx;

    if (pMdl != NULL)
    {
        MmUnlockPages (pMdl);
        IoFreeMdl (pMdl);
    }

#elif defined(__LINUX__)
    NOREF(pvCtx);

#else
    /* default to IPRT */
    RTR0MEMOBJ MemObj = (RTR0MEMOBJ)pvCtx;
    int rc = RTR0MemObjFree(MemObj, false);
    AssertRC(rc);

#endif
}

#ifndef VBGL_VBOXGUEST

#if defined (__LINUX__) && !defined (__KERNEL__)
# include <unistd.h>
# include <errno.h>
# include <sys/fcntl.h>
# include <sys/ioctl.h>
#endif

#ifdef __LINUX__
__BEGIN_DECLS
extern DECLVBGL(void *) vboxadd_cmc_open (void);
extern DECLVBGL(void) vboxadd_cmc_close (void *);
extern DECLVBGL(int) vboxadd_cmc_call (void *opaque, uint32_t func, void *data);
__END_DECLS
#endif /* __LINUX__ */

#ifdef __OS2__
__BEGIN_DECLS
/* 
 * On OS/2 we'll do the connecting in the assembly code of the 
 * client driver, exporting a g_VBoxGuestIDC symbol containing
 * the connection information obtained from the 16-bit IDC.
 */
extern VBOXGUESTOS2IDCCONNECT g_VBoxGuestIDC;
__END_DECLS
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

#elif defined (__LINUX__)
    void *opaque;

    opaque = (void *) vboxadd_cmc_open ();
    if (!opaque)
    {
        return VERR_NOT_IMPLEMENTED;
    }
    pDriver->opaque = opaque;
    return VINF_SUCCESS;

#elif defined (__OS2__)
    /* 
     * Just check whether the connection was made or not.
     */
    if (    g_VBoxGuestIDC.u32Version == VMMDEV_VERSION
        &&  VALID_PTR(g_VBoxGuestIDC.u32Session)
        &&  VALID_PTR(g_VBoxGuestIDC.pfnServiceEP))
    {
        pDriver->u32Session = g_VBoxGuestIDC.u32Session;
        return VINF_SUCCESS;
    }
    pDriver->u32Session = UINT32_MAX;
    Log(("vbglDriverOpen: failed\n"));
    return VERR_FILE_NOT_FOUND;

#else
# error "Port me"
#endif
}

int vbglDriverIOCtl (VBGLDRIVER *pDriver, uint32_t u32Function, void *pvData, uint32_t cbData)
{
#ifdef __WIN__
    IO_STATUS_BLOCK ioStatusBlock;

    KEVENT Event;
    KeInitializeEvent (&Event, NotificationEvent, FALSE);

    PIRP irp = IoBuildDeviceIoControlRequest (u32Function,
                                              pDriver->pDeviceObject,
                                              pvData,
                                              cbData,
                                              pvData,
                                              cbData,
                                              FALSE, /* external */
                                              &Event,
                                              &ioStatusBlock);
    if (irp == NULL)
    {
        Log(("vbglDriverIOCtl: IoBuildDeviceIoControlRequest failed\n"));
        return VERR_NO_MEMORY;
    }

    NTSTATUS rc = IoCallDriver (pDriver->pDeviceObject, irp);

    if (rc == STATUS_PENDING)
    {
        Log(("vbglDriverIOCtl: STATUS_PENDING\n"));
        rc = KeWaitForSingleObject(&Event,
                                   Executive,
                                   KernelMode,
                                   FALSE,
                                   NULL);

        rc = ioStatusBlock.Status;                     
    }    

    if (!NT_SUCCESS(rc))
        Log(("vbglDriverIOCtl: IoCallDriver failed with ntstatus=%x\n", rc));

    return NT_SUCCESS(rc)? VINF_SUCCESS: VERR_VBGL_IOCTL_FAILED;

#elif defined (__LINUX__)
    return vboxadd_cmc_call (pDriver->opaque, u32Function, pvData);

#elif defined (__OS2__)
    if (    pDriver->u32Session 
        &&  pDriver->u32Session == g_VBoxGuestIDC.u32Session)
        return g_VBoxGuestIDC.pfnServiceEP(pDriver->u32Session, u32Function, pvData, cbData, NULL);

    Log(("vbglDriverIOCtl: No connection\n"));
    return VERR_WRONG_ORDER;

#else
# error "Port me"
#endif
}

void vbglDriverClose (VBGLDRIVER *pDriver)
{
#ifdef __WIN__
    Log(("vbglDriverClose pDeviceObject=%x\n", pDriver->pDeviceObject));
    ObDereferenceObject (pDriver->pFileObject);

#elif defined (__LINUX__)
    vboxadd_cmc_close (pDriver->opaque);

#elif defined (__OS2__)
    pDriver->u32Session = 0;

#else
# error "Port me"
#endif
}

#endif /* !VBGL_VBOXGUEST */
