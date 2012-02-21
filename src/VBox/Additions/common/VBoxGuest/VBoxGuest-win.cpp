/** @file
 *
 * VBoxGuest - Windows specifics.
 *
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_SUP_DRV
#include "VBoxGuest-win.h"
#include "VBoxGuestInternal.h"

#include <iprt/asm.h>

#include <VBox/log.h>
#include <VBox/VBoxGuestLib.h>

/*
 * XP DDK #defines ExFreePool to ExFreePoolWithTag. The latter does not exist
 * on NT4, so... The same for ExAllocatePool.
 */
#ifdef TARGET_NT4
# undef ExAllocatePool
# undef ExFreePool
#endif

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
RT_C_DECLS_BEGIN
static NTSTATUS vboxguestwinAddDevice(PDRIVER_OBJECT pDrvObj, PDEVICE_OBJECT pDevObj);
static void     vboxguestwinUnload(PDRIVER_OBJECT pDrvObj);
static NTSTATUS vboxguestwinCreate(PDEVICE_OBJECT pDevObj, PIRP pIrp);
static NTSTATUS vboxguestwinClose(PDEVICE_OBJECT pDevObj, PIRP pIrp);
static NTSTATUS vboxguestwinIOCtl(PDEVICE_OBJECT pDevObj, PIRP pIrp);
static NTSTATUS vboxguestwinInternalIOCtl(PDEVICE_OBJECT pDevObj, PIRP pIrp);
static NTSTATUS vboxguestwinRegistryReadDWORD(ULONG ulRoot, PCWSTR pwszPath, PWSTR pwszName, PULONG puValue);
static NTSTATUS vboxguestwinSystemControl(PDEVICE_OBJECT pDevObj, PIRP pIrp);
static NTSTATUS vboxguestwinShutdown(PDEVICE_OBJECT pDevObj, PIRP pIrp);
static NTSTATUS vboxguestwinNotSupportedStub(PDEVICE_OBJECT pDevObj, PIRP pIrp);
#ifdef DEBUG
static void     vboxguestwinDoTests(void);
#endif
RT_C_DECLS_END


/*******************************************************************************
*   Exported Functions                                                         *
*******************************************************************************/
RT_C_DECLS_BEGIN
ULONG DriverEntry(PDRIVER_OBJECT pDrvObj, PUNICODE_STRING pRegPath);
RT_C_DECLS_END

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, vboxguestwinAddDevice)
#pragma alloc_text (PAGE, vboxguestwinUnload)
#pragma alloc_text (PAGE, vboxguestwinCreate)
#pragma alloc_text (PAGE, vboxguestwinClose)
#pragma alloc_text (PAGE, vboxguestwinIOCtl)
#pragma alloc_text (PAGE, vboxguestwinShutdown)
#pragma alloc_text (PAGE, vboxguestwinNotSupportedStub)
#pragma alloc_text (PAGE, vboxguestwinScanPCIResourceList)
#endif

/** The detected Windows version. */
winVersion_t g_winVersion;

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

    Log(("VBoxGuest::DriverEntry. Driver built: %s %s\n", __DATE__, __TIME__));

    ULONG majorVersion;
    ULONG minorVersion;
    ULONG buildNumber;
    BOOLEAN bCheckedBuild = PsGetVersion(&majorVersion, &minorVersion, &buildNumber, NULL);
    Log(("VBoxGuest::DriverEntry: Running on Windows NT version %d.%d, build %d\n", majorVersion, minorVersion, buildNumber));
    if (bCheckedBuild)
        Log(("VBoxGuest::DriverEntry: Running on a Windows checked build (debug)!\n"));
#ifdef DEBUG
    vboxguestwinDoTests();
#endif
    switch (majorVersion)
    {
        case 6: /* Windows Vista or Windows 7 (based on minor ver) */
            switch (minorVersion)
            {
                case 0: /* Note: Also could be Windows 2008 Server! */
                    g_winVersion = WINVISTA;
                    break;
                case 1: /* Note: Also could be Windows 2008 Server R2! */
                    g_winVersion = WIN7;
                    break;
                case 2:
                    g_winVersion = WIN8;
                    break;
                default:
                    Log(("VBoxGuest::DriverEntry: Unknown version of Windows (%u.%u), refusing!\n",
                         majorVersion, minorVersion));
                    rc = STATUS_DRIVER_UNABLE_TO_LOAD;
                    break;
            }
            break;
        case 5:
            switch (minorVersion)
            {
                case 2:
                    g_winVersion = WIN2K3;
                    break;
                case 1:
                    g_winVersion = WINXP;
                    break;
                case 0:
                    g_winVersion = WIN2K;
                    break;
                default:
                    Log(("VBoxGuest::DriverEntry: Unknown version of Windows (%u.%u), refusing!\n",
                         majorVersion, minorVersion));
                    rc = STATUS_DRIVER_UNABLE_TO_LOAD;
            }
            break;
        case 4:
            g_winVersion = WINNT4;
            break;
        default:
            Log(("VBoxGuest::DriverEntry: At least Windows NT4 required!\n"));
            rc = STATUS_DRIVER_UNABLE_TO_LOAD;
    }

    if (NT_SUCCESS(rc))
    {
        /*
         * Setup the driver entry points in pDrvObj.
         */
        pDrvObj->DriverUnload                                  = vboxguestwinUnload;
        pDrvObj->MajorFunction[IRP_MJ_CREATE]                  = vboxguestwinCreate;
        pDrvObj->MajorFunction[IRP_MJ_CLOSE]                   = vboxguestwinClose;
        pDrvObj->MajorFunction[IRP_MJ_DEVICE_CONTROL]          = vboxguestwinIOCtl;
        pDrvObj->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = vboxguestwinInternalIOCtl;
        pDrvObj->MajorFunction[IRP_MJ_SHUTDOWN]                = vboxguestwinShutdown;
        pDrvObj->MajorFunction[IRP_MJ_READ]                    = vboxguestwinNotSupportedStub;
        pDrvObj->MajorFunction[IRP_MJ_WRITE]                   = vboxguestwinNotSupportedStub;
#ifdef TARGET_NT4
        rc = vboxguestwinnt4CreateDevice(pDrvObj, NULL /* pDevObj */, pRegPath);
#else
        pDrvObj->MajorFunction[IRP_MJ_PNP]                     = vboxguestwinPnP;
        pDrvObj->MajorFunction[IRP_MJ_POWER]                   = vboxguestwinPower;
        pDrvObj->MajorFunction[IRP_MJ_SYSTEM_CONTROL]          = vboxguestwinSystemControl;
        pDrvObj->DriverExtension->AddDevice                    = (PDRIVER_ADD_DEVICE)vboxguestwinAddDevice;
#endif
    }

    Log(("VBoxGuest::DriverEntry returning %#x\n", rc));
    return rc;
}


#ifndef TARGET_NT4
/**
 * Handle request from the Plug & Play subsystem.
 *
 * @returns NT status code
 * @param  pDrvObj   Driver object
 * @param  pDevObj   Device object
 */
