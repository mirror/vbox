/* $Id$ */
/** @file
 * VBoxServiceExec - Host-driven Command Execution.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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
#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/env.h>
#include <iprt/file.h>
#include <iprt/mem.h>
#include <iprt/path.h>
#include <iprt/param.h>
#include <iprt/process.h>
#include <iprt/string.h>
#include <iprt/semaphore.h>
#include <iprt/thread.h>
#include <VBox/version.h>
#include <VBox/VBoxGuestLib.h>
#include "VBoxServiceInternal.h"
#include "VBoxServiceUtils.h"


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The vminfo interval (millseconds). */
static uint32_t         g_cMsExecInterval = 0;
/** The semaphore we're blocking on. */
static RTSEMEVENTMULTI  g_hExecEvent = NIL_RTSEMEVENTMULTI;
/** The guest property service client ID. */
static uint32_t         g_uExecGuestPropSvcClientID = 0;


/** @copydoc VBOXSERVICE::pfnPreInit */
static DECLCALLBACK(int) VBoxServiceExecPreInit(void)
{
    return VINF_SUCCESS;
}


/** @copydoc VBOXSERVICE::pfnOption */
static DECLCALLBACK(int) VBoxServiceExecOption(const char **ppszShort, int argc, char **argv, int *pi)
{
    int rc = -1;
    if (ppszShort)
        /* no short options */;
    else if (!strcmp(argv[*pi], "--exec-interval"))
        rc = VBoxServiceArgUInt32(argc, argv, "", pi, &g_cMsExecInterval, 1, UINT32_MAX - 1);
    return rc;
}


/** @copydoc VBOXSERVICE::pfnInit */
static DECLCALLBACK(int) VBoxServiceExecInit(void)
{
    /*
     * If not specified, find the right interval default.
     * Then create the event sem to block on.
     */
    if (!g_cMsExecInterval)
        g_cMsExecInterval = g_DefaultInterval * 1000;
    if (!g_cMsExecInterval)
        g_cMsExecInterval = 10 * 1000;

    int rc = RTSemEventMultiCreate(&g_hExecEvent);
    AssertRCReturn(rc, rc);

    rc = VbglR3GuestPropConnect(&g_uExecGuestPropSvcClientID);
    if (RT_SUCCESS(rc))
        VBoxServiceVerbose(3, "Exec: Property Service Client ID: %#x\n", g_uExecGuestPropSvcClientID);
    else
    {
        VBoxServiceError("Exec: Failed to connect to the guest property service! Error: %Rrc\n", rc);
        RTSemEventMultiDestroy(g_hExecEvent);
        g_hExecEvent = NIL_RTSEMEVENTMULTI;
    }

    return rc;
}


/**
 * Validates flags for executable guest properties.
 *
 * @returns VBox status code. Success means they are valid.
 *
 * @param   pszFlags     Pointer to flags to be checked.
 */
static int VBoxServiceExecValidateFlags(const char *pszFlags)
{
    if (!pszFlags)
        return VERR_ACCESS_DENIED;
    if (!RTStrStr(pszFlags, "TRANSIENT"))
        return VERR_ACCESS_DENIED;
    if (!RTStrStr(pszFlags, "RDONLYGUEST"))
        return VERR_ACCESS_DENIED;
    return VINF_SUCCESS;
}


/**
 * Reads a host transient property.
 *
 * This will validate the flags to make sure it is a transient property that can
 * only be change by the host.
 *
 * @returns VBox status code, fully bitched.
 * @param   pszPropName         The property name.
 * @param   ppszValue           Where to return the value.  This is always set
 *                              to NULL.  Free it using RTStrFree().
 * @param   puTimestamp         Where to return the timestamp.  This is only set
 *                              on success.  Optional.
 */
