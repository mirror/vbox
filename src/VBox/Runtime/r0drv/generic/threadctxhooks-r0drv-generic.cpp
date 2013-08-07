/* $Id$ */
/** @file
 * IPRT - Thread-Context Hooks, Ring-0 Driver, Generic.
 */

/*
 * Copyright (C) 2013 Oracle Corporation
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
#include <iprt/thread.h>
#include <iprt/err.h>

#include "internal/iprt.h"

RTDECL(int) RTThreadCtxHooksCreate(PRTTHREADCTX phThreadCtx)
{
    NOREF(phThreadCtx);
    return VERR_NOT_SUPPORTED;
}
RT_EXPORT_SYMBOL(RTThreadCtxHooksCreate);


RTDECL(uint32_t) RTThreadCtxHooksRelease(RTTHREADCTX hThreadCtx)
{
    NOREF(hThreadCtx);
    return UINT32_MAX;
}
RT_EXPORT_SYMBOL(RTThreadCtxHooksRelease);


RTDECL(uint32_t) RTThreadCtxHooksRetain(RTTHREADCTX hThreadCtx)
{
    NOREF(hThreadCtx);
    return UINT32_MAX;
}
RT_EXPORT_SYMBOL(RTThreadCtxHooksRetain);


RTDECL(int) RTThreadCtxHooksRegister(RTTHREADCTX hThreadCtx, PFNRTTHREADCTXHOOK pfnCallback, void *pvUser)
{
    NOREF(hThreadCtx);
    NOREF(pfnCallback);
    NOREF(pvUser);
    return VERR_NOT_SUPPORTED;
}
RT_EXPORT_SYMBOL(RTThreadCtxHooksRegister);


RTDECL(int) RTThreadCtxHooksDeregister(RTTHREADCTX hThreadCtx)
{
    NOREF(hThreadCtx);
    return VERR_NOT_SUPPORTED;
}
RT_EXPORT_SYMBOL(RTThreadCtxHooksDeregister);


RTDECL(bool) RTThreadCtxHooksAreRegistered(RTTHREADCTX hThreadCtx)
{
    NOREF(hThreadCtx);
    return false;
}
RT_EXPORT_SYMBOL(RTThreadCtxHooksAreRegistered);

