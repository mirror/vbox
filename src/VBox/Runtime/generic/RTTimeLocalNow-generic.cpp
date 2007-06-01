/* $Id $ */
/** @file
 * innotek Portable Runtime - Time, generic RTTimeLocalNow.
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
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/time.h>


RTDECL(PRTTIMESPEC) RTTimeLocalNow(PRTTIMESPEC pTime)
{
    int64_t i64PreDelta;
    int64_t i64PostDelta;
    do
    {
        i64PreDelta = RTTimeLocalDeltaNano();
        RTTimeNow(pTime);
        i64PostDelta = RTTimeLocalDeltaNano();
    } while (i64PreDelta != i64PostDelta);

    return RTTimeSpecAddNano(pTime, i64PostDelta);
}

