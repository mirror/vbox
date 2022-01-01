/* $Id$ */
/** @file
 * Runtime VBox - RTLogCreateEx.
 */

/*
 * Copyright (C) 2006-2022 Oracle Corporation
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
#include <iprt/log.h>
#include "internal/iprt.h"

#include <iprt/stdarg.h>



RTDECL(int) RTLogCreateEx(PRTLOGGER *ppLogger, const char *pszEnvVarBase, uint64_t fFlags, const char *pszGroupSettings,
                          unsigned cGroups, const char * const *papszGroups, uint32_t cMaxEntriesPerGroup,
                          uint32_t cBufDescs, PRTLOGBUFFERDESC paBufDescs, uint32_t fDestFlags,
                          PFNRTLOGPHASE pfnPhase, uint32_t cHistory, uint64_t cbHistoryFileMax, uint32_t cSecsHistoryTimeSlot,
                          PRTERRINFO pErrInfo, const char *pszFilenameFmt, ...)
{
    va_list va;
    int     rc;

    va_start(va, pszFilenameFmt);
    rc = RTLogCreateExV(ppLogger, pszEnvVarBase, fFlags, pszGroupSettings, cGroups, papszGroups, cMaxEntriesPerGroup,
                        cBufDescs, paBufDescs, fDestFlags,
                        pfnPhase, cHistory, cbHistoryFileMax, cSecsHistoryTimeSlot,
                        pErrInfo, pszFilenameFmt, va);
    va_end(va);
    return rc;
}
RT_EXPORT_SYMBOL(RTLogCreateEx);

