/* $Id$ */
/** @file
 * VBoxGuest - Windows specifics.
 */

/*
 * Copyright (C) 2010-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
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


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
RT_C_DECLS_BEGIN
static NTSTATUS vgdrvNtAddDevice(PDRIVER_OBJECT pDrvObj, PDEVICE_OBJECT pDevObj);
static void     vgdrvNtUnload(PDRIVER_OBJECT pDrvObj);
static NTSTATUS vgdrvNtCreate(PDEVICE_OBJECT pDevObj, PIRP pIrp);
static NTSTATUS vgdrvNtClose(PDEVICE_OBJECT pDevObj, PIRP pIrp);
static NTSTATUS vgdrvNtIOCtl(PDEVICE_OBJECT pDevObj, PIRP pIrp);
static NTSTATUS vgdrvNtInternalIOCtl(PDEVICE_OBJECT pDevObj, PIRP pIrp);
static NTSTATUS vgdrvNtRegistryReadDWORD(ULONG ulRoot, PCWSTR pwszPath, PWSTR pwszName, PULONG puValue);
static NTSTATUS vgdrvNtSystemControl(PDEVICE_OBJECT pDevObj, PIRP pIrp);
static NTSTATUS vgdrvNtShutdown(PDEVICE_OBJECT pDevObj, PIRP pIrp);
static NTSTATUS vgdrvNtNotSupportedStub(PDEVICE_OBJECT pDevObj, PIRP pIrp);
#ifdef VBOX_STRICT
static void     vgdrvNtDoTests(void);
#endif
static VOID     vgdrvNtDpcHandler(PKDPC pDPC, PDEVICE_OBJECT pDevObj, PIRP pIrp, PVOID pContext);
static BOOLEAN  vgdrvNtIsrHandler(PKINTERRUPT interrupt, PVOID serviceContext);
static NTSTATUS vgdrvNtScanPCIResourceList(PCM_RESOURCE_LIST pResList, PVBOXGUESTDEVEXTWIN pDevExt);
static NTSTATUS vgdrvNtMapVMMDevMemory(PVBOXGUESTDEVEXTWIN pDevExt, PHYSICAL_ADDRESS PhysAddr, ULONG cbToMap,
                                       void **ppvMMIOBase, uint32_t *pcbMMIO);
RT_C_DECLS_END


/*********************************************************************************************************************************
*   Exported Functions                                                                                                           *
*********************************************************************************************************************************/
RT_C_DECLS_BEGIN
ULONG DriverEntry(PDRIVER_OBJECT pDrvObj, PUNICODE_STRING pRegPath);
RT_C_DECLS_END

#ifdef ALLOC_PRAGMA
# pragma alloc_text(INIT, DriverEntry)
# pragma alloc_text(PAGE, vgdrvNtAddDevice)
# pragma alloc_text(PAGE, vgdrvNtUnload)
# pragma alloc_text(PAGE, vgdrvNtCreate)
# pragma alloc_text(PAGE, vgdrvNtClose)
# pragma alloc_text(PAGE, vgdrvNtShutdown)
# pragma alloc_text(PAGE, vgdrvNtNotSupportedStub)
# pragma alloc_text(PAGE, vgdrvNtScanPCIResourceList)
#endif


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** The detected NT (windows) version. */
VGDRVNTVER g_enmVGDrvNtVer = VGDRVNTVER_INVALID;



/**
 * Driver entry point.
 *
 * @returns appropriate status code.
 * @param   pDrvObj     Pointer to driver object.
 * @param   pRegPath    Registry base path.
 */
ULONG DriverEntry(PDRIVER_OBJECT pDrvObj, PUNICODE_STRING pRegPath)
{
    RT_NOREF1(pRegPath);
    NTSTATUS rc = STATUS_SUCCESS;

    LogFunc(("Driver built: %s %s\n", __DATE__, __TIME__));

    /*
     * Check if the the NT version is supported and initializing
     * g_enmVGDrvNtVer in the process.
     */
    ULONG ulMajorVer;
    ULONG ulMinorVer;
    ULONG ulBuildNo;
    BOOLEAN fCheckedBuild = PsGetVersion(&ulMajorVer, &ulMinorVer, &ulBuildNo, NULL);

    /* Use RTLogBackdoorPrintf to make sure that this goes to VBox.log */
    RTLogBackdoorPrintf("VBoxGuest: Windows version %u.%u, build %u\n", ulMajorVer, ulMinorVer, ulBuildNo);
    if (fCheckedBuild)
        RTLogBackdoorPrintf("VBoxGuest: Windows checked build\n");

#ifdef VBOX_STRICT
    vgdrvNtDoTests();
#endif
    switch (ulMajorVer)
    {
        case 10:
            switch (ulMinorVer)
            {
                case 0:
                    /* Windows 10 Preview builds starting with 9926. */
                default:
                    /* Also everything newer. */
                    g_enmVGDrvNtVer = VGDRVNTVER_WIN10;
                    break;
            }
            break;
        case 6: /* Windows Vista or Windows 7 (based on minor ver) */
            switch (ulMinorVer)
            {
                case 0: /* Note: Also could be Windows 2008 Server! */
                    g_enmVGDrvNtVer = VGDRVNTVER_WINVISTA;
                    break;
                case 1: /* Note: Also could be Windows 2008 Server R2! */
                    g_enmVGDrvNtVer = VGDRVNTVER_WIN7;
                    break;
                case 2:
                    g_enmVGDrvNtVer = VGDRVNTVER_WIN8;
                    break;
                case 3:
                    g_enmVGDrvNtVer = VGDRVNTVER_WIN81;
                    break;
                case 4:
                    /* Windows 10 Preview builds. */
                default:
                    /* Also everything newer. */
                    g_enmVGDrvNtVer = VGDRVNTVER_WIN10;
                    break;
            }
            break;
        case 5:
            switch (ulMinorVer)
            {
                default:
                case 2:
                    g_enmVGDrvNtVer = VGDRVNTVER_WIN2K3;
                    break;
                case 1:
                    g_enmVGDrvNtVer = VGDRVNTVER_WINXP;
                    break;
                case 0:
                    g_enmVGDrvNtVer = VGDRVNTVER_WIN2K;
                    break;
            }
            break;
        case 4:
            g_enmVGDrvNtVer = VGDRVNTVER_WINNT4;
            break;
        default:
            if (ulMajorVer > 6)
            {
                /* "Windows 10 mode" for Windows 8.1+. */
                g_enmVGDrvNtVer = VGDRVNTVER_WIN10;
            }
            else
            {
                if (ulMajorVer < 4)
                    LogRelFunc(("At least Windows NT4 required! (%u.%u)\n", ulMajorVer, ulMinorVer));
                else
                    LogRelFunc(("Unknown version %u.%u!\n", ulMajorVer, ulMinorVer));
                rc = STATUS_DRIVER_UNABLE_TO_LOAD;
            }
            break;
    }

    if (NT_SUCCESS(rc))
    {
        /*
         * Setup the driver entry points in pDrvObj.
         */
        pDrvObj->DriverUnload                                  = vgdrvNtUnload;
        pDrvObj->MajorFunction[IRP_MJ_CREATE]                  = vgdrvNtCreate;
        pDrvObj->MajorFunction[IRP_MJ_CLOSE]                   = vgdrvNtClose;
        pDrvObj->MajorFunction[IRP_MJ_DEVICE_CONTROL]          = vgdrvNtIOCtl;
        pDrvObj->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = vgdrvNtInternalIOCtl;
        pDrvObj->MajorFunction[IRP_MJ_SHUTDOWN]                = vgdrvNtShutdown;
        pDrvObj->MajorFunction[IRP_MJ_READ]                    = vgdrvNtNotSupportedStub;
        pDrvObj->MajorFunction[IRP_MJ_WRITE]                   = vgdrvNtNotSupportedStub;
#ifdef TARGET_NT4
        rc = vgdrvNt4CreateDevice(pDrvObj, pRegPath);
#else
        pDrvObj->MajorFunction[IRP_MJ_PNP]                     = vgdrvNtPnP;
        pDrvObj->MajorFunction[IRP_MJ_POWER]                   = vgdrvNtPower;
        pDrvObj->MajorFunction[IRP_MJ_SYSTEM_CONTROL]          = vgdrvNtSystemControl;
        pDrvObj->DriverExtension->AddDevice                    = (PDRIVER_ADD_DEVICE)vgdrvNtAddDevice;
#endif
    }

    LogFlowFunc(("Returning %#x\n", rc));
    return rc;
}


#ifndef TARGET_NT4
/**
 * Handle request from the Plug & Play subsystem.
 *
 * @returns NT status code
 * @param   pDrvObj   Driver object
 * @param   pDevObj   Device object
 *
 * @remarks Parts of this is duplicated in VBoxGuest-win-legacy.cpp.
 */
static NTSTATUS vgdrvNtAddDevice(PDRIVER_OBJECT pDrvObj, PDEVICE_OBJECT pDevObj)
{
    NTSTATUS rc;
    LogFlowFuncEnter();

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

            pDevExt->pDeviceObject   = pDeviceObject;
            pDevExt->enmPrevDevState = VGDRVNTDEVSTATE_STOPPED;
            pDevExt->enmDevState     = VGDRVNTDEVSTATE_STOPPED;

            pDevExt->pNextLowerDriver = IoAttachDeviceToDeviceStack(pDeviceObject, pDevObj);
            if (pDevExt->pNextLowerDriver != NULL)
            {
                /*
                 * If we reached this point we're fine with the basic driver setup,
                 * so continue to init our own things.
                 */
#ifdef VBOX_WITH_GUEST_BUGCHECK_DETECTION
                vgdrvNtBugCheckCallback(pDevExt); /* Ignore failure! */
#endif
                if (NT_SUCCESS(rc))
                {
                    /* VBoxGuestPower is pageable; ensure we are not called at elevated IRQL */
                    pDeviceObject->Flags |= DO_POWER_PAGABLE;

                    /* Driver is ready now. */
                    pDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
                    LogFlowFunc(("Returning with rc=%#x (success)\n", rc));
                    return rc;
                }

                IoDetachDevice(pDevExt->pNextLowerDriver);
            }
            else
            {
                LogFunc(("IoAttachDeviceToDeviceStack did not give a nextLowerDriver!\n"));
                rc = STATUS_DEVICE_NOT_CONNECTED;
            }

            /* bail out */
            IoDeleteSymbolicLink(&DosName);
        }
        else
            LogFunc(("IoCreateSymbolicLink failed with rc=%#x!\n", rc));
        IoDeleteDevice(pDeviceObject);
    }
    else
        LogFunc(("IoCreateDevice failed with rc=%#x!\n", rc));

    LogFunc(("Returning with rc=%#x\n", rc));
    return rc;
}
#endif /* TARGET_NT4 */


#ifdef LOG_ENABLED
/**
 * Debug helper to dump a device resource list.
 *
 * @param pResourceList  list of device resources.
 */
static void vgdrvNtShowDeviceResources(PCM_PARTIAL_RESOURCE_LIST pResourceList)
{
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

        LogFunc(("Type=%s", uType < RT_ELEMENTS(s_apszName) ? s_apszName[uType] : "Unknown"));

        switch (uType)
        {
            case CmResourceTypePort:
            case CmResourceTypeMemory:
                LogFunc(("Start %8X%8.8lX, length=%X\n",
                         pResource->u.Port.Start.HighPart, pResource->u.Port.Start.LowPart, pResource->u.Port.Length));
                break;

            case CmResourceTypeInterrupt:
                LogFunc(("Level=%X, vector=%X, affinity=%X\n",
                         pResource->u.Interrupt.Level, pResource->u.Interrupt.Vector, pResource->u.Interrupt.Affinity));
                break;

            case CmResourceTypeDma:
                LogFunc(("Channel %d, Port %X\n", pResource->u.Dma.Channel, pResource->u.Dma.Port));
                break;

            default:
                LogFunc(("\n"));
                break;
        }
    }
}
#endif /* LOG_ENABLED */


/**
 * Global initialisation stuff (PnP + NT4 legacy).
 *
 * @param  pDevObj    Device object.
 * @param  pIrp       Request packet.
 */
