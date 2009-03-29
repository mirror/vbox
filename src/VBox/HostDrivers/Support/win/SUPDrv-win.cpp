/* $Id$ */
/** @file
 * VBoxDrv - The VirtualBox Support Driver - Windows NT specifics.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_SUP_DRV
#include "../SUPDrvInternal.h"
#include <excpt.h>
#include <iprt/assert.h>
#include <iprt/process.h>
#include <iprt/initterm.h>
#include <iprt/power.h>
#include <VBox/log.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The support service name. */
#define SERVICE_NAME    "VBoxDrv"
/** Win32 Device name. */
#define DEVICE_NAME     "\\\\.\\VBoxDrv"
/** NT Device name. */
#define DEVICE_NAME_NT   L"\\Device\\VBoxDrv"
/** Win Symlink name. */
#define DEVICE_NAME_DOS  L"\\DosDevices\\VBoxDrv"
/** The Pool tag (VBox). */
#define SUPDRV_NT_POOL_TAG  'xoBV'


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
#if 0 //def RT_ARCH_AMD64
typedef struct SUPDRVEXECMEM
{
    PMDL pMdl;
    void *pvMapping;
    void *pvAllocation;
} SUPDRVEXECMEM, *PSUPDRVEXECMEM;
#endif


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static void     _stdcall   VBoxDrvNtUnload(PDRIVER_OBJECT pDrvObj);
static NTSTATUS _stdcall   VBoxDrvNtCreate(PDEVICE_OBJECT pDevObj, PIRP pIrp);
static NTSTATUS _stdcall   VBoxDrvNtClose(PDEVICE_OBJECT pDevObj, PIRP pIrp);
static NTSTATUS _stdcall   VBoxDrvNtDeviceControl(PDEVICE_OBJECT pDevObj, PIRP pIrp);
static int                 VBoxDrvNtDeviceControlSlow(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PIRP pIrp, PIO_STACK_LOCATION pStack);
static NTSTATUS _stdcall   VBoxDrvNtInternalDeviceControl(PDEVICE_OBJECT pDevObj, PIRP pIrp);
static VOID     _stdcall   VBoxPowerDispatchCallback(PVOID pCallbackContext, PVOID pArgument1, PVOID pArgument2);
static NTSTATUS _stdcall   VBoxDrvNtNotSupportedStub(PDEVICE_OBJECT pDevObj, PIRP pIrp);
static NTSTATUS            VBoxDrvNtErr2NtStatus(int rc);

/*******************************************************************************
*   External Functions                                                         *
*******************************************************************************/
DECLASM(int)   UNWIND_WRAP(RTPowerSignalEvent)(RTPOWEREVENT enmEvent);

/*******************************************************************************
*   Exported Functions                                                         *
*******************************************************************************/
__BEGIN_DECLS
ULONG _stdcall DriverEntry(PDRIVER_OBJECT pDrvObj, PUNICODE_STRING pRegPath);
__END_DECLS


/**
 * Driver entry point.
 *
 * @returns appropriate status code.
 * @param   pDrvObj     Pointer to driver object.
 * @param   pRegPath    Registry base path.
 */
