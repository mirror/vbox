/* $Id$ */
/** @file
 * VBox Qt GUI - UIMachineLogicNormal class implementation.
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
#include <QMenu>

/* GUI includes: */
#include "VBoxGlobal.h"
#include "UIMessageCenter.h"
#include "UISession.h"
#include "UIActionPoolRuntime.h"
#include "UIMachineLogicNormal.h"
#include "UIMachineWindow.h"
#include "UIMenuBarEditorWindow.h"
#include "UIStatusBarEditorWindow.h"
#include "UIExtraDataManager.h"
#include "UIFrameBuffer.h"
#ifdef Q_WS_MAC
#include "VBoxUtils.h"
#endif /* Q_WS_MAC */

/* COM includes: */
#include "CConsole.h"
#include "CDisplay.h"

UIMachineLogicNormal::UIMachineLogicNormal(QObject *pParent, UISession *pSession)
    : UIMachineLogic(pParent, pSession, UIVisualStateType_Normal)
{
}

bool UIMachineLogicNormal::checkAvailability()
{
    /* Normal mode is always available: */
    return true;
}

void UIMachineLogicNormal::sltCheckForRequestedVisualStateType()
{
    /* Do not try to change visual-state type if machine was not started yet: */
    if (!uisession()->isRunning() && !uisession()->isPaused())
        return;

    /* Check requested visual-state types: */
    switch (uisession()->requestedVisualState())
    {
        /* If 'seamless' visual-state type is requested: */
        case UIVisualStateType_Seamless:
        {
            /* And supported: */
            if (uisession()->isGuestSupportsSeamless())
            {
                LogRel(("UIMachineLogicNormal::sltCheckForRequestedVisualStateType: "
                        "Going 'seamless' as requested...\n"));
                uisession()->setRequestedVisualState(UIVisualStateType_Invalid);
                uisession()->changeVisualState(UIVisualStateType_Seamless);
            }
            break;
        }
        default:
            break;
    }
}

void UIMachineLogicNormal::sltOpenMenuBarSettings()
{
    /* Do not process if window(s) missed! */
    AssertReturnVoid(isMachineWindowsCreated());

    /* Make sure menu-bar is enabled: */
    const bool fEnabled = actionPool()->action(UIActionIndexRT_M_View_M_MenuBar_T_Visibility)->isChecked();
    AssertReturnVoid(fEnabled);

    /* Prevent user from opening another one editor or toggle menu-bar: */
    actionPool()->action(UIActionIndexRT_M_View_M_MenuBar_S_Settings)->setEnabled(false);
    actionPool()->action(UIActionIndexRT_M_View_M_MenuBar_T_Visibility)->setEnabled(false);
    /* Create menu-bar editor: */
    UIMenuBarEditorWindow *pMenuBarEditor = new UIMenuBarEditorWindow(activeMachineWindow(), actionPool());
    AssertPtrReturnVoid(pMenuBarEditor);
    {
        /* Configure menu-bar editor: */
        connect(pMenuBarEditor, SIGNAL(destroyed(QObject*)),
                this, SLOT(sltMenuBarSettingsClosed()));
#ifdef Q_WS_MAC
        connect(this, SIGNAL(sigNotifyAbout3DOverlayVisibilityChange(bool)),
                pMenuBarEditor, SLOT(sltActivateWindow()));
#endif /* Q_WS_MAC */
        /* Show window: */
        pMenuBarEditor->show();
    }
}

void UIMachineLogicNormal::sltMenuBarSettingsClosed()
{
    /* Make sure menu-bar is enabled: */
    const bool fEnabled = actionPool()->action(UIActionIndexRT_M_View_M_MenuBar_T_Visibility)->isChecked();
    AssertReturnVoid(fEnabled);

    /* Allow user to open editor and toggle menu-bar again: */
    actionPool()->action(UIActionIndexRT_M_View_M_MenuBar_S_Settings)->setEnabled(true);
    actionPool()->action(UIActionIndexRT_M_View_M_MenuBar_T_Visibility)->setEnabled(true);
}

void UIMachineLogicNormal::sltToggleMenuBar()
{
    /* Do not process if window(s) missed! */
    AssertReturnVoid(isMachineWindowsCreated());

    /* Invert menu-bar availability option: */
    const bool fEnabled = gEDataManager->menuBarEnabled(vboxGlobal().managedVMUuid());
    gEDataManager->setMenuBarEnabled(!fEnabled, vboxGlobal().managedVMUuid());
}

