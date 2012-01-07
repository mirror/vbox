/* $Id$ */
/** @file
 * VirtualBox COM IEvent implementation
 */

/*
 * Copyright (C) 2010-2012 Oracle Corporation
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
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IEvent)
{
public:
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(VBoxEvent, IEvent)

    DECLARE_NOT_AGGREGATABLE(VBoxEvent)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(VBoxEvent)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IEvent)
    END_COM_MAP()

    VBoxEvent() {}
    virtual ~VBoxEvent() {}

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(IEventSource *aSource, VBoxEventType_T aType, BOOL aWaitable);
    void uninit();

    // IEvent properties
    STDMETHOD(COMGETTER(Type))(VBoxEventType_T *aType);
    STDMETHOD(COMGETTER(Source))(IEventSource * *aSource);
    STDMETHOD(COMGETTER(Waitable))(BOOL *aWaitable);

    // IEvent methods
    STDMETHOD(SetProcessed)();
    STDMETHOD(WaitProcessed)(LONG aTimeout, BOOL *aResult);

private:
    struct Data;

    Data* m;
};

class ATL_NO_VTABLE VBoxVetoEvent :
    public VBoxEvent,
    VBOX_SCRIPTABLE_IMPL(IVetoEvent)
{
public:
    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(VBoxVetoEvent, IVetoEvent)

    DECLARE_NOT_AGGREGATABLE(VBoxVetoEvent)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(VBoxVetoEvent)
        COM_INTERFACE_ENTRY2(IEvent, IVetoEvent)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IVetoEvent)
    END_COM_MAP()

    VBoxVetoEvent() {}
    virtual ~VBoxVetoEvent() {}

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(IEventSource *aSource, VBoxEventType_T aType);
    void uninit();

    // IEvent properties
    STDMETHOD(COMGETTER(Type))(VBoxEventType_T *aType)
    {
        return VBoxEvent::COMGETTER(Type)(aType);
    }
    STDMETHOD(COMGETTER(Source))(IEventSource * *aSource)
    {
        return VBoxEvent::COMGETTER(Source)(aSource);
    }
    STDMETHOD(COMGETTER(Waitable))(BOOL *aWaitable)
    {
        return VBoxEvent::COMGETTER(Waitable)(aWaitable);
    }

    // IEvent methods
    STDMETHOD(SetProcessed)()
    {
        return VBoxEvent::SetProcessed();
    }
    STDMETHOD(WaitProcessed)(LONG aTimeout, BOOL *aResult)
    {
        return VBoxEvent::WaitProcessed(aTimeout, aResult);
    }

     // IVetoEvent methods
    STDMETHOD(AddVeto)(IN_BSTR aVeto);
    STDMETHOD(IsVetoed)(BOOL *aResult);
    STDMETHOD(GetVetos)(ComSafeArrayOut(BSTR, aVetos));

private:
    struct Data;

    Data* m;
};

class ATL_NO_VTABLE EventSource :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IEventSource)
{
public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(EventSource, IEventSource)

    DECLARE_NOT_AGGREGATABLE(EventSource)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(EventSource)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IEventSource)
    END_COM_MAP()

    DECLARE_EMPTY_CTOR_DTOR(EventSource)

    HRESULT FinalConstruct();
    void FinalRelease();

    // public initializer/uninitializer for internal purposes only
    HRESULT init(IUnknown *aParent);
    void uninit();

    // IEventSource methods
    STDMETHOD(CreateListener)(IEventListener **aListener);
    STDMETHOD(CreateAggregator)(ComSafeArrayIn(IEventSource *, aSubordinates),
                                IEventSource **aAggregator);
    STDMETHOD(RegisterListener)(IEventListener *aListener,
                                ComSafeArrayIn(VBoxEventType_T, aInterested),
                                BOOL aActive);
    STDMETHOD(UnregisterListener)(IEventListener *aListener);
    STDMETHOD(FireEvent)(IEvent *aEvent, LONG aTimeout, BOOL *aProcessed);
    STDMETHOD(GetEvent)(IEventListener *aListener, LONG aTimeout,
                        IEvent **aEvent);
    STDMETHOD(EventProcessed)(IEventListener *aListener, IEvent *aEvent);

private:
    struct Data;

    Data* m;

    friend class ListenerRecord;
};

class VBoxEventDesc
{
public:
    VBoxEventDesc() : mEvent(0), mEventSource(0)
    {}

    ~VBoxEventDesc()
    {}

    /**
     * This function to be used with some care, as arguments order must match
     * attribute declaration order event class and its superclasses up to
     * IEvent. If unsure, consult implementation in generated VBoxEvents.cpp.
     */
    HRESULT init(IEventSource* aSource, VBoxEventType_T aType, ...);

    /**
    * Function similar to the above, but assumes that init() for this type
    * already called once, so no need to allocate memory, and only reinit
    * fields. Assumes event is subtype of IReusableEvent, asserts otherwise.
    */
    HRESULT reinit(VBoxEventType_T aType, ...);

    void uninit()
    {
        mEvent.setNull();
        mEventSource.setNull();
    }

    void getEvent(IEvent **aEvent)
    {
        mEvent.queryInterfaceTo(aEvent);
    }

    BOOL fire(LONG aTimeout)
    {
        if (mEventSource && mEvent)
        {
            BOOL fDelivered = FALSE;
            int rc = mEventSource->FireEvent(mEvent, aTimeout, &fDelivered);
            AssertRCReturn(rc, FALSE);
            return fDelivered;
        }
        return FALSE;
    }

private:
    ComPtr<IEvent>          mEvent;
    ComPtr<IEventSource>    mEventSource;
};

#endif // ____H_EVENTIMPL
