/* $Id$ */
/** @file
 * VirtualBox COM Event class implementation
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

#include "EventImpl.h"

struct VBoxEvent::Data
{
    Data()
        : 
        mType(VBoxEventType_Invalid),
        mWaitable(FALSE)
    {}
    VBoxEventType_T mType;
    BOOL            mWaitable;
};

HRESULT VBoxEvent::FinalConstruct()
{
    m = new Data;
    return S_OK;
}

void VBoxEvent::FinalRelease()
{
    uninit();
    delete m;
}


HRESULT VBoxEvent::init(IEventSource *aSource, VBoxEventType_T aType, BOOL aWaitable)
{
    HRESULT rc = S_OK;

    m->mType = aType;
    m->mWaitable = aWaitable;

    return rc;
}

void VBoxEvent::uninit()
{
    m->mType = VBoxEventType_Invalid;
}

STDMETHODIMP VBoxEvent::COMGETTER(Type)(VBoxEventType_T *aType)
{
    // never  changes till event alive, no locking?
    *aType = m->mType;
    return S_OK;
}

STDMETHODIMP VBoxEvent::COMGETTER(Source)(IEventSource* *aSource)
{
    return E_NOTIMPL;
}

STDMETHODIMP VBoxEvent::COMGETTER(Waitable)(BOOL *aWaitable)
{
    // never  changes till event alive, no locking?
    *aWaitable = m->mWaitable;
    return S_OK;
}


STDMETHODIMP VBoxEvent::SetProcessed()
{
    return E_NOTIMPL;
}

STDMETHODIMP VBoxEvent::WaitProcessed(LONG aTimeout, BOOL *aResult)
{
    return E_NOTIMPL;
}


struct EventSource::Data
{
    Data()
        : 
        mBogus(0)
    {}
    int32_t            mBogus;
};

HRESULT EventSource::FinalConstruct()
{
    m = new Data;
    return S_OK;
}

void EventSource::FinalRelease()
{
    uninit();
    delete m;
}


HRESULT EventSource::init()
{
    HRESULT rc = S_OK;
    return rc;
}

void EventSource::uninit()
{
}

STDMETHODIMP EventSource::RegisterListener(IEventListener * aListener, 
                                           ComSafeArrayIn(VBoxEventType, aInterested),
                                           BOOL             aActive)
{
    return E_NOTIMPL;
}

STDMETHODIMP EventSource::UnregisterListener(IEventListener * aListener)
{
    return E_NOTIMPL;
}

STDMETHODIMP EventSource::FireEvent(IEvent * aEvent,
                                    LONG     aTimeout,
                                    BOOL     *aProcessed)
{
    return E_NOTIMPL;
}


STDMETHODIMP EventSource::GetEvent(IEventListener * aListener,
                                   LONG      aTimeout,
                                   IEvent  * *aEvent)
{
    return E_NOTIMPL;
}

STDMETHODIMP EventSource::EventProcessed(IEventListener * aListener,
                                         IEvent *         aEvent)
{
    return E_NOTIMPL;
}

