/* $Id$ */
/** @file
 * VBoxGuest-win-pnp - Windows Plug'n'Play specifics.
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
RT_C_DECLS_BEGIN
static NTSTATUS vbgdNtSendIrpSynchronously(PDEVICE_OBJECT pDevObj, PIRP pIrp, BOOLEAN fStrict);
static NTSTATUS vbgdNtPnPIrpComplete(PDEVICE_OBJECT pDevObj, PIRP pIrp, PKEVENT pEvent);
static VOID     vbgdNtShowDeviceResources(PCM_PARTIAL_RESOURCE_LIST pResourceList);
RT_C_DECLS_END

#ifdef ALLOC_PRAGMA
# pragma alloc_text(PAGE, vbgdNtPnP)
# pragma alloc_text(PAGE, vbgdNtPower)
# pragma alloc_text(PAGE, vbgdNtSendIrpSynchronously)
# pragma alloc_text(PAGE, vbgdNtShowDeviceResources)
#endif


/**
 * Irp completion routine for PnP Irps we send.
 *
 * @param   pDevObj   Device object.
 * @param   pIrp      Request packet.
 * @param   pEvent    Semaphore.
 * @return   NT status code
 */
static NTSTATUS vbgdNtPnpIrpComplete(PDEVICE_OBJECT pDevObj, PIRP pIrp, PKEVENT pEvent)
{
    KeSetEvent(pEvent, 0, FALSE);
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
static NTSTATUS vbgdNtSendIrpSynchronously(PDEVICE_OBJECT pDevObj, PIRP pIrp, BOOLEAN fStrict)
{
    KEVENT Event;

    KeInitializeEvent(&Event, SynchronizationEvent, FALSE);

    IoCopyCurrentIrpStackLocationToNext(pIrp);
    IoSetCompletionRoutine(pIrp, (PIO_COMPLETION_ROUTINE)vbgdNtPnpIrpComplete,
                           &Event, TRUE, TRUE, TRUE);

    NTSTATUS rc = IoCallDriver(pDevObj, pIrp);

    if (rc == STATUS_PENDING)
    {
        KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
        rc = pIrp->IoStatus.Status;
    }

    if (!fStrict
        && (rc == STATUS_NOT_SUPPORTED || rc == STATUS_INVALID_DEVICE_REQUEST))
    {
        rc = STATUS_SUCCESS;
    }

    Log(("VBoxGuest::vbgdNtSendIrpSynchronously: Returning 0x%x\n", rc));
    return rc;
}


/**
 * PnP Request handler.
 *
 * @param  pDevObj    Device object.
 * @param  pIrp       Request packet.
 */
NTSTATUS vbgdNtPnP(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    PVBOXGUESTDEVEXTWIN pDevExt = (PVBOXGUESTDEVEXTWIN)pDevObj->DeviceExtension;
    PIO_STACK_LOCATION  pStack  = IoGetCurrentIrpStackLocation(pIrp);

#ifdef LOG_ENABLED
    static char *s_apszFnctName[] =
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
        "IRP_MN_0xE",
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
    Log(("VBoxGuest::vbgdNtGuestPnp: MinorFunction: %s\n",
         pStack->MinorFunction < RT_ELEMENTS(s_apszFnctName)) ? s_apszFnctName[pStack->MinorFunction] : "Unknown"));
#endif

    NTSTATUS rc = STATUS_SUCCESS;
    switch (pStack->MinorFunction)
    {
        case IRP_MN_START_DEVICE:
        {
            Log(("VBoxGuest::vbgdNtVBoxGuestPnP: START_DEVICE\n"));

            /* This must be handled first by the lower driver. */
            rc = vbgdNtSendIrpSynchronously(pDevExt->pNextLowerDriver, pIrp, TRUE);

            if (   NT_SUCCESS(rc)
                && NT_SUCCESS(pIrp->IoStatus.Status))
            {
                Log(("VBoxGuest::vbgdNtVBoxGuestPnP: START_DEVICE: pStack->Parameters.StartDevice.AllocatedResources = %p\n",
                     pStack->Parameters.StartDevice.AllocatedResources));

                if (!pStack->Parameters.StartDevice.AllocatedResources)
                {
                    Log(("VBoxGuest::vbgdNtVBoxGuestPnP: START_DEVICE: No resources, pDevExt = %p, nextLowerDriver = %p!\n",
                         pDevExt, pDevExt ? pDevExt->pNextLowerDriver : NULL));
                    rc = STATUS_UNSUCCESSFUL;
                }
                else
                {
                    rc = vbgdNtInit(pDevObj, pIrp);
                }
            }

            if (NT_ERROR(rc))
            {
                Log(("VBoxGuest::vbgdNtGuestPnp: START_DEVICE: Error: rc = 0x%x\n", rc));

                /* Need to unmap memory in case of errors ... */
                vbgdNtUnmapVMMDevMemory(pDevExt);
            }
            break;
        }

        case IRP_MN_CANCEL_REMOVE_DEVICE:
        {
            Log(("VBoxGuest::vbgdNtVBoxGuestPnP: CANCEL_REMOVE_DEVICE\n"));

            /* This must be handled first by the lower driver. */
            rc = vbgdNtSendIrpSynchronously(pDevExt->pNextLowerDriver, pIrp, TRUE);

            if (NT_SUCCESS(rc) && pDevExt->devState == PENDINGREMOVE)
            {
                /* Return to the state prior to receiving the IRP_MN_QUERY_REMOVE_DEVICE request. */
                pDevExt->devState = pDevExt->prevDevState;
            }

            /* Complete the IRP. */
            break;
        }

        case IRP_MN_SURPRISE_REMOVAL:
        {
            Log(("VBoxGuest::vbgdNtVBoxGuestPnP: IRP_MN_SURPRISE_REMOVAL\n"));

            VBOXGUEST_UPDATE_DEVSTATE(pDevExt, SURPRISEREMOVED);

            /* Do nothing here actually. Cleanup is done in IRP_MN_REMOVE_DEVICE.
             * This request is not expected for VBoxGuest.
             */
            LogRel(("VBoxGuest: unexpected device removal\n"));

            /* Pass to the lower driver. */
            pIrp->IoStatus.Status = STATUS_SUCCESS;

            IoSkipCurrentIrpStackLocation(pIrp);

            rc = IoCallDriver(pDevExt->pNextLowerDriver, pIrp);

            /* Do not complete the IRP. */
            return rc;
        }

        case IRP_MN_QUERY_REMOVE_DEVICE:
        {
            Log(("VBoxGuest::vbgdNtVBoxGuestPnP: QUERY_REMOVE_DEVICE\n"));

#ifdef VBOX_REBOOT_ON_UNINSTALL
            Log(("VBoxGuest::vbgdNtGuestPnp: QUERY_REMOVE_DEVICE: Device cannot be removed without a reboot.\n"));
            rc = STATUS_UNSUCCESSFUL;
#endif

            if (NT_SUCCESS(rc))
            {
                VBOXGUEST_UPDATE_DEVSTATE(pDevExt, PENDINGREMOVE);

                /* This IRP passed down to lower driver. */
                pIrp->IoStatus.Status = STATUS_SUCCESS;

                IoSkipCurrentIrpStackLocation(pIrp);

                rc = IoCallDriver(pDevExt->pNextLowerDriver, pIrp);
                Log(("VBoxGuest::vbgdNtGuestPnp: QUERY_REMOVE_DEVICE: Next lower driver replied rc = 0x%x\n", rc));

                /* we must not do anything the IRP after doing IoSkip & CallDriver
                 * since the driver below us will complete (or already have completed) the IRP.
                 * I.e. just return the status we got from IoCallDriver */
                return rc;
            }

            /* Complete the IRP on failure. */
            break;
        }

        case IRP_MN_REMOVE_DEVICE:
        {
            Log(("VBoxGuest::vbgdNtVBoxGuestPnP: REMOVE_DEVICE\n"));

            VBOXGUEST_UPDATE_DEVSTATE(pDevExt, REMOVED);

            /* Free hardware resources. */
            /* @todo this should actually free I/O ports, interrupts, etc. */
            rc = vbgdNtCleanup(pDevObj);
            Log(("VBoxGuest::vbgdNtGuestPnp: REMOVE_DEVICE: vbgdNtCleanup rc = 0x%08X\n", rc));

            /*
             * We need to send the remove down the stack before we detach,
             * but we don't need to wait for the completion of this operation
             * (and to register a completion routine).
             */
            pIrp->IoStatus.Status = STATUS_SUCCESS;

            IoSkipCurrentIrpStackLocation(pIrp);

            rc = IoCallDriver(pDevExt->pNextLowerDriver, pIrp);
            Log(("VBoxGuest::vbgdNtGuestPnp: REMOVE_DEVICE: Next lower driver replied rc = 0x%x\n", rc));

            IoDetachDevice(pDevExt->pNextLowerDriver);

            Log(("VBoxGuest::vbgdNtGuestPnp: REMOVE_DEVICE: Removing device ...\n"));

            /* Destroy device extension and clean up everything else. */
            VBoxGuestDeleteDevExt(&pDevExt->Core);

            /* Remove DOS device + symbolic link. */
            UNICODE_STRING win32Name;
            RtlInitUnicodeString(&win32Name, VBOXGUEST_DEVICE_NAME_DOS);
            IoDeleteSymbolicLink(&win32Name);

            Log(("VBoxGuest::vbgdNtGuestPnp: REMOVE_DEVICE: Deleting device ...\n"));

            /* Last action: Delete our device! pDevObj is *not* failed
             * anymore after this call! */
            IoDeleteDevice(pDevObj);

            Log(("VBoxGuest::vbgdNtGuestPnp: REMOVE_DEVICE: Device removed!\n"));

            /* Propagating rc from IoCallDriver. */
            return rc; /* Make sure that we don't do anything below here anymore! */
        }

        case IRP_MN_CANCEL_STOP_DEVICE:
        {
            Log(("VBoxGuest::vbgdNtVBoxGuestPnP: CANCEL_STOP_DEVICE\n"));

            /* This must be handled first by the lower driver. */
            rc = vbgdNtSendIrpSynchronously(pDevExt->pNextLowerDriver, pIrp, TRUE);

            if (NT_SUCCESS(rc) && pDevExt->devState == PENDINGSTOP)
            {
                /* Return to the state prior to receiving the IRP_MN_QUERY_STOP_DEVICE request. */
                pDevExt->devState = pDevExt->prevDevState;
            }

            /* Complete the IRP. */
            break;
        }

        case IRP_MN_QUERY_STOP_DEVICE:
        {
            Log(("VBoxGuest::vbgdNtVBoxGuestPnP: QUERY_STOP_DEVICE\n"));

#ifdef VBOX_REBOOT_ON_UNINSTALL
            Log(("VBoxGuest::vbgdNtGuestPnp: QUERY_STOP_DEVICE: Device cannot be stopped without a reboot!\n"));
            pIrp->IoStatus.Status = STATUS_UNSUCCESSFUL;
#endif

            if (NT_SUCCESS(rc))
            {
                VBOXGUEST_UPDATE_DEVSTATE(pDevExt, PENDINGSTOP);

                /* This IRP passed down to lower driver. */
                pIrp->IoStatus.Status = STATUS_SUCCESS;

                IoSkipCurrentIrpStackLocation(pIrp);

                rc = IoCallDriver(pDevExt->pNextLowerDriver, pIrp);
                Log(("VBoxGuest::vbgdNtGuestPnp: QUERY_STOP_DEVICE: Next lower driver replied rc = 0x%x\n", rc));

                /* we must not do anything with the IRP after doing IoSkip & CallDriver
                 * since the driver below us will complete (or already have completed) the IRP.
                 * I.e. just return the status we got from IoCallDriver */
                return rc;
            }

            /* Complete the IRP on failure. */
            break;
        }

        case IRP_MN_STOP_DEVICE:
        {
            Log(("VBoxGuest::vbgdNtVBoxGuestPnP: STOP_DEVICE\n"));

            VBOXGUEST_UPDATE_DEVSTATE(pDevExt, STOPPED);

            /* Free hardware resources. */
            /* @todo this should actually free I/O ports, interrupts, etc. */
            rc = vbgdNtCleanup(pDevObj);
            Log(("VBoxGuest::vbgdNtGuestPnp: STOP_DEVICE: cleaning up, rc = 0x%x\n", rc));

            /* Pass to the lower driver. */
            pIrp->IoStatus.Status = STATUS_SUCCESS;

            IoSkipCurrentIrpStackLocation(pIrp);

            rc = IoCallDriver(pDevExt->pNextLowerDriver, pIrp);
            Log(("VBoxGuest::vbgdNtGuestPnp: STOP_DEVICE: Next lower driver replied rc = 0x%x\n", rc));

            return rc;
        }

        default:
        {
            IoSkipCurrentIrpStackLocation(pIrp);
            rc = IoCallDriver(pDevExt->pNextLowerDriver, pIrp);
            return rc;
        }
    }

    pIrp->IoStatus.Status = rc;
    IoCompleteRequest(pIrp, IO_NO_INCREMENT);

    Log(("VBoxGuest::vbgdNtGuestPnp: Returning with rc = 0x%x\n", rc));
    return rc;
}


