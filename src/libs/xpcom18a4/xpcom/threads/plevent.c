/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nspr.h"
#include "plevent.h"

#include <errno.h>
#include <stddef.h>
#include <unistd.h>
/* for fcntl */
#include <sys/types.h>
#include <fcntl.h>

#if defined(XP_MACOSX)
# include <CoreFoundation/CoreFoundation.h>
#endif

#include "private/pprthred.h"

#include <iprt/errcore.h>

static PRLogModuleInfo *event_lm = NULL;

/*******************************************************************************
 * Private Stuff
 ******************************************************************************/

/*
** EventQueueType -- Defines notification type for an event queue
**
*/
typedef enum {
    EventQueueIsNative = 1,
    EventQueueIsMonitored = 2
} EventQueueType;


struct PLEventQueue {
    const char*         name;
    PRCList             queue;
    PRMonitor*          monitor;
    PRThread*           handlerThread;
    EventQueueType      type;
    PRPackedBool        processingEvents;
    PRPackedBool        notified;

#if defined(XP_UNIX) && !defined(XP_MACOSX)
    PRInt32             eventPipe[2];
    PLGetEventIDFunc    idFunc;
    void*               idFuncClosure;
#elif defined(XP_MACOSX)
    CFRunLoopSourceRef  mRunLoopSource;
    CFRunLoopRef        mMainRunLoop;
    CFStringRef         mRunLoopModeStr;                                                                /* vbox */
#endif
};

#define PR_EVENT_PTR(_qp) \
    ((PLEvent*) ((char*) (_qp) - offsetof(PLEvent, link)))

static PRStatus    _pl_SetupNativeNotifier(PLEventQueue* self);
static void        _pl_CleanupNativeNotifier(PLEventQueue* self);
static PRStatus    _pl_NativeNotify(PLEventQueue* self);
static PRStatus    _pl_AcknowledgeNativeNotify(PLEventQueue* self);
static void        _md_CreateEventQueue( PLEventQueue *eventQueue );
static PRInt32     _pl_GetEventCount(PLEventQueue* self);


/*******************************************************************************
 * Event Queue Operations
 ******************************************************************************/

/*
** _pl_CreateEventQueue() -- Create the event queue
**
**
*/
static PLEventQueue * _pl_CreateEventQueue(const char *name,
                                           PRThread *handlerThread,
                                           EventQueueType  qtype)
{
    PRStatus err;
    PLEventQueue* self = NULL;
    PRMonitor* mon = NULL;

    if (event_lm == NULL)
        event_lm = PR_NewLogModule("event");

    self = PR_NEWZAP(PLEventQueue);
    if (self == NULL) return NULL;

    mon = PR_NewNamedMonitor(name);
    if (mon == NULL) goto error;

    self->name = name;
    self->monitor = mon;
    self->handlerThread = handlerThread;
    self->processingEvents = PR_FALSE;
    self->type = qtype;
    self->notified = PR_FALSE;

    PR_INIT_CLIST(&self->queue);
    if ( qtype == EventQueueIsNative ) {
        err = _pl_SetupNativeNotifier(self);
        if (err) goto error;
        _md_CreateEventQueue( self );
    }
    return self;

  error:
    if (mon != NULL)
        PR_DestroyMonitor(mon);
    PR_DELETE(self);
    return NULL;
}

PR_IMPLEMENT(PLEventQueue*)
PL_CreateEventQueue(const char* name, PRThread* handlerThread)
{
    return( _pl_CreateEventQueue( name, handlerThread, EventQueueIsNative ));
}

PR_EXTERN(PLEventQueue *)
PL_CreateNativeEventQueue(const char *name, PRThread *handlerThread)
{
    return( _pl_CreateEventQueue( name, handlerThread, EventQueueIsNative ));
}

PR_EXTERN(PLEventQueue *)
PL_CreateMonitoredEventQueue(const char *name, PRThread *handlerThread)
{
    return( _pl_CreateEventQueue( name, handlerThread, EventQueueIsMonitored ));
}

PR_IMPLEMENT(PRMonitor*)
PL_GetEventQueueMonitor(PLEventQueue* self)
{
    return self->monitor;
}

static void PR_CALLBACK
_pl_destroyEvent(PLEvent* event, void* data, PLEventQueue* queue)
{
    PL_DequeueEvent(event, queue);
    PL_DestroyEvent(event);
}

PR_IMPLEMENT(void)
PL_DestroyEventQueue(PLEventQueue* self)
{
    PR_EnterMonitor(self->monitor);

    /* destroy undelivered events */
    PL_MapEvents(self, _pl_destroyEvent, NULL);

    if ( self->type == EventQueueIsNative )
        _pl_CleanupNativeNotifier(self);

    /* destroying the monitor also destroys the name */
    PR_ExitMonitor(self->monitor);
    PR_DestroyMonitor(self->monitor);
    PR_DELETE(self);

}

PR_IMPLEMENT(PRStatus)
PL_PostEvent(PLEventQueue* self, PLEvent* event)
{
    PRStatus err = PR_SUCCESS;
    PRMonitor* mon;

    if (self == NULL)
        return PR_FAILURE;

    mon = self->monitor;
    PR_EnterMonitor(mon);

#if defined(XP_UNIX) && !defined(XP_MACOSX)
    if (self->idFunc && event)
        event->id = self->idFunc(self->idFuncClosure);
#endif

    /* insert event into thread's event queue: */
    if (event != NULL) {
        PR_APPEND_LINK(&event->link, &self->queue);
    }

    if (self->type == EventQueueIsNative && !self->notified) {
        err = _pl_NativeNotify(self);

        if (err != PR_SUCCESS)
            goto error;

        self->notified = PR_TRUE;
    }

    /*
     * This may fall on deaf ears if we're really notifying the native
     * thread, and no one has called PL_WaitForEvent (or PL_EventLoop):
     */
    err = PR_Notify(mon);

error:
    PR_ExitMonitor(mon);
    return err;
}

PR_IMPLEMENT(void*)
PL_PostSynchronousEvent(PLEventQueue* self, PLEvent* event)
{
    void* result;

    if (self == NULL)
        return NULL;

    PR_ASSERT(event != NULL);

    if (PR_GetCurrentThread() == self->handlerThread) {
        /* Handle the case where the thread requesting the event handling
         * is also the thread that's supposed to do the handling. */
        result = event->handler(event);
    }
    else {
        int i, entryCount;

        int vrc = RTCritSectInit(&event->lock);
        if (RT_FAILURE(vrc)) {
          return NULL;
        }

        vrc = RTSemEventCreate(&event->condVar);
        if(RT_FAILURE(vrc))
        {
          RTCritSectDelete(&event->lock);
          return NULL;
        }

        RTCritSectEnter(&event->lock);

        entryCount = PR_GetMonitorEntryCount(self->monitor);

        event->synchronousResult = (void*)PR_TRUE;

        PL_PostEvent(self, event);

        /* We need temporarily to give up our event queue monitor if
           we're holding it, otherwise, the thread we're going to wait
           for notification from won't be able to enter it to process
           the event. */
        if (entryCount) {
            for (i = 0; i < entryCount; i++)
                PR_ExitMonitor(self->monitor);
        }

        event->handled = PR_FALSE;

        while (!event->handled) {
            /* wait for event to be handled or destroyed */
            RTCritSectLeave(&event->lock);
            RTSemEventWait(event->condVar, RT_INDEFINITE_WAIT);
            RTCritSectEnter(&event->lock);
        }

        if (entryCount) {
            for (i = 0; i < entryCount; i++)
                PR_EnterMonitor(self->monitor);
        }

        result = event->synchronousResult;
        event->synchronousResult = NULL;
        RTCritSectLeave(&event->lock);
    }

    /* For synchronous events, they're destroyed here on the caller's
       thread before the result is returned. See PL_HandleEvent. */
    PL_DestroyEvent(event);

    return result;
}

PR_IMPLEMENT(PLEvent*)
PL_GetEvent(PLEventQueue* self)
{
    PLEvent* event = NULL;
    PRStatus err = PR_SUCCESS;

    if (self == NULL)
        return NULL;

    PR_EnterMonitor(self->monitor);

    if (!PR_CLIST_IS_EMPTY(&self->queue)) {
        if ( self->type == EventQueueIsNative &&
             self->notified                   &&
             !self->processingEvents          &&
             0 == _pl_GetEventCount(self)     )
        {
            err = _pl_AcknowledgeNativeNotify(self);
            self->notified = PR_FALSE;
        }
        if (err)
            goto done;

        /* then grab the event and return it: */
        event = PR_EVENT_PTR(self->queue.next);
        PR_REMOVE_AND_INIT_LINK(&event->link);
    }

  done:
    PR_ExitMonitor(self->monitor);
    return event;
}

PR_IMPLEMENT(PRBool)
PL_EventAvailable(PLEventQueue* self)
{
    PRBool result = PR_FALSE;

    if (self == NULL)
        return PR_FALSE;

    PR_EnterMonitor(self->monitor);

    if (!PR_CLIST_IS_EMPTY(&self->queue))
        result = PR_TRUE;

    PR_ExitMonitor(self->monitor);
    return result;
}

PR_IMPLEMENT(void)
PL_MapEvents(PLEventQueue* self, PLEventFunProc fun, void* data)
{
    PRCList* qp;

    if (self == NULL)
        return;

    PR_EnterMonitor(self->monitor);
    qp = self->queue.next;
    while (qp != &self->queue) {
        PLEvent* event = PR_EVENT_PTR(qp);
        qp = qp->next;
        (*fun)(event, data, self);
    }
    PR_ExitMonitor(self->monitor);
}

static void PR_CALLBACK
_pl_DestroyEventForOwner(PLEvent* event, void* owner, PLEventQueue* queue)
{
    PR_ASSERT(PR_GetMonitorEntryCount(queue->monitor) > 0);
    if (event->owner == owner) {
        PR_LOG(event_lm, PR_LOG_DEBUG,
               ("$$$ \tdestroying event %0x for owner %0x", event, owner));
        PL_DequeueEvent(event, queue);

        if (event->synchronousResult == (void*)PR_TRUE) {
            RTCritSectEnter(&event->lock);
            event->synchronousResult = NULL;
            event->handled = PR_TRUE;
            RTSemEventSignal(event->condVar);
            RTCritSectLeave(&event->lock);
        }
        else {
            PL_DestroyEvent(event);
        }
    }
    else {
        PR_LOG(event_lm, PR_LOG_DEBUG,
               ("$$$ \tskipping event %0x for owner %0x", event, owner));
    }
}

PR_IMPLEMENT(void)
PL_RevokeEvents(PLEventQueue* self, void* owner)
{
    if (self == NULL)
        return;

    PR_LOG(event_lm, PR_LOG_DEBUG,
         ("$$$ revoking events for owner %0x", owner));

    /*
    ** First we enter the monitor so that no one else can post any events
    ** to the queue:
    */
    PR_EnterMonitor(self->monitor);
    PR_LOG(event_lm, PR_LOG_DEBUG, ("$$$ owner %0x, entered monitor", owner));

    /*
    ** Discard any pending events for this owner:
    */
    PL_MapEvents(self, _pl_DestroyEventForOwner, owner);

#ifdef DEBUG
    {
        PRCList* qp = self->queue.next;
        while (qp != &self->queue) {
            PLEvent* event = PR_EVENT_PTR(qp);
            qp = qp->next;
            PR_ASSERT(event->owner != owner);
        }
    }
#endif /* DEBUG */

    PR_ExitMonitor(self->monitor);

    PR_LOG(event_lm, PR_LOG_DEBUG,
           ("$$$ revoking events for owner %0x", owner));
}

static PRInt32
_pl_GetEventCount(PLEventQueue* self)
{
    PRCList* node;
    PRInt32  count = 0;

    PR_EnterMonitor(self->monitor);
    node = PR_LIST_HEAD(&self->queue);
    while (node != &self->queue) {
        count++;
        node = PR_NEXT_LINK(node);
    }
    PR_ExitMonitor(self->monitor);

    return count;
}

PR_IMPLEMENT(void)
PL_ProcessPendingEvents(PLEventQueue* self)
{
    PRInt32 count;

    if (self == NULL)
        return;


    PR_EnterMonitor(self->monitor);

    if (self->processingEvents) {
        _pl_AcknowledgeNativeNotify(self);
        self->notified = PR_FALSE;
        PR_ExitMonitor(self->monitor);
        return;
    }
    self->processingEvents = PR_TRUE;

    /* Only process the events that are already in the queue, and
     * not any new events that get added. Do this by counting the
     * number of events currently in the queue
     */
    count = _pl_GetEventCount(self);
    PR_ExitMonitor(self->monitor);

    while (count-- > 0) {
        PLEvent* event = PL_GetEvent(self);
        if (event == NULL)
            break;

        PR_LOG(event_lm, PR_LOG_DEBUG, ("$$$ processing event"));
        PL_HandleEvent(event);
        PR_LOG(event_lm, PR_LOG_DEBUG, ("$$$ done processing event"));
    }

    PR_EnterMonitor(self->monitor);

    if (self->type == EventQueueIsNative) {
        count = _pl_GetEventCount(self);

        if (count <= 0) {
            _pl_AcknowledgeNativeNotify(self);
            self->notified = PR_FALSE;
        }
        else {
            _pl_NativeNotify(self);
            self->notified = PR_TRUE;
        }

    }
    self->processingEvents = PR_FALSE;

    PR_ExitMonitor(self->monitor);
}

/*******************************************************************************
 * Event Operations
 ******************************************************************************/

PR_IMPLEMENT(void)
PL_InitEvent(PLEvent* self, void* owner,
             PLHandleEventProc handler,
             PLDestroyEventProc destructor)
{
    PR_INIT_CLIST(&self->link);
    self->handler = handler;
    self->destructor = destructor;
    self->owner = owner;
    self->synchronousResult = NULL;
    self->handled = PR_FALSE;
    self->condVar = NIL_RTSEMEVENT;
#if defined(XP_UNIX) && !defined(XP_MACOSX)
    self->id = 0;
#endif
}

PR_IMPLEMENT(void*)
PL_GetEventOwner(PLEvent* self)
{
    return self->owner;
}

PR_IMPLEMENT(void)
PL_HandleEvent(PLEvent* self)
{
    void* result;
    if (self == NULL)
        return;

    /* This event better not be on an event queue anymore. */
    PR_ASSERT(PR_CLIST_IS_EMPTY(&self->link));

    result = self->handler(self);
    if (NULL != self->synchronousResult) {
        RTCritSectEnter(&self->lock);
        self->synchronousResult = result;
        self->handled = PR_TRUE;
        RTSemEventSignal(self->condVar);
        RTCritSectLeave(&self->lock);
    }
    else {
        /* For asynchronous events, they're destroyed by the event-handler
           thread. See PR_PostSynchronousEvent. */
        PL_DestroyEvent(self);
    }
}

PR_IMPLEMENT(void)
PL_DestroyEvent(PLEvent* self)
{
    if (self == NULL)
        return;

    /* This event better not be on an event queue anymore. */
    PR_ASSERT(PR_CLIST_IS_EMPTY(&self->link));

    if(self->condVar != NIL_RTSEMEVENT)
      RTSemEventDestroy(self->condVar);
    if(RTCritSectIsInitialized(&self->lock))
      RTCritSectDelete(&self->lock);

    self->destructor(self);
}

PR_IMPLEMENT(void)
PL_DequeueEvent(PLEvent* self, PLEventQueue* queue)
{
    if (self == NULL)
        return;

    /* Only the owner is allowed to dequeue events because once the
       client has put it in the queue, they have no idea whether it's
       been processed and destroyed or not. */

    PR_ASSERT(queue->handlerThread == PR_GetCurrentThread());

    PR_EnterMonitor(queue->monitor);

    PR_ASSERT(!PR_CLIST_IS_EMPTY(&self->link));

#if 0
    /*  I do not think that we need to do this anymore.
        if we do not acknowledge and this is the only
        only event in the queue, any calls to process
        the eventQ will be effective noop.
    */
    if (queue->type == EventQueueIsNative)
      _pl_AcknowledgeNativeNotify(queue);
#endif

    PR_REMOVE_AND_INIT_LINK(&self->link);

    PR_ExitMonitor(queue->monitor);
}

PR_IMPLEMENT(void)
PL_FavorPerformanceHint(PRBool favorPerformanceOverEventStarvation,
                        PRUint32 starvationDelay)
{
}

/*******************************************************************************
 * Pure Event Queues
 *
 * For when you're only processing PLEvents and there is no native
 * select, thread messages, or AppleEvents.
 ******************************************************************************/

PR_IMPLEMENT(PLEvent*)
PL_WaitForEvent(PLEventQueue* self)
{
    PLEvent* event;
    PRMonitor* mon;

    if (self == NULL)
        return NULL;

    mon = self->monitor;
    PR_EnterMonitor(mon);

    while ((event = PL_GetEvent(self)) == NULL) {
        PRStatus err;
        PR_LOG(event_lm, PR_LOG_DEBUG, ("$$$ waiting for event"));
        err = PR_Wait(mon, PR_INTERVAL_NO_TIMEOUT);
        if ((err == PR_FAILURE)
            && (PR_PENDING_INTERRUPT_ERROR == PR_GetError())) break;
    }

    PR_ExitMonitor(mon);
    return event;
}

PR_IMPLEMENT(void)
PL_EventLoop(PLEventQueue* self)
{
    if (self == NULL)
        return;

    while (PR_TRUE) {
        PLEvent* event = PL_WaitForEvent(self);
        if (event == NULL) {
            /* This can only happen if the current thread is interrupted */
            return;
        }

        PR_LOG(event_lm, PR_LOG_DEBUG, ("$$$ processing event"));
        PL_HandleEvent(event);
        PR_LOG(event_lm, PR_LOG_DEBUG, ("$$$ done processing event"));
    }
}

/*******************************************************************************
 * Native Event Queues
 *
 * For when you need to call select, or WaitNextEvent, and yet also want
 * to handle PLEvents.
 ******************************************************************************/

static PRStatus
_pl_SetupNativeNotifier(PLEventQueue* self)
{
#if defined(XP_UNIX) && !defined(XP_MACOSX)
    int err;
    int flags;

    self->idFunc = 0;
    self->idFuncClosure = 0;

    err = pipe(self->eventPipe);
    if (err != 0) {
        return PR_FAILURE;
    }
#ifdef VBOX
    fcntl(self->eventPipe[0], F_SETFD, FD_CLOEXEC);
    fcntl(self->eventPipe[1], F_SETFD, FD_CLOEXEC);
#endif

    /* make the pipe nonblocking */
    flags = fcntl(self->eventPipe[0], F_GETFL, 0);
    if (flags == -1) {
        goto failed;
    }
    err = fcntl(self->eventPipe[0], F_SETFL, flags | O_NONBLOCK);
    if (err == -1) {
        goto failed;
    }
    flags = fcntl(self->eventPipe[1], F_GETFL, 0);
    if (flags == -1) {
        goto failed;
    }
    err = fcntl(self->eventPipe[1], F_SETFL, flags | O_NONBLOCK);
    if (err == -1) {
        goto failed;
    }
    return PR_SUCCESS;

failed:
    close(self->eventPipe[0]);
    close(self->eventPipe[1]);
    return PR_FAILURE;
#else
    return PR_SUCCESS;
#endif
}

static void
_pl_CleanupNativeNotifier(PLEventQueue* self)
{
#if defined(XP_UNIX) && !defined(XP_MACOSX)
    close(self->eventPipe[0]);
    close(self->eventPipe[1]);
#elif defined(MAC_USE_CFRUNLOOPSOURCE)

    CFRunLoopRemoveSource(self->mMainRunLoop, self->mRunLoopSource, kCFRunLoopCommonModes);
    CFRunLoopRemoveSource(self->mMainRunLoop, self->mRunLoopSource, self->mRunLoopModeStr);             /* vbox */
    CFRelease(self->mRunLoopSource);
    CFRelease(self->mMainRunLoop);
    CFRelease(self->mRunLoopModeStr);                                                                   /* vbox */
#endif
}

#if defined(XP_UNIX) && !defined(XP_MACOSX)

static PRStatus
_pl_NativeNotify(PLEventQueue* self)
{
#define NOTIFY_TOKEN    0xFA
    PRInt32 count;
    unsigned char buf[] = { NOTIFY_TOKEN };

# ifdef VBOX
    /* Don't write two chars, because we'll only acknowledge one and that'll
       cause trouble for anyone selecting/polling on the read descriptor. */
    if (self->notified)
        return PR_SUCCESS;
# endif

    PR_LOG(event_lm, PR_LOG_DEBUG,
           ("_pl_NativeNotify: self=%p",
            self));
    count = write(self->eventPipe[1], buf, 1);
    if (count == 1)
        return PR_SUCCESS;
    if (count == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
        return PR_SUCCESS;
    return PR_FAILURE;
}/* --- end _pl_NativeNotify() --- */
#endif /* defined(XP_UNIX) && !defined(XP_MACOSX) */

#if defined(XP_MACOSX)
static PRStatus
_pl_NativeNotify(PLEventQueue* self)
{
  	CFRunLoopSourceSignal(self->mRunLoopSource);
  	CFRunLoopWakeUp(self->mMainRunLoop);
    return PR_SUCCESS;
}
#endif /* defined(XP_MACOSX) */

static PRStatus
_pl_AcknowledgeNativeNotify(PLEventQueue* self)
{
#if defined(XP_UNIX) && !defined(XP_MACOSX)

    PRInt32 count;
    unsigned char c;
    PR_LOG(event_lm, PR_LOG_DEBUG,
            ("_pl_AcknowledgeNativeNotify: self=%p",
             self));
    /* consume the byte NativeNotify put in our pipe: */
    count = read(self->eventPipe[0], &c, 1);
    if ((count == 1) && (c == NOTIFY_TOKEN))
        return PR_SUCCESS;
    if ((count == -1) && ((errno == EAGAIN) || (errno == EWOULDBLOCK)))
        return PR_SUCCESS;
    return PR_FAILURE;
#elif defined(XP_MACOSX)                                                                                /* vbox */
                                                                                                        /* vbox */
    CFRunLoopRunInMode(self->mRunLoopModeStr, 0.0, 1);                                                  /* vbox */
    return PR_SUCCESS;                                                                                  /* vbox */
#else
    /* nothing to do on the other platforms */
    return PR_SUCCESS;
#endif
}

PR_IMPLEMENT(PRInt32)
PL_GetEventQueueSelectFD(PLEventQueue* self)
{
    if (self == NULL)
    return -1;

#if defined(XP_UNIX) && !defined(XP_MACOSX)
    return self->eventPipe[0];
#else
    return -1;    /* other platforms don't handle this (yet) */
#endif
}

PR_IMPLEMENT(PRBool)
PL_IsQueueOnCurrentThread( PLEventQueue *queue )
{
    PRThread *me = PR_GetCurrentThread();
    return me == queue->handlerThread;
}

PR_EXTERN(PRBool)
PL_IsQueueNative(PLEventQueue *queue)
{
    return queue->type == EventQueueIsNative ? PR_TRUE : PR_FALSE;
}

#if defined(XP_UNIX) && !defined(XP_MACOSX)
/*
** _md_CreateEventQueue() -- ModelDependent initializer
*/
static void _md_CreateEventQueue( PLEventQueue *eventQueue )
{
    /* there's really nothing special to do here,
    ** the guts of the unix stuff is in the setupnativenotify
    ** and related functions.
    */
    return;
} /* end _md_CreateEventQueue() */
#endif /* defined(XP_UNIX) && !defined(XP_MACOSX) */

static void _md_EventReceiverProc(void *info)
{
  PLEventQueue *queue = (PLEventQueue*)info;
  PL_ProcessPendingEvents(queue);
}

#if defined(XP_MACOSX)
static void _md_CreateEventQueue( PLEventQueue *eventQueue )
{
    CFRunLoopSourceContext sourceContext = { 0 };
    sourceContext.version = 0;
    sourceContext.info = (void*)eventQueue;
    sourceContext.perform = _md_EventReceiverProc;

    /* make a run loop source */
    eventQueue->mRunLoopSource = CFRunLoopSourceCreate(kCFAllocatorDefault, 0 /* order */, &sourceContext);
    PR_ASSERT(eventQueue->mRunLoopSource);

    eventQueue->mMainRunLoop = CFRunLoopGetCurrent();
    CFRetain(eventQueue->mMainRunLoop);

    /* and add it to the run loop */
    CFRunLoopAddSource(eventQueue->mMainRunLoop, eventQueue->mRunLoopSource, kCFRunLoopCommonModes);

    /* Add it again but with a unique mode name so we can acknowledge it
       without processing any other message sources. */
    {                                                                                                   /* vbox */
        char szModeName[80];                                                                            /* vbox */
        snprintf(szModeName, sizeof(szModeName), "VBoxXPCOMQueueMode-%p", eventQueue);                  /* vbox */
        eventQueue->mRunLoopModeStr = CFStringCreateWithCString(kCFAllocatorDefault,                    /* vbox */
                                                                szModeName, kCFStringEncodingASCII);    /* vbox */
        CFRunLoopAddSource(eventQueue->mMainRunLoop,                                                    /* vbox */
                           eventQueue->mRunLoopSource, eventQueue->mRunLoopModeStr);                    /* vbox */
    }                                                                                                   /* vbox */
} /* end _md_CreateEventQueue() */
#endif /* defined(XP_MACOSX) */

/* extra functions for unix */

#if defined(XP_UNIX) && !defined(XP_MACOSX)

PR_IMPLEMENT(PRInt32)
PL_ProcessEventsBeforeID(PLEventQueue *aSelf, unsigned long aID)
{
    PRInt32 count = 0;
    PRInt32 fullCount;

    if (aSelf == NULL)
        return -1;

    PR_EnterMonitor(aSelf->monitor);

    if (aSelf->processingEvents) {
        PR_ExitMonitor(aSelf->monitor);
        return 0;
    }

    aSelf->processingEvents = PR_TRUE;

    /* Only process the events that are already in the queue, and
     * not any new events that get added. Do this by counting the
     * number of events currently in the queue
     */
    fullCount = _pl_GetEventCount(aSelf);
    PR_LOG(event_lm, PR_LOG_DEBUG,
           ("$$$ fullCount is %d id is %ld\n", fullCount, aID));

    if (fullCount == 0) {
        aSelf->processingEvents = PR_FALSE;
        PR_ExitMonitor(aSelf->monitor);
        return 0;
    }

    PR_ExitMonitor(aSelf->monitor);

    while (fullCount-- > 0) {
        /* peek at the next event */
        PLEvent *event;
        event = PR_EVENT_PTR(aSelf->queue.next);
        if (event == NULL)
            break;
        PR_LOG(event_lm, PR_LOG_DEBUG, ("$$$ processing event %ld\n",
                                        event->id));
        if (event->id >= aID) {
            PR_LOG(event_lm, PR_LOG_DEBUG, ("$$$ skipping event and breaking"));
            break;
        }

        event = PL_GetEvent(aSelf);
        PL_HandleEvent(event);
        PR_LOG(event_lm, PR_LOG_DEBUG, ("$$$ done processing event"));
        count++;
    }

    PR_EnterMonitor(aSelf->monitor);

    /* if full count still had items left then there's still items left
       in the queue.  Let the native notify token stay. */

    if (aSelf->type == EventQueueIsNative) {
        fullCount = _pl_GetEventCount(aSelf);

        if (fullCount <= 0) {
            _pl_AcknowledgeNativeNotify(aSelf);
            aSelf->notified = PR_FALSE;
        }
    }

    aSelf->processingEvents = PR_FALSE;

    PR_ExitMonitor(aSelf->monitor);

    return count;
}

PR_IMPLEMENT(void)
PL_RegisterEventIDFunc(PLEventQueue *aSelf, PLGetEventIDFunc aFunc,
                       void *aClosure)
{
    aSelf->idFunc = aFunc;
    aSelf->idFuncClosure = aClosure;
}

PR_IMPLEMENT(void)
PL_UnregisterEventIDFunc(PLEventQueue *aSelf)
{
    aSelf->idFunc = 0;
    aSelf->idFuncClosure = 0;
}

#endif /* defined(XP_UNIX) && !defined(XP_MACOSX) */

/* --- end plevent.c --- */
