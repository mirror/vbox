/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * QIHelpButton class implementation
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

#include "QIHelpButton.h"

/* Qt includes */
#include <QPainter>
#include <QBitmap>
#include <QMouseEvent>

/* From: src/gui/styles/qmacstyle_mac.cpp */
static const int PushButtonLeftOffset = 6;
static const int PushButtonTopOffset = 4;
static const int PushButtonRightOffset = 12;
static const int PushButtonBottomOffset = 12;

QIHelpButton::QIHelpButton(QWidget *aParent /* = NULL */)
    : QIWithRetranslateUI<QPushButton> (aParent)
{
#ifdef Q_WS_MAC
    mButtonPressed = false;
    mNormalPixmap = new QPixmap (":/help_button_normal_mac_22px.png");
    mPressedPixmap = new QPixmap (":/help_button_pressed_mac_22px.png");
    mSize = mNormalPixmap->size();
    mMask = new QImage (mNormalPixmap->mask().toImage());
    mBRect = QRect (PushButtonLeftOffset, 
                    PushButtonTopOffset,
                    mSize.width(),
                    mSize.height());
#endif /* Q_WS_MAC */
    /* Applying language settings */
    retranslateUi();
}

void QIHelpButton::initFrom (QPushButton *aOther)
{
    setIcon (aOther->icon());
    setText (aOther->text());
    setShortcut (aOther->shortcut());
    setFlat (aOther->isFlat());
    setAutoDefault (aOther->autoDefault());
    setDefault (aOther->isDefault());
    /* Applying language settings */
    retranslateUi();
}

#ifdef Q_WS_MAC
QIHelpButton::~QIHelpButton()
{
    delete mNormalPixmap;
    delete mPressedPixmap;
    delete mMask;
}

QSize QIHelpButton::sizeHint() const
{
    return QSize (mSize.width() + PushButtonLeftOffset + PushButtonRightOffset,
                  mSize.height() + PushButtonTopOffset + PushButtonBottomOffset);
}

bool QIHelpButton::hitButton (const QPoint &pos) const
{
    if (mBRect.contains (pos))
        return  mMask->pixel (pos.x() - PushButtonLeftOffset,
                              pos.y() - PushButtonTopOffset) == 0xff000000;
    else
        return false;
}

void QIHelpButton::mousePressEvent (QMouseEvent *aEvent) 
{
    if (hitButton (aEvent->pos()))
        mButtonPressed = true;
    QPushButton::mousePressEvent (aEvent);
    update();
}

void QIHelpButton::mouseReleaseEvent (QMouseEvent *aEvent) 
{
    QPushButton::mouseReleaseEvent (aEvent);
    mButtonPressed = false;
    update();
}

void QIHelpButton::leaveEvent (QEvent * aEvent)
{
    QPushButton::leaveEvent (aEvent);
    mButtonPressed = false;
    update();
}

void QIHelpButton::paintEvent (QPaintEvent *aEvent)
{
    QPainter painter (this);
    painter.drawPixmap (PushButtonLeftOffset, PushButtonTopOffset, mButtonPressed ? *mPressedPixmap: *mNormalPixmap);
}
#endif /* Q_WS_MAC */

void QIHelpButton::retranslateUi()
{
    QPushButton::setText (tr ("&Help"));
    if (QPushButton::shortcut().isEmpty())
        QPushButton::setShortcut (QKeySequence::HelpContents); 
}

