/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * VBoxMiniToolBar class declaration & implementation. This is the toolbar shown on fullscreen mode.
 */

/*
 * Copyright (C) 2009-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Local includes */
#include "UIIconPool.h"
#include "VBoxGlobal.h"
#include "VBoxMiniToolBar.h"

/* Global includes */
#include <QCursor>
#include <QDesktopWidget>
#include <QLabel>
#include <QMenu>
#include <QPaintEvent>
#include <QPainter>
#include <QPolygon>
#include <QRect>
#include <QRegion>
#include <QTimer>
#include <QToolButton>

/* Mini-toolbar constructor */
VBoxMiniToolBar::VBoxMiniToolBar(QWidget *pParent, Alignment alignment, bool fActive, bool fAutoHide)
    : UIToolBar(pParent)
    , m_pAutoHideAction(0)
    , m_pDisplayLabel(0)
    , m_pMinimizeAction(0)
    , m_pRestoreAction(0)
    , m_pCloseAction(0)
    , m_fActive(fActive)
    , m_fPolished(false)
    , m_fSeamless(false)
    , m_fAutoHide(fAutoHide)
    , m_fSlideToScreen(true)
    , m_fHideAfterSlide(false)
    , m_iAutoHideCounter(0)
    , m_iPositionX(0)
    , m_iPositionY(0)
    , m_pInsertPosition(0)
    , m_alignment(alignment)
    , m_fAnimated(true)
    , m_iScrollDelay(10)
    , m_iAutoScrollDelay(100)
    , m_iAutoHideTotalCounter(10)
{
    /* Check parent widget presence: */
    AssertMsg(parentWidget(), ("Parent widget must be set!\n"));

    /* Toolbar options: */
    setIconSize(QSize(16, 16));
    setVisible(false);

    /* Add pushpin: */
    m_pAutoHideAction = new QAction(this);
    m_pAutoHideAction->setIcon(UIIconPool::iconSet(":/pin_16px.png"));
    m_pAutoHideAction->setToolTip(tr("Always show the toolbar"));
    m_pAutoHideAction->setCheckable(true);
    m_pAutoHideAction->setChecked(!m_fAutoHide);
    connect(m_pAutoHideAction, SIGNAL(toggled(bool)), this, SLOT(togglePushpin(bool)));
    addAction(m_pAutoHideAction);

    /* Left menu margin: */
    m_Spacings << widgetForAction(addWidget(new QWidget(this)));

    /* Right menu margin: */
    m_pInsertPosition = addWidget(new QWidget(this));
    m_Spacings << widgetForAction(m_pInsertPosition);

    /* Left label margin: */
    m_LabelMargins << widgetForAction(addWidget(new QWidget(this)));

    /* Insert a label for VM Name: */
    m_pDisplayLabel = new QLabel(this);
    m_pDisplayLabel->setAlignment(Qt::AlignCenter);
    addWidget(m_pDisplayLabel);

    /* Right label margin: */
    m_LabelMargins << widgetForAction(addWidget(new QWidget(this)));

    /* Minimize action: */
    m_pMinimizeAction = new QAction(this);
    m_pMinimizeAction->setIcon(UIIconPool::iconSet(":/minimize_16px.png"));
    m_pMinimizeAction->setToolTip(tr("Minimize Window"));
    connect(m_pMinimizeAction, SIGNAL(triggered()), this, SIGNAL(minimizeAction()));
    addAction(m_pMinimizeAction);

    /* Exit action: */
    m_pRestoreAction = new QAction(this);
    m_pRestoreAction->setIcon(UIIconPool::iconSet(":/restore_16px.png"));
    m_pRestoreAction->setToolTip(tr("Exit Full Screen or Seamless Mode"));
    connect(m_pRestoreAction, SIGNAL(triggered()), this, SIGNAL(exitAction()));
    addAction(m_pRestoreAction);

    /* Close action: */
    m_pCloseAction = new QAction(this);
    m_pCloseAction->setIcon(UIIconPool::iconSet(":/close_16px.png"));
    m_pCloseAction->setToolTip(tr("Close VM"));
    connect(m_pCloseAction, SIGNAL(triggered()), this, SIGNAL(closeAction()));
    addAction(m_pCloseAction);

    /* Event-filter for parent widget to control resize: */
    pParent->installEventFilter(this);

    /* Enable mouse-tracking for this & children allowing to get mouse-move events: */
    setMouseTrackingEnabled(m_fAutoHide);
}

