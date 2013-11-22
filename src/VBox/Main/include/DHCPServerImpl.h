/* $Id$ */

/** @file
 *
 * VirtualBox COM class implementation
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

#ifndef ____H_H_DHCPSERVERIMPL
#define ____H_H_DHCPSERVERIMPL

#include "VirtualBoxBase.h"

#ifdef VBOX_WITH_HOSTNETIF_API
struct NETIFINFO;
#endif

namespace settings
{
    struct DHCPServer;
    struct VmNameSlotKey;
}
#ifdef RT_OS_WINDOWS
# define DHCP_EXECUTABLE_NAME "VBoxNetDHCP.exe"
#else
# define DHCP_EXECUTABLE_NAME "VBoxNetDHCP"
#endif

class DHCPServerRunner: public NetworkServiceRunner
{
public:
    DHCPServerRunner():NetworkServiceRunner(DHCP_EXECUTABLE_NAME){}
    virtual ~DHCPServerRunner(){};

    static const std::string kDsrKeyGateway;
    static const std::string kDsrKeyLowerIp;
    static const std::string kDsrKeyUpperIp;
};

/**
 *  for server configuration needs, it's perhaps better to use (VM,slot) pair
 *  (vm-name, slot) <----> (MAC)
 *
 *  but for client configuration, when server will have MACs at hand, it'd be
 *  easier to requiest options by MAC.
 *  (MAC) <----> (option-list)
 *
 *  Doubts: What should be done if MAC changed for (vm-name, slot), when syncing should?
 *  XML: serialization of dependecy (DHCP options) - (VM,slot) shouldn't be done via MAC in
 *  the middle.
 */

typedef std::map<DhcpOpt_T, com::Utf8Str> DhcpOptionMap;
typedef DhcpOptionMap::value_type DhcpOptValuePair;
typedef DhcpOptionMap::const_iterator DhcpOptConstIterator;
typedef DhcpOptionMap::iterator DhcpOptIterator;

typedef std::map<settings::VmNameSlotKey, DhcpOptionMap> VmSlot2OptionsMap;
typedef VmSlot2OptionsMap::value_type VmSlot2OptionsPair;
typedef VmSlot2OptionsMap::iterator VmSlot2OptionsIterator;


class ATL_NO_VTABLE DHCPServer :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IDHCPServer)
{
public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(DHCPServer, IDHCPServer)

    DECLARE_NOT_AGGREGATABLE (DHCPServer)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP (DHCPServer)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IDHCPServer)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR (DHCPServer)

    HRESULT FinalConstruct();
    void FinalRelease();

    HRESULT init(VirtualBox *aVirtualBox,
                 IN_BSTR aName);
    HRESULT init(VirtualBox *aVirtualBox,
                 const settings::DHCPServer &data);
    HRESULT saveSettings(settings::DHCPServer &data);

    void uninit();

    // IDHCPServer properties
    STDMETHOD(COMGETTER(NetworkName))(BSTR *aName);
    STDMETHOD(COMGETTER(Enabled))(BOOL *aEnabled);
    STDMETHOD(COMSETTER(Enabled))(BOOL aEnabled);
    STDMETHOD(COMGETTER(IPAddress))(BSTR *aIPAddress);
    STDMETHOD(COMGETTER(NetworkMask))(BSTR *aNetworkMask);
    STDMETHOD(COMGETTER(LowerIP))(BSTR *aIPAddress);
    STDMETHOD(COMGETTER(UpperIP))(BSTR *aIPAddress);

    STDMETHOD(AddGlobalOption)(DhcpOpt_T aOption, IN_BSTR aValue);
    STDMETHOD(COMGETTER(GlobalOptions))(ComSafeArrayOut(BSTR, aValue));
    STDMETHOD(COMGETTER(VmConfigs))(ComSafeArrayOut(BSTR, aValue));
    STDMETHOD(AddVmSlotOption)(IN_BSTR aVmName, LONG aSlot, DhcpOpt_T aOption, IN_BSTR aValue);
    STDMETHOD(RemoveVmSlotOptions)(IN_BSTR aVmName, LONG aSlot);
    STDMETHOD(GetVmSlotOptions)(IN_BSTR aVmName, LONG aSlot, ComSafeArrayOut(BSTR, aValues));
    STDMETHOD(GetMacOptions)(IN_BSTR aMAC, ComSafeArrayOut(BSTR, aValues));
    STDMETHOD(COMGETTER(EventSource))(IEventSource **aEventSource);

    STDMETHOD(SetConfiguration)(IN_BSTR aIPAddress, IN_BSTR aNetworkMask, IN_BSTR aFromIPAddress, IN_BSTR aToIPAddress);

    STDMETHOD(Start)(IN_BSTR aNetworkName, IN_BSTR aTrunkName, IN_BSTR aTrunkType);
    STDMETHOD(Stop)();

private:
    struct Data;
    Data *m;
    /** weak VirtualBox parent */
    VirtualBox * const      mVirtualBox;
    const Bstr mName;

    DhcpOptionMap& findOptMapByVmNameSlot(const com::Utf8Str& aVmName,
                                          LONG Slot);
};

#endif // ____H_H_DHCPSERVERIMPL
