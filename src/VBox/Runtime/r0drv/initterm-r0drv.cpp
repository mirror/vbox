/* $Id$ */
/** @file
 * IPRT - Initialization & Termination, R0 Driver, Common.
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/initterm.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#ifndef IN_GUEST /* play safe for now */
# include "r0drv/mp-r0drv.h"
# include "r0drv/power-r0drv.h"
#endif

#include "internal/initterm.h"
#include "internal/thread.h"


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Count of current IPRT users.
 * In ring-0 several drivers / kmods / kexts / wossnames may share the
 * same runtime code. So, we need to keep count in order not to terminate
 * it prematurely. */
static int32_t volatile g_crtR0Users = 0;


/**
 * Initalizes the ring-0 driver runtime library.
 *
 * @returns iprt status code.
 * @param   fReserved       Flags reserved for the future.
 */
RTR0DECL(int) RTR0Init(unsigned fReserved)
{
    int rc;
    Assert(fReserved == 0);

    /*
     * The first user initializes it.
     * We rely on the module loader to ensure that there are no
     * initialization races should two modules share the IPRT.
     */
    if (ASMAtomicIncS32(&g_crtR0Users) != 1)
        return VINF_SUCCESS;

    rc = rtR0InitNative();
    if (RT_SUCCESS(rc))
    {
#if !defined(RT_OS_LINUX)
        rc = rtThreadInit();
#endif
        if (RT_SUCCESS(rc))
        {
#ifndef IN_GUEST /* play safe for now */
            rc = rtR0MpNotificationInit();
            if (RT_SUCCESS(rc))
                rc = rtR0PowerNotificationInit();
#endif
            if (RT_SUCCESS(rc))
                return rc;
        }

        rtR0TermNative();
    }
    return rc;
}


/**
 * Terminates the ring-0 driver runtime library.
 */
RTR0DECL(void) RTR0Term(void)
{
    /*
     * Last user does the cleanup.
     */
    int32_t cNewUsers = ASMAtomicDecS32(&g_crtR0Users);
    Assert(cNewUsers >= 0);
    if (cNewUsers != 0)
        return;

#if !defined(RT_OS_LINUX) && !defined(RT_OS_WINDOWS)
    rtThreadTerm();
#endif
#ifndef IN_GUEST /* play safe for now */
    rtR0PowerNotificationTerm();
    rtR0MpNotificationTerm();
#endif
    rtR0TermNative();
}

