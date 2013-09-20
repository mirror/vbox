/* $Id$ */
/** @file
 *
 * Host video capture device implementation.
 */

/*
 * Copyright (C) 2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "HostVideoInputDeviceImpl.h"
#include "Logging.h"

#ifdef RT_OS_WINDOWS
#include <dshow.h>
#endif

/*
 * HostVideoInputDevice implementation.
 */
DEFINE_EMPTY_CTOR_DTOR(HostVideoInputDevice)

HRESULT HostVideoInputDevice::FinalConstruct()
{
    return BaseFinalConstruct();
}

void HostVideoInputDevice::FinalRelease()
{
    uninit();

    BaseFinalRelease();
}

/*
 * Initializes the instance.
 */
HRESULT HostVideoInputDevice::init(const com::Utf8Str &name, const com::Utf8Str &path, const com::Utf8Str &alias)
{
    LogFlowThisFunc(("\n"));

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m.name = name;
    m.path = path;
    m.alias = alias;

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return S_OK;
}

/*
 * Uninitializes the instance.
 * Called either from FinalRelease() or by the parent when it gets destroyed.
 */
void HostVideoInputDevice::uninit()
{
    LogFlowThisFunc(("\n"));

    m.name.setNull();
    m.path.setNull();
    m.alias.setNull();

    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;
}

static HRESULT hostVideoInputDeviceAdd(HostVideoInputDeviceList *pList,
                                       const com::Utf8Str &name,
                                       const com::Utf8Str &path,
                                       const com::Utf8Str &alias)
{
    ComObjPtr<HostVideoInputDevice> obj;
    HRESULT hr = obj.createObject();
    if (SUCCEEDED(hr))
    {
        hr = obj->init(name, path, alias);
        if (SUCCEEDED(hr))
            pList->push_back(obj);
    }
    return hr;
}

#ifdef RT_OS_WINDOWS
static HRESULT hvcdCreateEnumerator(IEnumMoniker **ppEnumMoniker)
{
    ICreateDevEnum *pCreateDevEnum = NULL;
    HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&pCreateDevEnum));
    if (SUCCEEDED(hr))
    {
        hr = pCreateDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, ppEnumMoniker, 0);
        pCreateDevEnum->Release();
    }
    return hr;
}

static HRESULT hvcdFillList(HostVideoInputDeviceList *pList, IEnumMoniker *pEnumMoniker)
{
    int iDevice = 0;
    IMoniker *pMoniker = NULL;
    while (pEnumMoniker->Next(1, &pMoniker, NULL) == S_OK)
    {
        IPropertyBag *pPropBag = NULL;
        HRESULT hr = pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPropBag));
        if (FAILED(hr))
        {
            pMoniker->Release();
            continue;
        }

        VARIANT var;
        VariantInit(&var);

        hr = pPropBag->Read(L"DevicePath", &var, 0);
        if (FAILED(hr))
        {
            /* Can't use a device without path. */
            pMoniker->Release();
            continue;
        }

        ++iDevice;

        com::Utf8Str path = var.bstrVal;
        VariantClear(&var);

        hr = pPropBag->Read(L"FriendlyName", &var, 0);
        if (FAILED(hr))
        {
            hr = pPropBag->Read(L"Description", &var, 0);
        }

        com::Utf8Str name;
        if (SUCCEEDED(hr))
        {
            name = var.bstrVal;
            VariantClear(&var);
        }
        else
        {
            name = com::Utf8StrFmt("Video Input Device #%d", iDevice);
        }

        com::Utf8Str alias = com::Utf8StrFmt(".%d", iDevice);

        hr = hostVideoInputDeviceAdd(pList, name, path, alias);

        pPropBag->Release();
        pMoniker->Release();

        if (FAILED(hr))
            return hr;
    }

    return S_OK;
}

static HRESULT fillDeviceList(HostVideoInputDeviceList *pList)
{
    IEnumMoniker *pEnumMoniker = NULL;
    HRESULT hr = hvcdCreateEnumerator(&pEnumMoniker);
    if (SUCCEEDED(hr))
    {
        if (hr != S_FALSE)
        {
            /* List not empty */
            hr = hvcdFillList(pList, pEnumMoniker);
            pEnumMoniker->Release();
        }
        else
        {
            hr = S_OK; /* Return empty list. */
        }
    }
    return hr;
}
#else
static HRESULT fillDeviceList(HostVideoInputDeviceList *pList)
{
    NOREF(pList);
    return E_NOTIMPL;
}
#endif /* RT_OS_WINDOWS */

/* static */ HRESULT HostVideoInputDevice::queryHostDevices(HostVideoInputDeviceList *pList)
{
    HRESULT hr = fillDeviceList(pList);

    if (FAILED(hr))
    {
        pList->clear();
    }

    return hr;
}

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
