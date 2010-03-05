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
    /* Cleanup common things: */
    cleanupCommon();

    /* Cleanup frame buffer: */
    cleanupFrameBuffer();
}

void UIMachineViewNormal::sltAdditionsStateChanged()
{
    /* Check if we should restrict minimum size: */
    maybeRestrictMinimumSize();
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

/* If the desktop geometry is set automatically, this will update it: */
void UIMachineViewNormal::sltDesktopResized()
{
    calculateDesktopGeometry();
}

bool UIMachineViewNormal::event(QEvent *pEvent)
{
    switch (pEvent->type())
    {
        case VBoxDefs::ResizeEventType:
        {
            /* Some situations require initial VGA Resize Request
             * to be ignored at all, leaving previous framebuffer,
             * machine view and machine window sizes preserved: */
            if (uisession()->isGuestResizeIgnored())
                return true;

            bool oldIgnoreMainwndResize = isMachineWindowResizeIgnored();
            setMachineWindowResizeIgnored(true);

            UIResizeEvent *pResizeEvent = static_cast<UIResizeEvent*>(pEvent);

            /* Store the new size to prevent unwanted resize hints being sent back. */
            storeConsoleSize(pResizeEvent->width(), pResizeEvent->height());

            /* Unfortunately restoreOverrideCursor() is broken in Qt 4.4.0 if WA_PaintOnScreen widgets are present.
             * This is the case on linux with SDL. As workaround we save/restore the arrow cursor manually.
             * See http://trolltech.com/developer/task-tracker/index_html?id=206165&method=entry for details.
             * Moreover the current cursor, which could be set by the guest, should be restored after resize: */
            QCursor cursor;
            if (uisession()->isHidingHostPointer())
                cursor = QCursor(Qt::BlankCursor);
            else
                cursor = viewport()->cursor();
            frameBuffer()->resizeEvent(pResizeEvent);
            viewport()->setCursor(cursor);

#ifdef Q_WS_MAC
            // TODO_NEW_CORE
//            mDockIconPreview->setOriginalSize(pResizeEvent->width(), pResizeEvent->height());
#endif /* Q_WS_MAC */

            /* This event appears in case of guest video was changed for somehow even without video resolution change.
             * In this last case the host VM window will not be resized according this event and the host mouse cursor
             * which was unset to default here will not be hidden in capture state. So it is necessary to perform
             * updateMouseClipping() for the guest resize event if the mouse cursor was captured: */
            if (uisession()->isMouseCaptured())
                updateMouseClipping();

            /* Apply maximum size restriction: */
            setMaximumSize(sizeHint());

            /* May be we have to restrict minimum size? */
            maybeRestrictMinimumSize();

            /* Resize the guest canvas: */
            if (!isFrameBufferResizeIgnored())
                resize(pResizeEvent->width(), pResizeEvent->height());
            updateSliders();

            /* Let our toplevel widget calculate its sizeHint properly. */
#ifdef Q_WS_X11
            /* We use processEvents rather than sendPostedEvents & set the time out value to max cause on X11 otherwise
             * the layout isn't calculated correctly. Dosn't find the bug in Qt, but this could be triggered through
             * the async nature of the X11 window event system. */
            QCoreApplication::processEvents(QEventLoop::AllEvents, INT_MAX);
#else /* Q_WS_X11 */
            QCoreApplication::sendPostedEvents(0, QEvent::LayoutRequest);
#endif /* Q_WS_X11 */

            if (!isFrameBufferResizeIgnored())
                normalizeGeometry(true /* adjustPosition */);

            /* Report to the VM thread that we finished resizing */
            session().GetConsole().GetDisplay().ResizeCompleted(screenId());

            setMachineWindowResizeIgnored(oldIgnoreMainwndResize);

            /* Make sure that all posted signals are processed: */
            qApp->processEvents();

            /* Emit a signal about guest was resized: */
            emit resizeHintDone();

            /* We also recalculate the desktop geometry if this is determined
             * automatically.  In fact, we only need this on the first resize,
             * but it is done every time to keep the code simpler. */
            calculateDesktopGeometry();

            /* Enable frame-buffer resize watching: */
            if (isFrameBufferResizeIgnored())
                setFrameBufferResizeIgnored(false);

            return true;
        }

        case QEvent::KeyPress:
        case QEvent::KeyRelease:
        {
            QKeyEvent *pKeyEvent = static_cast<QKeyEvent*>(pEvent);

            if (isHostKeyPressed() && pEvent->type() == QEvent::KeyPress)
            {
                if (pKeyEvent->key() == Qt::Key_Home)
                {
                    /* In Qt4 it is not enough to just set the focus to menu-bar.
                     * So to get the menu-bar we have to send Qt::Key_Alt press/release events directly: */
                    QKeyEvent e1(QEvent::KeyPress, Qt::Key_Alt, Qt::NoModifier);
                    QKeyEvent e2(QEvent::KeyRelease, Qt::Key_Alt, Qt::NoModifier);
                    QMenuBar *pMenuBar = machineWindowWrapper() && machineWindowWrapper()->machineWindow() ?
                                         qobject_cast<QIMainDialog*>(machineWindowWrapper()->machineWindow())->menuBar() : 0;
                    QApplication::sendEvent(pMenuBar, &e1);
                    QApplication::sendEvent(pMenuBar, &e2);
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
    /* Who are we watchin? */
    QIMainDialog *pMainDialog = machineWindowWrapper() && machineWindowWrapper()->machineWindow() ?
        qobject_cast<QIMainDialog*>(machineWindowWrapper()->machineWindow()) : 0;
    QMenuBar *pMenuBar = pMainDialog ? qobject_cast<QIMainDialog*>(pMainDialog)->menuBar() : 0;

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
# endif
#endif /* defined (Q_WS_WIN32) */
            default:
                break;
        }
    }
    else if (pWatched != 0 && pWatched == pMenuBar)
    {
        /* Sometimes when we press ESC in the menu it brings the focus away (Qt bug?)
         * causing no widget to have a focus, or holds the focus itself, instead of
         * returning the focus to the console window. Here we fix this: */
        switch (pEvent->type())
        {
            case QEvent::FocusOut:
            {
                if (qApp->focusWidget() == 0)
                    setFocus();
                break;
            }
            case QEvent::KeyPress:
            {
                QKeyEvent *pKeyEvent = static_cast<QKeyEvent*>(pEvent);
                if (pKeyEvent->key() == Qt::Key_Escape && (pKeyEvent->modifiers() == Qt::NoModifier))
                    if (pMenuBar->hasFocus())
                        setFocus();
                break;
            }
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

