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

/** @page pg_main_events    Events
 *
 * Theory of operations.
 *
 * This code implements easily extensible event mechanism, letting us
 * to make any VirtualBox object an event source (by aggregating an EventSource instance).
 * Another entity could subscribe to the event source for events it is interested in.
 * If an event is waitable, it's possible to wait until all listeners
 * registered at the moment of firing event as ones interested in this
 * event acknowledged that they finished event processing (thus allowing
 * vetoable events).
 *
 * Listeners can be registered as active or passive ones, defining policy of delivery.
 * For *active* listeners, their HandleEvent() method is invoked when event is fired by
 * the event source (pretty much callbacks).
 * For *passive* listeners, it's up to an event consumer to perform GetEvent() operation
 * with given listener, and then perform desired operation with returned event, if any.
 * For passive listeners case, listener instance serves as merely a key referring to
 * particular event consumer, thus HandleEvent() implementation isn't that important.
 * IEventSource's CreateListener() could be used to create such a listener.
 * Passive mode is designed for transports not allowing callbacks, such as webservices
 * running on top of HTTP, and for situations where consumer wants exact control on
 * context where event handler is executed (such as GUI thread for some toolkits).
 *
 * Internal EventSource data structures are optimized for fast event delivery, while
 * listener registration/unregistration operations are expected being pretty rare.
 * Passive mode listeners keep an internal event queue for all events they receive,
 * and all waitable events are added to the pending events map. This map keeps track
 * of how many listeners are still not acknowledged their event, and once this counter
 * reach zero, element is removed from pending events map, and event is marked as processed.
 * Thus if passive listener's user forgets to call IEventSource's EventProcessed()
 * waiters may never know that event processing finished.
 */

#include <list>
#include <map>
#include <deque>

#include "EventImpl.h"
#include "AutoCaller.h"
#include "Logging.h"

#include <iprt/semaphore.h>
#include <iprt/critsect.h>
#include <iprt/asm.h>
#include <iprt/time.h>

#include <VBox/com/array.h>

class ListenerRecord;

struct VBoxEvent::Data
{
    Data()
        : mType(VBoxEventType_Invalid),
          mWaitEvent(NIL_RTSEMEVENT),
          mWaitable(FALSE),
          mProcessed(FALSE)
    {}

    VBoxEventType_T         mType;
    RTSEMEVENT              mWaitEvent;
    BOOL                    mWaitable;
    BOOL                    mProcessed;
    ComPtr<IEventSource>    mSource;
};

HRESULT VBoxEvent::FinalConstruct()
{
    m = new Data;
    return BaseFinalConstruct();
}

void VBoxEvent::FinalRelease()
{
    if (m)
    {
        uninit();
        delete m;
        m = 0;
        BaseFinalRelease();
    }
}

HRESULT VBoxEvent::init(IEventSource *aSource, VBoxEventType_T aType, BOOL aWaitable)
{
    HRESULT rc = S_OK;

    AssertReturn(aSource != NULL, E_INVALIDARG);

    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    m->mSource = aSource;
    m->mType = aType;
    m->mWaitable = aWaitable;
    m->mProcessed = !aWaitable;

    do {
        if (aWaitable)
        {
            int vrc = ::RTSemEventCreate(&m->mWaitEvent);

            if (RT_FAILURE(vrc))
            {
                AssertFailed ();
                return setError(E_FAIL,
                                tr("Internal error (%Rrc)"), vrc);
            }
        }
    } while (0);

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return rc;
}

