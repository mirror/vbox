/* $Id$ */
/** @file
 * VBox Qt GUI - UIExecutionQueue class declaration.
 */

/*
 * Copyright (C) 2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef FEQT_INCLUDED_SRC_objects_UIExecutionQueue_h
#define FEQT_INCLUDED_SRC_objects_UIExecutionQueue_h

/* Qt includes: */
#include <QObject>
#include <QQueue>

/** QObject subclass providing GUI with
  * interface for an execution step. */
class UIExecutionStep : public QObject
{
    Q_OBJECT;

signals:

    /** Notifies about step finished. */
    void sigStepFinished();

public:

    /** Constructs execution step. */
    UIExecutionStep();

    /** Executes the step. */
    virtual void exec() = 0;
};

/** QObject subclass providing GUI with
  * an object to process queue of execution steps. */
class UIExecutionQueue : public QObject
{
    Q_OBJECT;

signals:

    /** Starts the queue. */
    void sigStartQueue();

    /** Notifies about queue finished. */
    void sigQueueFinished();

public:

    /** Constructs execution queue passing @a pParent to the base-class. */
    UIExecutionQueue(QObject *pParent = 0);
    /** Destructs execution queue. */
    virtual ~UIExecutionQueue() /* override final */;

    /** Enqueues pStep into queue. */
    void enqueue(UIExecutionStep *pStep);

    /** Starts the queue. */
    void start() { emit sigStartQueue(); }

private slots:

    /** Starts subsequent step. */
    void sltStartsSubsequentStep();

private:

    /** Holds the execution step queue. */
    QQueue<UIExecutionStep*>  m_queue;
    /** Holds current step being executed. */
    UIExecutionStep          *m_pExecutedStep;
};

#endif /* !FEQT_INCLUDED_SRC_objects_UIExecutionQueue_h */
