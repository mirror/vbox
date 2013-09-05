/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIThreadPool class implementation
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

/* Qt includes: */
#include <QThread>

/* GUI includes: */
#include "COMDefs.h"
#include "UIThreadPool.h"

/* Other VBox defines: */
#define LOG_GROUP LOG_GROUP_GUI

/* Other VBox includes: */
#include <VBox/log.h>
#include <VBox/sup.h>

/* Worker-thread prototype.
 * Performs COM-related GUI tasks in separate thread. */
class UIThreadWorker : public QThread
{
    Q_OBJECT;

signals:

    /* Notifier: Worker stuff: */
    void sigFinished(UIThreadWorker *pWorker);

public:

    /* Constructor: */
    UIThreadWorker(UIThreadPool *pPool, int iIndex);

    /* API: Busy stuff: */
    bool isBusy() const;
    void setBusy(bool fBusy);

private:

    /* Helper: Worker stuff: */
    void run();

    /* Variables: General stuff: */
    UIThreadPool *m_pPool;
    int m_iIndex;

    /* Variables: Busy stuff: */
    bool m_fBusy;
    mutable QMutex m_busyLocker;
};


UIThreadPool::UIThreadPool(ulong uWorkerCount /*= 3*/, ulong uWorkerIdleTimeout /*= 5000*/)
    : m_workers(uWorkerCount /* maximum worker count */)
    , m_uIdleTimeout(uWorkerIdleTimeout) /* time for worker idle timeout */
    , m_fTerminating(false) /* termination status */
{
}

UIThreadPool::~UIThreadPool()
{
    /* Set termination status: */
    setTerminating(true);

    /* Cleanup all the workers: */
    m_taskCondition.wakeAll();
    for (int iWorkerIndex = 0; iWorkerIndex < m_workers.size(); ++iWorkerIndex)
        cleanupWorker(iWorkerIndex);
}

void UIThreadPool::enqueueTask(UITask *pTask)
{
    /* Prepare task: */
    connect(pTask, SIGNAL(sigComplete(UITask*)), this, SLOT(sltHandleTaskComplete(UITask*)), Qt::QueuedConnection);

    /* Post task into queue: */
    m_taskLocker.lock();
    m_tasks.enqueue(pTask);
    m_taskLocker.unlock();

    /* Search for the first 'not yet created' worker or wake idle if exists: */
    int iFirstNotYetCreatedWorkerIndex = -1;
    for (int iWorkerIndex = 0; iWorkerIndex < m_workers.size(); ++iWorkerIndex)
    {
        /* Remember first 'not yet created' worker: */
        if (iFirstNotYetCreatedWorkerIndex == -1 && !m_workers[iWorkerIndex])
            iFirstNotYetCreatedWorkerIndex = iWorkerIndex;
        /* But if worker 'not yet created' or 'busy' now, just skip it: */
        if (!m_workers[iWorkerIndex] || m_workers[iWorkerIndex]->isBusy())
            continue;
        /* If we found idle worker, wake it up: */
        m_taskCondition.wakeOne();
        return;
    }

    /* Should we create new worker? */
    if (iFirstNotYetCreatedWorkerIndex != -1)
    {
        /* Prepare worker: */
        UIThreadWorker *pWorker = new UIThreadWorker(this, iFirstNotYetCreatedWorkerIndex);
        connect(pWorker, SIGNAL(sigFinished(UIThreadWorker*)), this, SLOT(sltHandleWorkerFinished(UIThreadWorker*)), Qt::QueuedConnection);
        m_workers[iFirstNotYetCreatedWorkerIndex] = pWorker;
        /* And start it: */
        pWorker->start();
    }
}

bool UIThreadPool::isTerminating() const
{
    /* Acquire termination-flag: */
    bool fTerminating = false;
    {
        QMutexLocker lock(&m_terminationLocker);
        fTerminating = m_fTerminating;
        Q_UNUSED(lock);
    }
    return fTerminating;
}

void UIThreadPool::setTerminating(bool fTerminating)
{
    /* Assign termination-flag: */
    QMutexLocker lock(&m_terminationLocker);
    m_fTerminating = fTerminating;
    Q_UNUSED(lock);
}

UITask* UIThreadPool::dequeueTask(UIThreadWorker *pWorker)
{
    /* Prepare task: */
    UITask *pTask = 0;

    /* Lock task locker: */
    m_taskLocker.lock();

    /* Try to get task (moving it from queue to processing list): */
    if (!m_tasks.isEmpty())
        pTask = m_tasks.dequeue();

    /* If there is no task currently: */
    if (!pTask)
    {
        /* Mark thread as not busy: */
        pWorker->setBusy(false);

        /* Wait for <m_uIdleTimeout> milli-seconds for the next task,
         * this issue will temporary unlock <m_taskLocker>: */
        m_taskCondition.wait(&m_taskLocker, m_uIdleTimeout);

        /* Mark thread as busy again: */
        pWorker->setBusy(true);

        /* Try to get task again (moving it from queue to processing list): */
        if (!m_tasks.isEmpty())
            pTask = m_tasks.dequeue();
    }

    /* Unlock task locker: */
    m_taskLocker.unlock();

    /* Return task: */
    return pTask;
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
    /* Skip on termination: */
    if (isTerminating())
        return;

    /* Make sure that is one of our workers: */
    int iIndexOfWorker = m_workers.indexOf(pWorker);
    AssertReturnVoid(iIndexOfWorker != -1);

    /* Cleanup worker: */
    cleanupWorker(iIndexOfWorker);
}

void UIThreadPool::cleanupWorker(int iWorkerIndex)
{
    /* Cleanup worker if any: */
    if (!m_workers[iWorkerIndex])
        return;
    m_workers[iWorkerIndex]->wait();
    delete m_workers[iWorkerIndex];
    m_workers[iWorkerIndex] = 0;
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
    , m_fBusy(true)
{
}

bool UIThreadWorker::isBusy() const
{
    /* Acquire busy-flag: */
    bool fBusy = false;
    {
        QMutexLocker lock(&m_busyLocker);
        fBusy = m_fBusy;
        Q_UNUSED(lock);
    }
    return fBusy;
}

void UIThreadWorker::setBusy(bool fBusy)
{
    /* Assign busy-flag: */
    QMutexLocker lock(&m_busyLocker);
    m_fBusy = fBusy;
    Q_UNUSED(lock);
}

void UIThreadWorker::run()
{
//    LogRelFlow(("UIThreadWorker #%d: Started...\n", m_iIndex));

    /* Initialize COM: */
    COMBase::InitializeCOM(false);

    /* Try to get task from thread-pool queue: */
    while (UITask *pTask = m_pPool->dequeueTask(this))
    {
        /* Process task: */
//        LogRelFlow(("UIThreadWorker #%d: Task acquired...\n", m_iIndex));
        if (!m_pPool->isTerminating())
            pTask->start();
//        LogRelFlow(("UIThreadWorker #%d: Task processed!\n", m_iIndex));
    }

    /* Cleanup COM: */
    COMBase::CleanupCOM();

    /* Notify listener: */
    emit sigFinished(this);

//    LogRelFlow(("UIThreadWorker #%d: Finished!\n", m_iIndex));
}


#include "UIThreadPool.moc"

