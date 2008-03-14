/* $Id$ */
/** @file
 * innotek Portable Runtime - Multiprocessor, Ring-0 Driver, NT.
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
#include "the-nt-kernel.h"

#include <iprt/mp.h>
#include <iprt/cpuset.h>
#include <iprt/err.h>
#include <iprt/asm.h>
#include "r0drv/mp-r0drv.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef enum
{
    RT_NT_CPUID_SPECIFIC,
    RT_NT_CPUID_OTHERS,
    RT_NT_CPUID_ALL
} RT_NT_CPUID;


/* test a couple of assumption. */
AssertCompile(MAXIMUM_PROCESSORS <= RTCPUSET_MAX_CPUS);
AssertCompile(NIL_RTCPUID >= MAXIMUM_PROCESSORS);

/** @todo
 * We cannot do other than assume a 1:1 relationship between the
 * affinity mask and the process despite the vagueness/warnings in
 * the docs. If someone knows a better way to get this done, please
 * let bird know.
 */


RTDECL(RTCPUID) RTMpCpuId(void)
{
    /* WDK upgrade warning: PCR->Number changed from BYTE to WORD. */
    return KeGetCurrentProcessorNumber();
}


RTDECL(int) RTMpCpuIdToSetIndex(RTCPUID idCpu)
{
    return idCpu < MAXIMUM_PROCESSORS ? idCpu : -1;
}


RTDECL(RTCPUID) RTMpCpuIdFromSetIndex(int iCpu)
{
    return (unsigned)iCpu < MAXIMUM_PROCESSORS ? iCpu : NIL_RTCPUID;
}


RTDECL(RTCPUID) RTMpGetMaxCpuId(void)
{
    return MAXIMUM_PROCESSORS - 1;
}


RTDECL(bool) RTMpIsCpuOnline(RTCPUID idCpu)
{
    if (idCpu >= MAXIMUM_PROCESSORS)
        return false;

    KAFFINITY Mask = KeQueryActiveProcessors();
    return !!(Mask & RT_BIT_64(idCpu));
}


RTDECL(bool) RTMpDoesCpuExist(RTCPUID idCpu)
{
    /* Cannot easily distinguish between online and offline cpus. */
    return RTMpIsCpuOnline(idCpu);
}


RTDECL(PRTCPUSET) RTMpGetSet(PRTCPUSET pSet)
{
    return RTMpGetOnlineSet(pSet);
}


RTDECL(RTCPUID) RTMpGetCount(void)
{
    return RTMpGetOnlineCount();
}


RTDECL(PRTCPUSET) RTMpGetOnlineSet(PRTCPUSET pSet)
{
    KAFFINITY Mask = KeQueryActiveProcessors();
    return RTCpuSetFromU64(pSet, Mask);
}


RTDECL(RTCPUID) RTMpGetOnlineCount(void)
{
    RTCPUSET Set;
    RTMpGetOnlineSet(&Set);
    return RTCpuSetCount(&Set);
}


/**
 * Wrapper between the native nt per-cpu callbacks and PFNRTWORKER
 *
 * @param   Dpc                 DPC object
 * @param   DeferredContext     Context argument specified by KeInitializeDpc
 * @param   SystemArgument1     Argument specified by KeInsertQueueDpc
 * @param   SystemArgument2     Argument specified by KeInsertQueueDpc
 */
static VOID rtmpNtDPCWrapper(IN PKDPC Dpc, IN PVOID DeferredContext, IN PVOID SystemArgument1, IN PVOID SystemArgument2)
{
    PRTMPARGS pArgs = (PRTMPARGS)DeferredContext;
    ASMAtomicIncU32(&pArgs->cHits);
    pArgs->pfnWorker(KeGetCurrentProcessorNumber(), pArgs->pvUser1, pArgs->pvUser2);
}


/**
 * Internal worker for the RTMpOn* APIs.
 *
 * @returns IPRT status code.
 * @param   pfnWorker       The callback.
 * @param   pvUser1         User argument 1.
 * @param   pvUser2         User argument 2.
 * @param   enmCpuid        What to do / is idCpu valid.
 * @param   idCpu           Used if enmCpuid RT_NT_CPUID_SPECIFIC, otherwise ignored.
 */
