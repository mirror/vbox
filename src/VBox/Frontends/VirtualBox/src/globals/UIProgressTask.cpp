/* $Id$ */
/** @file
 * VBox Qt GUI - UIProgressTask class implementation.
 */

/*
 * Copyright (C) 2020 Oracle Corporation
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
#include <QTimer>

/* GUI includes: */
#include "UIProgressTask.h"


UIProgressTask::UIProgressTask(QObject *pParent)
    : QObject(pParent)
    , m_pTimer(0)
{
    prepare();
}

UIProgressTask::~UIProgressTask()
{
    cleanup();
}

bool UIProgressTask::isScheduled() const
{
    AssertPtrReturn(m_pTimer, false);
    return m_pTimer->isActive();
}

bool UIProgressTask::isRunning() const
{
    return m_pProgressObject;
}

void UIProgressTask::schedule(int iMsec)
{
    AssertPtrReturnVoid(m_pTimer);
    m_pTimer->setInterval(iMsec);
    m_pTimer->start();
}

void UIProgressTask::start()
{
    /* Ignore request if already running: */
    if (isRunning())
        return;

    /* Call for a virtual stuff to create progress-wrapper itself: */
    m_comProgress = createProgress();

    /* Prepare progress-object: */
    m_pProgressObject = new UIProgressObject(m_comProgress, this);
    if (m_pProgressObject)
    {
        /* Setup connections: */
        connect(m_pProgressObject.data(), &UIProgressObject::sigProgressEventHandlingFinished,
                this, &UIProgressTask::sltHandleProgressEventHandlingFinished);

        /* Notify external listeners: */
        emit sigProgressStarted();
    }
}

void UIProgressTask::cancel()
{
    if (m_pProgressObject)
        m_pProgressObject->cancel();
}

void UIProgressTask::sltHandleProgressEventHandlingFinished()
{
    /* Call for a virtual stuff to let sub-class handle result: */
    handleProgressFinished(m_comProgress);

    /* Cleanup progress-object and progress-wrapper: */
    delete m_pProgressObject;
    m_comProgress = CProgress();

    /* Notify external listeners: */
    emit sigProgressFinished();
}

void UIProgressTask::prepare()
{
    /* Prepare schedule-timer: */
    m_pTimer = new QTimer(this);
    if (m_pTimer)
    {
        m_pTimer->setSingleShot(true);
        connect(m_pTimer, &QTimer::timeout,
                this, &UIProgressTask::start);
    }
}

void UIProgressTask::cleanup()
{
    /* Cleanup progress-object and progress-wrapper: */
    delete m_pProgressObject;
    m_comProgress = CProgress();

    /* Cleanup schedule-timer: */
    delete m_pTimer;
    m_pTimer = 0;
}
