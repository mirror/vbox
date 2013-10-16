/* $Id$ */
/** @file
 * INATNetwork implementation header, lives in VBoxSVC.
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_H_NATNETWORKIMPL
#define ____H_H_NATNETWORKIMPL

#include "VirtualBoxBase.h"

#ifdef VBOX_WITH_HOSTNETIF_API
struct NETIFINFO;
#endif

namespace settings
{
    struct NATNetwork;
    struct NATRule;
}

#ifdef RT_OS_WINDOWS
# define NATSR_EXECUTABLE_NAME "VBoxNetNAT.exe"
#else
# define NATSR_EXECUTABLE_NAME "VBoxNetNAT"
#endif

enum ADDRESSLOOKUPTYPE
{
    ADDR_GATEWAY,
    ADDR_DHCP,
    ADDR_DHCPLOWERIP,
    ADDR_ANY
};

class NATNetworkServiceRunner: public NetworkServiceRunner
{
public:
    NATNetworkServiceRunner(): NetworkServiceRunner(NATSR_EXECUTABLE_NAME){}
    virtual ~NATNetworkServiceRunner(){}
};

class ATL_NO_VTABLE NATNetwork :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(INATNetwork)
{
public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(NATNetwork, INATNetwork)

    DECLARE_NOT_AGGREGATABLE (NATNetwork)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP (NATNetwork)
        VBOX_DEFAULT_INTERFACE_ENTRIES(INATNetwork)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR (NATNetwork)

    HRESULT FinalConstruct();
    void FinalRelease();

    HRESULT init(VirtualBox *aVirtualBox,
                 IN_BSTR aName);


    HRESULT init(VirtualBox *aVirtualBox,
                 const settings::NATNetwork &data);
    HRESULT saveSettings(settings::NATNetwork &data);
    void uninit();
    // INATNetwork::EventSource
    STDMETHOD(COMGETTER(EventSource))(IEventSource **IEventSource);
    // INATNetwork properties
    STDMETHOD(COMGETTER(Enabled))(BOOL *aEnabled);
    STDMETHOD(COMSETTER(Enabled))(BOOL aEnabled);

    STDMETHOD(COMGETTER(NetworkName))(BSTR *aName);
    STDMETHOD(COMSETTER(NetworkName))(IN_BSTR aName);

    STDMETHOD(COMGETTER(Gateway))(BSTR *aIPGateway);

    STDMETHOD(COMGETTER(Network))(BSTR *aIPNetwork);
    STDMETHOD(COMSETTER(Network))(IN_BSTR aIPNetwork);

    STDMETHOD(COMGETTER(IPv6Enabled))(BOOL *aEnabled);
    STDMETHOD(COMSETTER(IPv6Enabled))(BOOL aEnabled);

    STDMETHOD(COMGETTER(IPv6Prefix))(BSTR *aName);
    STDMETHOD(COMSETTER(IPv6Prefix))(IN_BSTR aName);

    STDMETHOD(COMGETTER(AdvertiseDefaultIPv6RouteEnabled))(BOOL *aEnabled);
    STDMETHOD(COMSETTER(AdvertiseDefaultIPv6RouteEnabled))(BOOL aEnabled);

    STDMETHOD(COMGETTER(NeedDhcpServer))(BOOL *aEnabled);
    STDMETHOD(COMSETTER(NeedDhcpServer))(BOOL aEnabled);

    STDMETHOD(COMGETTER(LocalMappings))(ComSafeArrayOut(BSTR, aLocalMappings));
    STDMETHOD(AddLocalMapping)(IN_BSTR aHostId, LONG aOffset);

    STDMETHOD(COMGETTER(LoopbackIp6))(LONG *aLoopbackIp6);
    STDMETHOD(COMSETTER(LoopbackIp6))(LONG aLoopbackIp6);

    STDMETHOD(COMGETTER(PortForwardRules4))(ComSafeArrayOut(BSTR, aPortForwardRules4));
    STDMETHOD(COMGETTER(PortForwardRules6))(ComSafeArrayOut(BSTR, aPortForwardRules6));

    STDMETHOD(AddPortForwardRule)(BOOL aIsIpv6,
                                  IN_BSTR aPortForwardRuleName,
                                  NATProtocol_T aProto,
                                  IN_BSTR aHostIp,
                                  USHORT aHostPort,
                                  IN_BSTR aGuestIp,
                                  USHORT aGuestPort);
    STDMETHOD(RemovePortForwardRule)(BOOL aIsIpv6, IN_BSTR aPortForwardRuleName);

    STDMETHOD(Start)(IN_BSTR aTrunkType);
    STDMETHOD(Stop)();

private:
    int recalculateIpv4AddressAssignments();
    int findFirstAvailableOffset(ADDRESSLOOKUPTYPE, uint32_t *);

    typedef std::map<Utf8Str, settings::NATRule> NATRuleMap;
    typedef NATRuleMap::const_iterator constNATRuleMapIterator;

    void GetPortForwardRulesFromMap(ComSafeArrayOut(BSTR, aPortForwardRules), NATRuleMap& aRules);
    /** weak VirtualBox parent */
    VirtualBox * const      mVirtualBox;

    const Bstr mName;
    struct Data;
    struct Data *m;

};

#endif // !____H_H_NATNETWORKIMPL
