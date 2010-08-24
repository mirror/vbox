/** @file
 *
 * VBox USB drivers - USB Monitor driver - Win32 host:
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
 *
 * Oracle Corporation confidential
 * All rights reserved
 */


#include "USBMon.h"
#include <VBox/usblib.h>
#include <excpt.h>
#include <stdio.h>

/*
 * Note: Must match the VID & PID in the USB driver .inf file!!
 */
/*
  BusQueryDeviceID USB\Vid_80EE&Pid_CAFE
  BusQueryInstanceID 2
  BusQueryHardwareIDs USB\Vid_80EE&Pid_CAFE&Rev_0100
  BusQueryHardwareIDs USB\Vid_80EE&Pid_CAFE
  BusQueryCompatibleIDs USB\Class_ff&SubClass_00&Prot_00
  BusQueryCompatibleIDs USB\Class_ff&SubClass_00
  BusQueryCompatibleIDs USB\Class_ff
*/

#define szBusQueryDeviceId       L"USB\\Vid_80EE&Pid_CAFE"
#define szBusQueryHardwareIDs    L"USB\\Vid_80EE&Pid_CAFE&Rev_0100\0USB\\Vid_80EE&Pid_CAFE\0\0"
#define szBusQueryCompatibleIDs  L"USB\\Class_ff&SubClass_00&Prot_00\0USB\\Class_ff&SubClass_00\0USB\\Class_ff\0\0"

#define szDeviceTextDescription          L"VirtualBox USB"

static PDEVICE_OBJECT ControlDeviceObject = NULL;
static PDRIVER_DISPATCH pfnOldPnPHandler = 0;

static struct
{
    bool    fValid;
} InstalledPDOHooks[16] = {0};

/**
 * Driver entry point.
 *
 * @returns appropriate status code.
 * @param   pDrvObj     Pointer to driver object.
 * @param   pRegPath    Registry base path.
 */
NTSTATUS _stdcall DriverEntry(PDRIVER_OBJECT pDrvObj, PUNICODE_STRING pRegPath)
{
    NTSTATUS    rc;
    UNICODE_STRING  DevName;
    PDEVICE_OBJECT  pDevObj;

    DebugPrint(("VBoxUSBMon::DriverEntry\n"));

    /*
     * Create device.
     * (That means creating a device object and a symbolic link so the DOS
     * subsystems (OS/2, win32, ++) can access the device.)
     */
    RtlInitUnicodeString(&DevName, USBMON_DEVICE_NAME_NT);
    rc = IoCreateDevice(pDrvObj, sizeof(DEVICE_EXTENSION), &DevName, FILE_DEVICE_UNKNOWN, 0, FALSE, &pDevObj);
    if (NT_SUCCESS(rc))
    {
        UNICODE_STRING DosName;

        /* Save device object pointer in order to check whether we're being called in the entry points we register. */
        ControlDeviceObject = pDevObj;

        RtlInitUnicodeString(&DosName, USBMON_DEVICE_NAME_DOS);
        rc = IoCreateSymbolicLink(&DosName, &DevName);
        if (NT_SUCCESS(rc))
        {
            ULONG              ulIndex;
            PDRIVER_DISPATCH  *dispatch;

            /*
             * Initialize the device extension.
             */
            PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)pDevObj->DeviceExtension;
            memset(pDevExt, 0, sizeof(*pDevExt));

            /*
             * Setup the driver entry points in pDrvObj.
             */
            for (ulIndex = 0, dispatch = pDrvObj->MajorFunction;
                 ulIndex <= IRP_MJ_MAXIMUM_FUNCTION;
                 ulIndex++, dispatch++) {

                *dispatch = VBoxUSBMonStub;
            }

            pDrvObj->DriverUnload                           = VBoxUSBMonUnload;
            pDrvObj->MajorFunction[IRP_MJ_CREATE]           = VBoxUSBMonCreate;
            pDrvObj->MajorFunction[IRP_MJ_CLOSE]            = VBoxUSBMonClose;
            pDrvObj->MajorFunction[IRP_MJ_DEVICE_CONTROL]   = VBoxUSBMonDeviceControl;

//            pDrvObj->MajorFunction[IRP_MJ_PNP]              = VBoxUSBMonDispatchPnp;
//            pDrvObj->MajorFunction[IRP_MJ_POWER]            = VBoxUSBMonDispatchPower;
            /* Note: unable to unload driver if this is set: */
////            pDrvObj->DriverExtension->AddDevice             = VBoxUSBMonAddDevice;

            VBoxUSBInit();
            DebugPrint(("VBoxUSBMon::DriverEntry   returning STATUS_SUCCESS\n"));
            return STATUS_SUCCESS;
        }
        else
            DebugPrint(("IoCreateSymbolicLink failed with rc=%#x!\n", rc));

        IoDeleteDevice(pDevObj);
    }
    else
        DebugPrint(("IoCreateDevice failed with rc=%#x!\n", rc));

    if (NT_SUCCESS(rc))
        rc = STATUS_INVALID_PARAMETER;
    DebugPrint(("VBoxUSBMon::DriverEntry returning %#x\n", rc));
    return rc;
}

