/* $Id$ */
/** @file
 * IPRT - Initialization & Termination, R0 Driver, NT.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
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
#include <iprt/asm-amd64-x86.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/mp.h>
#include <iprt/string.h>
#include "internal/initterm.h"
#include "internal-r0drv-nt.h"
#include "../mp-r0drv.h"
#include "symdb.h"
#include "symdbdata.h"


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** The NT CPU set.
 * KeQueryActiveProcssors() cannot be called at all IRQLs and therefore we'll
 * have to cache it. Fortunately, Nt doesn't really support taking CPUs offline
 * or online. It's first with W2K8 that support for CPU hotplugging was added.
 * Once we start caring about this, we'll simply let the native MP event callback
 * and update this variable as CPUs comes online. (The code is done already.)
 */
RTCPUSET                                g_rtMpNtCpuSet;
/** Maximum number of processor groups. */
uint32_t                                g_cRtMpNtMaxGroups;
/** Maximum number of processors. */
uint32_t                                g_cRtMpNtMaxCpus;
/** The handle of the rtR0NtMpProcessorChangeCallback registration. */
static PVOID                            g_pvMpCpuChangeCallback = NULL;

/** ExSetTimerResolution, introduced in W2K. */
PFNMYEXSETTIMERRESOLUTION               g_pfnrtNtExSetTimerResolution;
/** KeFlushQueuedDpcs, introduced in XP. */
PFNMYKEFLUSHQUEUEDDPCS                  g_pfnrtNtKeFlushQueuedDpcs;
/** HalRequestIpi, version introduced with windows 7. */
PFNHALREQUESTIPI_W7PLUS                 g_pfnrtHalRequestIpiW7Plus;
/** HalRequestIpi, version valid up to windows vista?? */
PFNHALREQUESTIPI_PRE_W7                 g_pfnrtHalRequestIpiPreW7;
/** Worker for RTMpPokeCpu. */
PFNRTSENDIPI                            g_pfnrtMpPokeCpuWorker;
/** KeIpiGenericCall - Introduced in Windows Server 2003. */
PFNRTKEIPIGENERICCALL                   g_pfnrtKeIpiGenericCall;
/** KeSetTargetProcessorDpcEx - Introduced in Windows 7. */
PFNKESETTARGETPROCESSORDPCEX            g_pfnrtKeSetTargetProcessorDpcEx;
/** KeInitializeAffinityEx - Introducted in Windows 7. */
PFNKEINITIALIZEAFFINITYEX               g_pfnrtKeInitializeAffinityEx;
/** KeAddProcessorAffinityEx - Introducted in Windows 7. */
PFNKEADDPROCESSORAFFINITYEX             g_pfnrtKeAddProcessorAffinityEx;
/** KeGetProcessorIndexFromNumber - Introducted in Windows 7. */
PFNKEGETPROCESSORINDEXFROMNUMBER        g_pfnrtKeGetProcessorIndexFromNumber;
/** KeGetProcessorNumberFromIndex - Introducted in Windows 7. */
PFNKEGETPROCESSORNUMBERFROMINDEX        g_pfnrtKeGetProcessorNumberFromIndex;
/** KeGetCurrentProcessorNumberEx - Introducted in Windows 7. */
PFNKEGETCURRENTPROCESSORNUMBEREX        g_pfnrtKeGetCurrentProcessorNumberEx;
/** KeQueryActiveProcessors - Introducted in Windows 2000. */
PFNKEQUERYACTIVEPROCESSORS              g_pfnrtKeQueryActiveProcessors;
/** KeQueryMaximumProcessorCount   - Introducted in Vista and obsoleted W7. */
PFNKEQUERYMAXIMUMPROCESSORCOUNT         g_pfnrtKeQueryMaximumProcessorCount;
/** KeQueryMaximumProcessorCountEx - Introducted in Windows 7. */
PFNKEQUERYMAXIMUMPROCESSORCOUNTEX       g_pfnrtKeQueryMaximumProcessorCountEx;
/** KeQueryMaximumGroupCount - Introducted in Windows 7. */
PFNKEQUERYMAXIMUMGROUPCOUNT             g_pfnrtKeQueryMaximumGroupCount;
/** KeQueryLogicalProcessorRelationship - Introducted in Windows 7. */
PFNKEQUERYLOGICALPROCESSORRELATIONSHIP  g_pfnrtKeQueryLogicalProcessorRelationship;
/** KeRegisterProcessorChangeCallback - Introducted in Windows 7. */
PFNKEREGISTERPROCESSORCHANGECALLBACK    g_pfnrtKeRegisterProcessorChangeCallback;
/** KeDeregisterProcessorChangeCallback - Introducted in Windows 7. */
PFNKEDEREGISTERPROCESSORCHANGECALLBACK  g_pfnrtKeDeregisterProcessorChangeCallback;
/** RtlGetVersion, introduced in ??. */
PFNRTRTLGETVERSION                      g_pfnrtRtlGetVersion;
#ifndef RT_ARCH_AMD64
/** KeQueryInterruptTime - exported/new in Windows 2000. */
PFNRTKEQUERYINTERRUPTTIME               g_pfnrtKeQueryInterruptTime;
/** KeQuerySystemTime - exported/new in Windows 2000. */
PFNRTKEQUERYSYSTEMTIME                  g_pfnrtKeQuerySystemTime;
#endif
/** KeQueryInterruptTimePrecise - new in Windows 8. */
PFNRTKEQUERYINTERRUPTTIMEPRECISE        g_pfnrtKeQueryInterruptTimePrecise;
/** KeQuerySystemTimePrecise - new in Windows 8. */
PFNRTKEQUERYSYSTEMTIMEPRECISE           g_pfnrtKeQuerySystemTimePrecise;

