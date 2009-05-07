/** @file
 * IPRT - System Information.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___iprt_system_h
#define ___iprt_system_h

#include <iprt/cdefs.h>
#include <iprt/types.h>


__BEGIN_DECLS

/** @defgroup grp_rt_system RTSystem - System Information
 * @ingroup grp_rt
 * @{
 */

/**
 * Info level for RTSystemGetOSInfo().
 */
typedef enum RTSYSOSINFO
{
    RTSYSOSINFO_INVALID = 0,    /**< The usual invalid entry. */
    RTSYSOSINFO_PRODUCT,        /**< OS product name. (uname -o) */
    RTSYSOSINFO_RELEASE,        /**< OS release. (uname -r) */
    RTSYSOSINFO_VERSION,        /**< OS version, optional. (uname -v) */
    RTSYSOSINFO_SERVICE_PACK,   /**< Service/fix pack level, optional. */
    RTSYSOSINFO_END             /**< End of the valid info levels. */
} RTSYSOSINFO;


/**
 * Queries information about the OS.
 *
 * @returns IPRT status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_INVALID_PARAMETER if enmInfo is invalid.
 * @retval  VERR_INVALID_POINTER if pszInfoStr is invalid.
 * @retval  VERR_BUFFER_OVERFLOW if the buffer is too small. The buffer will
 *          contain the chopped off result in this case, provided cchInfo isn't 0.
 * @retval  VERR_NOT_SUPPORTED if the info level isn't implemented. The buffer will
 *          contain an empty string.
 *
 * @param   enmInfo         The OS info level.
 * @param   pszInfo         Where to store the result.
 * @param   cchInfo         The size of the output buffer.
 */
RTDECL(int) RTSystemQueryOSInfo(RTSYSOSINFO enmInfo, char *pszInfo, size_t cchInfo);

/**
 * Queries the total amount of RAM accessible to the system.
 *
 * This figure should not include memory that is installed but not used,
 * nor memory that will be slow to bring online. The definition of 'slow'
 * here is slower than swapping out a MB of pages to disk.
 *
 * @returns IPRT status code.
 * @retval  VINF_SUCCESS and *pcb on sucess.
 * @retval  VERR_ACCESS_DENIED if the information isn't accessible to the
 *          caller.
 *
 * @param   pcb             Where to store the result (in bytes).
 */
RTDECL(int) RTSystemQueryTotalRam(uint64_t *pcb);

/**
 * Queries the amount of RAM that is currently locked down or in some other
 * way made impossible to virtualize within reasonably short time.
 *
 * The purposes of this API is, when combined with RTSystemQueryTotalRam, to
 * be able to determin an absolute max limit for how much fixed memory it is
 * (theoretically) possible to allocate (or lock down).
 *
 * The kind memory covered by this function includes:
 *      - locked (wired) memory - like for instance RTR0MemObjLockUser
 *        and RTR0MemObjLockKernel makes,
 *      - kernel pools and heaps - like for instance the ring-0 variant
 *        of RTMemAlloc taps into,
 *      - fixed (not pagable) kernel allocations - like for instance
 *        all the RTR0MemObjAlloc* functions makes,
 *      - any similar memory that isn't easily swapped out, discarded,
 *        or flushed to disk.
 *
 * This works against the value returned by RTSystemQueryTotalRam, and
 * the value reported by this function can never be larger than what a
 * call to RTSystemQueryTotalRam returns.
 *
 * The short time term here is relative to swapping to disk like in
 * RTSystemQueryTotalRam. This could mean that (part of) the dirty buffers
 * in the dynamic I/O cache could be included in the total. If the dynamic
 * I/O cache isn't likely to either flush buffers when the load increases
 * and put them back into normal circulation, they should be included in
 * the memory accounted for here.
 *
 * @retval  VINF_SUCCESS and *pcb on sucess.
 * @retval  VERR_NOT_SUPPORTED if the information isn't available on the
 *          system in general. The caller must handle this scenario.
 * @retval  VERR_ACCESS_DENIED if the information isn't accessible to the
 *          caller.
 *
 * @param   pcb             Where to store the result (in bytes).
 *
 * @remarks This function could've been inverted and called
 *          RTSystemQueryAvailableRam, but that might give impression that
 *          it would be possible to allocate the amount of memory it
 *          indicates for a single purpose, something which would be very
 *          improbable on most systems.
 *
 * @remarks We might have to add another output parameter to this function
 *          that indicates if some of the memory kinds listed above cannot
 *          be accounted for on the system and therefore is not include in
 *          the returned amount.
 */
RTDECL(int) RTSystemQueryUnavailableRam(uint64_t *pcb);


/** @} */

__END_DECLS

#endif

