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

#ifndef ____H_H_DHCPSERVERIMPL
#define ____H_H_DHCPSERVERIMPL

#include "VirtualBoxBase.h"
#include "VirtualBoxImpl.h"

#ifdef VBOX_WITH_HOSTNETIF_API
/* class DHCPServer; */
/* #include "netif.h" */
struct NETIFINFO;
#endif

class ATL_NO_VTABLE DHCPServer :
    public VirtualBoxBaseNEXT,
    public VirtualBoxSupportErrorInfoImpl <DHCPServer, IDHCPServer>,
    public VirtualBoxSupportTranslation <DHCPServer>,
    public IDHCPServer
{
public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT (DHCPServer)

    DECLARE_NOT_AGGREGATABLE (DHCPServer)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP (DHCPServer)
        COM_INTERFACE_ENTRY (ISupportErrorInfo)
        COM_INTERFACE_ENTRY (IDHCPServer)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    DECLARE_EMPTY_CTOR_DTOR (DHCPServer)

    HRESULT FinalConstruct();
    void FinalRelease();

    HRESULT init(VirtualBox *aVirtualBox, IN_BSTR aName);
    HRESULT init(VirtualBox *aVirtualBox, const settings::Key &aNode);
    HRESULT saveSettings (settings::Key &aParentNode);

    void uninit();

    // IDHCPServer properties
    STDMETHOD(COMGETTER(NetworkName)) (BSTR *aName);
    STDMETHOD(COMGETTER(Enabled)) (BOOL *aEnabled);
    STDMETHOD(COMSETTER(Enabled)) (BOOL aEnabled);
    STDMETHOD(COMGETTER(IPAddress)) (BSTR *aIPAddress);
    STDMETHOD(COMGETTER(NetworkMask)) (BSTR *aNetworkMask);
    STDMETHOD(COMGETTER(LowerIP)) (BSTR *aIPAddress);
    STDMETHOD(COMGETTER(UpperIP)) (BSTR *aIPAddress);

    STDMETHOD(SetConfiguration) (IN_BSTR aIPAddress, IN_BSTR aNetworkMask, IN_BSTR aFromIPAddress, IN_BSTR aToIPAddress);

    STDMETHOD(Start) (IN_BSTR aNetworkName, IN_BSTR aTrunkName, IN_BSTR aTrunkType);
    STDMETHOD(Stop) ();

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"DHCPServer"; }

private:
    /** weak VirtualBox parent */
    const ComObjPtr <VirtualBox, ComWeakRef> mVirtualBox;

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
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