/** Offset of the _KPRCB::QuantumEnd field. 0 if not found. */
uint32_t                                g_offrtNtPbQuantumEnd;
/** Size of the _KPRCB::QuantumEnd field. 0 if not found. */
uint32_t                                g_cbrtNtPbQuantumEnd;
/** Offset of the _KPRCB::DpcQueueDepth field. 0 if not found. */
uint32_t                                g_offrtNtPbDpcQueueDepth;


/**
 * Determines the NT kernel verison information.
 *
 * @param   pOsVerInfo          Where to return the version information.
 *
 * @remarks pOsVerInfo->fSmp is only definitive if @c true.
 * @remarks pOsVerInfo->uCsdNo is set to MY_NIL_CSD if it cannot be determined.
 */
static void rtR0NtGetOsVersionInfo(PRTNTSDBOSVER pOsVerInfo)
{
    ULONG       ulMajorVersion = 0;
    ULONG       ulMinorVersion = 0;
    ULONG       ulBuildNumber  = 0;

    pOsVerInfo->fChecked    = PsGetVersion(&ulMajorVersion, &ulMinorVersion, &ulBuildNumber, NULL) == TRUE;
    pOsVerInfo->uMajorVer   = (uint8_t)ulMajorVersion;
    pOsVerInfo->uMinorVer   = (uint8_t)ulMinorVersion;
    pOsVerInfo->uBuildNo    = ulBuildNumber;
#define MY_NIL_CSD      0x3f
    pOsVerInfo->uCsdNo      = MY_NIL_CSD;

    if (g_pfnrtRtlGetVersion)
    {
        RTL_OSVERSIONINFOEXW VerInfo;
        RT_ZERO(VerInfo);
        VerInfo.dwOSVersionInfoSize = sizeof(VerInfo);

        NTSTATUS rcNt = g_pfnrtRtlGetVersion(&VerInfo);
        if (NT_SUCCESS(rcNt))
            pOsVerInfo->uCsdNo = VerInfo.wServicePackMajor;
    }

    /* Note! We cannot quite say if something is MP or UNI. So, fSmp is
             redefined to indicate that it must be MP.
       Note! RTMpGetCount is not available here. */
    pOsVerInfo->fSmp = ulMajorVersion >= 6; /* Vista and later has no UNI kernel AFAIK. */
    if (!pOsVerInfo->fSmp)
    {
        if (   g_pfnrtKeQueryMaximumProcessorCountEx
            && g_pfnrtKeQueryMaximumProcessorCountEx(ALL_PROCESSOR_GROUPS) > 1)
            pOsVerInfo->fSmp = true;
        else if (   g_pfnrtKeQueryMaximumProcessorCount
                 && g_pfnrtKeQueryMaximumProcessorCount() > 1)
            pOsVerInfo->fSmp = true;
        else if (   g_pfnrtKeQueryActiveProcessors
                 && g_pfnrtKeQueryActiveProcessors() > 1)
            pOsVerInfo->fSmp = true;
        else if (KeNumberProcessors > 1)
            pOsVerInfo->fSmp = true;
    }
}


/**
 * Tries a set against the current kernel.
 *
 * @retval  true if it matched up, global variables are updated.
 * @retval  false otherwise (no globals updated).
 * @param   pSet                The data set.
 * @param   pbPrcb              Pointer to the processor control block.
 * @param   pszVendor           Pointer to the processor vendor string.
 * @param   pOsVerInfo          The OS version info.
 */