static NTSTATUS vboxguestwinAddDevice(PDRIVER_OBJECT pDrvObj, PDEVICE_OBJECT pDevObj)
{
    NTSTATUS rc;
    Log(("VBoxGuest::vboxguestwinGuestAddDevice\n"));

    /*
     * Create device.
     */
    PDEVICE_OBJECT pDeviceObject = NULL;
    PVBOXGUESTDEVEXT pDevExt = NULL;
    UNICODE_STRING devName;
    UNICODE_STRING win32Name;
    RtlInitUnicodeString(&devName, VBOXGUEST_DEVICE_NAME_NT);
    rc = IoCreateDevice(pDrvObj, sizeof(VBOXGUESTDEVEXT), &devName, FILE_DEVICE_UNKNOWN, 0, FALSE, &pDeviceObject);
    if (NT_SUCCESS(rc))
    {
        /*
         * Create symbolic link (DOS devices).
         */
        RtlInitUnicodeString(&win32Name, VBOXGUEST_DEVICE_NAME_DOS);
        rc = IoCreateSymbolicLink(&win32Name, &devName);
        if (NT_SUCCESS(rc))
        {
            /*
             * Setup the device extension.
             */
            pDevExt = (PVBOXGUESTDEVEXT)pDeviceObject->DeviceExtension;
            RtlZeroMemory(pDevExt, sizeof(VBOXGUESTDEVEXT));

            KeInitializeSpinLock(&pDevExt->win.s.MouseEventAccessLock);

            pDevExt->win.s.pDeviceObject = pDeviceObject;
            pDevExt->win.s.prevDevState = STOPPED;
            pDevExt->win.s.devState = STOPPED;

            pDevExt->win.s.pNextLowerDriver = IoAttachDeviceToDeviceStack(pDeviceObject, pDevObj);
            if (pDevExt->win.s.pNextLowerDriver == NULL)
            {
                Log(("VBoxGuest::vboxguestwinGuestAddDevice: IoAttachDeviceToDeviceStack did not give a nextLowerDriver!\n"));
                rc = STATUS_DEVICE_NOT_CONNECTED;
            }
        }
        else
            Log(("VBoxGuest::vboxguestwinGuestAddDevice: IoCreateSymbolicLink failed with rc=%#x!\n", rc));
    }
    else
        Log(("VBoxGuest::vboxguestwinGuestAddDevice: IoCreateDevice failed with rc=%#x!\n", rc));

    if (NT_SUCCESS(rc))
    {
        /*
         * If we reached this point we're fine with the basic driver setup,
         * so continue to init our own things.
         */
#ifdef VBOX_WITH_GUEST_BUGCHECK_DETECTION
        vboxguestwinBugCheckCallback(pDevExt); /* Ignore failure! */
#endif
        /* VBoxGuestPower is pageable; ensure we are not called at elevated IRQL */
        pDeviceObject->Flags |= DO_POWER_PAGABLE;

        /* Driver is ready now. */
        pDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
    }

    /* Cleanup on error. */
    if (NT_ERROR(rc))
    {
        if (pDevExt)
        {
            if (pDevExt->win.s.pNextLowerDriver)
                IoDetachDevice(pDevExt->win.s.pNextLowerDriver);
        }
        IoDeleteSymbolicLink(&win32Name);
        if (pDeviceObject)
            IoDeleteDevice(pDeviceObject);
    }

    Log(("VBoxGuest::vboxguestwinGuestAddDevice: returning with rc = 0x%x\n", rc));
    return rc;
}
#endif


/**
 * Debug helper to dump a device resource list.
 *
 * @param pResourceList  list of device resources.
 */
static void vboxguestwinShowDeviceResources(PCM_PARTIAL_RESOURCE_LIST pResourceList)
{
#ifdef LOG_ENABLED
    PCM_PARTIAL_RESOURCE_DESCRIPTOR resource = pResourceList->PartialDescriptors;
    ULONG nres = pResourceList->Count;
    ULONG i;

    for (i = 0; i < nres; ++i, ++resource)
    {
        ULONG uType = resource->Type;
        static char* aszName[] =
        {
            "CmResourceTypeNull",
            "CmResourceTypePort",
            "CmResourceTypeInterrupt",
            "CmResourceTypeMemory",
            "CmResourceTypeDma",
            "CmResourceTypeDeviceSpecific",
            "CmResourceTypeBusNumber",
            "CmResourceTypeDevicePrivate",
            "CmResourceTypeAssignedResource",
            "CmResourceTypeSubAllocateFrom",
        };

        Log(("VBoxGuest::vboxguestwinShowDeviceResources: Type %s",
               uType < (sizeof(aszName) / sizeof(aszName[0]))
             ? aszName[uType] : "Unknown"));

        switch (uType)
        {
            case CmResourceTypePort:
            case CmResourceTypeMemory:
                Log(("VBoxGuest::vboxguestwinShowDeviceResources: Start %8X%8.8lX length %X\n",
                         resource->u.Port.Start.HighPart, resource->u.Port.Start.LowPart,
                         resource->u.Port.Length));
                break;

            case CmResourceTypeInterrupt:
                Log(("VBoxGuest::vboxguestwinShowDeviceResources: Level %X, Vector %X, Affinity %X\n",
                         resource->u.Interrupt.Level, resource->u.Interrupt.Vector,
                         resource->u.Interrupt.Affinity));
                break;

            case CmResourceTypeDma:
                Log(("VBoxGuest::vboxguestwinShowDeviceResources: Channel %d, Port %X\n",
                         resource->u.Dma.Channel, resource->u.Dma.Port));
                break;

            default:
                Log(("\n"));
                break;
        }
    }
#endif
}


/**
 * Global initialisation stuff (PnP + NT4 legacy).
 *
 * @param  pDevObj    Device object.
 * @param  pIrp       Request packet.
 */
