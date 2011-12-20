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
    bool fDryRun;
    bool fHostToGuest;
    char *pszUsername;
    char *pszPassword;
} COPYCONTEXT, *PCOPYCONTEXT;

/**
 * An entry for a source element, including an optional DOS-like wildcard (*,?).
 */
class SOURCEFILEENTRY
{
    public:

        SOURCEFILEENTRY(const char *pszSource, const char *pszFilter)
                        : mSource(pszSource),
                          mFilter(pszFilter) {}

        SOURCEFILEENTRY(const char *pszSource)
                        : mSource(pszSource)
        {
            Parse(pszSource);
        }

        const char* GetSource() const
        {
            return mSource.c_str();
        }

        const char* GetFilter() const
        {
            return mFilter.c_str();
        }

    private:

        int Parse(const char *pszPath)
        {
            AssertPtrReturn(pszPath, VERR_INVALID_POINTER);

            if (   !RTFileExists(pszPath)
                && !RTDirExists(pszPath))
            {
                /* No file and no directory -- maybe a filter? */
                char *pszFilename = RTPathFilename(pszPath);
                if (   pszFilename
                    && strpbrk(pszFilename, "*?"))
                {
                    /* Yep, get the actual filter part. */
                    mFilter = RTPathFilename(pszPath);
                    /* Remove the filter from actual sourcec directory name. */
                    RTPathStripFilename(mSource.mutableRaw());
                    mSource.jolt();
                }
            }

            return VINF_SUCCESS; /* @todo */
        }

    private:

        Utf8Str mSource;
        Utf8Str mFilter;
};
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

static int ctrlCopyDirExists(PCOPYCONTEXT pContext, bool bGuest, const char *pszDir, bool *fExists);

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

/**
 * Prints the desired guest output to a stream.
 *
 * @return  IPRT status code.
 * @param   pGuest          Pointer to IGuest interface.
 * @param   uPID            PID of guest process to get the output from.
 * @param   fOutputFlags    Output flags of type ProcessOutputFlag.
 * @param   cMsTimeout      Timeout value (in ms) to wait for output.
 */
