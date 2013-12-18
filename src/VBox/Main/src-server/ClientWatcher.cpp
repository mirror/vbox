/** @file
 *
 * VirtualBox API client session crash watcher
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/log.h>
#include <iprt/semaphore.h>
#include <iprt/process.h>

#include <VBox/com/defs.h>

#include <vector>

#include "VirtualBoxBase.h"
#include "AutoCaller.h"
#include "ClientWatcher.h"
#include "ClientToken.h"
#include "VirtualBoxImpl.h"
#include "MachineImpl.h"

#if defined(VBOX_WITH_SYS_V_IPC_SESSION_WATCHER) || defined(VBOX_WITH_GENERIC_SESSION_WATCHER)
/** Table for adaptive timeouts. After an update the counter starts at the
 * maximum value and decreases to 0, i.e. first the short timeouts are used
 * and then the longer ones. This minimizes the detection latency in the
 * cases where a change is expected, for crashes. */
static const RTMSINTERVAL s_aUpdateTimeoutSteps[] = { 500, 200, 100, 50, 20, 10, 5 };
#endif



VirtualBox::ClientWatcher::ClientWatcher() :
    mLock(LOCKCLASS_OBJECTSTATE)
{
    AssertReleaseFailed();
}

VirtualBox::ClientWatcher::~ClientWatcher()
{
    if (mThread != NIL_RTTHREAD)
    {
        /* signal the client watcher thread, should be exiting now */
        update();
        /* wait for termination */
        RTThreadWait(mThread, RT_INDEFINITE_WAIT, NULL);
        mThread = NIL_RTTHREAD;
    }
    mProcesses.clear();
#if defined(RT_OS_WINDOWS)
    if (mUpdateReq != NULL)
    {
        ::CloseHandle(mUpdateReq);
        mUpdateReq = NULL;
    }
#elif defined(RT_OS_OS2) || defined(VBOX_WITH_SYS_V_IPC_SESSION_WATCHER) || defined(VBOX_WITH_GENERIC_SESSION_WATCHER)
    if (mUpdateReq != NIL_RTSEMEVENT)
    {
        RTSemEventDestroy(mUpdateReq);
        mUpdateReq = NIL_RTSEMEVENT;
    }
#else
# error "Port me!"
#endif
}

VirtualBox::ClientWatcher::ClientWatcher(const ComObjPtr<VirtualBox> &pVirtualBox) :
    mVirtualBox(pVirtualBox),
    mThread(NIL_RTTHREAD),
    mUpdateReq(CWUPDATEREQARG),
    mLock(LOCKCLASS_OBJECTSTATE)
{
#if defined(RT_OS_WINDOWS)
    mUpdateReq = ::CreateEvent(NULL, FALSE, FALSE, NULL);
#elif defined(RT_OS_OS2)
    RTSemEventCreate(&mUpdateReq);
#elif defined(VBOX_WITH_SYS_V_IPC_SESSION_WATCHER) || defined(VBOX_WITH_GENERIC_SESSION_WATCHER)
    RTSemEventCreate(&mUpdateReq);
    /* start with high timeouts, nothing to do */
    ASMAtomicUoWriteU8(&mUpdateAdaptCtr, 0);
#else
# error "Port me!"
#endif

    int vrc = RTThreadCreate(&mThread,
                             worker,
                             (void *)this,
                             0,
                             RTTHREADTYPE_MAIN_WORKER,
                             RTTHREADFLAGS_WAITABLE,
                             "Watcher");
    AssertRC(vrc);
}

bool VirtualBox::ClientWatcher::isReady()
{
    return mThread != NIL_RTTHREAD;
}

/**
 * Sends a signal to the thread to rescan the clients/VMs having open sessions.
 */
