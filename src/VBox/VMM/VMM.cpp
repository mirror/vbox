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
*   Global Variables                                                           *
*******************************************************************************/
/** Array of switcher defininitions.
 * The type and index shall match!
 */
static PVMMSWITCHERDEF s_apSwitchers[VMMSWITCHER_MAX] =
{
    NULL, /* invalid entry */
#ifndef RT_ARCH_AMD64
    &vmmR3Switcher32BitTo32Bit_Def,
    &vmmR3Switcher32BitToPAE_Def,
    NULL,   //&vmmR3Switcher32BitToAMD64_Def,
    &vmmR3SwitcherPAETo32Bit_Def,
    &vmmR3SwitcherPAEToPAE_Def,
    NULL,   //&vmmR3SwitcherPAEToAMD64_Def,
# ifdef VBOX_WITH_HYBIRD_32BIT_KERNEL
    &vmmR3SwitcherAMD64ToPAE_Def,
# else
    NULL,   //&vmmR3SwitcherAMD64ToPAE_Def,
# endif
    NULL    //&vmmR3SwitcherAMD64ToAMD64_Def,
#else  /* RT_ARCH_AMD64 */
    NULL,   //&vmmR3Switcher32BitTo32Bit_Def,
    NULL,   //&vmmR3Switcher32BitToPAE_Def,
    NULL,   //&vmmR3Switcher32BitToAMD64_Def,
    NULL,   //&vmmR3SwitcherPAETo32Bit_Def,
    NULL,   //&vmmR3SwitcherPAEToPAE_Def,
    NULL,   //&vmmR3SwitcherPAEToAMD64_Def,
    &vmmR3SwitcherAMD64ToPAE_Def,
    NULL    //&vmmR3SwitcherAMD64ToAMD64_Def,
#endif /* RT_ARCH_AMD64 */
};


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int                  vmmR3InitCoreCode(PVM pVM);
static int                  vmmR3InitStacks(PVM pVM);
static int                  vmmR3InitLoggers(PVM pVM);
static void                 vmmR3InitRegisterStats(PVM pVM);
static DECLCALLBACK(int)    vmmR3Save(PVM pVM, PSSMHANDLE pSSM);
static DECLCALLBACK(int)    vmmR3Load(PVM pVM, PSSMHANDLE pSSM, uint32_t u32Version);
static DECLCALLBACK(void)   vmmR3YieldEMT(PVM pVM, PTMTIMER pTimer, void *pvUser);
static int                  vmmR3ServiceCallHostRequest(PVM pVM);
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
    AssertMsg(sizeof(pVM->vmm.padding) >= sizeof(pVM->vmm.s),
              ("pVM->vmm.padding is too small! vmm.padding %d while vmm.s is %d\n",
               sizeof(pVM->vmm.padding), sizeof(pVM->vmm.s)));

    /*
     * Init basic VM VMM members.
     */
    pVM->vmm.s.offVM = RT_OFFSETOF(VM, vmm);
    int rc = CFGMR3QueryU32(CFGMR3GetRoot(pVM), "YieldEMTInterval", &pVM->vmm.s.cYieldEveryMillies);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        pVM->vmm.s.cYieldEveryMillies = 23; /* Value arrived at after experimenting with the grub boot prompt. */
        //pVM->vmm.s.cYieldEveryMillies = 8; //debugging
    else
        AssertMsgRCReturn(rc, ("Configuration error. Failed to query \"YieldEMTInterval\", rc=%Vrc\n", rc), rc);

    /* GC switchers are enabled by default. Turned off by HWACCM. */
    pVM->vmm.s.fSwitcherDisabled = false;

    /*
     * Register the saved state data unit.
     */
    rc = SSMR3RegisterInternal(pVM, "vmm", 1, VMM_SAVED_STATE_VERSION, VMM_STACK_SIZE + sizeof(RTGCPTR),
                               NULL, vmmR3Save, NULL,
                               NULL, vmmR3Load, NULL);
    if (VBOX_FAILURE(rc))
        return rc;

    /*
     * Register the Ring-0 VM handle with the session for fast ioctl calls.
     */
    rc = SUPSetVMForFastIOCtl(pVM->pVMR0);
    if (VBOX_FAILURE(rc))
        return rc;

    /*
     * Init various sub-components.
     */
    rc = vmmR3InitCoreCode(pVM);
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
                rc = RTCritSectInit(&pVM->vmm.s.CritSectVMLock);
                if (VBOX_SUCCESS(rc))
                {
                    /*
                     * Debug info and statistics.
                     */
                    DBGFR3InfoRegisterInternal(pVM, "ff", "Displays the current Forced actions Flags.", vmmR3InfoFF);
                    vmmR3InitRegisterStats(pVM);

                    return VINF_SUCCESS;
                }
            }
        }
        /** @todo: Need failure cleanup. */

        //more todo in here?
        //if (VBOX_SUCCESS(rc))
        //{
        //}
        //int rc2 = vmmR3TermCoreCode(pVM);
        //AssertRC(rc2));
    }

    return rc;
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
 * VMMR3Init worker that initiates the core code.
 *
 * This is core per VM code which might need fixups and/or for ease of use are
 * put on linear contiguous backing.
 *
 * @returns VBox status code.
 * @param   pVM     Pointer to the shared VM structure.
 */
