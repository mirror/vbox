/* $Id$ */
/** @file
 * VBoxDrv - The VirtualBox Support Driver - Windows NT specifics.
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_SUP_DRV
#include "../SUPDrvInternal.h"
#include <excpt.h>
#include <ntimage.h>

#include <iprt/assert.h>
#include <iprt/initterm.h>
#include <iprt/mem.h>
#include <iprt/process.h>
#include <iprt/power.h>
#include <iprt/string.h>
#include <VBox/log.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The support service name. */
#define SERVICE_NAME    "VBoxDrv"
/** The Pool tag (VBox). */
#define SUPDRV_NT_POOL_TAG  'xoBV'

/** Win32 device name for system access. */
#define DEVICE_NAME_SYS         "\\\\.\\VBoxDrv"
/** NT device name for system access. */
#define DEVICE_NAME_NT_SYS      L"\\Device\\VBoxDrv"
/** Win Symlink name for system access. */
#define DEVICE_NAME_DOS_SYS     L"\\DosDevices\\VBoxDrv"

/** Win32 device name for user access. */
#define DEVICE_NAME_USR         "\\\\.\\VBoxDrvU"
/** NT device name for user access. */
#define DEVICE_NAME_NT_USR      L"\\Device\\VBoxDrvU"
/** Win Symlink name for user access. */
#define DEVICE_NAME_DOS_USR     L"\\DosDevices\\VBoxDrvU"


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

/**
 * Device extension used by VBoxDrvU.
 */
typedef struct SUPDRVDEVEXTUSR
{
    /** Global cookie (same location as in SUPDRVDEVEXT, different value). */
    uint32_t                        u32Cookie;
    /** Pointer to the main driver extension. */
    PSUPDRVDEVEXT                   pMainDrvExt;
} SUPDRVDEVEXTUSR;
AssertCompileMembersAtSameOffset(SUPDRVDEVEXT, u32Cookie, SUPDRVDEVEXTUSR, u32Cookie);
/** Pointer to the VBoxDrvU device extension. */
typedef SUPDRVDEVEXTUSR *PSUPDRVDEVEXTUSR;
/** Value of SUPDRVDEVEXTUSR::u32Cookie. */
#define SUPDRVDEVEXTUSR_COOKIE      UINT32_C(0x12345678)

/** Get the main device extension. */
#define SUPDRVNT_GET_DEVEXT(pDevObj) \
    (  pDevObj != g_pDevObjUsr \
     ? (PSUPDRVDEVEXT)pDevObj->DeviceExtension \
     : ((PSUPDRVDEVEXTUSR)pDevObj->DeviceExtension)->pMainDrvExt )


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static void     _stdcall   VBoxDrvNtUnload(PDRIVER_OBJECT pDrvObj);
static NTSTATUS _stdcall   VBoxDrvNtCreate(PDEVICE_OBJECT pDevObj, PIRP pIrp);
static NTSTATUS _stdcall   VBoxDrvNtCleanup(PDEVICE_OBJECT pDevObj, PIRP pIrp);
static NTSTATUS _stdcall   VBoxDrvNtClose(PDEVICE_OBJECT pDevObj, PIRP pIrp);
static NTSTATUS _stdcall   VBoxDrvNtDeviceControl(PDEVICE_OBJECT pDevObj, PIRP pIrp);
static int                 VBoxDrvNtDeviceControlSlow(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PIRP pIrp, PIO_STACK_LOCATION pStack);
static NTSTATUS _stdcall   VBoxDrvNtInternalDeviceControl(PDEVICE_OBJECT pDevObj, PIRP pIrp);
static VOID     _stdcall   VBoxPowerDispatchCallback(PVOID pCallbackContext, PVOID pArgument1, PVOID pArgument2);
static NTSTATUS _stdcall   VBoxDrvNtNotSupportedStub(PDEVICE_OBJECT pDevObj, PIRP pIrp);
static NTSTATUS            VBoxDrvNtErr2NtStatus(int rc);


/*******************************************************************************
*   Exported Functions                                                         *
*******************************************************************************/
RT_C_DECLS_BEGIN
ULONG _stdcall DriverEntry(PDRIVER_OBJECT pDrvObj, PUNICODE_STRING pRegPath);
RT_C_DECLS_END


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Pointer to the system device instance. */
static PDEVICE_OBJECT g_pDevObjSys = NULL;
/** Pointer to the user device instance. */
static PDEVICE_OBJECT g_pDevObjUsr = NULL;


/**
 * Takes care of creating the devices and their symbolic links.
 *
 * @returns NT status code.
 * @param   pDrvObj     Pointer to driver object.
 */
