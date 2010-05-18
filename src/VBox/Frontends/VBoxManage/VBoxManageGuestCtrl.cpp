/* $Id$ */
/** @file
 * VBoxManage - The 'guestcontrol' command.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "VBoxManage.h"

#include <VBox/com/com.h>
#include <VBox/com/string.h>
#include <VBox/com/array.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint.h>

#include <VBox/com/VirtualBox.h>
#include <VBox/com/EventQueue.h>

#include <VBox/HostServices/GuestControlSvc.h> /* for PROC_STS_XXX */

#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/getopt.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/time.h>
#include <iprt/thread.h>

#ifdef USE_XPCOM_QUEUE
# include <sys/select.h>
# include <errno.h>
#endif

#include <signal.h>

#ifdef RT_OS_DARWIN
# include <CoreFoundation/CFRunLoop.h>
#endif

using namespace com;

/**
 * IVirtualBoxCallback implementation for handling the GuestControlCallback in
 * relation to the "guestcontrol * wait" command.
 */
/** @todo */

/** Set by the signal handler. */
static volatile bool    g_fExecCanceled = false;

void usageGuestControl(void)
{
    RTPrintf("VBoxManage guestcontrol     execute <vmname>|<uuid>\n"
             "                            <path to program> --username <name>\n"
             "                            [--password <password>]\n"
             "                            [--arguments \"<arguments>\"]\n"
             "                            [--environment \"<NAME>=<VALUE> [<NAME>=<VALUE>]\"]\n"
             "                            [--flags <flags>] [--timeout <msec>]\n"             
             "                            [--verbose] [--wait-for exit,stdout,stderr||]\n"
             "\n");
}

/**
 * Signal handler that sets g_fCanceled.
 *
 * This can be executed on any thread in the process, on Windows it may even be
 * a thread dedicated to delivering this signal.  Do not doing anything
 * unnecessary here.
 */
static void execProcessSignalHandler(int iSignal)
{
    NOREF(iSignal);
    ASMAtomicWriteBool(&g_fExecCanceled, true);
}

static const char *getStatus(ULONG uStatus)
{
    switch (uStatus)
    {
        case guestControl::PROC_STS_STARTED:
            return "started";
        case guestControl::PROC_STS_TEN:
            return "successfully terminated";
        case guestControl::PROC_STS_TES:
            return "terminated by signal";
        case guestControl::PROC_STS_TEA:
            return "abnormally aborted";
        case guestControl::PROC_STS_TOK:
            return "timed out";
        case guestControl::PROC_STS_TOA:
            return "timed out, hanging";
        case guestControl::PROC_STS_DWN:
            return "killed";
        case guestControl::PROC_STS_ERROR:
            return "error";
        default:
            return "unknown";
    }
}

