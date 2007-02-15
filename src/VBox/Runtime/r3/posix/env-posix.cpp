/* $Id$ */
/** @file
 * InnoTek Portable Runtime - Environment, Posix.
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/env.h>
#include <iprt/string.h>

#include <stdlib.h>
#include <errno.h>


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
RTDECL(const char *) RTEnvGet(const char *pszVar)
{
    return getenv(pszVar);
}


/**
 * Puts an variable=value string into the environment (putenv).
 * 
 * @returns IPRT status code. Typical error is VERR_NO_MEMORY.
 * 
 * @param   pszVarEqualValue    The variable '=' value string. If the value and '=' is 
 *                              omitted, the variable is removed from the environment.
 */
RTDECL(int) RTEnvPut(const char *pszVarEqualValue)
{
    /** @todo putenv is a memory leak. deal with this on a per system basis. */
    if (!putenv((char *)pszVarEqualValue))
        return 0;
    return RTErrConvertFromErrno(errno);
}

