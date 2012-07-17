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

#include <list>

using namespace com;



int handleRegisterVM(HandlerArg *a)
{
    HRESULT rc;

    if (a->argc != 1)
        return errorSyntax(USAGE_REGISTERVM, "Incorrect number of parameters");

    ComPtr<IMachine> machine;
    /** @todo Ugly hack to get both the API interpretation of relative paths
     * and the client's interpretation of relative paths. Remove after the API
     * has been redesigned. */
    rc = a->virtualBox->OpenMachine(Bstr(a->argv[0]).raw(),
                                    machine.asOutParam());
    if (rc == VBOX_E_FILE_ERROR)
    {
        char szVMFileAbs[RTPATH_MAX] = "";
        int vrc = RTPathAbs(a->argv[0], szVMFileAbs, sizeof(szVMFileAbs));
        if (RT_FAILURE(vrc))
        {
            RTMsgError("Cannot convert filename \"%s\" to absolute path", a->argv[0]);
            return 1;
        }
        CHECK_ERROR(a->virtualBox, OpenMachine(Bstr(szVMFileAbs).raw(),
                                               machine.asOutParam()));
    }
    else if (FAILED(rc))
        CHECK_ERROR(a->virtualBox, OpenMachine(Bstr(a->argv[0]).raw(),
                                               machine.asOutParam()));
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

int handleUnregisterVM(HandlerArg *a)
{
    HRESULT rc;
    const char *VMName = NULL;
    bool fDelete = false;

    int c;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    // start at 0 because main() has hacked both the argc and argv given to us
    RTGetOptInit(&GetState, a->argc, a->argv, g_aUnregisterVMOptions, RT_ELEMENTS(g_aUnregisterVMOptions),
                 0, RTGETOPTINIT_FLAGS_NO_STD_OPTS);
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
    CHECK_ERROR_RET(a->virtualBox, FindMachine(Bstr(VMName).raw(),
                                               machine.asOutParam()),
                    RTEXITCODE_FAILURE);
    SafeIfaceArray<IMedium> aMedia;
    CHECK_ERROR_RET(machine, Unregister(fDelete ? (CleanupMode_T)CleanupMode_DetachAllReturnHardDisksOnly : (CleanupMode_T)CleanupMode_DetachAllReturnNone,
                                        ComSafeArrayAsOutParam(aMedia)),
                    RTEXITCODE_FAILURE);
    if (fDelete)
    {
        ComPtr<IProgress> pProgress;
        CHECK_ERROR_RET(machine, Delete(ComSafeArrayAsInParam(aMedia), pProgress.asOutParam()),
                        RTEXITCODE_FAILURE);

        rc = showProgress(pProgress);
        CHECK_PROGRESS_ERROR_RET(pProgress, ("Machine delete failed"), RTEXITCODE_FAILURE);
    }
    return RTEXITCODE_SUCCESS;
}

static const RTGETOPTDEF g_aCreateVMOptions[] =
{
    { "--name",           'n', RTGETOPT_REQ_STRING },
    { "-name",            'n', RTGETOPT_REQ_STRING },
    { "--groups",         'g', RTGETOPT_REQ_STRING },
    { "--basefolder",     'p', RTGETOPT_REQ_STRING },
    { "-basefolder",      'p', RTGETOPT_REQ_STRING },
    { "--ostype",         'o', RTGETOPT_REQ_STRING },
    { "-ostype",          'o', RTGETOPT_REQ_STRING },
    { "--uuid",           'u', RTGETOPT_REQ_UUID },
    { "-uuid",            'u', RTGETOPT_REQ_UUID },
    { "--register",       'r', RTGETOPT_REQ_NOTHING },
    { "-register",        'r', RTGETOPT_REQ_NOTHING },
};

int handleCreateVM(HandlerArg *a)
{
    HRESULT rc;
    Bstr bstrBaseFolder;
    Bstr bstrName;
    Bstr bstrOsTypeId;
    Bstr bstrUuid;
    bool fRegister = false;
    com::SafeArray<BSTR> groups;

    int c;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    // start at 0 because main() has hacked both the argc and argv given to us
    RTGetOptInit(&GetState, a->argc, a->argv, g_aCreateVMOptions, RT_ELEMENTS(g_aCreateVMOptions),
                 0, RTGETOPTINIT_FLAGS_NO_STD_OPTS);
    while ((c = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (c)
        {
            case 'n':   // --name
                bstrName = ValueUnion.psz;
                break;

            case 'g':   // --groups
                parseGroups(ValueUnion.psz, &groups);
                break;

            case 'p':   // --basefolder
                bstrBaseFolder = ValueUnion.psz;
                break;

            case 'o':   // --ostype
                bstrOsTypeId = ValueUnion.psz;
                break;

            case 'u':   // --uuid
                bstrUuid = Guid(ValueUnion.Uuid).toUtf16().raw();
                break;

            case 'r':   // --register
                fRegister = true;
                break;

            default:
                return errorGetOpt(USAGE_CREATEVM, c, &ValueUnion);
        }
    }

    /* check for required options */
    if (bstrName.isEmpty())
        return errorSyntax(USAGE_CREATEVM, "Parameter --name is required");

    do
    {
        Bstr bstrPrimaryGroup;
        if (groups.size())
            bstrPrimaryGroup = groups[0];
        Bstr bstrSettingsFile;
        CHECK_ERROR_BREAK(a->virtualBox,
                          ComposeMachineFilename(bstrName.raw(),
                                                 bstrPrimaryGroup.raw(),
                                                 bstrBaseFolder.raw(),
                                                 bstrSettingsFile.asOutParam()));
        ComPtr<IMachine> machine;
        CHECK_ERROR_BREAK(a->virtualBox,
                          CreateMachine(bstrSettingsFile.raw(),
                                        bstrName.raw(),
                                        ComSafeArrayAsInParam(groups),
                                        bstrOsTypeId.raw(),
                                        bstrUuid.raw(),
                                        FALSE /* forceOverwrite */,
                                        machine.asOutParam()));

        CHECK_ERROR_BREAK(machine, SaveSettings());
        if (fRegister)
        {
            CHECK_ERROR_BREAK(a->virtualBox, RegisterMachine(machine));
        }
        Bstr uuid;
        CHECK_ERROR_BREAK(machine, COMGETTER(Id)(uuid.asOutParam()));
        Bstr settingsFile;
        CHECK_ERROR_BREAK(machine, COMGETTER(SettingsFilePath)(settingsFile.asOutParam()));
        RTPrintf("Virtual machine '%ls' is created%s.\n"
                 "UUID: %s\n"
                 "Settings file: '%ls'\n",
                 bstrName.raw(), fRegister ? " and registered" : "",
                 Utf8Str(uuid).c_str(), settingsFile.raw());
    }
    while (0);

    return SUCCEEDED(rc) ? 0 : 1;
}

static const RTGETOPTDEF g_aCloneVMOptions[] =
{
    { "--snapshot",       's', RTGETOPT_REQ_STRING },
    { "--name",           'n', RTGETOPT_REQ_STRING },
    { "--groups",         'g', RTGETOPT_REQ_STRING },
    { "--mode",           'm', RTGETOPT_REQ_STRING },
    { "--options",        'o', RTGETOPT_REQ_STRING },
    { "--register",       'r', RTGETOPT_REQ_NOTHING },
    { "--basefolder",     'p', RTGETOPT_REQ_STRING },
    { "--uuid",           'u', RTGETOPT_REQ_UUID },
};

static int parseCloneMode(const char *psz, CloneMode_T *pMode)
{
    if (!RTStrICmp(psz, "machine"))
        *pMode = CloneMode_MachineState;
    else if (!RTStrICmp(psz, "machineandchildren"))
        *pMode = CloneMode_MachineAndChildStates;
    else if (!RTStrICmp(psz, "all"))
        *pMode = CloneMode_AllStates;
    else
        return VERR_PARSE_ERROR;

    return VINF_SUCCESS;
}

static int parseCloneOptions(const char *psz, com::SafeArray<CloneOptions_T> *options)
{
    int rc = VINF_SUCCESS;
    while (psz && *psz && RT_SUCCESS(rc))
    {
        size_t len;
        const char *pszComma = strchr(psz, ',');
        if (pszComma)
            len = pszComma - psz;
        else
            len = strlen(psz);
        if (len > 0)
        {
            if (!RTStrNICmp(psz, "KeepAllMACs", len))
                options->push_back(CloneOptions_KeepAllMACs);
            else if (!RTStrNICmp(psz, "KeepNATMACs", len))
                options->push_back(CloneOptions_KeepNATMACs);
            else if (!RTStrNICmp(psz, "KeepDiskNames", len))
                options->push_back(CloneOptions_KeepDiskNames);
            else if (   !RTStrNICmp(psz, "Link", len)
                     || !RTStrNICmp(psz, "Linked", len))
                options->push_back(CloneOptions_Link);
            else
                rc = VERR_PARSE_ERROR;
        }
        if (pszComma)
            psz += len + 1;
        else
            psz += len;
    }

    return rc;
}

int handleCloneVM(HandlerArg *a)
{
    HRESULT                        rc;
    const char                    *pszSrcName       = NULL;
    const char                    *pszSnapshotName  = NULL;
    CloneMode_T                    mode             = CloneMode_MachineState;
    com::SafeArray<CloneOptions_T> options;
    const char                    *pszTrgName       = NULL;
    const char                    *pszTrgBaseFolder = NULL;
    bool                           fRegister        = false;
    Bstr                           bstrUuid;
    com::SafeArray<BSTR> groups;

    int c;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    // start at 0 because main() has hacked both the argc and argv given to us
    RTGetOptInit(&GetState, a->argc, a->argv, g_aCloneVMOptions, RT_ELEMENTS(g_aCloneVMOptions),
                 0, RTGETOPTINIT_FLAGS_NO_STD_OPTS);
    while ((c = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (c)
        {
            case 's':   // --snapshot
                pszSnapshotName = ValueUnion.psz;
                break;

            case 'n':   // --name
                pszTrgName = ValueUnion.psz;
                break;

            case 'g':   // --groups
                parseGroups(ValueUnion.psz, &groups);
                break;

            case 'p':   // --basefolder
                pszTrgBaseFolder = ValueUnion.psz;
                break;

            case 'm':   // --mode
                if (RT_FAILURE(parseCloneMode(ValueUnion.psz, &mode)))
                    return errorArgument("Invalid clone mode '%s'\n", ValueUnion.psz);
                break;

            case 'o':   // --options
                if (RT_FAILURE(parseCloneOptions(ValueUnion.psz, &options)))
                    return errorArgument("Invalid clone options '%s'\n", ValueUnion.psz);
                break;

            case 'u':   // --uuid
                bstrUuid = Guid(ValueUnion.Uuid).toUtf16().raw();
                break;

            case 'r':   // --register
                fRegister = true;
                break;

            case VINF_GETOPT_NOT_OPTION:
                if (!pszSrcName)
                    pszSrcName = ValueUnion.psz;
                else
                    return errorSyntax(USAGE_CLONEVM, "Invalid parameter '%s'", ValueUnion.psz);
                break;

            default:
                return errorGetOpt(USAGE_CLONEVM, c, &ValueUnion);
        }
    }

    /* Check for required options */
    if (!pszSrcName)
        return errorSyntax(USAGE_CLONEVM, "VM name required");

    /* Get the machine object */
    ComPtr<IMachine> srcMachine;
    CHECK_ERROR_RET(a->virtualBox, FindMachine(Bstr(pszSrcName).raw(),
                                               srcMachine.asOutParam()),
                    RTEXITCODE_FAILURE);

    /* If a snapshot name/uuid was given, get the particular machine of this
     * snapshot. */
    if (pszSnapshotName)
    {
        ComPtr<ISnapshot> srcSnapshot;
        CHECK_ERROR_RET(srcMachine, FindSnapshot(Bstr(pszSnapshotName).raw(),
                                                 srcSnapshot.asOutParam()),
                        RTEXITCODE_FAILURE);
        CHECK_ERROR_RET(srcSnapshot, COMGETTER(Machine)(srcMachine.asOutParam()),
                        RTEXITCODE_FAILURE);
    }

    /* Default name necessary? */
    if (!pszTrgName)
        pszTrgName = RTStrAPrintf2("%s Clone", pszSrcName);

    Bstr bstrPrimaryGroup;
    if (groups.size())
        bstrPrimaryGroup = groups[0];
    Bstr bstrSettingsFile;
    CHECK_ERROR_RET(a->virtualBox,
                    ComposeMachineFilename(Bstr(pszTrgName).raw(),
                                           bstrPrimaryGroup.raw(),
                                           Bstr(pszTrgBaseFolder).raw(),
                                           bstrSettingsFile.asOutParam()),
                    RTEXITCODE_FAILURE);

    ComPtr<IMachine> trgMachine;
    CHECK_ERROR_RET(a->virtualBox, CreateMachine(bstrSettingsFile.raw(),
                                                 Bstr(pszTrgName).raw(),
                                                 ComSafeArrayAsInParam(groups),
                                                 NULL,
                                                 bstrUuid.raw(),
                                                 FALSE,
                                                 trgMachine.asOutParam()),
                    RTEXITCODE_FAILURE);

    /* Start the cloning */
    ComPtr<IProgress> progress;
    CHECK_ERROR_RET(srcMachine, CloneTo(trgMachine,
                                        mode,
                                        ComSafeArrayAsInParam(options),
                                        progress.asOutParam()),
                    RTEXITCODE_FAILURE);
    rc = showProgress(progress);
    CHECK_PROGRESS_ERROR_RET(progress, ("Clone VM failed"), RTEXITCODE_FAILURE);

    if (fRegister)
        CHECK_ERROR_RET(a->virtualBox, RegisterMachine(trgMachine), RTEXITCODE_FAILURE);

    Bstr bstrNewName;
    CHECK_ERROR_RET(trgMachine, COMGETTER(Name)(bstrNewName.asOutParam()), RTEXITCODE_FAILURE);
    RTPrintf("Machine has been successfully cloned as \"%ls\"\n", bstrNewName.raw());

    return RTEXITCODE_SUCCESS;
}

int handleStartVM(HandlerArg *a)
{
    HRESULT rc = S_OK;
    std::list<const char *> VMs;
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
    RTGetOptInit(&GetState, a->argc, a->argv, s_aStartVMOptions, RT_ELEMENTS(s_aStartVMOptions),
                 0, RTGETOPTINIT_FLAGS_NO_STD_OPTS);
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
                    sessionType = ValueUnion.psz;
                break;

            case VINF_GETOPT_NOT_OPTION:
                VMs.push_back(ValueUnion.psz);
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
    if (VMs.empty())
        return errorSyntax(USAGE_STARTVM, "at least one VM name or uuid required");

    for (std::list<const char *>::const_iterator it = VMs.begin();
         it != VMs.end();
         ++it)
    {
        HRESULT rc2 = rc;
        const char *pszVM = *it;
        ComPtr<IMachine> machine;
        CHECK_ERROR(a->virtualBox, FindMachine(Bstr(pszVM).raw(),
                                               machine.asOutParam()));
        if (machine)
        {
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
            CHECK_ERROR(machine, LaunchVMProcess(a->session, sessionType.raw(),
                                                 env.raw(), progress.asOutParam()));
            if (SUCCEEDED(rc) && !progress.isNull())
            {
                RTPrintf("Waiting for VM \"%s\" to power on...\n", pszVM);
                CHECK_ERROR(progress, WaitForCompletion(-1));
                if (SUCCEEDED(rc))
                {
                    BOOL completed = true;
                    CHECK_ERROR(progress, COMGETTER(Completed)(&completed));
                    if (SUCCEEDED(rc))
                    {
                        ASSERT(completed);

                        LONG iRc;
                        CHECK_ERROR(progress, COMGETTER(ResultCode)(&iRc));
                        if (SUCCEEDED(rc))
                        {
                            if (FAILED(iRc))
                            {
                                ProgressErrorInfo info(progress);
                                com::GluePrintErrorInfo(info);
                            }
                            else
                            {
                                RTPrintf("VM \"%s\" has been successfully started.\n", pszVM);
                            }
                        }
                    }
                }
            }
        }

        /* it's important to always close sessions */
        a->session->UnlockMachine();

        /* make sure that we remember the failed state */
        if (FAILED(rc2))
            rc = rc2;
    }

    return SUCCEEDED(rc) ? 0 : 1;
}

int handleDiscardState(HandlerArg *a)
{
    HRESULT rc;

    if (a->argc != 1)
        return errorSyntax(USAGE_DISCARDSTATE, "Incorrect number of parameters");

    ComPtr<IMachine> machine;
    CHECK_ERROR(a->virtualBox, FindMachine(Bstr(a->argv[0]).raw(),
                                           machine.asOutParam()));
    if (machine)
    {
        do
        {
            /* we have to open a session for this task */
            CHECK_ERROR_BREAK(machine, LockMachine(a->session, LockType_Write));
            do
            {
                ComPtr<IConsole> console;
                CHECK_ERROR_BREAK(a->session, COMGETTER(Console)(console.asOutParam()));
                CHECK_ERROR_BREAK(console, DiscardSavedState(true /* fDeleteFile */));
            } while (0);
            CHECK_ERROR_BREAK(a->session, UnlockMachine());
        } while (0);
    }

    return SUCCEEDED(rc) ? 0 : 1;
}

int handleAdoptState(HandlerArg *a)
{
    HRESULT rc;

    if (a->argc != 2)
        return errorSyntax(USAGE_ADOPTSTATE, "Incorrect number of parameters");

    ComPtr<IMachine> machine;
    CHECK_ERROR(a->virtualBox, FindMachine(Bstr(a->argv[0]).raw(),
                                           machine.asOutParam()));
    if (machine)
    {
        char szStateFileAbs[RTPATH_MAX] = "";
        int vrc = RTPathAbs(a->argv[1], szStateFileAbs, sizeof(szStateFileAbs));
        if (RT_FAILURE(vrc))
        {
            RTMsgError("Cannot convert filename \"%s\" to absolute path", a->argv[0]);
            return 1;
        }

        do
        {
            /* we have to open a session for this task */
            CHECK_ERROR_BREAK(machine, LockMachine(a->session, LockType_Write));
            do
            {
                ComPtr<IConsole> console;
                CHECK_ERROR_BREAK(a->session, COMGETTER(Console)(console.asOutParam()));
                CHECK_ERROR_BREAK(console, AdoptSavedState(Bstr(szStateFileAbs).raw()));
            } while (0);
            CHECK_ERROR_BREAK(a->session, UnlockMachine());
        } while (0);
    }

    return SUCCEEDED(rc) ? 0 : 1;
}

int handleGetExtraData(HandlerArg *a)
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
                CHECK_ERROR(a->virtualBox, GetExtraData(bstrKey.raw(),
                                                        bstrValue.asOutParam()));

                RTPrintf("Key: %ls, Value: %ls\n", bstrKey.raw(), bstrValue.raw());
            }
        }
        else
        {
            Bstr value;
            CHECK_ERROR(a->virtualBox, GetExtraData(Bstr(a->argv[1]).raw(),
                                                    value.asOutParam()));
            if (!value.isEmpty())
                RTPrintf("Value: %ls\n", value.raw());
            else
                RTPrintf("No value set!\n");
        }
    }
    else
    {
        ComPtr<IMachine> machine;
        CHECK_ERROR(a->virtualBox, FindMachine(Bstr(a->argv[0]).raw(),
                                               machine.asOutParam()));
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
                    CHECK_ERROR(machine, GetExtraData(bstrKey.raw(),
                                                      bstrValue.asOutParam()));

                    RTPrintf("Key: %ls, Value: %ls\n", bstrKey.raw(), bstrValue.raw());
                }
            }
            else
            {
                Bstr value;
                CHECK_ERROR(machine, GetExtraData(Bstr(a->argv[1]).raw(),
                                                  value.asOutParam()));
                if (!value.isEmpty())
                    RTPrintf("Value: %ls\n", value.raw());
                else
                    RTPrintf("No value set!\n");
            }
        }
    }
    return SUCCEEDED(rc) ? 0 : 1;
}

int handleSetExtraData(HandlerArg *a)
{
    HRESULT rc = S_OK;

    if (a->argc < 2)
        return errorSyntax(USAGE_SETEXTRADATA, "Not enough parameters");

    /* global data? */
    if (!strcmp(a->argv[0], "global"))
    {
        /** @todo passing NULL is deprecated */
        if (a->argc < 3)
            CHECK_ERROR(a->virtualBox, SetExtraData(Bstr(a->argv[1]).raw(),
                                                    NULL));
        else if (a->argc == 3)
            CHECK_ERROR(a->virtualBox, SetExtraData(Bstr(a->argv[1]).raw(),
                                                    Bstr(a->argv[2]).raw()));
        else
            return errorSyntax(USAGE_SETEXTRADATA, "Too many parameters");
    }
    else
    {
        ComPtr<IMachine> machine;
        CHECK_ERROR(a->virtualBox, FindMachine(Bstr(a->argv[0]).raw(),
                                               machine.asOutParam()));
        if (machine)
        {
            /** @todo passing NULL is deprecated */
            if (a->argc < 3)
                CHECK_ERROR(machine, SetExtraData(Bstr(a->argv[1]).raw(),
                                                  NULL));
            else if (a->argc == 3)
                CHECK_ERROR(machine, SetExtraData(Bstr(a->argv[1]).raw(),
                                                  Bstr(a->argv[2]).raw()));
            else
                return errorSyntax(USAGE_SETEXTRADATA, "Too many parameters");
        }
    }
    return SUCCEEDED(rc) ? 0 : 1;
}