void VBoxEvent::uninit()
{
    if (!m)
        return;

    m->mProcessed = TRUE;
    m->mType = VBoxEventType_Invalid;
    m->mSource.setNull();

    if (m->mWaitEvent != NIL_RTSEMEVENT)
    {
        Assert(m->mWaitable);
        ::RTSemEventDestroy(m->mWaitEvent);
        m->mWaitEvent = NIL_RTSEMEVENT;
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
        {
            *aResult = TRUE;
            return S_OK;
        }

        if (aTimeout == 0)
        {
            *aResult = m->mProcessed;
            return S_OK;
        }
    }

    /* @todo: maybe while loop for spurious wakeups? */
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

typedef std::list<Bstr> VetoList;
struct VBoxVetoEvent::Data
{
    Data()
        :
        mVetoed(FALSE)
    {}
    BOOL                    mVetoed;
    VetoList                mVetoList;
};

HRESULT VBoxVetoEvent::FinalConstruct()
{
    VBoxEvent::FinalConstruct();
    m = new Data;
    return S_OK;
}

void VBoxVetoEvent::FinalRelease()
{
    if (m)
    {
        uninit();
        delete m;
        m = 0;
    }
    VBoxEvent::FinalRelease();
}


HRESULT VBoxVetoEvent::init(IEventSource *aSource, VBoxEventType_T aType)
{
    HRESULT rc = S_OK;
    // all veto events are waitable
    rc = VBoxEvent::init(aSource, aType, TRUE);
    if (FAILED(rc)) return rc;

    m->mVetoed = FALSE;
    m->mVetoList.clear();

    return rc;
}

void VBoxVetoEvent::uninit()
{
    VBoxEvent::uninit();
    if (!m)
        return;
    m->mVetoed = FALSE;
}

STDMETHODIMP VBoxVetoEvent::AddVeto(IN_BSTR aVeto)
{
    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    if (aVeto)
        m->mVetoList.push_back(aVeto);

    m->mVetoed = TRUE;

    return S_OK;
}

STDMETHODIMP VBoxVetoEvent::IsVetoed(BOOL * aResult)
{
    CheckComArgOutPointerValid(aResult);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aResult = m->mVetoed;

    return S_OK;
}

STDMETHODIMP  VBoxVetoEvent::GetVetos(ComSafeArrayOut(BSTR, aVetos))
{
    if (ComSafeArrayOutIsNull(aVetos))
        return E_POINTER;

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    com::SafeArray<BSTR> vetos(m->mVetoList.size());
    int i = 0;
    for (VetoList::const_iterator it = m->mVetoList.begin();
         it != m->mVetoList.end();
         ++it, ++i)
    {
        const Bstr &str = *it;
        str.cloneTo(&vetos[i]);
    }
    vetos.detachTo(ComSafeArrayOutArg(aVetos));

    return S_OK;

}

static const int FirstEvent = (int)VBoxEventType_LastWildcard + 1;
static const int LastEvent  = (int)VBoxEventType_Last;
static const int NumEvents  = LastEvent - FirstEvent;

/**
 * Class replacing std::list and able to provide required stability
 * during iteration. It's acheived by delaying structural modifications
 * to the list till the moment particular element is no longer used by
 * current iterators.
 */
class EventMapRecord
{
public:
    /**
     * We have to be double linked, as structural modifications in list are delayed
     * till element removed, so we have to know our previous one to update its next
     */
    EventMapRecord* mNext;
    bool            mAlive;
private:
    EventMapRecord* mPrev;
    ListenerRecord* mRef; /* must be weak reference */
    int32_t         mRefCnt;

public:
    EventMapRecord(ListenerRecord* aRef)
        :
        mNext(0),
        mAlive(true),
        mPrev(0),
        mRef(aRef),
        mRefCnt(1)
    {}

    EventMapRecord(EventMapRecord& aOther)
    {
        mNext = aOther.mNext;
        mPrev = aOther.mPrev;
        mRef = aOther.mRef;
        mRefCnt = aOther.mRefCnt;
        mAlive = aOther.mAlive;
    }

    ~EventMapRecord()
    {
        if (mNext)
            mNext->mPrev = mPrev;
        if (mPrev)
            mPrev->mNext = mNext;
    }

    void addRef()
    {
        ASMAtomicIncS32(&mRefCnt);
    }

    void release()
    {
        if (ASMAtomicDecS32(&mRefCnt) <= 0) delete this;
    }

    // Called when an element is no longer needed
    void kill()
    {
        mAlive = false;
        release();
    }

    ListenerRecord* ref()
    {
        return mAlive ? mRef : 0;
    }

    friend class EventMapList;
};


class EventMapList
{
    EventMapRecord* mHead;
    uint32_t        mSize;
public:
    EventMapList()
        :
        mHead(0),
        mSize(0)
    {}
    ~EventMapList()
    {
        EventMapRecord* aCur = mHead;
        while (aCur)
        {
            EventMapRecord* aNext = aCur->mNext;
            aCur->release();
            aCur = aNext;
        }
    }

    /*
     * Elements have to be added to the front of the list, to make sure
     * that iterators doesn't see newly added listeners, and iteration
     * will always complete.
     */
    void add(ListenerRecord* aRec)
    {
        EventMapRecord* aNew = new EventMapRecord(aRec);
        aNew->mNext = mHead;
        if (mHead)
            mHead->mPrev = aNew;
        mHead = aNew;
        mSize++;
    }

    /*
     * Mark element as removed, actual removal could be delayed until
     * all consumers release it too. This helps to keep list stable
     * enough for iterators to allow long and probably intrusive callbacks.
     */
    void remove(ListenerRecord* aRec)
    {
        EventMapRecord* aCur = mHead;
        while (aCur)
        {
            EventMapRecord* aNext = aCur->mNext;
            if (aCur->ref() == aRec)
            {
                if (aCur == mHead)
                    mHead = aNext;
                aCur->kill();
                mSize--;
                // break?
            }
            aCur = aNext;
        }
    }

    uint32_t size() const
    {
        return mSize;
    }

    struct iterator
    {
      EventMapRecord* mCur;

      iterator()
      : mCur(0)
      {}

      explicit
      iterator(EventMapRecord* aCur)
      : mCur(aCur)
      {
          // Prevent element removal, till we're at it
          if (mCur)
              mCur->addRef();
      }

      ~iterator()
      {
          if (mCur)
              mCur->release();
      }

      ListenerRecord*
      operator*() const
      {
          return mCur->ref();
      }

      EventMapList::iterator&
      operator++()
      {
          EventMapRecord* aPrev = mCur;
          do {
              mCur = mCur->mNext;
          } while (mCur && !mCur->mAlive);

          // now we can safely release previous element
          aPrev->release();

          // And grab the new current
          if (mCur)
              mCur->addRef();

          return *this;
      }

      bool
      operator==(const EventMapList::iterator& aOther) const
      {
          return mCur == aOther.mCur;
      }

      bool
      operator!=(const EventMapList::iterator& aOther) const
      {
          return mCur != aOther.mCur;
      }
    };

    iterator begin()
    {
        return iterator(mHead);
    }

    iterator end()
    {
        return iterator(0);
    }
};

typedef EventMapList EventMap[NumEvents];
typedef std::map<IEvent*, int32_t> PendingEventsMap;
typedef std::deque<ComPtr<IEvent> > PassiveQueue;

class ListenerRecord
{
private:
    ComPtr<IEventListener>        mListener;
    BOOL                          mActive;
    EventSource*                  mOwner;

    RTSEMEVENT                    mQEvent;
    RTCRITSECT                    mcsQLock;
    PassiveQueue                  mQueue;
    int32_t volatile              mRefCnt;
    uint64_t                      mLastRead;

public:
    ListenerRecord(IEventListener*                    aListener,
                   com::SafeArray<VBoxEventType_T>&   aInterested,
                   BOOL                               aActive,
                   EventSource*                       aOwner);
    ~ListenerRecord();

    HRESULT process(IEvent* aEvent, BOOL aWaitable, PendingEventsMap::iterator& pit, AutoLockBase& alock);
    HRESULT enqueue(IEvent* aEvent);
    HRESULT dequeue(IEvent* *aEvent, LONG aTimeout, AutoLockBase& aAlock);
    HRESULT eventProcessed(IEvent * aEvent, PendingEventsMap::iterator& pit);
    void addRef()
    {
        ASMAtomicIncS32(&mRefCnt);
    }
    void release()
    {
        if (ASMAtomicDecS32(&mRefCnt) <= 0) delete this;
    }
    BOOL isActive()
    {
        return mActive;
    }

    friend class EventSource;
};

/* Handy class with semantics close to ComPtr, but for list records */
template<typename Held>
class RecordHolder
{
public:
    RecordHolder(Held* lr)
    :
    held(lr)
    {
        addref();
    }
    RecordHolder(const RecordHolder& that)
    :
    held(that.held)
    {
        addref();
    }
    RecordHolder()
    :
    held(0)
    {
    }
    ~RecordHolder()
    {
        release();
    }

    Held* obj()
    {
        return held;
    }

    RecordHolder &operator=(const RecordHolder &that)
    {
        safe_assign(that.held);
        return *this;
    }
private:
    Held* held;

    void addref()
    {
        if (held)
            held->addRef();
    }
    void release()
    {
        if (held)
            held->release();
    }
    void safe_assign (Held *that_p)
    {
        if (that_p)
            that_p->addRef();
        release();
        held = that_p;
    }
};

typedef std::map<IEventListener*, RecordHolder<ListenerRecord> >  Listeners;

struct EventSource::Data
{
    Data() {}
    Listeners                     mListeners;
    EventMap                      mEvMap;
    PendingEventsMap              mPendingMap;
};

/**
 * This function defines what wildcard expands to.
 */
static BOOL implies(VBoxEventType_T who, VBoxEventType_T what)
{
    switch (who)
    {
        case VBoxEventType_Any:
            return TRUE;
        case VBoxEventType_Vetoable:
            return     (what == VBoxEventType_OnExtraDataCanChange)
                    || (what == VBoxEventType_OnCanShowWindow);
        case VBoxEventType_MachineEvent:
            return     (what == VBoxEventType_OnMachineStateChanged)
                    || (what == VBoxEventType_OnMachineDataChanged)
                    || (what == VBoxEventType_OnMachineRegistered)
                    || (what == VBoxEventType_OnSessionStateChanged)
                    || (what == VBoxEventType_OnGuestPropertyChanged);
        case VBoxEventType_SnapshotEvent:
            return     (what == VBoxEventType_OnSnapshotTaken)
                    || (what == VBoxEventType_OnSnapshotDeleted)
                    || (what == VBoxEventType_OnSnapshotChanged)
                    ;
        case VBoxEventType_InputEvent:
            return     (what == VBoxEventType_OnKeyboardLedsChanged)
                    || (what == VBoxEventType_OnMousePointerShapeChanged)
                    || (what == VBoxEventType_OnMouseCapabilityChanged)
                    ;
        case VBoxEventType_Invalid:
            return FALSE;
        default:
            return who == what;
    }
}

ListenerRecord::ListenerRecord(IEventListener*                  aListener,
                               com::SafeArray<VBoxEventType_T>& aInterested,
                               BOOL                             aActive,
                               EventSource*                     aOwner)
    :
    mActive(aActive),
    mOwner(aOwner),
    mRefCnt(0)
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
                (*aEvMap)[j - FirstEvent].add(this);
            }
        }
    }

    if (!mActive)
    {
        ::RTCritSectInit(&mcsQLock);
        ::RTSemEventCreate (&mQEvent);
        mLastRead = RTTimeMilliTS();
    }
    else
    {
        mQEvent =NIL_RTSEMEVENT;
        RT_ZERO(mcsQLock);
        mLastRead = 0;
    }
}

