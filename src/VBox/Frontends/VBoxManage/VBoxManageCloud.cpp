/* $Id$ */
/** @file
 * VBoxManageCloud - The cloud related commands.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
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
#include <iprt/thread.h>
#include <iprt/uuid.h>
#include <iprt/file.h>
#include <VBox/log.h>

#include <iprt/cpp/path.h>

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
        { "--state",          's', RTGETOPT_REQ_STRING },
        { "help",             1001, RTGETOPT_REQ_NOTHING },
        { "--help",           1002, RTGETOPT_REQ_NOTHING }
    };
    RTGETOPTSTATE GetState;
    RTGETOPTUNION ValueUnion;
    int vrc = RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), iFirst, 0);
    AssertRCReturn(vrc, RTEXITCODE_FAILURE);
    if (a->argc == iFirst)
    {
        RTPrintf("Empty command parameter list, show help.\n");
        printHelp(g_pStdOut);
        return RTEXITCODE_SUCCESS;
    }

    Utf8Str strCompartmentId;
    com::SafeArray<CloudMachineState_T> machineStates;

    int c;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 'c':
                strCompartmentId = ValueUnion.psz;
                break;

            case 's':
            {
                const char * const pszState = ValueUnion.psz;

                if (RTStrICmp(pszState, "creatingimage") == 0)
                    machineStates.push_back(CloudMachineState_CreatingImage);
                else if (RTStrICmp(pszState, "paused") == 0) /* XXX */
                    machineStates.push_back(CloudMachineState_Stopped);
                else if (RTStrICmp(pszState, "provisioning") == 0)
                    machineStates.push_back(CloudMachineState_Provisioning);
                else if (RTStrICmp(pszState, "running") == 0)
                    machineStates.push_back(CloudMachineState_Running);
                else if (RTStrICmp(pszState, "starting") == 0)
                    machineStates.push_back(CloudMachineState_Starting);
                else if (RTStrICmp(pszState, "stopped") == 0)
                    machineStates.push_back(CloudMachineState_Stopped);
                else if (RTStrICmp(pszState, "stopping") == 0)
                    machineStates.push_back(CloudMachineState_Stopping);
                else if (RTStrICmp(pszState, "terminated") == 0)
                    machineStates.push_back(CloudMachineState_Terminated);
                else if (RTStrICmp(pszState, "terminating") == 0)
                    machineStates.push_back(CloudMachineState_Terminating);
                else
                    return errorArgument("Unknown cloud instance state \"%s\"", pszState);
                break;
            }
            case 1001:
            case 1002:
                printHelp(g_pStdOut);
                return RTEXITCODE_SUCCESS;
            case VINF_GETOPT_NOT_OPTION:
                return errorUnknownSubcommand(ValueUnion.psz);

            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    HRESULT hrc = S_OK;

    /* Delayed check. It allows us to print help information.*/
    hrc = checkAndSetCommonOptions(a, pCommonOpts);
    if (FAILED(hrc))
        return RTEXITCODE_FAILURE;

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
                     ListInstances(ComSafeArrayAsInParam(machineStates),
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
        { "--state",          's', RTGETOPT_REQ_STRING },
        { "help",             1001, RTGETOPT_REQ_NOTHING },
        { "--help",           1002, RTGETOPT_REQ_NOTHING }
    };
    RTGETOPTSTATE GetState;
    RTGETOPTUNION ValueUnion;
    int vrc = RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), iFirst, 0);
    AssertRCReturn(vrc, RTEXITCODE_FAILURE);
    if (a->argc == iFirst)
    {
        RTPrintf("Empty command parameter list, show help.\n");
        printHelp(g_pStdOut);
        return RTEXITCODE_SUCCESS;
    }

    Utf8Str strCompartmentId;
    com::SafeArray<CloudImageState_T> imageStates;

    int c;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 'c':
                strCompartmentId = ValueUnion.psz;
                break;

            case 's':
            {
                const char * const pszState = ValueUnion.psz;

                if (RTStrICmp(pszState, "available") == 0)
                    imageStates.push_back(CloudImageState_Available);
                else if (RTStrICmp(pszState, "deleted") == 0)
                    imageStates.push_back(CloudImageState_Deleted);
                else if (RTStrICmp(pszState, "disabled") == 0)
                    imageStates.push_back(CloudImageState_Disabled);
                else if (RTStrICmp(pszState, "exporting") == 0)
                    imageStates.push_back(CloudImageState_Exporting);
                else if (RTStrICmp(pszState, "importing") == 0)
                    imageStates.push_back(CloudImageState_Importing);
                else if (RTStrICmp(pszState, "provisioning") == 0)
                    imageStates.push_back(CloudImageState_Provisioning);
                else
                    return errorArgument("Unknown cloud image state \"%s\"", pszState);
                break;
            }
            case 1001:
            case 1002:
                printHelp(g_pStdOut);
                return RTEXITCODE_SUCCESS;
            case VINF_GETOPT_NOT_OPTION:
                return errorUnknownSubcommand(ValueUnion.psz);

            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }


    HRESULT hrc = S_OK;

    /* Delayed check. It allows us to print help information.*/
    hrc = checkAndSetCommonOptions(a, pCommonOpts);
    if (FAILED(hrc))
        return RTEXITCODE_FAILURE;

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
    setCurrentCommand(HELP_CMD_CLOUDLIST);
    setCurrentSubcommand(HELP_SCOPE_CLOUDLIST);
    if (a->argc == iFirst)
    {
        RTPrintf("Empty command parameter list, show help.\n");
        printHelp(g_pStdOut);
        return RTEXITCODE_SUCCESS;
    }

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "images",              1000, RTGETOPT_REQ_NOTHING },
        { "instances",           1001, RTGETOPT_REQ_NOTHING },
        { "networks",            1002, RTGETOPT_REQ_NOTHING },
        { "subnets",             1003, RTGETOPT_REQ_NOTHING },
        { "vcns",                1004, RTGETOPT_REQ_NOTHING },
        { "objects",             1005, RTGETOPT_REQ_NOTHING },
        { "help",                1006, RTGETOPT_REQ_NOTHING },
        { "--help",              1007, RTGETOPT_REQ_NOTHING }
    };

//  Bstr bstrProvider(pCommonOpts->provider.pszProviderName);
//  Bstr bstrProfile(pCommonOpts->profile.pszProfileName);
//
//  /* check for required options */
//  if (bstrProvider.isEmpty())
//      return errorSyntax(USAGE_S_NEWCMD, "Parameter --provider is required");
//  if (bstrProfile.isEmpty())
//      return errorSyntax(USAGE_S_NEWCMD, "Parameter --profile is required");

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
                setCurrentSubcommand(HELP_SCOPE_CLOUDLIST_IMAGES);
                return listCloudImages(a, GetState.iNext, pCommonOpts);
            case 1001:
                setCurrentSubcommand(HELP_SCOPE_CLOUDLIST_INSTANCES);
                return listCloudInstances(a, GetState.iNext, pCommonOpts);
            case 1006:
            case 1007:
                printHelp(g_pStdOut);
                return RTEXITCODE_SUCCESS;
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
    HRESULT hrc = S_OK;

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--image-id",       'i', RTGETOPT_REQ_STRING },
        { "--boot-volume-id", 'v', RTGETOPT_REQ_STRING },
        { "--display-name",   'n', RTGETOPT_REQ_STRING },
        { "--launch-mode",    'm', RTGETOPT_REQ_STRING },
        { "--shape",          's', RTGETOPT_REQ_STRING },
        { "--domain-name",    'd', RTGETOPT_REQ_STRING },
        { "--boot-disk-size", 'b', RTGETOPT_REQ_STRING },
        { "--publicip",       'p', RTGETOPT_REQ_STRING },
        { "--subnet",         't', RTGETOPT_REQ_STRING },
        { "--privateip",      'P', RTGETOPT_REQ_STRING },
        { "--launch",         'l', RTGETOPT_REQ_STRING },
        { "--public-ssh-key", 'k', RTGETOPT_REQ_STRING },
        { "help",             1001, RTGETOPT_REQ_NOTHING },
        { "--help",           1002, RTGETOPT_REQ_NOTHING }
    };
    RTGETOPTSTATE GetState;
    RTGETOPTUNION ValueUnion;
    int vrc = RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), iFirst, 0);
    AssertRCReturn(vrc, RTEXITCODE_FAILURE);
    if (a->argc == iFirst)
    {
        RTPrintf("Empty command parameter list, show help.\n");
        printHelp(g_pStdOut);
        return RTEXITCODE_SUCCESS;
    }

    ComPtr<IAppliance> pAppliance;
    CHECK_ERROR2_RET(hrc, a->virtualBox, CreateAppliance(pAppliance.asOutParam()), RTEXITCODE_FAILURE);
    ULONG vsdNum = 1;
    CHECK_ERROR2_RET(hrc, pAppliance, CreateVirtualSystemDescriptions(1, &vsdNum), RTEXITCODE_FAILURE);
    com::SafeIfaceArray<IVirtualSystemDescription> virtualSystemDescriptions;
    CHECK_ERROR2_RET(hrc, pAppliance,
                     COMGETTER(VirtualSystemDescriptions)(ComSafeArrayAsOutParam(virtualSystemDescriptions)),
                     RTEXITCODE_FAILURE);
    ComPtr<IVirtualSystemDescription> pVSD = virtualSystemDescriptions[0];

    Utf8Str strDisplayName, strImageId, strBootVolumeId, strPublicSSHKey;
    int c;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 'i':
                strImageId = ValueUnion.psz;
                pVSD->AddDescription(VirtualSystemDescriptionType_CloudImageId,
                                     Bstr(ValueUnion.psz).raw(), NULL);
                break;

            case 'v':
                strBootVolumeId = ValueUnion.psz;
                pVSD->AddDescription(VirtualSystemDescriptionType_CloudBootVolumeId,
                                     Bstr(ValueUnion.psz).raw(), NULL);
                break;
            case 'n':
                strDisplayName = ValueUnion.psz;
                pVSD->AddDescription(VirtualSystemDescriptionType_Name,
                                     Bstr(ValueUnion.psz).raw(), NULL);
                break;
            case 'm':
                pVSD->AddDescription(VirtualSystemDescriptionType_CloudOCILaunchMode,
                                     Bstr(ValueUnion.psz).raw(), NULL);
                break;
            case 's':
                pVSD->AddDescription(VirtualSystemDescriptionType_CloudInstanceShape,
                                     Bstr(ValueUnion.psz).raw(), NULL);
                break;
            case 'd':
                pVSD->AddDescription(VirtualSystemDescriptionType_CloudDomain,
                                     Bstr(ValueUnion.psz).raw(), NULL);
                break;
            case 'b':
                pVSD->AddDescription(VirtualSystemDescriptionType_CloudBootDiskSize,
                                     Bstr(ValueUnion.psz).raw(), NULL);
                break;
            case 'p':
                pVSD->AddDescription(VirtualSystemDescriptionType_CloudPublicIP,
                                     Bstr(ValueUnion.psz).raw(), NULL);
                break;
            case 'P':
                pVSD->AddDescription(VirtualSystemDescriptionType_CloudPrivateIP,
                                     Bstr(ValueUnion.psz).raw(), NULL);
                break;
            case 't':
                pVSD->AddDescription(VirtualSystemDescriptionType_CloudOCISubnet,
                                     Bstr(ValueUnion.psz).raw(), NULL);
                break;
            case 'l':
                {
                    Utf8Str strLaunch(ValueUnion.psz);
                    if (strLaunch.isNotEmpty() && (strLaunch.equalsIgnoreCase("true") || strLaunch.equalsIgnoreCase("false")))
                        pVSD->AddDescription(VirtualSystemDescriptionType_CloudLaunchInstance,
                                             Bstr(ValueUnion.psz).raw(), NULL);
                    break;
                }
            case 'k':
                strPublicSSHKey = ValueUnion.psz;
                pVSD->AddDescription(VirtualSystemDescriptionType_CloudPublicSSHKey,
                                     Bstr(ValueUnion.psz).raw(), NULL);
                break;
            case 1001:
            case 1002:
                printHelp(g_pStdOut);
                return RTEXITCODE_SUCCESS;
            case VINF_GETOPT_NOT_OPTION:
                return errorUnknownSubcommand(ValueUnion.psz);
            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    /* Delayed check. It allows us to print help information.*/
    hrc = checkAndSetCommonOptions(a, pCommonOpts);
    if (FAILED(hrc))
        return RTEXITCODE_FAILURE;

    if (strPublicSSHKey.isEmpty())
        RTPrintf("Warning!!! Public SSH key doesn't present in the passed arguments...\n");

    if (strImageId.isNotEmpty() && strBootVolumeId.isNotEmpty())
        return errorArgument("Parameters --image-id and --boot-volume-id are mutually exclusive. "
                             "Only one of them must be presented.");

    if (strImageId.isEmpty() && strBootVolumeId.isEmpty())
        return errorArgument("Missing parameter --image-id or --boot-volume-id. One of them must be presented.");

    ComPtr<ICloudProfile> pCloudProfile = pCommonOpts->profile.pCloudProfile;

    pVSD->AddDescription(VirtualSystemDescriptionType_CloudProfileName,
                         Bstr(pCommonOpts->profile.pszProfileName).raw(),
                         NULL);

    ComObjPtr<ICloudClient> oCloudClient;
    CHECK_ERROR2_RET(hrc, pCloudProfile,
                     CreateCloudClient(oCloudClient.asOutParam()),
                     RTEXITCODE_FAILURE);

    ComPtr<IStringArray> infoArray;
    com::SafeArray<BSTR> pStrInfoArray;
    ComPtr<IProgress> pProgress;