/**
 * Uninstall the PnP irp hook
 *
 * @param   pDevExt     Device extension
 */
void VBoxUSBUninstallPnPHook(PDEVICE_EXTENSION pDevExt)
{
    NTSTATUS        status;
    UNICODE_STRING  szStandardHubName;

    szStandardHubName.Length = 0;
    szStandardHubName.MaximumLength = 0;
    szStandardHubName.Buffer = 0;
    RtlInitUnicodeString(&szStandardHubName, L"\\Driver\\usbhub");

    DebugPrint(("Unhook USB hub drivers\n"));
    for (int i = 0; i < RT_ELEMENTS(InstalledPDOHooks); i++)
    {
        char            szHubName[32];
        UNICODE_STRING  UnicodeName;
        ANSI_STRING     AnsiName;
        PDEVICE_OBJECT  pHubDevObj;
        PFILE_OBJECT    pHubFileObj;

        /* Don't bother to check PDO's that we haven't hooked; might even lead to host crashes during shutdown as IoGetDeviceObjectPointer can reopen a driver. */
        if (!InstalledPDOHooks[i].fValid)
            continue;

        sprintf(szHubName, "\\Device\\USBPDO-%d", i);

        UnicodeName.Length = 0;
        UnicodeName.MaximumLength = 0;
        UnicodeName.Buffer = 0;

        RtlInitAnsiString(&AnsiName, szHubName);
        RtlAnsiStringToUnicodeString(&UnicodeName, &AnsiName, TRUE);

        status = IoGetDeviceObjectPointer(&UnicodeName, FILE_READ_DATA, &pHubFileObj, &pHubDevObj);
        if (status == STATUS_SUCCESS)
        {
            DebugPrint(("IoGetDeviceObjectPointer for %s returned %p %p\n", szHubName, pHubDevObj, pHubFileObj));

            if (    pHubDevObj->DriverObject
                &&  pHubDevObj->DriverObject->DriverName.Buffer
                &&  pHubDevObj->DriverObject->DriverName.Length
                &&  !RtlCompareUnicodeString(&szStandardHubName, &pHubDevObj->DriverObject->DriverName, TRUE /* case insensitive */))
            {
#if 0
                DebugPrint(("Associated driver"));
                DebugPrintUnicodeString(&pHubDevObj->DriverObject->DriverName);
#endif
                DebugPrint(("pnp handler %p\n", pHubDevObj->DriverObject->MajorFunction[IRP_MJ_PNP]));
                InterlockedCompareExchangePointer((PVOID *)&pHubDevObj->DriverObject->MajorFunction[IRP_MJ_PNP], pfnOldPnPHandler, VBoxUSBMonPnPHook);
            }
            ObDereferenceObject(pHubFileObj);
        }
//        else
//            DebugPrint(("IoGetDeviceObjectPointer for %s returned %x\n", szHubName, status));

        RtlFreeUnicodeString(&UnicodeName);
    }
    pDevExt->fHookDevice = FALSE;
    pfnOldPnPHandler = NULL;
}

/**
 * Unload the driver.
 *
 * @param   pDrvObj     Driver object.
 */
void _stdcall VBoxUSBMonUnload(PDRIVER_OBJECT pDrvObj)
{
    PDEVICE_EXTENSION       pDevExt = (PDEVICE_EXTENSION)pDrvObj->DeviceObject->DeviceExtension;

    DebugPrint(("VBoxUSBMonUnload pDevExt=%p ControlDeviceObject=%p\n", pDevExt, ControlDeviceObject));

    Assert(!pfnOldPnPHandler);
    if (pfnOldPnPHandler) /* very bad! */
        VBoxUSBUninstallPnPHook(pDevExt);

    /* Clean up the filter manager */
    VBoxUSBTerm();

    /*
     * We ASSUME that it's not possible to unload a driver with open handles.
     * Start by deleting the symbolic link
     */
    UNICODE_STRING DosName;
    RtlInitUnicodeString(&DosName, USBMON_DEVICE_NAME_DOS);
    NTSTATUS rc = IoDeleteSymbolicLink(&DosName);

    IoDeleteDevice(pDrvObj->DeviceObject);
}


/**
 * Create (i.e. Open) file entry point.
 *
 * @param   pDevObj     Device object.
 * @param   pIrp        Request packet.
 */
