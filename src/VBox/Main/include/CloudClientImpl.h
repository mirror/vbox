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
#include "ProgressImpl.h"

/* VBox forward declarations */
class CloudUserProfiles;
class CloudCommandCl;
class CloudAPI;
struct DataModel;

////////////////////////////////////////////////////////////////////////////////
//
// CloudClient 
//
// /////////////////////////////////////////////////////////////////////////////
class CloudClient:
    public CloudClientWrap
{
public:
    DECLARE_EMPTY_CTOR_DTOR(CloudClient)

    HRESULT FinalConstruct();
    void FinalRelease();

    HRESULT init(VirtualBox *aVirtualBox);
    HRESULT initCloudClient(CloudUserProfiles *aProfiles,
                            VirtualBox *aParent,
                            CloudProviderId_T aCloudProvider,
                            const com::Utf8Str &aProfileName);
    void uninit();

protected:
    ComPtr<VirtualBox> const mParent;       /**< Strong reference to the parent object (VirtualBox/IMachine). */
    CloudProviderId_T mCloudProvider;
    std::map <Utf8Str, Utf8Str> mCloudProfile;
    std::map <com::Guid, CloudCommandCl*> mCloudCommandMap;
    std::map <Utf8Str, CloudAPI*> mCloudAPIInstanceMap;

private:
    HRESULT createCommand(CloudCommand_T aCloudCommand, com::Guid &aId);
    HRESULT runCommand(const com::Guid &aId, LONG64 aTimeout, CloudCommandResult_T *aResult);
//  HRESULT runCommand(const com::Guid &aId, LONG64 aTimeout, CloudCommandResult_T *aResult, ComPtr<IProgress> &aProgress);
    HRESULT validateCommand(const com::Guid &aId, BOOL *aValid);
    HRESULT checkCommandResult(const com::Guid &aId,
                               LONG64 *aStartTime,
                               LONG64 *aLastTime,
                               CloudCommandResult_T *aResult);

    HRESULT getCommandParameterNames(CloudCommand_T aCloudCommand,
                                     std::vector<com::Utf8Str> &aParameterNames);
    HRESULT getCommandParameterProperties(const com::Utf8Str &aParameterName,
                                          com::Utf8Str &aParameterType,
                                          com::Utf8Str &aParameterDesc,
                                          std::vector<com::Utf8Str> &aParameterValues);
    HRESULT getCommandParameters(CloudCommand_T aCloudCommand, com::Utf8Str &aJsonString);
    HRESULT setCommandParameters(const com::Guid &aId,
                                 const std::vector<com::Utf8Str> &aNames,
                                 const std::vector<com::Utf8Str> &aValues);
    HRESULT getCommandsForOperation(CloudOperation_T aCloudOperation,
                                    BOOL aPrep,
                                    std::vector<CloudCommand_T> &aCloudCommands);
    HRESULT getOperationParameters(CloudOperation_T aCloudOperation, com::Utf8Str &aJsonString);

protected:
    //Next 4 functions are wrappers for the corresponding private functions without prefix "i_"
    int i_createCommand(CloudCommand_T aCloudCommand, com::Guid &aId);
    int i_runCommand(CloudCommandCl &aCC);
//  int i_runCommand(CloudCommandCl &aCC, ComObjPtr<Progress> *aProgress);
    int i_validateCommand(CloudCommandCl &aCC, 
                          const std::map<Utf8Str, Utf8Str> &aParamNameValueMap,
                          bool fWeak);
    int i_checkCommandResult(CloudCommandCl &aCC, CloudCommandResult_T &aResult);
    //End 4 wrappers

    int i_getCommandById(const com::Guid &aId, CloudCommandCl **aCC);
    int i_parseResponse(CloudCommandCl &aCC);
    virtual int i_getCommandParameterNames(const CloudCommandCl &aCC,
                                           std::vector<com::Utf8Str> &aParameterNames);
    virtual int i_isSimpleCommand(const CloudCommandCl &aCC, bool &fSimple);
//  virtual int i_findCommandById(const com::Guid &aId, CloudCommandCl &res);
    virtual int i_queryAPIForCommand(CloudCommand_T aCommand, CloudAPI **apiObj);
    virtual HRESULT i_getSupportedAPIs(std::list<Utf8Str> &apiNames);
    virtual HRESULT i_queryAPI(const Utf8Str &apiName, CloudAPI **apiObj);
    virtual int i_queryAPINameForCommand(CloudCommand_T aCommand, Utf8Str &apiName);
    HRESULT i_setUpProgress(ComObjPtr<Progress> &pProgress,
                            const Utf8Str &strDescription,
                            ULONG aOperations);
};

////////////////////////////////////////////////////////////////////////////////
//
// CloudCLientOCI 
//
// /////////////////////////////////////////////////////////////////////////////
class CloudClientOCI : public CloudClient
{
public:
    CloudClientOCI();
    ~CloudClientOCI();
    HRESULT initCloudClient(CloudUserProfiles *aProfiles,
                            VirtualBox *aParent,
                            CloudProviderId_T aCloudProvider,
                            const com::Utf8Str &aProfileName);
private:
    HRESULT getCommandsForOperation(CloudOperation_T aCloudOperation,
                                    BOOL aPrep,
                                    std::vector<CloudCommand_T> &aCloudCommands);
    HRESULT getCommandParameters(CloudCommand_T aCommand, com::Utf8Str &aJsonString);
    HRESULT getOperationParameters(CloudOperation_T aCloudOperation, com::Utf8Str &aJsonString);
    HRESULT getCommandParameterNames(CloudCommand_T aCloudCommand, std::vector<com::Utf8Str> &aParameterNames);

protected:

    int i_isSimpleCommand(const CloudCommandCl &aCC, bool &fSimple);
    int i_queryAPIForCommand(CloudCommand_T aCommand, CloudAPI **apiObj);
    HRESULT i_getSupportedAPIs(std::list<Utf8Str> &apiNames);
    HRESULT i_queryAPI(const Utf8Str &apiName, CloudAPI **apiObj);
    int i_queryAPINameForCommand(CloudCommand_T aCommand, Utf8Str &apiName);
    int i_extractAskedParamsFromResponse(CloudCommandCl &aCC, std::map<Utf8Str, Utf8Str> &aParams);
};

////////////////////////////////////////////////////////////////////////////////
//
// CloudCLientGCP 
//
// /////////////////////////////////////////////////////////////////////////////
//CloudCLientGCP : public CloudClient
//{
//    CloudCLientGCP();
//    ~CloudCLientGCP();
//};

#endif // !____H_CLOUDCLIENTIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */

