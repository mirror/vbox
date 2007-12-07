/** @file
 *
 * VBoxGuest -- VirtualBox Win32 guest support driver PnP code
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
#include "VBoxGuestPnP.h"
#include "Helper.h"
#include <VBox/err.h>

#include <VBox/VBoxGuestLib.h>

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/


extern "C"
{
static NTSTATUS sendIrpSynchronously(PDEVICE_OBJECT pDevObj, PIRP pIrp, BOOLEAN fStrict);
static NTSTATUS pnpIrpComplete(PDEVICE_OBJECT pDevObj, PIRP pIrp, PKEVENT event);
static VOID     showDeviceResources(PCM_PARTIAL_RESOURCE_LIST pResourceList);
}

#ifdef ALLOC_PRAGMA
#pragma alloc_text (PAGE, VBoxGuestPnP)
#pragma alloc_text (PAGE, VBoxGuestPower)
#pragma alloc_text (PAGE, sendIrpSynchronously)
#pragma alloc_text (PAGE, showDeviceResources)
#endif

/* reenable logging, this was #undef'ed on iprt/log.h for RING0 */
#define LOG_ENABLED

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/

/**
 * Irp completion routine for PnP Irps we send.
 *
 * @param   pDevObj   Device object.
 * @param   pIrp      Request packet.
 * @param   event     Semaphore.
 * @return   NT status code
 */
static NTSTATUS pnpIrpComplete(PDEVICE_OBJECT pDevObj, PIRP pIrp, PKEVENT event)
{
    KeSetEvent(event, 0, FALSE);
    return STATUS_MORE_PROCESSING_REQUIRED;
}

/**
 * Helper to send a PnP IRP and wait until it's done.
 *
 * @param    pDevObj    Device object.
 * @param    pIrp       Request packet.
 * @param    fStrict    When set, returns an error if the IRP gives an error.
 * @return   NT status code
 */
static NTSTATUS sendIrpSynchronously(PDEVICE_OBJECT pDevObj, PIRP pIrp, BOOLEAN fStrict)
{
    KEVENT event;

    KeInitializeEvent(&event, SynchronizationEvent, FALSE);

    IoCopyCurrentIrpStackLocationToNext(pIrp);
    IoSetCompletionRoutine(pIrp, (PIO_COMPLETION_ROUTINE)pnpIrpComplete, &event, TRUE, TRUE, TRUE);

    NTSTATUS rc = IoCallDriver(pDevObj, pIrp);

    if (rc == STATUS_PENDING)
    {
        KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
        rc = pIrp->IoStatus.Status;
    }

    if (!fStrict
        && (rc == STATUS_NOT_SUPPORTED || rc == STATUS_INVALID_DEVICE_REQUEST))
    {
        rc = STATUS_SUCCESS;
    }

    dprintf(("VBoxGuest::sendIrpSynchronously: returning 0x%x\n", rc));

    return rc;
}


/**
 * PnP Request handler.
 *
 * @param  pDevObj    Device object.
 * @param  pIrp       Request packet.
 */
