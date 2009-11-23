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
#include <VBox/com/com.h>
#include <VBox/com/string.h>
#include <VBox/com/Guid.h>
#include <VBox/com/array.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint.h>
#include <VBox/com/EventQueue.h>

#include <VBox/com/VirtualBox.h>

#include <vector>
#include <list>
#endif /* !VBOX_ONLY_DOCS */

#include <iprt/asm.h>
#include <iprt/buildconfig.h>
#include <iprt/cidr.h>
#include <iprt/ctype.h>
#include <iprt/dir.h>
#include <iprt/env.h>
#include <VBox/err.h>
#include <iprt/file.h>
#include <iprt/initterm.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/stdarg.h>
#include <iprt/thread.h>
#include <iprt/uuid.h>
#include <iprt/getopt.h>
#include <iprt/ctype.h>
#include <VBox/version.h>
#include <VBox/log.h>

#include "VBoxManage.h"

#ifndef VBOX_ONLY_DOCS
using namespace com;

/** command handler type */
typedef int (*PFNHANDLER)(HandlerArg *a);

#endif /* !VBOX_ONLY_DOCS */

////////////////////////////////////////////////////////////////////////////////
//
// global variables
//
////////////////////////////////////////////////////////////////////////////////

/*extern*/ bool g_fDetailedProgress = false;

////////////////////////////////////////////////////////////////////////////////
//
// functions
//
////////////////////////////////////////////////////////////////////////////////

#ifndef VBOX_ONLY_DOCS
/**
 * Print out progress on the console
 */
