/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineViewSeamless class implementation
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
#include <QMainWindow>
#include <QTimer>
#ifdef Q_WS_MAC
#include <QMenuBar>
#endif
#ifdef Q_WS_X11
#include <limits.h>
#endif

/* Local includes */
#include "VBoxGlobal.h"
#include "UISession.h"
#include "UIMachineWindow.h"
#include "UIMachineLogic.h"
#include "UIFrameBuffer.h"
#include "UIMachineLogicSeamless.h"
#include "UIMachineViewSeamless.h"

UIMachineViewSeamless::UIMachineViewSeamless(  UIMachineWindow *pMachineWindow
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
    , m_fShouldWeDoResize(false)
    , m_pSyncBlocker(0)
{
    /* Load machine view settings: */
    loadMachineViewSettings();

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

    /* Prepare seamless view: */
    prepareSeamless();

    /* Initialization: */
    sltMachineStateChanged();
    sltAdditionsStateChanged();
    sltMousePointerShapeChanged();
    sltMouseCapabilityChanged();
}

UIMachineViewSeamless::~UIMachineViewSeamless()
{
    /* Cleanup seamless mode: */
    cleanupSeamless();

    /* Cleanup common things: */
    cleanupCommon();

    /* Cleanup frame buffer: */
    cleanupFrameBuffer();
}

void UIMachineViewSeamless::sltPerformGuestResize(const QSize &toSize)
{
    if (uisession()->isGuestSupportsGraphics())
    {
        /* Get machine window: */
        QMainWindow *pMachineWindow = machineWindowWrapper() && machineWindowWrapper()->machineWindow() ?
                                      qobject_cast<QMainWindow*>(machineWindowWrapper()->machineWindow()) : 0;

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

void UIMachineViewSeamless::sltAdditionsStateChanged()
{
    // TODO: Exit seamless if additions doesn't support it!
}

void UIMachineViewSeamless::sltDesktopResized()
{
    // TODO: Try to resize framebuffer according new desktop size, exit seamless if resize is failed!

    /* If the desktop geometry is set automatically, this will update it: */
    calculateDesktopGeometry();
}

bool UIMachineViewSeamless::event(QEvent *pEvent)
{
    switch (pEvent->type())
    {
        case VBoxDefs::SetRegionEventType:
        {
            /* Get region-update event: */
            UISetRegionEvent *pSetRegionEvent = static_cast<UISetRegionEvent*>(pEvent);

            /* Apply new region: */
            if (pSetRegionEvent->region() != m_lastVisibleRegion)
            {
                m_lastVisibleRegion = pSetRegionEvent->region();
                machineWindowWrapper()->setMask(m_lastVisibleRegion);
            }
            return true;
        }

        case QEvent::KeyPress:
        case QEvent::KeyRelease:
        {
            /* Get key-event: */
            QKeyEvent *pKeyEvent = static_cast<QKeyEvent*>(pEvent);

            /* Process Host+Home for menu popup: */
            if (isHostKeyPressed() && pEvent->type() == QEvent::KeyPress)
            {
                if (pKeyEvent->key() == Qt::Key_Home)
                    QTimer::singleShot(0, machineWindowWrapper()->machineWindow(), SLOT(sltPopupMainMenu()));
                else
                    pEvent->ignore();
            }
        }

        case VBoxDefs::ResizeEventType:
        {
            /* Unlock after processing guest resize event: */
            bool fResult = UIMachineView::event(pEvent);
            if (m_pSyncBlocker && m_pSyncBlocker->isRunning())
                m_pSyncBlocker->quit();
            return fResult;
        }

        default:
            break;
    }
    return UIMachineView::event(pEvent);
}

bool UIMachineViewSeamless::eventFilter(QObject *pWatched, QEvent *pEvent)
{
    /* Who are we watching? */
    QMainWindow *pMainDialog = machineWindowWrapper() && machineWindowWrapper()->machineWindow() ?
                               qobject_cast<QMainWindow*>(machineWindowWrapper()->machineWindow()) : 0;

    if (pWatched != 0 && pWatched == pMainDialog)
    {
        switch (pEvent->type())
        {
            case QEvent::Resize:
            {
                /* Send guest-resize hint only if top window resizing to required dimension: */
                QResizeEvent *pResizeEvent = static_cast<QResizeEvent*>(pEvent);
                if (pResizeEvent->size() != workingArea().size())
                    break;

                /* Set the "guest needs to resize" hint.
                 * This hint is acted upon when (and only when) the autoresize property is "true": */
                m_fShouldWeDoResize = uisession()->isGuestSupportsGraphics();
                if (m_fShouldWeDoResize)
                    QTimer::singleShot(0, this, SLOT(sltPerformGuestResize()));
                break;
            }
            default:
                break;
        }
    }

    return UIMachineView::eventFilter(pWatched, pEvent);
}

void UIMachineViewSeamless::prepareCommon()
{
    /* Base class common settings: */
    UIMachineView::prepareCommon();

    /* Minimum size is ignored: */
    setMinimumSize(0, 0);
    /* No scrollbars: */
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

void UIMachineViewSeamless::prepareFilters()
{
    /* Base class filters: */
    UIMachineView::prepareFilters();

#ifdef Q_WS_MAC // TODO: Is it really needed? See UIMachineViewSeamless::eventFilter(...);
    /* Menu bar filter: */
    qobject_cast<QMainWindow*>(machineWindowWrapper()->machineWindow())->menuBar()->installEventFilter(this);
#endif
}

void UIMachineViewSeamless::prepareConnections()
{
    connect(QApplication::desktop(), SIGNAL(resized(int)), this, SLOT(sltDesktopResized()));
}

void UIMachineViewSeamless::prepareConsoleConnections()
{
    /* Base class connections: */
    UIMachineView::prepareConsoleConnections();

    /* Guest additions state-change updater: */
    connect(uisession(), SIGNAL(sigAdditionsStateChange()), this, SLOT(sltAdditionsStateChanged()));
}

void UIMachineViewSeamless::prepareSeamless()
{
    /* Set seamless feature flag to the guest: */
    session().GetConsole().GetDisplay().SetSeamlessMode(true);
    /* Create sync-blocker: */
    m_pSyncBlocker = new UIMachineViewBlocker;
}

void UIMachineViewSeamless::cleanupSeamless()
{
    /* If machine still running: */
    if (uisession()->isRunning())
    {
        /* Reset seamless feature flag of the guest: */
        session().GetConsole().GetDisplay().SetSeamlessMode(false);

        /* Rollback seamless frame-buffer size to normal: */
        machineWindowWrapper()->machineWindow()->hide();
        sltPerformGuestResize(guestSizeHint());
        m_pSyncBlocker->exec();

        /* Delete sync-blocker: */
        m_pSyncBlocker->deleteLater();
    }
}

QRect UIMachineViewSeamless::workingArea()
{
    /* Get corresponding screen: */
    int iScreen = static_cast<UIMachineLogicSeamless*>(machineLogic())->hostScreenForGuestScreen(screenId());
    /* Return available geometry for that screen: */
    return vboxGlobal().availableGeometry(iScreen);
}

void UIMachineViewSeamless::calculateDesktopGeometry()
{
    /* This method should not get called until we have initially set up the desktop geometry type: */
    Assert((desktopGeometryType() != DesktopGeo_Invalid));
    /* If we are not doing automatic geometry calculation then there is nothing to do: */
    if (desktopGeometryType() == DesktopGeo_Automatic)
    {
        m_desktopGeometry = QSize(workingArea().width(), workingArea().height());
    }
}

