/* $Id$ */
/** @file
 * InnoTek Portable Runtime - Threads, Ring-0 Driver, Linux.
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

