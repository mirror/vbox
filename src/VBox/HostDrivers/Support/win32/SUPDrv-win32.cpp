/** @file
 *
 * VBox host drivers - Ring-0 support drivers - Win32 host:
 * Win32 host driver code
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
#if 0 //def __AMD64__
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
static void     _stdcall   VBoxSupDrvUnload(PDRIVER_OBJECT pDrvObj);
static NTSTATUS _stdcall   VBoxSupDrvCreate(PDEVICE_OBJECT pDevObj, PIRP pIrp);
static NTSTATUS _stdcall   VBoxSupDrvClose(PDEVICE_OBJECT pDevObj, PIRP pIrp);
static NTSTATUS _stdcall   VBoxSupDrvDeviceControl(PDEVICE_OBJECT pDevObj, PIRP pIrp);
static int                 VBoxSupDrvDeviceControlSlow(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PIRP pIrp, PIO_STACK_LOCATION pStack);
static NTSTATUS _stdcall   VBoxSupDrvNotSupportedStub(PDEVICE_OBJECT pDevObj, PIRP pIrp);
static NTSTATUS            VBoxSupDrvErr2NtStatus(int rc);
static NTSTATUS            VBoxSupDrvGipInit(PSUPDRVDEVEXT pDevExt);
static void                VBoxSupDrvGipTerm(PSUPDRVDEVEXT pDevExt);
static void     _stdcall   VBoxSupDrvGipTimer(IN PKDPC pDpc, IN PVOID pvUser, IN PVOID SystemArgument1, IN PVOID SystemArgument2);
static void     _stdcall   VBoxSupDrvGipPerCpuDpc(IN PKDPC pDpc, IN PVOID pvUser, IN PVOID SystemArgument1, IN PVOID SystemArgument2);


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
            /*
             * Initialize the device extension.
             */
            PSUPDRVDEVEXT pDevExt = (PSUPDRVDEVEXT)pDevObj->DeviceExtension;
            memset(pDevExt, 0, sizeof(*pDevExt));
            int vrc = supdrvInitDevExt(pDevExt);
            if (!vrc)
            {
                /*
                 * Inititalize the GIP.
                 */
                rc = VBoxSupDrvGipInit(pDevExt);
                if (NT_SUCCESS(rc))
                {
                    /*
                     * Setup the driver entry points in pDrvObj.
                     */
                    pDrvObj->DriverUnload                           = VBoxSupDrvUnload;
                    pDrvObj->MajorFunction[IRP_MJ_CREATE]           = VBoxSupDrvCreate;
                    pDrvObj->MajorFunction[IRP_MJ_CLOSE]            = VBoxSupDrvClose;
                    pDrvObj->MajorFunction[IRP_MJ_DEVICE_CONTROL]   = VBoxSupDrvDeviceControl;
                    pDrvObj->MajorFunction[IRP_MJ_READ]             = VBoxSupDrvNotSupportedStub;
                    pDrvObj->MajorFunction[IRP_MJ_WRITE]            = VBoxSupDrvNotSupportedStub;
                    /* more? */
                    dprintf(("VBoxDrv::DriverEntry   returning STATUS_SUCCESS\n"));
                    return STATUS_SUCCESS;
                }
                dprintf(("VBoxSupDrvGipInit failed with rc=%#x!\n", rc));

                supdrvDeleteDevExt(pDevExt);
            }
            else
            {
                dprintf(("supdrvInitDevExit failed with vrc=%d!\n", vrc));
                rc = VBoxSupDrvErr2NtStatus(vrc);
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
void _stdcall VBoxSupDrvUnload(PDRIVER_OBJECT pDrvObj)
{
    dprintf(("VBoxSupDrvUnload\n"));
    PSUPDRVDEVEXT       pDevExt = (PSUPDRVDEVEXT)pDrvObj->DeviceObject->DeviceExtension;

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
    VBoxSupDrvGipTerm(pDevExt);
    supdrvDeleteDevExt(pDevExt);
    IoDeleteDevice(pDrvObj->DeviceObject);
}


/**
 * Create (i.e. Open) file entry point.
 *
 * @param   pDevObj     Device object.
 * @param   pIrp        Request packet.
 */
NTSTATUS _stdcall VBoxSupDrvCreate(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    dprintf(("VBoxSupDrvCreate\n"));
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

    NTSTATUS    rcNt = pIrp->IoStatus.Status = VBoxSupDrvErr2NtStatus(rc);
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
NTSTATUS _stdcall VBoxSupDrvClose(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    PSUPDRVDEVEXT       pDevExt = (PSUPDRVDEVEXT)pDevObj->DeviceExtension;
    PIO_STACK_LOCATION  pStack = IoGetCurrentIrpStackLocation(pIrp);
    PFILE_OBJECT        pFileObj = pStack->FileObject;
    dprintf(("VBoxSupDrvClose: pDevExt=%p pFileObj=%p pSession=%p\n",
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
NTSTATUS _stdcall VBoxSupDrvDeviceControl(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    PSUPDRVDEVEXT       pDevExt = (PSUPDRVDEVEXT)pDevObj->DeviceExtension;
    PIO_STACK_LOCATION  pStack = IoGetCurrentIrpStackLocation(pIrp);
    PSUPDRVSESSION      pSession = (PSUPDRVSESSION)pStack->FileObject->FsContext;

#ifdef VBOX_WITHOUT_IDT_PATCHING
    /*
     * Deal with the two high-speed IOCtl that takes it's arguments from
     * the session and iCmd, and only returns a VBox status code.
     */
    ULONG ulCmd = pStack->Parameters.DeviceIoControl.IoControlCode;
    if (    ulCmd == SUP_IOCTL_FAST_DO_RAW_RUN
        ||  ulCmd == SUP_IOCTL_FAST_DO_HWACC_RUN
        ||  ulCmd == SUP_IOCTL_FAST_DO_NOP)
    {
        int rc = supdrvIOCtlFast(ulCmd, pDevExt, pSession);

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
#endif /* VBOX_WITHOUT_IDT_PATCHING */

    return VBoxSupDrvDeviceControlSlow(pDevExt, pSession, pIrp, pStack);
}


/**
 * Worker for VBoxSupDrvDeviceControl that takes the slow IOCtl functions.
 *
 * @returns NT status code.
 *
 * @param   pDevObj     Device object.
 * @param   pSession    The session.
 * @param   pIrp        Request packet.
 * @param   pStack      The stack location containing the DeviceControl parameters.
 */
static int VBoxSupDrvDeviceControlSlow(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PIRP pIrp, PIO_STACK_LOCATION pStack)
{
    NTSTATUS    rcNt = STATUS_NOT_SUPPORTED;
    unsigned    cbOut = 0;
    int         rc = 0;
    dprintf2(("VBoxSupDrvDeviceControlSlow(%p,%p): ioctl=%#x pBuf=%p cbIn=%#x cbOut=%#x pSession=%p\n",
             pDevObj, pIrp, pStack->Parameters.DeviceIoControl.IoControlCode,
             pBuf, pStack->Parameters.DeviceIoControl.InputBufferLength,
             pStack->Parameters.DeviceIoControl.OutputBufferLength, pSession));

#ifdef __AMD64__
    /* Don't allow 32-bit processes to do any I/O controls. */
    if (!IoIs32bitProcess(pIrp))
#endif
    {
        /* Verify that it's a buffered CTL. */
        if ((pStack->Parameters.DeviceIoControl.IoControlCode & 0x3) == METHOD_BUFFERED)
        {
            char *pBuf = (char *)pIrp->AssociatedIrp.SystemBuffer;

            /*
             * Do the job.
             */
            rc = supdrvIOCtl(pStack->Parameters.DeviceIoControl.IoControlCode, pDevExt, pSession,
                             pBuf, pStack->Parameters.DeviceIoControl.InputBufferLength,
                             pBuf, pStack->Parameters.DeviceIoControl.OutputBufferLength,
                             &cbOut);
            rcNt = VBoxSupDrvErr2NtStatus(rc);

            /* sanity check. */
            AssertMsg(cbOut <= pStack->Parameters.DeviceIoControl.OutputBufferLength,
                      ("cbOut is too large! cbOut=%d max=%d! ioctl=%#x\n",
                       cbOut, pStack->Parameters.DeviceIoControl.OutputBufferLength,
                       pStack->Parameters.DeviceIoControl.IoControlCode));
            if (cbOut > pStack->Parameters.DeviceIoControl.OutputBufferLength)
                cbOut = pStack->Parameters.DeviceIoControl.OutputBufferLength;
            dprintf2(("VBoxSupDrvDeviceControlSlow: returns %#x cbOut=%d rc=%#x\n", rcNt, cbOut, rc));
        }
        else
            dprintf(("VBoxSupDrvDeviceControlSlow: not buffered request (%#x) - not supported\n",
                     pStack->Parameters.DeviceIoControl.IoControlCode));
    }
#ifdef __AMD64__
    else
        dprintf(("VBoxSupDrvDeviceControlSlow: WOW64 req - not supported\n"));
#endif

    /* complete the request. */
    pIrp->IoStatus.Status = rcNt;
    pIrp->IoStatus.Information = NT_SUCCESS(rcNt) ? cbOut : rc; /* does this rc passing actually work?!? */
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
NTSTATUS _stdcall VBoxSupDrvNotSupportedStub(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    dprintf(("VBoxSupDrvNotSupportedStub\n"));
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
 * OS Specific code for locking down memory.
 *
 * @returns 0 on success.
 * @returns SUPDRV_ERR_* on failure.
 * @param   pMem        Pointer to memory.
 *                      This is not linked in anywhere.
 * @param   paPages     Array which should be filled with the address of the physical pages.
 */
int  VBOXCALL   supdrvOSLockMemOne(PSUPDRVMEMREF pMem, PSUPPAGE paPages)
{
    /* paranoia */
    if (!pMem->cb)
    {
        AssertMsgFailed(("Fool! No memory to lock!\n"));
        return SUPDRV_ERR_INVALID_PARAM;
    }
    Assert(RT_ALIGN(pMem->cb, PAGE_SIZE) == pMem->cb);

    /*
     * Calc the number of MDLs we need to allocate.
     */
    unsigned cMdls = pMem->cb / MAX_LOCK_MEM_SIZE;
    if ((pMem->cb % MAX_LOCK_MEM_SIZE) > 0)
        cMdls++;

    /*
     * Allocate memory for the MDL pointer array.
     */
    pMem->u.locked.papMdl = (PMDL *)ExAllocatePoolWithTag(NonPagedPool, sizeof(*pMem->u.locked.papMdl) * cMdls, SUPDRV_NT_POOL_TAG);
    if (!pMem->u.locked.papMdl)
    {
        AssertMsgFailed(("shit, couldn't allocated %d bytes for the mdl pointer array!\n", sizeof(*pMem->u.locked.papMdl) * cMdls));
        return SUPDRV_ERR_NO_MEMORY;
    }

    /*
     * Loop locking down the sub parts of the memory.
     */
    PSUPPAGE    pPage   = paPages;
    unsigned    cbTotal = 0;
    uint8_t    *pu8     = (uint8_t *)pMem->pvR3;
    for (unsigned i = 0; i < cMdls; i++)
    {
        /*
         * Calc the number of bytes to lock this time.
         */
        unsigned cbCur = pMem->cb - cbTotal;
        if (cbCur > MAX_LOCK_MEM_SIZE)
            cbCur = MAX_LOCK_MEM_SIZE;

        if (cbCur == 0)
            AssertMsgFailed(("cbCur: 0!\n"));

        /*
         * Allocate pMdl.
         */
        PMDL pMdl = IoAllocateMdl(pu8, cbCur, FALSE, FALSE, NULL);
        if (!pMdl)
        {
            AssertMsgFailed(("Ops! IoAllocateMdl failed for pu8=%p and cb=%d\n", pu8, cbCur));
            return SUPDRV_ERR_NO_MEMORY;
        }

        /*
         * Lock the pages.
         */
        NTSTATUS rc = STATUS_SUCCESS;
        __try
        {
            MmProbeAndLockPages(pMdl, UserMode, IoModifyAccess);
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            rc = GetExceptionCode();
            dprintf(("supdrvOSLockMemOne: Exception Code %#x\n", rc));
        }

        if (!NT_SUCCESS(rc))
        {
            /*
             * Cleanup and fail.
             */
            IoFreeMdl(pMdl);
            while (i-- > 0)
            {
                MmUnlockPages(pMem->u.locked.papMdl[i]);
                IoFreeMdl(pMem->u.locked.papMdl[i]);
            }
            ExFreePool(pMem->u.locked.papMdl);
            pMem->u.locked.papMdl = NULL;
            return SUPDRV_ERR_LOCK_FAILED;
        }

        /*
         * Add MDL to array and update the pages.
         */
        pMem->u.locked.papMdl[i] = pMdl;

        const uintptr_t *pauPFNs = (uintptr_t *)(pMdl + 1);  /* ASSUMES ULONG_PTR == uintptr_t, NTDDK4 doesn't have ULONG_PTR. */
        for (unsigned iPage = 0, cPages = cbCur >> PAGE_SHIFT; iPage < cPages; iPage++)
        {
            pPage->Phys = (RTHCPHYS)pauPFNs[iPage] << PAGE_SHIFT;
            pPage->uReserved = 0;
            pPage++;
        }

        /* next */
        cbTotal += cbCur;
        pu8     += cbCur;
    }

    /*
     * Finish structure and return succesfully.
     */
    pMem->u.locked.cMdls = cMdls;

    dprintf2(("supdrvOSLockMemOne: pvR3=%p cb=%d cMdls=%d\n",
              pMem->pvR3, pMem->cb, cMdls));
    return 0;
}


/**
 * Unlocks the memory pointed to by pv.
 *
 * @param   pv  Memory to unlock.
 * @param   cb  Size of the memory (debug).
 */
void VBOXCALL supdrvOSUnlockMemOne(PSUPDRVMEMREF pMem)
{
    dprintf2(("supdrvOSUnlockMemOne: pvR3=%p cb=%d cMdl=%p papMdl=%p\n",
              pMem->pvR3, pMem->cb, pMem->u.locked.cMdls, pMem->u.locked.papMdl));

    for (unsigned i = 0; i < pMem->u.locked.cMdls; i++)
    {
        MmUnlockPages(pMem->u.locked.papMdl[i]);
        IoFreeMdl(pMem->u.locked.papMdl[i]);
    }

    ExFreePool(pMem->u.locked.papMdl);
    pMem->u.locked.papMdl = NULL;
}


/**
 * OS Specific code for allocating page aligned memory with continuous fixed
 * physical paged backing.
 *
 * @returns 0 on success.
 * @returns SUPDRV_ERR_* on failure.
 * @param   pMem    Memory reference record of the memory to be allocated.
 *                  (This is not linked in anywhere.)
 * @param   ppvR0       Where to store the virtual address of the ring-0 mapping. (optional)
 * @param   ppvR3       Where to store the virtual address of the ring-3 mapping.
 * @param   pHCPhys     Where to store the physical address.
 */
int VBOXCALL supdrvOSContAllocOne(PSUPDRVMEMREF pMem, PRTR0PTR ppvR0, PRTR3PTR ppvR3, PRTHCPHYS pHCPhys)
{
    Assert(ppvR3);
    Assert(pHCPhys);

    /*
     * Try allocate the memory.
     */
    PHYSICAL_ADDRESS Phys;
    Phys.HighPart = 0;
    Phys.LowPart = ~0;
    unsigned cbAligned = RT_ALIGN(pMem->cb, PAGE_SIZE);
    pMem->pvR0 = MmAllocateContiguousMemory(cbAligned, Phys);
    if (!pMem->pvR0)
        return SUPDRV_ERR_NO_MEMORY;

    /*
     * Map into user space.
     */
    int rc = SUPDRV_ERR_NO_MEMORY;
    pMem->u.cont.pMdl = IoAllocateMdl(pMem->pvR0, cbAligned, FALSE, FALSE, NULL);
    if (pMem->u.cont.pMdl)
    {
        MmBuildMdlForNonPagedPool(pMem->u.cont.pMdl);
        __try
        {
            pMem->pvR3 = (RTR3PTR)MmMapLockedPages(pMem->u.cont.pMdl, UserMode);
            if (pMem->pvR3)
            {
                /*
                 * Done, setup pMem and return values.
                 */
#ifdef __AMD64__
                 MmProtectMdlSystemAddress(pMem->u.cont.pMdl, PAGE_EXECUTE_READWRITE);
#endif
                *ppvR3 = pMem->pvR3;
                if (ppvR0)
                    *ppvR0 = pMem->pvR0;
                const uintptr_t *pauPFNs = (const uintptr_t *)(pMem->u.cont.pMdl + 1); /* ASSUMES ULONG_PTR == uintptr_t, NTDDK4 doesn't have ULONG_PTR. */
                *pHCPhys = (RTHCPHYS)pauPFNs[0] << PAGE_SHIFT;
                dprintf2(("supdrvOSContAllocOne: pvR0=%p pvR3=%p cb=%d pMdl=%p *pHCPhys=%VHp\n",
                          pMem->pvR0, pMem->pvR3, pMem->cb, pMem->u.mem.pMdl, *pHCPhys));
                return 0;
            }
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            NTSTATUS rc = GetExceptionCode();
            dprintf(("supdrvOSContAllocOne: Exception Code %#x\n", rc));
        }
        IoFreeMdl(pMem->u.cont.pMdl);
        rc = SUPDRV_ERR_LOCK_FAILED;
    }
    MmFreeContiguousMemory(pMem->pvR0);
    pMem->pvR0 = NULL;
    return rc;
}


/**
 * Frees contiguous memory.
 *
 * @param   pMem    Memory reference record of the memory to be freed.
 */
void VBOXCALL supdrvOSContFreeOne(PSUPDRVMEMREF pMem)
{
    __try
    {
        dprintf2(("supdrvOSContFreeOne: pvR0=%p pvR3=%p cb=%d pMdl=%p\n",
                 pMem->pvR0, pMem->pvR3, pMem->cb, pMem->u.cont.pMdl));
        if (pMem->pvR3)
        {
            MmUnmapLockedPages((void *)pMem->pvR3, pMem->u.cont.pMdl);
            dprintf2(("MmUnmapLockedPages ok!\n"));
            pMem->pvR3 = NULL;
        }

        IoFreeMdl(pMem->u.cont.pMdl);
        dprintf2(("IoFreeMdl ok!\n"));
        pMem->u.cont.pMdl = NULL;

        MmFreeContiguousMemory(pMem->pvR0);
        dprintf2(("MmFreeContiguousMemory ok!\n"));
        pMem->pvR0 = NULL;
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        NTSTATUS rc = GetExceptionCode();
        dprintf(("supdrvOSContFreeOne: Exception Code %#x\n", rc));
    }
}


/**
 * Allocates memory which mapped into both kernel and user space.
 * The returned memory is page aligned and so is the allocation.
 *
 * @returns 0 on success.
 * @returns SUPDRV_ERR_* on failure.
 * @param   pMem        Memory reference record of the memory to be allocated.
 *                      (This is not linked in anywhere.)
 * @param   ppvR0       Where to store the address of the Ring-0 mapping.
 * @param   ppvR3       Where to store the address of the Ring-3 mapping.
 */
int  VBOXCALL   supdrvOSMemAllocOne(PSUPDRVMEMREF pMem, PRTR0PTR ppvR0, PRTR3PTR ppvR3)
{
    Assert(ppvR0);
    Assert(ppvR3);

    /*
     * Try allocate the memory.
     */
    unsigned cbAligned = RT_ALIGN(RT_MAX(pMem->cb, PAGE_SIZE * 2), PAGE_SIZE);
    pMem->pvR0 = ExAllocatePoolWithTag(NonPagedPool, cbAligned, SUPDRV_NT_POOL_TAG);
    if (!pMem->pvR0)
        return SUPDRV_ERR_NO_MEMORY;

    /*
     * Map into user space.
     */
    int rc = SUPDRV_ERR_NO_MEMORY;
    pMem->u.mem.pMdl = IoAllocateMdl(pMem->pvR0, cbAligned, FALSE, FALSE, NULL);
    if (pMem->u.mem.pMdl)
    {
        MmBuildMdlForNonPagedPool(pMem->u.mem.pMdl);
        __try
        {
            pMem->pvR3 = (RTR3PTR)MmMapLockedPages(pMem->u.mem.pMdl, UserMode);
            if (pMem->pvR3)
            {
                /*
                 * Done, setup pMem and return values.
                 */
                *ppvR3 = pMem->pvR3;
                *ppvR0 = pMem->pvR0;
                dprintf2(("supdrvOSContAllocOne: pvR0=%p pvR3=%p cb=%d pMdl=%p\n",
                          pMem->pvR0, pMem->pvR3, pMem->cb, pMem->u.mem.pMdl));
                return 0;
            }
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            NTSTATUS rc = GetExceptionCode();
            dprintf(("supdrvOSContAllocOne: Exception Code %#x\n", rc));
        }
        rc = SUPDRV_ERR_LOCK_FAILED;

        IoFreeMdl(pMem->u.mem.pMdl);
        pMem->u.mem.pMdl = NULL;
        pMem->pvR3 = NULL;
    }

    MmFreeContiguousMemory(pMem->pvR0);
    pMem->pvR0 = NULL;
    return rc;
}


/**
 * Get the physical addresses of the pages in the allocation.
 * This is called while inside bundle the spinlock.
 *
 * @param   pMem        Memory reference record of the memory.
 * @param   paPages     Where to store the page addresses.
 */
void VBOXCALL   supdrvOSMemGetPages(PSUPDRVMEMREF pMem, PSUPPAGE paPages)
{
    const unsigned      cPages = RT_ALIGN(pMem->cb, PAGE_SIZE) >> PAGE_SHIFT;
    const uintptr_t    *pauPFNs = (const uintptr_t *)(pMem->u.mem.pMdl + 1); /* ASSUMES ULONG_PTR == uintptr_t, NTDDK doesn't have ULONG_PTR. */
    for (unsigned iPage = 0; iPage < cPages; iPage++)
    {
        paPages[iPage].Phys = (RTHCPHYS)pauPFNs[iPage] << PAGE_SHIFT;
        paPages[iPage].uReserved = 0;
    }
}


/**
 * Frees memory allocated by supdrvOSMemAllocOne().
 *
 * @param   pMem        Memory reference record of the memory to be free.
 */
void VBOXCALL   supdrvOSMemFreeOne(PSUPDRVMEMREF pMem)
{
    __try
    {
        dprintf2(("supdrvOSContFreeOne: pvR0=%p pvR3=%p cb=%d pMdl=%p\n",
                 pMem->pvR0, pMem->pvR3, pMem->cb, pMem->u.mem.pMdl));
        if (pMem->pvR3)
        {
            MmUnmapLockedPages((void *)pMem->pvR3, pMem->u.mem.pMdl);
            pMem->pvR3 = NULL;
            dprintf2(("MmUnmapLockedPages ok!\n"));
        }

        IoFreeMdl(pMem->u.mem.pMdl);
        pMem->u.mem.pMdl = NULL;
        dprintf2(("IoFreeMdl ok!\n"));

        ExFreePool(pMem->pvR0);
        pMem->pvR0 = NULL;
        dprintf2(("MmFreeContiguousMemory ok!\n"));
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        NTSTATUS rc = GetExceptionCode();
        dprintf(("supdrvOSContFreeOne: Exception Code %#x\n", rc));
    }
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
static NTSTATUS VBoxSupDrvGipInit(PSUPDRVDEVEXT pDevExt)
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
                KeInitializeDpc(&pDevExt->GipDpc, VBoxSupDrvGipTimer, pDevExt);

                /*
                 * Initialize the DPCs we're using to update the per-cpu GIP data.
                 * (Not sure if we need to be this careful with KeSetTargetProcessorDpc...)
                 */
                UNICODE_STRING  RoutineName;
                RtlInitUnicodeString(&RoutineName, L"KeSetTargetProcessorDpc");
                VOID (*pfnKeSetTargetProcessorDpc)(IN PRKDPC, IN CCHAR) = (VOID (*)(IN PRKDPC, IN CCHAR))MmGetSystemRoutineAddress(&RoutineName);

                for (unsigned i = 0; i < RT_ELEMENTS(pDevExt->aGipCpuDpcs); i++)
                {
                    KeInitializeDpc(&pDevExt->aGipCpuDpcs[i], VBoxSupDrvGipPerCpuDpc, pGip);
                    KeSetImportanceDpc(&pDevExt->aGipCpuDpcs[i], HighImportance);
                    if (pfnKeSetTargetProcessorDpc)
                        pfnKeSetTargetProcessorDpc(&pDevExt->aGipCpuDpcs[i], i);
                }

                dprintf(("VBoxSupDrvGipInit: ulClockFreq=%ld ulClockInterval=%ld ulClockIntervalActual=%ld Phys=%x%08x\n",
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
static void VBoxSupDrvGipTerm(PSUPDRVDEVEXT pDevExt)
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
            pfnKeFlushQueuedDpcs();
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
static void _stdcall VBoxSupDrvGipTimer(IN PKDPC pDpc, IN PVOID pvUser, IN PVOID SystemArgument1, IN PVOID SystemArgument2)
{
    PSUPDRVDEVEXT pDevExt = (PSUPDRVDEVEXT)pvUser;
    PSUPGLOBALINFOPAGE pGip = pDevExt->pGip;
    if (pGip)
    {
        if (pGip->u32Mode != SUPGIPMODE_ASYNC_TSC)
            supdrvGipUpdate(pGip, supdrvOSMonotime());
        else
        {
            RTCCUINTREG xFL = ASMGetFlags();
            ASMIntDisable();

            /*
             * We cannot do other than assume a 1:1 relation ship between the
             * affinity mask and the process despite the warnings in the docs.
             * If someone knows a better way to get this done, please let bird know.
             */
            unsigned iSelf = KeGetCurrentProcessorNumber();
            KAFFINITY Mask = KeQueryActiveProcessors();

            for (unsigned i = 0; i < RT_ELEMENTS(pDevExt->aGipCpuDpcs); i++)
            {
                if (    i != iSelf
                    &&  (Mask & RT_BIT_64(i)))
                    KeInsertQueueDpc(&pDevExt->aGipCpuDpcs[i], 0, 0);
            }

            /* Run the normal update. */
            supdrvGipUpdate(pGip, supdrvOSMonotime());

            ASMSetFlags(xFL);
        }
    }
}


/**
 * Per cpu callback callback function.
 * The pvUser parameter is the pGip pointer.
 */
static void _stdcall VBoxSupDrvGipPerCpuDpc(IN PKDPC pDpc, IN PVOID pvUser, IN PVOID SystemArgument1, IN PVOID SystemArgument2)
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
int VBOXCALL supdrvOSGipMap(PSUPDRVDEVEXT pDevExt, PCSUPGLOBALINFOPAGE *ppGip)
{
    dprintf2(("supdrvOSGipMap: ppGip=%p (pDevExt->pGipMdl=%p)\n", ppGip, pDevExt->pGipMdl));

    /*
     * Map into user space.
     */
    int rc = 0;
    void *pv = NULL;
    __try
    {
        *ppGip = (PCSUPGLOBALINFOPAGE)MmMapLockedPages(pDevExt->pGipMdl, UserMode);
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
int VBOXCALL supdrvOSGipUnmap(PSUPDRVDEVEXT pDevExt, PCSUPGLOBALINFOPAGE pGip)
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
#ifdef __AMD64__
    ExSetTimerResolution(0, FALSE);
#endif
}


/**
 * Allocate small amounts of memory which is does not have the NX bit set.
 *
 * @returns Pointer to the allocated memory
 * @returns NULL if out of memory.
 * @param   cb   Size of the memory block.
 */
void *VBOXCALL  supdrvOSExecAlloc(size_t cb)
{
#if 0 //def __AMD64__
    cb = RT_ALIGN_Z(cb, PAGE_SIZE);
    void *pv = ExAllocatePoolWithTag(NonPagedPool, cb, SUPDRV_NT_POOL_TAG);
    if (pv)
    {
        /*
         * Create a kernel mapping which we make PAGE_EXECUTE_READWRITE using
         * the MmProtectMdlSystemAddress API.
         */
        int rc = SUPDRV_ERR_NO_MEMORY;
        PMDL pMdl = IoAllocateMdl(pv, cb, FALSE, FALSE, NULL);
        if (pMdl)
        {
            MmBuildMdlForNonPagedPool(pMdl);
            __try
            {
                void *pvMapping = MmMapLockedPages(pMdl, KernelMode);
                if (pvMapping)
                {
                    NTSTATUS rc = MmProtectMdlSystemAddress(pMdl, PAGE_EXECUTE_READWRITE);
                    if (NT_SUCCESS(rc))
                    {
                        /*
                         * Create tracking structure and insert it into the list.
                         */


                        return pvMapping;
                    }

                    MmUnmapLockedPages(pvMapping, pMdl);
                }
            }
            __except(EXCEPTION_EXECUTE_HANDLER)
            {
                NTSTATUS rc = GetExceptionCode();
                dprintf(("supdrvOSExecAlloc: Exception Code %#x\n", rc));
            }
            IoFreeMdl(pMem->u.mem.pMdl);
        }
        ExFreePool(pv);
    }
    dprintf2(("supdrvOSExecAlloc(%d): returns NULL\n", cb));
    return NULL;
#else
    void *pv = ExAllocatePoolWithTag(NonPagedPool, cb, SUPDRV_NT_POOL_TAG);
    dprintf2(("supdrvOSExecAlloc(%d): returns %p\n", cb, pv));
    return pv;
#endif
}


/**
 * Get the current CPU count.
 * @returns Number of cpus.
 */
unsigned VBOXCALL supdrvOSGetCPUCount(void)
{
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
bool VBOXCALL  supdrvOSGetForcedAsyncTscMode(void)
{
    return false;
}


/**
 * Converts a supdrv error code to an nt status code.
 *
 * @returns corresponding nt status code.
 * @param   rc  supdrv error code (SUPDRV_ERR_* defines).
 */
static NTSTATUS     VBoxSupDrvErr2NtStatus(int rc)
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

