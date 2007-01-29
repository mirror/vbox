/* $Id$ */
/** @file
 * VMM - Guest Context.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_VMM
#include <VBox/vmm.h>
#include "VMMInternal.h"
#include <VBox/vm.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/assert.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Default logger instance. */
extern "C" DECLIMPORT(RTLOGGERGC)   g_Logger;
extern "C" DECLIMPORT(RTLOGGERGC)   g_RelLogger;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int vmmGCTest(PVM pVM, unsigned uOperation, unsigned uArg);



/**
 * The GC entry point.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   uOperation  Which operation to execute (VMMGCOPERATION).
 * @param   uArg        Argument to that operation.
 */
VMMGCDECL(int) VMMGCEntry(PVM pVM, unsigned uOperation, unsigned uArg)
{
    /* todo */
    switch (uOperation)
    {
        /*
         * Init GC modules.
         */
        case VMMGC_DO_VMMGC_INIT:
        {
            Log(("VMMGCEntry: VMMGC_DO_VMMGC_INIT - uArg=%#x\n", uArg));
            /** @todo validate version. */
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
 * Internal GC logger worker: Flush logger.
 *
 * @returns VINF_SUCCESS.
 * @param   pLogger     The logger instance to flush.
 * @remark  This function must be exported!
 */
VMMGCDECL(int) vmmGCLoggerFlush(PRTLOGGERGC pLogger)
{
    PVM pVM = &g_VM;
    NOREF(pLogger);
    return VMMGCCallHost(pVM, VMMCALLHOST_VMM_LOGGER_FLUSH, 0);
}


/**
 * Switches from guest context to host context.
 *
 * @param   pVM         The VM handle.
 * @param   rc          The status code.
 */
VMMGCDECL(void) VMMGCGuestToHost(PVM pVM, int rc)
{
    pVM->vmm.s.pfnGCGuestToHost(rc);
}


/**
 * Calls the ring-3 host code.
 *
 * @returns VBox status code of the ring-3 call.
 * @param   pVM             The VM handle.
 * @param   enmOperation    The operation.
 * @param   uArg            The argument to the operation.
 */
VMMGCDECL(int) VMMGCCallHost(PVM pVM, VMMCALLHOST enmOperation, uint64_t uArg)
{
/** @todo profile this! */
    pVM->vmm.s.enmCallHostOperation = enmOperation;
    pVM->vmm.s.u64CallHostArg = uArg;
    pVM->vmm.s.rcCallHost = VERR_INTERNAL_ERROR;
    pVM->vmm.s.pfnGCGuestToHost(VINF_VMM_CALL_HOST);
    return pVM->vmm.s.rcCallHost;
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

