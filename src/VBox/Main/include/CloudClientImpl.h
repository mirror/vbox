/* $Id$ */
/** @file
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


#ifndef ____H_CLOUDCLIENTIMPL
#define ____H_CLOUDCLIENTIMPL

/* VBox includes */

#include "CloudClientWrap.h"

/* VBox forward declarations */
class CloudUserProfileList;

class CloudClient :
    public CloudClientWrap
{
public:
    DECLARE_EMPTY_CTOR_DTOR(CloudClient)
//  explicit CloudClient(CloudProviderId_T aCloudProvider);

    HRESULT FinalConstruct();
    void FinalRelease();

    HRESULT init(VirtualBox *aVirtualBox);
    HRESULT initCloudClient(CloudUserProfileList *aProfiles,
                            VirtualBox *aParent,
                            CloudProviderId_T aCloudProvider,
                            const com::Utf8Str &aProfileName);
    void uninit();

protected:
    ComPtr<VirtualBox> const mParent;       /**< Strong reference to the parent object (VirtualBox/IMachine). */
    CloudProviderId_T mCloudProvider;
    std::map <Utf8Str, Utf8Str> userProfile;

private:
    HRESULT getOperationParameters(CloudOperation_T aCloudOperation, com::Utf8Str &aJsonString);

    HRESULT createOperation(const com::Utf8Str &aProfileName,
                            CloudOperation_T aCloudOperation,
                            com::Guid &aOpId);
    HRESULT runOperation(const com::Guid &aOpId,
                         LONG64 aTimeout);
    HRESULT checkOperationResult(const com::Guid &aOpId,
                                 LONG64 *aStartOpTime,
                                 LONG64 *aLastTime,
                                 CloudOperationResult_T *aResult);
    HRESULT getOperationParameterNames(CloudOperation_T aCloudOperation,
                                       std::vector<com::Utf8Str> &aParameterNames);
    HRESULT getOperationParameterProperties(const com::Utf8Str &aOpParameterName,
                                            com::Utf8Str &aOpParameterType,
                                            com::Utf8Str &aOpParameterDesc,
                                            std::vector<com::Utf8Str> &aOpParameterValues);
    HRESULT setParametersForOperation(const com::Guid &aOpId,
                                      const std::vector<com::Utf8Str> &aValues);
};

class CloudClientOCI :
    public CloudClient
{
public:
    DECLARE_EMPTY_CTOR_DTOR(CloudClientOCI)

    HRESULT initCloudClient(CloudUserProfileList *aProfiles,
                            VirtualBox *aParent,
                            CloudProviderId_T aCloudProvider,
                            const com::Utf8Str &aProfileName);

private:
    HRESULT getOperationParameters(CloudOperation_T aCloudOperation, com::Utf8Str &aJsonString);

    HRESULT createOperation(const com::Utf8Str &aProfileName,
                            CloudOperation_T aCloudOperation,
                            com::Guid &aOpId);
    HRESULT runOperation(const com::Guid &aOpId,
                         LONG64 aTimeout);
    HRESULT checkOperationResult(const com::Guid &aOpId,
                                 LONG64 *aStartOpTime,
                                 LONG64 *aLastTime,
                                 CloudOperationResult_T *aResult);
    HRESULT getOperationParameterNames(CloudOperation_T aCloudOperation,
                                       std::vector<com::Utf8Str> &aParameterNames);
    HRESULT getOperationParameterProperties(const com::Utf8Str &aOpParameterName,
                                            com::Utf8Str &aOpParameterType,
                                            com::Utf8Str &aOpParameterDesc,
                                            std::vector<com::Utf8Str> &aOpParameterValues);
    HRESULT setParametersForOperation(const com::Guid &aOpId,
                                      const std::vector<com::Utf8Str> &aValues);
};

#endif // !____H_CLOUDCLIENTIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */

