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

#include "DHCPServerRunner.h"
#include "DHCPServerImpl.h"
#include "Logging.h"

#include <VBox/settings.h>

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR (DHCPServer)

HRESULT DHCPServer::FinalConstruct()
{
    return S_OK;
}

void DHCPServer::FinalRelease()
{
    uninit ();
}

void DHCPServer::uninit()
{
    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan (this);
    if (autoUninitSpan.uninitDone())
        return;

//    /* we uninit children and reset mParent
//     * and VirtualBox::removeDependentChild() needs a write lock */
//    AutoMultiWriteLock2 alock (mVirtualBox->lockHandle(), this->treeLock());

    mVirtualBox->removeDependentChild (this);

    unconst (mVirtualBox).setNull();
}

HRESULT DHCPServer::init(VirtualBox *aVirtualBox, IN_BSTR aName)
{
    AssertReturn (aName != NULL, E_INVALIDARG);

    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_FAIL);

    /* share VirtualBox weakly (parent remains NULL so far) */
    unconst (mVirtualBox) = aVirtualBox;

    unconst(mName) = aName;
    m.IPAddress = "0.0.0.0";
    m.networkMask = "0.0.0.0";
    m.enabled = FALSE;
    m.lowerIP = "0.0.0.0";
    m.upperIP = "0.0.0.0";

    /* register with VirtualBox early, since uninit() will
     * unconditionally unregister on failure */
    aVirtualBox->addDependentChild (this);

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

HRESULT DHCPServer::init(VirtualBox *aVirtualBox, const settings::Key &aNode)
{
    using namespace settings;

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan (this);
    AssertReturn (autoInitSpan.isOk(), E_FAIL);

    /* share VirtualBox weakly (parent remains NULL so far) */
    unconst (mVirtualBox) = aVirtualBox;

    aVirtualBox->addDependentChild (this);

    unconst(mName) = aNode.stringValue ("networkName");
    m.IPAddress = aNode.stringValue ("IPAddress");
    m.networkMask = aNode.stringValue ("networkMask");
    m.enabled = aNode.value <BOOL> ("enabled");
    m.lowerIP = aNode.stringValue ("lowerIP");
    m.upperIP = aNode.stringValue ("upperIP");

    autoInitSpan.setSucceeded();

    return S_OK;
}

HRESULT DHCPServer::saveSettings (settings::Key &aParentNode)
{
    using namespace settings;

    AssertReturn (!aParentNode.isNull(), E_FAIL);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    Key aNode = aParentNode.appendKey ("DHCPServer");
    /* required */
    aNode.setValue <Bstr> ("networkName", mName);
    aNode.setValue <Bstr> ("IPAddress", m.IPAddress);
    aNode.setValue <Bstr> ("networkMask", m.networkMask);
    aNode.setValue <Bstr> ("lowerIP", m.lowerIP);
    aNode.setValue <Bstr> ("upperIP", m.upperIP);
    aNode.setValue <BOOL> ("enabled", m.enabled);

    return S_OK;
}

STDMETHODIMP DHCPServer::COMGETTER(NetworkName) (BSTR *aName)
{
    CheckComArgOutPointerValid(aName);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    mName.cloneTo(aName);

    return S_OK;
}

STDMETHODIMP DHCPServer::COMGETTER(Enabled) (BOOL *aEnabled)
{
    CheckComArgOutPointerValid(aEnabled);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    *aEnabled = m.enabled;

    return S_OK;
}

STDMETHODIMP DHCPServer::COMSETTER(Enabled) (BOOL aEnabled)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* VirtualBox::saveSettings() needs a write lock */
    AutoMultiWriteLock2 alock (mVirtualBox, this);

    m.enabled = aEnabled;

    HRESULT rc = mVirtualBox->saveSettings();

    return rc;
}

STDMETHODIMP DHCPServer::COMGETTER(IPAddress) (BSTR *aIPAddress)
{
    CheckComArgOutPointerValid(aIPAddress);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    m.IPAddress.cloneTo(aIPAddress);

    return S_OK;
}

