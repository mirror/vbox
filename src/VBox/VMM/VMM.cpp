/* $Id$ */
/** @file
 * VMM - The Virtual Machine Monitor Core.
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

//#define NO_SUPCALLR0VMM

/** @page pg_vmm        VMM - The Virtual Machine Monitor
 *
 * The VMM component is two things at the moment, it's a component doing a few
 * management and routing tasks, and it's the whole virtual machine monitor
 * thing.  For hysterical reasons, it is not doing all the management that one
 * would expect, this is instead done by @ref pg_vm.  We'll address this
 * misdesign eventually.
 *
 * @see grp_vmm, grp_vm
 *
 *
 * @section sec_vmmstate        VMM State
 *
 * @image html VM_Statechart_Diagram.gif
 *
 * To be written.
 *
 *
 * @subsection  subsec_vmm_init     VMM Initialization
 *
 * To be written.
 *
 *
 * @subsection  subsec_vmm_term     VMM Termination
 *
 * To be written.
 *
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_VMM
#include <VBox/vmm.h>
#include <VBox/vmapi.h>
#include <VBox/pgm.h>
#include <VBox/cfgm.h>
#include <VBox/pdmqueue.h>
#include <VBox/pdmcritsect.h>
#include <VBox/pdmapi.h>
#include <VBox/cpum.h>
#include <VBox/mm.h>
#include <VBox/iom.h>
#include <VBox/trpm.h>
#include <VBox/selm.h>
#include <VBox/em.h>
#include <VBox/sup.h>
#include <VBox/dbgf.h>
#include <VBox/csam.h>
#include <VBox/patm.h>
#include <VBox/rem.h>
#include <VBox/ssm.h>
#include <VBox/tm.h>
#include "VMMInternal.h"
#include "VMMSwitcher/VMMSwitcher.h"
#include <VBox/vm.h>

#include <VBox/err.h>
#include <VBox/param.h>
#include <VBox/version.h>
#include <VBox/x86.h>
#include <VBox/hwaccm.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/asm.h>
#include <iprt/time.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/stdarg.h>
#include <iprt/ctype.h>



/** The saved state version. */
#define VMM_SAVED_STATE_VERSION     3


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int                  vmmR3InitStacks(PVM pVM);
static int                  vmmR3InitLoggers(PVM pVM);
static void                 vmmR3InitRegisterStats(PVM pVM);
static DECLCALLBACK(int)    vmmR3Save(PVM pVM, PSSMHANDLE pSSM);
static DECLCALLBACK(int)    vmmR3Load(PVM pVM, PSSMHANDLE pSSM, uint32_t u32Version);
static DECLCALLBACK(void)   vmmR3YieldEMT(PVM pVM, PTMTIMER pTimer, void *pvUser);
static int                  vmmR3ServiceCallHostRequest(PVM pVM, PVMCPU pVCpu);
static DECLCALLBACK(void)   vmmR3InfoFF(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);


/**
 * Initializes the VMM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(int) VMMR3Init(PVM pVM)
{
    LogFlow(("VMMR3Init\n"));

    /*
     * Assert alignment, sizes and order.
     */
    AssertMsg(pVM->vmm.s.offVM == 0, ("Already initialized!\n"));
    AssertCompile(sizeof(pVM->vmm.s) <= sizeof(pVM->vmm.padding));
    AssertCompile(sizeof(pVM->aCpus[0].vmm.s) <= sizeof(pVM->aCpus[0].vmm.padding));

    /*
     * Init basic VM VMM members.
     */
    pVM->vmm.s.offVM = RT_OFFSETOF(VM, vmm);
    int rc = CFGMR3QueryU32(CFGMR3GetRoot(pVM), "YieldEMTInterval", &pVM->vmm.s.cYieldEveryMillies);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pVM->vmm.s.cYieldEveryMillies = 23; /* Value arrived at after experimenting with the grub boot prompt. */
        //pVM->vmm.s.cYieldEveryMillies = 8; //debugging
    else
        AssertMsgRCReturn(rc, ("Configuration error. Failed to query \"YieldEMTInterval\", rc=%Rrc\n", rc), rc);

    /*
     * Initialize the VMM sync critical section.
     */
    rc = RTCritSectInit(&pVM->vmm.s.CritSectSync);
    AssertRCReturn(rc, rc);

    /* GC switchers are enabled by default. Turned off by HWACCM. */
    pVM->vmm.s.fSwitcherDisabled = false;

    /*
     * Register the saved state data unit.
     */
    rc = SSMR3RegisterInternal(pVM, "vmm", 1, VMM_SAVED_STATE_VERSION, VMM_STACK_SIZE + sizeof(RTGCPTR),
                               NULL, vmmR3Save, NULL,
                               NULL, vmmR3Load, NULL);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Register the Ring-0 VM handle with the session for fast ioctl calls.
     */
    rc = SUPSetVMForFastIOCtl(pVM->pVMR0);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Init various sub-components.
     */
    rc = vmmR3SwitcherInit(pVM);
    if (RT_SUCCESS(rc))
    {
        rc = vmmR3InitStacks(pVM);
        if (RT_SUCCESS(rc))
        {
            rc = vmmR3InitLoggers(pVM);

#ifdef VBOX_WITH_NMI
            /*
             * Allocate mapping for the host APIC.
             */
            if (RT_SUCCESS(rc))
            {
                rc = MMR3HyperReserve(pVM, PAGE_SIZE, "Host APIC", &pVM->vmm.s.GCPtrApicBase);
                AssertRC(rc);
            }
#endif
            if (RT_SUCCESS(rc))
            {
                /*
                 * Debug info and statistics.
                 */
                DBGFR3InfoRegisterInternal(pVM, "ff", "Displays the current Forced actions Flags.", vmmR3InfoFF);
                vmmR3InitRegisterStats(pVM);

                return VINF_SUCCESS;
            }
        }
        /** @todo: Need failure cleanup. */

        //more todo in here?
        //if (RT_SUCCESS(rc))
        //{
        //}
        //int rc2 = vmmR3TermCoreCode(pVM);
        //AssertRC(rc2));
    }

    return rc;
}


/**
 * Allocate & setup the VMM RC stack(s) (for EMTs).
 *
 * The stacks are also used for long jumps in Ring-0.
 *
 * @returns VBox status code.
 * @param   pVM     Pointer to the shared VM structure.
 *
 * @remarks The optional guard page gets it protection setup up during R3 init
 *          completion because of init order issues.
 */
static int vmmR3InitStacks(PVM pVM)
{
    int rc = VINF_SUCCESS;

    for (VMCPUID idCpu = 0; idCpu < pVM->cCPUs; idCpu++)
    {
        PVMCPU pVCpu = &pVM->aCpus[idCpu];

#ifdef VBOX_STRICT_VMM_STACK
        rc = MMR3HyperAllocOnceNoRel(pVM, VMM_STACK_SIZE + PAGE_SIZE + PAGE_SIZE, PAGE_SIZE, MM_TAG_VMM, (void **)&pVCpu->vmm.s.pbEMTStackR3);
#else
        rc = MMR3HyperAllocOnceNoRel(pVM, VMM_STACK_SIZE, PAGE_SIZE, MM_TAG_VMM, (void **)&pVCpu->vmm.s.pbEMTStackR3);
#endif
        if (RT_SUCCESS(rc))
        {
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE
            /* MMHyperR3ToR0 returns R3 when not doing hardware assisted virtualization. */
            if (!VMMIsHwVirtExtForced(pVM))
                pVCpu->vmm.s.CallHostR0JmpBuf.pvSavedStack = NIL_RTR0PTR;
            else
#endif
                pVCpu->vmm.s.CallHostR0JmpBuf.pvSavedStack = MMHyperR3ToR0(pVM, pVCpu->vmm.s.pbEMTStackR3);
            pVCpu->vmm.s.pbEMTStackRC       = MMHyperR3ToRC(pVM, pVCpu->vmm.s.pbEMTStackR3);
            pVCpu->vmm.s.pbEMTStackBottomRC = pVCpu->vmm.s.pbEMTStackRC + VMM_STACK_SIZE;
            AssertRelease(pVCpu->vmm.s.pbEMTStackRC);

            CPUMSetHyperESP(pVCpu, pVCpu->vmm.s.pbEMTStackBottomRC);
        }
    }

    return rc;
}


/**
 * Initialize the loggers.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the shared VM structure.
 */
static int vmmR3InitLoggers(PVM pVM)
{
    int rc;

    /*
     * Allocate RC & R0 Logger instances (they are finalized in the relocator).
     */
#ifdef LOG_ENABLED
    PRTLOGGER pLogger = RTLogDefaultInstance();
    if (pLogger)
    {
        pVM->vmm.s.cbRCLogger = RT_OFFSETOF(RTLOGGERRC, afGroups[pLogger->cGroups]);
        rc = MMR3HyperAllocOnceNoRel(pVM, pVM->vmm.s.cbRCLogger, 0, MM_TAG_VMM, (void **)&pVM->vmm.s.pRCLoggerR3);
        if (RT_FAILURE(rc))
            return rc;
        pVM->vmm.s.pRCLoggerRC = MMHyperR3ToRC(pVM, pVM->vmm.s.pRCLoggerR3);

# ifdef VBOX_WITH_R0_LOGGING
        for (unsigned i = 0; i < pVM->cCPUs; i++)
        {
            PVMCPU pVCpu = &pVM->aCpus[i];

            rc = MMR3HyperAllocOnceNoRel(pVM, RT_OFFSETOF(VMMR0LOGGER, Logger.afGroups[pLogger->cGroups]),
                                     0, MM_TAG_VMM, (void **)&pVCpu->vmm.s.pR0LoggerR3);
            if (RT_FAILURE(rc))
                return rc;
            pVCpu->vmm.s.pR0LoggerR3->pVM = pVM->pVMR0;
            //pVCpu->vmm.s.pR0LoggerR3->fCreated = false;
            pVCpu->vmm.s.pR0LoggerR3->cbLogger = RT_OFFSETOF(RTLOGGER, afGroups[pLogger->cGroups]);
            pVCpu->vmm.s.pR0LoggerR0 = MMHyperR3ToR0(pVM, pVCpu->vmm.s.pR0LoggerR3);
        }
# endif
    }
#endif /* LOG_ENABLED */

#ifdef VBOX_WITH_RC_RELEASE_LOGGING
    /*
     * Allocate RC release logger instances (finalized in the relocator).
     */
    PRTLOGGER pRelLogger = RTLogRelDefaultInstance();
    if (pRelLogger)
    {
        pVM->vmm.s.cbRCRelLogger = RT_OFFSETOF(RTLOGGERRC, afGroups[pRelLogger->cGroups]);
        rc = MMR3HyperAllocOnceNoRel(pVM, pVM->vmm.s.cbRCRelLogger, 0, MM_TAG_VMM, (void **)&pVM->vmm.s.pRCRelLoggerR3);
        if (RT_FAILURE(rc))
            return rc;
        pVM->vmm.s.pRCRelLoggerRC = MMHyperR3ToRC(pVM, pVM->vmm.s.pRCRelLoggerR3);
    }
#endif /* VBOX_WITH_RC_RELEASE_LOGGING */
    return VINF_SUCCESS;
}


