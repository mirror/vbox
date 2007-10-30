/** @file
 *
 * VBoxGuest -- VirtualBox Win32 guest support driver
 *
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

// enable backdoor logging
//#define LOG_ENABLED

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "VBoxGuest_Internal.h"
#ifdef TARGET_NT4
#include "NTLegacy.h"
#else
#include "VBoxGuestPnP.h"
#endif
#include "Helper.h"
#include <excpt.h>
#include <VBox/err.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <stdio.h>
#include <VBox/VBoxGuestLib.h>
#include <VBoxGuestInternal.h>

#ifdef TARGET_NT4
/* XP DDK #defines ExFreePool to ExFreePoolWithTag. The latter does not exist on NT4, so... 
 * The same for ExAllocatePool.
 */
#undef ExAllocatePool
#undef ExFreePool
#endif

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
extern "C"
{
static NTSTATUS VBoxGuestAddDevice(PDRIVER_OBJECT pDrvObj, PDEVICE_OBJECT pDevObj);
static void     VBoxGuestUnload(PDRIVER_OBJECT pDrvObj);
static NTSTATUS VBoxGuestCreate(PDEVICE_OBJECT pDevObj, PIRP pIrp);
static NTSTATUS VBoxGuestClose(PDEVICE_OBJECT pDevObj, PIRP pIrp);
static NTSTATUS VBoxGuestDeviceControl(PDEVICE_OBJECT pDevObj, PIRP pIrp);
static NTSTATUS VBoxGuestSystemControl(PDEVICE_OBJECT pDevObj, PIRP pIrp);
static NTSTATUS VBoxGuestShutdown(PDEVICE_OBJECT pDevObj, PIRP pIrp);
static NTSTATUS VBoxGuestNotSupportedStub(PDEVICE_OBJECT pDevObj, PIRP pIrp);
static VOID     vboxWorkerThread(PVOID context);
static VOID     reserveHypervisorMemory(PVBOXGUESTDEVEXT pDevExt);
static VOID     vboxIdleThread(PVOID context);
}


/*******************************************************************************
*   Exported Functions                                                         *
*******************************************************************************/
__BEGIN_DECLS
ULONG DriverEntry(PDRIVER_OBJECT pDrvObj, PUNICODE_STRING pRegPath);
__END_DECLS

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, createThreads)
#pragma alloc_text (PAGE, unreserveHypervisorMemory)
#pragma alloc_text (PAGE, VBoxGuestAddDevice)
#pragma alloc_text (PAGE, VBoxGuestUnload)
#pragma alloc_text (PAGE, VBoxGuestCreate)
#pragma alloc_text (PAGE, VBoxGuestClose)
#pragma alloc_text (PAGE, VBoxGuestDeviceControl)
#pragma alloc_text (PAGE, VBoxGuestShutdown)
#pragma alloc_text (PAGE, VBoxGuestNotSupportedStub)
/* Note: at least the isr handler should be in non-pageable memory! */
/*#pragma alloc_text (PAGE, VBoxGuestDpcHandler)
 #pragma alloc_text (PAGE, VBoxGuestIsrHandler) */
#pragma alloc_text (PAGE, vboxWorkerThread)
#pragma alloc_text (PAGE, reserveHypervisorMemory)
#pragma alloc_text (PAGE, vboxIdleThread)
#endif

winVersion_t winVersion;

/**
 * Driver entry point.
 *
 * @returns appropriate status code.
 * @param   pDrvObj     Pointer to driver object.
 * @param   pRegPath    Registry base path.
 */
ULONG DriverEntry(PDRIVER_OBJECT pDrvObj, PUNICODE_STRING pRegPath)
{
    NTSTATUS rc = STATUS_SUCCESS;

    dprintf(("VBoxGuest::DriverEntry. Driver built: %s %s\n", __DATE__, __TIME__));

    ULONG majorVersion;
    ULONG minorVersion;
    ULONG buildNumber;
    PsGetVersion(&majorVersion, &minorVersion, &buildNumber, NULL);
    dprintf(("VBoxGuest::DriverEntry: running on Windows NT version %d.%d, build %d\n", majorVersion, minorVersion, buildNumber));
    switch (majorVersion)
    {
        case 6:
            winVersion = WINVISTA;
            break;
        case 5:
            switch (minorVersion)
            {
                case 2:
                    winVersion = WIN2K3;
                    break;
                case 1:
                    winVersion = WINXP;
                    break;
                case 0:
                    winVersion = WIN2K;
                    break;
                default:
                    dprintf(("VBoxGuest::DriverEntry: unknown version of Windows, refusing!\n"));
                    return STATUS_DRIVER_UNABLE_TO_LOAD;
            }
            break;
        case 4:
            winVersion = WINNT4;
            break;
        default:
            dprintf(("VBoxGuest::DriverEntry: NT4 required!\n"));
            return STATUS_DRIVER_UNABLE_TO_LOAD;
    }

    /*
     * Setup the driver entry points in pDrvObj.
     */
    pDrvObj->DriverUnload                             = VBoxGuestUnload;
    pDrvObj->MajorFunction[IRP_MJ_CREATE]             = VBoxGuestCreate;
    pDrvObj->MajorFunction[IRP_MJ_CLOSE]              = VBoxGuestClose;
    pDrvObj->MajorFunction[IRP_MJ_DEVICE_CONTROL]     = VBoxGuestDeviceControl;
    pDrvObj->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = VBoxGuestDeviceControl;
    pDrvObj->MajorFunction[IRP_MJ_SHUTDOWN]           = VBoxGuestShutdown;
    pDrvObj->MajorFunction[IRP_MJ_READ]               = VBoxGuestNotSupportedStub;
    pDrvObj->MajorFunction[IRP_MJ_WRITE]              = VBoxGuestNotSupportedStub;
#ifdef TARGET_NT4
    rc = ntCreateDevice(pDrvObj, NULL, pRegPath);
#else
    pDrvObj->MajorFunction[IRP_MJ_PNP]                = VBoxGuestPnP;
    pDrvObj->MajorFunction[IRP_MJ_POWER]              = VBoxGuestPower;
    pDrvObj->MajorFunction[IRP_MJ_SYSTEM_CONTROL]     = VBoxGuestSystemControl;
    pDrvObj->DriverExtension->AddDevice               = (PDRIVER_ADD_DEVICE)VBoxGuestAddDevice;
#endif

    dprintf(("VBoxGuest::DriverEntry returning %#x\n", rc));
    return rc;
}

#ifndef TARGET_NT4
/**
 * Handle request from the Plug & Play subsystem
 *
 * @returns NT status code
 * @param  pDrvObj   Driver object
 * @param  pDevObj   Device object
 */