NTSTATUS VBoxGuestPnP(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    PVBOXGUESTDEVEXT    pDevExt = (PVBOXGUESTDEVEXT)pDevObj->DeviceExtension;
    PIO_STACK_LOCATION pStack  = IoGetCurrentIrpStackLocation(pIrp);
    NTSTATUS rc = STATUS_SUCCESS;

#ifdef LOG_ENABLED
    static char* fcnname[] =
    {
        "IRP_MN_START_DEVICE",
        "IRP_MN_QUERY_REMOVE_DEVICE",
        "IRP_MN_REMOVE_DEVICE",
        "IRP_MN_CANCEL_REMOVE_DEVICE",
        "IRP_MN_STOP_DEVICE",
        "IRP_MN_QUERY_STOP_DEVICE",
        "IRP_MN_CANCEL_STOP_DEVICE",
        "IRP_MN_QUERY_DEVICE_RELATIONS",
        "IRP_MN_QUERY_INTERFACE",
        "IRP_MN_QUERY_CAPABILITIES",
        "IRP_MN_QUERY_RESOURCES",
        "IRP_MN_QUERY_RESOURCE_REQUIREMENTS",
        "IRP_MN_QUERY_DEVICE_TEXT",
        "IRP_MN_FILTER_RESOURCE_REQUIREMENTS",
        "",
        "IRP_MN_READ_CONFIG",
        "IRP_MN_WRITE_CONFIG",
        "IRP_MN_EJECT",
        "IRP_MN_SET_LOCK",
        "IRP_MN_QUERY_ID",
        "IRP_MN_QUERY_PNP_DEVICE_STATE",
        "IRP_MN_QUERY_BUS_INFORMATION",
        "IRP_MN_DEVICE_USAGE_NOTIFICATION",
        "IRP_MN_SURPRISE_REMOVAL",
    };
    dprintf(("VBoxGuest::VBoxGuestPnp: MinorFunction: %s\n", pStack->MinorFunction < (sizeof(fcnname)/sizeof(fcnname[0])) ? fcnname[pStack->MinorFunction] : "unknown"));
#endif
    switch (pStack->MinorFunction)
    {
        case IRP_MN_START_DEVICE:
        {
            rc = sendIrpSynchronously(pDevExt->nextLowerDriver, pIrp, TRUE);

            if (NT_SUCCESS(rc) && NT_SUCCESS(pIrp->IoStatus.Status))
            {
                dprintf(("VBoxGuest::START_DEVICE: pStack->Parameters.StartDevice.AllocatedResources = %p\n", pStack->Parameters.StartDevice.AllocatedResources));
                
                if (!pStack->Parameters.StartDevice.AllocatedResources)
                {
                    dprintf(("VBoxGuest::START_DEVICE: no resources, pDevExt = %p, nextLowerDriver = %p!!!\n", pDevExt, pDevExt? pDevExt->nextLowerDriver: NULL));
                    rc = STATUS_UNSUCCESSFUL;
                }
                else
                {
                    showDeviceResources(&pStack->Parameters.StartDevice.AllocatedResources->List[0].PartialResourceList);

                    VBoxScanPCIResourceList(pStack->Parameters.StartDevice.AllocatedResourcesTranslated,
                                            pDevExt);

                    /** @todo cleanup and merging codepath with NT */
                    int rcVBox;
                    rcVBox = VbglInit (pDevExt->startPortAddress, pDevExt->pVMMDevMemory);
                    if (!VBOX_SUCCESS(rcVBox))
                    {
                        dprintf(("VBoxGuest::START_DEVICE: VbglInit failed. rcVBox = %d\n", rcVBox));
                        rc = STATUS_UNSUCCESSFUL;
                    }

                    if (NT_SUCCESS(rc))
                    {
                        rcVBox = VbglGRAlloc ((VMMDevRequestHeader **)&pDevExt->irqAckEvents, sizeof (VMMDevEvents), VMMDevReq_AcknowledgeEvents);
                        if (!VBOX_SUCCESS(rc))
                        {
                            dprintf(("VBoxGuest::START_DEVICE: VbglAlloc failed. rcVBox = %d\n", rcVBox));
                            rc = STATUS_UNSUCCESSFUL;
                        }
                    }

                    if (NT_SUCCESS(rc))
                    {
                        // Map physical address of VMMDev memory
                        rc = hlpVBoxMapVMMDevMemory(pDevExt);
                        if (!NT_SUCCESS(rc))
                        {
                            dprintf(("VBoxGuest::START_DEVICE: can't map physical memory, rc = %d\n", rc));
                        }
                    }

                    if (NT_SUCCESS(rc))
                    {
                        rc = hlpVBoxReportGuestInfo (pDevExt);
                        if (!NT_SUCCESS(rc))
                        {
                            dprintf(("VBoxGuest::START_DEVICE: could not report information to host, rc = %d\n", rc));
                        }
                    }

                    if (NT_SUCCESS(rc))
                    {
                        // register DPC and ISR
                        dprintf(("VBoxGuest::VBoxGuestPnp: initializing DPC...\n"));
                        IoInitializeDpcRequest(pDevExt->deviceObject, VBoxGuestDpcHandler);

                        rc = IoConnectInterrupt(&pDevExt->interruptObject,              // out: interrupt object
                                                (PKSERVICE_ROUTINE)VBoxGuestIsrHandler, // ISR
                                                pDevExt,                                // context
                                                NULL,                                   // optional spinlock
                                                pDevExt->interruptVector,               // interrupt vector
                                                (KIRQL)pDevExt->interruptLevel,         // interrupt level
                                                (KIRQL)pDevExt->interruptLevel,         // interrupt level
                                                pDevExt->interruptMode,                 // LevelSensitive or Latched
                                                TRUE,                                   // shareable interrupt
                                                pDevExt->interruptAffinity,             // CPU affinity
                                                FALSE);                                 // don't save FPU stack
                        if (!NT_SUCCESS(rc))
                        {
                            dprintf(("VBoxGuest::VBoxGuestPnp: could not connect interrupt: rc = 0x%x\n", rc));
                        }
                    }
                }
            }
            if (NT_SUCCESS(rc))
            {
                createThreads(pDevExt);

                // initialize the event notification semaphore
                KeInitializeEvent(&pDevExt->keventNotification, NotificationEvent, FALSE);

                VBoxInitMemBalloon(pDevExt);

                // ready to rumble!
                dprintf(("VBoxGuest::VBoxGuestPnp: device is ready!\n"));
                pDevExt->devState = WORKING;
            } else
            {
                dprintf(("VBoxGuest::VBoxGuestPnp: error: rc = 0x%x\n", rc));

                // need to unmap memory in case of errors
                hlpVBoxUnmapVMMDevMemory (pDevExt);
            }
            pIrp->IoStatus.Status = rc;
            pIrp->IoStatus.Information = 0;
            IoCompleteRequest(pIrp, IO_NO_INCREMENT);
            break;
        }

        case IRP_MN_QUERY_REMOVE_DEVICE:
        {
#ifdef VBOX_REBOOT_ON_UNINSTALL
            /* The device can not be removed without a reboot. */
            if (pDevExt->devState == WORKING)
            {
                pDevExt->devState = PENDINGREMOVE;
            }
            pIrp->IoStatus.Status = STATUS_UNSUCCESSFUL;
            pIrp->IoStatus.Information = 0;
            IoCompleteRequest(pIrp, IO_NO_INCREMENT);
            rc = STATUS_UNSUCCESSFUL;
            
            dprintf(("VBoxGuest::VBoxGuestPnp: refuse with rc = %p\n", pIrp->IoStatus.Status));
#else
            pIrp->IoStatus.Status = STATUS_SUCCESS;
            if (pDevExt->devState == WORKING)
            {
                pDevExt->devState = PENDINGREMOVE;
            }
            IoSkipCurrentIrpStackLocation(pIrp);
            rc = IoCallDriver(pDevExt->nextLowerDriver, pIrp);
#endif /* VBOX_REBOOT_ON_UNINSTALL */
            break;
        }

        case IRP_MN_REMOVE_DEVICE:
        {
            /* @todo merge Remove and Stop, make a helper for common actions */
            pIrp->IoStatus.Status = STATUS_SUCCESS;

            unreserveHypervisorMemory(pDevExt);

            if (pDevExt->workerThread)
            {
                dprintf(("VBoxGuest::VBoxGuestPnp: waiting for the worker thread to terminate...\n"));
                pDevExt->stopThread = TRUE;
                KeSetEvent(&pDevExt->workerThreadRequest, 0, FALSE);
                KeWaitForSingleObject(pDevExt->workerThread,
                                      Executive, KernelMode, FALSE, NULL);
                dprintf(("VBoxGuest::VBoxGuestPnp: returned from KeWaitForSingleObject for worker thread\n"));
            }

            if (pDevExt->idleThread)
            {
                dprintf(("VBoxGuest::VBoxGuestPnp: waiting for the idle thread to terminate...\n"));
                pDevExt->stopThread = TRUE;
                KeWaitForSingleObject(pDevExt->idleThread,
                                      Executive, KernelMode, FALSE, NULL);
                dprintf(("VBoxGuest::VBoxGuestPnp: returned from KeWaitForSingleObject for idle thread\n"));
            }

            VbglTerminate ();

            // according to MSDN we have to unmap previously mapped memory
            hlpVBoxUnmapVMMDevMemory (pDevExt);

            if (pDevExt->nextLowerDriver != NULL)
            {
                IoDetachDevice(pDevExt->nextLowerDriver);
            }
            UNICODE_STRING win32Name;
            RtlInitUnicodeString(&win32Name, VBOXGUEST_DEVICE_NAME_DOS);
            IoDeleteSymbolicLink(&win32Name);
            IoDeleteDevice(pDevObj);
            pDevExt->devState = REMOVED;
            IoSkipCurrentIrpStackLocation(pIrp);
            rc = IoCallDriver(pDevExt->nextLowerDriver, pIrp);
            break;
        }

        case IRP_MN_QUERY_STOP_DEVICE:
        {
#ifdef VBOX_REBOOT_ON_UNINSTALL
            dprintf(("VBoxGuest::VBoxGuestPnp: refuse\n"));
            /* The device can not be stopped without a reboot. */
            if (pDevExt->devState == WORKING)
            {
                pDevExt->devState = PENDINGSTOP;
            }
            pIrp->IoStatus.Status = STATUS_UNSUCCESSFUL;
            IoCompleteRequest(pIrp, IO_NO_INCREMENT);
            rc = STATUS_UNSUCCESSFUL;
#else
            pIrp->IoStatus.Status = STATUS_SUCCESS;
            if (pDevExt->devState == WORKING)
            {
                pDevExt->devState = PENDINGSTOP;
            }
            IoSkipCurrentIrpStackLocation(pIrp);
            rc = IoCallDriver(pDevExt->nextLowerDriver, pIrp);
#endif /* VBOX_REBOOT_ON_UNINSTALL */
            break;
        }

        case IRP_MN_STOP_DEVICE:
        {
            pIrp->IoStatus.Status = STATUS_SUCCESS;
            if (pDevExt->devState == PENDINGSTOP)
            {
                VbglTerminate ();

                // according to MSDN we have to unmap previously mapped memory
                hlpVBoxUnmapVMMDevMemory (pDevExt);

                pDevExt->devState = STOPPED;
                dprintf(("VBoxGuest::VBoxGuestPnp: device has been disabled\n"));
            } else
            {
                dprintf(("VBoxGuest::VBoxGuestPnp: devState not PENDINGSTOP but %d\n", pDevExt->devState));
            }
            IoSkipCurrentIrpStackLocation(pIrp);
            rc = IoCallDriver(pDevExt->nextLowerDriver, pIrp);
            break;
        }

        default:
        {
            IoSkipCurrentIrpStackLocation(pIrp);
            rc = IoCallDriver(pDevExt->nextLowerDriver, pIrp);
        }

    }
    return rc;
}


