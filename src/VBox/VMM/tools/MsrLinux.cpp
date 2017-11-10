/* $Id$ */
/** @file
 * MsrLinux - Linux-specific MSR access.
 */

/*
 * Copyright (C) 2013-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/ctype.h>
#include <iprt/thread.h>

#include <VBox/sup.h>
#include "VBoxCpuReport.h"

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>


#define MSR_DEV_NAME    "/dev/cpu/0/msr"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** The /dev/xxx/msr file descriptor. */
static int              g_fdMsr;


static int linuxMsrProberRead(uint32_t uMsr, RTCPUID idCpu, uint64_t *puValue, bool *pfGp)
{
    int  rc = VINF_SUCCESS;

    if (idCpu != NIL_RTCPUID)
        return VERR_INVALID_PARAMETER;

    if (g_fdMsr < 0)
        return VERR_INVALID_STATE;

    *pfGp = true;
    if (pread(g_fdMsr, puValue, sizeof(*puValue), uMsr) != sizeof(*puValue))
        rc = VERR_READ_ERROR;
    else
        *pfGp = false;

    return RT_SUCCESS(rc) && !pfGp;
}

static int linuxMsrProberWrite(uint32_t uMsr, RTCPUID idCpu, uint64_t uValue, bool *pfGp)
{
    int  rc = VINF_SUCCESS;

    if (idCpu != NIL_RTCPUID)
        return VERR_INVALID_PARAMETER;

    if (g_fdMsr < 0)
        return VERR_INVALID_STATE;

    *pfGp = true;
    if (pwrite(g_fdMsr, &uValue, sizeof(uValue), uMsr) != sizeof(uValue))
        rc = VERR_WRITE_ERROR;
    else
        *pfGp = false;

    return RT_SUCCESS(rc) && !pfGp;
}

static int linuxMsrProberModify(uint32_t uMsr, RTCPUID idCpu, uint64_t fAndMask, uint64_t fOrMask, PSUPMSRPROBERMODIFYRESULT pResult)
{
    int         rc = VINF_SUCCESS;
    uint64_t    uBefore, uWrite, uAfter;
    int         rcBefore, rcWrite, rcAfter, rcRestore;

    if (idCpu != NIL_RTCPUID)
        return VERR_INVALID_PARAMETER;

    if (g_fdMsr < 0)
        return VERR_INVALID_STATE;

#if 0
    vbCpuRepDebug("MSR %#x\n", uMsr);
    RTThreadSleep(10);
#endif
    rcBefore = pread(g_fdMsr, &uBefore, sizeof(uBefore), uMsr) != sizeof(uBefore);
    uWrite = (uBefore & fAndMask) | fOrMask;
    rcWrite = pwrite(g_fdMsr, &uWrite, sizeof(uWrite), uMsr);
    rcAfter = pread(g_fdMsr, &uAfter, sizeof(uAfter), uMsr) != sizeof(uAfter);
    rcRestore = pwrite(g_fdMsr, &uBefore, sizeof(uBefore), uMsr) != sizeof(uBefore);

#if 0
    vbCpuRepDebug("MSR: %#x, %#llx -> %#llx -> %#llx (%d/%d/%d/%d)\n",
                  uMsr, uBefore, uWrite, uAfter,
                  rcBefore, rcWrite != sizeof(uWrite), rcAfter, rcRestore);
#endif
    pResult->uBefore    = uBefore;
    pResult->uWritten   = uWrite;
    pResult->uAfter     = uAfter;
    pResult->fBeforeGp  = rcBefore;
    pResult->fModifyGp  = rcWrite != sizeof(uWrite);
    pResult->fAfterGp   = rcAfter;
    pResult->fRestoreGp = rcRestore;

    return rc;
}

static int linuxMsrProberTerm(void)
{
    if (g_fdMsr < 0)
        return VERR_INVALID_STATE;

    close(g_fdMsr);
    return VINF_SUCCESS;
}

int PlatformMsrProberInit(VBMSRFNS *fnsMsr, bool *pfAtomicMsrMod)
{
    if (access(MSR_DEV_NAME, F_OK))
    {
        vbCpuRepDebug("warning: The " MSR_DEV_NAME " device does not exist\n");
        return VERR_NOT_FOUND;
    }

    g_fdMsr = open(MSR_DEV_NAME, O_RDWR);
    if (g_fdMsr <= 0)
    {
        vbCpuRepDebug("warning: Failed to open " MSR_DEV_NAME "\n");
        return VERR_ACCESS_DENIED;
    }

    fnsMsr->msrRead       = linuxMsrProberRead;
    fnsMsr->msrWrite      = linuxMsrProberWrite;
    fnsMsr->msrModify     = linuxMsrProberModify;
    fnsMsr->msrProberTerm = linuxMsrProberTerm;
    *pfAtomicMsrMod       = false;  /* Can't modify/restore MSRs without trip to R3. */

    return VINF_SUCCESS;
}
