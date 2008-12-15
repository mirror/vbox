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
HRESULT HostNetworkInterface::init (Bstr aInterfaceName, Guid aGuid)
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

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

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

static Bstr composeHardwareAddress(PRTMAC aMacPtr)
{
    char szTmp[6*3];

    RTStrPrintf(szTmp, sizeof(szTmp),
                "%02x:%02x:%02x:%02x:%02x:%02x",
                aMacPtr->au8[0], aMacPtr->au8[1],
                aMacPtr->au8[2], aMacPtr->au8[3],
                aMacPtr->au8[4], aMacPtr->au8[5]);
    return Bstr(szTmp);
}

/**
 * Initializes the host object.
 *
 * @returns COM result indicator
 * @param   aInterfaceName name of the network interface
 * @param   aGuid GUID of the host network interface
 */
HRESULT HostNetworkInterface::init (Bstr aInterfaceName, PNETIFINFO pIf)
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
    m.IPAddress = pIf->IPAddress.u;
    m.networkMask = pIf->IPNetMask.u;
    m.IPV6Address = composeIPv6Address(&pIf->IPv6Address);
    m.IPV6NetworkMask = composeIPv6Address(&pIf->IPv6NetMask);
    m.hardwareAddress = composeHardwareAddress(&pIf->MACAddress);
#ifdef RT_OS_WINDOWS
    m.type = (HostNetworkInterfaceType)pIf->enmType;
    m.status = (HostNetworkInterfaceStatus)pIf->enmStatus;
#else /* !RT_OS_WINDOWS */
    m.type = pIf->enmType;
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


/**
 * Returns the IP address of the host network interface.
 *
 * @returns COM status code
 * @param   aIPAddress address of result pointer
 */
STDMETHODIMP HostNetworkInterface::COMGETTER(IPAddress) (ULONG *aIPAddress)
{
    CheckComArgOutPointerValid(aIPAddress);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    *aIPAddress = m.IPAddress;

    return S_OK;
}

/**
 * Returns the netwok mask of the host network interface.
 *
 * @returns COM status code
 * @param   aNetworkMask address of result pointer
 */
STDMETHODIMP HostNetworkInterface::COMGETTER(NetworkMask) (ULONG *aNetworkMask)
{
    CheckComArgOutPointerValid(aNetworkMask);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    *aNetworkMask = m.networkMask;

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
STDMETHODIMP HostNetworkInterface::COMGETTER(IPV6NetworkMask) (BSTR *aIPV6Mask)
{
    CheckComArgOutPointerValid(aIPV6Mask);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    m.IPV6NetworkMask.cloneTo (aIPV6Mask);

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
STDMETHODIMP HostNetworkInterface::COMGETTER(Type) (HostNetworkInterfaceType_T *aType)
{
    CheckComArgOutPointerValid(aType);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    *aType = m.type;

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

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
