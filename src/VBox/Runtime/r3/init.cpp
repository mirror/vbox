/* $Id$ */
/** @file
 * IPRT - Init Ring-3.
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
#define LOG_GROUP RTLOGGROUP_DEFAULT
#ifdef RT_OS_WINDOWS
# include <process.h>
#else
# include <unistd.h>
#endif
#include <locale.h>

#include <iprt/initterm.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/log.h>
#include <iprt/path.h>
#include <iprt/time.h>
#include <iprt/string.h>
#include <iprt/param.h>
#if !defined(IN_GUEST) && !defined(RT_NO_GIP)
# include <iprt/file.h>
# include <VBox/sup.h>
# include <stdlib.h>
#endif

#include "internal/path.h"
#include "internal/process.h"
#include "internal/thread.h"
#include "internal/thread.h"
#include "internal/time.h"


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The number of calls to RTR3Init. */
static int32_t volatile g_cUsers = 0;
/** Whether we're currently initializing the IPRT. */
static bool volatile    g_fInitializing = false;

/** The process path.
 * This is used by RTPathProgram and RTProcGetExecutableName and set by rtProcInitName. */
char        g_szrtProcExePath[RTPATH_MAX];
/** The length of g_szrtProcExePath. */
size_t      g_cchrtProcExePath;
/** The length of directory path component of g_szrtProcExePath. */
size_t      g_cchrtProcDir;
/** The offset of the process name into g_szrtProcExePath. */
size_t      g_offrtProcName;

/**
 * Program start nanosecond TS.
 */
uint64_t    g_u64ProgramStartNanoTS;

/**
 * Program start microsecond TS.
 */
uint64_t    g_u64ProgramStartMicroTS;

/**
 * Program start millisecond TS.
 */
uint64_t    g_u64ProgramStartMilliTS;

/**
 * The process identifier of the running process.
 */
RTPROCESS g_ProcessSelf = NIL_RTPROCESS;

/**
 * The current process priority.
 */
RTPROCPRIORITY g_enmProcessPriority = RTPROCPRIORITY_DEFAULT;



/**
 * Internal worker which initializes or re-initializes the
 * program path, name and directory globals.
 *
 * @returns IPRT status code.
 * @param   pszProgramPath  The program path, NULL if not specified.
 */
static int rtR3InitProgramPath(const char *pszProgramPath)
{
    /*
     * We're reserving 32 bytes here for file names as what not.
     */
    if (!pszProgramPath)
    {
        int rc = rtProcInitExePath(g_szrtProcExePath, sizeof(g_szrtProcExePath) - 32);
        if (RT_FAILURE(rc))
            return rc;
    }
    else
    {
        size_t cch = strlen(pszProgramPath);
        Assert(cch > 1);
        AssertMsgReturn(cch < sizeof(g_szrtProcExePath) - 32, ("%zu\n", cch), VERR_BUFFER_OVERFLOW);
        memcpy(g_szrtProcExePath, pszProgramPath, cch + 1);
    }

    /*
     * Parse the name.
     */
    ssize_t offName;
    g_cchrtProcExePath = RTPathParse(g_szrtProcExePath, &g_cchrtProcDir, &offName, NULL);
    g_offrtProcName = offName;
    return VINF_SUCCESS;
}


/**
 * Internal initialization worker.
 *
 * @returns IPRT status code.
 * @param   fInitSUPLib     Whether to call SUPR3Init.
 * @param   pszProgramPath  The program path, NULL if not specified.
 */
