/* $Id$ */
/** @file
 * IHostOnlyNetwork implementation header, lives in VBoxSVC.
 */

/*
 * Copyright (C) 2021-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef MAIN_INCLUDED_HostOnlyNetworkImpl_h
#define MAIN_INCLUDED_HostOnlyNetworkImpl_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "HostOnlyNetworkWrap.h"

namespace settings
{
    struct HostOnlyNetwork;
}

class ATL_NO_VTABLE HostOnlyNetwork :
    public HostOnlyNetworkWrap
{
public:

    DECLARE_EMPTY_CTOR_DTOR(HostOnlyNetwork)

    HRESULT FinalConstruct();
    void FinalRelease();

    HRESULT init(VirtualBox *aVirtualBox, com::Utf8Str aName);
    HRESULT i_loadSettings(const settings::HostOnlyNetwork &data);
    void uninit();
    HRESULT i_saveSettings(settings::HostOnlyNetwork &data);

    // Internal methods
    // Utf8Str i_getNetworkName();
    // Utf8Str i_getNetworkId();
private:

    // Wrapped IHostOnlyNetwork properties
    HRESULT getNetworkName(com::Utf8Str &aNetworkName);
    HRESULT setNetworkName(const com::Utf8Str &aNetworkName);
    HRESULT getNetworkMask(com::Utf8Str &aNetworkMask);
    HRESULT setNetworkMask(const com::Utf8Str &aNetworkMask);
    HRESULT getEnabled(BOOL *aEnabled);
    HRESULT setEnabled(BOOL aEnabled);
    HRESULT getHostIP(com::Utf8Str &aHostIP);
    HRESULT getLowerIP(com::Utf8Str &aLowerIP);
    HRESULT setLowerIP(const com::Utf8Str &aLowerIP);
    HRESULT getUpperIP(com::Utf8Str &aUpperIP);
    HRESULT setUpperIP(const com::Utf8Str &aUpperIP);
    HRESULT getId(com::Guid &aId);
    HRESULT setId(const com::Guid &aId);

    struct Data;
    Data *m;
};

#endif /* !MAIN_INCLUDED_HostOnlyNetworkImpl_h */
