/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineLogicFullscreen class implementation.
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
#include <QTimer>

/* GUI includes: */
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"
#include "UISession.h"
#include "UIActionPoolRuntime.h"
#include "UIMachineLogicFullscreen.h"
#include "UIMachineWindowFullscreen.h"
#include "UIMultiScreenLayout.h"
#include "QIMenu.h"
#ifdef Q_WS_MAC
# include "UIExtraDataManager.h"
# include "VBoxUtils.h"
# include "UIFrameBuffer.h"
# include <Carbon/Carbon.h>
#endif /* Q_WS_MAC */


UIMachineLogicFullscreen::UIMachineLogicFullscreen(QObject *pParent, UISession *pSession)
    : UIMachineLogic(pParent, pSession, UIVisualStateType_Fullscreen)
    , m_pPopupMenu(0)
#ifdef Q_WS_MAC
    , m_fadeToken(kCGDisplayFadeReservationInvalidToken)
#endif /* Q_WS_MAC */
{
    /* Create multiscreen layout: */
    m_pScreenLayout = new UIMultiScreenLayout(this);
}

UIMachineLogicFullscreen::~UIMachineLogicFullscreen()
{
    /* Delete multiscreen layout: */
    delete m_pScreenLayout;
}

bool UIMachineLogicFullscreen::checkAvailability()
{
    /* Temporary get a machine object: */
    const CMachine &machine = uisession()->session().GetMachine();

    /* Check if there is enough physical memory to enter fullscreen: */
    if (uisession()->isGuestAdditionsActive())
    {
        quint64 availBits = machine.GetVRAMSize() /* VRAM */ * _1M /* MiB to bytes */ * 8 /* to bits */;
        quint64 usedBits = m_pScreenLayout->memoryRequirements();
        if (availBits < usedBits)
        {
            if (!msgCenter().cannotEnterFullscreenMode(0, 0, 0, (((usedBits + 7) / 8 + _1M - 1) / _1M) * _1M))
                return false;
        }
    }

    /* Take the toggle hot key from the menu item.
     * Since VBoxGlobal::extractKeyFromActionText gets exactly
     * the linked key without the 'Host+' part we are adding it here. */
    QString hotKey = QString("Host+%1")
        .arg(VBoxGlobal::extractKeyFromActionText(gpActionPool->action(UIActionIndexRT_M_View_T_Fullscreen)->text()));
    Assert(!hotKey.isEmpty());

    /* Show the info message. */
    if (!msgCenter().confirmGoingFullscreen(hotKey))
        return false;

    return true;
}

/** Adjusts guest screen count/size for the machine-logic we have. */
void UIMachineLogicFullscreen::maybeAdjustGuestScreenSize()
{
    /* We should rebuild screen-layout: */
    m_pScreenLayout->rebuild();
    /* Make sure all machine-window(s) have proper geometry: */
    foreach (UIMachineWindow *pMachineWindow, machineWindows())
        pMachineWindow->showInNecessaryMode();
}

int UIMachineLogicFullscreen::hostScreenForGuestScreen(int iScreenId) const
{
    return m_pScreenLayout->hostScreenForGuestScreen(iScreenId);
}

bool UIMachineLogicFullscreen::hasHostScreenForGuestScreen(int iScreenId) const
{
    return m_pScreenLayout->hasHostScreenForGuestScreen(iScreenId);
}

#ifdef RT_OS_DARWIN
void UIMachineLogicFullscreen::sltHandleNativeFullscreenWillEnter()
{
    /* Make sure this method is only used for ML and next: */
    AssertReturnVoid(vboxGlobal().osRelease() > MacOSXRelease_Lion);

    /* Get sender machine-window: */
    UIMachineWindow *pMachineWindow = qobject_cast<UIMachineWindow*>(sender());
    AssertPtrReturnVoid(pMachineWindow);
    LogRel(("UIMachineLogicFullscreen::sltHandleNativeFullscreenWillEnter: "
            "Machine-window #%d will enter native fullscreen.\n",
            (int)pMachineWindow->screenId()));

    /* Fade to black: */
    fadeToBlack();
}

