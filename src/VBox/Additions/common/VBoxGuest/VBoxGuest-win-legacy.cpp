/* $Id$ */
/** @file
 * VBoxGuest-win-legacy - Windows NT4 specifics.
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
#include "VBoxGuest-win.h"
#include "VBoxGuestInternal.h"
#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/version.h>
#include <VBox/VBoxGuestLib.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#ifndef PCI_MAX_BUSES
# define PCI_MAX_BUSES 256
#endif


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
RT_C_DECLS_BEGIN
static NTSTATUS vbgdNt4FindPciDevice(PULONG pulBusNumber, PPCI_SLOT_NUMBER pSlotNumber);
RT_C_DECLS_END

#ifdef ALLOC_PRAGMA
# pragma alloc_text(INIT, vbgdNt4CreateDevice)
# pragma alloc_text(INIT, vbgdNt4FindPciDevice)
#endif


/**
 * Legacy helper function to create the device object.
 *
 * @returns NT status code.
 *
 * @param   pDrvObj         The driver object.
 * @param   pDevObj         Unused. NULL. Dunno why it's here, makes no sense.
 * @param   pRegPath        The driver registry path.
 */
NTSTATUS vbgdNt4CreateDevice(PDRIVER_OBJECT pDrvObj, PDEVICE_OBJECT pDevObj, PUNICODE_STRING pRegPath)
{
    Log(("VBoxGuest::vbgdNt4CreateDevice: pDrvObj=%p, pDevObj=%p, pRegPath=%p\n", pDrvObj, pDevObj, pRegPath));

    /*
     * Find our virtual PCI device
     */
    ULONG uBusNumber;
    PCI_SLOT_NUMBER SlotNumber;
    NTSTATUS rc = vbgdNt4FindPciDevice(&uBusNumber, &SlotNumber);
    if (NT_ERROR(rc))
    {
        Log(("VBoxGuest::vbgdNt4CreateDevice: Device not found!\n"));
        return rc;
    }

    /*
     * Create device.
     */
    UNICODE_STRING szDevName;
    RtlInitUnicodeString(&szDevName, VBOXGUEST_DEVICE_NAME_NT);
    PDEVICE_OBJECT pDeviceObject = NULL;
    rc = IoCreateDevice(pDrvObj, sizeof(VBOXGUESTDEVEXT), &szDevName, FILE_DEVICE_UNKNOWN, 0, FALSE, &pDeviceObject);
    if (NT_SUCCESS(rc))
    {
        Log(("VBoxGuest::vbgdNt4CreateDevice: Device created\n"));

        UNICODE_STRING DosName;
        RtlInitUnicodeString(&DosName, VBOXGUEST_DEVICE_NAME_DOS);
        rc = IoCreateSymbolicLink(&DosName, &szDevName);
        if (NT_SUCCESS(rc))
        {
            Log(("VBoxGuest::vbgdNt4CreateDevice: Symlink created\n"));

            /*
             * Setup the device extension.
             */
            Log(("VBoxGuest::vbgdNt4CreateDevice: Setting up device extension ...\n"));

            PVBOXGUESTDEVEXT pDevExt = (PVBOXGUESTDEVEXT)pDeviceObject->DeviceExtension;
            RtlZeroMemory(pDevExt, sizeof(VBOXGUESTDEVEXT));

            Log(("VBoxGuest::vbgdNt4CreateDevice: Device extension created\n"));

            /* Store a reference to ourself. */
            pDevExt->win.s.pDeviceObject = pDeviceObject;

            /* Store bus and slot number we've queried before. */
            pDevExt->win.s.busNumber  = uBusNumber;
            pDevExt->win.s.slotNumber = SlotNumber.u.AsULONG;

#ifdef VBOX_WITH_GUEST_BUGCHECK_DETECTION
            rc = hlpRegisterBugCheckCallback(pDevExt);
#endif

            /* Do the actual VBox init ... */
            if (NT_SUCCESS(rc))
            {
                rc = vboxguestwinInit(pDrvObj, pDeviceObject, pRegPath);
                if (NT_SUCCESS(rc))
                {
                    Log(("VBoxGuest::vbgdNt4CreateDevice: Returning rc = 0x%x (succcess)\n", rc));
                    return rc;
                }

                /* bail out */
            }
            IoDeleteSymbolicLink(&DosName);
        }
        else
            Log(("VBoxGuest::vbgdNt4CreateDevice: IoCreateSymbolicLink failed with rc = %#x\n", rc));
        IoDeleteDevice(pDeviceObject);
    }
    else
        Log(("VBoxGuest::vbgdNt4CreateDevice: IoCreateDevice failed with rc = %#x\n", rc));
    Log(("VBoxGuest::vbgdNt4CreateDevice: Returning rc = 0x%x\n", rc));
    return rc;
}


/**
 * Helper function to handle the PCI device lookup.
 *
 * @returns NT status code.
 *
 * @param   pulBusNumber    Where to return the bus number on success.
 * @param   pSlotNumber     Where to return the slot number on success.
 */
static NTSTATUS vbgdNt4FindPciDevice(PULONG pulBusNumber, PPCI_SLOT_NUMBER pSlotNumber)
{
    Log(("VBoxGuest::vbgdNt4FindPciDevice\n"));

    PCI_SLOT_NUMBER SlotNumber;
    SlotNumber.u.AsULONG = 0;

    /* Scan each bus. */
    for (ULONG ulBusNumber = 0; ulBusNumber < PCI_MAX_BUSES; ulBusNumber++)
    {
        /* Scan each device. */
        for (ULONG deviceNumber = 0; deviceNumber < PCI_MAX_DEVICES; deviceNumber++)
        {
            SlotNumber.u.bits.DeviceNumber = deviceNumber;

            /* Scan each function (not really required...). */
            for (ULONG functionNumber = 0; functionNumber < PCI_MAX_FUNCTION; functionNumber++)
            {
                SlotNumber.u.bits.FunctionNumber = functionNumber;

                /* Have a look at what's in this slot. */
                PCI_COMMON_CONFIG PciData;
                if (!HalGetBusData(PCIConfiguration, ulBusNumber, SlotNumber.u.AsULONG, &PciData, sizeof(ULONG)))
                {
                    /* No such bus, we're done with it. */
                    deviceNumber = PCI_MAX_DEVICES;
                    break;
                }

                if (PciData.VendorID == PCI_INVALID_VENDORID)
                    /* We have to proceed to the next function. */
                    continue;

                /* Check if it's another device. */
                if (   PciData.VendorID != VMMDEV_VENDORID
                    || PciData.DeviceID != VMMDEV_DEVICEID)
                    continue;

                /* Hooray, we've found it! */
                Log(("VBoxGuest::vbgdNt4FindPciDevice: Device found!\n"));

                *pulBusNumber = ulBusNumber;
                *pSlotNumber  = SlotNumber;
                return STATUS_SUCCESS;
            }
        }
    }

    return STATUS_DEVICE_DOES_NOT_EXIST;
}

