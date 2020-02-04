/* $Id$ */
/** @file
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2018-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef MAIN_INCLUDED_CloudProviderManagerImpl_h
#define MAIN_INCLUDED_CloudProviderManagerImpl_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "CloudProviderManagerWrap.h"


class ATL_NO_VTABLE CloudProviderManager
    : public CloudProviderManagerWrap
{
public:
    DECLARE_EMPTY_CTOR_DTOR(CloudProviderManager)

    HRESULT FinalConstruct();
    void FinalRelease();

    HRESULT init();
    void uninit();

#ifdef VBOX_WITH_EXTPACK
    // Safe helpers, take care of caller and lock themselves.
    bool i_canRemoveExtPack(IExtPack *aExtPack);
    void i_addExtPack(IExtPack *aExtPack);
#endif

private:
    // wrapped ICloudProviderManager attributes and methods
    HRESULT getProviders(std::vector<ComPtr<ICloudProvider> > &aProviders);
    HRESULT getProviderById(const com::Guid &aProviderId,
                            ComPtr<ICloudProvider> &aProvider);
    HRESULT getProviderByShortName(const com::Utf8Str &aProviderName,
                                   ComPtr<ICloudProvider> &aProvider);
    HRESULT getProviderByName(const com::Utf8Str &aProviderName,
                              ComPtr<ICloudProvider> &aProvider);

private:
#ifdef VBOX_WITH_EXTPACK
    typedef std::map<com::Utf8Str, ComPtr<ICloudProviderManager> > ExtPackNameCloudProviderManagerMap;
    ExtPackNameCloudProviderManagerMap m_mapCloudProviderManagers;
    typedef std::vector<com::Utf8Str> ExtPackNameVec;
    ExtPackNameVec m_astrExtPackNames;
#endif

    typedef std::vector<ComPtr<ICloudProvider> > CloudProviderVec;
    CloudProviderVec m_apCloudProviders;
};

#endif /* !MAIN_INCLUDED_CloudProviderManagerImpl_h */

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
