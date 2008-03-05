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

/** Maximum number of CPUs we support in one system. 
 * @remarks The current limit in Windows (affinity mask)
 *
 */
#define RT_MP_MAX_CPU                   64


/** A CPU identifier. 
 * @remarks This doesn't have to correspond to the APIC ID (intel/amd). Nor 
 *          does it have to correspond to the bits in the affinity mask, at
 *          least not until we've sorted out Windows NT. */
typedef RTHCUINTPTR RTCPUID;
/** Pointer to a CPU identifier. */
typedef RTCPUID *PRTCPUID;
/** Pointer to a const CPU identifier. */
typedef RTCPUID const *PCRTCPUID;
/** Nil CPU Id. */
#define NIL_RTCPUID ( (RTCPUID)~0 )

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

