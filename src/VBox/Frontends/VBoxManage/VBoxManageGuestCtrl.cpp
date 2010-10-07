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

#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/getopt.h>
#include <iprt/list.h>
#include <iprt/path.h>
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
    /* assume it's an UUID */
    HRESULT rc = a->virtualBox->GetMachine(Bstr(a->argv[0]).raw(),
                                           machine.asOutParam());
    if (FAILED(rc) || !machine)
    {
        /* must be a name */
        CHECK_ERROR(a->virtualBox, FindMachine(Bstr(a->argv[0]).raw(),
                                               machine.asOutParam()));
    }

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

#ifdef VBOX_WITH_COPYTOGUEST

#define VBOX_ISO9660_MAX_SYSTEM_ID 32
#define VBOX_ISO9660_MAX_VOLUME_ID 32
#define VBOX_ISO9660_MAX_PUBLISHER_ID   128
#define VBOX_ISO9660_MAX_VOLUME_ID   32
#define VBOX_ISO9660_MAX_VOLUMESET_ID   128
#define VBOX_ISO9660_MAX_PREPARER_ID   128
#define VBOX_ISO9660_MAX_APPLICATION_ID   128
#define VBOX_ISO9660_STANDARD_ID "CD001"
#define VBOX_ISO9660_SECTOR_SIZE 2048

#pragma pack(1)
typedef struct VBoxISO9660DateShort
{
    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    int8_t 	gmt_offset;
} VBoxISO9660DateShort;

typedef struct VBoxISO9660DateLong
{
    char year[4];
    char month[2];
    char day[2];
    char hour[2];
    char minute[2];
    char second[2];
    char hseconds[2];
    int8_t gmt_offset;
} VBoxISO9660DateLong;

typedef struct VBoxISO9660DirRecord
{
	uint8_t record_length;
	uint8_t extented_attr_length;
	uint32_t extent_location;
    uint32_t extent_location_big;
	uint32_t extent_data_length; /* Number of bytes (file) / len (directory). */
    uint32_t extent_data_length_big;
    VBoxISO9660DateShort date;
	uint8_t flags;
	uint8_t interleave_unit_size;
    uint8_t interleave_gap_size;
	uint16_t volume_sequence_number;
    uint16_t volume_sequence_number_big;
	uint8_t name_len;
    /* Starting here there will be the actual directory entry name
     * and a padding of 1 byte if name_len is odd. */
} VBoxISO9660DirRecord;

typedef struct VBoxISO9660PriVolDesc
{
	uint8_t type;
	char name_id[6];
	uint8_t version;
	char system_id[VBOX_ISO9660_MAX_SYSTEM_ID];
	char volume_id[VBOX_ISO9660_MAX_VOLUME_ID];
	uint8_t unused2[8];
	uint32_t volume_space_size; /* Number of sectors, Little Endian. */
	uint32_t volume_space_size_big; /* Number of sectors Big Endian. */
    uint8_t unused3[32];
	uint16_t volume_set_size;
    uint16_t volume_set_size_big;
    uint16_t volume_sequence_number;
    uint16_t volume_sequence_number_big;
    uint16_t logical_block_size; /* 2048. */
    uint16_t logical_block_size_big;
	uint32_t path_table_size; /* Size in bytes. */
	uint32_t path_table_size_big; /* Size in bytes. */
    uint32_t path_table_start_first;
    uint32_t path_table_start_second;
    uint32_t path_table_start_first_big;
    uint32_t path_table_start_second_big;
    VBoxISO9660DirRecord root_directory_record;
    uint8_t directory_padding;
    char volume_set_id[VBOX_ISO9660_MAX_VOLUMESET_ID];
    char publisher_id[VBOX_ISO9660_MAX_PUBLISHER_ID];
    char preparer_id[VBOX_ISO9660_MAX_PREPARER_ID];
    char application_id[VBOX_ISO9660_MAX_APPLICATION_ID];
    char copyright_file_id[37];
    char abstract_file_id[37];
    char bibliographic_file_id[37];
    VBoxISO9660DateLong creation_date;
    VBoxISO9660DateLong modification_date;
    VBoxISO9660DateLong expiration_date;
    VBoxISO9660DateLong effective_date;
    uint8_t file_structure_version;
    uint8_t unused4[1];
    char application_data[512];
    uint8_t unused5[653];
} VBoxISO9660PriVolDesc;

