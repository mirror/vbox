/* $Id$ */
/** @file
 * VMM - Host Context Ring 0.
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
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_VMM
#include <VBox/vmm.h>
#include <VBox/sup.h>
#include <VBox/trpm.h>
#include <VBox/cpum.h>
#include <VBox/pgm.h>
#include <VBox/stam.h>
#include <VBox/tm.h>
#include "VMMInternal.h"
#include <VBox/vm.h>
#include <VBox/gvmm.h>
#include <VBox/gmm.h>
#include <VBox/intnet.h>
#include <VBox/hwaccm.h>
#include <VBox/param.h>

#include <VBox/err.h>
#include <VBox/version.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/stdarg.h>
#include <iprt/mp.h>
#include <iprt/string.h>

#if defined(_MSC_VER) && defined(RT_ARCH_AMD64) /** @todo check this with with VC7! */
#  pragma intrinsic(_AddressOfReturnAddress)
#endif


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
__BEGIN_DECLS
VMMR0DECL(int) ModuleInit(void);
VMMR0DECL(void) ModuleTerm(void);
__END_DECLS


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Pointer to the internal networking service instance. */
PINTNET g_pIntNet = 0;


/**
 * Initialize the module.
 * This is called when we're first loaded.
 *
 * @returns 0 on success.
 * @returns VBox status on failure.
 */
VMMR0DECL(int) ModuleInit(void)
{
    LogFlow(("ModuleInit:\n"));

    /*
     * Initialize the GVMM, GMM, HWACCM, PGM (Darwin) and INTNET.
     */
    int rc = GVMMR0Init();
    if (RT_SUCCESS(rc))
    {
        rc = GMMR0Init();
        if (RT_SUCCESS(rc))
        {
            rc = HWACCMR0Init();
            if (RT_SUCCESS(rc))
            {
                rc = PGMRegisterStringFormatTypes();
                if (RT_SUCCESS(rc))
                {
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE
                    rc = PGMR0DynMapInit();
#endif
                    if (RT_SUCCESS(rc))
                    {
                        LogFlow(("ModuleInit: g_pIntNet=%p\n", g_pIntNet));
                        g_pIntNet = NULL;
                        LogFlow(("ModuleInit: g_pIntNet=%p should be NULL now...\n", g_pIntNet));
                        rc = INTNETR0Create(&g_pIntNet);
                        if (RT_SUCCESS(rc))
                        {
                            LogFlow(("ModuleInit: returns success. g_pIntNet=%p\n", g_pIntNet));
                            return VINF_SUCCESS;
                        }

                        /* bail out */
                        g_pIntNet = NULL;
                        LogFlow(("ModuleTerm: returns %Rrc\n", rc));
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE
                        PGMR0DynMapTerm();
#endif
                    }
                    PGMDeregisterStringFormatTypes();
                }
                HWACCMR0Term();
            }
            GMMR0Term();
        }
       GVMMR0Term();
    }

    LogFlow(("ModuleInit: failed %Rrc\n", rc));
    return rc;
}


/**
 * Terminate the module.
 * This is called when we're finally unloaded.
 */
VMMR0DECL(void) ModuleTerm(void)
{
    LogFlow(("ModuleTerm:\n"));

    /*
     * Destroy the internal networking instance.
     */
    if (g_pIntNet)
    {
        INTNETR0Destroy(g_pIntNet);
        g_pIntNet = NULL;
    }

    /*
     * PGM (Darwin) and HWACCM global cleanup.
     * Destroy the GMM and GVMM instances.
     */
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE
    PGMR0DynMapTerm();
#endif
    PGMDeregisterStringFormatTypes();
    HWACCMR0Term();

    GMMR0Term();
    GVMMR0Term();

    LogFlow(("ModuleTerm: returns\n"));
}


/**
 * Initaties the R0 driver for a particular VM instance.
 *
 * @returns VBox status code.
 *
 * @param   pVM         The VM instance in question.
 * @param   uSvnRev     The SVN revision of the ring-3 part.
 * @thread  EMT.
 */
static int vmmR0InitVM(PVM pVM, uint32_t uSvnRev)
{
    /*
     * Match the SVN revisions.
     */
    if (uSvnRev != VMMGetSvnRev())
    {
        LogRel(("VMMR0InitVM: Revision mismatch, r3=%d r0=%d\n", uSvnRev, VMMGetSvnRev()));
        SUPR0Printf("VMMR0InitVM: Revision mismatch, r3=%d r0=%d\n", uSvnRev, VMMGetSvnRev());
        return VERR_VERSION_MISMATCH;
    }
    if (    !VALID_PTR(pVM)
        ||  pVM->pVMR0 != pVM)
        return VERR_INVALID_PARAMETER;

    /*
     * Register the EMT R0 logger instance.
     */
    PVMMR0LOGGER pR0Logger = pVM->vmm.s.pR0LoggerR0;
    if (pR0Logger)
    {
#if 0 /* testing of the logger. */
        LogCom(("vmmR0InitVM: before %p\n", RTLogDefaultInstance()));
        LogCom(("vmmR0InitVM: pfnFlush=%p actual=%p\n", pR0Logger->Logger.pfnFlush, vmmR0LoggerFlush));
        LogCom(("vmmR0InitVM: pfnLogger=%p actual=%p\n", pR0Logger->Logger.pfnLogger, vmmR0LoggerWrapper));
        LogCom(("vmmR0InitVM: offScratch=%d fFlags=%#x fDestFlags=%#x\n", pR0Logger->Logger.offScratch, pR0Logger->Logger.fFlags, pR0Logger->Logger.fDestFlags));

        RTLogSetDefaultInstanceThread(&pR0Logger->Logger, (uintptr_t)pVM->pSession);
        LogCom(("vmmR0InitVM: after %p reg\n", RTLogDefaultInstance()));
        RTLogSetDefaultInstanceThread(NULL, 0);
        LogCom(("vmmR0InitVM: after %p dereg\n", RTLogDefaultInstance()));

        pR0Logger->Logger.pfnLogger("hello ring-0 logger\n");
        LogCom(("vmmR0InitVM: returned succesfully from direct logger call.\n"));
        pR0Logger->Logger.pfnFlush(&pR0Logger->Logger);
        LogCom(("vmmR0InitVM: returned succesfully from direct flush call.\n"));

        RTLogSetDefaultInstanceThread(&pR0Logger->Logger, (uintptr_t)pVM->pSession);
        LogCom(("vmmR0InitVM: after %p reg2\n", RTLogDefaultInstance()));
        pR0Logger->Logger.pfnLogger("hello ring-0 logger\n");
        LogCom(("vmmR0InitVM: returned succesfully from direct logger call (2). offScratch=%d\n", pR0Logger->Logger.offScratch));
        RTLogSetDefaultInstanceThread(NULL, 0);
        LogCom(("vmmR0InitVM: after %p dereg2\n", RTLogDefaultInstance()));

        RTLogLoggerEx(&pR0Logger->Logger, 0, ~0U, "hello ring-0 logger (RTLogLoggerEx)\n");
        LogCom(("vmmR0InitVM: RTLogLoggerEx returned fine offScratch=%d\n", pR0Logger->Logger.offScratch));

        RTLogSetDefaultInstanceThread(&pR0Logger->Logger, (uintptr_t)pVM->pSession);
        RTLogPrintf("hello ring-0 logger (RTLogPrintf)\n");
        LogCom(("vmmR0InitVM: RTLogPrintf returned fine offScratch=%d\n", pR0Logger->Logger.offScratch));
#endif
        Log(("Switching to per-thread logging instance %p (key=%p)\n", &pR0Logger->Logger, pVM->pSession));
        RTLogSetDefaultInstanceThread(&pR0Logger->Logger, (uintptr_t)pVM->pSession);
    }

    /*
     * Initialize the per VM data for GVMM and GMM.
     */
    int rc = GVMMR0InitVM(pVM);
//    if (RT_SUCCESS(rc))
//        rc = GMMR0InitPerVMData(pVM);
    if (RT_SUCCESS(rc))
    {
        /*
         * Init HWACCM, CPUM and PGM (Darwin only).
         */
        rc = HWACCMR0InitVM(pVM);
        if (RT_SUCCESS(rc))
        {
            rc = CPUMR0Init(pVM); /** @todo rename to CPUMR0InitVM */
            if (RT_SUCCESS(rc))
            {
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE
                rc = PGMR0DynMapInitVM(pVM);
#endif
                if (RT_SUCCESS(rc))
                {
                    GVMMR0DoneInitVM(pVM);
                    return rc;
                }

                /* bail out */
            }
            HWACCMR0TermVM(pVM);
        }
    }
    RTLogSetDefaultInstanceThread(NULL, 0);
    return rc;
}


