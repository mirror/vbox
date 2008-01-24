/* $Id$ */
/** @file
 * VBoxGuestR3Lib - Ring-3 Support Library for VirtualBox guest additions, Mouse.
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
#include "VBGLR3Internal.h"


VBGLR3DECL(int) VbglR3GetMouseStatus(uint32_t *pfFeatures, uint32_t *px, uint32_t *py)
{
    VMMDevReqMouseStatus Req;
    vmmdevInitRequest(&Req.header, VMMDevReq_GetMouseStatus);
    Req.mouseFeatures = 0;
    Req.pointerXPos = 0;
    Req.pointerYPos = 0;
    int rc = VbglR3GRPerform(&Req.header);
    if (RT_SUCCESS(rc))
    {
        if (pfFeatures)
            *pfFeatures = Req.mouseFeatures;
        if (px)
            *px = Req.pointerXPos;
        if (py)
            *py = Req.pointerYPos;
    }
    return rc;
}


VBGLR3DECL(int) VbglR3SetMouseStatus(uint32_t fFeatures)
{
    VMMDevReqMouseStatus Req;
    vmmdevInitRequest(&Req.header, VMMDevReq_SetMouseStatus);
    Req.mouseFeatures = fFeatures;
    Req.pointerXPos = 0;
    Req.pointerYPos = 0;
    return VbglR3GRPerform(&Req.header);
}

