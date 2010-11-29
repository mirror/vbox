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
#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/isofs.h>
#include <iprt/getopt.h>
#include <iprt/list.h>
#include <iprt/path.h>

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

/*
 * Structure holding a directory entry.
 */
typedef struct DIRECTORYENTRY
{
    char       *pszSourcePath;
    char       *pszDestPath;
    RTLISTNODE  Node;
} DIRECTORYENTRY, *PDIRECTORYENTRY;

#endif /* VBOX_ONLY_DOCS */

void usageGuestControl(PRTSTREAM pStrm)
{
    RTStrmPrintf(pStrm,
                 "VBoxManage guestcontrol     exec[ute] <vmname>|<uuid>\n"
                 "                            <path to program>\n"
                 "                            --username <name> --password <password>\n"
                 "                            [--arguments \"<arguments>\"]\n"
                 "                            [--environment \"<NAME>=<VALUE> [<NAME>=<VALUE>]\"]\n"
                 "                            [--flags <flags>] [--timeout <msec>]\n"
                 "                            [--verbose] [--wait-for exit,stdout,stderr||]\n"
                 /** @todo Add a "--" parameter (has to be last parameter) to directly execute
                  *        stuff, e.g. "VBoxManage guestcontrol execute <VMName> --username <> ... -- /bin/rm -Rf /foo". */
                 "\n"
                 "                            copyto|cp <vmname>|<uuid>\n"
                 "                            <source on host> <destination on guest>\n"
                 "                            --username <name> --password <password>\n"
                 "                            [--dryrun] [--recursive] [--verbose] [--flags <flags>]\n"
                 "\n"
                 "                            createdir[ectory]|mkdir|md <vmname>|<uuid>\n"
                 "                            <directory to create on guest>\n"
                 "                            --username <name> --password <password>\n"
                 "                            [--parents] [--mode <mode>]\n"
                 "\n"
                 "                            updateadditions <vmname>|<uuid>\n"
                 "                            [--source <guest additions .ISO file to use>] [--verbose]\n"
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

static int ctrlPrintError(ComPtr<IUnknown> object, const GUID &aIID)
{
    com::ErrorInfo info(object, aIID);
    if (   info.isFullAvailable()
        || info.isBasicAvailable())
    {
        /* If we got a VBOX_E_IPRT error we handle the error in a more gentle way
         * because it contains more accurate info about what went wrong. */
        if (info.getResultCode() == VBOX_E_IPRT_ERROR)
            RTMsgError("%ls.", info.getText().raw());
        else
        {
            RTMsgError("Error details:");
            GluePrintErrorInfo(info);
        }
        return VERR_GENERAL_FAILURE; /** @todo */
    }
    AssertMsgFailedReturn(("Object has indicated no error!?\n"),
                          VERR_INVALID_PARAMETER);
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
 * Initializes the VM, that is checks whether it's up and
 * running, if it can be locked (shared only) and returns a
 * valid IGuest pointer on success.
 *
 * @return  IPRT status code.
 * @param   pArg            Our command line argument structure.
 * @param   pszNameOrId     The VM's name or UUID to use.
 * @param   pGuest          Pointer where to store the IGuest interface.
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
        RTMsgError("Machine \"%s\" is not running!\n", pszNameOrId);
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

static int handleCtrlExecProgram(HandlerArg *a)
{
    /*
     * Check the syntax.  We can deduce the correct syntax from the number of
     * arguments.
     */
    if (a->argc < 2) /* At least the command we want to execute in the guest should be present :-). */
        return errorSyntax(USAGE_GUESTCONTROL, "Incorrect parameters");

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--arguments",           'a',         RTGETOPT_REQ_STRING  },
        { "--environment",         'e',         RTGETOPT_REQ_STRING  },
        { "--flags",               'f',         RTGETOPT_REQ_STRING  },
        { "--password",            'p',         RTGETOPT_REQ_STRING  },
        { "--timeout",             't',         RTGETOPT_REQ_UINT32  },
        { "--username",            'u',         RTGETOPT_REQ_STRING  },
        { "--verbose",             'v',         RTGETOPT_REQ_NOTHING },
        { "--wait-for",            'w',         RTGETOPT_REQ_STRING  }
    };

    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, 0);

    Utf8Str Utf8Cmd;
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

    int vrc = VINF_SUCCESS;
    bool fUsageOK = true;
    while (   (ch = RTGetOpt(&GetState, &ValueUnion))
           && RT_SUCCESS(vrc))
    {
        /* For options that require an argument, ValueUnion has received the value. */
        switch (ch)
        {
            case 'a': /* Arguments */
            {
                char **papszArg;
                int cArgs;

                vrc = RTGetOptArgvFromString(&papszArg, &cArgs, ValueUnion.psz, NULL);
                if (RT_SUCCESS(vrc))
                {
                    for (int j = 0; j < cArgs; j++)
                        args.push_back(Bstr(papszArg[j]).raw());

                    RTGetOptArgvFree(papszArg);
                }
                break;
            }

            case 'e': /* Environment */
            {
                char **papszArg;
                int cArgs;

                vrc = RTGetOptArgvFromString(&papszArg, &cArgs, ValueUnion.psz, NULL);
                if (RT_SUCCESS(vrc))
                {
                    for (int j = 0; j < cArgs; j++)
                        env.push_back(Bstr(papszArg[j]).raw());

                    RTGetOptArgvFree(papszArg);
                }
                break;
            }

            case 'f': /* Flags */
                /** @todo Needs a bit better processing as soon as we have more flags. */
                if (!RTStrICmp(ValueUnion.psz, "ignoreorphanedprocesses"))
                    uFlags |= ExecuteProcessFlag_IgnoreOrphanedProcesses;
                else
                    fUsageOK = false;
                break;

            case 'p': /* Password */
                Utf8Password = ValueUnion.psz;
                break;

            case 't': /* Timeout */
                u32TimeoutMS = ValueUnion.u32;
                fTimeout = true;
                break;

            case 'u': /* User name */
                Utf8UserName = ValueUnion.psz;
                break;

            case 'v': /* Verbose */
                fVerbose = true;
                break;

            case 'w': /* Wait for ... */
            {
                if (!RTStrICmp(ValueUnion.psz, "exit"))
                    fWaitForExit = true;
                else if (!RTStrICmp(ValueUnion.psz, "stdout"))
                {
                    fWaitForExit = true;
                    fWaitForStdOut = true;
                }
                else if (!RTStrICmp(ValueUnion.psz, "stderr"))
                {
                    fWaitForExit = true;
                    fWaitForStdErr = true;
                }
                else
                    fUsageOK = false;
                break;
            }

            case VINF_GETOPT_NOT_OPTION:
            {
                /* The actual command we want to execute on the guest. */
                Utf8Cmd = ValueUnion.psz;
                break;
            }

            default:
                return RTGetOptPrintError(ch, &ValueUnion);
        }
    }

    if (!fUsageOK)
        return errorSyntax(USAGE_GUESTCONTROL, "Incorrect parameters");

    if (Utf8Cmd.isEmpty())
        return errorSyntax(USAGE_GUESTCONTROL,
                           "No command to execute specified!");

    if (Utf8UserName.isEmpty())
        return errorSyntax(USAGE_GUESTCONTROL,
                           "No user name specified!");

    HRESULT rc = S_OK;
    ComPtr<IGuest> guest;
    vrc = ctrlInitVM(a, a->argv[0] /* VM Name */, &guest);
    if (RT_SUCCESS(vrc))
    {
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
            vrc = ctrlPrintError(guest, COM_IIDOF(IGuest));
        }
        else
        {
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
                            vrc = ctrlPrintError(guest, COM_IIDOF(IGuest));

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
                    {
                        break;
                    }
                }

                /* Undo signal handling */
                if (fCancelable)
                    ctrlSignalHandlerUninstall();

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
                        vrc = ctrlPrintError(progress, COM_IIDOF(IProgress));
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
        }
        ctrlUninitVM(a);
    }

    if (RT_FAILURE(vrc))
        rc = VBOX_E_IPRT_ERROR;
    return SUCCEEDED(rc) ? 0 : 1;
}