void UIMachineLogicFullscreen::sltHandleNativeFullscreenDidEnter()
{
    /* Make sure this method is only used for ML and next: */
    AssertReturnVoid(vboxGlobal().osRelease() > MacOSXRelease_Lion);

    /* Get sender machine-window: */
    UIMachineWindow *pMachineWindow = qobject_cast<UIMachineWindow*>(sender());
    AssertPtrReturnVoid(pMachineWindow);
    LogRel(("UIMachineLogicFullscreen::sltHandleNativeFullscreenDidEnter: "
            "Machine-window #%d did enter native fullscreen.\n",
            (int)pMachineWindow->screenId()));

    /* Add machine-window to corresponding set: */
    m_fullscreenMachineWindows.insert(pMachineWindow);
    AssertReturnVoid(m_fullscreenMachineWindows.contains(pMachineWindow));

    /* Fade to normal if necessary: */
    QSet<UIMachineWindow*> visibleMachineWindows;
    foreach (UIMachineWindow *pMachineWindow, machineWindows())
        if (   uisession()->isScreenVisible(pMachineWindow->screenId())
            && hasHostScreenForGuestScreen(pMachineWindow->screenId()))
            visibleMachineWindows << pMachineWindow;
    if (   !darwinScreensHaveSeparateSpaces()
        || m_fullscreenMachineWindows == visibleMachineWindows)
        fadeToNormal();
}

void UIMachineLogicFullscreen::sltHandleNativeFullscreenWillExit()
{
    /* Make sure this method is only used for ML and next: */
    AssertReturnVoid(vboxGlobal().osRelease() > MacOSXRelease_Lion);

    /* Get sender machine-window: */
    UIMachineWindow *pMachineWindow = qobject_cast<UIMachineWindow*>(sender());
    AssertPtrReturnVoid(pMachineWindow);
    LogRel(("UIMachineLogicFullscreen::sltHandleNativeFullscreenWillExit: "
            "Machine-window #%d will exit native fullscreen.\n",
            (int)pMachineWindow->screenId()));

    /* Fade to black: */
    fadeToBlack();
}

void UIMachineLogicFullscreen::sltHandleNativeFullscreenDidExit()
{
    /* Make sure this method is only used for ML and next: */
    AssertReturnVoid(vboxGlobal().osRelease() > MacOSXRelease_Lion);

    /* Get sender machine-window: */
    UIMachineWindow *pMachineWindow = qobject_cast<UIMachineWindow*>(sender());
    AssertPtrReturnVoid(pMachineWindow);

    /* Remove machine-window from corresponding set: */
    bool fResult = m_fullscreenMachineWindows.remove(pMachineWindow);
    AssertReturnVoid(!m_fullscreenMachineWindows.contains(pMachineWindow));

    /* We have same signal if window did fail to enter native fullscreen.
     * In that case window missed in m_fullscreenMachineWindows,
     * ignore this signal silently: */
    if (!fResult)
        return;

    /* If that window was invalidated: */
    if (m_invalidFullscreenMachineWindows.contains(pMachineWindow))
    {
        LogRel(("UIMachineLogicFullscreen::sltHandleNativeFullscreenDidExit: "
                "Machine-window #%d exited invalidated native fullscreen, revalidate it.\n",
                (int)pMachineWindow->screenId()));

        /* Exclude window from invalidation list: */
        m_invalidFullscreenMachineWindows.remove(pMachineWindow);

        /* Revalidate 'fullscreen' window: */
        revalidateNativeFullScreen(pMachineWindow);
    }
    /* If there are no invalidated windows: */
    else if (m_invalidFullscreenMachineWindows.isEmpty())
    {
        /* If there are 'fullscreen' windows: */
        if (!m_fullscreenMachineWindows.isEmpty())
        {
            LogRel(("UIMachineLogicFullscreen::sltHandleNativeFullscreenDidExit: "
                    "Machine-window #%d exited native fullscreen, asking others to exit too...\n",
                    (int)pMachineWindow->screenId()));

            /* Ask window(s) to exit 'fullscreen' mode: */
            emit sigNotifyAboutNativeFullscreenShouldBeExited();
        }
        /* If there are no 'fullscreen' windows: */
        else
        {
            LogRel(("UIMachineLogicFullscreen::sltHandleNativeFullscreenDidExit: "
                    "Machine-window #%d exited native fullscreen, changing visual-state to requested...\n",
                    (int)pMachineWindow->screenId()));

            /* Change visual-state to requested: */
            UIVisualStateType type = uisession()->requestedVisualState();
            if (type == UIVisualStateType_Invalid)
                type = UIVisualStateType_Normal;
            uisession()->setRequestedVisualState(UIVisualStateType_Invalid);
            uisession()->changeVisualState(type);

            /* Fade to normal: */
            fadeToNormal();
        }
    }
}

