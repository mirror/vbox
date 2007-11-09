/** @file
 * MS COM / XPCOM Abstraction Layer:
 * Event and EventQueue class declaration
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBox_com_EventQueue_h
#define ___VBox_com_EventQueue_h

#if defined (RT_OS_WINDOWS)
#include <windows.h>
#else
#include <nsEventQueueUtils.h>
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
 *  On Linux, if this queue is created on the main thread, it automatically
 *  processes XPCOM/IPC events while waiting for its own (Event) events.
 */
class EventQueue
{
public:

    EventQueue();
    ~EventQueue();

    BOOL postEvent (Event *event);
    BOOL waitForEvent (Event **event);
    BOOL handleEvent (Event *event);

private:

#if defined (RT_OS_WINDOWS)

    DWORD mThreadId;

#else

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

