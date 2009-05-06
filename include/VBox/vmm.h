/** @file
 * VMM - The Virtual Machine Monitor.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___VBox_vmm_h
#define ___VBox_vmm_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/vmapi.h>
#include <VBox/sup.h>
#include <iprt/stdarg.h>

__BEGIN_DECLS

/** @defgroup grp_vmm       The Virtual Machine Monitor API
 * @{
 */

/**
 * World switcher identifiers.
 */
typedef enum VMMSWITCHER
{
    /** The usual invalid 0. */
    VMMSWITCHER_INVALID = 0,
    /** Switcher for 32-bit host to 32-bit shadow paging. */
    VMMSWITCHER_32_TO_32,
    /** Switcher for 32-bit host paging to PAE shadow paging. */
    VMMSWITCHER_32_TO_PAE,
    /** Switcher for 32-bit host paging to AMD64 shadow paging. */
    VMMSWITCHER_32_TO_AMD64,
    /** Switcher for PAE host to 32-bit shadow paging. */
    VMMSWITCHER_PAE_TO_32,
    /** Switcher for PAE host to PAE shadow paging. */
    VMMSWITCHER_PAE_TO_PAE,
    /** Switcher for PAE host paging to AMD64 shadow paging. */
    VMMSWITCHER_PAE_TO_AMD64,
    /** Switcher for AMD64 host paging to 32-bit shadow paging. */
    VMMSWITCHER_AMD64_TO_32,
    /** Switcher for AMD64 host paging to PAE shadow paging. */
    VMMSWITCHER_AMD64_TO_PAE,
    /** Switcher for AMD64 host paging to AMD64 shadow paging. */
    VMMSWITCHER_AMD64_TO_AMD64,
    /** Used to make a count for array declarations and suchlike. */
    VMMSWITCHER_MAX,
    /** The usual 32-bit paranoia. */
    VMMSWITCHER_32BIT_HACK = 0x7fffffff
} VMMSWITCHER;


/**
 * VMMGCCallHost operations.
 */
typedef enum VMMCALLHOST
{
    /** Invalid operation.  */
    VMMCALLHOST_INVALID = 0,
    /** Acquire the PDM lock. */
    VMMCALLHOST_PDM_LOCK,
    /** Call PDMR3QueueFlushWorker. */
    VMMCALLHOST_PDM_QUEUE_FLUSH,
    /** Acquire the PGM lock. */
    VMMCALLHOST_PGM_LOCK,
    /** Grow the PGM shadow page pool. */
    VMMCALLHOST_PGM_POOL_GROW,
    /** Maps a chunk into ring-3. */
    VMMCALLHOST_PGM_MAP_CHUNK,
    /** Allocates more handy pages. */
    VMMCALLHOST_PGM_ALLOCATE_HANDY_PAGES,
    /** Replay the REM handler notifications. */
    VMMCALLHOST_REM_REPLAY_HANDLER_NOTIFICATIONS,
    /** Flush the GC/R0 logger. */
    VMMCALLHOST_VMM_LOGGER_FLUSH,
    /** Set the VM error message. */
    VMMCALLHOST_VM_SET_ERROR,
    /** Set the VM runtime error message. */
    VMMCALLHOST_VM_SET_RUNTIME_ERROR,
    /** Signal a ring 0 assertion. */
    VMMCALLHOST_VM_R0_ASSERTION,
    /** Ring switch to force preemption. */
    VMMCALLHOST_VM_R0_PREEMPT,
    /** The usual 32-bit hack. */
    VMMCALLHOST_32BIT_HACK = 0x7fffffff
} VMMCALLHOST;

VMMDECL(RTRCPTR)     VMMGetStackRC(PVM pVM);
VMMDECL(VMCPUID)     VMMGetCpuId(PVM pVM);
VMMDECL(PVMCPU)      VMMGetCpu(PVM pVM);
VMMDECL(PVMCPU)      VMMGetCpu0(PVM pVM);
VMMDECL(PVMCPU)      VMMGetCpuById(PVM pVM, RTCPUID idCpu);
VMMDECL(uint32_t)    VMMGetSvnRev(void);
VMMDECL(VMMSWITCHER) VMMGetSwitcher(PVM pVM);
VMMDECL(void)        VMMSendSipi(PVM pVM, VMCPUID idCpu, int iVector);

