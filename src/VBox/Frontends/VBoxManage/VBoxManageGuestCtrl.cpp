/* $Id$ */
/** @file
 * VBoxManage - Implementation of guestcontrol command.
 */

/*
 * Copyright (C) 2010-2012 Oracle Corporation
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
    COPYCONTEXT() : fVerbose(false), fDryRun(false), fHostToGuest(false)
    {
    }

    ComPtr<IGuestSession> pGuestSession;
    bool fVerbose;
    bool fDryRun;
    bool fHostToGuest;
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
    GETOPTDEF_EXEC_PASSWORD,
    GETOPTDEF_EXEC_WAITFOREXIT,
    GETOPTDEF_EXEC_WAITFORSTDOUT,
    GETOPTDEF_EXEC_WAITFORSTDERR
};

enum GETOPTDEF_COPY
{
    GETOPTDEF_COPY_DRYRUN = 1000,
    GETOPTDEF_COPY_FOLLOW,
    GETOPTDEF_COPY_PASSWORD,
    GETOPTDEF_COPY_TARGETDIR
};

enum GETOPTDEF_MKDIR
{
    GETOPTDEF_MKDIR_PASSWORD = 1000
};

enum GETOPTDEF_STAT
{
    GETOPTDEF_STAT_PASSWORD = 1000
};

enum OUTPUTTYPE
{
    OUTPUTTYPE_UNDEFINED = 0,
    OUTPUTTYPE_DOS2UNIX  = 10,
    OUTPUTTYPE_UNIX2DOS  = 20
};

static int ctrlCopyDirExists(PCOPYCONTEXT pContext, bool bGuest, const char *pszDir, bool *fExists);

#endif /* VBOX_ONLY_DOCS */

