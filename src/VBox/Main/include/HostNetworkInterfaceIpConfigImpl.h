/* $Id$ */

/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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

#ifndef ____H_HOSTNETWORKINTERFACEIPCONFIGIMPL
#define ____H_HOSTNETWORKINTERFACEIPCONFIGIMPL

#include "VirtualBoxBase.h"
#include "Collection.h"

#ifdef VBOX_WITH_HOSTNETIF_API
/* class HostNetworkInterfaceIpConfig; */
/* #include "netif.h" */
struct NETIFINFO;
#endif

class ATL_NO_VTABLE HostNetworkInterfaceIpConfig :
    public VirtualBoxBaseNEXT,
    public VirtualBoxSupportErrorInfoImpl <HostNetworkInterfaceIpConfig, IHostNetworkInterfaceIpConfig>,
    public VirtualBoxSupportTranslation <HostNetworkInterfaceIpConfig>,
    public IHostNetworkInterfaceIpConfig
{
public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT (HostNetworkInterfaceIpConfig)

    DECLARE_NOT_AGGREGATABLE (HostNetworkInterfaceIpConfig)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP (HostNetworkInterfaceIpConfig)
        COM_INTERFACE_ENTRY (ISupportErrorInfo)
        COM_INTERFACE_ENTRY (IHostNetworkInterfaceIpConfig)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    DECLARE_EMPTY_CTOR_DTOR (HostNetworkInterfaceIpConfig)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
#ifdef VBOX_WITH_HOSTNETIF_API
    HRESULT init (struct NETIFINFO *pIfs);
#endif

    // IHostNetworkInterfaceIpConfig properties
    STDMETHOD(COMGETTER(IPAddress)) (ULONG *aIPAddress);
    STDMETHOD(COMGETTER(NetworkMask)) (ULONG *aNetworkMask);
    STDMETHOD(COMGETTER(DefaultGateway)) (ULONG *aNetworkMask);
    STDMETHOD(COMGETTER(IPV6Address)) (BSTR *aIPV6Address);
    STDMETHOD(COMGETTER(IPV6NetworkMask)) (BSTR *aIPV6Mask);

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"HostNetworkInterfaceIpConfig"; }

private:
    ULONG IPAddress;
    ULONG networkMask;
    ULONG defaultGateway;
    Bstr IPV6Address;
    Bstr IPV6NetworkMask;
};

#endif // ____H_HOSTNETWORKINTERFACEIPCONFIGIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
