/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QIArrowButtonPress class implementation
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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
#include "VBoxGlobal.h"
#include "QIArrowButtonPress.h"

/* Qt includes */
#include <QKeyEvent>


/** @class QIArrowButtonPress
 *
 *  The QIArrowButtonPress class is an arrow tool-botton with text-label,
 *  used as back/next buttons in QIMessageBox class.
 *
 */

QIArrowButtonPress::QIArrowButtonPress (bool aNext, const QString &aName, QWidget *aParent)
    : QIRichToolButton (aName, aParent)
    , mNext (aNext)
{
    updateIcon();
}

void QIArrowButtonPress::updateIcon()
{
    mButton->setIcon (VBoxGlobal::iconSet (mNext ?
                      ":/arrow_right_10px.png" : ":/arrow_left_10px.png"));
}

bool QIArrowButtonPress::eventFilter (QObject *aObject, QEvent *aEvent)
{
    /* Process only QIArrowButtonPress or children */
    if (!(aObject == this || children().contains (aObject)))
        return QIRichToolButton::eventFilter (aObject, aEvent);

    /* Process keyboard events */
    if (aEvent->type() == QEvent::KeyPress)
    {
        QKeyEvent *kEvent = static_cast <QKeyEvent*> (aEvent);
        if ((mNext && kEvent->key() == Qt::Key_PageUp) ||
            (!mNext && kEvent->key() == Qt::Key_PageDown))
            animateClick();
    }

    /* Default one handler */
    return QIRichToolButton::eventFilter (aObject, aEvent);
}
