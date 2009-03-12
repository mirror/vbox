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

    in_addr tmp;
    tmp.S_un.S_addr = m.IPAddress;
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

    in_addr tmp;
    tmp.S_un.S_addr = m.networkMask;
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

STDMETHODIMP HostNetworkInterface::EnableStaticIpConfig (IN_BSTR aIPAddress, IN_BSTR aNetMask)
{
#ifndef VBOX_WITH_HOSTNETIF_API
    return E_NOTIMPL;
#else
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    ULONG ip, mask;
    ip = inet_addr(Utf8Str(aIPAddress).raw());
    if(ip != INADDR_NONE)
    {
        mask = inet_addr(Utf8Str(aNetMask).raw());
        if(mask != INADDR_NONE)
        {
            int rc = NetIfEnableStaticIpConfig(mVBox, this, ip, mask);
            if (RT_SUCCESS(rc))
            {
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

    int rc = NetIfEnableStaticIpConfigV6(mVBox, this, aIPV6Address, aIPV6MaskPrefixLength);
    if (RT_FAILURE(rc))
    {
        LogRel(("Failed to EnableStaticIpConfigV6 with rc=%Vrc\n", rc));
        return rc == VERR_NOT_IMPLEMENTED ? E_NOTIMPL : E_FAIL;
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
        LogRel(("Failed to EnableStaticIpConfigV6 with rc=%Vrc\n", rc));
        return rc == VERR_NOT_IMPLEMENTED ? E_NOTIMPL : E_FAIL;
    }
    return S_OK;
#endif
}

HRESULT HostNetworkInterface::setVirtualBox(VirtualBox *pVBox)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());
    mVBox = pVBox;

    return S_OK;
}

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