/**
 * Terminates the R0 driver for a particular VM instance.
 *
 * This is normally called by ring-3 as part of the VM termination process, but
 * may alternatively be called during the support driver session cleanup when
 * the VM object is destroyed (see GVMM).
 *
 * @returns VBox status code.
 *
 * @param   pVM         The VM instance in question.
 * @param   pGVM        Pointer to the global VM structure. Optional.
 * @thread  EMT or session clean up thread.
 */
VMMR0DECL(int) VMMR0TermVM(PVM pVM, PGVM pGVM)
{
    /*
     * Tell GVMM what we're up to and check that we only do this once.
     */
    if (GVMMR0DoingTermVM(pVM, pGVM))
    {
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE
        PGMR0DynMapTermVM(pVM);
#endif
        HWACCMR0TermVM(pVM);
    }

    /*
     * Deregister the logger.
     */
    RTLogSetDefaultInstanceThread(NULL, 0);
    return VINF_SUCCESS;
}


/**
 * Calls the ring-3 host code.
 *
 * @returns VBox status code of the ring-3 call.
 * @param   pVM             The VM handle.
 * @param   enmOperation    The operation.
 * @param   uArg            The argument to the operation.
 */
VMMR0DECL(int) VMMR0CallHost(PVM pVM, VMMCALLHOST enmOperation, uint64_t uArg)
{
/** @todo profile this! */
    pVM->vmm.s.enmCallHostOperation = enmOperation;
    pVM->vmm.s.u64CallHostArg = uArg;
    pVM->vmm.s.rcCallHost = VERR_INTERNAL_ERROR;
    int rc = vmmR0CallHostLongJmp(&pVM->vmm.s.CallHostR0JmpBuf, VINF_VMM_CALL_HOST);
    if (rc == VINF_SUCCESS)
        rc = pVM->vmm.s.rcCallHost;
    return rc;
}


#ifdef VBOX_WITH_STATISTICS
/**
 * Record return code statistics
 * @param   pVM         The VM handle.
 * @param   rc          The status code.
 */
