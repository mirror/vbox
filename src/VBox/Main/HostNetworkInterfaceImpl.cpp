/** @file
 *
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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

// constructor / destructor
/////////////////////////////////////////////////////////////////////////////

HostNetworkInterface::HostNetworkInterface()
{
}

HostNetworkInterface::~HostNetworkInterface()
{
}

// public initializer/uninitializer for internal purposes only
/////////////////////////////////////////////////////////////////////////////

/**
 * Initializes the host object.
 *
 * @returns COM result indicator
 * @param   interfaceName name of the network interface
 */
HRESULT HostNetworkInterface::init (Bstr interfaceName, Guid guid)
{
    ComAssertRet (interfaceName, E_INVALIDARG);
    ComAssertRet (!guid.isEmpty(), E_INVALIDARG);

    AutoLock lock(this);
    mInterfaceName = interfaceName;
    mGuid = guid;
    setReady(true);
    return S_OK;
}

// IHostNetworkInterface properties
/////////////////////////////////////////////////////////////////////////////

/**
 * Returns the name of the host network interface.
 *
 * @returns COM status code
 * @param   interfaceName address of result pointer
 */
STDMETHODIMP HostNetworkInterface::COMGETTER(Name) (BSTR *interfaceName)
{
    if (!interfaceName)
        return E_POINTER;
    AutoLock lock(this);
    CHECK_READY();
    mInterfaceName.cloneTo(interfaceName);
    return S_OK;
}

/**
 * Returns the GUID of the host network interface.
 *
 * @returns COM status code
 * @param   guid address of result pointer
 */
STDMETHODIMP HostNetworkInterface::COMGETTER(Id) (GUIDPARAMOUT guid)
{
    if (!guid)
        return E_POINTER;
    AutoLock lock(this);
    CHECK_READY();
    mGuid.cloneTo(guid);
    return S_OK;
}
