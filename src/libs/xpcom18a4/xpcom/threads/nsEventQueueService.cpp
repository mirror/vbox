/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Rick Potts <rpotts@netscape.com>
 *   Ramiro Estrugo <ramiro@netscape.com>
 *   Warren Harris <warren@netscape.com>
 *   Leaf Nunes <leaf@mozilla.org>
 *   David Matiskella <davidm@netscape.com>
 *   David Hyatt <hyatt@netscape.com>
 *   Seth Spitzer <sspitzer@netscape.com>
 *   Suresh Duddi <dp@netscape.com>
 *   Bruce Mitchener <bruce@cybersight.com>
 *   Scott Collins <scc@netscape.com>
 *   Daniel Matejka <danm@netscape.com>
 *   Doug Turner <dougt@netscape.com>
 *   Stuart Parmenter <pavlov@netscape.com>
 *   Mike Kaply <mkaply@us.ibm.com>
 *   Dan Mosedale <dmose@mozilla.org>
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

#include "nsEventQueueService.h"
#include "nsIComponentManager.h"
#include "nsPIEventQueueChain.h"

#include "nsXPCOM.h"

#include <iprt/assert.h>
#include <VBox/log.h>

static NS_DEFINE_CID(kEventQueueCID, NS_EVENTQUEUE_CID);

nsEventQueueServiceImpl::nsEventQueueServiceImpl()
{
    mEventQMonitor = NIL_RTSEMFASTMUTEX;
    int vrc = RTSemFastMutexCreate(&mEventQMonitor);
    AssertRC(vrc); RT_NOREF(vrc);
}

PR_STATIC_CALLBACK(PLDHashOperator)
hash_enum_remove_queues(const void *aThread_ptr,
                        nsCOMPtr<nsIEventQueue>& aEldestQueue,
                        void* closure)
{
    // 'aQueue' should be the eldest queue.
    nsCOMPtr<nsPIEventQueueChain> pie(do_QueryInterface(aEldestQueue));
    nsCOMPtr<nsIEventQueue> q;

    // stop accepting events for youngest to oldest
    pie->GetYoungest(getter_AddRefs(q));
    while (q)
    {
        q->StopAcceptingEvents();

        nsCOMPtr<nsPIEventQueueChain> pq(do_QueryInterface(q));
        pq->GetElder(getter_AddRefs(q));
    }

    return PL_DHASH_REMOVE;
}

nsEventQueueServiceImpl::~nsEventQueueServiceImpl()
{
    // XXX make it so we only enum over this once
    mEventQTable.Enumerate(hash_enum_remove_queues, nsnull); // call StopAcceptingEvents on everything and clear out the hashtable

    int vrc = RTSemFastMutexDestroy(mEventQMonitor);
    AssertRC(vrc); RT_NOREF(vrc);
    mEventQMonitor = NIL_RTSEMFASTMUTEX;
}

nsresult
nsEventQueueServiceImpl::Init()
{
    NS_ENSURE_TRUE(mEventQMonitor != NIL_RTSEMFASTMUTEX, NS_ERROR_OUT_OF_MEMORY);

    // This will only be called once on the main thread, so it's safe to
    // not enter the monitor here.
    if (!mEventQTable.Init())
        return NS_ERROR_OUT_OF_MEMORY;

    // ensure that a main thread event queue exists!
    RTTHREAD hMainThread;
    nsresult rv = NS_GetMainThread(&hMainThread);
    if (NS_SUCCEEDED(rv))
      rv = CreateEventQueue(hMainThread, PR_TRUE);

  return rv;
}

/* nsISupports interface implementation... */
NS_IMPL_THREADSAFE_ISUPPORTS1(nsEventQueueServiceImpl, nsIEventQueueService)

/* nsIEventQueueService interface implementation... */

NS_IMETHODIMP
nsEventQueueServiceImpl::CreateThreadEventQueue()
{
    return CreateEventQueue(RTThreadSelf(), PR_TRUE);
}

NS_IMETHODIMP
nsEventQueueServiceImpl::CreateMonitoredThreadEventQueue()
{
    return CreateEventQueue(RTThreadSelf(), PR_FALSE);
}

NS_IMETHODIMP
nsEventQueueServiceImpl::CreateFromIThread(RTTHREAD aThread, PRBool aNative,
                                           nsIEventQueue **aResult)
{
    nsresult rv;

    rv = CreateEventQueue(aThread, aNative); // addrefs
    if (NS_SUCCEEDED(rv))
      rv = GetThreadEventQueue(aThread, aResult); // addrefs

    return rv;
}

