/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineViewSeamless class implementation.
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

#ifdef VBOX_WITH_PRECOMPILED_HEADERS
# include <precomp.h>
#else  /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* Qt includes: */
# include <QApplication>
# include <QDesktopWidget>
# include <QMainWindow>
# include <QTimer>
# ifdef Q_WS_MAC
#  include <QMenuBar>
# endif /* Q_WS_MAC */

/* GUI includes: */
# include "VBoxGlobal.h"
# include "UISession.h"
# include "UIMachineLogicSeamless.h"
# include "UIMachineWindow.h"
# include "UIMachineViewSeamless.h"
# include "UIFrameBuffer.h"

/* COM includes: */
# include "CConsole.h"
# include "CDisplay.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */

/* External includes: */
#ifdef Q_WS_X11
# include <limits.h>
#endif /* Q_WS_X11 */



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
    /* Prepare seamless view: */
    prepareSeamless();
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
    adjustGuestScreenSize();
}

void UIMachineViewSeamless::sltHandleSetVisibleRegion(QRegion region)
{
    /* Apply new seamless-region: */
    m_pFrameBuffer->applyVisibleRegion(region);
}

bool UIMachineViewSeamless::eventFilter(QObject *pWatched, QEvent *pEvent)
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

void UIMachineViewSeamless::adjustGuestScreenSize()
{
    /* Check if we should adjust guest-screen to new size: */
    if (frameBuffer()->isAutoEnabled() ||
        (int)frameBuffer()->width() != workingArea().size().width() ||
        (int)frameBuffer()->height() != workingArea().size().height())
        if (uisession()->isGuestSupportsGraphics() &&
            uisession()->isScreenVisible(screenId()))
        {
            frameBuffer()->setAutoEnabled(false);
            sltPerformGuestResize(workingArea().size());
        }
}

QRect UIMachineViewSeamless::workingArea() const
{
    /* Get corresponding screen: */
    int iScreen = static_cast<UIMachineLogicSeamless*>(machineLogic())->hostScreenForGuestScreen(screenId());
    /* Return available geometry for that screen: */
    return QApplication::desktop()->availableGeometry(iScreen);
}

QSize UIMachineViewSeamless::calculateMaxGuestSize() const
{
    return workingArea().size();
}

