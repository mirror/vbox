/* $Id$ */
/** @file
 * VBoxManage - The 'guestcontrol' command.
 */

/*
 * Copyright (C) 2010 Sun Microsystems, Inc.
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
#include "VBoxManage.h"

#include <VBox/com/com.h>
#include <VBox/com/string.h>
#include <VBox/com/array.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint.h>

#include <VBox/com/VirtualBox.h>
#include <VBox/com/EventQueue.h>

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

#ifdef RT_OS_DARWIN
# include <CoreFoundation/CFRunLoop.h>
#endif

using namespace com;

/**
 * IVirtualBoxCallback implementation for handling the GuestControlCallback in
 * relation to the "guestcontrol * wait" command.
 */
/** @todo */

void usageGuestControl(void)
{
    RTPrintf("VBoxManage guestcontrol     execute <vmname>|<uuid>\n"
             "                            <path to program> [--arguments \"<arguments>\"] [--environment \"NAME=VALUE NAME=VALUE\"]\n"
             "                            [--flags <flags>] [--timeout <msec>] [--username <name> [--password <password>]]\n"
             "                            [--verbose] [--wait-for exit]\n"
             "\n");
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
    Utf8Str Utf8StdIn;
    Utf8Str Utf8StdOut;
    Utf8Str Utf8StdErr;
    Utf8Str Utf8UserName;
    Utf8Str Utf8Password;
    uint32_t uTimeoutMS = 0;
    bool waitForExit = false;
    bool waitForStdOut = false;
    bool waitForStdErr = false;
    bool verbose = false;

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
                || RTStrToUInt32Full(a->argv[i + 1], 10, &uTimeoutMS) != VINF_SUCCESS)
                usageOK = false;
            else
                ++i;
        }
        else if (!strcmp(a->argv[i], "--wait-for"))
        {
            if (i + 1 >= a->argc)
                usageOK = false;
            else
            {
                if (!strcmp(a->argv[i + 1], "exit"))
                    waitForExit = true;
                else if (!strcmp(a->argv[i + 1], "stdout"))
                    waitForStdOut = true;
                else if (!strcmp(a->argv[i + 1], "stderr"))
                    waitForStdErr = true;
                else
                    usageOK = false;
                ++i;
            }
        }
        else if (!strcmp(a->argv[i], "--verbose"))
        {
            verbose = true;
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

    /* If a password was specified, check if we also got a user name. */
    if (   !Utf8Password.isEmpty()
        &&  Utf8UserName.isEmpty())
    {
        return errorSyntax(USAGE_GUESTCONTROL,
                           "No user name for password specified!");
    }

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

            if (verbose)
            {
                if (uTimeoutMS == 0)
                    RTPrintf("Waiting for guest to start process ...\n");
                else
                    RTPrintf("Waiting for guest to start process (within %ums)\n", uTimeoutMS);
            }

            /* Get current time stamp to later calculate rest of timeout left. */
            uint32_t uStartMS = RTTimeMilliTS();

            /* Execute the process. */
            CHECK_ERROR_BREAK(guest, ExecuteProcess(Bstr(Utf8Cmd), uFlags,
                                                    ComSafeArrayAsInParam(args), ComSafeArrayAsInParam(env),
                                                    Bstr(Utf8StdIn), Bstr(Utf8StdOut), Bstr(Utf8StdErr),
                                                    Bstr(Utf8UserName), Bstr(Utf8Password), uTimeoutMS,
                                                    &uPID, progress.asOutParam()));
            if (verbose) RTPrintf("Process '%s' (PID: %u) started.\n", Utf8Cmd.raw(), uPID);
            if (waitForExit)
            {
                /* Calculate timeout value left after process has been started.  */
                if (uTimeoutMS)
                    uTimeoutMS = uTimeoutMS - (RTTimeMilliTS() - uStartMS);
                if (verbose)
                {
                    if (uTimeoutMS == 0)
                        RTPrintf("Waiting for process to exit ...\n");
                    else
                        RTPrintf("Waiting for process to exit (%ums left) ...\n", uTimeoutMS);
                }

                /* Wait for process to exit ... */
                ASSERT(progress);
                rc = progress->WaitForCompletion(uTimeoutMS == 0 ? -1 /* Wait forever */ : (uTimeoutMS + 5000));
                if (FAILED(rc))
                {
                    if (uTimeoutMS)
                        RTStrmPrintf(g_pStdErr, "Process '%s' (PID: %u) did not end within %ums! Wait aborted.\n",
                                     Utf8Cmd.raw(), uPID, uTimeoutMS);
                    break;
                }
                else
                {
                    BOOL completed;
                    CHECK_ERROR_RET(progress, COMGETTER(Completed)(&completed), rc);
                    ASSERT(completed);

                    LONG iRc;
                    CHECK_ERROR_RET(progress, COMGETTER(ResultCode)(&iRc), rc);
                    if (FAILED(iRc))
                    {
                        ComPtr<IVirtualBoxErrorInfo> execError;
                        rc = progress->COMGETTER(ErrorInfo)(execError.asOutParam());
                        com::ErrorInfo info (execError);
                        GluePrintErrorInfo(info);
                    }
                    else
                    {   
                        if (verbose)
                            RTPrintf("Process completed.\n");
                    }

                    /* Print output if wanted. */
                    if (   waitForStdOut
                        || waitForStdErr)
                    {
                        if (verbose) RTPrintf("Retrieving output data ...\n");
                        while (true)
                        {
                            SafeArray<BYTE> aOutputData;
                            ULONG cbOutputData;
                            CHECK_ERROR_BREAK(guest, GetProcessOutput(uPID, 0 /* aFlags */, _512K, ComSafeArrayAsOutParam(aOutputData)));
                            cbOutputData = aOutputData.size();
                            if (cbOutputData == 0)
                                break;

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
