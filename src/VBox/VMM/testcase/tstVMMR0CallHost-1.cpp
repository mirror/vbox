/* $Id$ */
/** @file
 * Testcase for the VMMR0JMPBUF operations.
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
#include <iprt/initterm.h>
#include <iprt/string.h>
#include <iprt/stream.h>
#include <iprt/alloca.h>
#include <VBox/err.h>

#define IN_VMM_R0
#define IN_RING0 /* pretent we're in Ring-0 to get the prototypes. */
#include <VBox/vmm.h>
#include "VMMInternal.h"



/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The jump buffer. */
static VMMR0JMPBUF          g_Jmp;
/** The number of jumps we've done. */
static unsigned volatile    g_cJmps;
/** The saved stack. */
static uint8_t              g_Stack[8192];


int foo(int i, int iZero, int iMinusOne)
{
    /* allocate a buffer which we fill up to the end. */
    size_t cb = (i % 5555) + 32;
    char  *pv = (char *)alloca(cb);
    RTStrPrintf(pv, cb, "i=%d%*s\n", i, cb, "");

    /* Do long jmps every 7th time */
    if ((i % 7) == 0)
    {
        g_cJmps++;
        int rc = vmmR0CallHostLongJmp(&g_Jmp, 42);
        if (!rc)
            return i + 10000;
        return -1;
    }
    return i;
}


DECLCALLBACK(int) tst2(intptr_t i, intptr_t i2)
{
    if (i < 0 || i > 8192)
    {
        RTPrintf("tstVMMR0CallHost-1: FAILURE - i=%d is out of range [0..8192]\n", i);
        return 1;
    }
    if (i2 != 0)
    {
        RTPrintf("tstVMMR0CallHost-1: FAILURE - i2=%d is out of range [0]\n", i2);
        return 1;
    }
    int iExpect = (i % 7) == 0 ? i + 10000 : i;
    int rc = foo(i, 0, -1);
    if (rc != iExpect)
    {
        RTPrintf("tstVMMR0CallHost-1: FAILURE - i=%d rc=%d expected=%d\n", i, rc, iExpect);
        return 1;
    }
    return 0;
}

int tst(int iFrom, int iTo, int iInc)
{
    g_cJmps = 0;
    for (int i = iFrom; i != iTo; i += iInc)
    {
        int rc = vmmR0CallHostSetJmp(&g_Jmp, (PFNVMMR0SETJMP)tst2, (PVM)i, 0);
        if (rc != 0 && rc != 42)
        {
            RTPrintf("tstVMMR0CallHost-1: FAILURE - i=%d rc=%d setjmp\n", i, rc);
            return 1;
        }
    }
    if (!g_cJmps)
    {
        RTPrintf("tstVMMR0CallHost-1: FAILURE - no jumps!\n");
        return 1;
    }
    return 0;
}


int main()
{
    /*
     * Init.
     */
    RTR3Init();
    g_Jmp.pvSavedStack = (RTR0PTR)&g_Stack[0];

    /*
     * Try about 1000 long jumps with increasing stack size..
     */
    RTPrintf("tstVMMR0CallHost-1: Testing 1\n");
    int rc = tst(0, 7000, 1);
    if (!rc)
    {
        RTPrintf("tstVMMR0CallHost-1: Testing 2\n");
        rc = tst(7599, 0, -1);
    }

    if (!rc)
        RTPrintf("tstVMMR0CallHost-1: SUCCESS\n");
    else
        RTPrintf("tstVMMR0CallHost-1: FAILED\n");
    return !!rc;
}