/** @def VMMIsHwVirtExtForced
 * Checks if forced to use the hardware assisted virtualization extensions.
 *
 * This is intended for making setup decisions where we can save resources when
 * using hardware assisted virtualization.
 *
 * @returns true / false.
 * @param   pVM     Pointer to the shared VM structure. */
#define VMMIsHwVirtExtForced(pVM)   ((pVM)->fHwVirtExtForced)


#ifdef IN_RING3
/** @defgroup grp_vmm_r3    The VMM Host Context Ring 3 API
 * @ingroup grp_vmm
 * @{
 */
VMMR3DECL(int)      VMMR3Init(PVM pVM);
VMMR3DECL(int)      VMMR3InitCPU(PVM pVM);
VMMR3DECL(int)      VMMR3InitFinalize(PVM pVM);
VMMR3DECL(int)      VMMR3InitR0(PVM pVM);
VMMR3DECL(int)      VMMR3InitRC(PVM pVM);
VMMR3DECL(int)      VMMR3Term(PVM pVM);
VMMR3DECL(int)      VMMR3TermCPU(PVM pVM);
VMMR3DECL(void)     VMMR3Relocate(PVM pVM, RTGCINTPTR offDelta);
VMMR3DECL(int)      VMMR3UpdateLoggers(PVM pVM);
VMMR3DECL(const char *) VMMR3GetRZAssertMsg1(PVM pVM);
VMMR3DECL(const char *) VMMR3GetRZAssertMsg2(PVM pVM);
VMMR3DECL(int)      VMMR3GetImportRC(PVM pVM, const char *pszSymbol, PRTRCPTR pRCPtrValue);
VMMR3DECL(int)      VMMR3SelectSwitcher(PVM pVM, VMMSWITCHER enmSwitcher);
VMMR3DECL(int)      VMMR3DisableSwitcher(PVM pVM);
VMMR3DECL(RTR0PTR)  VMMR3GetHostToGuestSwitcher(PVM pVM, VMMSWITCHER enmSwitcher);
VMMR3DECL(int)      VMMR3RawRunGC(PVM pVM, PVMCPU pVCpu);
VMMR3DECL(int)      VMMR3HwAccRunGC(PVM pVM, PVMCPU pVCpu);
VMMR3DECL(int)      VMMR3CallRC(PVM pVM, RTRCPTR RCPtrEntry, unsigned cArgs, ...);
VMMR3DECL(int)      VMMR3CallRCV(PVM pVM, RTRCPTR RCPtrEntry, unsigned cArgs, va_list args);
VMMR3DECL(int)      VMMR3CallR0(PVM pVM, uint32_t uOperation, uint64_t u64Arg, PSUPVMMR0REQHDR pReqHdr);
VMMR3DECL(int)      VMMR3ResumeHyper(PVM pVM, PVMCPU pVCpu);
VMMR3DECL(void)     VMMR3FatalDump(PVM pVM, PVMCPU pVCpu, int rcErr);
VMMR3DECL(void)     VMMR3YieldSuspend(PVM pVM);
VMMR3DECL(void)     VMMR3YieldStop(PVM pVM);
VMMR3DECL(void)     VMMR3YieldResume(PVM pVM);
/** @} */
#endif /* IN_RING3 */


/** @defgroup grp_vmm_r0    The VMM Host Context Ring 0 API
 * @ingroup grp_vmm
 * @{
 */

/**
 * The VMMR0Entry() codes.
 */
