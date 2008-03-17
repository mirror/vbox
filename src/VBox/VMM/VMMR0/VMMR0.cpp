/* $Id$ */
/** @file
 * VMM - Host Context Ring 0.
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
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_VMM
#include <VBox/vmm.h>
#include <VBox/sup.h>
#include <VBox/trpm.h>
#include <VBox/cpum.h>
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

#if defined(_MSC_VER) && defined(RT_ARCH_AMD64) /** @todo check this with with VC7! */
#  pragma intrinsic(_AddressOfReturnAddress)
#endif


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int VMMR0Init(PVM pVM, unsigned uVersion);
static int VMMR0Term(PVM pVM);
__BEGIN_DECLS
VMMR0DECL(int) ModuleInit(void);
VMMR0DECL(void) ModuleTerm(void);
__END_DECLS


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
#ifdef VBOX_WITH_INTERNAL_NETWORKING
/** Pointer to the internal networking service instance. */
PINTNET g_pIntNet = 0;
#endif

/*******************************************************************************
*   Local Variables                                                            *
*******************************************************************************/

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
     * Initialize the GVMM, GMM.& HWACCM
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
#ifdef VBOX_WITH_INTERNAL_NETWORKING
                LogFlow(("ModuleInit: g_pIntNet=%p\n", g_pIntNet));
                g_pIntNet = NULL;
                LogFlow(("ModuleInit: g_pIntNet=%p should be NULL now...\n", g_pIntNet));
                rc = INTNETR0Create(&g_pIntNet);
                if (VBOX_SUCCESS(rc))
                {
                    LogFlow(("ModuleInit: returns success. g_pIntNet=%p\n", g_pIntNet));
                    return VINF_SUCCESS;
                }
                g_pIntNet = NULL;
                LogFlow(("ModuleTerm: returns %Vrc\n", rc));
#else
                LogFlow(("ModuleInit: returns success.\n"));
                return VINF_SUCCESS;
#endif
            }
        }
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

#ifdef VBOX_WITH_INTERNAL_NETWORKING
    /*
     * Destroy the internal networking instance.
     */
    if (g_pIntNet)
    {
        INTNETR0Destroy(g_pIntNet);
        g_pIntNet = NULL;
    }
#endif

    /* Global HWACCM cleanup */
    HWACCMR0Term();

    /*
     * Destroy the GMM and GVMM instances.
     */
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
 * @param   uVersion    The minimum module version required.
 * @thread  EMT.
 */
static int VMMR0Init(PVM pVM, unsigned uVersion)
{
    /*
     * Check if compatible version.
     */
    if (    uVersion != VBOX_VERSION
        &&  (   VBOX_GET_VERSION_MAJOR(uVersion) != VBOX_VERSION_MAJOR
             || VBOX_GET_VERSION_MINOR(uVersion) < VBOX_VERSION_MINOR))
        return VERR_VERSION_MISMATCH;
    if (    !VALID_PTR(pVM)
        ||  pVM->pVMR0 != pVM)
        return VERR_INVALID_PARAMETER;

    /*
     * Register the EMT R0 logger instance.
     */
    PVMMR0LOGGER pR0Logger = pVM->vmm.s.pR0Logger;
    if (pR0Logger)
    {
#if 0 /* testing of the logger. */
        LogCom(("VMMR0Init: before %p\n", RTLogDefaultInstance()));
        LogCom(("VMMR0Init: pfnFlush=%p actual=%p\n", pR0Logger->Logger.pfnFlush, vmmR0LoggerFlush));
        LogCom(("VMMR0Init: pfnLogger=%p actual=%p\n", pR0Logger->Logger.pfnLogger, vmmR0LoggerWrapper));
        LogCom(("VMMR0Init: offScratch=%d fFlags=%#x fDestFlags=%#x\n", pR0Logger->Logger.offScratch, pR0Logger->Logger.fFlags, pR0Logger->Logger.fDestFlags));

        RTLogSetDefaultInstanceThread(&pR0Logger->Logger, (uintptr_t)pVM->pSession);
        LogCom(("VMMR0Init: after %p reg\n", RTLogDefaultInstance()));
        RTLogSetDefaultInstanceThread(NULL, 0);
        LogCom(("VMMR0Init: after %p dereg\n", RTLogDefaultInstance()));

        pR0Logger->Logger.pfnLogger("hello ring-0 logger\n");
        LogCom(("VMMR0Init: returned succesfully from direct logger call.\n"));
        pR0Logger->Logger.pfnFlush(&pR0Logger->Logger);
        LogCom(("VMMR0Init: returned succesfully from direct flush call.\n"));

        RTLogSetDefaultInstanceThread(&pR0Logger->Logger, (uintptr_t)pVM->pSession);
        LogCom(("VMMR0Init: after %p reg2\n", RTLogDefaultInstance()));
        pR0Logger->Logger.pfnLogger("hello ring-0 logger\n");
        LogCom(("VMMR0Init: returned succesfully from direct logger call (2). offScratch=%d\n", pR0Logger->Logger.offScratch));
        RTLogSetDefaultInstanceThread(NULL, 0);
        LogCom(("VMMR0Init: after %p dereg2\n", RTLogDefaultInstance()));

        RTLogLoggerEx(&pR0Logger->Logger, 0, ~0U, "hello ring-0 logger (RTLogLoggerEx)\n");
        LogCom(("VMMR0Init: RTLogLoggerEx returned fine offScratch=%d\n", pR0Logger->Logger.offScratch));

        RTLogSetDefaultInstanceThread(&pR0Logger->Logger, (uintptr_t)pVM->pSession);
        RTLogPrintf("hello ring-0 logger (RTLogPrintf)\n");
        LogCom(("VMMR0Init: RTLogPrintf returned fine offScratch=%d\n", pR0Logger->Logger.offScratch));
#endif
        Log(("Switching to per-thread logging instance %p (key=%p)\n", &pR0Logger->Logger, pVM->pSession));
        RTLogSetDefaultInstanceThread(&pR0Logger->Logger, (uintptr_t)pVM->pSession);
    }

    /*
     * nitalize the per VM data for GVMM and GMM.
     */
    int rc = GVMMR0InitVM(pVM);
//    if (RT_SUCCESS(rc))
//        rc = GMMR0InitPerVMData(pVM);
    if (RT_SUCCESS(rc))
    {
        /*
         * Init HWACCM.
         */
        rc = HWACCMR0InitVM(pVM);
        if (RT_SUCCESS(rc))
        {
            /*
             * Init CPUM.
             */
            rc = CPUMR0Init(pVM);
            if (RT_SUCCESS(rc))
                return rc;
        }
    }

    /* failed */
    RTLogSetDefaultInstanceThread(NULL, 0);
    return rc;
}


/**
 * Terminates the R0 driver for a particular VM instance.
 *
 * @returns VBox status code.
 *
 * @param   pVM         The VM instance in question.
 * @thread  EMT.
 */
static int VMMR0Term(PVM pVM)
{
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
            STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetNormal);
            break;
        case VINF_EM_RAW_INTERRUPT:
            STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetInterrupt);
            break;
        case VINF_EM_RAW_INTERRUPT_HYPER:
            STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetInterruptHyper);
            break;
        case VINF_EM_RAW_GUEST_TRAP:
            STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetGuestTrap);
            break;
        case VINF_EM_RAW_RING_SWITCH:
            STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetRingSwitch);
            break;
        case VINF_EM_RAW_RING_SWITCH_INT:
            STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetRingSwitchInt);
            break;
        case VINF_EM_RAW_EXCEPTION_PRIVILEGED:
            STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetExceptionPrivilege);
            break;
        case VINF_EM_RAW_STALE_SELECTOR:
            STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetStaleSelector);
            break;
        case VINF_EM_RAW_IRET_TRAP:
            STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetIRETTrap);
            break;
        case VINF_IOM_HC_IOPORT_READ:
            STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetIORead);
            break;
        case VINF_IOM_HC_IOPORT_WRITE:
            STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetIOWrite);
            break;
        case VINF_IOM_HC_MMIO_READ:
            STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetMMIORead);
            break;
        case VINF_IOM_HC_MMIO_WRITE:
            STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetMMIOWrite);
            break;
        case VINF_IOM_HC_MMIO_READ_WRITE:
            STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetMMIOReadWrite);
            break;
        case VINF_PATM_HC_MMIO_PATCH_READ:
            STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetMMIOPatchRead);
            break;
        case VINF_PATM_HC_MMIO_PATCH_WRITE:
            STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetMMIOPatchWrite);
            break;
        case VINF_EM_RAW_EMULATE_INSTR:
            STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetEmulate);
            break;
        case VINF_PATCH_EMULATE_INSTR:
            STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetPatchEmulate);
            break;
        case VINF_EM_RAW_EMULATE_INSTR_LDT_FAULT:
            STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetLDTFault);
            break;
        case VINF_EM_RAW_EMULATE_INSTR_GDT_FAULT:
            STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetGDTFault);
            break;
        case VINF_EM_RAW_EMULATE_INSTR_IDT_FAULT:
            STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetIDTFault);
            break;
        case VINF_EM_RAW_EMULATE_INSTR_TSS_FAULT:
            STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetTSSFault);
            break;
        case VINF_EM_RAW_EMULATE_INSTR_PD_FAULT:
            STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetPDFault);
            break;
        case VINF_CSAM_PENDING_ACTION:
            STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetCSAMTask);
            break;
        case VINF_PGM_SYNC_CR3:
            STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetSyncCR3);
            break;
        case VINF_PATM_PATCH_INT3:
            STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetPatchInt3);
            break;
        case VINF_PATM_PATCH_TRAP_PF:
            STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetPatchPF);
            break;
        case VINF_PATM_PATCH_TRAP_GP:
            STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetPatchGP);
            break;
        case VINF_PATM_PENDING_IRQ_AFTER_IRET:
            STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetPatchIretIRQ);
            break;
        case VERR_REM_FLUSHED_PAGES_OVERFLOW:
            STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetPageOverflow);
            break;
        case VINF_EM_RESCHEDULE_REM:
            STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetRescheduleREM);
            break;
        case VINF_EM_RAW_TO_R3:
            STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetToR3);
            break;
        case VINF_EM_RAW_TIMER_PENDING:
            STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetTimerPending);
            break;
        case VINF_EM_RAW_INTERRUPT_PENDING:
            STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetInterruptPending);
            break;
        case VINF_VMM_CALL_HOST:
            switch (pVM->vmm.s.enmCallHostOperation)
            {
                case VMMCALLHOST_PDM_LOCK:
                    STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetPDMLock);
                    break;
                case VMMCALLHOST_PDM_QUEUE_FLUSH:
                    STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetPDMQueueFlush);
                    break;
                case VMMCALLHOST_PGM_POOL_GROW:
                    STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetPGMPoolGrow);
                    break;
                case VMMCALLHOST_PGM_LOCK:
                    STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetPGMLock);
                    break;
                case VMMCALLHOST_REM_REPLAY_HANDLER_NOTIFICATIONS:
                    STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetRemReplay);
                    break;
                case VMMCALLHOST_PGM_RAM_GROW_RANGE:
                    STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetPGMGrowRAM);
                    break;
                case VMMCALLHOST_VMM_LOGGER_FLUSH:
                    STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetLogFlush);
                    break;
                case VMMCALLHOST_VM_SET_ERROR:
                    STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetVMSetError);
                    break;
                case VMMCALLHOST_VM_SET_RUNTIME_ERROR:
                    STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetVMSetRuntimeError);
                    break;
                default:
                    STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetCallHost);
                    break;
            }
            break;
        case VINF_PATM_DUPLICATE_FUNCTION:
            STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetPATMDuplicateFn);
            break;
        case VINF_PGM_CHANGE_MODE:
            STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetPGMChangeMode);
            break;
        case VINF_EM_RAW_EMULATE_INSTR_HLT:
            STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetEmulHlt);
            break;
        case VINF_EM_PENDING_REQUEST:
            STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetPendingRequest);
            break;
        default:
            STAM_COUNTER_INC(&pVM->vmm.s.StatGCRetMisc);
            break;
    }
}
#endif /* VBOX_WITH_STATISTICS */



/**
 * The Ring 0 entry point, called by the interrupt gate.
 *
 * @returns VBox status code.
 * @param   pVM             The VM to operate on.
 * @param   enmOperation    Which operation to execute.
 * @param   pvArg           Argument to the operation.
 * @remarks Assume called with interrupts disabled.
 */
VMMR0DECL(int) VMMR0EntryInt(PVM pVM, VMMR0OPERATION enmOperation, void *pvArg)
{
    switch (enmOperation)
    {
#ifdef VBOX_WITH_IDT_PATCHING
        /*
         * Switch to GC.
         * These calls return whatever the GC returns.
         */
        case VMMR0_DO_RAW_RUN:
        {
            /* Safety precaution as VMX disables the switcher. */
            Assert(!pVM->vmm.s.fSwitcherDisabled);
            if (pVM->vmm.s.fSwitcherDisabled)
                return VERR_NOT_SUPPORTED;

            STAM_COUNTER_INC(&pVM->vmm.s.StatRunGC);
            register int rc;
            pVM->vmm.s.iLastGCRc = rc = pVM->vmm.s.pfnR0HostToGuest(pVM);

#ifdef VBOX_WITH_STATISTICS
            vmmR0RecordRC(pVM, rc);
#endif

            /*
             * We'll let TRPM change the stack frame so our return is different.
             * Just keep in mind that after the call, things have changed!
             */
            if (    rc == VINF_EM_RAW_INTERRUPT
                ||  rc == VINF_EM_RAW_INTERRUPT_HYPER)
            {
                /*
                 * Don't trust the compiler to get this right.
                 * gcc -fomit-frame-pointer screws up big time here. This works fine in 64-bit
                 * mode too because we push the arguments on the stack in the IDT patch code.
                 */
# if defined(__GNUC__)
                void *pvRet = (uint8_t *)__builtin_frame_address(0) + sizeof(void *);
# elif defined(_MSC_VER) && defined(RT_ARCH_AMD64) /** @todo check this with with VC7! */
                void *pvRet = (uint8_t *)_AddressOfReturnAddress();
# elif defined(RT_ARCH_X86)
                void *pvRet = (uint8_t *)&pVM - sizeof(pVM);
# else
#  error "huh?"
# endif
                if (    ((uintptr_t *)pvRet)[1] == (uintptr_t)pVM
                    &&  ((uintptr_t *)pvRet)[2] == (uintptr_t)enmOperation
                    &&  ((uintptr_t *)pvRet)[3] == (uintptr_t)pvArg)
                    TRPMR0SetupInterruptDispatcherFrame(pVM, pvRet);
                else
                {
# if defined(DEBUG) || defined(LOG_ENABLED)
                    static bool  s_fHaveWarned = false;
                    if (!s_fHaveWarned)
                    {
                         s_fHaveWarned = true;
                         RTLogPrintf("VMMR0.r0: The compiler can't find the stack frame!\n");
                         RTLogComPrintf("VMMR0.r0: The compiler can't find the stack frame!\n");
                    }
# endif
                    TRPMR0DispatchHostInterrupt(pVM);
                }
            }
            return rc;
        }

        /*
         * Switch to GC to execute Hypervisor function.
         */
        case VMMR0_DO_CALL_HYPERVISOR:
        {
            /* Safety precaution as VMX disables the switcher. */
            Assert(!pVM->vmm.s.fSwitcherDisabled);
            if (pVM->vmm.s.fSwitcherDisabled)
                return VERR_NOT_SUPPORTED;

            RTCCUINTREG fFlags = ASMIntDisableFlags();
            int rc = pVM->vmm.s.pfnR0HostToGuest(pVM);
            /** @todo dispatch interrupts? */
            ASMSetFlags(fFlags);
            return rc;
        }

        /*
         * For profiling.
         */
        case VMMR0_DO_NOP:
            return VINF_SUCCESS;
#endif /* VBOX_WITH_IDT_PATCHING */

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
 * The Ring 0 entry point, called by the fast-ioctl path.
 *
 * @returns VBox status code.
 * @param   pVM             The VM to operate on.
 * @param   enmOperation    Which operation to execute.
 * @remarks Assume called with interrupts _enabled_.
 */
VMMR0DECL(int) VMMR0EntryFast(PVM pVM, VMMR0OPERATION enmOperation)
{
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

                int rc = pVM->vmm.s.pfnR0HostToGuest(pVM);
                pVM->vmm.s.iLastGCRc = rc;

                if (    rc == VINF_EM_RAW_INTERRUPT
                    ||  rc == VINF_EM_RAW_INTERRUPT_HYPER)
                    TRPMR0DispatchHostInterrupt(pVM);

                ASMSetFlags(uFlags);

#ifdef VBOX_WITH_STATISTICS
                STAM_COUNTER_INC(&pVM->vmm.s.StatRunGC);
                vmmR0RecordRC(pVM, rc);
#endif
                return rc;
            }

            Assert(!pVM->vmm.s.fSwitcherDisabled);
            return VERR_NOT_SUPPORTED;
        }

        /*
         * Run guest code using the available hardware acceleration technology.
         *
         * Disable interrupts before we do anything interesting. On Windows we avoid
         * this by having the support driver raise the IRQL before calling us, this way
         * we hope to get away we page faults and later calling into the kernel.
         */
        case VMMR0_DO_HWACC_RUN:
        {
            STAM_COUNTER_INC(&pVM->vmm.s.StatRunGC);

#ifndef RT_OS_WINDOWS /** @todo check other hosts */
            RTCCUINTREG uFlags = ASMIntDisableFlags();
#endif
            int rc = HWACCMR0Enter(pVM);
            if (VBOX_SUCCESS(rc))
            {
                rc = vmmR0CallHostSetJmp(&pVM->vmm.s.CallHostR0JmpBuf, HWACCMR0RunGuestCode, pVM); /* this may resume code. */
                int rc2 = HWACCMR0Leave(pVM);
                AssertRC(rc2);
            }
            pVM->vmm.s.iLastGCRc = rc;
#ifndef RT_OS_WINDOWS /** @todo check other hosts */
            ASMSetFlags(uFlags);
#endif

#ifdef VBOX_WITH_STATISTICS
            vmmR0RecordRC(pVM, rc);
#endif
            /* No special action required for external interrupts, just return. */
            return rc;
        }

        /*
         * For profiling.
         */
        case VMMR0_DO_NOP:
            return VINF_SUCCESS;

        /*
         * Impossible.
         */
        default:
            AssertMsgFailed(("%#x\n", enmOperation));
            return VERR_NOT_SUPPORTED;
    }
}


