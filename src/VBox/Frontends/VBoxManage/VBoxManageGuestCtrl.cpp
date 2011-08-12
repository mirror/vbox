/* $Id$ */
/** @file
 * VBoxManage - Implementation of guestcontrol command.
 */

/*
 * Copyright (C) 2010-2011 Oracle Corporation
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

#include <VBox/err.h>
#include <VBox/log.h>

#include <iprt/asm.h>
#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/isofs.h>
#include <iprt/getopt.h>
#include <iprt/list.h>
#include <iprt/path.h>
#include <iprt/thread.h>

#include <map>
#include <vector>

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
static volatile bool    g_fGuestCtrlCanceled = false;

typedef struct COPYCONTEXT
{
    IGuest *pGuest;
    bool fVerbose;
    bool fHostToGuest;
    char *pszUsername;
    char *pszPassword;
} COPYCONTEXT, *PCOPYCONTEXT;

/**
 * An entry for a source element, including an optional DOS-like wildcard (*,?).
 */
typedef struct SOURCEFILEENTRY
{
    SOURCEFILEENTRY(const char *pszSource, const char *pszFilter)
                    : mSource(pszSource),
                      mFilter(pszFilter) {}
    SOURCEFILEENTRY(const char *pszSource)
                    : mSource(pszSource)
    {
        if (   !RTFileExists(pszSource)
            && !RTDirExists(pszSource))
        {
            /* No file and no directory -- maybe a filter? */
            char *pszFilename = RTPathFilename(pszSource);
            if (   pszFilename
                && strpbrk(pszFilename, "*?"))
            {
                /* Yep, get the actual filter part. */
                mFilter = RTPathFilename(pszSource);
                /* Remove the filter from actual sourcec directory name. */
                RTPathStripFilename(mSource.mutableRaw());
                mSource.jolt();
            }
        }
    }
    Utf8Str mSource;
    Utf8Str mFilter;
} SOURCEFILEENTRY, *PSOURCEFILEENTRY;
typedef std::vector<SOURCEFILEENTRY> SOURCEVEC, *PSOURCEVEC;

/**
 * An entry for an element which needs to be copied/created to/on the guest.
 */
typedef struct DESTFILEENTRY
{
    DESTFILEENTRY(Utf8Str strFileName) : mFileName(strFileName) {}
    Utf8Str mFileName;
} DESTFILEENTRY, *PDESTFILEENTRY;
/*
 * Map for holding destination entires, whereas the key is the destination
 * directory and the mapped value is a vector holding all elements for this directoy.
 */
typedef std::map< Utf8Str, std::vector<DESTFILEENTRY> > DESTDIRMAP, *PDESTDIRMAP;
typedef std::map< Utf8Str, std::vector<DESTFILEENTRY> >::iterator DESTDIRMAPITER, *PDESTDIRMAPITER;

/**
 * Special exit codes for returning errors/information of a
 * started guest process to the command line VBoxManage was started from.
 * Useful for e.g. scripting.
 *
 * @note    These are frozen as of 4.1.0.
 */
enum EXITCODEEXEC
{
    EXITCODEEXEC_SUCCESS        = RTEXITCODE_SUCCESS,
    /* Process exited normally but with an exit code <> 0. */
    EXITCODEEXEC_CODE           = 16,
    EXITCODEEXEC_FAILED         = 17,
    EXITCODEEXEC_TERM_SIGNAL    = 18,
    EXITCODEEXEC_TERM_ABEND     = 19,
    EXITCODEEXEC_TIMEOUT        = 20,
    EXITCODEEXEC_DOWN           = 21,
    EXITCODEEXEC_CANCELED       = 22
};

/**
 * RTGetOpt-IDs for the guest execution control command line.
 */
enum GETOPTDEF_EXEC
{
    GETOPTDEF_EXEC_IGNOREORPHANEDPROCESSES = 1000,
    GETOPTDEF_EXEC_NO_PROFILE,
    GETOPTDEF_EXEC_OUTPUTFORMAT,
    GETOPTDEF_EXEC_DOS2UNIX,
    GETOPTDEF_EXEC_UNIX2DOS,
    GETOPTDEF_EXEC_WAITFOREXIT,
    GETOPTDEF_EXEC_WAITFORSTDOUT,
    GETOPTDEF_EXEC_WAITFORSTDERR
};

enum GETOPTDEF_COPYFROM
{
    GETOPTDEF_COPYFROM_DRYRUN = 1000,
    GETOPTDEF_COPYFROM_FOLLOW,
    GETOPTDEF_COPYFROM_PASSWORD,
    GETOPTDEF_COPYFROM_TARGETDIR,
    GETOPTDEF_COPYFROM_USERNAME
};

enum GETOPTDEF_COPYTO
{
    GETOPTDEF_COPYTO_DRYRUN = 1000,
    GETOPTDEF_COPYTO_FOLLOW,
    GETOPTDEF_COPYTO_PASSWORD,
    GETOPTDEF_COPYTO_TARGETDIR,
    GETOPTDEF_COPYTO_USERNAME
};

enum GETOPTDEF_MKDIR
{
    GETOPTDEF_MKDIR_PASSWORD = 1000,
    GETOPTDEF_MKDIR_USERNAME
};

enum GETOPTDEF_STAT
{
    GETOPTDEF_STAT_PASSWORD = 1000,
    GETOPTDEF_STAT_USERNAME
};

enum OUTPUTTYPE
{
    OUTPUTTYPE_UNDEFINED = 0,
    OUTPUTTYPE_DOS2UNIX  = 10,
    OUTPUTTYPE_UNIX2DOS  = 20
};

#endif /* VBOX_ONLY_DOCS */

void usageGuestControl(PRTSTREAM pStrm)
{
    RTStrmPrintf(pStrm,
                 "VBoxManage guestcontrol     <vmname>|<uuid>\n"
                 "                            exec[ute]\n"
                 "                            --image <path to program>\n"
                 "                            --username <name> --password <password>\n"
                 "                            [--dos2unix]\n"
                 "                            [--environment \"<NAME>=<VALUE> [<NAME>=<VALUE>]\"]\n"
                 "                            [--timeout <msec>] [--unix2dos] [--verbose]\n"
                 "                            [--wait-exit] [--wait-stdout] [--wait-stderr]\n"
                 "                            [-- [<argument1>] ... [<argumentN>]]\n"
                 /** @todo Add a "--" parameter (has to be last parameter) to directly execute
                  *        stuff, e.g. "VBoxManage guestcontrol execute <VMName> --username <> ... -- /bin/rm -Rf /foo". */
                 "\n"
                 "                            copyfrom\n"
                 "                            <source on guest> <destination on host>\n"
                 "                            --username <name> --password <password>\n"
                 "                            [--dryrun] [--follow] [--recursive] [--verbose]\n"
                 "\n"
                 "                            copyto|cp\n"
                 "                            <source on host> <destination on guest>\n"
                 "                            --username <name> --password <password>\n"
                 "                            [--dryrun] [--follow] [--recursive] [--verbose]\n"
                 "\n"
                 "                            createdir[ectory]|mkdir|md\n"
                 "                            <director[y|ies] to create on guest>\n"
                 "                            --username <name> --password <password>\n"
                 "                            [--parents] [--mode <mode>] [--verbose]\n"
                 "\n"
                 "                            stat\n"
                 "                            <file element(s) to check on guest>\n"
                 "                            --username <name> --password <password>\n"
                 "                            [--verbose]\n"
                 "\n"
                 "                            updateadditions\n"
                 "                            [--source <guest additions .ISO>] [--verbose]\n"
                 "\n");
}

#ifndef VBOX_ONLY_DOCS

/**
 * Signal handler that sets g_fGuestCtrlCanceled.
 *
 * This can be executed on any thread in the process, on Windows it may even be
 * a thread dedicated to delivering this signal.  Do not doing anything
 * unnecessary here.
 */
static void guestCtrlSignalHandler(int iSignal)
{
    NOREF(iSignal);
    ASMAtomicWriteBool(&g_fGuestCtrlCanceled, true);
}

/**
 * Installs a custom signal handler to get notified
 * whenever the user wants to intercept the program.
 */
static void ctrlSignalHandlerInstall()
{
    signal(SIGINT,   guestCtrlSignalHandler);
#ifdef SIGBREAK
    signal(SIGBREAK, guestCtrlSignalHandler);
#endif
}

/**
 * Uninstalls a previously installed signal handler.
 */
static void ctrlSignalHandlerUninstall()
{
    signal(SIGINT,   SIG_DFL);
#ifdef SIGBREAK
    signal(SIGBREAK, SIG_DFL);
#endif
}

/**
 * Translates a process status to a human readable
 * string.
 */
