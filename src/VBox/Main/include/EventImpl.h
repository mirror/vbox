/** @file
 *
 * VirtualBox COM IEvent implementation
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

#ifndef ____H_EVENTIMPL
#define ____H_EVENTIMPL

#include "VirtualBoxBase.h"

class ATL_NO_VTABLE VBoxEvent :
    public VirtualBoxSupportErrorInfoImpl<VBoxEvent, IEvent>,
    public VirtualBoxSupportTranslation<VBoxEvent>,
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IEvent)
{
public:
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(VBoxEvent)

    DECLARE_NOT_AGGREGATABLE(VBoxEvent)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(VBoxEvent)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IEvent)
        COM_INTERFACE_ENTRY(IDispatch)
    END_COM_MAP()

    VBoxEvent() {}
    virtual ~VBoxEvent() {}

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (IEventSource *aSource, VBoxEventType_T aType, BOOL aWaitable);
    void uninit();

    // IEvent properties
    STDMETHOD(COMGETTER(Type)) (VBoxEventType_T *aType);
    STDMETHOD(COMGETTER(Source)) (IEventSource * *aSource);
    STDMETHOD(COMGETTER(Waitable)) (BOOL *aWaitable);
    
    // IEvent methods
    STDMETHOD(SetProcessed)();
    STDMETHOD(WaitProcessed)(LONG aTimeout, BOOL *aResult);

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"Event"; }

private:
    struct Data;

    Data* m;
};

class ATL_NO_VTABLE EventSource :
    public VirtualBoxBase,
    public VirtualBoxSupportErrorInfoImpl<EventSource, IEventSource>,
    public VirtualBoxSupportTranslation<EventSource>,
    VBOX_SCRIPTABLE_IMPL(IEventSource)
{
public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(EventSource)

    DECLARE_NOT_AGGREGATABLE(EventSource)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(EventSource)
        COM_INTERFACE_ENTRY(ISupportErrorInfo)
        COM_INTERFACE_ENTRY(IEventSource)
        COM_INTERFACE_ENTRY(IDispatch)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR (EventSource)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init (IUnknown * aParent);
    void uninit();

    // IEventSource methods
    STDMETHOD(CreateListener)(IEventListener ** aListener);
    STDMETHOD(RegisterListener)(IEventListener * aListener, 
                                ComSafeArrayIn(VBoxEventType_T, aInterested),
                                BOOL             aActive);
    STDMETHOD(UnregisterListener)(IEventListener * aListener);
    STDMETHOD(FireEvent)(IEvent * aEvent,
                         LONG     aTimeout,
                         BOOL     *aProcessed);
    STDMETHOD(GetEvent)(IEventListener * aListener,
                        LONG      aTimeout,
                        IEvent  * *aEvent);
    STDMETHOD(EventProcessed)(IEventListener * aListener,
                              IEvent *         aEvent);

    // for VirtualBoxSupportErrorInfoImpl
    static const wchar_t *getComponentName() { return L"EventSource"; }

private:
    struct Data;

    Data* m;

    friend class ListenerRecord;
};

#endif // ____H_EVENTIMPL
