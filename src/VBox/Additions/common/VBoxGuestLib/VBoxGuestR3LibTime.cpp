/** $Id$ */
/** @file
 * VBoxGuestR3Lib - Ring-3 Support Library for VirtualBox guest additions, Time.
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
#include <iprt/time.h>
#include "VBGLR3Internal.h"


VBGLR3DECL(int) VbglR3GetHostTime(PRTTIMESPEC pTime)
{
    VMMDevReqHostTime Req;
    vmmdevInitRequest(&Req.header, VMMDevReq_GetHostTime);
    Req.time = UINT64_MAX;
    int rc = vbglR3GRPerform(&Req.header);
    if (RT_SUCCESS(rc))
        RTTimeSpecSetMilli(pTime, (int64_t)Req.time);
    return rc;
}