/* Appends passed menus into internal menu-list */
VBoxMiniToolBar& VBoxMiniToolBar::operator<<(QList<QMenu*> menus)
{
    for (int i = 0; i < menus.size(); ++i)
    {
        QAction *pAction = menus[i]->menuAction();
        insertAction(m_pInsertPosition, pAction);
        if (QToolButton *pButton = qobject_cast<QToolButton*>(widgetForAction(pAction)))
        {
            pButton->setPopupMode(QToolButton::InstantPopup);
            pButton->setAutoRaise(true);
        }
        if (i != menus.size() - 1)
            m_Spacings << widgetForAction(insertWidget(m_pInsertPosition, new QWidget(this)));
    }
    return *this;
}

/* Seamless mode setter */
void VBoxMiniToolBar::setSeamlessMode(bool fSeamless)
{
    m_fSeamless = fSeamless;
}

/* Update the display text, usually the VM Name */
void VBoxMiniToolBar::setDisplayText(const QString &strText)
{
    /* If text was really changed: */
    if (m_pDisplayLabel->text() != strText)
    {
        /* Update toolbar label: */
        m_pDisplayLabel->setText(strText);

        /* Reinitialize: */
        initialize();

        /* Update toolbar if its not hidden: */
        if (!isHidden())
            updateDisplay(!m_fAutoHide, false);
    }
}

/* Is auto-hide feature enabled? */
bool VBoxMiniToolBar::isAutoHide() const
{
    return m_fAutoHide;
}

void VBoxMiniToolBar::updateDisplay(bool fShow, bool fSetHideFlag)
{
    m_iAutoHideCounter = 0;

    setMouseTrackingEnabled(m_fAutoHide);

    if (fShow)
    {
        if (isHidden())
            moveToBase();

        if (m_fAnimated)
        {
            if (fSetHideFlag)
            {
                m_fHideAfterSlide = false;
                m_fSlideToScreen = true;
            }
            if (m_fActive)
                show();
            m_scrollTimer.start(m_iScrollDelay, this);
        }
        else if (m_fActive)
            show();

        if (m_fAutoHide)
            m_autoScrollTimer.start(m_iAutoScrollDelay, this);
        else
            m_autoScrollTimer.stop();
    }
    else
    {
        if (m_fAnimated)
        {
            if (fSetHideFlag)
            {
                m_fHideAfterSlide = true;
                m_fSlideToScreen = false;
            }
            m_scrollTimer.start(m_iScrollDelay, this);
        }
        else
            hide();

        if (m_fAutoHide)
            m_autoScrollTimer.start(m_iAutoScrollDelay, this);
        else
            m_autoScrollTimer.stop();
    }
}

/* Parent widget event-filter */
bool VBoxMiniToolBar::eventFilter(QObject *pObject, QEvent *pEvent)
{
    /* If parent widget was resized: */
    if (pObject == parent() && pEvent->type() == QEvent::Resize)
    {
        /* Update toolbar position: */
        moveToBase();
        return true;
    }
    /* Base-class event-filter: */
    return UIToolBar::eventFilter(pObject, pEvent);
}

/* Mouse-move event processor */
void VBoxMiniToolBar::mouseMoveEvent(QMouseEvent *pEvent)
{
    /* Activate sliding animation on mouse move: */
    if (!m_fHideAfterSlide)
    {
        m_fSlideToScreen = true;
        m_scrollTimer.start(m_iScrollDelay, this);
    }
    /* Base-class mouse-move event processing: */
    UIToolBar::mouseMoveEvent(pEvent);
}

/* Timer event processor
 * Handles auto hide feature of the toolbar */