static int ctrlExecPrintOutput(IGuest *pGuest, ULONG uPID,
                               PRTSTREAM pStrmOutput, uint32_t fOutputFlags,
                               uint32_t cMsTimeout)
{
    AssertPtrReturn(pGuest, VERR_INVALID_POINTER);
    AssertReturn(uPID, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pStrmOutput, VERR_INVALID_POINTER);

    SafeArray<BYTE> aOutputData;
    ULONG cbOutputData = 0;

    int vrc = VINF_SUCCESS;
    HRESULT rc = pGuest->GetProcessOutput(uPID, fOutputFlags,
                                          cMsTimeout,
                                          _64K, ComSafeArrayAsOutParam(aOutputData));
    if (FAILED(rc))
    {
        vrc = ctrlPrintError(pGuest, COM_IIDOF(IGuest));
        cbOutputData = 0;
    }
    else
    {
        cbOutputData = aOutputData.size();
        if (cbOutputData > 0)
        {
            BYTE *pBuf = aOutputData.raw();
            AssertPtr(pBuf);
            pBuf[cbOutputData - 1] = 0; /* Properly terminate buffer. */

            /** @todo r=bird: Use a VFS I/O stream filter for doing this, it's a
            *        generic problem and the new VFS APIs will handle it more
            *        transparently. (requires writing dos2unix/unix2dos filters ofc) */

            /*
             * If aOutputData is text data from the guest process' stdout or stderr,
             * it has a platform dependent line ending. So standardize on
             * Unix style, as RTStrmWrite does the LF -> CR/LF replacement on
             * Windows. Otherwise we end up with CR/CR/LF on Windows.
             */

            char *pszBufUTF8;
            vrc = RTStrCurrentCPToUtf8(&pszBufUTF8, (const char*)aOutputData.raw());
            if (RT_SUCCESS(vrc))
            {
                cbOutputData = strlen(pszBufUTF8);

                ULONG cbOutputDataPrint = cbOutputData;
                for (char *s = pszBufUTF8, *d = s;
                     s - pszBufUTF8 < (ssize_t)cbOutputData;
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

                vrc = RTStrmWrite(pStrmOutput, pszBufUTF8, cbOutputDataPrint);
                if (RT_FAILURE(vrc))
                    RTMsgError("Unable to write output, rc=%Rrc\n", vrc);

                RTStrFree(pszBufUTF8);
            }
            else
                RTMsgError("Unable to convert output, rc=%Rrc\n", vrc);
        }
    }

    return vrc;
}

/**
 * Returns the remaining time (in ms) based on the start time and a set
 * timeout value. Returns RT_INDEFINITE_WAIT if no timeout was specified.
 *
 * @return  RTMSINTERVAL    Time left (in ms).
 * @param   u64StartMs      Start time (in ms).
 * @param   u32TimeoutMs    Timeout value (in ms).
 */
inline RTMSINTERVAL ctrlExecGetRemainingTime(uint64_t u64StartMs, uint32_t u32TimeoutMs)
{
    if (!u32TimeoutMs) /* If no timeout specified, wait forever. */
        return RT_INDEFINITE_WAIT;

    uint64_t u64ElapsedMs = RTTimeMilliTS() - u64StartMs;
    if (u64ElapsedMs >= u32TimeoutMs)
        return 0;

    return u32TimeoutMs - u64ElapsedMs;
}

/* <Missing docuemntation> */
static int handleCtrlExecProgram(ComPtr<IGuest> pGuest, HandlerArg *pArg)
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
    uint32_t                fExecFlags      = ExecuteProcessFlag_None;
    com::SafeArray<IN_BSTR> args;
    com::SafeArray<IN_BSTR> env;
    Utf8Str                 Utf8UserName;
    Utf8Str                 Utf8Password;
    uint32_t                cMsTimeout      = 0;
    OUTPUTTYPE              eOutputType     = OUTPUTTYPE_UNDEFINED;
    bool                    fOutputBinary   = false;
    bool                    fWaitForExit    = false;
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
                fExecFlags |= ExecuteProcessFlag_WaitForStdOut;
                fWaitForExit = true;
                break;

            case GETOPTDEF_EXEC_WAITFORSTDERR:
                fExecFlags |= ExecuteProcessFlag_WaitForStdErr;
                fWaitForExit = true;
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
    rc = pGuest->ExecuteProcess(Bstr(Utf8Cmd).raw(),
                               fExecFlags,
                               ComSafeArrayAsInParam(args),
                               ComSafeArrayAsInParam(env),
                               Bstr(Utf8UserName).raw(),
                               Bstr(Utf8Password).raw(),
                               cMsTimeout,
                               &uPID,
                               progress.asOutParam());
    if (FAILED(rc))
        return ctrlPrintError(pGuest, COM_IIDOF(IGuest));

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

        vrc = RTStrmSetMode(g_pStdOut, 1 /* Binary mode */, -1 /* Code set, unchanged */);
        if (RT_FAILURE(vrc))
            RTMsgError("Unable to set stdout's binary mode, rc=%Rrc\n", vrc);

        PRTSTREAM pStream = g_pStdOut; /* StdOut by default. */
        AssertPtr(pStream);

        /* Wait for process to exit ... */
        BOOL fCompleted    = FALSE;
        BOOL fCanceled     = FALSE;
        while (   SUCCEEDED(progress->COMGETTER(Completed(&fCompleted)))
               && !fCompleted)
        {
            /* Do we need to output stuff? */
            uint32_t cMsTimeLeft;
            if (fExecFlags & ExecuteProcessFlag_WaitForStdOut)
            {
                cMsTimeLeft = ctrlExecGetRemainingTime(u64StartMS, cMsTimeout);
                if (cMsTimeLeft)
                    vrc = ctrlExecPrintOutput(pGuest, uPID,
                                              pStream, ProcessOutputFlag_None /* StdOut */,
                                              cMsTimeLeft == RT_INDEFINITE_WAIT ? 0 : cMsTimeLeft);
            }

            if (fExecFlags & ExecuteProcessFlag_WaitForStdErr)
            {
                cMsTimeLeft = ctrlExecGetRemainingTime(u64StartMS, cMsTimeout);
                if (cMsTimeLeft)
                    vrc = ctrlExecPrintOutput(pGuest, uPID,
                                              pStream, ProcessOutputFlag_StdErr /* StdErr */,
                                              cMsTimeLeft == RT_INDEFINITE_WAIT ? 0 : cMsTimeLeft);
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
                rc = pGuest->GetProcessStatus(uPID, &uRetExitCode, &uRetFlags, &retStatus);
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

/**
 * Creates a copy context structure which then can be used with various
 * guest control copy functions. Needs to be free'd with ctrlCopyContextFree().
 *
 * @return  IPRT status code.
 * @param   pGuest                  Pointer to IGuest interface to use.
 * @param   fVerbose                Flag indicating if we want to run in verbose mode.
 * @param   fDryRun                 Flag indicating if we want to run a dry run only.
 * @param   fHostToGuest            Flag indicating if we want to copy from host to guest
 *                                  or vice versa.
 * @param   pszUsername             Username of account to use on the guest side.
 * @param   pszPassword             Password of account to use.
 * @param   ppContext               Pointer which receives the allocated copy context.
 */
static int ctrlCopyContextCreate(IGuest *pGuest, bool fVerbose, bool fDryRun,
                                 bool fHostToGuest,
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
    pContext->fDryRun = fDryRun;
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

/**
 * Frees are previously allocated copy context structure.
 *
 * @param   pContext                Pointer to copy context to free.
 */
static void ctrlCopyContextFree(PCOPYCONTEXT pContext)
{
    if (pContext)
    {
        RTStrFree(pContext->pszUsername);
        RTStrFree(pContext->pszPassword);
        RTMemFree(pContext);
    }
}

/**
 * Translates a source path to a destintation path (can be both sides,
 * either host or guest). The source root is needed to determine the start
 * of the relative source path which also needs to present in the destination
 * path.
 *
 * @return  IPRT status code.
 * @param   pszSourceRoot           Source root path. No trailing directory slash!
 * @param   pszSource               Actual source to transform. Must begin with
 *                                  the source root path!
 * @param   pszDest                 Destination path.
 * @param   ppszTranslated          Pointer to the allocated, translated destination
 *                                  path. Must be free'd with RTStrFree().
 */
static int ctrlCopyTranslatePath(const char *pszSourceRoot, const char *pszSource,
                                 const char *pszDest, char **ppszTranslated)
{
    AssertPtrReturn(pszSourceRoot, VERR_INVALID_POINTER);
    AssertPtrReturn(pszSource, VERR_INVALID_POINTER);
    AssertPtrReturn(pszDest, VERR_INVALID_POINTER);
    AssertPtrReturn(ppszTranslated, VERR_INVALID_POINTER);
    AssertReturn(RTPathStartsWith(pszSource, pszSourceRoot), VERR_INVALID_PARAMETER);

    /* Construct the relative dest destination path by "subtracting" the
     * source from the source root, e.g.
     *
     * source root path = "e:\foo\", source = "e:\foo\bar"
     * dest = "d:\baz\"
     * translated = "d:\baz\bar\"
     */
    char szTranslated[RTPATH_MAX];
    size_t srcOff = strlen(pszSourceRoot);
    AssertReturn(srcOff, VERR_INVALID_PARAMETER);

    char *pszDestPath = RTStrDup(pszDest);
    AssertPtrReturn(pszDestPath, VERR_NO_MEMORY);

    int rc;
    if (!RTPathFilename(pszDestPath))
    {
        rc = RTPathJoin(szTranslated, sizeof(szTranslated),
                        pszDestPath, &pszSource[srcOff]);
    }
    else
    {
        char *pszDestFileName = RTStrDup(RTPathFilename(pszDestPath));
        if (pszDestFileName)
        {
            RTPathStripFilename(pszDestPath);
            rc = RTPathJoin(szTranslated, sizeof(szTranslated),
                            pszDestPath, pszDestFileName);
            RTStrFree(pszDestFileName);
        }
        else
            rc = VERR_NO_MEMORY;
    }
    RTStrFree(pszDestPath);

    if (RT_SUCCESS(rc))
    {
        *ppszTranslated = RTStrDup(szTranslated);
#ifdef DEBUG
        RTPrintf("Root: %s, Source: %s, Dest: %s, Translated: %s\n",
                 pszSourceRoot, pszSource, pszDest, *ppszTranslated);
#endif
    }
    return rc;
}

#ifdef DEBUG_andy
static int tstTranslatePath()
{
    RTAssertSetMayPanic(false /* Do not freak out, please. */);

    static struct
    {
        const char *pszSourceRoot;
        const char *pszSource;
        const char *pszDest;
        const char *pszTranslated;
        int         iResult;
    } aTests[] =
    {
        /* Invalid stuff. */
        { NULL, NULL, NULL, NULL, VERR_INVALID_POINTER },
#ifdef RT_OS_WINDOWS
        /* Windows paths. */
        { "c:\\foo", "c:\\foo\\bar.txt", "c:\\test", "c:\\test\\bar.txt", VINF_SUCCESS },
        { "c:\\foo", "c:\\foo\\baz\\bar.txt", "c:\\test", "c:\\test\\baz\\bar.txt", VINF_SUCCESS },
#else /* RT_OS_WINDOWS */
        { "/home/test/foo", "/home/test/foo/bar.txt", "/opt/test", "/opt/test/bar.txt", VINF_SUCCESS },
        { "/home/test/foo", "/home/test/foo/baz/bar.txt", "/opt/test", "/opt/test/baz/bar.txt", VINF_SUCCESS },
#endif /* !RT_OS_WINDOWS */
        /* Mixed paths*/
        /** @todo */
        { NULL }
    };

    size_t iTest = 0;
    for (iTest; iTest < RT_ELEMENTS(aTests); iTest++)
    {
        RTPrintf("=> Test %d\n", iTest);
        RTPrintf("\tSourceRoot=%s, Source=%s, Dest=%s\n",
                 aTests[iTest].pszSourceRoot, aTests[iTest].pszSource, aTests[iTest].pszDest);

        char *pszTranslated = NULL;
        int iResult =  ctrlCopyTranslatePath(aTests[iTest].pszSourceRoot, aTests[iTest].pszSource,
                                             aTests[iTest].pszDest, &pszTranslated);
        if (iResult != aTests[iTest].iResult)
        {
            RTPrintf("\tReturned %Rrc, expected %Rrc\n",
                     iResult, aTests[iTest].iResult);
        }
        else if (   pszTranslated
                 && strcmp(pszTranslated, aTests[iTest].pszTranslated))
        {
            RTPrintf("\tReturned translated path %s, expected %s\n",
                     pszTranslated, aTests[iTest].pszTranslated);
        }

        if (pszTranslated)
        {
            RTPrintf("\tTranslated=%s\n", pszTranslated);
            RTStrFree(pszTranslated);
        }
    }

    return VINF_SUCCESS; /* @todo */
}
#endif

/**
 * Creates a directory on the destination, based on the current copy
 * context.
 *
 * @return  IPRT status code.
 * @param   pContext                Pointer to current copy control context.
 * @param   pszDir                  Directory to create.
 */
static int ctrlCopyDirCreate(PCOPYCONTEXT pContext, const char *pszDir)
{
    AssertPtrReturn(pContext, VERR_INVALID_POINTER);
    AssertPtrReturn(pszDir, VERR_INVALID_POINTER);

    bool fDirExists;
    int rc = ctrlCopyDirExists(pContext, pContext->fHostToGuest, pszDir, &fDirExists);
    if (   RT_SUCCESS(rc)
        && fDirExists)
    {
        if (pContext->fVerbose)
            RTPrintf("Directory \"%s\" already exists\n", pszDir);
        return VINF_SUCCESS;
    }

    if (pContext->fVerbose)
        RTPrintf("Creating directory \"%s\" ...\n", pszDir);

    if (pContext->fDryRun)
        return VINF_SUCCESS;

    if (pContext->fHostToGuest) /* We want to create directories on the guest. */
    {
        HRESULT hrc = pContext->pGuest->DirectoryCreate(Bstr(pszDir).raw(),
                                                        Bstr(pContext->pszUsername).raw(), Bstr(pContext->pszPassword).raw(),
                                                        0700, DirectoryCreateFlag_Parents);
        if (FAILED(hrc))
            rc = ctrlPrintError(pContext->pGuest, COM_IIDOF(IGuest));
    }
    else /* ... or on the host. */
    {
        rc = RTDirCreateFullPath(pszDir, 0700);
        if (rc == VERR_ALREADY_EXISTS)
            rc = VINF_SUCCESS;
    }
    return rc;
}

/**
 * Checks whether a specific host/guest directory exists.
 *
 * @return  IPRT status code.
 * @param   pContext                Pointer to current copy control context.
 * @param   bGuest                  true if directory needs to be checked on the guest
 *                                  or false if on the host.
 * @param   pszDir                  Actual directory to check.
 * @param   fExists                 Pointer which receives the result if the
 *                                  given directory exists or not.
 */
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

/**
 * Checks whether a specific directory exists on the destination, based
 * on the current copy context.
 *
 * @return  IPRT status code.
 * @param   pContext                Pointer to current copy control context.
 * @param   pszDir                  Actual directory to check.
 * @param   fExists                 Pointer which receives the result if the
 *                                  given directory exists or not.
 */
static int ctrlCopyDirExistsOnDest(PCOPYCONTEXT pContext, const char *pszDir,
                                   bool *fExists)
{
    return ctrlCopyDirExists(pContext, pContext->fHostToGuest,
                             pszDir, fExists);
}

/**
 * Checks whether a specific directory exists on the source, based
 * on the current copy context.
 *
 * @return  IPRT status code.
 * @param   pContext                Pointer to current copy control context.
 * @param   pszDir                  Actual directory to check.
 * @param   fExists                 Pointer which receives the result if the
 *                                  given directory exists or not.
 */
static int ctrlCopyDirExistsOnSource(PCOPYCONTEXT pContext, const char *pszDir,
                                     bool *fExists)
{
    return ctrlCopyDirExists(pContext, !pContext->fHostToGuest,
                             pszDir, fExists);
}

/**
 * Checks whether a specific host/guest file exists.
 *
 * @return  IPRT status code.
 * @param   pContext                Pointer to current copy control context.
 * @param   bGuest                  true if file needs to be checked on the guest
 *                                  or false if on the host.
 * @param   pszFile                 Actual file to check.
 * @param   fExists                 Pointer which receives the result if the
 *                                  given file exists or not.
 */
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

/**
 * Checks whether a specific file exists on the destination, based on the
 * current copy context.
 *
 * @return  IPRT status code.
 * @param   pContext                Pointer to current copy control context.
 * @param   pszFile                 Actual file to check.
 * @param   fExists                 Pointer which receives the result if the
 *                                  given file exists or not.
 */
static int ctrlCopyFileExistsOnDest(PCOPYCONTEXT pContext, const char *pszFile,
                                    bool *fExists)
{
    return ctrlCopyFileExists(pContext, pContext->fHostToGuest,
                              pszFile, fExists);
}

/**
 * Checks whether a specific file exists on the source, based on the
 * current copy context.
 *
 * @return  IPRT status code.
 * @param   pContext                Pointer to current copy control context.
 * @param   pszFile                 Actual file to check.
 * @param   fExists                 Pointer which receives the result if the
 *                                  given file exists or not.
 */
static int ctrlCopyFileExistsOnSource(PCOPYCONTEXT pContext, const char *pszFile,
                                      bool *fExists)
{
    return ctrlCopyFileExists(pContext, !pContext->fHostToGuest,
                              pszFile, fExists);
}

/**
 * Copies a source file to the destination.
 *
 * @return  IPRT status code.
 * @param   pContext                Pointer to current copy control context.
 * @param   pszFileSource           Source file to copy to the destination.
 * @param   pszFileDest             Name of copied file on the destination.
 * @param   fFlags                  Copy flags. No supported at the moment and needs
 *                                  to be set to 0.
 */
static int ctrlCopyFileToDest(PCOPYCONTEXT pContext, const char *pszFileSource,
                              const char *pszFileDest, uint32_t fFlags)
{
    AssertPtrReturn(pContext, VERR_INVALID_POINTER);
    AssertPtrReturn(pszFileSource, VERR_INVALID_POINTER);
    AssertPtrReturn(pszFileDest, VERR_INVALID_POINTER);
    AssertReturn(!fFlags, VERR_INVALID_POINTER); /* No flags supported yet. */

    if (pContext->fVerbose)
        RTPrintf("Copying \"%s\" to \"%s\" ...\n",
                 pszFileSource, pszFileDest);

    if (pContext->fDryRun)
        return VINF_SUCCESS;

    int vrc = VINF_SUCCESS;
    ComPtr<IProgress> progress;
    HRESULT rc;
    if (pContext->fHostToGuest)
    {
        rc = pContext->pGuest->CopyToGuest(Bstr(pszFileSource).raw(), Bstr(pszFileDest).raw(),
                                           Bstr(pContext->pszUsername).raw(), Bstr(pContext->pszPassword).raw(),
                                           fFlags, progress.asOutParam());
    }
    else
    {
        rc = pContext->pGuest->CopyFromGuest(Bstr(pszFileSource).raw(), Bstr(pszFileDest).raw(),
                                             Bstr(pContext->pszUsername).raw(), Bstr(pContext->pszPassword).raw(),
                                             fFlags, progress.asOutParam());
    }

    if (FAILED(rc))
        vrc = ctrlPrintError(pContext->pGuest, COM_IIDOF(IGuest));
    else
    {
        rc = showProgress(progress);
        CHECK_PROGRESS_ERROR(progress, ("File copy failed"));
        if (FAILED(rc))
            vrc = ctrlPrintError(pContext->pGuest, COM_IIDOF(IGuest));
    }

    return vrc;
}

/**
 * Copys a directory (tree) from host to the guest.
 *
 * @return  IPRT status code.
 * @param   pContext                Pointer to current copy control context.
 * @param   pszSource               Source directory on the host to copy to the guest.
 * @param   pszFilter               DOS-style wildcard filter (?, *).  Optional.
 * @param   pszDest                 Destination directory on the guest.
 * @param   fFlags                  Copy flags, such as recursive copying.
 * @param   pszSubDir               Current sub directory to handle. Needs to NULL and only
 *                                  is needed for recursion.
 */
static int ctrlCopyDirToGuest(PCOPYCONTEXT pContext,
                              const char *pszSource, const char *pszFilter,
                              const char *pszDest, uint32_t fFlags,
                              const char *pszSubDir /* For recursion. */)
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

    if (pContext->fVerbose)
        RTPrintf("Processing host directory: %s\n", szCurDir);

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

                    if (pContext->fVerbose)
                        RTPrintf("Directory: %s\n", DirEntry.szName);

                    if (fFlags & CopyFileFlag_Recursive)
                    {
                        char *pszNewSub = NULL;
                        if (pszSubDir)
                            pszNewSub = RTPathJoinA(pszSubDir, DirEntry.szName);
                        else
                        {
                            pszNewSub = RTStrDup(DirEntry.szName);
                            RTPathStripTrailingSlash(pszNewSub);
                        }

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
                    if (   pszFilter
                        && !RTStrSimplePatternMatch(pszFilter, DirEntry.szName))
                    {
                        break; /* Filter does not match. */
                    }

                    if (pContext->fVerbose)
                        RTPrintf("File: %s\n", DirEntry.szName);

                    if (!fDirCreated)
                    {
                        char *pszDestDir;
                        rc = ctrlCopyTranslatePath(pszSource, szCurDir,
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
                        char *pszFileSource = RTPathJoinA(szCurDir, DirEntry.szName);
                        if (pszFileSource)
                        {
                            char *pszFileDest;
                            rc = ctrlCopyTranslatePath(pszSource, pszFileSource,
                                                       pszDest, &pszFileDest);
                            if (RT_SUCCESS(rc))
                            {
                                rc = ctrlCopyFileToDest(pContext, pszFileSource,
                                                        pszFileDest, 0 /* Flags */);
                                RTStrFree(pszFileDest);
                            }
                            RTStrFree(pszFileSource);
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

/**
 * Copys a directory (tree) from guest to the host.
 *
 * @return  IPRT status code.
 * @param   pContext                Pointer to current copy control context.
 * @param   pszSource               Source directory on the guest to copy to the host.
 * @param   pszFilter               DOS-style wildcard filter (?, *).  Optional.
 * @param   pszDest                 Destination directory on the host.
 * @param   fFlags                  Copy flags, such as recursive copying.
 * @param   pszSubDir               Current sub directory to handle. Needs to NULL and only
 *                                  is needed for recursion.
 */
static int ctrlCopyDirToHost(PCOPYCONTEXT pContext,
                             const char *pszSource, const char *pszFilter,
                             const char *pszDest, uint32_t fFlags,
                             const char *pszSubDir /* For recursion. */)
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

    if (pContext->fVerbose)
        RTPrintf("Processing guest directory: %s\n", szCurDir);

    /* Flag indicating whether the current directory was created on the
     * target or not. */
    bool fDirCreated = false;

    ULONG uDirHandle;
    HRESULT hr = pContext->pGuest->DirectoryOpen(Bstr(szCurDir).raw(), Bstr(pszFilter).raw(),
                                                 DirectoryOpenFlag_None /* No flags supported yet. */,
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

                    if (pContext->fVerbose)
                    {
                        Utf8Str Utf8Dir(strName);
                        RTPrintf("Directory: %s\n", Utf8Dir.c_str());
                    }

                    if (fFlags & CopyFileFlag_Recursive)
                    {
                        Utf8Str strDir(strName);
                        char *pszNewSub = NULL;
                        if (pszSubDir)
                            pszNewSub = RTPathJoinA(pszSubDir, strDir.c_str());
                        else
                        {
                            pszNewSub = RTStrDup(strDir.c_str());
                            RTPathStripTrailingSlash(pszNewSub);
                        }
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
                    if (   pszFilter
                        && !RTStrSimplePatternMatch(pszFilter, strFile.c_str()))
                    {
                        break; /* Filter does not match. */
                    }

                    if (pContext->fVerbose)
                        RTPrintf("File: %s\n", strFile.c_str());

                    if (!fDirCreated)
                    {
                        char *pszDestDir;
                        rc = ctrlCopyTranslatePath(pszSource, szCurDir,
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
                        char *pszFileSource = RTPathJoinA(szCurDir, strFile.c_str());
                        if (pszFileSource)
                        {
                            char *pszFileDest;
                            rc = ctrlCopyTranslatePath(pszSource, pszFileSource,
                                                       pszDest, &pszFileDest);
                            if (RT_SUCCESS(rc))
                            {
                                rc = ctrlCopyFileToDest(pContext, pszFileSource,
                                                        pszFileDest, 0 /* Flags */);
                                RTStrFree(pszFileDest);
                            }
                            RTStrFree(pszFileSource);
                        }
                        else
                            rc = VERR_NO_MEMORY;
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

/**
 * Copys a directory (tree) to the destination, based on the current copy
 * context.
 *
 * @return  IPRT status code.
 * @param   pContext                Pointer to current copy control context.
 * @param   pszSource               Source directory to copy to the destination.
 * @param   pszFilter               DOS-style wildcard filter (?, *).  Optional.
 * @param   pszDest                 Destination directory where to copy in the source
 *                                  source directory.
 * @param   fFlags                  Copy flags, such as recursive copying.
 */
static int ctrlCopyDirToDest(PCOPYCONTEXT pContext,
                             const char *pszSource, const char *pszFilter,
                             const char *pszDest, uint32_t fFlags)
{
    if (pContext->fHostToGuest)
        return ctrlCopyDirToGuest(pContext, pszSource, pszFilter,
                                  pszDest, fFlags, NULL /* Sub directory, only for recursion. */);
    return ctrlCopyDirToHost(pContext, pszSource, pszFilter,
                             pszDest, fFlags, NULL /* Sub directory, only for recursion. */);
}

/**
 * Creates a source root by stripping file names or filters of the specified source.
 *
 * @return  IPRT status code.
 * @param   pszSource               Source to create source root for.
 * @param   ppszSourceRoot          Pointer that receives the allocated source root. Needs
 *                                  to be free'd with ctrlCopyFreeSourceRoot().
 */
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

/**
 * Frees a previously allocated source root.
 *
 * @return  IPRT status code.
 * @param   pszSourceRoot           Source root to free.
 */
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
                 * --target-directory yet? Then use the current
                 * (= last) argument as destination. */
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
    vrc = ctrlCopyContextCreate(guest, fVerbose, fDryRun, fHostToGuest,
                                Utf8UserName.c_str(), Utf8Password.c_str(),
                                &pContext);
    if (RT_FAILURE(vrc))
    {
        RTMsgError("Unable to create copy context, rc=%Rrc\n", vrc);
        return RTEXITCODE_FAILURE;
    }

    /* If the destination is a path, (try to) create it. */
    const char *pszDest = Utf8Dest.c_str();
    if (!RTPathFilename(pszDest))
    {
        vrc = ctrlCopyDirCreate(pContext, pszDest);
    }
    else
    {
        /* We assume we got a file name as destination -- so strip
         * the actual file name and make sure the appropriate
         * directories get created. */
        char *pszDestDir = RTStrDup(pszDest);
        AssertPtr(pszDestDir);
        RTPathStripFilename(pszDestDir);
        vrc = ctrlCopyDirCreate(pContext, pszDestDir);
        RTStrFree(pszDestDir);
    }

    if (RT_SUCCESS(vrc))
    {
        /*
         * Here starts the actual fun!
         * Handle all given sources one by one.
         */
        for (unsigned long s = 0; s < vecSources.size(); s++)
        {
            char *pszSource = RTStrDup(vecSources[s].GetSource());
            AssertPtrBreakStmt(pszSource, vrc = VERR_NO_MEMORY);
            const char *pszFilter = vecSources[s].GetFilter();
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
            bool fSourceIsFile = false;
            bool fSourceExists;

            size_t cchSource = strlen(pszSource);
            if (   cchSource > 1
                && RTPATH_IS_SLASH(pszSource[cchSource - 1]))
            {
                if (pszFilter) /* Directory with filter (so use source root w/o the actual filter). */
                    vrc = ctrlCopyDirExistsOnSource(pContext, pszSourceRoot, &fSourceExists);
                else /* Regular directory without filter. */
                    vrc = ctrlCopyDirExistsOnSource(pContext, pszSource, &fSourceExists);

                if (fSourceExists)
                {
                    /* Strip trailing slash from our source element so that other functions
                     * can use this stuff properly (like RTPathStartsWith). */
                    RTPathStripTrailingSlash(pszSource);
                }
            }
            else
            {
                vrc = ctrlCopyFileExistsOnSource(pContext, pszSource, &fSourceExists);
                if (   RT_SUCCESS(vrc)
                    && fSourceExists)
                {
                    fSourceIsFile = true;
                }
            }

            if (   RT_SUCCESS(vrc)
                && fSourceExists)
            {
                if (fSourceIsFile)
                {
                    /* Single file. */
                    char *pszDestFile;
                    vrc = ctrlCopyTranslatePath(pszSourceRoot, pszSource,
                                                Utf8Dest.c_str(), &pszDestFile);
                    if (RT_SUCCESS(vrc))
                    {
                        vrc = ctrlCopyFileToDest(pContext, pszSource,
                                                 pszDestFile, 0 /* Flags */);
                        RTStrFree(pszDestFile);
                    }
                    else
                        RTMsgError("Unable to translate path for \"%s\", rc=%Rrc\n",
                                   pszSource, vrc);
                }
                else
                {
                    /* Directory (with filter?). */
                    vrc = ctrlCopyDirToDest(pContext, pszSource, pszFilter,
                                            Utf8Dest.c_str(), fFlags);
                }
            }

            ctrlCopyFreeSourceRoot(pszSourceRoot);

            if (   RT_SUCCESS(vrc)
                && !fSourceExists)
            {
                RTMsgError("Warning: Source \"%s\" does not exist, skipping!\n",
                           pszSource);
                RTStrFree(pszSource);
                continue;
            }
            else if (RT_FAILURE(vrc))
            {
                RTMsgError("Error processing \"%s\", rc=%Rrc\n",
                           pszSource, vrc);
                RTStrFree(pszSource);
                break;
            }

            RTStrFree(pszSource);
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
                    RTPrintf("Cannot stat for element \"%s\": No such file or directory\n",
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
            RTMsgError("Source could not be determined! Please use --source to specify a valid source\n");
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
            CHECK_PROGRESS_ERROR(progress, ("Guest additions update failed"));
            if (FAILED(rc))
                vrc = ctrlPrintError(guest, COM_IIDOF(IGuest));
            else if (   SUCCEEDED(rc)
                     && fVerbose)
            {
                RTPrintf("Guest Additions update successful\n");
            }
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

#ifdef DEBUG_andy_disabled
    if (RT_FAILURE(tstTranslatePath()))
        return RTEXITCODE_FAILURE;
#endif

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
        return RT_FAILURE(rcExit) ? RTEXITCODE_FAILURE : RTEXITCODE_SUCCESS;
    }
    return RTEXITCODE_FAILURE;
}

#endif /* !VBOX_ONLY_DOCS */