/**
 * Appends a new to-copy object to a copy list.
 *
 * @return  IPRT status code.
 * @param   pszFileSource       Full qualified source path of file to copy.
 * @param   pszFileDest         Full qualified destination path.
 * @param   pList               Copy list used for insertion.
 */
static int ctrlCopyDirectoryEntryAppend(const char *pszFileSource, const char *pszFileDest,
                                        PRTLISTNODE pList)
{
    AssertPtrReturn(pszFileSource, VERR_INVALID_POINTER);
    AssertPtrReturn(pszFileDest, VERR_INVALID_POINTER);
    AssertPtrReturn(pList, VERR_INVALID_POINTER);

    PDIRECTORYENTRY pNode = (PDIRECTORYENTRY)RTMemAlloc(sizeof(DIRECTORYENTRY));
    if (pNode == NULL)
        return VERR_NO_MEMORY;

    pNode->pszSourcePath = RTStrDup(pszFileSource);
    pNode->pszDestPath = RTStrDup(pszFileDest);
    if (   !pNode->pszSourcePath
        || !pNode->pszDestPath)
    {
        return VERR_NO_MEMORY;
    }

    pNode->Node.pPrev = NULL;
    pNode->Node.pNext = NULL;
    RTListAppend(pList, &pNode->Node);
    return VINF_SUCCESS;
}

/**
 * Reads a specified directory (recursively) based on the copy flags
 * and appends all matching entries to the supplied list.
 *
 * @return  IPRT status code.
 * @param   pszRootDir          Directory to start with. Must end with
 *                              a trailing slash and must be absolute.
 * @param   pszSubDir           Sub directory part relative to the root
 *                              directory; needed for recursion.
 * @param   pszFilter           Search filter (e.g. *.pdf).
 * @param   pszDest             Destination directory.
 * @param   uFlags              Copy flags.
 * @param   pcObjects           Where to store the overall objects to
 *                              copy found.
 * @param   pList               Pointer to the object list to use.
 */
static int ctrlCopyDirectoryRead(const char *pszRootDir, const char *pszSubDir,
                                 const char *pszFilter, const char *pszDest,
                                 uint32_t uFlags, uint32_t *pcObjects, PRTLISTNODE pList)
{
    AssertPtrReturn(pszRootDir, VERR_INVALID_POINTER);
    /* Sub directory is optional. */
    /* Filter directory is optional. */
    AssertPtrReturn(pszDest, VERR_INVALID_POINTER);
    AssertPtrReturn(pcObjects, VERR_INVALID_POINTER);
    AssertPtrReturn(pList, VERR_INVALID_POINTER);

    PRTDIR pDir = NULL;

    int rc = VINF_SUCCESS;
    char szCurDir[RTPATH_MAX];
    /* Construct current path. */
    if (RTStrPrintf(szCurDir, sizeof(szCurDir), pszRootDir))
    {
        if (pszSubDir != NULL)
            rc = RTPathAppend(szCurDir, sizeof(szCurDir), pszSubDir);
    }
    else
        rc = VERR_NO_MEMORY;

    if (RT_SUCCESS(rc))
    {
        /* Open directory without a filter - RTDirOpenFiltered unfortunately
         * cannot handle sub directories so we have to do the filtering ourselves. */
        rc = RTDirOpen(&pDir, szCurDir);
        for (;RT_SUCCESS(rc);)
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
                    /* Skip "." and ".." entrires. */
                    if (   !strcmp(DirEntry.szName, ".")
                        || !strcmp(DirEntry.szName, ".."))
                    {
                        break;
                    }
                    if (uFlags & CopyFileFlag_Recursive)
                    {
                        char *pszNewSub = NULL;
                        if (pszSubDir)
                            RTStrAPrintf(&pszNewSub, "%s%s/", pszSubDir, DirEntry.szName);
                        else
                            RTStrAPrintf(&pszNewSub, "%s/", DirEntry.szName);

                        if (pszNewSub)
                        {
                            rc = ctrlCopyDirectoryRead(pszRootDir, pszNewSub,
                                                       pszFilter, pszDest,
                                                       uFlags, pcObjects, pList);
                            RTStrFree(pszNewSub);
                        }
                        else
                            rc = VERR_NO_MEMORY;
                    }
                    break;

                case RTDIRENTRYTYPE_SYMLINK:
                    if (   (uFlags & CopyFileFlag_Recursive)
                        && (uFlags & CopyFileFlag_FollowLinks))
                    {
                        /* Fall through to next case is intentional. */
                    }
                    else
                        break;

                case RTDIRENTRYTYPE_FILE:
                {
                    bool fProcess = false;
                    if (pszFilter && RTStrSimplePatternMatch(pszFilter, DirEntry.szName))
                        fProcess = true;
                    else if (!pszFilter)
                        fProcess = true;

                    if (fProcess)
                    {
                        char *pszFileSource = NULL;
                        char *pszFileDest = NULL;
                        if (RTStrAPrintf(&pszFileSource, "%s%s%s",
                                         pszRootDir, pszSubDir ? pszSubDir : "",
                                         DirEntry.szName) >= 0)
                        {
                            if (RTStrAPrintf(&pszFileDest, "%s%s%s",
                                             pszDest, pszSubDir ? pszSubDir : "",
                                             DirEntry.szName) <= 0)
                            {
                                rc = VERR_NO_MEMORY;
                            }
                        }
                        else
                            rc = VERR_NO_MEMORY;

                        if (RT_SUCCESS(rc))
                        {
                            rc = ctrlCopyDirectoryEntryAppend(pszFileSource, pszFileDest, pList);
                            if (RT_SUCCESS(rc))
                                *pcObjects = *pcObjects + 1;
                        }

                        if (pszFileSource)
                            RTStrFree(pszFileSource);
                        if (pszFileDest)
                            RTStrFree(pszFileDest);
                    }
                }
                break;

                default:
                    break;
            }
            if (RT_FAILURE(rc))
                break;
        }
    }

    if (pDir)
        RTDirClose(pDir);
    return rc;
}

/**
 * Initializes the copy process and builds up an object list
 * with all required information to start the actual copy process.
 *
 * @return  IPRT status code.
 * @param   pszSource           Source path on host to use.
 * @param   pszDest             Destination path on guest to use.
 * @param   uFlags              Copy flags.
 * @param   pcObjects           Where to store the count of objects to be copied.
 * @param   pList               Where to store the object list.
 */
static int ctrlCopyInit(const char *pszSource, const char *pszDest, uint32_t uFlags,
                        uint32_t *pcObjects, PRTLISTNODE pList)
{
    AssertPtrReturn(pszSource, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszDest, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcObjects, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pList, VERR_INVALID_PARAMETER);

    int rc = VINF_SUCCESS;
    char *pszSourceAbs = RTPathAbsDup(pszSource);
    if (pszSourceAbs)
    {
        if (   RTPathFilename(pszSourceAbs)
            && RTFileExists(pszSourceAbs)) /* We have a single file ... */
        {
            char *pszDestAbs = RTStrDup(pszDest);
            if (pszDestAbs)
            {
                /* Do we have a trailing slash for the destination?
                 * Then this is a directory ... */
                size_t cch = strlen(pszDestAbs);
                if (    cch > 1
                    && (   RTPATH_IS_SLASH(pszDestAbs[cch - 1])
                        || RTPATH_IS_SLASH(pszDestAbs[cch - 2])
                       )
                   )
                {
                    rc = RTStrAAppend(&pszDestAbs, RTPathFilename(pszSourceAbs));
                }
                else
                {
                    /* Since the desetination seems not to be a directory,
                     * we assume that this is the absolute path to the destination
                     * file -> nothing to do here ... */
                }

                if (RT_SUCCESS(rc))
                {
                    RTListInit(pList);
                    rc = ctrlCopyDirectoryEntryAppend(pszSourceAbs, pszDestAbs, pList);
                    *pcObjects = 1;
                }
                RTStrFree(pszDestAbs);
            }
            else
                rc = VERR_NO_MEMORY;
        }
        else /* ... or a directory. */
        {
            /* Append trailing slash to absolute directory. */
            if (RTDirExists(pszSourceAbs))
                RTStrAAppend(&pszSourceAbs, RTPATH_SLASH_STR);

            /* Extract directory filter (e.g. "*.exe"). */
            char *pszFilter = RTPathFilename(pszSourceAbs);
            char *pszSourceAbsRoot = RTStrDup(pszSourceAbs);
            char *pszDestAbs = RTStrDup(pszDest);
            if (   pszSourceAbsRoot
                && pszDestAbs)
            {
                if (pszFilter)
                {
                    RTPathStripFilename(pszSourceAbsRoot);
                    rc = RTStrAAppend(&pszSourceAbsRoot, RTPATH_SLASH_STR);
                }
                else
                {
                    /*
                     * If we have more than one file to copy, make sure that we have
                     * a trailing slash so that we can construct a full path name
                     * (e.g. "foo.txt" -> "c:/foo/temp.txt") as destination.
                     */
                    size_t cch = strlen(pszSourceAbsRoot);
                    if (    cch > 1
                        &&  !RTPATH_IS_SLASH(pszSourceAbsRoot[cch - 1])
                        &&  !RTPATH_IS_SLASH(pszSourceAbsRoot[cch - 2]))
                    {
                        rc = RTStrAAppend(&pszSourceAbsRoot, RTPATH_SLASH_STR);
                    }
                }

                if (RT_SUCCESS(rc))
                {
                    /*
                     * Make sure we have a valid destination path. All we can do
                     * here is to check whether we have a trailing slash -- the rest
                     * (i.e. path creation, rights etc.) needs to be done inside the guest.
                     */
                    size_t cch = strlen(pszDestAbs);
                    if (    cch > 1
                        &&  !RTPATH_IS_SLASH(pszDestAbs[cch - 1])
                        &&  !RTPATH_IS_SLASH(pszDestAbs[cch - 2]))
                    {
                        rc = RTStrAAppend(&pszDestAbs, RTPATH_SLASH_STR);
                    }
                }

                if (RT_SUCCESS(rc))
                {
                    RTListInit(pList);
                    rc = ctrlCopyDirectoryRead(pszSourceAbsRoot, NULL /* Sub directory */,
                                               pszFilter, pszDestAbs,
                                               uFlags, pcObjects, pList);
                    if (RT_SUCCESS(rc) && *pcObjects == 0)
                        rc = VERR_NOT_FOUND;
                }

                if (pszDestAbs)
                    RTStrFree(pszDestAbs);
                if (pszSourceAbsRoot)
                    RTStrFree(pszSourceAbsRoot);
            }
            else
                rc = VERR_NO_MEMORY;
        }
        RTStrFree(pszSourceAbs);
    }
    else
        rc = VERR_NO_MEMORY;
    return rc;
}

