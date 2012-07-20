/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIProgressDialog class implementation
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
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
#include <QCloseEvent>
#include <QEventLoop>
#include <QProgressBar>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>

/* GUI includes: */
#include "UIProgressDialog.h"
#include "QIDialogButtonBox.h"
#include "QILabel.h"
#include "UISpecialControls.h"
#include "VBoxGlobal.h"
#ifdef Q_WS_MAC
# include "VBoxUtils-darwin.h"
#endif /* Q_WS_MAC */

/* COM includes: */
#include "CProgress.h"

const char *UIProgressDialog::m_spcszOpDescTpl = "%1 ... (%2/%3)";

UIProgressDialog::UIProgressDialog(CProgress &progress,
                                   const QString &strTitle,
                                   QPixmap *pImage /* = 0 */,
                                   bool fSheetOnDarwin /* = false */,
                                   int cMinDuration /* = 2000 */,
                                   QWidget *pParent /* = 0 */)
  : QIDialog(pParent, Qt::MSWindowsFixedSizeDialogHint | Qt::WindowTitleHint)
  , m_progress(progress)
  , m_pImageLbl(0)
  , m_pCancelBtn(0)
  , m_fCancelEnabled(false)
  , m_cOperations(m_progress.GetOperationCount())
  , m_iCurrentOperation(m_progress.GetOperation() + 1)
  , m_fEnded(false)
{
    setModal(true);

    QHBoxLayout *pLayout0 = new QHBoxLayout(this);

#ifdef Q_WS_MAC
    /* No sheets in another mode than normal for now. Firstly it looks ugly and
     * secondly in some cases it is broken. */
    if (   fSheetOnDarwin
        && vboxGlobal().isSheetWindowsAllowed(pParent))
        setWindowFlags(Qt::Sheet);
    ::darwinSetHidesAllTitleButtons(this);
    ::darwinSetShowsResizeIndicator(this, false);
    if (pImage)
        pLayout0->setContentsMargins(30, 15, 30, 15);
    else
        pLayout0->setContentsMargins(6, 6, 6, 6);
#else
    NOREF(fSheetOnDarwin);
#endif /* Q_WS_MAC */

    if (pImage)
    {
        m_pImageLbl = new QILabel(this);
        m_pImageLbl->setPixmap(*pImage);
        pLayout0->addWidget(m_pImageLbl);
    }

    QVBoxLayout *pLayout1 = new QVBoxLayout();
    pLayout1->setMargin(0);
    pLayout0->addLayout(pLayout1);
    pLayout1->addStretch(1);
    m_pDescriptionLbl = new QILabel(this);
    pLayout1->addWidget(m_pDescriptionLbl, 0, Qt::AlignHCenter);

    QHBoxLayout *pLayout2 = new QHBoxLayout();
    pLayout2->setMargin(0);
    pLayout1->addLayout(pLayout2);

    m_progressBar = new QProgressBar(this);
    pLayout2->addWidget(m_progressBar, 0, Qt::AlignVCenter);

    if (m_cOperations > 1)
        m_pDescriptionLbl->setText(QString(m_spcszOpDescTpl)
                                   .arg(m_progress.GetOperationDescription())
                                   .arg(m_iCurrentOperation).arg(m_cOperations));
    else
        m_pDescriptionLbl->setText(QString("%1 ...")
                                   .arg(m_progress.GetOperationDescription()));
    m_progressBar->setMaximum(100);
    setWindowTitle(QString("%1: %2").arg(strTitle, m_progress.GetDescription()));
    m_progressBar->setValue(0);
    m_fCancelEnabled = m_progress.GetCancelable();
    if (m_fCancelEnabled)
    {
        m_pCancelBtn = new UIMiniCancelButton(this);
        m_pCancelBtn->setFocusPolicy(Qt::ClickFocus);
        pLayout2->addWidget(m_pCancelBtn, 0, Qt::AlignVCenter);
        connect(m_pCancelBtn, SIGNAL(clicked()), this, SLOT(cancelOperation()));
    }

    m_pEtaLbl = new QILabel(this);
    pLayout1->addWidget(m_pEtaLbl, 0, Qt::AlignLeft | Qt::AlignVCenter);

    pLayout1->addStretch(1);

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    retranslateUi();

    /* The progress dialog will be shown automatically after
     * the duration is over if progress is not finished yet. */
    QTimer::singleShot(cMinDuration, this, SLOT(showDialog()));
}

void UIProgressDialog::retranslateUi()
{
    m_strCancel = tr("Canceling...");
    if (m_pCancelBtn)
    {
        m_pCancelBtn->setText(tr("&Cancel"));
        m_pCancelBtn->setToolTip(tr("Cancel the current operation"));
    }
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
         * responsive and is it in our case (We show the progress dialog bar). */
#ifndef Q_WS_MAC
        if (m_fCancelEnabled)
            QApplication::setOverrideCursor(QCursor(Qt::BusyCursor));
        else
            QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
#endif /* Q_WS_MAC */

        /* Enter the modal loop, but don't show the window immediately */
        exec(false);

        /* Kill refresh timer */
        killTimer(id);

#ifndef Q_WS_MAC
        /* Reset the busy cursor */
        QApplication::restoreOverrideCursor();
#endif /* Q_WS_MAC */

        return result();
    }
    return Rejected;
}

void UIProgressDialog::reject()
{
    if (m_fCancelEnabled)
        cancelOperation();
}

void UIProgressDialog::timerEvent(QTimerEvent * /* pEvent */)
{
    /* We should hide progress-dialog
     * if it was already finalized but not yet closed.
     * This could happens in case of some other
     * modal dialog prevents our event-loop from
     * being exit overlapping 'this'. */
    if (m_fEnded && !isHidden())
    {
        hide();
        return;
    }
    else if (m_fEnded)
        return;

    if (!m_fEnded && (!m_progress.isOk() || m_progress.GetCompleted()))
    {
        /* Progress finished */
        if (m_progress.isOk())
        {
            m_progressBar->setValue(100);
            done(Accepted);
        }
        /* Progress is not valid */
        else
            done(Rejected);

        /* Request to exit loop */
        m_fEnded = true;
        return;
    }

    if (!m_progress.GetCanceled())
    {
        /* Update the progress dialog */
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

        /* Then operation text if changed */
        ulong newOp = m_progress.GetOperation() + 1;
        if (newOp != m_iCurrentOperation)
        {
            m_iCurrentOperation = newOp;
            m_pDescriptionLbl->setText(QString(m_spcszOpDescTpl)
                                       .arg(m_progress.GetOperationDescription())
                                       .arg(m_iCurrentOperation).arg(m_cOperations));
        }
        m_progressBar->setValue(m_progress.GetPercent());
    }
    else
        m_pEtaLbl->setText(m_strCancel);
}

void UIProgressDialog::closeEvent(QCloseEvent *pEvent)
{
    if (m_fCancelEnabled)
        cancelOperation();
    else
        pEvent->ignore();
}

void UIProgressDialog::showDialog()
{
    /* We should not show progress-dialog
     * if it was already finalized but not yet closed.
     * This could happens in case of some other
     * modal dialog prevents our event-loop from
     * being exit overlapping 'this'. */
    if (!m_fEnded)
        show();
}

void UIProgressDialog::cancelOperation()
{
    if (m_pCancelBtn)
        m_pCancelBtn->setEnabled(false);
    m_progress.Cancel();
}

