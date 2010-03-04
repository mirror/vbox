/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineViewFullscreen class implementation
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
#include <QTimer>
#ifdef Q_WS_MAC
#include <QMenuBar>
#endif
#ifdef Q_WS_X11
#include <limits.h>
#endif

/* Local includes */
#include "UISession.h"
#include "UIActionsPool.h"
#include "UIMachineLogic.h"
#include "UIMachineWindow.h"
#include "UIFrameBuffer.h"
#include "UIMachineViewFullscreen.h"
#include "QIMainDialog.h"

UIMachineViewFullscreen::UIMachineViewFullscreen(  UIMachineWindow *pMachineWindow
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
    , m_fIsInitialResizeEventProcessed(false)
    , m_bIsGuestAutoresizeEnabled(pMachineWindow->machineLogic()->actionsPool()->action(UIActionIndex_Toggle_GuestAutoresize)->isChecked())
    , m_fShouldWeDoResize(false)
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

    /* Initialization: */
    sltMachineStateChanged();
    sltAdditionsStateChanged();
    sltMousePointerShapeChanged();
    sltMouseCapabilityChanged();
}

UIMachineViewFullscreen::~UIMachineViewFullscreen()
{
    /* Cleanup common things: */
    cleanupCommon();

    /* Cleanup frame buffer: */
    cleanupFrameBuffer();
}

