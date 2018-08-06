/* $Id$*/
/** @file
 * ICloudClient  COM class implementations.
 */

/*
 * Copyright (C) 2008-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <iostream>
#include <algorithm>
#include <map>

#include <iprt/path.h>
#include <iprt/cpp/utils.h>
#include <VBox/com/array.h>

#include "CloudClientImpl.h"
#include "CloudUserProfileManagerImpl.h"
#include "CloudUserProfilesImpl.h"
#include "CloudAPI.h"
#include "VirtualBoxImpl.h"
#include "Global.h"
#include "MachineImpl.h"
#include "AutoCaller.h"
#include "Logging.h"

using namespace std;


////////////////////////////////////////////////////////////////////////////////
//
// CloudClient implementation
//
////////////////////////////////////////////////////////////////////////////////
CloudClient::CloudClient()
    : mParent(NULL)
{
    LogRel(("CloudClient::CloudClient()\n"));
}

CloudClient::~CloudClient()
{
    LogRel(("CloudClient::~CloudClient()\n"));
}

HRESULT CloudClient::FinalConstruct()
{
    return BaseFinalConstruct();
}

void CloudClient::FinalRelease()
{
    uninit();

    BaseFinalRelease();
}

void CloudClient::uninit()
{
    LogRel(("CloudClient::uninit()\n"));
    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    unconst(mParent) = NULL;

    {
        std::map <com::Guid, CloudCommandCl*>::iterator it = mCloudCommandMap.begin();
        while (it != mCloudCommandMap.end())
        {
            delete it->second;
            ++it;
        }
    }

    {
        std::map <Utf8Str, CloudAPI*>::iterator it = mCloudAPIInstanceMap.begin();
        while (it != mCloudAPIInstanceMap.end())
        {
            delete it->second;
            ++it;
        }
    }
}

HRESULT CloudClient::init(VirtualBox *aParent)
{
    LogRel(("aParent=%p\n", aParent));
    ComAssertRet(aParent, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    unconst(mParent) = aParent;

    mCloudProvider = CloudProviderId_OCI;

    LogRel(("aParent=%p\n", aParent));

    /*
     * Confirm a successful initialization
     */
    autoInitSpan.setSucceeded();

    return S_OK;
}

