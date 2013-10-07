/** @file
 * VBox Qt GUI - UIThreadPool and UITask class declaration.
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

#ifndef ___UIThreadPool_h___
#define ___UIThreadPool_h___

/* Qt includes: */
#include <QObject>
#include <QMutex>
#include <QQueue>
#include <QVariant>
#include <QVector>
#include <QWaitCondition>

/* Forward declarations: */
class UIThreadWorker;
class UITask;

/* Worker-thread pool prototype.
 * Schedules COM-related GUI tasks to multiple worker-threads. */
class UIThreadPool : public QObject
{
    Q_OBJECT;

signals:

    /* Notifier: Task-queue stuff: */
    void sigTaskComplete(UITask *pTask);

public:

    /* Constructor/destructor: */
    UIThreadPool(ulong cMaxWorkers = 3, ulong cMsWorkerIdleTimeout = 5000);
    ~UIThreadPool();

    /* API: Task-queue stuff: */
    void enqueueTask(UITask *pTask);

    /* API: Termination stuff: */
    bool isTerminating() const;
    void setTerminating();

protected:

    /* Protected API: Worker-thread stuff: */
    UITask* dequeueTask(UIThreadWorker *pWorker);

private slots:

    /* Handler: Task-queue stuff: */
    void sltHandleTaskComplete(UITask *pTask);

    /* Handler: Worker stuff: */
    void sltHandleWorkerFinished(UIThreadWorker *pWorker);

private:

    /** @name Worker thread related variables.
     * @{ */
    QVector<UIThreadWorker*> m_workers;
    /** Milliseconds  */
    const ulong m_cMsIdleTimeout;
    /** The number of worker threads.
     * @remarks We cannot use the vector size since it may contain NULL pointers. */
    int m_cWorkers;
    /** The number of idle worker threads. */
    int m_cIdleWorkers;
    /** Set by the destructor to indicate that all worker threads should
     *  terminate ASAP. */
    bool m_fTerminating;
    /** @} */

    /** @name Task related variables
     * @{ */
    /** Queue (FIFO) of pending tasks. */
    QQueue<UITask*> m_tasks;
    /** Condition variable that gets signalled when queuing a new task and there are
     * idle worker threads around.
     *
     * Idle threads sits in dequeueTask waiting for this. Thus on thermination
     * setTerminating() will do a broadcast signal to wake up all workers (after
     * setting m_fTerminating of course). */
    QWaitCondition m_taskCondition;
    /** @} */


    /** This mutex is used with the m_taskCondition condition and protects m_tasks,
     *  m_fTerminating and m_workers. */
    mutable QMutex m_everythingLocker;

    /* Friends: */
    friend class UIThreadWorker;
};

/* GUI task interface.
 * Describes executable task to be used with UIThreadPool object. */
class UITask : public QObject
{
    Q_OBJECT;

signals:

    /* Notifier: Task stuff: */
    void sigComplete(UITask *pTask);

public:

    /* Constructor: */
    UITask(const QVariant &data);

    /* API: Data stuff: */
    const QVariant& data() const;

    /* API: Run stuff: */
    void start();

protected:

    /* Helper: Run stuff: */
    virtual void run() = 0;

    /* Variable: Data stuff: */
    QVariant m_data;
};

#endif /* !___UIThreadPool_h___ */

