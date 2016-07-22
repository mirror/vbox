/* $Id$ */
/** @file
 * VBox Qt GUI - UIProgressDialog class implementation.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
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
# include <QCloseEvent>
# include <QEventLoop>
# include <QProgressBar>
# include <QTime>
# include <QTimer>
# include <QVBoxLayout>

/* GUI includes: */
# include "UIProgressDialog.h"
# include "UIMessageCenter.h"
# include "UISpecialControls.h"
# include "UIModalWindowManager.h"
# include "QIDialogButtonBox.h"
# include "QILabel.h"
# include "VBoxGlobal.h"
# ifdef VBOX_WS_MAC
#  include "VBoxUtils-darwin.h"
# endif /* VBOX_WS_MAC */

/* COM includes: */
# include "CProgress.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


const char *UIProgressDialog::m_spcszOpDescTpl = "%1 ... (%2/%3)";

UIProgressDialog::UIProgressDialog(CProgress &progress,
                                   const QString &strTitle,
                                   QPixmap *pImage /* = 0 */,
                                   int cMinDuration /* = 2000 */,
                                   QWidget *pParent /* = 0 */)
    : QIWithRetranslateUI2<QIDialog>(pParent, Qt::MSWindowsFixedSizeDialogHint | Qt::WindowTitleHint)
    , m_progress(progress)
    , m_pImageLbl(0)
    , m_fCancelEnabled(false)
    , m_cOperations(m_progress.GetOperationCount())
    , m_iCurrentOperation(m_progress.GetOperation() + 1)
    , m_fEnded(false)
{
    /* Setup dialog: */
    setWindowTitle(QString("%1: %2").arg(strTitle, m_progress.GetDescription()));
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    /* Create main layout: */
    QHBoxLayout *pMainLayout = new QHBoxLayout(this);

#ifdef VBOX_WS_MAC
    ::darwinSetHidesAllTitleButtons(this);
    if (pImage)
        pMainLayout->setContentsMargins(30, 15, 30, 15);
    else
        pMainLayout->setContentsMargins(6, 6, 6, 6);
#endif /* VBOX_WS_MAC */

    /* Create image: */
    if (pImage)
    {
        m_pImageLbl = new QLabel(this);
        m_pImageLbl->setPixmap(*pImage);
        pMainLayout->addWidget(m_pImageLbl);
    }

    /* Create description: */
    m_pDescriptionLbl = new QILabel(this);
    if (m_cOperations > 1)
        m_pDescriptionLbl->setText(QString(m_spcszOpDescTpl)
                                   .arg(m_progress.GetOperationDescription())
                                   .arg(m_iCurrentOperation).arg(m_cOperations));
    else
        m_pDescriptionLbl->setText(QString("%1 ...")
                                   .arg(m_progress.GetOperationDescription()));

    /* Create progress-bar: */
    m_pProgressBar = new QProgressBar(this);
    m_pProgressBar->setMaximum(100);
    m_pProgressBar->setValue(0);

    /* Create cancel button: */
    m_fCancelEnabled = m_progress.GetCancelable();
    m_pCancelBtn = new UIMiniCancelButton(this);
    m_pCancelBtn->setEnabled(m_fCancelEnabled);
    m_pCancelBtn->setFocusPolicy(Qt::ClickFocus);
    connect(m_pCancelBtn, SIGNAL(clicked()), this, SLOT(sltCancelOperation()));

    /* Create estimation label: */
    m_pEtaLbl = new QILabel(this);

    /* Create proggress layout: */
    QHBoxLayout *pProgressLayout = new QHBoxLayout;
    pProgressLayout->setMargin(0);
    pProgressLayout->addWidget(m_pProgressBar, 0, Qt::AlignVCenter);
    pProgressLayout->addWidget(m_pCancelBtn, 0, Qt::AlignVCenter);

    /* Create description layout: */
    QVBoxLayout *pDescriptionLayout = new QVBoxLayout;
    pDescriptionLayout->setMargin(0);
    pDescriptionLayout->addStretch(1);
    pDescriptionLayout->addWidget(m_pDescriptionLbl, 0, Qt::AlignHCenter);
    pDescriptionLayout->addLayout(pProgressLayout);
    pDescriptionLayout->addWidget(m_pEtaLbl, 0, Qt::AlignLeft | Qt::AlignVCenter);
    pDescriptionLayout->addStretch(1);
    pMainLayout->addLayout(pDescriptionLayout);

    /* Translate finally: */
    retranslateUi();

    /* The progress dialog will be shown automatically after
     * the duration is over if progress is not finished yet. */
    QTimer::singleShot(cMinDuration, this, SLOT(show()));
}

