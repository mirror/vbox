/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QIArrowButtonSwitch class implementation
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
#include "QIArrowButtonSwitch.h"

/* Qt includes */
#include <QKeyEvent>


/** @class QIArrowButtonSwitch
 *
 *  The QIArrowButtonSwitch class is an arrow tool-botton with text-label,
 *  used as collaps/expand switch in QIMessageBox class.
 *
 */

QIArrowButtonSwitch::QIArrowButtonSwitch (QWidget *aParent)
    : QIRichToolButton (aParent)
    , mIsExpanded (false)
{
    updateIcon();
}

QIArrowButtonSwitch::QIArrowButtonSwitch (const QString &aName, QWidget *aParent)
    : QIRichToolButton (aName, aParent)
    , mIsExpanded (false)
{
    updateIcon();
}

void QIArrowButtonSwitch::buttonClicked()
{
    mIsExpanded = !mIsExpanded;
    updateIcon();
    QIRichToolButton::buttonClicked();
}

void QIArrowButtonSwitch::updateIcon()
{
    mButton->setIcon (VBoxGlobal::iconSet (mIsExpanded ?
                      ":/arrow_down_10px.png" : ":/arrow_right_10px.png"));
}

bool QIArrowButtonSwitch::eventFilter (QObject *aObject, QEvent *aEvent)
{
    /* Process only QIArrowButtonSwitch or children */
    if (!(aObject == this || children().contains (aObject)))
        return QIRichToolButton::eventFilter (aObject, aEvent);

    /* Process keyboard events */
    if (aEvent->type() == QEvent::KeyPress)
    {
        QKeyEvent *kEvent = static_cast <QKeyEvent*> (aEvent);
        if ((mIsExpanded && kEvent->key() == Qt::Key_Minus) ||
            (!mIsExpanded && kEvent->key() == Qt::Key_Plus))
            animateClick();
    }

    /* Default one handler */
    return QIRichToolButton::eventFilter (aObject, aEvent);
}
