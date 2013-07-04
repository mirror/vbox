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
}

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

    STDMETHOD(SetConfiguration)(IN_BSTR aIPAddress, IN_BSTR aNetworkMask, IN_BSTR aFromIPAddress, IN_BSTR aToIPAddress);

    STDMETHOD(Start)(IN_BSTR aNetworkName, IN_BSTR aTrunkName, IN_BSTR aTrunkType);
    STDMETHOD(Stop)();

private:
    /** weak VirtualBox parent */
    VirtualBox * const      mVirtualBox;

    const Bstr mName;

    struct Data
    {
        Data() : enabled(FALSE) {}

        Bstr IPAddress;
        Bstr networkMask;
        Bstr lowerIP;
        Bstr upperIP;
        BOOL enabled;

        DHCPServerRunner dhcp;
    } m;

};

#endif // ____H_H_DHCPSERVERIMPL
