/* $Id$ */
/** @file
 * ICloudProviderManager  COM class implementations.
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


#include <VBox/com/array.h>

#include "VirtualBoxImpl.h"
#include "CloudProviderManagerImpl.h"
#include "ExtPackManagerImpl.h"
#include "AutoCaller.h"
#include "Logging.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////
//
// CloudProviderManager constructor / destructor
//
// ////////////////////////////////////////////////////////////////////////////////
CloudProviderManager::CloudProviderManager()
{
}

CloudProviderManager::~CloudProviderManager()
{
}


HRESULT CloudProviderManager::FinalConstruct()
{
    return BaseFinalConstruct();
}

void CloudProviderManager::FinalRelease()
{
    uninit();

    BaseFinalRelease();
}

HRESULT CloudProviderManager::init(VirtualBox *aParent)
{
    // Enclose the state transition NotReady->InInit->Ready.
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m_apCloudProviders.clear();

#if defined(VBOX_WITH_CLOUD_PROVIDERS_IN_EXTPACK) && defined(VBOX_WITH_EXTPACK)
    // Engage the extension pack manager and get all the implementations of
    // this class and all implemented cloud providers.
    ExtPackManager *pExtPackMgr = aParent->i_getExtPackManager();
    mpExtPackMgr = pExtPackMgr;
    if (pExtPackMgr)
    {
        mcExtPackMgrUpdate = pExtPackMgr->i_getUpdateCounter();
        // Make sure that the current value doesn't match, forcing a refresh.
        mcExtPackMgrUpdate--;
        i_refreshProviders();
    }
#else
    RT_NOREF(aParent);
#endif

    autoInitSpan.setSucceeded();
    return S_OK;
}

void CloudProviderManager::uninit()
{
    // Enclose the state transition Ready->InUninit->NotReady.
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;
}

#ifdef VBOX_WITH_CLOUD_PROVIDERS_IN_EXTPACK
void CloudProviderManager::i_refreshProviders()
{
    uint64_t cExtPackMgrUpdate;
    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
        if (mpExtPackMgr.isNull())
            return;
        cExtPackMgrUpdate = mpExtPackMgr->i_getUpdateCounter();
        if (cExtPackMgrUpdate == mcExtPackMgrUpdate)
            return;
    }

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    // Reread the current value to figure out if some other thead did the work
    // already before this thread got hold of the write lock.
    cExtPackMgrUpdate = mpExtPackMgr->i_getUpdateCounter();
    if (cExtPackMgrUpdate == mcExtPackMgrUpdate)
        return;
    mcExtPackMgrUpdate = cExtPackMgrUpdate;

    if (!m_mapCloudProviderManagers.empty())
    {
        /// @todo The code below will need to be extended to handle extpack
        // install and uninstall safely (and the latter needs non-trivial
        // checks if any extpack related cloud activity is pending.
        return;
    }

    std::vector<ComPtr<IUnknown> > apObjects;
    std::vector<com::Utf8Str> astrExtPackNames;
    com::Guid idObj(COM_IIDOF(ICloudProviderManager));
    mpExtPackMgr->i_queryObjects(idObj.toString(), apObjects, &astrExtPackNames);
    for (unsigned i = 0; i < apObjects.size(); i++)
    {
        ComPtr<ICloudProviderManager> pTmp;
        HRESULT hrc = apObjects[i].queryInterfaceTo(pTmp.asOutParam());
        if (SUCCEEDED(hrc))
            m_mapCloudProviderManagers[astrExtPackNames[i]] = pTmp;
        SafeIfaceArray<ICloudProvider> apProvidersFromCurrExtPack;
        hrc = pTmp->COMGETTER(Providers)(ComSafeArrayAsOutParam(apProvidersFromCurrExtPack));
        if (SUCCEEDED(hrc))
        {
            for (unsigned j = 0; j < apProvidersFromCurrExtPack.size(); j++)
                m_apCloudProviders.push_back(apProvidersFromCurrExtPack[i]);
        }
    }
}
#endif

HRESULT CloudProviderManager::getProviders(std::vector<ComPtr<ICloudProvider> > &aProviders)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    aProviders = m_apCloudProviders;
    return S_OK;
}

HRESULT CloudProviderManager::getProviderById(const com::Guid &aProviderId,
                                              ComPtr<ICloudProvider> &aProvider)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    for (size_t i = 0; i < m_apCloudProviders.size(); i++)
    {
        Bstr bstrId;
        HRESULT hrc = m_apCloudProviders[i]->COMGETTER(Id)(bstrId.asOutParam());
        if (SUCCEEDED(hrc) && aProviderId == bstrId)
        {
            aProvider = m_apCloudProviders[i];
            return S_OK;
        }
    }
    return setError(VBOX_E_OBJECT_NOT_FOUND, tr("Could not find a cloud provider with UUID {%RTuuid}"),
                    aProviderId.raw());
}

HRESULT CloudProviderManager::getProviderByShortName(const com::Utf8Str &aProviderName,
                                                     ComPtr<ICloudProvider> &aProvider)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    for (size_t i = 0; i < m_apCloudProviders.size(); i++)
    {
        Bstr bstrName;
        HRESULT hrc = m_apCloudProviders[i]->COMGETTER(ShortName)(bstrName.asOutParam());
        if (SUCCEEDED(hrc) && bstrName.equals(aProviderName))
        {
            aProvider = m_apCloudProviders[i];
            return S_OK;
        }
    }
    return setError(VBOX_E_OBJECT_NOT_FOUND, tr("Could not find a cloud provider with short name '%s'"),
                    aProviderName.c_str());
}

HRESULT CloudProviderManager::getProviderByName(const com::Utf8Str &aProviderName,
                                                ComPtr<ICloudProvider> &aProvider)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    for (size_t i = 0; i < m_apCloudProviders.size(); i++)
    {
        Bstr bstrName;
        HRESULT hrc = m_apCloudProviders[i]->COMGETTER(Name)(bstrName.asOutParam());
        if (SUCCEEDED(hrc) && bstrName.equals(aProviderName))
        {
            aProvider = m_apCloudProviders[i];
            return S_OK;
        }
    }
    return setError(VBOX_E_OBJECT_NOT_FOUND, tr("Could not find a cloud provider with name '%s'"),
                    aProviderName.c_str());
}