static NTSTATUS VBoxGuestAddDevice(PDRIVER_OBJECT pDrvObj, PDEVICE_OBJECT pDevObj)
{
    NTSTATUS rc;
    dprintf(("VBoxGuest::VBoxGuestAddDevice\n"));

    /*
     * Create device.
     */
    PDEVICE_OBJECT deviceObject = NULL;
    UNICODE_STRING  devName;
    RtlInitUnicodeString(&devName, VBOXGUEST_DEVICE_NAME_NT);
    rc = IoCreateDevice(pDrvObj, sizeof(VBOXGUESTDEVEXT), &devName, FILE_DEVICE_UNKNOWN, 0, FALSE, &deviceObject);
    if (!NT_SUCCESS(rc))
    {
        dprintf(("VBoxGuest::VBoxGuestAddDevice: IoCreateDevice failed with rc=%#x!\n", rc));
        return rc;
    }
    UNICODE_STRING win32Name;
    RtlInitUnicodeString(&win32Name, VBOXGUEST_DEVICE_NAME_DOS);
    rc = IoCreateSymbolicLink(&win32Name, &devName);
    if (!NT_SUCCESS(rc))
    {
        dprintf(("VBoxGuest::VBoxGuestAddDevice: IoCreateSymbolicLink failed with rc=%#x!\n", rc));
        IoDeleteDevice(deviceObject);
        return rc;
    }

    /*
     * Setup the device extension.
     */
    PVBOXGUESTDEVEXT pDevExt = (PVBOXGUESTDEVEXT)deviceObject->DeviceExtension;
    RtlZeroMemory(pDevExt, sizeof(VBOXGUESTDEVEXT));

    pDevExt->deviceObject = deviceObject;
    pDevExt->devState = STOPPED;

    pDevExt->nextLowerDriver = IoAttachDeviceToDeviceStack(deviceObject, pDevObj);
    if (pDevExt->nextLowerDriver == NULL)
    {
        dprintf(("VBoxGuest::VBoxGuestAddDevice: IoAttachDeviceToDeviceStack did not give a nextLowerDrive\n"));
        IoDeleteSymbolicLink(&win32Name);
        IoDeleteDevice(deviceObject);
        return STATUS_DEVICE_NOT_CONNECTED;
    }

    // driver is ready now
    deviceObject->Flags &= ~DO_DEVICE_INITIALIZING;

    dprintf(("VBoxGuest::VBoxGuestAddDevice: returning with rc = 0x%x\n", rc));
    return rc;
}
#endif


/**
 * Unload the driver.
 *
 * @param   pDrvObj     Driver object.
 */
void VBoxGuestUnload(PDRIVER_OBJECT pDrvObj)
{
    dprintf(("VBoxGuest::VBoxGuestUnload\n"));
#ifdef TARGET_NT4
    PVBOXGUESTDEVEXT pDevExt = (PVBOXGUESTDEVEXT)pDrvObj->DeviceObject->DeviceExtension;
    unreserveHypervisorMemory(pDevExt);
    if (pDevExt->workerThread)
    {
        dprintf(("VBoxGuest::VBoxGuestUnload: waiting for the worker thread to terminate...\n"));
        pDevExt->stopThread = TRUE;
        KeSetEvent(&pDevExt->workerThreadRequest, 0, FALSE);
        KeWaitForSingleObject(pDevExt->workerThread,
                              Executive, KernelMode, FALSE, NULL);
        dprintf(("VBoxGuest::VBoxGuestUnload: returned from KeWaitForSingleObject for worker thread\n"));
    }
    if (pDevExt->idleThread)
    {
        dprintf(("VBoxGuest::VBoxGuestUnload: waiting for the idle thread to terminate...\n"));
        pDevExt->stopThread = TRUE;
        KeWaitForSingleObject(pDevExt->idleThread,
                              Executive, KernelMode, FALSE, NULL);
        dprintf(("VBoxGuest::VBoxGuestUnload: returned from KeWaitForSingleObject for idle thread\n"));
    }

    hlpVBoxUnmapVMMDevMemory (pDevExt);

    VBoxCleanupMemBalloon(pDevExt);

    /*
     * I don't think it's possible to unload a driver which processes have
     * opened, at least we'll blindly assume that here.
     */
    UNICODE_STRING win32Name;
    RtlInitUnicodeString(&win32Name, VBOXGUEST_DEVICE_NAME_DOS);
    NTSTATUS rc = IoDeleteSymbolicLink(&win32Name);
    IoDeleteDevice(pDrvObj->DeviceObject);
#endif
    dprintf(("VBoxGuest::VBoxGuestUnload: returning\n"));
}


/**
 * Create (i.e. Open) file entry point.
 *
 * @param   pDevObj     Device object.
 * @param   pIrp        Request packet.
 */
NTSTATUS VBoxGuestCreate(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    dprintf(("VBoxGuest::VBoxGuestCreate\n"));

    PIO_STACK_LOCATION  pStack = IoGetCurrentIrpStackLocation(pIrp);
    PFILE_OBJECT        pFileObj = pStack->FileObject;
    PVBOXGUESTDEVEXT    pDevExt = (PVBOXGUESTDEVEXT)pDevObj->DeviceExtension;

    /*
     * We are not remotely similar to a directory...
     * (But this is possible.)
     */
    if (pStack->Parameters.Create.Options & FILE_DIRECTORY_FILE)
    {
        dprintf(("VBoxGuest::VBoxGuestCreate: we're not a directory!\n"));
        pIrp->IoStatus.Status       = STATUS_NOT_A_DIRECTORY;
        pIrp->IoStatus.Information  = 0;
        IoCompleteRequest(pIrp, IO_NO_INCREMENT);
        return STATUS_NOT_A_DIRECTORY;
    }

    NTSTATUS    rcNt = pIrp->IoStatus.Status = STATUS_SUCCESS;
    pIrp->IoStatus.Information  = 0;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);

    dprintf(("VBoxGuest::VBoxGuestCreate: returning 0x%x\n", rcNt));
    return rcNt;
}


/**
 * Close file entry point.
 *
 * @param   pDevObj     Device object.
 * @param   pIrp        Request packet.
 */
NTSTATUS VBoxGuestClose(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    dprintf(("VBoxGuest::VBoxGuestClose\n"));

    PVBOXGUESTDEVEXT   pDevExt  = (PVBOXGUESTDEVEXT)pDevObj->DeviceExtension;
    PIO_STACK_LOCATION pStack   = IoGetCurrentIrpStackLocation(pIrp);
    PFILE_OBJECT       pFileObj = pStack->FileObject;
    dprintf(("VBoxGuest::VBoxGuestClose: pDevExt=%p pFileObj=%p pSession=%p\n",
             pDevExt, pFileObj, pFileObj->FsContext));

    pFileObj->FsContext = NULL;
    pIrp->IoStatus.Information = 0;
    pIrp->IoStatus.Status = STATUS_SUCCESS;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}

#ifdef VBOX_HGCM
DECLVBGL(void) VBoxHGCMCallback (VMMDevHGCMRequestHeader *pHeader, void *pvData, uint32_t u32Data)
{
    PVBOXGUESTDEVEXT pDevExt = (PVBOXGUESTDEVEXT)pvData;
    LARGE_INTEGER timeout;

    dprintf(("VBoxHGCMCallback\n"));
        
    /* Possible problem with request completion right between the fu32Flags check and KeWaitForSingleObject 
     * call; introduce a timeout to make sure we don't wait indefinitely.
     */
    timeout.QuadPart  = 250;
    timeout.QuadPart *= -10000;     /* relative in 100ns units */

    while ((pHeader->fu32Flags & VBOX_HGCM_REQ_DONE) == 0)
    {
        NTSTATUS rc = KeWaitForSingleObject (&pDevExt->keventNotification, Executive,
                                             KernelMode, TRUE, &timeout
                                            );
        dprintf(("VBoxHGCMCallback: Wait returned %d fu32Flags=%x\n", rc, pHeader->fu32Flags));

        if (rc == STATUS_TIMEOUT)
            continue;

        if (rc != STATUS_WAIT_0)
        {
            dprintf(("VBoxHGCMCallback: The external event was signalled or the wait timed out or terminated.\n"));
            break;
        }

        dprintf(("VBoxHGCMCallback: fu32Flags = %08X\n", pHeader->fu32Flags));
    }
    
    return;
}

NTSTATUS vboxHGCMVerifyIOBuffers (PIO_STACK_LOCATION pStack, unsigned cb)
{
    if (pStack->Parameters.DeviceIoControl.OutputBufferLength < cb)
    {
        dprintf(("VBoxGuest::vboxHGCMVerifyIOBuffers: OutputBufferLength %d < %d\n",
                 pStack->Parameters.DeviceIoControl.OutputBufferLength, cb));
        return STATUS_INVALID_PARAMETER;
    }

    if (pStack->Parameters.DeviceIoControl.InputBufferLength < cb)
    {
        dprintf(("VBoxGuest::vboxHGCMVerifyIOBuffers: InputBufferLength %d < %d\n",
                 pStack->Parameters.DeviceIoControl.InputBufferLength, cb));
        return STATUS_INVALID_PARAMETER;
    }

    return STATUS_SUCCESS;
}

