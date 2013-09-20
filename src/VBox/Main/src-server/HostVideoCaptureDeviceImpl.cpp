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

#include "HostVideoCaptureDeviceImpl.h"
#include "Logging.h"

/*
 * HostVideoCaptureDevice implementation.
 */
DEFINE_EMPTY_CTOR_DTOR(HostVideoCaptureDevice)

HRESULT HostVideoCaptureDevice::FinalConstruct()
{
    return BaseFinalConstruct();
}

void HostVideoCaptureDevice::FinalRelease()
{
    uninit();

    BaseFinalRelease();
}

/*
 * Initializes the instance.
 */
HRESULT HostVideoCaptureDevice::init(com::Utf8Str name, com::Utf8Str path, com::Utf8Str alias)
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
void HostVideoCaptureDevice::uninit()
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

/* static */ HRESULT HostVideoCaptureDevice::queryHostDevices(HostVideoCaptureDeviceList *pList)
{
    HRESULT hr = S_OK;
#if 0
    PDARWINETHERNIC pEtherNICs = DarwinGetEthernetControllers();
    while (pEtherNICs)
    {
        ComObjPtr<HostNetworkInterface> IfObj;
        IfObj.createObject();
        if (SUCCEEDED(IfObj->init(Bstr(pEtherNICs->szName), Guid(pEtherNICs->Uuid), HostNetworkInterfaceType_Bridged)))
            list.push_back(IfObj);

        /* next, free current */
        void *pvFree = pEtherNICs;
        pEtherNICs = pEtherNICs->pNext;
        RTMemFree(pvFree);
    }
#endif
    return hr;
}

/* vi: set tabstop=4 shiftwidth=4 expandtab: */