static const char *ctrlExecProcessStatusToText(ExecuteProcessStatus_T enmStatus)
{
    switch (enmStatus)
    {
        case ExecuteProcessStatus_Started:
            return "started";
        case ExecuteProcessStatus_TerminatedNormally:
            return "successfully terminated";
        case ExecuteProcessStatus_TerminatedSignal:
            return "terminated by signal";
        case ExecuteProcessStatus_TerminatedAbnormally:
            return "abnormally aborted";
        case ExecuteProcessStatus_TimedOutKilled:
            return "timed out";
        case ExecuteProcessStatus_TimedOutAbnormally:
            return "timed out, hanging";
        case ExecuteProcessStatus_Down:
            return "killed";
        case ExecuteProcessStatus_Error:
            return "error";
        default:
            break;
    }
    return "unknown";
}

static int ctrlExecProcessStatusToExitCode(ExecuteProcessStatus_T enmStatus, ULONG uExitCode)
{
    int rc = EXITCODEEXEC_SUCCESS;
    switch (enmStatus)
    {
        case ExecuteProcessStatus_Started:
            rc = EXITCODEEXEC_SUCCESS;
            break;
        case ExecuteProcessStatus_TerminatedNormally:
            rc = !uExitCode ? EXITCODEEXEC_SUCCESS : EXITCODEEXEC_CODE;
            break;
        case ExecuteProcessStatus_TerminatedSignal:
            rc = EXITCODEEXEC_TERM_SIGNAL;
            break;
        case ExecuteProcessStatus_TerminatedAbnormally:
            rc = EXITCODEEXEC_TERM_ABEND;
            break;
        case ExecuteProcessStatus_TimedOutKilled:
            rc = EXITCODEEXEC_TIMEOUT;
            break;
        case ExecuteProcessStatus_TimedOutAbnormally:
            rc = EXITCODEEXEC_TIMEOUT;
            break;
        case ExecuteProcessStatus_Down:
            /* Service/OS is stopping, process was killed, so
             * not exactly an error of the started process ... */
            rc = EXITCODEEXEC_DOWN;
            break;
        case ExecuteProcessStatus_Error:
            rc = EXITCODEEXEC_FAILED;
            break;
        default:
            AssertMsgFailed(("Unknown exit code (%u) from guest process returned!\n", enmStatus));
            break;
    }
    return rc;
}

static int ctrlPrintError(com::ErrorInfo &errorInfo)
{
    if (   errorInfo.isFullAvailable()
        || errorInfo.isBasicAvailable())
    {
        /* If we got a VBOX_E_IPRT error we handle the error in a more gentle way
         * because it contains more accurate info about what went wrong. */
        if (errorInfo.getResultCode() == VBOX_E_IPRT_ERROR)
            RTMsgError("%ls.", errorInfo.getText().raw());
        else
        {
            RTMsgError("Error details:");
            GluePrintErrorInfo(errorInfo);
        }
        return VERR_GENERAL_FAILURE; /** @todo */
    }
    AssertMsgFailedReturn(("Object has indicated no error (%Rrc)!?\n", errorInfo.getResultCode()),
                          VERR_INVALID_PARAMETER);
}

static int ctrlPrintError(IUnknown *pObj, const GUID &aIID)
{
    com::ErrorInfo ErrInfo(pObj, aIID);
    return ctrlPrintError(ErrInfo);
}

static int ctrlPrintProgressError(ComPtr<IProgress> progress)
{
    int rc;
    BOOL fCanceled;
    if (   SUCCEEDED(progress->COMGETTER(Canceled(&fCanceled)))
        && fCanceled)
    {
        rc = VERR_CANCELLED;
    }
    else
    {
        com::ProgressErrorInfo ErrInfo(progress);
        rc = ctrlPrintError(ErrInfo);
    }
    return rc;
}

/**
 * Un-initializes the VM after guest control usage.
 */
static void ctrlUninitVM(HandlerArg *pArg)
{
    AssertPtrReturnVoid(pArg);
    if (pArg->session)
        pArg->session->UnlockMachine();
}

/**
 * Initializes the VM for IGuest operations.
 *
 * That is, checks whether it's up and running, if it can be locked (shared
 * only) and returns a valid IGuest pointer on success.
 *
 * @return  IPRT status code.
 * @param   pArg            Our command line argument structure.
 * @param   pszNameOrId     The VM's name or UUID.
 * @param   pGuest          Where to return the IGuest interface pointer.
 */
static int ctrlInitVM(HandlerArg *pArg, const char *pszNameOrId, ComPtr<IGuest> *pGuest)
{
    AssertPtrReturn(pArg, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszNameOrId, VERR_INVALID_PARAMETER);

    /* Lookup VM. */
    ComPtr<IMachine> machine;
    /* Assume it's an UUID. */
    HRESULT rc;
    CHECK_ERROR(pArg->virtualBox, FindMachine(Bstr(pszNameOrId).raw(),
                                              machine.asOutParam()));
    if (FAILED(rc))
        return VERR_NOT_FOUND;

    /* Machine is running? */
    MachineState_T machineState;
    CHECK_ERROR_RET(machine, COMGETTER(State)(&machineState), 1);
    if (machineState != MachineState_Running)
    {
        RTMsgError("Machine \"%s\" is not running (currently %s)!\n",
                   pszNameOrId, machineStateToName(machineState, false));
        return VERR_VM_INVALID_VM_STATE;
    }

    do
    {
        /* Open a session for the VM. */
        CHECK_ERROR_BREAK(machine, LockMachine(pArg->session, LockType_Shared));
        /* Get the associated console. */
        ComPtr<IConsole> console;
        CHECK_ERROR_BREAK(pArg->session, COMGETTER(Console)(console.asOutParam()));
        /* ... and session machine. */
        ComPtr<IMachine> sessionMachine;
        CHECK_ERROR_BREAK(pArg->session, COMGETTER(Machine)(sessionMachine.asOutParam()));
        /* Get IGuest interface. */
        CHECK_ERROR_BREAK(console, COMGETTER(Guest)(pGuest->asOutParam()));
    } while (0);

    if (FAILED(rc))
        ctrlUninitVM(pArg);
    return SUCCEEDED(rc) ? VINF_SUCCESS : VERR_GENERAL_FAILURE;
}

