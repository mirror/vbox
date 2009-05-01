/* $Id$ */
/** @file
 * VMMDev - Guest <-> VMM/Host communication device.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
/* #define LOG_ENABLED */
/* Enable dev_vmm Log3 statements to get IRQ-related logging. */

#define LOG_GROUP LOG_GROUP_DEV_VMM
#include <VBox/VBoxDev.h>
#include <VBox/VBoxGuest.h>
#include <VBox/log.h>
#include <VBox/param.h>
#include <VBox/mm.h>
#include <VBox/pgm.h>
#include <VBox/err.h>
#include <VBox/vm.h> /* for VM_IS_EMT */

#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/time.h>
#ifndef IN_RC
# include <iprt/mem.h>
#endif

#include "VMMDevState.h"
#ifdef VBOX_WITH_HGCM
# include "VMMDevHGCM.h"
#endif


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#define PCIDEV_2_VMMDEVSTATE(pPciDev)              ( (VMMDevState *)(pPciDev) )
#define VMMDEVSTATE_2_DEVINS(pVMMDevState)         ( (pVMMDevState)->pDevIns )

#define VBOX_GUEST_ADDITIONS_VERSION_1_03(s)            \
    ((RT_HIWORD ((s)->guestInfo.additionsVersion) == 1) && \
     (RT_LOWORD ((s)->guestInfo.additionsVersion) == 3))

#define VBOX_GUEST_ADDITIONS_VERSION_OK(additionsVersion) \
      (RT_HIWORD(additionsVersion) == RT_HIWORD(VMMDEV_VERSION) \
    && RT_LOWORD(additionsVersion) <= RT_LOWORD(VMMDEV_VERSION))