ListenerRecord::~ListenerRecord()
{
    /* Remove references to us from the event map */
    EventMap* aEvMap = &mOwner->m->mEvMap;
    for (int j = FirstEvent; j < LastEvent; j++)
    {
        (*aEvMap)[j - FirstEvent].remove(this);
    }

    if (!mActive)
    {
        // at this moment nobody could add elements to our queue, so we can safely
        // clean it up, otherwise there will be pending events map elements
        PendingEventsMap* aPem = &mOwner->m->mPendingMap;
        while (true)
        {
            ComPtr<IEvent> aEvent;

            if (mQueue.empty())
                break;

            mQueue.front().queryInterfaceTo(aEvent.asOutParam());
            mQueue.pop_front();

            BOOL aWaitable = FALSE;
            aEvent->COMGETTER(Waitable)(&aWaitable);
            if (aWaitable)
            {
                PendingEventsMap::iterator pit = aPem->find(aEvent);
                if (pit != aPem->end())
                    eventProcessed(aEvent, pit);
            }
        }

        ::RTCritSectDelete(&mcsQLock);
        ::RTSemEventDestroy(mQEvent);
    }
}

HRESULT ListenerRecord::process(IEvent*                     aEvent,
                                BOOL                        aWaitable,
                                PendingEventsMap::iterator& pit,
                                AutoLockBase&               aAlock)
{
    if (mActive)
    {
        /*
         * We release lock here to allow modifying ops on EventSource inside callback.
         */
        HRESULT rc =  S_OK;
        if (mListener)
        {
            aAlock.release();
            rc =  mListener->HandleEvent(aEvent);
#ifdef RT_OS_WINDOWS
            Assert(rc != RPC_E_WRONG_THREAD);
#endif
            aAlock.acquire();
        }
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

    // put an event the queue
    ::RTCritSectEnter(&mcsQLock);

    // If there was no events reading from the listener for the long time,
    // and events keep coming, or queue is oversized we shall unregister this listener.
    uint64_t sinceRead = RTTimeMilliTS() - mLastRead;
    size_t queueSize = mQueue.size();
    if ( (queueSize > 1000) || ((queueSize > 500) && (sinceRead > 60 * 1000)))
    {
        ::RTCritSectLeave(&mcsQLock);
        return E_ABORT;
    }


    if (queueSize != 0 && mQueue.back() == aEvent)
        /* if same event is being pushed multiple times - it's reusable event and
           we don't really need multiple instances of it in the queue */
        (void)aEvent;
    else
        mQueue.push_back(aEvent);

    ::RTCritSectLeave(&mcsQLock);

     // notify waiters
    ::RTSemEventSignal(mQEvent);

    return S_OK;
}

HRESULT ListenerRecord::dequeue (IEvent*       *aEvent,
                                 LONG          aTimeout,
                                 AutoLockBase& aAlock)
{
    if (mActive)
        return VBOX_E_INVALID_OBJECT_STATE;

    // retain listener record
    RecordHolder<ListenerRecord> holder(this);

    ::RTCritSectEnter(&mcsQLock);

    mLastRead = RTTimeMilliTS();

    if (mQueue.empty())    {
        ::RTCritSectLeave(&mcsQLock);
        // Speed up common case
        if (aTimeout == 0)
        {
            *aEvent = NULL;
            return S_OK;
        }
        // release lock while waiting, listener will not go away due to above holder
        aAlock.release();
        ::RTSemEventWait(mQEvent, aTimeout);
        // reacquire lock
        aAlock.acquire();
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
        Assert(pit->first == aEvent);
        aEvent->SetProcessed();
        mOwner->m->mPendingMap.erase(pit);
    }

    return S_OK;
}

EventSource::EventSource()
{}

EventSource::~EventSource()
{}

HRESULT EventSource::FinalConstruct()
{
    m = new Data;
    return BaseFinalConstruct();
}

void EventSource::FinalRelease()
{
    uninit();
    delete m;
    BaseFinalRelease();
}

HRESULT EventSource::init(IUnknown *)
{
    HRESULT rc = S_OK;

    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();
    return rc;
}

void EventSource::uninit()
{
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;
    m->mListeners.clear();
    // m->mEvMap shall be cleared at this point too by destructors, assert?
}

STDMETHODIMP EventSource::RegisterListener(IEventListener * aListener,
                                           ComSafeArrayIn(VBoxEventType_T, aInterested),
                                           BOOL             aActive)
{
    CheckComArgNotNull(aListener);
    CheckComArgSafeArrayNotNull(aInterested);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

        Listeners::const_iterator it = m->mListeners.find(aListener);
        if (it != m->mListeners.end())
            return setError(E_INVALIDARG,
                            tr("This listener already registered"));

        com::SafeArray<VBoxEventType_T> interested(ComSafeArrayInArg (aInterested));
        RecordHolder<ListenerRecord> lrh(new ListenerRecord(aListener, interested, aActive, this));
        m->mListeners.insert(Listeners::value_type(aListener, lrh));
    }

    VBoxEventDesc evDesc;
    evDesc.init(this, VBoxEventType_OnEventSourceChanged, aListener, TRUE);
    evDesc.fire(0);

    return S_OK;
}

