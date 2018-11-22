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

HRESULT CloudProviderManager::init()
{
    // Enclose the state transition NotReady->InInit->Ready.
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m_apCloudProviders.clear();

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

#ifdef VBOX_WITH_EXTPACK
bool CloudProviderManager::i_canRemoveExtPack(IExtPack *aExtPack)
{
    AssertReturn(aExtPack, false);

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    // If any cloud provider in this extension pack fails to prepare the
    // uninstall it and the cloud provider will be kept, so that the user
    // can retry safely later. All other cloud providers in this extpack
    // will be done as usual. No attempt is made to bring back the other
    // cloud providers into working shape.

    bool fRes = true;
    Bstr bstrName;
    aExtPack->GetName(bstrName.asOutParam());
    Utf8Str strName(bstrName);
    ExtPackNameCloudProviderManagerMap::iterator it = m_mapCloudProviderManagers.find(strName);
    if (it != m_mapCloudProviderManagers.end())
    {
        ComPtr<ICloudProviderManager> pTmp(it->second);

        Assert(m_astrExtPackNames.size() == m_apCloudProviders.size());
        for (size_t i = 0; i < m_astrExtPackNames.size(); )
        {
            if (m_astrExtPackNames[i] != strName)
            {
                i++;
                continue;
            }

            // pTmpProvider will point to an object with refcount > 0 until
            // the ComPtr is removed from m_apCloudProviders.
            HRESULT hrc = S_OK;
            ULONG uRefCnt = 1;
            ICloudProvider *pTmpProvider(m_apCloudProviders[i]);
            if (pTmpProvider)
            {
                hrc = pTmpProvider->PrepareUninstall();
                // Sanity check the refcount, it should be 1 at this point.
                pTmpProvider->AddRef();
                uRefCnt = pTmpProvider->Release();
                Assert(uRefCnt == 1);
            }
            if (SUCCEEDED(hrc) && uRefCnt == 1)
            {
                m_astrExtPackNames.erase(m_astrExtPackNames.begin() + i);
                m_apCloudProviders.erase(m_apCloudProviders.begin() + i);
            }
            else
            {
                LogRel(("CloudProviderManager: provider '%s' blocks extpack uninstall, result=%Rhrc, refcount=%u\n", strName.c_str(), hrc, uRefCnt));
                fRes = false;
                i++;
            }
        }

        if (fRes)
            m_mapCloudProviderManagers.erase(it);
    }

    return fRes;
}

void CloudProviderManager::i_addExtPack(IExtPack *aExtPack)
{
    AssertReturnVoid(aExtPack);

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    Bstr bstrName;
    aExtPack->GetName(bstrName.asOutParam());
    Utf8Str strName(bstrName);
    ComPtr<IUnknown> pObj;
    std::vector<com::Utf8Str> astrExtPackNames;
    com::Guid idObj(COM_IIDOF(ICloudProviderManager));
    HRESULT hrc = aExtPack->QueryObject(Bstr(idObj.toString()).raw(), pObj.asOutParam());
    if (FAILED(hrc))
        return;

    ComPtr<ICloudProviderManager> pTmp(pObj);
    if (pTmp.isNull())
        return;

    SafeIfaceArray<ICloudProvider> apProvidersFromCurrExtPack;
    hrc = pTmp->COMGETTER(Providers)(ComSafeArrayAsOutParam(apProvidersFromCurrExtPack));
    if (FAILED(hrc))
        return;

    m_mapCloudProviderManagers[strName] = pTmp;
    for (unsigned i = 0; i < apProvidersFromCurrExtPack.size(); i++)
    {
        Assert(m_astrExtPackNames.size() == m_apCloudProviders.size());
        m_astrExtPackNames.push_back(strName);
        m_apCloudProviders.push_back(apProvidersFromCurrExtPack[i]);
    }
}
#endif  /* VBOX_WITH_EXTPACK */

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

