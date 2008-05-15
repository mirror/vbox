/* $Id$ */
/** @file
 * VirtualBox Support Driver - Windows NT specific parts.
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
#include "SUPDRV.h"
#include <excpt.h>
#include <iprt/assert.h>
#include <iprt/process.h>


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
static NTSTATUS _stdcall   VBoxDrvNtNotSupportedStub(PDEVICE_OBJECT pDevObj, PIRP pIrp);
static NTSTATUS            VBoxDrvNtErr2NtStatus(int rc);
static NTSTATUS            VBoxDrvNtGipInit(PSUPDRVDEVEXT pDevExt);
static void                VBoxDrvNtGipTerm(PSUPDRVDEVEXT pDevExt);
static void     _stdcall   VBoxDrvNtGipTimer(IN PKDPC pDpc, IN PVOID pvUser, IN PVOID SystemArgument1, IN PVOID SystemArgument2);
static void     _stdcall   VBoxDrvNtGipPerCpuDpc(IN PKDPC pDpc, IN PVOID pvUser, IN PVOID SystemArgument1, IN PVOID SystemArgument2);


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
            uint64_t  u64DiffCores;

            /*
             * Initialize the device extension.
             */
            PSUPDRVDEVEXT pDevExt = (PSUPDRVDEVEXT)pDevObj->DeviceExtension;
            memset(pDevExt, 0, sizeof(*pDevExt));

            int vrc = supdrvInitDevExt(pDevExt);
            if (!vrc)
            {
                /* Make sure the tsc is consistent across cpus/cores. */
                pDevExt->fForceAsyncTsc = supdrvDetermineAsyncTsc(&u64DiffCores);
                dprintf(("supdrvDetermineAsyncTsc: fAsync=%d u64DiffCores=%u.\n", pDevExt->fForceAsyncTsc, (uint32_t)u64DiffCores));

                /*
                 * Inititalize the GIP.
                 */
                rc = VBoxDrvNtGipInit(pDevExt);
                if (NT_SUCCESS(rc))
                {
                    /*
                     * Setup the driver entry points in pDrvObj.
                     */
                    pDrvObj->DriverUnload                           = VBoxDrvNtUnload;
                    pDrvObj->MajorFunction[IRP_MJ_CREATE]           = VBoxDrvNtCreate;
                    pDrvObj->MajorFunction[IRP_MJ_CLOSE]            = VBoxDrvNtClose;
                    pDrvObj->MajorFunction[IRP_MJ_DEVICE_CONTROL]   = VBoxDrvNtDeviceControl;
                    pDrvObj->MajorFunction[IRP_MJ_READ]             = VBoxDrvNtNotSupportedStub;
                    pDrvObj->MajorFunction[IRP_MJ_WRITE]            = VBoxDrvNtNotSupportedStub;
                    /* more? */
                    dprintf(("VBoxDrv::DriverEntry   returning STATUS_SUCCESS\n"));
                    return STATUS_SUCCESS;
                }
                dprintf(("VBoxDrvNtGipInit failed with rc=%#x!\n", rc));

                supdrvDeleteDevExt(pDevExt);
            }
            else
            {
                dprintf(("supdrvInitDevExit failed with vrc=%d!\n", vrc));
                rc = VBoxDrvNtErr2NtStatus(vrc);
            }

            IoDeleteSymbolicLink(&DosName);
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
    VBoxDrvNtGipTerm(pDevExt);
    supdrvDeleteDevExt(pDevExt);
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
    int rc = supdrvCreateSession(pDevExt, &pSession);
    if (!rc)
    {
        pSession->Uid       = NIL_RTUID;
        pSession->Gid       = NIL_RTGID;
        pSession->Process   = RTProcSelf();
        pSession->R0Process = RTR0ProcHandleSelf();
        pFileObj->FsContext = pSession;
    }

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
     */
    ULONG ulCmd = pStack->Parameters.DeviceIoControl.IoControlCode;
    if (    ulCmd == SUP_IOCTL_FAST_DO_RAW_RUN
        ||  ulCmd == SUP_IOCTL_FAST_DO_HWACC_RUN
        ||  ulCmd == SUP_IOCTL_FAST_DO_NOP)
    {
        KIRQL oldIrql;
        int   rc;

	 	/* Raise the IRQL to DISPATCH_LEVEl to prevent Windows from rescheduling us to another CPU/core. */ 
        Assert(KeGetCurrentIrql() <= DISPATCH_LEVEL);
        KeRaiseIrql(DISPATCH_LEVEL, &oldIrql);        
        rc = supdrvIOCtlFast(ulCmd, pDevExt, pSession);
        KeLowerIrql(oldIrql);

        /* Complete the I/O request. */
        NTSTATUS rcNt = pIrp->IoStatus.Status = STATUS_SUCCESS;
        pIrp->IoStatus.Information = sizeof(rc);
        __try
        {
            *(int *)pIrp->UserBuffer = rc;
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            rcNt = pIrp->IoStatus.Status = GetExceptionCode();
            dprintf(("VBoxSupDrvDeviceContorl: Exception Code %#x\n", rcNt));
        }
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
 * Gets the monotone timestamp (nano seconds).
 * @returns NanoTS.
 */
static inline uint64_t supdrvOSMonotime(void)
{
    return (uint64_t)KeQueryInterruptTime() * 100;
}


/**
 * Initializes the GIP.
 *
 * @returns NT status code.
 * @param   pDevExt     Instance data. GIP stuff may be updated.
 */
static NTSTATUS VBoxDrvNtGipInit(PSUPDRVDEVEXT pDevExt)
{
    dprintf2(("VBoxSupDrvTermGip:\n"));

    /*
     * Try allocate the memory.
     * Make sure it's below 4GB for 32-bit GC support
     */
    NTSTATUS rc;
    PHYSICAL_ADDRESS Phys;
    Phys.HighPart = 0;
    Phys.LowPart = ~0;
    PSUPGLOBALINFOPAGE pGip = (PSUPGLOBALINFOPAGE)MmAllocateContiguousMemory(PAGE_SIZE, Phys);
    if (pGip)
    {
        if (!((uintptr_t)pGip & (PAGE_SIZE - 1)))
        {
            pDevExt->pGipMdl = IoAllocateMdl(pGip, PAGE_SIZE, FALSE, FALSE, NULL);
            if (pDevExt->pGipMdl)
            {
                MmBuildMdlForNonPagedPool(pDevExt->pGipMdl);

                /*
                 * Figure the timer interval and frequency.
                 * It turns out trying 1023Hz doesn't work. So, we'll set the max Hz at 128 for now.
                 */
                ExSetTimerResolution(156250, TRUE);
                ULONG ulClockIntervalActual = ExSetTimerResolution(0, FALSE);
                ULONG ulClockInterval = RT_MAX(ulClockIntervalActual, 78125); /* 1/128 */
                ULONG ulClockFreq = 10000000 / ulClockInterval;
                pDevExt->ulGipTimerInterval = ulClockInterval / 10000; /* ms */

                /*
                 * Call common initialization routine.
                 */
                Phys = MmGetPhysicalAddress(pGip); /* could perhaps use the Mdl, not that it looks much better */
                supdrvGipInit(pDevExt, pGip, (RTHCPHYS)Phys.QuadPart, supdrvOSMonotime(), ulClockFreq);

                /*
                 * Initialize the timer.
                 */
                KeInitializeTimerEx(&pDevExt->GipTimer, SynchronizationTimer);
                KeInitializeDpc(&pDevExt->GipDpc, VBoxDrvNtGipTimer, pDevExt);

                /*
                 * Initialize the DPCs we're using to update the per-cpu GIP data.
                 */
                for (unsigned i = 0; i < RT_ELEMENTS(pDevExt->aGipCpuDpcs); i++)
                {
                    KeInitializeDpc(&pDevExt->aGipCpuDpcs[i], VBoxDrvNtGipPerCpuDpc, pGip);
                    KeSetImportanceDpc(&pDevExt->aGipCpuDpcs[i], HighImportance);
                    KeSetTargetProcessorDpc(&pDevExt->aGipCpuDpcs[i], i);
                }

                dprintf(("VBoxDrvNtGipInit: ulClockFreq=%ld ulClockInterval=%ld ulClockIntervalActual=%ld Phys=%x%08x\n",
                         ulClockFreq, ulClockInterval, ulClockIntervalActual, Phys.HighPart, Phys.LowPart));
                return STATUS_SUCCESS;
            }

            dprintf(("VBoxSupDrvInitGip: IoAllocateMdl failed for %p/PAGE_SIZE\n", pGip));
            rc = STATUS_NO_MEMORY;
        }
        else
        {
            dprintf(("VBoxSupDrvInitGip: GIP memory is not page aligned! pGip=%p\n", pGip));
            rc = STATUS_INVALID_ADDRESS;
        }
        MmFreeContiguousMemory(pGip);
    }
    else
    {
        dprintf(("VBoxSupDrvInitGip: no cont memory.\n"));
        rc = STATUS_NO_MEMORY;
    }
    return rc;
}


/**
 * Terminates the GIP.
 *
 * @returns negative errno.
 * @param   pDevExt     Instance data. GIP stuff may be updated.
 */
static void VBoxDrvNtGipTerm(PSUPDRVDEVEXT pDevExt)
{
    dprintf(("VBoxSupDrvTermGip:\n"));
    PSUPGLOBALINFOPAGE pGip;

    /*
     * Cancel the timer and wait on DPCs if it was still pending.
     */
    if (KeCancelTimer(&pDevExt->GipTimer))
    {
        UNICODE_STRING  RoutineName;
        RtlInitUnicodeString(&RoutineName, L"KeFlushQueuedDpcs");
        VOID (*pfnKeFlushQueuedDpcs)(VOID) = (VOID (*)(VOID))MmGetSystemRoutineAddress(&RoutineName);
        if (pfnKeFlushQueuedDpcs)
        {
            /* KeFlushQueuedDpcs must be run at IRQL PASSIVE_LEVEL */
            AssertMsg(KeGetCurrentIrql() == PASSIVE_LEVEL, ("%d != %d (PASSIVE_LEVEL)\n", KeGetCurrentIrql(), PASSIVE_LEVEL));
            pfnKeFlushQueuedDpcs();
        }
    }

    /*
     * Uninitialize the content.
     */
    pGip = pDevExt->pGip;
    pDevExt->pGip = NULL;
    if (pGip)
    {
        supdrvGipTerm(pGip);

        /*
         * Free the page.
         */
        if (pDevExt->pGipMdl)
        {
            IoFreeMdl(pDevExt->pGipMdl);
            pDevExt->pGipMdl = NULL;
        }
        MmFreeContiguousMemory(pGip);
    }
}


/**
 * Timer callback function.
 * The pvUser parameter is the pDevExt pointer.
 */
static void _stdcall VBoxDrvNtGipTimer(IN PKDPC pDpc, IN PVOID pvUser, IN PVOID SystemArgument1, IN PVOID SystemArgument2)
{
    PSUPDRVDEVEXT pDevExt = (PSUPDRVDEVEXT)pvUser;
    PSUPGLOBALINFOPAGE pGip = pDevExt->pGip;
    if (pGip)
    {
        if (pGip->u32Mode != SUPGIPMODE_ASYNC_TSC)
            supdrvGipUpdate(pGip, supdrvOSMonotime());
        else
        {
            KIRQL oldIrql;

            /* KeQueryActiveProcessors must be executed at IRQL < DISPATCH_LEVEL */
            Assert(KeGetCurrentIrql() < DISPATCH_LEVEL);
            KAFFINITY Mask = KeQueryActiveProcessors();

            /* Raise the IRQL to DISPATCH_LEVEL so we can't be rescheduled to another cpu */
            KeRaiseIrql(DISPATCH_LEVEL, &oldIrql);

            /*
             * We cannot do other than assume a 1:1 relationship between the
             * affinity mask and the process despite the warnings in the docs.
             * If someone knows a better way to get this done, please let bird know.
             */
            unsigned iSelf = KeGetCurrentProcessorNumber();

            for (unsigned i = 0; i < RT_ELEMENTS(pDevExt->aGipCpuDpcs); i++)
            {
                if (    i != iSelf
                    &&  (Mask & RT_BIT_64(i)))
                    KeInsertQueueDpc(&pDevExt->aGipCpuDpcs[i], 0, 0);
            }

            /* Run the normal update. */
            supdrvGipUpdate(pGip, supdrvOSMonotime());

            KeLowerIrql(oldIrql);
        }
    }
}


/**
 * Per cpu callback callback function.
 * The pvUser parameter is the pGip pointer.
 */
static void _stdcall VBoxDrvNtGipPerCpuDpc(IN PKDPC pDpc, IN PVOID pvUser, IN PVOID SystemArgument1, IN PVOID SystemArgument2)
{
    PSUPGLOBALINFOPAGE pGip = (PSUPGLOBALINFOPAGE)pvUser;
    supdrvGipUpdatePerCpu(pGip, supdrvOSMonotime(), ASMGetApicId());
}


/**
 * Maps the GIP into user space.
 *
 * @returns negative errno.
 * @param   pDevExt     Instance data.
 */
int VBOXCALL supdrvOSGipMap(PSUPDRVDEVEXT pDevExt, PSUPGLOBALINFOPAGE *ppGip)
{
    dprintf2(("supdrvOSGipMap: ppGip=%p (pDevExt->pGipMdl=%p)\n", ppGip, pDevExt->pGipMdl));

    /*
     * Map into user space.
     */
    int rc = 0;
    void *pv = NULL;
    __try
    {
        *ppGip = (PSUPGLOBALINFOPAGE)MmMapLockedPagesSpecifyCache(pDevExt->pGipMdl, UserMode, MmCached, NULL, FALSE, NormalPagePriority);
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        NTSTATUS rcNt = GetExceptionCode();
        dprintf(("supdrvOsGipMap: Exception Code %#x\n", rcNt));
        rc = SUPDRV_ERR_LOCK_FAILED;
    }

    dprintf2(("supdrvOSGipMap: returns %d, *ppGip=%p\n", rc, *ppGip));
    return 0;
}


/**
 * Maps the GIP into user space.
 *
 * @returns negative errno.
 * @param   pDevExt     Instance data.
 */
int VBOXCALL supdrvOSGipUnmap(PSUPDRVDEVEXT pDevExt, PSUPGLOBALINFOPAGE pGip)
{
    dprintf2(("supdrvOSGipUnmap: pGip=%p (pGipMdl=%p)\n", pGip, pDevExt->pGipMdl));

    int rc = 0;
    __try
    {
        MmUnmapLockedPages((void *)pGip, pDevExt->pGipMdl);
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        NTSTATUS rcNt = GetExceptionCode();
        dprintf(("supdrvOSGipUnmap: Exception Code %#x\n", rcNt));
        rc = SUPDRV_ERR_GENERAL_FAILURE;
    }
    dprintf2(("supdrvOSGipUnmap: returns %d\n", rc));
    return rc;
}


/**
 * Resumes the GIP updating.
 *
 * @param   pDevExt     Instance data.
 */
void  VBOXCALL  supdrvOSGipResume(PSUPDRVDEVEXT pDevExt)
{
    dprintf2(("supdrvOSGipResume:\n"));
    LARGE_INTEGER DueTime;
    DueTime.QuadPart = -10000; /* 1ms, relative */
    KeSetTimerEx(&pDevExt->GipTimer, DueTime, pDevExt->ulGipTimerInterval, &pDevExt->GipDpc);
}


/**
 * Suspends the GIP updating.
 *
 * @param   pDevExt     Instance data.
 */
void  VBOXCALL  supdrvOSGipSuspend(PSUPDRVDEVEXT pDevExt)
{
    dprintf2(("supdrvOSGipSuspend:\n"));
    KeCancelTimer(&pDevExt->GipTimer);
#ifdef RT_ARCH_AMD64
    ExSetTimerResolution(0, FALSE);
#endif
}


/**
 * Get the current CPU count.
 * @returns Number of cpus.
 */
unsigned VBOXCALL supdrvOSGetCPUCount(void)
{
    /* KeQueryActiveProcessors must be executed at IRQL < DISPATCH_LEVEL */
    Assert(KeGetCurrentIrql() < DISPATCH_LEVEL);
    KAFFINITY Mask = KeQueryActiveProcessors();
    unsigned cCpus = 0;
    unsigned iBit;
    for (iBit = 0; iBit < sizeof(Mask) * 8; iBit++)
        if (Mask & RT_BIT_64(iBit))
            cCpus++;
    if (cCpus == 0) /* paranoia */
        cCpus = 1;
    return cCpus;
}


/**
 * Force async tsc mode (stub).
 */
bool VBOXCALL  supdrvOSGetForcedAsyncTscMode(PSUPDRVDEVEXT pDevExt)
{
    return pDevExt->fForceAsyncTsc != 0;
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


/** Runtime assert implementation for Native Win32 Ring-0. */
RTDECL(void) AssertMsg1(const char *pszExpr, unsigned uLine, const char *pszFile, const char *pszFunction)
{
    DbgPrint("\n!!Assertion Failed!!\n"
             "Expression: %s\n"
             "Location  : %s(%d) %s\n",
             pszExpr, pszFile, uLine, pszFunction);
}

int VBOXCALL mymemcmp(const void *pv1, const void *pv2, size_t cb)
{
    const uint8_t *pb1 = (const uint8_t *)pv1;
    const uint8_t *pb2 = (const uint8_t *)pv2;
    for (; cb > 0; cb--, pb1++, pb2++)
        if (*pb1 != *pb2)
            return *pb1 - *pb2;
    return 0;
}

