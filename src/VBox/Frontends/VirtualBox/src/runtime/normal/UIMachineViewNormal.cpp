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
    /* Connect view to handlers */
    connect(this, SIGNAL(additionsStateChanged(const QString&, bool, bool, bool)),
            this, SLOT(sltAdditionsStateChanged(const QString &, bool, bool, bool)));
}

void UIMachineViewNormal::normalizeGeometry(bool bAdjustPosition /* = false */)
{
    /* Make no normalizeGeometry in case we are in manual resize mode or main window is maximized */
    if (machineWindowWrapper()->machineWindow()->isMaximized())
        return;

    QWidget *pTopLevelWidget = window();

    /* calculate client window offsets */
    QRect fr = pTopLevelWidget->frameGeometry();
    QRect r = pTopLevelWidget->geometry();
    int dl = r.left() - fr.left();
    int dt = r.top() - fr.top();
    int dr = fr.right() - r.right();
    int db = fr.bottom() - r.bottom();

    /* get the best size w/o scroll bars */
    QSize s = pTopLevelWidget->sizeHint();

    /* resize the frame to fit the contents */
    s -= pTopLevelWidget->size();
    fr.setRight(fr.right() + s.width());
    fr.setBottom(fr.bottom() + s.height());

    if (bAdjustPosition)
    {
        QRegion ar;
        QDesktopWidget *dwt = QApplication::desktop();
        if (dwt->isVirtualDesktop())
            /* Compose complex available region */
            for (int i = 0; i < dwt->numScreens(); ++ i)
                ar += dwt->availableGeometry(i);
        else
            /* Get just a simple available rectangle */
            ar = dwt->availableGeometry(pTopLevelWidget->pos());

        fr = VBoxGlobal::normalizeGeometry(fr, ar, mode != VBoxDefs::SDLMode /* canResize */);
    }

#if 0
    /* Center the frame on the desktop: */
    fr.moveCenter(ar.center());
#endif

    /* Finally, set the frame geometry */
    pTopLevelWidget->setGeometry(fr.left() + dl, fr.top() + dt, fr.width() - dl - dr, fr.height() - dt - db);
}

void UIMachineViewNormal::maybeRestrictMinimumSize()
{
    /* Sets the minimum size restriction depending on the auto-resize feature state and the current rendering mode.
     * Currently, the restriction is set only in SDL mode and only when the auto-resize feature is inactive.
     * We need to do that because we cannot correctly draw in a scrolled window in SDL mode.
     * In all other modes, or when auto-resize is in force, this function does nothing. */
    if (mode == VBoxDefs::SDLMode)
    {
        if (!m_bIsGuestSupportsGraphics || !m_bIsGuestAutoresizeEnabled)
            setMinimumSize(sizeHint());
        else
            setMinimumSize(0, 0);
    }
}

void UIMachineViewNormal::doResizeHint(const QSize & /*toSize*/)
{
#if 0 // TODO: fix that logic!
    if (m_bIsGuestSupportsGraphics && m_bIsGuestAutoresizeEnabled)
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

/* If the desktop geometry is set automatically, this will update it. */
void UIMachineViewNormal::doResizeDesktop(int)
{
    calculateDesktopGeometry();
}

void UIMachineViewNormal::sltToggleGuestAutoresize(bool bOn)
{
    if (m_bIsGuestAutoresizeEnabled != bOn)
    {
        m_bIsGuestAutoresizeEnabled = bOn;

        maybeRestrictMinimumSize();

        if (m_bIsGuestSupportsGraphics && m_bIsGuestAutoresizeEnabled)
            doResizeHint();
    }
}

void UIMachineViewNormal::sltAdditionsStateChanged(const QString & /* strVersion */, bool /* bIsActive */,
                                                   bool /* bIsSeamlessSupported */, bool bIsGraphicsSupported)
{
    /* Enable/Disable guest auto-resizing depending on advanced graphics availablability: */
    sltToggleGuestAutoresize(bIsGraphicsSupported && m_bIsGuestAutoresizeEnabled);
}
