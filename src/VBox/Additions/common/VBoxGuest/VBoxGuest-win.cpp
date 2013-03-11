/* $Id$ */
/** @file
 * VBoxGuest - Windows specifics.
 */

/*
 * Copyright (C) 2010-2013 Oracle Corporation
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
#include <iprt/asm-amd64-x86.h>

#include <VBox/log.h>
#include <VBox/VBoxGuestLib.h>
#include <iprt/string.h>

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
static NTSTATUS vbgdNtAddDevice(PDRIVER_OBJECT pDrvObj, PDEVICE_OBJECT pDevObj);
static void     vbgdNtUnload(PDRIVER_OBJECT pDrvObj);
static NTSTATUS vbgdNtCreate(PDEVICE_OBJECT pDevObj, PIRP pIrp);
static NTSTATUS vbgdNtClose(PDEVICE_OBJECT pDevObj, PIRP pIrp);
static NTSTATUS vbgdNtIOCtl(PDEVICE_OBJECT pDevObj, PIRP pIrp);
static NTSTATUS vbgdNtInternalIOCtl(PDEVICE_OBJECT pDevObj, PIRP pIrp);
static NTSTATUS vbgdNtRegistryReadDWORD(ULONG ulRoot, PCWSTR pwszPath, PWSTR pwszName, PULONG puValue);
static NTSTATUS vbgdNtSystemControl(PDEVICE_OBJECT pDevObj, PIRP pIrp);
static NTSTATUS vbgdNtShutdown(PDEVICE_OBJECT pDevObj, PIRP pIrp);
static NTSTATUS vbgdNtNotSupportedStub(PDEVICE_OBJECT pDevObj, PIRP pIrp);
#ifdef DEBUG
static void     vbgdNtDoTests(void);
#endif
RT_C_DECLS_END


/*******************************************************************************
*   Exported Functions                                                         *
*******************************************************************************/
RT_C_DECLS_BEGIN
ULONG DriverEntry(PDRIVER_OBJECT pDrvObj, PUNICODE_STRING pRegPath);
RT_C_DECLS_END

#ifdef ALLOC_PRAGMA
# pragma alloc_text(INIT, DriverEntry)
# pragma alloc_text(PAGE, vbgdNtAddDevice)
# pragma alloc_text(PAGE, vbgdNtUnload)
# pragma alloc_text(PAGE, vbgdNtCreate)
# pragma alloc_text(PAGE, vbgdNtClose)
# pragma alloc_text(PAGE, vbgdNtShutdown)
# pragma alloc_text(PAGE, vbgdNtNotSupportedStub)
# pragma alloc_text(PAGE, vbgdNtScanPCIResourceList)
#endif


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The detected NT (windows) version. */
VBGDNTVER g_enmVbgdNtVer = VBGDNTVER_INVALID;



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

    /*
     * Check if the the NT version is supported and initializing
     * g_enmVbgdNtVer in the process.
     */
    ULONG ulMajorVer;
    ULONG ulMinorVer;
    ULONG ulBuildNo;
    BOOLEAN fCheckedBuild = PsGetVersion(&ulMajorVer, &ulMinorVer, &ulBuildNo, NULL);
    Log(("VBoxGuest::DriverEntry: Running on Windows NT version %u.%u, build %u\n", ulMajorVer, ulMinorVer, ulBuildNo));
    if (fCheckedBuild)
        Log(("VBoxGuest::DriverEntry: Running on a Windows checked build (debug)!\n"));
#ifdef DEBUG
    vbgdNtDoTests();
