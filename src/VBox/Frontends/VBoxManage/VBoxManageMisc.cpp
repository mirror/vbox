/* $Id$ */
/** @file
 * VBoxManage - VirtualBox's command-line interface.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#ifndef VBOX_ONLY_DOCS
# include <VBox/com/com.h>
# include <VBox/com/string.h>
# include <VBox/com/Guid.h>
# include <VBox/com/array.h>
# include <VBox/com/ErrorInfo.h>
# include <VBox/com/errorprint.h>
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
#include <iprt/sha.h>
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



RTEXITCODE handleRegisterVM(HandlerArg *a)
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
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Cannot convert filename \"%s\" to absolute path: %Rrc", a->argv[0], vrc);
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
    return SUCCEEDED(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

static const RTGETOPTDEF g_aUnregisterVMOptions[] =
{
    { "--delete",       'd', RTGETOPT_REQ_NOTHING },
    { "-delete",        'd', RTGETOPT_REQ_NOTHING },    // deprecated
};

RTEXITCODE handleUnregisterVM(HandlerArg *a)
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
    CHECK_ERROR_RET(machine, Unregister(CleanupMode_DetachAllReturnHardDisksOnly,
                                        ComSafeArrayAsOutParam(aMedia)),
                    RTEXITCODE_FAILURE);
    if (fDelete)
    {
        ComPtr<IProgress> pProgress;
        CHECK_ERROR_RET(machine, DeleteConfig(ComSafeArrayAsInParam(aMedia), pProgress.asOutParam()),
                        RTEXITCODE_FAILURE);

        rc = showProgress(pProgress);
        CHECK_PROGRESS_ERROR_RET(pProgress, ("Machine delete failed"), RTEXITCODE_FAILURE);
    }
    else
    {
        /* Note that the IMachine::Unregister method will return the medium
         * reference in a sane order, which means that closing will normally
         * succeed, unless there is still another machine which uses the
         * medium. No harm done if we ignore the error. */
        for (size_t i = 0; i < aMedia.size(); i++)
        {
            IMedium *pMedium = aMedia[i];
            if (pMedium)
                rc = pMedium->Close();
        }
        rc = S_OK;
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