static int VBoxServiceExecReadHostProp(const char *pszPropName, char **ppszValue, uint64_t *puTimestamp)
{
    size_t  cbBuf = _1K;
    void   *pvBuf = NULL;
    int     rc;
    *ppszValue = NULL;

    char *pszPropNameUTF8;
    rc = RTStrCurrentCPToUtf8(&pszPropNameUTF8, pszPropName);
    if (RT_FAILURE(rc))
    {
        VBoxServiceError("Exec: Failed to convert property name \"%s\" to UTF-8!\n", pszPropName);
        return rc;
    }

    for (unsigned cTries = 0; cTries < 10; cTries++)
    {
        /*
         * (Re-)Allocate the buffer and try read the property.
         */
        RTMemFree(pvBuf);
        pvBuf = RTMemAlloc(cbBuf);
        if (!pvBuf)
        {
            VBoxServiceError("Exec: Failed to allocate %zu bytes\n", cbBuf);
            rc = VERR_NO_MEMORY;
            break;
        }
        char    *pszValue;
        char    *pszFlags;
        uint64_t uTimestamp;
        rc = VbglR3GuestPropRead(g_uExecGuestPropSvcClientID, pszPropNameUTF8,
                                 pvBuf, cbBuf,
                                 &pszValue, &uTimestamp, &pszFlags, NULL);
        if (RT_FAILURE(rc))
        {
            if (rc == VERR_BUFFER_OVERFLOW)
            {
                /* try again with a bigger buffer. */
                cbBuf *= 2;
                continue;
            }
            if (rc == VERR_NOT_FOUND)
                VBoxServiceVerbose(2, "Exec: %s not found\n", pszPropName);
            else
                VBoxServiceError("Exec: Failed to query \"%s\": %Rrc\n", pszPropName, rc);
            break;
        }

        /*
         * Validate it and set return values on success.
         */
        rc = VBoxServiceExecValidateFlags(pszFlags);
        if (RT_FAILURE(rc))
        {
            static uint32_t s_cBitched = 0;
            if (++s_cBitched < 10)
                VBoxServiceError("Exec: Flag validation failed for \"%s\": %Rrc; flags=\"%s\"\n",
                                 pszPropName, rc, pszFlags);
            break;
        }
        VBoxServiceVerbose(2, "Exec: Read \"%s\" = \"%s\", timestamp %RU64n\n",
                           pszPropName, pszValue, uTimestamp);
        *ppszValue = RTStrDup(pszValue);
        if (!*ppszValue)
        {
            VBoxServiceError("Exec: RTStrDup failed for \"%s\"\n", pszValue);
            rc = VERR_NO_MEMORY;
            break;
        }

        if (puTimestamp)
            *puTimestamp = uTimestamp;
        break; /* done */
    }
    RTMemFree(pvBuf);
    RTStrFree(pszPropNameUTF8);
    return rc;
}


/**
 * Frees an argument vector constructed by VBoxServiceExecCreateArgV.
 *
 * @param   papszArgs           The vector to free.
 */
static void VBoxServiceExecFreeArgV(char **papszArgs)
{
    for (size_t i = 0; papszArgs[i]; i++)
    {
        RTStrFree(papszArgs[i]);
        papszArgs[i] = NULL;
    }
    RTMemFree(papszArgs);
}


/**
 * Creates an argument vector out of an executable name and a string containing
 * the arguments separated by spaces.
 *
 * @returns VBox status code. Not bitched.
 * @param   pszExec             The executable name.
 * @param   pszArgs             The string containging the arguments.
 * @param   ppapszArgs          Where to return the argument vector.  Not set on
 *                              failure.  Use VBoxServiceExecFreeArgV to free.
 *
 * @todo    Quoted strings. Do it unix (bourne shell) fashion.
 */
