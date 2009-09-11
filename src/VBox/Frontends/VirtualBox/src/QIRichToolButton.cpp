/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VirtualBox Qt extensions: QIRichToolButton class implementation
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
#include "QIRichToolButton.h"

/* Qt includes */
#include <QLabel>
#include <QHBoxLayout>
#include <QToolButton>
#include <QKeyEvent>
#include <QStylePainter>
#include <QStyleOptionFocusRect>

QIRichToolButton::QIRichToolButton (const QString &aName, QWidget *aParent)
        : QWidget (aParent)
        , mButton (new QToolButton())
        , mLabel (new QLabel (aName))
{
    /* Setup itself */
    setFocusPolicy (Qt::StrongFocus);

    /* Setup tool-button */
    mButton->setAutoRaise (true);
    mButton->setFixedSize (17, 16);
    mButton->setFocusPolicy (Qt::NoFocus);
    mButton->setStyleSheet ("QToolButton {border: 0px none black;}");
    connect (mButton, SIGNAL (clicked (bool)), this, SLOT (buttonClicked()));

    /* Setup text-label */
    mLabel->setBuddy (mButton);
    mLabel->setStyleSheet ("QLabel {padding: 2px 0px 2px 0px;}");

    /* Setup main-layout */
    QHBoxLayout *mainLayout = new QHBoxLayout (this);
    VBoxGlobal::setLayoutMargin (mainLayout, 0);
    mainLayout->setSpacing (0);
    mainLayout->addWidget (mButton);
    mainLayout->addWidget (mLabel);

    /* Install event-filter */
    qApp->installEventFilter (this);
}

bool QIRichToolButton::eventFilter (QObject *aObject, QEvent *aEvent)
{
    /* Process only QIRichToolButton or children */
    if (!(aObject == this || children().contains (aObject)))
        return QWidget::eventFilter (aObject, aEvent);

    /* Process keyboard events */
    if (aEvent->type() == QEvent::KeyPress)
    {
        QKeyEvent *kEvent = static_cast <QKeyEvent*> (aEvent);
        if (kEvent->key() == Qt::Key_Space)
            animateClick();
    }

    /* Process mouse events */
    if ((aEvent->type() == QEvent::MouseButtonPress ||
         aEvent->type() == QEvent::MouseButtonDblClick)
        && aObject == mLabel)
    {
        /* Label click as toggle */
        animateClick();
    }

    /* Default one handler */
    return QWidget::eventFilter (aObject, aEvent);
}

void QIRichToolButton::paintEvent (QPaintEvent *aEvent)
{
    /* Draw focus around mLabel if focused */
    if (hasFocus())
    {
        QStylePainter painter (this);
        QStyleOptionFocusRect option;
        option.initFrom (this);
        option.rect = mLabel->frameGeometry();
        painter.drawPrimitive (QStyle::PE_FrameFocusRect, option);
    }
    QWidget::paintEvent (aEvent);
}
