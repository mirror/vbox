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

#include "DhcpServerImpl.h"
#include "Logging.h"

#include <VBox/settings.h>

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR (DhcpServer)

HRESULT DhcpServer::FinalConstruct()
{
    return S_OK;
}

void DhcpServer::FinalRelease()
{
    uninit ();
}

HRESULT DhcpServer::init(VirtualBox *aVirtualBox, IN_BSTR aName)
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
    m.FromIPAddress = "0.0.0.0";
    m.ToIPAddress = "0.0.0.0";

    /* register with VirtualBox early, since uninit() will
     * unconditionally unregister on failure */
    aVirtualBox->addDependentChild (this);

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

HRESULT DhcpServer::init(VirtualBox *aVirtualBox, const settings::Key &aNode)
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
    m.FromIPAddress = aNode.stringValue ("lowerIp");
    m.ToIPAddress = aNode.stringValue ("upperIp");

    autoInitSpan.setSucceeded();

    return S_OK;
}

HRESULT DhcpServer::saveSettings (settings::Key &aParentNode)
{
    using namespace settings;

    AssertReturn (!aParentNode.isNull(), E_FAIL);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    AutoReadLock alock (this);

    Key aNode = aParentNode.appendKey ("DhcpServer");
    /* required */
    aNode.setValue <Bstr> ("networkName", mName);
    aNode.setValue <Bstr> ("IPAddress", m.IPAddress);
    aNode.setValue <Bstr> ("networkMask", m.networkMask);
    aNode.setValue <Bstr> ("FromIPAddress", m.FromIPAddress);
    aNode.setValue <Bstr> ("ToIPAddress", m.ToIPAddress);

    return S_OK;
}

STDMETHODIMP DhcpServer::COMGETTER(NetworkName) (BSTR *aName)
{
    CheckComArgOutPointerValid(aName);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    mName.cloneTo(aName);

    return S_OK;

}

STDMETHODIMP DhcpServer::COMGETTER(Enabled) (BOOL *aEnabled)
{
    CheckComArgOutPointerValid(aEnabled);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    *aEnabled = m.enabled;

    return S_OK;

}

STDMETHODIMP DhcpServer::COMSETTER(Enabled) (BOOL aEnabled)
{
    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* VirtualBox::saveSettings() needs a write lock */
    AutoMultiWriteLock2 alock (mVirtualBox, this);

    m.enabled = aEnabled;

    HRESULT rc = mVirtualBox->saveSettings();

    return rc;
}

STDMETHODIMP DhcpServer::COMGETTER(IPAddress) (BSTR *aIPAddress)
{
    CheckComArgOutPointerValid(aIPAddress);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    m.IPAddress.cloneTo(aIPAddress);

    return S_OK;

}

STDMETHODIMP DhcpServer::COMGETTER(NetworkMask) (BSTR *aNetworkMask)
{
    CheckComArgOutPointerValid(aNetworkMask);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    m.networkMask.cloneTo(aNetworkMask);

    return S_OK;

}

STDMETHODIMP DhcpServer::COMGETTER(FromIPAddress) (BSTR *aIPAddress)
{
    CheckComArgOutPointerValid(aIPAddress);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    m.FromIPAddress.cloneTo(aIPAddress);

    return S_OK;

}

STDMETHODIMP DhcpServer::COMGETTER(ToIPAddress) (BSTR *aIPAddress)
{
    CheckComArgOutPointerValid(aIPAddress);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    m.ToIPAddress.cloneTo(aIPAddress);

    return S_OK;

}

STDMETHODIMP DhcpServer::SetConfiguration (IN_BSTR aIPAddress, IN_BSTR aNetworkMask, IN_BSTR aFromIPAddress, IN_BSTR aToIPAddress)
{
    AssertReturn (aIPAddress != NULL, E_INVALIDARG);
    AssertReturn (aNetworkMask != NULL, E_INVALIDARG);
    AssertReturn (aFromIPAddress != NULL, E_INVALIDARG);
    AssertReturn (aToIPAddress != NULL, E_INVALIDARG);

    AutoCaller autoCaller (this);
    CheckComRCReturnRC (autoCaller.rc());

    /* VirtualBox::saveSettings() needs a write lock */
    AutoMultiWriteLock2 alock (mVirtualBox, this);

    m.IPAddress = aIPAddress;
    m.networkMask = aNetworkMask;
    m.FromIPAddress = aFromIPAddress;
    m.ToIPAddress = aToIPAddress;

    HRESULT rc = mVirtualBox->saveSettings();

    return rc;
}
