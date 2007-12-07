/** @file
 *
 * MS COM / XPCOM Abstraction Layer:
 * Event and EventQueue class declaration
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VBox/com/EventQueue.h"

namespace com
{

// EventQueue class
////////////////////////////////////////////////////////////////////////////////

#if defined (RT_OS_WINDOWS)

#define CHECK_THREAD_RET(ret) \
    do { \
        AssertMsg (GetCurrentThreadId() == mThreadId, ("Must be on event queue thread!")); \
        if (GetCurrentThreadId() != mThreadId) \
            return ret; \
    } while (0)

#else // !defined (RT_OS_WINDOWS)

#define CHECK_THREAD_RET(ret) \
    do { \
        if (!mEventQ) \
            return ret; \
        BOOL isOnCurrentThread = FALSE; \
        mEventQ->IsOnCurrentThread (&isOnCurrentThread); \
        AssertMsg (isOnCurrentThread, ("Must be on event queue thread!")); \
        if (!isOnCurrentThread) \
            return ret; \
    } while (0)

#endif // !defined (RT_OS_WINDOWS)

/**
 *  Constructs an event queue for the current thread.
 *
 *  Currently, there can be only one event queue per thread, so if an event
 *  queue for the current thread already exists, this object is simply attached
 *  to the existing event queue.
 */
EventQueue::EventQueue()
{
#if defined (RT_OS_WINDOWS)

    mThreadId = GetCurrentThreadId();
    // force the system to create the message queue for the current thread
    MSG msg;
    PeekMessage (&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

#else

    mEQCreated = FALSE;

    mLastEvent = NULL;
    mGotEvent = FALSE;

    // Here we reference the global nsIEventQueueService instance and hold it
    // until we're destroyed. This is necessary to keep NS_ShutdownXPCOM() away
    // from calling StopAcceptingEvents() on all event queues upon destruction of
    // nsIEventQueueService, and makes sense when, for some reason, this happens
    // *before* we're able to send a NULL event to stop our event handler thread
    // when doing unexpected cleanup caused indirectly by NS_ShutdownXPCOM()
    // that is performing a global cleanup of everything. A good example of such
    // situation is when NS_ShutdownXPCOM() is called while the VirtualBox component
    // is still alive (because it is still referenced): eventually, it results in
    // a VirtualBox::uninit() call from where it is already not possible to post
    // NULL to the event thread (because it stopped accepting events).

    nsresult rc = NS_GetEventQueueService (getter_AddRefs (mEventQService));

    if (NS_SUCCEEDED (rc))
    {
        rc = mEventQService->GetThreadEventQueue (NS_CURRENT_THREAD,
                                                  getter_AddRefs (mEventQ));
        if (rc == NS_ERROR_NOT_AVAILABLE)
        {
            rc = mEventQService->CreateMonitoredThreadEventQueue();
            if (NS_SUCCEEDED (rc))
            {
                mEQCreated = TRUE;
                rc = mEventQService->GetThreadEventQueue (NS_CURRENT_THREAD,
                                                          getter_AddRefs (mEventQ));
            }
        }
    }
    AssertComRC (rc);

#endif
}

EventQueue::~EventQueue()
{
#if defined (RT_OS_WINDOWS)
#else
    // process all pending events before destruction
    if (mEventQ)
    {
        if (mEQCreated)
        {
            mEventQ->StopAcceptingEvents();
            mEventQ->ProcessPendingEvents();
            mEventQService->DestroyThreadEventQueue();
        }
        mEventQ = nsnull;
        mEventQService = nsnull;
    }
#endif
}

/**
 *  Posts an event to this event loop asynchronously.
 *
 *  @param  event   the event to post, must be allocated using |new|
 *  @return         TRUE if successful and false otherwise
 */
BOOL EventQueue::postEvent (Event *event)
{
#if defined (RT_OS_WINDOWS)

    return PostThreadMessage (mThreadId, WM_USER, (WPARAM) event, NULL);

#else

    if (!mEventQ)
        return FALSE;

    MyPLEvent *ev = new MyPLEvent (event);
    mEventQ->InitEvent (ev, this, plEventHandler, plEventDestructor);
    HRESULT rc = mEventQ->PostEvent (ev);
    return NS_SUCCEEDED (rc);

#endif
}

/**
 *  Waits for a single event.
 *  This method must be called on the same thread where this event queue
 *  is created.
 *
 *  After this method returns TRUE and non-NULL event, the caller should call
 *  #handleEvent() in order to process the returned event (otherwise the event
 *  is just removed from the queue, but not processed).
 *
 *  There is a special case when the returned event is NULL (and the method
 *  returns TRUE), meaning that this event queue must finish its execution
 *  (i.e., quit the event loop),
 *
 *  @param event    next event removed from the queue
 *  @return         TRUE if successful and false otherwise
 */
BOOL EventQueue::waitForEvent (Event **event)
{
    Assert (event);
    if (!event)
        return FALSE;

    *event = NULL;

    CHECK_THREAD_RET (FALSE);

#if defined (RT_OS_WINDOWS)

    MSG msg;
    BOOL rc = GetMessage (&msg, NULL, WM_USER, WM_USER);
    // check for error
    if (rc == -1)
        return FALSE;
    // check for WM_QUIT
    if (!rc)
        return TRUE;

    // retrieve our event
    *event = (Event *) msg.wParam;

#else

    PLEvent *ev = NULL;
    HRESULT rc;

    mGotEvent = FALSE;

    do
    {
        rc = mEventQ->WaitForEvent (&ev);
        // check for error
        if (FAILED (rc))
            return FALSE;
        // check for EINTR signal
        if (!ev)
            return TRUE;

        // run PLEvent handler. This will just set mLastEvent if it is an
        // MyPLEvent instance, and then delete ev.
        mEventQ->HandleEvent (ev);
    }
    while (!mGotEvent);

    // retrieve our event
    *event = mLastEvent;

#endif

    return TRUE;
}

/**
 *  Handles the given event and |delete|s it.
 *  This method must be called on the same thread where this event queue
 *  is created.
 */
BOOL EventQueue::handleEvent (Event *event)
{
    Assert (event);
    if (!event)
        return FALSE;

    CHECK_THREAD_RET (FALSE);

    event->handler();
    delete event;

    return TRUE;
}

} /* namespace com */