#ifndef TARGET_NT4
NTSTATUS vboxguestwinInit(PDEVICE_OBJECT pDevObj, PIRP pIrp)
#else
NTSTATUS vboxguestwinInit(PDRIVER_OBJECT pDrvObj, PDEVICE_OBJECT pDevObj, PUNICODE_STRING pRegPath)
#endif
{
    PVBOXGUESTDEVEXT pDevExt   = (PVBOXGUESTDEVEXT)pDevObj->DeviceExtension;
#ifndef TARGET_NT4
    PIO_STACK_LOCATION pStack  = IoGetCurrentIrpStackLocation(pIrp);
#endif

    Log(("VBoxGuest::vboxguestwinInit\n"));

    int rc = STATUS_SUCCESS;
#ifdef TARGET_NT4
    /*
     * Let's have a look at what our PCI adapter offers.
     */
    Log(("VBoxGuest::vboxguestwinInit: Starting to scan PCI resources of VBoxGuest ...\n"));

    /* Assign the PCI resources. */
    PCM_RESOURCE_LIST pResourceList = NULL;
    UNICODE_STRING classNameString;
    RtlInitUnicodeString(&classNameString, L"VBoxGuestAdapter");
    rc = HalAssignSlotResources(pRegPath, &classNameString,
                                pDrvObj, pDevObj,
                                PCIBus, pDevExt->win.s.busNumber, pDevExt->win.s.slotNumber,
                                &pResourceList);
    if (pResourceList && pResourceList->Count > 0)
        vboxguestwinShowDeviceResources(&pResourceList->List[0].PartialResourceList);
    if (NT_SUCCESS(rc))
        rc = vboxguestwinScanPCIResourceList(pResourceList, pDevExt);
#else
    if (pStack->Parameters.StartDevice.AllocatedResources->Count > 0)
        vboxguestwinShowDeviceResources(&pStack->Parameters.StartDevice.AllocatedResources->List[0].PartialResourceList);
    if (NT_SUCCESS(rc))
        rc = vboxguestwinScanPCIResourceList(pStack->Parameters.StartDevice.AllocatedResourcesTranslated,
                                             pDevExt);
#endif
    if (NT_SUCCESS(rc))
    {
        /*
         * Map physical address of VMMDev memory into MMIO region
         * and init the common device extension bits.
         */
        void *pvMMIOBase = NULL;
        uint32_t cbMMIO = 0;
        rc = vboxguestwinMapVMMDevMemory(pDevExt,
                                         pDevExt->win.s.vmmDevPhysMemoryAddress,
                                         pDevExt->win.s.vmmDevPhysMemoryLength,
                                         &pvMMIOBase,
                                         &cbMMIO);
        if (NT_SUCCESS(rc))
        {
            pDevExt->pVMMDevMemory = (VMMDevMemory *)pvMMIOBase;

            Log(("VBoxGuest::vboxguestwinInit: pvMMIOBase = 0x%p, pDevExt = 0x%p, pDevExt->pVMMDevMemory = 0x%p\n",
                 pvMMIOBase, pDevExt, pDevExt ? pDevExt->pVMMDevMemory : NULL));

            int vrc = VBoxGuestInitDevExt(pDevExt,
                                          pDevExt->IOPortBase,
                                          pvMMIOBase, cbMMIO,
                                          vboxguestwinVersionToOSType(g_winVersion),
                                          VMMDEV_EVENT_MOUSE_POSITION_CHANGED);
            if (RT_FAILURE(vrc))
            {
                Log(("VBoxGuest::vboxguestwinInit: Could not init device extension, rc = %Rrc!\n", vrc));
                rc = STATUS_DEVICE_CONFIGURATION_ERROR;
            }
        }
        else
            Log(("VBoxGuest::vboxguestwinInit: Could not map physical address of VMMDev, rc = 0x%x!\n", rc));
    }

    if (NT_SUCCESS(rc))
    {
        int vrc = VbglGRAlloc((VMMDevRequestHeader **)&pDevExt->win.s.pPowerStateRequest,
                              sizeof (VMMDevPowerStateRequest), VMMDevReq_SetPowerStatus);
        if (RT_FAILURE(vrc))
        {
            Log(("VBoxGuest::vboxguestwinInit: Alloc for pPowerStateRequest failed, rc = %Rrc\n", vrc));
            rc = STATUS_UNSUCCESSFUL;
        }
    }

    if (NT_SUCCESS(rc))
    {
        /*
         * Register DPC and ISR.
         */
        Log(("VBoxGuest::vboxguestwinInit: Initializing DPC/ISR ...\n"));

        IoInitializeDpcRequest(pDevExt->win.s.pDeviceObject, vboxguestwinDpcHandler);
#ifdef TARGET_NT4
        ULONG uInterruptVector;
        KIRQL irqLevel;
        /* Get an interrupt vector. */
        /* Only proceed if the device provides an interrupt. */
        if (   pDevExt->win.s.interruptLevel
            || pDevExt->win.s.interruptVector)
        {
            Log(("VBoxGuest::vboxguestwinInit: Getting interrupt vector (HAL): Bus: %u, IRQL: %u, Vector: %u\n",
                 pDevExt->win.s.busNumber, pDevExt->win.s.interruptLevel, pDevExt->win.s.interruptVector));

            uInterruptVector = HalGetInterruptVector(PCIBus,
                                                     pDevExt->win.s.busNumber,
                                                     pDevExt->win.s.interruptLevel,
                                                     pDevExt->win.s.interruptVector,
                                                     &irqLevel,
                                                     &pDevExt->win.s.interruptAffinity);
            Log(("VBoxGuest::vboxguestwinInit: HalGetInterruptVector returns vector %u\n", uInterruptVector));
            if (uInterruptVector == 0)
                Log(("VBoxGuest::vboxguestwinInit: No interrupt vector found!\n"));
        }
        else
            Log(("VBoxGuest::vboxguestwinInit: Device does not provide an interrupt!\n"));
#endif
        if (pDevExt->win.s.interruptVector)
        {
            Log(("VBoxGuest::vboxguestwinInit: Connecting interrupt ...\n"));

            rc = IoConnectInterrupt(&pDevExt->win.s.pInterruptObject,          /* Out: interrupt object. */
                                    (PKSERVICE_ROUTINE)vboxguestwinIsrHandler, /* Our ISR handler. */
                                    pDevExt,                                   /* Device context. */
                                    NULL,                                      /* Optional spinlock. */
#ifdef TARGET_NT4
                                    uInterruptVector,                          /* Interrupt vector. */
                                    irqLevel,                                  /* Interrupt level. */
                                    irqLevel,                                  /* Interrupt level. */
#else
                                    pDevExt->win.s.interruptVector,            /* Interrupt vector. */
                                    (KIRQL)pDevExt->win.s.interruptLevel,      /* Interrupt level. */
                                    (KIRQL)pDevExt->win.s.interruptLevel,      /* Interrupt level. */
#endif
                                    pDevExt->win.s.interruptMode,              /* LevelSensitive or Latched. */
                                    TRUE,                                      /* Shareable interrupt. */
                                    pDevExt->win.s.interruptAffinity,          /* CPU affinity. */
                                    FALSE);                                    /* Don't save FPU stack. */
            if (NT_ERROR(rc))
                Log(("VBoxGuest::vboxguestwinInit: Could not connect interrupt, rc = 0x%x\n", rc));
        }
        else
            Log(("VBoxGuest::vboxguestwinInit: No interrupt vector found!\n"));
    }


#ifdef VBOX_WITH_HGCM
    Log(("VBoxGuest::vboxguestwinInit: Allocating kernel session data ...\n"));
    int vrc = VBoxGuestCreateKernelSession(pDevExt, &pDevExt->win.s.pKernelSession);
    if (RT_FAILURE(vrc))
    {
        Log(("VBoxGuest::vboxguestwinInit: Failed to allocated kernel session data! rc = %Rrc\n", rc));
        rc = STATUS_UNSUCCESSFUL;
    }
#endif

    if (RT_SUCCESS(rc))
    {
        ULONG ulValue = 0;
        NTSTATUS s = vboxguestwinRegistryReadDWORD(RTL_REGISTRY_SERVICES, L"VBoxGuest", L"LoggingEnabled",
                                                   &ulValue);
        if (NT_SUCCESS(s))
        {
            pDevExt->fLoggingEnabled = ulValue >= 0xFF;
            if (pDevExt->fLoggingEnabled)
                Log(("Logging to release log enabled (0x%x)", ulValue));
        }

        /* Ready to rumble! */
        Log(("VBoxGuest::vboxguestwinInit: Device is ready!\n"));
        VBOXGUEST_UPDATE_DEVSTATE(pDevExt, WORKING);
    }
    else
    {
        pDevExt->win.s.pInterruptObject = NULL;
    }

    Log(("VBoxGuest::vboxguestwinInit: Returned with rc = 0x%x\n", rc));
    return rc;
}


