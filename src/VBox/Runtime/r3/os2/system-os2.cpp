/* $Id$ */
/** @file
 * InnoTek Portable Runtime - System, OS/2.
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
#define INCL_BASE
#define INCL_ERRORS
#include <os2.h>
#undef RT_MAX

#include <iprt/system.h>
#include <iprt/assert.h>


RTDECL(unsigned) RTSystemProcessorGetCount(void)
{
    ULONG cCpus = 1;
    int rc = DosQuerySysInfo(QSV_NUMPROCESSORS, QSV_NUMPROCESSORS, &cCpus, sizeof(cCpus));
    if (rc || !cCpus)
        cCpus = 1;
    return cCpus;
}


RTR3DECL(uint64_t) RTSystemProcessorGetActiveMask(void)
{
    union
    {
        uint64_t u64;
        MPAFFINITY mpaff;
    } u;

    int rc = DosQueryThreadAffinity(AFNTY_SYSTEM, &u.mpaff);
    if (rc)
        u.u64 = 1;
    return u.u64;
}

