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
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
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
RTR3DECL(uint64_t)  RTTimeProgramNanoTS(void)
{
    AssertMsg(g_u64ProgramStartNanoTS, ("RTR3Init hasn't been called!\n"));
    return RTTimeNanoTS() - g_u64ProgramStartNanoTS;
}


/**
 * Get the millisecond timestamp relative to program startup.
 *
 * @returns Timestamp relative to program startup.
 */
RTR3DECL(uint64_t)  RTTimeProgramMilliTS(void)
{
/*    AssertMsg(g_u64ProgramStartMilliTS, ("RTR3Init hasn't been called!\n")); */
    return RTTimeMilliTS() - g_u64ProgramStartMilliTS;
}


/**
 * Get the second timestamp relative to program startup.
 *
 * @returns Timestamp relative to program startup.
 */
RTR3DECL(uint32_t)  RTTimeProgramSecTS(void)
{
    AssertMsg(g_u64ProgramStartMilliTS, ("RTR3Init hasn't been called!\n"));
    return (uint32_t)(RTTimeProgramMilliTS() / 1000);
}