void usageGuestControl(PRTSTREAM pStrm, const char *pcszSep1, const char *pcszSep2)
{
    RTStrmPrintf(pStrm,
                       "%s guestcontrol %s    <vmname>|<uuid>\n"
                 "                            exec[ute]\n"
                 "                            --image <path to program> --username <name>\n"
                 "                            [--passwordfile <file> | --password <password>]\n"
                 "                            [--domain <domain>] [--verbose] [--timeout <msec>]\n"
                 "                            [--environment \"<NAME>=<VALUE> [<NAME>=<VALUE>]\"]\n"
                 "                            [--wait-exit] [--wait-stdout] [--wait-stderr]\n"
                 "                            [--dos2unix] [--unix2dos]\n"
                 "                            [-- [<argument1>] ... [<argumentN>]]\n"
                 /** @todo Add a "--" parameter (has to be last parameter) to directly execute
                  *        stuff, e.g. "VBoxManage guestcontrol execute <VMName> --username <> ... -- /bin/rm -Rf /foo". */
                 "\n"
                 "                            copyfrom\n"
                 "                            <guest source> <host dest> --username <name>\n"
                 "                            [--passwordfile <file> | --password <password>]\n"
                 "                            [--domain <domain>] [--verbose]\n"
                 "                            [--dryrun] [--follow] [--recursive]\n"
                 "\n"
                 "                            copyto|cp\n"
                 "                            <host source> <guest dest> --username <name>\n"
                 "                            [--passwordfile <file> | --password <password>]\n"
                 "                            [--domain <domain>] [--verbose]\n"
                 "                            [--dryrun] [--follow] [--recursive]\n"
                 "\n"
                 "                            createdir[ectory]|mkdir|md\n"
                 "                            <guest directory>... --username <name>\n"
                 "                            [--passwordfile <file> | --password <password>]\n"
                 "                            [--domain <domain>] [--verbose]\n"
                 "                            [--parents] [--mode <mode>]\n"
                 "\n"
                 "                            stat\n"
                 "                            <file>... --username <name>\n"
                 "                            [--passwordfile <file> | --password <password>]\n"
                 "                            [--domain <domain>] [--verbose]\n"
                 "\n"
                 "                            updateadditions\n"
                 "                            [--source <guest additions .ISO>] [--verbose]\n"
                 "\n", pcszSep1, pcszSep2);
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
static const char *ctrlExecProcessStatusToText(ProcessStatus_T enmStatus)
{
    switch (enmStatus)
    {
        case ProcessStatus_Starting:
            return "starting";
        case ProcessStatus_Started:
            return "started";
        case ProcessStatus_Paused:
            return "paused";
        case ProcessStatus_Terminating:
            return "terminating";
        case ProcessStatus_TerminatedNormally:
            return "successfully terminated";
        case ProcessStatus_TerminatedSignal:
            return "terminated by signal";
        case ProcessStatus_TerminatedAbnormally:
            return "abnormally aborted";
        case ProcessStatus_TimedOutKilled:
            return "timed out";
        case ProcessStatus_TimedOutAbnormally:
            return "timed out, hanging";
        case ProcessStatus_Down:
            return "killed";
        case ProcessStatus_Error:
            return "error";
        default:
            break;
    }
    return "unknown";
}

static int ctrlExecProcessStatusToExitCode(ProcessStatus_T enmStatus, ULONG uExitCode)
{
    int vrc = EXITCODEEXEC_SUCCESS;
    switch (enmStatus)
    {
        case ProcessStatus_Starting:
            vrc = EXITCODEEXEC_SUCCESS;
            break;
        case ProcessStatus_Started:
            vrc = EXITCODEEXEC_SUCCESS;
            break;
        case ProcessStatus_Paused:
            vrc = EXITCODEEXEC_SUCCESS;
            break;
        case ProcessStatus_Terminating:
            vrc = EXITCODEEXEC_SUCCESS;
            break;
        case ProcessStatus_TerminatedNormally:
            vrc = !uExitCode ? EXITCODEEXEC_SUCCESS : EXITCODEEXEC_CODE;
            break;
        case ProcessStatus_TerminatedSignal:
            vrc = EXITCODEEXEC_TERM_SIGNAL;
            break;
        case ProcessStatus_TerminatedAbnormally:
            vrc = EXITCODEEXEC_TERM_ABEND;
            break;
        case ProcessStatus_TimedOutKilled:
            vrc = EXITCODEEXEC_TIMEOUT;
            break;
        case ProcessStatus_TimedOutAbnormally:
            vrc = EXITCODEEXEC_TIMEOUT;
            break;
        case ProcessStatus_Down:
            /* Service/OS is stopping, process was killed, so
             * not exactly an error of the started process ... */
            vrc = EXITCODEEXEC_DOWN;
            break;
        case ProcessStatus_Error:
            vrc = EXITCODEEXEC_FAILED;
            break;
        default:
            AssertMsgFailed(("Unknown exit code (%u) from guest process returned!\n", enmStatus));
            break;
    }
    return vrc;
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
    AssertMsgFailedReturn(("Object has indicated no error (%Rhrc)!?\n", errorInfo.getResultCode()),
                          VERR_INVALID_PARAMETER);
}

static int ctrlPrintError(IUnknown *pObj, const GUID &aIID)
{
    com::ErrorInfo ErrInfo(pObj, aIID);
    return ctrlPrintError(ErrInfo);
}

static int ctrlPrintProgressError(ComPtr<IProgress> pProgress)
{
    int vrc = VINF_SUCCESS;
    HRESULT rc;

    do
    {
        BOOL fCanceled;
        CHECK_ERROR_BREAK(pProgress, COMGETTER(Canceled)(&fCanceled));
        if (!fCanceled)
        {
            LONG rcProc;
            CHECK_ERROR_BREAK(pProgress, COMGETTER(ResultCode)(&rcProc));
            if (FAILED(rcProc))
            {
                com::ProgressErrorInfo ErrInfo(pProgress);
                vrc = ctrlPrintError(ErrInfo);
            }
        }

    } while(0);

    if (FAILED(rc))
        AssertMsgStmt(NULL, ("Could not lookup progress information\n"), vrc = VERR_COM_UNEXPECTED);

    return vrc;
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
 * @param   pProcess        Pointer to appropriate process object.
 * @param   pStrmOutput     Where to write the data.
 * @param   hStream         Where to read the data from.
 */
static int ctrlExecPrintOutput(IProcess *pProcess, PRTSTREAM pStrmOutput,
                               ULONG uHandle)
{
    AssertPtrReturn(pProcess, VERR_INVALID_POINTER);
    AssertPtrReturn(pStrmOutput, VERR_INVALID_POINTER);

    int vrc = VINF_SUCCESS;

    SafeArray<BYTE> aOutputData;
    HRESULT rc = pProcess->Read(uHandle, _64K, 30 * 1000 /* 30s timeout. */,
                                ComSafeArrayAsOutParam(aOutputData));
    if (FAILED(rc))
        vrc = ctrlPrintError(pProcess, COM_IIDOF(IProcess));
    else
    {
        /** @todo implement the dos2unix/unix2dos conversions */
        vrc = RTStrmWrite(pStrmOutput, aOutputData.raw(), aOutputData.size());
        if (RT_FAILURE(vrc))
            RTMsgError("Unable to write output, rc=%Rrc\n", vrc);
    }

    return vrc;
}

/**
 * Returns the remaining time (in ms) based on the start time and a set
 * timeout value. Returns RT_INDEFINITE_WAIT if no timeout was specified.
 *
 * @return  RTMSINTERVAL    Time left (in ms).
 * @param   u64StartMs      Start time (in ms).
 * @param   cMsTimeout      Timeout value (in ms).
 */
inline RTMSINTERVAL ctrlExecGetRemainingTime(uint64_t u64StartMs, RTMSINTERVAL cMsTimeout)
{
    if (!cMsTimeout || cMsTimeout == RT_INDEFINITE_WAIT) /* If no timeout specified, wait forever. */
        return RT_INDEFINITE_WAIT;

    uint32_t u64ElapsedMs = RTTimeMilliTS() - u64StartMs;
    if (u64ElapsedMs >= cMsTimeout)
        return 0;

    return cMsTimeout - u64ElapsedMs;
}

/* <Missing documentation> */
static int handleCtrlExecProgram(ComPtr<IGuest> pGuest, HandlerArg *pArg)
{
    AssertPtrReturn(pArg, VERR_INVALID_PARAMETER);

    /*
     * Parse arguments.
     */
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--dos2unix",                     GETOPTDEF_EXEC_DOS2UNIX,                  RTGETOPT_REQ_NOTHING },
        { "--environment",                  'e',                                      RTGETOPT_REQ_STRING  },
        { "--flags",                        'f',                                      RTGETOPT_REQ_STRING  },
        { "--ignore-operhaned-processes",   GETOPTDEF_EXEC_IGNOREORPHANEDPROCESSES,   RTGETOPT_REQ_NOTHING },
        { "--image",                        'i',                                      RTGETOPT_REQ_STRING  },
        { "--no-profile",                   GETOPTDEF_EXEC_NO_PROFILE,                RTGETOPT_REQ_NOTHING },
        { "--username",                     'u',                                      RTGETOPT_REQ_STRING  },
        { "--passwordfile",                 'p',                                      RTGETOPT_REQ_STRING  },
        { "--password",                     GETOPTDEF_EXEC_PASSWORD,                  RTGETOPT_REQ_STRING  },
        { "--domain",                       'd',                                      RTGETOPT_REQ_STRING  },
        { "--timeout",                      't',                                      RTGETOPT_REQ_UINT32  },
        { "--unix2dos",                     GETOPTDEF_EXEC_UNIX2DOS,                  RTGETOPT_REQ_NOTHING },
        { "--verbose",                      'v',                                      RTGETOPT_REQ_NOTHING },
        { "--wait-exit",                    GETOPTDEF_EXEC_WAITFOREXIT,               RTGETOPT_REQ_NOTHING },
        { "--wait-stdout",                  GETOPTDEF_EXEC_WAITFORSTDOUT,             RTGETOPT_REQ_NOTHING },
        { "--wait-stderr",                  GETOPTDEF_EXEC_WAITFORSTDERR,             RTGETOPT_REQ_NOTHING }
    };

    int                     ch;
    RTGETOPTUNION           ValueUnion;
    RTGETOPTSTATE           GetState;
    RTGetOptInit(&GetState, pArg->argc, pArg->argv, s_aOptions, RT_ELEMENTS(s_aOptions), 0, 0);

    Utf8Str                 strCmd;
    com::SafeArray<ProcessCreateFlag_T>  aCreateFlags;
    com::SafeArray<ProcessWaitForFlag_T> aWaitFlags;
    com::SafeArray<IN_BSTR>              args;
    com::SafeArray<IN_BSTR>              env;
    Utf8Str                              strUsername;
    Utf8Str                              strPassword;
    Utf8Str                              strDomain;
    RTMSINTERVAL                         cMsTimeout      = 0;
    OUTPUTTYPE                           eOutputType     = OUTPUTTYPE_UNDEFINED;
    bool                                 fWaitForExit    = false;
    bool                                 fVerbose        = false;
    int                                  vrc             = VINF_SUCCESS;

    /* Wait for process start in any case. This is useful for scripting VBoxManage
     * when relying on its overall exit code. */
    aWaitFlags.push_back(ProcessWaitForFlag_Start);

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
                aCreateFlags.push_back(ProcessCreateFlag_IgnoreOrphanedProcesses);
                break;

            case GETOPTDEF_EXEC_NO_PROFILE:
                aCreateFlags.push_back(ProcessCreateFlag_NoProfile);
                break;

            case 'i':
                strCmd = ValueUnion.psz;
                break;

            /** @todo Add a hidden flag. */

            case 'u': /* User name */
                strUsername = ValueUnion.psz;
                break;

            case GETOPTDEF_EXEC_PASSWORD: /* Password */
                strPassword = ValueUnion.psz;
                break;

            case 'p': /* Password file */
            {
                RTEXITCODE rcExit = readPasswordFile(ValueUnion.psz, &strPassword);
                if (rcExit != RTEXITCODE_SUCCESS)
                    return rcExit;
                break;
            }

            case 'd': /* domain */
                strDomain = ValueUnion.psz;
                break;

            case 't': /* Timeout */
                cMsTimeout = ValueUnion.u32;
                break;

            case GETOPTDEF_EXEC_UNIX2DOS:
                if (eOutputType != OUTPUTTYPE_UNDEFINED)
                    return errorSyntax(USAGE_GUESTCONTROL, "More than one output type (dos2unix/unix2dos) specified!");
                eOutputType = OUTPUTTYPE_UNIX2DOS;
                break;

            case 'v': /* Verbose */
                fVerbose = true;
                break;

            case GETOPTDEF_EXEC_WAITFOREXIT:
                aWaitFlags.push_back(ProcessWaitForFlag_Terminate);
                fWaitForExit = true;
                break;

            case GETOPTDEF_EXEC_WAITFORSTDOUT:
                aCreateFlags.push_back(ProcessCreateFlag_WaitForStdOut);
                aWaitFlags.push_back(ProcessWaitForFlag_StdOut);
                fWaitForExit = true;
                break;

            case GETOPTDEF_EXEC_WAITFORSTDERR:
                aCreateFlags.push_back(ProcessCreateFlag_WaitForStdErr);
                aWaitFlags.push_back(ProcessWaitForFlag_StdErr);
                fWaitForExit = true;
                break;

            case VINF_GETOPT_NOT_OPTION:
            {
                if (args.size() == 0 && strCmd.isEmpty())
                    strCmd = ValueUnion.psz;
                else
                    args.push_back(Bstr(ValueUnion.psz).raw());
                break;
            }

            default:
                return RTGetOptPrintError(ch, &ValueUnion);
        }
    }

    if (strCmd.isEmpty())
        return errorSyntax(USAGE_GUESTCONTROL, "No command to execute specified!");

    if (strUsername.isEmpty())
        return errorSyntax(USAGE_GUESTCONTROL, "No user name specified!");

    /* Any output conversion not supported yet! */
    if (eOutputType != OUTPUTTYPE_UNDEFINED)
        return errorSyntax(USAGE_GUESTCONTROL, "Output conversion not implemented yet!");

    /*
     * Start with the real work.
     */
    HRESULT rc = S_OK;
    if (fVerbose)
    {
        if (cMsTimeout == 0)
            RTPrintf("Waiting for guest to start process ...\n");
        else
            RTPrintf("Waiting for guest to start process (within %ums)\n", cMsTimeout);
    }

    ComPtr<IGuestSession> pGuestSession;
    rc = pGuest->CreateSession(Bstr(strUsername).raw(),
                               Bstr(strPassword).raw(),
                               Bstr(strDomain).raw(),
                               Bstr("VBoxManage Guest Control Exec").raw(),
                               pGuestSession.asOutParam());
    if (FAILED(rc))
    {
        ctrlPrintError(pGuest, COM_IIDOF(IGuest));
        return RTEXITCODE_FAILURE;
    }

    /* Get current time stamp to later calculate rest of timeout left. */
    uint64_t u64StartMS = RTTimeMilliTS();

    /*
     * Execute the process.
     */
    ComPtr<IGuestProcess> pProcess;
    rc = pGuestSession->ProcessCreate(Bstr(strCmd).raw(),
                                      ComSafeArrayAsInParam(args),
                                      ComSafeArrayAsInParam(env),
                                      ComSafeArrayAsInParam(aCreateFlags),
                                      cMsTimeout,
                                      pProcess.asOutParam());
    if (FAILED(rc))
    {
        ctrlPrintError(pGuestSession, COM_IIDOF(IGuestSession));
        return RTEXITCODE_FAILURE;
    }

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

        /** @todo does this need signal handling? there's no progress object etc etc */

        vrc = RTStrmSetMode(g_pStdOut, 1 /* Binary mode */, -1 /* Code set, unchanged */);
        if (RT_FAILURE(vrc))
            RTMsgError("Unable to set stdout's binary mode, rc=%Rrc\n", vrc);
        vrc = RTStrmSetMode(g_pStdErr, 1 /* Binary mode */, -1 /* Code set, unchanged */);
        if (RT_FAILURE(vrc))
            RTMsgError("Unable to set stderr's binary mode, rc=%Rrc\n", vrc);

        /* Wait for process to exit ... */
        RTMSINTERVAL cMsTimeLeft = 1;
        bool fReadStdOut, fReadStdErr;
        fReadStdOut = fReadStdErr = false;
        bool fCompleted = false;
        while (!fCompleted && cMsTimeLeft != 0)
        {
            cMsTimeLeft = ctrlExecGetRemainingTime(u64StartMS, cMsTimeout);
            ProcessWaitResult_T waitResult;
            rc = pProcess->WaitForArray(ComSafeArrayAsInParam(aWaitFlags), cMsTimeLeft, &waitResult);
            if (FAILED(rc))
            {
                ctrlPrintError(pProcess, COM_IIDOF(IProcess));
                return RTEXITCODE_FAILURE;
            }

            switch (waitResult)
            {
                case ProcessWaitResult_Start:
                {
                    if (fVerbose)
                    {
                        ULONG uPID = 0;
                        rc = pProcess->COMGETTER(PID)(&uPID);
                        if (FAILED(rc))
                        {
                            ctrlPrintError(pProcess, COM_IIDOF(IProcess));
                            return RTEXITCODE_FAILURE;
                        }
                        RTPrintf("Process '%s' (PID: %u) started\n", strCmd.c_str(), uPID);
                    }
                    break;
                }
                case ProcessWaitResult_StdOut:
                    fReadStdOut = true;
                    break;
               case ProcessWaitResult_StdErr:
                    fReadStdErr = true;
                    break;
                case ProcessWaitResult_Terminate:
                    /* Process terminated, we're done */
                    fCompleted = true;
                    break;
                case ProcessWaitResult_WaitFlagNotSupported:
                {
                    /* The guest does not support waiting for stdout/err, so
                     * yield to reduce the CPU load due to busy waiting. */
                    RTThreadYield(); /* Optional, don't check rc. */

                    /* Try both, stdout + stderr. */
                    fReadStdOut = fReadStdErr = true;
                    break;
                }
                default:
                    /* Ignore all other results, let the timeout expire */;
                    break;
            }

            if (fReadStdOut) /* Do we need to fetch stdout data? */
            {
                vrc = ctrlExecPrintOutput(pProcess, g_pStdOut, 1 /* StdOut */);
                fReadStdOut = false;
            }

            if (fReadStdErr) /* Do we need to fetch stdout data? */
            {
                vrc = ctrlExecPrintOutput(pProcess, g_pStdErr, 2 /* StdErr */);
                fReadStdErr = false;
            }

            if (RT_FAILURE(vrc))
                break;

        } /* while */

        /* Report status back to the user. */
        if (fCompleted)
        {
            ProcessStatus_T status;
            rc = pProcess->COMGETTER(Status)(&status);
            if (FAILED(rc))
            {
                ctrlPrintError(pProcess, COM_IIDOF(IProcess));
                return RTEXITCODE_FAILURE;
            }
            LONG exitCode;
            rc = pProcess->COMGETTER(ExitCode)(&exitCode);
            if (FAILED(rc))
            {
                ctrlPrintError(pProcess, COM_IIDOF(IProcess));
                return RTEXITCODE_FAILURE;
            }
            if (fVerbose)
                RTPrintf("Exit code=%u (Status=%u [%s])\n", exitCode, status, ctrlExecProcessStatusToText(status));
            return ctrlExecProcessStatusToExitCode(status, exitCode);
        }
        else
        {
            if (fVerbose)
                RTPrintf("Process execution aborted!\n");
            return EXITCODEEXEC_TERM_ABEND;
        }
    }

    return RT_FAILURE(vrc) || FAILED(rc) ? RTEXITCODE_FAILURE : RTEXITCODE_SUCCESS;
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
 * @param   strUsername             Username of account to use on the guest side.
 * @param   strPassword             Password of account to use.
 * @param   strDomain               Domain of account to use.
 * @param   strSessionName          Session name (only for identification purposes).
 * @param   ppContext               Pointer which receives the allocated copy context.
 */
