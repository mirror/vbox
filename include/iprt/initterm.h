/** @file
 * IPRT - Runtime Init/Term.
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
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


/**
 * Termination reason.
 */
typedef enum RTTERMREASON
{
    /** Normal exit. iStatus contains the exit code. */
    RTTERMREASON_EXIT = 1,
    /** Any abnormal exit. iStatus is 0 and has no meaning. */
    RTTERMREASON_ABEND,
    /** Killed by a signal. The iStatus contains the signal number. */
    RTTERMREASON_SIGNAL,
    /** The IPRT module is being unloaded. iStatus is 0 and has no meaning. */
    RTTERMREASON_UNLOAD
} RTTERMREASON;

/** Whether lazy clean up is Okay or not.
 * When the process is exiting, it is a waste of time to for instance free heap
 * memory or close open files. OTOH, when the runtime is unloaded from the
 * process, it is important to release absolutely all resources to prevent
 * resource leaks. */
#define RTTERMREASON_IS_LAZY_CLEANUP_OK(enmReason)  ((enmReason) != RTTERMREASON_UNLOAD)


/**
 * IPRT termination callback function.
 *
 * @param   enmReason           The cause of the termination.
 * @param   iStatus             The meaning of this depends on enmReason.
 * @param   pvUser              User argument passed to RTTermRegisterCallback.
 */
typedef DECLCALLBACK(void) FNRTTERMCALLBACK(RTTERMREASON enmReason, int32_t iStatus, void *pvUser);
/** Pointer to an IPRT termination callback function. */
typedef FNRTTERMCALLBACK *PFNRTTERMCALLBACK;


/**
 * Registers a termination callback.
 *
 * This is intended for performing clean up during IPRT termination. Frequently
 * paired with lazy initialization thru RTOnce.
 *
 * The callbacks are called in LIFO order.
 *
 * @returns IPRT status code.
 *
 * @param   pfnCallback         The callback function.
 * @param   pvUser              The user argument for the callback.
 *
 * @remarks May need to acquire a fast mutex or critical section, so use with
 *          some care in ring-0 context.
 *
 * @remarks Be very careful using this from code that may be unloaded before
 *          IPRT terminates. Unlike some atexit and on_exit implementations,
 *          IPRT will not automatically unregister callbacks when a module gets
 *          unloaded.
 */
RTDECL(int) RTTermRegisterCallback(PFNRTTERMCALLBACK pfnCallback, void *pvUser);

/**
 * Deregister a termination callback.
 *
 * @returns VINF_SUCCESS if found, VERR_NOT_FOUND if the callback/pvUser pair
 *          wasn't found.
 *
 * @param   pfnCallback         The callback function.
 * @param   pvUser              The user argument for the callback.
 */
RTDECL(int) RTTermDeregisterCallback(PFNRTTERMCALLBACK pfnCallback, void *pvUser);

/**
 * Runs the termination callback queue.
 *
 * Normally called by an internal IPRT termination function, but may also be
 * called by external code immediately prior to terminating IPRT if it is in a
 * better position to state the termination reason and/or status.
 *
 * @param   enmReason           The reason why it's called.
 * @param   iStatus             The associated exit status or signal number.
 */
RTDECL(void) RTTermRunCallbacks(RTTERMREASON enmReason, int32_t iStatus);

/** @} */

/** @} */

__END_DECLS


#endif