STDMETHODIMP DHCPServer::COMGETTER(NetworkMask) (BSTR *aNetworkMask)
{
    CheckComArgOutPointerValid(aNetworkMask);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    m.networkMask.cloneTo(aNetworkMask);

    return S_OK;
}

STDMETHODIMP DHCPServer::COMGETTER(LowerIP) (BSTR *aIPAddress)
{
    CheckComArgOutPointerValid(aIPAddress);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    m.lowerIP.cloneTo(aIPAddress);

    return S_OK;
}

STDMETHODIMP DHCPServer::COMGETTER(UpperIP) (BSTR *aIPAddress)
{
    CheckComArgOutPointerValid(aIPAddress);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    m.upperIP.cloneTo(aIPAddress);

    return S_OK;
}

STDMETHODIMP DHCPServer::SetConfiguration (IN_BSTR aIPAddress, IN_BSTR aNetworkMask, IN_BSTR aLowerIP, IN_BSTR aUpperIP)
{
    AssertReturn (aIPAddress != NULL, E_INVALIDARG);
    AssertReturn (aNetworkMask != NULL, E_INVALIDARG);
    AssertReturn (aLowerIP != NULL, E_INVALIDARG);
    AssertReturn (aUpperIP != NULL, E_INVALIDARG);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* VirtualBox::saveSettings() needs a write lock */
    AutoMultiWriteLock2 alock (mVirtualBox, this);

    m.IPAddress = aIPAddress;
    m.networkMask = aNetworkMask;
    m.lowerIP = aLowerIP;
    m.upperIP = aUpperIP;

    return mVirtualBox->saveSettings();
}

STDMETHODIMP DHCPServer::Start (IN_BSTR aNetworkName, IN_BSTR aTrunkName, IN_BSTR aTrunkType)
{
    /* Silently ignore attepmts to run disabled servers. */
    if (!m.enabled)
        return S_OK;

    m.dhcp.setOption(DHCPCFG_NETNAME, Utf8Str(aNetworkName));
    Bstr tmp(aTrunkName);
    if (!tmp.isEmpty())
        m.dhcp.setOption(DHCPCFG_TRUNKNAME, Utf8Str(tmp));
    m.dhcp.setOption(DHCPCFG_TRUNKTYPE, Utf8Str(aTrunkType));
    //temporary hack for testing
    //    DHCPCFG_NAME
    char strMAC[13];
    Guid guid;
    guid.create();
    RTStrPrintf (strMAC, sizeof(strMAC), "080027%02X%02X%02X",
                 guid.ptr()->au8[0], guid.ptr()->au8[1], guid.ptr()->au8[2]);
    m.dhcp.setOption(DHCPCFG_MACADDRESS, strMAC);
    m.dhcp.setOption(DHCPCFG_IPADDRESS,  Utf8Str(m.IPAddress));
    //        DHCPCFG_LEASEDB,
    //        DHCPCFG_VERBOSE,
    //        DHCPCFG_GATEWAY,
    m.dhcp.setOption(DHCPCFG_LOWERIP,  Utf8Str(m.lowerIP));
    m.dhcp.setOption(DHCPCFG_UPPERIP,  Utf8Str(m.upperIP));
    m.dhcp.setOption(DHCPCFG_NETMASK,  Utf8Str(m.networkMask));

    //        DHCPCFG_HELP,
    //        DHCPCFG_VERSION,
    //        DHCPCFG_NOTOPT_MAXVAL
    m.dhcp.setOption(DHCPCFG_BEGINCONFIG,  "");

    return RT_FAILURE(m.dhcp.start()) ? E_FAIL : S_OK;
    //m.dhcp.detachFromServer(); /* need to do this to avoid server shutdown on runner destruction */
}

STDMETHODIMP DHCPServer::Stop (void)
{
    return RT_FAILURE(m.dhcp.stop()) ? E_FAIL : S_OK;
}
