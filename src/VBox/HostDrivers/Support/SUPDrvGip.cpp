/* $Id$ */
/** @file
 * VBoxDrv - The VirtualBox Support Driver - Common code for GIP.
 */

/*
 * Copyright (C) 2006-2015 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_SUP_DRV
#define SUPDRV_AGNOSTIC
#include "SUPDrvInternal.h"
#ifndef PAGE_SHIFT
# include <iprt/param.h>
#endif
#include <iprt/asm.h>
#include <iprt/asm-amd64-x86.h>
#include <iprt/asm-math.h>
#include <iprt/cpuset.h>
#include <iprt/handletable.h>
#include <iprt/mem.h>
#include <iprt/mp.h>
#include <iprt/power.h>
#include <iprt/process.h>
#include <iprt/semaphore.h>
#include <iprt/spinlock.h>
#include <iprt/thread.h>
#include <iprt/uuid.h>
#include <iprt/net.h>
#include <iprt/crc.h>
#include <iprt/string.h>
#include <iprt/timer.h>
#if defined(RT_OS_DARWIN) || defined(RT_OS_SOLARIS) || defined(RT_OS_FREEBSD)
# include <iprt/rand.h>
# include <iprt/path.h>
#endif
#include <iprt/uint128.h>
#include <iprt/x86.h>

#include <VBox/param.h>
#include <VBox/log.h>
#include <VBox/err.h>

#if defined(RT_OS_SOLARIS) || defined(RT_OS_DARWIN)
# include "dtrace/SUPDrv.h"
#else
/* ... */
#endif


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The frequency by which we recalculate the u32UpdateHz and
 * u32UpdateIntervalNS GIP members. The value must be a power of 2.
 *
 * Warning: Bumping this too high might overflow u32UpdateIntervalNS.
 */
#define GIP_UPDATEHZ_RECALC_FREQ            0x800

/** A reserved TSC value used for synchronization as well as measurement of
 *  TSC deltas. */
#define GIP_TSC_DELTA_RSVD                  UINT64_MAX
/** The number of TSC delta measurement loops in total (includes primer and
 *  read-time loops). */
#define GIP_TSC_DELTA_LOOPS                 96
/** The number of cache primer loops. */
#define GIP_TSC_DELTA_PRIMER_LOOPS          4
/** The number of loops until we keep computing the minumum read time. */
#define GIP_TSC_DELTA_READ_TIME_LOOPS       24
/** Stop measurement of TSC delta. */
#define GIP_TSC_DELTA_SYNC_STOP             0
/** Start measurement of TSC delta. */
#define GIP_TSC_DELTA_SYNC_START            1
/** Worker thread is ready for reading the TSC. */
#define GIP_TSC_DELTA_SYNC_WORKER_READY     2
/** Worker thread is done updating TSC delta info. */
#define GIP_TSC_DELTA_SYNC_WORKER_DONE      3
/** When IPRT is isn't concurrent safe: Master is ready and will wait for worker
 *  with a timeout. */
#define GIP_TSC_DELTA_SYNC_PRESTART_MASTER  4
/** When IPRT is isn't concurrent safe: Worker is ready after waiting for
 *  master with a timeout. */
#define GIP_TSC_DELTA_SYNC_PRESTART_WORKER  5
/** The TSC-refinement interval in seconds. */
#define GIP_TSC_REFINE_INTERVAL             5
/** The TSC-delta threshold for the SUPGIPUSETSCDELTA_PRACTICALLY_ZERO rating */
#define GIP_TSC_DELTA_THRESHOLD_PRACTICALLY_ZERO    32
/** The TSC-delta threshold for the SUPGIPUSETSCDELTA_ROUGHLY_ZERO rating */
#define GIP_TSC_DELTA_THRESHOLD_ROUGHLY_ZERO        448
/** The TSC delta value for the initial GIP master - 0 in regular builds.
 * To test the delta code this can be set to a non-zero value.  */
#if 1
# define GIP_TSC_DELTA_INITIAL_MASTER_VALUE INT64_C(170139095182512) /* 0x00009abd9854acb0 */
#else
# define GIP_TSC_DELTA_INITIAL_MASTER_VALUE INT64_C(0)
#endif

AssertCompile(GIP_TSC_DELTA_PRIMER_LOOPS < GIP_TSC_DELTA_READ_TIME_LOOPS);
AssertCompile(GIP_TSC_DELTA_PRIMER_LOOPS + GIP_TSC_DELTA_READ_TIME_LOOPS < GIP_TSC_DELTA_LOOPS);

/** @def VBOX_SVN_REV
 * The makefile should define this if it can. */
#ifndef VBOX_SVN_REV
# define VBOX_SVN_REV 0
#endif

#if 0 /* Don't start the GIP timers. Useful when debugging the IPRT timer code. */
# define DO_NOT_START_GIP
#endif


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(void)   supdrvGipSyncAndInvariantTimer(PRTTIMER pTimer, void *pvUser, uint64_t iTick);
static DECLCALLBACK(void)   supdrvGipAsyncTimer(PRTTIMER pTimer, void *pvUser, uint64_t iTick);
static void                 supdrvGipInitCpu(PSUPDRVDEVEXT pDevExt, PSUPGLOBALINFOPAGE pGip, PSUPGIPCPU pCpu, uint64_t u64NanoTS);
#ifdef SUPDRV_USE_TSC_DELTA_THREAD
static int                  supdrvTscDeltaThreadInit(PSUPDRVDEVEXT pDevExt);
static void                 supdrvTscDeltaTerm(PSUPDRVDEVEXT pDevExt);
static int                  supdrvTscDeltaThreadWaitForOnlineCpus(PSUPDRVDEVEXT pDevExt);
#endif


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
DECLEXPORT(PSUPGLOBALINFOPAGE) g_pSUPGlobalInfoPage = NULL;



/*
 *
 * Misc Common GIP Code
 * Misc Common GIP Code
 * Misc Common GIP Code
 *
 *
 */


/**
 * Finds the GIP CPU index corresponding to @a idCpu.
 *
 * @returns GIP CPU array index, UINT32_MAX if not found.
 * @param   pGip                The GIP.
 * @param   idCpu               The CPU ID.
 */
static uint32_t supdrvGipFindCpuIndexForCpuId(PSUPGLOBALINFOPAGE pGip, RTCPUID idCpu)
{
    uint32_t i;
    for (i = 0; i < pGip->cCpus; i++)
        if (pGip->aCPUs[i].idCpu == idCpu)
            return i;
    return UINT32_MAX;
}


/**
 * Applies the TSC delta to the supplied raw TSC value.
 *
 * @returns VBox status code. (Ignored by all users, just FYI.)
 * @param   pGip            Pointer to the GIP.
 * @param   puTsc           Pointer to a valid TSC value before the TSC delta has been applied.
 * @param   idApic          The APIC ID of the CPU @c puTsc corresponds to.
 * @param   fDeltaApplied   Where to store whether the TSC delta was succesfully
 *                          applied or not (optional, can be NULL).
 *
 * @remarks Maybe called with interrupts disabled in ring-0!
 *
 * @note    Don't you dare change the delta calculation.  If you really do, make
 *          sure you update all places where it's used (IPRT, SUPLibAll.cpp,
 *          SUPDrv.c, supdrvGipMpEvent, and more).
 */
DECLINLINE(int) supdrvTscDeltaApply(PSUPGLOBALINFOPAGE pGip, uint64_t *puTsc, uint16_t idApic, bool *pfDeltaApplied)
{
    int rc;

    /*
     * Validate input.
     */
    AssertPtr(puTsc);
    AssertPtr(pGip);
    Assert(pGip->enmUseTscDelta > SUPGIPUSETSCDELTA_ZERO_CLAIMED);

    /*
     * Carefully convert the idApic into a GIPCPU entry.
     */
    if (RT_LIKELY(idApic < RT_ELEMENTS(pGip->aiCpuFromApicId)))
    {
        uint16_t iCpu = pGip->aiCpuFromApicId[idApic];
        if (RT_LIKELY(iCpu < pGip->cCpus))
        {
            PSUPGIPCPU pGipCpu = &pGip->aCPUs[iCpu];

            /*
             * Apply the delta if valid.
             */
            if (RT_LIKELY(pGipCpu->i64TSCDelta != INT64_MAX))
            {
                *puTsc -= pGipCpu->i64TSCDelta;
                if (pfDeltaApplied)
                    *pfDeltaApplied = true;
                return VINF_SUCCESS;
            }

            rc = VINF_SUCCESS;
        }
        else
        {
            AssertMsgFailed(("iCpu=%u cCpus=%u\n", iCpu, pGip->cCpus));
            rc = VERR_INVALID_CPU_INDEX;
        }
    }
    else
    {
        AssertMsgFailed(("idApic=%u\n", idApic));
        rc = VERR_INVALID_CPU_ID;
    }
    if (pfDeltaApplied)
        *pfDeltaApplied = false;
    return rc;
}


/*
 *
 * GIP Mapping and Unmapping Related Code.
 * GIP Mapping and Unmapping Related Code.
 * GIP Mapping and Unmapping Related Code.
 *
 *
 */


/**
 * (Re-)initializes the per-cpu structure prior to starting or resuming the GIP
 * updating.
 *
 * @param   pGip             Pointer to the GIP.
 * @param   pGipCpu          The per CPU structure for this CPU.
 * @param   u64NanoTS        The current time.
 */
static void supdrvGipReInitCpu(PSUPGLOBALINFOPAGE pGip, PSUPGIPCPU pGipCpu, uint64_t u64NanoTS)
{
    /*
     * Here we don't really care about applying the TSC delta. The re-initialization of this
     * value is not relevant especially while (re)starting the GIP as the first few ones will
     * be ignored anyway, see supdrvGipDoUpdateCpu().
     */
    pGipCpu->u64TSC    = ASMReadTSC() - pGipCpu->u32UpdateIntervalTSC;
    pGipCpu->u64NanoTS = u64NanoTS;
}


/**
 * Set the current TSC and NanoTS value for the CPU.
 *
 * @param   idCpu            The CPU ID. Unused - we have to use the APIC ID.
 * @param   pvUser1          Pointer to the ring-0 GIP mapping.
 * @param   pvUser2          Pointer to the variable holding the current time.
 */
static DECLCALLBACK(void) supdrvGipReInitCpuCallback(RTCPUID idCpu, void *pvUser1, void *pvUser2)
{
    PSUPGLOBALINFOPAGE  pGip = (PSUPGLOBALINFOPAGE)pvUser1;
    unsigned            iCpu = pGip->aiCpuFromApicId[ASMGetApicId()];

    if (RT_LIKELY(iCpu < pGip->cCpus && pGip->aCPUs[iCpu].idCpu == idCpu))
        supdrvGipReInitCpu(pGip, &pGip->aCPUs[iCpu], *(uint64_t *)pvUser2);

    NOREF(pvUser2);
    NOREF(idCpu);
}


/**
 * State structure for supdrvGipDetectGetGipCpuCallback.
 */
typedef struct SUPDRVGIPDETECTGETCPU
{
    /** Bitmap of APIC IDs that has been seen (initialized to zero).
     *  Used to detect duplicate APIC IDs (paranoia). */
    uint8_t volatile    bmApicId[256 / 8];
    /** Mask of supported GIP CPU getter methods (SUPGIPGETCPU_XXX) (all bits set
     *  initially). The callback clears the methods not detected. */
    uint32_t volatile   fSupported;
    /** The first callback detecting any kind of range issues (initialized to
     * NIL_RTCPUID). */
    RTCPUID volatile    idCpuProblem;
} SUPDRVGIPDETECTGETCPU;
/** Pointer to state structure for supdrvGipDetectGetGipCpuCallback. */
typedef SUPDRVGIPDETECTGETCPU *PSUPDRVGIPDETECTGETCPU;


/**
 * Checks for alternative ways of getting the CPU ID.
 *
 * This also checks the APIC ID, CPU ID and CPU set index values against the
 * GIP tables.
 *
 * @param   idCpu            The CPU ID. Unused - we have to use the APIC ID.
 * @param   pvUser1          Pointer to the state structure.
 * @param   pvUser2          Pointer to the GIP.
 */
static DECLCALLBACK(void) supdrvGipDetectGetGipCpuCallback(RTCPUID idCpu, void *pvUser1, void *pvUser2)
{
    PSUPDRVGIPDETECTGETCPU  pState = (PSUPDRVGIPDETECTGETCPU)pvUser1;
    PSUPGLOBALINFOPAGE      pGip   = (PSUPGLOBALINFOPAGE)pvUser2;
    uint32_t                fSupported = 0;
    uint16_t                idApic;
    int                     iCpuSet;

    AssertMsg(idCpu == RTMpCpuId(), ("idCpu=%#x RTMpCpuId()=%#x\n", idCpu, RTMpCpuId())); /* paranoia^3 */

    /*
     * Check that the CPU ID and CPU set index are interchangable.
     */
    iCpuSet = RTMpCpuIdToSetIndex(idCpu);
    if ((RTCPUID)iCpuSet == idCpu)
    {
        AssertCompile(RT_IS_POWER_OF_TWO(RTCPUSET_MAX_CPUS));
        if (   iCpuSet >= 0
            && iCpuSet < RTCPUSET_MAX_CPUS
            && RT_IS_POWER_OF_TWO(RTCPUSET_MAX_CPUS))
        {
            /*
             * Check whether the IDTR.LIMIT contains a CPU number.
             */
#ifdef RT_ARCH_X86
            uint16_t const  cbIdt = sizeof(X86DESC64SYSTEM) * 256;
#else
            uint16_t const  cbIdt = sizeof(X86DESCGATE)     * 256;
#endif
            RTIDTR          Idtr;
            ASMGetIDTR(&Idtr);
            if (Idtr.cbIdt >= cbIdt)
            {
                uint32_t uTmp = Idtr.cbIdt - cbIdt;
                uTmp &= RTCPUSET_MAX_CPUS - 1;
                if (uTmp == idCpu)
                {
                    RTIDTR Idtr2;
                    ASMGetIDTR(&Idtr2);
                    if (Idtr2.cbIdt == Idtr.cbIdt)
                        fSupported |= SUPGIPGETCPU_IDTR_LIMIT_MASK_MAX_SET_CPUS;
                }
            }

            /*
             * Check whether RDTSCP is an option.
             */
            if (ASMHasCpuId())
            {
                if (   ASMIsValidExtRange(ASMCpuId_EAX(UINT32_C(0x80000000)))
                    && (ASMCpuId_EDX(UINT32_C(0x80000001)) & X86_CPUID_EXT_FEATURE_EDX_RDTSCP) )
                {
                    uint32_t uAux;
                    ASMReadTscWithAux(&uAux);
                    if ((uAux & (RTCPUSET_MAX_CPUS - 1)) == idCpu)
                    {
                        ASMNopPause();
                        ASMReadTscWithAux(&uAux);
                        if ((uAux & (RTCPUSET_MAX_CPUS - 1)) == idCpu)
                            fSupported |= SUPGIPGETCPU_RDTSCP_MASK_MAX_SET_CPUS;
                    }
                }
            }
        }
    }

    /*
     * Check that the APIC ID is unique.
     */
    idApic = ASMGetApicId();
    if (RT_LIKELY(   idApic < RT_ELEMENTS(pGip->aiCpuFromApicId)
                  && !ASMAtomicBitTestAndSet(pState->bmApicId, idApic)))
        fSupported |= SUPGIPGETCPU_APIC_ID;
    else
    {
        AssertCompile(sizeof(pState->bmApicId) * 8 == RT_ELEMENTS(pGip->aiCpuFromApicId));
        ASMAtomicCmpXchgU32(&pState->idCpuProblem, idCpu, NIL_RTCPUID);
        LogRel(("supdrvGipDetectGetGipCpuCallback: idCpu=%#x iCpuSet=%d idApic=%#x - duplicate APIC ID.\n",
                idCpu, iCpuSet, idApic));
    }

    /*
     * Check that the iCpuSet is within the expected range.
     */
    if (RT_UNLIKELY(   iCpuSet < 0
                    || (unsigned)iCpuSet >= RTCPUSET_MAX_CPUS
                    || (unsigned)iCpuSet >= RT_ELEMENTS(pGip->aiCpuFromCpuSetIdx)))
    {
        ASMAtomicCmpXchgU32(&pState->idCpuProblem, idCpu, NIL_RTCPUID);
        LogRel(("supdrvGipDetectGetGipCpuCallback: idCpu=%#x iCpuSet=%d idApic=%#x - CPU set index is out of range.\n",
                idCpu, iCpuSet, idApic));
    }
    else
    {
        RTCPUID idCpu2 = RTMpCpuIdFromSetIndex(iCpuSet);
        if (RT_UNLIKELY(idCpu2 != idCpu))
        {
            ASMAtomicCmpXchgU32(&pState->idCpuProblem, idCpu, NIL_RTCPUID);
            LogRel(("supdrvGipDetectGetGipCpuCallback: idCpu=%#x iCpuSet=%d idApic=%#x - CPU id/index roundtrip problem: %#x\n",
                    idCpu, iCpuSet, idApic, idCpu2));
        }
    }

    /*
     * Update the supported feature mask before we return.
     */
    ASMAtomicAndU32(&pState->fSupported, fSupported);

    NOREF(pvUser2);
}


/**
 * Increase the timer freqency on hosts where this is possible (NT).
 *
 * The idea is that more interrupts is better for us... Also, it's better than
 * we increase the timer frequence, because we might end up getting inaccurate
 * callbacks if someone else does it.
 *
 * @param   pDevExt   Sets u32SystemTimerGranularityGrant if increased.
 */
static void supdrvGipRequestHigherTimerFrequencyFromSystem(PSUPDRVDEVEXT pDevExt)
{
    if (pDevExt->u32SystemTimerGranularityGrant == 0)
    {
        uint32_t u32SystemResolution;
        if (   RT_SUCCESS_NP(RTTimerRequestSystemGranularity(  976563 /* 1024 HZ */, &u32SystemResolution))
            || RT_SUCCESS_NP(RTTimerRequestSystemGranularity( 1000000 /* 1000 HZ */, &u32SystemResolution))
            || RT_SUCCESS_NP(RTTimerRequestSystemGranularity( 1953125 /*  512 HZ */, &u32SystemResolution))
            || RT_SUCCESS_NP(RTTimerRequestSystemGranularity( 2000000 /*  500 HZ */, &u32SystemResolution))
           )
        {
            Assert(RTTimerGetSystemGranularity() <= u32SystemResolution);
            pDevExt->u32SystemTimerGranularityGrant = u32SystemResolution;
        }
    }
}


/**
 * Undoes supdrvGipRequestHigherTimerFrequencyFromSystem.
 *
 * @param   pDevExt     Clears u32SystemTimerGranularityGrant.
 */
static void supdrvGipReleaseHigherTimerFrequencyFromSystem(PSUPDRVDEVEXT pDevExt)
{
    if (pDevExt->u32SystemTimerGranularityGrant)
    {
        int rc2 = RTTimerReleaseSystemGranularity(pDevExt->u32SystemTimerGranularityGrant);
        AssertRC(rc2);
        pDevExt->u32SystemTimerGranularityGrant = 0;
    }
}