/**
 * Cleans up hardware resources.
 * Do not delete DevExt here.
 *
 * @param   pDrvObj     Driver object.
 */
NTSTATUS vboxguestwinCleanup(PDEVICE_OBJECT pDevObj)
{
    Log(("VBoxGuest::vboxguestwinCleanup\n"));

    PVBOXGUESTDEVEXT pDevExt = (PVBOXGUESTDEVEXT)pDevObj->DeviceExtension;
    if (pDevExt)
    {

#if 0 /* @todo: test & enable cleaning global session data */
#ifdef VBOX_WITH_HGCM
        if (pDevExt->win.s.pKernelSession)
        {
            VBoxGuestCloseSession(pDevExt, pDevExt->win.s.pKernelSession);
            pDevExt->win.s.pKernelSession = NULL;
        }
#endif
#endif

        if (pDevExt->win.s.pInterruptObject)
        {
            IoDisconnectInterrupt(pDevExt->win.s.pInterruptObject);
            pDevExt->win.s.pInterruptObject = NULL;
        }

        /* @todo: cleanup the rest stuff */


#ifdef VBOX_WITH_GUEST_BUGCHECK_DETECTION
        hlpDeregisterBugCheckCallback(pDevExt); /* ignore failure! */
#endif
        /* According to MSDN we have to unmap previously mapped memory. */
        vboxguestwinUnmapVMMDevMemory(pDevExt);
    }
    return STATUS_SUCCESS;
}


/**
 * Unload the driver.
 *
 * @param   pDrvObj     Driver object.
 */
static void vboxguestwinUnload(PDRIVER_OBJECT pDrvObj)
{
    Log(("VBoxGuest::vboxguestwinGuestUnload\n"));
#ifdef TARGET_NT4
    vboxguestwinCleanup(pDrvObj->DeviceObject);

    /* Destroy device extension and clean up everything else. */
    if (pDrvObj->DeviceObject && pDrvObj->DeviceObject->DeviceExtension)
        VBoxGuestDeleteDevExt((PVBOXGUESTDEVEXT)pDrvObj->DeviceObject->DeviceExtension);

    /*
     * I don't think it's possible to unload a driver which processes have
     * opened, at least we'll blindly assume that here.
     */
    UNICODE_STRING win32Name;
    RtlInitUnicodeString(&win32Name, VBOXGUEST_DEVICE_NAME_DOS);
    NTSTATUS rc = IoDeleteSymbolicLink(&win32Name);

    IoDeleteDevice(pDrvObj->DeviceObject);
#else /* TARGET_NT4 */
    /* On a PnP driver this routine will be called after
     * IRP_MN_REMOVE_DEVICE (where we already did the cleanup),
     * so don't do anything here (yet). */
#endif

    Log(("VBoxGuest::vboxguestwinGuestUnload: returning\n"));
}


/**
 * Create (i.e. Open) file entry point.
 *
 * @param   pDevObj     Device object.
 * @param   pIrp        Request packet.
 */
static NTSTATUS vboxguestwinCreate(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    /** @todo AssertPtrReturn(pIrp); */
    PIO_STACK_LOCATION pStack   = IoGetCurrentIrpStackLocation(pIrp);
    /** @todo AssertPtrReturn(pStack); */
    PFILE_OBJECT       pFileObj = pStack->FileObject;
    PVBOXGUESTDEVEXT   pDevExt  = (PVBOXGUESTDEVEXT)pDevObj->DeviceExtension;
    NTSTATUS           rc       = STATUS_SUCCESS;

    if (pDevExt->win.s.devState != WORKING)
    {
        Log(("VBoxGuest::vboxguestwinGuestCreate: device is not working currently: %d!\n",
             pDevExt->win.s.devState));
        rc = STATUS_UNSUCCESSFUL;
    }
    else if (pStack->Parameters.Create.Options & FILE_DIRECTORY_FILE)
    {
        /*
         * We are not remotely similar to a directory...
         * (But this is possible.)
         */
        Log(("VBoxGuest::vboxguestwinGuestCreate: Uhm, we're not a directory!\n"));
        rc = STATUS_NOT_A_DIRECTORY;
    }
    else
    {
#ifdef VBOX_WITH_HGCM
        if (pFileObj)
        {
            Log(("VBoxGuest::vboxguestwinGuestCreate: File object type = %d\n",
                 pFileObj->Type));

            int vrc;
            PVBOXGUESTSESSION pSession;
            if (pFileObj->Type == 5 /* File Object */)
            {
                /*
                 * Create a session object if we have a valid file object. This session object
                 * exists for every R3 process.
                 */
                vrc = VBoxGuestCreateUserSession(pDevExt, &pSession);
            }
            else
            {
                /* ... otherwise we've been called from R0! */
                vrc = VBoxGuestCreateKernelSession(pDevExt, &pSession);
            }
            if (RT_SUCCESS(vrc))
                pFileObj->FsContext = pSession;
        }
#endif
    }

    /* Complete the request! */
    pIrp->IoStatus.Information  = 0;
    pIrp->IoStatus.Status = rc;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);

    Log(("VBoxGuest::vboxguestwinGuestCreate: Returning 0x%x\n", rc));
    return rc;
}


/**
 * Close file entry point.
 *
 * @param   pDevObj     Device object.
 * @param   pIrp        Request packet.
 */
