/* $Id$ */
/** @file
 * VBoxManageCloud - The cloud related commands.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <VBox/com/com.h>
#include <VBox/com/string.h>
#include <VBox/com/Guid.h>
#include <VBox/com/array.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint.h>
#include <VBox/com/VirtualBox.h>

#include <iprt/ctype.h>
#include <iprt/getopt.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/uuid.h>
#include <iprt/file.h>
#include <VBox/log.h>

#include "VBoxManage.h"

#include <list>

using namespace com;//at least for Bstr

/**
 * Common Cloud options.
 */
typedef struct
{
    const char     *pszProviderName;
    const char     *pszProfileName;
} CLOUDCOMMONOPT;
typedef CLOUDCOMMONOPT *PCLOUDCOMMONOPT;

/**
 * List all available cloud instances for the specified cloud provider.
 * Available cloud instance is one which state whether "running" or "stopped".
 *
 * @returns RTEXITCODE
 * @param a is the list of passed arguments
 * @param iFirst is the position of the first unparsed argument in the arguments list
 * @param pCommonOpts is a pointer to the structure CLOUDCOMMONOPT with some common
 * arguments which have been already parsed before
 */
static RTEXITCODE listCloudInstances(HandlerArg *a, int iFirst, PCLOUDCOMMONOPT pCommonOpts)
{
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--compartment-id", 'c', RTGETOPT_REQ_STRING }
    };
    RTGETOPTSTATE GetState;
    RTGETOPTUNION ValueUnion;
    int vrc = RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), iFirst, 0);
    AssertRCReturn(vrc, RTEXITCODE_FAILURE);

    Utf8Str strCompartmentId;
    int c;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 'c':
                    strCompartmentId = ValueUnion.psz;
                break;
            case VINF_GETOPT_NOT_OPTION:
                return errorUnknownSubcommand(ValueUnion.psz);
            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    HRESULT hrc = S_OK;
    ComPtr<IVirtualBox> pVirtualBox = a->virtualBox;
    ComPtr<ICloudProviderManager> pCloudProviderManager;
    CHECK_ERROR2_RET(hrc, pVirtualBox,
                     COMGETTER(CloudProviderManager)(pCloudProviderManager.asOutParam()),
                     RTEXITCODE_FAILURE);
    ComPtr<ICloudProvider> pCloudProvider;

    CHECK_ERROR2_RET(hrc, pCloudProviderManager,
                     GetProviderByShortName(Bstr(pCommonOpts->pszProviderName).raw(), pCloudProvider.asOutParam()),
                     RTEXITCODE_FAILURE);
    ComPtr<ICloudProfile> pCloudProfile;

    CHECK_ERROR2_RET(hrc, pCloudProvider,
                     GetProfileByName(Bstr(pCommonOpts->pszProfileName).raw(), pCloudProfile.asOutParam()),
                     RTEXITCODE_FAILURE);

    if (strCompartmentId.isNotEmpty())
    {
        CHECK_ERROR2_RET(hrc, pCloudProfile,
                         SetProperty(Bstr("compartment").raw(), Bstr(strCompartmentId).raw()),
                         RTEXITCODE_FAILURE);
    }
    else
    {
        RTPrintf("Parameter \'compartment\' is empty or absent.\n"
                 "Trying to get the compartment from the passed cloud profile \'%s\'\n", pCommonOpts->pszProfileName);
        Bstr bStrCompartmentId;
        CHECK_ERROR2_RET(hrc, pCloudProfile,
                         GetProperty(Bstr("compartment").raw(), bStrCompartmentId.asOutParam()),
                         RTEXITCODE_FAILURE);
        strCompartmentId = bStrCompartmentId;
        RTPrintf("Found the compartment \'%s\':\n", strCompartmentId.c_str());
    }

    Bstr bstrProfileName;
    pCloudProfile->COMGETTER(Name)(bstrProfileName.asOutParam());

    ComObjPtr<ICloudClient> oCloudClient;
    CHECK_ERROR2_RET(hrc, pCloudProfile,
                     CreateCloudClient(oCloudClient.asOutParam()),
                     RTEXITCODE_FAILURE);

    com::SafeArray<BSTR> arrayVMNames;
    com::SafeArray<BSTR> arrayVMIds;
    RTPrintf("Getting a list of available cloud instances...\n");
    RTPrintf("Reply is in the form \'instance name\' = \'instance id\'\n");
    CHECK_ERROR2_RET(hrc, oCloudClient,
                     ListInstances(CloudMachineState_Running,
                                   ComSafeArrayAsOutParam(arrayVMNames),
                                   ComSafeArrayAsOutParam(arrayVMIds)),
                     RTEXITCODE_FAILURE);
    RTPrintf("List of available instances for the cloud profile \'%ls\' \nand compartment \'%s\':\n",
             bstrProfileName.raw(), strCompartmentId.c_str());
    size_t cIds = arrayVMIds.size();
    size_t cNames = arrayVMNames.size();
    for (size_t k = 0; k < cNames; k++)
    {
        Bstr value;
        if (k < cIds)
            value = arrayVMIds[k];
        RTPrintf("\t%ls = %ls\n", arrayVMNames[k], value.raw());
    }
    CHECK_ERROR2_RET(hrc, oCloudClient,
                     ListInstances(CloudMachineState_Stopped,
                                   ComSafeArrayAsOutParam(arrayVMNames),
                                   ComSafeArrayAsOutParam(arrayVMIds)),
                     RTEXITCODE_FAILURE);
    cNames = arrayVMNames.size();
    cIds = arrayVMIds.size();
    for (size_t k = 0; k < cNames; k++)
    {
        Bstr value;
        if (k < cIds)
            value = arrayVMIds[k];
        RTPrintf("\t%ls=%ls\n", arrayVMNames[k], value.raw());
    }

    return SUCCEEDED(hrc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

/**
 * List all available cloud images for the specified cloud provider.
 *
 * @returns RTEXITCODE
 * @param a is the list of passed arguments
 * @param iFirst is the position of the first unparsed argument in the arguments list
 * @param pCommonOpts is a pointer to the structure CLOUDCOMMONOPT with some common
 * arguments which have been already parsed before
 */
static RTEXITCODE listCloudImages(HandlerArg *a, int iFirst, PCLOUDCOMMONOPT pCommonOpts)
{
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--compartment-id", 'c', RTGETOPT_REQ_STRING }
    };
    RTGETOPTSTATE GetState;
    RTGETOPTUNION ValueUnion;
    int vrc = RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), iFirst, 0);
    AssertRCReturn(vrc, RTEXITCODE_FAILURE);

    Utf8Str strCompartmentId;
    int c;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 'c':
                    strCompartmentId = ValueUnion.psz;
                break;
            case VINF_GETOPT_NOT_OPTION:
                return errorUnknownSubcommand(ValueUnion.psz);
            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    HRESULT hrc = S_OK;
    ComPtr<IVirtualBox> pVirtualBox = a->virtualBox;
    ComPtr<ICloudProviderManager> pCloudProviderManager;
    CHECK_ERROR2_RET(hrc, pVirtualBox,
                     COMGETTER(CloudProviderManager)(pCloudProviderManager.asOutParam()),
                     RTEXITCODE_FAILURE);
    ComPtr<ICloudProvider> pCloudProvider;

    CHECK_ERROR2_RET(hrc, pCloudProviderManager,
                     GetProviderByShortName(Bstr(pCommonOpts->pszProviderName).raw(), pCloudProvider.asOutParam()),
                     RTEXITCODE_FAILURE);
    ComPtr<ICloudProfile> pCloudProfile;

    CHECK_ERROR2_RET(hrc, pCloudProvider,
                     GetProfileByName(Bstr(pCommonOpts->pszProfileName).raw(), pCloudProfile.asOutParam()),
                     RTEXITCODE_FAILURE);
    if (strCompartmentId.isNotEmpty())
    {
        CHECK_ERROR2_RET(hrc, pCloudProfile,
                         SetProperty(Bstr("compartment").raw(), Bstr(strCompartmentId).raw()),\
                         RTEXITCODE_FAILURE);
    }
    else
    {
        RTPrintf("Parameter \'compartment\' is empty or absent.\n"
                 "Trying to get the compartment from the passed cloud profile \'%s\'\n", pCommonOpts->pszProfileName);
        Bstr bStrCompartmentId;
        CHECK_ERROR2_RET(hrc, pCloudProfile,
                         GetProperty(Bstr("compartment").raw(), bStrCompartmentId.asOutParam()),
                         RTEXITCODE_FAILURE);
        strCompartmentId = bStrCompartmentId;
        RTPrintf("Found the compartment \'%s\':\n", strCompartmentId.c_str());
    }

    Bstr bstrProfileName;
    pCloudProfile->COMGETTER(Name)(bstrProfileName.asOutParam());

    ComObjPtr<ICloudClient> oCloudClient;
    CHECK_ERROR2_RET(hrc, pCloudProfile,
                     CreateCloudClient(oCloudClient.asOutParam()),
                     RTEXITCODE_FAILURE);
    com::SafeArray<BSTR> arrayVMNames;
    com::SafeArray<BSTR> arrayVMIds;
    RTPrintf("Getting a list of available cloud images...\n");
    RTPrintf("Reply is in the form \'image name\' = \'image id\'\n");
    CHECK_ERROR2_RET(hrc, oCloudClient,
                     ListImages(CloudImageState_Available,
                                ComSafeArrayAsOutParam(arrayVMNames),
                                ComSafeArrayAsOutParam(arrayVMIds)),
                     RTEXITCODE_FAILURE);
    RTPrintf("List of available images for the cloud profile \'%ls\' \nand compartment \'%s\':\n",
             bstrProfileName.raw(), strCompartmentId.c_str());
    size_t cNames = arrayVMNames.size();
    size_t cIds = arrayVMIds.size();
    for (size_t k = 0; k < cNames; k++)
    {
        Bstr value;
        if (k < cIds)
            value = arrayVMIds[k];
        RTPrintf("\t%ls = %ls\n", arrayVMNames[k], value.raw());
    }

    return SUCCEEDED(hrc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

/**
 * General function which handles the "list" commands
 *
 * @returns RTEXITCODE
 * @param a is the list of passed arguments
 * @param iFirst is the position of the first unparsed argument in the arguments list
 * @param pCommonOpts is a pointer to the structure CLOUDCOMMONOPT with some common
 * arguments which have been already parsed before
 */
static RTEXITCODE handleCloudLists(HandlerArg *a, int iFirst, PCLOUDCOMMONOPT pCommonOpts)
{
    if (a->argc < 1)
        return errorNoSubcommand();

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "images",              1000, RTGETOPT_REQ_NOTHING },
        { "instances",           1001, RTGETOPT_REQ_NOTHING },
        { "networks",            1002, RTGETOPT_REQ_NOTHING },
        { "subnets",             1003, RTGETOPT_REQ_NOTHING },
        { "vcns",                1004, RTGETOPT_REQ_NOTHING },
        { "objects",             1005, RTGETOPT_REQ_NOTHING }
    };


    Bstr bstrProvider(pCommonOpts->pszProviderName);
    Bstr bstrProfile(pCommonOpts->pszProfileName);

    /* check for required options */
    if (bstrProvider.isEmpty())
        return errorSyntax(USAGE_CLOUDPROFILE, "Parameter --provider is required");
    if (bstrProfile.isEmpty())
        return errorSyntax(USAGE_CLOUDPROFILE, "Parameter --profile is required");

    RTGETOPTSTATE GetState;
    int vrc = RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), iFirst, 0);
    AssertRCReturn(vrc, RTEXITCODE_FAILURE);

    int c;
    RTGETOPTUNION ValueUnion;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 1000:
