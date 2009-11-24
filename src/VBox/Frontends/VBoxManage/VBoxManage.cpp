/* $Id$ */
/** @file
 * VBoxManage - VirtualBox's command-line interface.
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
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
#ifndef VBOX_ONLY_DOCS
# include <VBox/com/com.h>
# include <VBox/com/string.h>
# include <VBox/com/Guid.h>
# include <VBox/com/array.h>
# include <VBox/com/ErrorInfo.h>
# include <VBox/com/errorprint.h>
# include <VBox/com/EventQueue.h>

# include <VBox/com/VirtualBox.h>

# include <vector>
# include <list>
#endif /* !VBOX_ONLY_DOCS */

#include <VBox/err.h>
#include <VBox/version.h>

#include <iprt/buildconfig.h>
#include <iprt/env.h>
#include <iprt/initterm.h>
#include <iprt/stream.h>
#include <iprt/string.h>

#include "VBoxManage.h"


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/*extern*/ bool g_fDetailedProgress = false;


#ifndef VBOX_ONLY_DOCS
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
        RTPrintf("0%%...");
        RTStrmFlush(g_pStdOut);
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

            if (    (ulCurrentPercent != ulLastPercent)
                 || (ulCurrentOperationPercent != ulLastOperationPercent)
               )
            {
                LONG lSecsRem;
                progress->COMGETTER(TimeRemaining)(&lSecsRem);

                RTPrintf("(%ld/%ld) %ls %ld%% => %ld%% (%d s remaining)\n", ulOperation + 1, cOperations, bstrOperationDescription.raw(), ulCurrentOperationPercent, ulCurrentPercent, lSecsRem);
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
                        RTPrintf("%ld%%...", curVal);
                        RTStrmFlush(g_pStdOut);
                    }
                }
                ulLastPercent = (ulCurrentPercent / 10) * 10;
            }
        }
        if (fCompleted)
            break;

        /* make sure the loop is not too tight */
        progress->WaitForCompletion(100);
    }

    /* complete the line. */
    LONG iRc = E_FAIL;
    if (SUCCEEDED(progress->COMGETTER(ResultCode)(&iRc)))
    {
        if (SUCCEEDED(iRc))
            RTPrintf("100%%\n");
        else
            RTPrintf("FAILED\n");
    }
    else
        RTPrintf("\n");
    RTStrmFlush(g_pStdOut);
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

    bool fShowLogo = true;
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
            showLogo();
            printUsage(USAGE_ALL);
            return 0;
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
            printUsage(USAGE_DUMPOPTS);
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
        showLogo();


#ifdef VBOX_ONLY_DOCS
    int rc = 0;
#else /* !VBOX_ONLY_DOCS */
    using namespace com;
    HRESULT rc = 0;

    rc = com::Initialize();
    if (FAILED(rc))
    {
        RTPrintf("ERROR: failed to initialize COM!\n");
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
        argv[i] = converted;
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
        RTPrintf("ERROR: failed to create the VirtualBox object!\n");
    else
    {
        rc = session.createInprocObject(CLSID_Session);
        if (FAILED(rc))
            RTPrintf("ERROR: failed to create a session object!\n");
    }

    if (FAILED(rc))
    {
        com::ErrorInfo info;
        if (!info.isFullAvailable() && !info.isBasicAvailable())
        {
            com::GluePrintRCMessage(rc);
            RTPrintf("Most likely, the VirtualBox COM server is not running or failed to start.\n");
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
        int (*handler)(HandlerArg *a);
    } s_commandHandlers[] =
    {
        { "internalcommands", handleInternalCommands },
        { "list",             handleList },
        { "showvminfo",       handleShowVMInfo },
        { "registervm",       handleRegisterVM },
        { "unregistervm",     handleUnregisterVM },
        { "createhd",         handleCreateHardDisk },
        { "createvdi",        handleCreateHardDisk }, /* backward compatiblity */
        { "modifyhd",         handleModifyHardDisk },
        { "modifyvdi",        handleModifyHardDisk }, /* backward compatiblity */
        { "clonehd",          handleCloneHardDisk },
        { "clonevdi",         handleCloneHardDisk }, /* backward compatiblity */
        { "addiscsidisk",     handleAddiSCSIDisk },
        { "createvm",         handleCreateVM },
        { "modifyvm",         handleModifyVM },
        { "startvm",          handleStartVM },
        { "controlvm",        handleControlVM },
        { "discardstate",     handleDiscardState },
        { "adoptstate",       handleAdoptdState },
        { "snapshot",         handleSnapshot },
        { "openmedium",       handleOpenMedium },
        { "registerimage",    handleOpenMedium }, /* backward compatiblity */
        { "closemedium",      handleCloseMedium },
        { "unregisterimage",  handleCloseMedium }, /* backward compatiblity */
        { "storageattach",    handleStorageAttach },
        { "storagectl",       handleStorageController },
        { "showhdinfo",       handleShowHardDiskInfo },
        { "showvdiinfo",      handleShowHardDiskInfo }, /* backward compatiblity */
        { "getextradata",     handleGetExtraData },
        { "setextradata",     handleSetExtraData },
        { "setproperty",      handleSetProperty },
        { "usbfilter",        handleUSBFilter },
        { "sharedfolder",     handleSharedFolder },
        { "vmstatistics",     handleVMStatistics },
#ifdef VBOX_WITH_GUEST_PROPS
        { "guestproperty",    handleGuestProperty },
#endif
        { "metrics",          handleMetrics },
        { "import",           handleImportAppliance },
        { "export",           handleExportAppliance },
#ifdef VBOX_WITH_NETFLT
        { "hostonlyif",       handleHostonlyIf },
#endif
        { "dhcpserver",       handleDHCPServer},
        { NULL,               NULL }
    };

    int commandIndex;
    for (commandIndex = 0; s_commandHandlers[commandIndex].command != NULL; commandIndex++)
    {
        if (!strcmp(s_commandHandlers[commandIndex].command, argv[iCmd]))
        {
            handlerArg.argc = argc - iCmdArg;
            handlerArg.argv = &argv[iCmdArg];

            rc = s_commandHandlers[commandIndex].handler(&handlerArg);
            break;
        }
    }
    if (!s_commandHandlers[commandIndex].command)
    {
        rc = errorSyntax(USAGE_ALL, "Invalid command '%s'", Utf8Str(argv[iCmd]).raw());
    }

    /* Although all handlers should always close the session if they open it,
     * we do it here just in case if some of the handlers contains a bug --
     * leaving the direct session not closed will turn the machine state to
     * Aborted which may have unwanted side effects like killing the saved
     * state file (if the machine was in the Saved state before). */
    session->Close();

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