static int vmmR3InitCoreCode(PVM pVM)
{
    /*
     * Calc the size.
     */
    unsigned cbCoreCode = 0;
    for (unsigned iSwitcher = 0; iSwitcher < RT_ELEMENTS(s_apSwitchers); iSwitcher++)
    {
        pVM->vmm.s.aoffSwitchers[iSwitcher] = cbCoreCode;
        PVMMSWITCHERDEF pSwitcher = s_apSwitchers[iSwitcher];
        if (pSwitcher)
        {
            AssertRelease((unsigned)pSwitcher->enmType == iSwitcher);
            cbCoreCode += RT_ALIGN_32(pSwitcher->cbCode + 1, 32);
        }
    }

    /*
     * Allocate continguous pages for switchers and deal with
     * conflicts in the intermediate mapping of the code.
     */
    pVM->vmm.s.cbCoreCode = RT_ALIGN_32(cbCoreCode, PAGE_SIZE);
    pVM->vmm.s.pvCoreCodeR3 = SUPContAlloc2(pVM->vmm.s.cbCoreCode >> PAGE_SHIFT, &pVM->vmm.s.pvCoreCodeR0, &pVM->vmm.s.HCPhysCoreCode);
    int rc = VERR_NO_MEMORY;
    if (pVM->vmm.s.pvCoreCodeR3)
    {
        rc = PGMR3MapIntermediate(pVM, pVM->vmm.s.pvCoreCodeR0, pVM->vmm.s.HCPhysCoreCode, cbCoreCode);
        if (rc == VERR_PGM_INTERMEDIATE_PAGING_CONFLICT)
        {
            /* try more allocations - Solaris, Linux.  */
            const unsigned cTries = 8234;
            struct VMMInitBadTry
            {
                RTR0PTR  pvR0;
                void    *pvR3;
                RTHCPHYS HCPhys;
                RTUINT   cb;
            } *paBadTries = (struct VMMInitBadTry *)RTMemTmpAlloc(sizeof(*paBadTries) * cTries);
            AssertReturn(paBadTries, VERR_NO_TMP_MEMORY);
            unsigned i = 0;
            do
            {
                paBadTries[i].pvR3 = pVM->vmm.s.pvCoreCodeR3;
                paBadTries[i].pvR0 = pVM->vmm.s.pvCoreCodeR0;
                paBadTries[i].HCPhys = pVM->vmm.s.HCPhysCoreCode;
                i++;
                pVM->vmm.s.pvCoreCodeR0 = NIL_RTR0PTR;
                pVM->vmm.s.HCPhysCoreCode = NIL_RTHCPHYS;
                pVM->vmm.s.pvCoreCodeR3 = SUPContAlloc2(pVM->vmm.s.cbCoreCode >> PAGE_SHIFT, &pVM->vmm.s.pvCoreCodeR0, &pVM->vmm.s.HCPhysCoreCode);
                if (!pVM->vmm.s.pvCoreCodeR3)
                    break;
                rc = PGMR3MapIntermediate(pVM, pVM->vmm.s.pvCoreCodeR0, pVM->vmm.s.HCPhysCoreCode, cbCoreCode);
            } while (   rc == VERR_PGM_INTERMEDIATE_PAGING_CONFLICT
                     && i < cTries - 1);

            /* cleanup */
            if (VBOX_FAILURE(rc))
            {
                paBadTries[i].pvR3   = pVM->vmm.s.pvCoreCodeR3;
                paBadTries[i].pvR0   = pVM->vmm.s.pvCoreCodeR0;
                paBadTries[i].HCPhys = pVM->vmm.s.HCPhysCoreCode;
                paBadTries[i].cb     = pVM->vmm.s.cbCoreCode;
                i++;
                LogRel(("Failed to allocated and map core code: rc=%Vrc\n", rc));
            }
            while (i-- > 0)
            {
                LogRel(("Core code alloc attempt #%d: pvR3=%p pvR0=%p HCPhys=%VHp\n",
                        i, paBadTries[i].pvR3, paBadTries[i].pvR0, paBadTries[i].HCPhys));
                SUPContFree(paBadTries[i].pvR3, paBadTries[i].cb >> PAGE_SHIFT);
            }
            RTMemTmpFree(paBadTries);
        }
    }
    if (VBOX_SUCCESS(rc))
    {
        /*
         * copy the code.
         */
        for (unsigned iSwitcher = 0; iSwitcher < RT_ELEMENTS(s_apSwitchers); iSwitcher++)
        {
            PVMMSWITCHERDEF pSwitcher = s_apSwitchers[iSwitcher];
            if (pSwitcher)
                memcpy((uint8_t *)pVM->vmm.s.pvCoreCodeR3 + pVM->vmm.s.aoffSwitchers[iSwitcher],
                       pSwitcher->pvCode, pSwitcher->cbCode);
        }

        /*
         * Map the code into the GC address space.
         */
        RTGCPTR GCPtr;
        rc = MMR3HyperMapHCPhys(pVM, pVM->vmm.s.pvCoreCodeR3, pVM->vmm.s.HCPhysCoreCode, cbCoreCode, "Core Code", &GCPtr);
        if (VBOX_SUCCESS(rc))
        {
            pVM->vmm.s.pvCoreCodeRC = GCPtr;
            MMR3HyperReserve(pVM, PAGE_SIZE, "fence", NULL);
            LogRel(("CoreCode: R3=%VHv R0=%VHv GC=%VRv Phys=%VHp cb=%#x\n",
                    pVM->vmm.s.pvCoreCodeR3, pVM->vmm.s.pvCoreCodeR0, pVM->vmm.s.pvCoreCodeRC, pVM->vmm.s.HCPhysCoreCode, pVM->vmm.s.cbCoreCode));

            /*
             * Finally, PGM probably have selected a switcher already but we need
             * to get the routine addresses, so we'll reselect it.
             * This may legally fail so, we're ignoring the rc.
             */
            VMMR3SelectSwitcher(pVM, pVM->vmm.s.enmSwitcher);
            return rc;
        }

        /* shit */
        AssertMsgFailed(("PGMR3Map(,%VRv, %VGp, %#x, 0) failed with rc=%Vrc\n", pVM->vmm.s.pvCoreCodeRC, pVM->vmm.s.HCPhysCoreCode, cbCoreCode, rc));
        SUPContFree(pVM->vmm.s.pvCoreCodeR3, pVM->vmm.s.cbCoreCode >> PAGE_SHIFT);
    }
    else
        VMSetError(pVM, rc, RT_SRC_POS,
                   N_("Failed to allocate %d bytes of contiguous memory for the world switcher code"),
                   cbCoreCode);

    pVM->vmm.s.pvCoreCodeR3 = NULL;
    pVM->vmm.s.pvCoreCodeR0 = NIL_RTR0PTR;
    pVM->vmm.s.pvCoreCodeRC = 0;
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
    /** @todo SMP: On stack per vCPU. */
#ifdef VBOX_STRICT_VMM_STACK
    int rc = MMHyperAlloc(pVM, VMM_STACK_SIZE + PAGE_SIZE + PAGE_SIZE, PAGE_SIZE, MM_TAG_VMM, (void **)&pVM->vmm.s.pbEMTStackR3);
#else
    int rc = MMHyperAlloc(pVM, VMM_STACK_SIZE, PAGE_SIZE, MM_TAG_VMM, (void **)&pVM->vmm.s.pbEMTStackR3);
#endif
    if (VBOX_SUCCESS(rc))
    {
        pVM->vmm.s.CallHostR0JmpBuf.pvSavedStack = MMHyperR3ToR0(pVM, pVM->vmm.s.pbEMTStackR3);
        pVM->vmm.s.pbEMTStackRC = MMHyperR3ToRC(pVM, pVM->vmm.s.pbEMTStackR3);
        pVM->vmm.s.pbEMTStackBottomRC = pVM->vmm.s.pbEMTStackRC + VMM_STACK_SIZE;
        AssertRelease(pVM->vmm.s.pbEMTStackRC);

        CPUMSetHyperESP(pVM, pVM->vmm.s.pbEMTStackBottomRC);
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
        rc = MMHyperAlloc(pVM, pVM->vmm.s.cbRCLogger, 0, MM_TAG_VMM, (void **)&pVM->vmm.s.pRCLoggerR3);
        if (RT_FAILURE(rc))
            return rc;
        pVM->vmm.s.pRCLoggerRC = MMHyperR3ToRC(pVM, pVM->vmm.s.pRCLoggerR3);

# ifdef VBOX_WITH_R0_LOGGING
        rc = MMHyperAlloc(pVM, RT_OFFSETOF(VMMR0LOGGER, Logger.afGroups[pLogger->cGroups]),
                          0, MM_TAG_VMM, (void **)&pVM->vmm.s.pR0LoggerR3);
        if (RT_FAILURE(rc))
            return rc;
        pVM->vmm.s.pR0LoggerR3->pVM = pVM->pVMR0;
        //pVM->vmm.s.pR0LoggerR3->fCreated = false;
        pVM->vmm.s.pR0LoggerR3->cbLogger = RT_OFFSETOF(RTLOGGER, afGroups[pLogger->cGroups]);
        pVM->vmm.s.pR0LoggerR0 = MMHyperR3ToR0(pVM, pVM->vmm.s.pR0LoggerR3);
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
        rc = MMHyperAlloc(pVM, pVM->vmm.s.cbRCRelLogger, 0, MM_TAG_VMM, (void **)&pVM->vmm.s.pRCRelLoggerR3);
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
#ifndef VBOX_WITH_NEW_PHYS_CODE
    STAM_REG(pVM, &pVM->vmm.s.StatRZCallPGMGrowRAM,         STAMTYPE_COUNTER, "/VMM/RZCallR3/PGMGrowRAM",       STAMUNIT_OCCURENCES, "Number of VMMCALLHOST_PGM_RAM_GROW_RANGE calls.");
#endif
    STAM_REG(pVM, &pVM->vmm.s.StatRZCallRemReplay,          STAMTYPE_COUNTER, "/VMM/RZCallR3/REMReplay",        STAMUNIT_OCCURENCES, "Number of VMMCALLHOST_REM_REPLAY_HANDLER_NOTIFICATIONS calls.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZCallLogFlush,           STAMTYPE_COUNTER, "/VMM/RZCallR3/VMMLogFlush",      STAMUNIT_OCCURENCES, "Number of VMMCALLHOST_VMM_LOGGER_FLUSH calls.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZCallVMSetError,         STAMTYPE_COUNTER, "/VMM/RZCallR3/VMSetError",       STAMUNIT_OCCURENCES, "Number of VMMCALLHOST_VM_SET_ERROR calls.");
    STAM_REG(pVM, &pVM->vmm.s.StatRZCallVMSetRuntimeError,  STAMTYPE_COUNTER, "/VMM/RZCallR3/VMRuntimeError",   STAMUNIT_OCCURENCES, "Number of VMMCALLHOST_VM_SET_RUNTIME_ERROR calls.");
}


/**
 * Ring-3 init finalizing.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 */
VMMR3DECL(int) VMMR3InitFinalize(PVM pVM)
{
#ifdef VBOX_STRICT_VMM_STACK
    /*
     * Two inaccessible pages at each sides of the stack to catch over/under-flows.
     */
    memset(pVM->vmm.s.pbEMTStackR3 - PAGE_SIZE, 0xcc, PAGE_SIZE);
    PGMMapSetPage(pVM, MMHyperR3ToRC(pVM, pVM->vmm.s.pbEMTStackR3 - PAGE_SIZE), PAGE_SIZE, 0);
    RTMemProtect(pVM->vmm.s.pbEMTStackR3 - PAGE_SIZE, PAGE_SIZE, RTMEM_PROT_NONE);

    memset(pVM->vmm.s.pbEMTStackR3 + VMM_STACK_SIZE, 0xcc, PAGE_SIZE);
    PGMMapSetPage(pVM, MMHyperR3ToRC(pVM, pVM->vmm.s.pbEMTStackR3 + VMM_STACK_SIZE), PAGE_SIZE, 0);
    RTMemProtect(pVM->vmm.s.pbEMTStackR3 + VMM_STACK_SIZE, PAGE_SIZE, RTMEM_PROT_NONE);
#endif

    /*
     * Set page attributes to r/w for stack pages.
     */
    int rc = PGMMapSetPage(pVM, pVM->vmm.s.pbEMTStackRC, VMM_STACK_SIZE, X86_PTE_P | X86_PTE_A | X86_PTE_D | X86_PTE_RW);
    AssertRC(rc);
    if (VBOX_SUCCESS(rc))
    {
        /*
         * Create the EMT yield timer.
         */
        rc = TMR3TimerCreateInternal(pVM, TMCLOCK_REAL, vmmR3YieldEMT, NULL, "EMT Yielder", &pVM->vmm.s.pYieldTimer);
        if (VBOX_SUCCESS(rc))
           rc = TMTimerSetMillies(pVM->vmm.s.pYieldTimer, pVM->vmm.s.cYieldEveryMillies);
    }

#ifdef VBOX_WITH_NMI
    /*
     * Map the host APIC into GC - This is AMD/Intel + Host OS specific!
     */
    if (VBOX_SUCCESS(rc))
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
    int rc;

    /*
     * Initialize the ring-0 logger if we haven't done so yet.
     */
    if (    pVM->vmm.s.pR0LoggerR3
        &&  !pVM->vmm.s.pR0LoggerR3->fCreated)
    {
        rc = VMMR3UpdateLoggers(pVM);
        if (VBOX_FAILURE(rc))
            return rc;
    }

    /*
     * Call Ring-0 entry with init code.
     */
    for (;;)
    {
#ifdef NO_SUPCALLR0VMM
        //rc = VERR_GENERAL_FAILURE;
        rc = VINF_SUCCESS;
#else
        rc = SUPCallVMMR0Ex(pVM->pVMR0, VMMR0_DO_VMMR0_INIT, VMMGetSvnRev(), NULL);
#endif
        if (    pVM->vmm.s.pR0LoggerR3
            &&  pVM->vmm.s.pR0LoggerR3->Logger.offScratch > 0)
            RTLogFlushToLogger(&pVM->vmm.s.pR0LoggerR3->Logger, NULL);
        if (rc != VINF_VMM_CALL_HOST)
            break;
        rc = vmmR3ServiceCallHostRequest(pVM);
        if (VBOX_FAILURE(rc) || (rc >= VINF_EM_FIRST && rc <= VINF_EM_LAST))
            break;
        /* Resume R0 */
    }

    if (VBOX_FAILURE(rc) || (rc >= VINF_EM_FIRST && rc <= VINF_EM_LAST))
    {
        LogRel(("R0 init failed, rc=%Vra\n", rc));
        if (VBOX_SUCCESS(rc))
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
    /* In VMX mode, there's no need to init RC. */
    if (pVM->vmm.s.fSwitcherDisabled)
        return VINF_SUCCESS;

    /*
     * Call VMMGCInit():
     *      -# resolve the address.
     *      -# setup stackframe and EIP to use the trampoline.
     *      -# do a generic hypervisor call.
     */
    RTGCPTR32 GCPtrEP;
    int rc = PDMR3LdrGetSymbolRC(pVM, VMMGC_MAIN_MODULE_NAME, "VMMGCEntry", &GCPtrEP);
    if (VBOX_SUCCESS(rc))
    {
        CPUMHyperSetCtxCore(pVM, NULL);
        CPUMSetHyperESP(pVM, pVM->vmm.s.pbEMTStackBottomRC); /* Clear the stack. */
        uint64_t u64TS = RTTimeProgramStartNanoTS();
        CPUMPushHyper(pVM, (uint32_t)(u64TS >> 32));    /* Param 3: The program startup TS - Hi. */
        CPUMPushHyper(pVM, (uint32_t)u64TS);            /* Param 3: The program startup TS - Lo. */
        CPUMPushHyper(pVM, VMMGetSvnRev());             /* Param 2: Version argument. */
        CPUMPushHyper(pVM, VMMGC_DO_VMMGC_INIT);        /* Param 1: Operation. */
        CPUMPushHyper(pVM, pVM->pVMRC);                 /* Param 0: pVM */
        CPUMPushHyper(pVM, 3 * sizeof(RTGCPTR32));      /* trampoline param: stacksize.  */
        CPUMPushHyper(pVM, GCPtrEP);                    /* Call EIP. */
        CPUMSetHyperEIP(pVM, pVM->vmm.s.pfnCallTrampolineRC);

        for (;;)
        {
#ifdef NO_SUPCALLR0VMM
            //rc = VERR_GENERAL_FAILURE;
            rc = VINF_SUCCESS;
#else
            rc = SUPCallVMMR0(pVM->pVMR0, VMMR0_DO_CALL_HYPERVISOR, NULL);
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
            rc = vmmR3ServiceCallHostRequest(pVM);
            if (VBOX_FAILURE(rc) || (rc >= VINF_EM_FIRST && rc <= VINF_EM_LAST))
                break;
        }

        if (VBOX_FAILURE(rc) || (rc >= VINF_EM_FIRST && rc <= VINF_EM_LAST))
        {
            VMMR3FatalDump(pVM, rc);
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
        rc = SUPCallVMMR0Ex(pVM->pVMR0, VMMR0_DO_VMMR0_TERM, 0, NULL);
#endif
        if (    pVM->vmm.s.pR0LoggerR3
            &&  pVM->vmm.s.pR0LoggerR3->Logger.offScratch > 0)
            RTLogFlushToLogger(&pVM->vmm.s.pR0LoggerR3->Logger, NULL);
        if (rc != VINF_VMM_CALL_HOST)
            break;
        rc = vmmR3ServiceCallHostRequest(pVM);
        if (VBOX_FAILURE(rc) || (rc >= VINF_EM_FIRST && rc <= VINF_EM_LAST))
            break;
        /* Resume R0 */
    }
    if (VBOX_FAILURE(rc) || (rc >= VINF_EM_FIRST && rc <= VINF_EM_LAST))
    {
        LogRel(("VMMR3Term: R0 term failed, rc=%Vra. (warning)\n", rc));
        if (VBOX_SUCCESS(rc))
            rc = VERR_INTERNAL_ERROR;
    }

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
    LogFlow(("VMMR3Relocate: offDelta=%VGv\n", offDelta));

    /*
     * Recalc the RC address.
     */
    pVM->vmm.s.pvCoreCodeRC = MMHyperR3ToRC(pVM, pVM->vmm.s.pvCoreCodeR3);

    /*
     * The stack.
     */
    CPUMSetHyperESP(pVM, CPUMGetHyperESP(pVM) + offDelta);
    pVM->vmm.s.pbEMTStackRC = MMHyperR3ToRC(pVM, pVM->vmm.s.pbEMTStackR3);
    pVM->vmm.s.pbEMTStackBottomRC = pVM->vmm.s.pbEMTStackRC + VMM_STACK_SIZE;

    /*
     * All the switchers.
     */
    for (unsigned iSwitcher = 0; iSwitcher < RT_ELEMENTS(s_apSwitchers); iSwitcher++)
    {
        PVMMSWITCHERDEF pSwitcher = s_apSwitchers[iSwitcher];
        if (pSwitcher && pSwitcher->pfnRelocate)
        {
            unsigned off = pVM->vmm.s.aoffSwitchers[iSwitcher];
            pSwitcher->pfnRelocate(pVM,
                                   pSwitcher,
                                   pVM->vmm.s.pvCoreCodeR0 + off,
                                   (uint8_t *)pVM->vmm.s.pvCoreCodeR3 + off,
                                   pVM->vmm.s.pvCoreCodeRC + off,
                                   pVM->vmm.s.HCPhysCoreCode + off);
        }
    }

    /*
     * Recalc the RC address for the current switcher.
     */
    PVMMSWITCHERDEF pSwitcher   = s_apSwitchers[pVM->vmm.s.enmSwitcher];
    RTRCPTR         RCPtr       = pVM->vmm.s.pvCoreCodeRC + pVM->vmm.s.aoffSwitchers[pVM->vmm.s.enmSwitcher];
    pVM->vmm.s.pfnGuestToHostRC         = RCPtr + pSwitcher->offGCGuestToHost;
    pVM->vmm.s.pfnCallTrampolineRC      = RCPtr + pSwitcher->offGCCallTrampoline;
    pVM->pfnVMMGCGuestToHostAsm         = RCPtr + pSwitcher->offGCGuestToHostAsm;
    pVM->pfnVMMGCGuestToHostAsmHyperCtx = RCPtr + pSwitcher->offGCGuestToHostAsmHyperCtx;
    pVM->pfnVMMGCGuestToHostAsmGuestCtx = RCPtr + pSwitcher->offGCGuestToHostAsmGuestCtx;

    /*
     * Get other RC entry points.
     */
    int rc = PDMR3LdrGetSymbolRC(pVM, VMMGC_MAIN_MODULE_NAME, "CPUMGCResumeGuest", &pVM->vmm.s.pfnCPUMRCResumeGuest);
    AssertReleaseMsgRC(rc, ("CPUMGCResumeGuest not found! rc=%Vra\n", rc));

    rc = PDMR3LdrGetSymbolRC(pVM, VMMGC_MAIN_MODULE_NAME, "CPUMGCResumeGuestV86", &pVM->vmm.s.pfnCPUMRCResumeGuestV86);
    AssertReleaseMsgRC(rc, ("CPUMGCResumeGuestV86 not found! rc=%Vra\n", rc));

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
        AssertReleaseMsgRC(rc, ("vmmGCLoggerFlush not found! rc=%Vra\n", rc));
    }

    if (pVM->vmm.s.pRCLoggerR3)
    {
        RTRCPTR RCPtrLoggerWrapper = 0;
        rc = PDMR3LdrGetSymbolRC(pVM, VMMGC_MAIN_MODULE_NAME, "vmmGCLoggerWrapper", &RCPtrLoggerWrapper);
        AssertReleaseMsgRC(rc, ("vmmGCLoggerWrapper not found! rc=%Vra\n", rc));

        pVM->vmm.s.pRCLoggerRC = MMHyperR3ToRC(pVM, pVM->vmm.s.pRCLoggerR3);
        rc = RTLogCloneRC(NULL /* default */, pVM->vmm.s.pRCLoggerR3, pVM->vmm.s.cbRCLogger,
                          RCPtrLoggerWrapper,  RCPtrLoggerFlush, RTLOGFLAGS_BUFFERED);
        AssertReleaseMsgRC(rc, ("RTLogCloneRC failed! rc=%Vra\n", rc));
    }

#ifdef VBOX_WITH_RC_RELEASE_LOGGING
    if (pVM->vmm.s.pRCRelLoggerR3)
    {
        RTRCPTR RCPtrLoggerWrapper = 0;
        rc = PDMR3LdrGetSymbolRC(pVM, VMMGC_MAIN_MODULE_NAME, "vmmGCRelLoggerWrapper", &RCPtrLoggerWrapper);
        AssertReleaseMsgRC(rc, ("vmmGCRelLoggerWrapper not found! rc=%Vra\n", rc));

        pVM->vmm.s.pRCRelLoggerRC = MMHyperR3ToRC(pVM, pVM->vmm.s.pRCRelLoggerR3);
        rc = RTLogCloneRC(RTLogRelDefaultInstance(), pVM->vmm.s.pRCRelLoggerR3, pVM->vmm.s.cbRCRelLogger,
                          RCPtrLoggerWrapper,  RCPtrLoggerFlush, RTLOGFLAGS_BUFFERED);
        AssertReleaseMsgRC(rc, ("RTLogCloneRC failed! rc=%Vra\n", rc));
    }
#endif /* VBOX_WITH_RC_RELEASE_LOGGING */

    /*
     * For the ring-0 EMT logger, we use a per-thread logger instance
     * in ring-0. Only initialize it once.
     */
    PVMMR0LOGGER pR0LoggerR3 = pVM->vmm.s.pR0LoggerR3;
    if (pR0LoggerR3)
    {
        if (!pR0LoggerR3->fCreated)
        {
            RTR0PTR pfnLoggerWrapper = NIL_RTR0PTR;
            rc = PDMR3LdrGetSymbolR0(pVM, VMMR0_MAIN_MODULE_NAME, "vmmR0LoggerWrapper", &pfnLoggerWrapper);
            AssertReleaseMsgRCReturn(rc, ("VMMLoggerWrapper not found! rc=%Vra\n", rc), rc);

            RTR0PTR pfnLoggerFlush = NIL_RTR0PTR;
            rc = PDMR3LdrGetSymbolR0(pVM, VMMR0_MAIN_MODULE_NAME, "vmmR0LoggerFlush", &pfnLoggerFlush);
            AssertReleaseMsgRCReturn(rc, ("VMMLoggerFlush not found! rc=%Vra\n", rc), rc);

            rc = RTLogCreateForR0(&pR0LoggerR3->Logger, pR0LoggerR3->cbLogger,
                                  *(PFNRTLOGGER *)&pfnLoggerWrapper, *(PFNRTLOGFLUSH *)&pfnLoggerFlush,
                                  RTLOGFLAGS_BUFFERED, RTLOGDEST_DUMMY);
            AssertReleaseMsgRCReturn(rc, ("RTLogCreateForR0 failed! rc=%Vra\n", rc), rc);
            pR0LoggerR3->fCreated = true;
        }

        rc = RTLogCopyGroupsAndFlags(&pR0LoggerR3->Logger, NULL /* default */, pVM->vmm.s.pRCLoggerR3->fFlags, RTLOGFLAGS_BUFFERED);
        AssertRC(rc);
    }

    return rc;
}


/**
 * Generic switch code relocator.
 *
 * @param   pVM         The VM handle.
 * @param   pSwitcher   The switcher definition.
 * @param   pu8CodeR3   Pointer to the core code block for the switcher, ring-3 mapping.
 * @param   R0PtrCode   Pointer to the core code block for the switcher, ring-0 mapping.
 * @param   GCPtrCode   The guest context address corresponding to pu8Code.
 * @param   u32IDCode   The identity mapped (ID) address corresponding to pu8Code.
 * @param   SelCS       The hypervisor CS selector.
 * @param   SelDS       The hypervisor DS selector.
 * @param   SelTSS      The hypervisor TSS selector.
 * @param   GCPtrGDT    The GC address of the hypervisor GDT.
 * @param   SelCS64     The 64-bit mode hypervisor CS selector.
 */
static void vmmR3SwitcherGenericRelocate(PVM pVM, PVMMSWITCHERDEF pSwitcher, RTR0PTR R0PtrCode, uint8_t *pu8CodeR3, RTGCPTR GCPtrCode, uint32_t u32IDCode,
                                         RTSEL SelCS, RTSEL SelDS, RTSEL SelTSS, RTGCPTR GCPtrGDT, RTSEL SelCS64)
{
    union
    {
        const uint8_t *pu8;
        const uint16_t *pu16;
        const uint32_t *pu32;
        const uint64_t *pu64;
        const void     *pv;
        uintptr_t       u;
    } u;
    u.pv = pSwitcher->pvFixups;

    /*
     * Process fixups.
     */
    uint8_t u8;
    while ((u8 = *u.pu8++) != FIX_THE_END)
    {
        /*
         * Get the source (where to write the fixup).
         */
        uint32_t offSrc = *u.pu32++;
        Assert(offSrc < pSwitcher->cbCode);
        union
        {
            uint8_t    *pu8;
            uint16_t   *pu16;
            uint32_t   *pu32;
            uint64_t   *pu64;
            uintptr_t   u;
        } uSrc;
        uSrc.pu8 = pu8CodeR3 + offSrc;

        /* The fixup target and method depends on the type. */
        switch (u8)
        {
            /*
             * 32-bit relative, source in HC and target in GC.
             */
            case FIX_HC_2_GC_NEAR_REL:
            {
                Assert(offSrc - pSwitcher->offHCCode0 < pSwitcher->cbHCCode0 || offSrc - pSwitcher->offHCCode1 < pSwitcher->cbHCCode1);
                uint32_t offTrg = *u.pu32++;
                Assert(offTrg - pSwitcher->offGCCode < pSwitcher->cbGCCode);
                *uSrc.pu32 = (uint32_t)((GCPtrCode + offTrg) - (uSrc.u + 4));
                break;
            }

            /*
             * 32-bit relative, source in HC and target in ID.
             */
            case FIX_HC_2_ID_NEAR_REL:
            {
                Assert(offSrc - pSwitcher->offHCCode0 < pSwitcher->cbHCCode0 || offSrc - pSwitcher->offHCCode1 < pSwitcher->cbHCCode1);
                uint32_t offTrg = *u.pu32++;
                Assert(offTrg - pSwitcher->offIDCode0 < pSwitcher->cbIDCode0 || offTrg - pSwitcher->offIDCode1 < pSwitcher->cbIDCode1);
                *uSrc.pu32 = (uint32_t)((u32IDCode + offTrg) - (R0PtrCode + offSrc + 4));
                break;
            }

            /*
             * 32-bit relative, source in GC and target in HC.
             */
            case FIX_GC_2_HC_NEAR_REL:
            {
                Assert(offSrc - pSwitcher->offGCCode < pSwitcher->cbGCCode);
                uint32_t offTrg = *u.pu32++;
                Assert(offTrg - pSwitcher->offHCCode0 < pSwitcher->cbHCCode0 || offTrg - pSwitcher->offHCCode1 < pSwitcher->cbHCCode1);
                *uSrc.pu32 = (uint32_t)((R0PtrCode + offTrg) - (GCPtrCode + offSrc + 4));
                break;
            }

            /*
             * 32-bit relative, source in GC and target in ID.
             */
            case FIX_GC_2_ID_NEAR_REL:
            {
                Assert(offSrc - pSwitcher->offGCCode < pSwitcher->cbGCCode);
                uint32_t offTrg = *u.pu32++;
                Assert(offTrg - pSwitcher->offIDCode0 < pSwitcher->cbIDCode0 || offTrg - pSwitcher->offIDCode1 < pSwitcher->cbIDCode1);
                *uSrc.pu32 = (uint32_t)((u32IDCode + offTrg) - (GCPtrCode + offSrc + 4));
                break;
            }

            /*
             * 32-bit relative, source in ID and target in HC.
             */
            case FIX_ID_2_HC_NEAR_REL:
            {
                Assert(offSrc - pSwitcher->offIDCode0 < pSwitcher->cbIDCode0 || offSrc - pSwitcher->offIDCode1 < pSwitcher->cbIDCode1);
                uint32_t offTrg = *u.pu32++;
                Assert(offTrg - pSwitcher->offHCCode0 < pSwitcher->cbHCCode0 || offTrg - pSwitcher->offHCCode1 < pSwitcher->cbHCCode1);
                *uSrc.pu32 = (uint32_t)((R0PtrCode + offTrg) - (u32IDCode + offSrc + 4));
                break;
            }

            /*
             * 32-bit relative, source in ID and target in HC.
             */
            case FIX_ID_2_GC_NEAR_REL:
            {
                Assert(offSrc - pSwitcher->offIDCode0 < pSwitcher->cbIDCode0 || offSrc - pSwitcher->offIDCode1 < pSwitcher->cbIDCode1);
                uint32_t offTrg = *u.pu32++;
                Assert(offTrg - pSwitcher->offGCCode < pSwitcher->cbGCCode);
                *uSrc.pu32 = (uint32_t)((GCPtrCode + offTrg) - (u32IDCode + offSrc + 4));
                break;
            }

            /*
             * 16:32 far jump, target in GC.
             */
            case FIX_GC_FAR32:
            {
                uint32_t offTrg = *u.pu32++;
                Assert(offTrg - pSwitcher->offGCCode < pSwitcher->cbGCCode);
                *uSrc.pu32++ = (uint32_t)(GCPtrCode + offTrg);
                *uSrc.pu16++ = SelCS;
                break;
            }

            /*
             * Make 32-bit GC pointer given CPUM offset.
             */
            case FIX_GC_CPUM_OFF:
            {
                uint32_t offCPUM = *u.pu32++;
                Assert(offCPUM < sizeof(pVM->cpum));
                *uSrc.pu32 = (uint32_t)(VM_GUEST_ADDR(pVM, &pVM->cpum) + offCPUM);
                break;
            }

            /*
             * Make 32-bit GC pointer given VM offset.
             */
            case FIX_GC_VM_OFF:
            {
                uint32_t offVM = *u.pu32++;
                Assert(offVM < sizeof(VM));
                *uSrc.pu32 = (uint32_t)(VM_GUEST_ADDR(pVM, pVM) + offVM);
                break;
            }

            /*
             * Make 32-bit HC pointer given CPUM offset.
             */
            case FIX_HC_CPUM_OFF:
            {
                uint32_t offCPUM = *u.pu32++;
                Assert(offCPUM < sizeof(pVM->cpum));
                *uSrc.pu32 = (uint32_t)pVM->pVMR0 + RT_OFFSETOF(VM, cpum) + offCPUM;
                break;
            }

            /*
             * Make 32-bit R0 pointer given VM offset.
             */
            case FIX_HC_VM_OFF:
            {
                uint32_t offVM = *u.pu32++;
                Assert(offVM < sizeof(VM));
                *uSrc.pu32 = (uint32_t)pVM->pVMR0 + offVM;
                break;
            }

            /*
             * Store the 32-Bit CR3 (32-bit) for the intermediate memory context.
             */
            case FIX_INTER_32BIT_CR3:
            {

                *uSrc.pu32 = PGMGetInter32BitCR3(pVM);
                break;
            }

            /*
             * Store the PAE CR3 (32-bit) for the intermediate memory context.
             */
            case FIX_INTER_PAE_CR3:
            {

                *uSrc.pu32 = PGMGetInterPaeCR3(pVM);
                break;
            }

            /*
             * Store the AMD64 CR3 (32-bit) for the intermediate memory context.
             */
            case FIX_INTER_AMD64_CR3:
            {

                *uSrc.pu32 = PGMGetInterAmd64CR3(pVM);
                break;
            }

            /*
             * Store the 32-Bit CR3 (32-bit) for the hypervisor (shadow) memory context.
             */
            case FIX_HYPER_32BIT_CR3:
            {

                *uSrc.pu32 = PGMGetHyper32BitCR3(pVM);
                break;
            }

            /*
             * Store the PAE CR3 (32-bit) for the hypervisor (shadow) memory context.
             */
            case FIX_HYPER_PAE_CR3:
            {

                *uSrc.pu32 = PGMGetHyperPaeCR3(pVM);
                break;
            }

            /*
             * Store the AMD64 CR3 (32-bit) for the hypervisor (shadow) memory context.
             */
            case FIX_HYPER_AMD64_CR3:
            {

                *uSrc.pu32 = PGMGetHyperAmd64CR3(pVM);
                break;
            }

            /*
             * Store Hypervisor CS (16-bit).
             */
            case FIX_HYPER_CS:
            {
                *uSrc.pu16 = SelCS;
                break;
            }

            /*
             * Store Hypervisor DS (16-bit).
             */
            case FIX_HYPER_DS:
            {
                *uSrc.pu16 = SelDS;
                break;
            }

            /*
             * Store Hypervisor TSS (16-bit).
             */
            case FIX_HYPER_TSS:
            {
                *uSrc.pu16 = SelTSS;
                break;
            }

            /*
             * Store the 32-bit GC address of the 2nd dword of the TSS descriptor (in the GDT).
             */
            case FIX_GC_TSS_GDTE_DW2:
            {
                RTGCPTR GCPtr = GCPtrGDT + (SelTSS & ~7) + 4;
                *uSrc.pu32 = (uint32_t)GCPtr;
                break;
            }


            ///@todo case FIX_CR4_MASK:
            ///@todo case FIX_CR4_OSFSXR:

            /*
             * Insert relative jump to specified target it FXSAVE/FXRSTOR isn't supported by the cpu.
             */
            case FIX_NO_FXSAVE_JMP:
            {
                uint32_t offTrg = *u.pu32++;
                Assert(offTrg < pSwitcher->cbCode);
                if (!CPUMSupportsFXSR(pVM))
                {
                    *uSrc.pu8++ = 0xe9; /* jmp rel32 */
                    *uSrc.pu32++ = offTrg - (offSrc + 5);
                }
                else
                {
                    *uSrc.pu8++ = *((uint8_t *)pSwitcher->pvCode + offSrc);
                    *uSrc.pu32++ = *(uint32_t *)((uint8_t *)pSwitcher->pvCode + offSrc + 1);
                }
                break;
            }

            /*
             * Insert relative jump to specified target it SYSENTER isn't used by the host.
             */
            case FIX_NO_SYSENTER_JMP:
            {
                uint32_t offTrg = *u.pu32++;
                Assert(offTrg < pSwitcher->cbCode);
                if (!CPUMIsHostUsingSysEnter(pVM))
                {
                    *uSrc.pu8++ = 0xe9; /* jmp rel32 */
                    *uSrc.pu32++ = offTrg - (offSrc + 5);
                }
                else
                {
                    *uSrc.pu8++ = *((uint8_t *)pSwitcher->pvCode + offSrc);
                    *uSrc.pu32++ = *(uint32_t *)((uint8_t *)pSwitcher->pvCode + offSrc + 1);
                }
                break;
            }

            /*
             * Insert relative jump to specified target it SYSENTER isn't used by the host.
             */
            case FIX_NO_SYSCALL_JMP:
            {
                uint32_t offTrg = *u.pu32++;
                Assert(offTrg < pSwitcher->cbCode);
                if (!CPUMIsHostUsingSysEnter(pVM))
                {
                    *uSrc.pu8++ = 0xe9; /* jmp rel32 */
                    *uSrc.pu32++ = offTrg - (offSrc + 5);
                }
                else
                {
                    *uSrc.pu8++ = *((uint8_t *)pSwitcher->pvCode + offSrc);
                    *uSrc.pu32++ = *(uint32_t *)((uint8_t *)pSwitcher->pvCode + offSrc + 1);
                }
                break;
            }

            /*
             * 32-bit HC pointer fixup to (HC) target within the code (32-bit offset).
             */
            case FIX_HC_32BIT:
            {
                uint32_t offTrg = *u.pu32++;
                Assert(offSrc < pSwitcher->cbCode);
                Assert(offTrg - pSwitcher->offHCCode0 < pSwitcher->cbHCCode0 || offTrg - pSwitcher->offHCCode1 < pSwitcher->cbHCCode1);
                *uSrc.pu32 = R0PtrCode + offTrg;
                break;
            }

#if defined(RT_ARCH_AMD64) || defined(VBOX_WITH_HYBIRD_32BIT_KERNEL)
            /*
             * 64-bit HC pointer fixup to (HC) target within the code (32-bit offset).
             */
            case FIX_HC_64BIT:
            {
                uint32_t offTrg = *u.pu32++;
                Assert(offSrc < pSwitcher->cbCode);
                Assert(offTrg - pSwitcher->offHCCode0 < pSwitcher->cbHCCode0 || offTrg - pSwitcher->offHCCode1 < pSwitcher->cbHCCode1);
                *uSrc.pu64 = R0PtrCode + offTrg;
                break;
            }

            /*
             * 64-bit HC Code Selector (no argument).
             */
            case FIX_HC_64BIT_CS:
            {
                Assert(offSrc < pSwitcher->cbCode);
#if defined(RT_OS_DARWIN) && defined(VBOX_WITH_HYBIRD_32BIT_KERNEL)
                *uSrc.pu16 = 0x80; /* KERNEL64_CS from i386/seg.h */
#else
                AssertFatalMsgFailed(("FIX_HC_64BIT_CS not implemented for this host\n"));
#endif
                break;
            }

            /*
             * 64-bit HC pointer to the CPUM instance data (no argument).
             */
            case FIX_HC_64BIT_CPUM:
            {
                Assert(offSrc < pSwitcher->cbCode);
                *uSrc.pu64 = pVM->pVMR0 + RT_OFFSETOF(VM, cpum);
                break;
            }
#endif

            /*
             * 32-bit ID pointer to (ID) target within the code (32-bit offset).
             */
            case FIX_ID_32BIT:
            {
                uint32_t offTrg = *u.pu32++;
                Assert(offSrc < pSwitcher->cbCode);
                Assert(offTrg - pSwitcher->offIDCode0 < pSwitcher->cbIDCode0 || offTrg - pSwitcher->offIDCode1 < pSwitcher->cbIDCode1);
                *uSrc.pu32 = u32IDCode + offTrg;
                break;
            }

            /*
             * 64-bit ID pointer to (ID) target within the code (32-bit offset).
             */
            case FIX_ID_64BIT:
            {
                uint32_t offTrg = *u.pu32++;
                Assert(offSrc < pSwitcher->cbCode);
                Assert(offTrg - pSwitcher->offIDCode0 < pSwitcher->cbIDCode0 || offTrg - pSwitcher->offIDCode1 < pSwitcher->cbIDCode1);
                *uSrc.pu64 = u32IDCode + offTrg;
                break;
            }

            /*
             * Far 16:32 ID pointer to 64-bit mode (ID) target within the code (32-bit offset).
             */
            case FIX_ID_FAR32_TO_64BIT_MODE:
            {
                uint32_t offTrg = *u.pu32++;
                Assert(offSrc < pSwitcher->cbCode);
                Assert(offTrg - pSwitcher->offIDCode0 < pSwitcher->cbIDCode0 || offTrg - pSwitcher->offIDCode1 < pSwitcher->cbIDCode1);
                *uSrc.pu32++ = u32IDCode + offTrg;
                *uSrc.pu16 = SelCS64;
                AssertRelease(SelCS64);
                break;
            }

#ifdef VBOX_WITH_NMI
            /*
             * 32-bit address to the APIC base.
             */
            case FIX_GC_APIC_BASE_32BIT:
            {
                *uSrc.pu32 = pVM->vmm.s.GCPtrApicBase;
                break;
            }
#endif

            default:
                AssertReleaseMsgFailed(("Unknown fixup %d in switcher %s\n", u8, pSwitcher->pszDesc));
                break;
        }
    }

#ifdef LOG_ENABLED
    /*
     * If Log2 is enabled disassemble the switcher code.
     *
     * The switcher code have 1-2 HC parts, 1 GC part and 0-2 ID parts.
     */
    if (LogIs2Enabled())
    {
        RTLogPrintf("*** Disassembly of switcher %d '%s' %#x bytes ***\n"
                    "   R0PtrCode   = %p\n"
                    "   pu8CodeR3   = %p\n"
                    "   GCPtrCode   = %VGv\n"
                    "   u32IDCode   = %08x\n"
                    "   pVMGC       = %VGv\n"
                    "   pCPUMGC     = %VGv\n"
                    "   pVMHC       = %p\n"
                    "   pCPUMHC     = %p\n"
                    "   GCPtrGDT    = %VGv\n"
                    "   InterCR3s   = %08x, %08x, %08x (32-Bit, PAE, AMD64)\n"
                    "   HyperCR3s   = %08x, %08x, %08x (32-Bit, PAE, AMD64)\n"
                    "   SelCS       = %04x\n"
                    "   SelDS       = %04x\n"
                    "   SelCS64     = %04x\n"
                    "   SelTSS      = %04x\n",
                    pSwitcher->enmType, pSwitcher->pszDesc, pSwitcher->cbCode,
                    R0PtrCode, pu8CodeR3, GCPtrCode, u32IDCode, VM_GUEST_ADDR(pVM, pVM),
                    VM_GUEST_ADDR(pVM, &pVM->cpum), pVM, &pVM->cpum,
                    GCPtrGDT,
                    PGMGetHyper32BitCR3(pVM), PGMGetHyperPaeCR3(pVM), PGMGetHyperAmd64CR3(pVM),
                    PGMGetInter32BitCR3(pVM), PGMGetInterPaeCR3(pVM), PGMGetInterAmd64CR3(pVM),
                    SelCS, SelDS, SelCS64, SelTSS);

        uint32_t offCode = 0;
        while (offCode < pSwitcher->cbCode)
        {
            /*
             * Figure out where this is.
             */
            const char *pszDesc = NULL;
            RTUINTPTR   uBase;
            uint32_t    cbCode;
            if (offCode - pSwitcher->offHCCode0 < pSwitcher->cbHCCode0)
            {
                pszDesc = "HCCode0";
                uBase   = R0PtrCode;
                offCode = pSwitcher->offHCCode0;
                cbCode  = pSwitcher->cbHCCode0;
            }
            else if (offCode - pSwitcher->offHCCode1 < pSwitcher->cbHCCode1)
            {
                pszDesc = "HCCode1";
                uBase   = R0PtrCode;
                offCode = pSwitcher->offHCCode1;
                cbCode  = pSwitcher->cbHCCode1;
            }
            else if (offCode - pSwitcher->offGCCode < pSwitcher->cbGCCode)
            {
                pszDesc = "GCCode";
                uBase   = GCPtrCode;
                offCode = pSwitcher->offGCCode;
                cbCode  = pSwitcher->cbGCCode;
            }
            else if (offCode - pSwitcher->offIDCode0 < pSwitcher->cbIDCode0)
            {
                pszDesc = "IDCode0";
                uBase   = u32IDCode;
                offCode = pSwitcher->offIDCode0;
                cbCode  = pSwitcher->cbIDCode0;
            }
            else if (offCode - pSwitcher->offIDCode1 < pSwitcher->cbIDCode1)
            {
                pszDesc = "IDCode1";
                uBase   = u32IDCode;
                offCode = pSwitcher->offIDCode1;
                cbCode  = pSwitcher->cbIDCode1;
            }
            else
            {
                RTLogPrintf("  %04x: %02x '%c' (nowhere)\n",
                            offCode, pu8CodeR3[offCode], isprint(pu8CodeR3[offCode]) ? pu8CodeR3[offCode] : ' ');
                offCode++;
                continue;
            }

            /*
             * Disassemble it.
             */
            RTLogPrintf("  %s: offCode=%#x cbCode=%#x\n", pszDesc, offCode, cbCode);
            DISCPUSTATE Cpu;

            memset(&Cpu, 0, sizeof(Cpu));
            Cpu.mode = CPUMODE_32BIT;
            while (cbCode > 0)
            {
                /* try label it */
                if (pSwitcher->offR0HostToGuest == offCode)
                    RTLogPrintf(" *R0HostToGuest:\n");
                if (pSwitcher->offGCGuestToHost == offCode)
                    RTLogPrintf(" *GCGuestToHost:\n");
                if (pSwitcher->offGCCallTrampoline == offCode)
                    RTLogPrintf(" *GCCallTrampoline:\n");
                if (pSwitcher->offGCGuestToHostAsm == offCode)
                    RTLogPrintf(" *GCGuestToHostAsm:\n");
                if (pSwitcher->offGCGuestToHostAsmHyperCtx == offCode)
                    RTLogPrintf(" *GCGuestToHostAsmHyperCtx:\n");
                if (pSwitcher->offGCGuestToHostAsmGuestCtx == offCode)
                    RTLogPrintf(" *GCGuestToHostAsmGuestCtx:\n");

                /* disas */
                uint32_t cbInstr = 0;
                char szDisas[256];
                if (RT_SUCCESS(DISInstr(&Cpu, (RTUINTPTR)pu8CodeR3 + offCode, uBase - (RTUINTPTR)pu8CodeR3, &cbInstr, szDisas)))
                    RTLogPrintf("  %04x: %s", offCode, szDisas); //for whatever reason szDisas includes '\n'.
                else
                {
                    RTLogPrintf("  %04x: %02x '%c'\n",
                                offCode, pu8CodeR3[offCode], isprint(pu8CodeR3[offCode]) ? pu8CodeR3[offCode] : ' ');
                    cbInstr = 1;
                }
                offCode += cbInstr;
                cbCode -= RT_MIN(cbInstr, cbCode);
            }
        }
    }
#endif
}


/**
 * Relocator for the 32-Bit to 32-Bit world switcher.
 */
DECLCALLBACK(void) vmmR3Switcher32BitTo32Bit_Relocate(PVM pVM, PVMMSWITCHERDEF pSwitcher, RTR0PTR R0PtrCode, uint8_t *pu8CodeR3, RTGCPTR GCPtrCode, uint32_t u32IDCode)
{
    vmmR3SwitcherGenericRelocate(pVM, pSwitcher, R0PtrCode, pu8CodeR3, GCPtrCode, u32IDCode,
                                 SELMGetHyperCS(pVM), SELMGetHyperDS(pVM), SELMGetHyperTSS(pVM), SELMGetHyperGDT(pVM), 0);
}


/**
 * Relocator for the 32-Bit to PAE world switcher.
 */
DECLCALLBACK(void) vmmR3Switcher32BitToPAE_Relocate(PVM pVM, PVMMSWITCHERDEF pSwitcher, RTR0PTR R0PtrCode, uint8_t *pu8CodeR3, RTGCPTR GCPtrCode, uint32_t u32IDCode)
{
    vmmR3SwitcherGenericRelocate(pVM, pSwitcher, R0PtrCode, pu8CodeR3, GCPtrCode, u32IDCode,
                                 SELMGetHyperCS(pVM), SELMGetHyperDS(pVM), SELMGetHyperTSS(pVM), SELMGetHyperGDT(pVM), 0);
}


/**
 * Relocator for the PAE to 32-Bit world switcher.
 */
DECLCALLBACK(void) vmmR3SwitcherPAETo32Bit_Relocate(PVM pVM, PVMMSWITCHERDEF pSwitcher, RTR0PTR R0PtrCode, uint8_t *pu8CodeR3, RTGCPTR GCPtrCode, uint32_t u32IDCode)
{
    vmmR3SwitcherGenericRelocate(pVM, pSwitcher, R0PtrCode, pu8CodeR3, GCPtrCode, u32IDCode,
                                 SELMGetHyperCS(pVM), SELMGetHyperDS(pVM), SELMGetHyperTSS(pVM), SELMGetHyperGDT(pVM), 0);
}


/**
 * Relocator for the PAE to PAE world switcher.
 */
DECLCALLBACK(void) vmmR3SwitcherPAEToPAE_Relocate(PVM pVM, PVMMSWITCHERDEF pSwitcher, RTR0PTR R0PtrCode, uint8_t *pu8CodeR3, RTGCPTR GCPtrCode, uint32_t u32IDCode)
{
    vmmR3SwitcherGenericRelocate(pVM, pSwitcher, R0PtrCode, pu8CodeR3, GCPtrCode, u32IDCode,
                                 SELMGetHyperCS(pVM), SELMGetHyperDS(pVM), SELMGetHyperTSS(pVM), SELMGetHyperGDT(pVM), 0);
}


/**
 * Relocator for the AMD64 to PAE world switcher.
 */
DECLCALLBACK(void) vmmR3SwitcherAMD64ToPAE_Relocate(PVM pVM, PVMMSWITCHERDEF pSwitcher, RTR0PTR R0PtrCode, uint8_t *pu8CodeR3, RTGCPTR GCPtrCode, uint32_t u32IDCode)
{
    vmmR3SwitcherGenericRelocate(pVM, pSwitcher, R0PtrCode, pu8CodeR3, GCPtrCode, u32IDCode,
                                 SELMGetHyperCS(pVM), SELMGetHyperDS(pVM), SELMGetHyperTSS(pVM), SELMGetHyperGDT(pVM), SELMGetHyperCS64(pVM));
}


/**
 * Gets the pointer to g_szRTAssertMsg1 in GC.
 * @returns Pointer to VMMGC::g_szRTAssertMsg1.
 *          Returns NULL if not present.
 * @param   pVM         The VM handle.
 */
VMMR3DECL(const char *) VMMR3GetGCAssertMsg1(PVM pVM)
{
    RTGCPTR32 GCPtr;
    int rc = PDMR3LdrGetSymbolRC(pVM, NULL, "g_szRTAssertMsg1", &GCPtr);
    if (VBOX_SUCCESS(rc))
        return (const char *)MMHyperGC2HC(pVM, GCPtr);
    return NULL;
}


/**
 * Gets the pointer to g_szRTAssertMsg2 in GC.
 * @returns Pointer to VMMGC::g_szRTAssertMsg2.
 *          Returns NULL if not present.
 * @param   pVM         The VM handle.
 */
VMMR3DECL(const char *) VMMR3GetGCAssertMsg2(PVM pVM)
{
    RTGCPTR32 GCPtr;
    int rc = PDMR3LdrGetSymbolRC(pVM, NULL, "g_szRTAssertMsg2", &GCPtr);
    if (VBOX_SUCCESS(rc))
        return (const char *)MMHyperGC2HC(pVM, GCPtr);
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
     * Note! See not in vmmR3Load.
     */
    SSMR3PutRCPtr(pSSM, pVM->vmm.s.pbEMTStackBottomRC);
    RTRCPTR RCPtrESP = CPUMGetHyperESP(pVM);
    AssertMsg(pVM->vmm.s.pbEMTStackBottomRC - RCPtrESP <= VMM_STACK_SIZE, ("Bottom %RRv ESP=%RRv\n", pVM->vmm.s.pbEMTStackBottomRC, RCPtrESP));
    SSMR3PutRCPtr(pSSM, RCPtrESP);
    SSMR3PutMem(pSSM, pVM->vmm.s.pbEMTStackR3, VMM_STACK_SIZE);
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
    if (VBOX_FAILURE(rc))
        return rc;

    /* restore the stack.  */
    SSMR3GetMem(pSSM, pVM->vmm.s.pbEMTStackR3, VMM_STACK_SIZE);

    /* terminator */
    uint32_t u32;
    rc = SSMR3GetU32(pSSM, &u32);
    if (VBOX_FAILURE(rc))
        return rc;
    if (u32 != ~0U)
    {
        AssertMsgFailed(("u32=%#x\n", u32));
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    }
    return VINF_SUCCESS;
}


/**
 * Selects the switcher to be used for switching to GC.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   enmSwitcher     The new switcher.
 * @remark  This function may be called before the VMM is initialized.
 */
VMMR3DECL(int) VMMR3SelectSwitcher(PVM pVM, VMMSWITCHER enmSwitcher)
{
    /*
     * Validate input.
     */
    if (    enmSwitcher < VMMSWITCHER_INVALID
        ||  enmSwitcher >= VMMSWITCHER_MAX)
    {
        AssertMsgFailed(("Invalid input enmSwitcher=%d\n", enmSwitcher));
        return VERR_INVALID_PARAMETER;
    }

    /* Do nothing if the switcher is disabled. */
    if (pVM->vmm.s.fSwitcherDisabled)
        return VINF_SUCCESS;

    /*
     * Select the new switcher.
     */
    PVMMSWITCHERDEF pSwitcher = s_apSwitchers[enmSwitcher];
    if (pSwitcher)
    {
        Log(("VMMR3SelectSwitcher: enmSwitcher %d -> %d %s\n", pVM->vmm.s.enmSwitcher, enmSwitcher, pSwitcher->pszDesc));
        pVM->vmm.s.enmSwitcher = enmSwitcher;

        RTR0PTR     pbCodeR0 = (RTR0PTR)pVM->vmm.s.pvCoreCodeR0 + pVM->vmm.s.aoffSwitchers[enmSwitcher]; /** @todo fix the pvCoreCodeR0 type */
        pVM->vmm.s.pfnHostToGuestR0 = pbCodeR0 + pSwitcher->offR0HostToGuest;

        RTGCPTR     GCPtr = pVM->vmm.s.pvCoreCodeRC + pVM->vmm.s.aoffSwitchers[enmSwitcher];
        pVM->vmm.s.pfnGuestToHostRC         = GCPtr + pSwitcher->offGCGuestToHost;
        pVM->vmm.s.pfnCallTrampolineRC      = GCPtr + pSwitcher->offGCCallTrampoline;
        pVM->pfnVMMGCGuestToHostAsm         = GCPtr + pSwitcher->offGCGuestToHostAsm;
        pVM->pfnVMMGCGuestToHostAsmHyperCtx = GCPtr + pSwitcher->offGCGuestToHostAsmHyperCtx;
        pVM->pfnVMMGCGuestToHostAsmGuestCtx = GCPtr + pSwitcher->offGCGuestToHostAsmGuestCtx;
        return VINF_SUCCESS;
    }
    return VERR_NOT_IMPLEMENTED;
}

/**
 * Disable the switcher logic permanently.
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 */
VMMR3DECL(int) VMMR3DisableSwitcher(PVM pVM)
{
/** @todo r=bird: I would suggest that we create a dummy switcher which just does something like:
 * @code
 *       mov eax, VERR_INTERNAL_ERROR
 *       ret
 * @endcode
 * And then check for fSwitcherDisabled in VMMR3SelectSwitcher() in order to prevent it from being removed.
 */
    pVM->vmm.s.fSwitcherDisabled = true;
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
 * Suspends the the CPU yielder.
 *
 * @param   pVM             The VM handle.
 */
VMMR3DECL(void) VMMR3YieldSuspend(PVM pVM)
{
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
 * Stops the the CPU yielder.
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
     * This really needs some careful tuning. While we shouldn't be too gready since
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
 * Acquire global VM lock.
 *
 * @returns VBox status code
 * @param   pVM         The VM to operate on.
 *
 * @remarks The global VMM lock isn't really used for anything any longer.
 */
VMMR3DECL(int) VMMR3Lock(PVM pVM)
{
    return RTCritSectEnter(&pVM->vmm.s.CritSectVMLock);
}


/**
 * Release global VM lock.
 *
 * @returns VBox status code
 * @param   pVM         The VM to operate on.
 *
 * @remarks The global VMM lock isn't really used for anything any longer.
 */
VMMR3DECL(int) VMMR3Unlock(PVM pVM)
{
    return RTCritSectLeave(&pVM->vmm.s.CritSectVMLock);
}


/**
 * Return global VM lock owner.
 *
 * @returns Thread id of owner.
 * @returns NIL_RTTHREAD if no owner.
 * @param   pVM         The VM to operate on.
 *
 * @remarks The global VMM lock isn't really used for anything any longer.
 */
VMMR3DECL(RTNATIVETHREAD) VMMR3LockGetOwner(PVM pVM)
{
    return RTCritSectGetOwner(&pVM->vmm.s.CritSectVMLock);
}


/**
 * Checks if the current thread is the owner of the global VM lock.
 *
 * @returns true if owner.
 * @returns false if not owner.
 * @param   pVM         The VM to operate on.
 *
 * @remarks The global VMM lock isn't really used for anything any longer.
 */
VMMR3DECL(bool) VMMR3LockIsOwner(PVM pVM)
{
    return RTCritSectIsOwner(&pVM->vmm.s.CritSectVMLock);
}


/**
 * Executes guest code in the raw-mode context.
 *
 * @param   pVM         VM handle.
 */
VMMR3DECL(int) VMMR3RawRunGC(PVM pVM)
{
    Log2(("VMMR3RawRunGC: (cs:eip=%04x:%08x)\n", CPUMGetGuestCS(pVM), CPUMGetGuestEIP(pVM)));

    /*
     * Set the EIP and ESP.
     */
    CPUMSetHyperEIP(pVM, CPUMGetGuestEFlags(pVM) & X86_EFL_VM
                    ? pVM->vmm.s.pfnCPUMRCResumeGuestV86
                    : pVM->vmm.s.pfnCPUMRCResumeGuest);
    CPUMSetHyperESP(pVM, pVM->vmm.s.pbEMTStackBottomRC);

    /*
     * We hide log flushes (outer) and hypervisor interrupts (inner).
     */
    for (;;)
    {
        int rc;
        do
        {
#ifdef NO_SUPCALLR0VMM
            rc = VERR_GENERAL_FAILURE;
#else
            rc = SUPCallVMMR0Fast(pVM->pVMR0, VMMR0_DO_RAW_RUN);
            if (RT_LIKELY(rc == VINF_SUCCESS))
                rc = pVM->vmm.s.iLastGZRc;
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
            Log2(("VMMR3RawRunGC: returns %Vrc (cs:eip=%04x:%08x)\n", rc, CPUMGetGuestCS(pVM), CPUMGetGuestEIP(pVM)));
            return rc;
        }
        rc = vmmR3ServiceCallHostRequest(pVM);
        if (VBOX_FAILURE(rc))
            return rc;
        /* Resume GC */
    }
}


/**
 * Executes guest code (Intel VT-x and AMD-V).
 *
 * @param   pVM         VM handle.
 */
VMMR3DECL(int) VMMR3HwAccRunGC(PVM pVM)
{
    Log2(("VMMR3HwAccRunGC: (cs:eip=%04x:%08x)\n", CPUMGetGuestCS(pVM), CPUMGetGuestEIP(pVM)));

    for (;;)
    {
        int rc;
        do
        {
#ifdef NO_SUPCALLR0VMM
            rc = VERR_GENERAL_FAILURE;
#else
            rc = SUPCallVMMR0Fast(pVM->pVMR0, VMMR0_DO_HWACC_RUN);
            if (RT_LIKELY(rc == VINF_SUCCESS))
                rc = pVM->vmm.s.iLastGZRc;
#endif
        } while (rc == VINF_EM_RAW_INTERRUPT_HYPER);

#ifdef LOG_ENABLED
        /*
         * Flush the log
         */
        PVMMR0LOGGER pR0LoggerR3 = pVM->vmm.s.pR0LoggerR3;
        if (    pR0LoggerR3
            &&  pR0LoggerR3->Logger.offScratch > 0)
            RTLogFlushToLogger(&pR0LoggerR3->Logger, NULL);
#endif /* !LOG_ENABLED */
        if (rc != VINF_VMM_CALL_HOST)
        {
            Log2(("VMMR3HwAccRunGC: returns %Vrc (cs:eip=%04x:%08x)\n", rc, CPUMGetGuestCS(pVM), CPUMGetGuestEIP(pVM)));
            return rc;
        }
        rc = vmmR3ServiceCallHostRequest(pVM);
        if (VBOX_FAILURE(rc))
            return rc;
        /* Resume R0 */
    }
}

/**
 * Calls GC a function.
 *
 * @param   pVM         The VM handle.
 * @param   GCPtrEntry  The GC function address.
 * @param   cArgs       The number of arguments in the ....
 * @param   ...         Arguments to the function.
 */
VMMR3DECL(int) VMMR3CallGC(PVM pVM, RTRCPTR GCPtrEntry, unsigned cArgs, ...)
{
    va_list args;
    va_start(args, cArgs);
    int rc = VMMR3CallGCV(pVM, GCPtrEntry, cArgs, args);
    va_end(args);
    return rc;
}


/**
 * Calls GC a function.
 *
 * @param   pVM         The VM handle.
 * @param   GCPtrEntry  The GC function address.
 * @param   cArgs       The number of arguments in the ....
 * @param   args        Arguments to the function.
 */
VMMR3DECL(int) VMMR3CallGCV(PVM pVM, RTRCPTR GCPtrEntry, unsigned cArgs, va_list args)
{
    Log2(("VMMR3CallGCV: GCPtrEntry=%VRv cArgs=%d\n", GCPtrEntry, cArgs));

    /*
     * Setup the call frame using the trampoline.
     */
    CPUMHyperSetCtxCore(pVM, NULL);
    memset(pVM->vmm.s.pbEMTStackR3, 0xaa, VMM_STACK_SIZE); /* Clear the stack. */
    CPUMSetHyperESP(pVM, pVM->vmm.s.pbEMTStackBottomRC - cArgs * sizeof(RTGCUINTPTR32));
    PRTGCUINTPTR32 pFrame = (PRTGCUINTPTR32)(pVM->vmm.s.pbEMTStackR3 + VMM_STACK_SIZE) - cArgs;
    int i = cArgs;
    while (i-- > 0)
        *pFrame++ = va_arg(args, RTGCUINTPTR32);

    CPUMPushHyper(pVM, cArgs * sizeof(RTGCUINTPTR32));                          /* stack frame size */
    CPUMPushHyper(pVM, GCPtrEntry);                                             /* what to call */
    CPUMSetHyperEIP(pVM, pVM->vmm.s.pfnCallTrampolineRC);

    /*
     * We hide log flushes (outer) and hypervisor interrupts (inner).
     */
    for (;;)
    {
        int rc;
        do
        {
#ifdef NO_SUPCALLR0VMM
            rc = VERR_GENERAL_FAILURE;
#else
            rc = SUPCallVMMR0Fast(pVM->pVMR0, VMMR0_DO_RAW_RUN);
            if (RT_LIKELY(rc == VINF_SUCCESS))
                rc = pVM->vmm.s.iLastGZRc;
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
            VMMR3FatalDump(pVM, rc);
        if (rc != VINF_VMM_CALL_HOST)
        {
            Log2(("VMMR3CallGCV: returns %Vrc (cs:eip=%04x:%08x)\n", rc, CPUMGetGuestCS(pVM), CPUMGetGuestEIP(pVM)));
            return rc;
        }
        rc = vmmR3ServiceCallHostRequest(pVM);
        if (VBOX_FAILURE(rc))
            return rc;
    }
}


/**
 * Resumes executing hypervisor code when interrupted by a queue flush or a
 * debug event.
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 */
VMMR3DECL(int) VMMR3ResumeHyper(PVM pVM)
{
    Log(("VMMR3ResumeHyper: eip=%VGv esp=%VGv\n", CPUMGetHyperEIP(pVM), CPUMGetHyperESP(pVM)));

    /*
     * We hide log flushes (outer) and hypervisor interrupts (inner).
     */
    for (;;)
    {
        int rc;
        do
        {
#ifdef NO_SUPCALLR0VMM
            rc = VERR_GENERAL_FAILURE;
#else
            rc = SUPCallVMMR0Fast(pVM->pVMR0, VMMR0_DO_RAW_RUN);
            if (RT_LIKELY(rc == VINF_SUCCESS))
                rc = pVM->vmm.s.iLastGZRc;
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
            VMMR3FatalDump(pVM, rc);
        if (rc != VINF_VMM_CALL_HOST)
        {
            Log(("VMMR3ResumeHyper: returns %Vrc\n", rc));
            return rc;
        }
        rc = vmmR3ServiceCallHostRequest(pVM);
        if (VBOX_FAILURE(rc))
            return rc;
    }
}


/**
 * Service a call to the ring-3 host code.
 *
 * @returns VBox status code.
 * @param   pVM     VM handle.
 * @remark  Careful with critsects.
 */
static int vmmR3ServiceCallHostRequest(PVM pVM)
{
    switch (pVM->vmm.s.enmCallHostOperation)
    {
        /*
         * Acquire the PDM lock.
         */
        case VMMCALLHOST_PDM_LOCK:
        {
            pVM->vmm.s.rcCallHost = PDMR3LockCall(pVM);
            break;
        }

        /*
         * Flush a PDM queue.
         */
        case VMMCALLHOST_PDM_QUEUE_FLUSH:
        {
            PDMR3QueueFlushWorker(pVM, NULL);
            pVM->vmm.s.rcCallHost = VINF_SUCCESS;
            break;
        }

        /*
         * Grow the PGM pool.
         */
        case VMMCALLHOST_PGM_POOL_GROW:
        {
            pVM->vmm.s.rcCallHost = PGMR3PoolGrow(pVM);
            break;
        }

        /*
         * Maps an page allocation chunk into ring-3 so ring-0 can use it.
         */
        case VMMCALLHOST_PGM_MAP_CHUNK:
        {
            pVM->vmm.s.rcCallHost = PGMR3PhysChunkMap(pVM, pVM->vmm.s.u64CallHostArg);
            break;
        }

        /*
         * Allocates more handy pages.
         */
        case VMMCALLHOST_PGM_ALLOCATE_HANDY_PAGES:
        {
            pVM->vmm.s.rcCallHost = PGMR3PhysAllocateHandyPages(pVM);
            break;
        }
#ifndef VBOX_WITH_NEW_PHYS_CODE

        case VMMCALLHOST_PGM_RAM_GROW_RANGE:
        {
            const RTGCPHYS GCPhys = pVM->vmm.s.u64CallHostArg;
            pVM->vmm.s.rcCallHost = PGM3PhysGrowRange(pVM, &GCPhys);
            break;
        }
#endif

        /*
         * Acquire the PGM lock.
         */
        case VMMCALLHOST_PGM_LOCK:
        {
            pVM->vmm.s.rcCallHost = PGMR3LockCall(pVM);
            break;
        }

        /*
         * Flush REM handler notifications.
         */
        case VMMCALLHOST_REM_REPLAY_HANDLER_NOTIFICATIONS:
        {
            REMR3ReplayHandlerNotifications(pVM);
            break;
        }

        /*
         * This is a noop. We just take this route to avoid unnecessary
         * tests in the loops.
         */
        case VMMCALLHOST_VMM_LOGGER_FLUSH:
            break;

        /*
         * Set the VM error message.
         */
        case VMMCALLHOST_VM_SET_ERROR:
            VMR3SetErrorWorker(pVM);
            break;

        /*
         * Set the VM runtime error message.
         */
        case VMMCALLHOST_VM_SET_RUNTIME_ERROR:
            VMR3SetRuntimeErrorWorker(pVM);
            break;

        /*
         * Signal a ring 0 hypervisor assertion.
         * Cancel the longjmp operation that's in progress.
         */
        case VMMCALLHOST_VM_R0_ASSERTION:
            pVM->vmm.s.enmCallHostOperation = VMMCALLHOST_INVALID;
            pVM->vmm.s.CallHostR0JmpBuf.fInRing3Call = false;
#ifdef RT_ARCH_X86
            pVM->vmm.s.CallHostR0JmpBuf.eip = 0;
#else
            pVM->vmm.s.CallHostR0JmpBuf.rip = 0;
#endif
            LogRel((pVM->vmm.s.szRing0AssertMsg1));
            LogRel((pVM->vmm.s.szRing0AssertMsg2));
            return VERR_VMM_RING0_ASSERTION;

        default:
            AssertMsgFailed(("enmCallHostOperation=%d\n", pVM->vmm.s.enmCallHostOperation));
            return VERR_INTERNAL_ERROR;
    }

    pVM->vmm.s.enmCallHostOperation = VMMCALLHOST_INVALID;
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
    const uint32_t fForcedActions = pVM->fForcedActions;

    pHlp->pfnPrintf(pHlp, "Forced action Flags: %#RX32", fForcedActions);

    /* show the flag mnemonics  */
    int c = 0;
    uint32_t f = fForcedActions;
#define PRINT_FLAG(flag) do { \
        if (f & (flag)) \
        { \
            static const char *s_psz = #flag; \
            if (!(c % 6)) \
                pHlp->pfnPrintf(pHlp, "%s\n    %s", c ? "," : "", s_psz + 6); \
            else \
                pHlp->pfnPrintf(pHlp, ", %s", s_psz + 6); \
            c++; \
            f &= ~(flag); \
        } \
    } while (0)
    PRINT_FLAG(VM_FF_INTERRUPT_APIC);
    PRINT_FLAG(VM_FF_INTERRUPT_PIC);
    PRINT_FLAG(VM_FF_TIMER);
    PRINT_FLAG(VM_FF_PDM_QUEUES);
    PRINT_FLAG(VM_FF_PDM_DMA);
    PRINT_FLAG(VM_FF_PDM_CRITSECT);
    PRINT_FLAG(VM_FF_DBGF);
    PRINT_FLAG(VM_FF_REQUEST);
    PRINT_FLAG(VM_FF_TERMINATE);
    PRINT_FLAG(VM_FF_RESET);
    PRINT_FLAG(VM_FF_PGM_SYNC_CR3);
    PRINT_FLAG(VM_FF_PGM_SYNC_CR3_NON_GLOBAL);
    PRINT_FLAG(VM_FF_TRPM_SYNC_IDT);
    PRINT_FLAG(VM_FF_SELM_SYNC_TSS);
    PRINT_FLAG(VM_FF_SELM_SYNC_GDT);
    PRINT_FLAG(VM_FF_SELM_SYNC_LDT);
    PRINT_FLAG(VM_FF_INHIBIT_INTERRUPTS);
    PRINT_FLAG(VM_FF_CSAM_SCAN_PAGE);
    PRINT_FLAG(VM_FF_CSAM_PENDING_ACTION);
    PRINT_FLAG(VM_FF_TO_R3);
    PRINT_FLAG(VM_FF_DEBUG_SUSPEND);
    if (f)
        pHlp->pfnPrintf(pHlp, "%s\n    Unknown bits: %#RX32\n", c ? "," : "", f);
    else
        pHlp->pfnPrintf(pHlp, "\n");
#undef PRINT_FLAG

    /* the groups */
    c = 0;
#define PRINT_GROUP(grp) do { \
        if (fForcedActions & (grp)) \
        { \
            static const char *s_psz = #grp; \
            if (!(c % 5)) \
                pHlp->pfnPrintf(pHlp, "%s    %s", c ? ",\n" : "Groups:\n", s_psz + 6); \
            else \
                pHlp->pfnPrintf(pHlp, ", %s", s_psz + 6); \
            c++; \
        } \
    } while (0)
    PRINT_GROUP(VM_FF_EXTERNAL_SUSPENDED_MASK);
    PRINT_GROUP(VM_FF_EXTERNAL_HALTED_MASK);
    PRINT_GROUP(VM_FF_HIGH_PRIORITY_PRE_MASK);
    PRINT_GROUP(VM_FF_HIGH_PRIORITY_PRE_RAW_MASK);
    PRINT_GROUP(VM_FF_HIGH_PRIORITY_POST_MASK);
    PRINT_GROUP(VM_FF_NORMAL_PRIORITY_POST_MASK);
    PRINT_GROUP(VM_FF_NORMAL_PRIORITY_MASK);
    PRINT_GROUP(VM_FF_RESUME_GUEST_MASK);
    PRINT_GROUP(VM_FF_ALL_BUT_RAW_MASK);
    if (c)
        pHlp->pfnPrintf(pHlp, "\n");
#undef PRINT_GROUP
}

