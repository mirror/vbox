/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineViewFullscreen class implementation.
 */

/*
 * Copyright (C) 2010-2012 Oracle Corporation
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
#include "UIActionPoolRuntime.h"
#include "UIMachineLogicFullscreen.h"
#include "UIMachineWindow.h"
#include "UIMachineViewFullscreen.h"
#include "UIFrameBuffer.h"

UIMachineViewFullscreen::UIMachineViewFullscreen(  UIMachineWindow *pMachineWindow
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
    , m_bIsGuestAutoresizeEnabled(actionPool()->action(UIActionIndexRT_M_View_T_GuestAutoresize)->isChecked())
{
}

UIMachineViewFullscreen::~UIMachineViewFullscreen()
{
    /* Cleanup frame buffer: */
    cleanupFrameBuffer();
}

void UIMachineViewFullscreen::sltAdditionsStateChanged()
{
    adjustGuestScreenSize();
}

bool UIMachineViewFullscreen::eventFilter(QObject *pWatched, QEvent *pEvent)
{
    if (pWatched != 0 && pWatched == machineWindow())
    {
        switch (pEvent->type())
        {
            case QEvent::Resize:
            {
                /* Send guest-resize hint only if top window resizing to required dimension: */
                QResizeEvent *pResizeEvent = static_cast<QResizeEvent*>(pEvent);
                if (pResizeEvent->size() != workingArea().size())
                    break;

                /* Recalculate max guest size: */
                setMaxGuestSize();
                /* And resize guest to that size: */
                if (m_bIsGuestAutoresizeEnabled && uisession()->isGuestSupportsGraphics())
                    QTimer::singleShot(0, this, SLOT(sltPerformGuestResize()));
                break;
            }
            default:
                break;
        }
    }

    return UIMachineView::eventFilter(pWatched, pEvent);
}

void UIMachineViewFullscreen::prepareCommon()
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

void UIMachineViewFullscreen::prepareFilters()
{
    /* Base class filters: */
    UIMachineView::prepareFilters();
}

void UIMachineViewFullscreen::prepareConsoleConnections()
{
    /* Base class connections: */
    UIMachineView::prepareConsoleConnections();

    /* Guest additions state-change updater: */
    connect(uisession(), SIGNAL(sigAdditionsStateChange()), this, SLOT(sltAdditionsStateChanged()));
}

void UIMachineViewFullscreen::setGuestAutoresizeEnabled(bool fEnabled)
{
    if (m_bIsGuestAutoresizeEnabled != fEnabled)
    {
        m_bIsGuestAutoresizeEnabled = fEnabled;

        if (uisession()->isGuestSupportsGraphics())
            sltPerformGuestResize();
    }
}

void UIMachineViewFullscreen::adjustGuestScreenSize()
{
    /* Check if we should adjust guest-screen to new size: */
    if (frameBuffer()->isAutoEnabled() ||
        (int)frameBuffer()->width() != workingArea().size().width() ||
        (int)frameBuffer()->height() != workingArea().size().height())
        if (m_bIsGuestAutoresizeEnabled &&
            uisession()->isGuestSupportsGraphics() &&
            uisession()->isScreenVisible(screenId()))
        {
            frameBuffer()->setAutoEnabled(false);
            sltPerformGuestResize(workingArea().size());
        }
}

QRect UIMachineViewFullscreen::workingArea() const
{
    /* Get corresponding screen: */
    int iScreen = static_cast<UIMachineLogicFullscreen*>(machineLogic())->hostScreenForGuestScreen(screenId());
    /* Return available geometry for that screen: */
    return QApplication::desktop()->screenGeometry(iScreen);
}

QSize UIMachineViewFullscreen::calculateMaxGuestSize() const
{
    return workingArea().size();
}