STDMETHODIMP EventSource::UnregisterListener(IEventListener * aListener)
{
    CheckComArgNotNull(aListener);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT rc;
    {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

        Listeners::iterator it = m->mListeners.find(aListener);

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
    }

    if (SUCCEEDED(rc))
    {
        VBoxEventDesc evDesc;
        evDesc.init(this, VBoxEventType_OnEventSourceChanged, aListener, FALSE);
        evDesc.fire(0);
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

    HRESULT hrc;
    BOOL aWaitable = FALSE;
    aEvent->COMGETTER(Waitable)(&aWaitable);

    do {
        AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

        VBoxEventType_T evType;
        hrc = aEvent->COMGETTER(Type)(&evType);
        AssertComRCReturn(hrc, hrc);

        EventMapList& listeners = m->mEvMap[(int)evType-FirstEvent];

        /* Anyone interested in this event? */
        uint32_t cListeners = listeners.size();
        if (cListeners == 0)
        {
            aEvent->SetProcessed();
            break; // just leave the lock and update event object state
        }

        PendingEventsMap::iterator pit;

        if (aWaitable)
        {
            m->mPendingMap.insert(PendingEventsMap::value_type(aEvent, cListeners));
            // we keep iterator here to allow processing active listeners without
            // pending events lookup
            pit = m->mPendingMap.find(aEvent);
        }
        for(EventMapList::iterator it = listeners.begin();
            it != listeners.end(); ++it)
        {
            HRESULT cbRc;
            // keep listener record reference, in case someone will remove it while in callback
            RecordHolder<ListenerRecord> record(*it);

            /*
             * We pass lock here to allow modifying ops on EventSource inside callback
             * in active mode. Note that we expect list iterator stability as 'alock'
             * could be temporary released when calling event handler.
             */
            cbRc = record.obj()->process(aEvent, aWaitable, pit, alock);

            /* Note that E_ABORT is used above to signal that a passive
             * listener was unregistered due to not picking up its event.
             * This overlaps with XPCOM specific use of E_ABORT to signal
             * death of an active listener, but that's irrelevant here. */
            if (FAILED_DEAD_INTERFACE(cbRc) || cbRc == E_ABORT)
            {
                Listeners::iterator lit = m->mListeners.find(record.obj()->mListener);
                if (lit != m->mListeners.end())
                    m->mListeners.erase(lit);
            }
            // anything else to do with cbRc?
        }
    } while (0);
    /* We leave the lock here */

    if (aWaitable)
        hrc = aEvent->WaitProcessed(aTimeout, aProcessed);
    else
        *aProcessed = TRUE;

    return hrc;
}


STDMETHODIMP EventSource::GetEvent(IEventListener * aListener,
                                   LONG             aTimeout,
                                   IEvent  **       aEvent)
{

    CheckComArgNotNull(aListener);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    Listeners::iterator it = m->mListeners.find(aListener);
    HRESULT rc;

    if (it != m->mListeners.end())
        rc = it->second.obj()->dequeue(aEvent, aTimeout, alock);
    else
        rc = setError(VBOX_E_OBJECT_NOT_FOUND,
                      tr("Listener was never registered"));

    if (rc == VBOX_E_INVALID_OBJECT_STATE)
        return setError(rc, tr("Listener must be passive"));

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
        ListenerRecord* aRecord = it->second.obj();

        if (aRecord->isActive())
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
                rc = aRecord->eventProcessed(aEvent, pit);
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

/**
 * This class serves as feasible listener implementation
 * which could be used by clients not able to create local
 * COM objects, but still willing to receive event
 * notifications in passive mode, such as webservices.
 */
class ATL_NO_VTABLE PassiveEventListener :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IEventListener)
{
public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(PassiveEventListener, IEventListener)

    DECLARE_NOT_AGGREGATABLE(PassiveEventListener)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(PassiveEventListener)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IEventListener)
    END_COM_MAP()

    PassiveEventListener()
    {}
    ~PassiveEventListener()
    {}

    HRESULT FinalConstruct()
    {
        return BaseFinalConstruct();
    }
    void FinalRelease()
    {
        BaseFinalRelease();
    }

    // IEventListener methods
    STDMETHOD(HandleEvent)(IEvent *)
    {
        ComAssertMsgRet(false, ("HandleEvent() of wrapper shall never be called"),
                        E_FAIL);
    }
};