static void vmmR0RecordRC(PVM pVM, int rc)
{
    /*
     * Collect statistics.
     */
    switch (rc)
    {
        case VINF_SUCCESS:
            STAM_COUNTER_INC(&pVM->vmm.s.StatRZRetNormal);
            break;
        case VINF_EM_RAW_INTERRUPT:
            STAM_COUNTER_INC(&pVM->vmm.s.StatRZRetInterrupt);
            break;
        case VINF_EM_RAW_INTERRUPT_HYPER:
            STAM_COUNTER_INC(&pVM->vmm.s.StatRZRetInterruptHyper);
            break;
        case VINF_EM_RAW_GUEST_TRAP:
            STAM_COUNTER_INC(&pVM->vmm.s.StatRZRetGuestTrap);
            break;
        case VINF_EM_RAW_RING_SWITCH:
            STAM_COUNTER_INC(&pVM->vmm.s.StatRZRetRingSwitch);
            break;
        case VINF_EM_RAW_RING_SWITCH_INT:
            STAM_COUNTER_INC(&pVM->vmm.s.StatRZRetRingSwitchInt);
            break;
        case VINF_EM_RAW_EXCEPTION_PRIVILEGED:
            STAM_COUNTER_INC(&pVM->vmm.s.StatRZRetExceptionPrivilege);
            break;
        case VINF_EM_RAW_STALE_SELECTOR:
            STAM_COUNTER_INC(&pVM->vmm.s.StatRZRetStaleSelector);
            break;
        case VINF_EM_RAW_IRET_TRAP:
            STAM_COUNTER_INC(&pVM->vmm.s.StatRZRetIRETTrap);
            break;
        case VINF_IOM_HC_IOPORT_READ:
            STAM_COUNTER_INC(&pVM->vmm.s.StatRZRetIORead);
            break;
        case VINF_IOM_HC_IOPORT_WRITE:
            STAM_COUNTER_INC(&pVM->vmm.s.StatRZRetIOWrite);
            break;
        case VINF_IOM_HC_MMIO_READ:
            STAM_COUNTER_INC(&pVM->vmm.s.StatRZRetMMIORead);
            break;
        case VINF_IOM_HC_MMIO_WRITE:
            STAM_COUNTER_INC(&pVM->vmm.s.StatRZRetMMIOWrite);
            break;
        case VINF_IOM_HC_MMIO_READ_WRITE:
            STAM_COUNTER_INC(&pVM->vmm.s.StatRZRetMMIOReadWrite);
            break;
        case VINF_PATM_HC_MMIO_PATCH_READ:
            STAM_COUNTER_INC(&pVM->vmm.s.StatRZRetMMIOPatchRead);
            break;
        case VINF_PATM_HC_MMIO_PATCH_WRITE:
            STAM_COUNTER_INC(&pVM->vmm.s.StatRZRetMMIOPatchWrite);
            break;
        case VINF_EM_RAW_EMULATE_INSTR:
            STAM_COUNTER_INC(&pVM->vmm.s.StatRZRetEmulate);
            break;
        case VINF_EM_RAW_EMULATE_IO_BLOCK:
            STAM_COUNTER_INC(&pVM->vmm.s.StatRZRetIOBlockEmulate);
            break;
        case VINF_PATCH_EMULATE_INSTR:
            STAM_COUNTER_INC(&pVM->vmm.s.StatRZRetPatchEmulate);
            break;
        case VINF_EM_RAW_EMULATE_INSTR_LDT_FAULT:
            STAM_COUNTER_INC(&pVM->vmm.s.StatRZRetLDTFault);
            break;
        case VINF_EM_RAW_EMULATE_INSTR_GDT_FAULT:
            STAM_COUNTER_INC(&pVM->vmm.s.StatRZRetGDTFault);
            break;
        case VINF_EM_RAW_EMULATE_INSTR_IDT_FAULT:
            STAM_COUNTER_INC(&pVM->vmm.s.StatRZRetIDTFault);
            break;
        case VINF_EM_RAW_EMULATE_INSTR_TSS_FAULT:
            STAM_COUNTER_INC(&pVM->vmm.s.StatRZRetTSSFault);
            break;
        case VINF_EM_RAW_EMULATE_INSTR_PD_FAULT:
            STAM_COUNTER_INC(&pVM->vmm.s.StatRZRetPDFault);
            break;
        case VINF_CSAM_PENDING_ACTION:
            STAM_COUNTER_INC(&pVM->vmm.s.StatRZRetCSAMTask);
            break;
        case VINF_PGM_SYNC_CR3:
            STAM_COUNTER_INC(&pVM->vmm.s.StatRZRetSyncCR3);
            break;
        case VINF_PATM_PATCH_INT3:
            STAM_COUNTER_INC(&pVM->vmm.s.StatRZRetPatchInt3);
            break;
        case VINF_PATM_PATCH_TRAP_PF:
            STAM_COUNTER_INC(&pVM->vmm.s.StatRZRetPatchPF);
            break;
        case VINF_PATM_PATCH_TRAP_GP:
            STAM_COUNTER_INC(&pVM->vmm.s.StatRZRetPatchGP);
            break;
        case VINF_PATM_PENDING_IRQ_AFTER_IRET:
            STAM_COUNTER_INC(&pVM->vmm.s.StatRZRetPatchIretIRQ);
            break;
        case VERR_REM_FLUSHED_PAGES_OVERFLOW:
            STAM_COUNTER_INC(&pVM->vmm.s.StatRZRetPageOverflow);
            break;
        case VINF_EM_RESCHEDULE_REM:
            STAM_COUNTER_INC(&pVM->vmm.s.StatRZRetRescheduleREM);
            break;
        case VINF_EM_RAW_TO_R3:
            STAM_COUNTER_INC(&pVM->vmm.s.StatRZRetToR3);
            break;
        case VINF_EM_RAW_TIMER_PENDING:
            STAM_COUNTER_INC(&pVM->vmm.s.StatRZRetTimerPending);
            break;
        case VINF_EM_RAW_INTERRUPT_PENDING:
            STAM_COUNTER_INC(&pVM->vmm.s.StatRZRetInterruptPending);
            break;
        case VINF_VMM_CALL_HOST:
            switch (pVM->vmm.s.enmCallHostOperation)
            {
                case VMMCALLHOST_PDM_LOCK:
                    STAM_COUNTER_INC(&pVM->vmm.s.StatRZCallPDMLock);
                    break;
                case VMMCALLHOST_PDM_QUEUE_FLUSH:
                    STAM_COUNTER_INC(&pVM->vmm.s.StatRZCallPDMQueueFlush);
                    break;
                case VMMCALLHOST_PGM_POOL_GROW:
                    STAM_COUNTER_INC(&pVM->vmm.s.StatRZCallPGMPoolGrow);
                    break;
                case VMMCALLHOST_PGM_LOCK:
                    STAM_COUNTER_INC(&pVM->vmm.s.StatRZCallPGMLock);
                    break;
                case VMMCALLHOST_PGM_MAP_CHUNK:
                    STAM_COUNTER_INC(&pVM->vmm.s.StatRZCallPGMMapChunk);
                    break;
                case VMMCALLHOST_PGM_ALLOCATE_HANDY_PAGES:
                    STAM_COUNTER_INC(&pVM->vmm.s.StatRZCallPGMAllocHandy);
                    break;
                case VMMCALLHOST_REM_REPLAY_HANDLER_NOTIFICATIONS:
                    STAM_COUNTER_INC(&pVM->vmm.s.StatRZCallRemReplay);
                    break;
                case VMMCALLHOST_VMM_LOGGER_FLUSH:
                    STAM_COUNTER_INC(&pVM->vmm.s.StatRZCallLogFlush);
                    break;
                case VMMCALLHOST_VM_SET_ERROR:
                    STAM_COUNTER_INC(&pVM->vmm.s.StatRZCallVMSetError);
                    break;
                case VMMCALLHOST_VM_SET_RUNTIME_ERROR:
                    STAM_COUNTER_INC(&pVM->vmm.s.StatRZCallVMSetRuntimeError);
                    break;
                case VMMCALLHOST_VM_R0_ASSERTION:
                default:
                    STAM_COUNTER_INC(&pVM->vmm.s.StatRZRetCallHost);
                    break;
            }
            break;
        case VINF_PATM_DUPLICATE_FUNCTION:
            STAM_COUNTER_INC(&pVM->vmm.s.StatRZRetPATMDuplicateFn);
            break;
        case VINF_PGM_CHANGE_MODE:
            STAM_COUNTER_INC(&pVM->vmm.s.StatRZRetPGMChangeMode);
            break;
        case VINF_EM_RAW_EMULATE_INSTR_HLT:
            STAM_COUNTER_INC(&pVM->vmm.s.StatRZRetEmulHlt);
            break;
        case VINF_EM_PENDING_REQUEST:
            STAM_COUNTER_INC(&pVM->vmm.s.StatRZRetPendingRequest);
            break;
        default:
            STAM_COUNTER_INC(&pVM->vmm.s.StatRZRetMisc);
            break;
    }
}
#endif /* VBOX_WITH_STATISTICS */


