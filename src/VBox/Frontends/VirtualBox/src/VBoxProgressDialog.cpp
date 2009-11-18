/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxProgressDialog class implementation
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/* VBox includes */
#include "COMDefs.h"
#include "QIDialogButtonBox.h"
#include "QILabel.h"
#include "VBoxGlobal.h"
#include "VBoxProgressDialog.h"
#include "VBoxSpecialControls.h"
#ifdef Q_WS_MAC
# include "VBoxUtils-darwin.h"
#endif

/* Qt includes */
#include <QCloseEvent>
#include <QEventLoop>
#include <QProgressBar>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>

#define VBOX_SECOND 1
#define VBOX_MINUTE VBOX_SECOND * 60
#define VBOX_HOUR VBOX_MINUTE * 60
#define VBOX_DAY VBOX_HOUR * 24

const char *VBoxProgressDialog::sOpDescTpl = "%1... (%2/%3)";

VBoxProgressDialog::VBoxProgressDialog (CProgress &aProgress,
                                        const QString &aTitle,
                                        int aMinDuration /* = 2000 */,
                                        QWidget *aParent /* = 0 */)
//  : QIDialog (aParent, Qt::Sheet | Qt::MSWindowsFixedSizeDialogHint | Qt::WindowTitleHint)
  : QIDialog (aParent, Qt::MSWindowsFixedSizeDialogHint | Qt::WindowTitleHint)
  , mProgress (aProgress)
  , mCancelBtn (0)
  , mCancelEnabled (false)
  , mOpCount (mProgress.GetOperationCount())
  , mCurOp (mProgress.GetOperation() + 1)
  , mEnded (false)
{
    setModal (true);

    QVBoxLayout *pLayout1 = new QVBoxLayout (this);

#ifdef Q_WS_MAC
    ::darwinSetHidesAllTitleButtons (this);
    ::darwinSetShowsResizeIndicator (this, false);
    VBoxGlobal::setLayoutMargin (pLayout1, 6);
#endif /* Q_WS_MAC */

    mLabel = new QILabel (this);
    pLayout1->addWidget (mLabel, 0, Qt::AlignHCenter);

    QHBoxLayout *pLayout2 = new QHBoxLayout();
    pLayout2->setMargin (0);
    pLayout1->addLayout (pLayout2);

    mProgressBar = new QProgressBar (this);
    pLayout2->addWidget (mProgressBar, 0, Qt::AlignVCenter);

    if (mOpCount > 1)
        mLabel->setText (QString (sOpDescTpl)
                         .arg (mProgress.GetOperationDescription())
                         .arg (mCurOp).arg (mOpCount));
    else
        mLabel->setText (QString ("%1...")
                         .arg (mProgress.GetOperationDescription()));
    mProgressBar->setMaximum (100);
    setWindowTitle (QString ("%1: %2").arg (aTitle, mProgress.GetDescription()));
    mProgressBar->setValue (0);
    mCancelEnabled = aProgress.GetCancelable();
    if (mCancelEnabled)
    {
        mCancelBtn = new VBoxMiniCancelButton (this);
        mCancelBtn->setFocusPolicy (Qt::ClickFocus);
        pLayout2->addWidget (mCancelBtn, 0, Qt::AlignVCenter);
        connect (mCancelBtn, SIGNAL (clicked()), this, SLOT (cancelOperation()));
    }

    mETA = new QILabel (this);
    pLayout1->addWidget (mETA, 0, Qt::AlignLeft | Qt::AlignVCenter);

    setSizePolicy (QSizePolicy::Fixed, QSizePolicy::Fixed);

    retranslateUi();

    /* The progress dialog will be shown automatically after
     * the duration is over if progress is not finished yet. */
    QTimer::singleShot (aMinDuration, this, SLOT (showDialog()));
}

void VBoxProgressDialog::retranslateUi()
{
    mETAText[ 0] = tr ("%1 days, %2 hours remaining");
    mETAText[ 1] = tr ("%1 days, %2 minutes remaining");
    mETAText[ 2] = tr ("%1 days remaining");
    mETAText[ 3] = tr ("1 day, %1 hours remaining");
    mETAText[ 4] = tr ("1 day, %1 minutes remaining");
    mETAText[ 5] = tr ("1 day remaining");
    mETAText[ 6] = tr ("%1 hours, %2 minutes remaining");
    mETAText[ 7] = tr ("1 hour, %1 minutes remaining");
    mETAText[ 8] = tr ("1 hour remaining");
    mETAText[ 9] = tr ("%1 minutes remaining");
    mETAText[10] = tr ("1 minute, %2 seconds remaining");
    mETAText[11] = tr ("1 minute remaining");
    mETAText[12] = tr ("%1 seconds remaining");
    mETAText[13] = tr ("A few seconds remaining");

    mCancelText = tr ("Canceling...");
    if (mCancelBtn)
    {
        mCancelBtn->setText (tr ("&Cancel"));
        mCancelBtn->setToolTip (tr ("Cancel the current operation"));
    }
}