/* Proxy listener class, used to aggregate multiple event sources into one */
class ATL_NO_VTABLE ProxyEventListener :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IEventListener)
{
    ComPtr<IEventSource> mSource;
public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(ProxyEventListener, IEventListener)

    DECLARE_NOT_AGGREGATABLE(ProxyEventListener)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(ProxyEventListener)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IEventListener)
    END_COM_MAP()

    ProxyEventListener()
    {}
    ~ProxyEventListener()
    {}

    HRESULT FinalConstruct()
    {
        return BaseFinalConstruct();
    }
    void FinalRelease()
    {
        BaseFinalRelease();
    }

    HRESULT init(IEventSource* aSource)
    {
        mSource = aSource;
        return S_OK;
    }

    // IEventListener methods
    STDMETHOD(HandleEvent)(IEvent * aEvent)
    {
        BOOL fProcessed = FALSE;
        if (mSource)
            return mSource->FireEvent(aEvent, 0, &fProcessed);
        else
            return S_OK;
    }
};

class ATL_NO_VTABLE EventSourceAggregator :
    public VirtualBoxBase,
    VBOX_SCRIPTABLE_IMPL(IEventSource)
{
    typedef std::list <ComPtr<IEventSource> > EventSourceList;
    /* key is weak reference */
    typedef std::map<IEventListener*, ComPtr<IEventListener> > ProxyListenerMap;

    EventSourceList           mEventSources;
    ProxyListenerMap          mListenerProxies;
    ComObjPtr<EventSource>    mSource;

public:

    VIRTUALBOXBASE_ADD_ERRORINFO_SUPPORT(EventSourceAggregator, IEventSource)

    DECLARE_NOT_AGGREGATABLE(EventSourceAggregator)

    DECLARE_PROTECT_FINAL_CONSTRUCT()

    BEGIN_COM_MAP(EventSourceAggregator)
        VBOX_DEFAULT_INTERFACE_ENTRIES(IEventSource)
    END_COM_MAP()

    EventSourceAggregator()
    {}
    ~EventSourceAggregator()
    {}

    HRESULT FinalConstruct()
    {
        return BaseFinalConstruct();
    }
    void FinalRelease()
    {
        mEventSources.clear();
        mListenerProxies.clear();
        mSource->uninit();
        BaseFinalRelease();
    }

    // internal public
    HRESULT init(ComSafeArrayIn(IEventSource *, aSources));

    // IEventSource methods
    STDMETHOD(CreateListener)(IEventListener ** aListener);
    STDMETHOD(CreateAggregator)(ComSafeArrayIn(IEventSource*, aSubordinates),
                                IEventSource **               aAggregator);
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

  protected:
    HRESULT createProxyListener(IEventListener * aListener,
                                IEventListener * *aProxy);
    HRESULT getProxyListener   (IEventListener * aListener,
                                IEventListener * *aProxy);
    HRESULT removeProxyListener(IEventListener * aListener);
};

