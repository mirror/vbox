/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineViewNormal class implementation
 */

/*
 * Copyright (C) 2010 Sun Microsystems, Inc.
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
#include <QDesktopWidget>
#include <QMenuBar>
#include <QScrollBar>
#include <QTimer>

/* Local includes */
#include "VBoxGlobal.h"
#include "UISession.h"
#include "UIActionsPool.h"
#include "UIMachineLogic.h"
#include "UIMachineWindow.h"
#include "UIFrameBuffer.h"
#include "UIMachineViewNormal.h"
#include "QIMainDialog.h"

UIMachineViewNormal::UIMachineViewNormal(  UIMachineWindow *pMachineWindow
                                         , VBoxDefs::RenderMode renderMode
#ifdef VBOX_WITH_VIDEOHWACCEL
                                         , bool bAccelerate2DVideo
#endif
                                         , ulong uMonitor)
    : UIMachineView(  pMachineWindow
                    , renderMode
#ifdef VBOX_WITH_VIDEOHWACCEL
                    , bAccelerate2DVideo
#endif
                    , uMonitor)
    , m_bIsGuestAutoresizeEnabled(pMachineWindow->machineLogic()->actionsPool()->action(UIActionIndex_Toggle_GuestAutoresize)->isChecked())
    , m_fShouldWeDoResize(false)
{
    /* Prepare frame buffer: */
    prepareFrameBuffer();

    /* Prepare common things: */
    prepareCommon();

    /* Prepare event-filters: */
    prepareFilters();

    /* Prepare connections: */
    prepareConnections();

    /* Prepare console connections: */
    prepareConsoleConnections();

    /* Load machine view settings: */
    loadMachineViewSettings();

    /* Initialization: */
    sltMachineStateChanged();
    sltAdditionsStateChanged();
    sltMousePointerShapeChanged();
    sltMouseCapabilityChanged();
}

UIMachineViewNormal::~UIMachineViewNormal()
{
    /* Save machine view settings: */
    saveMachineViewSettings();

    /* Cleanup common things: */
    cleanupCommon();

    /* Cleanup frame buffer: */
    cleanupFrameBuffer();
}

void UIMachineViewNormal::sltPerformGuestResize(const QSize &toSize)
{
    if (m_bIsGuestAutoresizeEnabled && uisession()->isGuestSupportsGraphics())
    {
        /* Get machine window: */
        QIMainDialog *pMachineWindow = machineWindowWrapper() && machineWindowWrapper()->machineWindow() ?
                                       qobject_cast<QIMainDialog*>(machineWindowWrapper()->machineWindow()) : 0;

        /* If this slot is invoked directly then use the passed size otherwise get
         * the available size for the guest display. We assume here that centralWidget()
         * contains this view only and gives it all available space: */
        QSize newSize(toSize.isValid() ? toSize : pMachineWindow ? pMachineWindow->centralWidget()->size() : QSize());
        AssertMsg(newSize.isValid(), ("Size should be valid!\n"));

        /* Do not send the same hints as we already have: */
        if ((newSize.width() == storedConsoleSize().width()) && (newSize.height() == storedConsoleSize().height()))
            return;

        /* We only actually send the hint if either an explicit new size was given
         * (e.g. if the request was triggered directly by a console resize event) or
         * if no explicit size was specified but a resize is flagged as being needed
         * (e.g. the autoresize was just enabled and the console was resized while it was disabled). */
        if (toSize.isValid() || m_fShouldWeDoResize)
        {
            /* Remember the new size: */
            storeConsoleSize(newSize.width(), newSize.height());

            /* Send new size-hint to the guest: */
            session().GetConsole().GetDisplay().SetVideoModeHint(newSize.width(), newSize.height(), 0, screenId());
        }

        /* We had requested resize now, rejecting other accident requests: */
        m_fShouldWeDoResize = false;
    }
}

void UIMachineViewNormal::sltAdditionsStateChanged()
{
    /* Check if we should restrict minimum size: */
    maybeRestrictMinimumSize();
}

void UIMachineViewNormal::sltDesktopResized()
{
    /* If the desktop geometry is set automatically, this will update it: */
    calculateDesktopGeometry();
}

