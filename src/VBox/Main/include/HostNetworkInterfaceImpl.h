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

#ifndef ____H_HOSTNETWORKINTERFACEIMPL
#define ____H_HOSTNETWORKINTERFACEIMPL

#include "VirtualBoxBase.h"
#include "Collection.h"
#ifdef VBOX_WITH_HOSTNETIF_API
/* class HostNetworkInterface; */
/* #include "netif.h" */
struct NETIFINFO;
#endif

class ATL_NO_VTABLE HostNetworkInterface :
    public VirtualBoxBaseNEXT,
    public VirtualBoxSupportErrorInfoImpl <HostNetworkInterface, IHostNetworkInterface>,
    public VirtualBoxSupportTranslation <HostNetworkInterface>,
    public IHostNetworkInterface
{
public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT (HostNetworkInterface)

    DECLARE_NOT_AGGREGATABLE (HostNetworkInterface)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP (HostNetworkInterface)
        COM_INTERFACE_ENTRY (ISupportErrorInfo)
        COM_INTERFACE_ENTRY (IHostNetworkInterface)
    END_COM_MAP()

    NS_DECL_ISUPPORTS

    DECLARE_EMPTY_CTOR_DTOR (HostNetworkInterface)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (Bstr interfaceName, Guid guid, BOOL aReal);
#ifdef VBOX_WITH_HOSTNETIF_API
    HRESULT init (Bstr aInterfaceName, BOOL aReal, struct NETIFINFO *pIfs);
#endif

    // IHostNetworkInterface properties
    STDMETHOD(COMGETTER(Name)) (BSTR *aInterfaceName);
    STDMETHOD(COMGETTER(Id)) (OUT_GUID aGuid);
    STDMETHOD(COMGETTER(IPAddress)) (ULONG *aIPAddress);
    STDMETHOD(COMGETTER(NetworkMask)) (ULONG *aNetworkMask);
    STDMETHOD(COMGETTER(IPV6Address)) (BSTR *aIPV6Address);
    STDMETHOD(COMGETTER(IPV6NetworkMask)) (BSTR *aIPV6Mask);
    STDMETHOD(COMGETTER(HardwareAddress)) (BSTR *aHardwareAddress);
    STDMETHOD(COMGETTER(Type)) (HostNetworkInterfaceType_T *aType);
    STDMETHOD(COMGETTER(Status)) (HostNetworkInterfaceStatus_T *aStatus);
    STDMETHOD(COMGETTER(Real)) (BOOL *aReal);

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"HostNetworkInterface"; }

private:
    const Bstr mInterfaceName;
    const Guid mGuid;
    BOOL mReal;

    struct Data
    {
        Data() : IPAddress (0), networkMask (0),
            type (HostNetworkInterfaceType_Unknown),
            status(HostNetworkInterfaceStatus_Down){}

        ULONG IPAddress;
        ULONG networkMask;
        Bstr IPV6Address;
        Bstr IPV6NetworkMask;
        Bstr hardwareAddress;
        HostNetworkInterfaceType_T type;
        HostNetworkInterfaceStatus_T status;
    } m;

};

#endif // ____H_H_HOSTNETWORKINTERFACEIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
