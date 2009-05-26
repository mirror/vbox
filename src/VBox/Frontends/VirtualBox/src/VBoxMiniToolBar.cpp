/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxMiniToolBar class declaration & implementation. This is the toolbar shown on fullscreen mode.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

VBoxMiniToolBar::VBoxMiniToolBar (QList <QMenu*> aMenus)
    : VBoxToolBar (0)
    , mAutoHideCounter (0)
    , mAutoHide (true)
    , mSlideDown (true)
    , mHideAfterSlide (false)
    , mAnimated (true)
    , mScrollDelay (20)
    , mAutoScrollDelay (100)
    , mAutoHideTotalCounter (10)
{
    setWindowFlags (Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);

    /* Make the toolbar transparent */
    //setWindowOpacity (0.3);

    /* Play with toolbar mode as child of the main window, rather than top parentless */
    //setAllowedAreas (Qt::NoToolBarArea);
    //mMainWindow->addToolBar (/* Qt::NoToolBarArea, */ this);

    // TODO, we need to hide the tab icon in taskbar when in seamless mode
    setWindowTitle (tr ("VirtualBox Mini Toolbar"));

    setIconSize (QSize (16, 16));

    /* Left margin of tool-bar */
    QWidget *indent = new QWidget (this);
    indent->setMinimumWidth (25);
    addWidget (indent);

    /* Add pushpin */
    mAutoHideAct = new QAction (this);
    mAutoHideAct->setIcon (VBoxGlobal::iconSet (":/pin_16px.png"));
    mAutoHideAct->setToolTip (tr ("Always show the toolbar"));
    mAutoHideAct->setCheckable (true);
    mAutoHideAct->setChecked (!mAutoHide);
    connect (mAutoHideAct, SIGNAL (toggled (bool)), this, SLOT (togglePushpin (bool)));
    addAction (mAutoHideAct);

    /* Pre-menu spacing */
    indent = new QWidget (this);
    indent->setMinimumWidth (5);
    addWidget (indent);

    /* Add the menus */
    foreach (QMenu *m, aMenus)
    {
        addAction (m->menuAction());

        /* Menu spacing */
        indent = new QWidget (this);
        indent->setMinimumWidth (5);
        addWidget (indent);
    }

    /* Left spacing of VM Name */
    indent = new QWidget (this);
    indent->setMinimumWidth (15);
    addWidget (indent);

    /* Insert a label for VM Name */
    mDisplayLabel = new QLabel (this);
    addWidget (mDisplayLabel);

    /* Right spacing of VM Name */
    indent = new QWidget (this);
    indent->setMinimumWidth (20);
    addWidget (indent);

    /* Restore action */
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
    indent = new QWidget (this);
    indent->setMinimumWidth (25);
    addWidget (indent);

    setMouseTracking (mAutoHide);
    setVisible (false);
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
                mSlideDown = true;
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
                mSlideDown = false;
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
void VBoxMiniToolBar::setDisplayText (const QString& aText)
{
    mDisplayLabel->setText (aText);
}

/* Create polygon shaped toolbar */
void VBoxMiniToolBar::resizeEvent (QResizeEvent* /* aEvent */)
{
    int triangular_x = height();
    int points [8];

    points [0] = 0;
    points [1] = 0;

    points [2] = triangular_x;
    points [3] = height();

    points [4] = width() - triangular_x;
    points [5] = height();

    points [6] = width();
    points [7] = 0;

    QPolygon polygon;
    polygon.setPoints (4, points);

    QRegion maskedRegion (polygon);

    setMask (maskedRegion);

    /* Position to the top center of the desktop */
    QDesktopWidget *d = QApplication::desktop();
    int x = d->width() / 2 - width() / 2;
    int y = - height() + 1;
    move (x, y);
}

void VBoxMiniToolBar::mouseMoveEvent (QMouseEvent *aEvent)
{
    if (!mHideAfterSlide)
    {
        mSlideDown = true;
        mScrollTimer.start (mScrollDelay, this);
    }

    QToolBar::mouseMoveEvent (aEvent);
}

/* Handles auto hide feature of the toolbar */
void VBoxMiniToolBar::timerEvent (QTimerEvent *aEvent)
{
    if (aEvent->timerId() == mScrollTimer.timerId())
    {
        QRect rect = this->rect();
        QPoint p (rect.left(), rect.top());
        QPoint screen_point = mapToGlobal (p);

        int x = screen_point.x();
        int y = screen_point.y();

        if (((screen_point.y() == 0) && mSlideDown) ||
            ((screen_point.y() == - height() + 1) && !mSlideDown))
        {
            mScrollTimer.stop();

            if (mHideAfterSlide)
            {
                mHideAfterSlide = false;
                hide();
            }
            return;
        }

        if (mSlideDown)
            ++ y;
        else
            -- y;

        move (x, y);
    }
    else if (aEvent->timerId() == mAutoScrollTimer.timerId())
    {
        QRect rect = this->rect();
        QPoint cursor_pos = QCursor::pos();
        QPoint p = mapFromGlobal (cursor_pos);

        if (!rect.contains(p)) // cursor_pos.y() <= height())
        {
            ++ mAutoHideCounter;

            if (mAutoHideCounter == mAutoHideTotalCounter)
            {
                mSlideDown = false;
                mScrollTimer.start (mScrollDelay, this);
            }
        }
        else
            mAutoHideCounter = 0;
    }
    else
        QWidget::timerEvent (aEvent);
}

void VBoxMiniToolBar::togglePushpin (bool aOn)
{
    mAutoHide = !aOn;
    updateDisplay (!mAutoHide, false);
}

