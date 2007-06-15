/* $Id$ */
/** @file
 * innotek Portable Runtime - Init Guest Context.
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
#define LOG_GROUP RTLOGGROUP_DEFAULT

#include <iprt/initterm.h>
#include <iprt/time.h>
#include <iprt/log.h>
#include <iprt/err.h>
#include <iprt/string.h>
#include "internal/time.h"


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/**
 * Program start nanosecond TS.
 */
uint64_t    g_u64ProgramStartNanoTS;

/**
 * Program start microsecond TS.
 */
uint64_t    g_u64ProgramStartMicroTS;

/**
 * Program start millisecond TS.
 */
uint64_t    g_u64ProgramStartMilliTS;


/**
 * Initalizes the guest context runtime library. 
 *
 * @returns iprt status code.
 *
 * @param   u64ProgramStartNanoTS  The startup timestamp.
 */
RTGCDECL(int) RTGCInit(uint64_t u64ProgramStartNanoTS)
{
    /*
     * Init the program start TSes.
     */
    g_u64ProgramStartNanoTS = u64ProgramStartNanoTS;
    g_u64ProgramStartMicroTS = u64ProgramStartNanoTS / 1000;
    g_u64ProgramStartMilliTS = u64ProgramStartNanoTS / 1000000;

    LogFlow(("RTGCInit: returns VINF_SUCCESS\n"));
    return VINF_SUCCESS;
}


/**
 * Terminates the guest context runtime library.
 */
RTGCDECL(void) RTGCTerm(void)
{
    /* do nothing */
}