int handleSetProperty(HandlerArg *a)
{
    HRESULT rc;

    /* there must be two arguments: property name and value */
    if (a->argc != 2)
        return errorSyntax(USAGE_SETPROPERTY, "Incorrect number of parameters");

    ComPtr<ISystemProperties> systemProperties;
    a->virtualBox->COMGETTER(SystemProperties)(systemProperties.asOutParam());

    if (!strcmp(a->argv[0], "machinefolder"))
    {
        /* reset to default? */
        if (!strcmp(a->argv[1], "default"))
            CHECK_ERROR(systemProperties, COMSETTER(DefaultMachineFolder)(NULL));
        else
            CHECK_ERROR(systemProperties, COMSETTER(DefaultMachineFolder)(Bstr(a->argv[1]).raw()));
    }
    else if (   !strcmp(a->argv[0], "vrdeauthlibrary")
             || !strcmp(a->argv[0], "vrdpauthlibrary"))
    {
        if (!strcmp(a->argv[0], "vrdpauthlibrary"))
            RTStrmPrintf(g_pStdErr, "Warning: 'vrdpauthlibrary' is deprecated. Use 'vrdeauthlibrary'.\n");

        /* reset to default? */
        if (!strcmp(a->argv[1], "default"))
            CHECK_ERROR(systemProperties, COMSETTER(VRDEAuthLibrary)(NULL));
        else
            CHECK_ERROR(systemProperties, COMSETTER(VRDEAuthLibrary)(Bstr(a->argv[1]).raw()));
    }
    else if (!strcmp(a->argv[0], "websrvauthlibrary"))
    {
        /* reset to default? */
        if (!strcmp(a->argv[1], "default"))
            CHECK_ERROR(systemProperties, COMSETTER(WebServiceAuthLibrary)(NULL));
        else
            CHECK_ERROR(systemProperties, COMSETTER(WebServiceAuthLibrary)(Bstr(a->argv[1]).raw()));
    }
    else if (!strcmp(a->argv[0], "vrdeextpack"))
    {
        /* disable? */
        if (!strcmp(a->argv[1], "null"))
            CHECK_ERROR(systemProperties, COMSETTER(DefaultVRDEExtPack)(NULL));
        else
            CHECK_ERROR(systemProperties, COMSETTER(DefaultVRDEExtPack)(Bstr(a->argv[1]).raw()));
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
    else if (!strcmp(a->argv[0], "autostartdbpath"))
    {
        /* disable? */
        if (!strcmp(a->argv[1], "null"))
            CHECK_ERROR(systemProperties, COMSETTER(AutostartDatabasePath)(NULL));
        else
            CHECK_ERROR(systemProperties, COMSETTER(AutostartDatabasePath)(Bstr(a->argv[1]).raw()));
    }
    else
        return errorSyntax(USAGE_SETPROPERTY, "Invalid parameter '%s'", a->argv[0]);

    return SUCCEEDED(rc) ? 0 : 1;
}

int handleSharedFolder(HandlerArg *a)
{
    HRESULT rc;

    /* we need at least a command and target */
    if (a->argc < 2)
        return errorSyntax(USAGE_SHAREDFOLDER, "Not enough parameters");

    ComPtr<IMachine> machine;
    CHECK_ERROR(a->virtualBox, FindMachine(Bstr(a->argv[1]).raw(),
                                           machine.asOutParam()));
    if (!machine)
        return 1;

    if (!strcmp(a->argv[0], "add"))
    {
        /* we need at least four more parameters */
        if (a->argc < 5)
            return errorSyntax(USAGE_SHAREDFOLDER_ADD, "Not enough parameters");

        char *name = NULL;
        char *hostpath = NULL;
        bool fTransient = false;
        bool fWritable = true;
        bool fAutoMount = false;

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
            else if (   !strcmp(a->argv[i], "--automount")
                     || !strcmp(a->argv[i], "-automount"))
            {
                fAutoMount = true;
            }
            else
                return errorSyntax(USAGE_SHAREDFOLDER_ADD, "Invalid parameter '%s'", Utf8Str(a->argv[i]).c_str());
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
            CHECK_ERROR_RET(machine, LockMachine(a->session, LockType_Shared), 1);
            /* get the session machine */
            CHECK_ERROR_RET(a->session, COMGETTER(Machine)(machine.asOutParam()), 1);
            /* get the session console */
            CHECK_ERROR_RET(a->session, COMGETTER(Console)(console.asOutParam()), 1);

            CHECK_ERROR(console, CreateSharedFolder(Bstr(name).raw(),
                                                    Bstr(hostpath).raw(),
                                                    fWritable, fAutoMount));
            if (console)
                a->session->UnlockMachine();
        }
        else
        {
            /* open a session for the VM */
            CHECK_ERROR_RET(machine, LockMachine(a->session, LockType_Write), 1);

            /* get the mutable session machine */
            a->session->COMGETTER(Machine)(machine.asOutParam());

            CHECK_ERROR(machine, CreateSharedFolder(Bstr(name).raw(),
                                                    Bstr(hostpath).raw(),
                                                    fWritable, fAutoMount));
            if (SUCCEEDED(rc))
                CHECK_ERROR(machine, SaveSettings());

            a->session->UnlockMachine();
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
                return errorSyntax(USAGE_SHAREDFOLDER_REMOVE, "Invalid parameter '%s'", Utf8Str(a->argv[i]).c_str());
        }

        /* required arguments */
        if (!name)
            return errorSyntax(USAGE_SHAREDFOLDER_REMOVE, "Parameter --name is required");

        if (fTransient)
        {
            ComPtr <IConsole> console;

            /* open an existing session for the VM */
            CHECK_ERROR_RET(machine, LockMachine(a->session, LockType_Shared), 1);
            /* get the session machine */
            CHECK_ERROR_RET(a->session, COMGETTER(Machine)(machine.asOutParam()), 1);
            /* get the session console */
            CHECK_ERROR_RET(a->session, COMGETTER(Console)(console.asOutParam()), 1);

            CHECK_ERROR(console, RemoveSharedFolder(Bstr(name).raw()));

            if (console)
                a->session->UnlockMachine();
        }
        else
        {
            /* open a session for the VM */
            CHECK_ERROR_RET(machine, LockMachine(a->session, LockType_Write), 1);

            /* get the mutable session machine */
            a->session->COMGETTER(Machine)(machine.asOutParam());

            CHECK_ERROR(machine, RemoveSharedFolder(Bstr(name).raw()));

            /* commit and close the session */
            CHECK_ERROR(machine, SaveSettings());
            a->session->UnlockMachine();
        }
    }
    else
        return errorSyntax(USAGE_SHAREDFOLDER, "Invalid parameter '%s'", Utf8Str(a->argv[0]).c_str());

    return 0;
}

int handleExtPack(HandlerArg *a)
{
    if (a->argc < 1)
        return errorSyntax(USAGE_EXTPACK, "Incorrect number of parameters");

    ComObjPtr<IExtPackManager> ptrExtPackMgr;
    CHECK_ERROR2_RET(a->virtualBox, COMGETTER(ExtensionPackManager)(ptrExtPackMgr.asOutParam()), RTEXITCODE_FAILURE);

    RTGETOPTSTATE   GetState;
    RTGETOPTUNION   ValueUnion;
    int             ch;
    HRESULT         hrc = S_OK;

    if (!strcmp(a->argv[0], "install"))
    {
        const char *pszName  = NULL;
        bool        fReplace = false;

        static const RTGETOPTDEF s_aInstallOptions[] =
        {
            { "--replace",  'r', RTGETOPT_REQ_NOTHING },
        };

        RTGetOptInit(&GetState, a->argc, a->argv, s_aInstallOptions, RT_ELEMENTS(s_aInstallOptions), 1, 0 /*fFlags*/);
        while ((ch = RTGetOpt(&GetState, &ValueUnion)))
        {
            switch (ch)
            {
                case 'r':
                    fReplace = true;
                    break;

                case VINF_GETOPT_NOT_OPTION:
                    if (pszName)
                        return errorSyntax(USAGE_EXTPACK, "Too many extension pack names given to \"extpack uninstall\"");
                    pszName = ValueUnion.psz;
                    break;

                default:
                    return errorGetOpt(USAGE_EXTPACK, ch, &ValueUnion);
            }
        }
        if (!pszName)
            return errorSyntax(USAGE_EXTPACK, "No extension pack name was given to \"extpack install\"");

        char szPath[RTPATH_MAX];
        int vrc = RTPathAbs(pszName, szPath, sizeof(szPath));
        if (RT_FAILURE(vrc))
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "RTPathAbs(%s,,) failed with rc=%Rrc", pszName, vrc);

        Bstr bstrTarball(szPath);
        Bstr bstrName;
        ComPtr<IExtPackFile> ptrExtPackFile;
        CHECK_ERROR2_RET(ptrExtPackMgr, OpenExtPackFile(bstrTarball.raw(), ptrExtPackFile.asOutParam()), RTEXITCODE_FAILURE);
        CHECK_ERROR2_RET(ptrExtPackFile, COMGETTER(Name)(bstrName.asOutParam()), RTEXITCODE_FAILURE);
        ComPtr<IProgress> ptrProgress;
        CHECK_ERROR2_RET(ptrExtPackFile, Install(fReplace, NULL, ptrProgress.asOutParam()), RTEXITCODE_FAILURE);
        hrc = showProgress(ptrProgress);
        CHECK_PROGRESS_ERROR_RET(ptrProgress, ("Failed to install \"%s\"", szPath), RTEXITCODE_FAILURE);

        RTPrintf("Successfully installed \"%ls\".\n", bstrName.raw());
    }
    else if (!strcmp(a->argv[0], "uninstall"))
    {
        const char *pszName = NULL;
        bool        fForced = false;

        static const RTGETOPTDEF s_aUninstallOptions[] =
        {
            { "--force",  'f', RTGETOPT_REQ_NOTHING },
        };

        RTGetOptInit(&GetState, a->argc, a->argv, s_aUninstallOptions, RT_ELEMENTS(s_aUninstallOptions), 1, 0);
        while ((ch = RTGetOpt(&GetState, &ValueUnion)))
        {
            switch (ch)
            {
                case 'f':
                    fForced = true;
                    break;

                case VINF_GETOPT_NOT_OPTION:
                    if (pszName)
                        return errorSyntax(USAGE_EXTPACK, "Too many extension pack names given to \"extpack uninstall\"");
                    pszName = ValueUnion.psz;
                    break;

                default:
                    return errorGetOpt(USAGE_EXTPACK, ch, &ValueUnion);
            }
        }
        if (!pszName)
            return errorSyntax(USAGE_EXTPACK, "No extension pack name was given to \"extpack uninstall\"");

        Bstr bstrName(pszName);
        ComPtr<IProgress> ptrProgress;
        CHECK_ERROR2_RET(ptrExtPackMgr, Uninstall(bstrName.raw(), fForced, NULL, ptrProgress.asOutParam()), RTEXITCODE_FAILURE);
        hrc = showProgress(ptrProgress);
        CHECK_PROGRESS_ERROR_RET(ptrProgress, ("Failed to uninstall \"%s\"", pszName), RTEXITCODE_FAILURE);

        RTPrintf("Successfully uninstalled \"%s\".\n", pszName);
    }
    else if (!strcmp(a->argv[0], "cleanup"))
    {
        if (a->argc > 1)
            return errorSyntax(USAGE_EXTPACK, "Too many parameters given to \"extpack cleanup\"");

        CHECK_ERROR2_RET(ptrExtPackMgr, Cleanup(), RTEXITCODE_FAILURE);
        RTPrintf("Successfully performed extension pack cleanup\n");
    }
    else
        return errorSyntax(USAGE_EXTPACK, "Unknown command \"%s\"", a->argv[0]);

    return RTEXITCODE_SUCCESS;
}