/* <Missing docuemntation> */
static int handleCtrlExecProgram(ComPtr<IGuest> guest, HandlerArg *pArg)
{
    AssertPtrReturn(pArg, VERR_INVALID_PARAMETER);

    /*
     * Parse arguments.
     */
    if (pArg->argc < 2) /* At least the command we want to execute in the guest should be present :-). */
        return errorSyntax(USAGE_GUESTCONTROL, "Incorrect parameters");

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--dos2unix",                     GETOPTDEF_EXEC_DOS2UNIX,                  RTGETOPT_REQ_NOTHING },
        { "--environment",                  'e',                                      RTGETOPT_REQ_STRING  },
        { "--flags",                        'f',                                      RTGETOPT_REQ_STRING  },
        { "--ignore-operhaned-processes",   GETOPTDEF_EXEC_IGNOREORPHANEDPROCESSES,   RTGETOPT_REQ_NOTHING },
        { "--image",                        'i',                                      RTGETOPT_REQ_STRING  },
        { "--no-profile",                   GETOPTDEF_EXEC_NO_PROFILE,                RTGETOPT_REQ_NOTHING },
        { "--password",                     'p',                                      RTGETOPT_REQ_STRING  },
        { "--timeout",                      't',                                      RTGETOPT_REQ_UINT32  },
        { "--unix2dos",                     GETOPTDEF_EXEC_UNIX2DOS,                  RTGETOPT_REQ_NOTHING },
        { "--username",                     'u',                                      RTGETOPT_REQ_STRING  },
        { "--verbose",                      'v',                                      RTGETOPT_REQ_NOTHING },
        { "--wait-exit",                    GETOPTDEF_EXEC_WAITFOREXIT,               RTGETOPT_REQ_NOTHING },
        { "--wait-stdout",                  GETOPTDEF_EXEC_WAITFORSTDOUT,             RTGETOPT_REQ_NOTHING },
        { "--wait-stderr",                  GETOPTDEF_EXEC_WAITFORSTDERR,             RTGETOPT_REQ_NOTHING }
    };

    int                     ch;
    RTGETOPTUNION           ValueUnion;
    RTGETOPTSTATE           GetState;
    RTGetOptInit(&GetState, pArg->argc, pArg->argv, s_aOptions, RT_ELEMENTS(s_aOptions), 0, 0);

    Utf8Str                 Utf8Cmd;
    uint32_t                fExecFlags = ExecuteProcessFlag_None;
    uint32_t                fOutputFlags = ProcessOutputFlag_None;
    com::SafeArray<IN_BSTR> args;
    com::SafeArray<IN_BSTR> env;
    Utf8Str                 Utf8UserName;
    Utf8Str                 Utf8Password;
    uint32_t                cMsTimeout      = 0;
    OUTPUTTYPE              eOutputType     = OUTPUTTYPE_UNDEFINED;
    bool                    fOutputBinary   = false;
    bool                    fWaitForExit    = false;
    bool                    fWaitForStdOut  = false;
    bool                    fVerbose        = false;

    int                     vrc             = VINF_SUCCESS;
    while (   (ch = RTGetOpt(&GetState, &ValueUnion))
           && RT_SUCCESS(vrc))
    {
        /* For options that require an argument, ValueUnion has received the value. */
        switch (ch)
        {
            case GETOPTDEF_EXEC_DOS2UNIX:
                if (eOutputType != OUTPUTTYPE_UNDEFINED)
                    return errorSyntax(USAGE_GUESTCONTROL, "More than one output type (dos2unix/unix2dos) specified!");
                eOutputType = OUTPUTTYPE_DOS2UNIX;
                break;

            case 'e': /* Environment */
            {
                char **papszArg;
                int cArgs;

                vrc = RTGetOptArgvFromString(&papszArg, &cArgs, ValueUnion.psz, NULL);
                if (RT_FAILURE(vrc))
                    return errorSyntax(USAGE_GUESTCONTROL, "Failed to parse environment value, rc=%Rrc", vrc);
                for (int j = 0; j < cArgs; j++)
                    env.push_back(Bstr(papszArg[j]).raw());

                RTGetOptArgvFree(papszArg);
                break;
            }

            case GETOPTDEF_EXEC_IGNOREORPHANEDPROCESSES:
                fExecFlags |= ExecuteProcessFlag_IgnoreOrphanedProcesses;
                break;

            case GETOPTDEF_EXEC_NO_PROFILE:
                fExecFlags |= ExecuteProcessFlag_NoProfile;
                break;

            case 'i':
                Utf8Cmd = ValueUnion.psz;
                break;

            /** @todo Add a hidden flag. */

            case 'p': /* Password */
                Utf8Password = ValueUnion.psz;
                break;

            case 't': /* Timeout */
                cMsTimeout = ValueUnion.u32;
                break;

            case GETOPTDEF_EXEC_UNIX2DOS:
                if (eOutputType != OUTPUTTYPE_UNDEFINED)
                    return errorSyntax(USAGE_GUESTCONTROL, "More than one output type (dos2unix/unix2dos) specified!");
                eOutputType = OUTPUTTYPE_UNIX2DOS;
                break;

            case 'u': /* User name */
                Utf8UserName = ValueUnion.psz;
                break;

            case 'v': /* Verbose */
                fVerbose = true;
                break;

            case GETOPTDEF_EXEC_WAITFOREXIT:
                fWaitForExit = true;
                break;

            case GETOPTDEF_EXEC_WAITFORSTDOUT:
                fWaitForExit = true;
                fWaitForStdOut = true;
                break;

            case GETOPTDEF_EXEC_WAITFORSTDERR:
                fWaitForExit = (fOutputFlags |= ProcessOutputFlag_StdErr) ? true : false;
                break;

            case VINF_GETOPT_NOT_OPTION:
            {
                if (args.size() == 0 && Utf8Cmd.isEmpty())
                    Utf8Cmd = ValueUnion.psz;
                else
                    args.push_back(Bstr(ValueUnion.psz).raw());
                break;
            }

            default:
                return RTGetOptPrintError(ch, &ValueUnion);
        }
    }

    if (Utf8Cmd.isEmpty())
        return errorSyntax(USAGE_GUESTCONTROL, "No command to execute specified!");

    if (Utf8UserName.isEmpty())
        return errorSyntax(USAGE_GUESTCONTROL, "No user name specified!");

    /* Any output conversion not supported yet! */
    if (eOutputType != OUTPUTTYPE_UNDEFINED)
        return errorSyntax(USAGE_GUESTCONTROL, "Output conversion not implemented yet!");

    /*
     * <missing comment indicating that we're done parsing args and started doing something else>
     */
    HRESULT rc = S_OK;
    if (fVerbose)
    {
        if (cMsTimeout == 0)
            RTPrintf("Waiting for guest to start process ...\n");
        else
            RTPrintf("Waiting for guest to start process (within %ums)\n", cMsTimeout);
    }

    /* Get current time stamp to later calculate rest of timeout left. */
    uint64_t u64StartMS = RTTimeMilliTS();

    /* Execute the process. */
    int rcProc = RTEXITCODE_FAILURE;
    ComPtr<IProgress> progress;
    ULONG uPID = 0;
    rc = guest->ExecuteProcess(Bstr(Utf8Cmd).raw(),
                               fExecFlags,
                               ComSafeArrayAsInParam(args),
                               ComSafeArrayAsInParam(env),
                               Bstr(Utf8UserName).raw(),
                               Bstr(Utf8Password).raw(),
                               cMsTimeout,
                               &uPID,
                               progress.asOutParam());
    if (FAILED(rc))
        return ctrlPrintError(guest, COM_IIDOF(IGuest));

    if (fVerbose)
        RTPrintf("Process '%s' (PID: %u) started\n", Utf8Cmd.c_str(), uPID);
    if (fWaitForExit)
    {
        if (fVerbose)
        {
            if (cMsTimeout) /* Wait with a certain timeout. */
            {
                /* Calculate timeout value left after process has been started.  */
                uint64_t u64Elapsed = RTTimeMilliTS() - u64StartMS;
                /* Is timeout still bigger than current difference? */
                if (cMsTimeout > u64Elapsed)
                    RTPrintf("Waiting for process to exit (%ums left) ...\n", cMsTimeout - u64Elapsed);
                else
                    RTPrintf("No time left to wait for process!\n"); /** @todo a bit misleading ... */
            }
            else /* Wait forever. */
                RTPrintf("Waiting for process to exit ...\n");
        }

        /* Setup signal handling if cancelable. */
        ASSERT(progress);
        bool fCanceledAlready = false;
        BOOL fCancelable;
        HRESULT hrc = progress->COMGETTER(Cancelable)(&fCancelable);
        if (FAILED(hrc))
            fCancelable = FALSE;
        if (fCancelable)
            ctrlSignalHandlerInstall();

        /* Wait for process to exit ... */
        BOOL fCompleted    = FALSE;
        BOOL fCanceled     = FALSE;
        while (SUCCEEDED(progress->COMGETTER(Completed(&fCompleted))))
        {
            SafeArray<BYTE> aOutputData;
            ULONG cbOutputData = 0;

            /*
             * Some data left to output?
             */
            if (fOutputFlags || fWaitForStdOut)
            {
                /** @todo r=bird: The timeout argument is bogus in several
                 * ways:
                 *  1. RT_MAX will evaluate the arguments twice, which may
                 *     result in different values because RTTimeMilliTS()
                 *     returns a higher value the 2nd time. Worst case:
                 *     Imagine when RT_MAX calculates the remaining time
                 *     out (first expansion) there is say 60 ms left.  Then
                 *     we're preempted and rescheduled after, say, 120 ms.
                 *     We call RTTimeMilliTS() again and ends up with a
                 *     value -60 ms, which translate to a UINT32_MAX - 59
                 *     ms timeout.
                 *
                 *  2. When the period expires, we will wait forever since
                 *     both 0 and -1 mean indefinite timeout with this API,
                 *     at least that's one way of reading the main code.
                 *
                 *  3. There is a signed/unsigned ambiguity in the
                 *     RT_MAX expression.  The left hand side is signed
                 *     integer (0), the right side is unsigned 64-bit. From
                 *     what I can tell, the compiler will treat this as
                 *     unsigned 64-bit and never return 0.
                 */
                rc = guest->GetProcessOutput(uPID, fOutputFlags,
                                             RT_MAX(0, cMsTimeout - (RTTimeMilliTS() - u64StartMS)) /* Timeout in ms */,
                                             _64K, ComSafeArrayAsOutParam(aOutputData));
                if (FAILED(rc))
                {
                    vrc = ctrlPrintError(guest, COM_IIDOF(IGuest));
                    cbOutputData = 0;
                }
                else
                {
                    cbOutputData = aOutputData.size();
                    if (cbOutputData > 0)
                    {
                        /** @todo r=bird: Use a VFS I/O stream filter for doing this, it's a
                        *        generic problem and the new VFS APIs will handle it more
                        *        transparently. (requires writing dos2unix/unix2dos filters ofc) */

                        /*
                         * If aOutputData is text data from the guest process' stdout or stderr,
                         * it has a platform dependent line ending. So standardize on
                         * Unix style, as RTStrmWrite does the LF -> CR/LF replacement on
                         * Windows. Otherwise we end up with CR/CR/LF on Windows.
                         */
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

            /* No more output data left? */
            if (cbOutputData <= 0)
            {
                /* Only break out from process handling loop if we processed (displayed)
                 * all output data or if there simply never was output data and the process
                 * has been marked as complete. */
                if (fCompleted)
                    break;
            }

            /* Process async cancelation */
            if (g_fGuestCtrlCanceled && !fCanceledAlready)
            {
                hrc = progress->Cancel();
                if (SUCCEEDED(hrc))
                    fCanceledAlready = TRUE;
                else
                    g_fGuestCtrlCanceled = false;
            }

            /* Progress canceled by Main API? */
            if (   SUCCEEDED(progress->COMGETTER(Canceled(&fCanceled)))
                && fCanceled)
                break;

            /* Did we run out of time? */
            if (   cMsTimeout
                && RTTimeMilliTS() - u64StartMS > cMsTimeout)
            {
                progress->Cancel();
                break;
            }
        } /* while */

        /* Undo signal handling */
        if (fCancelable)
            ctrlSignalHandlerUninstall();

        /* Report status back to the user. */
        if (fCanceled)
        {
            if (fVerbose)
                RTPrintf("Process execution canceled!\n");
            rcProc = EXITCODEEXEC_CANCELED;
        }
        else if (   fCompleted
                 && SUCCEEDED(rc)) /* The GetProcessOutput rc. */
        {
            LONG iRc;
            CHECK_ERROR_RET(progress, COMGETTER(ResultCode)(&iRc), rc);
            if (FAILED(iRc))
                vrc = ctrlPrintProgressError(progress);
            else
            {
                ExecuteProcessStatus_T retStatus;
                ULONG uRetExitCode, uRetFlags;
                rc = guest->GetProcessStatus(uPID, &uRetExitCode, &uRetFlags, &retStatus);
                if (SUCCEEDED(rc) && fVerbose)
                    RTPrintf("Exit code=%u (Status=%u [%s], Flags=%u)\n", uRetExitCode, retStatus, ctrlExecProcessStatusToText(retStatus), uRetFlags);
                rcProc = ctrlExecProcessStatusToExitCode(retStatus, uRetExitCode);
            }
        }
        else
        {
            if (fVerbose)
                RTPrintf("Process execution aborted!\n");
            rcProc = EXITCODEEXEC_TERM_ABEND;
        }
    }

    if (RT_FAILURE(vrc) || FAILED(rc))
        return RTEXITCODE_FAILURE;
    return rcProc;
}

static int ctrlCopyContextCreate(IGuest *pGuest, bool fVerbose, bool fHostToGuest,
                                 const char *pszUsername, const char *pszPassword,
                                 PCOPYCONTEXT *ppContext)
{
    AssertPtrReturn(pGuest, VERR_INVALID_POINTER);
    AssertPtrReturn(pszUsername, VERR_INVALID_POINTER);
    AssertPtrReturn(pszPassword, VERR_INVALID_POINTER);

    PCOPYCONTEXT pContext = (PCOPYCONTEXT)RTMemAlloc(sizeof(COPYCONTEXT));
    AssertPtrReturn(pContext, VERR_NO_MEMORY);
    pContext->pGuest = pGuest;
    pContext->fVerbose = fVerbose;
    pContext->fHostToGuest = fHostToGuest;

    pContext->pszUsername = RTStrDup(pszUsername);
    if (!pContext->pszUsername)
    {
        RTMemFree(pContext);
        return VERR_NO_MEMORY;
    }

    pContext->pszPassword = RTStrDup(pszPassword);
    if (!pContext->pszPassword)
    {
        RTStrFree(pContext->pszUsername);
        RTMemFree(pContext);
        return VERR_NO_MEMORY;
    }

    *ppContext = pContext;

    return VINF_SUCCESS;
}

static void ctrlCopyContextFree(PCOPYCONTEXT pContext)
{
    if (pContext)
    {
        RTStrFree(pContext->pszUsername);
        RTStrFree(pContext->pszPassword);
        RTMemFree(pContext);
    }
}

/*
 * Source          Dest                 Translated
 * c:\foo.txt      c:\from_host         c:\from_host\foo.txt
 * c:\asdf\foo     c:\qwer              c:\qwer\foo
 * c:\bar\baz.txt  d:\users\            d:\users\baz.txt
 * c:\*.dll        e:\baz               e:\baz
 */
static int ctrlCopyTranslatePath(PCOPYCONTEXT pContext,
                                 const char *pszSourceRoot, const char *pszSource,
                                 const char *pszDest, char **ppszTranslated)
{
    AssertPtrReturn(pContext, VERR_INVALID_POINTER);
    AssertPtrReturn(pszSourceRoot, VERR_INVALID_POINTER);
    AssertPtrReturn(pszSource, VERR_INVALID_POINTER);
    AssertPtrReturn(pszDest, VERR_INVALID_POINTER);
    AssertPtrReturn(ppszTranslated, VERR_INVALID_POINTER);

    /* Source path must contain the source root! */
    if (!RTPathStartsWith(pszSource, pszSourceRoot))
        return VERR_INVALID_PARAMETER;

    /* Construct the relative dest destination path by "subtracting" the
     * source from the source root, e.g.
     *
     * source root path = "e:\foo\", source = "e:\foo\bar"
     * dest = "d:\baz\"
     * translated = "d:\baz\bar\"
     */

    size_t lenRoot = strlen(pszSourceRoot);
    AssertReturn(lenRoot, VERR_INVALID_PARAMETER);
    char *pszTranslated = RTStrDup(pszDest);
    AssertReturn(pszTranslated, VERR_NO_MEMORY);
    int vrc = RTStrAAppend(&pszTranslated, &pszSource[lenRoot]);
    if (RT_FAILURE(vrc))
        return vrc;

    *ppszTranslated = pszTranslated;

#ifdef DEBUG
    if (pContext->fVerbose)
        RTPrintf("Translating root=%s, source=%s, dest=%s -> %s\n",
                 pszSourceRoot, pszSource, pszDest, *ppszTranslated);
#endif

    return vrc;
}

static int ctrlCopyDirCreate(PCOPYCONTEXT pContext, const char *pszDir)
{
    AssertPtrReturn(pContext, VERR_INVALID_POINTER);
    AssertPtrReturn(pszDir, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;
    if (pContext->fHostToGuest) /* We want to create directories on the guest. */
    {
        HRESULT hrc = pContext->pGuest->DirectoryCreate(Bstr(pszDir).raw(),
                                                        Bstr(pContext->pszUsername).raw(), Bstr(pContext->pszPassword).raw(),
                                                        700, DirectoryCreateFlag_Parents);
        if (FAILED(hrc))
            rc = ctrlPrintError(pContext->pGuest, COM_IIDOF(IGuest));
    }
    else /* ... or on the host. */
    {
        rc = RTDirCreate(pszDir, 700);
        if (rc == VERR_ALREADY_EXISTS)
            rc = VINF_SUCCESS;
    }
    return rc;
}

static int ctrlCopyDirExists(PCOPYCONTEXT pContext, bool bGuest,
                             const char *pszDir, bool *fExists)
{
    AssertPtrReturn(pContext, false);
    AssertPtrReturn(pszDir, false);
    AssertPtrReturn(fExists, false);

    int rc = VINF_SUCCESS;
    if (bGuest)
    {
        BOOL fDirExists = FALSE;
        /** @todo Replace with DirectoryExists as soon as API is in place. */
        HRESULT hr = pContext->pGuest->FileExists(Bstr(pszDir).raw(),
                                                  Bstr(pContext->pszUsername).raw(),
                                                  Bstr(pContext->pszPassword).raw(), &fDirExists);
        if (FAILED(hr))
            rc = ctrlPrintError(pContext->pGuest, COM_IIDOF(IGuest));
        else
            *fExists = fDirExists ? true : false;
    }
    else
        *fExists = RTDirExists(pszDir);
    return rc;
}

static int ctrlCopyDirExistsOnDest(PCOPYCONTEXT pContext, const char *pszDir,
                                   bool *fExists)
{
    return ctrlCopyDirExists(pContext, pContext->fHostToGuest,
                             pszDir, fExists);
}

static int ctrlCopyDirExistsOnSource(PCOPYCONTEXT pContext, const char *pszDir,
                                     bool *fExists)
{
    return ctrlCopyDirExists(pContext, !pContext->fHostToGuest,
                             pszDir, fExists);
}

static int ctrlCopyFileExists(PCOPYCONTEXT pContext, bool bOnGuest,
                              const char *pszFile, bool *fExists)
{
    AssertPtrReturn(pContext, false);
    AssertPtrReturn(pszFile, false);
    AssertPtrReturn(fExists, false);

    int rc = VINF_SUCCESS;
    if (bOnGuest)
    {
        BOOL fFileExists = FALSE;
        HRESULT hr = pContext->pGuest->FileExists(Bstr(pszFile).raw(),
                                                  Bstr(pContext->pszUsername).raw(),
                                                  Bstr(pContext->pszPassword).raw(), &fFileExists);
        if (FAILED(hr))
            rc = ctrlPrintError(pContext->pGuest, COM_IIDOF(IGuest));
        else
            *fExists = fFileExists ? true : false;
    }
    else
        *fExists = RTFileExists(pszFile);
    return rc;
}

static int ctrlCopyFileExistsOnDest(PCOPYCONTEXT pContext, const char *pszFile,
                                    bool *fExists)
{
    return ctrlCopyFileExists(pContext, pContext->fHostToGuest,
                              pszFile, fExists);
}

static int ctrlCopyFileExistsOnSource(PCOPYCONTEXT pContext, const char *pszFile,
                                      bool *fExists)
{
    return ctrlCopyFileExists(pContext, !pContext->fHostToGuest,
                              pszFile, fExists);
}

static int ctrlCopyFileToTarget(PCOPYCONTEXT pContext, const char *pszFileSource,
                                const char *pszFileDest, uint32_t fFlags)
{
    AssertPtrReturn(pContext, VERR_INVALID_POINTER);
    AssertPtrReturn(pszFileSource, VERR_INVALID_POINTER);
    AssertPtrReturn(pszFileDest, VERR_INVALID_POINTER);

    if (pContext->fVerbose)
    {
        RTPrintf("Copying \"%s\" to \"%s\" ...\n",
                 pszFileSource, pszFileDest);
    }

    int vrc = VINF_SUCCESS;
    ComPtr<IProgress> progress;
    HRESULT hr;
    if (pContext->fHostToGuest)
    {
        hr = pContext->pGuest->CopyToGuest(Bstr(pszFileSource).raw(), Bstr(pszFileDest).raw(),
                                           Bstr(pContext->pszUsername).raw(), Bstr(pContext->pszPassword).raw(),
                                           fFlags, progress.asOutParam());
    }
    else
    {
        hr = pContext->pGuest->CopyFromGuest(Bstr(pszFileSource).raw(), Bstr(pszFileDest).raw(),
                                             Bstr(pContext->pszUsername).raw(), Bstr(pContext->pszPassword).raw(),
                                             fFlags, progress.asOutParam());
    }

    if (FAILED(hr))
        vrc = ctrlPrintError(pContext->pGuest, COM_IIDOF(IGuest));
    else
    {
        hr = showProgress(progress);
        if (FAILED(hr))
            vrc = ctrlPrintProgressError(progress);
    }

    return vrc;
}

static int ctrlCopyDirToGuest(PCOPYCONTEXT pContext,
                              const char *pszSource, const char *pszFilter,
                              const char *pszDest, uint32_t fFlags,
                              const char *pszSubDir /* For recursion */)
{
    AssertPtrReturn(pContext, VERR_INVALID_POINTER);
    AssertPtrReturn(pszSource, VERR_INVALID_POINTER);
    /* Filter is optional. */
    AssertPtrReturn(pszDest, VERR_INVALID_POINTER);
    /* Sub directory is optional. */

    /*
     * Construct current path.
     */
    char szCurDir[RTPATH_MAX];
    int rc = RTStrCopy(szCurDir, sizeof(szCurDir), pszSource);
    if (RT_SUCCESS(rc) && pszSubDir)
        rc = RTPathAppend(szCurDir, sizeof(szCurDir), pszSubDir);

    /* Flag indicating whether the current directory was created on the
     * target or not. */
    bool fDirCreated = false;

    /*
     * Open directory without a filter - RTDirOpenFiltered unfortunately
     * cannot handle sub directories so we have to do the filtering ourselves.
     */
    PRTDIR pDir = NULL;
    if (RT_SUCCESS(rc))
    {
        rc = RTDirOpen(&pDir, szCurDir);
        if (RT_FAILURE(rc))
            pDir = NULL;
    }
    if (RT_SUCCESS(rc))
    {
        /*
         * Enumerate the directory tree.
         */
        while (RT_SUCCESS(rc))
        {
            RTDIRENTRY DirEntry;
            rc = RTDirRead(pDir, &DirEntry, NULL);
            if (RT_FAILURE(rc))
            {
                if (rc == VERR_NO_MORE_FILES)
                    rc = VINF_SUCCESS;
                break;
            }
            switch (DirEntry.enmType)
            {
                case RTDIRENTRYTYPE_DIRECTORY:
                {
                    /* Skip "." and ".." entries. */
                    if (   !strcmp(DirEntry.szName, ".")
                        || !strcmp(DirEntry.szName, ".."))
                        break;

                    if (fFlags & CopyFileFlag_Recursive)
                    {
                        char *pszNewSub = NULL;
                        if (pszSubDir)
                            RTStrAPrintf(&pszNewSub, "%s/%s", pszSubDir, DirEntry.szName);
                        else
                            RTStrAPrintf(&pszNewSub, "%s", DirEntry.szName);

                        if (pszNewSub)
                        {
                            rc = ctrlCopyDirToGuest(pContext,
                                                    pszSource, pszFilter,
                                                    pszDest, fFlags, pszNewSub);
                            RTStrFree(pszNewSub);
                        }
                        else
                            rc = VERR_NO_MEMORY;
                    }
                    break;
                }

                case RTDIRENTRYTYPE_SYMLINK:
                    if (   (fFlags & CopyFileFlag_Recursive)
                        && (fFlags & CopyFileFlag_FollowLinks))
                    {
                        /* Fall through to next case is intentional. */
                    }
                    else
                        break;

                case RTDIRENTRYTYPE_FILE:
                {
                    if (   !pszFilter
                        || RTStrSimplePatternMatch(pszFilter, DirEntry.szName))
                    {
                        if (!fDirCreated)
                        {
                            char *pszDestDir;
                            rc = ctrlCopyTranslatePath(pContext, pszSource, szCurDir,
                                                       pszDest, &pszDestDir);
                            if (RT_SUCCESS(rc))
                            {
                                rc = ctrlCopyDirCreate(pContext, pszDestDir);
                                RTStrFree(pszDestDir);

                                fDirCreated = true;
                            }
                        }

                        if (RT_SUCCESS(rc))
                        {
                            char *pszFileSource;
                            if (RTStrAPrintf(&pszFileSource, "%s/%s",
                                             szCurDir, DirEntry.szName))
                            {
                                char *pszFileDest;
                                rc = ctrlCopyTranslatePath(pContext, pszSource, pszFileSource,
                                                           pszDest, &pszFileDest);
                                if (RT_SUCCESS(rc))
                                {
                                    rc = ctrlCopyFileToTarget(pContext, pszFileSource,
                                                              pszFileDest, 0 /* Flags? */);
                                    RTStrFree(pszFileDest);
                                }
                                RTStrFree(pszFileSource);
                            }
                        }
                    }
                    break;
                }

                default:
                    break;
            }
            if (RT_FAILURE(rc))
                break;
        }

        RTDirClose(pDir);
    }
    return rc;
}

static int ctrlCopyDirToHost(PCOPYCONTEXT pContext,
                             const char *pszSource, const char *pszFilter,
                             const char *pszDest, uint32_t fFlags,
                             const char *pszSubDir /* For recursion */)
{
    AssertPtrReturn(pContext, VERR_INVALID_POINTER);
    AssertPtrReturn(pszSource, VERR_INVALID_POINTER);
    /* Filter is optional. */
    AssertPtrReturn(pszDest, VERR_INVALID_POINTER);
    /* Sub directory is optional. */

    /*
     * Construct current path.
     */
    char szCurDir[RTPATH_MAX];
    int rc = RTStrCopy(szCurDir, sizeof(szCurDir), pszSource);
    if (RT_SUCCESS(rc) && pszSubDir)
        rc = RTPathAppend(szCurDir, sizeof(szCurDir), pszSubDir);

    if (RT_FAILURE(rc))
        return rc;

    /* Flag indicating whether the current directory was created on the
     * target or not. */
    bool fDirCreated = false;

    ULONG uDirHandle;
    HRESULT hr = pContext->pGuest->DirectoryOpen(Bstr(szCurDir).raw(), Bstr(pszFilter).raw(),
                                                 fFlags,
                                                 Bstr(pContext->pszUsername).raw(), Bstr(pContext->pszPassword).raw(),
                                                 &uDirHandle);
    if (FAILED(hr))
        rc = ctrlPrintError(pContext->pGuest, COM_IIDOF(IGuest));
    else
    {
        ComPtr <IGuestDirEntry> dirEntry;
        while (SUCCEEDED(hr = pContext->pGuest->DirectoryRead(uDirHandle, dirEntry.asOutParam())))
        {
            GuestDirEntryType_T enmType;
            dirEntry->COMGETTER(Type)(&enmType);

            Bstr strName;
            dirEntry->COMGETTER(Name)(strName.asOutParam());

            switch (enmType)
            {
                case GuestDirEntryType_Directory:
                {
                    /* Skip "." and ".." entries. */
                    if (   !strName.compare(Bstr("."))
                        || !strName.compare(Bstr("..")))
                        break;

                    if (fFlags & CopyFileFlag_Recursive)
                    {
                        Utf8Str strDir(strName);
                        char *pszNewSub = NULL;
                        if (pszSubDir)
                            RTStrAPrintf(&pszNewSub, "%s/%s", pszSubDir, strDir.c_str());
                        else
                            RTStrAPrintf(&pszNewSub, "%s", strDir.c_str());

                        if (pszNewSub)
                        {
                            rc = ctrlCopyDirToHost(pContext,
                                                   pszSource, pszFilter,
                                                   pszDest, fFlags, pszNewSub);
                            RTStrFree(pszNewSub);
                        }
                        else
                            rc = VERR_NO_MEMORY;
                    }
                    break;
                }

                case GuestDirEntryType_Symlink:
                    if (   (fFlags & CopyFileFlag_Recursive)
                        && (fFlags & CopyFileFlag_FollowLinks))
                    {
                        /* Fall through to next case is intentional. */
                    }
                    else
                        break;

                case GuestDirEntryType_File:
                {
                    Utf8Str strFile(strName);
                    if (   !pszFilter
                        || RTStrSimplePatternMatch(pszFilter, strFile.c_str()))
                    {
                        if (!fDirCreated)
                        {
                            char *pszDestDir;
                            rc = ctrlCopyTranslatePath(pContext, pszSource, szCurDir,
                                                       pszDest, &pszDestDir);
                            if (RT_SUCCESS(rc))
                            {
                                rc = ctrlCopyDirCreate(pContext, pszDestDir);
                                RTStrFree(pszDestDir);

                                fDirCreated = true;
                            }
                        }

                        if (RT_SUCCESS(rc))
                        {
                            char *pszFileSource;
                            if (RTStrAPrintf(&pszFileSource, "%s/%s",
                                             szCurDir, strFile.c_str()))
                            {
                                char *pszFileDest;
                                rc = ctrlCopyTranslatePath(pContext, pszSource, pszFileSource,
                                                           pszDest, &pszFileDest);
                                if (RT_SUCCESS(rc))
                                {
                                    rc = ctrlCopyFileToTarget(pContext, pszFileSource,
                                                              pszFileDest, 0 /* Flags? */);
                                    RTStrFree(pszFileDest);
                                }
                                RTStrFree(pszFileSource);
                            }
                            else
                                rc = VERR_NO_MEMORY;
                        }
                    }
                    break;
                }

                default:
                    break;
            }

            if (RT_FAILURE(rc))
                break;
        }

        if (FAILED(hr))
        {
            if (hr != E_ABORT)
                rc = ctrlPrintError(pContext->pGuest, COM_IIDOF(IGuest));
        }

        HRESULT hr2 = pContext->pGuest->DirectoryClose(uDirHandle);
        if (FAILED(hr2))
        {
            int rc2 = ctrlPrintError(pContext->pGuest, COM_IIDOF(IGuest));
            if (RT_SUCCESS(rc))
                rc = rc2;
        }
        else if (SUCCEEDED(hr))
            hr = hr2;
    }

    return rc;
}

static int ctrlCopyDirToTarget(PCOPYCONTEXT pContext,
                               const char *pszSource, const char *pszFilter,
                               const char *pszDest, uint32_t fFlags,
                               const char *pszSubDir /* For recursion */)
{
    if (pContext->fHostToGuest)
        return ctrlCopyDirToGuest(pContext, pszSource, pszFilter,
                                  pszDest, fFlags, pszSubDir);
    return ctrlCopyDirToHost(pContext, pszSource, pszFilter,
                             pszDest, fFlags, pszSubDir);
}

static int ctrlCopyCreateSourceRoot(const char *pszSource, char **ppszSourceRoot)
{
    AssertPtrReturn(pszSource, VERR_INVALID_POINTER);
    AssertPtrReturn(ppszSourceRoot, VERR_INVALID_POINTER);

    char *pszNewRoot = RTStrDup(pszSource);
    AssertPtrReturn(pszNewRoot, VERR_NO_MEMORY);

    size_t lenRoot = strlen(pszNewRoot);
    if (   lenRoot
        && pszNewRoot[lenRoot - 1] == '/'
        && pszNewRoot[lenRoot - 1] == '\\'
        && lenRoot > 1
        && pszNewRoot[lenRoot - 2] == '/'
        && pszNewRoot[lenRoot - 2] == '\\')
    {
        *ppszSourceRoot = pszNewRoot;
        if (lenRoot > 1)
            *ppszSourceRoot[lenRoot - 2] = '\0';
        *ppszSourceRoot[lenRoot - 1] = '\0';
    }
    else
    {
        /* If there's anything (like a file name or a filter),
         * strip it! */
        RTPathStripFilename(pszNewRoot);
        *ppszSourceRoot = pszNewRoot;
    }

    return VINF_SUCCESS;
}

static void ctrlCopyFreeSourceRoot(char *pszSourceRoot)
{
    RTStrFree(pszSourceRoot);
}

static int handleCtrlCopyTo(ComPtr<IGuest> guest, HandlerArg *pArg,
                            bool fHostToGuest)
{
    AssertPtrReturn(pArg, VERR_INVALID_PARAMETER);

    /** @todo r=bird: This command isn't very unix friendly in general. mkdir
     * is much better (partly because it is much simpler of course).  The main
     * arguments against this is that (1) all but two options conflicts with
     * what 'man cp' tells me on a GNU/Linux system, (2) wildchar matching is
     * done windows CMD style (though not in a 100% compatible way), and (3)
     * that only one source is allowed - efficiently sabotaging default
     * wildcard expansion by a unix shell.  The best solution here would be
     * two different variant, one windowsy (xcopy) and one unixy (gnu cp). */

    /*
     * IGuest::CopyToGuest is kept as simple as possible to let the developer choose
     * what and how to implement the file enumeration/recursive lookup, like VBoxManage
     * does in here.
     */

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--dryrun",              GETOPTDEF_COPYTO_DRYRUN,         RTGETOPT_REQ_NOTHING },
        { "--follow",              GETOPTDEF_COPYTO_FOLLOW,         RTGETOPT_REQ_NOTHING },
        { "--password",            GETOPTDEF_COPYTO_PASSWORD,       RTGETOPT_REQ_STRING  },
        { "--recursive",           'R',                             RTGETOPT_REQ_NOTHING },
        { "--target-directory",    GETOPTDEF_COPYTO_TARGETDIR,      RTGETOPT_REQ_STRING  },
        { "--username",            GETOPTDEF_COPYTO_USERNAME,       RTGETOPT_REQ_STRING  },
        { "--verbose",             'v',                             RTGETOPT_REQ_NOTHING }
    };

    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, pArg->argc, pArg->argv,
                 s_aOptions, RT_ELEMENTS(s_aOptions), 0, RTGETOPTINIT_FLAGS_OPTS_FIRST);

    Utf8Str Utf8Source;
    Utf8Str Utf8Dest;
    Utf8Str Utf8UserName;
    Utf8Str Utf8Password;
    uint32_t fFlags = CopyFileFlag_None;
    bool fVerbose = false;
    bool fCopyRecursive = false;
    bool fDryRun = false;

    SOURCEVEC vecSources;

    int vrc = VINF_SUCCESS;
    while ((ch = RTGetOpt(&GetState, &ValueUnion)))
    {
        /* For options that require an argument, ValueUnion has received the value. */
        switch (ch)
        {
            case GETOPTDEF_COPYTO_DRYRUN:
                fDryRun = true;
                break;

            case GETOPTDEF_COPYTO_FOLLOW:
                fFlags |= CopyFileFlag_FollowLinks;
                break;

            case GETOPTDEF_COPYTO_PASSWORD:
                Utf8Password = ValueUnion.psz;
                break;

            case 'R': /* Recursive processing */
                fFlags |= CopyFileFlag_Recursive;
                break;

            case GETOPTDEF_COPYTO_TARGETDIR:
                Utf8Dest = ValueUnion.psz;
                break;

            case GETOPTDEF_COPYTO_USERNAME:
                Utf8UserName = ValueUnion.psz;
                break;

            case 'v': /* Verbose */
                fVerbose = true;
                break;

            case VINF_GETOPT_NOT_OPTION:
            {
                /* Last argument and no destination specified with
                 * --target-directory yet? Then use the current argument
                 * as destination. */
                if (   pArg->argc == GetState.iNext
                    && Utf8Dest.isEmpty())
                {
                    Utf8Dest = ValueUnion.psz;
                }
                else
                {
                    /* Save the source directory. */
                    vecSources.push_back(SOURCEFILEENTRY(ValueUnion.psz));
                }
                break;
            }

            default:
                return RTGetOptPrintError(ch, &ValueUnion);
        }
    }

    if (!vecSources.size())
        return errorSyntax(USAGE_GUESTCONTROL,
                           "No source(s) specified!");

    if (Utf8Dest.isEmpty())
        return errorSyntax(USAGE_GUESTCONTROL,
                           "No destination specified!");

    if (Utf8UserName.isEmpty())
        return errorSyntax(USAGE_GUESTCONTROL,
                           "No user name specified!");

    /*
     * Done parsing arguments, do some more preparations.
     */
    if (fVerbose)
    {
        if (fHostToGuest)
            RTPrintf("Copying from host to guest ...\n");
        else
            RTPrintf("Copying from guest to host ...\n");
        if (fDryRun)
            RTPrintf("Dry run - no files copied!\n");
    }

    /* Create the copy context -- it contains all information
     * the routines need to know when handling the actual copying. */
    PCOPYCONTEXT pContext;
    vrc = ctrlCopyContextCreate(guest, fVerbose, fHostToGuest,
                                Utf8UserName.c_str(), Utf8Password.c_str(),
                                &pContext);
    if (RT_FAILURE(vrc))
    {
        RTMsgError("Unable to create copy context, rc=%Rrc\n", vrc);
        return RTEXITCODE_FAILURE;
    }

    /* If the destination is a path, (try to) create it. */
    const char *pszDest = Utf8Dest.c_str();
    AssertPtr(pszDest);
    size_t lenDest = strlen(pszDest);
    if (   lenDest
         ||pszDest[lenDest - 1] == '/'
        || pszDest[lenDest - 1] == '\\')
    {
        vrc = ctrlCopyDirCreate(pContext, pszDest);
    }

    if (RT_SUCCESS(vrc))
    {
        /*
         * Here starts the actual fun!
         * Handle all given sources one by one.
         */
        for (unsigned long s = 0; s < vecSources.size(); s++)
        {
            const char *pszSource = vecSources[s].mSource.c_str();
            const char *pszFilter = vecSources[s].mFilter.c_str();
            if (!strlen(pszFilter))
                pszFilter = NULL; /* If empty filter then there's no filter :-) */

            char *pszSourceRoot;
            vrc = ctrlCopyCreateSourceRoot(pszSource, &pszSourceRoot);
            if (RT_FAILURE(vrc))
            {
                RTMsgError("Unable to create source root, rc=%Rrc\n", vrc);
                break;
            }

            if (fVerbose)
                RTPrintf("Source: %s\n", pszSource);

            /** @todo Files with filter?? */
            bool fIsFile = false;
            bool fExists;
            Utf8Str Utf8CurSource(pszSource);
            if (   Utf8CurSource.endsWith("/")
                || Utf8CurSource.endsWith("\\"))
            {
                if (pszFilter) /* Directory with filter. */
                    vrc = ctrlCopyDirExistsOnSource(pContext, pszSourceRoot, &fExists);
                else /* Regular directory without filter. */
                    vrc = ctrlCopyDirExistsOnSource(pContext, pszSource, &fExists);
            }
            else
            {
                vrc = ctrlCopyFileExistsOnSource(pContext, pszSource, &fExists);
                if (   RT_SUCCESS(vrc)
                    && fExists)
                    fIsFile = true;
            }

            if (RT_SUCCESS(vrc))
            {
                if (fIsFile)
                {
                    /* Single file. */
                    char *pszDest;
                    vrc = ctrlCopyTranslatePath(pContext, pszSourceRoot, pszSource,
                                                Utf8Dest.c_str(), &pszDest);
                    if (RT_SUCCESS(vrc))
                    {
                        vrc = ctrlCopyFileToTarget(pContext, pszSource,
                                                   pszDest, fFlags);
                        RTStrFree(pszDest);
                    }
                    else
                    {
                        RTMsgError("Unable to translate path for \"%s\", rc=%Rrc\n",
                                   pszSource, vrc);
                    }
                }
                else
                {
                    /* Directory (with filter?). */
                    vrc = ctrlCopyDirToTarget(pContext, pszSource, pszFilter,
                                              Utf8Dest.c_str(), fFlags, NULL /* Subdir */);
                }
            }

            ctrlCopyFreeSourceRoot(pszSourceRoot);

            if (   RT_SUCCESS(vrc)
                && !fExists)
            {
                RTMsgError("Warning: Source \"%s\" does not exist, skipping!\n",
                           pszSource);
                continue;
            }

            if (RT_FAILURE(vrc))
            {
                RTMsgError("Error processing \"%s\", rc=%Rrc\n",
                           pszSource, vrc);
                break;
            }
        }
    }

    ctrlCopyContextFree(pContext);

    return RT_SUCCESS(vrc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

static int handleCtrlCreateDirectory(ComPtr<IGuest> guest, HandlerArg *pArg)
{
    AssertPtrReturn(pArg, VERR_INVALID_PARAMETER);

    /*
     * Parse arguments.
     *
     * Note! No direct returns here, everyone must go thru the cleanup at the
     *       end of this function.
     */
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--mode",                'm',                             RTGETOPT_REQ_UINT32  },
        { "--parents",             'P',                             RTGETOPT_REQ_NOTHING },
        { "--password",            GETOPTDEF_MKDIR_PASSWORD,        RTGETOPT_REQ_STRING  },
        { "--username",            GETOPTDEF_MKDIR_USERNAME,        RTGETOPT_REQ_STRING  },
        { "--verbose",             'v',                             RTGETOPT_REQ_NOTHING }
    };

    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, pArg->argc, pArg->argv,
                 s_aOptions, RT_ELEMENTS(s_aOptions), 0, RTGETOPTINIT_FLAGS_OPTS_FIRST);

    Utf8Str Utf8UserName;
    Utf8Str Utf8Password;
    uint32_t fFlags = DirectoryCreateFlag_None;
    uint32_t fDirMode = 0; /* Default mode. */
    bool fVerbose = false;

    DESTDIRMAP mapDirs;

    RTEXITCODE rcExit = RTEXITCODE_SUCCESS;
    while (   (ch = RTGetOpt(&GetState, &ValueUnion))
           && rcExit == RTEXITCODE_SUCCESS)
    {
        /* For options that require an argument, ValueUnion has received the value. */
        switch (ch)
        {
            case 'm': /* Mode */
                fDirMode = ValueUnion.u32;
                break;

            case 'P': /* Create parents */
                fFlags |= DirectoryCreateFlag_Parents;
                break;

            case GETOPTDEF_MKDIR_PASSWORD: /* Password */
                Utf8Password = ValueUnion.psz;
                break;

            case GETOPTDEF_MKDIR_USERNAME: /* User name */
                Utf8UserName = ValueUnion.psz;
                break;

            case 'v': /* Verbose */
                fVerbose = true;
                break;

            case VINF_GETOPT_NOT_OPTION:
            {
                mapDirs[ValueUnion.psz]; /* Add destination directory to map. */
                break;
            }

            default:
                rcExit = RTGetOptPrintError(ch, &ValueUnion);
                break;
        }
    }

    uint32_t cDirs = mapDirs.size();
    if (rcExit == RTEXITCODE_SUCCESS && !cDirs)
        rcExit = errorSyntax(USAGE_GUESTCONTROL, "No directory to create specified!");

    if (rcExit == RTEXITCODE_SUCCESS && Utf8UserName.isEmpty())
        rcExit = errorSyntax(USAGE_GUESTCONTROL, "No user name specified!");

    if (rcExit == RTEXITCODE_SUCCESS)
    {
        /*
         * Create the directories.
         */
        HRESULT hrc = S_OK;
        if (fVerbose && cDirs)
            RTPrintf("Creating %u directories ...\n", cDirs);

        DESTDIRMAPITER it = mapDirs.begin();
        while (it != mapDirs.end())
        {
            if (fVerbose)
                RTPrintf("Creating directory \"%s\" ...\n", it->first.c_str());

            hrc = guest->DirectoryCreate(Bstr(it->first).raw(),
                                         Bstr(Utf8UserName).raw(), Bstr(Utf8Password).raw(),
                                         fDirMode, fFlags);
            if (FAILED(hrc))
            {
                ctrlPrintError(guest, COM_IIDOF(IGuest)); /* Return code ignored, save original rc. */
                break;
            }

            it++;
        }

        if (FAILED(hrc))
            rcExit = RTEXITCODE_FAILURE;
    }

    return rcExit;
}

static int handleCtrlStat(ComPtr<IGuest> guest, HandlerArg *pArg)
{
    AssertPtrReturn(pArg, VERR_INVALID_PARAMETER);

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--dereference",         'L',                             RTGETOPT_REQ_NOTHING },
        { "--file-system",         'f',                             RTGETOPT_REQ_NOTHING },
        { "--format",              'c',                             RTGETOPT_REQ_STRING },
        { "--password",            GETOPTDEF_STAT_PASSWORD,         RTGETOPT_REQ_STRING  },
        { "--terse",               't',                             RTGETOPT_REQ_NOTHING },
        { "--username",            GETOPTDEF_STAT_USERNAME,         RTGETOPT_REQ_STRING  },
        { "--verbose",             'v',                             RTGETOPT_REQ_NOTHING }
    };

    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, pArg->argc, pArg->argv,
                 s_aOptions, RT_ELEMENTS(s_aOptions), 0, RTGETOPTINIT_FLAGS_OPTS_FIRST);

    Utf8Str Utf8UserName;
    Utf8Str Utf8Password;

    bool fVerbose = false;
    DESTDIRMAP mapObjs;

    RTEXITCODE rcExit = RTEXITCODE_SUCCESS;
    while (   (ch = RTGetOpt(&GetState, &ValueUnion))
           && rcExit == RTEXITCODE_SUCCESS)
    {
        /* For options that require an argument, ValueUnion has received the value. */
        switch (ch)
        {
            case GETOPTDEF_STAT_PASSWORD: /* Password */
                Utf8Password = ValueUnion.psz;
                break;

            case GETOPTDEF_STAT_USERNAME: /* User name */
                Utf8UserName = ValueUnion.psz;
                break;

            case 'L': /* Dereference */
            case 'f': /* File-system */
            case 'c': /* Format */
            case 't': /* Terse */
                return errorSyntax(USAGE_GUESTCONTROL, "Command \"%s\" not implemented yet!",
                                   ValueUnion.psz);
                break; /* Never reached. */

            case 'v': /* Verbose */
                fVerbose = true;
                break;

            case VINF_GETOPT_NOT_OPTION:
            {
                mapObjs[ValueUnion.psz]; /* Add element to check to map. */
                break;
            }

            default:
                return RTGetOptPrintError(ch, &ValueUnion);
                break; /* Never reached. */
        }
    }

    uint32_t cObjs = mapObjs.size();
    if (rcExit == RTEXITCODE_SUCCESS && !cObjs)
        rcExit = errorSyntax(USAGE_GUESTCONTROL, "No element(s) to check specified!");

    if (rcExit == RTEXITCODE_SUCCESS && Utf8UserName.isEmpty())
        rcExit = errorSyntax(USAGE_GUESTCONTROL, "No user name specified!");

    if (rcExit == RTEXITCODE_SUCCESS)
    {
        /*
         * Create the directories.
         */
        HRESULT hrc = S_OK;

        DESTDIRMAPITER it = mapObjs.begin();
        while (it != mapObjs.end())
        {
            if (fVerbose)
                RTPrintf("Checking for element \"%s\" ...\n", it->first.c_str());

            BOOL fExists;
            hrc = guest->FileExists(Bstr(it->first).raw(),
                                    Bstr(Utf8UserName).raw(), Bstr(Utf8Password).raw(),
                                    &fExists);
            if (FAILED(hrc))
            {
                ctrlPrintError(guest, COM_IIDOF(IGuest)); /* Return code ignored, save original rc. */
                break;
            }
            else
            {
                /** @todo: Output vbox_stat's stdout output to get more information about
                 *         what happened. */

                /* If there's at least one element which does not exist on the guest,
                 * drop out with exitcode 1. */
                if (!fExists)
                {
                    RTPrintf("Cannot stat for element \"%s\": No such file or directory.\n",
                             it->first.c_str());
                    rcExit = RTEXITCODE_FAILURE;
                }
            }

            it++;
        }

        if (FAILED(hrc))
            rcExit = RTEXITCODE_FAILURE;
    }

    return rcExit;
}

