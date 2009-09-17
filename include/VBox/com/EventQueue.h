/** @file
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___VBox_com_EventQueue_h
#define ___VBox_com_EventQueue_h

#if !defined (VBOX_WITH_XPCOM)
# include <Windows.h>
#else
# include <nsEventQueueUtils.h>
#endif

#include <VBox/com/defs.h>
#include <VBox/com/assert.h>

namespace com
{

class EventQueue;

/**
 *  Base class for all events. Intended to be subclassed to introduce new events
 *  and handlers for them.
 *
 *  Subclasses usually reimplement virtual #handler() (that does nothing by
 *  default) and add new data members describing the event.
 */
class Event
{
public:

    Event() {}

protected:

    virtual ~Event() {};

    /**
     *  Event handler. Called in the context of the event queue's thread.
     *  Always reimplemented by subclasses
     *
     *  @return reserved, should be NULL.
     */
    virtual void *handler() { return NULL; }

    friend class EventQueue;
};

/**
 *  Simple event queue.
 *
 *  When using XPCOM, this will map onto the default XPCOM queue for the thread.
 *  So, if a queue is created on the main thread, it automatically processes
 *  XPCOM/IPC events while waiting for its own (Event) events.
 *
 *  When using Windows, Darwin and OS/2, this will map onto the native thread
 *  queue/runloop.  So, windows messages and want not will be processed while
 *  waiting for events.
 */
class EventQueue
{
public:

    EventQueue();
    ~EventQueue();

    BOOL postEvent (Event *event);
    BOOL waitForEvent (Event **event);
    BOOL handleEvent (Event *event);
    int processEventQueue(uint32_t cMsTimeout);
    int interruptEventQueueProcessing();
    int getSelectFD();
    static int init();
    static int uninit();
    static EventQueue *getMainEventQueue();

private:
    static EventQueue *mMainQueue;

#if !defined (VBOX_WITH_XPCOM)

    /** The thread which the queue belongs to. */
    DWORD mThreadId;
    /** Duplicated thread handle for MsgWaitForMultipleObjects. */
    HANDLE mhThread;

#else

    /** Whether it was created (and thus needs destroying) or if a queue already
     *  associated with the thread was used. */
    BOOL mEQCreated;

    nsCOMPtr <nsIEventQueue> mEventQ;
    nsCOMPtr <nsIEventQueueService> mEventQService;

    Event *mLastEvent;
    BOOL mGotEvent;

    struct MyPLEvent : public PLEvent
    {
        MyPLEvent (Event *e) : event (e) {}
        Event *event;
    };

    static void * PR_CALLBACK plEventHandler (PLEvent* self)
    {
        // nsIEventQueue doesn't expose PL_GetEventOwner(), so use an internal
        // field of PLEvent directly (hackish, but doesn' require an extra lib)
        EventQueue *owner = (EventQueue *) self->owner;
        Assert (owner);
        owner->mLastEvent = ((MyPLEvent *) self)->event;
        owner->mGotEvent = TRUE;
        return 0;
    }

    static void PR_CALLBACK plEventDestructor (PLEvent* self) { delete self; }

#endif
};

} /* namespace com */

#endif