/**
 * Maps the GIP into userspace and/or get the physical address of the GIP.
 *
 * @returns IPRT status code.
 * @param   pSession        Session to which the GIP mapping should belong.
 * @param   ppGipR3         Where to store the address of the ring-3 mapping. (optional)
 * @param   pHCPhysGip      Where to store the physical address. (optional)
 *
 * @remark  There is no reference counting on the mapping, so one call to this function
 *          count globally as one reference. One call to SUPR0GipUnmap() is will unmap GIP
 *          and remove the session as a GIP user.
 */
SUPR0DECL(int) SUPR0GipMap(PSUPDRVSESSION pSession, PRTR3PTR ppGipR3, PRTHCPHYS pHCPhysGip)
{
    int             rc;
    PSUPDRVDEVEXT   pDevExt = pSession->pDevExt;
    RTR3PTR         pGipR3  = NIL_RTR3PTR;
    RTHCPHYS        HCPhys  = NIL_RTHCPHYS;
    LogFlow(("SUPR0GipMap: pSession=%p ppGipR3=%p pHCPhysGip=%p\n", pSession, ppGipR3, pHCPhysGip));

    /*
     * Validate
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    AssertPtrNullReturn(ppGipR3, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pHCPhysGip, VERR_INVALID_POINTER);

#ifdef SUPDRV_USE_MUTEX_FOR_GIP
    RTSemMutexRequest(pDevExt->mtxGip, RT_INDEFINITE_WAIT);
#else
    RTSemFastMutexRequest(pDevExt->mtxGip);
#endif
    if (pDevExt->pGip)
    {
        /*
         * Map it?
         */
        rc = VINF_SUCCESS;
        if (ppGipR3)
        {
            if (pSession->GipMapObjR3 == NIL_RTR0MEMOBJ)
                rc = RTR0MemObjMapUser(&pSession->GipMapObjR3, pDevExt->GipMemObj, (RTR3PTR)-1, 0,
                                       RTMEM_PROT_READ, RTR0ProcHandleSelf());
            if (RT_SUCCESS(rc))
                pGipR3 = RTR0MemObjAddressR3(pSession->GipMapObjR3);
        }

        /*
         * Get physical address.
         */
        if (pHCPhysGip && RT_SUCCESS(rc))
            HCPhys = pDevExt->HCPhysGip;

        /*
         * Reference globally.
         */
        if (!pSession->fGipReferenced && RT_SUCCESS(rc))
        {
            pSession->fGipReferenced = 1;
            pDevExt->cGipUsers++;
            if (pDevExt->cGipUsers == 1)
            {
                PSUPGLOBALINFOPAGE pGipR0 = pDevExt->pGip;
                uint64_t u64NanoTS;

                /*
                 * GIP starts/resumes updating again.  On windows we bump the
                 * host timer frequency to make sure we don't get stuck in guest
                 * mode and to get better timer (and possibly clock) accuracy.
                 */
                LogFlow(("SUPR0GipMap: Resumes GIP updating\n"));

                supdrvGipRequestHigherTimerFrequencyFromSystem(pDevExt);

                /*
                 * document me
                 */
                if (pGipR0->aCPUs[0].u32TransactionId != 2 /* not the first time */)
                {
                    unsigned i;
                    for (i = 0; i < pGipR0->cCpus; i++)
                        ASMAtomicUoWriteU32(&pGipR0->aCPUs[i].u32TransactionId,
                                            (pGipR0->aCPUs[i].u32TransactionId + GIP_UPDATEHZ_RECALC_FREQ * 2)
                                            & ~(GIP_UPDATEHZ_RECALC_FREQ * 2 - 1));
                    ASMAtomicWriteU64(&pGipR0->u64NanoTSLastUpdateHz, 0);
                }

                /*
                 * document me
                 */
                u64NanoTS = RTTimeSystemNanoTS() - pGipR0->u32UpdateIntervalNS;
                if (   pGipR0->u32Mode == SUPGIPMODE_INVARIANT_TSC
                    || pGipR0->u32Mode == SUPGIPMODE_SYNC_TSC
                    || RTMpGetOnlineCount() == 1)
                    supdrvGipReInitCpu(pGipR0, &pGipR0->aCPUs[0], u64NanoTS);
                else
                    RTMpOnAll(supdrvGipReInitCpuCallback, pGipR0, &u64NanoTS);

                /*
                 * Detect alternative ways to figure the CPU ID in ring-3 and
                 * raw-mode context.  Check the sanity of the APIC IDs, CPU IDs,
                 * and CPU set indexes while we're at it.
                 */
                if (RT_SUCCESS(rc))
                {
                    SUPDRVGIPDETECTGETCPU DetectState;
                    RT_BZERO((void *)&DetectState.bmApicId, sizeof(DetectState.bmApicId));
                    DetectState.fSupported   = UINT32_MAX;
                    DetectState.idCpuProblem = NIL_RTCPUID;
                    rc = RTMpOnAll(supdrvGipDetectGetGipCpuCallback, &DetectState, pGipR0);
                    if (DetectState.idCpuProblem == NIL_RTCPUID)
                    {
                        if (   DetectState.fSupported != UINT32_MAX
                            && DetectState.fSupported != 0)
                        {
                            if (pGipR0->fGetGipCpu != DetectState.fSupported)
                            {
                                pGipR0->fGetGipCpu = DetectState.fSupported;
                                LogRel(("SUPR0GipMap: fGetGipCpu=%#x\n", DetectState.fSupported));
                            }
                        }
                        else
                        {
                            LogRel(("SUPR0GipMap: No supported ways of getting the APIC ID or CPU number in ring-3! (%#x)\n",
                                    DetectState.fSupported));
                            rc = VERR_UNSUPPORTED_CPU;
                        }
                    }
                    else
                    {
                        LogRel(("SUPR0GipMap: APIC ID, CPU ID or CPU set index problem detected on CPU #%u (%#x)!\n",
                                DetectState.idCpuProblem, DetectState.idCpuProblem));
                        rc = VERR_INVALID_CPU_ID;
                    }
                }

                /*
                 * Start the GIP timer if all is well..
                 */
                if (RT_SUCCESS(rc))
                {
#ifndef DO_NOT_START_GIP
                    rc = RTTimerStart(pDevExt->pGipTimer, 0 /* fire ASAP */); AssertRC(rc);
#endif
                    rc = VINF_SUCCESS;
                }

                /*
                 * Bail out on error.
                 */
                if (RT_FAILURE(rc))
                {
                    LogRel(("SUPR0GipMap: failed rc=%Rrc\n", rc));
                    pDevExt->cGipUsers = 0;
                    pSession->fGipReferenced = 0;
                    if (pSession->GipMapObjR3 != NIL_RTR0MEMOBJ)
                    {
                        int rc2 = RTR0MemObjFree(pSession->GipMapObjR3, false); AssertRC(rc2);
                        if (RT_SUCCESS(rc2))
                            pSession->GipMapObjR3 = NIL_RTR0MEMOBJ;
                    }
                    HCPhys = NIL_RTHCPHYS;
                    pGipR3 = NIL_RTR3PTR;
                }
            }
        }
    }
    else
    {
        rc = VERR_GENERAL_FAILURE;
        Log(("SUPR0GipMap: GIP is not available!\n"));
    }
#ifdef SUPDRV_USE_MUTEX_FOR_GIP
    RTSemMutexRelease(pDevExt->mtxGip);
#else
    RTSemFastMutexRelease(pDevExt->mtxGip);
#endif

    /*
     * Write returns.
     */
    if (pHCPhysGip)
        *pHCPhysGip = HCPhys;
    if (ppGipR3)
        *ppGipR3 = pGipR3;

#ifdef DEBUG_DARWIN_GIP
    OSDBGPRINT(("SUPR0GipMap: returns %d *pHCPhysGip=%lx pGipR3=%p\n", rc, (unsigned long)HCPhys, (void *)pGipR3));
#else
    LogFlow((   "SUPR0GipMap: returns %d *pHCPhysGip=%lx pGipR3=%p\n", rc, (unsigned long)HCPhys, (void *)pGipR3));
#endif
    return rc;
}


/**
 * Unmaps any user mapping of the GIP and terminates all GIP access
 * from this session.
 *
 * @returns IPRT status code.
 * @param   pSession        Session to which the GIP mapping should belong.
 */
SUPR0DECL(int) SUPR0GipUnmap(PSUPDRVSESSION pSession)
{
    int                     rc = VINF_SUCCESS;
    PSUPDRVDEVEXT           pDevExt = pSession->pDevExt;
#ifdef DEBUG_DARWIN_GIP
    OSDBGPRINT(("SUPR0GipUnmap: pSession=%p pGip=%p GipMapObjR3=%p\n",
                pSession,
                pSession->GipMapObjR3 != NIL_RTR0MEMOBJ ? RTR0MemObjAddress(pSession->GipMapObjR3) : NULL,
                pSession->GipMapObjR3));
#else
    LogFlow(("SUPR0GipUnmap: pSession=%p\n", pSession));
#endif
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);

#ifdef SUPDRV_USE_MUTEX_FOR_GIP
    RTSemMutexRequest(pDevExt->mtxGip, RT_INDEFINITE_WAIT);
#else
    RTSemFastMutexRequest(pDevExt->mtxGip);
#endif

    /*
     * Unmap anything?
     */
    if (pSession->GipMapObjR3 != NIL_RTR0MEMOBJ)
    {
        rc = RTR0MemObjFree(pSession->GipMapObjR3, false);
        AssertRC(rc);
        if (RT_SUCCESS(rc))
            pSession->GipMapObjR3 = NIL_RTR0MEMOBJ;
    }

    /*
     * Dereference global GIP.
     */
    if (pSession->fGipReferenced && !rc)
    {
        pSession->fGipReferenced = 0;
        if (    pDevExt->cGipUsers > 0
            &&  !--pDevExt->cGipUsers)
        {
            LogFlow(("SUPR0GipUnmap: Suspends GIP updating\n"));
#ifndef DO_NOT_START_GIP
            rc = RTTimerStop(pDevExt->pGipTimer); AssertRC(rc); rc = VINF_SUCCESS;
#endif
            supdrvGipReleaseHigherTimerFrequencyFromSystem(pDevExt);
        }
    }

#ifdef SUPDRV_USE_MUTEX_FOR_GIP
    RTSemMutexRelease(pDevExt->mtxGip);
#else
    RTSemFastMutexRelease(pDevExt->mtxGip);
#endif

    return rc;
}


/**
 * Gets the GIP pointer.
 *
 * @returns Pointer to the GIP or NULL.
 */
SUPDECL(PSUPGLOBALINFOPAGE) SUPGetGIP(void)
{
    return g_pSUPGlobalInfoPage;
}





/*
 *
 *
 * GIP Initialization, Termination and CPU Offline / Online Related Code.
 * GIP Initialization, Termination and CPU Offline / Online Related Code.
 * GIP Initialization, Termination and CPU Offline / Online Related Code.
 *
 *
 */


/**
 * Timer callback function for TSC frequency refinement in invariant GIP mode.
 *
 * @param   pTimer      The timer.
 * @param   pvUser      Opaque pointer to the device instance data.
 * @param   iTick       The timer tick.
 */
static DECLCALLBACK(void) supdrvInitAsyncRefineTscTimer(PRTTIMER pTimer, void *pvUser, uint64_t iTick)
{
    PSUPDRVDEVEXT      pDevExt = (PSUPDRVDEVEXT)pvUser;
    PSUPGLOBALINFOPAGE pGip = pDevExt->pGip;
    bool               fDeltaApplied = false;
    uint8_t            idApic;
    uint64_t           u64DeltaNanoTS;
    uint64_t           u64DeltaTsc;
    uint64_t           u64NanoTS;
    uint64_t           u64Tsc;
    RTCCUINTREG        uFlags;

    /* Paranoia. */
    Assert(pGip);
    Assert(pGip->u32Mode == SUPGIPMODE_INVARIANT_TSC);

#if !defined(RT_OS_OS2) /* PORTME: Disable if timers are called from clock interrupt handler or with interrupts disabled. */
    u64NanoTS = RTTimeSystemNanoTS();
    while (RTTimeSystemNanoTS() == u64NanoTS)
        ASMNopPause();
#endif
    uFlags    = ASMIntDisableFlags();
    idApic    = ASMGetApicId();
    u64Tsc    = ASMReadTSC();
    u64NanoTS = RTTimeSystemNanoTS();
    ASMSetFlags(uFlags);
    if (pGip->enmUseTscDelta > SUPGIPUSETSCDELTA_PRACTICALLY_ZERO)
        supdrvTscDeltaApply(pGip, &u64Tsc, idApic, &fDeltaApplied);
    u64DeltaNanoTS = u64NanoTS - pDevExt->u64NanoTSAnchor;
    u64DeltaTsc = u64Tsc - pDevExt->u64TscAnchor;

    if (RT_UNLIKELY(   pGip->enmUseTscDelta > SUPGIPUSETSCDELTA_PRACTICALLY_ZERO
                    && !fDeltaApplied))
    {
        Log(("vboxdrv: failed to refine TSC frequency as TSC-deltas unavailable after %d seconds!\n",
                    GIP_TSC_REFINE_INTERVAL));
        return;
    }

    /* Calculate the TSC frequency. */
    if (   u64DeltaTsc < UINT64_MAX / RT_NS_1SEC
        && u64DeltaNanoTS < UINT32_MAX)
        pGip->u64CpuHz = ASMMultU64ByU32DivByU32(u64DeltaTsc, RT_NS_1SEC, (uint32_t)u64DeltaNanoTS);
    else
    {
        RTUINT128U CpuHz, Tmp, Divisor;
        CpuHz.s.Lo = CpuHz.s.Hi = 0;
        RTUInt128MulU64ByU64(&Tmp, u64DeltaTsc, RT_NS_1SEC_64);
        RTUInt128Div(&CpuHz, &Tmp, RTUInt128AssignU64(&Divisor, u64DeltaNanoTS));
        pGip->u64CpuHz = CpuHz.s.Lo;
    }

    /* Update rest of GIP. */
    Assert(pGip->u32Mode != SUPGIPMODE_ASYNC_TSC); /* See SUPGetCpuHzFromGIP().*/
    pGip->aCPUs[0].u64CpuHz = pGip->u64CpuHz;
}


/**
 * Starts the TSC-frequency refinement phase asynchronously.
 *
 * @param   pDevExt        Pointer to the device instance data.
 */
static void supdrvGipInitAsyncRefineTscFreq(PSUPDRVDEVEXT pDevExt)
{
    uint64_t            u64NanoTS;
    RTCCUINTREG         uFlags;
    uint8_t             idApic;
    int                 rc;
    PSUPGLOBALINFOPAGE  pGip;

    /* Validate. */
    Assert(pDevExt);
    Assert(pDevExt->pGip);
    pGip = pDevExt->pGip;

#ifdef SUPDRV_USE_TSC_DELTA_THREAD
    /*
     * If the TSC-delta thread is created, wait until it's done calculating
     * the TSC-deltas on the relevant online CPUs before we start the TSC refinement.
     */
    if (   pGip->enmUseTscDelta > SUPGIPUSETSCDELTA_ZERO_CLAIMED
        && ASMAtomicReadS32(&pDevExt->rcTscDelta) == VERR_NOT_AVAILABLE)
    {
        rc = supdrvTscDeltaThreadWaitForOnlineCpus(pDevExt);
        if (rc == VERR_TIMEOUT)
        {
            SUPR0Printf("vboxdrv: Skipping refinement of TSC frequency as TSC-delta measurement timed out!\n");
            return;
        }
    }
#endif

    /*
     * Record the TSC and NanoTS as the starting anchor point for refinement of the
     * TSC. We deliberately avoid using SUPReadTSC() here as we want to keep the
     * reading of the TSC and the NanoTS as close as possible.
     */
    u64NanoTS = RTTimeSystemNanoTS();
    while (RTTimeSystemNanoTS() == u64NanoTS)
        ASMNopPause();
    uFlags                   = ASMIntDisableFlags();
    idApic                   = ASMGetApicId();
    pDevExt->u64TscAnchor    = ASMReadTSC();
    pDevExt->u64NanoTSAnchor = RTTimeSystemNanoTS();
    ASMSetFlags(uFlags);
    if (pGip->enmUseTscDelta > SUPGIPUSETSCDELTA_PRACTICALLY_ZERO)
        supdrvTscDeltaApply(pGip, &pDevExt->u64TscAnchor, idApic, NULL /* pfDeltaApplied */);

    rc = RTTimerCreateEx(&pDevExt->pTscRefineTimer, 0 /* one-shot */, RTTIMER_FLAGS_CPU_ANY,
                         supdrvInitAsyncRefineTscTimer, pDevExt);
    if (RT_SUCCESS(rc))
    {
        /*
         * Refine the TSC frequency measurement over a long interval. Ideally, we want to keep the
         * interval as small as possible while gaining the most consistent and accurate frequency
         * (compared to what the host OS might have measured).
         *
         * In theory, we gain more accuracy with longer intervals, but we want VMs to startup with the
         * same TSC frequency whenever possible so we need to keep the interval short.
         */
        rc = RTTimerStart(pDevExt->pTscRefineTimer, GIP_TSC_REFINE_INTERVAL * RT_NS_1SEC_64);
        AssertRC(rc);
    }
    else
        OSDBGPRINT(("RTTimerCreateEx failed to create one-shot timer. rc=%Rrc\n", rc));
}


/**
 * Measures the TSC frequency of the system.
 *
 * Uses a busy-wait method for the async. case as it is intended to help push
 * the CPU frequency up, while for the invariant cases using a sleeping method.
 *
 * The TSC frequency can vary on systems which are not reported as invariant.
 * On such systems the object of this function is to find out what the nominal,
 * maximum TSC frequency under 'normal' CPU operation.
 *
 * @returns VBox status code.
 * @param   pDevExt        Pointer to the device instance.
 *
 * @remarks Must be called only -after- measuring the TSC deltas.
 */
