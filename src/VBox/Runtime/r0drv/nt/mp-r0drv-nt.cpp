/* $Id$ */
/** @file
 * IPRT - Multiprocessor, Ring-0 Driver, NT.
 */

/*
 * Copyright (C) 2008-2016 Oracle Corporation
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
#include "the-nt-kernel.h"

#include <iprt/mp.h>
#include <iprt/cpuset.h>
#include <iprt/err.h>
#include <iprt/asm.h>
#include <iprt/log.h>
#include <iprt/time.h>
#include "r0drv/mp-r0drv.h"
#include "internal-r0drv-nt.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
typedef enum
{
    RT_NT_CPUID_SPECIFIC,
    RT_NT_CPUID_PAIR,
    RT_NT_CPUID_OTHERS,
    RT_NT_CPUID_ALL
} RT_NT_CPUID;


/**
 * Used by the RTMpOnSpecific.
 */
typedef struct RTMPNTONSPECIFICARGS
{
    /** Set if we're executing. */
    bool volatile       fExecuting;
    /** Set when done executing. */
    bool volatile       fDone;
    /** Number of references to this heap block. */
    uint32_t volatile   cRefs;
    /** Event that the calling thread is waiting on. */
    KEVENT              DoneEvt;
    /** The deferred procedure call object. */
    KDPC                Dpc;
    /** The callback argument package. */
    RTMPARGS            CallbackArgs;
} RTMPNTONSPECIFICARGS;
/** Pointer to an argument/state structure for RTMpOnSpecific on NT. */
typedef RTMPNTONSPECIFICARGS *PRTMPNTONSPECIFICARGS;



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


RTDECL(int) RTMpCurSetIndex(void)
{
    /* WDK upgrade warning: PCR->Number changed from BYTE to WORD. */
    return KeGetCurrentProcessorNumber();
}


RTDECL(int) RTMpCurSetIndexAndId(PRTCPUID pidCpu)
{
    return *pidCpu = KeGetCurrentProcessorNumber();
}


RTDECL(int) RTMpCpuIdToSetIndex(RTCPUID idCpu)
{
    return idCpu < MAXIMUM_PROCESSORS ? (int)idCpu : -1;
}


RTDECL(RTCPUID) RTMpCpuIdFromSetIndex(int iCpu)
{
    return (unsigned)iCpu < MAXIMUM_PROCESSORS ? iCpu : NIL_RTCPUID;
}


RTDECL(RTCPUID) RTMpGetMaxCpuId(void)
{
    /** @todo use KeQueryMaximumProcessorCount on vista+ */
    return MAXIMUM_PROCESSORS - 1;
}


RTDECL(bool) RTMpIsCpuOnline(RTCPUID idCpu)
{
    if (idCpu >= MAXIMUM_PROCESSORS)
        return false;

#if 0 /* this isn't safe at all IRQLs (great work guys) */
    KAFFINITY Mask = KeQueryActiveProcessors();
    return !!(Mask & RT_BIT_64(idCpu));
#else
    return RTCpuSetIsMember(&g_rtMpNtCpuSet, idCpu);
#endif
}


RTDECL(bool) RTMpIsCpuPossible(RTCPUID idCpu)
{
    /* Cannot easily distinguish between online and offline cpus. */
    /** @todo online/present cpu stuff must be corrected for proper W2K8 support
     *        (KeQueryMaximumProcessorCount). */
    return RTMpIsCpuOnline(idCpu);
}



RTDECL(PRTCPUSET) RTMpGetSet(PRTCPUSET pSet)
{
    /** @todo online/present cpu stuff must be corrected for proper W2K8 support
     *        (KeQueryMaximumProcessorCount). */
    return RTMpGetOnlineSet(pSet);
}


RTDECL(RTCPUID) RTMpGetCount(void)
{
    /** @todo online/present cpu stuff must be corrected for proper W2K8 support
     *        (KeQueryMaximumProcessorCount). */
    return RTMpGetOnlineCount();
}


RTDECL(PRTCPUSET) RTMpGetOnlineSet(PRTCPUSET pSet)
{
#if 0 /* this isn't safe at all IRQLs (great work guys) */
    KAFFINITY Mask = KeQueryActiveProcessors();
    return RTCpuSetFromU64(pSet, Mask);
#else
    *pSet = g_rtMpNtCpuSet;
    return pSet;
#endif
}


RTDECL(RTCPUID) RTMpGetOnlineCount(void)
{
    RTCPUSET Set;
    RTMpGetOnlineSet(&Set);
    return RTCpuSetCount(&Set);
}


#if 0
/* Experiment with checking the undocumented KPRCB structure
 * 'dt nt!_kprcb 0xaddress' shows the layout
 */
typedef struct
{
    LIST_ENTRY     DpcListHead;
    ULONG_PTR      DpcLock;
    volatile ULONG DpcQueueDepth;
    ULONG          DpcQueueCount;
} KDPC_DATA, *PKDPC_DATA;

RTDECL(bool) RTMpIsCpuWorkPending(void)
{
    uint8_t *pkprcb;
    PKDPC_DATA pDpcData;

    _asm {
        mov eax, fs:0x20
        mov pkprcb, eax
    }
    pDpcData = (PKDPC_DATA)(pkprcb + 0x19e0);
    if (pDpcData->DpcQueueDepth)
        return true;

    pDpcData++;
    if (pDpcData->DpcQueueDepth)
        return true;
    return false;
}
#else
RTDECL(bool) RTMpIsCpuWorkPending(void)
{
    /** @todo not implemented */
    return false;
}
#endif


/**
 * Wrapper between the native KIPI_BROADCAST_WORKER and IPRT's PFNRTMPWORKER for
 * the RTMpOnAll case.
 *
 * @param   uUserCtx            The user context argument (PRTMPARGS).
 */
static ULONG_PTR rtmpNtOnAllBroadcastIpiWrapper(ULONG_PTR uUserCtx)
{
    PRTMPARGS pArgs = (PRTMPARGS)uUserCtx;
    /*ASMAtomicIncU32(&pArgs->cHits); - not needed */
    pArgs->pfnWorker(KeGetCurrentProcessorNumber(), pArgs->pvUser1, pArgs->pvUser2);
    return 0;
}


/**
 * Wrapper between the native KIPI_BROADCAST_WORKER and IPRT's PFNRTMPWORKER for
 * the RTMpOnOthers case.
 *
 * @param   uUserCtx            The user context argument (PRTMPARGS).
 */
static ULONG_PTR rtmpNtOnOthersBroadcastIpiWrapper(ULONG_PTR uUserCtx)
{
    PRTMPARGS pArgs = (PRTMPARGS)uUserCtx;
    RTCPUID idCpu = KeGetCurrentProcessorNumber();
    if (pArgs->idCpu != idCpu)
    {
        /*ASMAtomicIncU32(&pArgs->cHits); - not needed */
        pArgs->pfnWorker(idCpu, pArgs->pvUser1, pArgs->pvUser2);
    }
    return 0;
}


/**
 * Wrapper between the native KIPI_BROADCAST_WORKER and IPRT's PFNRTMPWORKER for
 * the RTMpOnPair case.
 *
 * @param   uUserCtx            The user context argument (PRTMPARGS).
 */
static ULONG_PTR rtmpNtOnPairBroadcastIpiWrapper(ULONG_PTR uUserCtx)
{
    PRTMPARGS pArgs = (PRTMPARGS)uUserCtx;
    RTCPUID idCpu = KeGetCurrentProcessorNumber();
    if (   pArgs->idCpu  == idCpu
        || pArgs->idCpu2 == idCpu)
    {
        ASMAtomicIncU32(&pArgs->cHits);
        pArgs->pfnWorker(idCpu, pArgs->pvUser1, pArgs->pvUser2);
    }
    return 0;
}


/**
 * Wrapper between the native KIPI_BROADCAST_WORKER and IPRT's PFNRTMPWORKER for
 * the RTMpOnSpecific case.
 *
 * @param   uUserCtx            The user context argument (PRTMPARGS).
 */
static ULONG_PTR rtmpNtOnSpecificBroadcastIpiWrapper(ULONG_PTR uUserCtx)
{
    PRTMPARGS pArgs = (PRTMPARGS)uUserCtx;
    RTCPUID idCpu = KeGetCurrentProcessorNumber();
    if (pArgs->idCpu == idCpu)
    {
        ASMAtomicIncU32(&pArgs->cHits);
        pArgs->pfnWorker(idCpu, pArgs->pvUser1, pArgs->pvUser2);
    }
    return 0;
}


/**
 * Internal worker for the RTMpOn* APIs using KeIpiGenericCall.
 *
 * @returns VINF_SUCCESS.
 * @param   pfnWorker           The callback.
 * @param   pvUser1             User argument 1.
 * @param   pvUser2             User argument 2.
 * @param   pfnNativeWrapper    The wrapper between the NT and IPRT callbacks.
 * @param   idCpu               First CPU to match, ultimately specific to the
 *                              pfnNativeWrapper used.
 * @param   idCpu2              Second CPU to match, ultimately specific to the
 *                              pfnNativeWrapper used.
 * @param   pcHits              Where to return the number of this. Optional.
 */
static int rtMpCallUsingBroadcastIpi(PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2,
                                     PKIPI_BROADCAST_WORKER pfnNativeWrapper, RTCPUID idCpu, RTCPUID idCpu2,
                                     uint32_t *pcHits)
{
    RTMPARGS Args;
    Args.pfnWorker = pfnWorker;
    Args.pvUser1   = pvUser1;
    Args.pvUser2   = pvUser2;
    Args.idCpu     = idCpu;
    Args.idCpu2    = idCpu2;
    Args.cRefs     = 0;
    Args.cHits     = 0;

    AssertPtr(g_pfnrtKeIpiGenericCall);
    g_pfnrtKeIpiGenericCall(pfnNativeWrapper, (uintptr_t)&Args);
    if (pcHits)
        *pcHits = Args.cHits;
    return VINF_SUCCESS;
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

    /* Dereference the argument structure. */
    int32_t cRefs = ASMAtomicDecS32(&pArgs->cRefs);
    Assert(cRefs >= 0);
    if (cRefs == 0)
        ExFreePool(pArgs);
}


/**
 * Internal worker for the RTMpOn* APIs.
 *
 * @returns IPRT status code.
 * @param   pfnWorker   The callback.
 * @param   pvUser1     User argument 1.
 * @param   pvUser2     User argument 2.
 * @param   enmCpuid    What to do / is idCpu valid.
 * @param   idCpu       Used if enmCpuid is RT_NT_CPUID_SPECIFIC or
 *                      RT_NT_CPUID_PAIR, otherwise ignored.
 * @param   idCpu2      Used if enmCpuid is RT_NT_CPUID_PAIR, otherwise ignored.
 * @param   pcHits      Where to return the number of this. Optional.
 */
