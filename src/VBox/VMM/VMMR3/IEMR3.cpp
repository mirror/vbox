/* $Id$ */
/** @file
 * IEM - Interpreted Execution Manager.
 */

/*
 * Copyright (C) 2011-2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_EM
#include <VBox/vmm/iem.h>
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/dbgf.h>
#include <VBox/vmm/mm.h>
#if defined(VBOX_VMM_TARGET_ARMV8)
# include "IEMInternal-armv8.h"
#else
# include "IEMInternal.h"
#endif
#include <VBox/vmm/vm.h>
#include <VBox/vmm/vmapi.h>
#include <VBox/err.h>
#ifdef VBOX_WITH_DEBUGGER
# include <VBox/dbg.h>
#endif

#include <iprt/assert.h>
#include <iprt/getopt.h>
#include <iprt/string.h>


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static FNDBGFINFOARGVINT iemR3InfoITlb;
static FNDBGFINFOARGVINT iemR3InfoDTlb;
#ifdef VBOX_WITH_DEBUGGER
static void iemR3RegisterDebuggerCommands(void);
#endif


#if !defined(VBOX_VMM_TARGET_ARMV8)
static const char *iemGetTargetCpuName(uint32_t enmTargetCpu)
{
    switch (enmTargetCpu)
    {
#define CASE_RET_STR(enmValue) case enmValue: return #enmValue + (sizeof("IEMTARGETCPU_") - 1)
        CASE_RET_STR(IEMTARGETCPU_8086);
        CASE_RET_STR(IEMTARGETCPU_V20);
        CASE_RET_STR(IEMTARGETCPU_186);
        CASE_RET_STR(IEMTARGETCPU_286);
        CASE_RET_STR(IEMTARGETCPU_386);
        CASE_RET_STR(IEMTARGETCPU_486);
        CASE_RET_STR(IEMTARGETCPU_PENTIUM);
        CASE_RET_STR(IEMTARGETCPU_PPRO);
        CASE_RET_STR(IEMTARGETCPU_CURRENT);
#undef CASE_RET_STR
        default: return "Unknown";
    }
}
#endif


/**
 * Initializes the interpreted execution manager.
 *
 * This must be called after CPUM as we're quering information from CPUM about
 * the guest and host CPUs.
 *
 * @returns VBox status code.
 * @param   pVM                The cross context VM structure.
 */
