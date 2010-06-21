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

#include <list>
#include <map>
#include <deque>

#include "EventImpl.h"
#include "AutoCaller.h"
#include "Logging.h"

#include <iprt/semaphore.h>
#include <iprt/critsect.h>
#include <VBox/com/array.h>

struct VBoxEvent::Data
{
    Data()
        :
        mType(VBoxEventType_Invalid),
        mWaitEvent(NIL_RTSEMEVENT),
        mWaitable(FALSE),
        mProcessed(FALSE)
    {}
    ComPtr<IEventSource>    mSource;
    VBoxEventType_T         mType;
    RTSEMEVENT              mWaitEvent;
    BOOL                    mWaitable;
    BOOL                    mProcessed;
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

    m->mSource = aSource;
    m->mType = aType;
    m->mWaitable = aWaitable;
    m->mProcessed = !aWaitable;

    do {
        if (aWaitable)
        {
            int vrc = ::RTSemEventCreate (&m->mWaitEvent);

            if (RT_FAILURE(vrc))
            {
                AssertFailed ();
                rc = setError(E_FAIL,
                              tr("Internal error (%Rrc)"), vrc);
                break;
            }
        }
    } while (0);

    return rc;
}

void VBoxEvent::uninit()
{
    m->mProcessed = TRUE;
    m->mType = VBoxEventType_Invalid;
    m->mSource.setNull();

    if (m->mWaitEvent != NIL_RTSEMEVENT)
    {
        ::RTSemEventDestroy(m->mWaitEvent);
    }
}

STDMETHODIMP VBoxEvent::COMGETTER(Type)(VBoxEventType_T *aType)
{
    CheckComArgNotNull(aType);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    // never  changes till event alive, no locking?
    *aType = m->mType;
    return S_OK;
}

STDMETHODIMP VBoxEvent::COMGETTER(Source)(IEventSource* *aSource)
{
    CheckComArgOutPointerValid(aSource);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    m->mSource.queryInterfaceTo(aSource);
    return S_OK;
}

STDMETHODIMP VBoxEvent::COMGETTER(Waitable)(BOOL *aWaitable)
{
    CheckComArgNotNull(aWaitable);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    // never  changes till event alive, no locking?
    *aWaitable = m->mWaitable;
    return S_OK;
}


STDMETHODIMP VBoxEvent::SetProcessed()
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (m->mProcessed)
        return S_OK;

    m->mProcessed = TRUE;

    // notify waiters
    ::RTSemEventSignal(m->mWaitEvent);

    return S_OK;
}

STDMETHODIMP VBoxEvent::WaitProcessed(LONG aTimeout, BOOL *aResult)
{
    CheckComArgNotNull(aResult);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    {
        AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

        if (m->mProcessed)
            return S_OK;
    }

    int vrc = ::RTSemEventWait(m->mWaitEvent, aTimeout);
    AssertMsg(RT_SUCCESS(vrc) || vrc == VERR_TIMEOUT || vrc == VERR_INTERRUPTED,
              ("RTSemEventWait returned %Rrc\n", vrc));


    if (RT_SUCCESS(vrc))
    {
        AssertMsg(m->mProcessed,
                  ("mProcessed must be set here\n"));
        *aResult = m->mProcessed;
    }
    else
    {
        *aResult = FALSE;
    }

    return S_OK;
}

static const int FirstEvent = (int)VBoxEventType_LastWildcard + 1;
static const int LastEvent  = (int)VBoxEventType_Last;
static const int NumEvents  = LastEvent - FirstEvent;

struct ListenerRecord;
typedef std::list<ListenerRecord*> EventMap[NumEvents];
typedef std::map<IEvent*, int32_t> PendingEventsMap;
typedef std::deque<ComPtr<IEvent> > PassiveQueue;

struct ListenerRecord
{
    ComPtr<IEventListener>        mListener;
    BOOL                          mActive;
    EventSource*                  mOwner;

    RTSEMEVENT                    mQEvent;
    RTCRITSECT                    mcsQLock;
    PassiveQueue                  mQueue;

    ListenerRecord(IEventListener*                    aListener,
                   com::SafeArray<VBoxEventType_T>&   aInterested,
                   BOOL                               aActive,
                   EventSource*                       aOwner);
    ~ListenerRecord();