static int rtMpCallUsingDpcs(PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2,
                             RT_NT_CPUID enmCpuid, RTCPUID idCpu, RTCPUID idCpu2, uint32_t *pcHits)
{
    PRTMPARGS pArgs;
    KDPC     *paExecCpuDpcs;

#if 0
    /* KeFlushQueuedDpcs must be run at IRQL PASSIVE_LEVEL according to MSDN, but the
     * driver verifier doesn't complain...
     */
    AssertMsg(KeGetCurrentIrql() == PASSIVE_LEVEL, ("%d != %d (PASSIVE_LEVEL)\n", KeGetCurrentIrql(), PASSIVE_LEVEL));
#endif

#ifdef IPRT_TARGET_NT4
    KAFFINITY Mask;
    /* g_pfnrtNt* are not present on NT anyway. */
    return VERR_NOT_SUPPORTED;
#else
    KAFFINITY Mask = KeQueryActiveProcessors();
#endif

    /* KeFlushQueuedDpcs is not present in Windows 2000; import it dynamically so we can just fail this call. */
    if (!g_pfnrtNtKeFlushQueuedDpcs)
        return VERR_NOT_SUPPORTED;

    pArgs = (PRTMPARGS)ExAllocatePoolWithTag(NonPagedPool, MAXIMUM_PROCESSORS*sizeof(KDPC) + sizeof(RTMPARGS), (ULONG)'RTMp');
    if (!pArgs)
        return VERR_NO_MEMORY;

    pArgs->pfnWorker = pfnWorker;
    pArgs->pvUser1   = pvUser1;
    pArgs->pvUser2   = pvUser2;
    pArgs->idCpu     = NIL_RTCPUID;
    pArgs->idCpu2    = NIL_RTCPUID;
    pArgs->cHits     = 0;
    pArgs->cRefs     = 1;

    paExecCpuDpcs = (KDPC *)(pArgs + 1);

    if (enmCpuid == RT_NT_CPUID_SPECIFIC)
    {
        KeInitializeDpc(&paExecCpuDpcs[0], rtmpNtDPCWrapper, pArgs);
        KeSetImportanceDpc(&paExecCpuDpcs[0], HighImportance);
        KeSetTargetProcessorDpc(&paExecCpuDpcs[0], (int)idCpu);
        pArgs->idCpu = idCpu;
    }
    else if (enmCpuid == RT_NT_CPUID_PAIR)
    {
        KeInitializeDpc(&paExecCpuDpcs[0], rtmpNtDPCWrapper, pArgs);
        KeSetImportanceDpc(&paExecCpuDpcs[0], HighImportance);
        KeSetTargetProcessorDpc(&paExecCpuDpcs[0], (int)idCpu);
        pArgs->idCpu = idCpu;

        KeInitializeDpc(&paExecCpuDpcs[1], rtmpNtDPCWrapper, pArgs);
        KeSetImportanceDpc(&paExecCpuDpcs[1], HighImportance);
        KeSetTargetProcessorDpc(&paExecCpuDpcs[1], (int)idCpu2);
        pArgs->idCpu2 = idCpu2;
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
    ASMCompilerBarrier(); /* paranoia */
    if (enmCpuid == RT_NT_CPUID_SPECIFIC)
    {
        ASMAtomicIncS32(&pArgs->cRefs);
        BOOLEAN ret = KeInsertQueueDpc(&paExecCpuDpcs[0], 0, 0);
        Assert(ret);
    }
    else if (enmCpuid == RT_NT_CPUID_PAIR)
    {
        ASMAtomicIncS32(&pArgs->cRefs);
        BOOLEAN ret = KeInsertQueueDpc(&paExecCpuDpcs[0], 0, 0);
        Assert(ret);

        ASMAtomicIncS32(&pArgs->cRefs);
        ret = KeInsertQueueDpc(&paExecCpuDpcs[1], 0, 0);
        Assert(ret);
    }
    else
    {
        unsigned iSelf = KeGetCurrentProcessorNumber();

        for (unsigned i = 0; i < MAXIMUM_PROCESSORS; i++)
        {
            if (    (i != iSelf)
                &&  (Mask & RT_BIT_64(i)))
            {
                ASMAtomicIncS32(&pArgs->cRefs);
                BOOLEAN ret = KeInsertQueueDpc(&paExecCpuDpcs[i], 0, 0);
                Assert(ret);
            }
        }
        if (enmCpuid != RT_NT_CPUID_OTHERS)
            pfnWorker(iSelf, pvUser1, pvUser2);
    }

    KeLowerIrql(oldIrql);

    /* Flush all DPCs and wait for completion. (can take long!) */
    /** @todo Consider changing this to an active wait using some atomic inc/dec
     *  stuff (and check for the current cpu above in the specific case). */
    /** @todo Seems KeFlushQueuedDpcs doesn't wait for the DPCs to be completely
     *        executed. Seen pArgs being freed while some CPU was using it before
     *        cRefs was added. */
    g_pfnrtNtKeFlushQueuedDpcs();

    if (pcHits)
        *pcHits = pArgs->cHits;

    /* Dereference the argument structure. */
    int32_t cRefs = ASMAtomicDecS32(&pArgs->cRefs);
    Assert(cRefs >= 0);
    if (cRefs == 0)
        ExFreePool(pArgs);

    return VINF_SUCCESS;
}


RTDECL(int) RTMpOnAll(PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2)
{
    if (g_pfnrtKeIpiGenericCall)
        return rtMpCallUsingBroadcastIpi(pfnWorker, pvUser1, pvUser2, rtmpNtOnAllBroadcastIpiWrapper,
                                         NIL_RTCPUID, NIL_RTCPUID, NULL);
    return rtMpCallUsingDpcs(pfnWorker, pvUser1, pvUser2, RT_NT_CPUID_ALL, NIL_RTCPUID, NIL_RTCPUID, NULL);
}


RTDECL(int) RTMpOnOthers(PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2)
{
    if (g_pfnrtKeIpiGenericCall)
        return rtMpCallUsingBroadcastIpi(pfnWorker, pvUser1, pvUser2, rtmpNtOnOthersBroadcastIpiWrapper,
                                         NIL_RTCPUID, NIL_RTCPUID, NULL);
    return rtMpCallUsingDpcs(pfnWorker, pvUser1, pvUser2, RT_NT_CPUID_OTHERS, NIL_RTCPUID, NIL_RTCPUID, NULL);
}


RTDECL(int) RTMpOnPair(RTCPUID idCpu1, RTCPUID idCpu2, uint32_t fFlags, PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2)
{
    int rc;
    AssertReturn(idCpu1 != idCpu2, VERR_INVALID_PARAMETER);
    AssertReturn(!(fFlags & RTMPON_F_VALID_MASK), VERR_INVALID_FLAGS);
    if ((fFlags & RTMPON_F_CONCURRENT_EXEC) && !g_pfnrtKeIpiGenericCall)
        return VERR_NOT_SUPPORTED;

    /*
     * Check that both CPUs are online before doing the broadcast call.
     */
    if (   RTMpIsCpuOnline(idCpu1)
        && RTMpIsCpuOnline(idCpu2))
    {
        /*
         * The broadcast IPI isn't quite as bad as it could have been, because
         * it looks like windows doesn't synchronize CPUs on the way out, they
         * seems to get back to normal work while the pair is still busy.
         */
        uint32_t cHits = 0;
        if (g_pfnrtKeIpiGenericCall)
            rc = rtMpCallUsingBroadcastIpi(pfnWorker, pvUser1, pvUser2, rtmpNtOnPairBroadcastIpiWrapper, idCpu1, idCpu2, &cHits);
        else
            rc = rtMpCallUsingDpcs(pfnWorker, pvUser1, pvUser2, RT_NT_CPUID_PAIR, idCpu1, idCpu2, &cHits);
        if (RT_SUCCESS(rc))
        {
            Assert(cHits <= 2);
            if (cHits == 2)
                rc = VINF_SUCCESS;
            else if (cHits == 1)
                rc = VERR_NOT_ALL_CPUS_SHOWED;
            else if (cHits == 0)
                rc = VERR_CPU_OFFLINE;
            else
                rc = VERR_CPU_IPE_1;
        }
    }
    /*
     * A CPU must be present to be considered just offline.
     */
    else if (   RTMpIsCpuPresent(idCpu1)
             && RTMpIsCpuPresent(idCpu2))
        rc = VERR_CPU_OFFLINE;
    else
        rc = VERR_CPU_NOT_FOUND;
    return rc;
}


RTDECL(bool) RTMpOnPairIsConcurrentExecSupported(void)
{
    return g_pfnrtKeIpiGenericCall != NULL;
}


/**
 * Releases a reference to a RTMPNTONSPECIFICARGS heap allocation, freeing it
 * when the last reference is released.
 */
DECLINLINE(void) rtMpNtOnSpecificRelease(PRTMPNTONSPECIFICARGS pArgs)
{
    uint32_t cRefs = ASMAtomicDecU32(&pArgs->cRefs);
    AssertMsg(cRefs <= 1, ("cRefs=%#x\n", cRefs));
    if (cRefs == 0)
        ExFreePool(pArgs);
}


/**
 * Wrapper between the native nt per-cpu callbacks and PFNRTWORKER
 *
 * @param   Dpc                 DPC object
 * @param   DeferredContext     Context argument specified by KeInitializeDpc
 * @param   SystemArgument1     Argument specified by KeInsertQueueDpc
 * @param   SystemArgument2     Argument specified by KeInsertQueueDpc
 */
static VOID rtMpNtOnSpecificDpcWrapper(IN PKDPC Dpc, IN PVOID DeferredContext,
                                       IN PVOID SystemArgument1, IN PVOID SystemArgument2)
{
    PRTMPNTONSPECIFICARGS pArgs = (PRTMPNTONSPECIFICARGS)DeferredContext;
    ASMAtomicWriteBool(&pArgs->fExecuting, true);

    pArgs->CallbackArgs.pfnWorker(KeGetCurrentProcessorNumber(), pArgs->CallbackArgs.pvUser1, pArgs->CallbackArgs.pvUser2);

    ASMAtomicWriteBool(&pArgs->fDone, true);
    KeSetEvent(&pArgs->DoneEvt, 1 /*PriorityIncrement*/, FALSE /*Wait*/);

    rtMpNtOnSpecificRelease(pArgs);
}


RTDECL(int) RTMpOnSpecific(RTCPUID idCpu, PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2)
{
    /*
     * Don't try mess with an offline CPU.
     */
    if (!RTMpIsCpuOnline(idCpu))
        return !RTMpIsCpuPossible(idCpu)
              ? VERR_CPU_NOT_FOUND
              : VERR_CPU_OFFLINE;

    /*
     * Use the broadcast IPI routine if there are no more than two CPUs online,
     * or if the current IRQL is unsuitable for KeWaitForSingleObject.
     */
    int rc;
    uint32_t cHits = 0;
    if (   g_pfnrtKeIpiGenericCall
        && (   RTMpGetOnlineCount() <= 2
            || KeGetCurrentIrql()   > APC_LEVEL)
       )
    {
        rc = rtMpCallUsingBroadcastIpi(pfnWorker, pvUser1, pvUser2, rtmpNtOnSpecificBroadcastIpiWrapper,
                                       idCpu, NIL_RTCPUID, &cHits);
        if (RT_SUCCESS(rc))
        {
            if (cHits == 1)
                return VINF_SUCCESS;
            rc = cHits == 0 ? VERR_CPU_OFFLINE : VERR_CPU_IPE_1;
        }
        return rc;
    }

#if 0
    rc = rtMpCallUsingDpcs(pfnWorker, pvUser1, pvUser2, RT_NT_CPUID_SPECIFIC, idCpu, NIL_RTCPUID, &cHits);
    if (RT_SUCCESS(rc))
    {
        if (cHits == 1)
            return VINF_SUCCESS;
        rc = cHits == 0 ? VERR_CPU_OFFLINE : VERR_CPU_IPE_1;
    }
    return rc;

#else
    /*
     * Initialize the argument package and the objects within it.
     * The package is referenced counted to avoid unnecessary spinning to
     * synchronize cleanup and prevent stack corruption.
     */
    PRTMPNTONSPECIFICARGS pArgs = (PRTMPNTONSPECIFICARGS)ExAllocatePoolWithTag(NonPagedPool, sizeof(*pArgs), (ULONG)'RTMp');
    if (!pArgs)
        return VERR_NO_MEMORY;
    pArgs->cRefs                  = 2;
    pArgs->fExecuting             = false;
    pArgs->fDone                  = false;
    pArgs->CallbackArgs.pfnWorker = pfnWorker;
    pArgs->CallbackArgs.pvUser1   = pvUser1;
    pArgs->CallbackArgs.pvUser2   = pvUser2;
    pArgs->CallbackArgs.idCpu     = idCpu;
    pArgs->CallbackArgs.cHits     = 0;
    pArgs->CallbackArgs.cRefs     = 2;
    KeInitializeEvent(&pArgs->DoneEvt, SynchronizationEvent, FALSE /* not signalled */);
    KeInitializeDpc(&pArgs->Dpc, rtMpNtOnSpecificDpcWrapper, pArgs);
    KeSetImportanceDpc(&pArgs->Dpc, HighImportance);
    KeSetTargetProcessorDpc(&pArgs->Dpc, (int)idCpu);

    /*
     * Disable preemption while we check the current processor and inserts the DPC.
     */
    KIRQL bOldIrql;
    KeRaiseIrql(DISPATCH_LEVEL, &bOldIrql);
    ASMCompilerBarrier(); /* paranoia */

    if (RTMpCpuId() == idCpu)
    {
        /* Just execute the callback on the current CPU. */
        pfnWorker(idCpu, pvUser1, pvUser2);
        KeLowerIrql(bOldIrql);

        ExFreePool(pArgs);
        return VINF_SUCCESS;
    }

    /* Different CPU, so queue it if the CPU is still online. */
    if (RTMpIsCpuOnline(idCpu))
    {
        BOOLEAN fRc = KeInsertQueueDpc(&pArgs->Dpc, 0, 0);
        Assert(fRc);
        KeLowerIrql(bOldIrql);

        uint64_t const nsRealWaitTS = RTTimeNanoTS();

        /*
         * Wait actively for a while in case the CPU/thread responds quickly.
         */
        uint32_t cLoopsLeft = 0x20000;
        while (cLoopsLeft-- > 0)
        {
            if (pArgs->fDone)
            {
                rtMpNtOnSpecificRelease(pArgs);
                return VINF_SUCCESS;
            }
            ASMNopPause();
        }

        /*
         * It didn't respond, so wait on the event object, poking the CPU if it's slow.
         */
        LARGE_INTEGER Timeout;
        Timeout.QuadPart = -10000; /* 1ms */
        NTSTATUS rcNt = KeWaitForSingleObject(&pArgs->DoneEvt, Executive, KernelMode, FALSE /* Alertable */, &Timeout);
        if (rcNt == STATUS_SUCCESS)
        {
            rtMpNtOnSpecificRelease(pArgs);
            return VINF_SUCCESS;
        }

        /* If it hasn't respondend yet, maybe poke it and wait some more. */
        if (rcNt == STATUS_TIMEOUT)
        {
#ifndef IPRT_TARGET_NT4
            if (   !pArgs->fExecuting
                && (   g_pfnrtMpPokeCpuWorker == rtMpPokeCpuUsingHalReqestIpiW7Plus
                    || g_pfnrtMpPokeCpuWorker == rtMpPokeCpuUsingHalReqestIpiPreW7))
                RTMpPokeCpu(idCpu);
#endif

            Timeout.QuadPart = -1280000; /* 128ms */
            rcNt = KeWaitForSingleObject(&pArgs->DoneEvt, Executive, KernelMode, FALSE /* Alertable */, &Timeout);
            if (rcNt == STATUS_SUCCESS)
            {
                rtMpNtOnSpecificRelease(pArgs);
                return VINF_SUCCESS;
            }
        }

        /*
         * Something weird is happening, try bail out.
         */
        if (KeRemoveQueueDpc(&pArgs->Dpc))
        {
            ExFreePool(pArgs); /* DPC was still queued, so we can return without further ado. */
            LogRel(("RTMpOnSpecific(%#x): Not processed after %llu ns: rcNt=%#x\n", idCpu, RTTimeNanoTS() - nsRealWaitTS, rcNt));
        }
        else
        {
            /* DPC is running, wait a good while for it to complete. */
            LogRel(("RTMpOnSpecific(%#x): Still running after %llu ns: rcNt=%#x\n", idCpu, RTTimeNanoTS() - nsRealWaitTS, rcNt));

            Timeout.QuadPart = -30*1000*1000*10; /* 30 seconds */
            rcNt = KeWaitForSingleObject(&pArgs->DoneEvt, Executive, KernelMode, FALSE /* Alertable */, &Timeout);
            if (rcNt != STATUS_SUCCESS)
                LogRel(("RTMpOnSpecific(%#x): Giving up on running worker after %llu ns: rcNt=%#x\n", idCpu, RTTimeNanoTS() - nsRealWaitTS, rcNt));
        }
        rc = RTErrConvertFromNtStatus(rcNt);
    }
    else
    {
        /* CPU is offline.*/
        KeLowerIrql(bOldIrql);
        rc = !RTMpIsCpuPossible(idCpu) ? VERR_CPU_NOT_FOUND : VERR_CPU_OFFLINE;
    }

    rtMpNtOnSpecificRelease(pArgs);
    return rc;
#endif
}




static VOID rtMpNtPokeCpuDummy(IN PKDPC Dpc, IN PVOID DeferredContext, IN PVOID SystemArgument1, IN PVOID SystemArgument2)
{
    NOREF(Dpc);
    NOREF(DeferredContext);
    NOREF(SystemArgument1);
    NOREF(SystemArgument2);
}

#ifndef IPRT_TARGET_NT4

/** Callback used by rtMpPokeCpuUsingBroadcastIpi. */
static ULONG_PTR rtMpIpiGenericCall(ULONG_PTR Argument)
{
    NOREF(Argument);
    return 0;
}


/**
 * RTMpPokeCpu worker that uses broadcast IPIs for doing the work.
 *
 * @returns VINF_SUCCESS
 * @param   idCpu           The CPU identifier.
 */
int rtMpPokeCpuUsingBroadcastIpi(RTCPUID idCpu)
{
    g_pfnrtKeIpiGenericCall(rtMpIpiGenericCall, 0);
    return VINF_SUCCESS;
}


/**
 * RTMpPokeCpu worker that uses the Windows 7 and later version of
 * HalRequestIpip to get the job done.
 *
 * @returns VINF_SUCCESS
 * @param   idCpu           The CPU identifier.
 */
int rtMpPokeCpuUsingHalReqestIpiW7Plus(RTCPUID idCpu)
{
    /*
     * I think we'll let idCpu be an NT processor number and not a HAL processor
     * index.  KeAddProcessorAffinityEx is for HAL and uses HAL processor
     * indexes as input from what I can tell.
     */
    PROCESSOR_NUMBER ProcNumber = { /*Group=*/ idCpu / 64, /*Number=*/ idCpu % 64, /* Reserved=*/ 0};
    KAFFINITY_EX     Target;
    g_pfnrtKeInitializeAffinityEx(&Target);
    g_pfnrtKeAddProcessorAffinityEx(&Target, g_pfnrtKeGetProcessorIndexFromNumber(&ProcNumber));

    g_pfnrtHalRequestIpiW7Plus(0, &Target);
    return VINF_SUCCESS;
}


/**
 * RTMpPokeCpu worker that uses the Vista and earlier version of HalRequestIpip
 * to get the job done.
 *
 * @returns VINF_SUCCESS
 * @param   idCpu           The CPU identifier.
 */
int rtMpPokeCpuUsingHalReqestIpiPreW7(RTCPUID idCpu)
{
    __debugbreak(); /** @todo this code needs testing!!  */
    KAFFINITY Target = 1;
    Target <<= idCpu;
    g_pfnrtHalRequestIpiPreW7(Target);
    return VINF_SUCCESS;
}

#endif /* !IPRT_TARGET_NT4 */


int rtMpPokeCpuUsingDpc(RTCPUID idCpu)
{
    /*
     * APC fallback.
     */
    static KDPC s_aPokeDpcs[MAXIMUM_PROCESSORS] = {0};
    static bool s_fPokeDPCsInitialized = false;

    if (!s_fPokeDPCsInitialized)
    {
        for (unsigned i = 0; i < RT_ELEMENTS(s_aPokeDpcs); i++)
        {
            KeInitializeDpc(&s_aPokeDpcs[i], rtMpNtPokeCpuDummy, NULL);
            KeSetImportanceDpc(&s_aPokeDpcs[i], HighImportance);
            KeSetTargetProcessorDpc(&s_aPokeDpcs[i], (int)i);
        }
        s_fPokeDPCsInitialized = true;
    }

    /* Raise the IRQL to DISPATCH_LEVEL so we can't be rescheduled to another cpu.
     * KeInsertQueueDpc must also be executed at IRQL >= DISPATCH_LEVEL.
     */
    KIRQL oldIrql;
    KeRaiseIrql(DISPATCH_LEVEL, &oldIrql);

    KeSetImportanceDpc(&s_aPokeDpcs[idCpu], HighImportance);
    KeSetTargetProcessorDpc(&s_aPokeDpcs[idCpu], (int)idCpu);

    /* Assuming here that high importance DPCs will be delivered immediately; or at least an IPI will be sent immediately.
     * @note: not true on at least Vista & Windows 7
     */
    BOOLEAN bRet = KeInsertQueueDpc(&s_aPokeDpcs[idCpu], 0, 0);

    KeLowerIrql(oldIrql);
    return (bRet == TRUE) ? VINF_SUCCESS : VERR_ACCESS_DENIED /* already queued */;
}


RTDECL(int) RTMpPokeCpu(RTCPUID idCpu)
{
    if (!RTMpIsCpuOnline(idCpu))
        return !RTMpIsCpuPossible(idCpu)
              ? VERR_CPU_NOT_FOUND
              : VERR_CPU_OFFLINE;
    /* Calls rtMpSendIpiFallback, rtMpSendIpiWin7AndLater or rtMpSendIpiVista. */
    return g_pfnrtMpPokeCpuWorker(idCpu);
}


RTDECL(bool) RTMpOnAllIsConcurrentSafe(void)
{
    return false;
}

