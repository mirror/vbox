/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxMiniToolBar class declaration & implementation. This is the toolbar shown on fullscreen mode.
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

/* VBox includes */
#include "VBoxGlobal.h"
#include "VBoxMiniToolBar.h"

/* Qt includes */
#include <QCursor>
#include <QDesktopWidget>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPaintEvent>
#include <QPolygon>
#include <QRect>
#include <QRegion>
#include <QTimer>
#include <QToolButton>

VBoxMiniToolBar::VBoxMiniToolBar (QWidget *aParent, Alignment aAlignment)
    : VBoxToolBar (aParent)
    , mAutoHideCounter (0)
    , mAutoHide (true)
    , mSlideToScreen (true)
    , mHideAfterSlide (false)
    , mPolished (false)
    , mIsSeamless (false)
    , mAlignment (aAlignment)
    , mAnimated (true)
    , mScrollDelay (20)
    , mAutoScrollDelay (100)
    , mAutoHideTotalCounter (10)
{
    AssertMsg (parentWidget(), ("Parent widget must be set!!!\n"));

    /* Various options */
    setIconSize (QSize (16, 16));
    setMouseTracking (mAutoHide);
    setVisible (false);

    /* Left margin of tool-bar */
    mMargins << widgetForAction (addWidget (new QWidget (this)));

    /* Add pushpin */
    mAutoHideAct = new QAction (this);
    mAutoHideAct->setIcon (VBoxGlobal::iconSet (":/pin_16px.png"));
    mAutoHideAct->setToolTip (tr ("Always show the toolbar"));
    mAutoHideAct->setCheckable (true);
    mAutoHideAct->setChecked (!mAutoHide);
    connect (mAutoHideAct, SIGNAL (toggled (bool)), this, SLOT (togglePushpin (bool)));
    addAction (mAutoHideAct);

    /* Left menu margin */
    mSpacings << widgetForAction (addWidget (new QWidget (this)));

    /* Right menu margin */
    mInsertPosition = addWidget (new QWidget (this));
    mSpacings << widgetForAction (mInsertPosition);

    /* Left label margin */
    mLabelMargins << widgetForAction (addWidget (new QWidget (this)));

    /* Insert a label for VM Name */
    mDisplayLabel = new QLabel (this);
    mDisplayLabel->setAlignment (Qt::AlignCenter);
    addWidget (mDisplayLabel);

    /* Right label margin */
    mLabelMargins << widgetForAction (addWidget (new QWidget (this)));

    /* Exit action */
    QAction *restoreAct = new QAction (this);
    restoreAct->setIcon (VBoxGlobal::iconSet (":/restore_16px.png"));
    restoreAct->setToolTip (tr ("Exit Full Screen or Seamless Mode"));
    connect (restoreAct, SIGNAL (triggered()), this, SIGNAL (exitAction()));
    addAction (restoreAct);

    /* Close action */
    QAction *closeAct = new QAction (this);
    closeAct->setIcon (VBoxGlobal::iconSet (":/close_16px.png"));
    closeAct->setToolTip (tr ("Close VM"));
    connect (closeAct, SIGNAL (triggered()), this, SIGNAL (closeAction()));
    addAction (closeAct);

    /* Right margin of tool-bar */
    mMargins << widgetForAction (addWidget (new QWidget (this)));
}

void VBoxMiniToolBar::setIsSeamlessMode (bool aIsSeamless)
{
    mIsSeamless = aIsSeamless;
}

VBoxMiniToolBar& VBoxMiniToolBar::operator<< (QList <QMenu*> aMenus)
{
    for (int i = 0; i < aMenus.size(); ++ i)
    {
        QAction *action = aMenus [i]->menuAction();
        insertAction (mInsertPosition, action);
        if (QToolButton *button = qobject_cast <QToolButton*> (widgetForAction (action)))
            button->setPopupMode (QToolButton::InstantPopup);
        if (i != aMenus.size() - 1)
            mSpacings << widgetForAction (insertWidget (mInsertPosition, new QWidget (this)));
    }
    return *this;
}

void VBoxMiniToolBar::updateDisplay (bool aShow, bool aSetHideFlag)
{
    mAutoHideCounter = 0;

    setMouseTracking (mAutoHide);

    if (aShow)
    {
        if (isHidden())
            moveToBase();

        if (mAnimated)
        {
            if (aSetHideFlag)
            {
                mHideAfterSlide = false;
                mSlideToScreen = true;
            }
            show();
            mScrollTimer.start (mScrollDelay, this);
        }
        else
            show();

        if (mAutoHide)
            mAutoScrollTimer.start (mAutoScrollDelay, this);
        else
            mAutoScrollTimer.stop();
    }
    else
    {
        if (mAnimated)
        {
            if (aSetHideFlag)
            {
                mHideAfterSlide = true;
                mSlideToScreen = false;
            }
            mScrollTimer.start (mScrollDelay, this);
        }
        else
            hide();

        if (mAutoHide)
            mAutoScrollTimer.start (mAutoScrollDelay, this);
        else
            mAutoScrollTimer.stop();
    }
}

/* Update the display text, usually the VM Name */
void VBoxMiniToolBar::setDisplayText (const QString &aText)
{
    mDisplayLabel->setText (aText);
}

