/** @file
 * InnoTek Portable Runtime - Process Environment Strings.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef __iprt_env_h__
#define __iprt_env_h__

#include <iprt/cdefs.h>
#include <iprt/types.h>

__BEGIN_DECLS

/** @defgroup grp_rt_env    RTProc - Process Environment Strings
 * @ingroup grp_rt
 * @{
 */

#ifdef IN_RING3

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

/** @todo Add the missing environment APIs: safe, printf like, and various modifications. */

#endif /* IN_RING3 */

/** @} */

__END_DECLS

#endif

