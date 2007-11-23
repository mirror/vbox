/** $Id$ */
/** @file
 * VBoxGuestR3Lib - Ring-3 Support Library for VirtualBox guest additions, Seamless mode.
 */

/*
 * Copyright (C) 2007 innotek GmbH
 *
 * innotek GmbH confidential
 * All rights reserved
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/assert.h>

#include <VBox/VBoxGuest.h>
#include <VBox/VBoxDev.h>

#include <string.h>

/* Move this to a header { */

extern int vbglR3DoIOCtl(unsigned iFunction, void *pvData, size_t cbData);

/* } */

/**
 * Tell the host that we support (or no longer support) seamless mode.
 *
 * @returns IPRT status value
 * @param   bState whether or not we support seamless mode
 *
 * @todo    Currently this will trample over any other capabilities the guest may have.
 *          This will have to be fixed when more capabilities are added at the latest.
 */
VBGLR3DECL(int) VbglR3SeamlessSetCap(bool bState)
{
    VMMDevReqGuestCapabilities vmmreqGuestCaps = { {0} };

    vmmdevInitRequest(&vmmreqGuestCaps.header, VMMDevReq_ReportGuestCapabilities);
    vmmreqGuestCaps.caps = bState ? VMMDEV_GUEST_SUPPORTS_SEAMLESS : 0;
    return VbglR3GRPerform(&vmmreqGuestCaps.header);
}

/**
 * Wait for a seamless mode change event.
 *
 * @returns IPRT status value
 * @retval  pMode on success, the seamless mode to switch into (i.e. disabled, visible region
 *                or host window)
 */
VBGLR3DECL(int) VbglR3SeamlessWaitEvent(VMMDevSeamlessMode *pMode)
{
    VBoxGuestWaitEventInfo waitEvent;
    int rc;

    AssertReturn(pMode != 0, VERR_INVALID_PARAMETER);
    waitEvent.u32TimeoutIn = 0;
    waitEvent.u32EventMaskIn = VMMDEV_EVENT_SEAMLESS_MODE_CHANGE_REQUEST;
    rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_WAITEVENT, &waitEvent, sizeof(waitEvent));
    if (RT_SUCCESS(rc))
    {
        /* did we get the right event? */
        if (waitEvent.u32EventFlagsOut & VMMDEV_EVENT_SEAMLESS_MODE_CHANGE_REQUEST)
        {
            VMMDevSeamlessChangeRequest seamlessChangeRequest;

            /* get the seamless change request */
            vmmdevInitRequest(&seamlessChangeRequest.header, VMMDevReq_GetSeamlessChangeRequest);
            seamlessChangeRequest.eventAck = VMMDEV_EVENT_SEAMLESS_MODE_CHANGE_REQUEST;
            rc = VbglR3GRPerform(&seamlessChangeRequest.header);
            if (RT_SUCCESS(rc))
            {
                *pMode = seamlessChangeRequest.mode;
                return VINF_SUCCESS;
            }
        }
        else
            rc = VERR_TRY_AGAIN;
    }
    return rc;
}

/**
 * Inform the host about the visible region
 *
 * @returns IPRT status code
 * @param   cRects number of rectangles in the list of visible rectangles
 * @param   pRects list of visible rectangles on the guest display
 *
 * @todo A scatter-gather version of VbglR3GRPerform would be nice, so that we don't have
 *       to copy our rectangle and header data into a single structure and perform an
 *       additional allocation.
 */
VBGLR3DECL(int) VbglR3SeamlessSendRects(uint32_t cRects, PRTRECT pRects)
{
    VMMDevVideoSetVisibleRegion *req;
    int rc;

    rc = VbglR3GRAlloc ((VMMDevRequestHeader **)&req,
                      sizeof (VMMDevVideoSetVisibleRegion) + (cRects-1)*sizeof(RTRECT),
                      VMMDevReq_VideoSetVisibleRegion);
    if (RT_SUCCESS(rc))
    {
        req->cRect = cRects;
        memcpy(&req->Rect, pRects, cRects*sizeof(RTRECT));
        rc = VbglR3GRPerform (&req->header);
        VbglR3GRFree(&req->header);
        if (RT_SUCCESS(rc))
        {
            if (RT_SUCCESS(req->header.rc))
            {
                return VINF_SUCCESS;
            }
            else
                rc = req->header.rc;
        }
    }
    return rc;
}