void VBoxMiniToolBar::timerEvent(QTimerEvent *pEvent)
{
    if (pEvent->timerId() == m_scrollTimer.timerId())
    {
        /* Due to X11 async nature, this timer-event could come before parent
         * VM window become visible, we should ignore those timer-events: */
        if (QApplication::desktop()->screenNumber(window()) == -1)
            return;

        /* Update tool-bar position: */
        QRect screen = m_fSeamless ? vboxGlobal().availableGeometry(QApplication::desktop()->screenNumber(window())) :
                                     QApplication::desktop()->screenGeometry(window());
        switch (m_alignment)
        {
            case AlignTop:
            {
                if (((m_iPositionY == screen.y()) && m_fSlideToScreen) ||
                    ((m_iPositionY == screen.y() - height() + 1) && !m_fSlideToScreen))
                {
                    m_scrollTimer.stop();
                    if (m_fHideAfterSlide)
                    {
                        m_fHideAfterSlide = false;
                        hide();
                    }
                    return;
                }
                m_fSlideToScreen ? ++m_iPositionY : --m_iPositionY;
                break;
            }
            case AlignBottom:
            {
                if (((m_iPositionY == screen.y() + screen.height() - height()) && m_fSlideToScreen) ||
                    ((m_iPositionY == screen.y() + screen.height() - 1) && !m_fSlideToScreen))
                {
                    m_scrollTimer.stop();
                    if (m_fHideAfterSlide)
                    {
                        m_fHideAfterSlide = false;
                        hide();
                    }
                    return;
                }
                m_fSlideToScreen ? --m_iPositionY : ++m_iPositionY;
                break;
            }
            default:
                break;
        }
        move(parentWidget()->mapFromGlobal(QPoint(m_iPositionX, m_iPositionY)));
        emit geometryUpdated();
    }
    else if (pEvent->timerId() == m_autoScrollTimer.timerId())
    {
        QRect rect = this->rect();
        QPoint p = mapFromGlobal(QCursor::pos());
        if (!rect.contains(p))
        {
            ++m_iAutoHideCounter;

            if (m_iAutoHideCounter == m_iAutoHideTotalCounter)
            {
                m_fSlideToScreen = false;
                m_scrollTimer.start(m_iScrollDelay, this);
            }
        }
        else
            m_iAutoHideCounter = 0;
    }
    else
        QWidget::timerEvent(pEvent);
}

/* Show event processor */
void VBoxMiniToolBar::showEvent(QShowEvent *pEvent)
{
    if (!m_fPolished)
    {
        /* Tool-bar spacings: */
        foreach(QWidget *pSpacing, m_Spacings)
            pSpacing->setMinimumWidth(5);

        /* Title spacings: */
        foreach(QWidget *pLableMargin, m_LabelMargins)
            pLableMargin->setMinimumWidth(15);

        /* Initialize: */
        initialize();

        m_fPolished = true;
    }
    /* Base-class show event processing: */
    UIToolBar::showEvent(pEvent);
}

/* Show event processor */
void VBoxMiniToolBar::paintEvent(QPaintEvent *pEvent)
{
    /* Paint background */
    QPainter painter;
    painter.begin(this);
    painter.fillRect(pEvent->rect(), QApplication::palette().color(QPalette::Active, QPalette::Window));
    painter.end();
    /* Base-class paint event processing: */
    UIToolBar::paintEvent(pEvent);
}

/* Toggle push-pin */
void VBoxMiniToolBar::togglePushpin(bool fOn)
{
    m_fAutoHide = !fOn;
    updateDisplay(!m_fAutoHide, false);
}

/* Initialize mini-toolbar */
void VBoxMiniToolBar::initialize()
{
    /* Resize to sizehint: */
    resize(sizeHint());

    /* Update geometry: */
    moveToBase();
}

/* Move mini-toolbar to the base location */
void VBoxMiniToolBar::moveToBase()
{
    QRect screen = m_fSeamless ? vboxGlobal().availableGeometry(QApplication::desktop()->screenNumber(window())) :
                                 QApplication::desktop()->screenGeometry(window());
    m_iPositionX = screen.x() + (screen.width() / 2) - (width() / 2);
    switch (m_alignment)
    {
        case AlignTop:
        {
            m_iPositionY = screen.y() - height() + 1;
            break;
        }
        case AlignBottom:
        {
            m_iPositionY = screen.y() + screen.height() - 1;
            break;
        }
        default:
        {
            m_iPositionY = 0;
            break;
        }
    }
    move(parentWidget()->mapFromGlobal(QPoint(m_iPositionX, m_iPositionY)));
}

/* Enable/disable mouse-tracking for required widgets */
void VBoxMiniToolBar::setMouseTrackingEnabled(bool fEnabled)
{
    setMouseTracking(fEnabled);
    if (m_pDisplayLabel)
        m_pDisplayLabel->setMouseTracking(fEnabled);
    if (m_pAutoHideAction && widgetForAction(m_pAutoHideAction))
        widgetForAction(m_pAutoHideAction)->setMouseTracking(fEnabled);
    if (m_pMinimizeAction && widgetForAction(m_pMinimizeAction))
        widgetForAction(m_pMinimizeAction)->setMouseTracking(fEnabled);
    if (m_pRestoreAction && widgetForAction(m_pRestoreAction))
        widgetForAction(m_pRestoreAction)->setMouseTracking(fEnabled);
    if (m_pCloseAction && widgetForAction(m_pCloseAction))
        widgetForAction(m_pCloseAction)->setMouseTracking(fEnabled);
}

