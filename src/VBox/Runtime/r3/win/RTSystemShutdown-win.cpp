/* $Id$ */
/** @file
 * IPRT - RTSystemShutdown, Windows.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/system.h>
#include "internal/iprt.h"

#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/string.h>


#include <Windows.h>

RTDECL(int) RTSystemShutdown(RTMSINTERVAL cMsDelay, uint32_t fFlags, const char *pszLogMsg)
{
    AssertPtrReturn(pszLogMsg, VERR_INVALID_POINTER);
    AssertReturn(!(fFlags & ~RTSYSTEM_SHUTDOWN_VALID_MASK), VERR_INVALID_PARAMETER);
    
    PRTUTF16 pwszLogMsg;
    int rc = RTStrToUtf16(pszLogMsg, &pwszLogMsg);
    if (RT_FAILURE(rc))
        return rc;
    DWORD cSecsTimeout = (cMsDelay + 499) / 1000;
    BOOL  fRebootAfterShutdown = (fFlags & RTSYSTEM_SHUTDOWN_ACTION_MASK) == RTSYSTEM_SHUTDOWN_REBOOT
                               ? TRUE : FALSE;
    BOOL  fForceAppsClosed = fFlags & RTSYSTEM_SHUTDOWN_FORCE ? TRUE : FALSE;
    if (InitiateSystemShutdownW(NULL /*pwszMachineName = NULL = localhost*/, 
                                pwszLogMsg,
                                cSecsTimeout,
                                fForceAppsClosed,
                                fRebootAfterShutdown))
        return (fFlags & RTSYSTEM_SHUTDOWN_ACTION_MASK) == RTSYSTEM_SHUTDOWN_HALT ? VINF_SYS_MAY_POWER_OFF : VINF_SUCCESS;
    return RTErrConvertFromWin32(GetLastError());
}
RT_EXPORT_SYMBOL(RTSystemShutdown);

