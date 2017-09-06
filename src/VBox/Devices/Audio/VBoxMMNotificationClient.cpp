/* $Id$ */
/** @file
 * VBoxMMNotificationClient.cpp - Implementation of the IMMNotificationClient interface
 *                                to detect audio endpoint changes.
 */

/*
 * Copyright (C) 2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VBoxMMNotificationClient.h"

#include <iprt/win/windows.h>

#pragma warning(push)
#pragma warning(disable: 4201)
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#pragma warning(pop)

#define LOG_GROUP LOG_GROUP_DRV_HOST_AUDIO
#include <VBox/log.h>

VBoxMMNotificationClient::VBoxMMNotificationClient(void)
    : m_fRegisteredClient(false)
    , m_cRef(1)
{
}

VBoxMMNotificationClient::~VBoxMMNotificationClient(void)
{
}

void VBoxMMNotificationClient::Dispose(void)
{
    DetachFromEndpoint();

    if (m_fRegisteredClient)
    {
        m_pEnum->UnregisterEndpointNotificationCallback(this);

        m_fRegisteredClient = false;
    }
}

HRESULT VBoxMMNotificationClient::Initialize(void)
{
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), 0, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                  (void **)&m_pEnum);
    if (SUCCEEDED(hr))
    {
        hr = m_pEnum->RegisterEndpointNotificationCallback(this);
        if (SUCCEEDED(hr))
        {
            hr = AttachToDefaultEndpoint();
        }
    }

    LogFunc(("Returning %Rhrc\n",  hr));
    return hr;
}

HRESULT VBoxMMNotificationClient::AttachToDefaultEndpoint(void)
{
    return S_OK;
}

void VBoxMMNotificationClient::DetachFromEndpoint(void)
{

}

STDMETHODIMP VBoxMMNotificationClient::OnDeviceStateChanged(LPCWSTR pwstrDeviceId, DWORD dwNewState)
{
    char  *pszState = "UNKNOWN";

    switch (dwNewState)
    {
        case DEVICE_STATE_ACTIVE:
            pszState = "ACTIVE";
            break;
        case DEVICE_STATE_DISABLED:
            pszState = "DISABLED";
            break;
        case DEVICE_STATE_NOTPRESENT:
            pszState = "NOTPRESENT";
            break;
        case DEVICE_STATE_UNPLUGGED:
            pszState = "UNPLUGGED";
            break;
    }

    LogFunc(("%ls: %s\n", pwstrDeviceId, pszState));
    return S_OK;
}

STDMETHODIMP VBoxMMNotificationClient::OnDeviceAdded(LPCWSTR pwstrDeviceId)
{
    LogFunc(("%ls\n", pwstrDeviceId));
    return S_OK;
}

STDMETHODIMP VBoxMMNotificationClient::OnDeviceRemoved(LPCWSTR pwstrDeviceId)
{
    LogFunc(("%ls\n", pwstrDeviceId));
    return S_OK;
}

STDMETHODIMP VBoxMMNotificationClient::OnDefaultDeviceChanged(EDataFlow eFlow, ERole eRole, LPCWSTR pwstrDefaultDeviceId)
{
    RT_NOREF(eRole, pwstrDefaultDeviceId);

    if (eFlow == eRender)
    {

    }

    return S_OK;
}

STDMETHODIMP VBoxMMNotificationClient::QueryInterface(REFIID interfaceID, void **ppvInterface)
{
    const IID IID_IMMNotificationClient = __uuidof(IMMNotificationClient);

    if (   IsEqualIID(interfaceID, IID_IUnknown)
        || IsEqualIID(interfaceID, IID_IMMNotificationClient))
    {
        *ppvInterface = static_cast<IMMNotificationClient*>(this);
        AddRef();
        return S_OK;
    }

    *ppvInterface = NULL;
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) VBoxMMNotificationClient::AddRef(void)
{
    return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) VBoxMMNotificationClient::Release(void)
{
    long lRef = InterlockedDecrement(&m_cRef);
    if (lRef == 0)
        delete this;

    return lRef;
}

