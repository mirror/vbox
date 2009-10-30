/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QIDialog class implementation
 */

/*
 * Copyright (C) 2008-2009 Sun Microsystems, Inc.
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
#include "QIDialog.h"
#include "VBoxGlobal.h"
#ifdef Q_WS_MAC
# include "VBoxUtils.h"
#endif /* Q_WS_MAC */

/* Qt includes */
#include <QPointer>

QIDialog::QIDialog (QWidget *aParent /* = 0 */, Qt::WindowFlags aFlags /* = 0 */)
    : QDialog (aParent, aFlags)
    , mPolished (false)
    , mEventLoop (0)
{
}

void QIDialog::showEvent (QShowEvent * /* aEvent */)
{
    /* Two thinks to do for fixed size dialogs on MacOS X:
     * 1. Make sure the layout is polished and have the right size
     * 2. Disable that _unnecessary_ size grip (Bug in Qt?) */
    QSizePolicy policy = sizePolicy();
    if ((policy.horizontalPolicy() == QSizePolicy::Fixed &&
         policy.verticalPolicy() == QSizePolicy::Fixed) ||
        (windowFlags() & Qt::Sheet) == Qt::Sheet)
    {
        adjustSize();
        setFixedSize (size());
#ifdef Q_WS_MAC
        ::darwinSetShowsResizeIndicator (this, false);
#endif /* Q_WS_MAC */
    }

    /* Polishing border */
    if (mPolished)
        return;
    mPolished = true;

    /* Explicit widget centering relatively to it's parent
     * if any or desktop if parent is missed. */
    VBoxGlobal::centerWidget (this, parentWidget(), false);
}

int QIDialog::exec (bool aShow /* = true */)
{
    /* Reset the result code */
    setResult (QDialog::Rejected);

    bool wasDeleteOnClose = testAttribute (Qt::WA_DeleteOnClose);
    setAttribute (Qt::WA_DeleteOnClose, false);
#if defined(Q_WS_MAC) && QT_VERSION >= 0x040500
    /* After 4.5 Qt changed the behavior of Sheets for the window/application
     * modal case. See "New Ways of Using Dialogs" in
     * http://doc.trolltech.com/qq/QtQuarterly30.pdf why. We want the old
     * behavior back, where all modal windows where shown as sheets. So make
     * the modal mode window, but be application modal in any case. */
    Qt::WindowModality winModality = windowModality();
    bool wasSetWinModality = testAttribute (Qt::WA_SetWindowModality);
    if ((windowFlags() & Qt::Sheet) == Qt::Sheet)
    {
        setWindowModality (Qt::WindowModal);
        setAttribute (Qt::WA_SetWindowModality, false);
    }
#endif /* defined(Q_WS_MAC) && QT_VERSION >= 0x040500 */
    /* The dialog has to modal in any case. Save the current modality to
     * restore it later. */
    bool wasShowModal = testAttribute (Qt::WA_ShowModal);
    setAttribute (Qt::WA_ShowModal, true);

    /* Create a local event loop */
    mEventLoop = new QEventLoop();
    /* Show the window if requested */
    if (aShow)
        show();
    /* A guard to ourself for the case we destroy ourself. */
    QPointer<QIDialog> guard = this;
    /* Start the event loop. This blocks. */
    mEventLoop->exec();
    /* Delete the event loop */
    delete mEventLoop;
    mEventLoop = 0;
    /* Are we valid anymore? */
    if (guard.isNull())
        return QDialog::Rejected;
    /* Save the result code in case we delete ourself */
    QDialog::DialogCode res = (QDialog::DialogCode)result();
#if defined(Q_WS_MAC) && QT_VERSION >= 0x040500
    /* Restore old modality mode */
    if ((windowFlags() & Qt::Sheet) == Qt::Sheet)
    {
        setWindowModality (winModality);
        setAttribute (Qt::WA_SetWindowModality, wasSetWinModality);
    }
#endif /* defined(Q_WS_MAC) && QT_VERSION >= 0x040500 */
    /* Set the old show modal attribute */
    setAttribute (Qt::WA_ShowModal, wasShowModal);
    /* Delete us in the case we should do so on close */
    if (wasDeleteOnClose)
        delete this;
    /* Return the final result */
    return res;
}

void QIDialog::setVisible (bool aVisible)
{
    QDialog::setVisible (aVisible);
    /* Exit from the event loop if there is any and we are changing our state
     * from visible to invisible. */
    if (mEventLoop && !aVisible)
        mEventLoop->exit();
}

