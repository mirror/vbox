/* $Id$ */
/** @file
 * VBoxServiceExec - In-VM Command Execution Service.
 */
/** @todo r=bird: Why is this called VMExec while the filename is VBoxServiceExec.cpp? See VMInfo... */
/** @todo r=bird: Use svn-ps.[sh|cmd] -a when adding new files, please. Then the EOLs and $Id$ won't be messed up all the time. */

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
#include <iprt/env.h>
#include <iprt/file.h>
#include <iprt/mem.h>
#include <iprt/process.h>
#include <iprt/string.h>
#include <iprt/semaphore.h>
#include <iprt/thread.h>
#include <VBox/version.h>
#include <VBox/VBoxGuestLib.h>
#include "VBoxServiceInternal.h"
#include "VBoxServiceUtils.h"
#ifdef VBOX_WITH_GUEST_PROPS
# include <VBox/HostServices/GuestPropertySvc.h>
#endif


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The vminfo interval (millseconds). */
uint32_t                g_VMExecInterval = 0;
/** The semaphore we're blocking on. */
static RTSEMEVENTMULTI  g_VMExecEvent = NIL_RTSEMEVENTMULTI;
/** The guest property service client ID. */
static uint32_t         g_VMExecGuestPropSvcClientID = 0;
/** The maximum of arguments a command can have. */
#define EXEC_MAX_ARGS   32

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
    else if (!strcmp(argv[*pi], "--vmexec-interval"))
        rc = VBoxServiceArgUInt32(argc, argv, "", pi, &g_VMExecInterval, 1, UINT32_MAX - 1);
    return rc;
}


/** @copydoc VBOXSERVICE::pfnInit */
static DECLCALLBACK(int) VBoxServiceExecInit(void)
{
    /*
     * If not specified, find the right interval default.
     * Then create the event sem to block on.
     */
    if (!g_VMExecInterval)
        g_VMExecInterval = g_DefaultInterval * 1000;
    if (!g_VMExecInterval)
        g_VMExecInterval = 10 * 1000;

    int rc = RTSemEventMultiCreate(&g_VMExecEvent);
    AssertRCReturn(rc, rc);

    rc = VbglR3GuestPropConnect(&g_VMExecGuestPropSvcClientID);
    if (RT_SUCCESS(rc))
        VBoxServiceVerbose(3, "Property Service Client ID: %#x\n", g_VMExecGuestPropSvcClientID);
    else
    {
        VBoxServiceError("Failed to connect to the guest property service! Error: %Rrc\n", rc);
        RTSemEventMultiDestroy(g_VMExecEvent);
        g_VMExecEvent = NIL_RTSEMEVENTMULTI;
    }

    return rc;
}


/**
 * Validates flags for executable guest properties.
 *
 * @returns boolean.
 * @retval  true on success.
 * @retval  false if not valid.
 *
 * @param   pszFlags     Pointer to flags to be checked.
 */
bool VBoxServiceExecValidateFlags(char* pszFlags)
{
    if (pszFlags == NULL)
        return false;

    if (   (NULL != RTStrStr(pszFlags, "TRANSIENT"))
        && (NULL != RTStrStr(pszFlags, "RDONLYGUEST")))
    {
        return true;
    }
    return false;
}


