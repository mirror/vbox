/* $Id$ */
/** @file
 * VBoxManage - Implementation of guestcontrol command.
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

#ifndef VBOX_ONLY_DOCS

#include <VBox/com/com.h>
#include <VBox/com/string.h>
#include <VBox/com/array.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint.h>

#include <VBox/com/VirtualBox.h>
#include <VBox/com/EventQueue.h>

#include <VBox/HostServices/GuestControlSvc.h> /* for PROC_STS_XXX */

#include <iprt/asm.h>
#include <iprt/isofs.h>
#include <iprt/getopt.h>

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
static volatile bool    g_fCopyCanceled = false;

#endif /* VBOX_ONLY_DOCS */

void usageGuestControl(PRTSTREAM pStrm)
{
    RTStrmPrintf(pStrm,
                 "VBoxManage guestcontrol     execute <vmname>|<uuid>\n"
                 "                            <path to program>\n"
                 "                            --username <name> --password <password>\n"
                 "                            [--arguments \"<arguments>\"]\n"
                 "                            [--environment \"<NAME>=<VALUE> [<NAME>=<VALUE>]\"]\n"
                 "                            [--flags <flags>] [--timeout <msec>]\n"
                 "                            [--verbose] [--wait-for exit,stdout,stderr||]\n"
                 /** @todo Add a "--" parameter (has to be last parameter) to directly execute
                  *        stuff, e.g. "VBoxManage guestcontrol execute <VMName> --username <> ... -- /bin/rm -Rf /foo". */
#ifdef VBOX_WITH_COPYTOGUEST
                 "\n"
                 "                            copyto <vmname>|<uuid>\n"
                 "                            <source on host> <destination on guest>\n"
                 "                            [--recursive] [--verbose] [--flags <flags>]\n"
#endif
                 "\n");
}

#ifndef VBOX_ONLY_DOCS

/**
 * Signal handler that sets g_fCanceled.
 *
 * This can be executed on any thread in the process, on Windows it may even be
 * a thread dedicated to delivering this signal.  Do not doing anything
 * unnecessary here.
 */
static void ctrlExecProcessSignalHandler(int iSignal)
{
    NOREF(iSignal);
    ASMAtomicWriteBool(&g_fExecCanceled, true);
}

static const char *ctrlExecGetStatus(ULONG uStatus)
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

static int handleCtrlExecProgram(HandlerArg *a)
{
    /*
     * Check the syntax.  We can deduce the correct syntax from the number of
     * arguments.
     */
    if (a->argc < 2) /* At least the command we want to execute in the guest should be present :-). */
        return errorSyntax(USAGE_GUESTCONTROL, "Incorrect parameters");

    Utf8Str Utf8Cmd(a->argv[1]);
    uint32_t uFlags = 0;
    /* Note: this uses IN_BSTR as it must be BSTR on COM and CBSTR on XPCOM */
    com::SafeArray<IN_BSTR> args;
    com::SafeArray<IN_BSTR> env;
    Utf8Str Utf8UserName;
    Utf8Str Utf8Password;
    uint32_t u32TimeoutMS = 0;
    bool fWaitForExit = false;
    bool fWaitForStdOut = false;
    bool fWaitForStdErr = false;
    bool fVerbose = false;
    bool fTimeout = false;

    /* Always use the actual command line as argv[0]. */
    args.push_back(Bstr(Utf8Cmd).raw());

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
                        args.push_back(Bstr(papszArg[j]).raw());

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
                        env.push_back(Bstr(papszArg[j]).raw());

                    RTGetOptArgvFree(papszArg);
                }
                ++i;
            }
        }
        else if (!strcmp(a->argv[i], "--flags"))
        {
            if (i + 1 >= a->argc)
                usageOK = false;
            else
            {
                /** @todo Needs a bit better processing as soon as we have more flags. */
                if (!strcmp(a->argv[i + 1], "ignoreorphanedprocesses"))
                    uFlags |= ExecuteProcessFlag_IgnoreOrphanedProcesses;
                else
                    usageOK = false;
                ++i;
            }
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
            fVerbose = true;
        /** @todo Add fancy piping stuff here. */
        else
            return errorSyntax(USAGE_GUESTCONTROL,
                               "Invalid parameter '%s'", Utf8Str(a->argv[i]).c_str());
    }

    if (!usageOK)
        return errorSyntax(USAGE_GUESTCONTROL, "Incorrect parameters");

    if (Utf8UserName.isEmpty())
        return errorSyntax(USAGE_GUESTCONTROL,
                           "No user name specified!");

    /* lookup VM. */
    ComPtr<IMachine> machine;
    HRESULT rc;
    CHECK_ERROR(a->virtualBox, FindMachine(Bstr(a->argv[0]).raw(),
                                           machine.asOutParam()));
    if (machine)
    {
        do
        {
            /* open an existing session for VM */
            CHECK_ERROR_BREAK(machine, LockMachine(a->session, LockType_Shared));
            // @todo r=dj assert that it's an existing session

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
            rc = guest->ExecuteProcess(Bstr(Utf8Cmd).raw(), uFlags,
                                       ComSafeArrayAsInParam(args),
                                       ComSafeArrayAsInParam(env),
                                       Bstr(Utf8UserName).raw(),
                                       Bstr(Utf8Password).raw(), u32TimeoutMS,
                                       &uPID, progress.asOutParam());
            if (FAILED(rc))
            {
                /* If we got a VBOX_E_IPRT error we handle the error in a more gentle way
                 * because it contains more accurate info about what went wrong. */
                ErrorInfo info(guest, COM_IIDOF(IGuest));
                if (info.isFullAvailable())
                {
                    if (rc == VBOX_E_IPRT_ERROR)
                        RTMsgError("%ls.", info.getText().raw());
                    else
                        RTMsgError("%ls (%Rhrc).", info.getText().raw(), info.getResultCode());
                }
                break;
            }
            if (fVerbose)
                RTPrintf("Process '%s' (PID: %u) started\n", Utf8Cmd.c_str(), uPID);
            if (fWaitForExit)
            {
                if (fTimeout)
                {
                    /* Calculate timeout value left after process has been started.  */
                    uint64_t u64Elapsed = RTTimeMilliTS() - u64StartMS;
                    /* Is timeout still bigger than current difference? */
                    if (u32TimeoutMS > u64Elapsed)
                    {
                        u32TimeoutMS -= (uint32_t)u64Elapsed;
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
                    signal(SIGINT,   ctrlExecProcessSignalHandler);
            #ifdef SIGBREAK
                    signal(SIGBREAK, ctrlExecProcessSignalHandler);
            #endif
                }

                /* Wait for process to exit ... */
                BOOL fCompleted = FALSE;
                BOOL fCanceled = FALSE;
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
                        rc = guest->GetProcessOutput(uPID, 0 /* aFlags */,
                                                     u32TimeoutMS, _64K, ComSafeArrayAsOutParam(aOutputData));
                        if (FAILED(rc))
                        {
                            /* If we got a VBOX_E_IPRT error we handle the error in a more gentle way
                             * because it contains more accurate info about what went wrong. */
                            ErrorInfo info(guest, COM_IIDOF(IGuest));
                            if (info.isFullAvailable())
                            {
                                if (rc == VBOX_E_IPRT_ERROR)
                                    RTMsgError("%ls.", info.getText().raw());
                                else
                                    RTMsgError("%ls (%Rhrc).", info.getText().raw(), info.getResultCode());
                            }
                            cbOutputData = 0;
                            fCompleted = true; /* rc contains a failure, so we'll go into aborted state down below. */
                        }
                        else
                        {
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
                    }

#if 0
                    static int sent = 0;
                    if (sent < 1)
                    {
                        RTISOFSFILE file;
                        int vrc = RTIsoFsOpen(&file, "c:\\Downloads\\VBoxGuestAdditions_3.2.8.iso");
                        if (RT_SUCCESS(vrc))
                        {
                            uint32_t cbOffset;
                            size_t cbLength;
                            vrc = RTIsoFsGetFileInfo(&file, "VBOXWINDOWSADDITIONS_X86.EXE", &cbOffset, &cbLength);
                            //vrc = RTIsoFsGetFileInfo(&file, "32BIT/README.TXT", &cbOffset, &cbLength);
                            if (   RT_SUCCESS(vrc)
                                && cbOffset
                                && cbLength)
                            {
                                vrc = RTFileSeek(file.file, cbOffset, RTFILE_SEEK_BEGIN, NULL);

                                size_t cbRead;
                                size_t cbToRead;
                                SafeArray<BYTE> aInputData(_64K);
                                while (   cbLength
                                       && RT_SUCCESS(vrc))
                                {
                                    cbToRead = RT_MIN(cbLength, _64K);
                                    vrc = RTFileRead(file.file, (uint8_t*)aInputData.raw(), cbToRead, &cbRead);
                                    if (   cbRead
                                        && RT_SUCCESS(vrc))
                                    {
                                        aInputData.resize(cbRead);

                                        /* Did we reach the end of the content
                                         * we want to transfer (last chunk)? */
                                        ULONG uFlags = ProcessInputFlag_None;
                                        if (cbLength < _64K)
                                        {
                                            RTPrintf("Last!\n");
                                            uFlags |= ProcessInputFlag_EndOfFile;
                                        }

                                        ULONG uBytesWritten;
                                        rc = guest->SetProcessInput(uPID, uFlags,
                                                                    ComSafeArrayAsInParam(aInputData), &uBytesWritten);
                                        if (FAILED(rc))
                                        {
                                            /* If we got a VBOX_E_IPRT error we handle the error in a more gentle way
                                             * because it contains more accurate info about what went wrong. */
                                            ErrorInfo info(guest, COM_IIDOF(IGuest));
                                            if (info.isFullAvailable())
                                            {
                                                if (rc == VBOX_E_IPRT_ERROR)
                                                    RTMsgError("%ls.", info.getText().raw());
                                                else
                                                    RTMsgError("%ls (%Rhrc).", info.getText().raw(), info.getResultCode());
                                            }
                                            break;
                                        }
                                        else
                                        {
                                            //Assert(uTransfered == cbRead);
                                            cbLength -= uBytesWritten;
                                            RTPrintf("Left: %u\n", cbLength);
                                        }
                                    }
                                }
                                RTPrintf("Finished\n");
                            }
                            RTIsoFsClose(&file);
                        }
                    }
                    sent++;
#endif
                    if (cbOutputData <= 0) /* No more output data left? */
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

                    /* Process async cancelation */
                    if (g_fExecCanceled && !fCanceledAlready)
                    {
                        hrc = progress->Cancel();
                        if (SUCCEEDED(hrc))
                            fCanceledAlready = TRUE;
                        else
                            g_fExecCanceled = false;
                    }

                    /* Progress canceled by Main API? */
                    if (   SUCCEEDED(progress->COMGETTER(Canceled(&fCanceled)))
                        && fCanceled)
                    {
                        break;
                    }
                }

                /* Undo signal handling */
                if (fCancelable)
                {
                    signal(SIGINT,   SIG_DFL);
            #ifdef SIGBREAK
                    signal(SIGBREAK, SIG_DFL);
            #endif
                }

                if (fCanceled)
                {
                    if (fVerbose)
                        RTPrintf("Process execution canceled!\n");
                }
                else if (   fCompleted
                         && SUCCEEDED(rc))
                {
                    LONG iRc = false;
                    CHECK_ERROR_RET(progress, COMGETTER(ResultCode)(&iRc), rc);
                    if (FAILED(iRc))
                    {
                        com::ProgressErrorInfo info(progress);
                        if (   info.isFullAvailable()
                            || info.isBasicAvailable())
                        {
                            /* If we got a VBOX_E_IPRT error we handle the error in a more gentle way
                             * because it contains more accurate info about what went wrong. */
                            if (info.getResultCode() == VBOX_E_IPRT_ERROR)
                                RTMsgError("%ls.", info.getText().raw());
                            else
                            {
                                RTMsgError("Process error details:");
                                GluePrintErrorInfo(info);
                            }
                        }
                        else
                            com::GluePrintRCMessage(iRc);
                    }
                    else if (fVerbose)
                    {
                        ULONG uRetStatus, uRetExitCode, uRetFlags;
                        rc = guest->GetProcessStatus(uPID, &uRetExitCode, &uRetFlags, &uRetStatus);
                        if (SUCCEEDED(rc))
                            RTPrintf("Exit code=%u (Status=%u [%s], Flags=%u)\n", uRetExitCode, uRetStatus, ctrlExecGetStatus(uRetStatus), uRetFlags);
                    }
                }
                else
                {
                    if (fVerbose)
                        RTPrintf("Process execution aborted!\n");
                }
            }
            a->session->UnlockMachine();
        } while (0);
    }
    return SUCCEEDED(rc) ? 0 : 1;
}

