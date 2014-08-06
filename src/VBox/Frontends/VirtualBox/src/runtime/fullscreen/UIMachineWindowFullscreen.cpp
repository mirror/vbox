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

    /* Make sure this window should be shown at all: */
    if (!uisession()->isScreenVisible(m_uScreenId))
        return;

    /* Make sure this window has fullscreen logic: */
    UIMachineLogicFullscreen *pFullscreenLogic = qobject_cast<UIMachineLogicFullscreen*>(machineLogic());
    AssertPtrReturnVoid(pFullscreenLogic);

    /* Make sure this window mapped to some host-screen: */
    if (!pFullscreenLogic->hasHostScreenForGuestScreen(m_uScreenId))
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
        UICocoaApplication::instance()->registerToNativeNotification("NSWindowWillEnterFullScreenNotification", this,
                                                                     UIMachineWindow::handleNativeNotification);
        UICocoaApplication::instance()->registerToNativeNotification("NSWindowDidEnterFullScreenNotification", this,
                                                                     UIMachineWindow::handleNativeNotification);
        UICocoaApplication::instance()->registerToNativeNotification("NSWindowWillExitFullScreenNotification", this,
                                                                     UIMachineWindow::handleNativeNotification);
        UICocoaApplication::instance()->registerToNativeNotification("NSWindowDidExitFullScreenNotification", this,
                                                                     UIMachineWindow::handleNativeNotification);
        UICocoaApplication::instance()->registerToNativeNotification("NSWindowDidFailToEnterFullScreenNotification", this,
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
                                              IntegrationMode_Embedded,
                                              gEDataManager->miniToolbarAlignment(vboxGlobal().managedVMUuid()),
                                              gEDataManager->autoHideMiniToolbar(vboxGlobal().managedVMUuid()));
    m_pMiniToolBar->addMenus(actionPool()->menus());
#ifndef RT_OS_DARWIN
    connect(m_pMiniToolBar, SIGNAL(sigMinimizeAction()), this, SLOT(showMinimized()));
#endif /* !RT_OS_DARWIN */
    connect(m_pMiniToolBar, SIGNAL(sigExitAction()),
            actionPool()->action(UIActionIndexRT_M_View_T_Fullscreen), SLOT(trigger()));
    connect(m_pMiniToolBar, SIGNAL(sigCloseAction()),
            actionPool()->action(UIActionIndexRT_M_Machine_S_Close), SLOT(trigger()));
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
        UICocoaApplication::instance()->unregisterFromNativeNotification("NSWindowWillEnterFullScreenNotification", this);
        UICocoaApplication::instance()->unregisterFromNativeNotification("NSWindowDidEnterFullScreenNotification", this);
        UICocoaApplication::instance()->unregisterFromNativeNotification("NSWindowWillExitFullScreenNotification", this);
        UICocoaApplication::instance()->unregisterFromNativeNotification("NSWindowDidExitFullScreenNotification", this);
        UICocoaApplication::instance()->unregisterFromNativeNotification("NSWindowDidFailToEnterFullScreenNotification", this);
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
    int iScreen = qobject_cast<UIMachineLogicFullscreen*>(machineLogic())->hostScreenForGuestScreen(m_uScreenId);
    /* Calculate working area: */
    QRect workingArea = QApplication::desktop()->screenGeometry(iScreen);
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
#else /* !Q_WS_MAC */
    /* Resize to the appropriate size: */
    resize(workingArea.size());
#endif /* !Q_WS_MAC */
    /* Adjust guest screen size if necessary: */
    machineView()->maybeAdjustGuestScreenSize();
    /* Move mini-toolbar into appropriate place: */
    if (m_pMiniToolBar)
        m_pMiniToolBar->adjustGeometry();
}

void UIMachineWindowFullscreen::showInNecessaryMode()
{
    /* Make sure this window should be shown at all: */
    if (!uisession()->isScreenVisible(m_uScreenId))
        return hide();

    /* Make sure this window has fullscreen logic: */
    UIMachineLogicFullscreen *pFullscreenLogic = qobject_cast<UIMachineLogicFullscreen*>(machineLogic());
    if (!pFullscreenLogic)
        return hide();

    /* Make sure this window mapped to some host-screen: */
    if (!pFullscreenLogic->hasHostScreenForGuestScreen(m_uScreenId))
        return hide();

    /* Make sure this window is not minimized: */
    if (isMinimized())
        return;

    /* Make sure this window is maximized and placed on valid screen: */
    placeOnScreen();

#ifdef Q_WS_MAC
    /* ML and next using native stuff, so we can call for simple show(): */
    if (vboxGlobal().osRelease() > MacOSXRelease_Lion) show();
    /* Lion and previous using Qt stuff, so we should call for showFullScreen(): */
    else showFullScreen();
#else /* !Q_WS_MAC */
    /* Show in fullscreen mode: */
    showFullScreen();
#endif /* !Q_WS_MAC */

#ifdef Q_WS_X11
    /* Make sure the window is placed on valid screen again
     * after window is shown & window's decorations applied.
     * That is required (still?) due to X11 Window Geometry Rules. */
    placeOnScreen();
#endif /* Q_WS_X11 */
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