/**
 * VMMR3Init worker that register the statistics with STAM.
 *
 * @param   pVM         The shared VM structure.
 */
static void vmmR3InitRegisterStats(PVM pVM)
{
    /*
     * Statistics.
     */
    STAM_REG(pVM, &pVM->vmm.s.StatRunRC,                    STAMTYPE_COUNTER, "/VMM/RunRC",                     STAMUNIT_OCCURENCES, "Number of context switches.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZRetNormal,              STAMTYPE_COUNTER, "/VMM/RZRet/Normal",              STAMUNIT_OCCURENCES, "Number of VINF_SUCCESS returns.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZRetInterrupt,           STAMTYPE_COUNTER, "/VMM/RZRet/Interrupt",           STAMUNIT_OCCURENCES, "Number of VINF_EM_RAW_INTERRUPT returns.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZRetInterruptHyper,      STAMTYPE_COUNTER, "/VMM/RZRet/InterruptHyper",      STAMUNIT_OCCURENCES, "Number of VINF_EM_RAW_INTERRUPT_HYPER returns.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZRetGuestTrap,           STAMTYPE_COUNTER, "/VMM/RZRet/GuestTrap",           STAMUNIT_OCCURENCES, "Number of VINF_EM_RAW_GUEST_TRAP returns.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZRetRingSwitch,          STAMTYPE_COUNTER, "/VMM/RZRet/RingSwitch",          STAMUNIT_OCCURENCES, "Number of VINF_EM_RAW_RING_SWITCH returns.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZRetRingSwitchInt,       STAMTYPE_COUNTER, "/VMM/RZRet/RingSwitchInt",       STAMUNIT_OCCURENCES, "Number of VINF_EM_RAW_RING_SWITCH_INT returns.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZRetExceptionPrivilege,  STAMTYPE_COUNTER, "/VMM/RZRet/ExceptionPrivilege",  STAMUNIT_OCCURENCES, "Number of VINF_EM_RAW_EXCEPTION_PRIVILEGED returns.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZRetStaleSelector,       STAMTYPE_COUNTER, "/VMM/RZRet/StaleSelector",       STAMUNIT_OCCURENCES, "Number of VINF_EM_RAW_STALE_SELECTOR returns.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZRetIRETTrap,            STAMTYPE_COUNTER, "/VMM/RZRet/IRETTrap",            STAMUNIT_OCCURENCES, "Number of VINF_EM_RAW_IRET_TRAP returns.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZRetEmulate,             STAMTYPE_COUNTER, "/VMM/RZRet/Emulate",             STAMUNIT_OCCURENCES, "Number of VINF_EM_EXECUTE_INSTRUCTION returns.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZRetIOBlockEmulate,      STAMTYPE_COUNTER, "/VMM/RZRet/EmulateIOBlock",      STAMUNIT_OCCURENCES, "Number of VINF_EM_RAW_EMULATE_IO_BLOCK returns.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZRetPatchEmulate,        STAMTYPE_COUNTER, "/VMM/RZRet/PatchEmulate",        STAMUNIT_OCCURENCES, "Number of VINF_PATCH_EMULATE_INSTR returns.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZRetIORead,              STAMTYPE_COUNTER, "/VMM/RZRet/IORead",              STAMUNIT_OCCURENCES, "Number of VINF_IOM_HC_IOPORT_READ returns.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZRetIOWrite,             STAMTYPE_COUNTER, "/VMM/RZRet/IOWrite",             STAMUNIT_OCCURENCES, "Number of VINF_IOM_HC_IOPORT_WRITE returns.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZRetMMIORead,            STAMTYPE_COUNTER, "/VMM/RZRet/MMIORead",            STAMUNIT_OCCURENCES, "Number of VINF_IOM_HC_MMIO_READ returns.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZRetMMIOWrite,           STAMTYPE_COUNTER, "/VMM/RZRet/MMIOWrite",           STAMUNIT_OCCURENCES, "Number of VINF_IOM_HC_MMIO_WRITE returns.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZRetMMIOReadWrite,       STAMTYPE_COUNTER, "/VMM/RZRet/MMIOReadWrite",       STAMUNIT_OCCURENCES, "Number of VINF_IOM_HC_MMIO_READ_WRITE returns.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZRetMMIOPatchRead,       STAMTYPE_COUNTER, "/VMM/RZRet/MMIOPatchRead",       STAMUNIT_OCCURENCES, "Number of VINF_IOM_HC_MMIO_PATCH_READ returns.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZRetMMIOPatchWrite,      STAMTYPE_COUNTER, "/VMM/RZRet/MMIOPatchWrite",      STAMUNIT_OCCURENCES, "Number of VINF_IOM_HC_MMIO_PATCH_WRITE returns.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZRetLDTFault,            STAMTYPE_COUNTER, "/VMM/RZRet/LDTFault",            STAMUNIT_OCCURENCES, "Number of VINF_EM_EXECUTE_INSTRUCTION_GDT_FAULT returns.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZRetGDTFault,            STAMTYPE_COUNTER, "/VMM/RZRet/GDTFault",            STAMUNIT_OCCURENCES, "Number of VINF_EM_EXECUTE_INSTRUCTION_LDT_FAULT returns.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZRetIDTFault,            STAMTYPE_COUNTER, "/VMM/RZRet/IDTFault",            STAMUNIT_OCCURENCES, "Number of VINF_EM_EXECUTE_INSTRUCTION_IDT_FAULT returns.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZRetTSSFault,            STAMTYPE_COUNTER, "/VMM/RZRet/TSSFault",            STAMUNIT_OCCURENCES, "Number of VINF_EM_EXECUTE_INSTRUCTION_TSS_FAULT returns.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZRetPDFault,             STAMTYPE_COUNTER, "/VMM/RZRet/PDFault",             STAMUNIT_OCCURENCES, "Number of VINF_EM_EXECUTE_INSTRUCTION_PD_FAULT returns.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZRetCSAMTask,            STAMTYPE_COUNTER, "/VMM/RZRet/CSAMTask",            STAMUNIT_OCCURENCES, "Number of VINF_CSAM_PENDING_ACTION returns.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZRetSyncCR3,             STAMTYPE_COUNTER, "/VMM/RZRet/SyncCR",              STAMUNIT_OCCURENCES, "Number of VINF_PGM_SYNC_CR3 returns.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZRetMisc,                STAMTYPE_COUNTER, "/VMM/RZRet/Misc",                STAMUNIT_OCCURENCES, "Number of misc returns.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZRetPatchInt3,           STAMTYPE_COUNTER, "/VMM/RZRet/PatchInt3",           STAMUNIT_OCCURENCES, "Number of VINF_PATM_PATCH_INT3 returns.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZRetPatchPF,             STAMTYPE_COUNTER, "/VMM/RZRet/PatchPF",             STAMUNIT_OCCURENCES, "Number of VINF_PATM_PATCH_TRAP_PF returns.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZRetPatchGP,             STAMTYPE_COUNTER, "/VMM/RZRet/PatchGP",             STAMUNIT_OCCURENCES, "Number of VINF_PATM_PATCH_TRAP_GP returns.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZRetPatchIretIRQ,        STAMTYPE_COUNTER, "/VMM/RZRet/PatchIret",           STAMUNIT_OCCURENCES, "Number of VINF_PATM_PENDING_IRQ_AFTER_IRET returns.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZRetPageOverflow,        STAMTYPE_COUNTER, "/VMM/RZRet/InvlpgOverflow",      STAMUNIT_OCCURENCES, "Number of VERR_REM_FLUSHED_PAGES_OVERFLOW returns.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZRetRescheduleREM,       STAMTYPE_COUNTER, "/VMM/RZRet/ScheduleREM",         STAMUNIT_OCCURENCES, "Number of VINF_EM_RESCHEDULE_REM returns.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZRetToR3,                STAMTYPE_COUNTER, "/VMM/RZRet/ToR3",                STAMUNIT_OCCURENCES, "Number of VINF_EM_RAW_TO_R3 returns.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZRetTimerPending,        STAMTYPE_COUNTER, "/VMM/RZRet/TimerPending",        STAMUNIT_OCCURENCES, "Number of VINF_EM_RAW_TIMER_PENDING returns.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZRetInterruptPending,    STAMTYPE_COUNTER, "/VMM/RZRet/InterruptPending",    STAMUNIT_OCCURENCES, "Number of VINF_EM_RAW_INTERRUPT_PENDING returns.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZRetPATMDuplicateFn,     STAMTYPE_COUNTER, "/VMM/RZRet/PATMDuplicateFn",     STAMUNIT_OCCURENCES, "Number of VINF_PATM_DUPLICATE_FUNCTION returns.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZRetPGMChangeMode,       STAMTYPE_COUNTER, "/VMM/RZRet/PGMChangeMode",       STAMUNIT_OCCURENCES, "Number of VINF_PGM_CHANGE_MODE returns.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZRetEmulHlt,             STAMTYPE_COUNTER, "/VMM/RZRet/EmulHlt",             STAMUNIT_OCCURENCES, "Number of VINF_EM_RAW_EMULATE_INSTR_HLT returns.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZRetPendingRequest,      STAMTYPE_COUNTER, "/VMM/RZRet/PendingRequest",      STAMUNIT_OCCURENCES, "Number of VINF_EM_PENDING_REQUEST returns.");

    STAM_REG(pVM, &pVM->vmm.s.StatRZRetCallHost,            STAMTYPE_COUNTER, "/VMM/RZCallR3/Misc",             STAMUNIT_OCCURENCES, "Number of Other ring-3 calls.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZCallPDMLock,            STAMTYPE_COUNTER, "/VMM/RZCallR3/PDMLock",          STAMUNIT_OCCURENCES, "Number of VMMCALLHOST_PDM_LOCK calls.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZCallPDMQueueFlush,      STAMTYPE_COUNTER, "/VMM/RZCallR3/PDMQueueFlush",    STAMUNIT_OCCURENCES, "Number of VMMCALLHOST_PDM_QUEUE_FLUSH calls.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZCallPGMLock,            STAMTYPE_COUNTER, "/VMM/RZCallR3/PGMLock",          STAMUNIT_OCCURENCES, "Number of VMMCALLHOST_PGM_LOCK calls.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZCallPGMPoolGrow,        STAMTYPE_COUNTER, "/VMM/RZCallR3/PGMPoolGrow",      STAMUNIT_OCCURENCES, "Number of VMMCALLHOST_PGM_POOL_GROW calls.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZCallPGMMapChunk,        STAMTYPE_COUNTER, "/VMM/RZCallR3/PGMMapChunk",      STAMUNIT_OCCURENCES, "Number of VMMCALLHOST_PGM_MAP_CHUNK calls.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZCallPGMAllocHandy,      STAMTYPE_COUNTER, "/VMM/RZCallR3/PGMAllocHandy",    STAMUNIT_OCCURENCES, "Number of VMMCALLHOST_PGM_ALLOCATE_HANDY_PAGES calls.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZCallRemReplay,          STAMTYPE_COUNTER, "/VMM/RZCallR3/REMReplay",        STAMUNIT_OCCURENCES, "Number of VMMCALLHOST_REM_REPLAY_HANDLER_NOTIFICATIONS calls.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZCallLogFlush,           STAMTYPE_COUNTER, "/VMM/RZCallR3/VMMLogFlush",      STAMUNIT_OCCURENCES, "Number of VMMCALLHOST_VMM_LOGGER_FLUSH calls.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZCallVMSetError,         STAMTYPE_COUNTER, "/VMM/RZCallR3/VMSetError",       STAMUNIT_OCCURENCES, "Number of VMMCALLHOST_VM_SET_ERROR calls.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZCallVMSetRuntimeError,  STAMTYPE_COUNTER, "/VMM/RZCallR3/VMRuntimeError",   STAMUNIT_OCCURENCES, "Number of VMMCALLHOST_VM_SET_RUNTIME_ERROR calls.");
}


