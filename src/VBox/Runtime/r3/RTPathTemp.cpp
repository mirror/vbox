/* $Id$ */
/** @file
 * IPRT - Path Manipulation, RTPathTemp.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "internal/iprt.h"
#include <iprt/path.h>

#include <iprt/assert.h>
#include <iprt/env.h>
#include <iprt/err.h>
#include <iprt/string.h>
#include "internal/path.h"


RTDECL(int) RTPathTemp(char *pszPath, size_t cchPath)
{
    /*
     * Try get it from the environment first.
     */
    static const char * const s_apszVars[] =
    {
        "IPRT_TMPDIR"
#if defined(RT_OS_WINDOWS)
        , "TMP", "TEMP", "USERPROFILE"
#elif defined(RT_OS_OS2)
        , "TMP", "TEMP", "TMPDIR"
#else
        , "TMPDIR"
#endif
    };
    for (size_t iVar = 0; iVar < RT_ELEMENTS(s_apszVars); iVar++)
    {
        int rc = RTEnvGetEx(RTENV_DEFAULT, s_apszVars[iVar], pszPath, cchPath, NULL);
        if (rc != VERR_ENV_VAR_NOT_FOUND)
            return rc;
    }

    /*
     * Here we should use some sane system default, instead we just use
     * the typical unix temp dir for now.
     */
    /** @todo Windows should default to the windows directory, see GetTempPath.
     * Some unixes has path.h and _PATH_TMP. There is also a question about
     * whether /var/tmp wouldn't be a better place...  */
    if (cchPath < sizeof("/tmp") )
        return VERR_BUFFER_OVERFLOW;
    memcpy(pszPath, "/tmp", sizeof("/tmp"));
    return VINF_SUCCESS;
}