typedef enum VMMR0OPERATION
{
    /** Run guest context. */
    VMMR0_DO_RAW_RUN = SUP_VMMR0_DO_RAW_RUN,
    /** Run guest code using the available hardware acceleration technology. */
    VMMR0_DO_HWACC_RUN = SUP_VMMR0_DO_HWACC_RUN,
    /** Official NOP that we use for profiling. */
    VMMR0_DO_NOP = SUP_VMMR0_DO_NOP,
    /** Official slow iocl NOP that we use for profiling. */
    VMMR0_DO_SLOW_NOP,

    /** Ask the GVMM to create a new VM. */
    VMMR0_DO_GVMM_CREATE_VM,
    /** Ask the GVMM to destroy the VM. */
    VMMR0_DO_GVMM_DESTROY_VM,
    /** Call GVMMR0SchedHalt(). */
    VMMR0_DO_GVMM_SCHED_HALT,
    /** Call GVMMR0SchedWakeUp(). */
    VMMR0_DO_GVMM_SCHED_WAKE_UP,
    /** Call GVMMR0SchedPoke(). */
    VMMR0_DO_GVMM_SCHED_POKE,
    /** Call GVMMR0SchedWakeUpAndPokeCpus(). */
    VMMR0_DO_GVMM_SCHED_WAKE_UP_AND_POKE_CPUS,
    /** Call GVMMR0SchedPoll(). */
    VMMR0_DO_GVMM_SCHED_POLL,
    /** Call GVMMR0QueryStatistics(). */
    VMMR0_DO_GVMM_QUERY_STATISTICS,
    /** Call GVMMR0ResetStatistics(). */
    VMMR0_DO_GVMM_RESET_STATISTICS,
    /** Call GVMMR0RegisterVCpu(). */
    VMMR0_DO_GVMM_REGISTER_VMCPU,

    /** Call VMMR0 Per VM Init. */
    VMMR0_DO_VMMR0_INIT,
    /** Call VMMR0 Per VM Termination. */
    VMMR0_DO_VMMR0_TERM,
    /** Setup the hardware accelerated raw-mode session. */
    VMMR0_DO_HWACC_SETUP_VM,
    /** Attempt to enable or disable hardware accelerated raw-mode. */
    VMMR0_DO_HWACC_ENABLE,
    /** Calls function in the hypervisor.
     * The caller must setup the hypervisor context so the call will be performed.
     * The difference between VMMR0_DO_RUN_GC and this one is the handling of
     * the return GC code. The return code will not be interpreted by this operation.
     */
    VMMR0_DO_CALL_HYPERVISOR,

    /** Call PGMR0PhysAllocateHandyPages(). */
    VMMR0_DO_PGM_ALLOCATE_HANDY_PAGES,

    /** Call GMMR0InitialReservation(). */
    VMMR0_DO_GMM_INITIAL_RESERVATION,
    /** Call GMMR0UpdateReservation(). */
    VMMR0_DO_GMM_UPDATE_RESERVATION,
    /** Call GMMR0AllocatePages(). */
    VMMR0_DO_GMM_ALLOCATE_PAGES,
    /** Call GMMR0FreePages(). */
    VMMR0_DO_GMM_FREE_PAGES,
    /** Call GMMR0BalloonedPages(). */
    VMMR0_DO_GMM_BALLOONED_PAGES,
    /** Call GMMR0DeflatedBalloon(). */
    VMMR0_DO_GMM_DEFLATED_BALLOON,
    /** Call GMMR0MapUnmapChunk(). */
    VMMR0_DO_GMM_MAP_UNMAP_CHUNK,
    /** Call GMMR0SeedChunk(). */
    VMMR0_DO_GMM_SEED_CHUNK,

    /** Set a GVMM or GMM configuration value. */
    VMMR0_DO_GCFGM_SET_VALUE,
    /** Query a GVMM or GMM configuration value. */
    VMMR0_DO_GCFGM_QUERY_VALUE,

    /** The start of the R0 service operations. */
    VMMR0_DO_SRV_START,
    /** Call INTNETR0Open(). */
    VMMR0_DO_INTNET_OPEN,
    /** Call INTNETR0IfClose(). */
    VMMR0_DO_INTNET_IF_CLOSE,
    /** Call INTNETR0IfGetRing3Buffer(). */
    VMMR0_DO_INTNET_IF_GET_RING3_BUFFER,
    /** Call INTNETR0IfSetPromiscuousMode(). */
    VMMR0_DO_INTNET_IF_SET_PROMISCUOUS_MODE,
    /** Call INTNETR0IfSetMacAddress(). */
    VMMR0_DO_INTNET_IF_SET_MAC_ADDRESS,
    /** Call INTNETR0IfSetActive(). */
    VMMR0_DO_INTNET_IF_SET_ACTIVE,
    /** Call INTNETR0IfSend(). */
    VMMR0_DO_INTNET_IF_SEND,
    /** Call INTNETR0IfWait(). */
    VMMR0_DO_INTNET_IF_WAIT,
    /** The end of the R0 service operations. */
    VMMR0_DO_SRV_END,

    /** Official call we use for testing Ring-0 APIs. */
    VMMR0_DO_TESTS,
    /** Test the 32->64 bits switcher. */
    VMMR0_DO_TEST_SWITCHER3264,

    /** The usual 32-bit type blow up. */
    VMMR0_DO_32BIT_HACK = 0x7fffffff
} VMMR0OPERATION;


/**
 * Request buffer for VMMR0_DO_GCFGM_SET_VALUE and VMMR0_DO_GCFGM_QUERY_VALUE.
 * @todo Move got GCFGM.h when it's implemented.
 */
typedef struct GCFGMVALUEREQ
{
    /** The request header.*/
    SUPVMMR0REQHDR      Hdr;
    /** The support driver session handle. */
    PSUPDRVSESSION      pSession;
    /** The value.
     * This is input for the set request and output for the query. */
    uint64_t            u64Value;
    /** The variable name.
     * This is fixed sized just to make things simple for the mock-up. */
    char                szName[48];
} GCFGMVALUEREQ;
/** Pointer to a VMMR0_DO_GCFGM_SET_VALUE and VMMR0_DO_GCFGM_QUERY_VALUE request buffer.
 * @todo Move got GCFGM.h when it's implemented.
 */
typedef GCFGMVALUEREQ *PGCFGMVALUEREQ;

VMMR0DECL(int)      VMMR0EntryInt(PVM pVM, VMMR0OPERATION enmOperation, void *pvArg);
VMMR0DECL(void)     VMMR0EntryFast(PVM pVM, unsigned idCpu, VMMR0OPERATION enmOperation);
VMMR0DECL(int)      VMMR0EntryEx(PVM pVM, unsigned idCpu, VMMR0OPERATION enmOperation, PSUPVMMR0REQHDR pReq, uint64_t u64Arg, PSUPDRVSESSION);
VMMR0DECL(int)      VMMR0TermVM(PVM pVM, PGVM pGVM);
VMMR0DECL(int)      VMMR0CallHost(PVM pVM, VMMCALLHOST enmOperation, uint64_t uArg);
VMMR0DECL(void)     VMMR0LogFlushDisable(PVMCPU pVCpu);
VMMR0DECL(void)     VMMR0LogFlushEnable(PVMCPU pVCpu);

/** @} */


#ifdef IN_RC
/** @defgroup grp_vmm_rc    The VMM Raw-Mode Context API
 * @ingroup grp_vmm
 * @{
 */
VMMRCDECL(int)      VMMGCEntry(PVM pVM, unsigned uOperation, unsigned uArg, ...);
VMMRCDECL(void)     VMMGCGuestToHost(PVM pVM, int rc);
VMMRCDECL(int)      VMMGCCallHost(PVM pVM, VMMCALLHOST enmOperation, uint64_t uArg);
VMMRCDECL(bool)     VMMGCLogDisable(PVM pVM);
VMMRCDECL(void)     VMMGCLogRestore(PVM pVM, bool fLog);
VMMRCDECL(void)     VMMGCLogFlushIfFull(PVM pVM);
/** @} */
#endif /* IN_RC */


/** @} */
__END_DECLS

#endif

