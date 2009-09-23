/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QISplitter class implementation
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

/* Global includes */
#include <QApplication>
#include <QEvent>
#include <QPainter>
#include <QPaintEvent>

/* Local includes */
#include "QISplitter.h"

QISplitter::QISplitter (QWidget *aParent)
    : QSplitter (aParent)
    , mPolished (false)
{
    qApp->installEventFilter (this);
}

bool QISplitter::eventFilter (QObject *aWatched, QEvent *aEvent)
{
    if (aWatched == handle (1))
    {
        switch (aEvent->type())
        {
            case QEvent::MouseButtonDblClick:
                restoreState (mBaseState);
                break;
            default:
                break;
        }
    }

    return QSplitter::eventFilter (aWatched, aEvent);
}

void QISplitter::showEvent (QShowEvent *aEvent)
{
    if (!mPolished)
    {
        mPolished = true;
        mBaseState = saveState();
    }

    return QSplitter::showEvent (aEvent);
}

QSplitterHandle* QISplitter::createHandle()
{
    return new QISplitterHandle (orientation(), this);
}

QISplitterHandle::QISplitterHandle (Qt::Orientation aOrientation, QISplitter *aParent)
    : QSplitterHandle (aOrientation, aParent)
{
}

void QISplitterHandle::paintEvent (QPaintEvent *aEvent)
{
    QPainter painter (this);
    QLinearGradient gradient;
    if (orientation() == Qt::Horizontal)
    {
        gradient.setStart (rect().left(), rect().height() / 2);
        gradient.setFinalStop (rect().right(), rect().height() / 2);
    }
    else
    {
        gradient.setStart (rect().width() / 2, rect().top());
        gradient.setFinalStop (rect().width() / 2, rect().bottom());
    }
    painter.fillRect (aEvent->rect(), QBrush (gradient));
}

