/* $Id$ */
/** @file
 * VBox Qt GUI - UIThreadPool and UITask class implementation.
 */

/*
 * Copyright (C) 2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QThread>

/* GUI includes: */
# include "COMDefs.h"
# include "UIThreadPool.h"
# include "UIDefs.h"

/* Other VBox includes: */
#include <iprt/assert.h>

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


/**
 * COM capable worker thread for the UIThreadPool.
 */
class UIThreadWorker : public QThread
{
    Q_OBJECT;

signals:

    /* Notifier: Worker stuff: */
    void sigFinished(UIThreadWorker *pWorker);

public:

    /* Constructor: */
    UIThreadWorker(UIThreadPool *pPool, int iIndex);

    int getIndex() const { return m_iIndex; }

    /** Disables sigFinished. For optimizing pool termination. */
    void setNoFinishedSignal()
    {
        m_fNoFinishedSignal = true;
    }

private:

    /* Helper: Worker stuff: */
    void run();

    /* Variables: General stuff: */
    UIThreadPool *m_pPool;

    /** The index into UIThreadPool::m_workers. */
    int m_iIndex;
    /** Indicates whether sigFinished should be emitted or not. */
    bool m_fNoFinishedSignal;
};


UIThreadPool::UIThreadPool(ulong cMaxWorkers /* = 3 */, ulong cMsWorkerIdleTimeout /* = 5000 */)
    : m_workers(cMaxWorkers)
    , m_cMsIdleTimeout(cMsWorkerIdleTimeout)
    , m_cWorkers(0)
    , m_cIdleWorkers(0)
    , m_fTerminating(false) /* termination status */
{
}

UIThreadPool::~UIThreadPool()
{
    /* Set termination status and alert all idle worker threads: */
    setTerminating();

    m_everythingLocker.lock(); /* paranoia */

    /* Cleanup all the workers: */
    for (int idxWorker = 0; idxWorker < m_workers.size(); ++idxWorker)
    {
        UIThreadWorker *pWorker = m_workers[idxWorker];
        m_workers[idxWorker] = NULL;

        /* Clean up the worker, if there was one. */
        if (pWorker)
        {
            m_cWorkers--;
            m_everythingLocker.unlock();

            pWorker->wait();

            m_everythingLocker.lock();
            delete pWorker;
        }
    }

    m_everythingLocker.unlock();
}

void UIThreadPool::enqueueTask(UITask *pTask)
{
    Assert(!isTerminating());

    /* Prepare task: */
    connect(pTask, SIGNAL(sigComplete(UITask*)), this, SLOT(sltHandleTaskComplete(UITask*)), Qt::QueuedConnection);

    m_everythingLocker.lock();

    /* Put the task onto the queue: */
    m_tasks.enqueue(pTask);

    /* Wake up an idle worker if we got one: */
    if (m_cIdleWorkers > 0)
    {
        m_taskCondition.wakeOne();
    }
    /* No idle worker threads, should we create a new one? */
    else if (m_cWorkers < m_workers.size())
    {
        /* Find free slot: */
        int idxFirstUnused = m_workers.size();
        while (idxFirstUnused-- > 0)
            if (m_workers[idxFirstUnused] == NULL)
            {
                /* Prepare the new worker: */
                UIThreadWorker *pWorker = new UIThreadWorker(this, idxFirstUnused);
                connect(pWorker, SIGNAL(sigFinished(UIThreadWorker*)), this,
                        SLOT(sltHandleWorkerFinished(UIThreadWorker*)), Qt::QueuedConnection);
                m_workers[idxFirstUnused] = pWorker;
                m_cWorkers++;

                /* And start it: */
                pWorker->start();
                break;
            }
    }
    /* else: wait for some worker to complete whatever it's busy with and jump to it. */

    m_everythingLocker.unlock();
}

/**
 * Checks if the thread pool is terminating.
 *
 * @returns @c true if terminating, @c false if not.
 * @note    Do NOT call this while owning the thread pool mutex!
 */