NTSTATUS _stdcall VBoxUSBMonCreate(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    PIO_STACK_LOCATION  pStack = IoGetCurrentIrpStackLocation(pIrp);
    PDEVICE_EXTENSION   pDevExt = (PDEVICE_EXTENSION)pDevObj->DeviceExtension;
    NTSTATUS            status;

    DebugPrint(("VBoxUSBMonCreate\n"));

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

    VBoxUSBCreate();

    if (InterlockedIncrement(&pDevExt->cOpened) == 1)
    {
        UNICODE_STRING  szStandardHubName;

        pDevExt->fHookDevice = TRUE;

        szStandardHubName.Length = 0;
        szStandardHubName.MaximumLength = 0;
        szStandardHubName.Buffer = 0;
        RtlInitUnicodeString(&szStandardHubName, L"\\Driver\\usbhub");

        DebugPrint(("Hook USB hub drivers\n"));
        for (int i = 0; i < RT_ELEMENTS(InstalledPDOHooks); i++)
        {
            char            szHubName[32];
            UNICODE_STRING  UnicodeName;
            ANSI_STRING     AnsiName;
            PDEVICE_OBJECT  pHubDevObj;
            PFILE_OBJECT    pHubFileObj;

            sprintf(szHubName, "\\Device\\USBPDO-%d", i);

            InstalledPDOHooks[i].fValid = false;

            UnicodeName.Length = 0;
            UnicodeName.MaximumLength = 0;
            UnicodeName.Buffer = 0;

            RtlInitAnsiString(&AnsiName, szHubName);
            RtlAnsiStringToUnicodeString(&UnicodeName, &AnsiName, TRUE);

            status = IoGetDeviceObjectPointer(&UnicodeName, FILE_READ_DATA, &pHubFileObj, &pHubDevObj);
            if (status == STATUS_SUCCESS)
            {
                DebugPrint(("IoGetDeviceObjectPointer for %s returned %p %p\n", szHubName, pHubDevObj, pHubFileObj));

                if (    pHubDevObj->DriverObject
                    &&  pHubDevObj->DriverObject->DriverName.Buffer
                    &&  pHubDevObj->DriverObject->DriverName.Length
                    &&  !RtlCompareUnicodeString(&szStandardHubName, &pHubDevObj->DriverObject->DriverName, TRUE /* case insensitive */))
                {
#if 0
                    DebugPrint(("Associated driver"));
                    DebugPrintUnicodeString(&pHubDevObj->DriverObject->DriverName);
#endif
                    DebugPrint(("pnp handler %p\n", pHubDevObj->DriverObject->MajorFunction[IRP_MJ_PNP]));
                    if (    !pfnOldPnPHandler
                        ||  pHubDevObj->DriverObject->MajorFunction[IRP_MJ_PNP] == pfnOldPnPHandler)
                    {
                        InstalledPDOHooks[i].fValid = true;
                        pfnOldPnPHandler = (PDRIVER_DISPATCH)InterlockedExchangePointer((PVOID *)&pHubDevObj->DriverObject->MajorFunction[IRP_MJ_PNP], VBoxUSBMonPnPHook);
                    }
                }
                ObDereferenceObject(pHubFileObj);
            }
//            else
//                DebugPrint(("IoGetDeviceObjectPointer for %s returned %x\n", szHubName, status));

            RtlFreeUnicodeString(&UnicodeName);
        }

        //
        // Let us use remove lock to keep count of IRPs so that we don't
        // detach and delete our deviceobject until all pending I/Os in our
        // devstack are completed. Remlock is required to protect us from
        // various race conditions where our driver can get unloaded while we
        // are still running dispatch or completion code.
        //

        IoInitializeRemoveLock (&pDevExt->RemoveLock , POOL_TAG,
                                1,      // MaxLockedMinutes
                                100);   // HighWatermark, this parameter is
                                        // used only on checked build. Specifies
                                        // the maximum number of outstanding
                                        // acquisitions allowed on the lock

        DebugPrint(("VBoxUSBMon: remove lock = %x\n", pDevExt->RemoveLock.Common.RemoveEvent));
    }

    status = IoAcquireRemoveLock(&pDevExt->RemoveLock, ControlDeviceObject);
    if (!NT_SUCCESS(status))
    {
        DebugPrint(("IoAcquireRemoveLock failed with %x\n", status));
        pIrp->IoStatus.Status = status;
        IoCompleteRequest(pIrp, IO_NO_INCREMENT);
        return status;
    }

    pIrp->IoStatus.Information  = 0;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;
}


/**
 * Close file entry point.
 *
 * @param   pDevObj     Device object.
 * @param   pIrp        Request packet.
 */
