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

#include "HostNetworkInterfaceImpl.h"
#include "Logging.h"
#include "netif.h"

#ifndef RT_OS_WINDOWS
#include <arpa/inet.h>
#endif /* RT_OS_WINDOWS */

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR (HostNetworkInterface)

HRESULT HostNetworkInterface::FinalConstruct()
{
    return S_OK;
}

void HostNetworkInterface::FinalRelease()
{
    uninit ();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the host object.
 *
 * @returns COM result indicator
 * @param   aInterfaceName name of the network interface
 * @param   aGuid GUID of the host network interface
 */
HRESULT HostNetworkInterface::init (Bstr aInterfaceName, Guid aGuid, HostNetworkInterfaceType_T ifType)
{
    LogFlowThisFunc (("aInterfaceName={%ls}, aGuid={%s}\n",
                      aInterfaceName.raw(), aGuid.toString().raw()));

    ComAssertRet (aInterfaceName, E_INVALIDARG);
    ComAssertRet (!aGuid.isEmpty(), E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_FAIL);

    unconst (mInterfaceName) = aInterfaceName;
    unconst (mGuid) = aGuid;
    mIfType = ifType;


    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

#ifdef VBOX_WITH_HOSTNETIF_API

HRESULT HostNetworkInterface::updateConfig ()
{
    NETIFINFO info;
    int rc = NetIfGetConfig(this, &info);
    if(RT_SUCCESS(rc))
    {
        m.IPAddress = info.IPAddress.u;
        m.networkMask = info.IPNetMask.u;
        m.dhcpEnabled = info.bDhcpEnabled;
        m.IPV6Address = composeIPv6Address(&info.IPv6Address);
        m.IPV6NetworkMaskPrefixLength = composeIPv6PrefixLenghFromAddress(&info.IPv6NetMask);
        m.hardwareAddress = composeHardwareAddress(&info.MACAddress);
#ifdef RT_OS_WINDOWS
        m.mediumType = (HostNetworkInterfaceMediumType)info.enmMediumType;
        m.status = (HostNetworkInterfaceStatus)info.enmStatus;
#else /* !RT_OS_WINDOWS */
        m.mediumType = info.enmMediumType;
        m.status = info.enmStatus;

#endif /* !RT_OS_WINDOWS */
        return S_OK;
    }
    return rc == VERR_NOT_IMPLEMENTED ? E_NOTIMPL : E_FAIL;
}

/**
 * Initializes the host object.
 *
 * @returns COM result indicator
 * @param   aInterfaceName name of the network interface
 * @param   aGuid GUID of the host network interface
 */
HRESULT HostNetworkInterface::init (Bstr aInterfaceName, HostNetworkInterfaceType_T ifType, PNETIFINFO pIf)
{
//    LogFlowThisFunc (("aInterfaceName={%ls}, aGuid={%s}\n",
//                      aInterfaceName.raw(), aGuid.toString().raw()));

//    ComAssertRet (aInterfaceName, E_INVALIDARG);
//    ComAssertRet (!aGuid.isEmpty(), E_INVALIDARG);
    ComAssertRet (pIf, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_FAIL);

    unconst (mInterfaceName) = aInterfaceName;
    unconst (mGuid) = pIf->Uuid;
    mIfType = ifType;

    m.IPAddress = pIf->IPAddress.u;
    m.networkMask = pIf->IPNetMask.u;
    m.IPV6Address = composeIPv6Address(&pIf->IPv6Address);
    m.IPV6NetworkMaskPrefixLength = composeIPv6PrefixLenghFromAddress(&pIf->IPv6NetMask);
    m.dhcpEnabled = pIf->bDhcpEnabled;
    m.hardwareAddress = composeHardwareAddress(&pIf->MACAddress);
#ifdef RT_OS_WINDOWS
    m.mediumType = (HostNetworkInterfaceMediumType)pIf->enmMediumType;
    m.status = (HostNetworkInterfaceStatus)pIf->enmStatus;
#else /* !RT_OS_WINDOWS */
    m.mediumType = pIf->enmMediumType;
    m.status = pIf->enmStatus;
#endif /* !RT_OS_WINDOWS */

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}
#endif

// IHostNetworkInterface properties
/////////////////////////////////////////////////////////////////////////////

/**
 * Returns the name of the host network interface.
 *
 * @returns COM status code
 * @param   aInterfaceName address of result pointer
 */
STDMETHODIMP HostNetworkInterface::COMGETTER(Name) (BSTR *aInterfaceName)
{
    CheckComArgOutPointerValid(aInterfaceName);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    mInterfaceName.cloneTo (aInterfaceName);

    return S_OK;
}

/**
 * Returns the GUID of the host network interface.
 *
 * @returns COM status code
 * @param   aGuid address of result pointer
 */
STDMETHODIMP HostNetworkInterface::COMGETTER(Id) (OUT_GUID aGuid)
{
    CheckComArgOutPointerValid(aGuid);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    mGuid.cloneTo (aGuid);

    return S_OK;
}

STDMETHODIMP HostNetworkInterface::COMGETTER(DhcpEnabled) (BOOL *aDhcpEnabled)
{
    CheckComArgOutPointerValid(aDhcpEnabled);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    *aDhcpEnabled = m.dhcpEnabled;

    return S_OK;
}


/**
 * Returns the IP address of the host network interface.
 *
 * @returns COM status code
 * @param   aIPAddress address of result pointer
 */
STDMETHODIMP HostNetworkInterface::COMGETTER(IPAddress) (BSTR *aIPAddress)
{
    CheckComArgOutPointerValid(aIPAddress);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    if (m.IPAddress == 0)
    {
        Bstr(VBOXNET_IPV4ADDR_DEFAULT).detachTo(aIPAddress);
        return S_OK;
    }

    in_addr tmp;
#if defined(RT_OS_WINDOWS)
    tmp.S_un.S_addr = m.IPAddress;
#else
    tmp.s_addr = m.IPAddress;
#endif
    char *addr = inet_ntoa(tmp);
    if(addr)
    {
        Bstr(addr).detachTo(aIPAddress);
        return S_OK;
    }

    return E_FAIL;
}

/**
 * Returns the netwok mask of the host network interface.
 *
 * @returns COM status code
 * @param   aNetworkMask address of result pointer
 */
STDMETHODIMP HostNetworkInterface::COMGETTER(NetworkMask) (BSTR *aNetworkMask)
{
    CheckComArgOutPointerValid(aNetworkMask);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    if (m.networkMask == 0)
    {
        Bstr(VBOXNET_IPV4MASK_DEFAULT).detachTo(aNetworkMask);
        return S_OK;
    }

    in_addr tmp;
#if defined(RT_OS_WINDOWS)
    tmp.S_un.S_addr = m.networkMask;
#else
    tmp.s_addr = m.networkMask;
#endif
    char *addr = inet_ntoa(tmp);
    if(addr)
    {
        Bstr(addr).detachTo(aNetworkMask);
        return S_OK;
    }

    return E_FAIL;
}

STDMETHODIMP HostNetworkInterface::COMGETTER(IPV6Supported) (BOOL *aIPV6Supported)
{
    CheckComArgOutPointerValid(aIPV6Supported);
#if defined(RT_OS_WINDOWS)
    *aIPV6Supported = FALSE;
#else
    *aIPV6Supported = TRUE;
#endif

    return S_OK;
}

/**
 * Returns the IP V6 address of the host network interface.
 *
 * @returns COM status code
 * @param   aIPV6Address address of result pointer
 */
STDMETHODIMP HostNetworkInterface::COMGETTER(IPV6Address) (BSTR *aIPV6Address)
{
    CheckComArgOutPointerValid(aIPV6Address);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    m.IPV6Address.cloneTo (aIPV6Address);

    return S_OK;
}

/**
 * Returns the IP V6 network mask of the host network interface.
 *
 * @returns COM status code
 * @param   aIPV6Mask address of result pointer
 */
STDMETHODIMP HostNetworkInterface::COMGETTER(IPV6NetworkMaskPrefixLength) (ULONG *aIPV6NetworkMaskPrefixLength)
{
    CheckComArgOutPointerValid(aIPV6NetworkMaskPrefixLength);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    *aIPV6NetworkMaskPrefixLength = m.IPV6NetworkMaskPrefixLength;

    return S_OK;
}

/**
 * Returns the hardware address of the host network interface.
 *
 * @returns COM status code
 * @param   aHardwareAddress address of result pointer
 */
STDMETHODIMP HostNetworkInterface::COMGETTER(HardwareAddress) (BSTR *aHardwareAddress)
{
    CheckComArgOutPointerValid(aHardwareAddress);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    m.hardwareAddress.cloneTo (aHardwareAddress);

    return S_OK;
}

/**
 * Returns the encapsulation protocol type of the host network interface.
 *
 * @returns COM status code
 * @param   aType address of result pointer
 */
STDMETHODIMP HostNetworkInterface::COMGETTER(MediumType) (HostNetworkInterfaceMediumType_T *aType)
{
    CheckComArgOutPointerValid(aType);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    *aType = m.mediumType;

    return S_OK;
}

/**
 * Returns the current state of the host network interface.
 *
 * @returns COM status code
 * @param   aStatus address of result pointer
 */
STDMETHODIMP HostNetworkInterface::COMGETTER(Status) (HostNetworkInterfaceStatus_T *aStatus)
{
    CheckComArgOutPointerValid(aStatus);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    *aStatus = m.status;

    return S_OK;
}

/**
 * Returns network interface type
 *
 * @returns COM status code
 * @param   aType address of result pointer
 */
STDMETHODIMP HostNetworkInterface::COMGETTER(InterfaceType) (HostNetworkInterfaceType_T *aType)
{
    CheckComArgOutPointerValid(aType);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    *aType = mIfType;

    return S_OK;

}

STDMETHODIMP HostNetworkInterface::COMGETTER(NetworkName) (BSTR *aNetworkName)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    Utf8Str utf8Name("HostInterfaceNetworking-");
    utf8Name.append(Utf8Str(mInterfaceName)) ;
    Bstr netName(utf8Name);
    netName.detachTo (aNetworkName);

    return S_OK;
}

STDMETHODIMP HostNetworkInterface::EnableStaticIpConfig (IN_BSTR aIPAddress, IN_BSTR aNetMask)
{
#ifndef VBOX_WITH_HOSTNETIF_API
    return E_NOTIMPL;
#else
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    if (Bstr(aIPAddress).isEmpty())
    {
        if (m.IPAddress)
        {
            int rc = NetIfEnableStaticIpConfig(mVBox, this, m.IPAddress, 0, 0);
            if (RT_SUCCESS(rc))
            {
                if (FAILED(mVBox->SetExtraData(Bstr(Utf8StrFmt("HostOnly/%ls/IPAddress", mInterfaceName.raw())), Bstr(""))))
                    return E_FAIL;
                if (FAILED(mVBox->SetExtraData(Bstr(Utf8StrFmt("HostOnly/%ls/IPNetMask", mInterfaceName.raw())), Bstr(""))))
                    return E_FAIL;
                return S_OK;
            }
        }
        else
            return S_OK;
    }

    ULONG ip, mask;
    ip = inet_addr(Utf8Str(aIPAddress).raw());
    if(ip != INADDR_NONE)
    {
        if (Bstr(aNetMask).isEmpty())
            mask = 0xFFFFFF;
        else
            mask = inet_addr(Utf8Str(aNetMask).raw());
        if(mask != INADDR_NONE)
        {
            if (m.IPAddress == ip && m.networkMask == mask)
                return S_OK;
            int rc = NetIfEnableStaticIpConfig(mVBox, this, m.IPAddress, ip, mask);
            if (RT_SUCCESS(rc))
            {
                if (FAILED(mVBox->SetExtraData(Bstr(Utf8StrFmt("HostOnly/%ls/IPAddress", mInterfaceName.raw())), Bstr(aIPAddress))))
                    return E_FAIL;
                if (FAILED(mVBox->SetExtraData(Bstr(Utf8StrFmt("HostOnly/%ls/IPNetMask", mInterfaceName.raw())), Bstr(aNetMask))))
                    return E_FAIL;
                return S_OK;
            }
            else
            {
                LogRel(("Failed to EnableStaticIpConfig with rc=%Vrc\n", rc));
                return rc == VERR_NOT_IMPLEMENTED ? E_NOTIMPL : E_FAIL;
            }

        }
    }
    return E_FAIL;
#endif
}

STDMETHODIMP HostNetworkInterface::EnableStaticIpConfigV6 (IN_BSTR aIPV6Address, ULONG aIPV6MaskPrefixLength)
{
#ifndef VBOX_WITH_HOSTNETIF_API
    return E_NOTIMPL;
#else
    if (!aIPV6Address)
        return E_INVALIDARG;
    if (aIPV6MaskPrefixLength > 128)
        return E_INVALIDARG;

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    int rc = S_OK;
    if (m.IPV6Address != aIPV6Address || m.IPV6NetworkMaskPrefixLength != aIPV6MaskPrefixLength)
    {
        if (aIPV6MaskPrefixLength == 0)
            aIPV6MaskPrefixLength = 64;
        rc = NetIfEnableStaticIpConfigV6(mVBox, this, m.IPV6Address, aIPV6Address, aIPV6MaskPrefixLength);
        if (RT_FAILURE(rc))
        {
            LogRel(("Failed to EnableStaticIpConfigV6 with rc=%Vrc\n", rc));
            return rc == VERR_NOT_IMPLEMENTED ? E_NOTIMPL : E_FAIL;
        }
        else
        {
            if (FAILED(mVBox->SetExtraData(Bstr(Utf8StrFmt("HostOnly/%ls/IPV6Address", mInterfaceName.raw())), Bstr(aIPV6Address))))
                return E_FAIL;
            if (FAILED(mVBox->SetExtraData(Bstr(Utf8StrFmt("HostOnly/%ls/IPV6NetMask", mInterfaceName.raw())), 
                                           Bstr(Utf8StrFmt("%u", aIPV6MaskPrefixLength)))))
                return E_FAIL;
        }

    }
    return S_OK;
#endif
}

STDMETHODIMP HostNetworkInterface::EnableDynamicIpConfig ()
{
#ifndef VBOX_WITH_HOSTNETIF_API
    return E_NOTIMPL;
#else
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    int rc = NetIfEnableDynamicIpConfig(mVBox, this);
    if (RT_FAILURE(rc))
    {
        LogRel(("Failed to EnableDynamicIpConfig with rc=%Vrc\n", rc));
        return rc == VERR_NOT_IMPLEMENTED ? E_NOTIMPL : E_FAIL;
    }
    return S_OK;
#endif
}

STDMETHODIMP HostNetworkInterface::DhcpRediscover ()
{
#ifndef VBOX_WITH_HOSTNETIF_API
    return E_NOTIMPL;
#else
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    int rc = NetIfDhcpRediscover(mVBox, this);
    if (RT_FAILURE(rc))
    {
        LogRel(("Failed to DhcpRediscover with rc=%Vrc\n", rc));
        return rc == VERR_NOT_IMPLEMENTED ? E_NOTIMPL : E_FAIL;
    }
    return S_OK;
#endif
}

HRESULT HostNetworkInterface::setVirtualBox(VirtualBox *pVBox)
{
    HRESULT hrc;
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());
    mVBox = pVBox;

    /* If IPv4 address hasn't been initialized */
    if (m.IPAddress == 0)
    {
        Bstr tmpAddr, tmpMask;
        hrc = mVBox->GetExtraData(Bstr(Utf8StrFmt("HostOnly/%ls/IPAddress", mInterfaceName.raw())), tmpAddr.asOutParam());
        hrc = mVBox->GetExtraData(Bstr(Utf8StrFmt("HostOnly/%ls/IPNetMask", mInterfaceName.raw())), tmpMask.asOutParam());
        if (tmpAddr.isEmpty())
            tmpAddr = Bstr(VBOXNET_IPV4ADDR_DEFAULT);
        if (tmpMask.isEmpty())
            tmpMask = Bstr(VBOXNET_IPV4MASK_DEFAULT);
        m.IPAddress = inet_addr(Utf8Str(tmpAddr).raw());
        m.networkMask = inet_addr(Utf8Str(tmpMask).raw());
    }

    if (m.IPV6Address.isEmpty())
    {
        Bstr tmpPrefixLen;
        hrc = mVBox->GetExtraData(Bstr(Utf8StrFmt("HostOnly/%ls/IPV6Address", mInterfaceName.raw())), m.IPV6Address.asOutParam());
        if (!m.IPV6Address.isEmpty())
        {
            hrc = mVBox->GetExtraData(Bstr(Utf8StrFmt("HostOnly/%ls/IPV6PrefixLen", mInterfaceName.raw())), tmpPrefixLen.asOutParam());
            if (SUCCEEDED(hrc) && !tmpPrefixLen.isEmpty())
                m.IPV6NetworkMaskPrefixLength = atol(Utf8Str(tmpPrefixLen).raw());
            else
                m.IPV6NetworkMaskPrefixLength = 64;
        }
    }

    return S_OK;
}

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