void UIMachineLogicFullscreen::sltHandleNativeFullscreenFailToEnter()
{
    /* Make sure this method is only used for ML and next: */
    AssertReturnVoid(vboxGlobal().osRelease() > MacOSXRelease_Lion);

    /* Get sender machine-window: */
    UIMachineWindow *pMachineWindow = qobject_cast<UIMachineWindow*>(sender());
    AssertReturnVoid(pMachineWindow);

    /* Make sure this window is not registered somewhere: */
    AssertReturnVoid(!m_fullscreenMachineWindows.remove(pMachineWindow));
    AssertReturnVoid(!m_invalidFullscreenMachineWindows.remove(pMachineWindow));

    /* If there are 'fullscreen' windows: */
    if (!m_fullscreenMachineWindows.isEmpty())
    {
        LogRel(("UIMachineLogicFullscreen::sltHandleNativeFullscreenFailToEnter: "
                "Machine-window #%d failed to enter native fullscreen, asking others to exit...\n",
                (int)pMachineWindow->screenId()));

        /* Ask window(s) to exit 'fullscreen' mode: */
        emit sigNotifyAboutNativeFullscreenShouldBeExited();
    }
    /* If there are no 'fullscreen' windows: */
    else
    {
        LogRel(("UIMachineLogicFullscreen::sltHandleNativeFullscreenFailToEnter: "
                "Machine-window #%d failed to enter native fullscreen, requesting change visual-state to normal...\n",
                (int)pMachineWindow->screenId()));

        /* Ask session to change 'fullscreen' mode to 'normal': */
        uisession()->setRequestedVisualState(UIVisualStateType_Normal);

        /* If session started already => push mode-change directly: */
        if (uisession()->isStarted())
            sltCheckForRequestedVisualStateType();
    }
}

void UIMachineLogicFullscreen::sltChangeVisualStateToNormal()
{
    /* Base-class handling for Lion and previous: */
    if (vboxGlobal().osRelease() <= MacOSXRelease_Lion)
        UIMachineLogic::sltChangeVisualStateToNormal();
    /* Special handling for ML and next: */
    else
    {
        /* Request 'normal' (window) visual-state: */
        uisession()->setRequestedVisualState(UIVisualStateType_Normal);
        /* Ask window(s) to exit 'fullscreen' mode: */
        emit sigNotifyAboutNativeFullscreenShouldBeExited();
    }
}

void UIMachineLogicFullscreen::sltChangeVisualStateToSeamless()
{
    /* Base-class handling for Lion and previous: */
    if (vboxGlobal().osRelease() <= MacOSXRelease_Lion)
        UIMachineLogic::sltChangeVisualStateToSeamless();
    /* Special handling for ML and next: */
    else
    {
        /* Request 'seamless' visual-state: */
        uisession()->setRequestedVisualState(UIVisualStateType_Seamless);
        /* Ask window(s) to exit 'fullscreen' mode: */
        emit sigNotifyAboutNativeFullscreenShouldBeExited();
    }
}

void UIMachineLogicFullscreen::sltChangeVisualStateToScale()
{
    /* Base-class handling for Lion and previous: */
    if (vboxGlobal().osRelease() <= MacOSXRelease_Lion)
        UIMachineLogic::sltChangeVisualStateToScale();
    /* Special handling for ML and next: */
    else
    {
        /* Request 'scale' visual-state: */
        uisession()->setRequestedVisualState(UIVisualStateType_Scale);
        /* Ask window(s) to exit 'fullscreen' mode: */
        emit sigNotifyAboutNativeFullscreenShouldBeExited();
    }
}