#if 0
        /*
         * OCI API returns an error during an instance creation if the image isn't available
         * or in the inappropriate state. So the check can be omitted.
         */
        RTPrintf("Checking the cloud image with id \'%s\'...\n", strImageId.c_str());
        CHECK_ERROR2_RET(hrc, oCloudClient,
                         GetImageInfo(Bstr(strImageId).raw(),
                                      infoArray.asOutParam(),
                                      pProgress.asOutParam()),
                         RTEXITCODE_FAILURE);

        hrc = showProgress(pProgress);
        CHECK_PROGRESS_ERROR_RET(pProgress, ("Checking the cloud image failed"), RTEXITCODE_FAILURE);

        pProgress.setNull();
#endif

    if (strImageId.isNotEmpty())
        RTPrintf("Creating cloud instance with name \'%s\' from the image \'%s\'...\n",
                 strDisplayName.c_str(), strImageId.c_str());
    else
        RTPrintf("Creating cloud instance with name \'%s\' from the boot volume \'%s\'...\n",
                 strDisplayName.c_str(), strBootVolumeId.c_str());

    CHECK_ERROR2_RET(hrc, oCloudClient, LaunchVM(pVSD, pProgress.asOutParam()), RTEXITCODE_FAILURE);

    hrc = showProgress(pProgress);
    CHECK_PROGRESS_ERROR_RET(pProgress, ("Creating cloud instance failed"), RTEXITCODE_FAILURE);

    if (SUCCEEDED(hrc))
        RTPrintf("Cloud instance was created successfully\n");

    return SUCCEEDED(hrc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
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

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--id",   'i', RTGETOPT_REQ_STRING },
        { "help",   1001, RTGETOPT_REQ_NOTHING },
        { "--help", 1002, RTGETOPT_REQ_NOTHING }
    };
    RTGETOPTSTATE GetState;
    RTGETOPTUNION ValueUnion;
    int vrc = RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), iFirst, 0);
    AssertRCReturn(vrc, RTEXITCODE_FAILURE);
    if (a->argc == iFirst)
    {
        RTPrintf("Empty command parameter list, show help.\n");
        printHelp(g_pStdOut);
        return RTEXITCODE_SUCCESS;
    }

    Utf8Str strInstanceId;

    int c;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 'i':
            {
                if (strInstanceId.isNotEmpty())
                    return errorArgument("Duplicate parameter: --id");

                strInstanceId = ValueUnion.psz;
                if (strInstanceId.isEmpty())
                    return errorArgument("Empty parameter: --id");

                break;
            }
            case 1001:
            case 1002:
                printHelp(g_pStdOut);
                return RTEXITCODE_SUCCESS;
            case VINF_GETOPT_NOT_OPTION:
                return errorUnknownSubcommand(ValueUnion.psz);

            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    /* Delayed check. It allows us to print help information.*/
    hrc = checkAndSetCommonOptions(a, pCommonOpts);
    if (FAILED(hrc))
        return RTEXITCODE_FAILURE;

    if (strInstanceId.isEmpty())
        return errorArgument("Missing parameter: --id");

    ComPtr<ICloudProfile> pCloudProfile = pCommonOpts->profile.pCloudProfile;

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
                     GetInstanceInfo(Bstr(strInstanceId).raw(), instanceDescription, progress.asOutParam()),
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

    const size_t vsdHReadableArraySize = 12;//the number of items in the vsdHReadableArray
    vsdHReadable vsdHReadableArray[vsdHReadableArraySize] = {
        {VirtualSystemDescriptionType_CloudDomain, "Availability domain = %ls\n", "Availability domain wasn't found\n"},
        {VirtualSystemDescriptionType_Name, "Instance displayed name = %ls\n", "Instance displayed name wasn't found\n"},
        {VirtualSystemDescriptionType_CloudInstanceState, "Instance state = %ls\n", "Instance state wasn't found\n"},
        {VirtualSystemDescriptionType_CloudInstanceId, "Instance Id = %ls\n", "Instance Id wasn't found\n"},
        {VirtualSystemDescriptionType_CloudInstanceDisplayName, "Instance name = %ls\n", "Instance name wasn't found\n"},
        {VirtualSystemDescriptionType_CloudImageId, "Bootable image Id = %ls\n",
            "Image Id whom the instance is booted up wasn't found\n"},
        {VirtualSystemDescriptionType_CloudInstanceShape, "Shape of the instance = %ls\n",
            "The shape of the instance wasn't found\n"},
        {VirtualSystemDescriptionType_OS, "Type of guest OS = %ls\n", "Type of guest OS wasn't found\n"},
        {VirtualSystemDescriptionType_Memory, "RAM = %ls MB\n", "Value for RAM wasn't found\n"},
        {VirtualSystemDescriptionType_CPU, "CPUs = %ls\n", "Numbers of CPUs weren't found\n"},
        {VirtualSystemDescriptionType_CloudPublicIP, "Instance public IP = %ls\n", "Public IP wasn't found\n"},
        {VirtualSystemDescriptionType_Miscellaneous, "%ls\n", "Free-form tags or metadata weren't found\n"}
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
        {
            LogRel(("Size is %d", aVBoxValues.size()));
            for (size_t j = 0; j<aVBoxValues.size(); ++j)
            {
                RTPrintf(vsdHReadableArray[i].strFound.c_str(), aVBoxValues[j]);
            }
        }

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

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--id",   'i', RTGETOPT_REQ_STRING },
        { "help",   1001, RTGETOPT_REQ_NOTHING },
        { "--help", 1002, RTGETOPT_REQ_NOTHING }
    };
    RTGETOPTSTATE GetState;
    RTGETOPTUNION ValueUnion;
    int vrc = RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), iFirst, 0);
    AssertRCReturn(vrc, RTEXITCODE_FAILURE);
    if (a->argc == iFirst)
    {
        RTPrintf("Empty command parameter list, show help.\n");
        printHelp(g_pStdOut);
        return RTEXITCODE_SUCCESS;
    }

    Utf8Str strInstanceId;

    int c;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 'i':
            {
                if (strInstanceId.isNotEmpty())
                    return errorArgument("Duplicate parameter: --id");

                strInstanceId = ValueUnion.psz;
                if (strInstanceId.isEmpty())
                    return errorArgument("Empty parameter: --id");

                break;
            }
            case 1001:
            case 1002:
                printHelp(g_pStdOut);
                return RTEXITCODE_SUCCESS;
            case VINF_GETOPT_NOT_OPTION:
                return errorUnknownSubcommand(ValueUnion.psz);

            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    /* Delayed check. It allows us to print help information.*/
    hrc = checkAndSetCommonOptions(a, pCommonOpts);
    if (FAILED(hrc))
        return RTEXITCODE_FAILURE;

    if (strInstanceId.isEmpty())
        return errorArgument("Missing parameter: --id");

    ComPtr<ICloudProfile> pCloudProfile = pCommonOpts->profile.pCloudProfile;

    ComObjPtr<ICloudClient> oCloudClient;
    CHECK_ERROR2_RET(hrc, pCloudProfile,
                     CreateCloudClient(oCloudClient.asOutParam()),
                     RTEXITCODE_FAILURE);
    RTPrintf("Starting cloud instance with id %s...\n", strInstanceId.c_str());

    ComPtr<IProgress> progress;
    CHECK_ERROR2_RET(hrc, oCloudClient,
                     StartInstance(Bstr(strInstanceId).raw(), progress.asOutParam()),
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

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--id",   'i', RTGETOPT_REQ_STRING },
        { "help",   1001, RTGETOPT_REQ_NOTHING },
        { "--help", 1002, RTGETOPT_REQ_NOTHING }
    };
    RTGETOPTSTATE GetState;
    RTGETOPTUNION ValueUnion;
    int vrc = RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), iFirst, 0);
    AssertRCReturn(vrc, RTEXITCODE_FAILURE);
    if (a->argc == iFirst)
    {
        RTPrintf("Empty command parameter list, show help.\n");
        printHelp(g_pStdOut);
        return RTEXITCODE_SUCCESS;
    }

    Utf8Str strInstanceId;

    int c;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 'i':
            {
                if (strInstanceId.isNotEmpty())
                    return errorArgument("Duplicate parameter: --id");

                strInstanceId = ValueUnion.psz;
                if (strInstanceId.isEmpty())
                    return errorArgument("Empty parameter: --id");

                break;
            }
            case 1001:
            case 1002:
                printHelp(g_pStdOut);
                return RTEXITCODE_SUCCESS;
            case VINF_GETOPT_NOT_OPTION:
                return errorUnknownSubcommand(ValueUnion.psz);

            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    /* Delayed check. It allows us to print help information.*/
    hrc = checkAndSetCommonOptions(a, pCommonOpts);
    if (FAILED(hrc))
        return RTEXITCODE_FAILURE;

    if (strInstanceId.isEmpty())
        return errorArgument("Missing parameter: --id");

    ComPtr<ICloudProfile> pCloudProfile = pCommonOpts->profile.pCloudProfile;

    ComObjPtr<ICloudClient> oCloudClient;
    CHECK_ERROR2_RET(hrc, pCloudProfile,
                     CreateCloudClient(oCloudClient.asOutParam()),
                     RTEXITCODE_FAILURE);
    RTPrintf("Pausing cloud instance with id %s...\n", strInstanceId.c_str());

    ComPtr<IProgress> progress;
    CHECK_ERROR2_RET(hrc, oCloudClient,
                     PauseInstance(Bstr(strInstanceId).raw(), progress.asOutParam()),
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

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--id",   'i', RTGETOPT_REQ_STRING },
        { "help",   1001, RTGETOPT_REQ_NOTHING },
        { "--help", 1002, RTGETOPT_REQ_NOTHING }
    };
    RTGETOPTSTATE GetState;
    RTGETOPTUNION ValueUnion;
    int vrc = RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), iFirst, 0);
    AssertRCReturn(vrc, RTEXITCODE_FAILURE);
    if (a->argc == iFirst)
    {
        RTPrintf("Empty command parameter list, show help.\n");
        printHelp(g_pStdOut);
        return RTEXITCODE_SUCCESS;
    }

    Utf8Str strInstanceId;

    int c;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 'i':
            {
                if (strInstanceId.isNotEmpty())
                    return errorArgument("Duplicate parameter: --id");

                strInstanceId = ValueUnion.psz;
                if (strInstanceId.isEmpty())
                    return errorArgument("Empty parameter: --id");

                break;
            }
            case 1001:
            case 1002:
                printHelp(g_pStdOut);
                return RTEXITCODE_SUCCESS;
            case VINF_GETOPT_NOT_OPTION:
                return errorUnknownSubcommand(ValueUnion.psz);

            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    /* Delayed check. It allows us to print help information.*/
    hrc = checkAndSetCommonOptions(a, pCommonOpts);
    if (FAILED(hrc))
        return RTEXITCODE_FAILURE;

    if (strInstanceId.isEmpty())
        return errorArgument("Missing parameter: --id");


    ComPtr<ICloudProfile> pCloudProfile = pCommonOpts->profile.pCloudProfile;

    ComObjPtr<ICloudClient> oCloudClient;
    CHECK_ERROR2_RET(hrc, pCloudProfile,
                     CreateCloudClient(oCloudClient.asOutParam()),
                     RTEXITCODE_FAILURE);
    RTPrintf("Terminating cloud instance with id %s...\n", strInstanceId.c_str());

    ComPtr<IProgress> progress;
    CHECK_ERROR2_RET(hrc, oCloudClient,
                     TerminateInstance(Bstr(strInstanceId).raw(), progress.asOutParam()),
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
    setCurrentCommand(HELP_CMD_CLOUDINSTANCE);
    setCurrentSubcommand(HELP_SCOPE_CLOUDINSTANCE);
    if (a->argc == iFirst)
    {
        RTPrintf("Empty command parameter list, show help.\n");
        printHelp(g_pStdOut);
        return RTEXITCODE_SUCCESS;
    }

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "create",         1000, RTGETOPT_REQ_NOTHING },
        { "start",          1001, RTGETOPT_REQ_NOTHING },
        { "pause",          1002, RTGETOPT_REQ_NOTHING },
        { "info",           1003, RTGETOPT_REQ_NOTHING },
        { "update",         1004, RTGETOPT_REQ_NOTHING },
        { "terminate",      1005, RTGETOPT_REQ_NOTHING },
        { "help",           1006, RTGETOPT_REQ_NOTHING },
        { "--help",         1007, RTGETOPT_REQ_NOTHING }
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
                setCurrentSubcommand(HELP_SCOPE_CLOUDINSTANCE_CREATE);
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
            case 1006:
            case 1007:
                printHelp(g_pStdOut);
                return RTEXITCODE_SUCCESS;
            case VINF_GETOPT_NOT_OPTION:
                return errorUnknownSubcommand(ValueUnion.psz);

            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    return errorNoSubcommand();
}