#ifndef TARGET_NT4
NTSTATUS vgdrvNtInit(PDEVICE_OBJECT pDevObj, PIRP pIrp)
#else
NTSTATUS vgdrvNtInit(PDRIVER_OBJECT pDrvObj, PDEVICE_OBJECT pDevObj, PUNICODE_STRING pRegPath)
#endif
{
    PVBOXGUESTDEVEXTWIN pDevExt = (PVBOXGUESTDEVEXTWIN)pDevObj->DeviceExtension;
#ifndef TARGET_NT4
    PIO_STACK_LOCATION  pStack  = IoGetCurrentIrpStackLocation(pIrp);
#endif

    LogFlowFuncEnter();

    NTSTATUS rcNt;
#ifdef TARGET_NT4
    /*
     * Let's have a look at what our PCI adapter offers.
     */
    LogFlowFunc(("Starting to scan PCI resources of VBoxGuest ...\n"));

    /* Assign the PCI resources. */
    PCM_RESOURCE_LIST pResourceList = NULL;
    UNICODE_STRING classNameString;
    RtlInitUnicodeString(&classNameString, L"VBoxGuestAdapter");
    rcNt = HalAssignSlotResources(pRegPath, &classNameString, pDrvObj, pDevObj,
                                  PCIBus, pDevExt->busNumber, pDevExt->slotNumber, &pResourceList);
# ifdef LOG_ENABLED
    if (pResourceList && pResourceList->Count > 0)
        vgdrvNtShowDeviceResources(&pResourceList->List[0].PartialResourceList);
# endif
    if (NT_SUCCESS(rcNt))
        rcNt = vgdrvNtScanPCIResourceList(pResourceList, pDevExt);
#else
# ifdef LOG_ENABLED
    if (pStack->Parameters.StartDevice.AllocatedResources->Count > 0)
        vgdrvNtShowDeviceResources(&pStack->Parameters.StartDevice.AllocatedResources->List[0].PartialResourceList);
# endif
    rcNt = vgdrvNtScanPCIResourceList(pStack->Parameters.StartDevice.AllocatedResourcesTranslated, pDevExt);
#endif
    if (NT_SUCCESS(rcNt))
    {
        /*
         * Map physical address of VMMDev memory into MMIO region
         * and init the common device extension bits.
         */
        void *pvMMIOBase = NULL;
        uint32_t cbMMIO = 0;
        rcNt = vgdrvNtMapVMMDevMemory(pDevExt,
                                      pDevExt->vmmDevPhysMemoryAddress,
                                      pDevExt->vmmDevPhysMemoryLength,
                                      &pvMMIOBase,
                                      &cbMMIO);
        if (NT_SUCCESS(rcNt))
        {
            pDevExt->Core.pVMMDevMemory = (VMMDevMemory *)pvMMIOBase;

            LogFunc(("pvMMIOBase=0x%p, pDevExt=0x%p, pDevExt->Core.pVMMDevMemory=0x%p\n",
                     pvMMIOBase, pDevExt, pDevExt ? pDevExt->Core.pVMMDevMemory : NULL));

            int vrc = VGDrvCommonInitDevExt(&pDevExt->Core,
                                            pDevExt->Core.IOPortBase,
                                            pvMMIOBase, cbMMIO,
                                            vgdrvNtVersionToOSType(g_enmVGDrvNtVer),
                                            VMMDEV_EVENT_MOUSE_POSITION_CHANGED);
            if (RT_FAILURE(vrc))
            {
                LogFunc(("Could not init device extension, vrc=%Rrc\n", vrc));
                rcNt = STATUS_DEVICE_CONFIGURATION_ERROR;
            }
        }
        else
            LogFunc(("Could not map physical address of VMMDev, rcNt=%#x\n", rcNt));
    }

    if (NT_SUCCESS(rcNt))
    {
        int vrc = VbglGRAlloc((VMMDevRequestHeader **)&pDevExt->pPowerStateRequest,
                              sizeof(VMMDevPowerStateRequest), VMMDevReq_SetPowerStatus);
        if (RT_FAILURE(vrc))
        {
            LogFunc(("Alloc for pPowerStateRequest failed, vrc=%Rrc\n", vrc));
            rcNt = STATUS_UNSUCCESSFUL;
        }
    }

    if (NT_SUCCESS(rcNt))
    {
        /*
         * Register DPC and ISR.
         */
        LogFlowFunc(("Initializing DPC/ISR ...\n"));

        IoInitializeDpcRequest(pDevExt->pDeviceObject, vgdrvNtDpcHandler);
#ifdef TARGET_NT4
        ULONG uInterruptVector = UINT32_MAX;
        KIRQL irqLevel = UINT8_MAX;
        /* Get an interrupt vector. */
        /* Only proceed if the device provides an interrupt. */
        if (   pDevExt->interruptLevel
            || pDevExt->interruptVector)
        {
            LogFlowFunc(("Getting interrupt vector (HAL): Bus=%u, IRQL=%u, Vector=%u\n",
                         pDevExt->busNumber, pDevExt->interruptLevel, pDevExt->interruptVector));

            uInterruptVector = HalGetInterruptVector(PCIBus,
                                                     pDevExt->busNumber,
                                                     pDevExt->interruptLevel,
                                                     pDevExt->interruptVector,
                                                     &irqLevel,
                                                     &pDevExt->interruptAffinity);
            LogFlowFunc(("HalGetInterruptVector returns vector=%u\n", uInterruptVector));
            if (uInterruptVector == 0)
                LogFunc(("No interrupt vector found!\n"));
        }
        else
            LogFunc(("Device does not provide an interrupt!\n"));
#endif
        if (pDevExt->interruptVector)
        {
            LogFlowFunc(("Connecting interrupt ...\n"));

            rcNt = IoConnectInterrupt(&pDevExt->pInterruptObject,                 /* Out: interrupt object. */
                                      (PKSERVICE_ROUTINE)vgdrvNtIsrHandler,        /* Our ISR handler. */
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
            if (NT_ERROR(rcNt))
                LogFunc(("Could not connect interrupt, rcNt=%#x\n", rcNt));
        }
        else
            LogFunc(("No interrupt vector found!\n"));
    }


#ifdef VBOX_WITH_HGCM
    LogFunc(("Allocating kernel session data ...\n"));
    int vrc = VGDrvCommonCreateKernelSession(&pDevExt->Core, &pDevExt->pKernelSession);
    if (RT_FAILURE(vrc))
    {
        LogFunc(("Failed to allocated kernel session data, vrc=%Rrc\n", vrc));
        rcNt = STATUS_UNSUCCESSFUL;
    }
#endif

    if (NT_SUCCESS(rcNt))
    {
        ULONG uValue = 0;
        NTSTATUS rcNt2 = vgdrvNtRegistryReadDWORD(RTL_REGISTRY_SERVICES, L"VBoxGuest", L"LoggingEnabled", &uValue);
        if (NT_SUCCESS(rcNt2))
        {
            pDevExt->Core.fLoggingEnabled = uValue >= 0xFF;
            if (pDevExt->Core.fLoggingEnabled)
                LogRelFunc(("Logging to host log enabled (%#x)", uValue));
        }

        /* Ready to rumble! */
        LogRelFunc(("Device is ready!\n"));
        VBOXGUEST_UPDATE_DEVSTATE(pDevExt, VGDRVNTDEVSTATE_WORKING);
    }
    else
        pDevExt->pInterruptObject = NULL;

    /** @todo r=bird: The error cleanup here is completely missing. We'll leak a
     *        whole bunch of things... */

    LogFunc(("Returned with rcNt=%#x\n", rcNt));
    return rcNt;
}


/**
 * Cleans up hardware resources.
 * Do not delete DevExt here.
 *
 * @param   pDevObj     Device object.
 */
NTSTATUS vgdrvNtCleanup(PDEVICE_OBJECT pDevObj)
{
    LogFlowFuncEnter();

    PVBOXGUESTDEVEXTWIN pDevExt = (PVBOXGUESTDEVEXTWIN)pDevObj->DeviceExtension;
    if (pDevExt)
    {

#if 0 /** @todo  test & enable cleaning global session data */
#ifdef VBOX_WITH_HGCM
        if (pDevExt->pKernelSession)
        {
            VGDrvCommonCloseSession(pDevExt, pDevExt->pKernelSession);
            pDevExt->pKernelSession = NULL;
        }
#endif
#endif

        if (pDevExt->pInterruptObject)
        {
            IoDisconnectInterrupt(pDevExt->pInterruptObject);
            pDevExt->pInterruptObject = NULL;
        }

        /** @todo cleanup the rest stuff */


#ifdef VBOX_WITH_GUEST_BUGCHECK_DETECTION
        hlpDeregisterBugCheckCallback(pDevExt); /* ignore failure! */
#endif
        /* According to MSDN we have to unmap previously mapped memory. */
        vgdrvNtUnmapVMMDevMemory(pDevExt);
    }

    return STATUS_SUCCESS;
}


/**
 * Unload the driver.
 *
 * @param   pDrvObj     Driver object.
 */
static void vgdrvNtUnload(PDRIVER_OBJECT pDrvObj)
{
    LogFlowFuncEnter();

#ifdef TARGET_NT4
    vgdrvNtCleanup(pDrvObj->DeviceObject);

    /* Destroy device extension and clean up everything else. */
    if (pDrvObj->DeviceObject && pDrvObj->DeviceObject->DeviceExtension)
        VGDrvCommonDeleteDevExt((PVBOXGUESTDEVEXT)pDrvObj->DeviceObject->DeviceExtension);

    /*
     * I don't think it's possible to unload a driver which processes have
     * opened, at least we'll blindly assume that here.
     */
    UNICODE_STRING DosName;
    RtlInitUnicodeString(&DosName, VBOXGUEST_DEVICE_NAME_DOS);
    IoDeleteSymbolicLink(&DosName);

    IoDeleteDevice(pDrvObj->DeviceObject);
#else  /* !TARGET_NT4 */
    /* On a PnP driver this routine will be called after
     * IRP_MN_REMOVE_DEVICE (where we already did the cleanup),
     * so don't do anything here (yet). */
    RT_NOREF1(pDrvObj);
#endif /* !TARGET_NT4 */

    LogFlowFunc(("Returning\n"));
}


/**
 * Create (i.e. Open) file entry point.
 *
 * @param   pDevObj     Device object.
 * @param   pIrp        Request packet.
 */
static NTSTATUS vgdrvNtCreate(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    /** @todo AssertPtrReturn(pIrp); */
    PIO_STACK_LOCATION  pStack   = IoGetCurrentIrpStackLocation(pIrp);
    /** @todo AssertPtrReturn(pStack); */
    PFILE_OBJECT        pFileObj = pStack->FileObject;
    PVBOXGUESTDEVEXTWIN pDevExt  = (PVBOXGUESTDEVEXTWIN)pDevObj->DeviceExtension;
    NTSTATUS            rc       = STATUS_SUCCESS;

    if (pDevExt->enmDevState != VGDRVNTDEVSTATE_WORKING)
    {
        LogFunc(("Device is not working currently, state=%d\n", pDevExt->enmDevState));
        rc = STATUS_UNSUCCESSFUL;
    }
    else if (pStack->Parameters.Create.Options & FILE_DIRECTORY_FILE)
    {
        /*
         * We are not remotely similar to a directory...
         * (But this is possible.)
         */
        LogFlowFunc(("Uhm, we're not a directory!\n"));
        rc = STATUS_NOT_A_DIRECTORY;
    }
    else
    {
#ifdef VBOX_WITH_HGCM
        if (pFileObj)
        {
            LogFlowFunc(("File object type=%d\n", pFileObj->Type));

            int vrc;
            PVBOXGUESTSESSION pSession;
            if (pFileObj->Type == 5 /* File Object */)
            {
                /*
                 * Create a session object if we have a valid file object. This session object
                 * exists for every R3 process.
                 */
                vrc = VGDrvCommonCreateUserSession(&pDevExt->Core, &pSession);
            }
            else
            {
                /* ... otherwise we've been called from R0! */
                vrc = VGDrvCommonCreateKernelSession(&pDevExt->Core, &pSession);
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

    LogFlowFunc(("Returning rc=%#x\n", rc));
    return rc;
}


/**
 * Close file entry point.
 *
 * @param   pDevObj     Device object.
 * @param   pIrp        Request packet.
 */
static NTSTATUS vgdrvNtClose(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    PVBOXGUESTDEVEXTWIN pDevExt  = (PVBOXGUESTDEVEXTWIN)pDevObj->DeviceExtension;
    PIO_STACK_LOCATION  pStack   = IoGetCurrentIrpStackLocation(pIrp);
    PFILE_OBJECT        pFileObj = pStack->FileObject;

    LogFlowFunc(("pDevExt=0x%p, pFileObj=0x%p, FsContext=0x%p\n", pDevExt, pFileObj, pFileObj->FsContext));

#ifdef VBOX_WITH_HGCM
    /* Close both, R0 and R3 sessions. */
    PVBOXGUESTSESSION pSession = (PVBOXGUESTSESSION)pFileObj->FsContext;
    if (pSession)
        VGDrvCommonCloseSession(&pDevExt->Core, pSession);
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
static NTSTATUS vgdrvNtIOCtl(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    NTSTATUS            Status   = STATUS_SUCCESS;
    PVBOXGUESTDEVEXTWIN pDevExt  = (PVBOXGUESTDEVEXTWIN)pDevObj->DeviceExtension;
    PIO_STACK_LOCATION  pStack   = IoGetCurrentIrpStackLocation(pIrp);
    unsigned int        uCmd     = (unsigned int)pStack->Parameters.DeviceIoControl.IoControlCode;

    char               *pBuf     = (char *)pIrp->AssociatedIrp.SystemBuffer; /* All requests are buffered. */
    size_t              cbData   = pStack->Parameters.DeviceIoControl.InputBufferLength;
    size_t              cbOut    = 0;

    /* Do we have a file object associated?*/
    PFILE_OBJECT        pFileObj = pStack->FileObject;
    PVBOXGUESTSESSION   pSession = NULL;
    if (pFileObj) /* ... then we might have a session object as well! */
        pSession = (PVBOXGUESTSESSION)pFileObj->FsContext;

    LogFlowFunc(("uCmd=%u, pDevExt=0x%p, pSession=0x%p\n", uCmd, pDevExt, pSession));

    /* We don't have a session associated with the file object? So this seems
     * to be a kernel call then. */
    /** @todo r=bird: What on earth is this supposed to be? Each kernel session
     *        shall have its own context of course, no hacks, pleeease. */
    if (pSession == NULL)
    {
        LogFunc(("XXX: BUGBUG: FIXME: Using ugly kernel session data hack ...\n"));
#ifdef DEBUG_andy
        RTLogBackdoorPrintf("XXX: BUGBUG: FIXME: Using ugly kernel session data hack ... Please don't forget to fix this one, Andy!\n");
#endif
        pSession = pDevExt->pKernelSession;
    }

    /* Verify that it's a buffered CTL. */
    if ((pStack->Parameters.DeviceIoControl.IoControlCode & 0x3) == METHOD_BUFFERED)
    {
        /*
         * Process the common IOCtls.
         */
        size_t cbDataReturned;
        int vrc = VGDrvCommonIoCtl(uCmd, &pDevExt->Core, pSession, pBuf, cbData, &cbDataReturned);

        LogFlowFunc(("rc=%Rrc, pBuf=0x%p, cbData=%u, cbDataReturned=%u\n",
                     vrc, pBuf, cbData, cbDataReturned));

        if (RT_SUCCESS(vrc))
        {
            if (RT_UNLIKELY(   cbDataReturned > cbData
                            || cbDataReturned > pStack->Parameters.DeviceIoControl.OutputBufferLength))
            {
                LogFunc(("Too much output data %u - expected %u!\n", cbDataReturned, cbData));
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
    }
    else
    {
        LogFunc(("Not buffered request (%#x) - not supported\n", pStack->Parameters.DeviceIoControl.IoControlCode));
        Status = STATUS_NOT_SUPPORTED;
    }

    pIrp->IoStatus.Status = Status;
    pIrp->IoStatus.Information = cbOut;

    IoCompleteRequest(pIrp, IO_NO_INCREMENT);

    //LogFlowFunc(("Returned cbOut=%d rc=%#x\n", cbOut, Status));
    return Status;
}

/**
 * Internal Device I/O Control entry point.
 *
 * @param   pDevObj     Device object.
 * @param   pIrp        Request packet.
 */
static NTSTATUS vgdrvNtInternalIOCtl(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    NTSTATUS            Status      = STATUS_SUCCESS;
    PVBOXGUESTDEVEXTWIN pDevExt     = (PVBOXGUESTDEVEXTWIN)pDevObj->DeviceExtension;
    PIO_STACK_LOCATION  pStack      = IoGetCurrentIrpStackLocation(pIrp);
    unsigned int        uCmd        = (unsigned int)pStack->Parameters.DeviceIoControl.IoControlCode;
    bool                fProcessed  = false;
    unsigned            Info        = 0;

    /*
     * Override common behavior of some operations.
     */
    /** @todo r=bird: Better to add dedicated worker functions for this! */
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

    /*
     * No override, go to common code.
     */
    return vgdrvNtIOCtl(pDevObj, pIrp);
}


/**
 * IRP_MJ_SYSTEM_CONTROL handler.
 *
 * @returns NT status code
 * @param   pDevObj     Device object.
 * @param   pIrp        IRP.
 */
static NTSTATUS vgdrvNtSystemControl(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    PVBOXGUESTDEVEXTWIN pDevExt = (PVBOXGUESTDEVEXTWIN)pDevObj->DeviceExtension;

    LogFlowFuncEnter();

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
static NTSTATUS vgdrvNtShutdown(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    PVBOXGUESTDEVEXTWIN pDevExt = (PVBOXGUESTDEVEXTWIN)pDevObj->DeviceExtension;
    LogFlowFuncEnter();

    VMMDevPowerStateRequest *pReq = pDevExt->pPowerStateRequest;
    if (pReq)
    {
        pReq->header.requestType = VMMDevReq_SetPowerStatus;
        pReq->powerState = VMMDevPowerState_PowerOff;

        int rc = VbglGRPerform(&pReq->header);
        if (RT_FAILURE(rc))
            LogFunc(("Error performing request to VMMDev, rc=%Rrc\n", rc));
    }

    /* just in case, since we shouldn't normally get here. */
    pIrp->IoStatus.Information = 0;
    pIrp->IoStatus.Status = STATUS_SUCCESS;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}


/**
 * Stub function for functions we don't implemented.
 *
 * @returns STATUS_NOT_SUPPORTED
 * @param   pDevObj     Device object.
 * @param   pIrp        IRP.
 */
static NTSTATUS vgdrvNtNotSupportedStub(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    RT_NOREF1(pDevObj);
    LogFlowFuncEnter();

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
static void vgdrvNtDpcHandler(PKDPC pDPC, PDEVICE_OBJECT pDevObj, PIRP pIrp, PVOID pContext)
{
    RT_NOREF3(pDPC, pIrp, pContext);
    PVBOXGUESTDEVEXTWIN pDevExt = (PVBOXGUESTDEVEXTWIN)pDevObj->DeviceExtension;
    Log3Func(("pDevExt=0x%p\n", pDevExt));

    /* Test & reset the counter. */
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
     * in vgdrvNtIsrHandler(). */
    VGDrvCommonWaitDoWakeUps(&pDevExt->Core);
}


/**
 * ISR handler.
 *
 * @return  BOOLEAN         Indicates whether the IRQ came from us (TRUE) or not (FALSE).
 * @param   pInterrupt      Interrupt that was triggered.
 * @param   pServiceContext Context specific pointer.
 */
static BOOLEAN vgdrvNtIsrHandler(PKINTERRUPT pInterrupt, PVOID pServiceContext)
{
    RT_NOREF1(pInterrupt);
    PVBOXGUESTDEVEXTWIN pDevExt = (PVBOXGUESTDEVEXTWIN)pServiceContext;
    if (pDevExt == NULL)
        return FALSE;

    /*Log3Func(("pDevExt=0x%p, pVMMDevMemory=0x%p\n", pDevExt, pDevExt ? pDevExt->pVMMDevMemory : NULL));*/

    /* Enter the common ISR routine and do the actual work. */
    BOOLEAN fIRQTaken = VGDrvCommonISR(&pDevExt->Core);

    /* If we need to wake up some events we do that in a DPC to make
     * sure we're called at the right IRQL. */
    if (fIRQTaken)
    {
        Log3Func(("IRQ was taken! pInterrupt=0x%p, pDevExt=0x%p\n", pInterrupt, pDevExt));
        if (ASMAtomicUoReadU32(   &pDevExt->Core.u32MousePosChangedSeq)
                               || !RTListIsEmpty(&pDevExt->Core.WakeUpList))
        {
            Log3Func(("Requesting DPC ...\n"));
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
void VGDrvNativeISRMousePollEvent(PVBOXGUESTDEVEXT pDevExt)
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
static NTSTATUS vgdrvNtRegistryReadDWORD(ULONG ulRoot, PCWSTR pwszPath, PWSTR pwszName, PULONG puValue)
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
static NTSTATUS vgdrvNtScanPCIResourceList(PCM_RESOURCE_LIST pResList, PVBOXGUESTDEVEXTWIN pDevExt)
{
    /* Enumerate the resource list. */
    LogFlowFunc(("Found %d resources\n",
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
                    LogFlowFunc(("I/O range: Base=%08x:%08x, length=%08x\n",
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

                    LogFunc(("I/O range for VMMDev found! Base=%08x:%08x, length=%08x\n",
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
                LogFunc(("Interrupt: Level=%x, vector=%x, mode=%x\n",
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
                    LogFlowFunc(("Memory range: Base=%08x:%08x, length=%08x\n",
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

                        LogFunc(("Memory range for VMMDev found! Base = %08x:%08x, Length = %08x\n",
                                 pPartialData->u.Memory.Start.HighPart,
                                 pPartialData->u.Memory.Start.LowPart,
                                 pPartialData->u.Memory.Length));

                        /* Next item ... */
                        rangeCount++; pBaseAddress++; cMMIORange++;
                    }
                    else
                        LogFunc(("Ignoring memory: Flags=%08x\n", pPartialData->Flags));
                }
                break;
            }

            default:
            {
                LogFunc(("Unhandled resource found, type=%d\n", pPartialData->Type));
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
static NTSTATUS vgdrvNtMapVMMDevMemory(PVBOXGUESTDEVEXTWIN pDevExt, PHYSICAL_ADDRESS PhysAddr, ULONG cbToMap,
                                       void **ppvMMIOBase, uint32_t *pcbMMIO)
{
    AssertPtrReturn(pDevExt, VERR_INVALID_POINTER);
    AssertPtrReturn(ppvMMIOBase, VERR_INVALID_POINTER);
    /* pcbMMIO is optional. */

    NTSTATUS rc = STATUS_SUCCESS;
    if (PhysAddr.LowPart > 0) /* We're mapping below 4GB. */
    {
         VMMDevMemory *pVMMDevMemory = (VMMDevMemory *)MmMapIoSpace(PhysAddr, cbToMap, MmNonCached);
         LogFlowFunc(("pVMMDevMemory = %#x\n", pVMMDevMemory));
         if (pVMMDevMemory)
         {
             LogFunc(("VMMDevMemory: Version = %#x, Size = %d\n", pVMMDevMemory->u32Version, pVMMDevMemory->u32Size));

             /* Check version of the structure; do we have the right memory version? */
             if (pVMMDevMemory->u32Version == VMMDEV_MEMORY_VERSION)
             {
                 /* Save results. */
                 *ppvMMIOBase = pVMMDevMemory;
                 if (pcbMMIO) /* Optional. */
                     *pcbMMIO = pVMMDevMemory->u32Size;

                 LogFlowFunc(("VMMDevMemory found and mapped! pvMMIOBase = 0x%p\n", *ppvMMIOBase));
             }
             else
             {
                 /* Not our version, refuse operation and unmap the memory. */
                 LogFunc(("Wrong version (%u), refusing operation!\n", pVMMDevMemory->u32Version));

                 vgdrvNtUnmapVMMDevMemory(pDevExt);
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
void vgdrvNtUnmapVMMDevMemory(PVBOXGUESTDEVEXTWIN pDevExt)
{
    LogFlowFunc(("pVMMDevMemory = %#x\n", pDevExt->Core.pVMMDevMemory));
    if (pDevExt->Core.pVMMDevMemory)
    {
        MmUnmapIoSpace((void*)pDevExt->Core.pVMMDevMemory, pDevExt->vmmDevPhysMemoryLength);
        pDevExt->Core.pVMMDevMemory = NULL;
    }

    pDevExt->vmmDevPhysMemoryAddress.QuadPart = 0;
    pDevExt->vmmDevPhysMemoryLength = 0;
}


/**
 * Translates NT version to VBox OS.
 *
 * @returns VBox OS type.
 * @param   enmNtVer            The NT version.
 */
VBOXOSTYPE vgdrvNtVersionToOSType(VGDRVNTVER enmNtVer)
{
    VBOXOSTYPE enmOsType;
    switch (enmNtVer)
    {
        case VGDRVNTVER_WINNT4:
            enmOsType = VBOXOSTYPE_WinNT4;
            break;

        case VGDRVNTVER_WIN2K:
            enmOsType = VBOXOSTYPE_Win2k;
            break;

        case VGDRVNTVER_WINXP:
#if ARCH_BITS == 64
            enmOsType = VBOXOSTYPE_WinXP_x64;
#else
            enmOsType = VBOXOSTYPE_WinXP;
#endif
            break;

        case VGDRVNTVER_WIN2K3:
#if ARCH_BITS == 64
            enmOsType = VBOXOSTYPE_Win2k3_x64;
#else
            enmOsType = VBOXOSTYPE_Win2k3;
#endif
            break;

        case VGDRVNTVER_WINVISTA:
#if ARCH_BITS == 64
            enmOsType = VBOXOSTYPE_WinVista_x64;
#else
            enmOsType = VBOXOSTYPE_WinVista;
#endif
            break;

        case VGDRVNTVER_WIN7:
#if ARCH_BITS == 64
            enmOsType = VBOXOSTYPE_Win7_x64;
#else
            enmOsType = VBOXOSTYPE_Win7;
#endif
            break;

        case VGDRVNTVER_WIN8:
#if ARCH_BITS == 64
            enmOsType = VBOXOSTYPE_Win8_x64;
#else
            enmOsType = VBOXOSTYPE_Win8;
#endif
            break;

        case VGDRVNTVER_WIN81:
#if ARCH_BITS == 64
            enmOsType = VBOXOSTYPE_Win81_x64;
#else
            enmOsType = VBOXOSTYPE_Win81;
#endif
            break;

        case VGDRVNTVER_WIN10:
#if ARCH_BITS == 64
            enmOsType = VBOXOSTYPE_Win10_x64;
#else
            enmOsType = VBOXOSTYPE_Win10;
#endif
            break;

        default:
            /* We don't know, therefore NT family. */
            enmOsType = VBOXOSTYPE_WinNT;
            break;
    }
    return enmOsType;
}

#ifdef VBOX_STRICT

/**
 * A quick implementation of AtomicTestAndClear for uint32_t and multiple bits.
 */
static uint32_t vgdrvNtAtomicBitsTestAndClear(void *pu32Bits, uint32_t u32Mask)
{
    AssertPtrReturn(pu32Bits, 0);
    LogFlowFunc(("*pu32Bits=%#x, u32Mask=%#x\n", *(uint32_t *)pu32Bits, u32Mask));
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
    LogFlowFunc(("Returning %#x\n", u32Result));
    return u32Result;
}


static void vgdrvNtTestAtomicTestAndClearBitsU32(uint32_t u32Mask, uint32_t u32Bits, uint32_t u32Exp)
{
    ULONG u32Bits2 = u32Bits;
    uint32_t u32Result = vgdrvNtAtomicBitsTestAndClear(&u32Bits2, u32Mask);
    if (   u32Result != u32Exp
        || (u32Bits2 & u32Mask)
        || (u32Bits2 & u32Result)
        || ((u32Bits2 | u32Result) != u32Bits)
       )
        AssertLogRelMsgFailed(("TEST FAILED: u32Mask=%#x, u32Bits (before)=%#x, u32Bits (after)=%#x, u32Result=%#x, u32Exp=%#x\n",
                               u32Mask, u32Bits, u32Bits2, u32Result));
}


static void vgdrvNtDoTests(void)
{
    vgdrvNtTestAtomicTestAndClearBitsU32(0x00, 0x23, 0);
    vgdrvNtTestAtomicTestAndClearBitsU32(0x11, 0, 0);
    vgdrvNtTestAtomicTestAndClearBitsU32(0x11, 0x22, 0);
    vgdrvNtTestAtomicTestAndClearBitsU32(0x11, 0x23, 0x1);
    vgdrvNtTestAtomicTestAndClearBitsU32(0x11, 0x32, 0x10);
    vgdrvNtTestAtomicTestAndClearBitsU32(0x22, 0x23, 0x22);
}

#endif /* VBOX_STRICT */

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
static VOID vgdrvNtDpcLatencyCallback(PKDPC pDpc, PVOID pvDeferredContext, PVOID SystemArgument1, PVOID SystemArgument2)
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
int VGDrvNtIOCtl_DpcLatencyChecker(void)
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
    KeInitializeDpc(&pData->Dpc, vgdrvNtDpcLatencyCallback, pData);
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

