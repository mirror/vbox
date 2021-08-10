/* $Id$ */
/** @file
 * VBox Qt GUI - UINotificationObject class implementation.
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

/* GUI includes: */
#include "UINotificationObject.h"
#include "UINotificationProgressTask.h"
#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
# include "UIDownloader.h"
#endif


/*********************************************************************************************************************************
*   Class UINotificationObject implementation.                                                                                   *
*********************************************************************************************************************************/

UINotificationObject::UINotificationObject()
{
}

void UINotificationObject::close()
{
    emit sigAboutToClose();
}


/*********************************************************************************************************************************
*   Class UINotificationProgress implementation.                                                                                 *
*********************************************************************************************************************************/

UINotificationProgress::UINotificationProgress()
    : m_pTask(0)
    , m_uPercent(0)
{
}

UINotificationProgress::~UINotificationProgress()
{
    delete m_pTask;
    m_pTask = 0;
}

ulong UINotificationProgress::percent() const
{
    return m_uPercent;
}

bool UINotificationProgress::isCancelable() const
{
    return m_pTask ? m_pTask->isCancelable() : false;
}

QString UINotificationProgress::error() const
{
    return m_pTask ? m_pTask->errorMessage() : QString();
}

void UINotificationProgress::handle()
{
    /* Prepare task: */
    m_pTask = new UINotificationProgressTask(this);
    if (m_pTask)
    {
        connect(m_pTask, &UIProgressTask::sigProgressStarted,
                this, &UINotificationProgress::sigProgressStarted);
        connect(m_pTask, &UIProgressTask::sigProgressChange,
                this, &UINotificationProgress::sltHandleProgressChange);
        connect(m_pTask, &UIProgressTask::sigProgressFinished,
                this, &UINotificationProgress::sltHandleProgressFinished);

        /* And start it finally: */
        m_pTask->start();
    }
}

void UINotificationProgress::close()
{
    /* Cancel task: */
    if (m_pTask)
        m_pTask->cancel();
    /* Call to base-class: */
    UINotificationObject::close();
}

void UINotificationProgress::sltHandleProgressChange(ulong uPercent)
{
    m_uPercent = uPercent;
    emit sigProgressChange(uPercent);
}

void UINotificationProgress::sltHandleProgressFinished()
{
    m_uPercent = 100;
    emit sigProgressFinished();
}


#ifdef VBOX_GUI_WITH_NETWORK_MANAGER


/*********************************************************************************************************************************
*   Class UINotificationDownloader implementation.                                                                               *
*********************************************************************************************************************************/

UINotificationDownloader::UINotificationDownloader()
{
}

UINotificationDownloader::~UINotificationDownloader()
{
    delete m_pDownloader;
}

ulong UINotificationDownloader::percent() const
{
    return m_uPercent;
}

QString UINotificationDownloader::error() const
{
    return m_strError;
}

void UINotificationDownloader::handle()
{
    /* Prepare downloader: */
    m_pDownloader = createDownloader();
    if (m_pDownloader)
    {
        connect(m_pDownloader, &UIDownloader::sigToStartAcknowledging,
                this, &UINotificationDownloader::sigProgressStarted);
        connect(m_pDownloader, &UIDownloader::sigToStartDownloading,
                this, &UINotificationDownloader::sigProgressStarted);
        connect(m_pDownloader, &UIDownloader::sigToStartVerifying,
                this, &UINotificationDownloader::sigProgressStarted);
        connect(m_pDownloader, &UIDownloader::sigProgressChange,
                this, &UINotificationDownloader::sltHandleProgressChange);
        connect(m_pDownloader, &UIDownloader::sigProgressFailed,
                this, &UINotificationDownloader::sltHandleProgressFailed);
        connect(m_pDownloader, &UIDownloader::sigProgressCanceled,
                this, &UINotificationDownloader::sigProgressCanceled);
        connect(m_pDownloader, &UIDownloader::sigProgressFinished,
                this, &UINotificationDownloader::sigProgressFinished);
        connect(m_pDownloader, &UIDownloader::destroyed,
                this, &UINotificationDownloader::sigDownloaderDestroyed);

        /* And start it finally: */
        m_pDownloader->start();
    }
}

void UINotificationDownloader::close()
{
    /* Cancel downloader: */
    if (m_pDownloader)
        m_pDownloader->cancel();
    /* Call to base-class: */
    UINotificationObject::close();
}

void UINotificationDownloader::sltHandleProgressChange(ulong uPercent)
{
    m_uPercent = uPercent;
    emit sigProgressChange(uPercent);
}

void UINotificationDownloader::sltHandleProgressFailed(const QString &strError)
{
    m_strError = strError;
    emit sigProgressFailed();
}

#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */
