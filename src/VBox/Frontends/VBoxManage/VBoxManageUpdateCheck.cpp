/* $Id$ */
/** @file
 * VBoxManage - The 'updatecheck' command.
 */

/*
 * Copyright (C) 2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_ONLY_DOCS


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <VBox/com/com.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint.h>
#include <VBox/com/VirtualBox.h>
#include <VBox/com/array.h>

#include <iprt/buildconfig.h>
#include <VBox/version.h>

#include <VBox/log.h>
#include <iprt/getopt.h>
#include <iprt/stream.h>
#include <iprt/ctype.h>
#include <iprt/message.h>

#include "VBoxManage.h"

using namespace com;    // SafeArray

/**
 * updatecheck getsettings
 */
static RTEXITCODE doVBoxUpdateGetSettings(int argc, char **argv, ComPtr<IVirtualBox> aVirtualBox)
{
    /*
     * Parse options.
     */
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--machine-readable", 'm', RTGETOPT_REQ_NOTHING }
    };
    RTGETOPTSTATE GetState;
    int vrc = RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 0 /* First */, 0);
    AssertRCReturn(vrc, RTEXITCODE_INIT);

    bool fMachineReadable = false;

    int c;
    RTGETOPTUNION ValueUnion;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 'm':
                fMachineReadable = true;
                break;

            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    /*
     * Do the work.
     */
    ComPtr<ISystemProperties> pSystemProperties;
    CHECK_ERROR2I_RET(aVirtualBox, COMGETTER(SystemProperties)(pSystemProperties.asOutParam()), RTEXITCODE_FAILURE);

    BOOL fVBoxUpdateEnabled;
    CHECK_ERROR2I_RET(pSystemProperties, COMGETTER(VBoxUpdateEnabled)(&fVBoxUpdateEnabled), RTEXITCODE_FAILURE);
    if (fMachineReadable)
        outputMachineReadableBool("enabled", &fVBoxUpdateEnabled);
    else
        RTPrintf("Enabled:                %s\n", fVBoxUpdateEnabled ? "yes" : "no");

    ULONG cVBoxUpdateCount;
    CHECK_ERROR2I_RET(pSystemProperties, COMGETTER(VBoxUpdateCount)(&cVBoxUpdateCount), RTEXITCODE_FAILURE);
    if (fMachineReadable)
        outputMachineReadableULong("count", &cVBoxUpdateCount);
    else
        RTPrintf("Count:                  %u\n", cVBoxUpdateCount);

    ULONG cDaysFrequencey;
    CHECK_ERROR2I_RET(pSystemProperties, COMGETTER(VBoxUpdateFrequency)(&cDaysFrequencey), RTEXITCODE_FAILURE);
    if (fMachineReadable)
        outputMachineReadableULong("frequency", &cDaysFrequencey);
    else if (cDaysFrequencey == 0)
        RTPrintf("Frequency:              never\n"); /** @todo r=bird: Two inconsistencies here. HostUpdateImpl.cpp code will indicate the need for updating if no last-check-date.  modifysettings cannot set it to zero (I added the error message, you just skipped setting it originally). */
    else if (cDaysFrequencey == 1)
        RTPrintf("Frequency:              every day\n");
    else
        RTPrintf("Frequency:              every %u days\n", cDaysFrequencey);

    VBoxUpdateTarget_T enmVBoxUpdateTarget;
    CHECK_ERROR2I_RET(pSystemProperties, COMGETTER(VBoxUpdateTarget)(&enmVBoxUpdateTarget), RTEXITCODE_FAILURE);
    const char *psz;
    const char *pszMachine;
    switch (enmVBoxUpdateTarget)
    {
        case VBoxUpdateTarget_Stable:
            psz = "Stable - new minor and maintenance releases";
            pszMachine = "stable";
            break;
        case VBoxUpdateTarget_AllReleases:
            psz = "All releases - new minor, maintenance, and major releases";
            pszMachine = "all-releases";
            break;
        case VBoxUpdateTarget_WithBetas:
            psz = "With Betas - new minor, maintenance, major, and beta releases";
            pszMachine = "with-betas";
            break;
        default:
            AssertFailed();
            psz = "Unset";
            pszMachine = "invalid";
            break;
    }
    if (fMachineReadable)
        outputMachineReadableString("target", pszMachine);
    else
        RTPrintf("Target:                 %s\n", psz);

    Bstr bstrLastCheckDate;
    CHECK_ERROR2I_RET(pSystemProperties, COMGETTER(VBoxUpdateLastCheckDate)(bstrLastCheckDate.asOutParam()),
                      RTEXITCODE_FAILURE);
    if (fMachineReadable)
        outputMachineReadableString("last-check-date", &bstrLastCheckDate);
    else if (bstrLastCheckDate.isNotEmpty())
        RTPrintf("Last Check Date:        %ls\n", bstrLastCheckDate.raw());

    return RTEXITCODE_SUCCESS;
}

