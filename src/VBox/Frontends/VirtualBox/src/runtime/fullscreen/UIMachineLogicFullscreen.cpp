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

/* GUI includes: */
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"
#include "UISession.h"
#include "UIActionPoolRuntime.h"
#include "UIMachineLogicFullscreen.h"
#include "UIMachineWindowFullscreen.h"
#include "UIMultiScreenLayout.h"
#ifdef Q_WS_MAC
# include "UIExtraDataEventHandler.h"
# include "VBoxUtils.h"
# include <Carbon/Carbon.h>
#endif /* Q_WS_MAC */

UIMachineLogicFullscreen::UIMachineLogicFullscreen(QObject *pParent, UISession *pSession)
    : UIMachineLogic(pParent, pSession, UIVisualStateType_Fullscreen)
#ifdef Q_WS_MAC
    , m_fIsFullscreenInvalidated(false)
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
        .arg(VBoxGlobal::extractKeyFromActionText(gActionPool->action(UIActionIndexRuntime_Toggle_Fullscreen)->text()));
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
void UIMachineLogicFullscreen::sltHandleNativeFullscreenDidEnter()
{
    /* Make sure this method is only used for ML and next: */
    AssertReturnVoid(vboxGlobal().osRelease() > MacOSXRelease_Lion);

    /* Get sender machine-window: */
    UIMachineWindow *pMachineWindow = qobject_cast<UIMachineWindow*>(sender());
    AssertReturnVoid(pMachineWindow);

    /* Add machine-window to corresponding set: */
    m_fullscreenMachineWindows.insert(pMachineWindow);
    AssertReturnVoid(m_fullscreenMachineWindows.contains(pMachineWindow));
}

void UIMachineLogicFullscreen::sltHandleNativeFullscreenDidExit()
{
    /* Make sure this method is only used for ML and next: */
    AssertReturnVoid(vboxGlobal().osRelease() > MacOSXRelease_Lion);

    /* Get sender machine-window: */
    UIMachineWindow *pMachineWindow = qobject_cast<UIMachineWindow*>(sender());
    AssertReturnVoid(pMachineWindow);

    /* Remove machine-window from corresponding set: */
    bool fResult = m_fullscreenMachineWindows.remove(pMachineWindow);
    AssertReturnVoid(fResult && !m_fullscreenMachineWindows.contains(pMachineWindow));
    Q_UNUSED(fResult);

    /* If there is/are still fullscreen window(s) present: */
    if (!m_fullscreenMachineWindows.isEmpty())
    {
        /* Exit fullscreen mode if it was not invalidated yet: */
        if (!m_fIsFullscreenInvalidated)
            emit sigNotifyAboutNativeFullscreenShouldBeExited();
    }
    /* If there is/are no more fullscreen window(s) left: */
    else
    {
        /* If fullscreen mode was just invalidated: */
        if (m_fIsFullscreenInvalidated)
        {
            LogRel(("UIMachineLogicFullscreen::sltHandleNativeFullscreenDidExit: "
                    "Machine-window(s) exited invalidated fullscreen, enter again...\n"));

            /* Mark fullscreen mode valid again: */
            m_fIsFullscreenInvalidated = false;
            /* Reactivate 'presentation mode': */
            setPresentationModeEnabled(true);
            /* Make sure all machine-window(s) have proper geometry: */
            foreach (UIMachineWindow *pMachineWindow, machineWindows())
                pMachineWindow->showInNecessaryMode();
            /* Ask all window(s) to re-enter fullscreen mode: */
            emit sigNotifyAboutNativeFullscreenShouldBeEntered();
        }
        /* If fullscreen mode was manually exited: */
        else
        {
            LogRel(("UIMachineLogicFullscreen::sltHandleNativeFullscreenDidExit: "
                    "Machine-window(s) exited fullscreen, changing visual-state to requested...\n"));

            /* Change visual-state to requested: */
            UIVisualStateType type = uisession()->requestedVisualState();
            if (type == UIVisualStateType_Invalid)
                type = UIVisualStateType_Normal;
            uisession()->setRequestedVisualState(UIVisualStateType_Invalid);
            uisession()->changeVisualState(type);
        }
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
        /* Ask all window(s) to exit fullscreen mode: */
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
        /* Ask all window(s) to exit fullscreen mode: */
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
        /* Ask all window(s) to exit fullscreen mode: */
        emit sigNotifyAboutNativeFullscreenShouldBeExited();
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

#ifdef Q_WS_MAC
void UIMachineLogicFullscreen::sltChangePresentationMode(bool /* fEnabled */)
{
    setPresentationModeEnabled(true);
}
#endif /* Q_WS_MAC */

void UIMachineLogicFullscreen::sltScreenLayoutChanged()
{
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
    /* Invalidate fullscreen mode for ML and next: */
    else if (shouldWeInvalidateFullscreenMode())
        invalidateFullscreenMode();
    else
    {
        /* Make sure all machine-window(s) have proper geometry: */
        foreach (UIMachineWindow *pMachineWindow, machineWindows())
            pMachineWindow->showInNecessaryMode();
    }
#else /* !Q_WS_MAC */
    /* Make sure all machine-window(s) have proper geometry: */
    foreach (UIMachineWindow *pMachineWindow, machineWindows())
        pMachineWindow->showInNecessaryMode();
#endif /* !Q_WS_MAC */
}

void UIMachineLogicFullscreen::sltGuestMonitorChange(KGuestMonitorChangedEventType changeType, ulong uScreenId, QRect screenGeo)
{
    LogRelFlow(("UIMachineLogicFullscreen: Guest-screen count changed.\n"));

    /* Update multi-screen layout before any window update: */
    if (changeType == KGuestMonitorChangedEventType_Enabled ||
        changeType == KGuestMonitorChangedEventType_Disabled)
        m_pScreenLayout->rebuild();

#ifdef Q_WS_MAC
    /* Call to base-class for Lion and previous: */
    if (vboxGlobal().osRelease() <= MacOSXRelease_Lion)
        UIMachineLogic::sltGuestMonitorChange(changeType, uScreenId, screenGeo);
    /* Invalidate fullscreen mode for ML and next: */
    else if (shouldWeInvalidateFullscreenMode())
        invalidateFullscreenMode();
    /* Call to base-class otherwise: */
    else UIMachineLogic::sltGuestMonitorChange(changeType, uScreenId, screenGeo);
#else /* !Q_WS_MAC */
    /* Call to base-class: */
    UIMachineLogic::sltGuestMonitorChange(changeType, uScreenId, screenGeo);
#endif /* !Q_WS_MAC */
}

void UIMachineLogicFullscreen::sltHostScreenCountChanged()
{
    LogRelFlow(("UIMachineLogicFullscreen: Host-screen count changed.\n"));

    /* Update multi-screen layout before any window update: */
    m_pScreenLayout->rebuild();

#ifdef Q_WS_MAC
    /* Call to base-class for Lion and previous: */
    if (vboxGlobal().osRelease() <= MacOSXRelease_Lion)
        UIMachineLogic::sltHostScreenCountChanged();
    /* Invalidate fullscreen mode for ML and next: */
    else if (shouldWeInvalidateFullscreenMode())
        invalidateFullscreenMode();
    /* Call to base-class otherwise: */
    else UIMachineLogic::sltHostScreenCountChanged();
#else /* !Q_WS_MAC */
    /* Call to base-class: */
    UIMachineLogic::sltHostScreenCountChanged();
#endif /* !Q_WS_MAC */
}

void UIMachineLogicFullscreen::prepareActionGroups()
{
    /* Call to base-class: */
    UIMachineLogic::prepareActionGroups();

    /* Adjust-window action isn't allowed in fullscreen: */
    gActionPool->action(UIActionIndexRuntime_Simple_AdjustWindow)->setVisible(false);

    /* Take care of view-action toggle state: */
    UIAction *pActionFullscreen = gActionPool->action(UIActionIndexRuntime_Toggle_Fullscreen);
    if (!pActionFullscreen->isChecked())
    {
        pActionFullscreen->blockSignals(true);
        pActionFullscreen->setChecked(true);
        pActionFullscreen->blockSignals(false);
        pActionFullscreen->update();
    }
}

void UIMachineLogicFullscreen::prepareActionConnections()
{
    /* Call to base-class: */
    UIMachineLogic::prepareActionConnections();

    /* "View" actions connections: */
    connect(gActionPool->action(UIActionIndexRuntime_Toggle_Fullscreen), SIGNAL(triggered(bool)),
            this, SLOT(sltChangeVisualStateToNormal()));
    connect(gActionPool->action(UIActionIndexRuntime_Toggle_Seamless), SIGNAL(triggered(bool)),
            this, SLOT(sltChangeVisualStateToSeamless()));
    connect(gActionPool->action(UIActionIndexRuntime_Toggle_Scale), SIGNAL(triggered(bool)),
            this, SLOT(sltChangeVisualStateToScale()));
}

#ifdef Q_WS_MAC
void UIMachineLogicFullscreen::prepareOtherConnections()
{
    /* Make sure 'presentation mode' preference handling
     * is updated at runtime for Lion and previous: */
    if (vboxGlobal().osRelease() <= MacOSXRelease_Lion)
        connect(gEDataEvents, SIGNAL(sigPresentationModeChange(bool)),
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
#endif /* Q_WS_MAC */

    /* Update the multi-screen layout: */
    m_pScreenLayout->update();

    /* Create machine-window(s): */
    for (uint cScreenId = 0; cScreenId < session().GetMachine().GetMonitorCount(); ++cScreenId)
        addMachineWindow(UIMachineWindow::create(this, cScreenId));

    /* Connect multi-screen layout change handler: */
    connect(m_pScreenLayout, SIGNAL(sigScreenLayoutChanged()),
            this, SLOT(sltScreenLayoutChanged()));

#ifdef Q_WS_MAC
    /* Activate 'presentation mode': */
    setPresentationModeEnabled(true);
    /* For ML and next: */
    if (vboxGlobal().osRelease() > MacOSXRelease_Lion)
    {
        /* Enable native fullscreen support: */
        foreach (UIMachineWindow *pMachineWindow, machineWindows())
        {
            /* Logic => window signals: */
            connect(this, SIGNAL(sigNotifyAboutNativeFullscreenShouldBeEntered()),
                    pMachineWindow, SLOT(sltEnterNativeFullscreen()));
            connect(this, SIGNAL(sigNotifyAboutNativeFullscreenShouldBeExited()),
                    pMachineWindow, SLOT(sltExitNativeFullscreen()));
            /* Window => logic signals: */
            connect(pMachineWindow, SIGNAL(sigNotifyAboutNativeFullscreenDidEnter()),
                    this, SLOT(sltHandleNativeFullscreenDidEnter()));
            connect(pMachineWindow, SIGNAL(sigNotifyAboutNativeFullscreenDidExit()),
                    this, SLOT(sltHandleNativeFullscreenDidExit()));
        }
        /* Ask all window(s) to enter fullscreen mode: */
        emit sigNotifyAboutNativeFullscreenShouldBeEntered();
    }
#endif /* Q_WS_MAC */

    /* Mark machine-window(s) created: */
    setMachineWindowsCreated(true);
}

void UIMachineLogicFullscreen::prepareMenu()
{
    /* Call to base-class: */
    UIMachineLogic::prepareMenu();

    /* Finally update view-menu, if necessary: */
    if (uisession()->allowedActionsMenuView() & RuntimeMenuViewActionType_Multiscreen)
        m_pScreenLayout->setViewMenu(gActionPool->action(UIActionIndexRuntime_Menu_View)->menu());
}

void UIMachineLogicFullscreen::cleanupMachineWindows()
{
    /* Do not destroy machine-window(s) if they destroyed already: */
    if (!isMachineWindowsCreated())
        return;

    /* Mark machine-window(s) destroyed: */
    setMachineWindowsCreated(false);

    /* Destroy machine-window(s): */
    foreach (UIMachineWindow *pMachineWindow, machineWindows())
        UIMachineWindow::destroy(pMachineWindow);

#ifdef Q_WS_MAC
    /* Deactivate 'presentation mode': */
    setPresentationModeEnabled(false);
#endif/* Q_WS_MAC */
}

void UIMachineLogicFullscreen::cleanupActionConnections()
{
    /* "View" actions disconnections: */
    disconnect(gActionPool->action(UIActionIndexRuntime_Toggle_Fullscreen), SIGNAL(triggered(bool)),
               this, SLOT(sltChangeVisualStateToNormal()));
    disconnect(gActionPool->action(UIActionIndexRuntime_Toggle_Seamless), SIGNAL(triggered(bool)),
               this, SLOT(sltChangeVisualStateToSeamless()));
    disconnect(gActionPool->action(UIActionIndexRuntime_Toggle_Scale), SIGNAL(triggered(bool)),
               this, SLOT(sltChangeVisualStateToScale()));

    /* Call to base-class: */
    UIMachineLogic::cleanupActionConnections();
}

void UIMachineLogicFullscreen::cleanupActionGroups()
{
    /* Take care of view-action toggle state: */
    UIAction *pActionFullscreen = gActionPool->action(UIActionIndexRuntime_Toggle_Fullscreen);
    if (pActionFullscreen->isChecked())
    {
        pActionFullscreen->blockSignals(true);
        pActionFullscreen->setChecked(false);
        pActionFullscreen->blockSignals(false);
        pActionFullscreen->update();
    }

    /* Reenable adjust-window action: */
    gActionPool->action(UIActionIndexRuntime_Simple_AdjustWindow)->setVisible(true);

    /* Call to base-class: */
    UIMachineLogic::cleanupActionGroups();
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
                /* Load 'presentation mode' preference: */
                QString strPresentationMode = vboxGlobal().virtualBox().GetExtraData(GUI_PresentationModeEnabled).toLower();
                /* Default to 'false' if it is an empty value: */
                if (strPresentationMode.isEmpty() || strPresentationMode == "false")
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

bool UIMachineLogicFullscreen::shouldWeInvalidateFullscreenMode() const
{
    /* Prepare result: */
    bool fSomethingInvalid = false;

    /* Enumerate windows: */
    foreach (UIMachineWindow *pMachineWindow, machineWindows())
    {
        /* Get screen ID: */
        ulong uScreenID = pMachineWindow->screenId();
        /* Check window which can be fullscreen: */
        if (uScreenID == 0 || darwinScreensHaveSeparateSpaces())
        {
            /* Check window which is not in fullscreen: */
            if (!darwinIsInFullscreenMode(pMachineWindow))
            {
                /* Check if that window should really
                 * be shown and is mapped to some host-screen: */
                if (   uisession()->isScreenVisible(uScreenID)
                    && hasHostScreenForGuestScreen(uScreenID))
                {
                    fSomethingInvalid = true;
                    break;
                }
            }
            /* Check window which is in fullscreen: */
            else
            {
                /* Check if that window shouldn't really
                 * be shown or isn't mapped to some host-screen: */
                if (   !uisession()->isScreenVisible(uScreenID)
                    || !hasHostScreenForGuestScreen(uScreenID))
                {
                    fSomethingInvalid = true;
                    break;
                }
                /* Check if that window should be located
                 * on another host-screen than currently. */
                if (hostScreenForGuestScreen((int)uScreenID) !=
                    QApplication::desktop()->screenNumber(pMachineWindow))
                {
                    fSomethingInvalid = true;
                    break;
                }
            }
        }
    }

    /* Return result: */
    return fSomethingInvalid;
}

void UIMachineLogicFullscreen::invalidateFullscreenMode()
{
    /* Make sure 'fullscreen' mode is not invalidated yet: */
    if (m_fIsFullscreenInvalidated)
        return;

    LogRel(("UIMachineLogicFullscreen::invalidateFullscreenMode: "
            "Fullscreen mode invalidated...\n"));

    /* Mark 'fullscreen' mode as invalidated: */
    m_fIsFullscreenInvalidated = true;

    /* Ask all window(s) to exit fullscreen mode: */
    emit sigNotifyAboutNativeFullscreenShouldBeExited();
}
#endif /* Q_WS_MAC */