NTSTATUS _stdcall VBoxUSBMonClose(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    PDEVICE_EXTENSION   pDevExt  = (PDEVICE_EXTENSION)pDevObj->DeviceExtension;
    PIO_STACK_LOCATION  pStack   = IoGetCurrentIrpStackLocation(pIrp);
    PFILE_OBJECT        pFileObj = pStack->FileObject;

    DebugPrint(("VBoxUSBMonClose: pDevExt=%p pFileObj=%p pSession=%p\n", pDevExt, pFileObj, pFileObj->FsContext));

    /* Uninstall hook when we close the last instance. */
    if (InterlockedDecrement(&pDevExt->cOpened) == 0)
        VBoxUSBUninstallPnPHook(pDevExt);

    VBoxUSBClose();

    /* Wait for all outstanding requests to complete */
    /* We're supposed to use it in the remove device function. During unload
     * is no option as that crashes Vista64
     */
    DebugPrint(("Waiting for outstanding requests\n"));
    IoReleaseRemoveLockAndWait(&pDevExt->RemoveLock, ControlDeviceObject);

    DebugPrint(("Done waiting.\n"));
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
NTSTATUS _stdcall VBoxUSBMonDeviceControl(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    PDEVICE_EXTENSION   pDevExt = (PDEVICE_EXTENSION)pDevObj->DeviceExtension;
    PIO_STACK_LOCATION  pStack = IoGetCurrentIrpStackLocation(pIrp);
    NTSTATUS            status = STATUS_SUCCESS;

    status = IoAcquireRemoveLock(&pDevExt->RemoveLock, ControlDeviceObject);
    if (!NT_SUCCESS(status))
    {
        DebugPrint(("IoAcquireRemoveLock failed with %x\n", status));
        pIrp->IoStatus.Status = status;
        IoCompleteRequest(pIrp, IO_NO_INCREMENT);
        return status;
    }

    /* Handle ioctl meant for us */
    pIrp->IoStatus.Information = 0;
    status = VBoxUSBDispatchIO(pDevObj, pIrp);
    pIrp->IoStatus.Status = status;
    IoCompleteRequest (pIrp, IO_NO_INCREMENT);

    /* Release the remove lock */
    IoReleaseRemoveLock(&pDevExt->RemoveLock, ControlDeviceObject);
    return status;
}

/**
 * Pass on or refuse entry point
 *
 * @param   pDevObj     Device object.
 * @param   pIrp        Request packet.
 */
NTSTATUS _stdcall VBoxUSBMonStub(IN PDEVICE_OBJECT DeviceObject, IN PIRP pIrp)
{
#ifdef DEBUG
    PIO_STACK_LOCATION          irpStack;

    irpStack = IoGetCurrentIrpStackLocation(pIrp);
    DebugPrint (("VBoxUSBMonStub %x\n", irpStack->MinorFunction));
#endif

    /* Meant for us; report error */
    pIrp->IoStatus.Information = 0;
    pIrp->IoStatus.Status = STATUS_NOT_SUPPORTED;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);

    return STATUS_NOT_SUPPORTED;
}


/**
 * Handle the specific PnP ioctls we need for stealing devices.
 *
 * @param   irpStack        Device object
 * @param   pIoStatus       IO status of finished IRP
 */
