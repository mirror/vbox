/* $Id$ */
/** @file
 * InnoTek Portable Runtime - RTTimeNow, POSIX.
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
#define LOG_GROUP RTLOGGROUP_TIME
#define RTTIME_INCL_TIMEVAL
#include <sys/time.h>
#include <time.h>

#include <iprt/time.h>


/**
 * Gets the current system time.
 *
 * @returns pTime.
 * @param   pTime   Where to store the time.
 */
RTR3DECL(PRTTIMESPEC) RTTimeNow(PRTTIMESPEC pTime)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return RTTimeSpecSetTimeval(pTime, &tv);
}