/**
 * Unused ring-0 entry point that used to be called from the interrupt gate.
 *
 * Will be removed one of the next times we do a major SUPDrv version bump.
 *
 * @returns VBox status code.
 * @param   pVM             The VM to operate on.
 * @param   enmOperation    Which operation to execute.
 * @param   pvArg           Argument to the operation.
 * @remarks Assume called with interrupts disabled.
 */
VMMR0DECL(int) VMMR0EntryInt(PVM pVM, VMMR0OPERATION enmOperation, void *pvArg)
{
    /*
     * We're returning VERR_NOT_SUPPORT here so we've got something else
     * than -1 which the interrupt gate glue code might return.
     */
    Log(("operation %#x is not supported\n", enmOperation));
    return VERR_NOT_SUPPORTED;
}


/**
 * The Ring 0 entry point, called by the fast-ioctl path.
 *
 * @param   pVM             The VM to operate on.
 *                          The return code is stored in pVM->vmm.s.iLastGZRc.
 * @param   idCpu           VMCPU id.
 * @param   enmOperation    Which operation to execute.
 * @remarks Assume called with interrupts _enabled_.
 */
VMMR0DECL(void) VMMR0EntryFast(PVM pVM, unsigned idCpu, VMMR0OPERATION enmOperation)
{
    PVMCPU pVCpu = &pVM->aCpus[idCpu];

    if (RT_UNLIKELY(idCpu >= pVM->cCPUs))
    {
        pVM->vmm.s.iLastGZRc = VERR_INVALID_PARAMETER;
        return;
    }

    switch (enmOperation)
    {
        /*
         * Switch to GC and run guest raw mode code.
         * Disable interrupts before doing the world switch.
         */
        case VMMR0_DO_RAW_RUN:
        {
            /* Safety precaution as hwaccm disables the switcher. */
            if (RT_LIKELY(!pVM->vmm.s.fSwitcherDisabled))
            {
                RTCCUINTREG uFlags = ASMIntDisableFlags();
                int rc;
                bool fVTxDisabled;

                if (RT_UNLIKELY(pVM->cCPUs > 1))
                {
                    pVM->vmm.s.iLastGZRc = VERR_RAW_MODE_INVALID_SMP;
                    return;
                }

#ifndef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
                if (RT_UNLIKELY(!PGMGetHyperCR3(pVCpu)))
                {
                    pVM->vmm.s.iLastGZRc = VERR_PGM_NO_CR3_SHADOW_ROOT;
                    return;
                }
#endif

                /* We might need to disable VT-x if the active switcher turns off paging. */
                rc = HWACCMR0EnterSwitcher(pVM, &fVTxDisabled);
                if (RT_FAILURE(rc))
                {
                    pVM->vmm.s.iLastGZRc = rc;
                    return;
                }

                TMNotifyStartOfExecution(pVCpu);
                rc = pVM->vmm.s.pfnHostToGuestR0(pVM);
                pVM->vmm.s.iLastGZRc = rc;
                TMNotifyEndOfExecution(pVCpu);

                /* Re-enable VT-x if previously turned off. */
                HWACCMR0LeaveSwitcher(pVM, fVTxDisabled);

                if (    rc == VINF_EM_RAW_INTERRUPT
                    ||  rc == VINF_EM_RAW_INTERRUPT_HYPER)
                    TRPMR0DispatchHostInterrupt(pVM);

                ASMSetFlags(uFlags);

#ifdef VBOX_WITH_STATISTICS
                STAM_COUNTER_INC(&pVM->vmm.s.StatRunRC);
                vmmR0RecordRC(pVM, rc);
#endif
            }
            else
            {
                Assert(!pVM->vmm.s.fSwitcherDisabled);
                pVM->vmm.s.iLastGZRc = VERR_NOT_SUPPORTED;
            }
            break;
        }

        /*
         * Run guest code using the available hardware acceleration technology.
         *
         * Disable interrupts before we do anything interesting. On Windows we avoid
         * this by having the support driver raise the IRQL before calling us, this way
         * we hope to get away with page faults and later calling into the kernel.
         */
        case VMMR0_DO_HWACC_RUN:
        {
            int rc;
            PVMCPU pVCpu = &pVM->aCpus[idCpu];

            STAM_COUNTER_INC(&pVM->vmm.s.StatRunRC);

#ifndef RT_OS_WINDOWS /** @todo check other hosts */
            RTCCUINTREG uFlags = ASMIntDisableFlags();
#endif
            if (!HWACCMR0SuspendPending())
            {
                rc = HWACCMR0Enter(pVM, pVCpu);
                if (RT_SUCCESS(rc))
                {
                    rc = vmmR0CallHostSetJmp(&pVM->vmm.s.CallHostR0JmpBuf, HWACCMR0RunGuestCode, pVM, pVCpu); /* this may resume code. */
                    int rc2 = HWACCMR0Leave(pVM, pVCpu);
                    AssertRC(rc2);
                }
            }
            else
            {
                /* System is about to go into suspend mode; go back to ring 3. */
                rc = VINF_EM_RAW_INTERRUPT;
            }
            pVM->vmm.s.iLastGZRc = rc;
#ifndef RT_OS_WINDOWS /** @todo check other hosts */
            ASMSetFlags(uFlags);
#endif

#ifdef VBOX_WITH_STATISTICS
            vmmR0RecordRC(pVM, rc);
#endif
            /* No special action required for external interrupts, just return. */
            break;
        }

        /*
         * For profiling.
         */
        case VMMR0_DO_NOP:
            pVM->vmm.s.iLastGZRc = VINF_SUCCESS;
            break;

        /*
         * Impossible.
         */
        default:
            AssertMsgFailed(("%#x\n", enmOperation));
            pVM->vmm.s.iLastGZRc = VERR_NOT_SUPPORTED;
            break;
    }
}


/**
 * Validates a session or VM session argument.
 *
 * @returns true / false accordingly.
 * @param   pVM         The VM argument.
 * @param   pSession    The session argument.
 */
