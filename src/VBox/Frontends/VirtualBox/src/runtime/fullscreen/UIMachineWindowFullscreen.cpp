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
    , m_pMainMenu(0)
    , m_pMiniToolBar(0)
{
}

#ifdef Q_WS_MAC
void UIMachineWindowFullscreen::handleNativeNotification(const QString &strNativeNotificationName)
{
    /* Make sure this method is only used for ML and next: */
    AssertReturnVoid(vboxGlobal().osRelease() > MacOSXRelease_Lion);

    /* Handle arrived notification: */
    LogRel(("UIMachineWindowFullscreen::handleNativeNotification: Notification '%s' received.\n",
            strNativeNotificationName.toAscii().constData()));
    /* Handle 'NSWindowDidEnterFullScreenNotification' notification: */
    if (strNativeNotificationName == "NSWindowDidEnterFullScreenNotification")
    {
        LogRel(("UIMachineWindowFullscreen::handleNativeNotification: "
                "Native fullscreen mode entered, notifying listener...\n"));
        emit sigNotifyAboutNativeFullscreenDidEnter();
    }
    /* Handle 'NSWindowDidExitFullScreenNotification' notification: */
    else if (strNativeNotificationName == "NSWindowDidExitFullScreenNotification")
    {
        LogRel(("UIMachineWindowFullscreen::handleNativeNotification: "
                "Native fullscreen mode exited, notifying listener...\n"));
        emit sigNotifyAboutNativeFullscreenDidExit();
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

void UIMachineWindowFullscreen::sltPopupMainMenu()
{
    /* Popup main-menu if present: */
    if (m_pMainMenu && !m_pMainMenu->isEmpty())
    {
        m_pMainMenu->popup(geometry().center());
        QTimer::singleShot(0, m_pMainMenu, SLOT(sltHighlightFirstAction()));
    }
}

void UIMachineWindowFullscreen::prepareMenu()
{
    /* Call to base-class: */
    UIMachineWindow::prepareMenu();

    /* Prepare menu: */
    CMachine machine = session().GetMachine();
    RuntimeMenuType restrictedMenus = VBoxGlobal::restrictedRuntimeMenuTypes(machine);
    RuntimeMenuType allowedMenus = static_cast<RuntimeMenuType>(RuntimeMenuType_All ^ restrictedMenus);
    m_pMainMenu = uisession()->newMenu(allowedMenus);
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
        /* Enable fullscreen support for every screen which requires it: */
        if (darwinScreensHaveSeparateSpaces() || m_uScreenId == 0)
            darwinEnableFullscreenSupport(this);
        /* Enable transience support for other screens: */
        else
            darwinEnableTransienceSupport(this);
        /* Register to native fullscreen notifications: */
        UICocoaApplication::instance()->registerToNativeNotification("NSWindowDidEnterFullScreenNotification", this,
                                                                     UIMachineWindow::handleNativeNotification);
        UICocoaApplication::instance()->registerToNativeNotification("NSWindowDidExitFullScreenNotification", this,
                                                                     UIMachineWindow::handleNativeNotification);
    }
#endif /* Q_WS_MAC */
}

void UIMachineWindowFullscreen::prepareMiniToolbar()
{
    /* Get machine: */
    CMachine m = machine();

    /* Make sure mini-toolbar is necessary: */
    bool fIsActive = m.GetExtraData(GUI_ShowMiniToolBar) != "no";
    if (!fIsActive)
        return;

    /* Get the mini-toolbar alignment: */
    bool fIsAtTop = m.GetExtraData(GUI_MiniToolBarAlignment) == "top";
    /* Get the mini-toolbar auto-hide feature availability: */
    bool fIsAutoHide = m.GetExtraData(GUI_MiniToolBarAutoHide) != "off";
    /* Create mini-toolbar: */
    m_pMiniToolBar = new UIRuntimeMiniToolBar(this,
                                              fIsAtTop ? Qt::AlignTop : Qt::AlignBottom,
                                              IntegrationMode_Embedded,
                                              fIsAutoHide);
    QList<QMenu*> menus;
    RuntimeMenuType restrictedMenus = VBoxGlobal::restrictedRuntimeMenuTypes(m);
    RuntimeMenuType allowedMenus = static_cast<RuntimeMenuType>(RuntimeMenuType_All ^ restrictedMenus);
    QList<QAction*> actions = uisession()->newMenu(allowedMenus)->actions();
    for (int i=0; i < actions.size(); ++i)
        menus << actions.at(i)->menu();
    m_pMiniToolBar->addMenus(menus);
#ifndef RT_OS_DARWIN
    connect(m_pMiniToolBar, SIGNAL(sigMinimizeAction()), this, SLOT(showMinimized()));
#endif /* !RT_OS_DARWIN */
    connect(m_pMiniToolBar, SIGNAL(sigExitAction()),
            gActionPool->action(UIActionIndexRuntime_Toggle_Fullscreen), SLOT(trigger()));
    connect(m_pMiniToolBar, SIGNAL(sigCloseAction()),
            gActionPool->action(UIActionIndexRuntime_Simple_Close), SLOT(trigger()));
}

void UIMachineWindowFullscreen::cleanupMiniToolbar()
{
    /* Make sure mini-toolbar was created: */
    if (!m_pMiniToolBar)
        return;

    /* Save mini-toolbar settings: */
    machine().SetExtraData(GUI_MiniToolBarAutoHide, m_pMiniToolBar->autoHide() ? QString() : "off");
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
        UICocoaApplication::instance()->unregisterFromNativeNotification("NSWindowDidEnterFullScreenNotification", this);
        UICocoaApplication::instance()->unregisterFromNativeNotification("NSWindowDidExitFullScreenNotification", this);
    }
#endif /* Q_WS_MAC */

    /* Cleanup mini-toolbar: */
    cleanupMiniToolbar();

    /* Call to base-class: */
    UIMachineWindow::cleanupVisualState();
}

void UIMachineWindowFullscreen::cleanupMenu()
{
    /* Cleanup menu: */
    delete m_pMainMenu;
    m_pMainMenu = 0;

    /* Call to base-class: */
    UIMachineWindow::cleanupMenu();
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
    /* Resize to the appropriate size on Lion and previous: */
    if (vboxGlobal().osRelease() <= MacOSXRelease_Lion)
        resize(workingArea.size());
    /* Resize to the appropriate size on ML and next
     * only if that screen has no own user-space: */
    else if (!darwinScreensHaveSeparateSpaces() && m_uScreenId != 0)
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
    /* Process pending move & resize events: */
    qApp->processEvents();
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

#ifdef Q_WS_WIN
    /* On Windows we should activate main window first,
     * because entering fullscreen there doesn't means window will be auto-activated,
     * so no window-activation event will be received and no keyboard-hook created otherwise... */
    if (m_uScreenId == 0)
        setWindowState(windowState() | Qt::WindowActive);
#endif /* Q_WS_WIN */

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

