/* $Id$ */
/** @file
 *
 * VBox frontends: Qt GUI ("VirtualBox"):
 * UIMachineWindowFullscreen class implementation
 */

/*
 * Copyright (C) 2010-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* Qt includes: */
#include <QDesktopWidget>
#include <QMenu>
#include <QTimer>

/* GUI includes: */
#include "VBoxGlobal.h"
#include "UIExtraDataManager.h"
#include "UISession.h"
#include "UIActionPoolRuntime.h"
#include "UIMachineLogicFullscreen.h"
#include "UIMachineWindowFullscreen.h"
#include "UIMachineView.h"
#include "UIFrameBuffer.h"
#include "UIMachineDefs.h"
#include "UIMiniToolBar.h"
#ifdef Q_WS_MAC
# include "VBoxUtils-darwin.h"
# include "UICocoaApplication.h"
#endif /* Q_WS_MAC */

/* COM includes: */
#include "CSnapshot.h"

UIMachineWindowFullscreen::UIMachineWindowFullscreen(UIMachineLogic *pMachineLogic, ulong uScreenId)
    : UIMachineWindow(pMachineLogic, uScreenId)
    , m_pMiniToolBar(0)
#ifdef Q_WS_MAC
    , m_fIsInFullscreenTransition(false)
#endif /* Q_WS_MAC */
{
}

#ifdef Q_WS_MAC
void UIMachineWindowFullscreen::handleNativeNotification(const QString &strNativeNotificationName)
{
    /* Make sure this method is only used for ML and next: */
    AssertReturnVoid(vboxGlobal().osRelease() > MacOSXRelease_Lion);

    /* Log all arrived notifications: */
    LogRel(("UIMachineWindowFullscreen::handleNativeNotification: Notification '%s' received.\n",
            strNativeNotificationName.toAscii().constData()));

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
#endif /* Q_WS_MAC */

void UIMachineWindowFullscreen::sltMachineStateChanged()
{
    /* Call to base-class: */
    UIMachineWindow::sltMachineStateChanged();

    /* Update mini-toolbar: */
    updateAppearanceOf(UIVisualElement_MiniToolBar);
}

#ifdef Q_WS_MAC
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
#endif /* Q_WS_MAC */

void UIMachineWindowFullscreen::sltRevokeFocus()
{
    /* Make sure window is visible: */
    if (!isVisible())
        return;

    /* Revoke stolen focus: */
    m_pMachineView->setFocus();
}

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

    /* Prepare mini-toolbar: */
    prepareMiniToolbar();

#ifdef Q_WS_MAC
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
#endif /* Q_WS_MAC */
}

void UIMachineWindowFullscreen::prepareMiniToolbar()
{
    /* Make sure mini-toolbar is not restricted: */
    if (!gEDataManager->miniToolbarEnabled(vboxGlobal().managedVMUuid()))
        return;

    /* Create mini-toolbar: */
    m_pMiniToolBar = new UIRuntimeMiniToolBar(this,
                                              GeometryType_Full,
                                              gEDataManager->miniToolbarAlignment(vboxGlobal().managedVMUuid()),
                                              gEDataManager->autoHideMiniToolbar(vboxGlobal().managedVMUuid()));
    m_pMiniToolBar->addMenus(actionPool()->menus());
#ifndef RT_OS_DARWIN
    connect(m_pMiniToolBar, SIGNAL(sigMinimizeAction()), this, SLOT(showMinimized()));
#endif /* !RT_OS_DARWIN */
    connect(m_pMiniToolBar, SIGNAL(sigExitAction()),
            actionPool()->action(UIActionIndexRT_M_View_T_Fullscreen), SLOT(trigger()));
#ifdef RT_OS_DARWIN
    connect(m_pMiniToolBar, SIGNAL(sigCloseAction()),
            actionPool()->action(UIActionIndex_M_Application_S_Close), SLOT(trigger()));
#else /* !RT_OS_DARWIN */
    connect(m_pMiniToolBar, SIGNAL(sigCloseAction()),
            actionPool()->action(UIActionIndexRT_M_Machine_S_Close), SLOT(trigger()));
#endif /* !RT_OS_DARWIN */
    connect(m_pMiniToolBar, SIGNAL(sigNotifyAboutFocusStolen()), this, SLOT(sltRevokeFocus()));
}

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

void UIMachineWindowFullscreen::cleanupVisualState()
{
#ifdef Q_WS_MAC
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
#endif /* Q_WS_MAC */

    /* Cleanup mini-toolbar: */
    cleanupMiniToolbar();

    /* Call to base-class: */
    UIMachineWindow::cleanupVisualState();
}

void UIMachineWindowFullscreen::placeOnScreen()
{
    /* Get corresponding screen: */
    const int iScreen = qobject_cast<UIMachineLogicFullscreen*>(machineLogic())->hostScreenForGuestScreen(m_uScreenId);
    /* And corresponding working area: */
    const QRect workingArea = QApplication::desktop()->screenGeometry(iScreen);

    /* Move to the appropriate position: */
    move(workingArea.topLeft());

#ifdef Q_WS_MAC
    /* Make sure this window has fullscreen logic: */
    UIMachineLogicFullscreen *pFullscreenLogic = qobject_cast<UIMachineLogicFullscreen*>(machineLogic());
    AssertPtrReturnVoid(pFullscreenLogic);

    /* Resize to the appropriate size on Lion and previous: */
    if (vboxGlobal().osRelease() <= MacOSXRelease_Lion)
        resize(workingArea.size());
    /* Resize to the appropriate size on ML and next
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
#else /* !Q_WS_MAC */
    /* Resize to the appropriate size: */
    resize(workingArea.size());
#endif /* !Q_WS_MAC */
}

void UIMachineWindowFullscreen::showInNecessaryMode()
{
    /* Make sure this window has fullscreen logic: */
    UIMachineLogicFullscreen *pFullscreenLogic = qobject_cast<UIMachineLogicFullscreen*>(machineLogic());
    AssertPtrReturnVoid(pFullscreenLogic);

    /* Make sure this window should be shown and mapped to some host-screen: */
    if (!uisession()->isScreenVisible(m_uScreenId) ||
        !pFullscreenLogic->hasHostScreenForGuestScreen(m_uScreenId))
    {
        /* Hide mini-toolbar: */
        if (m_pMiniToolBar)
            m_pMiniToolBar->hide();
        /* Hide window: */
        hide();
        return;
    }

    /* Make sure this window is not minimized: */
    if (isMinimized())
        return;

    /* Which host-screen should that machine-window located on? */
    const int iHostScreen = pFullscreenLogic->hostScreenForGuestScreen(m_uScreenId);

#ifdef Q_WS_X11
    /* On X11 calling placeOnScreen() is only needed for legacy window managers
     * which we do not test, so this is 'best effort' code. With window managers which
     * support the _NET_WM_FULLSCREEN_MONITORS protocol this would interfere unreliable. */
    const bool fSupportsNativeFullScreen = VBoxGlobal::supportsFullScreenMonitorsProtocolX11();
    if (!fSupportsNativeFullScreen)
        placeOnScreen();
#else /* !Q_WS_X11 */
    /* Make sure this window is maximized and placed on valid screen: */
    placeOnScreen();
#endif /* !Q_WS_X11 */

#ifdef Q_WS_MAC
    /* ML and next using native stuff, so we can call for simple show(),
     * Lion and previous using Qt stuff, so we should call for showFullScreen(): */
    const bool fSupportsNativeFullScreen = vboxGlobal().osRelease() > MacOSXRelease_Lion;
    if (fSupportsNativeFullScreen)
        show();
    else
        showFullScreen();
#else /* !Q_WS_MAC */
    /* Show in fullscreen mode: */
    showFullScreen();
#endif /* !Q_WS_MAC */

#ifdef Q_WS_X11
    if (fSupportsNativeFullScreen)
    {
        /* Tell recent window managers which screen this window should be mapped to.
         * Apparently some window managers will not respond to requests for
         * unmapped windows, so do this *after* the call to showFullScreen(). */
        VBoxGlobal::setFullScreenMonitorX11(this, iHostScreen);
    }
    else
    {
        /* On X11 calling placeOnScreen() is only needed for legacy window managers
         * which we do not test, so this is 'best effort' code. With window managers which
         * support the _NET_WM_FULLSCREEN_MONITORS protocol this would interfere unreliable. */
        placeOnScreen();
    }
#endif /* Q_WS_X11 */

    /* Adjust guest-screen size if necessary: */
    machineView()->maybeAdjustGuestScreenSize();

    /* Show/Move mini-toolbar into appropriate place: */
    if (m_pMiniToolBar)
    {
        m_pMiniToolBar->show();
        m_pMiniToolBar->adjustGeometry(iHostScreen);
    }
}

void UIMachineWindowFullscreen::updateAppearanceOf(int iElement)
{
    /* Call to base-class: */
    UIMachineWindow::updateAppearanceOf(iElement);

    /* Update mini-toolbar: */
    if (iElement & UIVisualElement_MiniToolBar)
    {
        if (m_pMiniToolBar)
        {
            /* Get machine: */
            const CMachine &m = machine();
            /* Get snapshot(s): */
            QString strSnapshotName;
            if (m.GetSnapshotCount() > 0)
            {
                CSnapshot snapshot = m.GetCurrentSnapshot();
                strSnapshotName = " (" + snapshot.GetName() + ")";
            }
            /* Update mini-toolbar text: */
            m_pMiniToolBar->setText(m.GetName() + strSnapshotName);
        }
    }
}