static RTEXITCODE createCloudImage(HandlerArg *a, int iFirst, PCLOUDCOMMONOPT pCommonOpts)
{
    HRESULT hrc = S_OK;

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--object-name",    'o', RTGETOPT_REQ_STRING },
        { "--bucket-name",    'b', RTGETOPT_REQ_STRING },
        { "--compartment-id", 'c', RTGETOPT_REQ_STRING },
        { "--instance-id",    'i', RTGETOPT_REQ_STRING },
        { "--display-name",   'd', RTGETOPT_REQ_STRING },
        { "--launch-mode",    'm', RTGETOPT_REQ_STRING },
        { "help",             1001, RTGETOPT_REQ_NOTHING },
        { "--help",           1002, RTGETOPT_REQ_NOTHING }
    };
    RTGETOPTSTATE GetState;
    RTGETOPTUNION ValueUnion;
    int vrc = RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), iFirst, 0);
    AssertRCReturn(vrc, RTEXITCODE_FAILURE);
    if (a->argc == iFirst)
    {
        RTPrintf("Empty command parameter list, show help.\n");
        printHelp(g_pStdOut);
        return RTEXITCODE_SUCCESS;
    }

    Utf8Str strCompartmentId;
    Utf8Str strInstanceId;
    Utf8Str strDisplayName;
    Utf8Str strBucketName;
    Utf8Str strObjectName;
    com::SafeArray<BSTR>  parameters;

    int c;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 'c':
                strCompartmentId=ValueUnion.psz;
                Bstr(Utf8Str("compartment-id=").append(ValueUnion.psz)).detachTo(parameters.appendedRaw());
                break;
            case 'i':
                strInstanceId=ValueUnion.psz;
                Bstr(Utf8Str("instance-id=").append(ValueUnion.psz)).detachTo(parameters.appendedRaw());
                break;
            case 'd':
                strDisplayName=ValueUnion.psz;
                Bstr(Utf8Str("display-name=").append(ValueUnion.psz)).detachTo(parameters.appendedRaw());
                break;
            case 'o':
                strObjectName=ValueUnion.psz;
                Bstr(Utf8Str("object-name=").append(ValueUnion.psz)).detachTo(parameters.appendedRaw());
                break;
            case 'b':
                strBucketName=ValueUnion.psz;
                Bstr(Utf8Str("bucket-name=").append(ValueUnion.psz)).detachTo(parameters.appendedRaw());
                break;
            case 'm':
                strBucketName=ValueUnion.psz;
                Bstr(Utf8Str("launch-mode=").append(ValueUnion.psz)).detachTo(parameters.appendedRaw());
                break;
            case 1001:
            case 1002:
                printHelp(g_pStdOut);
                return RTEXITCODE_SUCCESS;
            case VINF_GETOPT_NOT_OPTION:
                return errorUnknownSubcommand(ValueUnion.psz);
            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    /* Delayed check. It allows us to print help information.*/
    hrc = checkAndSetCommonOptions(a, pCommonOpts);
    if (FAILED(hrc))
        return RTEXITCODE_FAILURE;

    if (strInstanceId.isNotEmpty() && strObjectName.isNotEmpty())
        return errorArgument("Conflicting parameters: --instance-id and --object-name can't be used together. Choose one.");

    ComPtr<ICloudProfile> pCloudProfile = pCommonOpts->profile.pCloudProfile;

    ComObjPtr<ICloudClient> oCloudClient;
    CHECK_ERROR2_RET(hrc, pCloudProfile,
                     CreateCloudClient(oCloudClient.asOutParam()),
                     RTEXITCODE_FAILURE);
    if (strInstanceId.isNotEmpty())
        RTPrintf("Creating cloud image with name \'%s\' from the instance \'%s\'...\n",
                 strDisplayName.c_str(), strInstanceId.c_str());
    else
        RTPrintf("Creating cloud image with name \'%s\' from the object \'%s\' in the bucket \'%s\'...\n",
                 strDisplayName.c_str(), strObjectName.c_str(), strBucketName.c_str());

    ComPtr<IProgress> progress;
    CHECK_ERROR2_RET(hrc, oCloudClient,
                     CreateImage(ComSafeArrayAsInParam(parameters), progress.asOutParam()),
                     RTEXITCODE_FAILURE);
    hrc = showProgress(progress);
    CHECK_PROGRESS_ERROR_RET(progress, ("Creating cloud image failed"), RTEXITCODE_FAILURE);

    if (SUCCEEDED(hrc))
        RTPrintf("Cloud image was created successfully\n");

    return SUCCEEDED(hrc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}


static RTEXITCODE exportCloudImage(HandlerArg *a, int iFirst, PCLOUDCOMMONOPT pCommonOpts)
{
    HRESULT hrc = S_OK;

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--id",             'i', RTGETOPT_REQ_STRING },
        { "--bucket-name",    'b', RTGETOPT_REQ_STRING },
        { "--object-name",    'o', RTGETOPT_REQ_STRING },
        { "--display-name",   'd', RTGETOPT_REQ_STRING },
        { "--launch-mode",    'm', RTGETOPT_REQ_STRING },
        { "help",             1001, RTGETOPT_REQ_NOTHING },
        { "--help",           1002, RTGETOPT_REQ_NOTHING }
    };
    RTGETOPTSTATE GetState;
    RTGETOPTUNION ValueUnion;
    int vrc = RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), iFirst, 0);
    AssertRCReturn(vrc, RTEXITCODE_FAILURE);
    if (a->argc == iFirst)
    {
        RTPrintf("Empty command parameter list, show help.\n");
        printHelp(g_pStdOut);
        return RTEXITCODE_SUCCESS;
    }

    Utf8Str strImageId;   /* XXX: this is vbox "image", i.e. medium */
    Utf8Str strBucketName;
    Utf8Str strObjectName;
    Utf8Str strDisplayName;
    Utf8Str strLaunchMode;
    com::SafeArray<BSTR> parameters;

    int c;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 'b': /* --bucket-name */
            {
                if (strBucketName.isNotEmpty())
                    return errorArgument("Duplicate parameter: --bucket-name");

                strBucketName = ValueUnion.psz;
                if (strBucketName.isEmpty())
                    return errorArgument("Empty parameter: --bucket-name");

                break;
            }

            case 'o': /* --object-name */
            {
                if (strObjectName.isNotEmpty())
                    return errorArgument("Duplicate parameter: --object-name");

                strObjectName = ValueUnion.psz;
                if (strObjectName.isEmpty())
                    return errorArgument("Empty parameter: --object-name");

                break;
            }

            case 'i': /* --id */
            {
                if (strImageId.isNotEmpty())
                    return errorArgument("Duplicate parameter: --id");

                strImageId = ValueUnion.psz;
                if (strImageId.isEmpty())
                    return errorArgument("Empty parameter: --id");

                break;
            }

            case 'd': /* --display-name */
            {
                if (strDisplayName.isNotEmpty())
                    return errorArgument("Duplicate parameter: --display-name");

                strDisplayName = ValueUnion.psz;
                if (strDisplayName.isEmpty())
                    return errorArgument("Empty parameter: --display-name");

                break;
            }

            case 'm': /* --launch-mode */
            {
                if (strLaunchMode.isNotEmpty())
                    return errorArgument("Duplicate parameter: --launch-mode");

                strLaunchMode = ValueUnion.psz;
                if (strLaunchMode.isEmpty())
                    return errorArgument("Empty parameter: --launch-mode");

                break;
            }

            case 1001:
            case 1002:
                printHelp(g_pStdOut);
                return RTEXITCODE_SUCCESS;

            case VINF_GETOPT_NOT_OPTION:
                return errorUnknownSubcommand(ValueUnion.psz);

            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    /* Delayed check. It allows us to print help information.*/
    hrc = checkAndSetCommonOptions(a, pCommonOpts);
    if (FAILED(hrc))
        return RTEXITCODE_FAILURE;

    if (strImageId.isNotEmpty())
        BstrFmt("image-id=%s", strImageId.c_str()).detachTo(parameters.appendedRaw());
    else
        return errorArgument("Missing parameter: --id");

    if (strBucketName.isNotEmpty())
        BstrFmt("bucket-name=%s", strBucketName.c_str()).detachTo(parameters.appendedRaw());
    else
        return errorArgument("Missing parameter: --bucket-name");

    if (strObjectName.isNotEmpty())
        BstrFmt("object-name=%s", strObjectName.c_str()).detachTo(parameters.appendedRaw());

    if (strDisplayName.isNotEmpty())
        BstrFmt("display-name=%s", strDisplayName.c_str()).detachTo(parameters.appendedRaw());

    if (strLaunchMode.isNotEmpty())
        BstrFmt("launch-mode=%s", strLaunchMode.c_str()).detachTo(parameters.appendedRaw());


    ComPtr<ICloudProfile> pCloudProfile = pCommonOpts->profile.pCloudProfile;

    ComObjPtr<ICloudClient> oCloudClient;
    CHECK_ERROR2_RET(hrc, pCloudProfile,
                     CreateCloudClient(oCloudClient.asOutParam()),
                     RTEXITCODE_FAILURE);

    if (strObjectName.isNotEmpty())
        RTPrintf("Exporting image \'%s\' to the Cloud with name \'%s\'...\n",
                 strImageId.c_str(), strObjectName.c_str());
    else
        RTPrintf("Exporting image \'%s\' to the Cloud with default name\n",
                 strImageId.c_str());

    ComPtr<IVirtualBox> pVirtualBox = a->virtualBox;
    SafeIfaceArray<IMedium> aImageList;
    CHECK_ERROR2_RET(hrc, pVirtualBox,
                     COMGETTER(HardDisks)(ComSafeArrayAsOutParam(aImageList)),
                     RTEXITCODE_FAILURE);

    ComPtr<IMedium> pImage;
    size_t cImages = aImageList.size();
    bool fFound = false;
    for (size_t i = 0; i < cImages; ++i)
    {
        pImage = aImageList[i];
        Bstr bstrImageId;
        hrc = pImage->COMGETTER(Id)(bstrImageId.asOutParam());
        if (FAILED(hrc))
            continue;

        com::Guid imageId(bstrImageId);

        if (!imageId.isValid() || imageId.isZero())
            continue;

        if (!strImageId.compare(imageId.toString()))
        {
            fFound = true;
            RTPrintf("Image %s was found\n", strImageId.c_str());
            break;
        }
    }

    if (!fFound)
    {
        RTPrintf("Process of exporting the image to the Cloud was interrupted. The image wasn't found.\n");
        return RTEXITCODE_FAILURE;
    }

    ComPtr<IProgress> progress;
    CHECK_ERROR2_RET(hrc, oCloudClient,
                     ExportImage(pImage, ComSafeArrayAsInParam(parameters), progress.asOutParam()),
                     RTEXITCODE_FAILURE);
    hrc = showProgress(progress);
    CHECK_PROGRESS_ERROR_RET(progress, ("Export the image to the Cloud failed"), RTEXITCODE_FAILURE);

    if (SUCCEEDED(hrc))
        RTPrintf("Export the image to the Cloud was successfull\n");

    return SUCCEEDED(hrc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

static RTEXITCODE importCloudImage(HandlerArg *a, int iFirst, PCLOUDCOMMONOPT pCommonOpts)
{
    HRESULT hrc = S_OK;

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--id",             'i', RTGETOPT_REQ_STRING },
        { "--bucket-name",    'b', RTGETOPT_REQ_STRING },
        { "--object-name",    'o', RTGETOPT_REQ_STRING },
        { "help",             1001, RTGETOPT_REQ_NOTHING },
        { "--help",           1002, RTGETOPT_REQ_NOTHING }
    };
    RTGETOPTSTATE GetState;
    RTGETOPTUNION ValueUnion;
    int vrc = RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), iFirst, 0);
    AssertRCReturn(vrc, RTEXITCODE_FAILURE);
    if (a->argc == iFirst)
    {
        RTPrintf("Empty command parameter list, show help.\n");
        printHelp(g_pStdOut);
        return RTEXITCODE_SUCCESS;
    }

    Utf8Str strImageId;
    Utf8Str strCompartmentId;
    Utf8Str strBucketName;
    Utf8Str strObjectName;
    Utf8Str strDisplayName;
    com::SafeArray<BSTR>  parameters;

    int c;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 'i':
                strImageId=ValueUnion.psz;
                break;
            case 'b':
                strBucketName=ValueUnion.psz;
                Bstr(Utf8Str("bucket-name=").append(ValueUnion.psz)).detachTo(parameters.appendedRaw());
                break;
            case 'o':
                strObjectName=ValueUnion.psz;
                Bstr(Utf8Str("object-name=").append(ValueUnion.psz)).detachTo(parameters.appendedRaw());
                break;
            case 1001:
            case 1002:
                printHelp(g_pStdOut);
                return RTEXITCODE_SUCCESS;
            case VINF_GETOPT_NOT_OPTION:
                return errorUnknownSubcommand(ValueUnion.psz);
            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    /* Delayed check. It allows us to print help information.*/
    hrc = checkAndSetCommonOptions(a, pCommonOpts);
    if (FAILED(hrc))
        return RTEXITCODE_FAILURE;

    ComPtr<ICloudProfile> pCloudProfile = pCommonOpts->profile.pCloudProfile;

    ComPtr<IVirtualBox> pVirtualBox = a->virtualBox;
    ComObjPtr<ICloudClient> oCloudClient;
    CHECK_ERROR2_RET(hrc, pCloudProfile,
                     CreateCloudClient(oCloudClient.asOutParam()),
                     RTEXITCODE_FAILURE);
    RTPrintf("Creating an object \'%s\' from the cloud image \'%s\'...\n", strObjectName.c_str(), strImageId.c_str());

    ComPtr<IProgress> progress;
    CHECK_ERROR2_RET(hrc, oCloudClient,
                     ImportImage(Bstr(strImageId).raw(), ComSafeArrayAsInParam(parameters), progress.asOutParam()),
                     RTEXITCODE_FAILURE);
    hrc = showProgress(progress);
    CHECK_PROGRESS_ERROR_RET(progress, ("Cloud image import failed"), RTEXITCODE_FAILURE);

    if (SUCCEEDED(hrc))
    {
        RTPrintf("Cloud image was imported successfully. Find the downloaded object with the name %s "
                 "in the system temp folder (find the possible environment variables like TEMP, TMP and etc.)\n",
                 strObjectName.c_str());
    }

    return SUCCEEDED(hrc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

static RTEXITCODE showCloudImageInfo(HandlerArg *a, int iFirst, PCLOUDCOMMONOPT pCommonOpts)
{
    HRESULT hrc = S_OK;

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--id", 'i', RTGETOPT_REQ_STRING },
        { "help",   1001, RTGETOPT_REQ_NOTHING },
        { "--help", 1002, RTGETOPT_REQ_NOTHING }
    };
    RTGETOPTSTATE GetState;
    RTGETOPTUNION ValueUnion;
    int vrc = RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), iFirst, 0);
    AssertRCReturn(vrc, RTEXITCODE_FAILURE);
    if (a->argc == iFirst)
    {
        RTPrintf("Empty command parameter list, show help.\n");
        printHelp(g_pStdOut);
        return RTEXITCODE_SUCCESS;
    }

    Utf8Str strImageId;

    int c;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 'i':
                strImageId = ValueUnion.psz;
                break;
            case 1001:
            case 1002:
                printHelp(g_pStdOut);
                return RTEXITCODE_SUCCESS;
            case VINF_GETOPT_NOT_OPTION:
                return errorUnknownSubcommand(ValueUnion.psz);
            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    /* Delayed check. It allows us to print help information.*/
    hrc = checkAndSetCommonOptions(a, pCommonOpts);
    if (FAILED(hrc))
        return RTEXITCODE_FAILURE;

    ComPtr<ICloudProfile> pCloudProfile = pCommonOpts->profile.pCloudProfile;

    ComObjPtr<ICloudClient> oCloudClient;
    CHECK_ERROR2_RET(hrc, pCloudProfile,
                     CreateCloudClient(oCloudClient.asOutParam()),
                     RTEXITCODE_FAILURE);
    RTPrintf("Getting information about the cloud image with id \'%s\'...\n", strImageId.c_str());

    ComPtr<IStringArray> infoArray;
    com::SafeArray<BSTR> pStrInfoArray;
    ComPtr<IProgress> pProgress;

    RTPrintf("Reply is in the form \'image property\' = \'value\'\n");
    CHECK_ERROR2_RET(hrc, oCloudClient,
                     GetImageInfo(Bstr(strImageId).raw(),
                                  infoArray.asOutParam(),
                                  pProgress.asOutParam()),
                     RTEXITCODE_FAILURE);

    hrc = showProgress(pProgress);
    CHECK_PROGRESS_ERROR_RET(pProgress, ("Getting information about the cloud image failed"), RTEXITCODE_FAILURE);

    CHECK_ERROR2_RET(hrc,
                     infoArray, COMGETTER(Values)(ComSafeArrayAsOutParam(pStrInfoArray)),
                     RTEXITCODE_FAILURE);

    RTPrintf("General information about the image:\n");
    size_t cParamNames = pStrInfoArray.size();
    for (size_t k = 0; k < cParamNames; k++)
    {
        Utf8Str data(pStrInfoArray[k]);
        RTPrintf("\t%s\n", data.c_str());
    }

    return SUCCEEDED(hrc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

static RTEXITCODE updateCloudImage(HandlerArg *a, int iFirst, PCLOUDCOMMONOPT pCommonOpts)
{
    RT_NOREF(a);
    RT_NOREF(iFirst);
    RT_NOREF(pCommonOpts);
    return RTEXITCODE_SUCCESS;
}

static RTEXITCODE deleteCloudImage(HandlerArg *a, int iFirst, PCLOUDCOMMONOPT pCommonOpts)
{
    HRESULT hrc = S_OK;

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--id", 'i', RTGETOPT_REQ_STRING },
        { "help",   1001, RTGETOPT_REQ_NOTHING },
        { "--help", 1002, RTGETOPT_REQ_NOTHING }
    };
    RTGETOPTSTATE GetState;
    RTGETOPTUNION ValueUnion;
    int vrc = RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), iFirst, 0);
    AssertRCReturn(vrc, RTEXITCODE_FAILURE);
    if (a->argc == iFirst)
    {
        RTPrintf("Empty command parameter list, show help.\n");
        printHelp(g_pStdOut);
        return RTEXITCODE_SUCCESS;
    }

    Utf8Str strImageId;

    int c;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 'i':
            {
                if (strImageId.isNotEmpty())
                    return errorArgument("Duplicate parameter: --id");

                strImageId = ValueUnion.psz;
                if (strImageId.isEmpty())
                    return errorArgument("Empty parameter: --id");

                break;
            }

            case 1001:
            case 1002:
                printHelp(g_pStdOut);
                return RTEXITCODE_SUCCESS;
            case VINF_GETOPT_NOT_OPTION:
                return errorUnknownSubcommand(ValueUnion.psz);

            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    /* Delayed check. It allows us to print help information.*/
    hrc = checkAndSetCommonOptions(a, pCommonOpts);
    if (FAILED(hrc))
        return RTEXITCODE_FAILURE;

    if (strImageId.isEmpty())
        return errorArgument("Missing parameter: --id");


    ComPtr<ICloudProfile> pCloudProfile = pCommonOpts->profile.pCloudProfile;

    ComObjPtr<ICloudClient> oCloudClient;
    CHECK_ERROR2_RET(hrc, pCloudProfile,
                     CreateCloudClient(oCloudClient.asOutParam()),
                     RTEXITCODE_FAILURE);
    RTPrintf("Deleting cloud image with id %s...\n", strImageId.c_str());

    ComPtr<IProgress> progress;
    CHECK_ERROR2_RET(hrc, oCloudClient,
                     DeleteImage(Bstr(strImageId).raw(), progress.asOutParam()),
                     RTEXITCODE_FAILURE);
    hrc = showProgress(progress);
    CHECK_PROGRESS_ERROR_RET(progress, ("Deleting cloud image failed"), RTEXITCODE_FAILURE);

    if (SUCCEEDED(hrc))
        RTPrintf("Cloud image with was deleted successfully\n");

    return SUCCEEDED(hrc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}

