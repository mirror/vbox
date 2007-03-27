/* $Id$ */
/** @file
 * InnoTek Portable Runtime - Init Ring-3.
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
#define LOG_GROUP RTLOGGROUP_DEFAULT
#ifdef __WIN__
# include <process.h>
#else
# include <unistd.h>
#endif

#include <iprt/runtime.h>
#include <iprt/path.h>
#include <iprt/assert.h>
#include <iprt/log.h>
#include <iprt/time.h>
#include <iprt/err.h>
#include <iprt/string.h>
#include <iprt/param.h>
#if !defined(IN_GUEST) && !defined(IN_GUEST_R3)
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
/** Program path.
 * The size is hardcoded, so we'll have to check for overflow when setting it
 * since some hosts might support longer paths.
 * @internal
 */
char        g_szrtProgramPath[RTPATH_MAX];

/**
 * Program start nanosecond TS.
 */
uint64_t    g_u64ProgramStartNanoTS;

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
 * Initalizes the runtime library.
 *
 * @returns iprt status code.
 *
 * @param   fInitSUPLib     Set if SUPInit() shall be called during init (default).
 *                          Clear if not to call it.
 * @param   cbReserve       The number of bytes of contiguous memory that should be reserved by
 *                          the runtime / support library.
 *                          Set this to 0 if no reservation is required. (default)
 *                          Set this to ~0 if the maximum amount supported by the VM is to be
 *                          attempted reserved, or the maximum available.
 *                          This argument only applies if fInitSUPLib is true and we're in ring-3 HC.
 */
RTR3DECL(int) RTR3Init(bool fInitSUPLib, size_t cbReserve)
{
    /* no entry log flow, because prefixes and thread may freak out. */

#if !defined(IN_GUEST) && !defined(IN_GUEST_R3)
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
#endif /* !IN_GUEST and !IN_GUEST_R3 */

    /*
     * Thread Thread database and adopt the caller thread as 'main'.
     * This must be done before everything else or else we'll call into threading
     * without having initialized TLS entries and suchlike.
     */
    int rc = rtThreadInit();
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("Failed to get executable directory path, rc=%d!\n", rc));
        return rc;
    }

#if !defined(IN_GUEST) && !defined(IN_GUEST_R3)
    if (fInitSUPLib)
    {
        /*
         * Init GIP first.
         * (The more time for updates before real use, the better.)
         */
        SUPInit(NULL, cbReserve);
    }
#endif

    /*
     * Init the program start TSes.
     */
    g_u64ProgramStartNanoTS = RTTimeNanoTS();
    g_u64ProgramStartMilliTS = RTTimeMilliTS();

#if !defined(IN_GUEST) && !defined(IN_GUEST_R3)
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
     * Get the executable path.
     *
     * We're also checking the depth here since we'll be
     * appending filenames to the executable path. Currently
     * we assume 16 bytes are what we need.
     */
    char szPath[RTPATH_MAX - 16];
    rc = RTPathProgram(szPath, sizeof(szPath));
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("Failed to get executable directory path, rc=%d!\n", rc));
        return rc;
    }

    /*
     * The Process ID.
     */
#ifdef _MSC_VER
    g_ProcessSelf = _getpid(); /* crappy ansi compiler */
#else
    g_ProcessSelf = getpid();
#endif

    /*
     * More stuff to come.
     */

    LogFlow(("RTR3Init: returns VINF_SUCCESS\n"));
    return VINF_SUCCESS;
}