/**
 * Destroys a copy list.
 */
static void ctrlCopyDestroy(PRTLISTNODE pList)
{
    AssertPtr(pList);

    /* Destroy file list. */
    PDIRECTORYENTRY pNode = RTListGetFirst(pList, DIRECTORYENTRY, Node);
    while (pNode)
    {
        PDIRECTORYENTRY pNext = RTListNodeGetNext(&pNode->Node, DIRECTORYENTRY, Node);
        bool fLast = RTListNodeIsLast(pList, &pNode->Node);

        if (pNode->pszSourcePath)
            RTStrFree(pNode->pszSourcePath);
        if (pNode->pszDestPath)
            RTStrFree(pNode->pszDestPath);
        RTListNodeRemove(&pNode->Node);
        RTMemFree(pNode);

        if (fLast)
            break;

        pNode = pNext;
    }
}

/**
 * Copys a file from host to the guest.
 *
 * @return  IPRT status code.
 * @param   pGuest          IGuest interface pointer.
 * @param   pszSource       Source path of existing host file to copy.
 * @param   pszDest         Destination path on guest to copy the file to.
 * @param   pszUserName     User name on guest to use for the copy operation.
 * @param   pszPassword     Password of user account.
 * @param   uFlags          Copy flags.
 */
static int ctrlCopyFileToGuest(IGuest *pGuest, const char *pszSource, const char *pszDest,
                               const char *pszUserName, const char *pszPassword,
                               uint32_t uFlags)
{
    AssertPtrReturn(pszSource, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszDest, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszUserName, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszPassword, VERR_INVALID_PARAMETER);

    int vrc = VINF_SUCCESS;
    ComPtr<IProgress> progress;
    HRESULT rc = pGuest->CopyToGuest(Bstr(pszSource).raw(), Bstr(pszDest).raw(),
                                     Bstr(pszUserName).raw(), Bstr(pszPassword).raw(),
                                     uFlags, progress.asOutParam());
    if (FAILED(rc))
    {
        vrc = ctrlPrintError(pGuest, COM_IIDOF(IGuest));
    }
    else
    {
        /* Setup signal handling if cancelable. */
        ASSERT(progress);
        bool fCanceledAlready = false;
        BOOL fCancelable = FALSE;
        HRESULT hrc = progress->COMGETTER(Cancelable)(&fCancelable);
        if (fCancelable)
            ctrlSignalHandlerInstall();

        /* Wait for process to exit ... */
        BOOL fCompleted = FALSE;
        BOOL fCanceled = FALSE;
        while (   SUCCEEDED(progress->COMGETTER(Completed(&fCompleted)))
               && !fCompleted)
        {
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
            {
                break;
            }
        }

        /* Undo signal handling. */
        if (fCancelable)
            ctrlSignalHandlerUninstall();

        if (fCanceled)
        {
            /* Nothing to do here right now. */
        }
        else if (   fCompleted
                 && SUCCEEDED(rc))
        {
            LONG iRc = false;
            CHECK_ERROR_RET(progress, COMGETTER(ResultCode)(&iRc), rc);
            if (FAILED(iRc))
                vrc = ctrlPrintError(progress, COM_IIDOF(IProgress));
        }
    }
    return vrc;
}

static int handleCtrlCopyTo(HandlerArg *a)
{
    /*
     * Check the syntax.  We can deduce the correct syntax from the number of
     * arguments.
     */
    if (a->argc < 3) /* At least the source + destination should be present :-). */
        return errorSyntax(USAGE_GUESTCONTROL, "Incorrect parameters");

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--dryrun",              'd',         RTGETOPT_REQ_NOTHING },
        //{ "--flags",               'f',         RTGETOPT_REQ_STRING  },
        { "--follow",              'F',         RTGETOPT_REQ_NOTHING },
        { "--password",            'p',         RTGETOPT_REQ_STRING  },
        { "--recursive",           'R',         RTGETOPT_REQ_NOTHING },
        { "--update",              'U',         RTGETOPT_REQ_NOTHING },
        { "--username",            'u',         RTGETOPT_REQ_STRING  },
        { "--verbose",             'v',         RTGETOPT_REQ_NOTHING }
    };

    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, 0);

    Utf8Str Utf8Source;
    Utf8Str Utf8Dest;
    Utf8Str Utf8UserName;
    Utf8Str Utf8Password;
    uint32_t uFlags = CopyFileFlag_None;
    bool fVerbose = false;
    bool fCopyRecursive = false;
    bool fDryRun = false;

    int vrc = VINF_SUCCESS;
    uint32_t uNoOptionIdx = 0;
    bool fUsageOK = true;
    while (   (ch = RTGetOpt(&GetState, &ValueUnion))
           && RT_SUCCESS(vrc))
    {
        /* For options that require an argument, ValueUnion has received the value. */
        switch (ch)
        {
            case 'd': /* Dry run */
                fDryRun = true;
                break;

            case 'f': /* Flags */
                /* Nothing to do here yet. */
                break;

            case 'F': /* Follow symlinks */
                uFlags |= CopyFileFlag_FollowLinks;
                break;

            case 'p': /* Password */
                Utf8Password = ValueUnion.psz;
                break;

            case 'R': /* Recursive processing */
                uFlags |= CopyFileFlag_Recursive;
                break;

            case 'U': /* Only update newer files */
                uFlags |= CopyFileFlag_Update;
                break;

            case 'u': /* User name */
                Utf8UserName = ValueUnion.psz;
                break;

            case 'v': /* Verbose */
                fVerbose = true;
                break;

            case VINF_GETOPT_NOT_OPTION:
            {
                /* Get the actual source + destination. */
                switch (uNoOptionIdx)
                {
                    case 0:
                        Utf8Source = ValueUnion.psz;
                        break;

                    case 1:
                        Utf8Dest = ValueUnion.psz;
                        break;

                    default:
                        break;
                }
                uNoOptionIdx++;
                break;
            }

            default:
                return RTGetOptPrintError(ch, &ValueUnion);
        }
    }

    if (!fUsageOK)
        return errorSyntax(USAGE_GUESTCONTROL, "Incorrect parameters");

    if (Utf8Source.isEmpty())
        return errorSyntax(USAGE_GUESTCONTROL,
                           "No source specified!");

    if (Utf8Dest.isEmpty())
        return errorSyntax(USAGE_GUESTCONTROL,
                           "No destination specified!");

    if (Utf8UserName.isEmpty())
        return errorSyntax(USAGE_GUESTCONTROL,
                           "No user name specified!");
    HRESULT rc = S_OK;
    ComPtr<IGuest> guest;
    vrc = ctrlInitVM(a, a->argv[0] /* VM Name */, &guest);
    if (RT_SUCCESS(vrc))
    {
        if (fVerbose)
        {
            if (fDryRun)
                RTPrintf("Dry run - no files copied!\n");
            RTPrintf("Gathering file information ...\n");
        }

        RTLISTNODE listToCopy;
        uint32_t cObjects = 0;
        vrc = ctrlCopyInit(Utf8Source.c_str(), Utf8Dest.c_str(), uFlags,
                           &cObjects, &listToCopy);
        if (RT_FAILURE(vrc))
        {
            switch (vrc)
            {
                case VERR_NOT_FOUND:
                    RTMsgError("No files to copy found!\n");
                    break;

                case VERR_FILE_NOT_FOUND:
                    RTMsgError("Source path \"%s\" not found!\n", Utf8Source.c_str());
                    break;

                default:
                    RTMsgError("Failed to initialize, rc=%Rrc\n", vrc);
                    break;
            }
        }
        else
        {
            if (RT_SUCCESS(vrc) && fVerbose)
            {
                if (fCopyRecursive)
                    RTPrintf("Recursively copying \"%s\" to \"%s\" (%u file(s)) ...\n",
                             Utf8Source.c_str(), Utf8Dest.c_str(), cObjects);
                else
                    RTPrintf("Copying \"%s\" to \"%s\" (%u file(s)) ...\n",
                             Utf8Source.c_str(), Utf8Dest.c_str(), cObjects);
            }

            if (RT_SUCCESS(vrc))
            {
                PDIRECTORYENTRY pNode;
                uint32_t uCurObject = 1;
                RTListForEach(&listToCopy, pNode, DIRECTORYENTRY, Node)
                {
                    char *pszDestPath = RTStrDup(pNode->pszDestPath);
                    AssertPtrBreak(pszDestPath);
                    RTPathStripFilename(pszDestPath);

                    if (fVerbose)
                        RTPrintf("Preparing guest directory \"%s\" ...\n", pszDestPath);

                    ComPtr<IProgress> progressDir;
                    if (!fDryRun)
                        rc = guest->CreateDirectory(Bstr(pszDestPath).raw(),
                                                    Bstr(Utf8UserName).raw(), Bstr(Utf8Password).raw(),
                                                    0 /* Creation mode */, CreateDirectoryFlag_Parents,
                                                    progressDir.asOutParam());
                    RTStrFree(pszDestPath);
                    if (FAILED(rc))
                        vrc = ctrlPrintError(guest, COM_IIDOF(IGuest));
                    else
                    {
                        if (fVerbose)
                    #ifndef DEBUG
                            RTPrintf("Copying \"%s\" (%u/%u) ...\n",
                                     pNode->pszSourcePath, uCurObject, cObjects);
                    #else
                            RTPrintf("Copying \"%s\" to \"%s\" (%u/%u) ...\n",
                                     pNode->pszSourcePath, pNode->pszDestPath, uCurObject, cObjects);
                    #endif
                        /* Finally copy the desired file (if no dry run selected). */
                        if (!fDryRun)
                            vrc = ctrlCopyFileToGuest(guest, pNode->pszSourcePath, pNode->pszDestPath,
                                                      Utf8UserName.c_str(), Utf8Password.c_str(), uFlags);
                    }
                    if (RT_FAILURE(vrc))
                        break;
                    uCurObject++;
                }
                if (RT_SUCCESS(vrc) && fVerbose)
                    RTPrintf("Copy operation successful!\n");
            }
            ctrlCopyDestroy(&listToCopy);
        }
        ctrlUninitVM(a);
    }

    if (RT_FAILURE(vrc))
        rc = VBOX_E_IPRT_ERROR;
    return SUCCEEDED(rc) ? 0 : 1;
}