void UIMachineLogicFullscreen::sltCheckForRequestedVisualStateType()
{
    /* Do not try to change visual-state type if machine was not started yet: */
    if (!uisession()->isRunning() && !uisession()->isPaused())
        return;

    /* Check requested visual-state types: */
    switch (uisession()->requestedVisualState())
    {
        /* If 'normal' visual-state type is requested: */
        case UIVisualStateType_Normal:
        {
            LogRel(("UIMachineLogicFullscreen::sltCheckForRequestedVisualStateType: "
                    "Going 'normal' as requested...\n"));
            uisession()->setRequestedVisualState(UIVisualStateType_Invalid);
            uisession()->changeVisualState(UIVisualStateType_Normal);
            break;
        }
        default:
            break;
    }
}
#endif /* RT_OS_DARWIN */

void UIMachineLogicFullscreen::sltMachineStateChanged()
{
    /* Call to base-class: */
    UIMachineLogic::sltMachineStateChanged();

    /* If machine-state changed from 'paused' to 'running': */
    if (uisession()->isRunning() && uisession()->wasPaused())
    {
        LogRelFlow(("UIMachineLogicFullscreen: "
                    "Machine-state changed from 'paused' to 'running': "
                    "Updating screen-layout...\n"));

        /* Make sure further code will be called just once: */
        uisession()->forgetPreviousMachineState();
        /* We should rebuild screen-layout: */
        m_pScreenLayout->rebuild();
        /* Make sure all machine-window(s) have proper geometry: */
        foreach (UIMachineWindow *pMachineWindow, machineWindows())
            pMachineWindow->showInNecessaryMode();
    }
}

void UIMachineLogicFullscreen::sltInvokePopupMenu()
{
    /* Popup main-menu if present: */
    if (m_pPopupMenu && !m_pPopupMenu->isEmpty())
    {
        m_pPopupMenu->popup(activeMachineWindow()->geometry().center());
        QTimer::singleShot(0, m_pPopupMenu, SLOT(sltHighlightFirstAction()));
    }
}

#ifdef Q_WS_MAC
void UIMachineLogicFullscreen::sltChangePresentationMode(bool /* fEnabled */)
{
    setPresentationModeEnabled(true);
}
#endif /* Q_WS_MAC */

void UIMachineLogicFullscreen::sltScreenLayoutChanged()
{
    LogRel(("UIMachineLogicFullscreen::sltScreenLayoutChanged: Multi-screen layout changed.\n"));

#ifdef Q_WS_MAC
    /* For Lion and previous: */
    if (vboxGlobal().osRelease() <= MacOSXRelease_Lion)
    {
        /* Make sure all machine-window(s) have proper geometry: */
        foreach (UIMachineWindow *pMachineWindow, machineWindows())
            pMachineWindow->showInNecessaryMode();
        /* Update 'presentation mode': */
        setPresentationModeEnabled(true);
    }
    /* Revalidate 'fullscreen' windows for ML and next: */
    else revalidateNativeFullScreen();
#else /* !Q_WS_MAC */
    /* Make sure all machine-window(s) have proper geometry: */
    foreach (UIMachineWindow *pMachineWindow, machineWindows())
        pMachineWindow->showInNecessaryMode();
#endif /* !Q_WS_MAC */
}

void UIMachineLogicFullscreen::sltGuestMonitorChange(KGuestMonitorChangedEventType changeType, ulong uScreenId, QRect screenGeo)
{
    LogRel(("UIMachineLogicFullscreen::sltGuestMonitorChange: Guest-screen count changed.\n"));

    /* Update multi-screen layout before any window update: */
    if (changeType == KGuestMonitorChangedEventType_Enabled ||
        changeType == KGuestMonitorChangedEventType_Disabled)
        m_pScreenLayout->rebuild();

#ifdef Q_WS_MAC
    /* Call to base-class for Lion and previous: */
    if (vboxGlobal().osRelease() <= MacOSXRelease_Lion)
        UIMachineLogic::sltGuestMonitorChange(changeType, uScreenId, screenGeo);
    /* Revalidate 'fullscreen' windows for ML and next: */
    else revalidateNativeFullScreen();
#else /* !Q_WS_MAC */
    /* Call to base-class: */
    UIMachineLogic::sltGuestMonitorChange(changeType, uScreenId, screenGeo);
#endif /* !Q_WS_MAC */
}

