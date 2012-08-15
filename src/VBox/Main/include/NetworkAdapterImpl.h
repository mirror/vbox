/* $Id$ */

/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_NETWORKADAPTER
#define ____H_NETWORKADAPTER

#include "VirtualBoxBase.h"
#include "NATEngineImpl.h"
#include "BandwidthGroupImpl.h"

class GuestOSType;

namespace settings
{
    struct NetworkAdapter;
}

class ATL_NO_VTABLE NetworkAdapter :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(INetworkAdapter)
{
public:

    struct Data
    {
        Data() : mSlot(0),
                 mEnabled(FALSE),
                 mAttachmentType(NetworkAttachmentType_Null),
                 mCableConnected(TRUE),
                 mLineSpeed(0),
                 mPromiscModePolicy(NetworkAdapterPromiscModePolicy_Deny),
                 mTraceEnabled(FALSE),
                 mBridgedInterface("") /* cannot be null */,
                 mHostOnlyInterface("") /* cannot be null */,
                 mNATNetwork("") /* cannot be null */,
                 mBootPriority(0)
        {}

        NetworkAdapterType_T mAdapterType;
        ULONG mSlot;
        BOOL mEnabled;
        Bstr mMACAddress;
        NetworkAttachmentType_T mAttachmentType;
        BOOL mCableConnected;
        ULONG mLineSpeed;
        NetworkAdapterPromiscModePolicy_T mPromiscModePolicy;
        BOOL mTraceEnabled;
        Bstr mTraceFile;
        Bstr mBridgedInterface;
        Bstr mHostOnlyInterface;
        Bstr mInternalNetwork;
        Bstr mNATNetwork;
        Bstr mGenericDriver;
        settings::StringsMap mGenericProperties;
        ULONG mBootPriority;
        Utf8Str mBandwidthGroup;
    };

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(NetworkAdapter, INetworkAdapter)

    DECLARE_NOT_AGGREGATABLE(NetworkAdapter)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(NetworkAdapter)
        VBOX_DEFAULT_INTERFACE_ENTRIES(INetworkAdapter)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR(NetworkAdapter)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(Machine *aParent, ULONG aSlot);
    HRESULT init(Machine *aParent, NetworkAdapter *aThat, bool aReshare = false);
    HRESULT initCopy(Machine *aParent, NetworkAdapter *aThat);
    void uninit();

    // INetworkAdapter properties
    STDMETHOD(COMGETTER(AdapterType))(NetworkAdapterType_T *aAdapterType);
    STDMETHOD(COMSETTER(AdapterType))(NetworkAdapterType_T aAdapterType);
    STDMETHOD(COMGETTER(Slot))(ULONG *aSlot);
    STDMETHOD(COMGETTER(Enabled))(BOOL *aEnabled);
    STDMETHOD(COMSETTER(Enabled))(BOOL aEnabled);
    STDMETHOD(COMGETTER(MACAddress))(BSTR *aMACAddress);
    STDMETHOD(COMSETTER(MACAddress))(IN_BSTR aMACAddress);
    STDMETHOD(COMGETTER(AttachmentType))(NetworkAttachmentType_T *aAttachmentType);
    STDMETHOD(COMSETTER(AttachmentType))(NetworkAttachmentType_T aAttachmentType);
    STDMETHOD(COMGETTER(BridgedInterface))(BSTR *aBridgedInterface);
    STDMETHOD(COMSETTER(BridgedInterface))(IN_BSTR aBridgedInterface);
    STDMETHOD(COMGETTER(HostOnlyInterface))(BSTR *aHostOnlyInterface);
    STDMETHOD(COMSETTER(HostOnlyInterface))(IN_BSTR aHostOnlyInterface);
    STDMETHOD(COMGETTER(InternalNetwork))(BSTR *aInternalNetwork);
    STDMETHOD(COMSETTER(InternalNetwork))(IN_BSTR aInternalNetwork);
    STDMETHOD(COMGETTER(NATNetwork))(BSTR *aNATNetwork);
    STDMETHOD(COMSETTER(NATNetwork))(IN_BSTR aNATNetwork);
    STDMETHOD(COMGETTER(GenericDriver))(BSTR *aGenericDriver);
    STDMETHOD(COMSETTER(GenericDriver))(IN_BSTR aGenericDriver);
    STDMETHOD(COMGETTER(CableConnected))(BOOL *aConnected);
    STDMETHOD(COMSETTER(CableConnected))(BOOL aConnected);
    STDMETHOD(COMGETTER(TraceEnabled))(BOOL *aEnabled);
    STDMETHOD(COMSETTER(TraceEnabled))(BOOL aEnabled);
    STDMETHOD(COMGETTER(LineSpeed))(ULONG *aSpeed);
    STDMETHOD(COMSETTER(LineSpeed))(ULONG aSpeed);
    STDMETHOD(COMGETTER(PromiscModePolicy))(NetworkAdapterPromiscModePolicy_T *aPromiscModePolicy);
    STDMETHOD(COMSETTER(PromiscModePolicy))(NetworkAdapterPromiscModePolicy_T aPromiscModePolicy);
    STDMETHOD(COMGETTER(TraceFile))(BSTR *aTraceFile);
    STDMETHOD(COMSETTER(TraceFile))(IN_BSTR aTraceFile);
    STDMETHOD(COMGETTER(NATEngine))(INATEngine **aNATEngine);
    STDMETHOD(COMGETTER(BootPriority))(ULONG *aBootPriority);
    STDMETHOD(COMSETTER(BootPriority))(ULONG aBootPriority);
    STDMETHOD(COMGETTER(BandwidthGroup))(IBandwidthGroup **aBwGroup);
    STDMETHOD(COMSETTER(BandwidthGroup))(IBandwidthGroup *aBwGroup);

    // INetworkAdapter methods
    STDMETHOD(GetProperty)(IN_BSTR aName, BSTR *aValue);
    STDMETHOD(SetProperty)(IN_BSTR aName, IN_BSTR aValue);
    STDMETHOD(GetProperties)(IN_BSTR aNames,
                             ComSafeArrayOut(BSTR, aReturnNames),
                             ComSafeArrayOut(BSTR, aReturnValues));

    // public methods only for internal purposes

    HRESULT loadSettings(BandwidthControl *bwctl, const settings::NetworkAdapter &data);
    HRESULT saveSettings(settings::NetworkAdapter &data);

    bool isModified();
    void rollback();
    void commit();
    void copyFrom(NetworkAdapter *aThat);
    void applyDefaults(GuestOSType *aOsType);

    ComObjPtr<NetworkAdapter> getPeer();

private:

    void generateMACAddress();
    HRESULT updateMacAddress(Utf8Str aMacAddress);
    void updateBandwidthGroup(BandwidthGroup *aBwGroup);

    Machine * const     mParent;
    const ComObjPtr<NetworkAdapter> mPeer;
    const ComObjPtr<NATEngine> mNATEngine;

    bool                m_fModified;
    Backupable<Data>    mData;
};

#endif // ____H_NETWORKADAPTER
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