    HRESULT process(IEvent* aEvent, BOOL aWaitable, PendingEventsMap::iterator& pit);
    HRESULT enqueue(IEvent* aEvent);
    HRESULT dequeue(IEvent* *aEvent, LONG aTimeout);
    HRESULT eventProcessed(IEvent * aEvent, PendingEventsMap::iterator& pit);
};

typedef std::map<IEventListener*, ListenerRecord>  Listeners;

struct EventSource::Data
{
    Data() {}
    Listeners                     mListeners;
    EventMap                      mEvMap;
    PendingEventsMap              mPendingMap;
};

static BOOL implies(VBoxEventType_T who, VBoxEventType_T what)
{
    switch (who)
    {
        case VBoxEventType_Any:
            return TRUE;
        case VBoxEventType_MachineEvent:
            return (what == VBoxEventType_OnMachineStateChange) || (what == VBoxEventType_OnMachineDataChange);
        case VBoxEventType_Invalid:
            return FALSE;
    }
    return who == what;
}

ListenerRecord::ListenerRecord(IEventListener*                  aListener,
                               com::SafeArray<VBoxEventType_T>& aInterested,
                               BOOL                             aActive,
                               EventSource*                     aOwner)
    :
    mActive(aActive),
    mOwner(aOwner)
{
    mListener = aListener;
    EventMap* aEvMap = &aOwner->m->mEvMap;

    for (size_t i = 0; i < aInterested.size(); ++i)
    {
        VBoxEventType_T interested = aInterested[i];
        for (int j = FirstEvent; j < LastEvent; j++)
        {
            VBoxEventType_T candidate = (VBoxEventType_T)j;
            if (implies(interested, candidate))
            {
                (*aEvMap)[j - FirstEvent].push_back(this);
            }
        }
    }

    ::RTCritSectInitEx(&mcsQLock, 0, NIL_RTLOCKVALCLASS, RTLOCKVAL_SUB_CLASS_ANY, NULL);
    ::RTSemEventCreate (&mQEvent);
}

ListenerRecord::~ListenerRecord()
{
    /* Remove references to us from the event map */
    EventMap* aEvMap = &mOwner->m->mEvMap;
    for (int j = FirstEvent; j < LastEvent; j++)
    {
        (*aEvMap)[j - FirstEvent].remove(this);
    }

    ::RTCritSectDelete(&mcsQLock);
    ::RTSemEventDestroy(mQEvent);
}

HRESULT ListenerRecord::process(IEvent* aEvent, BOOL aWaitable, PendingEventsMap::iterator& pit)
{
    if (mActive)
    {
        HRESULT rc =  mListener->HandleEvent(aEvent);
        if (aWaitable)
            eventProcessed(aEvent, pit);
        return rc;
    }
    else
        return enqueue(aEvent);
}


HRESULT ListenerRecord::enqueue (IEvent* aEvent)
{
    AssertMsg(!mActive, ("must be passive\n"));
    ::RTCritSectEnter(&mcsQLock);

    mQueue.push_back(aEvent);
    // notify waiters
    ::RTSemEventSignal(mQEvent);

    ::RTCritSectLeave(&mcsQLock);

    return S_OK;
}

HRESULT ListenerRecord::dequeue (IEvent* *aEvent, LONG aTimeout)
{
    AssertMsg(!mActive, ("must be passive\n"));

    ::RTCritSectEnter(&mcsQLock);
    if (mQueue.empty())
    {
        ::RTCritSectLeave(&mcsQLock);
        ::RTSemEventWait(mQEvent, aTimeout);
        ::RTCritSectEnter(&mcsQLock);
    }
    if (mQueue.empty())
    {
        *aEvent = NULL;
    }
    else
    {
        mQueue.front().queryInterfaceTo(aEvent);
        mQueue.pop_front();
    }
    ::RTCritSectLeave(&mcsQLock);
    return S_OK;
}

HRESULT ListenerRecord::eventProcessed (IEvent* aEvent, PendingEventsMap::iterator& pit)
{
    if (--pit->second == 0)
    {
        aEvent->SetProcessed();
        mOwner->m->mPendingMap.erase(pit);
    }

    Assert(pit->second >= 0);
    return S_OK;
}

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
    m->mListeners.clear();
    // m->mEvMap shall be cleared at this point too by destructors
}