/**
 * updatecheck modifysettings
 */
static RTEXITCODE doVBoxUpdateModifySettings(int argc, char **argv, ComPtr<IVirtualBox> aVirtualBox)
{
    /*
     * Parse options.
     */
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--enable",        'e', RTGETOPT_REQ_NOTHING },
        { "--disable",       'd', RTGETOPT_REQ_NOTHING },
        { "--target",        't', RTGETOPT_REQ_STRING },
        { "--frequency",     'f', RTGETOPT_REQ_UINT32 },
    };

    RTGETOPTSTATE GetState;
    int vrc = RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 0 /* First */, 0);
    AssertRCReturn(vrc, RTEXITCODE_INIT);

    int                         fEnabled                = -1; /* tristate: -1 (not modified), false, true */
    VBoxUpdateTarget_T const    enmVBoxUpdateTargetNil  = (VBoxUpdateTarget_T)-999;
    VBoxUpdateTarget_T          enmVBoxUpdateTarget     = enmVBoxUpdateTargetNil;
    uint32_t                    cDaysUpdateFrequency    = 0;

    int c;
    RTGETOPTUNION ValueUnion;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 'e':
                fEnabled = true;
                break;

            case 'd':
                fEnabled = false;
                break;

            case 't':
                if (!RTStrICmp(ValueUnion.psz, "stable"))
                    enmVBoxUpdateTarget = VBoxUpdateTarget_Stable;
                else if (!RTStrICmp(ValueUnion.psz, "withbetas"))
                    enmVBoxUpdateTarget = VBoxUpdateTarget_WithBetas;
                else if (!RTStrICmp(ValueUnion.psz, "allreleases"))
                    enmVBoxUpdateTarget = VBoxUpdateTarget_AllReleases;
                else
                    return errorArgument("Unknown target specified: '%s'", ValueUnion.psz);
                break;

            case 'f':
                cDaysUpdateFrequency = ValueUnion.u32;
                if (cDaysUpdateFrequency == 0)
                    return errorArgument("The update frequency cannot be zero");
                break;

            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    if (   fEnabled == -1
        && enmVBoxUpdateTarget != enmVBoxUpdateTargetNil
        && cDaysUpdateFrequency == 0)
        return errorSyntax("No change requested");

    /*
     * Make the changes.
     */
    ComPtr<ISystemProperties> pSystemProperties;
    CHECK_ERROR2I_RET(aVirtualBox, COMGETTER(SystemProperties)(pSystemProperties.asOutParam()), RTEXITCODE_FAILURE);

    if (enmVBoxUpdateTarget != enmVBoxUpdateTargetNil)
    {
        CHECK_ERROR2I_RET(pSystemProperties, COMSETTER(VBoxUpdateTarget)(enmVBoxUpdateTarget), RTEXITCODE_FAILURE);
    }

    if (fEnabled != -1)
    {
        CHECK_ERROR2I_RET(pSystemProperties, COMSETTER(VBoxUpdateEnabled)((BOOL)fEnabled), RTEXITCODE_FAILURE);
    }

    if (cDaysUpdateFrequency)
    {
        CHECK_ERROR2I_RET(pSystemProperties, COMSETTER(VBoxUpdateFrequency)(cDaysUpdateFrequency), RTEXITCODE_FAILURE);
    }

    return RTEXITCODE_SUCCESS;
}

/**
 * updatecheck perform
 */
static RTEXITCODE doVBoxUpdate(int argc, char **argv, ComPtr<IVirtualBox> aVirtualBox)
{
    /*
     * Parse arguments.
     */
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--machine-readable", 'm', RTGETOPT_REQ_NOTHING }
    };
    RTGETOPTSTATE GetState;
    int vrc = RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 0 /* First */, 0);
    AssertRCReturn(vrc, RTEXITCODE_INIT);

    bool fMachineReadable = false;

    int c;
    RTGETOPTUNION ValueUnion;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 'm':
                fMachineReadable = true;
                break;

            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    /*
     * Do the work.
     */
    ComPtr<IHost> pHost;
    CHECK_ERROR2I_RET(aVirtualBox, COMGETTER(Host)(pHost.asOutParam()), RTEXITCODE_FAILURE);

    ComPtr<IHostUpdate> pHostUpdate;
    CHECK_ERROR2I_RET(pHost, COMGETTER(Update)(pHostUpdate.asOutParam()), RTEXITCODE_FAILURE);

    UpdateCheckType_T updateCheckType = UpdateCheckType_VirtualBox;
    ComPtr<IProgress> ptrProgress;

    RTPrintf("Checking for a new VirtualBox version...\n");

    // we don't call CHECK_ERROR2I_RET(pHostUpdate, VBoxUpdate(updateCheckType, ...); here so we can check for a specific
    // return value indicating update checks are disabled.
    HRESULT rc = pHostUpdate->UpdateCheck(updateCheckType, ptrProgress.asOutParam());
    if (FAILED(rc))
    {
/** @todo r=bird: WTF? This makes no sense. I've commented upon this in the
 *        HostUpdateImpl.cpp code too. */
        if (rc == E_NOTIMPL)
        {
            RTPrintf("VirtualBox update checking has been disabled.\n");
            return RTEXITCODE_SUCCESS;
        }

        if (ptrProgress.isNull())
            RTStrmPrintf(g_pStdErr, "Failed to create ptrProgress object: %Rhrc\n", rc);
        else
            com::GlueHandleComError(pHostUpdate, "VBoxUpdate(updateCheckType, ptrProgress.asOutParam())", rc, __FILE__, __LINE__);
        return RTEXITCODE_FAILURE;
    }

    /* HRESULT hrc = */ showProgress(ptrProgress);
    CHECK_PROGRESS_ERROR_RET(ptrProgress, ("Check for update failed."), RTEXITCODE_FAILURE);

    BOOL fUpdateNeeded = FALSE;
    CHECK_ERROR2I_RET(pHostUpdate, COMGETTER(UpdateResponse)(&fUpdateNeeded), RTEXITCODE_FAILURE);

    if (fMachineReadable)
        outputMachineReadableBool("update-needed", &fUpdateNeeded);

    if (fUpdateNeeded)
    {
        Bstr bstrUpdateVersion;
        CHECK_ERROR2I_RET(pHostUpdate, COMGETTER(UpdateVersion)(bstrUpdateVersion.asOutParam()), RTEXITCODE_FAILURE);
        Bstr bstrUpdateURL;
        CHECK_ERROR2I_RET(pHostUpdate, COMGETTER(UpdateURL)(bstrUpdateURL.asOutParam()), RTEXITCODE_FAILURE);

        if (fMachineReadable)
            RTPrintf("A new version of VirtualBox has been released! Version %ls is available at virtualbox.org.\n"
                     "You can download this version here: %ls\n", bstrUpdateVersion.raw(), bstrUpdateURL.raw());
        else
        {
            outputMachineReadableString("update-version", &bstrUpdateVersion);
            outputMachineReadableString("update-url", &bstrUpdateURL);
        }
    }
    else if (!fMachineReadable)
        RTPrintf("You are already running the most recent version of VirtualBox.\n");

    return RTEXITCODE_SUCCESS;
}

/**
 * Handles the 'updatecheck' command.
 *
 * @returns Appropriate exit code.
 * @param   a                   Handler argument.
 */
RTEXITCODE handleUpdateCheck(HandlerArg *a)
{
    if (a->argc < 1)
        return errorNoSubcommand();
    if (!strcmp(a->argv[0], "perform"))
    {
        setCurrentSubcommand(HELP_SCOPE_UPDATECHECK_PERFORM);
        return doVBoxUpdate(a->argc - 1, &a->argv[1], a->virtualBox);
    }
    if (!strcmp(a->argv[0], "getsettings"))
    {
        setCurrentSubcommand(HELP_SCOPE_UPDATECHECK_GETSETTINGS);
        return doVBoxUpdateGetSettings(a->argc - 1, &a->argv[1], a->virtualBox);
    }
    if (!strcmp(a->argv[0], "modifysettings"))
    {
        setCurrentSubcommand(HELP_SCOPE_UPDATECHECK_MODIFYSETTINGS);
        return doVBoxUpdateModifySettings(a->argc - 1, &a->argv[1], a->virtualBox);
    }
    return errorUnknownSubcommand(a->argv[0]);
}

#endif /* !VBOX_ONLY_DOCS */
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