NTSTATUS _stdcall VBoxUSBMonHandlePnPIoctl(PIO_STACK_LOCATION irpStack, PIO_STATUS_BLOCK pIoStatus)
{
    DebugPrint(("VBoxUSBMonHandlePnPIoctl IRQL = %d\n", KeGetCurrentIrql()));
    switch(irpStack->MinorFunction)
    {
    case IRP_MN_QUERY_DEVICE_TEXT:
    {
        DebugPrint(("IRP_MN_QUERY_DEVICE_TEXT: pIoStatus->Status = %x\n", pIoStatus->Status));
        if (pIoStatus->Status == STATUS_SUCCESS)
        {
            WCHAR *pId = (WCHAR *)pIoStatus->Information;
            if (VALID_PTR(pId))
            {
                switch(irpStack->Parameters.QueryDeviceText.DeviceTextType)
                {
                case DeviceTextLocationInformation:
                    DebugPrint(("DeviceTextLocationInformation %ws\n", pId));
                    break;

                case DeviceTextDescription:
                    DebugPrint(("DeviceTextDescription %ws\n", pId));
                    if (VBoxUSBDeviceIsCaptured(irpStack->DeviceObject))
                    {
                        WCHAR *pId = (WCHAR *)ExAllocatePool(PagedPool, sizeof(szDeviceTextDescription));
                        if (!pId)
                        {
                            AssertFailed();
                            break;
                        }
                        memcpy(pId, szDeviceTextDescription, sizeof(szDeviceTextDescription));
                        DebugPrint(("NEW szDeviceTextDescription %ws\n", pId));
                        ExFreePool((PVOID)pIoStatus->Information);
                        pIoStatus->Information = (ULONG_PTR)pId;
                    }
                    break;
                }
            }
            else
                DebugPrint(("Invalid pointer %p\n", pId));
        }
        break;
    }

    case IRP_MN_QUERY_ID:
    {
        DebugPrint(("IRP_MN_QUERY_ID: Irp->pIoStatus->Status = %x\n", pIoStatus->Status));
        if (    pIoStatus->Status == STATUS_SUCCESS
            &&  irpStack->DeviceObject)
        {
            WCHAR *pId = (WCHAR *)pIoStatus->Information;
#ifdef DEBUG
            WCHAR *pTmp;
#endif
            if (VALID_PTR(pId))
            {
                switch(irpStack->Parameters.QueryDeviceRelations.Type)
                {
                case BusQueryInstanceID:
                    DebugPrint(("BusQueryInstanceID %ws\n", pId));
                    break;

                case BusQueryDeviceID:
                    VBoxUSBAddDevice(irpStack->DeviceObject);
                    if (VBoxMatchFilter(irpStack->DeviceObject) == true)
                    {
                        pId = (WCHAR *)ExAllocatePool(PagedPool, sizeof(szBusQueryDeviceId));
                        if (!pId)
                        {
                            AssertFailed();
                            break;
                        }
                        memcpy(pId, szBusQueryDeviceId, sizeof(szBusQueryDeviceId));
                        DebugPrint(("NEW BusQueryDeviceID %ws\n", pId));
                        ExFreePool((PVOID)pIoStatus->Information);
                        pIoStatus->Information = (ULONG_PTR)pId;
                        VBoxUSBCaptureDevice(irpStack->DeviceObject);
                    }
                    break;

                case BusQueryHardwareIDs:
#ifdef DEBUG
                    while(*pId) //MULTI_SZ
                    {
                        DebugPrint(("BusQueryHardwareIDs %ws\n", pId));
                        while(*pId) pId++;
                        pId++;
                    }
#endif
                    VBoxUSBAddDevice(irpStack->DeviceObject);
                    if (VBoxMatchFilter(irpStack->DeviceObject) == true)
                    {
                        pId = (WCHAR *)ExAllocatePool(PagedPool, sizeof(szBusQueryHardwareIDs));
                        if (!pId)
                        {
                            AssertFailed();
                            break;
                        }
                        memcpy(pId, szBusQueryHardwareIDs, sizeof(szBusQueryHardwareIDs));
#ifdef DEBUG
                        pTmp = pId;
                        while(*pTmp) //MULTI_SZ
                        {
                            DebugPrint(("NEW BusQueryHardwareIDs %ws\n", pTmp));
                            while(*pTmp) pTmp++;
                            pTmp++;
                        }
#endif
                        ExFreePool((PVOID)pIoStatus->Information);
                        pIoStatus->Information = (ULONG_PTR)pId;
                        VBoxUSBCaptureDevice(irpStack->DeviceObject);
                    }
                    break;

                case BusQueryCompatibleIDs:
#ifdef DEBUG
                    while(*pId) //MULTI_SZ
                    {
                        DebugPrint(("BusQueryCompatibleIDs %ws\n", pId));
                        while(*pId) pId++;
                        pId++;
                    }
#endif
                    if (VBoxUSBDeviceIsCaptured(irpStack->DeviceObject))
                    {
                        pId = (WCHAR *)ExAllocatePool(PagedPool, sizeof(szBusQueryCompatibleIDs));
                        if (!pId)
                        {
                            AssertFailed();
                            break;
                        }
                        memcpy(pId, szBusQueryCompatibleIDs, sizeof(szBusQueryCompatibleIDs));
#ifdef DEBUG
                        pTmp = pId;
                        while(*pTmp) //MULTI_SZ
                        {
                            DebugPrint(("NEW BusQueryCompatibleIDs %ws\n", pTmp));
                            while(*pTmp) pTmp++;
                            pTmp++;
                        }
#endif
                        ExFreePool((PVOID)pIoStatus->Information);
                        pIoStatus->Information = (ULONG_PTR)pId;
                    }
                    break;
                }
            }
            else
                DebugPrint(("Invalid pointer %p\n", pId));
        }
        break;
    }

#ifdef DEBUG
    case IRP_MN_QUERY_DEVICE_RELATIONS:
    {
        switch(irpStack->Parameters.QueryDeviceRelations.Type)
        {
        case BusRelations:
        {
            DebugPrint(("BusRelations\n"));

            if (pIoStatus->Status == STATUS_SUCCESS)
            {
                PDEVICE_RELATIONS pRel = (PDEVICE_RELATIONS)pIoStatus->Information;
                DebugPrint(("pRel = %p\n", pRel));
                if (VALID_PTR(pRel))
                {
                    for (unsigned i=0;i<pRel->Count;i++)
                    {
                        if (VBoxUSBDeviceIsCaptured(irpStack->DeviceObject))
                            DebugPrint(("New PDO %p\n", pRel->Objects[i]));
                    }
                }
                else
                    DebugPrint(("Invalid pointer %p\n", pRel));
            }
            break;
        }
        case TargetDeviceRelation:
            DebugPrint(("TargetDeviceRelation\n"));
            break;
        case RemovalRelations:
            DebugPrint(("RemovalRelations\n"));
            break;
        case EjectionRelations:
            DebugPrint(("EjectionRelations\n"));
            break;
        }
        break;
    }

    case IRP_MN_QUERY_CAPABILITIES:
    {
        DebugPrint(("IRP_MN_QUERY_CAPABILITIES: pIoStatus->Status = %x\n", pIoStatus->Status));
        if (pIoStatus->Status == STATUS_SUCCESS)
        {
            PDEVICE_CAPABILITIES pCaps = irpStack->Parameters.DeviceCapabilities.Capabilities;
            if (VALID_PTR(pCaps))
            {
                DebugPrint(("Caps.SilentInstall  = %d\n", pCaps->SilentInstall));
                DebugPrint(("Caps.UniqueID       = %d\n", pCaps->UniqueID ));
                DebugPrint(("Caps.Address        = %d\n", pCaps->Address ));
                DebugPrint(("Caps.UINumber       = %d\n", pCaps->UINumber ));
            }
            else
                DebugPrint(("Invalid pointer %p\n", pCaps));
        }
        break;
    }
#endif
    } /*switch */
    DebugPrint(("VBoxUSBMonHandlePnPIoctl returns %x (IRQL = %d)\n", pIoStatus->Status, KeGetCurrentIrql()));
    return pIoStatus->Status;
}