static bool rtR0NtTryMatchSymSet(PCRTNTSDBSET pSet, uint8_t *pbPrcb, const char *pszVendor, PCRTNTSDBOSVER pOsVerInfo)
{
    /*
     * Don't bother trying stuff where the NT kernel version number differs, or
     * if the build type or SMPness doesn't match up.
     */
    if (   pSet->OsVerInfo.uMajorVer != pOsVerInfo->uMajorVer
        || pSet->OsVerInfo.uMinorVer != pOsVerInfo->uMinorVer
        || pSet->OsVerInfo.fChecked  != pOsVerInfo->fChecked
        || (!pSet->OsVerInfo.fSmp && pOsVerInfo->fSmp /*must-be-smp*/) )
    {
        //DbgPrint("IPRT: #%d Version/type mismatch.\n", pSet - &g_artNtSdbSets[0]);
        return false;
    }

    /*
     * Do the CPU vendor test.
     *
     * Note! The MmIsAddressValid call is the real #PF security here as the
     *       __try/__except has limited/no ability to catch everything we need.
     */
    char *pszPrcbVendorString = (char *)&pbPrcb[pSet->KPRCB.offVendorString];
    if (!MmIsAddressValid(&pszPrcbVendorString[4 * 3 - 1]))
    {
        //DbgPrint("IPRT: #%d invalid vendor string address.\n", pSet - &g_artNtSdbSets[0]);
        return false;
    }
    __try
    {
        if (memcmp(pszPrcbVendorString, pszVendor, RT_MIN(4 * 3, pSet->KPRCB.cbVendorString)) != 0)
        {
            //DbgPrint("IPRT: #%d Vendor string mismatch.\n", pSet - &g_artNtSdbSets[0]);
            return false;
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        DbgPrint("IPRT: %#d Exception\n", pSet - &g_artNtSdbSets[0]);
        return false;
    }

    /*
     * Got a match, update the global variables and report succcess.
     */
    g_offrtNtPbQuantumEnd    = pSet->KPRCB.offQuantumEnd;
    g_cbrtNtPbQuantumEnd     = pSet->KPRCB.cbQuantumEnd;
    g_offrtNtPbDpcQueueDepth = pSet->KPRCB.offDpcQueueDepth;

#if 0
    DbgPrint("IPRT: Using data set #%u for %u.%usp%u build %u %s %s.\n",
             pSet - &g_artNtSdbSets[0],
             pSet->OsVerInfo.uMajorVer,
             pSet->OsVerInfo.uMinorVer,
             pSet->OsVerInfo.uCsdNo,
             pSet->OsVerInfo.uBuildNo,
             pSet->OsVerInfo.fSmp ? "smp" : "uni",
             pSet->OsVerInfo.fChecked ? "checked" : "free");
#endif
    return true;
}


/**
 * Implements the NT PROCESSOR_CALLBACK_FUNCTION callback function.
 *
 * This maintains the g_rtMpNtCpuSet and works MP notification callbacks.  When
 * registered, it's called for each active CPU in the system, avoiding racing
 * CPU hotplugging (as well as testing the callback).
 *
 * @param   pvUser              User context (not used).
 * @param   pChangeCtx          Change context (in).
 * @param   prcOperationStatus  Operation status (in/out).
 */
static VOID __stdcall rtR0NtMpProcessorChangeCallback(void *pvUser, PKE_PROCESSOR_CHANGE_NOTIFY_CONTEXT pChangeCtx,
                                                      PNTSTATUS prcOperationStatus)
{
    RT_NOREF(pvUser, prcOperationStatus);
    switch (pChangeCtx->State)
    {
        case KeProcessorAddCompleteNotify:
            if (pChangeCtx->NtNumber < RTCPUSET_MAX_CPUS)
            {
                RTCpuSetAddByIndex(&g_rtMpNtCpuSet, pChangeCtx->NtNumber);
                rtMpNotificationDoCallbacks(RTMPEVENT_ONLINE, pChangeCtx->NtNumber);
            }
            else
            {
                DbgPrint("rtR0NtMpProcessorChangeCallback: NtNumber=%u (%#x) is higher than RTCPUSET_MAX_CPUS (%d)\n",
                         pChangeCtx->NtNumber, pChangeCtx->NtNumber, RTCPUSET_MAX_CPUS);
                AssertMsgFailed(("NtNumber=%u (%#x)\n", pChangeCtx->NtNumber, pChangeCtx->NtNumber));
            }
            break;

        case KeProcessorAddStartNotify:
        case KeProcessorAddFailureNotify:
            /* ignore */
            break;

        default:
            AssertMsgFailed(("State=%u\n", pChangeCtx->State));
    }
}


/**
 * Wrapper around KeQueryLogicalProcessorRelationship.
 *
 * @returns IPRT status code.
 * @param   ppInfo  Where to return the info. Pass to RTMemFree when done.
 */
static int rtR0NtInitQueryGroupRelations(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX **ppInfo)
{
    ULONG    cbInfo = sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)
                    + g_cRtMpNtMaxGroups * sizeof(GROUP_RELATIONSHIP);
    NTSTATUS rcNt;
    do
    {
        SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *pInfo = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *)RTMemAlloc(cbInfo);
        if (pInfo)
        {
            rcNt = g_pfnrtKeQueryLogicalProcessorRelationship(NULL /*pProcNumber*/, RelationGroup, pInfo, &cbInfo);
            if (NT_SUCCESS(rcNt))
            {
                *ppInfo = pInfo;
                return VINF_SUCCESS;
            }

            RTMemFree(pInfo);
            pInfo = NULL;
        }
        else
            rcNt = STATUS_NO_MEMORY;
    } while (rcNt == STATUS_INFO_LENGTH_MISMATCH);
    DbgPrint("IPRT: Fatal: KeQueryLogicalProcessorRelationship failed: %#x\n", rcNt);
    AssertMsgFailed(("KeQueryLogicalProcessorRelationship failed: %#x\n", rcNt));
    return RTErrConvertFromNtStatus(rcNt);
}


/**
 * Initalizes multiprocessor globals.
 *
 * @returns IPRT status code.
 */