static int ctrlCopyContextCreate(IGuest *pGuest, bool fVerbose, bool fDryRun,
                                 bool fHostToGuest, const Utf8Str &strUsername,
                                 const Utf8Str &strPassword, const Utf8Str &strDomain,
                                 const Utf8Str &strSessionName,
                                 PCOPYCONTEXT *ppContext)
{
    AssertPtrReturn(pGuest, VERR_INVALID_POINTER);

    PCOPYCONTEXT pContext = new COPYCONTEXT();
    AssertPtrReturn(pContext, VERR_NO_MEMORY); /**< @todo r=klaus cannot happen with new */
    ComPtr<IGuestSession> pGuestSession;
    HRESULT rc = pGuest->CreateSession(Bstr(strUsername).raw(),
                                       Bstr(strPassword).raw(),
                                       Bstr(strDomain).raw(),
                                       Bstr(strSessionName).raw(),
                                       pGuestSession.asOutParam());
    if (FAILED(rc))
        return ctrlPrintError(pGuest, COM_IIDOF(IGuest));

    pContext->fVerbose = fVerbose;
    pContext->fDryRun = fDryRun;
    pContext->fHostToGuest = fHostToGuest;
    pContext->pGuestSession = pGuestSession;

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
        if (pContext->pGuestSession)
            pContext->pGuestSession->Close();
        delete pContext;
    }
}

