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

// display all of the VBoxUpdate settings
static RTEXITCODE doVBoxUpdateGetSettings(int argc, char **argv, ComPtr<IVirtualBox> aVirtualBox)
{
    int c;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    /** @todo r=brent decide on the best approach for options to specify here */
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--verbose",       'v',        RTGETOPT_REQ_NOTHING }
    };
    int vrc = RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 0 /* First */, 0);
    AssertRCReturn(vrc, RTEXITCODE_INIT);

    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 'h':
                printUsage(USAGE_UPDATECHECK, RTMSGREFENTRYSTR_SCOPE_GLOBAL, g_pStdOut);
                return RTEXITCODE_SUCCESS;

            default:
                return errorGetOpt(USAGE_UPDATECHECK, c, &ValueUnion);
        }
    }

    if (argc != 0)
        return errorSyntax(USAGE_UPDATECHECK, "Incorrect number of parameters");

    ComPtr<ISystemProperties> pSystemProperties;
    CHECK_ERROR2I_RET(aVirtualBox, COMGETTER(SystemProperties)(pSystemProperties.asOutParam()), RTEXITCODE_FAILURE);

    BOOL fVBoxUpdateEnabled;
    CHECK_ERROR2I_RET(pSystemProperties, COMGETTER(VBoxUpdateEnabled)(&fVBoxUpdateEnabled), RTEXITCODE_FAILURE);
    RTPrintf("Enabled:                %s\n", fVBoxUpdateEnabled ? "yes" : "no");

    ULONG cVBoxUpdateCount;
    CHECK_ERROR2I_RET(pSystemProperties, COMGETTER(VBoxUpdateCount)(&cVBoxUpdateCount), RTEXITCODE_FAILURE);
    RTPrintf("Count:                  %u\n", cVBoxUpdateCount);

    ULONG uVBoxUpdateFrequency;
    CHECK_ERROR2I_RET(pSystemProperties, COMGETTER(VBoxUpdateFrequency)(&uVBoxUpdateFrequency), RTEXITCODE_FAILURE);
    if (uVBoxUpdateFrequency == 0)
        RTPrintf("Frequency:              never\n");
    else if (uVBoxUpdateFrequency == 1)
        RTPrintf("Frequency:              every day\n");
    else
        RTPrintf("Frequency:              every %u days\n", uVBoxUpdateFrequency);

    VBoxUpdateTarget_T enmVBoxUpdateTarget;
    CHECK_ERROR2I_RET(pSystemProperties, COMGETTER(VBoxUpdateTarget)(&enmVBoxUpdateTarget), RTEXITCODE_FAILURE);
    const char *psz;
    switch (enmVBoxUpdateTarget)
    {
        case VBoxUpdateTarget_Stable:
            psz = "Stable: new minor and maintenance releases";
            break;
        case VBoxUpdateTarget_AllReleases:
            psz = "All releases: new minor, maintenance, and major releases";
            break;
        case VBoxUpdateTarget_WithBetas:
            psz = "With Betas: new minor, maintenance, major, and beta releases";
            break;
        default:
            psz = "Unset";
            break;
    }
    RTPrintf("Target:                 %s\n", psz);

    Bstr strVBoxUpdateLastCheckDate;
    CHECK_ERROR2I_RET(pSystemProperties, COMGETTER(VBoxUpdateLastCheckDate)(strVBoxUpdateLastCheckDate.asOutParam()),
                      RTEXITCODE_FAILURE);
    if (!strVBoxUpdateLastCheckDate.isEmpty())
        RTPrintf("Last Check Date:        %ls\n", strVBoxUpdateLastCheckDate.raw());

    return RTEXITCODE_SUCCESS;
}