HRESULT CloudClient::initCloudClient(CloudUserProfiles *aProfiles,
                                     VirtualBox *aParent,
                                     CloudProviderId_T aCloudProvider,
                                     const com::Utf8Str &aProfileName)
{
    LogRel(("aParent=%p\n", aParent));
    ComAssertRet(aParent, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    unconst(mParent) = aParent;

    HRESULT hrc = S_OK;

    try
    {
        mCloudProvider = aCloudProvider;
        LogRel(("CloudProvider = %d\n", mCloudProvider));
        CloudUserProfiles *lProfiles = aProfiles;
        std::vector<com::Utf8Str> lNames;
        std::vector<com::Utf8Str> lValues;
        hrc = lProfiles->getProfileProperties(aProfileName, lNames, lValues);
        if (FAILED(hrc))
        {
            return hrc;
        }

        for (size_t i=0;i<lNames.size();++i)
        {
            com::Utf8Str value = (i<lValues.size()) ? lValues.at(i) : "";
            mCloudProfile[lNames.at(i)] = value;
        }

        CloudProviderId_T aProvider;
        hrc = lProfiles->getProvider(&aProvider);

        switch(aProvider)
        {
            case CloudProviderId_OCI:
            default:
                LogRel(("Reading profile %s has been done\n", aProfileName.c_str()));
                break;
        }
    }
    catch (HRESULT arc)
    {
        hrc = arc;
        LogRel(("Get cought an exception %d\n", hrc));
    }
    catch (std::bad_alloc &)
    {
        return E_OUTOFMEMORY;
    }

    /*
     * Confirm a successful initialization
     */
    autoInitSpan.setSucceeded();

    return hrc;
}

HRESULT CloudClient::createCommand(CloudCommand_T aCloudCommand, com::Guid &aId)
{
    HRESULT hrc = S_OK;
    LogRel(("CloudClient::createCommand: %s \n", aCloudCommand));
    int vrc = i_createCommand(aCloudCommand, aId);
    if (RT_FAILURE(vrc))
        hrc = setErrorBoth(E_FAIL, vrc, tr("Couldn't create command %d"), aCloudCommand);

    return hrc;
}

int CloudClient::i_createCommand(CloudCommand_T aCloudCommand, com::Guid &aId)
{
    LogRel(("Enter CloudClient::i_createCommand()\n"));
    int vrc = VINF_SUCCESS;

    try
    {
        CloudAPI* actualAPI = NULL;
        Utf8Str apiName;
        vrc = i_queryAPINameForCommand(aCloudCommand, apiName);
        if (RT_SUCCESS(vrc) && mCloudAPIInstanceMap[apiName] == NULL)
        {
            vrc = i_queryAPIForCommand(aCloudCommand, &actualAPI);
            if (RT_SUCCESS(vrc) && actualAPI != NULL)
                mCloudAPIInstanceMap[apiName] = actualAPI;
        }
        else
            actualAPI = mCloudAPIInstanceMap[apiName];

        if (RT_SUCCESS(vrc))
        {
            com::Guid lId;
            lId.create();
            CloudCommandCl *newCloudCommand = NULL;

            actualAPI->createCloudCommand(aCloudCommand, mCloudProfile, lId, &newCloudCommand);
            newCloudCommand->mAPIBaseURI = actualAPI->getBaseAPIUrl(actualAPI->getDefaultVersion(), mCloudProfile);
            if (!newCloudCommand->mAPIBaseURI.isEmpty())
            {
                LogRel(("Base API \"%s\" URI is %s \n", apiName.c_str(), newCloudCommand->mAPIBaseURI.c_str()));
                newCloudCommand->mUsedAPI = actualAPI;
                mCloudCommandMap.insert(make_pair(newCloudCommand->mId, newCloudCommand));
                aId = newCloudCommand->mId;
            }
            else
                vrc = VERR_NOT_FOUND;
        }

    }
    catch (int arc)
    {
        vrc = arc;
        LogRel(("Get cought an exception %d\n", vrc));
    }

    return vrc;
}

HRESULT CloudClient::runCommand(const com::Guid &aId, LONG64 aTimeout, CloudCommandResult_T *aResult)
{
    LogRel(("CloudClient::runCommand: %s, %d\n", aId.toString().c_str(), aTimeout));
    HRESULT hrc = S_OK;
    std::map <com::Guid, CloudCommandCl*>::iterator it = mCloudCommandMap.find(aId);
    if (it !=mCloudCommandMap.end() && it->second->mfValid == true)
    {
        CloudCommandCl* cc = it->second;
        cc->mTimeout = aTimeout;
        int vrc = i_runCommand(*cc);
        if (RT_FAILURE(vrc))
        {
            hrc = setErrorBoth(E_FAIL, vrc, tr("Execution of the command with UUID \"%s\" was failed"),
                               aId.toString().c_str());
            *aResult = CloudCommandResult_success;
        }
        else 
            *aResult = CloudCommandResult_failed;
    }
    else
        hrc = setErrorBoth(E_FAIL, VERR_NOT_FOUND, tr("Command with UUID \"%s\" wasn't found or hasn't been validated yet"),
                           aId.toString().c_str());

    return hrc;
}

//int CloudClient::i_runCommand(CloudCommandCl &aCC, ComObjPtr<Progress> *aProgress)
//{
//    LogRel(("Enter CloudClient::i_runCommand()\n"));
//    int vrc = aCC.mUsedAPI->callAPICommand(aCC, aProgress);
//    return vrc;
//}

int CloudClient::i_runCommand(CloudCommandCl &aCC)
{
    LogRel(("Enter CloudClient::i_runCommand()\n"));
    int vrc = aCC.mUsedAPI->callAPICommand(aCC);
    return vrc;
}

HRESULT CloudClient::validateCommand(const com::Guid &aId, BOOL *aValid)
{
    HRESULT hrc = S_OK;
    //check all data in the operation and set the flag
    //this step allows easy to control which operation may be launch
    std::map <com::Guid, CloudCommandCl*>::const_iterator cit = mCloudCommandMap.find(aId);
    if (cit != mCloudCommandMap.end())
    {
        std::map<Utf8Str, Utf8Str> paramNameValueMap;
        int vrc = i_validateCommand(*(cit->second), paramNameValueMap, false);
        if (RT_SUCCESS(vrc))
            *aValid = true;
        else
        {
            *aValid = false;
            hrc = setErrorBoth(E_FAIL, vrc, tr("The command %d not validated"), cit->second->mCommand);
        }
    }
    else
        hrc = setErrorBoth(E_FAIL, VERR_NOT_FOUND, tr("Command with UUID \"%s\" wasn't found"),
                           aId.toString().c_str());
    return hrc;
}

int CloudClient::i_validateCommand(CloudCommandCl &aCC,
                                   const std::map<Utf8Str, Utf8Str> &aParamNameValueMap,
                                   bool fWeak)
{
    LogRel(("Enter CloudClient::i_validateCommand()\n"));
    int vrc = VINF_SUCCESS;
//  bool isSimple = false;
//  vrc = i_isSimpleCommand(aCC, isSimple);
//  if (!isSimple)//parameters are needed for the command
    {
        std::vector<com::Utf8Str> paramNames;
        paramNames.reserve(15);
        std::map<Utf8Str, Utf8Str> freshParamNameValueMap;

        vrc = i_getCommandParameterNames(aCC, paramNames);
        if (RT_SUCCESS(vrc))
        {
            for (size_t i=0;i<paramNames.size();++i)
            {
                std::map <Utf8Str, Utf8Str>::const_iterator cit = aParamNameValueMap.find(paramNames.at(i));
                if (cit != aParamNameValueMap.end())
                    freshParamNameValueMap.insert(make_pair(cit->first,cit->second));
                else
                {
                    if (fWeak)
                        vrc = VINF_TRY_AGAIN;
                    else
                        vrc = VERR_NOT_FOUND;

                    break;
                }
            }

            if (RT_SUCCESS(vrc) && vrc != VINF_TRY_AGAIN)
            {
                std::map <Utf8Str, Utf8Str>::const_iterator cit = freshParamNameValueMap.begin();
                while (cit != freshParamNameValueMap.end())
                {
                    //insert new parameter/value or update old one
                    aCC.mParameters[cit->first] = cit->second;
                    ++cit;
                }

                aCC.mfValid = true;
            }
            else
                aCC.mfValid = false;
        }
    }

    return vrc;
}

HRESULT CloudClient::checkCommandResult(const com::Guid &aId,
                                        LONG64 *aStartTime,
                                        LONG64 *aLastTime,
                                        CloudCommandResult_T *aResult)
{
    HRESULT hrc = S_OK;
    int vrc = VINF_SUCCESS;
    LogRel(("CloudClient::checkCommandResult: %s, %d, %d, %d\n",
            aId.toString().c_str(),
            *aStartTime,
            *aLastTime,
            aResult));
    std::map <com::Guid, CloudCommandCl*>::const_iterator cit = mCloudCommandMap.find(aId);
    if (cit != mCloudCommandMap.end())
    {
        CloudCommandResult_T res;
        CloudCommandCl *cc = cit->second;
        vrc = i_checkCommandResult(*cc, res);
        if (RT_FAILURE(vrc))
            hrc = setErrorBoth(E_FAIL, vrc, tr("Coudn't get the result for command with UUID \"%s\" "),
                               aId.toString().c_str());
        else
            *aResult = res;
    }
    else
    {
        vrc = VERR_NOT_FOUND;
        if (RT_FAILURE(vrc))
            hrc = setErrorBoth(E_FAIL, VERR_NOT_FOUND, tr("Command with UUID \"%s\" wasn't found"),
                               aId.toString().c_str());
    }

    return hrc;
}

int CloudClient::i_checkCommandResult(CloudCommandCl &aCC,
                                      CloudCommandResult_T &aResult)
{
    LogRel(("Enter CloudClient::i_checkCommandResult()\n"));
    RT_NOREF(aCC, aResult);
    int vrc = VINF_SUCCESS;//aCC.parseResult(aResult);
    return vrc;
}

int CloudClient::i_parseResponse(CloudCommandCl &aCC)
{
    LogRel(("Enter CloudClient::i_parseResponse()\n"));
    int vrc = VINF_SUCCESS;
    //create DataModel here and fill it from the response data
    DataParser *dp = NULL;
    vrc = aCC.createDataParser(&dp);
    if (RT_SUCCESS(vrc))
        vrc = dp->parseData(aCC);
    ////////////////////////////////
    //maybe partially work with data here
    ////////////////////////////////

    delete dp;

    return vrc;
}

int CloudClient::i_getCommandById(const com::Guid &aId, CloudCommandCl **aCC)
{
    int vrc = VINF_SUCCESS;
    std::map <com::Guid, CloudCommandCl*>::iterator cit = mCloudCommandMap.find(aId);
    if (cit != mCloudCommandMap.end())
    {
        *aCC = cit->second;
//      CloudCommandCl * lCC = cit->second->copy();
//      aCC = *lCC;
    }
    else
        vrc = VERR_NOT_FOUND;

    return vrc;
}

HRESULT CloudClient::getCommandParameterNames(CloudCommand_T aCloudCommand,
                                              std::vector<com::Utf8Str> &aParameterNames)
{
    LogRel(("CloudClient::getCommandParameterNames()\n"));
    RT_NOREF(aCloudCommand, aParameterNames);
    HRESULT hrc = setErrorBoth(E_FAIL, VERR_NOT_IMPLEMENTED, tr("Not implemented"));
    return hrc;
}

HRESULT CloudClient::getCommandParameterProperties(const com::Utf8Str &aParameterName,
                                                   com::Utf8Str &aParameterType,
                                                   com::Utf8Str &aParameterDesc,
                                                   std::vector<com::Utf8Str> &aParameterValues)
{
    LogRel(("CloudClient::getCommandParameterProperties()\n"));
    RT_NOREF(aParameterName, aParameterType, aParameterDesc, aParameterValues);
    HRESULT hrc = setErrorBoth(E_FAIL, VERR_NOT_IMPLEMENTED, tr("Not implemented"));
    return hrc;
}

HRESULT CloudClient::getCommandParameters(CloudCommand_T aCloudCommand, com::Utf8Str &aJsonString)
{
    LogRel(("CloudClient::getCommandParameters()\n"));
    RT_NOREF(aCloudCommand, aJsonString);
    HRESULT hrc = setErrorBoth(E_FAIL, VERR_NOT_IMPLEMENTED, tr("Not implemented"));
    return hrc;
}

HRESULT CloudClient::setCommandParameters(const com::Guid &aId,
                                          const std::vector<com::Utf8Str> &aNames,
                                          const std::vector<com::Utf8Str> &aValues)
{
    HRESULT hrc = S_OK;
    std::vector<com::Utf8Str> parameterNames;
    std::map <com::Guid, CloudCommandCl*>::iterator it = mCloudCommandMap.find(aId);
    if (it !=mCloudCommandMap.end())
    {
        hrc = getCommandParameterNames(it->second->mCommand, parameterNames);
        std::vector<com::Utf8Str>::const_iterator cit = aNames.begin();
        for (size_t i=0 ; i<aNames.size() && i<aValues.size(); ++i)
        {
            std::vector<com::Utf8Str>::iterator fit = find(parameterNames.begin(), parameterNames.end(), *cit);
            if (fit != parameterNames.end())
            {
                it->second->mParameters[aNames[i]] = aValues[i];
            }
        }
    }
    return hrc;
}

HRESULT CloudClient::getCommandsForOperation(CloudOperation_T aCloudOperation,
                                             BOOL aPrep,
                                             std::vector<CloudCommand_T> &aCloudCommands)
{
    LogRel(("CloudClient::getCommandsforOperation()\n"));
    RT_NOREF(aCloudOperation, aPrep, aCloudCommands);
    HRESULT hrc = setErrorBoth(E_FAIL, VERR_NOT_IMPLEMENTED, tr("Not implemented"));
    return hrc;
}

HRESULT CloudClient::getOperationParameters(CloudOperation_T aCloudOperation, com::Utf8Str &aJsonString)
{
    LogRel(("CloudClient::getOperationParameters()\n"));
    RT_NOREF(aCloudOperation, aJsonString);
    HRESULT hrc = setErrorBoth(E_FAIL, VERR_NOT_IMPLEMENTED, tr("Not implemented"));
    return hrc;
}

int CloudClient::i_getCommandParameterNames(const CloudCommandCl &aCC,
                                            std::vector<com::Utf8Str> &aParameterNames)
{
    LogRel(("Enter CloudClient::i_getCommandParameterNames()\n"));
    int vrc = VINF_SUCCESS;
    std::vector<com::Utf8Str> localParameterNames;
    localParameterNames.reserve(20);
    vrc = aCC.getParameterNames(localParameterNames);
//  vrc = aCC.mUsedAPI->getCommandParameterNames(&aCC, aParameterNames);
    if (RT_FAILURE(vrc))
        vrc = VERR_NOT_FOUND;
    else
    {
        size_t vecSize = localParameterNames.size();
        for (size_t i=0;i<vecSize;++i)
        {
            aParameterNames.push_back(localParameterNames.at(i));
        }
    }
    return vrc;
}

int CloudClient::i_isSimpleCommand(const CloudCommandCl &aCC, bool &fSimple)
{
    LogRel(("CloudClient::i_isSimpleCommand()\n"));
    RT_NOREF(aCC, fSimple);
    return VERR_NOT_IMPLEMENTED;
}

//int CloudClient::i_findCommandById(const com::Guid &aId, CloudCommandCl &res)
//{
//    HRESULT hrc = S_OK;
//    std::map <com::Guid, CloudCommandCl*>::const_iterator it = mCloudCommandMap.find(aId);
//    if (it !=mCloudCommandMap.end())
//        res = *(it->second);
//    else
//        hrc = setErrorBoth(E_FAIL, VERR_NOT_FOUND, tr("Could not find the command object"));
//    return hrc;
//}

int CloudClient::i_queryAPIForCommand(CloudCommand_T aCommand, CloudAPI** apiObj)
{
    LogRel(("CloudClient::queryAPIForCommand()\n"));
    RT_NOREF(aCommand, apiObj);
    return VERR_NOT_IMPLEMENTED;
}

HRESULT CloudClient::i_getSupportedAPIs(std::list<Utf8Str> &apiNames)
{
    LogRel(("CloudClient::getSupportedAPIs()\n"));
    RT_NOREF(apiNames);
    HRESULT hrc = setErrorBoth(E_FAIL, VERR_NOT_IMPLEMENTED, tr("Not implemented"));
    return hrc;
}

HRESULT CloudClient::i_queryAPI(const Utf8Str &apiName, CloudAPI **apiObj)
{
    LogRel(("CloudClient::queryAPI()\n"));
    RT_NOREF(apiName, apiObj);
    HRESULT hrc = setErrorBoth(E_FAIL, VERR_NOT_IMPLEMENTED, tr("Not implemented"));
    return hrc;
}


int CloudClient::i_queryAPINameForCommand(CloudCommand_T aCommand, Utf8Str &apiName)
{
    LogRel(("CloudClient::i_queryAPINameForCommand()\n"));
    RT_NOREF(aCommand, apiName);
    HRESULT hrc = setErrorBoth(E_FAIL, VERR_NOT_IMPLEMENTED, tr("Not implemented"));
    return hrc;
}

HRESULT CloudClient::i_setUpProgress(ComObjPtr<Progress> &pProgress,
                                     const Utf8Str &strDescription,
                                     ULONG aOperations)
{
    HRESULT hrc;

    /* Create the progress object */
    pProgress.createObject();

    ULONG ulTotalOperationsWeight = 100;
    ULONG ulFirstOperationWeight = ulTotalOperationsWeight/aOperations;
    hrc = pProgress->init(mParent, static_cast<ICloudClient*>(this),
                         Bstr(strDescription).raw(),
                         TRUE /* aCancelable */,
                         aOperations,
                         ulTotalOperationsWeight,
                         Bstr(strDescription).raw(),
                         ulFirstOperationWeight);
    return hrc;
}

////////////////////////////////////////////////////////////////////////////////
//
// CloudClientOCI implementation
//
////////////////////////////////////////////////////////////////////////////////

CloudClientOCI::CloudClientOCI()
{
    LogRel(("CloudClientOCI::CloudClientOCI()\n"));
}

CloudClientOCI::~CloudClientOCI()
{
    LogRel(("CloudClientOCI::~CloudClientOCI()\n"));
}

HRESULT CloudClientOCI::initCloudClient(CloudUserProfiles *aProfiles,
                                     VirtualBox *aParent,
                                     CloudProviderId_T aCloudProvider,
                                     const com::Utf8Str &aProfileName)
{
    HRESULT hrc = S_OK;
    try
    {
        hrc = CloudClient::initCloudClient(aProfiles, aParent, aCloudProvider, aProfileName);
        if (SUCCEEDED(hrc))
        {
            std::list<Utf8Str> apiNames;
            i_getSupportedAPIs(apiNames);
            std::list<Utf8Str>::const_iterator cit = apiNames.begin();
            while (cit!=apiNames.end())
            {
                mCloudAPIInstanceMap[*cit] = NULL;
                ++cit;
            }
        }
    }
    catch (HRESULT arc)
    {
        hrc = arc;
        LogRel(("Get cought an exception %d\n", hrc));
    }

    return hrc;
}

HRESULT CloudClientOCI::getCommandsForOperation(CloudOperation_T aCloudOperation,
                                                BOOL aPrep,
                                                std::vector<CloudCommand_T> &aCloudCommands)
{
    HRESULT hrc = S_OK;
    if (aCloudOperation == CloudOperation_exportVM)
    {
        if (aPrep)
        {
            aCloudCommands.push_back(CloudCommand_listAvailabilityDomains);
            aCloudCommands.push_back(CloudCommand_listShapes);
            aCloudCommands.push_back(CloudCommand_getNamespace);
            aCloudCommands.push_back(CloudCommand_listBuckets);
            aCloudCommands.push_back(CloudCommand_listNetworks);
        }
        else
        {
            aCloudCommands.push_back(CloudCommand_uploadFile);
            aCloudCommands.push_back(CloudCommand_importImage);
            aCloudCommands.push_back(CloudCommand_launchInstance);
        }
    }

    return hrc;
}

int CloudClientOCI::i_isSimpleCommand(const CloudCommandCl &aCC, bool &fSimple)
{
    int vrc = S_OK;
    bool res = false;
    std::vector<com::Utf8Str> paramNames;
    HRESULT hrc = getCommandParameterNames(aCC.mCommand, paramNames);
    if (SUCCEEDED(hrc))
    {
        if (paramNames.empty())
            res = true;
    }
    else
        vrc = VERR_NOT_FOUND;

    fSimple = res;

    return vrc;
}

int CloudClientOCI::i_queryAPIForCommand(CloudCommand_T aCommand, CloudAPI **apiObj)
{
    int vrc = VINF_SUCCESS;
    CloudAPI *newCloudAPI = NULL;

    try
    {
        switch (aCommand)
        {
            case CloudCommand_createInstance:
            case CloudCommand_launchInstance:
            case CloudCommand_pauseInstance:
            case CloudCommand_stopInstance:
            case CloudCommand_deleteInstance:
            case CloudCommand_getInstanceProperties:
            case CloudCommand_listInstances:
            case CloudCommand_listShapes:
            case CloudCommand_listNetworks:
            case CloudCommand_importImage:
                newCloudAPI = new OCICompute();
                break;
            case CloudCommand_uploadFile:
            case CloudCommand_downloadObject:
            case CloudCommand_deleteObject:
            case CloudCommand_getObjectProperties:
            case CloudCommand_listObjects:
            case CloudCommand_listBuckets:
            case CloudCommand_getNamespace:
                newCloudAPI = new OCIObjectStorage();
                break;
            case CloudCommand_listRegions:
            case CloudCommand_listCompartments:
            case CloudCommand_listAvailabilityDomains:
                newCloudAPI = new OCIIAM();
            case CloudCommand_Unknown:
            default:
                break;
        }
    }
    catch (int arc)
    {
        vrc = arc;
        LogRel(("Get cought an exception %d\n", vrc));
    }

    *apiObj = newCloudAPI;
    return vrc;
}

HRESULT CloudClientOCI::i_getSupportedAPIs(std::list<Utf8Str> &apiNames)
{
    HRESULT hrc = S_OK;
    apiNames.push_back("compute");
    apiNames.push_back("object storage");
    apiNames.push_back("iam");
    return hrc;
}

HRESULT CloudClientOCI::i_queryAPI(const Utf8Str &apiName, CloudAPI **apiObj)
{
    HRESULT hrc = S_OK;
    CloudAPI *newCloudAPI = NULL;
    try
    {
        if (apiName == "compute")
             newCloudAPI = new OCICompute();
        else if (apiName == "object storage")
             newCloudAPI = new OCIObjectStorage();
        else if (apiName == "iam")
             newCloudAPI = new OCIIAM();
    }
    catch (HRESULT arc)
    {
        hrc = arc;
        LogRel(("Get cought an exception %d\n", hrc));
    }

    *apiObj = newCloudAPI;
    return hrc;
}

int CloudClientOCI::i_queryAPINameForCommand(CloudCommand_T aCommand, Utf8Str &apiName)
{
    int vrc = VINF_SUCCESS;
    switch (aCommand)
    {
        case CloudCommand_createInstance:
        case CloudCommand_launchInstance:
        case CloudCommand_pauseInstance:
        case CloudCommand_stopInstance:
        case CloudCommand_deleteInstance:
        case CloudCommand_getInstanceProperties:
        case CloudCommand_listInstances:
        case CloudCommand_listShapes:
        case CloudCommand_listNetworks:
        case CloudCommand_importImage:
            apiName = "compute";
            break;
        case CloudCommand_uploadFile:
        case CloudCommand_downloadObject:
        case CloudCommand_deleteObject:
        case CloudCommand_getObjectProperties:
        case CloudCommand_listObjects:
        case CloudCommand_listBuckets:
        case CloudCommand_getNamespace:
            apiName = "object storage";
            break;
        case CloudCommand_listRegions:
        case CloudCommand_listCompartments:
        case CloudCommand_listAvailabilityDomains:
            apiName = "iam";
            break;
        case CloudOperation_Unknown:
        default:
            vrc = VERR_NOT_IMPLEMENTED;
            break;
    }

    return vrc;
}

HRESULT CloudClientOCI::getCommandParameterNames(CloudCommand_T aCloudCommand,
                                                 std::vector<com::Utf8Str> &aParameterNames)
{
    RT_NOREF(aCloudCommand, aParameterNames);
    HRESULT hrc = S_OK;
//  switch (aCloudCommand)
//  {
//      case CloudCommand_createInstance:
//      case CloudCommand_launchInstance:
//          aParameterNames.push_back(paramsNames[0]);
//          aParameterNames.push_back(paramsNames[1]);
//          aParameterNames.push_back(paramsNames[2]);
//          aParameterNames.push_back(paramsNames[3]);
//          aParameterNames.push_back(paramsNames[4]);
//          aParameterNames.push_back(paramsNames[5]);
//          aParameterNames.push_back(paramsNames[6]);
//          break;
//      case CloudCommand_pauseInstance:
//      case CloudCommand_stopInstance:
//      case CloudCommand_deleteInstance:
//      case CloudCommand_getInstanceProperties:
//          aParameterNames.push_back(paramsNames[7]);
//          break;
//      //only compartment Id is needed
//      case CloudCommand_listInstances:
//      case CloudCommand_listRegions:
//      case CloudCommand_listShapes:
//      case CloudCommand_listNetworks:
//      case CloudCommand_listBuckets:
//          break;
//      case CloudCommand_importImage:
//          aParameterNames.push_back(paramsNames[8]);
//          break;
//      case CloudCommand_uploadFile:
//          aParameterNames.push_back(paramsNames[9]);
//          aParameterNames.push_back(paramsNames[3]);
//          break;
//      case CloudCommand_downloadObject:
//      case CloudCommand_deleteObject:
//      case CloudCommand_getObjectProperties:
//          aParameterNames.push_back(paramsNames[10]);
//          aParameterNames.push_back(paramsNames[3]);
//          break;
//      case CloudCommand_listObjects:
//          aParameterNames.push_back(paramsNames[3]);
//          break;
//      case CloudCommand_Unknown:
//      default:
//          hrc = setErrorBoth(E_FAIL, VERR_NOT_FOUND, tr("Unknown cloud command %d"), aCloudCommand);
//          break;
//  }

    return hrc;
}

HRESULT CloudClientOCI::getCommandParameters(CloudCommand_T aCloudCommand, com::Utf8Str &aJsonString)
{
    LogRel(("CloudClientOCI::getOperationParameters: %d\n", aCloudCommand));
    RT_NOREF(aJsonString);
    HRESULT hrc = setErrorBoth(E_FAIL, VERR_NOT_IMPLEMENTED, tr("Not implemented"));
    return hrc;
}

static Utf8Str strExportParametersToOCI = "{\n"
"\t""\"Shape of instance\": {\n"
"\t""\t""\"type\": 27,\n"
"\t""\t""\"items\": [\n"
"\t""\t""\t""\"VM.Standard1.1\",\n"
"\t""\t""\t""\"VM.Standard1.2\",\n"
"\t""\t""\t""\"VM.Standard1.4\",\n"
"\t""\t""\t""\"VM.Standard1.8\",\n"
"\t""\t""\t""\"VM.Standard1.16\"\n"
"\t""\t""]\n"
"\t""},\n"
"\t""\"Availability domain\": {\n"
"\t""\t""\"type\": 28,\n"
"\t""\t""\"items\": [\n"
"\t""\t""\t""\"ergw:US-ASHBURN-AD-1\",\n"
"\t""\t""\t""\"ergw:US-ASHBURN-AD-2\",\n"
"\t""\t""\t""\"ergw:US-ASHBURN-AD-3\"\n"
"\t""\t""]\n"
"\t""},\n"
"\t""\"Disk size\": {\n"
"\t""\t""\"type\": 29,\n"
"\t""\t""\"min\": 10,\n"
"\t""\t""\"max\": 300,\n"
"\t""\t""\"unit\": \"GB\"\n"
"\t""},\n"
"\t""\"Bucket\": {\n"
"\t""\t""\"type\": 30,\n"
"\t""\t""\"items\": [\n"
"\t""\t""\t""\"temporarly-bucket-python-ubuntu-14-x64-server\",\n"
"\t""\t""\t""\"temporarly-bucket-python-un-ubuntu\",\n"
"\t""\t""\t""\"test_cpp_bucket\",\n"
"\t""\t""\t""\"vbox_test_bucket_1\"\n"
"\t""\t""]\n"
"\t""},\n"
"\t""\"Virtual Cloud Network\": {\n"
"\t""\t""\"type\": 31,\n"
"\t""\t""\"items\": [\n"
"\t""\t""\t""\"vcn-scoter\",\n"
"\t""\t""\t""\"vcn20180510154500\",\n"
"\t""\t""\t""\"vcn_spider\"\n"
"\t""\t""]\n"
"\t""},\n"
"\t""\"Public IP address\": {\n"
"\t""\t""\"type\": 32,\n"
"\t""\t""\"bool\": true\n"
"\t""}\n"
"}";

static const char* listAvDomain = "[\n"
"\t{\n"
"\t\t\"compartmentId\": \"ocid1.compartment.oc1..aaaaaaaap4ma65zj4xgaqpg35jrousfazylikmrsvm3lbzfvjwqbwdme3hlq\",\n"
"\t\t\"name\": \"ergw:US-ASHBURN-AD-1\"\n"
"\t},\n"
"\t{\n"
"\t\t\"compartmentId\": \"ocid1.compartment.oc1..aaaaaaaap4ma65zj4xgaqpg35jrousfazylikmrsvm3lbzfvjwqbwdme3hlq\",\n"
"\t\t\"name\": \"ergw:US-ASHBURN-AD-2\"\n"
"\t},\n"
"\t{\n"
"\t\t\"compartmentId\": \"ocid1.compartment.oc1..aaaaaaaap4ma65zj4xgaqpg35jrousfazylikmrsvm3lbzfvjwqbwdme3hlq\",\n"
"\t\t\"name\": \"ergw:US-ASHBURN-AD-3\"\n"
"\t}\n"
"]\n";

static Utf8Str strListAvDomain(listAvDomain);

static const char* listShape = "[\n"
"\t{\n"
"\t\t\"shape\": \"VM.Standard1.1\"\n"
"\t},\n"
"\t{\n"
"\t\t\"shape\": \"VM.Standard1.2\"\n"
"\t},\n"
"\t{\n"
"\t\t\"shape\": \"VM.Standard1.4\"\n"
"\t},\n"
"\t{\n"
"\t\t\"shape\": \"VM.Standard1.8\"\n"
"\t},\n"
"\t{\n"
"\t\t\"shape\": \"VM.Standard1.16\"\n"
"\t}\n"
"]\n";

static Utf8Str strListShape(listShape);

static const char* namespaceName = "{\n"
"\t\"ovm\"\n"
"}\n";

static Utf8Str strNamespaceName(namespaceName);

static const char* listBucket = "[\n"
"\t{\n"
"\t\t\"compartmentId\": \"ocid1.compartment.oc1..aaaaaaaap4ma65zj4xgaqpg35jrousfazylikmrsvm3lbzfvjwqbwdme3hlq\",\n"
"\t\t\"createdBy\": \"ocid1.user.oc1..aaaaaaaa73t54ewmkiti4i4bmsxyjfwgiujhuf4hnhkvipgmmukkgextxujq\",\n"
"\t\t\"definedTags\": null,\n"
"\t\t\"etag\": \"06262c11-a6b3-44a1-9ccd-6d062dcf73cb\",\n"
"\t\t\"freeformTags\": null,\n"
"\t\t\"name\": \"temporarly-bucket-python-ubuntu-14-x64-server\",\n"
"\t\t\"namespace\": \"ovm\",\n"
"\t\t\"timeCreated\": \"2018-05-15T11:23:05.653000+00:00\"\n"
"\t},\n"
"\t{\n"
"\t\t\"compartmentId\": \"ocid1.compartment.oc1..aaaaaaaap4ma65zj4xgaqpg35jrousfazylikmrsvm3lbzfvjwqbwdme3hlq\",\n"
"\t\t\"createdBy\": \"ocid1.user.oc1..aaaaaaaa73t54ewmkiti4i4bmsxyjfwgiujhuf4hnhkvipgmmukkgextxujq\",\n"
"\t\t\"definedTags\": null,\n"
"\t\t\"etag\": \"036be210-3021-4598-b707-c82e5c5cfa08\",\n"
"\t\t\"freeformTags\": null,\n"
"\t\t\"name\": \"temporarly-bucket-python-un-ubuntu\",\n"
"\t\t\"namespace\": \"ovm\",\n"
"\t\t\"timeCreated\": \"2018-05-10T12:30:40.376000+00:00\"\n"
"\t},\n"
"\t{\n"
"\t\t\"compartmentId\": \"ocid1.compartment.oc1..aaaaaaaap4ma65zj4xgaqpg35jrousfazylikmrsvm3lbzfvjwqbwdme3hlq\",\n"
"\t\t\"createdBy\": \"ocid1.user.oc1..aaaaaaaas2vfq7jfhct4dto4uk5myhripw5cdvnz4ddgu44buv4fcxgafhhq\",\n"
"\t\t\"definedTags\": null,\n"
"\t\t\"etag\": \"b7c9f373-36be-4228-b505-0a46f07b4c41\",\n"
"\t\t\"freeformTags\": null,\n"
"\t\t\"name\": \"test_cpp_bucket\",\n"
"\t\t\"namespace\": \"ovm\",\n"
"\t\t\"timeCreated\": \"2018-07-18T15:04:31.285000+00:00\"\n"
"\t},\n"
"\t{\n"
"\t\t\"compartmentId\": \"ocid1.compartment.oc1..aaaaaaaap4ma65zj4xgaqpg35jrousfazylikmrsvm3lbzfvjwqbwdme3hlq\",\n"
"\t\t\"createdBy\": \"ocid1.user.oc1..aaaaaaaat47uhcg5ucbznrknalr2ta2s35flxbpfwly74rzp2ivj3fd4sy3a\",\n"
"\t\t\"definedTags\": null,\n"
"\t\t\"etag\": \"5a4477d9-8fc4-401f-994e-0e940b1dfb84\",\n"
"\t\t\"freeformTags\": null,\n"
"\t\t\"name\": \"vbox_test_bucket_1\",\n"
"\t\t\"namespace\": \"ovm\",\n"
"\t\t\"timeCreated\": \"2018-05-04T17:56:20.222000+00:00\"\n"
"\t}\n"
"]\n";

static Utf8Str strListBucket(listBucket);

static const char* listVcn = "[\n"
"\t{\n"
"\t\t\"cidr-block\": \"192.168.0.0/24\",\n"
"\t\t\"compartment-id\": \"ocid1.compartment.oc1..aaaaaaaap4ma65zj4xgaqpg35jrousfazylikmrsvm3lbzfvjwqbwdme3hlq\",\n"
"\t\t\"default-dhcp-options-id\": \"ocid1.dhcpoptions.oc1.iad.aaaaaaaadubfpdssid2f3iewwnq2e2iudguqx4vnn5uivlvwp3uuoydxgq4q\",\n"
"\t\t\"default-route-table-id\": \"ocid1.routetable.oc1.iad.aaaaaaaajiksnjaqmrzqy2dfiijyxny4qnk65hyfr72cuulaeteis363w5lq\",\n"
"\t\t\"default-security-list-id\": \"ocid1.securitylist.oc1.iad.aaaaaaaagzuwh3lxnbhmnl6wtuycynwoxoekl62fttg4tqvwh7apfbuedixq\",\n"
"\t\t\"defined-tags\": {},\n"
"\t\t\"display-name\": \"vcn-scoter\",\n"
"\t\t\"dns-label\": \"vcnscoter\",\n"
"\t\t\"freeform-tags\": {},\n"
"\t\t\"id\": \"ocid1.vcn.oc1.iad.aaaaaaaayytm3gse3cr7wqq56zjatcak2wk4prb26sh3s3egkz2azknc5d6q\",\n"
"\t\t\"lifecycle-state\": \"AVAILABLE\",\n"
"\t\t\"time-created\": \"2018-06-14T16:29:52.788000+00:00\",\n"
"\t\t\"vcn-domain-name\": \"vcnscoter.oraclevcn.com\"\n"
"\t},\n"
"\t{\n"
"\t\t\"cidr-block\": \"10.8.0.0/16\",\n"
"\t\t\"compartment-id\": \"ocid1.compartment.oc1..aaaaaaaap4ma65zj4xgaqpg35jrousfazylikmrsvm3lbzfvjwqbwdme3hlq\",\n"
"\t\t\"default-dhcp-options-id\": \"ocid1.dhcpoptions.oc1.iad.aaaaaaaasqjcnaqo2f7wgovvnnajgrcfinnoifqt3wc2xc5rdd76yqn3pp3q\",\n"
"\t\t\"default-route-table-id\": \"ocid1.routetable.oc1.iad.aaaaaaaavzwgctg35rfqr44st5d7endapdyelnndbo6nul63i4qec7ptdjna\",\n"
"\t\t\"default-security-list-id\": \"ocid1.securitylist.oc1.iad.aaaaaaaasb5al7waxkyb4rmuky3qwym2sb4ctpphqzxbigh6e2rkfj5zgxqq\",\n"
"\t\t\"defined-tags\": {},\n"
"\t\t\"display-name\": \"vcn_spider\",\n"
"\t\t\"dns-label\": \"vcnspider\",\n"
"\t\t\"freeform-tags\": {},\n"
"\t\t\"id\": \"ocid1.vcn.oc1.iad.aaaaaaaa4mhghya6ztii7par7v2o3v2il5nbbbx32benobjv3wlcs2ekpyaq\",\n"
"\t\t\"lifecycle-state\": \"AVAILABLE\",\n"
"\t\t\"time-created\": \"2018-06-07T14:46:10.855000+00:00\",\n"
"\t\t\"vcn-domain-name\": \"vcnspider.oraclevcn.com\"\n"
"\t},\n"
"\t{\n"
"\t\t\"cidr-block\": \"10.0.0.0/16\",\n"
"\t\t\"compartment-id\": \"ocid1.compartment.oc1..aaaaaaaap4ma65zj4xgaqpg35jrousfazylikmrsvm3lbzfvjwqbwdme3hlq\",\n"
"\t\t\"default-dhcp-options-id\": \"ocid1.dhcpoptions.oc1.iad.aaaaaaaao5tkhev2vqvpyzrpvzwiqbxz6w4kolcfdoo57xxktehv3qpv6hza\",\n"
"\t\t\"default-route-table-id\": \"ocid1.routetable.oc1.iad.aaaaaaaantab6kld6b2wcxgv5rs5abmg4tmd5ok274emejfnofl744pignva\",\n"
"\t\t\"default-security-list-id\": \"ocid1.securitylist.oc1.iad.aaaaaaaashi4gkt35wyfwozh5vsbxi4e76leca27xkgun42n4aorgg5qtfuq\",\n"
"\t\t\"defined-tags\": {},\n"
"\t\t\"display-name\": \"vcn20180510154500\",\n"
"\t\t\"dns-label\": \"vcn0510154459\",\n"
"\t\t\"freeform-tags\": {},\n"
"\t\t\"id\": \"ocid1.vcn.oc1.iad.aaaaaaaaaxetjzokkuahch46aglb4peltzto67tihc73mmako6o2ct4yk75a\",\n"
"\t\t\"lifecycle-state\": \"AVAILABLE\",\n"
"\t\t\"time-created\": \"2018-05-10T15:45:00.579000+00:00\",\n"
"\t\t\"vcn-domain-name\": \"vcn0510154459.oraclevcn.com\"\n"
"\t}\n"
"]\n";

static Utf8Str strListVcn(listVcn);

//HRESULT CloudClientOCI::getOperationParameters(CloudOperation_T aCloudOperation, com::Utf8Str &aJsonString)
//{
//    LogRel(("CloudClientOCI::getOperationParameters: %d\n", aCloudOperation));
//    HRESULT hrc = S_OK;
//    if (aCloudOperation == CloudOperation_exportVM)
//        aJsonString = strExportParametersToOCI;
//    else
//        hrc = setErrorBoth(E_FAIL, VERR_NOT_IMPLEMENTED, tr("Not implemented"));
//    return hrc;
//}

HRESULT CloudClientOCI::getOperationParameters(CloudOperation_T aCloudOperation,
                                               com::Utf8Str &aJsonString)
//                                             ComPtr<IProgress> &aProgress)
{
    LogRel(("CloudClientOCI::getOperationParameters: %d\n", aCloudOperation));
    RT_NOREF(aJsonString);
    HRESULT hrc = S_OK;
    int vrc = VINF_SUCCESS;

    std::vector<CloudCommand_T> commandList;
    hrc = getCommandsForOperation(aCloudOperation, true, commandList);
    if (FAILED(hrc))
        return setErrorBoth(E_FAIL, VERR_NOT_FOUND, tr("Commands not found"));

    com::Guid firstCommandId;
    std::vector<com::Guid> commandIDs;
    for (size_t i=0;i<commandList.size();++i)
    {
        com::Guid cId;
        vrc = i_createCommand(commandList[i], cId);
        if (RT_SUCCESS(vrc))
        {

            //remember first command Id for getting the command description for Progress
            if (i==0)
                firstCommandId = cId;

            commandIDs.push_back(cId);
            LogRel(("Command %d have got UUID %s\n", commandList[i], cId.toString().c_str()));
        }
        else
        {
            setErrorBoth(E_FAIL, vrc, tr("Creation of command %d failed."), commandList[i]);
            break;
        }
    }

    if (FAILED(hrc))
        return hrc;

    std::map<Utf8Str, Utf8Str> paramNameValueMap;

    //Setup Progress object here
    ComObjPtr<Progress> progress;
    ComPtr<IProgress> aProgress;
    {
        CloudCommandCl *cc = NULL;
        vrc = i_getCommandById(firstCommandId, &cc);
        if (RT_SUCCESS(vrc))
        {
            paramNameValueMap = cc->mParameters;
            Utf8Str strDesc = cc->getDescription();
            hrc = i_setUpProgress(progress, strDesc, (ULONG)commandList.size());
            if (SUCCEEDED(hrc))
                /* Return progress to the caller */
                progress.queryInterfaceTo(aProgress.asOutParam());
        }
        else
            hrc = setErrorBoth(E_FAIL, VERR_NOT_FOUND, tr("Command with Id %s not found"), firstCommandId.toString().c_str());
    }

    if (FAILED(hrc))
        return hrc;

    //stores the last command successful response
    std::vector<DataModel*> dm;
    std::map<Utf8Str, Utf8Str> summaryInfo;
    com::Guid lastCommandId;

    /*
    * STRONG prerequisity: First command MUST BE validated always.
    * It means that all needed parameters must be correct or
    * the command hasn't any parameters.
    */
    bool fWeakValidation = false;//validation has two tries, but for the first command there is only one try

    for (size_t i=0;i<commandIDs.size();++i)
    {
        CloudCommandCl *cc = NULL;
        vrc = i_getCommandById(commandIDs.at(i), &cc);
        if (RT_SUCCESS(vrc) && cc!=NULL)
        {
            vrc = i_validateCommand(*cc, paramNameValueMap, fWeakValidation);
            if (RT_SUCCESS(vrc) && vrc != VINF_TRY_AGAIN)
            {
//              vrc = i_runCommand(*cc, progress);//<---Progress must be passed here
                vrc = i_runCommand(*cc);

                CloudCommandResult_T commResult = CloudCommandResult_success;
                if (RT_SUCCESS(vrc))
                {
                    if (true)
                    {
                        switch (cc->mCommand)
                        {
                            case CloudCommand_getNamespace:
                                cc->mResponse.mRaw = strNamespaceName;
                                break;
                            case CloudCommand_listAvailabilityDomains:
                                cc->mResponse.mRaw = strListAvDomain;
                                break;
                            case CloudCommand_listNetworks:
                                cc->mResponse.mRaw = strListVcn;
                                break;
                            case CloudCommand_listShapes:
                                cc->mResponse.mRaw = strListShape;
                                break;
                            case CloudCommand_listBuckets:
                                cc->mResponse.mRaw = strListBucket;
                                break;
                            default:
                                break;
                        }
                    }
//                  vrc = i_checkCommandResult(*cc, commResult);
//                  if (RT_SUCCESS(vrc))
                    {
                        vrc = i_parseResponse(*cc);
//                      dm = cc->getData();
//                      summaryInfo = cc->getSummary();
                        lastCommandId = commandIDs.at(i);//remember the last command Id
                        fWeakValidation = true;//restore weak validation for the next command
                    }
                }
                else
                {
                    hrc = setError(E_FAIL, tr("The call of command %d was unsuccessful"), cc->mCommand);
                }
                LogRel(("Command %d result is %d\n", commandList[i], commResult));
            }
            else
            {
                if (RT_FAILURE(vrc))
                {
                    hrc = setErrorBoth(E_FAIL, vrc, tr("Command %d wasn't validated"), commandList[i]);
                    break;
                }

                if (vrc == VINF_TRY_AGAIN)
                {
                    LogRel(("Command %d hasn't been validated yet.\n", commandList[i]));
                    //try to find and extract needed parameters from the last executed command response
                    //convert summaryInfo into paramNameValueMap (extract from summaryInfo and convert it)
                    std::vector<com::Utf8Str> paramNames;

                    vrc = i_getCommandParameterNames(*cc, paramNames);
                    if (RT_SUCCESS(vrc))
                    {
                        CloudCommandCl *lastCC = NULL;
                        if (lastCommandId.isValid())
                        {
                            vrc = i_getCommandById(lastCommandId, &lastCC);
                            if (RT_SUCCESS(vrc) && lastCC!=NULL)
                            {
                                for (size_t j=0;j<paramNames.size();++j)
                                {
                                    Utf8Str strValue;
                                    vrc = lastCC->findParameter(paramNames.at(j), strValue);
                                    if (RT_SUCCESS(vrc))
                                        paramNameValueMap.insert(make_pair(paramNames.at(j), strValue));
                                    else
                                    {
                                        hrc = setErrorBoth(E_FAIL, vrc,
                                                           tr("Command %d wasn't validated"),
                                                           commandList[j]);
                                        break;
                                    }
                                }
                            }
                            else
                                hrc = setErrorBoth(E_FAIL, vrc,
                                                   tr("Couldn't find command with Id \"%s\""),
                                                   lastCommandId.toString().c_str());
                        }
                        else
                        {
                            vrc = VERR_INVALID_UUID_FORMAT;
                            hrc = setErrorBoth(E_FAIL, vrc, tr("Invalid UUID for the command %d "), commandList[i]);
                        }
                    }
                    else
                        hrc = setErrorBoth(E_FAIL, vrc, tr("Couldn't find command parameter names"));

                    if (RT_FAILURE(vrc))
                    {
                        LogRel(("Command %d hasn't been validated twice. Can't continue.\n", commandList[i]));
                        break;
                    }

                    /*
                    * Set validation to "Strong"
                    * and try to validate the command one more time
                    */
                    {
                        fWeakValidation = false;
                        --i;
                    }
                }
            }
        }
    }

    return hrc;
}

////////////////////////////////////////////////////////////////////////////////
//
// CloudClientGCP implementation
//
////////////////////////////////////////////////////////////////////////////////
//HRESULT CloudClientGCP::i_getSupportedAPIs(std::list<Utf8Str> &apiNames)
//{
//    HRESULT hrc = S_OK;
//    apiNames.push_back("compute");
//    apiNames.push_back("storage");
//    return hrc;
//}
//
//HRESULT CloudClientGCP::i_queryAPI(const Utf8Str &apiName, CloudAPI* apiObj)
//{
//    HRESULT hrc = S_OK;
//    try
//    {
//        CloudAPI *newCloudAPI = NULL;
//        if (APIName == "compute")
//             newCloudAPI = new GCPCompute();
//        else if (APIName == "storage")
//             newCloudAPI = new GCPStorage();
//        apiObj = newCloudAPI;
//    }
//    catch (HRESULT arc)
//    {
//        hrc = arc;
//        LogRel(("Get cought an exception %d\n", hrc));
//    }
//
//    return hrc;
//}
//
//HRESULT CloudClientGCP::i_queryAPINameForCommand(CloudCommand_T aCommand, Utf8Str &apiName)
//{
//    LogRel(("CloudClientGCP::queryAPINameForCommand: %d, %s\n", aCommand, apiName.c_str()));
//    HRESULT hrc = setErrorBoth(E_FAIL, VERR_NOT_IMPLEMENTED, tr("Not implemented"));
//    return hrc;
//}

