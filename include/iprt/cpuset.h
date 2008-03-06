/** @file
 * innotek Portable Runtime - CPU Set.
 */

/*
 * Copyright (C) 2008 innotek GmbH
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

#ifndef ___iprt_cpuset_h
#define ___iprt_cpuset_h

#include <iprt/types.h>
#include <iprt/mp.h> /* RTMpCpuIdToSetIndex */


__BEGIN_DECLS

/** @defgroup grp_rt_mp RTCpuSet - CPU Set
 * @ingroup grp_rt
 * @{
 */

/**
 * Clear all CPUs.
 *
 * @param   pSet    Pointer to the set.
 */
DECLINLINE(void) RTCpuSetEmpty(PRTCPUSET pSet)
{
    *pSet = 0;
}


/**
 * Set all CPUs.
 *
 * @param   pSet    Pointer to the set.
 */
DECLINLINE(void) RTCpuSetFill(PRTCPUSET pSet)
{
    *pSet = UINT64_MAX;
}


/**
 * Adds a CPU given by it's identifier to the set.
 *
 * @returns 0 on success, -1 if idCpu isn't valid.
 * @param   pSet    Pointer to the set.
 * @param   idCpu   The identifier of the CPU to add.
 */
DECLINLINE(int) RTCpuSetAdd(PRTCPUSET pSet, RTCPUID idCpu)
{
    int iCpu = RTMpCpuIdToSetIndex(idCpu);
    if (RT_UNLIKELY(iCpu < 0))
        return -1;
    *pSet |= RT_BIT_64(iCpu);
    return 0;
}


/**
 * Removes a CPU given by it's identifier from the set.
 *
 * @returns 0 on success, -1 if idCpu isn't valid.
 * @param   pSet    Pointer to the set.
 * @param   idCpu   The identifier of the CPU to delete.
 */
DECLINLINE(int) RTCpuSetDel(PRTCPUSET pSet, RTCPUID idCpu)
{
    int iCpu = RTMpCpuIdToSetIndex(idCpu);
    if (RT_UNLIKELY(iCpu < 0))
        return -1;
    *pSet &= ~RT_BIT_64(iCpu);
    return 0;
}


/**
 * Checks if a CPU given by it's identifier is a member of the set.
 *
 * @returns true / false accordingly.
 * @param   pSet    Pointer to the set.
 * @param   idCpu   The identifier of the CPU to look for.
 */
DECLINLINE(int) RTCpuSetIsMember(PCRTCPUSET pSet, RTCPUID idCpu)
{
    int iCpu = RTMpCpuIdToSetIndex(idCpu);
    if (RT_UNLIKELY(iCpu < 0))
        return false;
    return !!(*pSet & RT_BIT_64(iCpu));
}


/**
 * Converts the CPU set to a 64-bit mask.
 *
 * @returns The mask.
 * @param   pSet    Pointer to the set.
 */
DECLINLINE(uint64_t) RTCpuSetToU64(PCRTCPUSET pSet)
{
    return *pSet;
}


/**
 * Initializes the CPU set from a 64-bit mask.
 *
 * @param   pSet    Pointer to the set.
 * @param   fMask   The mask.
 */
DECLINLINE(void) RTCpuSetFromU64(PRTCPUSET pSet, uint64_t fMask)
{
    *pSet = fMask;
}


/**
 * Gets the identifier of the CPU executing the call.
 *
 * When called from a system mode where scheduling is active, like ring-3 or
 * kernel mode with interrupts enabled on some systems, no assumptions should
 * be made about the current CPU when the call returns.
 *
 * @returns CPU Id.
 */
RTDECL(RTCPUID) RTMpCpuId(void);

/**
 * Checks if a CPU is online or not.
 *
 * @returns true/false accordingly.
 * @param   idCpu       The identifier of the CPU.
 */
RTDECL(bool) RTMpIsCpuOnline(RTCPUID idCpu);

/**
 * Checks if a CPU exist or not / validates a CPU id.
 *
 * @returns true/false accordingly.
 * @param   idCpu       The identifier of the CPU.
 */
RTDECL(bool) RTMpDoesCpuExist(RTCPUID idCpu);

/**
 * Converts a CPU identifier to a CPU set index.
 *
 * @returns The CPU set index on success, -1 on failure.
 * @param   idCpu       The identifier of the CPU.
 */
RTDECL(int) RTMpCpuIdToSetIndex(RTCPUID idCpu);


/** @} */

__END_DECLS

#endif


