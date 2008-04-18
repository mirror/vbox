/** @file
 * innotek Portable Runtime - Runtime Init/Term.
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

#ifndef ___iprt_initterm_h
#define ___iprt_initterm_h

#include <iprt/cdefs.h>
#include <iprt/types.h>

__BEGIN_DECLS

/** @defgroup grp_rt    innotek Portable Runtime APIs
 * @{
 */

/** @defgroup grp_rt_initterm  Init / Term
 * @{
 */

#ifdef IN_RING3
/**
 * Initalizes the runtime library.
 *
 * @returns iprt status code.
 *
 * @param   fInitSUPLib     Set if SUPInit() shall be called during init (default).
 *                          Clear if not to call it.
 * @param   cbReserve       The number of bytes of contiguous memory that should be reserved by
 *                          the runtime / support library.
 *                          Set this to 0 if no reservation is required. (default)
 *                          Set this to ~(size_t)0 if the maximum amount supported by the VM is to be
 *                          attempted reserved, or the maximum available.
 *                          This argument only applies if fInitSUPLib is true and we're in ring-3 HC.
 */
RTR3DECL(int) RTR3Init(
#ifdef __cplusplus
    bool fInitSUPLib = true,
    size_t cbReserve = 0
#else
    bool fInitSUPLib,
    size_t cbReserve
#endif
    );

/**
 * Terminates the runtime library.
 */
RTR3DECL(void) RTR3Term(void);
#endif


#ifdef IN_RING0
/**
 * Initalizes the ring-0 driver runtime library.
 *
 * @returns iprt status code.
 * @param   fReserved       Flags reserved for the future.
 */
RTR0DECL(int) RTR0Init(unsigned fReserved);

/**
 * Terminates the ring-0 driver runtime library.
 */
RTR0DECL(void) RTR0Term(void);
#endif

#ifdef IN_GC
/**
 * Initalizes the guest context runtime library.
 *
 * @returns iprt status code.
 *
 * @param   u64ProgramStartNanoTS  The startup timestamp.
 */
RTGCDECL(int) RTGCInit(uint64_t u64ProgramStartNanoTS);

/**
 * Terminates the guest context runtime library.
 */
RTGCDECL(void) RTGCTerm(void);
#endif


/** @} */

/** @} */

__END_DECLS


#endif