#endif /* VBOX_HGCM */

static bool
__declspec (naked) __fastcall
TestAndClearEvent (PVBOXGUESTDEVEXT pDevExt, int iBitOffset)
{
    _asm {
        lock btr PVBOXGUESTDEVEXT[ecx].u32Events, edx;
        setc al;
        movzx eax, al;
        ret;
    }
}

static bool IsPowerOfTwo (uint32_t val)
{
    return (val & (val - 1)) == 0;
}

static int __declspec (naked) __fastcall GetMsb32 (uint32_t val)
{
    _asm {
        bsf eax, ecx;
        ret;
    }
}

static bool CtlGuestFilterMask (uint32_t u32OrMask, uint32_t u32NotMask)
{
    bool result = false;
    VMMDevCtlGuestFilterMask *req;
    int rc = VbglGRAlloc ((VMMDevRequestHeader **) &req, sizeof (*req),
                          VMMDevReq_CtlGuestFilterMask);

    if (VBOX_SUCCESS (rc))
    {
        req->u32OrMask = u32OrMask;
        req->u32NotMask = u32NotMask;

        rc = VbglGRPerform (&req->header);
        if (VBOX_FAILURE (rc) || VBOX_FAILURE (req->header.rc))
        {
            dprintf (("VBoxGuest::VBoxGuestDeviceControl: error issuing request to VMMDev! "
                      "rc = %d, VMMDev rc = %Vrc\n", rc, req->header.rc));
        }
        else
        {
            result = true;
        }
        VbglGRFree (&req->header);
    }

    return result;
}

#ifdef VBOX_WITH_MANAGEMENT
static int VBoxGuestSetBalloonSize(PVBOXGUESTDEVEXT pDevExt, uint32_t u32BalloonSize)
{
    VMMDevChangeMemBalloon *req = NULL;
    int rc = VINF_SUCCESS;

    if (u32BalloonSize > pDevExt->MemBalloon.cMaxBalloons)
    {
        AssertMsgFailed(("VBoxGuestSetBalloonSize illegal balloon size %d (max=%d)\n", u32BalloonSize, pDevExt->MemBalloon.cMaxBalloons));
        return VERR_INVALID_PARAMETER;
    }

    if (u32BalloonSize == pDevExt->MemBalloon.cBalloons)
        return VINF_SUCCESS;    /* nothing to do */

    /* Allocate request packet */
    rc = VbglGRAlloc((VMMDevRequestHeader **)&req, RT_OFFSETOF(VMMDevChangeMemBalloon, aPhysPage[VMMDEV_MEMORY_BALLOON_CHUNK_PAGES]), VMMDevReq_ChangeMemBalloon);
    if (VBOX_FAILURE(rc))
        return rc;

    vmmdevInitRequest(&req->header, VMMDevReq_ChangeMemBalloon);

    if (u32BalloonSize > pDevExt->MemBalloon.cBalloons)
    {
        /* inflate */
        for (uint32_t i=pDevExt->MemBalloon.cBalloons;i<u32BalloonSize;i++)
        {
#ifndef TARGET_NT4
            /*
             * Use MmAllocatePagesForMdl to specify the range of physical addresses we wish to use.
             */
            PHYSICAL_ADDRESS Zero;
            PHYSICAL_ADDRESS HighAddr;
            Zero.QuadPart = 0;
            HighAddr.QuadPart = _4G - 1;
            PMDL pMdl = MmAllocatePagesForMdl(Zero, HighAddr, Zero, VMMDEV_MEMORY_BALLOON_CHUNK_SIZE);
            if (pMdl)
            {
                if (MmGetMdlByteCount(pMdl) < VMMDEV_MEMORY_BALLOON_CHUNK_SIZE)
                {
                    MmFreePagesFromMdl(pMdl);
                    ExFreePool(pMdl);
                    rc = VERR_NO_MEMORY;
                    goto end;
                }
            }
#else
            PVOID pvBalloon;
            pvBalloon = ExAllocatePool(PagedPool, VMMDEV_MEMORY_BALLOON_CHUNK_SIZE);
            if (!pvBalloon)
            {
                rc = VERR_NO_MEMORY;
                goto end;
            }

            PMDL pMdl = IoAllocateMdl (pvBalloon, VMMDEV_MEMORY_BALLOON_CHUNK_SIZE, FALSE, FALSE, NULL);
            if (pMdl == NULL)
            {
                rc = VERR_NO_MEMORY;
                ExFreePool(pvBalloon);
                AssertMsgFailed(("IoAllocateMdl %VGv %x failed!!\n", pvBalloon, VMMDEV_MEMORY_BALLOON_CHUNK_SIZE));
                goto end;
            }
            else
            {
                __try {
                    /* Calls to MmProbeAndLockPages must be enclosed in a try/except block. */
                    MmProbeAndLockPages (pMdl, KernelMode, IoModifyAccess);
                } 
                __except(EXCEPTION_EXECUTE_HANDLER) 
                {
                    dprintf(("MmProbeAndLockPages failed!\n"));
                    rc = VERR_NO_MEMORY;
                    IoFreeMdl (pMdl);
                    ExFreePool(pvBalloon);
                    goto end;
                }
            }
#endif

            PPFN_NUMBER pPageDesc = MmGetMdlPfnArray(pMdl);

            /* Copy manually as RTGCPHYS is always 64 bits */
            for (uint32_t j=0;j<VMMDEV_MEMORY_BALLOON_CHUNK_PAGES;j++)
                req->aPhysPage[j] = pPageDesc[j];

            req->header.size = RT_OFFSETOF(VMMDevChangeMemBalloon, aPhysPage[VMMDEV_MEMORY_BALLOON_CHUNK_PAGES]);
            req->cPages      = VMMDEV_MEMORY_BALLOON_CHUNK_PAGES;
            req->fInflate    = true;

            rc = VbglGRPerform(&req->header);
            if (VBOX_FAILURE(rc) || VBOX_FAILURE(req->header.rc))
            {
                dprintf(("VBoxGuest::VBoxGuestSetBalloonSize: error issuing request to VMMDev!"
                         "rc = %d, VMMDev rc = %Vrc\n", rc, req->header.rc));

#ifndef TARGET_NT4
                MmFreePagesFromMdl(pMdl);
                ExFreePool(pMdl);
#else
                IoFreeMdl (pMdl);
                ExFreePool(pvBalloon);
#endif
                goto end;
            }
            else
            {
#ifndef TARGET_NT4
                dprintf(("VBoxGuest::VBoxGuestSetBalloonSize %d MB added chunk at %x\n", i, pMdl));
#else
                dprintf(("VBoxGuest::VBoxGuestSetBalloonSize %d MB added chunk at %x\n", i, pvBalloon));
#endif
                pDevExt->MemBalloon.paMdlMemBalloon[i] = pMdl;
                pDevExt->MemBalloon.cBalloons++;
            }
        }
    }
    else
    {
        /* deflate */
        for (uint32_t _i=pDevExt->MemBalloon.cBalloons;_i>u32BalloonSize;_i--)
        {
            uint32_t index = _i - 1;
            PMDL  pMdl = pDevExt->MemBalloon.paMdlMemBalloon[index];

            Assert(pMdl);
            if (pMdl)
            {
#ifdef TARGET_NT4
                PVOID pvBalloon = MmGetMdlVirtualAddress(pMdl);
#endif

                PPFN_NUMBER pPageDesc = MmGetMdlPfnArray(pMdl);

                /* Copy manually as RTGCPHYS is always 64 bits */
                for (uint32_t j=0;j<VMMDEV_MEMORY_BALLOON_CHUNK_PAGES;j++)
                    req->aPhysPage[j] = pPageDesc[j];

                req->header.size = RT_OFFSETOF(VMMDevChangeMemBalloon, aPhysPage[VMMDEV_MEMORY_BALLOON_CHUNK_PAGES]);
                req->cPages      = VMMDEV_MEMORY_BALLOON_CHUNK_PAGES;
                req->fInflate    = false;

                rc = VbglGRPerform(&req->header);
                if (VBOX_FAILURE(rc) || VBOX_FAILURE(req->header.rc))
                {
                    AssertMsgFailed(("VBoxGuest::VBoxGuestSetBalloonSize: error issuing request to VMMDev! rc = %d, VMMDev rc = %Vrc\n", rc, req->header.rc));
                    break;
                }

                /* Free the ballooned memory */
#ifndef TARGET_NT4
                dprintf(("VBoxGuest::VBoxGuestSetBalloonSize %d MB free chunk at %x\n", index, pMdl));
                MmFreePagesFromMdl(pMdl);
                ExFreePool(pMdl);
#else
                dprintf(("VBoxGuest::VBoxGuestSetBalloonSize %d MB free chunk at %x\n", index, pvBalloon));
                MmUnlockPages (pMdl);
                IoFreeMdl (pMdl);
                ExFreePool(pvBalloon);
#endif

                pDevExt->MemBalloon.paMdlMemBalloon[index] = NULL;
                pDevExt->MemBalloon.cBalloons--;
            }
        }
    }
    Assert(pDevExt->MemBalloon.cBalloons <= pDevExt->MemBalloon.cMaxBalloons);

end:
    VbglGRFree(&req->header);
    return rc;
}

