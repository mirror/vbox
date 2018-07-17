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


#include <iprt/path.h>
#include <iprt/cpp/utils.h>
#include <VBox/com/array.h>
#include <map>

#include "CloudClientImpl.h"
#include "CloudUserProfileManagerImpl.h"
#include "CloudUserProfileListImpl.h"
#include "VirtualBoxImpl.h"
#include "Global.h"
#include "ProgressImpl.h"
#include "MachineImpl.h"
#include "AutoCaller.h"
#include "Logging.h"

using namespace std;


////////////////////////////////////////////////////////////////////////////////
//
// CloudClient constructor / destructor
//
// ////////////////////////////////////////////////////////////////////////////////
CloudClient::CloudClient()
    : mParent(NULL)
{
    LogRel(("CloudClient::CloudClient()\n"));
}

CloudClient::~CloudClient()
{
    LogRel(("CloudClient::~CloudClient()\n"));
}

//CloudClient::CloudClient(CloudProviderId_T aCloudProvider)
//    : mParent(NULL)
//{
//    LogRel(("CloudClient::CloudClient(CloudProviderId_T %d)\n", mCloudProvider));
//    mCloudProvider = aCloudProvider;
//}

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

HRESULT CloudClient::initCloudClient(CloudUserProfileList *aProfiles,
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
        CloudUserProfileList *lProfiles = aProfiles;
        std::vector<com::Utf8Str> lNames;
        std::vector<com::Utf8Str> lValues;
        hrc = lProfiles->getProfileProperties(aProfileName, lNames, lValues);
        if (FAILED(hrc))
        {
            throw hrc;
        }

        for (size_t i=0;i<lNames.size();++i)
        {
            com::Utf8Str value = (i<lValues.size()) ? lValues.at(i) : "";
            userProfile[lNames.at(i)] = value;
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
    catch (std::bad_alloc)
    {
        return E_OUTOFMEMORY;
    }

    /*
     * Confirm a successful initialization
     */
    autoInitSpan.setSucceeded();

    return hrc;
}

HRESULT CloudClient::getOperationParameters(CloudOperation_T aCloudOperation, com::Utf8Str &aJsonString)
{
    LogRel(("CloudClient::getOperationParameters: %d, %s\n", aCloudOperation, aJsonString.c_str()));
    HRESULT hrc = VERR_NOT_IMPLEMENTED;

    return hrc;
}

HRESULT CloudClient::createOperation(const com::Utf8Str &aProfileName,
                                     CloudOperation_T aCloudOperation,
                                     com::Guid &aOpId)
{
    LogRel(("CloudClient::createOperation: %s, %d, %s\n", aProfileName.c_str(),
            aCloudOperation,
            aOpId.toString().c_str()));
    HRESULT hrc = VERR_NOT_IMPLEMENTED;
    return hrc;
}

HRESULT CloudClient::runOperation(const com::Guid &aOpId,
                                  LONG64 aTimeout)
{
    LogRel(("CloudClient::runOperation: %s, %d\n", aOpId.toString().c_str(), aTimeout));
    HRESULT hrc = VERR_NOT_IMPLEMENTED;
    return hrc;
}

HRESULT CloudClient::checkOperationResult(const com::Guid &aOpId,
                                          LONG64 *aStartOpTime,
                                          LONG64 *aLastTime,
                                          CloudOperationResult_T *aResult)
{
    LogRel(("CloudClient::checkOperationResult: %s, %d, %d, %d\n",
            aOpId.toString().c_str(),
            *aStartOpTime,
            *aLastTime,
            *aResult));
    HRESULT hrc = VERR_NOT_IMPLEMENTED;
    return hrc;
}

HRESULT CloudClient::getOperationParameterNames(CloudOperation_T aCloudOperation,
                                                std::vector<com::Utf8Str> &aParameterNames)
{
    LogRel(("CloudClient::getOperationParameterNames: %d\n", aCloudOperation));
    aParameterNames.clear();
    HRESULT hrc = VERR_NOT_IMPLEMENTED;
    return hrc;
}

HRESULT CloudClient::getOperationParameterProperties(const com::Utf8Str &aOpParameterName,
                                                     com::Utf8Str &aOpParameterType,
                                                     com::Utf8Str &aOpParameterDesc,
                                                     std::vector<com::Utf8Str> &aOpParameterValues)
{
    LogRel(("CloudClient::getOperationParameterProperties: %s, %s, %s\n",
            aOpParameterName.c_str(),
            aOpParameterType.c_str(),
            aOpParameterDesc.c_str()));
    aOpParameterValues.clear();
    HRESULT hrc = VERR_NOT_IMPLEMENTED;
    return hrc;
}

HRESULT CloudClient::setParametersForOperation(const com::Guid &aOpId,
                                               const std::vector<com::Utf8Str> &aValues)
{
    LogRel(("CloudClient::setParametersForOperation: %s\n", aOpId.toString().c_str()));
    for (size_t i=0;i<aValues.size();++i)
    {
        LogRel(("value %d: %s\n", i, aValues.at(i).c_str()));
    }

    HRESULT hrc = VERR_NOT_IMPLEMENTED;
    return hrc;
}
