/* $Id$ */
/** @file
 * VBoxManage - VirtualBox's command-line interface.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
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
#ifndef VBOX_ONLY_DOCS
# include <VBox/com/com.h>
# include <VBox/com/string.h>
# include <VBox/com/Guid.h>
# include <VBox/com/array.h>
# include <VBox/com/ErrorInfo.h>
# include <VBox/com/errorprint.h>
# include <VBox/com/EventQueue.h>

# include <VBox/com/VirtualBox.h>
#endif /* !VBOX_ONLY_DOCS */

#include <VBox/err.h>
#include <VBox/version.h>

#include <iprt/asm.h>
#include <iprt/buildconfig.h>
#include <iprt/initterm.h>
#include <iprt/stream.h>
#include <iprt/string.h>

#include <signal.h>

#include "VBoxManage.h"


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/*extern*/ bool         g_fDetailedProgress = false;

#ifndef VBOX_ONLY_DOCS
/** Set by the signal handler. */
static volatile bool    g_fCanceled = false;


/**
 * Signal handler that sets g_fCanceled.
 *
 * This can be executed on any thread in the process, on Windows it may even be
 * a thread dedicated to delivering this signal.  Do not doing anything
 * unnecessary here.
 */
static void showProgressSignalHandler(int iSignal)
{
    NOREF(iSignal);
    ASMAtomicWriteBool(&g_fCanceled, true);
}


/**
 * Print out progress on the console
 */