void VirtualBox::ClientWatcher::update()
{
    AssertReturnVoid(mThread != NIL_RTTHREAD);

    /* sent an update request */
#if defined(RT_OS_WINDOWS)
    ::SetEvent(mUpdateReq);
#elif defined(RT_OS_OS2)
    RTSemEventSignal(mUpdateReq);
#elif defined(VBOX_WITH_SYS_V_IPC_SESSION_WATCHER)
    /* use short timeouts, as we expect changes */
    ASMAtomicUoWriteU8(&mUpdateAdaptCtr, RT_ELEMENTS(s_aUpdateTimeoutSteps) - 1);
    RTSemEventSignal(mUpdateReq);
#elif defined(VBOX_WITH_GENERIC_SESSION_WATCHER)
    RTSemEventSignal(mUpdateReq);
#else
# error "Port me!"
#endif
}

/**
 * Adds a process to the list of processes to be reaped. This call should be
 * followed by a call to update() to cause the necessary actions immediately,
 * in case the process crashes straight away.
 */
void VirtualBox::ClientWatcher::addProcess(RTPROCESS pid)
{
    AssertReturnVoid(mThread != NIL_RTTHREAD);
    /* @todo r=klaus, do the reaping on all platforms! */
#ifndef RT_OS_WINDOWS
    AutoWriteLock alock(mLock COMMA_LOCKVAL_SRC_POS);
    mProcesses.push_back(pid);
#endif
}

/**
 * Thread worker function that watches the termination of all client processes
 * that have open sessions using IMachine::LockMachine()
 */
