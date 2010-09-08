/** @file
 *
 * VBoxGuest-win-legacy - Windows NT4 specifics.
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
#include "VBoxGuest-win.h"
#include "VBoxGuestInternal.h"
#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/version.h>
#include <VBox/VBoxGuestLib.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/

/* Reenable logging, this was #undef'ed on iprt/log.h for RING0. */
#define LOG_ENABLED

#ifndef PCI_MAX_BUSES
# define PCI_MAX_BUSES 256
#endif


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
RT_C_DECLS_BEGIN
static NTSTATUS vboxguestwinnt4FindPCIDevice(PULONG pBusNumber, PPCI_SLOT_NUMBER pSlotNumber);
static void     vboxguestwinnt4FreeDeviceResources(PDRIVER_OBJECT pDrvObj, PDEVICE_OBJECT pDevObj);
RT_C_DECLS_END

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, vboxguestwinnt4CreateDevice)
#pragma alloc_text (INIT, vboxguestwinnt4FindPCIDevice)
#pragma alloc_text (INIT, vboxguestwinnt4FreeDeviceResources)
#endif


/**
 * Helper function to create the device object
 *
 * @returns NT status code
 * @param
 */
NTSTATUS vboxguestwinnt4CreateDevice(PDRIVER_OBJECT pDrvObj, PDEVICE_OBJECT pDevObj, PUNICODE_STRING pRegPath)
{
    int vrc = VINF_SUCCESS;
    NTSTATUS rc = STATUS_SUCCESS;

    Log(("VBoxGuest::vboxguestwinnt4CreateDevice: pDrvObj=%x, pDevObj=%x, pRegPath=%x\n",
         pDrvObj, pDevObj, pRegPath));

    /*
     * Find our virtual PCI device
     */
    ULONG uBusNumber, uSlotNumber;
    rc = vboxguestwinnt4FindPCIDevice(&uBusNumber, (PCI_SLOT_NUMBER*)&uSlotNumber);
    if (NT_ERROR(rc))
        Log(("VBoxGuest::vboxguestwinnt4CreateDevice: Device not found!\n"));

    PDEVICE_OBJECT pDeviceObject = NULL;
    if (NT_SUCCESS(rc))
    {
        /*
         * Create device.
         */
        UNICODE_STRING szDevName;
        RtlInitUnicodeString(&szDevName, VBOXGUEST_DEVICE_NAME_NT);
        rc = IoCreateDevice(pDrvObj, sizeof(VBOXGUESTDEVEXT), &szDevName, FILE_DEVICE_UNKNOWN, 0, FALSE, &pDeviceObject);
        if (NT_SUCCESS(rc))
        {
            Log(("VBoxGuest::vboxguestwinnt4CreateDevice: Device created\n"));

            UNICODE_STRING DosName;
            RtlInitUnicodeString(&DosName, VBOXGUEST_DEVICE_NAME_DOS);
            rc = IoCreateSymbolicLink(&DosName, &szDevName);
            if (NT_ERROR(rc))
            {
                Log(("VBoxGuest::vboxguestwinnt4CreateDevice: IoCreateSymbolicLink failed with rc = %#x\n", rc));
                //IoDeleteDevice(pDeviceObject);
            }
            Log(("VBoxGuest::vboxguestwinnt4CreateDevice: Symlink created\n"));
        }
        else
            Log(("VBoxGuest::vboxguestwinnt4CreateDevice: IoCreateDevice failed with rc = %#x\n", rc));
    }

    /*
     * Setup the device extension.
     */
    PVBOXGUESTDEVEXT pDevExt = NULL;
    if (NT_SUCCESS(rc))
    {
        Log(("VBoxGuest::vboxguestwinnt4CreateDevice: Setting up device extension ...\n"));

        pDevExt = (PVBOXGUESTDEVEXT)pDeviceObject->DeviceExtension;
        Assert(pDevExt);
        RtlZeroMemory(pDevExt, sizeof(VBOXGUESTDEVEXT));
    }

    if (   NT_SUCCESS(rc)
        && pDevExt)
    {
        /* Store a reference to ourself. */
        pDevExt->win.s.pDeviceObject = pDeviceObject;

        /* Store bus and slot number we've queried before. */
        pDevExt->win.s.busNumber = uBusNumber;
        pDevExt->win.s.slotNumber = uSlotNumber;

    #ifdef VBOX_WITH_GUEST_BUGCHECK_DETECTION
        rc = hlpRegisterBugCheckCallback(pDevExt);
    #endif
    }

    if (NT_SUCCESS(rc))
    {
        rc = vboxguestwinInit(pDrvObj, pDevObj, pRegPath);
    }

    Log(("VBoxGuest::vboxguestwinnt4CreateDevice: Returning rc = 0x%x\n", rc));
    return rc;
}


/**
 * Helper function to handle the PCI device lookup
 *
 * @returns NT error codes
 */
static NTSTATUS vboxguestwinnt4FindPCIDevice(PULONG pBusNumber, PPCI_SLOT_NUMBER pSlotNumber)
{
    NTSTATUS rc;

    ULONG busNumber;
    ULONG deviceNumber;
    ULONG functionNumber;
    PCI_SLOT_NUMBER slotNumber;
    PCI_COMMON_CONFIG pciData;

    Log(("VBoxGuest::vboxguestwinnt4FindPCIDevice\n"));

    rc = STATUS_DEVICE_DOES_NOT_EXIST;
    slotNumber.u.AsULONG = 0;

    /* Scan each bus. */
    for (busNumber = 0; busNumber < PCI_MAX_BUSES; busNumber++)
    {
        /* Scan each device. */
        for (deviceNumber = 0; deviceNumber < PCI_MAX_DEVICES; deviceNumber++)
        {
            slotNumber.u.bits.DeviceNumber = deviceNumber;

            /* Scan each function (not really required...). */
            for (functionNumber = 0; functionNumber < PCI_MAX_FUNCTION; functionNumber++)
            {
                slotNumber.u.bits.FunctionNumber = functionNumber;

                /* Have a look at what's in this slot. */
                if (!HalGetBusData(PCIConfiguration, busNumber, slotNumber.u.AsULONG,
                                   &pciData, sizeof(ULONG)))
                {
                    /* No such bus, we're done with it. */
                    deviceNumber = PCI_MAX_DEVICES;
                    break;
                }

                if (pciData.VendorID == PCI_INVALID_VENDORID)
                {
                    /* We have to proceed to the next function. */
                    continue;
                }

                /* Check if it's another device. */
                if ((pciData.VendorID != VMMDEV_VENDORID) ||
                    (pciData.DeviceID != VMMDEV_DEVICEID))
                {
                    continue;
                }

                /* Hooray, we've found it! */
                Log(("VBoxGuest::vboxguestwinnt4FindPCIDevice: Device found!\n"));

                *pBusNumber = busNumber;
                *pSlotNumber = slotNumber;
                rc = STATUS_SUCCESS;
            }
        }
    }

    return rc;
}

