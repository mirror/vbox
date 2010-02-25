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
#include <QMainWindow>
#include <QMenuBar>

/* Local includes */
#include "VBoxGlobal.h"
#include "UIActionsPool.h"
#include "UIMachineLogic.h"
#include "UIMachineWindow.h"
#include "UIMachineViewNormal.h"

UIMachineViewNormal::UIMachineViewNormal(  UIMachineWindow *pMachineWindow
                                         , VBoxDefs::RenderMode renderMode
#ifdef VBOX_WITH_VIDEOHWACCEL
                                         , bool bAccelerate2DVideo
#endif
                                        )
    : UIMachineView(  pMachineWindow
                    , renderMode
#ifdef VBOX_WITH_VIDEOHWACCEL
                    , bAccelerate2DVideo
#endif
                   )
{
    /* Prepare frame buffer: */
    prepareFrameBuffer();

    /* Prepare common things: */
    prepareCommon();

    /* Prepare event-filters: */
    prepareFilters();

    /* Prepare console connections: */
    prepareConsoleConnections();

    /* Load machine view settings: */
    loadMachineViewSettings();
}

UIMachineViewNormal::~UIMachineViewNormal()
{
    /* Cleanup common things: */
    cleanupCommon();

    /* Cleanup frame buffer: */
    cleanupFrameBuffer();
}

void UIMachineViewNormal::sltPerformGuestResize(const QSize & /* toSize */)
{
#if 0 // TODO: fix that logic!
    if (isGuestSupportsGraphics() && m_bIsGuestAutoresizeEnabled)
    {
        /* If this slot is invoked directly then use the passed size
         * otherwise get the available size for the guest display.
         * We assume here that the centralWidget() contains this view only
         * and gives it all available space. */
        QSize sz(toSize.isValid() ? toSize : machineWindow()->centralWidget()->size());
        if (!toSize.isValid())
            sz -= QSize(frameWidth() * 2, frameWidth() * 2);
        /* Do not send out useless hints. */
        if ((sz.width() == mStoredConsoleSize.width()) && (sz.height() == mStoredConsoleSize.height()))
            return;
        /* We only actually send the hint if
         * 1) the autoresize property is set to true and
         * 2) either an explicit new size was given (e.g. if the request
         *    was triggered directly by a console resize event) or if no
         *    explicit size was specified but a resize is flagged as being
         *    needed (e.g. the autoresize was just enabled and the console
         *    was resized while it was disabled). */
        if (m_bIsGuestAutoresizeEnabled && (toSize.isValid() || mDoResize))
        {
            /* Remember the new size. */
            storeConsoleSize(sz.width(), sz.height());

            mConsole.GetDisplay().SetVideoModeHint(sz.width(), sz.height(), 0, 0);
        }
        /* We have resized now... */
        mDoResize = false;
    }
#endif
}

void UIMachineViewNormal::sltAdditionsStateChanged()
{
    /* Base-class additions state-change-handler: */
    UIMachineView::sltAdditionsStateChanged();

    /* Enable/Disable guest auto-resizing depending on advanced graphics availablability: */
    setGuestAutoresizeEnabled(isGuestSupportsGraphics() && m_bIsGuestAutoresizeEnabled);
}

void UIMachineViewNormal::prepareFilters()
{
    /* Prepare base-class filters: */
    UIMachineView::prepareFilters();

    /* Normal window filters: */
    qobject_cast<QMainWindow*>(machineWindowWrapper()->machineWindow())->menuBar()->installEventFilter(this);
}

void UIMachineViewNormal::setGuestAutoresizeEnabled(bool fEnabled)
{
    if (m_bIsGuestAutoresizeEnabled != fEnabled)
    {
        m_bIsGuestAutoresizeEnabled = fEnabled;

        maybeRestrictMinimumSize();

        if (isGuestSupportsGraphics() && m_bIsGuestAutoresizeEnabled)
            sltPerformGuestResize();
    }
}

void UIMachineViewNormal::normalizeGeometry(bool bAdjustPosition /* = false */)
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

        frameGeo = VBoxGlobal::normalizeGeometry(frameGeo, availableGeo, mode() != VBoxDefs::SDLMode /* canResize */);
    }

#if 0
    /* Center the frame on the desktop: */
    frameGeo.moveCenter(availableGeo.center());
#endif

    /* Finally, set the frame geometry */
    pTopLevelWidget->setGeometry(frameGeo.left() + dl, frameGeo.top() + dt, frameGeo.width() - dl - dr, frameGeo.height() - dt - db);
}

void UIMachineViewNormal::maybeRestrictMinimumSize()
{
    /* Sets the minimum size restriction depending on the auto-resize feature state and the current rendering mode.
     * Currently, the restriction is set only in SDL mode and only when the auto-resize feature is inactive.
     * We need to do that because we cannot correctly draw in a scrolled window in SDL mode.
     * In all other modes, or when auto-resize is in force, this function does nothing. */
    if (mode() == VBoxDefs::SDLMode)
    {
        if (!isGuestSupportsGraphics() || !m_bIsGuestAutoresizeEnabled)
            setMinimumSize(sizeHint());
        else
            setMinimumSize(0, 0);
    }
}