/**
 * Handle the power completion event.
 *
 * @returns NT status code.
 * @param pDevObj   Targetted device object.
 * @param pIrp      IO request packet.
 * @param pContext  Context value passed to IoSetCompletionRoutine in VBoxGuestPower.
 */
static NTSTATUS vbgdNtPowerComplete(IN PDEVICE_OBJECT pDevObj, IN PIRP pIrp, IN PVOID pContext)
{
#ifdef VBOX_STRICT
    PVBOXGUESTDEVEXTWIN pDevExt = (PVBOXGUESTDEVEXTWIN)pContext;
    PIO_STACK_LOCATION  pIrpSp  = IoGetCurrentIrpStackLocation(pIrp);

    Assert(pDevExt);
    Assert(pDevExt->signature == DEVICE_EXTENSION_SIGNATURE);

    if (pIrpSp)
    {
        Assert(pIrpSp->MajorFunction == IRP_MJ_POWER);
        if (NT_SUCCESS(pIrp->IoStatus.Status))
        {
            switch (pIrpSp->MinorFunction)
            {
                case IRP_MN_SET_POWER:

                    switch (pIrpSp->Parameters.Power.Type)
                    {
                        case DevicePowerState:
                            switch (pIrpSp->Parameters.Power.State.DeviceState)
                            {
                                case PowerDeviceD0:
                                    break;
                            }
                            break;
                    }
                    break;
            }
        }
    }
#endif

    return STATUS_SUCCESS;
}


/**
 * Handle the Power requests.
 *
 * @returns   NT status code
 * @param     pDevObj   device object
 * @param     pIrp      IRP
 */
NTSTATUS vbgdNtPower(PDEVICE_OBJECT pDevObj, PIRP pIrp)
{
    PIO_STACK_LOCATION  pStack   = IoGetCurrentIrpStackLocation(pIrp);
    PVBOXGUESTDEVEXTWIN pDevExt  = (PVBOXGUESTDEVEXTWIN)pDevObj->DeviceExtension;
    POWER_STATE_TYPE    enmPowerType   = pStack->Parameters.Power.Type;
    POWER_STATE         PowerState     = pStack->Parameters.Power.State;
    POWER_ACTION        enmPowerAction = pStack->Parameters.Power.ShutdownType;

    Log(("VBoxGuest::vbgdNtGuestPower\n"));

    switch (pStack->MinorFunction)
    {
        case IRP_MN_SET_POWER:
        {
            Log(("VBoxGuest::vbgdNtGuestPower: IRP_MN_SET_POWER, type= %d\n", enmPowerType));
            switch (enmPowerType)
            {
                case SystemPowerState:
                {
                    Log(("VBoxGuest::vbgdNtGuestPower: SystemPowerState, action = %d, state = %d/%d\n",
                         enmPowerAction, PowerState.SystemState, PowerState.DeviceState));

                    switch (enmPowerAction)
                    {
                        case PowerActionSleep:

                            /* System now is in a working state. */
                            if (PowerState.SystemState == PowerSystemWorking)
                            {
                                if (   pDevExt
                                    && pDevExt->LastSystemPowerAction == PowerActionHibernate)
                                {
                                    Log(("VBoxGuest::vbgdNtGuestPower: Returning from hibernation!\n"));
                                    int rc = VBoxGuestReinitDevExtAfterHibernation(&pDevExt->Core,
                                                                                   vbgdNtVersionToOSType(g_enmVbgdNtVer));
                                    if (RT_FAILURE(rc))
                                        Log(("VBoxGuest::vbgdNtGuestPower: Cannot re-init VMMDev chain, rc = %d!\n", rc));
                                }
                            }
                            break;

                        case PowerActionShutdownReset:
                        {
                            Log(("VBoxGuest::vbgdNtGuestPower: Power action reset!\n"));

                            /* Tell the VMM that we no longer support mouse pointer integration. */
                            VMMDevReqMouseStatus *pReq = NULL;
                            int vrc = VbglGRAlloc((VMMDevRequestHeader **)&pReq, sizeof (VMMDevReqMouseStatus),
                                                  VMMDevReq_SetMouseStatus);
                            if (RT_SUCCESS(vrc))
                            {
                                pReq->mouseFeatures = 0;
                                pReq->pointerXPos = 0;
                                pReq->pointerYPos = 0;

                                vrc = VbglGRPerform(&pReq->header);
                                if (RT_FAILURE(vrc))
                                {
                                    Log(("VBoxGuest::PowerStateRequest: error communicating new power status to VMMDev. vrc = %Rrc\n", vrc));
                                }

                                VbglGRFree(&pReq->header);
                            }

                            /* Don't do any cleanup here; there might be still coming in some IOCtls after we got this
                             * power action and would assert/crash when we already cleaned up all the stuff! */
                            break;
                        }

                        case PowerActionShutdown:
                        case PowerActionShutdownOff:
                        {
                            Log(("VBoxGuest::vbgdNtGuestPower: Power action shutdown!\n"));
                            if (PowerState.SystemState >= PowerSystemShutdown)
                            {
                                Log(("VBoxGuest::vbgdNtGuestPower: Telling the VMMDev to close the VM ...\n"));

                                VMMDevPowerStateRequest *pReq = pDevExt->pPowerStateRequest;
                                int vrc = VERR_NOT_IMPLEMENTED;
                                if (pReq)
                                {
                                    pReq->header.requestType = VMMDevReq_SetPowerStatus;
                                    pReq->powerState = VMMDevPowerState_PowerOff;

                                    vrc = VbglGRPerform(&pReq->header);
                                }
                                if (RT_FAILURE(vrc))
                                    Log(("VBoxGuest::PowerStateRequest: Error communicating new power status to VMMDev. vrc = %Rrc\n", vrc));

                                /* No need to do cleanup here; at this point we should've been
                                 * turned off by VMMDev already! */
                            }
                            break;
                        }

                        case PowerActionHibernate:

                            Log(("VBoxGuest::vbgdNtGuestPower: Power action hibernate!\n"));
                            break;
                    }

                    /*
                     * Save the current system power action for later use.
                     * This becomes handy when we return from hibernation for example.
                     */
                    if (pDevExt)
                        pDevExt->LastSystemPowerAction = enmPowerAction;

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
     * Whether we are completing or relaying this power IRP,
     * we must call PoStartNextPowerIrp.
     */
    PoStartNextPowerIrp(pIrp);

    /*
     * Send the IRP down the driver stack, using PoCallDriver
     * (not IoCallDriver, as for non-power irps).
     */
    IoCopyCurrentIrpStackLocationToNext(pIrp);
    IoSetCompletionRoutine(pIrp,
                           vbgdNtPowerComplete,
                           (PVOID)pDevExt,
                           TRUE,
                           TRUE,
                           TRUE);
    return PoCallDriver(pDevExt->pNextLowerDriver, pIrp);
}

