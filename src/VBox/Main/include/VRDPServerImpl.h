/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef ____H_VRDPSERVER
#define ____H_VRDPSERVER

#include "VirtualBoxBase.h"

#include <VBox/VRDPAuth.h>
#include <VBox/cfgldr.h>

class Machine;

class ATL_NO_VTABLE VRDPServer :
    public VirtualBoxSupportErrorInfoImpl <VRDPServer, IVRDPServer>,
    public VirtualBoxSupportTranslation <VRDPServer>,
    public VirtualBoxBase,
    public IVRDPServer
{
public:

    struct Data
    {
        bool operator== (const Data &that) const
        {
            return this == &that ||
                   (mEnabled == that.mEnabled &&
                    mVRDPPort == that.mVRDPPort &&
                    mVRDPAddress == that.mVRDPAddress &&
                    mAuthType == that.mAuthType &&
                    mAuthTimeout == that.mAuthTimeout);
        }

        BOOL mEnabled;
        ULONG mVRDPPort;
        Bstr mVRDPAddress;
        VRDPAuthType_T mAuthType;
        ULONG mAuthTimeout;
    };

    DECLARE_NOT_AGGREGATABLE(VRDPServer)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(VRDPServer)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IVRDPServer)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(Machine *parent);
    HRESULT init(Machine *parent, VRDPServer *that);
    HRESULT initCopy (Machine *parent, VRDPServer *that);
    void uninit();

    // IVRDPServer properties
    STDMETHOD(COMGETTER(Enabled))(BOOL *enabled);
    STDMETHOD(COMSETTER(Enabled))(BOOL enable);
    STDMETHOD(COMGETTER(Port))(ULONG *port);
    STDMETHOD(COMSETTER(Port))(ULONG port);
    STDMETHOD(COMGETTER(NetAddress))(BSTR *address);
    STDMETHOD(COMSETTER(NetAddress))(INPTR BSTR address);
    STDMETHOD(COMGETTER(AuthType))(VRDPAuthType_T *type);
    STDMETHOD(COMSETTER(AuthType))(VRDPAuthType_T type);
    STDMETHOD(COMGETTER(AuthTimeout))(ULONG *timeout);
    STDMETHOD(COMSETTER(AuthTimeout))(ULONG timeout);

    // IVRDPServer methods

    // public methods only for internal purposes

    void loadConfig (CFGNODE node);
    void saveConfig (CFGNODE node);

    const Backupable <Data> &data() const { return mData; }

    bool isModified() { AutoLock alock (this); return mData.isBackedUp(); }
    bool isReallyModified() { AutoLock alock (this); return mData.hasActualChanges(); }
    bool rollback();
    void commit();
    void copyFrom (VRDPServer *aThat);

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"VRDPServer"; }

private:

    ComObjPtr <Machine, ComWeakRef> mParent;
    ComObjPtr <VRDPServer> mPeer;
    Backupable <Data> mData;
};

#endif // ____H_VRDPSERVER
