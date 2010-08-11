/* $Id$ */

/** @file
 *
 * MS COM / XPCOM Abstraction Layer:
 * Event and EventQueue class declaration
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
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

#ifndef VBOX_WITH_XPCOM

#define CHECK_THREAD_RET(ret) \
    do { \
        AssertMsg(GetCurrentThreadId() == mThreadId, ("Must be on event queue thread!")); \
        if (GetCurrentThreadId() != mThreadId) \
            return ret; \
    } while (0)

#else // VBOX_WITH_XPCOM

#define CHECK_THREAD_RET(ret) \
    do { \
        if (!mEventQ) \
            return ret; \
        BOOL isOnCurrentThread = FALSE; \
        mEventQ->IsOnCurrentThread(&isOnCurrentThread); \
        AssertMsg(isOnCurrentThread, ("Must be on event queue thread!")); \
        if (!isOnCurrentThread) \
            return ret; \
    } while (0)

#endif // VBOX_WITH_XPCOM

EventQueue *EventQueue::mMainQueue = NULL;

#ifdef VBOX_WITH_XPCOM
struct MyPLEvent : public PLEvent
{
    MyPLEvent(Event *e) : event(e) {}
    Event *event;
};

/* static */
void *PR_CALLBACK com::EventQueue::plEventHandler(PLEvent *self)
{
    Event *ev = ((MyPLEvent *)self)->event;
    if (ev)
        ev->handler();
    else
    {
        EventQueue *eq = (EventQueue *)self->owner;
        Assert(eq);
        eq->mInterrupted = true;
    }
    return NULL;
}

/* static */
void PR_CALLBACK com::EventQueue::plEventDestructor(PLEvent *self)
{
    Event *ev = ((MyPLEvent *)self)->event;
    if (ev)
        delete ev;
    delete self;
}

#endif // VBOX_WITH_XPCOM

/**
 *  Constructs an event queue for the current thread.
 *
 *  Currently, there can be only one event queue per thread, so if an event
 *  queue for the current thread already exists, this object is simply attached
 *  to the existing event queue.
 */
EventQueue::EventQueue()
{
#ifndef VBOX_WITH_XPCOM

    mThreadId = GetCurrentThreadId();
    // force the system to create the message queue for the current thread
    MSG msg;
    PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

    if (!DuplicateHandle(GetCurrentProcess(),
                         GetCurrentThread(),
                         GetCurrentProcess(),
                         &mhThread,
                         0 /*dwDesiredAccess*/,
                         FALSE /*bInheritHandle*/,
                         DUPLICATE_SAME_ACCESS))
      mhThread = INVALID_HANDLE_VALUE;

#else // VBOX_WITH_XPCOM

    mEQCreated = FALSE;
    mInterrupted = FALSE;

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

    nsresult rc = NS_GetEventQueueService(getter_AddRefs(mEventQService));

    if (NS_SUCCEEDED(rc))
    {
        rc = mEventQService->GetThreadEventQueue(NS_CURRENT_THREAD,
                                                 getter_AddRefs(mEventQ));
        if (rc == NS_ERROR_NOT_AVAILABLE)
        {
            rc = mEventQService->CreateThreadEventQueue();
            if (NS_SUCCEEDED(rc))
            {
                mEQCreated = TRUE;
                rc = mEventQService->GetThreadEventQueue(NS_CURRENT_THREAD,
                                                         getter_AddRefs(mEventQ));
            }
        }
    }
    AssertComRC(rc);

#endif // VBOX_WITH_XPCOM
}

EventQueue::~EventQueue()
{
#ifndef VBOX_WITH_XPCOM
    if (mhThread != INVALID_HANDLE_VALUE)
    {
        CloseHandle(mhThread);
        mhThread = INVALID_HANDLE_VALUE;
    }
#else // VBOX_WITH_XPCOM
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
#endif // VBOX_WITH_XPCOM
}

/**
 *  Initializes the main event queue instance.
 *  @returns VBox status code.
 *
 *  @remarks If you're using the rest of the COM/XPCOM glue library,
 *           com::Initialize() will take care of initializing and uninitializing
 *           the EventQueue class.  If you don't call com::Initialize, you must
 *           make sure to call this method on the same thread that did the
 *           XPCOM initialization or we'll end up using the wrong main queue.
 */
/* static */
int EventQueue::init()
{
    Assert(mMainQueue == NULL);
    Assert(RTThreadIsMain(RTThreadSelf()));
    mMainQueue = new EventQueue();

#ifdef VBOX_WITH_XPCOM
    /* Check that it actually is the main event queue, i.e. that
       we're called on the right thread. */
    nsCOMPtr<nsIEventQueue> q;
    nsresult rv = NS_GetMainEventQ(getter_AddRefs(q));
    Assert(NS_SUCCEEDED(rv));
    Assert(q == mMainQueue->mEventQ);

    /* Check that it's a native queue. */
    PRBool fIsNative = PR_FALSE;
    rv = mMainQueue->mEventQ->IsQueueNative(&fIsNative);
    Assert(NS_SUCCEEDED(rv) && fIsNative);
#endif // VBOX_WITH_XPCOM

    return VINF_SUCCESS;
}

/**
 *  Uninitialize the global resources (i.e. the main event queue instance).
 *  @returns VINF_SUCCESS
 */
/* static */
int EventQueue::uninit()
{
    Assert(mMainQueue);
    /* Must process all events to make sure that no NULL event is left
     * after this point. It would need to modify the state of mMainQueue. */
    mMainQueue->processEventQueue(0);
    delete mMainQueue;
    mMainQueue = NULL;
    return VINF_SUCCESS;
}

/**
 *  Get main event queue instance.
 *
 *  Depends on init() being called first.
 */
/* static */
EventQueue* EventQueue::getMainEventQueue()
{
    return mMainQueue;
}

#ifdef VBOX_WITH_XPCOM
# ifdef RT_OS_DARWIN
/**
 *  Wait for events and process them (Darwin).
 *
 *  @returns VINF_SUCCESS or VERR_TIMEOUT.
 *
 *  @param  cMsTimeout      How long to wait, or RT_INDEFINITE_WAIT.
 */
static int
waitForEventsOnDarwin(unsigned cMsTimeout)
{
    /*
     * Wait for the requested time, if we get a hit we do a poll to process
     * any other pending messages.
     *
     * Note! About 1.0e10: According to the sources anything above 3.1556952e+9
     *       means indefinite wait and 1.0e10 is what CFRunLoopRun() uses.
     */
    CFTimeInterval rdTimeout = cMsTimeout == RT_INDEFINITE_WAIT ? 1e10 : (double)cMsTimeout / 1000;
    OSStatus orc = CFRunLoopRunInMode(kCFRunLoopDefaultMode, rdTimeout, true /*returnAfterSourceHandled*/);
    /** @todo Not entire sure if the poll actually processes more than one message.
     *        Feel free to check the sources anyone.  */
    if (orc == kCFRunLoopRunHandledSource)
        orc = CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.0, false /*returnAfterSourceHandled*/);
    if (    orc == 0
        ||  orc == kCFRunLoopRunHandledSource)
        return VINF_SUCCESS;
    if (    orc == kCFRunLoopRunStopped
        ||  orc == kCFRunLoopRunFinished)
        return VERR_INTERRUPTED;
    AssertMsg(orc == kCFRunLoopRunTimedOut, ("Unexpected status code from CFRunLoopRunInMode: %#x", orc));
    return VERR_TIMEOUT;
}
# else // !RT_OS_DARWIN