/**
 * Translates a source path to a destination path (can be both sides,
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

    int vrc;
    if (!RTPathFilename(pszDestPath))
    {
        vrc = RTPathJoin(szTranslated, sizeof(szTranslated),
                         pszDestPath, &pszSource[srcOff]);
    }
    else
    {
        char *pszDestFileName = RTStrDup(RTPathFilename(pszDestPath));
        if (pszDestFileName)
        {
            RTPathStripFilename(pszDestPath);
            vrc = RTPathJoin(szTranslated, sizeof(szTranslated),
                            pszDestPath, pszDestFileName);
            RTStrFree(pszDestFileName);
        }
        else
            vrc = VERR_NO_MEMORY;
    }
    RTStrFree(pszDestPath);

    if (RT_SUCCESS(vrc))
    {
        *ppszTranslated = RTStrDup(szTranslated);
#if 0
        RTPrintf("Root: %s, Source: %s, Dest: %s, Translated: %s\n",
                 pszSourceRoot, pszSource, pszDest, *ppszTranslated);
#endif
    }
    return vrc;
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
    int vrc = ctrlCopyDirExists(pContext, pContext->fHostToGuest, pszDir, &fDirExists);
    if (   RT_SUCCESS(vrc)
        && fDirExists)
    {
        if (pContext->fVerbose)
            RTPrintf("Directory \"%s\" already exists\n", pszDir);
        return VINF_SUCCESS;
    }

    /* If querying for a directory existence fails there's no point of even trying
     * to create such a directory. */
    if (RT_FAILURE(vrc))
        return vrc;

    if (pContext->fVerbose)
        RTPrintf("Creating directory \"%s\" ...\n", pszDir);

    if (pContext->fDryRun)
        return VINF_SUCCESS;

    if (pContext->fHostToGuest) /* We want to create directories on the guest. */
    {
        SafeArray<DirectoryCreateFlag_T> dirCreateFlags;
        dirCreateFlags.push_back(DirectoryCreateFlag_Parents);
        HRESULT rc = pContext->pGuestSession->DirectoryCreate(Bstr(pszDir).raw(),
                                                              0700, ComSafeArrayAsInParam(dirCreateFlags));
        if (FAILED(rc))
            vrc = ctrlPrintError(pContext->pGuestSession, COM_IIDOF(IGuestSession));
    }
    else /* ... or on the host. */
    {
        vrc = RTDirCreateFullPath(pszDir, 0700);
        if (vrc == VERR_ALREADY_EXISTS)
            vrc = VINF_SUCCESS;
    }
    return vrc;
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

    int vrc = VINF_SUCCESS;
    if (bGuest)
    {
        BOOL fDirExists = FALSE;
        HRESULT rc = pContext->pGuestSession->DirectoryExists(Bstr(pszDir).raw(), &fDirExists);
        if (FAILED(rc))
            vrc = ctrlPrintError(pContext->pGuestSession, COM_IIDOF(IGuestSession));
        else
            *fExists = fDirExists ? true : false;
    }
    else
        *fExists = RTDirExists(pszDir);
    return vrc;
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

    int vrc = VINF_SUCCESS;
    if (bOnGuest)
    {
        BOOL fFileExists = FALSE;
        HRESULT rc = pContext->pGuestSession->FileExists(Bstr(pszFile).raw(), &fFileExists);
        if (FAILED(rc))
            vrc = ctrlPrintError(pContext->pGuestSession, COM_IIDOF(IGuestSession));
        else
            *fExists = fFileExists ? true : false;
    }
    else
        *fExists = RTFileExists(pszFile);
    return vrc;
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
    ComPtr<IProgress> pProgress;
    HRESULT rc;
    if (pContext->fHostToGuest)
    {
        SafeArray<CopyFileFlag_T> copyFlags;
        rc = pContext->pGuestSession->CopyTo(Bstr(pszFileSource).raw(), Bstr(pszFileDest).raw(),
                                             ComSafeArrayAsInParam(copyFlags),

                                             pProgress.asOutParam());
    }
    else
    {
        SafeArray<CopyFileFlag_T> copyFlags;
        rc = pContext->pGuestSession->CopyFrom(Bstr(pszFileSource).raw(), Bstr(pszFileDest).raw(),
                                               ComSafeArrayAsInParam(copyFlags),
                                               pProgress.asOutParam());
    }

    if (FAILED(rc))
    {
        vrc = ctrlPrintError(pContext->pGuestSession, COM_IIDOF(IGuestSession));
    }
    else
    {
        if (pContext->fVerbose)
            rc = showProgress(pProgress);
        else
            rc = pProgress->WaitForCompletion(-1 /* No timeout */);
        if (SUCCEEDED(rc))
            CHECK_PROGRESS_ERROR(pProgress, ("File copy failed"));
        vrc = ctrlPrintProgressError(pProgress);
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
    int vrc = RTStrCopy(szCurDir, sizeof(szCurDir), pszSource);
    if (RT_SUCCESS(vrc) && pszSubDir)
        vrc = RTPathAppend(szCurDir, sizeof(szCurDir), pszSubDir);

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
    if (RT_SUCCESS(vrc))
    {
        vrc = RTDirOpen(&pDir, szCurDir);
        if (RT_FAILURE(vrc))
            pDir = NULL;
    }
    if (RT_SUCCESS(vrc))
    {
        /*
         * Enumerate the directory tree.
         */
        while (RT_SUCCESS(vrc))
        {
            RTDIRENTRY DirEntry;
            vrc = RTDirRead(pDir, &DirEntry, NULL);
            if (RT_FAILURE(vrc))
            {
                if (vrc == VERR_NO_MORE_FILES)
                    vrc = VINF_SUCCESS;
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
                            vrc = ctrlCopyDirToGuest(pContext,
                                                     pszSource, pszFilter,
                                                     pszDest, fFlags, pszNewSub);
                            RTStrFree(pszNewSub);
                        }
                        else
                            vrc = VERR_NO_MEMORY;
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
                        vrc = ctrlCopyTranslatePath(pszSource, szCurDir,
                                                    pszDest, &pszDestDir);
                        if (RT_SUCCESS(vrc))
                        {
                            vrc = ctrlCopyDirCreate(pContext, pszDestDir);
                            RTStrFree(pszDestDir);

                            fDirCreated = true;
                        }
                    }

                    if (RT_SUCCESS(vrc))
                    {
                        char *pszFileSource = RTPathJoinA(szCurDir, DirEntry.szName);
                        if (pszFileSource)
                        {
                            char *pszFileDest;
                            vrc = ctrlCopyTranslatePath(pszSource, pszFileSource,
                                                       pszDest, &pszFileDest);
                            if (RT_SUCCESS(vrc))
                            {
                                vrc = ctrlCopyFileToDest(pContext, pszFileSource,
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
            if (RT_FAILURE(vrc))
                break;
        }

        RTDirClose(pDir);
    }
    return vrc;
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
    int vrc = RTStrCopy(szCurDir, sizeof(szCurDir), pszSource);
    if (RT_SUCCESS(vrc) && pszSubDir)
        vrc = RTPathAppend(szCurDir, sizeof(szCurDir), pszSubDir);

    if (RT_FAILURE(vrc))
        return vrc;

    if (pContext->fVerbose)
        RTPrintf("Processing guest directory: %s\n", szCurDir);

    /* Flag indicating whether the current directory was created on the
     * target or not. */
    bool fDirCreated = false;
    SafeArray<DirectoryOpenFlag_T> dirOpenFlags; /* No flags supported yet. */
    ComPtr<IGuestDirectory> pDirectory;
    HRESULT rc = pContext->pGuestSession->DirectoryOpen(Bstr(szCurDir).raw(), Bstr(pszFilter).raw(),
                                                        ComSafeArrayAsInParam(dirOpenFlags),
                                                        pDirectory.asOutParam());
    if (FAILED(rc))
        return ctrlPrintError(pContext->pGuestSession, COM_IIDOF(IGuestSession));
    ComPtr<IFsObjInfo> dirEntry;
    while (true)
    {
        rc = pDirectory->Read(dirEntry.asOutParam());
        if (FAILED(rc))
            break;

        FsObjType_T enmType;
        dirEntry->COMGETTER(Type)(&enmType);

        Bstr strName;
        dirEntry->COMGETTER(Name)(strName.asOutParam());

        switch (enmType)
        {
            case FsObjType_Directory:
            {
                Assert(!strName.isEmpty());

                /* Skip "." and ".." entries. */
                if (   !strName.compare(Bstr("."))
                    || !strName.compare(Bstr("..")))
                    break;

                if (pContext->fVerbose)
                {
                    Utf8Str strDir(strName);
                    RTPrintf("Directory: %s\n", strDir.c_str());
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
                        vrc = ctrlCopyDirToHost(pContext,
                                                pszSource, pszFilter,
                                                pszDest, fFlags, pszNewSub);
                        RTStrFree(pszNewSub);
                    }
                    else
                        vrc = VERR_NO_MEMORY;
                }
                break;
            }

            case FsObjType_Symlink:
                if (   (fFlags & CopyFileFlag_Recursive)
                    && (fFlags & CopyFileFlag_FollowLinks))
                {
                    /* Fall through to next case is intentional. */
                }
                else
                    break;

            case FsObjType_File:
            {
                Assert(!strName.isEmpty());

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
                    vrc = ctrlCopyTranslatePath(pszSource, szCurDir,
                                                pszDest, &pszDestDir);
                    if (RT_SUCCESS(vrc))
                    {
                        vrc = ctrlCopyDirCreate(pContext, pszDestDir);
                        RTStrFree(pszDestDir);

                        fDirCreated = true;
                    }
                }

                if (RT_SUCCESS(vrc))
                {
                    char *pszFileSource = RTPathJoinA(szCurDir, strFile.c_str());
                    if (pszFileSource)
                    {
                        char *pszFileDest;
                        vrc = ctrlCopyTranslatePath(pszSource, pszFileSource,
                                                   pszDest, &pszFileDest);
                        if (RT_SUCCESS(vrc))
                        {
                            vrc = ctrlCopyFileToDest(pContext, pszFileSource,
                                                    pszFileDest, 0 /* Flags */);
                            RTStrFree(pszFileDest);
                        }
                        RTStrFree(pszFileSource);
                    }
                    else
                        vrc = VERR_NO_MEMORY;
                }
                break;
            }

            default:
                RTPrintf("Warning: Directory entry of type %ld not handled, skipping ...\n",
                         enmType);
                break;
        }

        if (RT_FAILURE(vrc))
            break;
    }

    if (RT_UNLIKELY(FAILED(rc)))
    {
        switch (rc)
        {
            case E_ABORT: /* No more directory entries left to process. */
                break;

            case VBOX_E_FILE_ERROR: /* Current entry cannot be accessed to
                                       to missing rights. */
            {
                RTPrintf("Warning: Cannot access \"%s\", skipping ...\n",
                         szCurDir);
                break;
            }

            default:
                vrc = ctrlPrintError(pDirectory, COM_IIDOF(IGuestDirectory));
                break;
        }
    }

    HRESULT rc2 = pDirectory->Close();
    if (FAILED(rc2))
    {
        int vrc2 = ctrlPrintError(pDirectory, COM_IIDOF(IGuestDirectory));
        if (RT_SUCCESS(vrc))
            vrc = vrc2;
    }
    else if (SUCCEEDED(rc))
        rc = rc2;

    return vrc;
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

static int handleCtrlCopy(ComPtr<IGuest> guest, HandlerArg *pArg,
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
        { "--dryrun",              GETOPTDEF_COPY_DRYRUN,           RTGETOPT_REQ_NOTHING },
        { "--follow",              GETOPTDEF_COPY_FOLLOW,           RTGETOPT_REQ_NOTHING },
        { "--username",            'u',                             RTGETOPT_REQ_STRING  },
        { "--passwordfile",        'p',                             RTGETOPT_REQ_STRING  },
        { "--password",            GETOPTDEF_COPY_PASSWORD,         RTGETOPT_REQ_STRING  },
        { "--domain",              'd',                             RTGETOPT_REQ_STRING  },
        { "--recursive",           'R',                             RTGETOPT_REQ_NOTHING },
        { "--target-directory",    GETOPTDEF_COPY_TARGETDIR,        RTGETOPT_REQ_STRING  },
        { "--verbose",             'v',                             RTGETOPT_REQ_NOTHING }
    };

    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, pArg->argc, pArg->argv,
                 s_aOptions, RT_ELEMENTS(s_aOptions), 0, RTGETOPTINIT_FLAGS_OPTS_FIRST);

    Utf8Str strSource;
    Utf8Str strDest;
    Utf8Str strUsername;
    Utf8Str strPassword;
    Utf8Str strDomain;
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
            case GETOPTDEF_COPY_DRYRUN:
                fDryRun = true;
                break;

            case GETOPTDEF_COPY_FOLLOW:
                fFlags |= CopyFileFlag_FollowLinks;
                break;

            case 'u': /* User name */
                strUsername = ValueUnion.psz;
                break;

            case GETOPTDEF_COPY_PASSWORD: /* Password */
                strPassword = ValueUnion.psz;
                break;

            case 'p': /* Password file */
            {
                RTEXITCODE rcExit = readPasswordFile(ValueUnion.psz, &strPassword);
                if (rcExit != RTEXITCODE_SUCCESS)
                    return rcExit;
                break;
            }

            case 'd': /* domain */
                strDomain = ValueUnion.psz;
                break;

            case 'R': /* Recursive processing */
                fFlags |= CopyFileFlag_Recursive;
                break;

            case GETOPTDEF_COPY_TARGETDIR:
                strDest = ValueUnion.psz;
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
                    && strDest.isEmpty())
                {
                    strDest = ValueUnion.psz;
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

    if (strDest.isEmpty())
        return errorSyntax(USAGE_GUESTCONTROL,
                           "No destination specified!");

    if (strUsername.isEmpty())
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
                                strUsername, strPassword, strDomain,
                                "VBoxManage Guest Control Copy", &pContext);
    if (RT_FAILURE(vrc))
    {
        RTMsgError("Unable to create copy context, rc=%Rrc\n", vrc);
        return RTEXITCODE_FAILURE;
    }

    /* If the destination is a path, (try to) create it. */
    const char *pszDest = strDest.c_str();
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
                                                strDest.c_str(), &pszDestFile);
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
                                            strDest.c_str(), fFlags);
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

