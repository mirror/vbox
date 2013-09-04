/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIThreadPool class declaration
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

#ifndef __UIThreadPool_h__
#define __UIThreadPool_h__

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
    UIThreadPool(ulong uWorkerCount = 3, ulong uWorkerIdleTimeout = 5000);
    ~UIThreadPool();

    /* API: Task-queue stuff: */
    void enqueueTask(UITask *pTask);

    /* API: Termination stuff: */
    bool isTerminating() const;
    void setTerminating(bool fTermintating);

protected:

    /* Protected API: Worker-thread stuff: */
    UITask* dequeueTask(UIThreadWorker *pWorker);

private slots:

    /* Handler: Task-queue stuff: */
    void sltHandleTaskComplete(UITask *pTask);

    /* Handler: Worker stuff: */
    void sltHandleWorkerFinished(UIThreadWorker *pWorker);

private:

    /* Helper: Worker stuff: */
    void cleanupWorker(int iWorkerIndex);

    /* Variables: Worker stuff: */
    QVector<UIThreadWorker*> m_workers;
    const ulong m_uIdleTimeout;

    /* Variables: Task stuff: */
    QQueue<UITask*> m_tasks;
    QMutex m_taskLocker;
    QWaitCondition m_taskCondition;

    /* Variables: Termination stuff: */
    bool m_fTerminating;
    mutable QMutex m_terminationLocker;

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

#endif /* __UIThreadPool_h__ */