void UIMachineLogicFullscreen::sltHostScreenCountChanged()
{
    LogRel(("UIMachineLogicFullscreen::sltHostScreenCountChanged: Host-screen count changed.\n"));

    /* Update multi-screen layout before any window update: */
    m_pScreenLayout->rebuild();

#ifdef Q_WS_MAC
    /* Call to base-class for Lion and previous: */
    if (vboxGlobal().osRelease() <= MacOSXRelease_Lion)
        UIMachineLogic::sltHostScreenCountChanged();
    /* Revalidate 'fullscreen' windows for ML and next: */
    else revalidateNativeFullScreen();
#else /* !Q_WS_MAC */
    /* Call to base-class: */
    UIMachineLogic::sltHostScreenCountChanged();
#endif /* !Q_WS_MAC */
}

void UIMachineLogicFullscreen::prepareActionGroups()
{
    /* Call to base-class: */
    UIMachineLogic::prepareActionGroups();

    /* Take care of view-action toggle state: */
    UIAction *pActionFullscreen = gpActionPool->action(UIActionIndexRT_M_View_T_Fullscreen);
    if (!pActionFullscreen->isChecked())
    {
        pActionFullscreen->blockSignals(true);
        pActionFullscreen->setChecked(true);
        pActionFullscreen->blockSignals(false);
    }
}

void UIMachineLogicFullscreen::prepareActionConnections()
{
    /* Call to base-class: */
    UIMachineLogic::prepareActionConnections();

    /* "View" actions connections: */
    connect(gpActionPool->action(UIActionIndexRT_M_View_T_Fullscreen), SIGNAL(triggered(bool)),
            this, SLOT(sltChangeVisualStateToNormal()));
    connect(gpActionPool->action(UIActionIndexRT_M_View_T_Seamless), SIGNAL(triggered(bool)),
            this, SLOT(sltChangeVisualStateToSeamless()));
    connect(gpActionPool->action(UIActionIndexRT_M_View_T_Scale), SIGNAL(triggered(bool)),
            this, SLOT(sltChangeVisualStateToScale()));
}

#ifdef Q_WS_MAC
void UIMachineLogicFullscreen::prepareOtherConnections()
{
    /* Make sure 'presentation mode' preference handling
     * is updated at runtime for Lion and previous: */
    if (vboxGlobal().osRelease() <= MacOSXRelease_Lion)
        connect(gEDataManager, SIGNAL(sigPresentationModeChange(bool)),
                this, SLOT(sltChangePresentationMode(bool)));
}
#endif /* Q_WS_MAC */