void UIMachineViewFullscreen::sltPerformGuestResize(const QSize &toSize)
{
    if (m_bIsGuestAutoresizeEnabled && uisession()->isGuestSupportsGraphics())
    {
        /* Taking Main Dialog: */
        QIMainDialog *pMainDialog = machineWindowWrapper() && machineWindowWrapper()->machineWindow() ?
                                    qobject_cast<QIMainDialog*>(machineWindowWrapper()->machineWindow()) : 0;

        /* If this slot is invoked directly then use the passed size
         * otherwise get the available size for the guest display.
         * We assume here that the centralWidget() contains this view only
         * and gives it all available space. */
        QSize newSize(toSize.isValid() ? toSize : pMainDialog ? pMainDialog->centralWidget()->size() : QSize());
        AssertMsg(newSize.isValid(), ("This size should be valid!\n"));

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

void UIMachineViewFullscreen::sltAdditionsStateChanged()
{
    /* Check if we should restrict minimum size: */
    maybeRestrictMinimumSize();
}

void UIMachineViewFullscreen::sltDesktopResized()
{
    /* If the desktop geometry is set automatically, this will update it: */
    calculateDesktopGeometry();
}

bool UIMachineViewFullscreen::event(QEvent *pEvent)
{
    switch (pEvent->type())
    {
        case VBoxDefs::ResizeEventType:
        {
            /* Some situations require framebuffer resize events to be ignored,
             * leaving machine window & machine view & framebuffer sizes preserved: */
            if (uisession()->isGuestResizeIgnored())
                return true;

            /* We are starting to perform machine view resize: */
            bool oldIgnoreMainwndResize = isMachineWindowResizeIgnored();
            setMachineWindowResizeIgnored(true);

            /* Get guest resize-event: */
            UIResizeEvent *pResizeEvent = static_cast<UIResizeEvent*>(pEvent);

            /* Perform framebuffer resize: */
            frameBuffer()->resizeEvent(pResizeEvent);

            /* Reapply maximum size restriction for machine view: */
            setMaximumSize(sizeHint());

            /* Store the new size to prevent unwanted resize hints being sent back: */
            storeConsoleSize(pResizeEvent->width(), pResizeEvent->height());

            /* Perform machine view resize: */
            resize(pResizeEvent->width(), pResizeEvent->height());

            /* Let our toplevel widget calculate its sizeHint properly. */
#ifdef Q_WS_X11
            /* We use processEvents rather than sendPostedEvents & set the time out value to max cause on X11 otherwise
             * the layout isn't calculated correctly. Dosn't find the bug in Qt, but this could be triggered through
             * the async nature of the X11 window event system. */
            QCoreApplication::processEvents(QEventLoop::AllEvents, INT_MAX);
#else /* Q_WS_X11 */
            QCoreApplication::sendPostedEvents(0, QEvent::LayoutRequest);
#endif /* Q_WS_X11 */

#ifdef Q_WS_MAC
            // TODO_NEW_CORE
//            mDockIconPreview->setOriginalSize(pResizeEvent->width(), pResizeEvent->height());
#endif /* Q_WS_MAC */

            /* Unfortunately restoreOverrideCursor() is broken in Qt 4.4.0 if WA_PaintOnScreen widgets are present.
             * This is the case on linux with SDL. As workaround we save/restore the arrow cursor manually.
             * See http://trolltech.com/developer/task-tracker/index_html?id=206165&method=entry for details.
             * Moreover the current cursor, which could be set by the guest, should be restored after resize: */
            if (uisession()->isValidPointerShapePresent())
                viewport()->setCursor(uisession()->cursor());
            else if (uisession()->isHidingHostPointer())
                viewport()->setCursor(Qt::BlankCursor);
            /* This event appears in case of guest video was changed for somehow even without video resolution change.
             * In this last case the host VM window will not be resized according this event and the host mouse cursor
             * which was unset to default here will not be hidden in capture state. So it is necessary to perform
             * updateMouseClipping() for the guest resize event if the mouse cursor was captured: */
            if (uisession()->isMouseCaptured())
                updateMouseClipping();

            /* May be we have to restrict minimum size? */
            maybeRestrictMinimumSize();

            /* Report to the VM thread that we finished resizing */
            session().GetConsole().GetDisplay().ResizeCompleted(screenId());

            /* We are finishing to perform machine view resize: */
            setMachineWindowResizeIgnored(oldIgnoreMainwndResize);

            /* Make sure that all posted signals are processed: */
            qApp->processEvents();

            /* We also recalculate the desktop geometry if this is determined automatically.
             * In fact, we only need this on the first resize,
             * but it is done every time to keep the code simpler. */
            calculateDesktopGeometry();

            /* Emit a signal about guest was resized: */
            emit resizeHintDone();

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
#if 0 // TODO: Activate Main Menu!
                    // should we create it first?
#endif
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

bool UIMachineViewFullscreen::eventFilter(QObject *pWatched, QEvent *pEvent)
{
    QIMainDialog *pMainDialog = machineWindowWrapper() && machineWindowWrapper()->machineWindow() ?
        qobject_cast<QIMainDialog*>(machineWindowWrapper()->machineWindow()) : 0;
    if (pWatched != 0 && pWatched == pMainDialog)
    {
        switch (pEvent->type())
        {
            case QEvent::Resize:
            {
                /* Ignore initial resize event: */
                if (!m_fIsInitialResizeEventProcessed)
                {
                    m_fIsInitialResizeEventProcessed = true;
                    break;
                }

                /* Set the "guest needs to resize" hint.
                 * This hint is acted upon when (and only when) the autoresize property is "true": */
                m_fShouldWeDoResize = uisession()->isGuestSupportsGraphics();
                if (!isMachineWindowResizeIgnored() && m_bIsGuestAutoresizeEnabled && uisession()->isGuestSupportsGraphics())
                    QTimer::singleShot(0, this, SLOT(sltPerformGuestResize()));
                break;
            }
            default:
                break;
        }
    }
#ifdef Q_WS_MAC // TODO: Is it really needed?
    QMenuBar *pMenuBar = machineWindowWrapper() && machineWindowWrapper()->machineWindow() ?
                         qobject_cast<QIMainDialog*>(machineWindowWrapper()->machineWindow())->menuBar() : 0;
    if (pWatched != 0 && pWatched == pMenuBar)
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
#endif /* Q_WS_MAC */
    return UIMachineView::eventFilter(pWatched, pEvent);
}

void UIMachineViewFullscreen::prepareCommon()
{
    /* Base class common settings: */
    UIMachineView::prepareCommon();

    /* Store old machine view size before bramebuffer resized: */
    m_normalSize = QSize(frameBuffer()->width(), frameBuffer()->height());

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

#ifdef Q_WS_MAC // TODO: Is it really needed? See UIMachineViewSeamless::eventFilter(...);
    /* Menu bar filter: */
    qobject_cast<QIMainDialog*>(machineWindowWrapper()->machineWindow())->menuBar()->installEventFilter(this);
#endif
}

void UIMachineViewFullscreen::prepareConnections()
{
    connect(QApplication::desktop(), SIGNAL(resized(int)), this, SLOT(sltDesktopResized()));
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

        maybeRestrictMinimumSize();

        sltPerformGuestResize();
    }
}

QRect UIMachineViewFullscreen::availableGeometry()
{
    return QApplication::desktop()->screenGeometry(this);
}

void UIMachineViewFullscreen::maybeRestrictMinimumSize()
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

