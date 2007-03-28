/** $Id$ */
/** @file
 * InnoTek Portable Runtime - Timers, Generic RTTimerCreate() Implementation.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
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
#include <iprt/timer.h>
#include <iprt/err.h>
#include <iprt/assert.h>


RTDECL(int) RTTimerCreate(PRTTIMER *ppTimer, unsigned uMilliesInterval, PFNRTTIMER pfnTimer, void *pvUser)
{
    int rc = RTTimerCreateEx(ppTimer, uMilliesInterval * UINT64_C(1000000), 0, pfnTimer, pvUser);
    if (RT_SUCCESS(rc))
    {
        rc = RTTimerStart(*ppTimer, 0);
        if (RT_SUCCESS(rc))
            return rc;
        int rc2 = RTTimerDestroy(*ppTimer); AssertRC(rc2);
        *ppTimer = NULL;
    }
    return rc;
}