DECLINLINE(bool) vmmR0IsValidSession(PVM pVM, PSUPDRVSESSION pClaimedSession, PSUPDRVSESSION pSession)
{
    /* This must be set! */
    if (!pSession)
        return false;

    /* Only one out of the two. */
    if (pVM && pClaimedSession)
        return false;
    if (pVM)
        pClaimedSession = pVM->pSession;
    return pClaimedSession == pSession;
}


/**
 * VMMR0EntryEx worker function, either called directly or when ever possible
 * called thru a longjmp so we can exit safely on failure.
 *
 * @returns VBox status code.
 * @param   pVM             The VM to operate on.
 * @param   idCpu           VMCPU id.
 * @param   enmOperation    Which operation to execute.
 * @param   pReqHdr         This points to a SUPVMMR0REQHDR packet. Optional.
 *                          The support driver validates this if it's present.
 * @param   u64Arg          Some simple constant argument.
 * @param   pSession        The session of the caller.
 * @remarks Assume called with interrupts _enabled_.
 */
static int vmmR0EntryExWorker(PVM pVM, unsigned idCpu, VMMR0OPERATION enmOperation, PSUPVMMR0REQHDR pReqHdr, uint64_t u64Arg, PSUPDRVSESSION pSession)
{
    /*
     * Common VM pointer validation.
     */
    if (pVM)
    {
        if (RT_UNLIKELY(    !VALID_PTR(pVM)
                        ||  ((uintptr_t)pVM & PAGE_OFFSET_MASK)))
        {
            SUPR0Printf("vmmR0EntryExWorker: Invalid pVM=%p! (op=%d)\n", pVM, enmOperation);
            return VERR_INVALID_POINTER;
        }
        if (RT_UNLIKELY(    pVM->enmVMState < VMSTATE_CREATING
                        ||  pVM->enmVMState > VMSTATE_TERMINATED
                        ||  pVM->pVMR0 != pVM))
        {
            SUPR0Printf("vmmR0EntryExWorker: Invalid pVM=%p:{enmVMState=%d, .pVMR0=%p}! (op=%d)\n",
                        pVM, pVM->enmVMState, pVM->pVMR0, enmOperation);
            return VERR_INVALID_POINTER;
        }

        if (RT_UNLIKELY(idCpu >= pVM->cCPUs))
        {
            SUPR0Printf("vmmR0EntryExWorker: Invalid idCpu (%d vs cCPUs=%d\n", idCpu, pVM->cCPUs);
            return VERR_INVALID_PARAMETER;
        }
    }

    switch (enmOperation)
    {
        /*
         * GVM requests
         */
        case VMMR0_DO_GVMM_CREATE_VM:
            if (pVM || u64Arg)
                return VERR_INVALID_PARAMETER;
            return GVMMR0CreateVMReq((PGVMMCREATEVMREQ)pReqHdr);

        case VMMR0_DO_GVMM_DESTROY_VM:
            if (pReqHdr || u64Arg)
                return VERR_INVALID_PARAMETER;
            return GVMMR0DestroyVM(pVM);

        case VMMR0_DO_GVMM_REGISTER_VMCPU:
            if (!pVM)
                return VERR_INVALID_PARAMETER;
            return GVMMR0RegisterVCpu(pVM, idCpu);

        case VMMR0_DO_GVMM_SCHED_HALT:
            if (pReqHdr)
                return VERR_INVALID_PARAMETER;
            return GVMMR0SchedHalt(pVM, idCpu, u64Arg);

        case VMMR0_DO_GVMM_SCHED_WAKE_UP:
            if (pReqHdr || u64Arg)
                return VERR_INVALID_PARAMETER;
            return GVMMR0SchedWakeUp(pVM, idCpu);

        case VMMR0_DO_GVMM_SCHED_POLL:
            if (pReqHdr || u64Arg > 1)
                return VERR_INVALID_PARAMETER;
            return GVMMR0SchedPoll(pVM, idCpu, !!u64Arg);

        case VMMR0_DO_GVMM_QUERY_STATISTICS:
            if (u64Arg)
                return VERR_INVALID_PARAMETER;
            return GVMMR0QueryStatisticsReq(pVM, (PGVMMQUERYSTATISTICSSREQ)pReqHdr);

        case VMMR0_DO_GVMM_RESET_STATISTICS:
            if (u64Arg)
                return VERR_INVALID_PARAMETER;
            return GVMMR0ResetStatisticsReq(pVM, (PGVMMRESETSTATISTICSSREQ)pReqHdr);

        /*
         * Initialize the R0 part of a VM instance.
         */
        case VMMR0_DO_VMMR0_INIT:
            return vmmR0InitVM(pVM, (uint32_t)u64Arg);

        /*
         * Terminate the R0 part of a VM instance.
         */
        case VMMR0_DO_VMMR0_TERM:
            return VMMR0TermVM(pVM, NULL);

        /*
         * Attempt to enable hwacc mode and check the current setting.
         *
         */
        case VMMR0_DO_HWACC_ENABLE:
            return HWACCMR0EnableAllCpus(pVM);

        /*
         * Setup the hardware accelerated raw-mode session.
         */
        case VMMR0_DO_HWACC_SETUP_VM:
        {
            RTCCUINTREG fFlags = ASMIntDisableFlags();
            int rc = HWACCMR0SetupVM(pVM);
            ASMSetFlags(fFlags);
            return rc;
        }

        /*
         * Switch to GC to execute Hypervisor function.
         */
        case VMMR0_DO_CALL_HYPERVISOR:
        {
            int rc;
            bool fVTxDisabled;

            /* Safety precaution as HWACCM can disable the switcher. */
            Assert(!pVM->vmm.s.fSwitcherDisabled);
            if (RT_UNLIKELY(pVM->vmm.s.fSwitcherDisabled))
                return VERR_NOT_SUPPORTED;

#ifndef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
            if (RT_UNLIKELY(!PGMGetHyperCR3(VMMGetCpu0(pVM))))
                return VERR_PGM_NO_CR3_SHADOW_ROOT;
#endif

            RTCCUINTREG fFlags = ASMIntDisableFlags();

            /* We might need to disable VT-x if the active switcher turns off paging. */
            rc = HWACCMR0EnterSwitcher(pVM, &fVTxDisabled);
            if (RT_FAILURE(rc))
                return rc;

            rc = pVM->vmm.s.pfnHostToGuestR0(pVM);

            /* Re-enable VT-x if previously turned off. */
            HWACCMR0LeaveSwitcher(pVM, fVTxDisabled);

            /** @todo dispatch interrupts? */
            ASMSetFlags(fFlags);
            return rc;
        }

        /*
         * PGM wrappers.
         */
        case VMMR0_DO_PGM_ALLOCATE_HANDY_PAGES:
            return PGMR0PhysAllocateHandyPages(pVM, &pVM->aCpus[idCpu]);

        /*
         * GMM wrappers.
         */
        case VMMR0_DO_GMM_INITIAL_RESERVATION:
            if (u64Arg)
                return VERR_INVALID_PARAMETER;
            return GMMR0InitialReservationReq(pVM, idCpu, (PGMMINITIALRESERVATIONREQ)pReqHdr);

        case VMMR0_DO_GMM_UPDATE_RESERVATION:
            if (u64Arg)
                return VERR_INVALID_PARAMETER;
            return GMMR0UpdateReservationReq(pVM, idCpu, (PGMMUPDATERESERVATIONREQ)pReqHdr);

        case VMMR0_DO_GMM_ALLOCATE_PAGES:
            if (u64Arg)
                return VERR_INVALID_PARAMETER;
            return GMMR0AllocatePagesReq(pVM, idCpu, (PGMMALLOCATEPAGESREQ)pReqHdr);

        case VMMR0_DO_GMM_FREE_PAGES:
            if (u64Arg)
                return VERR_INVALID_PARAMETER;
            return GMMR0FreePagesReq(pVM, idCpu, (PGMMFREEPAGESREQ)pReqHdr);

        case VMMR0_DO_GMM_BALLOONED_PAGES:
            if (u64Arg)
                return VERR_INVALID_PARAMETER;
            return GMMR0BalloonedPagesReq(pVM, idCpu, (PGMMBALLOONEDPAGESREQ)pReqHdr);

        case VMMR0_DO_GMM_DEFLATED_BALLOON:
            if (pReqHdr)
                return VERR_INVALID_PARAMETER;
            return GMMR0DeflatedBalloon(pVM, idCpu, (uint32_t)u64Arg);

        case VMMR0_DO_GMM_MAP_UNMAP_CHUNK:
            if (u64Arg)
                return VERR_INVALID_PARAMETER;
            return GMMR0MapUnmapChunkReq(pVM, idCpu, (PGMMMAPUNMAPCHUNKREQ)pReqHdr);

        case VMMR0_DO_GMM_SEED_CHUNK:
            if (pReqHdr)
                return VERR_INVALID_PARAMETER;
            return GMMR0SeedChunk(pVM, idCpu, (RTR3PTR)u64Arg);

        /*
         * A quick GCFGM mock-up.
         */
        /** @todo GCFGM with proper access control, ring-3 management interface and all that. */
        case VMMR0_DO_GCFGM_SET_VALUE:
        case VMMR0_DO_GCFGM_QUERY_VALUE:
        {
            if (pVM || !pReqHdr || u64Arg)
                return VERR_INVALID_PARAMETER;
            PGCFGMVALUEREQ pReq = (PGCFGMVALUEREQ)pReqHdr;
            if (pReq->Hdr.cbReq != sizeof(*pReq))
                return VERR_INVALID_PARAMETER;
            int rc;
            if (enmOperation == VMMR0_DO_GCFGM_SET_VALUE)
            {
                rc = GVMMR0SetConfig(pReq->pSession, &pReq->szName[0], pReq->u64Value);
                //if (rc == VERR_CFGM_VALUE_NOT_FOUND)
                //    rc = GMMR0SetConfig(pReq->pSession, &pReq->szName[0], pReq->u64Value);
            }
            else
            {
                rc = GVMMR0QueryConfig(pReq->pSession, &pReq->szName[0], &pReq->u64Value);
                //if (rc == VERR_CFGM_VALUE_NOT_FOUND)
                //    rc = GMMR0QueryConfig(pReq->pSession, &pReq->szName[0], &pReq->u64Value);
            }
            return rc;
        }


        /*
         * Requests to the internal networking service.
         */
        case VMMR0_DO_INTNET_OPEN:
        {
            PINTNETOPENREQ pReq = (PINTNETOPENREQ)pReqHdr;
            if (u64Arg || !pReq || !vmmR0IsValidSession(pVM, pReq->pSession, pSession))
                return VERR_INVALID_PARAMETER;
            if (!g_pIntNet)
                return VERR_NOT_SUPPORTED;
            return INTNETR0OpenReq(g_pIntNet, pSession, pReq);
        }

        case VMMR0_DO_INTNET_IF_CLOSE:
            if (u64Arg || !pReqHdr || !vmmR0IsValidSession(pVM, ((PINTNETIFCLOSEREQ)pReqHdr)->pSession, pSession))
                return VERR_INVALID_PARAMETER;
            if (!g_pIntNet)
                return VERR_NOT_SUPPORTED;
            return INTNETR0IfCloseReq(g_pIntNet, pSession, (PINTNETIFCLOSEREQ)pReqHdr);

        case VMMR0_DO_INTNET_IF_GET_RING3_BUFFER:
            if (u64Arg || !pReqHdr || !vmmR0IsValidSession(pVM, ((PINTNETIFGETRING3BUFFERREQ)pReqHdr)->pSession, pSession))
                return VERR_INVALID_PARAMETER;
            if (!g_pIntNet)
                return VERR_NOT_SUPPORTED;
            return INTNETR0IfGetRing3BufferReq(g_pIntNet, pSession, (PINTNETIFGETRING3BUFFERREQ)pReqHdr);

        case VMMR0_DO_INTNET_IF_SET_PROMISCUOUS_MODE:
            if (u64Arg || !pReqHdr || !vmmR0IsValidSession(pVM, ((PINTNETIFSETPROMISCUOUSMODEREQ)pReqHdr)->pSession, pSession))
                return VERR_INVALID_PARAMETER;
            if (!g_pIntNet)
                return VERR_NOT_SUPPORTED;
            return INTNETR0IfSetPromiscuousModeReq(g_pIntNet, pSession, (PINTNETIFSETPROMISCUOUSMODEREQ)pReqHdr);

        case VMMR0_DO_INTNET_IF_SET_MAC_ADDRESS:
            if (u64Arg || !pReqHdr || !vmmR0IsValidSession(pVM, ((PINTNETIFSETMACADDRESSREQ)pReqHdr)->pSession, pSession))
                return VERR_INVALID_PARAMETER;
            if (!g_pIntNet)
                return VERR_NOT_SUPPORTED;
            return INTNETR0IfSetMacAddressReq(g_pIntNet, pSession, (PINTNETIFSETMACADDRESSREQ)pReqHdr);

        case VMMR0_DO_INTNET_IF_SET_ACTIVE:
            if (u64Arg || !pReqHdr || !vmmR0IsValidSession(pVM, ((PINTNETIFSETACTIVEREQ)pReqHdr)->pSession, pSession))
                return VERR_INVALID_PARAMETER;
            if (!g_pIntNet)
                return VERR_NOT_SUPPORTED;
            return INTNETR0IfSetActiveReq(g_pIntNet, pSession, (PINTNETIFSETACTIVEREQ)pReqHdr);

        case VMMR0_DO_INTNET_IF_SEND:
            if (u64Arg || !pReqHdr || !vmmR0IsValidSession(pVM, ((PINTNETIFSENDREQ)pReqHdr)->pSession, pSession))
                return VERR_INVALID_PARAMETER;
            if (!g_pIntNet)
                return VERR_NOT_SUPPORTED;
            return INTNETR0IfSendReq(g_pIntNet, pSession, (PINTNETIFSENDREQ)pReqHdr);

        case VMMR0_DO_INTNET_IF_WAIT:
            if (u64Arg || !pReqHdr || !vmmR0IsValidSession(pVM, ((PINTNETIFWAITREQ)pReqHdr)->pSession, pSession))
                return VERR_INVALID_PARAMETER;
            if (!g_pIntNet)
                return VERR_NOT_SUPPORTED;
            return INTNETR0IfWaitReq(g_pIntNet, pSession, (PINTNETIFWAITREQ)pReqHdr);

        /*
         * For profiling.
         */
        case VMMR0_DO_NOP:
        case VMMR0_DO_SLOW_NOP:
            return VINF_SUCCESS;

        /*
         * For testing Ring-0 APIs invoked in this environment.
         */
        case VMMR0_DO_TESTS:
            /** @todo make new test */
            return VINF_SUCCESS;


#if HC_ARCH_BITS == 32 && defined(VBOX_WITH_64_BITS_GUESTS) && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
        case VMMR0_DO_TEST_SWITCHER3264:
            return HWACCMR0TestSwitcher3264(pVM);
#endif
        default:
            /*
             * We're returning VERR_NOT_SUPPORT here so we've got something else
             * than -1 which the interrupt gate glue code might return.
             */
            Log(("operation %#x is not supported\n", enmOperation));
            return VERR_NOT_SUPPORTED;
    }
}


/**
 * Argument for vmmR0EntryExWrapper containing the arguments for VMMR0EntryEx.
 */
typedef struct VMMR0ENTRYEXARGS
{
    PVM                 pVM;
    unsigned            idCpu;
    VMMR0OPERATION      enmOperation;
    PSUPVMMR0REQHDR     pReq;
    uint64_t            u64Arg;
    PSUPDRVSESSION      pSession;
} VMMR0ENTRYEXARGS;
/** Pointer to a vmmR0EntryExWrapper argument package. */
typedef VMMR0ENTRYEXARGS *PVMMR0ENTRYEXARGS;

/**
 * This is just a longjmp wrapper function for VMMR0EntryEx calls.
 *
 * @returns VBox status code.
 * @param   pvArgs      The argument package
 */
static int vmmR0EntryExWrapper(void *pvArgs)
{
    return vmmR0EntryExWorker(((PVMMR0ENTRYEXARGS)pvArgs)->pVM,
                              ((PVMMR0ENTRYEXARGS)pvArgs)->idCpu,
                              ((PVMMR0ENTRYEXARGS)pvArgs)->enmOperation,
                              ((PVMMR0ENTRYEXARGS)pvArgs)->pReq,
                              ((PVMMR0ENTRYEXARGS)pvArgs)->u64Arg,
                              ((PVMMR0ENTRYEXARGS)pvArgs)->pSession);
}


