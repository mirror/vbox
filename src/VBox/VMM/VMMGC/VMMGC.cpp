/* $Id$ */
/** @file
 * VMM - Raw-mode Context.
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
#include <VBox/trpm.h>
#include <VBox/pgm.h>
#include "VMMInternal.h"
#include <VBox/vm.h>
#include <VBox/sup.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/initterm.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Default logger instance. */
extern "C" DECLIMPORT(RTLOGGERRC)   g_Logger;
extern "C" DECLIMPORT(RTLOGGERRC)   g_RelLogger;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int vmmGCTest(PVM pVM, unsigned uOperation, unsigned uArg);
static DECLCALLBACK(int) vmmGCTestTmpPFHandler(PVM pVM, PCPUMCTXCORE pRegFrame);
static DECLCALLBACK(int) vmmGCTestTmpPFHandlerCorruptFS(PVM pVM, PCPUMCTXCORE pRegFrame);



/**
 * The GC entry point.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   uOperation  Which operation to execute (VMMGCOPERATION).
 * @param   uArg        Argument to that operation.
 */
VMMRCDECL(int) VMMGCEntry(PVM pVM, unsigned uOperation, unsigned uArg, ...)
{
    /* todo */
    switch (uOperation)
    {
        /*
         * Init RC modules.
         */
        case VMMGC_DO_VMMGC_INIT:
        {
            /*
             * Validate the svn revision (uArg).
             */
            if (uArg != VMMGetSvnRev())
                return VERR_VERSION_MISMATCH;

            /*
             * Initialize the runtime.
             * (The program timestamp is found in the elipsis.)
             */
            va_list va;
            va_start(va, uArg);
            uint64_t u64TS = va_arg(va, uint64_t);
            va_end(va);

            int rc = RTRCInit(u64TS);
            Log(("VMMGCEntry: VMMGC_DO_VMMGC_INIT - uArg=%u (svn revision) u64TS=%RX64; rc=%Rrc\n", uArg, u64TS, rc));
            AssertRCReturn(rc, rc);

            rc = PGMRegisterStringFormatTypes();
            AssertRCReturn(rc, rc);

            return VINF_SUCCESS;
        }

        /*
         * Testcase which is used to test interrupt forwarding.
         * It spins for a while with interrupts enabled.
         */
        case VMMGC_DO_TESTCASE_HYPER_INTERRUPT:
        {
            uint32_t volatile i = 0;
            ASMIntEnable();
            while (i < _2G32)
                i++;
            ASMIntDisable();
            return 0;
        }

        /*
         * Testcase which simply returns, this is used for
         * profiling of the switcher.
         */
        case VMMGC_DO_TESTCASE_NOP:
            return 0;

        /*
         * Testcase executes a privileged instruction to force a world switch. (in both SVM & VMX)
         */
        case VMMGC_DO_TESTCASE_HWACCM_NOP:
            ASMRdMsr_Low(MSR_IA32_SYSENTER_CS);
            return 0;

        /*
         * Delay for ~100us.
         */
        case VMMGC_DO_TESTCASE_INTERRUPT_MASKING:
        {
            uint64_t u64MaxTicks = (SUPGetCpuHzFromGIP(g_pSUPGlobalInfoPage) != ~(uint64_t)0
                                    ? SUPGetCpuHzFromGIP(g_pSUPGlobalInfoPage)
                                    : _2G)
                                   / 10000;
            uint64_t u64StartTSC = ASMReadTSC();
            uint64_t u64TicksNow;
            uint32_t volatile i = 0;

            do
            {
                /* waste some time and protect against getting stuck. */
                for (uint32_t volatile j = 0; j < 1000; j++, i++)
                    if (i > _2G32)
                        return VERR_GENERAL_FAILURE;

                /* check if we're done.*/
                u64TicksNow = ASMReadTSC() - u64StartTSC;
            } while (u64TicksNow < u64MaxTicks);

            return VINF_SUCCESS;
        }

        /*
         * Trap testcases and unknown operations.
         */
        default:
            if (    uOperation >= VMMGC_DO_TESTCASE_TRAP_FIRST
                &&  uOperation < VMMGC_DO_TESTCASE_TRAP_LAST)
                return vmmGCTest(pVM, uOperation, uArg);
            return VERR_INVALID_PARAMETER;
    }
}