/*static*/
DECLCALLBACK(int) VirtualBox::ClientWatcher::worker(RTTHREAD /* thread */, void *pvUser)
{
    LogFlowFuncEnter();

    VirtualBox::ClientWatcher *that = (VirtualBox::ClientWatcher *)pvUser;
    Assert(that);

    typedef std::vector<ComObjPtr<Machine> > MachineVector;
    typedef std::vector<ComObjPtr<SessionMachine> > SessionMachineVector;

    SessionMachineVector machines;
    MachineVector spawnedMachines;

    size_t cnt = 0;
    size_t cntSpawned = 0;

    VirtualBoxBase::initializeComForThread();

#if defined(RT_OS_WINDOWS)

    /// @todo (dmik) processes reaping!

    HANDLE handles[MAXIMUM_WAIT_OBJECTS];
    handles[0] = that->mUpdateReq;

    do
    {
        AutoCaller autoCaller(that->mVirtualBox);
        /* VirtualBox has been early uninitialized, terminate */
        if (!autoCaller.isOk())
            break;

        do
        {
            /* release the caller to let uninit() ever proceed */
            autoCaller.release();

            DWORD rc = ::WaitForMultipleObjects((DWORD)(1 + cnt + cntSpawned),
                                                handles,
                                                FALSE,
                                                INFINITE);

            /* Restore the caller before using VirtualBox. If it fails, this
             * means VirtualBox is being uninitialized and we must terminate. */
            autoCaller.add();
            if (!autoCaller.isOk())
                break;

            bool update = false;

            if (rc == WAIT_OBJECT_0)
            {
                /* update event is signaled */
                update = true;
            }
            else if (rc > WAIT_OBJECT_0 && rc <= (WAIT_OBJECT_0 + cnt))
            {
                /* machine mutex is released */
                (machines[rc - WAIT_OBJECT_0 - 1])->checkForDeath();
                update = true;
            }
            else if (rc > WAIT_ABANDONED_0 && rc <= (WAIT_ABANDONED_0 + cnt))
            {
                /* machine mutex is abandoned due to client process termination */
                (machines[rc - WAIT_ABANDONED_0 - 1])->checkForDeath();
                update = true;
            }
            else if (rc > WAIT_OBJECT_0 + cnt && rc <= (WAIT_OBJECT_0 + cntSpawned))
            {
                /* spawned VM process has terminated (normally or abnormally) */
                (spawnedMachines[rc - WAIT_OBJECT_0 - cnt - 1])->
                    checkForSpawnFailure();
                update = true;
            }

            if (update)
            {
                /* close old process handles */
                for (size_t i = 1 + cnt; i < 1 + cnt + cntSpawned; ++i)
                    CloseHandle(handles[i]);

                // get reference to the machines list in VirtualBox
                VirtualBox::MachinesOList &allMachines = that->mVirtualBox->getMachinesList();

                // lock the machines list for reading
                AutoReadLock thatLock(allMachines.getLockHandle() COMMA_LOCKVAL_SRC_POS);

                /* obtain a new set of opened machines */
                cnt = 0;
                machines.clear();

                for (MachinesOList::iterator it = allMachines.begin();
                     it != allMachines.end();
                     ++it)
                {
                    /// @todo handle situations with more than 64 objects
                    AssertMsgBreak((1 + cnt) <= MAXIMUM_WAIT_OBJECTS,
                                   ("MAXIMUM_WAIT_OBJECTS reached"));

                    ComObjPtr<SessionMachine> sm;
                    if ((*it)->isSessionOpenOrClosing(sm))
                    {
                        AutoCaller smCaller(sm);
                        if (smCaller.isOk())
                        {
                            AutoReadLock smLock(sm COMMA_LOCKVAL_SRC_POS);
                            Machine::ClientToken *ct = sm->getClientToken();
                            if (ct)
                            {
                                HANDLE ipcSem = ct->getToken();
                                machines.push_back(sm);
                                handles[1 + cnt] = ipcSem;
                                ++cnt;
                            }
                        }
                    }
                }

                LogFlowFunc(("UPDATE: direct session count = %d\n", cnt));

                /* obtain a new set of spawned machines */
                cntSpawned = 0;
                spawnedMachines.clear();

                for (MachinesOList::iterator it = allMachines.begin();
                     it != allMachines.end();
                     ++it)
                {
                    /// @todo handle situations with more than 64 objects
                    AssertMsgBreak((1 + cnt + cntSpawned) <= MAXIMUM_WAIT_OBJECTS,
                                   ("MAXIMUM_WAIT_OBJECTS reached"));

                    if ((*it)->isSessionSpawning())
                    {
                        ULONG pid;
                        HRESULT hrc = (*it)->COMGETTER(SessionPID)(&pid);
                        if (SUCCEEDED(hrc))
                        {
                            HANDLE ph = OpenProcess(SYNCHRONIZE, FALSE, pid);
                            AssertMsg(ph != NULL, ("OpenProcess (pid=%d) failed with %d\n",
                                                   pid, GetLastError()));
                            if (ph != NULL)
                            {
                                spawnedMachines.push_back(*it);
                                handles[1 + cnt + cntSpawned] = ph;
                                ++cntSpawned;
                            }
                        }
                    }
                }

                LogFlowFunc(("UPDATE: spawned session count = %d\n", cntSpawned));

                // machines lock unwinds here
            }
        }
        while (true);
    }
    while (0);

    /* close old process handles */
    for (size_t i = 1 + cnt; i < 1 + cnt + cntSpawned; ++i)
        CloseHandle(handles[i]);

    /* release sets of machines if any */
    machines.clear();
    spawnedMachines.clear();

    ::CoUninitialize();

#elif defined(RT_OS_OS2)

    /// @todo (dmik) processes reaping!

    /* according to PMREF, 64 is the maximum for the muxwait list */
    SEMRECORD handles[64];

    HMUX muxSem = NULLHANDLE;

    do
    {
        AutoCaller autoCaller(that->mVirtualBox);
        /* VirtualBox has been early uninitialized, terminate */
        if (!autoCaller.isOk())
            break;

        do
        {
            /* release the caller to let uninit() ever proceed */
            autoCaller.release();

            int vrc = RTSemEventWait(that->mUpdateReq, 500);

            /* Restore the caller before using VirtualBox. If it fails, this
             * means VirtualBox is being uninitialized and we must terminate. */
            autoCaller.add();
            if (!autoCaller.isOk())
                break;

            bool update = false;
            bool updateSpawned = false;

            if (RT_SUCCESS(vrc))
            {
                /* update event is signaled */
                update = true;
                updateSpawned = true;
            }
            else
            {
                AssertMsg(vrc == VERR_TIMEOUT || vrc == VERR_INTERRUPTED,
                          ("RTSemEventWait returned %Rrc\n", vrc));

                /* are there any mutexes? */
                if (cnt > 0)
                {
                    /* figure out what's going on with machines */

                    unsigned long semId = 0;
                    APIRET arc = ::DosWaitMuxWaitSem(muxSem,
                                                     SEM_IMMEDIATE_RETURN, &semId);

                    if (arc == NO_ERROR)
                    {
                        /* machine mutex is normally released */
                        Assert(semId >= 0 && semId < cnt);
                        if (semId >= 0 && semId < cnt)
                        {
#if 0//def DEBUG
                            {
                                AutoReadLock machineLock(machines[semId] COMMA_LOCKVAL_SRC_POS);
                                LogFlowFunc(("released mutex: machine='%ls'\n",
                                             machines[semId]->name().raw()));
                            }
#endif
                            machines[semId]->checkForDeath();
                        }
                        update = true;
                    }
                    else if (arc == ERROR_SEM_OWNER_DIED)
                    {
                        /* machine mutex is abandoned due to client process
                         * termination; find which mutex is in the Owner Died
                         * state */
                        for (size_t i = 0; i < cnt; ++i)
                        {
                            PID pid; TID tid;
                            unsigned long reqCnt;
                            arc = DosQueryMutexSem((HMTX)handles[i].hsemCur, &pid, &tid, &reqCnt);
                            if (arc == ERROR_SEM_OWNER_DIED)
                            {
                                /* close the dead mutex as asked by PMREF */
                                ::DosCloseMutexSem((HMTX)handles[i].hsemCur);

                                Assert(i >= 0 && i < cnt);
                                if (i >= 0 && i < cnt)
                                {
#if 0//def DEBUG
                                    {
                                        AutoReadLock machineLock(machines[semId] COMMA_LOCKVAL_SRC_POS);
                                        LogFlowFunc(("mutex owner dead: machine='%ls'\n",
                                                     machines[i]->name().raw()));
                                    }
#endif
                                    machines[i]->checkForDeath();
                                }
                            }
                        }
                        update = true;
                    }
                    else
                        AssertMsg(arc == ERROR_INTERRUPT || arc == ERROR_TIMEOUT,
                                  ("DosWaitMuxWaitSem returned %d\n", arc));
                }

                /* are there any spawning sessions? */
                if (cntSpawned > 0)
                {
                    for (size_t i = 0; i < cntSpawned; ++i)
                        updateSpawned |= (spawnedMachines[i])->
                            checkForSpawnFailure();
                }
            }

            if (update || updateSpawned)
            {
                // get reference to the machines list in VirtualBox
                VirtualBox::MachinesOList &allMachines = that->mVirtualBox->getMachinesList();

                // lock the machines list for reading
                AutoReadLock thatLock(allMachines.getLockHandle() COMMA_LOCKVAL_SRC_POS);

                if (update)
                {
                    /* close the old muxsem */
                    if (muxSem != NULLHANDLE)
                        ::DosCloseMuxWaitSem(muxSem);

                    /* obtain a new set of opened machines */
                    cnt = 0;
                    machines.clear();

                    for (MachinesOList::iterator it = allMachines.begin();
                         it != allMachines.end(); ++it)
                    {
                        /// @todo handle situations with more than 64 objects
                        AssertMsg(cnt <= 64 /* according to PMREF */,
                                  ("maximum of 64 mutex semaphores reached (%d)",
                                   cnt));

                        ComObjPtr<SessionMachine> sm;
                        if ((*it)->isSessionOpenOrClosing(sm))
                        {
                            AutoCaller smCaller(sm);
                            if (smCaller.isOk())
                            {
                                AutoReadLock smLock(sm COMMA_LOCKVAL_SRC_POS);
                                ClientToken *ct = sm->getClientToken();
                                if (ct)
                                {
                                    HMTX ipcSem = ct->getToken();
                                    machines.push_back(sm);
                                    handles[cnt].hsemCur = (HSEM)ipcSem;
                                    handles[cnt].ulUser = cnt;
                                    ++cnt;
                                }
                            }
                        }
                    }

                    LogFlowFunc(("UPDATE: direct session count = %d\n", cnt));

                    if (cnt > 0)
                    {
                        /* create a new muxsem */
                        APIRET arc = ::DosCreateMuxWaitSem(NULL, &muxSem, cnt,
                                                           handles,
                                                           DCMW_WAIT_ANY);
                        AssertMsg(arc == NO_ERROR,
                                  ("DosCreateMuxWaitSem returned %d\n", arc));
                        NOREF(arc);
                    }
                }

                if (updateSpawned)
                {
                    /* obtain a new set of spawned machines */
                    spawnedMachines.clear();

                    for (MachinesOList::iterator it = allMachines.begin();
                         it != allMachines.end(); ++it)
                    {
                        if ((*it)->isSessionSpawning())
                            spawnedMachines.push_back(*it);
                    }

                    cntSpawned = spawnedMachines.size();
                    LogFlowFunc(("UPDATE: spawned session count = %d\n", cntSpawned));
                }
            }
        }
        while (true);
    }
    while (0);

    /* close the muxsem */
    if (muxSem != NULLHANDLE)
        ::DosCloseMuxWaitSem(muxSem);

    /* release sets of machines if any */
    machines.clear();
    spawnedMachines.clear();

#elif defined(VBOX_WITH_SYS_V_IPC_SESSION_WATCHER)

    bool update = false;
    bool updateSpawned = false;

    do
    {
        AutoCaller autoCaller(that->mVirtualBox);
        if (!autoCaller.isOk())
            break;

        do
        {
            /* release the caller to let uninit() ever proceed */
            autoCaller.release();

            /* determine wait timeout adaptively: after updating information
             * relevant to the client watcher, check a few times more
             * frequently. This ensures good reaction time when the signalling
             * has to be done a bit before the actual change for technical
             * reasons, and saves CPU cycles when no activities are expected. */
            RTMSINTERVAL cMillies;
            {
                uint8_t uOld, uNew;
                do
                {
                    uOld = ASMAtomicUoReadU8(&that->mUpdateAdaptCtr);
                    uNew = uOld ? uOld - 1 : uOld;
                } while (!ASMAtomicCmpXchgU8(&that->mUpdateAdaptCtr, uNew, uOld));
                Assert(uOld <= RT_ELEMENTS(s_aUpdateTimeoutSteps) - 1);
                cMillies = s_aUpdateTimeoutSteps[uOld];
            }

            int rc = RTSemEventWait(that->mUpdateReq, cMillies);

            /*
             *  Restore the caller before using VirtualBox. If it fails, this
             *  means VirtualBox is being uninitialized and we must terminate.
             */
            autoCaller.add();
            if (!autoCaller.isOk())
                break;

            if (RT_SUCCESS(rc) || update || updateSpawned)
            {
                /* RT_SUCCESS(rc) means an update event is signaled */

                // get reference to the machines list in VirtualBox
                VirtualBox::MachinesOList &allMachines = that->mVirtualBox->getMachinesList();

                // lock the machines list for reading
                AutoReadLock thatLock(allMachines.getLockHandle() COMMA_LOCKVAL_SRC_POS);

                if (RT_SUCCESS(rc) || update)
                {
                    /* obtain a new set of opened machines */
                    machines.clear();

                    for (MachinesOList::iterator it = allMachines.begin();
                         it != allMachines.end();
                         ++it)
                    {
                        ComObjPtr<SessionMachine> sm;
                        if ((*it)->isSessionOpenOrClosing(sm))
                            machines.push_back(sm);
                    }

                    cnt = machines.size();
                    LogFlowFunc(("UPDATE: direct session count = %d\n", cnt));
                }

                if (RT_SUCCESS(rc) || updateSpawned)
                {
                    /* obtain a new set of spawned machines */
                    spawnedMachines.clear();

                    for (MachinesOList::iterator it = allMachines.begin();
                         it != allMachines.end();
                         ++it)
                    {
                        if ((*it)->isSessionSpawning())
                            spawnedMachines.push_back(*it);
                    }

                    cntSpawned = spawnedMachines.size();
                    LogFlowFunc(("UPDATE: spawned session count = %d\n", cntSpawned));
                }

                // machines lock unwinds here
            }

            update = false;
            for (size_t i = 0; i < cnt; ++i)
                update |= (machines[i])->checkForDeath();

            updateSpawned = false;
            for (size_t i = 0; i < cntSpawned; ++i)
                updateSpawned |= (spawnedMachines[i])->checkForSpawnFailure();

            /* reap child processes */
            {
                AutoWriteLock alock(that->mLock COMMA_LOCKVAL_SRC_POS);
                if (that->mProcesses.size())
                {
                    LogFlowFunc(("UPDATE: child process count = %d\n",
                                 that->mProcesses.size()));
                    VirtualBox::ClientWatcher::ProcessList::iterator it = that->mProcesses.begin();
                    while (it != that->mProcesses.end())
                    {
                        RTPROCESS pid = *it;
                        RTPROCSTATUS status;
                        int vrc = ::RTProcWait(pid, RTPROCWAIT_FLAGS_NOBLOCK, &status);
                        if (vrc == VINF_SUCCESS)
                        {
                            if (   status.enmReason != RTPROCEXITREASON_NORMAL
                                || status.iStatus   != RTEXITCODE_SUCCESS)
                            {
                                switch (status.enmReason)
                                {
                                    default:
                                    case RTPROCEXITREASON_NORMAL:
                                        LogRel(("Reaper: Pid %d (%x) exited normally: %d (%#x)\n",
                                                pid, pid, status.iStatus, status.iStatus));
                                        break;
                                    case RTPROCEXITREASON_ABEND:
                                        LogRel(("Reaper: Pid %d (%x) abended: %d (%#x)\n",
                                                pid, pid, status.iStatus, status.iStatus));
                                        break;
                                    case RTPROCEXITREASON_SIGNAL:
                                        LogRel(("Reaper: Pid %d (%x) was signalled: %d (%#x)\n",
                                                pid, pid, status.iStatus, status.iStatus));
                                        break;
                                }
                            }
                            else
                                LogFlowFunc(("pid %d (%x) was reaped, status=%d, reason=%d\n",
                                             pid, pid, status.iStatus,
                                             status.enmReason));
                            it = that->mProcesses.erase(it);
                        }
                        else
                        {
                            LogFlowFunc(("pid %d (%x) was NOT reaped, vrc=%Rrc\n",
                                         pid, pid, vrc));
                            if (vrc != VERR_PROCESS_RUNNING)
                            {
                                /* remove the process if it is not already running */
                                it = that->mProcesses.erase(it);
                            }
                            else
                                ++it;
                        }
                    }
                }
            }
        }
        while (true);
    }
    while (0);

    /* release sets of machines if any */
    machines.clear();
    spawnedMachines.clear();

#elif defined(VBOX_WITH_GENERIC_SESSION_WATCHER)

    bool updateSpawned = false;

    do
    {
        AutoCaller autoCaller(that->mVirtualBox);
        if (!autoCaller.isOk())
            break;

        do
        {
            /* release the caller to let uninit() ever proceed */
            autoCaller.release();

            /* determine wait timeout adaptively: after updating information
             * relevant to the client watcher, check a few times more
             * frequently. This ensures good reaction time when the signalling
             * has to be done a bit before the actual change for technical
             * reasons, and saves CPU cycles when no activities are expected. */
            RTMSINTERVAL cMillies;
            {
                uint8_t uOld, uNew;
                do
                {
                    uOld = ASMAtomicUoReadU8(&that->mUpdateAdaptCtr);
                    uNew = uOld ? uOld - 1 : uOld;
                } while (!ASMAtomicCmpXchgU8(&that->mUpdateAdaptCtr, uNew, uOld));
                Assert(uOld <= RT_ELEMENTS(s_aUpdateTimeoutSteps) - 1);
                cMillies = s_aUpdateTimeoutSteps[uOld];
            }

            int rc = RTSemEventWait(that->mUpdateReq, cMillies);

            /*
             *  Restore the caller before using VirtualBox. If it fails, this
             *  means VirtualBox is being uninitialized and we must terminate.
             */
            autoCaller.add();
            if (!autoCaller.isOk())
                break;

            /** @todo this quite big effort for catching machines in spawning
             * state which can't be caught by the token mechanism (as the token
             * can't be in the other process yet) could be eliminated if the
             * reaping is made smarter, having cross-reference information
             * from the pid to the corresponding machine object. Both cases do
             * more or less the same thing anyway. */
            if (RT_SUCCESS(rc) || updateSpawned)
            {
                /* RT_SUCCESS(rc) means an update event is signaled */

                // get reference to the machines list in VirtualBox
                VirtualBox::MachinesOList &allMachines = that->mVirtualBox->getMachinesList();

                // lock the machines list for reading
                AutoReadLock thatLock(allMachines.getLockHandle() COMMA_LOCKVAL_SRC_POS);

                if (RT_SUCCESS(rc) || updateSpawned)
                {
                    /* obtain a new set of spawned machines */
                    spawnedMachines.clear();

                    for (MachinesOList::iterator it = allMachines.begin();
                         it != allMachines.end();
                         ++it)
                    {
                        if ((*it)->isSessionSpawning())
                            spawnedMachines.push_back(*it);
                    }

                    cntSpawned = spawnedMachines.size();
                    LogFlowFunc(("UPDATE: spawned session count = %d\n", cntSpawned));
                }

                NOREF(cnt);
                // machines lock unwinds here
            }

            updateSpawned = false;
            for (size_t i = 0; i < cntSpawned; ++i)
                updateSpawned |= (spawnedMachines[i])->checkForSpawnFailure();

            /* reap child processes */
            {
                AutoWriteLock alock(that->mLock COMMA_LOCKVAL_SRC_POS);
                if (that->mProcesses.size())
                {
                    LogFlowFunc(("UPDATE: child process count = %d\n",
                                 that->mProcesses.size()));
                    VirtualBox::ClientWatcher::ProcessList::iterator it = that->mProcesses.begin();
                    while (it != that->mProcesses.end())
                    {
                        RTPROCESS pid = *it;
                        RTPROCSTATUS status;
                        int vrc = ::RTProcWait(pid, RTPROCWAIT_FLAGS_NOBLOCK, &status);
                        if (vrc == VINF_SUCCESS)
                        {
                            if (   status.enmReason != RTPROCEXITREASON_NORMAL
                                || status.iStatus   != RTEXITCODE_SUCCESS)
                            {
                                switch (status.enmReason)
                                {
                                    default:
                                    case RTPROCEXITREASON_NORMAL:
                                        LogRel(("Reaper: Pid %d (%x) exited normally: %d (%#x)\n",
                                                pid, pid, status.iStatus, status.iStatus));
                                        break;
                                    case RTPROCEXITREASON_ABEND:
                                        LogRel(("Reaper: Pid %d (%x) abended: %d (%#x)\n",
                                                pid, pid, status.iStatus, status.iStatus));
                                        break;
                                    case RTPROCEXITREASON_SIGNAL:
                                        LogRel(("Reaper: Pid %d (%x) was signalled: %d (%#x)\n",
                                                pid, pid, status.iStatus, status.iStatus));
                                        break;
                                }
                            }
                            else
                                LogFlowFunc(("pid %d (%x) was reaped, status=%d, reason=%d\n",
                                             pid, pid, status.iStatus,
                                             status.enmReason));
                            it = that->mProcesses.erase(it);
                        }
                        else
                        {
                            LogFlowFunc(("pid %d (%x) was NOT reaped, vrc=%Rrc\n",
                                         pid, pid, vrc));
                            if (vrc != VERR_PROCESS_RUNNING)
                            {
                                /* remove the process if it is not already running */
                                it = that->mProcesses.erase(it);
                            }
                            else
                                ++it;
                        }
                    }
                }
            }
        }
        while (true);
    }
    while (0);

    /* release sets of machines if any */
    machines.clear();
    spawnedMachines.clear();

#else
# error "Port me!"
#endif

    VirtualBoxBase::uninitializeComForThread();

    LogFlowFuncLeave();
    return 0;
}
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