ULONG _stdcall DriverEntry(PDRIVER_OBJECT pDrvObj, PUNICODE_STRING pRegPath)
{
    NTSTATUS    rc;
    dprintf(("VBoxDrv::DriverEntry\n"));

    /*
     * Create device.
     * (That means creating a device object and a symbolic link so the DOS
     * subsystems (OS/2, win32, ++) can access the device.)
     */
    UNICODE_STRING  DevName;
    RtlInitUnicodeString(&DevName, DEVICE_NAME_NT);
    PDEVICE_OBJECT  pDevObj;
    rc = IoCreateDevice(pDrvObj, sizeof(SUPDRVDEVEXT), &DevName, FILE_DEVICE_UNKNOWN, 0, FALSE, &pDevObj);
    if (NT_SUCCESS(rc))
    {
        UNICODE_STRING DosName;
        RtlInitUnicodeString(&DosName, DEVICE_NAME_DOS);
        rc = IoCreateSymbolicLink(&DosName, &DevName);
        if (NT_SUCCESS(rc))
        {
            int vrc = RTR0Init(0);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Initialize the device extension.
                 */
                PSUPDRVDEVEXT pDevExt = (PSUPDRVDEVEXT)pDevObj->DeviceExtension;
                memset(pDevExt, 0, sizeof(*pDevExt));

                vrc = supdrvInitDevExt(pDevExt);
                if (!vrc)
                {
                    /*
                     * Setup the driver entry points in pDrvObj.
                     */
                    pDrvObj->DriverUnload                                   = VBoxDrvNtUnload;
                    pDrvObj->MajorFunction[IRP_MJ_CREATE]                   = VBoxDrvNtCreate;
                    pDrvObj->MajorFunction[IRP_MJ_CLOSE]                    = VBoxDrvNtClose;
                    pDrvObj->MajorFunction[IRP_MJ_DEVICE_CONTROL]           = VBoxDrvNtDeviceControl;
//#if 0 /** @todo test IDC on windows. */
                    pDrvObj->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL]  = VBoxDrvNtInternalDeviceControl;
//#endif
                    pDrvObj->MajorFunction[IRP_MJ_READ]                     = VBoxDrvNtNotSupportedStub;
                    pDrvObj->MajorFunction[IRP_MJ_WRITE]                    = VBoxDrvNtNotSupportedStub;

                    /* more? */

                    /* Register ourselves for power state changes. */
                    UNICODE_STRING      CallbackName;
                    OBJECT_ATTRIBUTES   Attr;

                    RtlInitUnicodeString(&CallbackName, L"\\Callback\\PowerState");
                    InitializeObjectAttributes(&Attr, &CallbackName, OBJ_CASE_INSENSITIVE, NULL, NULL);

                    rc = ExCreateCallback(&pDevExt->pObjPowerCallback, &Attr, TRUE, TRUE);
                    if (rc == STATUS_SUCCESS)
                        pDevExt->hPowerCallback = ExRegisterCallback(pDevExt->pObjPowerCallback, VBoxPowerDispatchCallback, pDevObj);

                    dprintf(("VBoxDrv::DriverEntry returning STATUS_SUCCESS\n"));
                    return STATUS_SUCCESS;
                }

                dprintf(("supdrvInitDevExit failed with vrc=%d!\n", vrc));
                rc = VBoxDrvNtErr2NtStatus(vrc);

                IoDeleteSymbolicLink(&DosName);
                RTR0Term();
            }
            else
            {
                dprintf(("RTR0Init failed with vrc=%d!\n", vrc));
                rc = VBoxDrvNtErr2NtStatus(vrc);
            }
        }
        else
            dprintf(("IoCreateSymbolicLink failed with rc=%#x!\n", rc));

        IoDeleteDevice(pDevObj);
    }
    else
        dprintf(("IoCreateDevice failed with rc=%#x!\n", rc));

    if (NT_SUCCESS(rc))
        rc = STATUS_INVALID_PARAMETER;
    dprintf(("VBoxDrv::DriverEntry returning %#x\n", rc));
    return rc;
}


/**
 * Unload the driver.
 *
 * @param   pDrvObj     Driver object.
 */
void _stdcall VBoxDrvNtUnload(PDRIVER_OBJECT pDrvObj)
{
    PSUPDRVDEVEXT pDevExt = (PSUPDRVDEVEXT)pDrvObj->DeviceObject->DeviceExtension;

    dprintf(("VBoxDrvNtUnload at irql %d\n", KeGetCurrentIrql()));

    /* Clean up the power callback registration. */
    if (pDevExt->hPowerCallback)
        ExUnregisterCallback(pDevExt->hPowerCallback);
    if (pDevExt->pObjPowerCallback)
        ObDereferenceObject(pDevExt->pObjPowerCallback);

    /*
     * We ASSUME that it's not possible to unload a driver with open handles.
     * Start by deleting the symbolic link
     */
    UNICODE_STRING DosName;
    RtlInitUnicodeString(&DosName, DEVICE_NAME_DOS);
    NTSTATUS rc = IoDeleteSymbolicLink(&DosName);

    /*
     * Terminate the GIP page and delete the device extension.
     */
    supdrvDeleteDevExt(pDevExt);
    RTR0Term();
    IoDeleteDevice(pDrvObj->DeviceObject);
}


/**
 * Create (i.e. Open) file entry point.
 *
 * @param   pDevObj     Device object.
 * @param   pIrp        Request packet.
 */
NTSTATUS _stdcall VBoxDrvNtCreate(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    dprintf(("VBoxDrvNtCreate\n"));
    PIO_STACK_LOCATION  pStack = IoGetCurrentIrpStackLocation(pIrp);
    PFILE_OBJECT        pFileObj = pStack->FileObject;
    PSUPDRVDEVEXT       pDevExt = (PSUPDRVDEVEXT)pDevObj->DeviceExtension;

    /*
     * We are not remotely similar to a directory...
     * (But this is possible.)
     */
    if (pStack->Parameters.Create.Options & FILE_DIRECTORY_FILE)
    {
        pIrp->IoStatus.Status       = STATUS_NOT_A_DIRECTORY;
        pIrp->IoStatus.Information  = 0;
        IoCompleteRequest(pIrp, IO_NO_INCREMENT);
        return STATUS_NOT_A_DIRECTORY;
    }

    /*
     * Call common code for the rest.
     */
    pFileObj->FsContext = NULL;
    PSUPDRVSESSION pSession;
//#if 0 /** @todo check if this works, consider OBJ_KERNEL_HANDLE too. */
    bool fUser = pIrp->RequestorMode != KernelMode;
//#else
 //   bool fUser = true;
//#endif
    int rc = supdrvCreateSession(pDevExt, fUser, &pSession);
    if (!rc)
        pFileObj->FsContext = pSession;

    NTSTATUS    rcNt = pIrp->IoStatus.Status = VBoxDrvNtErr2NtStatus(rc);
    pIrp->IoStatus.Information  = 0;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);

    return rcNt;
}


/**
 * Close file entry point.
 *
 * @param   pDevObj     Device object.
 * @param   pIrp        Request packet.
 */
NTSTATUS _stdcall VBoxDrvNtClose(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    PSUPDRVDEVEXT       pDevExt = (PSUPDRVDEVEXT)pDevObj->DeviceExtension;
    PIO_STACK_LOCATION  pStack = IoGetCurrentIrpStackLocation(pIrp);
    PFILE_OBJECT        pFileObj = pStack->FileObject;
    dprintf(("VBoxDrvNtClose: pDevExt=%p pFileObj=%p pSession=%p\n",
             pDevExt, pFileObj, pFileObj->FsContext));
    supdrvCloseSession(pDevExt, (PSUPDRVSESSION)pFileObj->FsContext);
    pFileObj->FsContext = NULL;
    pIrp->IoStatus.Information = 0;
    pIrp->IoStatus.Status = STATUS_SUCCESS;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}


/**
 * Device I/O Control entry point.
 *
 * @param   pDevObj     Device object.
 * @param   pIrp        Request packet.
 */
NTSTATUS _stdcall VBoxDrvNtDeviceControl(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    PSUPDRVDEVEXT       pDevExt = (PSUPDRVDEVEXT)pDevObj->DeviceExtension;
    PIO_STACK_LOCATION  pStack = IoGetCurrentIrpStackLocation(pIrp);
    PSUPDRVSESSION      pSession = (PSUPDRVSESSION)pStack->FileObject->FsContext;

    /*
     * Deal with the two high-speed IOCtl that takes it's arguments from
     * the session and iCmd, and only returns a VBox status code.
     *
     * Note: The previous method of returning the rc prior to IOC version
     *       7.4 has been abandond, we're no longer compatible with that
     *       interface.
     */
    ULONG ulCmd = pStack->Parameters.DeviceIoControl.IoControlCode;
    if (    ulCmd == SUP_IOCTL_FAST_DO_RAW_RUN
        ||  ulCmd == SUP_IOCTL_FAST_DO_HWACC_RUN
        ||  ulCmd == SUP_IOCTL_FAST_DO_NOP)
    {
        /* Raise the IRQL to DISPATCH_LEVEl to prevent Windows from rescheduling us to another CPU/core. */
        Assert(KeGetCurrentIrql() <= DISPATCH_LEVEL);
        KIRQL oldIrql;
        KeRaiseIrql(DISPATCH_LEVEL, &oldIrql);
        int rc = supdrvIOCtlFast(ulCmd, (unsigned)(uintptr_t)pIrp->UserBuffer /* VMCPU id */, pDevExt, pSession);
        KeLowerIrql(oldIrql);

        /* Complete the I/O request. */
        NTSTATUS rcNt = pIrp->IoStatus.Status = RT_SUCCESS(rc) ? STATUS_SUCCESS : STATUS_INVALID_PARAMETER;
        IoCompleteRequest(pIrp, IO_NO_INCREMENT);
        return rcNt;
    }

    return VBoxDrvNtDeviceControlSlow(pDevExt, pSession, pIrp, pStack);
}


/**
 * Worker for VBoxDrvNtDeviceControl that takes the slow IOCtl functions.
 *
 * @returns NT status code.
 *
 * @param   pDevObj     Device object.
 * @param   pSession    The session.
 * @param   pIrp        Request packet.
 * @param   pStack      The stack location containing the DeviceControl parameters.
 */