static NTSTATUS vboxdrvNtCreateDevices(PDRIVER_OBJECT pDrvObj)
{
    /*
     * System device.
     */
    UNICODE_STRING DevName;
    RtlInitUnicodeString(&DevName, DEVICE_NAME_NT_SYS);
    NTSTATUS rcNt = IoCreateDevice(pDrvObj, sizeof(SUPDRVDEVEXT), &DevName, FILE_DEVICE_UNKNOWN, 0, FALSE, &g_pDevObjSys);
    if (NT_SUCCESS(rcNt))
    {
        UNICODE_STRING DosName;
        RtlInitUnicodeString(&DosName, DEVICE_NAME_DOS_SYS);
        rcNt = IoCreateSymbolicLink(&DosName, &DevName);
        if (NT_SUCCESS(rcNt))
        {
            /*
             * User device.
             */
            RtlInitUnicodeString(&DevName, DEVICE_NAME_NT_USR);
            rcNt = IoCreateDevice(pDrvObj, sizeof(SUPDRVDEVEXTUSR), &DevName, FILE_DEVICE_UNKNOWN, 0, FALSE, &g_pDevObjUsr);
            if (NT_SUCCESS(rcNt))
            {
                UNICODE_STRING DosName;
                RtlInitUnicodeString(&DosName, DEVICE_NAME_DOS_USR);
                rcNt = IoCreateSymbolicLink(&DosName, &DevName);
                if (NT_SUCCESS(rcNt))
                {
                    PSUPDRVDEVEXTUSR pDevExtUsr = (PSUPDRVDEVEXTUSR)g_pDevObjUsr->DeviceExtension;
                    pDevExtUsr->pMainDrvExt = (PSUPDRVDEVEXT)g_pDevObjSys->DeviceExtension;
                    pDevExtUsr->u32Cookie   = SUPDRVDEVEXTUSR_COOKIE;

                    /* Done. */
                    return rcNt;
                }

                /* Bail out. */
                IoDeleteDevice(g_pDevObjUsr);
                g_pDevObjUsr = NULL;
            }
            IoDeleteSymbolicLink(&DosName);
        }
        IoDeleteDevice(g_pDevObjSys);
        g_pDevObjSys = NULL;
    }
    return rcNt;
}

/**
 * Destroys the devices and links created by vboxdrvNtCreateDevices.
 */
static void vboxdrvNtDestroyDevices(void)
{
    UNICODE_STRING DosName;
    RtlInitUnicodeString(&DosName, DEVICE_NAME_DOS_SYS);
    NTSTATUS rcNt = IoDeleteSymbolicLink(&DosName);

    RtlInitUnicodeString(&DosName, DEVICE_NAME_DOS_USR);
    rcNt = IoDeleteSymbolicLink(&DosName);

    PSUPDRVDEVEXTUSR pDevExtUsr = (PSUPDRVDEVEXTUSR)g_pDevObjUsr->DeviceExtension;
    pDevExtUsr->pMainDrvExt = NULL;

    IoDeleteDevice(g_pDevObjUsr);
    g_pDevObjUsr = NULL;
    IoDeleteDevice(g_pDevObjSys);
    g_pDevObjSys = NULL;
}


/**
 * Driver entry point.
 *
 * @returns appropriate status code.
 * @param   pDrvObj     Pointer to driver object.
 * @param   pRegPath    Registry base path.
 */