static int VBoxGuestQueryMemoryBalloon(PVBOXGUESTDEVEXT pDevExt, ULONG *pMemBalloonSize)
{
    /* just perform the request */
    VMMDevGetMemBalloonChangeRequest *req = NULL;

    dprintf(("VBoxGuestQueryMemoryBalloon\n"));

    int rc = VbglGRAlloc((VMMDevRequestHeader **)&req, sizeof(VMMDevGetMemBalloonChangeRequest), VMMDevReq_GetMemBalloonChangeRequest);
    vmmdevInitRequest(&req->header, VMMDevReq_GetMemBalloonChangeRequest);
    req->eventAck = VMMDEV_EVENT_BALLOON_CHANGE_REQUEST;

    if (VBOX_SUCCESS(rc))
    {
        rc = VbglGRPerform(&req->header);

        if (VBOX_FAILURE(rc) || VBOX_FAILURE(req->header.rc))
        {
            dprintf(("VBoxGuest::VBoxGuestDeviceControl IOCTL_VBOXGUEST_CTL_CHECK_BALLOON: error issuing request to VMMDev!"
                     "rc = %d, VMMDev rc = %Vrc\n", rc, req->header.rc));
        }
        else
        {
            if (!pDevExt->MemBalloon.paMdlMemBalloon)
            {
                pDevExt->MemBalloon.cMaxBalloons = req->u32PhysMemSize;
                pDevExt->MemBalloon.paMdlMemBalloon = (PMDL *)ExAllocatePool(PagedPool, req->u32PhysMemSize * sizeof(PMDL));
                Assert(pDevExt->MemBalloon.paMdlMemBalloon);
                if (!pDevExt->MemBalloon.paMdlMemBalloon)
                    return VERR_NO_MEMORY;
            }
            Assert(pDevExt->MemBalloon.cMaxBalloons == req->u32PhysMemSize);

            rc = VBoxGuestSetBalloonSize(pDevExt, req->u32BalloonSize);
            /* ignore out of memory failures */
            if (rc == VERR_NO_MEMORY)
                rc = VINF_SUCCESS;

            if (pMemBalloonSize)
                *pMemBalloonSize = pDevExt->MemBalloon.cBalloons;
        }

        VbglGRFree(&req->header);
    }
    return rc;
}
#endif

void VBoxInitMemBalloon(PVBOXGUESTDEVEXT pDevExt)
{
#ifdef VBOX_WITH_MANAGEMENT
    ULONG dummy;

    pDevExt->MemBalloon.cBalloons       = 0;
    pDevExt->MemBalloon.cMaxBalloons    = 0;
    pDevExt->MemBalloon.paMdlMemBalloon = NULL;

    VBoxGuestQueryMemoryBalloon(pDevExt, &dummy);
#endif
}

void VBoxCleanupMemBalloon(PVBOXGUESTDEVEXT pDevExt)
{
#ifdef VBOX_WITH_MANAGEMENT
    if (pDevExt->MemBalloon.paMdlMemBalloon)
    {
        /* Clean up the memory balloon leftovers */
        VBoxGuestSetBalloonSize(pDevExt, 0);
        ExFreePool(pDevExt->MemBalloon.paMdlMemBalloon);
        pDevExt->MemBalloon.paMdlMemBalloon = NULL;
    }
    Assert(pDevExt->MemBalloon.cBalloons == 0);
#endif
}

/**
 * Device I/O Control entry point.
 *
 * @param   pDevObj     Device object.
 * @param   pIrp        Request packet.
 */