static int VBoxServiceExecCreateArgV(const char *pszExec, const char *pszArgs, char ***ppapszArgs)
{
    size_t  cAlloc   = 1;
    size_t  cUsed    = 1;
    char **papszArgs = (char **)RTMemAlloc(sizeof(char *) * (cAlloc + 1));
    if (!papszArgs)
        return VERR_NO_MEMORY;

    /*
     * Start by adding the executable name first.
     * Note! We keep the papszArgs fully terminated at all times to keep cleanup simple.
     */
    int     rc   = VERR_NO_MEMORY;
    papszArgs[1] = NULL;
    papszArgs[0] = RTStrDup(pszExec);
    if (papszArgs[0])
    {
        /*
         * Parse the argument string and add any arguments found in it.
         */
        for (;;)
        {
            /* skip leading spaces */
            char ch;
            while ((ch = *pszArgs) && RT_C_IS_SPACE(ch))
                pszArgs++;
            if (!*pszArgs)
            {
                *ppapszArgs = papszArgs;
                return VINF_SUCCESS;
            }

            /* find the of the current word. Quoting is ignored atm. */
            char const *pszEnd = pszArgs + 1;
            while ((ch = *pszEnd) && !RT_C_IS_SPACE(ch))
                pszEnd++;

            /* resize the vector. */
            if (cUsed == cAlloc)
            {
                cAlloc += 10;
                void *pvNew = RTMemRealloc(papszArgs, sizeof(char *) * (cAlloc + 1));
                if (!pvNew)
                    break;
                papszArgs = (char **)pvNew;
                for (size_t i = cUsed; i <= cAlloc; i++)
                    papszArgs[i] = NULL;
            }

            /* add it */
            papszArgs[cUsed] = RTStrDupN(pszArgs, (uintptr_t)pszEnd - (uintptr_t)pszArgs);
            if (!papszArgs[cUsed])
                break;
            cUsed++;

            /* advance */
            pszArgs = pszEnd;
        }
    }

    VBoxServiceExecFreeArgV(papszArgs);
    return rc;
}


