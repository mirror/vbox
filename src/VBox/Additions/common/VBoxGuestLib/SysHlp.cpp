/** @file
 *
 * VBoxGuestLib - A support library for VirtualBox guest additions:
 * Physical memory heap
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
#define LOG_GROUP LOG_GROUP_HGCM
#include <VBox/log.h>

#include <VBox/VBoxGuestLib.h>
#include "SysHlp.h"

#include <iprt/assert.h>
#if !defined(RT_OS_WINDOWS) && !defined(RT_OS_LINUX)
#include <iprt/memobj.h>
#endif


int vbglLockLinear (void **ppvCtx, void *pv, uint32_t u32Size, bool fWriteAccess)
{
    int rc = VINF_SUCCESS;

#ifdef RT_OS_WINDOWS
    PMDL pMdl = IoAllocateMdl (pv, u32Size, FALSE, FALSE, NULL);

    if (pMdl == NULL)
    {
        rc = VERR_NOT_SUPPORTED;
        AssertMsgFailed(("IoAllocateMdl %VGv %x failed!!\n", pv, u32Size));
    }
    else
    {
        __try {
            /* Calls to MmProbeAndLockPages must be enclosed in a try/except block. */
            MmProbeAndLockPages (pMdl,
                                 KernelMode,
                                 (fWriteAccess) ? IoModifyAccess : IoReadAccess);

            *ppvCtx = pMdl;

        } __except(EXCEPTION_EXECUTE_HANDLER) {

            IoFreeMdl (pMdl);
            rc = VERR_INVALID_PARAMETER;
            AssertMsgFailed(("MmProbeAndLockPages %VGv %x failed!!\n", pv, u32Size));
        }
    }

#elif defined(RT_OS_LINUX) || defined(RT_OS_FREEBSD) /** @todo r=bird: I don't think FreeBSD shouldn't go here, solaris and OS/2 doesn't
                                                      * (ignore linux as it's not using the same ioctl code).
                                                      * That said, the assumption below might be wrong for in kernel calls... */
    NOREF(ppvCtx);
    NOREF(pv);
    NOREF(u32Size);

#else
    /* Default to IPRT - this ASSUMES that it is USER addresses we're locking. */
    RTR0MEMOBJ MemObj;
    rc = RTR0MemObjLockUser(&MemObj, (RTR3PTR)pv, u32Size, NIL_RTR0PROCESS);
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

#ifdef RT_OS_WINDOWS
    PMDL pMdl = (PMDL)pvCtx;

    Assert(pMdl);
    if (pMdl != NULL)
    {
        MmUnlockPages (pMdl);
        IoFreeMdl (pMdl);
    }

#elif defined(RT_OS_LINUX) || defined(RT_OS_FREEBSD)
    NOREF(pvCtx);

#else
    /* default to IPRT */
    RTR0MEMOBJ MemObj = (RTR0MEMOBJ)pvCtx;
    int rc = RTR0MemObjFree(MemObj, false);
    AssertRC(rc);

#endif
}

#ifndef VBGL_VBOXGUEST

#if defined (RT_OS_LINUX) && !defined (__KERNEL__)
# include <unistd.h>
# include <errno.h>
# include <sys/fcntl.h>
# include <sys/ioctl.h>
#endif

#ifdef RT_OS_LINUX
__BEGIN_DECLS
extern DECLVBGL(void *) vboxadd_cmc_open (void);
extern DECLVBGL(void) vboxadd_cmc_close (void *);
extern DECLVBGL(int) vboxadd_cmc_call (void *opaque, uint32_t func, void *data);
__END_DECLS
#endif /* RT_OS_LINUX */

#ifdef RT_OS_OS2
__BEGIN_DECLS
/*
 * On OS/2 we'll do the connecting in the assembly code of the
 * client driver, exporting a g_VBoxGuestIDC symbol containing
 * the connection information obtained from the 16-bit IDC.
 */
extern VBOXGUESTOS2IDCCONNECT g_VBoxGuestIDC;
__END_DECLS
#endif

