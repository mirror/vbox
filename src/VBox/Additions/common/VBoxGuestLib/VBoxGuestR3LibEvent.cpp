/* $Id$ */
/** @file
 * VBoxGuestR3Lib - Ring-3 Support Library for VirtualBox guest additions, Events.
 */

/*
 * Copyright (C) 2007-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
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
 * @returns IPRT status code.
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
        /*
         * If a guest requests for an event which is not available on the host side
         * (because of an older host version, a disabled feature or older Guest Additions),
         * don't trigger an assertion here even in debug builds - would be annoying.
         */
#if 0
        AssertMsg(waitEvent.u32Result == VBOXGUEST_WAITEVENT_OK, ("%d rc=%Rrc\n", waitEvent.u32Result, rc));
#endif
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
 * event loops.  After calling this, VBOXGUEST_IOCTL_WAITEVENT should no longer
 * be called in the same session.  At the time of writing this is not enforced;
 * at the time of reading it may be.
 *
 * @returns IPRT status code.
 */
VBGLR3DECL(int) VbglR3InterruptEventWaits(void)
{
    return vbglR3DoIOCtl(VBOXGUEST_IOCTL_CANCEL_ALL_WAITEVENTS, 0, 0);
}

