/** @file
 *
 * VBox Guest/VMM/host communication:
 * Virtual communication device
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

/* #define LOG_ENABLED */

#include <stdio.h>
#include <string.h>

#include <VBox/VBoxDev.h>
#include <VBox/VBoxGuest.h>
#include <VBox/param.h>
#include <VBox/mm.h>
#include <VBox/pgm.h>
#include <VBox/err.h>

#define LOG_GROUP LOG_GROUP_DEV_VMM
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/time.h>

#include "VMMDevState.h"

#ifdef VBOX_HGCM
#include "VMMDevHGCM.h"
#endif

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
        & pVMMDevState->pVMMDevRAMHC->V.V1_03.u32GuestEventMask;

    Log(("vmmdevSetIRQ: u32EventFlags = 0x%08X, "
         "pVMMDevState->u32HostEventFlags = 0x%08X, "
         "pVMMDevState->pVMMDevRAMHC->u32GuestEventMask = 0x%08X\n",
         u32EventFlags,
         pVMMDevState->u32HostEventFlags,
         pVMMDevState->pVMMDevRAMHC->V.V1_03.u32GuestEventMask));

    /* Move event flags to VMMDev RAM */
    pVMMDevState->pVMMDevRAMHC->V.V1_03.u32HostEvents = u32EventFlags;

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

#ifdef DEBUG_sunlover
    Log(("vmmdevMaybeSetIRQ_EMT: u32HostEventFlags = 0x%08X, u32GuestFilterMask = 0x%08X.\n",
          pVMMDevState->u32HostEventFlags, pVMMDevState->u32GuestFilterMask));
#endif /* DEBUG_sunlover */

    if (pVMMDevState->u32HostEventFlags & pVMMDevState->u32GuestFilterMask)
    {
        pVMMDevState->pVMMDevRAMHC->V.V1_04.fHaveEvents = true;
        PDMDevHlpPCISetIrqNoWait (pDevIns, 0, 1);
#ifdef DEBUG_sunlover
        Log(("vmmdevMaybeSetIRQ_EMT: IRQ set.\n"));
#endif /* DEBUG_sunlover */
    }
}

