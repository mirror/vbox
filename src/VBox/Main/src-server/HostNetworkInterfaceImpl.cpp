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

#include "HostNetworkInterfaceImpl.h"
#include "AutoCaller.h"
#include "Logging.h"
#include "netif.h"
#include "Performance.h"
#include "PerformanceImpl.h"

#include <iprt/cpp/utils.h>

#ifdef RT_OS_FREEBSD
# include <netinet/in.h> /* INADDR_NONE */
#endif /* RT_OS_FREEBSD */

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

HostNetworkInterface::HostNetworkInterface()
    : mVBox(NULL)
{
}

HostNetworkInterface::~HostNetworkInterface()
{
}

HRESULT HostNetworkInterface::FinalConstruct()
{
    return BaseFinalConstruct();
}

void HostNetworkInterface::FinalRelease()
{
    uninit();
    BaseFinalRelease();
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
HRESULT HostNetworkInterface::init(Bstr aInterfaceName, Bstr aShortName, Guid aGuid, HostNetworkInterfaceType_T ifType)
{
    LogFlowThisFunc(("aInterfaceName={%ls}, aGuid={%s}\n",
                      aInterfaceName.raw(), aGuid.toString().c_str()));

    ComAssertRet(!aInterfaceName.isEmpty(), E_INVALIDARG);
    ComAssertRet(!aGuid.isValid(), E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    unconst(mInterfaceName) = aInterfaceName;
    unconst(mNetworkName) = composeNetworkName(aShortName);
    unconst(mGuid) = aGuid;
    mIfType = ifType;

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

void HostNetworkInterface::registerMetrics(PerformanceCollector *aCollector, ComPtr<IUnknown> objptr)
{
    LogFlowThisFunc(("mShortName={%ls}, mInterfaceName={%ls}, mGuid={%s}, mSpeedMbits=%u\n",
                     mShortName.raw(), mInterfaceName.raw(), mGuid.toString().c_str(), m.speedMbits));
    pm::CollectorHAL *hal = aCollector->getHAL();
    /* Create sub metrics */
    Utf8StrFmt strName("Net/%ls", mShortName.raw());
    pm::SubMetric *networkLoadRx   = new pm::SubMetric(strName + "/Load/Rx",
        "Percentage of network interface receive bandwidth used.");
    pm::SubMetric *networkLoadTx   = new pm::SubMetric(strName + "/Load/Tx",
        "Percentage of network interface transmit bandwidth used.");
    pm::SubMetric *networkLinkSpeed = new pm::SubMetric(strName + "/LinkSpeed",
        "Physical link speed.");

    /* Create and register base metrics */
    pm::BaseMetric *networkSpeed = new pm::HostNetworkSpeed(hal, objptr, strName + "/LinkSpeed", Utf8Str(mShortName), Utf8Str(mInterfaceName), m.speedMbits, networkLinkSpeed);
    aCollector->registerBaseMetric(networkSpeed);
    pm::BaseMetric *networkLoad = new pm::HostNetworkLoadRaw(hal, objptr, strName + "/Load", Utf8Str(mShortName), Utf8Str(mInterfaceName), m.speedMbits, networkLoadRx, networkLoadTx);
    aCollector->registerBaseMetric(networkLoad);

    aCollector->registerMetric(new pm::Metric(networkSpeed, networkLinkSpeed, 0));
    aCollector->registerMetric(new pm::Metric(networkSpeed, networkLinkSpeed,
                                              new pm::AggregateAvg()));
    aCollector->registerMetric(new pm::Metric(networkSpeed, networkLinkSpeed,
                                              new pm::AggregateMin()));
    aCollector->registerMetric(new pm::Metric(networkSpeed, networkLinkSpeed,
                                              new pm::AggregateMax()));

    aCollector->registerMetric(new pm::Metric(networkLoad, networkLoadRx, 0));
    aCollector->registerMetric(new pm::Metric(networkLoad, networkLoadRx,
                                              new pm::AggregateAvg()));
    aCollector->registerMetric(new pm::Metric(networkLoad, networkLoadRx,
                                              new pm::AggregateMin()));
    aCollector->registerMetric(new pm::Metric(networkLoad, networkLoadRx,
                                              new pm::AggregateMax()));

    aCollector->registerMetric(new pm::Metric(networkLoad, networkLoadTx, 0));
    aCollector->registerMetric(new pm::Metric(networkLoad, networkLoadTx,
                                              new pm::AggregateAvg()));
    aCollector->registerMetric(new pm::Metric(networkLoad, networkLoadTx,
                                              new pm::AggregateMin()));
    aCollector->registerMetric(new pm::Metric(networkLoad, networkLoadTx,
                                              new pm::AggregateMax()));
}

void HostNetworkInterface::unregisterMetrics(PerformanceCollector *aCollector, ComPtr<IUnknown> objptr)
{
    LogFlowThisFunc(("mShortName={%ls}, mInterfaceName={%ls}, mGuid={%s}\n",
                     mShortName.raw(), mInterfaceName.raw(), mGuid.toString().c_str()));
    Utf8StrFmt name("Net/%ls", mShortName.raw());
    aCollector->unregisterMetricsFor(objptr, name + "/*");
    aCollector->unregisterBaseMetricsFor(objptr, name);
}

#ifdef VBOX_WITH_HOSTNETIF_API

HRESULT HostNetworkInterface::updateConfig()
{
    NETIFINFO info;
    int rc = NetIfGetConfig(this, &info);
    if (RT_SUCCESS(rc))
    {
        m.realIPAddress = m.IPAddress = info.IPAddress.u;
        m.realNetworkMask = m.networkMask = info.IPNetMask.u;
        m.dhcpEnabled = info.bDhcpEnabled;
        m.realIPV6Address = m.IPV6Address = composeIPv6Address(&info.IPv6Address);
        m.realIPV6PrefixLength = m.IPV6NetworkMaskPrefixLength = composeIPv6PrefixLenghFromAddress(&info.IPv6NetMask);
        m.hardwareAddress = composeHardwareAddress(&info.MACAddress);
#ifdef RT_OS_WINDOWS
        m.mediumType = (HostNetworkInterfaceMediumType)info.enmMediumType;
        m.status = (HostNetworkInterfaceStatus)info.enmStatus;
#else /* !RT_OS_WINDOWS */
        m.mediumType = info.enmMediumType;
        m.status = info.enmStatus;
#endif /* !RT_OS_WINDOWS */
        m.speedMbits = info.uSpeedMbits;
        return S_OK;
    }
    return rc == VERR_NOT_IMPLEMENTED ? E_NOTIMPL : E_FAIL;
}

Bstr HostNetworkInterface::composeNetworkName(const Utf8Str aShortName)
{
    return Utf8Str("HostInterfaceNetworking-").append(aShortName);
}
/**
 * Initializes the host object.
 *
 * @returns COM result indicator
 * @param   aInterfaceName name of the network interface
 * @param   aGuid GUID of the host network interface
 */
HRESULT HostNetworkInterface::init(Bstr aInterfaceName, HostNetworkInterfaceType_T ifType, PNETIFINFO pIf)
{
//    LogFlowThisFunc(("aInterfaceName={%ls}, aGuid={%s}\n",
//                      aInterfaceName.raw(), aGuid.toString().raw()));

//    ComAssertRet(aInterfaceName, E_INVALIDARG);
//    ComAssertRet(aGuid.isValid(), E_INVALIDARG);
    ComAssertRet(pIf, E_INVALIDARG);

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    unconst(mInterfaceName) = aInterfaceName;
    unconst(mGuid) = pIf->Uuid;
    if (pIf->szShortName[0])
    {
        unconst(mNetworkName) = composeNetworkName(pIf->szShortName);
        unconst(mShortName)   = pIf->szShortName;
    }
    else
    {
        unconst(mNetworkName) = composeNetworkName(aInterfaceName);
        unconst(mShortName)   = aInterfaceName;
    }
    mIfType = ifType;

    m.realIPAddress = m.IPAddress = pIf->IPAddress.u;
    m.realNetworkMask = m.networkMask = pIf->IPNetMask.u;
    m.realIPV6Address = m.IPV6Address = composeIPv6Address(&pIf->IPv6Address);
    m.realIPV6PrefixLength = m.IPV6NetworkMaskPrefixLength = composeIPv6PrefixLenghFromAddress(&pIf->IPv6NetMask);
    m.dhcpEnabled = pIf->bDhcpEnabled;
    m.hardwareAddress = composeHardwareAddress(&pIf->MACAddress);
#ifdef RT_OS_WINDOWS
    m.mediumType = (HostNetworkInterfaceMediumType)pIf->enmMediumType;
    m.status = (HostNetworkInterfaceStatus)pIf->enmStatus;
#else /* !RT_OS_WINDOWS */
    m.mediumType = pIf->enmMediumType;
    m.status = pIf->enmStatus;
#endif /* !RT_OS_WINDOWS */
    m.speedMbits = pIf->uSpeedMbits;

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
STDMETHODIMP HostNetworkInterface::COMGETTER(Name)(BSTR *aInterfaceName)
{
    CheckComArgOutPointerValid(aInterfaceName);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    mInterfaceName.cloneTo(aInterfaceName);

    return S_OK;
}

/**
 * Returns the GUID of the host network interface.
 *
 * @returns COM status code
 * @param   aGuid address of result pointer
 */
STDMETHODIMP HostNetworkInterface::COMGETTER(Id)(BSTR *aGuid)
{
    CheckComArgOutPointerValid(aGuid);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    mGuid.toUtf16().cloneTo(aGuid);

    return S_OK;
}

STDMETHODIMP HostNetworkInterface::COMGETTER(DHCPEnabled)(BOOL *aDHCPEnabled)
{
    CheckComArgOutPointerValid(aDHCPEnabled);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    *aDHCPEnabled = m.dhcpEnabled;

    return S_OK;
}


/**
 * Returns the IP address of the host network interface.
 *
 * @returns COM status code
 * @param   aIPAddress address of result pointer
 */
STDMETHODIMP HostNetworkInterface::COMGETTER(IPAddress)(BSTR *aIPAddress)
{
    CheckComArgOutPointerValid(aIPAddress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    in_addr tmp;
#if defined(RT_OS_WINDOWS)
    tmp.S_un.S_addr = m.IPAddress;
#else
    tmp.s_addr = m.IPAddress;
#endif
    char *addr = inet_ntoa(tmp);
    if (addr)
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
STDMETHODIMP HostNetworkInterface::COMGETTER(NetworkMask)(BSTR *aNetworkMask)
{
    CheckComArgOutPointerValid(aNetworkMask);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    in_addr tmp;
#if defined(RT_OS_WINDOWS)
    tmp.S_un.S_addr = m.networkMask;
#else
    tmp.s_addr = m.networkMask;
#endif
    char *addr = inet_ntoa(tmp);
    if (addr)
    {
        Bstr(addr).detachTo(aNetworkMask);
        return S_OK;
    }

    return E_FAIL;
}

STDMETHODIMP HostNetworkInterface::COMGETTER(IPV6Supported)(BOOL *aIPV6Supported)
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
STDMETHODIMP HostNetworkInterface::COMGETTER(IPV6Address)(BSTR *aIPV6Address)
{
    CheckComArgOutPointerValid(aIPV6Address);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    m.IPV6Address.cloneTo(aIPV6Address);

    return S_OK;
}

/**
 * Returns the IP V6 network mask of the host network interface.
 *
 * @returns COM status code
 * @param   aIPV6Mask address of result pointer
 */
STDMETHODIMP HostNetworkInterface::COMGETTER(IPV6NetworkMaskPrefixLength)(ULONG *aIPV6NetworkMaskPrefixLength)
{
    CheckComArgOutPointerValid(aIPV6NetworkMaskPrefixLength);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    *aIPV6NetworkMaskPrefixLength = m.IPV6NetworkMaskPrefixLength;

    return S_OK;
}

/**
 * Returns the hardware address of the host network interface.
 *
 * @returns COM status code
 * @param   aHardwareAddress address of result pointer
 */
STDMETHODIMP HostNetworkInterface::COMGETTER(HardwareAddress)(BSTR *aHardwareAddress)
{
    CheckComArgOutPointerValid(aHardwareAddress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    m.hardwareAddress.cloneTo(aHardwareAddress);

    return S_OK;
}

/**
 * Returns the encapsulation protocol type of the host network interface.
 *
 * @returns COM status code
 * @param   aType address of result pointer
 */
STDMETHODIMP HostNetworkInterface::COMGETTER(MediumType)(HostNetworkInterfaceMediumType_T *aType)
{
    CheckComArgOutPointerValid(aType);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    *aType = m.mediumType;

    return S_OK;
}

/**
 * Returns the current state of the host network interface.
 *
 * @returns COM status code
 * @param   aStatus address of result pointer
 */
STDMETHODIMP HostNetworkInterface::COMGETTER(Status)(HostNetworkInterfaceStatus_T *aStatus)
{
    CheckComArgOutPointerValid(aStatus);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    *aStatus = m.status;

    return S_OK;
}

/**
 * Returns network interface type
 *
 * @returns COM status code
 * @param   aType address of result pointer
 */
STDMETHODIMP HostNetworkInterface::COMGETTER(InterfaceType)(HostNetworkInterfaceType_T *aType)
{
    CheckComArgOutPointerValid(aType);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    *aType = mIfType;

    return S_OK;

}

STDMETHODIMP HostNetworkInterface::COMGETTER(NetworkName)(BSTR *aNetworkName)
{
    CheckComArgOutPointerValid(aNetworkName);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    mNetworkName.cloneTo(aNetworkName);

    return S_OK;
}

STDMETHODIMP HostNetworkInterface::EnableStaticIPConfig(IN_BSTR aIPAddress, IN_BSTR aNetMask)
{
#ifndef VBOX_WITH_HOSTNETIF_API
    return E_NOTIMPL;
#else
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    if (Bstr(aIPAddress).isEmpty())
    {
        if (m.IPAddress)
        {
            int rc = NetIfEnableStaticIpConfig(mVBox, this, m.IPAddress, 0, 0);
            if (RT_SUCCESS(rc))
            {
                m.realIPAddress = 0;
                if (FAILED(mVBox->SetExtraData(BstrFmt("HostOnly/%ls/IPAddress", mInterfaceName.raw()).raw(), NULL)))
                    return E_FAIL;
                if (FAILED(mVBox->SetExtraData(BstrFmt("HostOnly/%ls/IPNetMask", mInterfaceName.raw()).raw(), NULL)))
                    return E_FAIL;
                return S_OK;
            }
        }
        else
            return S_OK;
    }

    ULONG ip, mask;
    ip = inet_addr(Utf8Str(aIPAddress).c_str());
    if (ip != INADDR_NONE)
    {
        if (Bstr(aNetMask).isEmpty())
            mask = 0xFFFFFF;
        else
            mask = inet_addr(Utf8Str(aNetMask).c_str());
        if (mask != INADDR_NONE)
        {
            if (m.realIPAddress == ip && m.realNetworkMask == mask)
                return S_OK;
            int rc = NetIfEnableStaticIpConfig(mVBox, this, m.IPAddress, ip, mask);
            if (RT_SUCCESS(rc))
            {
                m.realIPAddress   = ip;
                m.realNetworkMask = mask;
                if (FAILED(mVBox->SetExtraData(BstrFmt("HostOnly/%ls/IPAddress", mInterfaceName.raw()).raw(),
                                                       Bstr(aIPAddress).raw())))
                    return E_FAIL;
                if (FAILED(mVBox->SetExtraData(BstrFmt("HostOnly/%ls/IPNetMask", mInterfaceName.raw()).raw(),
                                               Bstr(aNetMask).raw())))
                    return E_FAIL;
                return S_OK;
            }
            else
            {
                LogRel(("Failed to EnableStaticIpConfig with rc=%Rrc\n", rc));
                return rc == VERR_NOT_IMPLEMENTED ? E_NOTIMPL : E_FAIL;
            }

        }
    }
    return E_FAIL;
#endif
}

STDMETHODIMP HostNetworkInterface::EnableStaticIPConfigV6(IN_BSTR aIPV6Address, ULONG aIPV6MaskPrefixLength)
{
#ifndef VBOX_WITH_HOSTNETIF_API
    return E_NOTIMPL;
#else
    if (!aIPV6Address)
        return E_INVALIDARG;
    if (aIPV6MaskPrefixLength > 128)
        return E_INVALIDARG;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    int rc = S_OK;
    if (m.realIPV6Address != aIPV6Address || m.realIPV6PrefixLength != aIPV6MaskPrefixLength)
    {
        if (aIPV6MaskPrefixLength == 0)
            aIPV6MaskPrefixLength = 64;
        rc = NetIfEnableStaticIpConfigV6(mVBox, this, m.IPV6Address.raw(), aIPV6Address, aIPV6MaskPrefixLength);
        if (RT_FAILURE(rc))
        {
            LogRel(("Failed to EnableStaticIpConfigV6 with rc=%Rrc\n", rc));
            return rc == VERR_NOT_IMPLEMENTED ? E_NOTIMPL : E_FAIL;
        }
        else
        {
            m.realIPV6Address = aIPV6Address;
            m.realIPV6PrefixLength = aIPV6MaskPrefixLength;
            if (FAILED(mVBox->SetExtraData(BstrFmt("HostOnly/%ls/IPV6Address", mInterfaceName.raw()).raw(),
                                           Bstr(aIPV6Address).raw())))
                return E_FAIL;
            if (FAILED(mVBox->SetExtraData(BstrFmt("HostOnly/%ls/IPV6NetMask", mInterfaceName.raw()).raw(),
                                           BstrFmt("%u", aIPV6MaskPrefixLength).raw())))
                return E_FAIL;
        }

    }
    return S_OK;
#endif
}

STDMETHODIMP HostNetworkInterface::EnableDynamicIPConfig()
{
#ifndef VBOX_WITH_HOSTNETIF_API
    return E_NOTIMPL;
#else
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    int rc = NetIfEnableDynamicIpConfig(mVBox, this);
    if (RT_FAILURE(rc))
    {
        LogRel(("Failed to EnableDynamicIpConfig with rc=%Rrc\n", rc));
        return rc == VERR_NOT_IMPLEMENTED ? E_NOTIMPL : E_FAIL;
    }
    return S_OK;
#endif
}

STDMETHODIMP HostNetworkInterface::DHCPRediscover()
{
#ifndef VBOX_WITH_HOSTNETIF_API
    return E_NOTIMPL;
#else
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    int rc = NetIfDhcpRediscover(mVBox, this);
    if (RT_FAILURE(rc))
    {
        LogRel(("Failed to DhcpRediscover with rc=%Rrc\n", rc));
        return rc == VERR_NOT_IMPLEMENTED ? E_NOTIMPL : E_FAIL;
    }
    return S_OK;
#endif
}

HRESULT HostNetworkInterface::setVirtualBox(VirtualBox *pVBox)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();
    AssertReturn(mVBox != pVBox, S_OK);

    unconst(mVBox) = pVBox;

#if !defined(RT_OS_WINDOWS)
    /* If IPv4 address hasn't been initialized */
    if (m.IPAddress == 0 && mIfType == HostNetworkInterfaceType_HostOnly)
    {
        Bstr tmpAddr, tmpMask;
        HRESULT hrc = mVBox->GetExtraData(BstrFmt("HostOnly/%ls/IPAddress", mInterfaceName.raw()).raw(),
                                          tmpAddr.asOutParam());
        if (FAILED(hrc) || tmpAddr.isEmpty())
            tmpAddr = getDefaultIPv4Address(mInterfaceName);

        hrc = mVBox->GetExtraData(BstrFmt("HostOnly/%ls/IPNetMask", mInterfaceName.raw()).raw(),
                                  tmpMask.asOutParam());
        if (FAILED(hrc) || tmpMask.isEmpty())
            tmpMask = Bstr(VBOXNET_IPV4MASK_DEFAULT);

        m.IPAddress = inet_addr(Utf8Str(tmpAddr).c_str());
        m.networkMask = inet_addr(Utf8Str(tmpMask).c_str());
    }

    if (m.IPV6Address.isEmpty())
    {
        Bstr tmpPrefixLen;
        HRESULT hrc = mVBox->GetExtraData(BstrFmt("HostOnly/%ls/IPV6Address", mInterfaceName.raw()).raw(),
                                          m.IPV6Address.asOutParam());
        if (SUCCEEDED(hrc) && !m.IPV6Address.isEmpty())
        {
            hrc = mVBox->GetExtraData(BstrFmt("HostOnly/%ls/IPV6PrefixLen", mInterfaceName.raw()).raw(),
                                      tmpPrefixLen.asOutParam());
            if (SUCCEEDED(hrc) && !tmpPrefixLen.isEmpty())
                m.IPV6NetworkMaskPrefixLength = Utf8Str(tmpPrefixLen).toUInt32();
            else
                m.IPV6NetworkMaskPrefixLength = 64;
        }
    }
#endif

    return S_OK;
}

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