static int VBoxDrvNtDeviceControlSlow(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PIRP pIrp, PIO_STACK_LOCATION pStack)
{
    NTSTATUS    rcNt;
    unsigned    cbOut = 0;
    int         rc = 0;
    dprintf2(("VBoxDrvNtDeviceControlSlow(%p,%p): ioctl=%#x pBuf=%p cbIn=%#x cbOut=%#x pSession=%p\n",
             pDevExt, pIrp, pStack->Parameters.DeviceIoControl.IoControlCode,
             pIrp->AssociatedIrp.SystemBuffer, pStack->Parameters.DeviceIoControl.InputBufferLength,
             pStack->Parameters.DeviceIoControl.OutputBufferLength, pSession));

#ifdef RT_ARCH_AMD64
    /* Don't allow 32-bit processes to do any I/O controls. */
    if (!IoIs32bitProcess(pIrp))
#endif
    {
        /* Verify that it's a buffered CTL. */
        if ((pStack->Parameters.DeviceIoControl.IoControlCode & 0x3) == METHOD_BUFFERED)
        {
            /* Verify that the sizes in the request header are correct. */
            PSUPREQHDR pHdr = (PSUPREQHDR)pIrp->AssociatedIrp.SystemBuffer;
            if (    pStack->Parameters.DeviceIoControl.InputBufferLength >= sizeof(*pHdr)
                &&  pStack->Parameters.DeviceIoControl.InputBufferLength ==  pHdr->cbIn
                &&  pStack->Parameters.DeviceIoControl.OutputBufferLength ==  pHdr->cbOut)
            {
                /*
                 * Do the job.
                 */
                rc = supdrvIOCtl(pStack->Parameters.DeviceIoControl.IoControlCode, pDevExt, pSession, pHdr);
                if (!rc)
                {
                    rcNt = STATUS_SUCCESS;
                    cbOut = pHdr->cbOut;
                    if (cbOut > pStack->Parameters.DeviceIoControl.OutputBufferLength)
                    {
                        cbOut = pStack->Parameters.DeviceIoControl.OutputBufferLength;
                        OSDBGPRINT(("VBoxDrvLinuxIOCtl: too much output! %#x > %#x; uCmd=%#x!\n",
                                    pHdr->cbOut, cbOut, pStack->Parameters.DeviceIoControl.IoControlCode));
                    }
                }
                else
                    rcNt = STATUS_INVALID_PARAMETER;
                dprintf2(("VBoxDrvNtDeviceControlSlow: returns %#x cbOut=%d rc=%#x\n", rcNt, cbOut, rc));
            }
            else
            {
                dprintf(("VBoxDrvNtDeviceControlSlow: Mismatching sizes (%#x) - Hdr=%#lx/%#lx Irp=%#lx/%#lx!\n",
                         pStack->Parameters.DeviceIoControl.IoControlCode,
                         pStack->Parameters.DeviceIoControl.InputBufferLength >= sizeof(*pHdr) ? pHdr->cbIn : 0,
                         pStack->Parameters.DeviceIoControl.InputBufferLength >= sizeof(*pHdr) ? pHdr->cbOut : 0,
                         pStack->Parameters.DeviceIoControl.InputBufferLength,
                         pStack->Parameters.DeviceIoControl.OutputBufferLength));
                rcNt = STATUS_INVALID_PARAMETER;
            }
        }
        else
        {
            dprintf(("VBoxDrvNtDeviceControlSlow: not buffered request (%#x) - not supported\n",
                     pStack->Parameters.DeviceIoControl.IoControlCode));
            rcNt = STATUS_NOT_SUPPORTED;
        }
    }
#ifdef RT_ARCH_AMD64
    else
    {
        dprintf(("VBoxDrvNtDeviceControlSlow: WOW64 req - not supported\n"));
        rcNt = STATUS_NOT_SUPPORTED;
    }
#endif

    /* complete the request. */
    pIrp->IoStatus.Status = rcNt;
    pIrp->IoStatus.Information = cbOut;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);
    return rcNt;
}


/**
 * Internal Device I/O Control entry point, used for IDC.
 *
 * @param   pDevObj     Device object.
 * @param   pIrp        Request packet.
 */