/**
 * Debug helper to dump a device resource list.
 *
 * @param pResourceList  list of device resources.
 */
static VOID showDeviceResources(PCM_PARTIAL_RESOURCE_LIST pResourceList)
{
#ifdef LOG_ENABLED
    PCM_PARTIAL_RESOURCE_DESCRIPTOR resource = pResourceList->PartialDescriptors;
    ULONG nres = pResourceList->Count;
    ULONG i;

    for (i = 0; i < nres; ++i, ++resource)
    {
        ULONG type = resource->Type;

        static char* name[] =
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

        dprintf(("VBoxGuest::showDeviceResources:    type %s", type < (sizeof(name)/sizeof(name[0])) ? name[type] : "unknown"));

        switch (type)
        {
            case CmResourceTypePort:
            case CmResourceTypeMemory:
                dprintf(("VBoxGuest::showDeviceResources: start %8X%8.8lX length %X\n",
                         resource->u.Port.Start.HighPart, resource->u.Port.Start.LowPart,
                         resource->u.Port.Length));
                break;

            case CmResourceTypeInterrupt:
                dprintf(("VBoxGuest::showDeviceResources:  level %X, vector %X, affinity %X\n",
                         resource->u.Interrupt.Level, resource->u.Interrupt.Vector,
                         resource->u.Interrupt.Affinity));
                break;

            case CmResourceTypeDma:
                dprintf(("VBoxGuest::showDeviceResources:  channel %d, port %X\n",
                         resource->u.Dma.Channel, resource->u.Dma.Port));
                break;

            default:
                dprintf(("\n"));
                break;
        }
    }
#endif
}