static int handleCtrlCreateDirectory(ComPtr<IGuest> pGuest, HandlerArg *pArg)
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
        { "--username",            'u',                             RTGETOPT_REQ_STRING  },
        { "--passwordfile",        'p',                             RTGETOPT_REQ_STRING  },
        { "--password",            GETOPTDEF_MKDIR_PASSWORD,        RTGETOPT_REQ_STRING  },
        { "--domain",              'd',                             RTGETOPT_REQ_STRING  },
        { "--verbose",             'v',                             RTGETOPT_REQ_NOTHING }
    };

    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, pArg->argc, pArg->argv,
                 s_aOptions, RT_ELEMENTS(s_aOptions), 0, RTGETOPTINIT_FLAGS_OPTS_FIRST);

    Utf8Str strUsername;
    Utf8Str strPassword;
    Utf8Str strDomain;
    SafeArray<DirectoryCreateFlag_T> dirCreateFlags;
    uint32_t fDirMode = 0; /* Default mode. */
    bool fVerbose = false;

    DESTDIRMAP mapDirs;

    while ((ch = RTGetOpt(&GetState, &ValueUnion)))
    {
        /* For options that require an argument, ValueUnion has received the value. */
        switch (ch)
        {
            case 'm': /* Mode */
                fDirMode = ValueUnion.u32;
                break;

            case 'P': /* Create parents */
                dirCreateFlags.push_back(DirectoryCreateFlag_Parents);
                break;

            case 'u': /* User name */
                strUsername = ValueUnion.psz;
                break;

            case GETOPTDEF_MKDIR_PASSWORD: /* Password */
                strPassword = ValueUnion.psz;
                break;

            case 'p': /* Password file */
            {
                RTEXITCODE rcExit = readPasswordFile(ValueUnion.psz, &strPassword);
                if (rcExit != RTEXITCODE_SUCCESS)
                    return rcExit;
                break;
            }

            case 'd': /* domain */
                strDomain = ValueUnion.psz;
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
                return RTGetOptPrintError(ch, &ValueUnion);
        }
    }

    uint32_t cDirs = mapDirs.size();
    if (!cDirs)
        return errorSyntax(USAGE_GUESTCONTROL, "No directory to create specified!");

    if (strUsername.isEmpty())
        return errorSyntax(USAGE_GUESTCONTROL, "No user name specified!");

    /*
     * Create the directories.
     */
    HRESULT hrc = S_OK;
    if (fVerbose && cDirs)
        RTPrintf("Creating %u directories ...\n", cDirs);

    ComPtr<IGuestSession> pGuestSession;
    hrc = pGuest->CreateSession(Bstr(strUsername).raw(),
                                Bstr(strPassword).raw(),
                                Bstr(strDomain).raw(),
                                Bstr("VBoxManage Guest Control MkDir").raw(),
                                pGuestSession.asOutParam());
    if (FAILED(hrc))
        return ctrlPrintError(pGuest, COM_IIDOF(IGuest));

    DESTDIRMAPITER it = mapDirs.begin();
    while (it != mapDirs.end())
    {
        if (fVerbose)
            RTPrintf("Creating directory \"%s\" ...\n", it->first.c_str());

        hrc = pGuestSession->DirectoryCreate(Bstr(it->first).raw(), fDirMode, ComSafeArrayAsInParam(dirCreateFlags));
        if (FAILED(hrc))
        {
            ctrlPrintError(pGuest, COM_IIDOF(IGuestSession)); /* Return code ignored, save original rc. */
            break;
        }

        it++;
    }

    if (!pGuestSession.isNull())
        pGuestSession->Close();

    return FAILED(hrc) ? RTEXITCODE_FAILURE : RTEXITCODE_SUCCESS;
}