#ifdef VBOX_WITH_XPCOM
NS_DECL_CLASSINFO(ProxyEventListener)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(ProxyEventListener, IEventListener)
NS_DECL_CLASSINFO(PassiveEventListener)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(PassiveEventListener, IEventListener)
NS_DECL_CLASSINFO(VBoxEvent)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(VBoxEvent, IEvent)
NS_DECL_CLASSINFO(VBoxVetoEvent)
NS_IMPL_ISUPPORTS_INHERITED1(VBoxVetoEvent, VBoxEvent, IVetoEvent)
NS_DECL_CLASSINFO(EventSource)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(EventSource, IEventSource)
NS_DECL_CLASSINFO(EventSourceAggregator)
NS_IMPL_THREADSAFE_ISUPPORTS1_CI(EventSourceAggregator, IEventSource)
#endif


STDMETHODIMP EventSource::CreateListener(IEventListener ** aListener)
{
    CheckComArgOutPointerValid(aListener);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ComObjPtr<PassiveEventListener> listener;

    HRESULT rc = listener.createObject();
    ComAssertMsgRet(SUCCEEDED(rc), ("Could not create wrapper object (%Rrc)", rc),
                    E_FAIL);
    listener.queryInterfaceTo(aListener);
    return S_OK;
}


STDMETHODIMP EventSource::CreateAggregator(ComSafeArrayIn(IEventSource*, aSubordinates),
                                           IEventSource **               aResult)
{
    CheckComArgOutPointerValid(aResult);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    ComObjPtr<EventSourceAggregator> agg;

    HRESULT rc = agg.createObject();
    ComAssertMsgRet(SUCCEEDED(rc), ("Could not create aggregator (%Rrc)", rc),
                    E_FAIL);

    rc = agg->init(ComSafeArrayInArg(aSubordinates));
    if (FAILED(rc))
        return rc;


    agg.queryInterfaceTo(aResult);
    return S_OK;
}

