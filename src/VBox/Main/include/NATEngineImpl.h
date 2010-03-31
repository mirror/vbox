/* $Id$ */

/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ____H_NATDRIVER
#define ____H_NATDRIVER


#include "VirtualBoxBase.h"
#include <VBox/settings.h>

namespace settings
{
    struct NAT;
}

class ATL_NO_VTABLE NATEngine :
    public VirtualBoxBase,
    public VirtualBoxSupportErrorInfoImpl<NATEngine, INATEngine>,
    public VirtualBoxSupportTranslation<NATEngine>,
    VBOX_SCRIPTABLE_IMPL(INATEngine)
{
    public:
    typedef std::map<Utf8Str, settings::NATRule> NATRuleMap;
    struct Data
    {
        Data(): mMtu(0),
               mSockRcv(0),
               mSockSnd(0),
               mTcpRcv(0),
               mTcpSnd(0),
               mDnsPassDomain(TRUE),
               mDnsProxy(FALSE),
               mDnsUseHostResolver(FALSE) {}

        com::Utf8Str mNetwork;
        com::Utf8Str mBindIP;
        uint32_t mMtu;
        uint32_t mSockRcv;
        uint32_t mSockSnd;
        uint32_t mTcpRcv;
        uint32_t mTcpSnd;
        /* TFTP service */
        Utf8Str  mTftpPrefix;
        Utf8Str  mTftpBootFile;
        Utf8Str  mTftpNextServer;
        /* DNS service */
        BOOL     mDnsPassDomain;
        BOOL     mDnsProxy;
        BOOL     mDnsUseHostResolver;
    };
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT (NATEngine)

    DECLARE_NOT_AGGREGATABLE(NATEngine)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(NATEngine)
        COM_INTERFACE_ENTRY  (ISupportErrorInfo)
        COM_INTERFACE_ENTRY  (INATEngine)
        COM_INTERFACE_ENTRY2 (IDispatch, INATEngine)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR (NATEngine)

    HRESULT FinalConstruct();
    HRESULT init(Machine *aParent);
    HRESULT init(Machine *aParent, NATEngine *aThat);
    HRESULT initCopy(Machine *aParent, NATEngine *aThat);
    bool isModified();
    bool isReallyModified();
    bool rollback();
    void commit();
    void uninit();
    void FinalRelease();

    HRESULT loadSettings(const settings::NAT &data);
    HRESULT saveSettings(settings::NAT &data);

    STDMETHOD(COMSETTER(Network)) (IN_BSTR aNetwork);
    STDMETHOD(COMGETTER(Network)) (BSTR *aNetwork);
    STDMETHOD(COMSETTER(HostIP)) (IN_BSTR aBindIP);
    STDMETHOD(COMGETTER(HostIP)) (BSTR *aBindIP);
    /* TFTP attributes */
    STDMETHOD(COMSETTER(TftpPrefix)) (IN_BSTR aTftpPrefix);
    STDMETHOD(COMGETTER(TftpPrefix)) (BSTR *aTftpPrefix);
    STDMETHOD(COMSETTER(TftpBootFile)) (IN_BSTR aTftpBootFile);
    STDMETHOD(COMGETTER(TftpBootFile)) (BSTR *aTftpBootFile);
    STDMETHOD(COMSETTER(TftpNextServer)) (IN_BSTR aTftpNextServer);
    STDMETHOD(COMGETTER(TftpNextServer)) (BSTR *aTftpNextServer);
    /* DNS attributes */
    STDMETHOD(COMSETTER(DnsPassDomain)) (BOOL aDnsPassDomain);
    STDMETHOD(COMGETTER(DnsPassDomain)) (BOOL *aDnsPassDomain);
    STDMETHOD(COMSETTER(DnsProxy)) (BOOL aDnsProxy);
    STDMETHOD(COMGETTER(DnsProxy)) (BOOL *aDnsProxy);
    STDMETHOD(COMGETTER(DnsUseHostResolver)) (BOOL *aDnsUseHostResolver);
    STDMETHOD(COMSETTER(DnsUseHostResolver)) (BOOL aDnsUseHostResolver);

    STDMETHOD(SetNetworkSettings)(ULONG aMtu, ULONG aSockSnd, ULONG aSockRcv, ULONG aTcpWndSnd, ULONG aTcpWndRcv);
    STDMETHOD(GetNetworkSettings)(ULONG *aMtu, ULONG *aSockSnd, ULONG *aSockRcv, ULONG *aTcpWndSnd, ULONG *aTcpWndRcv);

    STDMETHOD(COMGETTER(Redirects)) (ComSafeArrayOut (BSTR, aNatRules));
    STDMETHOD(AddRedirect)(IN_BSTR aName, ULONG aProto, IN_BSTR aBindIp, USHORT aHostPort, IN_BSTR aGuestIP, USHORT aGuestPort);
    STDMETHOD(RemoveRedirect)(IN_BSTR aName);

    static const wchar_t *getComponentName() { return L"NATEngine"; }
private:
    Backupable<Data> mData;
    bool m_fModified;
    const ComObjPtr<NATEngine> mPeer;
    Machine * const mParent;
    NATRuleMap mNATRules;
};
#endif
