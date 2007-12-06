/* $Id: $ */
/** @file
 * innotek Portable Runtime - Time, generic RTTimeLocalExplode.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/time.h>


RTDECL(PRTTIME) RTTimeLocalExplode(PRTTIME pTime, PCRTTIMESPEC pTimeSpec)
{
    pTime = RTTimeExplode(pTime, pTimeSpec);
    if (pTime)
    {
        pTime->fFlags = (pTime->fFlags & ~RTTIME_FLAGS_TYPE_MASK) | RTTIME_FLAGS_TYPE_LOCAL;
        pTime->offZone = RTTimeLocalDeltaNano() / (UINT64_C(1000000000)*3600); /** @todo this is obviosly wrong. Need RTTimeLocalDeltaNanoFor(pTimeSpec); */
    }
    return pTime;
}

