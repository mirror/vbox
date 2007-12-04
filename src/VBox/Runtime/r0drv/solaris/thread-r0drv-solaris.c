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
    clock_t cTicks;
    unsigned long timeout;
    
    if (!cMillies)
    {
        RTThreadYield();
        return VINF_SUCCESS;
    }

    if (cMillies != RT_INDEFINITE_WAIT)
        cTicks = drv_usectohz((clock_t)(cMillies * 1000L));
    else
        cTicks = 0;

#if 0
    timeout = ddi_get_lbolt();
    timeout += cTicks; 
 
    kcondvar_t cnd;
    kmutex_t mtx;
    mutex_init(&mtx, "IPRT Sleep Mutex", MUTEX_DRIVER, NULL);
    cv_init(&cnd, "IPRT Sleep CV", CV_DRIVER, NULL);
    mutex_enter(&mtx);
    cv_timedwait (&cnd, &mtx, timeout);
    mutex_exit(&mtx);
    cv_destroy(&cnd);
    mutex_destroy(&mtx);
#endif

#if 1
    delay(cTicks);
#endif

#if 0
    /*   Hmm, no same effect as using delay() */
    struct timespec t;
    t.tv_sec = 0;
    t.tv_nsec = cMillies * 1000000L;
    nanosleep (&t, NULL);
#endif

    return VINF_SUCCESS;
}


RTDECL(bool) RTThreadYield(void)
{
    schedctl_set_yield(curthread, 0);
    return true;
}

