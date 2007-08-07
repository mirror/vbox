/** @file
 * innotek Portable Runtime - Process Environment Strings.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
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


/**
 * Checks if an environment variable exists.
 * 
 * @returns IPRT status code. Typical error is VERR_NO_MEMORY.
 * 
 * @param   pszVar      The environment variable name.
 */
RTDECL(bool) RTEnvExist(const char *pszVar);

/**
 * Gets an environment variable (getenv).
 * 
 * The caller is responsible for ensuring that nobody changes the environment 
 * while it's using the returned string pointer!
 * 
 * @returns Pointer to read only string on success, NULL if the variable wasn't found.
 * 
 * @param   pszVar      The environment variable name.
 */
RTDECL(const char *) RTEnvGet(const char *pszVar);

/**
 * Puts an variable=value string into the environment (putenv).
 * 
 * @returns IPRT status code. Typical error is VERR_NO_MEMORY.
 * 
 * @param   pszVarEqualValue    The variable '=' value string. If the value and '=' is 
 *                              omitted, the variable is removed from the environment.
 */
RTDECL(int) RTEnvPut(const char *pszVarEqualValue);

/**
 * Sets an environment variable (setenv(,,1)).
 * 
 * @returns IPRT status code. Typical error is VERR_NO_MEMORY.
 * 
 * @param   pszVar      The environment variable name.
 * @param   pszValue    The environment variable value.
 */
RTDECL(int) RTEnvSet(const char *pszVar, const char *pszValue);

/** @todo Add the missing environment APIs: safe, printf like, and various modifications. */

/**
 * Creates an empty environment block.
 * 
 * @returns IPRT status code. Typical error is VERR_NO_MEMORY.
 * 
 * @param   pEnv        Where to store the handle of the environment block.
 */
RTDECL(int) RTEnvCreate(PRTENV pEnv);

/**
 * Destroys an environment block.
 *
 * @returns IPRT status code.
 *
 * @param   Env     Handle of the environment block.
 */
RTDECL(int) RTEnvDestroy(RTENV Env);

/**
 * Creates an environment block and fill it with variables from the given
 * environment array.
 *
 * @returns IPRT status code. Typical error is VERR_NO_MEMORY.
 *
 * @param   pEnv        Where to store the handle of the environment block.
 * @param   apszEnv     Pointer to the NULL-terminated array of environment
 *                      variables. If NULL, the current process' environment
 *                      will be cloned.
 */
RTDECL(int) RTEnvClone(PRTENV pEnv, char const *const *apszEnv);

/** @todo later */
/*
RTDECL(int) RTEnvCloneEx(PRTENV pEnv, const RTENV Env);

RTDECL(int) RTEnvExistEx(RTENV Env, const char *pszVar);
RTDECL(int) RTEnvSetEx(RTENV Env, const char *pszVar, const char *pszValue);
RTDECL(int) RTEnvGetEx(RTENV Env, const char *pszVar, const char **ppszValue);
RTDECL(int) RTEnvCountEx(RTENV Env, size_t *cbCount);
*/

/**
 * Puts a 'variable=value' string into the environment.
 *
 * The supplied string must be in the current process' codepage.
 * This function makes a copy of the supplied string.
 * 
 * @returns IPRT status code. Typical error is VERR_NO_MEMORY.
 * 
 * @param   Env                 Handle of the environment block.
 * @param   pszVarEqualValue    The variable '=' value string. If the value and '=' is 
 *                              omitted, the variable is removed from the environment.
 */
RTDECL(int) RTEnvPutEx(RTENV Env, const char *pszVarEqualValue);

/**
 * Returns a raw pointer to the array of environment variables of the given
 * environment block where every variable is a string in format
 * 'variable=value'.
 *
 * All returned strings are in the current process' codepage.
 * 
 * @returns Pointer to the raw array of environment variables.
 * @returns NULL if Env is NULL or invalid.
 * 
 * @param   Env                 Handle of the environment block.
 */
RTDECL(char const *const *) RTEnvGetArray(RTENV Env);

#endif /* IN_RING3 */

/** @} */

__END_DECLS

#endif

