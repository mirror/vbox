/** @file
 * VMM - The Virtual Machine Monitor.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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
#ifndef VBOX_WITH_NEW_PHYS_CODE
    /** Dynamically allocate physical guest RAM. */
    VMMCALLHOST_PGM_RAM_GROW_RANGE,
#endif
    /** Replay the REM handler notifications. */
    VMMCALLHOST_REM_REPLAY_HANDLER_NOTIFICATIONS,
    /** Flush the GC/R0 logger. */
    VMMCALLHOST_VMM_LOGGER_FLUSH,
    /** Set the VM error message. */
    VMMCALLHOST_VM_SET_ERROR,
    /** Set the VM runtime error message. */
    VMMCALLHOST_VM_SET_RUNTIME_ERROR,
    /** Signal a ring 0 hypervisor assertion. */
    VMMCALLHOST_VM_R0_HYPER_ASSERTION,
    /** The usual 32-bit hack. */
    VMMCALLHOST_32BIT_HACK = 0x7fffffff
} VMMCALLHOST;



/**
 * Gets the bottom of the hypervisor stack - GC Ptr.
 * I.e. the returned address is not actually writable.
 *
 * @returns bottom of the stack.
 * @param   pVM         The VM handle.
 */
RTGCPTR VMMGetStackGC(PVM pVM);

/**
 * Gets the bottom of the hypervisor stack - HC Ptr.
 * I.e. the returned address is not actually writable.
 *
 * @returns bottom of the stack.
 * @param   pVM         The VM handle.
 */
RTHCPTR VMMGetHCStack(PVM pVM);



#ifdef IN_RING3
/** @defgroup grp_vmm_r3    The VMM Host Context Ring 3 API
 * @ingroup grp_vmm
 * @{
 */

/**
 * Initializes the VMM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(int) VMMR3Init(PVM pVM);

/**
 * Ring-3 init finalizing.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 */
VMMR3DECL(int) VMMR3InitFinalize(PVM pVM);

/**
 * Initializes the R0 VMM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(int) VMMR3InitR0(PVM pVM);

/**
 * Initializes the GC VMM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(int) VMMR3InitGC(PVM pVM);

/**
 * Destroy the VMM bits.
 *
 * @returns VINF_SUCCESS.
 * @param   pVM         The VM handle.
 */
VMMR3DECL(int) VMMR3Term(PVM pVM);

/**
 * Applies relocations to data and code managed by this
 * component. This function will be called at init and
 * whenever the VMM need to relocate it self inside the GC.
 *
 * The VMM will need to apply relocations to the core code.
 *
 * @param   pVM         The VM handle.
 * @param   offDelta    The relocation delta.
 */
VMMR3DECL(void) VMMR3Relocate(PVM pVM, RTGCINTPTR offDelta);

/**
 * Updates the settings for the GC (and R0?) loggers.
 *
 * @returns VBox status code.
 * @param   pVM     The VM handle.
 */
VMMR3DECL(int)  VMMR3UpdateLoggers(PVM pVM);

/**
 * Gets the pointer to g_szRTAssertMsg1 in GC.
 * @returns Pointer to VMMGC::g_szRTAssertMsg1.
 *          Returns NULL if not present.
 * @param   pVM         The VM handle.
 */
VMMR3DECL(const char *) VMMR3GetGCAssertMsg1(PVM pVM);

/**
 * Gets the pointer to g_szRTAssertMsg2 in GC.
 * @returns Pointer to VMMGC::g_szRTAssertMsg2.
 *          Returns NULL if not present.
 * @param   pVM         The VM handle.
 */
VMMR3DECL(const char *) VMMR3GetGCAssertMsg2(PVM pVM);

/**
 * Resolve a builtin GC symbol.
 * Called by PDM when loading or relocating GC modules.
 *
 * @returns VBox status.
 * @param   pVM         VM Handle.
 * @param   pszSymbol   Symbol to resolv
 * @param   pGCPtrValue Where to store the symbol value.
 * @remark  This has to work before VMMR3Relocate() is called.
 */