static int handleCtrlCreateTemp(ComPtr<IGuest> pGuest, HandlerArg *pArg)
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
        { "--directory",           'D',                             RTGETOPT_REQ_NOTHING },
        { "--secure",              's',                             RTGETOPT_REQ_NOTHING },
        { "--tmpdir",              't',                             RTGETOPT_REQ_STRING  },
        { "--username",            'u',                             RTGETOPT_REQ_STRING  },
        { "--passwordfile",        'p',                             RTGETOPT_REQ_STRING  },
        { "--password",            GETOPTDEF_MKDIR_PASSWORD,        RTGETOPT_REQ_STRING  },
        { "--domain",              'd',                             RTGETOPT_REQ_STRING  },
        { "--verbose",             'v',                             RTGETOPT_REQ_NOTHING }
    };

    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, pArg->argc, pArg->argv,
                 s_aOptions, RT_ELEMENTS(s_aOptions), 0, RTGETOPTINIT_FLAGS_OPTS_FIRST);

    Utf8Str strUsername;
    Utf8Str strPassword;
    Utf8Str strDomain;
    Utf8Str strTemplate;
    uint32_t fMode = 0; /* Default mode. */
    bool fDirectory = false;
    bool fSecure = false;
    Utf8Str strTempDir;
    bool fVerbose = false;

    DESTDIRMAP mapDirs;

    while ((ch = RTGetOpt(&GetState, &ValueUnion)))
    {
        /* For options that require an argument, ValueUnion has received the value. */
        switch (ch)
        {
            case 'm': /* Mode */
                fMode = ValueUnion.u32;
                break;

            case 'D': /* Create directory */
                fDirectory = true;
                break;

            case 's': /* Secure */
                fSecure = true;
                break;

            case 't': /* Temp directory */
                strTempDir = ValueUnion.psz;
                break;

            case 'u': /* User name */
                strUsername = ValueUnion.psz;
                break;

            case GETOPTDEF_MKDIR_PASSWORD: /* Password */
                strPassword = ValueUnion.psz;
                break;

            case 'p': /* Password file */
            {
                RTEXITCODE rcExit = readPasswordFile(ValueUnion.psz, &strPassword);
                if (rcExit != RTEXITCODE_SUCCESS)
                    return rcExit;
                break;
            }

            case 'd': /* domain */
                strDomain = ValueUnion.psz;
                break;

            case 'v': /* Verbose */
                fVerbose = true;
                break;

            case VINF_GETOPT_NOT_OPTION:
            {
                if (strTemplate.isEmpty())
                    strTemplate = ValueUnion.psz;
                else
                    return errorSyntax(USAGE_GUESTCONTROL,
                                       "More than one template specified!\n");
                break;
            }

            default:
                return RTGetOptPrintError(ch, &ValueUnion);
        }
    }

    if (strTemplate.isEmpty())
        return errorSyntax(USAGE_GUESTCONTROL, "No template specified!");

    if (strUsername.isEmpty())
        return errorSyntax(USAGE_GUESTCONTROL, "No user name specified!");

    if (!fDirectory)
        return errorSyntax(USAGE_GUESTCONTROL, "Creating temporary files is currently not supported!");

    /*
     * Create the directories.
     */
    HRESULT hrc = S_OK;
    if (fVerbose)
    {
        if (fDirectory && !strTempDir.isEmpty())
            RTPrintf("Creating temporary directory from template '%s' in directory '%s' ...\n",
                     strTemplate.c_str(), strTempDir.c_str());
        else if (fDirectory)
            RTPrintf("Creating temporary directory from template '%s' in default temporary directory ...\n",
                     strTemplate.c_str());
        else if (!fDirectory && !strTempDir.isEmpty())
            RTPrintf("Creating temporary file from template '%s' in directory '%s' ...\n",
                     strTemplate.c_str(), strTempDir.c_str());
        else if (!fDirectory)
            RTPrintf("Creating temporary file from template '%s' in default temporary directory ...\n",
                     strTemplate.c_str());
    }

    ComPtr<IGuestSession> pGuestSession;
    hrc = pGuest->CreateSession(Bstr(strUsername).raw(),
                                Bstr(strPassword).raw(),
                                Bstr(strDomain).raw(),
                                Bstr("VBoxManage Guest Control MkTemp").raw(),
                                pGuestSession.asOutParam());
    if (FAILED(hrc))
        return ctrlPrintError(pGuest, COM_IIDOF(IGuest));

    if (fDirectory)
    {
        Bstr directory;
        hrc = pGuestSession->DirectoryCreateTemp(Bstr(strTemplate).raw(),
                                                 fMode, Bstr(strTempDir).raw(),
                                                 fSecure,
                                                 directory.asOutParam());
        if (SUCCEEDED(hrc))
            RTPrintf("Directory name: %ls\n", directory.raw());
    }
    // else - temporary file not yet implemented
    if (FAILED(hrc))
        ctrlPrintError(pGuest, COM_IIDOF(IGuestSession)); /* Return code ignored, save original rc. */

    if (!pGuestSession.isNull())
        pGuestSession->Close();

    return FAILED(hrc) ? RTEXITCODE_FAILURE : RTEXITCODE_SUCCESS;
}