HRESULT showProgress(ComPtr<IProgress> progress)
{
    using namespace com;

    BOOL fCompleted;
    ULONG ulCurrentPercent;
    ULONG ulLastPercent = 0;

    ULONG ulCurrentOperationPercent;
    ULONG ulLastOperationPercent = (ULONG)-1;

    ULONG ulLastOperation = (ULONG)-1;
    Bstr bstrOperationDescription;

    ULONG cOperations;
    progress->COMGETTER(OperationCount)(&cOperations);

    if (!g_fDetailedProgress)
    {
        RTStrmPrintf(g_pStdErr, "0%%...");
        RTStrmFlush(g_pStdErr);
    }

    /* setup signal handling if cancelable */
    bool fCanceledAlready = false;
    BOOL fCancelable;
    HRESULT hrc = progress->COMGETTER(Cancelable)(&fCancelable);
    if (FAILED(hrc))
        fCancelable = FALSE;
    if (fCancelable)
    {
        signal(SIGINT,   showProgressSignalHandler);
#ifdef SIGBREAK
        signal(SIGBREAK, showProgressSignalHandler);
#endif
    }

    while (SUCCEEDED(progress->COMGETTER(Completed(&fCompleted))))
    {
        ULONG ulOperation;
        progress->COMGETTER(Operation)(&ulOperation);

        progress->COMGETTER(Percent(&ulCurrentPercent));
        progress->COMGETTER(OperationPercent(&ulCurrentOperationPercent));

        if (g_fDetailedProgress)
        {
            if (ulLastOperation != ulOperation)
            {
                progress->COMGETTER(OperationDescription(bstrOperationDescription.asOutParam()));
                ulLastPercent = (ULONG)-1;        // force print
                ulLastOperation = ulOperation;
            }

            if (    ulCurrentPercent != ulLastPercent
                 || ulCurrentOperationPercent != ulLastOperationPercent
               )
            {
                LONG lSecsRem;
                progress->COMGETTER(TimeRemaining)(&lSecsRem);

                RTStrmPrintf(g_pStdErr, "(%ld/%ld) %ls %ld%% => %ld%% (%d s remaining)\n", ulOperation + 1, cOperations, bstrOperationDescription.raw(), ulCurrentOperationPercent, ulCurrentPercent, lSecsRem);
                ulLastPercent = ulCurrentPercent;
                ulLastOperationPercent = ulCurrentOperationPercent;
            }
        }
        else
        {
            /* did we cross a 10% mark? */
            if (ulCurrentPercent / 10  >  ulLastPercent / 10)
            {
                /* make sure to also print out missed steps */
                for (ULONG curVal = (ulLastPercent / 10) * 10 + 10; curVal <= (ulCurrentPercent / 10) * 10; curVal += 10)
                {
                    if (curVal < 100)
                    {
                        RTStrmPrintf(g_pStdErr, "%ld%%...", curVal);
                        RTStrmFlush(g_pStdErr);
                    }
                }
                ulLastPercent = (ulCurrentPercent / 10) * 10;
            }
        }
        if (fCompleted)
            break;

        /* process async cancelation */
        if (g_fCanceled && !fCanceledAlready)
        {
            hrc = progress->Cancel();
            if (SUCCEEDED(hrc))
                fCanceledAlready = true;
            else
                g_fCanceled = false;
        }

        /* make sure the loop is not too tight */
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

    /* complete the line. */
    LONG iRc = E_FAIL;
    if (SUCCEEDED(progress->COMGETTER(ResultCode)(&iRc)))
    {
        if (SUCCEEDED(iRc))
            RTStrmPrintf(g_pStdErr, "100%%\n");
        else if (g_fCanceled)
            RTStrmPrintf(g_pStdErr, "CANCELED\n");
        else
            RTStrmPrintf(g_pStdErr, "FAILED\n");
    }
    else
        RTStrmPrintf(g_pStdErr, "\n");
    RTStrmFlush(g_pStdErr);
    return iRc;
}

#endif /* !VBOX_ONLY_DOCS */

int main(int argc, char *argv[])
{
    /*
     * Before we do anything, init the runtime without loading
     * the support driver.
     */
    RTR3Init();

    bool fShowLogo = false;
    bool fShowHelp = false;
    int  iCmd      = 1;
    int  iCmdArg;

    /* global options */
    for (int i = 1; i < argc || argc <= iCmd; i++)
    {
        if (    argc <= iCmd
            ||  !strcmp(argv[i], "help")
            ||  !strcmp(argv[i], "-?")
            ||  !strcmp(argv[i], "-h")
            ||  !strcmp(argv[i], "-help")
            ||  !strcmp(argv[i], "--help"))
        {
            if (i >= argc - 1)
            {
                showLogo(g_pStdOut);
                printUsage(USAGE_ALL, g_pStdOut);
                return 0;
            }
            fShowLogo = true;
            fShowHelp = true;
            iCmd++;
            continue;
        }

        if (   !strcmp(argv[i], "-v")
            || !strcmp(argv[i], "-version")
            || !strcmp(argv[i], "-Version")
            || !strcmp(argv[i], "--version"))
        {
            /* Print version number, and do nothing else. */
            RTPrintf("%sr%d\n", VBOX_VERSION_STRING, RTBldCfgRevision());
            return 0;
        }

        if (   !strcmp(argv[i], "--dumpopts")
            || !strcmp(argv[i], "-dumpopts"))
        {
            /* Special option to dump really all commands,
             * even the ones not understood on this platform. */
            printUsage(USAGE_DUMPOPTS, g_pStdOut);
            return 0;
        }

        if (   !strcmp(argv[i], "--nologo")
            || !strcmp(argv[i], "-nologo")
            || !strcmp(argv[i], "-q"))
        {
            /* suppress the logo */
            fShowLogo = false;
            iCmd++;
        }
        else
        {
            break;
        }
    }

    iCmdArg = iCmd + 1;

    if (fShowLogo)
        showLogo(g_pStdOut);


#ifdef VBOX_ONLY_DOCS
    int rc = 0;
#else /* !VBOX_ONLY_DOCS */
    using namespace com;
    HRESULT rc = 0;

    rc = com::Initialize();
    if (FAILED(rc))
    {
        RTMsgError("Failed to initialize COM!");
        return rc;
    }

    /*
     * The input is in the host OS'es codepage (NT guarantees ACP).
     * For VBox we use UTF-8 and convert to UCS-2 when calling (XP)COM APIs.
     * For simplicity, just convert the argv[] array here.
     */
    for (int i = iCmdArg; i < argc; i++)
    {
        char *converted;
        RTStrCurrentCPToUtf8(&converted, argv[i]);
        if (converted)
            argv[i] = converted;
        else
            /* Conversion was not possible,probably due to invalid characters.
             * Keep in mind that we do RTStrFree on the whole array below. */
            argv[i] = RTStrDup(argv[i]);
    }

    do
    {
    // scopes all the stuff till shutdown
    ////////////////////////////////////////////////////////////////////////////

    /* convertfromraw: does not need a VirtualBox instantiation. */
    if (argc >= iCmdArg && (   !strcmp(argv[iCmd], "convertfromraw")
                            || !strcmp(argv[iCmd], "convertdd")))
    {
        rc = handleConvertFromRaw(argc - iCmdArg, argv + iCmdArg);
        break;
    }

    ComPtr<IVirtualBox> virtualBox;
    ComPtr<ISession> session;

    rc = virtualBox.createLocalObject(CLSID_VirtualBox);
    if (FAILED(rc))
        RTMsgError("Failed to create the VirtualBox object!");
    else
    {
        rc = session.createInprocObject(CLSID_Session);
        if (FAILED(rc))
            RTMsgError("Failed to create a session object!");
    }

    if (FAILED(rc))
    {
        com::ErrorInfo info;
        if (!info.isFullAvailable() && !info.isBasicAvailable())
        {
            com::GluePrintRCMessage(rc);
            RTMsgError("Most likely, the VirtualBox COM server is not running or failed to start.");
        }
        else
            com::GluePrintErrorInfo(info);
        break;
    }

    HandlerArg handlerArg = { 0, NULL, virtualBox, session };

    /*
     * All registered command handlers
     */
    static const struct
    {
        const char *command;
        USAGECATEGORY help;
        int (*handler)(HandlerArg *a);
    } s_commandHandlers[] =
    {
        { "internalcommands", 0,                       handleInternalCommands },
        { "list",             USAGE_LIST,              handleList },
        { "showvminfo",       USAGE_SHOWVMINFO,        handleShowVMInfo },
        { "registervm",       USAGE_REGISTERVM,        handleRegisterVM },
        { "unregistervm",     USAGE_UNREGISTERVM,      handleUnregisterVM },
        { "createhd",         USAGE_CREATEHD,          handleCreateHardDisk },
        { "createvdi",        USAGE_CREATEHD,          handleCreateHardDisk }, /* backward compatiblity */
        { "modifyhd",         USAGE_MODIFYHD,          handleModifyHardDisk },
        { "modifyvdi",        USAGE_MODIFYHD,          handleModifyHardDisk }, /* backward compatiblity */
        { "clonehd",          USAGE_CLONEHD,           handleCloneHardDisk },
        { "clonevdi",         USAGE_CLONEHD,           handleCloneHardDisk }, /* backward compatiblity */
        { "addiscsidisk",     USAGE_ADDISCSIDISK,      handleAddiSCSIDisk },
        { "createvm",         USAGE_CREATEVM,          handleCreateVM },
        { "modifyvm",         USAGE_MODIFYVM,          handleModifyVM },
        { "startvm",          USAGE_STARTVM,           handleStartVM },
        { "controlvm",        USAGE_CONTROLVM,         handleControlVM },
        { "discardstate",     USAGE_DISCARDSTATE,      handleDiscardState },
        { "adoptstate",       USAGE_ADOPTSTATE,        handleAdoptState },
        { "snapshot",         USAGE_SNAPSHOT,          handleSnapshot },
        { "openmedium",       USAGE_OPENMEDIUM,        handleOpenMedium },
        { "registerimage",    USAGE_OPENMEDIUM,        handleOpenMedium }, /* backward compatiblity */
        { "closemedium",      USAGE_CLOSEMEDIUM,       handleCloseMedium },
        { "unregisterimage",  USAGE_CLOSEMEDIUM,       handleCloseMedium }, /* backward compatiblity */
        { "storageattach",    USAGE_STORAGEATTACH,     handleStorageAttach },
        { "storagectl",       USAGE_STORAGECONTROLLER, handleStorageController },
        { "showhdinfo",       USAGE_SHOWHDINFO,        handleShowHardDiskInfo },
        { "showvdiinfo",      USAGE_SHOWHDINFO,        handleShowHardDiskInfo }, /* backward compatiblity */
        { "getextradata",     USAGE_GETEXTRADATA,      handleGetExtraData },
        { "setextradata",     USAGE_SETEXTRADATA,      handleSetExtraData },
        { "setproperty",      USAGE_SETPROPERTY,       handleSetProperty },
        { "usbfilter",        USAGE_USBFILTER,         handleUSBFilter },
        { "sharedfolder",     USAGE_SHAREDFOLDER,      handleSharedFolder },
        { "vmstatistics",     USAGE_VM_STATISTICS,     handleVMStatistics },
#ifdef VBOX_WITH_GUEST_PROPS
        { "guestproperty",    USAGE_GUESTPROPERTY,     handleGuestProperty },
#endif
#ifdef VBOX_WITH_GUEST_CONTROL
        { "guestcontrol",     USAGE_GUESTCONTROL,      handleGuestControl },
#endif
        { "metrics",          USAGE_METRICS,           handleMetrics },
        { "import",           USAGE_IMPORTAPPLIANCE,   handleImportAppliance },
        { "export",           USAGE_EXPORTAPPLIANCE,   handleExportAppliance },
#ifdef VBOX_WITH_NETFLT
        { "hostonlyif",       USAGE_HOSTONLYIFS,       handleHostonlyIf },
#endif
        { "dhcpserver",       USAGE_DHCPSERVER,        handleDHCPServer},
        { NULL,               0,                       NULL }
    };

    int commandIndex;
    for (commandIndex = 0; s_commandHandlers[commandIndex].command != NULL; commandIndex++)
    {
        if (!strcmp(s_commandHandlers[commandIndex].command, argv[iCmd]))
        {
            handlerArg.argc = argc - iCmdArg;
            handlerArg.argv = &argv[iCmdArg];

            if (   fShowHelp
                || (   argc - iCmdArg == 0
                    && s_commandHandlers[commandIndex].help))
            {
                printUsage(s_commandHandlers[commandIndex].help, g_pStdOut);
                rc = 1; /* error */
            }
            else
                rc = s_commandHandlers[commandIndex].handler(&handlerArg);
            break;
        }
    }
    if (!s_commandHandlers[commandIndex].command)
        rc = errorSyntax(USAGE_ALL, "Invalid command '%s'", Utf8Str(argv[iCmd]).c_str());

    /* Although all handlers should always close the session if they open it,
     * we do it here just in case if some of the handlers contains a bug --
     * leaving the direct session not closed will turn the machine state to
     * Aborted which may have unwanted side effects like killing the saved
     * state file (if the machine was in the Saved state before). */
    session->UnlockMachine();

    EventQueue::getMainEventQueue()->processEventQueue(0);
    // end "all-stuff" scope
    ////////////////////////////////////////////////////////////////////////////
    } while (0);

    com::Shutdown();
#endif /* !VBOX_ONLY_DOCS */

    /*
     * Free converted argument vector
     */
    for (int i = iCmdArg; i < argc; i++)
        RTStrFree(argv[i]);

    return rc != 0;
}