static int handleExecProgram(HandlerArg *a)
{
    /*
     * Check the syntax.  We can deduce the correct syntax from the number of
     * arguments.
     */
    if (a->argc < 2) /* At least the command we want to execute in the guest should be present :-). */
        return errorSyntax(USAGE_GUESTCONTROL, "Incorrect parameters");

    Utf8Str Utf8Cmd(a->argv[1]);
    uint32_t uFlags = 0;
    com::SafeArray <BSTR> args;
    com::SafeArray <BSTR> env;
    Utf8Str Utf8UserName;
    Utf8Str Utf8Password;
    uint32_t u32TimeoutMS = 0;
    bool fWaitForExit = false;
    bool fWaitForStdOut = false;
    bool fWaitForStdErr = false;
    bool fVerbose = false;
    bool fTimeout = false;

    /* Always use the actual command line as argv[0]. */
    args.push_back(Bstr(Utf8Cmd));

    /* Iterate through all possible commands (if available). */
    bool usageOK = true;
    for (int i = 2; usageOK && i < a->argc; i++)
    {
        if (   !strcmp(a->argv[i], "--arguments")
            || !strcmp(a->argv[i], "--args")
            || !strcmp(a->argv[i], "--arg"))
        {
            if (i + 1 >= a->argc)
                usageOK = false;
            else
            {
                char **papszArg;
                int cArgs;

                int vrc = RTGetOptArgvFromString(&papszArg, &cArgs, a->argv[i + 1], NULL);
                if (RT_SUCCESS(vrc))
                {
                    for (int j = 0; j < cArgs; j++)
                        args.push_back(Bstr(papszArg[j]));

                    RTGetOptArgvFree(papszArg);
                }
                ++i;
            }
        }
        else if (   !strcmp(a->argv[i], "--environment")
                 || !strcmp(a->argv[i], "--env"))
        {
            if (i + 1 >= a->argc)
                usageOK = false;
            else
            {
                char **papszArg;
                int cArgs;

                int vrc = RTGetOptArgvFromString(&papszArg, &cArgs, a->argv[i + 1], NULL);
                if (RT_SUCCESS(vrc))
                {
                    for (int j = 0; j < cArgs; j++)
                        env.push_back(Bstr(papszArg[j]));

                    RTGetOptArgvFree(papszArg);
                }
                ++i;
            }
        }
        else if (!strcmp(a->argv[i], "--flags"))
        {
            if (   i + 1 >= a->argc
                || RTStrToUInt32Full(a->argv[i + 1], 10, &uFlags) != VINF_SUCCESS)
                usageOK = false;
            else
                ++i;
        }
        else if (   !strcmp(a->argv[i], "--username")
                 || !strcmp(a->argv[i], "--user"))
        {
            if (i + 1 >= a->argc)
                usageOK = false;
            else
            {
                Utf8UserName = a->argv[i + 1];
                ++i;
            }
        }
        else if (   !strcmp(a->argv[i], "--password")
                 || !strcmp(a->argv[i], "--pwd"))
        {
            if (i + 1 >= a->argc)
                usageOK = false;
            else
            {
                Utf8Password = a->argv[i + 1];
                ++i;
            }
        }
        else if (!strcmp(a->argv[i], "--timeout"))
        {
            if (   i + 1 >= a->argc
                || RTStrToUInt32Full(a->argv[i + 1], 10, &u32TimeoutMS) != VINF_SUCCESS
                || u32TimeoutMS == 0)
            {
                usageOK = false;
            }
            else
            {
                fTimeout = true;
                ++i;
            }
        }
        else if (!strcmp(a->argv[i], "--wait-for"))
        {
            if (i + 1 >= a->argc)
                usageOK = false;
            else
            {
                if (!strcmp(a->argv[i + 1], "exit"))
                    fWaitForExit = true;
                else if (!strcmp(a->argv[i + 1], "stdout"))
                {
                    fWaitForExit = true;
                    fWaitForStdOut = true;
                }
                else if (!strcmp(a->argv[i + 1], "stderr"))
                {
                    fWaitForExit = true;
                    fWaitForStdErr = true;
                }
                else
                    usageOK = false;
                ++i;
            }
        }
        else if (!strcmp(a->argv[i], "--verbose"))
        {
            fVerbose = true;
        }
        /** @todo Add fancy piping stuff here. */
        else
        {
            return errorSyntax(USAGE_GUESTCONTROL,
                               "Invalid parameter '%s'", Utf8Str(a->argv[i]).raw());
        }
    }

    if (!usageOK)
        return errorSyntax(USAGE_GUESTCONTROL, "Incorrect parameters");

    if (Utf8UserName.isEmpty())
        return errorSyntax(USAGE_GUESTCONTROL,
                           "No user name specified!");

    /* lookup VM. */
    ComPtr<IMachine> machine;
    /* assume it's an UUID */
    HRESULT rc = a->virtualBox->GetMachine(Bstr(a->argv[0]), machine.asOutParam());
    if (FAILED(rc) || !machine)
    {
        /* must be a name */
        CHECK_ERROR(a->virtualBox, FindMachine(Bstr(a->argv[0]), machine.asOutParam()));
    }

    if (machine)
    {
        do
        {
            Bstr uuid;
            machine->COMGETTER(Id)(uuid.asOutParam());

            /* open an existing session for VM - so the VM has to be running */
            CHECK_ERROR_BREAK(a->virtualBox, OpenExistingSession(a->session, uuid));

            /* get the mutable session machine */
            a->session->COMGETTER(Machine)(machine.asOutParam());

            /* get the associated console */
            ComPtr<IConsole> console;
            CHECK_ERROR_BREAK(a->session, COMGETTER(Console)(console.asOutParam()));

            ComPtr<IGuest> guest;
            CHECK_ERROR_BREAK(console, COMGETTER(Guest)(guest.asOutParam()));

            ComPtr<IProgress> progress;
            ULONG uPID = 0;

            if (fVerbose)
            {
                if (u32TimeoutMS == 0)
                    RTPrintf("Waiting for guest to start process ...\n");
                else
                    RTPrintf("Waiting for guest to start process (within %ums)\n", u32TimeoutMS);
            }

            /* Get current time stamp to later calculate rest of timeout left. */
            uint64_t u64StartMS = RTTimeMilliTS();

            /* Execute the process. */
            rc = guest->ExecuteProcess(Bstr(Utf8Cmd), uFlags,
                                       ComSafeArrayAsInParam(args), ComSafeArrayAsInParam(env),
                                       Bstr(Utf8UserName), Bstr(Utf8Password), u32TimeoutMS,
                                       &uPID, progress.asOutParam());
            if (FAILED(rc))
            {
                ErrorInfo info(guest);
                if (info.isFullAvailable())
                    RTPrintf("ERROR: %ls (%Rhrc).\n", info.getText().raw(), info.getResultCode());
                break;
            }
            if (fVerbose)
                RTPrintf("Process '%s' (PID: %u) started\n", Utf8Cmd.raw(), uPID);
            if (fWaitForExit)
            {
                if (fTimeout)
                {
                    /* Calculate timeout value left after process has been started.  */
                    uint64_t u64Diff = RTTimeMilliTS() - u64StartMS;
                    /** @todo Check for uint64_t vs. uint32_t here! */
                    if (u32TimeoutMS > u64Diff) /* Is timeout still bigger than current difference? */
                    {
                        u32TimeoutMS = u32TimeoutMS - u64Diff;
                        if (fVerbose)
                            RTPrintf("Waiting for process to exit (%ums left) ...\n", u32TimeoutMS);
                    }
                    else
                    {
                        if (fVerbose)
                            RTPrintf("No time left to wait for process!\n");
                    }
                }
                else if (fVerbose)
                    RTPrintf("Waiting for process to exit ...\n");

                /* setup signal handling if cancelable */
                ASSERT(progress);
                bool fCanceledAlready = false;
                BOOL fCancelable;
                HRESULT hrc = progress->COMGETTER(Cancelable)(&fCancelable);
                if (FAILED(hrc))
                    fCancelable = FALSE;
                if (fCancelable)
                {
                    signal(SIGINT,   execProcessSignalHandler);
            #ifdef SIGBREAK
                    signal(SIGBREAK, execProcessSignalHandler);
            #endif
                }

                /* Wait for process to exit ... */
                BOOL fCompleted = false;
                while (SUCCEEDED(progress->COMGETTER(Completed(&fCompleted))))
                {
                    SafeArray<BYTE> aOutputData;
                    ULONG cbOutputData = 0;

                    /*
                     * Some data left to output?
                     */

                    if (   fWaitForStdOut
                        || fWaitForStdErr)
                    {
                        CHECK_ERROR_BREAK(guest, GetProcessOutput(uPID, 0 /* aFlags */,
                                                                  u32TimeoutMS, _64K, ComSafeArrayAsOutParam(aOutputData)));
                        cbOutputData = aOutputData.size();
                        if (cbOutputData > 0)
                        {
                            /* aOutputData has a platform dependent line ending, standardize on
                             * Unix style, as RTStrmWrite does the LF -> CR/LF replacement on
                             * Windows. Otherwise we end up with CR/CR/LF on Windows. */
                            ULONG cbOutputDataPrint = cbOutputData;
                            for (BYTE *s = aOutputData.raw(), *d = s;
                                 s - aOutputData.raw() < (ssize_t)cbOutputData;
                                 s++, d++)
                            {
                                if (*s == '\r')
                                {
                                    /* skip over CR, adjust destination */
                                    d--;
                                    cbOutputDataPrint--;
                                }
                                else if (s != d)
                                    *d = *s;
                            }
                            RTStrmWrite(g_pStdOut, aOutputData.raw(), cbOutputDataPrint);
                        }
                    }

                    if (cbOutputData <= 0)
                    {
                        if (fCompleted)
                            break;

                        if (   fTimeout
                            && RTTimeMilliTS() - u64StartMS > u32TimeoutMS + 5000)
                        {
                            progress->Cancel();
                            break;
                        }
                    }

                    /* process async cancelation */
                    if (g_fExecCanceled && !fCanceledAlready)
                    {
                        hrc = progress->Cancel();
                        if (SUCCEEDED(hrc))
                            fCanceledAlready = true;
                        else
                            g_fExecCanceled = false;
                    }

                    progress->WaitForCompletion(100);
                }

                /* undo signal handling */
                if (fCancelable)
                {
                    signal(SIGINT,   SIG_DFL);
            #ifdef SIGBREAK
                    signal(SIGBREAK, SIG_DFL);
            #endif
                }

                BOOL fCanceled;
                if (SUCCEEDED(progress->COMGETTER(Canceled)(&fCanceled)) && fCanceled)
                    if (fVerbose) RTPrintf("Process execution canceled!\n");

                if (fCompleted)
                {
                    LONG iRc = false;
                    CHECK_ERROR_RET(progress, COMGETTER(ResultCode)(&iRc), rc);
                    if (FAILED(iRc))
                    {
                        ComPtr<IVirtualBoxErrorInfo> execError;
                        rc = progress->COMGETTER(ErrorInfo)(execError.asOutParam());
                        com::ErrorInfo info (execError);
                        RTPrintf("\n\nProcess error details:\n");
                        GluePrintErrorInfo(info);
                        RTPrintf("\n");
                    }
                    if (fVerbose)
                    {
                        ULONG uRetStatus, uRetExitCode, uRetFlags;
                        CHECK_ERROR_BREAK(guest, GetProcessStatus(uPID, &uRetExitCode, &uRetFlags, &uRetStatus));
                        RTPrintf("Exit code=%u (Status=%u [%s], Flags=%u)\n", uRetExitCode, uRetStatus, getStatus(uRetStatus), uRetFlags);
                    }
                }
            }
            a->session->Close();
        } while (0);
    }
    return SUCCEEDED(rc) ? 0 : 1;
}

/**
 * Access the guest control store.
 *
 * @returns 0 on success, 1 on failure
 * @note see the command line API description for parameters
 */
int handleGuestControl(HandlerArg *a)
{
    HandlerArg arg = *a;
    arg.argc = a->argc - 1;
    arg.argv = a->argv + 1;

    if (a->argc == 0)
        return errorSyntax(USAGE_GUESTCONTROL, "Incorrect parameters");

    /* switch (cmd) */
    if (   strcmp(a->argv[0], "exec") == 0
        || strcmp(a->argv[0], "execute") == 0)
        return handleExecProgram(&arg);

    /* default: */
    return errorSyntax(USAGE_GUESTCONTROL, "Incorrect parameters");
}