void UIMachineLogicFullscreen::prepareMachineWindows()
{
    /* Do not create machine-window(s) if they created already: */
    if (isMachineWindowsCreated())
        return;

#ifdef Q_WS_MAC
    /* We have to make sure that we are getting the front most process.
     * This is necessary for Qt versions > 4.3.3: */
    darwinSetFrontMostProcess();

    /* Fade to black: */
    fadeToBlack();
#endif /* Q_WS_MAC */

    /* Update the multi-screen layout: */
    m_pScreenLayout->update();

    /* Create machine-window(s): */
    for (uint cScreenId = 0; cScreenId < session().GetMachine().GetMonitorCount(); ++cScreenId)
        addMachineWindow(UIMachineWindow::create(this, cScreenId));

    /* Listen for frame-buffer resize: */
    foreach (UIMachineWindow *pMachineWindow, machineWindows())
        connect(pMachineWindow, SIGNAL(sigFrameBufferResize()),
                this, SIGNAL(sigFrameBufferResize()));

    /* Connect multi-screen layout change handler: */
    connect(m_pScreenLayout, SIGNAL(sigScreenLayoutChanged()),
            this, SLOT(sltScreenLayoutChanged()));

#ifdef Q_WS_MAC
    /* Activate 'presentation mode': */
    setPresentationModeEnabled(true);

    /* For Lion and previous fade to normal: */
    if (vboxGlobal().osRelease() <= MacOSXRelease_Lion)
        fadeToNormal();

    /* For ML and next: */
    if (vboxGlobal().osRelease() > MacOSXRelease_Lion)
    {
        /* Enable native fullscreen support: */
        foreach (UIMachineWindow *pMachineWindow, machineWindows())
        {
            /* Logic => window signals: */
            connect(this, SIGNAL(sigNotifyAboutNativeFullscreenShouldBeEntered(UIMachineWindow*)),
                    pMachineWindow, SLOT(sltEnterNativeFullscreen(UIMachineWindow*)));
            connect(this, SIGNAL(sigNotifyAboutNativeFullscreenShouldBeExited(UIMachineWindow*)),
                    pMachineWindow, SLOT(sltExitNativeFullscreen(UIMachineWindow*)));
            /* Window => logic signals: */
            connect(pMachineWindow, SIGNAL(sigNotifyAboutNativeFullscreenWillEnter()),
                    this, SLOT(sltHandleNativeFullscreenWillEnter()),
                    Qt::QueuedConnection);
            connect(pMachineWindow, SIGNAL(sigNotifyAboutNativeFullscreenDidEnter()),
                    this, SLOT(sltHandleNativeFullscreenDidEnter()),
                    Qt::QueuedConnection);
            connect(pMachineWindow, SIGNAL(sigNotifyAboutNativeFullscreenWillExit()),
                    this, SLOT(sltHandleNativeFullscreenWillExit()),
                    Qt::QueuedConnection);
            connect(pMachineWindow, SIGNAL(sigNotifyAboutNativeFullscreenDidExit()),
                    this, SLOT(sltHandleNativeFullscreenDidExit()),
                    Qt::QueuedConnection);
            connect(pMachineWindow, SIGNAL(sigNotifyAboutNativeFullscreenFailToEnter()),
                    this, SLOT(sltHandleNativeFullscreenFailToEnter()),
                    Qt::QueuedConnection);
        }
        /* Revalidate 'fullscreen' windows: */
        revalidateNativeFullScreen();
    }
#endif /* Q_WS_MAC */

    /* Mark machine-window(s) created: */
    setMachineWindowsCreated(true);
}

void UIMachineLogicFullscreen::prepareMenu()
{
    /* Call to base-class: */
    UIMachineLogic::prepareMenu();

    /* Prepare popup-menu: */
    m_pPopupMenu = new QIMenu;
    AssertPtrReturnVoid(m_pPopupMenu);
    {
        /* Prepare popup-menu: */
        foreach (QMenu *pMenu, menus())
            m_pPopupMenu->addMenu(pMenu);
    }
}

void UIMachineLogicFullscreen::cleanupMenu()
{
    /* Cleanup popup-menu: */
    delete m_pPopupMenu;
    m_pPopupMenu = 0;

    /* Call to base-class: */
    UIMachineLogic::cleanupMenu();
}

void UIMachineLogicFullscreen::cleanupMachineWindows()
{
    /* Do not destroy machine-window(s) if they destroyed already: */
    if (!isMachineWindowsCreated())
        return;

#ifdef Q_WS_MAC
    /* For Lion and previous fade to black: */
    if (vboxGlobal().osRelease() <= MacOSXRelease_Lion)
        fadeToBlack();
#endif/* Q_WS_MAC */

    /* Mark machine-window(s) destroyed: */
    setMachineWindowsCreated(false);

    /* Destroy machine-window(s): */
    foreach (UIMachineWindow *pMachineWindow, machineWindows())
        UIMachineWindow::destroy(pMachineWindow);

#ifdef Q_WS_MAC
    /* Deactivate 'presentation mode': */
    setPresentationModeEnabled(false);

    /* Fade to normal: */
    fadeToNormal();
#endif/* Q_WS_MAC */
}