static int handleCtrlStat(ComPtr<IGuest> pGuest, HandlerArg *pArg)
{
    AssertPtrReturn(pArg, VERR_INVALID_PARAMETER);

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--dereference",         'L',                             RTGETOPT_REQ_NOTHING },
        { "--file-system",         'f',                             RTGETOPT_REQ_NOTHING },
        { "--format",              'c',                             RTGETOPT_REQ_STRING },
        { "--username",            'u',                             RTGETOPT_REQ_STRING  },
        { "--passwordfile",        'p',                             RTGETOPT_REQ_STRING  },
        { "--password",            GETOPTDEF_STAT_PASSWORD,         RTGETOPT_REQ_STRING  },
        { "--domain",              'd',                             RTGETOPT_REQ_STRING  },
        { "--terse",               't',                             RTGETOPT_REQ_NOTHING },
        { "--verbose",             'v',                             RTGETOPT_REQ_NOTHING }
    };

    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, pArg->argc, pArg->argv,
                 s_aOptions, RT_ELEMENTS(s_aOptions), 0, RTGETOPTINIT_FLAGS_OPTS_FIRST);

    Utf8Str strUsername;
    Utf8Str strPassword;
    Utf8Str strDomain;

    bool fVerbose = false;
    DESTDIRMAP mapObjs;

    while ((ch = RTGetOpt(&GetState, &ValueUnion)))
    {
        /* For options that require an argument, ValueUnion has received the value. */
        switch (ch)
        {
            case 'u': /* User name */
                strUsername = ValueUnion.psz;
                break;

            case GETOPTDEF_STAT_PASSWORD: /* Password */
                strPassword = ValueUnion.psz;
                break;

            case 'p': /* Password file */
            {
                RTEXITCODE rcExit = readPasswordFile(ValueUnion.psz, &strPassword);
                if (rcExit != RTEXITCODE_SUCCESS)
                    return rcExit;
                break;
            }

            case 'd': /* domain */
                strDomain = ValueUnion.psz;
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
        }
    }

    uint32_t cObjs = mapObjs.size();
    if (!cObjs)
        return errorSyntax(USAGE_GUESTCONTROL, "No element(s) to check specified!");

    if (strUsername.isEmpty())
        return errorSyntax(USAGE_GUESTCONTROL, "No user name specified!");

    ComPtr<IGuestSession> pGuestSession;
    HRESULT hrc = pGuest->CreateSession(Bstr(strUsername).raw(),
                                        Bstr(strPassword).raw(),
                                        Bstr(strDomain).raw(),
                                        Bstr("VBoxManage Guest Control Stat").raw(),
                                        pGuestSession.asOutParam());
    if (FAILED(hrc))
        return ctrlPrintError(pGuest, COM_IIDOF(IGuest));

    /*
     * Create the directories.
     */
    RTEXITCODE rcExit = RTEXITCODE_SUCCESS;
    DESTDIRMAPITER it = mapObjs.begin();
    while (it != mapObjs.end())
    {
        if (fVerbose)
            RTPrintf("Checking for element \"%s\" ...\n", it->first.c_str());

        ComPtr<IGuestFsObjInfo> pFsObjInfo;
        hrc = pGuestSession->FileQueryInfo(Bstr(it->first).raw(), pFsObjInfo.asOutParam());
        if (FAILED(hrc))
            hrc = pGuestSession->DirectoryQueryInfo(Bstr(it->first).raw(), pFsObjInfo.asOutParam());

        if (FAILED(hrc))
        {
            /* If there's at least one element which does not exist on the guest,
             * drop out with exitcode 1. */
            if (fVerbose)
                RTPrintf("Cannot stat for element \"%s\": No such element\n",
                         it->first.c_str());
            rcExit = RTEXITCODE_FAILURE;
        }
        else
        {
            FsObjType_T objType;
            hrc = pFsObjInfo->COMGETTER(Type)(&objType);
            if (FAILED(hrc))
                return ctrlPrintError(pGuest, COM_IIDOF(IGuestFsObjInfo));
            switch (objType)
            {
                case FsObjType_File:
                    RTPrintf("Element \"%s\" found: Is a file\n", it->first.c_str());
                    break;

                case FsObjType_Directory:
                    RTPrintf("Element \"%s\" found: Is a directory\n", it->first.c_str());
                    break;

                case FsObjType_Symlink:
                    RTPrintf("Element \"%s\" found: Is a symlink\n", it->first.c_str());
                    break;

                default:
                    RTPrintf("Element \"%s\" found, type unknown (%ld)\n", it->first.c_str(), objType);
                    break;
            }

            /** @todo: Show more information about this element. */
        }

        it++;
    }

    if (!pGuestSession.isNull())
        pGuestSession->Close();

    return rcExit;
}