VMMR3DECL(int)      IEMR3Init(PVM pVM)
{
    /*
     * Read configuration.
     */
#if (!defined(VBOX_VMM_TARGET_ARMV8) && !defined(VBOX_WITHOUT_CPUID_HOST_CALL)) || defined(VBOX_WITH_IEM_RECOMPILER)
    PCFGMNODE const pIem = CFGMR3GetChild(CFGMR3GetRoot(pVM), "IEM");
    int rc;
#endif

#if !defined(VBOX_VMM_TARGET_ARMV8) && !defined(VBOX_WITHOUT_CPUID_HOST_CALL)
    /** @cfgm{/IEM/CpuIdHostCall, boolean, false}
     * Controls whether the custom VBox specific CPUID host call interface is
     * enabled or not. */
# ifdef DEBUG_bird
    rc = CFGMR3QueryBoolDef(pIem, "CpuIdHostCall", &pVM->iem.s.fCpuIdHostCall, true);
# else
    rc = CFGMR3QueryBoolDef(pIem, "CpuIdHostCall", &pVM->iem.s.fCpuIdHostCall, false);
# endif
    AssertLogRelRCReturn(rc, rc);
#endif

#ifdef VBOX_WITH_IEM_RECOMPILER
    /** @cfgm{/IEM/MaxTbCount, uint32_t, 524288}
     * Max number of TBs per EMT. */
    uint32_t cMaxTbs = 0;
    rc = CFGMR3QueryU32Def(pIem, "MaxTbCount", &cMaxTbs, _512K);
    AssertLogRelRCReturn(rc, rc);
    if (cMaxTbs < _16K || cMaxTbs > _8M)
        return VMSetError(pVM, VERR_OUT_OF_RANGE, RT_SRC_POS,
                          "MaxTbCount value %u (%#x) is out of range (min %u, max %u)", cMaxTbs, cMaxTbs, _16K, _8M);

    /** @cfgm{/IEM/InitialTbCount, uint32_t, 32678}
     * Initial (minimum) number of TBs per EMT in ring-3. */
    uint32_t cInitialTbs = 0;
    rc = CFGMR3QueryU32Def(pIem, "InitialTbCount", &cInitialTbs, RT_MIN(cMaxTbs, _32K));
    AssertLogRelRCReturn(rc, rc);
    if (cInitialTbs < _16K || cInitialTbs > _8M)
        return VMSetError(pVM, VERR_OUT_OF_RANGE, RT_SRC_POS,
                          "InitialTbCount value %u (%#x) is out of range (min %u, max %u)", cInitialTbs, cInitialTbs, _16K, _8M);

    /* Check that the two values makes sense together. Expect user/api to do
       the right thing or get lost. */
    if (cInitialTbs > cMaxTbs)
        return VMSetError(pVM, VERR_OUT_OF_RANGE, RT_SRC_POS,
                          "InitialTbCount value %u (%#x) is higher than the MaxTbCount value %u (%#x)",
                          cInitialTbs, cInitialTbs, cMaxTbs, cMaxTbs);

    /** @cfgm{/IEM/MaxExecMem, uint64_t, 512 MiB}
     * Max executable memory for recompiled code per EMT. */
    uint64_t cbMaxExec = 0;
    rc = CFGMR3QueryU64Def(pIem, "MaxExecMem", &cbMaxExec, _512M);
    AssertLogRelRCReturn(rc, rc);
    if (cbMaxExec < _1M || cbMaxExec > 16*_1G64)
        return VMSetError(pVM, VERR_OUT_OF_RANGE, RT_SRC_POS,
                          "MaxExecMem value %'RU64 (%#RX64) is out of range (min %'RU64, max %'RU64)",
                          cbMaxExec, cbMaxExec, (uint64_t)_1M, 16*_1G64);

    /** @cfgm{/IEM/ExecChunkSize, uint32_t, 0 (auto)}
     * The executable memory allocator chunk size. */
    uint32_t cbChunkExec = 0;
    rc = CFGMR3QueryU32Def(pIem, "ExecChunkSize", &cbChunkExec, 0);
    AssertLogRelRCReturn(rc, rc);
    if (cbChunkExec != 0 && cbChunkExec != UINT32_MAX && (cbChunkExec < _1M || cbChunkExec > _256M))
        return VMSetError(pVM, VERR_OUT_OF_RANGE, RT_SRC_POS,
                          "ExecChunkSize value %'RU32 (%#RX32) is out of range (min %'RU32, max %'RU32)",
                          cbChunkExec, cbChunkExec, _1M, _256M);

    /** @cfgm{/IEM/InitialExecMemSize, uint64_t, 1}
     * The initial executable memory allocator size (per EMT).  The value is
     * rounded up to the nearest chunk size, so 1 byte means one chunk. */
    uint64_t cbInitialExec = 0;
    rc = CFGMR3QueryU64Def(pIem, "InitialExecMemSize", &cbInitialExec, 0);
    AssertLogRelRCReturn(rc, rc);
    if (cbInitialExec > cbMaxExec)
        return VMSetError(pVM, VERR_OUT_OF_RANGE, RT_SRC_POS,
                          "InitialExecMemSize value %'RU64 (%#RX64) is out of range (max %'RU64)",
                          cbInitialExec, cbInitialExec, cbMaxExec);

#endif /* VBOX_WITH_IEM_RECOMPILER*/

    /*
     * Initialize per-CPU data and register statistics.
     */
    uint64_t const uInitialTlbRevision = UINT64_C(0) - (IEMTLB_REVISION_INCR * 200U);
    uint64_t const uInitialTlbPhysRev  = UINT64_C(0) - (IEMTLB_PHYS_REV_INCR * 100U);

    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
    {
        PVMCPU pVCpu = pVM->apCpusR3[idCpu];
        AssertCompile(sizeof(pVCpu->iem.s) <= sizeof(pVCpu->iem.padding)); /* (tstVMStruct can't do it's job w/o instruction stats) */

        pVCpu->iem.s.CodeTlb.uTlbRevision = pVCpu->iem.s.DataTlb.uTlbRevision = uInitialTlbRevision;
        pVCpu->iem.s.CodeTlb.uTlbPhysRev  = pVCpu->iem.s.DataTlb.uTlbPhysRev  = uInitialTlbPhysRev;

        /*
         * Host and guest CPU information.
         */
        if (idCpu == 0)
        {
            pVCpu->iem.s.enmCpuVendor                     = CPUMGetGuestCpuVendor(pVM);
            pVCpu->iem.s.enmHostCpuVendor                 = CPUMGetHostCpuVendor(pVM);
#if !defined(VBOX_VMM_TARGET_ARMV8)
            pVCpu->iem.s.aidxTargetCpuEflFlavour[0]       =    pVCpu->iem.s.enmCpuVendor == CPUMCPUVENDOR_INTEL
                                                            || pVCpu->iem.s.enmCpuVendor == CPUMCPUVENDOR_VIA /*??*/
                                                          ? IEMTARGETCPU_EFL_BEHAVIOR_INTEL : IEMTARGETCPU_EFL_BEHAVIOR_AMD;
# if defined(RT_ARCH_X86) || defined(RT_ARCH_AMD64)
            if (pVCpu->iem.s.enmCpuVendor == pVCpu->iem.s.enmHostCpuVendor)
                pVCpu->iem.s.aidxTargetCpuEflFlavour[1]   = IEMTARGETCPU_EFL_BEHAVIOR_NATIVE;
            else
# endif
                pVCpu->iem.s.aidxTargetCpuEflFlavour[1]   = pVCpu->iem.s.aidxTargetCpuEflFlavour[0];
#else
            pVCpu->iem.s.aidxTargetCpuEflFlavour[0]   = IEMTARGETCPU_EFL_BEHAVIOR_NATIVE;
            pVCpu->iem.s.aidxTargetCpuEflFlavour[1]   = pVCpu->iem.s.aidxTargetCpuEflFlavour[0];
#endif

#if !defined(VBOX_VMM_TARGET_ARMV8) && (IEM_CFG_TARGET_CPU == IEMTARGETCPU_DYNAMIC)
            switch (pVM->cpum.ro.GuestFeatures.enmMicroarch)
            {
                case kCpumMicroarch_Intel_8086:     pVCpu->iem.s.uTargetCpu = IEMTARGETCPU_8086; break;
                case kCpumMicroarch_Intel_80186:    pVCpu->iem.s.uTargetCpu = IEMTARGETCPU_186; break;
                case kCpumMicroarch_Intel_80286:    pVCpu->iem.s.uTargetCpu = IEMTARGETCPU_286; break;
                case kCpumMicroarch_Intel_80386:    pVCpu->iem.s.uTargetCpu = IEMTARGETCPU_386; break;
                case kCpumMicroarch_Intel_80486:    pVCpu->iem.s.uTargetCpu = IEMTARGETCPU_486; break;
                case kCpumMicroarch_Intel_P5:       pVCpu->iem.s.uTargetCpu = IEMTARGETCPU_PENTIUM; break;
                case kCpumMicroarch_Intel_P6:       pVCpu->iem.s.uTargetCpu = IEMTARGETCPU_PPRO; break;
                case kCpumMicroarch_NEC_V20:        pVCpu->iem.s.uTargetCpu = IEMTARGETCPU_V20; break;
                case kCpumMicroarch_NEC_V30:        pVCpu->iem.s.uTargetCpu = IEMTARGETCPU_V20; break;
                default:                            pVCpu->iem.s.uTargetCpu = IEMTARGETCPU_CURRENT; break;
            }
            LogRel(("IEM: TargetCpu=%s, Microarch=%s aidxTargetCpuEflFlavour={%d,%d}\n",
                    iemGetTargetCpuName(pVCpu->iem.s.uTargetCpu), CPUMMicroarchName(pVM->cpum.ro.GuestFeatures.enmMicroarch),
                    pVCpu->iem.s.aidxTargetCpuEflFlavour[0], pVCpu->iem.s.aidxTargetCpuEflFlavour[1]));
#else
            LogRel(("IEM: Microarch=%s aidxTargetCpuEflFlavour={%d,%d}\n",
                    CPUMMicroarchName(pVM->cpum.ro.GuestFeatures.enmMicroarch),
                    pVCpu->iem.s.aidxTargetCpuEflFlavour[0], pVCpu->iem.s.aidxTargetCpuEflFlavour[1]));
#endif
        }
        else
        {
            pVCpu->iem.s.enmCpuVendor                     = pVM->apCpusR3[0]->iem.s.enmCpuVendor;
            pVCpu->iem.s.enmHostCpuVendor                 = pVM->apCpusR3[0]->iem.s.enmHostCpuVendor;
            pVCpu->iem.s.aidxTargetCpuEflFlavour[0]       = pVM->apCpusR3[0]->iem.s.aidxTargetCpuEflFlavour[0];
            pVCpu->iem.s.aidxTargetCpuEflFlavour[1]       = pVM->apCpusR3[0]->iem.s.aidxTargetCpuEflFlavour[1];
#if IEM_CFG_TARGET_CPU == IEMTARGETCPU_DYNAMIC
            pVCpu->iem.s.uTargetCpu                       = pVM->apCpusR3[0]->iem.s.uTargetCpu;
#endif
        }

        /*
         * Mark all buffers free.
         */
        uint32_t iMemMap = RT_ELEMENTS(pVCpu->iem.s.aMemMappings);
        while (iMemMap-- > 0)
            pVCpu->iem.s.aMemMappings[iMemMap].fAccess = IEM_ACCESS_INVALID;
    }


#ifdef VBOX_WITH_IEM_RECOMPILER
    /*
     * Initialize the TB allocator and cache (/ hash table).
     *
     * This is done by each EMT to try get more optimal thread/numa locality of
     * the allocations.
     */
    rc = VMR3ReqCallWait(pVM, VMCPUID_ALL, (PFNRT)iemTbInit, 6,
                         pVM, cInitialTbs, cMaxTbs, cbInitialExec, cbMaxExec, cbChunkExec);
    AssertLogRelRCReturn(rc, rc);
#endif

    /*
     * Register statistics.
     */
    for (VMCPUID idCpu = 0; idCpu < pVM->cCpus; idCpu++)
    {
#if !defined(VBOX_VMM_TARGET_ARMV8) && defined(VBOX_WITH_NESTED_HWVIRT_VMX) /* quick fix for stupid structure duplication non-sense */
        PVMCPU pVCpu = pVM->apCpusR3[idCpu];

        STAMR3RegisterF(pVM, &pVCpu->iem.s.cInstructions,               STAMTYPE_U32,       STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Instructions interpreted",                     "/IEM/CPU%u/cInstructions", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.cLongJumps,                  STAMTYPE_U32,       STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,
                        "Number of longjmp calls",                      "/IEM/CPU%u/cLongJumps", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.cPotentialExits,             STAMTYPE_U32,       STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Potential exits",                              "/IEM/CPU%u/cPotentialExits", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.cRetAspectNotImplemented,    STAMTYPE_U32_RESET, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "VERR_IEM_ASPECT_NOT_IMPLEMENTED",              "/IEM/CPU%u/cRetAspectNotImplemented", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.cRetInstrNotImplemented,     STAMTYPE_U32_RESET, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "VERR_IEM_INSTR_NOT_IMPLEMENTED",               "/IEM/CPU%u/cRetInstrNotImplemented", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.cRetInfStatuses,             STAMTYPE_U32_RESET, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Informational statuses returned",              "/IEM/CPU%u/cRetInfStatuses", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.cRetErrStatuses,             STAMTYPE_U32_RESET, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Error statuses returned",                      "/IEM/CPU%u/cRetErrStatuses", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.cbWritten,                   STAMTYPE_U32,       STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,
                        "Approx bytes written",                         "/IEM/CPU%u/cbWritten", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.cPendingCommit,              STAMTYPE_U32,       STAMVISIBILITY_ALWAYS, STAMUNIT_BYTES,
                        "Times RC/R0 had to postpone instruction committing to ring-3", "/IEM/CPU%u/cPendingCommit", idCpu);

# ifdef VBOX_WITH_STATISTICS
        STAMR3RegisterF(pVM, &pVCpu->iem.s.CodeTlb.cTlbHits,            STAMTYPE_U64_RESET, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Code TLB hits",                            "/IEM/CPU%u/CodeTlb-Hits", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.DataTlb.cTlbHits,            STAMTYPE_U64_RESET, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Data TLB hits",                            "/IEM/CPU%u/DataTlb-Hits", idCpu);
# endif
        STAMR3RegisterF(pVM, &pVCpu->iem.s.CodeTlb.cTlbMisses,          STAMTYPE_U32_RESET, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Code TLB misses",                          "/IEM/CPU%u/CodeTlb-Misses", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.CodeTlb.uTlbRevision,        STAMTYPE_X64,       STAMVISIBILITY_ALWAYS, STAMUNIT_NONE,
                        "Code TLB revision",                        "/IEM/CPU%u/CodeTlb-Revision", idCpu);
        STAMR3RegisterF(pVM, (void *)&pVCpu->iem.s.CodeTlb.uTlbPhysRev, STAMTYPE_X64,       STAMVISIBILITY_ALWAYS, STAMUNIT_NONE,
                        "Code TLB physical revision",               "/IEM/CPU%u/CodeTlb-PhysRev", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.CodeTlb.cTlbSlowReadPath,    STAMTYPE_U32_RESET, STAMVISIBILITY_ALWAYS, STAMUNIT_NONE,
                        "Code TLB slow read path",                  "/IEM/CPU%u/CodeTlb-SlowReads", idCpu);

        STAMR3RegisterF(pVM, &pVCpu->iem.s.DataTlb.cTlbMisses,          STAMTYPE_U32_RESET, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Data TLB misses",                          "/IEM/CPU%u/DataTlb-Misses", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.DataTlb.cTlbSafeReadPath,    STAMTYPE_U32_RESET, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Data TLB safe read path",                  "/IEM/CPU%u/DataTlb-SafeReads", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.DataTlb.cTlbSafeWritePath,   STAMTYPE_U32_RESET, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Data TLB safe write path",                 "/IEM/CPU%u/DataTlb-SafeWrites", idCpu);
        STAMR3RegisterF(pVM, &pVCpu->iem.s.DataTlb.uTlbRevision,        STAMTYPE_X64,       STAMVISIBILITY_ALWAYS, STAMUNIT_NONE,
                        "Data TLB revision",                        "/IEM/CPU%u/DataTlb-Revision", idCpu);
        STAMR3RegisterF(pVM, (void *)&pVCpu->iem.s.DataTlb.uTlbPhysRev, STAMTYPE_X64,       STAMVISIBILITY_ALWAYS, STAMUNIT_NONE,
                        "Data TLB physical revision",               "/IEM/CPU%u/DataTlb-PhysRev", idCpu);

#ifdef VBOX_WITH_IEM_RECOMPILER
        STAMR3RegisterF(pVM, (void *)&pVCpu->iem.s.cTbExecNative,       STAMTYPE_U64_RESET, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Executed native translation block",            "/IEM/CPU%u/re/cTbExecNative", idCpu);
        STAMR3RegisterF(pVM, (void *)&pVCpu->iem.s.cTbExecThreaded,     STAMTYPE_U64_RESET, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Executed threaded translation block",          "/IEM/CPU%u/re/cTbExecThreaded", idCpu);
        STAMR3RegisterF(pVM, (void *)&pVCpu->iem.s.StatTbExecBreaks,    STAMTYPE_COUNTER,   STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,
                        "Times TB execution was interrupted/broken off", "/IEM/CPU%u/re/cTbExecBreaks", idCpu);

        PIEMTBALLOCATOR const pTbAllocator = pVCpu->iem.s.pTbAllocatorR3;
        STAMR3RegisterF(pVM, (void *)&pTbAllocator->StatAllocs,         STAMTYPE_COUNTER,   STAMVISIBILITY_ALWAYS, STAMUNIT_CALLS,
                        "Translation block allocations",                "/IEM/CPU%u/re/cTbAllocCalls", idCpu);
        STAMR3RegisterF(pVM, (void *)&pTbAllocator->StatFrees,          STAMTYPE_COUNTER,   STAMVISIBILITY_ALWAYS, STAMUNIT_CALLS,
                        "Translation block frees",                      "/IEM/CPU%u/re/cTbFreeCalls", idCpu);
# ifdef VBOX_WITH_STATISTICS
        STAMR3RegisterF(pVM, (void *)&pTbAllocator->StatPrune,          STAMTYPE_PROFILE,   STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL,
                        "Time spent freeing up TBs when full at alloc", "/IEM/CPU%u/re/TbPruningAlloc", idCpu);
# endif
        STAMR3RegisterF(pVM, (void *)&pTbAllocator->cAllocatedChunks,   STAMTYPE_U16,   STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Populated TB chunks",                          "/IEM/CPU%u/re/cTbChunks", idCpu);
        STAMR3RegisterF(pVM, (void *)&pTbAllocator->cMaxChunks,         STAMTYPE_U8,    STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Max number of TB chunks",                      "/IEM/CPU%u/re/cTbChunksMax", idCpu);
        STAMR3RegisterF(pVM, (void *)&pTbAllocator->cTotalTbs,          STAMTYPE_U32,   STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Total number of TBs in the allocator",         "/IEM/CPU%u/re/cTbTotal", idCpu);
        STAMR3RegisterF(pVM, (void *)&pTbAllocator->cMaxTbs,            STAMTYPE_U32,   STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Max total number of TBs allowed",              "/IEM/CPU%u/re/cTbTotalMax", idCpu);
        STAMR3RegisterF(pVM, (void *)&pTbAllocator->cInUseTbs,          STAMTYPE_U32,   STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Number of currently allocated TBs",            "/IEM/CPU%u/re/cTbAllocated", idCpu);
        STAMR3RegisterF(pVM, (void *)&pTbAllocator->cNativeTbs,         STAMTYPE_U32,   STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Number of currently allocated native TBs",     "/IEM/CPU%u/re/cTbAllocatedNative", idCpu);
        STAMR3RegisterF(pVM, (void *)&pTbAllocator->cThreadedTbs,       STAMTYPE_U32,   STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Number of currently allocated threaded TBs",   "/IEM/CPU%u/re/cTbAllocatedThreaded", idCpu);

        PIEMTBCACHE     const pTbCache     = pVCpu->iem.s.pTbCacheR3;
        STAMR3RegisterF(pVM, (void *)&pTbCache->cHash,                  STAMTYPE_U32, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Translation block lookup table size",          "/IEM/CPU%u/re/cTbHashTab", idCpu);

        STAMR3RegisterF(pVM, (void *)&pTbCache->cLookupHits,            STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,
                        "Translation block lookup hits",                "/IEM/CPU%u/re/cTbLookupHits", idCpu);
        STAMR3RegisterF(pVM, (void *)&pTbCache->cLookupMisses,          STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,
                        "Translation block lookup misses",              "/IEM/CPU%u/re/cTbLookupMisses", idCpu);
        STAMR3RegisterF(pVM, (void *)&pTbCache->cCollisions,            STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES,
                        "Translation block hash table collisions",      "/IEM/CPU%u/re/cTbCollisions", idCpu);
# ifdef VBOX_WITH_STATISTICS
        STAMR3RegisterF(pVM, (void *)&pTbCache->StatPrune,              STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_TICKS_PER_CALL,
                        "Time spent shortening collision lists",        "/IEM/CPU%u/re/TbPruningCollisions", idCpu);
# endif

        STAMR3RegisterF(pVM, (void *)&pVCpu->iem.s.StatTbThreadedCalls, STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_CALLS_PER_TB,
                        "Calls per threaded translation block",         "/IEM/CPU%u/re/ThrdCallsPerTb", idCpu);
        STAMR3RegisterF(pVM, (void *)&pVCpu->iem.s.StatTbThreadedInstr, STAMTYPE_PROFILE, STAMVISIBILITY_ALWAYS, STAMUNIT_INSTR_PER_TB,
                        "Instruction per threaded translation block",   "/IEM/CPU%u/re/ThrdInstrPerTb", idCpu);

        STAMR3RegisterF(pVM, (void *)&pVCpu->iem.s.StatCheckIrqBreaks,  STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "TB breaks by CheckIrq",                        "/IEM/CPU%u/re/CheckIrqBreaks", idCpu);
        STAMR3RegisterF(pVM, (void *)&pVCpu->iem.s.StatCheckModeBreaks, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "TB breaks by CheckMode",                       "/IEM/CPU%u/re/CheckModeBreaks", idCpu);
        STAMR3RegisterF(pVM, (void *)&pVCpu->iem.s.StatCheckBranchMisses, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Branch target misses",                         "/IEM/CPU%u/re/CheckTbJmpMisses", idCpu);
        STAMR3RegisterF(pVM, (void *)&pVCpu->iem.s.StatCheckNeedCsLimChecking, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_COUNT,
                        "Needing CS.LIM checking TB after branch or on page crossing", "/IEM/CPU%u/re/CheckTbNeedCsLimChecking", idCpu);
#endif

        for (uint32_t i = 0; i < RT_ELEMENTS(pVCpu->iem.s.aStatXcpts); i++)
            STAMR3RegisterF(pVM, &pVCpu->iem.s.aStatXcpts[i], STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_OCCURENCES,
                            "", "/IEM/CPU%u/Exceptions/%02x", idCpu, i);
        for (uint32_t i = 0; i < RT_ELEMENTS(pVCpu->iem.s.aStatInts); i++)
            STAMR3RegisterF(pVM, &pVCpu->iem.s.aStatInts[i], STAMTYPE_U32_RESET, STAMVISIBILITY_USED, STAMUNIT_OCCURENCES,
                            "", "/IEM/CPU%u/Interrupts/%02x", idCpu, i);

# if !defined(VBOX_VMM_TARGET_ARMV8) && defined(VBOX_WITH_STATISTICS) && !defined(DOXYGEN_RUNNING)
        /* Instruction statistics: */
#  define IEM_DO_INSTR_STAT(a_Name, a_szDesc) \
            STAMR3RegisterF(pVM, &pVCpu->iem.s.StatsRZ.a_Name, STAMTYPE_U32_RESET, STAMVISIBILITY_USED, \
                            STAMUNIT_COUNT, a_szDesc, "/IEM/CPU%u/instr-RZ/" #a_Name, idCpu); \
            STAMR3RegisterF(pVM, &pVCpu->iem.s.StatsR3.a_Name, STAMTYPE_U32_RESET, STAMVISIBILITY_USED, \
                            STAMUNIT_COUNT, a_szDesc, "/IEM/CPU%u/instr-R3/" #a_Name, idCpu);
#  include "IEMInstructionStatisticsTmpl.h"
#  undef IEM_DO_INSTR_STAT
# endif

#endif /* !defined(VBOX_VMM_TARGET_ARMV8) && defined(VBOX_WITH_NESTED_HWVIRT_VMX) - quick fix for stupid structure duplication non-sense */
    }

#if !defined(VBOX_VMM_TARGET_ARMV8) && defined(VBOX_WITH_NESTED_HWVIRT_VMX)
    /*
     * Register the per-VM VMX APIC-access page handler type.
     */
    if (pVM->cpum.ro.GuestFeatures.fVmx)
    {
        rc = PGMR3HandlerPhysicalTypeRegister(pVM, PGMPHYSHANDLERKIND_ALL, PGMPHYSHANDLER_F_NOT_IN_HM,
                                              iemVmxApicAccessPageHandler,
                                              "VMX APIC-access page", &pVM->iem.s.hVmxApicAccessPage);
        AssertLogRelRCReturn(rc, rc);
    }
#endif

    DBGFR3InfoRegisterInternalArgv(pVM, "itlb", "IEM instruction TLB", iemR3InfoITlb, DBGFINFO_FLAGS_RUN_ON_EMT);
    DBGFR3InfoRegisterInternalArgv(pVM, "dtlb", "IEM instruction TLB", iemR3InfoDTlb, DBGFINFO_FLAGS_RUN_ON_EMT);
#ifdef VBOX_WITH_DEBUGGER
    iemR3RegisterDebuggerCommands();
#endif

    return VINF_SUCCESS;
}


VMMR3DECL(int)      IEMR3Term(PVM pVM)
{
    NOREF(pVM);
    return VINF_SUCCESS;
}


VMMR3DECL(void)     IEMR3Relocate(PVM pVM)
{
    RT_NOREF(pVM);
}


/** Worker for iemR3InfoTlbPrintSlots and iemR3InfoTlbPrintAddress. */
static void iemR3InfoTlbPrintHeader(PVMCPU pVCpu, PCDBGFINFOHLP pHlp, IEMTLB const *pTlb, bool *pfHeader)
{
    if (*pfHeader)
        return;
    pHlp->pfnPrintf(pHlp, "%cTLB for CPU %u:\n", &pVCpu->iem.s.CodeTlb == pTlb ? 'I' : 'D', pVCpu->idCpu);
    *pfHeader = true;
}


/** Worker for iemR3InfoTlbPrintSlots and iemR3InfoTlbPrintAddress. */
static void iemR3InfoTlbPrintSlot(PCDBGFINFOHLP pHlp, IEMTLB const *pTlb, IEMTLBENTRY const *pTlbe, uint32_t uSlot)
{
    pHlp->pfnPrintf(pHlp, "%02x: %s %#018RX64 -> %RGp / %p / %#05x %s%s%s%s/%s%s%s/%s %s\n",
                    uSlot,
                    (pTlbe->uTag & IEMTLB_REVISION_MASK) == pTlb->uTlbRevision ? "valid  "
                    : (pTlbe->uTag & IEMTLB_REVISION_MASK) == 0                ? "empty  "
                                                                               : "expired",
                    (pTlbe->uTag & ~IEMTLB_REVISION_MASK) << X86_PAGE_SHIFT,
                    pTlbe->GCPhys, pTlbe->pbMappingR3,
                    (uint32_t)(pTlbe->fFlagsAndPhysRev & ~IEMTLBE_F_PHYS_REV),
                    pTlbe->fFlagsAndPhysRev & IEMTLBE_F_PT_NO_EXEC      ? "NX" : " X",
                    pTlbe->fFlagsAndPhysRev & IEMTLBE_F_PT_NO_WRITE     ? "RO" : "RW",
                    pTlbe->fFlagsAndPhysRev & IEMTLBE_F_PT_NO_ACCESSED  ? "-"  : "A",
                    pTlbe->fFlagsAndPhysRev & IEMTLBE_F_PT_NO_DIRTY     ? "-"  : "D",
                    pTlbe->fFlagsAndPhysRev & IEMTLBE_F_PG_NO_WRITE     ? "-"  : "w",
                    pTlbe->fFlagsAndPhysRev & IEMTLBE_F_PG_NO_READ      ? "-"  : "r",
                    pTlbe->fFlagsAndPhysRev & IEMTLBE_F_PG_UNASSIGNED   ? "U"  : "-",
                    pTlbe->fFlagsAndPhysRev & IEMTLBE_F_NO_MAPPINGR3    ? "S"  : "M",
                    (pTlbe->fFlagsAndPhysRev & IEMTLBE_F_PHYS_REV) == pTlb->uTlbPhysRev ? "phys-valid"
                    : (pTlbe->fFlagsAndPhysRev & IEMTLBE_F_PHYS_REV) == 0 ? "phys-empty" : "phys-expired");
}


/** Displays one or more TLB slots. */
static void iemR3InfoTlbPrintSlots(PVMCPU pVCpu, PCDBGFINFOHLP pHlp, IEMTLB const *pTlb,
                                   uint32_t uSlot, uint32_t cSlots, bool *pfHeader)
{
    if (uSlot < RT_ELEMENTS(pTlb->aEntries))
    {
        if (cSlots > RT_ELEMENTS(pTlb->aEntries))
        {
            pHlp->pfnPrintf(pHlp, "error: Too many slots given: %u, adjusting it down to the max (%u)\n",
                            cSlots, RT_ELEMENTS(pTlb->aEntries));
            cSlots = RT_ELEMENTS(pTlb->aEntries);
        }

        iemR3InfoTlbPrintHeader(pVCpu, pHlp, pTlb, pfHeader);
        while (cSlots-- > 0)
        {
            IEMTLBENTRY const Tlbe = pTlb->aEntries[uSlot];
            iemR3InfoTlbPrintSlot(pHlp, pTlb, &Tlbe, uSlot);
            uSlot = (uSlot + 1) % RT_ELEMENTS(pTlb->aEntries);
        }
    }
    else
        pHlp->pfnPrintf(pHlp, "error: TLB slot is out of range: %u (%#x), max %u (%#x)\n",
                        uSlot, uSlot, RT_ELEMENTS(pTlb->aEntries) - 1, RT_ELEMENTS(pTlb->aEntries) - 1);
}


/** Displays the TLB slot for the given address. */
static void iemR3InfoTlbPrintAddress(PVMCPU pVCpu, PCDBGFINFOHLP pHlp, IEMTLB const *pTlb,
                                     uint64_t uAddress, bool *pfHeader)
{
    iemR3InfoTlbPrintHeader(pVCpu, pHlp, pTlb, pfHeader);

    uint64_t const    uTag  = (uAddress << 16) >> (X86_PAGE_SHIFT + 16);
    uint32_t const    uSlot = (uint8_t)uTag;
    IEMTLBENTRY const Tlbe  = pTlb->aEntries[uSlot];
    pHlp->pfnPrintf(pHlp, "Address %#RX64 -> slot %#x - %s\n", uAddress, uSlot,
                    Tlbe.uTag == (uTag | pTlb->uTlbRevision)  ? "match"
                    : (Tlbe.uTag & ~IEMTLB_REVISION_MASK) == uTag ? "expired" : "mismatch");
    iemR3InfoTlbPrintSlot(pHlp, pTlb, &Tlbe, uSlot);
}


/** Common worker for iemR3InfoDTlb and iemR3InfoITlb. */
static void iemR3InfoTlbCommon(PVM pVM, PCDBGFINFOHLP pHlp, int cArgs, char **papszArgs, bool fITlb)
{
    /*
     * This is entirely argument driven.
     */
    static RTGETOPTDEF const s_aOptions[] =
    {
        { "--cpu",     'c', RTGETOPT_REQ_UINT32                          },
        { "--vcpu",    'c', RTGETOPT_REQ_UINT32                          },
        { "all",       'A', RTGETOPT_REQ_NOTHING                         },
        { "--all",     'A', RTGETOPT_REQ_NOTHING                         },
        { "--address", 'a', RTGETOPT_REQ_UINT64      | RTGETOPT_FLAG_HEX },
        { "--range",   'r', RTGETOPT_REQ_UINT32_PAIR | RTGETOPT_FLAG_HEX },
        { "--slot",    's', RTGETOPT_REQ_UINT32      | RTGETOPT_FLAG_HEX },
    };

    char  szDefault[] = "-A";
    char *papszDefaults[2] = { szDefault, NULL };
    if (cArgs == 0)
    {
        cArgs     = 1;
        papszArgs = papszDefaults;
    }

    RTGETOPTSTATE State;
    int rc = RTGetOptInit(&State, cArgs, papszArgs, s_aOptions, RT_ELEMENTS(s_aOptions), 0 /*iFirst*/, 0 /*fFlags*/);
    AssertRCReturnVoid(rc);

    bool            fNeedHeader  = true;
    bool            fAddressMode = true;
    PVMCPU          pVCpu        = VMMGetCpu(pVM);
    if (!pVCpu)
        pVCpu = VMMGetCpuById(pVM, 0);

    RTGETOPTUNION   ValueUnion;
    while ((rc = RTGetOpt(&State, &ValueUnion)) != 0)
    {
        switch (rc)
        {
            case 'c':
                if (ValueUnion.u32 >= pVM->cCpus)
                    pHlp->pfnPrintf(pHlp, "error: Invalid CPU ID: %u\n", ValueUnion.u32);
                else if (!pVCpu || pVCpu->idCpu != ValueUnion.u32)
                {
                    pVCpu = VMMGetCpuById(pVM, ValueUnion.u32);
                    fNeedHeader = true;
                }
                break;

            case 'a':
                iemR3InfoTlbPrintAddress(pVCpu, pHlp, fITlb ? &pVCpu->iem.s.CodeTlb : &pVCpu->iem.s.DataTlb,
                                         ValueUnion.u64, &fNeedHeader);
                fAddressMode = true;
                break;

            case 'A':
                iemR3InfoTlbPrintSlots(pVCpu, pHlp, fITlb ? &pVCpu->iem.s.CodeTlb : &pVCpu->iem.s.DataTlb,
                                       0, RT_ELEMENTS(pVCpu->iem.s.CodeTlb.aEntries), &fNeedHeader);
                break;

            case 'r':
                iemR3InfoTlbPrintSlots(pVCpu, pHlp, fITlb ? &pVCpu->iem.s.CodeTlb : &pVCpu->iem.s.DataTlb,
                                       ValueUnion.PairU32.uFirst, ValueUnion.PairU32.uSecond, &fNeedHeader);
                fAddressMode = false;
                break;

            case 's':
                iemR3InfoTlbPrintSlots(pVCpu, pHlp, fITlb ? &pVCpu->iem.s.CodeTlb : &pVCpu->iem.s.DataTlb,
                                       ValueUnion.u32, 1, &fNeedHeader);
                fAddressMode = false;
                break;

            case VINF_GETOPT_NOT_OPTION:
                if (fAddressMode)
                {
                    uint64_t uAddr;
                    rc = RTStrToUInt64Full(ValueUnion.psz, 16, &uAddr);
                    if (RT_SUCCESS(rc) && rc != VWRN_NUMBER_TOO_BIG)
                        iemR3InfoTlbPrintAddress(pVCpu, pHlp, fITlb ? &pVCpu->iem.s.CodeTlb : &pVCpu->iem.s.DataTlb,
                                                 uAddr, &fNeedHeader);
                    else
                        pHlp->pfnPrintf(pHlp, "error: Invalid or malformed guest address '%s': %Rrc\n", ValueUnion.psz, rc);
                }
                else
                {
                    uint32_t uSlot;
                    rc = RTStrToUInt32Full(ValueUnion.psz, 16, &uSlot);
                    if (RT_SUCCESS(rc) && rc != VWRN_NUMBER_TOO_BIG)
                        iemR3InfoTlbPrintSlots(pVCpu, pHlp, fITlb ? &pVCpu->iem.s.CodeTlb : &pVCpu->iem.s.DataTlb,
                                               uSlot, 1, &fNeedHeader);
                    else
                        pHlp->pfnPrintf(pHlp, "error: Invalid or malformed TLB slot number '%s': %Rrc\n", ValueUnion.psz, rc);
                }
                break;

            case 'h':
                pHlp->pfnPrintf(pHlp,
                                "Usage: info %ctlb [options]\n"
                                "\n"
                                "Options:\n"
                                "  -c<n>, --cpu=<n>, --vcpu=<n>\n"
                                "    Selects the CPU which TLBs we're looking at. Default: Caller / 0\n"
                                "  -A, --all, all\n"
                                "    Display all the TLB entries (default if no other args).\n"
                                "  -a<virt>, --address=<virt>\n"
                                "    Shows the TLB entry for the specified guest virtual address.\n"
                                "  -r<slot:count>, --range=<slot:count>\n"
                                "    Shows the TLB entries for the specified slot range.\n"
                                "  -s<slot>,--slot=<slot>\n"
                                "    Shows the given TLB slot.\n"
                                "\n"
                                "Non-options are interpreted according to the last -a, -r or -s option,\n"
                                "defaulting to addresses if not preceeded by any of those options.\n"
                                , fITlb ? 'i' : 'd');
                return;

            default:
                pHlp->pfnGetOptError(pHlp, rc, &ValueUnion, &State);
                return;
        }
    }
}


/**
 * @callback_method_impl{FNDBGFINFOARGVINT, itlb}
 */
static DECLCALLBACK(void) iemR3InfoITlb(PVM pVM, PCDBGFINFOHLP pHlp, int cArgs, char **papszArgs)
{
    return iemR3InfoTlbCommon(pVM, pHlp, cArgs, papszArgs, true /*fITlb*/);
}


/**
 * @callback_method_impl{FNDBGFINFOARGVINT, dtlb}
 */
static DECLCALLBACK(void) iemR3InfoDTlb(PVM pVM, PCDBGFINFOHLP pHlp, int cArgs, char **papszArgs)
{
    return iemR3InfoTlbCommon(pVM, pHlp, cArgs, papszArgs, false /*fITlb*/);
}


#ifdef VBOX_WITH_DEBUGGER

/** @callback_method_impl{FNDBGCCMD,
 * Implements the '.alliem' command. }
 */
static DECLCALLBACK(int) iemR3DbgFlushTlbs(PCDBGCCMD pCmd, PDBGCCMDHLP pCmdHlp, PUVM pUVM, PCDBGCVAR paArgs, unsigned cArgs)
{
    VMCPUID idCpu = DBGCCmdHlpGetCurrentCpu(pCmdHlp);
    PVMCPU pVCpu = VMMR3GetCpuByIdU(pUVM, idCpu);
    if (pVCpu)
    {
        VMR3ReqPriorityCallVoidWaitU(pUVM, idCpu, (PFNRT)IEMTlbInvalidateAll, 1, pVCpu);
        return VINF_SUCCESS;
    }
    RT_NOREF(paArgs, cArgs);
    return DBGCCmdHlpFail(pCmdHlp, pCmd, "failed to get the PVMCPU for the current CPU");
}


/**
 * Called by IEMR3Init to register debugger commands.
 */
static void iemR3RegisterDebuggerCommands(void)
{
    /*
     * Register debugger commands.
     */
    static DBGCCMD const s_aCmds[] =
    {
        {
            /* .pszCmd = */         "iemflushtlb",
            /* .cArgsMin = */       0,
            /* .cArgsMax = */       0,
            /* .paArgDescs = */     NULL,
            /* .cArgDescs = */      0,
            /* .fFlags = */         0,
            /* .pfnHandler = */     iemR3DbgFlushTlbs,
            /* .pszSyntax = */      "",
            /* .pszDescription = */ "Flushed the code and data TLBs"
        },
    };

    int rc = DBGCRegisterCommands(&s_aCmds[0], RT_ELEMENTS(s_aCmds));
    AssertLogRelRC(rc);
}

#endif