/** @copydoc VBOXSERVICE::pfnWorker */
DECLCALLBACK(int) VBoxServiceExecWorker(bool volatile *pfShutdown)
{
    int rcRet = VINF_SUCCESS;

    /*
     * Tell the control thread that it can continue
     * spawning services.
     */
    RTThreadUserSignal(RTThreadSelf());
    Assert(g_uExecGuestPropSvcClientID > 0);

    /*
     * Execution loop.
     *
     * The thread at the moment does nothing but checking for one specific guest property
     * for triggering a hard coded sysprep command with parameters given by the host. This
     * feature was required by the VDI guys.
     *
     * Later this thread could become a general host->guest executor.. there are some
     * sketches for this in the code.
     */
#ifdef FULL_FEATURED_EXEC
    uint64_t    u64TimestampPrev = UINT64_MAX;
#endif
    bool        fSysprepDone = false;
    bool        fBitchedAboutMissingSysPrepCmd = false;
    for (;;)
    {
        if (!fSysprepDone)
        {
            /*
             * Get the sysprep command and arguments.
             *
             * The sysprep executable location is either retrieved from the host
             * or is in a hard coded location depending on the Windows version.
             */
            char *pszSysprepExec = NULL;
#ifdef SYSPREP_WITH_CMD
            int rc = VBoxServiceExecReadHostProp("/VirtualBox/HostGuest/SysprepExec", &pszSysprepExec, NULL);
            if (RT_SUCCESS(rc) && !*pszSysprepExec)
                rc = VERR_NOT_FOUND;
#else
            /* Predefined sysprep. */
            int  rc = VINF_SUCCESS;
            char szSysprepCmd[RTPATH_MAX] = "C:\\sysprep\\sysprep.exe";
            OSVERSIONINFOEX OSInfoEx;
            RT_ZERO(OSInfoEx);
            OSInfoEx.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
            if (    GetVersionEx((LPOSVERSIONINFO) &OSInfoEx)
                &&  OSInfoEx.dwPlatformId == VER_PLATFORM_WIN32_NT
                &&  OSInfoEx.dwMajorVersion >= 6 /* Vista or later */)
            {
                rc = RTEnvGetEx(RTENV_DEFAULT, "windir", szSysprepCmd, sizeof(szSysprepCmd), NULL);
                if (RT_SUCCESS(rc))
                    rc = RTPathAppend(szSysprepCmd, sizeof(szSysprepCmd), "system32\\sysprep\\sysprep.exe");
            }
            pszSysprepExec = szSysprepCmd;
#endif
            if (RT_SUCCESS(rc))
            {
                char *pszSysprepArgs;
                rc = VBoxServiceExecReadHostProp("/VirtualBox/HostGuest/SysprepArgs", &pszSysprepArgs, NULL);
                if (RT_SUCCESS(rc) && !*pszSysprepArgs)
                    rc = VERR_NOT_FOUND;
                if (RT_SUCCESS(rc))
                {
                    if (RTFileExists(pszSysprepExec))
                    {
                        char **papszArgs;
                        rc = VBoxServiceExecCreateArgV(pszSysprepExec, pszSysprepArgs, &papszArgs);
                        if (RT_SUCCESS(rc))
                        {
                            /*
                             * Execute it synchronously and store the result.
                             *
                             * Note that RTProcWait should never fail here and
                             * that (the host is screwed if it does though).
                             */
                            VBoxServiceVerbose(3, "Exec: Executing sysprep ...\n");
                            for (size_t i = 0; papszArgs[i]; i++)
                                VBoxServiceVerbose(3, "Exec: sysprep argv[%u]: \"%s\"\n", i, papszArgs[i]);

                            /*
                             * Status reporting via SysprepVBoxRC is required here in order to make a host program wait for it
                             * in any case (here, yet before the execution or down below if something failed). If no SysprepVBoxRC
                             * would be updated here the host wouldn't know if he now has to wait for SysprepRet (all succeded) or
                             * SysprepRC (something failed):
                             *
                             * 1. Host sets SysprepArgs
                             * 2. Host waits for a changing SysprepRC (this could be either here or in step 5)
                             * 3. If SysprepRC is successful go to 4, otherwise exit
                             * 4. Host waits for a changing SysprepRet (result of sysprep.exe execution)
                             * 5. Host waits for a changing SysprepRC (final result of operation)
                             */
                            rc = VBoxServiceWritePropF(g_uExecGuestPropSvcClientID, "/VirtualBox/HostGuest/SysprepVBoxRC", "%d", rc);
                            if (RT_FAILURE(rc))
                                VBoxServiceError("Exec: Failed to write SysprepVBoxRC while setting up: rc=%Rrc\n", rc);

                            RTPROCESS pid;
                            if (RT_SUCCESS(rc))
                                rc = RTProcCreate(pszSysprepExec, papszArgs, RTENV_DEFAULT, 0 /*fFlags*/, &pid);
                            if (RT_SUCCESS(rc))
                            {
                                RTPROCSTATUS Status;
                                rc = RTProcWait(pid, RTPROCWAIT_FLAGS_BLOCK, &Status);
                                if (RT_SUCCESS(rc))
                                {
                                    VBoxServiceVerbose(1, "Sysprep returned: %d (reason %d)\n",
                                                       Status.iStatus, Status.enmReason);
/** @todo r=bird: Figure out whether you should try re-execute sysprep if it
 *        fails or not. This is not mentioned in the defect.  */
                                    fSysprepDone = true; /* paranoia */

                                    /*
                                     * Store the result in Set return value so the host knows what happend.
                                     */
                                    rc = VBoxServiceWritePropF(g_uExecGuestPropSvcClientID,
                                                               "/VirtualBox/HostGuest/SysprepRet",
                                                               "%d", Status.iStatus);
                                    if (RT_FAILURE(rc))
                                        VBoxServiceError("Exec: Failed to write SysprepRet: rc=%Rrc\n", rc);
                                }
                                else
                                    VBoxServiceError("Exec: RTProcWait failed for sysprep: %Rrc\n", rc);
                            }
                            VBoxServiceExecFreeArgV(papszArgs);
                        }
                        else
                            VBoxServiceError("Exec: VBoxServiceExecCreateArgV: %Rrc\n", rc);
                    }
                    else
                    {
                        if (!fBitchedAboutMissingSysPrepCmd)
                        {
                            VBoxServiceError("Exec: Sysprep executable not found! Search path=%s\n", pszSysprepExec);
                            fBitchedAboutMissingSysPrepCmd = true;
                        }
                        rc = VERR_FILE_NOT_FOUND;
                    }
                    RTStrFree(pszSysprepArgs);
                }
#ifdef SYSPREP_WITH_CMD
                RTStrFree(pszSysprepExec);
#endif
            }

            /*
             * Only continue polling if the guest property value is empty/missing
             * or if the sysprep command is missing.
             */
            if (    rc != VERR_NOT_FOUND
                &&  rc != VERR_FILE_NOT_FOUND)
            {
                VBoxServiceVerbose(1, "Exec: Stopping sysprep processing (rc=%Rrc)\n", rc);
                fSysprepDone = true;
            }

            /*
             * Always let the host know what happend, except when the guest property
             * value is empty/missing.
             */
            if (rc != VERR_NOT_FOUND)
            {
                rc = VBoxServiceWritePropF(g_uExecGuestPropSvcClientID, "/VirtualBox/HostGuest/SysprepVBoxRC", "%d", rc);
                if (RT_FAILURE(rc))
                    VBoxServiceError("Exec: Failed to write final SysprepVBoxRC: rc=%Rrc\n", rc);
            }
        }

#ifdef FULL_FEATURED_EXEC
        1. Read the command - value, timestamp and flags.
        2. Check that the flags indicates that the guest cannot write to it and that it's transient.
        3. Check if the timestamp changed.
        4. Get the arguments and other stuff.
        5. Execute it. This may involve grabbing the output (stderr and/or stdout) and pushing into
           values afterwards. It may also entail redirecting input to a file containing text from a guest prop value.
        6. Set the result values (there will be three, one IPRT style one for everything up to
           and including RTProcWait and two that mirrors Status.iStatus and Status.enmReason (stringified)).
#endif

        /*
         * Block for a while.
         *
         * The event semaphore takes care of ignoring interruptions and it
         * allows us to implement service wakeup later.
         */
        if (*pfShutdown)
            break;
#ifdef FULL_FEATURED_EXEC
        Wait for changes to the command value. If that fails for some reason other than timeout / interrupt, fall back on the semaphore.
#else
        int rc2 = RTSemEventMultiWait(g_hExecEvent, g_cMsExecInterval);
#endif
        if (*pfShutdown)
            break;
        if (rc2 != VERR_TIMEOUT && RT_FAILURE(rc2))
        {
            VBoxServiceError("Exec: Service terminating - RTSemEventMultiWait: %Rrc\n", rc2);
            rcRet = rc2;
            break;
        }
    }

    RTSemEventMultiDestroy(g_hExecEvent);
    g_hExecEvent = NIL_RTSEMEVENTMULTI;
    return rcRet;
}


