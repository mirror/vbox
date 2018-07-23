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


#ifndef ____H_CLOUDUSERPROFILESIMPL
#define ____H_CLOUDUSERPROFILESIMPL

//#include <map>
#include <vector>

/* VBox includes */
#include "CloudClientImpl.h"
#include "CloudUserProfilesWrap.h"
#include "UnattendedScript.h"

/* VBox forward declarations */
class SimpleConfigFile;

class CloudUserProfiles : public CloudUserProfilesWrap
{
public:
    CloudUserProfiles();
    virtual ~CloudUserProfiles();
    HRESULT FinalConstruct();
    void FinalRelease();
    HRESULT init(VirtualBox *aVirtualBox);
    void uninit();

protected:
    ComPtr<VirtualBox> const mParent;       /**< Strong reference to the parent object (VirtualBox/IMachine). */

public:
    HRESULT getProvider(CloudProviderId_T *aProvider);
    HRESULT createCloudClient(const com::Utf8Str &aProfileName,
                             ComPtr<ICloudClient> &aCloudClient);
    HRESULT createProfile(const com::Utf8Str &aProfileName,
                          const std::vector<com::Utf8Str> &aNames,
                          const std::vector<com::Utf8Str> &aValues);
    HRESULT updateProfile(const com::Utf8Str &aProfileName,
                          const std::vector<com::Utf8Str> &aNames,
                          const std::vector<com::Utf8Str> &aValues);
    HRESULT getStoredProfilesNames(std::vector<com::Utf8Str> &aProfilesNames);
    HRESULT getProfileProperties(const com::Utf8Str &aProfileName,
                                 std::vector<com::Utf8Str> &aReturnNames,
                                 std::vector<com::Utf8Str> &aReturnValues);
    HRESULT getPropertyDescription(const com::Utf8Str &aName,
                                   com::Utf8Str &aDescription);

    virtual HRESULT getSupportedPropertiesNames(std::vector<com::Utf8Str> &aPropertiesNames);
    virtual HRESULT readProfiles(const Utf8Str &strConfigPath);

};


class OCIUserProfiles :
    public CloudUserProfiles
{
public:
    OCIUserProfiles();
    ~OCIUserProfiles();

    HRESULT getProvider(CloudProviderId_T *aProvider)
    {
        *aProvider = CloudProviderId_OCI;
        return S_OK;
    }

    HRESULT createCloudClient(const com::Utf8Str &aProfileName,
                             ComPtr<ICloudClient> &aCloudClient);
    HRESULT createProfile(const com::Utf8Str &aProfileName,
                          const std::vector<com::Utf8Str> &aNames,
                          const std::vector<com::Utf8Str> &aValues);
    HRESULT updateProfile(const com::Utf8Str &aProfileName,
                          const std::vector<com::Utf8Str> &aNames,
                          const std::vector<com::Utf8Str> &aValues);
    HRESULT getStoredProfilesNames(std::vector<com::Utf8Str> &aProfilesNames);
    HRESULT getProfileProperties(const com::Utf8Str &aProfileName,
                                 std::vector<com::Utf8Str> &aReturnNames,
                                 std::vector<com::Utf8Str> &aReturnValues);
    HRESULT getPropertyDescription(const com::Utf8Str &aName,
                                   com::Utf8Str &aDescription);
    HRESULT getSupportedPropertiesNames(std::vector<com::Utf8Str> &aPropertiesNames);

    HRESULT readProfiles(const Utf8Str &strConfigPath);

private:
    HRESULT i_createProfile(const com::Utf8Str &aProfileName,
                          const std::map <Utf8Str, Utf8Str> &aProfile);
    HRESULT i_updateProfile(const com::Utf8Str &aProfileName,
                          const std::map <Utf8Str, Utf8Str> &aProfile);
    HRESULT i_getProfileProperties(const com::Utf8Str &aName,
                                 std::map <Utf8Str, Utf8Str> &aProfileName);
private:
    Utf8Str mStrConfigPath;
    SimpleConfigFile *mpProfiles;
};


class SimpleConfigFile : public GeneralTextScript
{
protected:
    std::map < Utf8Str, std::map <Utf8Str, Utf8Str> > mSections;

public:
    explicit SimpleConfigFile(VirtualBoxBase *pSetError, const char *pszFilename = NULL);
    virtual ~SimpleConfigFile();

    HRESULT parse();
    //////////////////New functions//////////////////////////////
    HRESULT addSection(const Utf8Str &aSectionName, const std::map <Utf8Str, Utf8Str>& section);

    size_t getNumberOfSections() const
    {
        return mSections.size();
    }

    std::vector <Utf8Str> getSectionsNames() const;
    std::map <Utf8Str, Utf8Str> getSectionByName (const Utf8Str &strSectionName) const;
    bool isSectionExist(const Utf8Str &strSectionName) const;
    HRESULT updateSection (const Utf8Str &strSectionName,
                           const std::map <Utf8Str, Utf8Str> &newSection);
    //////////////////New functions//////////////////////////////
};
#endif // !____H_CLOUDUSERPROFILESIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */

