/* $Id$ */
/** @file
 * VBoxGuestR3Lib - Ring-3 support library for the guest additions, Internal header.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBGLR3Internal_h
#define ___VBGLR3Internal_h

#include <VBox/VBoxGuest.h>

__BEGIN_DECLS

int     vbglR3DoIOCtl(unsigned iFunction, void *pvData, size_t cbData);
int     vbglR3GRAlloc(VMMDevRequestHeader **ppReq, uint32_t cb, VMMDevRequestType enmReqType);
int     vbglR3GRPerform(VMMDevRequestHeader *pReq);
void    vbglR3GRFree(VMMDevRequestHeader *pReq);



DECLINLINE(void) VbglHGCMParmUInt32Set(HGCMFunctionParameter *pParm, uint32_t u32)
{
    pParm->type = VMMDevHGCMParmType_32bit;
    pParm->u.value32 = u32;
}


DECLINLINE(int) VbglHGCMParmUInt32Get(HGCMFunctionParameter *pParm, uint32_t *pu32)
{
    if (pParm->type == VMMDevHGCMParmType_32bit)
    {
        *pu32 = pParm->u.value32;
        return VINF_SUCCESS;
    }
    return VERR_INVALID_PARAMETER;
}


DECLINLINE(void) VbglHGCMParmPtrSet(HGCMFunctionParameter *pParm, void *pv, uint32_t cb)
{
    pParm->type                    = VMMDevHGCMParmType_LinAddr;
    pParm->u.Pointer.size          = cb;
    pParm->u.Pointer.u.linearAddr  = (vmmDevHypPtr)pv;
}

__END_DECLS

#endif