/**
 * Initializes the per-VCPU VMM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(int) VMMR3InitCPU(PVM pVM)
{
    LogFlow(("VMMR3InitCPU\n"));
    return VINF_SUCCESS;
}


/**
 * Ring-3 init finalizing.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 */
VMMR3DECL(int) VMMR3InitFinalize(PVM pVM)
{
    int rc = VINF_SUCCESS;

    for (VMCPUID idCpu = 0; idCpu < pVM->cCPUs; idCpu++)
    {
        PVMCPU pVCpu = &pVM->aCpus[idCpu];

#ifdef VBOX_STRICT_VMM_STACK
        /*
         * Two inaccessible pages at each sides of the stack to catch over/under-flows.
         */
        memset(pVCpu->vmm.s.pbEMTStackR3 - PAGE_SIZE, 0xcc, PAGE_SIZE);
        PGMMapSetPage(pVM, MMHyperR3ToRC(pVM, pVCpu->vmm.s.pbEMTStackR3 - PAGE_SIZE), PAGE_SIZE, 0);
        RTMemProtect(pVCpu->vmm.s.pbEMTStackR3 - PAGE_SIZE, PAGE_SIZE, RTMEM_PROT_NONE);

        memset(pVCpu->vmm.s.pbEMTStackR3 + VMM_STACK_SIZE, 0xcc, PAGE_SIZE);
        PGMMapSetPage(pVM, MMHyperR3ToRC(pVM, pVCpu->vmm.s.pbEMTStackR3 + VMM_STACK_SIZE), PAGE_SIZE, 0);
        RTMemProtect(pVCpu->vmm.s.pbEMTStackR3 + VMM_STACK_SIZE, PAGE_SIZE, RTMEM_PROT_NONE);
#endif

        /*
        * Set page attributes to r/w for stack pages.
        */
        rc = PGMMapSetPage(pVM, pVCpu->vmm.s.pbEMTStackRC, VMM_STACK_SIZE, X86_PTE_P | X86_PTE_A | X86_PTE_D | X86_PTE_RW);
        AssertRC(rc);
        if (RT_FAILURE(rc))
            break;
    }
    if (RT_SUCCESS(rc))
    {
        /*
         * Create the EMT yield timer.
         */
        rc = TMR3TimerCreateInternal(pVM, TMCLOCK_REAL, vmmR3YieldEMT, NULL, "EMT Yielder", &pVM->vmm.s.pYieldTimer);
        if (RT_SUCCESS(rc))
           rc = TMTimerSetMillies(pVM->vmm.s.pYieldTimer, pVM->vmm.s.cYieldEveryMillies);
    }

#ifdef VBOX_WITH_NMI
    /*
     * Map the host APIC into GC - This is AMD/Intel + Host OS specific!
     */
    if (RT_SUCCESS(rc))
        rc = PGMMap(pVM, pVM->vmm.s.GCPtrApicBase, 0xfee00000, PAGE_SIZE,
                    X86_PTE_P | X86_PTE_RW | X86_PTE_PWT | X86_PTE_PCD | X86_PTE_A | X86_PTE_D);
#endif
    return rc;
}


/**
 * Initializes the R0 VMM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(int) VMMR3InitR0(PVM pVM)
{
    int    rc;
    PVMCPU pVCpu = VMMGetCpu(pVM);
    Assert(pVCpu && pVCpu->idCpu == 0);

#ifdef LOG_ENABLED
    /*
     * Initialize the ring-0 logger if we haven't done so yet.
     */
    if (    pVCpu->vmm.s.pR0LoggerR3
        &&  !pVCpu->vmm.s.pR0LoggerR3->fCreated)
    {
        rc = VMMR3UpdateLoggers(pVM);
        if (RT_FAILURE(rc))
            return rc;
    }
#endif

    /*
     * Call Ring-0 entry with init code.
     */
    for (;;)
    {
#ifdef NO_SUPCALLR0VMM
        //rc = VERR_GENERAL_FAILURE;
        rc = VINF_SUCCESS;
#else
        rc = SUPCallVMMR0Ex(pVM->pVMR0, 0 /* VCPU 0 */, VMMR0_DO_VMMR0_INIT, VMMGetSvnRev(), NULL);
#endif
        /*
         * Flush the logs.
         */
#ifdef LOG_ENABLED
        if (    pVCpu->vmm.s.pR0LoggerR3
            &&  pVCpu->vmm.s.pR0LoggerR3->Logger.offScratch > 0)
            RTLogFlushToLogger(&pVCpu->vmm.s.pR0LoggerR3->Logger, NULL);
#endif
        if (rc != VINF_VMM_CALL_HOST)
            break;
        rc = vmmR3ServiceCallHostRequest(pVM, pVCpu);
        if (RT_FAILURE(rc) || (rc >= VINF_EM_FIRST && rc <= VINF_EM_LAST))
            break;
        /* Resume R0 */
    }

    if (RT_FAILURE(rc) || (rc >= VINF_EM_FIRST && rc <= VINF_EM_LAST))
    {
        LogRel(("R0 init failed, rc=%Rra\n", rc));
        if (RT_SUCCESS(rc))
            rc = VERR_INTERNAL_ERROR;
    }
    return rc;
}


/**
 * Initializes the RC VMM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(int) VMMR3InitRC(PVM pVM)
{
    PVMCPU pVCpu = VMMGetCpu(pVM);
    Assert(pVCpu && pVCpu->idCpu == 0);

    /* In VMX mode, there's no need to init RC. */
    if (pVM->vmm.s.fSwitcherDisabled)
        return VINF_SUCCESS;

    AssertReturn(pVM->cCPUs == 1, VERR_RAW_MODE_INVALID_SMP);

    /*
     * Call VMMGCInit():
     *      -# resolve the address.
     *      -# setup stackframe and EIP to use the trampoline.
     *      -# do a generic hypervisor call.
     */
    RTRCPTR RCPtrEP;
    int rc = PDMR3LdrGetSymbolRC(pVM, VMMGC_MAIN_MODULE_NAME, "VMMGCEntry", &RCPtrEP);
    if (RT_SUCCESS(rc))
    {
        CPUMHyperSetCtxCore(pVCpu, NULL);
        CPUMSetHyperESP(pVCpu, pVCpu->vmm.s.pbEMTStackBottomRC); /* Clear the stack. */
        uint64_t u64TS = RTTimeProgramStartNanoTS();
        CPUMPushHyper(pVCpu, (uint32_t)(u64TS >> 32));    /* Param 3: The program startup TS - Hi. */
        CPUMPushHyper(pVCpu, (uint32_t)u64TS);            /* Param 3: The program startup TS - Lo. */
        CPUMPushHyper(pVCpu, VMMGetSvnRev());             /* Param 2: Version argument. */
        CPUMPushHyper(pVCpu, VMMGC_DO_VMMGC_INIT);        /* Param 1: Operation. */
        CPUMPushHyper(pVCpu, pVM->pVMRC);                 /* Param 0: pVM */
        CPUMPushHyper(pVCpu, 5 * sizeof(RTRCPTR));        /* trampoline param: stacksize.  */
        CPUMPushHyper(pVCpu, RCPtrEP);                    /* Call EIP. */
        CPUMSetHyperEIP(pVCpu, pVM->vmm.s.pfnCallTrampolineRC);
        Assert(CPUMGetHyperCR3(pVCpu) && CPUMGetHyperCR3(pVCpu) == PGMGetHyperCR3(pVCpu));

        for (;;)
        {
#ifdef NO_SUPCALLR0VMM
            //rc = VERR_GENERAL_FAILURE;
            rc = VINF_SUCCESS;
#else
            rc = SUPCallVMMR0(pVM->pVMR0, 0 /* VCPU 0 */, VMMR0_DO_CALL_HYPERVISOR, NULL);
#endif
#ifdef LOG_ENABLED
            PRTLOGGERRC pLogger = pVM->vmm.s.pRCLoggerR3;
            if (    pLogger
                &&  pLogger->offScratch > 0)
                RTLogFlushRC(NULL, pLogger);
#endif
#ifdef VBOX_WITH_RC_RELEASE_LOGGING
            PRTLOGGERRC pRelLogger = pVM->vmm.s.pRCRelLoggerR3;
            if (RT_UNLIKELY(pRelLogger && pRelLogger->offScratch > 0))
                RTLogFlushRC(RTLogRelDefaultInstance(), pRelLogger);
#endif
            if (rc != VINF_VMM_CALL_HOST)
                break;
            rc = vmmR3ServiceCallHostRequest(pVM, pVCpu);
            if (RT_FAILURE(rc) || (rc >= VINF_EM_FIRST && rc <= VINF_EM_LAST))
                break;
        }

        if (RT_FAILURE(rc) || (rc >= VINF_EM_FIRST && rc <= VINF_EM_LAST))
        {
            VMMR3FatalDump(pVM, pVCpu, rc);
            if (rc >= VINF_EM_FIRST && rc <= VINF_EM_LAST)
                rc = VERR_INTERNAL_ERROR;
        }
        AssertRC(rc);
    }
    return rc;
}


