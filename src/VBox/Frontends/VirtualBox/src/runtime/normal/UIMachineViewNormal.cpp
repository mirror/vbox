/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineViewNormal class implementation
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Global includes */
#include <QApplication>
#include <QDesktopWidget>
#include <QMainWindow>
#include <QMenuBar>
#include <QScrollBar>
#include <QTimer>

/* Local includes */
#include "VBoxGlobal.h"
#include "UISession.h"
#include "UIActionPoolRuntime.h"
#include "UIMachineLogic.h"
#include "UIMachineWindow.h"
#include "UIMachineViewNormal.h"
#include "UIFrameBuffer.h"

UIMachineViewNormal::UIMachineViewNormal(  UIMachineWindow *pMachineWindow
                                         , ulong uScreenId
#ifdef VBOX_WITH_VIDEOHWACCEL
                                         , bool bAccelerate2DVideo
#endif
                                         )
    : UIMachineView(  pMachineWindow
                    , uScreenId
#ifdef VBOX_WITH_VIDEOHWACCEL
                    , bAccelerate2DVideo
#endif
                    )
    , m_bIsGuestAutoresizeEnabled(gActionPool->action(UIActionIndexRuntime_Toggle_GuestAutoresize)->isChecked())
{
    /* Load machine view settings: */
    loadMachineViewSettings();

    /* Prepare viewport: */
    prepareViewport();

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

    /* Initialization: */
    sltMachineStateChanged();
    sltAdditionsStateChanged();
}

UIMachineViewNormal::~UIMachineViewNormal()
{
    /* Save machine view settings: */
    saveMachineViewSettings();

    /* Cleanup frame buffer: */
    cleanupFrameBuffer();
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
        case VBoxDefs::ResizeEventType:
        {
            /* Some situations require framebuffer resize events to be ignored at all,
             * leaving machine-window, machine-view and framebuffer sizes preserved: */
            if (uisession()->isGuestResizeIgnored())
                return true;

            /* Get guest resize-event: */
            UIResizeEvent *pResizeEvent = static_cast<UIResizeEvent*>(pEvent);

            /* Perform framebuffer resize: */
            frameBuffer()->resizeEvent(pResizeEvent);

            /* Reapply maximum size restriction for machine-view: */
            setMaximumSize(sizeHint());

            /* Store the new size to prevent unwanted resize hints being sent back: */
            storeConsoleSize(pResizeEvent->width(), pResizeEvent->height());

            /* Perform machine-view resize: */
            resize(pResizeEvent->width(), pResizeEvent->height());

            /* May be we have to restrict minimum size? */
            maybeRestrictMinimumSize();

            /* Let our toplevel widget calculate its sizeHint properly: */
            QCoreApplication::sendPostedEvents(0, QEvent::LayoutRequest);

#ifdef Q_WS_MAC
            machineLogic()->updateDockIconSize(screenId(), pResizeEvent->width(), pResizeEvent->height());
#endif /* Q_WS_MAC */

            /* Update machine-view sliders: */
            updateSliders();

            /* Normalize machine-window geometry: */
            normalizeGeometry(true /* Adjust Position? */);

            /* Report to the VM thread that we finished resizing: */
            session().GetConsole().GetDisplay().ResizeCompleted(screenId());

            /* We also recalculate the desktop geometry if this is determined
             * automatically. In fact, we only need this on the first resize,
             * but it is done every time to keep the code simpler. */
            calculateDesktopGeometry();

            /* Emit a signal about guest was resized: */
            emit resizeHintDone();

            pEvent->accept();
            return true;
        }

        default:
            break;
    }
    return UIMachineView::event(pEvent);
}

