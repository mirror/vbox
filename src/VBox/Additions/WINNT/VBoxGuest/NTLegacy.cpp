/** @file
 *
 * VBoxGuest -- VirtualBox Win32 guest support driver
 *
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

// enable backdoor logging
//#define LOG_ENABLED

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "NTLegacy.h"
#include "Helper.h"

#include <VBox/VBoxGuestLib.h>

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
extern "C"
{
static NTSTATUS            findPCIDevice(PULONG pBusNumber, PPCI_SLOT_NUMBER pSlotNumber);
static void                freeDeviceResources(PDRIVER_OBJECT pDrvObj, PDEVICE_OBJECT pDevObj);
}

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, ntCreateDevice)
#pragma alloc_text (INIT, findPCIDevice)
#pragma alloc_text (INIT, freeDeviceResources)
#endif

/**
 * Helper function to create the device object
 *
 * @returns NT status code
 * @param
 */
NTSTATUS ntCreateDevice(PDRIVER_OBJECT pDrvObj, PDEVICE_OBJECT pDevObj, PUNICODE_STRING pRegPath)
{
    ULONG busNumber, slotNumber;
//    ULONG i;
    NTSTATUS rc = STATUS_SUCCESS;

    dprintf(("VBoxGuest::ntCreateDevice: entered\n"));

    // find our virtual PCI device
    rc = findPCIDevice(&busNumber, (PCI_SLOT_NUMBER*)&slotNumber);
    if (!NT_SUCCESS(rc))
    {
        dprintf(("VBoxGuest::createDevice: device not found, returning\n"));
        return rc;
    }

    /*
     * Create device.
     */
    PDEVICE_OBJECT deviceObject = NULL;
    UNICODE_STRING  DevName;
    RtlInitUnicodeString(&DevName, VBOXGUEST_DEVICE_NAME_NT);
    rc = IoCreateDevice(pDrvObj, sizeof(VBOXGUESTDEVEXT), &DevName, FILE_DEVICE_UNKNOWN, 0, FALSE, &deviceObject);
    if (!NT_SUCCESS(rc))
    {
        dprintf(("VBoxGuest::ntCreateDevice: IoCreateDevice failed with rc=%#x!\n", rc));
        return rc;
    }
    dprintf(("VBoxGuest::ntCreateDevice: device created\n"));
    UNICODE_STRING DosName;
    RtlInitUnicodeString(&DosName, VBOXGUEST_DEVICE_NAME_DOS);
    rc = IoCreateSymbolicLink(&DosName, &DevName);
    if (!NT_SUCCESS(rc))
    {
        dprintf(("VBoxGuest::ntCreateDevice: IoCreateSymbolicLink failed with rc=%#x!\n", rc));
        IoDeleteDevice(deviceObject);
        return rc;
    }
    dprintf(("VBoxGuest::ntCreateDevice: symlink created\n"));

    /*
     * Setup the device extension.
     */
    PVBOXGUESTDEVEXT pDevExt = (PVBOXGUESTDEVEXT)deviceObject->DeviceExtension;
    RtlZeroMemory(pDevExt, sizeof(VBOXGUESTDEVEXT));

    if (pDevObj)
    {
        pDevExt->nextLowerDriver = IoAttachDeviceToDeviceStack(deviceObject, pDevObj);
        if (pDevExt->nextLowerDriver == NULL)
        {
            dprintf(("VBoxGuest::ntCreateDevice: IoAttachDeviceToDeviceStack did not give a nextLowerDrive\n"));
            IoDeleteSymbolicLink(&DosName);
            IoDeleteDevice(deviceObject);
            return STATUS_NO_SUCH_DEVICE;
        }
    }
    // store a reference to ourself
    pDevExt->deviceObject = deviceObject;
    // store bus and slot number we've queried before
    pDevExt->busNumber = busNumber;
    pDevExt->slotNumber = slotNumber;

    //
    // let's have a look at what our PCI adapter offers
    //
    dprintf(("VBoxGuest::ntCreateDevice: starting to scan PCI resources of VBoxGuest\n"));
    // assign the PCI resources
    PCM_RESOURCE_LIST resourceList;
    UNICODE_STRING classNameString;
    RtlInitUnicodeString(&classNameString, L"VBoxGuestAdapter");
    rc = HalAssignSlotResources(pRegPath, &classNameString,
                                pDrvObj, pDevObj,
                                PCIBus, busNumber, slotNumber,
                                &resourceList);
    if (!NT_SUCCESS(rc))
    {
        dprintf(("VBoxGuest::ntCreateDevice: HalAssignSlotResources failed with rc=%#x!\n", rc));
        freeDeviceResources(pDrvObj, pDevObj);
        return rc;
    }

    rc = VBoxScanPCIResourceList(resourceList, pDevExt);

    rc = VbglInit (pDevExt->startPortAddress, pDevExt->pVMMDevMemory);
    if (!VBOX_SUCCESS(rc))
    {
        dprintf(("VBoxGuest::START_DEVICE: VbglInit failed. rc = %d\n", rc));
    }


    rc = VbglGRAlloc ((VMMDevRequestHeader **)&pDevExt->irqAckEvents, sizeof (VMMDevEvents), VMMDevReq_AcknowledgeEvents);
    if (!VBOX_SUCCESS(rc))
    {
       dprintf(("VBoxGuest::START_DEVICE: VbglAlloc failed. rc = %d\n", rc));
    }

#if 0
    //
    // now proceed to the busmaster DMA stuff
    //

    DEVICE_DESCRIPTION deviceDescription;
    ULONG numberOfMapRegisters;
    deviceDescription.Version = DEVICE_DESCRIPTION_VERSION;
    deviceDescription.Master = TRUE;
    deviceDescription.ScatterGather = TRUE;
    deviceDescription.BusNumber = pDevExt->busNumber;
    deviceDescription.InterfaceType = PCIBus;
    deviceDescription.MaximumLength = MAXIMUM_TRANSFER_LENGTH;
    pDevExt->adapterObject = HalGetAdapter(&deviceDescription, &numberOfMapRegisters);
    if (pDevExt->adapterObject == NULL)
    {
        dprintf(("VBoxGuest::ntCreateDevice: HalGetAdapter failed!\n"));
        freeDeviceResources(pDrvObj, pDevObj);
        return rc;
    }

    // @todo allocate S/G buffer
#endif


    //
    // it's time to map the I/O and memory spaces
    //

    // Map physical address of VMMDev memory
    rc = hlpVBoxMapVMMDevMemory (pDevExt);
    if (!NT_SUCCESS(rc))
    {
        dprintf(("VBoxGuest::ntCreateDevice: Unable to map VMMDev Memory, rc=%#x!\n", rc));
        freeDeviceResources(pDrvObj, pDevObj);
        return rc;
    }

    //
    // now we need an ISR and DPC
    //

    // register DPC routine
    dprintf(("VBoxGuest::ntCreateDevice: initializing DPC...\n"));
    IoInitializeDpcRequest(pDevExt->deviceObject, VBoxGuestDpcHandler);
    // get an interrupt vector
    ULONG vector;
    KIRQL irql;
    KAFFINITY affinity;
    // only proceed if the device provides an interrupt
    if (pDevExt->interruptLevel || pDevExt->interruptVector)
    {
        vector = HalGetInterruptVector(PCIBus,
                                       pDevExt->busNumber,
                                       pDevExt->interruptLevel,
                                       pDevExt->interruptVector,
                                       &irql,
                                       &affinity);
        dprintf(("VBoxGuest::ntCreateDevice: HalGetInterruptVector returns vector %u\n", vector));
        rc = IoConnectInterrupt(&pDevExt->interruptObject,              // out: interrupt object
                                (PKSERVICE_ROUTINE)VBoxGuestIsrHandler, // ISR
                                pDevExt,                                // context
                                NULL,                                   // optional spinlock
                                vector,                                 // interrupt vector
                                irql,                                   // interrupt level
                                irql,                                   // interrupt level
                                pDevExt->interruptMode,                 // LevelSensitive or Latched
                                TRUE,                                   // shareable interrupt
                                affinity,                               // CPU affinity
                                FALSE);                                 // don't save FPU stack
        if (!NT_SUCCESS(rc))
        {
            dprintf(("VBoxGuest::ntCreateDevice: Unable to connect interrupt, rc=%#x!\n", rc));
            pDevExt->interruptObject = NULL;
            freeDeviceResources(pDrvObj, pDevObj);
            return rc;
        }
        dprintf(("VBoxGuest::ntCreateDevice: IRQ connected!\n"));
    }

    if (NT_SUCCESS(rc))
    {
        // create our thread to inform the VBoxMouse driver
        rc = createThreads(pDevExt);
    }

    if (NT_SUCCESS(rc))
    {
        // initialize the event notification semaphore
        KeInitializeEvent(&pDevExt->keventNotification, NotificationEvent, FALSE);
    }

    rc = hlpVBoxReportGuestInfo (pDevExt);
    if (!NT_SUCCESS(rc))
    {
        dprintf(("VBoxGuest::AddDevice: could not report information to host, rc = %d, exiting!\n", rc));
        freeDeviceResources(pDrvObj, pDevObj);
        return STATUS_UNSUCCESSFUL;
    }

    /** @todo cleanup on failure */

    VBoxInitMemBalloon(pDevExt);

    // ready to rumble!
    pDevExt->devState = WORKING;
    dprintf(("returning from createDevice with rc = 0x%x\n", rc));
    return rc;
}