#ifdef VBOX_WITH_COPYTOGUEST
/**
 * Signal handler that sets g_fCopyCanceled.
 *
 * This can be executed on any thread in the process, on Windows it may even be
 * a thread dedicated to delivering this signal.  Do not doing anything
 * unnecessary here.
 */
static void ctrlCopySignalHandler(int iSignal)
{
    NOREF(iSignal);
    ASMAtomicWriteBool(&g_fCopyCanceled, true);
}

static int handleCtrlCopyTo(HandlerArg *a)
{
    /*
     * Check the syntax.  We can deduce the correct syntax from the number of
     * arguments.
     */
    if (a->argc < 3) /* At least the source + destination should be present :-). */
        return errorSyntax(USAGE_GUESTCONTROL, "Incorrect parameters");

    Utf8Str Utf8Source(a->argv[1]);
    Utf8Str Utf8Dest(a->argv[2]);
    uint32_t uFlags = 0;
    bool fVerbose = false;
    bool fCopyRecursive = false;

    /* Iterate through all possible commands (if available). */
    bool usageOK = true;
    for (int i = 3; usageOK && i < a->argc; i++)
    {
        if (!strcmp(a->argv[i], "--flags"))
        {
            if (i + 1 >= a->argc)
                usageOK = false;
            else
            {
                /* Nothing to do here yet. */
                ++i;
            }
        }
        else if (   !strcmp(a->argv[i], "--recursive")
                 || !strcmp(a->argv[i], "--r"))
        {
            uFlags |= CopyFileFlag_Recursive;
        }
        else if (   !strcmp(a->argv[i], "--update")
                 || !strcmp(a->argv[i], "--u"))
        {
            uFlags |= CopyFileFlag_Update;
        }
        else if (   !strcmp(a->argv[i], "--follow")
                 || !strcmp(a->argv[i], "--f"))
        {
            uFlags |= CopyFileFlag_FollowLinks;
        }
        /** @todo Add force flag for overwriting existing stuff. */
        else if (!strcmp(a->argv[i], "--verbose"))
            fVerbose = true;
        else
            return errorSyntax(USAGE_GUESTCONTROL,
                               "Invalid parameter '%s'", Utf8Str(a->argv[i]).c_str());
    }

    if (!usageOK)
        return errorSyntax(USAGE_GUESTCONTROL, "Incorrect parameters");

    if (Utf8Source.isEmpty())
        return errorSyntax(USAGE_GUESTCONTROL,
                           "No source specified!");

    if (Utf8Dest.isEmpty())
        return errorSyntax(USAGE_GUESTCONTROL,
                           "No destination specified!");

    /* lookup VM. */
    ComPtr<IMachine> machine;
    /* assume it's an UUID */
    HRESULT rc;
    CHECK_ERROR(a->virtualBox, FindMachine(Bstr(a->argv[0]).raw(),
                                           machine.asOutParam()));
    if (machine)
    {
        do
        {
            /* open an existing session for VM */
            CHECK_ERROR_BREAK(machine, LockMachine(a->session, LockType_Shared));
            // @todo r=dj assert that it's an existing session

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
                if (fCopyRecursive)
                    RTPrintf("Recursively copying \"%s\" to \"%s\" ...\n", Utf8Source, Utf8Dest);
                else
                    RTPrintf("Copying \"%s\" to \"%s\" ...\n", Utf8Source, Utf8Dest);
            }

            /* Do the copy. */
            rc = guest->CopyToGuest(Bstr(Utf8Source).raw(), Bstr(Utf8Dest).raw(),
                                    uFlags, progress.asOutParam());
            if (FAILED(rc))
            {
                /* If we got a VBOX_E_IPRT error we handle the error in a more gentle way
                 * because it contains more accurate info about what went wrong. */
                ErrorInfo info(guest, COM_IIDOF(IGuest));
                if (info.isFullAvailable())
                {
                    if (rc == VBOX_E_IPRT_ERROR)
                        RTMsgError("%ls.", info.getText().raw());
                    else
                        RTMsgError("%ls (%Rhrc).", info.getText().raw(), info.getResultCode());
                }
                break;
            }
            else
            {
                /* setup signal handling if cancelable */
                ASSERT(progress);
                bool fCanceledAlready = false;
                BOOL fCancelable;
                HRESULT hrc = progress->COMGETTER(Cancelable)(&fCancelable);
                if (FAILED(hrc))
                    fCancelable = FALSE;
                if (fCancelable)
                {
                    signal(SIGINT,   ctrlCopySignalHandler);
            #ifdef SIGBREAK
                    signal(SIGBREAK, ctrlCopySignalHandler);
            #endif
                }

                /* Wait for process to exit ... */
                BOOL fCompleted = FALSE;
                BOOL fCanceled = FALSE;
                while (SUCCEEDED(progress->COMGETTER(Completed(&fCompleted))))
                {
                    /* Process async cancelation */
                    if (g_fCopyCanceled && !fCanceledAlready)
                    {
                        hrc = progress->Cancel();
                        if (SUCCEEDED(hrc))
                            fCanceledAlready = TRUE;
                        else
                            g_fCopyCanceled = false;
                    }

                    /* Progress canceled by Main API? */
                    if (   SUCCEEDED(progress->COMGETTER(Canceled(&fCanceled)))
                        && fCanceled)
                    {
                        break;
                    }
                }

                /* Undo signal handling */
                if (fCancelable)
                {
                    signal(SIGINT,   SIG_DFL);
            #ifdef SIGBREAK
                    signal(SIGBREAK, SIG_DFL);
            #endif
                }

                if (fCanceled)
                {
                    if (fVerbose)
                        RTPrintf("Copy operation canceled!\n");
                }
                else if (   fCompleted
                         && SUCCEEDED(rc))
                {
                    LONG iRc = false;
                    CHECK_ERROR_RET(progress, COMGETTER(ResultCode)(&iRc), rc);
                    if (FAILED(iRc))
                    {
                        com::ProgressErrorInfo info(progress);
                        if (   info.isFullAvailable()
                            || info.isBasicAvailable())
                        {
                            /* If we got a VBOX_E_IPRT error we handle the error in a more gentle way
                             * because it contains more accurate info about what went wrong. */
                            if (info.getResultCode() == VBOX_E_IPRT_ERROR)
                                RTMsgError("%ls.", info.getText().raw());
                            else
                            {
                                RTMsgError("Copy operation error details:");
                                GluePrintErrorInfo(info);
                            }
                        }
                        else
                        {
                            if (RT_FAILURE(rc))
                                RTMsgError("Error while looking up error code, rc=%Rrc", rc);
                            else
                                com::GluePrintRCMessage(iRc);
                        }
                    }
                    else if (fVerbose)
                    {
                        RTPrintf("Copy operation successful!\n");
                    }
                }
                else
                {
                    if (fVerbose)
                        RTPrintf("Copy operation aborted!\n");
                }
            }
            a->session->UnlockMachine();
        } while (0);
    }
    return SUCCEEDED(rc) ? 0 : 1;
}
#endif

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
    {
        return handleCtrlExecProgram(&arg);
    }
#ifdef VBOX_WITH_COPYTOGUEST
    else if (   strcmp(a->argv[0], "copyto")  == 0
             || strcmp(a->argv[0], "copy_to") == 0)
    {
        return handleCtrlCopyTo(&arg);
    }
#endif

    /* default: */
    return errorSyntax(USAGE_GUESTCONTROL, "Incorrect parameters");
}

#endif /* !VBOX_ONLY_DOCS */