static RTEXITCODE doVBoxUpdateModifySettings(int argc, char **argv, ComPtr<IVirtualBox> aVirtualBox)
{
    BOOL fEnableVBoxUpdate = false;
    BOOL fDisableVBoxUpdate = false;
    const char *pszVBoxUpdateTarget = NULL;
    uint32_t uVBoxUpdateFrequency = 0;

    int c;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;

#   define UPDATECHECK_MODSETTING_ENABLE        (VINF_GETOPT_NOT_OPTION + 'e')
#   define UPDATECHECK_MODSETTING_DISABLE       (VINF_GETOPT_NOT_OPTION + 'd')
#   define UPDATECHECK_MODSETTING_TARGET        (VINF_GETOPT_NOT_OPTION + 't')
#   define UPDATECHECK_MODSETTING_FREQUENCY     (VINF_GETOPT_NOT_OPTION + 'f')

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--enable",        UPDATECHECK_MODSETTING_ENABLE,    RTGETOPT_REQ_NOTHING },
        { "--disable",       UPDATECHECK_MODSETTING_DISABLE,   RTGETOPT_REQ_NOTHING },
        { "--target",        UPDATECHECK_MODSETTING_TARGET,    RTGETOPT_REQ_STRING },
        { "--frequency",     UPDATECHECK_MODSETTING_FREQUENCY, RTGETOPT_REQ_UINT32 },
    };
    int vrc = RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 0 /* First */, 0);
    AssertRCReturn(vrc, RTEXITCODE_INIT);

    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case UPDATECHECK_MODSETTING_ENABLE:
                fEnableVBoxUpdate = true;
                break;

            case UPDATECHECK_MODSETTING_DISABLE:
                fDisableVBoxUpdate = true;
                break;

            case UPDATECHECK_MODSETTING_TARGET:
                pszVBoxUpdateTarget = ValueUnion.psz;
                break;

            case UPDATECHECK_MODSETTING_FREQUENCY:
                uVBoxUpdateFrequency = ValueUnion.u32;
                break;

            case 'h':
                printUsage(USAGE_UPDATECHECK, RTMSGREFENTRYSTR_SCOPE_GLOBAL, g_pStdOut);
                return RTEXITCODE_SUCCESS;

            default:
                return errorGetOpt(USAGE_UPDATECHECK, c, &ValueUnion);
        }
    }

    if (argc == 0)
        return errorSyntax(USAGE_UPDATECHECK, "Incorrect number of parameters");

    if (fEnableVBoxUpdate && fDisableVBoxUpdate)
        return errorSyntax(USAGE_UPDATECHECK, "Invalid combination of options: --enable and --disable");

    ComPtr<ISystemProperties> pSystemProperties;
    CHECK_ERROR2I_RET(aVirtualBox, COMGETTER(SystemProperties)(pSystemProperties.asOutParam()), RTEXITCODE_FAILURE);

    if (pszVBoxUpdateTarget)
    {
        VBoxUpdateTarget_T enmVBoxUpdateTarget;
        if (!RTStrICmp(pszVBoxUpdateTarget, "stable"))
            enmVBoxUpdateTarget = VBoxUpdateTarget_Stable;
        else if (!RTStrICmp(pszVBoxUpdateTarget, "withbetas"))
            enmVBoxUpdateTarget = VBoxUpdateTarget_WithBetas;
        else if (!RTStrICmp(pszVBoxUpdateTarget, "allreleases"))
            enmVBoxUpdateTarget = VBoxUpdateTarget_AllReleases;
        else
            return errorArgument("Unknown target specified: '%s'", pszVBoxUpdateTarget);

        CHECK_ERROR2I_RET(pSystemProperties, COMSETTER(VBoxUpdateTarget)(enmVBoxUpdateTarget), RTEXITCODE_FAILURE);
    }

    if (fEnableVBoxUpdate)
    {
        CHECK_ERROR2I_RET(pSystemProperties, COMSETTER(VBoxUpdateEnabled)(true), RTEXITCODE_FAILURE);
    }
    else if (fDisableVBoxUpdate)
    {
        CHECK_ERROR2I_RET(pSystemProperties, COMSETTER(VBoxUpdateEnabled)(false), RTEXITCODE_FAILURE);
    }

    if (uVBoxUpdateFrequency)
    {
        CHECK_ERROR2I_RET(pSystemProperties, COMSETTER(VBoxUpdateFrequency)(uVBoxUpdateFrequency), RTEXITCODE_FAILURE);
    }

    return RTEXITCODE_SUCCESS;
}