int VBoxProgressDialog::run (int aRefreshInterval)
{
    if (mProgress.isOk())
    {
        /* Start refresh timer */
        int id = startTimer (aRefreshInterval);

        /* Set busy cursor */
        QApplication::setOverrideCursor (QCursor (Qt::WaitCursor));

        /* Enter the modal loop, but don't show the window immediately */
        exec (false);

        /* Kill refresh timer */
        killTimer (id);

        QApplication::restoreOverrideCursor();

        return result();
    }
    return Rejected;
}

void VBoxProgressDialog::showDialog()
{
    /* We should not show progress-dialog
     * if it was already finalized but not yet closed.
     * This could happens in case of some other
     * modal dialog prevents our event-loop from
     * being exit overlapping 'this'. */
    if (!mEnded)
        show();
}

void VBoxProgressDialog::cancelOperation()
{
    if (mCancelBtn)
        mCancelBtn->setEnabled (false);
    mProgress.Cancel();
}

void VBoxProgressDialog::timerEvent (QTimerEvent * /* aEvent */)
{
    /* We should hide progress-dialog
     * if it was already finalized but not yet closed.
     * This could happens in case of some other
     * modal dialog prevents our event-loop from
     * being exit overlapping 'this'. */
    if (mEnded && !isHidden())
    {
        hide();
        return;
    }
    else if (mEnded)
        return;

    if (!mEnded && (!mProgress.isOk() || mProgress.GetCompleted()))
    {
        /* Progress finished */
        if (mProgress.isOk())
        {
            mProgressBar->setValue (100);
            done (Accepted);
        }
        /* Progress is not valid */
        else
            done (Rejected);

        /* Request to exit loop */
        mEnded = true;
        return;
    }

    if (!mProgress.GetCanceled())
    {
        /* Update the progress dialog */
        /* First ETA */
        long newTime = mProgress.GetTimeRemaining();
        QDateTime time;
        time.setTime_t (newTime);
        QDateTime refTime;
        refTime.setTime_t (0);

        int days = refTime.daysTo (time);
        int hours = time.addDays (-days).time().hour();
        int minutes = time.addDays (-days).time().minute();
        int seconds = time.addDays (-days).time().second();
        if (newTime > VBOX_DAY * 2 + VBOX_HOUR)
            mETA->setText (mETAText[ 0].arg (days).arg (hours));
        else if (newTime > VBOX_DAY * 2 + VBOX_MINUTE * 5)
            mETA->setText (mETAText[ 1].arg (days).arg (minutes));
        else if (newTime > VBOX_DAY * 2)
            mETA->setText (mETAText[ 2].arg (days));
        else if (newTime > VBOX_DAY + VBOX_HOUR)
            mETA->setText (mETAText[ 3].arg (hours));
        else if (newTime > VBOX_DAY + VBOX_MINUTE * 5)
            mETA->setText (mETAText[ 4].arg (minutes));
        else if (newTime > VBOX_HOUR * 23 + VBOX_MINUTE * 55)
            mETA->setText (mETAText[ 5]);
        else if (newTime >= VBOX_HOUR * 2)
            mETA->setText (mETAText[ 6].arg (hours).arg (minutes));
        else if (newTime > VBOX_HOUR + VBOX_MINUTE * 5)
            mETA->setText (mETAText[ 7].arg (minutes));
        else if (newTime > VBOX_MINUTE * 55)
            mETA->setText (mETAText[ 8]);
        else if (newTime > VBOX_MINUTE * 2)
            mETA->setText (mETAText[ 9].arg (minutes));
        else if (newTime > VBOX_MINUTE + VBOX_SECOND * 5)
            mETA->setText (mETAText[10].arg (seconds));
        else if (newTime > VBOX_SECOND * 55)
            mETA->setText (mETAText[11]);
        else if (newTime > VBOX_SECOND * 5)
            mETA->setText (mETAText[12].arg (seconds));
        else if (newTime >= 0)
            mETA->setText (mETAText[13]);
        else
            mETA->clear();

        /* Then operation text if changed */
        ulong newOp = mProgress.GetOperation() + 1;
        if (newOp != mCurOp)
        {
            mCurOp = newOp;
            mLabel->setText (QString (sOpDescTpl)
                             .arg (mProgress.GetOperationDescription())
                             .arg (mCurOp).arg (mOpCount));
        }
        mProgressBar->setValue (mProgress.GetPercent());
    }else
        mETA->setText (mCancelText);
}

void VBoxProgressDialog::reject()
{
    if (mCancelEnabled)
        cancelOperation();
}

void VBoxProgressDialog::closeEvent (QCloseEvent *aEvent)
{
    if (mCancelEnabled)
        cancelOperation();
    else
        aEvent->ignore();
}