/**
 * Terminate the VMM bits.
 *
 * @returns VINF_SUCCESS.
 * @param   pVM         The VM handle.
 */
VMMR3DECL(int) VMMR3Term(PVM pVM)
{
    PVMCPU pVCpu = VMMGetCpu(pVM);
    Assert(pVCpu && pVCpu->idCpu == 0);

    /*
     * Call Ring-0 entry with termination code.
     */
    int rc;
    for (;;)
    {
#ifdef NO_SUPCALLR0VMM
        //rc = VERR_GENERAL_FAILURE;
        rc = VINF_SUCCESS;
#else
        rc = SUPCallVMMR0Ex(pVM->pVMR0, 0 /* VCPU 0 */, VMMR0_DO_VMMR0_TERM, 0, NULL);
#endif
        /*
         * Flush the logs.
         */
#ifdef LOG_ENABLED
        if (    pVCpu->vmm.s.pR0LoggerR3
            &&  pVCpu->vmm.s.pR0LoggerR3->Logger.offScratch > 0)
            RTLogFlushToLogger(&pVCpu->vmm.s.pR0LoggerR3->Logger, NULL);
#endif
        if (rc != VINF_VMM_CALL_HOST)
            break;
        rc = vmmR3ServiceCallHostRequest(pVM, pVCpu);
        if (RT_FAILURE(rc) || (rc >= VINF_EM_FIRST && rc <= VINF_EM_LAST))
            break;
        /* Resume R0 */
    }
    if (RT_FAILURE(rc) || (rc >= VINF_EM_FIRST && rc <= VINF_EM_LAST))
    {
        LogRel(("VMMR3Term: R0 term failed, rc=%Rra. (warning)\n", rc));
        if (RT_SUCCESS(rc))
            rc = VERR_INTERNAL_ERROR;
    }

    RTCritSectDelete(&pVM->vmm.s.CritSectSync);

#ifdef VBOX_STRICT_VMM_STACK
    /*
     * Make the two stack guard pages present again.
     */
    RTMemProtect(pVM->vmm.s.pbEMTStackR3 - PAGE_SIZE,      PAGE_SIZE, RTMEM_PROT_READ | RTMEM_PROT_WRITE);
    RTMemProtect(pVM->vmm.s.pbEMTStackR3 + VMM_STACK_SIZE, PAGE_SIZE, RTMEM_PROT_READ | RTMEM_PROT_WRITE);
#endif
    return rc;
}


/**
 * Terminates the per-VCPU VMM.
 *
 * Termination means cleaning up and freeing all resources,
 * the VM it self is at this point powered off or suspended.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(int) VMMR3TermCPU(PVM pVM)
{
    return VINF_SUCCESS;
}


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
VMMR3DECL(void) VMMR3Relocate(PVM pVM, RTGCINTPTR offDelta)
{
    LogFlow(("VMMR3Relocate: offDelta=%RGv\n", offDelta));

    /*
     * Recalc the RC address.
     */
    pVM->vmm.s.pvCoreCodeRC = MMHyperR3ToRC(pVM, pVM->vmm.s.pvCoreCodeR3);

    /*
     * The stack.
     */
    for (VMCPUID i = 0; i < pVM->cCPUs; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];

        CPUMSetHyperESP(pVCpu, CPUMGetHyperESP(pVCpu) + offDelta);

        pVCpu->vmm.s.pbEMTStackRC       = MMHyperR3ToRC(pVM, pVCpu->vmm.s.pbEMTStackR3);
        pVCpu->vmm.s.pbEMTStackBottomRC = pVCpu->vmm.s.pbEMTStackRC + VMM_STACK_SIZE;
    }

    /*
     * All the switchers.
     */
    vmmR3SwitcherRelocate(pVM, offDelta);

    /*
     * Get other RC entry points.
     */
    int rc = PDMR3LdrGetSymbolRC(pVM, VMMGC_MAIN_MODULE_NAME, "CPUMGCResumeGuest", &pVM->vmm.s.pfnCPUMRCResumeGuest);
    AssertReleaseMsgRC(rc, ("CPUMGCResumeGuest not found! rc=%Rra\n", rc));

    rc = PDMR3LdrGetSymbolRC(pVM, VMMGC_MAIN_MODULE_NAME, "CPUMGCResumeGuestV86", &pVM->vmm.s.pfnCPUMRCResumeGuestV86);
    AssertReleaseMsgRC(rc, ("CPUMGCResumeGuestV86 not found! rc=%Rra\n", rc));

    /*
     * Update the logger.
     */
    VMMR3UpdateLoggers(pVM);
}


/**
 * Updates the settings for the RC and R0 loggers.
 *
 * @returns VBox status code.
 * @param   pVM     The VM handle.
 */
VMMR3DECL(int)  VMMR3UpdateLoggers(PVM pVM)
{
    /*
     * Simply clone the logger instance (for RC).
     */
    int rc = VINF_SUCCESS;
    RTRCPTR RCPtrLoggerFlush = 0;

    if (pVM->vmm.s.pRCLoggerR3
#ifdef VBOX_WITH_RC_RELEASE_LOGGING
        || pVM->vmm.s.pRCRelLoggerR3
#endif
       )
    {
        rc = PDMR3LdrGetSymbolRC(pVM, VMMGC_MAIN_MODULE_NAME, "vmmGCLoggerFlush", &RCPtrLoggerFlush);
        AssertReleaseMsgRC(rc, ("vmmGCLoggerFlush not found! rc=%Rra\n", rc));
    }

    if (pVM->vmm.s.pRCLoggerR3)
    {
        RTRCPTR RCPtrLoggerWrapper = 0;
        rc = PDMR3LdrGetSymbolRC(pVM, VMMGC_MAIN_MODULE_NAME, "vmmGCLoggerWrapper", &RCPtrLoggerWrapper);
        AssertReleaseMsgRC(rc, ("vmmGCLoggerWrapper not found! rc=%Rra\n", rc));

        pVM->vmm.s.pRCLoggerRC = MMHyperR3ToRC(pVM, pVM->vmm.s.pRCLoggerR3);
        rc = RTLogCloneRC(NULL /* default */, pVM->vmm.s.pRCLoggerR3, pVM->vmm.s.cbRCLogger,
                          RCPtrLoggerWrapper,  RCPtrLoggerFlush, RTLOGFLAGS_BUFFERED);
        AssertReleaseMsgRC(rc, ("RTLogCloneRC failed! rc=%Rra\n", rc));
    }

#ifdef VBOX_WITH_RC_RELEASE_LOGGING
    if (pVM->vmm.s.pRCRelLoggerR3)
    {
        RTRCPTR RCPtrLoggerWrapper = 0;
        rc = PDMR3LdrGetSymbolRC(pVM, VMMGC_MAIN_MODULE_NAME, "vmmGCRelLoggerWrapper", &RCPtrLoggerWrapper);
        AssertReleaseMsgRC(rc, ("vmmGCRelLoggerWrapper not found! rc=%Rra\n", rc));

        pVM->vmm.s.pRCRelLoggerRC = MMHyperR3ToRC(pVM, pVM->vmm.s.pRCRelLoggerR3);
        rc = RTLogCloneRC(RTLogRelDefaultInstance(), pVM->vmm.s.pRCRelLoggerR3, pVM->vmm.s.cbRCRelLogger,
                          RCPtrLoggerWrapper,  RCPtrLoggerFlush, RTLOGFLAGS_BUFFERED);
        AssertReleaseMsgRC(rc, ("RTLogCloneRC failed! rc=%Rra\n", rc));
    }
#endif /* VBOX_WITH_RC_RELEASE_LOGGING */

#ifdef LOG_ENABLED
    /*
     * For the ring-0 EMT logger, we use a per-thread logger instance
     * in ring-0. Only initialize it once.
     */
    for (unsigned i = 0; i < pVM->cCPUs; i++)
    {
        PVMCPU       pVCpu = &pVM->aCpus[i];
        PVMMR0LOGGER pR0LoggerR3 = pVCpu->vmm.s.pR0LoggerR3;
        if (pR0LoggerR3)
        {
            if (!pR0LoggerR3->fCreated)
            {
                RTR0PTR pfnLoggerWrapper = NIL_RTR0PTR;
                rc = PDMR3LdrGetSymbolR0(pVM, VMMR0_MAIN_MODULE_NAME, "vmmR0LoggerWrapper", &pfnLoggerWrapper);
                AssertReleaseMsgRCReturn(rc, ("VMMLoggerWrapper not found! rc=%Rra\n", rc), rc);

                RTR0PTR pfnLoggerFlush = NIL_RTR0PTR;
                rc = PDMR3LdrGetSymbolR0(pVM, VMMR0_MAIN_MODULE_NAME, "vmmR0LoggerFlush", &pfnLoggerFlush);
                AssertReleaseMsgRCReturn(rc, ("VMMLoggerFlush not found! rc=%Rra\n", rc), rc);

                rc = RTLogCreateForR0(&pR0LoggerR3->Logger, pR0LoggerR3->cbLogger,
                                    *(PFNRTLOGGER *)&pfnLoggerWrapper, *(PFNRTLOGFLUSH *)&pfnLoggerFlush,
                                    RTLOGFLAGS_BUFFERED, RTLOGDEST_DUMMY);
                AssertReleaseMsgRCReturn(rc, ("RTLogCreateForR0 failed! rc=%Rra\n", rc), rc);
                pR0LoggerR3->fCreated = true;
                pR0LoggerR3->fFlushingDisabled = false;
            }

            rc = RTLogCopyGroupsAndFlags(&pR0LoggerR3->Logger, NULL /* default */, pVM->vmm.s.pRCLoggerR3->fFlags, RTLOGFLAGS_BUFFERED);
            AssertRC(rc);
        }
    }
#endif
    return rc;
}