static int rtR3Init(bool fInitSUPLib, const char *pszProgramPath)
{
    int rc = VINF_SUCCESS;
    /* no entry log flow, because prefixes and thread may freak out. */

    /*
     * Do reference counting, only initialize the first time around.
     *
     * We are ASSUMING that nobody will be able to race RTR3Init calls when the
     * first one, the real init, is running (second assertion).
     */
    int32_t cUsers = ASMAtomicIncS32(&g_cUsers);
    if (cUsers != 1)
    {
        AssertMsg(cUsers > 1, ("%d\n", cUsers));
        Assert(!g_fInitializing);
#if !defined(IN_GUEST) && !defined(RT_NO_GIP)
        if (fInitSUPLib)
            SUPR3Init(NULL);
#endif
        if (pszProgramPath)
            rc = rtR3InitProgramPath(pszProgramPath);
        return rc;
    }
    ASMAtomicWriteBool(&g_fInitializing, true);

#if !defined(IN_GUEST) && !defined(RT_NO_GIP)
# ifdef VBOX
    /*
     * This MUST be done as the very first thing, before any file is opened.
     * The log is opened on demand, but the first log entries may be caused
     * by rtThreadInit() below.
     */
    const char *pszDisableHostCache = getenv("VBOX_DISABLE_HOST_DISK_CACHE");
    if (    pszDisableHostCache != NULL
        &&  strlen(pszDisableHostCache) > 0
        &&  strcmp(pszDisableHostCache, "0") != 0)
    {
        RTFileSetForceFlags(RTFILE_O_WRITE, RTFILE_O_WRITE_THROUGH, 0);
        RTFileSetForceFlags(RTFILE_O_READWRITE, RTFILE_O_WRITE_THROUGH, 0);
    }
# endif  /* VBOX */
#endif /* !IN_GUEST && !RT_NO_GIP */

    /*
     * Thread Thread database and adopt the caller thread as 'main'.
     * This must be done before everything else or else we'll call into threading
     * without having initialized TLS entries and suchlike.
     */
    rc = rtThreadInit();
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("Failed to initialize threads, rc=%Rrc!\n", rc));
        ASMAtomicWriteBool(&g_fInitializing, false);
        ASMAtomicDecS32(&g_cUsers);
        return rc;
    }

#if !defined(IN_GUEST) && !defined(RT_NO_GIP)
    if (fInitSUPLib)
    {
        /*
         * Init GIP first.
         * (The more time for updates before real use, the better.)
         */
        rc = SUPR3Init(NULL);
        if (RT_FAILURE(rc))
        {
            AssertMsgFailed(("Failed to initializeble the support library, rc=%Rrc!\n", rc));
            ASMAtomicWriteBool(&g_fInitializing, false);
            ASMAtomicDecS32(&g_cUsers);
            return rc;
        }
    }
#endif

    /*
     * The Process ID.
     */
#ifdef _MSC_VER
    g_ProcessSelf = _getpid(); /* crappy ansi compiler */
#else
    g_ProcessSelf = getpid();
#endif

    /*
     * The executable path, name and directory.
     */
    rc = rtR3InitProgramPath(pszProgramPath);
    if (RT_FAILURE(rc))
    {
        AssertLogRelMsgFailed(("Failed to get executable directory path, rc=%Rrc!\n", rc));
        ASMAtomicWriteBool(&g_fInitializing, false);
        ASMAtomicDecS32(&g_cUsers);
        return rc;
    }

#if !defined(IN_GUEST) && !defined(RT_NO_GIP)
    /*
     * The threading is initialized we can safely sleep a bit if GIP
     * needs some time to update itself updating.
     */
    if (fInitSUPLib && g_pSUPGlobalInfoPage)
    {
        RTThreadSleep(20);
        RTTimeNanoTS();
    }
#endif

    /*
     * Init the program start TSes.
     * Do that here to be sure that the GIP time was properly updated the 1st time.
     */
    g_u64ProgramStartNanoTS = RTTimeNanoTS();
    g_u64ProgramStartMicroTS = g_u64ProgramStartNanoTS / 1000;
    g_u64ProgramStartMilliTS = g_u64ProgramStartNanoTS / 1000000;

    /*
     * Init C runtime locale
     */
    setlocale(LC_CTYPE, "");

    /*
     * More stuff to come?
     */

    LogFlow(("RTR3Init: returns VINF_SUCCESS\n"));
    ASMAtomicWriteBool(&g_fInitializing, false);
    return VINF_SUCCESS;
}


RTR3DECL(int) RTR3Init(void)
{
    return rtR3Init(false /* fInitSUPLib */, NULL);
}


RTR3DECL(int) RTR3InitEx(uint32_t iVersion, const char *pszProgramPath, bool fInitSUPLib)
{
    AssertReturn(iVersion == 0, VERR_NOT_SUPPORTED);
    return rtR3Init(fInitSUPLib, pszProgramPath);
}


RTR3DECL(int) RTR3InitWithProgramPath(const char *pszProgramPath)
{
    return rtR3Init(false /* fInitSUPLib */, pszProgramPath);
}


RTR3DECL(int) RTR3InitAndSUPLib(void)
{
    return rtR3Init(true /* fInitSUPLib */, NULL /* pszProgramPath */);
}


RTR3DECL(int) RTR3InitAndSUPLibWithProgramPath(const char *pszProgramPath)
{
    return rtR3Init(true /* fInitSUPLib */, pszProgramPath);
}


#if 0 /** @todo implement RTR3Term. */
RTR3DECL(void) RTR3Term(void)
{
}
#endif