#define VBOX_GUEST_ADDITIONS_VERSION_OLD(additionsVersion) \
      ((RT_HIWORD(additionsVersion) < RT_HIWORD(VMMDEV_VERSION) \
      || ((RT_HIWORD(additionsVersion) == RT_HIWORD(VMMDEV_VERSION) \
          && RT_LOWORD(additionsVersion) <= RT_LOWORD(VMMDEV_VERSION))

#define VBOX_GUEST_ADDITIONS_VERSION_TOO_OLD(additionsVersion) \
      (RT_HIWORD(additionsVersion) < RT_HIWORD(VMMDEV_VERSION))

#define VBOX_GUEST_ADDITIONS_VERSION_NEW(additionsVersion) \
      ((RT_HIWORD(additionsVersion) > RT_HIWORD(VMMDEV_VERSION) \
      || ((RT_HIWORD(additionsVersion) == RT_HIWORD(VMMDEV_VERSION) \
          && RT_LOWORD(additionsVersion) > RT_LOWORD(VMMDEV_VERSION))

#ifndef VBOX_DEVICE_STRUCT_TESTCASE

/* Whenever host wants to inform guest about something
 * an IRQ notification will be raised.
 *
 * VMMDev PDM interface will contain the guest notification method.
 *
 * There is a 32 bit event mask which will be read
 * by guest on an interrupt. A non zero bit in the mask
 * means that the specific event occured and requires
 * processing on guest side.
 *
 * After reading the event mask guest must issue a
 * generic request AcknowlegdeEvents.
 *
 * IRQ line is set to 1 (request) if there are unprocessed
 * events, that is the event mask is not zero.
 *
 * After receiving an interrupt and checking event mask,
 * the guest must process events using the event specific
 * mechanism.
 *
 * That is if mouse capabilities were changed,
 * guest will use VMMDev_GetMouseStatus generic request.
 *
 * Event mask is only a set of flags indicating that guest
 * must proceed with a procedure.
 *
 * Unsupported events are therefore ignored.
 * The guest additions must inform host which events they
 * want to receive, to avoid unnecessary IRQ processing.
 * By default no events are signalled to guest.
 *
 * This seems to be fast method. It requires
 * only one context switch for an event processing.
 *
 */

static void vmmdevSetIRQ_Legacy_EMT (VMMDevState *pVMMDevState)
{
    if (!pVMMDevState->fu32AdditionsOk)
    {
        Log(("vmmdevSetIRQ: IRQ is not generated, guest has not yet reported to us.\n"));
        return;
    }

    uint32_t u32IRQLevel = 0;

    /* Filter unsupported events */
    uint32_t u32EventFlags =
        pVMMDevState->u32HostEventFlags
        & pVMMDevState->pVMMDevRAMR3->V.V1_03.u32GuestEventMask;

    Log(("vmmdevSetIRQ: u32EventFlags = 0x%08X, "
         "pVMMDevState->u32HostEventFlags = 0x%08X, "
         "pVMMDevState->pVMMDevRAMR3->u32GuestEventMask = 0x%08X\n",
         u32EventFlags,
         pVMMDevState->u32HostEventFlags,
         pVMMDevState->pVMMDevRAMR3->V.V1_03.u32GuestEventMask));

    /* Move event flags to VMMDev RAM */
    pVMMDevState->pVMMDevRAMR3->V.V1_03.u32HostEvents = u32EventFlags;

    if (u32EventFlags)
    {
        /* Clear host flags which will be delivered to guest. */
        pVMMDevState->u32HostEventFlags &= ~u32EventFlags;
        Log(("vmmdevSetIRQ: pVMMDevState->u32HostEventFlags = 0x%08X\n",
             pVMMDevState->u32HostEventFlags));
        u32IRQLevel = 1;
    }

    /* Set IRQ level for pin 0 */
    /** @todo make IRQ pin configurable, at least a symbolic constant */
    PPDMDEVINS pDevIns = VMMDEVSTATE_2_DEVINS(pVMMDevState);
    PDMDevHlpPCISetIrqNoWait(pDevIns, 0, u32IRQLevel);
    Log(("vmmdevSetIRQ: IRQ set %d\n", u32IRQLevel));
}

static void vmmdevMaybeSetIRQ_EMT (VMMDevState *pVMMDevState)
{
    PPDMDEVINS pDevIns = VMMDEVSTATE_2_DEVINS (pVMMDevState);

    Log3(("vmmdevMaybeSetIRQ_EMT: u32HostEventFlags = 0x%08X, u32GuestFilterMask = 0x%08X.\n",
          pVMMDevState->u32HostEventFlags, pVMMDevState->u32GuestFilterMask));

    if (pVMMDevState->u32HostEventFlags & pVMMDevState->u32GuestFilterMask)
    {
        pVMMDevState->pVMMDevRAMR3->V.V1_04.fHaveEvents = true;
        PDMDevHlpPCISetIrqNoWait (pDevIns, 0, 1);
        Log3(("vmmdevMaybeSetIRQ_EMT: IRQ set.\n"));
    }
}

static void vmmdevNotifyGuest_EMT (VMMDevState *pVMMDevState, uint32_t u32EventMask)
{
    Log3(("VMMDevNotifyGuest_EMT: u32EventMask = 0x%08X.\n", u32EventMask));

    if (VBOX_GUEST_ADDITIONS_VERSION_1_03 (pVMMDevState))
    {
        Log3(("VMMDevNotifyGuest_EMT: Old additions detected.\n"));

        pVMMDevState->u32HostEventFlags |= u32EventMask;
        vmmdevSetIRQ_Legacy_EMT (pVMMDevState);
    }
    else
    {
        Log3(("VMMDevNotifyGuest_EMT: New additions detected.\n"));

        if (!pVMMDevState->fu32AdditionsOk)
        {
            pVMMDevState->u32HostEventFlags |= u32EventMask;
            Log(("vmmdevNotifyGuest_EMT: IRQ is not generated, guest has not yet reported to us.\n"));
            return;
        }

        const bool fHadEvents =
            (pVMMDevState->u32HostEventFlags & pVMMDevState->u32GuestFilterMask) != 0;

        Log3(("VMMDevNotifyGuest_EMT: fHadEvents = %d, u32HostEventFlags = 0x%08X, u32GuestFilterMask = 0x%08X.\n",
              fHadEvents, pVMMDevState->u32HostEventFlags, pVMMDevState->u32GuestFilterMask));

        pVMMDevState->u32HostEventFlags |= u32EventMask;

        if (!fHadEvents)
            vmmdevMaybeSetIRQ_EMT (pVMMDevState);
    }
}

static void vmmdevCtlGuestFilterMask_EMT (VMMDevState *pVMMDevState,
                                          uint32_t u32OrMask,
                                          uint32_t u32NotMask)
{
    const bool fHadEvents =
        (pVMMDevState->u32HostEventFlags & pVMMDevState->u32GuestFilterMask) != 0;

    Log(("vmmdevCtlGuestFilterMask_EMT: u32OrMask = 0x%08X, u32NotMask = 0x%08X, fHadEvents = %d.\n", u32OrMask, u32NotMask, fHadEvents));
    if (fHadEvents)
    {
        if (!pVMMDevState->fNewGuestFilterMask)
            pVMMDevState->u32NewGuestFilterMask = pVMMDevState->u32GuestFilterMask;

        pVMMDevState->u32NewGuestFilterMask |= u32OrMask;
        pVMMDevState->u32NewGuestFilterMask &= ~u32NotMask;
        pVMMDevState->fNewGuestFilterMask = true;
    }
    else
    {
        pVMMDevState->u32GuestFilterMask |= u32OrMask;
        pVMMDevState->u32GuestFilterMask &= ~u32NotMask;
        vmmdevMaybeSetIRQ_EMT (pVMMDevState);
    }
}

void VMMDevCtlSetGuestFilterMask (VMMDevState *pVMMDevState,
                                  uint32_t u32OrMask,
                                  uint32_t u32NotMask)
{
    PPDMDEVINS pDevIns = VMMDEVSTATE_2_DEVINS(pVMMDevState);
    PVM pVM = PDMDevHlpGetVM(pDevIns);

    Log(("VMMDevCtlSetGuestFilterMask: u32OrMask = 0x%08X, u32NotMask = 0x%08X.\n", u32OrMask, u32NotMask));

    if (VM_IS_EMT(pVM))
    {
        vmmdevCtlGuestFilterMask_EMT (pVMMDevState, u32OrMask, u32NotMask);
    }
    else
    {
        int rc;
        PVMREQ pReq;

        rc = VMR3ReqCallVoid (pVM, VMCPUID_ANY, &pReq, RT_INDEFINITE_WAIT,
                              (PFNRT) vmmdevCtlGuestFilterMask_EMT,
                              3, pVMMDevState, u32OrMask, u32NotMask);
        AssertReleaseRC (rc);
        VMR3ReqFree (pReq);
    }
}

void VMMDevNotifyGuest (VMMDevState *pVMMDevState, uint32_t u32EventMask)
{
    PPDMDEVINS pDevIns = VMMDEVSTATE_2_DEVINS(pVMMDevState);
    PVM pVM = PDMDevHlpGetVM(pDevIns);
    int rc;

    Log3(("VMMDevNotifyGuest: u32EventMask = 0x%08X.\n", u32EventMask));

    /*
     * Drop notifications if the VM is not running yet/anymore.
     */
    if (PDMDevHlpVMState(pDevIns) != VMSTATE_RUNNING)
        return;

    /* No need to wait for the completion of this request. It is a notification
     * about something, which has already happened.
     */
    rc = VMR3ReqCallEx(pVM, VMCPUID_ANY, NULL, 0, VMREQFLAGS_NO_WAIT | VMREQFLAGS_VOID,
                       (PFNRT) vmmdevNotifyGuest_EMT,
                       2, pVMMDevState, u32EventMask);
    AssertRC(rc);
}

/**
 * Port I/O Handler for OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   uPort       Port number used for the IN operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
static DECLCALLBACK(int) vmmdevBackdoorLog(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    VMMDevState *pThis = PDMINS_2_DATA(pDevIns, VMMDevState *);

    if (!pThis->fBackdoorLogDisabled && cb == 1 && Port == RTLOG_DEBUG_PORT)
    {

        /* The raw version. */
        switch (u32)
        {
            case '\r': LogIt(LOG_INSTANCE, RTLOGGRPFLAGS_LEVEL_2, LOG_GROUP_DEV_VMM_BACKDOOR, ("vmmdev: <return>\n")); break;
            case '\n': LogIt(LOG_INSTANCE, RTLOGGRPFLAGS_LEVEL_2, LOG_GROUP_DEV_VMM_BACKDOOR, ("vmmdev: <newline>\n")); break;
            case '\t': LogIt(LOG_INSTANCE, RTLOGGRPFLAGS_LEVEL_2, LOG_GROUP_DEV_VMM_BACKDOOR, ("vmmdev: <tab>\n")); break;
            default:   LogIt(LOG_INSTANCE, RTLOGGRPFLAGS_LEVEL_2, LOG_GROUP_DEV_VMM_BACKDOOR, ("vmmdev: %c (%02x)\n", u32, u32)); break;
        }

        /* The readable, buffered version. */
        if (u32 == '\n' || u32 == '\r')
        {
            pThis->szMsg[pThis->iMsg] = '\0';
            if (pThis->iMsg)
                LogRelIt(LOG_REL_INSTANCE, RTLOGGRPFLAGS_LEVEL_1, LOG_GROUP_DEV_VMM_BACKDOOR, ("Guest Log: %s\n", pThis->szMsg));
            pThis->iMsg = 0;
        }
        else
        {
            if (pThis->iMsg >= sizeof(pThis->szMsg)-1)
            {
                pThis->szMsg[pThis->iMsg] = '\0';
                LogRelIt(LOG_REL_INSTANCE, RTLOGGRPFLAGS_LEVEL_1, LOG_GROUP_DEV_VMM_BACKDOOR, ("Guest Log: %s\n", pThis->szMsg));
                pThis->iMsg = 0;
            }
            pThis->szMsg[pThis->iMsg] = (char )u32;
            pThis->szMsg[++pThis->iMsg] = '\0';
        }
    }
    return VINF_SUCCESS;
}

#ifdef TIMESYNC_BACKDOOR
/**
 * Port I/O Handler for OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   uPort       Port number used for the IN operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
static DECLCALLBACK(int) vmmdevTimesyncBackdoorWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    NOREF(pvUser);
    if (cb == 4)
    {
        VMMDevState *pThis = PDMINS_2_DATA(pDevIns, VMMDevState *);
        switch (u32)
        {
            case 0:
                pThis->fTimesyncBackdoorLo = false;
                break;
            case 1:
                pThis->fTimesyncBackdoorLo = true;
        }
        return VINF_SUCCESS;

    }
    return VINF_SUCCESS;
}

/**
 * Port I/O Handler for backdoor timesync IN operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   uPort       Port number used for the IN operation.
 * @param   pu32        Where to store the result.
 * @param   cb          Number of bytes read.
 */
static DECLCALLBACK(int) vmmdevTimesyncBackdoorRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    int rc;
    NOREF(pvUser);
    if (cb == 4)
    {
        VMMDevState *pThis = PDMINS_2_DATA(pDevIns, VMMDevState *);
        RTTIMESPEC now;

        if (pThis->fTimesyncBackdoorLo)
            *pu32 = (uint32_t)pThis->hostTime;
        else
        {
            pThis->hostTime = RTTimeSpecGetMilli(PDMDevHlpUTCNow(pDevIns, &now));
            *pu32 = (uint32_t)(pThis->hostTime >> 32);
        }
        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_IOM_IOPORT_UNUSED;
    return rc;
}
#endif /* TIMESYNC_BACKDOOR */

/**
 * Port I/O Handler for the generic request interface
 * @see FNIOMIOPORTOUT for details.
 */
static DECLCALLBACK(int) vmmdevRequestHandler(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    VMMDevState *pThis = (VMMDevState*)pvUser;
    int rcRet = VINF_SUCCESS;

    /*
     * The caller has passed the guest context physical address
     * of the request structure. Copy the request packet.
     */
    VMMDevRequestHeader requestHeader = {0};
    VMMDevRequestHeader *pRequestHeader = NULL;

    PDMDevHlpPhysRead(pDevIns, (RTGCPHYS)u32, &requestHeader, sizeof(requestHeader));

    /* the structure size must be greater or equal to the header size */
    if (requestHeader.size < sizeof(VMMDevRequestHeader))
    {
        Log(("VMMDev request header size too small! size = %d\n", requestHeader.size));
        rcRet = VINF_SUCCESS;
        goto end;
    }

    /* check the version of the header structure */
    if (requestHeader.version != VMMDEV_REQUEST_HEADER_VERSION)
    {
        Log(("VMMDev: guest header version (0x%08X) differs from ours (0x%08X)\n", requestHeader.version, VMMDEV_REQUEST_HEADER_VERSION));
        rcRet = VINF_SUCCESS;
        goto end;
    }

    Log2(("VMMDev request issued: %d\n", requestHeader.requestType));

    if (    requestHeader.requestType != VMMDevReq_ReportGuestInfo
        && !pThis->fu32AdditionsOk)
    {
        Log(("VMMDev: guest has not yet reported to us. Refusing operation.\n"));
        requestHeader.rc = VERR_NOT_SUPPORTED;
        rcRet = VINF_SUCCESS;
        goto end;
    }

    /* Check upper limit */
    if (requestHeader.size > VMMDEV_MAX_VMMDEVREQ_SIZE)
    {
        LogRel(("VMMDev: request packet too big (%x). Refusing operation.\n", requestHeader.size));
        requestHeader.rc = VERR_NOT_SUPPORTED;
        rcRet = VINF_SUCCESS;
        goto end;
    }

    /* Read the entire request packet */
    pRequestHeader = (VMMDevRequestHeader *)RTMemAlloc(requestHeader.size);
    if (!pRequestHeader)
    {
        Log(("VMMDev: RTMemAlloc failed!\n"));
        rcRet = VINF_SUCCESS;
        requestHeader.rc = VERR_NO_MEMORY;
        goto end;
    }
    PDMDevHlpPhysRead(pDevIns, (RTGCPHYS)u32, pRequestHeader, requestHeader.size);

    /* which request was sent? */
    switch (pRequestHeader->requestType)
    {
        /*
         * Guest wants to give up a timeslice
         */
        case VMMDevReq_Idle:
        {
            /* just return to EMT telling it that we want to halt */
            rcRet = VINF_EM_HALT;
            break;
        }

        /*
         * Guest is reporting its information
         */
        case VMMDevReq_ReportGuestInfo:
        {
            if (pRequestHeader->size < sizeof(VMMDevReportGuestInfo))
            {
                AssertMsgFailed(("VMMDev guest information structure has invalid size!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevReportGuestInfo *guestInfo = (VMMDevReportGuestInfo*)pRequestHeader;

                if (memcmp (&pThis->guestInfo, &guestInfo->guestInfo, sizeof (guestInfo->guestInfo)) != 0)
                {
                    /* make a copy of supplied information */
                    pThis->guestInfo = guestInfo->guestInfo;

                    /* Check additions version */
                    pThis->fu32AdditionsOk = VBOX_GUEST_ADDITIONS_VERSION_OK(pThis->guestInfo.additionsVersion);

                    LogRel(("Guest Additions information report: additionsVersion = 0x%08X  osType = 0x%08X\n",
                            pThis->guestInfo.additionsVersion,
                            pThis->guestInfo.osType));
                    pThis->pDrv->pfnUpdateGuestVersion(pThis->pDrv, &pThis->guestInfo);
                }

                if (pThis->fu32AdditionsOk)
                {
                    pRequestHeader->rc = VINF_SUCCESS;
                }
                else
                {
                    pRequestHeader->rc = VERR_VERSION_MISMATCH;
                }
            }
            break;
        }

        /* Report guest capabilities */
        case VMMDevReq_ReportGuestCapabilities:
        {
            if (pRequestHeader->size != sizeof(VMMDevReqGuestCapabilities))
            {
                AssertMsgFailed(("VMMDev guest caps structure has invalid size!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevReqGuestCapabilities *guestCaps = (VMMDevReqGuestCapabilities*)pRequestHeader;

                /* Enable this automatically for guests using the old
                   request to report their capabilities. */
                /** @todo change this when we next bump the interface version */
                guestCaps->caps |= VMMDEV_GUEST_SUPPORTS_GRAPHICS;
                if (pThis->guestCaps != guestCaps->caps)
                {
                    /* make a copy of supplied information */
                    pThis->guestCaps = guestCaps->caps;

                    LogRel(("Guest Additions capability report: (0x%x) "
                            "seamless: %s, "
                            "hostWindowMapping: %s, "
                            "graphics: %s\n",
                            guestCaps->caps,
                            guestCaps->caps & VMMDEV_GUEST_SUPPORTS_SEAMLESS ? "yes" : "no",
                            guestCaps->caps & VMMDEV_GUEST_SUPPORTS_GUEST_HOST_WINDOW_MAPPING ? "yes" : "no",
                            guestCaps->caps & VMMDEV_GUEST_SUPPORTS_GRAPHICS ? "yes" : "no"));

                    pThis->pDrv->pfnUpdateGuestCapabilities(pThis->pDrv, guestCaps->caps);
                }
                pRequestHeader->rc = VINF_SUCCESS;
            }
            break;
        }

        /* Change guest capabilities */
        case VMMDevReq_SetGuestCapabilities:
        {
            if (pRequestHeader->size != sizeof(VMMDevReqGuestCapabilities2))
            {
                AssertMsgFailed(("VMMDev guest caps structure has invalid size!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevReqGuestCapabilities2 *guestCaps = (VMMDevReqGuestCapabilities2*)pRequestHeader;

                pThis->guestCaps |= guestCaps->u32OrMask;
                pThis->guestCaps &= ~guestCaps->u32NotMask;

                LogRel(("Guest Additions capability report: (0x%x) "
                        "seamless: %s, "
                        "hostWindowMapping: %s, "
                        "graphics: %s\n",
                        pThis->guestCaps,
                        pThis->guestCaps & VMMDEV_GUEST_SUPPORTS_SEAMLESS ? "yes" : "no",
                        pThis->guestCaps & VMMDEV_GUEST_SUPPORTS_GUEST_HOST_WINDOW_MAPPING ? "yes" : "no",
                        pThis->guestCaps & VMMDEV_GUEST_SUPPORTS_GRAPHICS ? "yes" : "no"));

                pThis->pDrv->pfnUpdateGuestCapabilities(pThis->pDrv, pThis->guestCaps);
                pRequestHeader->rc = VINF_SUCCESS;
            }
            break;
        }

        /*
         * Retrieve mouse information
         */
        case VMMDevReq_GetMouseStatus:
        {
            if (pRequestHeader->size != sizeof(VMMDevReqMouseStatus))
            {
                AssertMsgFailed(("VMMDev mouse status structure has invalid size!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevReqMouseStatus *mouseStatus = (VMMDevReqMouseStatus*)pRequestHeader;
                mouseStatus->mouseFeatures = 0;
                if (pThis->mouseCapabilities & VMMDEV_MOUSEHOSTWANTSABS)
                {
                    mouseStatus->mouseFeatures |= VBOXGUEST_MOUSE_HOST_CAN_ABSOLUTE;
                }
                if (pThis->mouseCapabilities & VMMDEV_MOUSEGUESTWANTSABS)
                {
                    mouseStatus->mouseFeatures |= VBOXGUEST_MOUSE_GUEST_CAN_ABSOLUTE;
                }
                if (pThis->mouseCapabilities & VMMDEV_MOUSEHOSTCANNOTHWPOINTER)
                {
                    mouseStatus->mouseFeatures |= VBOXGUEST_MOUSE_HOST_CANNOT_HWPOINTER;
                }
                mouseStatus->pointerXPos = pThis->mouseXAbs;
                mouseStatus->pointerYPos = pThis->mouseYAbs;
                Log2(("returning mouse status: features = %d, absX = %d, absY = %d\n", mouseStatus->mouseFeatures,
                      mouseStatus->pointerXPos, mouseStatus->pointerYPos));
                pRequestHeader->rc = VINF_SUCCESS;
            }
            break;
        }

        /*
         * Set mouse information
         */
        case VMMDevReq_SetMouseStatus:
        {
            if (pRequestHeader->size != sizeof(VMMDevReqMouseStatus))
            {
                AssertMsgFailed(("VMMDev mouse status structure has invalid size %d (%#x) version=%d!\n",
                                 pRequestHeader->size, pRequestHeader->size, pRequestHeader->size, pRequestHeader->version));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                bool bCapsChanged = false;

                VMMDevReqMouseStatus *mouseStatus = (VMMDevReqMouseStatus*)pRequestHeader;

                /* check if the guest wants absolute coordinates */
                if (mouseStatus->mouseFeatures & VBOXGUEST_MOUSE_GUEST_CAN_ABSOLUTE)
                {
                    /* set the capability flag and the changed flag if it's actually a change */
                    if (!(pThis->mouseCapabilities & VMMDEV_MOUSEGUESTWANTSABS))
                    {
                        pThis->mouseCapabilities |= VMMDEV_MOUSEGUESTWANTSABS;
                        bCapsChanged = true;
                        LogRel(("Guest requests mouse pointer integration\n"));
                    }
                } else
                {
                    if (pThis->mouseCapabilities & VMMDEV_MOUSEGUESTWANTSABS)
                    {
                        pThis->mouseCapabilities &= ~VMMDEV_MOUSEGUESTWANTSABS;
                        bCapsChanged = true;
                        LogRel(("Guest disables mouse pointer integration\n"));
                    }
                }
                if (mouseStatus->mouseFeatures & VBOXGUEST_MOUSE_GUEST_NEEDS_HOST_CURSOR)
                    pThis->mouseCapabilities |= VMMDEV_MOUSEGUESTNEEDSHOSTCUR;
                else
                    pThis->mouseCapabilities &= ~VMMDEV_MOUSEGUESTNEEDSHOSTCUR;

                /*
                 * Notify connector if something has changed
                 */
                if (bCapsChanged)
                {
                    Log(("VMMDevReq_SetMouseStatus: capabilities changed (%x), informing connector\n", pThis->mouseCapabilities));
                    pThis->pDrv->pfnUpdateMouseCapabilities(pThis->pDrv, pThis->mouseCapabilities);
                }
                pRequestHeader->rc = VINF_SUCCESS;
            }

            break;
        }

        /*
         * Set a new mouse pointer shape
         */
        case VMMDevReq_SetPointerShape:
        {
            if (pRequestHeader->size < sizeof(VMMDevReqMousePointer))
            {
                AssertMsg(pRequestHeader->size == 0x10028 && pRequestHeader->version == 10000,  /* don't bitch about legacy!!! */
                          ("VMMDev mouse shape structure has invalid size %d (%#x) version=%d!\n",
                           pRequestHeader->size, pRequestHeader->size, pRequestHeader->size, pRequestHeader->version));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevReqMousePointer *pointerShape = (VMMDevReqMousePointer*)pRequestHeader;

                bool fVisible = (pointerShape->fFlags & VBOX_MOUSE_POINTER_VISIBLE) != 0;
                bool fAlpha = (pointerShape->fFlags & VBOX_MOUSE_POINTER_ALPHA) != 0;
                bool fShape = (pointerShape->fFlags & VBOX_MOUSE_POINTER_SHAPE) != 0;

                Log(("VMMDevReq_SetPointerShape: visible: %d, alpha: %d, shape = %d, width: %d, height: %d\n",
                     fVisible, fAlpha, fShape, pointerShape->width, pointerShape->height));

                if (pRequestHeader->size == sizeof(VMMDevReqMousePointer))
                {
                    /* The guest did not provide the shape actually. */
                    fShape = false;
                }

                /* forward call to driver */
                if (fShape)
                {
                    pThis->pDrv->pfnUpdatePointerShape(pThis->pDrv,
                                                       fVisible,
                                                       fAlpha,
                                                       pointerShape->xHot, pointerShape->yHot,
                                                       pointerShape->width, pointerShape->height,
                                                       pointerShape->pointerData);
                }
                else
                {
                    pThis->pDrv->pfnUpdatePointerShape(pThis->pDrv,
                                                       fVisible,
                                                       0,
                                                       0, 0,
                                                       0, 0,
                                                       NULL);
                }
                pRequestHeader->rc = VINF_SUCCESS;
            }
            break;
        }

        /*
         * Query the system time from the host
         */
        case VMMDevReq_GetHostTime:
        {
            if (pRequestHeader->size != sizeof(VMMDevReqHostTime))
            {
                AssertMsgFailed(("VMMDev host time structure has invalid size!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else if (RT_UNLIKELY(pThis->fGetHostTimeDisabled))
                pRequestHeader->rc = VERR_NOT_SUPPORTED;
            else
            {
                VMMDevReqHostTime *hostTimeReq = (VMMDevReqHostTime*)pRequestHeader;
                RTTIMESPEC now;
                hostTimeReq->time = RTTimeSpecGetMilli(PDMDevHlpUTCNow(pDevIns, &now));
                pRequestHeader->rc = VINF_SUCCESS;
            }
            break;
        }

        /*
         * Query information about the hypervisor
         */
        case VMMDevReq_GetHypervisorInfo:
        {
            if (pRequestHeader->size != sizeof(VMMDevReqHypervisorInfo))
            {
                AssertMsgFailed(("VMMDev hypervisor info structure has invalid size!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevReqHypervisorInfo *hypervisorInfo = (VMMDevReqHypervisorInfo*)pRequestHeader;
                PVM pVM = PDMDevHlpGetVM(pDevIns);
                pRequestHeader->rc = PGMR3MappingsSize(pVM, &hypervisorInfo->hypervisorSize);
            }
            break;
        }

        /*
         * Set hypervisor information
         */
        case VMMDevReq_SetHypervisorInfo:
        {
            if (pRequestHeader->size != sizeof(VMMDevReqHypervisorInfo))
            {
                AssertMsgFailed(("VMMDev hypervisor info structure has invalid size!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevReqHypervisorInfo *hypervisorInfo = (VMMDevReqHypervisorInfo*)pRequestHeader;
                PVM pVM = PDMDevHlpGetVM(pDevIns);
                if (hypervisorInfo->hypervisorStart == 0)
                    pRequestHeader->rc = PGMR3MappingsUnfix(pVM);
                else
                {
                    /* only if the client has queried the size before! */
                    uint32_t mappingsSize;
                    pRequestHeader->rc = PGMR3MappingsSize(pVM, &mappingsSize);
                    if (RT_SUCCESS(pRequestHeader->rc) && hypervisorInfo->hypervisorSize == mappingsSize)
                    {
                        /* new reservation */
                        pRequestHeader->rc = PGMR3MappingsFix(pVM, hypervisorInfo->hypervisorStart,
                                                              hypervisorInfo->hypervisorSize);
                        LogRel(("Guest reported fixed hypervisor window at 0x%p (size = 0x%x, rc = %Rrc)\n",
                                (uintptr_t)hypervisorInfo->hypervisorStart,
                                hypervisorInfo->hypervisorSize,
                                pRequestHeader->rc));
                    }
                }
            }
            break;
        }

        /*
         * Set the system power status
         */
        case VMMDevReq_SetPowerStatus:
        {
            if (pRequestHeader->size != sizeof(VMMDevPowerStateRequest))
            {
                AssertMsgFailed(("VMMDev power state request structure has invalid size!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevPowerStateRequest *powerStateRequest = (VMMDevPowerStateRequest*)pRequestHeader;
                switch(powerStateRequest->powerState)
                {
                    case VMMDevPowerState_Pause:
                    {
                        LogRel(("Guest requests the VM to be suspended (paused)\n"));
                        pRequestHeader->rc = rcRet = PDMDevHlpVMSuspend(pDevIns);
                        break;
                    }

                    case VMMDevPowerState_PowerOff:
                    {
                        LogRel(("Guest requests the VM to be turned off\n"));
                        pRequestHeader->rc = rcRet = PDMDevHlpVMPowerOff(pDevIns);
                        break;
                    }

                    case VMMDevPowerState_SaveState:
                    {
                        /** @todo no API for that yet */
                        pRequestHeader->rc = VERR_NOT_IMPLEMENTED;
                        break;
                    }

                    default:
                        AssertMsgFailed(("VMMDev invalid power state request: %d\n", powerStateRequest->powerState));
                        pRequestHeader->rc = VERR_INVALID_PARAMETER;
                        break;
                }
            }
            break;
        }

        /*
         * Get display change request
         */
        case VMMDevReq_GetDisplayChangeRequest:
        {
            if (pRequestHeader->size != sizeof(VMMDevDisplayChangeRequest))
            {
                /* Assert only if the size also not equal to a previous version size to prevent
                 * assertion with old additions.
                 */
                AssertMsg(pRequestHeader->size == sizeof(VMMDevDisplayChangeRequest) - sizeof (uint32_t),
                          ("VMMDev display change request structure has invalid size!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevDisplayChangeRequest *displayChangeRequest = (VMMDevDisplayChangeRequest*)pRequestHeader;

                if (displayChangeRequest->eventAck == VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST)
                {
                    /* Remember which resolution the client has queried, subsequent reads will return the same values. */
                    pThis->lastReadDisplayChangeRequest = pThis->displayChangeRequest;
                }

                /* just pass on the information */
                Log(("VMMDev: returning display change request xres = %d, yres = %d, bpp = %d\n",
                     pThis->displayChangeRequest.xres, pThis->displayChangeRequest.yres, pThis->displayChangeRequest.bpp));
                displayChangeRequest->xres = pThis->lastReadDisplayChangeRequest.xres;
                displayChangeRequest->yres = pThis->lastReadDisplayChangeRequest.yres;
                displayChangeRequest->bpp  = pThis->lastReadDisplayChangeRequest.bpp;

                pRequestHeader->rc = VINF_SUCCESS;
            }
            break;
        }

        case VMMDevReq_GetDisplayChangeRequest2:
        {
            if (pRequestHeader->size != sizeof(VMMDevDisplayChangeRequest2))
            {
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevDisplayChangeRequest2 *displayChangeRequest = (VMMDevDisplayChangeRequest2*)pRequestHeader;

                if (displayChangeRequest->eventAck == VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST)
                {
                    /* Remember which resolution the client has queried, subsequent reads will return the same values. */
                    pThis->lastReadDisplayChangeRequest = pThis->displayChangeRequest;
                }

                /* just pass on the information */
                Log(("VMMDev: returning display change request xres = %d, yres = %d, bpp = %d at %d\n",
                     pThis->displayChangeRequest.xres, pThis->displayChangeRequest.yres, pThis->displayChangeRequest.bpp, pThis->displayChangeRequest.display));
                displayChangeRequest->xres    = pThis->lastReadDisplayChangeRequest.xres;
                displayChangeRequest->yres    = pThis->lastReadDisplayChangeRequest.yres;
                displayChangeRequest->bpp     = pThis->lastReadDisplayChangeRequest.bpp;
                displayChangeRequest->display = pThis->lastReadDisplayChangeRequest.display;

                pRequestHeader->rc = VINF_SUCCESS;
            }
            break;
        }

        /*
         * Query whether the given video mode is supported
         */
        case VMMDevReq_VideoModeSupported:
        {
            if (pRequestHeader->size != sizeof(VMMDevVideoModeSupportedRequest))
            {
                AssertMsgFailed(("VMMDev video mode supported request structure has invalid size!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevVideoModeSupportedRequest *videoModeSupportedRequest = (VMMDevVideoModeSupportedRequest*)pRequestHeader;
                /* forward the call */
                pRequestHeader->rc = pThis->pDrv->pfnVideoModeSupported(pThis->pDrv,
                                                                       videoModeSupportedRequest->width,
                                                                       videoModeSupportedRequest->height,
                                                                       videoModeSupportedRequest->bpp,
                                                                       &videoModeSupportedRequest->fSupported);
            }
            break;
        }

        /*
         * Query the height reduction in pixels
         */
        case VMMDevReq_GetHeightReduction:
        {
            if (pRequestHeader->size != sizeof(VMMDevGetHeightReductionRequest))
            {
                AssertMsgFailed(("VMMDev height reduction request structure has invalid size!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevGetHeightReductionRequest *heightReductionRequest = (VMMDevGetHeightReductionRequest*)pRequestHeader;
                /* forward the call */
                pRequestHeader->rc = pThis->pDrv->pfnGetHeightReduction(pThis->pDrv,
                                                                       &heightReductionRequest->heightReduction);
            }
            break;
        }

        /*
         * Acknowledge VMMDev events
         */
        case VMMDevReq_AcknowledgeEvents:
        {
            if (pRequestHeader->size != sizeof(VMMDevEvents))
            {
                AssertMsgFailed(("VMMDevReq_AcknowledgeEvents structure has invalid size!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                if (VBOX_GUEST_ADDITIONS_VERSION_1_03 (pThis))
                {
                    vmmdevSetIRQ_Legacy_EMT (pThis);
                }
                else
                {
                    VMMDevEvents *pAckRequest;

                    if (pThis->fNewGuestFilterMask)
                    {
                        pThis->fNewGuestFilterMask = false;
                        pThis->u32GuestFilterMask = pThis->u32NewGuestFilterMask;
                    }

                    pAckRequest = (VMMDevEvents *)pRequestHeader;
                    pAckRequest->events =
                        pThis->u32HostEventFlags & pThis->u32GuestFilterMask;

                    pThis->u32HostEventFlags &= ~pThis->u32GuestFilterMask;
                    pThis->pVMMDevRAMR3->V.V1_04.fHaveEvents = false;
                    PDMDevHlpPCISetIrqNoWait (pThis->pDevIns, 0, 0);
                }
                pRequestHeader->rc = VINF_SUCCESS;
            }
            break;
        }

        /*
         * Change guest filter mask
         */
        case VMMDevReq_CtlGuestFilterMask:
        {
            if (pRequestHeader->size != sizeof(VMMDevCtlGuestFilterMask))
            {
                AssertMsgFailed(("VMMDevReq_AcknowledgeEvents structure has invalid size!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevCtlGuestFilterMask *pCtlMaskRequest;

                pCtlMaskRequest = (VMMDevCtlGuestFilterMask *)pRequestHeader;
                /* The HGCM events are enabled by the VMMDev device automatically when any
                 * HGCM command is issued. The guest then can not disable these events.
                 */
                vmmdevCtlGuestFilterMask_EMT (pThis,
                                              pCtlMaskRequest->u32OrMask,
                                              pCtlMaskRequest->u32NotMask & ~VMMDEV_EVENT_HGCM);
                pRequestHeader->rc = VINF_SUCCESS;

            }
            break;
        }

#ifdef VBOX_WITH_HGCM
        /*
         * Process HGCM request
         */
        case VMMDevReq_HGCMConnect:
        {
            if (pRequestHeader->size < sizeof(VMMDevHGCMConnect))
            {
                AssertMsgFailed(("VMMDevReq_HGCMConnect structure has invalid size!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else if (!pThis->pHGCMDrv)
            {
                Log(("VMMDevReq_HGCMConnect HGCM Connector is NULL!\n"));
                pRequestHeader->rc = VERR_NOT_SUPPORTED;
            }
            else
            {
                VMMDevHGCMConnect *pHGCMConnect = (VMMDevHGCMConnect *)pRequestHeader;

                Log(("VMMDevReq_HGCMConnect\n"));

                pRequestHeader->rc = vmmdevHGCMConnect (pThis, pHGCMConnect, (RTGCPHYS)u32);
            }
            break;
        }

        case VMMDevReq_HGCMDisconnect:
        {
            if (pRequestHeader->size < sizeof(VMMDevHGCMDisconnect))
            {
                AssertMsgFailed(("VMMDevReq_HGCMDisconnect structure has invalid size!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else if (!pThis->pHGCMDrv)
            {
                Log(("VMMDevReq_HGCMDisconnect HGCM Connector is NULL!\n"));
                pRequestHeader->rc = VERR_NOT_SUPPORTED;
            }
            else
            {
                VMMDevHGCMDisconnect *pHGCMDisconnect = (VMMDevHGCMDisconnect *)pRequestHeader;

                Log(("VMMDevReq_VMMDevHGCMDisconnect\n"));
                pRequestHeader->rc = vmmdevHGCMDisconnect (pThis, pHGCMDisconnect, (RTGCPHYS)u32);
            }
            break;
        }

#ifdef VBOX_WITH_64_BITS_GUESTS
        case VMMDevReq_HGCMCall32:
        case VMMDevReq_HGCMCall64:
#else
        case VMMDevReq_HGCMCall:
#endif /* VBOX_WITH_64_BITS_GUESTS */
        {
            if (pRequestHeader->size < sizeof(VMMDevHGCMCall))
            {
                AssertMsgFailed(("VMMDevReq_HGCMCall structure has invalid size!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else if (!pThis->pHGCMDrv)
            {
                Log(("VMMDevReq_HGCMCall HGCM Connector is NULL!\n"));
                pRequestHeader->rc = VERR_NOT_SUPPORTED;
            }
            else
            {
                VMMDevHGCMCall *pHGCMCall = (VMMDevHGCMCall *)pRequestHeader;

                Log2(("VMMDevReq_HGCMCall: sizeof (VMMDevHGCMRequest) = %04X\n", sizeof (VMMDevHGCMCall)));
                Log2(("%.*Rhxd\n", pRequestHeader->size, pRequestHeader));

#ifdef VBOX_WITH_64_BITS_GUESTS
                bool f64Bits = (pRequestHeader->requestType == VMMDevReq_HGCMCall64);
#else
                bool f64Bits = false;
#endif /* VBOX_WITH_64_BITS_GUESTS */

                pRequestHeader->rc = vmmdevHGCMCall (pThis, pHGCMCall, (RTGCPHYS)u32, f64Bits);
            }
            break;
        }
#endif /* VBOX_WITH_HGCM */

        case VMMDevReq_HGCMCancel:
        {
            if (pRequestHeader->size < sizeof(VMMDevHGCMCancel))
            {
                AssertMsgFailed(("VMMDevReq_HGCMCancel structure has invalid size!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else if (!pThis->pHGCMDrv)
            {
                Log(("VMMDevReq_HGCMCancel HGCM Connector is NULL!\n"));
                pRequestHeader->rc = VERR_NOT_SUPPORTED;
            }
            else
            {
                VMMDevHGCMCancel *pHGCMCancel = (VMMDevHGCMCancel *)pRequestHeader;

                Log(("VMMDevReq_VMMDevHGCMCancel\n"));
                pRequestHeader->rc = vmmdevHGCMCancel (pThis, pHGCMCancel, (RTGCPHYS)u32);
            }
            break;
        }

        case VMMDevReq_VideoAccelEnable:
        {
            if (pRequestHeader->size < sizeof(VMMDevVideoAccelEnable))
            {
                Log(("VMMDevReq_VideoAccelEnable request size too small!!!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else if (!pThis->pDrv)
            {
                Log(("VMMDevReq_VideoAccelEnable Connector is NULL!!!\n"));
                pRequestHeader->rc = VERR_NOT_SUPPORTED;
            }
            else
            {
                VMMDevVideoAccelEnable *ptr = (VMMDevVideoAccelEnable *)pRequestHeader;

                if (ptr->cbRingBuffer != VBVA_RING_BUFFER_SIZE)
                {
                    /* The guest driver seems compiled with another headers. */
                    Log(("VMMDevReq_VideoAccelEnable guest ring buffer size %d, should be %d!!!\n", ptr->cbRingBuffer, VBVA_RING_BUFFER_SIZE));
                    pRequestHeader->rc = VERR_INVALID_PARAMETER;
                }
                else
                {
                    /* The request is correct. */
                    ptr->fu32Status |= VBVA_F_STATUS_ACCEPTED;

                    LogFlow(("VMMDevReq_VideoAccelEnable ptr->u32Enable = %d\n", ptr->u32Enable));

                    pRequestHeader->rc = ptr->u32Enable?
                        pThis->pDrv->pfnVideoAccelEnable (pThis->pDrv, true, &pThis->pVMMDevRAMR3->vbvaMemory):
                        pThis->pDrv->pfnVideoAccelEnable (pThis->pDrv, false, NULL);

                    if (   ptr->u32Enable
                        && RT_SUCCESS (pRequestHeader->rc))
                    {
                        ptr->fu32Status |= VBVA_F_STATUS_ENABLED;

                        /* Remember that guest successfully enabled acceleration.
                         * We need to reestablish it on restoring the VM from saved state.
                         */
                        pThis->u32VideoAccelEnabled = 1;
                    }
                    else
                    {
                        /* The acceleration was not enabled. Remember that. */
                        pThis->u32VideoAccelEnabled = 0;
                    }
                }
            }
            break;
        }

        case VMMDevReq_VideoAccelFlush:
        {
            if (pRequestHeader->size < sizeof(VMMDevVideoAccelFlush))
            {
                AssertMsgFailed(("VMMDevReq_VideoAccelFlush request size too small.\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else if (!pThis->pDrv)
            {
                Log(("VMMDevReq_VideoAccelFlush Connector is NULL!\n"));
                pRequestHeader->rc = VERR_NOT_SUPPORTED;
            }
            else
            {
                pThis->pDrv->pfnVideoAccelFlush (pThis->pDrv);

                pRequestHeader->rc = VINF_SUCCESS;
            }
            break;
        }

        case VMMDevReq_VideoSetVisibleRegion:
        {
            if (pRequestHeader->size < sizeof(VMMDevVideoSetVisibleRegion))
            {
                Log(("VMMDevReq_VideoSetVisibleRegion request size too small!!!\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else if (!pThis->pDrv)
            {
                Log(("VMMDevReq_VideoSetVisibleRegion Connector is NULL!!!\n"));
                pRequestHeader->rc = VERR_NOT_SUPPORTED;
            }
            else
            {
                VMMDevVideoSetVisibleRegion *ptr = (VMMDevVideoSetVisibleRegion *)pRequestHeader;

                if (!ptr->cRect)
                {
                    Log(("VMMDevReq_VideoSetVisibleRegion no rectangles!!!\n"));
                    pRequestHeader->rc = VERR_INVALID_PARAMETER;
                }
                else
                if (pRequestHeader->size != sizeof(VMMDevVideoSetVisibleRegion) + (ptr->cRect-1)*sizeof(RTRECT))
                {
                    Log(("VMMDevReq_VideoSetVisibleRegion request size too small!!!\n"));
                    pRequestHeader->rc = VERR_INVALID_PARAMETER;
                }
                else
                {
                    Log(("VMMDevReq_VideoSetVisibleRegion %d rectangles\n", ptr->cRect));
                    /* forward the call */
                    pRequestHeader->rc = pThis->pDrv->pfnSetVisibleRegion(pThis->pDrv, ptr->cRect, &ptr->Rect);
                }
            }
            break;
        }

        case VMMDevReq_GetSeamlessChangeRequest:
        {
            if (pRequestHeader->size != sizeof(VMMDevSeamlessChangeRequest))
            {
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevSeamlessChangeRequest *seamlessChangeRequest = (VMMDevSeamlessChangeRequest*)pRequestHeader;
                /* just pass on the information */
                Log(("VMMDev: returning seamless change request mode=%d\n", pThis->fSeamlessEnabled));
                if (pThis->fSeamlessEnabled)
                    seamlessChangeRequest->mode = VMMDev_Seamless_Visible_Region;
                else
                    seamlessChangeRequest->mode = VMMDev_Seamless_Disabled;

                if (seamlessChangeRequest->eventAck == VMMDEV_EVENT_SEAMLESS_MODE_CHANGE_REQUEST)
                {
                    /* Remember which mode the client has queried. */
                    pThis->fLastSeamlessEnabled = pThis->fSeamlessEnabled;
                }

                pRequestHeader->rc = VINF_SUCCESS;
            }
            break;
        }

        case VMMDevReq_GetVRDPChangeRequest:
        {
            if (pRequestHeader->size != sizeof(VMMDevVRDPChangeRequest))
            {
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevVRDPChangeRequest *vrdpChangeRequest = (VMMDevVRDPChangeRequest*)pRequestHeader;
                /* just pass on the information */
                Log(("VMMDev: returning VRDP status %d level %d\n", pThis->fVRDPEnabled, pThis->u32VRDPExperienceLevel));

                vrdpChangeRequest->u8VRDPActive = pThis->fVRDPEnabled;
                vrdpChangeRequest->u32VRDPExperienceLevel = pThis->u32VRDPExperienceLevel;

                pRequestHeader->rc = VINF_SUCCESS;
            }
            break;
        }

        case VMMDevReq_GetMemBalloonChangeRequest:
        {
            Log(("VMMDevReq_GetMemBalloonChangeRequest\n"));
            if (pRequestHeader->size != sizeof(VMMDevGetMemBalloonChangeRequest))
            {
                AssertFailed();
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevGetMemBalloonChangeRequest *memBalloonChangeRequest = (VMMDevGetMemBalloonChangeRequest*)pRequestHeader;
                /* just pass on the information */
                Log(("VMMDev: returning memory balloon size =%d\n", pThis->u32MemoryBalloonSize));
                memBalloonChangeRequest->u32BalloonSize = pThis->u32MemoryBalloonSize;
                memBalloonChangeRequest->u32PhysMemSize = pThis->cbGuestRAM / (uint64_t)_1M;

                if (memBalloonChangeRequest->eventAck == VMMDEV_EVENT_BALLOON_CHANGE_REQUEST)
                {
                    /* Remember which mode the client has queried. */
                    pThis->u32LastMemoryBalloonSize = pThis->u32MemoryBalloonSize;
                }

                pRequestHeader->rc = VINF_SUCCESS;
            }
            break;
        }

        case VMMDevReq_ChangeMemBalloon:
        {
            VMMDevChangeMemBalloon *memBalloonChange = (VMMDevChangeMemBalloon*)pRequestHeader;

            Log(("VMMDevReq_ChangeMemBalloon\n"));
            if (    pRequestHeader->size < sizeof(VMMDevChangeMemBalloon)
                ||  memBalloonChange->cPages != VMMDEV_MEMORY_BALLOON_CHUNK_PAGES
                ||  pRequestHeader->size != (uint32_t)RT_OFFSETOF(VMMDevChangeMemBalloon, aPhysPage[memBalloonChange->cPages]))
            {
                AssertFailed();
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                pRequestHeader->rc = pThis->pDrv->pfnChangeMemoryBalloon(pThis->pDrv, !!memBalloonChange->fInflate, memBalloonChange->cPages, memBalloonChange->aPhysPage);
            }
            break;
        }

        case VMMDevReq_GetStatisticsChangeRequest:
        {
            Log(("VMMDevReq_GetStatisticsChangeRequest\n"));
            if (pRequestHeader->size != sizeof(VMMDevGetStatisticsChangeRequest))
            {
                AssertFailed();
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevGetStatisticsChangeRequest *statIntervalChangeRequest = (VMMDevGetStatisticsChangeRequest*)pRequestHeader;
                /* just pass on the information */
                Log(("VMMDev: returning statistics interval %d seconds\n", pThis->u32StatIntervalSize));
                statIntervalChangeRequest->u32StatInterval = pThis->u32StatIntervalSize;

                if (statIntervalChangeRequest->eventAck == VMMDEV_EVENT_STATISTICS_INTERVAL_CHANGE_REQUEST)
                {
                    /* Remember which mode the client has queried. */
                    pThis->u32LastStatIntervalSize= pThis->u32StatIntervalSize;
                }

                pRequestHeader->rc = VINF_SUCCESS;
            }
            break;
        }

        case VMMDevReq_ReportGuestStats:
        {
            Log(("VMMDevReq_ReportGuestStats\n"));
            if (pRequestHeader->size != sizeof(VMMDevReportGuestStats))
            {
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevReportGuestStats *stats = (VMMDevReportGuestStats*)pRequestHeader;

#ifdef DEBUG
                VBoxGuestStatistics *pGuestStats = &stats->guestStats;

                Log(("Current statistics:\n"));
                if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_CPU_LOAD_IDLE)
                    Log(("CPU%d: CPU Load Idle          %-3d%%\n", pGuestStats->u32CpuId, pGuestStats->u32CpuLoad_Idle));

                if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_CPU_LOAD_KERNEL)
                    Log(("CPU%d: CPU Load Kernel        %-3d%%\n", pGuestStats->u32CpuId, pGuestStats->u32CpuLoad_Kernel));

                if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_CPU_LOAD_USER)
                    Log(("CPU%d: CPU Load User          %-3d%%\n", pGuestStats->u32CpuId, pGuestStats->u32CpuLoad_User));

                if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_THREADS)
                    Log(("CPU%d: Thread                 %d\n", pGuestStats->u32CpuId, pGuestStats->u32Threads));

                if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_PROCESSES)
                    Log(("CPU%d: Processes              %d\n", pGuestStats->u32CpuId, pGuestStats->u32Processes));

                if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_HANDLES)
                    Log(("CPU%d: Handles                %d\n", pGuestStats->u32CpuId, pGuestStats->u32Handles));

                if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_MEMORY_LOAD)
                    Log(("CPU%d: Memory Load            %d%%\n", pGuestStats->u32CpuId, pGuestStats->u32MemoryLoad));

                /* Note that reported values are in pages; upper layers expect them in megabytes */
                Assert(pGuestStats->u32PageSize == 4096);
                if (pGuestStats->u32PageSize != 4096)
                    pGuestStats->u32PageSize = 4096;

                if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_PHYS_MEM_TOTAL)
                    Log(("CPU%d: Total physical memory  %-4d MB\n", pGuestStats->u32CpuId, (pGuestStats->u32PhysMemTotal + (_1M/pGuestStats->u32PageSize)-1)/ (_1M/pGuestStats->u32PageSize)));

                if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_PHYS_MEM_AVAIL)
                    Log(("CPU%d: Free physical memory   %-4d MB\n", pGuestStats->u32CpuId, pGuestStats->u32PhysMemAvail / (_1M/pGuestStats->u32PageSize)));

                if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_PHYS_MEM_BALLOON)
                    Log(("CPU%d: Memory balloon size    %-4d MB\n", pGuestStats->u32CpuId, pGuestStats->u32PhysMemBalloon / (_1M/pGuestStats->u32PageSize)));

                if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_MEM_COMMIT_TOTAL)
                    Log(("CPU%d: Committed memory       %-4d MB\n", pGuestStats->u32CpuId, pGuestStats->u32MemCommitTotal / (_1M/pGuestStats->u32PageSize)));

                if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_MEM_KERNEL_TOTAL)
                    Log(("CPU%d: Total kernel memory    %-4d MB\n", pGuestStats->u32CpuId, pGuestStats->u32MemKernelTotal / (_1M/pGuestStats->u32PageSize)));

                if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_MEM_KERNEL_PAGED)
                    Log(("CPU%d: Paged kernel memory    %-4d MB\n", pGuestStats->u32CpuId, pGuestStats->u32MemKernelPaged / (_1M/pGuestStats->u32PageSize)));

                if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_MEM_KERNEL_NONPAGED)
                    Log(("CPU%d: Nonpaged kernel memory %-4d MB\n", pGuestStats->u32CpuId, pGuestStats->u32MemKernelNonPaged / (_1M/pGuestStats->u32PageSize)));

                if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_MEM_SYSTEM_CACHE)
                    Log(("CPU%d: System cache size      %-4d MB\n", pGuestStats->u32CpuId, pGuestStats->u32MemSystemCache / (_1M/pGuestStats->u32PageSize)));

                if (pGuestStats->u32StatCaps & VBOX_GUEST_STAT_PAGE_FILE_SIZE)
                    Log(("CPU%d: Page file size         %-4d MB\n", pGuestStats->u32CpuId, pGuestStats->u32PageFileSize / (_1M/pGuestStats->u32PageSize)));
                Log(("Statistics end *******************\n"));
#endif

                /* forward the call */
                pRequestHeader->rc = pThis->pDrv->pfnReportStatistics(pThis->pDrv, &stats->guestStats);
            }
            break;
        }

        case VMMDevReq_QueryCredentials:
        {
            if (pRequestHeader->size != sizeof(VMMDevCredentials))
            {
                AssertMsgFailed(("VMMDevReq_QueryCredentials request size too small.\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevCredentials *credentials = (VMMDevCredentials*)pRequestHeader;

                /* let's start by nulling out the data */
                memset(credentials->szUserName, '\0', VMMDEV_CREDENTIALS_STRLEN);
                memset(credentials->szPassword, '\0', VMMDEV_CREDENTIALS_STRLEN);
                memset(credentials->szDomain, '\0', VMMDEV_CREDENTIALS_STRLEN);

                /* should we return whether we got credentials for a logon? */
                if (credentials->u32Flags & VMMDEV_CREDENTIALS_QUERYPRESENCE)
                {
                    if (   pThis->credentialsLogon.szUserName[0]
                        || pThis->credentialsLogon.szPassword[0]
                        || pThis->credentialsLogon.szDomain[0])
                    {
                        credentials->u32Flags |= VMMDEV_CREDENTIALS_PRESENT;
                    }
                    else
                    {
                        credentials->u32Flags &= ~VMMDEV_CREDENTIALS_PRESENT;
                    }
                }

                /* does the guest want to read logon credentials? */
                if (credentials->u32Flags & VMMDEV_CREDENTIALS_READ)
                {
                    if (pThis->credentialsLogon.szUserName[0])
                        strcpy(credentials->szUserName, pThis->credentialsLogon.szUserName);
                    if (pThis->credentialsLogon.szPassword[0])
                        strcpy(credentials->szPassword, pThis->credentialsLogon.szPassword);
                    if (pThis->credentialsLogon.szDomain[0])
                        strcpy(credentials->szDomain, pThis->credentialsLogon.szDomain);
                    if (!pThis->credentialsLogon.fAllowInteractiveLogon)
                        credentials->u32Flags |= VMMDEV_CREDENTIALS_NOLOCALLOGON;
                    else
                        credentials->u32Flags &= ~VMMDEV_CREDENTIALS_NOLOCALLOGON;
                }

                if (!pThis->fKeepCredentials)
                {
                    /* does the caller want us to destroy the logon credentials? */
                    if (credentials->u32Flags & VMMDEV_CREDENTIALS_CLEAR)
                    {
                        memset(pThis->credentialsLogon.szUserName, '\0', VMMDEV_CREDENTIALS_STRLEN);
                        memset(pThis->credentialsLogon.szPassword, '\0', VMMDEV_CREDENTIALS_STRLEN);
                        memset(pThis->credentialsLogon.szDomain, '\0', VMMDEV_CREDENTIALS_STRLEN);
                    }
                }

                /* does the guest want to read credentials for verification? */
                if (credentials->u32Flags & VMMDEV_CREDENTIALS_READJUDGE)
                {
                    if (pThis->credentialsJudge.szUserName[0])
                        strcpy(credentials->szUserName, pThis->credentialsJudge.szUserName);
                    if (pThis->credentialsJudge.szPassword[0])
                        strcpy(credentials->szPassword, pThis->credentialsJudge.szPassword);
                    if (pThis->credentialsJudge.szDomain[0])
                        strcpy(credentials->szDomain, pThis->credentialsJudge.szDomain);
                }

                /* does the caller want us to destroy the judgement credentials? */
                if (credentials->u32Flags & VMMDEV_CREDENTIALS_CLEARJUDGE)
                {
                    memset(pThis->credentialsJudge.szUserName, '\0', VMMDEV_CREDENTIALS_STRLEN);
                    memset(pThis->credentialsJudge.szPassword, '\0', VMMDEV_CREDENTIALS_STRLEN);
                    memset(pThis->credentialsJudge.szDomain, '\0', VMMDEV_CREDENTIALS_STRLEN);
                }

                pRequestHeader->rc = VINF_SUCCESS;
            }
            break;
        }

        case VMMDevReq_ReportCredentialsJudgement:
        {
            if (pRequestHeader->size != sizeof(VMMDevCredentials))
            {
                AssertMsgFailed(("VMMDevReq_ReportCredentialsJudgement request size too small.\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevCredentials *credentials = (VMMDevCredentials*)pRequestHeader;

                /* what does the guest think about the credentials? (note: the order is important here!) */
                if (credentials->u32Flags & VMMDEV_CREDENTIALS_JUDGE_DENY)
                {
                    pThis->pDrv->pfnSetCredentialsJudgementResult(pThis->pDrv, VMMDEV_CREDENTIALS_JUDGE_DENY);
                }
                else if (credentials->u32Flags & VMMDEV_CREDENTIALS_JUDGE_NOJUDGEMENT)
                {
                    pThis->pDrv->pfnSetCredentialsJudgementResult(pThis->pDrv, VMMDEV_CREDENTIALS_JUDGE_NOJUDGEMENT);
                }
                else if (credentials->u32Flags & VMMDEV_CREDENTIALS_JUDGE_OK)
                {
                    pThis->pDrv->pfnSetCredentialsJudgementResult(pThis->pDrv, VMMDEV_CREDENTIALS_JUDGE_OK);
                }
                else
                    Log(("VMMDevReq_ReportCredentialsJudgement: invalid flags: %d!!!\n", credentials->u32Flags));

                pRequestHeader->rc = VINF_SUCCESS;
            }
            break;
        }

#ifdef DEBUG
        case VMMDevReq_LogString:
        {
            if (pRequestHeader->size < sizeof(VMMDevReqLogString))
            {
                AssertMsgFailed(("VMMDevReq_LogString request size too small.\n"));
                pRequestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevReqLogString *pReqLogString = (VMMDevReqLogString*)pRequestHeader;
#undef LOG_GROUP
#define LOG_GROUP LOG_GROUP_DEV_VMM_BACKDOOR
//                Log(("Guest Log: %s", pReqLogString->szString));
                Log(("DEBUG LOG: %s", pReqLogString->szString));

#undef LOG_GROUP
#define LOG_GROUP LOG_GROUP_DEV_VMM
                pRequestHeader->rc = VINF_SUCCESS;
            }
            break;
        }
#endif
        default:
        {
            pRequestHeader->rc = VERR_NOT_IMPLEMENTED;

            Log(("VMMDev unknown request type %d\n", pRequestHeader->requestType));

            break;
        }
    }

end:
    /* Write the result back to guest memory */
    if (pRequestHeader)
    {
        PDMDevHlpPhysWrite(pDevIns, (RTGCPHYS)u32, pRequestHeader, pRequestHeader->size);
        RTMemFree(pRequestHeader);
    }
    else
    {
        /* early error case; write back header only */
        PDMDevHlpPhysWrite(pDevIns, (RTGCPHYS)u32, &requestHeader, sizeof(requestHeader));
    }
    return rcRet;
}

/**
 * Callback function for mapping an PCI I/O region.
 *
 * @return VBox status code.
 * @param   pPciDev         Pointer to PCI device. Use pPciDev->pDevIns to get the device instance.
 * @param   iRegion         The region number.
 * @param   GCPhysAddress   Physical address of the region. If iType is PCI_ADDRESS_SPACE_IO, this is an
 *                          I/O port, else it's a physical address.
 *                          This address is *NOT* relative to pci_mem_base like earlier!
 * @param   enmType         One of the PCI_ADDRESS_SPACE_* values.
 */
static DECLCALLBACK(int) vmmdevIORAMRegionMap(PPCIDEVICE pPciDev, /*unsigned*/ int iRegion, RTGCPHYS GCPhysAddress, uint32_t cb, PCIADDRESSSPACE enmType)
{
    LogFlow(("vmmdevR3IORAMRegionMap: iRegion=%d GCPhysAddress=%RGp cb=%#x enmType=%d\n", iRegion, GCPhysAddress, cb, enmType));
    VMMDevState *pThis = PCIDEV_2_VMMDEVSTATE(pPciDev);
    int rc;

    if (iRegion == 1)
    {
        AssertReturn(enmType == PCI_ADDRESS_SPACE_MEM, VERR_INTERNAL_ERROR);
        Assert(pThis->pVMMDevRAMR3 != NULL);
        if (GCPhysAddress != NIL_RTGCPHYS)
        {
            /*
             * Map the MMIO2 memory.
             */
            pThis->GCPhysVMMDevRAM = GCPhysAddress;
            Assert(pThis->GCPhysVMMDevRAM == GCPhysAddress);
            rc = PDMDevHlpMMIO2Map(pPciDev->pDevIns, iRegion, GCPhysAddress);
        }
        else
        {
            /*
             * It is about to be unmapped, just clean up.
             */
            pThis->GCPhysVMMDevRAM = NIL_RTGCPHYS32;
            rc = VINF_SUCCESS;
        }
    }
    else if (iRegion == 2)
    {
        AssertReturn(enmType == PCI_ADDRESS_SPACE_MEM_PREFETCH, VERR_INTERNAL_ERROR);
        Assert(pThis->pVMMDevHeapR3 != NULL);
        if (GCPhysAddress != NIL_RTGCPHYS)
        {
            /*
             * Map the MMIO2 memory.
             */
            pThis->GCPhysVMMDevHeap = GCPhysAddress;
            Assert(pThis->GCPhysVMMDevHeap == GCPhysAddress);
            rc = PDMDevHlpMMIO2Map(pPciDev->pDevIns, iRegion, GCPhysAddress);
            if (RT_SUCCESS(rc))
                rc = PDMDevHlpRegisterVMMDevHeap(pPciDev->pDevIns, GCPhysAddress, pThis->pVMMDevHeapR3, VMMDEV_HEAP_SIZE);
        }
        else
        {
            /*
             * It is about to be unmapped, just clean up.
             */
            PDMDevHlpUnregisterVMMDevHeap(pPciDev->pDevIns, pThis->GCPhysVMMDevHeap);
            pThis->GCPhysVMMDevHeap = NIL_RTGCPHYS32;
            rc = VINF_SUCCESS;
        }
    }
    else
    {
        AssertMsgFailed(("%d\n", iRegion));
        rc = VERR_INVALID_PARAMETER;
    }

    return rc;
}


/**
 * Callback function for mapping a PCI I/O region.
 *
 * @return VBox status code.
 * @param   pPciDev         Pointer to PCI device. Use pPciDev->pDevIns to get the device instance.
 * @param   iRegion         The region number.
 * @param   GCPhysAddress   Physical address of the region. If iType is PCI_ADDRESS_SPACE_IO, this is an
 *                          I/O port, else it's a physical address.
 *                          This address is *NOT* relative to pci_mem_base like earlier!
 * @param   enmType         One of the PCI_ADDRESS_SPACE_* values.
 */
static DECLCALLBACK(int) vmmdevIOPortRegionMap(PPCIDEVICE pPciDev, /*unsigned*/ int iRegion, RTGCPHYS GCPhysAddress, uint32_t cb, PCIADDRESSSPACE enmType)
{
    VMMDevState *pThis = PCIDEV_2_VMMDEVSTATE(pPciDev);
    int         rc = VINF_SUCCESS;

    Assert(enmType == PCI_ADDRESS_SPACE_IO);
    Assert(iRegion == 0);
    AssertMsg(RT_ALIGN(GCPhysAddress, 8) == GCPhysAddress, ("Expected 8 byte alignment. GCPhysAddress=%#x\n", GCPhysAddress));

    /*
     * Save the base port address to simplify Port offset calculations.
     */
    pThis->PortBase = (RTIOPORT)GCPhysAddress;

    /*
     * Register our port IO handlers.
     */
    rc = PDMDevHlpIOPortRegister(pPciDev->pDevIns,
                                 (RTIOPORT)GCPhysAddress + PORT_VMMDEV_REQUEST_OFFSET, 1,
                                 (void*)pThis, vmmdevRequestHandler,
                                 NULL, NULL, NULL, "VMMDev Request Handler");
    AssertRC(rc);
    return rc;
}

/**
 * Queries an interface to the driver.
 *
 * @returns Pointer to interface.
 * @returns NULL if the interface was not supported by the driver.
 * @param   pInterface          Pointer to this interface structure.
 * @param   enmInterface        The requested interface identification.
 * @thread  Any thread.
 */
static DECLCALLBACK(void *) vmmdevPortQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    VMMDevState *pThis = (VMMDevState*)((uintptr_t)pInterface - RT_OFFSETOF(VMMDevState, Base));
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pThis->Base;
        case PDMINTERFACE_VMMDEV_PORT:
            return &pThis->Port;
#ifdef VBOX_WITH_HGCM
        case PDMINTERFACE_HGCM_PORT:
            return &pThis->HGCMPort;
#endif
        case PDMINTERFACE_LED_PORTS:
            /* Currently only for shared folders */
            return &pThis->SharedFolders.ILeds;
        default:
            return NULL;
    }
}

/**
 * Gets the pointer to the status LED of a unit.
 *
 * @returns VBox status code.
 * @param   pInterface      Pointer to the interface structure containing the called function pointer.
 * @param   iLUN            The unit which status LED we desire.
 * @param   ppLed           Where to store the LED pointer.
 */
static DECLCALLBACK(int) vmmdevQueryStatusLed(PPDMILEDPORTS pInterface, unsigned iLUN, PPDMLED *ppLed)
{
    VMMDevState *pThis = (VMMDevState *)( (uintptr_t)pInterface - RT_OFFSETOF(VMMDevState, SharedFolders.ILeds) );
    if (iLUN == 0) /* LUN 0 is shared folders */
    {
        *ppLed = &pThis->SharedFolders.Led;
        return VINF_SUCCESS;
    }
    return VERR_PDM_LUN_NOT_FOUND;
}

/* -=-=-=-=-=- IVMMDevPort -=-=-=-=-=- */

/** Converts a VMMDev port interface pointer to a VMMDev state pointer. */
#define IVMMDEVPORT_2_VMMDEVSTATE(pInterface) ( (VMMDevState*)((uintptr_t)pInterface - RT_OFFSETOF(VMMDevState, Port)) )


/**
 * Return the current absolute mouse position in pixels
 *
 * @returns VBox status code
 * @param   pAbsX   Pointer of result value, can be NULL
 * @param   pAbsY   Pointer of result value, can be NULL
 */
static DECLCALLBACK(int) vmmdevQueryAbsoluteMouse(PPDMIVMMDEVPORT pInterface, uint32_t *pAbsX, uint32_t *pAbsY)
{
    VMMDevState *pThis = IVMMDEVPORT_2_VMMDEVSTATE(pInterface);
    if (pAbsX)
        *pAbsX = pThis->mouseXAbs;
    if (pAbsY)
        *pAbsY = pThis->mouseYAbs;
    return VINF_SUCCESS;
}

/**
 * Set the new absolute mouse position in pixels
 *
 * @returns VBox status code
 * @param   absX   New absolute X position
 * @param   absY   New absolute Y position
 */
static DECLCALLBACK(int) vmmdevSetAbsoluteMouse(PPDMIVMMDEVPORT pInterface, uint32_t absX, uint32_t absY)
{
    VMMDevState *pThis = IVMMDEVPORT_2_VMMDEVSTATE(pInterface);
    Log2(("vmmdevSetAbsoluteMouse: settings absolute position to x = %d, y = %d\n", absX, absY));
    pThis->mouseXAbs = absX;
    pThis->mouseYAbs = absY;
    VMMDevNotifyGuest (pThis, VMMDEV_EVENT_MOUSE_POSITION_CHANGED);
    return VINF_SUCCESS;
}

/**
 * Return the current mouse capability flags
 *
 * @returns VBox status code
 * @param   pCapabilities  Pointer of result value
 */
static DECLCALLBACK(int) vmmdevQueryMouseCapabilities(PPDMIVMMDEVPORT pInterface, uint32_t *pCapabilities)
{
    VMMDevState *pThis = IVMMDEVPORT_2_VMMDEVSTATE(pInterface);
    if (!pCapabilities)
        return VERR_INVALID_PARAMETER;
    *pCapabilities = pThis->mouseCapabilities;
    return VINF_SUCCESS;
}

/**
 * Set the current mouse capability flag (host side)
 *
 * @returns VBox status code
 * @param   capabilities  Capability mask
 */
static DECLCALLBACK(int) vmmdevSetMouseCapabilities(PPDMIVMMDEVPORT pInterface, uint32_t capabilities)
{
    VMMDevState *pThis = IVMMDEVPORT_2_VMMDEVSTATE(pInterface);

    bool bCapsChanged = ((capabilities & VMMDEV_MOUSEHOSTWANTSABS)
                         != (pThis->mouseCapabilities & VMMDEV_MOUSEHOSTWANTSABS));

    Log(("vmmdevSetMouseCapabilities: bCapsChanged %d\n", bCapsChanged));

    if (capabilities & VMMDEV_MOUSEHOSTCANNOTHWPOINTER)
        pThis->mouseCapabilities |= VMMDEV_MOUSEHOSTCANNOTHWPOINTER;
    else
        pThis->mouseCapabilities &= ~VMMDEV_MOUSEHOSTCANNOTHWPOINTER;

    if (capabilities & VMMDEV_MOUSEHOSTWANTSABS)
        pThis->mouseCapabilities |= VMMDEV_MOUSEHOSTWANTSABS;
    else
        pThis->mouseCapabilities &= ~VMMDEV_MOUSEHOSTWANTSABS;

    if (bCapsChanged)
        VMMDevNotifyGuest (pThis, VMMDEV_EVENT_MOUSE_CAPABILITIES_CHANGED);

    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmmdevRequestDisplayChange(PPDMIVMMDEVPORT pInterface, uint32_t xres, uint32_t yres, uint32_t bpp, uint32_t display)
{
    VMMDevState *pThis = IVMMDEVPORT_2_VMMDEVSTATE(pInterface);

    /* Verify that the new resolution is different and that guest does not yet know about it. */
    bool fSameResolution = (!xres || (pThis->lastReadDisplayChangeRequest.xres == xres)) &&
                           (!yres || (pThis->lastReadDisplayChangeRequest.yres == yres)) &&
                           (!bpp || (pThis->lastReadDisplayChangeRequest.bpp == bpp)) &&
                           pThis->lastReadDisplayChangeRequest.display == display;

    if (!xres && !yres && !bpp)
    {
        /* Special case of reset video mode. */
        fSameResolution = false;
    }

    Log3(("vmmdevRequestDisplayChange: same=%d. new: xres=%d, yres=%d, bpp=%d, display=%d. old: xres=%d, yres=%d, bpp=%d, display=%d.\n",
          fSameResolution, xres, yres, bpp, display, pThis->lastReadDisplayChangeRequest.xres, pThis->lastReadDisplayChangeRequest.yres, pThis->lastReadDisplayChangeRequest.bpp, pThis->lastReadDisplayChangeRequest.display));

    if (!fSameResolution)
    {
        LogRel(("VMMDev::SetVideoModeHint: got a video mode hint (%dx%dx%d) at %d\n",
                xres, yres, bpp, display));

        /* we could validate the information here but hey, the guest can do that as well! */
        pThis->displayChangeRequest.xres    = xres;
        pThis->displayChangeRequest.yres    = yres;
        pThis->displayChangeRequest.bpp     = bpp;
        pThis->displayChangeRequest.display = display;

        /* IRQ so the guest knows what's going on */
        VMMDevNotifyGuest (pThis, VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST);
    }

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) vmmdevRequestSeamlessChange(PPDMIVMMDEVPORT pInterface, bool fEnabled)
{
    VMMDevState *pThis = IVMMDEVPORT_2_VMMDEVSTATE(pInterface);

    /* Verify that the new resolution is different and that guest does not yet know about it. */
    bool fSameMode = (pThis->fLastSeamlessEnabled == fEnabled);

    Log(("vmmdevRequestSeamlessChange: same=%d. new=%d\n", fSameMode, fEnabled));

    if (!fSameMode)
    {
        /* we could validate the information here but hey, the guest can do that as well! */
        pThis->fSeamlessEnabled = fEnabled;

        /* IRQ so the guest knows what's going on */
        VMMDevNotifyGuest (pThis, VMMDEV_EVENT_SEAMLESS_MODE_CHANGE_REQUEST);
    }

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) vmmdevSetMemoryBalloon(PPDMIVMMDEVPORT pInterface, uint32_t ulBalloonSize)
{
    VMMDevState *pThis = IVMMDEVPORT_2_VMMDEVSTATE(pInterface);

    /* Verify that the new resolution is different and that guest does not yet know about it. */
    bool fSame = (pThis->u32LastMemoryBalloonSize == ulBalloonSize);

    Log(("vmmdevSetMemoryBalloon: old=%d. new=%d\n", pThis->u32LastMemoryBalloonSize, ulBalloonSize));

    if (!fSame)
    {
        /* we could validate the information here but hey, the guest can do that as well! */
        pThis->u32MemoryBalloonSize = ulBalloonSize;

        /* IRQ so the guest knows what's going on */
        VMMDevNotifyGuest (pThis, VMMDEV_EVENT_BALLOON_CHANGE_REQUEST);
    }

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) vmmdevVRDPChange(PPDMIVMMDEVPORT pInterface, bool fVRDPEnabled, uint32_t u32VRDPExperienceLevel)
{
    VMMDevState *pThis = IVMMDEVPORT_2_VMMDEVSTATE(pInterface);

    bool fSame = (pThis->fVRDPEnabled == fVRDPEnabled);

    Log(("vmmdevVRDPChange: old=%d. new=%d\n", pThis->fVRDPEnabled, fVRDPEnabled));

    if (!fSame)
    {
        pThis->fVRDPEnabled = fVRDPEnabled;
        pThis->u32VRDPExperienceLevel = u32VRDPExperienceLevel;

        VMMDevNotifyGuest (pThis, VMMDEV_EVENT_VRDP);
    }

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) vmmdevSetStatisticsInterval(PPDMIVMMDEVPORT pInterface, uint32_t ulStatInterval)
{
    VMMDevState *pThis = IVMMDEVPORT_2_VMMDEVSTATE(pInterface);

    /* Verify that the new resolution is different and that guest does not yet know about it. */
    bool fSame = (pThis->u32LastStatIntervalSize == ulStatInterval);

    Log(("vmmdevSetStatisticsInterval: old=%d. new=%d\n", pThis->u32LastStatIntervalSize, ulStatInterval));

    if (!fSame)
    {
        /* we could validate the information here but hey, the guest can do that as well! */
        pThis->u32StatIntervalSize = ulStatInterval;

        /* IRQ so the guest knows what's going on */
        VMMDevNotifyGuest (pThis, VMMDEV_EVENT_STATISTICS_INTERVAL_CHANGE_REQUEST);
    }

    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmmdevSetCredentials(PPDMIVMMDEVPORT pInterface, const char *pszUsername,
                                              const char *pszPassword, const char *pszDomain,
                                              uint32_t u32Flags)
{
    VMMDevState *pThis = IVMMDEVPORT_2_VMMDEVSTATE(pInterface);

    /* logon mode? */
    if (u32Flags & VMMDEV_SETCREDENTIALS_GUESTLOGON)
    {
        /* memorize the data */
        strcpy(pThis->credentialsLogon.szUserName, pszUsername);
        strcpy(pThis->credentialsLogon.szPassword, pszPassword);
        strcpy(pThis->credentialsLogon.szDomain,   pszDomain);
        pThis->credentialsLogon.fAllowInteractiveLogon = !(u32Flags & VMMDEV_SETCREDENTIALS_NOLOCALLOGON);
    }
    /* credentials verification mode? */
    else if (u32Flags & VMMDEV_SETCREDENTIALS_JUDGE)
    {
        /* memorize the data */
        strcpy(pThis->credentialsJudge.szUserName, pszUsername);
        strcpy(pThis->credentialsJudge.szPassword, pszPassword);
        strcpy(pThis->credentialsJudge.szDomain,   pszDomain);

        VMMDevNotifyGuest (pThis, VMMDEV_EVENT_JUDGE_CREDENTIALS);
    }
    else
        return VERR_INVALID_PARAMETER;

    return VINF_SUCCESS;
}

/**
 * Notification from the Display. Especially useful when
 * acceleration is disabled after a video mode change.
 *
 * @param   fEnable   Current acceleration status.
 */
static DECLCALLBACK(void) vmmdevVBVAChange(PPDMIVMMDEVPORT pInterface, bool fEnabled)
{
    VMMDevState *pThis = IVMMDEVPORT_2_VMMDEVSTATE(pInterface);

    Log(("vmmdevVBVAChange: fEnabled = %d\n", fEnabled));

    if (pThis)
    {
        pThis->u32VideoAccelEnabled = fEnabled;
    }

    return;
}


/* -=-=-=-=-=- IHGCMPort -=-=-=-=-=- */

/** Converts a VMMDev port interface pointer to a VMMDev state pointer. */
#define IHGCMPORT_2_VMMDEVSTATE(pInterface) ( (VMMDevState*)((uintptr_t)pInterface - RT_OFFSETOF(VMMDevState, HGCMPort)) )



#define VMMDEV_SSM_VERSION  8

/**
 * Saves a state of the VMM device.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSMHandle  The handle to save the state to.
 */
static DECLCALLBACK(int) vmmdevSaveState(PPDMDEVINS pDevIns, PSSMHANDLE pSSMHandle)
{
    VMMDevState *pThis = PDMINS_2_DATA(pDevIns, VMMDevState*);
    SSMR3PutU32(pSSMHandle, pThis->hypervisorSize);
    SSMR3PutU32(pSSMHandle, pThis->mouseCapabilities);
    SSMR3PutU32(pSSMHandle, pThis->mouseXAbs);
    SSMR3PutU32(pSSMHandle, pThis->mouseYAbs);

    SSMR3PutBool(pSSMHandle, pThis->fNewGuestFilterMask);
    SSMR3PutU32(pSSMHandle, pThis->u32NewGuestFilterMask);
    SSMR3PutU32(pSSMHandle, pThis->u32GuestFilterMask);
    SSMR3PutU32(pSSMHandle, pThis->u32HostEventFlags);
    // here be dragons (probably)
//    SSMR3PutBool(pSSMHandle, pThis->pVMMDevRAMR3->V.V1_04.fHaveEvents);
    SSMR3PutMem(pSSMHandle, &pThis->pVMMDevRAMR3->V, sizeof (pThis->pVMMDevRAMR3->V));

    SSMR3PutMem(pSSMHandle, &pThis->guestInfo, sizeof (pThis->guestInfo));
    SSMR3PutU32(pSSMHandle, pThis->fu32AdditionsOk);
    SSMR3PutU32(pSSMHandle, pThis->u32VideoAccelEnabled);

    SSMR3PutU32(pSSMHandle, pThis->guestCaps);

#ifdef VBOX_WITH_HGCM
    vmmdevHGCMSaveState (pThis, pSSMHandle);
#endif /* VBOX_WITH_HGCM */

    return VINF_SUCCESS;
}

/**
 * Loads the saved VMM device state.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSMHandle  The handle to the saved state.
 * @param   u32Version  The data unit version number.
 */
static DECLCALLBACK(int) vmmdevLoadState(PPDMDEVINS pDevIns, PSSMHANDLE pSSMHandle, uint32_t u32Version)
{
    /** @todo The code load code is assuming we're always loaded into a fresh VM. */
    VMMDevState *pThis = PDMINS_2_DATA(pDevIns, VMMDevState*);
    if (   SSM_VERSION_MAJOR_CHANGED(u32Version, VMMDEV_SSM_VERSION)
        || (SSM_VERSION_MINOR(u32Version) < 6))
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;

    SSMR3GetU32(pSSMHandle, &pThis->hypervisorSize);
    SSMR3GetU32(pSSMHandle, &pThis->mouseCapabilities);
    SSMR3GetU32(pSSMHandle, &pThis->mouseXAbs);
    SSMR3GetU32(pSSMHandle, &pThis->mouseYAbs);

    SSMR3GetBool(pSSMHandle, &pThis->fNewGuestFilterMask);
    SSMR3GetU32(pSSMHandle, &pThis->u32NewGuestFilterMask);
    SSMR3GetU32(pSSMHandle, &pThis->u32GuestFilterMask);
    SSMR3GetU32(pSSMHandle, &pThis->u32HostEventFlags);
//    SSMR3GetBool(pSSMHandle, &pThis->pVMMDevRAMR3->fHaveEvents);
    // here be dragons (probably)
    SSMR3GetMem(pSSMHandle, &pThis->pVMMDevRAMR3->V, sizeof (pThis->pVMMDevRAMR3->V));

    SSMR3GetMem(pSSMHandle, &pThis->guestInfo, sizeof (pThis->guestInfo));
    SSMR3GetU32(pSSMHandle, &pThis->fu32AdditionsOk);
    SSMR3GetU32(pSSMHandle, &pThis->u32VideoAccelEnabled);

    SSMR3GetU32(pSSMHandle, &pThis->guestCaps);

    /* Attributes which were temporarily introduced in r30072 */
    if (   SSM_VERSION_MAJOR(u32Version) ==  0
        && SSM_VERSION_MINOR(u32Version) == 7)
    {
        uint32_t temp;
        SSMR3GetU32(pSSMHandle, &temp);
        SSMR3GetU32(pSSMHandle, &temp);
    }

#ifdef VBOX_WITH_HGCM
    vmmdevHGCMLoadState (pThis, pSSMHandle);
#endif /* VBOX_WITH_HGCM */

    /*
     * On a resume, we send the capabilities changed message so
     * that listeners can sync their state again
     */
    Log(("vmmdevLoadState: capabilities changed (%x), informing connector\n", pThis->mouseCapabilities));
    if (pThis->pDrv)
        pThis->pDrv->pfnUpdateMouseCapabilities(pThis->pDrv, pThis->mouseCapabilities);

    /* Reestablish the acceleration status. */
    if (    pThis->u32VideoAccelEnabled
        &&  pThis->pDrv)
    {
        pThis->pDrv->pfnVideoAccelEnable (pThis->pDrv, !!pThis->u32VideoAccelEnabled, &pThis->pVMMDevRAMR3->vbvaMemory);
    }

    if (pThis->fu32AdditionsOk)
    {
        LogRel(("Guest Additions information report: additionsVersion = 0x%08X, osType = 0x%08X\n",
                pThis->guestInfo.additionsVersion,
                pThis->guestInfo.osType));
        if (pThis->pDrv)
            pThis->pDrv->pfnUpdateGuestVersion(pThis->pDrv, &pThis->guestInfo);
    }
    if (pThis->pDrv)
        pThis->pDrv->pfnUpdateGuestCapabilities(pThis->pDrv, pThis->guestCaps);

    return VINF_SUCCESS;
}

/**
 * Load state done callback. Notify guest of restore event.
 *
 * @returns VBox status code.
 * @param   pDevIns    The device instance.
 * @param   pSSMHandle The handle to the saved state.
 */
static DECLCALLBACK(int) vmmdevLoadStateDone(PPDMDEVINS pDevIns, PSSMHANDLE pSSMHandle)
{
    VMMDevState *pThis = PDMINS_2_DATA(pDevIns, VMMDevState*);

#ifdef VBOX_WITH_HGCM
    vmmdevHGCMLoadStateDone (pThis, pSSMHandle);
#endif /* VBOX_WITH_HGCM */

    VMMDevNotifyGuest (pThis, VMMDEV_EVENT_RESTORED);

    return VINF_SUCCESS;
}

/**
 * (Re-)initializes the MMIO2 data.
 *
 * @param   pThis           Pointer to the VMMDev instance data.
 */
static void vmmdevInitRam(VMMDevState *pThis)
{
    memset(pThis->pVMMDevRAMR3, 0, sizeof(VMMDevMemory));
    pThis->pVMMDevRAMR3->u32Size = sizeof(VMMDevMemory);
    pThis->pVMMDevRAMR3->u32Version = VMMDEV_MEMORY_VERSION;
}

/**
 * Construct a device instance for a VM.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 *                      If the registration structure is needed, pDevIns->pDevReg points to it.
 * @param   iInstance   Instance number. Use this to figure out which registers and such to use.
 *                      The device number is also found in pDevIns->iInstance, but since it's
 *                      likely to be freqently used PDM passes it as parameter.
 * @param   pCfgHandle  Configuration node handle for the device. Use this to obtain the configuration
 *                      of the device instance. It's also found in pDevIns->pCfgHandle, but like
 *                      iInstance it's expected to be used a bit in this function.
 */
static DECLCALLBACK(int) vmmdevConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfgHandle)
{
    int rc;
    VMMDevState *pThis = PDMINS_2_DATA(pDevIns, VMMDevState *);

    Assert(iInstance == 0);

    /*
     * Validate and read the configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle,
                              "GetHostTimeDisabled\0"
                              "BackdoorLogDisabled\0"
                              "KeepCredentials\0"
                              "HeapEnabled\0"
                              ))
        return VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES;

    rc = CFGMR3QueryBoolDef(pCfgHandle, "GetHostTimeDisabled", &pThis->fGetHostTimeDisabled, false);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed querying \"GetHostTimeDisabled\" as a boolean"));

    rc = CFGMR3QueryBoolDef(pCfgHandle, "BackdoorLogDisabled", &pThis->fBackdoorLogDisabled, false);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed querying \"BackdoorLogDisabled\" as a boolean"));

    rc = CFGMR3QueryBoolDef(pCfgHandle, "KeepCredentials", &pThis->fKeepCredentials, false);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed querying \"KeepCredentials\" as a boolean"));

    bool fHeapEnabled;
    rc = CFGMR3QueryBoolDef(pCfgHandle, "HeapEnabled", &fHeapEnabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed querying \"HeapEnabled\" as a boolean"));

    /*
     * Initialize data (most of it anyway).
     */
    /* Save PDM device instance data for future reference. */
    pThis->pDevIns = pDevIns;

    /* PCI vendor, just a free bogus value */
    pThis->dev.config[0x00] = 0xee;
    pThis->dev.config[0x01] = 0x80;
    /* device ID */
    pThis->dev.config[0x02] = 0xfe;
    pThis->dev.config[0x03] = 0xca;
    /* class sub code (other type of system peripheral) */
    pThis->dev.config[0x0a] = 0x80;
    /* class base code (base system peripheral) */
    pThis->dev.config[0x0b] = 0x08;
    /* header type */
    pThis->dev.config[0x0e] = 0x00;
    /* interrupt on pin 0 */
    pThis->dev.config[0x3d] = 0x01;

    /*
     * Interfaces
     */
    /* Base */
    pThis->Base.pfnQueryInterface         = vmmdevPortQueryInterface;

    /* VMMDev port */
    pThis->Port.pfnQueryAbsoluteMouse     = vmmdevQueryAbsoluteMouse;
    pThis->Port.pfnSetAbsoluteMouse       = vmmdevSetAbsoluteMouse;
    pThis->Port.pfnQueryMouseCapabilities = vmmdevQueryMouseCapabilities;
    pThis->Port.pfnSetMouseCapabilities   = vmmdevSetMouseCapabilities;
    pThis->Port.pfnRequestDisplayChange   = vmmdevRequestDisplayChange;
    pThis->Port.pfnSetCredentials         = vmmdevSetCredentials;
    pThis->Port.pfnVBVAChange             = vmmdevVBVAChange;
    pThis->Port.pfnRequestSeamlessChange  = vmmdevRequestSeamlessChange;
    pThis->Port.pfnSetMemoryBalloon       = vmmdevSetMemoryBalloon;
    pThis->Port.pfnSetStatisticsInterval  = vmmdevSetStatisticsInterval;
    pThis->Port.pfnVRDPChange             = vmmdevVRDPChange;

    /* Shared folder LED */
    pThis->SharedFolders.Led.u32Magic     = PDMLED_MAGIC;
    pThis->SharedFolders.ILeds.pfnQueryStatusLed = vmmdevQueryStatusLed;

#ifdef VBOX_WITH_HGCM
    /* HGCM port */
    pThis->HGCMPort.pfnCompleted          = hgcmCompleted;
#endif

    /** @todo convert this into a config parameter like we do everywhere else! */
    pThis->cbGuestRAM = MMR3PhysGetRamSize(PDMDevHlpGetVM(pDevIns));

    /*
     * Register the backdoor logging port
     */
    rc = PDMDevHlpIOPortRegister(pDevIns, RTLOG_DEBUG_PORT, 1, NULL, vmmdevBackdoorLog, NULL, NULL, NULL, "VMMDev backdoor logging");
    AssertRCReturn(rc, rc);

#ifdef TIMESYNC_BACKDOOR
    /*
     * Alternative timesync source (temporary!)
     */
    rc = PDMDevHlpIOPortRegister(pDevIns, 0x505, 1, NULL, vmmdevTimesyncBackdoorWrite, vmmdevTimesyncBackdoorRead, NULL, NULL, "VMMDev timesync backdoor");
    AssertRCReturn(rc, rc);
#endif

    /*
     * Allocate and initialize the MMIO2 memory.
     */
    rc = PDMDevHlpMMIO2Register(pDevIns, 1 /*iRegion*/, VMMDEV_RAM_SIZE, 0 /*fFlags*/, (void **)&pThis->pVMMDevRAMR3, "VMMDev");
    if (RT_FAILURE(rc))
        return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                   N_("Failed to allocate %u bytes of memory for the VMM device"), VMMDEV_RAM_SIZE);
    vmmdevInitRam(pThis);

    if (fHeapEnabled)
    {
        rc = PDMDevHlpMMIO2Register(pDevIns, 2 /*iRegion*/, VMMDEV_HEAP_SIZE, 0 /*fFlags*/, (void **)&pThis->pVMMDevHeapR3, "VMMDev Heap");
        if (RT_FAILURE(rc))
            return PDMDevHlpVMSetError(pDevIns, rc, RT_SRC_POS,
                                       N_("Failed to allocate %u bytes of memory for the VMM device heap"), PAGE_SIZE);
    }

    /*
     * Register the PCI device.
     */
    rc = PDMDevHlpPCIRegister(pDevIns, &pThis->dev);
    if (RT_FAILURE(rc))
        return rc;
    if (pThis->dev.devfn == 32 || iInstance != 0)
        Log(("!!WARNING!!: pThis->dev.devfn=%d (ignore if testcase or no started by Main)\n", pThis->dev.devfn));
    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 0, 0x20, PCI_ADDRESS_SPACE_IO, vmmdevIOPortRegionMap);
    if (RT_FAILURE(rc))
        return rc;
    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 1, VMMDEV_RAM_SIZE, PCI_ADDRESS_SPACE_MEM, vmmdevIORAMRegionMap);
    if (RT_FAILURE(rc))
        return rc;
    if (fHeapEnabled)
    {
        rc = PDMDevHlpPCIIORegionRegister(pDevIns, 2, VMMDEV_HEAP_SIZE, PCI_ADDRESS_SPACE_MEM_PREFETCH, vmmdevIORAMRegionMap);
        if (RT_FAILURE(rc))
            return rc;
    }

    /*
     * Get the corresponding connector interface
     */
    rc = PDMDevHlpDriverAttach(pDevIns, 0, &pThis->Base, &pThis->pDrvBase, "VMM Driver Port");
    if (RT_SUCCESS(rc))
    {
        pThis->pDrv = (PPDMIVMMDEVCONNECTOR)pThis->pDrvBase->pfnQueryInterface(pThis->pDrvBase, PDMINTERFACE_VMMDEV_CONNECTOR);
        if (!pThis->pDrv)
            AssertMsgFailedReturn(("LUN #0 doesn't have a VMMDev connector interface!\n"), VERR_PDM_MISSING_INTERFACE);
#ifdef VBOX_WITH_HGCM
        pThis->pHGCMDrv = (PPDMIHGCMCONNECTOR)pThis->pDrvBase->pfnQueryInterface(pThis->pDrvBase, PDMINTERFACE_HGCM_CONNECTOR);
        if (!pThis->pHGCMDrv)
        {
            Log(("LUN #0 doesn't have a HGCM connector interface, HGCM is not supported. rc=%Rrc\n", rc));
            /* this is not actually an error, just means that there is no support for HGCM */
        }
#endif
    }
    else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
    {
        Log(("%s/%d: warning: no driver attached to LUN #0!\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
        rc = VINF_SUCCESS;
    }
    else
        AssertMsgFailedReturn(("Failed to attach LUN #0! rc=%Rrc\n", rc), rc);

    /*
     * Attach status driver for shared folders (optional).
     */
    PPDMIBASE pBase;
    rc = PDMDevHlpDriverAttach(pDevIns, PDM_STATUS_LUN, &pThis->Base, &pBase, "Status Port");
    if (RT_SUCCESS(rc))
        pThis->SharedFolders.pLedsConnector = (PPDMILEDCONNECTORS)
            pBase->pfnQueryInterface(pBase, PDMINTERFACE_LED_CONNECTORS);
    else if (rc != VERR_PDM_NO_ATTACHED_DRIVER)
    {
        AssertMsgFailed(("Failed to attach to status driver. rc=%Rrc\n", rc));
        return rc;
    }

    /*
     * Register saved state and init the HGCM CmdList critsect.
     */
    rc = PDMDevHlpSSMRegister(pDevIns, "VMMDev", iInstance, VMMDEV_SSM_VERSION, sizeof(*pThis),
                              NULL, vmmdevSaveState, NULL,
                              NULL, vmmdevLoadState, vmmdevLoadStateDone);
    AssertRCReturn(rc, rc);

#ifdef VBOX_WITH_HGCM
    pThis->pHGCMCmdList = NULL;
    rc = RTCritSectInit(&pThis->critsectHGCMCmdList);
    AssertRCReturn(rc, rc);
    pThis->u32HGCMEnabled = 0;
#endif /* VBOX_WITH_HGCM */

    return rc;
}

/**
 * Reset notification.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 */
static DECLCALLBACK(void) vmmdevReset(PPDMDEVINS pDevIns)
{
    VMMDevState *pThis = PDMINS_2_DATA(pDevIns, VMMDevState*);

    /*
     * Reset the mouse integration feature bit
     */
    if (pThis->mouseCapabilities & (VMMDEV_MOUSEGUESTWANTSABS|VMMDEV_MOUSEGUESTNEEDSHOSTCUR))
    {
        pThis->mouseCapabilities &= ~VMMDEV_MOUSEGUESTWANTSABS;
        /* notify the connector */
        Log(("vmmdevReset: capabilities changed (%x), informing connector\n", pThis->mouseCapabilities));
        pThis->pDrv->pfnUpdateMouseCapabilities(pThis->pDrv, pThis->mouseCapabilities);
    }

    pThis->hypervisorSize = 0;

    pThis->u32HostEventFlags = 0;

    /* re-initialize the VMMDev memory */
    if (pThis->pVMMDevRAMR3)
        vmmdevInitRam(pThis);

    /* credentials have to go away (by default) */
    if (!pThis->fKeepCredentials)
    {
        memset(pThis->credentialsLogon.szUserName, '\0', VMMDEV_CREDENTIALS_STRLEN);
        memset(pThis->credentialsLogon.szPassword, '\0', VMMDEV_CREDENTIALS_STRLEN);
        memset(pThis->credentialsLogon.szDomain, '\0', VMMDEV_CREDENTIALS_STRLEN);
    }
    memset(pThis->credentialsJudge.szUserName, '\0', VMMDEV_CREDENTIALS_STRLEN);
    memset(pThis->credentialsJudge.szPassword, '\0', VMMDEV_CREDENTIALS_STRLEN);
    memset(pThis->credentialsJudge.szDomain, '\0', VMMDEV_CREDENTIALS_STRLEN);

    /* Reset means that additions will report again. */
    const bool fVersionChanged = pThis->fu32AdditionsOk
                              || pThis->guestInfo.additionsVersion
                              || pThis->guestInfo.osType != VBOXOSTYPE_Unknown;
    if (fVersionChanged)
        Log(("vmmdevReset: fu32AdditionsOk=%d additionsVersion=%x osType=%#x\n",
             pThis->fu32AdditionsOk, pThis->guestInfo.additionsVersion, pThis->guestInfo.osType));
    pThis->fu32AdditionsOk = false;
    memset (&pThis->guestInfo, 0, sizeof (pThis->guestInfo));

    /* clear pending display change request. */
    memset (&pThis->lastReadDisplayChangeRequest, 0, sizeof (pThis->lastReadDisplayChangeRequest));

    /* disable seamless mode */
    pThis->fLastSeamlessEnabled = false;

    /* disabled memory ballooning */
    pThis->u32LastMemoryBalloonSize = 0;

    /* disabled statistics updating */
    pThis->u32LastStatIntervalSize = 0;

    /* Clear the "HGCM event enabled" flag so the event can be automatically reenabled.  */
    pThis->u32HGCMEnabled = 0;

    /*
     * Clear the event variables.
     *
     *   Note: The pThis->u32HostEventFlags is not cleared.
     *         It is designed that way so host events do not
     *         depend on guest resets.
     */
    pThis->u32GuestFilterMask    = 0;
    pThis->u32NewGuestFilterMask = 0;
    pThis->fNewGuestFilterMask   = 0;

    /* This is the default, as Windows and OS/2 guests take this for granted. (Actually, neither does...) */
    /** @todo change this when we next bump the interface version */
    const bool fCapsChanged = pThis->guestCaps != VMMDEV_GUEST_SUPPORTS_GRAPHICS;
    if (fCapsChanged)
        Log(("vmmdevReset: fCapsChanged=%#x -> %#x\n", pThis->guestCaps, VMMDEV_GUEST_SUPPORTS_GRAPHICS));
    pThis->guestCaps = VMMDEV_GUEST_SUPPORTS_GRAPHICS; /** @todo r=bird: why? I cannot see this being done at construction?*/

    /*
     * Call the update functions as required.
     */
    if (fVersionChanged)
        pThis->pDrv->pfnUpdateGuestVersion(pThis->pDrv, &pThis->guestInfo);
    if (fCapsChanged)
        pThis->pDrv->pfnUpdateGuestCapabilities(pThis->pDrv, pThis->guestCaps);
}


/**
 * The device registration structure.
 */
extern "C" const PDMDEVREG g_DeviceVMMDev =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szDeviceName */
    "VMMDev",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "VirtualBox VMM Device\n",
    /* fFlags */
    PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT | PDM_DEVREG_FLAGS_GUEST_BITS_32,
    /* fClass */
    PDM_DEVREG_CLASS_VMM_DEV,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(VMMDevState),
    /* pfnConstruct */
    vmmdevConstruct,
    /* pfnDestruct */
    NULL,
    /* pfnRelocate */
    NULL,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    vmmdevReset,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnQueryInterface. */
    NULL,
    /* pfnInitComplete */
    NULL,
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32VersionEnd */
    PDM_DEVREG_VERSION
};
#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */

