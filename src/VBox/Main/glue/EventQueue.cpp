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

EventQueue* EventQueue::mMainQueue = NULL;

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

/* static */ int EventQueue::init()
{
    mMainQueue = new EventQueue();
#if defined (VBOX_WITH_XPCOM)
    nsCOMPtr<nsIEventQueue> q;
    nsresult rv = NS_GetMainEventQ(getter_AddRefs(q));
    Assert(NS_SUCCEEDED(rv));
    Assert(q == mMainQueue->mEventQ);
    PRBool fIsNative = PR_FALSE;
    rv = mMainQueue->mEventQ->IsQueueNative(&fIsNative);
    Assert(NS_SUCCEEDED(rv) && fIsNative);
#endif
    return VINF_SUCCESS;
}

/* static */ int EventQueue::deinit()
{
    delete mMainQueue;
    mMainQueue = NULL;
    return VINF_SUCCESS;
}

/* static */ EventQueue* EventQueue::getMainEventQueue()
{
    return mMainQueue;
}

#ifdef RT_OS_DARWIN
static int
timedWaitForEventsOnDarwin(nsIEventQueue *pQueue, PRInt32 cMsTimeout)
{
  OSStatus       orc       = -1;
  CFTimeInterval rdTimeout = (double)cMsTimeout / 1000;
  orc = CFRunLoopRunInMode(kCFRunLoopDefaultMode, rdTimeout, true /*returnAfterSourceHandled*/);
  if (orc == kCFRunLoopRunHandledSource)
    orc = CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.0, false /*returnAfterSourceHandled*/);
  if (!orc || orc == kCFRunLoopRunHandledSource)
    return VINF_SUCCESS;

  if (orc != kCFRunLoopRunTimedOut) 
  {
      NS_WARNING("Unexpected status code from CFRunLoopRunInMode");
  }

  return VERR_TIMEOUT;
}
#endif

#ifdef RT_OS_WINDOWS
static int
processPendingEvents()
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
/** For automatic cleanup,   */
class MyThreadHandle
{
public:
  HANDLE mh;
  
  MyThreadHandle()
  {
    if (!DuplicateHandle(GetCurrentProcess(), 
                         GetCurrentThread(), 
                         GetCurrentProcess(),
                         &mh, 
                         0 /*dwDesiredAccess*/, 
                         FALSE /*bInheritHandle*/,
                         DUPLICATE_SAME_ACCESS))
      mh = INVALID_HANDLE_VALUE;
  }

  ~MyThreadHandle()
  {
    CloseHandle(mh);
    mh = INVALID_HANDLE_VALUE;
  }
};
#else
static int
processPendingEvents(nsIEventQueue* pQueue)
{
  /** @todo: rethink interruption events, current NULL event approach is bad */
  pQueue->ProcessPendingEvents();
  return VINF_SUCCESS; 
}
#endif
int EventQueue::processEventQueue(uint32_t cMsTimeout)
{
    int rc = VINF_SUCCESS;
    /** @todo: check that current thread == one we were created on */
#if defined (VBOX_WITH_XPCOM)
    do {
      PRBool fHasEvents = PR_FALSE;
      nsresult rc;
      
      rc = mEventQ->PendingEvents(&fHasEvents);     
      if (NS_FAILED (rc))
          return VERR_INTERNAL_ERROR_3;

      if (fHasEvents || cMsTimeout == 0)
          break;

      /**
       * Unfortunately, WaitForEvent isn't interruptible with Ctrl-C,
       * while select() is.
       */

      if (cMsTimeout == RT_INDEFINITE_WAIT)
      {
#if 0
          PLEvent *pEvent = NULL;
          int rc1 = mEventQ->WaitForEvent(&pEvent);
          if (NS_FAILED(rc1) || pEvent == NULL)
          {
                rc = VERR_INTERRUPTED;
                break;
          }
          mEventQ->HandleEvent(pEvent);
          break;
#else
          /* Pretty close to forever */
          cMsTimeout = 0xffff0000;
#endif
      }
      
      /* Bit tricky part - perform timed wait */
#  ifdef RT_OS_DARWIN
      rc = timedWaitForEventsOnDarwin(mEventQ, cMsTimeout);
#  else
      int fd = mEventQ->GetEventQueueSelectFD();
      fd_set fdsetR, fdsetE;
      struct timeval tv;
      
      FD_ZERO(&fdsetR);
      FD_SET(fd, &fdsetR);
      
      fdsetE = fdsetR;
      tv.tv_sec = (PRInt64)cMsTimeout / 1000;
      tv.tv_usec = ((PRInt64)cMsTimeout % 1000) * 1000;

      int aCode = select(fd + 1, &fdsetR, NULL, &fdsetE, &tv);
      if (aCode == 0)
        rc = VERR_TIMEOUT;
      else if (aCode == EINTR)
        rc = VERR_INTERRUPTED;
      else if (aCode < 0)
        rc = VERR_INTERNAL_ERROR_4;
      
#  endif      
    } while (0);

    rc = processPendingEvents(mEventQ);
#else /* Windows */
    do {
        int aCode = processPendingEventsOnWindows();
        if (aCode != VERR_TIMEOUT || cMsTimeout == 0)
        {
            rc = aCode;
            break;
        }
        
        if (cMsTimeout == RT_INDEFINITE_WAIT)
        {
            Event* aEvent = NULL;
          
            BOOL fHasEvent = waitForEvent(&aEvent);
            if (fHasEvent)
              handleEvent(aEvent);
            else
              rc = VERR_INTERRUPTED;
            break;
        }

        /* Perform timed wait */
        MyThreadHandle aHandle;

        DWORD aCode2 = MsgWaitForMultipleObjects(1, &aHandle.mh, 
                                                 TRUE /*fWaitAll*/, 
                                                 0 /*ms*/, 
                                                 QS_ALLINPUT);
        if (aCode2 == WAIT_TIMEOUT)
            rc = VERR_TIMEOUT;
        else if (aCode2 == WAIT_OBJECT_0)
            rc = VINF_SUCCESS;
        else
            rc = VERR_INTERNAL_ERROR_4;
    } while (0);

    rc = processPendingEvents();
#endif
    return rc;
}

int EventQueue::interruptEventQueueProcessing()
{
    /** @todo: rethink me! */
    postEvent(NULL);
    return VINF_SUCCESS;
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

int  EventQueue::getSelectFD()
{
#ifdef VBOX_WITH_XPCOM
    return mEventQ->GetEventQueueSelectFD();
#else
    return -1;
#endif
}
}
/* namespace com */