/**
 * IRP completion notification callback. Used for selected PNP irps.
 *
 * @param   pDevObj     Device object.(always NULL!)
 * @param   pIrp        Request packet.
 * @param   context     User parameter (old IRP)
 */
NTSTATUS _stdcall VBoxUSBPnPCompletion(DEVICE_OBJECT *pDevObj, IRP *pIrp, void *context)
{
    /* Note: pDevObj is NULL! */
    PIRP               pOrgIrp  = (PIRP)context;
    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(pOrgIrp);
    PDEVICE_EXTENSION  pDevExt  = (PDEVICE_EXTENSION)ControlDeviceObject->DeviceExtension;

    DebugPrint(("VBoxUSBPnPCompletion %p %p %p minor=%x\n", pDevObj, pIrp, context, irpStack->MinorFunction));

#ifdef USBMON_ASYNC
    VBoxUSBMonHandlePnPIoctl(irpStack, &pIrp->IoStatus);

    /* Copy back the result to the original IRP. */
    pOrgIrp->IoStatus.Information = pIrp->IoStatus.Information;
    pOrgIrp->IoStatus.Status      = pIrp->IoStatus.Status;

    /* Unlock & free mdls of our duplicate IRP. */
    while (pIrp->MdlAddress != NULL)
    {
        PMDL nextMdl;
        nextMdl = pIrp->MdlAddress->Next;
        DebugPrint(("Unlock & free MDL %p\n", pIrp->MdlAddress));
        MmUnlockPages(pIrp->MdlAddress);
        IoFreeMdl(pIrp->MdlAddress);
        pIrp->MdlAddress = nextMdl;
    }
    /* Free our duplicate IRP */
    IoFreeIrp(pIrp);

    /* Release the remove lock */
    IoReleaseRemoveLock(&pDevExt->RemoveLock, ControlDeviceObject);

    /* Complete the original request */
    IoCompleteRequest(pOrgIrp, IO_NO_INCREMENT);

    return STATUS_MORE_PROCESSING_REQUIRED; /* must return this as we allocated the IRP with IoBuildAsynchronousFsdRequest! */
#else
    return STATUS_CONTINUE_COMPLETION;
#endif
}

/**
 * Device PnP hook
 *
 * @param   pDevObj     Device object.
 * @param   pIrp         Request packet.
 */
