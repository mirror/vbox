/* $Id$ */
/** @file
 * innotek Portable Runtime - Multiprocessor, Ring-0 Driver, Linux.
 */

/*
 * Copyright (C) 2008 innotek GmbH
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
#include "the-linux-kernel.h"

#include <iprt/mp.h>
#include <iprt/err.h>
#include <iprt/asm.h>
#include "r0drv/mp-r0drv.h"


RTDECL(RTCPUID) RTMpCpuId(void)
{
    return smp_processor_id();
}


/** 
 * Wrapper between the native linux per-cpu callbacks and PFNRTWORKER 
 * 
 * @param   pvInfo      Pointer to the RTMPARGS package.
 */
static void rtmpLinuxWrapper(void *pvInfo)
{
    PRTMPARGS pArgs = (PRTMPARGS)pvInfo;
    ASMAtomicIncU32(&pArgs->cHits);
    pArgs->pfnWorker(smp_process_id(), pArgs->pvUser1, pArgs->pvUser2);
}


RTDECL(int) RTMpOnAll(PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2)
{
    int rc;
    RTMPARGS Args;
    Args.pfnWorker = pfnWorker;
    Args.pvUser1 = pvUser1;
    Args.pvUser2 = pvUser2;
    Args.idCpu = NIL_RTCPUID;
    Args.cHits = 0;

#if 1 /** @todo the proper checks for 2.6.x. */
    rc = on_each_cpu(rtmpLinuxWrapper, &Args, 0 /* retry */, 1 /* wait */);

#else /* older kernels */

# ifdef preempt_disable
    preempt_disable();
# endif
    rc = smp_call_function(rtmpLinuxWrapper, &Args, 0 /* retry */, 1 /* wait */);
    local_irq_disable();
    rtmpLinuxWrapper(&Args);
    local_irq_enable();
# ifdef preempt_enable
    preempt_enable();
# endif
#endif /* older kernels */
    Assert(rc == 0); NOREF(rc);
    return VINF_SUCCESS;
}


RTDECL(int) RTMpOnOthers(PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2)
{
    int rc;
    RTMPARGS Args;
    Args.pfnWorker = pfnWorker;
    Args.pvUser1 = pvUser1;
    Args.pvUser2 = pvUser2;
    Args.idCpu = NIL_RTCPUID;
    Args.cHits = 0;

# ifdef preempt_disable
    preempt_disable();
# endif
    rc = smp_call_function(rtmpLinuxWrapper, &Args, 0 /* retry */, 1 /* wait */);
# ifdef preempt_enable
    preempt_enable();
# endif

    Assert(rc == 0); NOREF(rc);
    return VINF_SUCCESS;
}


RTDECL(int) RTMpOnSpecific(RTCPUID idCpu, PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2)
{
    int rc;
    RTMPARGS Args;
    Args.pfnWorker = pfnWorker;
    Args.pvUser1 = pvUser1;
    Args.pvUser2 = pvUser2;
    Args.idCpu = idCpu;
    Args.cHits = 0;

    /** @todo validate idCpu . */

# ifdef preempt_disable
    preempt_disable();
# endif
    if (idCpu != RTMpCpuId())
        rc = smp_call_function_single(idCpu, rtmpLinuxWrapper, &Args, 0 /* retry */, 1 /* wait */);
    else
    {
        rtmpLinuxWrapper(&Args);
        rc = 0;
    }
# ifdef preempt_enable
    preempt_enable();
# endif

    Assert(rc == 0); NOREF(rc);
    return VINF_SUCCESS;
}