static void vmmdevNotifyGuest_EMT (VMMDevState *pVMMDevState, uint32_t u32EventMask)
{
#ifdef DEBUG_sunlover
    Log(("VMMDevNotifyGuest_EMT: u32EventMask = 0x%08X.\n", u32EventMask));
#endif /* DEBUG_sunlover */

    if (VBOX_GUEST_ADDITIONS_VERSION_1_03 (pVMMDevState))
    {
#ifdef DEBUG_sunlover
        Log(("VMMDevNotifyGuest_EMT: Old additions detected.\n"));
#endif /* DEBUG_sunlover */

        pVMMDevState->u32HostEventFlags |= u32EventMask;
        vmmdevSetIRQ_Legacy_EMT (pVMMDevState);
    }
    else
    {
#ifdef DEBUG_sunlover
        Log(("VMMDevNotifyGuest_EMT: New additions detected.\n"));
#endif /* DEBUG_sunlover */

        if (!pVMMDevState->fu32AdditionsOk)
        {
            pVMMDevState->u32HostEventFlags |= u32EventMask;
            Log(("vmmdevNotifyGuest_EMT: IRQ is not generated, guest has not yet reported to us.\n"));
            return;
        }

        const bool fHadEvents =
            (pVMMDevState->u32HostEventFlags & pVMMDevState->u32GuestFilterMask) != 0;

#ifdef DEBUG_sunlover
        Log(("VMMDevNotifyGuest_EMT: fHadEvents = %d, u32HostEventFlags = 0x%08X, u32GuestFilterMask = 0x%08X.\n",
              fHadEvents, pVMMDevState->u32HostEventFlags, pVMMDevState->u32GuestFilterMask));
#endif /* DEBUG_sunlover */

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

void VMMDevNotifyGuest (VMMDevState *pVMMDevState, uint32_t u32EventMask)
{
    PPDMDEVINS pDevIns = VMMDEVSTATE_2_DEVINS(pVMMDevState);
    PVM pVM = PDMDevHlpGetVM(pDevIns);
    int rc;
    PVMREQ pReq;

#ifdef DEBUG_sunlover
    Log(("VMMDevNotifyGuest: u32EventMask = 0x%08X.\n", u32EventMask));
#endif /* DEBUG_sunlover */

    rc = VMR3ReqCallVoid (pVM, &pReq, RT_INDEFINITE_WAIT,
                          (PFNRT) vmmdevNotifyGuest_EMT,
                          2, pVMMDevState, u32EventMask);
    AssertReleaseRC (rc);
    VMR3ReqFree (pReq);
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
#undef LOG_GROUP
#define LOG_GROUP LOG_GROUP_DEV_VMM_BACKDOOR

static DECLCALLBACK(int) vmmdevBackdoorLog(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    VMMDevState *pData = PDMINS2DATA(pDevIns, VMMDevState *);

    if (!pData->fBackdoorLogDisabled && cb == 1 && Port == RTLOG_DEBUG_PORT)
    {

        /* The raw version. */
        switch (u32)
        {
            case '\r': Log2(("vmmdev: <return>\n")); break;
            case '\n': Log2(("vmmdev: <newline>\n")); break;
            case '\t': Log2(("vmmdev: <tab>\n")); break;
            default:   Log2(("vmmdev: %c (%02x)\n", u32, u32)); break;
        }

        /* The readable, buffered version. */
        if (u32 == '\n' || u32 == '\r')
        {
            pData->szMsg[pData->iMsg] = '\0';
            if (pData->iMsg)
                LogRel(("Guest Log: %s\n", pData->szMsg));
            pData->iMsg = 0;
        }
        else
        {
            if (pData->iMsg >= sizeof(pData->szMsg)-1)
            {
                pData->szMsg[pData->iMsg] = '\0';
                LogRel(("Guest Log: %s\n", pData->szMsg));
                pData->iMsg = 0;
            }
            pData->szMsg[pData->iMsg] = (char )u32;
            pData->szMsg[++pData->iMsg] = '\0';
        }
    }
    return VINF_SUCCESS;
}
#undef LOG_GROUP
#define LOG_GROUP LOG_GROUP_DEV_VMM

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
        VMMDevState *pData = PDMINS2DATA(pDevIns, VMMDevState *);
        switch (u32)
        {
            case 0:
                pData->fTimesyncBackdoorLo = false;
                break;
            case 1:
                pData->fTimesyncBackdoorLo = true;
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
        VMMDevState *pData = PDMINS2DATA(pDevIns, VMMDevState *);
        RTTIMESPEC now;

        if (pData->fTimesyncBackdoorLo)
        {
            *pu32 = (uint32_t)(pData->hostTime & (uint64_t)0xFFFFFFFF);
        }
        else
        {
            pData->hostTime = RTTimeSpecGetMilli(RTTimeNow(&now));
            *pu32 = (uint32_t)(pData->hostTime >> 32);
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
    VMMDevState *pData = (VMMDevState*)pvUser;
    int rc;

    /*
     * The caller has passed the guest context physical address
     * of the request structure. Get the corresponding host virtual
     * address.
     */
    VMMDevRequestHeader *requestHeader = NULL;
    rc = PDMDevHlpPhys2HCVirt(pDevIns, (RTGCPHYS)u32, 0, (PRTHCPTR)&requestHeader);
    if (VBOX_FAILURE(rc) || !requestHeader)
    {
        AssertMsgFailed(("VMMDev could not convert guest physical address to host virtual! rc = %Vrc\n", rc));
        return VINF_SUCCESS;
    }

    /* the structure size must be greater or equal to the header size */
    if (requestHeader->size < sizeof(VMMDevRequestHeader))
    {
        Log(("VMMDev request header size too small! size = %d\n", requestHeader->size));
        return VINF_SUCCESS;
    }

    /* check the version of the header structure */
    if (requestHeader->version != VMMDEV_REQUEST_HEADER_VERSION)
    {
        Log(("VMMDev: guest header version (0x%08X) differs from ours (0x%08X)\n", requestHeader->version, VMMDEV_REQUEST_HEADER_VERSION));
        return VINF_SUCCESS;
    }

    Log(("VMMDev request issued: %d\n", requestHeader->requestType));

    if (requestHeader->requestType != VMMDevReq_ReportGuestInfo
        && !pData->fu32AdditionsOk)
    {
        Log(("VMMDev: guest has not yet reported to us. Refusing operation.\n"));
        requestHeader->rc = VERR_NOT_SUPPORTED;
        return VINF_SUCCESS;
    }

    /* which request was sent? */
    switch (requestHeader->requestType)
    {
        /*
         * Guest wants to give up a timeslice
         */
        case VMMDevReq_Idle:
        {
            /* just return to EMT telling it that we want to halt */
            return VINF_EM_HALT;
            break;
        }

        /*
         * Guest is reporting its information
         */
        case VMMDevReq_ReportGuestInfo:
        {
            if (requestHeader->size < sizeof(VMMDevReportGuestInfo))
            {
                AssertMsgFailed(("VMMDev guest information structure has invalid size!\n"));
                requestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevReportGuestInfo *guestInfo = (VMMDevReportGuestInfo*)requestHeader;

                if (memcmp (&pData->guestInfo, &guestInfo->guestInfo, sizeof (guestInfo->guestInfo)) != 0)
                {
                    /* make a copy of supplied information */
                    pData->guestInfo = guestInfo->guestInfo;

                    /* Check additions version */
                    pData->fu32AdditionsOk = VBOX_GUEST_ADDITIONS_VERSION_OK(pData->guestInfo.additionsVersion);

                    LogRel(("Guest Additions information report:\t"
                            "    additionsVersion = 0x%08X\t"
                            "    osType = 0x%08X\n",
                            pData->guestInfo.additionsVersion,
                            pData->guestInfo.osType));
                    pData->pDrv->pfnUpdateGuestVersion(pData->pDrv, &pData->guestInfo);
                }

                if (pData->fu32AdditionsOk)
                {
                    requestHeader->rc = VINF_SUCCESS;
                }
                else
                {
                    requestHeader->rc = VERR_VERSION_MISMATCH;
                }
            }
            break;
        }

        /*
         * Retrieve mouse information
         */
        case VMMDevReq_GetMouseStatus:
        {
            if (requestHeader->size != sizeof(VMMDevReqMouseStatus))
            {
                AssertMsgFailed(("VMMDev mouse status structure has invalid size!\n"));
                requestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevReqMouseStatus *mouseStatus = (VMMDevReqMouseStatus*)requestHeader;
                mouseStatus->mouseFeatures = 0;
                if (pData->mouseCapabilities & VMMDEV_MOUSEHOSTWANTSABS)
                {
                    mouseStatus->mouseFeatures |= VBOXGUEST_MOUSE_HOST_CAN_ABSOLUTE;
                }
                if (pData->mouseCapabilities & VMMDEV_MOUSEGUESTWANTSABS)
                {
                    mouseStatus->mouseFeatures |= VBOXGUEST_MOUSE_GUEST_CAN_ABSOLUTE;
                }
                if (pData->mouseCapabilities & VMMDEV_MOUSEHOSTCANNOTHWPOINTER)
                {
                    mouseStatus->mouseFeatures |= VBOXGUEST_MOUSE_HOST_CANNOT_HWPOINTER;
                }
                mouseStatus->pointerXPos = pData->mouseXAbs;
                mouseStatus->pointerYPos = pData->mouseYAbs;
                Log(("returning mouse status: features = %d, absX = %d, absY = %d\n", mouseStatus->mouseFeatures,
                      mouseStatus->pointerXPos, mouseStatus->pointerYPos));
                requestHeader->rc = VINF_SUCCESS;
            }
            break;
        }

        /*
         * Set mouse information
         */
        case VMMDevReq_SetMouseStatus:
        {
            if (requestHeader->size != sizeof(VMMDevReqMouseStatus))
            {
                AssertMsgFailed(("VMMDev mouse status structure has invalid size %d (%#x) version=%d!\n",
                                 requestHeader->size, requestHeader->size, requestHeader->size, requestHeader->version));
                requestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                bool bCapsChanged = false;

                VMMDevReqMouseStatus *mouseStatus = (VMMDevReqMouseStatus*)requestHeader;

                /* check if the guest wants absolute coordinates */
                if (mouseStatus->mouseFeatures & VBOXGUEST_MOUSE_GUEST_CAN_ABSOLUTE)
                {
                    /* set the capability flag and the changed flag if it's actually a change */
                    if (!(pData->mouseCapabilities & VMMDEV_MOUSEGUESTWANTSABS))
                    {
                        pData->mouseCapabilities |= VMMDEV_MOUSEGUESTWANTSABS;
                        bCapsChanged = true;
                        LogRel(("Guest requests mouse pointer integration\n"));
                    }
                } else
                {
                    if (pData->mouseCapabilities & VMMDEV_MOUSEGUESTWANTSABS)
                    {
                        pData->mouseCapabilities &= ~VMMDEV_MOUSEGUESTWANTSABS;
                        bCapsChanged = true;
                        LogRel(("Guest disables mouse pointer integration\n"));
                    }
                }
                if (mouseStatus->mouseFeatures & VBOXGUEST_MOUSE_GUEST_NEEDS_HOST_CURSOR)
                    pData->mouseCapabilities |= VMMDEV_MOUSEGUESTNEEDSHOSTCUR;
                else
                    pData->mouseCapabilities &= ~VMMDEV_MOUSEGUESTNEEDSHOSTCUR;

                /*
                 * Notify connector if something has changed
                 */
                if (bCapsChanged)
                {
                    Log(("VMMDevReq_SetMouseStatus: capabilities changed (%x), informing connector\n", pData->mouseCapabilities));
                    pData->pDrv->pfnUpdateMouseCapabilities(pData->pDrv, pData->mouseCapabilities);
                }
                requestHeader->rc = VINF_SUCCESS;
            }

            break;
        }

        /*
         * Set a new mouse pointer shape
         */
        case VMMDevReq_SetPointerShape:
        {
            if (requestHeader->size < sizeof(VMMDevReqMousePointer))
            {
                AssertMsg(requestHeader->size == 0x10028 && requestHeader->version == 10000,  /* don't bitch about legacy!!! */
                          ("VMMDev mouse shape structure has invalid size %d (%#x) version=%d!\n",
                           requestHeader->size, requestHeader->size, requestHeader->size, requestHeader->version));
                requestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevReqMousePointer *pointerShape = (VMMDevReqMousePointer*)requestHeader;

                bool fVisible = (pointerShape->fFlags & VBOX_MOUSE_POINTER_VISIBLE) != 0;
                bool fAlpha = (pointerShape->fFlags & VBOX_MOUSE_POINTER_ALPHA) != 0;
                bool fShape = (pointerShape->fFlags & VBOX_MOUSE_POINTER_SHAPE) != 0;

                Log(("VMMDevReq_SetPointerShape: visible: %d, alpha: %d, shape = %d, width: %d, height: %d\n",
                     fVisible, fAlpha, fShape, pointerShape->width, pointerShape->height));

                /* forward call to driver */
                if (fShape)
                {
                    pData->pDrv->pfnUpdatePointerShape(pData->pDrv,
                                                       fVisible,
                                                       fAlpha,
                                                       pointerShape->xHot, pointerShape->yHot,
                                                       pointerShape->width, pointerShape->height,
                                                       pointerShape->pointerData);
                }
                else
                {
                    pData->pDrv->pfnUpdatePointerShape(pData->pDrv,
                                                       fVisible,
                                                       0,
                                                       0, 0,
                                                       0, 0,
                                                       NULL);
                }
                requestHeader->rc = VINF_SUCCESS;
            }
            break;
        }

        /*
         * Query the system time from the host
         */
        case VMMDevReq_GetHostTime:
        {
            if (requestHeader->size != sizeof(VMMDevReqHostTime))
            {
                AssertMsgFailed(("VMMDev host time structure has invalid size!\n"));
                requestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else if (RT_UNLIKELY(pData->fGetHostTimeDisabled))
                requestHeader->rc = VERR_NOT_SUPPORTED;
            else
            {
                VMMDevReqHostTime *hostTimeReq = (VMMDevReqHostTime*)requestHeader;
                RTTIMESPEC now;
                hostTimeReq->time = RTTimeSpecGetMilli(RTTimeNow(&now));
                requestHeader->rc = VINF_SUCCESS;
            }
            break;
        }

        /*
         * Query information about the hypervisor
         */
        case VMMDevReq_GetHypervisorInfo:
        {
            if (requestHeader->size != sizeof(VMMDevReqHypervisorInfo))
            {
                AssertMsgFailed(("VMMDev hypervisor info structure has invalid size!\n"));
                requestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevReqHypervisorInfo *hypervisorInfo = (VMMDevReqHypervisorInfo*)requestHeader;
                PVM pVM = PDMDevHlpGetVM(pDevIns);
                size_t hypervisorSize = 0;
                requestHeader->rc = PGMR3MappingsSize(pVM, &hypervisorSize);
                hypervisorInfo->hypervisorSize = (uint32_t)hypervisorSize;
                Assert(hypervisorInfo->hypervisorSize == hypervisorSize);
            }
            break;
        }

        /*
         * Set hypervisor information
         */
        case VMMDevReq_SetHypervisorInfo:
        {
            if (requestHeader->size != sizeof(VMMDevReqHypervisorInfo))
            {
                AssertMsgFailed(("VMMDev hypervisor info structure has invalid size!\n"));
                requestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevReqHypervisorInfo *hypervisorInfo = (VMMDevReqHypervisorInfo*)requestHeader;
                PVM pVM = PDMDevHlpGetVM(pDevIns);
                if (hypervisorInfo->hypervisorStart == 0)
                {
                    requestHeader->rc = PGMR3MappingsUnfix(pVM);
                } else
                {
                    /* only if the client has queried the size before! */
                    size_t mappingsSize;
                    requestHeader->rc = PGMR3MappingsSize(pVM, &mappingsSize);
                    if (VBOX_SUCCESS(requestHeader->rc) && (hypervisorInfo->hypervisorSize == mappingsSize))
                    {
                        /* new reservation */
                        requestHeader->rc = PGMR3MappingsFix(pVM, hypervisorInfo->hypervisorStart,
                                                             hypervisorInfo->hypervisorSize);
                        LogRel(("Guest reported fixed hypervisor window at 0x%p (size = 0x%x, rc = %Vrc)\n",
                                hypervisorInfo->hypervisorStart,
                                hypervisorInfo->hypervisorSize,
                                requestHeader->rc));
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
            if (requestHeader->size != sizeof(VMMDevPowerStateRequest))
            {
                AssertMsgFailed(("VMMDev power state request structure has invalid size!\n"));
                requestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevPowerStateRequest *powerStateRequest = (VMMDevPowerStateRequest*)requestHeader;
                switch(powerStateRequest->powerState)
                {
                    case VMMDevPowerState_Pause:
                    {
                        LogRel(("Guest requests the VM to be suspended (paused)\n"));
                        requestHeader->rc = PDMDevHlpVMSuspend(pDevIns);
                        break;
                    }

                    case VMMDevPowerState_PowerOff:
                    {
                        LogRel(("Guest requests the VM to be turned off\n"));
                        requestHeader->rc = PDMDevHlpVMPowerOff(pDevIns); /** @todo pass the rc around! */
                        break;
                    }

                    case VMMDevPowerState_SaveState:
                    {
                        /** @todo no API for that yet */
                        requestHeader->rc = VERR_NOT_IMPLEMENTED;
                        break;
                    }

                    default:
                        AssertMsgFailed(("VMMDev invalid power state request: %d\n", powerStateRequest->powerState));
                        requestHeader->rc = VERR_INVALID_PARAMETER;
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
            if (requestHeader->size != sizeof(VMMDevDisplayChangeRequest))
            {
                /* Assert only if the size also not equal to a previous version size to prevent
                 * assertion with old additions.
                 */
                AssertMsg(requestHeader->size == sizeof(VMMDevDisplayChangeRequest) - sizeof (uint32_t),
                          ("VMMDev display change request structure has invalid size!\n"));
                requestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevDisplayChangeRequest *displayChangeRequest = (VMMDevDisplayChangeRequest*)requestHeader;
                /* just pass on the information */
                Log(("VMMDev: returning display change request xres = %d, yres = %d, bpp = %d\n",
                     pData->displayChangeRequest.xres, pData->displayChangeRequest.yres, pData->displayChangeRequest.bpp));
                displayChangeRequest->xres = pData->displayChangeRequest.xres;
                displayChangeRequest->yres = pData->displayChangeRequest.yres;
                displayChangeRequest->bpp  = pData->displayChangeRequest.bpp;

                if (displayChangeRequest->eventAck == VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST)
                {
                    /* Remember which resolution the client have queried. */
                    pData->lastReadDisplayChangeRequest = pData->displayChangeRequest;
                }

                requestHeader->rc = VINF_SUCCESS;
            }
            break;
        }

        /*
         * Query whether the given video mode is supported
         */
        case VMMDevReq_VideoModeSupported:
        {
            if (requestHeader->size != sizeof(VMMDevVideoModeSupportedRequest))
            {
                AssertMsgFailed(("VMMDev video mode supported request structure has invalid size!\n"));
                requestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevVideoModeSupportedRequest *videoModeSupportedRequest = (VMMDevVideoModeSupportedRequest*)requestHeader;
                /* forward the call */
                requestHeader->rc = pData->pDrv->pfnVideoModeSupported(pData->pDrv,
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
            if (requestHeader->size != sizeof(VMMDevGetHeightReductionRequest))
            {
                AssertMsgFailed(("VMMDev height reduction request structure has invalid size!\n"));
                requestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevGetHeightReductionRequest *heightReductionRequest = (VMMDevGetHeightReductionRequest*)requestHeader;
                /* forward the call */
                requestHeader->rc = pData->pDrv->pfnGetHeightReduction(pData->pDrv,
                                                                       &heightReductionRequest->heightReduction);
            }
            break;
        }

        /*
         * Acknowledge VMMDev events
         */
        case VMMDevReq_AcknowledgeEvents:
        {
            if (requestHeader->size != sizeof(VMMDevEvents))
            {
                AssertMsgFailed(("VMMDevReq_AcknowledgeEvents structure has invalid size!\n"));
                requestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                if (VBOX_GUEST_ADDITIONS_VERSION_1_03 (pData))
                {
                    vmmdevSetIRQ_Legacy_EMT (pData);
                }
                else
                {
                    VMMDevEvents *pAckRequest;

                    if (pData->fNewGuestFilterMask)
                    {
                        pData->fNewGuestFilterMask = false;
                        pData->u32GuestFilterMask = pData->u32NewGuestFilterMask;
                    }

                    pAckRequest = (VMMDevEvents *) requestHeader;
                    pAckRequest->events =
                        pData->u32HostEventFlags & pData->u32GuestFilterMask;

                    pData->u32HostEventFlags &= ~pData->u32GuestFilterMask;
                    pData->pVMMDevRAMHC->V.V1_04.fHaveEvents = false;
                    PDMDevHlpPCISetIrqNoWait (pData->pDevIns, 0, 0);
                }
                requestHeader->rc = VINF_SUCCESS;
            }
            break;
        }

        /*
         * Change guest filter mask
         */
        case VMMDevReq_CtlGuestFilterMask:
        {
            if (requestHeader->size != sizeof(VMMDevCtlGuestFilterMask))
            {
                AssertMsgFailed(("VMMDevReq_AcknowledgeEvents structure has invalid size!\n"));
                requestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevCtlGuestFilterMask *pCtlMaskRequest;

                pCtlMaskRequest = (VMMDevCtlGuestFilterMask *) requestHeader;
                vmmdevCtlGuestFilterMask_EMT (pData,
                                              pCtlMaskRequest->u32OrMask,
                                              pCtlMaskRequest->u32NotMask);
                requestHeader->rc = VINF_SUCCESS;

            }
            break;
        }

#ifdef VBOX_HGCM
        /*
         * Process HGCM request
         */
        case VMMDevReq_HGCMConnect:
        {
            if (requestHeader->size < sizeof(VMMDevHGCMConnect))
            {
                AssertMsgFailed(("VMMDevReq_HGCMConnect structure has invalid size!\n"));
                requestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else if (!pData->pHGCMDrv)
            {
                Log(("VMMDevReq_HGCMConnect HGCM Connector is NULL!\n"));
                requestHeader->rc = VERR_NOT_SUPPORTED;
            }
            else
            {
                VMMDevHGCMConnect *pHGCMConnect = (VMMDevHGCMConnect *)requestHeader;

                Log(("VMMDevReq_HGCMConnect\n"));

                requestHeader->rc = vmmdevHGCMConnect (pData, pHGCMConnect, (RTGCPHYS)u32);
            }
            break;
        }

        case VMMDevReq_HGCMDisconnect:
        {
            if (requestHeader->size < sizeof(VMMDevHGCMDisconnect))
            {
                AssertMsgFailed(("VMMDevReq_HGCMDisconnect structure has invalid size!\n"));
                requestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else if (!pData->pHGCMDrv)
            {
                Log(("VMMDevReq_HGCMDisconnect HGCM Connector is NULL!\n"));
                requestHeader->rc = VERR_NOT_SUPPORTED;
            }
            else
            {
                VMMDevHGCMDisconnect *pHGCMDisconnect = (VMMDevHGCMDisconnect *)requestHeader;

                Log(("VMMDevReq_VMMDevHGCMDisconnect\n"));
                requestHeader->rc = vmmdevHGCMDisconnect (pData, pHGCMDisconnect, (RTGCPHYS)u32);
            }
            break;
        }

        case VMMDevReq_HGCMCall:
        {
            if (requestHeader->size < sizeof(VMMDevHGCMCall))
            {
                AssertMsgFailed(("VMMDevReq_HGCMCall structure has invalid size!\n"));
                requestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else if (!pData->pHGCMDrv)
            {
                Log(("VMMDevReq_HGCMCall HGCM Connector is NULL!\n"));
                requestHeader->rc = VERR_NOT_SUPPORTED;
            }
            else
            {
                VMMDevHGCMCall *pHGCMCall = (VMMDevHGCMCall *)requestHeader;

                Log(("VMMDevReq_HGCMCall: sizeof (VMMDevHGCMRequest) = %04X\n", sizeof (VMMDevHGCMCall)));

                Log(("%.*Vhxd\n", requestHeader->size, requestHeader));

                requestHeader->rc = vmmdevHGCMCall (pData, pHGCMCall, (RTGCPHYS)u32);
            }
            break;
        }
#endif /* VBOX_HGCM */

        case VMMDevReq_VideoAccelEnable:
        {
            if (requestHeader->size < sizeof(VMMDevVideoAccelEnable))
            {
                Log(("VMMDevReq_VideoAccelEnable request size too small!!!\n"));
                requestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else if (!pData->pDrv)
            {
                Log(("VMMDevReq_VideoAccelEnable Connector is NULL!!!\n"));
                requestHeader->rc = VERR_NOT_SUPPORTED;
            }
            else
            {
                VMMDevVideoAccelEnable *ptr = (VMMDevVideoAccelEnable *)requestHeader;

                if (ptr->cbRingBuffer != VBVA_RING_BUFFER_SIZE)
                {
                    /* The guest driver seems compiled with another headers. */
                    Log(("VMMDevReq_VideoAccelEnable guest ring buffer size %d, should be %d!!!\n", ptr->cbRingBuffer, VBVA_RING_BUFFER_SIZE));
                    requestHeader->rc = VERR_INVALID_PARAMETER;
                }
                else
                {
                    /* The request is correct. */
                    ptr->fu32Status |= VBVA_F_STATUS_ACCEPTED;

                    LogFlow(("VMMDevReq_VideoAccelEnable ptr->u32Enable = %d\n", ptr->u32Enable));

                    requestHeader->rc = ptr->u32Enable?
                        pData->pDrv->pfnVideoAccelEnable (pData->pDrv, true, &pData->pVMMDevRAMHC->vbvaMemory):
                        pData->pDrv->pfnVideoAccelEnable (pData->pDrv, false, NULL);

                    if (   ptr->u32Enable
                        && VBOX_SUCCESS (requestHeader->rc))
                    {
                        ptr->fu32Status |= VBVA_F_STATUS_ENABLED;

                        /* Remember that guest successfully enabled acceleration.
                         * We need to reestablish it on restoring the VM from saved state.
                         */
                        pData->u32VideoAccelEnabled = 1;
                    }
                    else
                    {
                        /* The acceleration was not enabled. Remember that. */
                        pData->u32VideoAccelEnabled = 0;
                    }
                }
            }
            break;
        }

        case VMMDevReq_VideoAccelFlush:
        {
            if (requestHeader->size < sizeof(VMMDevVideoAccelFlush))
            {
                AssertMsgFailed(("VMMDevReq_VideoAccelFlush request size too small.\n"));
                requestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else if (!pData->pDrv)
            {
                Log(("VMMDevReq_VideoAccelFlush Connector is NULL!\n"));
                requestHeader->rc = VERR_NOT_SUPPORTED;
            }
            else
            {
                pData->pDrv->pfnVideoAccelFlush (pData->pDrv);

                requestHeader->rc = VINF_SUCCESS;
            }
            break;
        }

        case VMMDevReq_QueryCredentials:
        {
            if (requestHeader->size != sizeof(VMMDevCredentials))
            {
                AssertMsgFailed(("VMMDevReq_QueryCredentials request size too small.\n"));
                requestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevCredentials *credentials = (VMMDevCredentials*)requestHeader;

                /* let's start by nulling out the data */
                memset(credentials->szUserName, '\0', VMMDEV_CREDENTIALS_STRLEN);
                memset(credentials->szPassword, '\0', VMMDEV_CREDENTIALS_STRLEN);
                memset(credentials->szDomain, '\0', VMMDEV_CREDENTIALS_STRLEN);

                /* should we return whether we got credentials for a logon? */
                if (credentials->u32Flags & VMMDEV_CREDENTIALS_QUERYPRESENCE)
                {
                    if (   pData->credentialsLogon.szUserName[0]
                        || pData->credentialsLogon.szPassword[0]
                        || pData->credentialsLogon.szDomain[0])
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
                    if (pData->credentialsLogon.szUserName[0])
                        strcpy(credentials->szUserName, pData->credentialsLogon.szUserName);
                    if (pData->credentialsLogon.szPassword[0])
                        strcpy(credentials->szPassword, pData->credentialsLogon.szPassword);
                    if (pData->credentialsLogon.szDomain[0])
                        strcpy(credentials->szDomain, pData->credentialsLogon.szDomain);
                    if (!pData->credentialsLogon.fAllowInteractiveLogon)
                        credentials->u32Flags |= VMMDEV_CREDENTIALS_NOLOCALLOGON;
                    else
                        credentials->u32Flags &= ~VMMDEV_CREDENTIALS_NOLOCALLOGON;
                }

                /* does the caller want us to destroy the logon credentials? */
                if (credentials->u32Flags & VMMDEV_CREDENTIALS_CLEAR)
                {
                    memset(pData->credentialsLogon.szUserName, '\0', VMMDEV_CREDENTIALS_STRLEN);
                    memset(pData->credentialsLogon.szPassword, '\0', VMMDEV_CREDENTIALS_STRLEN);
                    memset(pData->credentialsLogon.szDomain, '\0', VMMDEV_CREDENTIALS_STRLEN);
                }

                /* does the guest want to read credentials for verification? */
                if (credentials->u32Flags & VMMDEV_CREDENTIALS_READJUDGE)
                {
                    if (pData->credentialsJudge.szUserName[0])
                        strcpy(credentials->szUserName, pData->credentialsJudge.szUserName);
                    if (pData->credentialsJudge.szPassword[0])
                        strcpy(credentials->szPassword, pData->credentialsJudge.szPassword);
                    if (pData->credentialsJudge.szDomain[0])
                        strcpy(credentials->szDomain, pData->credentialsJudge.szDomain);
                }

                /* does the caller want us to destroy the judgement credentials? */
                if (credentials->u32Flags & VMMDEV_CREDENTIALS_CLEARJUDGE)
                {
                    memset(pData->credentialsJudge.szUserName, '\0', VMMDEV_CREDENTIALS_STRLEN);
                    memset(pData->credentialsJudge.szPassword, '\0', VMMDEV_CREDENTIALS_STRLEN);
                    memset(pData->credentialsJudge.szDomain, '\0', VMMDEV_CREDENTIALS_STRLEN);
                }

                requestHeader->rc = VINF_SUCCESS;
            }
            break;
        }

        case VMMDevReq_ReportCredentialsJudgement:
        {
            if (requestHeader->size != sizeof(VMMDevCredentials))
            {
                AssertMsgFailed(("VMMDevReq_ReportCredentialsJudgement request size too small.\n"));
                requestHeader->rc = VERR_INVALID_PARAMETER;
            }
            else
            {
                VMMDevCredentials *credentials = (VMMDevCredentials*)requestHeader;

                /* what does the guest think about the credentials? (note: the order is important here!) */
                if (credentials->u32Flags & VMMDEV_CREDENTIALS_JUDGE_DENY)
                {
                    pData->pDrv->pfnSetCredentialsJudgementResult(pData->pDrv, VMMDEV_CREDENTIALS_JUDGE_DENY);
                }
                else if (credentials->u32Flags & VMMDEV_CREDENTIALS_JUDGE_NOJUDGEMENT)
                {
                    pData->pDrv->pfnSetCredentialsJudgementResult(pData->pDrv, VMMDEV_CREDENTIALS_JUDGE_NOJUDGEMENT);
                }
                else if (credentials->u32Flags & VMMDEV_CREDENTIALS_JUDGE_OK)
                {
                    pData->pDrv->pfnSetCredentialsJudgementResult(pData->pDrv, VMMDEV_CREDENTIALS_JUDGE_OK);
                }
                else
                    Log(("VMMDevReq_ReportCredentialsJudgement: invalid flags: %d!!!\n", credentials->u32Flags));

                requestHeader->rc = VINF_SUCCESS;
            }
            break;
        }

        default:
        {
            requestHeader->rc = VERR_NOT_IMPLEMENTED;

            Log(("VMMDev unknown request type %d\n", requestHeader->requestType));

            break;
        }
    }

    return VINF_SUCCESS;
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
    int         rc;
    VMMDevState *pData = PCIDEV_2_VMMDEVSTATE(pPciDev);
    LogFlow(("vmmdevR3IORAMRegionMap: iRegion=%d GCPhysAddress=%VGp cb=%#x enmType=%d\n", iRegion, GCPhysAddress, cb, enmType));


    Assert(pData->pVMMDevRAMHC != NULL);

    memset (pData->pVMMDevRAMHC, 0, sizeof (VMMDevMemory));
    pData->pVMMDevRAMHC->u32Size = sizeof (VMMDevMemory);
    pData->pVMMDevRAMHC->u32Version = VMMDEV_MEMORY_VERSION;

    /*
     * VMMDev RAM mapping.
     */
    if (iRegion == 1 && enmType == PCI_ADDRESS_SPACE_MEM)
    {
        /*
         * Register and lock the RAM.
         *
         * Windows usually re-initializes the PCI devices, so we have to check whether the memory was
         * already registered before trying to do that all over again.
         */
        PVM pVM = PDMDevHlpGetVM(pPciDev->pDevIns);

        if (pData->GCPhysVMMDevRAM)
        {
            /*
             * Relocate the already registered VMMDevRAM.
             */
            rc = MMR3PhysRelocate(pVM, pData->GCPhysVMMDevRAM, GCPhysAddress, VMMDEV_RAM_SIZE);
            if (VBOX_SUCCESS(rc))
            {
                pData->GCPhysVMMDevRAM = GCPhysAddress;
                return VINF_SUCCESS;
            }
            AssertReleaseMsgFailed(("Failed to relocate VMMDev RAM from %VGp to %VGp! rc=%Vra\n", pData->GCPhysVMMDevRAM, GCPhysAddress, rc));
        }
        else
        {
            /*
             * Register and lock the VMMDevRAM.
             */
            /** @todo MM_RAM_FLAGS_MMIO2 seems to be appropriate for a RW memory.
             * Need to check. May be a RO memory is enough for the device.
             */
            rc = MMR3PhysRegister(pVM, pData->pVMMDevRAMHC, GCPhysAddress, VMMDEV_RAM_SIZE, MM_RAM_FLAGS_MMIO2, "VBoxDev");
            if (VBOX_SUCCESS(rc))
            {
                pData->GCPhysVMMDevRAM = GCPhysAddress;
                return VINF_SUCCESS;
            }
            AssertReleaseMsgFailed(("Failed to register VMMDev RAM! rc=%Vra\n", rc));
        }
        return rc;
    }

    AssertReleaseMsgFailed(("VMMDev wrong region type: iRegion=%d enmType=%d\n", iRegion, enmType));
    return VERR_INTERNAL_ERROR;
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
    VMMDevState *pData = PCIDEV_2_VMMDEVSTATE(pPciDev);
    int         rc = VINF_SUCCESS;

    Assert(enmType == PCI_ADDRESS_SPACE_IO);
    Assert(iRegion == 0);
    AssertMsg(RT_ALIGN(GCPhysAddress, 8) == GCPhysAddress, ("Expected 8 byte alignment. GCPhysAddress=%#x\n", GCPhysAddress));

    /*
     * Save the base port address to simplify Port offset calculations.
     */
    pData->PortBase = (RTIOPORT)GCPhysAddress;

    /*
     * Register our port IO handlers.
     */
    rc = PDMDevHlpIOPortRegister(pPciDev->pDevIns,
                                 (RTIOPORT)GCPhysAddress + PORT_VMMDEV_REQUEST_OFFSET, 1,
                                 (void*)pData, vmmdevRequestHandler,
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
    VMMDevState *pData = (VMMDevState*)((uintptr_t)pInterface - RT_OFFSETOF(VMMDevState, Base));
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pData->Base;
        case PDMINTERFACE_VMMDEV_PORT:
            return &pData->Port;
#ifdef VBOX_HGCM
        case PDMINTERFACE_HGCM_PORT:
            return &pData->HGCMPort;
#endif
        default:
            return NULL;
    }
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
    VMMDevState *pData = IVMMDEVPORT_2_VMMDEVSTATE(pInterface);
    if (pAbsX)
        *pAbsX = pData->mouseXAbs;
    if (pAbsY)
        *pAbsY = pData->mouseYAbs;
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
    VMMDevState *pData = IVMMDEVPORT_2_VMMDEVSTATE(pInterface);
    Log(("vmmdevSetAbsoluteMouse: settings absolute position to x = %d, y = %d\n", absX, absY));
    pData->mouseXAbs = absX;
    pData->mouseYAbs = absY;
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
    VMMDevState *pData = IVMMDEVPORT_2_VMMDEVSTATE(pInterface);
    if (!pCapabilities)
        return VERR_INVALID_PARAMETER;
    *pCapabilities = pData->mouseCapabilities;
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
    VMMDevState *pData = IVMMDEVPORT_2_VMMDEVSTATE(pInterface);

    bool bCapsChanged = ((capabilities & VMMDEV_MOUSEHOSTWANTSABS)
                         != (pData->mouseCapabilities & VMMDEV_MOUSEHOSTWANTSABS));

    Log(("vmmdevSetMouseCapabilities: bCapsChanged %d\n", bCapsChanged));

    if (capabilities & VMMDEV_MOUSEHOSTCANNOTHWPOINTER)
        pData->mouseCapabilities |= VMMDEV_MOUSEHOSTCANNOTHWPOINTER;
    else
        pData->mouseCapabilities &= ~VMMDEV_MOUSEHOSTCANNOTHWPOINTER;

    if (capabilities & VMMDEV_MOUSEHOSTWANTSABS)
        pData->mouseCapabilities |= VMMDEV_MOUSEHOSTWANTSABS;
    else
        pData->mouseCapabilities &= ~VMMDEV_MOUSEHOSTWANTSABS;

    if (bCapsChanged)
        VMMDevNotifyGuest (pData, VMMDEV_EVENT_MOUSE_CAPABILITIES_CHANGED);

    return VINF_SUCCESS;
}


static DECLCALLBACK(int) vmmdevRequestDisplayChange(PPDMIVMMDEVPORT pInterface, uint32_t xres, uint32_t yres, uint32_t bpp)
{
    VMMDevState *pData = IVMMDEVPORT_2_VMMDEVSTATE(pInterface);

    /* Verify that the new resolution is different and that guest does not yet know about it. */
    bool fSameResolution = (!xres || (pData->lastReadDisplayChangeRequest.xres == xres)) &&
                           (!yres || (pData->lastReadDisplayChangeRequest.yres == yres)) &&
                           (!bpp || (pData->lastReadDisplayChangeRequest.bpp == bpp));

    if (!xres && !yres && !bpp)
    {
        /* Special case of reset video mode. */
        fSameResolution = false;
    }

#ifdef DEBUG_sunlover
    Log(("vmmdevRequestDisplayChange: same=%d. new: xres=%d, yres=%d, bpp=%d. old: xres=%d, yres=%d, bpp=%d.\n",
          fSameResolution, xres, yres, bpp, pData->lastReadDisplayChangeRequest.xres, pData->lastReadDisplayChangeRequest.yres, pData->lastReadDisplayChangeRequest.bpp));
#endif /* DEBUG_sunlover */

    if (!fSameResolution)
    {
        LogRel(("VMMDev::SetVideoModeHint: got a video mode hint (%dx%dx%d)\n",
                xres, yres, bpp));

        /* we could validate the information here but hey, the guest can do that as well! */
        pData->displayChangeRequest.xres = xres;
        pData->displayChangeRequest.yres = yres;
        pData->displayChangeRequest.bpp  = bpp;

        /* IRQ so the guest knows what's going on */
        VMMDevNotifyGuest (pData, VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST);
    }

    return VINF_SUCCESS;
}

static DECLCALLBACK(int) vmmdevSetCredentials(PPDMIVMMDEVPORT pInterface, const char *pszUsername,
                                              const char *pszPassword, const char *pszDomain,
                                              uint32_t u32Flags)
{
    VMMDevState *pData = IVMMDEVPORT_2_VMMDEVSTATE(pInterface);

    /* logon mode? */
    if (u32Flags & VMMDEV_SETCREDENTIALS_GUESTLOGON)
    {
        /* memorize the data */
        strcpy(pData->credentialsLogon.szUserName, pszUsername);
        strcpy(pData->credentialsLogon.szPassword, pszPassword);
        strcpy(pData->credentialsLogon.szDomain,   pszDomain);
        pData->credentialsLogon.fAllowInteractiveLogon = !(u32Flags & VMMDEV_SETCREDENTIALS_NOLOCALLOGON);
    }
    /* credentials verification mode? */
    else if (u32Flags & VMMDEV_SETCREDENTIALS_JUDGE)
    {
        /* memorize the data */
        strcpy(pData->credentialsJudge.szUserName, pszUsername);
        strcpy(pData->credentialsJudge.szPassword, pszPassword);
        strcpy(pData->credentialsJudge.szDomain,   pszDomain);

        VMMDevNotifyGuest (pData, VMMDEV_EVENT_JUDGE_CREDENTIALS);
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
    VMMDevState *pData = IVMMDEVPORT_2_VMMDEVSTATE(pInterface);

    Log(("vmmdevVBVAChange: fEnabled = %d\n", fEnabled));

    if (pData)
    {
        pData->u32VideoAccelEnabled = fEnabled;
    }

    return;
}


/* -=-=-=-=-=- IHGCMPort -=-=-=-=-=- */

/** Converts a VMMDev port interface pointer to a VMMDev state pointer. */
#define IHGCMPORT_2_VMMDEVSTATE(pInterface) ( (VMMDevState*)((uintptr_t)pInterface - RT_OFFSETOF(VMMDevState, HGCMPort)) )



#define VMMDEV_SSM_VERSION 3

/**
 * Saves a state of the VMM device.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSMHandle  The handle to save the state to.
 */
static DECLCALLBACK(int) vmmdevSaveState(PPDMDEVINS pDevIns, PSSMHANDLE pSSMHandle)
{
    VMMDevState *pData = PDMINS2DATA(pDevIns, VMMDevState*);
    SSMR3PutU32(pSSMHandle, pData->hypervisorSize);
    SSMR3PutU32(pSSMHandle, pData->mouseCapabilities);
    SSMR3PutU32(pSSMHandle, pData->mouseXAbs);
    SSMR3PutU32(pSSMHandle, pData->mouseYAbs);

    SSMR3PutBool(pSSMHandle, pData->fNewGuestFilterMask);
    SSMR3PutU32(pSSMHandle, pData->u32NewGuestFilterMask);
    SSMR3PutU32(pSSMHandle, pData->u32GuestFilterMask);
    SSMR3PutU32(pSSMHandle, pData->u32HostEventFlags);
    // here be dragons (probably)
//    SSMR3PutBool(pSSMHandle, pData->pVMMDevRAMHC->V.V1_04.fHaveEvents);
    SSMR3PutMem(pSSMHandle, &pData->pVMMDevRAMHC->V, sizeof (pData->pVMMDevRAMHC->V));

    SSMR3PutMem(pSSMHandle, &pData->guestInfo, sizeof (pData->guestInfo));
    SSMR3PutU32(pSSMHandle, pData->fu32AdditionsOk);
    SSMR3PutU32(pSSMHandle, pData->u32VideoAccelEnabled);

#ifdef VBOX_HGCM
    vmmdevHGCMSaveState (pData, pSSMHandle);
#endif /* VBOX_HGCM */

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
    VMMDevState *pData = PDMINS2DATA(pDevIns, VMMDevState*);
    if (u32Version != VMMDEV_SSM_VERSION)
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    SSMR3GetU32(pSSMHandle, &pData->hypervisorSize);
    SSMR3GetU32(pSSMHandle, &pData->mouseCapabilities);
    SSMR3GetU32(pSSMHandle, &pData->mouseXAbs);
    SSMR3GetU32(pSSMHandle, &pData->mouseYAbs);

    SSMR3GetBool(pSSMHandle, &pData->fNewGuestFilterMask);
    SSMR3GetU32(pSSMHandle, &pData->u32NewGuestFilterMask);
    SSMR3GetU32(pSSMHandle, &pData->u32GuestFilterMask);
    SSMR3GetU32(pSSMHandle, &pData->u32HostEventFlags);
//    SSMR3GetBool(pSSMHandle, &pData->pVMMDevRAMHC->fHaveEvents);
    // here be dragons (probably)
    SSMR3GetMem(pSSMHandle, &pData->pVMMDevRAMHC->V, sizeof (pData->pVMMDevRAMHC->V));

    SSMR3GetMem(pSSMHandle, &pData->guestInfo, sizeof (pData->guestInfo));
    SSMR3GetU32(pSSMHandle, &pData->fu32AdditionsOk);
    SSMR3GetU32(pSSMHandle, &pData->u32VideoAccelEnabled);

#ifdef VBOX_HGCM
    vmmdevHGCMLoadState (pData, pSSMHandle);
#endif /* VBOX_HGCM */

    /*
     * On a resume, we send the capabilities changed message so
     * that listeners can sync their state again
     */
    Log(("vmmdevLoadState: capabilities changed (%x), informing connector\n", pData->mouseCapabilities));
    pData->pDrv->pfnUpdateMouseCapabilities(pData->pDrv, pData->mouseCapabilities);

    /* Reestablish the acceleration status. */
    if (pData->u32VideoAccelEnabled)
    {
        pData->pDrv->pfnVideoAccelEnable (pData->pDrv, !!pData->u32VideoAccelEnabled, &pData->pVMMDevRAMHC->vbvaMemory);
    }

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
    VMMDevState *pData = PDMINS2DATA(pDevIns, VMMDevState*);

#ifdef VBOX_HGCM
    vmmdevHGCMLoadStateDone (pData, pSSMHandle);
#endif /* VBOX_HGCM */

    VMMDevNotifyGuest (pData, VMMDEV_EVENT_RESTORED);

    return VINF_SUCCESS;
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
    VMMDevState *pData = PDMINS2DATA(pDevIns, VMMDevState *);

    Assert(iInstance == 0);

    /*
     * Validate and read the configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "GetHostTimeDisabled\0BackdoorLogDisabled\0"))
        return VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES;

    rc = CFGMR3QueryBool(pCfgHandle, "GetHostTimeDisabled", &pData->fGetHostTimeDisabled);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pData->fGetHostTimeDisabled = false;
    else if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed querying \"GetHostTimeDisabled\" as a boolean"));

    rc = CFGMR3QueryBool(pCfgHandle, "BackdoorLogDisabled", &pData->fBackdoorLogDisabled);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pData->fBackdoorLogDisabled = false;
    else if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed querying \"BackdoorLogDisabled\" as a boolean"));

    /*
     * Initialize data (most of it anyway).
     */
    /* PCI vendor, just a free bogus value */
    pData->dev.config[0x00] = 0xee;
    pData->dev.config[0x01] = 0x80;
    /* device ID */
    pData->dev.config[0x02] = 0xfe;
    pData->dev.config[0x03] = 0xca;
    /* class sub code (other type of system peripheral) */
    pData->dev.config[0x0a] = 0x80;
    /* class base code (base system peripheral) */
    pData->dev.config[0x0b] = 0x08;
    /* header type */
    pData->dev.config[0x0e] = 0x00;
    /* interrupt on pin 0 */
    pData->dev.config[0x3d] = 0x01;

    /*
     * Register the backdoor logging port
     */
    rc = PDMDevHlpIOPortRegister(pDevIns, RTLOG_DEBUG_PORT, 1, NULL, vmmdevBackdoorLog, NULL, NULL, NULL, "VMMDev backdoor logging");

#ifdef TIMESYNC_BACKDOOR
    /*
     * Alternative timesync source (temporary!)
     */
    rc = PDMDevHlpIOPortRegister(pDevIns, 0x505, 1, NULL, vmmdevTimesyncBackdoorWrite, vmmdevTimesyncBackdoorRead, NULL, NULL, "VMMDev timesync backdoor");
#endif

    /*
     * Register the PCI device.
     */
    rc = PDMDevHlpPCIRegister(pDevIns, &pData->dev);
    if (VBOX_FAILURE(rc))
        return rc;
    if (pData->dev.devfn == 32 || iInstance != 0)
        Log(("!!WARNING!!: pData->dev.devfn=%d (ignore if testcase or no started by Main)\n", pData->dev.devfn));
    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 0, 0x20, PCI_ADDRESS_SPACE_IO, vmmdevIOPortRegionMap);
    if (VBOX_FAILURE(rc))
        return rc;
    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 1, VMMDEV_RAM_SIZE, PCI_ADDRESS_SPACE_MEM, vmmdevIORAMRegionMap);
    if (VBOX_FAILURE(rc))
        return rc;

    /*
     * Interfaces
     */
    /* Base */
    pData->Base.pfnQueryInterface         = vmmdevPortQueryInterface;

    /* VMMDev port */
    pData->Port.pfnQueryAbsoluteMouse     = vmmdevQueryAbsoluteMouse;
    pData->Port.pfnSetAbsoluteMouse       = vmmdevSetAbsoluteMouse;
    pData->Port.pfnQueryMouseCapabilities = vmmdevQueryMouseCapabilities;
    pData->Port.pfnSetMouseCapabilities   = vmmdevSetMouseCapabilities;
    pData->Port.pfnRequestDisplayChange   = vmmdevRequestDisplayChange;
    pData->Port.pfnSetCredentials         = vmmdevSetCredentials;
    pData->Port.pfnVBVAChange             = vmmdevVBVAChange;


#ifdef VBOX_HGCM
    /* HGCM port */
    pData->HGCMPort.pfnCompleted          = hgcmCompleted;
#endif

   /*    * Get the corresponding connector interface
    */
   rc = PDMDevHlpDriverAttach(pDevIns, 0, &pData->Base, &pData->pDrvBase, "VMM Driver Port");
   if (VBOX_SUCCESS(rc))
   {
       pData->pDrv = (PPDMIVMMDEVCONNECTOR)pData->pDrvBase->pfnQueryInterface(pData->pDrvBase, PDMINTERFACE_VMMDEV_CONNECTOR);
       if (!pData->pDrv)
       {
           AssertMsgFailed(("LUN #0 doesn't have a VMMDev connector interface! rc=%Vrc\n", rc));
           rc = VERR_PDM_MISSING_INTERFACE;
       }
#ifdef VBOX_HGCM
       else
       {
           pData->pHGCMDrv = (PPDMIHGCMCONNECTOR)pData->pDrvBase->pfnQueryInterface(pData->pDrvBase, PDMINTERFACE_HGCM_CONNECTOR);
           if (!pData->pHGCMDrv)
           {
               Log(("LUN #0 doesn't have a HGCM connector interface, HGCM is not supported. rc=%Vrc\n", rc));
               /* this is not actually an error, just means that there is no support for HGCM */
           }
       }
#endif
   }
   else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
   {
       Log(("%s/%d: warning: no driver attached to LUN #0!\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
       rc = VINF_SUCCESS;
   }
   else
       AssertMsgFailed(("Failed to attach LUN #0! rc=%Vrc\n", rc));

    rc = PDMDevHlpSSMRegister(pDevIns, "VMMDev", iInstance, VMMDEV_SSM_VERSION, sizeof(*pData),
                                          NULL, vmmdevSaveState, NULL,
                                          NULL, vmmdevLoadState, vmmdevLoadStateDone);

    /* Save PDM device instance data for future reference. */
    pData->pDevIns = pDevIns;


    /*
     * Allocate the VMMDev RAM region.
     */
    /** @todo freeing of the RAM. */
    rc = SUPPageAlloc(VMMDEV_RAM_SIZE >> PAGE_SHIFT, (void **)&pData->pVMMDevRAMHC);
    if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("VMMDev SUPPageAlloc(%#x,) -> %d\n", VMMDEV_RAM_SIZE, rc));
    }
    
#ifdef VBOX_HGCM
    rc = RTCritSectInit(&pData->critsectHGCMCmdList);
    AssertRC(rc);
#endif /* VBOX_HGCM */

    /* initialize the VMMDev memory */
    memset (pData->pVMMDevRAMHC, 0, sizeof (VMMDevMemory));
    pData->pVMMDevRAMHC->u32Size = sizeof (VMMDevMemory);
    pData->pVMMDevRAMHC->u32Version = VMMDEV_MEMORY_VERSION;

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
    VMMDevState *pData = PDMINS2DATA(pDevIns, VMMDevState*);
    /*
     * Reset the mouse integration feature bit
     */
    if (pData->mouseCapabilities & (VMMDEV_MOUSEGUESTWANTSABS|VMMDEV_MOUSEGUESTNEEDSHOSTCUR))
    {
        pData->mouseCapabilities &= ~VMMDEV_MOUSEGUESTWANTSABS;
        /* notify the connector */
        Log(("vmmdevReset: capabilities changed (%x), informing connector\n", pData->mouseCapabilities));
        pData->pDrv->pfnUpdateMouseCapabilities(pData->pDrv, pData->mouseCapabilities);
    }

    pData->hypervisorSize = 0;

    pData->u32HostEventFlags = 0;

    if (pData->pVMMDevRAMHC)
    {
        memset (pData->pVMMDevRAMHC, 0, sizeof (VMMDevMemory));
    }

    /* credentials have to go away */
    memset(pData->credentialsLogon.szUserName, '\0', VMMDEV_CREDENTIALS_STRLEN);
    memset(pData->credentialsLogon.szPassword, '\0', VMMDEV_CREDENTIALS_STRLEN);
    memset(pData->credentialsLogon.szDomain, '\0', VMMDEV_CREDENTIALS_STRLEN);
    memset(pData->credentialsJudge.szUserName, '\0', VMMDEV_CREDENTIALS_STRLEN);
    memset(pData->credentialsJudge.szPassword, '\0', VMMDEV_CREDENTIALS_STRLEN);
    memset(pData->credentialsJudge.szDomain, '\0', VMMDEV_CREDENTIALS_STRLEN);

    /* Reset means that additions will report again. */
    pData->fu32AdditionsOk = false;
    memset (&pData->guestInfo, 0, sizeof (pData->guestInfo));

    memset (&pData->lastReadDisplayChangeRequest, 0, sizeof (pData->lastReadDisplayChangeRequest));

    /* Clear the event variables.
     *
     *   Note: The pData->u32HostEventFlags is not cleared.
     *         It is designed that way so host events do not
     *         depend on guest resets.
     */
    pData->u32GuestFilterMask    = 0;
    pData->u32NewGuestFilterMask = 0;
    pData->fNewGuestFilterMask   = 0;
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
    /* szGCMod */
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
    NULL
};
#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */

