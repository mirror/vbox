/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineWindowFullscreen class implementation.
 */

/*
 * Copyright (C) 2010-2015 Oracle Corporation
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
# include <QMenu>
# include <QTimer>

/* GUI includes: */
# include "VBoxGlobal.h"
# include "UIExtraDataManager.h"
# include "UISession.h"
# include "UIActionPoolRuntime.h"
# include "UIMachineLogicFullscreen.h"
# include "UIMachineWindowFullscreen.h"
# include "UIMachineView.h"
# if   defined(VBOX_WS_WIN) || defined(VBOX_WS_X11)
#  include "UIMachineDefs.h"
#  include "UIMiniToolBar.h"
# elif defined(VBOX_WS_MAC)
#  include "UIFrameBuffer.h"
#  include "VBoxUtils-darwin.h"
#  include "UICocoaApplication.h"
# endif /* VBOX_WS_MAC */

/* COM includes: */
# include "CSnapshot.h"

#endif /* !VBOX_WITH_PRECOMPILED_HEADERS */


UIMachineWindowFullscreen::UIMachineWindowFullscreen(UIMachineLogic *pMachineLogic, ulong uScreenId)
    : UIMachineWindow(pMachineLogic, uScreenId)
#if defined(VBOX_WS_WIN) || defined(VBOX_WS_X11)
    , m_pMiniToolBar(0)
#endif /* VBOX_WS_WIN || VBOX_WS_X11 */
#ifdef VBOX_WS_MAC
    , m_fIsInFullscreenTransition(false)
#endif /* VBOX_WS_MAC */
{
}

void UIMachineWindowFullscreen::changeEvent(QEvent *pChangeEvent)
{
    /* Overriding changeEvent for controlling mini-tool bar visibilty.
     * Hiding tool-bar when machine-window is minimized and show it in full-screen: */
#if defined(Q_WS_WIN) || defined(Q_WS_X11)
    if (pChangeEvent->type() == QEvent::WindowStateChange)
    {
        /* If machine-window is in minimized mode: */
        if (isMinimized())
        {
            /* If there is a mini-toolbar: */
            if (m_pMiniToolBar)
            {
                /* Hide mini-toolbar: */
                m_pMiniToolBar->hide();
            }
        }
        /* If machine-window is in full-screen mode: */
        else
        {
            /* If there is a mini-toolbar: */
            if (m_pMiniToolBar)
            {
                /* Show mini-toolbar in full-screen mode: */
                m_pMiniToolBar->showFullScreen();
            }
        }
    }
#endif /* Q_WS_WIN || Q_WS_X11 */

    /* Call to base-class: */
    QMainWindow::changeEvent(pChangeEvent);
}

#ifdef VBOX_WS_MAC
void UIMachineWindowFullscreen::handleNativeNotification(const QString &strNativeNotificationName)
{
    /* Make sure this method is only used for ML and next: */
    AssertReturnVoid(vboxGlobal().osRelease() > MacOSXRelease_Lion);

    /* Log all arrived notifications: */
    LogRel(("UIMachineWindowFullscreen::handleNativeNotification: Notification '%s' received.\n",
            strNativeNotificationName.toLatin1().constData()));

    /* Handle 'NSWindowWillEnterFullScreenNotification' notification: */
    if (strNativeNotificationName == "NSWindowWillEnterFullScreenNotification")
    {
        LogRel(("UIMachineWindowFullscreen::handleNativeNotification: "
                "Native fullscreen mode about to enter, notifying listener...\n"));
        emit sigNotifyAboutNativeFullscreenWillEnter();
    }
    /* Handle 'NSWindowDidEnterFullScreenNotification' notification: */
    else if (strNativeNotificationName == "NSWindowDidEnterFullScreenNotification")
    {
        /* Mark window transition complete: */
        m_fIsInFullscreenTransition = false;
        LogRel(("UIMachineWindowFullscreen::handleNativeNotification: "
                "Native fullscreen mode entered, notifying listener...\n"));
        /* Update console's display viewport and 3D overlay: */
        machineView()->updateViewport();
        emit sigNotifyAboutNativeFullscreenDidEnter();
    }
    /* Handle 'NSWindowWillExitFullScreenNotification' notification: */
    else if (strNativeNotificationName == "NSWindowWillExitFullScreenNotification")
    {
        LogRel(("UIMachineWindowFullscreen::handleNativeNotification: "
                "Native fullscreen mode about to exit, notifying listener...\n"));
        emit sigNotifyAboutNativeFullscreenWillExit();
    }
    /* Handle 'NSWindowDidExitFullScreenNotification' notification: */
    else if (strNativeNotificationName == "NSWindowDidExitFullScreenNotification")
    {
        /* Mark window transition complete: */
        m_fIsInFullscreenTransition = false;
        LogRel(("UIMachineWindowFullscreen::handleNativeNotification: "
                "Native fullscreen mode exited, notifying listener...\n"));
        /* Update console's display viewport and 3D overlay: */
        machineView()->updateViewport();
        emit sigNotifyAboutNativeFullscreenDidExit();
    }
    /* Handle 'NSWindowDidFailToEnterFullScreenNotification' notification: */
    else if (strNativeNotificationName == "NSWindowDidFailToEnterFullScreenNotification")
    {
        /* Mark window transition complete: */
        m_fIsInFullscreenTransition = false;
        LogRel(("UIMachineWindowFullscreen::handleNativeNotification: "
                "Native fullscreen mode fail to enter, notifying listener...\n"));
        emit sigNotifyAboutNativeFullscreenFailToEnter();
    }
}
#endif /* VBOX_WS_MAC */

#if defined(VBOX_WS_WIN) || defined(VBOX_WS_X11)
void UIMachineWindowFullscreen::sltMachineStateChanged()
{
    /* Call to base-class: */
    UIMachineWindow::sltMachineStateChanged();

    /* Update mini-toolbar: */
    updateAppearanceOf(UIVisualElement_MiniToolBar);
}

void UIMachineWindowFullscreen::sltRevokeWindowActivation()
{
    /* Make sure window is visible: */
    if (!isVisible() || isMinimized())
        return;

    /* Revoke stolen activation: */
#ifdef VBOX_WS_X11
    raise();
#endif /* VBOX_WS_X11 */
    activateWindow();
}
#endif /* VBOX_WS_WIN || VBOX_WS_X11 */

#ifdef VBOX_WS_MAC
void UIMachineWindowFullscreen::sltEnterNativeFullscreen(UIMachineWindow *pMachineWindow)
{
    /* Make sure this slot is called only under ML and next: */
    AssertReturnVoid(vboxGlobal().osRelease() > MacOSXRelease_Lion);

    /* Make sure it is NULL or 'this' window passed: */
    if (pMachineWindow && pMachineWindow != this)
        return;

    /* Make sure this window has fullscreen logic: */
    UIMachineLogicFullscreen *pFullscreenLogic = qobject_cast<UIMachineLogicFullscreen*>(machineLogic());
    AssertPtrReturnVoid(pFullscreenLogic);

    /* Make sure this window should be shown and mapped to host-screen: */
    if (!uisession()->isScreenVisible(m_uScreenId) ||
        !pFullscreenLogic->hasHostScreenForGuestScreen(m_uScreenId))
        return;

    /* Mark window 'transitioned to fullscreen': */
    m_fIsInFullscreenTransition = true;

    /* Enter native fullscreen mode if necessary: */
    if (   (pFullscreenLogic->screensHaveSeparateSpaces() || m_uScreenId == 0)
        && !darwinIsInFullscreenMode(this))
        darwinToggleFullscreenMode(this);
}

void UIMachineWindowFullscreen::sltExitNativeFullscreen(UIMachineWindow *pMachineWindow)
{
    /* Make sure this slot is called only under ML and next: */
    AssertReturnVoid(vboxGlobal().osRelease() > MacOSXRelease_Lion);

    /* Make sure it is NULL or 'this' window passed: */
    if (pMachineWindow && pMachineWindow != this)
        return;

    /* Make sure this window has fullscreen logic: */
    UIMachineLogicFullscreen *pFullscreenLogic = qobject_cast<UIMachineLogicFullscreen*>(machineLogic());
    AssertPtrReturnVoid(pFullscreenLogic);

    /* Mark window 'transitioned from fullscreen': */
    m_fIsInFullscreenTransition = true;

    /* Exit native fullscreen mode if necessary: */
    if (   (pFullscreenLogic->screensHaveSeparateSpaces() || m_uScreenId == 0)
        && darwinIsInFullscreenMode(this))
        darwinToggleFullscreenMode(this);
}
#endif /* VBOX_WS_MAC */

void UIMachineWindowFullscreen::prepareVisualState()
{
    /* Call to base-class: */
    UIMachineWindow::prepareVisualState();

    /* The background has to go black: */
    QPalette palette(centralWidget()->palette());
    palette.setColor(centralWidget()->backgroundRole(), Qt::black);
    centralWidget()->setPalette(palette);
    centralWidget()->setAutoFillBackground(true);
    setAutoFillBackground(true);

#if defined(VBOX_WS_WIN) || defined(VBOX_WS_X11)
    /* Prepare mini-toolbar: */
    prepareMiniToolbar();
#endif /* VBOX_WS_WIN || VBOX_WS_X11 */

#ifdef VBOX_WS_MAC
    /* Native fullscreen stuff on ML and next: */
    if (vboxGlobal().osRelease() > MacOSXRelease_Lion)
    {
        /* Make sure this window has fullscreen logic: */
        UIMachineLogicFullscreen *pFullscreenLogic = qobject_cast<UIMachineLogicFullscreen*>(machineLogic());
        AssertPtrReturnVoid(pFullscreenLogic);
        /* Enable fullscreen support for every screen which requires it: */
        if (pFullscreenLogic->screensHaveSeparateSpaces() || m_uScreenId == 0)
            darwinEnableFullscreenSupport(this);
        /* Enable transience support for other screens: */
        else
            darwinEnableTransienceSupport(this);
        /* Register to native fullscreen notifications: */
        UICocoaApplication::instance()->registerToNotificationOfWindow("NSWindowWillEnterFullScreenNotification", this,
                                                                       UIMachineWindow::handleNativeNotification);
        UICocoaApplication::instance()->registerToNotificationOfWindow("NSWindowDidEnterFullScreenNotification", this,
                                                                       UIMachineWindow::handleNativeNotification);
        UICocoaApplication::instance()->registerToNotificationOfWindow("NSWindowWillExitFullScreenNotification", this,
                                                                       UIMachineWindow::handleNativeNotification);
        UICocoaApplication::instance()->registerToNotificationOfWindow("NSWindowDidExitFullScreenNotification", this,
                                                                       UIMachineWindow::handleNativeNotification);
        UICocoaApplication::instance()->registerToNotificationOfWindow("NSWindowDidFailToEnterFullScreenNotification", this,
                                                                       UIMachineWindow::handleNativeNotification);
}
#endif /* VBOX_WS_MAC */
}

#if defined(VBOX_WS_WIN) || defined(VBOX_WS_X11)
void UIMachineWindowFullscreen::prepareMiniToolbar()
{
    /* Make sure mini-toolbar is not restricted: */
    if (!gEDataManager->miniToolbarEnabled(vboxGlobal().managedVMUuid()))
        return;

    /* Create mini-toolbar: */
    m_pMiniToolBar = new UIMiniToolBar(this,
                                       GeometryType_Full,
                                       gEDataManager->miniToolbarAlignment(vboxGlobal().managedVMUuid()),
                                       gEDataManager->autoHideMiniToolbar(vboxGlobal().managedVMUuid()));
    AssertPtrReturnVoid(m_pMiniToolBar);
    {
        /* Configure mini-toolbar: */
        m_pMiniToolBar->addMenus(actionPool()->menus());
        connect(m_pMiniToolBar, SIGNAL(sigMinimizeAction()),
                this, SLOT(showMinimized()), Qt::QueuedConnection);
        connect(m_pMiniToolBar, SIGNAL(sigExitAction()),
                actionPool()->action(UIActionIndexRT_M_View_T_Fullscreen), SLOT(trigger()));
        connect(m_pMiniToolBar, SIGNAL(sigCloseAction()),
                actionPool()->action(UIActionIndex_M_Application_S_Close), SLOT(trigger()));
        connect(m_pMiniToolBar, SIGNAL(sigNotifyAboutWindowActivationStolen()),
                this, SLOT(sltRevokeWindowActivation()), Qt::QueuedConnection);
# ifdef VBOX_WS_X11
        // WORKAROUND:
        // Due to Unity bug we want native full-screen flag to be set
        // for mini-toolbar _before_ trying to show it in full-screen mode.
        // That significantly improves of chances to have required geometry.
        if (vboxGlobal().typeOfWindowManager() == X11WMType_Compiz)
            vboxGlobal().setFullScreenFlag(m_pMiniToolBar);
# endif /* VBOX_WS_X11 */
    }
}
#endif /* VBOX_WS_WIN || VBOX_WS_X11 */

#if defined(VBOX_WS_WIN) || defined(VBOX_WS_X11)
void UIMachineWindowFullscreen::cleanupMiniToolbar()
{
    /* Make sure mini-toolbar was created: */
    if (!m_pMiniToolBar)
        return;

    /* Save mini-toolbar settings: */
    gEDataManager->setAutoHideMiniToolbar(m_pMiniToolBar->autoHide(), vboxGlobal().managedVMUuid());
    /* Delete mini-toolbar: */
    delete m_pMiniToolBar;
    m_pMiniToolBar = 0;
}
#endif /* VBOX_WS_WIN || VBOX_WS_X11 */

void UIMachineWindowFullscreen::cleanupVisualState()
{
#ifdef VBOX_WS_MAC
    /* Native fullscreen stuff on ML and next: */
    if (vboxGlobal().osRelease() > MacOSXRelease_Lion)
    {
        /* Unregister from native fullscreen notifications: */
        UICocoaApplication::instance()->unregisterFromNotificationOfWindow("NSWindowWillEnterFullScreenNotification", this);
        UICocoaApplication::instance()->unregisterFromNotificationOfWindow("NSWindowDidEnterFullScreenNotification", this);
        UICocoaApplication::instance()->unregisterFromNotificationOfWindow("NSWindowWillExitFullScreenNotification", this);
        UICocoaApplication::instance()->unregisterFromNotificationOfWindow("NSWindowDidExitFullScreenNotification", this);
        UICocoaApplication::instance()->unregisterFromNotificationOfWindow("NSWindowDidFailToEnterFullScreenNotification", this);
    }
#endif /* VBOX_WS_MAC */

#if defined(VBOX_WS_WIN) || defined(VBOX_WS_X11)
    /* Cleanup mini-toolbar: */
    cleanupMiniToolbar();
#endif /* VBOX_WS_WIN || VBOX_WS_X11 */

    /* Call to base-class: */
    UIMachineWindow::cleanupVisualState();
}

void UIMachineWindowFullscreen::placeOnScreen()
{
    /* Get corresponding host-screen: */
    const int iHostScreen = qobject_cast<UIMachineLogicFullscreen*>(machineLogic())->hostScreenForGuestScreen(m_uScreenId);
    /* And corresponding working area: */
    const QRect workingArea = vboxGlobal().screenGeometry(iHostScreen);

#if   defined(VBOX_WS_WIN) || defined(VBOX_WS_X11)
# if defined(VBOX_WS_WIN) && QT_VERSION >= 0x050000
    /* On Windows you can't just resize/move Qt5 QWidget to required host-screen
     * because for simple normal QWidget maximum possible window size is limited
     * by the available geometry and Qt5 bumps it out to the main host-screen in
     * such case, need to tell window it will be full-screen without showing it. */
    setWindowState(Qt::WindowFullScreen);
    /* If there is a mini-toolbar: */
    if (m_pMiniToolBar)
        m_pMiniToolBar->setWindowState(Qt::WindowFullScreen);
# endif /* VBOX_WS_WIN && QT_VERSION >= 0x050000 */

    /* Set appropriate geometry for window: */
    resize(workingArea.size());
    move(workingArea.topLeft());

    /* If there is a mini-toolbar: */
    if (m_pMiniToolBar)
    {
        /* Set appropriate geometry for mini-toolbar: */
        m_pMiniToolBar->resize(workingArea.size());
        m_pMiniToolBar->move(workingArea.topLeft());
    }
#elif defined(VBOX_WS_MAC)
    /* Make sure this window has fullscreen logic: */
    UIMachineLogicFullscreen *pFullscreenLogic = qobject_cast<UIMachineLogicFullscreen*>(machineLogic());
    AssertPtrReturnVoid(pFullscreenLogic);

    /* Move window to the appropriate position: */
    move(workingArea.topLeft());

    /* Resize window to the appropriate size on Lion and previous: */
    if (vboxGlobal().osRelease() <= MacOSXRelease_Lion)
        resize(workingArea.size());
    /* Resize window to the appropriate size on ML and next
     * only if that screen has no own user-space: */
    else if (!pFullscreenLogic->screensHaveSeparateSpaces() && m_uScreenId != 0)
        resize(workingArea.size());
    else
    {
        /* Load normal geometry first of all: */
        QRect geo = gEDataManager->machineWindowGeometry(UIVisualStateType_Normal, m_uScreenId, vboxGlobal().managedVMUuid());
        /* If normal geometry is null => use frame-buffer size: */
        if (geo.isNull())
        {
            const UIFrameBuffer *pFrameBuffer = uisession()->frameBuffer(m_uScreenId);
            geo = QRect(QPoint(0, 0), QSize(pFrameBuffer->width(), pFrameBuffer->height()).boundedTo(workingArea.size()));
        }
        /* If frame-buffer size is null => use default size: */
        if (geo.isNull())
            geo = QRect(QPoint(0, 0), QSize(800, 600).boundedTo(workingArea.size()));
        /* Move window to the center of working-area: */
        geo.moveCenter(workingArea.center());
        setGeometry(geo);
    }
#endif /* VBOX_WS_MAC */
}

