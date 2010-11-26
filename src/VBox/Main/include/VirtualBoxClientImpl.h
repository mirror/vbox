/* $Id$ */

/** @file
 * Header file for the VirtualBoxClient (IVirtualBoxClient) class, VBoxC.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
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

#include "VirtualBoxBase.h"
#include "EventImpl.h"

class ATL_NO_VTABLE VirtualBoxClient :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IVirtualBoxClient)
#ifdef RT_OS_WINDOWS
    , public CComCoClass<VirtualBoxClient, &CLSID_VirtualBoxClient>
#endif
{
public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(VirtualBoxClient, IVirtualBoxClient)

    DECLARE_CLASSFACTORY_SINGLETON(VirtualBoxClient)

    DECLARE_REGISTRY_RESOURCE(IDR_VIRTUALBOX)
    DECLARE_NOT_AGGREGATABLE(VirtualBoxClient)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(VirtualBoxClient)
        COM_INTERFACE_ENTRY2(IDispatch, IVirtualBoxClient)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IVirtualBoxClient)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR(VirtualBoxClient)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init();
    void uninit();

    // IUSBDevice properties
    STDMETHOD(COMGETTER(VirtualBox))(IVirtualBox **aVirtualBox);
    STDMETHOD(COMGETTER(Session))(ISession **aSession);
    STDMETHOD(COMGETTER(EventSource))(IEventSource **aEventSource);

private:

    static DECLCALLBACK(int) SVCWatcherThread(RTTHREAD ThreadSelf, void *pvUser);

    struct Data
    {
        Data()
        {}

        const ComPtr<IVirtualBox> m_pVirtualBox;
        const ComObjPtr<EventSource> m_pEventSource;

        RTTHREAD m_ThreadWatcher;
        RTSEMEVENT m_SemEvWatcher;
    };

    Data mData;
};

#endif // ____H_VIRTUALBOXCLIENTIMPL
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