NTSTATUS _stdcall VBoxUSBMonPnPHook(IN PDEVICE_OBJECT pDevObj, IN PIRP pIrp)
{
    PIO_STACK_LOCATION  irpStack = IoGetCurrentIrpStackLocation(pIrp);
    PDEVICE_EXTENSION   pDevExt  = (PDEVICE_EXTENSION)ControlDeviceObject->DeviceExtension;
    NTSTATUS            status;

    DebugPrint(("VBoxUSBMonPnPHook pDevObj=%p %s IRP:%p \n", pDevObj, PnPMinorFunctionString(irpStack->MinorFunction), pIrp));

    status = IoAcquireRemoveLock(&pDevExt->RemoveLock, ControlDeviceObject);
    if (!NT_SUCCESS(status))
    {
        DebugPrint(("IoAcquireRemoveLock failed with %x\n", status));
        pIrp->IoStatus.Status = status;
        IoCompleteRequest(pIrp, IO_NO_INCREMENT);
        return status;
    }

    if (pDevExt->fHookDevice == TRUE)
    {
        switch(irpStack->MinorFunction)
        {
        case IRP_MN_QUERY_DEVICE_TEXT:
        case IRP_MN_QUERY_ID:
#ifdef DEBUG
        /* hooking this IRP causes problems for some reason */
        //case IRP_MN_QUERY_DEVICE_RELATIONS:
        case IRP_MN_QUERY_CAPABILITIES:
#endif
        {
#ifdef USBMON_ASYNC
            PIRP                pNewIrp;
            PIO_STACK_LOCATION  newIrpStack;

            /* The driver verifier claims all PNP irps must have Status preset to STATUS_NOT_SUPPORTED */
            IoStatus.Status = STATUS_NOT_SUPPORTED;

            pNewIrp = IoBuildAsynchronousFsdRequest(IRP_MJ_PNP, pDevObj, NULL, 0, NULL, NULL);
            Assert(pNewIrp);
            if (!pNewIrp)
                break;

            /* Get the next stack location as that is used for the new irp */
            newIrpStack = IoGetNextIrpStackLocation(pNewIrp);
            Assert(newIrpStack);
            if (newIrpStack)
            {
                /* Make a copy of the original IRP */
                newIrpStack->MajorFunction = irpStack->MajorFunction;
                newIrpStack->MinorFunction = irpStack->MinorFunction;
                newIrpStack->Parameters    = irpStack->Parameters;
                newIrpStack->FileObject    = irpStack->FileObject;

                IoSetCompletionRoutine(pNewIrp, VBoxUSBPnPCompletion, pIrp, TRUE, TRUE, TRUE);

                /* Mark the original Irp as pending; will be completed above */
                IoMarkIrpPending(pIrp);

                pDevExt->fHookDevice = FALSE;
                status =  IoCallDriver(pDevObj, pNewIrp);
                pDevExt->fHookDevice = TRUE;
                return STATUS_PENDING;  /* always return this! */
            }
            else
            {
                IoFreeIrp(pNewIrp);
                break;
            }
#else
            PIRP                pNewIrp;
            PIO_STACK_LOCATION  newIrpStack;
            KEVENT              event;
            IO_STATUS_BLOCK     IoStatus;

            KeInitializeEvent(&event, NotificationEvent, FALSE);

            /* The driver verifier claims all PNP irps must have Status preset to STATUS_NOT_SUPPORTED */
            IoStatus.Status = STATUS_NOT_SUPPORTED;

            pNewIrp = IoBuildSynchronousFsdRequest(IRP_MJ_PNP, pDevObj, NULL, 0, NULL, &event, &IoStatus);
            Assert(pNewIrp);
            if (!pNewIrp)
                break;

            /* Get the next stack location as that is used for the new irp */
            newIrpStack = IoGetNextIrpStackLocation(pNewIrp);
            Assert(newIrpStack);
            if (newIrpStack)
            {
                /* Make a copy of the original IRP */
                newIrpStack->MajorFunction = irpStack->MajorFunction;
                newIrpStack->MinorFunction = irpStack->MinorFunction;
                newIrpStack->Parameters    = irpStack->Parameters;
                newIrpStack->FileObject    = irpStack->FileObject;

                IoSetCompletionRoutine(pNewIrp, VBoxUSBPnPCompletion, pIrp, TRUE, TRUE, TRUE);

                pDevExt->fHookDevice = FALSE;
                status =  IoCallDriver(pDevObj, pNewIrp);
                pDevExt->fHookDevice = TRUE;

                if (status == STATUS_PENDING)
                {
                    KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
                    status = IoStatus.Status;
                }

                if (status == STATUS_SUCCESS)
                {
                    VBoxUSBMonHandlePnPIoctl(irpStack, &IoStatus);
                }
                /* Copy back the result to the original IRP. */
                pIrp->IoStatus.Information = IoStatus.Information;
                pIrp->IoStatus.Status      = IoStatus.Status;

                /* Complete the original request */
                IoCompleteRequest(pIrp, IO_NO_INCREMENT);

                /* Release the remove lock */
                IoReleaseRemoveLock(&pDevExt->RemoveLock, ControlDeviceObject);

                return status;
            }
            else
            {
                IoFreeIrp(pNewIrp);
                break;
            }
#endif
        }

        case IRP_MN_SURPRISE_REMOVAL:
        case IRP_MN_REMOVE_DEVICE:
            VBoxUSBRemoveDevice(irpStack->DeviceObject);
            break;

        /* These two IRPs are received when the PnP subsystem has determined the id of the newly arrived device */
        /* IRP_MN_START_DEVICE only arrives if it's a USB device of a known class or with a present host driver */
        case IRP_MN_QUERY_RESOURCE_REQUIREMENTS:
        case IRP_MN_QUERY_RESOURCES:
            VBoxUSBDeviceArrived(irpStack->DeviceObject);
            break;

        default:
            break;
        }
    }
    status = pfnOldPnPHandler(pDevObj, pIrp);
    IoReleaseRemoveLock(&pDevExt->RemoveLock, ControlDeviceObject);
    return status;
}


/**
 * Send IRP_MN_QUERY_DEVICE_RELATIONS
 *
 * @returns NT Status
 * @param   pDevObj         USB device pointer
 * @param   pFileObj        Valid file object pointer
 * @param   pDevRelations   Pointer to DEVICE_RELATIONS pointer (out)
 */
