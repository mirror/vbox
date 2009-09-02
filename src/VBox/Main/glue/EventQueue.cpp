/* $Id$ */

/** @file
 *
 * MS COM / XPCOM Abstraction Layer:
 * Event and EventQueue class declaration
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include "VBox/com/EventQueue.h"

#ifdef RT_OS_DARWIN
# include <CoreFoundation/CFRunLoop.h>
#endif

#if defined(VBOX_WITH_XPCOM) && !defined(RT_OS_DARWIN) && !defined(RT_OS_OS2)
# define USE_XPCOM_QUEUE
#endif

#include <iprt/err.h>
#include <iprt/time.h>
#include <iprt/thread.h>
#ifdef USE_XPCOM_QUEUE
# include <errno.h>
#endif

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

    if (NS_SUCCEEDED(rc))
    {
        rc = mEventQService->GetThreadEventQueue (NS_CURRENT_THREAD,
                                                  getter_AddRefs (mEventQ));
        if (rc == NS_ERROR_NOT_AVAILABLE)
        {
            rc = mEventQService->CreateMonitoredThreadEventQueue();
            if (NS_SUCCEEDED(rc))
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
    return NS_SUCCEEDED(rc);

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


#ifdef VBOX_WITH_XPCOM

/** Wrapper around nsIEventQueue::PendingEvents. */
DECLINLINE(bool) hasEventQueuePendingEvents(nsIEventQueue *pQueue)
{
    PRBool fHasEvents = PR_FALSE;
    nsresult rc = pQueue->PendingEvents(&fHasEvents);
    return NS_SUCCEEDED(rc) && fHasEvents ? true : false;
}

/** Wrapper around nsIEventQueue::IsQueueNative. */
DECLINLINE(bool) isEventQueueNative(nsIEventQueue *pQueue)
{
    PRBool fIsNative = PR_FALSE;
    nsresult rc = pQueue->IsQueueNative(&fIsNative);
    return NS_SUCCEEDED(rc) && fIsNative ? true : false;
}

/** Wrapper around nsIEventQueue::ProcessPendingEvents. */
DECLINLINE(void) processPendingEvents(nsIEventQueue *pQueue)
{
    pQueue->ProcessPendingEvents();
}

#else

/** For automatic cleanup.  */
class MyThreadHandle
{
public:
    HANDLE mh;

    MyThreadHandle(HANDLE hThread)
    {
        if (!DuplicateHandle(GetCurrentProcess(), hThread, GetCurrentProcess(),
                             &mh, 0 /*dwDesiredAccess*/, FALSE /*bInheritHandle*/,
                             DUPLICATE_SAME_ACCESS))
            mh = INVALID_HANDLE_VALUE;
    }

    ~MyThreadHandle()
    {
        CloseHandle(mh);
        mh = INVALID_HANDLE_VALUE;
    }
};

/** COM version of nsIEventQueue::PendingEvents. */
DECLINLINE(bool) hasEventQueuePendingEvents(MyThreadHandle &Handle)
{
    DWORD rc = MsgWaitForMultipleObjects(1, &Handle.mh, TRUE /*fWaitAll*/, 0 /*ms*/, QS_ALLINPUT);
    return rc == WAIT_OBJECT_0;
}

/** COM version of nsIEventQueue::IsQueueNative, the question doesn't make
 *  sense and we have to return false for the code below to work. */
DECLINLINE(bool) isEventQueueNative(MyThreadHandle const &Handle)
{
    return false;
}

/** COM version of nsIEventQueue::ProcessPendingEvents. */
static void processPendingEvents(MyThreadHandle const &Handle)
{
    /*
     * Process pending thead messages.
     */
    MSG Msg;
    while (PeekMessage(&Msg, NULL /*hWnd*/, 0 /*wMsgFilterMin*/, 0 /*wMsgFilterMax*/, PM_REMOVE))
    {
        if (Msg.message == WM_QUIT)
            return /*VERR_INTERRUPTED*/;
        DispatchMessage(&Msg);
    }
}

#endif /* VBOX_WITH_XPCOM */

/**
 *  Processes events for the current thread.
 *
 *  @param cMsTimeout       The timeout in milliseconds or RT_INDEFINITE_WAIT.
 *  @param pfnExitCheck     Optional callback for checking for some exit condition
 *                          while looping.  Note that this may be called
 *  @param pvUser           User argument for pfnExitCheck.
 *  @param cMsPollInterval  The interval cMsTimeout should be called at. 0 means
 *                          never default.
 *  @param fReturnOnEvent   If true, return immediately after some events has
 *                          been processed. If false, process events until we
 *                          time out, pfnExitCheck returns true, interrupted or
 *                          the queue receives some kind of quit message.
 *
 *  @returns VBox status code.
 *  @retval VINF_SUCCESS if events were processed.
 *  @retval VERR_TIMEOUT if no events before cMsTimeout elapsed.
 *  @retval VERR_INTERRUPTED if the wait was interrupted by a signal or other
 *          async event.
 *  @retval VERR_NOT_FOUND if the thread has no event queue.
 *  @retval VERR_CALLBACK_RETURN if the callback indicates return.
 *
 *  @todo This is just a quick approximation of what we need. Feel free to
 *        improve the interface and make it fit better in with the EventQueue
 *        class.
 */
