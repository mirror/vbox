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
#include "VBoxProgressDialog.h"
#include "COMDefs.h"
#include "QILabel.h"
#include "QIDialogButtonBox.h"
#include "VBoxGlobal.h"

#ifdef Q_WS_MAC
# include "VBoxUtils-darwin.h"
#endif

/* Qt includes */
#include <QVBoxLayout>
#include <QProgressBar>
#include <QTime>
#include <QEventLoop>
#include <QCloseEvent>
#include <QTimer>

const char *VBoxProgressDialog::sOpDescTpl = "%1... (%2/%3)";

VBoxProgressDialog::VBoxProgressDialog (CProgress &aProgress, const QString &aTitle,
                                        int aMinDuration /* = 2000 */, QWidget *aParent /* = NULL */)
  : QIDialog (aParent, Qt::MSWindowsFixedSizeDialogHint | Qt::WindowTitleHint)
  , mProgress (aProgress)
  , mEventLoop (new QEventLoop (this))
  , mCancelEnabled (false)
  , mOpCount (mProgress.GetOperationCount())
  , mCurOp (mProgress.GetOperation() + 1)
  , mEnded (false)
{
    setModal (true);

    QVBoxLayout *pMainLayout = new QVBoxLayout (this);

#ifdef Q_WS_MAC
    ::darwinSetHidesAllTitleButtons (this);
    ::darwinSetShowsResizeIndicator (this, false);
    VBoxGlobal::setLayoutMargin (pMainLayout, 6);
#endif /* Q_WS_MAC */

    mLabel = new QILabel (this);
    pMainLayout->addWidget (mLabel);
    pMainLayout->setAlignment (mLabel, Qt::AlignHCenter);

    mProgressBar = new QProgressBar (this);
    pMainLayout->addWidget (mProgressBar);

    QHBoxLayout *pLayout1 = new QHBoxLayout();
    pLayout1->setMargin (0);
    mETA = new QILabel (this);
    pLayout1->addWidget (mETA);
    pMainLayout->addLayout (pLayout1);

    if (mOpCount > 1)
        mLabel->setText (QString (sOpDescTpl)
                         .arg (mProgress.GetOperationDescription())
                         .arg (mCurOp).arg (mOpCount));
    else
        mLabel->setText (QString ("%1...")
                         .arg (mProgress.GetOperationDescription()));
    mProgressBar->setMaximum (100);
    setWindowTitle (QString ("%1: %2")
                    .arg (aTitle, mProgress.GetDescription()));
    QTimer::singleShot (aMinDuration, this, SLOT (show()));
    mProgressBar->setValue (0);
    mCancelEnabled = aProgress.GetCancelable();
    if (mCancelEnabled)
    {
        QDialogButtonBox *pBtnBox = new QDialogButtonBox (QDialogButtonBox::Cancel, Qt::Horizontal, this);
        pLayout1->addWidget (pBtnBox);
        connect (pBtnBox, SIGNAL (rejected()),
                 this, SLOT (cancelOperation()));
    }

    retranslateUi();
}

void VBoxProgressDialog::retranslateUi()
{
    mETAText = tr ("ETA: %1");
}

int VBoxProgressDialog::run (int aRefreshInterval)
{
    if (mProgress.isOk())
    {
        /* Start refresh timer */
        int id = startTimer (aRefreshInterval);

        /* The progress dialog is automatically shown after the duration is over */

        /* Enter the modal loop */
        mEventLoop->exec();

        /* Kill refresh timer */
        killTimer (id);

        return result();
    }
    return Rejected;
}

void VBoxProgressDialog::cancelOperation()
{
    mProgress.Cancel();
}

void VBoxProgressDialog::timerEvent (QTimerEvent * /* aEvent */)
{
    if (!mEnded && (!mProgress.isOk() || mProgress.GetCompleted()))
    {
        /* Progress finished */
        if (mProgress.isOk())
        {
            mProgressBar->setValue (100);
            setResult (Accepted);
        }
        /* Progress is not valid */
        else
            setResult (Rejected);

        /* Request to exit loop */
        mEnded = true;

        /* The progress will be finalized
         * on next timer iteration. */
        return;
    }

    if (mEnded)
    {
        /* Exit loop if it is running */
        if (mEventLoop->isRunning())
            mEventLoop->quit();
        return;
    }

    if (!mProgress.GetCanceled())
    {
        /* Update the progress dialog */
        /* First ETA */
        long newTime = mProgress.GetTimeRemaining();
        if (newTime > 0)
        {
            QTime time(0, 0);
            time = time.addSecs (newTime);
            mETA->setText (mETAText.arg (time.toString()));
        }else
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
    }
}

void VBoxProgressDialog::reject()
{
    if (mCancelEnabled)
        QIDialog::reject();
}

void VBoxProgressDialog::closeEvent (QCloseEvent *aEvent)
{
    if (mCancelEnabled)
        QIDialog::closeEvent (aEvent);
    else
        aEvent->ignore();
}

