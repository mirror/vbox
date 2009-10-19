/* $Id$ */
/** @file
 * VBoxGuestR3Lib - Ring-3 Support Library for VirtualBox guest additions, Misc.
 */

/*
 * Copyright (C) 2007 Sun Microsystems, Inc.
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
#include <iprt/mem.h>
#include <VBox/log.h>

#include "VBGLR3Internal.h"


/**
 * Wait for the host to signal one or more events and return which.
 *
 * The events will only be delivered by the host if they have been enabled
 * previously using @a VbglR3CtlFilterMask.  If one or several of the events
 * have already been signalled but not yet waited for, this function will return
 * immediately and return those events.
 *
 * @returns IPRT status code
 *
 * @param   fMask       The events we want to wait for, or-ed together.
 * @param   cMillies    How long to wait before giving up and returning
 *                      (VERR_TIMEOUT). Use RT_INDEFINITE_WAIT to wait until we
 *                      are interrupted or one of the events is signalled.
 * @param   pfEvents    Where to store the events signalled. Optional.
 */
VBGLR3DECL(int) VbglR3WaitEvent(uint32_t fMask, uint32_t cMillies, uint32_t *pfEvents)
{
    LogFlow(("VbglR3WaitEvent: fMask=0x%x, cMillies=%u, pfEvents=%p\n",
             fMask, cMillies, pfEvents));
    AssertReturn((fMask & ~VMMDEV_EVENT_VALID_EVENT_MASK) == 0, VERR_INVALID_PARAMETER);
    AssertPtrNullReturn(pfEvents, VERR_INVALID_POINTER);

    VBoxGuestWaitEventInfo waitEvent;
    waitEvent.u32TimeoutIn = cMillies;
    waitEvent.u32EventMaskIn = fMask;
    waitEvent.u32Result = VBOXGUEST_WAITEVENT_ERROR;
    waitEvent.u32EventFlagsOut = 0;
    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_WAITEVENT, &waitEvent, sizeof(waitEvent));
    if (RT_SUCCESS(rc))
    {
        AssertMsg(waitEvent.u32Result == VBOXGUEST_WAITEVENT_OK, ("%d\n", waitEvent.u32Result));
        if (pfEvents)
            *pfEvents = waitEvent.u32EventFlagsOut;
    }

    LogFlow(("VbglR3WaitEvent: rc=%Rrc, u32EventFlagsOut=0x%x. u32Result=%d\n",
             rc, waitEvent.u32EventFlagsOut, waitEvent.u32Result));
    return rc;
}


/**
 * Cause any pending WaitEvent calls (VBOXGUEST_IOCTL_WAITEVENT) to return
 * with a VERR_INTERRUPTED status.
 *
 * Can be used in combination with a termination flag variable for interrupting
 * event loops. Avoiding race conditions is the responsibility of the caller.
 *
 * @returns IPRT status code
 */
VBGLR3DECL(int) VbglR3InterruptEventWaits(void)
{
    return vbglR3DoIOCtl(VBOXGUEST_IOCTL_CANCEL_ALL_WAITEVENTS, 0, 0);
}


/**
 * Write to the backdoor logger from ring 3 guest code.
 *
 * @returns IPRT status code
 *
 * @remarks This currently does not accept more than 255 bytes of data at
 *          one time. It should probably be rewritten to use pass a pointer
 *          in the IOCtl.
 */
VBGLR3DECL(int) VbglR3WriteLog(const char *pch, size_t cb)
{
    /*
     * Quietly skip NULL strings.
     * (Happens in the RTLogBackdoorPrintf case.)
     */
    if (!cb)
        return VINF_SUCCESS;
    if (!VALID_PTR(pch))
        return VERR_INVALID_POINTER;

#ifdef RT_OS_WINDOWS
    /*
     * Duplicate the string as it may be read only (a C string).
     */
    void *pvTmp = RTMemDup(pch, cb);
    if (!pvTmp)
        return VERR_NO_MEMORY;
    int rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_LOG(cb), pvTmp, cb);
    RTMemFree(pvTmp);
    return rc;

#elif 0 /** @todo Several OSes could take this route (solaris and freebsd for instance). */
    /*
     * Handle the entire request in one go.
     */
    return vbglR3DoIOCtl(VBOXGUEST_IOCTL_LOG(cb), pvTmp, cb);

#else
    /*
     * *BSD does not accept more than 4KB per ioctl request, while
     * Linux can't express sizes above 8KB, so, split it up into 2KB chunks.
     */
# define STEP 2048
    int rc = VINF_SUCCESS;
    for (size_t off = 0; off < cb && RT_SUCCESS(rc); off += STEP)
    {
        size_t cbStep = RT_MIN(cb - off, STEP);
        rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_LOG(cbStep), (char *)pch + off, cbStep);
    }
# undef STEP
    return rc;
#endif
}


/**
 * Change the IRQ filter mask.
 *
 * @returns IPRT status code
 * @param   fOr     The OR mask.
 * @param   fNo     The NOT mask.
 */
VBGLR3DECL(int) VbglR3CtlFilterMask(uint32_t fOr, uint32_t fNot)
{
#if defined(RT_OS_WINDOWS)
    /** @todo Not yet implemented. */
    return VERR_NOT_SUPPORTED;

#else

    VBoxGuestFilterMaskInfo Info;
    Info.u32OrMask = fOr;
    Info.u32NotMask = fNot;
    return vbglR3DoIOCtl(VBOXGUEST_IOCTL_CTL_FILTER_MASK, &Info, sizeof(Info));
#endif
}


/**
 * Report a change in the capabilities that we support to the host.
 *
 * @returns IPRT status value
 * @param   fOr     Capabilities which have been added.
 * @param   fNot    Capabilities which have been removed.
 *
 * @todo    Move to a different file.
 */
VBGLR3DECL(int) VbglR3SetGuestCaps(uint32_t fOr, uint32_t fNot)
{
    VMMDevReqGuestCapabilities2 vmmreqGuestCaps;
    int rc;

    vmmdevInitRequest(&vmmreqGuestCaps.header, VMMDevReq_SetGuestCapabilities);
    vmmreqGuestCaps.u32OrMask = fOr;
    vmmreqGuestCaps.u32NotMask = fNot;
    rc = vbglR3GRPerform(&vmmreqGuestCaps.header);
#ifdef DEBUG
    if (RT_SUCCESS(rc))
        LogRel(("Successfully changed guest capabilities: or mask 0x%x, not mask 0x%x.\n",
                fOr, fNot));
    else
        LogRel(("Failed to change guest capabilities: or mask 0x%x, not mask 0x%x.  rc = %Rrc.\n",
                fOr, fNot, rc));
#endif
    return rc;
}