static int supdrvGipInitMeasureTscFreq(PSUPDRVDEVEXT pDevExt)
{
    int cTriesLeft = 4;
    PSUPGLOBALINFOPAGE pGip = pDevExt->pGip;

    /* Assert order. */
    AssertReturn(pGip, VERR_INVALID_PARAMETER);
    AssertReturn(pGip->u32Magic == SUPGLOBALINFOPAGE_MAGIC, VERR_WRONG_ORDER);

    while (cTriesLeft-- > 0)
    {
        RTCCUINTREG uFlags;
        uint64_t    u64NanoTsBefore;
        uint64_t    u64NanoTsAfter;
        uint64_t    u64TscBefore;
        uint64_t    u64TscAfter;
        uint8_t     idApicBefore;
        uint8_t     idApicAfter;

        /*
         * Synchronize with the host OS clock tick before reading the TSC.
         * Especially important on older Windows version where the granularity is terrible.
         */
        u64NanoTsBefore = RTTimeSystemNanoTS();
        while (RTTimeSystemNanoTS() == u64NanoTsBefore)
            ASMNopPause();

        uFlags          = ASMIntDisableFlags();
        idApicBefore    = ASMGetApicId();
        u64TscBefore    = ASMReadTSC();
        u64NanoTsBefore = RTTimeSystemNanoTS();
        ASMSetFlags(uFlags);

        if (pGip->u32Mode == SUPGIPMODE_INVARIANT_TSC)
        {
            /*
             * Sleep-wait since the TSC frequency is constant, it eases host load.
             * Shorter interval produces more variance in the frequency (esp. Windows).
             */
            RTThreadSleep(200);
            u64NanoTsAfter = RTTimeSystemNanoTS();
            while (RTTimeSystemNanoTS() == u64NanoTsAfter)
                ASMNopPause();
            u64NanoTsAfter = RTTimeSystemNanoTS();
        }
        else
        {
            /* Busy-wait keeping the frequency up and measure. */
            for (;;)
            {
                u64NanoTsAfter = RTTimeSystemNanoTS();
                if (u64NanoTsAfter < RT_NS_100MS + u64NanoTsBefore)
                    ASMNopPause();
                else
                    break;
            }
        }

        uFlags      = ASMIntDisableFlags();
        idApicAfter = ASMGetApicId();
        u64TscAfter = ASMReadTSC();
        ASMSetFlags(uFlags);

        if (pGip->enmUseTscDelta > SUPGIPUSETSCDELTA_PRACTICALLY_ZERO)
        {
            int rc;
            bool fAppliedBefore;
            bool fAppliedAfter;
            rc = supdrvTscDeltaApply(pGip, &u64TscBefore, idApicBefore, &fAppliedBefore);   AssertRCReturn(rc, rc);
            rc = supdrvTscDeltaApply(pGip, &u64TscAfter,  idApicAfter,  &fAppliedAfter);    AssertRCReturn(rc, rc);

            if (   !fAppliedBefore
                || !fAppliedAfter)
            {
#ifdef SUPDRV_USE_TSC_DELTA_THREAD
                /*
                 * The TSC-delta measurements are kicked-off asynchronously as each host CPU is initialized.
                 * Therefore, if we failed to have a delta for the CPU(s) we were scheduled on (idApicBefore
                 * and idApicAfter) then wait until we have TSC-delta measurements for all online CPUs and
                 * proceed. This should be triggered just once if we're rather unlucky.
                 */
                rc = supdrvTscDeltaThreadWaitForOnlineCpus(pDevExt);
                if (rc == VERR_TIMEOUT)
                {
                    SUPR0Printf("vboxdrv: supdrvGipInitMeasureTscFreq: timedout waiting for TSC-delta measurements.\n");
                    return VERR_SUPDRV_TSC_FREQ_MEASUREMENT_FAILED;
                }
#else
                SUPR0Printf("vboxdrv: supdrvGipInitMeasureTscFreq: idApicBefore=%u idApicAfter=%u cTriesLeft=%u\n",
                            idApicBefore, idApicAfter, cTriesLeft);
#endif
                continue;
            }
        }

        /*
         * Update GIP.
         */
        pGip->u64CpuHz = ((u64TscAfter - u64TscBefore) * RT_NS_1SEC_64) / (u64NanoTsAfter - u64NanoTsBefore);
        if (pGip->u32Mode != SUPGIPMODE_ASYNC_TSC)
            pGip->aCPUs[0].u64CpuHz = pGip->u64CpuHz;
        return VINF_SUCCESS;
    }

    return VERR_SUPDRV_TSC_FREQ_MEASUREMENT_FAILED;
}


/**
 * Finds our (@a idCpu) entry, or allocates a new one if not found.
 *
 * @returns Index of the CPU in the cache set.
 * @param   pGip                The GIP.
 * @param   idCpu               The CPU ID.
 */
static uint32_t supdrvGipFindOrAllocCpuIndexForCpuId(PSUPGLOBALINFOPAGE pGip, RTCPUID idCpu)
{
    uint32_t i, cTries;

    /*
     * ASSUMES that CPU IDs are constant.
     */
    for (i = 0; i < pGip->cCpus; i++)
        if (pGip->aCPUs[i].idCpu == idCpu)
            return i;

    cTries = 0;
    do
    {
        for (i = 0; i < pGip->cCpus; i++)
        {
            bool fRc;
            ASMAtomicCmpXchgSize(&pGip->aCPUs[i].idCpu, idCpu, NIL_RTCPUID, fRc);
            if (fRc)
                return i;
        }
    } while (cTries++ < 32);
    AssertReleaseFailed();
    return i - 1;
}


/**
 * The calling CPU should be accounted as online, update GIP accordingly.
 *
 * This is used by supdrvGipCreate() as well as supdrvGipMpEvent().
 *
 * @param   pDevExt             The device extension.
 * @param   idCpu               The CPU ID.
 */
static void supdrvGipMpEventOnlineOrInitOnCpu(PSUPDRVDEVEXT pDevExt, RTCPUID idCpu)
{
    int         iCpuSet = 0;
    uint16_t    idApic = UINT16_MAX;
    uint32_t    i = 0;
    uint64_t    u64NanoTS = 0;
    PSUPGLOBALINFOPAGE pGip = pDevExt->pGip;

    AssertPtrReturnVoid(pGip);
    AssertRelease(idCpu == RTMpCpuId());
    Assert(pGip->cPossibleCpus == RTMpGetCount());

    /*
     * Do this behind a spinlock with interrupts disabled as this can fire
     * on all CPUs simultaneously, see @bugref{6110}.
     */
    RTSpinlockAcquire(pDevExt->hGipSpinlock);

    /*
     * Update the globals.
     */
    ASMAtomicWriteU16(&pGip->cPresentCpus,  RTMpGetPresentCount());
    ASMAtomicWriteU16(&pGip->cOnlineCpus,   RTMpGetOnlineCount());
    iCpuSet = RTMpCpuIdToSetIndex(idCpu);
    if (iCpuSet >= 0)
    {
        Assert(RTCpuSetIsMemberByIndex(&pGip->PossibleCpuSet, iCpuSet));
        RTCpuSetAddByIndex(&pGip->OnlineCpuSet, iCpuSet);
        RTCpuSetAddByIndex(&pGip->PresentCpuSet, iCpuSet);
    }

    /*
     * Update the entry.
     */
    u64NanoTS = RTTimeSystemNanoTS() - pGip->u32UpdateIntervalNS;
    i = supdrvGipFindOrAllocCpuIndexForCpuId(pGip, idCpu);
    supdrvGipInitCpu(pDevExt, pGip, &pGip->aCPUs[i], u64NanoTS);
    idApic = ASMGetApicId();
    ASMAtomicWriteU16(&pGip->aCPUs[i].idApic,  idApic);
    ASMAtomicWriteS16(&pGip->aCPUs[i].iCpuSet, (int16_t)iCpuSet);
    ASMAtomicWriteSize(&pGip->aCPUs[i].idCpu,  idCpu);

    /*
     * Update the APIC ID and CPU set index mappings.
     */
    ASMAtomicWriteU16(&pGip->aiCpuFromApicId[idApic],     i);
    ASMAtomicWriteU16(&pGip->aiCpuFromCpuSetIdx[iCpuSet], i);

    /* Update the Mp online/offline counter. */
    ASMAtomicIncU32(&pDevExt->cMpOnOffEvents);

    /* Add this CPU to the set of CPUs for which we need to calculate their TSC-deltas. */
    if (pGip->enmUseTscDelta > SUPGIPUSETSCDELTA_ZERO_CLAIMED)
    {
        RTCpuSetAddByIndex(&pDevExt->TscDeltaCpuSet, iCpuSet);
#ifdef SUPDRV_USE_TSC_DELTA_THREAD
        RTSpinlockAcquire(pDevExt->hTscDeltaSpinlock);
        if (   pDevExt->enmTscDeltaThreadState == kTscDeltaThreadState_Listening
            || pDevExt->enmTscDeltaThreadState == kTscDeltaThreadState_Measuring)
        {
            pDevExt->enmTscDeltaThreadState = kTscDeltaThreadState_WaitAndMeasure;
        }
        RTSpinlockRelease(pDevExt->hTscDeltaSpinlock);
#endif
    }

    /* commit it */
    ASMAtomicWriteSize(&pGip->aCPUs[i].enmState, SUPGIPCPUSTATE_ONLINE);

    RTSpinlockRelease(pDevExt->hGipSpinlock);
}


/**
 * The CPU should be accounted as offline, update the GIP accordingly.
 *
 * This is used by supdrvGipMpEvent.
 *
 * @param   pDevExt             The device extension.
 * @param   idCpu               The CPU ID.
 */
static void supdrvGipMpEventOffline(PSUPDRVDEVEXT pDevExt, RTCPUID idCpu)
{
    PSUPGLOBALINFOPAGE  pGip = pDevExt->pGip;
    int                 iCpuSet;
    unsigned            i;

    AssertPtrReturnVoid(pGip);
    RTSpinlockAcquire(pDevExt->hGipSpinlock);

    iCpuSet = RTMpCpuIdToSetIndex(idCpu);
    AssertReturnVoid(iCpuSet >= 0);

    i = pGip->aiCpuFromCpuSetIdx[iCpuSet];
    AssertReturnVoid(i < pGip->cCpus);
    AssertReturnVoid(pGip->aCPUs[i].idCpu == idCpu);

    Assert(RTCpuSetIsMemberByIndex(&pGip->PossibleCpuSet, iCpuSet));
    RTCpuSetDelByIndex(&pGip->OnlineCpuSet, iCpuSet);

    /* Update the Mp online/offline counter. */
    ASMAtomicIncU32(&pDevExt->cMpOnOffEvents);

    /* If we are the initiator going offline while measuring the TSC delta, unspin other waiting CPUs! */
    if (ASMAtomicReadU32(&pDevExt->idTscDeltaInitiator) == idCpu)
    {
        ASMAtomicWriteU32(&pDevExt->pTscDeltaSync->u, GIP_TSC_DELTA_SYNC_START);
        ASMAtomicWriteU64(&pGip->aCPUs[i].u64TSCSample, ~GIP_TSC_DELTA_RSVD);
    }

    if (pGip->enmUseTscDelta > SUPGIPUSETSCDELTA_ZERO_CLAIMED)
    {
        /* Reset the TSC delta, we will recalculate it lazily. */
        ASMAtomicWriteS64(&pGip->aCPUs[i].i64TSCDelta, INT64_MAX);
        /* Remove this CPU from the set of CPUs that we have obtained the TSC deltas. */
        RTCpuSetDelByIndex(&pDevExt->TscDeltaObtainedCpuSet, iCpuSet);
    }

    /* commit it */
    ASMAtomicWriteSize(&pGip->aCPUs[i].enmState, SUPGIPCPUSTATE_OFFLINE);

    RTSpinlockRelease(pDevExt->hGipSpinlock);
}


/**
 * Multiprocessor event notification callback.
 *
 * This is used to make sure that the GIP master gets passed on to
 * another CPU.  It also updates the associated CPU data.
 *
 * @param   enmEvent    The event.
 * @param   idCpu       The cpu it applies to.
 * @param   pvUser      Pointer to the device extension.
 *
 * @remarks This function -must- fire on the newly online'd CPU for the
 *          RTMPEVENT_ONLINE case and can fire on any CPU for the
 *          RTMPEVENT_OFFLINE case.
 */