#ifdef RT_OS_SOLARIS
__BEGIN_DECLS
extern DECLVBGL(void *) VBoxGuestSolarisServiceOpen (uint32_t *pu32Version);
extern DECLVBGL(void) VBoxGuestSolarisServiceClose (void *pvOpaque);
extern DECLVBGL(int) VBoxGuestSolarisServiceCall (void *pvOpaque, unsigned int iCmd, void *pvData, size_t cbSize, size_t *pcbReturn);
__END_DECLS

#elif defined (RT_OS_FREEBSD)
__BEGIN_DECLS
extern DECLVBGL(void *) VBoxGuestFreeBSDServiceOpen (uint32_t *pu32Version);
extern DECLVBGL(void) VBoxGuestFreeBSDServiceClose (void *pvOpaque);
extern DECLVBGL(int) VBoxGuestFreeBSDServiceCall (void *pvOpaque, unsigned int iCmd, void *pvData, size_t cbSize, size_t *pcbReturn);
__END_DECLS

#endif

int vbglDriverOpen (VBGLDRIVER *pDriver)
{
#ifdef RT_OS_WINDOWS
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

#elif defined (RT_OS_LINUX)
    void *opaque;

    opaque = (void *) vboxadd_cmc_open ();
    if (!opaque)
    {
        return VERR_NOT_IMPLEMENTED;
    }
    pDriver->opaque = opaque;
    return VINF_SUCCESS;

#elif defined (RT_OS_OS2)
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

#elif defined (RT_OS_SOLARIS)
    uint32_t u32VMMDevVersion;
    pDriver->pvOpaque = VBoxGuestSolarisServiceOpen(&u32VMMDevVersion);
    if (    pDriver->pvOpaque
        &&  u32VMMDevVersion == VMMDEV_VERSION)
        return VINF_SUCCESS;

    Log(("vbglDriverOpen: failed\n"));
    return VERR_FILE_NOT_FOUND;

#elif defined (RT_OS_FREEBSD)
    uint32_t u32VMMDevVersion;
    pDriver->pvOpaque = VBoxGuestFreeBSDServiceOpen(&u32VMMDevVersion);
    if (pDriver->pvOpaque && (u32VMMDevVersion == VMMDEV_VERSION))
        return VINF_SUCCESS;

    Log(("vbglDriverOpen: failed\n"));
    return VERR_FILE_NOT_FOUND;

#else
# error "Port me"
#endif
}

int vbglDriverIOCtl (VBGLDRIVER *pDriver, uint32_t u32Function, void *pvData, uint32_t cbData)
{
#ifdef RT_OS_WINDOWS
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

#elif defined (RT_OS_LINUX)
    return vboxadd_cmc_call (pDriver->opaque, u32Function, pvData);

#elif defined (RT_OS_OS2)
    if (    pDriver->u32Session
        &&  pDriver->u32Session == g_VBoxGuestIDC.u32Session)
        return g_VBoxGuestIDC.pfnServiceEP(pDriver->u32Session, u32Function, pvData, cbData, NULL);

    Log(("vbglDriverIOCtl: No connection\n"));
    return VERR_WRONG_ORDER;

#elif defined (RT_OS_SOLARIS)
    return VBoxGuestSolarisServiceCall(pDriver->pvOpaque, u32Function, pvData, cbData, NULL);

#elif defined (RT_OS_FREEBSD)
    return VBoxGuestFreeBSDServiceCall(pDriver->pvOpaque, u32Function, pvData, cbData, NULL);

#else
# error "Port me"
#endif
}

void vbglDriverClose (VBGLDRIVER *pDriver)
{
#ifdef RT_OS_WINDOWS
    Log(("vbglDriverClose pDeviceObject=%x\n", pDriver->pDeviceObject));
    ObDereferenceObject (pDriver->pFileObject);

#elif defined (RT_OS_LINUX)
    vboxadd_cmc_close (pDriver->opaque);

#elif defined (RT_OS_OS2)
    pDriver->u32Session = 0;

#elif defined (RT_OS_SOLARIS)
    VBoxGuestSolarisServiceClose (pDriver->pvOpaque);

#elif defined (RT_OS_FREEBSD)
    VBoxGuestFreeBSDServiceClose(pDriver->pvOpaque);

#else
# error "Port me"
#endif
}

#endif /* !VBGL_VBOXGUEST */