/**
 * Handle the power completion event.
 *
 * @returns NT status code
 * @param devObj  targetted device object
 * @param irp     IO request packet
 * @param context context value passed to IoSetCompletionRoutine in VBoxGuestPower
 */
NTSTATUS VBoxGuestPowerComplete(IN PDEVICE_OBJECT devObj,
                                IN PIRP irp, IN PVOID context)
{
    PIO_STACK_LOCATION irpSp;
    PVBOXGUESTDEVEXT devExt = (PVBOXGUESTDEVEXT)context;

    ASSERT(devExt);
    ASSERT(devExt->signature == DEVICE_EXTENSION_SIGNATURE);

    irpSp = IoGetCurrentIrpStackLocation(irp);
    ASSERT(irpSp->MajorFunction == IRP_MJ_POWER);

    if (NT_SUCCESS(irp->IoStatus.Status))
    {
        switch (irpSp->MinorFunction)
        {
            case IRP_MN_SET_POWER:

                switch (irpSp->Parameters.Power.Type)
                {
                    case DevicePowerState:
                        switch (irpSp->Parameters.Power.State.DeviceState)
                        {
                            case PowerDeviceD0:
                                break;
                        }
                        break;
                }
                break;
        }
    }

    return STATUS_SUCCESS;
}


/**
 * Handle the Power requests.
 *
 * @returns   NT status code
 * @param     pDevObj   device object
 * @param     pIrp      IRP
 */
