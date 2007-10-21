/* $Id$ */
/** @file
 * innotek Portable Runtime - System, Win32.
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
#define LOG_GROUP RTLOGGROUP_SYSTEM
#include <Windows.h>
#include <iprt/system.h>
#include <iprt/assert.h>



RTDECL(unsigned) RTSystemProcessorGetCount(void)
{
    SYSTEM_INFO SysInfo;

    GetSystemInfo(&SysInfo);

    unsigned cCpus = (unsigned)SysInfo.dwNumberOfProcessors;
    Assert((DWORD)cCpus == SysInfo.dwNumberOfProcessors);
    return cCpus;
}


RTDECL(uint64_t) RTSystemProcessorGetActiveMask(void)
{
    SYSTEM_INFO SysInfo;

    GetSystemInfo(&SysInfo);

    return SysInfo.dwActiveProcessorMask;
}