/**
 *  Wait for events (generic XPCOM).
 *
 *  @returns VINF_SUCCESS or VERR_TIMEOUT.
 *
 *  @param  pQueue          The queue to wait on.
 *  @param  cMsTimeout      How long to wait, or RT_INDEFINITE_WAIT.
 */
static
int waitForEventsOnXPCOM(nsIEventQueue *pQueue, unsigned cMsTimeout)
{
    int     fd = pQueue->GetEventQueueSelectFD();
    fd_set  fdsetR;
    FD_ZERO(&fdsetR);
    FD_SET(fd, &fdsetR);

    fd_set  fdsetE = fdsetR;

    struct timeval  tv = {0,0};
    struct timeval *ptv;
    if (cMsTimeout == RT_INDEFINITE_WAIT)
        ptv = NULL;
    else
    {
        tv.tv_sec  = cMsTimeout / 1000;
        tv.tv_usec = (cMsTimeout % 1000) * 1000;
        ptv = &tv;
    }

    int rc = select(fd + 1, &fdsetR, NULL, &fdsetE, ptv);
    if (rc > 0)
        rc = VINF_SUCCESS;
    else if (rc == 0)
        rc = VERR_TIMEOUT;
    else if (errno == EINTR)
        rc = VERR_INTERRUPTED;
    else
    {
        AssertMsgFailed(("rc=%d errno=%d\n", rc, errno));
        rc = VERR_INTERNAL_ERROR_4;
    }
    return rc;
}

# endif // !RT_OS_DARWIN
#endif // VBOX_WITH_XPCOM

#ifndef VBOX_WITH_XPCOM
/**
 *  Process pending events (Windows).
 *  @returns VINF_SUCCESS, VERR_TIMEOUT or VERR_INTERRUPTED.
 */
static int
processPendingEvents(void)
{
    MSG Msg;
    int rc = VERR_TIMEOUT;
    while (PeekMessage(&Msg, NULL /*hWnd*/, 0 /*wMsgFilterMin*/, 0 /*wMsgFilterMax*/, PM_REMOVE))
    {
        if (Msg.message == WM_QUIT)
            rc = VERR_INTERRUPTED;
        DispatchMessage(&Msg);
        if (rc == VERR_INTERRUPTED)
            break;
        rc = VINF_SUCCESS;
    }
    return rc;
}
#else // VBOX_WITH_XPCOM
/**
 * Process pending XPCOM events.
 * @param pQueue The queue to process events on.
 * @returns VINF_SUCCESS or VERR_TIMEOUT.
 */
static
int processPendingEvents(nsIEventQueue *pQueue)
{
    /* Check for timeout condition so the caller can be a bit more lazy. */
    PRBool fHasEvents = PR_FALSE;
    nsresult hr = pQueue->PendingEvents(&fHasEvents);
    if (NS_FAILED(hr))
        return VERR_INTERNAL_ERROR_2;
    if (!fHasEvents)
        return VERR_TIMEOUT;

    pQueue->ProcessPendingEvents();
    return VINF_SUCCESS;
}
#endif // VBOX_WITH_XPCOM


/**
 *  Process events pending on this event queue, and wait up to given timeout, if
 *  nothing is available.
 *
 *  Must be called on same thread this event queue was created on.
 *
 *  @param cMsTimeout The timeout specified as milliseconds.  Use
 *                    RT_INDEFINITE_WAIT to wait till an event is posted on the
 *                    queue.
 *
 *  @returns VBox status code
 *  @retval VINF_SUCCESS
 *  @retval VERR_TIMEOUT
 *  @retval VERR_INVALID_CONTEXT
 */