/**
 * Internal RC logger worker: Flush logger.
 *
 * @returns VINF_SUCCESS.
 * @param   pLogger     The logger instance to flush.
 * @remark  This function must be exported!
 */
VMMRCDECL(int) vmmGCLoggerFlush(PRTLOGGERRC pLogger)
{
    PVM pVM = &g_VM;
    NOREF(pLogger);
    return VMMGCCallHost(pVM, VMMCALLHOST_VMM_LOGGER_FLUSH, 0);
}


/**
 * Flush logger if almost full.
 *
 * @param   pVM             The VM handle.
 */
VMMRCDECL(void) VMMGCLogFlushIfFull(PVM pVM)
{
    if (    pVM->vmm.s.pRCLoggerRC
        &&  pVM->vmm.s.pRCLoggerRC->offScratch >= (sizeof(pVM->vmm.s.pRCLoggerRC->achScratch)*3/4))
    {
        VMMGCCallHost(pVM, VMMCALLHOST_VMM_LOGGER_FLUSH, 0);
    }
}

/**
 * Disables the GC logger temporarily, restore with VMMGCLogRestore.
 *
 * @param   pVM             The VM handle.
 */
VMMRCDECL(bool) VMMGCLogDisable(PVM pVM)
{
    bool fLog = pVM->vmm.s.pRCLoggerRC
             && !(pVM->vmm.s.pRCLoggerRC->fFlags & RTLOGFLAGS_DISABLED);
    if (fLog)
        pVM->vmm.s.pRCLoggerRC->fFlags |= RTLOGFLAGS_DISABLED;
    return fLog;
}


/**
 * Restores the GC logger after a call to VMMGCLogDisable.
 *
 * @param   pVM             The VM handle.
 * @param   fLog            What VMMGCLogDisable returned.
 */
VMMRCDECL(void) VMMGCLogRestore(PVM pVM, bool fLog)
{
    if (fLog && pVM->vmm.s.pRCLoggerRC)
        pVM->vmm.s.pRCLoggerRC->fFlags &= ~RTLOGFLAGS_DISABLED;
}


/**
 * Switches from guest context to host context.
 *
 * @param   pVM         The VM handle.
 * @param   rc          The status code.
 */
VMMRCDECL(void) VMMGCGuestToHost(PVM pVM, int rc)
{
    pVM->vmm.s.pfnGuestToHostRC(rc);
}


/**
 * Calls the ring-3 host code.
 *
 * @returns VBox status code of the ring-3 call.
 * @param   pVM             The VM handle.
 * @param   enmOperation    The operation.
 * @param   uArg            The argument to the operation.
 */
VMMRCDECL(int) VMMGCCallHost(PVM pVM, VMMCALLHOST enmOperation, uint64_t uArg)
{
    PVMCPU pVCpu = VMMGetCpu0(pVM);

/** @todo profile this! */
    pVCpu->vmm.s.enmCallHostOperation = enmOperation;
    pVCpu->vmm.s.u64CallHostArg = uArg;
    pVCpu->vmm.s.rcCallHost = VERR_INTERNAL_ERROR;
    pVM->vmm.s.pfnGuestToHostRC(VINF_VMM_CALL_HOST);
    return pVCpu->vmm.s.rcCallHost;
}


/**
 * Execute the trap testcase.
 *
 * There is some common code here, that's why we're collecting them
 * like this. Odd numbered variation (uArg) are executed with write
 * protection (WP) enabled.
 *
 * @returns VINF_SUCCESS if it was a testcase setup up to continue and did so successfully.
 * @returns VERR_NOT_IMPLEMENTED if the testcase wasn't implemented.
 * @returns VERR_GENERAL_FAILURE if the testcase continued when it shouldn't.
 *
 * @param   pVM         The VM handle.
 * @param   uOperation  The testcase.
 * @param   uArg        The variation. See function description for odd / even details.
 *
 * @remark  Careful with the trap 08 testcase and WP, it will tripple
 *          fault the box if the TSS, the Trap8 TSS and the fault TSS
 *          GDTE are in pages which are read-only.
 *          See bottom of SELMR3Init().
 */
