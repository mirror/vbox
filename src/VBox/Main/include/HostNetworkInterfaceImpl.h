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

#ifndef ____H_HOSTNETWORKINTERFACEIMPL
#define ____H_HOSTNETWORKINTERFACEIMPL

#include "VirtualBoxBase.h"
#include "VirtualBoxImpl.h"

#ifdef VBOX_WITH_HOSTNETIF_API
/* class HostNetworkInterface; */
/* #include "netif.h" */
struct NETIFINFO;
#endif

class ATL_NO_VTABLE HostNetworkInterface :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IHostNetworkInterface)
{
public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(HostNetworkInterface, IHostNetworkInterface)

    DECLARE_NOT_AGGREGATABLE(HostNetworkInterface)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(HostNetworkInterface)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IHostNetworkInterface)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR(HostNetworkInterface)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(Bstr interfaceName, Bstr shortName, Guid guid, HostNetworkInterfaceType_T ifType);
#ifdef VBOX_WITH_HOSTNETIF_API
    HRESULT init(Bstr aInterfaceName, HostNetworkInterfaceType_T ifType, struct NETIFINFO *pIfs);
    HRESULT updateConfig();
#endif

    // IHostNetworkInterface properties
    STDMETHOD(COMGETTER(Name))(BSTR *aInterfaceName);
    STDMETHOD(COMGETTER(Id))(BSTR *aGuid);
    STDMETHOD(COMGETTER(DHCPEnabled))(BOOL *aDHCPEnabled);
    STDMETHOD(COMGETTER(IPAddress))(BSTR *aIPAddress);
    STDMETHOD(COMGETTER(NetworkMask))(BSTR *aNetworkMask);
    STDMETHOD(COMGETTER(IPV6Supported))(BOOL *aIPV6Supported);
    STDMETHOD(COMGETTER(IPV6Address))(BSTR *aIPV6Address);
    STDMETHOD(COMGETTER(IPV6NetworkMaskPrefixLength))(ULONG *aIPV6NetworkMaskPrefixLength);
    STDMETHOD(COMGETTER(HardwareAddress))(BSTR *aHardwareAddress);
    STDMETHOD(COMGETTER(MediumType))(HostNetworkInterfaceMediumType_T *aType);
    STDMETHOD(COMGETTER(Status))(HostNetworkInterfaceStatus_T *aStatus);
    STDMETHOD(COMGETTER(InterfaceType))(HostNetworkInterfaceType_T *aType);
    STDMETHOD(COMGETTER(NetworkName))(BSTR *aNetworkName);

    STDMETHOD(EnableStaticIPConfig)(IN_BSTR aIPAddress, IN_BSTR aNetworkMask);
    STDMETHOD(EnableStaticIPConfigV6)(IN_BSTR aIPV6Address, ULONG aIPV6MaskPrefixLength);
    STDMETHOD(EnableDynamicIPConfig)();
    STDMETHOD(DHCPRediscover)();

    HRESULT setVirtualBox(VirtualBox *pVBox);
    void registerMetrics(PerformanceCollector *aCollector, ComPtr<IUnknown> objptr);
    void unregisterMetrics(PerformanceCollector *aCollector, ComPtr<IUnknown> objptr);

private:
    Bstr composeNetworkName(const Utf8Str szShortName);

    const Bstr mInterfaceName;
    const Guid mGuid;
    const Bstr mNetworkName;
    HostNetworkInterfaceType_T mIfType;

    VirtualBox * const  mVBox;

    struct Data
    {
        Data() : IPAddress(0), networkMask(0), dhcpEnabled(FALSE),
            mediumType(HostNetworkInterfaceMediumType_Unknown),
            status(HostNetworkInterfaceStatus_Down){}

        ULONG IPAddress;
        ULONG networkMask;
        Bstr IPV6Address;
        ULONG IPV6NetworkMaskPrefixLength;
        ULONG realIPAddress;
        ULONG realNetworkMask;
        Bstr  realIPV6Address;
        ULONG realIPV6PrefixLength;
        BOOL dhcpEnabled;
        Bstr hardwareAddress;
        HostNetworkInterfaceMediumType_T mediumType;
        HostNetworkInterfaceStatus_T status;
        ULONG speedMbytes;
    } m;

};

typedef std::list<ComObjPtr<HostNetworkInterface> > HostNetworkInterfaceList;

#endif // ____H_H_HOSTNETWORKINTERFACEIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
