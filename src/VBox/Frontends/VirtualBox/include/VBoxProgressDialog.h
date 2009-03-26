/** @file
 *
 * VBox frontends: Qt4 GUI ("VirtualBox"):
 * VBoxProgressDialog class declaration
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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

#ifndef __VBoxProgressDialog_h__
#define __VBoxProgressDialog_h__

/* Qt includes */
#include <QProgressDialog>

/* VBox forward declarations */
class CProgress;

/* Qt forward declarations */
class QEventLoop;

/**
 * A QProgressDialog enhancement that allows to:
 *
 * 1) prevent closing the dialog when it has no cancel button;
 * 2) effectively track the IProgress object completion (w/o using
 *    IProgress::waitForCompletion() and w/o blocking the UI thread in any other
 *    way for too long).
 *
 * @note The CProgress instance is passed as a non-const reference to the
 *       constructor (to memorize COM errors if they happen), and therefore must
 *       not be destroyed before the created VBoxProgressDialog instance is
 *       destroyed.
 */
class VBoxProgressDialog: protected QProgressDialog
{
    Q_OBJECT;

public:
    VBoxProgressDialog (CProgress &aProgress, const QString &aTitle,
                        int aMinDuration = 2000, QWidget *aParent = NULL);

    int run (int aRefreshInterval);
    bool cancelEnabled() const { return mCancelEnabled; }

protected:
    virtual void reject();
    virtual void timerEvent (QTimerEvent *aEvent);
    virtual void closeEvent (QCloseEvent *aEvent);

private:
    /* Private member vars */
    CProgress &mProgress;
    QEventLoop *mEventLoop;
    bool mCancelEnabled;
    const ulong mOpCount;
    ulong mCurOp;
    bool mEnded;

    static const char *sOpDescTpl;
};

#endif /* __VBoxProgressDialog_h__ */