static RTEXITCODE doVBoxUpdate(int argc, char **argv, ComPtr<IVirtualBox> aVirtualBox)
{
    int c;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    /** @todo r=brent decide on the best approach for options to specify here */
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--verbose",       'v',        RTGETOPT_REQ_NOTHING }
    };
    int vrc = RTGetOptInit(&GetState, argc, argv, s_aOptions, RT_ELEMENTS(s_aOptions), 0 /* First */, 0);
    AssertRCReturn(vrc, RTEXITCODE_INIT);

    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 'h':
                printUsage(USAGE_UPDATECHECK, RTMSGREFENTRYSTR_SCOPE_GLOBAL, g_pStdOut);
                return RTEXITCODE_SUCCESS;

            default:
                return errorGetOpt(USAGE_UPDATECHECK, c, &ValueUnion);
        }
    }

    ComPtr<IHost> pHost;
    CHECK_ERROR2I_RET(aVirtualBox, COMGETTER(Host)(pHost.asOutParam()), RTEXITCODE_FAILURE);

    ComPtr<IHostUpdate> pHostUpdate;
    CHECK_ERROR2I_RET(pHost, COMGETTER(Update)(pHostUpdate.asOutParam()), RTEXITCODE_FAILURE);

    UpdateCheckType_T updateCheckType = UpdateCheckType_VirtualBox;
    BOOL updateNeeded;
    ComPtr<IProgress> progress;

    RTPrintf("Checking for a new VirtualBox version...\n");

    // we don't call CHECK_ERROR2I_RET(pHostUpdate, VBoxUpdate(updateCheckType, ...); here so we can check for a specific
    // return value indicating update checks are disabled.
    HRESULT rc = pHostUpdate->UpdateCheck(updateCheckType, progress.asOutParam());
    if (FAILED(rc))
    {
        if (rc == E_NOTIMPL)
        {
            RTPrintf("VirtualBox update checking has been disabled.\n");
            return RTEXITCODE_SUCCESS;
        }

        if (progress.isNull())
            RTStrmPrintf(g_pStdErr, "Failed to create progress object: %Rhrc\n", rc);
        else
            com::GlueHandleComError(pHostUpdate, "VBoxUpdate(updateCheckType, progress.asOutParam())", rc,
                                    __FILE__, __LINE__);
        return RTEXITCODE_FAILURE;
    }

    /* HRESULT hrc = */ showProgress(progress);
    CHECK_PROGRESS_ERROR_RET(progress, ("Check for update failed."), RTEXITCODE_FAILURE);

    CHECK_ERROR2I_RET(pHostUpdate, COMGETTER(UpdateResponse)(&updateNeeded), RTEXITCODE_FAILURE);

    if (!updateNeeded)
    {
        RTPrintf("You are already running the most recent version of VirtualBox.\n");
    }
    else
    {
        Bstr updateVersion;
        Bstr updateURL;

        CHECK_ERROR2I_RET(pHostUpdate, COMGETTER(UpdateVersion)(updateVersion.asOutParam()), RTEXITCODE_FAILURE);
        CHECK_ERROR2I_RET(pHostUpdate, COMGETTER(UpdateURL)(updateURL.asOutParam()), RTEXITCODE_FAILURE);

        RTPrintf("A new version of VirtualBox has been released! Version %ls is available "
                 "at virtualbox.org.\nYou can download this version here: %ls\n", updateVersion.raw(), updateURL.raw());
    }

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
    if (!strcmp(a->argv[0], "perform"))
    {
        // VBoxManage updatecheck
        return doVBoxUpdate(a->argc - 1, &a->argv[1], a->virtualBox);
    }
    else if (!strcmp(a->argv[0], "getsettings"))
    {
        // VBoxManage updatecheck getsettings
        return doVBoxUpdateGetSettings(a->argc - 1, &a->argv[1], a->virtualBox);
    }
    else if (!strcmp(a->argv[0], "modifysettings"))
    {
        // VBoxManage updatecheck modifysettings key=value
        return doVBoxUpdateModifySettings(a->argc - 1, &a->argv[1], a->virtualBox);
    }
    else
    {
        printUsage(USAGE_UPDATECHECK, RTMSGREFENTRYSTR_SCOPE_GLOBAL, g_pStdOut);
        return RTEXITCODE_FAILURE;
    }
}

#endif /* !VBOX_ONLY_DOCS */
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
