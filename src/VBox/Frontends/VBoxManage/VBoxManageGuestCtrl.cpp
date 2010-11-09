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
static volatile bool    g_fExecCanceled = false;
static volatile bool    g_fCopyCanceled = false;

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
                 "VBoxManage guestcontrol     execute <vmname>|<uuid>\n"
                 "                            <path to program>\n"
                 "                            --username <name> --password <password>\n"
                 "                            [--arguments \"<arguments>\"]\n"
                 "                            [--environment \"<NAME>=<VALUE> [<NAME>=<VALUE>]\"]\n"
                 "                            [--flags <flags>] [--timeout <msec>]\n"
                 "                            [--verbose] [--wait-for exit,stdout,stderr||]\n"
                 /** @todo Add a "--" parameter (has to be last parameter) to directly execute
                  *        stuff, e.g. "VBoxManage guestcontrol execute <VMName> --username <> ... -- /bin/rm -Rf /foo". */
                 "\n"
                 "                            copyto <vmname>|<uuid>\n"
                 "                            <source on host> <destination on guest>\n"
                 "                            --username <name> --password <password>\n"
                 "                            [--dryrun] [--recursive] [--verbose] [--flags <flags>]\n"
                 "\n"
                 "                            createdirectory <vmname>|<uuid>\n"
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

    /* Lookup VM. */
    ComPtr<IMachine> machine;
    /* Assume it's an UUID. */
    HRESULT rc;
    CHECK_ERROR(a->virtualBox, FindMachine(Bstr(a->argv[0]).raw(),
                                           machine.asOutParam()));
    if (FAILED(rc))
        return 1;

    /* Machine is running? */
    MachineState_T machineState;
    CHECK_ERROR_RET(machine, COMGETTER(State)(&machineState), 1);
    if (machineState != MachineState_Running)
    {
        RTMsgError("Machine \"%s\" is not running!\n", a->argv[0]);
        return 1;
    }

    /* Open a session for the VM. */
    CHECK_ERROR_RET(machine, LockMachine(a->session, LockType_Shared), 1);

    do
    {
        /* Get the associated console. */
        ComPtr<IConsole> console;
        CHECK_ERROR_BREAK(a->session, COMGETTER(Console)(console.asOutParam()));
        /* ... and session machine */
        ComPtr<IMachine> sessionMachine;
        CHECK_ERROR_BREAK(a->session, COMGETTER(Machine)(sessionMachine.asOutParam()));

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

            /* Setup signal handling if cancelable. */
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
    return SUCCEEDED(rc) ? 0 : 1;
}

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

/**
 * Appends a new to-copy object to a copy list.
 *
 * @return  IPRT status code.
 * @param   pszFileSource       Full qualified source path of file to copy.
 * @param   pszFileDest         Full qualified destination path.
 * @param   pList               Copy list used for insertion.
 */