static int rtMpCall(PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2, RT_NT_CPUID enmCpuid, RTCPUID idCpu)
{
    PRTMPARGS pArgs;
    KDPC     *paExecCpuDpcs;

    /* KeFlushQueuedDpcs must be run at IRQL PASSIVE_LEVEL */
    AssertMsg(KeGetCurrentIrql() == PASSIVE_LEVEL, ("%d != %d (PASSIVE_LEVEL)\n", KeGetCurrentIrql(), PASSIVE_LEVEL));

    KAFFINITY Mask = KeQueryActiveProcessors();

    if (    enmCpuid == RT_NT_CPUID_SPECIFIC
        &&  !(Mask & RT_BIT_64(idCpu)))
        return VERR_CPU_NOT_FOUND;  /* can't distinguish between cpu not present or offline */

    /* KeFlushQueuedDpcs is not present in Windows 2000; import it dynamically so we can just fail this call. */
    UNICODE_STRING  RoutineName;
    RtlInitUnicodeString(&RoutineName, L"KeFlushQueuedDpcs");
    VOID (*pfnKeFlushQueuedDpcs)(VOID) = (VOID (*)(VOID))MmGetSystemRoutineAddress(&RoutineName);
    if (!pfnKeFlushQueuedDpcs)
        return VERR_NOT_SUPPORTED;

    pArgs = (PRTMPARGS)ExAllocatePoolWithTag(NonPagedPool, MAXIMUM_PROCESSORS*sizeof(KDPC) + sizeof(RTMPARGS), (ULONG)'RTMp');
    if (!pArgs)
        return VERR_NO_MEMORY;

    pArgs->pfnWorker = pfnWorker;
    pArgs->pvUser1   = pvUser1;
    pArgs->pvUser2   = pvUser2;
    pArgs->idCpu     = NIL_RTCPUID;
    pArgs->cHits     = 0;

    paExecCpuDpcs = (KDPC *)(pArgs + 1);

    if (enmCpuid == RT_NT_CPUID_SPECIFIC)
    {
        KeInitializeDpc(&paExecCpuDpcs[0], rtmpNtDPCWrapper, pArgs);
        KeSetImportanceDpc(&paExecCpuDpcs[0], HighImportance);
        KeSetTargetProcessorDpc(&paExecCpuDpcs[0], idCpu);
    }
    else
    {
        for (unsigned i = 0; i < MAXIMUM_PROCESSORS; i++)
        {
            KeInitializeDpc(&paExecCpuDpcs[i], rtmpNtDPCWrapper, pArgs);
            KeSetImportanceDpc(&paExecCpuDpcs[i], HighImportance);
            KeSetTargetProcessorDpc(&paExecCpuDpcs[i], i);
        }
    }

    /* Raise the IRQL to DISPATCH_LEVEL so we can't be rescheduled to another cpu.
     * KeInsertQueueDpc must also be executed at IRQL >= DISPATCH_LEVEL.
     */
    KIRQL oldIrql;
    KeRaiseIrql(DISPATCH_LEVEL, &oldIrql);

    /*
     * We cannot do other than assume a 1:1 relationship between the
     * affinity mask and the process despite the warnings in the docs.
     * If someone knows a better way to get this done, please let bird know.
     */
    if (enmCpuid == RT_NT_CPUID_SPECIFIC)
    {
        BOOLEAN ret = KeInsertQueueDpc(&paExecCpuDpcs[0], 0, 0);
        Assert(ret);
    }
    else
    {
        unsigned iSelf = KeGetCurrentProcessorNumber();

        for (unsigned i = 0; i < MAXIMUM_PROCESSORS; i++)
        {
            if (    (i != iSelf || enmCpuid != RT_NT_CPUID_OTHERS)
                &&  (Mask & RT_BIT_64(i)))
            {
                BOOLEAN ret = KeInsertQueueDpc(&paExecCpuDpcs[i], 0, 0);
                Assert(ret);
            }
        }
    }

    KeLowerIrql(oldIrql);

    /* Flush all DPCs and wait for completion. (can take long!) */
    pfnKeFlushQueuedDpcs();

    ExFreePool(pArgs);
    return VINF_SUCCESS;
}

RTDECL(int) RTMpOnAll(PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2)
{
    return rtMpCall(pfnWorker, pvUser1, pvUser2, RT_NT_CPUID_ALL, 0);
}

RTDECL(int) RTMpOnOthers(PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2)
{
    return rtMpCall(pfnWorker, pvUser1, pvUser2, RT_NT_CPUID_OTHERS, 0);
}

RTDECL(int) RTMpOnSpecific(RTCPUID idCpu, PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2)
{
    return rtMpCall(pfnWorker, pvUser1, pvUser2, RT_NT_CPUID_SPECIFIC, idCpu);
}