/**
 * VMMR0EntryEx worker function, either called directly or when ever possible
 * called thru a longjmp so we can exit safely on failure.
 *
 * @returns VBox status code.
 * @param   pVM             The VM to operate on.
 * @param   enmOperation    Which operation to execute.
 * @param   pReqHdr         This points to a SUPVMMR0REQHDR packet. Optional.
 * @param   u64Arg          Some simple constant argument.
 * @remarks Assume called with interrupts _enabled_.
 */
static int vmmR0EntryExWorker(PVM pVM, VMMR0OPERATION enmOperation, PSUPVMMR0REQHDR pReqHdr, uint64_t u64Arg)
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
    }

    switch (enmOperation)
    {
        /*
         * GVM requests
         */
        case VMMR0_DO_GVMM_CREATE_VM:
            if (pVM || u64Arg)
                return VERR_INVALID_PARAMETER;
            SUPR0Printf("-> GVMMR0CreateVMReq\n");
            return GVMMR0CreateVMReq((PGVMMCREATEVMREQ)pReqHdr);

        case VMMR0_DO_GVMM_DESTROY_VM:
            if (pReqHdr || u64Arg)
                return VERR_INVALID_PARAMETER;
            return GVMMR0DestroyVM(pVM);

        case VMMR0_DO_GVMM_SCHED_HALT:
            if (pReqHdr)
                return VERR_INVALID_PARAMETER;
            return GVMMR0SchedHalt(pVM, u64Arg);

        case VMMR0_DO_GVMM_SCHED_WAKE_UP:
            if (pReqHdr || u64Arg)
                return VERR_INVALID_PARAMETER;
            return GVMMR0SchedWakeUp(pVM);

        case VMMR0_DO_GVMM_SCHED_POLL:
            if (pReqHdr || u64Arg > 1)
                return VERR_INVALID_PARAMETER;
            return GVMMR0SchedPoll(pVM, (bool)u64Arg);

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
            return VMMR0Init(pVM, (unsigned)u64Arg);

        /*
         * Terminate the R0 part of a VM instance.
         */
        case VMMR0_DO_VMMR0_TERM:
            return VMMR0Term(pVM);

        /*
         * Attempt to enable hwacc mode and check the current setting.
         *
         */
        case VMMR0_DO_HWACC_ENABLE:
            return HWACCMR0EnableAllCpus(pVM, (HWACCMSTATE)u64Arg);

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
            /* Safety precaution as HWACCM can disable the switcher. */
            Assert(!pVM->vmm.s.fSwitcherDisabled);
            if (RT_UNLIKELY(pVM->vmm.s.fSwitcherDisabled))
                return VERR_NOT_SUPPORTED;

            RTCCUINTREG fFlags = ASMIntDisableFlags();
            int rc = pVM->vmm.s.pfnR0HostToGuest(pVM);
            /** @todo dispatch interrupts? */
            ASMSetFlags(fFlags);
            return rc;
        }

        /*
         * PGM wrappers.
         */
        case VMMR0_DO_PGM_ALLOCATE_HANDY_PAGES:
            return PGMR0PhysAllocateHandyPages(pVM);

        /*
         * GMM wrappers.
         */
        case VMMR0_DO_GMM_INITIAL_RESERVATION:
            if (u64Arg)
                return VERR_INVALID_PARAMETER;
            return GMMR0InitialReservationReq(pVM, (PGMMINITIALRESERVATIONREQ)pReqHdr);
        case VMMR0_DO_GMM_UPDATE_RESERVATION:
            if (u64Arg)
                return VERR_INVALID_PARAMETER;
            return GMMR0UpdateReservationReq(pVM, (PGMMUPDATERESERVATIONREQ)pReqHdr);

        case VMMR0_DO_GMM_ALLOCATE_PAGES:
            if (u64Arg)
                return VERR_INVALID_PARAMETER;
            return GMMR0AllocatePagesReq(pVM, (PGMMALLOCATEPAGESREQ)pReqHdr);
        case VMMR0_DO_GMM_FREE_PAGES:
            if (u64Arg)
                return VERR_INVALID_PARAMETER;
            return GMMR0FreePagesReq(pVM, (PGMMFREEPAGESREQ)pReqHdr);
        case VMMR0_DO_GMM_BALLOONED_PAGES:
            if (u64Arg)
                return VERR_INVALID_PARAMETER;
            return GMMR0BalloonedPagesReq(pVM, (PGMMBALLOONEDPAGESREQ)pReqHdr);
        case VMMR0_DO_GMM_DEFLATED_BALLOON:
            if (pReqHdr)
                return VERR_INVALID_PARAMETER;
            return GMMR0DeflatedBalloon(pVM, (uint32_t)u64Arg);

        case VMMR0_DO_GMM_MAP_UNMAP_CHUNK:
            if (u64Arg)
                return VERR_INVALID_PARAMETER;
            return GMMR0MapUnmapChunkReq(pVM, (PGMMMAPUNMAPCHUNKREQ)pReqHdr);
        case VMMR0_DO_GMM_SEED_CHUNK:
            if (pReqHdr)
                return VERR_INVALID_PARAMETER;
            return GMMR0SeedChunk(pVM, (RTR3PTR)u64Arg);

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