UIProgressDialog::~UIProgressDialog()
{
    /* Wait for CProgress to complete: */
    m_progress.WaitForCompletion(-1);
    /* Call the timer event handling delegate: */
    handleTimerEvent();
}

void UIProgressDialog::retranslateUi()
{
    m_strCancel = tr("Canceling...");
    m_pCancelBtn->setText(tr("&Cancel"));
    m_pCancelBtn->setToolTip(tr("Cancel the current operation"));
}

int UIProgressDialog::run(int cRefreshInterval)
{
    if (m_progress.isOk())
    {
        /* Start refresh timer */
        int id = startTimer(cRefreshInterval);

        /* Set busy cursor.
         * We don't do this on the Mac, cause regarding the design rules of
         * Apple there is no busy window behavior. A window should always be
         * responsive and it is in our case (We show the progress dialog bar). */
#ifndef VBOX_WS_MAC
        if (m_fCancelEnabled)
            QApplication::setOverrideCursor(QCursor(Qt::BusyCursor));
        else
            QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
#endif /* VBOX_WS_MAC */

        /* Create a local event-loop: */
        {
            /* Guard ourself for the case
             * we destroyed ourself in our event-loop: */
            QPointer<UIProgressDialog> guard = this;

            /* Holds the modal loop, but don't show the window immediately: */
            execute(false);

            /* Are we still valid? */
            if (guard.isNull())
                return Rejected;
        }

        /* Kill refresh timer */
        killTimer(id);

#ifndef VBOX_WS_MAC
        /* Reset the busy cursor */
        QApplication::restoreOverrideCursor();
#endif /* VBOX_WS_MAC */

        return result();
    }
    return Rejected;
}

void UIProgressDialog::show()
{
    /* We should not show progress-dialog
     * if it was already finalized but not yet closed.
     * This could happens in case of some other
     * modal dialog prevents our event-loop from
     * being exit overlapping 'this'. */
    if (!m_fEnded)
        QIDialog::show();
}

void UIProgressDialog::reject()
{
    if (m_fCancelEnabled)
        sltCancelOperation();
}

void UIProgressDialog::timerEvent(QTimerEvent*)
{
    /* Call the timer event handling delegate: */
    handleTimerEvent();
}

void UIProgressDialog::closeEvent(QCloseEvent *pEvent)
{
    if (m_fCancelEnabled)
        sltCancelOperation();
    else
        pEvent->ignore();
}

void UIProgressDialog::sltCancelOperation()
{
    m_pCancelBtn->setEnabled(false);
    m_progress.Cancel();
}

void UIProgressDialog::handleTimerEvent()
{
    /* We should hide progress-dialog
     * if it was already finalized but not yet closed.
     * This could happens in case of some other
     * modal dialog prevents our event-loop from
     * being exit overlapping 'this'. */
    if (m_fEnded && !isHidden() && windowManager().isWindowOnTheTopOfTheModalWindowStack(this))
    {
        hide();
        return;
    }
    else if (m_fEnded)
        return;

    if (!m_fEnded && (!m_progress.isOk() || m_progress.GetCompleted()))
    {
        /* Is this progress-dialog a top-level modal-dialog now? */
        if (windowManager().isWindowOnTheTopOfTheModalWindowStack(this))
        {
            /* Progress finished: */
            if (m_progress.isOk())
            {
                m_pProgressBar->setValue(100);
                done(Accepted);
            }
            /* Progress is not valid: */
            else
                done(Rejected);

            /* Request to exit loop: */
            m_fEnded = true;
            return;
        }
        /* Else we should wait until all the subsequent
         * top-level modal-dialog(s) will be dismissed: */
        return;
    }

    if (!m_progress.GetCanceled())
    {
        /* Update the progress dialog: */
        /* First ETA */
        long newTime = m_progress.GetTimeRemaining();
        long seconds;
        long minutes;
        long hours;
        long days;

        seconds  = newTime < 0 ? 0 : newTime;
        minutes  = seconds / 60;
        seconds -= minutes * 60;
        hours    = minutes / 60;
        minutes -= hours   * 60;
        days     = hours   / 24;
        hours   -= days    * 24;

        QString strDays = VBoxGlobal::daysToString(days);
        QString strHours = VBoxGlobal::hoursToString(hours);
        QString strMinutes = VBoxGlobal::minutesToString(minutes);
        QString strSeconds = VBoxGlobal::secondsToString(seconds);

        QString strTwoComp = tr("%1, %2 remaining", "You may wish to translate this more like \"Time remaining: %1, %2\"");
        QString strOneComp = tr("%1 remaining", "You may wish to translate this more like \"Time remaining: %1\"");

        if      (days > 1 && hours > 0)
            m_pEtaLbl->setText(strTwoComp.arg(strDays).arg(strHours));
        else if (days > 1)
            m_pEtaLbl->setText(strOneComp.arg(strDays));
        else if (days > 0 && hours > 0)
            m_pEtaLbl->setText(strTwoComp.arg(strDays).arg(strHours));
        else if (days > 0 && minutes > 5)
            m_pEtaLbl->setText(strTwoComp.arg(strDays).arg(strMinutes));
        else if (days > 0)
            m_pEtaLbl->setText(strOneComp.arg(strDays));
        else if (hours > 2)
            m_pEtaLbl->setText(strOneComp.arg(strHours));
        else if (hours > 0 && minutes > 0)
            m_pEtaLbl->setText(strTwoComp.arg(strHours).arg(strMinutes));
        else if (hours > 0)
            m_pEtaLbl->setText(strOneComp.arg(strHours));
        else if (minutes > 2)
            m_pEtaLbl->setText(strOneComp.arg(strMinutes));
        else if (minutes > 0 && seconds > 5)
            m_pEtaLbl->setText(strTwoComp.arg(strMinutes).arg(strSeconds));
        else if (minutes > 0)
            m_pEtaLbl->setText(strOneComp.arg(strMinutes));
        else if (seconds > 5)
            m_pEtaLbl->setText(strOneComp.arg(strSeconds));
        else if (seconds > 0)
            m_pEtaLbl->setText(tr("A few seconds remaining"));
        else
            m_pEtaLbl->clear();

        /* Then operation text if changed: */
        ulong newOp = m_progress.GetOperation() + 1;
        if (newOp != m_iCurrentOperation)
        {
            m_iCurrentOperation = newOp;
            m_pDescriptionLbl->setText(QString(m_spcszOpDescTpl)
                                       .arg(m_progress.GetOperationDescription())
                                       .arg(m_iCurrentOperation).arg(m_cOperations));
        }
        m_pProgressBar->setValue(m_progress.GetPercent());

        /* Then cancel button: */
        m_fCancelEnabled = m_progress.GetCancelable();
        m_pCancelBtn->setEnabled(m_fCancelEnabled);

        /* Notify listeners about the operation progress update: */
        emit sigProgressChange(m_cOperations, m_progress.GetOperationDescription(),
                               m_progress.GetOperation() + 1, m_progress.GetPercent());
    }
    else
        m_pEtaLbl->setText(m_strCancel);
}


UIProgress::UIProgress(CProgress &progress, QObject *pParent /* = 0 */)
    : QObject(pParent)
    , m_progress(progress)
    , m_cOperations(m_progress.GetOperationCount())
    , m_fEnded(false)
{
}

void UIProgress::run(int iRefreshInterval)
{
    /* Make sure the CProgress still valid: */
    if (!m_progress.isOk())
        return;

    /* Start the refresh timer: */
    int id = startTimer(iRefreshInterval);

    /* Create a local event-loop: */
    {
        QEventLoop eventLoop;
        m_pEventLoop = &eventLoop;

        /* Guard ourself for the case
         * we destroyed ourself in our event-loop: */
        QPointer<UIProgress> guard = this;

        /* Start the blocking event-loop: */
        eventLoop.exec();

        /* Are we still valid? */
        if (guard.isNull())
            return;

        m_pEventLoop = 0;
    }

    /* Kill the refresh timer: */
    killTimer(id);
}

void UIProgress::timerEvent(QTimerEvent*)
{
    /* Make sure the UIProgress still 'running': */
    if (m_fEnded)
        return;

    /* If progress had failed or finished: */
    if (!m_progress.isOk() || m_progress.GetCompleted())
    {
        /* Notify listeners about the operation progress error: */
        if (!m_progress.isOk() || m_progress.GetResultCode() != 0)
            emit sigProgressError(UIMessageCenter::formatErrorInfo(m_progress));

        /* Exit from the event-loop if there is any: */
        if (m_pEventLoop)
            m_pEventLoop->exit();

        /* Mark UIProgress as 'ended': */
        m_fEnded = true;

        /* Return early: */
        return;
    }

    /* If CProgress was not yet canceled: */
    if (!m_progress.GetCanceled())
    {
        /* Notify listeners about the operation progress update: */
        emit sigProgressChange(m_cOperations, m_progress.GetOperationDescription(),
                               m_progress.GetOperation() + 1, m_progress.GetPercent());
    }
}