ULONG _stdcall DriverEntry(PDRIVER_OBJECT pDrvObj, PUNICODE_STRING pRegPath)
{
    /*
     * Create device.
     * (That means creating a device object and a symbolic link so the DOS
     * subsystems (OS/2, win32, ++) can access the device.)
     */
    NTSTATUS rcNt = vboxdrvNtCreateDevices(pDrvObj);
    if (NT_SUCCESS(rcNt))
    {
        int vrc = RTR0Init(0);
        if (RT_SUCCESS(vrc))
        {
            Log(("VBoxDrv::DriverEntry\n"));

            /*
             * Initialize the device extension.
             */
            PSUPDRVDEVEXT pDevExt = (PSUPDRVDEVEXT)g_pDevObjSys->DeviceExtension;
            memset(pDevExt, 0, sizeof(*pDevExt));

            vrc = supdrvInitDevExt(pDevExt, sizeof(SUPDRVSESSION));
            if (!vrc)
            {
                /*
                 * Setup the driver entry points in pDrvObj.
                 */
                pDrvObj->DriverUnload                                   = VBoxDrvNtUnload;
                pDrvObj->MajorFunction[IRP_MJ_CREATE]                   = VBoxDrvNtCreate;
                pDrvObj->MajorFunction[IRP_MJ_CLEANUP]                  = VBoxDrvNtCleanup;
                pDrvObj->MajorFunction[IRP_MJ_CLOSE]                    = VBoxDrvNtClose;
                pDrvObj->MajorFunction[IRP_MJ_DEVICE_CONTROL]           = VBoxDrvNtDeviceControl;
                pDrvObj->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL]  = VBoxDrvNtInternalDeviceControl;
                pDrvObj->MajorFunction[IRP_MJ_READ]                     = VBoxDrvNtNotSupportedStub;
                pDrvObj->MajorFunction[IRP_MJ_WRITE]                    = VBoxDrvNtNotSupportedStub;

                /* more? */

                /* Register ourselves for power state changes. */
                UNICODE_STRING      CallbackName;
                OBJECT_ATTRIBUTES   Attr;

                RtlInitUnicodeString(&CallbackName, L"\\Callback\\PowerState");
                InitializeObjectAttributes(&Attr, &CallbackName, OBJ_CASE_INSENSITIVE, NULL, NULL);

                rcNt = ExCreateCallback(&pDevExt->pObjPowerCallback, &Attr, TRUE, TRUE);
                if (rcNt == STATUS_SUCCESS)
                    pDevExt->hPowerCallback = ExRegisterCallback(pDevExt->pObjPowerCallback, VBoxPowerDispatchCallback,
                                                                 g_pDevObjSys);

                Log(("VBoxDrv::DriverEntry returning STATUS_SUCCESS\n"));
                return STATUS_SUCCESS;
            }

            Log(("supdrvInitDevExit failed with vrc=%d!\n", vrc));
            rcNt = VBoxDrvNtErr2NtStatus(vrc);

            RTR0Term();
        }
        else
        {
            Log(("RTR0Init failed with vrc=%d!\n", vrc));
            rcNt = VBoxDrvNtErr2NtStatus(vrc);
        }

        vboxdrvNtDestroyDevices();
    }
    if (NT_SUCCESS(rcNt))
        rcNt = STATUS_INVALID_PARAMETER;
    return rcNt;
}


/**
 * Unload the driver.
 *
 * @param   pDrvObj     Driver object.
 */
void _stdcall VBoxDrvNtUnload(PDRIVER_OBJECT pDrvObj)
{
    PSUPDRVDEVEXT pDevExt = (PSUPDRVDEVEXT)g_pDevObjSys->DeviceExtension;

    Log(("VBoxDrvNtUnload at irql %d\n", KeGetCurrentIrql()));

    /* Clean up the power callback registration. */
    if (pDevExt->hPowerCallback)
        ExUnregisterCallback(pDevExt->hPowerCallback);
    if (pDevExt->pObjPowerCallback)
        ObDereferenceObject(pDevExt->pObjPowerCallback);

    /*
     * We ASSUME that it's not possible to unload a driver with open handles.
     */
    supdrvDeleteDevExt(pDevExt);
    RTR0Term();
    vboxdrvNtDestroyDevices();

    NOREF(pDrvObj);
}


/**
 * Create (i.e. Open) file entry point.
 *
 * @param   pDevObj     Device object.
 * @param   pIrp        Request packet.
 */
NTSTATUS _stdcall VBoxDrvNtCreate(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    Log(("VBoxDrvNtCreate: RequestorMode=%d\n", pIrp->RequestorMode));
    const bool          fUnrestricted = pDevObj == g_pDevObjSys;
    PIO_STACK_LOCATION  pStack = IoGetCurrentIrpStackLocation(pIrp);
    PFILE_OBJECT        pFileObj = pStack->FileObject;
    PSUPDRVDEVEXT       pDevExt = SUPDRVNT_GET_DEVEXT(pDevObj);

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
     * Don't create a session for kernel clients, they'll close the handle
     * immediately and work with the file object via
     * VBoxDrvNtInternalDeviceControl.  The first request will there be one
     * to create a session.
     */
    NTSTATUS rcNt;
    if (pIrp->RequestorMode == KernelMode)
        rcNt = STATUS_SUCCESS;
    else
    {
        /*
         * Call common code for the rest.
         */
        pFileObj->FsContext = NULL;
        PSUPDRVSESSION pSession;
        int rc = supdrvCreateSession(pDevExt, true /*fUser*/, fUnrestricted, &pSession);
        if (!rc)
            pFileObj->FsContext = pSession;
        rcNt = pIrp->IoStatus.Status = VBoxDrvNtErr2NtStatus(rc);
    }

    pIrp->IoStatus.Information  = 0;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);

    return rcNt;
}


/**
 * Clean up file handle entry point.
 *
 * @param   pDevObj     Device object.
 * @param   pIrp        Request packet.
 */