NTSTATUS VBoxUSBQueryBusRelations(PDEVICE_OBJECT pDevObj, PFILE_OBJECT pFileObj, PDEVICE_RELATIONS *pDevRelations)
{
    PDEVICE_EXTENSION   pDevExt  = (PDEVICE_EXTENSION)ControlDeviceObject->DeviceExtension;
    IO_STATUS_BLOCK     IoStatus;
    KEVENT              event;
    NTSTATUS            status;
    IRP                *pIrp;
    PIO_STACK_LOCATION  irpStack;

    KeInitializeEvent(&event, NotificationEvent, FALSE);

    Assert(pDevRelations);
    *pDevRelations = NULL;

    pIrp = IoBuildSynchronousFsdRequest(IRP_MJ_PNP, pDevObj, NULL, 0, NULL, &event, &IoStatus);
    if (!pIrp)
    {
        AssertMsgFailed(("IoBuildDeviceIoControlRequest failed!!\n"));
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    IoStatus.Status = STATUS_NOT_SUPPORTED;

    /* Get the next stack location as that is used for the new irp */
    irpStack = IoGetNextIrpStackLocation(pIrp);
    irpStack->MajorFunction  = IRP_MJ_PNP;
    irpStack->MinorFunction  = IRP_MN_QUERY_DEVICE_RELATIONS;
    irpStack->Parameters.QueryDeviceRelations.Type = BusRelations;
    irpStack->FileObject     = pFileObj;

    pDevExt->fHookDevice = FALSE;
    status = IoCallDriver(pDevObj, pIrp);
    pDevExt->fHookDevice = TRUE;
    if (status == STATUS_PENDING)
    {
        DebugPrint(("IoCallDriver returned STATUS_PENDING!!\n"));
        KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
        status = IoStatus.Status;
    }

    if (status == STATUS_SUCCESS)
    {
        PDEVICE_RELATIONS pRel = (PDEVICE_RELATIONS)IoStatus.Information;
        DebugPrint(("pRel = %p\n", pRel));
        if (VALID_PTR(pRel))
        {
            *pDevRelations = pRel;
        }
        else
            DebugPrint(("Invalid pointer %p\n", pRel));
    }

    DebugPrint(("IoCallDriver returned %x\n", status));
    return status;
}

#ifdef DEBUG

PCHAR PnPMinorFunctionString(UCHAR MinorFunction)
{
    switch (MinorFunction)
    {
        case IRP_MN_START_DEVICE:
            return "IRP_MN_START_DEVICE";
        case IRP_MN_QUERY_REMOVE_DEVICE:
            return "IRP_MN_QUERY_REMOVE_DEVICE";
        case IRP_MN_REMOVE_DEVICE:
            return "IRP_MN_REMOVE_DEVICE";
        case IRP_MN_CANCEL_REMOVE_DEVICE:
            return "IRP_MN_CANCEL_REMOVE_DEVICE";
        case IRP_MN_STOP_DEVICE:
            return "IRP_MN_STOP_DEVICE";
        case IRP_MN_QUERY_STOP_DEVICE:
            return "IRP_MN_QUERY_STOP_DEVICE";
        case IRP_MN_CANCEL_STOP_DEVICE:
            return "IRP_MN_CANCEL_STOP_DEVICE";
        case IRP_MN_QUERY_DEVICE_RELATIONS:
            return "IRP_MN_QUERY_DEVICE_RELATIONS";
        case IRP_MN_QUERY_INTERFACE:
            return "IRP_MN_QUERY_INTERFACE";
        case IRP_MN_QUERY_CAPABILITIES:
            return "IRP_MN_QUERY_CAPABILITIES";
        case IRP_MN_QUERY_RESOURCES:
            return "IRP_MN_QUERY_RESOURCES";
        case IRP_MN_QUERY_RESOURCE_REQUIREMENTS:
            return "IRP_MN_QUERY_RESOURCE_REQUIREMENTS";
        case IRP_MN_QUERY_DEVICE_TEXT:
            return "IRP_MN_QUERY_DEVICE_TEXT";
        case IRP_MN_FILTER_RESOURCE_REQUIREMENTS:
            return "IRP_MN_FILTER_RESOURCE_REQUIREMENTS";
        case IRP_MN_READ_CONFIG:
            return "IRP_MN_READ_CONFIG";
        case IRP_MN_WRITE_CONFIG:
            return "IRP_MN_WRITE_CONFIG";
        case IRP_MN_EJECT:
            return "IRP_MN_EJECT";
        case IRP_MN_SET_LOCK:
            return "IRP_MN_SET_LOCK";
        case IRP_MN_QUERY_ID:
            return "IRP_MN_QUERY_ID";
        case IRP_MN_QUERY_PNP_DEVICE_STATE:
            return "IRP_MN_QUERY_PNP_DEVICE_STATE";
        case IRP_MN_QUERY_BUS_INFORMATION:
            return "IRP_MN_QUERY_BUS_INFORMATION";
        case IRP_MN_DEVICE_USAGE_NOTIFICATION:
            return "IRP_MN_DEVICE_USAGE_NOTIFICATION";
        case IRP_MN_SURPRISE_REMOVAL:
            return "IRP_MN_SURPRISE_REMOVAL";

        default:
            return "unknown_pnp_irp";
    }
}

void DebugPrintUnicodeString(PUNICODE_STRING pString)
{
    int i;

    for (i=0;i<pString->Length/2;i++)
    {
        DebugPrint(("%c", pString->Buffer[i]));
    }
}

#endif

