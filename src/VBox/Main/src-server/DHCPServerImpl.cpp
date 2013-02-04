/* $Id$ */

/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "DHCPServerRunner.h"
#include "DHCPServerImpl.h"
#include "AutoCaller.h"
#include "Logging.h"

#include <iprt/cpp/utils.h>

#include <VBox/settings.h>

#include "VirtualBoxImpl.h"

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DHCPServer::DHCPServer()
    : mVirtualBox(NULL)
{
}

DHCPServer::~DHCPServer()
{
}

HRESULT DHCPServer::FinalConstruct()
{
    return BaseFinalConstruct();
}

void DHCPServer::FinalRelease()
{
    uninit ();

    BaseFinalRelease();
}

void DHCPServer::uninit()
{
    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    unconst(mVirtualBox) = NULL;
}

HRESULT DHCPServer::init(VirtualBox *aVirtualBox, IN_BSTR aName)
{
    AssertReturn(aName != NULL, E_INVALIDARG);

    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    /* share VirtualBox weakly (parent remains NULL so far) */
    unconst(mVirtualBox) = aVirtualBox;

    unconst(mName) = aName;
    m.IPAddress = "0.0.0.0";
    m.networkMask = "0.0.0.0";
    m.enabled = FALSE;
    m.lowerIP = "0.0.0.0";
    m.upperIP = "0.0.0.0";

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

HRESULT DHCPServer::init(VirtualBox *aVirtualBox,
                         const settings::DHCPServer &data)
{
    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    /* share VirtualBox weakly (parent remains NULL so far) */
    unconst(mVirtualBox) = aVirtualBox;

    unconst(mName) = data.strNetworkName;
    m.IPAddress = data.strIPAddress;
    m.networkMask = data.strIPNetworkMask;
    m.enabled = data.fEnabled;
    m.lowerIP = data.strIPLower;
    m.upperIP = data.strIPUpper;

    autoInitSpan.setSucceeded();

    return S_OK;
}

HRESULT DHCPServer::saveSettings(settings::DHCPServer &data)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    data.strNetworkName = mName;
    data.strIPAddress = m.IPAddress;
    data.strIPNetworkMask = m.networkMask;
    data.fEnabled = !!m.enabled;
    data.strIPLower = m.lowerIP;
    data.strIPUpper = m.upperIP;

    return S_OK;
}

STDMETHODIMP DHCPServer::COMGETTER(NetworkName) (BSTR *aName)
{
    CheckComArgOutPointerValid(aName);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    mName.cloneTo(aName);

    return S_OK;
}

STDMETHODIMP DHCPServer::COMGETTER(Enabled) (BOOL *aEnabled)
{
    CheckComArgOutPointerValid(aEnabled);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    *aEnabled = m.enabled;

    return S_OK;
}

STDMETHODIMP DHCPServer::COMSETTER(Enabled) (BOOL aEnabled)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    m.enabled = aEnabled;

    // save the global settings; for that we should hold only the VirtualBox lock
    alock.release();
    AutoWriteLock vboxLock(mVirtualBox COMMA_LOCKVAL_SRC_POS);
    HRESULT rc = mVirtualBox->saveSettings();

    return rc;
}

STDMETHODIMP DHCPServer::COMGETTER(IPAddress) (BSTR *aIPAddress)
{
    CheckComArgOutPointerValid(aIPAddress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    m.IPAddress.cloneTo(aIPAddress);

    return S_OK;
}

STDMETHODIMP DHCPServer::COMGETTER(NetworkMask) (BSTR *aNetworkMask)
{
    CheckComArgOutPointerValid(aNetworkMask);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    m.networkMask.cloneTo(aNetworkMask);

    return S_OK;
}

STDMETHODIMP DHCPServer::COMGETTER(LowerIP) (BSTR *aIPAddress)
{
    CheckComArgOutPointerValid(aIPAddress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    m.lowerIP.cloneTo(aIPAddress);

    return S_OK;
}

STDMETHODIMP DHCPServer::COMGETTER(UpperIP) (BSTR *aIPAddress)
{
    CheckComArgOutPointerValid(aIPAddress);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    m.upperIP.cloneTo(aIPAddress);

    return S_OK;
}

STDMETHODIMP DHCPServer::SetConfiguration (IN_BSTR aIPAddress, IN_BSTR aNetworkMask, IN_BSTR aLowerIP, IN_BSTR aUpperIP)
{
    AssertReturn(aIPAddress != NULL, E_INVALIDARG);
    AssertReturn(aNetworkMask != NULL, E_INVALIDARG);
    AssertReturn(aLowerIP != NULL, E_INVALIDARG);
    AssertReturn(aUpperIP != NULL, E_INVALIDARG);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    m.IPAddress = aIPAddress;
    m.networkMask = aNetworkMask;
    m.lowerIP = aLowerIP;
    m.upperIP = aUpperIP;

    // save the global settings; for that we should hold only the VirtualBox lock
    alock.release();
    AutoWriteLock vboxLock(mVirtualBox COMMA_LOCKVAL_SRC_POS);
    return mVirtualBox->saveSettings();
}

STDMETHODIMP DHCPServer::Start(IN_BSTR aNetworkName, IN_BSTR aTrunkName, IN_BSTR aTrunkType)
{
    /* Silently ignore attempts to run disabled servers. */
    if (!m.enabled)
        return S_OK;

    m.dhcp.setOption(DHCPCFG_NETNAME, Utf8Str(aNetworkName), true);
    Bstr tmp(aTrunkName);
    if (!tmp.isEmpty())
        m.dhcp.setOption(DHCPCFG_TRUNKNAME, Utf8Str(tmp), true);
    m.dhcp.setOption(DHCPCFG_TRUNKTYPE, Utf8Str(aTrunkType), true);
    //temporary hack for testing
    //    DHCPCFG_NAME
    char strMAC[32];
    Guid guid;
    guid.create();
    RTStrPrintf (strMAC, sizeof(strMAC), "08:00:27:%02X:%02X:%02X",
                 guid.raw()->au8[0], guid.raw()->au8[1], guid.raw()->au8[2]);
    m.dhcp.setOption(DHCPCFG_MACADDRESS, strMAC, true);
    m.dhcp.setOption(DHCPCFG_IPADDRESS,  Utf8Str(m.IPAddress), true);
    //        DHCPCFG_LEASEDB,
    //        DHCPCFG_VERBOSE,
    //        DHCPCFG_GATEWAY,
    m.dhcp.setOption(DHCPCFG_LOWERIP,  Utf8Str(m.lowerIP), true);
    m.dhcp.setOption(DHCPCFG_UPPERIP,  Utf8Str(m.upperIP), true);
    m.dhcp.setOption(DHCPCFG_NETMASK,  Utf8Str(m.networkMask), true);

    //        DHCPCFG_HELP,
    //        DHCPCFG_VERSION,
    //        DHCPCFG_NOTOPT_MAXVAL
    m.dhcp.setOption(DHCPCFG_BEGINCONFIG,  "", true);

    return RT_FAILURE(m.dhcp.start()) ? E_FAIL : S_OK;
    //m.dhcp.detachFromServer(); /* need to do this to avoid server shutdown on runner destruction */
}

STDMETHODIMP DHCPServer::Stop (void)
{
    return RT_FAILURE(m.dhcp.stop()) ? E_FAIL : S_OK;
}
