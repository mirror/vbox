/* $Id$ */
/** @file
 * VBoxGuestR3Lib - Ring-3 Support Library for VirtualBox guest additions, Misc.
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
 * Change the IRQ filter mask.
 *
 * @returns IPRT status code.
 * @param   fOr     The OR mask.
 * @param   fNot    The NOT mask.
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
 * @returns IPRT status code.
 * @param   fOr     Capabilities which have been added.
 * @param   fNot    Capabilities which have been removed.
 *
 * @todo    Move to a different file.
 */
VBGLR3DECL(int) VbglR3SetGuestCaps(uint32_t fOr, uint32_t fNot)
{
    VBoxGuestSetCapabilitiesInfo Info;
    Info.u32OrMask = fOr;
    Info.u32NotMask = fNot;
    return vbglR3DoIOCtl(VBOXGUEST_IOCTL_SET_GUEST_CAPABILITIES, &Info,
                         sizeof(Info));
}


/**
 * Acquire capabilities to report to the host.  The capabilities which can be
 * acquired are the same as those reported by @a VbglR3SetGuestCaps, and once
 * a capability has been acquired once is is switched to "acquire mode" and can
 * no longer be set using @a VbglR3SetGuestCaps.  Capabilities can also be
 * switched to acquire mode without actually being acquired.  A client can not
 * acquire a capability which has been acquired and not released by another
 * client.  Capabilities acquired are automatically released on session
 * termination.
 *
 * @returns IPRT status code
 * @returns VERR_RESOURCE_BUSY and acquires nothing if another client has
 *          acquired and not released at least one of the @a fOr capabilities
 * @param   fOr      Capabilities to acquire or to switch to acquire mode
 * @param   fNot     Capabilities to release
 * @param   fConfig  if set, capabilities in @a fOr are switched to acquire mode
 *                   but not acquired, and @a fNot is ignored.
 */
VBGLR3DECL(int) VbglR3AcquireGuestCaps(uint32_t fOr, uint32_t fNot, bool fConfig)
{
    VBoxGuestCapsAquire Info;
    int rc;

    Info.u32OrMask = fOr;
    Info.u32NotMask = fNot;
    Info.enmFlags = fConfig ? VBOXGUESTCAPSACQUIRE_FLAGS_CONFIG_ACQUIRE_MODE
                            : VBOXGUESTCAPSACQUIRE_FLAGS_NONE;
    rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_GUEST_CAPS_ACQUIRE, &Info, sizeof(Info));
    if (RT_FAILURE(rc))
        return rc;
    return Info.rc;
}


/**
 * Query the session ID of this VM.
 *
 * The session id is an unique identifier that gets changed for each VM start,
 * reset or restore.  Useful for detection a VM restore.
 *
 * @returns IPRT status code.
 * @param   pu64IdSession       Session id (out).  This is NOT changed on
 *                              failure, so the caller can depend on this to
 *                              deal with backward compatibility (see
 *                              VBoxServiceVMInfoWorker() for an example.)
 */
VBGLR3DECL(int) VbglR3GetSessionId(uint64_t *pu64IdSession)
{
    VMMDevReqSessionId Req;

    vmmdevInitRequest(&Req.header, VMMDevReq_GetSessionId);
    Req.idSession = 0;
    int rc = vbglR3GRPerform(&Req.header);
    if (RT_SUCCESS(rc))
        *pu64IdSession = Req.idSession;

    return rc;
}
