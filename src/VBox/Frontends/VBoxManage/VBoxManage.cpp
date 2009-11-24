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
#endif /* !VBOX_ONLY_DOCS */

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
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

static int handleStartVM(HandlerArg *a)
{
    HRESULT rc;
    const char *VMName = NULL;
    Bstr sessionType = "gui";

    static const RTGETOPTDEF s_aStartVMOptions[] =
    {
        { "--type",         't', RTGETOPT_REQ_STRING },
        { "-type",          't', RTGETOPT_REQ_STRING },     // deprecated
    };
    int c;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    // start at 0 because main() has hacked both the argc and argv given to us
    RTGetOptInit(&GetState, a->argc, a->argv, s_aStartVMOptions, RT_ELEMENTS(s_aStartVMOptions), 0, 0 /* fFlags */);
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
