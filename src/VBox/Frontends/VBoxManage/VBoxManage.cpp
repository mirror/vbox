/* $Id$ */
/** @file
 * VBoxManage - VirtualBox's command-line interface.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
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
#include <iprt/ctype.h>
#include <iprt/initterm.h>
#include <iprt/path.h>
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
 * Print out progress on the console.
 *
 * This runs the main event queue every now and then to prevent piling up
 * unhandled things (which doesn't cause real problems, just makes things
 * react a little slower than in the ideal case).
 */
HRESULT showProgress(ComPtr<IProgress> progress)
{
    using namespace com;

    BOOL fCompleted = FALSE;
    ULONG ulCurrentPercent = 0;
    ULONG ulLastPercent = 0;

    ULONG ulLastOperationPercent = (ULONG)-1;

    ULONG ulLastOperation = (ULONG)-1;
    Bstr bstrOperationDescription;

    EventQueue::getMainEventQueue()->processEventQueue(0);

    ULONG cOperations = 1;
    HRESULT hrc = progress->COMGETTER(OperationCount)(&cOperations);
    if (FAILED(hrc))
    {
        RTStrmPrintf(g_pStdErr, "Progress object failure: %Rhrc\n", hrc);
        RTStrmFlush(g_pStdErr);
        return hrc;
    }

    /*
     * Note: Outputting the progress info to stderr (g_pStdErr) is intentional
     *       to not get intermixed with other (raw) stdout data which might get
     *       written in the meanwhile.
     */

    if (!g_fDetailedProgress)
    {
        RTStrmPrintf(g_pStdErr, "0%%...");
        RTStrmFlush(g_pStdErr);
    }

    /* setup signal handling if cancelable */
    bool fCanceledAlready = false;
    BOOL fCancelable;
    hrc = progress->COMGETTER(Cancelable)(&fCancelable);
    if (FAILED(hrc))
        fCancelable = FALSE;
    if (fCancelable)
    {
        signal(SIGINT,   showProgressSignalHandler);
#ifdef SIGBREAK
        signal(SIGBREAK, showProgressSignalHandler);
#endif
    }

    hrc = progress->COMGETTER(Completed(&fCompleted));
    while (SUCCEEDED(hrc))
    {
        progress->COMGETTER(Percent(&ulCurrentPercent));

        if (g_fDetailedProgress)
        {
            ULONG ulOperation = 1;
            hrc = progress->COMGETTER(Operation)(&ulOperation);
            if (FAILED(hrc))
                break;
            ULONG ulCurrentOperationPercent = 0;
            hrc = progress->COMGETTER(OperationPercent(&ulCurrentOperationPercent));
            if (FAILED(hrc))
                break;

            if (ulLastOperation != ulOperation)
            {
                hrc = progress->COMGETTER(OperationDescription(bstrOperationDescription.asOutParam()));
                if (FAILED(hrc))
                    break;
                ulLastPercent = (ULONG)-1;        // force print
                ulLastOperation = ulOperation;
            }

            if (    ulCurrentPercent != ulLastPercent
                 || ulCurrentOperationPercent != ulLastOperationPercent
               )
            {
                LONG lSecsRem = 0;
                progress->COMGETTER(TimeRemaining)(&lSecsRem);

                RTStrmPrintf(g_pStdErr, "(%u/%u) %ls %02u%% => %02u%% (%d s remaining)\n", ulOperation + 1, cOperations, bstrOperationDescription.raw(), ulCurrentOperationPercent, ulCurrentPercent, lSecsRem);
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
                        RTStrmPrintf(g_pStdErr, "%u%%...", curVal);
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

        EventQueue::getMainEventQueue()->processEventQueue(0);
        hrc = progress->COMGETTER(Completed(&fCompleted));
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
    hrc = progress->COMGETTER(ResultCode)(&iRc);
    if (SUCCEEDED(hrc))
    {
        if (SUCCEEDED(iRc))
            RTStrmPrintf(g_pStdErr, "100%%\n");
        else if (g_fCanceled)
            RTStrmPrintf(g_pStdErr, "CANCELED\n");
        else
        {
            if (!g_fDetailedProgress)
                RTStrmPrintf(g_pStdErr, "\n");
            RTStrmPrintf(g_pStdErr, "Progress state: %Rhrc\n", iRc);
        }
        hrc = iRc;
    }
    else
    {
        if (!g_fDetailedProgress)
            RTStrmPrintf(g_pStdErr, "\n");
        RTStrmPrintf(g_pStdErr, "Progress object failure: %Rhrc\n", hrc);
    }
    RTStrmFlush(g_pStdErr);
    return hrc;
}

#ifdef RT_OS_WINDOWS
// Required for ATL
static CComModule _Module;
#endif

#endif /* !VBOX_ONLY_DOCS */


#ifndef VBOX_ONLY_DOCS
RTEXITCODE readPasswordFile(const char *pszFilename, com::Utf8Str *pPasswd)
{
    size_t cbFile;
    char szPasswd[512];
    int vrc = VINF_SUCCESS;
    RTEXITCODE rcExit = RTEXITCODE_SUCCESS;
    bool fStdIn = !strcmp(pszFilename, "stdin");
    PRTSTREAM pStrm;
    if (!fStdIn)
        vrc = RTStrmOpen(pszFilename, "r", &pStrm);
    else
        pStrm = g_pStdIn;
    if (RT_SUCCESS(vrc))
    {
        vrc = RTStrmReadEx(pStrm, szPasswd, sizeof(szPasswd)-1, &cbFile);
        if (RT_SUCCESS(vrc))
        {
            if (cbFile >= sizeof(szPasswd)-1)
                rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "Provided password in file '%s' is too long", pszFilename);
            else
            {
                unsigned i;
                for (i = 0; i < cbFile && !RT_C_IS_CNTRL(szPasswd[i]); i++)
                    ;
                szPasswd[i] = '\0';
                *pPasswd = szPasswd;
            }
        }
        else
            rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "Cannot read password from file '%s': %Rrc", pszFilename, vrc);
        if (!fStdIn)
            RTStrmClose(pStrm);
    }
    else
        rcExit = RTMsgErrorExit(RTEXITCODE_FAILURE, "Cannot open password file '%s' (%Rrc)", pszFilename, vrc);

    return rcExit;
}

static RTEXITCODE settingsPasswordFile(ComPtr<IVirtualBox> virtualBox, const char *pszFilename)
{
    com::Utf8Str passwd;
    RTEXITCODE rcExit = readPasswordFile(pszFilename, &passwd);
    if (rcExit == RTEXITCODE_SUCCESS)
    {
        int rc;
        CHECK_ERROR(virtualBox, SetSettingsSecret(com::Bstr(passwd).raw()));
        if (FAILED(rc))
            rcExit = RTEXITCODE_FAILURE;
    }

    return rcExit;
}
#endif

int main(int argc, char *argv[])
{
    /*
     * Before we do anything, init the runtime without loading
     * the support driver.
     */
    RTR3InitExe(argc, &argv, 0);

    /*
     * Parse the global options
     */
    bool fShowLogo = false;
    bool fShowHelp = false;
    int  iCmd      = 1;
    int  iCmdArg;
    const char *g_pszSettingsPw = NULL;
    const char *g_pszSettingsPwFile = NULL;

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
        else if (   !strcmp(argv[i], "--detailed-progress")
                 || !strcmp(argv[i], "-d"))
        {
            /* detailed progress report */
            g_fDetailedProgress = true;
            iCmd++;
        }
        else if (!strcmp(argv[i], "--settingspw"))
        {
            if (i >= argc-1)
                return RTMsgErrorExit(RTEXITCODE_FAILURE,
                                      "Password expected");
            /* password for certain settings */
            g_pszSettingsPw = argv[i+1];
            iCmd += 2;
        }
        else if (!strcmp(argv[i], "--settingspwfile"))
        {
            if (i >= argc-1)
                return RTMsgErrorExit(RTEXITCODE_FAILURE,
                                      "No password file specified");
            g_pszSettingsPwFile = argv[i+1];
            iCmd += 2;
        }
        else
            break;
    }

    iCmdArg = iCmd + 1;

    if (fShowLogo)
        showLogo(g_pStdOut);


#ifndef VBOX_ONLY_DOCS
    /*
     * Initialize COM.
     */
    using namespace com;
    HRESULT hrc = com::Initialize();
# ifdef VBOX_WITH_XPCOM
    if (hrc == NS_ERROR_FILE_ACCESS_DENIED)
    {
        char szHome[RTPATH_MAX] = "";
        com::GetVBoxUserHomeDirectory(szHome, sizeof(szHome));
        return RTMsgErrorExit(RTEXITCODE_FAILURE,
               "Failed to initialize COM because the global settings directory '%s' is not accessible!", szHome);
    }
# endif
    if (FAILED(hrc))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Failed to initialize COM!");

    RTEXITCODE rcExit = RTEXITCODE_FAILURE;
    do
    {
    ///////////////////////////////////////////////////////////////////////////
    // scopes all the stuff till shutdown
        /*
         * convertfromraw: does not need a VirtualBox instantiation.
         */
        if (argc >= iCmdArg && (   !strcmp(argv[iCmd], "convertfromraw")
                                || !strcmp(argv[iCmd], "convertdd")))
        {
            rcExit = handleConvertFromRaw(argc - iCmdArg, argv + iCmdArg);
            break;
        }

        /*
         * Get the remote VirtualBox object and create a local session object.
         */
        ComPtr<IVirtualBox> virtualBox;
        ComPtr<ISession> session;

        hrc = virtualBox.createLocalObject(CLSID_VirtualBox);
        if (FAILED(hrc))
            RTMsgError("Failed to create the VirtualBox object!");
        else
        {
            hrc = session.createInprocObject(CLSID_Session);
            if (FAILED(hrc))
                RTMsgError("Failed to create a session object!");
        }
        if (FAILED(hrc))
        {
            com::ErrorInfo info;
            if (!info.isFullAvailable() && !info.isBasicAvailable())
            {
                com::GluePrintRCMessage(hrc);
                RTMsgError("Most likely, the VirtualBox COM server is not running or failed to start.");
            }
            else
                com::GluePrintErrorInfo(info);
            break;
        }

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
            { "clonevm",          USAGE_CLONEVM,           handleCloneVM },
            { "createhd",         USAGE_CREATEHD,          handleCreateHardDisk },
            { "createvdi",        USAGE_CREATEHD,          handleCreateHardDisk }, /* backward compatibility */
            { "modifyhd",         USAGE_MODIFYHD,          handleModifyHardDisk },
            { "modifyvdi",        USAGE_MODIFYHD,          handleModifyHardDisk }, /* backward compatibility */
            { "clonehd",          USAGE_CLONEHD,           handleCloneHardDisk },
            { "clonevdi",         USAGE_CLONEHD,           handleCloneHardDisk }, /* backward compatibility */
            { "createvm",         USAGE_CREATEVM,          handleCreateVM },
            { "modifyvm",         USAGE_MODIFYVM,          handleModifyVM },
            { "startvm",          USAGE_STARTVM,           handleStartVM },
            { "controlvm",        USAGE_CONTROLVM,         handleControlVM },
            { "discardstate",     USAGE_DISCARDSTATE,      handleDiscardState },
            { "adoptstate",       USAGE_ADOPTSTATE,        handleAdoptState },
            { "snapshot",         USAGE_SNAPSHOT,          handleSnapshot },
            { "closemedium",      USAGE_CLOSEMEDIUM,       handleCloseMedium },
            { "storageattach",    USAGE_STORAGEATTACH,     handleStorageAttach },
            { "storagectl",       USAGE_STORAGECONTROLLER, handleStorageController },
            { "showhdinfo",       USAGE_SHOWHDINFO,        handleShowHardDiskInfo },
            { "showvdiinfo",      USAGE_SHOWHDINFO,        handleShowHardDiskInfo }, /* backward compatibility */
            { "getextradata",     USAGE_GETEXTRADATA,      handleGetExtraData },
            { "setextradata",     USAGE_SETEXTRADATA,      handleSetExtraData },
            { "setproperty",      USAGE_SETPROPERTY,       handleSetProperty },
            { "usbfilter",        USAGE_USBFILTER,         handleUSBFilter },
            { "sharedfolder",     USAGE_SHAREDFOLDER,      handleSharedFolder },
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
            { "extpack",          USAGE_EXTPACK,           handleExtPack},
            { "bandwidthctl",     USAGE_BANDWIDTHCONTROL,  handleBandwidthControl},
            { "debugvm",          USAGE_DEBUGVM,           handleDebugVM},
            { NULL,               0,                       NULL }
        };

        if (g_pszSettingsPw)
        {
            int rc;
            CHECK_ERROR(virtualBox, SetSettingsSecret(Bstr(g_pszSettingsPw).raw()));
            if (FAILED(rc))
            {
                rcExit = RTEXITCODE_FAILURE;
                break;
            }
        }
        else if (g_pszSettingsPwFile)
        {
            rcExit = settingsPasswordFile(virtualBox, g_pszSettingsPwFile);
            if (rcExit != RTEXITCODE_SUCCESS)
                break;
        }

        HandlerArg  handlerArg = { 0, NULL, virtualBox, session };
        int         commandIndex;
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
                    rcExit = RTEXITCODE_FAILURE; /* error */
                }
                else
                    rcExit = (RTEXITCODE)s_commandHandlers[commandIndex].handler(&handlerArg); /** @todo Change to return RTEXITCODE. */
                break;
            }
        }
        if (!s_commandHandlers[commandIndex].command)
        {
            /* Help topics. */
            if (fShowHelp && !strcmp(argv[iCmd], "commands"))
            {
                RTPrintf("commands:\n");
                for (unsigned i = 0; i < RT_ELEMENTS(s_commandHandlers) - 1; i++)
                    if (   i ==  0  /* skip backwards compatibility entries */
                        || s_commandHandlers[i].help != s_commandHandlers[i - 1].help)
                        RTPrintf("    %s\n", s_commandHandlers[i].command);
            }
            else
                rcExit = errorSyntax(USAGE_ALL, "Invalid command '%s'", Utf8Str(argv[iCmd]).c_str());
        }

        /* Although all handlers should always close the session if they open it,
         * we do it here just in case if some of the handlers contains a bug --
         * leaving the direct session not closed will turn the machine state to
         * Aborted which may have unwanted side effects like killing the saved
         * state file (if the machine was in the Saved state before). */
        session->UnlockMachine();

        EventQueue::getMainEventQueue()->processEventQueue(0);

    // end "all-stuff" scope
    ///////////////////////////////////////////////////////////////////////////
    } while (0);

    com::Shutdown();

    return rcExit;
#else  /* VBOX_ONLY_DOCS */
    return RTEXITCODE_SUCCESS;
#endif /* VBOX_ONLY_DOCS */
}
