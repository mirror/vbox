/* $Id$ */
/** @file
 * ICloudUserProfileList COM class implementations.
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
// SimpleConfigFile implementation
//
////////////////////////////////////////////////////////////////////////////////
SimpleConfigFile::SimpleConfigFile(VirtualBoxBase *pSetError, const char *pszFilename)
    : GeneralTextScript(pSetError, pszFilename)
{
    LogRel(("SimpleConfigFile::SimpleConfigFile(VirtualBoxBase *pSetError,...)\n"));
}

SimpleConfigFile::~SimpleConfigFile()
{
    LogRel(("SimpleConfigFile::~SimpleConfigFile()\n"));
}

HRESULT SimpleConfigFile::parse()
{
    LogRel(("Starting to parse the existing profiles\n"));
    HRESULT hrc = S_OK;
    hrc = GeneralTextScript::parse();
    if (SUCCEEDED(hrc))
    {
        size_t lines = getLineNumbersOfScript();
        LogRel(("Initial parsing (GeneralTextScript::parse()) was succeeded \n"));
        LogRel(("Read lines is %d \n", lines));
        size_t startSectionPos=0;
        bool fInSection = false;
        std::map <Utf8Str, Utf8Str> sectionConfiguration;
        Utf8Str strSectionName("Default section ");

        for (size_t i=0; i<lines; ++i)
        {
            //find the beginning of section, it starts from the line like "[section name]"
            //next find the end of section, it ends up when we find the beginning
            //of the next section or reach the end of the contents.
            //Go through the found lines and split each of them up by "=".
            //Each correct line must look like "key=value"
            //if there is not the character "=" in the line one is skipped
            //class Utf8Str contains the function parseKeyValue() for this action
            LogRel(("Parsing the line %d \n", i));
            Utf8Str strLineContent = getContentOfLine(i);
            if (strLineContent.isEmpty() || strLineContent.startsWith("#"))
                continue;

            LogRel(("Content of the line %d is %s \n", i, strLineContent.c_str()));

            //strips the white-space characters from the string. It's needed for strings on Windows with "\n\r".
            strLineContent = strLineContent.strip();
            if ( strLineContent.startsWith("[") && strLineContent.endsWith("]") )
            {
                LogRel(("Found section in the line %d\n", i));
                if ( fInSection == true )
                {
                    if ( i > startSectionPos )//at least we can have here 1 line in the section
                    {
                        LogRel(("Add section \"%s\" to the map \n", strSectionName.c_str()));
                        mSections.insert(make_pair(strSectionName, sectionConfiguration));
                        sectionConfiguration.clear();
                        strSectionName.append(Utf8StrFmt("%d",i).c_str());
                    }
                }

                strSectionName = strLineContent.substr(strLineContent.find("[")+1,
                                                       strLineContent.find("]")-1);
                LogRel(("Section name is \"%s\" \n", strSectionName.c_str()));
                fInSection = true;
                startSectionPos = i+1;
            }
            else
            {
                if ( fInSection == true )
                {
                    LogRel(("Continue to parse section \"%s\" \n", strSectionName.c_str()));
                    if ( i >= startSectionPos )
                    {
                        LogRel(("Extracting key and value:\n"));
                        Utf8Str key, value;
                        size_t offEnd = strLineContent.parseKeyValue(key, value);
                        if (offEnd == strLineContent.length())
                        {
                            LogRel(("%s=%s \n", key.c_str(), value.c_str()));
                            sectionConfiguration.insert(make_pair(key,value));
                        }
                        else
                        {
                            //else something goes wrong, skip the line
                            LogRel(("Coudn't extract key and value from the line %d\n", i));
                        }
                    }
                }
                else
                {
                    LogRel(("Goes to the next line %d\n", i+1));
                }
            }
        }

        if (fInSection == false)//there is not any section
        {
            LogRel(("There are not any sections in the config\n"));
            hrc = E_FAIL;
        }
        else //the last section hasn't a close tag (the close tag is just the beginning of next section)
        {
            //just insert the last section into the map
            LogRel(("Add the last section %s to the map \n", strSectionName.c_str()));
            mSections.insert(make_pair(strSectionName, sectionConfiguration));
        }
    }

    return hrc;
}

HRESULT SimpleConfigFile::addSection(const Utf8Str &aSectionName, const std::map <Utf8Str, Utf8Str>& section)
{
    HRESULT hrc = S_OK;
    if (aSectionName.isEmpty())
        hrc = E_FAIL;
    else
        mSections.insert(make_pair(aSectionName, section));

    return hrc;
}

std::vector <Utf8Str> SimpleConfigFile::getSectionsNames() const
{
    std::vector <Utf8Str> res;
    std::map < Utf8Str, std::map <Utf8Str, Utf8Str> >::const_iterator cit = mSections.begin();
    while (cit!=mSections.end())
    {
        res.push_back(cit->first);
        ++cit;
    }
    return res;
}

std::map <Utf8Str, Utf8Str> SimpleConfigFile::getSectionByName (const Utf8Str &strSectionName) const
{
    std::map <Utf8Str, Utf8Str> res;
    std::map < Utf8Str, std::map <Utf8Str, Utf8Str> >::const_iterator cit;
    if ( (cit=mSections.find(strSectionName)) != mSections.end() )
    {
        res=cit->second;
    }
    return res;
}

HRESULT SimpleConfigFile::updateSection (const Utf8Str &strSectionName,
                                         const std::map <Utf8Str, Utf8Str> &newSection)
{
    HRESULT hrc = S_OK;
    std::map <Utf8Str, Utf8Str> oldSection = getSectionByName(strSectionName);
    if (oldSection.empty())
    {
        //add new section
        hrc = addSection(strSectionName, newSection);
    }
    else
    {
        //update old section by new values or add new pair key/value if there isn't such key
        std::map <Utf8Str, Utf8Str>::const_iterator cit = newSection.begin();
        while (cit != newSection.end())
        {
            oldSection[cit->first] = cit->second;
            ++cit;
        }

    }
    return hrc;
}

bool SimpleConfigFile::isSectionExist(const Utf8Str &strSectionName) const
{
    return ((mSections.find(strSectionName) == mSections.end()) ? false : true);
}

////////////////////////////////////////////////////////////////////////////////
//
// ICloudUserProfileList implementation
//
////////////////////////////////////////////////////////////////////////////////
CloudUserProfileList::CloudUserProfileList()
    : mParent(NULL)
{
}

CloudUserProfileList::~CloudUserProfileList()
{
    LogRel(("CloudUserProfileListImpl::~CloudUserProfileListImpl()\n"));
    unconst(mParent) = NULL;
}

HRESULT CloudUserProfileList::FinalConstruct()
{
    return BaseFinalConstruct();
}

void CloudUserProfileList::FinalRelease()
{
    uninit();

    BaseFinalRelease();
}

void CloudUserProfileList::uninit()
{
    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    unconst(mParent) = NULL;
}

HRESULT CloudUserProfileList::init(VirtualBox *aParent)
{
    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    unconst(mParent) = aParent;

    autoInitSpan.setSucceeded();
    return S_OK;
}


HRESULT CloudUserProfileList::getSupportedPropertiesNames(std::vector<com::Utf8Str> &aPropertiesNames)
{
    LogRel(("CloudUserProfileList::getSupportedPropertiesNames:\n"));
    aPropertiesNames.clear();
    return setErrorBoth(E_FAIL, VERR_NOT_IMPLEMENTED, tr("Not implemented"));
}

HRESULT CloudUserProfileList::readProfiles(const Utf8Str &strConfigPath)
{
    LogRel(("CloudUserProfileList::readProfiles: %s\n", strConfigPath.c_str()));
    return setErrorBoth(E_FAIL, VERR_NOT_IMPLEMENTED, tr("Not implemented"));
}

HRESULT CloudUserProfileList::getProvider(CloudProviderId_T *aProvider)
{
    *aProvider = CloudProviderId_Unknown;
    LogRel(("CloudUserProfileList::getProvider: %d\n", *aProvider));
    return setErrorBoth(E_FAIL, VERR_NOT_IMPLEMENTED, tr("Not implemented"));
}

HRESULT CloudUserProfileList::createProfile(const com::Utf8Str &aProfileName,
                                            const std::vector<com::Utf8Str> &aNames,
                                            const std::vector<com::Utf8Str> &aValues)
{
    LogRel(("CloudUserProfileList::createProfile: %s, %d, %d\n", aProfileName.c_str(), aNames.size(), aValues.size()));
    return setErrorBoth(E_FAIL, VERR_NOT_IMPLEMENTED, tr("Not implemented"));
}

HRESULT CloudUserProfileList::updateProfile(const com::Utf8Str &aProfileName,
                                            const std::vector<com::Utf8Str> &aNames,
                                            const std::vector<com::Utf8Str> &aValues)
{
    LogRel(("CloudUserProfileList::updateProfile: %s, %d, %d\n", aProfileName.c_str(), aNames.size(), aValues.size()));
    return setErrorBoth(E_FAIL, VERR_NOT_IMPLEMENTED, tr("Not implemented"));
}

HRESULT CloudUserProfileList::getStoredProfilesNames(std::vector<com::Utf8Str> &aProfilesNames)
{

    LogRel(("CloudUserProfileList::getStoredProfilesNames:\n"));
    aProfilesNames.clear();
    return setErrorBoth(E_FAIL, VERR_NOT_IMPLEMENTED, tr("Not implemented"));
}

HRESULT CloudUserProfileList::getProfileProperties(const com::Utf8Str &aProfileName,
                                              std::vector<com::Utf8Str> &aReturnNames,
                                              std::vector<com::Utf8Str> &aReturnValues)
{
    LogRel(("CloudUserProfileList::getProfileProperties: %s\n", aProfileName.c_str()));
    aReturnNames.clear();
    aReturnValues.clear();
    return setErrorBoth(E_FAIL, VERR_NOT_IMPLEMENTED, tr("Not implemented"));
}

HRESULT CloudUserProfileList::getPropertyDescription(const com::Utf8Str &aName,
                                                     com::Utf8Str &aDescription)
{
    LogRel(("CloudUserProfileList::getPropertyDescription: %s, %s\n", aName.c_str(), aDescription.c_str()));
    return setErrorBoth(E_FAIL, VERR_NOT_IMPLEMENTED, tr("Not implemented"));
}

HRESULT CloudUserProfileList::createCloudClient(const com::Utf8Str &aProfileName,
                                                ComPtr<ICloudClient> &aCloudClient)
{
    LogRel(("CloudUserProfileList::createCloudClient: %s\n", aProfileName.c_str()));

    if (aCloudClient.isNull())
    {
        LogRel(("aCloudClient is NULL\n"));
    }

    return setErrorBoth(E_FAIL, VERR_NOT_IMPLEMENTED, tr("Not implemented"));
}

////////////////////////////////////////////////////////////////////////////////
//
// CloudConnectionOCI implementation
//
////////////////////////////////////////////////////////////////////////////////
static struct
{
    const char *pszOCIConfigEntry, *pszDesciption;
} const g_aOCIConfigEntryToDescription[] =
{
    { "user",               "OCID of the user calling the API." },
    { "tenancy",            "OCID of your tenancy." },
    { "compartment",        "OCID of your compartment." },
    { "fingerprint",        "Fingerprint for the key pair being used." },
    { "key_file",           "Full path and filename of the private key."
                            "If you encrypted the key with a passphrase, you must also include "
                            "the pass_phrase entry in the config file."},
    { "pass_phrase",        "Passphrase used for the key, if it is encrypted." },
    { "region",             "An Oracle Cloud Infrastructure region" },
};


OCIUserProfileList::OCIUserProfileList()
{
    LogRel(("OCIUserProfileList::OCIUserProfileList()\n"));
    mpProfiles = new SimpleConfigFile(mParent);
    LogRel(("Succeeded create SimpleConfigFile\n"));
}

OCIUserProfileList::~OCIUserProfileList()
{
    LogRel(("OCIUserProfileList::~OCIUserProfileList()\n"));
    if (mpProfiles)
        delete mpProfiles;
}

HRESULT OCIUserProfileList::createCloudClient(const com::Utf8Str &aProfileName,
                                              ComPtr<ICloudClient> &aCloudClient)
{
    HRESULT hrc = S_OK;
    CloudProviderId_T providerId;
    hrc = getProvider(&providerId);

    ComObjPtr<CloudClient> ptrCloudClient;
    hrc = ptrCloudClient.createObject();
    if (SUCCEEDED(hrc))
    {
        ComObjPtr<CloudClientOCI> ptrCloudClientOCI;
        hrc = ptrCloudClientOCI.createObject();
        AutoReadLock wlock(this COMMA_LOCKVAL_SRC_POS);
        hrc = ptrCloudClientOCI->initCloudClient(this, mParent, providerId, aProfileName);
        if (SUCCEEDED(hrc))
        {
            ptrCloudClient = ptrCloudClientOCI;
            hrc = ptrCloudClient.queryInterfaceTo(aCloudClient.asOutParam());
        }
    }

    return hrc;
}

HRESULT OCIUserProfileList::readProfiles(const Utf8Str &strConfigPath)
{
    LogRel(("Reading profiles from %s\n", strConfigPath.c_str()));
    HRESULT hrc = S_OK;
    if ( !strConfigPath.isEmpty() )
    {
        mStrConfigPath = strConfigPath;
        hrc = mpProfiles->read(mStrConfigPath);
        if (SUCCEEDED(hrc))
        {
            LogRel(("Successfully read the profiles from the config %s\n", mStrConfigPath.c_str()));
            hrc = mpProfiles->parse();
            if (FAILED(hrc))
            {
                return hrc;
            }
            LogRel(("Successfully parsed %d profiles\n", mpProfiles->getNumberOfSections()));
        }
    }
    else
    {
        LogRel(("Empty path to config file\n"));
        hrc = setErrorBoth(E_FAIL, VERR_INVALID_PARAMETER, tr("Empty path to config file"));
    }

    return hrc;
}

HRESULT OCIUserProfileList::createProfile(const com::Utf8Str &aProfileName,
                                          const std::vector<com::Utf8Str> &aNames,
                                          const std::vector<com::Utf8Str> &aValues)
{
    HRESULT hrc = S_OK;

    if (!mpProfiles->isSectionExist(aProfileName))
    {
        std::map <Utf8Str, Utf8Str> newProfile;

        for (size_t i=0;i<aNames.size();++i)
        {
            com::Utf8Str newValue = (i<aValues.size()) ? aValues.at(i) : "";
            newProfile[aNames.at(i)] = newValue;
        }

        hrc = mpProfiles->addSection(aProfileName, newProfile);
    }
    else
        hrc = setErrorBoth(E_FAIL, VERR_ALREADY_EXISTS, tr("Profile '%s' has already exested"), aProfileName.c_str());

    return hrc;
}

HRESULT OCIUserProfileList::updateProfile(const com::Utf8Str &aProfileName,
                                          const std::vector<com::Utf8Str> &aNames,
                                          const std::vector<com::Utf8Str> &aValues)
{
    HRESULT hrc = S_OK;
    if (mpProfiles->isSectionExist(aProfileName))
    {
        std::map <Utf8Str, Utf8Str> newProfile;

        for (size_t i=0;i<aNames.size();++i)
        {
            com::Utf8Str newValue = (i<aValues.size()) ? aValues.at(i) : "";
            newProfile[aNames.at(i)] = newValue;
        }

        hrc = mpProfiles->updateSection(aProfileName, newProfile);
    }
    else
        hrc = setErrorBoth(E_FAIL, VERR_NOT_FOUND, tr("Profile '%s' wasn't found"), aProfileName.c_str());

    return hrc;
}

HRESULT OCIUserProfileList::getStoredProfilesNames(std::vector<com::Utf8Str> &aProfilesNames)
{
    HRESULT hrc = S_OK;
    aProfilesNames = mpProfiles->getSectionsNames();
    if (aProfilesNames.empty())
        hrc = setErrorBoth(E_FAIL, VERR_NOT_FOUND, tr("There aren't any profiles in the config"));

    return hrc;
}

HRESULT OCIUserProfileList::getProfileProperties(const com::Utf8Str &aProfileName,
                                                 std::vector<com::Utf8Str> &aReturnNames,
                                                 std::vector<com::Utf8Str> &aReturnValues)
{
    HRESULT hrc = S_OK;

    if (mpProfiles->isSectionExist(aProfileName))
    {
        std::map <Utf8Str, Utf8Str> profile;
        hrc = i_getProfileProperties(aProfileName, profile);
        if (SUCCEEDED(hrc))
        {
            aReturnNames.clear();
            aReturnValues.clear();
            std::map <Utf8Str, Utf8Str>::const_iterator cit = profile.begin();
            while (cit!=profile.end())
            {
                aReturnNames.push_back(cit->first);
                aReturnValues.push_back(cit->second);
                ++cit;
            }
        }
    }
    else
        hrc = setErrorBoth(E_FAIL, VERR_NOT_FOUND, tr("Profile '%s' wasn't found"), aProfileName.c_str());

    return hrc;
}

HRESULT OCIUserProfileList::getSupportedPropertiesNames(std::vector<com::Utf8Str> &aPropertiesNames)
{
    HRESULT hrc = S_OK;
    for (size_t i = 0; i < RT_ELEMENTS(g_aOCIConfigEntryToDescription); ++i)
        aPropertiesNames.push_back(g_aOCIConfigEntryToDescription[i].pszOCIConfigEntry);
    return hrc;
}

HRESULT OCIUserProfileList::getPropertyDescription(const com::Utf8Str &aName, com::Utf8Str &aDescription)
{
    HRESULT hrc = S_OK;
    for (size_t i = 0; i < RT_ELEMENTS(g_aOCIConfigEntryToDescription); ++i)
        if (aName.contains(g_aOCIConfigEntryToDescription[i].pszOCIConfigEntry, Utf8Str::CaseInsensitive))
        {
            aDescription = g_aOCIConfigEntryToDescription[i].pszDesciption;
        }
    return hrc;
}


HRESULT OCIUserProfileList::i_createProfile(const com::Utf8Str &aProfileName,
                                           const std::map <Utf8Str, Utf8Str> &aProfile)
{
    HRESULT hrc = S_OK;

    hrc = mpProfiles->addSection(aProfileName, aProfile);

    return hrc;
}

HRESULT OCIUserProfileList::i_updateProfile(const com::Utf8Str &aProfileName,
                                           const std::map <Utf8Str, Utf8Str> &aProfile)
{
    HRESULT hrc = S_OK;
    if (mpProfiles->isSectionExist(aProfileName))
        hrc = mpProfiles->updateSection(aProfileName, aProfile);
    else
        hrc = setErrorBoth(E_FAIL, VERR_NOT_FOUND, tr("Profile '%s' wasn't found"), aProfileName.c_str());


    return hrc;
}

HRESULT OCIUserProfileList::i_getProfileProperties(const com::Utf8Str &aProfileName,
                                                  std::map <Utf8Str, Utf8Str> &aProfile)
{
    HRESULT hrc = S_OK;
    std::map <Utf8Str, Utf8Str> defProfile = mpProfiles->getSectionByName("DEFAULT");
    std::map <Utf8Str, Utf8Str> reqProfile = mpProfiles->getSectionByName(aProfileName);

    std::map <Utf8Str, Utf8Str>::iterator itDefProfile = defProfile.begin();
    while (itDefProfile != defProfile.end())
    {
        std::map <Utf8Str, Utf8Str>::iterator itProfile = reqProfile.find(itDefProfile->first);
        if (itProfile == reqProfile.end())
        {
            //Add a key=value pair from defProfile into the reqProfile if the key doesn't exist in the reqProfile.
            reqProfile.insert(*itDefProfile);
        }
        ++itDefProfile;
    }

    if (!reqProfile.empty())
        aProfile = reqProfile;
    else
        hrc = setErrorBoth(E_FAIL, VERR_NOT_FOUND, tr("Profile '%s' wasn't found"), aProfileName.c_str());

    return hrc;
}

