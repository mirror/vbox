/* $Id$ */

/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_VRDPSERVER
#define ____H_VRDPSERVER

#include "VirtualBoxBase.h"

#include <VBox/VRDPAuth.h>
#include <VBox/settings.h>

class ATL_NO_VTABLE VRDEServer :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IVRDEServer)
{
public:

    struct Data
    {
        BOOL mEnabled;
        AuthType_T mAuthType;
        ULONG mAuthTimeout;
        BOOL mAllowMultiConnection;
        BOOL mReuseSingleConnection;
        BOOL mVideoChannel;
        ULONG mVideoChannelQuality;
        Utf8Str mVrdeExtPack;
        settings::StringsMap mProperties;
    };

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(VRDEServer, IVRDEServer)

    DECLARE_NOT_AGGREGATABLE(VRDEServer)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(VRDEServer)
        COM_INTERFACE_ENTRY  (ISupportErrorInfo)
        COM_INTERFACE_ENTRY  (IVRDEServer)
        COM_INTERFACE_ENTRY2 (IDispatch, IVRDEServer)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR (VRDEServer)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(Machine *aParent);
    HRESULT init(Machine *aParent, VRDEServer *aThat);
    HRESULT initCopy (Machine *aParent, VRDEServer *aThat);
    void uninit();

    // IVRDEServer properties
    STDMETHOD(COMGETTER(Enabled)) (BOOL *aEnabled);
    STDMETHOD(COMSETTER(Enabled)) (BOOL aEnable);
    STDMETHOD(COMGETTER(AuthType)) (AuthType_T *aType);
    STDMETHOD(COMSETTER(AuthType)) (AuthType_T aType);
    STDMETHOD(COMGETTER(AuthTimeout)) (ULONG *aTimeout);
    STDMETHOD(COMSETTER(AuthTimeout)) (ULONG aTimeout);
    STDMETHOD(COMGETTER(AllowMultiConnection)) (BOOL *aAllowMultiConnection);
    STDMETHOD(COMSETTER(AllowMultiConnection)) (BOOL aAllowMultiConnection);
    STDMETHOD(COMGETTER(ReuseSingleConnection)) (BOOL *aReuseSingleConnection);
    STDMETHOD(COMSETTER(ReuseSingleConnection)) (BOOL aReuseSingleConnection);
    STDMETHOD(COMGETTER(VideoChannel)) (BOOL *aVideoChannel);
    STDMETHOD(COMSETTER(VideoChannel)) (BOOL aVideoChannel);
    STDMETHOD(COMGETTER(VideoChannelQuality)) (ULONG *aVideoChannelQuality);
    STDMETHOD(COMSETTER(VideoChannelQuality)) (ULONG aVideoChannelQuality);
    STDMETHOD(COMGETTER(VRDEExtPack))(BSTR *aExtPack);
    STDMETHOD(COMSETTER(VRDEExtPack))(IN_BSTR aExtPack);

    // IVRDEServer methods
    STDMETHOD(SetVRDEProperty) (IN_BSTR aKey, IN_BSTR aValue);
    STDMETHOD(GetVRDEProperty) (IN_BSTR aKey, BSTR *aValue);

    // public methods only for internal purposes

    HRESULT loadSettings(const settings::VRDESettings &data);
    HRESULT saveSettings(settings::VRDESettings &data);

    void rollback();
    void commit();
    void copyFrom (VRDEServer *aThat);

private:

    Machine * const     mParent;
    const ComObjPtr<VRDEServer> mPeer;

    Backupable<Data>    mData;
};

#endif // ____H_VRDPSERVER
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