static int vmmGCTest(PVM pVM, unsigned uOperation, unsigned uArg)
{
    /*
     * Set up the testcase.
     */
#if 0
    switch (uOperation)
    {
        default:
            break;
    }
#endif

    /*
     * Enable WP if odd variation.
     */
    if (uArg & 1)
        vmmGCEnableWP();

    /*
     * Execute the testcase.
     */
    int rc = VERR_NOT_IMPLEMENTED;
    switch (uOperation)
    {
        //case VMMGC_DO_TESTCASE_TRAP_0:
        //case VMMGC_DO_TESTCASE_TRAP_1:
        //case VMMGC_DO_TESTCASE_TRAP_2:

        case VMMGC_DO_TESTCASE_TRAP_3:
        {
            if (uArg <= 1)
                rc = vmmGCTestTrap3();
            break;
        }

        //case VMMGC_DO_TESTCASE_TRAP_4:
        //case VMMGC_DO_TESTCASE_TRAP_5:
        //case VMMGC_DO_TESTCASE_TRAP_6:
        //case VMMGC_DO_TESTCASE_TRAP_7:

        case VMMGC_DO_TESTCASE_TRAP_8:
        {
#ifndef DEBUG_bird /** @todo dynamic check that this won't tripple fault... */
            if (uArg & 1)
                break;
#endif
            if (uArg <= 1)
                rc = vmmGCTestTrap8();
            break;
        }

        //VMMGC_DO_TESTCASE_TRAP_9,
        //VMMGC_DO_TESTCASE_TRAP_0A,
        //VMMGC_DO_TESTCASE_TRAP_0B,
        //VMMGC_DO_TESTCASE_TRAP_0C,

        case VMMGC_DO_TESTCASE_TRAP_0D:
        {
            if (uArg <= 1)
                rc = vmmGCTestTrap0d();
            break;
        }

        case VMMGC_DO_TESTCASE_TRAP_0E:
        {
            if (uArg <= 1)
                rc = vmmGCTestTrap0e();
            else if (uArg == 2 || uArg == 4)
            {
                /*
                 * Test the use of a temporary #PF handler.
                 */
                rc = TRPMGCSetTempHandler(pVM, X86_XCPT_PF, uArg != 4 ? vmmGCTestTmpPFHandler : vmmGCTestTmpPFHandlerCorruptFS);
                if (RT_SUCCESS(rc))
                {
                    rc = vmmGCTestTrap0e();

                    /* in case it didn't fire. */
                    int rc2 = TRPMGCSetTempHandler(pVM, X86_XCPT_PF, NULL);
                    if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
                        rc = rc2;
                }
            }
            break;
        }
    }

    /*
     * Re-enable WP.
     */
    if (uArg & 1)
        vmmGCDisableWP();

    return rc;
}


/**
 * Temporary #PF trap handler for the #PF test case.
 *
 * @returns VBox status code (appropriate for GC return).
 *          In this context RT_SUCCESS means to restart the instruction.
 * @param   pVM         VM handle.
 * @param   pRegFrame   Trap register frame.
 */
static DECLCALLBACK(int) vmmGCTestTmpPFHandler(PVM pVM, PCPUMCTXCORE pRegFrame)
{
    if (pRegFrame->eip == (uintptr_t)vmmGCTestTrap0e_FaultEIP)
    {
        pRegFrame->eip = (uintptr_t)vmmGCTestTrap0e_ResumeEIP;
        return VINF_SUCCESS;
    }
    return VERR_INTERNAL_ERROR;
}


/**
 * Temporary #PF trap handler for the #PF test case, this one messes up the fs selector.
 *
 * @returns VBox status code (appropriate for GC return).
 *          In this context RT_SUCCESS means to restart the instruction.
 * @param   pVM         VM handle.
 * @param   pRegFrame   Trap register frame.
 */
static DECLCALLBACK(int) vmmGCTestTmpPFHandlerCorruptFS(PVM pVM, PCPUMCTXCORE pRegFrame)
{
    int rc = vmmGCTestTmpPFHandler(pVM, pRegFrame);
    pRegFrame->fs = 0x30;
    return rc;
}