static int rtR0NtInitMp(RTNTSDBOSVER const *pOsVerInfo)
{
#define MY_CHECK_BREAK(a_Check, a_DbgPrintArgs) \
        AssertMsgBreakStmt(a_Check, a_DbgPrintArgs, DbgPrint a_DbgPrintArgs; rc = VERR_INTERNAL_ERROR_4 )
#define MY_CHECK_RETURN(a_Check, a_DbgPrintArgs, a_rcRet) \
        AssertMsgReturnStmt(a_Check, a_DbgPrintArgs, DbgPrint a_DbgPrintArgs, a_rcRet)
#define MY_CHECK(a_Check, a_DbgPrintArgs) \
        AssertMsgStmt(a_Check, a_DbgPrintArgs, DbgPrint a_DbgPrintArgs; rc = VERR_INTERNAL_ERROR_4 )

    /*
     * API combination checks.
     */
    MY_CHECK_RETURN(!g_pfnrtKeSetTargetProcessorDpcEx || g_pfnrtKeGetProcessorNumberFromIndex,
                    ("IPRT: Fatal: Missing KeSetTargetProcessorDpcEx without KeGetProcessorNumberFromIndex!\n"),
                    VERR_SYMBOL_NOT_FOUND);

    /*
     * Get max number of processor groups.
     */
    if (g_pfnrtKeQueryMaximumGroupCount)
    {
        g_cRtMpNtMaxGroups = g_pfnrtKeQueryMaximumGroupCount();
        MY_CHECK_RETURN(g_cRtMpNtMaxGroups <= RTCPUSET_MAX_CPUS && g_cRtMpNtMaxGroups > 0,
                        ("IPRT: Fatal: g_cRtMpNtMaxGroups=%u, max %u\n", g_cRtMpNtMaxGroups, RTCPUSET_MAX_CPUS),
                        VERR_MP_TOO_MANY_CPUS);
    }
    else
        g_cRtMpNtMaxGroups = 1;

    /*
     * Get max number CPUs.
     * This also defines the range of NT CPU indexes, RTCPUID and index into RTCPUSET.
     */
    if (g_pfnrtKeQueryMaximumProcessorCountEx)
    {
        g_cRtMpNtMaxCpus = g_pfnrtKeQueryMaximumProcessorCountEx(ALL_PROCESSOR_GROUPS);
        MY_CHECK_RETURN(g_cRtMpNtMaxCpus <= RTCPUSET_MAX_CPUS && g_cRtMpNtMaxCpus > 0,
                        ("IPRT: Fatal: g_cRtMpNtMaxGroups=%u, max %u [KeQueryMaximumProcessorCountEx]\n",
                         g_cRtMpNtMaxGroups, RTCPUSET_MAX_CPUS),
                        VERR_MP_TOO_MANY_CPUS);
    }
    else if (g_pfnrtKeQueryMaximumProcessorCount)
    {
        g_cRtMpNtMaxCpus = g_pfnrtKeQueryMaximumProcessorCount();
        MY_CHECK_RETURN(g_cRtMpNtMaxCpus <= RTCPUSET_MAX_CPUS && g_cRtMpNtMaxCpus > 0,
                        ("IPRT: Fatal: g_cRtMpNtMaxGroups=%u, max %u [KeQueryMaximumProcessorCount]\n",
                         g_cRtMpNtMaxGroups, RTCPUSET_MAX_CPUS),
                        VERR_MP_TOO_MANY_CPUS);
    }
    else if (g_pfnrtKeQueryActiveProcessors)
    {
        KAFFINITY fActiveProcessors = g_pfnrtKeQueryActiveProcessors();
        MY_CHECK_RETURN(fActiveProcessors != 0,
                        ("IPRT: Fatal: KeQueryActiveProcessors returned 0!\n"),
                        VERR_INTERNAL_ERROR_2);
        g_cRtMpNtMaxCpus = 0;
        do
        {
            g_cRtMpNtMaxCpus++;
            fActiveProcessors >>= 1;
        } while (fActiveProcessors);
    }
    else
        g_cRtMpNtMaxCpus = KeNumberProcessors;

    /*
     * Query the details for the groups to figure out which CPUs are online as
     * well as the NT index limit.
     */
    if (g_pfnrtKeQueryLogicalProcessorRelationship)
    {
        MY_CHECK_RETURN(g_pfnrtKeGetProcessorIndexFromNumber,
                        ("IPRT: Fatal: Found KeQueryLogicalProcessorRelationship but not KeGetProcessorIndexFromNumber!\n"),
                        VERR_SYMBOL_NOT_FOUND);
        MY_CHECK_RETURN(g_pfnrtKeGetProcessorNumberFromIndex,
                        ("IPRT: Fatal: Found KeQueryLogicalProcessorRelationship but not KeGetProcessorIndexFromNumber!\n"),
                        VERR_SYMBOL_NOT_FOUND);
        MY_CHECK_RETURN(g_pfnrtKeSetTargetProcessorDpcEx,
                        ("IPRT: Fatal: Found KeQueryLogicalProcessorRelationship but not KeSetTargetProcessorDpcEx!\n"),
                        VERR_SYMBOL_NOT_FOUND);

        SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *pInfo = NULL;
        int rc = rtR0NtInitQueryGroupRelations(&pInfo);
        if (RT_FAILURE(rc))
            return rc;

        AssertReturnStmt(pInfo->Group.MaximumGroupCount == g_cRtMpNtMaxGroups, RTMemFree(pInfo), VERR_INTERNAL_ERROR_3);

        /*
         * Calc online mask.
         *
         * Also check ASSUMPTIONS:
         *      - Processor indexes going to KeQueryMaximumProcessorCountEx(ALL_PROCESSOR_GROUPS)
         *      - Processor indexes being assigned to absent hotswappable CPUs, i.e.
         *        KeGetProcessorIndexFromNumber and KeGetProcessorNumberFromIndex works
         *        all possible indexes. [Not yet confirmed!]
         *      - Processor indexes are assigned in group order.
         *      - MaximumProcessorCount specifies the highest bit in the active mask.
         *        This is for confirming process IDs assigned by IPRT in ring-3.
         */
        /** @todo Test the latter on a real/virtual system. */
        RTCpuSetEmpty(&g_rtMpNtCpuSet);
        uint32_t idxCpuExpect = 0;
        for (uint32_t idxGroup = 0; RT_SUCCESS(rc) && idxGroup < pInfo->Group.ActiveGroupCount; idxGroup++)
        {
            const PROCESSOR_GROUP_INFO *pGrpInfo = &pInfo->Group.GroupInfo[idxGroup];
            MY_CHECK_BREAK(pGrpInfo->MaximumProcessorCount <= MAXIMUM_PROC_PER_GROUP,
                           ("IPRT: Fatal: MaximumProcessorCount=%u\n", pGrpInfo->MaximumProcessorCount));
            MY_CHECK_BREAK(pGrpInfo->ActiveProcessorCount <= MAXIMUM_PROC_PER_GROUP,
                           ("IPRT: Fatal: ActiveProcessorCount=%u\n", pGrpInfo->ActiveProcessorCount));
            MY_CHECK_BREAK(pGrpInfo->ActiveProcessorCount <= pGrpInfo->MaximumProcessorCount,
                           ("IPRT: Fatal: ActiveProcessorCount=%u > MaximumProcessorCount=%u\n",
                            pGrpInfo->ActiveProcessorCount, pGrpInfo->MaximumProcessorCount));
            for (uint32_t idxMember = 0; idxMember < pGrpInfo->MaximumProcessorCount; idxMember++, idxCpuExpect++)
            {
                PROCESSOR_NUMBER ProcNum;
                ProcNum.Group    = (USHORT)idxGroup;
                ProcNum.Number   = (UCHAR)idxMember;
                ProcNum.Reserved = 0;
                ULONG idxCpu = g_pfnrtKeGetProcessorIndexFromNumber(&ProcNum);
                if (idxCpu != INVALID_PROCESSOR_INDEX)
                {
                    MY_CHECK_BREAK(idxCpu < g_cRtMpNtMaxCpus && idxCpu < RTCPUSET_MAX_CPUS,
                                   ("IPRT: Fatal: idxCpu=%u >= g_cRtMpNtMaxCpu=%u (RTCPUSET_MAX_CPUS=%u)\n",
                                    idxCpu, g_cRtMpNtMaxCpus, RTCPUSET_MAX_CPUS));
                    MY_CHECK_BREAK(idxCpu == idxCpuExpect, ("IPRT: Fatal: idxCpu=%u != idxCpuExpect=%u\n", idxCpu, idxCpuExpect));

                    ProcNum.Group    = UINT16_MAX;
                    ProcNum.Number   = UINT8_MAX;
                    ProcNum.Reserved = UINT8_MAX;
                    NTSTATUS rcNt = g_pfnrtKeGetProcessorNumberFromIndex(idxCpu, &ProcNum);
                    MY_CHECK_BREAK(NT_SUCCESS(rcNt), ("IPRT: Fatal: KeGetProcessorNumberFromIndex(%u,) -> %#x!\n", idxCpu, rcNt));
                    MY_CHECK_BREAK(ProcNum.Group == idxGroup && ProcNum.Number == idxMember,
                                   ("IPRT: Fatal: KeGetProcessorXxxxFromYyyy roundtrip error for %#x! Group: %u vs %u, Number: %u vs %u\n",
                                    idxCpu, ProcNum.Group, idxGroup, ProcNum.Number, idxMember));

                    if (pGrpInfo->ActiveProcessorMask & RT_BIT_64(idxMember))
                        RTCpuSetAddByIndex(&g_rtMpNtCpuSet, idxCpu);
                }
                else
                {
                    /* W2K8 server gives me a max of 64 logical CPUs, even if the system only has 12,
                       causing failures here.  Not yet sure how this would work with two CPU groups yet... */
                    MY_CHECK_BREAK(   idxMember >= pGrpInfo->ActiveProcessorCount
                                   && !(pGrpInfo->ActiveProcessorMask & RT_BIT_64(idxMember)),
                                   ("IPRT: Fatal: KeGetProcessorIndexFromNumber(%u/%u) failed! cMax=%u cActive=%u\n",
                                    idxGroup, idxMember, pGrpInfo->MaximumProcessorCount, pGrpInfo->ActiveProcessorCount));
                }
            }
        }
        RTMemFree(pInfo);
        if (RT_FAILURE(rc)) /* MY_CHECK_BREAK sets rc. */
            return rc;
    }
    else
    {
        /* Legacy: */
        MY_CHECK_RETURN(g_cRtMpNtMaxGroups == 1, ("IPRT: Fatal: Missing KeQueryLogicalProcessorRelationship!\n"),
                        VERR_SYMBOL_NOT_FOUND);

        if (g_pfnrtKeQueryActiveProcessors)
            RTCpuSetFromU64(&g_rtMpNtCpuSet, g_pfnrtKeQueryActiveProcessors());
        else if (g_cRtMpNtMaxCpus < 64)
            RTCpuSetFromU64(&g_rtMpNtCpuSet, (UINT64_C(1) << g_cRtMpNtMaxCpus) - 1);
        else
        {
            MY_CHECK_RETURN(g_cRtMpNtMaxCpus == 64, ("IPRT: Fatal: g_cRtMpNtMaxCpus=%u, expect 64 or less\n", g_cRtMpNtMaxCpus),
                            VERR_MP_TOO_MANY_CPUS);
            RTCpuSetFromU64(&g_rtMpNtCpuSet, UINT64_MAX);
        }
    }

    /*
     * Register CPU hot plugging callback.
     */
    Assert(g_pvMpCpuChangeCallback == NULL);
    if (g_pfnrtKeRegisterProcessorChangeCallback)
    {
        MY_CHECK_RETURN(g_pfnrtKeDeregisterProcessorChangeCallback,
                        ("IPRT: Fatal: KeRegisterProcessorChangeCallback without KeDeregisterProcessorChangeCallback!\n"),
                        VERR_SYMBOL_NOT_FOUND);

        RTCPUSET ActiveSetCopy = g_rtMpNtCpuSet;
        RTCpuSetEmpty(&g_rtMpNtCpuSet);
        g_pvMpCpuChangeCallback = g_pfnrtKeRegisterProcessorChangeCallback(rtR0NtMpProcessorChangeCallback, NULL /*pvUser*/,
                                                                           KE_PROCESSOR_CHANGE_ADD_EXISTING);
        if (!g_pvMpCpuChangeCallback)  
        {
            AssertFailed();
            g_rtMpNtCpuSet = ActiveSetCopy;
        }
    }

    /*
     * Special IPI fun for RTMpPokeCpu.
     *
     * On Vista and later the DPC method doesn't seem to reliably send IPIs,
     * so we have to use alternative methods.
     *
     * On AMD64 We used to use the HalSendSoftwareInterrupt API (also x86 on
     * W10+), it looks faster and more convenient to use, however we're either
     * using it wrong or it doesn't reliably do what we want (see @bugref{8343}).
     *
     * The HalRequestIpip API is thus far the only alternative to KeInsertQueueDpc
     * for doing targetted IPIs.  Trouble with this API is that it changed
     * fundamentally in Window 7 when they added support for lots of processors.
     *
     * If we really think we cannot use KeInsertQueueDpc, we use the broadcast IPI
     * API KeIpiGenericCall.
     */
    if (   pOsVerInfo->uMajorVer > 6
        || (pOsVerInfo->uMajorVer == 6 && pOsVerInfo->uMinorVer > 0))
        g_pfnrtHalRequestIpiPreW7 = NULL;
    else
        g_pfnrtHalRequestIpiW7Plus = NULL;

    g_pfnrtMpPokeCpuWorker = rtMpPokeCpuUsingDpc;
#ifndef IPRT_TARGET_NT4
    if (   g_pfnrtHalRequestIpiW7Plus
        && g_pfnrtKeInitializeAffinityEx
        && g_pfnrtKeAddProcessorAffinityEx
        && g_pfnrtKeGetProcessorIndexFromNumber)
    {
        DbgPrint("IPRT: RTMpPoke => rtMpPokeCpuUsingHalReqestIpiW7Plus\n");
        g_pfnrtMpPokeCpuWorker = rtMpPokeCpuUsingHalReqestIpiW7Plus;
    }
    else if (pOsVerInfo->uMajorVer >= 6 && g_pfnrtKeIpiGenericCall)
    {
        DbgPrint("IPRT: RTMpPoke => rtMpPokeCpuUsingBroadcastIpi\n");
        g_pfnrtMpPokeCpuWorker = rtMpPokeCpuUsingBroadcastIpi;
    }
    else
        DbgPrint("IPRT: RTMpPoke => rtMpPokeCpuUsingDpc\n");
    /* else: Windows XP should send always send an IPI -> VERIFY */
#endif

    return VINF_SUCCESS;
}


