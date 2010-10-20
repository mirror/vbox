/** @file
 * IPRT - Execute Once.
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
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

#ifndef ___iprt_once_h
#define ___iprt_once_h

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/err.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_rt_once       RTOnce - Execute Once
 * @ingroup grp_rt
 * @{
 */

/**
 * Execute once structure.
 *
 * This is typically a global variable that is statically initialized
 * by RTONCE_INITIALIZER.
 */
typedef struct RTONCE
{
    /** Event semaphore that the other guys are blocking on. */
    RTSEMEVENTMULTI volatile hEventMulti;
    /** Reference counter for hEventMulti. */
    int32_t volatile cEventRefs;
    /** -1 when uninitialized, 1 when initializing (busy) and 2 when done. */
    int32_t volatile iState;
    /** The return code of pfnOnce. */
    int32_t volatile rc;
} RTONCE;
/** Pointer to a execute once struct. */
typedef RTONCE *PRTONCE;

/**
 * The execute once statemachine.
 */
typedef enum RTONCESTATE
{
    /** RTOnce() has not been called.
     *  Next: NO_SEM */
    RTONCESTATE_UNINITIALIZED = 1,
    /** RTOnce() is busy, no race.
     *  Next: CREATING_SEM, DONE */
    RTONCESTATE_BUSY_NO_SEM,
    /** More than one RTOnce() caller is busy.
     *  Next: BUSY_HAVE_SEM, BUSY_SPIN, DONE_CREATING_SEM, DONE */
    RTONCESTATE_BUSY_CREATING_SEM,
    /** More than one RTOnce() caller, the first is busy, the others are
     *  waiting.
     *  Next: DONE */
    RTONCESTATE_BUSY_HAVE_SEM,
    /** More than one RTOnce() caller, the first is busy, the others failed to
     *  create a semaphore and are spinning.
     *  Next: DONE */
    RTONCESTATE_BUSY_SPIN,
    /** More than one RTOnce() caller, the first has completed, the others
     *  are busy creating the semaphore.
     *  Next: DONE_HAVE_SEM */
    RTONCESTATE_DONE_CREATING_SEM,
    /** More than one RTOnce() caller, the first is busy grabbing the
     *  semaphore, while the others are waiting.
     *  Next: DONE */
    RTONCESTATE_DONE_HAVE_SEM,
    /** The execute once stuff has completed. */
    RTONCESTATE_DONE = 16
} RTONCESTATE;

/** Static initializer for RTONCE variables. */
#define RTONCE_INITIALIZER      { NIL_RTSEMEVENTMULTI, 0, RTONCESTATE_UNINITIALIZED, VERR_INTERNAL_ERROR }


/**
 * Callback that gets executed once.
 *
 * @returns IPRT style status code, RTOnce returns this.
 *
 * @param   pvUser1         The first user parameter.
 * @param   pvUser2         The second user parameter.
 */
typedef DECLCALLBACK(int32_t) FNRTONCE(void *pvUser1, void *pvUser2);
/** Pointer to a FNRTONCE. */
typedef FNRTONCE *PFNRTONCE;

/**
 * Serializes execution of the pfnOnce function, making sure it's
 * executed exactly once and that nobody returns from RTOnce before
 * it has executed successfully.
 *
 * @returns IPRT like status code returned by pfnOnce.
 *
 * @param   pOnce           Pointer to the execute once variable.
 * @param   pfnOnce         The function to executed once.
 * @param   pvUser1         The first user parameter for pfnOnce.
 * @param   pvUser2         The second user parameter for pfnOnce.
 */
RTDECL(int) RTOnce(PRTONCE pOnce, PFNRTONCE pfnOnce, void *pvUser1, void *pvUser2);

/**
 * Resets an execute once variable.
 *
 * The caller is responsible for making sure there are no concurrent accesses to
 * the execute once variable.
 *
 * @param   pOnce           Pointer to the execute once variable.
 */
RTDECL(void) RTOnceReset(PRTONCE pOnce);

/** @} */

RT_C_DECLS_END

#endif