NTSTATUS _stdcall VBoxDrvNtCleanup(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    PSUPDRVDEVEXT       pDevExt  = SUPDRVNT_GET_DEVEXT(pDevObj);
    PIO_STACK_LOCATION  pStack   = IoGetCurrentIrpStackLocation(pIrp);
    PFILE_OBJECT        pFileObj = pStack->FileObject;
    PSUPDRVSESSION      pSession = (PSUPDRVSESSION)pFileObj->FsContext;

    Log(("VBoxDrvNtCleanup: pDevExt=%p pFileObj=%p pSession=%p\n", pDevExt, pFileObj, pSession));
    if (pSession)
    {
        supdrvCloseSession(pDevExt, (PSUPDRVSESSION)pFileObj->FsContext);
        pFileObj->FsContext = NULL;
    }

    pIrp->IoStatus.Information = 0;
    pIrp->IoStatus.Status = STATUS_SUCCESS;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}


/**
 * Close file entry point.
 *
 * @param   pDevObj     Device object.
 * @param   pIrp        Request packet.
 */
NTSTATUS _stdcall VBoxDrvNtClose(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    PSUPDRVDEVEXT       pDevExt  = SUPDRVNT_GET_DEVEXT(pDevObj);
    PIO_STACK_LOCATION  pStack   = IoGetCurrentIrpStackLocation(pIrp);
    PFILE_OBJECT        pFileObj = pStack->FileObject;
    PSUPDRVSESSION      pSession = (PSUPDRVSESSION)pFileObj->FsContext;

    Log(("VBoxDrvNtClose: pDevExt=%p pFileObj=%p pSession=%p\n", pDevExt, pFileObj, pSession));
    if (pSession)
    {
        supdrvCloseSession(pDevExt, (PSUPDRVSESSION)pFileObj->FsContext);
        pFileObj->FsContext = NULL;
    }

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
    PSUPDRVDEVEXT       pDevExt  = SUPDRVNT_GET_DEVEXT(pDevObj);
    PIO_STACK_LOCATION  pStack   = IoGetCurrentIrpStackLocation(pIrp);
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
    if (   (   ulCmd == SUP_IOCTL_FAST_DO_RAW_RUN
            || ulCmd == SUP_IOCTL_FAST_DO_HM_RUN
            || ulCmd == SUP_IOCTL_FAST_DO_NOP)
        && pSession->fUnrestricted == true)
    {
#ifdef VBOX_WITH_VMMR0_DISABLE_PREEMPTION
        int rc = supdrvIOCtlFast(ulCmd, (unsigned)(uintptr_t)pIrp->UserBuffer /* VMCPU id */, pDevExt, pSession);
#else
        /* Raise the IRQL to DISPATCH_LEVEL to prevent Windows from rescheduling us to another CPU/core. */
        Assert(KeGetCurrentIrql() <= DISPATCH_LEVEL);
        KIRQL oldIrql;
        KeRaiseIrql(DISPATCH_LEVEL, &oldIrql);
        int rc = supdrvIOCtlFast(ulCmd, (unsigned)(uintptr_t)pIrp->UserBuffer /* VMCPU id */, pDevExt, pSession);
        KeLowerIrql(oldIrql);
#endif

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
    Log2(("VBoxDrvNtDeviceControlSlow(%p,%p): ioctl=%#x pBuf=%p cbIn=%#x cbOut=%#x pSession=%p\n",
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
                Log2(("VBoxDrvNtDeviceControlSlow: returns %#x cbOut=%d rc=%#x\n", rcNt, cbOut, rc));
            }
            else
            {
                Log(("VBoxDrvNtDeviceControlSlow: Mismatching sizes (%#x) - Hdr=%#lx/%#lx Irp=%#lx/%#lx!\n",
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
            Log(("VBoxDrvNtDeviceControlSlow: not buffered request (%#x) - not supported\n",
                 pStack->Parameters.DeviceIoControl.IoControlCode));
            rcNt = STATUS_NOT_SUPPORTED;
        }
    }
#ifdef RT_ARCH_AMD64
    else
    {
        Log(("VBoxDrvNtDeviceControlSlow: WOW64 req - not supported\n"));
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
    PSUPDRVDEVEXT       pDevExt  = SUPDRVNT_GET_DEVEXT(pDevObj);
    PIO_STACK_LOCATION  pStack   = IoGetCurrentIrpStackLocation(pIrp);
    PFILE_OBJECT        pFileObj = pStack ? pStack->FileObject : NULL;
    PSUPDRVSESSION      pSession = pFileObj ? (PSUPDRVSESSION)pFileObj->FsContext : NULL;
    NTSTATUS            rcNt;
    unsigned            cbOut = 0;
    int                 rc = 0;
    Log2(("VBoxDrvNtInternalDeviceControl(%p,%p): ioctl=%#x pBuf=%p cbIn=%#x cbOut=%#x pSession=%p\n",
          pDevExt, pIrp, pStack->Parameters.DeviceIoControl.IoControlCode,
          pIrp->AssociatedIrp.SystemBuffer, pStack->Parameters.DeviceIoControl.InputBufferLength,
          pStack->Parameters.DeviceIoControl.OutputBufferLength, pSession));

    /* Verify that it's a buffered CTL. */
    if ((pStack->Parameters.DeviceIoControl.IoControlCode & 0x3) == METHOD_BUFFERED)
    {
        /* Verify the pDevExt in the session. */
        if (  pStack->Parameters.DeviceIoControl.IoControlCode != SUPDRV_IDC_REQ_CONNECT
            ? VALID_PTR(pSession) && pSession->pDevExt == pDevExt
            : !pSession
           )
        {
            /* Verify that the size in the request header is correct. */
            PSUPDRVIDCREQHDR pHdr = (PSUPDRVIDCREQHDR)pIrp->AssociatedIrp.SystemBuffer;
            if (    pStack->Parameters.DeviceIoControl.InputBufferLength >= sizeof(*pHdr)
                &&  pStack->Parameters.DeviceIoControl.InputBufferLength  == pHdr->cb
                &&  pStack->Parameters.DeviceIoControl.OutputBufferLength == pHdr->cb)
            {
                /*
                 * Call the generic code.
                 *
                 * Note! Connect and disconnect requires some extra attention
                 *       in order to get the session handling right.
                 */
                if (pStack->Parameters.DeviceIoControl.IoControlCode == SUPDRV_IDC_REQ_DISCONNECT)
                    pFileObj->FsContext = NULL;

                rc = supdrvIDC(pStack->Parameters.DeviceIoControl.IoControlCode, pDevExt, pSession, pHdr);
                if (!rc)
                {
                    if (pStack->Parameters.DeviceIoControl.IoControlCode == SUPDRV_IDC_REQ_CONNECT)
                        pFileObj->FsContext = ((PSUPDRVIDCREQCONNECT)pHdr)->u.Out.pSession;

                    rcNt = STATUS_SUCCESS;
                    cbOut = pHdr->cb;
                }
                else
                {
                    rcNt = STATUS_INVALID_PARAMETER;
                    if (pStack->Parameters.DeviceIoControl.IoControlCode == SUPDRV_IDC_REQ_DISCONNECT)
                        pFileObj->FsContext = pSession;
                }
                Log2(("VBoxDrvNtInternalDeviceControl: returns %#x/rc=%#x\n", rcNt, rc));
            }
            else
            {
                Log(("VBoxDrvNtInternalDeviceControl: Mismatching sizes (%#x) - Hdr=%#lx Irp=%#lx/%#lx!\n",
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
        Log(("VBoxDrvNtInternalDeviceControl: not buffered request (%#x) - not supported\n",
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
    Log(("VBoxDrvNtNotSupportedStub\n"));
    NOREF(pDevObj);

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

    Log(("VBoxPowerDispatchCallback: %x %x\n", pArgument1, pArgument2));

    /* Power change imminent? */
    if ((unsigned)pArgument1 == PO_CB_SYSTEM_STATE_LOCK)
    {
        if ((unsigned)pArgument2 == 0)
            Log(("VBoxPowerDispatchCallback: about to go into suspend mode!\n"));
        else
            Log(("VBoxPowerDispatchCallback: resumed!\n"));

        /* Inform any clients that have registered themselves with IPRT. */
        RTPowerSignalEvent(((unsigned)pArgument2 == 0) ? RTPOWEREVENT_SUSPEND : RTPOWEREVENT_RESUME);
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


#define MY_SystemLoadGdiDriverInSystemSpaceInformation  54
#define MY_SystemUnloadGdiDriverInformation             27

typedef struct MYSYSTEMGDIDRIVERINFO
{
    UNICODE_STRING  Name;                   /**< In:  image file name. */
    PVOID           ImageAddress;           /**< Out: the load address. */
    PVOID           SectionPointer;         /**< Out: section object. */
    PVOID           EntryPointer;           /**< Out: entry point address. */
    PVOID           ExportSectionPointer;   /**< Out: export directory/section. */
    ULONG           ImageLength;            /**< Out: SizeOfImage. */
} MYSYSTEMGDIDRIVERINFO;

extern "C" __declspec(dllimport) NTSTATUS NTAPI ZwSetSystemInformation(ULONG, PVOID, ULONG);

int  VBOXCALL   supdrvOSLdrOpen(PSUPDRVDEVEXT pDevExt, PSUPDRVLDRIMAGE pImage, const char *pszFilename)
{
    pImage->pvNtSectionObj = NULL;
    pImage->hMemLock = NIL_RTR0MEMOBJ;

#ifdef VBOX_WITHOUT_NATIVE_R0_LOADER
# ifndef RT_ARCH_X86
#  error "VBOX_WITHOUT_NATIVE_R0_LOADER is only safe on x86."
# endif
    NOREF(pDevExt); NOREF(pszFilename); NOREF(pImage);
    return VERR_NOT_SUPPORTED;

#else
    /*
     * Convert the filename from DOS UTF-8 to NT UTF-16.
     */
    size_t cwcFilename;
    int rc = RTStrCalcUtf16LenEx(pszFilename, RTSTR_MAX, &cwcFilename);
    if (RT_FAILURE(rc))
        return rc;

    PRTUTF16 pwcsFilename = (PRTUTF16)RTMemTmpAlloc((4 + cwcFilename + 1) * sizeof(RTUTF16));
    if (!pwcsFilename)
        return VERR_NO_TMP_MEMORY;

    pwcsFilename[0] = '\\';
    pwcsFilename[1] = '?';
    pwcsFilename[2] = '?';
    pwcsFilename[3] = '\\';
    PRTUTF16 pwcsTmp = &pwcsFilename[4];
    rc = RTStrToUtf16Ex(pszFilename, RTSTR_MAX, &pwcsTmp, cwcFilename + 1, NULL);
    if (RT_SUCCESS(rc))
    {
        /*
         * Try load it.
         */
        MYSYSTEMGDIDRIVERINFO Info;
        RtlInitUnicodeString(&Info.Name, pwcsFilename);
        Info.ImageAddress           = NULL;
        Info.SectionPointer         = NULL;
        Info.EntryPointer           = NULL;
        Info.ExportSectionPointer   = NULL;
        Info.ImageLength            = 0;

        NTSTATUS rcNt = ZwSetSystemInformation(MY_SystemLoadGdiDriverInSystemSpaceInformation, &Info, sizeof(Info));
        if (NT_SUCCESS(rcNt))
        {
            pImage->pvImage = Info.ImageAddress;
            pImage->pvNtSectionObj = Info.SectionPointer;
            Log(("ImageAddress=%p SectionPointer=%p ImageLength=%#x cbImageBits=%#x rcNt=%#x '%ls'\n",
                 Info.ImageAddress, Info.SectionPointer, Info.ImageLength, pImage->cbImageBits, rcNt, Info.Name.Buffer));
# ifdef DEBUG_bird
            SUPR0Printf("ImageAddress=%p SectionPointer=%p ImageLength=%#x cbImageBits=%#x rcNt=%#x '%ws'\n",
                        Info.ImageAddress, Info.SectionPointer, Info.ImageLength, pImage->cbImageBits, rcNt, Info.Name.Buffer);
# endif
            if (pImage->cbImageBits == Info.ImageLength)
            {
                /*
                 * Lock down the entire image, just to be on the safe side.
                 */
                rc = RTR0MemObjLockKernel(&pImage->hMemLock, pImage->pvImage, pImage->cbImageBits, RTMEM_PROT_READ);
                if (RT_FAILURE(rc))
                {
                    pImage->hMemLock = NIL_RTR0MEMOBJ;
                    supdrvOSLdrUnload(pDevExt, pImage);
                }
            }
            else
            {
                supdrvOSLdrUnload(pDevExt, pImage);
                rc = VERR_LDR_MISMATCH_NATIVE;
            }
        }
        else
        {
            Log(("rcNt=%#x '%ls'\n", rcNt, pwcsFilename));
            SUPR0Printf("VBoxDrv: rcNt=%x '%ws'\n", rcNt, pwcsFilename);
            switch (rcNt)
            {
                case /* 0xc0000003 */ STATUS_INVALID_INFO_CLASS:
# ifdef RT_ARCH_AMD64
                    /* Unwind will crash and BSOD, so no fallback here! */
                    rc = VERR_NOT_IMPLEMENTED;
# else
                    /*
                     * Use the old way of loading the modules.
                     *
                     * Note! We do *NOT* try class 26 because it will probably
                     *       not work correctly on terminal servers and such.
                     */
                    rc = VERR_NOT_SUPPORTED;
# endif
                    break;
                case /* 0xc0000034 */ STATUS_OBJECT_NAME_NOT_FOUND:
                    rc = VERR_MODULE_NOT_FOUND;
                    break;
                case /* 0xC0000263 */ STATUS_DRIVER_ENTRYPOINT_NOT_FOUND:
                    rc = VERR_LDR_IMPORTED_SYMBOL_NOT_FOUND;
                    break;
                case    0xC0000428 /* STATUS_INVALID_IMAGE_HASH */ :
                    rc = VERR_LDR_IMAGE_HASH;
                    break;
                case    0xC000010E /* STATUS_IMAGE_ALREADY_LOADED */ :
                    Log(("WARNING: see @bugref{4853} for cause of this failure on Windows 7 x64\n"));
                    rc = VERR_ALREADY_LOADED;
                    break;
                default:
                    rc = VERR_LDR_GENERAL_FAILURE;
                    break;
            }

            pImage->pvNtSectionObj = NULL;
        }
    }

    RTMemTmpFree(pwcsFilename);
    NOREF(pDevExt);
    return rc;
#endif
}


void VBOXCALL   supdrvOSLdrNotifyOpened(PSUPDRVDEVEXT pDevExt, PSUPDRVLDRIMAGE pImage)
{
    NOREF(pDevExt); NOREF(pImage);
}


int  VBOXCALL   supdrvOSLdrValidatePointer(PSUPDRVDEVEXT pDevExt, PSUPDRVLDRIMAGE pImage, void *pv, const uint8_t *pbImageBits)
{
    NOREF(pDevExt); NOREF(pImage); NOREF(pv); NOREF(pbImageBits);
    return VINF_SUCCESS;
}


/**
 * memcmp + log.
 *
 * @returns Same as memcmp.
 * @param   pImage          The image.
 * @param   pbImageBits     The image bits ring-3 uploads.
 * @param   uRva            The RVA to start comparing at.
 * @param   cb              The number of bytes to compare.
 */
static int supdrvNtCompare(PSUPDRVLDRIMAGE pImage, const uint8_t *pbImageBits, uint32_t uRva, uint32_t cb)
{
    int iDiff = memcmp((uint8_t const *)pImage->pvImage + uRva, pbImageBits + uRva, cb);
    if (iDiff)
    {
        uint32_t        cbLeft = cb;
        const uint8_t  *pbNativeBits = (const uint8_t *)pImage->pvImage;
        for (size_t off = uRva; cbLeft > 0; off++, cbLeft--)
            if (pbNativeBits[off] != pbImageBits[off])
            {
                char szBytes[128];
                RTStrPrintf(szBytes, sizeof(szBytes), "native: %.*Rhxs  our: %.*Rhxs",
                            RT_MIN(12, cbLeft), &pbNativeBits[off],
                            RT_MIN(12, cbLeft), &pbImageBits[off]);
                SUPR0Printf("VBoxDrv: Mismatch at %#x of %s: %s\n", off, pImage->szName, szBytes);
                break;
            }
    }
    return iDiff;
}

int  VBOXCALL   supdrvOSLdrLoad(PSUPDRVDEVEXT pDevExt, PSUPDRVLDRIMAGE pImage, const uint8_t *pbImageBits, PSUPLDRLOAD pReq)
{
    NOREF(pDevExt); NOREF(pReq);
    if (pImage->pvNtSectionObj)
    {
        /*
         * Usually, the entire image matches exactly.
         */
        if (!memcmp(pImage->pvImage, pbImageBits, pImage->cbImageBits))
            return VINF_SUCCESS;

        /*
         * However, on Windows Server 2003 (sp2 x86) both import thunk tables
         * are fixed up and we typically get a mismatch in the INIT section.
         *
         * So, lets see if everything matches when excluding the
         * OriginalFirstThunk tables.  To make life simpler, set the max number
         * of imports to 16 and just record and sort the locations that needs
         * to be excluded from the comparison.
         */
        IMAGE_NT_HEADERS const *pNtHdrs;
        pNtHdrs = (IMAGE_NT_HEADERS const *)(pbImageBits
                                             + (  *(uint16_t *)pbImageBits == IMAGE_DOS_SIGNATURE
                                                ? ((IMAGE_DOS_HEADER const *)pbImageBits)->e_lfanew
                                                : 0));
        if (    pNtHdrs->Signature == IMAGE_NT_SIGNATURE
            &&  pNtHdrs->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR_MAGIC
            &&  pNtHdrs->OptionalHeader.NumberOfRvaAndSizes > IMAGE_DIRECTORY_ENTRY_IMPORT
            &&  pNtHdrs->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size >= sizeof(IMAGE_IMPORT_DESCRIPTOR)
            &&  pNtHdrs->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress > sizeof(IMAGE_NT_HEADERS)
            &&  pNtHdrs->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress < pImage->cbImageBits
            )
        {
            struct MyRegion
            {
                uint32_t uRva;
                uint32_t cb;
            }           aExcludeRgns[16];
            unsigned    cExcludeRgns = 0;
            uint32_t    cImpsLeft    = pNtHdrs->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size
                                     / sizeof(IMAGE_IMPORT_DESCRIPTOR);
            IMAGE_IMPORT_DESCRIPTOR const *pImp;
            pImp = (IMAGE_IMPORT_DESCRIPTOR const *)(pbImageBits
                                                     + pNtHdrs->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
            while (   cImpsLeft-- > 0
                   && cExcludeRgns < RT_ELEMENTS(aExcludeRgns))
            {
                uint32_t uRvaThunk = pImp->OriginalFirstThunk;
                if (    uRvaThunk >  sizeof(IMAGE_NT_HEADERS)
                    &&  uRvaThunk <= pImage->cbImageBits - sizeof(IMAGE_THUNK_DATA)
                    &&  uRvaThunk != pImp->FirstThunk)
                {
                    /* Find the size of the thunk table. */
                    IMAGE_THUNK_DATA const *paThunk    = (IMAGE_THUNK_DATA const *)(pbImageBits + uRvaThunk);
                    uint32_t                cMaxThunks = (pImage->cbImageBits - uRvaThunk) / sizeof(IMAGE_THUNK_DATA);
                    uint32_t                cThunks    = 0;
                    while (cThunks < cMaxThunks && paThunk[cThunks].u1.Function != 0)
                        cThunks++;

                    /* Ordered table insert. */
                    unsigned i = 0;
                    for (; i < cExcludeRgns; i++)
                        if (uRvaThunk < aExcludeRgns[i].uRva)
                            break;
                    if (i != cExcludeRgns)
                        memmove(&aExcludeRgns[i + 1], &aExcludeRgns[i], (cExcludeRgns - i) * sizeof(aExcludeRgns[0]));
                    aExcludeRgns[i].uRva = uRvaThunk;
                    aExcludeRgns[i].cb   = cThunks * sizeof(IMAGE_THUNK_DATA);
                    cExcludeRgns++;
                }

                /* advance */
                pImp++;
            }

            /*
             * Ok, do the comparison.
             */
            int         iDiff    = 0;
            uint32_t    uRvaNext = 0;
            for (unsigned i = 0; !iDiff && i < cExcludeRgns; i++)
            {
                if (uRvaNext < aExcludeRgns[i].uRva)
                    iDiff = supdrvNtCompare(pImage, pbImageBits, uRvaNext, aExcludeRgns[i].uRva - uRvaNext);
                uRvaNext = aExcludeRgns[i].uRva + aExcludeRgns[i].cb;
            }
            if (!iDiff && uRvaNext < pImage->cbImageBits)
                iDiff = supdrvNtCompare(pImage, pbImageBits, uRvaNext, pImage->cbImageBits - uRvaNext);
            if (!iDiff)
                return VINF_SUCCESS;
        }
        else
            supdrvNtCompare(pImage, pbImageBits, 0, pImage->cbImageBits);
        return VERR_LDR_MISMATCH_NATIVE;
    }
    return VERR_INTERNAL_ERROR_4;
}


void VBOXCALL   supdrvOSLdrUnload(PSUPDRVDEVEXT pDevExt, PSUPDRVLDRIMAGE pImage)
{
    if (pImage->pvNtSectionObj)
    {
        if (pImage->hMemLock != NIL_RTR0MEMOBJ)
        {
            RTR0MemObjFree(pImage->hMemLock, false /*fFreeMappings*/);
            pImage->hMemLock = NIL_RTR0MEMOBJ;
        }

        NTSTATUS rcNt = ZwSetSystemInformation(MY_SystemUnloadGdiDriverInformation,
                                               &pImage->pvNtSectionObj, sizeof(pImage->pvNtSectionObj));
        if (rcNt != STATUS_SUCCESS)
            SUPR0Printf("VBoxDrv: failed to unload '%s', rcNt=%#x\n", pImage->szName, rcNt);
        pImage->pvNtSectionObj = NULL;
    }
    NOREF(pDevExt);
}


/**
 * Converts an IPRT error code to an nt status code.
 *
 * @returns corresponding nt status code.
 * @param   rc      IPRT error status code.
 */
static NTSTATUS     VBoxDrvNtErr2NtStatus(int rc)
{
    switch (rc)
    {
        case VINF_SUCCESS:                  return STATUS_SUCCESS;
        case VERR_GENERAL_FAILURE:          return STATUS_NOT_SUPPORTED;
        case VERR_INVALID_PARAMETER:        return STATUS_INVALID_PARAMETER;
        case VERR_INVALID_MAGIC:            return STATUS_UNKNOWN_REVISION;
        case VERR_INVALID_HANDLE:           return STATUS_INVALID_HANDLE;
        case VERR_INVALID_POINTER:          return STATUS_INVALID_ADDRESS;
        case VERR_LOCK_FAILED:              return STATUS_NOT_LOCKED;
        case VERR_ALREADY_LOADED:           return STATUS_IMAGE_ALREADY_LOADED;
        case VERR_PERMISSION_DENIED:        return STATUS_ACCESS_DENIED;
        case VERR_VERSION_MISMATCH:         return STATUS_REVISION_MISMATCH;
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