NTSTATUS _stdcall VBoxDrvNtInternalDeviceControl(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    PSUPDRVDEVEXT       pDevExt = (PSUPDRVDEVEXT)pDevObj->DeviceExtension;
    PIO_STACK_LOCATION  pStack = IoGetCurrentIrpStackLocation(pIrp);
    PFILE_OBJECT        pFileObj = pStack ? pStack->FileObject : NULL;
    PSUPDRVSESSION      pSession = pFileObj ? (PSUPDRVSESSION)pFileObj->FsContext : NULL;
    NTSTATUS            rcNt;
    unsigned            cbOut = 0;
    int                 rc = 0;
    dprintf2(("VBoxDrvNtInternalDeviceControl(%p,%p): ioctl=%#x pBuf=%p cbIn=%#x cbOut=%#x pSession=%p\n",
             pDevExt, pIrp, pStack->Parameters.DeviceIoControl.IoControlCode,
             pIrp->AssociatedIrp.SystemBuffer, pStack->Parameters.DeviceIoControl.InputBufferLength,
             pStack->Parameters.DeviceIoControl.OutputBufferLength, pSession));

/** @todo IDC on NT: figure when to create the session and that stuff... */

    /* Verify that it's a buffered CTL. */
    if ((pStack->Parameters.DeviceIoControl.IoControlCode & 0x3) == METHOD_BUFFERED)
    {
        /* Verify the pDevExt in the session. */
        if (    (   !pSession
                 && pStack->Parameters.DeviceIoControl.IoControlCode == SUPDRV_IDC_REQ_CONNECT)
            ||  (   VALID_PTR(pSession)
                 && pSession->pDevExt == pDevExt))
        {
            /* Verify that the size in the request header is correct. */
            PSUPDRVIDCREQHDR pHdr = (PSUPDRVIDCREQHDR)pIrp->AssociatedIrp.SystemBuffer;
            if (    pStack->Parameters.DeviceIoControl.InputBufferLength >= sizeof(*pHdr)
                &&  pStack->Parameters.DeviceIoControl.InputBufferLength  == pHdr->cb
                &&  pStack->Parameters.DeviceIoControl.OutputBufferLength == pHdr->cb)
            {
                /*
                 * Do the job.
                 */
                rc = supdrvIDC(pStack->Parameters.DeviceIoControl.IoControlCode, pDevExt, pSession, pHdr);
                if (!rc)
                {
                    rcNt = STATUS_SUCCESS;
                    cbOut = pHdr->cb;
                }
                else
                    rcNt = STATUS_INVALID_PARAMETER;
                dprintf2(("VBoxDrvNtInternalDeviceControl: returns %#x/rc=%#x\n", rcNt, rc));
            }
            else
            {
                dprintf(("VBoxDrvNtInternalDeviceControl: Mismatching sizes (%#x) - Hdr=%#lx Irp=%#lx/%#lx!\n",
                         pStack->Parameters.DeviceIoControl.IoControlCode,
                         pStack->Parameters.DeviceIoControl.InputBufferLength >= sizeof(*pHdr) ? pHdr->cb : 0,
                         pStack->Parameters.DeviceIoControl.InputBufferLength,
                         pStack->Parameters.DeviceIoControl.OutputBufferLength));
                rcNt = STATUS_INVALID_PARAMETER;
            }
        }
        else
            rcNt = STATUS_NOT_SUPPORTED;
    }
    else
    {
        dprintf(("VBoxDrvNtInternalDeviceControl: not buffered request (%#x) - not supported\n",
                 pStack->Parameters.DeviceIoControl.IoControlCode));
        rcNt = STATUS_NOT_SUPPORTED;
    }

    /* complete the request. */
    pIrp->IoStatus.Status = rcNt;
    pIrp->IoStatus.Information = cbOut;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);
    return rcNt;
}


/**
 * Stub function for functions we don't implemented.
 *
 * @returns STATUS_NOT_SUPPORTED
 * @param   pDevObj     Device object.
 * @param   pIrp        IRP.
 */
NTSTATUS _stdcall VBoxDrvNtNotSupportedStub(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    dprintf(("VBoxDrvNtNotSupportedStub\n"));
    pDevObj = pDevObj;

    pIrp->IoStatus.Information = 0;
    pIrp->IoStatus.Status = STATUS_NOT_SUPPORTED;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);

    return STATUS_NOT_SUPPORTED;
}


/**
 * ExRegisterCallback handler for power events
 *
 * @param   pCallbackContext    User supplied parameter (pDevObj)
 * @param   pArgument1          First argument
 * @param   pArgument2          Second argument
 */