bool UIMachineViewNormal::eventFilter(QObject *pWatched, QEvent *pEvent)
{
    /* Who are we watching? */
    QMainWindow *pMainDialog = machineWindowWrapper() && machineWindowWrapper()->machineWindow() ?
                               qobject_cast<QMainWindow*>(machineWindowWrapper()->machineWindow()) : 0;
#ifdef Q_WS_WIN
    QMenuBar *pMenuBar = pMainDialog ? pMainDialog->menuBar() : 0;
#endif /* Q_WS_WIN */

    if (pWatched != 0 && pWatched == pMainDialog)
    {
        switch (pEvent->type())
        {
            case QEvent::Resize:
            {
                /* Set the "guest needs to resize" hint.
                 * This hint is acted upon when (and only when) the autoresize property is "true": */
                m_fShouldWeDoResize = uisession()->isGuestSupportsGraphics();
                if (pEvent->spontaneous() && m_bIsGuestAutoresizeEnabled && uisession()->isGuestSupportsGraphics())
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

#ifdef Q_WS_WIN
    else if (pWatched != 0 && pWatched == pMenuBar)
    {
        /* Due to windows host uses separate 'focus set' to let menubar to
         * operate while popped up (see UIMachineViewNormal::event() for details),
         * it also requires backward processing: */
        switch (pEvent->type())
        {
            /* If menubar gets the focus while not popped up => give it back: */
            case QEvent::FocusIn:
            {
                if (!QApplication::activePopupWidget())
                    setFocus();
            }
            default:
                break;
        }
    }
#endif /* Q_WS_WIN */

    return UIMachineView::eventFilter(pWatched, pEvent);
}

void UIMachineViewNormal::prepareCommon()
{
    /* Base class common settings: */
    UIMachineView::prepareCommon();

    /* Setup size-policy: */
    setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum));
    /* Maximum size to sizehint: */
    setMaximumSize(sizeHint());
}

void UIMachineViewNormal::prepareFilters()
{
    /* Base class filters: */
    UIMachineView::prepareFilters();

    /* Menu bar filters: */
    qobject_cast<QMainWindow*>(machineWindowWrapper()->machineWindow())->menuBar()->installEventFilter(this);
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
    storeGuestSizeHint(QSize(frameBuffer()->width(), frameBuffer()->height()));
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
#ifndef VBOX_GUI_WITH_CUSTOMIZATIONS1
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

        frameGeo = VBoxGlobal::normalizeGeometry(frameGeo, availableGeo, vboxGlobal().vmRenderMode() != VBoxDefs::SDLMode /* can resize? */);
    }

#if 0
    /* Center the frame on the desktop: */
    frameGeo.moveCenter(availableGeo.center());
#endif

    /* Finally, set the frame geometry */
    pTopLevelWidget->setGeometry(frameGeo.left() + dl, frameGeo.top() + dt, frameGeo.width() - dl - dr, frameGeo.height() - dt - db);

#else /* !VBOX_GUI_WITH_CUSTOMIZATIONS1 */
    Q_UNUSED(bAdjustPosition);
#endif /* VBOX_GUI_WITH_CUSTOMIZATIONS1 */
}

QRect UIMachineViewNormal::workingArea()
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
        QRect centralWidgetGeo = static_cast<QMainWindow*>(machineWindowWrapper()->machineWindow())->centralWidget()->geometry();
        /* To work out how big we can make the console window while still fitting on the desktop,
         * we calculate workingArea() - (windowGeo - centralWidgetGeo).
         * This works because the difference between machine window and machine central widget
         * (or at least its width and height) is a constant. */
        m_desktopGeometry = QSize(workingArea().width() - (windowGeo.width() - centralWidgetGeo.width()),
                                  workingArea().height() - (windowGeo.height() - centralWidgetGeo.height()));
    }
}

void UIMachineViewNormal::maybeRestrictMinimumSize()
{
    /* Sets the minimum size restriction depending on the auto-resize feature state and the current rendering mode.
     * Currently, the restriction is set only in SDL mode and only when the auto-resize feature is inactive.
     * We need to do that because we cannot correctly draw in a scrolled window in SDL mode.
     * In all other modes, or when auto-resize is in force, this function does nothing. */
    if (vboxGlobal().vmRenderMode() == VBoxDefs::SDLMode)
    {
        if (!uisession()->isGuestSupportsGraphics() || !m_bIsGuestAutoresizeEnabled)
            setMinimumSize(sizeHint());
        else
            setMinimumSize(0, 0);
    }
}

