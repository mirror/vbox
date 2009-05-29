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
#include <QPolygon>
#include <QRect>
#include <QRegion>
#include <QTimer>

VBoxMiniToolBar::VBoxMiniToolBar (Alignment aAlignment)
    : VBoxToolBar (0)
    , mAutoHideCounter (0)
    , mAutoHide (true)
    , mSlideToScreen (true)
    , mHideAfterSlide (false)
    , mPolished (false)
    , mAlignment (aAlignment)
    , mAnimated (true)
    , mScrollDelay (20)
    , mAutoScrollDelay (100)
    , mAutoHideTotalCounter (10)
{
    /* Play with toolbar mode as child of the main window, rather than top parentless */
    //setAllowedAreas (Qt::NoToolBarArea);
    //mMainWindow->addToolBar (/* Qt::NoToolBarArea, */ this);

    setWindowFlags (Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setIconSize (QSize (16, 16));
    setMouseTracking (mAutoHide);
    setVisible (false);

    // TODO, we need to hide the tab icon in taskbar when in seamless mode
    setWindowTitle (tr ("VirtualBox Mini Toolbar"));

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
    closeAct->setIcon (VBoxGlobal::iconSet (":/delete_16px.png"));
    closeAct->setToolTip (tr ("Close VM"));
    connect (closeAct, SIGNAL (triggered()), this, SIGNAL (closeAction()));
    addAction (closeAct);

    /* Right margin of tool-bar */
    mMargins << widgetForAction (addWidget (new QWidget (this)));
}

VBoxMiniToolBar& VBoxMiniToolBar::operator<< (QList <QMenu*> aMenus)
{
    for (int i = 0; i < aMenus.size(); ++ i)
    {
        insertAction (mInsertPosition, aMenus [i]->menuAction());
        if (i != aMenus.size() - 1)
        {
            QWidget *spacer = new QWidget (this);
            insertWidget (mInsertPosition, spacer);
            mSpacings << spacer;
        }
    }
    return *this;
}

void VBoxMiniToolBar::updateDisplay (bool aShow, bool aSetHideFlag)
{
    mAutoHideCounter = 0;

    setMouseTracking (mAutoHide);

    if (aShow)
    {
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

void VBoxMiniToolBar::resizeEvent (QResizeEvent*)
{
    /* Create polygon shaped toolbar */
    int triangular_x = height();
    int points [8];
    switch (mAlignment)
    {
        case AlignTop:
        {
            points [0] = 0;
            points [1] = 0;

            points [2] = triangular_x;
            points [3] = height();

            points [4] = width() - triangular_x;
            points [5] = height();

            points [6] = width();
            points [7] = 0;

            break;
        }
        case AlignBottom:
        {
            points [0] = triangular_x;
            points [1] = 0;

            points [2] = 0;
            points [3] = height();

            points [4] = width();
            points [5] = height();

            points [6] = width() - triangular_x;
            points [7] = 0;

            break;
        }
        default:
            break;
    }
    QPolygon polygon;
    polygon.setPoints (4, points);
    setMask (polygon);

    /* Set the initial position */
    QRect d = QApplication::desktop()->screenGeometry (this);
    mPositionX = d.width() / 2 - width() / 2;
    switch (mAlignment)
    {
        case AlignTop:
        {
            mPositionY = - height() + 1;
            break;
        }
        case AlignBottom:
        {
            mPositionY = d.height() - 1;
            break;
        }
        default:
        {
            mPositionY = 0;
            break;
        }
    }
    move (mPositionX, mPositionY);
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
                QRect d = QApplication::desktop()->screenGeometry (this);
                if (((mPositionY == d.height() - height()) && mSlideToScreen) ||
                    ((mPositionY == d.height() - 1) && !mSlideToScreen))
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
        move (mPositionX, mPositionY);
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

        mPolished = true;
    }

    VBoxToolBar::showEvent (aEvent);
}

void VBoxMiniToolBar::togglePushpin (bool aOn)
{
    mAutoHide = !aOn;
    updateDisplay (!mAutoHide, false);
}

