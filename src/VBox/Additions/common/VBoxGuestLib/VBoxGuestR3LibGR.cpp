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
#ifdef VBOX_VBGLR3_XFREE86
/* For the definitions of standard X server library functions like xalloc. */
/* Make the headers C++-compatible */
# define class xf86_vbox_class
# define bool xf86_vbox_bool
# define private xf86_vbox_private
# define new xf86_vbox_new
extern "C"
{
# include "xf86.h"
# include "xf86_OSproc.h"
# include "xf86Resources.h"
# include "xf86_ansic.h"
}
# undef class
# undef bool
# undef private
# undef new
#else
# include <iprt/mem.h>
# include <iprt/assert.h>
# include <iprt/string.h>
#endif
#include <iprt/err.h>
#include <VBox/VBoxGuest.h>
#include "VBGLR3Internal.h"


int vbglR3GRAlloc(VMMDevRequestHeader **ppReq, uint32_t cb, VMMDevRequestType enmReqType)
{
    VMMDevRequestHeader *pReq;

#ifdef VBOX_VBGLR3_XFREE86
    pReq = (VMMDevRequestHeader *)xalloc(cb);
#else
    AssertPtrReturn(ppReq, VERR_INVALID_PARAMETER);
    AssertMsgReturn(cb >= sizeof(VMMDevRequestHeader), ("%#x vs %#zx\n", cb, sizeof(VMMDevRequestHeader)),
                    VERR_INVALID_PARAMETER);

    pReq = (VMMDevRequestHeader *)RTMemTmpAlloc(cb);
#endif
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
#ifdef VBOX_VBGLR3_XFREE86
    xfree(pReq);
#else
    RTMemTmpFree(pReq);
#endif
}