#ifdef VBOX_WITH_INTERNAL_NETWORKING
        /*
         * Requests to the internal networking service.
         */
        case VMMR0_DO_INTNET_OPEN:
            if (!pVM || u64Arg)
                return VERR_INVALID_PARAMETER;
            if (!g_pIntNet)
                return VERR_NOT_SUPPORTED;
            return INTNETR0OpenReq(g_pIntNet, pVM->pSession, (PINTNETOPENREQ)pReqHdr);

        case VMMR0_DO_INTNET_IF_CLOSE:
            if (!pVM || u64Arg)
                return VERR_INVALID_PARAMETER;
            if (!g_pIntNet)
                return VERR_NOT_SUPPORTED;
            return INTNETR0IfCloseReq(g_pIntNet, (PINTNETIFCLOSEREQ)pReqHdr);

        case VMMR0_DO_INTNET_IF_GET_RING3_BUFFER:
            if (!pVM || u64Arg)
                return VERR_INVALID_PARAMETER;
            if (!g_pIntNet)
                return VERR_NOT_SUPPORTED;
            return INTNETR0IfGetRing3BufferReq(g_pIntNet, (PINTNETIFGETRING3BUFFERREQ)pReqHdr);

        case VMMR0_DO_INTNET_IF_SET_PROMISCUOUS_MODE:
            if (!pVM || u64Arg)
                return VERR_INVALID_PARAMETER;
            if (!g_pIntNet)
                return VERR_NOT_SUPPORTED;
            return INTNETR0IfSetPromiscuousModeReq(g_pIntNet, (PINTNETIFSETPROMISCUOUSMODEREQ)pReqHdr);

        case VMMR0_DO_INTNET_IF_SEND:
            if (!pVM || u64Arg)
                return VERR_INVALID_PARAMETER;
            if (!g_pIntNet)
                return VERR_NOT_SUPPORTED;
            return INTNETR0IfSendReq(g_pIntNet, (PINTNETIFSENDREQ)pReqHdr);

        case VMMR0_DO_INTNET_IF_WAIT:
            if (!pVM || u64Arg)
                return VERR_INVALID_PARAMETER;
            if (!g_pIntNet)
                return VERR_NOT_SUPPORTED;
            return INTNETR0IfWaitReq(g_pIntNet, (PINTNETIFWAITREQ)pReqHdr);
