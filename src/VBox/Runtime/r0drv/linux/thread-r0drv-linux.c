/* $Id$ */
/** @file
 * innotek Portable Runtime - Threads, Ring-0 Driver, Linux.
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
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "the-linux-kernel.h"

#include <iprt/thread.h>
#include <iprt/err.h>


RTDECL(RTNATIVETHREAD) RTThreadNativeSelf(void)
{
    return (RTNATIVETHREAD)current;
}


RTDECL(int)   RTThreadSleep(unsigned cMillies)
{
    long cJiffies = msecs_to_jiffies(cMillies);
    set_current_state(TASK_INTERRUPTIBLE);
    cJiffies = schedule_timeout(cJiffies);
    if (!cJiffies)
        return VINF_SUCCESS;
    return VERR_INTERRUPTED;
}


RTDECL(bool) RTThreadYield(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 20)
    yield();
#else
    set_current_state(TASK_RUNNING);
    sys_sched_yield();
    schedule();
#endif
    return true;
}