/**
 * Helper function to handle the PCI device lookup
 *
 * @returns NT error codes
 */
static NTSTATUS findPCIDevice(PULONG pBusNumber, PPCI_SLOT_NUMBER pSlotNumber)
{
    NTSTATUS rc;

    ULONG busNumber;
    ULONG deviceNumber;
    ULONG functionNumber;
    PCI_SLOT_NUMBER slotNumber;
    PCI_COMMON_CONFIG pciData;

    dprintf(("findPCIDevice\n"));

    rc = STATUS_DEVICE_DOES_NOT_EXIST;
    slotNumber.u.AsULONG = 0;
    // scan each bus
    for (busNumber = 0; busNumber < PCI_MAX_BUSES; busNumber++)
    {
        // scan each device
        for (deviceNumber = 0; deviceNumber < PCI_MAX_DEVICES; deviceNumber++)
        {
            slotNumber.u.bits.DeviceNumber = deviceNumber;
            // scan each function (not really required...)
            for (functionNumber = 0; functionNumber < PCI_MAX_FUNCTION; functionNumber++)
            {
                slotNumber.u.bits.FunctionNumber = functionNumber;
                // have a look at what's in this slot
                if (!HalGetBusData(PCIConfiguration, busNumber, slotNumber.u.AsULONG,
                                   &pciData, sizeof(ULONG)))
                {
                    // no such bus, we're done with it
                    deviceNumber = PCI_MAX_DEVICES;
                    break;
                }

                if (pciData.VendorID == PCI_INVALID_VENDORID)
                {
                    // we have to proceed to the next function
                    continue;
                }

                // check if it's another device
                if ((pciData.VendorID != VMMDEV_VENDORID) ||
                    (pciData.DeviceID != VMMDEV_DEVICEID))
                {
                    continue;
                }

                // Hooray, we've found it!
                dprintf(("device found!\n"));
                *pBusNumber = busNumber;
                *pSlotNumber = slotNumber;
                rc = STATUS_SUCCESS;
            }
        }
    }

    return rc;
}

/**
 * Helper function to cleanup resources
 *
 * @param   pDrvObj     Driver object.
 * @param   pDevObj     Device object.
 */
static void freeDeviceResources(PDRIVER_OBJECT pDrvObj, PDEVICE_OBJECT pDevObj)
{
    PVBOXGUESTDEVEXT pDevExt = (PVBOXGUESTDEVEXT)pDevObj->DeviceExtension;

    // if there's no device extension, we're screwed
    if (!pDevExt)
    {
        dprintf(("freeDeviceResources: FATAL ERROR! device extension pointer is NULL! Not freeing resources!\n"));
        return;
    }

    // indicate that the device is no longer ready
    pDevExt->devState = STOPPED;

    // disconnect interrupts
    if (pDevExt->interruptObject)
    {
        IoDisconnectInterrupt(pDevExt->interruptObject);
    }

    // unmap mem/io resources
    hlpVBoxUnmapVMMDevMemory (pDevExt);

    VBoxCleanupMemBalloon(pDevExt);
}

