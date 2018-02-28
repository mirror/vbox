/* $Id$ */
/** @file
* VBox Global COM Class implementation.
*/

/*
 * Copyright (C) 2017-2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "Logging.h"
#include "VirtualBoxClientListImpl.h"

#include <iprt/process.h>
#include <iprt/asm.h>


////////////////// CClientListWatcher implementation /////////////////

/*
* Helper class that tracks existance of registered client processes.
* It removes the client process from client list if it shutdowned unexpectedly,
* without calling DeRegisterClient().
* Also it notifies VBoxSVC and VBoxSDS that this client process finished.
*/
class CClientListWatcher
{
public:
    CClientListWatcher(TClientSet& list, RTCRITSECTRW& m_clientListCritSect);
    virtual ~CClientListWatcher();

protected:
    static DECLCALLBACK(int) WatcherWorker(RTTHREAD ThreadSelf, void *pvUser);
    void NotifySDSAllClientsFinished();
    // m_WatcherThread is static to check that single watcher thread used only
    static volatile RTTHREAD    m_WatcherThread;
    volatile bool               m_fWatcherRunning;
    TClientSet&                 m_clientList;
    RTCRITSECTRW&               m_clientListCritSect;
    RTSEMEVENT                  m_wakeUpWatcherEvent;
};

volatile RTTHREAD CClientListWatcher::m_WatcherThread = NULL;

CClientListWatcher::CClientListWatcher(TClientSet& list, RTCRITSECTRW& clientListCritSect)
    : m_clientList(list), m_clientListCritSect(clientListCritSect)
{
    Assert(ASMAtomicReadPtr((void* volatile*)&CClientListWatcher::m_WatcherThread) == NULL);

    if (ASMAtomicReadPtr((void* volatile*)&CClientListWatcher::m_WatcherThread) != NULL)
    {
        LogRelFunc(("Error: watcher thread already created.\n"));
        return;
    }

    int rc = RTSemEventCreate(&m_wakeUpWatcherEvent);
    if (RT_FAILURE(rc))
    {
        LogRelFunc(("Error: failed to create wake up event for watcher thread. %Rrs \n", rc));
        return;
    }

    RTTHREAD watcherThread;
    rc = RTThreadCreate(&watcherThread,
        (PFNRTTHREAD)CClientListWatcher::WatcherWorker,
        this, // pVUser
        0,    // cbStack
        RTTHREADTYPE_DEFAULT,
        RTTHREADFLAGS_WAITABLE,
        "CLWatcher");
    if (RT_SUCCESS(rc))
    {
        ASMAtomicWritePtr((void* volatile*)&CClientListWatcher::m_WatcherThread, watcherThread);
        LogRelFunc(("Created client list watcher thread.\n"));
    }
    else
        LogRelFunc(("Failed to create client list watcher thread %Rrs \n", rc));
}


CClientListWatcher::~CClientListWatcher()
{
    Assert(ASMAtomicReadPtr((void* volatile*)&CClientListWatcher::m_WatcherThread));

    // mark watcher thread to finish
    ASMAtomicWriteBool(&m_fWatcherRunning, false);

    // Wake up watcher thread to finish it faster
    int rc = RTSemEventSignal(m_wakeUpWatcherEvent);
    Assert(RT_SUCCESS(rc));

    rc = RTThreadWait(CClientListWatcher::m_WatcherThread, RT_INDEFINITE_WAIT, NULL);
    if (RT_FAILURE(rc))
        LogRelFunc(("Error: watcher thread didn't finished. Possible thread leak. %Rrs \n", rc));
    else
        LogRelFunc(("Watcher thread finished. %\n"));

    ASMAtomicWriteNullPtr((void* volatile*)&CClientListWatcher::m_WatcherThread);

    RTSemEventDestroy(m_wakeUpWatcherEvent);
}


/*
* Notifies the VBoxSDS that API client list is empty.
* Initiates the chain of closing VBoxSDS/VBoxSVC.
* VBoxSDS, in it's turn notifies VBoxSvc that relates on it,
* VBoxSvc instances releases itself and releases their references to VBoxSDS.
* in result VBoxSDS and VBoxSvc finishes itself after delay
*/
void CClientListWatcher::NotifySDSAllClientsFinished()
{
    ComPtr<IVirtualBoxSDS> ptrVirtualBoxSDS;
    /*
    * Connect to VBoxSDS.
    */
    HRESULT hrc = CoCreateInstance(CLSID_VirtualBoxSDS, NULL, CLSCTX_LOCAL_SERVER, IID_IVirtualBoxSDS,
        (void **)ptrVirtualBoxSDS.asOutParam());
    if (SUCCEEDED(hrc))
    {
        LogRelFunc(("Notify SDS that all API clients finished\n"));
        ptrVirtualBoxSDS->NotifyClientsFinished();
    }
}