/**
 * Gets the pointer to a buffer containing the R0/RC AssertMsg1 output.
 *
 * @returns Pointer to the buffer.
 * @param   pVM         The VM handle.
 */
VMMR3DECL(const char *) VMMR3GetRZAssertMsg1(PVM pVM)
{
    if (HWACCMIsEnabled(pVM))
        return pVM->vmm.s.szRing0AssertMsg1;

    RTRCPTR RCPtr;
    int rc = PDMR3LdrGetSymbolRC(pVM, NULL, "g_szRTAssertMsg1", &RCPtr);
    if (RT_SUCCESS(rc))
        return (const char *)MMHyperRCToR3(pVM, RCPtr);

    return NULL;
}


/**
 * Gets the pointer to a buffer containing the R0/RC AssertMsg2 output.
 *
 * @returns Pointer to the buffer.
 * @param   pVM         The VM handle.
 */
VMMR3DECL(const char *) VMMR3GetRZAssertMsg2(PVM pVM)
{
    if (HWACCMIsEnabled(pVM))
        return pVM->vmm.s.szRing0AssertMsg2;

    RTRCPTR RCPtr;
    int rc = PDMR3LdrGetSymbolRC(pVM, NULL, "g_szRTAssertMsg2", &RCPtr);
    if (RT_SUCCESS(rc))
        return (const char *)MMHyperRCToR3(pVM, RCPtr);

    return NULL;
}


/**
 * Execute state save operation.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pSSM            SSM operation handle.
 */
static DECLCALLBACK(int) vmmR3Save(PVM pVM, PSSMHANDLE pSSM)
{
    LogFlow(("vmmR3Save:\n"));

    /*
     * The hypervisor stack.
     * Note! See note in vmmR3Load (remove this on version change).
     */
    PVMCPU pVCpu0 = &pVM->aCpus[0];
    SSMR3PutRCPtr(pSSM, pVCpu0->vmm.s.pbEMTStackBottomRC);
    RTRCPTR RCPtrESP = CPUMGetHyperESP(pVCpu0);
    AssertMsg(pVCpu0->vmm.s.pbEMTStackBottomRC - RCPtrESP <= VMM_STACK_SIZE, ("Bottom %RRv ESP=%RRv\n", pVCpu0->vmm.s.pbEMTStackBottomRC, RCPtrESP));
    SSMR3PutRCPtr(pSSM, RCPtrESP);
    SSMR3PutMem(pSSM, pVCpu0->vmm.s.pbEMTStackR3, VMM_STACK_SIZE);

    /*
     * Save the started/stopped state of all CPUs except 0 as it will always
     * be running. This avoids breaking the saved state version. :-)
     */
    for (VMCPUID i = 1; i < pVM->cCPUs; i++)
        SSMR3PutBool(pSSM, VMCPUSTATE_IS_STARTED(VMCPU_GET_STATE(&pVM->aCpus[i])));

    return SSMR3PutU32(pSSM, ~0); /* terminator */
}


/**
 * Execute state load operation.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pSSM            SSM operation handle.
 * @param   u32Version      Data layout version.
 */
static DECLCALLBACK(int) vmmR3Load(PVM pVM, PSSMHANDLE pSSM, uint32_t u32Version)
{
    LogFlow(("vmmR3Load:\n"));

    /*
     * Validate version.
     */
    if (u32Version != VMM_SAVED_STATE_VERSION)
    {
        AssertMsgFailed(("vmmR3Load: Invalid version u32Version=%d!\n", u32Version));
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }

    /*
     * Check that the stack is in the same place, or that it's fearly empty.
     *
     * Note! This can be skipped next time we update saved state as we will
     *       never be in a R0/RC -> ring-3 call when saving the state. The
     *       stack and the two associated pointers are not required.
     */
    RTRCPTR RCPtrStackBottom;
    SSMR3GetRCPtr(pSSM, &RCPtrStackBottom);
    RTRCPTR RCPtrESP;
    int rc = SSMR3GetRCPtr(pSSM, &RCPtrESP);
    if (RT_FAILURE(rc))
        return rc;
    SSMR3GetMem(pSSM, pVM->aCpus[0].vmm.s.pbEMTStackR3, VMM_STACK_SIZE);

    /* Restore the VMCPU states. VCPU 0 is always started. */
    VMCPU_SET_STATE(&pVM->aCpus[0], VMCPUSTATE_STARTED);
    for (VMCPUID i = 1; i < pVM->cCPUs; i++)
    {
        bool fStarted;
        rc = SSMR3GetBool(pSSM, &fStarted);
        if (RT_FAILURE(rc))
            return rc;
        VMCPU_SET_STATE(&pVM->aCpus[i], fStarted ? VMCPUSTATE_STARTED : VMCPUSTATE_STOPPED);
    }

    /* terminator */
    uint32_t u32;
    rc = SSMR3GetU32(pSSM, &u32);
    if (RT_FAILURE(rc))
        return rc;
    if (u32 != ~0U)
    {
        AssertMsgFailed(("u32=%#x\n", u32));
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    }
    return VINF_SUCCESS;
}


/**
 * Resolve a builtin RC symbol.
 *
 * Called by PDM when loading or relocating RC modules.
 *
 * @returns VBox status
 * @param   pVM             VM Handle.
 * @param   pszSymbol       Symbol to resolv
 * @param   pRCPtrValue     Where to store the symbol value.
 *
 * @remark  This has to work before VMMR3Relocate() is called.
 */
VMMR3DECL(int) VMMR3GetImportRC(PVM pVM, const char *pszSymbol, PRTRCPTR pRCPtrValue)
{
    if (!strcmp(pszSymbol, "g_Logger"))
    {
        if (pVM->vmm.s.pRCLoggerR3)
            pVM->vmm.s.pRCLoggerRC = MMHyperR3ToRC(pVM, pVM->vmm.s.pRCLoggerR3);
        *pRCPtrValue = pVM->vmm.s.pRCLoggerRC;
    }
    else if (!strcmp(pszSymbol, "g_RelLogger"))
    {
#ifdef VBOX_WITH_RC_RELEASE_LOGGING
        if (pVM->vmm.s.pRCRelLoggerR3)
            pVM->vmm.s.pRCRelLoggerRC = MMHyperR3ToRC(pVM, pVM->vmm.s.pRCRelLoggerR3);
        *pRCPtrValue = pVM->vmm.s.pRCRelLoggerRC;
#else
        *pRCPtrValue = NIL_RTRCPTR;
#endif
    }
    else
        return VERR_SYMBOL_NOT_FOUND;
    return VINF_SUCCESS;
}


/**
 * Suspends the CPU yielder.
 *
 * @param   pVM             The VM handle.
 */
VMMR3DECL(void) VMMR3YieldSuspend(PVM pVM)
{
    VMCPU_ASSERT_EMT(&pVM->aCpus[0]);
    if (!pVM->vmm.s.cYieldResumeMillies)
    {
        uint64_t u64Now = TMTimerGet(pVM->vmm.s.pYieldTimer);
        uint64_t u64Expire = TMTimerGetExpire(pVM->vmm.s.pYieldTimer);
        if (u64Now >= u64Expire || u64Expire == ~(uint64_t)0)
            pVM->vmm.s.cYieldResumeMillies = pVM->vmm.s.cYieldEveryMillies;
        else
            pVM->vmm.s.cYieldResumeMillies = TMTimerToMilli(pVM->vmm.s.pYieldTimer, u64Expire - u64Now);
        TMTimerStop(pVM->vmm.s.pYieldTimer);
    }
    pVM->vmm.s.u64LastYield = RTTimeNanoTS();
}


/**
 * Stops the CPU yielder.
 *
 * @param   pVM             The VM handle.
 */
VMMR3DECL(void) VMMR3YieldStop(PVM pVM)
{
    if (!pVM->vmm.s.cYieldResumeMillies)
        TMTimerStop(pVM->vmm.s.pYieldTimer);
    pVM->vmm.s.cYieldResumeMillies = pVM->vmm.s.cYieldEveryMillies;
    pVM->vmm.s.u64LastYield = RTTimeNanoTS();
}


/**
 * Resumes the CPU yielder when it has been a suspended or stopped.
 *
 * @param   pVM             The VM handle.
 */
VMMR3DECL(void) VMMR3YieldResume(PVM pVM)
{
    if (pVM->vmm.s.cYieldResumeMillies)
    {
        TMTimerSetMillies(pVM->vmm.s.pYieldTimer, pVM->vmm.s.cYieldResumeMillies);
        pVM->vmm.s.cYieldResumeMillies = 0;
    }
}


/**
 * Internal timer callback function.
 *
 * @param   pVM             The VM.
 * @param   pTimer          The timer handle.
 * @param   pvUser          User argument specified upon timer creation.
 */
static DECLCALLBACK(void) vmmR3YieldEMT(PVM pVM, PTMTIMER pTimer, void *pvUser)
{
    /*
     * This really needs some careful tuning. While we shouldn't be too greedy since
     * that'll cause the rest of the system to stop up, we shouldn't be too nice either
     * because that'll cause us to stop up.
     *
     * The current logic is to use the default interval when there is no lag worth
     * mentioning, but when we start accumulating lag we don't bother yielding at all.
     *
     * (This depends on the TMCLOCK_VIRTUAL_SYNC to be scheduled before TMCLOCK_REAL
     * so the lag is up to date.)
     */
    const uint64_t u64Lag = TMVirtualSyncGetLag(pVM);
    if (    u64Lag     <   50000000 /* 50ms */
        ||  (   u64Lag < 1000000000 /*  1s */
             && RTTimeNanoTS() - pVM->vmm.s.u64LastYield < 500000000 /* 500 ms */)
       )
    {
        uint64_t u64Elapsed = RTTimeNanoTS();
        pVM->vmm.s.u64LastYield = u64Elapsed;

        RTThreadYield();

#ifdef LOG_ENABLED
        u64Elapsed = RTTimeNanoTS() - u64Elapsed;
        Log(("vmmR3YieldEMT: %RI64 ns\n", u64Elapsed));
#endif
    }
    TMTimerSetMillies(pTimer, pVM->vmm.s.cYieldEveryMillies);
}


/**
 * Executes guest code in the raw-mode context.
 *
 * @param   pVM         VM handle.
 * @param   pVCpu       The VMCPU to operate on.
 */
VMMR3DECL(int) VMMR3RawRunGC(PVM pVM, PVMCPU pVCpu)
{
    Log2(("VMMR3RawRunGC: (cs:eip=%04x:%08x)\n", CPUMGetGuestCS(pVCpu), CPUMGetGuestEIP(pVCpu)));

    AssertReturn(pVM->cCPUs == 1, VERR_RAW_MODE_INVALID_SMP);

    /*
     * Set the EIP and ESP.
     */
    CPUMSetHyperEIP(pVCpu, CPUMGetGuestEFlags(pVCpu) & X86_EFL_VM
                    ? pVM->vmm.s.pfnCPUMRCResumeGuestV86
                    : pVM->vmm.s.pfnCPUMRCResumeGuest);
    CPUMSetHyperESP(pVCpu, pVCpu->vmm.s.pbEMTStackBottomRC);

    /*
     * We hide log flushes (outer) and hypervisor interrupts (inner).
     */
    for (;;)
    {
        Assert(CPUMGetHyperCR3(pVCpu) && CPUMGetHyperCR3(pVCpu) == PGMGetHyperCR3(pVCpu));
#ifdef VBOX_STRICT
        PGMMapCheck(pVM);
#endif
        int rc;
        do
        {
#ifdef NO_SUPCALLR0VMM
            rc = VERR_GENERAL_FAILURE;
#else
            rc = SUPCallVMMR0Fast(pVM->pVMR0, VMMR0_DO_RAW_RUN, 0);
            if (RT_LIKELY(rc == VINF_SUCCESS))
                rc = pVCpu->vmm.s.iLastGZRc;
#endif
        } while (rc == VINF_EM_RAW_INTERRUPT_HYPER);

        /*
         * Flush the logs.
         */
#ifdef LOG_ENABLED
        PRTLOGGERRC pLogger = pVM->vmm.s.pRCLoggerR3;
        if (    pLogger
            &&  pLogger->offScratch > 0)
            RTLogFlushRC(NULL, pLogger);
#endif
#ifdef VBOX_WITH_RC_RELEASE_LOGGING
        PRTLOGGERRC pRelLogger = pVM->vmm.s.pRCRelLoggerR3;
        if (RT_UNLIKELY(pRelLogger && pRelLogger->offScratch > 0))
            RTLogFlushRC(RTLogRelDefaultInstance(), pRelLogger);
#endif
        if (rc != VINF_VMM_CALL_HOST)
        {
            Log2(("VMMR3RawRunGC: returns %Rrc (cs:eip=%04x:%08x)\n", rc, CPUMGetGuestCS(pVCpu), CPUMGetGuestEIP(pVCpu)));
            return rc;
        }
        rc = vmmR3ServiceCallHostRequest(pVM, pVCpu);
        if (RT_FAILURE(rc))
            return rc;
        /* Resume GC */
    }
}


/**
 * Executes guest code (Intel VT-x and AMD-V).
 *
 * @param   pVM         VM handle.
 * @param   pVCpu       The VMCPU to operate on.
 */
VMMR3DECL(int) VMMR3HwAccRunGC(PVM pVM, PVMCPU pVCpu)
{
    Log2(("VMMR3HwAccRunGC: (cs:eip=%04x:%08x)\n", CPUMGetGuestCS(pVCpu), CPUMGetGuestEIP(pVCpu)));

    for (;;)
    {
        int rc;
        do
        {
#ifdef NO_SUPCALLR0VMM
            rc = VERR_GENERAL_FAILURE;
#else
            rc = SUPCallVMMR0Fast(pVM->pVMR0, VMMR0_DO_HWACC_RUN, pVCpu->idCpu);
            if (RT_LIKELY(rc == VINF_SUCCESS))
                rc = pVCpu->vmm.s.iLastGZRc;
#endif
        } while (rc == VINF_EM_RAW_INTERRUPT_HYPER);

#ifdef LOG_ENABLED
        /*
         * Flush the log
         */
        PVMMR0LOGGER pR0LoggerR3 = pVCpu->vmm.s.pR0LoggerR3;
        if (    pR0LoggerR3
            &&  pR0LoggerR3->Logger.offScratch > 0)
            RTLogFlushToLogger(&pR0LoggerR3->Logger, NULL);
#endif /* !LOG_ENABLED */
        if (rc != VINF_VMM_CALL_HOST)
        {
            Log2(("VMMR3HwAccRunGC: returns %Rrc (cs:eip=%04x:%08x)\n", rc, CPUMGetGuestCS(pVCpu), CPUMGetGuestEIP(pVCpu)));
            return rc;
        }
        rc = vmmR3ServiceCallHostRequest(pVM, pVCpu);
        if (RT_FAILURE(rc))
            return rc;
        /* Resume R0 */
    }
}

/**
 * VCPU worker for VMMSendSipi.
 *
 * @param   pVM         The VM to operate on.
 * @param   idCpu       Virtual CPU to perform SIPI on
 * @param   uVector     SIPI vector
 */
DECLCALLBACK(int) vmmR3SendSipi(PVM pVM, VMCPUID idCpu, uint32_t uVector)
{
    PVMCPU pVCpu = VMMGetCpuById(pVM, idCpu);
    VMCPU_ASSERT_EMT(pVCpu);

    /** @todo what are we supposed to do if the processor is already running? */
    if (EMGetState(pVCpu) != EMSTATE_WAIT_SIPI)
        return VERR_ACCESS_DENIED;


    PCPUMCTX pCtx = CPUMQueryGuestCtxPtr(pVCpu);

    pCtx->cs                        = uVector << 8;
    pCtx->csHid.u64Base             = uVector << 12;
    pCtx->csHid.u32Limit            = 0x0000ffff;
    pCtx->rip                       = 0;

# if 1 /* If we keep the EMSTATE_WAIT_SIPI method, then move this to EM.cpp. */
    EMSetState(pVCpu, EMSTATE_HALTED);
    return VINF_EM_RESCHEDULE;
# else /* And if we go the VMCPU::enmState way it can stay here. */
    VMCPU_ASSERT_STATE(pVCpu, VMCPUSTATE_STOPPED);
    VMCPU_SET_STATE(pVCpu, VMCPUSTATE_STARTED);
    return VINF_SUCCESS;
# endif
}

DECLCALLBACK(int) vmmR3SendInitIpi(PVM pVM, VMCPUID idCpu)
{
    PVMCPU pVCpu = VMMGetCpuById(pVM, idCpu);
    VMCPU_ASSERT_EMT(pVCpu);

    CPUMR3ResetCpu(pVCpu);
    return VINF_EM_WAIT_SIPI;
}

/**
 * Sends SIPI to the virtual CPU by setting CS:EIP into vector-dependent state
 * and unhalting processor
 *
 * @param   pVM         The VM to operate on.
 * @param   idCpu       Virtual CPU to perform SIPI on
 * @param   uVector     SIPI vector
 */
VMMR3DECL(void) VMMR3SendSipi(PVM pVM, VMCPUID idCpu,  uint32_t uVector)
{
    AssertReturnVoid(idCpu < pVM->cCPUs);

    PVMREQ pReq;
    int rc = VMR3ReqCallU(pVM->pUVM, idCpu, &pReq, 0, VMREQFLAGS_NO_WAIT,
                          (PFNRT)vmmR3SendSipi, 3, pVM, idCpu, uVector);
    AssertRC(rc);
}

/**
 * Sends init IPI to the virtual CPU.
 *
 * @param   pVM         The VM to operate on.
 * @param   idCpu       Virtual CPU to perform int IPI on
 */
VMMR3DECL(void) VMMR3SendInitIpi(PVM pVM, VMCPUID idCpu)
{
    AssertReturnVoid(idCpu < pVM->cCPUs);

    PVMREQ pReq;
    int rc = VMR3ReqCallU(pVM->pUVM, idCpu, &pReq, 0, VMREQFLAGS_NO_WAIT,
                          (PFNRT)vmmR3SendInitIpi, 2, pVM, idCpu);
    AssertRC(rc);
}


/**
 * VCPU worker for VMMR3SynchronizeAllVCpus.
 *
 * @param   pVM         The VM to operate on.
 * @param   idCpu       Virtual CPU to perform SIPI on
 * @param   uVector     SIPI vector
 */
DECLCALLBACK(int) vmmR3SyncVCpu(PVM pVM)
{
    /* Block until the job in the caller has finished. */
    RTCritSectEnter(&pVM->vmm.s.CritSectSync);
    RTCritSectLeave(&pVM->vmm.s.CritSectSync);
    return VINF_SUCCESS;
}


/**
 * Atomically execute a callback handler
 * Note: This is very expensive; avoid using it frequently!
 *
 * @param   pVM         The VM to operate on.
 * @param   pfnHandler  Callback handler
 * @param   pvUser      User specified parameter
 */
VMMR3DECL(int) VMMR3AtomicExecuteHandler(PVM pVM, PFNATOMICHANDLER pfnHandler, void *pvUser)
{
    int    rc;
    PVMCPU pVCpu = VMMGetCpu(pVM);
    AssertReturn(pVCpu, VERR_VM_THREAD_NOT_EMT);

    /* Shortcut for the uniprocessor case. */
    if (pVM->cCPUs == 1)
        return pfnHandler(pVM, pvUser);

    RTCritSectEnter(&pVM->vmm.s.CritSectSync);
    for (VMCPUID idCpu = 0; idCpu < pVM->cCPUs; idCpu++)
    {
        if (idCpu != pVCpu->idCpu)
        {
            rc = VMR3ReqCallU(pVM->pUVM, idCpu, NULL, 0, VMREQFLAGS_NO_WAIT,
                              (PFNRT)vmmR3SyncVCpu, 1, pVM);
            AssertRC(rc);
        }
    }
    /* Wait until all other VCPUs are waiting for us. */
    while (RTCritSectGetWaiters(&pVM->vmm.s.CritSectSync) != (int32_t)(pVM->cCPUs - 1))
        RTThreadSleep(1);

    rc = pfnHandler(pVM, pvUser);
    RTCritSectLeave(&pVM->vmm.s.CritSectSync);
    return rc;
}


/**
 * Read from the ring 0 jump buffer stack
 *
 * @returns VBox status code.
 *
 * @param   pVM             Pointer to the shared VM structure.
 * @param   idCpu           The ID of the source CPU context (for the address).
 * @param   pAddress        Where to start reading.
 * @param   pvBuf           Where to store the data we've read.
 * @param   cbRead          The number of bytes to read.
 */