#endif /* VBOX_WITH_INTERNAL_NETWORKING */

        /*
         * For profiling.
         */
        case VMMR0_DO_NOP:
            return VINF_SUCCESS;

        /*
         * For testing Ring-0 APIs invoked in this environment.
         */
        case VMMR0_DO_TESTS:
            /** @todo make new test */
            return VINF_SUCCESS;


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
 * Argument for vmmR0EntryExWrapper containing the argument s ofr VMMR0EntryEx.
 */
typedef struct VMMR0ENTRYEXARGS
{
    PVM                 pVM;
    VMMR0OPERATION      enmOperation;
    PSUPVMMR0REQHDR     pReq;
    uint64_t            u64Arg;
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
                              ((PVMMR0ENTRYEXARGS)pvArgs)->enmOperation,
                              ((PVMMR0ENTRYEXARGS)pvArgs)->pReq,
                              ((PVMMR0ENTRYEXARGS)pvArgs)->u64Arg);
}


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
VMMR0DECL(int) VMMR0EntryEx(PVM pVM, VMMR0OPERATION enmOperation, PSUPVMMR0REQHDR pReq, uint64_t u64Arg)
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
            case VMMR0_DO_VMMR0_INIT:
            case VMMR0_DO_VMMR0_TERM:
            case VMMR0_DO_GMM_INITIAL_RESERVATION:
            case VMMR0_DO_GMM_UPDATE_RESERVATION:
            case VMMR0_DO_GMM_ALLOCATE_PAGES:
            case VMMR0_DO_GMM_FREE_PAGES:
            case VMMR0_DO_GMM_BALLOONED_PAGES:
            case VMMR0_DO_GMM_DEFLATED_BALLOON:
            case VMMR0_DO_GMM_MAP_UNMAP_CHUNK:
            case VMMR0_DO_GMM_SEED_CHUNK:
            {
                /** @todo validate this EMT claim... GVM knows. */
                VMMR0ENTRYEXARGS Args;
                Args.pVM = pVM;
                Args.enmOperation = enmOperation;
                Args.pReq = pReq;
                Args.u64Arg = u64Arg;
                return vmmR0CallHostSetJmpEx(&pVM->vmm.s.CallHostR0JmpBuf, vmmR0EntryExWrapper, &Args);
            }

            default:
                break;
        }
    }
    return vmmR0EntryExWorker(pVM, enmOperation, pReq, u64Arg);
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
        ||  !VALID_PTR(pLogger)
        ||  pLogger->u32Magic != RTLOGGER_MAGIC)
    {
        LogCom(("vmmR0LoggerFlush: pLogger=%p!\n", pLogger));
        return;
    }

    PVM pVM = pR0Logger->pVM;
    if (    !VALID_PTR(pVM)
        ||  pVM->pVMR0 != pVM)
    {
        LogCom(("vmmR0LoggerFlush: pVM=%p! pLogger=%p\n", pVM, pLogger));
        return;
    }

    /*
     * Check that the jump buffer is armed.
     */
