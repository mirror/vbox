/* $Id$ */
/** @file
 * innotek Portable Runtime - Assertions, generic RTAssertDoBreakpoint.
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/assert.h>
#include <iprt/env.h>
#include <iprt/string.h>

/** @def VBOX_RTASSERT_WITH_GDB
 * Enables the 'gdb' VBOX_ASSERT option.
 */
#if defined(DOXYGEN_RUNNING)
 || (   !defined(VBOX_RTASSERT_WITH_GDB) \
     && !defined(IN_GUEST) \
     && (!defined(RT_OS_OS2) && !defined(RT_OS_WINDOWS)))
# define VBOX_ASSERT_WITH_GDB
#endif

#ifdef VBOX_ASSERT_WITH_GDB
# include <iprt/process.h>
# include <iprt/path.h>
# include <iprt/thread.h>
#endif


RTDECL(bool)    RTAssertDoBreakpoint(void)
{
    /*
     * Check for the VBOX_ASSERT variable.
     */
    const char *psz = RTEnvGet("VBOX_ASSERT");

    /* not defined => default behaviour. */
    if (!psz)
        return true;

    /* 'breakpoint' means default behaviour. */
    if (!strcmp(psz, "breakpoint"))
        return true;

#ifdef VBOX_ASSERT_WITH_GDB
    /* 'gdb' - means try launch a gdb session in xterm. */
    if (!strcmp(psz, "gdb"))
    {
        /* Our PID. */
        char szPid[32];
        RTStrPrintf(szPid, sizeof(szPid), "%d", RTProcSelf());

        /* Try find a suitable terminal program. */
        const char *pszTerm = "/usr/bin/xterm"; /** @todo make this configurable */
        if (!RTPathExists(pszTerm))
        {
            pszTerm = "/usr/X11R6/bin/xterm";
            if (!RTPathExists(pszTerm))
            {
                pszTerm = "/usr/bin/gnome-terminal";
                if (!RTPathExists(pszTerm))
                    return true;
            }
        }

        /* Try spawn the process. */
        const char * apszArgs[] =
        {
            pszTerm,
            "-e",
            "/usr/bin/gdb",
            "program",
            szPid,
            NULL
        };
        RTPROCESS Process;
        int rc = RTProcCreate(apszArgs[0], &apszArgs[0], RTENV_DEFAULT, 0, &Process);
        if (RT_FAILURE(rc))
            return false;

        /* Wait for gdb to attach. */
        RTThreadSleep(15000);
        return true;
    }
#endif

    /* '*' - don't hit the breakpoint. */
    return false;
}

