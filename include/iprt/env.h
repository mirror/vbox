/** @file
 * Incredibly Portable Runtime - Process Environment Strings.
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

#ifndef ___iprt_env_h
#define ___iprt_env_h

#include <iprt/cdefs.h>
#include <iprt/types.h>

__BEGIN_DECLS

/** @defgroup grp_rt_env    RTProc - Process Environment Strings
 * @ingroup grp_rt
 * @{
 */

#ifdef IN_RING3

/** Special handle that indicates the default process environment. */
#define RTENV_DEFAULT   ((RTENV)~(uintptr_t)0)

/**
 * Creates an empty environment block.
 *
 * @returns IPRT status code. Typical error is VERR_NO_MEMORY.
 *
 * @param   pEnv        Where to store the handle of the new environment block.
 */
RTDECL(int) RTEnvCreate(PRTENV pEnv);

/**
 * Creates an environment block and fill it with variables from the given
 * environment array.
 *
 * @returns IPRT status code. Typical error is VERR_NO_MEMORY.
 *
 * @param   pEnv        Where to store the handle of the new environment block.
 * @param   EnvToClone  The environment to clone.
 */
RTDECL(int) RTEnvClone(PRTENV pEnv, RTENV EnvToClone);

/**
 * Destroys an environment block.
 *
 * @returns IPRT status code.
 *
 * @param   Env     Environment block handle.
 *                  Both RTENV_DEFAULT and NIL_RTENV are silently ignored.
 */
RTDECL(int) RTEnvDestroy(RTENV Env);

/**
 * Get the execve/spawnve/main envp.
 *
 * All returned strings are in the current process' codepage.
 * This array is only valid until the next RTEnv call.
 *
 * @returns Pointer to the raw array of environment variables.
 * @returns NULL if Env is NULL or invalid.
 *
 * @param   Env     Environment block handle.
 */
RTDECL(char const * const *) RTEnvGetExecEnvP(RTENV Env);


/**
 * Checks if an environment variable exists in the default environment block.
 *
 * @returns IPRT status code. Typical error is VERR_NO_MEMORY.
 *
 * @param   pszVar      The environment variable name.
 * @remark  WARNING! The current implementation does not perform the appropriate
 *          codeset conversion. We'll figure this out when it becomes necessary.
 */
RTDECL(bool) RTEnvExist(const char *pszVar);

/**
 * Checks if an environment variable exists in a specific environment block.
 *
 * @returns IPRT status code. Typical error is VERR_NO_MEMORY.
 *
 * @param   Env         The environment handle.
 * @param   pszVar      The environment variable name.
 */
RTDECL(bool) RTEnvExistEx(RTENV Env, const char *pszVar);

/**
 * Gets an environment variable from the default environment block. (getenv).
 *
 * The caller is responsible for ensuring that nobody changes the environment
 * while it's using the returned string pointer!
 *
 * @returns Pointer to read only string on success, NULL if the variable wasn't found.
 *
 * @param   pszVar      The environment variable name.
 *
 * @remark  WARNING! The current implementation does not perform the appropriate
 *          codeset conversion. We'll figure this out when it becomes necessary.
 */
RTDECL(const char *) RTEnvGet(const char *pszVar);

/**
 * Gets an environment variable in a specific environment block.
 *
 * @returns IPRT status code.
 *
 * @param   Env         The environment handle.
 * @param   pszVar      The environment variable name.
 * @param   pszValue    Where to put the buffer.
 * @param   cbValue     The size of the value buffer.
 * @param   pcchActual  Returns the actual value string length. Optional.
 */
RTDECL(int) RTEnvGetEx(RTENV Env, const char *pszVar, char *pszValue, size_t cbValue, size_t *pcchActual);

/**
 * Puts an variable=value string into the environment (putenv).
 *
 * @returns IPRT status code. Typical error is VERR_NO_MEMORY.
 *
 * @param   pszVarEqualValue    The variable '=' value string. If the value and '=' is
 *                              omitted, the variable is removed from the environment.
 *
 * @remark Don't assume the value is copied.
 * @remark  WARNING! The current implementation does not perform the appropriate
 *          codeset conversion. We'll figure this out when it becomes necessary.
 */
RTDECL(int) RTEnvPut(const char *pszVarEqualValue);

/**
 * Puts a copy of the passed in 'variable=value' string into the environment block.
 *
 * @returns IPRT status code. Typical error is VERR_NO_MEMORY.
 *
 * @param   Env                 Handle of the environment block.
 * @param   pszVarEqualValue    The variable '=' value string. If the value and '=' is
 *                              omitted, the variable is removed from the environment.
 */
RTDECL(int) RTEnvPutEx(RTENV Env, const char *pszVarEqualValue);

/**
 * Sets an environment variable (setenv(,,1)).
 *
 * @returns IPRT status code. Typical error is VERR_NO_MEMORY.
 *
 * @param   pszVar      The environment variable name.
 * @param   pszValue    The environment variable value.
 *
 * @remark  WARNING! The current implementation does not perform the appropriate
 *          codeset conversion. We'll figure this out when it becomes necessary.
 */
RTDECL(int) RTEnvSet(const char *pszVar, const char *pszValue);

/**
 * Sets an environment variable (setenv(,,1)).
 *
 * @returns IPRT status code. Typical error is VERR_NO_MEMORY.
 *
 * @param   Env         The environment handle.
 * @param   pszVar      The environment variable name.
 * @param   pszValue    The environment variable value.
 */
RTDECL(int) RTEnvSetEx(RTENV Env, const char *pszVar, const char *pszValue);

/**
 * Removes an environment variable from the default environment block.
 *
 * @returns IPRT status code.
 * @returns VINF_ENV_VAR_NOT_FOUND if the variable was not found.
 *
 * @param   pszVar      The environment variable name.
 *
 * @remark  WARNING! The current implementation does not perform the appropriate
 *          codeset conversion. We'll figure this out when it becomes necessary.
 */
RTDECL(int) RTEnvUnset(const char *pszVar);

/**
 * Removes an environment variable from the specified environment block.
 *
 * @returns IPRT status code.
 * @returns VINF_ENV_VAR_NOT_FOUND if the variable was not found.
 *
 * @param   Env         The environment handle.
 * @param   pszVar      The environment variable name.
 */
RTDECL(int) RTEnvUnsetEx(RTENV Env, const char *pszVar);

#endif /* IN_RING3 */

/** @} */

__END_DECLS

#endif