static RTEXITCODE handleCloudImage(HandlerArg *a, int iFirst, PCLOUDCOMMONOPT pCommonOpts)
{
    setCurrentCommand(HELP_CMD_CLOUDIMAGE);
    setCurrentSubcommand(HELP_SCOPE_CLOUDIMAGE);
    if (a->argc == iFirst)
    {
        RTPrintf("Empty command parameter list, show help.\n");
        printHelp(g_pStdOut);
        return RTEXITCODE_SUCCESS;
    }

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "create",         1000, RTGETOPT_REQ_NOTHING },
        { "export",         1001, RTGETOPT_REQ_NOTHING },
        { "import",         1002, RTGETOPT_REQ_NOTHING },
        { "info",           1003, RTGETOPT_REQ_NOTHING },
        { "update",         1004, RTGETOPT_REQ_NOTHING },
        { "delete",         1005, RTGETOPT_REQ_NOTHING },
        { "help",           1006, RTGETOPT_REQ_NOTHING },
        { "--help",         1007, RTGETOPT_REQ_NOTHING }
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
                setCurrentSubcommand(HELP_SCOPE_CLOUDIMAGE_CREATE);
                return createCloudImage(a, GetState.iNext, pCommonOpts);
            case 1001:
                setCurrentSubcommand(HELP_SCOPE_CLOUDIMAGE_EXPORT);
                return exportCloudImage(a, GetState.iNext, pCommonOpts);
            case 1002:
                setCurrentSubcommand(HELP_SCOPE_CLOUDIMAGE_IMPORT);
                return importCloudImage(a, GetState.iNext, pCommonOpts);
            case 1003:
                setCurrentSubcommand(HELP_SCOPE_CLOUDIMAGE_INFO);
                return showCloudImageInfo(a, GetState.iNext, pCommonOpts);
            case 1004:
//              setCurrentSubcommand(HELP_SCOPE_CLOUDIMAGE_UPDATE);
                return updateCloudImage(a, GetState.iNext, pCommonOpts);
            case 1005:
                setCurrentSubcommand(HELP_SCOPE_CLOUDIMAGE_DELETE);
                return deleteCloudImage(a, GetState.iNext, pCommonOpts);
            case 1006:
            case 1007:
                printHelp(g_pStdOut);
                return RTEXITCODE_SUCCESS;
            case VINF_GETOPT_NOT_OPTION:
                return errorUnknownSubcommand(ValueUnion.psz);

            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    return errorNoSubcommand();
}

#ifdef VBOX_WITH_CLOUD_NET
struct CloudNetworkOptions
{
    BOOL fEnable;
    BOOL fDisable;
    Bstr strNetworkId;
    Bstr strNetworkName;
};
typedef struct CloudNetworkOptions CLOUDNETOPT;
typedef CLOUDNETOPT *PCLOUDNETOPT;

static RTEXITCODE createUpdateCloudNetworkCommon(ComPtr<ICloudNetwork> cloudNetwork, CLOUDNETOPT& options, PCLOUDCOMMONOPT pCommonOpts)
{
    HRESULT hrc = S_OK;

    Bstr strProvider = pCommonOpts->provider.pszProviderName;
    Bstr strProfile = pCommonOpts->profile.pszProfileName;

    if (options.fEnable)
    {
        CHECK_ERROR2_RET(hrc, cloudNetwork, COMSETTER(Enabled)(TRUE), RTEXITCODE_FAILURE);
    }
    if (options.fDisable)
    {
        CHECK_ERROR2_RET(hrc, cloudNetwork, COMSETTER(Enabled)(FALSE), RTEXITCODE_FAILURE);
    }
    if (options.strNetworkId.isNotEmpty())
    {
        CHECK_ERROR2_RET(hrc, cloudNetwork, COMSETTER(NetworkId)(options.strNetworkId.raw()), RTEXITCODE_FAILURE);
    }
    if (strProvider.isNotEmpty())
    {
        CHECK_ERROR2_RET(hrc, cloudNetwork, COMSETTER(Provider)(strProvider.raw()), RTEXITCODE_FAILURE);
    }
    if (strProfile.isNotEmpty())
    {
        CHECK_ERROR2_RET(hrc, cloudNetwork, COMSETTER(Profile)(strProfile.raw()), RTEXITCODE_FAILURE);
    }

    return RTEXITCODE_SUCCESS;
}