VMMR3DECL(int) VMMR3ReadR0Stack(PVM pVM, VMCPUID idCpu, RTHCUINTPTR pAddress, void *pvBuf, size_t cbRead)
{
    PVMCPU  pVCpu   = VMMGetCpuById(pVM, idCpu);
    AssertReturn(pVCpu, VERR_INVALID_PARAMETER);

    RTHCUINTPTR offset = pVCpu->vmm.s.CallHostR0JmpBuf.SpCheck - pAddress;
    if (offset >= pVCpu->vmm.s.CallHostR0JmpBuf.cbSavedStack)
        return VERR_INVALID_POINTER;

    memcpy(pvBuf, pVCpu->vmm.s.pbEMTStackR3 + pVCpu->vmm.s.CallHostR0JmpBuf.cbSavedStack - offset, cbRead);
    return VINF_SUCCESS;
}


/**
 * Calls a RC function.
 *
 * @param   pVM         The VM handle.
 * @param   RCPtrEntry  The address of the RC function.
 * @param   cArgs       The number of arguments in the ....
 * @param   ...         Arguments to the function.
 */
VMMR3DECL(int) VMMR3CallRC(PVM pVM, RTRCPTR RCPtrEntry, unsigned cArgs, ...)
{
    va_list args;
    va_start(args, cArgs);
    int rc = VMMR3CallRCV(pVM, RCPtrEntry, cArgs, args);
    va_end(args);
    return rc;
}


/**
 * Calls a RC function.
 *
 * @param   pVM         The VM handle.
 * @param   RCPtrEntry  The address of the RC function.
 * @param   cArgs       The number of arguments in the ....
 * @param   args        Arguments to the function.
 */
VMMR3DECL(int) VMMR3CallRCV(PVM pVM, RTRCPTR RCPtrEntry, unsigned cArgs, va_list args)
{
    /* Raw mode implies 1 VCPU. */
    AssertReturn(pVM->cCPUs == 1, VERR_RAW_MODE_INVALID_SMP);
    PVMCPU pVCpu = &pVM->aCpus[0];

    Log2(("VMMR3CallGCV: RCPtrEntry=%RRv cArgs=%d\n", RCPtrEntry, cArgs));

    /*
     * Setup the call frame using the trampoline.
     */
    CPUMHyperSetCtxCore(pVCpu, NULL);
    memset(pVCpu->vmm.s.pbEMTStackR3, 0xaa, VMM_STACK_SIZE); /* Clear the stack. */
    CPUMSetHyperESP(pVCpu, pVCpu->vmm.s.pbEMTStackBottomRC - cArgs * sizeof(RTGCUINTPTR32));
    PRTGCUINTPTR32 pFrame = (PRTGCUINTPTR32)(pVCpu->vmm.s.pbEMTStackR3 + VMM_STACK_SIZE) - cArgs;
    int i = cArgs;
    while (i-- > 0)
        *pFrame++ = va_arg(args, RTGCUINTPTR32);

    CPUMPushHyper(pVCpu, cArgs * sizeof(RTGCUINTPTR32));                          /* stack frame size */
    CPUMPushHyper(pVCpu, RCPtrEntry);                                             /* what to call */
    CPUMSetHyperEIP(pVCpu, pVM->vmm.s.pfnCallTrampolineRC);

    /*
     * We hide log flushes (outer) and hypervisor interrupts (inner).
     */
    for (;;)
    {
        int rc;
        Assert(CPUMGetHyperCR3(pVCpu) && CPUMGetHyperCR3(pVCpu) == PGMGetHyperCR3(pVCpu));
        do
        {
#ifdef NO_SUPCALLR0VMM
            rc = VERR_GENERAL_FAILURE;
#else
            rc = SUPCallVMMR0Fast(pVM->pVMR0, VMMR0_DO_RAW_RUN, 0);
            if (RT_LIKELY(rc == VINF_SUCCESS))
                rc = pVCpu->vmm.s.iLastGZRc;
#endif
        } while (rc == VINF_EM_RAW_INTERRUPT_HYPER);

        /*
         * Flush the logs.
         */
#ifdef LOG_ENABLED
        PRTLOGGERRC pLogger = pVM->vmm.s.pRCLoggerR3;
        if (    pLogger
            &&  pLogger->offScratch > 0)
            RTLogFlushRC(NULL, pLogger);
#endif
#ifdef VBOX_WITH_RC_RELEASE_LOGGING
        PRTLOGGERRC pRelLogger = pVM->vmm.s.pRCRelLoggerR3;
        if (RT_UNLIKELY(pRelLogger && pRelLogger->offScratch > 0))
            RTLogFlushRC(RTLogRelDefaultInstance(), pRelLogger);
#endif
        if (rc == VERR_TRPM_PANIC || rc == VERR_TRPM_DONT_PANIC)
            VMMR3FatalDump(pVM, pVCpu, rc);
        if (rc != VINF_VMM_CALL_HOST)
        {
            Log2(("VMMR3CallGCV: returns %Rrc (cs:eip=%04x:%08x)\n", rc, CPUMGetGuestCS(pVCpu), CPUMGetGuestEIP(pVCpu)));
            return rc;
        }
        rc = vmmR3ServiceCallHostRequest(pVM, pVCpu);
        if (RT_FAILURE(rc))
            return rc;
    }
}


/**
 * Wrapper for SUPCallVMMR0Ex which will deal with
 * VINF_VMM_CALL_HOST returns.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   uOperation  Operation to execute.
 * @param   u64Arg      Constant argument.
 * @param   pReqHdr     Pointer to a request header. See SUPCallVMMR0Ex for
 *                      details.
 */
VMMR3DECL(int) VMMR3CallR0(PVM pVM, uint32_t uOperation, uint64_t u64Arg, PSUPVMMR0REQHDR pReqHdr)
{
    PVMCPU pVCpu = VMMGetCpu(pVM);
    AssertReturn(pVCpu, VERR_VM_THREAD_NOT_EMT);

    /*
     * Call Ring-0 entry with init code.
     */
    int rc;
    for (;;)
    {
#ifdef NO_SUPCALLR0VMM
        rc = VERR_GENERAL_FAILURE;
#else
        rc = SUPCallVMMR0Ex(pVM->pVMR0, pVCpu->idCpu, uOperation, u64Arg, pReqHdr);
#endif
        /*
         * Flush the logs.
         */
#ifdef LOG_ENABLED
        if (    pVCpu->vmm.s.pR0LoggerR3
            &&  pVCpu->vmm.s.pR0LoggerR3->Logger.offScratch > 0)
            RTLogFlushToLogger(&pVCpu->vmm.s.pR0LoggerR3->Logger, NULL);
#endif
        if (rc != VINF_VMM_CALL_HOST)
            break;
        rc = vmmR3ServiceCallHostRequest(pVM, pVCpu);
        if (RT_FAILURE(rc) || (rc >= VINF_EM_FIRST && rc <= VINF_EM_LAST))
            break;
        /* Resume R0 */
    }

    AssertLogRelMsgReturn(rc == VINF_SUCCESS || VBOX_FAILURE(rc),
                          ("uOperation=%u rc=%Rrc\n", uOperation, rc),
                          VERR_INTERNAL_ERROR);
    return rc;
}


/**
 * Resumes executing hypervisor code when interrupted by a queue flush or a
 * debug event.
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   pVCpu       VMCPU handle.
 */
VMMR3DECL(int) VMMR3ResumeHyper(PVM pVM, PVMCPU pVCpu)
{
    Log(("VMMR3ResumeHyper: eip=%RRv esp=%RRv\n", CPUMGetHyperEIP(pVCpu), CPUMGetHyperESP(pVCpu)));
    AssertReturn(pVM->cCPUs == 1, VERR_RAW_MODE_INVALID_SMP);

    /*
     * We hide log flushes (outer) and hypervisor interrupts (inner).
     */
    for (;;)
    {
        int rc;
        Assert(CPUMGetHyperCR3(pVCpu) && CPUMGetHyperCR3(pVCpu) == PGMGetHyperCR3(pVCpu));
        do
        {
#ifdef NO_SUPCALLR0VMM
            rc = VERR_GENERAL_FAILURE;
#else
            rc = SUPCallVMMR0Fast(pVM->pVMR0, VMMR0_DO_RAW_RUN, 0);
            if (RT_LIKELY(rc == VINF_SUCCESS))
                rc = pVCpu->vmm.s.iLastGZRc;
#endif
        } while (rc == VINF_EM_RAW_INTERRUPT_HYPER);

        /*
         * Flush the loggers,
         */
#ifdef LOG_ENABLED
        PRTLOGGERRC pLogger = pVM->vmm.s.pRCLoggerR3;
        if (    pLogger
            &&  pLogger->offScratch > 0)
            RTLogFlushRC(NULL, pLogger);
#endif
#ifdef VBOX_WITH_RC_RELEASE_LOGGING
        PRTLOGGERRC pRelLogger = pVM->vmm.s.pRCRelLoggerR3;
        if (RT_UNLIKELY(pRelLogger && pRelLogger->offScratch > 0))
            RTLogFlushRC(RTLogRelDefaultInstance(), pRelLogger);
#endif
        if (rc == VERR_TRPM_PANIC || rc == VERR_TRPM_DONT_PANIC)
            VMMR3FatalDump(pVM, pVCpu, rc);
        if (rc != VINF_VMM_CALL_HOST)
        {
            Log(("VMMR3ResumeHyper: returns %Rrc\n", rc));
            return rc;
        }
        rc = vmmR3ServiceCallHostRequest(pVM, pVCpu);
        if (RT_FAILURE(rc))
            return rc;
    }
}


/**
 * Service a call to the ring-3 host code.
 *
 * @returns VBox status code.
 * @param   pVM     VM handle.
 * @param   pVCpu   VMCPU handle
 * @remark  Careful with critsects.
 */