/*static*/ int
EventQueue::processThreadEventQueue(uint32_t cMsTimeout, bool (*pfnExitCheck)(void *pvUser) /*= 0*/,
                                    void *pvUser /*= 0*/, uint32_t cMsPollInterval /*= 1000*/,
                                    bool fReturnOnEvent /*= true*/)
{
    uint64_t const StartMsTS = RTTimeMilliTS();

    /* set default. */
    if (cMsPollInterval == 0)
        cMsPollInterval = 1000;

    /*
     * Get the event queue / thread.
     */
#ifdef VBOX_WITH_XPCOM
    nsCOMPtr<nsIEventQueue> q;
    nsresult rv = NS_GetCurrentEventQ(getter_AddRefs(q));
    if (NS_FAILED(rv))
        return VERR_NOT_FOUND;
#else
    MyThreadHandle q(GetCurrentThread());
#endif

    /*
     * Check for pending before setting up the wait.
     */
    if (    !hasEventQueuePendingEvents(q)
        ||  !fReturnOnEvent)
    {
        bool fIsNative = isEventQueueNative(q);
        if (    fIsNative
            ||  cMsTimeout != RT_INDEFINITE_WAIT
            ||  pfnExitCheck
            ||  !fReturnOnEvent /** @todo !fReturnOnEvent and cMsTimeout RT_INDEFINITE_WAIT can be handled in else */)
        {
#ifdef USE_XPCOM_QUEUE
            int const fdQueue = fIsNative ? q->GetEventQueueSelectFD() : -1;
            if (fIsNative && fdQueue == -1)
                return VERR_INTERNAL_ERROR_4;
#endif
            for (;;)
            {
                /*
                 * Check for events.
                 */
                if (hasEventQueuePendingEvents(q))
                {
                    if (fReturnOnEvent)
                        break;
                    processPendingEvents(q);
                }

                /*
                 * Check the user exit.
                 */
                if (   pfnExitCheck
                    && pfnExitCheck(pvUser))
                    return VERR_CALLBACK_RETURN;

                /*
                 * Figure out how much we have left to wait and if we've timed out already.
                 */
                uint32_t cMsLeft;
                if (cMsTimeout == RT_INDEFINITE_WAIT)
                    cMsLeft = RT_INDEFINITE_WAIT;
                else
                {
                    uint64_t cMsElapsed = RTTimeMilliTS() - StartMsTS;
                    if (cMsElapsed >= cMsTimeout)
                        break; /* timeout */
                    cMsLeft = cMsTimeout - (uint32_t)cMsElapsed;
                }

                /*
                 * Wait in a queue & platform specific manner.
                 */
#ifdef VBOX_WITH_XPCOM
                if (!fIsNative)
                    RTThreadSleep(250 /*ms*/);
                else
                {
# ifdef USE_XPCOM_QUEUE
                    fd_set fdset;
                    FD_ZERO(&fdset);
                    FD_SET(fdQueue, &fdset);
                    struct timeval tv;
                    if (    cMsLeft == RT_INDEFINITE_WAIT
                        ||  cMsLeft >= cMsPollInterval)
                    {
                        tv.tv_sec = cMsPollInterval / 1000;
                        tv.tv_usec = (cMsPollInterval % 1000) * 1000;
                    }
                    else
                    {
                        tv.tv_sec = cMsLeft / 1000;
                        tv.tv_usec = (cMsLeft % 1000) * 1000;
                    }
                    int prc = select(fdQueue + 1, &fdset, NULL, NULL, &tv);
                    if (prc == -1)
                        return RTErrConvertFromErrno(errno);

# elif defined(RT_OS_DARWIN)
                    CFTimeInterval rdTimeout = (double)RT_MIN(cMsLeft, cMsPollInterval) / 1000;
                    OSStatus orc = CFRunLoopRunInMode(kCFRunLoopDefaultMode, rdTimeout, true /*returnAfterSourceHandled*/);
                    if (orc == kCFRunLoopRunHandledSource)
                        orc = CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.0, false /*returnAfterSourceHandled*/);
                    if (   orc != 0
                        && orc != kCFRunLoopRunHandledSource
                        && orc != kCFRunLoopRunTimedOut)
                        return orc == kCFRunLoopRunStopped || orc == kCFRunLoopRunFinished
                             ? VERR_INTERRUPTED
                             : RTErrConvertFromDarwin(orc);
# else
#  warning "PORTME:"
                    RTThreadSleep(250);
# endif
                }

#else  /* !VBOX_WITH_XPCOM */
                DWORD rc = MsgWaitForMultipleObjects(1, &q.mh, TRUE /*fWaitAll*/, RT_MIN(cMsLeft, cMsPollInterval), QS_ALLINPUT);
                if (rc == WAIT_OBJECT_0)
                {
                    if (fReturnOnEvent)
                        break;
                    processPendingEvents(q);
                }
                else if (rc == WAIT_FAILED)
                    return RTErrConvertFromWin32(GetLastError());
                else if (rc != WAIT_TIMEOUT)
                    return VERR_INTERNAL_ERROR_4;
#endif /* !VBOX_WITH_XPCOM */
            } /* for (;;) */
        }
        else
        {
            /*
             * Indefinite wait without any complications.
             */
#ifdef VBOX_WITH_XPCOM
            PLEvent *pEvent = NULL;
            rv = q->WaitForEvent(&pEvent);
            if (NS_FAILED(rv))
                return VERR_INTERRUPTED;
            q->HandleEvent(pEvent);
#else
            DWORD rc = MsgWaitForMultipleObjects(1, &q.mh, TRUE /*fWaitAll*/, INFINITE, QS_ALLINPUT);
            if (rc != WAIT_OBJECT_0)
            {
                if (rc == WAIT_FAILED)
                    return RTErrConvertFromWin32(GetLastError());
                return VERR_INTERNAL_ERROR_3;
            }
#endif
        }
    }

    /*
     * We have/had events in the queue. Process pending events and
     * return successfully.
     */
    processPendingEvents(q);

    return VINF_SUCCESS;
}

}
/* namespace com */