bool UIThreadPool::isTerminating() const
{
    /* Acquire termination-flag: */
    m_everythingLocker.lock();
    bool fTerminating = m_fTerminating;
    m_everythingLocker.unlock();

    return fTerminating;
}

void UIThreadPool::setTerminating()
{
    m_everythingLocker.lock();

    /* Indicate that we're terminating: */
    m_fTerminating = true;

    /* Tell all threads to NOT queue any termination signals: */
    for (int idxWorker = 0; idxWorker < m_workers.size(); ++idxWorker)
    {
        UIThreadWorker *pWorker = m_workers[idxWorker];
        if (pWorker)
            pWorker->setNoFinishedSignal();
    }

    /* Wake up all idle worker threads: */
    m_taskCondition.wakeAll();

    m_everythingLocker.unlock();
}

UITask* UIThreadPool::dequeueTask(UIThreadWorker *pWorker)
{
    /* Dequeue a task, watching out for terminations.
     * For opimal efficiency in enqueueTask() we keep count of idle threads.
     * If the wait times out, we'll return NULL and terminate the thread. */
    m_everythingLocker.lock();

    bool fIdleTimedOut = false;
    while (!m_fTerminating)
    {
        Assert(m_workers[pWorker->getIndex()] == pWorker); /* paranoia */

        /* Dequeue task if there is one: */
        if (!m_tasks.isEmpty())
        {
            UITask *pTask = m_tasks.dequeue();
            if (pTask)
            {
                m_everythingLocker.unlock();
                return pTask;
            }
        }

        /* If we timed out already, then quit the worker thread. To prevent a
           race between enqueueTask and the queue removal of the thread from
           the workers vector, we remove it here already. (This does not apply
           to the termination scenario.) */
        if (fIdleTimedOut)
        {
            m_workers[pWorker->getIndex()] = NULL;
            m_cWorkers--;
            break;
        }

        /* Wait for a task or timeout.*/
        m_cIdleWorkers++;
        fIdleTimedOut = !m_taskCondition.wait(&m_everythingLocker, m_cMsIdleTimeout);
        m_cIdleWorkers--;
    }

    m_everythingLocker.unlock();

    return NULL;
}

void UIThreadPool::sltHandleTaskComplete(UITask *pTask)
{
    /* Skip on termination: */
    if (isTerminating())
        return;

    /* Notify listener: */
    emit sigTaskComplete(pTask);
}

void UIThreadPool::sltHandleWorkerFinished(UIThreadWorker *pWorker)
{
    /* Wait for the thread to finish completely, then delete the thread
       object. We have already removed the thread from the workers vector.
       Note! We don't want to use 'this' here, in case it's invalid. */
    pWorker->wait();
    delete pWorker;
}


UITask::UITask(const QVariant &data)
    : m_data(data)
{
}

const QVariant& UITask::data() const
{
    return m_data;
}

void UITask::start()
{
    /* Run task: */
    run();
    /* Notify listener: */
    emit sigComplete(this);
}


UIThreadWorker::UIThreadWorker(UIThreadPool *pPool, int iIndex)
    : m_pPool(pPool)
    , m_iIndex(iIndex)
    , m_fNoFinishedSignal(false)
{
}

void UIThreadWorker::run()
{
    /* Initialize COM: */
    COMBase::InitializeCOM(false);

    /* Try get a task from the pool queue. */
    while (UITask *pTask = m_pPool->dequeueTask(this))
    {
        /* Process the task if we are not terminating.
         * Please take into account tasks are cleared by their creator. */
        if (!m_pPool->isTerminating())
            pTask->start();
    }

    /* Cleanup COM: */
    COMBase::CleanupCOM();

    /* Queue a signal to for the pool to do thread cleanup, unless the pool is
       already terminating and doesn't need the signal. */
    if (!m_fNoFinishedSignal)
        emit sigFinished(this);
}


#include "UIThreadPool.moc"