bool UIMachineViewNormal::event(QEvent *pEvent)
{
    switch (pEvent->type())
    {
        case QEvent::KeyPress:
        case QEvent::KeyRelease:
        {
            /* Get key-event: */
            QKeyEvent *pKeyEvent = static_cast<QKeyEvent*>(pEvent);

            /* Process Host+Home as menu-bar activator: */
            if (isHostKeyPressed() && pEvent->type() == QEvent::KeyPress)
            {
                if (pKeyEvent->key() == Qt::Key_Home)
                {
                    /* Trying to get menu-bar: */
                    QMenuBar *pMenuBar = machineWindowWrapper() && machineWindowWrapper()->machineWindow() ?
                                         qobject_cast<QIMainDialog*>(machineWindowWrapper()->machineWindow())->menuBar() : 0;

                    /* If menu-bar is present and have actions: */
                    if (pMenuBar && !pMenuBar->actions().isEmpty())
                    {
                        /* If 'active' action is NOT chosen: */
                        if (!pMenuBar->activeAction())
                            /* Set first menu-bar action as 'active': */
                            pMenuBar->setActiveAction(pMenuBar->actions()[0]);

                        /* If 'active' action is chosen: */
                        if (pMenuBar->activeAction())
                        {
                            /* Activate 'active' menu-bar action: */
                            pMenuBar->activeAction()->activate(QAction::Trigger);

                            /* Accept this event: */
                            pEvent->accept();
                            return true;
                        }
                    }
                }
                else
                    pEvent->ignore();
            }
        }
        default:
            break;
    }
    return UIMachineView::event(pEvent);
}

bool UIMachineViewNormal::eventFilter(QObject *pWatched, QEvent *pEvent)
{
    /* Who are we watching? */
    QIMainDialog *pMainDialog = machineWindowWrapper() && machineWindowWrapper()->machineWindow() ?
                                qobject_cast<QIMainDialog*>(machineWindowWrapper()->machineWindow()) : 0;

    if (pWatched != 0 && pWatched == pMainDialog)
    {
        switch (pEvent->type())
        {
            case QEvent::Resize:
            {
                /* Set the "guest needs to resize" hint.
                 * This hint is acted upon when (and only when) the autoresize property is "true": */
                m_fShouldWeDoResize = uisession()->isGuestSupportsGraphics();
                if (!isMachineWindowResizeIgnored() && m_bIsGuestAutoresizeEnabled && uisession()->isGuestSupportsGraphics())
                    QTimer::singleShot(300, this, SLOT(sltPerformGuestResize()));
                break;
            }
#if defined (Q_WS_WIN32)
# if defined (VBOX_GUI_USE_DDRAW)
            case QEvent::Move:
            {
                /* Notification from our parent that it has moved. We need this in order
                 * to possibly adjust the direct screen blitting: */
                if (frameBuffer())
                    frameBuffer()->moveEvent(static_cast<QMoveEvent*>(pEvent));
                break;
            }
# endif /* defined (VBOX_GUI_USE_DDRAW) */
#endif /* defined (Q_WS_WIN32) */
            default:
                break;
        }
    }

    return UIMachineView::eventFilter(pWatched, pEvent);
}

void UIMachineViewNormal::prepareFilters()
{
    /* Base class filters: */
    UIMachineView::prepareFilters();

    /* Menu bar filters: */
    qobject_cast<QIMainDialog*>(machineWindowWrapper()->machineWindow())->menuBar()->installEventFilter(this);
}

void UIMachineViewNormal::prepareConnections()
{
    connect(QApplication::desktop(), SIGNAL(resized(int)), this, SLOT(sltDesktopResized()));
}

void UIMachineViewNormal::prepareConsoleConnections()
{
    /* Base class connections: */
    UIMachineView::prepareConsoleConnections();

    /* Guest additions state-change updater: */
    connect(uisession(), SIGNAL(sigAdditionsStateChange()), this, SLOT(sltAdditionsStateChanged()));
}

void UIMachineViewNormal::saveMachineViewSettings()
{
    /* Store guest size hint: */
    uisession()->setGuestSizeHint(screenId(), QSize(frameBuffer()->width(), frameBuffer()->height()));
}

void UIMachineViewNormal::setGuestAutoresizeEnabled(bool fEnabled)
{
    if (m_bIsGuestAutoresizeEnabled != fEnabled)
    {
        m_bIsGuestAutoresizeEnabled = fEnabled;

        maybeRestrictMinimumSize();

        if (m_bIsGuestAutoresizeEnabled && uisession()->isGuestSupportsGraphics())
            sltPerformGuestResize();
    }
}