static int handleCtrlUpdateAdditions(ComPtr<IGuest> guest, HandlerArg *pArg)
{
    AssertPtrReturn(pArg, VERR_INVALID_PARAMETER);

    /*
     * Check the syntax.  We can deduce the correct syntax from the number of
     * arguments.
     */
    Utf8Str strSource;
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
                strSource = ValueUnion.psz;
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
    if (strSource.isEmpty())
        strSource = "c:\\Downloads\\VBoxGuestAdditions-r67158.iso";
#endif

    /* Determine source if not set yet. */
    if (strSource.isEmpty())
    {
        char strTemp[RTPATH_MAX];
        vrc = RTPathAppPrivateNoArch(strTemp, sizeof(strTemp));
        AssertRC(vrc);
        Utf8Str strSrc1 = Utf8Str(strTemp).append("/VBoxGuestAdditions.iso");

        vrc = RTPathExecDir(strTemp, sizeof(strTemp));
        AssertRC(vrc);
        Utf8Str strSrc2 = Utf8Str(strTemp).append("/additions/VBoxGuestAdditions.iso");

        /* Check the standard image locations */
        if (RTFileExists(strSrc1.c_str()))
            strSource = strSrc1;
        else if (RTFileExists(strSrc2.c_str()))
            strSource = strSrc2;
        else
        {
            RTMsgError("Source could not be determined! Please use --source to specify a valid source\n");
            vrc = VERR_FILE_NOT_FOUND;
        }
    }
    else if (!RTFileExists(strSource.c_str()))
    {
        RTMsgError("Source \"%s\" does not exist!\n", strSource.c_str());
        vrc = VERR_FILE_NOT_FOUND;
    }

    if (RT_SUCCESS(vrc))
    {
        if (fVerbose)
            RTPrintf("Using source: %s\n", strSource.c_str());

        HRESULT rc = S_OK;
        ComPtr<IProgress> pProgress;

        SafeArray<AdditionsUpdateFlag_T> updateFlags;
        CHECK_ERROR(guest, UpdateGuestAdditions(Bstr(strSource).raw(),
                                                /* Wait for whole update process to complete. */
                                                ComSafeArrayAsInParam(updateFlags),
                                                pProgress.asOutParam()));
        if (FAILED(rc))
            vrc = ctrlPrintError(guest, COM_IIDOF(IGuest));
        else
        {
            if (fVerbose)
                rc = showProgress(pProgress);
            else
                rc = pProgress->WaitForCompletion(-1 /* No timeout */);

            if (SUCCEEDED(rc))
                CHECK_PROGRESS_ERROR(pProgress, ("Guest additions update failed"));
            vrc = ctrlPrintProgressError(pProgress);
            if (   RT_SUCCESS(vrc)
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
        if (pArg->argc < 2)
            rcExit = errorSyntax(USAGE_GUESTCONTROL, "No sub command specified!");
        else if (   !strcmp(pArg->argv[1], "exec")
                 || !strcmp(pArg->argv[1], "execute"))
            rcExit = handleCtrlExecProgram(guest, &arg);
        else if (!strcmp(pArg->argv[1], "copyfrom"))
            rcExit = handleCtrlCopy(guest, &arg, false /* Guest to host */);
        else if (   !strcmp(pArg->argv[1], "copyto")
                 || !strcmp(pArg->argv[1], "cp"))
            rcExit = handleCtrlCopy(guest, &arg, true /* Host to guest */);
        else if (   !strcmp(pArg->argv[1], "createdirectory")
                 || !strcmp(pArg->argv[1], "createdir")
                 || !strcmp(pArg->argv[1], "mkdir")
                 || !strcmp(pArg->argv[1], "md"))
            rcExit = handleCtrlCreateDirectory(guest, &arg);
        else if (   !strcmp(pArg->argv[1], "createtemporary")
                 || !strcmp(pArg->argv[1], "createtemp")
                 || !strcmp(pArg->argv[1], "mktemp"))
            rcExit = handleCtrlCreateTemp(guest, &arg);
        else if (   !strcmp(pArg->argv[1], "stat"))
            rcExit = handleCtrlStat(guest, &arg);
        else if (   !strcmp(pArg->argv[1], "updateadditions")
                 || !strcmp(pArg->argv[1], "updateadds"))
            rcExit = handleCtrlUpdateAdditions(guest, &arg);
        else
            rcExit = errorSyntax(USAGE_GUESTCONTROL, "Unknown sub command '%s' specified!", pArg->argv[1]);

        ctrlUninitVM(pArg);
        return rcExit;
    }
    return RTEXITCODE_FAILURE;
}

#endif /* !VBOX_ONLY_DOCS */

