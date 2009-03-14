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
/* class DhcpServer; */
/* #include "netif.h" */
struct NETIFINFO;
#endif

class ATL_NO_VTABLE DhcpServer :
    public VirtualBoxBaseNEXT,
    public VirtualBoxSupportErrorInfoImpl <DhcpServer, IDhcpServer>,
    public VirtualBoxSupportTranslation <DhcpServer>,
    public IDhcpServer
{
public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT (DhcpServer)

    DECLARE_NOT_AGGREGATABLE (DhcpServer)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP (DhcpServer)
        COM_INTERFACE_ENTRY (ISupportErrorInfo)
        COM_INTERFACE_ENTRY (IDhcpServer)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    DECLARE_EMPTY_CTOR_DTOR (DhcpServer)

    HRESULT FinalConstruct();
    void FinalRelease();

    HRESULT init(IN_BSTR aName);
    HRESULT init(const settings::Key &aNode);
    HRESULT saveSettings (settings::Key &aParentNode);

    // IDhcpServer properties
    STDMETHOD(COMGETTER(NetworkName)) (BSTR *aName);
    STDMETHOD(COMGETTER(Enabled)) (BOOL *aEnabled);
    STDMETHOD(COMSETTER(Enabled)) (BOOL aEnabled);
    STDMETHOD(COMGETTER(IPAddress)) (BSTR *aIPAddress);
    STDMETHOD(COMGETTER(NetworkMask)) (BSTR *aNetworkMask);
    STDMETHOD(COMGETTER(FromIPAddress)) (BSTR *aIPAddress);
    STDMETHOD(COMGETTER(ToIPAddress)) (BSTR *aIPAddress);

    STDMETHOD(SetConfiguration) (IN_BSTR aIPAddress, IN_BSTR aNetworkMask, IN_BSTR aFromIPAddress, IN_BSTR aToIPAddress);

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"DhcpServer"; }

private:
    const Bstr mName;

    struct Data
    {
        Data() : enabled(FALSE) {}

        Bstr IPAddress;
        Bstr networkMask;
        Bstr FromIPAddress;
        Bstr ToIPAddress;
        BOOL enabled;
    } m;

};

#endif // ____H_H_DHCPSERVERIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