/**
 * The Ring 0 entry point, called by the support library (SUP).
 *
 * @returns VBox status code.
 * @param   pVM             The VM to operate on.
 * @param   idCpu           VMCPU id.
 * @param   enmOperation    Which operation to execute.
 * @param   pReq            This points to a SUPVMMR0REQHDR packet. Optional.
 * @param   u64Arg          Some simple constant argument.
 * @param   pSession        The session of the caller.
 * @remarks Assume called with interrupts _enabled_.
 */
VMMR0DECL(int) VMMR0EntryEx(PVM pVM, unsigned idCpu, VMMR0OPERATION enmOperation, PSUPVMMR0REQHDR pReq, uint64_t u64Arg, PSUPDRVSESSION pSession)
{
    /*
     * Requests that should only happen on the EMT thread will be
     * wrapped in a setjmp so we can assert without causing trouble.
     */
    if (    VALID_PTR(pVM)
        &&  pVM->pVMR0)
    {
        switch (enmOperation)
        {
            /* These might/will be called before VMMR3Init. */
            case VMMR0_DO_GMM_INITIAL_RESERVATION:
            case VMMR0_DO_GMM_UPDATE_RESERVATION:
            case VMMR0_DO_GMM_ALLOCATE_PAGES:
            case VMMR0_DO_GMM_FREE_PAGES:
            case VMMR0_DO_GMM_BALLOONED_PAGES:
            case VMMR0_DO_GMM_DEFLATED_BALLOON:
            /* On the mac we might not have a valid jmp buf, so check these as well. */
            case VMMR0_DO_VMMR0_INIT:
            case VMMR0_DO_VMMR0_TERM:
            {
                if (!pVM->vmm.s.CallHostR0JmpBuf.pvSavedStack)
                    break;

                /** @todo validate this EMT claim... GVM knows. */
                VMMR0ENTRYEXARGS Args;
                Args.pVM = pVM;
                Args.idCpu = idCpu;
                Args.enmOperation = enmOperation;
                Args.pReq = pReq;
                Args.u64Arg = u64Arg;
                Args.pSession = pSession;
                return vmmR0CallHostSetJmpEx(&pVM->vmm.s.CallHostR0JmpBuf, vmmR0EntryExWrapper, &Args);
            }

            default:
                break;
        }
    }
    return vmmR0EntryExWorker(pVM, idCpu, enmOperation, pReq, u64Arg, pSession);
}


/**
 * Internal R0 logger worker: Flush logger.
 *
 * @param   pLogger     The logger instance to flush.
 * @remark  This function must be exported!
 */