static DECLCALLBACK(void) supdrvGipMpEvent(RTMPEVENT enmEvent, RTCPUID idCpu, void *pvUser)
{
    PSUPDRVDEVEXT       pDevExt = (PSUPDRVDEVEXT)pvUser;
    PSUPGLOBALINFOPAGE  pGip    = pDevExt->pGip;

    AssertRelease(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    /*
     * Update the GIP CPU data.
     */
    if (pGip)
    {
        switch (enmEvent)
        {
            case RTMPEVENT_ONLINE:
                AssertRelease(idCpu == RTMpCpuId());
                supdrvGipMpEventOnlineOrInitOnCpu(pDevExt, idCpu);
                break;
            case RTMPEVENT_OFFLINE:
                supdrvGipMpEventOffline(pDevExt, idCpu);
                break;
        }
    }

    /*
     * Make sure there is a master GIP.
     */
    if (enmEvent == RTMPEVENT_OFFLINE)
    {
        RTCPUID idGipMaster = ASMAtomicReadU32(&pDevExt->idGipMaster);
        if (idGipMaster == idCpu)
        {
            /*
             * The GIP master is going offline, find a new one.
             */
            bool        fIgnored;
            unsigned    i;
            RTCPUID     idNewGipMaster = NIL_RTCPUID;
            RTCPUSET    OnlineCpus;
            RTMpGetOnlineSet(&OnlineCpus);

            for (i = 0; i < RTCPUSET_MAX_CPUS; i++)
                if (RTCpuSetIsMemberByIndex(&OnlineCpus, i))
                {
                    RTCPUID idCurCpu = RTMpCpuIdFromSetIndex(i);
                    if (idCurCpu != idGipMaster)
                    {
                        idNewGipMaster = idCurCpu;
                        break;
                    }
                }

            Log(("supdrvGipMpEvent: Gip master %#lx -> %#lx\n", (long)idGipMaster, (long)idNewGipMaster));
            ASMAtomicCmpXchgSize(&pDevExt->idGipMaster, idNewGipMaster, idGipMaster, fIgnored);
            NOREF(fIgnored);
        }
    }
}


/**
 * On CPU initialization callback for RTMpOnAll.
 *
 * @param   idCpu               The CPU ID.
 * @param   pvUser1             The device extension.
 * @param   pvUser2             The GIP.
 */
static DECLCALLBACK(void) supdrvGipInitOnCpu(RTCPUID idCpu, void *pvUser1, void *pvUser2)
{
    /* This is good enough, even though it will update some of the globals a
       bit to much. */
    supdrvGipMpEventOnlineOrInitOnCpu((PSUPDRVDEVEXT)pvUser1, idCpu);
}


/**
 * Callback used by supdrvDetermineAsyncTSC to read the TSC on a CPU.
 *
 * @param   idCpu       Ignored.
 * @param   pvUser1     Where to put the TSC.
 * @param   pvUser2     Ignored.
 */
static DECLCALLBACK(void) supdrvGipInitDetermineAsyncTscWorker(RTCPUID idCpu, void *pvUser1, void *pvUser2)
{
    ASMAtomicWriteU64((uint64_t volatile *)pvUser1, ASMReadTSC());
}


/**
 * Determine if Async GIP mode is required because of TSC drift.
 *
 * When using the default/normal timer code it is essential that the time stamp counter
 * (TSC) runs never backwards, that is, a read operation to the counter should return
 * a bigger value than any previous read operation. This is guaranteed by the latest
 * AMD CPUs and by newer Intel CPUs which never enter the C2 state (P4). In any other
 * case we have to choose the asynchronous timer mode.
 *
 * @param   poffMin     Pointer to the determined difference between different
 *                      cores (optional, can be NULL).
 * @return  false if the time stamp counters appear to be synchronized, true otherwise.
 */
static bool supdrvGipInitDetermineAsyncTsc(uint64_t *poffMin)
{
    /*
     * Just iterate all the cpus 8 times and make sure that the TSC is
     * ever increasing. We don't bother taking TSC rollover into account.
     */
    int         iEndCpu = RTMpGetArraySize();
    int         iCpu;
    int         cLoops = 8;
    bool        fAsync = false;
    int         rc = VINF_SUCCESS;
    uint64_t    offMax = 0;
    uint64_t    offMin = ~(uint64_t)0;
    uint64_t    PrevTsc = ASMReadTSC();

    while (cLoops-- > 0)
    {
        for (iCpu = 0; iCpu < iEndCpu; iCpu++)
        {
            uint64_t CurTsc;
            rc = RTMpOnSpecific(RTMpCpuIdFromSetIndex(iCpu), supdrvGipInitDetermineAsyncTscWorker, &CurTsc, NULL);
            if (RT_SUCCESS(rc))
            {
                if (CurTsc <= PrevTsc)
                {
                    fAsync = true;
                    offMin = offMax = PrevTsc - CurTsc;
                    Log(("supdrvGipInitDetermineAsyncTsc: iCpu=%d cLoops=%d CurTsc=%llx PrevTsc=%llx\n",
                         iCpu, cLoops, CurTsc, PrevTsc));
                    break;
                }

                /* Gather statistics (except the first time). */
                if (iCpu != 0 || cLoops != 7)
                {
                    uint64_t off = CurTsc - PrevTsc;
                    if (off < offMin)
                        offMin = off;
                    if (off > offMax)
                        offMax = off;
                    Log2(("%d/%d: off=%llx\n", cLoops, iCpu, off));
                }

                /* Next */
                PrevTsc = CurTsc;
            }
            else if (rc == VERR_NOT_SUPPORTED)
                break;
            else
                AssertMsg(rc == VERR_CPU_NOT_FOUND || rc == VERR_CPU_OFFLINE, ("%d\n", rc));
        }

        /* broke out of the loop. */
        if (iCpu < iEndCpu)
            break;
    }

    if (poffMin)
        *poffMin = offMin; /* Almost RTMpOnSpecific profiling. */
    Log(("supdrvGipInitDetermineAsyncTsc: returns %d; iEndCpu=%d rc=%d offMin=%llx offMax=%llx\n",
         fAsync, iEndCpu, rc, offMin, offMax));
#if !defined(RT_OS_SOLARIS) && !defined(RT_OS_OS2) && !defined(RT_OS_WINDOWS)
    OSDBGPRINT(("vboxdrv: fAsync=%d offMin=%#lx offMax=%#lx\n", fAsync, (long)offMin, (long)offMax));
#endif
    return fAsync;
}


/**
 * supdrvGipInit() worker that determines the GIP TSC mode.
 *
 * @returns The most suitable TSC mode.
 * @param   pDevExt     Pointer to the device instance data.
 */
static SUPGIPMODE supdrvGipInitDetermineTscMode(PSUPDRVDEVEXT pDevExt)
{
    uint64_t u64DiffCoresIgnored;
    uint32_t uEAX, uEBX, uECX, uEDX;

    /*
     * Establish whether the CPU advertises TSC as invariant, we need that in
     * a couple of places below.
     */
    bool fInvariantTsc = false;
    if (ASMHasCpuId())
    {
        uEAX = ASMCpuId_EAX(0x80000000);
        if (ASMIsValidExtRange(uEAX) && uEAX >= 0x80000007)
        {
            uEDX = ASMCpuId_EDX(0x80000007);
            if (uEDX & X86_CPUID_AMD_ADVPOWER_EDX_TSCINVAR)
                fInvariantTsc = true;
        }
    }

    /*
     * On single CPU systems, we don't need to consider ASYNC mode.
     */
    if (RTMpGetCount() <= 1)
        return fInvariantTsc ? SUPGIPMODE_INVARIANT_TSC : SUPGIPMODE_SYNC_TSC;

    /*
     * Allow the user and/or OS specific bits to force async mode.
     */
    if (supdrvOSGetForcedAsyncTscMode(pDevExt))
        return SUPGIPMODE_ASYNC_TSC;


#if 0 /** @todo enable this when i64TscDelta is applied in all places where it's needed */
    /*
     * Use invariant mode if the CPU says TSC is invariant.
     */
    if (fInvariantTsc)
        return SUPGIPMODE_INVARIANT_TSC;
#endif

    /*
     * TSC is not invariant and we're on SMP, this presents two problems:
     *
     *      (1) There might be a skew between the CPU, so that cpu0
     *          returns a TSC that is slightly different from cpu1.
     *          This screw may be due to (2), bad TSC initialization
     *          or slightly different TSC rates.
     *
     *      (2) Power management (and other things) may cause the TSC
     *          to run at a non-constant speed, and cause the speed
     *          to be different on the cpus. This will result in (1).
     *
     * If any of the above is detected, we will have to use ASYNC mode.
     */

    /* (1). Try check for current differences between the cpus. */
    if (supdrvGipInitDetermineAsyncTsc(&u64DiffCoresIgnored))
        return SUPGIPMODE_ASYNC_TSC;

#if 1 /** @todo remove once i64TscDelta is applied everywhere. Enable #if 0 above. */
    if (fInvariantTsc)
        return SUPGIPMODE_INVARIANT_TSC;
#endif

    /* (2) If it's an AMD CPU with power management, we won't trust its TSC. */
    ASMCpuId(0, &uEAX, &uEBX, &uECX, &uEDX);
    if (   ASMIsValidStdRange(uEAX)
        && ASMIsAmdCpuEx(uEBX, uECX, uEDX))
    {
        /* Check for APM support. */
        uEAX = ASMCpuId_EAX(0x80000000);
        if (ASMIsValidExtRange(uEAX) && uEAX >= 0x80000007)
        {
            uEDX = ASMCpuId_EDX(0x80000007);
            if (uEDX & 0x3e)  /* STC|TM|THERMTRIP|VID|FID. Ignore TS. */
                return SUPGIPMODE_ASYNC_TSC;
        }
    }

    return SUPGIPMODE_SYNC_TSC;
}


/**
 * Initializes per-CPU GIP information.
 *
 * @param   pDevExt     Pointer to the device instance data.
 * @param   pGip        Pointer to the GIP.
 * @param   pCpu        Pointer to which GIP CPU to initalize.
 * @param   u64NanoTS   The current nanosecond timestamp.
 */
static void supdrvGipInitCpu(PSUPDRVDEVEXT pDevExt, PSUPGLOBALINFOPAGE pGip, PSUPGIPCPU pCpu, uint64_t u64NanoTS)
{
    /* !!! Warning !!! The GIP may not be linked to the device instance data at this point!
       which is why we have 2 separate parameters. Don't dereference pDevExt->pGip here. */
    pCpu->u32TransactionId   = 2;
    pCpu->u64NanoTS          = u64NanoTS;
    pCpu->u64TSC             = ASMReadTSC();
    pCpu->u64TSCSample       = GIP_TSC_DELTA_RSVD;
    pCpu->i64TSCDelta        = pGip->enmUseTscDelta > SUPGIPUSETSCDELTA_ZERO_CLAIMED ? INT64_MAX : 0;

    ASMAtomicWriteSize(&pCpu->enmState, SUPGIPCPUSTATE_INVALID);
    ASMAtomicWriteSize(&pCpu->idCpu,    NIL_RTCPUID);
    ASMAtomicWriteS16(&pCpu->iCpuSet,   -1);
    ASMAtomicWriteU16(&pCpu->idApic,    UINT16_MAX);

    /*
     * We don't know the following values until we've executed updates.
     * So, we'll just pretend it's a 4 GHz CPU and adjust the history it on
     * the 2nd timer callout.
     */
    pCpu->u64CpuHz          = _4G + 1; /* tstGIP-2 depends on this. */
    pCpu->u32UpdateIntervalTSC
        = pCpu->au32TSCHistory[0]
        = pCpu->au32TSCHistory[1]
        = pCpu->au32TSCHistory[2]
        = pCpu->au32TSCHistory[3]
        = pCpu->au32TSCHistory[4]
        = pCpu->au32TSCHistory[5]
        = pCpu->au32TSCHistory[6]
        = pCpu->au32TSCHistory[7]
        = (uint32_t)(_4G / pGip->u32UpdateHz);
}


/**
 * Initializes the GIP data.
 *
 * @param   pDevExt             Pointer to the device instance data.
 * @param   pGip                Pointer to the read-write kernel mapping of the GIP.
 * @param   HCPhys              The physical address of the GIP.
 * @param   u64NanoTS           The current nanosecond timestamp.
 * @param   uUpdateHz           The update frequency.
 * @param   uUpdateIntervalNS   The update interval in nanoseconds.
 * @param   cCpus               The CPU count.
 */
static void supdrvGipInit(PSUPDRVDEVEXT pDevExt, PSUPGLOBALINFOPAGE pGip, RTHCPHYS HCPhys,
                          uint64_t u64NanoTS, unsigned uUpdateHz, unsigned uUpdateIntervalNS, unsigned cCpus)
{
    size_t const    cbGip = RT_ALIGN_Z(RT_OFFSETOF(SUPGLOBALINFOPAGE, aCPUs[cCpus]), PAGE_SIZE);
    unsigned        i;
#ifdef DEBUG_DARWIN_GIP
    OSDBGPRINT(("supdrvGipInit: pGip=%p HCPhys=%lx u64NanoTS=%llu uUpdateHz=%d cCpus=%u\n", pGip, (long)HCPhys, u64NanoTS, uUpdateHz, cCpus));
#else
    LogFlow(("supdrvGipInit: pGip=%p HCPhys=%lx u64NanoTS=%llu uUpdateHz=%d cCpus=%u\n", pGip, (long)HCPhys, u64NanoTS, uUpdateHz, cCpus));
#endif

    /*
     * Initialize the structure.
     */
    memset(pGip, 0, cbGip);

    pGip->u32Magic                = SUPGLOBALINFOPAGE_MAGIC;
    pGip->u32Version              = SUPGLOBALINFOPAGE_VERSION;
    pGip->u32Mode                 = supdrvGipInitDetermineTscMode(pDevExt);
    if (   pGip->u32Mode == SUPGIPMODE_INVARIANT_TSC
        /*|| pGip->u32Mode == SUPGIPMODE_SYNC_TSC */)
        pGip->enmUseTscDelta      = supdrvOSAreTscDeltasInSync() /* Allow OS override (windows). */
                                  ? SUPGIPUSETSCDELTA_ZERO_CLAIMED : SUPGIPUSETSCDELTA_PRACTICALLY_ZERO /* downgrade later */;
    else
        pGip->enmUseTscDelta      = SUPGIPUSETSCDELTA_NOT_APPLICABLE;
    pGip->cCpus                   = (uint16_t)cCpus;
    pGip->cPages                  = (uint16_t)(cbGip / PAGE_SIZE);
    pGip->u32UpdateHz             = uUpdateHz;
    pGip->u32UpdateIntervalNS     = uUpdateIntervalNS;
    pGip->fGetGipCpu              = SUPGIPGETCPU_APIC_ID;
    RTCpuSetEmpty(&pGip->OnlineCpuSet);
    RTCpuSetEmpty(&pGip->PresentCpuSet);
    RTMpGetSet(&pGip->PossibleCpuSet);
    pGip->cOnlineCpus             = RTMpGetOnlineCount();
    pGip->cPresentCpus            = RTMpGetPresentCount();
    pGip->cPossibleCpus           = RTMpGetCount();
    pGip->idCpuMax                = RTMpGetMaxCpuId();
    for (i = 0; i < RT_ELEMENTS(pGip->aiCpuFromApicId); i++)
        pGip->aiCpuFromApicId[i]    = UINT16_MAX;
    for (i = 0; i < RT_ELEMENTS(pGip->aiCpuFromCpuSetIdx); i++)
        pGip->aiCpuFromCpuSetIdx[i] = UINT16_MAX;
    for (i = 0; i < cCpus; i++)
        supdrvGipInitCpu(pDevExt, pGip, &pGip->aCPUs[i], u64NanoTS);

    /*
     * Link it to the device extension.
     */
    pDevExt->pGip      = pGip;
    pDevExt->HCPhysGip = HCPhys;
    pDevExt->cGipUsers = 0;
}


/**
 * Creates the GIP.
 *
 * @returns VBox status code.
 * @param   pDevExt     Instance data. GIP stuff may be updated.
 */
int VBOXCALL supdrvGipCreate(PSUPDRVDEVEXT pDevExt)
{
    PSUPGLOBALINFOPAGE  pGip;
    RTHCPHYS            HCPhysGip;
    uint32_t            u32SystemResolution;
    uint32_t            u32Interval;
    uint32_t            u32MinInterval;
    uint32_t            uMod;
    unsigned            cCpus;
    int                 rc;

    LogFlow(("supdrvGipCreate:\n"));

    /* Assert order. */
    Assert(pDevExt->u32SystemTimerGranularityGrant == 0);
    Assert(pDevExt->GipMemObj == NIL_RTR0MEMOBJ);
    Assert(!pDevExt->pGipTimer);

    /*
     * Check the CPU count.
     */
    cCpus = RTMpGetArraySize();
    if (   cCpus > RTCPUSET_MAX_CPUS
        || cCpus > 256 /* ApicId is used for the mappings */)
    {
        SUPR0Printf("VBoxDrv: Too many CPUs (%u) for the GIP (max %u)\n", cCpus, RT_MIN(RTCPUSET_MAX_CPUS, 256));
        return VERR_TOO_MANY_CPUS;
    }

    /*
     * Allocate a contiguous set of pages with a default kernel mapping.
     */
    rc = RTR0MemObjAllocCont(&pDevExt->GipMemObj, RT_UOFFSETOF(SUPGLOBALINFOPAGE, aCPUs[cCpus]), false /*fExecutable*/);
    if (RT_FAILURE(rc))
    {
        OSDBGPRINT(("supdrvGipCreate: failed to allocate the GIP page. rc=%d\n", rc));
        return rc;
    }
    pGip = (PSUPGLOBALINFOPAGE)RTR0MemObjAddress(pDevExt->GipMemObj); AssertPtr(pGip);
    HCPhysGip = RTR0MemObjGetPagePhysAddr(pDevExt->GipMemObj, 0); Assert(HCPhysGip != NIL_RTHCPHYS);

    /*
     * Allocate the TSC-delta sync struct on a separate cache line.
     */
    pDevExt->pvTscDeltaSync = RTMemAllocZ(sizeof(SUPTSCDELTASYNC) + 63);
    pDevExt->pTscDeltaSync  = RT_ALIGN_PT(pDevExt->pvTscDeltaSync, 64, PSUPTSCDELTASYNC);
    Assert(RT_ALIGN_PT(pDevExt->pTscDeltaSync, 64, PSUPTSCDELTASYNC) == pDevExt->pTscDeltaSync);

    /*
     * Find a reasonable update interval and initialize the structure.
     */
    supdrvGipRequestHigherTimerFrequencyFromSystem(pDevExt);
    /** @todo figure out why using a 100Ms interval upsets timekeeping in VMs.
     *        See @bugref{6710}. */
    u32MinInterval      = RT_NS_10MS;
    u32SystemResolution = RTTimerGetSystemGranularity();
    u32Interval         = u32MinInterval;
    uMod                = u32MinInterval % u32SystemResolution;
    if (uMod)
        u32Interval += u32SystemResolution - uMod;

    supdrvGipInit(pDevExt, pGip, HCPhysGip, RTTimeSystemNanoTS(), RT_NS_1SEC / u32Interval /*=Hz*/, u32Interval, cCpus);

    if (RT_UNLIKELY(   pGip->enmUseTscDelta == SUPGIPUSETSCDELTA_ZERO_CLAIMED
                    && pGip->u32Mode == SUPGIPMODE_ASYNC_TSC
                    && !supdrvOSGetForcedAsyncTscMode(pDevExt)))
    {
        /* Basically, invariant Windows boxes, should never be detected as async (i.e. TSC-deltas should be 0). */
        OSDBGPRINT(("supdrvGipCreate: The TSC-deltas should be normalized by the host OS, but verifying shows it's not!\n"));
        return VERR_INTERNAL_ERROR_2;
    }

    RTCpuSetEmpty(&pDevExt->TscDeltaCpuSet);
    RTCpuSetEmpty(&pDevExt->TscDeltaObtainedCpuSet);
#ifdef SUPDRV_USE_TSC_DELTA_THREAD
    if (pGip->enmUseTscDelta > SUPGIPUSETSCDELTA_ZERO_CLAIMED)
    {
        /* Initialize TSC-delta measurement thread before executing any Mp event callbacks. */
        rc = supdrvTscDeltaThreadInit(pDevExt);
    }
#endif
    if (RT_SUCCESS(rc))
    {
        rc = RTMpNotificationRegister(supdrvGipMpEvent, pDevExt);
        if (RT_SUCCESS(rc))
        {
            rc = RTMpOnAll(supdrvGipInitOnCpu, pDevExt, pGip);
            if (RT_SUCCESS(rc))
            {
#ifndef SUPDRV_USE_TSC_DELTA_THREAD
                uint16_t iCpu;
                if (pGip->enmUseTscDelta > SUPGIPUSETSCDELTA_ZERO_CLAIMED)
                {
                    /*
                     * Measure the TSC deltas now that we have MP notifications.
                     */
                    int cTries = 5;
                    do
                    {
                        rc = supdrvMeasureInitialTscDeltas(pDevExt);
                        if (   rc != VERR_TRY_AGAIN
                            && rc != VERR_CPU_OFFLINE)
                            break;
                    } while (--cTries > 0);
                    for (iCpu = 0; iCpu < pGip->cCpus; iCpu++)
                        Log(("supdrvTscDeltaInit: cpu[%u] delta %lld\n", iCpu, pGip->aCPUs[iCpu].i64TSCDelta));
                }
                else
                {
                    for (iCpu = 0; iCpu < pGip->cCpus; iCpu++)
                        AssertMsg(!pGip->aCPUs[iCpu].i64TSCDelta, ("iCpu=%u %lld mode=%d\n", iCpu, pGip->aCPUs[iCpu].i64TSCDelta, pGip->u32Mode));
                }
#endif
                if (RT_SUCCESS(rc))
                {
                    rc = supdrvGipInitMeasureTscFreq(pDevExt);
                    if (RT_SUCCESS(rc))
                    {
                        /*
                         * Create the timer.
                         * If CPU_ALL isn't supported we'll have to fall back to synchronous mode.
                         */
                        if (pGip->u32Mode == SUPGIPMODE_ASYNC_TSC)
                        {
                            rc = RTTimerCreateEx(&pDevExt->pGipTimer, u32Interval, RTTIMER_FLAGS_CPU_ALL,
                                                 supdrvGipAsyncTimer, pDevExt);
                            if (rc == VERR_NOT_SUPPORTED)
                            {
                                OSDBGPRINT(("supdrvGipCreate: omni timer not supported, falling back to synchronous mode\n"));
                                pGip->u32Mode = SUPGIPMODE_SYNC_TSC;
                            }
                        }
                        if (pGip->u32Mode != SUPGIPMODE_ASYNC_TSC)
                            rc = RTTimerCreateEx(&pDevExt->pGipTimer, u32Interval, 0 /* fFlags */,
                                                 supdrvGipSyncAndInvariantTimer, pDevExt);
                        if (RT_SUCCESS(rc))
                        {
                            /*
                             * We're good.
                             */
                            Log(("supdrvGipCreate: %u ns interval.\n", u32Interval));
                            supdrvGipReleaseHigherTimerFrequencyFromSystem(pDevExt);

                            g_pSUPGlobalInfoPage = pGip;
                            if (pGip->u32Mode == SUPGIPMODE_INVARIANT_TSC)
                                supdrvGipInitAsyncRefineTscFreq(pDevExt);
                            return VINF_SUCCESS;
                        }

                        OSDBGPRINT(("supdrvGipCreate: failed create GIP timer at %u ns interval. rc=%Rrc\n", u32Interval, rc));
                        Assert(!pDevExt->pGipTimer);
                    }
                    else
                        OSDBGPRINT(("supdrvGipCreate: supdrvGipInitMeasureTscFreq failed. rc=%Rrc\n", rc));
                }
                else
                    OSDBGPRINT(("supdrvGipCreate: supdrvMeasureInitialTscDeltas failed. rc=%Rrc\n", rc));
            }
            else
                OSDBGPRINT(("supdrvGipCreate: RTMpOnAll failed. rc=%Rrc\n", rc));
        }
        else
            OSDBGPRINT(("supdrvGipCreate: failed to register MP event notfication. rc=%Rrc\n", rc));
    }
    else
        OSDBGPRINT(("supdrvGipCreate: supdrvTscDeltaInit failed. rc=%Rrc\n", rc));

    supdrvGipDestroy(pDevExt); /* Releases timer frequency increase too. */
    return rc;
}


/**
 * Invalidates the GIP data upon termination.
 *
 * @param   pGip        Pointer to the read-write kernel mapping of the GIP.
 */
static void supdrvGipTerm(PSUPGLOBALINFOPAGE pGip)
{
    unsigned i;
    pGip->u32Magic = 0;
    for (i = 0; i < pGip->cCpus; i++)
    {
        pGip->aCPUs[i].u64NanoTS = 0;
        pGip->aCPUs[i].u64TSC = 0;
        pGip->aCPUs[i].iTSCHistoryHead = 0;
        pGip->aCPUs[i].u64TSCSample = 0;
        pGip->aCPUs[i].i64TSCDelta = INT64_MAX;
    }
}


/**
 * Terminates the GIP.
 *
 * @param   pDevExt     Instance data. GIP stuff may be updated.
 */
void VBOXCALL supdrvGipDestroy(PSUPDRVDEVEXT pDevExt)
{
    int rc;
#ifdef DEBUG_DARWIN_GIP
    OSDBGPRINT(("supdrvGipDestroy: pDevExt=%p pGip=%p pGipTimer=%p GipMemObj=%p\n", pDevExt,
                pDevExt->GipMemObj != NIL_RTR0MEMOBJ ? RTR0MemObjAddress(pDevExt->GipMemObj) : NULL,
                pDevExt->pGipTimer, pDevExt->GipMemObj));
#endif

    /*
     * Stop receiving MP notifications before tearing anything else down.
     */
    RTMpNotificationDeregister(supdrvGipMpEvent, pDevExt);

#ifdef SUPDRV_USE_TSC_DELTA_THREAD
    /*
     * Terminate the TSC-delta measurement thread and resources.
     */
    supdrvTscDeltaTerm(pDevExt);
#endif

    /*
     * Destroy the TSC-refinement one-shot timer.
     */
    if (pDevExt->pTscRefineTimer)
    {
        RTTimerDestroy(pDevExt->pTscRefineTimer);
        pDevExt->pTscRefineTimer = NULL;
    }

    if (pDevExt->pvTscDeltaSync)
    {
        RTMemFree(pDevExt->pvTscDeltaSync);
        pDevExt->pTscDeltaSync  = NULL;
        pDevExt->pvTscDeltaSync = NULL;
    }

    /*
     * Invalid the GIP data.
     */
    if (pDevExt->pGip)
    {
        supdrvGipTerm(pDevExt->pGip);
        pDevExt->pGip = NULL;
    }
    g_pSUPGlobalInfoPage = NULL;

    /*
     * Destroy the timer and free the GIP memory object.
     */
    if (pDevExt->pGipTimer)
    {
        rc = RTTimerDestroy(pDevExt->pGipTimer); AssertRC(rc);
        pDevExt->pGipTimer = NULL;
    }

    if (pDevExt->GipMemObj != NIL_RTR0MEMOBJ)
    {
        rc = RTR0MemObjFree(pDevExt->GipMemObj, true /* free mappings */); AssertRC(rc);
        pDevExt->GipMemObj = NIL_RTR0MEMOBJ;
    }

    /*
     * Finally, make sure we've release the system timer resolution request
     * if one actually succeeded and is still pending.
     */
    supdrvGipReleaseHigherTimerFrequencyFromSystem(pDevExt);
}




/*
 *
 *
 * GIP Update Timer Related Code
 * GIP Update Timer Related Code
 * GIP Update Timer Related Code
 *
 *
 */


/**
 * Worker routine for supdrvGipUpdate() and supdrvGipUpdatePerCpu() that
 * updates all the per cpu data except the transaction id.
 *
 * @param   pDevExt         The device extension.
 * @param   pGipCpu         Pointer to the per cpu data.
 * @param   u64NanoTS       The current time stamp.
 * @param   u64TSC          The current TSC.
 * @param   iTick           The current timer tick.
 *
 * @remarks Can be called with interrupts disabled!
 */
static void supdrvGipDoUpdateCpu(PSUPDRVDEVEXT pDevExt, PSUPGIPCPU pGipCpu, uint64_t u64NanoTS, uint64_t u64TSC, uint64_t iTick)
{
    uint64_t    u64TSCDelta;
    uint32_t    u32UpdateIntervalTSC;
    uint32_t    u32UpdateIntervalTSCSlack;
    unsigned    iTSCHistoryHead;
    uint64_t    u64CpuHz;
    uint32_t    u32TransactionId;

    PSUPGLOBALINFOPAGE pGip = pDevExt->pGip;
    AssertPtrReturnVoid(pGip);

    /* Delta between this and the previous update. */
    ASMAtomicUoWriteU32(&pGipCpu->u32PrevUpdateIntervalNS, (uint32_t)(u64NanoTS - pGipCpu->u64NanoTS));

    /*
     * Update the NanoTS.
     */
    ASMAtomicWriteU64(&pGipCpu->u64NanoTS, u64NanoTS);

    /*
     * Calc TSC delta.
     */
    u64TSCDelta = u64TSC - pGipCpu->u64TSC;
    ASMAtomicWriteU64(&pGipCpu->u64TSC, u64TSC);

    /* We don't need to keep realculating the frequency when it's invariant. */
    if (pGip->u32Mode == SUPGIPMODE_INVARIANT_TSC)
        return;

    if (u64TSCDelta >> 32)
    {
        u64TSCDelta = pGipCpu->u32UpdateIntervalTSC;
        pGipCpu->cErrors++;
    }

    /*
     * On the 2nd and 3rd callout, reset the history with the current TSC
     * interval since the values entered by supdrvGipInit are totally off.
     * The interval on the 1st callout completely unreliable, the 2nd is a bit
     * better, while the 3rd should be most reliable.
     */
    u32TransactionId = pGipCpu->u32TransactionId;
    if (RT_UNLIKELY(   (   u32TransactionId == 5
                        || u32TransactionId == 7)
                    && (   iTick == 2
                        || iTick == 3) ))
    {
        unsigned i;
        for (i = 0; i < RT_ELEMENTS(pGipCpu->au32TSCHistory); i++)
            ASMAtomicUoWriteU32(&pGipCpu->au32TSCHistory[i], (uint32_t)u64TSCDelta);
    }

    /*
     * Validate the NanoTS deltas between timer fires with an arbitrary threshold of 0.5%.
     * Wait until we have at least one full history since the above history reset. The
     * assumption is that the majority of the previous history values will be tolerable.
     * See @bugref{6710} comment #67.
     */
    if (   u32TransactionId > 23 /* 7 + (8 * 2) */
        && pGip->u32Mode != SUPGIPMODE_ASYNC_TSC)
    {
        uint32_t uNanoTsThreshold = pGip->u32UpdateIntervalNS / 200;
        if (   pGipCpu->u32PrevUpdateIntervalNS > pGip->u32UpdateIntervalNS + uNanoTsThreshold
            || pGipCpu->u32PrevUpdateIntervalNS < pGip->u32UpdateIntervalNS - uNanoTsThreshold)
        {
            uint32_t u32;
            u32  = pGipCpu->au32TSCHistory[0];
            u32 += pGipCpu->au32TSCHistory[1];
            u32 += pGipCpu->au32TSCHistory[2];
            u32 += pGipCpu->au32TSCHistory[3];
            u32 >>= 2;
            u64TSCDelta  = pGipCpu->au32TSCHistory[4];
            u64TSCDelta += pGipCpu->au32TSCHistory[5];
            u64TSCDelta += pGipCpu->au32TSCHistory[6];
            u64TSCDelta += pGipCpu->au32TSCHistory[7];
            u64TSCDelta >>= 2;
            u64TSCDelta += u32;
            u64TSCDelta >>= 1;
        }
    }

    /*
     * TSC History.
     */
    Assert(RT_ELEMENTS(pGipCpu->au32TSCHistory) == 8);
    iTSCHistoryHead = (pGipCpu->iTSCHistoryHead + 1) & 7;
    ASMAtomicWriteU32(&pGipCpu->iTSCHistoryHead, iTSCHistoryHead);
    ASMAtomicWriteU32(&pGipCpu->au32TSCHistory[iTSCHistoryHead], (uint32_t)u64TSCDelta);

    /*
     * UpdateIntervalTSC = average of last 8,2,1 intervals depending on update HZ.
     *
     * On Windows, we have an occasional (but recurring) sour value that messed up
     * the history but taking only 1 interval reduces the precision overall.
     * However, this problem existed before the invariant mode was introduced.
     */
    if (   pGip->u32Mode == SUPGIPMODE_INVARIANT_TSC
        || pGip->u32UpdateHz >= 1000)
    {
        uint32_t u32;
        u32  = pGipCpu->au32TSCHistory[0];
        u32 += pGipCpu->au32TSCHistory[1];
        u32 += pGipCpu->au32TSCHistory[2];
        u32 += pGipCpu->au32TSCHistory[3];
        u32 >>= 2;
        u32UpdateIntervalTSC  = pGipCpu->au32TSCHistory[4];
        u32UpdateIntervalTSC += pGipCpu->au32TSCHistory[5];
        u32UpdateIntervalTSC += pGipCpu->au32TSCHistory[6];
        u32UpdateIntervalTSC += pGipCpu->au32TSCHistory[7];
        u32UpdateIntervalTSC >>= 2;
        u32UpdateIntervalTSC += u32;
        u32UpdateIntervalTSC >>= 1;

        /* Value chosen for a 2GHz Athlon64 running linux 2.6.10/11. */
        u32UpdateIntervalTSCSlack = u32UpdateIntervalTSC >> 14;
    }
    else if (pGip->u32UpdateHz >= 90)
    {
        u32UpdateIntervalTSC  = (uint32_t)u64TSCDelta;
        u32UpdateIntervalTSC += pGipCpu->au32TSCHistory[(iTSCHistoryHead - 1) & 7];
        u32UpdateIntervalTSC >>= 1;

        /* value chosen on a 2GHz thinkpad running windows */
        u32UpdateIntervalTSCSlack = u32UpdateIntervalTSC >> 7;
    }
    else
    {
        u32UpdateIntervalTSC  = (uint32_t)u64TSCDelta;

        /* This value hasn't be checked yet.. waiting for OS/2 and 33Hz timers.. :-) */
        u32UpdateIntervalTSCSlack = u32UpdateIntervalTSC >> 6;
    }
    ASMAtomicWriteU32(&pGipCpu->u32UpdateIntervalTSC, u32UpdateIntervalTSC + u32UpdateIntervalTSCSlack);

    /*
     * CpuHz.
     */
    u64CpuHz = ASMMult2xU32RetU64(u32UpdateIntervalTSC, RT_NS_1SEC);
    u64CpuHz /= pGip->u32UpdateIntervalNS;
    ASMAtomicWriteU64(&pGipCpu->u64CpuHz, u64CpuHz);
}


/**
 * Updates the GIP.
 *
 * @param   pDevExt         The device extension.
 * @param   u64NanoTS       The current nanosecond timesamp.
 * @param   u64TSC          The current TSC timesamp.
 * @param   idCpu           The CPU ID.
 * @param   iTick           The current timer tick.
 *
 * @remarks Can be called with interrupts disabled!
 */
static void supdrvGipUpdate(PSUPDRVDEVEXT pDevExt, uint64_t u64NanoTS, uint64_t u64TSC, RTCPUID idCpu, uint64_t iTick)
{
    /*
     * Determine the relevant CPU data.
     */
    PSUPGIPCPU pGipCpu;
    PSUPGLOBALINFOPAGE pGip = pDevExt->pGip;
    AssertPtrReturnVoid(pGip);

    if (pGip->u32Mode != SUPGIPMODE_ASYNC_TSC)
        pGipCpu = &pGip->aCPUs[0];
    else
    {
        unsigned iCpu = pGip->aiCpuFromApicId[ASMGetApicId()];
        if (RT_UNLIKELY(iCpu >= pGip->cCpus))
            return;
        pGipCpu = &pGip->aCPUs[iCpu];
        if (RT_UNLIKELY(pGipCpu->idCpu != idCpu))
            return;
    }

    /*
     * Start update transaction.
     */
    if (!(ASMAtomicIncU32(&pGipCpu->u32TransactionId) & 1))
    {
        /* this can happen on win32 if we're taking to long and there are more CPUs around. shouldn't happen though. */
        AssertMsgFailed(("Invalid transaction id, %#x, not odd!\n", pGipCpu->u32TransactionId));
        ASMAtomicIncU32(&pGipCpu->u32TransactionId);
        pGipCpu->cErrors++;
        return;
    }

    /*
     * Recalc the update frequency every 0x800th time.
     */
    if (   pGip->u32Mode != SUPGIPMODE_INVARIANT_TSC   /* cuz we're not recalculating the frequency on invariants hosts. */
        && !(pGipCpu->u32TransactionId & (GIP_UPDATEHZ_RECALC_FREQ * 2 - 2)))
    {
        if (pGip->u64NanoTSLastUpdateHz)
        {
#ifdef RT_ARCH_AMD64 /** @todo fix 64-bit div here to work on x86 linux. */
            uint64_t u64Delta = u64NanoTS - pGip->u64NanoTSLastUpdateHz;
            uint32_t u32UpdateHz = (uint32_t)((RT_NS_1SEC_64 * GIP_UPDATEHZ_RECALC_FREQ) / u64Delta);
            if (u32UpdateHz <= 2000 && u32UpdateHz >= 30)
            {
                /** @todo r=ramshankar: Changing u32UpdateHz might screw up TSC frequency
                 *        calculation on non-invariant hosts if it changes the history decision
                 *        taken in supdrvGipDoUpdateCpu(). */
                uint64_t u64Interval = u64Delta / GIP_UPDATEHZ_RECALC_FREQ;
                ASMAtomicWriteU32(&pGip->u32UpdateHz, u32UpdateHz);
                ASMAtomicWriteU32(&pGip->u32UpdateIntervalNS, (uint32_t)u64Interval);
            }
#endif
        }
        ASMAtomicWriteU64(&pGip->u64NanoTSLastUpdateHz, u64NanoTS | 1);
    }

    /*
     * Update the data.
     */
    supdrvGipDoUpdateCpu(pDevExt, pGipCpu, u64NanoTS, u64TSC, iTick);

    /*
     * Complete transaction.
     */
    ASMAtomicIncU32(&pGipCpu->u32TransactionId);
}


/**
 * Updates the per cpu GIP data for the calling cpu.
 *
 * @param   pDevExt         The device extension.
 * @param   u64NanoTS       The current nanosecond timesamp.
 * @param   u64TSC          The current TSC timesamp.
 * @param   idCpu           The CPU ID.
 * @param   idApic          The APIC id for the CPU index.
 * @param   iTick           The current timer tick.
 *
 * @remarks Can be called with interrupts disabled!
 */
static void supdrvGipUpdatePerCpu(PSUPDRVDEVEXT pDevExt, uint64_t u64NanoTS, uint64_t u64TSC,
                                  RTCPUID idCpu, uint8_t idApic, uint64_t iTick)
{
    uint32_t iCpu;
    PSUPGLOBALINFOPAGE pGip = pDevExt->pGip;

    /*
     * Avoid a potential race when a CPU online notification doesn't fire on
     * the onlined CPU but the tick creeps in before the event notification is
     * run.
     */
    if (RT_UNLIKELY(iTick == 1))
    {
        iCpu = supdrvGipFindOrAllocCpuIndexForCpuId(pGip, idCpu);
        if (pGip->aCPUs[iCpu].enmState == SUPGIPCPUSTATE_OFFLINE)
            supdrvGipMpEventOnlineOrInitOnCpu(pDevExt, idCpu);
    }

    iCpu = pGip->aiCpuFromApicId[idApic];
    if (RT_LIKELY(iCpu < pGip->cCpus))
    {
        PSUPGIPCPU pGipCpu = &pGip->aCPUs[iCpu];
        if (pGipCpu->idCpu == idCpu)
        {
            /*
             * Start update transaction.
             */
            if (!(ASMAtomicIncU32(&pGipCpu->u32TransactionId) & 1))
            {
                AssertMsgFailed(("Invalid transaction id, %#x, not odd!\n", pGipCpu->u32TransactionId));
                ASMAtomicIncU32(&pGipCpu->u32TransactionId);
                pGipCpu->cErrors++;
                return;
            }

            /*
             * Update the data.
             */
            supdrvGipDoUpdateCpu(pDevExt, pGipCpu, u64NanoTS, u64TSC, iTick);

            /*
             * Complete transaction.
             */
            ASMAtomicIncU32(&pGipCpu->u32TransactionId);
        }
    }
}


/**
 * Timer callback function for the sync and invariant GIP modes.
 *
 * @param   pTimer      The timer.
 * @param   pvUser      Opaque pointer to the device extension.
 * @param   iTick       The timer tick.
 */
static DECLCALLBACK(void) supdrvGipSyncAndInvariantTimer(PRTTIMER pTimer, void *pvUser, uint64_t iTick)
{
    RTCCUINTREG        uFlags;
    uint64_t           u64TSC;
    uint64_t           u64NanoTS;
    PSUPDRVDEVEXT      pDevExt = (PSUPDRVDEVEXT)pvUser;
    PSUPGLOBALINFOPAGE pGip = pDevExt->pGip;

    uFlags    = ASMIntDisableFlags(); /* No interruptions please (real problem on S10). */
    u64TSC    = ASMReadTSC();
    u64NanoTS = RTTimeSystemNanoTS();

    if (pGip->enmUseTscDelta > SUPGIPUSETSCDELTA_PRACTICALLY_ZERO)
    {
        /*
         * The calculations in supdrvGipUpdate() is very timing sensitive and doesn't handle
         * missed timer ticks. So for now it is better to use a delta of 0 and have the TSC rate
         * affected a bit until we get proper TSC deltas than implementing options like
         * rescheduling the tick to be delivered on the right CPU or missing the tick entirely.
         *
         * The likely hood of this happening is really low. On Windows, Linux, and Solaris
         * timers fire on the CPU they were registered/started on.  Darwin timers doesn't
         * necessarily (they are high priority threads waiting).
         */
        Assert(!ASMIntAreEnabled());
        supdrvTscDeltaApply(pGip, &u64TSC, ASMGetApicId(), NULL /* pfDeltaApplied */);
    }

    supdrvGipUpdate(pDevExt, u64NanoTS, u64TSC, NIL_RTCPUID, iTick);

    ASMSetFlags(uFlags);

#ifdef SUPDRV_USE_TSC_DELTA_THREAD
    if (   pGip->enmUseTscDelta > SUPGIPUSETSCDELTA_ZERO_CLAIMED
        && !RTCpuSetIsEmpty(&pDevExt->TscDeltaCpuSet))
    {
        RTSpinlockAcquire(pDevExt->hTscDeltaSpinlock);
        if (   pDevExt->enmTscDeltaThreadState == kTscDeltaThreadState_Listening
            || pDevExt->enmTscDeltaThreadState == kTscDeltaThreadState_Measuring)
            pDevExt->enmTscDeltaThreadState = kTscDeltaThreadState_WaitAndMeasure;
        RTSpinlockRelease(pDevExt->hTscDeltaSpinlock);
        /** @todo Do the actual poking using -- RTThreadUserSignal() */
    }
#endif
}


/**
 * Timer callback function for async GIP mode.
 * @param   pTimer      The timer.
 * @param   pvUser      Opaque pointer to the device extension.
 * @param   iTick       The timer tick.
 */
static DECLCALLBACK(void) supdrvGipAsyncTimer(PRTTIMER pTimer, void *pvUser, uint64_t iTick)
{
    RTCCUINTREG     fOldFlags = ASMIntDisableFlags(); /* No interruptions please (real problem on S10). */
    PSUPDRVDEVEXT   pDevExt   = (PSUPDRVDEVEXT)pvUser;
    RTCPUID         idCpu     = RTMpCpuId();
    uint64_t        u64TSC    = ASMReadTSC();
    uint64_t        NanoTS    = RTTimeSystemNanoTS();

    /** @todo reset the transaction number and whatnot when iTick == 1. */
    if (pDevExt->idGipMaster == idCpu)
        supdrvGipUpdate(pDevExt, NanoTS, u64TSC, idCpu, iTick);
    else
        supdrvGipUpdatePerCpu(pDevExt, NanoTS, u64TSC, idCpu, ASMGetApicId(), iTick);

    ASMSetFlags(fOldFlags);
}




/*
 *
 *
 * TSC Delta Measurements And Related Code
 * TSC Delta Measurements And Related Code
 * TSC Delta Measurements And Related Code
 *
 *
 */


/*
 * Select TSC delta measurement algorithm.
 */
#if 1
# define GIP_TSC_DELTA_METHOD_1
#else
# define GIP_TSC_DELTA_METHOD_2
#endif


#ifdef GIP_TSC_DELTA_METHOD_2

/**
 * TSC delta measurment algorithm \#2 result entry.
 */
typedef struct SUPDRVTSCDELTAMETHOD2ENTRY
{
    uint32_t    iSeqMine;
    uint32_t    iSeqOther;
    uint64_t    uTsc;
} SUPDRVTSCDELTAMETHOD2ENTRY;

/**
 * TSC delta measurment algorithm \#2 Data.
 */
typedef struct SUPDRVTSCDELTAMETHOD2
{
    /** Padding to make sure the iCurSeqNo is in its own cache line.
     * ASSUMES cacheline sizes <= 128 bytes. */
    uint32_t                    au32CacheLinePaddingBefore[128 / sizeof(uint32_t)];
    /** The current sequence number of this worker. */
    uint32_t volatile           iCurSeqNo;
    /** Padding to make sure the iCurSeqNo is in its own cache line.
     * ASSUMES cacheline sizes <= 128 bytes. */
    uint32_t                    au32CacheLinePaddingAfter[128 / sizeof(uint32_t) - 1];
    /** Result table. */
    SUPDRVTSCDELTAMETHOD2ENTRY  aResults[96];
} SUPDRVTSCDELTAMETHOD2;
/** Pointer to the data for TSC delta mesurment algorithm \#2 .*/
typedef SUPDRVTSCDELTAMETHOD2 *PSUPDRVTSCDELTAMETHOD2;

#endif /* GIP_TSC_DELTA_METHOD_2 */

/**
 * Argument package/state passed by supdrvMeasureTscDeltaOne to the RTMpOn
 * callback worker.
 */
typedef struct SUPDRVGIPTSCDELTARGS
{
    PSUPDRVDEVEXT           pDevExt;
    PSUPGIPCPU              pWorker;
    PSUPGIPCPU              pMaster;
    RTCPUID                 idMaster;
#ifdef GIP_TSC_DELTA_METHOD_2
    PSUPDRVTSCDELTAMETHOD2  pMasterData;
    PSUPDRVTSCDELTAMETHOD2  pWorkerData;
    uint32_t                cHits;
    /*uint32_t                cOffByOne;*/
    uint32_t                iAttempt;       /**< 1-base outer loop counter. */
    bool                    fLagMaster;
    bool                    fLagWorker;
#endif
} SUPDRVGIPTSCDELTARGS;
typedef SUPDRVGIPTSCDELTARGS *PSUPDRVGIPTSCDELTARGS;


#ifdef GIP_TSC_DELTA_METHOD_2
/*
 * TSC delta measurement algorithm \#2 configuration and code - Experimental!!
 */
# undef  GIP_TSC_DELTA_LOOPS
# undef  GIP_TSC_DELTA_READ_TIME_LOOPS
# undef  GIP_TSC_DELTA_PRIMER_LOOPS
# define GIP_TSC_DELTA_LOOPS             17
# define GIP_TSC_DELTA_PRIMER_LOOPS      1
# define GIP_TSC_DELTA_READ_TIME_LOOPS   GIP_TSC_DELTA_PRIMER_LOOPS /* no read-time-loops necessary */


static int supdrvTscDeltaMethod2Init(PSUPDRVGIPTSCDELTARGS pArgs)
{
    uint32_t const fFlags = /*RTMEMALLOCEX_FLAGS_ANY_CTX |*/ RTMEMALLOCEX_FLAGS_ZEROED;
    int rc = RTMemAllocEx(sizeof(*pArgs->pMasterData), 0, fFlags, (void **)&pArgs->pWorkerData);
    if (RT_SUCCESS(rc))
        rc = RTMemAllocEx(sizeof(*pArgs->pMasterData), 0, fFlags, (void **)&pArgs->pMasterData);
    return rc;
}


static void supdrvTscDeltaMethod2Term(PSUPDRVGIPTSCDELTARGS pArgs)
{
    RTMemFreeEx(pArgs->pMasterData, sizeof(*pArgs->pMasterData));
    RTMemFreeEx(pArgs->pWorkerData, sizeof(*pArgs->pWorkerData));
    /*SUPR0Printf("cHits=%d cOffByOne=%d m=%d w=%d\n", pArgs->cHits, pArgs->cOffByOne, pArgs->pMaster->idApic, pArgs->pWorker->idApic);*/
}


static void supdrvTscDeltaMethod2Looped(PSUPDRVGIPTSCDELTARGS pArgs, RTCPUID idCpu, unsigned iLoop)
{
    if (pArgs->idMaster == idCpu)
    {
        if (iLoop < GIP_TSC_DELTA_PRIMER_LOOPS)
        {
            if (iLoop == 0)
                pArgs->iAttempt++;

            /* Lag during the priming to be nice to everyone.. */
            pArgs->fLagMaster = true;
            pArgs->fLagWorker = true;
        }
        else if (iLoop < (GIP_TSC_DELTA_LOOPS - GIP_TSC_DELTA_PRIMER_LOOPS) / 4)
        {
            /* 25 % of the body without lagging. */
            pArgs->fLagMaster = false;
            pArgs->fLagWorker = false;
        }
        else if (iLoop < (GIP_TSC_DELTA_LOOPS - GIP_TSC_DELTA_PRIMER_LOOPS) / 4 * 2)
        {
            /* 25 % of the body with both lagging. */
            pArgs->fLagMaster = true;
            pArgs->fLagWorker = true;
        }
        else
        {
            /* 50% of the body with alternating lag. */
            pArgs->fLagMaster = (iLoop & 1) == 0;
            pArgs->fLagWorker = (iLoop & 1) == 1;
        }
    }
}


/**
 * The core function of the 2nd TSC delta mesurment algorithm.
 *
 * The idea here is that we have the two CPUs execute the exact same code
 * collecting a largish set of TSC samples.  The code has one data dependency on
 * the other CPU which intention it is to synchronize the execution as well as
 * help cross references the two sets of TSC samples (the sequence numbers).
 *
 * The @a fLag parameter is used to modify the execution a tiny bit on one or
 * both of the CPUs.  When @a fLag differs between the CPUs, it is thought that
 * it will help with making the CPUs enter lock step execution occationally.
 *
 */
static void supdrvTscDeltaMethod2CollectData(PSUPDRVTSCDELTAMETHOD2 pMyData, uint32_t volatile *piOtherSeqNo, bool fLag)
{
    SUPDRVTSCDELTAMETHOD2ENTRY *pEntry = &pMyData->aResults[0];
    uint32_t                    cLeft  = RT_ELEMENTS(pMyData->aResults);

    ASMAtomicWriteU32(&pMyData->iCurSeqNo, 0);
    ASMSerializeInstruction();
    while (cLeft-- > 0)
    {
        uint64_t uTsc;
        uint32_t iSeqMine  = ASMAtomicIncU32(&pMyData->iCurSeqNo);
        uint32_t iSeqOther = ASMAtomicReadU32(piOtherSeqNo);
        ASMCompilerBarrier();
        ASMSerializeInstruction(); /* Way better result than with ASMMemoryFenceSSE2() in this position! */
        uTsc = ASMReadTSC();
        ASMAtomicIncU32(&pMyData->iCurSeqNo);
        ASMCompilerBarrier();
        ASMSerializeInstruction();
        pEntry->iSeqMine  = iSeqMine;
        pEntry->iSeqOther = iSeqOther;
        pEntry->uTsc      = uTsc;
        pEntry++;
        ASMSerializeInstruction();
        if (fLag)
            ASMNopPause();
    }
}


static void supdrvTscDeltaMethod2ProcessDataSet(PSUPDRVGIPTSCDELTARGS pArgs, PSUPDRVTSCDELTAMETHOD2 pMyData,
                                                bool fIsMaster, uint32_t cResults,
                                                PSUPDRVTSCDELTAMETHOD2 pOtherData, int64_t iMasterTscDelta,
                                                int64_t volatile *piWorkerTscDelta)
{
    uint32_t cHits      = 0;
#if 0
    uint32_t cOffByOne  = 0;
#endif
    uint32_t idxResult  = 0;
    int64_t  iBestDelta = *piWorkerTscDelta;

    if (cResults > RT_ELEMENTS(pMyData->aResults))
        cResults = RT_ELEMENTS(pMyData->aResults);

    for (idxResult = 0; idxResult < cResults; idxResult++)
    {
        uint32_t idxOther = pMyData->aResults[idxResult].iSeqOther;
        if (idxOther & 1)
        {
            idxOther >>= 1;
            if (idxOther < RT_ELEMENTS(pOtherData->aResults))
            {
                if (pOtherData->aResults[idxOther].iSeqOther == pMyData->aResults[idxResult].iSeqMine)
                {
                    int64_t iDelta;
                    if (fIsMaster)
                        iDelta = pOtherData->aResults[idxOther].uTsc
                               - (pMyData->aResults[idxResult].uTsc - iMasterTscDelta);
                    else
                        iDelta = (pOtherData->aResults[idxResult].uTsc - iMasterTscDelta)
                               - pMyData->aResults[idxOther].uTsc;
                    if (  iDelta >= GIP_TSC_DELTA_INITIAL_MASTER_VALUE
                        ? iDelta < iBestDelta
                        : iDelta > iBestDelta || iBestDelta == INT64_MAX)
                        iBestDelta = iDelta;
                    cHits++;
                }
            }
        }
#if 0  /* Can be used to detect battles between threads on the same core. Decided to change the master instead.  */
        else
        {
            idxOther >>= 1;
            if (   idxOther < RT_ELEMENTS(pOtherData->aResults)
                && pOtherData->aResults[idxOther].iSeqOther == pMyData->aResults[idxResult].iSeqMine)
                cOffByOne++;
        }
#endif
    }

    if (cHits > 0)
        *piWorkerTscDelta = iBestDelta;
    pArgs->cHits     += cHits;
#if 0
    pArgs->cOffByOne += cOffByOne;
#endif
}


static void supdrvTscDeltaMethod2ProcessDataOnMaster(PSUPDRVGIPTSCDELTARGS pArgs, bool fFinalLoop)
{
    supdrvTscDeltaMethod2ProcessDataSet(pArgs,
                                        pArgs->pMasterData,
                                        true /*fIsMaster*/,
                                        RT_ELEMENTS(pArgs->pMasterData->aResults),
                                        pArgs->pWorkerData,
                                        pArgs->pMaster->i64TSCDelta,
                                        &pArgs->pWorker->i64TSCDelta);

    supdrvTscDeltaMethod2ProcessDataSet(pArgs,
                                        pArgs->pWorkerData,
                                        false /*fIsMaster*/,
                                        ASMAtomicReadU32(&pArgs->pWorkerData->iCurSeqNo) >> 1,
                                        pArgs->pMasterData,
                                        pArgs->pMaster->i64TSCDelta,
                                        &pArgs->pWorker->i64TSCDelta);
}

#endif /* GIP_TSC_DELTA_METHOD_2 */


/**
 * Callback used by supdrvMeasureInitialTscDeltas() to read the TSC on two CPUs
 * and compute the delta between them.
 *
 * @param   idCpu       The CPU we are current scheduled on.
 * @param   pvUser1     Pointer to a parameter package (SUPDRVGIPTSCDELTARGS).
 * @param   pvUser2     Unused.
 *
 * @remarks Measuring TSC deltas between the CPUs is tricky because we need to
 *          read the TSC at exactly the same time on both the master and the
 *          worker CPUs. Due to DMA, bus arbitration, cache locality,
 *          contention, SMI, pipelining etc. there is no guaranteed way of
 *          doing this on x86 CPUs.
 *
 *          GIP_TSC_DELTA_METHOD_1:
 *          We ignore the first few runs of the loop in order to prime the
 *          cache. Also, we need to be careful about using 'pause' instruction
 *          in critical busy-wait loops in this code - it can cause undesired
 *          behaviour with hyperthreading.
 *
 *          We try to minimize the measurement error by computing the minimum
 *          read time of the compare statement in the worker by taking TSC
 *          measurements across it.
 *
 *          It must be noted that the computed minimum read time is mostly to
 *          eliminate huge deltas when the worker is too early and doesn't by
 *          itself help produce more accurate deltas. We allow two times the
 *          computed minimum as an arbibtrary acceptable threshold. Therefore,
 *          it is still possible to get negative deltas where there are none
 *          when the worker is earlier. As long as these occasional negative
 *          deltas are lower than the time it takes to exit guest-context and
 *          the OS to reschedule EMT on a different CPU we won't expose a TSC
 *          that jumped backwards. It is because of the existence of the
 *          negative deltas we don't recompute the delta with the master and
 *          worker interchanged to eliminate the remaining measurement error.
 *
 *          For GIP_TSC_DELTA_METHOD_2, see supdrvTscDeltaMethod2CollectData.
 */
static DECLCALLBACK(void) supdrvMeasureTscDeltaCallback(RTCPUID idCpu, void *pvUser1, void *pvUser2)
{
    PSUPDRVGIPTSCDELTARGS pArgs = (PSUPDRVGIPTSCDELTARGS)pvUser1;
    PSUPDRVDEVEXT      pDevExt          = pArgs->pDevExt;
    PSUPGIPCPU         pGipCpuWorker    = pArgs->pWorker;
    PSUPGIPCPU         pGipCpuMaster    = pArgs->pMaster;
    RTCPUID            idMaster         = pArgs->idMaster;
    int                cTriesLeft;

    /* A bit of paranoia first. */
    if (!pGipCpuMaster || !pGipCpuWorker)
        return;

    /* If the CPU isn't part of the measurement, return immediately. */
    if (   idCpu != idMaster
        && idCpu != pGipCpuWorker->idCpu)
        return;

    /* If the IPRT API isn't concurrent safe, the master and worker wait for each other
       with a timeout to avoid deadlocking the entire system. */
    if (!RTMpOnAllIsConcurrentSafe())
    {
        /** @todo This was introduced for Windows, but since Windows doesn't use this
         *        code path any longer (as DPC timeouts BSOD regardless of interrupts,
         *        see @bugref{6710} comment 81), eventually phase it out. */
        uint64_t       uTscNow;
        uint64_t       uTscStart;
        uint64_t const cWaitTicks = 130000;  /* Arbitrary value, can be tweaked later. */

        ASMSerializeInstruction();
        uTscStart = ASMReadTSC();
        if (idCpu == idMaster)
        {
            ASMAtomicWriteU32(&pDevExt->pTscDeltaSync->u, GIP_TSC_DELTA_SYNC_PRESTART_MASTER);
            while (ASMAtomicReadU32(&pDevExt->pTscDeltaSync->u) != GIP_TSC_DELTA_SYNC_PRESTART_WORKER)
            {
                ASMSerializeInstruction();
                uTscNow = ASMReadTSC();
                if (uTscNow - uTscStart > cWaitTicks)
                {
                    /* Set the worker delta to indicate failure, not the master. */
                    ASMAtomicWriteS64(&pGipCpuWorker->i64TSCDelta, INT64_MAX);
                    return;
                }

                ASMNopPause();
            }
        }
        else
        {
            while (ASMAtomicReadU32(&pDevExt->pTscDeltaSync->u) != GIP_TSC_DELTA_SYNC_PRESTART_MASTER)
            {
                ASMSerializeInstruction();
                uTscNow = ASMReadTSC();
                if (uTscNow - uTscStart > cWaitTicks)
                {
                    ASMAtomicWriteS64(&pGipCpuWorker->i64TSCDelta, INT64_MAX);
                    return;
                }

                ASMNopPause();
            }
            ASMAtomicWriteU32(&pDevExt->pTscDeltaSync->u, GIP_TSC_DELTA_SYNC_PRESTART_WORKER);
        }
    }

    /*
     * ...
     */
    Assert(pGipCpuWorker->i64TSCDelta == INT64_MAX);
    cTriesLeft = 12;
    while (cTriesLeft-- > 0)
    {
        unsigned i;
        uint64_t uMinCmpReadTime = UINT64_MAX;
        for (i = 0; i < GIP_TSC_DELTA_LOOPS; i++)
        {
#ifdef GIP_TSC_DELTA_METHOD_2
            supdrvTscDeltaMethod2Looped(pArgs, idCpu, i);
#endif
            if (idCpu == idMaster)
            {
                /*
                 * The master.
                 */
                RTCCUINTREG uFlags;
                AssertMsg(pGipCpuMaster->u64TSCSample == GIP_TSC_DELTA_RSVD,
                          ("%#llx idMaster=%#x idWorker=%#x (idGipMaster=%#x)\n",
                           pGipCpuMaster->u64TSCSample, idMaster, pGipCpuWorker->idCpu, pDevExt->idGipMaster));
                ASMAtomicWriteU32(&pDevExt->pTscDeltaSync->u, GIP_TSC_DELTA_SYNC_START);

                /* Disable interrupts only in the master for as short a period
                   as possible, thanks again to Windows. See @bugref{6710} comment #73. */
                uFlags = ASMIntDisableFlags();

                while (ASMAtomicReadU32(&pDevExt->pTscDeltaSync->u) == GIP_TSC_DELTA_SYNC_START)
                { /* nothing */ }

#ifdef GIP_TSC_DELTA_METHOD_1
                do
                {
                    ASMSerializeInstruction();
                    ASMAtomicWriteU64(&pGipCpuMaster->u64TSCSample, ASMReadTSC());
                } while (pGipCpuMaster->u64TSCSample == GIP_TSC_DELTA_RSVD);

#elif defined(GIP_TSC_DELTA_METHOD_2)
                supdrvTscDeltaMethod2CollectData(pArgs->pMasterData, &pArgs->pWorkerData->iCurSeqNo, pArgs->fLagMaster);
#else
# error "tsc delta method not selected"
#endif

                /* Sync up with worker. */
                ASMSetFlags(uFlags);

                while (ASMAtomicReadU32(&pDevExt->pTscDeltaSync->u) != GIP_TSC_DELTA_SYNC_WORKER_DONE)
                { /* nothing */ }

                /* Process the data. */
                if (i > GIP_TSC_DELTA_PRIMER_LOOPS + GIP_TSC_DELTA_READ_TIME_LOOPS)
                {
#ifdef GIP_TSC_DELTA_METHOD_1
                    if (pGipCpuWorker->u64TSCSample != GIP_TSC_DELTA_RSVD)
                    {
                        int64_t iDelta = pGipCpuWorker->u64TSCSample
                                       - (pGipCpuMaster->u64TSCSample - pGipCpuMaster->i64TSCDelta);
                        if (  iDelta >= GIP_TSC_DELTA_INITIAL_MASTER_VALUE
                            ? iDelta < pGipCpuWorker->i64TSCDelta
                            : iDelta > pGipCpuWorker->i64TSCDelta || pGipCpuWorker->i64TSCDelta == INT64_MAX)
                            pGipCpuWorker->i64TSCDelta = iDelta;
                    }
#elif defined(GIP_TSC_DELTA_METHOD_2)
                    if (i > GIP_TSC_DELTA_PRIMER_LOOPS)
                        supdrvTscDeltaMethod2ProcessDataOnMaster(pArgs, i == GIP_TSC_DELTA_LOOPS - 1);
#else
# error "tsc delta method not selected"
#endif
                }

                /* Reset our TSC sample and tell the worker to move on. */
                ASMAtomicWriteU64(&pGipCpuMaster->u64TSCSample, GIP_TSC_DELTA_RSVD);
                ASMAtomicWriteU32(&pDevExt->pTscDeltaSync->u, GIP_TSC_DELTA_SYNC_STOP);
            }
            else
            {
                /*
                 * The worker.
                 */
                uint64_t uTscWorker;
                uint64_t uTscWorkerFlushed;
                uint64_t uCmpReadTime;

                ASMAtomicReadU64(&pGipCpuMaster->u64TSCSample);     /* Warm the cache line. */
                while (ASMAtomicReadU32(&pDevExt->pTscDeltaSync->u) != GIP_TSC_DELTA_SYNC_START)
                { /* nothing */ }
                Assert(pGipCpuMaster->u64TSCSample == GIP_TSC_DELTA_RSVD);
                ASMAtomicWriteU32(&pDevExt->pTscDeltaSync->u, GIP_TSC_DELTA_SYNC_WORKER_READY);

#ifdef GIP_TSC_DELTA_METHOD_1
                /*
                 * Keep reading the TSC until we notice that the master has read his. Reading
                 * the TSC -after- the master has updated the memory is way too late. We thus
                 * compensate by trying to measure how long it took for the worker to notice
                 * the memory flushed from the master.
                 */
                do
                {
                    ASMSerializeInstruction();
                    uTscWorker = ASMReadTSC();
                } while (pGipCpuMaster->u64TSCSample == GIP_TSC_DELTA_RSVD);
                ASMSerializeInstruction();
                uTscWorkerFlushed = ASMReadTSC();

                uCmpReadTime = uTscWorkerFlushed - uTscWorker;
                if (i > GIP_TSC_DELTA_PRIMER_LOOPS + GIP_TSC_DELTA_READ_TIME_LOOPS)
                {
                    /* This is totally arbitrary a.k.a I don't like it but I have no better ideas for now. */
                    if (uCmpReadTime < (uMinCmpReadTime << 1))
                    {
                        ASMAtomicWriteU64(&pGipCpuWorker->u64TSCSample, uTscWorker);
                        if (uCmpReadTime < uMinCmpReadTime)
                            uMinCmpReadTime = uCmpReadTime;
                    }
                    else
                        ASMAtomicWriteU64(&pGipCpuWorker->u64TSCSample, GIP_TSC_DELTA_RSVD);
                }
                else if (i > GIP_TSC_DELTA_PRIMER_LOOPS)
                {
                    if (uCmpReadTime < uMinCmpReadTime)
                        uMinCmpReadTime = uCmpReadTime;
                }

#elif defined(GIP_TSC_DELTA_METHOD_2)
                supdrvTscDeltaMethod2CollectData(pArgs->pWorkerData, &pArgs->pMasterData->iCurSeqNo, pArgs->fLagWorker);
#else
# error "tsc delta method not selected"
#endif

                /* Tell master we're done collecting our data. */
                ASMAtomicWriteU32(&pDevExt->pTscDeltaSync->u, GIP_TSC_DELTA_SYNC_WORKER_DONE);

                /* Wait for the master to process the data. */
                while (ASMAtomicReadU32(&pDevExt->pTscDeltaSync->u) == GIP_TSC_DELTA_SYNC_WORKER_DONE)
                    ASMNopPause();
            }
        }

        /*
         * We must reset the worker TSC sample value in case it gets picked as a
         * GIP master later on (it's trashed above, naturally).
         */
        if (idCpu == idMaster)
            ASMAtomicWriteU64(&pGipCpuWorker->u64TSCSample, GIP_TSC_DELTA_RSVD);

        /*
         * Success? If so, stop trying.
         */
        if (pGipCpuWorker->i64TSCDelta != INT64_MAX)
        {
            if (idCpu == idMaster)
            {
                RTCpuSetDelByIndex(&pDevExt->TscDeltaCpuSet, pGipCpuMaster->iCpuSet);
                RTCpuSetAddByIndex(&pDevExt->TscDeltaObtainedCpuSet, pGipCpuMaster->iCpuSet);
            }
            else
            {
                RTCpuSetDelByIndex(&pDevExt->TscDeltaCpuSet, pGipCpuWorker->iCpuSet);
                RTCpuSetAddByIndex(&pDevExt->TscDeltaObtainedCpuSet, pGipCpuWorker->iCpuSet);
            }
            break;
        }
    }
}


/**
 * Clears TSC delta related variables.
 *
 * Clears all TSC samples as well as the delta synchronization variable on the
 * all the per-CPU structs.  Optionally also clears the per-cpu deltas too.
 *
 * @param   pDevExt         Pointer to the device instance data.
 * @param   fClearDeltas    Whether the deltas are also to be cleared.
 */
DECLINLINE(void) supdrvClearTscSamples(PSUPDRVDEVEXT pDevExt, bool fClearDeltas)
{
    unsigned iCpu;
    PSUPGLOBALINFOPAGE pGip = pDevExt->pGip;
    for (iCpu = 0; iCpu < pGip->cCpus; iCpu++)
    {
        PSUPGIPCPU pGipCpu = &pGip->aCPUs[iCpu];
        ASMAtomicWriteU64(&pGipCpu->u64TSCSample, GIP_TSC_DELTA_RSVD);
        if (fClearDeltas)
            ASMAtomicWriteS64(&pGipCpu->i64TSCDelta, INT64_MAX);
    }
    ASMAtomicWriteU32(&pDevExt->pTscDeltaSync->u, GIP_TSC_DELTA_SYNC_STOP);
}


/**
 * Measures the TSC delta between the master GIP CPU and one specified worker
 * CPU.
 *
 * @returns VBox status code.
 * @param   pDevExt         Pointer to the device instance data.
 * @param   idxWorker       The index of the worker CPU from the GIP's array of
 *                          CPUs.
 *
 * @remarks This must be called with preemption enabled!
 */
static int supdrvMeasureTscDeltaOne(PSUPDRVDEVEXT pDevExt, uint32_t idxWorker)
{
    int                 rc;
    PSUPGLOBALINFOPAGE  pGip          = pDevExt->pGip;
    RTCPUID             idMaster      = pDevExt->idGipMaster;
    PSUPGIPCPU          pGipCpuWorker = &pGip->aCPUs[idxWorker];
    PSUPGIPCPU          pGipCpuMaster;
    uint32_t            iGipCpuMaster;

    /* Validate input a bit. */
    AssertReturn(pGip, VERR_INVALID_PARAMETER);
    Assert(pGip->enmUseTscDelta > SUPGIPUSETSCDELTA_ZERO_CLAIMED);
    Assert(RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    /*
     * Don't attempt measuring the delta for the GIP master.
     */
    if (pGipCpuWorker->idCpu == idMaster)
    {
        if (pGipCpuWorker->i64TSCDelta == INT64_MAX) /* This shouldn't happen, but just in case. */
            ASMAtomicWriteS64(&pGipCpuWorker->i64TSCDelta, GIP_TSC_DELTA_INITIAL_MASTER_VALUE);
        return VINF_SUCCESS;
    }

    /*
     * If the CPU has hyper-threading and the APIC IDs of the master and worker are adjacent,
     * try pick a different master.  (This fudge only works with multi core systems.)
     * ASSUMES related threads have adjacent APIC IDs.  ASSUMES two threads per core.
     */
    iGipCpuMaster = supdrvGipFindCpuIndexForCpuId(pGip, idMaster);
    AssertReturn(iGipCpuMaster < pGip->cCpus, VERR_INVALID_CPU_ID);
    pGipCpuMaster = &pGip->aCPUs[iGipCpuMaster];
    if (   (pGipCpuMaster->idApic & ~1) == (pGipCpuWorker->idApic & ~1)
        && ASMHasCpuId()
        && ASMIsValidStdRange(ASMCpuId_EAX(0))
        && (ASMCpuId_EDX(1) & X86_CPUID_FEATURE_EDX_HTT)
        && pGip->cOnlineCpus > 2)
    {
        uint32_t i;
        for (i = 0; i < pGip->cCpus; i++)
            if (   i != iGipCpuMaster
                && i != idxWorker
                && pGip->aCPUs[i].enmState == SUPGIPCPUSTATE_ONLINE
                && pGip->aCPUs[i].i64TSCDelta != INT64_MAX
                && pGip->aCPUs[i].idCpu  != NIL_RTCPUID
                && pGip->aCPUs[i].idCpu  != idMaster              /* paranoia starts here... */
                && pGip->aCPUs[i].idCpu  != pGipCpuWorker->idCpu
                && pGip->aCPUs[i].idApic != pGipCpuWorker->idApic
                && pGip->aCPUs[i].idApic != pGipCpuMaster->idApic)
            {
                iGipCpuMaster = i;
                pGipCpuMaster = &pGip->aCPUs[i];
                idMaster = pGipCpuMaster->idCpu;
                break;
            }
    }

    /*
     * Set the master TSC as the initiator.  This serializes delta measurments.
     */
    while (!ASMAtomicCmpXchgU32(&pDevExt->idTscDeltaInitiator, idMaster, NIL_RTCPUID))
    {
        /*
         * Sleep here rather than spin as there is a parallel measurement
         * being executed and that can take a good while to be done.
         */
        RTThreadSleep(1);
    }

    if (RTCpuSetIsMemberByIndex(&pGip->OnlineCpuSet, pGipCpuWorker->iCpuSet))
    {
        /*
         * Initialize data package for the RTMpOnAll callback.
         */
        SUPDRVGIPTSCDELTARGS Args;
        RT_ZERO(Args);
        Args.pWorker  = pGipCpuWorker;
        Args.pMaster  = pGipCpuMaster;
        Args.idMaster = idMaster;
        Args.pDevExt  = pDevExt;
#ifdef GIP_TSC_DELTA_METHOD_1
        rc = VINF_SUCCESS;
#elif defined(GIP_TSC_DELTA_METHOD_2)
        rc = supdrvTscDeltaMethod2Init(&Args);
#else
# error "huh?"
#endif
        if (RT_SUCCESS(rc))
        {
            /*
             * Fire TSC-read workers on all CPUs but only synchronize between master
             * and one worker to ease memory contention.
             */
            ASMAtomicWriteS64(&pGipCpuWorker->i64TSCDelta, INT64_MAX);
            ASMAtomicWriteU32(&pDevExt->pTscDeltaSync->u, GIP_TSC_DELTA_SYNC_STOP);

            rc = RTMpOnAll(supdrvMeasureTscDeltaCallback, &Args, NULL);
            if (RT_SUCCESS(rc))
            {
                if (RT_LIKELY(pGipCpuWorker->i64TSCDelta != INT64_MAX))
                {
                    /*
                     * Work the TSC delta applicability rating.  It starts
                     * optimistic in supdrvGipInit, we downgrade it here.
                     */
                    SUPGIPUSETSCDELTA enmRating;
                    if (   pGipCpuWorker->i64TSCDelta >  GIP_TSC_DELTA_THRESHOLD_ROUGHLY_ZERO
                        || pGipCpuWorker->i64TSCDelta < -GIP_TSC_DELTA_THRESHOLD_ROUGHLY_ZERO)
                        enmRating = SUPGIPUSETSCDELTA_NOT_ZERO;
                    else if (   pGipCpuWorker->i64TSCDelta >  GIP_TSC_DELTA_THRESHOLD_PRACTICALLY_ZERO
                             || pGipCpuWorker->i64TSCDelta < -GIP_TSC_DELTA_THRESHOLD_PRACTICALLY_ZERO)
                        enmRating = SUPGIPUSETSCDELTA_ROUGHLY_ZERO;
                    else
                        enmRating = SUPGIPUSETSCDELTA_PRACTICALLY_ZERO;
                    if (pGip->enmUseTscDelta < enmRating)
                    {
                        AssertCompile(sizeof(pGip->enmUseTscDelta) == sizeof(uint32_t));
                        ASMAtomicWriteU32((uint32_t volatile *)&pGip->enmUseTscDelta, enmRating);
                    }
                }
                else
                    rc = VERR_SUPDRV_TSC_DELTA_MEASUREMENT_FAILED;
            }
        }

#ifdef GIP_TSC_DELTA_METHOD_2
        supdrvTscDeltaMethod2Term(&Args);
#endif
    }
    else
        rc = VERR_CPU_OFFLINE;

    ASMAtomicWriteU32(&pDevExt->idTscDeltaInitiator, NIL_RTCPUID);
    return rc;
}


/**
 * Performs the initial measurements of the TSC deltas between CPUs.
 *
 * This is called by supdrvGipCreate or triggered by it if threaded.
 *
 * @returns VBox status code.
 * @param   pDevExt     Pointer to the device instance data.
 *
 * @remarks Must be called only after supdrvGipInitOnCpu() as this function uses
 *          idCpu, GIP's online CPU set which are populated in
 *          supdrvGipInitOnCpu().
 */
static int supdrvMeasureInitialTscDeltas(PSUPDRVDEVEXT pDevExt)
{
    PSUPGIPCPU pGipCpuMaster;
    unsigned   iCpu;
    unsigned   iOddEven;
    PSUPGLOBALINFOPAGE pGip   = pDevExt->pGip;
    uint32_t   idxMaster      = UINT32_MAX;
    int        rc             = VINF_SUCCESS;
    uint32_t   cMpOnOffEvents = ASMAtomicReadU32(&pDevExt->cMpOnOffEvents);

    Assert(pGip->enmUseTscDelta > SUPGIPUSETSCDELTA_ZERO_CLAIMED);

    /*
     * Pick the first CPU online as the master TSC and make it the new GIP master based
     * on the APIC ID.
     *
     * Technically we can simply use "idGipMaster" but doing this gives us master as CPU 0
     * in most cases making it nicer/easier for comparisons. It is safe to update the GIP
     * master as this point since the sync/async timer isn't created yet.
     */
    supdrvClearTscSamples(pDevExt, true /* fClearDeltas */);
    for (iCpu = 0; iCpu < RT_ELEMENTS(pGip->aiCpuFromApicId); iCpu++)
    {
        uint16_t idxCpu = pGip->aiCpuFromApicId[iCpu];
        if (idxCpu != UINT16_MAX)
        {
            PSUPGIPCPU pGipCpu = &pGip->aCPUs[idxCpu];
            if (RTCpuSetIsMemberByIndex(&pGip->OnlineCpuSet, pGipCpu->iCpuSet))
            {
                idxMaster = idxCpu;
                pGipCpu->i64TSCDelta = GIP_TSC_DELTA_INITIAL_MASTER_VALUE;
                break;
            }
        }
    }
    AssertReturn(idxMaster != UINT32_MAX, VERR_CPU_NOT_FOUND);
    pGipCpuMaster = &pGip->aCPUs[idxMaster];
    ASMAtomicWriteSize(&pDevExt->idGipMaster, pGipCpuMaster->idCpu);

    /*
     * If there is only a single CPU online we have nothing to do.
     */
    if (pGip->cOnlineCpus <= 1)
    {
        AssertReturn(pGip->cOnlineCpus > 0, VERR_INTERNAL_ERROR_5);
        return VINF_SUCCESS;
    }

    /*
     * Loop thru the GIP CPU array and get deltas for each CPU (except the
     * master).   We do the CPUs with the even numbered APIC IDs first so that
     * we've got alternative master CPUs to pick from on hyper-threaded systems.
     */
    for (iOddEven = 0; iOddEven < 2; iOddEven++)
    {
        for (iCpu = 0; iCpu < pGip->cCpus; iCpu++)
        {
            PSUPGIPCPU pGipCpuWorker = &pGip->aCPUs[iCpu];
            if (   iCpu != idxMaster
                && (iOddEven > 0 || (pGipCpuWorker->idApic & 1) == 0)
                && RTCpuSetIsMemberByIndex(&pDevExt->TscDeltaCpuSet, pGipCpuWorker->iCpuSet))
            {
                rc = supdrvMeasureTscDeltaOne(pDevExt, iCpu);
                if (RT_FAILURE(rc))
                {
                    SUPR0Printf("supdrvMeasureTscDeltaOne failed. rc=%d CPU[%u].idCpu=%u Master[%u].idCpu=%u\n", rc, iCpu,
                                pGipCpuWorker->idCpu, idxMaster, pDevExt->idGipMaster, pGipCpuMaster->idCpu);
                    break;
                }

                if (ASMAtomicReadU32(&pDevExt->cMpOnOffEvents) != cMpOnOffEvents)
                {
                    SUPR0Printf("One or more CPUs transitioned between online & offline states. I'm confused, retry...\n");
                    rc = VERR_TRY_AGAIN;
                    break;
                }
            }
        }
    }

    return rc;
}


#ifdef SUPDRV_USE_TSC_DELTA_THREAD

/**
 * Switches the TSC-delta measurement thread into the butchered state.
 *
 * @returns VBox status code.
 * @param pDevExt           Pointer to the device instance data.
 * @param fSpinlockHeld     Whether the TSC-delta spinlock is held or not.
 * @param pszFailed         An error message to log.
 * @param rcFailed          The error code to exit the thread with.
 */
static int supdrvTscDeltaThreadButchered(PSUPDRVDEVEXT pDevExt, bool fSpinlockHeld, const char *pszFailed, int rcFailed)
{
    if (!fSpinlockHeld)
        RTSpinlockAcquire(pDevExt->hTscDeltaSpinlock);

    pDevExt->enmTscDeltaThreadState = kTscDeltaThreadState_Butchered;
    RTSpinlockRelease(pDevExt->hTscDeltaSpinlock);
    OSDBGPRINT(("supdrvTscDeltaThreadButchered: %s. rc=%Rrc\n", rcFailed));
    return rcFailed;
}


/**
 * The TSC-delta measurement thread.
 *
 * @returns VBox status code.
 * @param hThread   The thread handle.
 * @param pvUser    Opaque pointer to the device instance data.
 */
static DECLCALLBACK(int) supdrvTscDeltaThread(RTTHREAD hThread, void *pvUser)
{
    PSUPDRVDEVEXT     pDevExt = (PSUPDRVDEVEXT)pvUser;
    bool              fInitialMeasurement = true;
    uint32_t          cConsecutiveTimeouts = 0;
    int               rc = VERR_INTERNAL_ERROR_2;
    for (;;)
    {
        /*
         * Switch on the current state.
         */
        SUPDRVTSCDELTATHREADSTATE enmState;
        RTSpinlockAcquire(pDevExt->hTscDeltaSpinlock);
        enmState = pDevExt->enmTscDeltaThreadState;
        switch (enmState)
        {
            case kTscDeltaThreadState_Creating:
            {
                pDevExt->enmTscDeltaThreadState = kTscDeltaThreadState_Listening;
                rc = RTSemEventSignal(pDevExt->hTscDeltaEvent);
                if (RT_FAILURE(rc))
                    return supdrvTscDeltaThreadButchered(pDevExt, true /* fSpinlockHeld */, "RTSemEventSignal", rc);
                /* fall thru */
            }

            case kTscDeltaThreadState_Listening:
            {
                RTSpinlockRelease(pDevExt->hTscDeltaSpinlock);

                /* Simple adaptive timeout. */
                if (cConsecutiveTimeouts++ == 10)
                {
                    if (pDevExt->cMsTscDeltaTimeout == 1)           /* 10 ms */
                        pDevExt->cMsTscDeltaTimeout = 10;
                    else if (pDevExt->cMsTscDeltaTimeout == 10)     /* +100 ms */
                        pDevExt->cMsTscDeltaTimeout = 100;
                    else if (pDevExt->cMsTscDeltaTimeout == 100)    /* +1000 ms */
                        pDevExt->cMsTscDeltaTimeout = 500;
                    cConsecutiveTimeouts = 0;
                }
                rc = RTThreadUserWait(pDevExt->hTscDeltaThread, pDevExt->cMsTscDeltaTimeout);
                if (   RT_FAILURE(rc)
                    && rc != VERR_TIMEOUT)
                    return supdrvTscDeltaThreadButchered(pDevExt, false /* fSpinlockHeld */, "RTThreadUserWait", rc);
                RTThreadUserReset(pDevExt->hTscDeltaThread);
                break;
            }

            case kTscDeltaThreadState_WaitAndMeasure:
            {
                pDevExt->enmTscDeltaThreadState = kTscDeltaThreadState_Measuring;
                rc = RTSemEventSignal(pDevExt->hTscDeltaEvent); /* (Safe on windows as long as spinlock isn't IRQ safe.) */
                if (RT_FAILURE(rc))
                    return supdrvTscDeltaThreadButchered(pDevExt, true /* fSpinlockHeld */, "RTSemEventSignal", rc);
                RTSpinlockRelease(pDevExt->hTscDeltaSpinlock);
                pDevExt->cMsTscDeltaTimeout = 1;
                RTThreadSleep(10);
                /* fall thru */
            }

            case kTscDeltaThreadState_Measuring:
            {
                cConsecutiveTimeouts = 0;
                if (fInitialMeasurement)
                {
                    int cTries = 8;
                    int cMsWaitPerTry = 10;
                    fInitialMeasurement = false;
                    do
                    {
                        rc = supdrvMeasureInitialTscDeltas(pDevExt);
                        if (   RT_SUCCESS(rc)
                            || (   RT_FAILURE(rc)
                                && rc != VERR_TRY_AGAIN
                                && rc != VERR_CPU_OFFLINE))
                        {
                            break;
                        }
                        RTThreadSleep(cMsWaitPerTry);
                    } while (cTries-- > 0);
                }
                else
                {
                    PSUPGLOBALINFOPAGE pGip = pDevExt->pGip;
                    unsigned iCpu;

                    /* Measure TSC-deltas only for the CPUs that are in the set. */
                    rc = VINF_SUCCESS;
                    for (iCpu = 0; iCpu < pGip->cCpus; iCpu++)
                    {
                        PSUPGIPCPU pGipCpuWorker = &pGip->aCPUs[iCpu];
                        if (   pGipCpuWorker->i64TSCDelta == INT64_MAX
                            && RTCpuSetIsMemberByIndex(&pDevExt->TscDeltaCpuSet, pGipCpuWorker->iCpuSet))
                        {
                            rc |= supdrvMeasureTscDeltaOne(pDevExt, iCpu);
                        }
                    }
                }
                RTSpinlockAcquire(pDevExt->hTscDeltaSpinlock);
                if (pDevExt->enmTscDeltaThreadState == kTscDeltaThreadState_Measuring)
                    pDevExt->enmTscDeltaThreadState = kTscDeltaThreadState_Listening;
                RTSpinlockRelease(pDevExt->hTscDeltaSpinlock);
                Assert(rc != VERR_NOT_AVAILABLE);   /* VERR_NOT_AVAILABLE is used as the initial value. */
                ASMAtomicWriteS32(&pDevExt->rcTscDelta, rc);
                break;
            }

            case kTscDeltaThreadState_Terminating:
                pDevExt->enmTscDeltaThreadState = kTscDeltaThreadState_Destroyed;
                RTSpinlockRelease(pDevExt->hTscDeltaSpinlock);
                return VINF_SUCCESS;

            case kTscDeltaThreadState_Butchered:
            default:
                return supdrvTscDeltaThreadButchered(pDevExt, true /* fSpinlockHeld */, "Invalid state", VERR_INVALID_STATE);
        }
    }

    return rc;
}


/**
 * Waits for the TSC-delta measurement thread to respond to a state change.
 *
 * @returns VINF_SUCCESS on success, VERR_TIMEOUT if it doesn't respond in time,
 *          other error code on internal error.
 *
 * @param   pThis           Pointer to the grant service instance data.
 * @param   enmCurState     The current state.
 * @param   enmNewState     The new state we're waiting for it to enter.
 */
static int supdrvTscDeltaThreadWait(PSUPDRVDEVEXT pDevExt, SUPDRVTSCDELTATHREADSTATE enmCurState,
                                    SUPDRVTSCDELTATHREADSTATE enmNewState)
{
    /*
     * Wait a short while for the expected state transition.
     */
    int rc;
    RTSemEventWait(pDevExt->hTscDeltaEvent, RT_MS_1SEC);
    RTSpinlockAcquire(pDevExt->hTscDeltaSpinlock);
    if (pDevExt->enmTscDeltaThreadState == enmNewState)
    {
        RTSpinlockRelease(pDevExt->hTscDeltaSpinlock);
        rc = VINF_SUCCESS;
    }
    else if (pDevExt->enmTscDeltaThreadState == enmCurState)
    {
        /*
         * Wait longer if the state has not yet transitioned to the one we want.
         */
        RTSpinlockRelease(pDevExt->hTscDeltaSpinlock);
        rc = RTSemEventWait(pDevExt->hTscDeltaEvent, 50 * RT_MS_1SEC);
        if (   RT_SUCCESS(rc)
            || rc == VERR_TIMEOUT)
        {
            /*
             * Check the state whether we've succeeded.
             */
            SUPDRVTSCDELTATHREADSTATE enmState;
            RTSpinlockAcquire(pDevExt->hTscDeltaSpinlock);
            enmState = pDevExt->enmTscDeltaThreadState;
            RTSpinlockRelease(pDevExt->hTscDeltaSpinlock);
            if (enmState == enmNewState)
                rc = VINF_SUCCESS;
            else if (enmState == enmCurState)
            {
                rc = VERR_TIMEOUT;
                OSDBGPRINT(("supdrvTscDeltaThreadWait: timed out state transition. enmState=%d enmNewState=%d\n", enmState,
                            enmNewState));
            }
            else
            {
                rc = VERR_INTERNAL_ERROR;
                OSDBGPRINT(("supdrvTscDeltaThreadWait: invalid state transition from %d to %d, expected %d\n", enmCurState,
                            enmState, enmNewState));
            }
        }
        else
            OSDBGPRINT(("supdrvTscDeltaThreadWait: RTSemEventWait failed. rc=%Rrc\n", rc));
    }
    else
    {
        RTSpinlockRelease(pDevExt->hTscDeltaSpinlock);
        OSDBGPRINT(("supdrvTscDeltaThreadWait: invalid state transition from %d to %d\n", enmCurState, enmNewState));
        rc = VERR_INTERNAL_ERROR;
    }

    return rc;
}


/**
 * Terminates the TSC-delta measurement thread.
 *
 * @param   pDevExt   Pointer to the device instance data.
 */
static void supdrvTscDeltaThreadTerminate(PSUPDRVDEVEXT pDevExt)
{
    int rc;
    RTSpinlockAcquire(pDevExt->hTscDeltaSpinlock);
    pDevExt->enmTscDeltaThreadState = kTscDeltaThreadState_Terminating;
    RTSpinlockRelease(pDevExt->hTscDeltaSpinlock);
    RTThreadUserSignal(pDevExt->hTscDeltaThread);
    rc = RTThreadWait(pDevExt->hTscDeltaThread, 50 * RT_MS_1SEC, NULL /* prc */);
    if (RT_FAILURE(rc))
    {
        /* Signal a few more times before giving up. */
        int cTriesLeft = 5;
        while (--cTriesLeft > 0)
        {
            RTThreadUserSignal(pDevExt->hTscDeltaThread);
            rc = RTThreadWait(pDevExt->hTscDeltaThread, 2 * RT_MS_1SEC, NULL /* prc */);
            if (rc != VERR_TIMEOUT)
                break;
        }
    }
}


/**
 * Initializes and spawns the TSC-delta measurement thread.
 *
 * A thread is required for servicing re-measurement requests from events like
 * CPUs coming online, suspend/resume etc. as it cannot be done synchronously
 * under all contexts on all OSs.
 *
 * @returns VBox status code.
 * @param   pDevExt           Pointer to the device instance data.
 *
 * @remarks Must only be called -after- initializing GIP and setting up MP
 *          notifications!
 */
static int supdrvTscDeltaThreadInit(PSUPDRVDEVEXT pDevExt)
{
    int rc;
    Assert(pDevExt->pGip->enmUseTscDelta > SUPGIPUSETSCDELTA_ZERO_CLAIMED);
    rc = RTSpinlockCreate(&pDevExt->hTscDeltaSpinlock, RTSPINLOCK_FLAGS_INTERRUPT_UNSAFE, "VBoxTscSpnLck");
    if (RT_SUCCESS(rc))
    {
        rc = RTSemEventCreate(&pDevExt->hTscDeltaEvent);
        if (RT_SUCCESS(rc))
        {
            pDevExt->enmTscDeltaThreadState = kTscDeltaThreadState_Creating;
            pDevExt->cMsTscDeltaTimeout = 1;
            rc = RTThreadCreate(&pDevExt->hTscDeltaThread, supdrvTscDeltaThread, pDevExt, 0 /* cbStack */,
                                RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, "VBoxTscThread");
            if (RT_SUCCESS(rc))
            {
                rc = supdrvTscDeltaThreadWait(pDevExt, kTscDeltaThreadState_Creating, kTscDeltaThreadState_Listening);
                if (RT_SUCCESS(rc))
                {
                    ASMAtomicWriteS32(&pDevExt->rcTscDelta, VERR_NOT_AVAILABLE);
                    return rc;
                }

                OSDBGPRINT(("supdrvTscDeltaInit: supdrvTscDeltaThreadWait failed. rc=%Rrc\n", rc));
                supdrvTscDeltaThreadTerminate(pDevExt);
            }
            else
                OSDBGPRINT(("supdrvTscDeltaInit: RTThreadCreate failed. rc=%Rrc\n", rc));
            RTSemEventDestroy(pDevExt->hTscDeltaEvent);
            pDevExt->hTscDeltaEvent = NIL_RTSEMEVENT;
        }
        else
            OSDBGPRINT(("supdrvTscDeltaInit: RTSemEventCreate failed. rc=%Rrc\n", rc));
        RTSpinlockDestroy(pDevExt->hTscDeltaSpinlock);
        pDevExt->hTscDeltaSpinlock = NIL_RTSPINLOCK;
    }
    else
        OSDBGPRINT(("supdrvTscDeltaInit: RTSpinlockCreate failed. rc=%Rrc\n", rc));

    return rc;
}


/**
 * Terminates the TSC-delta measurement thread and cleanup.
 *
 * @param   pDevExt         Pointer to the device instance data.
 */
static void supdrvTscDeltaTerm(PSUPDRVDEVEXT pDevExt)
{
    if (   pDevExt->hTscDeltaSpinlock != NIL_RTSPINLOCK
        && pDevExt->hTscDeltaEvent != NIL_RTSEMEVENT)
    {
        supdrvTscDeltaThreadTerminate(pDevExt);
    }

    if (pDevExt->hTscDeltaSpinlock != NIL_RTSPINLOCK)
    {
        RTSpinlockDestroy(pDevExt->hTscDeltaSpinlock);
        pDevExt->hTscDeltaSpinlock = NIL_RTSPINLOCK;
    }

    if (pDevExt->hTscDeltaEvent != NIL_RTSEMEVENT)
    {
        RTSemEventDestroy(pDevExt->hTscDeltaEvent);
        pDevExt->hTscDeltaEvent = NIL_RTSEMEVENT;
    }

    ASMAtomicWriteS32(&pDevExt->rcTscDelta, VERR_NOT_AVAILABLE);
}


/**
 * Waits for TSC-delta measurements to be completed for all online CPUs.
 *
 * @returns VBox status code.
 * @param   pDevExt         Pointer to the device instance data.
 */
static int supdrvTscDeltaThreadWaitForOnlineCpus(PSUPDRVDEVEXT pDevExt)
{
    int cTriesLeft = 5;
    int cMsTotalWait;
    int cMsWaited = 0;
    int cMsWaitGranularity = 1;

    PSUPGLOBALINFOPAGE pGip = pDevExt->pGip;
    AssertReturn(pGip, VERR_INVALID_POINTER);

    if (RT_UNLIKELY(pDevExt->hTscDeltaThread == NIL_RTTHREAD))
        return VERR_THREAD_NOT_WAITABLE;

    cMsTotalWait = RT_MIN(pGip->cPresentCpus + 10, 200);
    while (cTriesLeft-- > 0)
    {
        if (RTCpuSetIsEqual(&pDevExt->TscDeltaObtainedCpuSet, &pGip->OnlineCpuSet))
            return VINF_SUCCESS;
        RTThreadSleep(cMsWaitGranularity);
        cMsWaited += cMsWaitGranularity;
        if (cMsWaited >= cMsTotalWait)
            break;
    }

    return VERR_TIMEOUT;
}

#endif /* SUPDRV_USE_TSC_DELTA_THREAD */


/**
 * Service a TSC-delta measurement request.
 *
 * @returns VBox status code.
 * @param   pDevExt         Pointer to the device instance data.
 * @param   pSession        The support driver session.
 * @param   pReq            Pointer to the TSC-delta measurement request.
 */
int VBOXCALL supdrvIOCtl_TscDeltaMeasure(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPTSCDELTAMEASURE pReq)
{
    PSUPGLOBALINFOPAGE pGip;
    RTCPUID            idCpuWorker;
    int                rc;
    int16_t            cTries;
    RTMSINTERVAL       cMsWaitRetry;
    uint16_t           iCpu;

    /*
     * Validate.
     */
    AssertPtr(pDevExt); AssertPtr(pSession); AssertPtr(pReq); /* paranoia^2 */
    if (pSession->GipMapObjR3 == NIL_RTR0MEMOBJ)
        return VERR_WRONG_ORDER;
    pGip = pDevExt->pGip;
    AssertReturn(pGip, VERR_INTERNAL_ERROR_2);

    idCpuWorker = pReq->u.In.idCpu;
    if (idCpuWorker == NIL_RTCPUID)
        return VERR_INVALID_CPU_ID;
    cTries       = RT_MAX(pReq->u.In.cRetries + 1, 10);
    cMsWaitRetry = RT_MAX(pReq->u.In.cMsWaitRetry, 5);

    /*
     * The request is a noop if the TSC delta isn't being used.
     */
    pGip = pDevExt->pGip;
    if (pGip->enmUseTscDelta <= SUPGIPUSETSCDELTA_ZERO_CLAIMED)
        return VINF_SUCCESS;

    rc = VERR_CPU_NOT_FOUND;
    for (iCpu = 0; iCpu < pGip->cCpus; iCpu++)
    {
        PSUPGIPCPU pGipCpuWorker = &pGip->aCPUs[iCpu];
        if (pGipCpuWorker->idCpu == idCpuWorker)
        {
            if (   pGipCpuWorker->i64TSCDelta != INT64_MAX
                && !pReq->u.In.fForce)
                return VINF_SUCCESS;

#ifdef SUPDRV_USE_TSC_DELTA_THREAD
            if (pReq->u.In.fAsync)
            {
                /** @todo Async. doesn't implement options like retries, waiting. We'll need
                 *        to pass those options to the thread somehow and implement it in the
                 *        thread. Check if anyone uses/needs fAsync before implementing this. */
                RTSpinlockAcquire(pDevExt->hTscDeltaSpinlock);
                RTCpuSetAddByIndex(&pDevExt->TscDeltaCpuSet, pGipCpuWorker->iCpuSet);
                if (   pDevExt->enmTscDeltaThreadState == kTscDeltaThreadState_Listening
                    || pDevExt->enmTscDeltaThreadState == kTscDeltaThreadState_Measuring)
                {
                    pDevExt->enmTscDeltaThreadState = kTscDeltaThreadState_WaitAndMeasure;
                }
                RTSpinlockRelease(pDevExt->hTscDeltaSpinlock);
                RTThreadUserSignal(pDevExt->hTscDeltaThread);
                return VINF_SUCCESS;
            }

            /*
             * If a TSC-delta measurement request is already being serviced by the thread,
             * wait 'cTries' times if a retry-timeout is provided, otherwise bail as busy.
             */
            while (cTries-- > 0)
            {
                SUPDRVTSCDELTATHREADSTATE enmState;
                RTSpinlockAcquire(pDevExt->hTscDeltaSpinlock);
                enmState = pDevExt->enmTscDeltaThreadState;
                RTSpinlockRelease(pDevExt->hTscDeltaSpinlock);

                if (   enmState == kTscDeltaThreadState_Measuring
                    || enmState == kTscDeltaThreadState_WaitAndMeasure)
                {
                    if (   !cTries
                        || !cMsWaitRetry)
                        return VERR_SUPDRV_TSC_DELTA_MEASUREMENT_BUSY;
                    if (cMsWaitRetry)
                        RTThreadSleep(cMsWaitRetry);
                }
            }
            cTries = RT_MAX(pReq->u.In.cRetries + 1, 10);
#endif

            while (cTries-- > 0)
            {
                rc = supdrvMeasureTscDeltaOne(pDevExt, iCpu);
                if (RT_SUCCESS(rc))
                {
                    Assert(pGipCpuWorker->i64TSCDelta != INT64_MAX);
                    break;
                }

                if (cMsWaitRetry)
                    RTThreadSleep(cMsWaitRetry);
            }

            break;
        }
    }
    return rc;
}


/**
 * Reads TSC with delta applied.
 *
 * Will try to resolve delta value INT64_MAX before applying it.  This is the
 * main purpose of this function, to handle the case where the delta needs to be
 * determined.
 *
 * @returns VBox status code.
 * @param   pDevExt         Pointer to the device instance data.
 * @param   pSession        The support driver session.
 * @param   pReq            Pointer to the TSC-read request.
 */
int VBOXCALL supdrvIOCtl_TscRead(PSUPDRVDEVEXT pDevExt, PSUPDRVSESSION pSession, PSUPTSCREAD pReq)
{
    PSUPGLOBALINFOPAGE pGip;
    int rc;

    /*
     * Validate.  We require the client to have mapped GIP (no asserting on
     * ring-3 preconditions).
     */
    AssertPtr(pDevExt); AssertPtr(pReq); AssertPtr(pSession); /* paranoia^2 */
    if (pSession->GipMapObjR3 == NIL_RTR0MEMOBJ)
        return VERR_WRONG_ORDER;
    pGip = pDevExt->pGip;
    AssertReturn(pGip, VERR_INTERNAL_ERROR_2);

    /*
     * We're usually here because we need to apply delta, but we shouldn't be
     * upset if the GIP is some different mode.
     */
    if (pGip->enmUseTscDelta > SUPGIPUSETSCDELTA_ZERO_CLAIMED)
    {
        uint32_t cTries = 0;
        for (;;)
        {
            /*
             * Start by gathering the data, using CLI for disabling preemption
             * while we do that.
             */
            RTCCUINTREG uFlags  = ASMIntDisableFlags();
            int         iCpuSet = RTMpCpuIdToSetIndex(RTMpCpuId());
            int         iGipCpu;
            if (RT_LIKELY(   (unsigned)iCpuSet < RT_ELEMENTS(pGip->aiCpuFromCpuSetIdx)
                          && (iGipCpu = pGip->aiCpuFromCpuSetIdx[iCpuSet]) < pGip->cCpus ))
            {
                int64_t i64Delta   = pGip->aCPUs[iGipCpu].i64TSCDelta;
                pReq->u.Out.idApic = pGip->aCPUs[iGipCpu].idApic;
                pReq->u.Out.u64AdjustedTsc = ASMReadTSC();
                ASMSetFlags(uFlags);

                /*
                 * If we're lucky we've got a delta, but no predicitions here
                 * as this I/O control is normally only used when the TSC delta
                 * is set to INT64_MAX.
                 */
                if (i64Delta != INT64_MAX)
                {
                    pReq->u.Out.u64AdjustedTsc -= i64Delta;
                    rc = VINF_SUCCESS;
                    break;
                }

                /* Give up after a few times. */
                if (cTries >= 4)
                {
                    rc = VWRN_SUPDRV_TSC_DELTA_MEASUREMENT_FAILED;
                    break;
                }

                /* Need to measure the delta an try again. */
                rc = supdrvMeasureTscDeltaOne(pDevExt, iGipCpu);
                Assert(pGip->aCPUs[iGipCpu].i64TSCDelta != INT64_MAX || RT_FAILURE_NP(rc));
            }
            else
            {
                /* This really shouldn't happen. */
                AssertMsgFailed(("idCpu=%#x iCpuSet=%#x (%d)\n", RTMpCpuId(), iCpuSet, iCpuSet));
                pReq->u.Out.idApic = ASMGetApicId();
                pReq->u.Out.u64AdjustedTsc = ASMReadTSC();
                ASMSetFlags(uFlags);
                rc = VERR_INTERNAL_ERROR_5; /** @todo change to warning. */
                break;
            }
        }
    }
    else
    {
        /*
         * No delta to apply. Easy. Deal with preemption the lazy way.
         */
        RTCCUINTREG uFlags  = ASMIntDisableFlags();
        int         iCpuSet = RTMpCpuIdToSetIndex(RTMpCpuId());
        int         iGipCpu;
        if (RT_LIKELY(   (unsigned)iCpuSet < RT_ELEMENTS(pGip->aiCpuFromCpuSetIdx)
                      && (iGipCpu = pGip->aiCpuFromCpuSetIdx[iCpuSet]) < pGip->cCpus ))
            pReq->u.Out.idApic = pGip->aCPUs[iGipCpu].idApic;
        else
            pReq->u.Out.idApic = ASMGetApicId();
        pReq->u.Out.u64AdjustedTsc = ASMReadTSC();
        ASMSetFlags(uFlags);
        rc = VINF_SUCCESS;
    }

    return rc;
}