/*
* Deregister all staled VBoxSVC through VBoxSDS and forcebly close VBoxSVC process
* Parameters:
*   ThreadSelf - current thread id
*   pvUser - pointer to CClientListWatcher that created this thread. */
DECLCALLBACK(int) CClientListWatcher::WatcherWorker(RTTHREAD ThreadSelf, void *pvUser)
{
    NOREF(ThreadSelf);
    Assert(ASMAtomicReadPtr((void* volatile*)&CClientListWatcher::m_WatcherThread));
    LogRelFunc(("Enter watcher thread\n"));

    CClientListWatcher* pThis = (CClientListWatcher*)pvUser;
    Assert(pThis);

    ASMAtomicWriteBool(&pThis->m_fWatcherRunning, true);

    while (ASMAtomicReadBool(&pThis->m_fWatcherRunning))
    {
        /* remove finished API clients from list */
        int rc = RTCritSectRwEnterShared(&pThis->m_clientListCritSect);
        Assert(RT_SUCCESS(rc));
        NOREF(rc);

        TClientSet::iterator it = pThis->m_clientList.begin();
        TClientSet::iterator end = pThis->m_clientList.end();
        for (; it != end; ++it)
        {
            // check status of client process by his pid
            int rc = RTProcWait(*it, RTPROCWAIT_FLAGS_NOBLOCK, NULL);
            if (rc == VERR_PROCESS_NOT_FOUND)
            {
                LogRelFunc(("Finished process detected: %d\n", *it));
                /* delete finished process from client list */
                it = pThis->m_clientList.erase(it);

                if (pThis->m_clientList.empty())
                {
                    /*
                        Starts here chain of events between SDS and VBoxSVC
                       to shutdown them in case when all clients finished and
                       some of them crashed
                    */
                    pThis->NotifySDSAllClientsFinished();
                }
            }
        }

        rc = RTCritSectRwLeaveShared(&pThis->m_clientListCritSect);
        Assert(RT_SUCCESS(rc));

        /*
         * Wait for two second before next iteration.
         * Destructor will wake up it immidietely.
         */
        RTSemEventWait(pThis->m_wakeUpWatcherEvent, 2000);
    }
    LogRelFunc(("Finish watcher thread.  Client list size: %d\n", pThis->m_clientList.size()));
    return 0;
}

///////////////////////// VirtualBoxClientList implementation //////////////////////////

HRESULT VirtualBoxClientList::FinalConstruct()
{
    int rc = RTCritSectRwInit(&m_MapCritSect);
    Assert(RT_SUCCESS(rc));
    if (RT_FAILURE(rc))
    {
        LogRelFunc(("Error: creating client list critical section. %Rrs\n", rc));
        return E_FAIL;
    }

    m_pWatcher = new CClientListWatcher(m_ClientSet, m_MapCritSect);
    Assert(m_pWatcher);

    AutoInitSpan autoInitSpan(this);
    autoInitSpan.setSucceeded();
    BaseFinalConstruct();

    LogRelFunc(("VirtualBoxClientList initialized.\n"));
    return S_OK;
}

void VirtualBoxClientList::FinalRelease()
{
    Assert(m_pWatcher);
    if (m_pWatcher)
    {
        delete m_pWatcher;
        m_pWatcher = NULL;
    }

    int rc = RTCritSectRwDelete(&m_MapCritSect);
    Assert(RT_SUCCESS(rc));
    NOREF(rc);

    BaseFinalRelease();
    LogRelFunc(("VirtualBoxClientList released. res=%d\n", rc));
}

/*
* Deregister API client to add it to API client list
* API client process calls this function at start to include this process to client list
*
*/
HRESULT VirtualBoxClientList::registerClient(LONG aPid)
{
    int rc = RTCritSectRwEnterExcl(&m_MapCritSect);
    Assert(RT_SUCCESS(rc));
    NOREF(rc);
    Assert(m_pWatcher);

    m_ClientSet.insert(aPid);

    rc = RTCritSectRwLeaveExcl(&m_MapCritSect);
    Assert(RT_SUCCESS(rc));
    LogRelFunc(("VirtualBoxClientList client registered. pid: %d res=%d\n", aPid, rc));
    return S_OK;
}


/*
* Returns list of api client processes.
* Result list contains PID of all clients exept VBoxSDS and VBoxSVC
*   aEnvironment - reference to clients list that should be filled here
*
*/
HRESULT VirtualBoxClientList::getClients(std::vector<LONG> &aEnvironment)
{
    int rc = RTCritSectRwEnterShared(&m_MapCritSect);
    Assert(RT_SUCCESS(rc));
    NOREF(rc);
    if (RT_SUCCESS(rc))
    {
        if (!m_ClientSet.empty())
        {
            Assert(aEnvironment.empty());
            aEnvironment.reserve(m_ClientSet.size());
            aEnvironment.assign(m_ClientSet.begin(), m_ClientSet.end());
            Assert(aEnvironment.size());
        }
        else
        {
            LogFunc(("Client list is empty\n"));
        }

        rc = RTCritSectRwLeaveShared(&m_MapCritSect);
        Assert(RT_SUCCESS(rc));
    }
    LogRelFunc(("VirtualBoxClientList client list requested. res=%d\n", rc));
    return S_OK;
}
