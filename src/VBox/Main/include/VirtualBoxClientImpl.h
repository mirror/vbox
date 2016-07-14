/* $Id$ */

/** @file
 * Header file for the VirtualBoxClient (IVirtualBoxClient) class, VBoxC.
 */

/*
 * Copyright (C) 2010-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_VIRTUALBOXCLIENTIMPL
#define ____H_VIRTUALBOXCLIENTIMPL

#include "VirtualBoxClientWrap.h"
#include "EventImpl.h"

#ifdef RT_OS_WINDOWS
# include "win/resource.h"
#endif

class ATL_NO_VTABLE VirtualBoxClient :
    public VirtualBoxClientWrap
#ifdef RT_OS_WINDOWS
    , public ATL::CComCoClass<VirtualBoxClient, &CLSID_VirtualBoxClient>
#endif
{
public:
    DECLARE_CLASSFACTORY_SINGLETON(VirtualBoxClient)

    // Do not use any ATL registry support.
    //DECLARE_REGISTRY_RESOURCEID(IDR_VIRTUALBOX)

    DECLARE_NOT_AGGREGATABLE(VirtualBoxClient)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init();
    void uninit();

private:
    // wrapped IVirtualBoxClient properties
    virtual HRESULT getVirtualBox(ComPtr<IVirtualBox> &aVirtualBox);
    virtual HRESULT getSession(ComPtr<ISession> &aSession);
    virtual HRESULT getEventSource(ComPtr<IEventSource> &aEventSource);

    // wrapped IVirtualBoxClient methods
    virtual HRESULT checkMachineError(const ComPtr<IMachine> &aMachine);

    /** Instance counter for simulating something similar to a singleton.
     * Only the first instance will be a usable object, all additional
     * instances will return a failure at creation time and will not work. */
    static uint32_t g_cInstances;

    static DECLCALLBACK(int) SVCWatcherThread(RTTHREAD ThreadSelf, void *pvUser);

    struct Data
    {
        Data()
        {}

        ComPtr<IVirtualBox> m_pVirtualBox;
        const ComObjPtr<EventSource> m_pEventSource;

        RTTHREAD m_ThreadWatcher;
        RTSEMEVENT m_SemEvWatcher;
    };

    Data mData;
};

#endif // ____H_VIRTUALBOXCLIENTIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