void UIMachineViewNormal::normalizeGeometry(bool bAdjustPosition)
{
    QWidget *pTopLevelWidget = window();

    /* Make no normalizeGeometry in case we are in manual resize mode or main window is maximized: */
    if (pTopLevelWidget->isMaximized())
        return;

    /* Calculate client window offsets: */
    QRect frameGeo = pTopLevelWidget->frameGeometry();
    QRect geo = pTopLevelWidget->geometry();
    int dl = geo.left() - frameGeo.left();
    int dt = geo.top() - frameGeo.top();
    int dr = frameGeo.right() - geo.right();
    int db = frameGeo.bottom() - geo.bottom();

    /* Get the best size w/o scroll bars: */
    QSize s = pTopLevelWidget->sizeHint();

    /* Resize the frame to fit the contents: */
    s -= pTopLevelWidget->size();
    frameGeo.setRight(frameGeo.right() + s.width());
    frameGeo.setBottom(frameGeo.bottom() + s.height());

    if (bAdjustPosition)
    {
        QRegion availableGeo;
        QDesktopWidget *dwt = QApplication::desktop();
        if (dwt->isVirtualDesktop())
            /* Compose complex available region */
            for (int i = 0; i < dwt->numScreens(); ++ i)
                availableGeo += dwt->availableGeometry(i);
        else
            /* Get just a simple available rectangle */
            availableGeo = dwt->availableGeometry(pTopLevelWidget->pos());

        frameGeo = VBoxGlobal::normalizeGeometry(frameGeo, availableGeo, mode() != VBoxDefs::SDLMode /* can resize? */);
    }

#if 0
    /* Center the frame on the desktop: */
    frameGeo.moveCenter(availableGeo.center());
#endif

    /* Finally, set the frame geometry */
    pTopLevelWidget->setGeometry(frameGeo.left() + dl, frameGeo.top() + dt, frameGeo.width() - dl - dr, frameGeo.height() - dt - db);
}

QRect UIMachineViewNormal::availableGeometry()
{
    return QApplication::desktop()->availableGeometry(this);
}

void UIMachineViewNormal::calculateDesktopGeometry()
{
    /* This method should not get called until we have initially set up the desktop geometry type: */
    Assert((desktopGeometryType() != DesktopGeo_Invalid));
    /* If we are not doing automatic geometry calculation then there is nothing to do: */
    if (desktopGeometryType() == DesktopGeo_Automatic)
    {
        /* The area taken up by the machine window on the desktop,
         * including window frame, title, menu bar and status bar: */
        QRect windowGeo = machineWindowWrapper()->machineWindow()->frameGeometry();
        /* The area taken up by the machine central widget, so excluding all decorations: */
        QRect centralWidgetGeo = static_cast<QIMainDialog*>(machineWindowWrapper()->machineWindow())->centralWidget()->geometry();
        /* To work out how big we can make the console window while still fitting on the desktop,
         * we calculate availableGeometry() - (windowGeo - centralWidgetGeo).
         * This works because the difference between machine window and machine central widget
         * (or at least its width and height) is a constant. */
        m_desktopGeometry = QSize(availableGeometry().width() - (windowGeo.width() - centralWidgetGeo.width()),
                                  availableGeometry().height() - (windowGeo.height() - centralWidgetGeo.height()));
    }
}

void UIMachineViewNormal::maybeRestrictMinimumSize()
{
    /* Sets the minimum size restriction depending on the auto-resize feature state and the current rendering mode.
     * Currently, the restriction is set only in SDL mode and only when the auto-resize feature is inactive.
     * We need to do that because we cannot correctly draw in a scrolled window in SDL mode.
     * In all other modes, or when auto-resize is in force, this function does nothing. */
    if (mode() == VBoxDefs::SDLMode)
    {
        if (!uisession()->isGuestSupportsGraphics() || !m_bIsGuestAutoresizeEnabled)
            setMinimumSize(sizeHint());
        else
            setMinimumSize(0, 0);
    }
}

void UIMachineViewNormal::updateSliders()
{
    QSize p = viewport()->size();
    QSize m = maximumViewportSize();

    QSize v = QSize(frameBuffer()->width(), frameBuffer()->height());
    /* No scroll bars needed: */
    if (m.expandedTo(v) == m)
        p = m;

    horizontalScrollBar()->setRange(0, v.width() - p.width());
    verticalScrollBar()->setRange(0, v.height() - p.height());
    horizontalScrollBar()->setPageStep(p.width());
    verticalScrollBar()->setPageStep(p.height());
}

