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
#include "iprt/netif.h"
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
    HRESULT init (Bstr interfaceName, Guid guid);
#ifdef VBOX_WITH_HOSTNETIF_API
    HRESULT init (PRTNETIFINFO pIfs);
#endif

    // IHostNetworkInterface properties
    STDMETHOD(COMGETTER(Name)) (BSTR *aInterfaceName);
    STDMETHOD(COMGETTER(Id)) (OUT_GUID aGuid);
    STDMETHOD(COMGETTER(IPAddress)) (ULONG *aIPAddress);
    STDMETHOD(COMGETTER(NetworkMask)) (ULONG *aNetworkMask);
    STDMETHOD(COMGETTER(IPV6Address)) (BSTR *aIPV6Address);
    STDMETHOD(COMGETTER(HardwareAddress)) (BSTR *aHardwareAddress);
    STDMETHOD(COMGETTER(Type)) (HostNetworkInterfaceType_T *aType);
    STDMETHOD(COMGETTER(Status)) (HostNetworkInterfaceStatus_T *aStatus);

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"HostNetworkInterface"; }

private:
    const Bstr mInterfaceName;
    const Guid mGuid;
    struct Data
    {
        Data() : IPAddress (0), networkMask (0),
            type (HostNetworkInterfaceType_Unknown),
            status(HostNetworkInterfaceStatus_Down) {}
        
        ULONG IPAddress;
        ULONG networkMask;
        Bstr IPV6Address;
        Bstr hardwareAddress;
        HostNetworkInterfaceType_T type;
        HostNetworkInterfaceStatus_T status;
    } m;

};

COM_DECL_READONLY_ENUM_AND_COLLECTION_BEGIN (HostNetworkInterface)

    STDMETHOD(FindByName) (IN_BSTR name, IHostNetworkInterface **networkInterface)
    {
        if (!name)
            return E_INVALIDARG;
        if (!networkInterface)
            return E_POINTER;

        *networkInterface = NULL;
        Vector::value_type found;
        Vector::iterator it = vec.begin();
        while (it != vec.end() && !found)
        {
            Bstr n;
            (*it)->COMGETTER(Name) (n.asOutParam());
            if (n == name)
                found = *it;
            ++ it;
        }

        if (!found)
            return setError (E_INVALIDARG, HostNetworkInterfaceCollection::tr (
                "The host network interface with the given name could not be found"));

        return found.queryInterfaceTo (networkInterface);
    }

    STDMETHOD(FindById) (IN_GUID id, IHostNetworkInterface **networkInterface)
    {
        if (Guid(id).isEmpty())
            return E_INVALIDARG;
        if (!networkInterface)
            return E_POINTER;

        *networkInterface = NULL;
        Vector::value_type found;
        Vector::iterator it = vec.begin();
        while (it != vec.end() && !found)
        {
            Guid g;
            (*it)->COMGETTER(Id) (g.asOutParam());
            if (g == Guid(id))
                found = *it;
            ++ it;
        }

        if (!found)
            return setError (E_INVALIDARG, HostNetworkInterfaceCollection::tr (
                "The host network interface with the given GUID could not be found"));

        return found.queryInterfaceTo (networkInterface);
    }


COM_DECL_READONLY_ENUM_AND_COLLECTION_END (HostNetworkInterface)


#endif // ____H_H_HOSTNETWORKINTERFACEIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