int EventQueue::processEventQueue(uint32_t cMsTimeout)
{
    int rc;
    CHECK_THREAD_RET(VERR_INVALID_CONTEXT);

#ifdef VBOX_WITH_XPCOM
    /*
     * Process pending events, if none are available and we're not in a
     * poll call, wait for some to appear.  (We have to be a little bit
     * careful after waiting for the events since Darwin will process
     * them as part of the wait, while the XPCOM case will not.)
     *
     * Note! Unfortunately, WaitForEvent isn't interruptible with Ctrl-C,
     *       while select() is.  So we cannot use it for indefinite waits.
     */
    rc = processPendingEvents(mEventQ);
    if (    rc == VERR_TIMEOUT
        &&  cMsTimeout > 0)
    {
# ifdef RT_OS_DARWIN
        /** @todo check how Ctrl-C works on Darwin. */
        rc = waitForEventsOnDarwin(cMsTimeout);
        if (rc == VERR_TIMEOUT)
            rc = processPendingEvents(mEventQ);
# else // !RT_OS_DARWIN
        rc = waitForEventsOnXPCOM(mEventQ, cMsTimeout);
        if (    RT_SUCCESS(rc)
            ||  rc == VERR_TIMEOUT)
            rc = processPendingEvents(mEventQ);
# endif // !RT_OS_DARWIN
    }
    if (RT_SUCCESS(rc) && mInterrupted)
    {
        mInterrupted = false;
        rc = VERR_INTERRUPTED;
    }

#else // !VBOX_WITH_XPCOM
    if (cMsTimeout == RT_INDEFINITE_WAIT)
    {
        BOOL bRet;
        MSG Msg;
        int rc = VINF_SUCCESS;
        while ((bRet = GetMessage(&Msg, NULL /*hWnd*/, WM_USER, WM_USER)))
        {
            if (bRet != -1)
                DispatchMessage(&Msg);
        }
        if (bRet == 0)
            rc = VERR_INTERRUPTED;
    }
    else
    {
        rc = processPendingEvents();
        if (   rc == VERR_TIMEOUT
            || cMsTimeout == 0)
        {
            DWORD rcW = MsgWaitForMultipleObjects(1,
                                                  &mhThread,
                                                  TRUE /*fWaitAll*/,
                                                  cMsTimeout,
                                                  QS_ALLINPUT);
            AssertMsgReturn(rcW == WAIT_TIMEOUT || rcW == WAIT_OBJECT_0,
                            ("%d\n", rcW),
                            VERR_INTERNAL_ERROR_4);
            rc = processPendingEvents();
        }
    }
#endif // !VBOX_WITH_XPCOM
    return rc;
}

/**
 *  Interrupt thread waiting on event queue processing.
 *
 *  Can be called on any thread.
 */
int EventQueue::interruptEventQueueProcessing()
{
    /* Send a NULL event. This gets us out of the event loop on XPCOM, and
     * doesn't hurt on Windows. It is the responsibility of the caller to
     * take care of not running the loop again in a way which will hang. */
    postEvent(NULL);
    return VINF_SUCCESS;
}

/**
 *  Posts an event to this event loop asynchronously.
 *
 *  @param  event   the event to post, must be allocated using |new|
 *  @return         TRUE if successful and false otherwise
 */
BOOL EventQueue::postEvent(Event *event)
{
#ifndef VBOX_WITH_XPCOM

    return PostThreadMessage(mThreadId, WM_USER, (WPARAM)event, NULL);

#else // VBOX_WITH_XPCOM

    if (!mEventQ)
        return FALSE;

    MyPLEvent *ev = new MyPLEvent(event);
    mEventQ->InitEvent(ev, this, com::EventQueue::plEventHandler,
                       com::EventQueue::plEventDestructor);
    HRESULT rc = mEventQ->PostEvent(ev);
    return NS_SUCCEEDED(rc);

#endif // VBOX_WITH_XPCOM
}


/**
 *  Get select()'able selector for this event queue.
 *  This will return -1 on platforms and queue variants not supporting such
 *  functionality.
 */
int EventQueue::getSelectFD()
{
#ifdef VBOX_WITH_XPCOM
    return mEventQ->GetEventQueueSelectFD();
#else
    return -1;
#endif
}

}
/* namespace com */
