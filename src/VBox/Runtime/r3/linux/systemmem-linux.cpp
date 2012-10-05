/* $Id$ */
/** @file
 * IPRT - RTSystemQueryTotalRam, Linux ring-3.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
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
#include <iprt/system.h>
#include "internal/iprt.h"

#include <iprt/err.h>
#include <iprt/assert.h>

#include <errno.h>

/* Satisfy compiller warning */
#define __EXPORTED_HEADERS__
#include <sys/sysinfo.h>
#undef __EXPORTED_HEADERS__


RTDECL(int) RTSystemQueryTotalRam(uint64_t *pcb)
{
    AssertPtrReturn(pcb, VERR_INVALID_POINTER);

    struct sysinfo info;
    int rc = sysinfo(&info);
    if (rc == 0)
    {
        *pcb = (uint64_t)(info.totalram * (unsigned long)info.mem_unit);
        return VINF_SUCCESS;
    }
    return RTErrConvertFromErrno(errno);
}


RTDECL(int) RTSystemQueryAvailableRam(uint64_t *pcb)
{
    AssertPtrReturn(pcb, VERR_INVALID_POINTER);

    struct sysinfo info;
    int rc = sysinfo(&info);
    if (rc == 0)
    {
        /* XXX Actually this is not quite correct. We would also need to add the cached
         *     RAM but this information is not available in sysinfo. */
        *pcb = (uint64_t)((info.freeram + info.bufferram) * (unsigned long)info.mem_unit);
        return VINF_SUCCESS;
    }
    return RTErrConvertFromErrno(errno);
}