static NTSTATUS vboxguestwinClose(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    PVBOXGUESTDEVEXT   pDevExt  = (PVBOXGUESTDEVEXT)pDevObj->DeviceExtension;
    PIO_STACK_LOCATION pStack   = IoGetCurrentIrpStackLocation(pIrp);
    PFILE_OBJECT       pFileObj = pStack->FileObject;

    Log(("VBoxGuest::vboxguestwinGuestClose: pDevExt=0x%p pFileObj=0x%p FsContext=0x%p\n",
         pDevExt, pFileObj, pFileObj->FsContext));

#ifdef VBOX_WITH_HGCM
    /* Close both, R0 and R3 sessions. */
    PVBOXGUESTSESSION pSession = (PVBOXGUESTSESSION)pFileObj->FsContext;
    if (pSession)
        VBoxGuestCloseSession(pDevExt, pSession);
#endif

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
static NTSTATUS vboxguestwinIOCtl(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    NTSTATUS            Status   = STATUS_SUCCESS;
    PVBOXGUESTDEVEXT    pDevExt  = (PVBOXGUESTDEVEXT)pDevObj->DeviceExtension;
    PIO_STACK_LOCATION  pStack   = IoGetCurrentIrpStackLocation(pIrp);
    unsigned int        uCmd     = (unsigned int)pStack->Parameters.DeviceIoControl.IoControlCode;

    char               *pBuf     = (char *)pIrp->AssociatedIrp.SystemBuffer; /* All requests are buffered. */
    size_t              cbData   = pStack->Parameters.DeviceIoControl.InputBufferLength;
    unsigned            cbOut    = 0;

    /* Do we have a file object associated?*/
    PFILE_OBJECT        pFileObj = pStack->FileObject;
    PVBOXGUESTSESSION   pSession = NULL;
    if (pFileObj) /* ... then we might have a session object as well! */
        pSession = (PVBOXGUESTSESSION)pFileObj->FsContext;

    Log(("VBoxGuest::vboxguestwinIOCtl: uCmd=%u, pDevExt=0x%p, pSession=0x%p\n",
         uCmd, pDevExt, pSession));

    /* We don't have a session associated with the file object? So this seems
     * to be a kernel call then. */
    /** @todo r=bird: What on earth is this supposed to be? Each kernel session
     *        shall have its own context of course, no hacks, pleeease. */
    if (pSession == NULL)
    {
        Log(("VBoxGuest::vboxguestwinIOCtl: Using kernel session data ...\n"));
        pSession = pDevExt->win.s.pKernelSession;
    }

    /*
     * First process Windows specific stuff which cannot be handled
     * by the common code used on all other platforms. In the default case
     * we then finally handle the common cases.
     */
    switch (uCmd)
    {
#ifdef VBOX_WITH_VRDP_SESSION_HANDLING
        case VBOXGUEST_IOCTL_ENABLE_VRDP_SESSION:
        {
            LogRel(("VBoxGuest::vboxguestwinIOCtl: ENABLE_VRDP_SESSION: Currently: %sabled\n",
                    pDevExt->fVRDPEnabled? "en": "dis"));
            if (!pDevExt->fVRDPEnabled)
            {
                KUSER_SHARED_DATA *pSharedUserData = (KUSER_SHARED_DATA *)KI_USER_SHARED_DATA;

                pDevExt->fVRDPEnabled            = true;
                LogRel(("VBoxGuest::vboxguestwinIOCtl: ENABLE_VRDP_SESSION: Current active console ID: 0x%08X\n",
                        pSharedUserData->ActiveConsoleId));
                pDevExt->ulOldActiveConsoleId    = pSharedUserData->ActiveConsoleId;
                pSharedUserData->ActiveConsoleId = 2;
            }
            break;
        }

        case VBOXGUEST_IOCTL_DISABLE_VRDP_SESSION:
        {
            LogRel(("VBoxGuest::vboxguestwinIOCtl: DISABLE_VRDP_SESSION: Currently: %sabled\n",
                    pDevExt->fVRDPEnabled? "en": "dis"));
            if (pDevExt->fVRDPEnabled)
            {
                KUSER_SHARED_DATA *pSharedUserData = (KUSER_SHARED_DATA *)KI_USER_SHARED_DATA;

                pDevExt->fVRDPEnabled            = false;
                Log(("VBoxGuest::vboxguestwinIOCtl: DISABLE_VRDP_SESSION: Current active console ID: 0x%08X\n",
                     pSharedUserData->ActiveConsoleId));
                pSharedUserData->ActiveConsoleId = pDevExt->ulOldActiveConsoleId;
                pDevExt->ulOldActiveConsoleId    = 0;
            }
            break;
        }
#else
        /* Add at least one (bogus) fall through case to shut up MSVC! */
        case 0:
#endif
        default:
        {
            /*
             * Process the common IOCtls.
             */
            size_t cbDataReturned;
            int vrc = VBoxGuestCommonIOCtl(uCmd, pDevExt, pSession, pBuf, cbData, &cbDataReturned);

            Log(("VBoxGuest::vboxguestwinGuestDeviceControl: rc=%Rrc, pBuf=0x%p, cbData=%u, cbDataReturned=%u\n",
                 vrc, pBuf, cbData, cbDataReturned));

            if (RT_SUCCESS(vrc))
            {
                if (RT_UNLIKELY(cbDataReturned > cbData))
                {
                    Log(("VBoxGuest::vboxguestwinGuestDeviceControl: Too much output data %u - expected %u!\n", cbDataReturned, cbData));
                    cbDataReturned = cbData;
                    Status = STATUS_BUFFER_TOO_SMALL;
                }
                if (cbDataReturned > 0)
                    cbOut = cbDataReturned;
            }
            else
            {
                if (   vrc == VERR_NOT_SUPPORTED
                    || vrc == VERR_INVALID_PARAMETER)
                {
                    Status = STATUS_INVALID_PARAMETER;
                }
                else
                    Status = STATUS_UNSUCCESSFUL;
            }
            break;
        }
    }

    pIrp->IoStatus.Status = Status;
    pIrp->IoStatus.Information = cbOut;

    IoCompleteRequest(pIrp, IO_NO_INCREMENT);

    //Log(("VBoxGuest::vboxguestwinGuestDeviceControl: returned cbOut=%d rc=%#x\n", cbOut, Status));
    return Status;
}

/**
 * Internal Device I/O Control entry point.
 *
 * We do not want to allow some IOCTLs to be originated from user mode, this is
 * why we have a different entry point for internal IOCTLs.
 *
 * @param   pDevObj     Device object.
 * @param   pIrp        Request packet.
 *
 * @todo r=bird: This is no need for this extra function for the purpose of
 *       securing an IOCTL from user space access.  VBoxGuestCommonIOCtl
 *       has a way to do this already, see VBOXGUEST_IOCTL_GETVMMDEVPORT.
 */
static NTSTATUS vboxguestwinInternalIOCtl(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    NTSTATUS            Status      = STATUS_SUCCESS;
    PVBOXGUESTDEVEXT    pDevExt     = (PVBOXGUESTDEVEXT)pDevObj->DeviceExtension;
    PIO_STACK_LOCATION  pStack      = IoGetCurrentIrpStackLocation(pIrp);
    unsigned int        uCmd        = (unsigned int)pStack->Parameters.DeviceIoControl.IoControlCode;
    bool                fProcessed  = false;
    unsigned            Info        = 0;

    switch (uCmd)
    {
        case VBOXGUEST_IOCTL_INTERNAL_SET_MOUSE_NOTIFY_CALLBACK:
        {
            PVOID pvBuf = pStack->Parameters.Others.Argument1;
            size_t cbData = (size_t)pStack->Parameters.Others.Argument2;
            fProcessed = true;
            if (cbData != sizeof(VBoxGuestMouseSetNotifyCallback))
            {
                AssertFailed();
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            KIRQL OldIrql;
            VBoxGuestMouseSetNotifyCallback *pInfo = (VBoxGuestMouseSetNotifyCallback*)pvBuf;
            /* we need a lock here to avoid concurrency with the set event functionality */
            KeAcquireSpinLock(&pDevExt->win.s.MouseEventAccessLock, &OldIrql);
            pDevExt->win.s.pfnMouseNotify =  pInfo->pfnNotify;
            pDevExt->win.s.pvMouseNotify =  pInfo->pvNotify;
            KeReleaseSpinLock(&pDevExt->win.s.MouseEventAccessLock, OldIrql);

            Status = STATUS_SUCCESS;
            break;
        }

        default:
            break;
    }


    if (fProcessed)
    {
        pIrp->IoStatus.Status = Status;
        pIrp->IoStatus.Information = Info;

        IoCompleteRequest(pIrp, IO_NO_INCREMENT);
        return Status;
    }

    return vboxguestwinIOCtl(pDevObj, pIrp);
}


/**
 * IRP_MJ_SYSTEM_CONTROL handler.
 *
 * @returns NT status code
 * @param   pDevObj     Device object.
 * @param   pIrp        IRP.
 */
NTSTATUS vboxguestwinSystemControl(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    PVBOXGUESTDEVEXT pDevExt = (PVBOXGUESTDEVEXT)pDevObj->DeviceExtension;

    Log(("VBoxGuest::vboxguestwinGuestSystemControl\n"));

    /* Always pass it on to the next driver. */
    IoSkipCurrentIrpStackLocation(pIrp);

    return IoCallDriver(pDevExt->win.s.pNextLowerDriver, pIrp);
}


/**
 * IRP_MJ_SHUTDOWN handler.
 *
 * @returns NT status code
 * @param pDevObj    Device object.
 * @param pIrp       IRP.
 */
NTSTATUS vboxguestwinShutdown(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    PVBOXGUESTDEVEXT pDevExt = (PVBOXGUESTDEVEXT)pDevObj->DeviceExtension;

    Log(("VBoxGuest::vboxguestwinGuestShutdown\n"));

    VMMDevPowerStateRequest *pReq = pDevExt->win.s.pPowerStateRequest;
    if (pReq)
    {
        pReq->header.requestType = VMMDevReq_SetPowerStatus;
        pReq->powerState = VMMDevPowerState_PowerOff;

        int rc = VbglGRPerform(&pReq->header);
        if (RT_FAILURE(rc))
        {
            Log(("VBoxGuest::vboxguestwinGuestShutdown: Error performing request to VMMDev! "
                     "rc = %Rrc\n", rc));
        }
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
NTSTATUS vboxguestwinNotSupportedStub(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    Log(("VBoxGuest::vboxguestwinGuestNotSupportedStub\n"));

    pIrp->IoStatus.Information = 0;
    pIrp->IoStatus.Status = STATUS_NOT_SUPPORTED;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);

    return STATUS_NOT_SUPPORTED;
}


/**
 * DPC handler.
 *
 * @param   pDPC        DPC descriptor.
 * @param   pDevObj     Device object.
 * @param   pIrp        Interrupt request packet.
 * @param   pContext    Context specific pointer.
 */
void vboxguestwinDpcHandler(PKDPC pDPC, PDEVICE_OBJECT pDevObj,
                            PIRP pIrp, PVOID pContext)
{
    PVBOXGUESTDEVEXT pDevExt = (PVBOXGUESTDEVEXT)pDevObj->DeviceExtension;
    Log(("VBoxGuest::vboxguestwinGuestDpcHandler: pDevExt=0x%p\n", pDevExt));

    /* test & reset the counter */
    if (ASMAtomicXchgU32(&pDevExt->u32MousePosChangedSeq, 0))
    {
        Assert(KeGetCurrentIrql() == DISPATCH_LEVEL);
        /* we need a lock here to avoid concurrency with the set event ioctl handler thread,
         * i.e. to prevent the event from destroyed while we're using it */
        KeAcquireSpinLockAtDpcLevel(&pDevExt->win.s.MouseEventAccessLock);
        if (pDevExt->win.s.pfnMouseNotify)
        {
            pDevExt->win.s.pfnMouseNotify(pDevExt->win.s.pvMouseNotify);
        }
        KeReleaseSpinLockFromDpcLevel(&pDevExt->win.s.MouseEventAccessLock);
    }

    /* Process the wake-up list we were asked by the scheduling a DPC
     *  in vboxguestwinIsrHandler(). */
    VBoxGuestWaitDoWakeUps(pDevExt);
}


/**
 * ISR handler.
 *
 * @return  BOOLEAN         Indicates whether the IRQ came from us (TRUE) or not (FALSE).
 * @param   pInterrupt      Interrupt that was triggered.
 * @param   pServiceContext Context specific pointer.
 */
BOOLEAN vboxguestwinIsrHandler(PKINTERRUPT pInterrupt, PVOID pServiceContext)
{
    PVBOXGUESTDEVEXT pDevExt = (PVBOXGUESTDEVEXT)pServiceContext;
    if (pDevExt == NULL)
        return FALSE;

    /*Log(("VBoxGuest::vboxguestwinGuestIsrHandler: pDevExt = 0x%p, pVMMDevMemory = 0x%p\n",
             pDevExt, pDevExt ? pDevExt->pVMMDevMemory : NULL));*/

    /* Enter the common ISR routine and do the actual work. */
    BOOLEAN fIRQTaken = VBoxGuestCommonISR(pDevExt);

    /* If we need to wake up some events we do that in a DPC to make
     * sure we're called at the right IRQL. */
    if (fIRQTaken)
    {
        Log(("VBoxGuest::vboxguestwinGuestIsrHandler: IRQ was taken! pInterrupt = 0x%p, pDevExt = 0x%p\n",
             pInterrupt, pDevExt));
        if (ASMAtomicUoReadU32(&pDevExt->u32MousePosChangedSeq) || !RTListIsEmpty(&pDevExt->WakeUpList))
        {
            Log(("VBoxGuest::vboxguestwinGuestIsrHandler: Requesting DPC ...\n"));
            IoRequestDpc(pDevExt->win.s.pDeviceObject, pDevExt->win.s.pCurrentIrp, NULL);
        }
    }
    return fIRQTaken;
}


/*
 * Overridden routine for mouse polling events.
 *
 * @param pDevExt     Device extension structure.
 */
void VBoxGuestNativeISRMousePollEvent(PVBOXGUESTDEVEXT pDevExt)
{
    NOREF(pDevExt);
    /* nothing to do here - i.e. since we can not KeSetEvent from ISR level,
     * we rely on the pDevExt->u32MousePosChangedSeq to be set to a non-zero value on a mouse event
     * and queue the DPC in our ISR routine in that case doing KeSetEvent from the DPC routine */
}


/**
 * Queries (gets) a DWORD value from the registry.
 *
 * @return  NTSTATUS
 * @param   ulRoot      Relative path root. See RTL_REGISTRY_SERVICES or RTL_REGISTRY_ABSOLUTE.
 * @param   pwszPath    Path inside path root.
 * @param   pwszName    Actual value name to look up.
 * @param   puValue     On input this can specify the default value (if RTL_REGISTRY_OPTIONAL is
 *                      not specified in ulRoot), on output this will retrieve the looked up
 *                      registry value if found.
 */
NTSTATUS vboxguestwinRegistryReadDWORD(ULONG ulRoot, PCWSTR pwszPath, PWSTR pwszName,
                                       PULONG puValue)
{
    if (!pwszPath || !pwszName || !puValue)
        return STATUS_INVALID_PARAMETER;

    ULONG ulDefault = *puValue;

    RTL_QUERY_REGISTRY_TABLE  tblQuery[2];
    RtlZeroMemory(tblQuery, sizeof(tblQuery));
    /** @todo Add RTL_QUERY_REGISTRY_TYPECHECK! */
    tblQuery[0].Flags         = RTL_QUERY_REGISTRY_DIRECT;
    tblQuery[0].Name          = pwszName;
    tblQuery[0].EntryContext  = puValue;
    tblQuery[0].DefaultType   = REG_DWORD;
    tblQuery[0].DefaultData   = &ulDefault;
    tblQuery[0].DefaultLength = sizeof(ULONG);

    return RtlQueryRegistryValues(ulRoot,
                                  pwszPath,
                                  &tblQuery[0],
                                  NULL /* Context */,
                                  NULL /* Environment */);
}


/**
 * Helper to scan the PCI resource list and remember stuff.
 *
 * @param pResList  Resource list
 * @param pDevExt   Device extension
 */
NTSTATUS vboxguestwinScanPCIResourceList(PCM_RESOURCE_LIST pResList, PVBOXGUESTDEVEXT pDevExt)
{
    /* Enumerate the resource list. */
    Log(("VBoxGuest::vboxguestwinScanPCIResourceList: Found %d resources\n",
         pResList->List->PartialResourceList.Count));

    NTSTATUS rc = STATUS_SUCCESS;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR pPartialData = NULL;
    ULONG rangeCount = 0;
    ULONG cMMIORange = 0;
    PVBOXGUESTWINBASEADDRESS pBaseAddress = pDevExt->win.s.pciBaseAddress;
    for (ULONG i = 0; i < pResList->List->PartialResourceList.Count; i++)
    {
        pPartialData = &pResList->List->PartialResourceList.PartialDescriptors[i];
        switch (pPartialData->Type)
        {
            case CmResourceTypePort:
            {
                /* Overflow protection. */
                if (rangeCount < PCI_TYPE0_ADDRESSES)
                {
                    Log(("VBoxGuest::vboxguestwinScanPCIResourceList: I/O range: Base = %08x:%08x, Length = %08x\n",
                            pPartialData->u.Port.Start.HighPart,
                            pPartialData->u.Port.Start.LowPart,
                            pPartialData->u.Port.Length));

                    /* Save the IO port base. */
                    /** @todo Not so good. */
                    pDevExt->IOPortBase = (RTIOPORT)pPartialData->u.Port.Start.LowPart;

                    /* Save resource information. */
                    pBaseAddress->RangeStart     = pPartialData->u.Port.Start;
                    pBaseAddress->RangeLength    = pPartialData->u.Port.Length;
                    pBaseAddress->RangeInMemory  = FALSE;
                    pBaseAddress->ResourceMapped = FALSE;

                    Log(("VBoxGuest::vboxguestwinScanPCIResourceList: I/O range for VMMDev found! Base = %08x:%08x, Length = %08x\n",
                            pPartialData->u.Port.Start.HighPart,
                            pPartialData->u.Port.Start.LowPart,
                            pPartialData->u.Port.Length));

                    /* Next item ... */
                    rangeCount++; pBaseAddress++;
                }
                break;
            }

            case CmResourceTypeInterrupt:
            {
                Log(("VBoxGuest::vboxguestwinScanPCIResourceList: Interrupt: Level = %x, Vector = %x, Mode = %x\n",
                     pPartialData->u.Interrupt.Level,
                     pPartialData->u.Interrupt.Vector,
                     pPartialData->Flags));

                /* Save information. */
                pDevExt->win.s.interruptLevel    = pPartialData->u.Interrupt.Level;
                pDevExt->win.s.interruptVector   = pPartialData->u.Interrupt.Vector;
                pDevExt->win.s.interruptAffinity = pPartialData->u.Interrupt.Affinity;

                /* Check interrupt mode. */
                if (pPartialData->Flags & CM_RESOURCE_INTERRUPT_LATCHED)
                {
                    pDevExt->win.s.interruptMode = Latched;
                }
                else
                {
                    pDevExt->win.s.interruptMode = LevelSensitive;
                }
                break;
            }

            case CmResourceTypeMemory:
            {
                /* Overflow protection. */
                if (rangeCount < PCI_TYPE0_ADDRESSES)
                {
                    Log(("VBoxGuest::vboxguestwinScanPCIResourceList: Memory range: Base = %08x:%08x, Length = %08x\n",
                         pPartialData->u.Memory.Start.HighPart,
                         pPartialData->u.Memory.Start.LowPart,
                         pPartialData->u.Memory.Length));

                    /* We only care about read/write memory. */
                    /** @todo Reconsider memory type. */
                    if (   cMMIORange == 0 /* Only care about the first MMIO range (!!!). */
                        && (pPartialData->Flags & VBOX_CM_PRE_VISTA_MASK) == CM_RESOURCE_MEMORY_READ_WRITE)
                    {
                        /* Save physical MMIO base + length for VMMDev. */
                        pDevExt->win.s.vmmDevPhysMemoryAddress = pPartialData->u.Memory.Start;
                        pDevExt->win.s.vmmDevPhysMemoryLength = (ULONG)pPartialData->u.Memory.Length;

                        /* Save resource information. */
                        pBaseAddress->RangeStart     = pPartialData->u.Memory.Start;
                        pBaseAddress->RangeLength    = pPartialData->u.Memory.Length;
                        pBaseAddress->RangeInMemory  = TRUE;
                        pBaseAddress->ResourceMapped = FALSE;

                        Log(("VBoxGuest::vboxguestwinScanPCIResourceList: Memory range for VMMDev found! Base = %08x:%08x, Length = %08x\n",
                             pPartialData->u.Memory.Start.HighPart,
                             pPartialData->u.Memory.Start.LowPart,
                             pPartialData->u.Memory.Length));

                        /* Next item ... */
                        rangeCount++; pBaseAddress++; cMMIORange++;
                    }
                    else
                    {
                        Log(("VBoxGuest::vboxguestwinScanPCIResourceList: Ignoring memory: Flags = %08x\n",
                             pPartialData->Flags));
                    }
                }
                break;
            }

            default:
            {
                Log(("VBoxGuest::vboxguestwinScanPCIResourceList: Unhandled resource found, type = %d\n", pPartialData->Type));
                break;
            }
        }
    }

    /* Memorize the number of resources found. */
    pDevExt->win.s.pciAddressCount = rangeCount;
    return rc;
}


/**
 * Maps the I/O space from VMMDev to virtual kernel address space.
 *
 * @return NTSTATUS
 *
 * @param pDevExt           The device extension.
 * @param physicalAdr       Physical address to map.
 * @param ulLength          Length (in bytes) to map.
 * @param ppvMMIOBase       Pointer of mapped I/O base.
 * @param pcbMMIO           Length of mapped I/O base.
 */
NTSTATUS vboxguestwinMapVMMDevMemory(PVBOXGUESTDEVEXT pDevExt, PHYSICAL_ADDRESS physicalAdr, ULONG ulLength,
                                     void **ppvMMIOBase, uint32_t *pcbMMIO)
{
    AssertPtrReturn(pDevExt, VERR_INVALID_POINTER);
    AssertPtrReturn(ppvMMIOBase, VERR_INVALID_POINTER);
    /* pcbMMIO is optional. */

    NTSTATUS rc = STATUS_SUCCESS;
    if (physicalAdr.LowPart > 0) /* We're mapping below 4GB. */
    {
         VMMDevMemory *pVMMDevMemory = (VMMDevMemory *)MmMapIoSpace(physicalAdr, ulLength, MmNonCached);
         Log(("VBoxGuest::vboxguestwinMapVMMDevMemory: pVMMDevMemory = 0x%x\n", pVMMDevMemory));
         if (pVMMDevMemory)
         {
             Log(("VBoxGuest::vboxguestwinMapVMMDevMemory: VMMDevMemory: Version = 0x%x, Size = %d\n",
                  pVMMDevMemory->u32Version, pVMMDevMemory->u32Size));

             /* Check version of the structure; do we have the right memory version? */
             if (pVMMDevMemory->u32Version != VMMDEV_MEMORY_VERSION)
             {
                 Log(("VBoxGuest::vboxguestwinMapVMMDevMemory: Wrong version (%u), refusing operation!\n",
                      pVMMDevMemory->u32Version));

                 /* Not our version, refuse operation and unmap the memory. */
                 vboxguestwinUnmapVMMDevMemory(pDevExt);
                 rc = STATUS_UNSUCCESSFUL;
             }
             else
             {
                 /* Save results. */
                 *ppvMMIOBase = pVMMDevMemory;
                 if (pcbMMIO) /* Optional. */
                     *pcbMMIO = pVMMDevMemory->u32Size;

                 Log(("VBoxGuest::vboxguestwinMapVMMDevMemory: VMMDevMemory found and mapped! pvMMIOBase = 0x%p\n",
                      *ppvMMIOBase));
             }
         }
         else
             rc = STATUS_UNSUCCESSFUL;
    }
    return rc;
}


/**
 * Unmaps the VMMDev I/O range from kernel space.
 *
 * @param   pDevExt     The device extension.
 */
void vboxguestwinUnmapVMMDevMemory(PVBOXGUESTDEVEXT pDevExt)
{
    Log(("VBoxGuest::vboxguestwinUnmapVMMDevMemory: pVMMDevMemory = 0x%x\n", pDevExt->pVMMDevMemory));
    if (pDevExt->pVMMDevMemory)
    {
        MmUnmapIoSpace((void*)pDevExt->pVMMDevMemory, pDevExt->win.s.vmmDevPhysMemoryLength);
        pDevExt->pVMMDevMemory = NULL;
    }

    pDevExt->win.s.vmmDevPhysMemoryAddress.QuadPart = 0;
    pDevExt->win.s.vmmDevPhysMemoryLength = 0;
}


VBOXOSTYPE vboxguestwinVersionToOSType(winVersion_t winVer)
{
    VBOXOSTYPE enmOsType;
    switch (winVer)
    {
        case WINNT4:
            enmOsType = VBOXOSTYPE_WinNT4;
            break;

        case WIN2K:
            enmOsType = VBOXOSTYPE_Win2k;
            break;

        case WINXP:
#if ARCH_BITS == 64
            enmOsType = VBOXOSTYPE_WinXP_x64;
#else
            enmOsType = VBOXOSTYPE_WinXP;
#endif
            break;

        case WIN2K3:
#if ARCH_BITS == 64
            enmOsType = VBOXOSTYPE_Win2k3_x64;
#else
            enmOsType = VBOXOSTYPE_Win2k3;
#endif
            break;

        case WINVISTA:
#if ARCH_BITS == 64
            enmOsType = VBOXOSTYPE_WinVista_x64;
#else
            enmOsType = VBOXOSTYPE_WinVista;
#endif
            break;

        case WIN7:
#if ARCH_BITS == 64
            enmOsType = VBOXOSTYPE_Win7_x64;
#else
            enmOsType = VBOXOSTYPE_Win7;
#endif
            break;

        case WIN8:
#if ARCH_BITS == 64
            enmOsType = VBOXOSTYPE_Win8_x64;
#else
            enmOsType = VBOXOSTYPE_Win8;
#endif
            break;

        default:
            /* We don't know, therefore NT family. */
            enmOsType = VBOXOSTYPE_WinNT;
            break;
    }
    return enmOsType;
}

#ifdef DEBUG

/**
 * A quick implementation of AtomicTestAndClear for uint32_t and multiple bits.
 */
static uint32_t vboxugestwinAtomicBitsTestAndClear(void *pu32Bits, uint32_t u32Mask)
{
    AssertPtrReturn(pu32Bits, 0);
    LogFlowFunc(("*pu32Bits=0x%x, u32Mask=0x%x\n", *(long *)pu32Bits,
                 u32Mask));
    uint32_t u32Result = 0;
    uint32_t u32WorkingMask = u32Mask;
    int iBitOffset = ASMBitFirstSetU32 (u32WorkingMask);

    while (iBitOffset > 0)
    {
        bool fSet = ASMAtomicBitTestAndClear(pu32Bits, iBitOffset - 1);
        if (fSet)
            u32Result |= 1 << (iBitOffset - 1);
        u32WorkingMask &= ~(1 << (iBitOffset - 1));
        iBitOffset = ASMBitFirstSetU32 (u32WorkingMask);
    }
    LogFlowFunc(("Returning 0x%x\n", u32Result));
    return u32Result;
}


static void vboxguestwinTestAtomicTestAndClearBitsU32(uint32_t u32Mask, uint32_t u32Bits,
                                                      uint32_t u32Exp)
{
    ULONG u32Bits2 = u32Bits;
    uint32_t u32Result = vboxugestwinAtomicBitsTestAndClear(&u32Bits2, u32Mask);
    if (   u32Result != u32Exp
        || (u32Bits2 & u32Mask)
        || (u32Bits2 & u32Result)
        || ((u32Bits2 | u32Result) != u32Bits)
       )
        AssertLogRelMsgFailed(("%s: TEST FAILED: u32Mask=0x%x, u32Bits (before)=0x%x, u32Bits (after)=0x%x, u32Result=0x%x, u32Exp=ox%x\n",
                               __PRETTY_FUNCTION__, u32Mask, u32Bits, u32Bits2,
                               u32Result));
}


static void vboxguestwinDoTests()
{
    vboxguestwinTestAtomicTestAndClearBitsU32(0x00, 0x23, 0);
    vboxguestwinTestAtomicTestAndClearBitsU32(0x11, 0, 0);
    vboxguestwinTestAtomicTestAndClearBitsU32(0x11, 0x22, 0);
    vboxguestwinTestAtomicTestAndClearBitsU32(0x11, 0x23, 0x1);
    vboxguestwinTestAtomicTestAndClearBitsU32(0x11, 0x32, 0x10);
    vboxguestwinTestAtomicTestAndClearBitsU32(0x22, 0x23, 0x22);
}

#endif /* DEBUG */

