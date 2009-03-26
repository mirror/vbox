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

#ifdef Q_WS_MAC
# include "VBoxUtils-darwin.h"
#endif

/* Qt includes */
#include <QProgressDialog>
#include <QEventLoop>

const char *VBoxProgressDialog::sOpDescTpl = "%1... (%2/%3)";

VBoxProgressDialog::VBoxProgressDialog (CProgress &aProgress, const QString &aTitle,
                                        int aMinDuration /* = 2000 */, QWidget *aParent /* = NULL */)
  : QProgressDialog (aParent, Qt::MSWindowsFixedSizeDialogHint | Qt::WindowTitleHint)
  , mProgress (aProgress)
  , mEventLoop (new QEventLoop (this))
  , mCancelEnabled (false)
  , mOpCount (mProgress.GetOperationCount())
  , mCurOp (mProgress.GetOperation() + 1)
  , mEnded (false)
{
    setModal (true);
#ifdef Q_WS_MAC
    ::darwinSetHidesAllTitleButtons (this);
    ::darwinSetShowsResizeIndicator (this, false);
#endif /* Q_WS_MAC */

    if (mOpCount > 1)
        setLabelText (QString (sOpDescTpl)
                      .arg (mProgress.GetOperationDescription())
                      .arg (mCurOp).arg (mOpCount));
    else
        setLabelText (QString ("%1...")
                      .arg (mProgress.GetOperationDescription()));
    setCancelButtonText (QString::null);
    setMaximum (100);
    setWindowTitle (QString ("%1: %2")
                    .arg (aTitle, mProgress.GetDescription()));
    setMinimumDuration (aMinDuration);
    setValue (0);
    mCancelEnabled = aProgress.GetCancelable();
    if (mCancelEnabled)
        setCancelButtonText(tr ("&Cancel"));
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

void VBoxProgressDialog::timerEvent (QTimerEvent * /* aEvent */)
{
    if (!mEnded && (!mProgress.isOk() || mProgress.GetCompleted()))
    {
        /* Progress finished */
        if (mProgress.isOk())
        {
            setValue (100);
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

    /* Update the progress dialog */
    ulong newOp = mProgress.GetOperation() + 1;
    if (newOp != mCurOp)
    {
        mCurOp = newOp;
        setLabelText (QString (sOpDescTpl)
                      .arg (mProgress.GetOperationDescription())
                      .arg (mCurOp).arg (mOpCount));
    }
    setValue (mProgress.GetPercent());
}

void VBoxProgressDialog::reject()
{
    if (mCancelEnabled)
        QProgressDialog::reject();
}

void VBoxProgressDialog::closeEvent (QCloseEvent *aEvent)
{
    if (mCancelEnabled)
        QProgressDialog::closeEvent (aEvent);
    else
        aEvent->ignore();
}

