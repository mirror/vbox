/* $Id$ */
/** @file
 * VBox Qt GUI - UINotificationObject class implementation.
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
#include "UIExtraDataManager.h"
#include "UINotificationObject.h"
#include "UINotificationProgressTask.h"
#ifdef VBOX_GUI_WITH_NETWORK_MANAGER
# include "UIDownloader.h"
# include "UINewVersionChecker.h"
#endif


/*********************************************************************************************************************************
*   Class UINotificationObject implementation.                                                                                   *
*********************************************************************************************************************************/

UINotificationObject::UINotificationObject()
{
}

void UINotificationObject::dismiss()
{
    emit sigAboutToClose(true);
}

void UINotificationObject::close()
{
    emit sigAboutToClose(false);
}


/*********************************************************************************************************************************
*   Class UINotificationSimple implementation.                                                                                   *
*********************************************************************************************************************************/

UINotificationSimple::UINotificationSimple(const QString &strName,
                                           const QString &strDetails,
                                           const QString &strInternalName,
                                           const QString &strHelpKeyword,
                                           bool fCritical /* = true */)
    : m_strName(strName)
    , m_strDetails(strDetails)
    , m_strInternalName(strInternalName)
    , m_strHelpKeyword(strHelpKeyword)
    , m_fCritical(fCritical)
{
}

bool UINotificationSimple::isCritical() const
{
    return m_fCritical;
}

bool UINotificationSimple::isDone() const
{
    return true;
}

QString UINotificationSimple::name() const
{
    return m_strName;
}

QString UINotificationSimple::details() const
{
    return m_strDetails;
}

QString UINotificationSimple::internalName() const
{
    return m_strInternalName;
}

QString UINotificationSimple::helpKeyword() const
{
    return m_strHelpKeyword;
}

void UINotificationSimple::handle()
{
}

/* static */
bool UINotificationSimple::isSuppressed(const QString &strInternalName)
{
    /* Sanity check: */
    if (strInternalName.isEmpty())
        return false;

    /* Acquire and check suppressed message names: */
    const QStringList suppressedMessages = gEDataManager->suppressedMessages();
    return    suppressedMessages.contains(strInternalName)
           || suppressedMessages.contains("all");
}


/*********************************************************************************************************************************
*   Class UINotificationProgress implementation.                                                                                 *
*********************************************************************************************************************************/

UINotificationProgress::UINotificationProgress()
    : m_pTask(0)
    , m_uPercent(0)
    , m_fDone(false)
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

bool UINotificationProgress::isCritical() const
{
    return true;
}

bool UINotificationProgress::isDone() const
{
    return m_fDone;
}

QString UINotificationProgress::internalName() const
{
    return QString();
}

QString UINotificationProgress::helpKeyword() const
{
    return QString();
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
    m_fDone = true;
    emit sigProgressFinished();

    /* If there was no error and no reason to keep progress alive, - finish him! */
    if (   error().isEmpty()
        && !gEDataManager->keepSuccessfullNotificationProgresses())
        close();
}


#ifdef VBOX_GUI_WITH_NETWORK_MANAGER


/*********************************************************************************************************************************
*   Class UINotificationDownloader implementation.                                                                               *
*********************************************************************************************************************************/

UINotificationDownloader::UINotificationDownloader()
    : m_pDownloader(0)
    , m_uPercent(0)
    , m_fDone(false)
{
}

UINotificationDownloader::~UINotificationDownloader()
{
    delete m_pDownloader;
    m_pDownloader = 0;
}

ulong UINotificationDownloader::percent() const
{
    return m_uPercent;
}

QString UINotificationDownloader::error() const
{
    return m_strError;
}

bool UINotificationDownloader::isCritical() const
{
    return true;
}

bool UINotificationDownloader::isDone() const
{
    return m_fDone;
}

QString UINotificationDownloader::internalName() const
{
    return QString();
}

QString UINotificationDownloader::helpKeyword() const
{
    return QString();
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
                this, &UINotificationDownloader::sltHandleProgressCanceled);
        connect(m_pDownloader, &UIDownloader::sigProgressFinished,
                this, &UINotificationDownloader::sltHandleProgressFinished);

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
    delete m_pDownloader;
    m_pDownloader = 0;
    m_strError = strError;
    m_fDone = true;
    emit sigProgressFailed();
}

void UINotificationDownloader::sltHandleProgressCanceled()
{
    delete m_pDownloader;
    m_pDownloader = 0;
    m_fDone = true;
    emit sigProgressCanceled();
}

void UINotificationDownloader::sltHandleProgressFinished()
{
    delete m_pDownloader;
    m_pDownloader = 0;
    m_fDone = true;
    emit sigProgressFinished();
}


/*********************************************************************************************************************************
*   Class UINotificationNewVersionChecker implementation.                                                                        *
*********************************************************************************************************************************/

UINotificationNewVersionChecker::UINotificationNewVersionChecker()
    : m_pChecker(0)
    , m_fDone(false)
{
}

UINotificationNewVersionChecker::~UINotificationNewVersionChecker()
{
    delete m_pChecker;
    m_pChecker = 0;
}

QString UINotificationNewVersionChecker::error() const
{
    return m_strError;
}

bool UINotificationNewVersionChecker::isCritical() const
{
    return m_pChecker ? m_pChecker->isItForcedCall() : true;
}

bool UINotificationNewVersionChecker::isDone() const
{
    return m_fDone;
}

QString UINotificationNewVersionChecker::internalName() const
{
    return QString();
}

QString UINotificationNewVersionChecker::helpKeyword() const
{
    return QString();
}

void UINotificationNewVersionChecker::handle()
{
    /* Prepare new-version-checker: */
    m_pChecker = createChecker();
    if (m_pChecker)
    {
        connect(m_pChecker, &UINewVersionChecker::sigProgressFailed,
                this, &UINotificationNewVersionChecker::sltHandleProgressFailed);
        connect(m_pChecker, &UINewVersionChecker::sigProgressCanceled,
                this, &UINotificationNewVersionChecker::sltHandleProgressCanceled);
        connect(m_pChecker, &UINewVersionChecker::sigProgressFinished,
                this, &UINotificationNewVersionChecker::sltHandleProgressFinished);

        /* And start it finally: */
        m_pChecker->start();
    }
}

void UINotificationNewVersionChecker::close()
{
    /* Cancel new-version-checker: */
    if (m_pChecker)
        m_pChecker->cancel();
    /* Call to base-class: */
    UINotificationObject::close();
}

void UINotificationNewVersionChecker::sltHandleProgressFailed(const QString &strError)
{
    delete m_pChecker;
    m_pChecker = 0;
    m_strError = strError;
    emit sigProgressFailed();
}

void UINotificationNewVersionChecker::sltHandleProgressCanceled()
{
    delete m_pChecker;
    m_pChecker = 0;
    emit sigProgressCanceled();
}

void UINotificationNewVersionChecker::sltHandleProgressFinished()
{
    delete m_pChecker;
    m_pChecker = 0;
    m_fDone = true;
    emit sigProgressFinished();

    /* If there was no error and no reason to keep progress alive, - finish him! */
    if (   error().isEmpty()
        && !gEDataManager->keepSuccessfullNotificationProgresses())
        close();
}

#endif /* VBOX_GUI_WITH_NETWORK_MANAGER */
