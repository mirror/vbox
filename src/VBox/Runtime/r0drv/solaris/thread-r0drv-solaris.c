/* $Id$ */
/** @file
 * innotek Portable Runtime - Threads, Ring-0 Driver, Solaris.
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
#include "the-solaris-kernel.h"

#include <iprt/thread.h>
#include <iprt/err.h>
#include <iprt/assert.h>


RTDECL(RTNATIVETHREAD) RTThreadNativeSelf(void)
{
    return (RTNATIVETHREAD)curthread;
}


RTDECL(int) RTThreadSleep(unsigned cMillies)
{
    int cTicks;

    if (!cMillies)
    {
        RTThreadYield();
        return VINF_SUCCESS;
    }

    if (cMillies != RT_INDEFINITE_WAIT)
        cTicks = drv_usectohz((clock_t)(cMillies * 1000L));
    else
        cTicks = 0;
    
    /* hm, maybe we could also use nanosleep() */
    delay(cTicks);
    
/*   Hmm, no same effect as using delay()
    struct timespec t;
    t.tv_sec = 0;
    t.tv_nsec = cMillies * 1000000L;
    nanosleep (&t, NULL);
*/    
    return VINF_SUCCESS;
}


RTDECL(bool) RTThreadYield(void)
{
    schedctl_set_yield(curthread, 0);
    return true;
}