RTEXITCODE handleCreateVM(HandlerArg *a)
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
        Bstr createFlags;
        if (!bstrUuid.isEmpty())
            createFlags = BstrFmt("UUID=%ls", bstrUuid.raw());
        Bstr bstrPrimaryGroup;
        if (groups.size())
            bstrPrimaryGroup = groups[0];
        Bstr bstrSettingsFile;
        CHECK_ERROR_BREAK(a->virtualBox,
                          ComposeMachineFilename(bstrName.raw(),
                                                 bstrPrimaryGroup.raw(),
                                                 createFlags.raw(),
                                                 bstrBaseFolder.raw(),
                                                 bstrSettingsFile.asOutParam()));
        ComPtr<IMachine> machine;
        CHECK_ERROR_BREAK(a->virtualBox,
                          CreateMachine(bstrSettingsFile.raw(),
                                        bstrName.raw(),
                                        ComSafeArrayAsInParam(groups),
                                        bstrOsTypeId.raw(),
                                        createFlags.raw(),
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

    return SUCCEEDED(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
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

RTEXITCODE handleCloneVM(HandlerArg *a)
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

    Bstr createFlags;
    if (!bstrUuid.isEmpty())
        createFlags = BstrFmt("UUID=%ls", bstrUuid.raw());
    Bstr bstrPrimaryGroup;
    if (groups.size())
        bstrPrimaryGroup = groups[0];
    Bstr bstrSettingsFile;
    CHECK_ERROR_RET(a->virtualBox,
                    ComposeMachineFilename(Bstr(pszTrgName).raw(),
                                           bstrPrimaryGroup.raw(),
                                           createFlags.raw(),
                                           Bstr(pszTrgBaseFolder).raw(),
                                           bstrSettingsFile.asOutParam()),
                    RTEXITCODE_FAILURE);

    ComPtr<IMachine> trgMachine;
    CHECK_ERROR_RET(a->virtualBox, CreateMachine(bstrSettingsFile.raw(),
                                                 Bstr(pszTrgName).raw(),
                                                 ComSafeArrayAsInParam(groups),
                                                 NULL,
                                                 createFlags.raw(),
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

RTEXITCODE handleStartVM(HandlerArg *a)
{
    HRESULT rc = S_OK;
    std::list<const char *> VMs;
    Bstr sessionType;
    Utf8Str strEnv;

#if defined(RT_OS_LINUX) || defined(RT_OS_SOLARIS)
    /* make sure the VM process will by default start on the same display as VBoxManage */
    {
        const char *pszDisplay = RTEnvGet("DISPLAY");
        if (pszDisplay)
            strEnv = Utf8StrFmt("DISPLAY=%s\n", pszDisplay);
        const char *pszXAuth = RTEnvGet("XAUTHORITY");
        if (pszXAuth)
            strEnv.append(Utf8StrFmt("XAUTHORITY=%s\n", pszXAuth));
    }
#endif

    static const RTGETOPTDEF s_aStartVMOptions[] =
    {
        { "--type",         't', RTGETOPT_REQ_STRING },
        { "-type",          't', RTGETOPT_REQ_STRING },     // deprecated
        { "--putenv",       'E', RTGETOPT_REQ_STRING },
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

            case 'E':   // --putenv
                if (!RTStrStr(ValueUnion.psz, "\n"))
                    strEnv.append(Utf8StrFmt("%s\n", ValueUnion.psz));
                else
                    return errorSyntax(USAGE_STARTVM, "Parameter to option --putenv must not contain any newline character");
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
            ComPtr<IProgress> progress;
            CHECK_ERROR(machine, LaunchVMProcess(a->session, sessionType.raw(),
                                                 Bstr(strEnv).raw(), progress.asOutParam()));
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
                            if (SUCCEEDED(iRc))
                                RTPrintf("VM \"%s\" has been successfully started.\n", pszVM);
                            else
                            {
                                ProgressErrorInfo info(progress);
                                com::GluePrintErrorInfo(info);
                            }
                            rc = iRc;
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

    return SUCCEEDED(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

RTEXITCODE handleDiscardState(HandlerArg *a)
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
                ComPtr<IMachine> sessionMachine;
                CHECK_ERROR_BREAK(a->session, COMGETTER(Machine)(sessionMachine.asOutParam()));
                CHECK_ERROR_BREAK(sessionMachine, DiscardSavedState(true /* fDeleteFile */));
            } while (0);
            CHECK_ERROR_BREAK(a->session, UnlockMachine());
        } while (0);
    }

    return SUCCEEDED(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

RTEXITCODE handleAdoptState(HandlerArg *a)
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
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "Cannot convert filename \"%s\" to absolute path: %Rrc", a->argv[0], vrc);

        do
        {
            /* we have to open a session for this task */
            CHECK_ERROR_BREAK(machine, LockMachine(a->session, LockType_Write));
            do
            {
                ComPtr<IMachine> sessionMachine;
                CHECK_ERROR_BREAK(a->session, COMGETTER(Machine)(sessionMachine.asOutParam()));
                CHECK_ERROR_BREAK(sessionMachine, AdoptSavedState(Bstr(szStateFileAbs).raw()));
            } while (0);
            CHECK_ERROR_BREAK(a->session, UnlockMachine());
        } while (0);
    }

    return SUCCEEDED(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

RTEXITCODE handleGetExtraData(HandlerArg *a)
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
    return SUCCEEDED(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

RTEXITCODE handleSetExtraData(HandlerArg *a)
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
            /* open an existing session for the VM */
            CHECK_ERROR_RET(machine, LockMachine(a->session, LockType_Shared), RTEXITCODE_FAILURE);
            /* get the session machine */
            ComPtr<IMachine> sessionMachine;
            CHECK_ERROR_RET(a->session, COMGETTER(Machine)(sessionMachine.asOutParam()), RTEXITCODE_FAILURE);
            /** @todo passing NULL is deprecated */
            if (a->argc < 3)
                CHECK_ERROR(sessionMachine, SetExtraData(Bstr(a->argv[1]).raw(),
                                                         NULL));
            else if (a->argc == 3)
                CHECK_ERROR(sessionMachine, SetExtraData(Bstr(a->argv[1]).raw(),
                                                         Bstr(a->argv[2]).raw()));
            else
                return errorSyntax(USAGE_SETEXTRADATA, "Too many parameters");
        }
    }
    return SUCCEEDED(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

RTEXITCODE handleSetProperty(HandlerArg *a)
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
    else if (!strcmp(a->argv[0], "hwvirtexclusive"))
    {
        bool   fHwVirtExclusive;

        if (!strcmp(a->argv[1], "on"))
            fHwVirtExclusive = true;
        else if (!strcmp(a->argv[1], "off"))
            fHwVirtExclusive = false;
        else
            return errorArgument("Invalid hwvirtexclusive argument '%s'", a->argv[1]);
        CHECK_ERROR(systemProperties, COMSETTER(ExclusiveHwVirt)(fHwVirtExclusive));
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
    else if (!strcmp(a->argv[0], "defaultfrontend"))
    {
        Bstr bstrDefaultFrontend(a->argv[1]);
        if (!strcmp(a->argv[1], "default"))
            bstrDefaultFrontend.setNull();
        CHECK_ERROR(systemProperties, COMSETTER(DefaultFrontend)(bstrDefaultFrontend.raw()));
    }
    else if (!strcmp(a->argv[0], "logginglevel"))
    {
        Bstr bstrLoggingLevel(a->argv[1]);
        if (!strcmp(a->argv[1], "default"))
            bstrLoggingLevel.setNull();
        CHECK_ERROR(systemProperties, COMSETTER(LoggingLevel)(bstrLoggingLevel.raw()));
    }
    else
        return errorSyntax(USAGE_SETPROPERTY, "Invalid parameter '%s'", a->argv[0]);

    return SUCCEEDED(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

RTEXITCODE handleSharedFolder(HandlerArg *a)
{
    HRESULT rc;

    /* we need at least a command and target */
    if (a->argc < 2)
        return errorSyntax(USAGE_SHAREDFOLDER, "Not enough parameters");

    const char *pszMachineName = a->argv[1];
    ComPtr<IMachine> machine;
    CHECK_ERROR(a->virtualBox, FindMachine(Bstr(pszMachineName).raw(), machine.asOutParam()));
    if (!machine)
        return RTEXITCODE_FAILURE;

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
            ComPtr<IConsole> console;

            /* open an existing session for the VM */
            CHECK_ERROR_RET(machine, LockMachine(a->session, LockType_Shared), RTEXITCODE_FAILURE);

            /* get the session machine */
            ComPtr<IMachine> sessionMachine;
            CHECK_ERROR_RET(a->session, COMGETTER(Machine)(sessionMachine.asOutParam()), RTEXITCODE_FAILURE);

            /* get the session console */
            CHECK_ERROR_RET(a->session, COMGETTER(Console)(console.asOutParam()), RTEXITCODE_FAILURE);
            if (console.isNull())
                return RTMsgErrorExit(RTEXITCODE_FAILURE,
                                      "Machine '%s' is not currently running.\n", pszMachineName);

            CHECK_ERROR(console, CreateSharedFolder(Bstr(name).raw(),
                                                    Bstr(hostpath).raw(),
                                                    fWritable, fAutoMount));
            a->session->UnlockMachine();
        }
        else
        {
            /* open a session for the VM */
            CHECK_ERROR_RET(machine, LockMachine(a->session, LockType_Write), RTEXITCODE_FAILURE);

            /* get the mutable session machine */
            ComPtr<IMachine> sessionMachine;
            a->session->COMGETTER(Machine)(sessionMachine.asOutParam());

            CHECK_ERROR(sessionMachine, CreateSharedFolder(Bstr(name).raw(),
                                                           Bstr(hostpath).raw(),
                                                           fWritable, fAutoMount));
            if (SUCCEEDED(rc))
                CHECK_ERROR(sessionMachine, SaveSettings());

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
            /* open an existing session for the VM */
            CHECK_ERROR_RET(machine, LockMachine(a->session, LockType_Shared), RTEXITCODE_FAILURE);
            /* get the session machine */
            ComPtr<IMachine> sessionMachine;
            CHECK_ERROR_RET(a->session, COMGETTER(Machine)(sessionMachine.asOutParam()), RTEXITCODE_FAILURE);
            /* get the session console */
            ComPtr<IConsole> console;
            CHECK_ERROR_RET(a->session, COMGETTER(Console)(console.asOutParam()), RTEXITCODE_FAILURE);
            if (console.isNull())
                return RTMsgErrorExit(RTEXITCODE_FAILURE,
                                      "Machine '%s' is not currently running.\n", pszMachineName);

            CHECK_ERROR(console, RemoveSharedFolder(Bstr(name).raw()));

            a->session->UnlockMachine();
        }
        else
        {
            /* open a session for the VM */
            CHECK_ERROR_RET(machine, LockMachine(a->session, LockType_Write), RTEXITCODE_FAILURE);

            /* get the mutable session machine */
            ComPtr<IMachine> sessionMachine;
            a->session->COMGETTER(Machine)(sessionMachine.asOutParam());

            CHECK_ERROR(sessionMachine, RemoveSharedFolder(Bstr(name).raw()));

            /* commit and close the session */
            CHECK_ERROR(sessionMachine, SaveSettings());
            a->session->UnlockMachine();
        }
    }
    else
        return errorSyntax(USAGE_SHAREDFOLDER, "Invalid parameter '%s'", Utf8Str(a->argv[0]).c_str());

    return RTEXITCODE_SUCCESS;
}

RTEXITCODE handleExtPack(HandlerArg *a)
{
    if (a->argc < 1)
        return errorNoSubcommand();

    ComObjPtr<IExtPackManager> ptrExtPackMgr;
    CHECK_ERROR2I_RET(a->virtualBox, COMGETTER(ExtensionPackManager)(ptrExtPackMgr.asOutParam()), RTEXITCODE_FAILURE);

    RTGETOPTSTATE   GetState;
    RTGETOPTUNION   ValueUnion;
    int             ch;
    HRESULT         hrc = S_OK;

    if (!strcmp(a->argv[0], "install"))
    {
        setCurrentSubcommand(HELP_SCOPE_EXTPACK_INSTALL);
        const char *pszName  = NULL;
        bool        fReplace = false;

        static const RTGETOPTDEF s_aInstallOptions[] =
        {
            { "--replace",        'r', RTGETOPT_REQ_NOTHING },
            { "--accept-license", 'a', RTGETOPT_REQ_STRING },
        };

        RTCList<RTCString> lstLicenseHashes;
        RTGetOptInit(&GetState, a->argc, a->argv, s_aInstallOptions, RT_ELEMENTS(s_aInstallOptions), 1, 0 /*fFlags*/);
        while ((ch = RTGetOpt(&GetState, &ValueUnion)))
        {
            switch (ch)
            {
                case 'r':
                    fReplace = true;
                    break;

                case 'a':
                    lstLicenseHashes.append(ValueUnion.psz);
                    lstLicenseHashes[lstLicenseHashes.size() - 1].toLower();
                    break;

                case VINF_GETOPT_NOT_OPTION:
                    if (pszName)
                        return errorSyntax("Too many extension pack names given to \"extpack uninstall\"");
                    pszName = ValueUnion.psz;
                    break;

                default:
                    return errorGetOpt(ch, &ValueUnion);
            }
        }
        if (!pszName)
            return errorSyntax("No extension pack name was given to \"extpack install\"");

        char szPath[RTPATH_MAX];
        int vrc = RTPathAbs(pszName, szPath, sizeof(szPath));
        if (RT_FAILURE(vrc))
            return RTMsgErrorExit(RTEXITCODE_FAILURE, "RTPathAbs(%s,,) failed with rc=%Rrc", pszName, vrc);

        Bstr bstrTarball(szPath);
        Bstr bstrName;
        ComPtr<IExtPackFile> ptrExtPackFile;
        CHECK_ERROR2I_RET(ptrExtPackMgr, OpenExtPackFile(bstrTarball.raw(), ptrExtPackFile.asOutParam()), RTEXITCODE_FAILURE);
        CHECK_ERROR2I_RET(ptrExtPackFile, COMGETTER(Name)(bstrName.asOutParam()), RTEXITCODE_FAILURE);
        BOOL fShowLicense = true;
        CHECK_ERROR2I_RET(ptrExtPackFile, COMGETTER(ShowLicense)(&fShowLicense), RTEXITCODE_FAILURE);
        if (fShowLicense)
        {
            Bstr bstrLicense;
            CHECK_ERROR2I_RET(ptrExtPackFile,
                              QueryLicense(Bstr("").raw() /* PreferredLocale */,
                                           Bstr("").raw() /* PreferredLanguage */,
                                           Bstr("txt").raw() /* Format */,
                                           bstrLicense.asOutParam()), RTEXITCODE_FAILURE);
            Utf8Str strLicense(bstrLicense);
            uint8_t abHash[RTSHA256_HASH_SIZE];
            char    szDigest[RTSHA256_DIGEST_LEN + 1];
            RTSha256(strLicense.c_str(), strLicense.length(), abHash);
            vrc = RTSha256ToString(abHash, szDigest, sizeof(szDigest));
            AssertRCStmt(vrc, szDigest[0] = '\0');
            if (lstLicenseHashes.contains(szDigest))
                RTPrintf("License accepted.\n");
            else
            {
                RTPrintf("%s\n", strLicense.c_str());
                RTPrintf("Do you agree to these license terms and conditions (y/n)? " );
                ch = RTStrmGetCh(g_pStdIn);
                RTPrintf("\n");
                if (ch != 'y' && ch != 'Y')
                {
                    RTPrintf("Installation of \"%ls\" aborted.\n", bstrName.raw());
                    return RTEXITCODE_FAILURE;
                }
                if (szDigest[0])
                    RTPrintf("License accepted. For batch installaltion add\n"
                             "--accept-license=%s\n"
                             "to the VBoxManage command line.\n\n", szDigest);
            }
        }
        ComPtr<IProgress> ptrProgress;
        CHECK_ERROR2I_RET(ptrExtPackFile, Install(fReplace, NULL, ptrProgress.asOutParam()), RTEXITCODE_FAILURE);
        hrc = showProgress(ptrProgress);
        CHECK_PROGRESS_ERROR_RET(ptrProgress, ("Failed to install \"%s\"", szPath), RTEXITCODE_FAILURE);

        RTPrintf("Successfully installed \"%ls\".\n", bstrName.raw());
    }
    else if (!strcmp(a->argv[0], "uninstall"))
    {
        setCurrentSubcommand(HELP_SCOPE_EXTPACK_UNINSTALL);
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
                        return errorSyntax("Too many extension pack names given to \"extpack uninstall\"");
                    pszName = ValueUnion.psz;
                    break;

                default:
                    return errorGetOpt(ch, &ValueUnion);
            }
        }
        if (!pszName)
            return errorSyntax("No extension pack name was given to \"extpack uninstall\"");

        Bstr bstrName(pszName);
        ComPtr<IProgress> ptrProgress;
        CHECK_ERROR2I_RET(ptrExtPackMgr, Uninstall(bstrName.raw(), fForced, NULL, ptrProgress.asOutParam()), RTEXITCODE_FAILURE);
        hrc = showProgress(ptrProgress);
        CHECK_PROGRESS_ERROR_RET(ptrProgress, ("Failed to uninstall \"%s\"", pszName), RTEXITCODE_FAILURE);

        RTPrintf("Successfully uninstalled \"%s\".\n", pszName);
    }
    else if (!strcmp(a->argv[0], "cleanup"))
    {
        setCurrentSubcommand(HELP_SCOPE_EXTPACK_CLEANUP);
        if (a->argc > 1)
            return errorTooManyParameters(&a->argv[1]);
        CHECK_ERROR2I_RET(ptrExtPackMgr, Cleanup(), RTEXITCODE_FAILURE);
        RTPrintf("Successfully performed extension pack cleanup\n");
    }
    else
        return errorUnknownSubcommand(a->argv[0]);

    return RTEXITCODE_SUCCESS;
}

RTEXITCODE handleUnattendedInstall(HandlerArg *a)
{
    /*
     * Options.
     */
    const char *pszIsoPath           = NULL;
    const char *pszUser              = NULL;
    const char *pszPassword          = NULL;
    const char *pszFullUserName      = NULL;
    const char *pszProductKey        = NULL;
    const char *pszAdditionsIsoPath  = NULL;
    int         fInstallAdditions    = -1;
    const char *pszAuxiliaryBasePath = NULL;
    const char *pszMachineName       = NULL;
    const char *pszSettingsFile      = NULL;
    bool        fSetImageIdx         = false;
    uint32_t    idxImage             = 0;
    const char *pszSessionType       = "headless";

    /*
     * Parse options.
     */
    if (a->argc <= 1)
        return errorSyntax(USAGE_UNATTENDEDINSTALL, "Missing VM name/UUID.");

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--iso-path",             'i', RTGETOPT_REQ_STRING },
        { "--user",                 'u', RTGETOPT_REQ_STRING },
        { "--password",             'p', RTGETOPT_REQ_STRING },
        { "--full-user-name",       'U', RTGETOPT_REQ_STRING },
        { "--key",                  'k', RTGETOPT_REQ_STRING },
        { "--install-additions",    'A', RTGETOPT_REQ_NOTHING },
        { "--no-install-additions", 'N', RTGETOPT_REQ_NOTHING },
        { "--additions-iso-path",   'a', RTGETOPT_REQ_STRING },
        { "--auxiliary-base-path",  'x', RTGETOPT_REQ_STRING },
        { "--image-index",          'm', RTGETOPT_REQ_UINT32 },
        { "--settings-file",        's', RTGETOPT_REQ_STRING },
        { "--session-type",         'S', RTGETOPT_REQ_STRING },
    };

    RTGETOPTSTATE GetState;
    int vrc = RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), 0, RTGETOPTINIT_FLAGS_OPTS_FIRST);
    AssertRCReturn(vrc, RTEXITCODE_FAILURE);

    int c;
    RTGETOPTUNION ValueUnion;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case VINF_GETOPT_NOT_OPTION:
                if (pszMachineName)
                    return errorSyntax(USAGE_UNATTENDEDINSTALL, "VM name/UUID given more than once!");
                pszMachineName = ValueUnion.psz;
                if (*pszMachineName == '\0')
                    return errorSyntax(USAGE_UNATTENDEDINSTALL, "VM name/UUID is empty!");
                break;

            case 's':   // --settings-file <key value file>
                pszSettingsFile = ValueUnion.psz;
                break;

            case 'u':   // --user
                pszUser = ValueUnion.psz;
                break;

            case 'p':   // --password
                pszPassword = ValueUnion.psz;
                break;

            case 'U':   // --full-user-name
                pszFullUserName = ValueUnion.psz;
                break;

            case 'k':   // --key
                pszProductKey = ValueUnion.psz;
                break;

            case 'A':   // --install-additions
                fInstallAdditions = true;
                break;
            case 'N':   // --no-install-additions
                fInstallAdditions = false;
                break;
            case 'a':   // --additions-iso-path
                pszAdditionsIsoPath = ValueUnion.psz;
                break;

            case 'i':   // --iso-path
                pszIsoPath = ValueUnion.psz;
                break;

            case 'x':  // --auxiliary-base-path
                pszAuxiliaryBasePath = ValueUnion.psz;
                break;

            case 'm':   // --image-index
                idxImage = ValueUnion.u32;
                fSetImageIdx = true;
                break;

            case 'S':   // --session-type
                pszSessionType = ValueUnion.psz;
                break;

            default:
                return errorGetOpt(USAGE_UNATTENDEDINSTALL, c, &ValueUnion);
        }
    }

    /*
     * Check for required stuff.
     */
    if (pszMachineName == NULL)
        return errorSyntax(USAGE_UNATTENDEDINSTALL, "Missing VM name/UUID");

    if (!pszSettingsFile)
    {
        if (!pszUser)
            return errorSyntax(USAGE_UNATTENDEDINSTALL, "Missing required --user (or --settings-file) option");
        if (!pszPassword)
            return errorSyntax(USAGE_UNATTENDEDINSTALL, "Missing required --password (or --settings-file) option");
        if (!pszIsoPath)
            return errorSyntax(USAGE_UNATTENDEDINSTALL, "Missing required --iso-path (or --settings-file) option");
    }

    /*
     * Prepare.
     */

    /* try to find the given machine */
    HRESULT rc;
    ComPtr<IMachine> machine;
    Bstr bstrMachineName = pszMachineName;
    CHECK_ERROR(a->virtualBox, FindMachine(bstrMachineName.raw(), machine.asOutParam()));
    if (FAILED(rc))
        return RTEXITCODE_FAILURE;

    CHECK_ERROR_RET(machine, COMGETTER(Name)(bstrMachineName.asOutParam()), RTEXITCODE_FAILURE);
    Bstr bstrUuid;
    CHECK_ERROR_RET(machine, COMGETTER(Id)(bstrUuid.asOutParam()), RTEXITCODE_FAILURE);
    /* open a session for the VM */
    CHECK_ERROR_RET(machine, LockMachine(a->session, LockType_Shared), RTEXITCODE_FAILURE);

    /* get the associated console */
    ComPtr<IConsole> console;
    CHECK_ERROR(a->session, COMGETTER(Console)(console.asOutParam()));
    if (console)
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "Machine '%ls' is currently running", bstrMachineName.raw());

    /* ... and session machine */
    ComPtr<IMachine> sessionMachine;
    CHECK_ERROR_RET(a->session, COMGETTER(Machine)(sessionMachine.asOutParam()), RTEXITCODE_FAILURE);

    /* Get the OS type name of the VM. */
    BSTR bstrInstalledOS;
    CHECK_ERROR_RET(sessionMachine, COMGETTER(OSTypeId)(&bstrInstalledOS), RTEXITCODE_FAILURE);
    Utf8Str strInstalledOS(bstrInstalledOS);

    do
    {
        RTPrintf("Start unattended installation OS %s on virtual machine '%ls'.\n"
                 "UUID: %s\n",
                 strInstalledOS.c_str(),
                 bstrMachineName.raw(),
                 Utf8Str(bstrUuid).c_str());

        /*
         * Instantiate and configure the unattended installer.
         */
        ComPtr<IUnattended> unAttended;
        CHECK_ERROR_BREAK(machine, COMGETTER(Unattended)(unAttended.asOutParam()));

        /* Always calls 'done' to clean up from any aborted previous session. */
        CHECK_ERROR_BREAK(unAttended,Done());

        if (pszSettingsFile)
            CHECK_ERROR_BREAK(unAttended, LoadSettings(Bstr(pszSettingsFile).raw()));

        if (pszIsoPath)
            CHECK_ERROR_BREAK(unAttended, COMSETTER(IsoPath)(Bstr(pszIsoPath).raw()));
        if (pszUser)
            CHECK_ERROR_BREAK(unAttended, COMSETTER(User)(Bstr(pszUser).raw()));
        if (pszPassword)
            CHECK_ERROR_BREAK(unAttended, COMSETTER(Password)(Bstr(pszPassword).raw()));
        if (pszFullUserName)
            CHECK_ERROR_BREAK(unAttended, COMSETTER(FullUserName)(Bstr(pszFullUserName).raw()));
        if (pszProductKey)
            CHECK_ERROR_BREAK(unAttended, COMSETTER(ProductKey)(Bstr(pszProductKey).raw()));
        if (pszAdditionsIsoPath)
            CHECK_ERROR_BREAK(unAttended, COMSETTER(AdditionsIsoPath)(Bstr(pszAdditionsIsoPath).raw()));
        if (fInstallAdditions >= 0)
            CHECK_ERROR_BREAK(unAttended, COMSETTER(InstallGuestAdditions)(fInstallAdditions != (int)false));
        if (fSetImageIdx)
            CHECK_ERROR_BREAK(unAttended, COMSETTER(ImageIndex)(idxImage));
        if (pszAuxiliaryBasePath)
            CHECK_ERROR_BREAK(unAttended, COMSETTER(AuxiliaryBasePath)(Bstr(pszAuxiliaryBasePath).raw()));

        CHECK_ERROR_BREAK(unAttended,Prepare());
        CHECK_ERROR_BREAK(unAttended,ConstructMedia());
        CHECK_ERROR_BREAK(unAttended,ReconfigureVM());
        CHECK_ERROR_BREAK(unAttended,Done());

        /*
         * Start the VM.
         */
        a->session->UnlockMachine();

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
            CHECK_ERROR(machine, LaunchVMProcess(a->session, Bstr(pszSessionType).raw(), env.raw(), progress.asOutParam()));
            if (SUCCEEDED(rc) && !progress.isNull())
            {
                RTPrintf("Waiting for VM \"%s\" to power on...\n", Utf8Str(bstrUuid).c_str());
                CHECK_ERROR(progress, WaitForCompletion(-1));
                if (SUCCEEDED(rc))
                {
                    BOOL fCompleted = true;
                    CHECK_ERROR(progress, COMGETTER(Completed)(&fCompleted));
                    if (SUCCEEDED(rc))
                    {
                        ASSERT(fCompleted);

                        LONG iRc;
                        CHECK_ERROR(progress, COMGETTER(ResultCode)(&iRc));
                        if (SUCCEEDED(rc))
                        {
                            if (SUCCEEDED(iRc))
                                RTPrintf("VM \"%s\" has been successfully started.\n", Utf8Str(bstrUuid).c_str());
                            else
                            {
                                ProgressErrorInfo info(progress);
                                com::GluePrintErrorInfo(info);
                            }
                            rc = iRc;
                        }
                    }
                }
            }
        }

        /*
         * Retrieve and display the parameters actually used.
         */
        Bstr bstrAdditionsIsoPath;
        CHECK_ERROR_BREAK(unAttended, COMGETTER(AdditionsIsoPath)(bstrAdditionsIsoPath.asOutParam()));
        BOOL fInstallGuestAdditions = FALSE;
        CHECK_ERROR_BREAK(unAttended, COMGETTER(InstallGuestAdditions)(&fInstallGuestAdditions));
        Bstr bstrIsoPath;
        CHECK_ERROR_BREAK(unAttended, COMGETTER(IsoPath)(bstrIsoPath.asOutParam()));
        Bstr bstrUser;
        CHECK_ERROR_BREAK(unAttended, COMGETTER(User)(bstrUser.asOutParam()));
        Bstr bstrPassword;
        CHECK_ERROR_BREAK(unAttended, COMGETTER(Password)(bstrPassword.asOutParam()));
        Bstr bstrFullUserName;
        CHECK_ERROR_BREAK(unAttended, COMGETTER(FullUserName)(bstrFullUserName.asOutParam()));
        Bstr bstrProductKey;
        CHECK_ERROR_BREAK(unAttended, COMGETTER(ProductKey)(bstrProductKey.asOutParam()));
        Bstr bstrAuxiliaryBasePath;
        CHECK_ERROR_BREAK(unAttended, COMGETTER(AuxiliaryBasePath)(bstrAuxiliaryBasePath.asOutParam()));
        ULONG idxImageActual = 0;
        CHECK_ERROR_BREAK(unAttended, COMGETTER(ImageIndex)(&idxImageActual));
        RTPrintf("Got values:\n"
                 " user:                  %ls\n"
                 " password:              %ls\n"
                 " fullUserName:          %ls\n"
                 " productKey:            %ls\n"
                 " image index:           %u\n"
                 " isoPath:               %ls\n"
                 " additionsIsoPath:      %ls\n"
                 " installGuestAdditions: %RTbool\n"
                 " auxiliaryBasePath:     %ls",
                 bstrUser.raw(),
                 bstrPassword.raw(),
                 bstrFullUserName.raw(),
                 bstrProductKey.raw(),
                 idxImageActual,
                 bstrIsoPath.raw(),
                 bstrAdditionsIsoPath.raw(),
                 fInstallGuestAdditions,
                 bstrAuxiliaryBasePath.raw());
    } while (0);

    return SUCCEEDED(rc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}
