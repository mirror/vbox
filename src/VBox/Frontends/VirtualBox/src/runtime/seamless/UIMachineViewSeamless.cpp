/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineViewSeamless class implementation
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
#include "UIMachineLogicSeamless.h"
#include "UIMachineWindow.h"
#include "UIMachineViewSeamless.h"
#include "UIFrameBuffer.h"

UIMachineViewSeamless::UIMachineViewSeamless(  UIMachineWindow *pMachineWindow
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

    /* Prepare seamless view: */
    prepareSeamless();

    /* Initialization: */
    sltMachineStateChanged();
    sltAdditionsStateChanged();
}

UIMachineViewSeamless::~UIMachineViewSeamless()
{
    /* Cleanup seamless mode: */
    cleanupSeamless();

    /* Cleanup frame buffer: */
    cleanupFrameBuffer();
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

            /* Perform machine-view resize: */
            resize(pResizeEvent->width(), pResizeEvent->height());

            /* Let our toplevel widget calculate its sizeHint properly: */
            QCoreApplication::sendPostedEvents(0, QEvent::LayoutRequest);

#ifdef Q_WS_MAC
            machineLogic()->updateDockIconSize(screenId(), pResizeEvent->width(), pResizeEvent->height());
#endif /* Q_WS_MAC */

            /* Update machine-view sliders: */
            updateSliders();

            /* Report to the VM thread that we finished resizing: */
            session().GetConsole().GetDisplay().ResizeCompleted(screenId());

            /* We also recalculate the desktop geometry if this is determined
             * automatically.  In fact, we only need this on the first resize,
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
                /* Store the new size to prevent unwanted resize hints being sent back: */
                storeConsoleSize(pResizeEvent->size().width(), pResizeEvent->size().height());

                if (uisession()->isGuestSupportsGraphics())
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

    /* Setup size-policy: */
    setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum));
    /* Maximum size to sizehint: */
    setMaximumSize(sizeHint());
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
}

void UIMachineViewSeamless::cleanupSeamless()
{
    /* If machine still running: */
    if (uisession()->isRunning())
        /* Reset seamless feature flag of the guest: */
        session().GetConsole().GetDisplay().SetSeamlessMode(false);
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
        m_desktopGeometry = workingArea().size();
}

