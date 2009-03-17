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