static RTEXITCODE createCloudNetwork(HandlerArg *a, int iFirst, PCLOUDCOMMONOPT pCommonOpts)
{
    HRESULT hrc = S_OK;
    hrc = checkAndSetCommonOptions(a, pCommonOpts);
    if (FAILED(hrc))
        return RTEXITCODE_FAILURE;

    /* Required parameters, the rest is handled in update */
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--disable",        'd', RTGETOPT_REQ_NOTHING },
        { "--enable",         'e', RTGETOPT_REQ_NOTHING },
        { "--network-id",     'i', RTGETOPT_REQ_STRING },
        { "--name",           'n', RTGETOPT_REQ_STRING },
    };

    RTGETOPTSTATE GetState;
    RTGETOPTUNION ValueUnion;
    int vrc = RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), iFirst, 0);
    AssertRCReturn(vrc, RTEXITCODE_FAILURE);

    CLOUDNETOPT options;
    options.fEnable = FALSE;
    options.fDisable = FALSE;

    int c;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 'd':
                options.fDisable = TRUE;
                break;
            case 'e':
                options.fEnable = TRUE;
                break;
            case 'i':
                options.strNetworkId=ValueUnion.psz;
                break;
            case 'n':
                options.strNetworkName=ValueUnion.psz;
                break;
            case VINF_GETOPT_NOT_OPTION:
                return errorUnknownSubcommand(ValueUnion.psz);
            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    if (options.strNetworkName.isEmpty())
        return errorArgument("Missing --name parameter");
    if (options.strNetworkId.isEmpty())
        return errorArgument("Missing --network-id parameter");

    ComPtr<IVirtualBox> pVirtualBox = a->virtualBox;

    ComPtr<ICloudNetwork> cloudNetwork;
    CHECK_ERROR2_RET(hrc, pVirtualBox,
                     CreateCloudNetwork(options.strNetworkName.raw(), cloudNetwork.asOutParam()),
                     RTEXITCODE_FAILURE);

    /* Fill out the created network */
    RTEXITCODE rc = createUpdateCloudNetworkCommon(cloudNetwork, options, pCommonOpts);
    if (RT_SUCCESS(rc))
        RTPrintf("Cloud network was created successfully\n");

    return rc;
}


static RTEXITCODE showCloudNetworkInfo(HandlerArg *a, int iFirst, PCLOUDCOMMONOPT pCommonOpts)
{
    RT_NOREF(pCommonOpts);
    HRESULT hrc = S_OK;
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--name",           'n', RTGETOPT_REQ_STRING },
    };
    RTGETOPTSTATE GetState;
    RTGETOPTUNION ValueUnion;
    int vrc = RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), iFirst, 0);
    AssertRCReturn(vrc, RTEXITCODE_FAILURE);

    Bstr strNetworkName;

    int c;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 'n':
                strNetworkName=ValueUnion.psz;
                break;
            case VINF_GETOPT_NOT_OPTION:
                return errorUnknownSubcommand(ValueUnion.psz);
            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    if (strNetworkName.isEmpty())
        return errorArgument("Missing --name parameter");

    ComPtr<IVirtualBox> pVirtualBox = a->virtualBox;
    ComPtr<ICloudNetwork> cloudNetwork;
    CHECK_ERROR2_RET(hrc, pVirtualBox,
                     FindCloudNetworkByName(strNetworkName.raw(), cloudNetwork.asOutParam()),
                     RTEXITCODE_FAILURE);

    RTPrintf("Name:            %ls\n", strNetworkName.raw());
    BOOL fEnabled = FALSE;
    cloudNetwork->COMGETTER(Enabled)(&fEnabled);
    RTPrintf("State:           %s\n", fEnabled ? "Enabled" : "Disabled");
    Bstr Provider;
    cloudNetwork->COMGETTER(Provider)(Provider.asOutParam());
    RTPrintf("CloudProvider:   %ls\n", Provider.raw());
    Bstr Profile;
    cloudNetwork->COMGETTER(Profile)(Profile.asOutParam());
    RTPrintf("CloudProfile:    %ls\n", Profile.raw());
    Bstr NetworkId;
    cloudNetwork->COMGETTER(NetworkId)(NetworkId.asOutParam());
    RTPrintf("CloudNetworkId:  %ls\n", NetworkId.raw());
    Bstr netName = BstrFmt("cloud-%ls", strNetworkName.raw());
    RTPrintf("VBoxNetworkName: %ls\n\n", netName.raw());

    return RTEXITCODE_SUCCESS;
}


static RTEXITCODE updateCloudNetwork(HandlerArg *a, int iFirst, PCLOUDCOMMONOPT pCommonOpts)
{
    HRESULT hrc = S_OK;

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--disable",        'd', RTGETOPT_REQ_NOTHING },
        { "--enable",         'e', RTGETOPT_REQ_NOTHING },
        { "--network-id",     'i', RTGETOPT_REQ_STRING },
        { "--name",           'n', RTGETOPT_REQ_STRING },
    };

    RTGETOPTSTATE GetState;
    RTGETOPTUNION ValueUnion;
    int vrc = RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), iFirst, 0);
    AssertRCReturn(vrc, RTEXITCODE_FAILURE);

    CLOUDNETOPT options;
    options.fEnable = FALSE;
    options.fDisable = FALSE;

    int c;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 'd':
                options.fDisable = TRUE;
                break;
            case 'e':
                options.fEnable = TRUE;
                break;
            case 'i':
                options.strNetworkId=ValueUnion.psz;
                break;
            case 'n':
                options.strNetworkName=ValueUnion.psz;
                break;
            case VINF_GETOPT_NOT_OPTION:
                return errorUnknownSubcommand(ValueUnion.psz);
            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    if (options.strNetworkName.isEmpty())
        return errorArgument("Missing --name parameter");

    ComPtr<IVirtualBox> pVirtualBox = a->virtualBox;
    ComPtr<ICloudNetwork> cloudNetwork;
    CHECK_ERROR2_RET(hrc, pVirtualBox,
                     FindCloudNetworkByName(options.strNetworkName.raw(), cloudNetwork.asOutParam()),
                     RTEXITCODE_FAILURE);

    RTEXITCODE rc = createUpdateCloudNetworkCommon(cloudNetwork, options, pCommonOpts);
    if (RT_SUCCESS(rc))
        RTPrintf("Cloud network %ls was updated successfully\n", options.strNetworkName.raw());

    return rc;
}


static RTEXITCODE deleteCloudNetwork(HandlerArg *a, int iFirst, PCLOUDCOMMONOPT pCommonOpts)
{
    RT_NOREF(pCommonOpts);
    HRESULT hrc = S_OK;
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--name",           'n', RTGETOPT_REQ_STRING },
    };
    RTGETOPTSTATE GetState;
    RTGETOPTUNION ValueUnion;
    int vrc = RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), iFirst, 0);
    AssertRCReturn(vrc, RTEXITCODE_FAILURE);

    Bstr strNetworkName;

    int c;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 'n':
                strNetworkName=ValueUnion.psz;
                break;
            case VINF_GETOPT_NOT_OPTION:
                return errorUnknownSubcommand(ValueUnion.psz);
            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    if (strNetworkName.isEmpty())
        return errorArgument("Missing --name parameter");

    ComPtr<IVirtualBox> pVirtualBox = a->virtualBox;
    ComPtr<ICloudNetwork> cloudNetwork;
    CHECK_ERROR2_RET(hrc, pVirtualBox,
                     FindCloudNetworkByName(strNetworkName.raw(), cloudNetwork.asOutParam()),
                     RTEXITCODE_FAILURE);

    CHECK_ERROR2_RET(hrc, pVirtualBox,
                     RemoveCloudNetwork(cloudNetwork),
                     RTEXITCODE_FAILURE);

    if (SUCCEEDED(hrc))
        RTPrintf("Cloud network %ls was deleted successfully\n", strNetworkName.raw());

    return SUCCEEDED(hrc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}


static bool errorOccured(HRESULT hrc, const char *pszFormat, ...)
{
    if (FAILED(hrc))
    {
        va_list va;
        va_start(va, pszFormat);
        Utf8Str strError(pszFormat, va);
        va_end(va);
        RTStrmPrintf(g_pStdErr, "%s (rc=%x)\n", strError.c_str(), hrc);
        RTStrmFlush(g_pStdErr);
        return true;
    }
    return false;
}


static int composeTemplatePath(const char *pcszTemplate, Bstr& strFullPath)
{
    com::Utf8Str strTemplatePath;
    int rc = RTPathAppPrivateNoArchCxx(strTemplatePath);
    if (RT_SUCCESS(rc))
        rc = RTPathAppendCxx(strTemplatePath, "UnattendedTemplates");
    if (RT_SUCCESS(rc))
        rc = RTPathAppendCxx(strTemplatePath, pcszTemplate);
    if (RT_FAILURE(rc))
    {
        RTStrmPrintf(g_pStdErr, "Failed to compose path to the unattended installer script templates (%Rrc)", rc);
        RTStrmFlush(g_pStdErr);
    }
    else
        strFullPath = strTemplatePath;

    return rc;
}