NTSTATUS VBoxGuestDeviceControl(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    dprintf(("VBoxGuest::VBoxGuestDeviceControl\n"));

    NTSTATUS Status = STATUS_SUCCESS;

    PVBOXGUESTDEVEXT pDevExt = (PVBOXGUESTDEVEXT)pDevObj->DeviceExtension;

    PIO_STACK_LOCATION pStack = IoGetCurrentIrpStackLocation(pIrp);

    char *pBuf = (char *)pIrp->AssociatedIrp.SystemBuffer; /* all requests are buffered. */

    unsigned cbOut = 0;

    switch (pStack->Parameters.DeviceIoControl.IoControlCode)
    {
        case IOCTL_VBOXGUEST_GETVMMDEVPORT:
        {
            dprintf(("VBoxGuest::VBoxGuestDeviceControl: IOCTL_VBOXGUEST_GETVMMDEVPORT\n"));

            if (pStack->Parameters.DeviceIoControl.OutputBufferLength < sizeof (VBoxGuestPortInfo))
            {
                Status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            VBoxGuestPortInfo *portInfo = (VBoxGuestPortInfo*)pBuf;

            portInfo->portAddress = pDevExt->startPortAddress;
            portInfo->pVMMDevMemory = pDevExt->pVMMDevMemory;

            cbOut = sizeof(VBoxGuestPortInfo);

            break;
        }

        case IOCTL_VBOXGUEST_WAITEVENT:
        {
            /* Need to be extended to support multiple waiters for an event,
             * array of counters for each event, event mask is computed, each
             * time a wait event is arrived.
             */
            dprintf(("VBoxGuest::VBoxGuestDeviceControl: IOCTL_VBOXGUEST_WAITEVENT\n"));

            if (pStack->Parameters.DeviceIoControl.OutputBufferLength < sizeof(VBoxGuestWaitEventInfo))
            {
                dprintf(("VBoxGuest::VBoxGuestDeviceControl: OutputBufferLength %d < sizeof(VBoxGuestWaitEventInfo)\n",
                         pStack->Parameters.DeviceIoControl.OutputBufferLength, sizeof(VBoxGuestWaitEventInfo)));
                Status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            if (pStack->Parameters.DeviceIoControl.InputBufferLength < sizeof(VBoxGuestWaitEventInfo)) {
                dprintf(("VBoxGuest::VBoxGuestDeviceControl: InputBufferLength %d < sizeof(VBoxGuestWaitEventInfo)\n",
                         pStack->Parameters.DeviceIoControl.InputBufferLength, sizeof(VBoxGuestWaitEventInfo)));
                Status = STATUS_BUFFER_TOO_SMALL;
                break;
            }

            VBoxGuestWaitEventInfo *eventInfo = (VBoxGuestWaitEventInfo *)pBuf;

            if (!eventInfo->u32EventMaskIn || !IsPowerOfTwo (eventInfo->u32EventMaskIn)) {
                dprintf (("VBoxGuest::VBoxGuestDeviceControl: Invalid input mask %#x\n",
                          eventInfo->u32EventMaskIn));
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            eventInfo->u32EventFlagsOut = 0;
            int iBitOffset = GetMsb32 (eventInfo->u32EventMaskIn);
            bool fTimeout = (eventInfo->u32TimeoutIn != ~0L);

            dprintf (("mask = %d, iBitOffset = %d\n", iBitOffset, eventInfo->u32EventMaskIn));

            /* Possible problem with request completion right between the pending event check and KeWaitForSingleObject 
             * call; introduce a timeout (if none was specified) to make sure we don't wait indefinitely.
             */
            LARGE_INTEGER timeout;
            timeout.QuadPart = (fTimeout) ? eventInfo->u32TimeoutIn : 250;
            timeout.QuadPart *= -10000;

            NTSTATUS rc = STATUS_SUCCESS;

            for (;;)
            {
                bool fEventPending = TestAndClearEvent (pDevExt, iBitOffset);
                if (fEventPending)
                {
                    eventInfo->u32EventFlagsOut = 1 << iBitOffset;
                    break;
                }

                rc = KeWaitForSingleObject (&pDevExt->keventNotification, Executive /** @todo UserRequest? */,
                                            KernelMode, TRUE, &timeout);
                dprintf(("IOCTL_VBOXGUEST_WAITEVENT: Wait returned %d -> event %x\n", rc, eventInfo->u32EventFlagsOut));

                if (!fTimeout && rc == STATUS_TIMEOUT)
                    continue;

                if (rc != STATUS_SUCCESS)
                {
                    /* There was a timeout or wait was interrupted, etc. */
                    break;
                }
            }

            dprintf (("u32EventFlagsOut = %#x\n", eventInfo->u32EventFlagsOut));
            cbOut = sizeof(VBoxGuestWaitEventInfo);
            break;
        }

        case IOCTL_VBOXGUEST_VMMREQUEST:
        {
            dprintf(("VBoxGuest::VBoxGuestDeviceControl: IOCTL_VBOXGUEST_VMMREQUEST\n"));

#define CHECK_SIZE(s) \
            if (pStack->Parameters.DeviceIoControl.OutputBufferLength < s) \
            { \
                dprintf(("VBoxGuest::VBoxGuestDeviceControl: OutputBufferLength %d < %d\n", \
                         pStack->Parameters.DeviceIoControl.OutputBufferLength, s)); \
                Status = STATUS_BUFFER_TOO_SMALL; \
                break; \
            } \
            if (pStack->Parameters.DeviceIoControl.InputBufferLength < s) { \
                dprintf(("VBoxGuest::VBoxGuestDeviceControl: InputBufferLength %d < %d\n", \
                         pStack->Parameters.DeviceIoControl.InputBufferLength, s)); \
                Status = STATUS_BUFFER_TOO_SMALL; \
                break; \
            }

            /* get the request header */
            CHECK_SIZE(sizeof(VMMDevRequestHeader));
            VMMDevRequestHeader *requestHeader = (VMMDevRequestHeader *)pBuf;
            if (!vmmdevGetRequestSize(requestHeader->requestType))
            {
                Status = STATUS_INVALID_PARAMETER;
                break;
            }
            /* make sure the buffers suit the request */
            CHECK_SIZE(vmmdevGetRequestSize(requestHeader->requestType));

            /* just perform the request */
            VMMDevRequestHeader *req = NULL;

            int rc = VbglGRAlloc((VMMDevRequestHeader **)&req, requestHeader->size, requestHeader->requestType);

            if (VBOX_SUCCESS(rc))
            {
                /* copy the request information */
                memcpy((void*)req, (void*)pBuf, requestHeader->size);
                rc = VbglGRPerform(req);

                if (VBOX_FAILURE(rc) || VBOX_FAILURE(req->rc))
                {
                    dprintf(("VBoxGuest::VBoxGuestDeviceControl IOCTL_VBOXGUEST_VMMREQUEST: error issuing request to VMMDev!"
                             "rc = %d, VMMDev rc = %Vrc\n", rc, req->rc));
                    Status = STATUS_UNSUCCESSFUL;
                }
                else
                {
                    /* copy result */
                    memcpy((void*)pBuf, (void*)req, requestHeader->size);
                    cbOut = requestHeader->size;
                }

                VbglGRFree(req);
            }
            else
            {
                Status = STATUS_UNSUCCESSFUL;
            }
#undef CHECK_SIZE
            break;
        }

        case IOCTL_VBOXGUEST_CTL_FILTER_MASK:
        {
            VBoxGuestFilterMaskInfo *maskInfo;

            if (pStack->Parameters.DeviceIoControl.InputBufferLength < sizeof(VBoxGuestFilterMaskInfo)) {
                dprintf (("VBoxGuest::VBoxGuestDeviceControl: InputBufferLength %d < %d\n",
                          pStack->Parameters.DeviceIoControl.InputBufferLength,
                          sizeof (VBoxGuestFilterMaskInfo)));
                Status = STATUS_BUFFER_TOO_SMALL;
                break;

            }

            maskInfo = (VBoxGuestFilterMaskInfo *) pBuf;
            if (!CtlGuestFilterMask (maskInfo->u32OrMask, maskInfo->u32NotMask))
            {
                Status = STATUS_UNSUCCESSFUL;
            }
            break;
        }

#ifdef VBOX_HGCM
        /* HGCM offers blocking IOCTLSs just like waitevent and actually
         * uses the same waiting code.
         */
        case IOCTL_VBOXGUEST_HGCM_CONNECT:
        {
            dprintf(("VBoxGuest::VBoxGuestDeviceControl: IOCTL_VBOXGUEST_HGCM_CONNECT\n"));

            if (pStack->Parameters.DeviceIoControl.OutputBufferLength != sizeof(VBoxGuestHGCMConnectInfo))
            {
                dprintf(("VBoxGuest::VBoxGuestDeviceControl: OutputBufferLength %d != sizeof(VBoxGuestHGCMConnectInfo) %d\n",
                         pStack->Parameters.DeviceIoControl.OutputBufferLength, sizeof(VBoxGuestHGCMConnectInfo)));
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            if (pStack->Parameters.DeviceIoControl.InputBufferLength != sizeof(VBoxGuestHGCMConnectInfo)) {
                dprintf(("VBoxGuest::VBoxGuestDeviceControl: InputBufferLength %d != sizeof(VBoxGuestHGCMConnectInfo) %d\n",
                         pStack->Parameters.DeviceIoControl.InputBufferLength, sizeof(VBoxGuestHGCMConnectInfo)));
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            VBoxGuestHGCMConnectInfo *ptr = (VBoxGuestHGCMConnectInfo *)pBuf;

            /* If request will be processed asynchronously, execution will
             * go to VBoxHGCMCallback. There it will wait for the request event, signalled from IRQ.
             * On IRQ arrival, the VBoxHGCMCallback(s) will check the request memory and, if completion
             * flag is set, returns.
             */

            dprintf(("a) ptr->u32ClientID = %d\n", ptr->u32ClientID));

            int rc = VbglHGCMConnect (ptr, VBoxHGCMCallback, pDevExt, 0);

            dprintf(("b) ptr->u32ClientID = %d\n", ptr->u32ClientID));

            if (VBOX_FAILURE(rc))
            {
                dprintf(("IOCTL_VBOXGUEST_HGCM_CONNECT: vbox rc = %Vrc\n", rc));
                Status = STATUS_UNSUCCESSFUL;
            }
            else
            {
                cbOut = pStack->Parameters.DeviceIoControl.OutputBufferLength;
            }

        } break;

        case IOCTL_VBOXGUEST_HGCM_DISCONNECT:
        {
            dprintf(("VBoxGuest::VBoxGuestDeviceControl: IOCTL_VBOXGUEST_HGCM_DISCONNECT\n"));

            if (pStack->Parameters.DeviceIoControl.OutputBufferLength != sizeof(VBoxGuestHGCMDisconnectInfo))
            {
                dprintf(("VBoxGuest::VBoxGuestDeviceControl: OutputBufferLength %d != sizeof(VBoxGuestHGCMDisconnectInfo) %d\n",
                         pStack->Parameters.DeviceIoControl.OutputBufferLength, sizeof(VBoxGuestHGCMDisconnectInfo)));
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            if (pStack->Parameters.DeviceIoControl.InputBufferLength != sizeof(VBoxGuestHGCMDisconnectInfo)) {
                dprintf(("VBoxGuest::VBoxGuestDeviceControl: InputBufferLength %d != sizeof(VBoxGuestHGCMDisconnectInfo) %d\n",
                         pStack->Parameters.DeviceIoControl.InputBufferLength, sizeof(VBoxGuestHGCMDisconnectInfo)));
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            VBoxGuestHGCMDisconnectInfo *ptr = (VBoxGuestHGCMDisconnectInfo *)pBuf;

            /* If request will be processed asynchronously, execution will
             * go to VBoxHGCMCallback. There it will wait for the request event, signalled from IRQ.
             * On IRQ arrival, the VBoxHGCMCallback(s) will check the request memory and, if completion
             * flag is set, returns.
             */

            int rc = VbglHGCMDisconnect (ptr, VBoxHGCMCallback, pDevExt, 0);

            if (VBOX_FAILURE(rc))
            {
                dprintf(("IOCTL_VBOXGUEST_HGCM_DISCONNECT: vbox rc = %Vrc\n", rc));
                Status = STATUS_UNSUCCESSFUL;
            }
            else
            {
                cbOut = pStack->Parameters.DeviceIoControl.OutputBufferLength;
            }

        } break;

        case IOCTL_VBOXGUEST_HGCM_CALL:
        {
            dprintf(("VBoxGuest::VBoxGuestDeviceControl: IOCTL_VBOXGUEST_HGCM_CALL\n"));

            Status = vboxHGCMVerifyIOBuffers (pStack,
                                              sizeof (VBoxGuestHGCMCallInfo));
                                              
            if (Status != STATUS_SUCCESS)
            {
                dprintf(("VBoxGuest::VBoxGuestDeviceControl: invalid parameter. Status: %p\n", Status));
                break;
            }

            VBoxGuestHGCMCallInfo *ptr = (VBoxGuestHGCMCallInfo *)pBuf;

            int rc = VbglHGCMCall (ptr, VBoxHGCMCallback, pDevExt, 0);

            if (VBOX_FAILURE(rc))
            {
                dprintf(("IOCTL_VBOXGUEST_HGCM_CALL: vbox rc = %Vrc\n", rc));
                Status = STATUS_UNSUCCESSFUL;
            }
            else
            {
                cbOut = pStack->Parameters.DeviceIoControl.OutputBufferLength;
            }

        } break;
#endif /* VBOX_HGCM */

#ifdef VBOX_WITH_VRDP_SESSION_HANDLING
        case IOCTL_VBOXGUEST_ENABLE_VRDP_SESSION:
        {
            if (!pDevExt->fVRDPEnabled)
            {
                KUSER_SHARED_DATA *pSharedUserData = (KUSER_SHARED_DATA *)KI_USER_SHARED_DATA;

                pDevExt->fVRDPEnabled            = TRUE;
                pDevExt->ulOldActiveConsoleId    = pSharedUserData->ActiveConsoleId;
                pSharedUserData->ActiveConsoleId = 2;
            }
            break;
        }

        case IOCTL_VBOXGUEST_DISABLE_VRDP_SESSION:
        {
            if (pDevExt->fVRDPEnabled)
            {
                KUSER_SHARED_DATA *pSharedUserData = (KUSER_SHARED_DATA *)KI_USER_SHARED_DATA;

                pDevExt->fVRDPEnabled            = FALSE;
                pSharedUserData->ActiveConsoleId = pDevExt->ulOldActiveConsoleId;
                pDevExt->ulOldActiveConsoleId    = 0;
            }
            break;
        }
#endif

#ifdef VBOX_WITH_MANAGEMENT
        case IOCTL_VBOXGUEST_CTL_CHECK_BALLOON:
        {
            ULONG *pMemBalloonSize = (ULONG *) pBuf;

            if (pStack->Parameters.DeviceIoControl.OutputBufferLength != sizeof(ULONG))
            {
                dprintf(("VBoxGuest::VBoxGuestDeviceControl: OutputBufferLength %d != sizeof(ULONG) %d\n",
                         pStack->Parameters.DeviceIoControl.OutputBufferLength, sizeof(ULONG)));
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            int rc = VBoxGuestQueryMemoryBalloon(pDevExt, pMemBalloonSize);
            if (VBOX_FAILURE(rc))
            {
                dprintf(("IOCTL_VBOXGUEST_CTL_CHECK_BALLOON: vbox rc = %Vrc\n", rc));
                Status = STATUS_UNSUCCESSFUL;
            }
            else
            {
                cbOut = pStack->Parameters.DeviceIoControl.OutputBufferLength;
            }
            break;
        }
#endif

        default:
             Status = STATUS_INVALID_PARAMETER;
             break;
    }

    pIrp->IoStatus.Status = Status;
    pIrp->IoStatus.Information = cbOut;

    IoCompleteRequest(pIrp, IO_NO_INCREMENT);

    dprintf(("VBoxGuest::VBoxGuestDeviceControl: returned cbOut=%d rc=%#x\n", cbOut, Status));

    return Status;
}


/**
 * IRP_MJ_SYSTEM_CONTROL handler
 *
 * @returns NT status code
 * @param   pDevObj     Device object.
 * @param   pIrp        IRP.
 */
NTSTATUS VBoxGuestSystemControl(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    PVBOXGUESTDEVEXT pDevExt = (PVBOXGUESTDEVEXT)pDevObj->DeviceExtension;

    dprintf(("VBoxGuest::VBoxGuestSystemControl\n"));

    /* Always pass it on to the next driver. */
    IoSkipCurrentIrpStackLocation(pIrp);

    return IoCallDriver(pDevExt->nextLowerDriver, pIrp);
}

/**
 * IRP_MJ_SHUTDOWN handler
 *
 * @returns NT status code
 * @param pDevObj    Device object.
 * @param pIrp       IRP.
 */
NTSTATUS VBoxGuestShutdown(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    VMMDevPowerStateRequest *req = NULL;

    dprintf(("VBoxGuest::VBoxGuestShutdown\n"));

    int rc = VbglGRAlloc ((VMMDevRequestHeader **)&req, sizeof (VMMDevPowerStateRequest), VMMDevReq_SetPowerStatus);

    if (VBOX_SUCCESS(rc))
    {
        req->powerState = VMMDevPowerState_PowerOff;

        rc = VbglGRPerform (&req->header);

        if (VBOX_FAILURE(rc) || VBOX_FAILURE(req->header.rc))
        {
            dprintf(("VBoxGuest::PowerStateRequest: error performing request to VMMDev."
                      "rc = %d, VMMDev rc = %Vrc\n", rc, req->header.rc));
        }

        VbglGRFree (&req->header);
    }

    return STATUS_SUCCESS;
}

/**
 * Stub function for functions we don't implemented.
 *
 * @returns STATUS_NOT_SUPPORTED
 * @param   pDevObj     Device object.
 * @param   pIrp        IRP.
 */
NTSTATUS VBoxGuestNotSupportedStub(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    dprintf(("VBoxGuest::VBoxGuestNotSupportedStub\n"));
    pDevObj = pDevObj;

    pIrp->IoStatus.Information = 0;
    pIrp->IoStatus.Status = STATUS_NOT_SUPPORTED;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);

    return STATUS_NOT_SUPPORTED;
}

/**
 * DPC handler
 *
 * @param   dpc         DPC descriptor.
 * @param   pDevObj     Device object.
 * @param   irp         Interrupt request packet.
 * @param   context     Context specific pointer.
 */
VOID VBoxGuestDpcHandler(PKDPC dpc, PDEVICE_OBJECT pDevObj,
                         PIRP irp, PVOID context)
{
    /* Unblock handlers waiting for arrived events.
     *
     * Events are very low things, there is one event flag (1 or more bit)
     * for each event. Each event is processed by exactly one handler.
     *
     * Assume that we trust additions and that other drivers will
     * handle its respective events without trying to fetch all events.
     *
     * Anyway design assures that wrong event processing will affect only guest.
     *
     * Event handler calls VMMDev IOCTL for waiting an event.
     * It supplies event mask. IOCTL blocks on EventNotification.
     * Here we just signal an the EventNotification to all waiting
     * threads, the IOCTL handler analyzes events and either
     * return to caller or blocks again.
     *
     * If we do not have too many events this is a simple and good
     * approach. Other way is to have as many Event objects as the callers
     * and wake up only callers waiting for the specific event.
     *
     * Now with the 'wake up all' appoach we probably do not need the DPC
     * handler and can signal event directly from ISR.
     *
     */

    PVBOXGUESTDEVEXT pDevExt = (PVBOXGUESTDEVEXT)pDevObj->DeviceExtension;

    dprintf(("VBoxGuest::VBoxGuestDpcHandler\n"));

    KePulseEvent(&pDevExt->keventNotification, 0, FALSE);

}

/**
 * ISR handler
 *
 * @return  BOOLEAN        indicates whether the IRQ came from us (TRUE) or not (FALSE)
 * @param   interrupt      Interrupt that was triggered.
 * @param   serviceContext Context specific pointer.
 */
BOOLEAN VBoxGuestIsrHandler(PKINTERRUPT interrupt, PVOID serviceContext)
{
    NTSTATUS rc;
    PVBOXGUESTDEVEXT pDevExt = (PVBOXGUESTDEVEXT)serviceContext;
    BOOLEAN fIRQTaken = FALSE;

    dprintf(("VBoxGuest::VBoxGuestIsrHandler haveEvents = %d\n",
             pDevExt->pVMMDevMemory->V.V1_04.fHaveEvents));

    /*
     * now we have to find out whether it was our IRQ. Read the event mask
     * from our device to see if there are any pending events
     */
    if (pDevExt->pVMMDevMemory->V.V1_04.fHaveEvents)
    {
        /* Acknowlegde events. */
        VMMDevEvents *req = pDevExt->irqAckEvents;

        rc = VbglGRPerform (&req->header);
        if (VBOX_SUCCESS(rc) && VBOX_SUCCESS(req->header.rc))
        {
            dprintf(("VBoxGuest::VBoxGuestIsrHandler: acknowledge events succeeded %#x\n",
                     req->events));

            ASMAtomicOrU32((uint32_t *)&pDevExt->u32Events, req->events);
            IoRequestDpc(pDevExt->deviceObject, pDevExt->currentIrp, NULL);
        }
        else
        {
            /* This can't be actually. This is sign of a serious problem. */
            dprintf(("VBoxGuest::VBoxGuestIsrHandler: "
                     "acknowledge events failed rc = %d, header rc = %d\n",
                     rc, req->header.rc));
        }

        /* Mark IRQ as taken, there were events for us. */
        fIRQTaken = TRUE;
    }

    return fIRQTaken;
}

/**
 * Worker thread to do periodic things such as synchronize the
 * system time and notify other drivers of events.
 *
 * @param   pDevExt device extension pointer
 */
VOID vboxWorkerThread(PVOID context)
{
    PVBOXGUESTDEVEXT pDevExt;

    pDevExt = (PVBOXGUESTDEVEXT)context;
    dprintf(("VBoxGuest::vboxWorkerThread entered\n"));

    VMMDevReqHostTime *req = NULL;

    int rc = VbglGRAlloc ((VMMDevRequestHeader **)&req, sizeof (VMMDevReqHostTime), VMMDevReq_GetHostTime);

    if (VBOX_FAILURE(rc))
    {
        dprintf(("VBoxGuest::vboxWorkerThread: could not allocate request buffer, exiting rc = %d!\n", rc));
        return;
    }

    /* perform the hypervisor address space reservation */
    reserveHypervisorMemory(pDevExt);

    do
    {
        /*
         * Do the time sync
         */
        {
            LARGE_INTEGER systemTime;
            #define TICKSPERSEC  10000000
            #define TICKSPERMSEC 10000
            #define SECSPERDAY   86400
            #define SECS_1601_TO_1970  ((369 * 365 + 89) * (uint64_t)SECSPERDAY)
            #define TICKS_1601_TO_1970 (SECS_1601_TO_1970 * TICKSPERSEC)


            req->header.rc = VERR_GENERAL_FAILURE;

            rc = VbglGRPerform (&req->header);

            if (VBOX_SUCCESS(rc) && VBOX_SUCCESS(req->header.rc))
            {
                uint64_t hostTime = req->time;

                // Windows was originally designed in 1601...
                systemTime.QuadPart = hostTime * (uint64_t)TICKSPERMSEC + (uint64_t)TICKS_1601_TO_1970;
                dprintf(("VBoxGuest::vboxWorkerThread: synching time with host time (msec/UTC): %llu\n", hostTime));
                ZwSetSystemTime(&systemTime, NULL);
            }
            else
            {
                dprintf(("VBoxGuest::PowerStateRequest: error performing request to VMMDev."
                          "rc = %d, VMMDev rc = %Vrc\n", rc, req->header.rc));
            }
        }

        /*
         * Go asleep unless we're supposed to terminate
         */
        if (!pDevExt->stopThread)
        {
            ULONG secWait = 60;
            dprintf(("VBoxGuest::vboxWorkerThread: waiting for %u seconds...\n", secWait));
            LARGE_INTEGER dueTime;
            dueTime.QuadPart = -10000 * 1000 * (int)secWait;
            if (KeWaitForSingleObject(&pDevExt->workerThreadRequest, Executive,
                                  KernelMode, FALSE, &dueTime) == STATUS_SUCCESS)
            {
                KeResetEvent(&pDevExt->workerThreadRequest);
            }
        }
    } while (!pDevExt->stopThread);

    dprintf(("VBoxGuest::vboxWorkerThread: we've been asked to terminate!\n"));

    /* free our request buffer */
    VbglGRFree (&req->header);

    if (pDevExt->workerThread)
    {
        ObDereferenceObject(pDevExt->workerThread);
        pDevExt->workerThread = NULL;
    }
    dprintf(("VBoxGuest::vboxWorkerThread: now really gone!\n"));
}

/**
 * Create driver worker threads
 *
 * @returns NTSTATUS NT status code
 * @param pDevExt    VBoxGuest device extension
 */
NTSTATUS createThreads(PVBOXGUESTDEVEXT pDevExt)
{
    NTSTATUS rc;
    HANDLE threadHandle;
    OBJECT_ATTRIBUTES objAttributes;

    dprintf(("VBoxGuest::createThreads\n"));

    // first setup the request semaphore
    KeInitializeEvent(&pDevExt->workerThreadRequest, SynchronizationEvent, FALSE);

// the API has slightly changed after NT4
#ifdef TARGET_NT4
#ifdef OBJ_KERNEL_HANDLE
#undef OBJ_KERNEL_HANDLE
#endif
#define OBJ_KERNEL_HANDLE 0
#endif

    /*
     * The worker thread
     */
    InitializeObjectAttributes(&objAttributes,
                               NULL,
                               OBJ_KERNEL_HANDLE,
                               NULL,
                               NULL);

    rc = PsCreateSystemThread(&threadHandle,
                              THREAD_ALL_ACCESS,
                              &objAttributes,
                              (HANDLE)0L,
                              NULL,
                              vboxWorkerThread,
                              pDevExt);
    dprintf(("VBoxGuest::createThreads: PsCreateSystemThread for worker thread returned: 0x%x\n", rc));
    rc = ObReferenceObjectByHandle(threadHandle,
                                   THREAD_ALL_ACCESS,
                                   NULL,
                                   KernelMode,
                                   (PVOID*)&pDevExt->workerThread,
                                   NULL);
    ZwClose(threadHandle);

    /*
     * The idle thread
     */
#if 0 /// @todo Windows "sees" that time is lost and reports 100% usage
    rc = PsCreateSystemThread(&threadHandle,
                              THREAD_ALL_ACCESS,
                              &objAttributes,
                              (HANDLE)0L,
                              NULL,
                              vboxIdleThread,
                              pDevExt);
    dprintf(("VBoxGuest::createThreads: PsCreateSystemThread for idle thread returned: 0x%x\n", rc));
    rc = ObReferenceObjectByHandle(threadHandle,
                                   THREAD_ALL_ACCESS,
                                   NULL,
                                   KernelMode,
                                   (PVOID*)&pDevExt->idleThread,
                                   NULL);
    ZwClose(threadHandle);
#endif

    return rc;
}

/**
 * Helper routine to reserve address space for the hypervisor
 * and communicate its position.
 *
 * @param pDevExt     Device extension structure.
 */
VOID reserveHypervisorMemory(PVBOXGUESTDEVEXT pDevExt)
{
    // @todo rc handling
    uint32_t hypervisorSize;

    VMMDevReqHypervisorInfo *req = NULL;

    int rc = VbglGRAlloc ((VMMDevRequestHeader **)&req, sizeof (VMMDevReqHypervisorInfo), VMMDevReq_GetHypervisorInfo);

    if (VBOX_SUCCESS(rc))
    {
        req->hypervisorStart = 0;
        req->hypervisorSize = 0;

        rc = VbglGRPerform (&req->header);

        if (VBOX_SUCCESS(rc) && VBOX_SUCCESS(req->header.rc))
        {
            hypervisorSize = req->hypervisorSize;

            if (!hypervisorSize)
            {
                dprintf(("VBoxGuest::reserveHypervisorMemory: host returned 0, not doing anything\n"));
                return;
            }

            dprintf(("VBoxGuest::reserveHypervisorMemory: host wants %u bytes of hypervisor address space\n", hypervisorSize));

            // Map fictive physical memory into the kernel address space to reserve virtual
            // address space. This API does not perform any checks but just allocate the
            // PTEs (which we don't really need/want but there isn't any other clean method).
            // The hypervisor only likes 4MB aligned virtual addresses, so we have to allocate
            // 4MB more than we are actually supposed to in order to guarantee that. Maybe we
            // can come up with a less lavish algorithm lateron.
            PHYSICAL_ADDRESS physAddr;
            physAddr.QuadPart = HYPERVISOR_PHYSICAL_START;
            pDevExt->hypervisorMappingSize = hypervisorSize + 0x400000;
            pDevExt->hypervisorMapping     = MmMapIoSpace(physAddr,
                                                          pDevExt->hypervisorMappingSize,
                                                          MmNonCached);
            if (!pDevExt->hypervisorMapping)
            {
                dprintf(("VBoxGuest::reserveHypervisorMemory: MmMapIoSpace returned NULL!\n"));
                return;
            }

            dprintf(("VBoxGuest::reserveHypervisorMemory: MmMapIoSpace returned %p\n", pDevExt->hypervisorMapping));
            dprintf(("VBoxGuest::reserveHypervisorMemory: communicating %p to host\n",
                    ALIGNP(pDevExt->hypervisorMapping, 0x400000)));

            /* align at 4MB */
            req->hypervisorStart = (RTGCPTR)ALIGNP(pDevExt->hypervisorMapping, 0x400000);

            req->header.requestType = VMMDevReq_SetHypervisorInfo;
            req->header.rc          = VERR_GENERAL_FAILURE;

            /* issue request */
            rc = VbglGRPerform (&req->header);

            if (VBOX_FAILURE(rc) || VBOX_FAILURE(req->header.rc))
            {
                dprintf(("VBoxGuest::reserveHypervisorMemory: error communicating physical address to VMMDev!"
                         "rc = %d, VMMDev rc = %Vrc\n", rc, req->header.rc));
            }
        }
        else
        {
            dprintf(("VBoxGuest::reserveHypervisorMemory: request failed with rc %d, VMMDev rc = %Vrc\n", rc, req->header.rc));
        }
        VbglGRFree (&req->header);
    }

    return;
}

/**
 * Helper function to unregister a virtual address space mapping
 *
 * @param pDevExt     Device extension
 */
VOID unreserveHypervisorMemory(PVBOXGUESTDEVEXT pDevExt)
{
    VMMDevReqHypervisorInfo *req = NULL;

    int rc = VbglGRAlloc ((VMMDevRequestHeader **)&req, sizeof (VMMDevReqHypervisorInfo), VMMDevReq_SetHypervisorInfo);

    if (VBOX_SUCCESS(rc))
    {
        /* tell the hypervisor that the mapping is no longer available */

        req->hypervisorStart = 0;
        req->hypervisorSize = 0;

        rc = VbglGRPerform (&req->header);

        if (VBOX_FAILURE(rc) || VBOX_FAILURE(req->header.rc))
        {
            dprintf(("VBoxGuest::unreserveHypervisorMemory: error communicating physical address to VMMDev!"
                     "rc = %d, VMMDev rc = %Vrc\n", rc, req->header.rc));
        }

        VbglGRFree (&req->header);
    }

    if (!pDevExt->hypervisorMapping)
    {
        dprintf(("VBoxGuest::unreserveHypervisorMemory: there is no mapping, returning\n"));
        return;
    }

    // unmap fictive IO space
    MmUnmapIoSpace(pDevExt->hypervisorMapping, pDevExt->hypervisorMappingSize);
    dprintf(("VBoxGuest::unreserveHypervisorMemmory: done\n"));
}

/**
 * Idle thread that runs at the lowest priority possible
 * and whenever scheduled, makes a VMMDev call to give up
 * timeslices. This is so prevent Windows from thinking that
 * nothing is happening on the machine and doing stupid things
 * that would steal time from other VMs it doesn't know of.
 *
 * @param   pDevExt device extension pointer
 */
VOID vboxIdleThread(PVOID context)
{
    PVBOXGUESTDEVEXT pDevExt;

    pDevExt = (PVBOXGUESTDEVEXT)context;
    dprintf(("VBoxGuest::vboxIdleThread entered\n"));

    /* set priority as low as possible */
    KeSetPriorityThread(KeGetCurrentThread(), LOW_PRIORITY);

    /* allocate VMMDev request structure */
    VMMDevReqIdle *req;
    int rc = VbglGRAlloc((VMMDevRequestHeader **)&req, sizeof (VMMDevReqHypervisorInfo), VMMDevReq_Idle);
    if (VBOX_FAILURE(rc))
    {
        dprintf(("VBoxGuest::vboxIdleThread: error %Vrc allocating request structure!\n"));
        return;
    }

    do
    {
        //dprintf(("VBoxGuest: performing idle request..\n"));
        /* perform idle request */
        VbglGRPerform(&req->header);

    } while (!pDevExt->stopThread);

    VbglGRFree(&req->header);

    dprintf(("VBoxGuest::vboxIdleThread leaving\n"));
}