DECLHIDDEN(int) rtR0InitNative(void)
{
    /*
     * Initialize the function pointers.
     */
#ifdef IPRT_TARGET_NT4
# define GET_SYSTEM_ROUTINE_EX(a_Prf, a_Name, a_pfnType) do { RT_CONCAT3(g_pfnrt, a_Prf, a_Name) = NULL; } while (0)
#else
    UNICODE_STRING RoutineName;
# define GET_SYSTEM_ROUTINE_EX(a_Prf, a_Name, a_pfnType) \
    do { \
        RtlInitUnicodeString(&RoutineName, L#a_Name); \
        RT_CONCAT3(g_pfnrt, a_Prf, a_Name) = (a_pfnType)MmGetSystemRoutineAddress(&RoutineName); \
    } while (0)
#endif
#define GET_SYSTEM_ROUTINE(a_Name)                 GET_SYSTEM_ROUTINE_EX(RT_NOTHING, a_Name, decltype(a_Name) *)
#define GET_SYSTEM_ROUTINE_PRF(a_Prf,a_Name)       GET_SYSTEM_ROUTINE_EX(a_Prf, a_Name, decltype(a_Name) *)
#define GET_SYSTEM_ROUTINE_TYPE(a_Name, a_pfnType) GET_SYSTEM_ROUTINE_EX(RT_NOTHING, a_Name, a_pfnType)

    GET_SYSTEM_ROUTINE_PRF(Nt,ExSetTimerResolution);
    GET_SYSTEM_ROUTINE_PRF(Nt,KeFlushQueuedDpcs);
    GET_SYSTEM_ROUTINE(KeIpiGenericCall);
    GET_SYSTEM_ROUTINE(KeSetTargetProcessorDpcEx);
    GET_SYSTEM_ROUTINE(KeInitializeAffinityEx);
    GET_SYSTEM_ROUTINE(KeAddProcessorAffinityEx);
    GET_SYSTEM_ROUTINE_TYPE(KeGetProcessorIndexFromNumber, PFNKEGETPROCESSORINDEXFROMNUMBER);
    GET_SYSTEM_ROUTINE(KeGetProcessorNumberFromIndex);
    GET_SYSTEM_ROUTINE_TYPE(KeGetCurrentProcessorNumberEx, PFNKEGETCURRENTPROCESSORNUMBEREX);
    GET_SYSTEM_ROUTINE(KeQueryActiveProcessors);
    GET_SYSTEM_ROUTINE(KeQueryMaximumProcessorCount);
    GET_SYSTEM_ROUTINE(KeQueryMaximumProcessorCountEx);
    GET_SYSTEM_ROUTINE(KeQueryMaximumGroupCount);
    GET_SYSTEM_ROUTINE(KeQueryLogicalProcessorRelationship);
    GET_SYSTEM_ROUTINE(KeRegisterProcessorChangeCallback);
    GET_SYSTEM_ROUTINE(KeDeregisterProcessorChangeCallback);

    GET_SYSTEM_ROUTINE_TYPE(RtlGetVersion, PFNRTRTLGETVERSION);
#ifndef RT_ARCH_AMD64
    GET_SYSTEM_ROUTINE(KeQueryInterruptTime);
    GET_SYSTEM_ROUTINE(KeQuerySystemTime);
#endif
    GET_SYSTEM_ROUTINE_TYPE(KeQueryInterruptTimePrecise, PFNRTKEQUERYINTERRUPTTIMEPRECISE);
    GET_SYSTEM_ROUTINE_TYPE(KeQuerySystemTimePrecise, PFNRTKEQUERYSYSTEMTIMEPRECISE);

#ifdef IPRT_TARGET_NT4
    g_pfnrtHalRequestIpiW7Plus = NULL;
    g_pfnrtHalRequestIpiPreW7 = NULL;
#else
    RtlInitUnicodeString(&RoutineName, L"HalRequestIpi");
    g_pfnrtHalRequestIpiW7Plus = (PFNHALREQUESTIPI_W7PLUS)MmGetSystemRoutineAddress(&RoutineName);
    g_pfnrtHalRequestIpiPreW7 = (PFNHALREQUESTIPI_PRE_W7)g_pfnrtHalRequestIpiW7Plus;
#endif

    /*
     * HACK ALERT! (and déjà vu warning - remember win32k.sys?)
     *
     * Try find _KPRCB::QuantumEnd and _KPRCB::[DpcData.]DpcQueueDepth.
     * For purpose of verification we use the VendorString member (12+1 chars).
     *
     * The offsets was initially derived by poking around with windbg
     * (dt _KPRCB, !prcb ++, and such like). Systematic harvesting was then
     * planned using dia2dump, grep and the symbol pack in a manner like this:
     *      dia2dump -type _KDPC_DATA -type _KPRCB EXE\ntkrnlmp.pdb | grep -wE "QuantumEnd|DpcData|DpcQueueDepth|VendorString"
     *
     * The final solution ended up using a custom harvester program called
     * ntBldSymDb that recursively searches thru unpacked symbol packages for
     * the desired structure offsets.  The program assumes that the packages
     * are unpacked into directories with the same name as the package, with
     * exception of some of the w2k packages which requires a 'w2k' prefix to
     * be distinguishable from another.
     */

    RTNTSDBOSVER OsVerInfo;
    rtR0NtGetOsVersionInfo(&OsVerInfo);

    /*
     * Gather consistent CPU vendor string and PRCB pointers.
     */
    KIRQL OldIrql;
    KeRaiseIrql(DISPATCH_LEVEL, &OldIrql); /* make sure we stay on the same cpu */

    union
    {
        uint32_t auRegs[4];
        char szVendor[4*3+1];
    } u;
    ASMCpuId(0, &u.auRegs[3], &u.auRegs[0], &u.auRegs[2], &u.auRegs[1]);
    u.szVendor[4*3] = '\0';

    uint8_t *pbPrcb;
    __try /* Warning. This try/except statement may provide some false safety. */
    {
#if defined(RT_ARCH_X86)
        PKPCR    pPcr   = (PKPCR)__readfsdword(RT_OFFSETOF(KPCR,SelfPcr));
        pbPrcb = (uint8_t *)pPcr->Prcb;
#elif defined(RT_ARCH_AMD64)
        PKPCR    pPcr   = (PKPCR)__readgsqword(RT_OFFSETOF(KPCR,Self));
        pbPrcb = (uint8_t *)pPcr->CurrentPrcb;
#else
# error "port me"
        pbPrcb = NULL;
#endif
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        pbPrcb = NULL;
    }

    /*
     * Search the database
     */
    if (pbPrcb)
    {
        /* Find the best matching kernel version based on build number. */
        uint32_t iBest      = UINT32_MAX;
        int32_t  iBestDelta = INT32_MAX;
        for (uint32_t i = 0; i < RT_ELEMENTS(g_artNtSdbSets); i++)
        {
            if (g_artNtSdbSets[i].OsVerInfo.fChecked != OsVerInfo.fChecked)
                continue;
            if (OsVerInfo.fSmp /*must-be-smp*/ && !g_artNtSdbSets[i].OsVerInfo.fSmp)
                continue;

            int32_t iDelta = RT_ABS((int32_t)OsVerInfo.uBuildNo - (int32_t)g_artNtSdbSets[i].OsVerInfo.uBuildNo);
            if (   iDelta == 0
                && (g_artNtSdbSets[i].OsVerInfo.uCsdNo == OsVerInfo.uCsdNo || OsVerInfo.uCsdNo == MY_NIL_CSD))
            {
                /* prefect */
                iBestDelta = iDelta;
                iBest      = i;
                break;
            }
            if (   iDelta < iBestDelta
                || iBest == UINT32_MAX
                || (   iDelta == iBestDelta
                    && OsVerInfo.uCsdNo != MY_NIL_CSD
                    &&   RT_ABS(g_artNtSdbSets[i    ].OsVerInfo.uCsdNo - (int32_t)OsVerInfo.uCsdNo)
                       < RT_ABS(g_artNtSdbSets[iBest].OsVerInfo.uCsdNo - (int32_t)OsVerInfo.uCsdNo)
                   )
                )
            {
                iBestDelta = iDelta;
                iBest      = i;
            }
        }
        if (iBest < RT_ELEMENTS(g_artNtSdbSets))
        {
            /* Try all sets: iBest -> End; iBest -> Start. */
            bool    fDone = false;
            int32_t i     = iBest;
            while (   i < RT_ELEMENTS(g_artNtSdbSets)
                   && !(fDone = rtR0NtTryMatchSymSet(&g_artNtSdbSets[i], pbPrcb, u.szVendor, &OsVerInfo)))
                i++;
            if (!fDone)
            {
                i = (int32_t)iBest - 1;
                while (   i >= 0
                       && !(fDone = rtR0NtTryMatchSymSet(&g_artNtSdbSets[i], pbPrcb, u.szVendor, &OsVerInfo)))
                    i--;
            }
        }
        else
            DbgPrint("IPRT: Failed to locate data set.\n");
    }
    else
        DbgPrint("IPRT: Failed to get PCBR pointer.\n");

    KeLowerIrql(OldIrql); /* Lowering the IRQL early in the hope that we may catch exceptions below. */

#ifndef IN_GUEST
    if (!g_offrtNtPbQuantumEnd && !g_offrtNtPbDpcQueueDepth)
        DbgPrint("IPRT: Neither _KPRCB::QuantumEnd nor _KPRCB::DpcQueueDepth was not found! Kernel %u.%u %u %s\n",
                 OsVerInfo.uMajorVer, OsVerInfo.uMinorVer, OsVerInfo.uBuildNo, OsVerInfo.fChecked ? "checked" : "free");
# ifdef DEBUG
    else
        DbgPrint("IPRT: _KPRCB:{.QuantumEnd=%x/%d, .DpcQueueDepth=%x/%d} Kernel %u.%u %u %s\n",
                 g_offrtNtPbQuantumEnd, g_cbrtNtPbQuantumEnd, g_offrtNtPbDpcQueueDepth,
                 OsVerInfo.uMajorVer, OsVerInfo.uMinorVer, OsVerInfo.uBuildNo, OsVerInfo.fChecked ? "checked" : "free");
# endif
#endif

    /*
     * Initialize multi processor stuff.  This registers a callback, so
     * we call rtR0TermNative to do the deregistration on failure.
     */
    int rc = rtR0NtInitMp(&OsVerInfo);
    if (RT_FAILURE(rc))
    {
        rtR0TermNative();
        DbgPrint("IPRT: Fatal: rtR0NtInitMp failed: %d\n", rc);
        return rc;
    }

    return VINF_SUCCESS;
}


DECLHIDDEN(void) rtR0TermNative(void)
{
    /*
     * Deregister the processor change callback.
     */
    PVOID pvMpCpuChangeCallback = g_pvMpCpuChangeCallback;
    g_pvMpCpuChangeCallback = NULL;
    if (pvMpCpuChangeCallback)
    {
        AssertReturnVoid(g_pfnrtKeDeregisterProcessorChangeCallback);
        g_pfnrtKeDeregisterProcessorChangeCallback(pvMpCpuChangeCallback);
    }
}