static HRESULT createLocalGatewayImage(ComPtr<IVirtualBox> virtualBox, const Bstr& aGatewayIso, const Bstr& aGuestAdditionsIso)
{
    /* Check if the image already exists. */
    HRESULT hrc;

    Bstr strGatewayVM = "lgw";
    Bstr strUser = "vbox";
    Bstr strPassword = "vbox";

    Bstr strInstallerScript;
    Bstr strPostInstallScript;

    if (RT_FAILURE(composeTemplatePath("lgw_ks.cfg", strInstallerScript)))
        return E_FAIL;
    if (RT_FAILURE(composeTemplatePath("lgw_postinstall.sh", strPostInstallScript)))
        return E_FAIL;

    ComPtr<ISystemProperties> systemProperties;
    ComPtr<IMedium> hd;
    Bstr defaultMachineFolder;
    Bstr guestAdditionsISO;
    hrc = virtualBox->COMGETTER(SystemProperties)(systemProperties.asOutParam());
    if (errorOccured(hrc, "Failed to obtain system properties."))
        return hrc;
    hrc = systemProperties->COMGETTER(DefaultMachineFolder)(defaultMachineFolder.asOutParam());
    if (errorOccured(hrc, "Failed to obtain default machine folder."))
        return hrc;
    if (aGuestAdditionsIso.isEmpty())
    {
        hrc = systemProperties->COMGETTER(DefaultAdditionsISO)(guestAdditionsISO.asOutParam());
        if (errorOccured(hrc, "Failed to obtain default guest additions ISO path."))
            return hrc;
        if (guestAdditionsISO.isEmpty())
        {
            errorOccured(E_INVALIDARG, "The default guest additions ISO path is empty nor it is provided as --guest-additions-iso parameter. Cannot proceed without it.");
            return E_INVALIDARG;
        }
    }
    else
        guestAdditionsISO = aGuestAdditionsIso;

    BstrFmt strGatewayImage("%ls\\gateways\\lgw.vdi", defaultMachineFolder.raw());
    hrc = virtualBox->OpenMedium(strGatewayImage.raw(), DeviceType_HardDisk, AccessMode_ReadWrite, FALSE, hd.asOutParam());
    /* If the image is already in place, there is nothing for us to do. */
    if (SUCCEEDED(hrc))
    {
        RTPrintf("Local gateway image already exists, skipping image preparation step.\n");
        return hrc;
    }

    RTPrintf("Preparing unattended install of temporary local gateway machine from %ls...\n", aGatewayIso.raw());
    /* The image does not exist, let's try to open the provided ISO file. */
    ComPtr<IMedium> iso;
    hrc = virtualBox->OpenMedium(aGatewayIso.raw(), DeviceType_DVD, AccessMode_ReadOnly, FALSE, iso.asOutParam());
    if (errorOccured(hrc, "Failed to open %ls.", aGatewayIso.raw()))
        return hrc;

    ComPtr<IMachine> machine;
    SafeArray<IN_BSTR> groups;
    groups.push_back(Bstr("/gateways").mutableRaw());
    hrc = virtualBox->CreateMachine(NULL, strGatewayVM.raw(), ComSafeArrayAsInParam(groups), Bstr("Oracle_64").raw(), Bstr("").raw(), machine.asOutParam());
    if (errorOccured(hrc, "Failed to create '%ls'.", strGatewayVM.raw()))
        return hrc;
    /* Initial configuration */
    hrc = machine->ApplyDefaults(NULL);
    if (errorOccured(hrc, "Failed to apply defaults to '%ls'.", strGatewayVM.raw()))
        return hrc;

    hrc = machine->COMSETTER(MemorySize)(512/*MB*/);
    if (errorOccured(hrc, "Failed to adjust memory size for '%ls'.", strGatewayVM.raw()))
        return hrc;

    /* No need for audio -- disable it. */
    ComPtr<IAudioAdapter> audioAdapter;
    hrc = machine->COMGETTER(AudioAdapter)(audioAdapter.asOutParam());
    if (errorOccured(hrc, "Failed to set attachment type for the second network adapter."))
        return hrc;

    hrc = audioAdapter->COMSETTER(Enabled)(FALSE);
    if (errorOccured(hrc, "Failed to disable the audio adapter."))
        return hrc;
    audioAdapter.setNull();

    hrc = virtualBox->RegisterMachine(machine);
    if (errorOccured(hrc, "Failed to register '%ls'.", strGatewayVM.raw()))
        return hrc;

    hrc = virtualBox->CreateMedium(Bstr("VDI").raw(), strGatewayImage.raw(), AccessMode_ReadWrite, DeviceType_HardDisk, hd.asOutParam());
    if (errorOccured(hrc, "Failed to create %ls.", strGatewayImage.raw()))
        return hrc;

    ComPtr<IProgress> progress;
    com::SafeArray<MediumVariant_T>  mediumVariant;
    mediumVariant.push_back(MediumVariant_Standard);

    /* Kick off the creation of a dynamic growing disk image with the given capacity. */
    hrc = hd->CreateBaseStorage(8ll * 1000 * 1000 * 1000 /* 8GB */,
                                ComSafeArrayAsInParam(mediumVariant),
                                progress.asOutParam());
    if (errorOccured(hrc, "Failed to create base storage for local gateway image."))
        return hrc;

    hrc = showProgress(progress);
    CHECK_PROGRESS_ERROR_RET(progress, ("Failed to create base storage for local gateway image."), hrc);

    ComPtr<ISession> session;
    hrc = session.createInprocObject(CLSID_Session);
    hrc = machine->LockMachine(session, LockType_Write);
    if (errorOccured(hrc, "Failed to lock '%ls' for modifications.", strGatewayVM.raw()))
        return hrc;

    ComPtr<IMachine> sessionMachine;
    hrc = session->COMGETTER(Machine)(sessionMachine.asOutParam());
    if (errorOccured(hrc, "Failed to obtain a mutable machine."))
        return hrc;

    hrc = sessionMachine->AttachDevice(Bstr("SATA").raw(), 0, 0, DeviceType_HardDisk, hd);
    if (errorOccured(hrc, "Failed to attach HD to '%ls'.", strGatewayVM.raw()))
        return hrc;

    hrc = sessionMachine->AttachDevice(Bstr("IDE").raw(), 0, 0, DeviceType_DVD, iso);
    if (errorOccured(hrc, "Failed to attach ISO to '%ls'.", strGatewayVM.raw()))
        return hrc;

    /* Save settings */
    hrc = sessionMachine->SaveSettings();
    if (errorOccured(hrc, "Failed to save '%ls' settings.", strGatewayVM.raw()))
        return hrc;
    session->UnlockMachine();

    /* Prepare unattended install */
    ComPtr<IUnattended> unattended;
    hrc = virtualBox->CreateUnattendedInstaller(unattended.asOutParam());
    if (errorOccured(hrc, "Failed to create unattended installer."))
        return hrc;

    hrc = unattended->COMSETTER(Machine)(machine);
    if (errorOccured(hrc, "Failed to set machine for the unattended installer."))
        return hrc;

    hrc = unattended->COMSETTER(IsoPath)(aGatewayIso.raw());
    if (errorOccured(hrc, "Failed to set machine for the unattended installer."))
        return hrc;

    hrc = unattended->COMSETTER(User)(strUser.raw());
    if (errorOccured(hrc, "Failed to set user for the unattended installer."))
        return hrc;

    hrc = unattended->COMSETTER(Password)(strPassword.raw());
    if (errorOccured(hrc, "Failed to set password for the unattended installer."))
        return hrc;

    hrc = unattended->COMSETTER(FullUserName)(strUser.raw());
    if (errorOccured(hrc, "Failed to set full user name for the unattended installer."))
        return hrc;

    hrc = unattended->COMSETTER(InstallGuestAdditions)(TRUE);
    if (errorOccured(hrc, "Failed to enable guest addtions for the unattended installer."))
        return hrc;

    hrc = unattended->COMSETTER(AdditionsIsoPath)(guestAdditionsISO.raw());
    if (errorOccured(hrc, "Failed to set guest addtions ISO path for the unattended installer."))
        return hrc;

    hrc = unattended->COMSETTER(ScriptTemplatePath)(strInstallerScript.raw());
    if (errorOccured(hrc, "Failed to set script template for the unattended installer."))
        return hrc;

    hrc = unattended->COMSETTER(PostInstallScriptTemplatePath)(strPostInstallScript.raw());
    if (errorOccured(hrc, "Failed to set post install script template for the unattended installer."))
        return hrc;

    hrc = unattended->Prepare();
    if (errorOccured(hrc, "Failed to prepare unattended installation."))
        return hrc;

    hrc = unattended->ConstructMedia();
    if (errorOccured(hrc, "Failed to construct media for unattended installation."))
        return hrc;

    hrc = unattended->ReconfigureVM();
    if (errorOccured(hrc, "Failed to reconfigure %ls for unattended installation.", strGatewayVM.raw()))
        return hrc;

#define SHOW_ATTR(a_Attr, a_szText, a_Type, a_szFmt) do { \
            a_Type Value; \
            HRESULT hrc2 = unattended->COMGETTER(a_Attr)(&Value); \
            if (SUCCEEDED(hrc2)) \
                RTPrintf("  %32s = " a_szFmt "\n", a_szText, Value); \
            else \
                RTPrintf("  %32s = failed: %Rhrc\n", a_szText, hrc2); \
        } while (0)
#define SHOW_STR_ATTR(a_Attr, a_szText) do { \
            Bstr bstrString; \
            HRESULT hrc2 = unattended->COMGETTER(a_Attr)(bstrString.asOutParam()); \
            if (SUCCEEDED(hrc2)) \
                RTPrintf("  %32s = %ls\n", a_szText, bstrString.raw()); \
            else \
                RTPrintf("  %32s = failed: %Rhrc\n", a_szText, hrc2); \
        } while (0)

    SHOW_STR_ATTR(IsoPath,                       "isoPath");
    SHOW_STR_ATTR(User,                          "user");
    SHOW_STR_ATTR(Password,                      "password");
    SHOW_STR_ATTR(FullUserName,                  "fullUserName");
    SHOW_STR_ATTR(ProductKey,                    "productKey");
    SHOW_STR_ATTR(AdditionsIsoPath,              "additionsIsoPath");
    SHOW_ATTR(    InstallGuestAdditions,         "installGuestAdditions",    BOOL, "%RTbool");
    SHOW_STR_ATTR(ValidationKitIsoPath,          "validationKitIsoPath");
    SHOW_ATTR(    InstallTestExecService,        "installTestExecService",   BOOL, "%RTbool");
    SHOW_STR_ATTR(Locale,                        "locale");
    SHOW_STR_ATTR(Country,                       "country");
    SHOW_STR_ATTR(TimeZone,                      "timeZone");
    SHOW_STR_ATTR(Proxy,                         "proxy");
    SHOW_STR_ATTR(Hostname,                      "hostname");
    SHOW_STR_ATTR(PackageSelectionAdjustments,   "packageSelectionAdjustments");
    SHOW_STR_ATTR(AuxiliaryBasePath,             "auxiliaryBasePath");
    SHOW_ATTR(    ImageIndex,                    "imageIndex",               ULONG, "%u");
    SHOW_STR_ATTR(ScriptTemplatePath,            "scriptTemplatePath");
    SHOW_STR_ATTR(PostInstallScriptTemplatePath, "postInstallScriptTemplatePath");
    SHOW_STR_ATTR(PostInstallCommand,            "postInstallCommand");
    SHOW_STR_ATTR(ExtraInstallKernelParameters,  "extraInstallKernelParameters");
    SHOW_STR_ATTR(Language,                      "language");
    SHOW_STR_ATTR(DetectedOSTypeId,              "detectedOSTypeId");
    SHOW_STR_ATTR(DetectedOSVersion,             "detectedOSVersion");
    SHOW_STR_ATTR(DetectedOSFlavor,              "detectedOSFlavor");
    SHOW_STR_ATTR(DetectedOSLanguages,           "detectedOSLanguages");
    SHOW_STR_ATTR(DetectedOSHints,               "detectedOSHints");