NTSTATUS VBoxGuestPower(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    PIO_STACK_LOCATION pStack = IoGetCurrentIrpStackLocation(pIrp);
    PVBOXGUESTDEVEXT   pDevExt = (PVBOXGUESTDEVEXT)pDevObj->DeviceExtension;
    POWER_STATE_TYPE   powerType;
    POWER_STATE        powerState;
    POWER_ACTION       powerAction;

    dprintf(("VBoxGuest::VBoxGuestPower\n"));

    powerType   = pStack->Parameters.Power.Type;
    powerAction = pStack->Parameters.Power.ShutdownType;
    powerState  = pStack->Parameters.Power.State;

    switch (pStack->MinorFunction)
    {
        case IRP_MN_SET_POWER:
        {
            dprintf(("VBoxGuest::VBoxGuestPower: IRP_MN_SET_POWER\n"));
            switch (powerType)
            {
                case SystemPowerState:
                {
                    dprintf(("VBoxGuest::VBoxGuestPower: SystemPowerState\n"));
                    switch (powerAction)
                    {
                        case PowerActionShutdownReset:
                        {
                            dprintf(("VBoxGuest::VBoxGuestPower: power action reset!\n"));
                            /* tell the VMM that we no longer support mouse pointer integration */

                            VMMDevReqMouseStatus *req = NULL;

                            int rc = VbglGRAlloc ((VMMDevRequestHeader **)&req, sizeof (VMMDevReqMouseStatus), VMMDevReq_SetMouseStatus);

                            if (VBOX_SUCCESS(rc))
                            {
                                req->mouseFeatures = 0;
                                req->pointerXPos = 0;
                                req->pointerYPos = 0;

                                rc = VbglGRPerform (&req->header);

                                if (VBOX_FAILURE(rc) || VBOX_FAILURE(req->header.rc))
                                {
                                    dprintf(("VBoxGuest::PowerStateRequest: error communicating new power status to VMMDev."
                                             "rc = %d, VMMDev rc = %Vrc\n", rc, req->header.rc));
                                }

                                VbglGRFree (&req->header);
                            }
                            break;
                        }

                        case PowerActionShutdown:
                        case PowerActionShutdownOff:
                        {
                            dprintf(("VBoxGuest::VBoxGuestPower: power action shutdown!\n"));
                            if (powerState.SystemState >= PowerSystemShutdown)
                            {
                                dprintf(("VBoxGuest::VBoxGuestPower: Telling the VMMDev to close the VM...\n"));
                                VMMDevPowerStateRequest *req = NULL;

                                int rc = VbglGRAlloc ((VMMDevRequestHeader **)&req, sizeof (VMMDevPowerStateRequest), VMMDevReq_SetPowerStatus);

                                if (VBOX_SUCCESS(rc))
                                {
                                    req->powerState = VMMDevPowerState_PowerOff;

                                    rc = VbglGRPerform (&req->header);

                                    if (VBOX_FAILURE(rc) || VBOX_FAILURE(req->header.rc))
                                    {
                                        dprintf(("VBoxGuest::PowerStateRequest: error communicating new power status to VMMDev."
                                                 "rc = %d, VMMDev rc = %Vrc\n", rc, req->header.rc));
                                    }

                                    VbglGRFree (&req->header);
                                }
                            }
                            break;
                        }
                    }
                    break;
                }
                default:
                    break;
            }
            break;
        }
        default:
            break;
    }

    /*
     *  Whether we are completing or relaying this power IRP,
     *  we must call PoStartNextPowerIrp.
     */
    PoStartNextPowerIrp(pIrp);

    /*
     *  Send the IRP down the driver stack,
     *  using PoCallDriver (not IoCallDriver, as for non-power irps).
     */
    IoCopyCurrentIrpStackLocationToNext(pIrp);
    IoSetCompletionRoutine(pIrp,
                           VBoxGuestPowerComplete,
                           (PVOID)pDevExt,
                           TRUE,
                           TRUE,
                           TRUE);
    return PoCallDriver(pDevExt->nextLowerDriver, pIrp);
}