static int vmmR3ServiceCallHostRequest(PVM pVM, PVMCPU pVCpu)
{
    /* We must also check for pending releases or else we can deadlock when acquiring a new lock here. 
     * On return we go straight back to R0/GC.
     */
    if (VMCPU_FF_ISPENDING(pVCpu, VMCPU_FF_PDM_CRITSECT))
        PDMR3CritSectFF(pVCpu);

    switch (pVCpu->vmm.s.enmCallHostOperation)
    {
        /*
         * Acquire the PDM lock.
         */
        case VMMCALLHOST_PDM_LOCK:
        {
            pVCpu->vmm.s.rcCallHost = PDMR3LockCall(pVM);
            break;
        }

        /*
         * Flush a PDM queue.
         */
        case VMMCALLHOST_PDM_QUEUE_FLUSH:
        {
            PDMR3QueueFlushWorker(pVM, NULL);
            pVCpu->vmm.s.rcCallHost = VINF_SUCCESS;
            break;
        }

        /*
         * Grow the PGM pool.
         */
        case VMMCALLHOST_PGM_POOL_GROW:
        {
            pVCpu->vmm.s.rcCallHost = PGMR3PoolGrow(pVM);
            break;
        }

        /*
         * Maps an page allocation chunk into ring-3 so ring-0 can use it.
         */
        case VMMCALLHOST_PGM_MAP_CHUNK:
        {
            pVCpu->vmm.s.rcCallHost = PGMR3PhysChunkMap(pVM, pVCpu->vmm.s.u64CallHostArg);
            break;
        }

        /*
         * Allocates more handy pages.
         */
        case VMMCALLHOST_PGM_ALLOCATE_HANDY_PAGES:
        {
            pVCpu->vmm.s.rcCallHost = PGMR3PhysAllocateHandyPages(pVM);
            break;
        }

        /*
         * Acquire the PGM lock.
         */
        case VMMCALLHOST_PGM_LOCK:
        {
            pVCpu->vmm.s.rcCallHost = PGMR3LockCall(pVM);
            break;
        }

        /*
         * Acquire the MM hypervisor heap lock.
         */
        case VMMCALLHOST_MMHYPER_LOCK:
        {
            pVCpu->vmm.s.rcCallHost = MMR3LockCall(pVM);
            break;
        }

        /*
         * Flush REM handler notifications.
         */
        case VMMCALLHOST_REM_REPLAY_HANDLER_NOTIFICATIONS:
        {
            REMR3ReplayHandlerNotifications(pVM);
            pVCpu->vmm.s.rcCallHost = VINF_SUCCESS;
            break;
        }

        /*
         * This is a noop. We just take this route to avoid unnecessary
         * tests in the loops.
         */
        case VMMCALLHOST_VMM_LOGGER_FLUSH:
            pVCpu->vmm.s.rcCallHost = VINF_SUCCESS;
            LogAlways(("*FLUSH*\n"));
            break;

        /*
         * Set the VM error message.
         */
        case VMMCALLHOST_VM_SET_ERROR:
            VMR3SetErrorWorker(pVM);
            pVCpu->vmm.s.rcCallHost = VINF_SUCCESS;
            break;

        /*
         * Set the VM runtime error message.
         */
        case VMMCALLHOST_VM_SET_RUNTIME_ERROR:
            pVCpu->vmm.s.rcCallHost = VMR3SetRuntimeErrorWorker(pVM);
            break;

        /*
         * Signal a ring 0 hypervisor assertion.
         * Cancel the longjmp operation that's in progress.
         */
        case VMMCALLHOST_VM_R0_ASSERTION:
            pVCpu->vmm.s.enmCallHostOperation = VMMCALLHOST_INVALID;
            pVCpu->vmm.s.CallHostR0JmpBuf.fInRing3Call = false;
#ifdef RT_ARCH_X86
            pVCpu->vmm.s.CallHostR0JmpBuf.eip = 0;
#else
            pVCpu->vmm.s.CallHostR0JmpBuf.rip = 0;
#endif
            LogRel((pVM->vmm.s.szRing0AssertMsg1));
            LogRel((pVM->vmm.s.szRing0AssertMsg2));
            return VERR_VMM_RING0_ASSERTION;

        /*
         * A forced switch to ring 0 for preemption purposes.
         */
        case VMMCALLHOST_VM_R0_PREEMPT:
            pVCpu->vmm.s.rcCallHost = VINF_SUCCESS;
            break;

        default:
            AssertMsgFailed(("enmCallHostOperation=%d\n", pVCpu->vmm.s.enmCallHostOperation));
            return VERR_INTERNAL_ERROR;
    }

    pVCpu->vmm.s.enmCallHostOperation = VMMCALLHOST_INVALID;
    return VINF_SUCCESS;
}


/**
 * Displays the Force action Flags.
 *
 * @param   pVM         The VM handle.
 * @param   pHlp        The output helpers.
 * @param   pszArgs     The additional arguments (ignored).
 */
static DECLCALLBACK(void) vmmR3InfoFF(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    int         c;
    uint32_t    f;
#define PRINT_FLAG(prf,flag) do { \
        if (f & (prf##flag)) \
        { \
            static const char *s_psz = #flag; \
            if (!(c % 6)) \
                pHlp->pfnPrintf(pHlp, "%s\n    %s", c ? "," : "", s_psz); \
            else \
                pHlp->pfnPrintf(pHlp, ", %s", s_psz); \
            c++; \
            f &= ~(prf##flag); \
        } \
    } while (0)

#define PRINT_GROUP(prf,grp,sfx) do { \
        if (f & (prf##grp##sfx)) \
        { \
            static const char *s_psz = #grp; \
            if (!(c % 5)) \
                pHlp->pfnPrintf(pHlp, "%s    %s", c ? ",\n" : "  Groups:\n", s_psz); \
            else \
                pHlp->pfnPrintf(pHlp, ", %s", s_psz); \
            c++; \
        } \
    } while (0)

    /*
     * The global flags.
     */
    const uint32_t fGlobalForcedActions = pVM->fGlobalForcedActions;
    pHlp->pfnPrintf(pHlp, "Global FFs: %#RX32", fGlobalForcedActions);

    /* show the flag mnemonics  */
    c = 0;
    f = fGlobalForcedActions;
    PRINT_FLAG(VM_FF_,TM_VIRTUAL_SYNC);
    PRINT_FLAG(VM_FF_,PDM_QUEUES);
    PRINT_FLAG(VM_FF_,PDM_DMA);
    PRINT_FLAG(VM_FF_,DBGF);
    PRINT_FLAG(VM_FF_,REQUEST);
    PRINT_FLAG(VM_FF_,TERMINATE);
    PRINT_FLAG(VM_FF_,RESET);
    PRINT_FLAG(VM_FF_,PGM_NEED_HANDY_PAGES);
    PRINT_FLAG(VM_FF_,PGM_NO_MEMORY);
    PRINT_FLAG(VM_FF_,REM_HANDLER_NOTIFY);
    PRINT_FLAG(VM_FF_,DEBUG_SUSPEND);
    if (f)
        pHlp->pfnPrintf(pHlp, "%s\n    Unknown bits: %#RX32\n", c ? "," : "", f);
    else
        pHlp->pfnPrintf(pHlp, "\n");

    /* the groups */
    c = 0;
    f = fGlobalForcedActions;
    PRINT_GROUP(VM_FF_,EXTERNAL_SUSPENDED,_MASK);
    PRINT_GROUP(VM_FF_,EXTERNAL_HALTED,_MASK);
    PRINT_GROUP(VM_FF_,HIGH_PRIORITY_PRE,_MASK);
    PRINT_GROUP(VM_FF_,HIGH_PRIORITY_PRE_RAW,_MASK);
    PRINT_GROUP(VM_FF_,HIGH_PRIORITY_POST,_MASK);
    PRINT_GROUP(VM_FF_,NORMAL_PRIORITY_POST,_MASK);
    PRINT_GROUP(VM_FF_,NORMAL_PRIORITY,_MASK);
    PRINT_GROUP(VM_FF_,ALL_BUT_RAW,_MASK);
    if (c)
        pHlp->pfnPrintf(pHlp, "\n");

    /*
     * Per CPU flags.
     */
    for (VMCPUID i = 0; i < pVM->cCPUs; i++)
    {
        const uint32_t fLocalForcedActions = pVM->aCpus[i].fLocalForcedActions;
        pHlp->pfnPrintf(pHlp, "CPU %u FFs: %#RX32", i, fLocalForcedActions);

        /* show the flag mnemonics */
        c = 0;
        f = fLocalForcedActions;
        PRINT_FLAG(VMCPU_FF_,INTERRUPT_APIC);
        PRINT_FLAG(VMCPU_FF_,INTERRUPT_PIC);
        PRINT_FLAG(VMCPU_FF_,TIMER);
        PRINT_FLAG(VMCPU_FF_,PDM_CRITSECT);
        PRINT_FLAG(VMCPU_FF_,PGM_SYNC_CR3);
        PRINT_FLAG(VMCPU_FF_,PGM_SYNC_CR3_NON_GLOBAL);
        PRINT_FLAG(VMCPU_FF_,TRPM_SYNC_IDT);
        PRINT_FLAG(VMCPU_FF_,SELM_SYNC_TSS);
        PRINT_FLAG(VMCPU_FF_,SELM_SYNC_GDT);
        PRINT_FLAG(VMCPU_FF_,SELM_SYNC_LDT);
        PRINT_FLAG(VMCPU_FF_,INHIBIT_INTERRUPTS);
        PRINT_FLAG(VMCPU_FF_,CSAM_SCAN_PAGE);
        PRINT_FLAG(VMCPU_FF_,CSAM_PENDING_ACTION);
        PRINT_FLAG(VMCPU_FF_,TO_R3);
        if (f)
            pHlp->pfnPrintf(pHlp, "%s\n    Unknown bits: %#RX32\n", c ? "," : "", f);
        else
            pHlp->pfnPrintf(pHlp, "\n");

        /* the groups */
        c = 0;
        f = fLocalForcedActions;
        PRINT_GROUP(VMCPU_FF_,EXTERNAL_SUSPENDED,_MASK);
        PRINT_GROUP(VMCPU_FF_,EXTERNAL_HALTED,_MASK);
        PRINT_GROUP(VMCPU_FF_,HIGH_PRIORITY_PRE,_MASK);
        PRINT_GROUP(VMCPU_FF_,HIGH_PRIORITY_PRE_RAW,_MASK);
        PRINT_GROUP(VMCPU_FF_,HIGH_PRIORITY_POST,_MASK);
        PRINT_GROUP(VMCPU_FF_,NORMAL_PRIORITY_POST,_MASK);
        PRINT_GROUP(VMCPU_FF_,NORMAL_PRIORITY,_MASK);
        PRINT_GROUP(VMCPU_FF_,RESUME_GUEST,_MASK);
        PRINT_GROUP(VMCPU_FF_,HWACCM_TO_R3,_MASK);
        PRINT_GROUP(VMCPU_FF_,ALL_BUT_RAW,_MASK);
        if (c)
            pHlp->pfnPrintf(pHlp, "\n");
    }

#undef PRINT_FLAG
#undef PRINT_GROUP
}

