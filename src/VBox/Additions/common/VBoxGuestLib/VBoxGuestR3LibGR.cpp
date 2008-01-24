/* $Id$ */
/** @file
 * VBoxGuestR3Lib - Ring-3 Support Library for VirtualBox guest additions, GR.
 */

/*
 * Copyright (C) 2007 innotek GmbH
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
#include <iprt/mem.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/err.h>
#include <VBox/VBoxGuest.h>
#include "VBGLR3Internal.h"


int vbglR3GRAlloc(VMMDevRequestHeader **ppReq, uint32_t cb, VMMDevRequestType enmReqType)
{
    VMMDevRequestHeader *pReq;

    AssertPtrReturn(ppReq, VERR_INVALID_PARAMETER);
    AssertMsgReturn(cb >= sizeof(VMMDevRequestHeader), ("%#x vs %#zx\n", cb, sizeof(VMMDevRequestHeader)),
                    VERR_INVALID_PARAMETER);

    pReq = (VMMDevRequestHeader *)RTMemTmpAlloc(cb);
    if (RT_UNLIKELY(!pReq))
        return VERR_NO_MEMORY;

    pReq->size        = cb;
    pReq->version     = VMMDEV_REQUEST_HEADER_VERSION;
    pReq->requestType = enmReqType;
    pReq->rc          = VERR_GENERAL_FAILURE;
    pReq->reserved1   = 0;
    pReq->reserved2   = 0;

    *ppReq = pReq;

    return VINF_SUCCESS;
}


VBGLR3DECL(int) vbglR3GRPerform(VMMDevRequestHeader *pReq)
{
    return vbglR3DoIOCtl(VBOXGUEST_IOCTL_VMMREQUEST(pReq->size), pReq, pReq->size);
}


void vbglR3GRFree(VMMDevRequestHeader *pReq)
{
    RTMemTmpFree(pReq);
}