VOID _stdcall VBoxPowerDispatchCallback(PVOID pCallbackContext, PVOID pArgument1, PVOID pArgument2)
{
    PDEVICE_OBJECT pDevObj = (PDEVICE_OBJECT)pCallbackContext;

    dprintf(("VBoxPowerDispatchCallback: %x %x\n", pArgument1, pArgument2));

    /* Power change imminent? */
    if ((unsigned)pArgument1 == PO_CB_SYSTEM_STATE_LOCK)
    {
        if ((unsigned)pArgument2 == 0)
            dprintf(("VBoxPowerDispatchCallback: about to go into suspend mode!\n"));
        else
            dprintf(("VBoxPowerDispatchCallback: resumed!\n"));

        /* Inform any clients that have registered themselves with IPRT. */
        UNWIND_WRAP(RTPowerSignalEvent)(((unsigned)pArgument2 == 0) ? RTPOWEREVENT_SUSPEND : RTPOWEREVENT_RESUME);
    }
}


/**
 * Initializes any OS specific object creator fields.
 */
void VBOXCALL   supdrvOSObjInitCreator(PSUPDRVOBJ pObj, PSUPDRVSESSION pSession)
{
    NOREF(pObj);
    NOREF(pSession);
}


/**
 * Checks if the session can access the object.
 *
 * @returns true if a decision has been made.
 * @returns false if the default access policy should be applied.
 *
 * @param   pObj        The object in question.
 * @param   pSession    The session wanting to access the object.
 * @param   pszObjName  The object name, can be NULL.
 * @param   prc         Where to store the result when returning true.
 */
bool VBOXCALL   supdrvOSObjCanAccess(PSUPDRVOBJ pObj, PSUPDRVSESSION pSession, const char *pszObjName, int *prc)
{
    NOREF(pObj);
    NOREF(pSession);
    NOREF(pszObjName);
    NOREF(prc);
    return false;
}


/**
 * Force async tsc mode (stub).
 */
bool VBOXCALL  supdrvOSGetForcedAsyncTscMode(PSUPDRVDEVEXT pDevExt)
{
    return false;
}


/**
 * Converts a supdrv error code to an nt status code.
 *
 * @returns corresponding nt status code.
 * @param   rc  supdrv error code (SUPDRV_ERR_* defines).
 */
static NTSTATUS     VBoxDrvNtErr2NtStatus(int rc)
{
    switch (rc)
    {
        case 0:                             return STATUS_SUCCESS;
        case SUPDRV_ERR_GENERAL_FAILURE:    return STATUS_NOT_SUPPORTED;
        case SUPDRV_ERR_INVALID_PARAM:      return STATUS_INVALID_PARAMETER;
        case SUPDRV_ERR_INVALID_MAGIC:      return STATUS_UNKNOWN_REVISION;
        case SUPDRV_ERR_INVALID_HANDLE:     return STATUS_INVALID_HANDLE;
        case SUPDRV_ERR_INVALID_POINTER:    return STATUS_INVALID_ADDRESS;
        case SUPDRV_ERR_LOCK_FAILED:        return STATUS_NOT_LOCKED;
        case SUPDRV_ERR_ALREADY_LOADED:     return STATUS_IMAGE_ALREADY_LOADED;
        case SUPDRV_ERR_PERMISSION_DENIED:  return STATUS_ACCESS_DENIED;
        case SUPDRV_ERR_VERSION_MISMATCH:   return STATUS_REVISION_MISMATCH;
    }

    return STATUS_UNSUCCESSFUL;
}



/** @todo use the nocrt stuff? */
int VBOXCALL mymemcmp(const void *pv1, const void *pv2, size_t cb)
{
    const uint8_t *pb1 = (const uint8_t *)pv1;
    const uint8_t *pb2 = (const uint8_t *)pv2;
    for (; cb > 0; cb--, pb1++, pb2++)
        if (*pb1 != *pb2)
            return *pb1 - *pb2;
    return 0;
}


#if 0 /* See alternative in SUPDrvA-win.asm */
/**
 * Alternative version of SUPR0Printf for Windows.
 *
 * @returns 0.
 * @param   pszFormat   The format string.
 */
SUPR0DECL(int) SUPR0Printf(const char *pszFormat, ...)
{
    va_list va;
    char    szMsg[512];

    va_start(va, pszFormat);
    size_t cch = RTStrPrintfV(szMsg, sizeof(szMsg) - 1, pszFormat, va);
    szMsg[sizeof(szMsg) - 1] = '\0';
    va_end(va);

    RTLogWriteDebugger(szMsg, cch);
    return 0;
}
#endif
