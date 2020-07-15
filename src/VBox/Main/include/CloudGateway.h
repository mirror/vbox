/* $Id$ */
/** @file
 * Implementation of local and cloud gateway management.
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

#ifndef MAIN_INCLUDED_CloudGateway_h
#define MAIN_INCLUDED_CloudGateway_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

struct GatewayInfo
{
    Bstr    mTargetVM;
    Utf8Str mGatewayVM;
    Utf8Str mGatewayInstanceId;
    Utf8Str mPublicSshKey;
    Bstr    mCloudProvider;
    Bstr    mCloudProfile;
    Utf8Str mCloudPublicIp;
    Utf8Str mCloudSecondaryPublicIp;
    RTMAC   mCloudMacAddress;
    RTMAC   mLocalMacAddress;
    int     mAdapterSlot;

    HRESULT setCloudMacAddress(const Utf8Str& mac);
    HRESULT setLocalMacAddress(const Utf8Str& mac);

    Utf8Str getCloudMacAddressWithoutColons() const;
    Utf8Str getLocalMacAddressWithoutColons() const;
    Utf8Str getLocalMacAddressWithColons() const;

    GatewayInfo() {}

    GatewayInfo(const GatewayInfo& other)
        : mGatewayVM(other.mGatewayVM),
          mGatewayInstanceId(other.mGatewayInstanceId),
          mPublicSshKey(other.mPublicSshKey),
          mCloudProvider(other.mCloudProvider),
          mCloudProfile(other.mCloudProfile),
          mCloudPublicIp(other.mCloudPublicIp),
          mCloudSecondaryPublicIp(other.mCloudSecondaryPublicIp),
          mCloudMacAddress(other.mCloudMacAddress),
          mLocalMacAddress(other.mLocalMacAddress),
          mAdapterSlot(other.mAdapterSlot)
    {}

    GatewayInfo& operator=(const GatewayInfo& other)
    {
        mGatewayVM = other.mGatewayVM;
        mGatewayInstanceId = other.mGatewayInstanceId;
        mPublicSshKey = other.mPublicSshKey;
        mCloudProvider = other.mCloudProvider;
        mCloudProfile = other.mCloudProfile;
        mCloudPublicIp = other.mCloudPublicIp;
        mCloudSecondaryPublicIp = other.mCloudSecondaryPublicIp;
        mCloudMacAddress = other.mCloudMacAddress;
        mLocalMacAddress = other.mLocalMacAddress;
        mAdapterSlot = other.mAdapterSlot;
        return *this;
    }

    void setNull()
    {
        mGatewayVM.setNull();
        mGatewayInstanceId.setNull();
        mPublicSshKey.setNull();
        mCloudProvider.setNull();
        mCloudProfile.setNull();
        mCloudPublicIp.setNull();
        mCloudSecondaryPublicIp.setNull();
        memset(&mCloudMacAddress, 0, sizeof(mCloudMacAddress));
        memset(&mLocalMacAddress, 0, sizeof(mLocalMacAddress));
        mAdapterSlot = -1;
    }
};

class CloudNetwork;

HRESULT startGateways(ComPtr<IVirtualBox> virtualBox, ComPtr<ICloudNetwork> network, GatewayInfo& pGateways);
HRESULT stopGateways(ComPtr<IVirtualBox> virtualBox, const GatewayInfo& gateways);

#endif /* !MAIN_INCLUDED_CloudGateway_h */

