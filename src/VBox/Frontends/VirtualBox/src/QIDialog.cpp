/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QIDialog class implementation
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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


#include "QIDialog.h"
#include "VBoxGlobal.h"
#ifdef Q_WS_MAC
#include "VBoxUtils.h"
#endif /* Q_WS_MAC */

QIDialog::QIDialog (QWidget *aParent, Qt::WindowFlags aFlags)
    : QDialog (aParent, aFlags)
    , mPolished (false)
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

int QIDialog::exec()
{
#if QT_VERSION >= 0x040500
    /* After 4.5 exec ignores the Qt::Sheet flag. See "New Ways of Using
     * Dialogs" in http://doc.trolltech.com/qq/QtQuarterly30.pdf why. Because
     * we are lazy, we recreate the old behavior. */
    if ((windowFlags() & Qt::Sheet) == Qt::Sheet)
    {
        QEventLoop eventLoop;
        connect(this, SIGNAL(finished(int)),
                &eventLoop, SLOT(quit()));
        /* Use the new open call. */
        open();
        eventLoop.exec();

        return result();
    }
    else
#endif /* QT_VERSION >= 0x040500 */
        return QDialog::exec();
}