void UIMachineLogicFullscreen::cleanupActionConnections()
{
    /* "View" actions disconnections: */
    disconnect(gpActionPool->action(UIActionIndexRT_M_View_T_Fullscreen), SIGNAL(triggered(bool)),
               this, SLOT(sltChangeVisualStateToNormal()));
    disconnect(gpActionPool->action(UIActionIndexRT_M_View_T_Seamless), SIGNAL(triggered(bool)),
               this, SLOT(sltChangeVisualStateToSeamless()));
    disconnect(gpActionPool->action(UIActionIndexRT_M_View_T_Scale), SIGNAL(triggered(bool)),
               this, SLOT(sltChangeVisualStateToScale()));

    /* Call to base-class: */
    UIMachineLogic::cleanupActionConnections();
}

void UIMachineLogicFullscreen::cleanupActionGroups()
{
    /* Take care of view-action toggle state: */
    UIAction *pActionFullscreen = gpActionPool->action(UIActionIndexRT_M_View_T_Fullscreen);
    if (pActionFullscreen->isChecked())
    {
        pActionFullscreen->blockSignals(true);
        pActionFullscreen->setChecked(false);
        pActionFullscreen->blockSignals(false);
    }

    /* Call to base-class: */
    UIMachineLogic::cleanupActionGroups();
}

void UIMachineLogicFullscreen::updateMenuView()
{
    /* Call to base-class: */
    UIMachineLogic::updateMenuView();

    /* Get corresponding menu: */
    QMenu *pMenu = gpActionPool->action(UIActionIndexRT_M_View)->menu();
    AssertPtrReturnVoid(pMenu);

    /* Append 'Multiscreen' submenu, if allowed: */
    if (uisession()->allowedActionsMenuView() & RuntimeMenuViewActionType_Multiscreen)
        m_pScreenLayout->setViewMenu(pMenu);
}

#ifdef Q_WS_MAC
void UIMachineLogicFullscreen::setPresentationModeEnabled(bool fEnabled)
{
    /* Should we enable it? */
    if (fEnabled)
    {
        /* For Lion and previous: */
        if (vboxGlobal().osRelease() <= MacOSXRelease_Lion)
        {
            /* Check if we have screen which contains the Dock or the Menubar (which hasn't to be the same),
             * only than the 'presentation mode' have to be changed. */
            if (m_pScreenLayout->isHostTaskbarCovert())
            {
                if (gEDataManager->presentationModeEnabled(vboxGlobal().managedVMUuid()))
                    SetSystemUIMode(kUIModeAllHidden, 0);
                else
                    SetSystemUIMode(kUIModeAllSuppressed, 0);
            }
        }
        /* For ML and next: */
        else
        {
            /* I am not sure we have to check anything here.
             * Without 'presentation mode' native fullscreen works pretty bad,
             * so we have to enable it anyway: */
            SetSystemUIMode(kUIModeAllSuppressed, 0);
        }
    }
    /* Should we disable it? */
    else SetSystemUIMode(kUIModeNormal, 0);
}

void UIMachineLogicFullscreen::fadeToBlack()
{
    /* Make sure fade-token invalid: */
    if (m_fadeToken != kCGDisplayFadeReservationInvalidToken)
        return;

    /* Acquire fade-token: */
    LogRel(("UIMachineLogicFullscreen::fadeToBlack\n"));
    CGAcquireDisplayFadeReservation(kCGMaxDisplayReservationInterval, &m_fadeToken);
    CGDisplayFade(m_fadeToken, 0.3, kCGDisplayBlendNormal, kCGDisplayBlendSolidColor, 0.0, 0.0, 0.0, true);
}

void UIMachineLogicFullscreen::fadeToNormal()
{
    /* Make sure fade-token valid: */
    if (m_fadeToken == kCGDisplayFadeReservationInvalidToken)
        return;

    /* Release fade-token: */
    LogRel(("UIMachineLogicFullscreen::fadeToNormal\n"));
    CGDisplayFade(m_fadeToken, 0.5, kCGDisplayBlendSolidColor, kCGDisplayBlendNormal, 0.0, 0.0, 0.0, false);
    CGReleaseDisplayFadeReservation(m_fadeToken);
    m_fadeToken = kCGDisplayFadeReservationInvalidToken;
}

void UIMachineLogicFullscreen::revalidateNativeFullScreen(UIMachineWindow *pMachineWindow)
{
    /* Make sure window is not already invalidated: */
    if (m_invalidFullscreenMachineWindows.contains(pMachineWindow))
        return;

    /* Ignore window if it is in 'fullscreen transition': */
    if (qobject_cast<UIMachineWindowFullscreen*>(pMachineWindow)->isInFullscreenTransition())
        return;

    /* Get screen ID: */
    ulong uScreenID = pMachineWindow->screenId();
    LogRel(("UIMachineLogicFullscreen::revalidateNativeFullScreen: For machine-window #%d.\n",
            (int)uScreenID));

    /* Validate window which can't be fullscreen: */
    if (uScreenID != 0 && !darwinScreensHaveSeparateSpaces())
    {
        LogRel(("UIMachineLogicFullscreen::revalidateNativeFullScreen: "
                "Ask machine-window #%d to show/normalize.\n", (int)uScreenID));

        /* Make sure window have proper geometry: */
        pMachineWindow->showInNecessaryMode();
    }
    /* Validate window which can be fullscreen: */
    else
    {
        /* Validate window which is not in fullscreen: */
        if (!darwinIsInFullscreenMode(pMachineWindow))
        {
            /* If that window
             * 1. should really be shown and
             * 2. is mapped to some host-screen: */
            if (   uisession()->isScreenVisible(uScreenID)
                && hasHostScreenForGuestScreen(uScreenID))
            {
                LogRel(("UIMachineLogicFullscreen::revalidateNativeFullScreen: "
                        "Ask machine-window #%d to enter native fullscreen.\n", (int)uScreenID));

                /* Fade to black: */
                fadeToBlack();

                /* Update 'presentation mode': */
                setPresentationModeEnabled(true);

                /* Make sure window have proper geometry and shown: */
                pMachineWindow->showInNecessaryMode();

                /* Ask window to enter 'fullscreen' mode: */
                emit sigNotifyAboutNativeFullscreenShouldBeEntered(pMachineWindow);
            }
            /* If that window
             * is shown while shouldn't: */
            else if (pMachineWindow->isVisible())
            {
                LogRel(("UIMachineLogicFullscreen::revalidateNativeFullScreen: "
                        "Ask machine-window #%d to hide.\n", (int)uScreenID));

                /* Else make sure that window is hidden: */
                pMachineWindow->showInNecessaryMode();

                /* Fade to normal: */
                fadeToNormal();
            }
        }
        /* Validate window which is in fullscreen: */
        else
        {
            /* Variables to compare: */
            const int iWantedHostScreenIndex = hostScreenForGuestScreen((int)uScreenID);
            const int iCurrentHostScreenIndex = QApplication::desktop()->screenNumber(pMachineWindow);
            const QSize frameBufferSize((int)uisession()->frameBuffer(uScreenID)->width(), (int)uisession()->frameBuffer(uScreenID)->height());
            const QSize screenSize = QApplication::desktop()->screenGeometry(iWantedHostScreenIndex).size();

            /* If that window
             * 1. shouldn't really be shown or
             * 2. isn't mapped to some host-screen or
             * 3. should be located on another host-screen than currently or
             * 4. have another frame-buffer size than actually should. */
            if (   !uisession()->isScreenVisible(uScreenID)
                || !hasHostScreenForGuestScreen(uScreenID)
                || iWantedHostScreenIndex != iCurrentHostScreenIndex
                || frameBufferSize != screenSize)
            {
                LogRel(("UIMachineLogicFullscreen::revalidateNativeFullScreen: "
                        "Ask machine-window #%d to exit native fullscreen.\n", (int)uScreenID));

                /* Fade to black: */
                fadeToBlack();

                /* Mark window as invalidated: */
                m_invalidFullscreenMachineWindows << pMachineWindow;

                /* Ask window to exit 'fullscreen' mode: */
                emit sigNotifyAboutNativeFullscreenShouldBeExited(pMachineWindow);
            }
        }
    }
}

void UIMachineLogicFullscreen::revalidateNativeFullScreen()
{
    /* Revalidate all fullscreen windows: */
    foreach (UIMachineWindow *pMachineWindow, machineWindows())
        revalidateNativeFullScreen(pMachineWindow);
}
#endif /* Q_WS_MAC */