#ifdef RT_ARCH_X86
    if (!pVM->vmm.s.CallHostR0JmpBuf.eip)
#else
    if (!pVM->vmm.s.CallHostR0JmpBuf.rip)
#endif
    {
        LogCom(("vmmR0LoggerFlush: Jump buffer isn't armed!\n"));
        pLogger->offScratch = 0;
        return;
    }

    VMMR0CallHost(pVM, VMMCALLHOST_VMM_LOGGER_FLUSH, 0);
}



/**
 * Jump back to ring-3 if we're the EMT and the longjmp is armed.
 *
 * @returns true if the breakpoint should be hit, false if it should be ignored.
 * @remark  The RTDECL() makes this a bit difficult to override on windows. Sorry.
 */
DECLEXPORT(bool) RTCALL RTAssertDoBreakpoint(void)
{
    PVM pVM = GVMMR0GetVMByEMT(NIL_RTNATIVETHREAD);
    if (pVM)
    {
#ifdef RT_ARCH_X86
        if (pVM->vmm.s.CallHostR0JmpBuf.eip)
#else
        if (pVM->vmm.s.CallHostR0JmpBuf.rip)
#endif
        {
            int rc = VMMR0CallHost(pVM, VMMCALLHOST_VM_R0_HYPER_ASSERTION, 0);
            return RT_FAILURE_NP(rc);
        }
    }
#ifdef RT_OS_LINUX
    return true;
#else
    return false;
#endif
}



# undef LOG_GROUP
# define LOG_GROUP LOG_GROUP_EM

/**
 * Override this so we can push
 *
 * @param   pszExpr     Expression. Can be NULL.
 * @param   uLine       Location line number.
 * @param   pszFile     Location file name.
 * @param   pszFunction Location function name.
 * @remark  This API exists in HC Ring-3 and GC.
 */
DECLEXPORT(void) RTCALL AssertMsg1(const char *pszExpr, unsigned uLine, const char *pszFile, const char *pszFunction)
{
    SUPR0Printf("\n!!R0-Assertion Failed!!\n"
                "Expression: %s\n"
                "Location  : %s(%d) %s\n",
                pszExpr, pszFile, uLine, pszFunction);

    LogRel(("\n!!R0-Assertion Failed!!\n"
            "Expression: %s\n"
            "Location  : %s(%d) %s\n",
            pszExpr, pszFile, uLine, pszFunction));
}


/**
 * Callback for RTLogFormatV which writes to the com port.
 * See PFNLOGOUTPUT() for details.
 */
static DECLCALLBACK(size_t) rtLogOutput(void *pv, const char *pachChars, size_t cbChars)
{
    for (size_t i = 0; i < cbChars; i++)
    {
        LogRel(("%c", pachChars[i])); /** @todo this isn't any release logging in ring-0 from what I can tell... */
        SUPR0Printf("%c", pachChars[i]);
    }

    return cbChars;
}


DECLEXPORT(void) RTCALL AssertMsg2(const char *pszFormat, ...)
{
    PRTLOGGER pLog = RTLogDefaultInstance();
    if (pLog)
    {
        va_list args;

        va_start(args, pszFormat);
        RTLogFormatV(rtLogOutput, pLog, pszFormat, args);
        va_end(args);
    }
}


