/* $Id$ */
/** @file
 * VBox Qt GUI - UIExecutionQueue class implementation.
 */

/*
 * Copyright (C) 2021-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* GUI includes: */
#include "UIExecutionQueue.h"


/*********************************************************************************************************************************
*   Class UIExecutionStep implementation.                                                                                        *
*********************************************************************************************************************************/

UIExecutionStep::UIExecutionStep()
{
}


/*********************************************************************************************************************************
*   Class UIExecutionQueue implementation.                                                                                       *
*********************************************************************************************************************************/

UIExecutionQueue::UIExecutionQueue(QObject *pParent /* = 0 */)
    : QObject(pParent)
    , m_pExecutedStep(0)
{
    /* Listen for the queue start signal: */
    connect(this, &UIExecutionQueue::sigStartQueue,
            this, &UIExecutionQueue::sltStartsSubsequentStep,
            Qt::QueuedConnection);
}

UIExecutionQueue::~UIExecutionQueue()
{
    /* Cleanup current step: */
    delete m_pExecutedStep;
    m_pExecutedStep = 0;

    /* Dequeue steps one-by-one: */
    while (!m_queue.isEmpty())
        delete m_queue.dequeue();
}

void UIExecutionQueue::enqueue(UIExecutionStep *pUpdateStep)
{
    m_queue.enqueue(pUpdateStep);
}

void UIExecutionQueue::sltStartsSubsequentStep()
{
    /* Cleanup current step: */
    delete m_pExecutedStep;
    m_pExecutedStep = 0;

    /* If queue is empty, we are finished: */
    if (m_queue.isEmpty())
        emit sigQueueFinished();
    else
    {
        /* Otherwise dequeue first step and start it: */
        m_pExecutedStep = m_queue.dequeue();
        connect(m_pExecutedStep, &UIExecutionStep::sigStepFinished,
                this, &UIExecutionQueue::sltStartsSubsequentStep,
                Qt::QueuedConnection);
        m_pExecutedStep->exec();
    }
}
