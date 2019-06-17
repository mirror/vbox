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
    struct {
        const char     *pszProviderName;
        ComPtr<ICloudProvider> pCloudProvider;
    }provider;
    struct {
        const char     *pszProfileName;
        ComPtr<ICloudProfile> pCloudProfile;
    }profile;

} CLOUDCOMMONOPT;
typedef CLOUDCOMMONOPT *PCLOUDCOMMONOPT;

static HRESULT checkAndSetCommonOptions(HandlerArg *a, PCLOUDCOMMONOPT pCommonOpts)
{
    HRESULT hrc = S_OK;

    Bstr bstrProvider(pCommonOpts->provider.pszProviderName);
    Bstr bstrProfile(pCommonOpts->profile.pszProfileName);

    /* check for required options */
    if (bstrProvider.isEmpty())
    {
        errorSyntax(USAGE_S_NEWCMD, "Parameter --provider is required");
        return E_FAIL;
    }
    if (bstrProfile.isEmpty())
    {
        errorSyntax(USAGE_S_NEWCMD, "Parameter --profile is required");
        return E_FAIL;
    }

    ComPtr<IVirtualBox> pVirtualBox = a->virtualBox;
    ComPtr<ICloudProviderManager> pCloudProviderManager;
    CHECK_ERROR2_RET(hrc, pVirtualBox,
                     COMGETTER(CloudProviderManager)(pCloudProviderManager.asOutParam()),
                     RTEXITCODE_FAILURE);

    ComPtr<ICloudProvider> pCloudProvider;
    CHECK_ERROR2_RET(hrc, pCloudProviderManager,
                     GetProviderByShortName(bstrProvider.raw(), pCloudProvider.asOutParam()),
                     RTEXITCODE_FAILURE);
    pCommonOpts->provider.pCloudProvider = pCloudProvider;

    ComPtr<ICloudProfile> pCloudProfile;
    CHECK_ERROR2_RET(hrc, pCloudProvider,
                     GetProfileByName(bstrProfile.raw(), pCloudProfile.asOutParam()),
                     RTEXITCODE_FAILURE);
    pCommonOpts->profile.pCloudProfile = pCloudProfile;

    return hrc;
}

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
        { "--compartment-id", 'c', RTGETOPT_REQ_STRING },
        { "--state",          's', RTGETOPT_REQ_STRING }
    };
    RTGETOPTSTATE GetState;
    RTGETOPTUNION ValueUnion;
    int vrc = RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), iFirst, 0);
    AssertRCReturn(vrc, RTEXITCODE_FAILURE);

    Utf8Str strCompartmentId;
    Utf8Str strState;

    int c;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 'c':
                strCompartmentId = ValueUnion.psz;
                break;
            case 's':
                strState = ValueUnion.psz;
                break;
            case VINF_GETOPT_NOT_OPTION:
                return errorUnknownSubcommand(ValueUnion.psz);
            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    com::SafeArray<CloudMachineState_T> machimeStates;
    if (strState.isNotEmpty())
    {
        if (strState.equals("running"))
            machimeStates.push_back(CloudMachineState_Running);
        else if (strState.equals("paused"))
            machimeStates.push_back(CloudMachineState_Stopped);
        else if (strState.equals("terminated"))
            machimeStates.push_back(CloudMachineState_Terminated);
    }

    HRESULT hrc = S_OK;
    ComPtr<IVirtualBox> pVirtualBox = a->virtualBox;
    ComPtr<ICloudProviderManager> pCloudProviderManager;
    CHECK_ERROR2_RET(hrc, pVirtualBox,
                     COMGETTER(CloudProviderManager)(pCloudProviderManager.asOutParam()),
                     RTEXITCODE_FAILURE);
    ComPtr<ICloudProvider> pCloudProvider;

    CHECK_ERROR2_RET(hrc, pCloudProviderManager,
                     GetProviderByShortName(Bstr(pCommonOpts->provider.pszProviderName).raw(), pCloudProvider.asOutParam()),
                     RTEXITCODE_FAILURE);
    ComPtr<ICloudProfile> pCloudProfile;

    CHECK_ERROR2_RET(hrc, pCloudProvider,
                     GetProfileByName(Bstr(pCommonOpts->profile.pszProfileName).raw(), pCloudProfile.asOutParam()),
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
                 "Trying to get the compartment from the passed cloud profile \'%s\'\n", pCommonOpts->profile.pszProfileName);
        Bstr bStrCompartmentId;
        CHECK_ERROR2_RET(hrc, pCloudProfile,
                         GetProperty(Bstr("compartment").raw(), bStrCompartmentId.asOutParam()),
                         RTEXITCODE_FAILURE);
        strCompartmentId = bStrCompartmentId;
        if (strCompartmentId.isNotEmpty())
            RTPrintf("Found the compartment \'%s\':\n", strCompartmentId.c_str());
        else
            return errorSyntax(USAGE_S_NEWCMD, "Parameter --compartment-id is required");
    }

    Bstr bstrProfileName;
    pCloudProfile->COMGETTER(Name)(bstrProfileName.asOutParam());

    ComObjPtr<ICloudClient> oCloudClient;
    CHECK_ERROR2_RET(hrc, pCloudProfile,
                     CreateCloudClient(oCloudClient.asOutParam()),
                     RTEXITCODE_FAILURE);

    ComPtr<IStringArray> pVMNamesHolder;
    ComPtr<IStringArray> pVMIdsHolder;
    com::SafeArray<BSTR> arrayVMNames;
    com::SafeArray<BSTR> arrayVMIds;
    ComPtr<IProgress> pProgress;

    RTPrintf("Reply is in the form \'instance name\' = \'instance id\'\n");

    CHECK_ERROR2_RET(hrc, oCloudClient,
                     ListInstances(ComSafeArrayAsInParam(machimeStates),
                                   pVMNamesHolder.asOutParam(),
                                   pVMIdsHolder.asOutParam(),
                                   pProgress.asOutParam()),
                     RTEXITCODE_FAILURE);
    showProgress(pProgress);
    CHECK_PROGRESS_ERROR_RET(pProgress, ("Failed to list instances"), RTEXITCODE_FAILURE);

    CHECK_ERROR2_RET(hrc,
        pVMNamesHolder, COMGETTER(Values)(ComSafeArrayAsOutParam(arrayVMNames)),
            RTEXITCODE_FAILURE);
    CHECK_ERROR2_RET(hrc,
        pVMIdsHolder, COMGETTER(Values)(ComSafeArrayAsOutParam(arrayVMIds)),
            RTEXITCODE_FAILURE);

    RTPrintf("The list of the instances for the cloud profile \'%ls\' \nand compartment \'%s\':\n",
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
        { "--compartment-id", 'c', RTGETOPT_REQ_STRING },
        { "--state",          's', RTGETOPT_REQ_STRING }
    };
    RTGETOPTSTATE GetState;
    RTGETOPTUNION ValueUnion;
    int vrc = RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), iFirst, 0);
    AssertRCReturn(vrc, RTEXITCODE_FAILURE);

    Utf8Str strCompartmentId;
    Utf8Str strState;
    int c;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 'c':
                strCompartmentId = ValueUnion.psz;
                break;
            case 's':
                strState = ValueUnion.psz;
                break;
            case VINF_GETOPT_NOT_OPTION:
                return errorUnknownSubcommand(ValueUnion.psz);
            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    com::SafeArray<CloudImageState_T> imageStates;
    if (strState.isNotEmpty())
    {
        if (strState.equals("available"))
            imageStates.push_back(CloudImageState_Available);
        else if (strState.equals("disabled"))
            imageStates.push_back(CloudImageState_Disabled);
        else if (strState.equals("deleted"))
            imageStates.push_back(CloudImageState_Deleted);
    }

    HRESULT hrc = S_OK;
    ComPtr<IVirtualBox> pVirtualBox = a->virtualBox;
    ComPtr<ICloudProviderManager> pCloudProviderManager;
    CHECK_ERROR2_RET(hrc, pVirtualBox,
                     COMGETTER(CloudProviderManager)(pCloudProviderManager.asOutParam()),
                     RTEXITCODE_FAILURE);
    ComPtr<ICloudProvider> pCloudProvider;

    CHECK_ERROR2_RET(hrc, pCloudProviderManager,
                     GetProviderByShortName(Bstr(pCommonOpts->provider.pszProviderName).raw(), pCloudProvider.asOutParam()),
                     RTEXITCODE_FAILURE);
    ComPtr<ICloudProfile> pCloudProfile;

    CHECK_ERROR2_RET(hrc, pCloudProvider,
                     GetProfileByName(Bstr(pCommonOpts->profile.pszProfileName).raw(), pCloudProfile.asOutParam()),
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
                 "Trying to get the compartment from the passed cloud profile \'%s\'\n", pCommonOpts->profile.pszProfileName);
        Bstr bStrCompartmentId;
        CHECK_ERROR2_RET(hrc, pCloudProfile,
                         GetProperty(Bstr("compartment").raw(), bStrCompartmentId.asOutParam()),
                         RTEXITCODE_FAILURE);
        strCompartmentId = bStrCompartmentId;
        if (strCompartmentId.isNotEmpty())
            RTPrintf("Found the compartment \'%s\':\n", strCompartmentId.c_str());
        else
            return errorSyntax(USAGE_S_NEWCMD, "Parameter --compartment-id is required");
    }

    Bstr bstrProfileName;
    pCloudProfile->COMGETTER(Name)(bstrProfileName.asOutParam());

    ComObjPtr<ICloudClient> oCloudClient;
    CHECK_ERROR2_RET(hrc, pCloudProfile,
                     CreateCloudClient(oCloudClient.asOutParam()),
                     RTEXITCODE_FAILURE);

    ComPtr<IStringArray> pVMNamesHolder;
    ComPtr<IStringArray> pVMIdsHolder;
    com::SafeArray<BSTR> arrayVMNames;
    com::SafeArray<BSTR> arrayVMIds;
    ComPtr<IProgress> pProgress;

    RTPrintf("Reply is in the form \'image name\' = \'image id\'\n");
    CHECK_ERROR2_RET(hrc, oCloudClient,
                     ListImages(ComSafeArrayAsInParam(imageStates),
                                pVMNamesHolder.asOutParam(),
                                pVMIdsHolder.asOutParam(),
                                pProgress.asOutParam()),
                     RTEXITCODE_FAILURE);
    showProgress(pProgress);
    CHECK_PROGRESS_ERROR_RET(pProgress, ("Failed to list images"), RTEXITCODE_FAILURE);

    CHECK_ERROR2_RET(hrc,
        pVMNamesHolder, COMGETTER(Values)(ComSafeArrayAsOutParam(arrayVMNames)),
            RTEXITCODE_FAILURE);
    CHECK_ERROR2_RET(hrc,
        pVMIdsHolder, COMGETTER(Values)(ComSafeArrayAsOutParam(arrayVMIds)),
            RTEXITCODE_FAILURE);

    RTPrintf("The list of the images for the cloud profile \'%ls\' \nand compartment \'%s\':\n",
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

    Bstr bstrProvider(pCommonOpts->provider.pszProviderName);
    Bstr bstrProfile(pCommonOpts->profile.pszProfileName);

    /* check for required options */
    if (bstrProvider.isEmpty())
        return errorSyntax(USAGE_S_NEWCMD, "Parameter --provider is required");
    if (bstrProfile.isEmpty())
        return errorSyntax(USAGE_S_NEWCMD, "Parameter --profile is required");

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
//              setCurrentSubcommand(HELP_SCOPE_CLOUDIMAGE_LIST);
                return listCloudImages(a, GetState.iNext, pCommonOpts);
            case 1001:
//              setCurrentSubcommand(HELP_SCOPE_CLOUDINSTANCE_LIST);
                return listCloudInstances(a, GetState.iNext, pCommonOpts);
            case VINF_GETOPT_NOT_OPTION:
                return errorUnknownSubcommand(ValueUnion.psz);

            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    return errorNoSubcommand();
}

static RTEXITCODE createCloudInstance(HandlerArg *a, int iFirst, PCLOUDCOMMONOPT pCommonOpts)
{
    RT_NOREF(a);
    RT_NOREF(iFirst);
    RT_NOREF(pCommonOpts);
    return RTEXITCODE_SUCCESS;
}

static RTEXITCODE updateCloudInstance(HandlerArg *a, int iFirst, PCLOUDCOMMONOPT pCommonOpts)
{
    RT_NOREF(a);
    RT_NOREF(iFirst);
    RT_NOREF(pCommonOpts);
    return RTEXITCODE_SUCCESS;
}

static RTEXITCODE showCloudInstanceInfo(HandlerArg *a, int iFirst, PCLOUDCOMMONOPT pCommonOpts)
{
    HRESULT hrc = S_OK;

    hrc = checkAndSetCommonOptions(a, pCommonOpts);
    if (FAILED(hrc))
        return RTEXITCODE_FAILURE;

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--id", 'i', RTGETOPT_REQ_STRING }
    };
    RTGETOPTSTATE GetState;
    RTGETOPTUNION ValueUnion;
    int vrc = RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), iFirst, 0);
    AssertRCReturn(vrc, RTEXITCODE_FAILURE);

    Utf8Str strInstanceId;
    int c;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 'i':
                    strInstanceId = ValueUnion.psz;
                break;
            case VINF_GETOPT_NOT_OPTION:
                return errorUnknownSubcommand(ValueUnion.psz);
            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    Bstr bstrProfileName;
    ComPtr<ICloudProfile> pCloudProfile = pCommonOpts->profile.pCloudProfile;
    pCloudProfile->COMGETTER(Name)(bstrProfileName.asOutParam());

    ComObjPtr<ICloudClient> oCloudClient;
    CHECK_ERROR2_RET(hrc, pCloudProfile,
                     CreateCloudClient(oCloudClient.asOutParam()),
                     RTEXITCODE_FAILURE);
    RTPrintf("Getting information about cloud instance with id %s...\n", strInstanceId.c_str());
    RTPrintf("Reply is in the form \'setting name\' = \'value\'\n");

    ComPtr<IAppliance> pAppliance;
    CHECK_ERROR2_RET(hrc, a->virtualBox, CreateAppliance(pAppliance.asOutParam()), RTEXITCODE_FAILURE);

    com::SafeIfaceArray<IVirtualSystemDescription> vsdArray;
    ULONG requestedVSDnums = 1;
    ULONG newVSDnums = 0;
    CHECK_ERROR2_RET(hrc, pAppliance, CreateVirtualSystemDescriptions(requestedVSDnums, &newVSDnums), RTEXITCODE_FAILURE);
    if (requestedVSDnums != newVSDnums)
        return RTEXITCODE_FAILURE;

    CHECK_ERROR2_RET(hrc, pAppliance, COMGETTER(VirtualSystemDescriptions)(ComSafeArrayAsOutParam(vsdArray)), RTEXITCODE_FAILURE);
    ComPtr<IVirtualSystemDescription> instanceDescription = vsdArray[0];

    ComPtr<IProgress> progress;
    CHECK_ERROR2_RET(hrc, oCloudClient,
                     GetInstanceInfo(Bstr(strInstanceId.c_str()).raw(), instanceDescription, progress.asOutParam()),
                     RTEXITCODE_FAILURE);

    hrc = showProgress(progress);
    CHECK_PROGRESS_ERROR_RET(progress, ("Getting information about cloud instance failed"), RTEXITCODE_FAILURE);

    RTPrintf("Cloud instance info (provider '%s'):\n",
             pCommonOpts->provider.pszProviderName);

    struct vsdHReadable {
        VirtualSystemDescriptionType_T vsdType;
        Utf8Str strFound;
        Utf8Str strNotFound;
    };

    size_t vsdHReadableArraySize = 9;//the number of items in the vsdHReadableArray
    vsdHReadable vsdHReadableArray[9] = {
        {VirtualSystemDescriptionType_CloudDomain, "Availability domain = '%ls'\n", "Availability domain wasn't found\n"},
        {VirtualSystemDescriptionType_Name, "Instance displayed name = '%ls'\n", "Instance displayed name wasn't found\n"},
        {VirtualSystemDescriptionType_CloudInstanceState, "Instance state = '%ls'\n", "Instance state wasn't found\n"},
        {VirtualSystemDescriptionType_CloudInstanceId, "Instance Id = '%ls'\n", "Instance Id wasn't found\n"},
        {VirtualSystemDescriptionType_CloudImageId, "Bootable image Id = '%ls'\n",
            "Image Id whom the instance is booted up wasn't found\n"},
        {VirtualSystemDescriptionType_CloudInstanceShape, "Shape of the instance = '%ls'\n",
            "The shape of the instance wasn't found\n"},
        {VirtualSystemDescriptionType_OS, "Type of guest OS = '%ls'\n", "Type of guest OS wasn't found.\n"},
        {VirtualSystemDescriptionType_Memory, "RAM = '%ls MB'\n", "Value for RAM wasn't found\n"},
        {VirtualSystemDescriptionType_CPU, "CPUs = '%ls'\n", "Numbers of CPUs weren't found\n"}
    };

    com::SafeArray<VirtualSystemDescriptionType_T> retTypes;
    com::SafeArray<BSTR> aRefs;
    com::SafeArray<BSTR> aOvfValues;
    com::SafeArray<BSTR> aVBoxValues;
    com::SafeArray<BSTR> aExtraConfigValues;

    for (size_t i=0; i<vsdHReadableArraySize ; ++i)
    {
        hrc = instanceDescription->GetDescriptionByType(vsdHReadableArray[i].vsdType,
                                                        ComSafeArrayAsOutParam(retTypes),
                                                        ComSafeArrayAsOutParam(aRefs),
                                                        ComSafeArrayAsOutParam(aOvfValues),
                                                        ComSafeArrayAsOutParam(aVBoxValues),
                                                        ComSafeArrayAsOutParam(aExtraConfigValues));
        if (FAILED(hrc) || aVBoxValues.size() == 0)
            LogRel((vsdHReadableArray[i].strNotFound.c_str()));
        else
            RTPrintf(vsdHReadableArray[i].strFound.c_str(), aVBoxValues[0]);

        retTypes.setNull();
        aRefs.setNull();
        aOvfValues.setNull();
        aVBoxValues.setNull();
        aExtraConfigValues.setNull();
    }

    return SUCCEEDED(hrc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

static RTEXITCODE startCloudInstance(HandlerArg *a, int iFirst, PCLOUDCOMMONOPT pCommonOpts)
{
    HRESULT hrc = S_OK;
    hrc = checkAndSetCommonOptions(a, pCommonOpts);
    if (FAILED(hrc))
        return RTEXITCODE_FAILURE;

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--id", 'i', RTGETOPT_REQ_STRING }
    };
    RTGETOPTSTATE GetState;
    RTGETOPTUNION ValueUnion;
    int vrc = RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), iFirst, 0);
    AssertRCReturn(vrc, RTEXITCODE_FAILURE);

    Utf8Str strInstanceId;
    int c;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 'i':
                    strInstanceId = ValueUnion.psz;
                break;
            case VINF_GETOPT_NOT_OPTION:
                return errorUnknownSubcommand(ValueUnion.psz);
            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    Bstr bstrProfileName;
    ComPtr<ICloudProfile> pCloudProfile = pCommonOpts->profile.pCloudProfile;
    pCloudProfile->COMGETTER(Name)(bstrProfileName.asOutParam());

    ComObjPtr<ICloudClient> oCloudClient;
    CHECK_ERROR2_RET(hrc, pCloudProfile,
                     CreateCloudClient(oCloudClient.asOutParam()),
                     RTEXITCODE_FAILURE);
    RTPrintf("Starting cloud instance with id %s...\n", strInstanceId.c_str());

    ComPtr<IProgress> progress;
    CHECK_ERROR2_RET(hrc, oCloudClient,
                     StartInstance(Bstr(strInstanceId.c_str()).raw(), progress.asOutParam()),
                     RTEXITCODE_FAILURE);
    hrc = showProgress(progress);
    CHECK_PROGRESS_ERROR_RET(progress, ("Starting the cloud instance failed"), RTEXITCODE_FAILURE);

    if (SUCCEEDED(hrc))
        RTPrintf("Cloud instance with id %s (provider = '%s', profile = '%s') was started\n",
                 strInstanceId.c_str(),
                 pCommonOpts->provider.pszProviderName,
                 pCommonOpts->profile.pszProfileName);

    return SUCCEEDED(hrc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

static RTEXITCODE pauseCloudInstance(HandlerArg *a, int iFirst, PCLOUDCOMMONOPT pCommonOpts)
{
    HRESULT hrc = S_OK;
    hrc = checkAndSetCommonOptions(a, pCommonOpts);

    if (FAILED(hrc))
        return RTEXITCODE_FAILURE;

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--id", 'i', RTGETOPT_REQ_STRING }
    };
    RTGETOPTSTATE GetState;
    RTGETOPTUNION ValueUnion;
    int vrc = RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), iFirst, 0);
    AssertRCReturn(vrc, RTEXITCODE_FAILURE);

    Utf8Str strInstanceId;
    int c;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 'i':
                    strInstanceId = ValueUnion.psz;
                break;
            case VINF_GETOPT_NOT_OPTION:
                return errorUnknownSubcommand(ValueUnion.psz);
            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    Bstr bstrProfileName;
    ComPtr<ICloudProfile> pCloudProfile = pCommonOpts->profile.pCloudProfile;
    pCloudProfile->COMGETTER(Name)(bstrProfileName.asOutParam());

    ComObjPtr<ICloudClient> oCloudClient;
    CHECK_ERROR2_RET(hrc, pCloudProfile,
                     CreateCloudClient(oCloudClient.asOutParam()),
                     RTEXITCODE_FAILURE);
    RTPrintf("Pausing cloud instance with id %s...\n", strInstanceId.c_str());

    ComPtr<IProgress> progress;
    CHECK_ERROR2_RET(hrc, oCloudClient,
                     PauseInstance(Bstr(strInstanceId.c_str()).raw(), progress.asOutParam()),
                     RTEXITCODE_FAILURE);
    hrc = showProgress(progress);
    CHECK_PROGRESS_ERROR_RET(progress, ("Pause the cloud instance failed"), RTEXITCODE_FAILURE);

    if (SUCCEEDED(hrc))
        RTPrintf("Cloud instance with id %s (provider = '%s', profile = '%s') was paused\n",
                 strInstanceId.c_str(),
                 pCommonOpts->provider.pszProviderName,
                 pCommonOpts->profile.pszProfileName);

    return SUCCEEDED(hrc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

static RTEXITCODE terminateCloudInstance(HandlerArg *a, int iFirst, PCLOUDCOMMONOPT pCommonOpts)
{
    HRESULT hrc = S_OK;

    hrc = checkAndSetCommonOptions(a, pCommonOpts);
    if (FAILED(hrc))
        return RTEXITCODE_FAILURE;

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--id", 'i', RTGETOPT_REQ_STRING }
    };
    RTGETOPTSTATE GetState;
    RTGETOPTUNION ValueUnion;
    int vrc = RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), iFirst, 0);
    AssertRCReturn(vrc, RTEXITCODE_FAILURE);

    Utf8Str strInstanceId;
    int c;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 'i':
                    strInstanceId = ValueUnion.psz;
                break;
            case VINF_GETOPT_NOT_OPTION:
                return errorUnknownSubcommand(ValueUnion.psz);
            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    Bstr bstrProfileName;
    ComPtr<ICloudProfile> pCloudProfile = pCommonOpts->profile.pCloudProfile;
    pCloudProfile->COMGETTER(Name)(bstrProfileName.asOutParam());

    ComObjPtr<ICloudClient> oCloudClient;
    CHECK_ERROR2_RET(hrc, pCloudProfile,
                     CreateCloudClient(oCloudClient.asOutParam()),
                     RTEXITCODE_FAILURE);
    RTPrintf("Terminating cloud instance with id %s...\n", strInstanceId.c_str());

    ComPtr<IProgress> progress;
    CHECK_ERROR2_RET(hrc, oCloudClient,
                     TerminateInstance(Bstr(strInstanceId.c_str()).raw(), progress.asOutParam()),
                     RTEXITCODE_FAILURE);
    hrc = showProgress(progress);
    CHECK_PROGRESS_ERROR_RET(progress, ("Termination the cloud instance failed"), RTEXITCODE_FAILURE);

    if (SUCCEEDED(hrc))
        RTPrintf("Cloud instance with id %s (provider = '%s', profile = '%s') was terminated\n",
                 strInstanceId.c_str(),
                 pCommonOpts->provider.pszProviderName,
                 pCommonOpts->profile.pszProfileName);

    return SUCCEEDED(hrc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

static RTEXITCODE handleCloudInstance(HandlerArg *a, int iFirst, PCLOUDCOMMONOPT pCommonOpts)
{
    if (a->argc < 1)
        return errorNoSubcommand();

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "create",         1000, RTGETOPT_REQ_NOTHING },
        { "start",          1001, RTGETOPT_REQ_NOTHING },
        { "pause",          1002, RTGETOPT_REQ_NOTHING },
        { "info",           1003, RTGETOPT_REQ_NOTHING },
        { "update",         1004, RTGETOPT_REQ_NOTHING },
        { "terminate",      1005, RTGETOPT_REQ_NOTHING }
    };

    RTGETOPTSTATE GetState;
    int vrc = RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), iFirst, 0);
    AssertRCReturn(vrc, RTEXITCODE_FAILURE);

    int c;
    RTGETOPTUNION ValueUnion;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            /* Sub-commands: */
            case 1000:
//              setCurrentSubcommand(HELP_SCOPE_CLOUDINSTANCE_CREATE);
                return createCloudInstance(a, GetState.iNext, pCommonOpts);
            case 1001:
                setCurrentSubcommand(HELP_SCOPE_CLOUDINSTANCE_START);
                return startCloudInstance(a, GetState.iNext, pCommonOpts);
            case 1002:
                setCurrentSubcommand(HELP_SCOPE_CLOUDINSTANCE_PAUSE);
                return pauseCloudInstance(a, GetState.iNext, pCommonOpts);
            case 1003:
                setCurrentSubcommand(HELP_SCOPE_CLOUDINSTANCE_INFO);
                return showCloudInstanceInfo(a, GetState.iNext, pCommonOpts);
            case 1004:
//              setCurrentSubcommand(HELP_SCOPE_CLOUDINSTANCE_UPDATE);
                return updateCloudInstance(a, GetState.iNext, pCommonOpts);
            case 1005:
                setCurrentSubcommand(HELP_SCOPE_CLOUDINSTANCE_TERMINATE);
                return terminateCloudInstance(a, GetState.iNext, pCommonOpts);
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

    CLOUDCOMMONOPT   commonOpts = { {NULL, NULL}, {NULL, NULL} };
    int c;
    RTGETOPTUNION ValueUnion;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 'v':   // --provider
                commonOpts.provider.pszProviderName = ValueUnion.psz;
                break;
            case 'f':   // --profile
                commonOpts.profile.pszProfileName = ValueUnion.psz;
                break;
            /* Sub-commands: */
            case 1000:
                return handleCloudLists(a, GetState.iNext, &commonOpts);
            case 1002:
                return handleCloudInstance(a, GetState.iNext, &commonOpts);
            case VINF_GETOPT_NOT_OPTION:
                return errorUnknownSubcommand(ValueUnion.psz);

            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    return errorNoSubcommand();
}
