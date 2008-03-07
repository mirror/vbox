/** @file
 * innotek Portable Runtime - Multiprocessor.
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

#ifndef ___iprt_mp_h
#define ___iprt_mp_h

#include <iprt/cdefs.h>
#include <iprt/types.h>


__BEGIN_DECLS

/** @defgroup grp_rt_mp RTMp - Multiprocessor
 * @ingroup grp_rt
 * @{
 */

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
 * Converts a CPU identifier to a CPU set index.
 *
 * This may or may not validate the precense of the CPU.
 *
 * @returns The CPU set index on success, -1 on failure.
 * @param   idCpu       The identifier of the CPU.
 */
RTDECL(int) RTMpCpuIdToSetIndex(RTCPUID idCpu);

/**
 * Converts a CPU set index to a a CPU identifier.
 *
 * This may or may not validate the precense of the CPU, so, use
 * RTMpDoesCpuExist for that.
 *
 * @returns The corresponding CPU identifier, NIL_RTCPUID on failure.
 * @param   iCpu    The CPU set index.
 */
RTDECL(RTCPUID) RTMpCpuIdFromSetIndex(int iCpu);

/**
 * Gets the max CPU identifier (inclusive).
 *
 * Inteded for brute force enumerations, but use with
 * care as it may be expensive.
 *
 * @returns The current higest CPU identifier value.
 */
RTDECL(RTCPUID) RTMpGetMaxCpuId(void);

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
 * Gets set of the CPUs present in the system.
 *
 * This may or may not validate the precense of the CPU, so, use
 * RTMpDoesCpuExist for that.
 *
 * @returns pSet.
 * @param   pSet    Where to put the set.
 */
RTDECL(PRTCPUSET) RTMpGetSet(PRTCPUSET pSet);

/**
 * Get the count of CPUs presetn in the system.
 *
 * @return The count.
 */
RTDECL(RTCPUID) RTMpGetCount(void);

/**
 * Gets set of the CPUs present that are currently online.
 *
 * @returns pSet.
 * @param   pSet    Where to put the set.
 */
RTDECL(PRTCPUSET) RTMpGetOnlineSet(PRTCPUSET pSet);

/**
 * Get the count of CPUs that are currently online.
 *
 * @return The count.
 */
RTDECL(RTCPUID) RTMpGetOnlineCount(void);


#ifdef IN_RING0

/**
 * Worker function passed to RTMpOnAll, RTMpOnOthers and RTMpOnSpecific that
 * is to be called on the target cpus.
 *
 * @param   idCpu       The identifier for the CPU the function is called on.
 * @param   pvUser1     The 1st user argument.
 * @param   pvUser2     The 2nd user argument.
 */
typedef DECLCALLBACK(void) FNRTMPWORKER(RTCPUID idCpu, void *pvUser1, void *pvUser2);
/** Pointer to a FNRTMPWORKER. */
typedef FNRTMPWORKER *PFNRTMPWORKER;

/**
 * Executes a function on each (online) CPU in the system.
 *
 * @returns IPRT status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_NOT_SUPPORTED if this kind of operation isn't supported by the system.
 *
 * @param   pfnWorker       The worker function.
 * @param   pvUser1         The first user argument for the worker.
 * @param   pvUser2         The second user argument for the worker.
 *
 * @remarks The execution isn't in any way guaranteed to be simultaneous,
 *          it might even be serial (cpu by cpu).
 */
RTDECL(int) RTMpOnAll(PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2);

/**
 * Executes a function on a all other (online) CPUs in the system.
 *
 * @returns IPRT status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_NOT_SUPPORTED if this kind of operation isn't supported by the system.
 *
 * @param   pfnWorker       The worker function.
 * @param   pvUser1         The first user argument for the worker.
 * @param   pvUser2         The second user argument for the worker.
 *
 * @remarks The execution isn't in any way guaranteed to be simultaneous,
 *          it might even be serial (cpu by cpu).
 */
RTDECL(int) RTMpOnOthers(PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2);

/**
 * Executes a function on a specific CPU in the system.
 *
 * @returns IPRT status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_NOT_SUPPORTED if this kind of operation isn't supported by the system.
 * @retval  VERR_CPU_OFFLINE if the CPU is offline.
 * @retval  VERR_CPU_NOT_FOUND if the CPU wasn't found.
 *
 * @param   idCpu           The id of the CPU.
 * @param   pfnWorker       The worker function.
 * @param   pvUser1         The first user argument for the worker.
 * @param   pvUser2         The second user argument for the worker.
 */
RTDECL(int) RTMpOnSpecific(RTCPUID idCpu, PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2);

#endif /* IN_RING0 */

/** @} */

__END_DECLS

#endif

