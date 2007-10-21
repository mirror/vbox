/* $Id$ */
/** @file
 * innotek Portable Runtime - Time Relative to Program Start.
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
#include <iprt/assert.h>
#include "internal/time.h"



/**
 * Get the nanosecond timestamp relative to program startup.
 *
 * @returns Timestamp relative to program startup.
 */
RTDECL(uint64_t)  RTTimeProgramNanoTS(void)
{
    return RTTimeNanoTS() - g_u64ProgramStartNanoTS;
}


/**
 * Get the microsecond timestamp relative to program startup.
 *
 * @returns Timestamp relative to program startup.
 */
RTDECL(uint64_t)  RTTimeProgramMicroTS(void)
{
    return RTTimeProgramNanoTS() / 1000;
}


/**
 * Get the millisecond timestamp relative to program startup.
 *
 * @returns Timestamp relative to program startup.
 */
RTDECL(uint64_t)  RTTimeProgramMilliTS(void)
{
    return RTTimeMilliTS() - g_u64ProgramStartMilliTS;
}


/**
 * Get the second timestamp relative to program startup.
 *
 * @returns Timestamp relative to program startup.
 */
RTDECL(uint32_t)  RTTimeProgramSecTS(void)
{
    AssertMsg(g_u64ProgramStartMilliTS, ("RTR3Init hasn't been called!\n"));
    return (uint32_t)(RTTimeProgramMilliTS() / 1000);
}


/**
 * Get the RTTimeNanoTS() of when the program started.
 *
 * @returns Program startup timestamp.
 */
RTDECL(uint64_t) RTTimeProgramStartNanoTS(void)
{
    return g_u64ProgramStartNanoTS;
}
