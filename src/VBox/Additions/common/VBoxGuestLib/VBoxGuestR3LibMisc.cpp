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
#include <VBox/log.h>

#include "VBGLR3Internal.h"

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
     * *BSD does not accept more than 4KB per ioctl request,
     * so, split it up into 2KB chunks.
     */
#define STEP 2048
    int rc = VINF_SUCCESS;
    for (size_t off = 0; off < cb && RT_SUCCESS(rc); off += STEP)
    {
        size_t cbStep = RT_MIN(cb - off, STEP);
        rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_LOG(cbStep), (char *)pch + off, cbStep);
    }
#undef STEP
    return rc;
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
    VBoxGuestFilterMaskInfo Info;
    Info.u32OrMask = fOr;
    Info.u32NotMask = fNot;
    return vbglR3DoIOCtl(VBOXGUEST_IOCTL_CTL_FILTER_MASK, &Info, sizeof(Info));
}


/**
 * Report a change in the capabilities that we support to the host.
 *
 * @returns IPRT status value
 * @param   fOr     Capabilities which have been added.
 * @param   fNot    Capabilities which have been removed.
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

