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

#include "HostNetworkInterfaceIpConfigImpl.h"
#include "HostNetworkInterfaceImpl.h" /* needed by netif.h */
#include "Logging.h"
#include "netif.h"

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR (HostNetworkInterfaceIpConfig)

HRESULT HostNetworkInterfaceIpConfig::FinalConstruct()
{
    return S_OK;
}

void HostNetworkInterfaceIpConfig::FinalRelease()
{
    uninit ();
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

#ifdef VBOX_WITH_HOSTNETIF_API
static Bstr composeIPv6Address(PRTNETADDRIPV6 aAddrPtr)
{
    char szTmp[8*5];

    RTStrPrintf(szTmp, sizeof(szTmp),
                "%02x%02x:%02x%02x:%02x%02x:%02x%02x:"
                "%02x%02x:%02x%02x:%02x%02x:%02x%02x",
                aAddrPtr->au8[0], aAddrPtr->au8[1],
                aAddrPtr->au8[2], aAddrPtr->au8[3],
                aAddrPtr->au8[4], aAddrPtr->au8[5],
                aAddrPtr->au8[6], aAddrPtr->au8[7],
                aAddrPtr->au8[8], aAddrPtr->au8[9],
                aAddrPtr->au8[10], aAddrPtr->au8[11],
                aAddrPtr->au8[12], aAddrPtr->au8[13],
                aAddrPtr->au8[14], aAddrPtr->au8[15]);
    return Bstr(szTmp);
}

/**
 * Initializes the host object.
 *
 * @returns COM result indicator
 * @param   aInterfaceName name of the network interface
 * @param   aGuid GUID of the host network interface
 */
HRESULT HostNetworkInterfaceIpConfig::init (PNETIFINFO pIf)
{
//    LogFlowThisFunc (("aInterfaceName={%ls}, aGuid={%s}\n",
//                      aInterfaceName.raw(), aGuid.toString().raw()));

//    ComAssertRet (aInterfaceName, E_INVALIDARG);
//    ComAssertRet (!aGuid.isEmpty(), E_INVALIDARG);
    ComAssertRet (pIf, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_FAIL);

    IPAddress = pIf->IPAddress.u;
    networkMask = pIf->IPNetMask.u;
    defaultGateway = pIf->IPDefaultGateway.u;
    IPV6Address = composeIPv6Address(&pIf->IPv6Address);
    IPV6NetworkMask = composeIPv6Address(&pIf->IPv6NetMask);

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}
#endif

// IHostNetworkInterfaceIpConfig properties
/////////////////////////////////////////////////////////////////////////////
/**
 * Returns the IP address of the host network interface.
 *
 * @returns COM status code
 * @param   aIPAddress address of result pointer
 */
STDMETHODIMP HostNetworkInterfaceIpConfig::COMGETTER(IPAddress) (ULONG *aIPAddress)
{
    CheckComArgOutPointerValid(aIPAddress);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    *aIPAddress = IPAddress;

    return S_OK;
}

/**
 * Returns the netwok mask of the host network interface.
 *
 * @returns COM status code
 * @param   aNetworkMask address of result pointer
 */
STDMETHODIMP HostNetworkInterfaceIpConfig::COMGETTER(NetworkMask) (ULONG *aNetworkMask)
{
    CheckComArgOutPointerValid(aNetworkMask);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    *aNetworkMask = networkMask;

    return S_OK;
}

/**
 * Returns the netwok mask of the host network interface.
 *
 * @returns COM status code
 * @param   aNetworkMask address of result pointer
 */
STDMETHODIMP HostNetworkInterfaceIpConfig::COMGETTER(DefaultGateway) (ULONG *aDefaultGateway)
{
    CheckComArgOutPointerValid(aDefaultGateway);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    *aDefaultGateway = defaultGateway;

    return S_OK;
}

/**
 * Returns the IP V6 address of the host network interface.
 *
 * @returns COM status code
 * @param   aIPV6Address address of result pointer
 */
STDMETHODIMP HostNetworkInterfaceIpConfig::COMGETTER(IPV6Address) (BSTR *aIPV6Address)
{
    CheckComArgOutPointerValid(aIPV6Address);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    IPV6Address.cloneTo (aIPV6Address);

    return S_OK;
}

/**
 * Returns the IP V6 network mask of the host network interface.
 *
 * @returns COM status code
 * @param   aIPV6Mask address of result pointer
 */
STDMETHODIMP HostNetworkInterfaceIpConfig::COMGETTER(IPV6NetworkMask) (BSTR *aIPV6Mask)
{
    CheckComArgOutPointerValid(aIPV6Mask);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    IPV6NetworkMask.cloneTo (aIPV6Mask);

    return S_OK;
}