static int handleCtrlUpdateAdditions(ComPtr<IGuest> guest, HandlerArg *pArg)
{
    AssertPtrReturn(pArg, VERR_INVALID_PARAMETER);

    /*
     * Check the syntax.  We can deduce the correct syntax from the number of
     * arguments.
     */
    Utf8Str Utf8Source;
    bool fVerbose = false;

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--source",              's',         RTGETOPT_REQ_STRING  },
        { "--verbose",             'v',         RTGETOPT_REQ_NOTHING }
    };

    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, pArg->argc, pArg->argv, s_aOptions, RT_ELEMENTS(s_aOptions), 0, 0);

    int vrc = VINF_SUCCESS;
    while (   (ch = RTGetOpt(&GetState, &ValueUnion))
           && RT_SUCCESS(vrc))
    {
        switch (ch)
        {
            case 's':
                Utf8Source = ValueUnion.psz;
                break;

            case 'v':
                fVerbose = true;
                break;

            default:
                return RTGetOptPrintError(ch, &ValueUnion);
        }
    }

    if (fVerbose)
        RTPrintf("Updating Guest Additions ...\n");

#ifdef DEBUG_andy
    if (Utf8Source.isEmpty())
        Utf8Source = "c:\\Downloads\\VBoxGuestAdditions-r67158.iso";