HRESULT  EventSourceAggregator::init(ComSafeArrayIn(IEventSource*, aSourcesIn))
{
    HRESULT rc;

    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);

    rc = mSource.createObject();
    ComAssertMsgRet(SUCCEEDED(rc), ("Could not create source (%Rrc)", rc),
                    E_FAIL);
    rc = mSource->init((IEventSource*)this);
    ComAssertMsgRet(SUCCEEDED(rc), ("Could not init source (%Rrc)", rc),
                    E_FAIL);

    com::SafeIfaceArray<IEventSource> aSources(ComSafeArrayInArg (aSourcesIn));

    size_t cSize = aSources.size();

    for (size_t i = 0; i < cSize; i++)
    {
        if (aSources[i] != NULL)
            mEventSources.push_back(aSources[i]);
    }

    /* Confirm a successful initialization */
    autoInitSpan.setSucceeded();

    return rc;
}

STDMETHODIMP EventSourceAggregator::CreateListener(IEventListener ** aListener)
{
    return mSource->CreateListener(aListener);
}

STDMETHODIMP EventSourceAggregator::CreateAggregator(ComSafeArrayIn(IEventSource*, aSubordinates),
                                                     IEventSource **               aResult)
{
    return mSource->CreateAggregator(ComSafeArrayInArg(aSubordinates), aResult);
}