void UIMachineLogicNormal::sltOpenStatusBarSettings()
{
    /* Do not process if window(s) missed! */
    AssertReturnVoid(isMachineWindowsCreated());

    /* Make sure status-bar is enabled: */
    const bool fEnabled = actionPool()->action(UIActionIndexRT_M_View_M_StatusBar_T_Visibility)->isChecked();
    AssertReturnVoid(fEnabled);

    /* Prevent user from opening another one editor or toggle status-bar: */
    actionPool()->action(UIActionIndexRT_M_View_M_StatusBar_S_Settings)->setEnabled(false);
    actionPool()->action(UIActionIndexRT_M_View_M_StatusBar_T_Visibility)->setEnabled(false);
    /* Create status-bar editor: */
    UIStatusBarEditorWindow *pStatusBarEditor = new UIStatusBarEditorWindow(activeMachineWindow());
    AssertPtrReturnVoid(pStatusBarEditor);
    {
        /* Configure status-bar editor: */
        connect(pStatusBarEditor, SIGNAL(destroyed(QObject*)),
                this, SLOT(sltStatusBarSettingsClosed()));
#ifdef Q_WS_MAC
        connect(this, SIGNAL(sigNotifyAbout3DOverlayVisibilityChange(bool)),
                pStatusBarEditor, SLOT(sltActivateWindow()));
#endif /* Q_WS_MAC */
        /* Show window: */
        pStatusBarEditor->show();
    }
}

void UIMachineLogicNormal::sltStatusBarSettingsClosed()
{
    /* Make sure status-bar is enabled: */
    const bool fEnabled = actionPool()->action(UIActionIndexRT_M_View_M_StatusBar_T_Visibility)->isChecked();
    AssertReturnVoid(fEnabled);

    /* Allow user to open editor and toggle status-bar again: */
    actionPool()->action(UIActionIndexRT_M_View_M_StatusBar_S_Settings)->setEnabled(true);
    actionPool()->action(UIActionIndexRT_M_View_M_StatusBar_T_Visibility)->setEnabled(true);
}

void UIMachineLogicNormal::sltToggleStatusBar()
{
    /* Do not process if window(s) missed! */
    AssertReturnVoid(isMachineWindowsCreated());

    /* Invert status-bar availability option: */
    const bool fEnabled = gEDataManager->statusBarEnabled(vboxGlobal().managedVMUuid());
    gEDataManager->setStatusBarEnabled(!fEnabled, vboxGlobal().managedVMUuid());
}

void UIMachineLogicNormal::sltHandleActionTriggerViewScreenToggle(int iIndex, bool fEnabled)
{
    /* Enable/disable guest keeping current size: */
    ULONG uWidth, uHeight, uBitsPerPixel;
    LONG uOriginX, uOriginY;
    CDisplay display = session().GetConsole().GetDisplay();
    display.GetScreenResolution(iIndex, uWidth, uHeight, uBitsPerPixel, uOriginX, uOriginY);
    if (!fEnabled)
        display.SetVideoModeHint(iIndex, false, false, 0, 0, 0, 0, 0);
    else
    {
        /* Defaults: */
        if (!uWidth)
            uWidth = 800;
        if (!uHeight)
            uHeight = 600;
        display.SetVideoModeHint(iIndex, true, false, 0, 0, uWidth, uHeight, 32);
    }
}

void UIMachineLogicNormal::sltHandleActionTriggerViewScreenResize(int iIndex, const QSize &size)
{
    /* Resize guest to required size: */
    CDisplay display = session().GetConsole().GetDisplay();
    display.SetVideoModeHint(iIndex, uisession()->isScreenVisible(iIndex),
                             false, 0, 0, size.width(), size.height(), 0);
}

void UIMachineLogicNormal::prepareActionConnections()
{
    /* Call to base-class: */
    UIMachineLogic::prepareActionConnections();

    /* Prepare 'View' actions connections: */
    connect(actionPool()->action(UIActionIndexRT_M_View_T_Fullscreen), SIGNAL(triggered(bool)),
            this, SLOT(sltChangeVisualStateToFullscreen()));
    connect(actionPool()->action(UIActionIndexRT_M_View_T_Seamless), SIGNAL(triggered(bool)),
            this, SLOT(sltChangeVisualStateToSeamless()));
    connect(actionPool()->action(UIActionIndexRT_M_View_T_Scale), SIGNAL(triggered(bool)),
            this, SLOT(sltChangeVisualStateToScale()));
    connect(actionPool()->action(UIActionIndexRT_M_View_M_MenuBar_S_Settings), SIGNAL(triggered(bool)),
            this, SLOT(sltOpenMenuBarSettings()));
    connect(actionPool()->action(UIActionIndexRT_M_View_M_MenuBar_T_Visibility), SIGNAL(triggered(bool)),
            this, SLOT(sltToggleMenuBar()));
    connect(actionPool()->action(UIActionIndexRT_M_View_M_StatusBar_S_Settings), SIGNAL(triggered(bool)),
            this, SLOT(sltOpenStatusBarSettings()));
    connect(actionPool()->action(UIActionIndexRT_M_View_M_StatusBar_T_Visibility), SIGNAL(triggered(bool)),
            this, SLOT(sltToggleStatusBar()));
    connect(actionPool(), SIGNAL(sigNotifyAboutTriggeringViewScreenToggle(int, bool)),
            this, SLOT(sltHandleActionTriggerViewScreenToggle(int, bool)));
    connect(actionPool(), SIGNAL(sigNotifyAboutTriggeringViewScreenResize(int, const QSize&)),
            this, SLOT(sltHandleActionTriggerViewScreenResize(int, const QSize&)));
}

void UIMachineLogicNormal::prepareMachineWindows()
{
    /* Do not create machine-window(s) if they created already: */
    if (isMachineWindowsCreated())
        return;

#ifdef Q_WS_MAC // TODO: Is that really need here?
    /* We have to make sure that we are getting the front most process.
     * This is necessary for Qt versions > 4.3.3: */
    ::darwinSetFrontMostProcess();
#endif /* Q_WS_MAC */

    /* Get monitors count: */
    ulong uMonitorCount = session().GetMachine().GetMonitorCount();
    /* Create machine window(s): */
    for (ulong uScreenId = 0; uScreenId < uMonitorCount; ++ uScreenId)
        addMachineWindow(UIMachineWindow::create(this, uScreenId));
    /* Order machine window(s): */
    for (ulong uScreenId = uMonitorCount; uScreenId > 0; -- uScreenId)
        machineWindows()[uScreenId - 1]->raise();

    /* Listen for frame-buffer resize: */
    foreach (UIMachineWindow *pMachineWindow, machineWindows())
        connect(pMachineWindow, SIGNAL(sigFrameBufferResize()),
                this, SIGNAL(sigFrameBufferResize()));
    emit sigFrameBufferResize();

    /* Mark machine-window(s) created: */
    setMachineWindowsCreated(true);
}

void UIMachineLogicNormal::cleanupMachineWindows()
{
    /* Do not destroy machine-window(s) if they destroyed already: */
    if (!isMachineWindowsCreated())
        return;

    /* Mark machine-window(s) destroyed: */
    setMachineWindowsCreated(false);

    /* Cleanup machine-window(s): */
    foreach (UIMachineWindow *pMachineWindow, machineWindows())
        UIMachineWindow::destroy(pMachineWindow);
}

void UIMachineLogicNormal::cleanupActionConnections()
{
    /* "View" actions disconnections: */
    disconnect(actionPool()->action(UIActionIndexRT_M_View_T_Fullscreen), SIGNAL(triggered(bool)),
               this, SLOT(sltChangeVisualStateToFullscreen()));
    disconnect(actionPool()->action(UIActionIndexRT_M_View_T_Seamless), SIGNAL(triggered(bool)),
               this, SLOT(sltChangeVisualStateToSeamless()));
    disconnect(actionPool()->action(UIActionIndexRT_M_View_T_Scale), SIGNAL(triggered(bool)),
               this, SLOT(sltChangeVisualStateToScale()));
    disconnect(actionPool()->action(UIActionIndexRT_M_View_M_MenuBar_S_Settings), SIGNAL(triggered(bool)),
               this, SLOT(sltOpenMenuBarSettings()));
    disconnect(actionPool()->action(UIActionIndexRT_M_View_M_MenuBar_T_Visibility), SIGNAL(triggered(bool)),
               this, SLOT(sltToggleMenuBar()));
    disconnect(actionPool()->action(UIActionIndexRT_M_View_M_StatusBar_S_Settings), SIGNAL(triggered(bool)),
               this, SLOT(sltOpenStatusBarSettings()));
    disconnect(actionPool()->action(UIActionIndexRT_M_View_M_StatusBar_T_Visibility), SIGNAL(triggered(bool)),
               this, SLOT(sltToggleStatusBar()));

    /* Call to base-class: */
    UIMachineLogic::cleanupActionConnections();
}