/** @copydoc VBOXSERVICE::pfnStop */
static DECLCALLBACK(void) VBoxServiceExecStop(void)
{
    /** @todo Later, figure what to do if we're in RTProcWait(). it's a very
     *        annoying call since doesn't support timeouts in the posix world. */
    RTSemEventMultiSignal(g_hExecEvent);
#ifdef FULL_FEATURED_EXEC
    Interrupts waits.
#endif
}


/** @copydoc VBOXSERVICE::pfnTerm */
static DECLCALLBACK(void) VBoxServiceExecTerm(void)
{
    /* Nothing here yet. */
    VbglR3GuestPropDisconnect(g_uExecGuestPropSvcClientID);
    g_uExecGuestPropSvcClientID = 0;

    RTSemEventMultiDestroy(g_hExecEvent);
    g_hExecEvent = NIL_RTSEMEVENTMULTI;
}


/**
 * The 'vminfo' service description.
 */
VBOXSERVICE g_Exec =
{
    /* pszName. */
    "exec",
    /* pszDescription. */
    "Host-driven Command Execution",
    /* pszUsage. */
    "[--exec-interval <ms>]"
    ,
    /* pszOptions. */
    "    --exec-interval     Specifies the interval at which to check for new\n"
    "                        remote execution commands. The default is 10000 ms.\n"
    ,
    /* methods */
    VBoxServiceExecPreInit,
    VBoxServiceExecOption,
    VBoxServiceExecInit,
    VBoxServiceExecWorker,
    VBoxServiceExecStop,
    VBoxServiceExecTerm
};

