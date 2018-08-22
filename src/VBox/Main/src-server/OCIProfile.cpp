/* $Id$ */
/** @file
 * ICloudProfile COM class implementation (OCI).
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

#include "OCIProfile.h"
#include "OCIProvider.h"
#include "CloudClientImpl.h"
#include "AutoCaller.h"
#define LOG_GROUP_MAIN_OVERRIDE LOG_GROUP_MAIN_CLOUDPROFILE
#include "Logging.h"

#include <iprt/cpp/utils.h>


////////////////////////////////////////////////////////////////////////////////
//
// OCIProfile implementation
//
////////////////////////////////////////////////////////////////////////////////
OCIProfile::OCIProfile()
    : mVirtualBox(NULL), mParent(NULL)
{
}

OCIProfile::~OCIProfile()
{
    LogRel(("OCIProfile::~OCIProfile()\n"));
    unconst(mVirtualBox) = NULL;
    unconst(mParent).setNull();
}

HRESULT OCIProfile::FinalConstruct()
{
    return BaseFinalConstruct();
}

void OCIProfile::FinalRelease()
{
    uninit();

    BaseFinalRelease();
}

void OCIProfile::uninit()
{
    // Enclose the state transition Ready->InUninit->NotReady.
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    unconst(mVirtualBox) = NULL;
    unconst(mParent).setNull();
}

HRESULT OCIProfile::initFromConfig(VirtualBox *aVirtualBox,
                                   OCIProvider *aParent,
                                   const com::Utf8Str &aProfileName)
{
    // Enclose the state transition NotReady->InInit->Ready.
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);
#ifndef VBOX_WITH_CLOUD_PROVIDERS_IN_EXTPACK
    AssertReturn(aVirtualBox, E_INVALIDARG);
#endif
    AssertReturn(aParent, E_INVALIDARG);
    AssertReturn(!aProfileName.isEmpty(), E_INVALIDARG);

    unconst(mVirtualBox) = aVirtualBox;
    unconst(mParent) = aParent;
    unconst(mName) = aProfileName;

    autoInitSpan.setSucceeded();
    return S_OK;
}

HRESULT OCIProfile::initNew(VirtualBox *aVirtualBox,
                            OCIProvider *aParent,
                            const com::Utf8Str &aProfileName,
                            const std::vector<com::Utf8Str> &aNames,
                            const std::vector<com::Utf8Str> &aValues)
{
    // Enclose the state transition NotReady->InInit->Ready.
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);
#ifndef VBOX_WITH_CLOUD_PROVIDERS_IN_EXTPACK
    AssertReturn(aVirtualBox, E_INVALIDARG);
#endif
    AssertReturn(aParent, E_INVALIDARG);
    AssertReturn(!aProfileName.isEmpty(), E_INVALIDARG);

    unconst(mVirtualBox) = aVirtualBox;
    unconst(mParent) = aParent;
    unconst(mName) = aProfileName;

    HRESULT hrc = S_OK;
    if (!aParent->i_existsProfile(aProfileName))
        hrc = aParent->i_addProfile(aProfileName, aNames, aValues);
    else
        hrc = setError(E_FAIL, tr("Profile '%s' already exists"), aProfileName.c_str());

    if (SUCCEEDED(hrc))
        autoInitSpan.setSucceeded();
    return hrc;
}

void OCIProfile::i_getProfile(StringMap &aProfile) const
{
    AutoCaller autoCaller(mParent);
    if (FAILED(autoCaller.rc()))
        return;
    AutoReadLock plock(mParent COMMA_LOCKVAL_SRC_POS);

    mParent->i_getProfileMap(mName, aProfile);
}


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


HRESULT OCIProfile::getName(com::Utf8Str &aName)
{
    // mName is constant during life time, no need to lock.
    aName = mName;
    return S_OK;
}

HRESULT OCIProfile::getProviderId(com::Guid &aProviderId)
{
    // provider id is constant during life time, no need to lock.
    aProviderId = mParent->i_getId();
    return S_OK;
}

HRESULT OCIProfile::getSupportedPropertyNames(std::vector<com::Utf8Str> &aSupportedPropertyNames)
{
    // g_aOCIConfigEntryToDescription is constant during life time, no need to lock.
    for (size_t i = 0; i < RT_ELEMENTS(g_aOCIConfigEntryToDescription); ++i)
        aSupportedPropertyNames.push_back(g_aOCIConfigEntryToDescription[i].pszOCIConfigEntry);
    return S_OK;
}

HRESULT OCIProfile::getProperty(const com::Utf8Str &aName,
                                Utf8Str &aReturnValue)
{
    AutoCaller autoCaller(mParent);
    if (FAILED(autoCaller.rc()))
        return autoCaller.rc();
    AutoReadLock alock(mParent COMMA_LOCKVAL_SRC_POS);
    mParent->i_getProfileProperty(mName, aName, aReturnValue);
    return S_OK;
}

HRESULT OCIProfile::setProperty(const com::Utf8Str &aName,
                                const com::Utf8Str &aValue)
{
    AutoCaller autoCaller(mParent);
    if (FAILED(autoCaller.rc()))
        return autoCaller.rc();
    AutoWriteLock alock(mParent COMMA_LOCKVAL_SRC_POS);
    mParent->i_updateProfileProperty(mName, aName, aValue);
    return S_OK;
}

HRESULT OCIProfile::getProperties(const com::Utf8Str &aNames,
                                  std::vector<com::Utf8Str> &aReturnNames,
                                  std::vector<com::Utf8Str> &aReturnValues)
{
    AutoCaller autoCaller(mParent);
    if (FAILED(autoCaller.rc()))
        return autoCaller.rc();
    AutoReadLock alock(mParent COMMA_LOCKVAL_SRC_POS);
    /// @todo implement name filtering (everywhere... lots of similar code in many places handling properties.
    RT_NOREF(aNames);
    mParent->i_getProfile(mName, aReturnNames, aReturnValues);
    return S_OK;
}

HRESULT OCIProfile::setProperties(const std::vector<com::Utf8Str> &aNames,
                                  const std::vector<com::Utf8Str> &aValues)
{
    AutoCaller autoCaller(mParent);
    if (FAILED(autoCaller.rc()))
        return autoCaller.rc();
    AutoWriteLock alock(mParent COMMA_LOCKVAL_SRC_POS);
    return mParent->i_updateProfile(mName, aNames, aValues);
}

HRESULT OCIProfile::getPropertyDescription(const com::Utf8Str &aName, com::Utf8Str &aDescription)
{
    // g_aOCIConfigEntryToDescription is constant during life time, no need to lock.
    for (size_t i = 0; i < RT_ELEMENTS(g_aOCIConfigEntryToDescription); ++i)
        if (aName.contains(g_aOCIConfigEntryToDescription[i].pszOCIConfigEntry, Utf8Str::CaseInsensitive))
        {
            aDescription = g_aOCIConfigEntryToDescription[i].pszDesciption;
        }
    return S_OK;
}


HRESULT OCIProfile::createCloudClient(ComPtr<ICloudClient> &aCloudClient)
{
    ComObjPtr<CloudClientOCI> pCloudClientOCI;
    HRESULT hrc = pCloudClientOCI.createObject();
    if (FAILED(hrc))
        return hrc;
    hrc = pCloudClientOCI->initCloudClient(mVirtualBox, this);
    if (SUCCEEDED(hrc))
        hrc = pCloudClientOCI.queryInterfaceTo(aCloudClient.asOutParam());

    return hrc;
}