#undef SHOW_STR_ATTR
#undef SHOW_ATTR

    /* 'unattended' is no longer needed. */
    unattended.setNull();

    RTPrintf("Performing unattended install of temporary local gateway...\n");

    hrc = machine->LaunchVMProcess(session, Bstr("gui").raw(), ComSafeArrayNullInParam(), progress.asOutParam());
    if (errorOccured(hrc, "Failed to launch '%ls'.", strGatewayVM.raw()))
        return hrc;

    hrc = progress->WaitForCompletion(-1);
    if (errorOccured(hrc, "Failed to launch '%ls'.", strGatewayVM.raw()))
        return hrc;

    unsigned i = 0;
    const char progressChars[] = { '|', '/', '-', '\\'};
    MachineState_T machineState;
    uint64_t u64Started = RTTimeMilliTS();
    do
    {
        RTThreadSleep(1000); /* One second */
        hrc = machine->COMGETTER(State)(&machineState);
        if (errorOccured(hrc, "Failed to get machine state."))
            break;
        RTPrintf("\r%c", progressChars[i++ % sizeof(progressChars)]);
        if (machineState == MachineState_Aborted)
        {
            errorOccured(E_ABORT, "Temporary local gateway VM has aborted.");
            return E_ABORT;
        }
    }
    while (machineState != MachineState_PoweredOff && RTTimeMilliTS() - u64Started < 40 * 60 * 1000);

    if (machineState != MachineState_PoweredOff)
    {
        errorOccured(E_ABORT, "Timed out (40min) while waiting for unattended install to finish.");
        return E_ABORT;
    }
    /* Machine will still be immutable for a short while after powering off, let's wait a little. */
    RTThreadSleep(5000); /* Five seconds */

    RTPrintf("\rDone.\n");

    hrc = machine->LockMachine(session, LockType_Write);
    if (errorOccured(hrc, "Failed to lock '%ls' for modifications.", strGatewayVM.raw()))
        return hrc;

    RTPrintf("Detaching local gateway image...\n");
    hrc = session->COMGETTER(Machine)(sessionMachine.asOutParam());
    if (errorOccured(hrc, "Failed to obtain a mutable machine."))
        return hrc;

    hrc = sessionMachine->DetachDevice(Bstr("SATA").raw(), 0, 0);
    if (errorOccured(hrc, "Failed to detach HD to '%ls'.", strGatewayVM.raw()))
        return hrc;

    /* Remove the image from the media registry. */
    hd->Close();

    /* Save settings */
    hrc = sessionMachine->SaveSettings();
    if (errorOccured(hrc, "Failed to save '%ls' settings.", strGatewayVM.raw()))
        return hrc;
    session->UnlockMachine();

#if 0
    /** @todo Unregistering the temporary VM makes the image mutable again. Find out the way around it! */
    RTPrintf("Unregistering temporary local gateway machine...\n");
    SafeIfaceArray<IMedium> media;
    hrc = machine->Unregister(CleanupMode_DetachAllReturnNone, ComSafeArrayAsOutParam(media));
    if (errorOccured(hrc, "Failed to unregister '%ls'.", strGatewayVM.raw()))
        return hrc;
    hrc = machine->DeleteConfig(ComSafeArrayAsInParam(media), progress.asOutParam());
    if (errorOccured(hrc, "Failed to delete config for '%ls'.", strGatewayVM.raw()))
        return hrc;
    hrc = progress->WaitForCompletion(-1);
    if (errorOccured(hrc, "Failed to delete config for '%ls'.", strGatewayVM.raw()))
        return hrc;
#endif

    RTPrintf("Making local gateway image immutable...\n");
    hrc = virtualBox->OpenMedium(strGatewayImage.raw(), DeviceType_HardDisk, AccessMode_ReadWrite, FALSE, hd.asOutParam());
    if (errorOccured(hrc, "Failed to open '%ls'.", strGatewayImage.raw()))
        return hrc;
    hd->COMSETTER(Type)(MediumType_Immutable);
    if (errorOccured(hrc, "Failed to make '%ls' immutable.", strGatewayImage.raw()))
        return hrc;

    return S_OK;
}


static RTEXITCODE setupCloudNetworkEnv(HandlerArg *a, int iFirst, PCLOUDCOMMONOPT pCommonOpts)
{
    RT_NOREF(pCommonOpts);
    HRESULT hrc = S_OK;
    static const RTGETOPTDEF s_aOptions[] =
    {
        { "--gateway-os-name",      'n', RTGETOPT_REQ_STRING },
        { "--gateway-os-version",   'v', RTGETOPT_REQ_STRING },
        { "--gateway-shape",        's', RTGETOPT_REQ_STRING },
        { "--tunnel-network-name",  't', RTGETOPT_REQ_STRING },
        { "--tunnel-network-range", 'r', RTGETOPT_REQ_STRING },
        { "--guest-additions-iso",  'a', RTGETOPT_REQ_STRING },
        { "--local-gateway-iso",    'l', RTGETOPT_REQ_STRING }
    };
    RTGETOPTSTATE GetState;
    RTGETOPTUNION ValueUnion;
    int vrc = RTGetOptInit(&GetState, a->argc, a->argv, s_aOptions, RT_ELEMENTS(s_aOptions), iFirst, 0);
    AssertRCReturn(vrc, RTEXITCODE_FAILURE);

    Bstr strGatewayOsName;
    Bstr strGatewayOsVersion;
    Bstr strGatewayShape;
    Bstr strTunnelNetworkName;
    Bstr strTunnelNetworkRange;
    Bstr strLocalGatewayIso;
    Bstr strGuestAdditionsIso;

    int c;
    while ((c = RTGetOpt(&GetState, &ValueUnion)) != 0)
    {
        switch (c)
        {
            case 'n':
                strGatewayOsName=ValueUnion.psz;
                break;
            case 'v':
                strGatewayOsVersion=ValueUnion.psz;
                break;
            case 's':
                strGatewayShape=ValueUnion.psz;
                break;
            case 't':
                strTunnelNetworkName=ValueUnion.psz;
                break;
            case 'r':
                strTunnelNetworkRange=ValueUnion.psz;
                break;
            case 'l':
                strLocalGatewayIso=ValueUnion.psz;
                break;
            case 'a':
                strGuestAdditionsIso=ValueUnion.psz;
                break;
            case VINF_GETOPT_NOT_OPTION:
                return errorUnknownSubcommand(ValueUnion.psz);
            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    /* Delayed check. It allows us to print help information.*/
    hrc = checkAndSetCommonOptions(a, pCommonOpts);
    if (FAILED(hrc))
        return RTEXITCODE_FAILURE;

    if (strLocalGatewayIso.isEmpty())
        return errorArgument("Missing --local-gateway-iso parameter");

    ComPtr<IVirtualBox> pVirtualBox = a->virtualBox;

    hrc = createLocalGatewayImage(pVirtualBox, strLocalGatewayIso, strGuestAdditionsIso);
    if (FAILED(hrc))
        return RTEXITCODE_FAILURE;

    RTPrintf("Setting up tunnel network in the cloud...\n");

    ComPtr<ICloudProfile> pCloudProfile = pCommonOpts->profile.pCloudProfile;

    ComObjPtr<ICloudClient> oCloudClient;
    CHECK_ERROR2_RET(hrc, pCloudProfile,
                     CreateCloudClient(oCloudClient.asOutParam()),
                     RTEXITCODE_FAILURE);

    ComPtr<ICloudNetworkEnvironmentInfo> cloudNetworkEnv;
    ComPtr<IProgress> progress;
    CHECK_ERROR2_RET(hrc, oCloudClient,
                     SetupCloudNetworkEnvironment(strTunnelNetworkName.raw(), strTunnelNetworkRange.raw(),
                                                  strGatewayOsName.raw(), strGatewayOsVersion.raw(), strGatewayShape.raw(),
                                                  cloudNetworkEnv.asOutParam(), progress.asOutParam()),
                     RTEXITCODE_FAILURE);

    hrc = showProgress(progress);
    CHECK_PROGRESS_ERROR_RET(progress, ("Setting up cloud network environment failed"), RTEXITCODE_FAILURE);

    Bstr tunnelNetworkId;
    hrc = cloudNetworkEnv->COMGETTER(TunnelNetworkId)(tunnelNetworkId.asOutParam());
    RTPrintf("Cloud network environment was set up successfully. Tunnel network id is: %ls\n", tunnelNetworkId.raw());

    return SUCCEEDED(hrc) ? RTEXITCODE_SUCCESS : RTEXITCODE_FAILURE;
}


static RTEXITCODE handleCloudNetwork(HandlerArg *a, int iFirst, PCLOUDCOMMONOPT pCommonOpts)
{
    if (a->argc < 1)
        return errorNoSubcommand();

    static const RTGETOPTDEF s_aOptions[] =
    {
        { "create",         1000, RTGETOPT_REQ_NOTHING },
        { "info",           1001, RTGETOPT_REQ_NOTHING },
        { "update",         1002, RTGETOPT_REQ_NOTHING },
        { "delete",         1003, RTGETOPT_REQ_NOTHING },
        { "setup",          1004, RTGETOPT_REQ_NOTHING }
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
                return createCloudNetwork(a, GetState.iNext, pCommonOpts);
            case 1001:
                return showCloudNetworkInfo(a, GetState.iNext, pCommonOpts);
            case 1002:
                return updateCloudNetwork(a, GetState.iNext, pCommonOpts);
            case 1003:
                return deleteCloudNetwork(a, GetState.iNext, pCommonOpts);
            case 1004:
                return setupCloudNetworkEnv(a, GetState.iNext, pCommonOpts);
            case VINF_GETOPT_NOT_OPTION:
                return errorUnknownSubcommand(ValueUnion.psz);

            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    return errorNoSubcommand();
}
#endif /* VBOX_WITH_CLOUD_NET */


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
            case 1001:
                return handleCloudImage(a, GetState.iNext, &commonOpts);
            case 1002:
                return handleCloudInstance(a, GetState.iNext, &commonOpts);
#ifdef VBOX_WITH_CLOUD_NET
            case 1003:
                return handleCloudNetwork(a, GetState.iNext, &commonOpts);
#endif /* VBOX_WITH_CLOUD_NET */
            case VINF_GETOPT_NOT_OPTION:
                return errorUnknownSubcommand(ValueUnion.psz);

            default:
                return errorGetOpt(c, &ValueUnion);
        }
    }

    return errorNoSubcommand();
}