STDMETHODIMP EventSourceAggregator::RegisterListener(IEventListener * aListener,
                                                     ComSafeArrayIn(VBoxEventType_T, aInterested),
                                                     BOOL             aActive)
{
    CheckComArgNotNull(aListener);
    CheckComArgSafeArrayNotNull(aInterested);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT rc;

    ComPtr<IEventListener> proxy;
    rc = createProxyListener(aListener, proxy.asOutParam());
    if (FAILED(rc))
        return rc;

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    for (EventSourceList::const_iterator it = mEventSources.begin(); it != mEventSources.end();
         ++it)
    {
        ComPtr<IEventSource> es = *it;
        /* Register active proxy listener on real event source */
        rc = es->RegisterListener(proxy, ComSafeArrayInArg(aInterested), TRUE);
    }
    /* And add real listener on our event source */
    rc = mSource->RegisterListener(aListener, ComSafeArrayInArg(aInterested), aActive);

    rc = S_OK;

    return rc;
}

STDMETHODIMP EventSourceAggregator::UnregisterListener(IEventListener * aListener)
{
    CheckComArgNotNull(aListener);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT rc = S_OK;

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    ComPtr<IEventListener> proxy;
    rc = getProxyListener(aListener, proxy.asOutParam());
    if (FAILED(rc))
        return rc;

    for (EventSourceList::const_iterator it = mEventSources.begin(); it != mEventSources.end();
         ++it)
    {
        ComPtr<IEventSource> es = *it;
        rc = es->UnregisterListener(proxy);
    }
    rc = mSource->UnregisterListener(aListener);

    return removeProxyListener(aListener);

}

STDMETHODIMP EventSourceAggregator::FireEvent(IEvent * aEvent,
                                              LONG     aTimeout,
                                              BOOL     *aProcessed)
{
    CheckComArgNotNull(aEvent);
    CheckComArgOutPointerValid(aProcessed);

    AutoCaller autoCaller(this);
    if (FAILED(autoCaller.rc())) return autoCaller.rc();

    HRESULT rc = S_OK;
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);
    /* Aggresgator event source shalln't have direct event firing, but we may
       wish to support aggregation chains */
    for (EventSourceList::const_iterator it = mEventSources.begin(); it != mEventSources.end();
         ++it)
    {
        ComPtr<IEventSource> es = *it;
        rc = es->FireEvent(aEvent, aTimeout, aProcessed);
        /* Current behavior is that aggregator's FireEvent() always succeeds,
           so that multiple event sources don't affect each other. */
        NOREF(rc);
    }

    return S_OK;
}

STDMETHODIMP EventSourceAggregator::GetEvent(IEventListener * aListener,
                                             LONG             aTimeout,
                                             IEvent  **       aEvent)
{
    return mSource->GetEvent(aListener, aTimeout, aEvent);
}

STDMETHODIMP EventSourceAggregator::EventProcessed(IEventListener * aListener,
                                                   IEvent *         aEvent)
{
    return mSource->EventProcessed(aListener, aEvent);
}

HRESULT EventSourceAggregator::createProxyListener(IEventListener * aListener,
                                                   IEventListener * *aProxy)
{
    ComObjPtr<ProxyEventListener> proxy;

    HRESULT rc = proxy.createObject();
    ComAssertMsgRet(SUCCEEDED(rc), ("Could not create proxy (%Rrc)", rc),
                    E_FAIL);

    rc = proxy->init(mSource);
    if (FAILED(rc))
        return rc;

    ProxyListenerMap::const_iterator it = mListenerProxies.find(aListener);
    if (it != mListenerProxies.end())
        return setError(E_INVALIDARG,
                        tr("This listener already registered"));

    mListenerProxies.insert(ProxyListenerMap::value_type(aListener, proxy));

    proxy.queryInterfaceTo(aProxy);
    return S_OK;
}

HRESULT EventSourceAggregator::getProxyListener(IEventListener * aListener,
                                                IEventListener * *aProxy)
{
    ProxyListenerMap::const_iterator it = mListenerProxies.find(aListener);
    if (it == mListenerProxies.end())
        return setError(E_INVALIDARG,
                        tr("This listener never registered"));

    (*it).second.queryInterfaceTo(aProxy);
    return S_OK;
}

HRESULT EventSourceAggregator::removeProxyListener(IEventListener * aListener)
{
    ProxyListenerMap::iterator it = mListenerProxies.find(aListener);
    if (it == mListenerProxies.end())
        return setError(E_INVALIDARG,
                        tr("This listener never registered"));

    mListenerProxies.erase(it);
    return S_OK;
}