VMMR3DECL(int) VMMR3GetImportGC(PVM pVM, const char *pszSymbol, PRTGCPTR pGCPtrValue);

/**
 * Selects the switcher to be used for switching to GC.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   enmSwitcher     The new switcher.
 * @remark  This function may be called before the VMM is initialized.
 */
VMMR3DECL(int) VMMR3SelectSwitcher(PVM pVM, VMMSWITCHER enmSwitcher);

/**
 * Disable the switcher logic permanently.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 */
VMMR3DECL(int) VMMR3DisableSwitcher(PVM pVM);

/**
 * Executes guest code.
 *
 * @param   pVM         VM handle.
 */
VMMR3DECL(int) VMMR3RawRunGC(PVM pVM);

/**
 * Executes guest code (Intel VMX and AMD SVM).
 *
 * @param   pVM         VM handle.
 */
VMMR3DECL(int) VMMR3HwAccRunGC(PVM pVM);

/**
 * Calls GC a function.
 *
 * @param   pVM         The VM handle.
 * @param   GCPtrEntry  The GC function address.
 * @param   cArgs       The number of arguments in the ....
 * @param   ...         Arguments to the function.
 */
VMMR3DECL(int) VMMR3CallGC(PVM pVM, RTGCPTR GCPtrEntry, unsigned cArgs, ...);

/**
 * Calls GC a function.
 *
 * @param   pVM         The VM handle.
 * @param   GCPtrEntry  The GC function address.
 * @param   cArgs       The number of arguments in the ....
 * @param   args        Arguments to the function.
 */
VMMR3DECL(int) VMMR3CallGCV(PVM pVM, RTGCPTR GCPtrEntry, unsigned cArgs, va_list args);

/**
 * Resumes executing hypervisor code when interrupted
 * by a queue flush or a debug event.
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 */
VMMR3DECL(int) VMMR3ResumeHyper(PVM pVM);

/**
 * Dumps the VM state on a fatal error.
 *
 * @param   pVM         VM Handle.
 * @param   rcErr       VBox status code.
 */
VMMR3DECL(void) VMMR3FatalDump(PVM pVM, int rcErr);

/**
 * Acquire global VM lock
 *
 * @returns VBox status code
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(int) VMMR3Lock(PVM pVM);

/**
 * Release global VM lock
 *
 * @returns VBox status code
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(int) VMMR3Unlock(PVM pVM);

/**
 * Return global VM lock owner
 *
 * @returns NIL_RTNATIVETHREAD -> no owner, otherwise thread id of owner
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(RTNATIVETHREAD) VMMR3LockGetOwner(PVM pVM);

/**
 * Checks if the current thread is the owner of the global VM lock.
 *
 * @returns true if owner.
 * @returns false if not owner.
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(bool) VMMR3LockIsOwner(PVM pVM);

/**
 * Suspends the the CPU yielder.
 *
 * @param   pVM             The VM handle.
 */
VMMR3DECL(void) VMMR3YieldSuspend(PVM pVM);

/**
 * Stops the the CPU yielder.
 *
 * @param   pVM             The VM handle.
 */
VMMR3DECL(void) VMMR3YieldStop(PVM pVM);

/**
 * Resumes the CPU yielder when it has been a suspended or stopped.
 *
 * @param   pVM             The VM handle.
 */
VMMR3DECL(void) VMMR3YieldResume(PVM pVM);

/** @} */
#endif

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

    /** Ask the GVMM to create a new VM. */
    VMMR0_DO_GVMM_CREATE_VM,
    /** Ask the GVMM to destroy the VM. */
    VMMR0_DO_GVMM_DESTROY_VM,
    /** Call GVMMR0SchedHalt(). */
    VMMR0_DO_GVMM_SCHED_HALT,
    /** Call GVMMR0SchedWakeUp(). */
    VMMR0_DO_GVMM_SCHED_WAKE_UP,
    /** Call GVMMR0SchedPoll(). */
    VMMR0_DO_GVMM_SCHED_POLL,
    /** Call GVMMR0QueryStatistics(). */
    VMMR0_DO_GVMM_QUERY_STATISTICS,
    /** Call GVMMR0ResetStatistics(). */
    VMMR0_DO_GVMM_RESET_STATISTICS,

    /** Call VMMR0 Per VM Init. */
    VMMR0_DO_VMMR0_INIT,
    /** Call VMMR0 Per VM Termination. */
    VMMR0_DO_VMMR0_TERM,
    /** Setup the hardware accelerated raw-mode session. */
    VMMR0_DO_HWACC_SETUP_VM,
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
    /** Call INTNETR0IfSend(). */
    VMMR0_DO_INTNET_IF_SEND,
    /** Call INTNETR0IfWait(). */
    VMMR0_DO_INTNET_IF_WAIT,
    /** The end of the R0 service operations. */
    VMMR0_DO_SRV_END,

    /** Official call we use for testing Ring-0 APIs. */
    VMMR0_DO_TESTS,

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


/**
 * The Ring 0 entry point, called by the interrupt gate.
 *
 * @returns VBox status code.
 * @param   pVM             The VM to operate on.
 * @param   enmOperation    Which operation to execute.
 * @param   pvArg           Argument to the operation.
 * @remarks Assume called with interrupts disabled.
 */
VMMR0DECL(int) VMMR0EntryInt(PVM pVM, VMMR0OPERATION enmOperation, void *pvArg);

/**
 * The Ring 0 entry point, called by the fast-ioctl path.
 *
 * @returns VBox status code.
 * @param   pVM             The VM to operate on.
 * @param   enmOperation    Which operation to execute.
 * @remarks Assume called with interrupts _enabled_.
 */
VMMR0DECL(int) VMMR0EntryFast(PVM pVM, VMMR0OPERATION enmOperation);

/**
 * The Ring 0 entry point, called by the support library (SUP).
 *
 * @returns VBox status code.
 * @param   pVM             The VM to operate on.
 * @param   enmOperation    Which operation to execute.
 * @param   pReq            This points to a SUPVMMR0REQHDR packet. Optional.
 * @param   u64Arg          Some simple constant argument.
 * @remarks Assume called with interrupts _enabled_.
 */
VMMR0DECL(int) VMMR0EntryEx(PVM pVM, VMMR0OPERATION enmOperation, PSUPVMMR0REQHDR pReq, uint64_t u64Arg);

/**
 * Calls the ring-3 host code.
 *
 * @returns VBox status code of the ring-3 call.
 * @param   pVM             The VM handle.
 * @param   enmOperation    The operation.
 * @param   uArg            The argument to the operation.
 */
VMMR0DECL(int) VMMR0CallHost(PVM pVM, VMMCALLHOST enmOperation, uint64_t uArg);

/** @} */


#ifdef IN_GC
/** @defgroup grp_vmm_gc    The VMM Guest Context API
 * @ingroup grp_vmm
 * @{
 */

/**
 * The GC entry point.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   uOperation  Which operation to execute (VMMGCOPERATION).
 * @param   uArg        Argument to that operation.
 * @param   ...         Additional arguments.
 */
VMMGCDECL(int) VMMGCEntry(PVM pVM, unsigned uOperation, unsigned uArg, ...);

/**
 * Switches from guest context to host context.
 *
 * @param   pVM         The VM handle.
 * @param   rc          The status code.
 */
VMMGCDECL(void) VMMGCGuestToHost(PVM pVM, int rc);

/**
 * Calls the ring-3 host code.
 *
 * @returns VBox status code of the ring-3 call.
 * @param   pVM             The VM handle.
 * @param   enmOperation    The operation.
 * @param   uArg            The argument to the operation.
 */
VMMGCDECL(int) VMMGCCallHost(PVM pVM, VMMCALLHOST enmOperation, uint64_t uArg);

/** @} */
#endif


/** @} */
__END_DECLS


#endif