void VBoxMiniToolBar::mouseMoveEvent (QMouseEvent *aEvent)
{
    if (!mHideAfterSlide)
    {
        mSlideToScreen = true;
        mScrollTimer.start (mScrollDelay, this);
    }

    QToolBar::mouseMoveEvent (aEvent);
}

/* Handles auto hide feature of the toolbar */
void VBoxMiniToolBar::timerEvent (QTimerEvent *aEvent)
{
    if (aEvent->timerId() == mScrollTimer.timerId())
    {
        switch (mAlignment)
        {
            case AlignTop:
            {
                if (((mPositionY == 0) && mSlideToScreen) ||
                    ((mPositionY == - height() + 1) && !mSlideToScreen))
                {
                    mScrollTimer.stop();
                    if (mHideAfterSlide)
                    {
                        mHideAfterSlide = false;
                        hide();
                    }
                    return;
                }
                mSlideToScreen ? ++ mPositionY : -- mPositionY;
                break;
            }
            case AlignBottom:
            {
                QRect screen = mIsSeamless ? QApplication::desktop()->availableGeometry (this) :
                                             QApplication::desktop()->screenGeometry (this);
                if (((mPositionY == screen.height() - height()) && mSlideToScreen) ||
                    ((mPositionY == screen.height() - 1) && !mSlideToScreen))
                {
                    mScrollTimer.stop();
                    if (mHideAfterSlide)
                    {
                        mHideAfterSlide = false;
                        hide();
                    }
                    return;
                }
                mSlideToScreen ? -- mPositionY : ++ mPositionY;
                break;
            }
            default:
                break;
        }
        move (mapFromScreen (QPoint (mPositionX, mPositionY)));
        emit geometryUpdated();
    }
    else if (aEvent->timerId() == mAutoScrollTimer.timerId())
    {
        QRect rect = this->rect();
        QPoint cursor_pos = QCursor::pos();
        QPoint p = mapFromGlobal (cursor_pos);

        if (!rect.contains (p))
        {
            ++ mAutoHideCounter;

            if (mAutoHideCounter == mAutoHideTotalCounter)
            {
                mSlideToScreen = false;
                mScrollTimer.start (mScrollDelay, this);
            }
        }
        else
            mAutoHideCounter = 0;
    }
    else
        QWidget::timerEvent (aEvent);
}

void VBoxMiniToolBar::showEvent (QShowEvent *aEvent)
{
    if (!mPolished)
    {
        /* Tool-bar margins */
        foreach (QWidget *margin, mMargins)
            margin->setMinimumWidth (height());

        /* Tool-bar spacings */
        foreach (QWidget *spacing, mSpacings)
            spacing->setMinimumWidth (5);

        /* Title spacings */
        foreach (QWidget *lableMargin, mLabelMargins)
            lableMargin->setMinimumWidth (15);

        /* Resize to sizehint */
        resize (sizeHint());

        /* Initialize */
        recreateMask();
        moveToBase();

        mPolished = true;
    }

    VBoxToolBar::showEvent (aEvent);
}

void VBoxMiniToolBar::paintEvent (QPaintEvent *aEvent)
{
    QPainter painter (this);
    painter.fillRect (aEvent->rect(), palette().brush (QPalette::Window));
    VBoxToolBar::paintEvent (aEvent);
}

void VBoxMiniToolBar::togglePushpin (bool aOn)
{
    mAutoHide = !aOn;
    updateDisplay (!mAutoHide, false);
}

void VBoxMiniToolBar::recreateMask()
{
    int edgeShift = height();
    int points [8];
    switch (mAlignment)
    {
        case AlignTop:
        {
            points [0] = 0;
            points [1] = 0;

            points [2] = edgeShift;
            points [3] = height();

            points [4] = width() - edgeShift;
            points [5] = height();

            points [6] = width();
            points [7] = 0;

            break;
        }
        case AlignBottom:
        {
            points [0] = edgeShift;
            points [1] = 0;

            points [2] = 0;
            points [3] = height();

            points [4] = width();
            points [5] = height();

            points [6] = width() - edgeShift;
            points [7] = 0;

            break;
        }
        default:
            break;
    }
    QPolygon polygon;
    polygon.setPoints (4, points);
    setMask (polygon);
}

void VBoxMiniToolBar::moveToBase()
{
    QRect screen = mIsSeamless ? QApplication::desktop()->availableGeometry (this) :
                                 QApplication::desktop()->screenGeometry (this);
    mPositionX = screen.width() / 2 - width() / 2;
    switch (mAlignment)
    {
        case AlignTop:
        {
            mPositionY = - height() + 1;
            break;
        }
        case AlignBottom:
        {
            mPositionY = screen.height() - 1;
            break;
        }
        default:
        {
            mPositionY = 0;
            break;
        }
    }
    move (mapFromScreen (QPoint (mPositionX, mPositionY)));
}

QPoint VBoxMiniToolBar::mapFromScreen (const QPoint &aPoint)
{
    QPoint globalPosition = parentWidget()->mapFromGlobal (aPoint);
    QRect fullArea = QApplication::desktop()->screenGeometry (this);
    QRect realArea = mIsSeamless ? QApplication::desktop()->availableGeometry (this) :
                                   QApplication::desktop()->screenGeometry (this);
    QPoint shiftToReal (realArea.topLeft() - fullArea.topLeft());
    return globalPosition + shiftToReal;
}

