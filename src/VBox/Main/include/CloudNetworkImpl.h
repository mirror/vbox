/* $Id$ */
/** @file
 * ICloudNetwork implementation header, lives in VBoxSVC.
 */

/*
 * Copyright (C) 2019-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef MAIN_INCLUDED_CloudNetworkImpl_h
#define MAIN_INCLUDED_CloudNetworkImpl_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "CloudNetworkWrap.h"

namespace settings
{
    struct CloudNetwork;
}

class ATL_NO_VTABLE CloudNetwork :
    public CloudNetworkWrap
{
public:

    DECLARE_EMPTY_CTOR_DTOR(CloudNetwork)

    HRESULT FinalConstruct();
    void FinalRelease();

    HRESULT init(VirtualBox *aVirtualBox, com::Utf8Str aName);
    HRESULT i_loadSettings(const settings::CloudNetwork &data);
    void uninit();
    HRESULT i_saveSettings(settings::CloudNetwork &data);

    // Internal methods
    Utf8Str i_getNetworkName();
    Utf8Str i_getProvider();
    Utf8Str i_getProfile();
    Utf8Str i_getNetworkId();
private:

    // Wrapped ICloudNetwork properties
    HRESULT getNetworkName(com::Utf8Str &aNetworkName);
    HRESULT setNetworkName(const com::Utf8Str &aNetworkName);
    HRESULT getEnabled(BOOL *aEnabled);
    HRESULT setEnabled(BOOL aEnabled);
    HRESULT getProvider(com::Utf8Str &aProvider);
    HRESULT setProvider(const com::Utf8Str &aProvider);
    HRESULT getProfile(com::Utf8Str &aProfile);
    HRESULT setProfile(const com::Utf8Str &aProfile);
    HRESULT getNetworkId(com::Utf8Str &aNetworkId);
    HRESULT setNetworkId(const com::Utf8Str &aNetworkId);

    struct Data;
    Data *m;
};

#endif /* !MAIN_INCLUDED_CloudNetworkImpl_h */