//              setCurrentSubcommand(HELP_SCOPE_CLOUDIMAGE_CREATE);
                return listCloudImages(a, GetState.iNext, pCommonOpts);
            case 1001:
//              setCurrentSubcommand(HELP_SCOPE_CLOUDIMAGE_INFO);
                return listCloudInstances(a, GetState.iNext, pCommonOpts);
            case VINF_GETOPT_NOT_OPTION:
                return errorUnknownSubcommand(ValueUnion.psz);

            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    return errorNoSubcommand();
}

RTEXITCODE handleCloud(HandlerArg *a)
{
    if (a->argc < 1)
        return errorNoSubcommand();

    static const RTGETOPTDEF s_aOptions[] =
    {
        /* common options */
        { "--provider",     'v', RTGETOPT_REQ_STRING },
        { "--profile",      'f', RTGETOPT_REQ_STRING },
        { "list",             1000, RTGETOPT_REQ_NOTHING },
        { "image",            1001, RTGETOPT_REQ_NOTHING },
        { "instance",         1002, RTGETOPT_REQ_NOTHING },
        { "network",          1003, RTGETOPT_REQ_NOTHING },
        { "volume",           1004, RTGETOPT_REQ_NOTHING },
        { "object",           1005, RTGETOPT_REQ_NOTHING }
    };

    RTGETOPTSTATE GetState;
    int vrc = RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), 0, 0);
    AssertRCReturn(vrc, RTEXITCODE_FAILURE);

    CLOUDCOMMONOPT   CommonOpts = { NULL, NULL };
    int c;
    RTGETOPTUNION ValueUnion;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 'v':   // --provider
                CommonOpts.pszProviderName = ValueUnion.psz;
                break;
            case 'f':   // --profile
                CommonOpts.pszProfileName = ValueUnion.psz;
                break;
            /* Sub-commands: */
            case 1000:
//              setCurrentSubcommand(HELP_SCOPE_CLOUDLIST);
                return handleCloudLists(a, GetState.iNext, &CommonOpts);
            case VINF_GETOPT_NOT_OPTION:
                return errorUnknownSubcommand(ValueUnion.psz);

            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    return errorNoSubcommand();
}