STDMETHODIMP EventSource::RegisterListener(IEventListener * aListener,
                                           ComSafeArrayIn(VBoxEventType_T, aInterested),
                                           BOOL             aActive)
{
    CheckComArgNotNull(aListener);
    CheckComArgSafeArrayNotNull(aInterested);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    Listeners::const_iterator it = m->mListeners.find(aListener);
    if (it != m->mListeners.end())
        return setError(E_INVALIDARG,
                        tr("This listener already registered"));

    com::SafeArray<VBoxEventType_T> interested(ComSafeArrayInArg (aInterested));
    m->mListeners.insert(
        Listeners::value_type(aListener,
                              ListenerRecord(aListener, interested, aActive, this))
                         );

    return S_OK;
}

STDMETHODIMP EventSource::UnregisterListener(IEventListener * aListener)
{
    CheckComArgNotNull(aListener);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    Listeners::iterator it = m->mListeners.find(aListener);
    HRESULT rc;

    if (it != m->mListeners.end())
    {
        m->mListeners.erase(it);
        // destructor removes refs from the event map
        rc = S_OK;
    }
    else
    {
        rc = setError(VBOX_E_OBJECT_NOT_FOUND,
                      tr("Listener was never registered"));
    }

    return rc;
}

STDMETHODIMP EventSource::FireEvent(IEvent * aEvent,
                                    LONG     aTimeout,
                                    BOOL     *aProcessed)
{
    CheckComArgNotNull(aEvent);
    CheckComArgOutPointerValid(aProcessed);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    VBoxEventType_T evType;
    HRESULT hrc = aEvent->COMGETTER(Type)(&evType);
    AssertComRCReturn(hrc, VERR_ACCESS_DENIED);

    BOOL aWaitable = FALSE;
    aEvent->COMGETTER(Waitable)(&aWaitable);

    std::list<ListenerRecord*>& listeners = m->mEvMap[(int)evType-FirstEvent];

    uint32_t cListeners = listeners.size();
    PendingEventsMap::iterator pit;

    if (cListeners > 0 && aWaitable)
    {
        m->mPendingMap.insert(PendingEventsMap::value_type(aEvent, cListeners));
        // we keep it here to allow processing active listeners without pending events lookup
        pit = m->mPendingMap.find(aEvent);
    }
    for(std::list<ListenerRecord*>::const_iterator it = listeners.begin();
        it != listeners.end(); ++it)
    {
        ListenerRecord* record = *it;
        HRESULT cbRc;

        // @todo: callback under (read) lock, is it good?
        cbRc = record->process(aEvent, aWaitable, pit);
        // what to do with cbRc?
    }

    if (aWaitable)
        hrc = aEvent->WaitProcessed(aTimeout, aProcessed);
    else
        *aProcessed = TRUE;

    return hrc;
}


STDMETHODIMP EventSource::GetEvent(IEventListener * aListener,
                                   LONG      aTimeout,
                                   IEvent  * *aEvent)
{

    CheckComArgNotNull(aListener);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    Listeners::iterator it = m->mListeners.find(aListener);
    HRESULT rc;

    if (it != m->mListeners.end())
    {
        rc = it->second.dequeue(aEvent, aTimeout);
    }
    else
    {
        rc = setError(VBOX_E_OBJECT_NOT_FOUND,
                      tr("Listener was never registered"));
    }

    return rc;
}

STDMETHODIMP EventSource::EventProcessed(IEventListener * aListener,
                                         IEvent *         aEvent)
{
    CheckComArgNotNull(aListener);
    CheckComArgNotNull(aEvent);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    Listeners::iterator it = m->mListeners.find(aListener);
    HRESULT rc;

    BOOL aWaitable = FALSE;
    aEvent->COMGETTER(Waitable)(&aWaitable);

    if (it != m->mListeners.end())
    {
        ListenerRecord& aRecord = it->second;

        if (aRecord.mActive)
            return setError(E_INVALIDARG,
                        tr("Only applicable to passive listeners"));

        if (aWaitable)
        {
            PendingEventsMap::iterator pit = m->mPendingMap.find(aEvent);

            if (pit == m->mPendingMap.end())
            {
                AssertFailed();
                rc = setError(VBOX_E_OBJECT_NOT_FOUND,
                              tr("Unknown event"));
            }
            else
                rc = aRecord.eventProcessed(aEvent, pit);
        }
        else
        {
            // for non-waitable events we're done
            rc = S_OK;
        }
    }
    else
    {
        rc = setError(VBOX_E_OBJECT_NOT_FOUND,
                      tr("Listener was never registered"));
    }

    return rc;
}