VMMR0DECL(void) vmmR0LoggerFlush(PRTLOGGER pLogger)
{
    /*
     * Convert the pLogger into a VM handle and 'call' back to Ring-3.
     * (This is a bit paranoid code.)
     */
    PVMMR0LOGGER pR0Logger = (PVMMR0LOGGER)((uintptr_t)pLogger - RT_OFFSETOF(VMMR0LOGGER, Logger));
    if (    !VALID_PTR(pR0Logger)
        ||  !VALID_PTR(pR0Logger + 1)
        ||  pLogger->u32Magic != RTLOGGER_MAGIC)
    {
#ifdef DEBUG
        SUPR0Printf("vmmR0LoggerFlush: pLogger=%p!\n", pLogger);
#endif
        return;
    }
    if (pR0Logger->fFlushingDisabled)
        return; /* quietly */

    PVM pVM = pR0Logger->pVM;
    if (    !VALID_PTR(pVM)
        ||  pVM->pVMR0 != pVM)
    {
#ifdef DEBUG
        SUPR0Printf("vmmR0LoggerFlush: pVM=%p! pVMR0=%p! pLogger=%p\n", pVM, pVM->pVMR0, pLogger);
#endif
        return;
    }

    /*
     * Check that the jump buffer is armed.
     */
#ifdef RT_ARCH_X86
    if (    !pVM->vmm.s.CallHostR0JmpBuf.eip
        ||  pVM->vmm.s.CallHostR0JmpBuf.fInRing3Call)
#else
    if (    !pVM->vmm.s.CallHostR0JmpBuf.rip
        ||  pVM->vmm.s.CallHostR0JmpBuf.fInRing3Call)
#endif
    {
#ifdef DEBUG
        SUPR0Printf("vmmR0LoggerFlush: Jump buffer isn't armed!\n");
#endif
        return;
    }
    VMMR0CallHost(pVM, VMMCALLHOST_VMM_LOGGER_FLUSH, 0);
}


/**
 * Disables flushing of the ring-0 debug log.
 *
 * @param   pVCpu       The shared virtual cpu structure.
 */
VMMR0DECL(void) VMMR0LogFlushDisable(PVMCPU pVCpu)
{
    PVM pVM = pVCpu->pVMR0;
    if (pVM->vmm.s.pR0LoggerR0)
        pVM->vmm.s.pR0LoggerR0->fFlushingDisabled = true;
}


/**
 * Enables flushing of the ring-0 debug log.
 *
 * @param   pVCpu       The shared virtual cpu structure.
 */
VMMR0DECL(void) VMMR0LogFlushEnable(PVMCPU pVCpu)
{
    PVM pVM = pVCpu->pVMR0;
    if (pVM->vmm.s.pR0LoggerR0)
        pVM->vmm.s.pR0LoggerR0->fFlushingDisabled = false;
}


/**
 * Jump back to ring-3 if we're the EMT and the longjmp is armed.
 *
 * @returns true if the breakpoint should be hit, false if it should be ignored.
 */
DECLEXPORT(bool) RTCALL RTAssertShouldPanic(void)
{
#if 0
    return true;
#else
    PVM pVM = GVMMR0GetVMByEMT(NIL_RTNATIVETHREAD);
    if (pVM)
    {
#ifdef RT_ARCH_X86
        if (    pVM->vmm.s.CallHostR0JmpBuf.eip
            &&  !pVM->vmm.s.CallHostR0JmpBuf.fInRing3Call)
#else
        if (    pVM->vmm.s.CallHostR0JmpBuf.rip
            &&  !pVM->vmm.s.CallHostR0JmpBuf.fInRing3Call)
#endif
        {
            int rc = VMMR0CallHost(pVM, VMMCALLHOST_VM_R0_ASSERTION, 0);
            return RT_FAILURE_NP(rc);
        }
    }
#ifdef RT_OS_LINUX
    return true;
#else
    return false;
#endif
#endif
}


/**
 * Override this so we can push it up to ring-3.
 *
 * @param   pszExpr     Expression. Can be NULL.
 * @param   uLine       Location line number.
 * @param   pszFile     Location file name.
 * @param   pszFunction Location function name.
 */
DECLEXPORT(void) RTCALL AssertMsg1(const char *pszExpr, unsigned uLine, const char *pszFile, const char *pszFunction)
{
#if !defined(DEBUG_sandervl) && !defined(RT_OS_DARWIN)
    SUPR0Printf("\n!!R0-Assertion Failed!!\n"
                "Expression: %s\n"
                "Location  : %s(%d) %s\n",
                pszExpr, pszFile, uLine, pszFunction);
#endif
    LogAlways(("\n!!R0-Assertion Failed!!\n"
               "Expression: %s\n"
               "Location  : %s(%d) %s\n",
               pszExpr, pszFile, uLine, pszFunction));

    PVM pVM = GVMMR0GetVMByEMT(NIL_RTNATIVETHREAD);
    if (pVM)
        RTStrPrintf(pVM->vmm.s.szRing0AssertMsg1, sizeof(pVM->vmm.s.szRing0AssertMsg1),
                    "\n!!R0-Assertion Failed!!\n"
                    "Expression: %s\n"
                    "Location  : %s(%d) %s\n",
                    pszExpr, pszFile, uLine, pszFunction);
#ifdef RT_OS_DARWIN
    RTAssertMsg1(pszExpr, uLine, pszFile, pszFunction);
#endif
}


/**
 * Callback for RTLogFormatV which writes to the ring-3 log port.
 * See PFNLOGOUTPUT() for details.
 */
static DECLCALLBACK(size_t) rtLogOutput(void *pv, const char *pachChars, size_t cbChars)
{
    for (size_t i = 0; i < cbChars; i++)
    {
#if !defined(DEBUG_sandervl) && !defined(RT_OS_DARWIN)
        SUPR0Printf("%c", pachChars[i]);
#endif
        LogAlways(("%c", pachChars[i]));
    }

    return cbChars;
}


DECLEXPORT(void) RTCALL AssertMsg2(const char *pszFormat, ...)
{
    va_list va;

    PRTLOGGER pLog = RTLogDefaultInstance(); /** @todo we want this for release as well! */
    if (pLog)
    {
        va_start(va, pszFormat);
        RTLogFormatV(rtLogOutput, pLog, pszFormat, va);
        va_end(va);

        PVM pVM = GVMMR0GetVMByEMT(NIL_RTNATIVETHREAD);
        if (pVM)
        {
            va_start(va, pszFormat);
            RTStrPrintfV(pVM->vmm.s.szRing0AssertMsg2, sizeof(pVM->vmm.s.szRing0AssertMsg2), pszFormat, va);
            va_end(va);
        }
    }

#ifdef RT_OS_DARWIN
    va_start(va, pszFormat);
    RTAssertMsg2V(pszFormat, va);
    va_end(va);
#endif
}