#endif

    /* Determine source if not set yet. */
    if (Utf8Source.isEmpty())
    {
        char strTemp[RTPATH_MAX];
        vrc = RTPathAppPrivateNoArch(strTemp, sizeof(strTemp));
        AssertRC(vrc);
        Utf8Str Utf8Src1 = Utf8Str(strTemp).append("/VBoxGuestAdditions.iso");

        vrc = RTPathExecDir(strTemp, sizeof(strTemp));
        AssertRC(vrc);
        Utf8Str Utf8Src2 = Utf8Str(strTemp).append("/additions/VBoxGuestAdditions.iso");

        /* Check the standard image locations */
        if (RTFileExists(Utf8Src1.c_str()))
            Utf8Source = Utf8Src1;
        else if (RTFileExists(Utf8Src2.c_str()))
            Utf8Source = Utf8Src2;
        else
        {
            RTMsgError("Source could not be determined! Please use --source to specify a valid source.\n");
            vrc = VERR_FILE_NOT_FOUND;
        }
    }
    else if (!RTFileExists(Utf8Source.c_str()))
    {
        RTMsgError("Source \"%s\" does not exist!\n", Utf8Source.c_str());
        vrc = VERR_FILE_NOT_FOUND;
    }

    if (RT_SUCCESS(vrc))
    {
        if (fVerbose)
            RTPrintf("Using source: %s\n", Utf8Source.c_str());

        HRESULT rc = S_OK;
        ComPtr<IProgress> progress;
        CHECK_ERROR(guest, UpdateGuestAdditions(Bstr(Utf8Source).raw(),
                                                /* Wait for whole update process to complete. */
                                                AdditionsUpdateFlag_None,
                                                progress.asOutParam()));
        if (FAILED(rc))
            vrc = ctrlPrintError(guest, COM_IIDOF(IGuest));
        else
        {
            rc = showProgress(progress);
            if (FAILED(rc))
                vrc = ctrlPrintProgressError(progress);
            else if (fVerbose)
                RTPrintf("Guest Additions update successful.\n");
        }
    }

    return RT_SUCCESS(vrc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

/**
 * Access the guest control store.
 *
 * @returns program exit code.
 * @note see the command line API description for parameters
 */
int handleGuestControl(HandlerArg *pArg)
{
    AssertPtrReturn(pArg, VERR_INVALID_PARAMETER);

    HandlerArg arg = *pArg;
    arg.argc = pArg->argc - 2; /* Skip VM name and sub command. */
    arg.argv = pArg->argv + 2; /* Same here. */

    ComPtr<IGuest> guest;
    int vrc = ctrlInitVM(pArg, pArg->argv[0] /* VM Name */, &guest);
    if (RT_SUCCESS(vrc))
    {
        int rcExit;
        if (   !strcmp(pArg->argv[1], "exec")
            || !strcmp(pArg->argv[1], "execute"))
        {
            rcExit = handleCtrlExecProgram(guest, &arg);
        }
        else if (!strcmp(pArg->argv[1], "copyfrom"))
        {
            rcExit = handleCtrlCopyTo(guest, &arg,
                                      false /* Guest to host */);
        }
        else if (   !strcmp(pArg->argv[1], "copyto")
                 || !strcmp(pArg->argv[1], "cp"))
        {
            rcExit = handleCtrlCopyTo(guest, &arg,
                                      true /* Host to guest */);
        }
        else if (   !strcmp(pArg->argv[1], "createdirectory")
                 || !strcmp(pArg->argv[1], "createdir")
                 || !strcmp(pArg->argv[1], "mkdir")
                 || !strcmp(pArg->argv[1], "md"))
        {
            rcExit = handleCtrlCreateDirectory(guest, &arg);
        }
        else if (   !strcmp(pArg->argv[1], "stat"))
        {
            rcExit = handleCtrlStat(guest, &arg);
        }
        else if (   !strcmp(pArg->argv[1], "updateadditions")
                 || !strcmp(pArg->argv[1], "updateadds"))
        {
            rcExit = handleCtrlUpdateAdditions(guest, &arg);
        }
        else
            rcExit = errorSyntax(USAGE_GUESTCONTROL, "No sub command specified!");

        ctrlUninitVM(pArg);
        return rcExit;
    }
    return RTEXITCODE_FAILURE;
}

#endif /* !VBOX_ONLY_DOCS */