HRESULT showProgress(ComPtr<IProgress> progress)
{
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
            if (((ulCurrentPercent / 10) > (ulLastPercent / 10)))
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

void showLogo(void)
{
    static bool fShown; /* show only once */

    if (!fShown)
    {
        RTPrintf("VirtualBox Command Line Management Interface Version "
                 VBOX_VERSION_STRING  "\n"
                 "(C) 2005-2009 Sun Microsystems, Inc.\n"
                 "All rights reserved.\n"
                 "\n");
        fShown = true;
    }
}

#ifndef VBOX_ONLY_DOCS

static int handleRegisterVM(HandlerArg *a)
{
    HRESULT rc;

    if (a->argc != 1)
        return errorSyntax(USAGE_REGISTERVM, "Incorrect number of parameters");

    ComPtr<IMachine> machine;
    /** @todo Ugly hack to get both the API interpretation of relative paths
     * and the client's interpretation of relative paths. Remove after the API
     * has been redesigned. */
    rc = a->virtualBox->OpenMachine(Bstr(a->argv[0]), machine.asOutParam());
    if (rc == VBOX_E_FILE_ERROR)
    {
        char szVMFileAbs[RTPATH_MAX] = "";
        int vrc = RTPathAbs(a->argv[0], szVMFileAbs, sizeof(szVMFileAbs));
        if (RT_FAILURE(vrc))
        {
            RTPrintf("Cannot convert filename \"%s\" to absolute path\n", a->argv[0]);
            return 1;
        }
        CHECK_ERROR(a->virtualBox, OpenMachine(Bstr(szVMFileAbs), machine.asOutParam()));
    }
    else if (FAILED(rc))
        CHECK_ERROR(a->virtualBox, OpenMachine(Bstr(a->argv[0]), machine.asOutParam()));
    if (SUCCEEDED(rc))
    {
        ASSERT(machine);
        CHECK_ERROR(a->virtualBox, RegisterMachine(machine));
    }
    return SUCCEEDED(rc) ? 0 : 1;
}

static const RTGETOPTDEF g_aUnregisterVMOptions[] =
{
    { "--delete",       'd', RTGETOPT_REQ_NOTHING },
    { "-delete",        'd', RTGETOPT_REQ_NOTHING },    // deprecated
};

static int handleUnregisterVM(HandlerArg *a)
{
    HRESULT rc;
    const char *VMName = NULL;
    bool fDelete = false;

    int c;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    // start at 0 because main() has hacked both the argc and argv given to us
    RTGetOptInit(&GetState, a->argc, a->argv, g_aUnregisterVMOptions, RT_ELEMENTS(g_aUnregisterVMOptions), 0, 0 /* fFlags */);
    while ((c = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (c)
        {
            case 'd':   // --delete
                fDelete = true;
                break;

            case VINF_GETOPT_NOT_OPTION:
                if (!VMName)
                    VMName = ValueUnion.psz;
                else
                    return errorSyntax(USAGE_UNREGISTERVM, "Invalid parameter '%s'", ValueUnion.psz);
                break;

            default:
                if (c > 0)
                {
                    if (RT_C_IS_PRINT(c))
                        return errorSyntax(USAGE_UNREGISTERVM, "Invalid option -%c", c);
                    else
                        return errorSyntax(USAGE_UNREGISTERVM, "Invalid option case %i", c);
                }
                else if (c == VERR_GETOPT_UNKNOWN_OPTION)
                    return errorSyntax(USAGE_UNREGISTERVM, "unknown option: %s\n", ValueUnion.psz);
                else if (ValueUnion.pDef)
                    return errorSyntax(USAGE_UNREGISTERVM, "%s: %Rrs", ValueUnion.pDef->pszLong, c);
                else
                    return errorSyntax(USAGE_UNREGISTERVM, "error: %Rrs", c);
        }
    }

    /* check for required options */
    if (!VMName)
        return errorSyntax(USAGE_UNREGISTERVM, "VM name required");

    ComPtr<IMachine> machine;
    /* assume it's a UUID */
    rc = a->virtualBox->GetMachine(Guid(VMName).toUtf16(), machine.asOutParam());
    if (FAILED(rc) || !machine)
    {
        /* must be a name */
        CHECK_ERROR(a->virtualBox, FindMachine(Bstr(VMName), machine.asOutParam()));
    }
    if (machine)
    {
        Bstr uuid;
        machine->COMGETTER(Id)(uuid.asOutParam());
        machine = NULL;
        CHECK_ERROR(a->virtualBox, UnregisterMachine(uuid, machine.asOutParam()));
        if (SUCCEEDED(rc) && machine && fDelete)
            CHECK_ERROR(machine, DeleteSettings());
    }
    return SUCCEEDED(rc) ? 0 : 1;
}

static int handleCreateVM(HandlerArg *a)
{
    HRESULT rc;
    Bstr baseFolder;
    Bstr settingsFile;
    Bstr name;
    Bstr osTypeId;
    RTUUID id;
    bool fRegister = false;

    RTUuidClear(&id);
    for (int i = 0; i < a->argc; i++)
    {
        if (   !strcmp(a->argv[i], "--basefolder")
            || !strcmp(a->argv[i], "-basefolder"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            baseFolder = a->argv[i];
        }
        else if (   !strcmp(a->argv[i], "--settingsfile")
                 || !strcmp(a->argv[i], "-settingsfile"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            settingsFile = a->argv[i];
        }
        else if (   !strcmp(a->argv[i], "--name")
                 || !strcmp(a->argv[i], "-name"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            name = a->argv[i];
        }
        else if (   !strcmp(a->argv[i], "--ostype")
                 || !strcmp(a->argv[i], "-ostype"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            osTypeId = a->argv[i];
        }
        else if (   !strcmp(a->argv[i], "--uuid")
                 || !strcmp(a->argv[i], "-uuid"))
        {
            if (a->argc <= i + 1)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            i++;
            if (RT_FAILURE(RTUuidFromStr(&id, a->argv[i])))
                return errorArgument("Invalid UUID format %s\n", a->argv[i]);
        }
        else if (   !strcmp(a->argv[i], "--register")
                 || !strcmp(a->argv[i], "-register"))
        {
            fRegister = true;
        }
        else
            return errorSyntax(USAGE_CREATEVM, "Invalid parameter '%s'", Utf8Str(a->argv[i]).raw());
    }
    if (!name)
        return errorSyntax(USAGE_CREATEVM, "Parameter --name is required");

    if (!!baseFolder && !!settingsFile)
        return errorSyntax(USAGE_CREATEVM, "Either --basefolder or --settingsfile must be specified");

    do
    {
        ComPtr<IMachine> machine;

        if (!settingsFile)
            CHECK_ERROR_BREAK(a->virtualBox,
                CreateMachine(name, osTypeId, baseFolder, Guid(id).toUtf16(), machine.asOutParam()));
        else
            CHECK_ERROR_BREAK(a->virtualBox,
                CreateLegacyMachine(name, osTypeId, settingsFile, Guid(id).toUtf16(), machine.asOutParam()));

        CHECK_ERROR_BREAK(machine, SaveSettings());
        if (fRegister)
        {
            CHECK_ERROR_BREAK(a->virtualBox, RegisterMachine(machine));
        }
        Bstr uuid;
        CHECK_ERROR_BREAK(machine, COMGETTER(Id)(uuid.asOutParam()));
        CHECK_ERROR_BREAK(machine, COMGETTER(SettingsFilePath)(settingsFile.asOutParam()));
        RTPrintf("Virtual machine '%ls' is created%s.\n"
                 "UUID: %s\n"
                 "Settings file: '%ls'\n",
                 name.raw(), fRegister ? " and registered" : "",
                 Utf8Str(uuid).raw(), settingsFile.raw());
    }
    while (0);

    return SUCCEEDED(rc) ? 0 : 1;
}

/**
 * Parses a number.
 *
 * @returns Valid number on success.
 * @returns 0 if invalid number. All necesary bitching has been done.
 * @param   psz     Pointer to the nic number.
 */
unsigned parseNum(const char *psz, unsigned cMaxNum, const char *name)
{
    uint32_t u32;
    char *pszNext;
    int rc = RTStrToUInt32Ex(psz, &pszNext, 10, &u32);
    if (    RT_SUCCESS(rc)
        &&  *pszNext == '\0'
        &&  u32 >= 1
        &&  u32 <= cMaxNum)
        return (unsigned)u32;
    errorArgument("Invalid %s number '%s'", name, psz);
    return 0;
}


/** @todo refine this after HDD changes; MSC 8.0/64 has trouble with handleModifyVM.  */
#if defined(_MSC_VER)
# pragma optimize("", on)
#endif

static const RTGETOPTDEF g_aStartVMOptions[] =
{
    { "--type",         't', RTGETOPT_REQ_STRING },
    { "-type",          't', RTGETOPT_REQ_STRING },     // deprecated
};

static int handleStartVM(HandlerArg *a)
{
    HRESULT rc;
    const char *VMName = NULL;
    Bstr sessionType = "gui";

    int c;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    // start at 0 because main() has hacked both the argc and argv given to us
    RTGetOptInit(&GetState, a->argc, a->argv, g_aStartVMOptions, RT_ELEMENTS(g_aStartVMOptions), 0, 0 /* fFlags */);
    while ((c = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (c)
        {
            case 't':   // --type
                if (!RTStrICmp(ValueUnion.psz, "gui"))
                {
                    sessionType = "gui";
                }
#ifdef VBOX_WITH_VBOXSDL
                else if (!RTStrICmp(ValueUnion.psz, "sdl"))
                {
                    sessionType = "sdl";
                }
#endif
#ifdef VBOX_WITH_VRDP
                else if (!RTStrICmp(ValueUnion.psz, "vrdp"))
                {
                    sessionType = "vrdp";
                }
#endif
#ifdef VBOX_WITH_HEADLESS
                else if (!RTStrICmp(ValueUnion.psz, "capture"))
                {
                    sessionType = "capture";
                }
                else if (!RTStrICmp(ValueUnion.psz, "headless"))
                {
                    sessionType = "headless";
                }
#endif
                else
                    return errorArgument("Invalid session type '%s'", ValueUnion.psz);
                break;

            case VINF_GETOPT_NOT_OPTION:
                if (!VMName)
                    VMName = ValueUnion.psz;
                else
                    return errorSyntax(USAGE_STARTVM, "Invalid parameter '%s'", ValueUnion.psz);
                break;

            default:
                if (c > 0)
                {
                    if (RT_C_IS_PRINT(c))
                        return errorSyntax(USAGE_STARTVM, "Invalid option -%c", c);
                    else
                        return errorSyntax(USAGE_STARTVM, "Invalid option case %i", c);
                }
                else if (c == VERR_GETOPT_UNKNOWN_OPTION)
                    return errorSyntax(USAGE_STARTVM, "unknown option: %s\n", ValueUnion.psz);
                else if (ValueUnion.pDef)
                    return errorSyntax(USAGE_STARTVM, "%s: %Rrs", ValueUnion.pDef->pszLong, c);
                else
                    return errorSyntax(USAGE_STARTVM, "error: %Rrs", c);
        }
    }

    /* check for required options */
    if (!VMName)
        return errorSyntax(USAGE_STARTVM, "VM name required");

    ComPtr<IMachine> machine;
    /* assume it's a UUID */
    rc = a->virtualBox->GetMachine(Guid(VMName).toUtf16(), machine.asOutParam());
    if (FAILED(rc) || !machine)
    {
        /* must be a name */
        CHECK_ERROR(a->virtualBox, FindMachine(Bstr(VMName), machine.asOutParam()));
    }
    if (machine)
    {
        Bstr uuid;
        machine->COMGETTER(Id)(uuid.asOutParam());


        Bstr env;
#if defined(RT_OS_LINUX) || defined(RT_OS_SOLARIS)
        /* make sure the VM process will start on the same display as VBoxManage */
        Utf8Str str;
        const char *pszDisplay = RTEnvGet("DISPLAY");
        if (pszDisplay)
            str = Utf8StrFmt("DISPLAY=%s\n", pszDisplay);
        const char *pszXAuth = RTEnvGet("XAUTHORITY");
        if (pszXAuth)
            str.append(Utf8StrFmt("XAUTHORITY=%s\n", pszXAuth));
        env = str;
#endif
        ComPtr<IProgress> progress;
        CHECK_ERROR_RET(a->virtualBox, OpenRemoteSession(a->session, uuid, sessionType,
                                                         env, progress.asOutParam()), rc);
        RTPrintf("Waiting for the remote session to open...\n");
        CHECK_ERROR_RET(progress, WaitForCompletion (-1), 1);

        BOOL completed;
        CHECK_ERROR_RET(progress, COMGETTER(Completed)(&completed), rc);
        ASSERT(completed);

        LONG iRc;
        CHECK_ERROR_RET(progress, COMGETTER(ResultCode)(&iRc), rc);
        if (FAILED(iRc))
        {
            ComPtr <IVirtualBoxErrorInfo> errorInfo;
            CHECK_ERROR_RET(progress, COMGETTER(ErrorInfo)(errorInfo.asOutParam()), 1);
            ErrorInfo info (errorInfo);
            com::GluePrintErrorInfo(info);
        }
        else
        {
            RTPrintf("Remote session has been successfully opened.\n");
        }
    }

    /* it's important to always close sessions */
    a->session->Close();

    return SUCCEEDED(rc) ? 0 : 1;
}

static int handleControlVM(HandlerArg *a)
{
    HRESULT rc;

    if (a->argc < 2)
        return errorSyntax(USAGE_CONTROLVM, "Not enough parameters");

    /* try to find the given machine */
    ComPtr <IMachine> machine;
    Bstr machineuuid (a->argv[0]);
    if (!Guid(machineuuid).isEmpty())
    {
        CHECK_ERROR(a->virtualBox, GetMachine(machineuuid, machine.asOutParam()));
    }
    else
    {
        CHECK_ERROR(a->virtualBox, FindMachine(machineuuid, machine.asOutParam()));
        if (SUCCEEDED (rc))
            machine->COMGETTER(Id)(machineuuid.asOutParam());
    }
    if (FAILED (rc))
        return 1;

    /* open a session for the VM */
    CHECK_ERROR_RET(a->virtualBox, OpenExistingSession(a->session, machineuuid), 1);

    do
    {
        /* get the associated console */
        ComPtr<IConsole> console;
        CHECK_ERROR_BREAK(a->session, COMGETTER(Console)(console.asOutParam()));
        /* ... and session machine */
        ComPtr<IMachine> sessionMachine;
        CHECK_ERROR_BREAK(a->session, COMGETTER(Machine)(sessionMachine.asOutParam()));

        /* which command? */
        if (!strcmp(a->argv[1], "pause"))
        {
            CHECK_ERROR_BREAK(console, Pause());
        }
        else if (!strcmp(a->argv[1], "resume"))
        {
            CHECK_ERROR_BREAK(console, Resume());
        }
        else if (!strcmp(a->argv[1], "reset"))
        {
            CHECK_ERROR_BREAK(console, Reset());
        }
        else if (!strcmp(a->argv[1], "poweroff"))
        {
            ComPtr<IProgress> progress;
            CHECK_ERROR_BREAK(console, PowerDown(progress.asOutParam()));

            rc = showProgress(progress);
            if (FAILED(rc))
            {
                com::ProgressErrorInfo info(progress);
                if (info.isBasicAvailable())
                {
                    RTPrintf("Error: failed to power off machine. Error message: %lS\n", info.getText().raw());
                }
                else
                {
                    RTPrintf("Error: failed to power off machine. No error message available!\n");
                }
            }
        }
        else if (!strcmp(a->argv[1], "savestate"))
        {
            ComPtr<IProgress> progress;
            CHECK_ERROR_BREAK(console, SaveState(progress.asOutParam()));

            rc = showProgress(progress);
            if (FAILED(rc))
            {
                com::ProgressErrorInfo info(progress);
                if (info.isBasicAvailable())
                {
                    RTPrintf("Error: failed to save machine state. Error message: %lS\n", info.getText().raw());
                }
                else
                {
                    RTPrintf("Error: failed to save machine state. No error message available!\n");
                }
            }
        }
        else if (!strcmp(a->argv[1], "acpipowerbutton"))
        {
            CHECK_ERROR_BREAK(console, PowerButton());
        }
        else if (!strcmp(a->argv[1], "acpisleepbutton"))
        {
            CHECK_ERROR_BREAK(console, SleepButton());
        }
        else if (!strcmp(a->argv[1], "injectnmi"))
        {
            /* get the machine debugger. */
            ComPtr <IMachineDebugger> debugger;
            CHECK_ERROR_BREAK(console, COMGETTER(Debugger)(debugger.asOutParam()));
            CHECK_ERROR_BREAK(debugger, InjectNMI());
        }
        else if (!strcmp(a->argv[1], "keyboardputscancode"))
        {
            ComPtr<IKeyboard> keyboard;
            CHECK_ERROR_BREAK(console, COMGETTER(Keyboard)(keyboard.asOutParam()));

            if (a->argc <= 1 + 1)
            {
                errorArgument("Missing argument to '%s'. Expected IBM PC AT set 2 keyboard scancode(s) as hex byte(s).", a->argv[1]);
                rc = E_FAIL;
                break;
            }

            /* Arbitrary restrict the length of a sequence of scancodes to 1024. */
            LONG alScancodes[1024];
            int cScancodes = 0;

            /* Process the command line. */
            int i;
            for (i = 1 + 1; i < a->argc && cScancodes < (int)RT_ELEMENTS(alScancodes); i++, cScancodes++)
            {
                if (   RT_C_IS_XDIGIT (a->argv[i][0])
                    && RT_C_IS_XDIGIT (a->argv[i][1])
                    && a->argv[i][2] == 0)
                {
                    uint8_t u8Scancode;
                    int rc = RTStrToUInt8Ex(a->argv[i], NULL, 16, &u8Scancode);
                    if (RT_FAILURE (rc))
                    {
                        RTPrintf("Error: converting '%s' returned %Rrc!\n", a->argv[i], rc);
                        rc = E_FAIL;
                        break;
                    }

                    alScancodes[cScancodes] = u8Scancode;
                }
                else
                {
                    RTPrintf("Error: '%s' is not a hex byte!\n", a->argv[i]);
                    rc = E_FAIL;
                    break;
                }
            }

            if (FAILED(rc))
                break;

            if (   cScancodes == RT_ELEMENTS(alScancodes)
                && i < a->argc)
            {
                RTPrintf("Error: too many scancodes, maximum %d allowed!\n", RT_ELEMENTS(alScancodes));
                rc = E_FAIL;
                break;
            }

            /* Send scancodes to the VM.
             * Note: 'PutScancodes' did not work here. Only the first scancode was transmitted.
             */
            for (i = 0; i < cScancodes; i++)
            {
                CHECK_ERROR_BREAK(keyboard, PutScancode(alScancodes[i]));
                RTPrintf("Scancode[%d]: 0x%02X\n", i, alScancodes[i]);
            }
        }
        else if (!strncmp(a->argv[1], "setlinkstate", 12))
        {
            /* Get the number of network adapters */
            ULONG NetworkAdapterCount = 0;
            ComPtr <ISystemProperties> info;
            CHECK_ERROR_BREAK(a->virtualBox, COMGETTER(SystemProperties)(info.asOutParam()));
            CHECK_ERROR_BREAK(info, COMGETTER(NetworkAdapterCount)(&NetworkAdapterCount));

            unsigned n = parseNum(&a->argv[1][12], NetworkAdapterCount, "NIC");
            if (!n)
            {
                rc = E_FAIL;
                break;
            }
            if (a->argc <= 1 + 1)
            {
                errorArgument("Missing argument to '%s'", a->argv[1]);
                rc = E_FAIL;
                break;
            }
            /* get the corresponding network adapter */
            ComPtr<INetworkAdapter> adapter;
            CHECK_ERROR_BREAK(sessionMachine, GetNetworkAdapter(n - 1, adapter.asOutParam()));
            if (adapter)
            {
                if (!strcmp(a->argv[2], "on"))
                {
                    CHECK_ERROR_BREAK(adapter, COMSETTER(CableConnected)(TRUE));
                }
                else if (!strcmp(a->argv[2], "off"))
                {
                    CHECK_ERROR_BREAK(adapter, COMSETTER(CableConnected)(FALSE));
                }
                else
                {
                    errorArgument("Invalid link state '%s'", Utf8Str(a->argv[2]).raw());
                    rc = E_FAIL;
                    break;
                }
            }
        }
#ifdef VBOX_DYNAMIC_NET_ATTACH
        /* here the order in which strncmp is called is important
         * cause nictracefile can be very well compared with
         * nictrace and nic and thus everything will always fail
         * if the order is changed
         */
        else if (!strncmp(a->argv[1], "nictracefile", 12))
        {
            /* Get the number of network adapters */
            ULONG NetworkAdapterCount = 0;
            ComPtr <ISystemProperties> info;
            CHECK_ERROR_BREAK(a->virtualBox, COMGETTER(SystemProperties)(info.asOutParam()));
            CHECK_ERROR_BREAK(info, COMGETTER(NetworkAdapterCount)(&NetworkAdapterCount));

            unsigned n = parseNum(&a->argv[1][12], NetworkAdapterCount, "NIC");
            if (!n)
            {
                rc = E_FAIL;
                break;
            }
            if (a->argc <= 2)
            {
                errorArgument("Missing argument to '%s'", a->argv[1]);
                rc = E_FAIL;
                break;
            }

            /* get the corresponding network adapter */
            ComPtr<INetworkAdapter> adapter;
            CHECK_ERROR_BREAK(sessionMachine, GetNetworkAdapter(n - 1, adapter.asOutParam()));
            if (adapter)
            {
                BOOL fEnabled;
                adapter->COMGETTER(Enabled)(&fEnabled);
                if (fEnabled)
                {
                    if (a->argv[2])
                    {
                        CHECK_ERROR_RET(adapter, COMSETTER(TraceFile)(Bstr(a->argv[2])), 1);
                    }
                    else
                    {
                        errorArgument("Invalid filename or filename not specified for NIC %lu", n);
                        rc = E_FAIL;
                        break;
                    }
                }
                else
                {
                    RTPrintf("The NIC %d is currently disabled and thus can't change its tracefile\n", n);
                }
            }
        }
        else if (!strncmp(a->argv[1], "nictrace", 8))
        {
            /* Get the number of network adapters */
            ULONG NetworkAdapterCount = 0;
            ComPtr <ISystemProperties> info;
            CHECK_ERROR_BREAK(a->virtualBox, COMGETTER(SystemProperties)(info.asOutParam()));
            CHECK_ERROR_BREAK(info, COMGETTER(NetworkAdapterCount)(&NetworkAdapterCount));

            unsigned n = parseNum(&a->argv[1][8], NetworkAdapterCount, "NIC");
            if (!n)
            {
                rc = E_FAIL;
                break;
            }
            if (a->argc <= 2)
            {
                errorArgument("Missing argument to '%s'", a->argv[1]);
                rc = E_FAIL;
                break;
            }

            /* get the corresponding network adapter */
            ComPtr<INetworkAdapter> adapter;
            CHECK_ERROR_BREAK(sessionMachine, GetNetworkAdapter(n - 1, adapter.asOutParam()));
            if (adapter)
            {
                BOOL fEnabled;
                adapter->COMGETTER(Enabled)(&fEnabled);
                if (fEnabled)
                {
                    if (!strcmp(a->argv[2], "on"))
                    {
                        CHECK_ERROR_RET(adapter, COMSETTER(TraceEnabled)(TRUE), 1);
                    }
                    else if (!strcmp(a->argv[2], "off"))
                    {
                        CHECK_ERROR_RET(adapter, COMSETTER(TraceEnabled)(FALSE), 1);
                    }
                    else
                    {
                        errorArgument("Invalid nictrace%lu argument '%s'", n, Utf8Str(a->argv[2]).raw());
                        rc = E_FAIL;
                        break;
                    }
                }
                else
                {
                    RTPrintf("The NIC %d is currently disabled and thus can't change its tracefile\n", n);
                }
            }
        }
        else if (!strncmp(a->argv[1], "nic", 3))
        {
            /* Get the number of network adapters */
            ULONG NetworkAdapterCount = 0;
            ComPtr <ISystemProperties> info;
            CHECK_ERROR_BREAK(a->virtualBox, COMGETTER(SystemProperties)(info.asOutParam()));
            CHECK_ERROR_BREAK(info, COMGETTER(NetworkAdapterCount)(&NetworkAdapterCount));

            unsigned n = parseNum(&a->argv[1][3], NetworkAdapterCount, "NIC");
            if (!n)
            {
                rc = E_FAIL;
                break;
            }
            if (a->argc <= 2)
            {
                errorArgument("Missing argument to '%s'", a->argv[1]);
                rc = E_FAIL;
                break;
            }

            /* get the corresponding network adapter */
            ComPtr<INetworkAdapter> adapter;
            CHECK_ERROR_BREAK(sessionMachine, GetNetworkAdapter(n - 1, adapter.asOutParam()));
            if (adapter)
            {
                BOOL fEnabled;
                adapter->COMGETTER(Enabled)(&fEnabled);
                if (fEnabled)
                {
                    if (!strcmp(a->argv[2], "null"))
                    {
                        CHECK_ERROR_RET(adapter, COMSETTER(Enabled)(TRUE), 1);
                        CHECK_ERROR_RET(adapter, Detach(), 1);
                    }
                    else if (!strcmp(a->argv[2], "nat"))
                    {
                        CHECK_ERROR_RET(adapter, COMSETTER(Enabled)(TRUE), 1);
                        if (a->argc == 4)
                            CHECK_ERROR_RET(adapter, COMSETTER(NATNetwork)(Bstr(a->argv[3])), 1);
                        CHECK_ERROR_RET(adapter, AttachToNAT(), 1);
                    }
                    else if (  !strcmp(a->argv[2], "bridged")
                            || !strcmp(a->argv[2], "hostif")) /* backward compatibility */
                    {
                        if (a->argc <= 3)
                        {
                            errorArgument("Missing argument to '%s'", a->argv[2]);
                            rc = E_FAIL;
                            break;
                        }
                        CHECK_ERROR_RET(adapter, COMSETTER(Enabled)(TRUE), 1);
                        CHECK_ERROR_RET(adapter, COMSETTER(HostInterface)(Bstr(a->argv[3])), 1);
                        CHECK_ERROR_RET(adapter, AttachToBridgedInterface(), 1);
                    }
                    else if (!strcmp(a->argv[2], "intnet"))
                    {
                        if (a->argc <= 3)
                        {
                            errorArgument("Missing argument to '%s'", a->argv[2]);
                            rc = E_FAIL;
                            break;
                        }
                        CHECK_ERROR_RET(adapter, COMSETTER(Enabled)(TRUE), 1);
                        CHECK_ERROR_RET(adapter, COMSETTER(InternalNetwork)(Bstr(a->argv[3])), 1);
                        CHECK_ERROR_RET(adapter, AttachToInternalNetwork(), 1);
                    }
#if defined(VBOX_WITH_NETFLT)
                    else if (!strcmp(a->argv[2], "hostonly"))
                    {
                        if (a->argc <= 3)
                        {
                            errorArgument("Missing argument to '%s'", a->argv[2]);
                            rc = E_FAIL;
                            break;
                        }
                        CHECK_ERROR_RET(adapter, COMSETTER(Enabled)(TRUE), 1);
                        CHECK_ERROR_RET(adapter, COMSETTER(HostInterface)(Bstr(a->argv[3])), 1);
                        CHECK_ERROR_RET(adapter, AttachToHostOnlyInterface(), 1);
                    }
#endif
                    else
                    {
                        errorArgument("Invalid type '%s' specfied for NIC %lu", Utf8Str(a->argv[2]).raw(), n);
                        rc = E_FAIL;
                        break;
                    }
                }
                else
                {
                    RTPrintf("The NIC %d is currently disabled and thus can't change its attachment type\n", n);
                }
            }
        }
#endif /* VBOX_DYNAMIC_NET_ATTACH */
#ifdef VBOX_WITH_VRDP
        else if (!strcmp(a->argv[1], "vrdp"))
        {
            if (a->argc <= 1 + 1)
            {
                errorArgument("Missing argument to '%s'", a->argv[1]);
                rc = E_FAIL;
                break;
            }
            /* get the corresponding VRDP server */
            ComPtr<IVRDPServer> vrdpServer;
            sessionMachine->COMGETTER(VRDPServer)(vrdpServer.asOutParam());
            ASSERT(vrdpServer);
            if (vrdpServer)
            {
                if (!strcmp(a->argv[2], "on"))
                {
                    CHECK_ERROR_BREAK(vrdpServer, COMSETTER(Enabled)(TRUE));
                }
                else if (!strcmp(a->argv[2], "off"))
                {
                    CHECK_ERROR_BREAK(vrdpServer, COMSETTER(Enabled)(FALSE));
                }
                else
                {
                    errorArgument("Invalid vrdp server state '%s'", Utf8Str(a->argv[2]).raw());
                    rc = E_FAIL;
                    break;
                }
            }
        }
        else if (!strcmp(a->argv[1], "vrdpport"))
        {
            if (a->argc <= 1 + 1)
            {
                errorArgument("Missing argument to '%s'", a->argv[1]);
                rc = E_FAIL;
                break;
            }
            /* get the corresponding VRDP server */
            ComPtr<IVRDPServer> vrdpServer;
            sessionMachine->COMGETTER(VRDPServer)(vrdpServer.asOutParam());
            ASSERT(vrdpServer);
            if (vrdpServer)
            {
                Bstr vrdpports;

                if (!strcmp(a->argv[2], "default"))
                    vrdpports = "0";
                else
                    vrdpports = a->argv [2];

                CHECK_ERROR_BREAK(vrdpServer, COMSETTER(Ports)(vrdpports));
            }
        }
#endif /* VBOX_WITH_VRDP */
        else if (   !strcmp(a->argv[1], "usbattach")
                 || !strcmp(a->argv[1], "usbdetach"))
        {
            if (a->argc < 3)
            {
                errorSyntax(USAGE_CONTROLVM, "Not enough parameters");
                rc = E_FAIL;
                break;
            }

            bool attach = !strcmp(a->argv[1], "usbattach");

            Bstr usbId = a->argv [2];
            if (Guid(usbId).isEmpty())
            {
                // assume address
                if (attach)
                {
                    ComPtr <IHost> host;
                    CHECK_ERROR_BREAK(a->virtualBox, COMGETTER(Host)(host.asOutParam()));
                    SafeIfaceArray <IHostUSBDevice> coll;
                    CHECK_ERROR_BREAK(host, COMGETTER(USBDevices)(ComSafeArrayAsOutParam(coll)));
                    ComPtr <IHostUSBDevice> dev;
                    CHECK_ERROR_BREAK(host, FindUSBDeviceByAddress(Bstr(a->argv [2]), dev.asOutParam()));
                    CHECK_ERROR_BREAK(dev, COMGETTER(Id)(usbId.asOutParam()));
                }
                else
                {
                    SafeIfaceArray <IUSBDevice> coll;
                    CHECK_ERROR_BREAK(console, COMGETTER(USBDevices)(ComSafeArrayAsOutParam(coll)));
                    ComPtr <IUSBDevice> dev;
                    CHECK_ERROR_BREAK(console, FindUSBDeviceByAddress(Bstr(a->argv [2]),
                                                       dev.asOutParam()));
                    CHECK_ERROR_BREAK(dev, COMGETTER(Id)(usbId.asOutParam()));
                }
            }

            if (attach)
                CHECK_ERROR_BREAK(console, AttachUSBDevice(usbId));
            else
            {
                ComPtr <IUSBDevice> dev;
                CHECK_ERROR_BREAK(console, DetachUSBDevice(usbId, dev.asOutParam()));
            }
        }
        else if (!strcmp(a->argv[1], "setvideomodehint"))
        {
            if (a->argc != 5 && a->argc != 6)
            {
                errorSyntax(USAGE_CONTROLVM, "Incorrect number of parameters");
                rc = E_FAIL;
                break;
            }
            uint32_t xres = RTStrToUInt32(a->argv[2]);
            uint32_t yres = RTStrToUInt32(a->argv[3]);
            uint32_t bpp  = RTStrToUInt32(a->argv[4]);
            uint32_t displayIdx = 0;
            if (a->argc == 6)
                displayIdx = RTStrToUInt32(a->argv[5]);

            ComPtr<IDisplay> display;
            CHECK_ERROR_BREAK(console, COMGETTER(Display)(display.asOutParam()));
            CHECK_ERROR_BREAK(display, SetVideoModeHint(xres, yres, bpp, displayIdx));
        }
        else if (!strcmp(a->argv[1], "setcredentials"))
        {
            bool fAllowLocalLogon = true;
            if (a->argc == 7)
            {
                if (   strcmp(a->argv[5], "--allowlocallogon")
                    && strcmp(a->argv[5], "-allowlocallogon"))
                {
                    errorArgument("Invalid parameter '%s'", a->argv[5]);
                    rc = E_FAIL;
                    break;
                }
                if (!strcmp(a->argv[6], "no"))
                    fAllowLocalLogon = false;
            }
            else if (a->argc != 5)
            {
                errorSyntax(USAGE_CONTROLVM, "Incorrect number of parameters");
                rc = E_FAIL;
                break;
            }

            ComPtr<IGuest> guest;
            CHECK_ERROR_BREAK(console, COMGETTER(Guest)(guest.asOutParam()));
            CHECK_ERROR_BREAK(guest, SetCredentials(Bstr(a->argv[2]), Bstr(a->argv[3]), Bstr(a->argv[4]), fAllowLocalLogon));
        }
        else if (!strcmp(a->argv[1], "dvdattach"))
        {
            Bstr uuid;
            if (a->argc != 3)
            {
                errorSyntax(USAGE_CONTROLVM, "Incorrect number of parameters");
                rc = E_FAIL;
                break;
            }

            ComPtr<IMedium> dvdMedium;

            /* unmount? */
            if (!strcmp(a->argv[2], "none"))
            {
                /* nothing to do, NULL object will cause unmount */
            }
            /* host drive? */
            else if (!strncmp(a->argv[2], "host:", 5))
            {
                ComPtr<IHost> host;
                CHECK_ERROR(a->virtualBox, COMGETTER(Host)(host.asOutParam()));

                rc = host->FindHostDVDDrive(Bstr(a->argv[2] + 5), dvdMedium.asOutParam());
                if (!dvdMedium)
                {
                    errorArgument("Invalid host DVD drive name \"%s\"",
                                  a->argv[2] + 5);
                    rc = E_FAIL;
                    break;
                }
            }
            else
            {
                /* first assume it's a UUID */
                uuid = a->argv[2];
                rc = a->virtualBox->GetDVDImage(uuid, dvdMedium.asOutParam());
                if (FAILED(rc) || !dvdMedium)
                {
                    /* must be a filename, check if it's in the collection */
                    rc = a->virtualBox->FindDVDImage(Bstr(a->argv[2]), dvdMedium.asOutParam());
                    /* not registered, do that on the fly */
                    if (!dvdMedium)
                    {
                        Bstr emptyUUID;
                        CHECK_ERROR(a->virtualBox, OpenDVDImage(Bstr(a->argv[2]), emptyUUID, dvdMedium.asOutParam()));
                    }
                }
                if (!dvdMedium)
                {
                    rc = E_FAIL;
                    break;
                }
            }

            /** @todo generalize this, allow arbitrary number of DVD drives
             * and as a consequence multiple attachments and different
             * storage controllers. */
            if (dvdMedium)
                dvdMedium->COMGETTER(Id)(uuid.asOutParam());
            else
                uuid = Guid().toString();
            CHECK_ERROR(machine, MountMedium(Bstr("IDE Controller"), 1, 0, uuid, FALSE /* aForce */));
        }
        else if (!strcmp(a->argv[1], "floppyattach"))
        {
            Bstr uuid;
            if (a->argc != 3)
            {
                errorSyntax(USAGE_CONTROLVM, "Incorrect number of parameters");
                rc = E_FAIL;
                break;
            }

            ComPtr<IMedium> floppyMedium;

            /* unmount? */
            if (!strcmp(a->argv[2], "none"))
            {
                /* nothing to do, NULL object will cause unmount */
            }
            /* host drive? */
            else if (!strncmp(a->argv[2], "host:", 5))
            {
                ComPtr<IHost> host;
                CHECK_ERROR(a->virtualBox, COMGETTER(Host)(host.asOutParam()));
                host->FindHostFloppyDrive(Bstr(a->argv[2] + 5), floppyMedium.asOutParam());
                if (!floppyMedium)
                {
                    errorArgument("Invalid host floppy drive name \"%s\"",
                                  a->argv[2] + 5);
                    rc = E_FAIL;
                    break;
                }
            }
            else
            {
                /* first assume it's a UUID */
                uuid = a->argv[2];
                rc = a->virtualBox->GetFloppyImage(uuid, floppyMedium.asOutParam());
                if (FAILED(rc) || !floppyMedium)
                {
                    /* must be a filename, check if it's in the collection */
                    rc = a->virtualBox->FindFloppyImage(Bstr(a->argv[2]), floppyMedium.asOutParam());
                    /* not registered, do that on the fly */
                    if (!floppyMedium)
                    {
                        Bstr emptyUUID;
                        CHECK_ERROR(a->virtualBox, OpenFloppyImage(Bstr(a->argv[2]), emptyUUID, floppyMedium.asOutParam()));
                    }
                }
                if (!floppyMedium)
                {
                    rc = E_FAIL;
                    break;
                }
            }
            floppyMedium->COMGETTER(Id)(uuid.asOutParam());
            CHECK_ERROR(machine, MountMedium(Bstr("Floppy Controller"), 0, 0, uuid, FALSE /* aForce */));
        }
#ifdef VBOX_WITH_MEM_BALLOONING
        else if (   !strcmp(a->argv[1], "--guestmemoryballoon")
                 || !strcmp(a->argv[1], "-guestmemoryballoon"))
        {
            if (a->argc != 3)
            {
                errorSyntax(USAGE_CONTROLVM, "Incorrect number of parameters");
                rc = E_FAIL;
                break;
            }
            uint32_t uVal;
            int vrc;
            vrc = RTStrToUInt32Ex(a->argv[2], NULL, 0, &uVal);
            if (vrc != VINF_SUCCESS)
            {
                errorArgument("Error parsing guest memory balloon size '%s'", a->argv[2]);
                rc = E_FAIL;
                break;
            }

            /* guest is running; update IGuest */
            ComPtr <IGuest> guest;

            rc = console->COMGETTER(Guest)(guest.asOutParam());
            if (SUCCEEDED(rc))
                CHECK_ERROR(guest, COMSETTER(MemoryBalloonSize)(uVal));
        }
#endif
        else if (   !strcmp(a->argv[1], "--gueststatisticsinterval")
                 || !strcmp(a->argv[1], "-gueststatisticsinterval"))
        {
            if (a->argc != 3)
            {
                errorSyntax(USAGE_CONTROLVM, "Incorrect number of parameters");
                rc = E_FAIL;
                break;
            }
            uint32_t uVal;
            int vrc;
            vrc = RTStrToUInt32Ex(a->argv[2], NULL, 0, &uVal);
            if (vrc != VINF_SUCCESS)
            {
                errorArgument("Error parsing guest statistics interval '%s'", a->argv[2]);
                rc = E_FAIL;
                break;
            }

            /* guest is running; update IGuest */
            ComPtr <IGuest> guest;

            rc = console->COMGETTER(Guest)(guest.asOutParam());
            if (SUCCEEDED(rc))
                CHECK_ERROR(guest, COMSETTER(StatisticsUpdateInterval)(uVal));
        }
        else if (!strcmp(a->argv[1], "teleport"))
        {
            Bstr        bstrHostname;
            uint32_t    uMaxDowntime = 250 /*ms*/;
            uint32_t    uPort        = UINT32_MAX;
            Bstr        bstrPassword("");
            static const RTGETOPTDEF s_aTeleportOptions[] =
            {
                { "--host",        'h', RTGETOPT_REQ_STRING }, /** @todo RTGETOPT_FLAG_MANDATORY */
                { "--hostname",    'h', RTGETOPT_REQ_STRING }, /** @todo remove this */
                { "--maxdowntime", 'd', RTGETOPT_REQ_UINT32 },
                { "--port",        'p', RTGETOPT_REQ_UINT32 }, /** @todo RTGETOPT_FLAG_MANDATORY */
                { "--password",    'P', RTGETOPT_REQ_STRING }
            };
            RTGETOPTSTATE GetOptState;
            RTGetOptInit(&GetOptState, a->argc, a->argv, s_aTeleportOptions, RT_ELEMENTS(s_aTeleportOptions), 2, 0 /*fFlags*/);
            int ch;
            RTGETOPTUNION Value;
            while (   SUCCEEDED(rc)
                   && (ch = RTGetOpt(&GetOptState, &Value)))
            {
                switch (ch)
                {
                    case 'h': bstrHostname  = Value.psz; break;
                    case 'd': uMaxDowntime  = Value.u32; break;
                    case 'p': uPort         = Value.u32; break;
                    case 'P': bstrPassword  = Value.psz; break;
                    default:
                        errorGetOpt(USAGE_CONTROLVM, ch, &Value);
                        rc = E_FAIL;
                        break;
                }
            }
            if (FAILED(rc))
                break;

            ComPtr<IProgress> progress;
            CHECK_ERROR_BREAK(console, Teleport(bstrHostname, uPort, bstrPassword, uMaxDowntime, progress.asOutParam()));
            rc = showProgress(progress);
            if (FAILED(rc))
            {
                com::ProgressErrorInfo info(progress);
                if (info.isBasicAvailable())
                    RTPrintf("Error: teleportation failed. Error message: %lS\n", info.getText().raw());
                else
                    RTPrintf("Error: teleportation failed. No error message available!\n");
            }
        }
        else
        {
            errorSyntax(USAGE_CONTROLVM, "Invalid parameter '%s'", Utf8Str(a->argv[1]).raw());
            rc = E_FAIL;
        }
    } while (0);

    a->session->Close();

    return SUCCEEDED(rc) ? 0 : 1;
}

static int handleDiscardState(HandlerArg *a)
{
    HRESULT rc;

    if (a->argc != 1)
        return errorSyntax(USAGE_DISCARDSTATE, "Incorrect number of parameters");

    ComPtr<IMachine> machine;
    /* assume it's a UUID */
    rc = a->virtualBox->GetMachine(Bstr(a->argv[0]), machine.asOutParam());
    if (FAILED(rc) || !machine)
    {
        /* must be a name */
        CHECK_ERROR(a->virtualBox, FindMachine(Bstr(a->argv[0]), machine.asOutParam()));
    }
    if (machine)
    {
        do
        {
            /* we have to open a session for this task */
            Bstr guid;
            machine->COMGETTER(Id)(guid.asOutParam());
            CHECK_ERROR_BREAK(a->virtualBox, OpenSession(a->session, guid));
            do
            {
                ComPtr<IConsole> console;
                CHECK_ERROR_BREAK(a->session, COMGETTER(Console)(console.asOutParam()));
                CHECK_ERROR_BREAK(console, ForgetSavedState(true));
            } while (0);
            CHECK_ERROR_BREAK(a->session, Close());
        } while (0);
    }

    return SUCCEEDED(rc) ? 0 : 1;
}

static int handleAdoptdState(HandlerArg *a)
{
    HRESULT rc;

    if (a->argc != 2)
        return errorSyntax(USAGE_ADOPTSTATE, "Incorrect number of parameters");

    ComPtr<IMachine> machine;
    /* assume it's a UUID */
    rc = a->virtualBox->GetMachine(Bstr(a->argv[0]), machine.asOutParam());
    if (FAILED(rc) || !machine)
    {
        /* must be a name */
        CHECK_ERROR(a->virtualBox, FindMachine(Bstr(a->argv[0]), machine.asOutParam()));
    }
    if (machine)
    {
        do
        {
            /* we have to open a session for this task */
            Bstr guid;
            machine->COMGETTER(Id)(guid.asOutParam());
            CHECK_ERROR_BREAK(a->virtualBox, OpenSession(a->session, guid));
            do
            {
                ComPtr<IConsole> console;
                CHECK_ERROR_BREAK(a->session, COMGETTER(Console)(console.asOutParam()));
                CHECK_ERROR_BREAK(console, AdoptSavedState(Bstr(a->argv[1])));
            } while (0);
            CHECK_ERROR_BREAK(a->session, Close());
        } while (0);
    }

    return SUCCEEDED(rc) ? 0 : 1;
}

static int handleGetExtraData(HandlerArg *a)
{
    HRESULT rc = S_OK;

    if (a->argc != 2)
        return errorSyntax(USAGE_GETEXTRADATA, "Incorrect number of parameters");

    /* global data? */
    if (!strcmp(a->argv[0], "global"))
    {
        /* enumeration? */
        if (!strcmp(a->argv[1], "enumerate"))
        {
            SafeArray<BSTR> aKeys;
            CHECK_ERROR(a->virtualBox, GetExtraDataKeys(ComSafeArrayAsOutParam(aKeys)));

            for (size_t i = 0;
                 i < aKeys.size();
                 ++i)
            {
                Bstr bstrKey(aKeys[i]);
                Bstr bstrValue;
                CHECK_ERROR(a->virtualBox, GetExtraData(bstrKey, bstrValue.asOutParam()));

                RTPrintf("Key: %lS, Value: %lS\n", bstrKey.raw(), bstrValue.raw());
            }
        }
        else
        {
            Bstr value;
            CHECK_ERROR(a->virtualBox, GetExtraData(Bstr(a->argv[1]), value.asOutParam()));
            if (!value.isEmpty())
                RTPrintf("Value: %lS\n", value.raw());
            else
                RTPrintf("No value set!\n");
        }
    }
    else
    {
        ComPtr<IMachine> machine;
        /* assume it's a UUID */
        rc = a->virtualBox->GetMachine(Bstr(a->argv[0]), machine.asOutParam());
        if (FAILED(rc) || !machine)
        {
            /* must be a name */
            CHECK_ERROR(a->virtualBox, FindMachine(Bstr(a->argv[0]), machine.asOutParam()));
        }
        if (machine)
        {
            /* enumeration? */
            if (!strcmp(a->argv[1], "enumerate"))
            {
                SafeArray<BSTR> aKeys;
                CHECK_ERROR(machine, GetExtraDataKeys(ComSafeArrayAsOutParam(aKeys)));

                for (size_t i = 0;
                    i < aKeys.size();
                    ++i)
                {
                    Bstr bstrKey(aKeys[i]);
                    Bstr bstrValue;
                    CHECK_ERROR(machine, GetExtraData(bstrKey, bstrValue.asOutParam()));

                    RTPrintf("Key: %lS, Value: %lS\n", bstrKey.raw(), bstrValue.raw());
                }
            }
            else
            {
                Bstr value;
                CHECK_ERROR(machine, GetExtraData(Bstr(a->argv[1]), value.asOutParam()));
                if (!value.isEmpty())
                    RTPrintf("Value: %lS\n", value.raw());
                else
                    RTPrintf("No value set!\n");
            }
        }
    }
    return SUCCEEDED(rc) ? 0 : 1;
}

static int handleSetExtraData(HandlerArg *a)
{
    HRESULT rc = S_OK;

    if (a->argc < 2)
        return errorSyntax(USAGE_SETEXTRADATA, "Not enough parameters");

    /* global data? */
    if (!strcmp(a->argv[0], "global"))
    {
        if (a->argc < 3)
            CHECK_ERROR(a->virtualBox, SetExtraData(Bstr(a->argv[1]), NULL));
        else if (a->argc == 3)
            CHECK_ERROR(a->virtualBox, SetExtraData(Bstr(a->argv[1]), Bstr(a->argv[2])));
        else
            return errorSyntax(USAGE_SETEXTRADATA, "Too many parameters");
    }
    else
    {
        ComPtr<IMachine> machine;
        /* assume it's a UUID */
        rc = a->virtualBox->GetMachine(Bstr(a->argv[0]), machine.asOutParam());
        if (FAILED(rc) || !machine)
        {
            /* must be a name */
            CHECK_ERROR(a->virtualBox, FindMachine(Bstr(a->argv[0]), machine.asOutParam()));
        }
        if (machine)
        {
            if (a->argc < 3)
                CHECK_ERROR(machine, SetExtraData(Bstr(a->argv[1]), NULL));
            else if (a->argc == 3)
                CHECK_ERROR(machine, SetExtraData(Bstr(a->argv[1]), Bstr(a->argv[2])));
            else
                return errorSyntax(USAGE_SETEXTRADATA, "Too many parameters");
        }
    }
    return SUCCEEDED(rc) ? 0 : 1;
}

static int handleSetProperty(HandlerArg *a)
{
    HRESULT rc;

    /* there must be two arguments: property name and value */
    if (a->argc != 2)
        return errorSyntax(USAGE_SETPROPERTY, "Incorrect number of parameters");

    ComPtr<ISystemProperties> systemProperties;
    a->virtualBox->COMGETTER(SystemProperties)(systemProperties.asOutParam());

    if (!strcmp(a->argv[0], "hdfolder"))
    {
        /* reset to default? */
        if (!strcmp(a->argv[1], "default"))
            CHECK_ERROR(systemProperties, COMSETTER(DefaultHardDiskFolder)(NULL));
        else
            CHECK_ERROR(systemProperties, COMSETTER(DefaultHardDiskFolder)(Bstr(a->argv[1])));
    }
    else if (!strcmp(a->argv[0], "machinefolder"))
    {
        /* reset to default? */
        if (!strcmp(a->argv[1], "default"))
            CHECK_ERROR(systemProperties, COMSETTER(DefaultMachineFolder)(NULL));
        else
            CHECK_ERROR(systemProperties, COMSETTER(DefaultMachineFolder)(Bstr(a->argv[1])));
    }
    else if (!strcmp(a->argv[0], "vrdpauthlibrary"))
    {
        /* reset to default? */
        if (!strcmp(a->argv[1], "default"))
            CHECK_ERROR(systemProperties, COMSETTER(RemoteDisplayAuthLibrary)(NULL));
        else
            CHECK_ERROR(systemProperties, COMSETTER(RemoteDisplayAuthLibrary)(Bstr(a->argv[1])));
    }
    else if (!strcmp(a->argv[0], "websrvauthlibrary"))
    {
        /* reset to default? */
        if (!strcmp(a->argv[1], "default"))
            CHECK_ERROR(systemProperties, COMSETTER(WebServiceAuthLibrary)(NULL));
        else
            CHECK_ERROR(systemProperties, COMSETTER(WebServiceAuthLibrary)(Bstr(a->argv[1])));
    }
    else if (!strcmp(a->argv[0], "loghistorycount"))
    {
        uint32_t uVal;
        int vrc;
        vrc = RTStrToUInt32Ex(a->argv[1], NULL, 0, &uVal);
        if (vrc != VINF_SUCCESS)
            return errorArgument("Error parsing Log history count '%s'", a->argv[1]);
        CHECK_ERROR(systemProperties, COMSETTER(LogHistoryCount)(uVal));
    }
    else
        return errorSyntax(USAGE_SETPROPERTY, "Invalid parameter '%s'", a->argv[0]);

    return SUCCEEDED(rc) ? 0 : 1;
}

static int handleSharedFolder(HandlerArg *a)
{
    HRESULT rc;

    /* we need at least a command and target */
    if (a->argc < 2)
        return errorSyntax(USAGE_SHAREDFOLDER, "Not enough parameters");

    ComPtr<IMachine> machine;
    /* assume it's a UUID */
    rc = a->virtualBox->GetMachine(Bstr(a->argv[1]), machine.asOutParam());
    if (FAILED(rc) || !machine)
    {
        /* must be a name */
        CHECK_ERROR(a->virtualBox, FindMachine(Bstr(a->argv[1]), machine.asOutParam()));
    }
    if (!machine)
        return 1;
    Bstr uuid;
    machine->COMGETTER(Id)(uuid.asOutParam());

    if (!strcmp(a->argv[0], "add"))
    {
        /* we need at least four more parameters */
        if (a->argc < 5)
            return errorSyntax(USAGE_SHAREDFOLDER_ADD, "Not enough parameters");

        char *name = NULL;
        char *hostpath = NULL;
        bool fTransient = false;
        bool fWritable = true;

        for (int i = 2; i < a->argc; i++)
        {
            if (   !strcmp(a->argv[i], "--name")
                || !strcmp(a->argv[i], "-name"))
            {
                if (a->argc <= i + 1 || !*a->argv[i+1])
                    return errorArgument("Missing argument to '%s'", a->argv[i]);
                i++;
                name = a->argv[i];
            }
            else if (   !strcmp(a->argv[i], "--hostpath")
                     || !strcmp(a->argv[i], "-hostpath"))
            {
                if (a->argc <= i + 1 || !*a->argv[i+1])
                    return errorArgument("Missing argument to '%s'", a->argv[i]);
                i++;
                hostpath = a->argv[i];
            }
            else if (   !strcmp(a->argv[i], "--readonly")
                     || !strcmp(a->argv[i], "-readonly"))
            {
                fWritable = false;
            }
            else if (   !strcmp(a->argv[i], "--transient")
                     || !strcmp(a->argv[i], "-transient"))
            {
                fTransient = true;
            }
            else
                return errorSyntax(USAGE_SHAREDFOLDER_ADD, "Invalid parameter '%s'", Utf8Str(a->argv[i]).raw());
        }

        if (NULL != strstr(name, " "))
            return errorSyntax(USAGE_SHAREDFOLDER_ADD, "No spaces allowed in parameter '-name'!");

        /* required arguments */
        if (!name || !hostpath)
        {
            return errorSyntax(USAGE_SHAREDFOLDER_ADD, "Parameters --name and --hostpath are required");
        }

        if (fTransient)
        {
            ComPtr <IConsole> console;

            /* open an existing session for the VM */
            CHECK_ERROR_RET(a->virtualBox, OpenExistingSession(a->session, uuid), 1);
            /* get the session machine */
            CHECK_ERROR_RET(a->session, COMGETTER(Machine)(machine.asOutParam()), 1);
            /* get the session console */
            CHECK_ERROR_RET(a->session, COMGETTER(Console)(console.asOutParam()), 1);

            CHECK_ERROR(console, CreateSharedFolder(Bstr(name), Bstr(hostpath), fWritable));

            if (console)
                a->session->Close();
        }
        else
        {
            /* open a session for the VM */
            CHECK_ERROR_RET(a->virtualBox, OpenSession(a->session, uuid), 1);

            /* get the mutable session machine */
            a->session->COMGETTER(Machine)(machine.asOutParam());

            CHECK_ERROR(machine, CreateSharedFolder(Bstr(name), Bstr(hostpath), fWritable));

            if (SUCCEEDED(rc))
                CHECK_ERROR(machine, SaveSettings());

            a->session->Close();
        }
    }
    else if (!strcmp(a->argv[0], "remove"))
    {
        /* we need at least two more parameters */
        if (a->argc < 3)
            return errorSyntax(USAGE_SHAREDFOLDER_REMOVE, "Not enough parameters");

        char *name = NULL;
        bool fTransient = false;

        for (int i = 2; i < a->argc; i++)
        {
            if (   !strcmp(a->argv[i], "--name")
                || !strcmp(a->argv[i], "-name"))
            {
                if (a->argc <= i + 1 || !*a->argv[i+1])
                    return errorArgument("Missing argument to '%s'", a->argv[i]);
                i++;
                name = a->argv[i];
            }
            else if (   !strcmp(a->argv[i], "--transient")
                     || !strcmp(a->argv[i], "-transient"))
            {
                fTransient = true;
            }
            else
                return errorSyntax(USAGE_SHAREDFOLDER_REMOVE, "Invalid parameter '%s'", Utf8Str(a->argv[i]).raw());
        }

        /* required arguments */
        if (!name)
            return errorSyntax(USAGE_SHAREDFOLDER_REMOVE, "Parameter --name is required");

        if (fTransient)
        {
            ComPtr <IConsole> console;

            /* open an existing session for the VM */
            CHECK_ERROR_RET(a->virtualBox, OpenExistingSession(a->session, uuid), 1);
            /* get the session machine */
            CHECK_ERROR_RET(a->session, COMGETTER(Machine)(machine.asOutParam()), 1);
            /* get the session console */
            CHECK_ERROR_RET(a->session, COMGETTER(Console)(console.asOutParam()), 1);

            CHECK_ERROR(console, RemoveSharedFolder(Bstr(name)));

            if (console)
                a->session->Close();
        }
        else
        {
            /* open a session for the VM */
            CHECK_ERROR_RET(a->virtualBox, OpenSession(a->session, uuid), 1);

            /* get the mutable session machine */
            a->session->COMGETTER(Machine)(machine.asOutParam());

            CHECK_ERROR(machine, RemoveSharedFolder(Bstr(name)));

            /* commit and close the session */
            CHECK_ERROR(machine, SaveSettings());
            a->session->Close();
        }
    }
    else
        return errorSyntax(USAGE_SETPROPERTY, "Invalid parameter '%s'", Utf8Str(a->argv[0]).raw());

    return 0;
}

static int handleVMStatistics(HandlerArg *a)
{
    HRESULT rc;

    /* at least one option: the UUID or name of the VM */
    if (a->argc < 1)
        return errorSyntax(USAGE_VM_STATISTICS, "Incorrect number of parameters");

    /* try to find the given machine */
    ComPtr <IMachine> machine;
    Bstr uuid (a->argv[0]);
    if (!Guid (a->argv[0]).isEmpty())
        CHECK_ERROR(a->virtualBox, GetMachine(uuid, machine.asOutParam()));
    else
    {
        CHECK_ERROR(a->virtualBox, FindMachine(Bstr(a->argv[0]), machine.asOutParam()));
        if (SUCCEEDED (rc))
            machine->COMGETTER(Id)(uuid.asOutParam());
    }
    if (FAILED(rc))
        return 1;

    /* parse arguments. */
    bool fReset = false;
    bool fWithDescriptions = false;
    const char *pszPattern = NULL; /* all */
    for (int i = 1; i < a->argc; i++)
    {
        if (   !strcmp(a->argv[i], "--pattern")
            || !strcmp(a->argv[i], "-pattern"))
        {
            if (pszPattern)
                return errorSyntax(USAGE_VM_STATISTICS, "Multiple --patterns options is not permitted");
            if (i + 1 >= a->argc)
                return errorArgument("Missing argument to '%s'", a->argv[i]);
            pszPattern = a->argv[++i];
        }
        else if (   !strcmp(a->argv[i], "--descriptions")
                 || !strcmp(a->argv[i], "-descriptions"))
            fWithDescriptions = true;
        /* add: --file <filename> and --formatted */
        else if (   !strcmp(a->argv[i], "--reset")
                 || !strcmp(a->argv[i], "-reset"))
            fReset = true;
        else
            return errorSyntax(USAGE_VM_STATISTICS, "Unknown option '%s'", a->argv[i]);
    }
    if (fReset && fWithDescriptions)
        return errorSyntax(USAGE_VM_STATISTICS, "The --reset and --descriptions options does not mix");


    /* open an existing session for the VM. */
    CHECK_ERROR(a->virtualBox, OpenExistingSession(a->session, uuid));
    if (SUCCEEDED(rc))
    {
        /* get the session console. */
        ComPtr <IConsole> console;
        CHECK_ERROR(a->session, COMGETTER(Console)(console.asOutParam()));
        if (SUCCEEDED(rc))
        {
            /* get the machine debugger. */
            ComPtr <IMachineDebugger> debugger;
            CHECK_ERROR(console, COMGETTER(Debugger)(debugger.asOutParam()));
            if (SUCCEEDED(rc))
            {
                if (fReset)
                    CHECK_ERROR(debugger, ResetStats(Bstr(pszPattern)));
                else
                {
                    Bstr stats;
                    CHECK_ERROR(debugger, GetStats(Bstr(pszPattern), fWithDescriptions, stats.asOutParam()));
                    if (SUCCEEDED(rc))
                    {
                        /* if (fFormatted)
                         { big mess }
                         else
                         */
                        RTPrintf("%ls\n", stats.raw());
                    }
                }
            }
            a->session->Close();
        }
    }

    return SUCCEEDED(rc) ? 0 : 1;
}
#endif /* !VBOX_ONLY_DOCS */

// main
///////////////////////////////////////////////////////////////////////////////

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
        else if (   !strcmp(argv[i], "-v")
                 || !strcmp(argv[i], "-version")
                 || !strcmp(argv[i], "-Version")
                 || !strcmp(argv[i], "--version"))
        {
            /* Print version number, and do nothing else. */
            RTPrintf("%sr%d\n", VBOX_VERSION_STRING, RTBldCfgRevision());
            return 0;
        }
        else if (   !strcmp(argv[i], "--dumpopts")
                 || !strcmp(argv[i], "-dumpopts"))
        {
            /* Special option to dump really all commands,
             * even the ones not understood on this platform. */
            printUsage(USAGE_DUMPOPTS);
            return 0;
        }
        else if (   !strcmp(argv[i], "--nologo")
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
    struct
    {
        const char *command;
        PFNHANDLER handler;
    } commandHandlers[] =
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
#endif /* VBOX_WITH_GUEST_PROPS defined */
        { "metrics",          handleMetrics },
        { "import",           handleImportAppliance },
        { "export",           handleExportAppliance },
#if defined(VBOX_WITH_NETFLT)
        { "hostonlyif",       handleHostonlyIf },
#endif
        { "dhcpserver",       handleDHCPServer},
        { NULL,               NULL }
    };

    int commandIndex;
    for (commandIndex = 0; commandHandlers[commandIndex].command != NULL; commandIndex++)
    {
        if (!strcmp(commandHandlers[commandIndex].command, argv[iCmd]))
        {
            handlerArg.argc = argc - iCmdArg;
            handlerArg.argv = &argv[iCmdArg];

            rc = commandHandlers[commandIndex].handler(&handlerArg);
            break;
        }
    }
    if (!commandHandlers[commandIndex].command)
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
