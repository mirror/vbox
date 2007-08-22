/* $Id$ */
/** @file
 * innotek Portable Runtime - Time, Ring-0 Driver, Solaris.
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
#define RTTIME_INCL_TIMESPEC

#include <iprt/time.h>


RTDECL(uint64_t) RTTimeNanoTS(void)
{
    return gethrtime();
}


RTDECL(uint64_t) RTTimeMilliTS(void)
{
    return RTTimeNanoTS() / 1000000;
}


RTDECL(uint64_t) RTTimeSystemNanoTS(void)
{
    return RTTimeNanoTS();
}


RTDECL(uint64_t) RTTimeSystemMilliTS(void)
{
    return RTTimeNanoTS() / 1000000;
}


RTDECL(PRTTIMESPEC) RTTimeNow(PRTTIMESPEC pTime)
{
    /* timestruc_t is actually just a typedef struct timespec */
    timestruc_t tv = tod_get();
    return RTTimeSpecSetNano(pTime, (uint64_t)tv.tv_sec * 1000000000 + tv.tv_nsec);
}
