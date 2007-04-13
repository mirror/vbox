/* $Id$ */
/** @file
 * TM - Timeout Manager, CPU Time, All Contexts.
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
#define LOG_GROUP LOG_GROUP_TM
#include <VBox/tm.h>
#include "TMInternal.h"
#include <VBox/vm.h>
#include <VBox/sup.h>

#include <VBox/param.h>
#include <VBox/err.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/log.h>


/**
 * Gets the raw cpu tick from current virtual time.
 */
DECLINLINE(uint64_t) tmCpuTickGetRawVirtual(PVM pVM)
{
    uint64_t u64 = TMVirtualGet(pVM);
    if (u64 != TMCLOCK_FREQ_VIRTUAL)
        u64 = ASMMultU64ByU32DivByU32(u64, pVM->tm.s.cTSCTicksPerSecond, TMCLOCK_FREQ_VIRTUAL);
    return u64;
}


/**
 * Resumes the CPU timestamp counter ticking.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
TMDECL(int) TMCpuTickResume(PVM pVM)
{
    if (!pVM->tm.s.fTSCTicking)
    {
        pVM->tm.s.fTSCTicking = true;
        if (pVM->tm.s.fTSCVirtualized)
        {
            if (pVM->tm.s.fTSCUseRealTSC)
                pVM->tm.s.u64TSCOffset = ASMReadTSC() - pVM->tm.s.u64TSC;
            else
                pVM->tm.s.u64TSCOffset = tmCpuTickGetRawVirtual(pVM) - pVM->tm.s.u64TSC;
        }
        return VINF_SUCCESS;
    }
    AssertFailed();
    return VERR_INTERNAL_ERROR;
}


/**
 * Pauses the CPU timestamp counter ticking.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
TMDECL(int) TMCpuTickPause(PVM pVM)
{
    if (pVM->tm.s.fTSCTicking)
    {
        pVM->tm.s.u64TSC = TMCpuTickGet(pVM);
        pVM->tm.s.fTSCTicking = false;
        return VINF_SUCCESS;
    }
    AssertFailed();
    return VERR_INTERNAL_ERROR;
}


/**
 * Returns the TSC offset
 *
 * @returns TSC ofset
 * @param   pVM         The VM to operate on.
 */
TMDECL(uint64_t) TMCpuTickGetOffset(PVM pVM)
{
    return pVM->tm.s.u64TSCOffset;
}


/**
 * Read the current CPU timstamp counter.
 *
 * @returns Gets the CPU tsc.
 * @param   pVM         The VM to operate on.
 */
TMDECL(uint64_t) TMCpuTickGet(PVM pVM)
{
    uint64_t u64;
    if (RT_LIKELY(pVM->tm.s.fTSCTicking))
    {
        if (pVM->tm.s.fTSCVirtualized)
        {
            if (pVM->tm.s.fTSCUseRealTSC)
                u64 = ASMReadTSC();
            else
                u64 = tmCpuTickGetRawVirtual(pVM);
            u64 -= pVM->tm.s.u64TSCOffset;
        }
        else
            u64 = ASMReadTSC();
    }
    else
        u64 = pVM->tm.s.u64TSC;
    return u64;
}


/**
 * Sets the current CPU timestamp counter.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   u64Tick     The new timestamp value.
 */
TMDECL(int) TMCpuTickSet(PVM pVM, uint64_t u64Tick)
{
    Assert(!pVM->tm.s.fTSCTicking);
    pVM->tm.s.u64TSC = u64Tick;
    return VINF_SUCCESS;
}


/**
 * Get the timestamp frequency.
 *
 * @returns Number of ticks per second.
 * @param   pVM     The VM.
 */
TMDECL(uint64_t) TMCpuTicksPerSecond(PVM pVM)
{
    if (pVM->tm.s.fTSCUseRealTSC)
    {
        uint64_t cTSCTicksPerSecond = SUPGetCpuHzFromGIP(g_pSUPGlobalInfoPage);
        if (RT_LIKELY(cTSCTicksPerSecond != ~(uint64_t)0))
            return cTSCTicksPerSecond;
    }
    return pVM->tm.s.cTSCTicksPerSecond;
}

