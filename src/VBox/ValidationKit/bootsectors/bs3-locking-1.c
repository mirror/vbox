/* $Id$ */
/** @file
 * BS3Kit - bs3-locking-1, 16-bit C code.
 */

/*
 * Copyright (C) 2007-2021 Oracle Corporation
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
#include <bs3kit.h>
#include <iprt/asm-amd64-x86.h>

#include <VBox/VMMDevTesting.h>


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static struct
{
    const char * BS3_FAR    pszName;
    uint32_t                cInnerLoops;
    uint32_t                uCtrlWord;
} g_aLockingTests[] =
{
    {
        "Contention 500us/250us",
        2000 + 16384,
        500 | (UINT32_C(250) << VMMDEV_TESTING_LOCKED_WAIT_SHIFT)
    },
    {
        "Contention 100us/50us",
        10000 + 4096,
        100 | (UINT32_C(50)  << VMMDEV_TESTING_LOCKED_WAIT_SHIFT)
    },
    {
        "Contention 500us/250us poke",
        2000 + 16384,
        500 | (UINT32_C(250) << VMMDEV_TESTING_LOCKED_WAIT_SHIFT) | VMMDEV_TESTING_LOCKED_POKE
    },
    {
        "Contention 100us/50us poke",
        10000 + 4096,
        100 | (UINT32_C(50)  << VMMDEV_TESTING_LOCKED_WAIT_SHIFT) | VMMDEV_TESTING_LOCKED_POKE
    },
    {
        "Contention 500us/250us poke void",
        2000 + 16384,
        500 | (UINT32_C(250) << VMMDEV_TESTING_LOCKED_WAIT_SHIFT) | VMMDEV_TESTING_LOCKED_POKE | VMMDEV_TESTING_LOCKED_BUSY_SUCCESS
    },
    {
        "Contention 50us/25us poke void",
        20000 + 4096,
        50 | (UINT32_C(25)  << VMMDEV_TESTING_LOCKED_WAIT_SHIFT) | VMMDEV_TESTING_LOCKED_POKE | VMMDEV_TESTING_LOCKED_BUSY_SUCCESS
    },
};


BS3_DECL(void) Main_rm()
{
    uint64_t const  cNsPerTest = RT_NS_15SEC;
    unsigned        i;

    Bs3InitAll_rm();
    Bs3TestInit("bs3-locking-1");

    /*
     * Since this is a host-side test and we don't have raw-mode any more, we
     * just stay in raw-mode when doing the test.
     */
    for (i = 0; i < RT_ELEMENTS(g_aLockingTests); i++)
    {
        uint64_t const nsStart    = Bs3TestNow();
        uint64_t       cNsElapsed = 0;
        uint32_t       cTotal     = 0;
        uint32_t       j;

        Bs3TestSub(g_aLockingTests[i].pszName);
        ASMOutU32(VMMDEV_TESTING_IOPORT_LOCKED, g_aLockingTests[i].uCtrlWord);

        for (j = 0; j < _2M && cTotal < _1G; j++)
        {

            /* The inner loop should avoid calling Bs3TestNow too often, while not overshooting the . */
            unsigned iInner = (unsigned)g_aLockingTests[i].cInnerLoops;
            cTotal += iInner;
            while (iInner-- > 0)
                ASMInU32(VMMDEV_TESTING_IOPORT_LOCKED);

            cNsElapsed = Bs3TestNow() - nsStart;
            if (cNsElapsed >= cNsPerTest)
                break;
        }

        /* Disable locking. */
        ASMOutU32(VMMDEV_TESTING_IOPORT_LOCKED, 0);

        Bs3TestValue("Loops", cTotal, VMMDEV_TESTING_UNIT_OCCURRENCES);
        Bs3TestValue("Elapsed", cNsElapsed, VMMDEV_TESTING_UNIT_NS);
    }

    Bs3TestTerm();
}