/** @copydoc VBOXSERVICE::pfnWorker */
DECLCALLBACK(int) VBoxServiceExecWorker(bool volatile *pfShutdown)
{
    using namespace guestProp;
    int rc = VINF_SUCCESS;

    /*
     * Tell the control thread that it can continue
     * spawning services.
     */
    RTThreadUserSignal(RTThreadSelf());
    Assert(g_VMExecGuestPropSvcClientID > 0);

    /*
     * Execution loop.
     */
#ifdef FULL_FEATURED_EXEC
    uint64_t    u64TimestampPrev = UINT64_MAX;
#endif
    bool        fSysprepDone     = false;
    for (;;)
    {
        /*
         * The thread at the moment does nothing but checking for one specific guest property
         * for triggering a hard coded sysprep command with parameters given by the host. This
         * feature was required by the VDI guys.
         *
         * Later this thread could become a general host->guest executor (remote shell?).
         */
        if (!fSysprepDone)
        {
            uint32_t cbBuf = MAX_VALUE_LEN + MAX_FLAGS_LEN + 1024;

            /* Get arguments. */
            /** @todo r=bird: How exactly does this work wrt. enabled/disabled?  In
             *        ConsoleImpl2.cpp it's now always set to "", which means it will
             *        always be executed. So, if someone adds a bogus
             *        c:\\sysprep\\sysprep.exe to their system and install the latest
             *        additions this will be executed everytime the system starts now - if
             *        I understand the code correctly. Is this intentional? */
            void* pvSysprepCmd = RTMemAllocZ(cbBuf);
            char* pszSysprepCmdValue = NULL;
            uint64_t u64SysprepCmdTimestamp = 0;
            char* pszSysprepCmdFlags = NULL;
#ifdef SYSPREP_WITH_CMD
            /* Get sysprep properties. */
            if (RT_SUCCESS(rc))
            {
                rc = VbglR3GuestPropRead(g_VMExecGuestPropSvcClientID, "/VirtualBox/HostGuest/SysprepCmd",
                                         pvSysprepCmd, cbBuf,
                                         &pszSysprepCmdValue, &u64SysprepCmdTimestamp, &pszSysprepCmdFlags, NULL);
                if (RT_FAILURE(rc))
                {
                    if (rc == VERR_NOT_FOUND)
                        VBoxServiceVerbose(2, "Sysprep cmd guest property not found\n");
                    else
                        VBoxServiceError("Sysprep cmd guest property: Error = %Rrc\n", rc);
                }
                else
                {
                    !VBoxServiceExecValidateFlags(pszSysprepCmdFlags) ? rc = rc : rc = VERR_ACCESS_DENIED;
                    if (RT_SUCCESS(rc))
                    {
                        if (RTStrNLen(pszSysprepCmdValue, _MAX_PATH) <= 0)
                            rc = VERR_NOT_FOUND;
                    }
                    VBoxServiceVerbose(2, "Sysprep cmd guest property: %Rrc\n", rc);
                }
            }
#else
            /* Choose sysprep image based on OS version. */
            char szSysprepCmd[_MAX_PATH] = "c:\\sysprep\\sysprep.exe";
            OSVERSIONINFOEX OSInfoEx;
            memset(&OSInfoEx, '\0', sizeof(OSInfoEx));
            OSInfoEx.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
            if (GetVersionEx((LPOSVERSIONINFO) &OSInfoEx))
            {
                if (OSInfoEx.dwMajorVersion >= 6) /* Use built-in sysprep on Vista or later. */
                    RTStrPrintf(szSysprepCmd, sizeof(szSysprepCmd), "%s\\system32\\sysprep\\sysprep.exe", RTEnvGet("windir"));
            }
            pszSysprepCmdValue = szSysprepCmd;
#endif
            void* pvSysprepArgs = RTMemAllocZ(cbBuf);
            char* pszSysprepArgsValue = NULL;
            uint64_t u64SysprepArgsTimestamp = 0;
            char* pszSysprepArgsFlags = NULL;

            if (RT_SUCCESS(rc))
            {
                pvSysprepArgs = RTMemAllocZ(cbBuf);
                char szVal[] = "/VirtualBox/HostGuest/SysprepArgs";
                char *pTmp = NULL;
                rc = RTStrCurrentCPToUtf8(&pTmp, szVal);
                rc = VbglR3GuestPropRead(g_VMExecGuestPropSvcClientID, pTmp,
                                         pvSysprepArgs, cbBuf,
                                         &pszSysprepArgsValue, &u64SysprepArgsTimestamp, &pszSysprepArgsFlags, NULL);
                RTStrFree(pTmp);
                if (RT_FAILURE(rc))
                {
                    if (rc == VERR_NOT_FOUND)
                        VBoxServiceVerbose(2, "Sysprep argument guest property not found\n");
                    else
                        VBoxServiceError("Sysprep argument guest property: Error = %Rrc\n", rc);
                }
                else
                {
                    !VBoxServiceExecValidateFlags(pszSysprepArgsFlags) ? rc = rc : rc = VERR_ACCESS_DENIED;
                    if (RT_SUCCESS(rc))
                    {
                        if (RTStrNLen(pszSysprepArgsValue, EXEC_MAX_ARGS) <= 0)
                            rc = VERR_NOT_FOUND;
                    }
                    VBoxServiceVerbose(2, "Sysprep argument guest property: %Rrc\n", rc);
                }
            }

            if (   RT_SUCCESS(rc)
                && !RTFileExists(pszSysprepCmdValue))
            {
                VBoxServiceError("Sysprep executable not found! Search path=%s\n", pszSysprepCmdValue);
                rc = VERR_FILE_NOT_FOUND;
            }

            if (RT_SUCCESS(rc))
            {
                RTPROCESS pid;

                /* Construct arguments list. */
                /** @todo would be handy to have such a function in IPRT. */
                int16_t index = 0;
                char* pszArg = pszSysprepArgsValue;

                const char* pArgs[EXEC_MAX_ARGS]; /* Do we have a #define in IPRT for max args? */

                VBoxServiceVerbose(3, "Sysprep argument value: %s\n", pszSysprepArgsValue);
                pArgs[index++] = pszSysprepCmdValue; /* Store image name as first argument. */
                while (   (pszArg != NULL)
                       && (*pszArg != NULL))
                {
                    char* pCurArg = (char*)RTMemAllocZ(_MAX_PATH);
                    int arg = 0;
                    while (   (*pszArg != ' ')
                           && (*pszArg != NULL))
                    {
                        pCurArg[arg++] = *pszArg;
                        pszArg++;
                    }
                    pszArg++; /* Skip leading space. */
                    VBoxServiceVerbose(3, "Sysprep argument %d = %s\n", index, pCurArg);
                    pArgs[index++] = pCurArg;
                }
                pArgs[index] = NULL;

                /* Execute ... */
                VBoxServiceVerbose(3, "Executing sysprep ...\n");
                rc = RTProcCreate(pszSysprepCmdValue, pArgs, RTENV_DEFAULT, 0, &pid);
                if (RT_SUCCESS(rc))
                {
                    RTPROCSTATUS Status;
                    rc = RTProcWait(pid, RTPROCWAIT_FLAGS_BLOCK, &Status);

                    VBoxServiceVerbose(3, "Sysprep returned: %d\n", Status.iStatus);
                    if (RT_SUCCESS(rc))
                    {
                        if (    Status.iStatus == 0
                            &&  Status.enmReason == RTPROCEXITREASON_NORMAL)
                        {
                            fSysprepDone = true; /* We're done, no need to repeat the sysprep exec. */
                        }
                    }

                    /* Set return value so the host knows what happend. */
                    rc = VboxServiceWritePropInt(g_VMExecGuestPropSvcClientID, "HostGuest/SysprepRet", Status.iStatus);
                    if (RT_FAILURE(rc))
                        VBoxServiceError("Failed to write SysprepRet: rc=%Rrc\n", rc);
                }

                /* Destroy argument list (all but the first and last arg, it's just a pointer). */
                for (int i=1; i<index-1; i++)
                    RTMemFree((void*)pArgs[i]);
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
        int rc2 = RTSemEventMultiWait(g_VMExecEvent, g_VMExecInterval);
#endif
        if (*pfShutdown)
            break;
        if (rc2 != VERR_TIMEOUT && RT_FAILURE(rc2))
        {
            VBoxServiceError("RTSemEventMultiWait failed; rc2=%Rrc\n", rc2);
            rc = rc2;
            break;
        }
    }

    RTSemEventMultiDestroy(g_VMExecEvent);
    g_VMExecEvent = NIL_RTSEMEVENTMULTI;
    return rc;
}


/** @copydoc VBOXSERVICE::pfnStop */
static DECLCALLBACK(void) VBoxServiceExecStop(void)
{
    /** @todo Later, figure what to do if we're in RTProcWait(). it's a very
     *        annoying call since doesn't support timeouts in the posix world. */
    RTSemEventMultiSignal(g_VMExecEvent);
#ifdef FULL_FEATURED_EXEC
    Interrupts waits.
#endif
}


/** @copydoc VBOXSERVICE::pfnTerm */
static DECLCALLBACK(void) VBoxServiceExecTerm(void)
{
    /* Nothing here yet. */
    VbglR3GuestPropDisconnect(g_VMExecGuestPropSvcClientID);
    g_VMExecGuestPropSvcClientID = 0;

    RTSemEventMultiDestroy(g_VMExecEvent);
    g_VMExecEvent = NIL_RTSEMEVENTMULTI;
}


/**
 * The 'vminfo' service description.
 */
VBOXSERVICE g_VMExec =
{
    /* pszName. */
    "vmexec",
    /* pszDescription. */
    "Virtual Machine Remote Execution",
    /* pszUsage. */
    "[--vmexec-interval <ms>]"
    ,
    /* pszOptions. */
    "    --vmexec-interval   Specifies the interval at which to check for new\n"
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