static int handleCtrlCreateDirectory(HandlerArg *a)
{
    /*
     * Check the syntax.  We can deduce the correct syntax from the number of
     * arguments.
     */
    if (a->argc < 2) /* At least the directory we want to create should be present :-). */
        return errorSyntax(USAGE_GUESTCONTROL, "Incorrect parameters");

    static const RTGETOPTDEF s_aOptions[] =
    {
        //{ "--flags",               'f',         RTGETOPT_REQ_STRING  },
        { "--mode",                'm',         RTGETOPT_REQ_UINT32  },
        { "--parents",             'P',         RTGETOPT_REQ_NOTHING },
        { "--password",            'p',         RTGETOPT_REQ_STRING  },
        { "--username",            'u',         RTGETOPT_REQ_STRING  },
        { "--verbose",             'v',         RTGETOPT_REQ_NOTHING }
    };

    int ch;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), 1, 0);

    Utf8Str Utf8Directory(a->argv[1]);
    Utf8Str Utf8UserName;
    Utf8Str Utf8Password;
    uint32_t uFlags = CreateDirectoryFlag_None;
    uint32_t uMode = 0;
    bool fVerbose = false;

    int vrc = VINF_SUCCESS;
    bool fUsageOK = true;
    while (   (ch = RTGetOpt(&GetState, &ValueUnion))
           && RT_SUCCESS(vrc))
    {
        /* For options that require an argument, ValueUnion has received the value. */
        switch (ch)
        {
            case 'f': /* Flags */
                /* Nothing to do here yet. */
                break;

            case 'm': /* Mode */
                uMode = ValueUnion.u32;
                break;

            case 'P': /* Create parents */
                uFlags |= CreateDirectoryFlag_Parents;
                break;

            case 'p': /* Password */
                Utf8Password = ValueUnion.psz;
                break;

            case 'u': /* User name */
                Utf8UserName = ValueUnion.psz;
                break;

            case 'v': /* Verbose */
                fVerbose = true;
                break;

            case VINF_GETOPT_NOT_OPTION:
            {
                Utf8Directory = ValueUnion.psz;
                break;
            }

            default:
                return RTGetOptPrintError(ch, &ValueUnion);
        }
    }

    if (!fUsageOK)
        return errorSyntax(USAGE_GUESTCONTROL, "Incorrect parameters");

    if (Utf8Directory.isEmpty())
        return errorSyntax(USAGE_GUESTCONTROL,
                           "No directory to create specified!");

    if (Utf8UserName.isEmpty())
        return errorSyntax(USAGE_GUESTCONTROL,
                           "No user name specified!");

    HRESULT rc = S_OK;
    ComPtr<IGuest> guest;
    vrc = ctrlInitVM(a, a->argv[0] /* VM Name */, &guest);
    if (RT_SUCCESS(vrc))
    {
        ComPtr<IProgress> progress;
        rc = guest->CreateDirectory(Bstr(Utf8Directory).raw(),
                                    Bstr(Utf8UserName).raw(), Bstr(Utf8Password).raw(),
                                    uMode, uFlags, progress.asOutParam());
        if (FAILED(rc))
            vrc = ctrlPrintError(guest, COM_IIDOF(IGuest));
        else
        {
            /* Setup signal handling if cancelable. */
            ASSERT(progress);
            bool fCanceledAlready = false;
            BOOL fCancelable;
            rc = progress->COMGETTER(Cancelable)(&fCancelable);
            if (FAILED(rc))
                fCancelable = FALSE;
            if (fCancelable)
                ctrlSignalHandlerInstall();

            /* Wait for process to exit ... */
            BOOL fCompleted = FALSE;
            BOOL fCanceled = FALSE;
            while (   SUCCEEDED(progress->COMGETTER(Completed(&fCompleted)))
                   && !fCompleted)
            {
                /* Process async cancelation */
                if (g_fGuestCtrlCanceled && !fCanceledAlready)
                {
                    rc = progress->Cancel();
                    if (SUCCEEDED(rc))
                        fCanceledAlready = TRUE;
                    else
                        g_fGuestCtrlCanceled = false;
                }

                /* Progress canceled by Main API? */
                if (   SUCCEEDED(progress->COMGETTER(Canceled(&fCanceled)))
                    && fCanceled)
                {
                    break;
                }
            }

            /* Undo signal handling. */
            if (fCancelable)
                ctrlSignalHandlerUninstall();

            if (fCanceled)
            {
                //RTPrintf("Copy operation canceled!\n");
            }
            else if (   fCompleted
                     && SUCCEEDED(rc))
            {
                LONG iRc = false;
                CHECK_ERROR_RET(progress, COMGETTER(ResultCode)(&iRc), rc);
                if (FAILED(iRc))
                    vrc = ctrlPrintError(progress, COM_IIDOF(IProgress));
            }
        }
        ctrlUninitVM(a);
    }

    if (RT_FAILURE(vrc))
        rc = VBOX_E_IPRT_ERROR;
    return SUCCEEDED(rc) ? 0 : 1;
}