typedef struct VBoxISO9660PathTableHeader
{
    uint8_t length;
    uint8_t extended_attr_sectors;
    /** Sector of starting directory table. */
    uint32_t sector_dir_table;
    /** Index of parent directory (1 for the root). */
    uint16_t parent_index;
    /* Starting here there will be the name of the directory,
     * specified by length above. */
} VBoxISO9660PathTableHeader;

typedef struct VBoxISO9660PathTableEntry
{
    char       *path;
    char       *path_full;
    VBoxISO9660PathTableHeader header;
    RTLISTNODE  Node;
} VBoxISO9660PathTableEntry;

typedef struct VBoxISO9660File
{
    RTFILE file;
    RTLISTNODE listPaths;
    VBoxISO9660PriVolDesc priVolDesc;
} VBoxISO9660File;
#pragma pack()

void rtISO9660DestroyPathCache(VBoxISO9660File *pFile)
{
    VBoxISO9660PathTableEntry *pNode = RTListNodeGetFirst(&pFile->listPaths, VBoxISO9660PathTableEntry, Node);
    while (pNode)
    {
        VBoxISO9660PathTableEntry *pNext = RTListNodeGetNext(&pNode->Node, VBoxISO9660PathTableEntry, Node);
        bool fLast = RTListNodeIsLast(&pFile->listPaths, &pNode->Node);

        if (pNode->path)
            RTStrFree(pNode->path);
        if (pNode->path_full)
            RTStrFree(pNode->path_full);
        RTListNodeRemove(&pNode->Node);
        RTMemFree(pNode);

        if (fLast)
            break;

        pNode = pNext;
    }
}

void RTISO9660Close(VBoxISO9660File *pFile)
{
    if (pFile)
    {
        rtISO9660DestroyPathCache(pFile);
        RTFileClose(pFile->file);
    }
}

int rtISO9660AddToPathCache(PRTLISTNODE pList, const char *pszPath,
                            VBoxISO9660PathTableHeader *pHeader)
{
    AssertPtrReturn(pList, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszPath, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pHeader, VERR_INVALID_PARAMETER);

    VBoxISO9660PathTableEntry *pNode = (VBoxISO9660PathTableEntry*)RTMemAlloc(sizeof(VBoxISO9660PathTableEntry));
    if (pNode == NULL)
        return VERR_NO_MEMORY;

    pNode->path = NULL;
    if (RT_SUCCESS(RTStrAAppend(&pNode->path, pszPath)))
    {
        memcpy((VBoxISO9660PathTableHeader*)&pNode->header,
               (VBoxISO9660PathTableHeader*)pHeader, sizeof(pNode->header));

        pNode->path_full = NULL;
        pNode->Node.pPrev = NULL;
        pNode->Node.pNext = NULL;
        RTListAppend(pList, &pNode->Node);
        return VINF_SUCCESS;
    }
    return VERR_NO_MEMORY;
}