int ctrlCopyDirectoryEntryAppend(const char *pszFileSource, const char *pszFileDest,
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
int ctrlCopyDirectoryRead(const char *pszRootDir, const char *pszSubDir,
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
int ctrlCopyInit(const char *pszSource, const char *pszDest, uint32_t uFlags,
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
void ctrlCopyDestroy(PRTLISTNODE pList)
{
    AssertPtr(pList);

    /* Destroy file list. */
    PDIRECTORYENTRY pNode = RTListNodeGetFirst(pList, DIRECTORYENTRY, Node);
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
int ctrlCopyFileToGuest(IGuest *pGuest, const char *pszSource, const char *pszDest,
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
        /* If we got a VBOX_E_IPRT error we handle the error in a more gentle way
         * because it contains more accurate info about what went wrong. */
        ErrorInfo info(pGuest, COM_IIDOF(IGuest));
        if (info.isFullAvailable())
        {
            if (rc == VBOX_E_IPRT_ERROR)
                RTMsgError("%ls.", info.getText().raw());
            else
                RTMsgError("%ls (%Rhrc).", info.getText().raw(), info.getResultCode());
        }
        vrc = VERR_GENERAL_FAILURE;
    }
    else
    {
        /* Setup signal handling if cancelable. */
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
        while (   SUCCEEDED(progress->COMGETTER(Completed(&fCompleted)))
               && !fCompleted)
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
            //RTPrintf("Copy operation canceled!\n");
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
                    if (RT_FAILURE(vrc))
                        RTMsgError("Error while looking up error code, rc=%Rrc\n", vrc);
                    else
                        com::GluePrintRCMessage(iRc);
                }
                vrc = VERR_GENERAL_FAILURE;
            }
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

    Utf8Str Utf8Source(a->argv[1]);
    Utf8Str Utf8Dest(a->argv[2]);
    Utf8Str Utf8UserName;
    Utf8Str Utf8Password;
    uint32_t uFlags = CopyFileFlag_None;
    bool fVerbose = false;
    bool fCopyRecursive = false;
    bool fDryRun = false;

    /* Iterate through all possible commands (if available). */
    bool usageOK = true;
    for (int i = 3; usageOK && i < a->argc; i++)
    {
        if (   !strcmp(a->argv[i], "--username")
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
        else if (!strcmp(a->argv[i], "--dryrun"))
        {
            fDryRun = true;
        }
        else if (!strcmp(a->argv[i], "--flags"))
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

    if (Utf8UserName.isEmpty())
        return errorSyntax(USAGE_GUESTCONTROL,
                           "No user name specified!");

    /* Lookup VM. */
    ComPtr<IMachine> machine;
    /* Assume it's an UUID. */
    HRESULT rc;
    CHECK_ERROR(a->virtualBox, FindMachine(Bstr(a->argv[0]).raw(),
                                           machine.asOutParam()));
    if (FAILED(rc))
        return 1;

    /* Machine is running? */
    MachineState_T machineState;
    CHECK_ERROR_RET(machine, COMGETTER(State)(&machineState), 1);
    if (machineState != MachineState_Running)
    {
        RTMsgError("Machine \"%s\" is not running!\n", a->argv[0]);
        return 1;
    }

    /* Open a session for the VM. */
    CHECK_ERROR_RET(machine, LockMachine(a->session, LockType_Shared), 1);

    do
    {
        /* Get the associated console. */
        ComPtr<IConsole> console;
        CHECK_ERROR_BREAK(a->session, COMGETTER(Console)(console.asOutParam()));
        /* ... and session machine */
        ComPtr<IMachine> sessionMachine;
        CHECK_ERROR_BREAK(a->session, COMGETTER(Machine)(sessionMachine.asOutParam()));

        ComPtr<IGuest> guest;
        CHECK_ERROR_BREAK(console, COMGETTER(Guest)(guest.asOutParam()));

        if (fVerbose)
        {
            if (fDryRun)
                RTPrintf("Dry run - no files copied!\n");
            RTPrintf("Gathering file information ...\n");
        }

        RTLISTNODE listToCopy;
        uint32_t cObjects = 0;
        int vrc = ctrlCopyInit(Utf8Source.c_str(), Utf8Dest.c_str(), uFlags,
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
                    if (RT_FAILURE(vrc))
                        break;
                    uCurObject++;
                }
                if (RT_SUCCESS(vrc) && fVerbose)
                    RTPrintf("Copy operation successful!\n");
            }
            ctrlCopyDestroy(&listToCopy);
        }
        a->session->UnlockMachine();
    } while (0);
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

    Utf8Str Utf8Directory(a->argv[1]);
    Utf8Str Utf8UserName;
    Utf8Str Utf8Password;
    uint32_t uFlags = CreateDirectoryFlag_None;
    uint32_t uMode = 0;
    bool fVerbose = false;

    /* Iterate through all possible commands (if available). */
    bool usageOK = true;
    for (int i = 3; usageOK && i < a->argc; i++)
    {
        if (   !strcmp(a->argv[i], "--username")
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
        else if (   !strcmp(a->argv[i], "--parents")
                 || !strcmp(a->argv[i], "-p"))
        {
            uFlags |= CreateDirectoryFlag_Parents;
        }
        else if (   !strcmp(a->argv[i], "--mode")
                 || !strcmp(a->argv[i], "-m"))
        {
            if (i + 1 >= a->argc)
                usageOK = false;
            else
            {
                uMode = atoi(a->argv[i + 1]);
                ++i;
            }
        }
        else if (!strcmp(a->argv[i], "--flags"))
        {
            if (i + 1 >= a->argc)
                usageOK = false;
            else
            {
                /* Nothing to do here yet. */
                ++i;
            }
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

    if (Utf8Directory.isEmpty())
        return errorSyntax(USAGE_GUESTCONTROL,
                           "No directory specified!");

    if (Utf8UserName.isEmpty())
        return errorSyntax(USAGE_GUESTCONTROL,
                           "No user name specified!");

    /* Lookup VM. */
    ComPtr<IMachine> machine;
    /* Assume it's an UUID. */
    HRESULT rc;
    CHECK_ERROR(a->virtualBox, FindMachine(Bstr(a->argv[0]).raw(),
                                           machine.asOutParam()));
    if (FAILED(rc))
        return 1;

    /* Machine is running? */
    MachineState_T machineState;
    CHECK_ERROR_RET(machine, COMGETTER(State)(&machineState), 1);
    if (machineState != MachineState_Running)
    {
        RTMsgError("Machine \"%s\" is not running!\n", a->argv[0]);
        return 1;
    }

    /* Open a session for the VM. */
    CHECK_ERROR_RET(machine, LockMachine(a->session, LockType_Shared), 1);

    do
    {
        /* Get the associated console. */
        ComPtr<IConsole> console;
        CHECK_ERROR_BREAK(a->session, COMGETTER(Console)(console.asOutParam()));
        /* ... and session machine */
        ComPtr<IMachine> sessionMachine;
        CHECK_ERROR_BREAK(a->session, COMGETTER(Machine)(sessionMachine.asOutParam()));

        ComPtr<IGuest> guest;
        CHECK_ERROR_BREAK(console, COMGETTER(Guest)(guest.asOutParam()));

        ComPtr<IProgress> progress;
        rc = guest->CreateDirectory(Bstr(Utf8Directory).raw(),
                                    Bstr(Utf8UserName).raw(), Bstr(Utf8Password).raw(),
                                    uMode, uFlags, progress.asOutParam());
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
        }
        else
        {
            /* Setup signal handling if cancelable. */
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
            while (   SUCCEEDED(progress->COMGETTER(Completed(&fCompleted)))
                   && !fCompleted)
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
                //RTPrintf("Copy operation canceled!\n");
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
                }
            }
        }

        a->session->UnlockMachine();
    } while (0);
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

    /* Lookup VM. */
    ComPtr<IMachine> machine;
    /* Assume it's an UUID. */
    HRESULT rc;
    CHECK_ERROR(a->virtualBox, FindMachine(Bstr(a->argv[0]).raw(),
                                           machine.asOutParam()));
    if (FAILED(rc))
        return 1;

    /* Machine is running? */
    MachineState_T machineState;
    CHECK_ERROR_RET(machine, COMGETTER(State)(&machineState), 1);
    if (machineState != MachineState_Running)
    {
        RTMsgError("Machine \"%s\" is not running!\n", a->argv[0]);
        return 1;
    }

    /* Open a session for the VM. */
    CHECK_ERROR_RET(machine, LockMachine(a->session, LockType_Shared), 1);

    do
    {
        /* Get the associated console. */
        ComPtr<IConsole> console;
        CHECK_ERROR_BREAK(a->session, COMGETTER(Console)(console.asOutParam()));
        /* ... and session machine */
        ComPtr<IMachine> sessionMachine;
        CHECK_ERROR_BREAK(a->session, COMGETTER(Machine)(sessionMachine.asOutParam()));

        ComPtr<IGuest> guest;
        CHECK_ERROR_BREAK(console, COMGETTER(Guest)(guest.asOutParam()));

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
            int vrc = RTPathAppPrivateNoArch(strTemp, sizeof(strTemp));
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
                break;
            }
        }
        else if (!RTFileExists(Utf8Source.c_str()))
        {
            RTMsgError("Source \"%s\" does not exist!\n", Utf8Source.c_str());
            break;
        }
        if (fVerbose)
            RTPrintf("Using source: %s\n", Utf8Source.c_str());

        ComPtr<IProgress> progress;
        CHECK_ERROR_BREAK(guest, UpdateGuestAdditions(Bstr(Utf8Source).raw(),
                                                      0 /* Flags, not used. */,
                                                      progress.asOutParam()));
        rc = showProgress(progress);
        if (FAILED(rc))
        {
            com::ProgressErrorInfo info(progress);
            if (info.isBasicAvailable())
                RTMsgError("Failed to start Guest Additions update. Error message: %lS\n", info.getText().raw());
            else
                RTMsgError("Failed to start Guest Additions update. No error message available!\n");
        }
        else
        {
            if (fVerbose)
                RTPrintf("Guest Additions installer successfully copied and started.\n");
        }
        a->session->UnlockMachine();
    } while (0);
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
    if (   !strcmp(a->argv[0], "exec")
        || !strcmp(a->argv[0], "execute"))
    {
        return handleCtrlExecProgram(&arg);
    }
    else if (   !strcmp(a->argv[0], "copyto")
             || !strcmp(a->argv[0], "cp"))
    {
        return handleCtrlCopyTo(&arg);
    }
    else if (   !strcmp(a->argv[0], "createdirectory")
             || !strcmp(a->argv[0], "createdir")
             || !strcmp(a->argv[0], "mkdir"))
    {
        return handleCtrlCreateDirectory(&arg);
    }
    else if (   !strcmp(a->argv[0], "updateadditions")
             || !strcmp(a->argv[0], "updateadds"))
    {
        return handleCtrlUpdateAdditions(&arg);
    }

    /* default: */
    return errorSyntax(USAGE_GUESTCONTROL, "Incorrect parameters");
}

#endif /* !VBOX_ONLY_DOCS */
