/* $Id$ */
/** @file
 * VBoxCredentialProvFactory - The VirtualBox Credential Provider factory.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VBoxCredentialProvider.h"

#include "VBoxCredProvFactory.h"
#include "VBoxCredProvProvider.h"

extern HRESULT VBoxCredProvProviderCreate(REFIID interfaceID, void **ppvInterface);

VBoxCredProvFactory::VBoxCredProvFactory(void)
    : m_cRefCount(1) /* Start with one instance. */
{
}

VBoxCredProvFactory::~VBoxCredProvFactory(void)
{
}

/** IUnknown overrides. */
ULONG VBoxCredProvFactory::AddRef(void)
{
    VBoxCredProvVerbose(0, "VBoxCredProvFactory: Increasing reference from %ld to %ld\n",
                        m_cRefCount, m_cRefCount + 1);

    return m_cRefCount++;
}

ULONG VBoxCredProvFactory::Release(void)
{
    VBoxCredProvVerbose(0, "VBoxCredProvFactory: Decreasing reference from %ld to %ld\n",
                        m_cRefCount, m_cRefCount - 1);

    ULONG cRefCount = --m_cRefCount;
    if (!cRefCount)
    {
        VBoxCredProvVerbose(0, "VBoxCredProvFactory: Calling destructor\n");
        delete this;
    }
    return cRefCount;
}

HRESULT VBoxCredProvFactory::QueryInterface(REFIID interfaceID, void **ppvInterface)
{
    HRESULT hr = S_OK;
    if (ppvInterface)
    {
        if (   IID_IClassFactory == interfaceID
            || IID_IUnknown      == interfaceID)
        {
            *ppvInterface = static_cast<IUnknown*>(this);
            reinterpret_cast<IUnknown*>(*ppvInterface)->AddRef();
        }
        else
        {
            *ppvInterface = NULL;
            hr = E_NOINTERFACE;
        }
    }
    else
        hr = E_INVALIDARG;
    return hr;
}

/** IClassFactory overrides. This creates our actual credential provider. */
HRESULT VBoxCredProvFactory::CreateInstance(IUnknown* pUnkOuter,
                                            REFIID interfaceID, void **ppvInterface)
{
    if (pUnkOuter)
        return CLASS_E_NOAGGREGATION;

    return VBoxCredProvProviderCreate(interfaceID, ppvInterface);
}

HRESULT VBoxCredProvFactory::LockServer(BOOL bLock)
{
    bLock ? VBoxCredentialProviderAcquire() : VBoxCredentialProviderRelease();
    return S_OK;
}

