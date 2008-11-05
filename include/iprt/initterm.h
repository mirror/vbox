/** @file
 * IPRT - Runtime Init/Term.
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

/** @defgroup grp_rt    IPRT APIs
 * @{
 */

/** @defgroup grp_rt_initterm  Init / Term
 * @{
 */

#ifdef IN_RING3
/**
 * Initializes the runtime library.
 *
 * @returns iprt status code.
 */
RTR3DECL(int) RTR3Init(void);


/**
 * Initializes the runtime library and try initialize SUPLib too.
 *
 * @returns IPRT status code.
 * @param   pszProgramPath      The path to the program file.
 *
 * @remarks Failure to initialize SUPLib is ignored.
 */
RTR3DECL(int) RTR3InitAndSUPLib(void);

/**
 * Initializes the runtime library passing it the program path.
 *
 * @returns IPRT status code.
 * @param   pszProgramPath      The path to the program file.
 */
RTR3DECL(int) RTR3InitWithProgramPath(const char *pszProgramPath);

/**
 * Initializes the runtime library passing it the program path,
 * and try initialize SUPLib too (failures ignored).
 *
 * @returns IPRT status code.
 * @param   pszProgramPath      The path to the program file.
 *
 * @remarks Failure to initialize SUPLib is ignored.
 */
RTR3DECL(int) RTR3InitAndSUPLibWithProgramPath(const char *pszProgramPath);

/**
 * Initializes the runtime library and possibly also SUPLib too.
 *
 * Avoid this interface, it's not considered stable.
 *
 * @returns IPRT status code.
 * @param   iVersion        The interface version. Must be 0 atm.
 * @param   pszProgramPath  The program path. Pass NULL if we're to figure it out ourselves.
 * @param   fInitSUPLib     Whether to initialize the support library or not.
 */
RTR3DECL(int) RTR3InitEx(uint32_t iVersion, const char *pszProgramPath, bool fInitSUPLib);

/**
 * Terminates the runtime library.
 */
RTR3DECL(void) RTR3Term(void);

#endif /* IN_RING3 */


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

#ifdef IN_RC
/**
 * Initializes the raw-mode context runtime library.
 *
 * @returns iprt status code.
 *
 * @param   u64ProgramStartNanoTS  The startup timestamp.
 */
RTGCDECL(int) RTRCInit(uint64_t u64ProgramStartNanoTS);

/**
 * Terminates the raw-mode context runtime library.
 */
RTGCDECL(void) RTRCTerm(void);
#endif


/** @} */

/** @} */

__END_DECLS


#endif