#endif
    switch (ulMajorVer)
    {
        case 6: /* Windows Vista or Windows 7 (based on minor ver) */
            switch (ulMinorVer)
            {
                case 0: /* Note: Also could be Windows 2008 Server! */
                    g_enmVbgdNtVer = VBGDNTVER_WINVISTA;
                    break;
                case 1: /* Note: Also could be Windows 2008 Server R2! */
                    g_enmVbgdNtVer = VBGDNTVER_WIN7;
                    break;
                case 2:
                    g_enmVbgdNtVer = VBGDNTVER_WIN8;
                    break;
                default:
                    Log(("VBoxGuest::DriverEntry: Unknown version of Windows (%u.%u), refusing!\n", ulMajorVer, ulMinorVer));
                    rc = STATUS_DRIVER_UNABLE_TO_LOAD;
                    break;
            }
            break;
        case 5:
            switch (ulMinorVer)
            {
                case 2:
                    g_enmVbgdNtVer = VBGDNTVER_WIN2K3;
                    break;
                case 1:
                    g_enmVbgdNtVer = VBGDNTVER_WINXP;
                    break;
                case 0:
                    g_enmVbgdNtVer = VBGDNTVER_WIN2K;
                    break;
                default:
                    Log(("VBoxGuest::DriverEntry: Unknown version of Windows (%u.%u), refusing!\n", ulMajorVer, ulMinorVer));
                    rc = STATUS_DRIVER_UNABLE_TO_LOAD;
            }
            break;
        case 4:
            g_enmVbgdNtVer = VBGDNTVER_WINNT4;
            break;
        default:
            if (ulMajorVer < 4)
                Log(("VBoxGuest::DriverEntry: At least Windows NT4 required! (%u.%u)\n", ulMajorVer, ulMinorVer));
            else
                Log(("VBoxGuest::DriverEntry: Too new version %u.%u!\n", ulMajorVer, ulMinorVer));
            rc = STATUS_DRIVER_UNABLE_TO_LOAD;
            break;
    }

    if (NT_SUCCESS(rc))
    {
        /*
         * Setup the driver entry points in pDrvObj.
         */
        pDrvObj->DriverUnload                                  = vbgdNtUnload;
        pDrvObj->MajorFunction[IRP_MJ_CREATE]                  = vbgdNtCreate;
        pDrvObj->MajorFunction[IRP_MJ_CLOSE]                   = vbgdNtClose;
        pDrvObj->MajorFunction[IRP_MJ_DEVICE_CONTROL]          = vbgdNtIOCtl;
        pDrvObj->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = vbgdNtInternalIOCtl;
        pDrvObj->MajorFunction[IRP_MJ_SHUTDOWN]                = vbgdNtShutdown;
        pDrvObj->MajorFunction[IRP_MJ_READ]                    = vbgdNtNotSupportedStub;
        pDrvObj->MajorFunction[IRP_MJ_WRITE]                   = vbgdNtNotSupportedStub;
#ifdef TARGET_NT4
        rc = vbgdNt4CreateDevice(pDrvObj, NULL /* pDevObj */, pRegPath);
#else
        pDrvObj->MajorFunction[IRP_MJ_PNP]                     = vbgdNtPnP;
        pDrvObj->MajorFunction[IRP_MJ_POWER]                   = vbgdNtPower;
        pDrvObj->MajorFunction[IRP_MJ_SYSTEM_CONTROL]          = vbgdNtSystemControl;
        pDrvObj->DriverExtension->AddDevice                    = (PDRIVER_ADD_DEVICE)vbgdNtAddDevice;
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
static NTSTATUS vbgdNtAddDevice(PDRIVER_OBJECT pDrvObj, PDEVICE_OBJECT pDevObj)
{
    NTSTATUS rc;
    Log(("VBoxGuest::vbgdNtGuestAddDevice\n"));

    /*
     * Create device.
     */
    UNICODE_STRING DevName;
    RtlInitUnicodeString(&DevName, VBOXGUEST_DEVICE_NAME_NT);
    PDEVICE_OBJECT pDeviceObject = NULL;
    rc = IoCreateDevice(pDrvObj, sizeof(VBOXGUESTDEVEXTWIN), &DevName, FILE_DEVICE_UNKNOWN, 0, FALSE, &pDeviceObject);
    if (NT_SUCCESS(rc))
    {
        /*
         * Create symbolic link (DOS devices).
         */
        UNICODE_STRING DosName;
        RtlInitUnicodeString(&DosName, VBOXGUEST_DEVICE_NAME_DOS);
        rc = IoCreateSymbolicLink(&DosName, &DevName);
        if (NT_SUCCESS(rc))
        {
            /*
             * Setup the device extension.
             */
            PVBOXGUESTDEVEXTWIN pDevExt = (PVBOXGUESTDEVEXTWIN)pDeviceObject->DeviceExtension;
            RT_ZERO(*pDevExt);

            KeInitializeSpinLock(&pDevExt->MouseEventAccessLock);

            pDevExt->pDeviceObject = pDeviceObject;
            pDevExt->prevDevState = STOPPED;
            pDevExt->devState = STOPPED;

            pDevExt->pNextLowerDriver = IoAttachDeviceToDeviceStack(pDeviceObject, pDevObj);
            if (pDevExt->pNextLowerDriver != NULL)
            {
                /*
                 * If we reached this point we're fine with the basic driver setup,
                 * so continue to init our own things.
                 */
#ifdef VBOX_WITH_GUEST_BUGCHECK_DETECTION
                vbgdNtBugCheckCallback(pDevExt); /* Ignore failure! */
#endif
                if (NT_SUCCESS(rc))
                {
                    /* VBoxGuestPower is pageable; ensure we are not called at elevated IRQL */
                    pDeviceObject->Flags |= DO_POWER_PAGABLE;

                    /* Driver is ready now. */
                    pDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
                    Log(("VBoxGuest::vbgdNtGuestAddDevice: returning with rc = 0x%x (success)\n", rc));
                    return rc;
                }

                IoDetachDevice(pDevExt->pNextLowerDriver);
            }
            else
            {
                Log(("VBoxGuest::vbgdNtGuestAddDevice: IoAttachDeviceToDeviceStack did not give a nextLowerDriver!\n"));
                rc = STATUS_DEVICE_NOT_CONNECTED;
            }

            /* bail out */
            IoDeleteSymbolicLink(&DosName);
        }
        else
            Log(("VBoxGuest::vbgdNtGuestAddDevice: IoCreateSymbolicLink failed with rc=%#x!\n", rc));
        IoDeleteDevice(pDeviceObject);
    }
    else
        Log(("VBoxGuest::vbgdNtGuestAddDevice: IoCreateDevice failed with rc=%#x!\n", rc));
    Log(("VBoxGuest::vbgdNtGuestAddDevice: returning with rc = 0x%x\n", rc));
    return rc;
}
#endif


/**
 * Debug helper to dump a device resource list.
 *
 * @param pResourceList  list of device resources.
 */
static void vbgdNtShowDeviceResources(PCM_PARTIAL_RESOURCE_LIST pResourceList)
{
#ifdef LOG_ENABLED
    PCM_PARTIAL_RESOURCE_DESCRIPTOR pResource = pResourceList->PartialDescriptors;
    ULONG cResources = pResourceList->Count;

    for (ULONG i = 0; i < cResources; ++i, ++pResource)
    {
        ULONG uType = pResource->Type;
        static char const * const s_apszName[] =
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

        Log(("VBoxGuest::vbgdNtShowDeviceResources: Type %s", uType < RT_ELEMENTS(s_apszName) ? aszName[uType] : "Unknown"));

        switch (uType)
        {
            case CmResourceTypePort:
            case CmResourceTypeMemory:
                Log(("VBoxGuest::vbgdNtShowDeviceResources: Start %8X%8.8lX length %X\n",
                     pResource->u.Port.Start.HighPart, pResource->u.Port.Start.LowPart,
                     pResource->u.Port.Length));
                break;

            case CmResourceTypeInterrupt:
                Log(("VBoxGuest::vbgdNtShowDeviceResources: Level %X, Vector %X, Affinity %X\n",
                     pResource->u.Interrupt.Level, pResource->u.Interrupt.Vector,
                     pResource->u.Interrupt.Affinity));
                break;

            case CmResourceTypeDma:
                Log(("VBoxGuest::vbgdNtShowDeviceResources: Channel %d, Port %X\n",
                     pResource->u.Dma.Channel, pResource->u.Dma.Port));
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
NTSTATUS vbgdNtInit(PDEVICE_OBJECT pDevObj, PIRP pIrp)
#else
NTSTATUS vbgdNtInit(PDRIVER_OBJECT pDrvObj, PDEVICE_OBJECT pDevObj, PUNICODE_STRING pRegPath)
#endif
{
    PVBOXGUESTDEVEXTWIN pDevExt = (PVBOXGUESTDEVEXTWIN)pDevObj->DeviceExtension;
#ifndef TARGET_NT4
    PIO_STACK_LOCATION  pStack  = IoGetCurrentIrpStackLocation(pIrp);
#endif

    Log(("VBoxGuest::vbgdNtInit\n"));

    int rc = STATUS_SUCCESS;
#ifdef TARGET_NT4
    /*
     * Let's have a look at what our PCI adapter offers.
     */
    Log(("VBoxGuest::vbgdNtInit: Starting to scan PCI resources of VBoxGuest ...\n"));

    /* Assign the PCI resources. */
    PCM_RESOURCE_LIST pResourceList = NULL;
    UNICODE_STRING classNameString;
    RtlInitUnicodeString(&classNameString, L"VBoxGuestAdapter");
    rc = HalAssignSlotResources(pRegPath, &classNameString,
                                pDrvObj, pDevObj,
                                PCIBus, pDevExt->busNumber, pDevExt->slotNumber,
                                &pResourceList);
    if (pResourceList && pResourceList->Count > 0)
        vbgdNtShowDeviceResources(&pResourceList->List[0].PartialResourceList);
    if (NT_SUCCESS(rc))
        rc = vbgdNtScanPCIResourceList(pResourceList, pDevExt);
#else
    if (pStack->Parameters.StartDevice.AllocatedResources->Count > 0)
        vbgdNtShowDeviceResources(&pStack->Parameters.StartDevice.AllocatedResources->List[0].PartialResourceList);
    if (NT_SUCCESS(rc))
        rc = vbgdNtScanPCIResourceList(pStack->Parameters.StartDevice.AllocatedResourcesTranslated, pDevExt);
#endif
    if (NT_SUCCESS(rc))
    {
        /*
         * Map physical address of VMMDev memory into MMIO region
         * and init the common device extension bits.
         */
        void *pvMMIOBase = NULL;
        uint32_t cbMMIO = 0;
        rc = vbgdNtMapVMMDevMemory(pDevExt,
                                   pDevExt->vmmDevPhysMemoryAddress,
                                   pDevExt->vmmDevPhysMemoryLength,
                                   &pvMMIOBase,
                                   &cbMMIO);
        if (NT_SUCCESS(rc))
        {
            pDevExt->Core.pVMMDevMemory = (VMMDevMemory *)pvMMIOBase;

            Log(("VBoxGuest::vbgdNtInit: pvMMIOBase = 0x%p, pDevExt = 0x%p, pDevExt->pVMMDevMemory = 0x%p\n",
                 pvMMIOBase, pDevExt, pDevExt ? pDevExt->pVMMDevMemory : NULL));

            int vrc = VBoxGuestInitDevExt(&pDevExt->Core,
                                          pDevExt->Core.IOPortBase,
                                          pvMMIOBase, cbMMIO,
                                          vbgdNtVersionToOSType(g_enmVbgdNtVer),
                                          VMMDEV_EVENT_MOUSE_POSITION_CHANGED);
            if (RT_FAILURE(vrc))
            {
                Log(("VBoxGuest::vbgdNtInit: Could not init device extension, rc = %Rrc!\n", vrc));
                rc = STATUS_DEVICE_CONFIGURATION_ERROR;
            }
        }
        else
            Log(("VBoxGuest::vbgdNtInit: Could not map physical address of VMMDev, rc = 0x%x!\n", rc));
    }

    if (NT_SUCCESS(rc))
    {
        int vrc = VbglGRAlloc((VMMDevRequestHeader **)&pDevExt->pPowerStateRequest,
                              sizeof(VMMDevPowerStateRequest), VMMDevReq_SetPowerStatus);
        if (RT_FAILURE(vrc))
        {
            Log(("VBoxGuest::vbgdNtInit: Alloc for pPowerStateRequest failed, rc = %Rrc\n", vrc));
            rc = STATUS_UNSUCCESSFUL;
        }
    }

    if (NT_SUCCESS(rc))
    {
        /*
         * Register DPC and ISR.
         */
        Log(("VBoxGuest::vbgdNtInit: Initializing DPC/ISR ...\n"));

        IoInitializeDpcRequest(pDevExt->pDeviceObject, vbgdNtDpcHandler);
#ifdef TARGET_NT4
        ULONG uInterruptVector;
        KIRQL irqLevel;
        /* Get an interrupt vector. */
        /* Only proceed if the device provides an interrupt. */
        if (   pDevExt->interruptLevel
            || pDevExt->interruptVector)
        {
            Log(("VBoxGuest::vbgdNtInit: Getting interrupt vector (HAL): Bus: %u, IRQL: %u, Vector: %u\n",
                 pDevExt->busNumber, pDevExt->interruptLevel, pDevExt->interruptVector));

            uInterruptVector = HalGetInterruptVector(PCIBus,
                                                     pDevExt->busNumber,
                                                     pDevExt->interruptLevel,
                                                     pDevExt->interruptVector,
                                                     &irqLevel,
                                                     &pDevExt->interruptAffinity);
            Log(("VBoxGuest::vbgdNtInit: HalGetInterruptVector returns vector %u\n", uInterruptVector));
            if (uInterruptVector == 0)
                Log(("VBoxGuest::vbgdNtInit: No interrupt vector found!\n"));
        }
        else
            Log(("VBoxGuest::vbgdNtInit: Device does not provide an interrupt!\n"));
#endif
        if (pDevExt->interruptVector)
        {
            Log(("VBoxGuest::vbgdNtInit: Connecting interrupt ...\n"));

            rc = IoConnectInterrupt(&pDevExt->pInterruptObject,                 /* Out: interrupt object. */
                                    (PKSERVICE_ROUTINE)vbgdNtIsrHandler,        /* Our ISR handler. */
                                    pDevExt,                                    /* Device context. */
                                    NULL,                                       /* Optional spinlock. */
#ifdef TARGET_NT4
                                    uInterruptVector,                           /* Interrupt vector. */
                                    irqLevel,                                   /* Interrupt level. */
                                    irqLevel,                                   /* Interrupt level. */
#else
                                    pDevExt->interruptVector,                   /* Interrupt vector. */
                                    (KIRQL)pDevExt->interruptLevel,             /* Interrupt level. */
                                    (KIRQL)pDevExt->interruptLevel,             /* Interrupt level. */
#endif
                                    pDevExt->interruptMode,                     /* LevelSensitive or Latched. */
                                    TRUE,                                       /* Shareable interrupt. */
                                    pDevExt->interruptAffinity,                 /* CPU affinity. */
                                    FALSE);                                     /* Don't save FPU stack. */
            if (NT_ERROR(rc))
                Log(("VBoxGuest::vbgdNtInit: Could not connect interrupt, rc = 0x%x\n", rc));
        }
        else
            Log(("VBoxGuest::vbgdNtInit: No interrupt vector found!\n"));
    }


#ifdef VBOX_WITH_HGCM
    Log(("VBoxGuest::vbgdNtInit: Allocating kernel session data ...\n"));
    int vrc = VBoxGuestCreateKernelSession(&pDevExt->Core, &pDevExt->pKernelSession);
    if (RT_FAILURE(vrc))
    {
        Log(("VBoxGuest::vbgdNtInit: Failed to allocated kernel session data! rc = %Rrc\n", rc));
        rc = STATUS_UNSUCCESSFUL;
    }
#endif

    if (RT_SUCCESS(rc))
    {
        ULONG ulValue = 0;
        NTSTATUS rcNt = vbgdNtRegistryReadDWORD(RTL_REGISTRY_SERVICES, L"VBoxGuest", L"LoggingEnabled", &ulValue);
        if (NT_SUCCESS(rcNt))
        {
            pDevExt->Core.fLoggingEnabled = ulValue >= 0xFF;
            if (pDevExt->Core.fLoggingEnabled)
                Log(("Logging to release log enabled (0x%x)", ulValue));
        }

        /* Ready to rumble! */
        Log(("VBoxGuest::vbgdNtInit: Device is ready!\n"));
        VBOXGUEST_UPDATE_DEVSTATE(pDevExt, WORKING);
    }
    else
        pDevExt->pInterruptObject = NULL;

    /** @todo r=bird: The error cleanup here is completely missing. We'll leak a
     *        whole bunch of things... */

    Log(("VBoxGuest::vbgdNtInit: Returned with rc = 0x%x\n", rc));
    return rc;
}


/**
 * Cleans up hardware resources.
 * Do not delete DevExt here.
 *
 * @param   pDrvObj     Driver object.
 */
NTSTATUS vbgdNtCleanup(PDEVICE_OBJECT pDevObj)
{
    Log(("VBoxGuest::vbgdNtCleanup\n"));

    PVBOXGUESTDEVEXTWIN pDevExt = (PVBOXGUESTDEVEXTWIN)pDevObj->DeviceExtension;
    if (pDevExt)
    {

#if 0 /* @todo: test & enable cleaning global session data */
#ifdef VBOX_WITH_HGCM
        if (pDevExt->pKernelSession)
        {
            VBoxGuestCloseSession(pDevExt, pDevExt->pKernelSession);
            pDevExt->pKernelSession = NULL;
        }
#endif
#endif

        if (pDevExt->pInterruptObject)
        {
            IoDisconnectInterrupt(pDevExt->pInterruptObject);
            pDevExt->pInterruptObject = NULL;
        }

        /** @todo: cleanup the rest stuff */


#ifdef VBOX_WITH_GUEST_BUGCHECK_DETECTION
        hlpDeregisterBugCheckCallback(pDevExt); /* ignore failure! */
#endif
        /* According to MSDN we have to unmap previously mapped memory. */
        vbgdNtUnmapVMMDevMemory(pDevExt);
    }
    return STATUS_SUCCESS;
}


/**
 * Unload the driver.
 *
 * @param   pDrvObj     Driver object.
 */
static void vbgdNtUnload(PDRIVER_OBJECT pDrvObj)
{
    Log(("VBoxGuest::vbgdNtGuestUnload\n"));
#ifdef TARGET_NT4
    vbgdNtCleanup(pDrvObj->DeviceObject);

    /* Destroy device extension and clean up everything else. */
    if (pDrvObj->DeviceObject && pDrvObj->DeviceObject->DeviceExtension)
        VBoxGuestDeleteDevExt((PVBOXGUESTDEVEXT)pDrvObj->DeviceObject->DeviceExtension);

    /*
     * I don't think it's possible to unload a driver which processes have
     * opened, at least we'll blindly assume that here.
     */
    UNICODE_STRING DosName;
    RtlInitUnicodeString(&DosName, VBOXGUEST_DEVICE_NAME_DOS);
    NTSTATUS rc = IoDeleteSymbolicLink(&DosName);

    IoDeleteDevice(pDrvObj->DeviceObject);
#else  /* !TARGET_NT4 */
    /* On a PnP driver this routine will be called after
     * IRP_MN_REMOVE_DEVICE (where we already did the cleanup),
     * so don't do anything here (yet). */
#endif /* !TARGET_NT4 */

    Log(("VBoxGuest::vbgdNtGuestUnload: returning\n"));
}


/**
 * Create (i.e. Open) file entry point.
 *
 * @param   pDevObj     Device object.
 * @param   pIrp        Request packet.
 */
static NTSTATUS vbgdNtCreate(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    /** @todo AssertPtrReturn(pIrp); */
    PIO_STACK_LOCATION  pStack   = IoGetCurrentIrpStackLocation(pIrp);
    /** @todo AssertPtrReturn(pStack); */
    PFILE_OBJECT        pFileObj = pStack->FileObject;
    PVBOXGUESTDEVEXTWIN pDevExt  = (PVBOXGUESTDEVEXTWIN)pDevObj->DeviceExtension;
    NTSTATUS            rc       = STATUS_SUCCESS;

    if (pDevExt->devState != WORKING)
    {
        Log(("VBoxGuest::vbgdNtGuestCreate: device is not working currently: %d!\n", pDevExt->devState));
        rc = STATUS_UNSUCCESSFUL;
    }
    else if (pStack->Parameters.Create.Options & FILE_DIRECTORY_FILE)
    {
        /*
         * We are not remotely similar to a directory...
         * (But this is possible.)
         */
        Log(("VBoxGuest::vbgdNtGuestCreate: Uhm, we're not a directory!\n"));
        rc = STATUS_NOT_A_DIRECTORY;
    }
    else
    {
#ifdef VBOX_WITH_HGCM
        if (pFileObj)
        {
            Log(("VBoxGuest::vbgdNtGuestCreate: File object type = %d\n",
                 pFileObj->Type));

            int vrc;
            PVBOXGUESTSESSION pSession;
            if (pFileObj->Type == 5 /* File Object */)
            {
                /*
                 * Create a session object if we have a valid file object. This session object
                 * exists for every R3 process.
                 */
                vrc = VBoxGuestCreateUserSession(&pDevExt->Core, &pSession);
            }
            else
            {
                /* ... otherwise we've been called from R0! */
                vrc = VBoxGuestCreateKernelSession(&pDevExt->Core, &pSession);
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

    Log(("VBoxGuest::vbgdNtGuestCreate: Returning 0x%x\n", rc));
    return rc;
}


/**
 * Close file entry point.
 *
 * @param   pDevObj     Device object.
 * @param   pIrp        Request packet.
 */
static NTSTATUS vbgdNtClose(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    PVBOXGUESTDEVEXTWIN pDevExt  = (PVBOXGUESTDEVEXTWIN)pDevObj->DeviceExtension;
    PIO_STACK_LOCATION  pStack   = IoGetCurrentIrpStackLocation(pIrp);
    PFILE_OBJECT        pFileObj = pStack->FileObject;

    Log(("VBoxGuest::vbgdNtGuestClose: pDevExt=0x%p pFileObj=0x%p FsContext=0x%p\n",
         pDevExt, pFileObj, pFileObj->FsContext));

#ifdef VBOX_WITH_HGCM
    /* Close both, R0 and R3 sessions. */
    PVBOXGUESTSESSION pSession = (PVBOXGUESTSESSION)pFileObj->FsContext;
    if (pSession)
        VBoxGuestCloseSession(&pDevExt->Core, pSession);
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
static NTSTATUS vbgdNtIOCtl(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    NTSTATUS            Status   = STATUS_SUCCESS;
    PVBOXGUESTDEVEXTWIN pDevExt  = (PVBOXGUESTDEVEXTWIN)pDevObj->DeviceExtension;
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

    Log(("VBoxGuest::vbgdNtIOCtl: uCmd=%u, pDevExt=0x%p, pSession=0x%p\n",
         uCmd, pDevExt, pSession));

    /* We don't have a session associated with the file object? So this seems
     * to be a kernel call then. */
    /** @todo r=bird: What on earth is this supposed to be? Each kernel session
     *        shall have its own context of course, no hacks, pleeease. */
    if (pSession == NULL)
    {
        Log(("VBoxGuest::vbgdNtIOCtl: Using kernel session data ...\n"));
        pSession = pDevExt->pKernelSession;
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
            LogRel(("VBoxGuest::vbgdNtIOCtl: ENABLE_VRDP_SESSION: Currently: %sabled\n",
                    pDevExt->fVRDPEnabled? "en": "dis"));
            if (!pDevExt->fVRDPEnabled)
            {
                KUSER_SHARED_DATA *pSharedUserData = (KUSER_SHARED_DATA *)KI_USER_SHARED_DATA;

                pDevExt->fVRDPEnabled            = true;
                LogRel(("VBoxGuest::vbgdNtIOCtl: ENABLE_VRDP_SESSION: Current active console ID: 0x%08X\n",
                        pSharedUserData->ActiveConsoleId));
                pDevExt->ulOldActiveConsoleId    = pSharedUserData->ActiveConsoleId;
                pSharedUserData->ActiveConsoleId = 2;
            }
            break;
        }

        case VBOXGUEST_IOCTL_DISABLE_VRDP_SESSION:
        {
            LogRel(("VBoxGuest::vbgdNtIOCtl: DISABLE_VRDP_SESSION: Currently: %sabled\n",
                    pDevExt->fVRDPEnabled? "en": "dis"));
            if (pDevExt->fVRDPEnabled)
            {
                KUSER_SHARED_DATA *pSharedUserData = (KUSER_SHARED_DATA *)KI_USER_SHARED_DATA;

                pDevExt->fVRDPEnabled            = false;
                Log(("VBoxGuest::vbgdNtIOCtl: DISABLE_VRDP_SESSION: Current active console ID: 0x%08X\n",
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
            int vrc = VBoxGuestCommonIOCtl(uCmd, &pDevExt->Core, pSession, pBuf, cbData, &cbDataReturned);

            Log(("VBoxGuest::vbgdNtGuestDeviceControl: rc=%Rrc, pBuf=0x%p, cbData=%u, cbDataReturned=%u\n",
                 vrc, pBuf, cbData, cbDataReturned));

            if (RT_SUCCESS(vrc))
            {
                if (RT_UNLIKELY(cbDataReturned > cbData))
                {
                    Log(("VBoxGuest::vbgdNtGuestDeviceControl: Too much output data %u - expected %u!\n", cbDataReturned, cbData));
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
                    Status = STATUS_INVALID_PARAMETER;
                else if (vrc == VERR_OUT_OF_RANGE)
                    Status = STATUS_INVALID_BUFFER_SIZE;
                else
                    Status = STATUS_UNSUCCESSFUL;
            }
            break;
        }
    }

    pIrp->IoStatus.Status = Status;
    pIrp->IoStatus.Information = cbOut;

    IoCompleteRequest(pIrp, IO_NO_INCREMENT);

    //Log(("VBoxGuest::vbgdNtGuestDeviceControl: returned cbOut=%d rc=%#x\n", cbOut, Status));
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
static NTSTATUS vbgdNtInternalIOCtl(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    NTSTATUS            Status      = STATUS_SUCCESS;
    PVBOXGUESTDEVEXTWIN pDevExt     = (PVBOXGUESTDEVEXTWIN)pDevObj->DeviceExtension;
    PIO_STACK_LOCATION  pStack      = IoGetCurrentIrpStackLocation(pIrp);
    unsigned int        uCmd        = (unsigned int)pStack->Parameters.DeviceIoControl.IoControlCode;
    bool                fProcessed  = false;
    unsigned            Info        = 0;

    switch (uCmd)
    {
        case VBOXGUEST_IOCTL_SET_MOUSE_NOTIFY_CALLBACK:
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

            VBoxGuestMouseSetNotifyCallback *pInfo = (VBoxGuestMouseSetNotifyCallback*)pvBuf;

            /* we need a lock here to avoid concurrency with the set event functionality */
            KIRQL OldIrql;
            KeAcquireSpinLock(&pDevExt->MouseEventAccessLock, &OldIrql);
            pDevExt->Core.MouseNotifyCallback = *pInfo;
            KeReleaseSpinLock(&pDevExt->MouseEventAccessLock, OldIrql);

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

    return vbgdNtIOCtl(pDevObj, pIrp);
}


/**
 * IRP_MJ_SYSTEM_CONTROL handler.
 *
 * @returns NT status code
 * @param   pDevObj     Device object.
 * @param   pIrp        IRP.
 */
NTSTATUS vbgdNtSystemControl(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    PVBOXGUESTDEVEXTWIN pDevExt = (PVBOXGUESTDEVEXTWIN)pDevObj->DeviceExtension;

    Log(("VBoxGuest::vbgdNtGuestSystemControl\n"));

    /* Always pass it on to the next driver. */
    IoSkipCurrentIrpStackLocation(pIrp);

    return IoCallDriver(pDevExt->pNextLowerDriver, pIrp);
}


/**
 * IRP_MJ_SHUTDOWN handler.
 *
 * @returns NT status code
 * @param pDevObj    Device object.
 * @param pIrp       IRP.
 */
NTSTATUS vbgdNtShutdown(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    PVBOXGUESTDEVEXTWIN pDevExt = (PVBOXGUESTDEVEXTWIN)pDevObj->DeviceExtension;

    Log(("VBoxGuest::vbgdNtGuestShutdown\n"));

    VMMDevPowerStateRequest *pReq = pDevExt->pPowerStateRequest;
    if (pReq)
    {
        pReq->header.requestType = VMMDevReq_SetPowerStatus;
        pReq->powerState = VMMDevPowerState_PowerOff;

        int rc = VbglGRPerform(&pReq->header);
        if (RT_FAILURE(rc))
        {
            Log(("VBoxGuest::vbgdNtGuestShutdown: Error performing request to VMMDev! "
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
NTSTATUS vbgdNtNotSupportedStub(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    Log(("VBoxGuest::vbgdNtGuestNotSupportedStub\n"));

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
void vbgdNtDpcHandler(PKDPC pDPC, PDEVICE_OBJECT pDevObj, PIRP pIrp, PVOID pContext)
{
    PVBOXGUESTDEVEXTWIN pDevExt = (PVBOXGUESTDEVEXTWIN)pDevObj->DeviceExtension;
    Log(("VBoxGuest::vbgdNtGuestDpcHandler: pDevExt=0x%p\n", pDevExt));

    /* test & reset the counter */
    if (ASMAtomicXchgU32(&pDevExt->Core.u32MousePosChangedSeq, 0))
    {
        /* we need a lock here to avoid concurrency with the set event ioctl handler thread,
         * i.e. to prevent the event from destroyed while we're using it */
        Assert(KeGetCurrentIrql() == DISPATCH_LEVEL);
        KeAcquireSpinLockAtDpcLevel(&pDevExt->MouseEventAccessLock);

        if (pDevExt->Core.MouseNotifyCallback.pfnNotify)
            pDevExt->Core.MouseNotifyCallback.pfnNotify(pDevExt->Core.MouseNotifyCallback.pvUser);

        KeReleaseSpinLockFromDpcLevel(&pDevExt->MouseEventAccessLock);
    }

    /* Process the wake-up list we were asked by the scheduling a DPC
     * in vbgdNtIsrHandler(). */
    VBoxGuestWaitDoWakeUps(&pDevExt->Core);
}


/**
 * ISR handler.
 *
 * @return  BOOLEAN         Indicates whether the IRQ came from us (TRUE) or not (FALSE).
 * @param   pInterrupt      Interrupt that was triggered.
 * @param   pServiceContext Context specific pointer.
 */
BOOLEAN vbgdNtIsrHandler(PKINTERRUPT pInterrupt, PVOID pServiceContext)
{
    PVBOXGUESTDEVEXTWIN pDevExt = (PVBOXGUESTDEVEXTWIN)pServiceContext;
    if (pDevExt == NULL)
        return FALSE;

    /*Log(("VBoxGuest::vbgdNtGuestIsrHandler: pDevExt = 0x%p, pVMMDevMemory = 0x%p\n",
             pDevExt, pDevExt ? pDevExt->pVMMDevMemory : NULL));*/

    /* Enter the common ISR routine and do the actual work. */
    BOOLEAN fIRQTaken = VBoxGuestCommonISR(&pDevExt->Core);

    /* If we need to wake up some events we do that in a DPC to make
     * sure we're called at the right IRQL. */
    if (fIRQTaken)
    {
        Log(("VBoxGuest::vbgdNtGuestIsrHandler: IRQ was taken! pInterrupt = 0x%p, pDevExt = 0x%p\n",
             pInterrupt, pDevExt));
        if (ASMAtomicUoReadU32(&pDevExt->Core.u32MousePosChangedSeq) || !RTListIsEmpty(&pDevExt->Core.WakeUpList))
        {
            Log(("VBoxGuest::vbgdNtGuestIsrHandler: Requesting DPC ...\n"));
            IoRequestDpc(pDevExt->pDeviceObject, pDevExt->pCurrentIrp, NULL);
        }
    }
    return fIRQTaken;
}


/**
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
NTSTATUS vbgdNtRegistryReadDWORD(ULONG ulRoot, PCWSTR pwszPath, PWSTR pwszName, PULONG puValue)
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
NTSTATUS vbgdNtScanPCIResourceList(PCM_RESOURCE_LIST pResList, PVBOXGUESTDEVEXTWIN pDevExt)
{
    /* Enumerate the resource list. */
    Log(("VBoxGuest::vbgdNtScanPCIResourceList: Found %d resources\n",
         pResList->List->PartialResourceList.Count));

    NTSTATUS rc = STATUS_SUCCESS;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR pPartialData = NULL;
    ULONG rangeCount = 0;
    ULONG cMMIORange = 0;
    PVBOXGUESTWINBASEADDRESS pBaseAddress = pDevExt->pciBaseAddress;
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
                    Log(("VBoxGuest::vbgdNtScanPCIResourceList: I/O range: Base = %08x:%08x, Length = %08x\n",
                         pPartialData->u.Port.Start.HighPart,
                         pPartialData->u.Port.Start.LowPart,
                         pPartialData->u.Port.Length));

                    /* Save the IO port base. */
                    /** @todo Not so good.
                     * Update/bird: What is not so good? That we just consider the last range?  */
                    pDevExt->Core.IOPortBase = (RTIOPORT)pPartialData->u.Port.Start.LowPart;

                    /* Save resource information. */
                    pBaseAddress->RangeStart     = pPartialData->u.Port.Start;
                    pBaseAddress->RangeLength    = pPartialData->u.Port.Length;
                    pBaseAddress->RangeInMemory  = FALSE;
                    pBaseAddress->ResourceMapped = FALSE;

                    Log(("VBoxGuest::vbgdNtScanPCIResourceList: I/O range for VMMDev found! Base = %08x:%08x, Length = %08x\n",
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
                Log(("VBoxGuest::vbgdNtScanPCIResourceList: Interrupt: Level = %x, Vector = %x, Mode = %x\n",
                     pPartialData->u.Interrupt.Level,
                     pPartialData->u.Interrupt.Vector,
                     pPartialData->Flags));

                /* Save information. */
                pDevExt->interruptLevel    = pPartialData->u.Interrupt.Level;
                pDevExt->interruptVector   = pPartialData->u.Interrupt.Vector;
                pDevExt->interruptAffinity = pPartialData->u.Interrupt.Affinity;

                /* Check interrupt mode. */
                if (pPartialData->Flags & CM_RESOURCE_INTERRUPT_LATCHED)
                    pDevExt->interruptMode = Latched;
                else
                    pDevExt->interruptMode = LevelSensitive;
                break;
            }

            case CmResourceTypeMemory:
            {
                /* Overflow protection. */
                if (rangeCount < PCI_TYPE0_ADDRESSES)
                {
                    Log(("VBoxGuest::vbgdNtScanPCIResourceList: Memory range: Base = %08x:%08x, Length = %08x\n",
                         pPartialData->u.Memory.Start.HighPart,
                         pPartialData->u.Memory.Start.LowPart,
                         pPartialData->u.Memory.Length));

                    /* We only care about read/write memory. */
                    /** @todo Reconsider memory type. */
                    if (   cMMIORange == 0 /* Only care about the first MMIO range (!!!). */
                        && (pPartialData->Flags & VBOX_CM_PRE_VISTA_MASK) == CM_RESOURCE_MEMORY_READ_WRITE)
                    {
                        /* Save physical MMIO base + length for VMMDev. */
                        pDevExt->vmmDevPhysMemoryAddress = pPartialData->u.Memory.Start;
                        pDevExt->vmmDevPhysMemoryLength = (ULONG)pPartialData->u.Memory.Length;

                        /* Save resource information. */
                        pBaseAddress->RangeStart     = pPartialData->u.Memory.Start;
                        pBaseAddress->RangeLength    = pPartialData->u.Memory.Length;
                        pBaseAddress->RangeInMemory  = TRUE;
                        pBaseAddress->ResourceMapped = FALSE;

                        Log(("VBoxGuest::vbgdNtScanPCIResourceList: Memory range for VMMDev found! Base = %08x:%08x, Length = %08x\n",
                             pPartialData->u.Memory.Start.HighPart,
                             pPartialData->u.Memory.Start.LowPart,
                             pPartialData->u.Memory.Length));

                        /* Next item ... */
                        rangeCount++; pBaseAddress++; cMMIORange++;
                    }
                    else
                    {
                        Log(("VBoxGuest::vbgdNtScanPCIResourceList: Ignoring memory: Flags = %08x\n",
                             pPartialData->Flags));
                    }
                }
                break;
            }

            default:
            {
                Log(("VBoxGuest::vbgdNtScanPCIResourceList: Unhandled resource found, type = %d\n", pPartialData->Type));
                break;
            }
        }
    }

    /* Memorize the number of resources found. */
    pDevExt->pciAddressCount = rangeCount;
    return rc;
}


/**
 * Maps the I/O space from VMMDev to virtual kernel address space.
 *
 * @return NTSTATUS
 *
 * @param pDevExt           The device extension.
 * @param PhysAddr          Physical address to map.
 * @param cbToMap           Number of bytes to map.
 * @param ppvMMIOBase       Pointer of mapped I/O base.
 * @param pcbMMIO           Length of mapped I/O base.
 */
NTSTATUS vbgdNtMapVMMDevMemory(PVBOXGUESTDEVEXTWIN pDevExt, PHYSICAL_ADDRESS PhysAddr, ULONG cbToMap,
                               void **ppvMMIOBase, uint32_t *pcbMMIO)
{
    AssertPtrReturn(pDevExt, VERR_INVALID_POINTER);
    AssertPtrReturn(ppvMMIOBase, VERR_INVALID_POINTER);
    /* pcbMMIO is optional. */

    NTSTATUS rc = STATUS_SUCCESS;
    if (PhysAddr.LowPart > 0) /* We're mapping below 4GB. */
    {
         VMMDevMemory *pVMMDevMemory = (VMMDevMemory *)MmMapIoSpace(PhysAddr, cbToMap, MmNonCached);
         Log(("VBoxGuest::vbgdNtMapVMMDevMemory: pVMMDevMemory = 0x%x\n", pVMMDevMemory));
         if (pVMMDevMemory)
         {
             Log(("VBoxGuest::vbgdNtMapVMMDevMemory: VMMDevMemory: Version = 0x%x, Size = %d\n",
                  pVMMDevMemory->u32Version, pVMMDevMemory->u32Size));

             /* Check version of the structure; do we have the right memory version? */
             if (pVMMDevMemory->u32Version == VMMDEV_MEMORY_VERSION)
             {
                 /* Save results. */
                 *ppvMMIOBase = pVMMDevMemory;
                 if (pcbMMIO) /* Optional. */
                     *pcbMMIO = pVMMDevMemory->u32Size;

                 Log(("VBoxGuest::vbgdNtMapVMMDevMemory: VMMDevMemory found and mapped! pvMMIOBase = 0x%p\n",
                      *ppvMMIOBase));
             }
             else
             {
                 /* Not our version, refuse operation and unmap the memory. */
                 Log(("VBoxGuest::vbgdNtMapVMMDevMemory: Wrong version (%u), refusing operation!\n", pVMMDevMemory->u32Version));
                 vbgdNtUnmapVMMDevMemory(pDevExt);
                 rc = STATUS_UNSUCCESSFUL;
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
void vbgdNtUnmapVMMDevMemory(PVBOXGUESTDEVEXTWIN pDevExt)
{
    Log(("VBoxGuest::vbgdNtUnmapVMMDevMemory: pVMMDevMemory = 0x%x\n", pDevExt->Core.pVMMDevMemory));
    if (pDevExt->Core.pVMMDevMemory)
    {
        MmUnmapIoSpace((void*)pDevExt->Core.pVMMDevMemory, pDevExt->vmmDevPhysMemoryLength);
        pDevExt->Core.pVMMDevMemory = NULL;
    }

    pDevExt->vmmDevPhysMemoryAddress.QuadPart = 0;
    pDevExt->vmmDevPhysMemoryLength = 0;
}


VBOXOSTYPE vbgdNtVersionToOSType(VBGDNTVER enmNtVer)
{
    VBOXOSTYPE enmOsType;
    switch (enmNtVer)
    {
        case VBGDNTVER_WINNT4:
            enmOsType = VBOXOSTYPE_WinNT4;
            break;

        case VBGDNTVER_WIN2K:
            enmOsType = VBOXOSTYPE_Win2k;
            break;

        case VBGDNTVER_WINXP:
#if ARCH_BITS == 64
            enmOsType = VBOXOSTYPE_WinXP_x64;
#else
            enmOsType = VBOXOSTYPE_WinXP;
#endif
            break;

        case VBGDNTVER_WIN2K3:
#if ARCH_BITS == 64
            enmOsType = VBOXOSTYPE_Win2k3_x64;
#else
            enmOsType = VBOXOSTYPE_Win2k3;
#endif
            break;

        case VBGDNTVER_WINVISTA:
#if ARCH_BITS == 64
            enmOsType = VBOXOSTYPE_WinVista_x64;
#else
            enmOsType = VBOXOSTYPE_WinVista;
#endif
            break;

        case VBGDNTVER_WIN7:
#if ARCH_BITS == 64
            enmOsType = VBOXOSTYPE_Win7_x64;
#else
            enmOsType = VBOXOSTYPE_Win7;
#endif
            break;

        case VBGDNTVER_WIN8:
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
    LogFlowFunc(("*pu32Bits=0x%x, u32Mask=0x%x\n", *(uint32_t *)pu32Bits, u32Mask));
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


static void vbgdNtTestAtomicTestAndClearBitsU32(uint32_t u32Mask, uint32_t u32Bits, uint32_t u32Exp)
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


static void vbgdNtDoTests(void)
{
    vbgdNtTestAtomicTestAndClearBitsU32(0x00, 0x23, 0);
    vbgdNtTestAtomicTestAndClearBitsU32(0x11, 0, 0);
    vbgdNtTestAtomicTestAndClearBitsU32(0x11, 0x22, 0);
    vbgdNtTestAtomicTestAndClearBitsU32(0x11, 0x23, 0x1);
    vbgdNtTestAtomicTestAndClearBitsU32(0x11, 0x32, 0x10);
    vbgdNtTestAtomicTestAndClearBitsU32(0x22, 0x23, 0x22);
}

#endif /* DEBUG */

#ifdef VBOX_WITH_DPC_LATENCY_CHECKER

/*
 * DPC latency checker.
 */

/**
 * One DPC latency sample.
 */
typedef struct DPCSAMPLE
{
    LARGE_INTEGER   PerfDelta;
    LARGE_INTEGER   PerfCounter;
    LARGE_INTEGER   PerfFrequency;
    uint64_t        u64TSC;
} DPCSAMPLE;
AssertCompileSize(DPCSAMPLE, 4*8);

/**
 * The DPC latency measurement workset.
 */
typedef struct DPCDATA
{
    KDPC            Dpc;
    KTIMER          Timer;
    KSPIN_LOCK      SpinLock;

    ULONG           ulTimerRes;

    bool volatile   fFinished;

    /** The timer interval (relative). */
    LARGE_INTEGER   DueTime;

    LARGE_INTEGER   PerfCounterPrev;

    /** Align the sample array on a 64 byte boundrary just for the off chance
     * that we'll get cache line aligned memory backing this structure. */
    uint32_t        auPadding[ARCH_BITS == 32 ? 5 : 7];

    int             cSamples;
    DPCSAMPLE       aSamples[8192];
} DPCDATA;

AssertCompileMemberAlignment(DPCDATA, aSamples, 64);

# define VBOXGUEST_DPC_TAG 'DPCS'


/**
 * DPC callback routine for the DPC latency measurement code.
 *
 * @param   pDpc                The DPC, not used.
 * @param   pvDeferredContext   Pointer to the DPCDATA.
 * @param   SystemArgument1     System use, ignored.
 * @param   SystemArgument2     System use, ignored.
 */
static VOID vbgdNtDpcLatencyCallback(PKDPC pDpc, PVOID pvDeferredContext, PVOID SystemArgument1, PVOID SystemArgument2)
{
    DPCDATA *pData = (DPCDATA *)pvDeferredContext;

    KeAcquireSpinLockAtDpcLevel(&pData->SpinLock);

    if (pData->cSamples >= RT_ELEMENTS(pData->aSamples))
        pData->fFinished = true;
    else
    {
        DPCSAMPLE *pSample = &pData->aSamples[pData->cSamples++];

        pSample->u64TSC = ASMReadTSC();
        pSample->PerfCounter = KeQueryPerformanceCounter(&pSample->PerfFrequency);
        pSample->PerfDelta.QuadPart = pSample->PerfCounter.QuadPart - pData->PerfCounterPrev.QuadPart;

        pData->PerfCounterPrev.QuadPart = pSample->PerfCounter.QuadPart;

        KeSetTimer(&pData->Timer, pData->DueTime, &pData->Dpc);
    }

    KeReleaseSpinLockFromDpcLevel(&pData->SpinLock);
}


/**
 * Handles the DPC latency checker request.
 *
 * @returns VBox status code.
 */
int VbgdNtIOCtl_DpcLatencyChecker(void)
{
    /*
     * Allocate a block of non paged memory for samples and related data.
     */
    DPCDATA *pData = (DPCDATA *)ExAllocatePoolWithTag(NonPagedPool, sizeof(DPCDATA), VBOXGUEST_DPC_TAG);
    if (!pData)
    {
        RTLogBackdoorPrintf("VBoxGuest: DPC: DPCDATA allocation failed.\n");
        return VERR_NO_MEMORY;
    }

    /*
     * Initialize the data.
     */
    KeInitializeDpc(&pData->Dpc, vbgdNtDpcLatencyCallback, pData);
    KeInitializeTimer(&pData->Timer);
    KeInitializeSpinLock(&pData->SpinLock);

    pData->fFinished = false;
    pData->cSamples = 0;
    pData->PerfCounterPrev.QuadPart = 0;

    pData->ulTimerRes = ExSetTimerResolution(1000 * 10, 1);
    pData->DueTime.QuadPart = -(int64_t)pData->ulTimerRes / 10;

    /*
     * Start the DPC measurements and wait for a full set.
     */
    KeSetTimer(&pData->Timer, pData->DueTime, &pData->Dpc);

    while (!pData->fFinished)
    {
        LARGE_INTEGER Interval;
        Interval.QuadPart = -100 * 1000 * 10;
        KeDelayExecutionThread(KernelMode, TRUE, &Interval);
    }

    ExSetTimerResolution(0, 0);

    /*
     * Log everything to the host.
     */
    RTLogBackdoorPrintf("DPC: ulTimerRes = %d\n", pData->ulTimerRes);
    for (int i = 0; i < pData->cSamples; i++)
    {
        DPCSAMPLE *pSample = &pData->aSamples[i];

        RTLogBackdoorPrintf("[%d] pd %lld pc %lld pf %lld t %lld\n",
                            i,
                            pSample->PerfDelta.QuadPart,
                            pSample->PerfCounter.QuadPart,
                            pSample->PerfFrequency.QuadPart,
                            pSample->u64TSC);
    }

    ExFreePoolWithTag(pData, VBOXGUEST_DPC_TAG);
    return VINF_SUCCESS;
}

#endif /* VBOX_WITH_DPC_LATENCY_CHECKER */