static int handleCtrlUpdateAdditions(HandlerArg *a)
{
    /*
     * Check the syntax.  We can deduce the correct syntax from the number of
     * arguments.
     */
    if (a->argc < 1) /* At least the VM name should be present :-). */
        return errorSyntax(USAGE_GUESTCONTROL, "Incorrect parameters");

    Utf8Str Utf8Source;
    bool fVerbose = false;

/** @todo r=bird: Use RTGetOpt here, no new code using strcmp-if-switching!  */
    /* Iterate through all possible commands (if available). */
    bool usageOK = true;
    for (int i = 1; usageOK && i < a->argc; i++)
    {
        if (!strcmp(a->argv[i], "--source"))
        {
            if (i + 1 >= a->argc)
                usageOK = false;
            else
            {
                Utf8Source = a->argv[i + 1];
                ++i;
            }
        }
        else if (!strcmp(a->argv[i], "--verbose"))
            fVerbose = true;
        else
            return errorSyntax(USAGE_GUESTCONTROL,
                               "Invalid parameter '%s'", Utf8Str(a->argv[i]).c_str());
    }

    if (!usageOK)
        return errorSyntax(USAGE_GUESTCONTROL, "Incorrect parameters");

    HRESULT rc = S_OK;
    ComPtr<IGuest> guest;
    int vrc = ctrlInitVM(a, a->argv[0] /* VM Name */, &guest);
    if (RT_SUCCESS(vrc))
    {
        if (fVerbose)
            RTPrintf("Updating Guest Additions of machine \"%s\" ...\n", a->argv[0]);

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

            ComPtr<IProgress> progress;
            CHECK_ERROR(guest, UpdateGuestAdditions(Bstr(Utf8Source).raw(),
                                                    AdditionsUpdateFlag_None,
                                                    progress.asOutParam()));
            if (FAILED(rc))
                vrc = ctrlPrintError(guest, COM_IIDOF(IGuest));
            else
            {
                rc = showProgress(progress);
                if (FAILED(rc))
                    vrc = ctrlPrintError(progress, COM_IIDOF(IProgress));
                else if (fVerbose)
                    RTPrintf("Guest Additions installer successfully copied and started.\n");
            }
        }
        ctrlUninitVM(a);
    }

    if (RT_FAILURE(vrc))
        rc = VBOX_E_IPRT_ERROR;
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
    if (   !RTStrICmp(a->argv[0], "exec")
        || !RTStrICmp(a->argv[0], "execute"))
    {
        return handleCtrlExecProgram(&arg);
    }
    else if (   !RTStrICmp(a->argv[0], "copyto")
             || !RTStrICmp(a->argv[0], "cp"))
    {
        return handleCtrlCopyTo(&arg);
    }
    else if (   !RTStrICmp(a->argv[0], "createdirectory")
             || !RTStrICmp(a->argv[0], "createdir")
             || !RTStrICmp(a->argv[0], "mkdir")
             || !RTStrICmp(a->argv[0], "md"))
    {
        return handleCtrlCreateDirectory(&arg);
    }
    else if (   !RTStrICmp(a->argv[0], "updateadditions")
             || !RTStrICmp(a->argv[0], "updateadds"))
    {
        return handleCtrlUpdateAdditions(&arg);
    }

    /* default: */
    return errorSyntax(USAGE_GUESTCONTROL, "Incorrect parameters");
}

#endif /* !VBOX_ONLY_DOCS */
