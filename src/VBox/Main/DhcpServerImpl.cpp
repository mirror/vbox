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

HRESULT DhcpServer::init(IN_BSTR aName)
{
    return E_NOTIMPL;
}

HRESULT DhcpServer::init(const settings::Key &aNode)
{
    return E_NOTIMPL;
}

HRESULT DhcpServer::saveSettings (settings::Key &aParentNode)
{
    return E_NOTIMPL;
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

    m.enabled = aEnabled;

    return S_OK;

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
    return E_NOTIMPL;
}