// private method
NS_IMETHODIMP
nsEventQueueServiceImpl::MakeNewQueue(RTTHREAD hThread,
                                      PRBool aNative,
                                      nsIEventQueue **aQueue)
{
    nsresult rv;
    nsCOMPtr<nsIEventQueue> queue = do_CreateInstance(kEventQueueCID, &rv);

    if (NS_SUCCEEDED(rv))
        rv = queue->InitFromPRThread(hThread, aNative);

    *aQueue = queue;
    NS_IF_ADDREF(*aQueue);
    return rv;
}

// private method
NS_IMETHODIMP
nsEventQueueServiceImpl::CreateEventQueue(RTTHREAD aThread, PRBool aNative)
{
    nsresult rv = NS_OK;
    /* Enter the lock that protects the EventQ hashtable... */
    RTSemFastMutexRequest(mEventQMonitor);

    /* create only one event queue chain per thread... */
    if (!mEventQTable.GetWeak(aThread))
    {
        nsCOMPtr<nsIEventQueue> queue;

        // we don't have one in the table
        rv = MakeNewQueue(aThread, aNative, getter_AddRefs(queue)); // create new queue
        mEventQTable.Put(aThread, queue); // add to the table (initial addref)
    }

    // Release the EventQ lock...
    RTSemFastMutexRelease(mEventQMonitor);
    return rv;
}


NS_IMETHODIMP
nsEventQueueServiceImpl::DestroyThreadEventQueue(void)
{
    nsresult rv = NS_OK;

    /* Enter the lock that protects the EventQ hashtable... */
    RTSemFastMutexRequest(mEventQMonitor);

    RTTHREAD hThread = RTThreadSelf();
    nsIEventQueue* queue = mEventQTable.GetWeak(hThread);
    if (queue)
    {
        queue->StopAcceptingEvents(); // tell the queue to stop accepting events
        queue = nsnull; // Queue may die on the next line
        mEventQTable.Remove(hThread); // remove nsIEventQueue from hash table (releases)
    }

    // Release the EventQ lock...
    RTSemFastMutexRelease(mEventQMonitor);
    return rv;
}

NS_IMETHODIMP
nsEventQueueServiceImpl::CreateFromPLEventQueue(PLEventQueue* aPLEventQueue, nsIEventQueue** aResult)
{
    // Create our thread queue using the component manager
    nsresult rv;
    nsCOMPtr<nsIEventQueue> queue = do_CreateInstance(kEventQueueCID, &rv);
    if (NS_FAILED(rv))
        return rv;

    rv = queue->InitFromPLQueue(aPLEventQueue);
    if (NS_FAILED(rv))
        return rv;

    *aResult = queue;
    NS_IF_ADDREF(*aResult);
    return NS_OK;
}


// Return the active event queue on our chain
/* inline */
nsresult nsEventQueueServiceImpl::GetYoungestEventQueue(nsIEventQueue *queue, nsIEventQueue **aResult)
{
    nsCOMPtr<nsIEventQueue> answer;

    if (queue)
    {
        nsCOMPtr<nsPIEventQueueChain> ourChain(do_QueryInterface(queue));
        if (ourChain)
            ourChain->GetYoungestActive(getter_AddRefs(answer));
        else
            answer = queue;
    }

    *aResult = answer;
    NS_IF_ADDREF(*aResult);
    return NS_OK;
}


// create new event queue, append it to the current thread's chain of event queues.
// return it, addrefed.
NS_IMETHODIMP
nsEventQueueServiceImpl::PushThreadEventQueue(nsIEventQueue **aNewQueue)
{
    nsresult rv = NS_OK;
    RTTHREAD hThread = RTThreadSelf();
    PRBool native = PR_TRUE; // native by default as per old comment

    NS_ASSERTION(aNewQueue, "PushThreadEventQueue called with null param");

    /* Enter the lock that protects the EventQ hashtable... */
    RTSemFastMutexRequest(mEventQMonitor);

    nsIEventQueue* queue = mEventQTable.GetWeak(hThread);
    
    NS_ASSERTION(queue, "pushed event queue on top of nothing");

    if (queue)
    {
        // find out what kind of queue our relatives are
        nsCOMPtr<nsIEventQueue> youngQueue;
        GetYoungestEventQueue(queue, getter_AddRefs(youngQueue));
        if (youngQueue)
          youngQueue->IsQueueNative(&native);
    }

    nsIEventQueue* newQueue = nsnull;
    MakeNewQueue(hThread, native, &newQueue); // create new queue; addrefs

    if (!queue)
    {
        // shouldn't happen. as a fallback, we guess you wanted a native queue
        mEventQTable.Put(hThread, newQueue);
    }

    // append to the event queue chain
    nsCOMPtr<nsPIEventQueueChain> ourChain(do_QueryInterface(queue)); // QI the queue in the hash table
    if (ourChain)
        ourChain->AppendQueue(newQueue); // append new queue to it

    *aNewQueue = newQueue;

#ifdef LOG_ENABLED
    PLEventQueue *equeue;
    (*aNewQueue)->GetPLEventQueue(&equeue);
    Log(("EventQueue: Service push queue [queue=%lx]",(long)equeue));
#endif

    // Release the EventQ lock...
    RTSemFastMutexRelease(mEventQMonitor);
    return rv;
}

// disable and release the given queue (though the last one won't be released)
NS_IMETHODIMP
nsEventQueueServiceImpl::PopThreadEventQueue(nsIEventQueue *aQueue)
{
    RTTHREAD hThread = RTThreadSelf();

    /* Enter the lock that protects the EventQ hashtable... */
    RTSemFastMutexRequest(mEventQMonitor);

    nsCOMPtr<nsIEventQueue> eldestQueue;
    mEventQTable.Get(hThread, getter_AddRefs(eldestQueue));

    // If we are popping the eldest queue, remove its mEventQTable entry.
    if (aQueue == eldestQueue)
        mEventQTable.Remove(hThread);

    // Exit the monitor before processing pending events to avoid deadlock.
    // Our reference from the eldestQueue nsCOMPtr will keep that object alive.
    // Since it is thread-private, no one else can race with us here.
    RTSemFastMutexRelease(mEventQMonitor);
    if (!eldestQueue)
        return NS_ERROR_FAILURE;

#ifdef LOG_ENABLED
    PLEventQueue *equeue;
    aQueue->GetPLEventQueue(&equeue);
    Log(("EventQueue: Service pop queue [queue=%lx]",(long)equeue));
#endif
    aQueue->StopAcceptingEvents();
    aQueue->ProcessPendingEvents(); // make sure we don't orphan any events

    return NS_OK;
}

NS_IMETHODIMP
nsEventQueueServiceImpl::GetThreadEventQueue(RTTHREAD aThread, nsIEventQueue** aResult)
{
    /* Parameter validation... */
    AssertReturn(aResult != NULL, NS_ERROR_NULL_POINTER);

    RTTHREAD keyThread = aThread;

    if (keyThread == NS_CURRENT_THREAD)
         keyThread = RTThreadSelf();
    else if (keyThread == NS_UI_THREAD)
    {
        // Get the primordial thread
        nsresult rv = NS_GetMainThread(&keyThread);
        if (NS_FAILED(rv))
            return rv;
    }

    /* Enter the lock that protects the EventQ hashtable... */
    RTSemFastMutexRequest(mEventQMonitor);

    nsCOMPtr<nsIEventQueue> queue;
    mEventQTable.Get(keyThread, getter_AddRefs(queue));

    RTSemFastMutexRelease(mEventQMonitor);

    if (queue)
      GetYoungestEventQueue(queue, aResult); // get the youngest active queue
    else
      *aResult = nsnull;

    // XXX: Need error code for requesting an event queue when none exists...
    if (!*aResult)
      return NS_ERROR_NOT_AVAILABLE;

    return NS_OK;
}


NS_IMETHODIMP
nsEventQueueServiceImpl::ResolveEventQueue(nsIEventQueue* queueOrConstant, nsIEventQueue* *resultQueue)
{
    if (queueOrConstant == NS_CURRENT_EVENTQ)
        return GetThreadEventQueue(NS_CURRENT_THREAD, resultQueue);
    else if (queueOrConstant == NS_UI_THREAD_EVENTQ)
        return GetThreadEventQueue(NS_UI_THREAD, resultQueue);

    *resultQueue = queueOrConstant;
    NS_ADDREF(*resultQueue);
    return NS_OK;
}

NS_IMETHODIMP
nsEventQueueServiceImpl::GetSpecialEventQueue(PRInt32 aQueue,
                                              nsIEventQueue* *_retval)
{
    AssertReturn(_retval, NS_ERROR_NULL_POINTER);

    // try and get the requested event queue, returning NS_ERROR_FAILURE if there
    // is a problem.  GetThreadEventQueue() does the AddRef() for us.
    switch (aQueue)
    {
        case CURRENT_THREAD_EVENT_QUEUE:
        {
            nsresult rv = GetThreadEventQueue(NS_CURRENT_THREAD, _retval);
            if (NS_FAILED(rv))
                return NS_ERROR_FAILURE;
            break;
        }
        case UI_THREAD_EVENT_QUEUE:
        {
            nsresult rv = GetThreadEventQueue(NS_UI_THREAD, _retval);
            if (NS_FAILED(rv))
                return NS_ERROR_FAILURE;
            break;
        }
        /* somebody handed us a bogus constant */
        default:
            return NS_ERROR_ILLEGAL_VALUE;
    }

    return NS_OK;
}