int rtISO9660GetParentPathSub(PRTLISTNODE pList, VBoxISO9660PathTableEntry *pNode,
                              char *pszPathNode, char **ppszPath)
{
    int rc = VINF_SUCCESS;
    if (pNode->header.parent_index > 1)
    {
        uint16_t idx = 1;
        VBoxISO9660PathTableEntry *pNodeParent = RTListNodeGetFirst(pList, VBoxISO9660PathTableEntry, Node);
        while (idx++ < pNode->header.parent_index)
            pNodeParent =  RTListNodeGetNext(&pNodeParent->Node, VBoxISO9660PathTableEntry, Node);
        char *pszPath;
        if (RTStrAPrintf(&pszPath, "%s/%s", pNodeParent->path, pszPathNode))
        {
            rc = rtISO9660GetParentPathSub(pList, pNodeParent, pszPath, ppszPath);
            RTStrFree(pszPath);
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else
    {
        char *pszPath = RTStrDup(pszPathNode);
        *ppszPath = pszPath;
    }
    return rc;
}

/* Is a *flat* structure! */
int rtISO9660UpdatePathCache(VBoxISO9660File *pFile)
{
    AssertPtrReturn(pFile, VERR_INVALID_PARAMETER);
    rtISO9660DestroyPathCache(pFile);

    RTListInit(&pFile->listPaths);

    /* Seek to path tables. */
    int rc = VINF_SUCCESS;
    Assert(pFile->priVolDesc.path_table_start_first > 16);
    uint64_t uTableStart = (pFile->priVolDesc.path_table_start_first * VBOX_ISO9660_SECTOR_SIZE);
    Assert(uTableStart % VBOX_ISO9660_SECTOR_SIZE == 0); /* Make sure it's aligned. */
    if (RTFileTell(pFile->file) != uTableStart)
        rc = RTFileSeek(pFile->file, uTableStart, RTFILE_SEEK_BEGIN, &uTableStart);

    /*
     * Since this is a sequential format, for performance it's best to read the
     * complete path table (every entry can have its own level (directory depth) first
     * and the actual directories of the path table afterwards.
     */

    /* Read in the path table ... */
    uint32_t cbLeft = pFile->priVolDesc.path_table_size;
    VBoxISO9660PathTableHeader header;
    while ((cbLeft > 0) && RT_SUCCESS(rc))
    {
        size_t cbRead;
        rc = RTFileRead(pFile->file, (VBoxISO9660PathTableHeader*)&header, sizeof(VBoxISO9660PathTableHeader), &cbRead);
        if (RT_FAILURE(rc))
            break;
        cbLeft -= cbRead;
        if (header.length)
        {
            Assert(cbLeft >= header.length);
            Assert(header.length <= 31);
            /* Allocate and read in the actual path name. */
            char *pszName = RTStrAlloc(header.length + 1);
            rc = RTFileRead(pFile->file, (char*)pszName, header.length, &cbRead);
            if (RT_SUCCESS(rc))
            {
                cbLeft -= cbRead;
                pszName[cbRead] = '\0'; /* Terminate string. */
                /* Add entry to cache ... */
                rc = rtISO9660AddToPathCache(&pFile->listPaths, pszName, &header);
            }
            RTStrFree(pszName);
            /* Read padding if required ... */
            if ((header.length % 2) != 0) /* If we have an odd length, read/skip the padding byte. */
            {
                rc = RTFileSeek(pFile->file, 1, RTFILE_SEEK_CURRENT, NULL);
                cbLeft--;
            }
        }
    }

    /* Transform path names into full paths. This is a bit ugly right now. */
    VBoxISO9660PathTableEntry *pNode = RTListNodeGetLast(&pFile->listPaths, VBoxISO9660PathTableEntry, Node);
    while (   pNode
           && !RTListNodeIsFirst(&pFile->listPaths, &pNode->Node)
           && RT_SUCCESS(rc))
    {
        rc = rtISO9660GetParentPathSub(&pFile->listPaths, pNode,
                                       pNode->path, &pNode->path_full);
        if (RT_SUCCESS(rc))
            pNode = RTListNodeGetPrev(&pNode->Node, VBoxISO9660PathTableEntry, Node);
    }

    return rc;
}

int RTISO9660Open(const char *pszFileName, VBoxISO9660File *pFile)
{
    AssertPtrReturn(pszFileName, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pFile, VERR_INVALID_PARAMETER);

    RTListInit(&pFile->listPaths);
#if 1
    Assert(sizeof(VBoxISO9660DateShort) == 7);
    Assert(sizeof(VBoxISO9660DateLong) == 17);
    int l = sizeof(VBoxISO9660DirRecord);
    RTPrintf("VBoxISO9660DirRecord=%ld\n", l);
    Assert(l == 33);
    /* Each volume descriptor exactly occupies one sector. */
    l = sizeof(VBoxISO9660PriVolDesc);
    RTPrintf("VBoxISO9660PriVolDesc=%ld\n", l);
    Assert(l == VBOX_ISO9660_SECTOR_SIZE);
#endif
    int rc = RTFileOpen(&pFile->file, pszFileName, RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_WRITE);
    if (RT_SUCCESS(rc))
    {
        uint64_t cbSize;
        rc = RTFileGetSize(pFile->file, &cbSize);
        if (   RT_SUCCESS(rc)
            && cbSize > 16 * VBOX_ISO9660_SECTOR_SIZE)
        {
            uint64_t cbOffset = 16 * VBOX_ISO9660_SECTOR_SIZE; /* Start reading at 32k. */
            size_t cbRead;
            VBoxISO9660PriVolDesc volDesc;
            bool fFoundPrimary = false;
            bool fIsValid = false;
            while (cbOffset < _1M)
            {
                /* Get primary descriptor. */
                rc = RTFileRead(pFile->file, (VBoxISO9660PriVolDesc*)&volDesc, sizeof(VBoxISO9660PriVolDesc), &cbRead);
                if (RT_FAILURE(rc) || cbRead < sizeof(VBoxISO9660PriVolDesc))
                    break;
                if (   RTStrStr((char*)volDesc.name_id, VBOX_ISO9660_STANDARD_ID)
                    && volDesc.type    == 0x1 /* Primary Volume Descriptor */)
                {
                    memcpy((VBoxISO9660PriVolDesc*)&pFile->priVolDesc,
                           (VBoxISO9660PriVolDesc*)&volDesc, sizeof(VBoxISO9660PriVolDesc));
                    fFoundPrimary = true;
                }
                else if(volDesc.type == 0xff /* Termination Volume Descriptor */)
                {
                    if (fFoundPrimary)
                        fIsValid = true;
                    break;
                }
                cbOffset += sizeof(VBoxISO9660PriVolDesc);
            }

            if (fIsValid)
                rc = rtISO9660UpdatePathCache(pFile);
            else
                rc = VERR_INVALID_PARAMETER;
        }
        if (RT_FAILURE(rc))
            RTISO9660Close(pFile);
    }
    return rc;
}

/* Parses the extent content given at a specified sector. */
int rtISO9660FindEntry(VBoxISO9660File *pFile, const char *pszFileName,
                       uint32_t uExtentSector, uint32_t cbExtent /* Bytes */,
                       VBoxISO9660DirRecord **ppRec)
{
    AssertPtrReturn(pFile, VERR_INVALID_PARAMETER);
    Assert(uExtentSector > 16);

    int rc = RTFileSeek(pFile->file, uExtentSector * VBOX_ISO9660_SECTOR_SIZE,
                        RTFILE_SEEK_BEGIN, NULL);
    if (RT_SUCCESS(rc))
    {
        rc = VERR_FILE_NOT_FOUND;

        uint8_t uBuffer[VBOX_ISO9660_SECTOR_SIZE];
        uint32_t cbLeft = cbExtent;
        while (!RT_SUCCESS(rc) && cbLeft > 0)
        {
            size_t cbRead;
            int rc2 = RTFileRead(pFile->file, (void*)&uBuffer, sizeof(uBuffer), &cbRead);
            Assert(RT_SUCCESS(rc2) && cbRead == VBOX_ISO9660_SECTOR_SIZE);
            cbLeft -= cbRead;

            uint32_t idx = 0;
            while (idx < cbRead)
            {
                VBoxISO9660DirRecord *pCurRecord = (VBoxISO9660DirRecord*)&uBuffer[idx];
                if (pCurRecord->record_length == 0)
                    break;

                Assert(pCurRecord->name_len > 0);
                char *pszName = RTStrAlloc(pCurRecord->name_len + 1);
                AssertPtr(pszName);
                Assert(idx + sizeof(VBoxISO9660DirRecord) < cbRead);
                memcpy(pszName, &uBuffer[idx + sizeof(VBoxISO9660DirRecord)], pCurRecord->name_len);

                if (   pCurRecord->name_len == 1
                    && pszName[0] == 0x0)
                {
                    /* This is a "." directory (self). */
                }
                else if (   pCurRecord->name_len == 1
                         && pszName[0] == 0x1)
                {
                    /* This is a ".." directory (parent). */
                }
                else /* Regular directory or file */
                {
                    if (pCurRecord->flags & RT_BIT(1)) /* Directory */
                    {
                        /* We don't recursively go into directories
                         * because we already have the cached path table. */
                        pszName[pCurRecord->name_len] = 0;
                        /*rc = rtISO9660ParseDir(pFile, pszFileName,
                                                 pDirHdr->extent_location, pDirHdr->extent_data_length);*/
                    }
                    else /* File */
                    {
                        /* Get last occurence of ";" and cut it off. */
                        char *pTerm = strrchr(pszName, ';');
                        if (pTerm)
                            pszName[pTerm - pszName] = 0;

                        /* Don't use case sensitive comparison here, in IS0 9660 all
                         * file / directory names are UPPERCASE. */
                        if (!RTStrICmp(pszName, pszFileName))
                        {
                            VBoxISO9660DirRecord *pRec = (VBoxISO9660DirRecord*)RTMemAlloc(sizeof(VBoxISO9660DirRecord));
                            if (pRec)
                            {
                                memcpy(pRec, pCurRecord, sizeof(VBoxISO9660DirRecord));
                                *ppRec = pRec;
                                rc = VINF_SUCCESS;
                                break;
                            }
                            else
                                rc = VERR_NO_MEMORY;
                        }
                    }
                }
                idx += pCurRecord->record_length;
            }
        }
    }
    return rc;
}

int rtISO9660ResolvePath(VBoxISO9660File *pFile, const char *pszPath, uint32_t *puSector)
{
    AssertPtrReturn(pFile, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszPath, VERR_INVALID_PARAMETER);
    AssertPtrReturn(puSector, VERR_INVALID_PARAMETER);

    int rc = VERR_FILE_NOT_FOUND;
    char *pszTemp = RTStrDup(pszPath);
    if (pszTemp)
    {
        RTPathStripFilename(pszTemp);

        bool bFound = false;
        VBoxISO9660PathTableEntry *pNode;
        if (!RTStrCmp(pszTemp, ".")) /* Root directory? Use first node! */
        {
            pNode = RTListNodeGetFirst(&pFile->listPaths, VBoxISO9660PathTableEntry, Node);
            bFound = true;
        }
        else
        {
            RTListForEach(&pFile->listPaths, pNode, VBoxISO9660PathTableEntry, Node)
            {
                if (   pNode->path_full != NULL /* Root does not have a path! */
                    && !RTStrICmp(pNode->path_full, pszTemp))
                {
                    bFound = true;
                    break;
                }
            }
        }
        if (bFound)
        {
            *puSector = pNode->header.sector_dir_table;
            rc = VINF_SUCCESS;
        }
        RTStrFree(pszTemp);
    }
    else
        rc = VERR_NO_MEMORY;
    return rc;
}

int rtISO9660ReadFileInternal(VBoxISO9660File *pFile, const char *pszPath,
                              uint32_t cbOffset, size_t cbToRead, size_t *pcbRead)
{
    AssertPtrReturn(pFile, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszPath, VERR_INVALID_PARAMETER);

    uint32_t uSector;
    int rc = rtISO9660ResolvePath(pFile, pszPath, &uSector);
    if (RT_SUCCESS(rc))
    {
        /* Seek to directory table. */
        rc = RTFileSeek(pFile->file, uSector * VBOX_ISO9660_SECTOR_SIZE,
                        RTFILE_SEEK_BEGIN, NULL);
        if (RT_SUCCESS(rc))
        {
            size_t cbRead;
            VBoxISO9660DirRecord dir_table;
            rc = RTFileRead(pFile->file, (VBoxISO9660DirRecord*)&dir_table, sizeof(VBoxISO9660DirRecord), &cbRead);
            if (RT_SUCCESS(rc))
            {
                Assert(cbRead == sizeof(VBoxISO9660DirRecord));
                VBoxISO9660DirRecord *pRecord = NULL;
                rc = rtISO9660FindEntry(pFile,
                                        RTPathFilename(pszPath),
                                        dir_table.extent_location,
                                        dir_table.extent_data_length,
                                        &pRecord);
                if (   RT_SUCCESS(rc)
                    && pRecord)
                {
                    RTMemFree(pRecord);
                }
            }
            else
                rc = VERR_INVALID_PARAMETER;
        }
    }
    return rc;
}

int RTISO9660ReadFile(VBoxISO9660File *pFile, const char *pszFileName,
                      uint32_t cbOffset, size_t cbToRead, size_t *pcbRead)
{
    return rtISO9660ReadFileInternal(pFile, pszFileName, cbOffset, cbToRead, pcbRead);
}
#endif

static int handleCtrlCopyTo(HandlerArg *a)
{
    VBoxISO9660File file;
    int vrc = RTISO9660Open("c:\\Downloads\\VBoxGuestAdditions_3.2.8.iso", &file);
    if (RT_SUCCESS(vrc))
    {
        uint32_t uOffset = 0;
        size_t cbRead;
        while (RT_SUCCESS(vrc))
        {
            vrc = RTISO9660ReadFile(&file, "32BIT/OS2/readme.txt",
                                    uOffset, _4K, &cbRead);
            uOffset += cbRead;
        }
        RTISO9660Close(&file);
    }
    return vrc;

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
    HRESULT rc = a->virtualBox->GetMachine(Bstr(a->argv[0]).raw(),
                                           machine.asOutParam());
    if (FAILED(rc) || !machine)
    {
        /* must be a name */
        CHECK_ERROR(a->virtualBox, FindMachine(Bstr(a->argv[0]).raw(),
                                               machine.asOutParam()));
    }

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