void UIMachineWindowFullscreen::showInNecessaryMode()
{
    /* Make sure window has fullscreen logic: */
    UIMachineLogicFullscreen *pFullscreenLogic = qobject_cast<UIMachineLogicFullscreen*>(machineLogic());
    AssertPtrReturnVoid(pFullscreenLogic);

    /* Make sure window should be shown and mapped to some host-screen: */
    if (!uisession()->isScreenVisible(m_uScreenId) ||
        !pFullscreenLogic->hasHostScreenForGuestScreen(m_uScreenId))
    {
        /* Hide window: */
        hide();
    }
    else
    {
        /* Ignore if window is minimized and visible: */
        if (isMinimized() && isVisible())
            return;

#ifdef VBOX_WS_X11
        /* If WM doesn't support native stuff, we need to call for placeOnScreen(): */
        const bool fSupportsNativeFullScreen = VBoxGlobal::supportsFullScreenMonitorsProtocolX11() &&
                                               !gEDataManager->legacyFullscreenModeRequested();
        if (!fSupportsNativeFullScreen)
        {
            /* Make sure window have appropriate geometry: */
            placeOnScreen();
        }
#else /* !VBOX_WS_X11 */
        /* Make sure window have appropriate geometry: */
        placeOnScreen();
#endif /* !VBOX_WS_X11 */

#if defined(VBOX_WS_MAC)
        /* ML and next using native stuff, so we can call for simple show(),
         * Lion and previous using Qt stuff, so we should call for showFullScreen(): */
        const bool fSupportsNativeFullScreen = vboxGlobal().osRelease() > MacOSXRelease_Lion;
        if (fSupportsNativeFullScreen)
        {
            /* Show window in normal mode: */
            show();
        }
        else
        {
            /* Show window in fullscreen mode: */
            showFullScreen();
        }
#elif defined(VBOX_WS_WIN) || defined(VBOX_WS_X11)
        /* If machine-window is in minimized mode: */
        if (isMinimized())
        {
            /* Show window in minimized mode: */
            showMinimized();
        }
        else
        {
            /* Show window in fullscreen mode: */
            showFullScreen();
        }
#endif /* VBOX_WS_WIN || VBOX_WS_X11 */

#ifdef VBOX_WS_X11
        /* If WM supports native stuff, we need to map window to corresponding host-screen. */
        if (fSupportsNativeFullScreen)
        {
            /* Tell recent window managers which host-screen this window should be mapped to: */
            VBoxGlobal::setFullScreenMonitorX11(this, pFullscreenLogic->hostScreenForGuestScreen(m_uScreenId));

            /* If there is a mini-toolbar: */
            if (m_pMiniToolBar)
            {
                /* Tell recent window managers which host-screen this mini-toolbar should be mapped to: */
                VBoxGlobal::setFullScreenMonitorX11(m_pMiniToolBar, pFullscreenLogic->hostScreenForGuestScreen(m_uScreenId));
            }
        }
#endif /* VBOX_WS_X11 */

        /* Adjust machine-view size if necessary: */
        adjustMachineViewSize();

        /* Make sure machine-view have focus: */
        m_pMachineView->setFocus();
    }
}

#if defined(VBOX_WS_WIN) || defined(VBOX_WS_X11)
void UIMachineWindowFullscreen::updateAppearanceOf(int iElement)
{
    /* Call to base-class: */
    UIMachineWindow::updateAppearanceOf(iElement);

    /* Update mini-toolbar: */
    if (iElement & UIVisualElement_MiniToolBar)
    {
        /* If there is a mini-toolbar: */
        if (m_pMiniToolBar)
        {
            /* Get snapshot(s): */
            QString strSnapshotName;
            if (machine().GetSnapshotCount() > 0)
            {
                CSnapshot snapshot = machine().GetCurrentSnapshot();
                strSnapshotName = " (" + snapshot.GetName() + ")";
            }
            /* Update mini-toolbar text: */
            m_pMiniToolBar->setText(machineName() + strSnapshotName);
        }
    }
}
#endif /* VBOX_WS_WIN || VBOX_WS_X11 */

#ifdef VBOX_WS_WIN
# if QT_VERSION >= 0x050000
void UIMachineWindowFullscreen::showEvent(QShowEvent *pEvent)
{
    /* Expose workaround again,
     * Qt devs will never fix that it seems.
     * This time they forget to set 'Mapped'
     * attribute for initially frame-less window. */
    setAttribute(Qt::WA_Mapped);

    /* Call to base-class: */
    UIMachineWindow::showEvent(pEvent);
}
# endif /* QT_VERSION >= 0x050000 */
#endif /* VBOX_WS_WIN */

